/** \file   vic20ui.c
 * \brief   Headless VIC20 UI
 *
 * \author  Marco van den Heuvel <blackystardust68@yahoo.com>
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
#include <string.h>

#include "vic20ui.h"
#include "amigalocale.h"
#include "amigabasic.h"
#include "amigamachine.h"
#include "version.h"
#include "machine.h"
#include "resources.h"
#include "vic20model.h"
#include "vic20rom.h"

/* ------------------------------------------------------------------------- */
/* the VIC-20 for the Amiga UI (amigamachine.h) */

static const amiga_machine_rom_t vic20_roms[] = {
    { "KernalName",  MSG_ROM_KERNAL,  VIC20_KERNAL_ROM_SIZE },
    { "BasicName",   MSG_ROM_BASIC,   VIC20_BASIC_ROM_SIZE },
    { "ChargenName", MSG_ROM_CHARGEN, VIC20_CHARGEN_ROM_SIZE }
};

static void vic20_roms_loaded(int *ok)
{
    vic20rom_get_loaded(&ok[0], &ok[1], &ok[2]);
}

/* the SuperVIC (VIC21) is an NTSC VIC-20 with 16 KB: RAM expansion cycle */
static const amiga_machine_model_t vic20_models[] = {
    { 0, "VIC-20 PAL",              VIC20MODEL_VIC20_PAL },
    { 0, "VIC-20 NTSC",             VIC20MODEL_VIC20_NTSC },
    { 0, "VIC-1001 (Japan, NTSC)",  VIC20MODEL_VIC1001 }
};

/* 1 when the character ROM is (a path to) the Japanese one */
static int chargen_is_jap(void)
{
    const char *name = NULL;
    size_t n, len = strlen(VIC20_CHARGEN_JAP_NAME);

    if (resources_get_string("ChargenName", &name) < 0 || name == NULL) {
        return 0;
    }
    n = strlen(name);
    return n >= len && strcmp(name + n - len, VIC20_CHARGEN_JAP_NAME) == 0;
}

/* video standard, and the VIC-1001 by its character ROM (vic20model.c
   compares the ROM and RAM settings, which are chosen apart here) */
static int vic20_model_get(void)
{
    int video;

    if (resources_get_int("MachineVideoStandard", &video) < 0) {
        return -1;
    }
    if (video == MACHINE_SYNC_PAL) {
        return VIC20MODEL_VIC20_PAL;
    }
    return chargen_is_jap() ? VIC20MODEL_VIC1001 : VIC20MODEL_VIC20_NTSC;
}

/* the video standard; the VIC-1001 is its ROMs, so they are set with it,
   and leaving it gives back the standard ones */
static void vic20_model_set(int model)
{
    int jap = chargen_is_jap();

    resources_set_int("MachineVideoStandard",
                      model == VIC20MODEL_VIC20_PAL ? MACHINE_SYNC_PAL : MACHINE_SYNC_NTSC);
    if (model == VIC20MODEL_VIC1001 && !jap) {
        resources_set_string("ChargenName", VIC20_CHARGEN_JAP_NAME);
        resources_set_string("KernalName", VIC20_KERNAL_REV2_NAME);
    } else if (model != VIC20MODEL_VIC1001 && jap) {
        resources_set_string("ChargenName", VIC20_CHARGEN_NAME);
        resources_set_string("KernalName", model == VIC20MODEL_VIC20_PAL
                             ? VIC20_KERNAL_REV7_NAME : VIC20_KERNAL_REV6_NAME);
    }
}

static void vic20_model_info(int model, char *out, size_t size)
{
    /* 5 KB, 3.5 KB of them for BASIC; more with the RAM expansion */
    snprintf(out, size, LOC(MSG_MODEL_INFO_RAM),
             model == VIC20MODEL_VIC20_PAL ? "PAL 50 Hz" : "NTSC 60 Hz", 5);
}

static void vic20_model_rom_names(int model, const char **names)
{
    switch (model) {
        case VIC20MODEL_VIC20_PAL:
            names[0] = VIC20_KERNAL_REV7_NAME;
            break;
        case VIC20MODEL_VIC1001:
            names[0] = VIC20_KERNAL_REV2_NAME;
            break;
        default:
            names[0] = VIC20_KERNAL_REV6_NAME;
            break;
    }
    names[1] = VIC20_BASIC_NAME;
    names[2] = model == VIC20MODEL_VIC1001 ? VIC20_CHARGEN_JAP_NAME : VIC20_CHARGEN_NAME;
}

/* RAM expansion: the RAM blocks as a bit mask, bit n for block n
   (0: $0400-$0FFF 3 KB, 1-3: $2000-$7FFF 8 KB each, 5: $A000-$BFFF) */
static const int ram_blocks[] = { 0, 1, 2, 3, 5 };

static int vic20_ram_get(void)
{
    unsigned int i;
    int mask = 0;

    for (i = 0; i < sizeof ram_blocks / sizeof ram_blocks[0]; i++) {
        int on = 0;

        resources_get_int_sprintf("RamBlock%d", &on, ram_blocks[i]);
        if (on) {
            mask |= 1 << ram_blocks[i];
        }
    }
    return mask;
}

