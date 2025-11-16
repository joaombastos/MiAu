#include <Arduino.h>
#include <math.h>
#include "synthesizer/synthesizer.h"
#include "DAC.h"
#include "pinos.h"
#include "midi_gate/midi_gate.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Configurações do sintetizador
#define MAX_VOICES 16
#define SAMPLE_RATE ((float)DAC_SAMPLE_RATE)
#define MICROS_PER_SECOND 1000000.0f

#pragma GCC optimize("O3")

// Estado do sintetizador
static bool s_initialized = false;
static bool s_active = false;
static int16_t s_outBuffer[DAC_BUFFER_SIZE];

// Buffers circulares para máxima performance
static float s_waveBuffer[DAC_BUFFER_SIZE]; // Buffer circular para ondas
static float s_doublerBuffer[DAC_BUFFER_SIZE]; // Buffer circular para doubler
static int s_bufferIndex = 0; // Índice do buffer circular

// Parâmetros atuais
static SynthParams s_params = {
    .waveform = SYNTH_WAVE_SAW,
    .filterType = SYNTH_FILTER_LOWPASS,
    .filterCutoff = 1.0f,
    .filterResonance = 0.0f,
    .ampEnvelope = {0.0f, 0.0f, 1.0f, 0.0f},
    .filterEnvelope = {0.0f, 0.0f, 1.0f, 0.0f},
    .lfoRate = 0.0f,
    .lfoAmount = 0.0f,
    .modWheel = 0.0f
};

// Vozes do sintetizador
static SynthVoice s_voices[MAX_VOICES];

// Filtro simples (1-pole)
static float s_filterState[2] = {0.0f, 0.0f};

// Estados para ring modulation e FM
static float s_ringModPhase = 0.0f;
static float s_fmPhase = 0.0f;

