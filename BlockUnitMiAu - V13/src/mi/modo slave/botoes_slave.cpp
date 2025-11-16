#include "controlador_botoes_geral.h"
#include "au_overlay.h"
#include "modo slave/botoes_slave.h"
#include "modo slave/midi_sequencer_slave.h"
#include "oled.h"
#include "led_mux_geral.h"
#include "edit_system.h"
#include "editor_parametros.h"
#include "midi_din_geral.h"
#include <Arduino.h>
#include "prests_slave/prests_slave.h"
#include "modo slave/input_mode.h"
#include "modo master/master_internal.h"
#include "modo master/master_internal.h"
#include "modo preset/controller_preset.h"
#include "midi_ble.h"
#include "strings_texto.h"
#include "slots_sysex.h"


bool gerador_button_last = true;
extern uint8_t passo_atual;

bool botoes_ler_encoder(void) {
    for (int i = 0; i < 4; i++) {
        digitalWrite(mux_s[i], (9 >> i) & 0x01);
    }
    delayMicroseconds(2);
    return digitalRead(MUX_SIG);
}

bool botoes_ler_gerador(void) {
    for (int i = 0; i < 4; i++) {
        digitalWrite(mux_s[i], (10 >> i) & 0x01);
    }
    delayMicroseconds(5);
    return digitalRead(MUX_SIG);
}

bool botoes_ler_extra_1(void) {
    for (int i = 0; i < 4; i++) {
        digitalWrite(mux_s[i], (8 >> i) & 0x01);
    }
    delayMicroseconds(5);
    return digitalRead(MUX_SIG);
}

// bool botoes_ler_extra_2(void) { REMOVIDO }

bool botoes_ler_extra_3(void) {
    // Q11 do multiplexador (ver pinos.h)
    for (int i = 0; i < 4; i++) {
        digitalWrite(mux_s[i], (11 >> i) & 0x01);
    }
    delayMicroseconds(5);
    return digitalRead(MUX_SIG);
}

static bool botoes_ler_extra_4(void) {
    // Q12 do multiplexador (ver pinos.h)
    for (int i = 0; i < 4; i++) {
        digitalWrite(mux_s[i], (12 >> i) & 0x01);
    }
    delayMicroseconds(5);
    return digitalRead(MUX_SIG);
}

// Extra 6 (Q14): restaurado para TAP/REC
static bool botoes_ler_extra_6(void) {
    for (int i = 0; i < 4; i++) {
        digitalWrite(mux_s[i], (14 >> i) & 0x01);
    }
    delayMicroseconds(5);
    return digitalRead(MUX_SIG);
}

// Extra 5 (Q13)
static bool botoes_ler_extra_5(void) {
    for (int i = 0; i < 4; i++) {
        digitalWrite(mux_s[i], (13 >> i) & 0x01);
    }
    delayMicroseconds(5);
    return digitalRead(MUX_SIG);
}

void botoes_ler_passos_slave(bool* btn_last) {
    for (uint8_t i = 0; i < 8; i++) {
        for (int j = 0; j < 4; j++) {
            digitalWrite(mux_s[j], (i >> j) & 0x01);
        }
        delayMicroseconds(5);
        bool btn = digitalRead(MUX_SIG);

        if (preset_should_capture_step_buttons()) {
            // Modo PRESET (edição de slots): long-press apaga slot; click curto aciona
            static unsigned long step_press_ms[8] = {0};
            static bool step_long_done[8] = {false};
            if (btn_last[i] && !btn) { step_press_ms[i] = millis(); step_long_done[i] = false; }
            if (!btn && !step_long_done[i]) {
                unsigned long held = millis() - step_press_ms[i];
                if (held >= 1000) { extern void preset_on_step_long_press(uint8_t idx); preset_on_step_long_press(i); step_long_done[i] = true; }
            }
            if (btn && !btn_last[i]) { if (!step_long_done[i]) { preset_on_step_press(i); } }
        } else if (slots_sysex_is_active()) {
            // Modo SLOTS SYSEX: processar botões de passos
            if (btn && !btn_last[i]) {
                slots_sysex_process_step_button(i);
            }
        } else if (input_mode_is_active()) {
            // Modo INPUT: long-press (>=1000ms) limpa slot; click curto alterna/guarda
            extern bool input_mode_slot_is_filled(uint8_t slot_idx);
            extern bool input_mode_slot_is_active(uint8_t slot_idx);
            extern void input_mode_slot_activate(uint8_t slot_idx);
            extern void input_mode_slot_deactivate(uint8_t slot_idx);
            extern void input_mode_slot_clear(uint8_t slot_idx);
            extern void input_mode_on_step_press(uint8_t step);
            extern InputModeState input_mode_state(void);
            static unsigned long input_step_press_ms[8] = {0};
            static bool input_step_long_done[8] = {false};
            if (btn_last[i] && !btn) { input_step_press_ms[i] = millis(); input_step_long_done[i] = false; }
            if (!btn && !input_step_long_done[i]) {
                unsigned long held = millis() - input_step_press_ms[i];
                if (held >= 1000) { if (input_mode_slot_is_filled(i)) { input_mode_slot_clear(i); } input_step_long_done[i] = true; }
            }
            if (btn && !btn_last[i]) {
                if (!input_step_long_done[i]) {
                    if (input_mode_state() == InputModeState::WAIT_STEP_PRESS) {
                        input_mode_on_step_press(i + 1);
                    } else if (input_mode_slot_is_filled(i)) {
                        if (input_mode_slot_is_active(i)) { input_mode_slot_deactivate(i); } else { input_mode_slot_activate(i); }
                    }
                }
            }
        } else {
            // Editor normal: somente na borda de descida
            if (btn_last[i] && !btn) {
                extern void editor_toggle_passo(uint8_t passo);
                editor_toggle_passo(i + 1);
            }
        }

        btn_last[i] = btn;
    }
}

