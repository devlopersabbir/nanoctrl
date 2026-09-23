#ifndef NANO_UI_MAC_H
#define NANO_UI_MAC_H

#include "../../core/session.h"

int nano_ui_run_app(int argc, char *argv[]);
int nano_ui_run_host_app(uint16_t port, const char *pin);
int nano_ui_run_host_relay_app(const char *server_host, uint16_t server_port, uint32_t device_id, const char *pin);
int nano_ui_run_controller_app(const char *host, uint16_t port, const char *pin);
int nano_ui_run_controller_relay_app(const char *server_host, uint16_t server_port, uint32_t target_id, const char *pin);

void nano_ui_start_host_gui(uint16_t port, const char *pin);
void nano_ui_start_host_relay_gui(const char *server_host, uint16_t server_port, uint32_t device_id, const char *pin);
void nano_ui_start_controller_gui(const char *host, uint16_t port, const char *pin);
void nano_ui_start_controller_relay_gui(const char *server_host, uint16_t server_port, uint32_t target_id, const char *pin);

#endif /* NANO_UI_MAC_H */
