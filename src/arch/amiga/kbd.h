/** \file   kbd.h
 * \brief   Headless specfic keyboard driver - header
 *
 * \author  Marco van den Heuvel <blackystardust68@yahoo.com>
 */

/*
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README file for copyright notice.
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

#ifndef VICE_KBD_H
#define VICE_KBD_H

void kbd_arch_init(void);
void kbd_arch_shutdown(void);
int kbd_arch_get_host_mapping(void);
void kbd_initialize_numpad_joykeys(int *joykeys);

#define KBD_PORT_PREFIX "amiga"

/* add more function prototypes as needed below */

signed long kbd_arch_keyname_to_keynum(char *keyname);
const char *kbd_arch_keynum_to_keyname(signed long keynum);

void kbd_hotkey_init(void);
void kbd_hotkey_shutdown(void);

/* keyboard mapping ("AmigaKeyboardSymbolic"):
 * - positional: every rawkey goes through the keymap file
 *   (amiga_positional.vkm), C64 key positions,
 * - symbolic (default): the special keys (modifiers, Return, Del, cursors,
 *   function keys, Run/Stop, Clr/Home, Restore, space) still go through
 *   the file, the other keys and the numeric keypad give the character of
 *   the Amiga keymap (MapRawKey()) and press the C64 keys of that
 *   character (keypad Enter: RETURN).
 *   Characters the C64 has not (é, à...) do nothing. */
int amiga_kbd_resources_init(void);
int amiga_kbd_get_symbolic(void);
void amiga_kbd_set_symbolic(int on);

/* an IDCMP_RAWKEY message (code with IECODE_UP_PREFIX), mods: KBD_MOD_* */
void amiga_kbd_rawkey(unsigned int code, unsigned int qualifier, int mods);
/* the lower case character of a rawkey with the Amiga keymap (no
   qualifier), whatever the symbolic/positional mode: for the Amiga+key
   shortcuts. -1 if none. */
int amiga_kbd_rawkey_shortcut_char(unsigned int code);
/* all keys up (window inactive, mode change) */
void amiga_kbd_clear(void);

/* keymap.c: a keymap file was loaded, add the symbolic mode entries */
void kbd_arch_keymap_loaded(void);
/* the standard positional keymap built in (amiga_positional.vkm without
   comments), loaded when the file is not on disk */
const char *kbd_arch_builtin_keymap(void);

#endif
