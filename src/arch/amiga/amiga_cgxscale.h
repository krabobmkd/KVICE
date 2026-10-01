/** \file   amiga_cgxscale.h
 * \brief   AmigaOS 3.x port: scaled drawing into cybergraphics bitmaps
 *
 * Draws the 8 bit palettized VICE draw buffer, scaled to any size, straight
 * into the video memory of a cybergraphics (RTG) window:
 * - layers.library DoHookClipRects(): the hook is called for each visible
 *   part of the window (the window may be partly hidden by others),
 * - cybergraphics LockBitMapTags(): direct CPU writes in the bitmap,
 * - 16.16 fixed point stepping: only one division per axis and redraw,
 * - per pixel format: a table of the 256 colors already in the screen pixel
 *   format, so drawing is "lookup + store" for 1, 2, 3 or 4 bytes per pixel.
 *   CLUT (8 bit) screens get the nearest existing pens with FindColor().
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

#ifndef VICE_AMIGA_CGXSCALE_H
#define VICE_AMIGA_CGXSCALE_H

#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/view.h>

/* Read the pixel format of a window bitmap. Returns TRUE if it is a
 * cybergraphics bitmap in a format we can draw to. \a cm is the screen
 * colormap, used for 8 bit (CLUT) screens. */
BOOL cgxscale_prepare(struct BitMap *bm, struct ColorMap *cm);

/* palette of the source pixels, 0x00RRGGBB per entry */
void cgxscale_set_palette(const ULONG *rgb, int count);

/* Draw source area (src_x, src_y, src_w, src_h) of the 8 bit buffer
 * \a src (bytes per row \a src_pitch), scaled to the destination area
 * (dst_x, dst_y, dst_w, dst_h) in rastport coordinates. Only the
 * destination part (upd_x0, upd_y0) - (upd_x1, upd_y1), exclusive bounds,
 * is redrawn. */
void cgxscale_draw(struct RastPort *rp, const UBYTE *src, ULONG src_pitch,
                   int src_x, int src_y, int src_w, int src_h,
                   int dst_x, int dst_y, int dst_w, int dst_h,
                   int upd_x0, int upd_y0, int upd_x1, int upd_y1);

#endif
