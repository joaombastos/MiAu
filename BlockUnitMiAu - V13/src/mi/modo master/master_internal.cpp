#include <Arduino.h>
#include "modo master/master_internal.h"
#include "modo slave/midi_buffer_slave.h"
#include "midi_ble.h"
#include "oled.h"
#include "strings_texto.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Gerador interno de MIDI clock profissional (ESP32 hw timer + task)

static bool s_active = false;
static bool s_running = false; // sinônimo de active
static uint16_t s_bpm = 120;

// FreeRTOS e timer de hardware
static hw_timer_t* s_timer = NULL;
static SemaphoreHandle_t s_tick_semaphore = NULL;
static TaskHandle_t s_clock_task_handle = NULL;
static QueueHandle_t s_midi_queue = NULL; // fila thread-safe para bytes 0xFA/0xF8/0xFC

// ISR minimal: acorda a task para emitir tick
IRAM_ATTR static void onTimerISR() {
    BaseType_t hp = pdFALSE;
    if (s_tick_semaphore) xSemaphoreGiveFromISR(s_tick_semaphore, &hp);
    if (hp) portYIELD_FROM_ISR();
}

static void configure_timer_for_bpm(uint16_t bpm) {
    if (bpm < 40) bpm = 40;
    if (bpm > 300) bpm = 300;
    // Cálculo em inteiro para minimizar jitter
    uint64_t ticks_per_beat_us = (uint64_t)60000000ULL / (uint64_t)bpm; // micros por beat
    uint64_t ticks_per_clock_us = ticks_per_beat_us / 24ULL;            // 24 PPQN

    if (!s_timer) {
        s_timer = timerBegin(1, 80, true); // 80MHz/80 = 1MHz (1 tick = 1us)
        timerAttachInterrupt(s_timer, &onTimerISR, true);
    }
    timerAlarmWrite(s_timer, (uint64_t)ticks_per_clock_us, true);
}

// Task que injeta clock no pipeline slave
static void clock_task(void* pv) {
    for (;;) {
        if (xSemaphoreTake(s_tick_semaphore, portMAX_DELAY) == pdTRUE) {
            if (s_active && s_running) {
                // 1) DIN OUT: timing preciso diretamente da task de clock (minimizar trabalho)
                Serial2.write((uint8_t)0xF8);
                midi_ble::sendClock();
                // 2) Interno: enfileirar para o loop principal injetar no parser
                if (s_midi_queue) {
                    uint8_t b = 0xF8;
                    xQueueSend(s_midi_queue, &b, 0);
                }
            }
        }
    }
}

void master_internal_init(void) {
    if (!s_tick_semaphore) {
        s_tick_semaphore = xSemaphoreCreateBinary();
    }
    if (!s_midi_queue) {
        s_midi_queue = xQueueCreate(128, sizeof(uint8_t));
    }
}

void master_internal_activate(void) {
    if (s_active) return;
    s_active = true;
    s_running = true;

    // BLE: Master = TX-only (não receba nada por BLE)
    midi_ble::set_master_tx_only(true);
    midi_ble::set_slave_rx_only(false);
    midi_ble::set_allow_note_tx_in_slave(false);

    // Criar task se ainda não existir
    if (!s_clock_task_handle) {
        xTaskCreatePinnedToCore(
            clock_task,
            "IntMidiClock",
            4096,
            NULL,
            configMAX_PRIORITIES - 1,
            &s_clock_task_handle,
            0
        );
    }

    // Configurar e iniciar timer
    configure_timer_for_bpm(s_bpm);
    timerAlarmEnable(s_timer);

    // Transporte: START imediato no DIN OUT
    Serial2.write((uint8_t)0xFA);
    midi_ble::sendStart();
    // E enfileirar para o pipeline interno
    if (s_midi_queue) { uint8_t b = 0xFA; xQueueSend(s_midi_queue, &b, 0); }

}

void master_internal_deactivate(void) {
    if (!s_active) return;

    // Transporte: STOP imediato no DIN OUT
    Serial2.write((uint8_t)0xFC);
    midi_ble::sendStop();
    // E enfileirar para o pipeline interno
    if (s_midi_queue) { uint8_t b = 0xFC; xQueueSend(s_midi_queue, &b, 0); }

    // Mensagem OLED
    oled_show_message_centered(STR_MODO_SLAVE_MSG);

    if (s_timer) {
        timerAlarmDisable(s_timer);
        // Mantemos timer para reuso; desalocar apenas ao desligar sistema
    }

    s_running = false;
    s_active = false;

    // BLE: voltar a RX-only (modo Slave)
    midi_ble::set_master_tx_only(false);
    midi_ble::set_slave_rx_only(true);
    midi_ble::set_allow_note_tx_in_slave(true);
}

bool master_internal_is_active(void) {
    return s_active;
}

void master_internal_set_bpm(uint16_t bpm) {
    s_bpm = bpm;
    if (s_active && s_timer) {
        configure_timer_for_bpm(s_bpm);
    }
}

uint16_t master_internal_get_bpm(void) {
    return s_bpm;
}

// Consumir bytes pendentes do gerador interno (chamar no loop principal)
bool master_internal_pop_byte(uint8_t* out_byte) {
    if (!s_midi_queue || out_byte == NULL) return false;
    uint8_t b;
    if (xQueueReceive(s_midi_queue, &b, 0) == pdTRUE) {
        *out_byte = b;
        return true;
    }
    return false;
}

// Pausar clock interno (para modo slots SYSEX)
void master_internal_pause_clock(void) {
    if (s_active) {
        s_running = false;
    }
}

// Retomar clock interno (após modo slots SYSEX)
void master_internal_resume_clock(void) {
    if (s_active) {
        s_running = true;
    }
}


