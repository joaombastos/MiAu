#include <Arduino.h>
#include "dmachine/dmachine.h"
#include "midi_gate/midi_gate.h"
#include "DAC.h"
#include "pinos.h"
#include <SD.h>
#include <SPI.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "effects/effects.h"
#include <math.h>

struct __attribute__((packed)) WavHeaderSimple {
	char riff[4];
	uint32_t size;
	char wave[4];
	char fmt[4];
	uint32_t fmt_size;
	uint16_t audio_format;
	uint16_t num_channels;
	uint32_t sample_rate;
	uint32_t byte_rate;
	uint16_t block_align;
	uint16_t bits_per_sample;
};

static bool s_initialized = false;
static bool s_active = false;
static int s_bankIndex = 0; // 0..7
static const char* s_bankNames[8] = {"808","CAPOE","BRAZIL","DRUM1","LONG","RECORD","INSTRUM","ELECTRO"};

// 8 vozes
static const int kMaxVoices = 8;
static const int kBlockFrames = DAC_BUFFER_SIZE / 2; // estéreo intercalado

struct SampleInfo {
	bool ok;
	uint32_t frames_total;
	int16_t *data;       // mono em RAM (opcional)
	uint32_t data_pos;   // posição do chunk data
	uint16_t channels;   // 1 ou 2
	bool stream_only;    // true se não coube em RAM
};

// Buffer circular simples em frames mono 16-bit
struct RingBuffer {
	int16_t *data;
	uint32_t capacity;       // em frames
	volatile uint32_t read_idx;
	volatile uint32_t write_idx;
	volatile uint32_t available; // frames disponíveis
};

struct Voice {
	bool active;
	float gain; // 0..1
	const int16_t *data; // RAM pointer (se houver)
	uint32_t pos_frames;
	uint32_t frames_total;
	// Streaming
	bool stream;
	File file;
	uint32_t stream_pos_frames; // frames lidos desde data_pos
	int sample_index; // 1..16
	RingBuffer rb;
	bool at_eof;
};

static SampleInfo s_samples[16];
static Voice s_voices[kMaxVoices];
static int16_t s_mixOut[DAC_BUFFER_SIZE];

// Sincronização dos RBs
static portMUX_TYPE s_rbMux[kMaxVoices] = {
	portMUX_INITIALIZER_UNLOCKED, portMUX_INITIALIZER_UNLOCKED, portMUX_INITIALIZER_UNLOCKED, portMUX_INITIALIZER_UNLOCKED,
	portMUX_INITIALIZER_UNLOCKED, portMUX_INITIALIZER_UNLOCKED, portMUX_INITIALIZER_UNLOCKED, portMUX_INITIALIZER_UNLOCKED
};

// Tarefa de streaming e parâmetros
static TaskHandle_t s_streamTaskHandle = nullptr;
static const uint32_t kRbFramesPerVoice = 16384; // ~372ms @44.1k

static inline uint32_t rb_space(const RingBuffer *rb) { return rb->capacity - rb->available; }

static uint32_t rb_write_frames(RingBuffer *rb, int voiceIdx, const int16_t *src, uint32_t frames) {
	uint32_t written = 0;
	if (frames == 0 || rb->capacity == 0) return 0;
	portENTER_CRITICAL(&s_rbMux[voiceIdx]);
	uint32_t space = rb_space(rb);
	uint32_t n = (frames > space) ? space : frames;
	uint32_t first = n;
	if (rb->write_idx + n > rb->capacity) first = rb->capacity - rb->write_idx;
	if (first > 0) {
		memcpy(&rb->data[rb->write_idx], src, first * sizeof(int16_t));
		rb->write_idx += first;
		if (rb->write_idx >= rb->capacity) rb->write_idx = 0;
	}
	uint32_t rem = n - first;
	if (rem > 0) {
		memcpy(&rb->data[rb->write_idx], src + first, rem * sizeof(int16_t));
		rb->write_idx += rem;
	}
	rb->available += n;
	written = n;
	portEXIT_CRITICAL(&s_rbMux[voiceIdx]);
	return written;
}

