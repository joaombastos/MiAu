#ifndef MIDI_BUFFER_SLAVE_H
#define MIDI_BUFFER_SLAVE_H

#include <Arduino.h>

void slave_add_to_midi_buffer(uint8_t byte);
void slave_process_midi_buffer(void);
void slave_parse_midi_byte(uint8_t byte);
void slave_process_midi_command(void);

void slave_clear_midi_buffer(void);

extern bool sequencer_running;

#endif 