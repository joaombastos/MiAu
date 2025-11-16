#ifndef PLAYER_WAV_H
#define PLAYER_WAV_H

#include <Arduino.h>

// Inicializa SD e toca um ficheiro WAV 16-bit PCM (mono ou estéreo)
// Retorna true em sucesso
bool player_wav_play(const char* filepath);

#endif

