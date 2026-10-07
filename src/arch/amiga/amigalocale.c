/** \file   amigalocale.c
 * \brief   AmigaOS 3.x port: localized UI strings (locale.library catalog)
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

#include "vice.h"

#include <proto/exec.h>
#include <proto/locale.h>
#include <libraries/locale.h>

#include "amigalocale.h"

/* Default English strings - MUST stay in the same order as the MSG_* enum */
static const char *default_strings[] = {
    /* MSG_WINDOW_TITLE */
    "VICE x64",

    /* MSG_MENU_C64 */
    "C64",
    /* MSG_AUTOSTART */
    "Autostart...",
    /* MSG_ATTACH_DISK8 */
    "Attach disk image to drive 8...",
    /* MSG_DETACH_DISK8 */
    "Detach disk image from drive 8",
    /* MSG_DRIVE8_DRAWER_DOTS */
    "Use an Amiga drawer as drive 8...",
    /* MSG_ATTACH_CART */
    "Attach cartridge...",
    /* MSG_DETACH_CART */
    "Detach cartridge",
    /* MSG_RESET */
    "Reset",
    /* MSG_HARD_RESET */
    "Hard reset",
    /* MSG_SETTINGS_DOTS */
    "Settings...",
    /* MSG_PAUSE */
    "Pause",
    /* MSG_QUIT */
    "Quit",

    /* MSG_MENU_DISPLAY */
    "Display",
    /* MSG_DISPLAY_WINDOW */
    "Window",
    /* MSG_WINDOW_SIZE */
    "Window Size",
    /* MSG_WINDOW_SIZE_1X1 */
    "1x1",
    /* MSG_WINDOW_SIZE_2X2 */
    "2x2",
    /* MSG_WINDOW_SIZE_3X3 */
    "3x3",
    /* MSG_BORDERS */
    "Borders",
    /* MSG_BORDERS_FULL */
    "Full",
    /* MSG_BORDERS_HALF */
    "Half",
    /* MSG_BORDERS_NONE */
    "None",

    /* MSG_MENU_SNAPSHOT */
    "Snapshot",
    /* MSG_SNAPSHOT_LOAD */
    "Load snapshot...",
    /* MSG_SNAPSHOT_SAVE */
    "Save snapshot...",

    /* MSG_SETTINGS_TITLE */
    "VICE Settings",
    /* MSG_SETTINGS_SAVE */
    "Save",
    /* MSG_SETTINGS_USE */
    "Use",
    /* MSG_SETTINGS_CANCEL */
    "Cancel",
    /* MSG_CATEGORY_INPUT */
    "Input",
    /* MSG_CATEGORY_KEYBOARD */
    "Keyboard",
    /* MSG_CATEGORY_SOUND */
    "Sound",
    /* MSG_CATEGORY_MACHINE */
    "Machine",
    /* MSG_CATEGORY_DRIVE8 */
    "Drive 8",
    /* MSG_CATEGORY_FULLSCREEN */
    "Fullscreen",

    /* MSG_AMIGA_PORT0 */
    "Amiga mouse port:",
    /* MSG_AMIGA_PORT1 */
    "Amiga joystick port:",
    /* MSG_AMIGA_PORT_NONE */
    "Not used",
    /* MSG_AMIGA_PORT_JOYSTICK */
    "Joystick / CD32 pad",
    /* MSG_AMIGA_PORT_PADDLES */
    "Analog paddles",
    /* MSG_AMIGA_PORT_NEXT_START */
    "Amiga port changes apply at the next start.",
    /* MSG_C64_PORT1 */
    "C64 control port 1:",
    /* MSG_C64_PORT2 */
    "C64 control port 2:",
    /* MSG_DEVICE_NONE */
    "None",

    /* MSG_KEYMAP */
    "Keymap:",
    /* MSG_KEYMAP_SYM */
    "Symbolic (amiga_sym.vkm)",
    /* MSG_KEYMAP_POS */
    "Positional (amiga_pos.vkm)",
    /* MSG_KEYMAP_CUSTOM */
    "Custom file",
    /* MSG_KEYMAP_FILE */
    "Custom keymap file:",
    /* MSG_KEYS_C64 */
    "C64 key",
    /* MSG_KEYS_AMIGA */
    "Amiga key",

    /* MSG_SOUND_ENABLE */
    "Sound emulation:",
    /* MSG_SOUND_RATE */
    "Sample rate:",

    /* MSG_ROM_KERNAL */
    "Kernal ROM:",
    /* MSG_ROM_BASIC */
    "BASIC ROM:",
    /* MSG_ROM_CHARGEN */
    "Character ROM:",
    /* MSG_ROM_DEFAULT */
    "Default ROMs",
    /* MSG_C64_MODEL */
    "C64 model:",
    /* MSG_MODEL_C64_PAL */
    "C64 PAL",
    /* MSG_MODEL_C64C_PAL */
    "C64C PAL",
    /* MSG_MODEL_C64_OLD_PAL */
    "C64 old PAL",
    /* MSG_MODEL_C64_NTSC */
    "C64 NTSC",
    /* MSG_MODEL_C64C_NTSC */
    "C64C NTSC",
    /* MSG_MODEL_C64_OLD_NTSC */
    "C64 old NTSC",
    /* MSG_MODEL_DREAN */
    "Drean (PAL-N)",

    /* MSG_DRIVE_TYPE */
    "Drive type:",
    /* MSG_DRIVE_NONE */
    "None",
    /* MSG_DRIVE_TRUE_EMULATION */
    "True drive emulation:",
    /* MSG_AUTOSTART_FAST_LOAD */
    "Autostart loads without true drive emulation:",
    /* MSG_DRIVE8_USE_DRAWER */
    "Read an Amiga drawer:",
    /* MSG_DRIVE8_DRAWER */
    "Amiga drawer:",
    /* MSG_BROWSE */
    "...",
    /* MSG_DRIVE8_DRAWER_NOTE */
    "Reading a drawer turns true drive emulation off.",

    /* MSG_FS_AUTO_MODE */
    "Automatic screen mode:",
    /* MSG_FS_SCREEN_MODE */
    "Screen mode:",
    /* MSG_FS_MENU */
    "Allow menu in fullscreen mode:",
    /* MSG_FS_NOTE */
    "Amiga+F switches between the window and the fullscreen.\n"
    "Without menu: Amiga+P pause, Amiga+R reset,\n"
    "Amiga+A autostart, Amiga+L/W load/save snapshot, Amiga+Q quit.",

    /* MSG_REQ_AUTOSTART */
    "Autostart a program, disk, tape or cartridge",
    /* MSG_REQ_DISK8 */
    "Disk image for drive 8",
    /* MSG_REQ_CART */
    "Cartridge image",
    /* MSG_REQ_DRAWER8 */
    "Amiga drawer for drive 8",
    /* MSG_REQ_ROM */
    "ROM file",
    /* MSG_REQ_KEYMAP */
    "Keymap file (.vkm)",
    /* MSG_REQ_SNAPSHOT_LOAD */
    "Load a snapshot",
    /* MSG_REQ_SNAPSHOT_SAVE */
    "Save a snapshot",

    /* MSG_ERROR_MENU */
    "Cannot create the menus.",
    /* MSG_ERROR_NO_MUI */
    "Settings need MUI (muimaster.library).",
    /* MSG_ERROR_SETTINGS_WINDOW */
    "Cannot open the settings window.",
    /* MSG_ERROR_ROM_FILE: %s is the label, then the file */
    "%s\nFile not found or wrong size:\n%s",
    /* MSG_ERROR_OK */
    "OK",
    /* MSG_ERROR_SNAPSHOT_LOAD */
    "Cannot load the snapshot\n%s",
    /* MSG_ERROR_SNAPSHOT_SAVE */
    "Cannot save the snapshot\n%s",

    /* MSG_NO_ROM */
    "(no rom)"
};

/* compile-time check: one default string per MSG_* id */
typedef char default_strings_size_check[
    (sizeof default_strings / sizeof default_strings[0] == MSG_COUNT) ? 1 : -1];

/* auto-opened by libnix */
extern struct LocaleBase *LocaleBase;

static struct Catalog *catalog = NULL;

BOOL AmigaLocale_Init(const char *catalogName, ULONG version)
{
    if (LocaleBase != NULL && catalogName != NULL && catalog == NULL) {
        catalog = OpenCatalog(NULL, (STRPTR)catalogName,
                              OC_Version, version,
                              OC_BuiltInLanguage, (ULONG)"english",
                              TAG_DONE);
    }
    return TRUE;
}

void AmigaLocale_Close(void)
{
    if (catalog != NULL) {
        CloseCatalog(catalog);
        catalog = NULL;
    }
}

const char *AmigaLocale_GetString(ULONG stringID)
{
    if (stringID >= MSG_COUNT) {
        return "???";
    }
    if (LocaleBase != NULL && catalog != NULL) {
        return (const char *)GetCatalogStr(catalog, (LONG)stringID,
                                           (STRPTR)default_strings[stringID]);
    }
    return default_strings[stringID];
}
