#include "mi_telemetry.h"
#include "pinos.h"
#include "oled.h"

static MiTelemetryData s_telemetry_data;
static bool s_initialized = false;
static uint8_t s_rx_buffer[sizeof(AuTelemetryData)];
static uint8_t s_rx_pos = 0;

void mi_telemetry_begin(void) {
    if (s_initialized) return;
    
    // Inicializar UART para comunicação com AU
    Serial1.begin(115200, SERIAL_8N1, -1, UART_RX_MI);
    
    // Inicializar estrutura de dados
    memset(&s_telemetry_data, 0, sizeof(MiTelemetryData));
    s_telemetry_data.data_valid = false;
    
    s_initialized = true;
}

void mi_telemetry_tick(void) {
    if (!s_initialized) return;
    
    // Ler dados disponíveis do UART
    while (Serial1.available() > 0) {
        uint8_t byte = Serial1.read();
        
        // Adicionar byte ao buffer
        if (s_rx_pos < sizeof(s_rx_buffer)) {
            s_rx_buffer[s_rx_pos++] = byte;
        }
        
        // Se buffer completo, processar dados
        if (s_rx_pos >= sizeof(AuTelemetryData)) {
            // Copiar dados para estrutura
            memcpy(&s_telemetry_data.data, s_rx_buffer, sizeof(AuTelemetryData));
            s_telemetry_data.data_valid = true;
            s_telemetry_data.last_receive = millis();
            
            // Resetar buffer
            s_rx_pos = 0;
        }
    }
    
    // Timeout removido
}

MiTelemetryData* mi_telemetry_get_data(void) {
    return &s_telemetry_data;
}

const char* mi_telemetry_get_mode_name(AuMode mode) {
    switch (mode) {
        case AU_MODE_ONDAS: return "ONDAS";
        case AU_MODE_GATE: return "GATE";
        case AU_MODE_DRUM: return "DRUM";
        case AU_MODE_SLICE: return "SLICE";
        case AU_MODE_SYNTH: return "SYNTH";
        default: return "UNKNOWN";
    }
}

const char* mi_telemetry_get_wave_type_name(uint8_t wave_type) {
    switch (wave_type) {
        case 0: return "QUAD";
        case 1: return "TRI";
        case 2: return "SAW";
        default: return "UNK";
    }
}
