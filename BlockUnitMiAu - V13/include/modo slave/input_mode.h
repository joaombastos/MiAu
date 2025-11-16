#ifndef INPUT_MODE_H
#define INPUT_MODE_H

#include <Arduino.h>
#include <stdint.h>

// Estados do modo input
enum class InputModeState {
	IDLE = 0,           // Modo normal, aguardando ação
    WAIT_DOWNBEAT = 4,  // Aguardar próximo tick 1 do passo 1
	WAIT_PREROLL = 1,   // Aguardando pre-count
	RECORDING = 2,      // Gravando eventos MIDI
	WAIT_STEP_PRESS = 3 // Gravação terminou, aguardando pressionar passo
};

// Parâmetros configuráveis
struct InputModeParams {
	uint8_t bars;        // 1-8 compassos
	uint8_t channelMode; // 1-16 (canal MIDI de saída)
	uint8_t preRoll;     // 0=NO PRE, 1=PRE1, 2=PRE2
};

// Estrutura para eventos MIDI gravados
struct InputEvent {
	uint8_t status;   // 0x8n..0xEn
	uint8_t channel;  // 1..16
	uint8_t data1;    // nota/cc/pc
	uint8_t data2;    // vel/valor
	uint32_t tick;    // posição em ticks dentro do ciclo
};

// Estrutura para slots de gravação
struct InputSlot {
	InputEvent events[256];   // Reduzido de 1024 para 256 eventos por slot
	int event_count;          // Número de eventos gravados
	uint8_t bars;             // Número de compassos gravados
	uint8_t channel;          // Canal MIDI usado
	bool filled;               // Slot tem dados
	bool active;               // Slot está ativo/reproduzindo
	uint32_t phase_offset;     // Deslocamento de fase para sincronizar no Tick 1 do Passo 1
    struct { uint8_t note; uint8_t channel; bool on; } playing[10]; // Notas atualmente ligadas por esta slot
};

// Funções principais
void input_mode_init(void);
// Ativação e controlo por botão EXTRA 4
void input_mode_toggle_active(void);
void input_mode_on_clock_tick(void);
void input_mode_on_transport_start(void);
void input_mode_on_transport_continue(void);
void input_mode_on_transport_stop(void);
// Ação rápida: armar gravação para o próximo downbeat
void input_mode_start_recording_on_downbeat(void);
// Callback do sequenciador: chamado no downbeat (tick 1 do passo 1)
void input_mode_on_downbeat_step1(void);
void input_mode_on_step_press(uint8_t step); // chamar quando utilizador preme passo no WAIT_STEP_PRESS
void input_mode_on_midi(uint8_t status, uint8_t channel, uint8_t data1, uint8_t data2);

// Funções de controle de parâmetros via encoder
void input_mode_next_param(void);
void input_mode_adjust_param(int direction);
void input_mode_reset_params(void);
uint8_t input_mode_get_current_param(void);
const InputModeParams& input_mode_get_params(void);
void input_mode_set_params(const InputModeParams& p);

// Funções de renderização
void input_mode_render_oled(void);
// Controle de LEDs
void input_mode_led_on(void);

// Funções de estado
InputModeState input_mode_get_state(void);
bool input_mode_is_active(void);
InputModeState input_mode_state(void);
uint8_t input_mode_get_preroll_remaining(void);
uint8_t input_mode_get_current_slot(void);
int8_t input_mode_get_pending_activation_slot(void);

// Funções de slots automáticos
void input_mode_slot_store_auto(void);           // Armazena automaticamente em slot vazio
void input_mode_slot_activate(uint8_t slot_idx); // Ativa slot específico
void input_mode_slot_deactivate(uint8_t slot_idx); // Desativa slot específico
void input_mode_slot_clear(uint8_t slot_idx); // Limpa slot (apaga eventos e desativa)
bool input_mode_slot_is_filled(uint8_t slot_idx); // Verifica se slot tem dados
bool input_mode_slot_is_active(uint8_t slot_idx); // Verifica se slot está ativo
uint8_t input_mode_get_next_free_slot(void);     // Retorna próximo slot livre (0-7)
void input_mode_slots_clear_all(void);           // Limpa todos os slots

// Função para garantir que o som LIVE nunca seja mutado no modo input
void input_mode_ensure_live_sound(void);

// Export/Import para Modo Gestão
bool input_mode_export_slot(uint8_t slot_idx, InputSlot* out_slot);
void input_mode_import_slot(uint8_t slot_idx, const InputSlot* in_slot);

InputSlot input_mode_get_slot_data(uint8_t slot_idx);

#endif // INPUT_MODE_H


