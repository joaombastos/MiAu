#include "editor_parametros.h"
#include "modo slave/midi_sequencer_slave.h"
#include "led_mux_geral.h"
#include "modo slave/led_mux_slave.h"
#include "controlador_botoes_geral.h"
#include "midi_din_geral.h"
#include "edit_system.h"
#include "modo slave/config_display_slave.h"
#include "prests_slave/prests_slave.h"
#include <Arduino.h>
#include <ESP32Encoder.h>


void ajustar_parametros_banco(StepParams* params, MenuStateSlave tipo, int direction);

#define ENCODER_PIN_CLK SLAVE_ENC2_CLK
#define ENCODER_PIN_DT  SLAVE_ENC2_DT
ESP32Encoder encoder;
static int64_t last_encoder_pos = 0;
static int encoder_ticks_acumulados = 0;
static const int TICKS_PARA_MUDANCA = 3;
static unsigned long botao_pressionado_tempo = 0;
static bool botao_pressionado = false;
static const unsigned long TEMPO_PARA_CLEAR = 1000;

MenuStateSlave menu_state = MENU_NORMAL;
uint8_t banco_atual = 1;
uint8_t editing_step = 1;
BancoConfig bancos[16]; 
StepParams step_params[8];

uint8_t quant_por_banco[16] = {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}; 
uint8_t canal_por_banco[16] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}; 

uint8_t ticks_por_banco[16] = {0}; 
uint8_t passo_atual_por_banco[16] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}; 
bool bancos_ativos_este_tick[16] = {false}; 

bool pagina_atual = false; 
bool editando_pagina_2 = false; 

// ==========================
// Relógio independente para RAM slots
// ==========================
// NUM_RAM_SLOTS já está em prests_slave.h
static uint8_t ticks_por_banco_slots[NUM_RAM_SLOTS][16] = {0};
static uint8_t passo_atual_por_banco_slots[NUM_RAM_SLOTS][16] = {0};
// Garantia de disparo do passo 1 (Live) sem duplicar
static bool live_step1_pending_fire[16] = {false};

void sequencer_reset_slot_counters(uint8_t slot_idx) {
    if (slot_idx >= NUM_RAM_SLOTS) return;
    for (int b = 0; b < NUM_BANCOS_SLAVE; b++) {
        ticks_por_banco_slots[slot_idx][b] = 0;
        passo_atual_por_banco_slots[slot_idx][b] = 0; // 0 força próximo avanço setar para 1
    }
}

void inicializar_banco(BancoConfig* banco, uint8_t banco_idx) {

    uint8_t canal_correto = canal_por_banco[banco_idx - 1];
    
    for (int passo = 0; passo < 8; passo++) {
        uint8_t nota_base = NOTA_BASE_SLAVE + (banco_idx - NOTA_BASE_OFFSET_SLAVE); 
        
        banco->step_params[passo].nota = nota_base;
        banco->step_params[passo].channel = canal_correto;
        banco->step_params[passo].velocity = VELOCITY_PADRAO_SLAVE;
        banco->step_params[passo].length = LENGTH_PADRAO_SLAVE;
        banco->step_params[passo].num_notas_extra = 0;
        banco->passos_ativos[passo] = false;
        
        banco->step_params_pagina_2[passo].nota = nota_base;
        banco->step_params_pagina_2[passo].channel = canal_correto;
        banco->step_params_pagina_2[passo].velocity = VELOCITY_PADRAO_SLAVE;
        banco->step_params_pagina_2[passo].length = LENGTH_PADRAO_SLAVE;
        banco->step_params_pagina_2[passo].num_notas_extra = 0;
        banco->passos_ativos_pagina_2[passo] = false;
    }
    banco->resolution = RESOLUCAO_PADRAO_SLAVE; 
    banco->duas_paginas = false; 
    
}

void carregar_banco_para_sequencia(BancoConfig* banco, Step* sequencia, StepParams* params) {
    for (int i = 0; i < 8; i++) {
        params[i] = banco->step_params[i];
        sequencia_slave[i].ativo = banco->passos_ativos[i];
        sequencia_slave[i].nota = params[i].nota;
    }
}

void salvar_banco_da_sequencia(BancoConfig* banco, Step* sequencia) {
    for (int i = 0; i < 8; i++) {
        banco->passos_ativos[i] = sequencia_slave[i].ativo;
    }
}

void editor_carregar_banco(uint8_t banco) {
    if (banco < 1 || banco > 16) return; 
    
    banco_atual = banco;
    carregar_banco_atual();
    

    extern void sequencer_update_resolution(void);
    sequencer_update_resolution();
    
    passo_atual = passo_atual_por_banco[banco_atual - 1];
    
    editando_pagina_2 = false; 
    
    extern void leds_show_passo_atual(void);
    leds_show_passo_atual_slave();
    
    extern void oled_clear(void);
    extern void oled_update(void);
    oled_clear();
    oled_update();
}

void editor_salvar_banco_atual(void) {
    if (banco_atual < 1 || banco_atual > 16) return; 
    
    extern Step sequencia_slave[8];
    for (int i = 0; i < 8; i++) {
        bancos[banco_atual - 1].step_params[i] = step_params[i];
        bancos[banco_atual - 1].passos_ativos[i] = sequencia_slave[i].ativo;
    }
}

void editor_toggle_passo(uint8_t passo) {
    if (passo < 1 || passo > 8) return;
    
    if (editando_pagina_2) {
        bancos[banco_atual - 1].passos_ativos_pagina_2[passo - 1] = !bancos[banco_atual - 1].passos_ativos_pagina_2[passo - 1];
    } else {
        bancos[banco_atual - 1].passos_ativos[passo - 1] = !bancos[banco_atual - 1].passos_ativos[passo - 1];
    }
    
    extern void leds_show_passo_atual(void);
    leds_show_passo_atual_slave();
}

