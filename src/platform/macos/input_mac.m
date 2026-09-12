#include "../../input/input.h"
#import <ApplicationServices/ApplicationServices.h>
#import <CoreGraphics/CoreGraphics.h>
#include <stdio.h>

bool nano_input_has_permission(void) {
    return AXIsProcessTrusted();
}

void nano_input_inject_mouse_move(uint16_t norm_x, uint16_t norm_y, uint16_t screen_w, uint16_t screen_h) {
    if (screen_w == 0 || screen_h == 0) return;

    CGFloat x = ((CGFloat)norm_x * screen_w) / 65535.0f;
    CGFloat y = ((CGFloat)norm_y * screen_h) / 65535.0f;
    CGPoint pt = CGPointMake(x, y);

    CGEventRef ev = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, pt, kCGMouseButtonLeft);
    if (ev) {
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
    }
}

void nano_input_inject_mouse_button(nano_mouse_button_t button, nano_key_action_t action,
                                   uint16_t norm_x, uint16_t norm_y, uint16_t screen_w, uint16_t screen_h) {
    if (screen_w == 0 || screen_h == 0) return;

    CGFloat x = ((CGFloat)norm_x * screen_w) / 65535.0f;
    CGFloat y = ((CGFloat)norm_y * screen_h) / 65535.0f;
    CGPoint pt = CGPointMake(x, y);

    CGEventType ev_type;
    CGMouseButton cg_btn;

    if (button == NANO_MOUSE_LEFT) {
        cg_btn = kCGMouseButtonLeft;
        ev_type = (action == NANO_ACTION_DOWN) ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
    } else if (button == NANO_MOUSE_RIGHT) {
        cg_btn = kCGMouseButtonRight;
        ev_type = (action == NANO_ACTION_DOWN) ? kCGEventRightMouseDown : kCGEventRightMouseUp;
    } else {
        cg_btn = kCGMouseButtonCenter;
        ev_type = (action == NANO_ACTION_DOWN) ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
    }

    CGEventRef ev = CGEventCreateMouseEvent(NULL, ev_type, pt, cg_btn);
    if (ev) {
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
    }
}

void nano_input_inject_mouse_scroll(int16_t dx, int16_t dy) {
    CGEventRef ev = CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitPixel, 2, (int32_t)dy, (int32_t)dx);
    if (ev) {
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
    }
}

void nano_input_inject_key(uint16_t keycode, nano_key_action_t action, uint8_t modifiers) {
    CGEventRef ev = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)keycode, action == NANO_ACTION_DOWN);
    if (ev) {
        CGEventFlags flags = 0;
        if (modifiers & 1) flags |= kCGEventFlagMaskShift;
        if (modifiers & 2) flags |= kCGEventFlagMaskControl;
        if (modifiers & 4) flags |= kCGEventFlagMaskAlternate;
        if (modifiers & 8) flags |= kCGEventFlagMaskCommand;
        CGEventSetFlags(ev, flags);
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
    }
}
