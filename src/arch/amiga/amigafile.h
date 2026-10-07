/** \file   amigafile.h
 * \brief   AmigaOS 3.x port: ASL file requester and dropped Workbench icons
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

#ifndef VICE_AMIGAFILE_H
#define VICE_AMIGAFILE_H

#include <exec/types.h>
#include <intuition/intuition.h>
#include <workbench/startup.h>

/* ASL file requester on the screen of \a window (may be NULL).
 * Returns a lib_malloc'd full path to free with lib_free(), or NULL. */
char *amiga_file_request(struct Window *window, const char *title, const char *pattern);
/* the same in save mode (a new file name can be typed) */
char *amiga_file_save_request(struct Window *window, const char *title, const char *pattern);

/* ASL drawer requester, starting in \a initial (may be NULL or "").
 * Returns a lib_malloc'd drawer path to free with lib_free(), or NULL. */
char *amiga_drawer_request(struct Window *window, const char *title, const char *initial);

/* full path of a Workbench argument (dropped icon), lib_free() it */
char *amiga_wbarg_path(const struct WBArg *arg);

#endif
