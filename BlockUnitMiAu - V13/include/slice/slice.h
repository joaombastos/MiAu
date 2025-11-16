#ifndef SLICE_H
#define SLICE_H

#include <Arduino.h>

// Inicia estado do modo Slice (não carrega arquivo ainda)
void slice_begin();

// Liga/Desliga modo Slice. Ao ligar, tenta carregar /slice/slice1.wav
void slice_set_active(bool enabled);

// Estado atual do modo Slice
bool slice_is_active();

// Processa eventos MIDI CH2 e reproduz slices; chamar apenas quando ativo
void slice_tick();

// Gestão de ficheiros: 0..15 => "/slice/1.wav".."/slice/16.wav"
void slice_set_file_index(int index);
int slice_get_file_index();

#endif


