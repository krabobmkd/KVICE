/** \file   amigakeys.c
 * \brief   AmigaOS 3.x port: C64 key -> Amiga key table of the keymap in use
 *
 * For the Keyboard page of the settings window: the C64 keys that are not
 * plain letters or digits (RUN/STOP, RESTORE, C=, CTRL, cursor and function
 * keys, symbols...) and the Amiga keys the loaded keymap (.vkm) gives them.
 * The Amiga printable keys are named with keymap.library, so they are the
 * ones printed on the user's keyboard (AZERTY, QWERTZ...).
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

#include <stdio.h>
#include <string.h>

#include <exec/types.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/keymap.h>

#include "amigakeys.h"
#include "keyboard.h"
#include "keymap.h"

/* opened only while the table is built */
struct Library *KeymapBase = NULL;

/* the C64 keys of the table: matrix position, and whether the C64 key is
 * the shifted one (CRSR UP is shifted CRSR DOWN, F2 shifted F1...) */
#define C64KEY_RESTORE    -1
#define C64KEY_SHIFTLOCK  -2

static const struct {
    const char *name;
    int row;
    int col;
    int shifted;
} c64keys[] = {
    { "RUN/STOP",       7, 7, 0 },
    { "RESTORE",        C64KEY_RESTORE, 0, 0 },
    { "C=",             7, 5, 0 },
    { "CTRL",           7, 2, 0 },
    { "SHIFT (left)",   1, 7, 0 },
    { "SHIFT (right)",  6, 4, 0 },
    { "SHIFT LOCK",     C64KEY_SHIFTLOCK, 0, 0 },
    { "CLR/HOME",       6, 3, 0 },
    { "INST/DEL",       0, 0, 0 },
    { "RETURN",         0, 1, 0 },
    { "CRSR DOWN",      0, 7, 0 },
    { "CRSR UP",        0, 7, 1 },
    { "CRSR RIGHT",     0, 2, 0 },
    { "CRSR LEFT",      0, 2, 1 },
    { "F1",             0, 4, 0 },
    { "F2",             0, 4, 1 },
    { "F3",             0, 5, 0 },
    { "F4",             0, 5, 1 },
    { "F5",             0, 6, 0 },
    { "F6",             0, 6, 1 },
    { "F7",             0, 3, 0 },
    { "F8",             0, 3, 1 },
    { "\xa3 (pound)",   6, 0, 0 },
    { "UP ARROW",       6, 6, 0 },
    { "LEFT ARROW",     7, 1, 0 },
    { "@",              5, 6, 0 },
    { "*",              6, 1, 0 },
    { "+",              5, 0, 0 },
    { "-",              5, 3, 0 },
    { "=",              6, 5, 0 },
    { ":",              5, 5, 0 },
    { ";",              6, 2, 0 },
    { "SPACE",          7, 4, 0 }
};

#define C64KEYS_COUNT (int)(sizeof c64keys / sizeof c64keys[0])

/* the Amiga keys that do not print a character */
static const char *rawkey_special_name(int code)
{
    static const char *const keypad[] = {
        /* 0x0f, 0x1d-0x1f, 0x2d-0x2f, 0x3c-0x3f */
        "Num 0", "Num 1", "Num 2", "Num 3", "Num 4", "Num 5", "Num 6",
        "Num .", "Num 7", "Num 8", "Num 9"
    };

    switch (code) {
        case 0x0f: return keypad[0];
        case 0x1d: return keypad[1];
        case 0x1e: return keypad[2];
        case 0x1f: return keypad[3];
        case 0x2d: return keypad[4];
        case 0x2e: return keypad[5];
        case 0x2f: return keypad[6];
        case 0x3c: return keypad[7];
        case 0x3d: return keypad[8];
        case 0x3e: return keypad[9];
        case 0x3f: return keypad[10];
        case 0x40: return "Space";
        case 0x41: return "Backspace";
        case 0x42: return "Tab";
        case 0x43: return "Num Enter";
        case 0x44: return "Return";
        case 0x45: return "Esc";
        case 0x46: return "Del";
        case 0x4a: return "Num -";
        case 0x4c: return "Cursor up";
        case 0x4d: return "Cursor down";
        case 0x4e: return "Cursor right";
        case 0x4f: return "Cursor left";
        case 0x50: return "F1";
        case 0x51: return "F2";
        case 0x52: return "F3";
        case 0x53: return "F4";
        case 0x54: return "F5";
        case 0x55: return "F6";
        case 0x56: return "F7";
        case 0x57: return "F8";
        case 0x58: return "F9";
        case 0x59: return "F10";
        case 0x5a: return "Num (";
        case 0x5b: return "Num )";
        case 0x5c: return "Num /";
        case 0x5d: return "Num *";
        case 0x5e: return "Num +";
        case 0x5f: return "Help";
        case 0x60: return "Shift (left)";
        case 0x61: return "Shift (right)";
        case 0x62: return "Caps Lock";
        case 0x63: return "Ctrl";
        case 0x64: return "Alt (left)";
        case 0x65: return "Alt (right)";
        case 0x66: return "Amiga (left)";
        case 0x67: return "Amiga (right)";
        default: return NULL;
    }
}

