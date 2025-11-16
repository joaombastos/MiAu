#ifndef AU_ENCODER_CONTROL_H
#define AU_ENCODER_CONTROL_H

#include <stdint.h>
#include <Arduino.h>
#include "au_telemetry.h"

// Estados de controle do encoder
typedef enum {
    ENCODER_STATE_IDLE,
    ENCODER_STATE_SELECTING_PARAM,
    ENCODER_STATE_EDITING_VALUE,
    ENCODER_STATE_MENU_NAVIGATION,
    ENCODER_STATE_MODE_ACTIVE
} EncoderControlState;

// Parâmetros editáveis por modo
typedef enum {
    // ONDAS
    PARAM_ONDAS_FREQ_OSC1 = 0,
    PARAM_ONDAS_FREQ_OSC2 = 1,
    PARAM_ONDAS_FREQ_OSC3 = 2,
    PARAM_ONDAS_WAVE_OSC1 = 3,
    PARAM_ONDAS_WAVE_OSC2 = 4,
    PARAM_ONDAS_WAVE_OSC3 = 5,
    
    // DRUM
    PARAM_DRUM_BANK = 10,
    
    // SLICE
    PARAM_SLICE_BANK = 20,
    
    // SYNTH
    PARAM_SYNTH_WAVE_TYPE = 30,
    
    PARAM_COUNT
} EncoderParam;

// Inicializar sistema de controle do encoder
void au_encoder_control_begin(void);

// Processar encoder (chamar no loop principal)
void au_encoder_control_tick(void);

// Obter parâmetro atual sendo editado
EncoderParam au_encoder_control_get_current_param(void);

// Obter nome do parâmetro
const char* au_encoder_control_get_param_name(EncoderParam param);

// Obter valor atual do parâmetro
int32_t au_encoder_control_get_param_value(EncoderParam param);

// Definir valor do parâmetro
void au_encoder_control_set_param_value(EncoderParam param, int32_t value);

// Funções do menu
uint8_t au_encoder_control_get_menu_selection(void);
void au_encoder_control_set_menu_selection(uint8_t selection);

#endif
