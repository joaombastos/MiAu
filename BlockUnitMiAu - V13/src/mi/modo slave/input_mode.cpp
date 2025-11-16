#include "modo slave/input_mode.h"
#include "oled.h"
#include "led_mux_geral.h"
#include "pinos.h"
#include "midi_din_geral.h"
#include "modo slave/config_display_input.h"
#include <Arduino.h>

// Estado interno
static bool s_active = false;
static InputModeState s_state = InputModeState::IDLE;
static InputModeParams s_params {8, 16, 1}; // bars=8, channel=16(fixo), pre=1
static uint8_t s_current_param = 0; // 0=bars, 1=channel, 2=preRoll
static int8_t s_pending_activate_slot = -1; // -1 = none

// Timeline
static uint32_t s_tick_in_cycle = 0;      // 0..(bars*4*24 - 1)
static uint32_t s_total_ticks_cycle = 8 * 4 * 24; // calculado via params
static uint8_t s_preroll_remaining_bars = 0;

// Sistema de slots automáticos (8 slots)
static const int NUM_INPUT_SLOTS = 8;
static InputSlot s_slots[NUM_INPUT_SLOTS];
static uint8_t s_current_slot = 0; // Slot atual sendo usado
static bool s_pending_activate_flags[NUM_INPUT_SLOTS]; // Agendar ativação no próximo downbeat

// Buffers
static const int MAX_EVENTS = 2048; // Aumentado para 2048 eventos
static InputEvent s_events[MAX_EVENTS];
static int s_event_count = 0;

// Notas ativas para limitar a 16 simultâneas durante reprodução
struct PlayingNote { uint8_t note; uint8_t channel; uint8_t velocity; uint32_t off_tick; bool on; };
static PlayingNote s_playing[16];


