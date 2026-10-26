/** \file   amigamonchips.c
 * \brief   AmigaOS 3.x port: video and sound chip lists of the monitor window
 *
 * Rows of 3 columns, function | address | value, the value decoded: the
 * sprites that are on and where they are, where the screen, the charset or
 * the bitmap are in memory, the SID voices with their frequency, waveform
 * and envelope...
 *
 * x64: VIC-II and SID, xplus4: TED (video and sound), xvic: VIC (video and
 * sound). The registers are read without side effect (the "io" bank peeks
 * of the monitor, the SID shadow of the written registers): reading them
 * here does not acknowledge an interrupt nor change the emulation.
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

#include "amigamonchips.h"
#include "fastsid.h"
#include "machine.h"
#include "monitor.h"
#include "montypes.h"
#include "resources.h"
#include "sid.h"
#include "sound.h"

/* ------------------------------------------------------------------------- */
/* reading and filling */

static int io_bank = -2;

/* a chip register, as the monitor "io" bank shows it (no side effect) */
static uint8_t chip_peek(uint16_t addr)
{
    if (io_bank == -2) {
        io_bank = mon_interfaces[e_comp_space]->mem_bank_from_name("io");
        if (io_bank < 0) {
            /* xvic: the CPU view has the I/O at all times */
            io_bank = 0;
        }
    }
    return mon_get_mem_val_ex_nosfx(e_comp_space, io_bank, addr);
}

typedef struct rows_s {
    amiga_chip_row_t *rows;
    int count;
    int max;
} rows_t;

/* a row: addr < 0 for a value computed from other registers (no address) */
static amiga_chip_row_t *add_row(rows_t *r, const char *name, int addr, int addr_end)
{
    amiga_chip_row_t *row;

    if (r->count >= r->max) {
        return NULL;
    }
    row = &r->rows[r->count++];
    snprintf(row->name, sizeof row->name, "%s", name);
    if (addr < 0) {
        row->addr[0] = '\0';
    } else if (addr_end > addr) {
        snprintf(row->addr, sizeof row->addr, "%04X-%X", (unsigned)addr, (unsigned)(addr_end & 0xf));
    } else {
        snprintf(row->addr, sizeof row->addr, "%04X", (unsigned)addr);
    }
    row->value[0] = '\0';
    return row;
}

#define ROW_VALUE(row, ...) \
    do { \
        if ((row) != NULL) { \
            snprintf((row)->value, sizeof (row)->value, __VA_ARGS__); \
        } \
    } while (0)

/* the names of the bits set, high bit first ("DEN BMM"), NULL: not shown */
static void bit_names(char *out, size_t size, uint8_t value, const char *const names[8])
{
    int bit;
    size_t len = strlen(out);

    for (bit = 7; bit >= 0; bit--) {
        if ((value & (1 << bit)) && names[7 - bit] != NULL && len + 6 < size) {
            len += (size_t)sprintf(out + len, " %s", names[7 - bit]);
        }
    }
}

/* "01----6-": the numbers of the bits set, bit 0 first */
static void bit_map(char *out, uint8_t value)
{
    int bit;

    for (bit = 0; bit < 8; bit++) {
        out[bit] = (value & (1 << bit)) ? (char)('0' + bit) : '-';
    }
    out[8] = '\0';
}

static const char *const c64_colors[16] = {
    "black", "white", "red", "cyan", "purple", "green", "blue", "yellow",
    "orange", "brown", "light red", "dark grey", "grey", "light green",
    "light blue", "light grey"
};

/* ------------------------------------------------------------------------- */
/* C64: VIC-II */

