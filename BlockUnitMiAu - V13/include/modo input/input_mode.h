#ifndef INPUT_MODE_H
#define INPUT_MODE_H

#include <Arduino.h>

// Estados do Modo Input
enum class InputModeState : uint8_t {
	IDLE = 0,           // Modo normal, aguardando ação
	WAIT_PREROLL = 1,   // Aguardando pre-count
	RECORDING = 2,      // Gravando eventos MIDI
	WAIT_STEP_PRESS = 3 // Gravação terminou, aguardando pressionar passo
};

// Parâmetros do Modo Input
struct InputModeParams {
	uint8_t bars;        // 1..8 (default 1)
	uint8_t quant;       // 0..6 (mapa: 1/8T,1/4,1/8,1/16,1/16T,1/32,FREE) (default FREE)
	uint8_t channelMode; // 0..16 (0 = EQUAL, 1..16 fixo) (default 0 = EQUAL) - CANAL DE SAÍDA
	uint8_t preRoll;     // 0..2 (0=NO PRE, 1=1 bar, 2=2 bars) (default 1)
};

// Inicialização
void input_mode_init(void);

// Ativação e controlo por botão EXTRA 4
void input_mode_toggle_active(void);  // Long press
bool input_mode_is_active(void);
InputModeState input_mode_state(void);

// Clock e transporte
void input_mode_on_clock_tick(void);      // chamar em cada 0xF8
void input_mode_on_transport_start(void); // 0xFA
void input_mode_on_transport_continue(void); // 0xFB
void input_mode_on_transport_stop(void);  // 0xFC

// Controle de passos
void input_mode_on_step_press(uint8_t step); // chamar quando utilizador preme passo no WAIT_STEP_PRESS

// Entrada MIDI para gravação (Note/CC/PC)
void input_mode_on_midi(uint8_t status, uint8_t channel, uint8_t data1, uint8_t data2);

// LED Extra4: se deve estar ON no frame atual (usar em render dos LEDs)
bool input_mode_led_on(uint32_t midi_tick_counter);

// OLED: desenhar UI do modo input
void input_mode_render_oled(void);

// Acesso/ajuste opcional de parâmetros (defaults já definidos)
const InputModeParams& input_mode_get_params(void);
void input_mode_set_params(const InputModeParams& p);

// Controle via encoder
void input_mode_next_param(void);
void input_mode_adjust_param(int direction);
void input_mode_reset_params(void);
uint8_t input_mode_get_current_param(void);

// Sistema de slots
void input_mode_slot_activate(uint8_t slot_idx);
void input_mode_slot_deactivate(uint8_t slot_idx);
bool input_mode_slot_is_active(uint8_t slot_idx);
bool input_mode_slot_is_filled(uint8_t slot_idx);
uint8_t input_mode_get_next_free_slot(void);
uint8_t input_mode_get_current_slot(void);

#endif
