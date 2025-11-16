#include "controlador_botoes_geral.h"
#include "pinos.h"
#include <Arduino.h>

const uint8_t mux_s[4] = {
    SLAVE_MUX_CD74HC4067_S0,
    SLAVE_MUX_CD74HC4067_S1,
    SLAVE_MUX_CD74HC4067_S2,
    SLAVE_MUX_CD74HC4067_S3
};
void botoes_inicializar(void) {
    for (int i = 0; i < 4; i++) {
        pinMode(mux_s[i], OUTPUT);
        digitalWrite(mux_s[i], LOW);
    }
    pinMode(MUX_SIG, INPUT_PULLUP);
}

bool botoes_ler_passo(uint8_t passo) {
    if (passo < 1 || passo > 8) return false;
    
    uint8_t canal = passo - 1; // Passos 1-8 correspondem aos canais 0-7
    for (int i = 0; i < 4; i++) {
        digitalWrite(mux_s[i], (canal >> i) & 0x01);
    }
    delayMicroseconds(5);
    return !digitalRead(MUX_SIG); // Retorna true se pressionado (LOW)
} 