static void vicii_rows(rows_t *r)
{
    static const char *const d011_bits[8] = { NULL, "ECM", "BMM", "DEN", "RSEL", NULL, NULL, NULL };
    static const char *const d016_bits[8] = { NULL, NULL, NULL, "MCM", "CSEL", NULL, NULL, NULL };
    static const char *const irq_bits[8] = { "IRQ", NULL, NULL, NULL, "LP", "SSC", "SBC", "RST" };
    amiga_chip_row_t *row;
    uint8_t d011 = chip_peek(0xd011), d016 = chip_peek(0xd016), d018 = chip_peek(0xd018);
    uint8_t d015 = chip_peek(0xd015), d010 = chip_peek(0xd010);
    unsigned int bank = (unsigned int)(3 - (chip_peek(0xdd00) & 3)) * 0x4000;
    unsigned int screen = bank + ((d018 >> 4) & 15) * 0x400;
    unsigned int chars = bank + ((d018 >> 1) & 7) * 0x800;
    char map[9];
    int bmm = (d011 & 0x20) != 0;
    int i;

    row = add_row(r, "Raster line", 0xd012, 0);
    ROW_VALUE(row, "%u", ((d011 & 0x80) << 1) | chip_peek(0xd012));
    row = add_row(r, "Control 1", 0xd011, 0);
    ROW_VALUE(row, "$%02X YSCR=%u", d011, d011 & 7);
    if (row != NULL) {
        bit_names(row->value, sizeof row->value, d011, d011_bits);
    }
    row = add_row(r, "Control 2", 0xd016, 0);
    ROW_VALUE(row, "$%02X XSCR=%u", d016, d016 & 7);
    if (row != NULL) {
        bit_names(row->value, sizeof row->value, d016, d016_bits);
    }
    row = add_row(r, "Mode", -1, 0);
    ROW_VALUE(row, "%s%s%s", (d011 & 0x40) ? "extended color " : "",
              (d016 & 0x10) ? "multicolor " : "", bmm ? "bitmap" : "text");
    row = add_row(r, "VIC bank", 0xdd00, 0);
    ROW_VALUE(row, "$%04X-$%04X", bank, bank + 0x3fff);
    row = add_row(r, "Memory", 0xd018, 0);
    ROW_VALUE(row, "$%02X", d018);
    row = add_row(r, "Screen", -1, 0);
    ROW_VALUE(row, "$%04X", screen);
    if (bmm) {
        row = add_row(r, "Bitmap", -1, 0);
        ROW_VALUE(row, "$%04X", bank + (d018 & 8) * 0x400);
    } else {
        row = add_row(r, "Charset", -1, 0);
        /* banks 0 and 2 see the character ROM at $1000-$1FFF */
        ROW_VALUE(row, "$%04X%s", chars,
                  ((bank & 0x4000) == 0 && (chars & 0x3000) == 0x1000) ? " ROM" : "");
    }
    row = add_row(r, "Border", 0xd020, 0);
    ROW_VALUE(row, "%u %s", chip_peek(0xd020) & 15, c64_colors[chip_peek(0xd020) & 15]);
    row = add_row(r, "Background", 0xd021, 0);
    ROW_VALUE(row, "%u %s", chip_peek(0xd021) & 15, c64_colors[chip_peek(0xd021) & 15]);
    row = add_row(r, "IRQ enable", 0xd01a, 0);
    ROW_VALUE(row, "$%02X", chip_peek(0xd01a) & 15);
    if (row != NULL) {
        bit_names(row->value, sizeof row->value, (uint8_t)(chip_peek(0xd01a) & 15), irq_bits);
    }
    row = add_row(r, "IRQ flags", 0xd019, 0);
    ROW_VALUE(row, "$%02X", chip_peek(0xd019));
    if (row != NULL) {
        bit_names(row->value, sizeof row->value, chip_peek(0xd019), irq_bits);
    }

    bit_map(map, d015);
    row = add_row(r, "Sprites on", 0xd015, 0);
    ROW_VALUE(row, "%s", map);
    bit_map(map, chip_peek(0xd01c));
    row = add_row(r, "Spr multicol", 0xd01c, 0);
    ROW_VALUE(row, "%s", map);
    bit_map(map, chip_peek(0xd01d));
    row = add_row(r, "Spr X expand", 0xd01d, 0);
    ROW_VALUE(row, "%s", map);
    bit_map(map, chip_peek(0xd017));
    row = add_row(r, "Spr Y expand", 0xd017, 0);
    ROW_VALUE(row, "%s", map);
    bit_map(map, chip_peek(0xd01b));
    row = add_row(r, "Spr behind", 0xd01b, 0);
    ROW_VALUE(row, "%s", map);
    for (i = 0; i < 8; i++) {
        char name[16];
        unsigned int x, y, ptr;

        if (!(d015 & (1 << i))) {
            continue;
        }
        x = chip_peek((uint16_t)(0xd000 + i * 2)) | ((d010 >> i) & 1) << 8;
        y = chip_peek((uint16_t)(0xd001 + i * 2));
        /* the pointer is in RAM, at the end of the screen */
        ptr = mon_get_mem_val_ex_nosfx(e_comp_space, mon_interfaces[e_comp_space]->mem_bank_from_name("ram"),
                                       (uint16_t)(screen + 0x3f8 + i));
        sprintf(name, "Sprite %d", i);
        row = add_row(r, name, 0xd000 + i * 2, 0xd001 + i * 2);
        ROW_VALUE(row, "X=%u Y=%u data $%04X col %u", x, y, bank + ptr * 64,
                  chip_peek((uint16_t)(0xd027 + i)) & 15);
    }
}

