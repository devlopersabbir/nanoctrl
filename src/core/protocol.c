#include "protocol.h"
#include <arpa/inet.h>
#include <string.h>

void nano_header_init(nano_header_t *hdr, nano_msg_type_t type, nano_msg_flags_t flags, uint32_t length) {
    if (!hdr) return;
    hdr->magic = NANO_MAGIC;
    hdr->version = NANO_VERSION;
    hdr->type = (uint8_t)type;
    hdr->flags = (uint8_t)flags;
    hdr->length = length;
}

bool nano_header_encode(const nano_header_t *hdr, uint8_t *buf, size_t buf_len) {
    if (!hdr || !buf || buf_len < NANO_HEADER_SIZE) return false;
    buf[0] = hdr->magic;
    buf[1] = hdr->version;
    buf[2] = hdr->type;
    buf[3] = hdr->flags;
    uint32_t net_len = htonl(hdr->length);
    memcpy(buf + 4, &net_len, sizeof(net_len));
    return true;
}

bool nano_header_decode(const uint8_t *buf, size_t buf_len, nano_header_t *hdr) {
    if (!buf || !hdr || buf_len < NANO_HEADER_SIZE) return false;
    if (buf[0] != NANO_MAGIC || buf[1] != NANO_VERSION) return false;
    hdr->magic = buf[0];
    hdr->version = buf[1];
    hdr->type = buf[2];
    hdr->flags = buf[3];
    uint32_t net_len;
    memcpy(&net_len, buf + 4, sizeof(net_len));
    hdr->length = ntohl(net_len);
    if (hdr->length > NANO_MAX_PAYLOAD) return false;
    return true;
}
