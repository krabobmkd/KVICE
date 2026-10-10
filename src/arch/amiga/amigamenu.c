/** \file   amigamenu.c
 * \brief   AmigaOS 3.x port: GadTools menu strip of the emulator window
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

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <libraries/gadtools.h>

#include "amigaaction.h"
#include "amigamachine.h"
#include "amigalocale.h"
#include "amigamenu.h"
#include "amigatrace.h"
#include "log.h"

/* ACTION_* id in the upper 16 bits, +1 so it is never 0 */
#define ACTION_UD(a)  ((ULONG)((a) + 1) << 16)
#define UD_IS_ACTION(ud) ((ULONG)(ud) > 0xFFFF)
#define UD_ACTION(ud) ((((ULONG)(ud)) >> 16) - 1)

#define MENU_TEMPLATE_MAX 80

/* nm_MutualExclude of 3 sibling radio items: each excludes the 2 others */
#define MX3(i) (7 & ~(1 << (i)))
/* and of 2 sibling radio items */
#define MX2(i) (3 & ~(1 << (i)))

static struct NewMenu s_menuTemplate[MENU_TEMPLATE_MAX];
static int s_n;

static struct Menu *s_menu = NULL;
static APTR s_visualInfo = NULL;

static void addEntry(UBYTE type, STRPTR label, STRPTR key,
                     UWORD flags, LONG mutex, ULONG udata)
{
    struct NewMenu *e = &s_menuTemplate[s_n++];

    e->nm_Type          = type;
    e->nm_Label         = label;
    e->nm_CommKey       = key;
    e->nm_Flags         = flags;
    e->nm_MutualExclude = mutex;
    e->nm_UserData      = (APTR)udata;
}

#define ADD(t, l, k, f, m, u)  addEntry((t), (STRPTR)(l), (STRPTR)(k), (f), (m), (ULONG)(u))
#define BAR(type)              ADD((type), NM_BARLABEL, 0, 0, 0, 0)

