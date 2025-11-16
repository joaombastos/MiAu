#ifndef MIDI_SEQUENCER_SLAVE_H
#define MIDI_SEQUENCER_SLAVE_H

#include <stdint.h>
#include <stdbool.h>
#include "editor_parametros.h"
#include "modo slave/edit_system_slave.h"

#define MIDI_NOTE_OFF         0x80
#define MIDI_NOTE_ON          0x90
#define MIDI_CONTROL_CHANGE   0xB0
#define MIDI_PROGRAM_CHANGE   0xC0
#define MIDI_PITCH_BEND       0xE0
#define SEQUENCER_NUM_STEPS   8
#define SEQUENCER_NUM_CHANNELS 16
#define SEQUENCER_TICKS_PER_STEP 24


typedef struct {
    uint8_t nota;
    uint8_t channel;
    uint8_t ticks_restantes;
} NotaAtiva;

typedef struct Step {
    bool ativo;
    uint8_t nota;
} Step;


extern Step sequencia_slave[8];
extern uint8_t passo_atual;
extern uint32_t midi_tick_counter;
extern uint8_t tick_in_step;
extern uint8_t banco_atual;

extern bool mudanca_resolucao_pendente;
extern uint8_t nova_resolucao_pendente;
extern uint8_t banco_alterado_pendente;

extern NotaAtiva notas_ativas[8];

extern bool pagina_atual;
extern bool editando_pagina_2;

void sequencer_clear_all_notes_slave(void);
void processar_banco_normal(int banco);
void sequencer_processar_notas_ativas(void);
void aplicar_mudanca_resolucao(void);
void sequencer_reset_counters_slave(void);
void processar_botao_extra_1(void);


void sequencer_init(void);
void sequencer_tick(void);
void sequencer_tick_otimizado(void);
void sequencer_processar_nao_critico(void);
void sequencer_reset_position(void);
void sequencer_send_note(uint8_t nota, uint8_t velocity, uint8_t channel);
void sequencer_send_note_off(uint8_t nota, uint8_t channel);
void sequencer_clear_all_notes(void);
void sequencer_add_nota_ativa(uint8_t nota, uint8_t channel, uint8_t velocity, uint8_t length_percent);
void sequencer_update_resolution(void);
void sequencer_reset_all_counters(void);
void sequencer_verificar_sincronizacao(void);
uint8_t calcular_ticks_por_passo(uint8_t resolution);
void sequencer_mudanca_modo(void);




void carregar_banco_atual(void);
void navegar_menu(void);
void atualizar_interface(void);
void sequencer_sync_all_banks_simultaneously(void);

#endif 