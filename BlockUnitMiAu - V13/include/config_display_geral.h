#ifndef CONFIG_DISPLAY_OLED_H
#define CONFIG_DISPLAY_OLED_H
#include <stdint.h>


#define DISPLAY_WIDTH 128                    // Largura total do display OLED
#define DISPLAY_HEIGHT 64                    // Altura total do display OLED


#define POS_TITULO_FASE_GRAVACAO_Y 20        // Posição Y dos títulos das fases (ESPERAR, GRAVAR, PROCESSANDO)
#define POS_NUMERO_FASE_GRAVACAO_Y 50        // Posição Y dos números das fases


#define POS_GERANDO_MIDI_X 10          // "Gerando MIDI Clock" (18 chars × 6px = 108px) → (128-108)/2 = 10
#define POS_AGUARDANDO_MIDI_X 4        // "Aguardando MIDI Clock" (20 chars × 6px = 120px) → (128-120)/2 = 4
#define POS_SEQUENCIADOR_X 13          // "Sequenciador Ativo" (17 chars × 6px = 102px) → (128-102)/2 = 13
#define POS_PRONTO_RECEBER_X 10        // "Pronto para Receber" (18 chars × 6px = 108px) → (128-108)/2 = 10


#define POS_MODO_X 1                   // Posição X do texto do modo
#define POS_MODO_Y 8                   // Posição Y do texto do modo
#define POS_PARAM_NOTA_Y 20            // Posição Y do parâmetro "Nota:"
#define POS_PARAM_VELOCITY_Y 30        // Posição Y do parâmetro "Vel:"
#define POS_PARAM_CANAL_Y 40           // Posição Y do parâmetro "Ch:"
#define POS_PARAM_LENGTH_Y 50          // Posição Y do parâmetro "Len:"
#define POS_PARAM_RESOLUTION_Y 60      // Posição Y do parâmetro "Res:"


#define POS_BANCO_QUADRADO_X 64        // Posição X do quadrado do banco
#define POS_BANCO_QUADRADO_Y 15        // Posição Y do quadrado do banco
#define BANCO_QUADRADO_TAMANHO 20      // Tamanho do quadrado (45x45 pixels)

#define BANCO_NUMERO_OFFSET_X 18       // Offset X do número
#define BANCO_NUMERO_OFFSET_Y 6        // Offset Y do número
#define BANCO_TEXTO_OFFSET_X 8         // Offset X do texto
#define BANCO_TEXTO_OFFSET_Y 26        // Offset Y do texto


#define TAMANHO_FRAME_LARGURA 45       // Largura padrão dos frames
#define TAMANHO_FRAME_ALTURA 10        // Altura padrão dos frames

// Posições removidas

// Delay para mensagens centradas
#define DELAY_MENSAGEM_CENTRADA_MS 2000  // Delay em ms para mensagens centradas
#define TAMANHO_FRAME_CANAL_LARGURA 30 // Largura específica do canal
#define TAMANHO_FRAME_LENGTH_LARGURA 50 // Largura específica do length

#define TAMANHO_FRAME_PARAMETRO_LARGURA 45   // Largura do frame de parâmetros
#define TAMANHO_FRAME_PARAMETRO_ALTURA 10    // Altura do frame de parâmetros
#define TAMANHO_FRAME_CANAL_ALTURA 10        // Altura específica do canal
#define TAMANHO_FRAME_LENGTH_ALTURA 10       // Altura específica do length
#define TAMANHO_FRAME_RESOLUCAO_LARGURA 45   // Largura específica da resolução
#define TAMANHO_FRAME_RESOLUCAO_ALTURA 10    // Altura específica da resolução


#define MARGEM_ESQUERDA 8              // Margem esquerda padrão
#define POS_RESOLUCAO_Y (POS_PARAM_LENGTH_Y + 10)  // Posição Y da resolução
#define POS_FRAME_RESOLUCAO_Y (POS_PARAM_LENGTH_Y + 2) // Posição Y do frame da resolução


#define LARGURA_NUMERO 10              // Largura padrão dos números
#define LARGURA_NUMERO_1 10            // "1"
#define LARGURA_NUMERO_2 10            // "2"
#define LARGURA_NUMERO_3 10            // "3"
#define LARGURA_NUMERO_4 10            // "4"
#define LARGURA_NUMERO_5 10            // "5"
#define LARGURA_NUMERO_6 10            // "6"
#define LARGURA_NUMERO_7 10            // "7"
#define LARGURA_NUMERO_8 10            // "8"


#define POS_TITULO_GRANDE_SLAVE_X 24   // "Drum Rack" (9 chars × 10px = 90px) → (128-90)/2 = 19 → 24 (ajustado)



#endif 