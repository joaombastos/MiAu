#include "midi_gate/midi_gate.h"
#include "pinos.h"
#include <MIDI.h>
#include "synthesizer/synthesizer.h"

static bool s_initialized = false;
static bool s_gateMode = false;
static bool s_gateOpen[3] = {false, false, false};

// MIDI no modo OMNI
static MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDIin);

// Fila circular de eventos MIDI (gate) com timestamp
static constexpr size_t MIDI_EVT_QUEUE_SIZE = 32;
static MidiGateEvent s_evtQueue[MIDI_EVT_QUEUE_SIZE];
static volatile size_t s_evtHead = 0; // push
static volatile size_t s_evtTail = 0; // pop

static inline bool queue_is_empty() { return s_evtHead == s_evtTail; }
static inline bool queue_is_full() { return ((s_evtHead + 1) % MIDI_EVT_QUEUE_SIZE) == s_evtTail; }
static inline void queue_push(const MidiGateEvent& e) {
    if (queue_is_full()) { s_evtTail = (s_evtTail + 1) % MIDI_EVT_QUEUE_SIZE; }
    s_evtQueue[s_evtHead] = e;
    s_evtHead = (s_evtHead + 1) % MIDI_EVT_QUEUE_SIZE;
}
static inline bool queue_peek(MidiGateEvent* out) {
    if (queue_is_empty()) return false;
    *out = s_evtQueue[s_evtTail];
    return true;
}
static inline void queue_pop() {
    if (!queue_is_empty()) s_evtTail = (s_evtTail + 1) % MIDI_EVT_QUEUE_SIZE;
}

static inline int channelToOscIndex(uint8_t ch) {
    // CH3->0, CH4->1, CH5->2 (MIDI canais são 1..16)
    if (ch == 3) return 0;
    if (ch == 4) return 1;
    if (ch == 5) return 2;
    return -1;
}

void midi_gate_begin() {
    if (s_initialized) return;
    
    // Configura UART para MIDI IN do AU
    Serial1.begin(31250, SERIAL_8N1, AU_MIDI_IN, -1);
    MIDIin.begin(MIDI_CHANNEL_OMNI);
    
    s_initialized = true;
}

void midi_gate_tick() {
    if (!s_initialized) return;

    // Processa mensagens MIDI
    while (MIDIin.read()) {
        uint8_t type = MIDIin.getType();
        uint8_t ch = MIDIin.getChannel();
        int idx = channelToOscIndex(ch);
        uint32_t ts = micros();
        
        // Processa MIDI para sintetizador (canal 1)
        if (synthesizer_is_active() && ch == 1) {
            if (type == midi::NoteOn) {
                uint8_t note = MIDIin.getData1();
                uint8_t velocity = MIDIin.getData2();
                if (velocity > 0) {
                    synthesizer_note_on(note, velocity);
                } else {
                    synthesizer_note_off(note);
                }
            } else if (type == midi::NoteOff) {
                synthesizer_note_off(MIDIin.getData1());
            }
        }
        
        if (type == midi::Stop) {
            // Fechar todos os gates em Stop
            s_gateOpen[0] = s_gateOpen[1] = s_gateOpen[2] = false;
            MidiGateEvent e{ts, ch, 0, 0, MIDI_GATE_EVT_STOP};
            queue_push(e);
            continue;
        }
        
        if (type == midi::NoteOn) {
            uint8_t velocity = MIDIin.getData2();
            MidiGateEvent e{ts, ch, MIDIin.getData1(), velocity,
                            (velocity == 0) ? MIDI_GATE_EVT_NOTE_OFF : MIDI_GATE_EVT_NOTE_ON};
            queue_push(e);
            
            // Atualiza gates dos osciladores (CH3..CH5)
            if (idx >= 0 && idx <= 2) {
                s_gateOpen[idx] = (velocity > 0);
            }
        } else if (type == midi::NoteOff) {
            MidiGateEvent e{ts, ch, MIDIin.getData1(), MIDIin.getData2(), MIDI_GATE_EVT_NOTE_OFF};
            queue_push(e);
            if (idx >= 0 && idx <= 2) s_gateOpen[idx] = false;
        }
    }
    
    // Atualiza pots do sintetizador
    static uint32_t lastPotUpdate = 0;
    uint32_t potNow = millis();
    
    if (synthesizer_is_active() && (potNow - lastPotUpdate) > 50) {
        synthesizer_update_from_pots();
        lastPotUpdate = potNow;
    }
}

bool midi_gate_is_gate_mode_enabled() {
    return s_gateMode;
}

void midi_gate_set_gate_mode(bool enabled) {
    s_gateMode = enabled;
}

bool midi_gate_is_open(int oscIndex) {
    if (oscIndex < 0 || oscIndex > 2) return false;
    return s_gateOpen[oscIndex];
}

bool midi_gate_peek_event(MidiGateEvent* outEvent) {
    return queue_peek(outEvent);
}

void midi_gate_pop_event() {
    queue_pop();
}
