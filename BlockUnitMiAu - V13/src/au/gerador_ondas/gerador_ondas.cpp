#include <Arduino.h>
#include <math.h>
#include "gerador_ondas/gerador_ondas.h"
#include "DAC.h"
#include "pinos.h"
#include "midi_gate/midi_gate.h"

// Suavização das leituras dos potenciômetros
#ifndef POT_SMOOTH_ALPHA
#define POT_SMOOTH_ALPHA 1.0f //maximo 1.0
#endif

// Curva de mapeamento pot->frequência (gamma > 1 dá mais graves no curso)
#ifndef POT_FREQ_GAMMA
#define POT_FREQ_GAMMA 0.4f
#endif

// Envelope de amplitude (suaviza clicks): tempos em milissegundos
#ifndef AMP_ATTACK_MS
#define AMP_ATTACK_MS 0.8f
#endif
#ifndef AMP_RELEASE_MS
#define AMP_RELEASE_MS 4.0f
#endif

static bool s_initialized = false;
static int16_t s_outBuffer[DAC_BUFFER_SIZE];

// Fases independentes para 3 osciladores
static float s_phase[3] = {0.0f, 0.0f, 0.0f}; // faixa 0..1

// Pinos dos potenciômetros
static const int s_potPins[3] = {POT1, POT2, POT3};
static float s_potFiltered[3] = {0.0f, 0.0f, 0.0f};
static float s_freqFiltered[3] = {0.0f, 0.0f, 0.0f};
static bool s_prevActive[3] = {false, false, false};
// Envelope de amplitude por oscilador (0..1)
static float s_amp[3] = {0.0f, 0.0f, 0.0f};

// Waveform
static GeradorWaveform s_waveform = GWF_SQUARE;

// Função simples para quantizar frequência para nota mais próxima
static inline float quantizeToNote(float hz) {
    if (hz < 1.0f) hz = 1.0f;
    if (hz > 4000.0f) hz = 4000.0f;
    float midi = 69.0f + 12.0f * log2f(hz / 440.0f);
    int midiRound = (int)floorf(midi + 0.5f);
    return 440.0f * powf(2.0f, (midiRound - 69) / 12.0f);
}

// Conversão de pot -> frequência (Hz) linear
static inline float mapPotToFrequency(int potValue) {
    // Faixa log: 1 Hz a 4000 Hz (gamma controla "cintura" nos graves)
    const float minHz = 1.0f;
    const float maxHz = 4000.0f;
    float v = (float)potValue / 4095.0f; // 0..1
    float vShaped = powf(v, POT_FREQ_GAMMA);
    return minHz * powf((maxHz / minHz), vShaped);
}


// Tabela de seno para máxima estabilidade
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

static inline float genSample(float phase, GeradorWaveform wf) {
    switch (wf) {
        case GWF_SINE: {
            int idx = (int)(phase * 256.0f) & 0xFF;
            return s_sineTable[idx];
        }
        case GWF_TRIANGLE: return (phase < 0.5f) ? (phase * 4.0f - 1.0f) : ((1.0f - phase) * 4.0f - 1.0f);
        case GWF_SAW:      return (phase * 2.0f) - 1.0f;
        case GWF_SQUARE:   return (phase < 0.5f) ? 1.0f : -1.0f;
        default:           return (phase < 0.5f) ? 1.0f : -1.0f;
    }
}

void gerador_ondas_begin() {
    if (s_initialized) return;
    dac_init();
    for (int i = 0; i < 3; ++i) {
        pinMode(s_potPins[i], INPUT);
        s_phase[i] = 0.0f;
        int raw = analogRead(s_potPins[i]);
        s_potFiltered[i] = (float)raw;
        s_freqFiltered[i] = 0.0f;
        s_prevActive[i] = false;
    }
    s_initialized = true;
}

void gerador_ondas_set_waveform(GeradorWaveform wf) {
    s_waveform = wf;
}


GeradorWaveform gerador_ondas_get_waveform() {
    return s_waveform;
}

void gerador_ondas_get_frequencies(float outHz[3]) {
    if (!outHz) return;
    outHz[0] = s_freqFiltered[0];
    outHz[1] = s_freqFiltered[1];
    outHz[2] = s_freqFiltered[2];
}

