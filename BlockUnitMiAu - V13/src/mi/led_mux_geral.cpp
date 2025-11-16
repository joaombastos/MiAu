#include "led_mux_geral.h"
#include "pinos.h"

// Declaração externa da função que já existe no sequenciador
extern uint8_t calcular_ticks_por_passo(uint8_t resolution);

LedMux::LedMux(uint8_t pinSCK, uint8_t pinSTR, uint8_t pinDATA)
    : _pinSCK(pinSCK), _pinSTR(pinSTR), _pinDATA(pinDATA), _state(0) {}

void LedMux::begin() {
    pinMode(_pinSCK, OUTPUT);
    pinMode(_pinSTR, OUTPUT);
    pinMode(_pinDATA, OUTPUT);
    clear();
}

void LedMux::write(uint32_t value) {
    _state = value & 0xFFFFFF;
    digitalWrite(_pinSTR, HIGH);
    digitalWrite(_pinSTR, LOW);
    
    for (int i = 23; i >= 0; i--) {
        digitalWrite(_pinSCK, LOW);
        digitalWrite(_pinDATA, (_state & (1UL << i)) ? HIGH : LOW);
        digitalWrite(_pinSCK, HIGH);
    }
    digitalWrite(_pinSTR, HIGH);
}

void LedMux::setLed(uint8_t index, bool on) {
    if (index > 23) return;
    if (on) {
        _state |= (1UL << index);
    } else {
        _state &= ~(1UL << index);
    }
    write(_state);
}

void LedMux::clear() {
    write(0);
}

LedMux ledMux(SLAVE_MUX_74HC595_LDSCK, SLAVE_MUX_74HC595_LDSTR, SLAVE_MUX_74HC595_DATA);

// Estados dos LEDs
bool passos_piscando[8] = {false};
// Definições necessárias para os buffers de piscar
#define TICKS_MAXIMO_PISCAR 48
#define TICKS_POR_PASSO_PADRAO 24
// Buffer de piscar utilizado por leds_limpar_piscar_modo (referenciado externamente)
uint8_t ticks_piscar[8][TICKS_MAXIMO_PISCAR] = {0};

void leds_inicializar(void) {
    ledMux.begin();
}

void leds_limpar_piscar_modo(void) {
    for (int i = 0; i < 8; i++) {
        passos_piscando[i] = false;
        // Usar 48 ticks como máximo (para suportar 1/2 = 48 ticks)
        for (int j = 0; j < TICKS_MAXIMO_PISCAR; j++) {
            ticks_piscar[i][j] = 0;
        }
    }
}

void leds_limpar_todos_passos(void) {
    uint32_t led_state = 0;
    ledMux.write(led_state);
}
