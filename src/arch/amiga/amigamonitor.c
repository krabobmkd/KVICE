/** \file   amigamonitor.c
 * \brief   AmigaOS 3.x port: MUI monitor window
 *
 * The front end of the VICE monitor (src/monitor): CPU registers on top,
 * the disassembly from PC under them with Break / Run / Step / Step over,
 * a hexadecimal memory view on its right; at the bottom the video chip and
 * the sound chip lists (amigamonchips.c) and the breakpoints.
 *
 * The VICE monitor stays in charge: when it stops the CPU (Break, a
 * breakpoint, the end of a step), monitor_startup() asks uimon_get_in()
 * for a command, and the uimon_get_in() of uimon.c calls
 * amiga_monitor_get_command(): the window is refreshed, then the UI is
 * handled (amiga_wait_events(), no CPU used) until a button gives a monitor
 * command ("x", "z", "n"). Stepping, breakpoints and the pause state are
 * VICE's own logic.
 *
 * When the emulation is paused (ui.c pause_trap()) the CPU is also stopped
 * between two instructions: the window shows its state too, Run ends the
 * pause, Break and Step go from the pause to the monitor.
 *
 * Labels: Load labels... reads a WLA-DX symbol file (wlalink -s, "00:080d
 * start") or a VICE one ("al C:080d .start") into the VICE monitor symbol
 * table: the disassembly shows them before their address and in the
 * operands (VICE's own disassembler), the address strings accept them.
 * Autostarting a .prg also loads the .sym file next to it.
 *
 * Closing the window ends the debugging: a stopped CPU runs again, and the
 * breakpoints are put aside (they would stop it and open the window again,
 * and while one exists each instruction is checked): the next opening puts
 * them back.
 *
 * The window is in the MUI application of amigamui.c.
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <intuition/classusr.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/alib.h>

#include "amigafile.h"
#include "amigalocale.h"
#include "amigamonchips.h"
#include "amigamonitor.h"
#include "amigamui.h"
#include "amigatrace.h"
#include "amigavideo.h"
#include "amigawait.h"
#include "drive.h"
#include "lib.h"
#include "machine.h"
#include "mem.h"
#include "monitor.h"
#include "montypes.h"
#include "mon_breakpoint.h"
#include "mon_disassemble.h"
#include "mon_register.h"
#include "ui.h"

/* GCC-safe MUI_NewObject wrapper, as in amigamui.c */
static Object * __attribute__((noinline))
MUI_NewObjectB(const char *cl, Tag tags, ...)
{
    return MUI_NewObjectA((char *)cl, (struct TagItem *)&tags);
}

/* in the MUI 3.8 SDK since V4, but only named in the later headers */
#ifndef MUIA_Window_InputEvent
#define MUIA_Window_InputEvent 0x804247d8
#endif

#ifndef MAKE_ID
#define MAKE_ID(a, b, c, d) \
    ((ULONG)(a) << 24 | (ULONG)(b) << 16 | (ULONG)(c) << 8 | (ULONG)(d))
#endif

/* disassembly lines: some before PC, PC, the rest after */
#define DIS_ROWS        20
#define DIS_ROWS_BEFORE 4

typedef struct dis_row_s {
    char addr[12];
    char ins[48];
} dis_row_t;

static dis_row_t dis_rows[DIS_ROWS];

/* memory view: HEX_ROWS lines of HEX_COLS bytes from hex_addr */
#define HEX_COLS        8
#define HEX_ROWS        16
#define HEX_PAGE        (HEX_COLS * HEX_ROWS)

static uint16_t hex_addr = 0;
/* "C000  00 11 22 33 44 55 66 77  ........\n" per line */
static char hex_text[HEX_ROWS * (6 + HEX_COLS * 3 + 1 + HEX_COLS + 1) + 1];

/* breakpoint list: number, address, hits */
#define BP_ROWS_MAX     32

typedef struct bp_row_s {
    int checknum;
    char num[8];
    char addr[48];          /* with the label of the address */
    char hits[12];
} bp_row_t;

static bp_row_t bp_rows[BP_ROWS_MAX];

/* video chip and sound chip lists: function, address, value */
#define CHIP_ROWS_MAX   32

static amiga_chip_row_t video_rows[CHIP_ROWS_MAX];
static amiga_chip_row_t sound_rows[CHIP_ROWS_MAX];

static Object *mon_win = NULL;
static Object *mon_regs = NULL;
static Object *mon_dis_list = NULL;
static Object *mon_status = NULL;
static Object *bt_break, *bt_run, *bt_step, *bt_next;
static Object *mon_hex = NULL;
static Object *mem_addr_str, *bt_mem_prev, *bt_mem_next;
static Object *mon_bp_list = NULL;
static Object *bp_addr_str, *bt_bp_add, *bt_bp_remove;
static Object *bt_labels;
static Object *mon_video_list = NULL;
static Object *mon_sound_list = NULL;
static int mon_open = 0;

/* the command given by a button, "" while none */
static char pending_cmd[16];
/* inside amiga_monitor_get_command(): the monitor waits for a command */
static int in_monitor = 0;

/* registers line and last monitor output line */
static char regs_text[160];
static char status_text[128];

/* the breakpoints put aside while the window is closed */
typedef struct parked_bp_s {
    MON_ADDR start, end;
    MEMORY_OP op;
    bool enabled;
} parked_bp_t;

static parked_bp_t parked_bps[BP_ROWS_MAX];
static int parked_count = 0;

/* the labels in the symbol table come from an autostart (replaced by the
 * next one), not from Load labels... */
