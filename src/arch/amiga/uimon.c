/** \file   uimon.c
 * \brief   Amiga UI monitor stuff: the MUI monitor window (amigamonitor.c)
 *
 * \author  Fabrizio Gennari <fabrizio.ge@tiscali.it>
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

#include <stdio.h>

#include "amigamonitor.h"
#include "console.h"
#include "log.h"
#include "uimon.h"


console_t *uimon_window_open(bool display_now)
{
    /* printf("%s\n", __func__); */

    return uimon_window_resume();
}

console_t *uimon_window_resume(void)
{
    /* printf("%s\n", __func__); */

    /* the window stays open while the emulation runs */
    static console_t console_log = { 80, 25, 1, 0, NULL };

    return &console_log;
}

void uimon_window_suspend(void)
{
    /* the emulation goes on: the window shows it */
    amiga_monitor_refresh();
}

int uimon_out(const char *buffer)
{
    amiga_monitor_out(buffer);

    return 0;
}

int uimon_petscii_out(const char *buffer, int num)
{
    /* printf("%s\n", __func__); */

    log_message(LOG_DEFAULT, "Monitor (PETSCII): %s", buffer);

    return 0;
}

int uimon_petscii_upper_out(const char *buffer, int num)
{
    /* printf("%s\n", __func__); */

    log_message(LOG_DEFAULT, "Monitor (PETSCII/Upper): %s", buffer);

    return 0;
}

int uimon_scrcode_out(const char *buffer, int num)
{
    /* printf("%s\n", __func__); */

    log_message(LOG_DEFAULT, "Monitor (SCRCODE): %s", buffer);

    return 0;
}

int uimon_scrcode_upper_out(const char *buffer, int num)
{
    /* printf("%s\n", __func__); */

    log_message(LOG_DEFAULT, "Monitor (SCRCODE): %s", buffer);

    return 0;
}

void uimon_window_close(void)
{
    /* kept open, as when suspended */
    amiga_monitor_refresh();
}

void uimon_notify_change(void)
{
    /* printf("%s\n", __func__); */
}

void uimon_set_interface(struct monitor_interface_s **interf, int i)
{
    /* printf("%s\n", __func__); */
}

char *uimon_get_in(char **ppchCommandLine, const char *prompt)
{
    /* from the buttons of the monitor window */
    return amiga_monitor_get_command();
}
