/** \file   amigaaction.c
 * \brief   AmigaOS 3.x port: table-driven UI actions (menus, keys)
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

#include "amigaaction.h"
#include "amigalocale.h"
#include "amigamui.h"
#include "amigatrace.h"
#include "amigafile.h"
#include "amigavideo.h"
#include "attach.h"
#include "autostart.h"
#include "cartridge.h"
#include "lib.h"
#include "machine.h"
#include "resources.h"
#include "vsync.h"
#include "archdep_exit.h"
#include "log.h"
#include "ui.h"

/* Display settings, drafted: window resizing is not implemented yet.
 * TODO: turn them into VICE resources so they are saved in vicerc. */


/* ------------------------------------------------------------------------- */
/* C64 menu */

/* file patterns for the requesters */
#define PATTERN_AUTOSTART "#?.(prg|p00|d64|d71|d81|g64|g71|x64|t64|tap|crt|zip|gz)"
#define PATTERN_DISK      "#?.(d64|d71|d81|g64|g71|x64|p64|zip|gz)"
#define PATTERN_CART      "#?.(crt|bin)"

/* ask a file name: the emulation is frozen meanwhile */
static char *request_file(ULONG title_msg, const char *pattern)
{
    char *path = amiga_file_request(amiga_video_window(), LOC(title_msg), pattern);

    /* the time spent in the requester must not count as emulation lag */
    vsync_suspend_speed_eval();
    return path;
}

BOOL amiga_autostart_file(const char *path)
{
    AMIGA_TRACE(("autostart %s", path));
    if (autostart_autodetect(path, NULL, 0, AUTOSTART_MODE_RUN) < 0) {
        log_error(LOG_DEFAULT, "cannot autostart `%s'.", path);
        return FALSE;
    }
    return TRUE;
}

static BOOL Action_Autostart(void)
{
    char *path = request_file(MSG_REQ_AUTOSTART, PATTERN_AUTOSTART);
    BOOL ok = FALSE;

    if (path != NULL) {
        ok = amiga_autostart_file(path);
        lib_free(path);
    }
    return ok;
}

static BOOL Action_AttachDisk8(void)
{
    char *path = request_file(MSG_REQ_DISK8, PATTERN_DISK);
    BOOL ok = FALSE;

    if (path != NULL) {
        if (file_system_attach_disk(8, 0, path) < 0) {
            log_error(LOG_DEFAULT, "cannot attach `%s' to drive 8.", path);
        } else {
            ok = TRUE;
        }
        lib_free(path);
    }
    return ok;
}

static BOOL Action_DetachDisk8(void)
{
    file_system_detach_disk(8, 0);
    return TRUE;
}

int amiga_drive8_drawer_get(void)
{
    int bus = 0, fs = 0, tde = 1;

    resources_get_int("BusDevice8", &bus);
    resources_get_int("FileSystemDevice8", &fs);
    resources_get_int("Drive8TrueEmulation", &tde);
    return bus && fs == ATTACH_DEVICE_FS && !tde;
}

void amiga_drive8_drawer_set(int on)
{
    AMIGA_TRACE(("drive 8 reads an Amiga drawer: %d", on));
    if (on) {
        /* the emulated 1541 would answer instead of the virtual device */
        resources_set_int("Drive8TrueEmulation", 0);
        resources_set_int("FileSystemDevice8", ATTACH_DEVICE_FS);
        resources_set_int("BusDevice8", 1);
    } else {
        resources_set_int("BusDevice8", 0);
        resources_set_int("Drive8TrueEmulation", 1);
    }
}

static BOOL Action_Drive8Drawer(void)
{
    const char *current = NULL;
    char *path;

    resources_get_string("FSDevice8Dir", &current);
    path = amiga_drawer_request(amiga_video_window(), LOC(MSG_REQ_DRAWER8), current);
    vsync_suspend_speed_eval();
    if (path == NULL) {
        return FALSE;
    }
    resources_set_string("FSDevice8Dir", path);
    lib_free(path);
    /* an attached disk image would be read instead of the drawer */
    file_system_detach_disk(8, 0);
    amiga_drive8_drawer_set(1);
    return TRUE;
}