static int labels_autoloaded = 0;

/* a jam reason without its padding spaces in status_text */
static void status_from_reason(const char *reason)
{
    size_t len;

    while (*reason == ' ') {
        reason++;
    }
    snprintf(status_text, sizeof status_text, "%s", reason);
    len = strlen(status_text);
    while (len > 0 && status_text[len - 1] == ' ') {
        status_text[--len] = '\0';
    }
}

/* ------------------------------------------------------------------------- */
/* MUI objects */

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

/* the disassembly table: 2 columns, address and instruction */
static ULONG dis_display(register struct Hook *hook __asm("a0"),
                         register char **array __asm("a2"),
                         register dis_row_t *row __asm("a1"))
{
    if (row == NULL) {
        /* title line */
        array[0] = (char *)LOC(MSG_MON_ADDRESS);
        array[1] = (char *)LOC(MSG_MON_INSTRUCTION);
    } else {
        array[0] = row->addr;
        array[1] = row->ins;
    }
    return 0;
}

static struct Hook dis_display_hook;

/* the breakpoint table: 3 columns, number, address, hits */
static ULONG bp_display(register struct Hook *hook __asm("a0"),
                        register char **array __asm("a2"),
                        register bp_row_t *row __asm("a1"))
{
    if (row == NULL) {
        array[0] = "#";
        array[1] = (char *)LOC(MSG_MON_ADDRESS);
        array[2] = (char *)LOC(MSG_MON_HITS);
    } else {
        array[0] = row->num;
        array[1] = row->addr;
        array[2] = row->hits;
    }
    return 0;
}

static struct Hook bp_display_hook;

/* the chip tables: 3 columns, function, address, value */
static ULONG chip_display(register struct Hook *hook __asm("a0"),
                          register char **array __asm("a2"),
                          register amiga_chip_row_t *row __asm("a1"))
{
    if (row == NULL) {
        array[0] = (char *)LOC(MSG_MON_REGISTER);
        array[1] = (char *)LOC(MSG_MON_ADDRESS);
        array[2] = (char *)LOC(MSG_MON_VALUE);
    } else {
        array[0] = row->name;
        array[1] = row->addr;
        array[2] = row->value;
    }
    return 0;
}

static struct Hook chip_display_hook;

/* a framed chip table, \a list gets its list */
static Object *make_chip_panel(const char *title, Object **list)
{
    *list = MUI_NewObjectB(MUIC_List,
                           MUIA_Frame, MUIV_Frame_ReadList,
                           MUIA_Font, MUIV_Font_Fixed,
                           MUIA_List_Format, (ULONG)"BAR,BAR,",
                           MUIA_List_Title, TRUE,
                           MUIA_List_DisplayHook, (ULONG)&chip_display_hook,
                           TAG_DONE);
    return MUI_NewObjectB(MUIC_Group,
                          MUIA_Frame, MUIV_Frame_Group,
                          MUIA_FrameTitle, (ULONG)title,
                          MUIA_Group_Child, (ULONG)MUI_NewObjectB(MUIC_Listview,
                              MUIA_Listview_List, (ULONG)*list,
                              MUIA_Listview_Input, FALSE,
                              TAG_DONE),
                          TAG_DONE);
}

/* an address: hexadecimal, or a label */
static Object *make_hex_string(void)
{
    return MUI_NewObjectB(MUIC_String,
                          MUIA_Frame, MUIV_Frame_String,
                          MUIA_String_MaxLen, 48,
                          MUIA_CycleChain, 1,
                          TAG_DONE);
}

/* a framed group, \a horiz is init only in MUI */
static Object *make_group_frame(const char *title, BOOL horiz)
{
    return MUI_NewObjectB(MUIC_Group,
                          MUIA_Group_Horiz, horiz,
                          MUIA_Frame, MUIV_Frame_Group,
                          MUIA_FrameTitle, (ULONG)title,
                          TAG_DONE);
}

static void hex_update(void);

