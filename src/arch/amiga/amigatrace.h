/** \file   amigatrace.h
 * \brief   Startup traces for the AmigaOS 3.x port
 *
 * Writes straight to stdout and flushes, bypassing the VICE log system
 * (which is not usable before log_init(), and filters debug messages).
 * Enabled with the VICE_AMIGA_TRACE cmake option.
 *
 * Usage: AMIGA_TRACE(("video_init() returned %d", ret));
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

#ifndef VICE_AMIGATRACE_H
#define VICE_AMIGATRACE_H

#ifdef VICE_AMIGA_TRACE

#include <stdio.h>

/* double parenthesis like the VICE DBG() macros: AMIGA_TRACE(("x=%d", x)) */
# define AMIGA_TRACE(args) \
    do { amiga_trace_at(__FILE__, __LINE__); amiga_trace_msg args; } while (0)

void amiga_trace_at(const char *file, int line);
void amiga_trace_msg(const char *format, ...);

#else
# define AMIGA_TRACE(args)
#endif

#endif
