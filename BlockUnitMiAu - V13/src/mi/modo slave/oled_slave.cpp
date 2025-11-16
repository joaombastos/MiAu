#include "oled.h"
#include "editor_parametros.h"
#include "modo slave/config_display_slave.h"
#include "strings_texto.h"
#include "gerenciador_fontes_geral.h"
#include "notas_midi.h"
#include <U8g2lib.h>
#include "pinos.h"
#include "prests_slave/prests_slave.h"
#include "modo preset/strings_texto_preset.h"
#include "modo preset/ui_oled_preset.h"
#include "modo slave/input_mode.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

void oled_init(void) {
    u8g2.begin();
}

void oled_clear(void) {
    u8g2.clearBuffer();
}

void oled_draw_str(uint8_t x, uint8_t y, const char* texto) {
    u8g2.drawStr(x, y, texto);
}

void oled_set_font(const uint8_t* font) {
    u8g2.setFont(font);
}

void oled_update(void) {
    u8g2.sendBuffer();
}

void oled_show_message_centered(const char* text) {
    oled_clear();
    // Usar fonte que suporta letras (evitar fontes somente numéricas)
    fonte_set_tipo(FONTE_MONO_MEDIA);
    uint8_t w = u8g2.getStrWidth(text);
    if (w > 120) {
        // Texto longo: reduzir fonte
        fonte_set_tipo(FONTE_MONO_PEQUENA);
        w = u8g2.getStrWidth(text);
    }
    int8_t ascent = u8g2.getAscent();
    int8_t descent = u8g2.getDescent();
    uint8_t h = (uint8_t)(ascent - descent);
    int16_t x_calc = (int16_t)(128 - w) / 2;
    if (x_calc < 0) x_calc = 0;
    uint8_t x = (uint8_t)x_calc;
    uint8_t y = (uint8_t)((64 - h) / 2 + ascent);
    oled_draw_str(x, y, text);
    oled_update();
    delay(DELAY_MENSAGEM_CENTRADA_MS); // manter por ~2s
}

