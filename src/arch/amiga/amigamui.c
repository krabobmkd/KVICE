/** \file   amigamui.c
 * \brief   AmigaOS 3.x port: MUI settings window
 *
 * One window: a category list on the left, a page per category on the right,
 * Save / Use / Cancel at the bottom (MUI prefs style).
 *
 * Each setting is bound to a VICE resource (or to a getter/setter pair), the
 * same idea as the VICE 3.2 Amiga port mui.c ui_to_from_t tables: values are
 * read into the gadgets when the window opens, and written back to the
 * resources on Use or Save. Save also writes the vicerc file.
 *
 * The window is not modal: the MUI application signals are part of the main
 * loop Wait() (amigawait.c) and polled at each frame only when they are set
 * (vsyncarch.c), so the emulation keeps running and an idle or closed window
 * costs nothing. muimaster.library is optional.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <intuition/classusr.h>
#include <graphics/modeid.h>
#include <libraries/asl.h>
#include <libraries/mui.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/alib.h>

#include "amigaaction.h"
#include "amigafile.h"
#include "amigalocale.h"
#include "amigamui.h"
#include "amigatrace.h"
#include "amiga_screenmode.h"
#include "c64model.h"
#include "drive.h"
#include "joystick.h"
#include "lib.h"
#include "log.h"
#include "resources.h"

/* opened when the settings window is first used: MUI is optional */
struct Library *MUIMasterBase = NULL;

#define MUI_MIN_VERSION 16

/* GCC-safe MUI_NewObject wrapper (noinline forces a real m68k stack frame),
 * same as in MUImojiGear */
static Object * __attribute__((noinline))
MUI_NewObjectB(const char *cl, Tag tags, ...)
{
    return MUI_NewObjectA((char *)cl, (struct TagItem *)&tags);
}

#ifndef MAKE_ID
#define MAKE_ID(a, b, c, d) \
    ((ULONG)(a) << 24 | (ULONG)(b) << 16 | (ULONG)(c) << 8 | (ULONG)(d))
#endif

/* MUIM_Application_ReturnID values */
#define RID_SAVE    1
#define RID_USE     2
#define RID_CANCEL  3
#define RID_BROWSE  4   /* "..." of the drawer row */

/* ------------------------------------------------------------------------- */
/* setting bindings */

typedef enum {
    BIND_CHECK,     /* boolean resource, checkmark */
    BIND_CYCLE,     /* integer resource, one of values[] */
    BIND_DRAWER,    /* string resource, drawer path + "..." requester */
    BIND_SCREENMODE /* mode id resource, ASL screen mode popup */
} bind_type_t;

#define MAX_PATH_LEN 256

#define MAX_ENTRIES 16

typedef struct setting_s {
    bind_type_t type;
    const char *resource;       /* resource name, or NULL to use getter/setter */
    int (*getter)(void);
    void (*setter)(int value);
    ULONG label_msg;            /* MSG_* of the row label */
    /* cycle entries: localized labels and their values */
    const char *entries[MAX_ENTRIES + 1];   /* NULL terminated, for MUI */
    int values[MAX_ENTRIES];
    int count;
    /* runtime */
    Object *obj;
    Object *browse;             /* BIND_DRAWER: the "..." button */
    int initial_index;          /* index shown at open, -1 if unknown */
    char initial_string[MAX_PATH_LEN];  /* BIND_DRAWER: value shown at open */
} setting_t;

/* the drawer row, for the "..." requester */
static setting_t *drawer_setting = NULL;

/* Fullscreen page: the screen mode row (only one) and the checkbox that
 * disables it. The mode shown is kept here between the popup and Use. */
static setting_t *fs_auto_setting = NULL;
static setting_t *fs_mode_setting = NULL;
static Object *fs_mode_text = NULL;
static ULONG fs_mode_id = INVALID_ID;
static ULONG fs_initial_mode_id = INVALID_ID;
static char fs_mode_name[80];

static void fs_show_mode(void)
{
    if (fs_mode_id == INVALID_ID) {
        strcpy(fs_mode_name, "-");
    } else {
        amiga_screenmode_name(fs_mode_id, fs_mode_name, (int)sizeof fs_mode_name);
    }
    if (fs_mode_text != NULL) {
        set(fs_mode_text, MUIA_Text_Contents, (ULONG)fs_mode_name);
    }
}

