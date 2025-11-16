#include "tom_inicial/tom_inicial.h"
#include "DAC.h"

static bool s_initialized = false;
static int16_t s_buffer[DAC_BUFFER_SIZE];
static float s_phase = 0.0f;      // 0..1
static float s_frequency_hz = 440.0f;

void tom_inicial_play(uint32_t duration_ms) {
    if (!s_initialized) {
        dac_init();
        s_phase = 0.0f;
        s_initialized = true;
    }

    const float sample_rate = (float)DAC_SAMPLE_RATE;
    const uint32_t start_ms = millis();
    while ((millis() - start_ms) < duration_ms) {
        for (int i = 0; i < DAC_BUFFER_SIZE; i += 2) {
            // Avança fase
            s_phase += s_frequency_hz / sample_rate;
            if (s_phase >= 1.0f) s_phase -= 1.0f;
            
            // Onda quadrada centrada
            float sample = (s_phase < 0.5f) ? 0.5f : -0.5f;
            int16_t s16 = (int16_t)(sample * 32767.0f);
            s_buffer[i] = s16;
            s_buffer[i + 1] = s16;
        }
        dac_play_buffer(s_buffer, DAC_BUFFER_SIZE * sizeof(int16_t));
    }
}