/* ------------------------------------------------------------------------- */
/* C64: SID */

static int sid_is_fastsid(void)
{
#ifdef HAVE_RESID
    int engine = -1;

    resources_get_int("SidEngine", &engine);
    return engine == SID_ENGINE_FASTSID;
#else
    /* the only engine of the Amiga build */
    return 1;
#endif
}

static void sid_rows(rows_t *r)
{
    static const char *const ctrl_bits[8] = { "NOI", "PUL", "SAW", "TRI", "TEST", "RING", "SYNC", "GATE" };
    static const char *const filt_bits[8] = { NULL, NULL, NULL, NULL, "EXT", "V3", "V2", "V1" };
    static const char *const mode_bits[8] = { "3OFF", "HP", "BP", "LP", NULL, NULL, NULL, NULL };
    static const char *const env_modes[5] = { "attack", "decay", "sustain", "release", "idle" };
    amiga_chip_row_t *row;
    uint32_t clk16 = (uint32_t)(machine_get_cycles_per_second() >> 4);
    sound_t *psid = sid_is_fastsid() ? sound_get_psid(0) : NULL;
    int v;

    for (v = 0; v < 3; v++) {
        uint16_t base = (uint16_t)(0xd400 + v * 7);
        unsigned int freq = sid_peek(base) | (sid_peek((uint16_t)(base + 1)) << 8);
        unsigned int pw = (sid_peek((uint16_t)(base + 2)) | (sid_peek((uint16_t)(base + 3)) << 8)) & 0xfff;
        uint8_t ctrl = sid_peek((uint16_t)(base + 4));
        uint8_t ad = sid_peek((uint16_t)(base + 5));
        uint8_t sr = sid_peek((uint16_t)(base + 6));
        char name[16];

        sprintf(name, "V%d freq", v + 1);
        row = add_row(r, name, base, base + 1);
        /* Hz = freq * clock / 2^24, in 32 bits */
        ROW_VALUE(row, "$%04X %u Hz", freq, (unsigned int)((freq * clk16) >> 20));
        sprintf(name, "V%d pulse", v + 1);
        row = add_row(r, name, base + 2, base + 3);
        ROW_VALUE(row, "$%03X %u%%", pw, pw * 100 / 4096);
        sprintf(name, "V%d control", v + 1);
        row = add_row(r, name, base + 4, 0);
        ROW_VALUE(row, "$%02X", ctrl);
        if (row != NULL) {
            bit_names(row->value, sizeof row->value, ctrl, ctrl_bits);
        }
        sprintf(name, "V%d ADSR", v + 1);
        row = add_row(r, name, base + 5, base + 6);
        ROW_VALUE(row, "A=%X D=%X S=%X R=%X", ad >> 4, ad & 15, sr >> 4, sr & 15);
        if (psid != NULL) {
            int mode = 0;
            int level = fastsid_voice_envelope(psid, v, &mode);

            sprintf(name, "V%d envelope", v + 1);
            row = add_row(r, name, -1, 0);
            ROW_VALUE(row, "$%02X %s", (unsigned int)level, env_modes[mode <= 4 ? mode : 4]);
        }
    }
    row = add_row(r, "Cutoff", 0xd415, 0xd416);
    ROW_VALUE(row, "$%03X", (unsigned int)((sid_peek(0xd415) & 7) | (sid_peek(0xd416) << 3)));
    row = add_row(r, "Resonance", 0xd417, 0);
    ROW_VALUE(row, "%u", (unsigned int)(sid_peek(0xd417) >> 4));
    if (row != NULL) {
        bit_names(row->value, sizeof row->value, (uint8_t)(sid_peek(0xd417) & 15), filt_bits);
    }
    row = add_row(r, "Mode/volume", 0xd418, 0);
    ROW_VALUE(row, "vol %u", (unsigned int)(sid_peek(0xd418) & 15));
    if (row != NULL) {
        bit_names(row->value, sizeof row->value, sid_peek(0xd418), mode_bits);
    }
}

