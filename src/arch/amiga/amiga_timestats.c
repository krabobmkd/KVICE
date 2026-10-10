/** \file   amiga_timestats.c
 * \brief   Measurement build: host time spent in each part of the emulation
 *
 * See timestats.h. Time is read with ReadEClock() (about 1.4 us steps) at
 * each TIMESTATS_ENTER()/TIMESTATS_LEAVE(): the elapsed time since the last
 * reading goes to the part running until then, so nested parts are
 * excluded from their parent. The readings themselves cost some time
 * (twice per raster line), it ends up in the measured parts.
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

#ifdef VICE_AMIGA_TIME_STATS

#include <stdio.h>
#include <string.h>

#include <exec/types.h>
#include <devices/timer.h>
#include <proto/timer.h>

#include "machine.h"
#include "maincpu.h"
#include "timestats.h"
#include "vsyncapi.h"

/* opened by archdep_tick.c */
extern struct Device *TimerBase;

#define TIMESTATS_PERIOD_SECONDS 5
#define TIMESTATS_DEPTH 16

static const char * const part_names[TSTAT_COUNT] = {
    "CPU", "VIC line", "VIC draw", "VIC gfx", "VIC sprites", "VIC fetch",
    "I/O", "CIA", "screen",
    "SID", "sound", "vsync", "wait"
};

static ULONG acc[TSTAT_COUNT];     /* E-clock ticks in each part */
/* measured intervals of each part: each one also contains about one
   ReadEClock() (half at each end), subtracted from the report */
static ULONG intervals[TSTAT_COUNT];
/* cost of one ReadEClock(), in 1/16 E-clock ticks (calibrated) */
static ULONG reading_cost16 = 0;
static ULONG calls[TSTAT_COUNT];   /* number of entries in each part */
static int stack[TIMESTATS_DEPTH];
static int depth = 0;
static int current = TSTAT_CPU;
static ULONG last = 0;          /* last reading */
static ULONG period_start = 0;
static ULONG eclock_freq = 0;
static CLOCK period_clk = 0;      /* emulated cycles at the period start */

static inline ULONG timestats_now(void)
{
    struct EClockVal ev;

    eclock_freq = ReadEClock(&ev);
    return ev.ev_lo;
}

/* count the time since the last reading for the running part */
static inline void timestats_account(void)
{
    ULONG now;

    if (TimerBase == NULL) {
        return;
    }
    now = timestats_now();
    acc[current] += now - last;
    intervals[current]++;
    last = now;
}

void amiga_timestats_enter(int part)
{
    timestats_account();
    if (depth < TIMESTATS_DEPTH) {
        stack[depth] = current;
    }
    depth++;
    current = part;
    calls[part]++;
}

void amiga_timestats_leave(void)
{
    timestats_account();
    if (depth > 0) {
        depth--;
        current = (depth < TIMESTATS_DEPTH) ? stack[depth] : TSTAT_CPU;
    }
}

/* per frame: print the report every TIMESTATS_PERIOD_SECONDS */
void amiga_timestats_frame(void)
{
    ULONG total, unit, emulated_ms, real_speed, overhead = 0;
    double speed, fps;
    int warp, i;

    timestats_account();
    if (TimerBase == NULL || eclock_freq == 0) {
        return;
    }
    if (period_start == 0) {
        /* calibration: the cost of one reading */
        ULONG t0 = timestats_now();

        for (i = 0; i < 64; i++) {
            timestats_now();
        }
        reading_cost16 = (timestats_now() - t0) * 16 / 65;
        printf("timestats: one ReadEClock() costs %lu.%02lu E-clock ticks (%lu ns)\n",
               (unsigned long)(reading_cost16 / 16),
               (unsigned long)((reading_cost16 % 16) * 100 / 16),
               (unsigned long)(reading_cost16 * 1000000 / 16 / (eclock_freq / 1000)));
        timestats_account();
        period_start = last;
        period_clk = maincpu_clk;
        memset(acc, 0, sizeof(acc));
        memset(calls, 0, sizeof(calls));
        memset(intervals, 0, sizeof(intervals));
        return;
    }
    total = last - period_start;
    if (total < eclock_freq * TIMESTATS_PERIOD_SECONDS) {
        return;
    }

    vsyncarch_get_metrics(&speed, &fps, &warp);

    /* in 0.1% steps, without overflow nor float */
    unit = total / 1000;
    if (unit == 0) {
        unit = 1;
    }
    /* real speed: emulated time / host time (VICE's own speed value is
       wrong while it keeps resyncing) */
    emulated_ms = (ULONG)((maincpu_clk - period_clk) / (machine_get_cycles_per_second() / 1000));
    real_speed = emulated_ms * 100 / (total / (eclock_freq / 1000));
    printf("timestats: real speed %lu%% (vsync says %d%% fps %d), %lu ms:",
           (unsigned long)real_speed, (int)speed, (int)fps,
           (unsigned long)(total / (eclock_freq / 1000)));
    /* the readings are not part of the emulation */
    for (i = 0; i < TSTAT_COUNT; i++) {
        ULONG cost = (ULONG)(((uint64_t)intervals[i] * reading_cost16) / 16);

        if (cost > acc[i]) {
            cost = acc[i];
        }
        acc[i] -= cost;
        overhead += cost;
    }
    for (i = 0; i < TSTAT_COUNT; i++) {
        ULONG permille = acc[i] / unit;

        printf(" %s %lu.%lu%%", part_names[i],
               (unsigned long)(permille / 10), (unsigned long)(permille % 10));
    }
    printf(" (measuring %lu.%lu%%)", (unsigned long)(overhead / unit / 10),
           (unsigned long)(overhead / unit % 10));
    printf("\ntimestats: %lu lines drawn, %lu screen refreshes, %lu sound flushes, "
           "%lu I/O accesses, %lu CIA accesses/alarms\n",
           (unsigned long)calls[TSTAT_VIC_DRAW], (unsigned long)calls[TSTAT_SCREEN],
           (unsigned long)calls[TSTAT_SOUND], (unsigned long)calls[TSTAT_IO],
           (unsigned long)calls[TSTAT_CIA]);
    fflush(stdout);

    memset(acc, 0, sizeof(acc));
    memset(calls, 0, sizeof(calls));
    memset(intervals, 0, sizeof(intervals));
    period_start = last;
    period_clk = maincpu_clk;
}

#endif
