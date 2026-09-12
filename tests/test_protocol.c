#include "../src/core/protocol.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

int main(void) {
    printf("Running Protocol Tests...\n");

    /* 1. Header encode/decode test */
    nano_header_t hdr;
    nano_header_init(&hdr, NANO_MSG_SCREEN_TILE, NANO_FLAG_COMPRESSED, 4096);

    assert(hdr.magic == NANO_MAGIC);
    assert(hdr.version == NANO_VERSION);
    assert(hdr.type == NANO_MSG_SCREEN_TILE);
    assert(hdr.flags == NANO_FLAG_COMPRESSED);
    assert(hdr.length == 4096);

    uint8_t buffer[NANO_HEADER_SIZE];
    bool enc_ok = nano_header_encode(&hdr, buffer, sizeof(buffer));
    assert(enc_ok);

    nano_header_t decoded;
    bool dec_ok = nano_header_decode(buffer, sizeof(buffer), &decoded);
    assert(dec_ok);
    assert(decoded.magic == NANO_MAGIC);
    assert(decoded.version == NANO_VERSION);
    assert(decoded.type == NANO_MSG_SCREEN_TILE);
    assert(decoded.flags == NANO_FLAG_COMPRESSED);
    assert(decoded.length == 4096);

    /* 2. Corrupt magic test */
    buffer[0] = 0x00;
    assert(!nano_header_decode(buffer, sizeof(buffer), &decoded));

    /* 3. Buffer too small test */
    assert(!nano_header_encode(&hdr, buffer, 4));
    assert(!nano_header_decode(buffer, 4, &decoded));

    printf("ALL PROTOCOL TESTS PASSED!\n");
    return 0;
}
