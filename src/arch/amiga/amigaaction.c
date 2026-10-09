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
#include "amigabasic.h"
#include "amigamachine.h"
#include "screenshot.h"
#include "kbd.h"
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
#include "interrupt.h"
#include "log.h"
#include "sound.h"
#include "ui.h"
#include "util.h"
#include "diskimage.h"
#include "vdrive-internal.h"
#include "cbmdos.h"
#include "charset.h"
#include "archdep_sanitize_filename.h"
#include "imagecontents.h"
#include "machine-drive.h"
#include "mem.h"
#include "vdrive.h"
#include "vdrive-command.h"
#include "vdrive-dir.h"
#include "vdrive-iec.h"

#include <string.h>
#include <intuition/intuition.h>
#include <proto/intuition.h>

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
    char *path;

    /* from the fullscreen: the requester screen in front, then back */
    amiga_video_requester_begin();
    path = amiga_file_request(amiga_video_window(), LOC(title_msg), pattern);
    amiga_video_requester_end();

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

/* a requester with \a path in its text, on the requester screen in
 * fullscreen. Returns the gadget chosen (1: first ... 0: last) */
static LONG path_request(ULONG text_msg, ULONG gadgets_msg, const char *path)
{
    struct EasyStruct es;
    ULONG args[1];
    LONG ret;

    es.es_StructSize = sizeof es;
    es.es_Flags = 0;
    es.es_Title = (UBYTE *)amiga_machine.title;
    es.es_TextFormat = (UBYTE *)LOC(text_msg);
    es.es_GadgetFormat = (UBYTE *)LOC(gadgets_msg);
    args[0] = (ULONG)path;
    amiga_video_requester_begin();
    ret = EasyRequestArgs(amiga_video_window(), &es, NULL, args);
    amiga_video_requester_end();
    vsync_suspend_speed_eval();
    return ret;
}

/* name of a new disk from its file: "NAME,01", up to 16 C64 characters in
 * upper case (PETSCII letters), without the characters CBM DOS reserves */
static void new_disk_name(const char *path, char *name, size_t size)
{
    const char *base = path;
    const char *p;
    size_t n = 0;

    for (p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == ':') {
            base = p + 1;
        }
    }
    for (p = base; *p != '\0' && n < 16 && n + 4 < size; p++) {
        char c = *p;

        if (util_strcasecmp(p, ".d64") == 0) {
            break;
        }
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        } else if (c == ',' || c == ':' || c == '"' || c == '*' || c == '?'
                   || c == '=' || (unsigned char)c < 0x20 || (unsigned char)c > 0x7e) {
            c = ' ';
        }
        name[n++] = c;
    }
    if (n == 0) {
        name[n++] = 'E';
    }
    strcpy(name + n, ",01");
}

/* a new formatted .d64, then in drive 8 */
static BOOL Action_CreateDisk8(void)
{
    char name[24];
    char *path;
    size_t len;
    BOOL ok = FALSE;

    amiga_video_requester_begin();
    path = amiga_file_save_request(amiga_video_window(), LOC(MSG_REQ_CREATE_DISK8), "#?.d64");
    amiga_video_requester_end();
    vsync_suspend_speed_eval();
    if (path == NULL) {
        return FALSE;
    }
    /* ".d64" or ".D64" at the end, else added */
    len = strlen(path);
    if (len < 4 || util_strcasecmp(path + len - 4, ".d64") != 0) {
        char *named = util_concat(path, ".d64", NULL);

        lib_free(path);
        path = named;
    }
    if (util_file_exists(path)
            && path_request(MSG_CONFIRM_REPLACE, MSG_REPLACE_CANCEL, path) != 1) {
        lib_free(path);
        return FALSE;
    }
    new_disk_name(path, name, sizeof name);
    if (vdrive_internal_create_format_disk_image(path, name, DISK_IMAGE_TYPE_D64) < 0) {
        log_error(LOG_DEFAULT, "cannot create the disk image `%s'.", path);
        path_request(MSG_ERROR_CREATE_DISK, MSG_ERROR_OK, path);
    } else if (file_system_attach_disk(8, 0, path) < 0) {
        log_error(LOG_DEFAULT, "cannot attach `%s' to drive 8.", path);
    } else {
        log_message(LOG_DEFAULT, "new disk `%s' (%s) in drive 8.", path, name);
        ok = TRUE;
    }
    lib_free(path);
    return ok;
}