static int setting_get_value(const setting_t *s, int *value)
{
    if (s->resource != NULL) {
        return resources_get_int(s->resource, value);
    }
    *value = s->getter();
    return 0;
}

static void setting_set_value(const setting_t *s, int value)
{
    if (s->resource != NULL) {
        if (resources_set_int(s->resource, value) < 0) {
            log_error(LOG_DEFAULT, "settings: cannot set %s to %d.", s->resource, value);
        }
    } else {
        s->setter(value);
    }
}

static void setting_add_entry(setting_t *s, const char *label, int value)
{
    if (s->count < MAX_ENTRIES) {
        s->entries[s->count] = label;
        s->values[s->count] = value;
        s->count++;
        s->entries[s->count] = NULL;
    }
}

/* resource -> gadget */
static void setting_to_ui(setting_t *s)
{
    int value = 0;
    int i;

    s->initial_index = -1;
    if (s->type == BIND_SCREENMODE) {
        int id = (int)INVALID_ID;

        resources_get_int("AmigaFullscreenModeID", &id);
        fs_mode_id = fs_initial_mode_id = (ULONG)id;
        fs_show_mode();
        return;
    }
    if (s->type == BIND_DRAWER) {
        const char *str = NULL;

        if (resources_get_string(s->resource, &str) < 0 || str == NULL) {
            str = "";
        }
        strncpy(s->initial_string, str, sizeof s->initial_string - 1);
        s->initial_string[sizeof s->initial_string - 1] = '\0';
        set(s->obj, MUIA_String_Contents, (ULONG)s->initial_string);
        return;
    }
    if (setting_get_value(s, &value) < 0) {
        log_error(LOG_DEFAULT, "settings: unknown resource %s.", s->resource);
        return;
    }
    if (s->type == BIND_CHECK) {
        s->initial_index = value ? 1 : 0;
        set(s->obj, MUIA_Selected, value ? TRUE : FALSE);
        return;
    }
    for (i = 0; i < s->count; i++) {
        if (s->values[i] == value) {
            s->initial_index = i;
            break;
        }
    }
    /* unknown value: show the first entry, but do not apply it on Use */
    set(s->obj, MUIA_Cycle_Active, s->initial_index >= 0 ? s->initial_index : 0);
}

/* gadget -> resource, only when the user changed something */
static void setting_from_ui(setting_t *s)
{
    ULONG v = 0;

    if (s->type == BIND_SCREENMODE) {
        if (fs_mode_id != fs_initial_mode_id) {
            resources_set_int("AmigaFullscreenModeID", (int)fs_mode_id);
        }
        return;
    }
    if (s->type == BIND_DRAWER) {
        STRPTR str = NULL;

        get(s->obj, MUIA_String_Contents, &str);
        if (str != NULL && strcmp((const char *)str, s->initial_string) != 0) {
            if (resources_set_string(s->resource, (const char *)str) < 0) {
                log_error(LOG_DEFAULT, "settings: cannot set %s.", s->resource);
            }
        }
        return;
    }
    if (s->type == BIND_CHECK) {
        get(s->obj, MUIA_Selected, &v);
        if ((int)(v ? 1 : 0) != s->initial_index) {
            setting_set_value(s, v ? 1 : 0);
        }
        return;
    }
    get(s->obj, MUIA_Cycle_Active, &v);
    if ((int)v < s->count && (int)v != s->initial_index) {
        setting_set_value(s, s->values[v]);
    }
}

/* ------------------------------------------------------------------------- */
/* the settings, by page */

enum {
    PAGE_INPUT = 0,
    PAGE_SOUND,
    PAGE_MACHINE,
    PAGE_DRIVE8,
    PAGE_FULLSCREEN,
    PAGE_COUNT
};

static const ULONG page_names[PAGE_COUNT] = {
    MSG_CATEGORY_INPUT, MSG_CATEGORY_SOUND, MSG_CATEGORY_MACHINE, MSG_CATEGORY_DRIVE8,
    MSG_CATEGORY_FULLSCREEN
};

