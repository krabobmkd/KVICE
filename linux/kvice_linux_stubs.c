/** \file   kvice_linux_stubs.c
 * \brief   What the KVICE Linux build leaves out, as in the Amiga build
 *
 * - the network code (arch/shared/socketdrv) and the ffmpeg video export,
 *   which needs it
 * - linenoise-ng (C++), used by the Unix monitor console: the console
 *   gets no input line, the emulation is not concerned
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

#include <stddef.h>

#include "archdep_network.h"
#include "ffmpegexedrv.h"
#include "lib/linenoise-ng/linenoise.h"

/* arch/shared/socketdrv/socketdrv.c */
void archdep_network_shutdown(void)
{
}

/* gfxoutputdrv/ffmpegexedrv.c */
void gfxoutput_init_ffmpegexe(int help)
{
}

/* lib/linenoise-ng */
char *linenoise(const char *prompt)
{
    return NULL;
}

int linenoiseHistoryAdd(const char *line)
{
    return 0;
}

int linenoiseHistorySetMaxLen(int len)
{
    return 0;
}

void linenoiseHistoryFree(void)
{
}
