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
#include "amigakeys.h"
#include "amigafile.h"
#include "amigalocale.h"
#include "amigamui.h"
#include "amigatrace.h"
#include "amiga_screenmode.h"
#include "amigavideo.h"
#include "amigawait.h"
#include "c64mem.h"
#include "c64model.h"
#include "c64rom.h"
#include "drive.h"
#include "drivetypes.h"
#include "iecbus.h"
#include "iecrom.h"
#include "keyboard.h"
#include "keymap.h"
#include "joyport.h"
#include "joystick.h"
#include "amigajoy.h"
#include "lib.h"
#include "log.h"
#include "machine.h"
#include "resources.h"
#include "sound.h"
#include "sysfile.h"
#include "util.h"

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
#define RID_ROM_DEFAULT 5   /* "Set ROM defaults for this model" */
#define RID_MODEL   7   /* the C64 model cycle changed: its description */
#define RID_DRIVE_TYPE 8    /* the drive 8 type cycle changed: its ROM file */
#define RID_INPUT_PORTS 20  /* an Amiga port type cycle changed */
#define RID_INPUT_APPLY 21  /* "Apply input configuration" */
#define RID_BROWSE_ROM  10  /* + 0..2: "..." of a ROM file row */
#define RID_BROWSE_FILE 6   /* "..." of the file row (custom keymap) */

/* ------------------------------------------------------------------------- */
/* setting bindings */

typedef enum {
    BIND_CHECK,     /* boolean resource, checkmark */
    BIND_CYCLE,     /* integer resource, one of values[] */
    BIND_DRAWER,    /* string resource, drawer path + "..." requester */
    BIND_SCREENMODE,/* mode id resource, ASL screen mode popup */
    BIND_ROMFILE,   /* ROM file name resource, file path + "..." requester */
    BIND_FILE       /* file name resource, file path + "..." requester */
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
    Object *browse;             /* BIND_DRAWER, BIND_ROMFILE: the "..." button */
    int rom_size;               /* BIND_ROMFILE: the exact file size */
    const char *pattern;        /* BIND_FILE: requester pattern */
    ULONG req_msg;              /* BIND_FILE: requester title */
    int initial_index;          /* index shown at open, -1 if unknown */
    char initial_string[MAX_PATH_LEN];  /* BIND_DRAWER, BIND_ROMFILE: value shown at open */
} setting_t;

/* Drive 8 page: the type cycle and, under it, the ROM file it needs */
static setting_t *drive_type_setting = NULL;
static Object *drive_rom_text = NULL;
static char drive_rom_info[MAX_PATH_LEN + 40];

/* the drawer row, for the "..." requester */
static setting_t *drawer_setting = NULL;
/* the file row (custom keymap), for the "..." requester */
static setting_t *file_setting = NULL;

/* Keyboard page: C64 key -> Amiga key table of the keymap in use */
#define KEYS_ROWS_MAX 40
static amiga_key_row_t keys_rows[KEYS_ROWS_MAX];
static Object *keys_list = NULL;

/* Machine page: Kernal, BASIC, character ROM file rows, their "Set ROM
 * defaults for this model" button, and the model cycle (the model does not
 * change the ROM files, the button does) */
#define ROM_COUNT 3
static setting_t *rom_settings[ROM_COUNT];
static Object *rom_default_button = NULL;
static setting_t *model_setting = NULL;
/* under the model cycle: video standard and SID of the model chosen */
static Object *model_info_text = NULL;
static char model_info[80];
/* where the ROMs are searched by default (sysfile path, machine drawer) */
#define ROM_DEFAULT_DRAWER "PROGDIR:C64/"

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

/* ROM file of a row as shown: the file found by the system file search
 * (PROGDIR:C64/... first), or where the default drawer would have it */
static void rom_display_path(const setting_t *s, char *out, size_t size)
{
    const char *name = NULL;
    char *found = NULL;

    if (resources_get_string(s->resource, &name) < 0 || name == NULL) {
        name = "";
    }
    if (strchr(name, ':') == NULL && strchr(name, '/') == NULL
            && sysfile_locate(name, "C64", &found) == 0 && found != NULL) {
        strncpy(out, found, size - 1);
        lib_free(found);
    } else if (strchr(name, ':') == NULL && strchr(name, '/') == NULL) {
        snprintf(out, size, "%s%s", ROM_DEFAULT_DRAWER, name);
    } else {
        strncpy(out, name, size - 1);
    }
    out[size - 1] = '\0';
}