static int create_window(void)
{
    Object *app = amiga_mui_app();

    if (app == NULL) {
        return -1;
    }
    Object *dis_group, *mem_group, *bp_group, *video_group, *sound_group;

    dis_display_hook.h_Entry = (ULONG (*)())dis_display;
    bp_display_hook.h_Entry = (ULONG (*)())bp_display;
    chip_display_hook.h_Entry = (ULONG (*)())chip_display;

    mon_regs = MUI_NewObjectB(MUIC_Text,
                              MUIA_Text_Contents, (ULONG)"",
                              MUIA_Font, MUIV_Font_Fixed,
                              MUIA_Frame, MUIV_Frame_Text,
                              MUIA_Background, MUII_TextBack,
                              TAG_DONE);
    mon_dis_list = MUI_NewObjectB(MUIC_List,
                                  MUIA_Frame, MUIV_Frame_ReadList,
                                  MUIA_Font, MUIV_Font_Fixed,
                                  MUIA_List_Format, (ULONG)"BAR,",
                                  MUIA_List_Title, TRUE,
                                  MUIA_List_DisplayHook, (ULONG)&dis_display_hook,
                                  TAG_DONE);
    mon_status = MUI_NewObjectB(MUIC_Text,
                                MUIA_Text_Contents, (ULONG)"",
                                MUIA_Frame, MUIV_Frame_Text,
                                MUIA_Background, MUII_TextBack,
                                TAG_DONE);
    bt_break = make_button(LOC(MSG_MON_BREAK));
    bt_run = make_button(LOC(MSG_MON_RUN));
    bt_step = make_button(LOC(MSG_MON_STEP));
    bt_next = make_button(LOC(MSG_MON_NEXT));
    bt_labels = make_button(LOC(MSG_MON_LABELS));

    /* left: disassembly, the CPU buttons, the labels */
    dis_group = MUI_NewObjectB(MUIC_Group,
                MUIA_Group_Child, (ULONG)MUI_NewObjectB(MUIC_Listview,
                    MUIA_Listview_List, (ULONG)mon_dis_list,
                    MUIA_Listview_Input, FALSE,
                    TAG_DONE),
                /* 2 lines of 2: Break Run, Step Step over */
                MUIA_Group_Child, (ULONG)MUI_NewObjectB(MUIC_Group,
                    MUIA_Group_Columns, 2,
                    MUIA_Group_SameSize, TRUE,
                    MUIA_Group_Child, (ULONG)bt_break,
                    MUIA_Group_Child, (ULONG)bt_run,
                    MUIA_Group_Child, (ULONG)bt_step,
                    MUIA_Group_Child, (ULONG)bt_next,
                    TAG_DONE),
                MUIA_Group_Child, (ULONG)bt_labels,
                TAG_DONE);

    /* right: the memory view, its address, previous and next page */
    mon_hex = MUI_NewObjectB(MUIC_Text,
                             MUIA_Text_Contents, (ULONG)"",
                             MUIA_Font, MUIV_Font_Fixed,
                             MUIA_Frame, MUIV_Frame_ReadList,
                             MUIA_Background, MUII_ReadListBack,
                             MUIA_VertWeight, 100,
                             TAG_DONE);
    mem_addr_str = make_hex_string();
    bt_mem_prev = make_button("<");
    bt_mem_next = make_button(">");
    mem_group = make_group_frame(LOC(MSG_MON_MEMORY), FALSE);
    if (mem_group != NULL) {
        DoMethod(mem_group, OM_ADDMEMBER, (ULONG)MUI_NewObjectB(MUIC_Group,
                    MUIA_Group_Horiz, TRUE,
                    MUIA_Group_Child, (ULONG)mem_addr_str,
                    MUIA_Group_Child, (ULONG)bt_mem_prev,
                    MUIA_Group_Child, (ULONG)bt_mem_next,
                    TAG_DONE));
        DoMethod(mem_group, OM_ADDMEMBER, (ULONG)mon_hex);
    }

    /* bottom: the video chip, the sound chip, and the breakpoints with an
     * address to add, remove the selected one */
    video_group = make_chip_panel(amiga_chips_video_title(), &mon_video_list);
    sound_group = make_chip_panel(amiga_chips_sound_title(), &mon_sound_list);
    mon_bp_list = MUI_NewObjectB(MUIC_List,
                                 MUIA_Frame, MUIV_Frame_InputList,
                                 MUIA_Font, MUIV_Font_Fixed,
                                 MUIA_List_Format, (ULONG)"BAR,BAR,",
                                 MUIA_List_Title, TRUE,
                                 MUIA_List_DisplayHook, (ULONG)&bp_display_hook,
                                 TAG_DONE);
    bp_addr_str = make_hex_string();
    bt_bp_add = make_button(LOC(MSG_MON_ADD));
    bt_bp_remove = make_button(LOC(MSG_MON_REMOVE));
    bp_group = make_group_frame(LOC(MSG_MON_BREAKPOINTS), FALSE);
    if (bp_group != NULL) {
        DoMethod(bp_group, OM_ADDMEMBER, (ULONG)MUI_NewObjectB(MUIC_Listview,
                    MUIA_Listview_List, (ULONG)mon_bp_list,
                    MUIA_CycleChain, 1,
                    TAG_DONE));
        DoMethod(bp_group, OM_ADDMEMBER, (ULONG)MUI_NewObjectB(MUIC_Group,
                    MUIA_Group_Horiz, TRUE,
                    MUIA_Group_Child, (ULONG)bp_addr_str,
                    MUIA_Group_Child, (ULONG)bt_bp_add,
                    MUIA_Group_Child, (ULONG)bt_bp_remove,
                    TAG_DONE));
    }

    mon_win = MUI_NewObjectB(MUIC_Window,
            MUIA_Window_Title, (ULONG)LOC(MSG_MON_TITLE),
            MUIA_Window_ID, MAKE_ID('V', 'M', 'O', 'N'),
            MUIA_Window_RootObject, (ULONG)MUI_NewObjectB(MUIC_Group,
                MUIA_Group_Child, (ULONG)mon_regs,
                MUIA_Group_Child, (ULONG)MUI_NewObjectB(MUIC_Group,
                    MUIA_Group_Horiz, TRUE,
                    MUIA_Group_Child, (ULONG)dis_group,
                    MUIA_Group_Child, (ULONG)mem_group,
                    TAG_DONE),
                MUIA_Group_Child, (ULONG)MUI_NewObjectB(MUIC_Group,
                    MUIA_Group_Horiz, TRUE,
                    MUIA_Group_Child, (ULONG)video_group,
                    MUIA_Group_Child, (ULONG)sound_group,
                    MUIA_Group_Child, (ULONG)bp_group,
                    TAG_DONE),
                MUIA_Group_Child, (ULONG)mon_status,
                TAG_DONE),
            TAG_DONE);
    if (mon_win == NULL) {
        return -1;
    }
    DoMethod(app, OM_ADDMEMBER, (ULONG)mon_win);

    DoMethod(bt_break, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_MON_BREAK);
    DoMethod(bt_run, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_MON_RUN);
    DoMethod(bt_step, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_MON_STEP);
    DoMethod(bt_next, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_MON_NEXT);
    DoMethod(mon_win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_MON_CLOSE);
    /* keys of the window, as in the IDEs (no F11 on Amiga keyboards):
     * F5 run, Shift+F5 break, F10 step over, Shift+F10 step. Like the
     * buttons, they do nothing when the state does not allow them */
    DoMethod(mon_win, MUIM_Notify, MUIA_Window_InputEvent, (ULONG)"f5",
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_MON_RUN);
    DoMethod(mon_win, MUIM_Notify, MUIA_Window_InputEvent, (ULONG)"shift f5",
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_MON_BREAK);
    DoMethod(mon_win, MUIM_Notify, MUIA_Window_InputEvent, (ULONG)"f10",
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_MON_NEXT);
    DoMethod(mon_win, MUIM_Notify, MUIA_Window_InputEvent, (ULONG)"shift f10",
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_MON_STEP);
    /* Return in the address strings: show / add */
    DoMethod(mem_addr_str, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_MON_MEM_ADDR);
    DoMethod(bt_mem_prev, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_MON_MEM_PREV);
    DoMethod(bt_mem_next, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_MON_MEM_NEXT);
    DoMethod(bp_addr_str, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_MON_BP_ADD);
    DoMethod(bt_bp_add, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_MON_BP_ADD);
    DoMethod(bt_bp_remove, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_MON_BP_REMOVE);
    DoMethod(bt_labels, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_MON_LABELS);

    /* the memory view at its full size from the start (its text sets it) */
    hex_update();
    return 0;
}

