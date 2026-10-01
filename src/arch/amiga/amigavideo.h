/** \file   amigavideo.h
 * \brief   AmigaOS 3.x video window, functions shared with the UI code
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

#ifndef VICE_AMIGAVIDEO_H
#define VICE_AMIGAVIDEO_H

#include <exec/types.h>
#include <intuition/intuition.h>

void amiga_video_handle_events(void);
void amiga_video_close_all(void);
ULONG amiga_video_signal_mask(void);
struct Window *amiga_video_window(void);

/* window scale (1 to 3) from the Display menu */
void amiga_video_set_scale(int scale);
int amiga_video_get_scale(void);

/* shown border (Display menu) */
#define AMIGA_BORDERS_FULL 0
#define AMIGA_BORDERS_HALF 1
#define AMIGA_BORDERS_NONE 2
void amiga_video_set_borders(int mode);
int amiga_video_get_borders(void);

/* window <-> fullscreen (F10, Display menu): done at the next event round */
void amiga_video_set_fullscreen(int on);
int amiga_video_is_fullscreen(void);

/* soundahi.c */
int amiga_ahi_stats(unsigned long *requests, unsigned long *underruns,
                    unsigned long *used_frames);

#endif
