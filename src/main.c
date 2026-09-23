#include "core/protocol.h"
#include "core/session.h"
#include "crypto/auth.h"
#include "crypto/random.h"
#include "net/server.h"
#include "platform/macos/ui_mac.h"
#include "input/input.h"
#include "screen/capture.h"
#include "core/version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static volatile bool g_running = true;
static bool g_auto_accept = false;

static void sigint_handler(int sig) {
    (void)sig;
    g_running = false;
}

static void cli_state_change(nano_session_t *s, nano_session_state_t state, const char *msg, void *user_data) {
    (void)s; (void)user_data;
    switch (state) {
        case SESSION_STATE_LISTENING:
            printf("[NANOCTRL] Status: WAITING FOR CONTROLLER...\n");
            break;
        case SESSION_STATE_CONNECTING:
            printf("[NANOCTRL] Status: CONNECTING... (%s)\n", msg ? msg : "");
            break;
        case SESSION_STATE_PENDING_APPROVAL:
            printf("[NANOCTRL] Status: CONNECTION REQUEST from %s\n", msg ? msg : "remote");
            break;
        case SESSION_STATE_ACTIVE:
            printf("[NANOCTRL] Status: ● REMOTE SESSION ACTIVE (%s)\n", msg ? msg : "");
            break;
        case SESSION_STATE_DISCONNECTED:
            printf("[NANOCTRL] Status: DISCONNECTED (%s)\n", msg ? msg : "");
            break;
        default:
            break;
    }
}

static bool cli_approval(nano_session_t *s, const char *remote_ip, void *user_data) {
    (void)s; (void)user_data;
    if (g_auto_accept) {
        printf("[NANOCTRL] Auto-accepting connection from %s\n", remote_ip ? remote_ip : "remote");
        return true;
    }

    printf("\n>>> Remote Control Request <<<\n");
    printf("A controller (%s) wants to control this computer.\n", remote_ip ? remote_ip : "unknown");
    printf("Accept connection? [y/N]: ");
    fflush(stdout);

    char line[16];
    if (fgets(line, sizeof(line), stdin)) {
        if (line[0] == 'y' || line[0] == 'Y') {
            return true;
        }
    }
    return false;
}

static void cli_frame_update(nano_session_t *s, const nano_frame_t *frame, void *user_data) {
    (void)s; (void)user_data;
    static uint32_t frame_count = 0;
    frame_count++;
    if (frame_count % 30 == 0) {
        printf("[NANOCTRL] Received frame %u: %ux%u (%u bytes)\n",
               frame_count, frame->width, frame->height, frame->size);
    }
}

static void print_version(void) {
    printf("NANOCTRL v%s (commit: %s, arch: %s)\n",
           nano_version_string(), nano_git_commit(), nano_build_arch());
}

static void print_usage(const char *prog) {
    printf("NANOCTRL v%s - Tiny Remote Control, Nothing Else.\n\n", nano_version_string());
    printf("Usage:\n");
    printf("  %s                              Launch native macOS GUI\n", prog);
    printf("  %s --host [port] [--pin 123456] [--server-addr host:port] [--id 123456789] [--cli] [--auto-accept]\n", prog);
    printf("  %s --controller <target> <pin> [--server-addr host:port] [--cli]\n", prog);
    printf("  %s --server [port]              Run standalone Rendezvous & Relay server\n", prog);
    printf("  %s --test                       Run built-in self-diagnostics\n", prog);
    printf("  %s --version, -v                Print version information\n", prog);
    printf("  %s --help, -h                   Show this help\n\n", prog);
    printf("Examples:\n");
    printf("  # 1. Run self-hosted relay server on your VPS:\n");
    printf("  %s --server 7443\n\n", prog);
    printf("  # 2. Host behind NAT via self-hosted server:\n");
    printf("  %s --host --server-addr vps.example.com:7443 --cli\n\n", prog);
    printf("  # 3. Control host via self-hosted server with Device ID & PIN:\n");
    printf("  %s --controller 842190345 582914 --server-addr vps.example.com:7443 --cli\n\n", prog);
    printf("  # 4. Direct LAN connection:\n");
    printf("  %s --controller 192.168.1.50:7443 582914 --cli\n", prog);
}

