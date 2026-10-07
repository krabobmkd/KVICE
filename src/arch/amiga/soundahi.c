/** \file   soundahi.c
 * \brief   AmigaOS 3.x AHI sound output
 *
 * VICE writes samples into a ring buffer, a separate AHI process drains it
 * with double buffered, linked CMD_WRITE requests (same scheme as the
 * AmigaMame AHI stream). sound.c only writes what bufferspace() allows and
 * sleeps meanwhile.
 *
 * Not a timing source: vsync keeps sleeping on the E-clock. When VICE does
 * not sleep, a busy main task can starve the AHI playback task (it happened
 * at startup, AHI then never played again).
 *
 * The emulation must never hang on the sound: the AHI process can always be
 * stopped with CTRL-C (it never blocks in WaitIO()), and if AHI stops
 * consuming samples the driver drops them instead of making sound.c wait.
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

#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <devices/ahi.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "amigatrace.h"
#include "archdep_tick.h"
#include "log.h"
#include "sound.h"

/* AHI does not like short requests: linked requests shorter than its
 * mixing buffer stopped completing (the 2nd request never came back with
 * 11 ms requests). Like AmigaMame: one request per 2 video frames (40 ms). */
#define AHI_MIN_CHUNK_FRAMES 256
#define AHI_CHUNKS_PER_SECOND 25

/* silent requests sent first, like AmigaMame, while VICE fills the ring */
#define AHI_SILENT_REQUESTS 2

/* no AHI request completed for that long: samples are dropped until AHI
 * plays again */
#define AHI_STALL_TICKS (3 * tick_per_second())

typedef enum {
    AHIS_NOT_STARTED = 0,
    AHIS_STARTING,
    AHIS_RUNNING,
    AHIS_ERROR
} ahi_state_t;

/* everything shared between the main process and the AHI process */
typedef struct ahi_server_s {
    struct Process *main_process;
    struct Process *process;
    volatile ULONG ask_death;
    volatile ahi_state_t state;
    volatile int alive;         /* the AHI process exists */
    int stalled;                /* AHI stopped playing: samples are dropped */

    ULONG freq;
    int channels;
    ULONG chunk_frames;         /* frames per AHI request */

    /* ring buffer of int16 samples, one slot always kept free */
    int16_t *ring;
    ULONG ring_len;             /* in int16 */
    volatile ULONG inptr;       /* written by the main process only */
    volatile ULONG outptr;      /* written by the AHI process only */

    /* statistics, written by the AHI process */
    volatile ULONG requests_done;
    volatile ULONG underruns;
    volatile LONG last_io_error;    /* io_Error of the last failed request */
    volatile ULONG io_errors;

    /* AHI process side: two requests, played one after the other */
    struct MsgPort *port;
    struct AHIRequest *req[2];
    int pending[2];
    int16_t *buf_alloc;
    int16_t *buf[2];
    int16_t last_sample[2];
} ahi_server_t;

static ahi_server_t *ahis = NULL;
static log_t ahi_log = LOG_DEFAULT;
static int atexit_registered = 0;

/* stall watchdog, main process side */
static ULONG watch_requests = 0;
static tick_t watch_tick = 0;

/* ------------------------------------------------------------------------- */
/* ring buffer */

static ULONG ring_used(const ahi_server_t *s)
{
    ULONG in = s->inptr;
    ULONG out = s->outptr;

    return (in >= out) ? (in - out) : (s->ring_len - out + in);
}

static ULONG ring_free(const ahi_server_t *s)
{
    return s->ring_len - 1 - ring_used(s);
}

/* ------------------------------------------------------------------------- */
/* AHI process */

static void ahi_process_close(ahi_server_t *s)
{
    int i;

    /* in-flight requests must be finished before closing the device */
    for (i = 0; i < 2; i++) {
        if (s->req[i] != NULL && s->pending[i]) {
            if (!CheckIO((struct IORequest *)s->req[i])) {
                AbortIO((struct IORequest *)s->req[i]);
            }
            WaitIO((struct IORequest *)s->req[i]);
            s->pending[i] = 0;
        }
    }
    if (s->req[1] != NULL) {
        DeleteIORequest((struct IORequest *)s->req[1]);
        s->req[1] = NULL;
    }
    if (s->req[0] != NULL) {
        if (s->req[0]->ahir_Std.io_Device != NULL) {
            CloseDevice((struct IORequest *)s->req[0]);
        }
        DeleteIORequest((struct IORequest *)s->req[0]);
        s->req[0] = NULL;
    }
    if (s->port != NULL) {
        DeleteMsgPort(s->port);
        s->port = NULL;
    }
    if (s->buf_alloc != NULL) {
        FreeVec(s->buf_alloc);
        s->buf_alloc = NULL;
    }
}

