/** \file   timestats.h
 * \brief   Measurement build: host time spent in each part of the emulation
 *
 * Built with VICE_AMIGA_TIME_STATS (cmake option of the Amiga port), the
 * time between TIMESTATS_ENTER() and TIMESTATS_LEAVE() is counted for the
 * given part, nested parts excluded: what is not in a part is the CPU
 * emulation and the rest. A report is printed every 5 seconds
 * (arch/amiga/amiga_timestats.c). Without the option, the macros are
 * empty.
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

#ifndef VICE_TIMESTATS_H
#define VICE_TIMESTATS_H

/* the measured parts, TSTAT_CPU counts everything else */
enum {
    TSTAT_CPU = 0,     /* 6510 emulation and everything not below */
    TSTAT_VIC_LINE,    /* VIC-II: per line alarm, drawing excluded */
    TSTAT_VIC_DRAW,    /* VIC-II: raster_line_emulate(), parts below excluded */
    TSTAT_VIC_GFX,     /* VIC-II: graphics mode drawing (text, bitmap...) */
    TSTAT_VIC_SPRITES, /* VIC-II: sprite drawing and collisions */
    TSTAT_VIC_FETCH,   /* VIC-II: fetch alarm (bad lines, sprite DMA) */
    TSTAT_IO,          /* $d000-$dfff register access (VIC-II, SID, I/O 1/2) */
    TSTAT_CIA,         /* CIA register access and timer/TOD/SDR alarms */
    TSTAT_SCREEN,      /* host screen output (video_canvas_refresh) */
    TSTAT_SID,         /* SID sample generation */
    TSTAT_SOUND,       /* sound mixing and output, SID excluded */
    TSTAT_VSYNC,       /* end of line/frame work: sync, input, UI events */
    TSTAT_WAIT,        /* sleeping in vsync, waiting for the speed limit */
    TSTAT_COUNT
};

#ifdef VICE_AMIGA_TIME_STATS
void amiga_timestats_enter(int part);
void amiga_timestats_leave(void);
void amiga_timestats_frame(void);
# define TIMESTATS_ENTER(part) amiga_timestats_enter(part)
# define TIMESTATS_LEAVE()     amiga_timestats_leave()
# define TIMESTATS_FRAME()     amiga_timestats_frame()
#else
# define TIMESTATS_ENTER(part)
# define TIMESTATS_LEAVE()
# define TIMESTATS_FRAME()
#endif

#endif
