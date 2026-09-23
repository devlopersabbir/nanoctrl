#include "random.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
/* arc4random_buf is built-in */
#else
#include <fcntl.h>
#include <unistd.h>
#endif

void nano_random_bytes(uint8_t *buf, size_t len) {
    if (!buf || len == 0) return;
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    arc4random_buf(buf, len);
#elif defined(_WIN32)
    HCRYPTPROV hProv;
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hProv, (DWORD)len, buf);
        CryptReleaseContext(hProv, 0);
    }
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t r = read(fd, buf, len);
        close(fd);
        if (r == (ssize_t)len) return;
    }
    for (size_t i = 0; i < len; ++i) {
        buf[i] = (uint8_t)(rand() & 0xFF);
    }
#endif
}

void nano_random_pin(char pin_out[7]) {
    if (!pin_out) return;
    uint32_t val = 0;
    nano_random_bytes((uint8_t *)&val, sizeof(val));
    val = val % 1000000;
    snprintf(pin_out, 7, "%06u", val);
}