/* ------------------------------------------------------------------------- */
/* Plus/4: TED */

/* TED color: 4 bits hue, 3 bits luminance */
static const char *const ted_hues[16] = {
    "black", "white", "red", "cyan", "purple", "green", "blue", "yellow",
    "orange", "brown", "yel-green", "pink", "blue-green", "light blue",
    "dark blue", "light green"
};

static void ted_color_row(rows_t *r, const char *name, uint16_t addr)
{
    uint8_t c = chip_peek(addr);
    amiga_chip_row_t *row = add_row(r, name, addr, 0);

    ROW_VALUE(row, "$%02X %s lum %u", c, ted_hues[c & 15], (unsigned int)((c >> 4) & 7));
}

static void ted_rows(rows_t *r)
{
    static const char *const ff06_bits[8] = { NULL, "ECM", "BMM", "DEN", "RSEL", NULL, NULL, NULL };
    static const char *const ff07_bits[8] = { "RVS", "NTSC", "FRZ", "MCM", "CSEL", NULL, NULL, NULL };
    static const char *const irq_bits[8] = { "IRQ", "T3", NULL, "T2", "T1", "LP", "RST", NULL };
    amiga_chip_row_t *row;
    uint8_t ff06 = chip_peek(0xff06), ff07 = chip_peek(0xff07);
    uint8_t ff12 = chip_peek(0xff12), ff13 = chip_peek(0xff13), ff14 = chip_peek(0xff14);

    row = add_row(r, "Raster line", 0xff1c, 0xff1d);
    ROW_VALUE(row, "%u", (unsigned int)(((chip_peek(0xff1c) & 1) << 8) | chip_peek(0xff1d)));
    row = add_row(r, "Control 1", 0xff06, 0);
    ROW_VALUE(row, "$%02X YSCR=%u", ff06, ff06 & 7);
    if (row != NULL) {
        bit_names(row->value, sizeof row->value, ff06, ff06_bits);
    }
    row = add_row(r, "Control 2", 0xff07, 0);
    ROW_VALUE(row, "$%02X XSCR=%u", ff07, ff07 & 7);
    if (row != NULL) {
        bit_names(row->value, sizeof row->value, ff07, ff07_bits);
    }
    row = add_row(r, "Mode", -1, 0);
    ROW_VALUE(row, "%s%s%s", (ff06 & 0x40) ? "extended color " : "",
              (ff07 & 0x10) ? "multicolor " : "", (ff06 & 0x20) ? "bitmap" : "text");
    row = add_row(r, "Attributes", 0xff14, 0);
    ROW_VALUE(row, "$%04X", (unsigned int)(ff14 & 0xf8) << 8);
    row = add_row(r, "Screen", -1, 0);
    ROW_VALUE(row, "$%04X", ((unsigned int)(ff14 & 0xf8) << 8) + 0x400);
    if (ff06 & 0x20) {
        row = add_row(r, "Bitmap", 0xff12, 0);
        ROW_VALUE(row, "$%04X", (unsigned int)((ff12 >> 3) & 7) * 0x2000);
    } else {
        row = add_row(r, "Charset", 0xff13, 0);
        ROW_VALUE(row, "$%04X%s", (unsigned int)(ff13 & 0xfc) << 8,
                  (ff12 & 4) ? " ROM" : "");
    }
    ted_color_row(r, "Background", 0xff15);
    ted_color_row(r, "Color 1", 0xff16);
    ted_color_row(r, "Color 2", 0xff17);
    ted_color_row(r, "Color 3", 0xff18);
    ted_color_row(r, "Border", 0xff19);
    row = add_row(r, "IRQ enable", 0xff0a, 0);
    ROW_VALUE(row, "$%02X", chip_peek(0xff0a));
    if (row != NULL) {
        bit_names(row->value, sizeof row->value, (uint8_t)(chip_peek(0xff0a) & 0x7e), irq_bits);
    }
    row = add_row(r, "IRQ flags", 0xff09, 0);
    ROW_VALUE(row, "$%02X", chip_peek(0xff09));
    if (row != NULL) {
        bit_names(row->value, sizeof row->value, chip_peek(0xff09), irq_bits);
    }
    row = add_row(r, "Cursor", 0xff0c, 0xff0d);
    ROW_VALUE(row, "%u", (unsigned int)(((chip_peek(0xff0c) & 3) << 8) | chip_peek(0xff0d)));
}

