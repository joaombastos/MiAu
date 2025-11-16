#ifndef CONTROLLER_PRESET_H
#define CONTROLLER_PRESET_H

#include <stdint.h>

// Deve capturar os botões de passos?
bool preset_should_capture_step_buttons(void);

// Evento: click no Extra 3 (entra/sai do modo de edição de slots RAM com housekeeping)
void preset_on_extra3_click(void);

// Evento: botão de passo pressionado (0..7)
void preset_on_step_press(uint8_t idx);

// Evento: long-press no botão de passo (0..7) – apagar slot
void preset_on_step_long_press(uint8_t idx);

#endif


