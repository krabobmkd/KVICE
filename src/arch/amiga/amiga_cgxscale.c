/** \file   amiga_cgxscale.c
 * \brief   AmigaOS 3.x port: scaled drawing into cybergraphics bitmaps
 *
 * See amiga_cgxscale.h. The pixel format only changes the color table:
 * the rows are per pixel size (1, 2, 3, 4 bytes), see amiga_scalerow.h.
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

#include "vice.h"

#include <string.h>

#include <exec/types.h>
#include <utility/hooks.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/cybergraphics.h>

#include "amiga_cgxscale.h"
#include "amiga_scalerow.h"

/* opened by video.c, may be NULL */
extern struct Library *CyberGfxBase;

/* ------------------------------------------------------------------------- */
/* destination pixel format and color tables */

static ULONG dst_pixfmt = ~0UL;
static int dst_bpp = 0;                 /* bytes per pixel, 0: unsupported */
static struct ColorMap *dst_cm = NULL;  /* CLUT screens */

static ULONG src_rgb[256];              /* source palette, 0x00RRGGBB */
static int src_count = 0;

static UBYTE clut8[256];                /* 1 byte per pixel: pens */
static UWORD clut16[256];               /* 2 bytes per pixel */
static ULONG clut24[256];               /* 3 bytes per pixel, memory order 0x00aabbcc */
static ULONG clut32[256];               /* 4 bytes per pixel */

/* "Simple" scanlines: darker lines between the source rows, as on a CRT
 * monitor. Only for 15/16, 24 and 32 bit, and a zoom of 1.5 or more in
 * height, else the classic drawing. Nothing is computed per pixel:
 * - SHADE_LEVELS copies of the color table, darker and darker (level 0 is
 *   the normal one), built once per palette or pixel format,
 * - the level of each destination line, built once per source and
 *   destination height (settings, window size).
 * The rows are then drawn as usual, with the table of their level. */
#define SHADE_LEVELS 8
/* intensity of the darkest level, /256 */
#define SHADE_DARKEST 96
/* the dark part at the bottom of each source row, 16.16: 3/8 of a row */
#define SHADE_GAP 0x6000UL
/* longest destination area, taller ones get the classic drawing */
#define SHADE_MAX_LINES 4096

static int scanlines_mode = CGXSCALE_SCANLINES_NONE;
static int shade_valid = 0;             /* shade tables of this palette */
static UWORD shade16[SHADE_LEVELS][256];
static ULONG shade32[SHADE_LEVELS][256]; /* 24 bit (memory order) and 32 bit */

static UBYTE line_level[SHADE_MAX_LINES];
static ULONG line_level_step = 0;       /* step_y and height they are for */
static int line_level_h = 0;

#ifdef CGXSCALE_SCANLINES_BILINEAR_ARCHIVE
/* "Bilinear" scanlines, archived: not compiled (CGXSCALE_SCANLINES_BILINEAR_ARCHIVE
 * is never defined). Tested 2026-10-26: no real gain over "Simple" for its
 * CPU cost, so the menu only has Scanlines on / off.
 * Each destination line mixes the 2 source rows
 * around it, each weighted by a CRT beam profile of its distance: bright on
 * the rows, smooth darker lines between them at any zoom. Same conditions
 * as "Simple". The weights are the same along a line and the source is
 * palettized, so they are in the color tables: BLEND_LEVELS copies of the
 * table, color * level / BLEND_LEVEL_MAX, and a pixel is 2 lookups and an
 * add. The weights of a line never sum over BLEND_LEVEL_MAX: each field of
 * the sum stays in range, no carry between them.
 * The 2 bytes formats are in the tables in their big endian order, the PC
 * ones swapped at the store. */
#define BLEND_LEVEL_MAX 32
#define BLEND_LEVELS (BLEND_LEVEL_MAX + 1)

