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
#include <proto/intuition.h>
#include <exec/tasks.h>
#include <intuition/intuition.h>
#include <workbench/startup.h>

#include "amigafile.h"
#include "amigamachine.h"

/* set by the libnix startup code when started from Workbench */
extern struct WBStartup *_WBenchMsg;

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
/* Started from Workbench, libnix calls main() with argc 0 and argv being
 * the WBStartup message: VICE needs an argv[0]. The program name, then the
 * project icons given with it (shift-click, or a project icon whose default
 * tool is this emulator) as full paths: VICE autostarts a file argument. */
#define WB_ARGS_MAX 8

static char *wb_argv[WB_ARGS_MAX + 1];

static int workbench_args(char ***argv)
{
    struct WBStartup *msg = _WBenchMsg;
    int argc = 0;
    LONG i;

    wb_argv[argc++] = (char *)((msg != NULL && msg->sm_NumArgs > 0)
                               ? msg->sm_ArgList[0].wa_Name : (BYTE *)amiga_machine.window_title);
    for (i = 1; msg != NULL && i < msg->sm_NumArgs && argc < WB_ARGS_MAX; i++) {
        char *path = amiga_wbarg_path(&msg->sm_ArgList[i]);

        if (path != NULL) {
            wb_argv[argc++] = path;
        }
    }
    wb_argv[argc] = NULL;
    *argv = wb_argv;
    return argc;
}

/* an error before VICE runs: a requester under Workbench (no console) */
static void startup_error(int from_workbench, const char *text)
{
    if (from_workbench) {
        struct EasyStruct es;

        es.es_StructSize = sizeof es;
        es.es_Flags = 0;
        es.es_Title = (UBYTE *)amiga_machine.title;
        es.es_TextFormat = (UBYTE *)"%s";
        es.es_GadgetFormat = (UBYTE *)"OK";
        EasyRequest(NULL, &es, NULL, (ULONG)text);
    } else {
        printf("%s\n", text);
    }
}

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
    int from_workbench = (argc == 0);

#ifdef VICE_AMIGA_TRACE
    /* registered first, so called last: the whole atexit() chain went fine */
    atexit(amiga_trace_atexit_end);
#endif
    if (stacksize < VICE_AMIGA_MIN_STACK) {
        char text[160];

        snprintf(text, sizeof text,
                 "%s needs at least %dK of stack (has %d).\n"
                 "Use \"stack 32768\", or set it in the icon.",
                 amiga_machine.window_title, VICE_AMIGA_MIN_STACK / 1024, stacksize);
        startup_error(from_workbench, text);
        return 1;
    }
    if (from_workbench) {
        argc = workbench_args(&argv);
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
