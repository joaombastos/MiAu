#include "prests_slave/prests_slave.h"
#include <string.h>

BancoConfig presets_ram_slave[NUM_BANCOS_SLAVE];
BancoConfig presets_ram_slots[NUM_RAM_SLOTS][NUM_BANCOS_SLAVE];
bool presets_slot_filled[NUM_RAM_SLOTS] = {false};
bool presets_slot_active[NUM_RAM_SLOTS] = {false};
bool presets_slot_syncing[NUM_RAM_SLOTS] = {false};
bool presets_slot_skip_tick[NUM_RAM_SLOTS] = {false};
uint8_t presets_slot_sync_stage[NUM_RAM_SLOTS] = {0};
bool presets_slot_toggle_syncing[NUM_RAM_SLOTS] = {false};
uint8_t presets_slot_toggle_stage[NUM_RAM_SLOTS] = {0};
bool presets_slot_toggle_target[NUM_RAM_SLOTS] = {false};

static bool s_presets_tem_dados = false;
static bool s_play_from_ram = false;
static bool s_slots_edit_mode = false;
static bool s_mute_live_while_presets = false;
bool presets_exit_syncing = false;
uint8_t presets_exit_stage = 0;
static bool s_unmute_live_next_tick = false;

extern BancoConfig bancos[NUM_BANCOS_SLAVE];

void presets_slave_init_ram(void) {
    memset(presets_ram_slave, 0, sizeof(presets_ram_slave));
    s_presets_tem_dados = false;
    memset(presets_ram_slots, 0, sizeof(presets_ram_slots));
    for (int i = 0; i < NUM_RAM_SLOTS; i++) {
        presets_slot_filled[i] = false;
        presets_slot_active[i] = false;
    }
    s_slots_edit_mode = false;
}

void presets_slave_store_from_bancos(void) {
    for (int i = 0; i < NUM_BANCOS_SLAVE; i++) {
        presets_ram_slave[i] = bancos[i];
    }
    s_presets_tem_dados = true;
}

void presets_slave_play_now_from_ram(void) {
    if (!s_presets_tem_dados) return;
    s_play_from_ram = true;
}

bool presets_slave_has_data(void) { return s_presets_tem_dados; }
bool presets_slave_is_playing_from_ram(void) { return s_play_from_ram; }
void presets_slave_set_play_from_ram(bool enabled) {
    s_play_from_ram = enabled;
    if (!enabled) {
        s_unmute_live_next_tick = true;
    }
}
const BancoConfig* presets_slave_get_read_array(void) {
    if (s_play_from_ram && s_presets_tem_dados) {
        return presets_ram_slave;
    }
    return bancos;
}

void presets_slots_enter_edit_mode(void) { s_slots_edit_mode = true; }
void presets_slots_exit_edit_mode(void) { s_slots_edit_mode = false; }
bool presets_slots_is_edit_mode(void) { return s_slots_edit_mode; }
bool presets_slots_has_syncing(void) { for (int i=0;i<NUM_RAM_SLOTS;i++) if (presets_slot_syncing[i]) return true; return false; }

bool presets_slots_any_active(void) {
    for (int i = 0; i < NUM_RAM_SLOTS; i++) if (presets_slot_active[i]) return true;
    return false;
}

bool presets_slot_is_filled(uint8_t slot_idx) { return slot_idx < NUM_RAM_SLOTS ? presets_slot_filled[slot_idx] : false; }
bool presets_slot_is_active(uint8_t slot_idx) { return slot_idx < NUM_RAM_SLOTS ? presets_slot_active[slot_idx] : false; }

void presets_slot_store(uint8_t slot_idx) {
    if (slot_idx >= NUM_RAM_SLOTS) return;
    for (int b = 0; b < NUM_BANCOS_SLAVE; b++) {
        presets_ram_slots[slot_idx][b] = bancos[b];
    }
    presets_slot_filled[slot_idx] = true;
}

void presets_slots_handle_step_press(uint8_t slot_idx) {
    if (slot_idx >= NUM_RAM_SLOTS) return;
    if (!presets_slot_filled[slot_idx]) {
        presets_slots_begin_sync(slot_idx);
    } else {
        presets_slot_toggle_syncing[slot_idx] = true;
        presets_slot_toggle_stage[slot_idx] = 0;
        presets_slot_toggle_target[slot_idx] = !presets_slot_active[slot_idx];
    }
    presets_slave_set_play_from_ram(presets_slots_any_active());
}

void presets_slots_begin_sync(uint8_t slot_idx) {
    if (slot_idx >= NUM_RAM_SLOTS) return;
    presets_slot_syncing[slot_idx] = true;
    presets_slot_sync_stage[slot_idx] = 0;
    extern void sequencer_reset_slot_counters(uint8_t slot_idx);
    sequencer_reset_slot_counters(slot_idx);
}

void presets_slot_clear(uint8_t slot_idx) {
    if (slot_idx >= NUM_RAM_SLOTS) return;
    memset(presets_ram_slots[slot_idx], 0, sizeof(presets_ram_slots[slot_idx]));
    presets_slot_filled[slot_idx] = false;
    presets_slot_active[slot_idx] = false;
    presets_slot_syncing[slot_idx] = false;
    presets_slot_toggle_syncing[slot_idx] = false;
    presets_slot_skip_tick[slot_idx] = false;
    presets_slot_sync_stage[slot_idx] = 0;
    presets_slot_toggle_stage[slot_idx] = 0;
    extern void sequencer_clear_all_midi_notes(void);
    sequencer_clear_all_midi_notes();
}

void presets_set_mute_live(bool enabled) { s_mute_live_while_presets = enabled; }
bool presets_should_mute_live(void) { return s_mute_live_while_presets; }
void presets_request_unmute_live_next_tick(void) { s_unmute_live_next_tick = true; }
bool presets_unmute_live_pending(void) { return s_unmute_live_next_tick; }
void presets_clear_unmute_live_pending(void) { s_unmute_live_next_tick = false; }

void presets_exit_begin_sync(void) { presets_exit_syncing = true; presets_exit_stage = 0; }
bool presets_exit_is_syncing(void) { return presets_exit_syncing; }
void presets_exit_clear_sync(void) { presets_exit_syncing = false; presets_exit_stage = 0; }


