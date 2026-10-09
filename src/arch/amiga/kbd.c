/** \file   kbd.c
 * \brief   Headless UI keyboard stuff
 *
 * \author  Marco van den Heuvel <blackystardust68@yahoo.com>
 * \author  Michael C. Martin <mcmartin@gmail.com>
 * \author  Oliver Schaertel
 * \author  pottendo <pottendo@gmx.net>
 * \author  Bas Wassink <b.wassink@ziggo.nl>
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include "vice.h"

#include <stdio.h>
#include <stdlib.h>
#include "lib.h"
#include "log.h"
#include "ui.h"

/* UNIX-specific; for kbd_arch_get_host_mapping */
#include <locale.h>
#include <string.h>


#include "keyboard.h"
#include "keymap.h"
#include "kbd.h"
#include "amigamachine.h"
#include "resources.h"

#include <exec/types.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/keymap.h>


int kbd_arch_get_host_mapping(void)
{
    /* printf("%s\n", __func__); */

    return KBD_MAPPING_US;
}


/** \brief  Initialize keyboard handling
 */
/* ------------------------------------------------------------------------- */
/* symbolic keyboard mapping */

/* keysyms of the characters: above the rawkeys (0-127) of the keymap file */
#define SYMBOLIC_KEYSYM(c) (0x100 + (signed long)(unsigned char)(c))

static int keyboard_symbolic = 1;
/* characters the C64 has (entries added to the keymap) */
static unsigned char symbolic_known[256];
/* keysym pressed by each rawkey in symbolic mode, 0: none. The release
 * uses it: the qualifiers may have changed in between. */
static signed long symbolic_down[128];

static void symbolic_add(unsigned char c, int row, int col, int shifted)
{
    /* a shifted character gets the C64 SHIFT, an unshifted one loses the
       Amiga shift (AZERTY digits are shifted) */
    keyboard_set_map_any(SYMBOLIC_KEYSYM(c), row, col,
                         shifted ? VIRTUAL_SHIFT : DESHIFT_SHIFT);
    symbolic_known[c] = 1;
}

const char *kbd_arch_builtin_keymap(void)
{
    return amiga_machine.builtin_keymap;
}

void kbd_arch_keymap_loaded(void)
{
    unsigned int i;

    const unsigned char (*letters)[2] = amiga_machine.letters;
    const amiga_machine_symkey_t *keys = amiga_machine.symkeys;

    memset(symbolic_known, 0, sizeof symbolic_known);
    for (i = 0; i < 26; i++) {
        symbolic_add((unsigned char)('a' + i), letters[i][0], letters[i][1], 0);
        symbolic_add((unsigned char)('A' + i), letters[i][0], letters[i][1], 1);
    }
    for (i = 0; i < (unsigned int)amiga_machine.symkey_count; i++) {
        symbolic_add(keys[i].c, keys[i].row, keys[i].col, keys[i].shifted);
    }
    /* keypad Enter: RETURN, with the Amiga shift if held (Shift+RETURN) */
    keyboard_set_map_any(SYMBOLIC_KEYSYM('\r'), amiga_machine.return_row,
                         amiga_machine.return_col, ALLOW_SHIFT);
    symbolic_known['\r'] = 1;
}

/* rawkeys that give a character in symbolic mode: the main block without
 * Clr/Home (0x0d), and the numeric keypad (digits, . + - * / ( ), Enter).
 * The other keys from 0x40 (space, Return, Del, cursors, function keys,
 * modifiers...) go through the keymap file. */
static int rawkey_is_symbolic(unsigned int key)
{
    switch (key) {
        case 0x0d:
            return 0;
        /* keypad */
        case 0x0f:
        case 0x1d: case 0x1e: case 0x1f:
        case 0x2d: case 0x2e: case 0x2f:
        case 0x3c: case 0x3d: case 0x3e: case 0x3f:
        case 0x43:  /* Enter */
        case 0x4a:  /* - */
        case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e:
            return 1;
        default:
            return key < 0x40;
    }
}

/* the character of a rawkey with the Amiga keymap, -1 if none. Only shift,
 * alt and keypad are kept: Ctrl, Amiga and Caps Lock are C64 keys or
 * nothing. */
static int rawkey_char(unsigned int key, unsigned int qualifier)
{
    struct InputEvent ie;
    char buf[8];
    LONG n;

    if (KeymapBase == NULL) {
        return -1;
    }
    memset(&ie, 0, sizeof ie);
    ie.ie_Class = IECLASS_RAWKEY;
    ie.ie_Code = (UWORD)key;
    ie.ie_Qualifier = (UWORD)(qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT
                                           | IEQUALIFIER_LALT | IEQUALIFIER_RALT
                                           | IEQUALIFIER_NUMERICPAD));
    n = MapRawKey(&ie, (STRPTR)buf, (LONG)sizeof buf, NULL);
    if (n != 1) {
        /* dead key, string or nothing */
        return -1;
    }
    return (unsigned char)buf[0];
}

