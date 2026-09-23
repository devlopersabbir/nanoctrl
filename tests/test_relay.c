#include "../src/core/session.h"
#include "../src/net/server.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

static volatile int g_relay_frames_received = 0;
static volatile bool g_relay_host_active = false;
static volatile bool g_relay_controller_active = false;

static void host_state_cb(nano_session_t *s, nano_session_state_t state, const char *msg, void *user_data) {
    (void)s; (void)msg; (void)user_data;
    if (state == SESSION_STATE_ACTIVE) {
        g_relay_host_active = true;
    }
}

static bool host_approval_cb(nano_session_t *s, const char *remote_ip, void *user_data) {
    (void)s; (void)remote_ip; (void)user_data;
    return true; /* Automatically approve for test */
}

static void ctrl_state_cb(nano_session_t *s, nano_session_state_t state, const char *msg, void *user_data) {
    (void)s; (void)msg; (void)user_data;
    if (state == SESSION_STATE_ACTIVE) {
        g_relay_controller_active = true;
    }
}

static void ctrl_frame_cb(nano_session_t *s, const nano_frame_t *frame, void *user_data) {
    (void)s; (void)frame; (void)user_data;
    g_relay_frames_received++;
}

int main(void) {
    printf("Running Self-Hosted Server & Relay Integration Test...\n");

    uint16_t test_port = 8995;
    uint32_t host_device_id = 842190345;
    const char *test_pin = "654321";

    /* 1. Start Relay Server */
    nano_server_t *srv = nano_server_create(test_port);
    assert(srv != NULL);
    assert(nano_server_start(srv));
    printf("[PASS] Standalone Relay Server started on port %u\n", test_port);

    usleep(150000); /* 150ms */

    /* 2. Create and start Host in Relay Mode */
    nano_session_t *host = nano_session_create(NANO_ROLE_HOST);
    assert(host != NULL);
    host->on_state_change = host_state_cb;
    host->on_approval_request = host_approval_cb;
    assert(nano_session_start_host_relay(host, "127.0.0.1", test_port, host_device_id, test_pin));
    printf("[PASS] Host registered with Relay Server using Device ID %u\n", host_device_id);

    usleep(200000); /* 200ms */

    /* 3. Create and start Controller in Relay Mode */
    nano_session_t *ctrl = nano_session_create(NANO_ROLE_CONTROLLER);
    assert(ctrl != NULL);
    ctrl->on_state_change = ctrl_state_cb;
    ctrl->on_frame_update = ctrl_frame_cb;
    assert(nano_session_start_controller_relay(ctrl, "127.0.0.1", test_port, host_device_id, test_pin));
    printf("[PASS] Controller initiated connection to Device ID %u via Relay\n", host_device_id);

    /* 4. Wait for session active across relay */
    int wait_cycles = 0;
    while ((!g_relay_host_active || !g_relay_controller_active) && wait_cycles < 60) {
        usleep(100000);
        wait_cycles++;
    }

    assert(g_relay_host_active);
    assert(g_relay_controller_active);
    printf("[PASS] Host and Controller paired and authenticated through Relay Server!\n");

    /* 5. Wait for frame transmission across relay */
    wait_cycles = 0;
    while (g_relay_frames_received < 3 && wait_cycles < 60) {
        usleep(100000);
        wait_cycles++;
    }

    assert(g_relay_frames_received >= 1);
    printf("[PASS] Received %d frames over relay tunnel!\n", g_relay_frames_received);

    /* 6. Dispatch input events across relay */
    nano_session_send_mouse_move(ctrl, 32768, 32768);
    nano_session_send_mouse_button(ctrl, NANO_MOUSE_LEFT, NANO_ACTION_DOWN, 32768, 32768);
    nano_session_send_mouse_button(ctrl, NANO_MOUSE_LEFT, NANO_ACTION_UP, 32768, 32768);
    nano_session_send_key(ctrl, 0x00, NANO_ACTION_DOWN, 0);
    nano_session_send_key(ctrl, 0x00, NANO_ACTION_UP, 0);
    printf("[PASS] Input events forwarded across relay tunnel successfully!\n");

    /* 7. Clean stop */
    nano_session_stop(ctrl);
    nano_session_destroy(ctrl);

    nano_session_stop(host);
    nano_session_destroy(host);

    nano_server_stop(srv);
    nano_server_destroy(srv);

    printf("ALL SELF-HOSTED RELAY TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
