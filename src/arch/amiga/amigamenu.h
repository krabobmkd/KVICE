/** \file   amigamenu.h
 * \brief   AmigaOS 3.x port: GadTools menu strip of the emulator window
 *
 * Same scheme as EmojiGear egmenu: nm_UserData holds an ACTION_* id in the
 * upper 16 bits (ACTION_UD()), or a MSG_* locale id in the lower 16 bits for
 * titles and branches.
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

#ifndef VICE_AMIGAMENU_H
#define VICE_AMIGAMENU_H

#include <exec/types.h>
#include <intuition/intuition.h>

/* create the menus and attach them to the window */
BOOL AmigaMenu_Create(struct Window *window);
/* detach from the window and free, safe to call twice */
void AmigaMenu_Close(struct Window *window);
/* IDCMP_MENUPICK: run the actions of all picked items */
void AmigaMenu_HandlePick(struct Window *window, UWORD menuCode);
/* update check marks from the action states */
void AmigaMenu_SyncChecks(struct Window *window);

#endif
