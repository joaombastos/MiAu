#include "led_mux_geral.h"
#include "modo slave/led_mux_slave.h"
#include "modo slave/midi_sequencer_slave.h"
#include "editor_parametros.h"
#include "prests_slave/prests_slave.h"
#include "modo slave/input_mode.h"

extern uint8_t calcular_ticks_por_passo(uint8_t resolution);
static bool s_suprimir_bancos = false;

void leds_suprimir_bancos(bool enabled) { s_suprimir_bancos = enabled; }

// void leds_set_extra2(bool on) { REMOVIDO }

void leds_show_passo_atual_slave(void) {
    if (input_mode_is_active()) {
        // MODO INPUT ATIVO
        uint32_t led_state = 0;
        
        extern InputModeState input_mode_get_state(void);
        InputModeState state = input_mode_get_state();
        
        if (state == InputModeState::RECORDING) {
            // Durante gravação ativa: LED Extra 4 aceso; piscar slot disponível pré-selecionada
            led_state |= (1UL << 11); // LED Extra 4 aceso
            extern uint8_t input_mode_get_current_slot(void);
            uint8_t slot = input_mode_get_current_slot();
            extern uint32_t midi_tick_counter;
            bool blink = ((midi_tick_counter / 12) % 2) == 0; // ~2Hz
            if (slot < 8 && blink) {
                led_state |= (1UL << slot);
            }
        } else if (state == InputModeState::WAIT_PREROLL) {
            // Pre-count: LED Extra 4 pisca e piscar slot disponível
            extern uint32_t midi_tick_counter;
            bool led_on = (midi_tick_counter % 24) < 12; // Piscar BPM/2
            if (led_on) {
                led_state |= (1UL << 11);
            }
            extern uint8_t input_mode_get_current_slot(void);
            uint8_t slot = input_mode_get_current_slot();
            bool blink = ((midi_tick_counter / 12) % 2) == 0;
            if (slot < 8 && blink) {
                led_state |= (1UL << slot);
            }
        } else if (state == InputModeState::WAIT_STEP_PRESS) {
            // LEDs 1-8: aceso = ativo, pisca = gravado inativo; slot atual (nova gravação) pisca também
            extern uint32_t midi_tick_counter;
            bool blink = ((midi_tick_counter / 12) % 2) == 0;
            extern uint8_t input_mode_get_current_slot(void);
            uint8_t suggested = input_mode_get_current_slot();
            for (int i = 0; i < 8; i++) {
                extern bool input_mode_slot_is_active(uint8_t slot_idx);
                extern bool input_mode_slot_is_filled(uint8_t slot_idx);
                if (input_mode_slot_is_active(i)) {
                    led_state |= (1UL << i);
                } else if (input_mode_slot_is_filled(i) || i == suggested) {
                    if (blink) led_state |= (1UL << i);
                }
            }
        } else if (state == InputModeState::IDLE || state == InputModeState::WAIT_DOWNBEAT || state == InputModeState::WAIT_STEP_PRESS) {
            // LEDs 1-8: ativo = aceso; desativado/cheio = piscar; pendente de ativação = piscar rápido
            extern uint32_t midi_tick_counter;
            bool blink = ((midi_tick_counter / 12) % 2) == 0; // ~2Hz
            bool blink_fast = ((midi_tick_counter / 6) % 2) == 0; // ~4Hz
            for (int i = 0; i < 8; i++) {
                extern bool input_mode_slot_is_active(uint8_t slot_idx);
                extern bool input_mode_slot_is_filled(uint8_t slot_idx);
                bool is_active = input_mode_slot_is_active(i);
                bool is_filled = input_mode_slot_is_filled(i);
                if (is_active) {
                    led_state |= (1UL << i);
                } else if (is_filled) {
                    // piscar lento quando desativada
                    if (blink) led_state |= (1UL << i);
                } else {
                    // Se estiver a aguardar gravação: piscar a slot corrente (Nova Gravação X -> Slot X)
                    if (state == InputModeState::WAIT_DOWNBEAT) {
                        extern uint8_t input_mode_get_current_slot(void);
                        uint8_t suggested = input_mode_get_current_slot();
                        if (i == suggested && blink) led_state |= (1UL << i);
                    }
                }
            }
        }
        
        // Atualizar LEDs
        ledMux.write(led_state);
        return;
    } else {
        // MODO SLAVE NORMAL
        extern uint8_t passo_atual;
        extern bool modo_master;
        extern uint32_t midi_tick_counter;
        extern uint8_t tick_in_step;
        extern BancoConfig bancos[16]; 
        extern uint8_t passo_atual_por_banco[16]; 
        uint32_t led_state = 0;
        
        // Modo de edição de slots RAM: LEDs mostram estado dos slots
        if (presets_slots_is_edit_mode()) {
            for (int i = 0; i < 8; i++) {
                bool on = false;
                bool filled = presets_slot_is_filled(i);
                bool active = presets_slot_is_active(i);
                bool syncing = presets_slot_syncing[i] || presets_slot_toggle_syncing[i];
                if (active) {
                    on = true; // ATIVO = aceso
                } else if (filled) {
                    // DESATIVADO mas cheio = piscar ~2Hz
                    bool blink = ((midi_tick_counter / 12) % 2) == 0;
                    on = blink;
                } else if (syncing) {
                    // A criar (sync) = piscar mais rápido
                    bool blink_fast = ((midi_tick_counter / 6) % 2) == 0;
                    on = blink_fast;
                }
                if (on) led_state |= (1UL << i);
            }
            // LED Extra 3 ON enquanto estiver no modo de slots
            led_state |= (1UL << 10);
            // LED Extra 2: FUNCIONALIDADE REMOVIDA
            // LED Extra 4 para Modo Input - SÓ quando estiver ativo
            extern bool input_mode_is_active(void);
            if (input_mode_is_active()) {
                led_state |= (1UL << 11);
            }
            ledMux.write(led_state);
            return;
        }
        
        uint8_t passo_atual_do_banco = passo_atual;
        
        static uint8_t last_passo_visual = 0;
        static uint32_t last_tick_visual = 0;
        
        for (int i = 0; i < 8; i++) {
            bool passo_ativo = false;
            
            if (banco_atual >= 1 && banco_atual <= 16) { 
                if (editando_pagina_2 && bancos[banco_atual - 1].duas_paginas) {
                    passo_ativo = bancos[banco_atual - 1].passos_ativos_pagina_2[i];
                } else {
                    passo_ativo = bancos[banco_atual - 1].passos_ativos[i];
                }
            }
            
            bool eh_passo_atual = false;
            
            if (editando_pagina_2 && bancos[banco_atual - 1].duas_paginas) {
                uint8_t passo_pagina_2 = passo_atual_do_banco - 8;
                eh_passo_atual = (i + 1) == passo_pagina_2;
            } else {
                eh_passo_atual = (i + 1) == passo_atual_do_banco;
            }
            
            if (eh_passo_atual) {
                if (passo_ativo) {

                    uint8_t ticks_por_passo = calcular_ticks_por_passo(bancos[banco_atual - 1].resolution);
                    uint8_t tick_atual_no_passo = midi_tick_counter % ticks_por_passo;
                    
                    if (tick_atual_no_passo < (ticks_por_passo / 5)) {
                        passo_ativo = false;
                    }
                } else {

                    passo_ativo = true;
                }
            }
            

            extern uint32_t midi_tick_counter;
            
            if (passos_piscando[i]) {
                uint8_t tick_atual = midi_tick_counter % TICKS_MAXIMO_PISCAR; // Usar 48 como máximo para piscar
                passo_ativo = passo_ativo && (ticks_piscar[i][tick_atual] == 1);
            }
            

            if (!s_suprimir_bancos && passo_ativo) {
                led_state |= (1UL << i);
            }
        }
        

        extern bool editando_pagina_2;
        extern BancoConfig bancos[16];
        extern uint8_t banco_atual;
        

        if (editando_pagina_2) {
            led_state |= (1UL << 8); 
        }
        

        // LED Extra 4 para Modo Input - SÓ quando estiver ativo
        extern bool input_mode_is_active(void);
        if (input_mode_is_active()) {
            led_state |= (1UL << 11);
        }
        ledMux.write(led_state);
    }
} 