/* ports and requests must belong to the process that uses them */
static int ahi_process_open(ahi_server_t *s)
{
    ULONG chunk_len = s->chunk_frames * s->channels;

    s->port = CreateMsgPort();
    if (s->port == NULL) {
        return -1;
    }
    s->req[0] = (struct AHIRequest *)CreateIORequest(s->port, sizeof(struct AHIRequest));
    if (s->req[0] == NULL) {
        return -1;
    }
    s->req[0]->ahir_Version = 4;
    if (OpenDevice((CONST_STRPTR)AHINAME, AHI_DEFAULT_UNIT, (struct IORequest *)s->req[0], 0) != 0) {
        s->req[0]->ahir_Std.io_Device = NULL;
        return -1;
    }
    s->req[1] = (struct AHIRequest *)CreateIORequest(s->port, sizeof(struct AHIRequest));
    if (s->req[1] == NULL) {
        return -1;
    }
    CopyMem(s->req[0], s->req[1], sizeof(struct AHIRequest));

    s->buf_alloc = AllocVec(chunk_len * 2 * sizeof(int16_t), MEMF_PUBLIC | MEMF_CLEAR);
    if (s->buf_alloc == NULL) {
        return -1;
    }
    s->buf[0] = s->buf_alloc;
    s->buf[1] = s->buf_alloc + chunk_len;
    return 0;
}

/* copy one chunk from the ring, repeat the last sample on underrun */
static void ahi_fill_chunk(ahi_server_t *s, int16_t *dst)
{
    ULONG want = s->chunk_frames * s->channels;
    ULONG avail = ring_used(s);
    ULONG n = (avail < want) ? avail : want;
    ULONG out = s->outptr;
    ULONG i;
    int c;

    /* whole frames only */
    n -= n % s->channels;
    for (i = 0; i < n; i++) {
        dst[i] = s->ring[out];
        if (++out == s->ring_len) {
            out = 0;
        }
    }
    s->outptr = out;

    if (n >= (ULONG)s->channels) {
        for (c = 0; c < s->channels; c++) {
            s->last_sample[c] = dst[n - s->channels + c];
        }
    }
    if (n < want) {
        s->underruns++;
    }
    for (; i < want; i += s->channels) {
        for (c = 0; c < s->channels; c++) {
            dst[i + c] = s->last_sample[c];
        }
    }
}

