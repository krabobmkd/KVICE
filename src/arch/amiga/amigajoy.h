/** \file   amigajoy.h
 * \brief   AmigaOS 3.x joystick driver, resources and command line
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

#ifndef VICE_AMIGAJOY_H
#define VICE_AMIGAJOY_H

int amiga_joy_resources_init(void);
int amiga_joy_cmdline_options_init(void);

/* Amiga ports (lowlevel numbering: 0 mouse port, 1 joystick port) */
void amiga_joy_reconfigure(void);
int amiga_joy_device_index(int port);
int amiga_joy_device_port(int index);
int amiga_joy_port_is_analog(int port);

#endif
