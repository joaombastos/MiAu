#include "slice/slice.h"
#include "DAC.h"
#include "pinos.h"
#include "midi_gate/midi_gate.h"
#include "effects/effects.h"
#include <SD.h>
#include <SPI.h>

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

// Buffer mono completo do arquivo carregado
static int16_t* s_data = nullptr;
static uint32_t s_totalFrames = 0;

// 8 slices (uma voz por passo)
struct SliceRegion { uint32_t start; uint32_t end; };
static SliceRegion s_regions[8];

struct SliceVoice { bool playing; uint32_t pos; };
static SliceVoice s_voices[8];

// POTs suavizados para efeitos
static float s_pot1Filtered = 0.0f; // reverb mix
static float s_pot2Filtered = 0.0f; // delay mix
static float s_pot3Filtered = 0.0f; // chorus/flanger mix

static int s_fileIndex = 0; // 0..15 => 1..16

static bool loadSliceFile() {
	// Inicializa SPI/SD como outros módulos do AU
	SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
	if (!SD.begin(SD_CS)) {
		return false;
	}
	char path[40];
	// Nome padrão: /slice/slice{1..16}.wav
	snprintf(path, sizeof(path), "/slice/slice%d.wav", s_fileIndex + 1);
	File f = SD.open(path, FILE_READ);
	if (!f) {
		// Retrocompatibilidade: /slice/{1..16}.wav
		snprintf(path, sizeof(path), "/slice/%d.wav", s_fileIndex + 1);
		f = SD.open(path, FILE_READ);
	}
	if (!f) return false;
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
	if (fmtHdr.num_channels != 1) { f.close(); return false; }
	if (fmtHdr.sample_rate != 44100) { /* permitir só 44.1k conforme pedido */ }
	// Procura chunk data
	bool gotData = false; uint32_t dataSize = 0; uint32_t dataPos = 0;
	while (f.available()) {
		char id[4]; uint32_t size;
		if (f.read((uint8_t*)id, 4) != 4) { break; }
		if (f.read((uint8_t*)&size, 4) != 4) { break; }
		if (memcmp(id, "data", 4) == 0) { gotData = true; dataSize = size; dataPos = f.position(); break; }
		else { f.seek(f.position() + size); }
	}
	if (!gotData || dataSize == 0) { f.close(); return false; }
	uint32_t frames = dataSize / sizeof(int16_t);
	if (frames == 0) { f.close(); return false; }

	// Aloca em PSRAM preferencialmente
	int16_t* mono = (int16_t*)heap_caps_malloc(frames * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (!mono) mono = (int16_t*)malloc(frames * sizeof(int16_t));
	if (!mono) { f.close(); return false; }
	f.seek(dataPos);
	uint32_t remain = dataSize; uint32_t offsetBytes = 0; const uint32_t chunk = 4096;
	while (remain > 0) {
		uint32_t n = remain > chunk ? chunk : remain;
		int rd = f.read((uint8_t*)((uint8_t*)mono + offsetBytes), n);
		if (rd <= 0) break; offsetBytes += (uint32_t)rd; remain -= (uint32_t)rd;
	}
	f.close();
	if (offsetBytes != dataSize) { free(mono); return false; }

	// Define regiões 8 slices
	uint32_t sliceLen = frames / 8u;
	for (int i=0;i<8;i++) {
		s_regions[i].start = i * sliceLen;
		s_regions[i].end = (i == 7) ? frames : (uint32_t)(i+1) * sliceLen;
	}

	// Commit
	if (s_data) free(s_data);
	s_data = mono; s_totalFrames = frames;
	for (int i=0;i<8;i++) { s_voices[i].playing = false; s_voices[i].pos = 0; }
	return true;
}

void slice_begin() {
	if (s_initialized) return;
	s_initialized = true;
	// Configurar POTs e estado inicial dos efeitos
	pinMode(POT1, INPUT);
	{
		int raw = analogRead(POT1);
		s_pot1Filtered = (float)raw;
	}
	pinMode(POT2, INPUT);
	{
		int raw = analogRead(POT2);
		s_pot2Filtered = (float)raw;
	}
	pinMode(POT3, INPUT);
	{
		int raw = analogRead(POT3);
		s_pot3Filtered = (float)raw;
	}
	effects_enable_reverb(true);
	effects_set_reverb_mix(0.0f);
	effects_enable_delay(true);
	effects_set_delay_time_ms(600);
	effects_set_delay_feedback(0.50f);
	effects_set_delay_mix(0.0f);
	effects_enable_chorus(true);
	effects_enable_flanger(true);
}

void slice_set_active(bool enabled) {
	if (enabled == s_active) return;
	if (enabled) {
		if (!loadSliceFile()) {
			s_active = false;
			return;
		}
		s_active = true;
		for (int i=0;i<8;i++) { s_voices[i].playing = false; s_voices[i].pos = 0; }
	} else {
		s_active = false;
		for (int i=0;i<8;i++) { s_voices[i].playing = false; s_voices[i].pos = 0; }
	}
}

bool slice_is_active() { return s_active; }

void slice_set_file_index(int index) {
	if (index < 0) index = 0; if (index > 15) index = 15;
	if (index == s_fileIndex) return;
	s_fileIndex = index;
	if (s_active) {
		(void)loadSliceFile();
	}
}

int slice_get_file_index() { return s_fileIndex; }

static inline int noteToSliceIndex(uint8_t midiNote) {
	// C2..G2 => 36..43 (conforme MI)
	if (midiNote < 36 || midiNote > 43) return -1;
	return (int)midiNote - 36; // 0..7
}

void slice_tick() {
	if (!s_active || !s_data) return;
	// Processamento MIDI otimizado para máxima precisão
	MidiGateEvent ev;
	int midiEvents = 0;
	const int maxMidiEvents = 8; // Limitar eventos por frame para estabilidade
	while (midi_gate_peek_event(&ev) && midiEvents < maxMidiEvents) {
		// Aceita qualquer canal para garantir disparo pelos passos ativos (C2..G2)
		if (ev.type == MIDI_GATE_EVT_NOTE_ON && ev.velocity > 0) {
			int idx = noteToSliceIndex(ev.note);
			if (idx >= 0 && idx < 8) { 
				s_voices[idx].playing = true; 
				s_voices[idx].pos = s_regions[idx].start; 
			}
		} else if (ev.type == MIDI_GATE_EVT_STOP) {
			for (int i=0;i<8;i++) s_voices[i].playing = false;
		}
		midi_gate_pop_event();
		midiEvents++;
	}

	// Mix simples mono -> estéreo duplicado
	static int16_t out[DAC_BUFFER_SIZE];
	for (int i=0;i<DAC_BUFFER_SIZE; ++i) out[i] = 0;
	const int frames = DAC_BUFFER_SIZE / 2; // intercalado
	// Processamento de POTs otimizado para menor latência
	{
		int raw = analogRead(POT1);
		const float alpha = 0.3f; // Resposta mais rápida
		s_pot1Filtered += alpha * ((float)raw - s_pot1Filtered);
		float mix = s_pot1Filtered / 4095.0f;
		mix = fmaxf(0.0f, fminf(1.0f, mix)); // Clamp otimizado
		effects_enable_reverb(true);
		effects_set_reverb_mix(mix);
	}
	{
		int raw = analogRead(POT2);
		const float alpha = 0.3f; // Resposta mais rápida
		s_pot2Filtered += alpha * ((float)raw - s_pot2Filtered);
		float mix = s_pot2Filtered / 4095.0f;
		mix = fmaxf(0.0f, fminf(1.0f, mix)); // Clamp otimizado
		effects_enable_delay(true);
		effects_set_delay_mix(mix);
	}
	{
		int raw = analogRead(POT3);
		const float alpha = 0.3f; // Resposta mais rápida
		s_pot3Filtered += alpha * ((float)raw - s_pot3Filtered);
		float mix = s_pot3Filtered / 4095.0f;
		mix = fmaxf(0.0f, fminf(1.0f, mix)); // Clamp otimizado
		effects_enable_chorus(true);
		effects_set_chorus_mix(mix);
		effects_enable_flanger(true);
		effects_set_flanger_mix(mix);
	}
	// Processamento de áudio otimizado para menor latência
	for (int i=0;i<frames; ++i) {
		int32_t mix = 0; // Usar int32 para evitar overflow
		int activeVoices = 0;
		
		// Loop otimizado - processar apenas vozes ativas
		for (int v=0; v<8; ++v) {
			if (!s_voices[v].playing) continue;
			
			uint32_t p = s_voices[v].pos;
			if (p >= s_regions[v].end) { 
				s_voices[v].playing = false; 
				continue; 
			}
			
			// Leitura direta sem verificação adicional
			mix += (int32_t)s_data[p];
			s_voices[v].pos = p + 1;
			activeVoices++;
		}
		
		// Normalização inteligente para evitar clipping
		int16_t finalMix;
		if (activeVoices > 0) {
			// Normalização por número de vozes ativas com limitação suave
			mix = mix / activeVoices;
			// Soft clipping para evitar distorção
			if (mix > 32767) mix = 32767; 
			else if (mix < -32768) mix = -32768;
		}
		finalMix = (int16_t)mix;
		
		// Output estéreo duplicado
		int outIdx = i * 2;
		out[outIdx] = finalMix; 
		out[outIdx + 1] = finalMix;
	}
	// Processa bloco de efeitos
	effects_process_block(out, frames);
	dac_play_buffer(out, sizeof(out));
}