static void buildMenuTemplate(void)
{
    s_n = 0;

    ADD(NM_TITLE, amiga_machine.menu_title, 0, 0, 0, 0);
    ADD(NM_ITEM,  NULL, "A", 0, 0, ACTION_UD(AMIGA_ACTION_AUTOSTART));
    ADD(NM_ITEM,  NULL, 0,   0, 0, MSG_MENU_BASIC);     /* branch */
    ADD(NM_SUB,   NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_SAVE_BASIC));
    ADD(NM_SUB,   NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_SAVE_BAS));
    ADD(NM_SUB,   NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_LOAD_BAS));
    BAR(NM_ITEM);
    ADD(NM_ITEM,  NULL, "D", 0, 0, ACTION_UD(AMIGA_ACTION_ATTACH_DISK8));
    ADD(NM_ITEM,  NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_DETACH_DISK8));
    ADD(NM_ITEM,  NULL, 0,   0, 0, MSG_MENU_DRIVE8);    /* branch */
    ADD(NM_SUB,   NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_CREATE_DISK8));
    ADD(NM_SUB,   NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_EXTRACT_DISK8));
    ADD(NM_SUB,   NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_DRIVE8_DRAWER));
    if (amiga_machine.has_tape) {
        ADD(NM_ITEM,  NULL, 0,   0, 0, MSG_MENU_TAPE);  /* branch */
        ADD(NM_SUB,   NULL, "T", 0, 0, ACTION_UD(AMIGA_ACTION_ATTACH_TAPE));
        ADD(NM_SUB,   NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_DETACH_TAPE));
        ADD(NM_SUB,   NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_CREATE_TAPE));
        BAR(NM_SUB);
        ADD(NM_SUB,   NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_TAPE_PLAY));
        ADD(NM_SUB,   NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_TAPE_STOP));
        ADD(NM_SUB,   NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_TAPE_REWIND));
        ADD(NM_SUB,   NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_TAPE_FORWARD));
        ADD(NM_SUB,   NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_TAPE_RECORD));
        BAR(NM_SUB);
        ADD(NM_SUB,   NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_TAPE_RESET));
    }
    BAR(NM_ITEM);
    ADD(NM_ITEM,  NULL, "C", 0, 0, ACTION_UD(AMIGA_ACTION_ATTACH_CART));
    ADD(NM_ITEM,  NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_DETACH_CART));
    BAR(NM_ITEM);
    ADD(NM_ITEM,  NULL, "R", 0, 0, ACTION_UD(AMIGA_ACTION_RESET));
    ADD(NM_ITEM,  NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_HARD_RESET));
    BAR(NM_ITEM);
    ADD(NM_ITEM,  NULL, 0,   0, 0, ACTION_UD(AMIGA_ACTION_ABOUT));
    ADD(NM_ITEM,  NULL, "S", 0, 0, ACTION_UD(AMIGA_ACTION_SETTINGS));
    ADD(NM_ITEM,  NULL, "P", CHECKIT | MENUTOGGLE, 0, ACTION_UD(AMIGA_ACTION_PAUSE));
    BAR(NM_ITEM);
    ADD(NM_ITEM,  NULL, "Q", 0, 0, ACTION_UD(AMIGA_ACTION_QUIT));

    ADD(NM_TITLE, NULL, 0, 0, 0, MSG_MENU_DISPLAY);
    ADD(NM_ITEM,  NULL, "F", CHECKIT | MENUTOGGLE, 0, ACTION_UD(AMIGA_ACTION_DISPLAY_WINDOW));
    ADD(NM_ITEM,  NULL, 0, 0, 0, MSG_WINDOW_SIZE);      /* branch */
    ADD(NM_SUB,   NULL, 0, CHECKIT, MX3(0), ACTION_UD(AMIGA_ACTION_WINDOW_SIZE_1X1));
    ADD(NM_SUB,   NULL, 0, CHECKIT, MX3(1), ACTION_UD(AMIGA_ACTION_WINDOW_SIZE_2X2));
    ADD(NM_SUB,   NULL, 0, CHECKIT, MX3(2), ACTION_UD(AMIGA_ACTION_WINDOW_SIZE_3X3));
    ADD(NM_ITEM,  NULL, 0, 0, 0, MSG_BORDERS);          /* branch */
    ADD(NM_SUB,   NULL, 0, CHECKIT, MX3(0), ACTION_UD(AMIGA_ACTION_BORDERS_FULL));
    ADD(NM_SUB,   NULL, 0, CHECKIT, MX3(1), ACTION_UD(AMIGA_ACTION_BORDERS_HALF));
    ADD(NM_SUB,   NULL, 0, CHECKIT, MX3(2), ACTION_UD(AMIGA_ACTION_BORDERS_NONE));
    BAR(NM_ITEM);
    ADD(NM_ITEM,  NULL, 0, 0, 0, ACTION_UD(AMIGA_ACTION_SAVE_SCREENSHOT));

    ADD(NM_TITLE, NULL, 0, 0, 0, MSG_MENU_SNAPSHOT);
    ADD(NM_ITEM,  NULL, "L", 0, 0, ACTION_UD(AMIGA_ACTION_SNAPSHOT_LOAD));
    ADD(NM_ITEM,  NULL, "W", 0, 0, ACTION_UD(AMIGA_ACTION_SNAPSHOT_SAVE));

    ADD(NM_TITLE, NULL, 0, 0, 0, MSG_MENU_KEYBOARD);
    ADD(NM_ITEM,  NULL, 0, CHECKIT, MX2(0), ACTION_UD(AMIGA_ACTION_KEYBOARD_SYMBOLIC));
    ADD(NM_ITEM,  NULL, 0, CHECKIT, MX2(1), ACTION_UD(AMIGA_ACTION_KEYBOARD_POSITIONAL));

    ADD(NM_END, NULL, 0, 0, 0, 0);
}

/* labels from the locale (titles, branches) and from the actions (items),
 * initial check marks from the action states */
