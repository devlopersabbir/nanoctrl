#include "config.h"
#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#define nano_mkdir(path) _mkdir(path)
#define PATH_SEP '\\'
#else
#include <unistd.h>
#define nano_mkdir(path) mkdir(path, 0755)
#define PATH_SEP '/'
#endif

static void trim_whitespace(char *str) {
    if (!str) return;
    char *end;

    /* Trim leading space */
    while (isspace((unsigned char)*str)) str++;

    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
}

static bool parse_server_endpoint(const char *endpoint, char *out_host, size_t max_host, uint16_t *out_port) {
    if (!endpoint || strlen(endpoint) == 0) return false;

    char tmp[NANO_CONFIG_MAX_HOST + 32];
    strncpy(tmp, endpoint, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    trim_whitespace(tmp);

    if (strlen(tmp) == 0) return false;

    char *colon = strrchr(tmp, ':');
    if (colon) {
        *colon = '\0';
        int p = atoi(colon + 1);
        if (p > 0 && p <= 65535) {
            *out_port = (uint16_t)p;
        } else {
            *out_port = NANO_DEFAULT_PORT;
        }
    } else {
        *out_port = NANO_DEFAULT_PORT;
    }

    if (strlen(tmp) > 0) {
        strncpy(out_host, tmp, max_host - 1);
        out_host[max_host - 1] = '\0';
        return true;
    }

    return false;
}

static bool ensure_parent_dir(const char *file_path) {
    if (!file_path) return false;

    char dir[1024];
    strncpy(dir, file_path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';

    char *last_sep = strrchr(dir, PATH_SEP);
#if defined(_WIN32)
    if (!last_sep) last_sep = strrchr(dir, '/');
#endif

    if (!last_sep) return true;
    *last_sep = '\0';

    /* Create each folder component in directory */
    for (char *p = dir + 1; *p; p++) {
        if (*p == PATH_SEP
#if defined(_WIN32)
            || *p == '/'
#endif
        ) {
            char orig = *p;
            *p = '\0';
            nano_mkdir(dir);
            *p = orig;
        }
    }
    nano_mkdir(dir);
    return true;
}

bool nano_config_get_path(char *out_path, size_t max_len) {
    if (!out_path || max_len == 0) return false;

#if defined(_WIN32)
    const char *appdata = getenv("APPDATA");
    if (appdata && strlen(appdata) > 0) {
        snprintf(out_path, max_len, "%s\\nanoctrl\\config", appdata);
        return true;
    }
    const char *userprofile = getenv("USERPROFILE");
    if (userprofile && strlen(userprofile) > 0) {
        snprintf(out_path, max_len, "%s\\.nanoctrl\\config", userprofile);
        return true;
    }
    snprintf(out_path, max_len, "nanoctrl.conf");
    return true;

#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    if (home && strlen(home) > 0) {
        /* If user already has ~/.config/nanoctrl/config, honor it */
        char xdg_candidate[1024];
        snprintf(xdg_candidate, sizeof(xdg_candidate), "%s/.config/nanoctrl/config", home);
        struct stat st;
        if (stat(xdg_candidate, &st) == 0) {
            strncpy(out_path, xdg_candidate, max_len - 1);
            out_path[max_len - 1] = '\0';
            return true;
        }

        /* Default macOS user application support directory */
        snprintf(out_path, max_len, "%s/Library/Application Support/nanoctrl/config", home);
        return true;
    }
    snprintf(out_path, max_len, "./nanoctrl.conf");
    return true;

#else
    /* Linux & other POSIX (XDG Base Directory specification) */
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && strlen(xdg) > 0) {
        snprintf(out_path, max_len, "%s/nanoctrl/config", xdg);
        return true;
    }

    const char *home = getenv("HOME");
    if (home && strlen(home) > 0) {
        snprintf(out_path, max_len, "%s/.config/nanoctrl/config", home);
        return true;
    }

    snprintf(out_path, max_len, "./nanoctrl.conf");
    return true;
#endif
}

bool nano_config_load(nano_config_t *cfg) {
    if (!cfg) return false;

    /* 1. Built-in defaults */
    strncpy(cfg->relay_host, "127.0.0.1", sizeof(cfg->relay_host) - 1);
    cfg->relay_host[sizeof(cfg->relay_host) - 1] = '\0';
    cfg->relay_port = NANO_DEFAULT_PORT;

    /* 2. Read from config file if available */
    char path[1024];
    if (nano_config_get_path(path, sizeof(path))) {
        FILE *fp = fopen(path, "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                char *eq = strchr(line, '=');
                if (!eq) continue;

                *eq = '\0';
                char *key = line;
                char *val = eq + 1;

                trim_whitespace(key);
                trim_whitespace(val);

                if (key[0] == '#' || key[0] == ';' || key[0] == '\0') {
                    continue;
                }

                if (strcmp(key, "server") == 0 || strcmp(key, "relay_server") == 0) {
                    char h[NANO_CONFIG_MAX_HOST];
                    uint16_t p = NANO_DEFAULT_PORT;
                    if (parse_server_endpoint(val, h, sizeof(h), &p)) {
                        strncpy(cfg->relay_host, h, sizeof(cfg->relay_host) - 1);
                        cfg->relay_port = p;
                    }
                }
            }
            fclose(fp);
        }
    }

    /* 3. Environment variable override (NANOCTRL_SERVER) */
    const char *env_srv = getenv("NANOCTRL_SERVER");
    if (env_srv && strlen(env_srv) > 0) {
        char h[NANO_CONFIG_MAX_HOST];
        uint16_t p = NANO_DEFAULT_PORT;
        if (parse_server_endpoint(env_srv, h, sizeof(h), &p)) {
            strncpy(cfg->relay_host, h, sizeof(cfg->relay_host) - 1);
            cfg->relay_port = p;
        }
    }

    return true;
}

bool nano_config_save(const nano_config_t *cfg) {
    if (!cfg) return false;

    char path[1024];
    if (!nano_config_get_path(path, sizeof(path))) return false;

    if (!ensure_parent_dir(path)) return false;

    FILE *fp = fopen(path, "w");
    if (!fp) return false;

    fprintf(fp, "# NANOCTRL Configuration\n");
    fprintf(fp, "# Auto-generated and synced across Host & Controller\n");
    fprintf(fp, "server=%s:%u\n", cfg->relay_host, cfg->relay_port);
    fclose(fp);
    return true;
}

void nano_config_get_server(char *out_host, size_t max_host, uint16_t *out_port) {
    nano_config_t cfg;
    nano_config_load(&cfg);

    if (out_host && max_host > 0) {
        strncpy(out_host, cfg.relay_host, max_host - 1);
        out_host[max_host - 1] = '\0';
    }
    if (out_port) {
        *out_port = cfg.relay_port;
    }
}

bool nano_config_set_server(const char *host, uint16_t port) {
    if (!host || strlen(host) == 0) return false;

    nano_config_t cfg;
    nano_config_load(&cfg);

    strncpy(cfg.relay_host, host, sizeof(cfg.relay_host) - 1);
    cfg.relay_host[sizeof(cfg.relay_host) - 1] = '\0';
    cfg.relay_port = port > 0 ? port : NANO_DEFAULT_PORT;

    return nano_config_save(&cfg);
}

bool nano_config_set_server_string(const char *server_str) {
    if (!server_str) return false;

    char h[NANO_CONFIG_MAX_HOST];
    uint16_t p = NANO_DEFAULT_PORT;
    if (!parse_server_endpoint(server_str, h, sizeof(h), &p)) {
        return false;
    }

    return nano_config_set_server(h, p);
}
