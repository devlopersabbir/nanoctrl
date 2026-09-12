#ifndef NANO_SOCKET_H
#define NANO_SOCKET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef _WIN32
  #include <winsock2.h>
  typedef SOCKET nano_socket_t;
  #define NANO_INVALID_SOCKET INVALID_SOCKET
#else
  typedef int nano_socket_t;
  #define NANO_INVALID_SOCKET (-1)
#endif

bool nano_net_init(void);
void nano_net_cleanup(void);

nano_socket_t nano_socket_create(void);
bool nano_socket_bind_listen(nano_socket_t sock, const char *bind_addr, uint16_t port, int backlog);
nano_socket_t nano_socket_accept(nano_socket_t server_sock, char *client_ip, size_t client_ip_len, uint16_t *client_port);
bool nano_socket_connect(nano_socket_t sock, const char *host, uint16_t port, int timeout_ms);
bool nano_socket_set_nonblocking(nano_socket_t sock, bool nonblocking);
bool nano_socket_set_nodelay(nano_socket_t sock, bool nodelay);

/* Send/receive exact byte counts with timeout */
bool nano_socket_send_all(nano_socket_t sock, const void *buf, size_t len, int timeout_ms);
bool nano_socket_recv_all(nano_socket_t sock, void *buf, size_t len, int timeout_ms);

void nano_socket_close(nano_socket_t sock);
bool nano_socket_is_valid(nano_socket_t sock);

#endif /* NANO_SOCKET_H */
