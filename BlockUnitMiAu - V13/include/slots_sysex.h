#ifndef SLOTS_SYSEX_H
#define SLOTS_SYSEX_H

#include <stdint.h>
#include "editor_parametros.h"

// Modo de gestão de slots via SysEx
typedef enum {
    SLOTS_MODE_NONE = 0,
    SLOTS_MODE_SAVE,
    SLOTS_MODE_LOAD
} slots_mode_t;

// Fase do menu
typedef enum {
    SLOTS_PHASE_SELECT_ACTION = 0,  // Escolher guardar/carregar
    SLOTS_PHASE_SELECT_SLOT,        // Escolher slot (1-8, todas, nenhuma)
    SLOTS_PHASE_CONFIRM,            // Confirmar ação
    SLOTS_PHASE_EXECUTING           // Executando e saindo
} slots_phase_t;

// Estado do menu de slots
typedef struct {
    slots_mode_t mode;
    uint8_t selected_slot;  // 0-7 para slots individuais, 8 para todos, 9 para nenhum
    slots_phase_t phase;
    bool active;
} slots_menu_state_t;

// Inicialização
void slots_sysex_init(void);

// Entrada no modo (long press Extra3 no Master)
void slots_sysex_enter_mode(void);

// Saída do modo
void slots_sysex_exit_mode(void);

// Verificar se está no modo
bool slots_sysex_is_active(void);

// Processar encoder no modo de slots
void slots_sysex_process_encoder(int direction);
void slots_sysex_process_encoder_click(void);

// Processar botões de passos no modo de slots
void slots_sysex_process_step_button(uint8_t step);

// Atualizar OLED no modo de slots
void slots_sysex_update_oled(void);

// Executar ação (interna)
void slots_sysex_execute_action(void);

// Funções SysEx
void slots_sysex_send_slot(uint8_t slot_id);
void slots_sysex_send_all_slots(void);
void slots_sysex_receive_slot(uint8_t slot_id, const uint8_t* data, uint16_t length);

// Processar mensagens SysEx recebidas
void slots_sysex_process_received_sysex(const uint8_t* data, uint16_t length);

// Controlo MIDI
void slots_sysex_send_midi_stop(void);
void slots_sysex_send_midi_start(void);

// Verificar timeout para saída automática
void slots_sysex_check_timeout(void);

#endif // SLOTS_SYSEX_H