void oled_show_step_slave(uint8_t step) {
    if (input_mode_is_active()) {
        input_mode_render_oled();
        return;
    }
    // Verificar se estamos no modo slots SysEx
    extern bool slots_sysex_is_active(void);
    if (slots_sysex_is_active()) {
        // No modo slots, não atualizar o OLED aqui
        return;
    }
    extern MenuStateSlave menu_state;
    oled_clear();
    
    fonte_set_tipo(FONTE_MONO_7PX);
    const char* modo_str = STR_MODO_SLAVE;
    oled_draw_str(POS_MODO_X_SLAVE, POS_MODO_Y_SLAVE, modo_str);
    
    extern StepParams step_params[8];
    extern uint8_t editing_step;
    extern uint8_t banco_atual;
    
    StepParams* params = &step_params[editing_step - 1];
    uint8_t banco_para_mostrar = banco_atual;
    
    fonte_set_tipo(FONTE_MONO_PEQUENA);
    char buffer[16];
    
    sprintf(buffer, "%s %s", STR_PARAM_NOTA, nota_get_nome(params->nota));
    oled_draw_str(POS_PARAM_NOTA_X_SLAVE, POS_PARAM_NOTA_Y_SLAVE, buffer);
    if (menu_state == MENU_EDIT_NOTE) {
        u8g2.drawFrame(POS_PARAM_NOTA_X_SLAVE - 1, POS_PARAM_NOTA_Y_SLAVE - 8, 
                      TAMANHO_FRAME_PARAMETRO_LARGURA_SLAVE, TAMANHO_FRAME_PARAMETRO_ALTURA_SLAVE);
    }
    
    sprintf(buffer, "%s %d", STR_PARAM_VELOCITY, params->velocity);
    oled_draw_str(POS_PARAM_VELOCITY_X_SLAVE, POS_PARAM_VELOCITY_Y_SLAVE, buffer);
    if (menu_state == MENU_EDIT_VELOCITY) {
        u8g2.drawFrame(POS_PARAM_VELOCITY_X_SLAVE - 1, POS_PARAM_VELOCITY_Y_SLAVE - 8, 
                      TAMANHO_FRAME_PARAMETRO_LARGURA_SLAVE, TAMANHO_FRAME_PARAMETRO_ALTURA_SLAVE);
    }
    
    sprintf(buffer, "%s %d", STR_PARAM_CANAL, params->channel);
    oled_draw_str(POS_PARAM_CANAL_X_SLAVE, POS_PARAM_CANAL_Y_SLAVE, buffer);
    if (menu_state == MENU_EDIT_CHANNEL) {
        u8g2.drawFrame(POS_PARAM_CANAL_X_SLAVE - 1, POS_PARAM_CANAL_Y_SLAVE - 8, 
                      TAMANHO_FRAME_CANAL_LARGURA_SLAVE, TAMANHO_FRAME_CANAL_ALTURA_SLAVE);
    }
    
    sprintf(buffer, "%s %d%%", STR_PARAM_LENGTH, params->length);
    oled_draw_str(POS_PARAM_LENGTH_X_SLAVE, POS_PARAM_LENGTH_Y_SLAVE, buffer);
    if (menu_state == MENU_EDIT_LENGTH) {
        u8g2.drawFrame(POS_PARAM_LENGTH_X_SLAVE - 1, POS_PARAM_LENGTH_Y_SLAVE - 8, 
                      TAMANHO_FRAME_LENGTH_LARGURA_SLAVE, TAMANHO_FRAME_LENGTH_ALTURA_SLAVE);
    }
    
    extern BancoConfig bancos[16];
    extern bool mudanca_resolucao_pendente;
    extern uint8_t nova_resolucao_pendente;
    extern uint8_t banco_alterado_pendente;
    
    uint8_t res_idx;
    if (mudanca_resolucao_pendente && banco_alterado_pendente == banco_para_mostrar) {
        res_idx = nova_resolucao_pendente;
    } else {
        res_idx = bancos[banco_para_mostrar - 1].resolution;
    }
    
    if (res_idx >= 7) res_idx = 1;
    
    sprintf(buffer, "%s %s", STR_PARAM_RESOLUTION, RESOLUCOES[res_idx]);
    oled_draw_str(POS_PARAM_RESOLUTION_X_SLAVE, POS_PARAM_RESOLUTION_Y_SLAVE, buffer);
    
    if (menu_state == MENU_EDIT_RESOLUTION) {
        u8g2.drawFrame(POS_FRAME_RESOLUCAO_X_SLAVE, POS_FRAME_RESOLUCAO_Y_SLAVE, 
                      TAMANHO_FRAME_RESOLUCAO_LARGURA_SLAVE, TAMANHO_FRAME_RESOLUCAO_ALTURA_SLAVE);
    }
    
    fonte_set_tipo(FONTE_MONO_PEQUENA);
    
    const char* banco_nome = "";
    
    fonte_set_tipo(FONTE_MONO_PEQUENA);
    
    char numero_banco[4];
    sprintf(numero_banco, "%d", banco_para_mostrar);
    
    if (menu_state == MENU_SELECT_STEP) {
        u8g2.drawFrame(POS_BANCO_QUADRADO_X_SLAVE, POS_BANCO_QUADRADO_Y_SLAVE, 
                      BANCO_QUADRADO_TAMANHO_SLAVE, BANCO_QUADRADO_TAMANHO_SLAVE);
    }
    
    fonte_set_tipo(FONTE_18PX);
    // Centralizar número do banco dentro do quadrado (centro geométrico)
    uint8_t num_w = u8g2.getStrWidth(numero_banco);
    uint8_t box_x = POS_BANCO_QUADRADO_X_SLAVE;
    uint8_t box_y = POS_BANCO_QUADRADO_Y_SLAVE;
    uint8_t box_w = BANCO_QUADRADO_TAMANHO_SLAVE;
    uint8_t box_h = BANCO_QUADRADO_TAMANHO_SLAVE;
    uint8_t num_x = box_x + (box_w - num_w) / 2;
    int8_t ascent_num = u8g2.getAscent();
    int8_t descent_num = u8g2.getDescent();
    uint8_t num_h = (uint8_t)(ascent_num - descent_num);
    uint8_t num_top = box_y + (box_h - num_h) / 2;
    uint8_t num_y = num_top + ascent_num;
    oled_draw_str(num_x, num_y, numero_banco);
    
    fonte_set_tipo(FONTE_MONO_PEQUENA);
    // Centralizar nome do banco dentro do quadrado (horizontalmente)
    uint8_t nome_w = u8g2.getStrWidth(banco_nome);
    uint8_t nome_x = box_x + (box_w - nome_w) / 2;
    oled_draw_str(nome_x, POS_BANCO_TEXTO_Y_SLAVE, banco_nome);
    
    oled_update();
} 