static BOOL Action_AttachCart(void)
{
    char *path = request_file(MSG_REQ_CART, PATTERN_CART);
    BOOL ok = FALSE;

    if (path != NULL) {
        /* attaching resets the machine */
        if (cartridge_attach_image(CARTRIDGE_CRT, path) < 0) {
            log_error(LOG_DEFAULT, "cannot attach cartridge `%s'.", path);
        } else {
            ok = TRUE;
        }
        lib_free(path);
    }
    return ok;
}

static BOOL Action_DetachCart(void)
{
    cartridge_detach_image(-1);
    return TRUE;
}

static BOOL Action_Reset(void)
{
    machine_trigger_reset(MACHINE_RESET_MODE_RESET_CPU);
    return TRUE;
}

static BOOL Action_HardReset(void)
{
    machine_trigger_reset(MACHINE_RESET_MODE_POWER_CYCLE);
    return TRUE;
}

static BOOL Action_Settings(void)
{
    amiga_settings_open();
    return TRUE;
}

static BOOL Action_Pause(void)
{
    if (ui_pause_active()) {
        ui_pause_disable();
    } else {
        ui_pause_enable();
    }
    return TRUE;
}

static int Checked_Pause(void)
{
    return ui_pause_active() ? 1 : 0;
}

static BOOL Action_Quit(void)
{
    archdep_vice_exit(0);
    return TRUE;
}

/* ------------------------------------------------------------------------- */
/* Display menu */

/* unchecking "Window" goes fullscreen (Amiga+F) */
static BOOL Action_DisplayWindow(void)
{
    amiga_video_set_fullscreen(!amiga_video_is_fullscreen());
    return TRUE;
}

static int Checked_DisplayWindow(void)
{
    return !amiga_video_is_fullscreen();
}

static BOOL set_scale(int scale)
{
    AMIGA_TRACE(("window size %dx%d", scale, scale));
    amiga_video_set_scale(scale);
    return TRUE;
}

static BOOL Action_WindowSize1x1(void) { return set_scale(1); }
static BOOL Action_WindowSize2x2(void) { return set_scale(2); }
static BOOL Action_WindowSize3x3(void) { return set_scale(3); }
static int Checked_WindowSize1x1(void) { return amiga_video_get_scale() == 1; }
static int Checked_WindowSize2x2(void) { return amiga_video_get_scale() == 2; }
static int Checked_WindowSize3x3(void) { return amiga_video_get_scale() == 3; }

static BOOL set_borders(int borders)
{
    amiga_video_set_borders(borders);
    return TRUE;
}

static BOOL Action_BordersFull(void) { return set_borders(AMIGA_BORDERS_FULL); }
static BOOL Action_BordersHalf(void) { return set_borders(AMIGA_BORDERS_HALF); }
static BOOL Action_BordersNone(void) { return set_borders(AMIGA_BORDERS_NONE); }
static int Checked_BordersFull(void) { return amiga_video_get_borders() == AMIGA_BORDERS_FULL; }
static int Checked_BordersHalf(void) { return amiga_video_get_borders() == AMIGA_BORDERS_HALF; }
static int Checked_BordersNone(void) { return amiga_video_get_borders() == AMIGA_BORDERS_NONE; }

/* ------------------------------------------------------------------------- */