#define MAX_SETTINGS_PER_PAGE 8

static setting_t settings[PAGE_COUNT][MAX_SETTINGS_PER_PAGE];
static int settings_count[PAGE_COUNT];

static setting_t *new_setting(int page, bind_type_t type, const char *resource, ULONG label_msg)
{
    setting_t *s = &settings[page][settings_count[page]++];

    memset(s, 0, sizeof *s);
    s->type = type;
    s->resource = resource;
    s->label_msg = label_msg;
    return s;
}

static void add_amiga_port_entries(setting_t *s)
{
    setting_add_entry(s, LOC(MSG_AMIGA_PORT_NONE), 0);
    setting_add_entry(s, LOC(MSG_AMIGA_PORT_JOYSTICK), 1);
    setting_add_entry(s, LOC(MSG_AMIGA_PORT_PADDLES), 2);
}

/* host joystick devices known to VICE, registered by arch/amiga/joy.c */
static void add_joydev_entries(setting_t *s)
{
    int i;

    setting_add_entry(s, LOC(MSG_DEVICE_NONE), JOYDEV_NONE);
    for (i = 0; i < joystick_device_count(); i++) {
        joystick_device_t *joydev = joystick_device_by_index(i);

        if (joydev != NULL && joydev->name != NULL) {
            setting_add_entry(s, joydev->name, JOYDEV_REALJOYSTICK_MIN + i);
        }
    }
}

static int get_c64model(void)
{
    return c64model_get();
}

static void set_c64model(int model)
{
    c64model_set(model);
}

/* (re)build the tables: labels are localized, joystick devices may change */
static void build_settings(void)
{
    setting_t *s;

    memset(settings_count, 0, sizeof settings_count);

    /* Input */
    s = new_setting(PAGE_INPUT, BIND_CYCLE, "AmigaJoyPort0", MSG_AMIGA_PORT0);
    add_amiga_port_entries(s);
    s = new_setting(PAGE_INPUT, BIND_CYCLE, "AmigaJoyPort1", MSG_AMIGA_PORT1);
    add_amiga_port_entries(s);
    s = new_setting(PAGE_INPUT, BIND_CYCLE, "JoyDevice1", MSG_C64_PORT1);
    add_joydev_entries(s);
    s = new_setting(PAGE_INPUT, BIND_CYCLE, "JoyDevice2", MSG_C64_PORT2);
    add_joydev_entries(s);

    /* Sound */
    new_setting(PAGE_SOUND, BIND_CHECK, "Sound", MSG_SOUND_ENABLE);
    s = new_setting(PAGE_SOUND, BIND_CYCLE, "SoundSampleRate", MSG_SOUND_RATE);
    setting_add_entry(s, "11025 Hz", 11025);
    setting_add_entry(s, "22050 Hz", 22050);
    setting_add_entry(s, "44100 Hz", 44100);
    setting_add_entry(s, "48000 Hz", 48000);

    /* Machine */
    s = new_setting(PAGE_MACHINE, BIND_CYCLE, NULL, MSG_C64_MODEL);
    s->getter = get_c64model;
    s->setter = set_c64model;
    setting_add_entry(s, LOC(MSG_MODEL_C64_PAL), C64MODEL_C64_PAL);
    setting_add_entry(s, LOC(MSG_MODEL_C64C_PAL), C64MODEL_C64C_PAL);
    setting_add_entry(s, LOC(MSG_MODEL_C64_OLD_PAL), C64MODEL_C64_OLD_PAL);
    setting_add_entry(s, LOC(MSG_MODEL_C64_NTSC), C64MODEL_C64_NTSC);
    setting_add_entry(s, LOC(MSG_MODEL_C64C_NTSC), C64MODEL_C64C_NTSC);
    setting_add_entry(s, LOC(MSG_MODEL_C64_OLD_NTSC), C64MODEL_C64_OLD_NTSC);
    setting_add_entry(s, LOC(MSG_MODEL_DREAN), C64MODEL_C64_PAL_N);

    /* Drive 8 */
    s = new_setting(PAGE_DRIVE8, BIND_CYCLE, "Drive8Type", MSG_DRIVE_TYPE);
    setting_add_entry(s, LOC(MSG_DRIVE_NONE), DRIVE_TYPE_NONE);
    setting_add_entry(s, "1541", DRIVE_TYPE_1541);
    setting_add_entry(s, "1541-II", DRIVE_TYPE_1541II);
    setting_add_entry(s, "1570", DRIVE_TYPE_1570);
    setting_add_entry(s, "1571", DRIVE_TYPE_1571);
    setting_add_entry(s, "1581", DRIVE_TYPE_1581);
    setting_add_entry(s, "2000", DRIVE_TYPE_2000);
    setting_add_entry(s, "4000", DRIVE_TYPE_4000);
    new_setting(PAGE_DRIVE8, BIND_CHECK, "Drive8TrueEmulation", MSG_DRIVE_TRUE_EMULATION);
    new_setting(PAGE_DRIVE8, BIND_CHECK, "AutostartHandleTrueDriveEmulation", MSG_AUTOSTART_FAST_LOAD);
    /* drawer before the switch: applied in this order on Use */
    drawer_setting = new_setting(PAGE_DRIVE8, BIND_DRAWER, "FSDevice8Dir", MSG_DRIVE8_DRAWER);
    s = new_setting(PAGE_DRIVE8, BIND_CHECK, NULL, MSG_DRIVE8_USE_DRAWER);
    s->getter = amiga_drive8_drawer_get;
    s->setter = amiga_drive8_drawer_set;

    /* Fullscreen */
    fs_auto_setting = new_setting(PAGE_FULLSCREEN, BIND_CHECK, "AmigaFullscreenAutoMode",
                                  MSG_FS_AUTO_MODE);
    fs_mode_setting = new_setting(PAGE_FULLSCREEN, BIND_SCREENMODE, NULL, MSG_FS_SCREEN_MODE);
}

