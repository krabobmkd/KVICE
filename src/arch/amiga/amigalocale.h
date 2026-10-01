/** \file   amigalocale.h
 * \brief   AmigaOS 3.x port: localized UI strings (locale.library catalog)
 *
 * Same scheme as EmojiGear eglocale: built-in English strings, replaced by
 * the "vice.catalog" strings when locale.library finds one.
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

#ifndef VICE_AMIGALOCALE_H
#define VICE_AMIGALOCALE_H

#include <exec/types.h>

/* String IDs - must stay in the same order as default_strings[] in
 * amigalocale.c, and are the catalog string numbers */
enum {
    MSG_WINDOW_TITLE = 0,

    /* Menu: C64 */
    MSG_MENU_C64,
    MSG_AUTOSTART,
    MSG_ATTACH_DISK8,
    MSG_DETACH_DISK8,
    MSG_DRIVE8_DRAWER_DOTS,
    MSG_ATTACH_CART,
    MSG_DETACH_CART,
    MSG_RESET,
    MSG_HARD_RESET,
    MSG_SETTINGS_DOTS,
    MSG_PAUSE,
    MSG_QUIT,

    /* Menu: Display */
    MSG_MENU_DISPLAY,
    MSG_DISPLAY_WINDOW,
    MSG_WINDOW_SIZE,
    MSG_WINDOW_SIZE_1X1,
    MSG_WINDOW_SIZE_2X2,
    MSG_WINDOW_SIZE_3X3,
    MSG_BORDERS,
    MSG_BORDERS_FULL,
    MSG_BORDERS_HALF,
    MSG_BORDERS_NONE,

    /* Settings window */
    MSG_SETTINGS_TITLE,
    MSG_SETTINGS_SAVE,
    MSG_SETTINGS_USE,
    MSG_SETTINGS_CANCEL,
    MSG_CATEGORY_INPUT,
    MSG_CATEGORY_SOUND,
    MSG_CATEGORY_MACHINE,
    MSG_CATEGORY_DRIVE8,
    MSG_CATEGORY_FULLSCREEN,

    /* Settings: Input */
    MSG_AMIGA_PORT0,
    MSG_AMIGA_PORT1,
    MSG_AMIGA_PORT_NONE,
    MSG_AMIGA_PORT_JOYSTICK,
    MSG_AMIGA_PORT_PADDLES,
    MSG_AMIGA_PORT_NEXT_START,
    MSG_C64_PORT1,
    MSG_C64_PORT2,
    MSG_DEVICE_NONE,

    /* Settings: Sound */
    MSG_SOUND_ENABLE,
    MSG_SOUND_RATE,

    /* Settings: Machine */
    MSG_C64_MODEL,
    MSG_MODEL_C64_PAL,
    MSG_MODEL_C64C_PAL,
    MSG_MODEL_C64_OLD_PAL,
    MSG_MODEL_C64_NTSC,
    MSG_MODEL_C64C_NTSC,
    MSG_MODEL_C64_OLD_NTSC,
    MSG_MODEL_DREAN,

    /* Settings: Drive 8 */
    MSG_DRIVE_TYPE,
    MSG_DRIVE_NONE,
    MSG_DRIVE_TRUE_EMULATION,
    MSG_DRIVE8_USE_DRAWER,
    MSG_DRIVE8_DRAWER,
    MSG_BROWSE,
    MSG_DRIVE8_DRAWER_NOTE,

    /* Settings: Fullscreen page */
    MSG_FS_AUTO_MODE,
    MSG_FS_SCREEN_MODE,
    MSG_FS_NOTE,

    /* File requesters */
    MSG_REQ_AUTOSTART,
    MSG_REQ_DISK8,
    MSG_REQ_CART,
    MSG_REQ_DRAWER8,

    /* Errors */
    MSG_ERROR_MENU,
    MSG_ERROR_NO_MUI,
    MSG_ERROR_SETTINGS_WINDOW,

    /* Must be last */
    MSG_COUNT
};

/* catalogName may be NULL (English only) */
BOOL AmigaLocale_Init(const char *catalogName, ULONG version);
void AmigaLocale_Close(void);
const char *AmigaLocale_GetString(ULONG stringID);

#define LOC(id) AmigaLocale_GetString(id)

#endif