/* a file of the disk: more bytes than any disk holds is a cyclic chain */
#define EXTRACT_FILE_MAX (2 * 1024 * 1024)

/* AmigaDOS path of \a name in \a drawer (lib_malloc'd) */
static char *drawer_file(const char *drawer, const char *name)
{
    size_t len = strlen(drawer);

    if (len == 0 || drawer[len - 1] == ':' || drawer[len - 1] == '/') {
        return util_concat(drawer, name, NULL);
    }
    return util_concat(drawer, "/", name, NULL);
}

/* one directory entry of the disk to a file of the drawer ("name.prg",
 * ".seq" or ".usr"), 0 if done */
static int extract_file(vdrive_t *vdrive, const uint8_t *slot, const char *drawer)
{
    uint8_t type = slot[SLOT_TYPE_OFFSET] & 7;
    uint8_t cbm_name[IMAGE_CONTENTS_FILE_NAME_LEN + 3];
    char name[IMAGE_CONTENTS_FILE_NAME_LEN + 5];
    unsigned int len;
    long size = 0;
    char *path;
    FILE *fd;
    uint8_t c;
    int status;

    memset(name, 0, sizeof name);
    for (len = 0; len < IMAGE_CONTENTS_FILE_NAME_LEN; len++) {
        if (slot[SLOT_NAME_OFFSET + len] == 0xa0) {
            break;
        }
        cbm_name[len] = slot[SLOT_NAME_OFFSET + len];
        name[len] = (char)cbm_name[len];
    }
    /* the type in the name to open SEQ and USR files (as c1541) */
    if (type == CBMDOS_FT_SEQ) {
        cbm_name[len++] = ',';
        cbm_name[len++] = 'S';
    } else if (type == CBMDOS_FT_USR) {
        cbm_name[len++] = ',';
        cbm_name[len++] = 'U';
    }
    charset_petconvstring((uint8_t *)name, CONVERT_TO_ASCII);
    archdep_sanitize_filename(name);
    if (name[0] == '\0') {
        strcpy(name, "noname");
    }
    /* the C64 file type as extension, for the Amiga side */
    strcat(name, type == CBMDOS_FT_SEQ ? ".seq" : type == CBMDOS_FT_USR ? ".usr" : ".prg");

    if (vdrive_iec_open(vdrive, cbm_name, len, 0, NULL) != SERIAL_OK) {
        log_error(LOG_DEFAULT, "extract: cannot open `%s' on the disk.", name);
        return -1;
    }
    path = drawer_file(drawer, name);
    fd = fopen(path, MODE_WRITE);
    if (fd == NULL) {
        log_error(LOG_DEFAULT, "extract: cannot create `%s'.", path);
        lib_free(path);
        vdrive_iec_close(vdrive, 0);
        return -1;
    }
    do {
        status = vdrive_iec_read(vdrive, &c, 0);
        fputc(c, fd);
    } while (status == SERIAL_OK && ++size < EXTRACT_FILE_MAX);
    vdrive_iec_close(vdrive, 0);
    if (fclose(fd) != 0 || size >= EXTRACT_FILE_MAX) {
        log_error(LOG_DEFAULT, "extract: cannot write `%s'.", path);
        lib_free(path);
        return -1;
    }
    lib_free(path);
    return 0;
}

/* the closed PRG, SEQ and USR files of the disk to \a drawer, along the
 * directory sectors as c1541 "extract" */
static void extract_files(vdrive_t *vdrive, const char *drawer, long *done, long *errors)
{
    const unsigned int channel = 2;
    unsigned int track = vdrive->Dir_Track;
    unsigned int sector = vdrive->Dir_Sector;
    unsigned int sectors = 0;

    if (vdrive_iec_open(vdrive, (const uint8_t *)"#", 1, channel, NULL) != SERIAL_OK) {
        (*errors)++;
        return;
    }
    /* a directory has less sectors than this: else it is a cyclic chain */
    while (sectors++ < 256) {
        uint8_t dirsector[256];
        char *cmd = lib_msprintf("B-R:%u 0 %u %u", channel, track, sector);
        int res = vdrive_command_execute(vdrive, (uint8_t *)cmd, (unsigned int)strlen(cmd));
        int i;

        lib_free(cmd);
        if (res != CBMDOS_IPE_OK) {
            (*errors)++;
            break;
        }
        /* the channel buffer is used again to read the files */
        memcpy(dirsector, vdrive->buffers[channel].buffer, sizeof dirsector);
        for (i = 0; i < 256; i += SLOT_SIZE) {
            uint8_t type = dirsector[i + SLOT_TYPE_OFFSET];

            if (((type & 7) == CBMDOS_FT_PRG || (type & 7) == CBMDOS_FT_SEQ
                    || (type & 7) == CBMDOS_FT_USR) && (type & CBMDOS_FT_CLOSED)) {
                if (extract_file(vdrive, dirsector + i, drawer) == 0) {
                    (*done)++;
                } else {
                    (*errors)++;
                }
            }
        }
        if (dirsector[0] == 0) {
            break;
        }
        track = dirsector[0];
        sector = dirsector[1];
    }
    vdrive_iec_close(vdrive, channel);
}

