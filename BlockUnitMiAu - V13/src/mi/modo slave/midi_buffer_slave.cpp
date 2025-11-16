#include "modo slave/midi_sequencer_slave.h"
#include "modo slave/input_mode.h"
#include "pinos.h"
#include <Arduino.h>
#include "midi_ble.h"

#define SLAVE_MIDI_BUFFER_SIZE 256
static uint8_t slave_midi_buffer[SLAVE_MIDI_BUFFER_SIZE];
static uint8_t slave_buffer_head = 0;
static uint8_t slave_buffer_tail = 0;
static uint8_t slave_buffer_count = 0;

static uint8_t slave_midi_state = 0;
static uint8_t slave_midi_command = 0;
static uint8_t slave_midi_data1 = 0;
static uint8_t slave_midi_data2 = 0;
static uint8_t slave_midi_data_count = 0;

void slave_parse_midi_byte(uint8_t byte);
void slave_process_midi_command(void);

bool sequencer_running = false;

void slave_add_to_midi_buffer(uint8_t byte) {

    

    if (slave_buffer_count < SLAVE_MIDI_BUFFER_SIZE) {
        slave_midi_buffer[slave_buffer_head] = byte;
        slave_buffer_head = (slave_buffer_head + 1) % SLAVE_MIDI_BUFFER_SIZE;
        slave_buffer_count++;
    }

    else {
        slave_midi_buffer[slave_buffer_head] = byte;
        slave_buffer_head = (slave_buffer_head + 1) % SLAVE_MIDI_BUFFER_SIZE;
        slave_buffer_tail = (slave_buffer_tail + 1) % SLAVE_MIDI_BUFFER_SIZE;
    }
}

void slave_process_midi_buffer(void) {
    while (slave_buffer_count > 0) {
        uint8_t byte = slave_midi_buffer[slave_buffer_tail];
        slave_buffer_tail = (slave_buffer_tail + 1) % SLAVE_MIDI_BUFFER_SIZE;
        slave_buffer_count--;
        

        slave_parse_midi_byte(byte);
    }
}

void slave_parse_midi_byte(uint8_t byte) {

    if (byte >= 0xF8) {
        switch (byte) {
            case 0xF8: // Timing Clock

                if (sequencer_running) {
                    extern void sequencer_tick_otimizado(void);
                    sequencer_tick_otimizado();
                    // Clock para o Modo Input (segue o mesmo gating do sequencer)
                    input_mode_on_clock_tick();
                }
                break;
            case 0xFA: // Start
                sequencer_running = true;
                extern void sequencer_reset_position(void);
                extern void sequencer_reset_all_counters(void);
                sequencer_reset_position();
                sequencer_reset_all_counters();
                input_mode_on_transport_start();
                break;
            case 0xFB: // Continue
                sequencer_running = true;
                    // Continuar do ponto atual sem resetar contadores
                    // Manter posição atual dos bancos
                input_mode_on_transport_continue();
                break;
            case 0xFC: // Stop
                sequencer_running = false;
                extern void sequencer_clear_all_midi_notes(void);
                sequencer_clear_all_midi_notes();
                input_mode_on_transport_stop();
                break;
        }
        slave_midi_state = 0;
        return;
    }
    
    if (byte >= 0x80 && byte <= 0xEF) {
        slave_midi_command = byte;
        slave_midi_data_count = 0;
        slave_midi_state = 1;
        return;
    }
    
    if (slave_midi_state == 1) {
        slave_midi_data1 = byte;
        slave_midi_data_count = 1;
        
        if ((slave_midi_command & 0xF0) == 0xC0 || 
            (slave_midi_command & 0xF0) == 0xD0) {
            slave_process_midi_command();
            slave_midi_state = 0;
        } else {
            slave_midi_state = 2;
        }
        return;
    }
    
    if (slave_midi_state == 2) {
        slave_midi_data2 = byte;
        slave_midi_data_count = 2;
        slave_process_midi_command();
        slave_midi_state = 0;
    }
}

void slave_process_midi_command(void) {
    uint8_t channel = (slave_midi_command & 0x0F) + 1;
    uint8_t command = slave_midi_command & 0xF0;
    
    switch (command) {
        case 0x80: // Note Off
            // Encaminhar para Modo Input (gravação)
            input_mode_on_midi(static_cast<uint8_t>(0x80 | ((channel - 1) & 0x0F)), channel, slave_midi_data1, slave_midi_data2);
            break;
        case 0x90: // Note On
            if (slave_midi_data2 > 0) {
                input_mode_on_midi(static_cast<uint8_t>(0x90 | ((channel - 1) & 0x0F)), channel, slave_midi_data1, slave_midi_data2);
            } else {
                input_mode_on_midi(static_cast<uint8_t>(0x90 | ((channel - 1) & 0x0F)), channel, slave_midi_data1, slave_midi_data2);
            }
            break;
        case 0xA0: // Polyphonic Key Pressure
            // Processar Polyphonic Key Pressure
            break;
        case 0xB0: // Control Change
            input_mode_on_midi(static_cast<uint8_t>(0xB0 | ((channel - 1) & 0x0F)), channel, slave_midi_data1, slave_midi_data2);
            break;
        case 0xC0: // Program Change
            input_mode_on_midi(static_cast<uint8_t>(0xC0 | ((channel - 1) & 0x0F)), channel, slave_midi_data1, 0);
            break;
        case 0xD0: // Channel Pressure
            // Processar Channel Pressure
            break;
        case 0xE0: // Pitch Bend
            // Processar Pitch Bend
            break;
    }
}

uint8_t slave_get_buffer_count(void) {
    return slave_buffer_count;
}

void slave_clear_midi_buffer(void) {
    slave_buffer_head = 0;
    slave_buffer_tail = 0;
    slave_buffer_count = 0;
    slave_midi_state = 0;
} 