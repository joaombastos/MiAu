#include "au_telemetry.h"
#include "pinos.h"
#include "gerador_ondas/gerador_ondas.h"
#include "midi_gate/midi_gate.h"
#include "dmachine/dmachine.h"
#include "slice/slice.h"
#include "synthesizer/synthesizer.h"
#include "au_encoder_control.h"

static AuTelemetryData s_telemetry_data;
static bool s_initialized = false;
static unsigned long s_last_send = 0;
static const unsigned long SEND_INTERVAL = 10; // Enviar a cada 10ms - REDUZIDO PARA MENOR LATÊNCIA

void au_telemetry_begin(void) {
    if (s_initialized) return;
    
    // Inicializar UART para comunicação com MI
    Serial2.begin(115200, SERIAL_8N1, -1, UART_TX_AU);
    
    // Inicializar controle do encoder
    au_encoder_control_begin();
    
    // Inicializar estrutura de dados
    memset(&s_telemetry_data, 0, sizeof(AuTelemetryData));
    
    // Começar no modo MENU_PRINCIPAL
    s_telemetry_data.current_mode = AU_MODE_MENU; // 0 = MENU_PRINCIPAL
    s_telemetry_data.menu_selected_option = 0;
    
    s_initialized = true;
}

void au_telemetry_tick(void) {
    if (!s_initialized) return;
    
    // Processar controle do encoder
    au_encoder_control_tick();
    
    // Atualizar timestamp
    s_telemetry_data.timestamp = millis();
    
    // Atualizar dados do menu (sempre)
    s_telemetry_data.menu_selected_option = au_encoder_control_get_menu_selection();
    
    // Modo menu - apenas navegação
    // Dados específicos dos modos removidos para simplificar
    
    // Enviar dados periodicamente
    unsigned long now = millis();
    if (now - s_last_send >= SEND_INTERVAL) {
                // Enviar dados via UART no protocolo antigo
                uint8_t buf[1 + 1 + 1 + 1 + 1 + 12 + 1 + 1]; // 18 bytes total
                buf[0] = 0xAA; // Marcador
                buf[1] = 0x00; // Flags
                buf[2] = s_telemetry_data.current_mode; // Modo
                // Obter tipo de onda atual
                uint8_t current_wave = 0;
                if (s_telemetry_data.current_mode == AU_MODE_ONDAS || s_telemetry_data.current_mode == AU_MODE_GATE) {
                    extern GeradorWaveform gerador_ondas_get_waveform(void);
                    current_wave = (uint8_t)gerador_ondas_get_waveform();
                }
                buf[3] = current_wave; // Wave atual
                buf[4] = s_telemetry_data.menu_selected_option; // Menu selection
        
        // 12 bytes de frequências (3 x 4 bytes float)
        float freqs[3] = {440.0f, 880.0f, 1320.0f};
        if (s_telemetry_data.current_mode == AU_MODE_ONDAS || s_telemetry_data.current_mode == AU_MODE_GATE) {
            extern void gerador_ondas_get_frequencies(float outHz[3]);
            gerador_ondas_get_frequencies(freqs);
        } else if (s_telemetry_data.current_mode == AU_MODE_DRUM) {
            // Para modo DRUM, usar dados do dmachine
            freqs[0] = (float)dmachine_get_bank_index(); // Banco atual
            freqs[1] = 0.0f; // Placeholder
            freqs[2] = 0.0f; // Placeholder
        } else if (s_telemetry_data.current_mode == AU_MODE_SLICE) {
            // Para modo SLICE, usar dados do slice
            extern int slice_get_file_index(void);
            freqs[0] = (float)slice_get_file_index(); // Ficheiro atual
            freqs[1] = 0.0f; // Placeholder
            freqs[2] = 0.0f; // Placeholder
        } else if (s_telemetry_data.current_mode == AU_MODE_SYNTH) {
            // Para modo SYNTH, usar dados do synthesizer
            extern const SynthParams& synthesizer_get_params(void);
            const SynthParams& params = synthesizer_get_params();
            freqs[0] = (float)params.waveform; // Tipo de onda atual
            freqs[1] = 0.0f; // Placeholder
            freqs[2] = 0.0f; // Placeholder
        }
        for (int i = 0; i < 3; i++) {
            union { float f; uint8_t b[4]; } u;
            u.f = freqs[i];
            buf[5 + i*4 + 0] = u.b[0]; // +1 para menu_selection
            buf[5 + i*4 + 1] = u.b[1];
            buf[5 + i*4 + 2] = u.b[2];
            buf[5 + i*4 + 3] = u.b[3];
        }
        
        buf[5 + 12] = 1; // Banco (+1 para menu_selection)
        
        // Checksum
        uint8_t x = 0;
        for (size_t i = 0; i < sizeof(buf) - 1; ++i) x ^= buf[i];
        buf[sizeof(buf) - 1] = x;
        
                    Serial2.write(buf, sizeof(buf));
        s_last_send = now;
    }
}

AuTelemetryData* au_telemetry_get_data(void) {
    return &s_telemetry_data;
}

void au_telemetry_set_mode(AuMode mode) {
    if (!s_initialized) return;
    
    s_telemetry_data.current_mode = mode;
    
    // Se voltar ao menu, resetar seleção
    if (mode == AU_MODE_MENU) {
        s_telemetry_data.menu_selected_option = 0;
    }
}
