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
//ULONG amiga_video_signal_mask(void);
extern ULONG currentUIWaitBit;

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

/* "no rom" state: the emulator screen shows a report instead of the
   emulated screen, one line per entry: "label  value", the value in red
   when bad. A NULL label is an empty line. The strings must stay valid
   while shown (localized ones). count 0: back to the emulated screen. */
typedef struct amiga_report_line_s {
    const char *label;
    const char *value;
    int bad;
} amiga_report_line_t;

#define AMIGA_REPORT_LINES_MAX 12
void amiga_video_show_report(const amiga_report_line_t *lines, int count);

/* menu check marks again from the states (a setting changed elsewhere) */
void amiga_video_sync_menu(void);

/* a message in a red box over the emulated screen, for \a frames refreshes
   (the string must stay valid that long). Shown again: the time restarts. */
void amiga_video_show_message(const char *text, int frames);

/* around a requester or the settings window opened on the Workbench (or
   default public) screen: shown in front of the fullscreen, then back */
void amiga_video_requester_begin(void);
void amiga_video_requester_end(void);

/* window <-> fullscreen (F10, Display menu): done at the next event round */
void amiga_video_set_fullscreen(int on);
int amiga_video_is_fullscreen(void);

/* soundahi.c */
int amiga_ahi_stats(unsigned long *requests, unsigned long *underruns,
                    unsigned long *used_frames);

#endif
