#ifndef DMACHINE_H
#define DMACHINE_H

#include <Arduino.h>

// Inicia o modo dmachine (SD, vozes, etc.)
void dmachine_begin();

// Executa o mixer do dmachine; chamar apenas quando ativo
void dmachine_tick();

// Estado do modo dmachine
bool dmachine_is_active();
void dmachine_set_active(bool enabled);

// Gestão de bancos (pastas no SD)
void dmachine_set_bank_index(int index); // 0..7
int dmachine_get_bank_index();
const char* dmachine_get_bank_name();

#endif