// Funções auxiliares
static float note_to_frequency(uint8_t note) {
    if (note >= 128) return 0.0f;
    
    // Cache otimizado para frequências mais usadas (C3-C7)
    static const float common_freqs[49] = {
        130.81f, 138.59f, 146.83f, 155.56f, 164.81f, 174.61f, 185.00f, 196.00f, 207.65f, 220.00f,
        233.08f, 246.94f, 261.63f, 277.18f, 293.66f, 311.13f, 329.63f, 349.23f, 369.99f, 392.00f,
        415.30f, 440.00f, 466.16f, 493.88f, 523.25f, 554.37f, 587.33f, 622.25f, 659.25f, 698.46f,
        739.99f, 783.99f, 830.61f, 880.00f, 932.33f, 987.77f, 1046.50f, 1108.73f, 1174.66f, 1244.51f,
        1318.51f, 1396.91f, 1479.98f, 1567.98f, 1661.22f, 1760.00f, 1864.66f, 1975.53f, 2093.00f
    };
    
    // Cache para notas mais comuns (C3-C7)
    if (note >= 48 && note <= 96) {
        return common_freqs[note - 48];
    }
    
    // Cálculo otimizado para outras notas
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

// Tabela de seno para máxima estabilidade (igual ao modo ondas)
static const float s_sineTable[256] = {
    0.000000f, 0.024541f, 0.049068f, 0.073565f, 0.098017f, 0.122411f, 0.146730f, 0.170962f,
    0.195090f, 0.219101f, 0.242980f, 0.266713f, 0.290285f, 0.313682f, 0.336890f, 0.359895f,
    0.382683f, 0.405241f, 0.427555f, 0.449611f, 0.471397f, 0.492898f, 0.514103f, 0.534998f,
    0.555570f, 0.575808f, 0.595699f, 0.615232f, 0.634393f, 0.653173f, 0.671559f, 0.689541f,
    0.707107f, 0.724247f, 0.740951f, 0.757209f, 0.773010f, 0.788346f, 0.803208f, 0.817585f,
    0.831470f, 0.844854f, 0.857729f, 0.870087f, 0.881921f, 0.893224f, 0.903989f, 0.914210f,
    0.923880f, 0.932993f, 0.941544f, 0.949528f, 0.956940f, 0.963776f, 0.970031f, 0.975702f,
    0.980785f, 0.985278f, 0.989177f, 0.992480f, 0.995185f, 0.997290f, 0.998795f, 0.999699f,
    1.000000f, 0.999699f, 0.998795f, 0.997290f, 0.995185f, 0.992480f, 0.989177f, 0.985278f,
    0.980785f, 0.975702f, 0.970031f, 0.963776f, 0.956940f, 0.949528f, 0.941544f, 0.932993f,
    0.923880f, 0.914210f, 0.903989f, 0.893224f, 0.881921f, 0.870087f, 0.857729f, 0.844854f,
    0.831470f, 0.817585f, 0.803208f, 0.788346f, 0.773010f, 0.757209f, 0.740951f, 0.724247f,
    0.707107f, 0.689541f, 0.671559f, 0.653173f, 0.634393f, 0.615232f, 0.595699f, 0.575808f,
    0.555570f, 0.534998f, 0.514103f, 0.492898f, 0.471397f, 0.449611f, 0.427555f, 0.405241f,
    0.382683f, 0.359895f, 0.336890f, 0.313682f, 0.290285f, 0.266713f, 0.242980f, 0.219101f,
    0.195090f, 0.170962f, 0.146730f, 0.122411f, 0.098017f, 0.073565f, 0.049068f, 0.024541f,
    0.000000f, -0.024541f, -0.049068f, -0.073565f, -0.098017f, -0.122411f, -0.146730f, -0.170962f,
    -0.195090f, -0.219101f, -0.242980f, -0.266713f, -0.290285f, -0.313682f, -0.336890f, -0.359895f,
    -0.382683f, -0.405241f, -0.427555f, -0.449611f, -0.471397f, -0.492898f, -0.514103f, -0.534998f,
    -0.555570f, -0.575808f, -0.595699f, -0.615232f, -0.634393f, -0.653173f, -0.671559f, -0.689541f,
    -0.707107f, -0.724247f, -0.740951f, -0.757209f, -0.773010f, -0.788346f, -0.803208f, -0.817585f,
    -0.831470f, -0.844854f, -0.857729f, -0.870087f, -0.881921f, -0.893224f, -0.903989f, -0.914210f,
    -0.923880f, -0.932993f, -0.941544f, -0.949528f, -0.956940f, -0.963776f, -0.970031f, -0.975702f,
    -0.980785f, -0.985278f, -0.989177f, -0.992480f, -0.995185f, -0.997290f, -0.998795f, -0.999699f,
    -1.000000f, -0.999699f, -0.998795f, -0.997290f, -0.995185f, -0.992480f, -0.989177f, -0.985278f,
    -0.980785f, -0.975702f, -0.970031f, -0.963776f, -0.956940f, -0.949528f, -0.941544f, -0.932993f,
    -0.923880f, -0.914210f, -0.903989f, -0.893224f, -0.881921f, -0.870087f, -0.857729f, -0.844854f,
    -0.831470f, -0.817585f, -0.803208f, -0.788346f, -0.773010f, -0.757209f, -0.740951f, -0.724247f,
    -0.707107f, -0.689541f, -0.671559f, -0.653173f, -0.634393f, -0.615232f, -0.595699f, -0.575808f,
    -0.555570f, -0.534998f, -0.514103f, -0.492898f, -0.471397f, -0.449611f, -0.427555f, -0.405241f,
    -0.382683f, -0.359895f, -0.336890f, -0.313682f, -0.290285f, -0.266713f, -0.242980f, -0.219101f,
    -0.195090f, -0.170962f, -0.146730f, -0.122411f, -0.098017f, -0.073565f, -0.049068f, -0.024541f
};

static float generate_waveform(SynthWaveform wave, float phase) {
    switch (wave) {
        case SYNTH_WAVE_SINE: {
            // Usar tabela de seno para máxima estabilidade
            int idx = (int)(phase * 256.0f) & 0xFF;
            return s_sineTable[idx];
        }
        case SYNTH_WAVE_SAW:
            return (phase * 2.0f) - 1.0f;
        case SYNTH_WAVE_SQUARE:
            return (phase < 0.5f) ? 1.0f : -1.0f;
        case SYNTH_WAVE_TRIANGLE:
            return (phase < 0.5f) ? (phase * 4.0f - 1.0f) : ((1.0f - phase) * 4.0f - 1.0f);
        case SYNTH_WAVE_NOISE:
            return ((random(0, 2000) * 0.001f) - 1.0f);
        default:
            return 0.0f;
    }
}

static float process_envelope(const SynthEnvelope& env, uint32_t noteOnTime, uint32_t noteOffTime, 
                             bool releasing, uint32_t currentTime) {
    uint32_t elapsed = currentTime - noteOnTime;
    uint32_t releaseElapsed = releasing ? (currentTime - noteOffTime) : 0;
    
    if (releasing) {
        float releaseTime = env.release * 50.0f; // Reduzido de 100ms para 50ms
        if (releaseElapsed >= releaseTime) {
            return 0.0f;
        }
        float sustainLevel = env.sustain;
        float releaseProgress = (float)releaseElapsed / releaseTime;
        return sustainLevel * (1.0f - releaseProgress); // Linear em vez de quadrático
    } else {
        float attackTime = env.attack * 2.0f; // Reduzido de 10ms para 2ms
        float decayTime = env.decay * 5.0f; // Reduzido de 20ms para 5ms
        
        if (elapsed < attackTime) {
            float progress = (float)elapsed / attackTime;
            return progress; // Linear em vez de cúbico
        } else if (elapsed < attackTime + decayTime) {
            float decayProgress = (elapsed - attackTime) / decayTime;
            return 1.0f - (1.0f - env.sustain) * decayProgress;
        } else {
            return env.sustain;
        }
    }
}

static float process_filter(float input, float cutoff, float resonance, SynthFilterType type) {
    float alpha = cutoff * 0.3f;
    alpha = fmaxf(0.01f, fminf(0.99f, alpha));
    
    float resonanceGain = 1.0f + (resonance * 2.0f);
    
    s_filterState[0] += alpha * (input - s_filterState[0]);
    float output = s_filterState[0] * resonanceGain;
    
    return output;
}

void synthesizer_begin() {
    if (s_initialized) return;
    
    // Inicializa vozes
    for (int i = 0; i < MAX_VOICES; i++) {
        s_voices[i].active = false;
        s_voices[i].note = 0;
        s_voices[i].velocity = 0;
        s_voices[i].frequency = 0.0f;
        s_voices[i].phase = 0.0f;
        s_voices[i].amplitude = 0.0f;
        s_voices[i].filterState = 0.0f;
        s_voices[i].noteOnTime = 0;
        s_voices[i].noteOffTime = 0;
        s_voices[i].releasing = false;
    }
    
    // Inicializa buffers circulares
    for (int i = 0; i < DAC_BUFFER_SIZE; i++) {
        s_waveBuffer[i] = 0.0f;
        s_doublerBuffer[i] = 0.0f;
        s_outBuffer[i] = 0;
    }
    s_bufferIndex = 0;
    
    s_filterState[0] = 0.0f;
    s_filterState[1] = 0.0f;
    
    // Inicializa parâmetros padrão
    synthesizer_reset_to_defaults();
    
    s_initialized = true;
}

void synthesizer_tick() {
    if (!s_initialized || !s_active) return;
    
    // Processa MIDI diretamente no synth para reduzir latência
    extern void midi_gate_tick(void);
    midi_gate_tick();
    
    // Gera áudio com normalização para evitar distorção
    for (int sample = 0; sample < DAC_BUFFER_SIZE; sample++) {
        float output = 0.0f;
        int activeVoices = 0;
        
        // Processa cada voz ativa
        for (int v = 0; v < MAX_VOICES; v++) {
            SynthVoice& voice = s_voices[v];
            if (!voice.active) continue;
            
            // Gera onda
            float wave = generate_waveform(s_params.waveform, voice.phase);
            
            // Volume baseado na velocity
            wave *= (float)voice.velocity / 127.0f * 0.3f;
            
            output += wave;
            activeVoices++;
            
            // Atualiza fase
            if (voice.active) {
                voice.phase += voice.frequency / SAMPLE_RATE;
                if (voice.phase >= 1.0f) {
                    voice.phase -= 1.0f;
                }
            }
        }
        
        // Normalização inteligente para evitar distorção
        if (activeVoices > 0) {
            // Normaliza por número de vozes ativas
            output = output / activeVoices;
            
            // Limita o volume máximo para evitar clipping
            if (activeVoices > 4) {
                output *= 0.8f; // Reduz volume quando há muitas vozes
            }
        }
        
        // Soft clipping para qualidade
        if (output > 1.0f) output = 1.0f;
        else if (output < -1.0f) output = -1.0f;
        
        // Converte para 16-bit
        s_outBuffer[sample] = (int16_t)(output * 32767.0f);
    }
    
    // Envia para DAC
    dac_play_buffer(s_outBuffer, DAC_BUFFER_SIZE * sizeof(int16_t));
}


bool synthesizer_is_active() {
    return s_active;
}

void synthesizer_set_active(bool enabled) {
    s_active = enabled;
    if (!enabled) {
        synthesizer_all_notes_off();
    }
}

void synthesizer_note_on(uint8_t note, uint8_t velocity) {
    if (note > 127 || velocity > 127) return;
    
    int voiceIndex = -1;
    uint32_t oldestTime = UINT32_MAX;
    int oldestIndex = 0;
    
    // Primeiro: procura voz livre
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!s_voices[i].active) {
            voiceIndex = i;
            break;
        }
    }
    
    // Se não há voz livre, encontra a mais antiga
    if (voiceIndex == -1) {
        for (int i = 0; i < MAX_VOICES; i++) {
            if (s_voices[i].noteOnTime < oldestTime) {
                oldestTime = s_voices[i].noteOnTime;
                oldestIndex = i;
            }
        }
        voiceIndex = oldestIndex;
    }
    
    SynthVoice& voice = s_voices[voiceIndex];
    voice.active = true;
    voice.note = note;
    voice.velocity = velocity;
    voice.frequency = note_to_frequency(note);
    voice.phase = 0.0f;
    voice.noteOnTime = micros();
    voice.noteOffTime = 0;
    voice.releasing = false;
}