static void ted_sound_rows(rows_t *r)
{
    amiga_chip_row_t *row;
    uint8_t ff11 = chip_peek(0xff11);
    unsigned int f1 = chip_peek(0xff0e) | ((chip_peek(0xff12) & 3) << 8);
    unsigned int f2 = chip_peek(0xff0f) | ((chip_peek(0xff10) & 3) << 8);
    /* single clock / 8 / 2: 111860 Hz PAL */
    unsigned int base = (unsigned int)(machine_get_cycles_per_second() / 8);

    row = add_row(r, "Voice 1", 0xff0e, 0);
    ROW_VALUE(row, "$%03X %u Hz%s", f1, f1 < 1023 ? base / (1024 - f1) : 0,
              (ff11 & 0x10) ? " on" : "");
    row = add_row(r, "Voice 2", 0xff0f, 0);
    ROW_VALUE(row, "$%03X %u Hz%s%s", f2, f2 < 1023 ? base / (1024 - f2) : 0,
              (ff11 & 0x20) ? " on" : "", (ff11 & 0x40) ? " noise" : "");
    row = add_row(r, "Control", 0xff11, 0);
    ROW_VALUE(row, "$%02X vol %u%s", ff11, (unsigned int)(ff11 & 15), (ff11 & 0x80) ? " D/A" : "");
}

/* ------------------------------------------------------------------------- */
/* VIC-20: VIC */

