#include "socket.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>

bool nano_net_init(void) {
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    return true;
}

void nano_net_cleanup(void) {
    /* No-op on POSIX */
}

nano_socket_t nano_socket_create(void) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return NANO_INVALID_SOCKET;
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_NOSIGPIPE
    int set = 1;
    setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
#endif
    nano_socket_set_nodelay(sock, true);
    return sock;
}

bool nano_socket_bind_listen(nano_socket_t sock, const char *bind_addr, uint16_t port, int backlog) {
    if (!nano_socket_is_valid(sock)) return false;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (!bind_addr || strlen(bind_addr) == 0 || strcmp(bind_addr, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, bind_addr, &addr.sin_addr) <= 0) {
            return false;
        }
    }

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        return false;
    }

    if (listen(sock, backlog > 0 ? backlog : 5) < 0) {
        return false;
    }

    return true;
}

nano_socket_t nano_socket_accept(nano_socket_t server_sock, char *client_ip, size_t client_ip_len, uint16_t *client_port) {
    if (!nano_socket_is_valid(server_sock)) return NANO_INVALID_SOCKET;

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) return NANO_INVALID_SOCKET;

    nano_socket_set_nodelay(client_fd, true);

    if (client_ip && client_ip_len > 0) {
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, (socklen_t)client_ip_len);
    }
    if (client_port) {
        *client_port = ntohs(client_addr.sin_port);
    }

    return client_fd;
}

bool nano_socket_connect(nano_socket_t sock, const char *host, uint16_t port, int timeout_ms) {
    if (!nano_socket_is_valid(sock) || !host) return false;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        /* Resolve hostname */
        struct hostent *he = gethostbyname(host);
        if (!he || !he->h_addr_list[0]) return false;
        memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(addr.sin_addr));
    }

    if (timeout_ms <= 0) {
        return connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0;
    }

    /* Non-blocking connect with timeout */
    nano_socket_set_nonblocking(sock, true);
    int res = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (res == 0) {
        nano_socket_set_nonblocking(sock, false);
        return true;
    }
    if (errno != EINPROGRESS) {
        nano_socket_set_nonblocking(sock, false);
        return false;
    }

    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = POLLOUT;
    int poll_res = poll(&pfd, 1, timeout_ms);
    if (poll_res <= 0) {
        nano_socket_set_nonblocking(sock, false);
        return false;
    }

    int err = 0;
    socklen_t err_len = sizeof(err);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &err_len) < 0 || err != 0) {
        nano_socket_set_nonblocking(sock, false);
        return false;
    }

    nano_socket_set_nonblocking(sock, false);
    return true;
}

bool nano_socket_set_nonblocking(nano_socket_t sock, bool nonblocking) {
    if (!nano_socket_is_valid(sock)) return false;
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return false;
    if (nonblocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return fcntl(sock, F_SETFL, flags) == 0;
}

bool nano_socket_set_nodelay(nano_socket_t sock, bool nodelay) {
    if (!nano_socket_is_valid(sock)) return false;
    int flag = nodelay ? 1 : 0;
    return setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) == 0;
}

bool nano_socket_send_all(nano_socket_t sock, const void *buf, size_t len, int timeout_ms) {
    if (!nano_socket_is_valid(sock) || !buf) return false;
    const uint8_t *ptr = (const uint8_t *)buf;
    size_t remaining = len;

    while (remaining > 0) {
        if (timeout_ms > 0) {
            struct pollfd pfd;
            pfd.fd = sock;
            pfd.events = POLLOUT;
            int ret = poll(&pfd, 1, timeout_ms);
            if (ret <= 0) return false;
        }

        ssize_t sent = send(sock, ptr, remaining, 0);
        if (sent <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }
            return false;
        }
        ptr += sent;
        remaining -= (size_t)sent;
    }
    return true;
}

bool nano_socket_recv_all(nano_socket_t sock, void *buf, size_t len, int timeout_ms) {
    if (!nano_socket_is_valid(sock) || !buf) return false;
    uint8_t *ptr = (uint8_t *)buf;
    size_t remaining = len;

    while (remaining > 0) {
        if (timeout_ms > 0) {
            struct pollfd pfd;
            pfd.fd = sock;
            pfd.events = POLLIN;
            int ret = poll(&pfd, 1, timeout_ms);
            if (ret <= 0) return false;
        }

        ssize_t received = recv(sock, ptr, remaining, 0);
        if (received <= 0) {
            if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                continue;
            }
            return false;
        }
        ptr += received;
        remaining -= (size_t)received;
    }
    return true;
}

void nano_socket_close(nano_socket_t sock) {
    if (nano_socket_is_valid(sock)) {
        close(sock);
    }
}

bool nano_socket_is_valid(nano_socket_t sock) {
    return sock >= 0;
}
