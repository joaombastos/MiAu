#include "au_encoder_control.h"
#include "pinos.h"
#include "au_telemetry.h"
#include "gerador_ondas/gerador_ondas.h"
#include "synthesizer/synthesizer.h"
#include <ESP32Encoder.h>

// Estado do controle do encoder
static bool s_initialized = false;
static EncoderControlState s_state = ENCODER_STATE_MENU_NAVIGATION;
static EncoderParam s_current_param = PARAM_ONDAS_FREQ_OSC1;
static int32_t s_current_value = 0;
static int32_t s_min_value = 0;
static int32_t s_max_value = 100;

// Sistema de debounce do encoder (como no MI)
#define ENCODER_PIN_CLK MASTER_ENC1_CLK
#define ENCODER_PIN_DT  MASTER_ENC1_DT
#define TICKS_PARA_MUDANCA 8
ESP32Encoder encoder;
static int64_t last_encoder_pos = 0;
static int encoder_ticks_acumulados = 0;

// Estado do menu
static uint8_t s_menu_selection = 0;
static const uint8_t s_menu_count = 5; // ONDAS, GATE, DRUM, SLICE, SYNTH

// Estados do encoder (apenas botão, rotação via ESP32Encoder)
static bool s_encoder_bt_last = false;

// Valores dos parâmetros
static int32_t s_param_values[PARAM_COUNT];

void au_encoder_control_begin(void) {
    if (s_initialized) return;
    
    // Configurar pinos do encoder
    pinMode(ENCODER_PIN_CLK, INPUT_PULLUP);
    pinMode(ENCODER_PIN_DT, INPUT_PULLUP);
    pinMode(MASTER_ENC1_BT, INPUT_PULLUP);
    
    // Configurar ESP32Encoder
    encoder.attachHalfQuad(ENCODER_PIN_CLK, ENCODER_PIN_DT);
    encoder.setCount(0);
    last_encoder_pos = encoder.getCount();
    
    // Inicializar valores dos parâmetros
    s_param_values[PARAM_ONDAS_FREQ_OSC1] = 440;
    s_param_values[PARAM_ONDAS_FREQ_OSC2] = 880;
    s_param_values[PARAM_ONDAS_FREQ_OSC3] = 1320;
    s_param_values[PARAM_ONDAS_WAVE_OSC1] = 0; // Quadrada
    s_param_values[PARAM_ONDAS_WAVE_OSC2] = 0; // Quadrada
    s_param_values[PARAM_ONDAS_WAVE_OSC3] = 0; // Quadrada
    s_param_values[PARAM_DRUM_BANK] = 0;
    s_param_values[PARAM_SLICE_BANK] = 0;
    s_param_values[PARAM_SYNTH_WAVE_TYPE] = 0; // Quadrada
    
    // Garantir que começa em ONDAS (0) no menu
    s_menu_selection = 0;
    s_state = ENCODER_STATE_MENU_NAVIGATION;
    
    s_initialized = true;
}

