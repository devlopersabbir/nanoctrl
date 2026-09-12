#include "auth.h"
#include <string.h>

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391050b4, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void sha256_transform(nano_sha256_ctx_t *ctx, const uint8_t data[64]) {
    uint32_t a, b, c, d, e, f, g, h, t1, t2, m[64];
    size_t i, j;

    for (i = 0, j = 0; i < 16; ++i, j += 4) {
        m[i] = ((uint32_t)data[j] << 24) |
               ((uint32_t)data[j + 1] << 16) |
               ((uint32_t)data[j + 2] << 8) |
               ((uint32_t)data[j + 3]);
    }
    for (; i < 64; ++i) {
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e, f, g) + K[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void nano_sha256_init(nano_sha256_ctx_t *ctx) {
    if (!ctx) return;
    ctx->count = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

void nano_sha256_update(nano_sha256_ctx_t *ctx, const uint8_t *data, size_t len) {
    if (!ctx || !data || len == 0) return;
    size_t i;
    for (i = 0; i < len; ++i) {
        ctx->buffer[ctx->count % 64] = data[i];
        ctx->count++;
        if ((ctx->count % 64) == 0) {
            sha256_transform(ctx, ctx->buffer);
        }
    }
}

void nano_sha256_final(nano_sha256_ctx_t *ctx, uint8_t digest[NANO_SHA256_DIGEST_SIZE]) {
    if (!ctx || !digest) return;
    uint64_t bitlen = ctx->count * 8;
    uint8_t pad = 0x80;
    size_t rem = ctx->count % 64;

    ctx->buffer[rem++] = pad;
    if (rem > 56) {
        memset(ctx->buffer + rem, 0, 64 - rem);
        sha256_transform(ctx, ctx->buffer);
        memset(ctx->buffer, 0, 56);
    } else {
        memset(ctx->buffer + rem, 0, 56 - rem);
    }

    for (int i = 7; i >= 0; --i) {
        ctx->buffer[56 + (7 - i)] = (uint8_t)(bitlen >> (i * 8));
    }
    sha256_transform(ctx, ctx->buffer);

    for (int i = 0; i < 8; ++i) {
        digest[i * 4 + 0] = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

void nano_sha256(const uint8_t *data, size_t len, uint8_t digest[NANO_SHA256_DIGEST_SIZE]) {
    nano_sha256_ctx_t ctx;
    nano_sha256_init(&ctx);
    nano_sha256_update(&ctx, data, len);
    nano_sha256_final(&ctx, digest);
}

void nano_hmac_sha256(const uint8_t *key, size_t key_len,
                      const uint8_t *data, size_t data_len,
                      uint8_t out[NANO_SHA256_DIGEST_SIZE]) {
    uint8_t k[64];
    memset(k, 0, sizeof(k));
    if (key_len > 64) {
        nano_sha256(key, key_len, k);
    } else {
        memcpy(k, key, key_len);
    }

    uint8_t k_ipad[64];
    uint8_t k_opad[64];
    for (int i = 0; i < 64; ++i) {
        k_ipad[i] = k[i] ^ 0x36;
        k_opad[i] = k[i] ^ 0x5c;
    }

    /* Inner hash */
    nano_sha256_ctx_t ctx;
    nano_sha256_init(&ctx);
    nano_sha256_update(&ctx, k_ipad, 64);
    nano_sha256_update(&ctx, data, data_len);
    uint8_t inner_hash[NANO_SHA256_DIGEST_SIZE];
    nano_sha256_final(&ctx, inner_hash);

    /* Outer hash */
    nano_sha256_init(&ctx);
    nano_sha256_update(&ctx, k_opad, 64);
    nano_sha256_update(&ctx, inner_hash, NANO_SHA256_DIGEST_SIZE);
    nano_sha256_final(&ctx, out);
}

void nano_auth_compute_response(const char *pin,
                                const uint8_t nonce[32],
                                uint8_t out_hash[32]) {
    size_t pin_len = pin ? strlen(pin) : 0;
    nano_hmac_sha256((const uint8_t *)pin, pin_len, nonce, 32, out_hash);
}

bool nano_auth_verify_response(const char *pin,
                               const uint8_t nonce[32],
                               const uint8_t client_hash[32]) {
    uint8_t expected[32];
    nano_auth_compute_response(pin, nonce, expected);

    /* Constant-time comparison */
    uint8_t diff = 0;
    for (int i = 0; i < 32; ++i) {
        diff |= (expected[i] ^ client_hash[i]);
    }
    return (diff == 0);
}