void editor_clear_banco_atual(void) {
    inicializar_banco(&bancos[banco_atual - 1], banco_atual);
    editor_carregar_banco(banco_atual);
}


void processar_botao_extra_1(void) {

    editando_pagina_2 = !editando_pagina_2;
    
    if (editando_pagina_2) {
        bancos[banco_atual - 1].duas_paginas = true;
    } else {
    }
    
    extern void leds_show_passo_atual_slave(void);
    leds_show_passo_atual_slave();
}

void editor_entrar_modo_edicao(void) {
    menu_state = MENU_SELECT_STEP;
    editing_step = banco_atual;
}

void ajustar_banco_atual(int direction) {
    int new_banco = banco_atual + direction;
    if (new_banco >= 1 && new_banco <= 16) { 
        banco_atual = new_banco;
        carregar_banco_atual();
        
        editando_pagina_2 = false;
        
        extern void leds_show_passo_atual_slave(void);
        leds_show_passo_atual_slave();
    }
}

void ajustar_resolucao(int direction) {
    int new_value = bancos[banco_atual - 1].resolution + direction;
    if (new_value >= 0 && new_value <= 6) {
        extern bool mudanca_resolucao_pendente;
        extern uint8_t nova_resolucao_pendente;
        extern uint8_t banco_alterado_pendente;
        mudanca_resolucao_pendente = true;
        nova_resolucao_pendente = new_value;
        banco_alterado_pendente = banco_atual;
    }
}

void carregar_banco_atual(void) {
    extern Step sequencia_slave[8];
    for (int i = 0; i < 8; i++) {
        step_params[i] = bancos[banco_atual - 1].step_params[i];
        sequencia_slave[i].ativo = bancos[banco_atual - 1].passos_ativos[i];
        sequencia_slave[i].nota = step_params[i].nota;
    }
}

void ajustar_parametros_normal(MenuStateSlave tipo, int direction) {
    // Declarações de variáveis para o caso MENU_MASTER_BPM
    uint16_t current_bpm, new_bpm;
    
    switch (tipo) {
        case MENU_SELECT_STEP:
            ajustar_banco_atual(direction);
            break;
        case MENU_EDIT_NOTE:
        case MENU_EDIT_VELOCITY:
        case MENU_EDIT_CHANNEL:
        case MENU_EDIT_LENGTH:
            ajustar_parametros_banco(step_params, tipo, direction);
            for (int i = 0; i < 8; i++) {
                bancos[banco_atual - 1].step_params[i] = step_params[i];
            }
            // Garantir que o canal (Ch) seja igual na página 2 e nos passos 1-8 (normal)
            if (tipo == MENU_EDIT_CHANNEL) {
                // Replicar o canal atualizado para a página 2 (passos 9-16)
                for (int i = 0; i < 8; i++) {
                    bancos[banco_atual - 1].step_params_pagina_2[i].channel = step_params[i].channel;
                }
                // Sincronizar o canal do banco para inicializações futuras
                canal_por_banco[banco_atual - 1] = step_params[0].channel;
            }
            break;
        case MENU_EDIT_RESOLUTION:
            ajustar_resolucao(direction);
            break;
        case MENU_MASTER_BPM:
            // Ajustar BPM no modo master
            extern uint16_t master_internal_get_bpm(void);
            extern void master_internal_set_bpm(uint16_t bpm);
            current_bpm = master_internal_get_bpm();
            new_bpm = current_bpm + (direction * 1);
            if (new_bpm >= 40 && new_bpm <= 300) {
                master_internal_set_bpm(new_bpm);
            }
            break;
        default:
            break;
    }
}

void navegar_menu_normal(void) {
    switch (menu_state) {
        case MENU_NORMAL:
            editor_entrar_modo_edicao();
            break;
        case MENU_SELECT_STEP:
            // Click no banco seleciona e vai para edição de nota
            menu_state = MENU_EDIT_NOTE;
            break;
        case MENU_EDIT_NOTE:
            // Click na nota confirma e vai para velocity
            menu_state = MENU_EDIT_VELOCITY;
            break;
        case MENU_EDIT_VELOCITY:
            // Click na velocity confirma e vai para channel
            menu_state = MENU_EDIT_CHANNEL;
            break;
        case MENU_EDIT_CHANNEL:
            // Click no channel confirma e vai para length
            menu_state = MENU_EDIT_LENGTH;
            break;
        case MENU_EDIT_LENGTH:
            // Click no length confirma e vai para resolution
            menu_state = MENU_EDIT_RESOLUTION;
            break;
        case MENU_EDIT_RESOLUTION:
            // Click na resolution confirma e vai para BPM (se modo master) ou volta para seleção de banco
            extern bool master_internal_is_active(void);
            if (master_internal_is_active()) {
                menu_state = MENU_MASTER_BPM;
            } else {
                menu_state = MENU_SELECT_STEP;
            }
            break;
        case MENU_MASTER_BPM:
            // Click no BPM confirma e volta para seleção de banco
            menu_state = MENU_SELECT_STEP;
            break;
    }
    
    // Atualizar interface após mudança de estado
    extern void atualizar_interface(void);
    atualizar_interface();
}


void inicializar_bancos_slave(void) {

    for (int i = 0; i < 16; i++) { 
        inicializar_banco(&bancos[i], i + 1);
    }
    
    for (int i = 0; i < 16; i++) { 
        for (int passo = 0; passo < 8; passo++) {
            bancos[i].step_params[passo].channel = canal_por_banco[i];
        }
    }
}