/* ------------------------------------------------------------------------- */
/* MUI objects */

static Object *make_label(const char *text)
{
    return MUI_NewObjectB(MUIC_Text,
                          MUIA_Text_Contents, (ULONG)text,
                          MUIA_Text_PreParse, (ULONG)"\33r",
                          MUIA_Weight, 0,
                          MUIA_InnerLeft, 0,
                          MUIA_InnerRight, 0,
                          /* same height as the gadget on its right */
                          MUIA_Frame, MUIV_Frame_Button,
                          MUIA_FramePhantomHoriz, TRUE,
                          TAG_DONE);
}

static Object *make_text(const char *text)
{
    return MUI_NewObjectB(MUIC_Text,
                          MUIA_Text_Contents, (ULONG)text,
                          TAG_DONE);
}

static Object *make_checkmark(void)
{
    return MUI_NewObjectB(MUIC_Image,
                          MUIA_Image_Spec, (ULONG)MUII_CheckMark,
                          MUIA_InputMode, MUIV_InputMode_Toggle,
                          MUIA_Frame, MUIV_Frame_ImageButton,
                          MUIA_Background, MUII_ButtonBack,
                          MUIA_ShowSelState, FALSE,
                          MUIA_Selected, FALSE,
                          MUIA_CycleChain, 1,
                          TAG_DONE);
}

static Object *make_cycle(const char **entries)
{
    return MUI_NewObjectB(MUIC_Cycle,
                          MUIA_Cycle_Entries, (ULONG)entries,
                          MUIA_CycleChain, 1,
                          TAG_DONE);
}

static Object *make_button(const char *text)
{
    return MUI_NewObjectB(MUIC_Text,
                          MUIA_Text_Contents, (ULONG)text,
                          MUIA_Text_PreParse, (ULONG)"\33c",
                          MUIA_Frame, MUIV_Frame_Button,
                          MUIA_Background, MUII_ButtonBack,
                          MUIA_InputMode, MUIV_InputMode_RelVerify,
                          MUIA_CycleChain, 1,
                          TAG_DONE);
}