static void vic20_ram_set(int mask)
{
    unsigned int i;

    for (i = 0; i < sizeof ram_blocks / sizeof ram_blocks[0]; i++) {
        resources_set_int_sprintf("RamBlock%d", (mask >> ram_blocks[i]) & 1, ram_blocks[i]);
    }
}

static const amiga_machine_model_t vic20_ram_entries[] = {
    { MSG_REU_OFF, NULL,                  0 },
    { 0,           "3 KB",                0x01 },
    { 0,           "8 KB",                0x02 },
    { 0,           "16 KB",               0x06 },
    { 0,           "24 KB",               0x0e },
    { 0,           "35 KB (all blocks)",  0x2f }
};

static const amiga_machine_cycle_t vic20_ram = {
    MSG_RAM_EXPANSION,
    vic20_ram_entries, sizeof vic20_ram_entries / sizeof vic20_ram_entries[0],
    vic20_ram_get,
    vic20_ram_set
};

/* the keys of each character: the C64 table of c64ui.c with the VIC-20
   matrix, which is the C64 one with rows 0 and 7 and columns 3 and 7
   swapped (as in VIC20/amiga_positional.vkm) */
static const amiga_machine_symkey_t vic20_symkeys[] = {
    { '1', 0, 0, 0 }, { '2', 0, 7, 0 }, { '3', 1, 0, 0 }, { '4', 1, 7, 0 },
    { '5', 2, 0, 0 }, { '6', 2, 7, 0 }, { '7', 3, 0, 0 }, { '8', 3, 7, 0 },
    { '9', 4, 0, 0 }, { '0', 4, 7, 0 },
    { '!', 0, 0, 1 }, { '"', 0, 7, 1 }, { '#', 1, 0, 1 }, { '$', 1, 7, 1 },
    { '%', 2, 0, 1 }, { '&', 2, 7, 1 }, { '\'', 3, 0, 1 }, { '(', 3, 7, 1 },
    { ')', 4, 0, 1 },
    { '+', 5, 0, 0 }, { '-', 5, 7, 0 }, { 0xa3, 6, 0, 0 },  /* pound */
    { '@', 5, 6, 0 }, { '*', 6, 1, 0 }, { '^', 6, 6, 0 },   /* up arrow */
    { ':', 5, 5, 0 }, { ';', 6, 2, 0 }, { '=', 6, 5, 0 },
    { ',', 5, 3, 0 }, { '.', 5, 4, 0 }, { '/', 6, 3, 0 },
    { '_', 0, 1, 0 },                                       /* left arrow */
    { '[', 5, 5, 1 }, { ']', 6, 2, 1 },
    { '<', 5, 3, 1 }, { '>', 5, 4, 1 }, { '?', 6, 3, 1 }
};

/* VIC-20 matrix of the letters A to Z */
static const unsigned char vic20_letters[26][2] = {
    { 1, 2 }, { 3, 4 }, { 2, 4 }, { 2, 2 }, { 1, 6 }, { 2, 5 }, { 3, 2 },
    { 3, 5 }, { 4, 1 }, { 4, 2 }, { 4, 5 }, { 5, 2 }, { 4, 4 }, { 4, 3 },
    { 4, 6 }, { 5, 1 }, { 0, 6 }, { 2, 1 }, { 1, 5 }, { 2, 6 }, { 3, 6 },
    { 3, 3 }, { 1, 1 }, { 2, 3 }, { 3, 1 }, { 1, 4 }
};

/* made by amiga/CMakeLists.txt from data/VIC20/amiga_positional.vkm */
#include "amiga_positional_vkm_vic20.h"

/* for the AmigaOS Version command (kept by "used": nothing reads it) */
static const char kvice_verstag[] __attribute__((used)) = KVICE_VERSTAG("xvic");

const amiga_machine_t amiga_machine = {
    "KVICE xvic",
    "xvic",
    "VIC-20",
    "VIC20",
    2,          /* pixel width: the VIC pixels are wide, VICE does not double them */
    vic20_roms, sizeof vic20_roms / sizeof vic20_roms[0],
    vic20_roms_loaded,
    MSG_MODEL,
    vic20_models, sizeof vic20_models / sizeof vic20_models[0],
    vic20_model_get,
    vic20_model_set,
    vic20_model_info,
    vic20_model_rom_names,
    &vic20_ram,
    0x8000,     /* BASIC RAM end (with the 24 KB expansion) */
    AMIGA_BASIC_V2,
    vic20_symkeys, sizeof vic20_symkeys / sizeof vic20_symkeys[0],
    vic20_letters,
    7, 1,       /* RETURN */
    amiga_positional_vkm,
    1           /* Tape menu */
};

/* ------------------------------------------------------------------------- */


/** \brief  Pre-initialize the UI before the canvas window gets created
 *
 * \return  0 on success, -1 on failure
 */
int vic20ui_init_early(void)
{
    /* printf("%s\n", __func__); */

    return 0;
}


/** \brief  Initialize the UI
 *
 * \return  0 on success, -1 on failure
 */
int vic20ui_init(void)
{
    /* printf("%s\n", __func__); */

    return 0;
}


/** \brief  Shut down the UI
 */
void vic20ui_shutdown(void)
{
    /* printf("%s\n", __func__); */

    /* NOP */
}