void editor_inicializar(void) {
    pinMode(ENCODER_PIN_CLK, INPUT_PULLUP);
    pinMode(ENCODER_PIN_DT, INPUT_PULLUP);
    encoder.attachHalfQuad(ENCODER_PIN_CLK, ENCODER_PIN_DT);
    encoder.setCount(0);
    last_encoder_pos = encoder.getCount();
    
    inicializar_bancos_slave();
    
    menu_state = MENU_NORMAL;
    banco_atual = 1;
    editing_step = 1;

    
    editor_carregar_banco(banco_atual);
    
    carregar_banco_atual();
}

void ajustar_valor_parametro(uint8_t* valor, int direction, uint8_t min_val, uint8_t max_val, int increment = 1) {
    int new_value = *valor + (direction * increment);
    if (new_value < min_val) new_value = min_val;
    if (new_value > max_val) new_value = max_val;
    *valor = new_value;
}

void editor_ajustar_valor_parametro(int direction) {
    ajustar_parametros_normal(menu_state, direction);
}

void editor_processar_encoder(void) {
    // Verificar se o modo slots SysEx está ativo
    extern bool slots_sysex_is_active(void);
    if (slots_sysex_is_active()) {
        // No modo slots SysEx, processar encoder
        int64_t current_pos = encoder.getCount();
        int64_t delta = current_pos - last_encoder_pos;
        if (delta != 0) {
            int direction = (delta > 0) ? -1 : 1;
            encoder_ticks_acumulados += direction;
            if (abs(encoder_ticks_acumulados) >= TICKS_PARA_MUDANCA) {
                direction = encoder_ticks_acumulados > 0 ? 1 : -1;
                // Processar encoder no modo slots
                extern void slots_sysex_process_encoder(int direction);
                slots_sysex_process_encoder(direction);
                encoder_ticks_acumulados = 0;
            }
            last_encoder_pos = current_pos;
        }
        
        // Processar click do encoder no modo slots
        extern bool botoes_ler_encoder(void);
        static bool encoder_button_last = true;
        bool encoder_button = botoes_ler_encoder();
        
        if (encoder_button == false && encoder_button_last == true) {
            botao_pressionado = true;
            botao_pressionado_tempo = millis();
        } else if (encoder_button == true && encoder_button_last == false) {
            botao_pressionado = false;
            if (millis() - botao_pressionado_tempo < TEMPO_PARA_CLEAR) {
                // Click curto no modo slots: confirmar
                extern void slots_sysex_process_encoder_click(void);
                slots_sysex_process_encoder_click();
            }
        }
        
        encoder_button_last = encoder_button;
        return;
    }
    
    // Verificar se o modo slots SysEx input está ativo
    if (false) {
        // No modo slots SysEx input, processar encoder
        int64_t current_pos = encoder.getCount();
        int64_t delta = current_pos - last_encoder_pos;
        if (delta != 0) {
            int direction = (delta > 0) ? -1 : 1;
            encoder_ticks_acumulados += direction;
            if (abs(encoder_ticks_acumulados) >= TICKS_PARA_MUDANCA) {
                direction = encoder_ticks_acumulados > 0 ? 1 : -1;
                // Processar encoder no modo slots input
                encoder_ticks_acumulados = 0;
            }
            last_encoder_pos = current_pos;
        }
        
        // Processar click do encoder no modo slots input
        extern bool botoes_ler_encoder(void);
        static bool encoder_button_last = true;
        bool encoder_button = botoes_ler_encoder();
        
        if (encoder_button == false && encoder_button_last == true) {
            botao_pressionado = true;
            botao_pressionado_tempo = millis();
        } else if (encoder_button == true && encoder_button_last == false) {
            botao_pressionado = false;
            if (millis() - botao_pressionado_tempo < TEMPO_PARA_CLEAR) {
                // Click curto no modo slots input: confirmar
            }
        }
        
        encoder_button_last = encoder_button;
        return;
    }
    
    // Verificar se o input_mode está ativo
    extern bool input_mode_is_active(void);
    if (input_mode_is_active()) {
        // No modo input, o encoder ajusta parâmetros do input_mode
        int64_t current_pos = encoder.getCount();
        int64_t delta = current_pos - last_encoder_pos;
        if (delta != 0) {
            int direction = (delta > 0) ? -1 : 1;
            encoder_ticks_acumulados += direction;
            if (abs(encoder_ticks_acumulados) >= TICKS_PARA_MUDANCA) {
                direction = encoder_ticks_acumulados > 0 ? 1 : -1;
                // Ajustar parâmetros do input_mode
                extern void input_mode_adjust_param(int direction);
                input_mode_adjust_param(direction);
                encoder_ticks_acumulados = 0;
            }
            last_encoder_pos = current_pos;
        }
        
        // Processar botão do encoder no modo input
        extern bool botoes_ler_encoder(void);
        static bool encoder_button_last = true;
        bool encoder_button = botoes_ler_encoder();
        
        if (encoder_button == false && encoder_button_last == true) {
            botao_pressionado = true;
            botao_pressionado_tempo = millis();
        } else if (encoder_button == true && encoder_button_last == false) {
            botao_pressionado = false;
            if (millis() - botao_pressionado_tempo < TEMPO_PARA_CLEAR) {
                // Click curto no modo input: próximo parâmetro
                extern void input_mode_next_param(void);
                input_mode_next_param();
            }
        }
        
        if (botao_pressionado && (millis() - botao_pressionado_tempo >= TEMPO_PARA_CLEAR)) {
            // Long press no modo input: reset parâmetros
            extern void input_mode_reset_params(void);
            input_mode_reset_params();
            botao_pressionado = false;
        }
        
        encoder_button_last = encoder_button;
        return; // Sair da função para não processar o editor normal
    }
    
    // Código original para o editor normal
    int64_t current_pos = encoder.getCount();
    int64_t delta = current_pos - last_encoder_pos;
    if (delta != 0) {
        int direction = (delta > 0) ? -1 : 1;
        encoder_ticks_acumulados += direction;
        if (abs(encoder_ticks_acumulados) >= TICKS_PARA_MUDANCA) {
            direction = encoder_ticks_acumulados > 0 ? 1 : -1;
            editor_ajustar_valor_parametro(direction);
            encoder_ticks_acumulados = 0;
        }
        last_encoder_pos = current_pos;
    }
    
    extern bool botoes_ler_encoder(void);
    static bool encoder_button_last = true;
    bool encoder_button = botoes_ler_encoder();
    
    if (encoder_button == false && encoder_button_last == true) {
        botao_pressionado = true;
        botao_pressionado_tempo = millis();
    } else if (encoder_button == true && encoder_button_last == false) {
        botao_pressionado = false;
        if (millis() - botao_pressionado_tempo < TEMPO_PARA_CLEAR) {
            navegar_menu();
        }
    }
    
    if (botao_pressionado && (millis() - botao_pressionado_tempo >= TEMPO_PARA_CLEAR)) {
        editor_clear_banco_atual();
        botao_pressionado = false;
    }
    
    encoder_button_last = encoder_button;
}

