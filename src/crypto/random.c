#include "random.h"
#include <stdlib.h>
#include <stdio.h>

void nano_random_bytes(uint8_t *buf, size_t len) {
    if (!buf || len == 0) return;
    arc4random_buf(buf, len);
}

void nano_random_pin(char pin_out[7]) {
    if (!pin_out) return;
    /* Generate 6 random decimal digits (000000 - 999999) */
    uint32_t val = arc4random_uniform(1000000);
    snprintf(pin_out, 7, "%06u", val);
}
