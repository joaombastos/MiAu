#include "slots_sysex.h"
#include "oled.h"
#include "gerenciador_fontes_geral.h"
#include "prests_slave/prests_slave.h"
#include "midi_din_geral.h"
#include "midi_ble.h"
#include <Arduino.h>

// Estado do menu
static slots_menu_state_t s_menu_state = {
    .mode = SLOTS_MODE_NONE,
    .selected_slot = 0,
    .phase = SLOTS_PHASE_SELECT_ACTION,
    .active = false
};

// Buffer para SysEx
static uint8_t s_sysex_buffer[2048];
static const uint8_t SYSEX_MANUFACTURER_ID = 0x7D; // Non-commercial

// Timeout para saída automática
static unsigned long s_last_slot_received = 0;
static const unsigned long SLOT_TIMEOUT_MS = 3000; // 3 segundos

void slots_sysex_init(void) {
    s_menu_state.mode = SLOTS_MODE_NONE;
    s_menu_state.selected_slot = 0;
    s_menu_state.phase = SLOTS_PHASE_SELECT_ACTION;
    s_menu_state.active = false;
}

void slots_sysex_enter_mode(void) {
    // Só funciona no modo Master
    extern bool master_internal_is_active(void);
    if (!master_internal_is_active()) return;
    
    // Parar o gerador interno de clock temporariamente
    extern void master_internal_pause_clock(void);
    master_internal_pause_clock();
    
    // START/STOP MIDI simples - para tudo
    Serial2.write(0xFC); // MIDI STOP DIN
    Serial2.flush();
    midi_ble::sendStop(); // MIDI STOP BLE
    
    // Ativar modo slots SYSEX
    s_menu_state.active = true;
    s_menu_state.phase = SLOTS_PHASE_SELECT_ACTION;
    s_menu_state.mode = SLOTS_MODE_NONE;
    s_menu_state.selected_slot = 0;
}

void slots_sysex_exit_mode(void) {
    if (!s_menu_state.active) return;
    
    // Mostrar mensagem de confirmação
    oled_clear();
    fonte_set_tipo(FONTE_MONO_PEQUENA);
    oled_draw_str(0, 10, "SLOTS CARREGADAS");
    oled_draw_str(0, 25, "COM SUCESSO!");
    oled_draw_str(0, 40, "Voltando ao");
    oled_draw_str(0, 55, "modo master...");
    oled_update();
    
    // Pequena pausa para mostrar a mensagem
    delay(2000);
    
    // Retomar clock interno se estivermos no modo master
    extern bool master_internal_is_active(void);
    extern void master_internal_resume_clock(void);
    if (master_internal_is_active()) {
        master_internal_resume_clock();
    }
    
    // Enviar MIDI START
    slots_sysex_send_midi_start();
    
    // Desativar modo
    s_menu_state.active = false;
    s_menu_state.phase = SLOTS_PHASE_SELECT_ACTION;
    s_menu_state.mode = SLOTS_MODE_NONE;
    s_menu_state.selected_slot = 0;
    
    // Limpar OLED e mostrar modo master
    oled_clear();
    fonte_set_tipo(FONTE_MONO_PEQUENA);
    oled_draw_str(0, 10, "MODO MASTER");
    oled_draw_str(0, 25, "Slots disponiveis");
    oled_draw_str(0, 40, "para usar!");
    oled_update();
    
    // Pequena pausa e depois limpar para voltar ao normal
    delay(1500);
    oled_clear();
    oled_update();
}

bool slots_sysex_is_active(void) {
    return s_menu_state.active;
}

