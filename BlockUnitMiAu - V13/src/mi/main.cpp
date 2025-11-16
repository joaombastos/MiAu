#include <Arduino.h>
#include "midi_din_geral.h"
#include "midi_ble.h"
#include "modo slave/midi_sequencer_slave.h"
#include "oled.h"
#include "controlador_botoes_geral.h"
#include "editor_parametros.h"
#include "led_mux_geral.h"
#include "modo slave/botoes_slave.h"
#include "prests_slave/prests_slave.h"
#include "modo slave/input_mode.h"
#include "modo master/master_internal.h"
#include "slots_sysex.h"
#include "au_overlay.h"
#include "mi_telemetry.h"

bool btn_last[8] = {1, 1, 1, 1, 1, 1, 1, 1};
extern uint8_t passo_atual;

void setup() {
    // Inicializar sistemas principais
    midi::begin();
    // BLE inicia desativado - ativar com botão Extra 5
    midi_ble::begin();
    presets_slave_init_ram();
    
    botoes_inicializar();
    oled_init();
    leds_inicializar();
    editor_inicializar();
    sequencer_init();
    botoes_ativar_modo_slave();
    input_mode_init();
    master_internal_init();
    slots_sysex_init();
    au_overlay_begin();
    mi_telemetry_begin();

    // Configurar comportamento BLE conforme modo no arranque (Slave por padrão no boot)
    midi_ble::set_master_tx_only(false);
    midi_ble::set_slave_rx_only(true);
    midi_ble::set_allow_note_tx_in_slave(true);
}

void loop() {
    botoes_processar_mudanca_modo();
    
    // Processar modo de slots SysEx se ativo
    if (slots_sysex_is_active()) {
        // No modo de slots, processar encoder e botões de passos
        extern void editor_processar_encoder(void);
        editor_processar_encoder();
        
        // Processar botões de passos
        botoes_ler_passos(btn_last);
        
        // Verificar timeout para saída automática
        extern void slots_sysex_check_timeout(void);
        slots_sysex_check_timeout();
        
        // Atualizar OLED
        slots_sysex_update_oled();
    } else {
        // Modo normal
        editor_processar_encoder();
        botoes_ler_passos(btn_last);
    }
    
    // Processar telemetria do AU
    mi_telemetry_tick();
    
    // Processar overlay AU se ativo (sempre por último para sobrepor tudo)
    if (au_overlay_is_active()) {
        au_overlay_tick();
        au_overlay_render_oled();
    }
    
    // Removido OLED per-frame para reduzir jitter; OLED é atualizado pelos módulos quando necessário
    
    midi::loop();
    midi_ble::loop();
    
    static uint8_t sync_counter = 0;
    if (++sync_counter >= 500) { // Verificação periódica leve
        extern void sequencer_verificar_sincronizacao(void);
        sequencer_verificar_sincronizacao();
        sync_counter = 0;
    }
}
