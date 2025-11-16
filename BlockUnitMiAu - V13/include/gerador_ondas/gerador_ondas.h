#ifndef GERADOR_ONDAS_H
#define GERADOR_ONDAS_H

#include <Arduino.h>

// Inicia o gerador de 3 ondas quadradas controladas pelos 3 pots
void gerador_ondas_begin();

// Gera e envia um bloco de áudio; chamar continuamente no loop()
void gerador_ondas_tick();

// Tipos de onda
typedef enum {
    GWF_SQUARE = 0,
    GWF_SINE = 1,
    GWF_TRIANGLE = 2,
    GWF_SAW = 3,
    GWF_NOTE = 4 // modo especial: frequência por nota da escala
} GeradorWaveform;

// Define o tipo de onda global
void gerador_ondas_set_waveform(GeradorWaveform wf);

// Define o índice de nota (quando GWF_NOTE ativo). 0..N-1 dentro da tabela interna
void gerador_ondas_set_note_index(int idx);

// Retorna o tipo de onda atual
GeradorWaveform gerador_ondas_get_waveform();

// Copia as frequências atuais filtradas (Hz) para outHz[3]
void gerador_ondas_get_frequencies(float outHz[3]);

#endif