static bool rb_read_one(RingBuffer *rb, int voiceIdx, int16_t *out) {
	bool ok = false;
	portENTER_CRITICAL(&s_rbMux[voiceIdx]);
	if (rb->available > 0) {
		*out = rb->data[rb->read_idx];
		rb->read_idx++;
		if (rb->read_idx >= rb->capacity) rb->read_idx = 0;
		rb->available--;
		ok = true;
	}
	portEXIT_CRITICAL(&s_rbMux[voiceIdx]);
	return ok;
}

static void streamTask(void *arg) {
	const int kIoFrames = 1024; // Tamanho equilibrado para estabilidade
	int16_t *ioBuf = (int16_t*)malloc(kIoFrames * 2 * sizeof(int16_t)); // até 1024 frames estéreo
	for (;;) {
		// Só stream quando dmachine estiver ativo
		if (!s_active) { vTaskDelay(pdMS_TO_TICKS(5)); continue; }
		for (int v = 0; v < kMaxVoices; ++v) {
			Voice &voice = s_voices[v];
			if (!voice.active || !voice.stream || !voice.file) continue;
			RingBuffer &rb = voice.rb;
			int channels = s_samples[voice.sample_index - 1].channels;
			// Headroom equilibrado para estabilidade
			if (rb.available >= (rb.capacity - 512)) continue;
			uint32_t spaceFrames = rb_space(&rb);
			if (spaceFrames < 128) continue; // Threshold equilibrado
			uint32_t chunkFrames = spaceFrames > (uint32_t)kIoFrames ? (uint32_t)kIoFrames : spaceFrames;
			int needBytes = (int)chunkFrames * channels * (int)sizeof(int16_t);
			int n = voice.file.read((uint8_t*)ioBuf, needBytes);
			if (n <= 0) { voice.at_eof = true; continue; }
			int gotSamples = n / (int)sizeof(int16_t);
			int gotFrames = (channels == 1) ? gotSamples : (gotSamples / 2);
			if (gotFrames <= 0) continue;
			if (channels == 1) {
				(void)rb_write_frames(&rb, v, ioBuf, (uint32_t)gotFrames);
			} else {
				for (int i = 0; i < gotFrames; ++i) ioBuf[i] = ioBuf[i * 2];
				(void)rb_write_frames(&rb, v, ioBuf, (uint32_t)gotFrames);
			}
			voice.stream_pos_frames += (uint32_t)gotFrames;
		}
		vTaskDelay(pdMS_TO_TICKS(1)); // Delay equilibrado
	}
}

// Pot1 suavizado para efeitos (reverb mix)
static float s_pot1Filtered = 0.0f;
// Pot2 suavizado para efeitos (delay mix)
static float s_pot2Filtered = 0.0f;
// Pot3 suavizado para distorção (mix)
static float s_pot3Filtered = 0.0f;
static float s_panPhase = 0.0f; // 0..1

// Mapeamento C1..D#2 → 1..16
static inline int noteToIndex(uint8_t midiNote) {
	const int base = 36; // C1
	int idx = (int)midiNote - base + 1;
	if (idx < 1 || idx > 16) return -1;
	return idx;
}

