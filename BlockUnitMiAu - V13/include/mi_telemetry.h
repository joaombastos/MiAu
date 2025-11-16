#ifndef MI_TELEMETRY_H
#define MI_TELEMETRY_H

#include <stdint.h>
#include <Arduino.h>
#include "au_telemetry.h"

// Estrutura de dados de telemetria recebidos do AU
typedef struct {
    AuTelemetryData data;      // Dados recebidos do AU
    bool data_valid;           // Se os dados são válidos
    uint32_t last_receive;     // Timestamp da última receção
} MiTelemetryData;

// Inicializar sistema de telemetria
void mi_telemetry_begin(void);

// Processar dados recebidos via UART
void mi_telemetry_tick(void);

// Obter dados atuais
MiTelemetryData* mi_telemetry_get_data(void);

// Obter nome do modo ativo
const char* mi_telemetry_get_mode_name(AuMode mode);

// Obter nome do tipo de onda
const char* mi_telemetry_get_wave_type_name(uint8_t wave_type);

#endif
