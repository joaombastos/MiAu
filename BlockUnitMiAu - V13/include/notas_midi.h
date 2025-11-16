#ifndef NOTAS_MIDI_H
#define NOTAS_MIDI_H
#include <stdint.h>

extern const char* NOTAS_NOMES[];

const char* nota_get_nome(uint8_t nota_midi);

#endif