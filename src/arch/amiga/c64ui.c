/** \file   c64ui.c
 * \brief   Headless C64 UI
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

#include "amigatrace.h"

#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "c64ui.h"
#include "amigalocale.h"
#include "amigabasic.h"
#include "amigamachine.h"
#include "version.h"
#include "c64mem.h"
#include "c64model.h"
#include "c64rom.h"
#include "log.h"
#include "machine.h"
#include "resources.h"

/* ------------------------------------------------------------------------- */
/* the C64 for the Amiga UI (amigamachine.h) */

static const amiga_machine_rom_t c64_roms[] = {
    { "KernalName",  MSG_ROM_KERNAL,  C64_KERNAL_ROM_SIZE },
    { "BasicName",   MSG_ROM_BASIC,   C64_BASIC_ROM_SIZE },
    { "ChargenName", MSG_ROM_CHARGEN, C64_CHARGEN_ROM_SIZE }
};

static void c64_roms_loaded(int *ok)
{
    c64rom_get_loaded(&ok[0], &ok[1], &ok[2]);
}

static const amiga_machine_model_t c64_models[] = {
    { MSG_MODEL_C64_PAL,      NULL, C64MODEL_C64_PAL },
    { MSG_MODEL_C64C_PAL,     NULL, C64MODEL_C64C_PAL },
    { MSG_MODEL_C64_OLD_PAL,  NULL, C64MODEL_C64_OLD_PAL },
    { MSG_MODEL_C64_NTSC,     NULL, C64MODEL_C64_NTSC },
    { MSG_MODEL_C64C_NTSC,    NULL, C64MODEL_C64C_NTSC },
    { MSG_MODEL_C64_OLD_NTSC, NULL, C64MODEL_C64_OLD_NTSC },
    { MSG_MODEL_DREAN,        NULL, C64MODEL_C64_PAL_N }
};

/* 1 when the machine settings (video, CIA, SID, board) are the ones of
 * \a model: the ROM files are not compared, they are chosen apart */
static int c64model_matches(int model)
{
    c64model_details_t details;

    memset(&details, 0, sizeof details);
    if (resources_get_int("MachineVideoStandard", &details.vicii_model) < 0
            || resources_get_int("SidModel", &details.sid_model) < 0
            || resources_get_int("CIA1Model", &details.cia1_model) < 0
            || resources_get_int("CIA2Model", &details.cia2_model) < 0
            || resources_get_int("BoardType", &details.board) < 0
            || resources_get_int("IECReset", &details.iecreset) < 0) {
        return 0;
    }
    details.chargen = c64model_get_chargen_name(model);
    details.kernalrev = c64model_get_kernal_rev(model);
    return c64model_get_model(&details) == model;
}

/* the model chosen last ("AmigaC64Model", C64 and C64 old only differ by
 * their kernal), else the first one the machine settings match */
static int c64_model_get(void)
{
    int stored = -1;
    unsigned int i;

    resources_get_int("AmigaC64Model", &stored);
    if (stored >= 0 && c64model_matches(stored)) {
        return stored;
    }
    for (i = 0; i < sizeof c64_models / sizeof c64_models[0]; i++) {
        if (c64model_matches(c64_models[i].value)) {
            return c64_models[i].value;
        }
    }
    return -1;
}

static void c64_model_set(int model)
{
    c64model_set(model);
    resources_set_int("AmigaC64Model", model);
}

static void c64_model_info(int model, char *out, size_t size)
{
    const char *video;

    switch (c64model_get_video(model)) {
        case MACHINE_SYNC_PAL:     video = "PAL 50 Hz"; break;
        case MACHINE_SYNC_PALN:    video = "PAL-N 50 Hz"; break;
        case MACHINE_SYNC_NTSC:    video = "NTSC 60 Hz"; break;
        case MACHINE_SYNC_NTSCOLD: video = "old NTSC 60 Hz"; break;
        default:                   video = "?"; break;
    }
    snprintf(out, size, LOC(MSG_MODEL_INFO), video,
             c64model_get_new_sid(model) > 0 ? "MOS 8580" : "MOS 6581");
}

/* kernal file name of a kernal revision (the table of c64-resources.c) */
static const char *kernal_rev_name(int rev)
{
    switch (rev) {
        case C64_KERNAL_JAP:  return C64_KERNAL_JAP_NAME;
        case C64_KERNAL_REV1: return C64_KERNAL_REV1_NAME;
        case C64_KERNAL_REV2: return C64_KERNAL_REV2_NAME;
        case C64_KERNAL_REV3: return C64_KERNAL_REV3_NAME;
        case C64_KERNAL_GS64: return C64_KERNAL_GS64_NAME;
        case C64_KERNAL_SX64: return C64_KERNAL_SX64_NAME;
        case C64_KERNAL_4064: return C64_KERNAL_4064_NAME;
        default:              return NULL;
    }
}

static void c64_model_rom_names(int model, const char **names)
{
    names[0] = kernal_rev_name(c64model_get_kernal_rev(model));
    names[1] = C64_BASIC_NAME;
    names[2] = c64model_get_chargen_name(model);
}

