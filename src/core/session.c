#include "session.h"
#include "../crypto/random.h"
#include "../crypto/auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define SCRATCH_BUF_SIZE (NANO_TILE_SIZE * NANO_TILE_SIZE * 4 + 1024)

static void host_frame_captured(const nano_frame_t *frame, void *user_data) {
    nano_session_t *s = (nano_session_t *)user_data;
    if (!s || !s->is_running || s->state != SESSION_STATE_ACTIVE) return;

    pthread_mutex_lock(&s->lock);

    if (!nano_socket_is_valid(s->conn_sock)) {
        pthread_mutex_unlock(&s->lock);
        return;
    }

    s->host_screen_w = frame->width;
    s->host_screen_h = frame->height;

    /* First frame: send SCREEN_HEADER */
    if (!s->screen_header_sent) {
        nano_msg_screen_header_t shdr;
        memset(&shdr, 0, sizeof(shdr));
        shdr.width = frame->width;
        shdr.height = frame->height;
        shdr.tile_w = NANO_TILE_SIZE;
        shdr.tile_h = NANO_TILE_SIZE;
        shdr.format = 0; /* BGRA32 */

        if (!nano_transport_send_msg(s->conn_sock, NANO_MSG_SCREEN_HEADER, NANO_FLAG_NONE,
                                     &shdr, sizeof(shdr), 1000)) {
            pthread_mutex_unlock(&s->lock);
            return;
        }

        nano_frame_free(&s->host_prev_frame);
        nano_frame_alloc(&s->host_prev_frame, frame->width, frame->height);
        s->screen_header_sent = true;
    }

    uint16_t tiles_x = (frame->width + NANO_TILE_SIZE - 1) / NANO_TILE_SIZE;
    uint16_t tiles_y = (frame->height + NANO_TILE_SIZE - 1) / NANO_TILE_SIZE;

    uint8_t raw_buf[NANO_TILE_SIZE * NANO_TILE_SIZE * 4];
    uint8_t rle_buf[NANO_TILE_SIZE * NANO_TILE_SIZE * 4 + 512];
    uint8_t packet_buf[SCRATCH_BUF_SIZE];

    for (uint16_t ty = 0; ty < tiles_y; ++ty) {
        for (uint16_t tx = 0; tx < tiles_x; ++tx) {
            uint16_t cur_tw = NANO_TILE_SIZE;
            if ((uint32_t)tx * NANO_TILE_SIZE + cur_tw > frame->width) {
                cur_tw = (uint16_t)(frame->width - (uint32_t)tx * NANO_TILE_SIZE);
            }
            uint16_t cur_th = NANO_TILE_SIZE;
            if ((uint32_t)ty * NANO_TILE_SIZE + cur_th > frame->height) {
                cur_th = (uint16_t)(frame->height - (uint32_t)ty * NANO_TILE_SIZE);
            }

            if (!nano_tile_is_dirty(frame, &s->host_prev_frame, tx, ty, cur_tw, cur_th)) {
                continue;
            }

            size_t raw_bytes = nano_tile_extract_raw(frame, tx, ty, cur_tw, cur_th, raw_buf, sizeof(raw_buf));
            if (raw_bytes == 0) continue;

            size_t pixel_count = (size_t)cur_tw * cur_th;
            size_t rle_bytes = nano_rle_encode((const uint32_t *)raw_buf, pixel_count, rle_buf, sizeof(rle_buf));

            nano_msg_flags_t flags = NANO_FLAG_RAW;
            const uint8_t *payload_ptr = raw_buf;
            size_t payload_len = raw_bytes;

            if (rle_bytes > 0 && rle_bytes < raw_bytes) {
                flags = NANO_FLAG_COMPRESSED;
                payload_ptr = rle_buf;
                payload_len = rle_bytes;
            }

            nano_msg_tile_header_t thdr;
            thdr.tile_x = tx;
            thdr.tile_y = ty;
            thdr.width = cur_tw;
            thdr.height = cur_th;
            thdr.uncompressed_len = (uint32_t)raw_bytes;

            /* Combine header and tile payload */
            memcpy(packet_buf, &thdr, sizeof(thdr));
            memcpy(packet_buf + sizeof(thdr), payload_ptr, payload_len);

            uint32_t total_msg_len = (uint32_t)(sizeof(thdr) + payload_len);
            if (!nano_transport_send_msg(s->conn_sock, NANO_MSG_SCREEN_TILE, flags,
                                         packet_buf, total_msg_len, 200)) {
                /* Send failed (socket closed/congested) */
                pthread_mutex_unlock(&s->lock);
                return;
            }

            /* Update prev frame for this tile */
            nano_tile_apply(&s->host_prev_frame, tx, ty, cur_tw, cur_th, raw_buf, raw_bytes, NANO_FLAG_RAW);
        }
    }

    pthread_mutex_unlock(&s->lock);
}

