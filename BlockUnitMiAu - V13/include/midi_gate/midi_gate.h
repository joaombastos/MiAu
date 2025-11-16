#ifndef MIDI_GATE_H
#define MIDI_GATE_H

#include <Arduino.h>

// Tipos de evento MIDI relevantes ao gate
enum MidiGateEventType : uint8_t {
    MIDI_GATE_EVT_NOTE_ON = 1,
    MIDI_GATE_EVT_NOTE_OFF = 2,
    MIDI_GATE_EVT_STOP = 3
};

struct MidiGateEvent {
    uint32_t timestamp_us; // micros() quando recebido
    uint8_t channel;       // 1..16
    uint8_t note;          // opcional (não usado para gate)
    uint8_t velocity;      // 0..127
    MidiGateEventType type;
};

// Inicia leitura MIDI e o botão do encoder para alternar o modo GATE
void midi_gate_begin();

// Processa eventos MIDI e o botão; chamar continuamente no loop()
void midi_gate_tick();

// Retorna true se o modo GATE estiver ativo (osc só tocam com Note ON)
bool midi_gate_is_gate_mode_enabled();

// Controla o modo GATE externamente
void midi_gate_set_gate_mode(bool enabled);

// Retorna true se o gate para o oscilador (0..2) estiver aberto
// Mapeamento: 0→CH3, 1→CH4, 2→CH5
bool midi_gate_is_open(int oscIndex);

// Fila de eventos: consulta topo sem remover. Retorna true se houver.
bool midi_gate_peek_event(MidiGateEvent* outEvent);

// Remove o evento do topo (chamar após peek/processamento)
void midi_gate_pop_event();

#endif


