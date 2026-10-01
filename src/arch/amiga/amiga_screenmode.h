/** \file   amiga_screenmode.h
 * \brief   AmigaOS 3.x port: fullscreen display mode helpers
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

#ifndef VICE_AMIGA_SCREENMODE_H
#define VICE_AMIGA_SCREENMODE_H

#include <exec/types.h>

/* automatic choice: smallest RTG mode showing width x height (8 bit first,
 * then 16 and 32 bit), else a native mode: 32 or 16 colors when
 * colors_needed <= 16, else 256/32/16. Returns INVALID_ID if nothing fits. */
ULONG amiga_screenmode_auto(int width, int height, int colors_needed, int *depth);

/* 1 for a cybergraphics (RTG) mode */
int amiga_screenmode_is_rtg(ULONG mode_id);

/* native palette mode: 5 bitplanes (32 colors) when possible, else 4 */
int amiga_screenmode_native_depth(ULONG mode_id);

/* nominal size of a mode, 0 on failure */
int amiga_screenmode_size(ULONG mode_id, int *width, int *height);

/* depth to open a mode with: deep RTG modes need their own depth (24 for 32
 * bit), asked is used for palette modes. */
int amiga_screenmode_depth(ULONG mode_id, int asked);

/* display database name of a mode, "0x%08lx" if it has none */
void amiga_screenmode_name(ULONG mode_id, char *buf, int size);

#endif
