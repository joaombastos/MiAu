#ifndef PRESTS_SLAVE_H
#define PRESTS_SLAVE_H

#include <stdint.h>
#include "editor_parametros.h"  // Usa BancoConfig

#define NUM_BANCOS_SLAVE 16
#define NUM_RAM_SLOTS 8

// Slot único legado (slot 0) – manter para compatibilidade interna
extern BancoConfig presets_ram_slave[NUM_BANCOS_SLAVE];

// 8 slots de RAM: cada slot guarda 16 bancos
extern BancoConfig presets_ram_slots[NUM_RAM_SLOTS][NUM_BANCOS_SLAVE];
extern bool presets_slot_filled[NUM_RAM_SLOTS];
extern bool presets_slot_active[NUM_RAM_SLOTS];
extern bool presets_slot_syncing[NUM_RAM_SLOTS];
extern bool presets_slot_skip_tick[NUM_RAM_SLOTS];
extern uint8_t presets_slot_sync_stage[NUM_RAM_SLOTS]; // 0 = armar, 1 = aguardar próximo downbeat para ativar

// Sync de saída (sair de presets para modo slave limpo)
extern bool presets_exit_syncing;
extern uint8_t presets_exit_stage; // 0 armar, 1 ativar no próximo downbeat
void presets_exit_begin_sync(void);
bool presets_exit_is_syncing(void);
void presets_exit_clear_sync(void);

// Sync para toggles (ativar/desativar) com downbeat duplo
extern bool presets_slot_toggle_syncing[NUM_RAM_SLOTS];
extern uint8_t presets_slot_toggle_stage[NUM_RAM_SLOTS]; // 0 armar, 1 aplicar
extern bool presets_slot_toggle_target[NUM_RAM_SLOTS];   // estado desejado

// Inicializa/limpa a RAM de presets (sem side effects noutros módulos)
void presets_slave_init_ram(void);

// Copia o estado atual dos bancos (bancos[]) para a RAM de presets
void presets_slave_store_from_bancos(void);

// Indica se a RAM já recebeu algum conteúdo válido desde o boot
bool presets_slave_has_data(void);

// Liga leitura direta (legado); atualmente será true quando existir ao menos um slot ativo
void presets_slave_play_now_from_ram(void);

// Controla/leitura do modo de reprodução direto da RAM
bool presets_slave_is_playing_from_ram(void);
void presets_slave_set_play_from_ram(bool enabled);

// Ponteiro de leitura usado pelo sequenciador (bancos ou presets RAM)
const BancoConfig* presets_slave_get_read_array(void);

// Gestão de slots (edição via botões de passos)
void presets_slots_enter_edit_mode(void);
void presets_slots_exit_edit_mode(void);
bool presets_slots_is_edit_mode(void);
bool presets_slots_has_syncing(void);

// Press de botão de passo no modo de slots:
// - se slot vazio: grava bancos no slot e ativa-o
// - se slot cheio: alterna ativo/desativo
void presets_slots_handle_step_press(uint8_t slot_idx);
// Inicia sincronização (esperar passo 1 tick 0) para criar slot
void presets_slots_begin_sync(uint8_t slot_idx);

// Utilitários
bool presets_slots_any_active(void);
bool presets_slot_is_filled(uint8_t slot_idx);
bool presets_slot_is_active(uint8_t slot_idx);
void presets_slot_store(uint8_t slot_idx); // grava snapshot nos slots
// Apaga conteúdos da slot e desativa
void presets_slot_clear(uint8_t slot_idx);

// Silenciar LIVE enquanto em presets após sync
void presets_set_mute_live(bool enabled);
bool presets_should_mute_live(void);
void presets_request_unmute_live_next_tick(void);
bool presets_unmute_live_pending(void);
void presets_clear_unmute_live_pending(void);

// Reset dos contadores do relógio de um slot para sincronização
void sequencer_reset_slot_counters(uint8_t slot_idx);

// (sem persistência em flash)

#endif // PRESTS_SLAVE_H


