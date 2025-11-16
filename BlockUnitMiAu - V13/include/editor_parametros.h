#ifndef EDITOR_PARAMETROS_H
#define EDITOR_PARAMETROS_H
#include <stdint.h>
#include "edit_system.h"
#include "modo slave/midi_sequencer_slave.h"

// Forward declarations das estruturas
typedef struct {
    uint8_t nota;
    uint8_t velocity;
    uint8_t channel;
    uint8_t length;
    uint8_t num_notas_extra;
} StepParams;

typedef struct {
    uint8_t resolution;
    bool duas_paginas;
    bool passos_ativos[8];
    bool passos_ativos_pagina_2[8];
    StepParams step_params[8];
    StepParams step_params_pagina_2[8];
} BancoConfig;


void editor_inicializar(void);

void editor_carregar_banco(uint8_t banco);
void editor_salvar_banco_atual(void);
void editor_toggle_passo(uint8_t passo);
void editor_clear_banco_atual(void);
void editor_entrar_modo_edicao(void);
void editor_ajustar_valor_parametro(int direction);
void editor_processar_encoder(void);



extern uint8_t banco_atual;
extern BancoConfig bancos[16]; 



// ===== Menu de Presets (Slots): FUNCIONALIDADE REMOVIDA =====

#endif