/* ------------------------------------------------------------------------- */
/* CPU state */

/* the CPU is stopped between two instructions: in the monitor or paused */
static int cpu_stopped(void)
{
    return in_monitor || ui_pause_active();
}

/* the byte at \a addr as the CPU sees it, without side effect */
static uint8_t peek(uint16_t addr, int mem_config)
{
    return mon_get_mem_val_nosfx(e_comp_space, (uint16_t)mem_config, addr);
}

/* disassemble the instruction at \a addr, returns its size */
static unsigned disassemble(uint16_t addr, int mem_config, const char **text)
{
    unsigned size = 1;
    const char *s;

    s = mon_disassemble_to_string_ex(e_comp_space, addr,
                                     peek(addr, mem_config),
                                     peek((uint16_t)(addr + 1), mem_config),
                                     peek((uint16_t)(addr + 2), mem_config),
                                     peek((uint16_t)(addr + 3), mem_config),
                                     1, &size);
    if (text != NULL) {
        /* the opcode bytes are a 12 characters field */
        *text = strlen(s) > 12 ? s + 12 : s;
    }
    return size > 0 ? size : 1;
}

/* where to start to have up to DIS_ROWS_BEFORE instructions before \a pc:
 * the 6502 instructions are not aligned, so the first start that falls
 * exactly on pc after them (a guess, as in any backwards disassembler) */
static uint16_t dis_start(uint16_t pc, int mem_config)
{
    uint16_t best = pc;
    int best_n = 0;
    int back, off, n;

    for (back = 1; back <= DIS_ROWS_BEFORE * 3; back++) {
        off = -back;
        n = 0;
        while (off < 0) {
            off += (int)disassemble((uint16_t)(pc + off), mem_config, NULL);
            n++;
        }
        if (off == 0 && n <= DIS_ROWS_BEFORE && n > best_n) {
            best = (uint16_t)(pc - back);
            best_n = n;
        }
    }
    return best;
}

static void regs_update(void)
{
    monitor_cpu_type_t *cpu = monitor_cpu_for_memspace[e_comp_space];
    mon_reg_list_t *list, *r;
    char *p = regs_text;
    char *end = regs_text + sizeof regs_text;
    int port_ddr = -1;  /* $00 of the 6510 port, shown with $01 */
    int bit;

    regs_text[0] = '\0';
    if (cpu == NULL || cpu->mon_register_list_get == NULL) {
        return;
    }
    list = cpu->mon_register_list_get(e_comp_space);
    if (list == NULL) {
        return;
    }
    /* 2 lines, the second from the flags (FL in hex, then its bits):
     * PC $C000  A $00  X $00  Y $00  SP $F3  io00 $2F37
     * FL $22  NV-BDIZC 00100010  LIN 256  CYC 63 */
    for (r = list; r->name != NULL && p < end - 28; r++) {
        if (r->id == e_FLAGS && !(r->flags & MON_REGISTER_IS_FLAGS) && p > regs_text) {
            /* no trailing spaces on the first line */
            while (p > regs_text && p[-1] == ' ') {
                p--;
            }
            *p++ = '\n';
        }
        if (r->flags & MON_REGISTER_IS_FLAGS) {
            p += sprintf(p, "%s ", r->name);
            for (bit = (int)r->size - 1; bit >= 0; bit--) {
                *p++ = (r->val & (1u << bit)) ? '1' : '0';
            }
            *p++ = ' ';
            *p++ = ' ';
            *p = '\0';
        } else if (r->id == e_Rasterline || r->id == e_Cycle) {
            /* video chip beam position: decimal, as VICE shows it */
            p += sprintf(p, "%s %u  ", r->name, r->val & 0xffff);
        } else if (r->size > 8) {
            p += sprintf(p, "%s $%04X  ", r->name, r->val & 0xffff);
        } else if (r->flags & MON_REGISTER_IS_MEMORY) {
            /* the 6510 port, $00 then $01 in one value: "io00 $2F37" */
            if (r->extra == 0) {
                port_ddr = (int)(r->val & 0xff);
            } else if (port_ddr >= 0) {
                p += sprintf(p, "io00 $%02X%02X  ", (unsigned int)port_ddr, r->val & 0xff);
            }
        } else {
            p += sprintf(p, "%s $%02X  ", r->name, r->val & 0xff);
        }
    }
    lib_free(list);
}

