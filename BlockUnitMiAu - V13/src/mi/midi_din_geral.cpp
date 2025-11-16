#include "midi_din_geral.h"
#include "pinos.h"
#include "modo slave/midi_buffer_slave.h"
#include "modo master/master_internal.h"
#include "midi_ble.h"
#include "slots_sysex.h"

namespace midi {
 
    extern uint8_t modo_atual; 
    bool modo_slave_ativo = false;
    void begin() {
        Serial2.begin(31250, SERIAL_8N1, SLAVE_MIDI_IN, SLAVE_MIDI_OUT);
        Serial2.setTimeout(0); 
    }
    
    void loop() {
        // Ler todos os bytes disponíveis e distribuir para o modo ativo
        while (Serial2.available() > 0) {
            uint8_t byte = Serial2.read();
            
                    // Se gerador interno ativo, filtrar APENAS clock externo para evitar double clock
                    if (master_internal_is_active() && byte == 0xF8) {
                        continue;
                    }

                    // Processar SysEx antes de distribuir para o modo ativo
                    static uint8_t sysex_buffer[2048];
                    static uint16_t sysex_pos = 0;
                    static bool in_sysex = false;
                    
                    if (byte == 0xF0) {
                        // Início de SysEx
                        in_sysex = true;
                        sysex_pos = 0;
                        sysex_buffer[sysex_pos++] = byte;
                    } else if (in_sysex) {
                        if (byte == 0xF7) {
                            // Fim de SysEx
                            sysex_buffer[sysex_pos++] = byte;
                            // Processar SYSEX sempre (a função interna verifica se é válida)
                            slots_sysex_process_received_sysex(sysex_buffer, sysex_pos);
                            in_sysex = false;
                            sysex_pos = 0;
                        } else if (sysex_pos < sizeof(sysex_buffer) - 1) {
                            // Dados SysEx
                            sysex_buffer[sysex_pos++] = byte;
                        }
                    } else {
                        // Byte MIDI normal - distribuir para o modo ativo
                        if (modo_slave_ativo) {
                            slave_add_to_midi_buffer(byte);
                        }
                    }
        }

        // Consumir clock interno e injetar no parser no contexto do loop principal
        if (master_internal_is_active()) {
            uint8_t b;
            while (master_internal_pop_byte(&b)) {
                // Apenas injetar no pipeline interno (o OUT já é enviado pela task de clock)
                slave_add_to_midi_buffer(b);
            }
        }

        // Processar buffer apenas do modo ativo (otimização)
        if (modo_slave_ativo) {
            slave_process_midi_buffer();
        }
    }
    
    void ativar_modo_slave(bool ativo) {
        modo_slave_ativo = ativo;
        if (!ativo) {
            slave_clear_midi_buffer(); // Limpar buffer ao desativar
        }
    }
    
    void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
        uint8_t buffer[3] = {static_cast<uint8_t>(0x90 | (channel - 1)), note, velocity};
        Serial2.write(buffer, 3);
        midi_ble::sendNoteOn(note, velocity, channel);
    }
    
    void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
        uint8_t buffer[3] = {static_cast<uint8_t>(0x80 | (channel - 1)), note, velocity};
        Serial2.write(buffer, 3);
        midi_ble::sendNoteOff(note, velocity, channel);
    }
} 