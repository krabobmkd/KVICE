/** \file   kvice_framecheck.c
 * \brief   Linux test build: a hash of every emulated frame
 *
 * Called at the end of each frame (raster-canvas.c, KVICE_FRAME_CHECKSUM),
 * before any frame skipping: the whole draw buffer of the frame goes into
 * one running hash, printed at exit. Two runs with the same program, -seed
 * and -limitcycles give the same hash when the emulated screens were the
 * same pixel for pixel: optimizations of the drawing code must not change
 * it. KVICE_FRAMECHECK=0 in the environment disables it (instruction
 * counts of the emulation alone).
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "autostart.h"
#include "kvice_framecheck.h"
#include "mem.h"
#include "screenshot.h"
#include "video.h"

static uint64_t frames_hash = 14695981039346656037ULL;  /* FNV-1a 64 */
static unsigned long frames = 0;
static int registered = 0;

/* test: KVICE_DUMPSCREEN=<hex address>[,columns,rows] prints the screen
   codes there as text at exit (C64 $0400, Plus/4 $0C00 40x25, VIC-20
   $1E00,22,23): did BASIC start? Read at the end of each frame, while the
   machine still runs: at exit its memory may be gone already. */
static unsigned int dump_base, dump_cols, dump_rows;
static unsigned char dump_codes[80 * 50];
static int dump_state = 0;   /* 0: not looked at, -1: off, 1: on, 2: read */

static void dump_screen_read(void)
{
    unsigned int i;

    if (dump_state == 0) {
        const char *env = getenv("KVICE_DUMPSCREEN");
        char *end;

        dump_state = -1;
        if (env == NULL) {
            return;
        }
        dump_cols = 40;
        dump_rows = 25;
        dump_base = (unsigned int)strtoul(env, &end, 16);
        if (*end == ',') {
            dump_cols = (unsigned int)strtoul(end + 1, &end, 10);
            if (*end == ',') {
                dump_rows = (unsigned int)strtoul(end + 1, NULL, 10);
            }
        }
        if (dump_cols == 0 || dump_cols > 80 || dump_rows == 0 || dump_rows > 50) {
            return;
        }
        dump_state = 1;
    }
    if (dump_state < 0) {
        return;
    }
    for (i = 0; i < dump_cols * dump_rows; i++) {
        dump_codes[i] = mem_bank_peek(0, (uint16_t)(dump_base + i), NULL);
    }
    dump_state = 2;
}

static void dump_screen(void)
{
    unsigned int x, y;

    if (dump_state != 2) {
        return;
    }
    for (y = 0; y < dump_rows; y++) {
        char line[81];

        for (x = 0; x < dump_cols; x++) {
            unsigned int c = dump_codes[y * dump_cols + x] & 0x7f;

            /* screen codes: 1-26 letters, 32-63 as ASCII */
            line[x] = (c >= 1 && c <= 26) ? (char)('A' + c - 1)
                      : (c >= 32 && c < 64) ? (char)c : (c == 0 ? '@' : '.');
        }
        line[dump_cols] = '\0';
        fprintf(stderr, "screen: |%s|\n", line);
    }
}

static void framecheck_report(void)
{
    fprintf(stderr, "framecheck: %lu frames, hash %016llx\n",
            frames, (unsigned long long)frames_hash);
    dump_screen();
}

/* test: KVICE_AUTOSTART2=<file> autostarts a second file at frame
   KVICE_AUTOSTART2_FRAME (default 1500), as the Amiga Autostart menu does
   while a program runs */
static void second_autostart(void)
{
    static long at = -1;
    const char *file = getenv("KVICE_AUTOSTART2");

    if (file == NULL) {
        return;
    }
    if (at < 0) {
        const char *f = getenv("KVICE_AUTOSTART2_FRAME");

        at = f != NULL ? atol(f) : 1500;
    }
    if ((long)frames == at) {
        fprintf(stderr, "framecheck: second autostart of %s at frame %lu\n", file, frames);
        autostart_autodetect(file, NULL, 0, AUTOSTART_MODE_RUN);
    }
}

/* KVICE_AUTOSTART2_PRESYNC set: the second autostart from the vsync
   presync, where the Amiga handles its menus, instead of the frame end */
void kvice_test_presync(void)
{
    if (getenv("KVICE_AUTOSTART2_PRESYNC") != NULL) {
        second_autostart();
    }
}

void kvice_frame_checksum(const struct draw_buffer_s *db)
{
    unsigned int x, y;

    if (getenv("KVICE_AUTOSTART2_PRESYNC") == NULL) {
        second_autostart();
    }

    if (!registered) {
        /* KVICE_FRAMECHECK=0: no hash, when counting the instructions of
           the emulation alone */
        const char *env = getenv("KVICE_FRAMECHECK");

        registered = (env != NULL && env[0] == '0') ? -1 : 1;
        if (registered == 1) {
            atexit(framecheck_report);
        }
    }
    dump_screen_read();
    if (registered < 0) {
        return;
    }
    if (db == NULL || db->draw_buffer == NULL) {
        return;
    }
    /* 4 pixels at a time: cheap enough not to hide the emulation cost */
    for (y = 0; y < db->draw_buffer_height; y++) {
        const uint8_t *p = db->draw_buffer + y * db->draw_buffer_pitch;

        for (x = 0; x + 4 <= db->draw_buffer_width; x += 4) {
            uint32_t w = (uint32_t)p[x] | ((uint32_t)p[x + 1] << 8)
                         | ((uint32_t)p[x + 2] << 16) | ((uint32_t)p[x + 3] << 24);

            frames_hash = (frames_hash ^ w) * 1099511628211ULL;
        }
        for (; x < db->draw_buffer_width; x++) {
            frames_hash = (frames_hash ^ p[x]) * 1099511628211ULL;
        }
    }
    frames++;
}

void kvice_frame_canvas(struct video_canvas_s *canvas)
{
    static long frame = 0;
    static long at = -1;
    const char *file = getenv("KVICE_SCREENSHOT");

    if (file == NULL) {
        return;
    }
    if (at < 0) {
        const char *f = getenv("KVICE_SCREENSHOT_FRAME");

        at = (f != NULL) ? atol(f) : 150;
    }
    if (++frame == at) {
        fprintf(stderr, "screenshot: frame %ld to %s: %s\n", frame, file,
                screenshot_save("IFF", file, canvas) == 0 ? "ok" : "FAILED");
    }
}
