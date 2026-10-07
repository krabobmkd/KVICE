/** \file   main.c
 * \brief   Headless UI startup
 *
 * \author  David Hogan <david.q.hogan@gmail.com>
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
#include <stdlib.h>
#include <signal.h>

#include "log.h"
#include "machine.h"
#include "main.h"
#include "video.h"

#include <proto/exec.h>
#include <exec/tasks.h>

//thank you:
//thank you #define STACK_WATCH 1

/* VICE keeps ARCHDEP_PATH_MAX (4KB) buffers on the stack in many places.
 * The 3.2 port declared "__stack = 512K", but with bebbo's libnix that
 * variable alone does not swap the stack, so check it like EmojiGear does.
 * Measure the real use with STACK_WATCH (cmake -DVICE_AMIGA_STACK_WATCH=ON)
 * in a typical session, then set this to that size + some KB.
 * krb: after some uses and measure total stack use in less than 8k.
 * We'll consider 28k "in case of", but not more.
 */
#define VICE_AMIGA_MIN_STACK (26 * 1024)

#ifdef STACK_WATCH
/* the free stack is filled with this at startup: at exit, the first word
 * that changed (from the stack bottom) is the deepest point reached */
#define STACK_WATCH_PATTERN 0xCAFEBABEUL

static void stack_watch_fill(struct Task *task)
{
    ULONG anchor = 0;
    /* 64 bytes under the current frame are left as they are */
    ULONG *near = (ULONG *)((ULONG)&anchor - 64);
    ULONG *p = (ULONG *)((ULONG)task->tc_SPLower + 4);

    while (p < near) {
        *p++ = STACK_WATCH_PATTERN;
    }
}

/* atexit(): registered first, so called last, after VICE shut down. VICE
 * quits with exit() from inside the emulation, main() does not return. */
static void stack_watch_report(void)
{
    struct Task *task = FindTask(NULL);
    ULONG *p = (ULONG *)((ULONG)task->tc_SPLower + 4);

    while (p < (ULONG *)task->tc_SPUpper && *p == STACK_WATCH_PATTERN) {
        p++;
    }
    printf("**** STACK_WATCH: total=%ld  real use=%ld\n",
           (long)((ULONG)task->tc_SPUpper - (ULONG)task->tc_SPLower),
           (long)((ULONG)task->tc_SPUpper - (ULONG)p));
}
#endif


/** \brief  Program driver
 *
 * \param[in]   argc    argument count
 * \param[in]   argv    argument vector
 *
 * \return  0 on success, any other value on failure
 *
 * \note    This should return either EXIT_SUCCESS on success or EXIT_FAILURE
 *          on failure. Unfortunately, there are a lot of exit(1)/exit(-1)
 *          calls, so don't expect to get a meaningful exit status.
 */
#ifdef VICE_AMIGA_TRACE
static void amiga_trace_atexit_end(void)
{
    AMIGA_TRACE(("last atexit() handler, back to libnix exit code"));
}
#endif

int main(int argc, char **argv)
{
    struct Task *task = FindTask(NULL);
    int stacksize = (int)task->tc_SPUpper - (int)task->tc_SPLower;

#ifdef VICE_AMIGA_TRACE
    /* registered first, so called last: the whole atexit() chain went fine */
    atexit(amiga_trace_atexit_end);
#endif
    if (stacksize < VICE_AMIGA_MIN_STACK) {
        printf("x64 needs at least %dk stack (has %d). Use \"stack 32768\" or set it in the icon.\n",
               VICE_AMIGA_MIN_STACK / 1024, stacksize);
        return 1;
    }
#ifdef STACK_WATCH
    /* first atexit() handler of VICE: reports after all the others */
    atexit(stack_watch_report);
    stack_watch_fill(task);
#endif

    return main_program(argc, argv);
}


/** \brief  Exit handler
 */
void main_exit(void)
{
    AMIGA_TRACE(("%s", __func__));

    log_message(LOG_DEFAULT, "\nExiting...");

    machine_shutdown();
    AMIGA_TRACE(("machine_shutdown done, calling exit()"));
}
