#ifndef NANO_SESSION_H
#define NANO_SESSION_H

#include "protocol.h"
#include "../net/socket.h"
#include "../net/transport.h"
#include "../screen/capture.h"
#include "../screen/diff.h"
#include "../input/input.h"
#include <pthread.h>

typedef enum {
    NANO_ROLE_HOST,
    NANO_ROLE_CONTROLLER
} nano_role_t;

typedef enum {
    SESSION_STATE_IDLE,
    SESSION_STATE_LISTENING,
    SESSION_STATE_CONNECTING,
    SESSION_STATE_PENDING_APPROVAL,
    SESSION_STATE_ACTIVE,
    SESSION_STATE_DISCONNECTED
} nano_session_state_t;

typedef struct nano_session_s nano_session_t;

/* Callback when state changes */
typedef void (*nano_session_state_cb_t)(nano_session_t *s, nano_session_state_t state, const char *msg, void *user_data);

/* Callback on Host when approval is requested */
typedef bool (*nano_session_approval_cb_t)(nano_session_t *s, const char *remote_ip, void *user_data);

/* Callback on Controller when framebuffer has been updated */
typedef void (*nano_session_frame_cb_t)(nano_session_t *s, const nano_frame_t *frame, void *user_data);

struct nano_session_s {
    nano_role_t role;
    nano_session_state_t state;
    char pin[7];
    char remote_host[128];
    uint16_t port;

    nano_socket_t listen_sock;
    nano_socket_t conn_sock;

    /* Host capture state */
    nano_capture_t *capture;
    nano_frame_t host_prev_frame;
    bool screen_header_sent;

    /* Controller framebuffer */
    nano_frame_t controller_frame;

    /* Host display dimensions */
    uint16_t host_screen_w;
    uint16_t host_screen_h;

    /* Callbacks */
    nano_session_state_cb_t on_state_change;
    nano_session_approval_cb_t on_approval_request;
    nano_session_frame_cb_t on_frame_update;
    void *user_data;

    volatile bool is_running;
    pthread_t thread;
    pthread_mutex_t lock;
};

nano_session_t *nano_session_create(nano_role_t role);
void nano_session_destroy(nano_session_t *s);

/* Host functions */
bool nano_session_start_host(nano_session_t *s, uint16_t port, const char *custom_pin);

/* Controller functions */
bool nano_session_start_controller(nano_session_t *s, const char *host, uint16_t port, const char *pin);

/* Controller input dispatchers */
void nano_session_send_mouse_move(nano_session_t *s, uint16_t norm_x, uint16_t norm_y);
void nano_session_send_mouse_button(nano_session_t *s, nano_mouse_button_t btn, nano_key_action_t act, uint16_t norm_x, uint16_t norm_y);
void nano_session_send_mouse_scroll(nano_session_t *s, int16_t dx, int16_t dy);
void nano_session_send_key(nano_session_t *s, uint16_t keycode, nano_key_action_t act, uint8_t modifiers);

/* Stop / disconnect */
void nano_session_stop(nano_session_t *s);

#endif /* NANO_SESSION_H */