/* the files of the disk image in drive 8 to an Amiga drawer */
static BOOL Action_ExtractDisk8(void)
{
    const char *image = file_system_get_disk_name(8, 0);
    char *drawer;
    vdrive_t *vdrive;
    long counts[3];

    if (image == NULL) {
        path_request(MSG_ERROR_NO_DISK8, MSG_ERROR_OK, "");
        return FALSE;
    }
    amiga_video_requester_begin();
    drawer = amiga_drawer_request(amiga_video_window(), LOC(MSG_REQ_EXTRACT_DISK8), NULL);
    amiga_video_requester_end();
    vsync_suspend_speed_eval();
    if (drawer == NULL) {
        return FALSE;
    }
    /* what the true drive wrote is in the image file first */
    machine_drive_flush();
    /* its own read-only access: the drive 8 channels are the C64's */
    vdrive = vdrive_internal_open_fsimage(image, 1);
    if (vdrive == NULL) {
        log_error(LOG_DEFAULT, "extract: cannot read `%s'.", image);
        path_request(MSG_ERROR_READ_DISK, MSG_ERROR_OK, image);
        lib_free(drawer);
        return FALSE;
    }
    counts[0] = counts[1] = 0;
    extract_files(vdrive, drawer, &counts[0], &counts[1]);
    vdrive_internal_close_disk_image(vdrive);
    log_message(LOG_DEFAULT, "extract: %ld file(s), %ld error(s) from `%s' to `%s'.",
                counts[0], counts[1], image, drawer);

    {
        struct EasyStruct es;

        counts[2] = (long)drawer;
        es.es_StructSize = sizeof es;
        es.es_Flags = 0;
        es.es_Title = (UBYTE *)amiga_machine.title;
        es.es_TextFormat = (UBYTE *)LOC(MSG_EXTRACT_DONE);
        es.es_GadgetFormat = (UBYTE *)LOC(MSG_ERROR_OK);
        amiga_video_requester_begin();
        EasyRequestArgs(amiga_video_window(), &es, NULL, counts);
        amiga_video_requester_end();
        vsync_suspend_speed_eval();
    }
    lib_free(drawer);
    return counts[1] == 0;
}

/* the BASIC program in memory: from the program start (TXTTAB, $2b/$2c)
 * to the variables start (VARTAB, $2d/$2e). FALSE, told, if there is none. */
static BOOL basic_program_range(unsigned int *start, unsigned int *end)
{
    *start = mem_ram[0x2b] | (mem_ram[0x2c] << 8);
    *end = mem_ram[0x2d] | (mem_ram[0x2e] << 8);

    /* NEW leaves 2 zero bytes; a machine code program may have moved the
       pointers anywhere */
    if (*start < 0x0400 || *end <= *start + 2 || *end > amiga_machine.basic_top) {
        path_request(MSG_ERROR_NO_BASIC, MSG_ERROR_OK, "");
        return FALSE;
    }
    return TRUE;
}

/* save requester for a file ending with ext (".prg"), added if missing,
 * replacing an existing file confirmed. NULL if cancelled, else lib_free(). */
static char *basic_save_path(ULONG title_msg, const char *pattern, const char *ext)
{
    char *path;
    size_t len, ext_len = strlen(ext);

    amiga_video_requester_begin();
    path = amiga_file_save_request(amiga_video_window(), LOC(title_msg), pattern);
    amiga_video_requester_end();
    vsync_suspend_speed_eval();
    if (path == NULL) {
        return NULL;
    }
    /* ".prg" or ".PRG" at the end, else added */
    len = strlen(path);
    if (len < ext_len || util_strcasecmp(path + len - ext_len, ext) != 0) {
        char *named = util_concat(path, ext, NULL);

        lib_free(path);
        path = named;
    }
    if (util_file_exists(path)
            && path_request(MSG_CONFIRM_REPLACE, MSG_REPLACE_CANCEL, path) != 1) {
        lib_free(path);
        return NULL;
    }
    return path;
}