void slots_sysex_process_encoder(int direction) {
    if (!s_menu_state.active) return;
    
    switch (s_menu_state.phase) {
        case SLOTS_PHASE_SELECT_ACTION:
            // Alternar entre guardar e carregar
            if (direction > 0) {
                s_menu_state.mode = SLOTS_MODE_SAVE;
            } else {
                s_menu_state.mode = SLOTS_MODE_LOAD;
            }
            break;
            
        case SLOTS_PHASE_SELECT_SLOT:
            // Não usado mais
            break;
            
        case SLOTS_PHASE_CONFIRM:
            // Não usado mais
            break;
            
        case SLOTS_PHASE_EXECUTING:
            // Não fazer nada durante execução
            break;
    }
    
    slots_sysex_update_oled();
}

// Processar click do encoder
void slots_sysex_process_encoder_click(void) {
    if (!s_menu_state.active) return;
    
    switch (s_menu_state.phase) {
        case SLOTS_PHASE_SELECT_ACTION:
            // Confirmar ação e executar imediatamente
            s_menu_state.phase = SLOTS_PHASE_EXECUTING;
            slots_sysex_execute_action();
            break;
            
        case SLOTS_PHASE_SELECT_SLOT:
            // Não deveria chegar aqui
            break;
            
        case SLOTS_PHASE_CONFIRM:
            // Não deveria chegar aqui
            break;
            
        case SLOTS_PHASE_EXECUTING:
            // Durante execução, não fazer nada
            break;
    }
    
    slots_sysex_update_oled();
}

void slots_sysex_process_step_button(uint8_t step) {
    // No modo slots SysEx, os botões de passos não fazem nada
    // Tudo é controlado pelo encoder
    (void)step; // Evitar warning de parâmetro não usado
}

void slots_sysex_update_oled(void) {
    if (!s_menu_state.active) return;

    oled_clear();
    fonte_set_tipo(FONTE_MONO_PEQUENA);

    switch (s_menu_state.phase) {
        case SLOTS_PHASE_SELECT_ACTION:
            oled_draw_str(0, 10, "SLOTS SYSEX");
            oled_draw_str(0, 25, s_menu_state.mode == SLOTS_MODE_SAVE ? "GUARDAR TODOS" : "CARREGAR TODOS");
            oled_draw_str(0, 40, "Encoder: rotação");
            oled_draw_str(0, 55, "Click: executar");
            break;
            
        case SLOTS_PHASE_SELECT_SLOT:
            // Não usado mais
            break;
            
        case SLOTS_PHASE_EXECUTING:
            // Durante execução, mostrar mensagem apropriada
            if (s_menu_state.mode == SLOTS_MODE_LOAD) {
                oled_draw_str(0, 10, "AGUARDANDO");
                oled_draw_str(0, 25, "SYSEX...");
                oled_draw_str(0, 40, "Envie dados");
                oled_draw_str(0, 55, "para carregar");
            }
            break;
    }

    oled_update();
}

void slots_sysex_execute_action(void) {
    if (s_menu_state.mode == SLOTS_MODE_SAVE) {
        // Guardar - sempre enviar todos os slots
        slots_sysex_send_all_slots();
        
        // Sair automaticamente após envio
        delay(500); // Pequena pausa para garantir envio
        slots_sysex_exit_mode();
    } else {
        // Carregar - mostrar mensagem de aguardar
        oled_clear();
        fonte_set_tipo(FONTE_MONO_PEQUENA);
        oled_draw_str(0, 10, "AGUARDANDO");
        oled_draw_str(0, 25, "SYSEX...");
        oled_draw_str(0, 40, "Envie dados");
        oled_draw_str(0, 55, "Extra3 para sair");
        oled_update();
        
        // Não sair automaticamente - aguardar receção
        // A saída será feita quando receber SysEx válido
    }
}

