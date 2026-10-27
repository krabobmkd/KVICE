/*
 * amigaleds.c - drive and tape LEDs: window title bar gadget, fullscreen
 *
 * In the window, a private boopsi gadget class (a gadgetclass subclass) in
 * the title bar, left of the depth gadget: "F" (floppy) power, activity and
 * jam LEDs, "T" (tape) motor LED. Intuition renders it with the window
 * borders (activation, resize), amiga_leds_window_update() redraws it
 * through SetGadgetAttrs() when the state changes. Its size follows the
 * screen font (title bar height) and the pixel aspect.
 *
 * In fullscreen there is no title bar: video.c draws the LEDs alone at
 * the bottom right of the screen with amiga_leds_draw_fullscreen().
 *
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

#include <string.h>

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/cghooks.h>
#include <intuition/screens.h>
#include <graphics/text.h>
#include <graphics/rastport.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/alib.h>

#include "drive.h"
#include "resources.h"

#include "amigaleds.h"
#include "amigalocale.h"
#include "amigatrace.h"

const ULONG amiga_led_rgb[AMIGA_LED_PENS] = {
    0x000000,   /* off */
    0x20d020,   /* green */
    0xf02020,   /* red */
    0xf0e020    /* yellow */
};

/* the LEDs of each group, left to right, and their color when on */
typedef struct amiga_led_s {
    ULONG bit;
    int pen;
} amiga_led_t;

static const amiga_led_t leds_floppy[] = {
    { AMIGA_LED_DRIVE_ON, AMIGA_LED_PEN_GREEN },
    { AMIGA_LED_DRIVE_ACT, AMIGA_LED_PEN_RED },
    { AMIGA_LED_DRIVE_JAM, AMIGA_LED_PEN_YELLOW }
};
#define LEDS_FLOPPY (sizeof leds_floppy / sizeof leds_floppy[0])

static const amiga_led_t leds_tape[] = {
    { AMIGA_LED_TAPE_MOTOR, AMIGA_LED_PEN_RED }
};
#define LEDS_TAPE (sizeof leds_tape / sizeof leds_tape[0])

/* empty pixels at the right of the title bar gadget */
#define LEDS_RIGHT_MARGIN 6

/* frames the activity LED stays on after a virtual drive byte */
#define LEDS_VIRTUAL_HOLD 6

/* from the UI calls */
static ULONG leds_raw = 0;
/* shown: leds_raw and what is polled, by amiga_leds_poll() */
static ULONG leds_state = 0;
/* virtual drive 8 (kernal traps) activity, in frames left */
static int leds_virtual_hold = 0;

/* the title bar gadget, at most one window has it */
static Class *leds_class = NULL;
static struct Gadget *leds_gadget = NULL;
static struct Window *leds_window = NULL;
static struct DrawInfo *leds_dri = NULL;
/* pens obtained on the window screen, -1: not obtained */
static LONG leds_pens[AMIGA_LED_PENS] = { -1, -1, -1, -1 };
static int leds_pen_obtained[AMIGA_LED_PENS];
/* state of the last rendering */
static ULONG leds_drawn = 0;

/* layout, relative to the gadget box. GM_RENDER may run on the
 * input.device task (activation, resize): it only reads these. */
static const char *lay_floppy;
static const char *lay_tape;
static WORD lay_text_y;     /* baseline */
static WORD lay_floppy_x;   /* "floppy:" */
static WORD lay_floppy_leds_x;
static WORD lay_tape_x;     /* "tape:" */
static WORD lay_tape_leds_x;
static WORD lay_led_y;
static WORD lay_led_w;
static WORD lay_led_h;
static WORD lay_led_gap;

void amiga_leds_set(ULONG bits, int on)
{
    if (on) {
        leds_raw |= bits;
    } else {
        leds_raw &= ~bits;
    }
}

void amiga_leds_virtual_activity(unsigned int device)
{
    if (device == 8) {
        leds_virtual_hold = LEDS_VIRTUAL_HOLD;
    }
}

ULONG amiga_leds_poll(void)
{
    ULONG state = leds_raw & ~(AMIGA_LED_DRIVE_ON | AMIGA_LED_DRIVE_ACT | AMIGA_LED_DRIVE_JAM);
    int tde = 0, traps = 0, bus = 0;

    /* drive 8 usable: the emulated drive, or the virtual one (kernal
     * traps, or the bus device of the drawer) when it is off */
    resources_get_int("Drive8TrueEmulation", &tde);
    if (!tde) {
        resources_get_int("TrapDevice8", &traps);
        resources_get_int("BusDevice8", &bus);
    }
    if ((leds_raw & AMIGA_LED_DRIVE_ON) || (!tde && (traps || bus))) {
        state |= AMIGA_LED_DRIVE_ON;
    }
    /* the emulated drive LED, or the virtual drive moving bytes */
    if ((leds_raw & AMIGA_LED_DRIVE_ACT) || leds_virtual_hold > 0) {
        state |= AMIGA_LED_DRIVE_ACT;
    }
    if (leds_virtual_hold > 0) {
        leds_virtual_hold--;
    }
    /* set by drive_jam(), cleared by the drive reset */
    if (drive_is_jammed(0)) {
        state |= AMIGA_LED_DRIVE_JAM;
    }
    leds_state = state;
    return state;
}