int amiga_kbd_rawkey_shortcut_char(unsigned int code)
{
    int c = rawkey_char(code & 0x7f, 0);

    if (c >= 'A' && c <= 'Z') {
        c += 'a' - 'A';
    }
    return c;
}

void amiga_kbd_rawkey(unsigned int code, unsigned int qualifier, int mods)
{
    unsigned int key = code & 0x7f;
    int c;

    if (!keyboard_symbolic || !rawkey_is_symbolic(key)) {
        if (code & IECODE_UP_PREFIX) {
            keyboard_key_released((signed long)key, mods);
        } else {
            keyboard_key_pressed((signed long)key, mods);
        }
        return;
    }
    if (code & IECODE_UP_PREFIX) {
        if (symbolic_down[key] != 0) {
            keyboard_key_released(symbolic_down[key], mods);
            symbolic_down[key] = 0;
        }
        return;
    }
    c = rawkey_char(key, qualifier);
    if (c < 0 || !symbolic_known[c]) {
        /* not a C64 character */
        return;
    }
    if (symbolic_down[key] != 0 && symbolic_down[key] != SYMBOLIC_KEYSYM(c)) {
        keyboard_key_released(symbolic_down[key], mods);
    }
    symbolic_down[key] = SYMBOLIC_KEYSYM(c);
    keyboard_key_pressed(symbolic_down[key], mods);
}

void amiga_kbd_clear(void)
{
    memset(symbolic_down, 0, sizeof symbolic_down);
    keyboard_key_clear();
}

int amiga_kbd_get_symbolic(void)
{
    return keyboard_symbolic;
}

void amiga_kbd_set_symbolic(int on)
{
    resources_set_int("AmigaKeyboardSymbolic", on ? 1 : 0);
}

static int set_keyboard_symbolic(int val, void *param)
{
    val = val ? 1 : 0;
    if (val != keyboard_symbolic) {
        keyboard_symbolic = val;
        /* a key pressed in one mode is released in the other one */
        amiga_kbd_clear();
    }
    return 0;
}

static const resource_int_t kbd_resources_int[] = {
    { "AmigaKeyboardSymbolic", 1, RES_EVENT_NO, NULL,
      &keyboard_symbolic, set_keyboard_symbolic, NULL },
    RESOURCE_INT_LIST_END
};

int amiga_kbd_resources_init(void)
{
    return resources_register_int(kbd_resources_int);
}

/* ------------------------------------------------------------------------- */

void kbd_arch_init(void)
{
    /* printf("%s\n", __func__); */

    /* the symbolic mode characters (amigakeys.c also uses it) */
    if (KeymapBase == NULL) {
        KeymapBase = OpenLibrary((CONST_STRPTR)"keymap.library", 37);
    }

    /* do NOT call kbd_hotkey_init(), keyboard.c calls this function *after*
     * the UI init stuff is called, allocating the hotkeys array again and thus
     * causing a memory leak
     */
}

void kbd_arch_shutdown(void)
{
    /* printf("%s\n", __func__); */

    if (KeymapBase != NULL) {
        CloseLibrary(KeymapBase);
        KeymapBase = NULL;
    }

    /* Also don't call kbd_hotkey_shutdown() here */
}

/* keysyms in the amiga_*.vkm files are decimal Amiga rawkey codes */
signed long kbd_arch_keyname_to_keynum(char *keyname)
{
    char *end;
    long keynum = strtol(keyname, &end, 10);

    if (end == keyname || keynum < 0 || keynum > 0x7f) {
        return -1;
    }
    return keynum;
}

const char *kbd_arch_keynum_to_keyname(signed long keynum)
{
    /* printf("%s\n", __func__); */

    static char keyname[20];

    memset(keyname, 0, 20);

    sprintf(keyname, "%li", keynum);

    return keyname;
}

void kbd_initialize_numpad_joykeys(int *joykeys)
{
    /* printf("%s\n", __func__); */
}

/** \brief  Initialize the hotkeys
 *
 * This allocates an initial hotkeys array of HOTKEYS_SIZE_INIT elements
 */
void kbd_hotkey_init(void)
{
    /* printf("%s\n", __func__); */
}

/** \brief  Clean up memory used by the hotkeys array
 */
void kbd_hotkey_shutdown(void)
{
    /* printf("%s\n", __func__); */
}
