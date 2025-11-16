#include "midi_ble.h"
#include <Control_Surface.h>
#include <MIDI_Interfaces/MIDI_Callbacks.hpp>
#include "modo slave/midi_buffer_slave.h"

namespace midi_ble {

static cs::BluetoothMIDI_Interface s_ble; // Default device name
static volatile bool s_master_tx_only = false;
static volatile bool s_slave_rx_only = false;
static volatile bool s_allow_note_tx_in_slave = false;
static volatile bool s_enabled = false;

void begin() {
    Control_Surface.begin();
}

void loop() {
    if (s_enabled) {
        Control_Surface.loop();
    }
}

void set_enabled(bool enabled) { s_enabled = enabled; }
bool is_enabled() { return s_enabled; }

void set_master_tx_only(bool enabled) { s_master_tx_only = enabled; }
void set_slave_rx_only(bool enabled) { s_slave_rx_only = enabled; }
void set_allow_note_tx_in_slave(bool enabled) { s_allow_note_tx_in_slave = enabled; }

// TX helpers (no-op if RX-only is set)
void sendClock()           { if (s_enabled && !s_slave_rx_only) s_ble.sendTimingClock(); }
void sendStart()           { if (s_enabled && !s_slave_rx_only) s_ble.sendStart(); }
void sendStop()            { if (s_enabled && !s_slave_rx_only) s_ble.sendStop(); }
void sendContinue()        { if (s_enabled && !s_slave_rx_only) s_ble.sendContinue(); }
void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
    if (!s_enabled) return;
    if (s_slave_rx_only && !s_allow_note_tx_in_slave) return;
    s_ble.sendNoteOn(cs::MIDIAddress(note, cs::Channel::createChannel(channel)), velocity);
}
void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
    if (!s_enabled) return;
    if (s_slave_rx_only && !s_allow_note_tx_in_slave) return;
    s_ble.sendNoteOff(cs::MIDIAddress(note, cs::Channel::createChannel(channel)), velocity);
}
void sendCC(uint8_t control, uint8_t value, uint8_t channel) {
    if (!s_enabled) return;
    if (s_slave_rx_only) return;
    s_ble.sendControlChange(cs::MIDIAddress(control, cs::Channel::createChannel(channel)), value);
}
void sendPC(uint8_t program, uint8_t channel) {
    if (!s_enabled) return;
    if (s_slave_rx_only) return;
    s_ble.sendProgramChange(cs::MIDIChannelCable(cs::Channel::createChannel(channel)), program);
}
// Função SysEx removida - usar Serial2 (DIN) por agora

// Reception path (Slave): feed bytes into the existing slave buffer
// Use coarse-grained callbacks to translate to raw bytes.
struct BLEInputHandler : cs::MIDI_Callbacks {
    void onChannelMessage(cs::MIDI_Interface &, cs::ChannelMessage msg) override {
        if (!s_enabled || !s_slave_rx_only) return;
        extern void slave_add_to_midi_buffer(uint8_t byte);
        using MMT = cs::MIDIMessageType;
        uint8_t status = 0;
        switch (msg.getMessageType()) {
            case MMT::NoteOn:         status = 0x90; break;
            case MMT::NoteOff:        status = 0x80; break;
            case MMT::ControlChange:  status = 0xB0; break;
            case MMT::ProgramChange:  status = 0xC0; break;
            default: return; // ignore others
        }
        status |= msg.getChannel().getRaw();
        ::slave_add_to_midi_buffer(status);
        ::slave_add_to_midi_buffer(msg.getData1());
        // ProgramChange has no data2
        if (msg.getMessageType() != MMT::ProgramChange)
            ::slave_add_to_midi_buffer(msg.getData2());
    }
    void onRealTimeMessage(cs::MIDI_Interface &, cs::RealTimeMessage msg) override {
        if (!s_enabled || !s_slave_rx_only) return;
        extern void slave_add_to_midi_buffer(uint8_t byte);
        using MMT = cs::MIDIMessageType;
        switch (msg.getMessageType()) {
            case MMT::TimingClock: ::slave_add_to_midi_buffer(0xF8); break;
            case MMT::Start:       ::slave_add_to_midi_buffer(0xFA); break;
            case MMT::Continue:    ::slave_add_to_midi_buffer(0xFB); break;
            case MMT::Stop:        ::slave_add_to_midi_buffer(0xFC); break;
            default: break;
        }
    }
    void onSysExMessage(cs::MIDI_Interface &, cs::SysExMessage msg) override {
        if (!s_enabled || !s_slave_rx_only) return;
        // SysEx processing será implementado posteriormente
        // Por agora, apenas ignorar mensagens SysEx
    }
} s_input_handler;

struct BLERegisterCallbacks {
    BLERegisterCallbacks() { s_ble.setCallbacks(s_input_handler); }
} s_register;

} // namespace midi_ble


