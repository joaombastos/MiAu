#ifndef OLED_H
#define OLED_H
#include <stdint.h>
#include <U8g2lib.h>
#include "pinos.h"
#include "modo slave/config_display_slave.h"
#include "strings_texto.h"
#include "gerenciador_fontes_geral.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
void oled_init(void);
void oled_clear(void);
void oled_draw_str(uint8_t x, uint8_t y, const char* texto);
void oled_set_font(const uint8_t* font);
void oled_update(void);
void oled_show_step(uint8_t step);

void oled_show_step_slave(uint8_t step);

// Mensagens rápidas de modo
void oled_show_message_centered(const char* text);

#endif