#include "server.h"
#include "transport.h"
#include "../crypto/random.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>

static uint64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

void nano_format_device_id(uint32_t id, char *out_buf, size_t buf_size) {
    if (!out_buf || buf_size < 12) return;
    char raw[12];
    snprintf(raw, sizeof(raw), "%09u", id % 1000000000);
    snprintf(out_buf, buf_size, "%.3s %.3s %.3s", raw, raw + 3, raw + 6);
}

uint32_t nano_parse_device_id(const char *id_str) {
    if (!id_str) return 0;
    uint32_t val = 0;
    while (*id_str) {
        if (*id_str >= '0' && *id_str <= '9') {
            val = val * 10 + (uint32_t)(*id_str - '0');
        }
        id_str++;
    }
    return val;
}

static void server_log(nano_server_t *srv, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (srv && srv->on_log) {
        srv->on_log(buf, srv->user_data);
    } else {
        printf("[NANOSRV] %s\n", buf);
        fflush(stdout);
    }
}

nano_server_t *nano_server_create(uint16_t port) {
    nano_server_t *srv = (nano_server_t *)calloc(1, sizeof(nano_server_t));
    if (!srv) return NULL;
    srv->port = port > 0 ? port : NANO_DEFAULT_PORT;
    srv->listen_sock = NANO_INVALID_SOCKET;
    srv->is_running = false;
    pthread_mutex_init(&srv->lock, NULL);
    return srv;
}

static nano_peer_entry_t *find_peer(nano_server_t *srv, uint32_t device_id) {
    for (size_t i = 0; i < NANO_MAX_PEERS; ++i) {
        if (srv->peers[i].is_active && srv->peers[i].device_id == device_id) {
            return &srv->peers[i];
        }
    }
    return NULL;
}

static nano_relay_room_t *find_room(nano_server_t *srv, uint64_t session_token) {
    for (size_t i = 0; i < NANO_MAX_RELAY_ROOMS; ++i) {
        if (srv->rooms[i].is_active && srv->rooms[i].session_token == session_token) {
            return &srv->rooms[i];
        }
    }
    return NULL;
}

static nano_relay_room_t *alloc_room(nano_server_t *srv, uint64_t session_token) {
    for (size_t i = 0; i < NANO_MAX_RELAY_ROOMS; ++i) {
        if (!srv->rooms[i].is_active) {
            memset(&srv->rooms[i], 0, sizeof(nano_relay_room_t));
            srv->rooms[i].session_token = session_token;
            srv->rooms[i].host_sock = NANO_INVALID_SOCKET;
            srv->rooms[i].ctrl_sock = NANO_INVALID_SOCKET;
            srv->rooms[i].created_at_ms = get_time_ms();
            srv->rooms[i].is_active = true;
            srv->room_count++;
            return &srv->rooms[i];
        }
    }
    return NULL;
}

