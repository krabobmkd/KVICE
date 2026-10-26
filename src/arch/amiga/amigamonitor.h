/** \file   amigamonitor.h
 * \brief   AmigaOS 3.x port: MUI monitor window
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

#ifndef VICE_AMIGAMONITOR_H
#define VICE_AMIGAMONITOR_H

#include <exec/types.h>

/* MUIM_Application_ReturnID values of the monitor window, handled by
   amiga_monitor_handle_rid() (amigamui.c gives it this range) */
#define RID_MON_FIRST   100
#define RID_MON_BREAK   100
#define RID_MON_RUN     101
#define RID_MON_STEP    102
#define RID_MON_NEXT    103
#define RID_MON_CLOSE   104
#define RID_MON_MEM_ADDR 105
#define RID_MON_MEM_PREV 106
#define RID_MON_MEM_NEXT 107
#define RID_MON_BP_ADD  108
#define RID_MON_BP_REMOVE 109
#define RID_MON_LABELS  110
#define RID_MON_LAST    199

/* open the monitor window (non-modal), or bring it to front */
void amiga_monitor_open(void);
int amiga_monitor_is_open(void);
void amiga_monitor_handle_rid(ULONG rid);
/* the monitor waits for a command: shows the window and handles the UI
   until a button gives one (lib_malloc'ed), NULL without MUI */
char *amiga_monitor_get_command(void);
/* the CPU stopped (pause) or goes on: update the window if it is open */
void amiga_monitor_refresh(void);
/* monitor text output (uimon_out()) */
void amiga_monitor_out(const char *text);
/* the labels of a program started or loaded from \a path: its .sym file
   next to it (WLA-DX), loaded again each time (it may have been linked
   again), with a message on the emulator screen. Without one, the labels
   loaded this way before go when \a forget (an autostart: another program),
   stay otherwise (LOAD of a data file from the drive 8 drawer). */
void amiga_monitor_autoload_labels(const char *path, int forget);
/* the MUI application is disposed, the window with it */
void amiga_monitor_dispose(void);

#endif