static void *host_thread_func(void *arg) {
    nano_session_t *s = (nano_session_t *)arg;

    s->listen_sock = nano_socket_create();
    if (!nano_socket_is_valid(s->listen_sock)) {
        if (s->on_state_change) s->on_state_change(s, SESSION_STATE_DISCONNECTED, "Failed to create socket", s->user_data);
        return NULL;
    }

    if (!nano_socket_bind_listen(s->listen_sock, "0.0.0.0", s->port, 5)) {
        if (s->on_state_change) s->on_state_change(s, SESSION_STATE_DISCONNECTED, "Failed to bind to port", s->user_data);
        nano_socket_close(s->listen_sock);
        s->listen_sock = NANO_INVALID_SOCKET;
        return NULL;
    }

    s->state = SESSION_STATE_LISTENING;
    if (s->on_state_change) s->on_state_change(s, SESSION_STATE_LISTENING, "Waiting for controller connection...", s->user_data);

    while (s->is_running) {
        char client_ip[64] = {0};
        uint16_t client_port = 0;
        nano_socket_t client_fd = nano_socket_accept(s->listen_sock, client_ip, sizeof(client_ip), &client_port);
        if (!nano_socket_is_valid(client_fd)) {
            if (!s->is_running) break;
            usleep(50000);
            continue;
        }

        /* 1. Recv HELLO */
        nano_header_t hdr;
        nano_msg_hello_t hello;
        if (!nano_transport_recv_msg(client_fd, &hdr, &hello, sizeof(hello), 3000) ||
            hdr.type != NANO_MSG_HELLO) {
            nano_socket_close(client_fd);
            continue;
        }

        /* 2. Send AUTH_CHALLENGE */
        nano_msg_auth_challenge_t challenge;
        nano_random_bytes(challenge.nonce, sizeof(challenge.nonce));
        if (!nano_transport_send_msg(client_fd, NANO_MSG_AUTH_CHALLENGE, NANO_FLAG_NONE,
                                     &challenge, sizeof(challenge), 2000)) {
            nano_socket_close(client_fd);
            continue;
        }

        /* 3. Recv AUTH_RESPONSE */
        nano_msg_auth_response_t response;
        if (!nano_transport_recv_msg(client_fd, &hdr, &response, sizeof(response), 3000) ||
            hdr.type != NANO_MSG_AUTH_RESPONSE) {
            nano_socket_close(client_fd);
            continue;
        }

        /* 4. Verify PIN */
        if (!nano_auth_verify_response(s->pin, challenge.nonce, response.hash)) {
            nano_msg_auth_result_t res = { .status = 2 }; /* Bad PIN */
            nano_transport_send_msg(client_fd, NANO_MSG_AUTH_RESULT, NANO_FLAG_NONE, &res, sizeof(res), 1000);
            nano_socket_close(client_fd);
            continue;
        }

        /* 5. Host Approval Check */
        s->state = SESSION_STATE_PENDING_APPROVAL;
        if (s->on_state_change) {
            s->on_state_change(s, SESSION_STATE_PENDING_APPROVAL, client_ip, s->user_data);
        }

        bool approved = true;
        if (s->on_approval_request) {
            approved = s->on_approval_request(s, client_ip, s->user_data);
        }

        if (!approved) {
            nano_msg_auth_result_t res = { .status = 1 }; /* Rejected */
            nano_transport_send_msg(client_fd, NANO_MSG_AUTH_RESULT, NANO_FLAG_NONE, &res, sizeof(res), 1000);
            nano_socket_close(client_fd);
            s->state = SESSION_STATE_LISTENING;
            if (s->on_state_change) s->on_state_change(s, SESSION_STATE_LISTENING, "Connection rejected. Waiting...", s->user_data);
            continue;
        }

        /* 6. Approved: Send AUTH_OK */
        nano_msg_auth_result_t res = { .status = 0 }; /* OK */
        if (!nano_transport_send_msg(client_fd, NANO_MSG_AUTH_RESULT, NANO_FLAG_NONE, &res, sizeof(res), 2000)) {
            nano_socket_close(client_fd);
            continue;
        }

        pthread_mutex_lock(&s->lock);
        s->conn_sock = client_fd;
        s->state = SESSION_STATE_ACTIVE;
        s->screen_header_sent = false;
        pthread_mutex_unlock(&s->lock);

        if (s->on_state_change) s->on_state_change(s, SESSION_STATE_ACTIVE, client_ip, s->user_data);

        /* Start screen capture */
        s->capture = nano_capture_create();
        nano_capture_start(s->capture, host_frame_captured, s);

        /* Input dispatch loop */
        uint8_t input_payload[1024];
        while (s->is_running && s->state == SESSION_STATE_ACTIVE) {
            if (!nano_transport_recv_header(s->conn_sock, &hdr, 500)) {
                /* Timeout or disconnect */
                if (errno == EAGAIN || errno == ETIMEDOUT) continue;
                break;
            }

            if (hdr.length > sizeof(input_payload)) break;
            if (!nano_transport_recv_payload(s->conn_sock, &hdr, input_payload, sizeof(input_payload), 500)) {
                break;
            }

            if (hdr.type == NANO_MSG_MOUSE_MOVE && hdr.length == sizeof(nano_msg_mouse_move_t)) {
                nano_msg_mouse_move_t *m = (nano_msg_mouse_move_t *)input_payload;
                nano_input_inject_mouse_move(m->norm_x, m->norm_y, s->host_screen_w, s->host_screen_h);
            } else if (hdr.type == NANO_MSG_MOUSE_BUTTON && hdr.length == sizeof(nano_msg_mouse_button_t)) {
                nano_msg_mouse_button_t *b = (nano_msg_mouse_button_t *)input_payload;
                nano_input_inject_mouse_button((nano_mouse_button_t)b->button, (nano_key_action_t)b->action,
                                               b->norm_x, b->norm_y, s->host_screen_w, s->host_screen_h);
            } else if (hdr.type == NANO_MSG_MOUSE_SCROLL && hdr.length == sizeof(nano_msg_mouse_scroll_t)) {
                nano_msg_mouse_scroll_t *sc = (nano_msg_mouse_scroll_t *)input_payload;
                nano_input_inject_mouse_scroll(sc->dx, sc->dy);
            } else if (hdr.type == NANO_MSG_KEY && hdr.length == sizeof(nano_msg_key_t)) {
                nano_msg_key_t *k = (nano_msg_key_t *)input_payload;
                nano_input_inject_key(k->keycode, (nano_key_action_t)k->action, k->modifiers);
            } else if (hdr.type == NANO_MSG_DISCONNECT) {
                break;
            }
        }

        /* Clean up active session */
        if (s->capture) {
            nano_capture_destroy(s->capture);
            s->capture = NULL;
        }

        pthread_mutex_lock(&s->lock);
        nano_socket_close(s->conn_sock);
        s->conn_sock = NANO_INVALID_SOCKET;
        nano_frame_free(&s->host_prev_frame);
        s->state = SESSION_STATE_LISTENING;
        pthread_mutex_unlock(&s->lock);

        if (s->on_state_change) s->on_state_change(s, SESSION_STATE_LISTENING, "Client disconnected. Waiting...", s->user_data);
    }

    if (nano_socket_is_valid(s->listen_sock)) {
        nano_socket_close(s->listen_sock);
        s->listen_sock = NANO_INVALID_SOCKET;
    }

    s->state = SESSION_STATE_DISCONNECTED;
    if (s->on_state_change) s->on_state_change(s, SESSION_STATE_DISCONNECTED, "Host stopped", s->user_data);
    return NULL;
}