/* wait for a request without blocking CTRL-C, returns 0 if CTRL-C came */
static int ahi_wait_request(ahi_server_t *s, int i)
{
    ULONG port_mask = 1UL << s->port->mp_SigBit;

    while (!CheckIO((struct IORequest *)s->req[i])) {
        if (Wait(port_mask | SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
            return 0;
        }
    }
    WaitIO((struct IORequest *)s->req[i]);
    s->pending[i] = 0;
    if (s->req[i]->ahir_Std.io_Error != 0) {
        s->last_io_error = s->req[i]->ahir_Std.io_Error;
        s->io_errors++;
    }
    return 1;
}

/* OS3 CreateNewProc() entry: no arguments, uses the global server */
static void ahi_process_entry(void)
{
    ahi_server_t *s = ahis;
    struct AHIRequest *io;
    int cur = 0;
    int prev = -1;
    int silent = AHI_SILENT_REQUESTS;

    if (ahi_process_open(s) != 0) {
        ahi_process_close(s);
        s->state = AHIS_ERROR;
        /* as at the end: gone before the main process frees or unloads */
        Forbid();
        Signal((struct Task *)s->main_process, SIGF_SINGLE);
        return;
    }
    s->alive = 1;
    s->state = AHIS_RUNNING;
    Signal((struct Task *)s->main_process, SIGF_SINGLE);

    while (!s->ask_death) {
        io = s->req[cur];

        if (silent > 0) {
            memset(s->buf[cur], 0, s->chunk_frames * s->channels * sizeof(int16_t));
            silent--;
        } else {
            ahi_fill_chunk(s, s->buf[cur]);
        }

        io->ahir_Std.io_Message.mn_Node.ln_Pri = 127;
        io->ahir_Std.io_Command = CMD_WRITE;
        io->ahir_Std.io_Data = s->buf[cur];
        io->ahir_Std.io_Length = s->chunk_frames * s->channels * sizeof(int16_t);
        io->ahir_Std.io_Offset = 0;
        io->ahir_Version = 4;
        io->ahir_Frequency = s->freq;
        io->ahir_Type = (s->channels == 2) ? AHIST_S16S : AHIST_M16S;
        io->ahir_Volume = 0x10000;
        io->ahir_Position = 0x8000;
        /* played right after the previous one */
        io->ahir_Link = (prev >= 0 && s->pending[prev]) ? s->req[prev] : NULL;
        SendIO((struct IORequest *)io);
        s->pending[cur] = 1;

        /* the new request is queued: wait for the previous one to end */
        if (prev >= 0 && s->pending[prev]) {
            if (!ahi_wait_request(s, prev)) {
                break;
            }
            s->requests_done++;
        }
        prev = cur;
        cur ^= 1;
    }

    ahi_process_close(s);
    s->state = AHIS_NOT_STARTED;
    s->alive = 0;

    /* Forbid() so the main process cannot unload this code before we are
     * completely gone: the process ends and Forbid() is broken by RemTask() */
    Forbid();
    Signal((struct Task *)s->main_process, SIGF_SINGLE);
}

/* ------------------------------------------------------------------------- */
/* main process side */

/** \brief  Stop the AHI process and free everything, safe to call twice
 */
static void ahi_shutdown(void)
{
    ULONG old_signals;

    if (ahis == NULL) {
        return;
    }
    if (ahis->alive) {
        AMIGA_TRACE(("stopping AHI process, %lu requests played, %lu underruns, %lu errors (last %ld)",
                     (unsigned long)ahis->requests_done, (unsigned long)ahis->underruns,
                     (unsigned long)ahis->io_errors, (long)ahis->last_io_error));
        old_signals = SetSignal(0L, SIGF_SINGLE);
        ahis->ask_death = 1;
        /* wakes it up even if AHI does not complete requests anymore */
        Signal((struct Task *)ahis->process, SIGBREAKF_CTRL_C);
        Wait(SIGF_SINGLE);
        SetSignal(old_signals, old_signals);
        AMIGA_TRACE(("AHI process stopped"));
    }
    if (ahis->ring != NULL) {
        FreeVec(ahis->ring);
    }
    FreeVec(ahis);
    ahis = NULL;
}

static int ahi_init(const char *param, int *speed,
                    int *fragsize, int *fragnr, int *channels)
{
    ULONG old_signals;
    ULONG ring_frames;
    struct TagItem tags[] = {
        { NP_Entry, (ULONG)ahi_process_entry },
        { NP_Priority, 60 },
        { NP_Name, (ULONG)"VICE AHI" },
        { NP_StackSize, 16384 },
        { NP_Output, 0 },
        { NP_CloseOutput, FALSE },
        { TAG_DONE, 0 }
    };

    ahi_log = log_open("AHI");
    if (!atexit_registered) {
        atexit(ahi_shutdown);
        atexit_registered = 1;
    }
    ahi_shutdown();

    if (*channels < 1 || *channels > 2) {
        *channels = 1;
    }

    ahis = AllocVec(sizeof(ahi_server_t), MEMF_PUBLIC | MEMF_CLEAR);
    if (ahis == NULL) {
        return 1;
    }
    ahis->main_process = (struct Process *)FindTask(NULL);
    ahis->freq = (ULONG)*speed;
    ahis->channels = *channels;
    /* independent of the VICE fragment size, which is only a few ms */
    ahis->chunk_frames = ((ahis->freq / AHI_CHUNKS_PER_SECOND) + 3) & ~3UL;
    if (ahis->chunk_frames < AHI_MIN_CHUNK_FRAMES) {
        ahis->chunk_frames = AHI_MIN_CHUNK_FRAMES;
    }
    /* room for the VICE buffer, and never less than 3 AHI chunks: one
     * playing, one queued, one being filled by VICE */
    ring_frames = (ULONG)(*fragsize * *fragnr);
    if (ring_frames < ahis->chunk_frames * 3) {
        ring_frames = ahis->chunk_frames * 3;
    }
    ahis->ring_len = ring_frames * ahis->channels + 1;
    ahis->ring = AllocVec(ahis->ring_len * sizeof(int16_t), MEMF_PUBLIC | MEMF_CLEAR);
    if (ahis->ring == NULL) {
        ahi_shutdown();
        return 1;
    }

    ahis->state = AHIS_STARTING;
    tags[4].ti_Data = (ULONG)ahis->main_process->pr_COS;
    old_signals = SetSignal(0L, SIGF_SINGLE);
    ahis->process = CreateNewProc(tags);
    if (ahis->process == NULL) {
        SetSignal(old_signals, old_signals);
        log_error(ahi_log, "cannot create the AHI process.");
        ahis->state = AHIS_ERROR;
        ahi_shutdown();
        return 1;
    }
    /* wait for the AHI process to open ahi.device */
    Wait(SIGF_SINGLE);
    SetSignal(old_signals, old_signals);

    if (ahis->state != AHIS_RUNNING) {
        log_error(ahi_log, "cannot open ahi.device unit 0, is AHI installed?");
        ahi_shutdown();
        return 1;
    }

    watch_requests = ahis->requests_done;
    watch_tick = tick_now();

    log_message(ahi_log, "%lu Hz, %d channel(s), %lu frames per AHI request, %lu frames buffer.",
                (unsigned long)ahis->freq, ahis->channels,
                (unsigned long)ahis->chunk_frames, (unsigned long)ring_frames);
    return 0;
}

/* nr is a number of int16, sound.c never writes more than bufferspace() */
static int ahi_write(int16_t *pbuf, size_t nr)
{
    ULONG in, i, space;

    if (ahis == NULL || ahis->state != AHIS_RUNNING || ahis->stalled) {
        return 0;
    }
    space = ring_free(ahis);
    if (nr > space) {
        /* only after a stall: drop what does not fit */
        nr = space;
    }
    in = ahis->inptr;
    for (i = 0; i < nr; i++) {
        ahis->ring[in] = pbuf[i];
        if (++in == ahis->ring_len) {
            in = 0;
        }
    }
    ahis->inptr = in;
    return 0;
}

/* free space in frames */
static int ahi_bufferspace(void)
{
    ULONG free_frames;
    tick_t now;

    if (ahis == NULL || ahis->state != AHIS_RUNNING) {
        /* no output: never make sound.c wait for us */
        return 0x7fffffff;
    }

    /* stall watchdog: AHI completed no request for too long. Requests
     * complete even when the ring is empty (underruns), so this only
     * triggers when AHI itself does not play. */
    now = tick_now();
    if (ahis->requests_done != watch_requests) {
        watch_requests = ahis->requests_done;
        watch_tick = now;
        if (ahis->stalled) {
            log_message(ahi_log, "AHI plays again.");
            ahis->stalled = 0;
        }
    } else if (!ahis->stalled && now - watch_tick > AHI_STALL_TICKS) {
        log_error(ahi_log, "AHI does not play (%lu requests played, %lu errors, last io_Error %ld), dropping sound.",
                  (unsigned long)ahis->requests_done, (unsigned long)ahis->io_errors,
                  (long)ahis->last_io_error);
        ahis->stalled = 1;
    }
    if (ahis->stalled) {
        return 0x7fffffff;
    }
    free_frames = ring_free(ahis) / (ULONG)ahis->channels;
    return (int)free_frames;
}

static void ahi_close(void)
{
    ahi_shutdown();
}

static int ahi_suspend(void)
{
    return 0;
}

static int ahi_resume(void)
{
    return 0;
}

static const sound_device_t ahi_device =
{
    "ahi",
    ahi_init,
    ahi_write,
    NULL,
    NULL,
    ahi_bufferspace,
    ahi_close,
    ahi_suspend,
    ahi_resume,
    1,
    2,
    false       /* not a timing source, see the top of this file */
};

int sound_init_ahi_device(void)
{
    return sound_register_device(&ahi_device);
}

/** \brief  Statistics for the heartbeat trace, 0 when AHI is not used
 *
 * \param[out]  requests    AHI requests played
 * \param[out]  underruns   requests padded because the emulation was late
 * \param[out]  used_frames frames waiting in the ring buffer
 *
 * \return  1 if the AHI output runs
 */
int amiga_ahi_stats(unsigned long *requests, unsigned long *underruns,
                    unsigned long *used_frames)
{
    if (ahis == NULL || ahis->state != AHIS_RUNNING) {
        *requests = *underruns = *used_frames = 0;
        return 0;
    }
    *requests = ahis->requests_done;
    *underruns = ahis->underruns;
    *used_frames = ring_used(ahis) / (ULONG)ahis->channels;
    return !ahis->stalled;
}