/* 1 when \a path is a file of exactly \a size bytes */
static int rom_file_ok(const char *path, int size)
{
    FILE *f;
    long len = -1;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    f = fopen(path, "rb");
    if (f == NULL) {
        return 0;
    }
    if (fseek(f, 0, SEEK_END) == 0) {
        len = ftell(f);
    }
    fclose(f);
    return len == (long)size;
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
    if (s->type == BIND_ROMFILE) {
        rom_display_path(s, s->initial_string, sizeof s->initial_string);
        set(s->obj, MUIA_String_Contents, (ULONG)s->initial_string);
        return;
    }
    if (s->type == BIND_DRAWER || s->type == BIND_FILE) {
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
    nnset(s->obj, MUIA_Cycle_Active, s->initial_index >= 0 ? s->initial_index : 0);
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
    if (s->type == BIND_DRAWER || s->type == BIND_FILE) {
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
    /* unknown value: the first entry is shown, apply only a real choice */
    if ((int)v < s->count
            && (int)v != (s->initial_index >= 0 ? s->initial_index : 0)) {
        setting_set_value(s, s->values[v]);
    }
}

/* ------------------------------------------------------------------------- */
/* the settings, by page */

enum {
    PAGE_MACHINE = 0,
    PAGE_INPUT,
    PAGE_KEYBOARD,
    PAGE_SOUND,
    PAGE_DRIVE8,
    PAGE_FULLSCREEN,
    PAGE_COUNT
};

static const ULONG page_names[PAGE_COUNT] = {
    MSG_CATEGORY_MACHINE, MSG_CATEGORY_INPUT, MSG_CATEGORY_KEYBOARD, MSG_CATEGORY_SOUND,
    MSG_CATEGORY_DRIVE8, MSG_CATEGORY_FULLSCREEN
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

/* analog paddles: joystick port only, the mouse port is input.device's */
static void add_amiga_port_entries(setting_t *s, int paddles)
{
    setting_add_entry(s, LOC(MSG_AMIGA_PORT_NONE), 0);
    setting_add_entry(s, LOC(MSG_AMIGA_PORT_JOYSTICK), 1);
    if (paddles) {
        setting_add_entry(s, LOC(MSG_AMIGA_PORT_PADDLES), 2);
    }
}

/* Input page: the Amiga port types (mouse port, joystick port) and what
 * drives each C64 control port: none, or the device of an Amiga port. All
 * applied together by input_apply() ("Apply input configuration", Use,
 * Save): the Amiga ports are opened again when their type changed. */
#define C64_FROM_NONE           0
#define C64_FROM_MOUSE_PORT     1   /* lowlevel port 0 */
#define C64_FROM_JOYSTICK_PORT  2   /* lowlevel port 1 */
static setting_t *amiga_port_setting[2];
static setting_t *c64_port_setting[2];
static Object *input_apply_button = NULL;

static void add_c64_port_entries(setting_t *s)
{
    setting_add_entry(s, LOC(MSG_DEVICE_NONE), C64_FROM_NONE);
    setting_add_entry(s, LOC(MSG_C64_FROM_MOUSE_PORT), C64_FROM_MOUSE_PORT);
    setting_add_entry(s, LOC(MSG_C64_FROM_JOYSTICK_PORT), C64_FROM_JOYSTICK_PORT);
}

/* the Amiga port driving C64 control port \a c64port (1 or 2) */
static int get_c64_port(int c64port)
{
    int dev = JOYDEV_NONE;

    resources_get_int_sprintf("JoyDevice%d", &dev, c64port);
    if (dev < JOYDEV_REALJOYSTICK_MIN) {
        return C64_FROM_NONE;
    }
    switch (amiga_joy_device_port(dev - JOYDEV_REALJOYSTICK_MIN)) {
        case 0:  return C64_FROM_MOUSE_PORT;
        case 1:  return C64_FROM_JOYSTICK_PORT;
        default: return C64_FROM_NONE;
    }
}

static int get_c64_port1(void) { return get_c64_port(1); }
static int get_c64_port2(void) { return get_c64_port(2); }
/* applied by input_apply(), after the Amiga ports */
static void set_c64_port_none(int value) { }

/* 1 when the machine settings (video, CIA, SID, board) are the ones of
 * \a model: the ROM files are not compared, they are chosen apart */
static int c64model_matches(int model)
{
    c64model_details_t details;

    memset(&details, 0, sizeof details);
    if (resources_get_int("MachineVideoStandard", &details.vicii_model) < 0
            || resources_get_int("SidModel", &details.sid_model) < 0
            || resources_get_int("CIA1Model", &details.cia1_model) < 0
            || resources_get_int("CIA2Model", &details.cia2_model) < 0
            || resources_get_int("BoardType", &details.board) < 0
            || resources_get_int("IECReset", &details.iecreset) < 0) {
        return 0;
    }
    details.chargen = c64model_get_chargen_name(model);
    details.kernalrev = c64model_get_kernal_rev(model);
    return c64model_get_model(&details) == model;
}

/* the model chosen last ("AmigaC64Model", C64 and C64 old only differ by
 * their kernal), else the first one the machine settings match */
static int get_c64model(void)
{
    int stored = -1;
    int i;

    resources_get_int("AmigaC64Model", &stored);
    if (stored >= 0 && c64model_matches(stored)) {
        return stored;
    }
    for (i = 0; model_setting != NULL && i < model_setting->count; i++) {
        if (c64model_matches(model_setting->values[i])) {
            return model_setting->values[i];
        }
    }
    return -1;
}

static void set_c64model(int model)
{
    c64model_set(model);
    resources_set_int("AmigaC64Model", model);
}

/* drive 8 type: the one chosen, even while its ROM is missing (the drive
 * is then "none" in VICE, and the type is saved for when the ROM is found) */
static int get_drive8_type(void)
{
    int type = DRIVE_TYPE_NONE;

    if (drive_rom_missing_type[0] != 0) {
        return (int)drive_rom_missing_type[0];
    }
    resources_get_int("Drive8Type", &type);
    return type;
}

static void set_drive8_type(int type)
{
    if (type != DRIVE_TYPE_NONE && iecrom_check_loaded((unsigned int)type) < 0) {
        int current = DRIVE_TYPE_NONE;

        /* VICE would refuse it: the drive is off as after a restart, and
           the type is kept ("none" first, it clears the kept type) */
        resources_get_int("Drive8Type", &current);
        if (current != DRIVE_TYPE_NONE) {
            resources_set_int("Drive8Type", DRIVE_TYPE_NONE);
        }
        drive_rom_missing_type[0] = (unsigned int)type;
        log_warning(LOG_DEFAULT, "settings: no ROM for drive 8 type %d, kept until its ROM is found.", type);
        return;
    }
    if (resources_set_int("Drive8Type", type) < 0) {
        log_error(LOG_DEFAULT, "settings: cannot set drive 8 type %d.", type);
    }
}

/* keymap file: the standard positional one or a custom one. Both VICE
 * indexes 0 and 1 give amiga_positional.vkm on Amiga (keymap.c). */
static int get_keymap_file(void)
{
    int idx = KBD_INDEX_POS;

    resources_get_int("KeymapIndex", &idx);
    return idx < 2 ? KBD_INDEX_POS : KBD_INDEX_USERPOS;
}

static void set_keymap_file(int idx)
{
    if (resources_set_int("KeymapIndex", idx) < 0) {
        log_error(LOG_DEFAULT, "settings: cannot load the keymap file.");
    }
}

/* REU: one cycle, 0 is off, else the size in KB */
static int get_reu(void)
{
    int on = 0, size = 0;

    resources_get_int("REU", &on);
    resources_get_int("REUsize", &size);
    return on ? size : 0;
}

static void set_reu(int size_kb)
{
    /* off while the size changes: set again on, the memory is reallocated */
    resources_set_int("REU", 0);
    if (size_kb > 0) {
        if (resources_set_int("REUsize", size_kb) < 0
                || resources_set_int("REU", 1) < 0) {
            log_error(LOG_DEFAULT, "settings: cannot enable a %d KB REU.", size_kb);
        }
    }
}

/* (re)build the tables: labels are localized, joystick devices may change */
static void build_settings(void)
{
    setting_t *s;

    memset(settings_count, 0, sizeof settings_count);

    /* Input */
    s = amiga_port_setting[0] = new_setting(PAGE_INPUT, BIND_CYCLE, "AmigaJoyPort0", MSG_AMIGA_PORT0);
    add_amiga_port_entries(s, 0);
    s = amiga_port_setting[1] = new_setting(PAGE_INPUT, BIND_CYCLE, "AmigaJoyPort1", MSG_AMIGA_PORT1);
    add_amiga_port_entries(s, 1);
    s = c64_port_setting[0] = new_setting(PAGE_INPUT, BIND_CYCLE, NULL, MSG_C64_PORT1);
    s->getter = get_c64_port1;
    s->setter = set_c64_port_none;
    add_c64_port_entries(s);
    s = c64_port_setting[1] = new_setting(PAGE_INPUT, BIND_CYCLE, NULL, MSG_C64_PORT2);
    s->getter = get_c64_port2;
    s->setter = set_c64_port_none;
    add_c64_port_entries(s);

    /* Keyboard: the custom file is applied before the choice (Use) */
    s = new_setting(PAGE_KEYBOARD, BIND_CYCLE, "AmigaKeyboardSymbolic", MSG_KEYBOARD_MAPPING);
    setting_add_entry(s, LOC(MSG_KEYBOARD_SYMBOLIC), 1);
    setting_add_entry(s, LOC(MSG_KEYBOARD_POSITIONAL), 0);
    s = new_setting(PAGE_KEYBOARD, BIND_CYCLE, NULL, MSG_KEYMAP);
    s->getter = get_keymap_file;
    s->setter = set_keymap_file;
    setting_add_entry(s, LOC(MSG_KEYMAP_STANDARD), KBD_INDEX_POS);
    setting_add_entry(s, LOC(MSG_KEYMAP_CUSTOM), KBD_INDEX_USERPOS);
    file_setting = s = new_setting(PAGE_KEYBOARD, BIND_FILE, "KeymapUserPosFile", MSG_KEYMAP_FILE);
    s->pattern = "#?.vkm";
    s->req_msg = MSG_REQ_KEYMAP;

    /* Sound */
    new_setting(PAGE_SOUND, BIND_CHECK, "Sound", MSG_SOUND_ENABLE);
    s = new_setting(PAGE_SOUND, BIND_CYCLE, "SoundSampleRate", MSG_SOUND_RATE);
    /* 48 kHz: too much work for an Amiga */
    setting_add_entry(s, "11025 Hz", 11025);
    setting_add_entry(s, "16000 Hz", 16000);
    setting_add_entry(s, "22050 Hz", 22050);
    setting_add_entry(s, "44100 Hz", 44100);

    /* Machine: the ROM files, then the model */
    s = rom_settings[0] = new_setting(PAGE_MACHINE, BIND_ROMFILE, "KernalName", MSG_ROM_KERNAL);
    s->rom_size = C64_KERNAL_ROM_SIZE;
    s = rom_settings[1] = new_setting(PAGE_MACHINE, BIND_ROMFILE, "BasicName", MSG_ROM_BASIC);
    s->rom_size = C64_BASIC_ROM_SIZE;
    s = rom_settings[2] = new_setting(PAGE_MACHINE, BIND_ROMFILE, "ChargenName", MSG_ROM_CHARGEN);
    s->rom_size = C64_CHARGEN_ROM_SIZE;
    s = model_setting = new_setting(PAGE_MACHINE, BIND_CYCLE, NULL, MSG_C64_MODEL);
    s->getter = get_c64model;
    s->setter = set_c64model;
    setting_add_entry(s, LOC(MSG_MODEL_C64_PAL), C64MODEL_C64_PAL);
    setting_add_entry(s, LOC(MSG_MODEL_C64C_PAL), C64MODEL_C64C_PAL);
    setting_add_entry(s, LOC(MSG_MODEL_C64_OLD_PAL), C64MODEL_C64_OLD_PAL);
    setting_add_entry(s, LOC(MSG_MODEL_C64_NTSC), C64MODEL_C64_NTSC);
    setting_add_entry(s, LOC(MSG_MODEL_C64C_NTSC), C64MODEL_C64C_NTSC);
    setting_add_entry(s, LOC(MSG_MODEL_C64_OLD_NTSC), C64MODEL_C64_OLD_NTSC);
    setting_add_entry(s, LOC(MSG_MODEL_DREAN), C64MODEL_C64_PAL_N);
    s = new_setting(PAGE_MACHINE, BIND_CYCLE, NULL, MSG_REU);
    s->getter = get_reu;
    s->setter = set_reu;
    setting_add_entry(s, LOC(MSG_REU_OFF), 0);
    setting_add_entry(s, "128 KB (1700)", 128);
    setting_add_entry(s, "256 KB (1764)", 256);
    setting_add_entry(s, "512 KB (1750)", 512);

    /* Drive 8 */
    s = drive_type_setting = new_setting(PAGE_DRIVE8, BIND_CYCLE, NULL, MSG_DRIVE_TYPE);
    s->getter = get_drive8_type;
    s->setter = set_drive8_type;
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
    new_setting(PAGE_FULLSCREEN, BIND_CHECK, "AmigaFullscreenMenu", MSG_FS_MENU);
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

/* the keys table: 2 columns, C64 key and Amiga key(s) */
static ULONG keys_display(register struct Hook *hook __asm("a0"),
                          register char **array __asm("a2"),
                          register amiga_key_row_t *row __asm("a1"))
{
    if (row == NULL) {
        /* title line */
        array[0] = (char *)LOC(MSG_KEYS_C64);
        array[1] = (char *)LOC(MSG_KEYS_AMIGA);
    } else {
        array[0] = row->c64;
        array[1] = row->host;
    }
    return 0;
}

static struct Hook keys_display_hook;

static Object *make_keys_list(void)
{
    keys_display_hook.h_Entry = (ULONG (*)())keys_display;
    keys_list = MUI_NewObjectB(MUIC_List,
                               MUIA_Frame, MUIV_Frame_ReadList,
                               MUIA_List_Format, (ULONG)"BAR,",
                               MUIA_List_Title, TRUE,
                               MUIA_List_DisplayHook, (ULONG)&keys_display_hook,
                               TAG_DONE);
    if (keys_list == NULL) {
        return NULL;
    }
    return MUI_NewObjectB(MUIC_Listview,
                          MUIA_Listview_List, (ULONG)keys_list,
                          MUIA_Listview_Input, FALSE,
                          TAG_DONE);
}

/* the table of the keymap in use, each time the window opens */
static void keys_list_fill(void)
{
    int i, n;

    if (keys_list == NULL) {
        return;
    }
    n = amiga_keys_table(keys_rows, KEYS_ROWS_MAX);
    set(keys_list, MUIA_List_Quiet, TRUE);
    DoMethod(keys_list, MUIM_List_Clear);
    for (i = 0; i < n; i++) {
        DoMethod(keys_list, MUIM_List_InsertSingle, (ULONG)&keys_rows[i],
                 MUIV_List_Insert_Bottom);
    }
    set(keys_list, MUIA_List_Quiet, FALSE);
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
            case BIND_ROMFILE:
            case BIND_FILE:
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
        if (s->type == BIND_DRAWER || s->type == BIND_ROMFILE || s->type == BIND_FILE) {
            /* string + small "..." button */
            Object *row = MUI_NewObjectB(MUIC_Group,
                                         MUIA_Group_Horiz, TRUE,
                                         MUIA_Group_Spacing, 0,
                                         MUIA_Group_Child, (ULONG)s->obj,
                                         MUIA_Group_Child, (ULONG)s->browse,
                                         TAG_DONE);
            set(s->browse, MUIA_Weight, 0);
            DoMethod(columns, OM_ADDMEMBER, (ULONG)row);
            if (s == rom_settings[ROM_COUNT - 1]) {
                /* after the last ROM row: the model preset, aligned left */
                rom_default_button = make_button(LOC(MSG_ROM_MODEL_DEFAULTS));
                DoMethod(columns, OM_ADDMEMBER, (ULONG)make_label(""));
                DoMethod(columns, OM_ADDMEMBER, (ULONG)MUI_NewObjectB(MUIC_Group,
                         MUIA_Group_Horiz, TRUE,
                         MUIA_Group_Child, (ULONG)rom_default_button,
                         MUIA_Group_Child, (ULONG)make_space(),
                         TAG_DONE));
            }
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
            if (s == drive_type_setting) {
                /* the ROM file of the type, under its cycle */
                drive_rom_text = make_text("");
                DoMethod(columns, OM_ADDMEMBER, (ULONG)make_label(LOC(MSG_DRIVE_ROM_FILE)));
                DoMethod(columns, OM_ADDMEMBER, (ULONG)drive_rom_text);
            }
            if (s == model_setting) {
                /* the model description, under its cycle */
                model_info_text = make_text("");
                DoMethod(columns, OM_ADDMEMBER, (ULONG)make_label(""));
                DoMethod(columns, OM_ADDMEMBER, (ULONG)model_info_text);
            }
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
        /* applies the 4 choices at once, the window stays open */
        input_apply_button = make_button(LOC(MSG_INPUT_APPLY));
        DoMethod(group, OM_ADDMEMBER, (ULONG)MUI_NewObjectB(MUIC_Group,
                 MUIA_Group_Horiz, TRUE,
                 MUIA_Group_Child, (ULONG)input_apply_button,
                 MUIA_Group_Child, (ULONG)make_space(),
                 TAG_DONE));
    }
    if (page == PAGE_DRIVE8) {
        DoMethod(group, OM_ADDMEMBER, (ULONG)make_text(LOC(MSG_DRIVE8_DRAWER_NOTE)));
    }
    if (page == PAGE_KEYBOARD) {
        Object *view = make_keys_list();

        DoMethod(group, OM_ADDMEMBER, (ULONG)make_text(LOC(MSG_KEYBOARD_NOTE)));
        if (view != NULL) {
            DoMethod(group, OM_ADDMEMBER, (ULONG)view);
            /* the list takes the free space */
            return group;
        }
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
static Object *mui_category_list = NULL;
/* signals MUI wants to be woken up for, 0 when nothing to do */
ULONG mui_sigs = 0;
static int mui_win_open = 0;

/* the ROM file rows: a changed file must exist and have the right size,
 * and all of them while ROMs are missing ("no rom" state). Returns 0, or -1
 * after showing which file is wrong (that row is then not applied). */
static int apply_rom_settings(void)
{
    int i, ret = 0;
    int check_all = !c64rom_all_loaded();

    for (i = 0; i < ROM_COUNT; i++) {
        setting_t *s = rom_settings[i];
        STRPTR str = NULL;
        int changed;

        if (s == NULL || s->obj == NULL) {
            continue;
        }
        get(s->obj, MUIA_String_Contents, &str);
        if (str == NULL) {
            continue;
        }
        changed = strcmp((const char *)str, s->initial_string) != 0;
        if (!changed && !check_all) {
            continue;
        }
        if (!rom_file_ok((const char *)str, s->rom_size)) {
            ULONG args[2];

            args[0] = (ULONG)LOC(s->label_msg);
            args[1] = (ULONG)str;
            MUI_RequestA(mui_app, mui_win, 0, NULL, (char *)LOC(MSG_ERROR_OK),
                         (char *)LOC(MSG_ERROR_ROM_FILE), args);
            ret = -1;
            continue;
        }
        if (!changed) {
            /* right file, not loaded yet: mem_load() below */
            continue;
        }
        if (resources_set_string(s->resource, (const char *)str) < 0) {
            log_error(LOG_DEFAULT, "settings: cannot load the ROM %s.", (const char *)str);
            ret = -1;
            continue;
        }
        /* changed and loaded: the new value is the reference now */
        strncpy(s->initial_string, (const char *)str, sizeof s->initial_string - 1);
    }
    /* "no rom" state: load them all again, a file may have been copied
       where the settings already pointed */
    if (check_all && ret == 0 && !c64rom_all_loaded()) {
        mem_load();
    }
    return ret;
}

/* Returns -1 when a ROM file was not accepted: the window stays open */
static void drive_watch_update(void);
static void input_apply(void);

/* the configuration file keeps the drive types set to "none" only for a
 * missing ROM, and the sound turned off only because ahi.device could not
 * be opened: they come back once the ROM or AHI is installed */
static int save_resources(void)
{
    unsigned int types[NUM_DISK_UNITS];
    unsigned int unit;
    int ret;

    for (unit = 0; unit < NUM_DISK_UNITS; unit++) {
        types[unit] = diskunit_context[unit]->type;
        if (drive_rom_missing_type[unit] != 0) {
            /* the resource variable, no setter: only for the save */
            diskunit_context[unit]->type = drive_rom_missing_type[unit];
        }
    }
    sound_save_wanted_begin();
    ret = resources_save(NULL);
    sound_save_wanted_end();
    for (unit = 0; unit < NUM_DISK_UNITS; unit++) {
        diskunit_context[unit]->type = types[unit];
    }
    return ret;
}

static int apply_settings(void)
{
    int page, i, ret;

    /* the files first: a keymap file is then loaded by the keymap choice */
    for (page = 0; page < PAGE_COUNT; page++) {
        for (i = 0; i < settings_count[page]; i++) {
            if (settings[page][i].type == BIND_FILE) {
                setting_from_ui(&settings[page][i]);
            }
        }
    }
    for (page = 0; page < PAGE_COUNT; page++) {
        if (page == PAGE_INPUT) {
            /* together, below */
            continue;
        }
        for (i = 0; i < settings_count[page]; i++) {
            if (settings[page][i].type != BIND_ROMFILE
                    && settings[page][i].type != BIND_FILE) {
                setting_from_ui(&settings[page][i]);
            }
        }
    }
    input_apply();
    ret = apply_rom_settings();
    /* drive 8 type may have changed */
    drive_watch_update();
    /* the keyboard mapping is also in a menu */
    amiga_video_sync_menu();
    /* the keymap may have changed (file, or the built-in one) */
    keys_list_fill();
    return ret;
}

static void model_info_update(void);
static void drive_rom_info_update(void);

static void settings_to_ui(void)
{
    int page, i;

    for (page = 0; page < PAGE_COUNT; page++) {
        for (i = 0; i < settings_count[page]; i++) {
            setting_to_ui(&settings[page][i]);
        }
    }
    keys_list_fill();
    /* the cycles were set without notification */
    model_info_update();
    drive_rom_info_update();
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

    /* in the PAGE_* order */
    pages = MUI_NewObjectB(MUIC_Group,
                           MUIA_Group_PageMode, TRUE,
                           MUIA_Group_Child, (ULONG)page_objs[0],
                           MUIA_Group_Child, (ULONG)page_objs[1],
                           MUIA_Group_Child, (ULONG)page_objs[2],
                           MUIA_Group_Child, (ULONG)page_objs[3],
                           MUIA_Group_Child, (ULONG)page_objs[4],
                           MUIA_Group_Child, (ULONG)page_objs[5],
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
    mui_category_list = list_obj;
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
    for (page = 0; page < ROM_COUNT; page++) {
        if (rom_settings[page] != NULL && rom_settings[page]->browse != NULL) {
            DoMethod(rom_settings[page]->browse, MUIM_Notify, MUIA_Pressed, FALSE,
                     (ULONG)mui_app, 2, MUIM_Application_ReturnID, RID_BROWSE_ROM + page);
        }
    }
    if (file_setting != NULL && file_setting->browse != NULL) {
        DoMethod(file_setting->browse, MUIM_Notify, MUIA_Pressed, FALSE,
                 (ULONG)mui_app, 2, MUIM_Application_ReturnID, RID_BROWSE_FILE);
    }
    if (rom_default_button != NULL) {
        DoMethod(rom_default_button, MUIM_Notify, MUIA_Pressed, FALSE,
                 (ULONG)mui_app, 2, MUIM_Application_ReturnID, RID_ROM_DEFAULT);
    }
    if (model_setting != NULL && model_setting->obj != NULL) {
        DoMethod(model_setting->obj, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
                 (ULONG)mui_app, 2, MUIM_Application_ReturnID, RID_MODEL);
    }
    if (drive_type_setting != NULL && drive_type_setting->obj != NULL) {
        DoMethod(drive_type_setting->obj, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
                 (ULONG)mui_app, 2, MUIM_Application_ReturnID, RID_DRIVE_TYPE);
    }
    for (page = 0; page < 2; page++) {
        if (amiga_port_setting[page] != NULL && amiga_port_setting[page]->obj != NULL) {
            DoMethod(amiga_port_setting[page]->obj, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
                     (ULONG)mui_app, 2, MUIM_Application_ReturnID, RID_INPUT_PORTS);
        }
    }
    if (input_apply_button != NULL) {
        DoMethod(input_apply_button, MUIM_Notify, MUIA_Pressed, FALSE,
                 (ULONG)mui_app, 2, MUIM_Application_ReturnID, RID_INPUT_APPLY);
    }
    return 0;
}

static void close_window(void)
{
    set(mui_win, MUIA_Window_Open, FALSE);
    mui_win_open = 0;
    /* opened from the fullscreen: back to it */
    amiga_video_requester_end();
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

/* "..." of a ROM row: ASL file requester */
static void browse_rom(int index)
{
    struct Window *window = NULL;
    char *path;

    if (index < 0 || index >= ROM_COUNT || rom_settings[index] == NULL) {
        return;
    }
    get(mui_win, MUIA_Window_Window, &window);
    set(mui_app, MUIA_Application_Sleep, TRUE);
    path = amiga_file_request(window, LOC(MSG_REQ_ROM), NULL);
    set(mui_app, MUIA_Application_Sleep, FALSE);
    if (path != NULL) {
        set(rom_settings[index]->obj, MUIA_String_Contents, (ULONG)path);
        lib_free(path);
    }
}

/* "..." of the file row: ASL file requester with its pattern */
static void browse_file(void)
{
    struct Window *window = NULL;
    char *path;

    if (file_setting == NULL) {
        return;
    }
    get(mui_win, MUIA_Window_Window, &window);
    set(mui_app, MUIA_Application_Sleep, TRUE);
    path = amiga_file_request(window, LOC(file_setting->req_msg), file_setting->pattern);
    set(mui_app, MUIA_Application_Sleep, FALSE);
    if (path != NULL) {
        set(file_setting->obj, MUIA_String_Contents, (ULONG)path);
        lib_free(path);
    }
}

/* kernal file name of a kernal revision (the table of c64-resources.c) */
static const char *kernal_rev_name(int rev)
{
    switch (rev) {
        case C64_KERNAL_JAP:  return C64_KERNAL_JAP_NAME;
        case C64_KERNAL_REV1: return C64_KERNAL_REV1_NAME;
        case C64_KERNAL_REV2: return C64_KERNAL_REV2_NAME;
        case C64_KERNAL_REV3: return C64_KERNAL_REV3_NAME;
        case C64_KERNAL_GS64: return C64_KERNAL_GS64_NAME;
        case C64_KERNAL_SX64: return C64_KERNAL_SX64_NAME;
        case C64_KERNAL_4064: return C64_KERNAL_4064_NAME;
        default:              return NULL;
    }
}

/* the line under the model cycle: video standard and SID of the model
 * chosen in it (not applied yet) */
static void model_info_update(void)
{
    const char *video;
    ULONG v = 0;
    int model;

    if (model_setting == NULL || model_setting->obj == NULL || model_info_text == NULL) {
        return;
    }
    get(model_setting->obj, MUIA_Cycle_Active, &v);
    if ((int)v >= model_setting->count) {
        return;
    }
    model = model_setting->values[v];
    switch (c64model_get_video(model)) {
        case MACHINE_SYNC_PAL:     video = "PAL 50 Hz"; break;
        case MACHINE_SYNC_PALN:    video = "PAL-N 50 Hz"; break;
        case MACHINE_SYNC_NTSC:    video = "NTSC 60 Hz"; break;
        case MACHINE_SYNC_NTSCOLD: video = "old NTSC 60 Hz"; break;
        default:                   video = "?"; break;
    }
    snprintf(model_info, sizeof model_info, LOC(MSG_MODEL_INFO), video,
             c64model_get_new_sid(model) > 0 ? "MOS 8580" : "MOS 6581");
    set(model_info_text, MUIA_Text_Contents, (ULONG)model_info);
}

/* cycle entry value of a setting as shown */
static int cycle_value(setting_t *s)
{
    ULONG v = 0;

    get(s->obj, MUIA_Cycle_Active, &v);
    return (int)v < s->count ? s->values[v] : 0;
}

/* an Amiga port type changed: a C64 port driven by an Amiga port now not
 * used drives nothing (shown at once, applied by input_apply()) */
static void input_ports_changed(void)
{
    int c;

    for (c = 0; c < 2; c++) {
        int from;

        if (c64_port_setting[c] == NULL || c64_port_setting[c]->obj == NULL) {
            continue;
        }
        from = cycle_value(c64_port_setting[c]);
        if (from != C64_FROM_NONE
                && cycle_value(amiga_port_setting[from - 1]) == 0) {
            nnset(c64_port_setting[c]->obj, MUIA_Cycle_Active, 0);
        }
    }
}

/* the Input page: the Amiga ports opened again when their type changed
 * (lowlevel.library, analog reader), then each C64 control port given the
 * device of its Amiga port, as a joystick or as paddles */
static void input_apply(void)
{
    int changed = 0;
    int i, c;

    if (amiga_port_setting[0] == NULL || amiga_port_setting[0]->obj == NULL) {
        return;
    }
    for (i = 0; i < 2; i++) {
        int type = cycle_value(amiga_port_setting[i]);
        int current = 0;

        resources_get_int_sprintf("AmigaJoyPort%d", &current, i);
        if (type != current) {
            resources_set_int_sprintf("AmigaJoyPort%d", type, i);
            changed = 1;
        }
    }
    if (changed) {
        amiga_joy_reconfigure();
    }
    for (c = 0; c < 2; c++) {
        int from = cycle_value(c64_port_setting[c]);
        int index = from != C64_FROM_NONE ? amiga_joy_device_index(from - 1) : -1;
        int dev = index >= 0 ? JOYDEV_REALJOYSTICK_MIN + index : JOYDEV_NONE;

        /* each C64 port as the controller plugged: paddles for analog */
        if (index >= 0) {
            resources_set_int_sprintf("JoyPort%dDevice",
                                      amiga_joy_port_is_analog(from - 1)
                                      ? JOYPORT_ID_PADDLES : JOYPORT_ID_JOYSTICK,
                                      c + 1);
        }
        if (resources_set_int_sprintf("JoyDevice%d", dev, c + 1) < 0) {
            log_error(LOG_DEFAULT, "settings: cannot set C64 control port %d.", c + 1);
        }
    }
    /* what is really in use now (an Amiga port may have failed to open) */
    for (i = 0; i < 2; i++) {
        setting_to_ui(amiga_port_setting[i]);
        setting_to_ui(c64_port_setting[i]);
    }
}

/* ROM file resource of a drive type, NULL if none */
static const char *drive_rom_resource(int type)
{
    switch (type) {
        case DRIVE_TYPE_1540:   return "DosName1540";
        case DRIVE_TYPE_1541:   return "DosName1541";
        case DRIVE_TYPE_1541II: return "DosName1541ii";
        case DRIVE_TYPE_1570:   return "DosName1570";
        case DRIVE_TYPE_1571:   return "DosName1571";
        case DRIVE_TYPE_1581:   return "DosName1581";
        case DRIVE_TYPE_2000:   return "DosName2000";
        case DRIVE_TYPE_4000:   return "DosName4000";
        default:                return NULL;
    }
}

/* the line under the drive type cycle: the ROM file the type chosen needs,
 * where it is found, else where it is searched first (PROGDIR:DRIVES/) */
static void drive_rom_info_update(void)
{
    const char *resource, *name = NULL;
    char *found = NULL;
    char path[MAX_PATH_LEN];
    ULONG v = 0;

    if (drive_type_setting == NULL || drive_type_setting->obj == NULL || drive_rom_text == NULL) {
        return;
    }
    get(drive_type_setting->obj, MUIA_Cycle_Active, &v);
    resource = (int)v < drive_type_setting->count
               ? drive_rom_resource(drive_type_setting->values[v]) : NULL;
    if (resource == NULL || resources_get_string(resource, &name) < 0 || name == NULL) {
        drive_rom_info[0] = '\0';
    } else if (sysfile_locate(name, "DRIVES", &found) >= 0 && found != NULL) {
        snprintf(drive_rom_info, sizeof drive_rom_info, "%s %s",
                 found, LOC(MSG_DRIVE_ROM_FOUND));
    } else {
        if (strchr(name, ':') != NULL || strchr(name, '/') != NULL) {
            snprintf(path, sizeof path, "%s", name);
        } else {
            snprintf(path, sizeof path, "PROGDIR:DRIVES/%s", name);
        }
        snprintf(drive_rom_info, sizeof drive_rom_info, "%s %s",
                 path, LOC(MSG_DRIVE_ROM_MISSING));
    }
    if (found != NULL) {
        lib_free(found);
    }
    set(drive_rom_text, MUIA_Text_Contents, (ULONG)drive_rom_info);
}

/* "Set ROM defaults for this model": the ROM rows show the files of the
 * model chosen in the cycle, in the default drawer (applied on Use/Save,
 * checked like a chosen file) */
static void rom_model_defaults(void)
{
    const char *names[ROM_COUNT];
    char path[MAX_PATH_LEN];
    ULONG v = 0;
    int model, i;

    if (model_setting == NULL || model_setting->obj == NULL) {
        return;
    }
    get(model_setting->obj, MUIA_Cycle_Active, &v);
    if ((int)v >= model_setting->count) {
        return;
    }
    model = model_setting->values[v];
    names[0] = kernal_rev_name(c64model_get_kernal_rev(model));
    names[1] = C64_BASIC_NAME;
    names[2] = c64model_get_chargen_name(model);
    for (i = 0; i < ROM_COUNT; i++) {
        if (names[i] == NULL || rom_settings[i] == NULL || rom_settings[i]->obj == NULL) {
            continue;
        }
        snprintf(path, sizeof path, "%s%s", ROM_DEFAULT_DRAWER, names[i]);
        set(rom_settings[i]->obj, MUIA_String_Contents, (ULONG)path);
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
    /* from the fullscreen: the Workbench (MUI window screen) in front */
    amiga_video_requester_begin();
    set(mui_win, MUIA_Window_Open, TRUE);
    mui_win_open = 1;
    AMIGA_TRACE(("settings window open"));

    /* first input round: gives the signals MUI waits for */
    amiga_mui_handle_events();
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
                if (apply_settings() < 0) {
                    /* a ROM file to fix: the window stays open */
                    break;
                }
                if (save_resources() < 0) {
                    log_error(LOG_DEFAULT, "settings: cannot save the configuration file.");
                }
                close_window();
                return;
            case RID_USE:
                if (apply_settings() < 0) {
                    break;
                }
                close_window();
                return;
            case RID_CANCEL:
            case MUIV_Application_ReturnID_Quit:
                close_window();
                return;
            case RID_BROWSE:
                browse_drawer();
                break;
            case RID_ROM_DEFAULT:
                rom_model_defaults();
                break;
            case RID_MODEL:
                model_info_update();
                break;
            case RID_DRIVE_TYPE:
                drive_rom_info_update();
                break;
            case RID_INPUT_PORTS:
                input_ports_changed();
                break;
            case RID_INPUT_APPLY:
                input_apply();
                break;
            case RID_BROWSE_FILE:
                browse_file();
                break;
            case RID_BROWSE_ROM:
            case RID_BROWSE_ROM + 1:
            case RID_BROWSE_ROM + 2:
                browse_rom((int)(rid - RID_BROWSE_ROM));
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

/* the startup report: Kernal, BASIC, character ROM, (empty), drive 8 ROM,
 * (empty), keymap file. Returns the number of lines; *state gets a bit per
 * missing file, to redraw only when it changes. */
static int startup_report(amiga_report_line_t *lines, unsigned int *state)
{
    static const ULONG rom_labels[ROM_COUNT] = {
        MSG_ROM_KERNAL, MSG_ROM_BASIC, MSG_ROM_CHARGEN
    };
    int ok[ROM_COUNT];
    int drive_type = DRIVE_TYPE_NONE;
    int n = 0, i;

    *state = 0;
    c64rom_get_loaded(&ok[0], &ok[1], &ok[2]);
    for (i = 0; i < ROM_COUNT; i++) {
        lines[n].label = LOC(rom_labels[i]);
        lines[n].value = LOC(ok[i] ? MSG_REPORT_OK : MSG_REPORT_NOT_FOUND);
        lines[n].bad = !ok[i];
        *state |= ok[i] ? 0 : (1U << i);
        n++;
    }

    memset(&lines[n++], 0, sizeof lines[0]);
    resources_get_int("Drive8Type", &drive_type);
    lines[n].label = LOC(MSG_REPORT_DRIVE8_ROM);
    if (drive_rom_missing_type[0] != 0) {
        /* drive_init() set it to "none": its ROM is missing */
        lines[n].value = LOC(MSG_REPORT_NOT_FOUND);
        lines[n].bad = 1;
        *state |= 1U << 3;
    } else if (drive_type == DRIVE_TYPE_NONE) {
        lines[n].value = LOC(MSG_REPORT_NO_DRIVE);
        lines[n].bad = 0;
    } else {
        int drive_ok = iecrom_check_loaded((unsigned int)drive_type) >= 0;

        lines[n].value = LOC(drive_ok ? MSG_REPORT_OK : MSG_REPORT_NOT_FOUND);
        lines[n].bad = !drive_ok;
        *state |= drive_ok ? 0 : (1U << 3);
    }
    n++;

    /* information only: without a file, the built-in keymap is used */
    memset(&lines[n++], 0, sizeof lines[0]);
    lines[n].label = LOC(MSG_REPORT_KEYMAP);
    lines[n].bad = 0;
    if (!keyboard_keymap_loaded()) {
        lines[n].value = LOC(MSG_REPORT_NOT_FOUND);
        lines[n].bad = 1;
    } else if (keyboard_keymap_builtin()) {
        lines[n].value = LOC(MSG_REPORT_BUILTIN);
    } else {
        lines[n].value = LOC(MSG_REPORT_OK);
    }
    n++;
    return n;
}

/* the C64 talks to the bus (ATN) while no drive is on it: drive 8 off for
 * its missing ROM is told. Without true drive emulation (also changed by
 * the drawer menu and the autostart), the virtual drive (kernal traps)
 * needs no drive ROM. */
static void drive8_no_rom_used(void)
{
    int tde = 0;

    if (drive_rom_missing_type[0] == 0) {
        return;
    }
    resources_get_int("Drive8TrueEmulation", &tde);
    if (tde) {
        /* about 6 seconds */
        amiga_video_show_message(LOC(MSG_DRIVE8_NO_ROM_USED), 300);
    }
}

/* the hook decides itself: the missing ROM state may change anytime */
static void drive_watch_update(void)
{
    iecbus_atn_hook = drive8_no_rom_used;
}

/* the emulation can start: the ROMs are loaded (a missing drive ROM is
 * accepted, a missing keymap file has the built-in one) */
static int startup_files_ok(void)
{
    return c64rom_all_loaded();
}

/** \brief  "No rom" state, before the CPU starts
 *
 * A ROM was not found at startup: the emulator screen shows which files
 * are found (drive ROM and keymap file as information), the settings window
 * opens on the Machine page, and the UI is handled (menus, window, settings,
 * CTRL-C) until the ROMs chosen there are all loaded. The report follows
 * each change. The CPU then starts as usual, from the reset.
 */
void amiga_wait_for_roms(void)
{
    amiga_report_line_t lines[AMIGA_REPORT_LINES_MAX];
    unsigned int state, shown_state;
    int count;

    drive_watch_update();
    if (startup_files_ok()) {
        return;
    }
    log_warning(LOG_DEFAULT, "ROMs missing: choose them in Settings.");
    count = startup_report(lines, &shown_state);
    amiga_video_show_report(lines, count);
    amiga_settings_open();
    if (mui_category_list != NULL) {
        set(mui_category_list, MUIA_List_Active, PAGE_MACHINE);
    }
    while (!startup_files_ok()) {
        amiga_wait_events();
        count = startup_report(lines, &state);
        if (state != shown_state) {
            shown_state = state;
            amiga_video_show_report(lines, count);
        }
    }
    amiga_video_show_report(NULL, 0);
    drive_watch_update();
    log_message(LOG_DEFAULT, "ROMs loaded: the emulation starts.");
}
