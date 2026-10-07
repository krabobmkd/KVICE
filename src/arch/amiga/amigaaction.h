/** \file   amigaaction.h
 * \brief   AmigaOS 3.x port: table-driven UI actions (menus, keys)
 *
 * Same scheme as EmojiGear egaction: each action has a function and a
 * localized name, menus only store action ids.
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

#ifndef VICE_AMIGAACTION_H
#define VICE_AMIGAACTION_H

#include <exec/types.h>

typedef BOOL (*AmigaActionFunc)(void);

/* \brief  Check/radio state of an action: -1 if it is not a check item */
typedef int (*AmigaActionCheckedFunc)(void);

typedef struct AmigaAction {
    AmigaActionFunc        func;
    AmigaActionCheckedFunc checked;     /* NULL for plain items */
    ULONG                  nameStringID;    /* MSG_* id */
    const char            *name;            /* localized, set by AmigaAction_Init() */
} AmigaAction;

/* Action IDs */
enum {
    AMIGA_ACTION_AUTOSTART = 0,
    AMIGA_ACTION_ATTACH_DISK8,
    AMIGA_ACTION_DETACH_DISK8,
    AMIGA_ACTION_DRIVE8_DRAWER,
    AMIGA_ACTION_ATTACH_CART,
    AMIGA_ACTION_DETACH_CART,
    AMIGA_ACTION_RESET,
    AMIGA_ACTION_HARD_RESET,
    AMIGA_ACTION_SETTINGS,
    AMIGA_ACTION_PAUSE,
    AMIGA_ACTION_QUIT,

    AMIGA_ACTION_DISPLAY_WINDOW,
    AMIGA_ACTION_WINDOW_SIZE_1X1,
    AMIGA_ACTION_WINDOW_SIZE_2X2,
    AMIGA_ACTION_WINDOW_SIZE_3X3,
    AMIGA_ACTION_BORDERS_FULL,
    AMIGA_ACTION_BORDERS_HALF,
    AMIGA_ACTION_BORDERS_NONE,

    AMIGA_ACTION_SNAPSHOT_LOAD,
    AMIGA_ACTION_SNAPSHOT_SAVE,

    AMIGA_ACTION_KEYBOARD_SYMBOLIC,
    AMIGA_ACTION_KEYBOARD_POSITIONAL,

    AMIGA_ACTION_CREATE_DISK8,
    AMIGA_ACTION_EXTRACT_DISK8,
    AMIGA_ACTION_SAVE_BASIC,

    /* Must be last */
    AMIGA_ACTION_COUNT
};

/* cache localized names, call after AmigaLocale_Init() */
void AmigaAction_Init(void);
AmigaAction *AmigaAction_Get(ULONG actionID);
BOOL AmigaAction_Execute(ULONG actionID);
/* 1/0 for check items, -1 for plain items */
int AmigaAction_IsChecked(ULONG actionID);

/* autostart a file (program, disk, tape, cartridge), from a requester or
 * an icon dropped on the emulator window */
BOOL amiga_autostart_file(const char *path);

/* drive 8 reads an Amiga drawer (FSDevice8Dir) instead of a 1541:
 * virtual IEC device on, filesystem device, true drive emulation off */
int amiga_drive8_drawer_get(void);
void amiga_drive8_drawer_set(int on);

#endif
