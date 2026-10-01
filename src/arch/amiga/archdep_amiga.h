/** \file   archdep_amiga.h
 * \brief   Miscellaneous AmigaOS 3.x (m68k) specific stuff
 *
 * Modeled after archdep_unix.h, values taken from the VICE 3.2
 * src/arch/amigaos/archdep.h port.
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

#ifndef VICE_ARCHDEP_AMIGA_H
#define VICE_ARCHDEP_AMIGA_H

#include "vice.h"

#define VICE_ARCHAPI_PRIVATE_API
#include "archapi.h"
#undef VICE_ARCHAPI_PRIVATE_API

#define ARCHDEP_FSDEVICE_DEFAULT_DIR "" /**< current dir ("." is not valid on AmigaDOS) */
#define ARCHDEP_DIR_SEP_STR "/"     /**< directory separator as a string */
#define ARCHDEP_DIR_SEP_CHR '/'     /**< directory separator as an integer */

/* ':' is the AmigaDOS volume separator, so path lists use ';' */
#define ARCHDEP_FINDPATH_SEPARATOR_CHAR   ';'

#define MODE_READ              "r"  /**< read mode (binary) */
#define MODE_READ_TEXT         "r"  /**< read mode (text) */
#define MODE_READ_WRITE        "r+" /**< read/write mode */
#define MODE_WRITE             "w"  /**< write mode (binary) */
#define MODE_WRITE_TEXT        "w"  /**< write mode (text) */
#define MODE_APPEND            "a"  /**< append mode */
#define MODE_APPEND_READ_WRITE "a+" /**< append mode and read/write(?) */

#define ARCHDEP_PRINTER_DEFAULT_DEV1 "print.dump"
#define ARCHDEP_PRINTER_DEFAULT_DEV2 "PRT:"
#define ARCHDEP_PRINTER_DEFAULT_DEV3 "PAR:"

#define ARCHDEP_RS232_DEV1 "SER:"
#define ARCHDEP_RS232_DEV2 "SER:"
#define ARCHDEP_RS232_DEV3 "127.0.0.1:25232"
#define ARCHDEP_RS232_DEV4 "SER:"

#define ARCHDEP_MIDI_IN_DEV  "midi"
#define ARCHDEP_MIDI_OUT_DEV "midi"

#define ARCHDEP_RAWDRIVE_DEFAULT "DF0:"

#define ARCHDEP_LINE_DELIMITER "\n"

#define ARCHDEP_ETHERNET_DEFAULT_DEVICE "eth0"

void archdep_signals_init(int do_core_dumps);
void archdep_signals_pipe_set(void);
void archdep_signals_pipe_unset(void);

#define ARCHDEP_MAKE_SO_NAME_VERSION(n, v) #n ".library"

#define ARCHDEP_OPENCBM_SO_NAME  "opencbm.library"
#define ARCHDEP_LAME_SO_NAME     "lame.library"

#define ARCHDEP_SOCKET_ERROR errno

#endif
