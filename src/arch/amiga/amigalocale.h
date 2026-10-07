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

    /* Menu: Snapshot */
    MSG_MENU_SNAPSHOT,
    MSG_SNAPSHOT_LOAD,
    MSG_SNAPSHOT_SAVE,

    /* Settings window */
    MSG_SETTINGS_TITLE,
    MSG_SETTINGS_SAVE,
    MSG_SETTINGS_USE,
    MSG_SETTINGS_CANCEL,
    MSG_CATEGORY_INPUT,
    MSG_CATEGORY_KEYBOARD,
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
    MSG_INPUT_APPLY,       /* slot of the former "next start" note */
    MSG_C64_PORT1,
    MSG_C64_PORT2,
    MSG_DEVICE_NONE,

    /* Settings: Keyboard */
    MSG_KEYMAP,
    MSG_KEYMAP_STANDARD,
    MSG_KEYBOARD_MAPPING,
    MSG_KEYMAP_CUSTOM,
    MSG_KEYMAP_FILE,
    MSG_KEYS_C64,
    MSG_KEYS_AMIGA,

    /* Settings: Sound */
    MSG_SOUND_ENABLE,
    MSG_SOUND_RATE,

    /* Settings: Machine */
    MSG_ROM_KERNAL,
    MSG_ROM_BASIC,
    MSG_ROM_CHARGEN,
    MSG_ROM_MODEL_DEFAULTS,
    MSG_C64_MODEL,
    MSG_MODEL_C64_PAL,
    MSG_MODEL_C64C_PAL,
    MSG_MODEL_C64_OLD_PAL,
    MSG_MODEL_C64_NTSC,
    MSG_MODEL_C64C_NTSC,
    MSG_MODEL_C64_OLD_NTSC,
    MSG_MODEL_DREAN,
    MSG_MODEL_INFO,
    MSG_REU,
    MSG_REU_OFF,

    /* Settings: Drive 8 */
    MSG_DRIVE_TYPE,
    MSG_DRIVE_ROM_FILE,
    MSG_DRIVE_ROM_FOUND,
    MSG_DRIVE_ROM_MISSING,
    MSG_DRIVE_NONE,
    MSG_DRIVE_TRUE_EMULATION,
    MSG_AUTOSTART_FAST_LOAD,
    MSG_DRIVE8_USE_DRAWER,
    MSG_DRIVE8_DRAWER,
    MSG_BROWSE,
    MSG_DRIVE8_DRAWER_NOTE,

    /* Settings: Fullscreen page */
    MSG_FS_AUTO_MODE,
    MSG_FS_SCREEN_MODE,
    MSG_FS_MENU,
    MSG_FS_NOTE,

    /* File requesters */
    MSG_REQ_AUTOSTART,
    MSG_REQ_DISK8,
    MSG_REQ_CART,
    MSG_REQ_DRAWER8,
    MSG_REQ_ROM,
    MSG_REQ_KEYMAP,
    MSG_REQ_SNAPSHOT_LOAD,
    MSG_REQ_SNAPSHOT_SAVE,

    /* Errors */
    MSG_ERROR_MENU,
    MSG_ERROR_NO_MUI,
    MSG_ERROR_SETTINGS_WINDOW,
    MSG_ERROR_ROM_FILE,
    MSG_ERROR_OK,
    MSG_ERROR_SNAPSHOT_LOAD,
    MSG_ERROR_SNAPSHOT_SAVE,

    /* emulator screen while files are missing: report values */
    MSG_REPORT_OK,          /* slot of the former "(no rom)" */

    /* report labels and values (ROM labels: MSG_ROM_*) */
    MSG_REPORT_DRIVE8_ROM,
    MSG_REPORT_KEYMAP,
    MSG_REPORT_NOT_FOUND,
    MSG_REPORT_NO_DRIVE,

    /* over the emulated screen */
    MSG_DRIVE8_NO_ROM_USED,

    /* Keyboard: menu, mapping choice */
    MSG_MENU_KEYBOARD,
    MSG_KEYBOARD_SYMBOLIC,
    MSG_KEYBOARD_POSITIONAL,
    MSG_KEYBOARD_NOTE,
    MSG_REPORT_BUILTIN,

    /* C64 menu: new empty disk */
    MSG_CREATE_DISK8,
    MSG_REQ_CREATE_DISK8,
    MSG_CONFIRM_REPLACE,
    MSG_REPLACE_CANCEL,
    MSG_ERROR_CREATE_DISK,

    /* C64 menu: files of the disk in drive 8 to an Amiga drawer */
    MSG_EXTRACT_DISK8,
    MSG_REQ_EXTRACT_DISK8,
    MSG_ERROR_NO_DISK8,
    MSG_ERROR_READ_DISK,
    MSG_EXTRACT_DONE,

    /* C64 menu: the BASIC program in memory to a .prg file */
    MSG_SAVE_BASIC,
    MSG_REQ_SAVE_BASIC,
    MSG_ERROR_NO_BASIC,
    MSG_ERROR_SAVE_BASIC,

    /* Settings: Input, what drives a C64 control port */
    MSG_C64_FROM_MOUSE_PORT,
    MSG_C64_FROM_JOYSTICK_PORT,

    /* Must be last */
    MSG_COUNT
};

/* catalogName may be NULL (English only) */
BOOL AmigaLocale_Init(const char *catalogName, ULONG version);
void AmigaLocale_Close(void);
const char *AmigaLocale_GetString(ULONG stringID);

#define LOC(id) AmigaLocale_GetString(id)

#endif
