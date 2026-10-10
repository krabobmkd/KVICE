/** \file   amiga_scalerow.h
 * \brief   AmigaOS 3.x port: scaled rows of the scaled drawing paths
 *
 * Shared by amiga_cgxscale.c (graphics card bitmaps) and
 * amiga_planarscale.c (chunky buffer for WriteChunkyPixels()).
 *
 * One row of destination pixels from one source row, through a color table,
 * with two bus savings:
 * - 8, 16, 24 bit pixels go out as aligned 32 bit words: 4, 2 pixels per
 *   word, or 4 pixels in 3 words for 24 bit (12 bytes). The unaligned pixels
 *   at the start and the end are written one by one.
 * - "lines": with a vertical zoom, up to 3 consecutive destination lines
 *   show the same source row. They are written together (nl = 1, 2 or 3,
 *   bpr bytes apart): the row is read and converted once. The writers are
 *   always inlined: called with a constant nl, each gives its own loop.
 *   nl > 1 needs bpr to be a multiple of 4 (aligned words on every line).
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

#ifndef VICE_AMIGA_SCALEROW_H
#define VICE_AMIGA_SCALEROW_H

#define SCALEROW_INLINE static inline __attribute__((always_inline))

/* 16.16 source pixels per destination pixel: destination pixel d (from the
 * destination area corner) shows source pixel (d * step) >> 16. The step
 * is rounded down, so this is not exactly d * src / dst: whoever computes
 * the destination of a source part must use SCALEROW_FIRST_DST(). */
#define SCALEROW_STEP(src, dst) (((ULONG)(src) << 16) / (ULONG)(dst))
/* the first destination pixel that shows source pixel \a s or one after
 * it: SCALEROW_FIRST_DST(s0) to SCALEROW_FIRST_DST(s1) excluded is exactly
 * what shows the source pixels s0 to s1 excluded */
#define SCALEROW_FIRST_DST(s, step) \
    ((int)((((ULONG)(s) << 16) + (step) - 1) / (step)))

/* big endian stores (the Amiga); a host test can replace them */
#ifndef SCALEROW_STORE32
#define SCALEROW_STORE32(p, v)  (*(ULONG *)(p) = (ULONG)(v))
#define SCALEROW_STORE16(p, v)  (*(UWORD *)(p) = (UWORD)(v))
#endif
#define SCALEROW_STORE8(p, v)   (*(UBYTE *)(p) = (UBYTE)(v))

/* one value to the nl lines, uses nl and bpr of the caller */
#define SCALEROW_PUT(size, p, v)                                    \
    do {                                                            \
        ULONG put_v_ = (ULONG)(v);                                  \
        SCALEROW_STORE##size((p), put_v_);                          \
        if (nl > 1) {                                               \
            SCALEROW_STORE##size((UBYTE *)(p) + bpr, put_v_);       \
        }                                                           \
        if (nl > 2) {                                               \
            SCALEROW_STORE##size((UBYTE *)(p) + 2 * bpr, put_v_);   \
        }                                                           \
    } while (0)

/* 1 byte per pixel: tab gives the pen */
SCALEROW_INLINE void scalerow8(UBYTE *d, const UBYTE *s, ULONG ax, ULONG step, int n,
                               const UBYTE *tab, int nl, ULONG bpr)
{
    while (n > 0 && ((ULONG)d & 3) != 0) {
        SCALEROW_PUT(8, d, tab[s[ax >> 16]]);
        d++;
        ax += step;
        n--;
    }
    while (n >= 4) {
        ULONG p;

        p = (ULONG)tab[s[ax >> 16]] << 24;
        ax += step;
        p |= (ULONG)tab[s[ax >> 16]] << 16;
        ax += step;
        p |= (ULONG)tab[s[ax >> 16]] << 8;
        ax += step;
        p |= (ULONG)tab[s[ax >> 16]];
        ax += step;
        SCALEROW_PUT(32, d, p);
        d += 4;
        n -= 4;
    }
    while (n-- > 0) {
        SCALEROW_PUT(8, d, tab[s[ax >> 16]]);
        d++;
        ax += step;
    }
}

/* 2 bytes per pixel */
SCALEROW_INLINE void scalerow16(UBYTE *d, const UBYTE *s, ULONG ax, ULONG step, int n,
                                const UWORD *tab, int nl, ULONG bpr)
{
    if (n > 0 && ((ULONG)d & 2) != 0) {
        SCALEROW_PUT(16, d, tab[s[ax >> 16]]);
        d += 2;
        ax += step;
        n--;
    }
    while (n >= 2) {
        ULONG p;

        /* big endian: the first pixel is the high word */
        p = (ULONG)tab[s[ax >> 16]] << 16;
        ax += step;
        p |= (ULONG)tab[s[ax >> 16]];
        ax += step;
        SCALEROW_PUT(32, d, p);
        d += 4;
        n -= 2;
    }
    if (n > 0) {
        SCALEROW_PUT(16, d, tab[s[ax >> 16]]);
    }
}

/* 3 bytes per pixel: tab gives the 3 bytes in memory order, 0x00aabbcc.
 * The address goes by 3: at most 3 pixels to a multiple of 4, then 4 pixels
 * are 3 aligned words. */
SCALEROW_INLINE void scalerow24(UBYTE *d, const UBYTE *s, ULONG ax, ULONG step, int n,
                                const ULONG *tab, int nl, ULONG bpr)
{
    while (n > 0 && ((ULONG)d & 3) != 0) {
        ULONG c = tab[s[ax >> 16]];

        SCALEROW_PUT(8, d, c >> 16);
        SCALEROW_PUT(8, d + 1, c >> 8);
        SCALEROW_PUT(8, d + 2, c);
        d += 3;
        ax += step;
        n--;
    }
    while (n >= 4) {
        ULONG a, b, c, e;

        a = tab[s[ax >> 16]];
        ax += step;
        b = tab[s[ax >> 16]];
        ax += step;
        c = tab[s[ax >> 16]];
        ax += step;
        e = tab[s[ax >> 16]];
        ax += step;
        /* a0 a1 a2 b0 | b1 b2 c0 c1 | c2 e0 e1 e2 */
        SCALEROW_PUT(32, d, (a << 8) | (b >> 16));
        SCALEROW_PUT(32, d + 4, (b << 16) | (c >> 8));
        SCALEROW_PUT(32, d + 8, (c << 24) | e);
        d += 12;
        n -= 4;
    }
    while (n-- > 0) {
        ULONG c = tab[s[ax >> 16]];

        SCALEROW_PUT(8, d, c >> 16);
        SCALEROW_PUT(8, d + 1, c >> 8);
        SCALEROW_PUT(8, d + 2, c);
        d += 3;
        ax += step;
    }
}

/* 4 bytes per pixel */
SCALEROW_INLINE void scalerow32(UBYTE *d, const UBYTE *s, ULONG ax, ULONG step, int n,
                                const ULONG *tab, int nl, ULONG bpr)
{
    while (n-- > 0) {
        SCALEROW_PUT(32, d, tab[s[ax >> 16]]);
        d += 4;
        ax += step;
    }
}

/* how many destination lines, from the one at ay (16.16 source), show the
 * same source row: 1 to 3, never more than left. multi: bpr allows it. */
SCALEROW_INLINE int scalerow_lines(ULONG ay, ULONG step_y, int left, int multi)
{
    ULONG sy = ay >> 16;

    if (!multi || left < 2 || ((ay + step_y) >> 16) != sy) {
        return 1;
    }
    if (left < 3 || ((ay + 2 * step_y) >> 16) != sy) {
        return 2;
    }
    return 3;
}

#endif
