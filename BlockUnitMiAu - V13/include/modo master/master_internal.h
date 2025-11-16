#ifndef MASTER_INTERNAL_H
#define MASTER_INTERNAL_H

#include <stdint.h>

// Inicialização (cria recursos necessários)
void master_internal_init(void);

// Ativa/desativa o gerador interno (toggle via EXTRA 2)
void master_internal_activate(void);
void master_internal_deactivate(void);

// Estado
bool master_internal_is_active(void);

// BPM (24 PPQN). Valores típicos 40..300
void master_internal_set_bpm(uint16_t bpm);
uint16_t master_internal_get_bpm(void);
// Precisão: 24 PPQN com jitter baixo (timer HW + task RTOS). Saída DIN OUT direta da task.

// Loop helper: retirar bytes gerados (0xFA/0xF8/0xFC) e injetar no parser
bool master_internal_pop_byte(uint8_t* out_byte);

// Controlo de pausa do clock (para modo slots SYSEX)
void master_internal_pause_clock(void);
void master_internal_resume_clock(void);

#endif