void navegar_menu(void) {
    navegar_menu_normal();
}



void ajustar_parametros_banco(StepParams* params, MenuStateSlave tipo, int direction) {
    switch (tipo) {
        case MENU_EDIT_NOTE:
            for (int i = 0; i < 8; i++) {
                ajustar_valor_parametro(&params[i].nota, direction, 0, 127);
            }
            break;
        case MENU_EDIT_VELOCITY:
            for (int i = 0; i < 8; i++) {
                ajustar_valor_parametro(&params[i].velocity, direction, 0, 127, 5);
            }
            break;
        case MENU_EDIT_CHANNEL:
            for (int i = 0; i < 8; i++) {
                ajustar_valor_parametro(&params[i].channel, direction, 1, 16);
            }
            break;
        case MENU_EDIT_LENGTH:
            for (int i = 0; i < 8; i++) {
                ajustar_valor_parametro(&params[i].length, direction, 0, 100, 5);
            }
            break;
        default:
            break;
    }
}


void sequencer_init_slave(void) {
    sequencer_reset_counters_slave();
}

void sequencer_reset_counters_slave(void) {
}

void sequencer_verificar_sincronizacao(void) {
}

void sequencer_update_resolution(void) {
}

void aplicar_mudanca_resolucao(void) {
    extern bool mudanca_resolucao_pendente;
    extern uint8_t nova_resolucao_pendente;
    extern uint8_t banco_alterado_pendente;
    
    if (mudanca_resolucao_pendente) {
        if (banco_alterado_pendente >= 1 && banco_alterado_pendente <= 16) {
            bancos[banco_alterado_pendente - 1].resolution = nova_resolucao_pendente;
            
            ticks_por_banco[banco_alterado_pendente - 1] = 0;
            
            passo_atual_por_banco[banco_alterado_pendente - 1] = 1;
            
            if (banco_alterado_pendente == banco_atual) {
                passo_atual = 1; 
                
                extern void leds_show_passo_atual_slave(void);
                leds_show_passo_atual_slave();
            }
        }
        mudanca_resolucao_pendente = false;
    }
}

void processar_banco_normal(int banco) {
    if (banco < 0 || banco >= 16) return;
    
    uint8_t passo_do_banco = passo_atual_por_banco[banco];
    
    bool ram_on = presets_slave_is_playing_from_ram();
    
    // LIVE: tocar do banco atual
    // Edge para passo 1: se marcado como pending, dispara aqui e evita duplicação
    if (passo_do_banco == 1 && live_step1_pending_fire[banco]) {
        live_step1_pending_fire[banco] = false;
        if (bancos[banco].duas_paginas) {
            if (bancos[banco].passos_ativos[0]) {
                const StepParams* params = &bancos[banco].step_params[0];
                sequencer_send_note(params->nota, params->velocity, params->channel);
                sequencer_add_nota_ativa(params->nota, params->channel, params->velocity, params->length);
            }
        } else {
            if (bancos[banco].passos_ativos[0]) {
                const StepParams* params = &bancos[banco].step_params[0];
                sequencer_send_note(params->nota, params->velocity, params->channel);
                sequencer_add_nota_ativa(params->nota, params->channel, params->velocity, params->length);
            }
        }
        return; // evitar tentar disparar de novo no mesmo passo
    }
    if (bancos[banco].duas_paginas) {
        if (passo_do_banco >= 1 && passo_do_banco <= 8) {
            if (bancos[banco].passos_ativos[passo_do_banco - 1]) {
                const StepParams* params = &bancos[banco].step_params[passo_do_banco - 1];
                sequencer_send_note(params->nota, params->velocity, params->channel);
                sequencer_add_nota_ativa(params->nota, params->channel, params->velocity, params->length);
            }
        } else if (passo_do_banco >= 9 && passo_do_banco <= 16) {
            uint8_t passo_pagina_2 = passo_do_banco - 8;
            if (bancos[banco].passos_ativos_pagina_2[passo_pagina_2 - 1]) {
                const StepParams* params = &bancos[banco].step_params_pagina_2[passo_pagina_2 - 1];
                sequencer_send_note(params->nota, params->velocity, params->channel);
                sequencer_add_nota_ativa(params->nota, params->channel, params->velocity, params->length);
            }
        }
    } else {
        if (passo_do_banco >= 1 && passo_do_banco <= 8) {
            if (bancos[banco].passos_ativos[passo_do_banco - 1]) {
                const StepParams* params = &bancos[banco].step_params[passo_do_banco - 1];
                sequencer_send_note(params->nota, params->velocity, params->channel);
                sequencer_add_nota_ativa(params->nota, params->channel, params->velocity, params->length);
            }
        }
    }
    
    // RAM: não disparar aqui; disparo exclusivo no relógio dos slots
}

