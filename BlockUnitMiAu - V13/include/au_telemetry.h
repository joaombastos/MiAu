#ifndef AU_TELEMETRY_H
#define AU_TELEMETRY_H

#include <stdint.h>
#include <Arduino.h>

// Tipos de modos do AU
typedef enum {
    AU_MODE_MENU = 0,
    AU_MODE_ONDAS = 1,
    AU_MODE_GATE = 2,
    AU_MODE_DRUM = 3,
    AU_MODE_SLICE = 4,
    AU_MODE_SYNTH = 5
} AuMode;

// Estrutura para modo ONDAS
typedef struct {
    uint16_t freq_osc1;        // Frequência oscilador 1 (Hz)
    uint16_t freq_osc2;        // Frequência oscilador 2 (Hz)
    uint16_t freq_osc3;        // Frequência oscilador 3 (Hz)
    uint8_t wave_type_osc1;    // Tipo de onda oscilador 1 (0=quadrada, 1=triangular, 2=dente de serra)
    uint8_t wave_type_osc2;    // Tipo de onda oscilador 2
    uint8_t wave_type_osc3;    // Tipo de onda oscilador 3
    uint8_t midi_channel;      // Canal MIDI que recebe
} AuOndasData;

// Estrutura para modo GATE
typedef struct {
    uint8_t midi_channel;      // Canal MIDI que recebe
    uint8_t gate_state_osc1;   // Estado do gate oscilador 1 (0/1)
    uint8_t gate_state_osc2;   // Estado do gate oscilador 2 (0/1)
    uint8_t gate_state_osc3;   // Estado do gate oscilador 3 (0/1)
} AuGateData;

// Estrutura para modo DRUM
typedef struct {
    uint8_t midi_channel;      // Canal MIDI que recebe
    uint8_t current_bank;      // Banco atual (0-7)
    uint8_t active_voices;     // Número de vozes ativas
    uint8_t current_sample;    // Sample atual (1-16)
} AuDrumData;

// Estrutura para modo SLICE
typedef struct {
    uint8_t midi_channel;      // Canal MIDI que recebe
    uint8_t current_bank;      // Banco atual (0-15)
    uint8_t active_voices;     // Número de vozes ativas
} AuSliceData;

// Estrutura para modo SYNTH
typedef struct {
    uint8_t midi_channel;      // Canal MIDI que recebe
    uint8_t wave_type;         // Tipo de onda (0=quadrada, 1=triangular, 2=dente de serra)
    uint16_t frequency;        // Frequência base
    uint8_t filter_cutoff;     // Frequência de corte do filtro (0-127)
    uint8_t filter_resonance;  // Ressonância do filtro (0-127)
} AuSynthData;

// Estrutura principal de telemetria
typedef struct {
    AuMode current_mode;       // Modo atual do AU
    uint32_t timestamp;        // Timestamp da leitura
    uint8_t menu_selected_option; // Opção selecionada no menu (0-4)
    uint8_t dummy[16];         // Dados dummy para manter tamanho fixo
} AuTelemetryData;

// Inicializar sistema de telemetria
void au_telemetry_begin(void);

// Atualizar e enviar dados de telemetria
void au_telemetry_tick(void);

// Obter dados atuais
AuTelemetryData* au_telemetry_get_data(void);

// Definir modo atual
void au_telemetry_set_mode(AuMode mode);

#endif
