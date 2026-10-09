/** \file   amigamui.h
 * \brief   AmigaOS 3.x port: MUI settings window
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

#ifndef VICE_AMIGAMUI_H
#define VICE_AMIGAMUI_H

#include <exec/types.h>

/* open the settings window (non-modal) */
void amiga_settings_open(void);
/* signals to add to the main loop Wait(), 0 when MUI is idle */
//ULONG amiga_mui_signal_mask(void);
/* process MUI input, when one of amiga_mui_signal_mask() is set */
void amiga_mui_handle_events(void);
/* open the about window (non-modal) */
void amiga_about_open(void);
/* free MUI resources, safe to call more than once */
void amiga_mui_close_all(void);
/* "no rom" state: when ROMs are missing, show "(no rom)", open the settings
   and handle the UI until they are all loaded (called before the CPU loop) */
void amiga_wait_for_roms(void);

extern ULONG mui_sigs;

#endif