void sequencer_processar_notas_ativas(void) {
    // ✅ OTIMIZAÇÃO: Processar notas ativas a cada tick (não a cada 4)
    for (int i = 0; i < 8; i++) {
        if (notas_ativas[i].ticks_restantes > 0) {
            notas_ativas[i].ticks_restantes--;
            if (notas_ativas[i].ticks_restantes == 0) {
                sequencer_send_note_off(notas_ativas[i].nota, notas_ativas[i].channel);
            }
        }
    }
}

void sequencer_add_nota_ativa(uint8_t nota, uint8_t channel, uint8_t velocity, uint8_t length_percent) {
    // Encontrar slot livre
    for (int i = 0; i < 8; i++) {
        if (notas_ativas[i].ticks_restantes == 0) {
            notas_ativas[i].nota = nota;
            notas_ativas[i].channel = channel;
            // Calcular duração baseada na resolução do banco que tocou a nota
            // Usar resolução padrão 1/4 (24 ticks) para duração consistente
            notas_ativas[i].ticks_restantes = (length_percent * 24) / 100; // Baseado em 24 PPQN fixo
            break;
        }
    }
}

void sequencer_clear_all_notes_slave(void) {
    for (int i = 0; i < 8; i++) {
        if (notas_ativas[i].ticks_restantes > 0) {
            sequencer_send_note_off(notas_ativas[i].nota, notas_ativas[i].channel);
            notas_ativas[i].ticks_restantes = 0;
        }
    }
}

uint8_t passo_atual = 1;
static uint8_t passo_anterior = 1;
uint8_t tick_in_step = 0;
uint32_t midi_tick_counter = 0;

bool mudanca_resolucao_pendente = false;
uint8_t nova_resolucao_pendente = 0;
uint8_t banco_alterado_pendente = 0;

NotaAtiva notas_ativas[8];

uint8_t calcular_ticks_por_passo(uint8_t resolution) {
    switch (resolution) {
        case 0: return 48;  // 1/2 (24 * 2 = 48 ticks)
        case 1: return 16;  // 1/8T (24 * 2/3 = 16 ticks) - colcheias jazz 12/8
        case 2: return 24;  // 1/4 (24 * 1 = 24 ticks)
        case 3: return 12;  // 1/8 (24 * 1/2 = 12 ticks)
        case 4: return 6;   // 1/16 (24 * 1/4 = 6 ticks)
        case 5: return 8;   // 1/16T (24 * 1/3 = 8 ticks)
        case 6: return 3;   // 1/32 (24 * 1/8 = 3 ticks)
        default: return 24; // 1/4 por padrão
    }
}

void sequencer_reset_all_counters(void) {
    tick_in_step = 0;
    
    for (int i = 0; i < NUM_BANCOS_SLAVE; i++) {
        ticks_por_banco[i] = 0;
        passo_atual_por_banco[i] = 1;
        bancos_ativos_este_tick[i] = false;
    }
    // Reset relógios RAM slots
    for (int s = 0; s < NUM_RAM_SLOTS; s++) {
        for (int b = 0; b < NUM_BANCOS_SLAVE; b++) {
            ticks_por_banco_slots[s][b] = 0;
            passo_atual_por_banco_slots[s][b] = 1;
        }
    }
    
    sequencer_reset_counters_slave();
}

void sequencer_init(void) {
    sequencer_init_slave();
    
    passo_atual = 1;
    passo_anterior = 1;
    tick_in_step = 0;
    midi_tick_counter = 0; 
    sequencer_reset_all_counters();
    extern void leds_show_passo_atual(void);
    leds_show_passo_atual_slave();
}

void sequencer_mudanca_modo(void) {
    extern void leds_limpar_piscar_modo(void);
    leds_limpar_piscar_modo();
    
    // Verificar se o overlay AU está ativo - se estiver, não atualizar o OLED
    extern bool au_overlay_is_active(void);
    if (!au_overlay_is_active()) {
        extern void oled_show_step(uint8_t step);
        oled_show_step(passo_atual);
    }
    
    extern void leds_show_passo_atual(void);
    leds_show_passo_atual_slave();
}

void sequencer_reset_position(void) {
    passo_atual = 1;
    passo_anterior = 1;
    tick_in_step = 0;
    
    for (int i = 0; i < 8; i++) {
        passo_atual_por_banco[i] = 1;
        ticks_por_banco[i] = 0;
    }
    
    extern void leds_show_passo_atual(void);
    leds_show_passo_atual_slave();
}

void sequencer_send_note(uint8_t nota, uint8_t velocity, uint8_t channel) {
    midi::sendNoteOn(nota, velocity, channel);
}

void sequencer_send_note_off(uint8_t nota, uint8_t channel) {
    midi::sendNoteOff(nota, 0, channel);
}