static void *controller_thread_func(void *arg) {
    nano_session_t *s = (nano_session_t *)arg;

    s->state = SESSION_STATE_CONNECTING;
    if (s->on_state_change) s->on_state_change(s, SESSION_STATE_CONNECTING, "Connecting to host...", s->user_data);

    s->conn_sock = nano_socket_create();
    if (!nano_socket_is_valid(s->conn_sock)) {
        s->state = SESSION_STATE_DISCONNECTED;
        if (s->on_state_change) s->on_state_change(s, SESSION_STATE_DISCONNECTED, "Socket error", s->user_data);
        return NULL;
    }

    if (!nano_socket_connect(s->conn_sock, s->remote_host, s->port, 5000)) {
        s->state = SESSION_STATE_DISCONNECTED;
        if (s->on_state_change) s->on_state_change(s, SESSION_STATE_DISCONNECTED, "Connection failed or timed out", s->user_data);
        nano_socket_close(s->conn_sock);
        s->conn_sock = NANO_INVALID_SOCKET;
        return NULL;
    }

    /* 1. Send HELLO */
    nano_msg_hello_t hello = { .role = 1, .width = 0, .height = 0, .reserved = 0 };
    if (!nano_transport_send_msg(s->conn_sock, NANO_MSG_HELLO, NANO_FLAG_NONE, &hello, sizeof(hello), 2000)) {
        s->state = SESSION_STATE_DISCONNECTED;
        if (s->on_state_change) s->on_state_change(s, SESSION_STATE_DISCONNECTED, "Failed to send hello", s->user_data);
        nano_socket_close(s->conn_sock);
        return NULL;
    }

    /* 2. Recv AUTH_CHALLENGE */
    nano_header_t hdr;
    nano_msg_auth_challenge_t challenge;
    if (!nano_transport_recv_msg(s->conn_sock, &hdr, &challenge, sizeof(challenge), 3000) ||
        hdr.type != NANO_MSG_AUTH_CHALLENGE) {
        s->state = SESSION_STATE_DISCONNECTED;
        if (s->on_state_change) s->on_state_change(s, SESSION_STATE_DISCONNECTED, "Failed to receive challenge", s->user_data);
        nano_socket_close(s->conn_sock);
        return NULL;
    }

    /* 3. Send AUTH_RESPONSE */
    nano_msg_auth_response_t response;
    nano_auth_compute_response(s->pin, challenge.nonce, response.hash);
    if (!nano_transport_send_msg(s->conn_sock, NANO_MSG_AUTH_RESPONSE, NANO_FLAG_NONE, &response, sizeof(response), 2000)) {
        s->state = SESSION_STATE_DISCONNECTED;
        if (s->on_state_change) s->on_state_change(s, SESSION_STATE_DISCONNECTED, "Failed to send auth response", s->user_data);
        nano_socket_close(s->conn_sock);
        return NULL;
    }

    /* 4. Recv AUTH_RESULT */
    nano_msg_auth_result_t result;
    if (!nano_transport_recv_msg(s->conn_sock, &hdr, &result, sizeof(result), 10000) ||
        hdr.type != NANO_MSG_AUTH_RESULT) {
        s->state = SESSION_STATE_DISCONNECTED;
        if (s->on_state_change) s->on_state_change(s, SESSION_STATE_DISCONNECTED, "Authentication response timed out", s->user_data);
        nano_socket_close(s->conn_sock);
        return NULL;
    }

    if (result.status != 0) {
        const char *err_msg = (result.status == 1) ? "Connection rejected by host" :
                              (result.status == 2) ? "Invalid PIN" : "Authentication error";
        s->state = SESSION_STATE_DISCONNECTED;
        if (s->on_state_change) s->on_state_change(s, SESSION_STATE_DISCONNECTED, err_msg, s->user_data);
        nano_socket_close(s->conn_sock);
        return NULL;
    }

    s->state = SESSION_STATE_ACTIVE;
    if (s->on_state_change) s->on_state_change(s, SESSION_STATE_ACTIVE, "Connected to remote desktop", s->user_data);

    /* Allocate buffer for receiving stream messages */
    size_t payload_capacity = SCRATCH_BUF_SIZE * 2;
    uint8_t *payload_buf = (uint8_t *)malloc(payload_capacity);

    while (s->is_running && s->state == SESSION_STATE_ACTIVE) {
        if (!nano_transport_recv_header(s->conn_sock, &hdr, 1000)) {
            if (errno == EAGAIN || errno == ETIMEDOUT) continue;
            break;
        }

        if (hdr.length > payload_capacity) {
            payload_capacity = hdr.length + 4096;
            payload_buf = (uint8_t *)realloc(payload_buf, payload_capacity);
        }

        if (!nano_transport_recv_payload(s->conn_sock, &hdr, payload_buf, payload_capacity, 3000)) {
            break;
        }

        if (hdr.type == NANO_MSG_SCREEN_HEADER && hdr.length >= sizeof(nano_msg_screen_header_t)) {
            nano_msg_screen_header_t *shdr = (nano_msg_screen_header_t *)payload_buf;
            pthread_mutex_lock(&s->lock);
            nano_frame_free(&s->controller_frame);
            nano_frame_alloc(&s->controller_frame, shdr->width, shdr->height);
            pthread_mutex_unlock(&s->lock);

            if (s->on_frame_update) {
                s->on_frame_update(s, &s->controller_frame, s->user_data);
            }
        } else if (hdr.type == NANO_MSG_SCREEN_TILE && hdr.length >= sizeof(nano_msg_tile_header_t)) {
            nano_msg_tile_header_t *thdr = (nano_msg_tile_header_t *)payload_buf;
            const uint8_t *tile_data = payload_buf + sizeof(nano_msg_tile_header_t);
            size_t tile_data_len = hdr.length - sizeof(nano_msg_tile_header_t);

            pthread_mutex_lock(&s->lock);
            nano_tile_apply(&s->controller_frame, thdr->tile_x, thdr->tile_y,
                            thdr->width, thdr->height,
                            tile_data, tile_data_len, (nano_msg_flags_t)hdr.flags);
            pthread_mutex_unlock(&s->lock);

            if (s->on_frame_update) {
                s->on_frame_update(s, &s->controller_frame, s->user_data);
            }
        } else if (hdr.type == NANO_MSG_DISCONNECT) {
            break;
        }
    }

    free(payload_buf);

    pthread_mutex_lock(&s->lock);
    nano_socket_close(s->conn_sock);
    s->conn_sock = NANO_INVALID_SOCKET;
    s->state = SESSION_STATE_DISCONNECTED;
    pthread_mutex_unlock(&s->lock);

    if (s->on_state_change) s->on_state_change(s, SESSION_STATE_DISCONNECTED, "Session terminated", s->user_data);
    return NULL;
}

