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
    "KVICE x64",

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
    "KVICE Settings",
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
    /* MSG_INPUT_APPLY */
    "Apply input configuration",
    /* MSG_C64_PORT1 */
    "C64 control port 1:",
    /* MSG_C64_PORT2 */
    "C64 control port 2:",
    /* MSG_DEVICE_NONE */
    "None",

    /* MSG_KEYMAP */
    "Positional keymap file:",
    /* MSG_KEYMAP_STANDARD */
    "Standard (amiga_positional.vkm)",
    /* MSG_KEYBOARD_MAPPING: not used any more (Keyboard menu), slot kept */
    "Keyboard mapping:",
    /* MSG_KEYMAP_CUSTOM */
    "Custom file",
    /* MSG_KEYMAP_FILE */
    "Custom positional keymap:",
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
    /* MSG_ROM_MODEL_DEFAULTS */
    "Set ROM defaults for this model",
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
    /* MSG_MODEL_INFO: video standard, SID chip */
    "Video: %s, SID: %s",
    /* MSG_REU */
    "RAM cart (REU):",
    /* MSG_REU_OFF */
    "None",

    /* MSG_DRIVE_TYPE */
    "Drive type:",
    /* MSG_DRIVE_ROM_FILE: label of the line under the drive type */
    "ROM file:",
    /* MSG_DRIVE_ROM_FOUND */
    "(found)",
    /* MSG_DRIVE_ROM_MISSING */
    "(not found)",
    /* MSG_DRIVE_NONE */
    "None",
    /* MSG_DRIVE_TRUE_EMULATION */
    "True drive emulation:",
    /* MSG_AUTOSTART_FAST_LOAD */
    "Autostart without true drive emu:",
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
    "Amiga+F switches between window and fullscreen.\n"
    "Without menu: Amiga+P pause, Amiga+R reset,\n"
    "Amiga+A autostart, Amiga+L/W load/save snapshot,\n Amiga+Q quit.",

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

    /* MSG_REPORT_OK */
    "Ok",

    /* MSG_REPORT_DRIVE8_ROM */
    "Drive 8 ROM:",
    /* MSG_REPORT_KEYMAP */
    "Keyboard file:",
    /* MSG_REPORT_NOT_FOUND */
    "Not found",
    /* MSG_REPORT_NO_DRIVE */
    "No drive",

    /* MSG_DRIVE8_NO_ROM_USED */
    "Drive 8 ROM not found: no disk drive",

    /* MSG_MENU_KEYBOARD */
    "Keyboard",
    /* MSG_KEYBOARD_SYMBOLIC */
    "Symbolic",
    /* MSG_KEYBOARD_POSITIONAL */
    "Positional",
    /* MSG_KEYBOARD_NOTE */
    "Keyboard menu, Symbolic:\n the keys type the characters of the Amiga keymap.\n"
    "Positional: the C64 key positions (keymap file), for games.\n"
    "Symbolic only takes the special keys from the keymap file\n"
    "(Shift, C=, CTRL, Run/Stop, Clr/Home, Restore, F-keys...).",
    /* MSG_REPORT_BUILTIN: keymap file not found, the built-in one is used */
    "Built-in",

    /* MSG_CREATE_DISK8 */
    "Create empty disk and mount...",
    /* MSG_REQ_CREATE_DISK8 */
    "New empty disk image (.d64)",
    /* MSG_CONFIRM_REPLACE: %s is the file */
    "%s\nalready exists. Replace it?",
    /* MSG_REPLACE_CANCEL */
    "Replace|Cancel",
    /* MSG_ERROR_CREATE_DISK */
    "Cannot create the disk image\n%s",

    /* MSG_EXTRACT_DISK8 */
    "Extract disk files...",
    /* MSG_REQ_EXTRACT_DISK8 */
    "Drawer for the files of the disk",
    /* MSG_ERROR_NO_DISK8: %s is empty */
    "No disk image in drive 8.%s",
    /* MSG_ERROR_READ_DISK: %s is the disk image */
    "Cannot read the disk image\n%s",
    /* MSG_EXTRACT_DONE: files written, files not written, drawer */
    "%ld file(s) extracted, %ld error(s), to\n%s",

    /* MSG_SAVE_BASIC */
    "Save program as .prg...",
    /* MSG_REQ_SAVE_BASIC */
    "Save the BASIC program (.prg)",
    /* MSG_ERROR_NO_BASIC: %s is empty */
    "No BASIC program in memory.%s",
    /* MSG_ERROR_SAVE_BASIC: %s is the file */
    "Cannot save the BASIC program\n%s",

    /* MSG_C64_FROM_MOUSE_PORT */
    "Amiga mouse port",
    /* MSG_C64_FROM_JOYSTICK_PORT */
    "Amiga joystick port",

    /* MSG_MODEL */
    "Model:",
    /* MSG_MODEL_INFO_RAM: video standard, RAM size in KB */
    "Video: %s, RAM: %d KB",
    /* MSG_RAM_EXPANSION */
    "RAM expansion:",

    /* MSG_SAVE_SCREENSHOT */
    "Save screenshot...",
    /* MSG_REQ_SAVE_SCREENSHOT */
    "Save screenshot (IFF ILBM picture)",
    /* MSG_ERROR_SAVE_SCREENSHOT: %s is the file */
    "Cannot save the screenshot\n%s",

    /* MSG_MENU_BASIC */
    "BASIC",
    /* MSG_MENU_DRIVE8 */
    "Drive 8",
    /* MSG_SAVE_BAS */
    "Save program as .bas...",
    /* MSG_LOAD_BAS */
    "Load program as .bas...",
    /* MSG_ABOUT */
    "About...",
    /* MSG_ABOUT_TITLE */
    "About KVICE",
    /* MSG_REQ_SAVE_BAS */
    "Save the BASIC program as text (.bas, UTF-8)",
    /* MSG_REQ_LOAD_BAS */
    "Load a BASIC program from text (.bas, UTF-8)",
    /* MSG_ERROR_READ_BAS */
    "Cannot read the file\n%s",
    /* MSG_ERROR_BASIC_MEMORY */
    "The BASIC memory pointers are not usable.%s\nReset the machine first.",
    /* MSG_BAS_NOT_LOADED */
    "The program was not loaded, %ld error(s):",
    /* MSG_BAS_WARNINGS */
    "%ld lines loaded, with %ld warning(s):",
    /* MSG_BAS_MORE */
    "... and %ld more, see the log.",
    /* MSG_BAS_LOADED */
    "%ld BASIC lines loaded.",
    /* MSG_BAS_AT_LINE */
    "Line %ld (BASIC %ld): ",
    /* MSG_BAS_AT_FILE_LINE */
    "Line %ld: ",
    /* MSG_BAS_ERR_UTF8 */
    "invalid UTF-8 text",
    /* MSG_BAS_ERR_CHAR */
    "character U+%04lX has no PETSCII equivalent",
    /* MSG_BAS_ERR_CONTROL */
    "unknown control code {%s}",
    /* MSG_BAS_ERR_BRACE */
    "{ without }",
    /* MSG_BAS_ERR_NUMBER */
    "line number %lu is above 63999",
    /* MSG_BAS_ERR_TOO_LONG */
    "line too long once tokenized (%lu bytes, 250 max)",
    /* MSG_BAS_ERR_MEMORY */
    "program too big: %lu bytes, %lu free",
    /* MSG_BAS_ERR_NO_LINES */
    "no numbered BASIC line in the file",
    /* MSG_BAS_WARN_DUPLICATE */
    "line %lu given again, the last one is kept",
    /* MSG_BAS_WARN_EMPTY */
    "line %lu has no text, ignored",

    /* MSG_MENU_TAPE */
    "Tape",
    /* MSG_ATTACH_TAPE */
    "Attach tape image...",
    /* MSG_DETACH_TAPE */
    "Detach tape image",
    /* MSG_CREATE_TAPE */
    "Create new .tap image...",
    /* MSG_TAPE_PLAY */
    "Play",
    /* MSG_TAPE_STOP */
    "Stop",
    /* MSG_TAPE_REWIND */
    "Rewind",
    /* MSG_TAPE_FORWARD */
    "Fast forward",
    /* MSG_TAPE_RECORD */
    "Record",
    /* MSG_TAPE_RESET */
    "Reset datasette",
    /* MSG_REQ_ATTACH_TAPE */
    "Attach a tape image (.t64, .tap)",
    /* MSG_REQ_CREATE_TAPE */
    "Create a new tape image (.tap)",
    /* MSG_ERROR_ATTACH_TAPE: %s is the file */
    "Cannot attach the tape image\n%s",
    /* MSG_ERROR_CREATE_TAPE: %s is the file */
    "Cannot create the tape image\n%s"
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
