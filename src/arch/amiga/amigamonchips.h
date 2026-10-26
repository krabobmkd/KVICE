/** \file   amigamonchips.h
 * \brief   AmigaOS 3.x port: video and sound chip lists of the monitor window
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

#ifndef VICE_AMIGAMONCHIPS_H
#define VICE_AMIGAMONCHIPS_H

/* a row of a chip list: what it is, its register address(es) or "" when
   it is computed from other registers or from the emulation, its value */
typedef struct amiga_chip_row_s {
    char name[16];
    char addr[12];
    char value[44];
} amiga_chip_row_t;

/* chip names of the machine: "VIC-II" and "SID", "TED"... */
const char *amiga_chips_video_title(void);
const char *amiga_chips_sound_title(void);
/* fill up to max rows, return their number */
int amiga_chips_video_rows(amiga_chip_row_t *rows, int max);
int amiga_chips_sound_rows(amiga_chip_row_t *rows, int max);

#endif