static int run_cli_server(uint16_t port) {
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    nano_server_t *srv = nano_server_create(port);
    if (!srv) {
        fprintf(stderr, "Error: Failed to allocate server instance.\n");
        return 1;
    }

    if (!nano_server_start(srv)) {
        fprintf(stderr, "Error: Failed to start server on port %u.\n", port);
        nano_server_destroy(srv);
        return 1;
    }

    printf("====================================================\n");
    printf("  NANOCTRL Rendezvous & Relay Server                \n");
    printf("  Port:     %u (TCP)                                \n", port);
    printf("  Status:   RUNNING (Press Ctrl+C to stop)          \n");
    printf("====================================================\n\n");

    while (g_running && srv->is_running) {
        sleep(1);
    }

    printf("\n[NANOCTRL] Stopping server...\n");
    nano_server_stop(srv);
    nano_server_destroy(srv);
    printf("[NANOCTRL] Server stopped.\n");
    return 0;
}

static int run_cli_host(uint16_t port, const char *server_addr, uint32_t custom_id, const char *custom_pin) {
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    nano_session_t *s = nano_session_create(NANO_ROLE_HOST);
    if (!s) {
        fprintf(stderr, "Error: Failed to create host session\n");
        return 1;
    }

    s->on_state_change = cli_state_change;
    s->on_approval_request = cli_approval;

    if (server_addr && strlen(server_addr) > 0) {
        char srv_host[128] = "127.0.0.1";
        uint16_t srv_port = NANO_DEFAULT_PORT;
        char tmp[128];
        strncpy(tmp, server_addr, sizeof(tmp) - 1);
        char *colon = strchr(tmp, ':');
        if (colon) {
            *colon = '\0';
            strncpy(srv_host, tmp, sizeof(srv_host) - 1);
            srv_port = (uint16_t)atoi(colon + 1);
        } else {
            strncpy(srv_host, server_addr, sizeof(srv_host) - 1);
        }

        if (!nano_session_start_host_relay(s, srv_host, srv_port, custom_id, custom_pin)) {
            fprintf(stderr, "Error: Failed to start host relay session\n");
            nano_session_destroy(s);
            return 1;
        }

        char id_str[32];
        nano_format_device_id(s->device_id, id_str, sizeof(id_str));

        printf("\n========================================\n");
        printf("               NANOCTRL                 \n");
        printf("========================================\n");
        printf("  Role:       HOST (Self-Hosted Relay)  \n");
        printf("  Server:     %s:%u                     \n", srv_host, srv_port);
        printf("  Device ID:  %s                        \n", id_str);
        printf("  PIN Code:   %.3s %.3s                 \n", s->pin, s->pin + 3);
        printf("  Screen:     %s                        \n",
               nano_capture_has_permission() ? "Permission Granted" : "Mock Fallback (Grant in System Settings)");
        printf("  Input:      %s                        \n",
               nano_input_has_permission() ? "Accessibility Trusted" : "Needs Accessibility Permission");
        printf("========================================\n");
        printf("Press Ctrl+C to terminate host.\n\n");
    } else {
        if (!nano_session_start_host(s, port, custom_pin)) {
            fprintf(stderr, "Error: Failed to start host on port %u\n", port);
            nano_session_destroy(s);
            return 1;
        }

        char id_str[32];
        nano_format_device_id(s->device_id, id_str, sizeof(id_str));

        printf("\n========================================\n");
        printf("               NANOCTRL                 \n");
        printf("========================================\n");
        printf("  Role:       HOST (Direct LAN)         \n");
        printf("  Port:       %u                        \n", s->port);
        printf("  Device ID:  %s                        \n", id_str);
        printf("  PIN Code:   %.3s %.3s                 \n", s->pin, s->pin + 3);
        printf("  Screen:     %s                        \n",
               nano_capture_has_permission() ? "Permission Granted" : "Mock Fallback (Grant in System Settings)");
        printf("  Input:      %s                        \n",
               nano_input_has_permission() ? "Accessibility Trusted" : "Needs Accessibility Permission");
        printf("========================================\n");
        printf("Press Ctrl+C to terminate host.\n\n");
    }

    while (g_running && s->state != SESSION_STATE_DISCONNECTED) {
        sleep(1);
    }

    printf("\n[NANOCTRL] Stopping host session...\n");
    nano_session_stop(s);
    nano_session_destroy(s);
    printf("[NANOCTRL] Done.\n");
    return 0;
}

