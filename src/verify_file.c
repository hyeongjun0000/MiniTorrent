#include "sha256.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 8192

static int sha256_file(const char *path, char hex[65])
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        perror("[ERROR] fopen failed");
        return -1;
    }

    SHA256Context ctx;
    uint8_t hash[SHA256_DIGEST_LENGTH];
    uint8_t buffer[BUFFER_SIZE];
    size_t n;

    sha256_init(&ctx);
    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        sha256_update(&ctx, buffer, n);
    }

    if (ferror(fp)) {
        perror("[ERROR] fread failed");
        fclose(fp);
        return -1;
    }

    fclose(fp);
    sha256_final(&ctx, hash);
    sha256_to_hex(hash, hex);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <original_file> <rebuilt_file>\n", argv[0]);
        return 1;
    }

    char original_hash[65];
    char rebuilt_hash[65];

    if (sha256_file(argv[1], original_hash) < 0 ||
        sha256_file(argv[2], rebuilt_hash) < 0) {
        return 1;
    }

    printf("Original SHA256: %s\n", original_hash);
    printf("Rebuilt  SHA256: %s\n", rebuilt_hash);

    if (strcmp(original_hash, rebuilt_hash) == 0) {
        printf("SHA256 MATCH\n");
        return 0;
    }

    printf("SHA256 MISMATCH\n");
    return 1;
}
