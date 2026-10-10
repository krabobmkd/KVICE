/** \file   amigamachine.h
 * \brief   What the Amiga UI needs to know of the emulated machine
 *
 * KVICE has three emulators: x64, xplus4 and xvic. The shared Amiga UI
 * (settings window, ROM report, keyboard, menus) only uses this table,
 * defined by the machine's own UI file (c64ui.c, plus4ui.c, vic20ui.c),
 * which is linked in that emulator only.
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

#ifndef VICE_AMIGAMACHINE_H
#define VICE_AMIGAMACHINE_H

#include <stddef.h>
#include <exec/types.h>

/* AmigaOS "Version" command tag of each emulator, KVICE_VERSTAG("x64"):
   "$VER: x64 1.0 fork3.10 68030". The CPU is the one the build targets
   (gcc -m680x0 defines __mc680x0__); VERSION comes from version.h. */
#define KVICE_VERSION "1.0"
#if defined(__mc68060__)
#define KVICE_CPU "68060"
#elif defined(__mc68040__)
#define KVICE_CPU "68040"
#elif defined(__mc68030__)
#define KVICE_CPU "68030"
#else
#define KVICE_CPU "68020"
#endif
#define KVICE_VERSTAG(name) "$VER: " name " " KVICE_VERSION " fork" VERSION " " KVICE_CPU

/* most ROM file rows of a machine (C64: Kernal, BASIC, character ROM) */
#define AMIGA_MACHINE_ROMS_MAX 3
/* most models of a machine */
#define AMIGA_MACHINE_MODELS_MAX 8

/* a ROM file row of the Machine page */
typedef struct amiga_machine_rom_s {
    const char *resource;   /* file name resource ("KernalName"...) */
    ULONG label_msg;        /* MSG_* of the row label */
    int size;               /* exact file size */
} amiga_machine_rom_t;

/* an entry of the model cycle */
typedef struct amiga_machine_model_s {
    ULONG name_msg;         /* MSG_* of its name, or 0: name */
    const char *name;
    int value;              /* the machine's model number */
} amiga_machine_model_t;

/* a cycle row of the Machine page under the model (C64 REU, VIC-20 RAM
 * expansion): entries as the models, its own getter and setter */
typedef struct amiga_machine_cycle_s {
    ULONG label_msg;
    const amiga_machine_model_t *entries;
    int count;
    int (*get)(void);
    void (*set)(int value);
} amiga_machine_cycle_t;

/* a character of the symbolic keyboard mode and the keys that type it */
typedef struct amiga_machine_symkey_s {
    unsigned char c;        /* Latin-1 character of the Amiga keymap */
    unsigned char row;      /* keyboard matrix */
    unsigned char col;
    unsigned char shifted;  /* with the machine's SHIFT */
} amiga_machine_symkey_t;

typedef struct amiga_machine_s {
    const char *title;          /* requester, MUI and about title */
    const char *window_title;   /* emulator window and fullscreen screen title */
    const char *menu_title;     /* first menu */
    const char *data_drawer;    /* machine drawer: "C64", "PLUS4"... */
    /* shown width of an emulated pixel, in screen pixels: the scalers
       widen the draw buffer by this (2 for the VIC-20, 1 otherwise) */
    int pixel_width;

    /* Machine page: ROM file rows, in the order of the startup report */
    const amiga_machine_rom_t *roms;
    int rom_count;
    /* each ROM loaded by the last mem_load(), ok[] of rom_count */
    void (*roms_loaded)(int *ok);

    /* model cycle: its label, its entries */
    ULONG model_label_msg;
    const amiga_machine_model_t *models;
    int model_count;
    /* the model the settings are, -1 if none */
    int (*model_get)(void);
    /* the model settings, but not the ROM files (the button does) */
    void (*model_set)(int model);
    /* the line under the cycle: what the model is */
    void (*model_info)(int model, char *out, size_t size);
    /* "Set ROM defaults for this model": file names, NULL to keep */
    void (*model_rom_names)(int model, const char **names);

    /* a memory cycle under the model (C64 REU, VIC-20 RAM), NULL: none */
    const amiga_machine_cycle_t *extra;

    /* BASIC program in memory (Save BASIC): the end must be below this */
    unsigned int basic_top;
    /* its tokens (Save program as .bas): AMIGA_BASIC_V2, AMIGA_BASIC_V35 */
    int basic_dialect;

    /* symbolic keyboard mode: the characters that are not letters, and the
       matrix of the letters A to Z */
    const amiga_machine_symkey_t *symkeys;
    int symkey_count;
    const unsigned char (*letters)[2];
    /* matrix of RETURN (keypad Enter in symbolic mode) */
    unsigned char return_row, return_col;

    /* amiga_positional.vkm of the machine without its comments, used when
       the file is not found */
    const char *builtin_keymap;

    /* the machine menu has the Tape submenu (datasette) */
    int has_tape;
} amiga_machine_t;

/* defined by the machine UI file of each emulator */
extern const amiga_machine_t amiga_machine;

#endif
