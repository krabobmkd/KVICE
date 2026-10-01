/** \file   amiga_planarscale.h
 * \brief   AmigaOS 3.x port: scaled drawing into native (planar) bitmaps
 *
 * Fallback of amiga_cgxscale.c when there is no cybergraphics.library or
 * the window bitmap is a native Amiga (planar, indexed) bitmap:
 * - the 8 bit VICE draw buffer is scaled with 16.16 fixed point stepping
 *   into a chunky buffer in fast RAM, the size of the window inner area,
 *   colors remapped to the nearest screen pens (graphics FindColor()),
 * - graphics.library WriteChunkyPixels() (V40) then writes it to the window
 *   rastport: it handles the window layering and the chunky to planar.
 * Only the dirty part is scaled and written.
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

#ifndef VICE_AMIGA_PLANARSCALE_H
#define VICE_AMIGA_PLANARSCALE_H

#include <exec/types.h>
#include <graphics/rastport.h>
#include <graphics/view.h>

/* \a cm: the screen colormap. FALSE if WriteChunkyPixels() is missing
 * (graphics.library older than V40, OS 3.0). */
BOOL planarscale_prepare(struct ColorMap *cm);

/* palette of the source pixels, 0x00RRGGBB per entry */
void planarscale_set_palette(const ULONG *rgb, int count);

/* same parameters as cgxscale_draw() */
void planarscale_draw(struct RastPort *rp, const UBYTE *src, ULONG src_pitch,
                      int src_x, int src_y, int src_w, int src_h,
                      int dst_x, int dst_y, int dst_w, int dst_h,
                      int upd_x0, int upd_y0, int upd_x1, int upd_y1);

/* free the chunky buffer, safe to call more than once */
void planarscale_free(void);

#endif