static Object *make_string(void)
{
    return MUI_NewObjectB(MUIC_String,
                          MUIA_Frame, MUIV_Frame_String,
                          MUIA_String_MaxLen, MAX_PATH_LEN,
                          MUIA_CycleChain, 1,
                          TAG_DONE);
}

/* ASL screen mode popup hooks (MUI Popasl), like AmigaMame LScreenModeReq.
 * No depth: the fullscreen code chooses it (16/32 colors on native modes). */
static struct TagItem fs_asl_tags[] = {
    { ASLSM_InitialDisplayID, 0 },
    { ASLSM_DoDepth, FALSE },
    { TAG_DONE, 0 }
};

static ULONG fs_popasl_start(register struct Hook *hook __asm("a0"),
                             register Object *popasl __asm("a2"),
                             register struct TagItem *tags __asm("a1"))
{
    int i;

    fs_asl_tags[0].ti_Data = (fs_mode_id != INVALID_ID) ? fs_mode_id : 0;
    /* chain our tags to the ones MUI passes to the requester */
    for (i = 0; tags[i].ti_Tag != TAG_DONE; i++) {
    }
    tags[i].ti_Tag = TAG_MORE;
    tags[i].ti_Data = (ULONG)fs_asl_tags;
    return TRUE;
}

static ULONG fs_popasl_stop(register struct Hook *hook __asm("a0"),
                            register Object *popasl __asm("a2"),
                            register struct ScreenModeRequester *smreq __asm("a1"))
{
    fs_mode_id = smreq->sm_DisplayID;
    AMIGA_TRACE(("screen mode chosen: 0x%08lx", (unsigned long)fs_mode_id));
    fs_show_mode();
    return 0;
}

static struct Hook fs_start_hook;
static struct Hook fs_stop_hook;

static Object *make_screenmode_popup(void)
{
    fs_start_hook.h_Entry = (ULONG (*)())fs_popasl_start;
    fs_stop_hook.h_Entry = (ULONG (*)())fs_popasl_stop;
    fs_mode_text = MUI_NewObjectB(MUIC_Text,
                                  MUIA_Frame, MUIV_Frame_Text,
                                  MUIA_Background, MUII_TextBack,
                                  TAG_DONE);
    return MUI_NewObjectB(MUIC_Popasl,
                          MUIA_Popstring_String, (ULONG)fs_mode_text,
                          MUIA_Popstring_Button, (ULONG)MUI_NewObjectB(MUIC_Image,
                                MUIA_Image_Spec, (ULONG)MUII_PopUp,
                                MUIA_Frame, MUIV_Frame_ImageButton,
                                MUIA_Background, MUII_ButtonBack,
                                MUIA_InputMode, MUIV_InputMode_RelVerify,
                                MUIA_CycleChain, 1,
                                TAG_DONE),
                          MUIA_Popasl_Type, ASL_ScreenModeRequest,
                          MUIA_Popasl_StartHook, (ULONG)&fs_start_hook,
                          MUIA_Popasl_StopHook, (ULONG)&fs_stop_hook,
                          TAG_DONE);
}

static Object *make_space(void)
{
    return MUI_NewObjectB(MUIC_Rectangle, TAG_DONE);
}

