#ifndef NANO_AUTH_H
#define NANO_AUTH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define NANO_SHA256_DIGEST_SIZE 32

/* Self-contained SHA-256 context */
typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t  buffer[64];
} nano_sha256_ctx_t;

void nano_sha256_init(nano_sha256_ctx_t *ctx);
void nano_sha256_update(nano_sha256_ctx_t *ctx, const uint8_t *data, size_t len);
void nano_sha256_final(nano_sha256_ctx_t *ctx, uint8_t digest[NANO_SHA256_DIGEST_SIZE]);
void nano_sha256(const uint8_t *data, size_t len, uint8_t digest[NANO_SHA256_DIGEST_SIZE]);

/* HMAC-SHA256 */
void nano_hmac_sha256(const uint8_t *key, size_t key_len,
                      const uint8_t *data, size_t data_len,
                      uint8_t out[NANO_SHA256_DIGEST_SIZE]);

/* Challenge-response pairing helpers */
void nano_auth_compute_response(const char *pin,
                                const uint8_t nonce[32],
                                uint8_t out_hash[32]);

bool nano_auth_verify_response(const char *pin,
                               const uint8_t nonce[32],
                               const uint8_t client_hash[32]);

#endif /* NANO_AUTH_H */
