#ifndef SHA256_H
#define SHA256_H

#include <stddef.h>
#include <stdint.h>

#define SHA256_DIGEST_LENGTH 32

typedef struct SHA256Context {
    uint32_t state[8];
    uint64_t bit_len;
    uint8_t data[64];
    size_t data_len;
} SHA256Context;

void sha256_init(SHA256Context *ctx);
void sha256_update(SHA256Context *ctx, const uint8_t *data, size_t len);
void sha256_final(SHA256Context *ctx, uint8_t hash[SHA256_DIGEST_LENGTH]);
void sha256_to_hex(const uint8_t hash[SHA256_DIGEST_LENGTH], char out[65]);

#endif
