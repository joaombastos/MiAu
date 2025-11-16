#ifndef BOTOES_SLAVE_H
#define BOTOES_SLAVE_H
#include <stdint.h>

void botoes_processar_mudanca_modo_slave(void);
void botoes_ler_passos_slave(bool* btn_last);
void botoes_ativar_modo_slave(void);

#endif 