static void handle_new_connection(nano_server_t *srv, nano_socket_t client_sock) {
    nano_header_t hdr;
    if (!nano_transport_recv_header(client_sock, &hdr, 3000)) {
        nano_socket_close(client_sock);
        return;
    }

    if (hdr.type == NANO_MSG_SRV_REGISTER && hdr.length >= sizeof(nano_msg_srv_register_t)) {
        nano_msg_srv_register_t reg;
        if (!nano_transport_recv_payload(client_sock, &hdr, &reg, sizeof(reg), 2000)) {
            nano_socket_close(client_sock);
            return;
        }

        pthread_mutex_lock(&srv->lock);
        nano_peer_entry_t *existing = find_peer(srv, reg.device_id);
        if (existing) {
            /* Replace old connection */
            nano_socket_close(existing->sock);
            existing->sock = client_sock;
            existing->last_seen_ms = get_time_ms();
        } else {
            for (size_t i = 0; i < NANO_MAX_PEERS; ++i) {
                if (!srv->peers[i].is_active) {
                    srv->peers[i].device_id = reg.device_id;
                    srv->peers[i].sock = client_sock;
                    srv->peers[i].last_seen_ms = get_time_ms();
                    srv->peers[i].is_active = true;
                    srv->peer_count++;
                    break;
                }
            }
        }
        pthread_mutex_unlock(&srv->lock);

        char id_str[32];
        nano_format_device_id(reg.device_id, id_str, sizeof(id_str));
        server_log(srv, "Registered Host ID: %s (fd: %d)", id_str, client_sock);

        nano_msg_srv_register_ack_t ack = { .device_id = reg.device_id, .status = 0 };
        nano_transport_send_msg(client_sock, NANO_MSG_SRV_REGISTER_ACK, NANO_FLAG_NONE, &ack, sizeof(ack), 2000);
        return;
    }

    if (hdr.type == NANO_MSG_SRV_CONNECT_REQ && hdr.length >= sizeof(nano_msg_srv_connect_req_t)) {
        nano_msg_srv_connect_req_t req;
        if (!nano_transport_recv_payload(client_sock, &hdr, &req, sizeof(req), 2000)) {
            nano_socket_close(client_sock);
            return;
        }

        char id_str[32];
        nano_format_device_id(req.target_id, id_str, sizeof(id_str));
        server_log(srv, "Connect request for Host ID: %s", id_str);

        pthread_mutex_lock(&srv->lock);
        nano_peer_entry_t *target = find_peer(srv, req.target_id);
        if (!target || !target->is_active) {
            pthread_mutex_unlock(&srv->lock);
            nano_msg_srv_error_t err = { .code = 2 }; /* Offline */
            nano_transport_send_msg(client_sock, NANO_MSG_SRV_ERROR, NANO_FLAG_NONE, &err, sizeof(err), 2000);
            nano_socket_close(client_sock);
            return;
        }

        uint64_t token = 0;
        nano_random_bytes((uint8_t *)&token, sizeof(token));
        while (token == 0 || find_room(srv, token) != NULL) {
            nano_random_bytes((uint8_t *)&token, sizeof(token));
        }

        nano_relay_room_t *room = alloc_room(srv, token);
        if (!room) {
            pthread_mutex_unlock(&srv->lock);
            nano_msg_srv_error_t err = { .code = 3 }; /* Busy */
            nano_transport_send_msg(client_sock, NANO_MSG_SRV_ERROR, NANO_FLAG_NONE, &err, sizeof(err), 2000);
            nano_socket_close(client_sock);
            return;
        }

        /* Signal Host of incoming request */
        nano_msg_srv_incoming_t inc;
        inc.session_token = token;
        inc.controller_id = req.controller_id;
        nano_transport_send_msg(target->sock, NANO_MSG_SRV_INCOMING, NANO_FLAG_NONE, &inc, sizeof(inc), 2000);

        /* Respond to Controller with session token */
        nano_transport_send_msg(client_sock, NANO_MSG_SRV_INCOMING, NANO_FLAG_NONE, &inc, sizeof(inc), 2000);
        pthread_mutex_unlock(&srv->lock);

        nano_socket_close(client_sock);
        return;
    }

    if (hdr.type == NANO_MSG_SRV_RELAY_JOIN && hdr.length >= sizeof(nano_msg_srv_relay_join_t)) {
        nano_msg_srv_relay_join_t join;
        if (!nano_transport_recv_payload(client_sock, &hdr, &join, sizeof(join), 2000)) {
            nano_socket_close(client_sock);
            return;
        }

        pthread_mutex_lock(&srv->lock);
        nano_relay_room_t *room = find_room(srv, join.session_token);
        if (!room) {
            pthread_mutex_unlock(&srv->lock);
            nano_msg_srv_error_t err = { .code = 4 }; /* Invalid Token */
            nano_transport_send_msg(client_sock, NANO_MSG_SRV_ERROR, NANO_FLAG_NONE, &err, sizeof(err), 2000);
            nano_socket_close(client_sock);
            return;
        }

        if (join.role == 0) {
            /* Host */
            room->host_sock = client_sock;
            room->host_ready = true;
            server_log(srv, "Host joined relay room token: %llu (fd: %d)", (unsigned long long)join.session_token, client_sock);
        } else {
            /* Controller */
            room->ctrl_sock = client_sock;
            room->ctrl_ready = true;
            server_log(srv, "Controller joined relay room token: %llu (fd: %d)", (unsigned long long)join.session_token, client_sock);
        }

        if (room->host_ready && room->ctrl_ready) {
            server_log(srv, "Relay bridge ACTIVE for room token: %llu", (unsigned long long)join.session_token);
            nano_msg_srv_relay_ready_t ready = { .session_token = room->session_token, .status = 0 };
            nano_transport_send_msg(room->host_sock, NANO_MSG_SRV_RELAY_READY, NANO_FLAG_NONE, &ready, sizeof(ready), 2000);
            nano_transport_send_msg(room->ctrl_sock, NANO_MSG_SRV_RELAY_READY, NANO_FLAG_NONE, &ready, sizeof(ready), 2000);
        }
        pthread_mutex_unlock(&srv->lock);
        return;
    }

    /* Unknown initial message */
    nano_socket_close(client_sock);
}