static bool preloadSampleIntoPsram(int sampleIndex) {
	const char* bank = s_bankNames[s_bankIndex];
	char path[48]; snprintf(path, sizeof(path), "/%s/%d.wav", bank, sampleIndex);
	File f = SD.open(path, FILE_READ); if (!f) return false;
	char riff[4]; uint32_t riff_size; char wave[4];
	if (f.read((uint8_t*)riff, 4) != 4) { f.close(); return false; }
	if (f.read((uint8_t*)&riff_size, 4) != 4) { f.close(); return false; }
	if (f.read((uint8_t*)wave, 4) != 4) { f.close(); return false; }
	if (memcmp(riff, "RIFF", 4) || memcmp(wave, "WAVE", 4)) { f.close(); return false; }
	WavHeaderSimple fmtHdr{}; bool gotFmt = false;
	while (f.available() && !gotFmt) {
		char id[4]; uint32_t size;
		if (f.read((uint8_t*)id, 4) != 4) { f.close(); return false; }
		if (f.read((uint8_t*)&size, 4) != 4) { f.close(); return false; }
		if (memcmp(id, "fmt ", 4) == 0) {
			if (size < 16) { f.close(); return false; }
			if (f.read((uint8_t*)&fmtHdr.audio_format, 16) != 16) { f.close(); return false; }
			if (size > 16) { f.seek(f.position() + (size - 16)); }
			gotFmt = true; break;
		} else { f.seek(f.position() + size); }
	}
	if (!gotFmt) { f.close(); return false; }
	if (fmtHdr.audio_format != 1 || fmtHdr.bits_per_sample != 16) { f.close(); return false; }
	bool gotData = false; uint32_t dataSize = 0; uint32_t dataPos = 0;
	while (f.available()) {
		char id[4]; uint32_t size;
		if (f.read((uint8_t*)id, 4) != 4) { break; }
		if (f.read((uint8_t*)&size, 4) != 4) { break; }
		if (memcmp(id, "data", 4) == 0) { gotData = true; dataSize = size; dataPos = f.position(); break; }
		else { f.seek(f.position() + size); }
	}
	if (!gotData || dataSize == 0) { f.close(); return false; }
	int channels = fmtHdr.num_channels;
	uint32_t framesTotal = dataSize / (channels * sizeof(int16_t));
	if (framesTotal == 0) { f.close(); return false; }

	int16_t *mono = (int16_t*)heap_caps_malloc(framesTotal * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	bool streamOnly = false;
	if (!mono) { mono = (int16_t*)malloc(framesTotal * sizeof(int16_t)); }
	if (!mono) { f.close(); return false; }
	{
		f.seek(dataPos);
		if (channels == 1) {
			uint32_t remain = dataSize; uint32_t offsetBytes = 0; const uint32_t chunk = 4096;
			while (remain > 0) {
				uint32_t n = remain > chunk ? chunk : remain;
				int rd = f.read((uint8_t*)((uint8_t*)mono + offsetBytes), n);
				if (rd <= 0) break; offsetBytes += (uint32_t)rd; remain -= (uint32_t)rd;
			}
		} else {
			const int stereoChunkFrames = 2048; int16_t *tmp = (int16_t*)malloc(stereoChunkFrames * 2 * sizeof(int16_t));
			if (!tmp) { free(mono); f.close(); return false; }
			else {
				uint32_t written = 0; f.seek(dataPos);
				while (written < framesTotal) {
					int need = framesTotal - written; if (need > stereoChunkFrames) need = stereoChunkFrames;
					int n = f.read((uint8_t*)tmp, need * 2 * (int)sizeof(int16_t));
					int got = n / (int)sizeof(int16_t); int pairs = got / 2;
					for (int i = 0; i < pairs; ++i) mono[written + i] = tmp[i * 2];
					written += pairs; if (pairs == 0) break;
				}
				free(tmp);
			}
		}
	}
	s_samples[sampleIndex - 1].ok = true;
	s_samples[sampleIndex - 1].frames_total = framesTotal;
	s_samples[sampleIndex - 1].data = mono;
	s_samples[sampleIndex - 1].data_pos = dataPos;
	s_samples[sampleIndex - 1].channels = (uint16_t)channels;
	s_samples[sampleIndex - 1].stream_only = false;
	f.close(); return true;
}

static int allocateVoice() { for (int i=0;i<kMaxVoices;++i) if (!s_voices[i].active) return i; return 0; }

void dmachine_begin() {
	if (s_initialized) return;
	SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
	SPI.setFrequency(40000000);
	if (!SD.begin(SD_CS)) {
		SPI.setFrequency(25000000);
		SD.begin(SD_CS);
	}
	for (int i=0;i<16;++i) s_samples[i].ok = preloadSampleIntoPsram(i+1);
	for (int i=0;i<kMaxVoices;++i) {
		s_voices[i] = {false,1.0f,nullptr,0,0,false,File(),0,0,{nullptr,0,0,0,0},false};
		s_voices[i].rb.data = (int16_t*)heap_caps_malloc(kRbFramesPerVoice * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
		if (!s_voices[i].rb.data) s_voices[i].rb.data = (int16_t*)malloc(kRbFramesPerVoice * sizeof(int16_t));
		s_voices[i].rb.capacity = s_voices[i].rb.data ? kRbFramesPerVoice : 0;
		s_voices[i].rb.read_idx = s_voices[i].rb.write_idx = s_voices[i].rb.available = 0;
	}
	effects_begin(DAC_SAMPLE_RATE);
	// Delay: ativado no dmachine; tempo/feedback fixos; mix via POT2
	effects_enable_delay(true);
	effects_set_delay_time_ms(600);
	effects_set_delay_feedback(0.50f);
	// Configura POT1 e inicializa filtro para mix de reverb
	pinMode(POT1, INPUT);
	{
		int raw = analogRead(POT1);
		s_pot1Filtered = (float)raw;
	}
	// Configura POT2 e inicializa filtro para mix de delay
	pinMode(POT2, INPUT);
	{
		int raw = analogRead(POT2);
		s_pot2Filtered = (float)raw;
	}
	// Reverb ativo apenas no dmachine; mix inicial 0
	effects_enable_reverb(true);
	effects_set_reverb_mix(0.0f);
	// Delay mix inicial 0
	effects_set_delay_mix(0.0f);
	// Chorus/Flanger: ativos no dmachine; mix via POT3
	effects_enable_chorus(true);
	effects_enable_flanger(true);
	pinMode(POT3, INPUT);
	{
		int raw = analogRead(POT3);
		s_pot3Filtered = (float)raw;
	}
	// Tarefa de streaming (core 1) com prioridade equilibrada
	xTaskCreatePinnedToCore(streamTask, "sd_stream", 6144, nullptr, 4, &s_streamTaskHandle, 1);
	s_initialized = true;
}

bool dmachine_is_active() { return s_active; }
void dmachine_set_active(bool enabled) { s_active = enabled; }

void dmachine_set_bank_index(int index) {
	if (index < 0) index = 0; if (index > 7) index = 7;
	if (index == s_bankIndex) return;
	s_bankIndex = index;
	// Parar vozes e pré-carregar novo banco
	for (int v = 0; v < kMaxVoices; ++v) {
		s_voices[v].active = false;
		if (s_voices[v].stream && s_voices[v].file) s_voices[v].file.close();
	}
	// Libertar samples do banco anterior (evitar esgotar PSRAM)
	for (int i = 0; i < 16; ++i) {
		if (s_samples[i].data) {
			free(s_samples[i].data);
			s_samples[i].data = nullptr;
		}
		s_samples[i].ok = false;
		s_samples[i].frames_total = 0;
		s_samples[i].data_pos = 0;
		s_samples[i].channels = 0;
		s_samples[i].stream_only = false;
	}
	for (int i=0;i<16;++i) {
		s_samples[i].ok = preloadSampleIntoPsram(i+1);
	}
}

int dmachine_get_bank_index() { return s_bankIndex; }
const char* dmachine_get_bank_name() { return s_bankNames[s_bankIndex]; }

static void startVoice(int voiceIdx, int sampleIndex, float gain) {
	Voice &voice = s_voices[voiceIdx]; const SampleInfo &si = s_samples[sampleIndex - 1];
	voice.active = true; voice.gain = gain; voice.frames_total = si.frames_total; voice.pos_frames = 0; voice.sample_index = sampleIndex; voice.at_eof = false;
	voice.rb.read_idx = voice.rb.write_idx = voice.rb.available = 0;
	if (!si.stream_only && si.data) { voice.stream = false; voice.data = si.data; }
	else {
		// Com preload total, streaming deve estar desativado; fallback de segurança
		voice.stream = false; voice.data = si.data;
	}
}

// Para todas as vozes que tocam um determinado sample index
static void stopVoicesForSampleIndex(int sampleIndex) {
	for (int v = 0; v < kMaxVoices; ++v) {
		Voice &voice = s_voices[v];
		if (voice.active && voice.sample_index == sampleIndex) {
			voice.active = false;
			if (voice.stream && voice.file) {
				voice.file.close();
			}
		}
	}
}

void dmachine_tick() {
	if (!s_initialized || !s_active) return;
	const int frames = kBlockFrames;
	// Atualiza mix do reverb a partir do POT1 (0..4095) com suavização simples
	{
		int raw = analogRead(POT1);
		const float alpha = 0.2f; // resposta suave
		s_pot1Filtered += alpha * ((float)raw - s_pot1Filtered);
		float mix = s_pot1Filtered / 4095.0f;
		if (mix < 0.0f) mix = 0.0f; else if (mix > 1.0f) mix = 1.0f;
		effects_enable_reverb(true);
		effects_set_reverb_mix(mix);
	}
	// Atualiza mix do delay a partir do POT2 (0..4095) com suavização simples
	{
		int raw = analogRead(POT2);
		const float alpha2 = 0.2f;
		s_pot2Filtered += alpha2 * ((float)raw - s_pot2Filtered);
		float mix2 = s_pot2Filtered / 4095.0f;
		if (mix2 < 0.0f) mix2 = 0.0f; else if (mix2 > 1.0f) mix2 = 1.0f;
		effects_enable_delay(true);
		effects_set_delay_mix(mix2);
	}
	// Atualiza mix do chorus a partir do POT3 (0..4095) com suavização simples
	{
		int raw = analogRead(POT3);
		const float alpha3 = 0.2f;
		s_pot3Filtered += alpha3 * ((float)raw - s_pot3Filtered);
		float mix3 = s_pot3Filtered / 4095.0f;
		if (mix3 < 0.0f) mix3 = 0.0f; else if (mix3 > 1.0f) mix3 = 1.0f;
		effects_enable_chorus(true);
		effects_set_chorus_mix(mix3);
		effects_enable_flanger(true);
		effects_set_flanger_mix(mix3);
	}
	MidiGateEvent ev;
	while (midi_gate_peek_event(&ev)) {
		if (ev.channel == 2) {
			if ((ev.type == MIDI_GATE_EVT_NOTE_ON) && ev.velocity > 0) {
				int idx = noteToIndex(ev.note);
				if (idx > 0 && s_samples[idx - 1].ok) {
					int vIdx = allocateVoice(); startVoice(vIdx, idx, (float)ev.velocity / 127.0f);
				}
			}
		}
		midi_gate_pop_event();
	}
	for (int i=0;i<DAC_BUFFER_SIZE;++i) s_mixOut[i] = 0;
	for (int v=0; v<kMaxVoices; ++v) {
		Voice &voice = s_voices[v]; if (!voice.active) continue; const float g = voice.gain;
		if (!voice.stream) {
			// RAM mixing
			for (int i=0;i<frames;++i) {
				int idx=i*2; int16_t s=0;
				if (voice.pos_frames < voice.frames_total) { s = voice.data[voice.pos_frames++]; }
				else { voice.active=false; break; }
				int32_t add = (int32_t)((int32_t)s * g);
				int32_t L = (int32_t)s_mixOut[idx] + add; int32_t R = (int32_t)s_mixOut[idx+1] + add;
				if (L>32767) L=32767; else if (L<-32768) L=-32768; if (R>32767) R=32767; else if (R<-32768) R=-32768;
				s_mixOut[idx]=(int16_t)L; s_mixOut[idx+1]=(int16_t)R;
			}
		} else {
			// Consumir do ring buffer
			for (int i = 0; i < frames; ++i) {
				int idxOut = i * 2; int16_t s = 0; bool ok = rb_read_one(&voice.rb, v, &s);
				if (!ok) {
					if (voice.at_eof && voice.rb.available == 0) { voice.active = false; break; }
					// sem dados: escreve zero
				}
				int32_t add = (int32_t)((int32_t)s * g);
				int32_t L = (int32_t)s_mixOut[idxOut] + add; int32_t R = (int32_t)s_mixOut[idxOut+1] + add;
				if (L>32767) L=32767; else if (L<-32768) L=-32768; if (R>32767) R=32767; else if (R<-32768) R=-32768;
				s_mixOut[idxOut]=(int16_t)L; s_mixOut[idxOut+1]=(int16_t)R;
			}
		}
	}
	effects_process_block(s_mixOut, frames);
	dac_play_buffer(s_mixOut, DAC_BUFFER_SIZE * sizeof(int16_t));
}


