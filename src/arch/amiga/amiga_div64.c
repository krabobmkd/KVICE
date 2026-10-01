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
#include <string.h>

#if defined(__mc68020__) || defined(__mc68030__) || defined(__mc68040__)
#define HAVE_DIVU64_32 1
#endif

uint64_t __udivdi3(uint64_t n, uint64_t d);
uint64_t __umoddi3(uint64_t n, uint64_t d);
int64_t __divdi3(int64_t n, int64_t d);
int64_t __moddi3(int64_t n, int64_t d);

#ifdef VICE_AMIGA_DIV64_STATS
/* Measurement build only: how many 64 bit divisions, and who calls them.
 * Callers are kept by return address in a small hash table, printed as
 * offsets from __udivdi3 (one code hunk: "nm -n x64" maps them back, see
 * amiga/tools/div64_sites.sh). */
#include <stdio.h>
#include <stdlib.h>

#define DIV64_SITES 128

typedef struct div64_site_s {
    uint32_t addr;
    uint32_t count;
} div64_site_t;

static div64_site_t div64_sites[DIV64_SITES];
static uint32_t div64_calls = 0;
static uint32_t div64_calls_hi = 0;     /* dividend >= 2^32 */
static uint32_t div64_slow = 0;         /* shift and subtract loop */
static uint32_t div64_other = 0;        /* table full */

static void div64_count(void *ret, uint64_t n)
{
    uint32_t addr = (uint32_t)ret;
    uint32_t i = (addr >> 1) & (DIV64_SITES - 1);
    int probe;

    div64_calls++;
    if ((n >> 32) != 0) {
        div64_calls_hi++;
    }
    for (probe = 0; probe < DIV64_SITES; probe++) {
        div64_site_t *site = &div64_sites[(i + probe) & (DIV64_SITES - 1)];

        if (site->addr == addr) {
            site->count++;
            return;
        }
        if (site->addr == 0) {
            site->addr = addr;
            site->count = 1;
            return;
        }
    }
    div64_other++;
}

#define DIV64_COUNT(n) div64_count(__builtin_return_address(0), (n))
#define DIV64_COUNT_SLOW() div64_slow++

static int div64_site_cmp(const void *a, const void *b)
{
    const div64_site_t *sa = a;
    const div64_site_t *sb = b;

    return (sb->count > sa->count) - (sb->count < sa->count);
}

/** \brief  Print the counters (per second) and the top callers, then reset
 *
 * \param[in]   elapsed_us  time since the previous report
 */
void amiga_div64_report(uint32_t elapsed_us)
{
    static div64_site_t sorted[DIV64_SITES];
    uint32_t secs_x10 = elapsed_us / 100000;
    int i;

    if (secs_x10 == 0) {
        secs_x10 = 1;
    }
    memcpy(sorted, div64_sites, sizeof sorted);
    qsort(sorted, DIV64_SITES, sizeof sorted[0], div64_site_cmp);
    printf("div64: %lu calls/s (%lu/s dividend >= 2^32, %lu/s slow loop, %lu/s untracked)\n",
           (unsigned long)(div64_calls * 10 / secs_x10),
           (unsigned long)(div64_calls_hi * 10 / secs_x10),
           (unsigned long)(div64_slow * 10 / secs_x10),
           (unsigned long)(div64_other * 10 / secs_x10));
    for (i = 0; i < 12 && sorted[i].count != 0; i++) {
        printf("div64:   %8lu/s from __udivdi3%+ld\n",
               (unsigned long)(sorted[i].count * 10 / secs_x10),
               (long)(sorted[i].addr - (uint32_t)__udivdi3));
    }
    fflush(stdout);
    memset(div64_sites, 0, sizeof div64_sites);
    div64_calls = div64_calls_hi = div64_slow = div64_other = 0;
}
#else
#define DIV64_COUNT(n)
#define DIV64_COUNT_SLOW()
#endif

/* any 64 bit divisor: shift and subtract */
static uint64_t __attribute__((noinline)) udivmod_slow(uint64_t n, uint64_t d, uint64_t *rem)
{
    uint64_t q = 0;
    int shift = 0;

    DIV64_COUNT_SLOW();
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

    DIV64_COUNT(n);
    return udivmod(n, d, &r);
}

uint64_t __umoddi3(uint64_t n, uint64_t d)
{
    uint64_t r;

    DIV64_COUNT(n);
    udivmod(n, d, &r);
    return r;
}

/* signed: C rounds toward zero, the remainder has the sign of n */
int64_t __divdi3(int64_t n, int64_t d)
{
    uint64_t un = (n < 0) ? -(uint64_t)n : (uint64_t)n;
    uint64_t ud = (d < 0) ? -(uint64_t)d : (uint64_t)d;
    uint64_t r;
    uint64_t q;

    DIV64_COUNT(un);
    q = udivmod(un, ud, &r);

    return ((n < 0) != (d < 0)) ? -(int64_t)q : (int64_t)q;
}

int64_t __moddi3(int64_t n, int64_t d)
{
    uint64_t un = (n < 0) ? -(uint64_t)n : (uint64_t)n;
    uint64_t ud = (d < 0) ? -(uint64_t)d : (uint64_t)d;
    uint64_t r;

    DIV64_COUNT(un);
    udivmod(un, ud, &r);
    return (n < 0) ? -(int64_t)r : (int64_t)r;
}
