#ifndef NANO_UI_MAC_H
#define NANO_UI_MAC_H

#include "../../core/session.h"

int nano_ui_run_app(int argc, char *argv[]);
int nano_ui_run_host_app(uint16_t port, const char *pin);
int nano_ui_run_controller_app(const char *host, uint16_t port, const char *pin);

#endif /* NANO_UI_MAC_H */
