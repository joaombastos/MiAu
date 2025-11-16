#include "modo preset/controller_preset.h"
#include "prests_slave/prests_slave.h"

bool preset_should_capture_step_buttons(void) {
    return presets_slots_is_edit_mode();
}

void preset_on_extra3_click(void) {
    if (presets_slots_is_edit_mode()) {
        // Sair do modo slots (com sync de saída e housekeeping)
        presets_exit_begin_sync();
        presets_slots_exit_edit_mode();
        extern void sequencer_clear_all_midi_notes(void);
        extern void leds_limpar_todos_passos(void);
        extern void leds_limpar_piscar_modo(void);
        extern void leds_suprimir_bancos(bool);
        sequencer_clear_all_midi_notes();
        leds_limpar_todos_passos();
        leds_limpar_piscar_modo();
        leds_suprimir_bancos(false);
    } else {
        // Entrar no modo slots (mostrar mapa de slots)
        presets_slots_enter_edit_mode();
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

void preset_on_step_press(uint8_t idx) {
    // Encaminhar para a camada de slots RAM (0..7)
    presets_slots_handle_step_press(idx);
}

void preset_on_step_long_press(uint8_t idx) {
    // Long press: apagar slot, se existir
    if (idx >= 8) return;
    if (presets_slot_is_filled(idx)) {
        extern void presets_slot_clear(uint8_t slot_idx);
        presets_slot_clear(idx);
        // Após limpar, garantir estado de reprodução correto
        presets_slave_set_play_from_ram(presets_slots_any_active());
    }
}