void synthesizer_note_off(uint8_t note) {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (s_voices[i].active && s_voices[i].note == note) {
            s_voices[i].active = false;
            s_voices[i].releasing = false;
            s_voices[i].note = 0; // Limpar nota
            s_voices[i].velocity = 0; // Limpar velocity
            s_voices[i].frequency = 0.0f; // Limpar frequência
            s_voices[i].phase = 0.0f; // Limpar fase
        }
    }
}

void synthesizer_all_notes_off() {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (s_voices[i].active) {
            s_voices[i].active = false;
            s_voices[i].releasing = false;
            s_voices[i].note = 0; // Limpar nota
            s_voices[i].velocity = 0; // Limpar velocity
            s_voices[i].frequency = 0.0f; // Limpar frequência
            s_voices[i].phase = 0.0f; // Limpar fase
        }
    }
}

void synthesizer_set_waveform(SynthWaveform wave) {
    s_params.waveform = wave;
}

void synthesizer_set_filter_type(SynthFilterType type) {
    s_params.filterType = type;
}

void synthesizer_set_filter_cutoff(float cutoff) {
    s_params.filterCutoff = fmaxf(0.0f, fminf(1.0f, cutoff));
}

void synthesizer_set_filter_resonance(float resonance) {
    s_params.filterResonance = fmaxf(0.0f, fminf(1.0f, resonance));
}


