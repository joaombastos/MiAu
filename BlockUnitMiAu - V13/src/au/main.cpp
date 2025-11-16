
#include <Arduino.h>
#include "tom_inicial/tom_inicial.h"
#include "player_wav/player_wav.h"
#include "gerador_ondas/gerador_ondas.h"
#include "midi_gate/midi_gate.h"
#include "dmachine/dmachine.h"
#include "slice/slice.h"
#include "synthesizer/synthesizer.h"
#include "au_telemetry.h"
#include "pinos.h"

void setup() {
    // Tom curto de teste (~500ms)
    tom_inicial_play(500);
    // Pausa de 500ms
    delay(500);
    // Tocar WAV do SD
    player_wav_play("/808/1.wav");
    // Inicia MIDI gate e botão do encoder
    midi_gate_begin();
    // Inicia dmachine
    dmachine_begin();
    // Inicia Slice
    slice_begin();
    // Inicia Sintetizador
    synthesizer_begin();
    // Inicia telemetria
    au_telemetry_begin();
    
}

void loop() {
    // Obter dados de telemetria para verificar modo atual
    AuTelemetryData* data = au_telemetry_get_data();
    
    // Executa modos baseado no modo atual - OTIMIZADO
    if (data && data->current_mode == AU_MODE_MENU) {
        // Modo menu - desativar todos os outros modos
        synthesizer_set_active(false);
        dmachine_set_active(false);
        slice_set_active(false);
        midi_gate_set_gate_mode(false);
        midi_gate_tick();
    } else if (data && data->current_mode == AU_MODE_SLICE) {
        // Modo Slice - desativar outros modos e ativar slice
        synthesizer_set_active(false);
        dmachine_set_active(false);
        static bool slice_initialized = false;
        if (!slice_initialized) {
            slice_set_active(true);
            midi_gate_set_gate_mode(false);
            slice_initialized = true;
        }
        midi_gate_tick();
        slice_tick();
    } else if (data && data->current_mode == AU_MODE_DRUM) {
        // Modo Drum - desativar outros modos e ativar dmachine
        synthesizer_set_active(false);
        slice_set_active(false);
        static bool drum_initialized = false;
        if (!drum_initialized) {
            dmachine_set_active(true);
            midi_gate_set_gate_mode(false);
            drum_initialized = true;
        }
        midi_gate_tick();
        dmachine_tick();
    } else if (data && data->current_mode == AU_MODE_SYNTH) {
        // Modo Synth - desativar outros modos e ativar synth
        dmachine_set_active(false);
        slice_set_active(false);
        static bool synth_initialized = false;
        if (!synth_initialized) {
            synthesizer_set_active(true);
            midi_gate_set_gate_mode(false);
            synth_initialized = true;
        }
        synthesizer_tick(); // MIDI já processado dentro
    } else if (data && data->current_mode == AU_MODE_GATE) {
        // Modo gate - desativar outros modos e ativar gate
        synthesizer_set_active(false);
        dmachine_set_active(false);
        slice_set_active(false);
        midi_gate_set_gate_mode(true); // Sempre ativar gate mode
        static bool gate_initialized = false;
        if (!gate_initialized) {
            gerador_ondas_begin();
            gate_initialized = true;
        }
        midi_gate_tick();
        gerador_ondas_tick();
    } else if (data && data->current_mode == AU_MODE_ONDAS) {
        // Modo Ondas - desativar outros modos e ativar ondas
        synthesizer_set_active(false);
        dmachine_set_active(false);
        slice_set_active(false);
        midi_gate_set_gate_mode(false);
        static bool ondas_initialized = false;
        if (!ondas_initialized) {
            gerador_ondas_begin();
            ondas_initialized = true;
        }
        midi_gate_tick();
        gerador_ondas_tick();
    }
    
    // Atualizar e enviar telemetria
    au_telemetry_tick();
}