/* REU: one cycle, 0 is off, else the size in KB */
static int get_reu(void)
{
    int on = 0, size = 0;

    resources_get_int("REU", &on);
    resources_get_int("REUsize", &size);
    return on ? size : 0;
}

static void set_reu(int size_kb)
{
    /* off while the size changes: set again on, the memory is reallocated */
    resources_set_int("REU", 0);
    if (size_kb > 0) {
        if (resources_set_int("REUsize", size_kb) < 0
                || resources_set_int("REU", 1) < 0) {
            log_error(LOG_DEFAULT, "settings: cannot enable a %d KB REU.", size_kb);
        }
    }
}

static const amiga_machine_model_t c64_reu_entries[] = {
    { MSG_REU_OFF, NULL,            0 },
    { 0,           "128 KB (1700)", 128 },
    { 0,           "256 KB (1764)", 256 },
    { 0,           "512 KB (1750)", 512 }
};

static const amiga_machine_cycle_t c64_reu = {
    MSG_REU,
    c64_reu_entries, sizeof c64_reu_entries / sizeof c64_reu_entries[0],
    get_reu,
    set_reu
};

/* the C64 keys of each character (C64 matrix as in amiga_positional.vkm) */
static const amiga_machine_symkey_t c64_symkeys[] = {
    { '1', 7, 0, 0 }, { '2', 7, 3, 0 }, { '3', 1, 0, 0 }, { '4', 1, 3, 0 },
    { '5', 2, 0, 0 }, { '6', 2, 3, 0 }, { '7', 3, 0, 0 }, { '8', 3, 3, 0 },
    { '9', 4, 0, 0 }, { '0', 4, 3, 0 },
    { '!', 7, 0, 1 }, { '"', 7, 3, 1 }, { '#', 1, 0, 1 }, { '$', 1, 3, 1 },
    { '%', 2, 0, 1 }, { '&', 2, 3, 1 }, { '\'', 3, 0, 1 }, { '(', 3, 3, 1 },
    { ')', 4, 0, 1 },
    { '+', 5, 0, 0 }, { '-', 5, 3, 0 }, { 0xa3, 6, 0, 0 },  /* pound */
    { '@', 5, 6, 0 }, { '*', 6, 1, 0 }, { '^', 6, 6, 0 },   /* up arrow */
    { ':', 5, 5, 0 }, { ';', 6, 2, 0 }, { '=', 6, 5, 0 },
    { ',', 5, 7, 0 }, { '.', 5, 4, 0 }, { '/', 6, 7, 0 },
    { '_', 7, 1, 0 },                                       /* left arrow */
    { '[', 5, 5, 1 }, { ']', 6, 2, 1 },
    { '<', 5, 7, 1 }, { '>', 5, 4, 1 }, { '?', 6, 7, 1 }
};

/* C64 matrix of the letters A to Z */
static const unsigned char c64_letters[26][2] = {
    { 1, 2 }, { 3, 4 }, { 2, 4 }, { 2, 2 }, { 1, 6 }, { 2, 5 }, { 3, 2 },
    { 3, 5 }, { 4, 1 }, { 4, 2 }, { 4, 5 }, { 5, 2 }, { 4, 4 }, { 4, 7 },
    { 4, 6 }, { 5, 1 }, { 7, 6 }, { 2, 1 }, { 1, 5 }, { 2, 6 }, { 3, 6 },
    { 3, 7 }, { 1, 1 }, { 2, 7 }, { 3, 1 }, { 1, 4 }
};

/* made by amiga/CMakeLists.txt from data/C64/amiga_positional.vkm */
#include "amiga_positional_vkm_c64.h"

/* for the AmigaOS Version command (kept by "used": nothing reads it) */
static const char kvice_verstag[] __attribute__((used)) = KVICE_VERSTAG("x64");

const amiga_machine_t amiga_machine = {
    "KVICE x64",
    "x64",
    "C64",
    "C64",
    1,          /* pixel width */
    c64_roms, sizeof c64_roms / sizeof c64_roms[0],
    c64_roms_loaded,
    MSG_C64_MODEL,
    c64_models, sizeof c64_models / sizeof c64_models[0],
    c64_model_get,
    c64_model_set,
    c64_model_info,
    c64_model_rom_names,
    &c64_reu,
    0xa000,     /* BASIC RAM end */
    AMIGA_BASIC_V2,
    c64_symkeys, sizeof c64_symkeys / sizeof c64_symkeys[0],
    c64_letters,
    0, 1,       /* RETURN */
    amiga_positional_vkm,
    1           /* Tape menu */
};

/* ------------------------------------------------------------------------- */


/** \brief  Pre-initialize the UI before the canvas window gets created
 *
 * \return  0 on success, -1 on failure
 */
int c64ui_init_early(void)
{
    AMIGA_TRACE(("%s", __func__));
    return 0;
}


/** \brief  Initialize the UI
 *
 * \return  0 on success, -1 on failure
 */
int c64ui_init(void)
{
    AMIGA_TRACE(("%s", __func__));
    return 0;
}


/** \brief  Shut down the UI
 */
void c64ui_shutdown(void)
{
    AMIGA_TRACE(("%s", __func__));
}