static void *server_thread_func(void *arg) {
    nano_server_t *srv = (nano_server_t *)arg;
    server_log(srv, "Starting NANOCTRL Rendezvous & Relay Server on port %u...", srv->port);

    srv->listen_sock = nano_socket_create();
    if (!nano_socket_is_valid(srv->listen_sock)) {
        server_log(srv, "Failed to create listening socket");
        srv->is_running = false;
        return NULL;
    }

    if (!nano_socket_bind_listen(srv->listen_sock, "0.0.0.0", srv->port, 64)) {
        server_log(srv, "Failed to bind to port %u", srv->port);
        nano_socket_close(srv->listen_sock);
        srv->listen_sock = NANO_INVALID_SOCKET;
        srv->is_running = false;
        return NULL;
    }

    server_log(srv, "Server listening on 0.0.0.0:%u [READY]", srv->port);

    uint8_t relay_buf[65536];

    while (srv->is_running) {
        struct pollfd fds[2 + NANO_MAX_PEERS + NANO_MAX_RELAY_ROOMS * 2];
        size_t fd_count = 0;

        /* Index 0: Listen socket */
        fds[0].fd = srv->listen_sock;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        fd_count++;

        /* Add registered peers */
        pthread_mutex_lock(&srv->lock);
        size_t peer_fd_start = fd_count;
        size_t peer_indices[NANO_MAX_PEERS];
        size_t active_peer_entries = 0;

        for (size_t i = 0; i < NANO_MAX_PEERS; ++i) {
            if (srv->peers[i].is_active && nano_socket_is_valid(srv->peers[i].sock)) {
                fds[fd_count].fd = srv->peers[i].sock;
                fds[fd_count].events = POLLIN;
                fds[fd_count].revents = 0;
                peer_indices[active_peer_entries++] = i;
                fd_count++;
            }
        }

        /* Add active relay room sockets */
        size_t room_fd_start = fd_count;
        size_t room_indices[NANO_MAX_RELAY_ROOMS];
        size_t active_room_entries = 0;

        for (size_t i = 0; i < NANO_MAX_RELAY_ROOMS; ++i) {
            if (srv->rooms[i].is_active && srv->rooms[i].host_ready && srv->rooms[i].ctrl_ready) {
                fds[fd_count].fd = srv->rooms[i].host_sock;
                fds[fd_count].events = POLLIN;
                fds[fd_count].revents = 0;
                fd_count++;

                fds[fd_count].fd = srv->rooms[i].ctrl_sock;
                fds[fd_count].events = POLLIN;
                fds[fd_count].revents = 0;
                fd_count++;

                room_indices[active_room_entries++] = i;
            }
        }
        pthread_mutex_unlock(&srv->lock);

        int poll_res = poll(fds, (nfds_t)fd_count, 100);
        if (poll_res < 0) {
            if (errno == EINTR) continue;
            break;
        }

        /* 1. Handle new incoming connections */
        if (fds[0].revents & POLLIN) {
            nano_socket_t client = nano_socket_accept(srv->listen_sock, NULL, 0, NULL);
            if (nano_socket_is_valid(client)) {
                handle_new_connection(srv, client);
            }
        }

        /* 2. Handle peer control heartbeats & disconnections */
        pthread_mutex_lock(&srv->lock);
        for (size_t p = 0; p < active_peer_entries; ++p) {
            size_t idx = peer_fd_start + p;
            size_t peer_idx = peer_indices[p];
            if (fds[idx].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                server_log(srv, "Peer disconnected (fd: %d)", srv->peers[peer_idx].sock);
                nano_socket_close(srv->peers[peer_idx].sock);
                srv->peers[peer_idx].is_active = false;
                srv->peers[peer_idx].sock = NANO_INVALID_SOCKET;
                srv->peer_count--;
            } else if (fds[idx].revents & POLLIN) {
                nano_header_t phdr;
                if (!nano_transport_recv_header(srv->peers[peer_idx].sock, &phdr, 100)) {
                    server_log(srv, "Peer closed connection (fd: %d)", srv->peers[peer_idx].sock);
                    nano_socket_close(srv->peers[peer_idx].sock);
                    srv->peers[peer_idx].is_active = false;
                    srv->peers[peer_idx].sock = NANO_INVALID_SOCKET;
                    srv->peer_count--;
                } else if (phdr.type == NANO_MSG_SRV_HEARTBEAT) {
                    nano_msg_srv_heartbeat_t hb;
                    nano_transport_recv_payload(srv->peers[peer_idx].sock, &phdr, &hb, sizeof(hb), 100);
                    nano_transport_send_msg(srv->peers[peer_idx].sock, NANO_MSG_SRV_HEARTBEAT, NANO_FLAG_NONE, &hb, sizeof(hb), 100);
                    srv->peers[peer_idx].last_seen_ms = get_time_ms();
                }
            }
        }

        /* 3. High-speed bidirectional relay data forwarding */
        for (size_t r = 0; r < active_room_entries; ++r) {
            size_t host_fd_idx = room_fd_start + r * 2;
            size_t ctrl_fd_idx = host_fd_idx + 1;
            size_t room_idx = room_indices[r];
            nano_relay_room_t *room = &srv->rooms[room_idx];

            bool close_room = false;

            /* Host -> Controller */
            if (fds[host_fd_idx].revents & POLLIN) {
                ssize_t n = recv(room->host_sock, relay_buf, sizeof(relay_buf), 0);
                if (n <= 0) {
                    close_room = true;
                } else {
                    if (!nano_socket_send_all(room->ctrl_sock, relay_buf, (size_t)n, 2000)) {
                        close_room = true;
                    }
                }
            } else if (fds[host_fd_idx].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                close_room = true;
            }

            /* Controller -> Host */
            if (!close_room && (fds[ctrl_fd_idx].revents & POLLIN)) {
                ssize_t n = recv(room->ctrl_sock, relay_buf, sizeof(relay_buf), 0);
                if (n <= 0) {
                    close_room = true;
                } else {
                    if (!nano_socket_send_all(room->host_sock, relay_buf, (size_t)n, 2000)) {
                        close_room = true;
                    }
                }
            } else if (fds[ctrl_fd_idx].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                close_room = true;
            }

            if (close_room) {
                server_log(srv, "Closing relay room token: %llu", (unsigned long long)room->session_token);
                nano_socket_close(room->host_sock);
                nano_socket_close(room->ctrl_sock);
                memset(room, 0, sizeof(nano_relay_room_t));
                room->host_sock = NANO_INVALID_SOCKET;
                room->ctrl_sock = NANO_INVALID_SOCKET;
                room->is_active = false;
                srv->room_count--;
            }
        }
        pthread_mutex_unlock(&srv->lock);
    }

    if (nano_socket_is_valid(srv->listen_sock)) {
        nano_socket_close(srv->listen_sock);
        srv->listen_sock = NANO_INVALID_SOCKET;
    }

    server_log(srv, "Server stopped");
    return NULL;
}

