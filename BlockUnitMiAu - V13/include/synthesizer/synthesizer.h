#ifndef SYNTHESIZER_H
#define SYNTHESIZER_H

#include <Arduino.h>

// Otimizações para PSRAM e buffers circulares
#define USE_PSRAM_BUFFERS 1
#define CIRCULAR_BUFFER_SIZE 512

// Tipos de onda do sintetizador
typedef enum {
    SYNTH_WAVE_SINE = 0,
    SYNTH_WAVE_SAW = 1,
    SYNTH_WAVE_SQUARE = 2,
    SYNTH_WAVE_TRIANGLE = 3,
    SYNTH_WAVE_NOISE = 4
} SynthWaveform;

// Tipos de filtro
typedef enum {
    SYNTH_FILTER_LOWPASS = 0,
    SYNTH_FILTER_HIGHPASS = 1,
    SYNTH_FILTER_BANDPASS = 2
} SynthFilterType;

// Estrutura do envelope ADSR
struct SynthEnvelope {
    float attack;   // 0.0 - 1.0
    float decay;    // 0.0 - 1.0
    float sustain;  // 0.0 - 1.0
    float release;  // 0.0 - 1.0
};

// Estrutura de parâmetros do sintetizador
struct SynthParams {
    SynthWaveform waveform;
    SynthFilterType filterType;
    float filterCutoff;     // 0.0 - 1.0
    float filterResonance;  // 0.0 - 1.0
    SynthEnvelope ampEnvelope;
    SynthEnvelope filterEnvelope;
    float lfoRate;          // 0.0 - 1.0
    float lfoAmount;        // 0.0 - 1.0
    float modWheel;         // 0.0 - 1.0
    bool doublerEnabled;    // Se o doubler está ativo
    float doublerOctave;    // Oitava do doubler (0.0 a +3.0)
    bool ringModEnabled;    // Se o ring modulation está ativo
    float ringModFreq;      // Frequência do ring modulation (0.1 - 20.0 Hz)
    bool fmEnabled;         // Se o FM synthesis está ativo
    float fmRatio;          // Ratio do FM (0.1 - 4.0)
    float fmAmount;         // Amount do FM (0.0 - 1.0)
    bool degradeEnabled;    // Se o Degrade está ativo
    float degradeBits;      // Bit depth do Degrade (2.0 - 16.0)
    float degradeSampleRate; // Sample rate do Degrade (6kHz - 48kHz)
    float degradeNoise;     // Ruído de quantização (0.0 - 0.5)
    bool randomNotesEnabled; // Se as random notes estão ativas
    float randomInterval;   // Intervalo das random notes (0.0 - 1.0)
};

// Estrutura de uma voz do sintetizador
struct SynthVoice {
    bool active;
    uint8_t note;           // Nota MIDI (0-127)
    uint8_t velocity;       // Velocity MIDI (0-127)
    float frequency;        // Frequência calculada
    float phase;            // Fase do oscilador (0.0 - 1.0)
    float amplitude;        // Amplitude atual (0.0 - 1.0)
    float filterState;      // Estado do filtro
    uint32_t noteOnTime;    // Timestamp do Note On
    uint32_t noteOffTime;   // Timestamp do Note Off
    bool releasing;         // Se está na fase de release
};

// Inicia o modo sintetizador
void synthesizer_begin();

// Processa o sintetizador; chamar apenas quando ativo
void synthesizer_tick();

// Estado do modo sintetizador
bool synthesizer_is_active();
void synthesizer_set_active(bool enabled);

// Controle de vozes
void synthesizer_note_on(uint8_t note, uint8_t velocity);
void synthesizer_note_off(uint8_t note);
void synthesizer_all_notes_off();

// Controle de parâmetros
void synthesizer_set_waveform(SynthWaveform wave);
void synthesizer_set_filter_type(SynthFilterType type);
void synthesizer_set_filter_cutoff(float cutoff);
void synthesizer_set_filter_resonance(float resonance);
void synthesizer_set_lfo_rate(float rate);
void synthesizer_set_lfo_amount(float amount);
void synthesizer_set_mod_wheel(float mod);

// Controle de envelope
void synthesizer_set_amp_envelope(const SynthEnvelope& env);
void synthesizer_set_filter_envelope(const SynthEnvelope& env);

// Acesso aos parâmetros atuais
const SynthParams& synthesizer_get_params();
void synthesizer_set_params(const SynthParams& params);

// Controle via potenciômetros (quando ativo)
void synthesizer_update_from_pots();

// Número de vozes ativas
uint8_t synthesizer_get_active_voice_count();

// Reset para valores padrão
void synthesizer_reset_to_defaults();

// Debug do doubler
void synthesizer_debug_doubler();

#endif // SYNTHESIZER_H
