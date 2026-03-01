#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t state[4];
    uint64_t count;
    uint8_t buffer[64];
} mbedtls_md5_context;
void mbedtls_md5_init(mbedtls_md5_context *ctx);
void mbedtls_md5_free(mbedtls_md5_context *ctx);
int mbedtls_md5_starts(mbedtls_md5_context *ctx);
int mbedtls_md5_update(mbedtls_md5_context *ctx, const unsigned char *input, size_t ilen);
int mbedtls_md5_finish(mbedtls_md5_context *ctx, unsigned char output[16]);
#define mbedtls_md5_starts_ret mbedtls_md5_starts
#define mbedtls_md5_update_ret mbedtls_md5_update
#define mbedtls_md5_finish_ret mbedtls_md5_finish
#ifdef __cplusplus
}
#endif
