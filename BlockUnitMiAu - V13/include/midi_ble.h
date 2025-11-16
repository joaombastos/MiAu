#ifndef MIDI_BLE_H
#define MIDI_BLE_H

#include <Arduino.h>

namespace midi_ble {

// Initialize BLE MIDI (always-on, advertises on boot)
void begin();

// Service loop; call each iteration
void loop();

// Global enable/disable
void set_enabled(bool enabled);
bool is_enabled();

// Enable or disable TX/RX behavior according to current role
// master_tx_only: when true, send only, no receiving
// slave_rx_only: when true, receive only, no sending
void set_master_tx_only(bool enabled);
void set_slave_rx_only(bool enabled);
// Allow sending only NoteOn/Off while in Slave (keep other TX blocked)
void set_allow_note_tx_in_slave(bool enabled);

// Transmit helpers used in Master mode (duplicated alongside DIN)
void sendClock();           // 0xF8
void sendStart();           // 0xFA
void sendStop();            // 0xFC
void sendContinue();        // 0xFB
void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel);
void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel);
void sendCC(uint8_t control, uint8_t value, uint8_t channel);
void sendPC(uint8_t program, uint8_t channel);
// sendSysEx removido - usar Serial2 (DIN) por agora

}

#endif