void gerador_ondas_tick() {
    if (!s_initialized) return;

    // Verifica se está no modo ONDAS (gate desabilitado)
    const bool isOndasMode = !midi_gate_is_gate_mode_enabled();
    
    // Reset envelopes quando muda de modo para evitar estado inconsistente
    static bool prevOndasMode = isOndasMode;
    static bool firstRun = true;
    if (firstRun || prevOndasMode != isOndasMode) {
        for (int i = 0; i < 3; ++i) {
            s_amp[i] = 0.0f; // Reset amplitude
            s_phase[i] = 0.0f; // Reset phase também
        }
        prevOndasMode = isOndasMode;
        firstRun = false;
    }

    if (isOndasMode) {
        // MODO ONDAS ULTRA SIMPLES: leitura com suavização mínima
        for (int i = 0; i < 3; ++i) {
            int raw = analogRead(s_potPins[i]);
            // Suavização para reduzir ruído dos potenciômetros
            s_potFiltered[i] += 0.15f * ((float)raw - s_potFiltered[i]);
            
            if (s_potFiltered[i] > 500.0f) {
                // Mapeamento linear simples para evitar instabilidade
                float v = (s_potFiltered[i] - 500.0f) / (4095.0f - 500.0f);
                v = fmaxf(0.0f, fminf(1.0f, v));
                float freq = 1.0f + v * 3999.0f; // 1Hz a 4000Hz
                
                // Aplica quantização musical se for modo NOTE
                if (s_waveform == GWF_NOTE) {
                    s_freqFiltered[i] = quantizeToNote(freq);
                } else {
                    s_freqFiltered[i] = freq;
                }
            } else {
                s_freqFiltered[i] = 0.0f;
            }
        }
    } else {
        // Outros modos: lógica complexa original
        int potValues[3];
        bool active[3];
        float freqHz[3];
        const int threshold = (int)(0.05f * 4095.0f);

        for (int i = 0; i < 3; ++i) {
            int raw = analogRead(s_potPins[i]);
            s_potFiltered[i] += POT_SMOOTH_ALPHA * ((float)raw - s_potFiltered[i]);
            potValues[i] = (int)(s_potFiltered[i] + 0.5f);
            bool potActive = potValues[i] > threshold;
            active[i] = potActive;
            float target = active[i] ? mapPotToFrequency(potValues[i]) : 0.0f;
            
            if (s_waveform == GWF_NOTE && active[i]) {
                target = quantizeToNote(target);
            }
            
            if (!active[i]) {
                s_freqFiltered[i] = 0.0f;
            } else {
                if (s_waveform == GWF_NOTE) {
                    if (!s_prevActive[i]) {
                        s_freqFiltered[i] = target;
                    } else {
                        const float alphaNote = 0.12f;
                        s_freqFiltered[i] += alphaNote * (target - s_freqFiltered[i]);
                    }
                } else if (!s_prevActive[i]) {
                    s_freqFiltered[i] = target;
                } else {
                    float alphaF = (target < 120.0f) ? 0.06f : 0.20f;
                    s_freqFiltered[i] += alphaF * (target - s_freqFiltered[i]);
                }
            }
            freqHz[i] = s_freqFiltered[i];
            s_prevActive[i] = active[i];
        }
    }

    const float sampleRate = (float)DAC_SAMPLE_RATE;

    // Variáveis para outros modos (não usadas no modo ONDAS)
    const bool gateMode = midi_gate_is_gate_mode_enabled();
    bool gateLocal[3] = {false, false, false};
    const uint32_t blockStartUs = micros();
    const double microsPerSample = 1000000.0 / (double)DAC_SAMPLE_RATE;
    const float attackAlpha = 1.0f - expf(-1.0f / (sampleRate * (AMP_ATTACK_MS * 0.001f)));
    const float releaseAlpha = 1.0f - expf(-1.0f / (sampleRate * (AMP_RELEASE_MS * 0.001f)));

    if (!isOndasMode) {
        // Inicializar gates apenas para outros modos
        gateLocal[0] = midi_gate_is_open(0);
        gateLocal[1] = midi_gate_is_open(1);
        gateLocal[2] = midi_gate_is_open(2);

        auto channelToOscIndexLocal = [](uint8_t ch) -> int {
            if (ch == 3) return 0;
            if (ch == 4) return 1;
            if (ch == 5) return 2;
            return -1;
        };

        // Aplica eventos MIDI apenas para outros modos
        MidiGateEvent ev;
        while (midi_gate_peek_event(&ev)) {
            if (ev.timestamp_us > blockStartUs) break;
            if (ev.type == MIDI_GATE_EVT_STOP) {
                gateLocal[0] = gateLocal[1] = gateLocal[2] = false;
            } else if (ev.type == MIDI_GATE_EVT_NOTE_ON || ev.type == MIDI_GATE_EVT_NOTE_OFF) {
                int idx = channelToOscIndexLocal(ev.channel);
                if (idx >= 0 && idx < 3) {
                    gateLocal[idx] = (ev.type == MIDI_GATE_EVT_NOTE_ON);
                }
            }
            midi_gate_pop_event();
        }
    }

    for (int i = 0; i < DAC_BUFFER_SIZE; i += 2) {
        float mixed = 0.0f;

        if (!isOndasMode) {
            // Aplica eventos MIDI apenas para outros modos
            MidiGateEvent ev;
            const uint32_t sampleUs = blockStartUs + (uint32_t)((double)(i / 2) * microsPerSample);
            while (midi_gate_peek_event(&ev)) {
                if (ev.timestamp_us > sampleUs) break;
                if (ev.type == MIDI_GATE_EVT_STOP) {
                    gateLocal[0] = gateLocal[1] = gateLocal[2] = false;
                } else if (ev.type == MIDI_GATE_EVT_NOTE_ON || ev.type == MIDI_GATE_EVT_NOTE_OFF) {
                    auto channelToOscIndexLocal = [](uint8_t ch) -> int {
                        if (ch == 3) return 0;
                        if (ch == 4) return 1;
                        if (ch == 5) return 2;
                        return -1;
                    };
                    int idx = channelToOscIndexLocal(ev.channel);
                    if (idx >= 0 && idx < 3) {
                        gateLocal[idx] = (ev.type == MIDI_GATE_EVT_NOTE_ON);
                    }
                }
                midi_gate_pop_event();
            }
        }

        if (isOndasMode) {
            // MODO ONDAS ULTRA SIMPLES: amplitude ajustada para evitar clipping
            for (int v = 0; v < 3; ++v) {
                if (s_freqFiltered[v] > 0.0f) {
                    s_phase[v] += s_freqFiltered[v] / sampleRate;
                    // Reset periódico para evitar drift
                    if (s_phase[v] >= 1.0f) {
                        s_phase[v] = fmodf(s_phase[v], 1.0f);
                    }
                    // Converte GWF_NOTE para GWF_SINE no modo ONDAS
                    GeradorWaveform wf = (s_waveform == GWF_NOTE) ? GWF_SINE : s_waveform;
                    mixed += genSample(s_phase[v], wf) * 0.3f;
                }
            }
        } else {
            // Outros modos: gate controlado por MIDI, amplitude fixa
            for (int v = 0; v < 3; ++v) {
                // No modo gate: só toca se potenciômetro ativo E gate MIDI ativo
                // Em outros modos: toca se potenciômetro ativo
                bool potActive = s_freqFiltered[v] > 0.0f;
                bool targetActive = gateMode ? (potActive && gateLocal[v]) : potActive;
                float targetAmp = targetActive ? 1.0f : 0.0f;
                
                // Envelope suave para eliminar clicks
                float a = (targetAmp > s_amp[v]) ? 0.2f : 0.15f; // Attack e release mais suaves
                s_amp[v] += a * (targetAmp - s_amp[v]);

                if (s_amp[v] > 0.001f && s_freqFiltered[v] > 0.0f) {
                    s_phase[v] += s_freqFiltered[v] / sampleRate;
                    if (s_phase[v] >= 1.0f) s_phase[v] -= 1.0f;
                    // Converte GWF_NOTE para GWF_SINE nos outros modos também
                    GeradorWaveform wf = (s_waveform == GWF_NOTE) ? GWF_SINE : s_waveform;
                    mixed += genSample(s_phase[v], wf) * 0.3f;
                }
            }
        }

        // Converte para 16-bit com soft clipping
        float clamped = mixed;
        if (clamped > 1.0f) clamped = 1.0f;
        if (clamped < -1.0f) clamped = -1.0f;
        int16_t s16 = (int16_t)(clamped * 32767.0f);
        s_outBuffer[i] = s16;       // L
        s_outBuffer[i + 1] = s16;   // R
    }

    dac_play_buffer(s_outBuffer, DAC_BUFFER_SIZE * sizeof(int16_t));
}


