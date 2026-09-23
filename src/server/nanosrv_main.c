#include "../net/server.h"
#include "../core/version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static volatile bool g_srv_running = true;

static void handle_signal(int sig) {
    (void)sig;
    g_srv_running = false;
}

static void print_banner(uint16_t port) {
    printf("====================================================\n");
    printf("  NANOCTRL Rendezvous & Relay Server (nanosrv)\n");
    printf("  Version: %s | Zero-Dependency C Server\n", nano_version_string());
    printf("====================================================\n");
    printf("  Port:     %u (TCP)\n", port);
    printf("  Features: Device ID Registry, WAN NAT Relay Tunneling\n");
    printf("  Status:   RUNNING (Press Ctrl+C to stop)\n");
    printf("====================================================\n\n");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    uint16_t port = NANO_DEFAULT_PORT;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [port] [--help] [--version]\n", argv[0]);
            printf("Default port: %u\n", NANO_DEFAULT_PORT);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("nanosrv v%s (%s)\n", nano_version_string(), nano_git_commit());
            return 0;
        } else {
            int p = atoi(argv[i]);
            if (p > 0 && p <= 65535) {
                port = (uint16_t)p;
            }
        }
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

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

    print_banner(port);

    while (g_srv_running && srv->is_running) {
        sleep(1);
    }

    printf("\nShutting down NANOCTRL server...\n");
    nano_server_stop(srv);
    nano_server_destroy(srv);
    printf("Server stopped cleanly.\n");
    return 0;
}
