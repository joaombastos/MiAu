#include "oled.h"
#include <U8g2lib.h>
#include "modo preset/strings_texto_preset.h"
#include "modo preset/layout_preset.h"
#include "gerenciador_fontes_geral.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// Renderiza a tela de ajuda das slots do modo preset
void preset_ui_render_slots_help(void) {
    oled_clear();
    fonte_set_tipo(FONTE_MONO_PEQUENA);

    uint8_t t_w = u8g2.getStrWidth(PRESET_STR_SLOTS_TITLE);
    uint8_t t_x = (128 - t_w) / 2;
    oled_draw_str(t_x, PRESET_Y_TITLE, PRESET_STR_SLOTS_TITLE);

    uint8_t h1_w = u8g2.getStrWidth(PRESET_STR_SLOTS_HINT_BLINK_MUTE);
    uint8_t h1_x = (128 - h1_w) / 2;
    oled_draw_str(h1_x, PRESET_Y_HINT1, PRESET_STR_SLOTS_HINT_BLINK_MUTE);

    uint8_t h2_w = u8g2.getStrWidth(PRESET_STR_SLOTS_HINT_ON_PLAY);
    uint8_t h2_x = (128 - h2_w) / 2;
    oled_draw_str(h2_x, PRESET_Y_HINT2, PRESET_STR_SLOTS_HINT_ON_PLAY);

    uint8_t h3_w = u8g2.getStrWidth(PRESET_STR_STEP_ERASE_HINT);
    uint8_t h3_x = (128 - h3_w) / 2;
    oled_draw_str(h3_x, PRESET_Y_HINT3, PRESET_STR_STEP_ERASE_HINT);

    oled_update();
}