/* the BASIC program in memory to a .prg file, as SAVE"NAME",8 writes it:
 * the load address, then the bytes from the program start (TXTTAB, $2b/$2c)
 * to the variables start (VARTAB, $2d/$2e). LOAD or autostart it back. */
static BOOL Action_SaveBasic(void)
{
    unsigned int start, end;
    char *path;
    FILE *fd;
    BOOL ok;

    if (!basic_program_range(&start, &end)) {
        return FALSE;
    }
    path = basic_save_path(MSG_REQ_SAVE_BASIC, "#?.prg", ".prg");
    if (path == NULL) {
        return FALSE;
    }
    fd = fopen(path, MODE_WRITE);
    ok = fd != NULL
         && fputc((int)(start & 0xff), fd) != EOF
         && fputc((int)(start >> 8), fd) != EOF
         && fwrite(mem_ram + start, 1, end - start, fd) == end - start;
    if (fd != NULL && fclose(fd) != 0) {
        ok = FALSE;
    }
    if (ok) {
        log_message(LOG_DEFAULT, "BASIC program $%04x-$%04x saved to `%s'.", start, end, path);
    } else {
        log_error(LOG_DEFAULT, "cannot save the BASIC program to `%s'.", path);
        path_request(MSG_ERROR_SAVE_BASIC, MSG_ERROR_OK, path);
    }
    lib_free(path);
    return ok;
}

/* Display menu: the emulator screen as an IFF ILBM picture (VICE IFF
 * screenshot driver: the canvas at its size, the borders as VICE draws
 * them, the machine palette) */
