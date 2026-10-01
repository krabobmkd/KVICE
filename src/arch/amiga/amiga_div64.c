/** \file   amiga_div64.c
 * \brief   AmigaOS 3.x port: fast 64 bit integer division for 68020+
 *
 * VICE clocks (CLOCK) are 64 bit: every "clk % cycles_per_line" of the
 * VIC-II, VIA, drive rotation... is a libgcc __umoddi3() / __udivdi3() call.
 * The linked libgcc is the 68000 multilib: generic C code that itself calls
 * the software __udivsi3() and __mulsi3(), very slow, and called many times
 * per raster line.
 *
 * These replace the libgcc ones (object files win over archive members at
 * link time). The divisors used by the emulation fit in 32 bits, so:
 * - 68020/030/040: one 32/32 divul.l and one 64/32 divu.l, both in hardware;
 * - 68060 (64/32 divu.l is emulated by 68060.library): 16 bit pieces with
 *   32/32 divul.l only, when the divisor fits in 16 bits.
 * Other cases (rare, not emulation paths): shift and subtract.
 */

/*
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include <stdint.h>

#if defined(__mc68020__) || defined(__mc68030__) || defined(__mc68040__)
#define HAVE_DIVU64_32 1
#endif

uint64_t __udivdi3(uint64_t n, uint64_t d);
uint64_t __umoddi3(uint64_t n, uint64_t d);
int64_t __divdi3(int64_t n, int64_t d);
int64_t __moddi3(int64_t n, int64_t d);

/* any 64 bit divisor: shift and subtract */
static uint64_t __attribute__((noinline)) udivmod_slow(uint64_t n, uint64_t d, uint64_t *rem)
{
    uint64_t q = 0;
    int shift = 0;

    if (d == 0) {
        /* like the 68k: no sensible result, do not loop forever */
        *rem = n;
        return ~(uint64_t)0;
    }
    if (d > n) {
        *rem = n;
        return 0;
    }
    while (!(d & ((uint64_t)1 << 63)) && (d << 1) <= n) {
        d <<= 1;
        shift++;
    }
    for (; shift >= 0; shift--) {
        q <<= 1;
        if (n >= d) {
            n -= d;
            q |= 1;
        }
        d >>= 1;
    }
    *rem = n;
    return q;
}

static inline __attribute__((always_inline)) uint64_t udivmod(uint64_t n, uint64_t d, uint64_t *rem)
{
    uint32_t hi = (uint32_t)(n >> 32);
    uint32_t lo = (uint32_t)n;
    uint32_t d32, qhi, qlo, r;

    if ((d >> 32) != 0) {
        return udivmod_slow(n, d, rem);
    }
    d32 = (uint32_t)d;
    if (d32 == 0) {
        return udivmod_slow(n, d, rem);
    }
    if (hi == 0) {
        /* both 32 bit: a single divul.l */
        *rem = lo % d32;
        return lo / d32;
    }
    /* high half first: one divul.l gives both */
    qhi = hi / d32;
    r = hi % d32;
#ifdef HAVE_DIVU64_32
    /* r:lo / d32, r < d32 so the quotient fits in 32 bits */
    __asm__("divu.l %2,%1:%0" : "+d"(lo), "+d"(r) : "dmi"(d32) : "cc");
    qlo = lo;
#else
    if (d32 < 0x10000) {
        /* r < d32 < 2^16: each step is a 32/32 division */
        uint32_t x = (r << 16) | (lo >> 16);
        uint32_t q1 = x / d32;

        r = x % d32;
        x = (r << 16) | (lo & 0xffff);
        qlo = (q1 << 16) | (x / d32);
        r = x % d32;
    } else {
        uint64_t r64;
        uint64_t q = udivmod_slow(((uint64_t)r << 32) | lo, d, &r64);

        qlo = (uint32_t)q;
        r = (uint32_t)r64;
    }
#endif
    *rem = r;
    return ((uint64_t)qhi << 32) | qlo;
}

uint64_t __udivdi3(uint64_t n, uint64_t d)
{
    uint64_t r;

    return udivmod(n, d, &r);
}

uint64_t __umoddi3(uint64_t n, uint64_t d)
{
    uint64_t r;

    udivmod(n, d, &r);
    return r;
}

/* signed: C rounds toward zero, the remainder has the sign of n */
int64_t __divdi3(int64_t n, int64_t d)
{
    uint64_t un = (n < 0) ? -(uint64_t)n : (uint64_t)n;
    uint64_t ud = (d < 0) ? -(uint64_t)d : (uint64_t)d;
    uint64_t r;
    uint64_t q = udivmod(un, ud, &r);

    return ((n < 0) != (d < 0)) ? -(int64_t)q : (int64_t)q;
}

int64_t __moddi3(int64_t n, int64_t d)
{
    uint64_t un = (n < 0) ? -(uint64_t)n : (uint64_t)n;
    uint64_t ud = (d < 0) ? -(uint64_t)d : (uint64_t)d;
    uint64_t r;

    udivmod(un, ud, &r);
    return (n < 0) ? -(int64_t)r : (int64_t)r;
}