static void dis_update(void)
{
    int mem_config = mem_get_current_bank_config();
    uint16_t pc, addr;
    const char *text;
    int label_shown = 0;
    int i;

    pc = (uint16_t)monitor_cpu_for_memspace[e_comp_space]->mon_register_get_val(e_comp_space, e_PC);
    addr = dis_start(pc, mem_config);
    for (i = 0; i < DIS_ROWS; i++) {
        const char *label = mon_symbol_table_lookup_name(e_comp_space, addr);
        unsigned size;
        /* PC line in bold */
        const char *mark = (addr == pc) ? "\33b" : "";

        if (label != NULL && !label_shown) {
            /* "start:" on its own line, before the instruction */
            dis_rows[i].addr[0] = '\0';
            snprintf(dis_rows[i].ins, sizeof dis_rows[i].ins, "%s:", label);
            label_shown = 1;
            continue;
        }
        size = disassemble(addr, mem_config, &text);
        snprintf(dis_rows[i].addr, sizeof dis_rows[i].addr, "%s%04X", mark, addr);
        snprintf(dis_rows[i].ins, sizeof dis_rows[i].ins, "%s%s", mark, text);
        addr = (uint16_t)(addr + size);
        label_shown = 0;
    }
    set(mon_dis_list, MUIA_List_Quiet, TRUE);
    DoMethod(mon_dis_list, MUIM_List_Clear);
    for (i = 0; i < DIS_ROWS; i++) {
        DoMethod(mon_dis_list, MUIM_List_InsertSingle, (ULONG)&dis_rows[i],
                 MUIV_List_Insert_Bottom);
    }
    set(mon_dis_list, MUIA_List_Quiet, FALSE);
}

/* the HEX_PAGE bytes from hex_addr, as the CPU sees them (side effect free:
 * the I/O registers are not acknowledged by reading them here) */
static void hex_update(void)
{
    int mem_config = mem_get_current_bank_config();
    char *p = hex_text;
    uint8_t bytes[HEX_COLS];
    int row, col;
    char addr_text[8];

    for (row = 0; row < HEX_ROWS; row++) {
        uint16_t addr = (uint16_t)(hex_addr + row * HEX_COLS);

        p += sprintf(p, "%04X ", addr);
        for (col = 0; col < HEX_COLS; col++) {
            bytes[col] = peek((uint16_t)(addr + col), mem_config);
            p += sprintf(p, " %02X", bytes[col]);
        }
        *p++ = ' ';
        *p++ = ' ';
        for (col = 0; col < HEX_COLS; col++) {
            /* printable ASCII only: no MUI escape sequence */
            *p++ = (bytes[col] >= 0x20 && bytes[col] < 0x7f) ? (char)bytes[col] : '.';
        }
        if (row < HEX_ROWS - 1) {
            *p++ = '\n';
        }
    }
    *p = '\0';
    set(mon_hex, MUIA_Text_Contents, (ULONG)hex_text);
    sprintf(addr_text, "%04X", hex_addr);
    nnset(mem_addr_str, MUIA_String_Contents, (ULONG)addr_text);
}

/* the breakpoints (exec checkpoints) of the computer CPU */
static void bp_update(void)
{
    mon_checkpoint_t **list;
    unsigned int len = 0, i;
    int n = 0;

    list = mon_breakpoint_checkpoint_list_get(&len);
    for (i = 0; i < len && n < BP_ROWS_MAX; i++) {
        mon_checkpoint_t *cp = list[i];
        bp_row_t *row = &bp_rows[n];

        if (!cp->check_exec || addr_memspace(cp->start_addr) != e_comp_space) {
            continue;
        }
        row->checknum = cp->checknum;
        sprintf(row->num, "%d", cp->checknum);
        if (cp->end_addr != cp->start_addr) {
            sprintf(row->addr, "%04X-%04X", (unsigned)addr_location(cp->start_addr),
                    (unsigned)addr_location(cp->end_addr));
        } else {
            sprintf(row->addr, "%04X", (unsigned)addr_location(cp->start_addr));
        }
        {
            const char *label = mon_symbol_table_lookup_name(e_comp_space,
                                                             (uint16_t)addr_location(cp->start_addr));

            if (label != NULL) {
                snprintf(row->addr + strlen(row->addr), sizeof row->addr - strlen(row->addr),
                         " %s", label);
            }
        }
        if (!cp->enabled) {
            /* disabled from the monitor commands: shown, does not stop */
            strcat(row->addr, " -");
        }
        sprintf(row->hits, "%d", cp->hit_count);
        n++;
    }
    lib_free(list);

    set(mon_bp_list, MUIA_List_Quiet, TRUE);
    DoMethod(mon_bp_list, MUIM_List_Clear);
    for (i = 0; i < (unsigned int)n; i++) {
        DoMethod(mon_bp_list, MUIM_List_InsertSingle, (ULONG)&bp_rows[i],
                 MUIV_List_Insert_Bottom);
    }
    set(mon_bp_list, MUIA_List_Quiet, FALSE);
}