void sequencer_tick_otimizado(void) {
    tick_in_step++;
    midi_tick_counter++;
    
    if (tick_in_step >= 24) {
        tick_in_step = 0;
    }

    // Downbeat rápido e robusto: ancora no banco 1 e aceita janela de 2 ticks
    bool is_downbeat_bank1 = (passo_atual_por_banco[0] == 1) && (tick_in_step == 1 || tick_in_step == 2 || tick_in_step == 3);

    if (is_downbeat_bank1) {
        // Ativar slots em sync no próximo downbeat 1
        if (presets_slots_has_syncing()) {
            for (int slot = 0; slot < NUM_RAM_SLOTS; slot++) {
                if (!presets_slot_syncing[slot]) continue;
                presets_slot_syncing[slot] = false;
                presets_slot_sync_stage[slot] = 0;
                presets_slot_store(slot);
                presets_slot_active[slot] = true;
                for (int b = 0; b < NUM_BANCOS_SLAVE; b++) {
                    ticks_por_banco_slots[slot][b] = 0;
                    passo_atual_por_banco_slots[slot][b] = 1;
                }
                // Disparo imediato da RAM no passo 1 (sem duplicar depois)
                const BancoConfig* rb = presets_ram_slots[slot];
                // Evitar sobreposição: limpar notas MIDI ativas do LIVE antes do disparo imediato
                extern void sequencer_clear_all_midi_notes(void);
                sequencer_clear_all_midi_notes();
                for (int b = 0; b < NUM_BANCOS_SLAVE; b++) {
                    if (rb[b].duas_paginas) {
                        if (rb[b].passos_ativos[0]) {
                            const StepParams* params = &rb[b].step_params[0];
                            sequencer_send_note(params->nota, params->velocity, params->channel);
                            sequencer_add_nota_ativa(params->nota, params->channel, params->velocity, params->length);
                        }
                    } else {
                        if (rb[b].passos_ativos[0]) {
                            const StepParams* params = &rb[b].step_params[0];
                            sequencer_send_note(params->nota, params->velocity, params->channel);
                            sequencer_add_nota_ativa(params->nota, params->channel, params->velocity, params->length);
                        }
                    }
                }
                presets_slot_skip_tick[slot] = true; // pular o próximo tick do relógio dos slots
                presets_slave_set_play_from_ram(true);
                presets_set_mute_live(true);
                for (int b = 0; b < NUM_BANCOS_SLAVE; b++) live_step1_pending_fire[b] = false;
            }
        }

        // Aplicar toggles armados no próximo downbeat 1
        for (int slot = 0; slot < NUM_RAM_SLOTS; slot++) {
            if (!presets_slot_toggle_syncing[slot]) continue;
            presets_slot_toggle_syncing[slot] = false;
            presets_slot_toggle_stage[slot] = 0;
            bool becoming_active = presets_slot_toggle_target[slot];
            presets_slot_active[slot] = becoming_active;
            if (becoming_active) {
                // Alinhar e disparar imediato passo 1
                for (int b = 0; b < NUM_BANCOS_SLAVE; b++) {
                    ticks_por_banco_slots[slot][b] = 0;
                    passo_atual_por_banco_slots[slot][b] = 1;
                }
                const BancoConfig* rb = presets_ram_slots[slot];
                // Evitar sobreposição: limpar notas MIDI ativas do LIVE antes do disparo imediato
                extern void sequencer_clear_all_midi_notes(void);
                sequencer_clear_all_midi_notes();
                for (int b = 0; b < NUM_BANCOS_SLAVE; b++) {
                    if (rb[b].duas_paginas) {
                        if (rb[b].passos_ativos[0]) {
                            const StepParams* params = &rb[b].step_params[0];
                            sequencer_send_note(params->nota, params->velocity, params->channel);
                            sequencer_add_nota_ativa(params->nota, params->channel, params->velocity, params->length);
                        }
                    } else {
                        if (rb[b].passos_ativos[0]) {
                            const StepParams* params = &rb[b].step_params[0];
                            sequencer_send_note(params->nota, params->velocity, params->channel);
                            sequencer_add_nota_ativa(params->nota, params->channel, params->velocity, params->length);
                        }
                    }
                }
                presets_slot_skip_tick[slot] = true;
                // Evitar nota dupla no downbeat: mutar LIVE imediatamente no toggle ON
                presets_set_mute_live(true);
                // Evitar duplicação de disparo do LIVE no passo 1
                for (int b = 0; b < NUM_BANCOS_SLAVE; b++) {
                    live_step1_pending_fire[b] = false;
                }
            }
            presets_slave_set_play_from_ram(presets_slots_any_active());
        }

        // Limpar Live no próximo downbeat 1 (saída Presets)
        extern bool presets_exit_syncing; extern uint8_t presets_exit_stage;
        if (presets_exit_syncing) {
            extern void inicializar_bancos_slave(void);
            extern uint8_t banco_atual;
            extern void editor_carregar_banco(uint8_t banco);
            extern void sequencer_reset_counters_slave(void);
            extern void leds_show_passo_atual_slave(void);
            extern void presets_request_unmute_live_next_tick(void);
            inicializar_bancos_slave();
            banco_atual = 1;
            editor_carregar_banco(banco_atual);
            sequencer_reset_counters_slave();
            leds_show_passo_atual_slave();
            presets_request_unmute_live_next_tick();
            extern void presets_exit_clear_sync(void);
            presets_exit_clear_sync();
        }
    }

    // Sincronização no início do tick (downbeat global = tick 1 e TODOS os bancos Live no passo 1)
    if (tick_in_step == 1) {
        // Verificar passo 1 em todos os bancos do Live
        bool live_all_step1 = true;
        for (int b = 0; b < NUM_BANCOS_SLAVE; b++) {
            if (passo_atual_por_banco[b] != 1) { live_all_step1 = false; break; }
        }
        if (live_all_step1 && presets_slots_has_syncing()) {
            // Ativar imediatamente no próximo downbeat 1
            for (int slot = 0; slot < NUM_RAM_SLOTS; slot++) {
                if (!presets_slot_syncing[slot]) continue;
                presets_slot_sync_stage[slot] = 0;
                presets_slot_syncing[slot] = false;
                presets_slot_store(slot);
                presets_slot_active[slot] = true;
                for (int b = 0; b < NUM_BANCOS_SLAVE; b++) {
                    ticks_por_banco_slots[slot][b] = 0;
                    passo_atual_por_banco_slots[slot][b] = 1;
                }
                presets_slave_set_play_from_ram(true);
                presets_set_mute_live(true);
                // Evitar sobreposição e duplo disparo no downbeat
                extern void sequencer_clear_all_midi_notes(void);
                sequencer_clear_all_midi_notes();
                presets_slot_skip_tick[slot] = true;
                for (int b = 0; b < NUM_BANCOS_SLAVE; b++) live_step1_pending_fire[b] = false;
            }
        }

        // Sync de toggle (ativar/desativar) por downbeat duplo
        if (live_all_step1) {
            for (int slot = 0; slot < NUM_RAM_SLOTS; slot++) {
                if (!presets_slot_toggle_syncing[slot]) continue;
                // Aplicar toggle já neste downbeat 1
                presets_slot_toggle_stage[slot] = 0;
                presets_slot_toggle_syncing[slot] = false;
                presets_slot_active[slot] = presets_slot_toggle_target[slot];
                presets_slave_set_play_from_ram(presets_slots_any_active());
                if (presets_slot_active[slot]) {
                    // Evitar nota dupla quando RAM dispara passo 1 e LIVE também estaria em 1
                    presets_set_mute_live(true);
                    presets_slot_skip_tick[slot] = true;
                    for (int b = 0; b < NUM_BANCOS_SLAVE; b++) {
                        live_step1_pending_fire[b] = false;
                    }
                }
            }
        }

        // Sincronização de saída do modo presets (limpar Live no próximo downbeat 1)
        extern bool presets_exit_syncing; extern uint8_t presets_exit_stage;
        if (live_all_step1 && presets_exit_syncing) {
            // Aplicar limpeza imediatamente neste downbeat 1
            extern void inicializar_bancos_slave(void);
            extern uint8_t banco_atual;
            extern void editor_carregar_banco(uint8_t banco);
            extern void sequencer_reset_counters_slave(void);
            extern void leds_show_passo_atual_slave(void);
            extern void presets_set_mute_live(bool);
            extern void presets_request_unmute_live_next_tick(void);
            inicializar_bancos_slave();
            banco_atual = 1;
            editor_carregar_banco(banco_atual);
            sequencer_reset_counters_slave();
            leds_show_passo_atual_slave();
            // Desmutar Live somente no próximo tick para evitar sobreposição no downbeat
            presets_request_unmute_live_next_tick();
            extern void presets_exit_clear_sync(void);
            presets_exit_clear_sync();
        }
    }
    
    bool aplicar_mudanca_agora = false;
    if (tick_in_step == 1) {
        for (int banco = 0; banco < NUM_BANCOS_SLAVE; banco++) {
            if (passo_atual_por_banco[banco] == 1) {
                aplicar_mudanca_agora = true;
                break;
            }
        }
    }
    
    if (aplicar_mudanca_agora) {
        aplicar_mudanca_resolucao();
    }
    // Processar pedido de desmute do Live no ciclo regular, longe do downbeat
    if (presets_unmute_live_pending()) {
        extern void presets_set_mute_live(bool);
        presets_set_mute_live(false);
        extern void presets_clear_unmute_live_pending(void);
        presets_clear_unmute_live_pending();
    }
    
    static uint8_t last_passo_displayed = 0;
    uint8_t current_passo = passo_atual_por_banco[banco_atual - 1];
    
    if (current_passo != last_passo_displayed) {
        passo_atual = current_passo;
        extern void leds_show_passo_atual_slave(void);
        leds_show_passo_atual_slave();
        atualizar_interface();
        last_passo_displayed = current_passo;
    }
    
    // Em ambos os casos, só toca LEDs quando o passo mudou (acima já tratamos)
    
    sequencer_sync_all_banks_simultaneously();
}

