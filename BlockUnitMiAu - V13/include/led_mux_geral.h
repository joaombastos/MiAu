#ifndef LED_MUX_H
#define LED_MUX_H
#include <Arduino.h>
class LedMux {
public:
    LedMux(uint8_t pinSCK, uint8_t pinSTR, uint8_t pinDATA);
    void begin();
    void write(uint32_t value);
    void setLed(uint8_t index, bool on);
    void clear();
private:
    uint8_t _pinSCK, _pinSTR, _pinDATA;
    uint32_t _state;
};
extern void leds_inicializar(void);
extern void leds_limpar_piscar_modo(void);
extern void leds_limpar_todos_passos(void);

    // Definições
#define TICKS_MAXIMO_PISCAR 48
#define TICKS_POR_PASSO_PADRAO 24

extern LedMux ledMux;
extern bool passos_piscando[8];
extern uint8_t ticks_piscar[8][48];

#endif