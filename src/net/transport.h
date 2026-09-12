#ifndef NANO_TRANSPORT_H
#define NANO_TRANSPORT_H

#include "socket.h"
#include "protocol.h"

bool nano_transport_send_msg(nano_socket_t sock,
                             nano_msg_type_t type,
                             nano_msg_flags_t flags,
                             const void *payload,
                             uint32_t payload_len,
                             int timeout_ms);

bool nano_transport_recv_header(nano_socket_t sock,
                                nano_header_t *out_hdr,
                                int timeout_ms);

bool nano_transport_recv_payload(nano_socket_t sock,
                                 const nano_header_t *hdr,
                                 void *payload_buf,
                                 size_t buf_size,
                                 int timeout_ms);

bool nano_transport_recv_msg(nano_socket_t sock,
                             nano_header_t *out_hdr,
                             void *payload_buf,
                             size_t buf_size,
                             int timeout_ms);

#endif /* NANO_TRANSPORT_H */