static int run_cli_controller(const char *target_str, const char *server_addr, const char *pin) {
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    nano_session_t *s = nano_session_create(NANO_ROLE_CONTROLLER);
    if (!s) {
        fprintf(stderr, "Error: Failed to create controller session\n");
        return 1;
    }

    s->on_state_change = cli_state_change;
    s->on_frame_update = cli_frame_update;

    if (strchr(target_str, ':') != NULL) {
        /* Direct LAN Mode */
        char host[128] = "127.0.0.1";
        uint16_t port = NANO_DEFAULT_PORT;
        char tmp[128];
        strncpy(tmp, target_str, sizeof(tmp) - 1);
        char *colon = strchr(tmp, ':');
        if (colon) {
            *colon = '\0';
            strncpy(host, tmp, sizeof(host) - 1);
            port = (uint16_t)atoi(colon + 1);
        } else {
            strncpy(host, target_str, sizeof(host) - 1);
        }

        printf("[NANOCTRL] Connecting directly to %s:%u with PIN %.3s %.3s...\n", host, port, pin, pin + 3);
        if (!nano_session_start_controller(s, host, port, pin)) {
            fprintf(stderr, "Error: Failed to start controller\n");
            nano_session_destroy(s);
            return 1;
        }
    } else {
        /* Self-Hosted Relay Mode by Device ID */
        uint32_t target_id = nano_parse_device_id(target_str);
        if (target_id == 0) {
            fprintf(stderr, "Error: Invalid Device ID '%s'\n", target_str);
            nano_session_destroy(s);
            return 1;
        }

        char srv_host[128] = "127.0.0.1";
        uint16_t srv_port = NANO_DEFAULT_PORT;
        if (server_addr && strlen(server_addr) > 0) {
            char tmp[128];
            strncpy(tmp, server_addr, sizeof(tmp) - 1);
            char *colon = strchr(tmp, ':');
            if (colon) {
                *colon = '\0';
                strncpy(srv_host, tmp, sizeof(srv_host) - 1);
                srv_port = (uint16_t)atoi(colon + 1);
            } else {
                strncpy(srv_host, server_addr, sizeof(srv_host) - 1);
            }
        }

        char id_str[32];
        nano_format_device_id(target_id, id_str, sizeof(id_str));
        printf("[NANOCTRL] Connecting via relay %s:%u to Device ID %s (PIN: %.3s %.3s)...\n",
               srv_host, srv_port, id_str, pin, pin + 3);

        if (!nano_session_start_controller_relay(s, srv_host, srv_port, target_id, pin)) {
            fprintf(stderr, "Error: Failed to start controller relay session\n");
            nano_session_destroy(s);
            return 1;
        }
    }

    while (g_running && s->state != SESSION_STATE_DISCONNECTED) {
        sleep(1);
    }

    printf("\n[NANOCTRL] Stopping controller session...\n");
    nano_session_stop(s);
    nano_session_destroy(s);
    return 0;
}

