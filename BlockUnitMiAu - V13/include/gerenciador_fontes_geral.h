#ifndef GERENCIADOR_FONTES_H
#define GERENCIADOR_FONTES_H

#include <stdint.h>

typedef enum {
    FONTE_PEQUENA,
    FONTE_GRANDE,
    FONTE_MONO_PEQUENA,
    FONTE_MONO_7PX,
    FONTE_MONO_MEDIA,
    FONTE_18PX,
    FONTE_IMENSA
} TipoFonte;

#define FONTE_6X10 u8g2_font_6x10_tr
#define FONTE_8X13 u8g2_font_8x13_tr
#define FONTE_10X20 u8g2_font_10x20_tr
#define FONTE_5X7 u8g2_font_5x7_tr
#define FONTE_18PX_FONT u8g2_font_inb24_mn
#define FONTE_IMENSA_FONT u8g2_font_10x20_tr
#define FONTE_PADRAO FONTE_6X10

void fonte_set_tipo(TipoFonte tipo);
int fonte_calcular_largura(const char* texto);
int fonte_centralizar_x(const char* texto);

#endif