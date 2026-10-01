/** \file   amiga_planarscale.c
 * \brief   AmigaOS 3.x port: scaled drawing into native (planar) bitmaps
 *
 * See amiga_planarscale.h.
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
#include <exec/memory.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include "amiga_planarscale.h"

static struct ColorMap *dst_cm = NULL;

static ULONG src_rgb[256];          /* source palette, 0x00RRGGBB */
static int src_count = 0;
static UBYTE pens[256];             /* source color -> nearest screen pen */
static int pens_identity = 0;       /* pens[i] == i for every source color */

/* chunky buffer, one byte per pixel of the destination area. Grown when
 * needed, never shrunk. */
static UBYTE *chunky = NULL;
static ULONG chunky_size = 0;

static void build_pens(void)
{
    int i;

    for (i = 0; i < src_count; i++) {
        ULONG r = (src_rgb[i] >> 16) & 0xff;
        ULONG g = (src_rgb[i] >> 8) & 0xff;
        ULONG b = src_rgb[i] & 0xff;

        /* nearest existing color, no pen allocated */
        pens[i] = (dst_cm != NULL)
                  ? (UBYTE)FindColor(dst_cm, r * 0x01010101UL, g * 0x01010101UL,
                                     b * 0x01010101UL, -1)
                  : (UBYTE)i;
    }
    pens_identity = 1;
    for (i = 0; i < src_count; i++) {
        if (pens[i] != (UBYTE)i) {
            pens_identity = 0;
            break;
        }
    }
}

BOOL planarscale_prepare(struct ColorMap *cm)
{
    if (GfxBase->LibNode.lib_Version < 40) {
        /* WriteChunkyPixels() is V40 (OS 3.1) */
        return FALSE;
    }
    dst_cm = cm;
    build_pens();
    return TRUE;
}

void planarscale_set_palette(const ULONG *rgb, int count)
{
    if (count > 256) {
        count = 256;
    }
    memcpy(src_rgb, rgb, (size_t)count * sizeof(ULONG));
    src_count = count;
    build_pens();
}

void planarscale_free(void)
{
    if (chunky != NULL) {
        FreeVec(chunky);
        chunky = NULL;
        chunky_size = 0;
    }
}

void planarscale_draw(struct RastPort *rp, const UBYTE *src, ULONG src_pitch,
                      int src_x, int src_y, int src_w, int src_h,
                      int dst_x, int dst_y, int dst_w, int dst_h,
                      int upd_x0, int upd_y0, int upd_x1, int upd_y1)
{
    ULONG step_x, step_y, ax0, ay;
    ULONG size = (ULONG)dst_w * (ULONG)dst_h;
    const UBYTE *src_area;
    UBYTE *line;
    int w, y;

    if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) {
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

    /* 1:1 and pen i is color i (native fullscreen): the emulator buffer is
     * already what the screen wants, no scaling nor copy pass */
    if (pens_identity && src_w == dst_w && src_h == dst_h) {
        WriteChunkyPixels(rp, (ULONG)upd_x0, (ULONG)upd_y0,
                          (ULONG)(upd_x1 - 1), (ULONG)(upd_y1 - 1),
                          (UBYTE *)src + (ULONG)(src_y + upd_y0 - dst_y) * src_pitch
                                       + (ULONG)(src_x + upd_x0 - dst_x),
                          (LONG)src_pitch);
        return;
    }

    /* the buffer follows the destination size, only grows */
    if (size > chunky_size) {
        planarscale_free();
        chunky = AllocVec(size, MEMF_ANY);
        if (chunky == NULL) {
            return;
        }
        chunky_size = size;
    }

    /* the only divisions: one per axis */
    step_x = ((ULONG)src_w << 16) / (ULONG)dst_w;
    step_y = ((ULONG)src_h << 16) / (ULONG)dst_h;
    src_area = src + (ULONG)src_y * src_pitch + (ULONG)src_x;

    ax0 = (ULONG)(upd_x0 - dst_x) * step_x;
    ay = (ULONG)(upd_y0 - dst_y) * step_y;
    w = upd_x1 - upd_x0;
    /* the buffer is laid out like the destination area: dst_w bytes per row */
    line = chunky + (ULONG)(upd_y0 - dst_y) * (ULONG)dst_w + (ULONG)(upd_x0 - dst_x);

    for (y = upd_y0; y < upd_y1; y++) {
        const UBYTE *s = src_area + (ay >> 16) * src_pitch;
        UBYTE *d = line;
        ULONG ax = ax0;
        int n = w;

        while (n-- > 0) {
            *d++ = pens[s[ax >> 16]];
            ax += step_x;
        }
        line += dst_w;
        ay += step_y;
    }

    /* layers clipping and chunky to planar by the OS */
    WriteChunkyPixels(rp, (ULONG)upd_x0, (ULONG)upd_y0,
                      (ULONG)(upd_x1 - 1), (ULONG)(upd_y1 - 1),
                      chunky + (ULONG)(upd_y0 - dst_y) * (ULONG)dst_w + (ULONG)(upd_x0 - dst_x),
                      (LONG)dst_w);
}