/* a page: 2 columns label / gadget, then free space */
static Object *make_page(int page)
{
    Object *columns;
    Object *group;
    int i;

    columns = MUI_NewObjectB(MUIC_Group,
                             MUIA_Group_Columns, 2,
                             TAG_DONE);
    if (columns == NULL) {
        return NULL;
    }
    for (i = 0; i < settings_count[page]; i++) {
        setting_t *s = &settings[page][i];

        switch (s->type) {
            case BIND_CHECK:
                s->obj = make_checkmark();
                break;
            case BIND_DRAWER:
                s->obj = make_string();
                s->browse = make_button(LOC(MSG_BROWSE));
                break;
            case BIND_SCREENMODE:
                s->obj = make_screenmode_popup();
                break;
            default:
                s->obj = make_cycle(s->entries);
                break;
        }
        if (s->obj == NULL) {
            MUI_DisposeObject(columns);
            return NULL;
        }
        DoMethod(columns, OM_ADDMEMBER, (ULONG)make_label(LOC(s->label_msg)));
        if (s->type == BIND_DRAWER) {
            /* string + small "..." button */
            Object *row = MUI_NewObjectB(MUIC_Group,
                                         MUIA_Group_Horiz, TRUE,
                                         MUIA_Group_Spacing, 0,
                                         MUIA_Group_Child, (ULONG)s->obj,
                                         MUIA_Group_Child, (ULONG)s->browse,
                                         TAG_DONE);
            set(s->browse, MUIA_Weight, 0);
            DoMethod(columns, OM_ADDMEMBER, (ULONG)row);
        } else if (s->type == BIND_CHECK) {
            /* keep the checkmark small, aligned left */
            Object *row = MUI_NewObjectB(MUIC_Group,
                                         MUIA_Group_Horiz, TRUE,
                                         MUIA_Group_Child, (ULONG)s->obj,
                                         MUIA_Group_Child, (ULONG)make_space(),
                                         TAG_DONE);
            DoMethod(columns, OM_ADDMEMBER, (ULONG)row);
        } else {
            DoMethod(columns, OM_ADDMEMBER, (ULONG)s->obj);
        }
    }

    group = MUI_NewObjectB(MUIC_Group,
                           MUIA_Frame, MUIV_Frame_Group,
                           MUIA_FrameTitle, (ULONG)LOC(page_names[page]),
                           MUIA_Group_Child, (ULONG)columns,
                           TAG_DONE);
    if (group == NULL) {
        MUI_DisposeObject(columns);
        return NULL;
    }
    if (page == PAGE_INPUT) {
        DoMethod(group, OM_ADDMEMBER, (ULONG)make_text(LOC(MSG_AMIGA_PORT_NEXT_START)));
    }
    if (page == PAGE_DRIVE8) {
        DoMethod(group, OM_ADDMEMBER, (ULONG)make_text(LOC(MSG_DRIVE8_DRAWER_NOTE)));
    }
    if (page == PAGE_FULLSCREEN) {
        DoMethod(group, OM_ADDMEMBER, (ULONG)make_text(LOC(MSG_FS_NOTE)));
    }
    DoMethod(group, OM_ADDMEMBER, (ULONG)make_space());
    return group;
}

/* ------------------------------------------------------------------------- */

static const char *category_names[PAGE_COUNT + 1];

/* the MUI application lives from the first Settings use to the exit */
static Object *mui_app = NULL;
static Object *mui_win = NULL;
/* signals MUI wants to be woken up for, 0 when nothing to do */
static ULONG mui_sigs = 0;
static int mui_win_open = 0;

static void apply_settings(void)
{
    int page, i;

    for (page = 0; page < PAGE_COUNT; page++) {
        for (i = 0; i < settings_count[page]; i++) {
            setting_from_ui(&settings[page][i]);
        }
    }
}

static void settings_to_ui(void)
{
    int page, i;

    for (page = 0; page < PAGE_COUNT; page++) {
        for (i = 0; i < settings_count[page]; i++) {
            setting_to_ui(&settings[page][i]);
        }
    }
    /* the notification only follows changes: initial state here */
    if (fs_auto_setting != NULL && fs_mode_setting != NULL) {
        ULONG automatic = FALSE;

        get(fs_auto_setting->obj, MUIA_Selected, &automatic);
        set(fs_mode_setting->obj, MUIA_Disabled, automatic ? TRUE : FALSE);
    }
}