static void resolveMenuTemplate(void)
{
    int i;

    for (i = 0; s_menuTemplate[i].nm_Type != NM_END; i++) {
        struct NewMenu *e = &s_menuTemplate[i];
        ULONG udata = (ULONG)e->nm_UserData;

        if (e->nm_Label == NM_BARLABEL || e->nm_Label != NULL) {
            continue;
        }
        if (UD_IS_ACTION(udata)) {
            AmigaAction *a = AmigaAction_Get(UD_ACTION(udata));

            e->nm_Label = (STRPTR)((a != NULL && a->name != NULL) ? a->name : "???");
            if (AmigaAction_IsChecked(UD_ACTION(udata)) == 1) {
                e->nm_Flags |= CHECKED;
            } else {
                e->nm_Flags &= ~CHECKED;
            }
        } else {
            e->nm_Label = (STRPTR)LOC(udata);
        }
    }
}

BOOL AmigaMenu_Create(struct Window *window)
{
    if (window == NULL || GadToolsBase == NULL) {
        return FALSE;
    }
    AmigaMenu_Close(window);

    s_visualInfo = GetVisualInfo(window->WScreen, TAG_END);
    if (s_visualInfo == NULL) {
        log_error(LOG_DEFAULT, "%s", LOC(MSG_ERROR_MENU));
        return FALSE;
    }
    buildMenuTemplate();
    resolveMenuTemplate();

    s_menu = CreateMenus(s_menuTemplate, TAG_END);
    if (s_menu == NULL
            || !LayoutMenus(s_menu, s_visualInfo, GTMN_NewLookMenus, TRUE, TAG_END)) {
        log_error(LOG_DEFAULT, "%s", LOC(MSG_ERROR_MENU));
        AmigaMenu_Close(NULL);
        return FALSE;
    }
    SetMenuStrip(window, s_menu);
    return TRUE;
}

void AmigaMenu_Close(struct Window *window)
{
    if (window != NULL && s_menu != NULL) {
        ClearMenuStrip(window);
    }
    if (s_menu != NULL) {
        FreeMenus(s_menu);
        s_menu = NULL;
    }
    if (s_visualInfo != NULL) {
        FreeVisualInfo(s_visualInfo);
        s_visualInfo = NULL;
    }
}

void AmigaMenu_HandlePick(struct Window *window, UWORD menuCode)
{
    /* several items can be picked in one go (multi-select) */
    while (menuCode != MENUNULL && s_menu != NULL) {
        struct MenuItem *item = ItemAddress(s_menu, menuCode);
        ULONG udata;

        if (item == NULL) {
            break;
        }
        /* read before running the action, which could rebuild the menus */
        menuCode = item->NextSelect;
        udata = (ULONG)GTMENUITEM_USERDATA(item);
        if (UD_IS_ACTION(udata)) {
            AmigaAction_Execute(UD_ACTION(udata));
        }
    }
    AmigaMenu_SyncChecks(window);
}

static void sync_items(struct MenuItem *item)
{
    for (; item != NULL; item = item->NextItem) {
        ULONG udata = (ULONG)GTMENUITEM_USERDATA(item);

        if (UD_IS_ACTION(udata)) {
            int checked = AmigaAction_IsChecked(UD_ACTION(udata));

            if (checked == 1) {
                item->Flags |= CHECKED;
            } else if (checked == 0) {
                item->Flags &= ~CHECKED;
            }
        }
        if (item->SubItem != NULL) {
            sync_items(item->SubItem);
        }
    }
}

void AmigaMenu_SyncChecks(struct Window *window)
{
    struct Menu *menu;

    if (window == NULL || s_menu == NULL) {
        return;
    }
    /* menu flags may only be changed while the strip is detached */
    ClearMenuStrip(window);
    for (menu = s_menu; menu != NULL; menu = menu->NextMenu) {
        sync_items(menu->FirstItem);
    }
    ResetMenuStrip(window, s_menu);
}
