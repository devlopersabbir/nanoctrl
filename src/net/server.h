#ifndef NANO_SERVER_H
#define NANO_SERVER_H

#include "socket.h"
#include "protocol.h"
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#define NANO_MAX_PEERS 1024
#define NANO_MAX_RELAY_ROOMS 512

typedef struct {
    uint32_t device_id;
    nano_socket_t sock;
    uint64_t last_seen_ms;
    bool is_active;
} nano_peer_entry_t;

typedef struct {
    uint64_t session_token;
    nano_socket_t host_sock;
    nano_socket_t ctrl_sock;
    bool host_ready;
    bool ctrl_ready;
    bool is_active;
    uint64_t created_at_ms;
} nano_relay_room_t;

typedef struct nano_server {
    nano_socket_t listen_sock;
    uint16_t port;
    volatile bool is_running;
    pthread_t thread;
    pthread_mutex_t lock;

    nano_peer_entry_t peers[NANO_MAX_PEERS];
    size_t peer_count;

    nano_relay_room_t rooms[NANO_MAX_RELAY_ROOMS];
    size_t room_count;

    /* Callback for logging */
    void (*on_log)(const char *msg, void *user_data);
    void *user_data;
} nano_server_t;

nano_server_t *nano_server_create(uint16_t port);
bool nano_server_start(nano_server_t *srv);
void nano_server_stop(nano_server_t *srv);
void nano_server_destroy(nano_server_t *srv);

/* Formats 9-digit Device ID e.g. "842 190 345" */
void nano_format_device_id(uint32_t id, char *out_buf, size_t buf_size);
/* Parses formatted or numeric Device ID string into uint32_t */
uint32_t nano_parse_device_id(const char *id_str);

#endif /* NANO_SERVER_H */