static inline uint32_t clamp_u32(uint32_t v, uint32_t lo, uint32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void rec_clear()
{
	s_event_count = 0;
}

static void rec_commit_event(uint8_t status, uint8_t channel, uint8_t data1, uint8_t data2, uint32_t tick)
{
    if (s_event_count >= MAX_EVENTS) {
        return;
    }

	// Sempre gravar no tick exato (sem quantização)
	uint8_t type = status & 0xF0;
	uint32_t event_tick = tick;
	// Normalizar ao ciclo
	event_tick %= s_total_ticks_cycle;

	// Registar evento
	s_events[s_event_count++] = {status, channel, data1, data2, event_tick};
	
}

static void playing_clear()
{
	for (int i = 0; i < 16; ++i) s_playing[i] = {0,0,0,0,false};
}

// Função para reproduzir eventos de slots ativos
static void play_active_slots(uint32_t tick) {
    // Contar slots ativas
    int active_slots_count = 0;
    for (int slot_idx = 0; slot_idx < NUM_INPUT_SLOTS; slot_idx++) {
        if (s_slots[slot_idx].active && s_slots[slot_idx].filled) {
            active_slots_count++;
        }
    }

    for (int slot_idx = 0; slot_idx < NUM_INPUT_SLOTS; slot_idx++) {
        if (s_slots[slot_idx].active && s_slots[slot_idx].filled) {
            const InputSlot& slot = s_slots[slot_idx];
            uint32_t slot_total_ticks = static_cast<uint32_t>(slot.bars) * 4u * 24u; // 96 por bar
            if (slot_total_ticks == 0) continue;
            uint32_t local_tick = (tick + slot.phase_offset) % slot_total_ticks;

            // NOVA FUNCIONALIDADE: Note Off automático no final do loop
            // Se estamos no último tick do loop, forçar Note Off para todas as notas ativas desta slot
            if (local_tick == slot_total_ticks - 1) {
                for (int k = 0; k < 10; ++k) {
                    if (slot.playing[k].on) {
                        midi::sendNoteOff(slot.playing[k].note, 0, slot.channel);
                        s_slots[slot_idx].playing[k].on = false;
                    }
                }
            }

            for (int i = 0; i < slot.event_count; i++) {
                if (slot.events[i].tick == local_tick) {
                    uint8_t ch = slot.channel;
                    uint8_t type = slot.events[i].status & 0xF0;
                    if (type == 0x90 && slot.events[i].data2 > 0) {
                        midi::sendNoteOn(slot.events[i].data1, slot.events[i].data2, ch);
                        // registrar nota ON nesta slot
                        for (int k = 0; k < 10; ++k) if (!slot.playing[k].on) { s_slots[slot_idx].playing[k] = {slot.events[i].data1, ch, true}; break; }
                    } else if (type == 0x80 || (type == 0x90 && slot.events[i].data2 == 0)) {
                        midi::sendNoteOff(slot.events[i].data1, 0, ch);
                        // limpar nota correspondente
                        for (int k = 0; k < 10; ++k) if (slot.playing[k].on && slot.playing[k].note == slot.events[i].data1 && slot.playing[k].channel == ch) { s_slots[slot_idx].playing[k].on = false; break; }
                    } else if (type == 0xB0) {
                        uint8_t buffer[3] = {static_cast<uint8_t>(0xB0 | (ch - 1)), slot.events[i].data1, slot.events[i].data2};
                        Serial2.write(buffer, 3);
                    } else if (type == 0xC0) {
                        uint8_t buffer[2] = {static_cast<uint8_t>(0xC0 | (ch - 1)), slot.events[i].data1};
                        Serial2.write(buffer, 2);
                    }
                }
            }
        }
    }
}

static void send_event_now(const InputEvent& ev)
{
	uint8_t ch = s_params.channelMode; // Sempre usar o canal especificado (1-16)
	uint8_t type = ev.status & 0xF0;
	if (type == 0x90 && ev.data2 > 0) {
		midi::sendNoteOn(ev.data1, ev.data2, ch);
		// Reproduzir exatamente como foi gravado - sem duração fixa
		for (int i = 0; i < 16; ++i) if (!s_playing[i].on) { s_playing[i] = {ev.data1, ch, ev.data2, 0, true}; break; }
	} else if (type == 0x80 || (type == 0x90 && ev.data2 == 0)) {
		midi::sendNoteOff(ev.data1, 0, ch);
		for (int i = 0; i < 16; ++i) if (s_playing[i].on && s_playing[i].note == ev.data1 && s_playing[i].channel == ch) { s_playing[i].on = false; break; }
	} else if (type == 0xB0) {
		uint8_t buffer[3] = {static_cast<uint8_t>(0xB0 | (ch - 1)), ev.data1, ev.data2};
		Serial2.write(buffer, 3);
	} else if (type == 0xC0) {
		uint8_t buffer[2] = {static_cast<uint8_t>(0xC0 | (ch - 1)), ev.data1};
		Serial2.write(buffer, 2);
	}
}

static void recalculate_cycle_ticks()
{
	// AFINADO: 1 compasso = 4 passos (em vez de 8)
	s_total_ticks_cycle = static_cast<uint32_t>(s_params.bars) * 4u * 24u;
	s_tick_in_cycle = 0;
}

void input_mode_init(void) {
    s_active = false;
    s_state = InputModeState::IDLE; // Começar em IDLE, não em gravação
    s_event_count = 0;
    s_tick_in_cycle = 0;
    s_preroll_remaining_bars = 0;
    
    // Limpar slots
    input_mode_slots_clear_all();
    for (int i = 0; i < NUM_INPUT_SLOTS; ++i) s_pending_activate_flags[i] = false;
    
    // Inicializar parâmetros
    s_params.bars = 8;
    s_params.preRoll = 1; // 1 compasso de pre-count por defeito
    s_params.channelMode = 16; // Canal 16 por defeito
    s_current_param = 0;
    
    // Calcular ticks do ciclo
    recalculate_cycle_ticks();
}

void input_mode_toggle_active(void) {
    // DEBUG: verificar se a função está sendo chamada
    
    
    if (s_active) {
        // Desativar modo input MAS MANTER SLOTS A TOCAR
        
        s_active = false;
        s_state = InputModeState::IDLE;
        
        // NOVA PROTEÇÃO: Limpar todas as notas ativas em reprodução para evitar notas penduradas
        for (int i = 0; i < 16; ++i) {
            if (s_playing[i].on) {
                midi::sendNoteOff(s_playing[i].note, 0, s_playing[i].channel);
                s_playing[i].on = false;
            }
        }
        
        // NÃO cancelar ativação pendente – permitir sync no próximo downbeat
        // s_pending_activate_slot = -1; // REMOVIDO
        
        // NÃO desativar slots – continuam a tocar por cima de Slave + RAM
        // for (int i = 0; i < NUM_INPUT_SLOTS; i++) { s_slots[i].active = false; }
        
        // NÃO limpar notas em reprodução
        // playing_clear();
        
        // Garantir LIVE conforme presets
        extern bool presets_should_mute_live(void);
        extern void presets_set_mute_live(bool);
        if (!presets_should_mute_live()) {
            presets_set_mute_live(false);
        }
        
        // Atualizar interface
        input_mode_render_oled();
    } else {
        // Ativar modo input
        
        s_active = true;
        s_state = InputModeState::IDLE; // Começar em IDLE, não em gravação
        
        // Limpar gravação anterior (apenas para nova gravação)
        rec_clear();
        
        // Garantir que o som LIVE continue (sem sobrepor mute de Presets)
        extern bool presets_should_mute_live(void);
        extern void presets_set_mute_live(bool);
        if (!presets_should_mute_live()) {
            presets_set_mute_live(false);
        }
        
        // NÃO limpar slots existentes - manter as que já estão a tocar
        // input_mode_slots_clear_all(); // REMOVIDO - manter slots existentes
        
        // Atualizar interface
        input_mode_render_oled();
    }
    
    
}

// Função para garantir que o som LIVE nunca seja mutado
void input_mode_ensure_live_sound(void)
{
    // Garantir LIVE apenas se Presets não estiver a pedir mute
    extern bool presets_should_mute_live(void);
    extern void presets_set_mute_live(bool);
    if (!presets_should_mute_live()) {
        presets_set_mute_live(false);
    }
}

bool input_mode_is_active(void) { 
    bool result = s_active;
    return result; 
}
InputModeState input_mode_state(void) { return s_state; }

void input_mode_on_clock_tick(void) {
    // Reproduzir SEMPRE as slots ativas do modo input, mesmo com o modo input desativado
    extern uint32_t midi_tick_counter;
    play_active_slots(midi_tick_counter);

    if (!s_active) return;
    
    // Aguardar downbeat (tick 1 do passo 1) para iniciar pre-roll/rec
    if (s_state == InputModeState::WAIT_DOWNBEAT) {
        extern uint8_t tick_in_step;
        extern uint8_t passo_atual_por_banco[];
        if (passo_atual_por_banco[0] == 1 && tick_in_step == 1) {
            // No downbeat agora: iniciar pre-roll ou gravação
            if (s_params.preRoll > 0) {
                s_state = InputModeState::WAIT_PREROLL;
                s_preroll_remaining_bars = s_params.preRoll;
                s_tick_in_cycle = 0;
            } else {
                s_state = InputModeState::RECORDING;
                s_tick_in_cycle = 0;
            }
        }
    }

    // Downbeat: aplicar ativações pendentes para entrar exatamente no Passo 1
    {
        extern uint8_t tick_in_step;
        extern uint8_t passo_atual_por_banco[];
        if (passo_atual_por_banco[0] == 1 && tick_in_step == 1) {
            extern uint32_t midi_tick_counter;
            for (int slot_idx = 0; slot_idx < NUM_INPUT_SLOTS; ++slot_idx) {
                if (s_pending_activate_flags[slot_idx]) {
                    InputSlot& slot = s_slots[slot_idx];
                    uint32_t slot_total_ticks = static_cast<uint32_t>(slot.bars) * 4u * 24u;
                    if (slot_total_ticks == 0) slot_total_ticks = 96; // fallback
                    uint32_t t = midi_tick_counter % slot_total_ticks;
                    slot.phase_offset = (slot_total_ticks - t) % slot_total_ticks;
                    slot.active = true;
                    s_pending_activate_flags[slot_idx] = false;
                }
            }
            if (s_pending_activate_slot >= 0 && s_pending_activate_slot < NUM_INPUT_SLOTS) {
                InputSlot& slot = s_slots[s_pending_activate_slot];
                uint32_t slot_total_ticks = static_cast<uint32_t>(slot.bars) * 4u * 24u;
                if (slot_total_ticks == 0) slot_total_ticks = 96;
                uint32_t t = midi_tick_counter % slot_total_ticks;
                slot.phase_offset = (slot_total_ticks - t) % slot_total_ticks;
                slot.active = true;
                s_pending_activate_slot = -1;
            }
        }
    }

    // Pre-count
    if (s_state == InputModeState::WAIT_PREROLL && s_preroll_remaining_bars > 0) {
        s_tick_in_cycle++;
        if (s_tick_in_cycle >= 96) { // 1 bar
            s_preroll_remaining_bars--;
            s_tick_in_cycle = 0;
            if (s_preroll_remaining_bars == 0) {
                s_state = InputModeState::RECORDING;
                s_tick_in_cycle = 0;
            }
        }
    }

    // Recording
    if (s_state == InputModeState::RECORDING) {
        s_tick_in_cycle++;
        if (s_tick_in_cycle >= s_total_ticks_cycle) {
            // Guardar automaticamente na próxima slot livre (sem sobrescrever) e ativar no próximo downbeat
            if (s_event_count > 0) {
                input_mode_slot_store_auto();
                if (s_slots[s_current_slot].filled) {
                    s_pending_activate_flags[s_current_slot] = true;
                }
            }
            
            // Gravação terminou - voltar a IDLE imediatamente
            s_state = InputModeState::IDLE;
            s_tick_in_cycle = 0;
        }
    }
}

// API pública: iniciar gravação (ambos os modos).
// Se o modo input não estiver ativo, ativa-o e agenda gravação.
void input_mode_start_recording(void) {
    if (!s_active) {
        // Ativar modo input sem interromper slots existentes
        s_active = true;
        s_state = InputModeState::IDLE;
        rec_clear();
        input_mode_render_oled();
    }
    // Agendar início no próximo downbeat, respeitando preRoll
    s_state = InputModeState::WAIT_DOWNBEAT;
    s_tick_in_cycle = 0;
}

void input_mode_on_transport_start(void)
{
	if (!s_active) return;
	
	// Se estivermos em WAIT_STEP_PRESS, não fazer nada - aguardar que o utilizador prema um passo
	if (s_state == InputModeState::WAIT_STEP_PRESS) {
		return;
	}
	
	// Se estivermos gravando, inicializar pre-count se configurado
	if (s_state == InputModeState::RECORDING && s_params.preRoll > 0) {
		s_state = InputModeState::WAIT_PREROLL; // Voltar para pre-count
		s_preroll_remaining_bars = s_params.preRoll; // Configurar pre-count
		s_tick_in_cycle = 0; // Reset apenas se voltar para pre-count
	}
	
	// Se estivermos em WAIT_PREROLL, não fazer nada - continuar pre-count
	if (s_state == InputModeState::WAIT_PREROLL) {
		return;
	}
}

void input_mode_on_transport_continue(void)
{
	if (!s_active) return;
	
	// Continuar transporte: manter posição atual do ciclo
	// Não resetar s_tick_in_cycle para manter sincronização
	// Não limpar notas ativas se estivermos reproduzindo
}

void input_mode_on_transport_stop(void)
{
	if (!s_active) return;
	
	// Se estivermos em WAIT_STEP_PRESS, não fazer nada - aguardar que o utilizador prema um passo
	if (s_state == InputModeState::WAIT_STEP_PRESS) {
		return;
	}
	
	// Se estivermos gravando ou em pre-count, voltar para IDLE
	if (s_state == InputModeState::RECORDING || s_state == InputModeState::WAIT_PREROLL) {
		s_state = InputModeState::IDLE;
		s_tick_in_cycle = 0;
		rec_clear(); // Limpar gravação anterior
		playing_clear(); // Limpar notas ativas
		
		// INICIALIZAR PRE-COUNT se configurado
		if (s_params.preRoll > 0) {
			s_preroll_remaining_bars = s_params.preRoll;
		}
	}
}

// Função chamada quando o utilizador preme um passo no estado WAIT_STEP_PRESS
void input_mode_on_step_press(uint8_t step) {
	if (!s_active || s_state != InputModeState::WAIT_STEP_PRESS) return;
	
	uint8_t slot_idx = (step > 0) ? (step - 1) : 0; // 1..8 -> 0..7
	if (slot_idx >= NUM_INPUT_SLOTS) slot_idx = NUM_INPUT_SLOTS - 1;
	
	// Armazenar gravação diretamente no slot escolhido
	if (s_event_count > 0) {
		// Se o utilizador escolheu um slot diferente do pré-selecionado, usa o escolhido.
		if (slot_idx != s_current_slot) {
			s_current_slot = slot_idx;
		}
		InputSlot& slot = s_slots[s_current_slot];
		slot.event_count = s_event_count;
		for (int i = 0; i < s_event_count; i++) {
			slot.events[i] = s_events[i];
		}
		slot.bars = s_params.bars;
		slot.channel = s_params.channelMode;
		slot.filled = true;
		slot.active = false;
		// Agendar ativação sincronizada
		s_pending_activate_slot = static_cast<int8_t>(s_current_slot);
		extern void presets_request_unmute_live_next_tick(void);
		presets_request_unmute_live_next_tick();
	}
	
	// Voltar para IDLE, aguardando próxima gravação
	s_state = InputModeState::IDLE;
	s_tick_in_cycle = 0;
	rec_clear(); // Limpar para próxima gravação
	
	// INICIALIZAR PRE-COUNT se configurado
	if (s_params.preRoll > 0) {
		s_preroll_remaining_bars = s_params.preRoll;
	}
}

void input_mode_on_midi(uint8_t status, uint8_t channel, uint8_t data1, uint8_t data2)
{
	if (!s_active) {
		return;
	}
	if (s_state != InputModeState::RECORDING) {
		return;
	}
	
	// Só gravar eventos do canal 16
	if (channel != 16) {
		return;
	}
	
	uint8_t type = status & 0xF0;
	// Registar Note On/Off, CC, PC, PitchBend, Aftertouch se quiser - aqui CC/PC/Note.
	if (type == 0x90 || type == 0x80 || type == 0xB0 || type == 0xC0) {
		rec_commit_event(status, channel, data1, data2, s_tick_in_cycle);
	}
}

bool input_mode_led_on(uint32_t midi_tick_counter)
{
	if (!s_active) return false;
	
	if (s_state == InputModeState::WAIT_PREROLL) {
		// LED pisca durante pre-count (BPM do MIDI clock)
		return (midi_tick_counter % 24) < 12; // Piscar a cada 1/4 de passo
	}
	
	if (s_state == InputModeState::RECORDING) {
		// LED aceso durante gravação
		return true;
	}
	
	if (s_state == InputModeState::WAIT_STEP_PRESS) {
		// LED aceso durante aguardar passo
		return true;
	}
	
	// Estado padrão: LED aceso (modo input ativo)
	return true;
}

void input_mode_render_oled(void) {
    oled_clear();
    fonte_set_tipo(FONTE_MONO_7PX);
    oled_draw_str(INPUT_POS_TITLE_X, INPUT_POS_TITLE_Y, STR_INPUT_TITLE);
    
    char buf[24];

    // Bars
    snprintf(buf, sizeof(buf), "%s%u", STR_INPUT_PARAM_BARS, s_params.bars);
    oled_draw_str(INPUT_POS_BARS_X, INPUT_POS_BARS_Y, buf);
    if (s_current_param == 0) {
        extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
        u8g2.drawFrame(INPUT_FRAME_BARS_X, INPUT_FRAME_BARS_Y, INPUT_FRAME_BARS_W, INPUT_FRAME_BARS_H);
    }
    
    // Channel e PreRoll
    const char* PREs[3] = {STR_INPUT_PREROLL_NO, STR_INPUT_PREROLL_1, STR_INPUT_PREROLL_2};
    snprintf(buf, sizeof(buf), "%s%u", STR_INPUT_PARAM_CHANNEL, s_params.channelMode);
    oled_draw_str(INPUT_POS_CHANNEL_X, INPUT_POS_CHANNEL_Y, buf);
    if (s_current_param == 1) {
        extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
        u8g2.drawFrame(INPUT_FRAME_CHANNEL_X, INPUT_FRAME_CHANNEL_Y, INPUT_FRAME_CHANNEL_W, INPUT_FRAME_CHANNEL_H);
    }
    
    snprintf(buf, sizeof(buf), "%s%s", STR_INPUT_PRE_REC_BAR, PREs[clamp_u32(s_params.preRoll,0,2)]);
    oled_draw_str(INPUT_POS_PREROLL_X, INPUT_POS_PREROLL_Y, buf);
    if (s_current_param == 2) {
        extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
        u8g2.drawFrame(INPUT_FRAME_PREROLL_X, INPUT_FRAME_PREROLL_Y, INPUT_FRAME_PREROLL_W, INPUT_FRAME_PREROLL_H);
    }

    oled_update();
}

const InputModeParams& input_mode_get_params(void) { return s_params; }
void input_mode_set_params(const InputModeParams& p) {
	s_params = p;
	recalculate_cycle_ticks();
}

// Funções de controle de parâmetros via encoder
void input_mode_next_param(void) {
	s_current_param = (s_current_param + 1) % 3; // 3 parâmetros: bars, channel, preRoll
	
	// Atualizar OLED imediatamente para mostrar mudança de parâmetro
	input_mode_render_oled();
}

void input_mode_adjust_param(int direction) {
	switch (s_current_param) {
		case 0: // bars
			s_params.bars = clamp_u32(s_params.bars + direction, 1, 8);
			break;
		case 1: // channelMode
			s_params.channelMode = clamp_u32(s_params.channelMode + direction, 1, 16);
			break;
		case 2: // preRoll
			s_params.preRoll = clamp_u32(s_params.preRoll + direction, 0, 2);
			break;
	}
	recalculate_cycle_ticks();
	
	// Atualizar OLED imediatamente para mostrar mudanças
	input_mode_render_oled();
}

void input_mode_reset_params(void) {
	s_params = {2, 16, 1}; // reset para valores padrão
	s_current_param = 0;
	recalculate_cycle_ticks();
	
	// Atualizar OLED imediatamente para mostrar reset
	input_mode_render_oled();
}

uint8_t input_mode_get_current_param(void) {
	return s_current_param;
}

// ===== SISTEMA DE SLOTS AUTOMÁTICOS =====

void input_mode_slot_store_auto(void) {
	// Encontrar próximo slot livre
	uint8_t free_slot = input_mode_get_next_free_slot();
	if (free_slot == 255) {
		// Todos os slots cheios: não sobrescrever automatica/implicitamente
		return;
	}
	
	// Guardar eventos
	InputSlot& slot = s_slots[free_slot];
	slot.event_count = s_event_count;
	for (int i = 0; i < s_event_count; i++) {
		slot.events[i] = s_events[i];
	}
	slot.bars = s_params.bars;
	slot.channel = s_params.channelMode;
	slot.filled = true;
	slot.active = false; // só ativa quando o utilizador escolhe o passo
	s_current_slot = free_slot;
}

uint8_t input_mode_get_next_free_slot(void) {
	for (uint8_t i = 0; i < NUM_INPUT_SLOTS; i++) {
		if (!s_slots[i].filled) {
			return i;
		}
	}
	return 255; // Todos os slots estão cheios
}

void input_mode_slot_activate(uint8_t slot_idx) {
	if (slot_idx >= NUM_INPUT_SLOTS) return;
	if (!s_slots[slot_idx].filled) return;
    // Agendar ativação no próximo downbeat para entrar no Passo 1
    s_pending_activate_flags[slot_idx] = true;
}

void input_mode_slot_deactivate(uint8_t slot_idx) {
	if (slot_idx >= NUM_INPUT_SLOTS) return;
    s_slots[slot_idx].active = false;
    s_pending_activate_flags[slot_idx] = false;
    s_slots[slot_idx].phase_offset = 0;
    // Enviar NoteOff para todas as notas que esta slot possa ter deixado ON
    for (int k = 0; k < 10; ++k) {
        if (s_slots[slot_idx].playing[k].on) {
            midi::sendNoteOff(s_slots[slot_idx].playing[k].note, 0, s_slots[slot_idx].playing[k].channel);
            s_slots[slot_idx].playing[k].on = false;
        }
    }
}

void input_mode_slot_clear(uint8_t slot_idx) {
    if (slot_idx >= NUM_INPUT_SLOTS) return;
    // garantir silêncio
    input_mode_slot_deactivate(slot_idx);
    // apagar dados
    s_slots[slot_idx].filled = false;
    s_slots[slot_idx].event_count = 0;
    s_slots[slot_idx].bars = 0;
    s_slots[slot_idx].phase_offset = 0;
    for (int i = 0; i < MAX_EVENTS; ++i) {
        // não é necessário limpar conteúdo se contamos só por event_count, mas zere por segurança leve
        // s_slots[slot_idx].events[i] = {0,0,0,0,0}; // opcional
        ;
    }
    for (int k = 0; k < 10; ++k) s_slots[slot_idx].playing[k] = {0,0,false};
}

bool input_mode_slot_is_filled(uint8_t slot_idx) {
	if (slot_idx >= NUM_INPUT_SLOTS) return false;
	return s_slots[slot_idx].filled;
}

bool input_mode_slot_is_active(uint8_t slot_idx) {
	if (slot_idx >= NUM_INPUT_SLOTS) return false;
	return s_slots[slot_idx].active;
}

void input_mode_slots_clear_all(void) {
	for (int i = 0; i < NUM_INPUT_SLOTS; i++) {
		s_slots[i].filled = false;
		s_slots[i].active = false;
		s_slots[i].event_count = 0;
		s_slots[i].phase_offset = 0;
		for (int k = 0; k < 10; ++k) s_slots[i].playing[k] = {0,0,false};
	}
	s_current_slot = 0;
}

uint8_t input_mode_get_preroll_remaining(void) {
    return s_preroll_remaining_bars;
}

InputModeState input_mode_get_state(void) {
    return s_state;
}

uint8_t input_mode_get_current_slot(void) {
    return s_current_slot;
}

int8_t input_mode_get_pending_activation_slot(void) { return s_pending_activate_slot; }

// Função para obter dados da slot
InputSlot input_mode_get_slot_data(uint8_t slot_idx) {
    InputSlot empty_slot = {0};
    if (slot_idx >= NUM_INPUT_SLOTS) return empty_slot;
    return s_slots[slot_idx];
}

// Função para importar dados da slot
void input_mode_import_slot(uint8_t slot_idx, const InputSlot* in_slot) {
    if (slot_idx >= NUM_INPUT_SLOTS) return;
    if (!in_slot) return;
    
    // Copiar dados principais
    s_slots[slot_idx].event_count = in_slot->event_count;
    s_slots[slot_idx].bars = in_slot->bars;
    s_slots[slot_idx].channel = in_slot->channel;
    s_slots[slot_idx].filled = in_slot->filled;
    s_slots[slot_idx].active = false; // Sempre inativo após importação
    s_slots[slot_idx].phase_offset = 0; // Reset phase offset
    
    // Copiar eventos
    for (int i = 0; i < in_slot->event_count && i < 256; i++) {
        s_slots[slot_idx].events[i] = in_slot->events[i];
    }
    
    // Inicializar array de playing
    for (int k = 0; k < 10; k++) {
        s_slots[slot_idx].playing[k] = {0, 0, false};
    }
}


