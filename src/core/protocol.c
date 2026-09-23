#include "protocol.h"
#include <string.h>

static inline void write_u32_be(uint8_t *buf, uint32_t val) {
    buf[0] = (uint8_t)((val >> 24) & 0xFF);
    buf[1] = (uint8_t)((val >> 16) & 0xFF);
    buf[2] = (uint8_t)((val >> 8) & 0xFF);
    buf[3] = (uint8_t)(val & 0xFF);
}

static inline uint32_t read_u32_be(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) |
           ((uint32_t)buf[3]);
}

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
    write_u32_be(buf + 4, hdr->length);
    return true;
}

bool nano_header_decode(const uint8_t *buf, size_t buf_len, nano_header_t *hdr) {
    if (!buf || !hdr || buf_len < NANO_HEADER_SIZE) return false;
    if (buf[0] != NANO_MAGIC || buf[1] != NANO_VERSION) return false;
    hdr->magic = buf[0];
    hdr->version = buf[1];
    hdr->type = buf[2];
    hdr->flags = buf[3];
    hdr->length = read_u32_be(buf + 4);
    if (hdr->length > NANO_MAX_PAYLOAD) return false;
    return true;
}
