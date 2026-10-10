#include "../src/core/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

int main(void) {
    printf("Running Cross-Platform Configuration Tests...\n");

    /* 1. Test config path resolution */
    char path[1024];
    assert(nano_config_get_path(path, sizeof(path)));
    assert(strlen(path) > 0);
    printf("  [PASS] Config path resolved: %s\n", path);

    /* 2. Test saving and loading config */
    nano_config_t cfg_save;
    strncpy(cfg_save.relay_host, "vps.example.com", sizeof(cfg_save.relay_host) - 1);
    cfg_save.relay_port = 8443;
    assert(nano_config_save(&cfg_save));

    nano_config_t cfg_load;
    assert(nano_config_load(&cfg_load));
    assert(strcmp(cfg_load.relay_host, "vps.example.com") == 0);
    assert(cfg_load.relay_port == 8443);
    printf("  [PASS] Saved & Loaded server correctly: %s:%u\n", cfg_load.relay_host, cfg_load.relay_port);

    /* 3. Test string parsing and setting */
    assert(nano_config_set_server_string("relay.custom-domain.org:9000"));
    char host[128];
    uint16_t port = 0;
    nano_config_get_server(host, sizeof(host), &port);
    assert(strcmp(host, "relay.custom-domain.org") == 0);
    assert(port == 9000);
    printf("  [PASS] Endpoint parser host:port (%s:%u)\n", host, port);

    /* 4. Test string parsing without explicit port (defaults to 7443) */
    assert(nano_config_set_server_string("192.168.1.100"));
    nano_config_get_server(host, sizeof(host), &port);
    assert(strcmp(host, "192.168.1.100") == 0);
    assert(port == 7443);
    printf("  [PASS] Endpoint parser default port fallback (%s:%u)\n", host, port);

    /* 5. Test environment variable override */
    setenv("NANOCTRL_SERVER", "env-relay.internal:7777", 1);
    nano_config_load(&cfg_load);
    assert(strcmp(cfg_load.relay_host, "env-relay.internal") == 0);
    assert(cfg_load.relay_port == 7777);
    unsetenv("NANOCTRL_SERVER");
    printf("  [PASS] Environment variable NANOCTRL_SERVER override\n");

    /* 6. Restore default for clean state */
    nano_config_set_server("127.0.0.1", 7443);

    printf("ALL CONFIGURATION TESTS PASSED!\n");
    return 0;
}