void oled_show_step(uint8_t step) {
    if (input_mode_is_active()) {
        input_mode_render_oled();
        return;
    }
    // Verificar se estamos no modo slots SysEx
    extern bool slots_sysex_is_active(void);
    if (slots_sysex_is_active()) {
        // No modo slots, não atualizar o OLED aqui
        return;
    }
    
    extern MenuStateSlave menu_state;
    // extern MenuPresetsState menu_presets; // REMOVIDO
    oled_clear();
    
    fonte_set_tipo(FONTE_MONO_7PX);
    
    // Mostrar título baseado no modo ativo
    extern bool master_internal_is_active(void);
    const char* modo_str;
    if (master_internal_is_active()) {
        modo_str = STR_MODO_MASTER;
    } else {
        modo_str = STR_MODO_SLAVE;
    }
    oled_draw_str(POS_MODO_X_SLAVE, POS_MODO_Y_SLAVE, modo_str);
    
    // Mostrar BPM no canto superior direito se estiver no modo master
    if (master_internal_is_active()) {
        extern uint16_t master_internal_get_bpm(void);
        char bpm_buffer[16];
        sprintf(bpm_buffer, STR_BPM_FORMAT, master_internal_get_bpm());
        
        // Calcular posição X para alinhar à direita
        uint8_t bpm_width = u8g2.getStrWidth(bpm_buffer);
        uint8_t bpm_x = 128 - bpm_width - 2; // 2 pixels de margem
        
        oled_draw_str(bpm_x, POS_MODO_Y_SLAVE, bpm_buffer);
        
        // Mostrar frame de seleção se estiver editando BPM
        if (menu_state == MENU_MASTER_BPM) {
            u8g2.drawFrame(bpm_x - 1, POS_MODO_Y_SLAVE - 8, 
                          bpm_width + 2, 10);
        }
    }
    
    extern StepParams step_params[8];
    extern uint8_t editing_step;
    extern uint8_t banco_atual;
    
    StepParams* params;
    uint8_t banco_para_mostrar;
    
  
        params = &step_params[editing_step - 1];
        banco_para_mostrar = banco_atual;

    extern bool editando_pagina_2;
    extern BancoConfig bancos[16];
    
    fonte_set_tipo(FONTE_MONO_PEQUENA);
    const char* passos_text;
    
    if (bancos[banco_para_mostrar - 1].duas_paginas) {
        passos_text = STR_16_PASSOS;
    } else {
        passos_text = STR_8_PASSOS;
    }
    
    oled_draw_str(POS_TEXTO_PASSOS_X_SLAVE, POS_TEXTO_PASSOS_Y_SLAVE, passos_text);
    
    fonte_set_tipo(FONTE_MONO_PEQUENA);
    char buffer[16];
    
    sprintf(buffer, "%s %s", STR_PARAM_NOTA, nota_get_nome(params->nota));
    oled_draw_str(POS_PARAM_NOTA_X_SLAVE, POS_PARAM_NOTA_Y_SLAVE, buffer);
    if (menu_state == MENU_EDIT_NOTE) {
        u8g2.drawFrame(POS_PARAM_NOTA_X_SLAVE - 1, POS_PARAM_NOTA_Y_SLAVE - 8, 
                      TAMANHO_FRAME_PARAMETRO_LARGURA_SLAVE, TAMANHO_FRAME_PARAMETRO_ALTURA_SLAVE);
    }
    
    sprintf(buffer, "%s %d", STR_PARAM_VELOCITY, params->velocity);
    oled_draw_str(POS_PARAM_VELOCITY_X_SLAVE, POS_PARAM_VELOCITY_Y_SLAVE, buffer);
    if (menu_state == MENU_EDIT_VELOCITY) {
        u8g2.drawFrame(POS_PARAM_VELOCITY_X_SLAVE - 1, POS_PARAM_VELOCITY_Y_SLAVE - 8, 
                      TAMANHO_FRAME_PARAMETRO_LARGURA_SLAVE, TAMANHO_FRAME_PARAMETRO_ALTURA_SLAVE);
    }
    
    sprintf(buffer, "%s %d", STR_PARAM_CANAL, params->channel);
    oled_draw_str(POS_PARAM_CANAL_X_SLAVE, POS_PARAM_CANAL_Y_SLAVE, buffer);
    if (menu_state == MENU_EDIT_CHANNEL) {
        u8g2.drawFrame(POS_PARAM_CANAL_X_SLAVE - 1, POS_PARAM_CANAL_Y_SLAVE - 8, 
                      TAMANHO_FRAME_CANAL_LARGURA_SLAVE, TAMANHO_FRAME_CANAL_ALTURA_SLAVE);
    }
    
    sprintf(buffer, "%s %d%%", STR_PARAM_LENGTH, params->length);
    oled_draw_str(POS_PARAM_LENGTH_X_SLAVE, POS_PARAM_LENGTH_Y_SLAVE, buffer);
    if (menu_state == MENU_EDIT_LENGTH) {
        u8g2.drawFrame(POS_PARAM_LENGTH_X_SLAVE - 1, POS_PARAM_LENGTH_Y_SLAVE - 8, 
                      TAMANHO_FRAME_LENGTH_LARGURA_SLAVE, TAMANHO_FRAME_LENGTH_ALTURA_SLAVE);
    }
    
    extern BancoConfig bancos[16];
    extern bool mudanca_resolucao_pendente;
    extern uint8_t nova_resolucao_pendente;
    extern uint8_t banco_alterado_pendente;
    
    uint8_t res_idx;
    if (mudanca_resolucao_pendente && banco_alterado_pendente == banco_para_mostrar) {
        res_idx = nova_resolucao_pendente;
    } else {
        res_idx = bancos[banco_para_mostrar - 1].resolution;
    }
    
        if (res_idx >= 7) res_idx = 1;
        
        sprintf(buffer, "%s %s", STR_PARAM_RESOLUTION, RESOLUCOES[res_idx]);
        oled_draw_str(POS_PARAM_RESOLUTION_X_SLAVE, POS_PARAM_RESOLUTION_Y_SLAVE, buffer);
        
        if (menu_state == MENU_EDIT_RESOLUTION) {
            u8g2.drawFrame(POS_FRAME_RESOLUCAO_X_SLAVE, POS_FRAME_RESOLUCAO_Y_SLAVE, 
                          TAMANHO_FRAME_RESOLUCAO_LARGURA_SLAVE, TAMANHO_FRAME_RESOLUCAO_ALTURA_SLAVE);
        }
    
    fonte_set_tipo(FONTE_MONO_PEQUENA);
    
    const char* banco_nome = "";
    
    fonte_set_tipo(FONTE_MONO_PEQUENA);
    
    char numero_banco[4];
    sprintf(numero_banco, "%d", banco_para_mostrar);
    
    if (menu_state == MENU_SELECT_STEP) {
        u8g2.drawFrame(POS_BANCO_QUADRADO_X_SLAVE, POS_BANCO_QUADRADO_Y_SLAVE, 
                      BANCO_QUADRADO_TAMANHO_SLAVE, BANCO_QUADRADO_TAMANHO_SLAVE);
    }
    
    fonte_set_tipo(FONTE_18PX);
    // Centralizar número do banco dentro do quadrado (centro geométrico)
    uint8_t num_w = u8g2.getStrWidth(numero_banco);
    uint8_t box_x = POS_BANCO_QUADRADO_X_SLAVE;
    uint8_t box_y = POS_BANCO_QUADRADO_Y_SLAVE;
    uint8_t box_w = BANCO_QUADRADO_TAMANHO_SLAVE;
    uint8_t box_h = BANCO_QUADRADO_TAMANHO_SLAVE;
    uint8_t num_x = box_x + (box_w - num_w) / 2;
    int8_t ascent_num = u8g2.getAscent();
    int8_t descent_num = u8g2.getDescent();
    uint8_t num_h = (uint8_t)(ascent_num - descent_num);
    uint8_t num_top = box_y + (box_h - num_h) / 2;
    uint8_t num_y = num_top + ascent_num;
    oled_draw_str(num_x, num_y, numero_banco);
    
    fonte_set_tipo(FONTE_MONO_PEQUENA);
    oled_draw_str(POS_BANCO_TEXTO_X_SLAVE, POS_BANCO_TEXTO_Y_SLAVE, banco_nome);
    
    // Durante saída sincronizada, manter tela de ajuda de slots
    if (presets_exit_is_syncing()) { preset_ui_render_slots_help(); return; }

    // Em modo slots: mostrar ajuda (menu de presets removido)
    if (presets_slots_is_edit_mode()) { preset_ui_render_slots_help(); return; }

    oled_update();
} 