static void vic_rows(rows_t *r)
{
    static const char *const vic_colors[16] = {
        "black", "white", "red", "cyan", "purple", "green", "blue", "yellow",
        "orange", "light orange", "pink", "light cyan", "light purple",
        "light green", "light blue", "light yellow"
    };
    amiga_chip_row_t *row;
    uint8_t r0 = chip_peek(0x9000), r1 = chip_peek(0x9001), r2 = chip_peek(0x9002);
    uint8_t r3 = chip_peek(0x9003), r5 = chip_peek(0x9005), rf = chip_peek(0x900f);
    /* VIC address bit 13 is the inverse of CPU address bit 15 */
    unsigned int screen = ((unsigned int)(r2 & 0x80) << 2) + ((unsigned int)(r5 & 0x70) << 6)
                          + ((r5 & 0x80) ? 0 : 0x8000);
    unsigned int chars = ((unsigned int)(r5 & 7) << 10) + ((r5 & 8) ? 0 : 0x8000);

    row = add_row(r, "Raster line", 0x9004, 0);
    ROW_VALUE(row, "%u", (unsigned int)((chip_peek(0x9004) << 1) | (r3 >> 7)));
    row = add_row(r, "Origin X", 0x9000, 0);
    ROW_VALUE(row, "%u%s", (unsigned int)(r0 & 0x7f), (r0 & 0x80) ? " interlace" : "");
    row = add_row(r, "Origin Y", 0x9001, 0);
    ROW_VALUE(row, "%u", (unsigned int)r1);
    row = add_row(r, "Columns", 0x9002, 0);
    ROW_VALUE(row, "%u", (unsigned int)(r2 & 0x7f));
    row = add_row(r, "Rows", 0x9003, 0);
    ROW_VALUE(row, "%u%s", (unsigned int)((r3 >> 1) & 0x3f), (r3 & 1) ? " 8x16 chars" : "");
    row = add_row(r, "Memory", 0x9005, 0);
    ROW_VALUE(row, "$%02X", r5);
    row = add_row(r, "Screen", -1, 0);
    ROW_VALUE(row, "$%04X", screen);
    row = add_row(r, "Color RAM", -1, 0);
    ROW_VALUE(row, "$%04X", (r2 & 0x80) ? 0x9600 : 0x9400);
    row = add_row(r, "Charset", -1, 0);
    ROW_VALUE(row, "$%04X%s", chars, (chars & 0xf000) == 0x8000 ? " ROM" : "");
    row = add_row(r, "Light pen", 0x9006, 0x9007);
    ROW_VALUE(row, "X=%u Y=%u", (unsigned int)chip_peek(0x9006), (unsigned int)chip_peek(0x9007));
    row = add_row(r, "Paddles", 0x9008, 0x9009);
    ROW_VALUE(row, "X=%u Y=%u", (unsigned int)chip_peek(0x9008), (unsigned int)chip_peek(0x9009));
    row = add_row(r, "Border", 0x900f, 0);
    ROW_VALUE(row, "%u %s", (unsigned int)(rf & 7), vic_colors[rf & 7]);
    row = add_row(r, "Background", 0x900f, 0);
    ROW_VALUE(row, "%u %s%s", (unsigned int)(rf >> 4), vic_colors[rf >> 4],
              (rf & 8) ? "" : " reverse");
    row = add_row(r, "Auxiliary", 0x900e, 0);
    ROW_VALUE(row, "%u %s", (unsigned int)(chip_peek(0x900e) >> 4), vic_colors[chip_peek(0x900e) >> 4]);
}

static void vic_sound_rows(rows_t *r)
{
    static const char *const names[4] = { "Bass", "Alto", "Soprano", "Noise" };
    amiga_chip_row_t *row;
    int v;

    for (v = 0; v < 4; v++) {
        uint8_t n = chip_peek((uint16_t)(0x900a + v));

        row = add_row(r, names[v], 0x900a + v, 0);
        ROW_VALUE(row, "$%02X %s", (unsigned int)(n & 0x7f), (n & 0x80) ? "on" : "off");
    }
    row = add_row(r, "Volume", 0x900e, 0);
    ROW_VALUE(row, "%u", (unsigned int)(chip_peek(0x900e) & 15));
}

/* ------------------------------------------------------------------------- */

const char *amiga_chips_video_title(void)
{
    switch (machine_class) {
        case VICE_MACHINE_PLUS4:
            return "TED";
        case VICE_MACHINE_VIC20:
            return "VIC";
        default:
            return "VIC-II";
    }
}

const char *amiga_chips_sound_title(void)
{
    switch (machine_class) {
        case VICE_MACHINE_PLUS4:
            return "TED sound";
        case VICE_MACHINE_VIC20:
            return "VIC sound";
        default:
            return "SID";
    }
}

int amiga_chips_video_rows(amiga_chip_row_t *rows, int max)
{
    rows_t r = { rows, 0, max };

    switch (machine_class) {
        case VICE_MACHINE_PLUS4:
            ted_rows(&r);
            break;
        case VICE_MACHINE_VIC20:
            vic_rows(&r);
            break;
        default:
            vicii_rows(&r);
            break;
    }
    return r.count;
}

int amiga_chips_sound_rows(amiga_chip_row_t *rows, int max)
{
    rows_t r = { rows, 0, max };

    switch (machine_class) {
        case VICE_MACHINE_PLUS4:
            ted_sound_rows(&r);
            break;
        case VICE_MACHINE_VIC20:
            vic_sound_rows(&r);
            break;
        default:
            sid_rows(&r);
            break;
    }
    return r.count;
}
