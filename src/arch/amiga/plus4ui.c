/** \file   plus4ui.c
 * \brief   Headless PLUS4 UI
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

#include "plus4ui.h"
#include "amigalocale.h"
#include "amigabasic.h"
#include "amigamachine.h"
#include "machine.h"
#include "plus4mem.h"
#include "plus4model.h"
#include "plus4rom.h"
#include "resources.h"

/* ------------------------------------------------------------------------- */
/* the Plus/4, C16 and C116 for the Amiga UI (amigamachine.h) */

/* the 3plus1 function ROMs come with the model */
static const amiga_machine_rom_t plus4_roms[] = {
    { "KernalName", MSG_ROM_KERNAL, PLUS4_KERNAL_ROM_SIZE },
    { "BasicName",  MSG_ROM_BASIC,  PLUS4_BASIC_ROM_SIZE }
};

static void plus4_roms_loaded(int *ok)
{
    plus4rom_get_loaded(&ok[0], &ok[1]);
}

static const amiga_machine_model_t plus4_models[] = {
    { 0, "C16/C116 PAL",  PLUS4MODEL_C16_PAL },
    { 0, "C16/C116 NTSC", PLUS4MODEL_C16_NTSC },
    { 0, "Plus/4 PAL",    PLUS4MODEL_PLUS4_PAL },
    { 0, "Plus/4 NTSC",   PLUS4MODEL_PLUS4_NTSC },
    { 0, "V364 NTSC",     PLUS4MODEL_V364_NTSC },
    { 0, "C232 NTSC",     PLUS4MODEL_232_NTSC }
};

/* what tells the models apart, the ROM files left out (plus4model.c
   compares the Kernal file name, which may be a path here) */
static const struct {
    int video;
    int ramsize;
    int speech;
} plus4_model_keys[] = {
    { MACHINE_SYNC_PAL,  16, 0 },   /* PLUS4MODEL_C16_PAL */
    { MACHINE_SYNC_NTSC, 16, 0 },   /* PLUS4MODEL_C16_NTSC */
    { MACHINE_SYNC_PAL,  64, 0 },   /* PLUS4MODEL_PLUS4_PAL */
    { MACHINE_SYNC_NTSC, 64, 0 },   /* PLUS4MODEL_PLUS4_NTSC */
    { MACHINE_SYNC_NTSC, 64, 1 },   /* PLUS4MODEL_V364_NTSC */
    { MACHINE_SYNC_NTSC, 32, 0 }    /* PLUS4MODEL_232_NTSC */
};

static int plus4_model_get(void)
{
    int video, ramsize, speech, i;

    if (resources_get_int("MachineVideoStandard", &video) < 0
            || resources_get_int("RamSize", &ramsize) < 0
            || resources_get_int("SpeechEnabled", &speech) < 0) {
        return -1;
    }
    for (i = 0; i < PLUS4MODEL_NUM; i++) {
        if (plus4_model_keys[i].video == video
                && plus4_model_keys[i].ramsize == ramsize
                && plus4_model_keys[i].speech == (speech != 0)) {
            return i;
        }
    }
    return -1;
}

/* plus4model_set() also sets the Kernal and BASIC files: the ones of the
   ROM rows are kept (the "Set ROM defaults" button changes them) */
static void plus4_model_set(int model)
{
    const char *name = NULL;
    char kernal[256], basic[256];

    kernal[0] = basic[0] = '\0';
    if (resources_get_string("KernalName", &name) == 0 && name != NULL) {
        strncpy(kernal, name, sizeof kernal - 1);
        kernal[sizeof kernal - 1] = '\0';
    }
    if (resources_get_string("BasicName", &name) == 0 && name != NULL) {
        strncpy(basic, name, sizeof basic - 1);
        basic[sizeof basic - 1] = '\0';
    }
    plus4model_set(model);
    if (kernal[0] != '\0') {
        resources_set_string("KernalName", kernal);
    }
    if (basic[0] != '\0') {
        resources_set_string("BasicName", basic);
    }
}

static void plus4_model_info(int model, char *out, size_t size)
{
    if (model < 0 || model >= PLUS4MODEL_NUM) {
        out[0] = '\0';
        return;
    }
    snprintf(out, size, LOC(MSG_MODEL_INFO_RAM),
             plus4_model_keys[model].video == MACHINE_SYNC_PAL ? "PAL 50 Hz" : "NTSC 60 Hz",
             plus4_model_keys[model].ramsize);
}

