#ifndef DAC_H
#define DAC_H

#include <stdint.h>
#include <stdbool.h>
#include <cstddef>
#include "pinos.h"

// Configuração I2S para DAC UDA1334A
#define DAC_SAMPLE_RATE     44100
#define DAC_BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define DAC_CHANNELS        2  // Estéreo
#define DAC_BUFFER_SIZE     512   // Reduzido para mínima latência

// Funções básicas necessárias para o AU atual (tom inicial)
void dac_init(void);
void dac_play_buffer(const void* buffer, size_t bytes);




#endif