/* build the application and its (closed) settings window */
static int create_app(void)
{
    Object *list, *pages;
    Object *bt_save, *bt_use, *bt_cancel;
    Object *page_objs[PAGE_COUNT];
    Object *list_obj = NULL;
    int page;

    if (MUIMasterBase == NULL) {
        MUIMasterBase = OpenLibrary((CONST_STRPTR)MUIMASTER_NAME, MUI_MIN_VERSION);
    }
    if (MUIMasterBase == NULL) {
        log_error(LOG_DEFAULT, "%s", LOC(MSG_ERROR_NO_MUI));
        return -1;
    }

    build_settings();
    for (page = 0; page < PAGE_COUNT; page++) {
        category_names[page] = LOC(page_names[page]);
        page_objs[page] = make_page(page);
    }
    category_names[PAGE_COUNT] = NULL;

    pages = MUI_NewObjectB(MUIC_Group,
                           MUIA_Group_PageMode, TRUE,
                           MUIA_Group_Child, (ULONG)page_objs[PAGE_INPUT],
                           MUIA_Group_Child, (ULONG)page_objs[PAGE_SOUND],
                           MUIA_Group_Child, (ULONG)page_objs[PAGE_MACHINE],
                           MUIA_Group_Child, (ULONG)page_objs[PAGE_DRIVE8],
                           MUIA_Group_Child, (ULONG)page_objs[PAGE_FULLSCREEN],
                           TAG_DONE);

    list = MUI_NewObjectB(MUIC_Listview,
                          MUIA_Listview_List, (ULONG)MUI_NewObjectB(MUIC_List,
                                MUIA_Frame, MUIV_Frame_InputList,
                                MUIA_List_SourceArray, (ULONG)category_names,
                                MUIA_List_AdjustWidth, TRUE,
                                MUIA_List_Active, 0,
                                TAG_DONE),
                          MUIA_CycleChain, 1,
                          TAG_DONE);

    bt_save = make_button(LOC(MSG_SETTINGS_SAVE));
    bt_use = make_button(LOC(MSG_SETTINGS_USE));
    bt_cancel = make_button(LOC(MSG_SETTINGS_CANCEL));

    mui_win = MUI_NewObjectB(MUIC_Window,
            MUIA_Window_Title, (ULONG)LOC(MSG_SETTINGS_TITLE),
            MUIA_Window_ID, MAKE_ID('V', 'S', 'E', 'T'),
            MUIA_Window_RootObject, (ULONG)MUI_NewObjectB(MUIC_Group,
                MUIA_Group_Child, (ULONG)MUI_NewObjectB(MUIC_Group,
                    MUIA_Group_Horiz, TRUE,
                    MUIA_Group_Child, (ULONG)list,
                    MUIA_Group_Child, (ULONG)pages,
                    TAG_DONE),
                MUIA_Group_Child, (ULONG)MUI_NewObjectB(MUIC_Group,
                    MUIA_Group_Horiz, TRUE,
                    MUIA_Group_SameSize, TRUE,
                    MUIA_Group_Child, (ULONG)bt_save,
                    MUIA_Group_Child, (ULONG)make_space(),
                    MUIA_Group_Child, (ULONG)bt_use,
                    MUIA_Group_Child, (ULONG)make_space(),
                    MUIA_Group_Child, (ULONG)bt_cancel,
                    TAG_DONE),
                TAG_DONE),
            TAG_DONE);

    mui_app = MUI_NewObjectB(MUIC_Application,
            MUIA_Application_Title, (ULONG)"VICE x64",
            MUIA_Application_Base, (ULONG)"VICEX64",
            MUIA_Application_Description, (ULONG)"Commodore 64 emulator",
            /* no Commodities broker nor ARexx port: nothing must need
             * servicing while the window is closed (MUI is not polled then) */
            MUIA_Application_UseCommodities, FALSE,
            MUIA_Application_UseRexx, FALSE,
            MUIA_Application_Window, (ULONG)mui_win,
            TAG_DONE);
    if (mui_app == NULL) {
        mui_win = NULL;
        log_error(LOG_DEFAULT, "%s", LOC(MSG_ERROR_SETTINGS_WINDOW));
        return -1;
    }

    /* list selection -> page */
    get(list, MUIA_Listview_List, &list_obj);
    DoMethod(list_obj, MUIM_Notify, MUIA_List_Active, MUIV_EveryTime,
             (ULONG)pages, 3, MUIM_Set, MUIA_Group_ActivePage, MUIV_TriggerValue);

    DoMethod(bt_save, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)mui_app, 2, MUIM_Application_ReturnID, RID_SAVE);
    DoMethod(bt_use, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)mui_app, 2, MUIM_Application_ReturnID, RID_USE);
    DoMethod(bt_cancel, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)mui_app, 2, MUIM_Application_ReturnID, RID_CANCEL);
    DoMethod(mui_win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             (ULONG)mui_app, 2, MUIM_Application_ReturnID, RID_CANCEL);
    /* automatic screen mode: no mode to choose */
    if (fs_auto_setting != NULL && fs_mode_setting != NULL
            && fs_auto_setting->obj != NULL && fs_mode_setting->obj != NULL) {
        DoMethod(fs_auto_setting->obj, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
                 (ULONG)fs_mode_setting->obj, 3, MUIM_Set, MUIA_Disabled, MUIV_TriggerValue);
    }
    if (drawer_setting != NULL && drawer_setting->browse != NULL) {
        DoMethod(drawer_setting->browse, MUIM_Notify, MUIA_Pressed, FALSE,
                 (ULONG)mui_app, 2, MUIM_Application_ReturnID, RID_BROWSE);
    }
    return 0;
}

