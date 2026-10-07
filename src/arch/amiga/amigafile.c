/** \file   amigafile.c
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

#include "vice.h"

#include <string.h>

#include <exec/types.h>
#include <libraries/asl.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/asl.h>
#include <proto/dos.h>

#include "amigafile.h"
#include "archdep_defs.h"
#include "lib.h"

/* drawer of the last chosen file, the next requester opens there */
static char last_drawer[ARCHDEP_PATH_MAX] = "";

/* load (open) or save requester */
static char *file_request(struct Window *window, const char *title, const char *pattern,
                          BOOL save)
{
    struct FileRequester *req;
    char *path = NULL;

    if (AslBase == NULL) {
        return NULL;
    }
    req = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
            ASLFR_TitleText, (ULONG)title,
            ASLFR_InitialDrawer, (ULONG)last_drawer,
            ASLFR_InitialPattern, (ULONG)(pattern != NULL ? pattern : "#?"),
            ASLFR_DoPatterns, TRUE,
            ASLFR_RejectIcons, TRUE,
            ASLFR_DoSaveMode, save,
            window != NULL ? ASLFR_Window : TAG_IGNORE, (ULONG)window,
            /* block the emulator window input while the requester is open */
            window != NULL ? ASLFR_SleepWindow : TAG_IGNORE, TRUE,
            TAG_DONE);
    if (req == NULL) {
        return NULL;
    }
    if (AslRequest(req, NULL) && req->fr_File != NULL && req->fr_File[0] != '\0') {
        size_t size = strlen(req->fr_Drawer) + strlen(req->fr_File) + 2;

        path = lib_malloc(size);
        strcpy(path, req->fr_Drawer);
        AddPart((STRPTR)path, (CONST_STRPTR)req->fr_File, size);

        strncpy(last_drawer, req->fr_Drawer, sizeof last_drawer - 1);
        last_drawer[sizeof last_drawer - 1] = '\0';
    }
    FreeAslRequest(req);
    return path;
}

char *amiga_file_request(struct Window *window, const char *title, const char *pattern)
{
    return file_request(window, title, pattern, FALSE);
}

char *amiga_file_save_request(struct Window *window, const char *title, const char *pattern)
{
    return file_request(window, title, pattern, TRUE);
}

char *amiga_drawer_request(struct Window *window, const char *title, const char *initial)
{
    struct FileRequester *req;
    char *path = NULL;

    if (AslBase == NULL) {
        return NULL;
    }
    req = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
            ASLFR_TitleText, (ULONG)title,
            ASLFR_InitialDrawer, (ULONG)((initial != NULL && initial[0] != '\0') ? initial : last_drawer),
            ASLFR_DrawersOnly, TRUE,
            window != NULL ? ASLFR_Window : TAG_IGNORE, (ULONG)window,
            window != NULL ? ASLFR_SleepWindow : TAG_IGNORE, TRUE,
            TAG_DONE);
    if (req == NULL) {
        return NULL;
    }
    if (AslRequest(req, NULL) && req->fr_Drawer != NULL) {
        path = lib_strdup(req->fr_Drawer);
    }
    FreeAslRequest(req);
    return path;
}

char *amiga_wbarg_path(const struct WBArg *arg)
{
    char buffer[ARCHDEP_PATH_MAX];

    if (arg == NULL || arg->wa_Lock == 0) {
        return NULL;
    }
    if (!NameFromLock(arg->wa_Lock, (STRPTR)buffer, sizeof buffer)) {
        return NULL;
    }
    /* a dropped drawer has an empty name: the lock is the path */
    if (arg->wa_Name != NULL && arg->wa_Name[0] != '\0') {
        if (!AddPart((STRPTR)buffer, (CONST_STRPTR)arg->wa_Name, sizeof buffer)) {
            return NULL;
        }
    }
    return lib_strdup(buffer);
}