nano_session_t *nano_session_create(nano_role_t role) {
    nano_session_t *s = (nano_session_t *)calloc(1, sizeof(nano_session_t));
    if (!s) return NULL;
    s->role = role;
    s->state = SESSION_STATE_IDLE;
    s->listen_sock = NANO_INVALID_SOCKET;
    s->conn_sock = NANO_INVALID_SOCKET;
    pthread_mutex_init(&s->lock, NULL);
    return s;
}

void nano_session_destroy(nano_session_t *s) {
    if (!s) return;
    nano_session_stop(s);
    nano_frame_free(&s->host_prev_frame);
    nano_frame_free(&s->controller_frame);
    if (s->capture) {
        nano_capture_destroy(s->capture);
        s->capture = NULL;
    }
    pthread_mutex_destroy(&s->lock);
    free(s);
}

bool nano_session_start_host(nano_session_t *s, uint16_t port, const char *custom_pin) {
    if (!s || s->role != NANO_ROLE_HOST) return false;
    s->port = port > 0 ? port : NANO_DEFAULT_PORT;

    if (custom_pin && strlen(custom_pin) == 6) {
        strncpy(s->pin, custom_pin, 6);
        s->pin[6] = '\0';
    } else {
        nano_random_pin(s->pin);
    }

    s->is_running = true;
    if (pthread_create(&s->thread, NULL, host_thread_func, s) != 0) {
        s->is_running = false;
        return false;
    }

    return true;
}