static int run_diagnostics(void) {
    printf("=== NANOCTRL Self-Diagnostics ===\n");

    /* 1. Random PIN */
    char pin[7];
    nano_random_pin(pin);
    printf("[PASS] Random 6-digit PIN generator: %s\n", pin);

    /* 2. SHA-256 and HMAC */
    uint8_t nonce[32] = { 0x01, 0x02, 0x03, 0x04 };
    uint8_t hash[32];
    nano_auth_compute_response(pin, nonce, hash);
    bool verified = nano_auth_verify_response(pin, nonce, hash);
    bool bad_verified = nano_auth_verify_response("000000", nonce, hash);
    if (verified && !bad_verified) {
        printf("[PASS] Crypto HMAC-SHA256 challenge-response verification\n");
    } else {
        printf("[FAIL] Crypto HMAC verification error\n");
        return 1;
    }

    /* 3. Screen Permissions */
    printf("[%s] macOS Screen Recording Permission\n",
           nano_capture_has_permission() ? "PASS" : "WARN");

    /* 4. Accessibility Permissions */
    printf("[%s] macOS Accessibility Input Injection Permission\n",
           nano_input_has_permission() ? "PASS" : "WARN");

    /* 5. Device ID Formatting */
    uint32_t dev_id = 842190345;
    char id_buf[32];
    nano_format_device_id(dev_id, id_buf, sizeof(id_buf));
    uint32_t parsed_id = nano_parse_device_id(id_buf);
    if (parsed_id == dev_id && strcmp(id_buf, "842 190 345") == 0) {
        printf("[PASS] 9-digit Device ID formatting & parsing (%s)\n", id_buf);
    } else {
        printf("[FAIL] Device ID format/parse mismatch\n");
        return 1;
    }

    /* 6. Frame & Diff Test */
    nano_frame_t f1, f2;
    nano_frame_alloc(&f1, 128, 128);
    nano_frame_alloc(&f2, 128, 128);
    nano_frame_generate_test_pattern(&f1, 0);
    nano_frame_generate_test_pattern(&f2, 1);
    bool dirty = nano_tile_is_dirty(&f2, &f1, 0, 0, NANO_TILE_SIZE, NANO_TILE_SIZE);
    bool clean = nano_tile_is_dirty(&f1, &f1, 0, 0, NANO_TILE_SIZE, NANO_TILE_SIZE);
    nano_frame_free(&f1);
    nano_frame_free(&f2);

    if (dirty && !clean) {
        printf("[PASS] Tile diff engine\n");
    } else {
        printf("[FAIL] Tile diff engine check failed\n");
        return 1;
    }

    printf("\nAll diagnostics passed successfully!\n");
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        /* Launch native macOS GUI */
        return nano_ui_run_app(argc, argv);
    }

    uint16_t port = NANO_DEFAULT_PORT;
    const char *custom_pin = NULL;
    const char *target = NULL;
    const char *server_addr = NULL;
    uint32_t custom_id = 0;
    bool is_host = false;
    bool is_controller = false;
    bool is_server = false;
    bool cli_mode = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--server") == 0) {
            is_server = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                port = (uint16_t)atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "--host") == 0) {
            is_host = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                port = (uint16_t)atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "--controller") == 0) {
            is_controller = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                target = argv[++i];
            }
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                custom_pin = argv[++i];
            }
        } else if (strcmp(argv[i], "--server-addr") == 0) {
            if (i + 1 < argc) {
                server_addr = argv[++i];
            }
        } else if (strcmp(argv[i], "--id") == 0) {
            if (i + 1 < argc) {
                custom_id = nano_parse_device_id(argv[++i]);
            }
        } else if (strcmp(argv[i], "--pin") == 0) {
            if (i + 1 < argc) {
                custom_pin = argv[++i];
            }
        } else if (strcmp(argv[i], "--cli") == 0) {
            cli_mode = true;
        } else if (strcmp(argv[i], "--auto-accept") == 0) {
            g_auto_accept = true;
        } else if (strcmp(argv[i], "--test") == 0) {
            return run_diagnostics();
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (is_server) {
        return run_cli_server(port);
    }

    if (is_host) {
        if (cli_mode) {
            return run_cli_host(port, server_addr, custom_id, custom_pin);
        } else {
            if (server_addr) {
                char srv_host[128] = "127.0.0.1";
                uint16_t srv_port = NANO_DEFAULT_PORT;
                char tmp[128];
                strncpy(tmp, server_addr, sizeof(tmp) - 1);
                char *colon = strchr(tmp, ':');
                if (colon) {
                    *colon = '\0';
                    strncpy(srv_host, tmp, sizeof(srv_host) - 1);
                    srv_port = (uint16_t)atoi(colon + 1);
                } else {
                    strncpy(srv_host, server_addr, sizeof(srv_host) - 1);
                }
                return nano_ui_run_host_relay_app(srv_host, srv_port, custom_id, custom_pin);
            } else {
                return nano_ui_run_host_app(port, custom_pin);
            }
        }
    }

    if (is_controller && target && custom_pin) {
        if (cli_mode) {
            return run_cli_controller(target, server_addr, custom_pin);
        } else {
            if (strchr(target, ':') != NULL) {
                char host[128] = "127.0.0.1";
                uint16_t cport = NANO_DEFAULT_PORT;
                char tmp[128];
                strncpy(tmp, target, sizeof(tmp) - 1);
                char *colon = strchr(tmp, ':');
                if (colon) {
                    *colon = '\0';
                    strncpy(host, tmp, sizeof(host) - 1);
                    cport = (uint16_t)atoi(colon + 1);
                } else {
                    strncpy(host, target, sizeof(host) - 1);
                }
                return nano_ui_run_controller_app(host, cport, custom_pin);
            } else {
                uint32_t tid = nano_parse_device_id(target);
                char srv_host[128] = "127.0.0.1";
                uint16_t srv_port = NANO_DEFAULT_PORT;
                if (server_addr) {
                    char tmp[128];
                    strncpy(tmp, server_addr, sizeof(tmp) - 1);
                    char *colon = strchr(tmp, ':');
                    if (colon) {
                        *colon = '\0';
                        strncpy(srv_host, tmp, sizeof(srv_host) - 1);
                        srv_port = (uint16_t)atoi(colon + 1);
                    } else {
                        strncpy(srv_host, server_addr, sizeof(srv_host) - 1);
                    }
                }
                return nano_ui_run_controller_relay_app(srv_host, srv_port, tid, custom_pin);
            }
        }
    }

    print_usage(argv[0]);
    return 1;
}