void au_encoder_control_tick(void) {
    if (!s_initialized) return;
    
    // Ler estado do botão do encoder
    bool encoder_bt = digitalRead(MASTER_ENC1_BT);
    
    // Processar rotação do encoder com debounce
    int64_t current_pos = encoder.getCount();
    int64_t delta = current_pos - last_encoder_pos;
    
    if (delta != 0) {
        int direction = delta > 0 ? -1 : 1;  // INVERTIDO
        encoder_ticks_acumulados += direction;
        
        if (abs(encoder_ticks_acumulados) >= TICKS_PARA_MUDANCA) {
            direction = encoder_ticks_acumulados > 0 ? 1 : -1;
            
            if (s_state == ENCODER_STATE_MENU_NAVIGATION) {
                // Navegar no menu
                if (direction > 0) {
                    s_menu_selection++;
                    if (s_menu_selection >= s_menu_count) s_menu_selection = 0;
                } else {
                    if (s_menu_selection == 0) {
                        s_menu_selection = s_menu_count - 1;
                    } else {
                        s_menu_selection--;
                    }
                }
                
            } else if (s_state == ENCODER_STATE_EDITING_VALUE) {
                // Alterar valor
                s_current_value += direction;
                
                // Limitar valor
                if (s_current_value < s_min_value) s_current_value = s_min_value;
                if (s_current_value > s_max_value) s_current_value = s_max_value;
                
                // Atualizar valor do parâmetro
                s_param_values[s_current_param] = s_current_value;
            } else if (s_state == ENCODER_STATE_MODE_ACTIVE) {
                // Modo ativo - rotação do encoder muda tipo de onda
                // Obter modo atual
                AuTelemetryData* data = au_telemetry_get_data();
                if (data && data->current_mode == AU_MODE_ONDAS) {
                    // Mudar tipo de onda
                    static int current_waveform = 0; // 0=SQUARE, 1=SINE, 2=TRIANGLE, 3=SAW, 4=NOTE
                    current_waveform += direction;
                    if (current_waveform < 0) current_waveform = 4;
                    if (current_waveform > 4) current_waveform = 0;
                    
                    // Aplicar mudança
                    gerador_ondas_set_waveform((GeradorWaveform)current_waveform);
                } else if (data && data->current_mode == AU_MODE_GATE) {
                    // Modo GATE - mudar tipo de onda para todos os 3 osciladores
                    static int current_waveform = 0; // 0=SQUARE, 1=SINE, 2=TRIANGLE, 3=SAW, 4=NOTE
                    current_waveform += direction;
                    if (current_waveform < 0) current_waveform = 4;
                    if (current_waveform > 4) current_waveform = 0;
                    
                    // Aplicar mudança para todos os osciladores
                    gerador_ondas_set_waveform((GeradorWaveform)current_waveform);
                } else if (data && data->current_mode == AU_MODE_DRUM) {
                    // Modo DRUM - mudar banco
                    static int current_bank = 0; // 0-7
                    current_bank += direction;
                    if (current_bank < 0) current_bank = 7;
                    if (current_bank > 7) current_bank = 0;
                    
                    // Aplicar mudança do banco
                    extern void dmachine_set_bank_index(int index);
                    dmachine_set_bank_index(current_bank);
                } else if (data && data->current_mode == AU_MODE_SLICE) {
                    // Modo SLICE - mudar ficheiro
                    static int current_file = 0; // 0-15
                    current_file += direction;
                    if (current_file < 0) current_file = 15;
                    if (current_file > 15) current_file = 0;
                    
                    // Aplicar mudança do ficheiro
                    extern void slice_set_file_index(int index);
                    slice_set_file_index(current_file);
                } else if (data && data->current_mode == AU_MODE_SYNTH) {
                    // Modo SYNTH - mudar tipo de onda
                    static int current_waveform = 1; // 0=SINE, 1=SAW, 2=SQUARE, 3=TRIANGLE, 4=NOISE
                    current_waveform += direction;
                    if (current_waveform < 0) current_waveform = 4;
                    if (current_waveform > 4) current_waveform = 0;
                    
                    // Aplicar mudança do tipo de onda
                    extern void synthesizer_set_waveform(SynthWaveform wave);
                    synthesizer_set_waveform((SynthWaveform)current_waveform);
                }
            }
            
            encoder_ticks_acumulados = 0;
        }
        last_encoder_pos = current_pos;
    }
    
    // Detectar click do botão
    if (encoder_bt == false && s_encoder_bt_last == true) {
        if (s_state == ENCODER_STATE_MENU_NAVIGATION) {
            // Selecionar modo do menu
            // Chamar função externa para mudar o modo
            extern void au_telemetry_set_mode(AuMode mode);
            AuMode selected_mode = (AuMode)(s_menu_selection + 1); // +1 porque AU_MODE_MENU = 0
            au_telemetry_set_mode(selected_mode);
            s_state = ENCODER_STATE_MODE_ACTIVE; // Entrar no modo ativo
        } else if (s_state == ENCODER_STATE_MODE_ACTIVE) {
            // Sair do modo ativo e voltar ao menu
            extern void au_telemetry_set_mode(AuMode mode);
            au_telemetry_set_mode(AU_MODE_MENU);
            s_state = ENCODER_STATE_MENU_NAVIGATION; // Voltar à navegação do menu
        } else if (s_state == ENCODER_STATE_IDLE) {
            // Iniciar seleção de parâmetro
            s_state = ENCODER_STATE_SELECTING_PARAM;
            s_current_param = PARAM_ONDAS_FREQ_OSC1;
        } else if (s_state == ENCODER_STATE_SELECTING_PARAM) {
            // Confirmar seleção e entrar em modo de edição
            s_state = ENCODER_STATE_EDITING_VALUE;
            s_current_value = s_param_values[s_current_param];
            
            // Definir limites baseados no parâmetro
            switch (s_current_param) {
                case PARAM_ONDAS_FREQ_OSC1:
                case PARAM_ONDAS_FREQ_OSC2:
                case PARAM_ONDAS_FREQ_OSC3:
                    s_min_value = 20;
                    s_max_value = 20000;
                    break;
                case PARAM_ONDAS_WAVE_OSC1:
                case PARAM_ONDAS_WAVE_OSC2:
                case PARAM_ONDAS_WAVE_OSC3:
                case PARAM_SYNTH_WAVE_TYPE:
                    s_min_value = 0;
                    s_max_value = 2;
                    break;
                case PARAM_DRUM_BANK:
                case PARAM_SLICE_BANK:
                    s_min_value = 0;
                    s_max_value = 15;
                    break;
                default:
                    s_min_value = 0;
                    s_max_value = 100;
                    break;
            }
        } else if (s_state == ENCODER_STATE_EDITING_VALUE) {
            // Confirmar valor e voltar ao modo de seleção
            s_state = ENCODER_STATE_SELECTING_PARAM;
        }
    }
    
    // Atualizar estado anterior do botão
    s_encoder_bt_last = encoder_bt;
}

EncoderParam au_encoder_control_get_current_param(void) {
    return s_current_param;
}

const char* au_encoder_control_get_param_name(EncoderParam param) {
    switch (param) {
        case PARAM_ONDAS_FREQ_OSC1: return "OSC1 FREQ";
        case PARAM_ONDAS_FREQ_OSC2: return "OSC2 FREQ";
        case PARAM_ONDAS_FREQ_OSC3: return "OSC3 FREQ";
        case PARAM_ONDAS_WAVE_OSC1: return "OSC1 WAVE";
        case PARAM_ONDAS_WAVE_OSC2: return "OSC2 WAVE";
        case PARAM_ONDAS_WAVE_OSC3: return "OSC3 WAVE";
        case PARAM_DRUM_BANK: return "DRUM BANK";
        case PARAM_SLICE_BANK: return "SLICE BANK";
        case PARAM_SYNTH_WAVE_TYPE: return "SYNTH WAVE";
        default: return "UNKNOWN";
    }
}

int32_t au_encoder_control_get_param_value(EncoderParam param) {
    if (param >= PARAM_COUNT) return 0;
    return s_param_values[param];
}

void au_encoder_control_set_param_value(EncoderParam param, int32_t value) {
    if (param >= PARAM_COUNT) return;
    s_param_values[param] = value;
}

uint8_t au_encoder_control_get_menu_selection(void) {
    return s_menu_selection;
}

void au_encoder_control_set_menu_selection(uint8_t selection) {
    if (selection < s_menu_count) {
        s_menu_selection = selection;
    }
}