bool nano_session_start_controller(nano_session_t *s, const char *host, uint16_t port, const char *pin) {
    if (!s || s->role != NANO_ROLE_CONTROLLER || !host || !pin) return false;
    strncpy(s->remote_host, host, sizeof(s->remote_host) - 1);
    s->port = port > 0 ? port : NANO_DEFAULT_PORT;
    strncpy(s->pin, pin, 6);
    s->pin[6] = '\0';

    s->is_running = true;
    if (pthread_create(&s->thread, NULL, controller_thread_func, s) != 0) {
        s->is_running = false;
        return false;
    }

    return true;
}

void nano_session_send_mouse_move(nano_session_t *s, uint16_t norm_x, uint16_t norm_y) {
    if (!s || s->state != SESSION_STATE_ACTIVE || !nano_socket_is_valid(s->conn_sock)) return;
    nano_msg_mouse_move_t m = { .norm_x = norm_x, .norm_y = norm_y };
    nano_transport_send_msg(s->conn_sock, NANO_MSG_MOUSE_MOVE, NANO_FLAG_NONE, &m, sizeof(m), 50);
}

void nano_session_send_mouse_button(nano_session_t *s, nano_mouse_button_t btn, nano_key_action_t act, uint16_t norm_x, uint16_t norm_y) {
    if (!s || s->state != SESSION_STATE_ACTIVE || !nano_socket_is_valid(s->conn_sock)) return;
    nano_msg_mouse_button_t b = {
        .button = (uint8_t)btn,
        .action = (uint8_t)act,
        .norm_x = norm_x,
        .norm_y = norm_y
    };
    nano_transport_send_msg(s->conn_sock, NANO_MSG_MOUSE_BUTTON, NANO_FLAG_NONE, &b, sizeof(b), 100);
}

