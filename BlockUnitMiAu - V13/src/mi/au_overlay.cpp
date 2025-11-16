#include "au_overlay.h"
#include "pinos.h"
#include "oled.h"
#include "gerenciador_fontes_geral.h"
#include "strings_texto.h"
#include <U8g2lib.h>
#include <math.h>

static bool s_started = false;
static bool s_enabled = false;
static uint32_t s_lastFrameMs = 0;

// Estado atual recebido
static uint8_t s_modo = 0; // 0=ondas,1=gate,2=dmachine,3=slice
static uint8_t s_wave = 0; // GWF_*
static float s_hz[3] = {0,0,0};
static uint8_t s_banco = 1; // 1=808
static float s_dispHz[3] = {0,0,0};
static uint8_t s_menu_selection = 0; // 0-4 para menu principal

void au_overlay_begin(void) {
    if (s_started) return;
    Serial1.begin(115200, SERIAL_8N1, UART_RX_MI, -1);
    Serial1.setTimeout(0);
    s_started = true;
}

static inline bool readFrame() {
    // Protocolo: 0xAA, flags, modo, wave, 12 bytes Hz, banco, cks
    while (Serial1.available() > 0) {
        int b = Serial1.peek();
        if (b < 0) return false;
        if ((uint8_t)b == 0xAA) break;
        Serial1.read();
    }
    if (Serial1.available() < 1 + 1 + 1 + 1 + 1 + 12 + 1 + 1) return false; // 18 bytes total
    uint8_t buf[1 + 1 + 1 + 1 + 1 + 12 + 1 + 1];
    Serial1.readBytes(buf, sizeof(buf));
    // checksum
    uint8_t x = 0; for (size_t i=0;i<sizeof(buf)-1;++i) x ^= buf[i];
    if (x != buf[sizeof(buf)-1]) return false;
    uint8_t flags = buf[1];
    if ((flags & 0x01) != 0) { s_lastFrameMs = millis(); return true; }
    s_modo = buf[2]; s_wave = buf[3];
    s_menu_selection = buf[4]; // Menu selection
    for (int i=0;i<3;i++) {
        union { float f; uint8_t b[4]; } u; u.b[0]=buf[5+i*4+0]; u.b[1]=buf[5+i*4+1]; u.b[2]=buf[5+i*4+2]; u.b[3]=buf[5+i*4+3]; // +1 para menu_selection
        s_hz[i] = u.f;
    }
    s_banco = buf[5 + 12]; // +1 para menu_selection
    // Atualizar valores de exibição com estabilidade por quantização (10 Hz)
    const float kStepHz = 10.0f;
    for (int i=0;i<3;i++) {
        float inHz = s_hz[i];
        if (inHz > 4000.0f) inHz = 4000.0f;
        if (inHz < 0.0f) inHz = 0.0f;
        float qHz = roundf(inHz / kStepHz) * kStepHz;
        if (s_modo == 1 || s_modo == 2) { // ONDAS ou GATE
            if (inHz <= 1.0f) {
                // manter última
            } else {
                s_dispHz[i] = qHz;
            }
        } else if (s_modo == 0) {
            s_dispHz[i] = qHz;
        } else {
            // dmachine: não altera
        }
    }
    s_lastFrameMs = millis();
    return true;
}

static void renderOverlay();

void au_overlay_tick(void) {
    if (!s_started) return;
    if (!s_enabled) return;
    bool got = readFrame();
    if (!got) { return; }
    renderOverlay();
}

void au_overlay_toggle(void) {
    s_enabled = !s_enabled;
}

bool au_overlay_is_active(void) {
    return s_enabled;
}

static const char* modoStr(uint8_t m) {
    switch (m) { case 1: return STR_AU_MODO_ONDAS; case 2: return STR_AU_MODO_GATE; case 3: return STR_AU_MODO_DMACH; case 4: return STR_AU_MODO_SLICE; case 5: return STR_AU_MODO_SYNTH; default: return "MENU"; }
}
static const char* waveStr(uint8_t w) {
    switch (w) { case 0: return STR_AU_WAVE_SQR; case 1: return STR_AU_WAVE_SIN; case 2: return STR_AU_WAVE_TRI; case 3: return STR_AU_WAVE_SAW; case 4: return STR_AU_WAVE_NOTE; default: return "?"; }
}