/* ------------------------------------------------------------------------- */

static void leds_row(struct RastPort *rp, const amiga_led_t *leds, int count,
                     WORD x, WORD y, WORD w, WORD h, WORD gap, const LONG *pens)
{
    int i;

    /* each LED in a dark 1 pixel frame, around x, y, w, h: an off LED is
     * all dark */
    for (i = 0; i < count; i++) {
        int on = (leds_state & leds[i].bit) != 0;

        if (pens[AMIGA_LED_PEN_OFF] >= 0) {
            SetAPen(rp, (UWORD)pens[AMIGA_LED_PEN_OFF]);
            RectFill(rp, x - 1, y - 1, x + w, y + h);
        }
        if (on && pens[leds[i].pen] >= 0) {
            SetAPen(rp, (UWORD)pens[leds[i].pen]);
            RectFill(rp, x, y, x + w - 1, y + h - 1);
        }
        x += w + gap;
    }
}

void amiga_leds_draw_fullscreen(struct RastPort *rp, WORD right, WORD bottom,
                                WORD led_w, WORD led_h, const LONG *pens)
{
    WORD gap = led_w / 3 + 3;
    WORD x, y = bottom - led_h;

    /* floppy group, a LED width, tape group; the frames inside right,
     * bottom */
    x = right - (WORD)LEDS_TAPE * (led_w + gap) + gap;
    leds_row(rp, leds_tape, LEDS_TAPE, x, y, led_w, led_h, gap, pens);
    x -= led_w + (WORD)LEDS_FLOPPY * (led_w + gap);
    leds_row(rp, leds_floppy, LEDS_FLOPPY, x, y, led_w, led_h, gap, pens);
}

/* ------------------------------------------------------------------------- */

/* the gadget box in the window, from its domain */
static void leds_box(struct Gadget *g, struct GadgetInfo *gi, struct IBox *box)
{
    box->Left = g->LeftEdge;
    box->Top = g->TopEdge;
    box->Width = g->Width;
    box->Height = g->Height;
    if (g->Flags & GFLG_RELRIGHT) {
        box->Left += gi->gi_Domain.Width - 1;
    }
    box->Left += gi->gi_Domain.Left;
    box->Top += gi->gi_Domain.Top;
}

static void leds_render(struct Gadget *g, struct gpRender *msg)
{
    struct GadgetInfo *gi = msg->gpr_GInfo;
    struct RastPort *rp = msg->gpr_RPort;
    struct DrawInfo *dri;
    struct IBox box;
    int active;

    if (gi == NULL || rp == NULL || gi->gi_Window == NULL) {
        return;
    }
    dri = gi->gi_DrInfo;
    leds_box(g, gi, &box);
    if (msg->gpr_Redraw != GREDRAW_UPDATE && dri != NULL) {
        active = (gi->gi_Window->Flags & WFLG_WINDOWACTIVE) != 0;
        /* the title bar, without its top and bottom lines, nor the right
         * margin: never over the frame line of the gadget on the right */
        SetAPen(rp, dri->dri_Pens[active ? FILLPEN : BACKGROUNDPEN]);
        SetDrMd(rp, JAM1);
        RectFill(rp, box.Left, box.Top + 1,
                 box.Left + box.Width - 1 - LEDS_RIGHT_MARGIN, box.Top + box.Height - 2);
        SetAPen(rp, dri->dri_Pens[active ? FILLTEXTPEN : TEXTPEN]);
        if (dri->dri_Font != NULL) {
            SetFont(rp, dri->dri_Font);
        }
        Move(rp, box.Left + lay_floppy_x, box.Top + lay_text_y);
        Text(rp, (CONST_STRPTR)lay_floppy, strlen(lay_floppy));
        Move(rp, box.Left + lay_tape_x, box.Top + lay_text_y);
        Text(rp, (CONST_STRPTR)lay_tape, strlen(lay_tape));
    }
    leds_row(rp, leds_floppy, LEDS_FLOPPY, box.Left + lay_floppy_leds_x, box.Top + lay_led_y,
             lay_led_w, lay_led_h, lay_led_gap, leds_pens);
    leds_row(rp, leds_tape, LEDS_TAPE, box.Left + lay_tape_leds_x, box.Top + lay_led_y,
             lay_led_w, lay_led_h, lay_led_gap, leds_pens);
    leds_drawn = leds_state;
}