/* the beam: weight /256 of a row at distance d / 32 rows from its center,
 * exp(-d^2 / 0.2076): 154/256 between 2 rows */
static const UBYTE beam_weight[BLEND_LEVEL_MAX + 1] = {
    255, 255, 251, 245, 237, 228, 216, 203, 189, 175, 160, 145, 130, 116, 102, 89,
    77, 66, 56, 47, 39, 32, 26, 21, 17, 14, 11, 8, 6, 5, 4, 3, 0
};

static int blend_valid = 0;             /* blend tables of this palette */
static ULONG blend_tab[BLEND_LEVELS][256];
static int blend_swap = 0;              /* PC 15/16 bit: bytes swapped at the store */
static ULONG blend_alpha = 0;           /* 32 bit: the alpha byte, out of the sums */

/* per destination line: the first source row, the levels of it and of the
 * next one */
static UWORD line_row[SHADE_MAX_LINES];
static UBYTE line_la[SHADE_MAX_LINES];
static UBYTE line_lb[SHADE_MAX_LINES];
static ULONG line_blend_step = 0;       /* step_y and heights they are for */
static int line_blend_h = 0;
static int line_blend_src_h = 0;
#endif /* CGXSCALE_SCANLINES_BILINEAR_ARCHIVE */

static UWORD swap16(UWORD v)
{
    return (UWORD)((v << 8) | (v >> 8));
}