static void renderOverlay() {
    oled_clear();
    fonte_set_tipo(FONTE_MONO_7PX);
    char line[24];
    
    // Se modo 0 (MENU), mostrar menu principal
    if (s_modo == 0) {
        oled_draw_str(32, 8, "AU OVERLAY");
        oled_draw_str(32, 16, "==========");
        
        const char* menu_items[] = {STR_AU_MODO_ONDAS, STR_AU_MODO_GATE, STR_AU_MODO_DMACH, STR_AU_MODO_SLICE, STR_AU_MODO_SYNTH};
        for (uint8_t i = 0; i < 5; i++) {
            int y = 24 + (i * 8);
            if (i == s_menu_selection) {
                oled_draw_str(16, y, ">");
            } else {
                oled_draw_str(16, y, " ");
            }
            oled_draw_str(24, y, menu_items[i]);
        }
    } else {
        // Modo específico ativo
        if (s_modo == 3) { // DRUM (dmachine)
            // Linha 1: Nome do banco (alterável com encoder)
            const char* bank_names[] = {"808", "CAPOE", "BRAZIL", "DRUM1", "LONG", "RECORD", "INSTRUM", "ELECTRO"};
            int bank_index = (int)s_hz[0]; // Banco atual do dmachine
            if (bank_index < 0 || bank_index >= 8) bank_index = 0;
            snprintf(line, sizeof(line), "BANCO: %s", bank_names[bank_index]);
            oled_draw_str(16, 8, line);
        } else if (s_modo == 4) { // SLICE
            // Linha 1: Nome do banco (alterável com encoder)
            const char* slice_bank_names[] = {"SLICE1", "SLICE2", "SLICE3", "SLICE4", "SLICE5", "SLICE6", "SLICE7", "SLICE8", 
                                             "SLICE9", "SLICE10", "SLICE11", "SLICE12", "SLICE13", "SLICE14", "SLICE15", "SLICE16"};
            int file_index = (int)s_hz[0]; // Ficheiro atual do slice
            if (file_index < 0 || file_index >= 16) file_index = 0;
            snprintf(line, sizeof(line), "BANCO: %s", slice_bank_names[file_index]);
            oled_draw_str(16, 8, line);
        } else if (s_modo == 5) { // SYNTH
            // Linha 1: Tipo de onda (alterável com encoder)
            const char* synth_wave_names[] = {"SINE", "SAW", "SQUARE", "TRIANGLE", "NOISE"};
            int wave_index = (int)s_hz[0]; // Tipo de onda atual do synth
            if (wave_index < 0 || wave_index >= 5) wave_index = 1; // Default SAW
            snprintf(line, sizeof(line), "ONDA: %s", synth_wave_names[wave_index]);
            oled_draw_str(16, 8, line);
        } else {
            snprintf(line, sizeof(line), "BANCO: %d", s_banco);
            oled_draw_str(16, 8, line);
        }
        snprintf(line, sizeof(line), "MODO: %s", modoStr(s_modo));
        oled_draw_str(16, 16, line);
        // Mostrar tipo de onda e frequências para ONDAS e GATE
        if (s_modo == 1 || s_modo == 2) { // ONDAS ou GATE
            snprintf(line, sizeof(line), "ONDA: %s", waveStr(s_wave));
            oled_draw_str(16, 24, line);
            snprintf(line, sizeof(line), "FREQ: %.0f %.0f %.0f", s_dispHz[0], s_dispHz[1], s_dispHz[2]);
            oled_draw_str(16, 32, line);
        } else if (s_modo == 3) { // DRUM (dmachine)
            // Mostrar dados específicos do dmachine
            // Linha 3: Apagar (não mostrar nada)
            // Linha 4: SAIR já está no final
        }
        // Botão SAIR
        oled_draw_str(16, 40, "SAIR: CLICK ENCODER");
    }
    oled_update();
}

void au_overlay_render_oled(void) {
    // Função vazia - renderOverlay() é chamada automaticamente
}

