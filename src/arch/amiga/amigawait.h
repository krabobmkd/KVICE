/** \file   amigawait.h
 * \brief   AmigaOS 3.x main loop waiting: VBlank signal + UI events
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

#ifndef VICE_AMIGAWAIT_H
#define VICE_AMIGAWAIT_H

#include <exec/types.h>

#include "archdep_tick.h"

int amiga_wait_init(void);
void amiga_wait_shutdown(void);
/* emulation wait loop */
void amiga_wait_until(tick_t deadline);
/* pause loop */
void amiga_wait_events(void);
void amiga_wait_poll_events(void);
ULONG amiga_wait_vblank_count(void);

#endif