/* a chip list from its rows */
static void chip_list_fill(Object *list, amiga_chip_row_t *rows, int n)
{
    int i;

    set(list, MUIA_List_Quiet, TRUE);
    DoMethod(list, MUIM_List_Clear);
    for (i = 0; i < n; i++) {
        DoMethod(list, MUIM_List_InsertSingle, (ULONG)&rows[i], MUIV_List_Insert_Bottom);
    }
    set(list, MUIA_List_Quiet, FALSE);
}

static void chips_update(void)
{
    chip_list_fill(mon_video_list, video_rows, amiga_chips_video_rows(video_rows, CHIP_ROWS_MAX));
    chip_list_fill(mon_sound_list, sound_rows, amiga_chips_sound_rows(sound_rows, CHIP_ROWS_MAX));
}

/* "C000", "$C000", "0xC000" or a label in a string gadget, -1 if not an
 * address. A label first: "add" or "beef" may be both. */
static int read_address(Object *str)
{
    STRPTR text = NULL;
    const char *p;
    unsigned long v = 0;
    int digits = 0;
    char name[64];

    get(str, MUIA_String_Contents, &text);
    if (text == NULL) {
        return -1;
    }
    p = (const char *)text;
    while (*p == ' ') {
        p++;
    }
    /* VICE labels are written with a dot: ".start" */
    snprintf(name, sizeof name, "%s", *p == '.' ? p + 1 : p);
    if (name[0] != '\0') {
        int addr = mon_symbol_table_lookup_addr(e_comp_space, name);

        if (addr >= 0) {
            return addr;
        }
    }
    if (*p == '$') {
        p++;
    } else if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }
    for (; isxdigit((unsigned char)*p) && digits < 5; p++, digits++) {
        v = v * 16 + (unsigned long)(isdigit((unsigned char)*p)
                                     ? *p - '0' : (tolower((unsigned char)*p) - 'a' + 10));
    }
    if (digits == 0 || *p != '\0' || v > 0xffff) {
        return -1;
    }
    return (int)v;
}

static void show_bad_address(void)
{
    strcpy(status_text, LOC(MSG_MON_BAD_ADDRESS));
    set(mon_status, MUIA_Text_Contents, (ULONG)status_text);
}

static void bp_add(void)
{
    int addr = read_address(bp_addr_str);

    if (addr < 0) {
        show_bad_address();
        return;
    }
    /* turns the CPU loop monitor test on (IK_MONITOR) */
    mon_breakpoint_add_checkpoint(new_addr(e_comp_space, (unsigned)addr),
                                  new_addr(e_comp_space, (unsigned)addr),
                                  true, e_exec, false, false);
    nnset(bp_addr_str, MUIA_String_Contents, (ULONG)"");
    bp_update();
}

static void bp_remove(void)
{
    LONG active = MUIV_List_Active_Off;
    bp_row_t *row = NULL;

    get(mon_bp_list, MUIA_List_Active, &active);
    if (active == MUIV_List_Active_Off) {
        return;
    }
    DoMethod(mon_bp_list, MUIM_List_GetEntry, active, (ULONG)&row);
    if (row == NULL) {
        return;
    }
    /* the last one turns the CPU loop monitor test off */
    mon_breakpoint_delete_checkpoint(row->checknum);
    bp_update();
}

/* ------------------------------------------------------------------------- */
/* labels */

static void refresh(void);

/* one label line, 0 if it is not one:
 * - WLA-DX (wlalink -s): "00:080d start", older ones "0000:080d start"
 * - VICE (ll): "al C:080d .start" */
static int label_line(char *line, unsigned int *addr, char **name)
{
    char *p = line;
    char *end;
    unsigned long v;

    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (strncmp(p, "al ", 3) == 0) {
        p += 3;
        while (*p == ' ') {
            p++;
        }
        if (isalpha((unsigned char)p[0]) && p[1] == ':') {
            /* memory space: C: the computer */
            if (toupper((unsigned char)p[0]) != 'C') {
                return 0;
            }
            p += 2;
        }
    } else {
        char *bank = p;

        /* the bank, not used: the CPU sees 64K */
        while (isxdigit((unsigned char)*p)) {
            p++;
        }
        if (*p != ':' || p == bank) {
            return 0;
        }
        p++;
    }
    v = strtoul(p, &end, 16);
    if (end == p || v > 0xffff || (*end != ' ' && *end != '\t')) {
        return 0;
    }
    p = end;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '.') {
        p++;
    }
    end = p;
    while (*end != '\0' && !isspace((unsigned char)*end)) {
        end++;
    }
    *end = '\0';
    /* the label sizes made by wlalink */
    if (*p == '\0' || strncmp(p, "_sizeof_", 8) == 0) {
        return 0;
    }
    *addr = (unsigned int)v;
    *name = p;
    return 1;
}

/* the labels of \a path in the symbol table, instead of the ones there.
 * Returns how many, -1 if the file cannot be read. */
static int labels_load(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[256];
    int in_labels = 1;  /* wlalink -S: only its [labels] part */
    int n = 0;

    if (f == NULL) {
        return -1;
    }
    mon_clear_symbol_table(e_comp_space);
    while (fgets(line, sizeof line, f) != NULL) {
        unsigned int addr;
        char *name;

        if (line[0] == ';' || line[0] == '#') {
            continue;
        }
        if (line[0] == '[') {
            in_labels = strncmp(line, "[labels]", 8) == 0;
            continue;
        }
        if (in_labels && label_line(line, &addr, &name)) {
            /* the table keeps the name */
            mon_add_name_to_symbol_table(new_addr(e_comp_space, addr), lib_strdup(name));
            n++;
        }
    }
    fclose(f);
    return n;
}

