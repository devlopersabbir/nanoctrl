#ifndef NANO_INPUT_H
#define NANO_INPUT_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    NANO_MOUSE_LEFT   = 0,
    NANO_MOUSE_RIGHT  = 1,
    NANO_MOUSE_MIDDLE = 2
} nano_mouse_button_t;

typedef enum {
    NANO_ACTION_UP   = 0,
    NANO_ACTION_DOWN = 1
} nano_key_action_t;

/* Check whether the host has OS permission to inject input (e.g. macOS Accessibility) */
bool nano_input_has_permission(void);

/* Inject mouse movement mapped to host display width and height */
void nano_input_inject_mouse_move(uint16_t norm_x, uint16_t norm_y, uint16_t screen_w, uint16_t screen_h);

/* Inject mouse button press or release */
void nano_input_inject_mouse_button(nano_mouse_button_t button, nano_key_action_t action,
                                   uint16_t norm_x, uint16_t norm_y, uint16_t screen_w, uint16_t screen_h);

/* Inject mouse wheel scroll */
void nano_input_inject_mouse_scroll(int16_t dx, int16_t dy);

/* Inject keyboard key press or release */
void nano_input_inject_key(uint16_t keycode, nano_key_action_t action, uint8_t modifiers);

#endif /* NANO_INPUT_H */
