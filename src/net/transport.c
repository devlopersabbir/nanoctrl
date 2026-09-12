#include "transport.h"
#include <stdlib.h>
#include <string.h>

bool nano_transport_send_msg(nano_socket_t sock,
                             nano_msg_type_t type,
                             nano_msg_flags_t flags,
                             const void *payload,
                             uint32_t payload_len,
                             int timeout_ms) {
    if (!nano_socket_is_valid(sock)) return false;

    nano_header_t hdr;
    nano_header_init(&hdr, type, flags, payload_len);

    uint8_t hdr_bytes[NANO_HEADER_SIZE];
    if (!nano_header_encode(&hdr, hdr_bytes, sizeof(hdr_bytes))) {
        return false;
    }

    if (!nano_socket_send_all(sock, hdr_bytes, NANO_HEADER_SIZE, timeout_ms)) {
        return false;
    }

    if (payload_len > 0 && payload) {
        if (!nano_socket_send_all(sock, payload, payload_len, timeout_ms)) {
            return false;
        }
    }

    return true;
}

bool nano_transport_recv_header(nano_socket_t sock,
                                nano_header_t *out_hdr,
                                int timeout_ms) {
    if (!nano_socket_is_valid(sock) || !out_hdr) return false;

    uint8_t hdr_bytes[NANO_HEADER_SIZE];
    if (!nano_socket_recv_all(sock, hdr_bytes, NANO_HEADER_SIZE, timeout_ms)) {
        return false;
    }

    return nano_header_decode(hdr_bytes, sizeof(hdr_bytes), out_hdr);
}

bool nano_transport_recv_payload(nano_socket_t sock,
                                 const nano_header_t *hdr,
                                 void *payload_buf,
                                 size_t buf_size,
                                 int timeout_ms) {
    if (!nano_socket_is_valid(sock) || !hdr || !payload_buf) return false;
    if (hdr->length > buf_size) return false;
    if (hdr->length == 0) return true;

    return nano_socket_recv_all(sock, payload_buf, hdr->length, timeout_ms);
}

bool nano_transport_recv_msg(nano_socket_t sock,
                             nano_header_t *out_hdr,
                             void *payload_buf,
                             size_t buf_size,
                             int timeout_ms) {
    if (!nano_transport_recv_header(sock, out_hdr, timeout_ms)) {
        return false;
    }

    return nano_transport_recv_payload(sock, out_hdr, payload_buf, buf_size, timeout_ms);
}