void botoes_processar_mudanca_modo_slave(void) {
    bool gerador_button = botoes_ler_gerador();
    bool extra_1_button = botoes_ler_extra_1();
    // bool extra_2_button = botoes_ler_extra_2(); // REMOVIDO
    bool extra_3_button = botoes_ler_extra_3();
    bool extra_4_button = botoes_ler_extra_4();
    bool extra_6_button = botoes_ler_extra_6();
    // Extra 7 (Q15): botão sem função
    bool extra_7_button;
    {
        for (int i = 0; i < 4; i++) { digitalWrite(mux_s[i], (15 >> i) & 0x01); }
        delayMicroseconds(5);
        extra_7_button = digitalRead(MUX_SIG);
    }
    static bool extra_1_button_last = true;
    // static bool extra_2_button_last = true; // REMOVIDO
    static bool extra_3_button_last = true;
    static bool extra_4_button_last = true;
    static bool extra_6_button_last = true;
    static bool extra_7_button_last = true;
    // Estado para long-press e TAP do Extra 6
    static unsigned long extra6_press_ms = 0;
    static bool extra6_long_done = false;
    static unsigned long tap_times[4] = {0,0,0,0};
    static uint8_t tap_index = 0;
    // Removido: static unsigned long extra4_press_ms = 0;
    // Removido: static bool extra4_long_done = false;

    // EXTRA 2 (usando o mesmo pino "gerador"): toggle clock interno em click
    if (gerador_button == true && gerador_button_last == false) {
        if (master_internal_is_active()) {
            master_internal_deactivate();
        } else {
            master_internal_activate();
        }
    }
    gerador_button_last = gerador_button;

    if (extra_1_button == false && extra_1_button_last == true) {
        extern void processar_botao_extra_1(void);
        processar_botao_extra_1();
    }
    extra_1_button_last = extra_1_button;
    // Extra 5: Toggle BLE enable/disable with OLED feedback
    static bool extra_5_button_last = true;
    bool extra_5_button;
    {
        for (int i = 0; i < 4; i++) { digitalWrite(mux_s[i], (13 >> i) & 0x01); }
        delayMicroseconds(5);
        extra_5_button = digitalRead(MUX_SIG);
    }
    if (extra_5_button == false && extra_5_button_last == true) {
        // Toggle BLE enabled state and show short OLED message
        extern void oled_show_message_centered(const char* text);
        extern bool master_internal_is_active(void);
        extern void midi_ble_set(bool);
        bool now = !midi_ble::is_enabled();
        midi_ble::set_enabled(now);
        if (now) {
            // On enable, set role according to mode
            if (master_internal_is_active()) {
                midi_ble::set_master_tx_only(true);
                midi_ble::set_slave_rx_only(false);
                midi_ble::set_allow_note_tx_in_slave(false);
                oled_show_message_centered(STR_BLE_ON_MASTER);
            } else {
                midi_ble::set_master_tx_only(false);
                midi_ble::set_slave_rx_only(true);
                midi_ble::set_allow_note_tx_in_slave(true);
                oled_show_message_centered(STR_BLE_ON_SLAVE);
            }
        } else {
            oled_show_message_centered(STR_BLE_OFF);
        }
    }
    extra_5_button_last = extra_5_button;


    // Botão Extra 2: toggle do gerador interno (START/STOP + entra/sai)
    // Observação: reader de extra_2 foi removido no roteamento atual. Se reativar a linha acima,
    // adaptar aqui para usar a leitura.
    // Implementação direta: usar transição de borda no pino Q10 se necessário.

    // Extra 3: long-press (>=1s) -> modo slots SysEx (apenas no Master); short-press mantém comportamento atual
    static unsigned long extra3_press_ms = 0;
    static bool extra3_long_done = false;
    if (extra_3_button_last && !extra_3_button) { extra3_press_ms = millis(); extra3_long_done = false; }
    if (!extra_3_button && !extra3_long_done) {
        unsigned long held = millis() - extra3_press_ms;
        if (held >= 1000) {
            // Long press: entrar no modo slots SysEx (apenas no Master)
            extern bool master_internal_is_active(void);
            extern void slots_sysex_enter_mode(void);
            if (master_internal_is_active()) {
                slots_sysex_enter_mode();
            }
            extra3_long_done = true;
        }
    }
    if (extra_3_button && !extra_3_button_last) {
        if (!extra3_long_done) {
            // Short press: verificar se estamos no modo slots SysEx
            extern bool slots_sysex_is_active(void);
            extern void slots_sysex_exit_mode(void);
            if (slots_sysex_is_active()) {
                // Sair do modo slots SysEx
                slots_sysex_exit_mode();
            } else {
                // Modo de edição de slots RAM (comportamento existente)
                preset_on_extra3_click();
                if (!preset_should_capture_step_buttons()) {
                    extern void leds_limpar_todos_passos(void);
                    extern void leds_limpar_piscar_modo(void);
                    extern void leds_suprimir_bancos(bool);
                    extern void sequencer_clear_all_midi_notes(void);
                    sequencer_clear_all_midi_notes();
                    leds_limpar_todos_passos();
                    leds_limpar_piscar_modo();
                    leds_suprimir_bancos(false);
                } else {
                    extern void leds_limpar_todos_passos(void);
                    extern void leds_limpar_piscar_modo(void);
                    extern void leds_suprimir_bancos(bool);
                    extern void leds_show_passo_atual_slave(void);
                    leds_limpar_todos_passos();
                    leds_limpar_piscar_modo();
                    leds_suprimir_bancos(true);
                    leds_show_passo_atual_slave();
                }
            }
        }
    }
    extra_3_button_last = extra_3_button;

    // EXTRA 4: Click simples ativa/desativa modo input; Long press entra no modo slots SysEx input
    static unsigned long extra4_press_ms = 0;
    static bool extra4_long_done = false;
    if (extra_4_button_last && !extra_4_button) { 
        extra4_press_ms = millis(); 
        extra4_long_done = false; 
    }
    if (!extra_4_button && !extra4_long_done) {
        unsigned long held = millis() - extra4_press_ms;
        if (held >= 1000) {
            // Long press: comportamento normal (apenas no Master)
            extern bool master_internal_is_active(void);
            extra4_long_done = true;
        }
    }
    if (extra_4_button && !extra_4_button_last) {
        if (!extra4_long_done) {
            // Short press: ativa/desativa modo input
            input_mode_toggle_active();
        }
    }
    extra_4_button_last = extra_4_button;

    // EXTRA 6: TAP (4x) no modo master; Long-press inicia gravação.
    // No modo slave: click e long-press iniciam gravação.
    extern bool master_internal_is_active(void);
    extern void master_internal_set_bpm(uint16_t bpm);
    if (extra_6_button_last && !extra_6_button) {
        // Pressionado agora (borda de descida)
        extra6_press_ms = millis();
        extra6_long_done = false;
    }
    if (!extra_6_button && !extra6_long_done) {
        unsigned long held = millis() - extra6_press_ms;
        if (held >= 1000) {
            // Long press -> iniciar gravação (master e slave)
            extern void input_mode_start_recording(void);
            input_mode_start_recording();
            extra6_long_done = true;
            // Reset sequência de TAP
            tap_index = 0;
        }
    }
    if (extra_6_button && !extra_6_button_last) {
        // Soltou agora (borda de subida) -> click curto se não for long-press
        if (!extra6_long_done) {
            if (master_internal_is_active()) {
                // TAP tempo: 4 cliques para definir BPM
                unsigned long now = millis();
                // Timeout para reiniciar sequência (2s)
                if (tap_index > 0 && (now - tap_times[tap_index - 1]) > 2000) {
                    tap_index = 0;
                }
                tap_times[tap_index++] = now;
                if (tap_index >= 4) {
                    // Calcular média dos 3 intervalos
                    uint32_t i1 = tap_times[1] - tap_times[0];
                    uint32_t i2 = tap_times[2] - tap_times[1];
                    uint32_t i3 = tap_times[3] - tap_times[2];
                    uint32_t avg = (i1 + i2 + i3) / 3u;
                    if (avg > 0) {
                        uint16_t bpm = (uint16_t) (60000UL / avg);
                        if (bpm < 40) bpm = 40;
                        if (bpm > 300) bpm = 300;
                        master_internal_set_bpm(bpm);
                    }
                    tap_index = 0;
                }
            } else {
                // Modo slave: click inicia gravação
                extern void input_mode_start_recording(void);
                input_mode_start_recording();
            }
        }
    }
    extra_6_button_last = extra_6_button;

    // EXTRA 7: toggle overlay AU
    if (extra_7_button == false && extra_7_button_last == true) {
        au_overlay_toggle();
    }
    extra_7_button_last = extra_7_button;
}

void botoes_ativar_modo_slave(void) {

    midi::ativar_modo_slave(true);

    extern void leds_limpar_todos_passos(void);
    leds_limpar_todos_passos();
    extern void editor_entrar_modo_edicao(void);
    editor_entrar_modo_edicao();
    extern void sequencer_mudanca_modo(void);
    sequencer_mudanca_modo();
}


void botoes_ler_passos(bool* btn_last) {
    botoes_ler_passos_slave(btn_last);
}

void botoes_processar_mudanca_modo(void) {
    botoes_processar_mudanca_modo_slave();
} 