void sequencer_tick(void) {
    sequencer_tick_otimizado();
}

void sequencer_processar_nao_critico(void) {
}

void atualizar_interface(void) {
    static uint8_t last_passo_for_leds = 0;
    
    uint8_t passo_para_interface = passo_atual;
    
    extern void leds_show_passo_atual_slave(void);
    leds_show_passo_atual_slave();
    
    // Verificar se o overlay AU está ativo - se estiver, não atualizar o OLED
    extern bool au_overlay_is_active(void);
    if (au_overlay_is_active()) {
        return; // Não atualizar OLED quando overlay AU está ativo
    }
    
    // Verificar se estamos no modo slots SysEx
    extern bool slots_sysex_is_active(void);
    if (!slots_sysex_is_active()) {
        // Atualizar OLED imediatamente para mostrar mudanças no menu state
        extern void oled_show_step(uint8_t step);
        oled_show_step(passo_para_interface);
    }
    
    if (passo_para_interface != last_passo_for_leds) {
        static uint8_t oled_counter = 0;
        oled_counter++;
        
        if (oled_counter >= 32) {
            if (!slots_sysex_is_active()) {
                extern void oled_show_step(uint8_t step);
                oled_show_step(passo_para_interface);
            }
            oled_counter = 0;
        }
        
        last_passo_for_leds = passo_para_interface;
    }
}

