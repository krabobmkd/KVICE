/** \file   risha256.h
 * \brief   KVICE Resource Installer: SHA-256 (FIPS 180-4), plain C
 */

#ifndef RISHA256_H
#define RISHA256_H

#include <stddef.h>

typedef struct ri_sha256_s {
    unsigned long state[8];
    unsigned long count_lo, count_hi;   /* bytes hashed */
    unsigned char block[64];
    unsigned int used;                  /* bytes in block */
} ri_sha256_t;

void ri_sha256_init(ri_sha256_t *c);
void ri_sha256_update(ri_sha256_t *c, const void *data, size_t len);
void ri_sha256_final(ri_sha256_t *c, unsigned char digest[32]);

/* the SHA-256 of a whole buffer */
void ri_sha256(const void *data, size_t len, unsigned char digest[32]);

#endif
