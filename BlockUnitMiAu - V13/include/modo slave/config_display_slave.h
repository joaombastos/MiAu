#ifndef CONFIG_DISPLAY_SLAVE_H
#define CONFIG_DISPLAY_SLAVE_H
#include <stdint.h>
#include "config_display_geral.h"


#define NOTA_BASE_SLAVE 36             // Nota C2 (36) - ponto de partida
#define NOTA_BASE_OFFSET_SLAVE 1       // Offset para cálculo de notas base
#define VELOCITY_PADRAO_SLAVE 100      // Velocity padrão
#define LENGTH_PADRAO_SLAVE 100        // Length padrão
#define RESOLUCAO_PADRAO_SLAVE 3       // Resolução padrão (1/8)



// Posições do texto do modo
#define POS_MODO_X_SLAVE 0            // Posição X do texto do modo slave
#define POS_MODO_Y_SLAVE 10             // Posição Y do texto do modo slave

// Posições dos parâmetros
#define POS_PARAM_NOTA_X_SLAVE 5      // Posição X do parâmetro "Nota:"
#define POS_PARAM_NOTA_Y_SLAVE 20     // Posição Y do parâmetro "Nota:"
#define POS_PARAM_VELOCITY_X_SLAVE 5  // Posição X do parâmetro "Vel:"
#define POS_PARAM_VELOCITY_Y_SLAVE 30  // Posição Y do parâmetro "Vel:"
#define POS_PARAM_CANAL_X_SLAVE 5    // Posição X do parâmetro "Ch:"
#define POS_PARAM_CANAL_Y_SLAVE 40     // Posição Y do parâmetro "Ch:"
#define POS_PARAM_LENGTH_X_SLAVE 5     // Posição X do parâmetro "Len:"
#define POS_PARAM_LENGTH_Y_SLAVE 50    // Posição Y do parâmetro "Len:"
#define POS_PARAM_RESOLUTION_X_SLAVE 5 // Posição X do parâmetro "Res:"
#define POS_PARAM_RESOLUTION_Y_SLAVE 60 // Posição Y do parâmetro "Res:"

// Posições do quadrado do banco
#define POS_BANCO_QUADRADO_X_SLAVE 65  // Posição X do quadrado do banco
#define POS_BANCO_QUADRADO_Y_SLAVE 15 // Posição Y do quadrado do banco
#define BANCO_QUADRADO_TAMANHO_SLAVE 48 // Tamanho do quadrado (45x45 pixels)

// Posições do número dentro do quadrado
#define POS_BANCO_NUMERO_X_SLAVE 80   // Posição X do número
#define POS_BANCO_NUMERO_Y_SLAVE 35    // Posição Y do número

// Posições do texto dentro do quadrado
#define POS_BANCO_TEXTO_X_SLAVE 70    // Posição X do texto
#define POS_BANCO_TEXTO_Y_SLAVE 55     // Posição Y do texto

// Posição do texto de passos
#define POS_TEXTO_PASSOS_X_SLAVE 65    // Posição X do texto "1BAR" / "2BAR"
#define POS_TEXTO_PASSOS_Y_SLAVE 10     // Posição Y do texto de passos

// Posições dos frames de parâmetros
#define POS_FRAME_RESOLUCAO_X_SLAVE 5  // Posição X do frame da resolução
#define POS_FRAME_RESOLUCAO_Y_SLAVE 52 // Posição Y do frame da resolução (8px acima do texto Y=60)

// Tamanhos dos frames
#define TAMANHO_FRAME_PARAMETRO_LARGURA_SLAVE 45   // Largura do frame de parâmetros
#define TAMANHO_FRAME_PARAMETRO_ALTURA_SLAVE 10    // Altura do frame de parâmetros
#define TAMANHO_FRAME_CANAL_LARGURA_SLAVE 30       // Largura específica do canal
#define TAMANHO_FRAME_CANAL_ALTURA_SLAVE 10        // Altura específica do canal
#define TAMANHO_FRAME_LENGTH_LARGURA_SLAVE 50      // Largura específica do length
#define TAMANHO_FRAME_LENGTH_ALTURA_SLAVE 10       // Altura específica do length
#define TAMANHO_FRAME_RESOLUCAO_LARGURA_SLAVE 45   // Largura específica da resolução
#define TAMANHO_FRAME_RESOLUCAO_ALTURA_SLAVE 10    // Altura específica da resolução

#endif 