void slots_sysex_send_slot(uint8_t slot_id) {
    if (slot_id >= 8) return;
    
    // Sempre enviar slot (mesmo que vazia)
    extern bool presets_slot_filled[8];

    // Limpar buffer
    memset(s_sysex_buffer, 0, sizeof(s_sysex_buffer));
    
    // Construir mensagem SysEx
    uint16_t pos = 0;
    s_sysex_buffer[pos++] = 0xF0; // SysEx start
    s_sysex_buffer[pos++] = SYSEX_MANUFACTURER_ID; // Manufacturer ID
    s_sysex_buffer[pos++] = 0x01; // Comando: guardar slot
    s_sysex_buffer[pos++] = slot_id; // ID do slot (0-7)
    s_sysex_buffer[pos++] = 0x00; // Byte reservado
    
    // Obter dados da slot RAM e codificar em formato compactado
    extern BancoConfig presets_ram_slots[8][16];
    
    // Codificar todos os 16 bancos da slot
    for (uint8_t banco = 0; banco < 16; banco++) {
        const BancoConfig& banco_config = presets_ram_slots[slot_id][banco];
        
        // Resolution (2 bytes - little endian)
        s_sysex_buffer[pos++] = banco_config.resolution & 0xFF;
        s_sysex_buffer[pos++] = (banco_config.resolution >> 8) & 0xFF;
        
        // PÁGINA 1: Passos ativos (8 bytes)
        for (uint8_t passo = 0; passo < 8; passo++) {
            s_sysex_buffer[pos++] = banco_config.passos_ativos[passo] ? 1 : 0;
        }
        
        // PÁGINA 1: Step params (8 × 5 bytes)
        for (uint8_t passo = 0; passo < 8; passo++) {
            s_sysex_buffer[pos++] = banco_config.step_params[passo].nota;
            s_sysex_buffer[pos++] = banco_config.step_params[passo].velocity;
            s_sysex_buffer[pos++] = banco_config.step_params[passo].channel;
            s_sysex_buffer[pos++] = banco_config.step_params[passo].length;
            s_sysex_buffer[pos++] = banco_config.step_params[passo].num_notas_extra;
        }
        
        // PÁGINA 2: Passos ativos (8 bytes)
        for (uint8_t passo = 0; passo < 8; passo++) {
            s_sysex_buffer[pos++] = banco_config.passos_ativos_pagina_2[passo] ? 1 : 0;
        }
        
        // PÁGINA 2: Step params (8 × 5 bytes)
        for (uint8_t passo = 0; passo < 8; passo++) {
            s_sysex_buffer[pos++] = banco_config.step_params_pagina_2[passo].nota;
            s_sysex_buffer[pos++] = banco_config.step_params_pagina_2[passo].velocity;
            s_sysex_buffer[pos++] = banco_config.step_params_pagina_2[passo].channel;
            s_sysex_buffer[pos++] = banco_config.step_params_pagina_2[passo].length;
            s_sysex_buffer[pos++] = banco_config.step_params_pagina_2[passo].num_notas_extra;
        }
    }
    
    s_sysex_buffer[pos++] = 0xF7; // SysEx end
    
    // Enviar via Serial2 (DIN)
    Serial2.write(s_sysex_buffer, pos);
    Serial2.flush(); // Garantir envio completo
    delay(10); // Pequena pausa para evitar corrupção
}

void slots_sysex_send_all_slots(void) {
    // Salvar banco atual antes de enviar para garantir dados atualizados
    extern void editor_salvar_banco_atual(void);
    editor_salvar_banco_atual();
    
    // Enviar cada slot separadamente
    for (uint8_t slot = 0; slot < 8; slot++) {
        slots_sysex_send_slot(slot);
        delay(200); // Pausa maior entre envios para evitar corrupção
    }
}