void synthesizer_set_mod_wheel(float mod) {
    s_params.modWheel = fmaxf(0.0f, fminf(1.0f, mod));
}

void synthesizer_set_amp_envelope(const SynthEnvelope& env) {
    s_params.ampEnvelope = env;
}

void synthesizer_set_filter_envelope(const SynthEnvelope& env) {
    s_params.filterEnvelope = env;
}

const SynthParams& synthesizer_get_params() {
    return s_params;
}

void synthesizer_set_params(const SynthParams& params) {
    s_params = params;
}

void synthesizer_update_from_pots() {
    if (!s_initialized || !s_active) return;
    
    static uint32_t lastPotRead = 0;
    uint32_t now = millis();
    if (now - lastPotRead < 5) return;
    lastPotRead = now;
    
    int pot1Value = analogRead(POT1);
    float doublerOctave = map(pot1Value, 0, 4095, 0.0f, 3.0f);
    
    if (pot1Value > 1) {
        s_params.doublerOctave = doublerOctave;
        s_params.doublerEnabled = true;
    } else {
        s_params.doublerEnabled = false;
        s_params.doublerOctave = 0.0f;
    }
}

uint8_t synthesizer_get_active_voice_count() {
    uint8_t count = 0;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (s_voices[i].active) count++;
    }
    return count;
}


void synthesizer_reset_to_defaults() {
    s_params.waveform = SYNTH_WAVE_SAW;
    s_params.filterType = SYNTH_FILTER_LOWPASS;
    s_params.filterCutoff = 0.5f;
    s_params.filterResonance = 0.5f;
    s_params.ampEnvelope = {0.0f, 0.0f, 1.0f, 0.0f};
    s_params.filterEnvelope = {0.0f, 0.0f, 1.0f, 0.0f};
    s_params.lfoRate = 0.0f;
    s_params.lfoAmount = 0.0f;
    s_params.modWheel = 0.0f;
    s_params.doublerEnabled = false;
    s_params.doublerOctave = 0.0f;
}

