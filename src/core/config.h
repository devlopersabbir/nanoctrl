#ifndef NANO_CONFIG_H
#define NANO_CONFIG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define NANO_CONFIG_MAX_HOST 128

typedef struct {
    char relay_host[NANO_CONFIG_MAX_HOST];
    uint16_t relay_port;
} nano_config_t;

/* Returns the resolved path to the config file for the current OS. */
bool nano_config_get_path(char *out_path, size_t max_len);

/* Loads config from disk, falling back to NANOCTRL_SERVER env, then default (127.0.0.1:7443). */
bool nano_config_load(nano_config_t *cfg);

/* Saves config to disk, creating parent directories if needed. */
bool nano_config_save(const nano_config_t *cfg);

/* Helper to get the active server host and port. */
void nano_config_get_server(char *out_host, size_t max_host, uint16_t *out_port);

/* Helper to set and persist active server host and port. */
bool nano_config_set_server(const char *host, uint16_t port);

/* Helper to parse and persist "host:port" or "host" string. */
bool nano_config_set_server_string(const char *server_str);

#endif /* NANO_CONFIG_H */
