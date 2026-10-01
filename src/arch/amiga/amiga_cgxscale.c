/** \file   amiga_cgxscale.c
 * \brief   AmigaOS 3.x port: scaled drawing into cybergraphics bitmaps
 *
 * See amiga_cgxscale.h. The pixel format only changes the color table:
 * the drawing hooks are per pixel size (1, 2, 3, 4 bytes).
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
static UBYTE clut24[256][3];            /* 3 bytes per pixel, in memory order */
static ULONG clut32[256];               /* 4 bytes per pixel */

static UWORD swap16(UWORD v)
{
    return (UWORD)((v << 8) | (v >> 8));
}

static void build_tables(void)
{
    int i;

    for (i = 0; i < src_count; i++) {
        ULONG r = (src_rgb[i] >> 16) & 0xff;
        ULONG g = (src_rgb[i] >> 8) & 0xff;
        ULONG b = src_rgb[i] & 0xff;
        UWORD rgb15 = (UWORD)(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
        UWORD bgr15 = (UWORD)(((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3));
        UWORD rgb16 = (UWORD)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        UWORD bgr16 = (UWORD)(((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3));

        switch (dst_pixfmt) {
            case PIXFMT_LUT8:
                clut8[i] = (dst_cm != NULL)
                           ? (UBYTE)FindColor(dst_cm, r * 0x01010101UL, g * 0x01010101UL,
                                              b * 0x01010101UL, -1)
                           : (UBYTE)i;
                break;
            case PIXFMT_RGB15:   clut16[i] = rgb15; break;
            case PIXFMT_BGR15:   clut16[i] = bgr15; break;
            case PIXFMT_RGB15PC: clut16[i] = swap16(rgb15); break;
            case PIXFMT_BGR15PC: clut16[i] = swap16(bgr15); break;
            case PIXFMT_RGB16:   clut16[i] = rgb16; break;
            case PIXFMT_BGR16:   clut16[i] = bgr16; break;
            case PIXFMT_RGB16PC: clut16[i] = swap16(rgb16); break;
            case PIXFMT_BGR16PC: clut16[i] = swap16(bgr16); break;
            case PIXFMT_RGB24:
                clut24[i][0] = (UBYTE)r; clut24[i][1] = (UBYTE)g; clut24[i][2] = (UBYTE)b;
                break;
            case PIXFMT_BGR24:
                clut24[i][0] = (UBYTE)b; clut24[i][1] = (UBYTE)g; clut24[i][2] = (UBYTE)r;
                break;
            case PIXFMT_ARGB32: clut32[i] = 0xff000000UL | (r << 16) | (g << 8) | b; break;
            case PIXFMT_BGRA32: clut32[i] = (b << 24) | (g << 16) | (r << 8) | 0xffUL; break;
            case PIXFMT_RGBA32: clut32[i] = (r << 24) | (g << 16) | (b << 8) | 0xffUL; break;
            default:
                break;
        }
    }
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

/* message of a DoHookClipRects() hook (layers.doc, InstallLayerHook) */
struct cliprect_msg {
    struct Layer *layer;        /* NULL for a non-layered rastport */
    struct Rectangle bounds;    /* part to draw, bitmap coordinates */
    LONG offsetx;               /* backfill pattern offsets, not used */
    LONG offsety;
};

/* one row of destination pixels, per pixel size */

static void row8(UBYTE *d, const UBYTE *s, ULONG ax, ULONG step, int n)
{
    while (n-- > 0) {
        *d++ = clut8[s[ax >> 16]];
        ax += step;
    }
}

static void row16(UWORD *d, const UBYTE *s, ULONG ax, ULONG step, int n)
{
    while (n-- > 0) {
        *d++ = clut16[s[ax >> 16]];
        ax += step;
    }
}

static void row24(UBYTE *d, const UBYTE *s, ULONG ax, ULONG step, int n)
{
    while (n-- > 0) {
        const UBYTE *c = clut24[s[ax >> 16]];

        d[0] = c[0];
        d[1] = c[1];
        d[2] = c[2];
        d += 3;
        ax += step;
    }
}

static void row32(ULONG *d, const UBYTE *s, ULONG ax, ULONG step, int n)
{
    while (n-- > 0) {
        *d++ = clut32[s[ax >> 16]];
        ax += step;
    }
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
    int y;

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

    for (y = 0; y < h; y++) {
        const UBYTE *s = p->src + (ay >> 16) * p->src_pitch;

        switch (dst_bpp) {
            case 1: row8(line, s, ax0, p->step_x, w); break;
            case 2: row16((UWORD *)line, s, ax0, p->step_x, w); break;
            case 3: row24(line, s, ax0, p->step_x, w); break;
            default: row32((ULONG *)line, s, ax0, p->step_x, w); break;
        }
        line += bpr;
        ay += p->step_y;
    }
    UnLockBitMap(lock);
    return 0;
}

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
    p.step_x = ((ULONG)src_w << 16) / (ULONG)dst_w;
    p.step_y = ((ULONG)src_h << 16) / (ULONG)dst_h;

    memset(&hook, 0, sizeof hook);
    hook.h_Entry = (ULONG (*)())cliprect_hook;
    hook.h_Data = &p;

    rect.MinX = (WORD)upd_x0;
    rect.MinY = (WORD)upd_y0;
    rect.MaxX = (WORD)(upd_x1 - 1);
    rect.MaxY = (WORD)(upd_y1 - 1);
    DoHookClipRects(&hook, rp, &rect);
}
