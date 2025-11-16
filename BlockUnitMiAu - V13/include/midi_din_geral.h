#ifndef MIDI_DIN_H
#define MIDI_DIN_H
#include <Arduino.h>
namespace midi {
    void begin();
    void loop();
    void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel);
    void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel);
    
    
    // Funções para modos simultâneos
    void ativar_modo_slave(bool ativo);
    
}
#endif