/* the result in the status line (after the warnings of the symbol table,
 * duplicate labels) */
static void labels_status(const char *path, int n)
{
    if (n < 0) {
        snprintf(status_text, sizeof status_text, LOC(MSG_MON_LABELS_ERROR), path);
    } else {
        snprintf(status_text, sizeof status_text, LOC(MSG_MON_LABELS_LOADED), n, path);
    }
}

/* Load labels...: a file requester on the window */
static void labels_request(void)
{
    struct Window *window = NULL;
    Object *app = amiga_mui_app();
    char *path;

    get(mon_win, MUIA_Window_Window, &window);
    set(app, MUIA_Application_Sleep, TRUE);
    path = amiga_file_request(window, LOC(MSG_REQ_LABELS), "#?.(sym|lbl|vs|labels)");
    set(app, MUIA_Application_Sleep, FALSE);
    if (path == NULL) {
        return;
    }
    labels_status(path, labels_load(path));
    labels_autoloaded = 0;
    lib_free(path);
}

/** \brief  The labels of a started or loaded program: its .sym file, if any
 */
void amiga_monitor_autoload_labels(const char *path, int forget)
{
    /* kept: the emulator screen shows it later */
    static char message[64];
    char sym[256];
    char *dot, *slash;
    FILE *f;
    int n;

    snprintf(sym, sizeof sym, "%s", path);
    dot = strrchr(sym, '.');
    slash = strrchr(sym, '/');
    if (slash == NULL) {
        slash = strrchr(sym, ':');
    }
    if (dot == NULL || (slash != NULL && dot < slash)) {
        dot = sym + strlen(sym);
    }
    if ((size_t)(dot - sym) + 5 > sizeof sym) {
        return;
    }
    strcpy(dot, ".sym");
    f = fopen(sym, "r");
    if (f == NULL) {
        if (forget && labels_autoloaded) {
            /* the ones of the previous program would be wrong */
            mon_clear_symbol_table(e_comp_space);
            labels_autoloaded = 0;
        }
        return;
    }
    fclose(f);
    n = labels_load(sym);
    labels_autoloaded = 1;
    labels_status(sym, n);
    if (n >= 0) {
        snprintf(message, sizeof message, LOC(MSG_MON_SYMBOLS_UPDATED), n);
        /* about 3 seconds */
        amiga_video_show_message(message, 150, AMIGA_MESSAGE_INFO);
    }
    if (mon_open) {
        /* the disassembly with the new names */
        refresh();
    }
}

/* the window is closed: the breakpoints that stop the CPU are removed and
 * kept here (not the temporary ones of the VICE monitor commands) */
static void breakpoints_park(void)
{
    mon_checkpoint_t **list;
    unsigned int len = 0, i;
    int nums[BP_ROWS_MAX];
    int n = 0;

    list = mon_breakpoint_checkpoint_list_get(&len);
    for (i = 0; i < len && n < BP_ROWS_MAX; i++) {
        mon_checkpoint_t *cp = list[i];
        parked_bp_t *bp = &parked_bps[parked_count];

        if (!cp->stop) {
            /* trace points: they do not stop the CPU */
            continue;
        }
        nums[n++] = cp->checknum;
        if (cp->temporary || parked_count >= BP_ROWS_MAX) {
            continue;
        }
        bp->start = cp->start_addr;
        bp->end = cp->end_addr;
        bp->op = (MEMORY_OP)((cp->check_load ? e_load : 0) | (cp->check_store ? e_store : 0)
                             | (cp->check_exec ? e_exec : 0));
        bp->enabled = cp->enabled;
        parked_count++;
    }
    lib_free(list);
    /* after the list, which the deletions change: the last one turns the
       CPU loop monitor test off */
    for (i = 0; i < (unsigned int)n; i++) {
        mon_breakpoint_delete_checkpoint(nums[i]);
    }
}

/* the window opens: the breakpoints put aside are back (new numbers, hits
 * from 0) */
static void breakpoints_unpark(void)
{
    int i;

    for (i = 0; i < parked_count; i++) {
        parked_bp_t *bp = &parked_bps[i];
        int num = mon_breakpoint_add_checkpoint(bp->start, bp->end, true, bp->op,
                                                false, false);

        if (!bp->enabled && num > 0) {
            mon_breakpoint_switch_checkpoint(e_OFF, num);
        }
    }
    parked_count = 0;
}

/* buttons for the state, and the CPU state when it is stopped */
static void refresh(void)
{
    int stopped;

    if (!mon_open) {
        return;
    }
    stopped = cpu_stopped();
    set(bt_break, MUIA_Disabled, in_monitor);
    set(bt_run, MUIA_Disabled, !stopped);
    set(bt_step, MUIA_Disabled, !stopped);
    set(bt_next, MUIA_Disabled, !in_monitor);
    if (!stopped) {
        set(mon_regs, MUIA_Text_Contents, (ULONG)LOC(MSG_MON_RUNNING));
        DoMethod(mon_dis_list, MUIM_List_Clear);
    } else {
        regs_update();
        set(mon_regs, MUIA_Text_Contents, (ULONG)regs_text);
        dis_update();
    }
    /* running: the memory and the chips as they are now */
    hex_update();
    chips_update();
    bp_update();
    set(mon_status, MUIA_Text_Contents, (ULONG)status_text);
}

/* ------------------------------------------------------------------------- */

/** \brief  Open the monitor window (non-modal), or bring it to front
 */