static ULONG leds_dispatch(register Class *cl __asm("a0"),
                           register Object *o __asm("a2"),
                           register Msg msg __asm("a1"))
{
    switch (msg->MethodID) {
        case GM_RENDER:
            leds_render((struct Gadget *)o, (struct gpRender *)msg);
            return 0;
        case GM_HITTEST:
            /* display only */
            return 0;
        case GM_GOACTIVE:
            return GMR_NOREUSE;
        case OM_SET:
        case OM_UPDATE:
        {
            /* only amiga_leds_window_update() sets attributes: the state
             * changed, draw the LEDs now */
            struct GadgetInfo *gi = ((struct opSet *)msg)->ops_GInfo;
            ULONG ret = DoSuperMethodA(cl, o, msg);

            if (gi != NULL) {
                struct RastPort *rp = ObtainGIRPort(gi);

                if (rp != NULL) {
                    DoMethod(o, GM_RENDER, (ULONG)gi, (ULONG)rp, GREDRAW_UPDATE);
                    ReleaseGIRPort(rp);
                }
            }
            return ret;
        }
        default:
            return DoSuperMethodA(cl, o, msg);
    }
}

/* ------------------------------------------------------------------------- */

static void leds_release_pens(struct ColorMap *cm)
{
    int i;

    for (i = 0; i < AMIGA_LED_PENS; i++) {
        if (leds_pen_obtained[i] && cm != NULL) {
            ReleasePen(cm, (ULONG)leds_pens[i]);
        }
        leds_pens[i] = -1;
        leds_pen_obtained[i] = 0;
    }
}

/* shared pens on the Workbench, else the nearest color */
static void leds_obtain_pens(struct ColorMap *cm)
{
    int i;

    for (i = 0; i < AMIGA_LED_PENS; i++) {
        ULONG c = amiga_led_rgb[i];
        ULONG r = ((c >> 16) & 0xff) * 0x01010101UL;
        ULONG g = ((c >> 8) & 0xff) * 0x01010101UL;
        ULONG b = (c & 0xff) * 0x01010101UL;
        LONG pen = ObtainBestPen(cm, r, g, b, OBP_Precision, PRECISION_GUI, TAG_DONE);

        if (pen >= 0) {
            leds_pen_obtained[i] = 1;
        } else {
            pen = FindColor(cm, r, g, b, -1);
        }
        leds_pens[i] = pen;
    }
}

/* layout from the screen font and the title bar height, returns the
 * gadget width */
static WORD leds_layout(struct Window *w, struct DrawInfo *dri)
{
    struct TextFont *font = dri->dri_Font;
    struct RastPort trp;
    WORD bar = w->BorderTop;
    WORD space, text_top, x;
    LONG rx = dri->dri_Resolution.X;
    LONG ry = dri->dri_Resolution.Y;

    lay_floppy = LOC(MSG_LEDS_FLOPPY);
    lay_tape = LOC(MSG_LEDS_TAPE);
    InitRastPort(&trp);
    SetFont(&trp, font);
    space = TextLength(&trp, (CONST_STRPTR)" ", 1);

    /* the title text place: below the WBorTop line, else centered */
    text_top = w->WScreen->WBorTop;
    if (text_top + (WORD)font->tf_YSize > bar - 1) {
        text_top = (bar - (WORD)font->tf_YSize) / 2;
    }
    lay_text_y = text_top + (WORD)font->tf_Baseline;

    /* half the height of most capitals, centered on them, plus the
     * frame */
    lay_led_h = ((WORD)font->tf_Baseline - 1) / 2;
    if (lay_led_h > bar - 6) {
        lay_led_h = bar - 6;
    }
    if (lay_led_h < 2) {
        lay_led_h = 2;
    }
    lay_led_y = text_top + ((WORD)font->tf_Baseline + 1 - lay_led_h) / 2;
    if (lay_led_y < 3) {
        lay_led_y = 3;
    }
    /* 3:2 on the monitor: the resolution is in ticks per pixel, the
     * Workbench pixels are often twice as high as wide */
    if (rx <= 0 || ry <= 0) {
        rx = ry = 1;
    }
    lay_led_w = (WORD)((3 * lay_led_h * ry) / (2 * rx));
    if (lay_led_w < 2) {
        lay_led_w = 2;
    }
    /* frames included */
    lay_led_gap = lay_led_w / 3 + 3;

    x = space;
    lay_floppy_x = x;
    x += TextLength(&trp, (CONST_STRPTR)lay_floppy, strlen(lay_floppy)) + space;
    /* the frames: 1 pixel each side */
    lay_floppy_leds_x = x + 1;
    x += (WORD)LEDS_FLOPPY * (lay_led_w + lay_led_gap) - lay_led_gap + 2 + 2 * space;
    lay_tape_x = x;
    x += TextLength(&trp, (CONST_STRPTR)lay_tape, strlen(lay_tape)) + space;
    lay_tape_leds_x = x + 1;
    /* and a margin before the gadgets on its right */
    x += (WORD)LEDS_TAPE * (lay_led_w + lay_led_gap) - lay_led_gap + 2 + space
         + LEDS_RIGHT_MARGIN;
    return x;
}