static void close_window(void)
{
    set(mui_win, MUIA_Window_Open, FALSE);
    mui_win_open = 0;
    /* closed window: MUI must not be polled nor waited for anymore */
    mui_sigs = 0;
    AMIGA_TRACE(("settings window closed"));
}

/* "..." of the drawer row: ASL drawer requester on the settings window */
static void browse_drawer(void)
{
    struct Window *window = NULL;
    STRPTR current = NULL;
    char *path;

    if (drawer_setting == NULL) {
        return;
    }
    get(mui_win, MUIA_Window_Window, &window);
    get(drawer_setting->obj, MUIA_String_Contents, &current);
    set(mui_app, MUIA_Application_Sleep, TRUE);
    path = amiga_drawer_request(window, LOC(MSG_REQ_DRAWER8), (const char *)current);
    set(mui_app, MUIA_Application_Sleep, FALSE);
    if (path != NULL) {
        set(drawer_setting->obj, MUIA_String_Contents, (ULONG)path);
        lib_free(path);
    }
}

/** \brief  Open the settings window (non-modal, the emulation goes on)
 */
void amiga_settings_open(void)
{
    if (mui_app == NULL && create_app() != 0) {
        return;
    }
    if (mui_win_open) {
        DoMethod(mui_win, MUIM_Window_ToFront);
        return;
    }
    /* show the current values each time the window opens */
    settings_to_ui();
    set(mui_win, MUIA_Window_Open, TRUE);
    mui_win_open = 1;
    AMIGA_TRACE(("settings window open"));

    /* first input round: gives the signals MUI waits for */
    amiga_mui_handle_events();
}

/** \brief  Signals to add to the main loop Wait(), 0 when MUI is idle
 */
ULONG amiga_mui_signal_mask(void)
{
    return mui_sigs;
}

/** \brief  Process MUI input: call when one of amiga_mui_signal_mask() is set
 *
 * Runs in the emulator task between frames, so settings can be applied here.
 */
void amiga_mui_handle_events(void)
{
    LONG rid;

    if (mui_app == NULL || !mui_win_open) {
        return;
    }
    do {
        rid = (LONG)DoMethod(mui_app, MUIM_Application_NewInput, (ULONG)&mui_sigs);
        switch (rid) {
            case RID_SAVE:
                apply_settings();
                if (resources_save(NULL) < 0) {
                    log_error(LOG_DEFAULT, "settings: cannot save the configuration file.");
                }
                close_window();
                return;
            case RID_USE:
                apply_settings();
                close_window();
                return;
            case RID_CANCEL:
            case MUIV_Application_ReturnID_Quit:
                close_window();
                return;
            case RID_BROWSE:
                browse_drawer();
                break;
            default:
                break;
        }
    } while (rid != 0);
}

/** \brief  Free MUI resources, safe to call more than once
 */
void amiga_mui_close_all(void)
{
    if (mui_app != NULL) {
        if (mui_win_open) {
            set(mui_win, MUIA_Window_Open, FALSE);
            mui_win_open = 0;
        }
        /* disposes the window and all gadgets too */
        MUI_DisposeObject(mui_app);
        mui_app = NULL;
        mui_win = NULL;
        mui_sigs = 0;
    }
    if (MUIMasterBase != NULL) {
        CloseLibrary(MUIMasterBase);
        MUIMasterBase = NULL;
    }
}
