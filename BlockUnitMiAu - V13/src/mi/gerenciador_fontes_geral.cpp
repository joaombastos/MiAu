#include "gerenciador_fontes_geral.h"
#include "oled.h"
static const uint8_t* fonte_atual = FONTE_PADRAO;

void fonte_set_tipo(TipoFonte tipo) {
    switch (tipo) {
        case FONTE_PEQUENA:
            fonte_atual = FONTE_6X10;
            break;
        case FONTE_GRANDE:
            fonte_atual = FONTE_10X20;
            break;
        case FONTE_MONO_PEQUENA:
            fonte_atual = FONTE_5X7;
            break;
        case FONTE_MONO_7PX:
            fonte_atual = FONTE_5X7;
            break;
        case FONTE_MONO_MEDIA:
            fonte_atual = FONTE_8X13;
            break;
        case FONTE_IMENSA:
            fonte_atual = FONTE_IMENSA_FONT;
            break;
        case FONTE_18PX:
            fonte_atual = FONTE_18PX_FONT;
            break;
        default:
            fonte_atual = FONTE_PADRAO;
            break;
    }
    oled_set_font(fonte_atual);
}

// Declaração externa da variável u8g2 do oled.cpp
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

int fonte_calcular_largura(const char* texto) {
    return u8g2.getStrWidth(texto);
}

int fonte_centralizar_x(const char* texto) {
    int largura = fonte_calcular_largura(texto);
    return (DISPLAY_WIDTH - largura) / 2;
} 