void sequencer_sync_all_banks_simultaneously(void) {
    bool ram_on = presets_slave_is_playing_from_ram();
    // Pre-armar mute do LIVE no exato downbeat quando haverá ativação de slots
    // para evitar duplicação do passo 1 (LIVE + RAM simultâneos)
    if (tick_in_step == 1 && passo_atual_por_banco[0] == 1 && presets_slots_has_syncing()) {
        extern void sequencer_clear_all_midi_notes(void);
        presets_set_mute_live(true);
        sequencer_clear_all_midi_notes();
        for (int b = 0; b < NUM_BANCOS_SLAVE; b++) {
            live_step1_pending_fire[b] = false;
        }
    }
    bool downbeat_now = false;
    for (int banco = 0; banco < NUM_BANCOS_SLAVE; banco++) {
        ticks_por_banco[banco]++;
        
        uint8_t ticks_necessarios = calcular_ticks_por_passo(bancos[banco].resolution);
        
        if (ticks_por_banco[banco] >= ticks_necessarios) {
            ticks_por_banco[banco] = 0; // Reset contador individual
            
            // ✅ NOVA FUNÇÃO: Avançar passo considerando páginas
            uint8_t max_passos = bancos[banco].duas_paginas ? 16 : 8;
            
            if (passo_atual_por_banco[banco] >= max_passos) {
                passo_atual_por_banco[banco] = 1;
            } else {
                passo_atual_por_banco[banco]++;
            }

            // Capturar downbeat do Live (âncora = banco 0): passo 1 e tick_in_step == 1
            if (banco == 0) {
                extern uint8_t tick_in_step;
                downbeat_now = (passo_atual_por_banco[0] == 1) && (tick_in_step == 1);
            }
            
            // Sempre processar o banco (live), exceto se LIVE estiver mutado por presets
            if (!presets_should_mute_live()) {
                processar_banco_normal(banco);
            }
        }
        // Relógio dos slots RAM (independente por slot e banco)
        if (ram_on || presets_slots_has_syncing()) {
            for (int slot = 0; slot < NUM_RAM_SLOTS; slot++) {
                if (!(presets_slot_is_active(slot) || presets_slot_syncing[slot])) continue;
                const BancoConfig* rb = presets_ram_slots[slot];
                ticks_por_banco_slots[slot][banco]++;
                uint8_t ticks_necessarios_slot = calcular_ticks_por_passo(rb[banco].resolution);
                if (ticks_por_banco_slots[slot][banco] >= ticks_necessarios_slot) {
                    ticks_por_banco_slots[slot][banco] = 0;
                    uint8_t max_passos_slot = rb[banco].duas_paginas ? 16 : 8;
                    if (passo_atual_por_banco_slots[slot][banco] == 0) passo_atual_por_banco_slots[slot][banco] = 1;
                    else if (passo_atual_por_banco_slots[slot][banco] >= max_passos_slot) passo_atual_por_banco_slots[slot][banco] = 1;
                    else passo_atual_por_banco_slots[slot][banco]++;
            // Disparar passo do slot (somente RAM; LIVE está mutado)
                    uint8_t passo_do_banco = passo_atual_por_banco_slots[slot][banco];
                    if (rb[banco].duas_paginas) {
                        if (passo_do_banco >= 1 && passo_do_banco <= 8) {
                            if (rb[banco].passos_ativos[passo_do_banco - 1]) {
                                const StepParams* params = &rb[banco].step_params[passo_do_banco - 1];
                                sequencer_send_note(params->nota, params->velocity, params->channel);
                                sequencer_add_nota_ativa(params->nota, params->channel, params->velocity, params->length);
                            }
                        } else if (passo_do_banco >= 9 && passo_do_banco <= 16) {
                            uint8_t p2 = passo_do_banco - 8;
                            if (rb[banco].passos_ativos_pagina_2[p2 - 1]) {
                                const StepParams* params = &rb[banco].step_params_pagina_2[p2 - 1];
                                sequencer_send_note(params->nota, params->velocity, params->channel);
                                sequencer_add_nota_ativa(params->nota, params->channel, params->velocity, params->length);
                            }
                        }
                    } else {
                        if (passo_do_banco >= 1 && passo_do_banco <= 8) {
                            if (rb[banco].passos_ativos[passo_do_banco - 1]) {
                                const StepParams* params = &rb[banco].step_params[passo_do_banco - 1];
                                sequencer_send_note(params->nota, params->velocity, params->channel);
                                sequencer_add_nota_ativa(params->nota, params->channel, params->velocity, params->length);
                            }
                        }
                    }
                }
            }
        }
    }

    // Se estamos a sincronizar alguma slot e é downbeat do Live, ativar slot(s) agora no passo 1
    if (downbeat_now && presets_slots_has_syncing()) {
            for (int slot = 0; slot < NUM_RAM_SLOTS; slot++) {
            if (!presets_slot_syncing[slot]) continue;
            presets_slot_syncing[slot] = false;
            presets_slot_store(slot);
            presets_slot_active[slot] = true;
            // Forçar início dos relógios do slot no passo 1
                for (int b = 0; b < NUM_BANCOS_SLAVE; b++) {
                ticks_por_banco_slots[slot][b] = 0;
                passo_atual_por_banco_slots[slot][b] = 1;
            }
            presets_slave_set_play_from_ram(true);
            presets_set_mute_live(true);
            presets_slot_skip_tick[slot] = true; // evitar duplo disparo no primeiro tick
        }
    }

    // Saída armada: no primeiro downbeat arma, no segundo aplica limpeza do Live
    extern bool presets_exit_syncing; extern uint8_t presets_exit_stage;
    if (downbeat_now && presets_exit_syncing) {
        if (presets_exit_stage == 0) {
            presets_exit_stage = 1; // armado
        } else {
            // aplicar limpeza do Live no downbeat
            extern void inicializar_bancos_slave(void);
            extern uint8_t banco_atual;
            extern void editor_carregar_banco(uint8_t banco);
            extern void sequencer_reset_counters_slave(void);
            extern void leds_show_passo_atual_slave(void);
            extern void presets_set_mute_live(bool);
            inicializar_bancos_slave();
            banco_atual = 1;
            editor_carregar_banco(banco_atual);
            sequencer_reset_counters_slave();
            leds_show_passo_atual_slave();
            presets_set_mute_live(false);
            extern void presets_exit_clear_sync(void);
            presets_exit_clear_sync();
        }
    }
    sequencer_processar_notas_ativas();
}

void sequencer_clear_all_midi_notes(void) {
    sequencer_clear_all_notes_slave();
} 