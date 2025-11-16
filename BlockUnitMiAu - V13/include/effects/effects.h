#ifndef EFFECTS_H
#define EFFECTS_H

#include <Arduino.h>

// Inicializa módulo de efeitos (usa PSRAM quando disponível)
void effects_begin(uint32_t sample_rate_hz);

// Ativa/desativa delay globalmente
void effects_enable_delay(bool enabled);

// Define mix do delay (0.0 .. 1.0)
void effects_set_delay_mix(float mix);

// Define tempo do delay em milissegundos (ajustado internamente ao máximo suportado)
void effects_set_delay_time_ms(uint32_t time_ms);

// Define feedback do delay (0.0 .. 0.95 aprox.)
void effects_set_delay_feedback(float feedback);

// Ativa/desativa reverb globalmente
void effects_enable_reverb(bool enabled);

// Define mix do reverb (0.0 .. 1.0)
void effects_set_reverb_mix(float mix);

// Ativa/desativa chorus globalmente
void effects_enable_chorus(bool enabled);

// Define mix do chorus (0.0 .. 1.0)
void effects_set_chorus_mix(float mix);

// Ativa/desativa flanger globalmente
void effects_enable_flanger(bool enabled);

// Define mix do flanger (0.0 .. 1.0)
void effects_set_flanger_mix(float mix);

// Processa um bloco estéreo intercalado (L,R,L,R,...) com 'frames' amostras
void effects_process_block(int16_t* interleaved_lr, int frames);

#endif


