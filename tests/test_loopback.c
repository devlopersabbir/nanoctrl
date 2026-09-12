#include "../src/core/session.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

static volatile int g_frames_received = 0;
static volatile bool g_host_active = false;
static volatile bool g_controller_active = false;

static void host_state_cb(nano_session_t *s, nano_session_state_t state, const char *msg, void *user_data) {
    (void)s; (void)msg; (void)user_data;
    if (state == SESSION_STATE_ACTIVE) {
        g_host_active = true;
    }
}

static bool host_approval_cb(nano_session_t *s, const char *remote_ip, void *user_data) {
    (void)s; (void)remote_ip; (void)user_data;
    return true; /* Automatically approve for test */
}

static void ctrl_state_cb(nano_session_t *s, nano_session_state_t state, const char *msg, void *user_data) {
    (void)s; (void)msg; (void)user_data;
    if (state == SESSION_STATE_ACTIVE) {
        g_controller_active = true;
    }
}

static void ctrl_frame_cb(nano_session_t *s, const nano_frame_t *frame, void *user_data) {
    (void)s; (void)frame; (void)user_data;
    g_frames_received++;
}

int main(void) {
    printf("Running Loopback Integration Test...\n");

    uint16_t test_port = 8990;
    const char *test_pin = "654321";

    /* 1. Create and start Host */
    nano_session_t *host = nano_session_create(NANO_ROLE_HOST);
    assert(host != NULL);
    host->on_state_change = host_state_cb;
    host->on_approval_request = host_approval_cb;
    assert(nano_session_start_host(host, test_port, test_pin));

    usleep(200000); /* 200ms */

    /* 2. Create and start Controller */
    nano_session_t *ctrl = nano_session_create(NANO_ROLE_CONTROLLER);
    assert(ctrl != NULL);
    ctrl->on_state_change = ctrl_state_cb;
    ctrl->on_frame_update = ctrl_frame_cb;
    assert(nano_session_start_controller(ctrl, "127.0.0.1", test_port, test_pin));

    /* 3. Wait for session active */
    int wait_cycles = 0;
    while ((!g_host_active || !g_controller_active) && wait_cycles < 50) {
        usleep(100000);
        wait_cycles++;
    }

    assert(g_host_active);
    assert(g_controller_active);
    printf("[PASS] Host and Controller connected and authenticated!\n");

    /* 4. Wait for frame transmission */
    wait_cycles = 0;
    while (g_frames_received < 3 && wait_cycles < 50) {
        usleep(100000);
        wait_cycles++;
    }

    assert(g_frames_received >= 1);
    printf("[PASS] Received %d frames over network loopback!\n", g_frames_received);

    /* 5. Dispatch input events from controller */
    nano_session_send_mouse_move(ctrl, 32768, 32768);
    nano_session_send_mouse_button(ctrl, NANO_MOUSE_LEFT, NANO_ACTION_DOWN, 32768, 32768);
    nano_session_send_mouse_button(ctrl, NANO_MOUSE_LEFT, NANO_ACTION_UP, 32768, 32768);
    nano_session_send_key(ctrl, 0x00, NANO_ACTION_DOWN, 0);
    nano_session_send_key(ctrl, 0x00, NANO_ACTION_UP, 0);
    printf("[PASS] Input events sent without errors!\n");

    /* 6. Clean stop */
    nano_session_stop(ctrl);
    nano_session_destroy(ctrl);

    nano_session_stop(host);
    nano_session_destroy(host);

    printf("ALL LOOPBACK INTEGRATION TESTS PASSED!\n");
    return 0;
}
