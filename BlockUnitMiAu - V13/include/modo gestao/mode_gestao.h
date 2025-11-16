#ifndef MODE_GESTAO_H
#define MODE_GESTAO_H

#include <stdint.h>
#include <stdbool.h>
#include "prests_slave/prests_slave.h"
#include "modo slave/input_mode.h"

// Funções principais do modo gestão
void gestao_enter_mode(void);
bool gestao_is_active(void);
void gestao_oled_render(void);
void gestao_on_encoder_click(void);
void gestao_on_encoder_rotate(int direction);

// Chamada no downbeat do sequenciador para executar ação pendente sincronizada
void gestao_on_downbeat(void);

#endif // MODE_GESTAO_H


