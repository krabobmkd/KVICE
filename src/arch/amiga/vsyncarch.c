/** \file   vsyncarch.c
 * \brief   End-of-frame handling for native GTK3 UI
 *
 * \note    This is altered and trimmed down to fit into the GTK3-native
 *          world, but it's still heavily reliant on UNIX internals.
 *
 * \author  Dag Lem <resid@nimrod.no>
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
#include "amigavideo.h"
#include "amigawait.h"
#include "archdep_tick.h"

#include "kbdbuf.h"
#include "mainlock.h"
#include "ui.h"
#include "vsyncapi.h"
#include "timestats.h"
#include "videoarch.h"

#include "joystick.h"

#ifdef WINDOWS_COMPILE
#   include "windows.h"
#elif defined(HAVE_NANOSLEEP)
#   include <time.h>
#else
#   include <unistd.h>
#   include <errno.h>
#   include <sys/time.h>
#endif

#ifdef MACOS_COMPILE
#   include <mach/mach.h>
#   include <mach/mach_time.h>
#endif

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

static int pause_pending = 0;

/* ------------------------------------------------------------------------- */

void vsyncarch_presync(void)
{
    TIMESTATS_FRAME();
    /* window and MUI events, ui_dispatch_events() is only used by the monitor */
    amiga_wait_poll_events();
    ui_update_lightpen();
    joystick();
}

void vsyncarch_postsync(void)
{
/* heartbeat trace, too slow for real machine tests: enable when needed */
#if 0
    static unsigned int frame_count = 0;

    static tick_t last_now = 0;
    static tick_t last_slept = 0;

    /* heartbeat every 50 frames: emulated speed, and host CPU load measured
     * as the part of the elapsed time not spent sleeping in vsync */
    if ((frame_count++ % 50) == 0) {
        tick_t now = tick_now();
        tick_t slept = amiga_tick_slept_total();
        tick_t elapsed = now - last_now;
        unsigned int busy = 0;
        double speed, fps;
        int warp;

        if (last_now != 0 && elapsed > 0) {
            busy = 100 - (unsigned int)((uint64_t)(slept - last_slept) * 100 / elapsed);
        }
        unsigned long ahi_req, ahi_under, ahi_used;
        int ahi_ok;

        vsyncarch_get_metrics(&speed, &fps, &warp);
        ahi_ok = amiga_ahi_stats(&ahi_req, &ahi_under, &ahi_used);
        AMIGA_TRACE(("vsync heartbeat, frame %u: speed %d%% fps %d, host busy %u%%, "
                     "AHI %s: %lu requests, %lu underruns, %lu frames buffered",
                     frame_count, (int)speed, (int)fps, busy,
                     ahi_ok ? "on" : "off", ahi_req, ahi_under, ahi_used));
        last_now = now;
        last_slept = slept;
    }
#endif
#ifdef VICE_AMIGA_DIV64_STATS
    {
        /* 64 bit division counters (amiga_div64.c), every 250 frames */
        extern void amiga_div64_report(uint32_t elapsed_us);
        static unsigned int div64_frames = 0;
        static tick_t div64_last = 0;

        if (++div64_frames >= 250) {
            tick_t now = tick_now();

            if (div64_last != 0) {
                amiga_div64_report((uint32_t)(now - div64_last));
            }
            div64_last = now;
            div64_frames = 0;
        }
    }
#endif
    /* this function is called once a frame, so this
       handles single frame advance */
    if (pause_pending) {
        ui_pause_enable();
        pause_pending = 0;
    }
}

void vsyncarch_advance_frame(void)
{
    ui_pause_disable();
    pause_pending = 1;
}