void amiga_leds_window_open(struct Window *w)
{
    struct Gadget *g;
    WORD width, left, right;

    if (w == NULL || leds_gadget != NULL || w->BorderTop < 6) {
        return;
    }
    if (leds_class == NULL) {
        leds_class = MakeClass(NULL, (ClassID)GADGETCLASS, NULL, 0, 0);
        if (leds_class == NULL) {
            return;
        }
        leds_class->cl_Dispatcher.h_Entry = (ULONG (*)())leds_dispatch;
    }
    leds_dri = GetScreenDrawInfo(w->WScreen);
    if (leds_dri == NULL || leds_dri->dri_Font == NULL) {
        goto fail;
    }
    width = leds_layout(w, leds_dri);

    /* left of the gadgets on the right of the title bar (depth, zoom,
     * iconify...), whatever their size and flags: the system ones have no
     * GACT_TOPBORDER, found by their place. The drag bar (relative width)
     * and the left half (close gadget) are not counted. */
    left = w->Width - w->BorderRight;
    for (g = w->FirstGadget; g != NULL; g = g->NextGadget) {
        WORD x = g->LeftEdge;

        if (g->Flags & GFLG_RELRIGHT) {
            x += w->Width - 1;
        }
        AMIGA_TRACE(("title bar: gadget type 0x%04x flags 0x%04x at %d,%d %dx%d",
                     g->GadgetType, g->Flags, x, g->TopEdge, g->Width, g->Height));
        if ((g->Flags & (GFLG_RELBOTTOM | GFLG_RELWIDTH))
                || g->TopEdge >= w->BorderTop
                || ((g->GadgetType & GTYP_SYSGADGET)
                    && (g->GadgetType & GTYP_SYSTYPEMASK) == GTYP_WDRAGGING)
                || x <= w->Width / 2) {
            continue;
        }
        if (x < left) {
            left = x;
        }
    }
    /* the gadget left edge, relative to the right edge */
    right = left - (w->Width - 1);

    leds_gadget = (struct Gadget *)NewObject(leds_class, NULL,
            GA_RelRight, (ULONG)(LONG)(right - width),
            GA_Top, 0,
            GA_Width, (ULONG)width,
            GA_Height, (ULONG)w->BorderTop,
            GA_TopBorder, TRUE,
            TAG_DONE);
    if (leds_gadget == NULL) {
        goto fail;
    }
    leds_obtain_pens(w->WScreen->ViewPort.ColorMap);
    leds_window = w;
    AddGList(w, leds_gadget, (UWORD)~0, 1, NULL);
    RefreshGList(leds_gadget, w, NULL, 1);
    AMIGA_TRACE(("LED gadget %d wide, at %d from the right", width, right - width));
    return;

fail:
    if (leds_dri != NULL) {
        FreeScreenDrawInfo(w->WScreen, leds_dri);
        leds_dri = NULL;
    }
}

void amiga_leds_window_close(struct Window *w)
{
    if (w == NULL || w != leds_window) {
        return;
    }
    RemoveGList(w, leds_gadget, 1);
    DisposeObject(leds_gadget);
    leds_gadget = NULL;
    leds_release_pens(w->WScreen->ViewPort.ColorMap);
    FreeScreenDrawInfo(w->WScreen, leds_dri);
    leds_dri = NULL;
    leds_window = NULL;
    /* made again for the next window */
    if (FreeClass(leds_class)) {
        leds_class = NULL;
    }
}

void amiga_leds_window_update(struct Window *w)
{
    if (w == NULL || w != leds_window || leds_state == leds_drawn) {
        return;
    }
    /* once per change, even if the window could not be drawn into */
    leds_drawn = leds_state;
    SetGadgetAttrs(leds_gadget, w, NULL, GA_ID, 0, TAG_DONE);
}
