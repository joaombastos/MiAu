#ifndef EDIT_SYSTEM_SLAVE_H
#define EDIT_SYSTEM_SLAVE_H
#include <stdint.h>

enum MenuStateSlave {
    MENU_NORMAL,
    MENU_SELECT_STEP,
    MENU_EDIT_NOTE,
    MENU_EDIT_CHANNEL,
    MENU_EDIT_VELOCITY,
    MENU_EDIT_LENGTH,
    MENU_EDIT_RESOLUTION,
    MENU_MASTER_BPM,
};


extern MenuStateSlave menu_state;
extern uint8_t editing_step;
extern uint8_t banco_atual;

#endif 