static BOOL Action_SaveScreenshot(void)
{
    struct video_canvas_s *canvas = amiga_video_canvas();
    char *path;
    size_t len;
    BOOL ok;

    if (canvas == NULL) {
        return FALSE;
    }
    amiga_video_requester_begin();
    path = amiga_file_save_request(amiga_video_window(), LOC(MSG_REQ_SAVE_SCREENSHOT), "#?.iff");
    amiga_video_requester_end();
    vsync_suspend_speed_eval();
    if (path == NULL) {
        return FALSE;
    }
    /* ".iff" or ".IFF" at the end, else added */
    len = strlen(path);
    if (len < 4 || util_strcasecmp(path + len - 4, ".iff") != 0) {
        char *named = util_concat(path, ".iff", NULL);

        lib_free(path);
        path = named;
    }
    if (util_file_exists(path)
            && path_request(MSG_CONFIRM_REPLACE, MSG_REPLACE_CANCEL, path) != 1) {
        lib_free(path);
        return FALSE;
    }
    ok = screenshot_save("IFF", path, canvas) == 0;
    if (ok) {
        log_message(LOG_DEFAULT, "Screenshot saved to `%s'.", path);
    } else {
        log_error(LOG_DEFAULT, "cannot save the screenshot to `%s'.", path);
        path_request(MSG_ERROR_SAVE_SCREENSHOT, MSG_ERROR_OK, path);
    }
    lib_free(path);
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
    amiga_video_requester_begin();
    path = amiga_drawer_request(amiga_video_window(), LOC(MSG_REQ_DRAWER8), current);
    amiga_video_requester_end();
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

/* ------------------------------------------------------------------------- */
/* Keyboard menu: symbolic or positional mapping, changed at once */

static BOOL Action_KeyboardSymbolic(void) { amiga_kbd_set_symbolic(1); return TRUE; }
static BOOL Action_KeyboardPositional(void) { amiga_kbd_set_symbolic(0); return TRUE; }
static int Checked_KeyboardSymbolic(void) { return amiga_kbd_get_symbolic() ? 1 : 0; }
static int Checked_KeyboardPositional(void) { return amiga_kbd_get_symbolic() ? 0 : 1; }

/* ------------------------------------------------------------------------- */
/* Snapshot menu: the whole machine state in a .vsf file */

#define PATTERN_SNAPSHOT "#?.vsf"

/* a snapshot error in a requester (on the requester screen in fullscreen) */
static void snapshot_error(ULONG format_msg, const char *path)
{
    struct EasyStruct es;
    ULONG args[1];

    es.es_StructSize = sizeof es;
    es.es_Flags = 0;
    es.es_Title = (UBYTE *)amiga_machine.title;
    es.es_TextFormat = (UBYTE *)LOC(format_msg);
    es.es_GadgetFormat = (UBYTE *)LOC(MSG_ERROR_OK);
    args[0] = (ULONG)path;
    log_error(LOG_DEFAULT, "snapshot: cannot %s `%s'.",
              format_msg == MSG_ERROR_SNAPSHOT_LOAD ? "load" : "save", path);
    amiga_video_requester_begin();
    EasyRequestArgs(amiga_video_window(), &es, NULL, args);
    amiga_video_requester_end();
}

/* CPU traps: between two instructions, the machine state is consistent.
 * data: the lib_malloc'd file name, freed here. */
static void snapshot_load_trap(uint16_t addr, void *data)
{
    char *path = (char *)data;

    vsync_suspend_speed_eval();
    sound_suspend();
    if (machine_read_snapshot(path, 0) < 0) {
        snapshot_error(MSG_ERROR_SNAPSHOT_LOAD, path);
    }
    lib_free(path);
}

static void snapshot_save_trap(uint16_t addr, void *data)
{
    char *path = (char *)data;

    vsync_suspend_speed_eval();
    sound_suspend();
    /* the attached disk images in the snapshot (their state is part of the
     * machine), not the ROMs (always there) */
    if (machine_write_snapshot(path, 0, 1, 0) < 0) {
        snapshot_error(MSG_ERROR_SNAPSHOT_SAVE, path);
    }
    lib_free(path);
}

/* at the next instruction, or now when paused (the CPU is already stopped
 * between two instructions) */
static void snapshot_run(void (*trap)(uint16_t, void *), char *path)
{
    if (ui_pause_active()) {
        trap(0, path);
    } else {
        interrupt_maincpu_trigger_trap(trap, path);
    }
}

static BOOL Action_SnapshotLoad(void)
{
    char *path = request_file(MSG_REQ_SNAPSHOT_LOAD, PATTERN_SNAPSHOT);

    if (path == NULL) {
        return FALSE;
    }
    snapshot_run(snapshot_load_trap, path);
    return TRUE;
}

static BOOL Action_SnapshotSave(void)
{
    char *path;
    size_t len;

    amiga_video_requester_begin();
    path = amiga_file_save_request(amiga_video_window(), LOC(MSG_REQ_SNAPSHOT_SAVE),
                                   PATTERN_SNAPSHOT);
    amiga_video_requester_end();
    vsync_suspend_speed_eval();
    if (path == NULL) {
        return FALSE;
    }
    /* the requester pattern only shows .vsf files: add the extension */
    len = strlen(path);
    if (len < 4 || util_strcasecmp(path + len - 4, ".vsf") != 0) {
        char *named = util_concat(path, ".vsf", NULL);

        lib_free(path);
        path = named;
    }
    snapshot_run(snapshot_save_trap, path);
    return TRUE;
}

/* BASIC menu: the program in memory as a .bas UTF-8 text file, as LIST
 * shows it (amigabasic.c) */
static BOOL Action_SaveBas(void)
{
    unsigned int start, end;
    char *path;
    FILE *fd;
    int lines = -1;

    if (!basic_program_range(&start, &end)) {
        return FALSE;
    }
    path = basic_save_path(MSG_REQ_SAVE_BAS, "#?.bas", ".bas");
    if (path == NULL) {
        return FALSE;
    }
    fd = fopen(path, MODE_WRITE);
    if (fd != NULL) {
        lines = amiga_basic_write_utf8(mem_ram, start, end, amiga_machine.basic_dialect, fd);
        if (fclose(fd) != 0) {
            lines = -1;
        }
    }
    if (lines >= 0) {
        log_message(LOG_DEFAULT, "BASIC program, %d lines, saved as text to `%s'.", lines, path);
    } else {
        log_error(LOG_DEFAULT, "cannot save the BASIC program to `%s'.", path);
        path_request(MSG_ERROR_SAVE_BASIC, MSG_ERROR_OK, path);
    }
    lib_free(path);
    return lines >= 0;
}

/* a message requester with a text built here (no format in it) */
static void text_request(const char *text)
{
    struct EasyStruct es;
    ULONG args[1];

    es.es_StructSize = sizeof es;
    es.es_Flags = 0;
    es.es_Title = (UBYTE *)amiga_machine.title;
    es.es_TextFormat = (UBYTE *)"%s";
    es.es_GadgetFormat = (UBYTE *)LOC(MSG_ERROR_OK);
    args[0] = (ULONG)text;
    amiga_video_requester_begin();
    EasyRequestArgs(amiga_video_window(), &es, NULL, args);
    amiga_video_requester_end();
    vsync_suspend_speed_eval();
}

/* one problem of a .bas file as a line of text */
static void bas_issue_text(const amiga_basic_issue_t *issue, char *out, size_t size)
{
    static const ULONG kind_msg[AMIGA_BAS_KIND_COUNT] = {
        MSG_BAS_ERR_UTF8, MSG_BAS_ERR_CHAR, MSG_BAS_ERR_CONTROL, MSG_BAS_ERR_BRACE,
        MSG_BAS_ERR_NUMBER, MSG_BAS_ERR_TOO_LONG, MSG_BAS_ERR_MEMORY, MSG_BAS_ERR_NO_LINES,
        MSG_BAS_WARN_DUPLICATE, MSG_BAS_WARN_EMPTY
    };
    size_t n = 0;

    if (issue->file_line > 0 && issue->basic_line >= 0) {
        n = (size_t)snprintf(out, size, LOC(MSG_BAS_AT_LINE),
                             (long)issue->file_line, issue->basic_line);
    } else if (issue->file_line > 0) {
        n = (size_t)snprintf(out, size, LOC(MSG_BAS_AT_FILE_LINE), (long)issue->file_line);
    }
    if (n >= size) {
        return;
    }
    if (issue->kind == AMIGA_BAS_ERR_CONTROL || issue->kind == AMIGA_BAS_ERR_BRACE) {
        snprintf(out + n, size - n, LOC(kind_msg[issue->kind]), issue->detail);
    } else {
        snprintf(out + n, size - n, LOC(kind_msg[issue->kind]), issue->value, issue->value2);
    }
}

/* the report of a .bas load, in the log and in a requester */
static void bas_report(const amiga_basic_report_t *report)
{
    char text[1600];
    char line[160];
    size_t n;
    int i;

    if (report->errors > 0) {
        snprintf(text, sizeof text, LOC(MSG_BAS_NOT_LOADED), (long)report->errors);
    } else {
        snprintf(text, sizeof text, LOC(MSG_BAS_WARNINGS), (long)report->lines,
                 (long)report->warnings);
    }
    for (i = 0; i < report->count; i++) {
        bas_issue_text(&report->issues[i], line, sizeof line);
        log_message(LOG_DEFAULT, "load .bas: %s", line);
        n = strlen(text);
        snprintf(text + n, sizeof text - n, "\n%s", line);
    }
    if (report->errors + report->warnings > report->count) {
        n = strlen(text);
        snprintf(line, sizeof line, LOC(MSG_BAS_MORE),
                 (long)(report->errors + report->warnings - report->count));
        snprintf(text + n, sizeof text - n, "\n%s", line);
    }
    text_request(text);
}

/* BASIC menu: a .bas UTF-8 text tokenized into memory, as typing its lines
 * would (amigabasic.c). Nothing changes if the text has errors, the report
 * says where. The program replaces the one in memory, like LOAD. */
static BOOL Action_LoadBas(void)
{
    unsigned int start = mem_ram[0x2b] | (mem_ram[0x2c] << 8);
    /* MEMSIZ, top of the BASIC memory (16 KB on a C16) */
    unsigned int limit = mem_ram[0x37] | (mem_ram[0x38] << 8);
    amiga_basic_report_t report;
    char *path, *text = NULL;
    long size = -1;
    FILE *fd;
    int ok;

    if (limit > amiga_machine.basic_top) {
        limit = amiga_machine.basic_top;
    }
    if (start < 0x0400 || start + 2 >= limit) {
        path_request(MSG_ERROR_BASIC_MEMORY, MSG_ERROR_OK, "");
        return FALSE;
    }
    amiga_video_requester_begin();
    path = amiga_file_request(amiga_video_window(), LOC(MSG_REQ_LOAD_BAS), "#?.bas");
    amiga_video_requester_end();
    vsync_suspend_speed_eval();
    if (path == NULL) {
        return FALSE;
    }
    fd = fopen(path, MODE_READ);
    if (fd != NULL && fseek(fd, 0, SEEK_END) == 0) {
        size = ftell(fd);
        /* more than 1 MB of text cannot fit in 64 KB */
        if (size >= 0 && size <= 1024 * 1024 && fseek(fd, 0, SEEK_SET) == 0) {
            text = lib_malloc((size_t)size + 1);
            if (fread(text, 1, (size_t)size, fd) != (size_t)size) {
                size = -1;
            }
        } else {
            size = -1;
        }
    }
    if (fd != NULL) {
        fclose(fd);
    }
    if (size < 0) {
        log_error(LOG_DEFAULT, "cannot read the BASIC text `%s'.", path);
        path_request(MSG_ERROR_READ_BAS, MSG_ERROR_OK, path);
        if (text != NULL) {
            lib_free(text);
        }
        lib_free(path);
        return FALSE;
    }

    ok = amiga_basic_read_utf8(text, (unsigned long)size, amiga_machine.basic_dialect,
                               mem_ram, start, limit, &report) == 0;
    lib_free(text);
    if (ok) {
        static char loaded[64];
        int i;

        /* VARTAB, ARYTAB, STREND after the program: no variables, as after
         * LOAD and CLR */
        for (i = 0x2d; i <= 0x31; i += 2) {
            mem_ram[i] = (uint8_t)(report.end & 0xff);
            mem_ram[i + 1] = (uint8_t)(report.end >> 8);
        }
        log_message(LOG_DEFAULT, "BASIC program, %d lines, loaded from `%s' ($%04x-$%04x).",
                    report.lines, path, start, report.end);
        snprintf(loaded, sizeof loaded, LOC(MSG_BAS_LOADED), (long)report.lines);
        /* about 3 seconds */
        amiga_video_show_message(loaded, 150);
    } else {
        log_error(LOG_DEFAULT, "BASIC text `%s' not loaded: %d error(s).", path, report.errors);
    }
    if (report.errors > 0 || report.warnings > 0) {
        bas_report(&report);
    }
    lib_free(path);
    return ok;
}

static BOOL Action_About(void)
{
    amiga_about_open();
    return TRUE;
}

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
    /* AMIGA_ACTION_BORDERS_NONE    */ { Action_BordersNone,   Checked_BordersNone,   MSG_BORDERS_NONE,    NULL },
    /* AMIGA_ACTION_SNAPSHOT_LOAD   */ { Action_SnapshotLoad,  NULL,                  MSG_SNAPSHOT_LOAD,   NULL },
    /* AMIGA_ACTION_SNAPSHOT_SAVE   */ { Action_SnapshotSave,  NULL,                  MSG_SNAPSHOT_SAVE,   NULL },
    /* AMIGA_ACTION_KEYBOARD_SYMBOLIC   */ { Action_KeyboardSymbolic,   Checked_KeyboardSymbolic,   MSG_KEYBOARD_SYMBOLIC,   NULL },
    /* AMIGA_ACTION_KEYBOARD_POSITIONAL */ { Action_KeyboardPositional, Checked_KeyboardPositional, MSG_KEYBOARD_POSITIONAL, NULL },
    /* AMIGA_ACTION_CREATE_DISK8    */ { Action_CreateDisk8,   NULL,                  MSG_CREATE_DISK8,    NULL },
    /* AMIGA_ACTION_EXTRACT_DISK8   */ { Action_ExtractDisk8,  NULL,                  MSG_EXTRACT_DISK8,   NULL },
    /* AMIGA_ACTION_SAVE_BASIC      */ { Action_SaveBasic,     NULL,                  MSG_SAVE_BASIC,      NULL },
    /* AMIGA_ACTION_SAVE_SCREENSHOT */ { Action_SaveScreenshot, NULL,                 MSG_SAVE_SCREENSHOT, NULL },
    /* AMIGA_ACTION_SAVE_BAS        */ { Action_SaveBas,       NULL,                  MSG_SAVE_BAS,        NULL },
    /* AMIGA_ACTION_LOAD_BAS        */ { Action_LoadBas,       NULL,                  MSG_LOAD_BAS,        NULL },
    /* AMIGA_ACTION_ABOUT           */ { Action_About,         NULL,                  MSG_ABOUT,           NULL }
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