static void plus4_model_rom_names(int model, const char **names)
{
    switch (model) {
        case PLUS4MODEL_C16_PAL:
        case PLUS4MODEL_PLUS4_PAL:
            names[0] = PLUS4_KERNAL_PAL_REV5_NAME;
            break;
        case PLUS4MODEL_V364_NTSC:
            names[0] = PLUS4_KERNAL_NTSC_364_NAME;
            break;
        case PLUS4MODEL_232_NTSC:
            names[0] = PLUS4_KERNAL_NTSC_REV1_NAME;
            break;
        default:
            names[0] = PLUS4_KERNAL_NTSC_REV5_NAME;
            break;
    }
    names[1] = PLUS4_BASIC_NAME;
}

/* the keys of each character (TED keyboard matrix as in
   PLUS4/amiga_positional.vkm). Up arrow is SHIFT+0, left arrow SHIFT+=. */
static const amiga_machine_symkey_t plus4_symkeys[] = {
    { '1', 7, 0, 0 }, { '2', 7, 3, 0 }, { '3', 1, 0, 0 }, { '4', 1, 3, 0 },
    { '5', 2, 0, 0 }, { '6', 2, 3, 0 }, { '7', 3, 0, 0 }, { '8', 3, 3, 0 },
    { '9', 4, 0, 0 }, { '0', 4, 3, 0 },
    { '!', 7, 0, 1 }, { '"', 7, 3, 1 }, { '#', 1, 0, 1 }, { '$', 1, 3, 1 },
    { '%', 2, 0, 1 }, { '&', 2, 3, 1 }, { '\'', 3, 0, 1 }, { '(', 3, 3, 1 },
    { ')', 4, 0, 1 },
    { '+', 6, 6, 0 }, { '-', 5, 6, 0 }, { 0xa3, 0, 2, 0 },  /* pound */
    { '@', 0, 7, 0 }, { '*', 6, 1, 0 }, { '^', 4, 3, 1 },   /* up arrow */
    { ':', 5, 5, 0 }, { ';', 6, 2, 0 }, { '=', 6, 5, 0 },
    { ',', 5, 7, 0 }, { '.', 5, 4, 0 }, { '/', 6, 7, 0 },
    { '_', 6, 5, 1 },                                       /* left arrow */
    { '[', 5, 5, 1 }, { ']', 6, 2, 1 },
    { '<', 5, 7, 1 }, { '>', 5, 4, 1 }, { '?', 6, 7, 1 }
};

/* the letters A to Z are where they are on the C64 */
static const unsigned char plus4_letters[26][2] = {
    { 1, 2 }, { 3, 4 }, { 2, 4 }, { 2, 2 }, { 1, 6 }, { 2, 5 }, { 3, 2 },
    { 3, 5 }, { 4, 1 }, { 4, 2 }, { 4, 5 }, { 5, 2 }, { 4, 4 }, { 4, 7 },
    { 4, 6 }, { 5, 1 }, { 7, 6 }, { 2, 1 }, { 1, 5 }, { 2, 6 }, { 3, 6 },
    { 3, 7 }, { 1, 1 }, { 2, 7 }, { 3, 1 }, { 1, 4 }
};

/* made by amiga/CMakeLists.txt from data/PLUS4/amiga_positional.vkm */
#include "amiga_positional_vkm_plus4.h"

const amiga_machine_t amiga_machine = {
    "KVICE xplus4",
    "xplus4",
    "Plus/4",
    "PLUS4",
    1,          /* pixel width */
    plus4_roms, sizeof plus4_roms / sizeof plus4_roms[0],
    plus4_roms_loaded,
    MSG_MODEL,
    plus4_models, sizeof plus4_models / sizeof plus4_models[0],
    plus4_model_get,
    plus4_model_set,
    plus4_model_info,
    plus4_model_rom_names,
    NULL,       /* no memory cycle */
    0xfd00,     /* BASIC RAM end (64 KB) */
    AMIGA_BASIC_V35,
    plus4_symkeys, sizeof plus4_symkeys / sizeof plus4_symkeys[0],
    plus4_letters,
    0, 1,       /* RETURN */
    amiga_positional_vkm
};

/* ------------------------------------------------------------------------- */


/** \brief  Pre-initialize the UI before the canvas window gets created
 *
 * \return  0 on success, -1 on failure
 */
int plus4ui_init_early(void)
{
    /* printf("%s\n", __func__); */

    return 0;
}


/** \brief  Initialize the UI
 *
 * \return  0 on success, -1 on failure
 */
int plus4ui_init(void)
{
    /* printf("%s\n", __func__); */

    return 0;
}


/** \brief  Shut down the UI
 */
void plus4ui_shutdown(void)
{
    /* printf("%s\n", __func__); */

    /* NOP */
}
