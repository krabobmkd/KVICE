/** \file   amigawait.c
 * \brief   AmigaOS 3.x main loop waiting: VBlank signal + UI events
 *
 * Instead of sleeping, the emulation waits with a single exec Wait() for:
 * - a signal sent by a VBlank interrupt server (50/60 Hz, the cheapest timer
 *   on Amiga: it only counts and Signal()s the main task),
 * - the emulator window IDCMP port,
 * - the MUI settings window signals, only while that window is open,
 * - CTRL-C.
 * UI events are handled as soon as they arrive, the wait goes on until the
 * deadline asked by VICE (E-clock time, see archdep_tick.c) is reached.
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
#include <stdlib.h>

#include <exec/types.h>
#include <exec/interrupts.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <hardware/intbits.h>
#include <dos/dos.h>
#include <proto/exec.h>

#include "amigatrace.h"
#include "amigavideo.h"
#include "amigamui.h"
#include "amigawait.h"
#include "archdep.h"
#include "archdep_exit.h"

/* shared with the interrupt: must be in public memory */
typedef struct vblank_data_s {
    struct Task *task;
    ULONG sigmask;
    volatile ULONG count;
} vblank_data_t;

static struct Interrupt *vblank_int = NULL;
static vblank_data_t *vblank_data = NULL;
static BYTE vblank_signal = -1;
static int atexit_registered = 0;

/* VBlank interrupt server: count, wake the main task.
 * a1 = is_Data. Returning 0 sets the Z flag: the server chain goes on. */
static int vblank_server(register vblank_data_t *data __asm("a1"))
{
    data->count++;
    Signal(data->task, data->sigmask);
    return 0;
}

/** \brief  Remove the VBlank server, safe to call more than once
 *
 * Registered with atexit(): an interrupt server must never outlive the
 * program code.
 */
void amiga_wait_shutdown(void)
{
    if (vblank_int != NULL) {
        RemIntServer(INTB_VERTB, vblank_int);
        FreeVec(vblank_int);
        vblank_int = NULL;
    }
    if (vblank_data != NULL) {
        FreeVec(vblank_data);
        vblank_data = NULL;
    }
    if (vblank_signal != -1) {
        FreeSignal(vblank_signal);
        vblank_signal = -1;
    }
}

int amiga_wait_init(void)
{
    if (vblank_int != NULL) {
        return 0;
    }
    if (!atexit_registered) {
        atexit(amiga_wait_shutdown);
        atexit_registered = 1;
    }
    vblank_signal = AllocSignal(-1);
    vblank_data = AllocVec(sizeof *vblank_data, MEMF_PUBLIC | MEMF_CLEAR);
    vblank_int = AllocVec(sizeof *vblank_int, MEMF_PUBLIC | MEMF_CLEAR);
    if (vblank_signal == -1 || vblank_data == NULL || vblank_int == NULL) {
        amiga_wait_shutdown();
        return -1;
    }
    vblank_data->task = FindTask(NULL);
    vblank_data->sigmask = 1UL << vblank_signal;

    vblank_int->is_Node.ln_Type = NT_INTERRUPT;
    vblank_int->is_Node.ln_Pri = 0;
    vblank_int->is_Node.ln_Name = (char *)"VICE vblank";
    vblank_int->is_Data = vblank_data;
    vblank_int->is_Code = (void (*)(void))vblank_server;
    AddIntServer(INTB_VERTB, vblank_int);

    AMIGA_TRACE(("VBlank server installed, signal %d", (int)vblank_signal));
    return 0;
}

ULONG amiga_wait_vblank_count(void)
{
    return (vblank_data != NULL) ? vblank_data->count : 0;
}

/** \brief  Wait until tick_now() reaches \a deadline, handling UI events
 *
 * \param[in]   deadline    E-clock based tick (see archdep_tick.c)
 */
/* CTRL-C from the Shell: quit like the close gadget, VICE shutdown and all */
static inline void amiga_check_ctrl_c(ULONG sigs)
{
    if (sigs & SIGBREAKF_CTRL_C) {
        AMIGA_TRACE(("CTRL-C"));
        archdep_vice_exit(0);
    }
}

void amiga_wait_until(tick_t deadline)
{
    ULONG sigs;

    if (vblank_data == NULL) {
        /* no VBlank server (init failed): do not block forever */
        return;
    }
    /* tick_t wraps: compare the signed distance */
    while ((int32_t)(deadline - tick_now()) > 0) {
        /* both may change after handling events: read them at each round */       
        sigs = Wait(vblank_data->sigmask |
                        currentUIWaitBit |
                        mui_sigs |
                        SIGBREAKF_CTRL_C
                        );
        if (sigs & currentUIWaitBit) {
            amiga_video_handle_events();
        }
        if (sigs & mui_sigs) {
            amiga_mui_handle_events();
        }
        amiga_check_ctrl_c(sigs);
    }
}

/** \brief  libnix CTRL-C check, replaced by an empty one
 *
 * libnix calls it in every read() and write() (printf, log lines): it would
 * eat the CTRL-C signal and raise(SIGINT), an exit without the VICE
 * shutdown. CTRL-C is handled by the main loop instead (see below).
 */
void __chkabort(void)
{
}

/** \brief  Per frame UI polling, for when the emulation never waits
 *
 * When nothing happened: one GetMsg() on the emulator window port (empty),
 * one SetSignal() for CTRL-C and MUI. A too slow emulation never sleeps in
 * Wait(): CTRL-C must be seen here too.
 */
void amiga_wait_poll_events(void)
{
    ULONG sigs;

    amiga_video_handle_events();

    /* clears CTRL-C, returns the signals as they were */
    sigs = SetSignal(0, SIGBREAKF_CTRL_C);
    amiga_check_ctrl_c(sigs);

    if ((sigs & mui_sigs) != 0) {
        amiga_mui_handle_events();
    }
}

/** \brief  Block until UI events arrive and handle them (pause loop)
 *
 * No CPU is used while nothing happens.
 */
void amiga_wait_events(void)
{
    ULONG wait_mask = currentUIWaitBit | mui_sigs | SIGBREAKF_CTRL_C;
    ULONG sigs;

    if (currentUIWaitBit == 0 && vblank_data != NULL) {
        /* no window: do not wait forever */
        wait_mask |= vblank_data->sigmask;
    }
    sigs = Wait(wait_mask);
    if (sigs & currentUIWaitBit) {
        amiga_video_handle_events();
    }
    if (sigs & mui_sigs) {
        amiga_mui_handle_events();
    }
    amiga_check_ctrl_c(sigs);
}