void nano_session_send_mouse_scroll(nano_session_t *s, int16_t dx, int16_t dy) {
    if (!s || s->state != SESSION_STATE_ACTIVE || !nano_socket_is_valid(s->conn_sock)) return;
    nano_msg_mouse_scroll_t sc = { .dx = dx, .dy = dy };
    nano_transport_send_msg(s->conn_sock, NANO_MSG_MOUSE_SCROLL, NANO_FLAG_NONE, &sc, sizeof(sc), 50);
}

void nano_session_send_key(nano_session_t *s, uint16_t keycode, nano_key_action_t act, uint8_t modifiers) {
    if (!s || s->state != SESSION_STATE_ACTIVE || !nano_socket_is_valid(s->conn_sock)) return;
    nano_msg_key_t k = { .keycode = keycode, .action = (uint8_t)act, .modifiers = modifiers };
    nano_transport_send_msg(s->conn_sock, NANO_MSG_KEY, NANO_FLAG_NONE, &k, sizeof(k), 100);
}

void nano_session_stop(nano_session_t *s) {
    if (!s || !s->is_running) return;
    s->is_running = false;

    if (nano_socket_is_valid(s->conn_sock)) {
        nano_msg_disconnect_t disc = { .reason = 0 };
        nano_transport_send_msg(s->conn_sock, NANO_MSG_DISCONNECT, NANO_FLAG_NONE, &disc, sizeof(disc), 100);
        nano_socket_close(s->conn_sock);
        s->conn_sock = NANO_INVALID_SOCKET;
    }

    if (nano_socket_is_valid(s->listen_sock)) {
        nano_socket_close(s->listen_sock);
        s->listen_sock = NANO_INVALID_SOCKET;
    }

    pthread_join(s->thread, NULL);
}