void slots_sysex_receive_slot(uint8_t slot_id, const uint8_t* data, uint16_t length) {
    extern BancoConfig presets_ram_slots[8][16];
    extern bool presets_slot_filled[8];
    
    if (slot_id == 8) {
        // Slot 8 = todas - carregar todas as 8 slots (processar 8 SysEx individuais)
        // Cada slot individual: 16 bancos × 49 bytes cada
        const uint16_t expected_length = 16 * (1 + 8 + 8 * 5); // 16 bancos × 49 bytes cada
        if (length < expected_length) {
            // Mostrar erro de tamanho
            oled_clear();
            fonte_set_tipo(FONTE_MONO_PEQUENA);
            oled_draw_str(0, 10, "ERRO TAMANHO!");
            char buf[32];
            snprintf(buf, sizeof(buf), "Esperado: %d", expected_length);
            oled_draw_str(0, 25, buf);
            snprintf(buf, sizeof(buf), "Recebido: %d", length);
            oled_draw_str(0, 40, buf);
            oled_update();
            delay(2000);
            return;
        }
        
        // Slot 8 não é usado mais - processar slots individuais
        oled_clear();
        fonte_set_tipo(FONTE_MONO_PEQUENA);
        oled_draw_str(0, 10, "SLOT 8 NAO USADO");
        oled_draw_str(0, 25, "Use slots 0-7");
        oled_update();
        delay(2000);
        return;
    } else {
        // Slot individual - processar dados SYX
        // Se não há dados suficientes, inicializar slot como vazia
        if (length < 10) { // Mínimo para ter algum dado
            // Inicializar slot como vazia
            for (uint8_t banco = 0; banco < 16; banco++) {
                BancoConfig& banco_config = presets_ram_slots[slot_id][banco];
                banco_config.resolution = 0;
                for (uint8_t passo = 0; passo < 8; passo++) {
                    banco_config.passos_ativos[passo] = false;
                    banco_config.step_params[passo] = {0, 0, 0, 0, 0};
                    banco_config.passos_ativos_pagina_2[passo] = false;
                    banco_config.step_params_pagina_2[passo] = {0, 0, 0, 0, 0};
                }
                banco_config.duas_paginas = false;
            }
            presets_slot_filled[slot_id] = false;
            return;
        }
        
        // Copiar dados do SYX para as slots (16 bancos, 2 páginas, 8 passos cada)
        uint16_t pos = 0;
        for (uint8_t banco = 0; banco < 16; banco++) {
            BancoConfig& banco_config = presets_ram_slots[slot_id][banco];
            
            // Resolution (2 bytes - little endian)
            if (pos + 1 < length) {
                banco_config.resolution = data[pos] | (data[pos + 1] << 8);
                pos += 2;
            } else {
                banco_config.resolution = 0;
                pos += 2;
            }
            
            // PÁGINA 1: Passos ativos (8 bytes)
            for (uint8_t passo = 0; passo < 8; passo++) {
                if (pos < length) {
                    banco_config.passos_ativos[passo] = (data[pos++] != 0);
                } else {
                    banco_config.passos_ativos[passo] = false;
                    pos++;
                }
            }
            
            // PÁGINA 1: Step params (8 × 5 bytes)
            for (uint8_t passo = 0; passo < 8; passo++) {
                if (pos < length) banco_config.step_params[passo].nota = data[pos++]; else { banco_config.step_params[passo].nota = 0; pos++; }
                if (pos < length) banco_config.step_params[passo].velocity = data[pos++]; else { banco_config.step_params[passo].velocity = 0; pos++; }
                if (pos < length) banco_config.step_params[passo].channel = data[pos++]; else { banco_config.step_params[passo].channel = 0; pos++; }
                if (pos < length) banco_config.step_params[passo].length = data[pos++]; else { banco_config.step_params[passo].length = 0; pos++; }
                if (pos < length) banco_config.step_params[passo].num_notas_extra = data[pos++]; else { banco_config.step_params[passo].num_notas_extra = 0; pos++; }
            }
            
            // PÁGINA 2: Passos ativos (8 bytes)
            for (uint8_t passo = 0; passo < 8; passo++) {
                if (pos < length) {
                    banco_config.passos_ativos_pagina_2[passo] = (data[pos++] != 0);
                } else {
                    banco_config.passos_ativos_pagina_2[passo] = false;
                    pos++;
                }
            }
            
            // PÁGINA 2: Step params (8 × 5 bytes)
            for (uint8_t passo = 0; passo < 8; passo++) {
                if (pos < length) banco_config.step_params_pagina_2[passo].nota = data[pos++]; else { banco_config.step_params_pagina_2[passo].nota = 0; pos++; }
                if (pos < length) banco_config.step_params_pagina_2[passo].velocity = data[pos++]; else { banco_config.step_params_pagina_2[passo].velocity = 0; pos++; }
                if (pos < length) banco_config.step_params_pagina_2[passo].channel = data[pos++]; else { banco_config.step_params_pagina_2[passo].channel = 0; pos++; }
                if (pos < length) banco_config.step_params_pagina_2[passo].length = data[pos++]; else { banco_config.step_params_pagina_2[passo].length = 0; pos++; }
                if (pos < length) banco_config.step_params_pagina_2[passo].num_notas_extra = data[pos++]; else { banco_config.step_params_pagina_2[passo].num_notas_extra = 0; pos++; }
            }
            
            banco_config.duas_paginas = true;
        }
        
        presets_slot_filled[slot_id] = true;
        
        // Marcar timestamp da última slot recebida
        s_last_slot_received = millis();
        
        // Mostrar confirmação de slot carregada
        oled_clear();
        fonte_set_tipo(FONTE_MONO_PEQUENA);
        oled_draw_str(0, 10, "SLOT CARREGADA!");
        char buf[32];
        snprintf(buf, sizeof(buf), "Slot %d OK", slot_id + 1);
        oled_draw_str(0, 25, buf);
        oled_draw_str(0, 40, "Aguardando mais...");
        oled_draw_str(0, 55, "Extra3 para sair");
        oled_update();
        
        // Não sair automaticamente - aguardar mais slots
        // Só sai se não receber mais slots por 3 segundos
    }
}