/* name of an Amiga raw key: the character it prints with the system keymap,
 * else its name, else its number */
static void rawkey_name(long code, char *out, size_t size)
{
    const char *special = rawkey_special_name((int)code);

    if (special != NULL) {
        snprintf(out, size, "%s", special);
        return;
    }
    if (KeymapBase != NULL && code >= 0 && code < 0x40) {
        struct InputEvent ie;
        char buf[8];
        LONG n;

        memset(&ie, 0, sizeof ie);
        ie.ie_Class = IECLASS_RAWKEY;
        ie.ie_Code = (UWORD)code;
        n = MapRawKey(&ie, (STRPTR)buf, (LONG)sizeof buf - 1, NULL);
        if (n == 1 && (unsigned char)buf[0] > ' ') {
            snprintf(out, size, "%c", buf[0]);
            return;
        }
    }
    snprintf(out, size, "#%ld", code);
}

/* append ", name" (with the host modifiers needed) to \a out */
static void append_key(char *out, size_t size, long code, int flags)
{
    char name[32];
    size_t len = strlen(out);

    rawkey_name(code, name, sizeof name);
    snprintf(out + len, size - len, "%s%s%s%s%s",
             len > 0 ? ", " : "",
             (flags & MAP_MOD_SHIFT) ? "Shift+" : "",
             (flags & MAP_MOD_RIGHT_ALT) ? "Alt+" : "",
             (flags & MAP_MOD_CTRL) ? "Ctrl+" : "",
             name);
}

int amiga_keys_table(amiga_key_row_t *rows, int max)
{
    int i, k, n = 0;
    int opened = 0;

    /* normally open (kbd.c) */
    if (KeymapBase == NULL) {
        KeymapBase = OpenLibrary((CONST_STRPTR)"keymap.library", 37);
        opened = 1;
    }

    for (k = 0; k < C64KEYS_COUNT && n < max; k++) {
        amiga_key_row_t *r = &rows[n++];

        snprintf(r->c64, sizeof r->c64, "%s", c64keys[k].name);
        r->host[0] = '\0';

        if (c64keys[k].row == C64KEY_RESTORE) {
            if (key_ctrl_restore1 >= 0) {
                append_key(r->host, sizeof r->host, key_ctrl_restore1, key_flags_restore1);
            }
            if (key_ctrl_restore2 >= 0) {
                append_key(r->host, sizeof r->host, key_ctrl_restore2, key_flags_restore2);
            }
        } else if (keyconvmap != NULL) {
            for (i = 0; i < keyconvmap_num_keys; i++) {
                const keyboard_conv_t *e = &keyconvmap[i];
                int match;

                /* not a rawkey: a symbolic mode character (kbd.c) */
                if ((e->shift & ALT_MAP) || e->sym > 0x7f) {
                    continue;
                }
                if (c64keys[k].row == C64KEY_SHIFTLOCK) {
                    match = (e->shift & SHIFT_LOCK) != 0;
                } else {
                    match = e->row == c64keys[k].row && e->column == c64keys[k].col
                            && ((e->shift & VIRTUAL_SHIFT) ? 1 : 0) == c64keys[k].shifted
                            && !(e->shift & SHIFT_LOCK);
                }
                if (match) {
                    append_key(r->host, sizeof r->host, e->sym, e->shift);
                }
            }
        }
        if (r->host[0] == '\0') {
            snprintf(r->host, sizeof r->host, "-");
        }
    }

    if (opened && KeymapBase != NULL) {
        CloseLibrary(KeymapBase);
        KeymapBase = NULL;
    }
    return n;
}