bool nano_server_start(nano_server_t *srv) {
    if (!srv || srv->is_running) return false;
    nano_net_init();
    srv->is_running = true;
    if (pthread_create(&srv->thread, NULL, server_thread_func, srv) != 0) {
        srv->is_running = false;
        return false;
    }
    return true;
}

void nano_server_stop(nano_server_t *srv) {
    if (!srv || !srv->is_running) return;
    srv->is_running = false;
    pthread_join(srv->thread, NULL);

    pthread_mutex_lock(&srv->lock);
    for (size_t i = 0; i < NANO_MAX_PEERS; ++i) {
        if (srv->peers[i].is_active && nano_socket_is_valid(srv->peers[i].sock)) {
            nano_socket_close(srv->peers[i].sock);
            srv->peers[i].sock = NANO_INVALID_SOCKET;
            srv->peers[i].is_active = false;
        }
    }
    for (size_t i = 0; i < NANO_MAX_RELAY_ROOMS; ++i) {
        if (srv->rooms[i].is_active) {
            if (nano_socket_is_valid(srv->rooms[i].host_sock)) nano_socket_close(srv->rooms[i].host_sock);
            if (nano_socket_is_valid(srv->rooms[i].ctrl_sock)) nano_socket_close(srv->rooms[i].ctrl_sock);
            srv->rooms[i].is_active = false;
        }
    }
    pthread_mutex_unlock(&srv->lock);
}

void nano_server_destroy(nano_server_t *srv) {
    if (!srv) return;
    nano_server_stop(srv);
    pthread_mutex_destroy(&srv->lock);
    free(srv);
}