/* MUST stay in the ACTION_* order */
static AmigaAction s_actions[AMIGA_ACTION_COUNT] = {
    /* AMIGA_ACTION_AUTOSTART       */ { Action_Autostart,     NULL,                  MSG_AUTOSTART,       NULL },
    /* AMIGA_ACTION_ATTACH_DISK8    */ { Action_AttachDisk8,   NULL,                  MSG_ATTACH_DISK8,    NULL },
    /* AMIGA_ACTION_DETACH_DISK8    */ { Action_DetachDisk8,   NULL,                  MSG_DETACH_DISK8,    NULL },
    /* AMIGA_ACTION_DRIVE8_DRAWER   */ { Action_Drive8Drawer,  NULL,                  MSG_DRIVE8_DRAWER_DOTS, NULL },
    /* AMIGA_ACTION_ATTACH_CART     */ { Action_AttachCart,    NULL,                  MSG_ATTACH_CART,     NULL },
    /* AMIGA_ACTION_DETACH_CART     */ { Action_DetachCart,    NULL,                  MSG_DETACH_CART,     NULL },
    /* AMIGA_ACTION_RESET           */ { Action_Reset,         NULL,                  MSG_RESET,           NULL },
    /* AMIGA_ACTION_HARD_RESET      */ { Action_HardReset,     NULL,                  MSG_HARD_RESET,      NULL },
    /* AMIGA_ACTION_SETTINGS        */ { Action_Settings,      NULL,                  MSG_SETTINGS_DOTS,   NULL },
    /* AMIGA_ACTION_PAUSE           */ { Action_Pause,         Checked_Pause,         MSG_PAUSE,           NULL },
    /* AMIGA_ACTION_QUIT            */ { Action_Quit,          NULL,                  MSG_QUIT,            NULL },
    /* AMIGA_ACTION_DISPLAY_WINDOW  */ { Action_DisplayWindow, Checked_DisplayWindow, MSG_DISPLAY_WINDOW,  NULL },
    /* AMIGA_ACTION_WINDOW_SIZE_1X1 */ { Action_WindowSize1x1, Checked_WindowSize1x1, MSG_WINDOW_SIZE_1X1, NULL },
    /* AMIGA_ACTION_WINDOW_SIZE_2X2 */ { Action_WindowSize2x2, Checked_WindowSize2x2, MSG_WINDOW_SIZE_2X2, NULL },
    /* AMIGA_ACTION_WINDOW_SIZE_3X3 */ { Action_WindowSize3x3, Checked_WindowSize3x3, MSG_WINDOW_SIZE_3X3, NULL },
    /* AMIGA_ACTION_BORDERS_FULL    */ { Action_BordersFull,   Checked_BordersFull,   MSG_BORDERS_FULL,    NULL },
    /* AMIGA_ACTION_BORDERS_HALF    */ { Action_BordersHalf,   Checked_BordersHalf,   MSG_BORDERS_HALF,    NULL },
    /* AMIGA_ACTION_BORDERS_NONE    */ { Action_BordersNone,   Checked_BordersNone,   MSG_BORDERS_NONE,    NULL }
};

void AmigaAction_Init(void)
{
    ULONG i;
    static int atexit_registered = 0;

    /* the settings window opens muimaster.library on first use */
    if (!atexit_registered) {
        atexit(amiga_mui_close_all);
        atexit_registered = 1;
    }

    for (i = 0; i < AMIGA_ACTION_COUNT; i++) {
        s_actions[i].name = LOC(s_actions[i].nameStringID);
    }
}

AmigaAction *AmigaAction_Get(ULONG actionID)
{
    if (actionID >= AMIGA_ACTION_COUNT) {
        return NULL;
    }
    return &s_actions[actionID];
}

BOOL AmigaAction_Execute(ULONG actionID)
{
    AmigaAction *a = AmigaAction_Get(actionID);

    if (a == NULL || a->func == NULL) {
        return FALSE;
    }
    AMIGA_TRACE(("action %lu: %s", (unsigned long)actionID, a->name ? a->name : "?"));
    return a->func();
}

int AmigaAction_IsChecked(ULONG actionID)
{
    AmigaAction *a = AmigaAction_Get(actionID);

    if (a == NULL || a->checked == NULL) {
        return -1;
    }
    return a->checked();
}
