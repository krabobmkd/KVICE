/** \file   risha256.c
 * \brief   KVICE Resource Installer: SHA-256 (FIPS 180-4), plain C
 *
 * Own small implementation, so the files can be checked without AmiSSL.
 * 32 bit words in unsigned long (at least 32 bits, masked).
 */

#include <string.h>

#include "risha256.h"

#define ROR(x, n) ((((x) >> (n)) | ((x) << (32 - (n)))) & 0xffffffffUL)

static const unsigned long k[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL, 0x59f111f1UL,
    0x923f82a4UL, 0xab1c5ed5UL, 0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
    0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL,
    0x0fc19dc6UL, 0x240ca1ccUL, 0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL,
    0x06ca6351UL, 0x14292967UL, 0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
    0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL, 0xa2bfe8a1UL, 0xa81a664bUL,
    0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL,
    0x5b9cca4fUL, 0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

static void transform(ri_sha256_t *c, const unsigned char *p)
{
    unsigned long w[64];
    unsigned long a, b, cc, d, e, f, g, h, t1, t2;
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = ((unsigned long)p[i * 4] << 24) | ((unsigned long)p[i * 4 + 1] << 16)
               | ((unsigned long)p[i * 4 + 2] << 8) | (unsigned long)p[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        unsigned long s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        unsigned long s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);

        w[i] = (w[i - 16] + s0 + w[i - 7] + s1) & 0xffffffffUL;
    }
    a = c->state[0];
    b = c->state[1];
    cc = c->state[2];
    d = c->state[3];
    e = c->state[4];
    f = c->state[5];
    g = c->state[6];
    h = c->state[7];
    for (i = 0; i < 64; i++) {
        t1 = (h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i])
             & 0xffffffffUL;
        t2 = ((ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & cc) ^ (b & cc)))
             & 0xffffffffUL;
        h = g;
        g = f;
        f = e;
        e = (d + t1) & 0xffffffffUL;
        d = cc;
        cc = b;
        b = a;
        a = (t1 + t2) & 0xffffffffUL;
    }
    c->state[0] = (c->state[0] + a) & 0xffffffffUL;
    c->state[1] = (c->state[1] + b) & 0xffffffffUL;
    c->state[2] = (c->state[2] + cc) & 0xffffffffUL;
    c->state[3] = (c->state[3] + d) & 0xffffffffUL;
    c->state[4] = (c->state[4] + e) & 0xffffffffUL;
    c->state[5] = (c->state[5] + f) & 0xffffffffUL;
    c->state[6] = (c->state[6] + g) & 0xffffffffUL;
    c->state[7] = (c->state[7] + h) & 0xffffffffUL;
}

void ri_sha256_init(ri_sha256_t *c)
{
    c->state[0] = 0x6a09e667UL;
    c->state[1] = 0xbb67ae85UL;
    c->state[2] = 0x3c6ef372UL;
    c->state[3] = 0xa54ff53aUL;
    c->state[4] = 0x510e527fUL;
    c->state[5] = 0x9b05688cUL;
    c->state[6] = 0x1f83d9abUL;
    c->state[7] = 0x5be0cd19UL;
    c->count_lo = 0;
    c->count_hi = 0;
    c->used = 0;
}

void ri_sha256_update(ri_sha256_t *c, const void *data, size_t len)
{
    const unsigned char *p = (const unsigned char *)data;

    while (len > 0) {
        size_t n = 64 - c->used;

        if (n > len) {
            n = len;
        }
        memcpy(c->block + c->used, p, n);
        c->used += (unsigned int)n;
        p += n;
        len -= n;
        c->count_lo = (c->count_lo + n) & 0xffffffffUL;
        if (c->count_lo < n) {
            c->count_hi++;
        }
        if (c->used == 64) {
            transform(c, c->block);
            c->used = 0;
        }
    }
}

void ri_sha256_final(ri_sha256_t *c, unsigned char digest[32])
{
    unsigned long bits_hi = ((c->count_hi << 3) | (c->count_lo >> 29)) & 0xffffffffUL;
    unsigned long bits_lo = (c->count_lo << 3) & 0xffffffffUL;
    int i;

    c->block[c->used++] = 0x80;
    if (c->used > 56) {
        memset(c->block + c->used, 0, 64 - c->used);
        transform(c, c->block);
        c->used = 0;
    }
    memset(c->block + c->used, 0, 56 - c->used);
    for (i = 0; i < 4; i++) {
        c->block[56 + i] = (unsigned char)(bits_hi >> (24 - i * 8));
        c->block[60 + i] = (unsigned char)(bits_lo >> (24 - i * 8));
    }
    transform(c, c->block);
    for (i = 0; i < 32; i++) {
        digest[i] = (unsigned char)(c->state[i / 4] >> (24 - (i % 4) * 8));
    }
}

void ri_sha256(const void *data, size_t len, unsigned char digest[32])
{
    ri_sha256_t c;

    ri_sha256_init(&c);
    ri_sha256_update(&c, data, len);
    ri_sha256_final(&c, digest);
}