void slots_sysex_process_received_sysex(const uint8_t* data, uint16_t length) {
    if (length < 4) return;
    if (data[0] != 0xF0 || data[length-1] != 0xF7) return;
    if (data[1] != SYSEX_MANUFACTURER_ID) return;
    
    // Processar SYX sempre (modo SYX pode ser ativado automaticamente)
    
    if (data[2] == 0x01 || data[2] == 0x02) { // Comando: guardar (0x01) ou carregar (0x02) slot
        uint8_t slot_id = data[3];
        if (slot_id <= 7) { // 0-7 para slots individuais
            // Slot individual - processar diretamente
            // Calcular tamanho dos dados (sem cabeçalho F0 7D 01 XX e sem F7)
            uint16_t data_length = length - 5; // Remover F0 7D 01 XX e F7
            
            // Sempre processar slot, mesmo que vazia
            slots_sysex_receive_slot(slot_id, &data[4], data_length);
        } else if (slot_id == 8) {
            // Slot 8 não é usado mais
            oled_clear();
            fonte_set_tipo(FONTE_MONO_PEQUENA);
            oled_draw_str(0, 10, "SLOT 8 NAO USADO");
            oled_draw_str(0, 25, "Use slots 0-7");
            oled_update();
            delay(2000);
        }
    }
}

void slots_sysex_send_midi_stop(void) {
    Serial2.write(0xFC); // MIDI Stop DIN
    Serial2.flush(); // Garantir envio
    midi_ble::sendStop(); // MIDI Stop BLE/USB
}

void slots_sysex_send_midi_start(void) {
    Serial2.write(0xFA); // MIDI Start
}

void slots_sysex_check_timeout(void) {
    if (!s_menu_state.active) return;
    if (s_last_slot_received == 0) return; // Nenhuma slot recebida ainda
    
    unsigned long now = millis();
    if (now - s_last_slot_received >= SLOT_TIMEOUT_MS) {
        // Timeout atingido - sair do modo
        slots_sysex_exit_mode();
    }
}