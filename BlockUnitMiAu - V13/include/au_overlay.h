#ifndef AU_OVERLAY_H
#define AU_OVERLAY_H

// Estados do overlay AU
typedef enum {
    AU_OVERLAY_OFF = 0,
    AU_OVERLAY_MENU,
    AU_OVERLAY_ONDAS,
    AU_OVERLAY_GATE,
    AU_OVERLAY_DRUM_MACHINE,
    AU_OVERLAY_SLICE,
    AU_OVERLAY_SYNTHESIZER
} AuOverlayState;

// Funções do overlay AU
void au_overlay_begin(void);
void au_overlay_tick(void);
void au_overlay_toggle(void);
bool au_overlay_is_active(void);
AuOverlayState au_overlay_get_state(void);
const char* au_overlay_get_mode_name(AuOverlayState state);
void au_overlay_render_oled(void);

#endif