/* one color in the destination pixel format (not CLUT) */
static ULONG pack_color(ULONG r, ULONG g, ULONG b)
{
    UWORD rgb15 = (UWORD)(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
    UWORD bgr15 = (UWORD)(((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3));
    UWORD rgb16 = (UWORD)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    UWORD bgr16 = (UWORD)(((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3));

    switch (dst_pixfmt) {
        case PIXFMT_RGB15:   return rgb15;
        case PIXFMT_BGR15:   return bgr15;
        case PIXFMT_RGB15PC: return swap16(rgb15);
        case PIXFMT_BGR15PC: return swap16(bgr15);
        case PIXFMT_RGB16:   return rgb16;
        case PIXFMT_BGR16:   return bgr16;
        case PIXFMT_RGB16PC: return swap16(rgb16);
        case PIXFMT_BGR16PC: return swap16(bgr16);
        case PIXFMT_RGB24:   return (r << 16) | (g << 8) | b;
        case PIXFMT_BGR24:   return (b << 16) | (g << 8) | r;
        case PIXFMT_ARGB32:  return 0xff000000UL | (r << 16) | (g << 8) | b;
        case PIXFMT_BGRA32:  return (b << 24) | (g << 16) | (r << 8) | 0xffUL;
        case PIXFMT_RGBA32:  return (r << 24) | (g << 16) | (b << 8) | 0xffUL;
        default:             return 0;
    }
}

static void build_tables(void)
{
    int i;

    for (i = 0; i < src_count; i++) {
        ULONG r = (src_rgb[i] >> 16) & 0xff;
        ULONG g = (src_rgb[i] >> 8) & 0xff;
        ULONG b = src_rgb[i] & 0xff;

        switch (dst_bpp) {
            case 1:
                clut8[i] = (dst_cm != NULL)
                           ? (UBYTE)FindColor(dst_cm, r * 0x01010101UL, g * 0x01010101UL,
                                              b * 0x01010101UL, -1)
                           : (UBYTE)i;
                break;
            case 2: clut16[i] = (UWORD)pack_color(r, g, b); break;
            case 3: clut24[i] = pack_color(r, g, b); break;
            case 4: clut32[i] = pack_color(r, g, b); break;
            default:
                break;
        }
    }
    shade_valid = 0;
#ifdef CGXSCALE_SCANLINES_BILINEAR_ARCHIVE
    blend_valid = 0;
#endif /* CGXSCALE_SCANLINES_BILINEAR_ARCHIVE */
}

/* the shade tables, from the source palette: 16 x 16 bit products only
 * (mulu.w) */
static void build_shade_tables(void)
{
    int lv, i;

    for (lv = 0; lv < SHADE_LEVELS; lv++) {
        UWORD k = (UWORD)(256 - (lv * (256 - SHADE_DARKEST)) / (SHADE_LEVELS - 1));

        for (i = 0; i < src_count; i++) {
            UWORD r = (UWORD)((src_rgb[i] >> 16) & 0xff);
            UWORD g = (UWORD)((src_rgb[i] >> 8) & 0xff);
            UWORD b = (UWORD)(src_rgb[i] & 0xff);
            ULONG c = pack_color((ULONG)(UWORD)(r * k) >> 8,
                                 (ULONG)(UWORD)(g * k) >> 8,
                                 (ULONG)(UWORD)(b * k) >> 8);

            if (dst_bpp == 2) {
                shade16[lv][i] = (UWORD)c;
            } else {
                shade32[lv][i] = c;
            }
        }
    }
    shade_valid = 1;
}

#ifdef CGXSCALE_SCANLINES_BILINEAR_ARCHIVE
/* the blend tables: 16 x 16 bit products only (mulu.w) */
static void build_blend_tables(void)
{
    int lv, i;

    blend_swap = dst_pixfmt == PIXFMT_RGB15PC || dst_pixfmt == PIXFMT_BGR15PC
                 || dst_pixfmt == PIXFMT_RGB16PC || dst_pixfmt == PIXFMT_BGR16PC;
    blend_alpha = dst_bpp == 4 ? pack_color(0, 0, 0) : 0;
    for (lv = 0; lv < BLEND_LEVELS; lv++) {
        UWORD k = (UWORD)(lv * (256 / BLEND_LEVEL_MAX));

        for (i = 0; i < src_count; i++) {
            UWORD r = (UWORD)((src_rgb[i] >> 16) & 0xff);
            UWORD g = (UWORD)((src_rgb[i] >> 8) & 0xff);
            UWORD b = (UWORD)(src_rgb[i] & 0xff);
            ULONG c = pack_color((ULONG)(UWORD)(r * k) >> 8,
                                 (ULONG)(UWORD)(g * k) >> 8,
                                 (ULONG)(UWORD)(b * k) >> 8);

            if (blend_swap) {
                c = swap16((UWORD)c);
            }
            blend_tab[lv][i] = c & ~blend_alpha;
        }
    }
    blend_valid = 1;
}

/* the rows and levels of each destination line. The line at ay (16.16
 * source) is between row a = ay >> 16 and a + 1, at distance t and 1 - t
 * from them. The next row is the last one at the bottom. */
static void build_line_blend(ULONG step_y, int dst_h, int src_h)
{
    ULONG ay = 0;
    int y;

    for (y = 0; y < dst_h; y++) {
        int a = (int)(ay >> 16);
        int t = (int)(((ay & 0xffffUL) + 0x400UL) >> 11);  /* 0..32 */
        int wa = beam_weight[t];
        int wb = beam_weight[BLEND_LEVEL_MAX - t];
        int la, lb;

        if (a >= src_h) {
            a = src_h - 1;
        }
        la = (wa + 4) >> 3;
        lb = (wb + 4) >> 3;
        if (la + lb > BLEND_LEVEL_MAX) {
            lb = BLEND_LEVEL_MAX - la;
        }
        if (a + 1 >= src_h) {
            /* no next row: its part is lost, as the beam below the screen */
            lb = 0;
        }
        line_row[y] = (UWORD)a;
        line_la[y] = (UBYTE)la;
        line_lb[y] = (UBYTE)lb;
        ay += step_y;
    }
    line_blend_step = step_y;
    line_blend_h = dst_h;
    line_blend_src_h = src_h;
}
#endif /* CGXSCALE_SCANLINES_BILINEAR_ARCHIVE */

/* the level of each destination line: how much of it covers the dark part
 * of its source row (SHADE_GAP at the bottom). The part of a line that
 * goes into the next row is its bright top: not counted. */
static void build_line_levels(ULONG step_y, int dst_h)
{
    ULONG ay = 0;
    int y;

    for (y = 0; y < dst_h; y++) {
        ULONG f = ay & 0xffffUL;
        ULONG e = f + step_y;
        ULONG g0 = 0x10000UL - SHADE_GAP;
        ULONG over = 0;

        if (e > 0x10000UL) {
            e = 0x10000UL;
        }
        if (f < g0) {
            f = g0;
        }
        if (e > f) {
            over = e - f;
        }
        line_level[y] = (UBYTE)((over * (SHADE_LEVELS - 1) + step_y / 2) / step_y);
        ay += step_y;
    }
    line_level_step = step_y;
    line_level_h = dst_h;
}

void cgxscale_set_scanlines(int mode)
{
    scanlines_mode = mode;
}

BOOL cgxscale_prepare(struct BitMap *bm, struct ColorMap *cm)
{
    ULONG pixfmt;
    int bpp;

    dst_bpp = 0;
    if (CyberGfxBase == NULL || bm == NULL
            || !GetCyberMapAttr(bm, CYBRMATTR_ISCYBERGFX)) {
        return FALSE;
    }
    pixfmt = GetCyberMapAttr(bm, CYBRMATTR_PIXFMT);
    switch (pixfmt) {
        case PIXFMT_LUT8:
            bpp = 1;
            break;
        case PIXFMT_RGB15: case PIXFMT_BGR15: case PIXFMT_RGB15PC: case PIXFMT_BGR15PC:
        case PIXFMT_RGB16: case PIXFMT_BGR16: case PIXFMT_RGB16PC: case PIXFMT_BGR16PC:
            bpp = 2;
            break;
        case PIXFMT_RGB24: case PIXFMT_BGR24:
            bpp = 3;
            break;
        case PIXFMT_ARGB32: case PIXFMT_BGRA32: case PIXFMT_RGBA32:
            bpp = 4;
            break;
        default:
            return FALSE;
    }
    dst_pixfmt = pixfmt;
    dst_bpp = bpp;
    dst_cm = cm;
    build_tables();
    return TRUE;
}

void cgxscale_set_palette(const ULONG *rgb, int count)
{
    if (count > 256) {
        count = 256;
    }
    memcpy(src_rgb, rgb, (size_t)count * sizeof(ULONG));
    src_count = count;
    if (dst_bpp != 0) {
        build_tables();
    }
}

/* ------------------------------------------------------------------------- */
/* drawing */

/* what the hook needs, set by cgxscale_draw() */
typedef struct scale_params_s {
    const UBYTE *src;           /* top left of the source area */
    ULONG src_pitch;
    int dst_x, dst_y;           /* destination area, rastport coordinates */
    ULONG step_x, step_y;       /* 16.16 source pixels per destination pixel */
} scale_params_t;

/* the scanline route (cliprect_hook_scanlines()) */
#define SCANLINE_FITS(src_h, dst_h) \
    (dst_bpp >= 2 && (dst_h) <= SHADE_MAX_LINES && (ULONG)(dst_h) * 2 >= (ULONG)(src_h) * 3)

/* message of a DoHookClipRects() hook (layers.doc, InstallLayerHook) */
struct cliprect_msg {
    struct Layer *layer;        /* NULL for a non-layered rastport */
    struct Rectangle bounds;    /* part to draw, bitmap coordinates */
    LONG offsetx;               /* backfill pattern offsets, not used */
    LONG offsety;
};

/* the rows of one part, nl destination lines at a time (amiga_scalerow.h):
 * one loop per pixel size and line count */
#define CGX_ROWS(nl)                                                            \
    switch (dst_bpp) {                                                          \
        case 1: scalerow8(line, s, ax0, p->step_x, w, clut8, nl, bpr); break;   \
        case 2: scalerow16(line, s, ax0, p->step_x, w, clut16, nl, bpr); break; \
        case 3: scalerow24(line, s, ax0, p->step_x, w, clut24, nl, bpr); break; \
        default: scalerow32(line, s, ax0, p->step_x, w, clut32, nl, bpr); break; \
    }

/* called by layers.library for each visible part of the update rectangle */
static ULONG cliprect_hook(register struct Hook *hook __asm("a0"),
                           register struct RastPort *rp __asm("a2"),
                           register struct cliprect_msg *msg __asm("a1"))
{
    const scale_params_t *p = (const scale_params_t *)hook->h_Data;
    UBYTE *base = NULL;
    ULONG bpr = 0;
    APTR lock;
    int w = msg->bounds.MaxX - msg->bounds.MinX + 1;
    int h = msg->bounds.MaxY - msg->bounds.MinY + 1;
    LONG win_x = msg->bounds.MinX;
    LONG win_y = msg->bounds.MinY;
    ULONG ax0, ay;
    UBYTE *line;
    int y, nl, multi;

    if (w <= 0 || h <= 0) {
        return 0;
    }
    /* bitmap -> rastport (window) coordinates: exact for the layer of a
     * simple refresh window, which only gets on-screen cliprects */
    if (msg->layer != NULL) {
        win_x -= msg->layer->bounds.MinX;
        win_y -= msg->layer->bounds.MinY;
    }
    /* position of this part inside the destination area, in 16.16 source */
    ax0 = (ULONG)(win_x - p->dst_x) * p->step_x;
    ay = (ULONG)(win_y - p->dst_y) * p->step_y;

    /* the bitmap of this part: the screen one for simple refresh windows
     * (smart refresh off-screen cliprects would be in another bitmap) */
    lock = LockBitMapTags(rp->BitMap,
                          LBMI_BASEADDRESS, (ULONG)&base,
                          LBMI_BYTESPERROW, (ULONG)&bpr,
                          TAG_DONE);
    if (lock == NULL) {
        return 0;
    }
    line = base + (ULONG)msg->bounds.MinY * bpr + (ULONG)msg->bounds.MinX * (ULONG)dst_bpp;
    /* lines written together: aligned words on each of them */
    multi = (bpr & 3) == 0;

    for (y = 0; y < h; y += nl) {
        const UBYTE *s = p->src + (ay >> 16) * p->src_pitch;

        /* up to 3 lines of the same source row, inside this part */
        nl = scalerow_lines(ay, p->step_y, h - y, multi);
        if (nl == 3) {
            CGX_ROWS(3)
        } else if (nl == 2) {
            CGX_ROWS(2)
        } else {
            CGX_ROWS(1)
        }
        line += bpr * (ULONG)nl;
        ay += p->step_y * (ULONG)nl;
    }
    UnLockBitMap(lock);
    return 0;
}

/* the scanline rows: same as CGX_ROWS() with the tables of level lv */
#define SCANLINE_ROWS(nl)                                                               \
    switch (dst_bpp) {                                                                  \
        case 2: scalerow16(line, s, ax0, p->step_x, w, shade16[lv], nl, bpr); break;    \
        case 3: scalerow24(line, s, ax0, p->step_x, w, shade32[lv], nl, bpr); break;    \
        default: scalerow32(line, s, ax0, p->step_x, w, shade32[lv], nl, bpr); break;   \
    }

/* lines of the same source row and the same level, written together */
SCALEROW_INLINE int scanline_lines(const UBYTE *lev, ULONG ay, ULONG step_y, int left, int multi)
{
    int nl = scalerow_lines(ay, step_y, left, multi);

    if (nl > 1 && lev[1] != lev[0]) {
        return 1;
    }
    if (nl > 2 && lev[2] != lev[0]) {
        return 2;
    }
    return nl;
}

/* cliprect_hook() with the shade table of each line (line_level[]) */
static ULONG cliprect_hook_scanlines(register struct Hook *hook __asm("a0"),
                                     register struct RastPort *rp __asm("a2"),
                                     register struct cliprect_msg *msg __asm("a1"))
{
    const scale_params_t *p = (const scale_params_t *)hook->h_Data;
    UBYTE *base = NULL;
    ULONG bpr = 0;
    APTR lock;
    int w = msg->bounds.MaxX - msg->bounds.MinX + 1;
    int h = msg->bounds.MaxY - msg->bounds.MinY + 1;
    LONG win_x = msg->bounds.MinX;
    LONG win_y = msg->bounds.MinY;
    ULONG ax0, ay;
    UBYTE *line;
    const UBYTE *lev;
    int y, nl, multi;

    if (w <= 0 || h <= 0) {
        return 0;
    }
    if (msg->layer != NULL) {
        win_x -= msg->layer->bounds.MinX;
        win_y -= msg->layer->bounds.MinY;
    }
    ax0 = (ULONG)(win_x - p->dst_x) * p->step_x;
    ay = (ULONG)(win_y - p->dst_y) * p->step_y;
    lev = line_level + (win_y - p->dst_y);

    lock = LockBitMapTags(rp->BitMap,
                          LBMI_BASEADDRESS, (ULONG)&base,
                          LBMI_BYTESPERROW, (ULONG)&bpr,
                          TAG_DONE);
    if (lock == NULL) {
        return 0;
    }
    line = base + (ULONG)msg->bounds.MinY * bpr + (ULONG)msg->bounds.MinX * (ULONG)dst_bpp;
    multi = (bpr & 3) == 0;

    for (y = 0; y < h; y += nl) {
        const UBYTE *s = p->src + (ay >> 16) * p->src_pitch;
        int lv = lev[y];

        nl = scanline_lines(lev + y, ay, p->step_y, h - y, multi);
        if (nl == 3) {
            SCANLINE_ROWS(3)
        } else if (nl == 2) {
            SCANLINE_ROWS(2)
        } else {
            SCANLINE_ROWS(1)
        }
        line += bpr * (ULONG)nl;
        ay += p->step_y * (ULONG)nl;
    }
    UnLockBitMap(lock);
    return 0;
}

#ifdef CGXSCALE_SCANLINES_BILINEAR_ARCHIVE
/* the blended pixel of source column i: rows sa and sb through the tables
 * of their levels */
#define BLEND_PIXEL(i) (ta[sa[i]] + tb[sb[i]])

/* blended rows, one destination line: as scalerow16/24/32() */
SCALEROW_INLINE void blendrow16(UBYTE *d, const UBYTE *sa, const UBYTE *sb, ULONG ax, ULONG step,
                                int n, const ULONG *ta, const ULONG *tb, int swap)
{
    ULONG v;

    if (n > 0 && ((ULONG)d & 2) != 0) {
        v = BLEND_PIXEL(ax >> 16);
        SCALEROW_STORE16(d, swap ? swap16((UWORD)v) : v);
        d += 2;
        ax += step;
        n--;
    }
    while (n >= 2) {
        ULONG p;

        p = BLEND_PIXEL(ax >> 16);
        ax += step;
        v = BLEND_PIXEL(ax >> 16);
        ax += step;
        if (swap) {
            p = swap16((UWORD)p);
            v = swap16((UWORD)v);
        }
        /* big endian: the first pixel is the high word */
        SCALEROW_STORE32(d, (p << 16) | v);
        d += 4;
        n -= 2;
    }
    if (n > 0) {
        v = BLEND_PIXEL(ax >> 16);
        SCALEROW_STORE16(d, swap ? swap16((UWORD)v) : v);
    }
}

SCALEROW_INLINE void blendrow24(UBYTE *d, const UBYTE *sa, const UBYTE *sb, ULONG ax, ULONG step,
                                int n, const ULONG *ta, const ULONG *tb)
{
    while (n > 0 && ((ULONG)d & 3) != 0) {
        ULONG c = BLEND_PIXEL(ax >> 16);

        SCALEROW_STORE8(d, c >> 16);
        SCALEROW_STORE8(d + 1, c >> 8);
        SCALEROW_STORE8(d + 2, c);
        d += 3;
        ax += step;
        n--;
    }
    while (n >= 4) {
        ULONG a, b, c, e;

        a = BLEND_PIXEL(ax >> 16);
        ax += step;
        b = BLEND_PIXEL(ax >> 16);
        ax += step;
        c = BLEND_PIXEL(ax >> 16);
        ax += step;
        e = BLEND_PIXEL(ax >> 16);
        ax += step;
        /* a0 a1 a2 b0 | b1 b2 c0 c1 | c2 e0 e1 e2 */
        SCALEROW_STORE32(d, (a << 8) | (b >> 16));
        SCALEROW_STORE32(d + 4, (b << 16) | (c >> 8));
        SCALEROW_STORE32(d + 8, (c << 24) | e);
        d += 12;
        n -= 4;
    }
    while (n-- > 0) {
        ULONG c = BLEND_PIXEL(ax >> 16);

        SCALEROW_STORE8(d, c >> 16);
        SCALEROW_STORE8(d + 1, c >> 8);
        SCALEROW_STORE8(d + 2, c);
        d += 3;
        ax += step;
    }
}

SCALEROW_INLINE void blendrow32(UBYTE *d, const UBYTE *sa, const UBYTE *sb, ULONG ax, ULONG step,
                                int n, const ULONG *ta, const ULONG *tb, ULONG alpha)
{
    while (n-- > 0) {
        SCALEROW_STORE32(d, BLEND_PIXEL(ax >> 16) | alpha);
        d += 4;
        ax += step;
    }
}

/* cliprect_hook() mixing 2 source rows per line (line_row[], line_la[],
 * line_lb[]), one line at a time */
static ULONG cliprect_hook_blend(register struct Hook *hook __asm("a0"),
                                 register struct RastPort *rp __asm("a2"),
                                 register struct cliprect_msg *msg __asm("a1"))
{
    const scale_params_t *p = (const scale_params_t *)hook->h_Data;
    UBYTE *base = NULL;
    ULONG bpr = 0;
    APTR lock;
    int w = msg->bounds.MaxX - msg->bounds.MinX + 1;
    int h = msg->bounds.MaxY - msg->bounds.MinY + 1;
    LONG win_x = msg->bounds.MinX;
    LONG win_y = msg->bounds.MinY;
    ULONG ax0;
    UBYTE *line;
    const UWORD *row;
    const UBYTE *la, *lb;
    int y;

    if (w <= 0 || h <= 0) {
        return 0;
    }
    if (msg->layer != NULL) {
        win_x -= msg->layer->bounds.MinX;
        win_y -= msg->layer->bounds.MinY;
    }
    ax0 = (ULONG)(win_x - p->dst_x) * p->step_x;
    row = line_row + (win_y - p->dst_y);
    la = line_la + (win_y - p->dst_y);
    lb = line_lb + (win_y - p->dst_y);

    lock = LockBitMapTags(rp->BitMap,
                          LBMI_BASEADDRESS, (ULONG)&base,
                          LBMI_BYTESPERROW, (ULONG)&bpr,
                          TAG_DONE);
    if (lock == NULL) {
        return 0;
    }
    line = base + (ULONG)msg->bounds.MinY * bpr + (ULONG)msg->bounds.MinX * (ULONG)dst_bpp;

    for (y = 0; y < h; y++) {
        const UBYTE *sa = p->src + (ULONG)row[y] * p->src_pitch;
        /* no next row part: level 0, all zero (the next row may not exist) */
        const UBYTE *sb = lb[y] != 0 ? sa + p->src_pitch : sa;
        const ULONG *ta = blend_tab[la[y]];
        const ULONG *tb = blend_tab[lb[y]];

        switch (dst_bpp) {
            case 2:
                if (blend_swap) {
                    blendrow16(line, sa, sb, ax0, p->step_x, w, ta, tb, 1);
                } else {
                    blendrow16(line, sa, sb, ax0, p->step_x, w, ta, tb, 0);
                }
                break;
            case 3:
                blendrow24(line, sa, sb, ax0, p->step_x, w, ta, tb);
                break;
            default:
                blendrow32(line, sa, sb, ax0, p->step_x, w, ta, tb, blend_alpha);
                break;
        }
        line += bpr;
    }
    UnLockBitMap(lock);
    return 0;
}
#endif /* CGXSCALE_SCANLINES_BILINEAR_ARCHIVE */

void cgxscale_draw(struct RastPort *rp, const UBYTE *src, ULONG src_pitch,
                   int src_x, int src_y, int src_w, int src_h,
                   int dst_x, int dst_y, int dst_w, int dst_h,
                   int upd_x0, int upd_y0, int upd_x1, int upd_y1)
{
    struct Hook hook;
    struct Rectangle rect;
    scale_params_t p;

    if (dst_bpp == 0 || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) {
        return;
    }
    /* never outside the destination area (window borders) */
    if (upd_x0 < dst_x) {
        upd_x0 = dst_x;
    }
    if (upd_y0 < dst_y) {
        upd_y0 = dst_y;
    }
    if (upd_x1 > dst_x + dst_w) {
        upd_x1 = dst_x + dst_w;
    }
    if (upd_y1 > dst_y + dst_h) {
        upd_y1 = dst_y + dst_h;
    }
    if (upd_x0 >= upd_x1 || upd_y0 >= upd_y1) {
        return;
    }

    p.src = src + (ULONG)src_y * src_pitch + (ULONG)src_x;
    p.src_pitch = src_pitch;
    p.dst_x = dst_x;
    p.dst_y = dst_y;
    /* the only divisions: one per axis */
    p.step_x = SCALEROW_STEP(src_w, dst_w);
    p.step_y = SCALEROW_STEP(src_h, dst_h);

    memset(&hook, 0, sizeof hook);
    hook.h_Entry = (ULONG (*)())cliprect_hook;
    if (scanlines_mode == CGXSCALE_SCANLINES_SIMPLE && SCANLINE_FITS(src_h, dst_h)) {
        if (!shade_valid) {
            build_shade_tables();
        }
        if (line_level_step != p.step_y || line_level_h != dst_h) {
            build_line_levels(p.step_y, dst_h);
        }
        hook.h_Entry = (ULONG (*)())cliprect_hook_scanlines;
    }
#ifdef CGXSCALE_SCANLINES_BILINEAR_ARCHIVE
    else if (scanlines_mode == CGXSCALE_SCANLINES_BILINEAR && SCANLINE_FITS(src_h, dst_h)) {
        if (!blend_valid) {
            build_blend_tables();
        }
        if (line_blend_step != p.step_y || line_blend_h != dst_h || line_blend_src_h != src_h) {
            build_line_blend(p.step_y, dst_h, src_h);
        }
        hook.h_Entry = (ULONG (*)())cliprect_hook_blend;
    }
#endif /* CGXSCALE_SCANLINES_BILINEAR_ARCHIVE */
    hook.h_Data = &p;

    rect.MinX = (WORD)upd_x0;
    rect.MinY = (WORD)upd_y0;
    rect.MaxX = (WORD)(upd_x1 - 1);
    rect.MaxY = (WORD)(upd_y1 - 1);
    DoHookClipRects(&hook, rp, &rect);
}
