#ifndef NANO_RANDOM_H
#define NANO_RANDOM_H

#include <stdint.h>
#include <stddef.h>

void nano_random_bytes(uint8_t *buf, size_t len);
void nano_random_pin(char pin_out[7]);

#endif /* NANO_RANDOM_H */