void amiga_monitor_open(void)
{
    if (mon_win == NULL && create_window() != 0) {
        return;
    }
    if (mon_open) {
        DoMethod(mon_win, MUIM_Window_ToFront);
        return;
    }
    amiga_video_requester_begin();
    set(mon_win, MUIA_Window_Open, TRUE);
    mon_open = 1;
    breakpoints_unpark();
    refresh();

    /* first input round: gives the signals MUI waits for */
    amiga_mui_handle_events();
}

static void close_window(void)
{
    set(mon_win, MUIA_Window_Open, FALSE);
    mon_open = 0;
    amiga_mui_window_closed();
}

int amiga_monitor_is_open(void)
{
    return mon_open;
}

/** \brief  A button or the close gadget of the monitor window
 */
void amiga_monitor_handle_rid(ULONG rid)
{
    switch (rid) {
        case RID_MON_BREAK:
            if (ui_pause_active()) {
                /* from the pause to the monitor, at the same instruction */
                ui_pause_disable();
            }
            monitor_startup_trap();
            break;
        case RID_MON_RUN:
            if (in_monitor) {
                strcpy(pending_cmd, "x");
            } else if (ui_pause_active()) {
                ui_pause_disable();
            }
            break;
        case RID_MON_STEP:
            if (in_monitor) {
                strcpy(pending_cmd, "z");
            } else if (ui_pause_active()) {
                /* to the monitor first, the next Step runs an instruction */
                ui_pause_disable();
                monitor_startup_trap();
            }
            break;
        case RID_MON_NEXT:
            if (in_monitor) {
                strcpy(pending_cmd, "n");
            }
            break;
        case RID_MON_MEM_ADDR:
            {
                int addr = read_address(mem_addr_str);

                if (addr < 0) {
                    show_bad_address();
                } else {
                    hex_addr = (uint16_t)addr;
                    hex_update();
                }
            }
            break;
        case RID_MON_MEM_PREV:
            hex_addr = (uint16_t)(hex_addr - HEX_PAGE);
            hex_update();
            break;
        case RID_MON_MEM_NEXT:
            hex_addr = (uint16_t)(hex_addr + HEX_PAGE);
            hex_update();
            break;
        case RID_MON_BP_ADD:
            bp_add();
            break;
        case RID_MON_BP_REMOVE:
            bp_remove();
            break;
        case RID_MON_LABELS:
            labels_request();
            /* the disassembly and the breakpoints with them */
            refresh();
            break;
        case RID_MON_CLOSE:
            /* the end of the debugging: no breakpoint opens the window
               again, a stopped CPU runs (a pause from the menu stays) */
            breakpoints_park();
            close_window();
            if (in_monitor) {
                strcpy(pending_cmd, "x");
            }
            break;
        default:
            break;
    }
}

/** \brief  The monitor waits for a command (uimon_get_in())
 *
 * \return  the command (lib_malloc'ed), NULL to leave the monitor
 */
char *amiga_monitor_get_command(void)
{
    if (default_memspace != e_comp_space) {
        /* stopped on another CPU (a drive JAM: same JAMAction as the
         * computer): the window only shows the computer CPU. As before,
         * that CPU stays jammed (logged by VICE), the monitor goes on and
         * back to the computer CPU for the next Break */
        if (mon_open) {
            int dnr = monitor_diskspace_dnr(default_memspace);

            if (dnr >= 0 && drive_is_jammed(dnr) && drive_jam_reason(dnr) != NULL) {
                status_from_reason(drive_jam_reason(dnr));
            } else {
                strcpy(status_text, LOC(MSG_MON_OTHER_CPU));
            }
            set(mon_status, MUIA_Text_Contents, (ULONG)status_text);
        }
        default_memspace = e_comp_space;
        return lib_strdup("x");
    }
    if (!mon_open) {
        /* a breakpoint reached, a CPU JAM: the monitor without the window */
        amiga_monitor_open();
    }
    if (!mon_open) {
        /* no MUI */
        return NULL;
    }
    in_monitor = 1;
    pending_cmd[0] = '\0';
    if (machine_is_jammed() && machine_jam_reason() != NULL) {
        /* "main CPU: JAM at $xxxx": until a reset, Run leaves it frozen */
        status_from_reason(machine_jam_reason());
    }
    refresh();
    while (pending_cmd[0] == '\0') {
        amiga_wait_events();
    }
    in_monitor = 0;
    status_text[0] = '\0';
    /* no refresh here: a step comes back at once (no flicker), leaving the
     * monitor refreshes in uimon_window_suspend() or _close() */
    return lib_strdup(pending_cmd);
}

/** \brief  The CPU stopped or goes on: pause, monitor left
 */
void amiga_monitor_refresh(void)
{
    refresh();
}

/** \brief  Monitor text output: its last line is shown under the buttons
 */
void amiga_monitor_out(const char *text)
{
    const char *line = text, *p;
    size_t len;

    /* last non empty line of text */
    for (p = text; *p != '\0'; p++) {
        if (*p == '\n' && p[1] != '\0' && p[1] != '\n') {
            line = p + 1;
        }
    }
    len = strcspn(line, "\n");
    if (len == 0) {
        return;
    }
    if (len >= sizeof status_text) {
        len = sizeof status_text - 1;
    }
    memcpy(status_text, line, len);
    status_text[len] = '\0';
}

/** \brief  The MUI application is disposed (with the window)
 */
void amiga_monitor_dispose(void)
{
    mon_win = NULL;
    mon_open = 0;
}
