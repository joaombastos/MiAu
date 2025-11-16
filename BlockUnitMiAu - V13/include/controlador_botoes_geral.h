#ifndef CONTROLADOR_BOTOES_H
#define CONTROLADOR_BOTOES_H
#include <stdint.h>
#include <Arduino.h>
#include "pinos.h"

// ============================================================================
// CONSTANTES E VARIÁVEIS COMUNS
// ============================================================================

extern const uint8_t mux_s[4];
#define MUX_SIG SLAVE_MUX_CD74HC4067_SIG

// ============================================================================
// FUNÇÕES COMUNS - HARDWARE E ROTEAMENTO
// ============================================================================

// Inicialização do hardware de botões
void botoes_inicializar(void);

// Leitura de botões físicos (comum a todos os modos)
bool botoes_ler_encoder(void);
bool botoes_ler_gerador(void);
bool botoes_ler_extra_1(void);
// bool botoes_ler_extra_2(void); // REMOVIDO
bool botoes_ler_extra_3(void);
// Removidos: botoes_ler_extra_4
bool botoes_ler_passo(uint8_t passo);

// Função de roteamento para leitura de passos (comum)
void botoes_ler_passos(bool* btn_last);

// Função de roteamento para processamento de mudança de modo (comum)
void botoes_processar_mudanca_modo(void);

#endif