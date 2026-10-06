/**
 * \file video.c
 * \brief AmigaOS 3.x video: Intuition window + cybergraphics WriteLUTPixelArray()
 *
 * \author Marco van den Heuvel <blackystardust68@yahoo.com>
 * \author Michael C. Martin <mcmartin@gmail.com>
 *
 * Amiga window code: VICE AmigaOS 3.x port.
 */

/* This file is part of VICE, the Versatile Commodore Emulator.
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

#include "amigatrace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/cybergraphics.h>
#include <graphics/view.h>
#include <graphics/gfxbase.h>
#include <graphics/modeid.h>
#include <devices/inputevent.h>
#include <workbench/workbench.h>
#include <workbench/startup.h>
#include <proto/wb.h>

#include "archdep.h"
#include "cmdline.h"
#include "keyboard.h"
#include "log.h"
#include "machine.h"
#include "palette.h"
#include "resources.h"
#include "videoarch.h"
#include "video.h"
#include "viewport.h"
#include "vsync.h"

#include "amigavideo.h"
#include "amigaaction.h"
#include "amigalocale.h"
#include "amigamenu.h"
#include "amigafile.h"
#include "lib.h"
#include "util.h"
#include "archdep_default_resource_file_name.h"
#include "archdep_default_portable_resource_file_name.h"
#include "amiga_cgxscale.h"
#include "amiga_planarscale.h"
#include "amiga_screenmode.h"

/* optional: NULL when there is no RTG system (plain AGA/ECS Amiga) */
struct Library *CyberGfxBase = NULL;

/* Drawing routes, chosen from the window bitmap each time it opens:
 * - CGX scaled: any cybergraphics/P96 bitmap in a known pixel format, 8 bit
 *         (CLUT, FindColor()) included: direct CPU writes, scaled to the
 *         window size (amiga_cgxscale.c).
 * - planar scaled: no cybergraphics.library, or a native (planar) bitmap:
 *         scaled into a fast RAM chunky buffer with FindColor() pens, then
 *         WriteChunkyPixels() (amiga_planarscale.c).
 * - RTG:  cybergraphics bitmap in an unknown format: WriteLUTPixelArray(),
 *         not scaled. */
#define AMIGA_HAVE_RTG(bm) \
    (CyberGfxBase != NULL && GetCyberMapAttr((bm), CYBRMATTR_ISCYBERGFX) != 0)
static int use_cgxscale = 0;
static int use_planarscale = 0;
static int use_rtg = 0;

/* The window is kept outside of the canvas: the atexit() cleanup must never
 * touch a canvas the core may already have freed. */
static struct Window *amiga_window = NULL;

/* bit of either fullscreen window user port, or WB window user port. */
ULONG currentUIWaitBit = 0;

/* Window geometry, kept across fullscreen switches and saved in vicerc at
 * exit: position, and inner size (without the borders: the window is not
 * GimmeZeroZero, Width includes them). -1 / 0: not known yet. */
static int window_left = -1;
static int window_top = -1;
static int window_inner_w = 0;
static int window_inner_h = 0;
/* changed since the start: vicerc must be updated at exit */
static int window_geometry_changed = 0;

/* Fullscreen: a screen drawn directly into its bitmap, and an invisible
 * borderless backdrop window on it only to receive the keyboard (IDCMP).
 * amiga_window is that window while fullscreen. */
static struct Screen *fs_screen = NULL;
static UWORD *fs_pointer = NULL;        /* blank mouse pointer, chip RAM */
static int fullscreen = 0;
/* switch asked by Amiga+F, a menu or a setting, done between IDCMP rounds:
 * -1 nothing, 0 window, 1 fullscreen */
static int fullscreen_request = -1;
static int fullscreen_reopen = 0;       /* screen mode setting changed */

/* fullscreen resources */
static int fs_auto_mode = 1;
static int fs_mode_id = (int)INVALID_ID;

/* native 16/32 color fullscreen: 4 or 5 bitplanes, 0 otherwise */
static int fs_planes = 0;
/* 32 colors: copy of the screen rastport and bitmap with 4 planes only */
static struct BitMap fs_bitmap4;
static struct RastPort fs_rastport4;
/* emulator color shown in pen 0 (32 colors), -1 if not set yet */
static int fs_border_index = -1;

/* drawing target: the window inner area, or a centered part of the screen */
static struct RastPort *draw_rp = NULL;
static int draw_x = 0;
static int draw_y = 0;
static unsigned int draw_width = 0;
static unsigned int draw_height = 0;
static unsigned int palette_entries = 0;

/* Shown part of the emulated screen: the whole canvas (full borders), or
 * less border. Source pixels and draw buffer pitch never change, only the
 * starting point (crop_x, crop_y, draw buffer coordinates) and the size
 * (base_width, base_height: the 1x window size). */
static unsigned int base_width = 0;
static unsigned int base_height = 0;
static int crop_x = 0;
static int crop_y = 0;
/* top left corner of the whole canvas in the draw buffer: always border */
static int canvas_x0 = -1;
static int canvas_y0 = -1;
static int border_mode = AMIGA_BORDERS_FULL;
static int display_scale = 1;

/* to redraw everything after a resize or an exposure */
static video_canvas_t *amiga_canvas = NULL;

/* AppWindow: Workbench icons dropped on the emulator window are autostarted.
 * Only possible when the window is on the Workbench screen. */
static struct MsgPort *app_port = NULL;
static struct AppWindow *app_window = NULL;

/* palette as a CTABFMT_XRGB8 table for WriteLUTPixelArray() */
static ULONG amiga_ctab[256];

static log_t amiga_video_log = LOG_DEFAULT;
static int atexit_registered = 0;

/** \brief  Command line options related to generic video output
 */
static const cmdline_option_t cmdline_options[] =
{
    CMDLINE_LIST_END
};


/** \brief  Integer/boolean resources related to video output
 */
static int set_fs_auto_mode(int val, void *param)
{
    val = val ? 1 : 0;
    if (val != fs_auto_mode && fullscreen) {
        fullscreen_reopen = 1;
    }
    fs_auto_mode = val;
    return 0;
}

static int set_fs_mode_id(int val, void *param)
{
    if (val != fs_mode_id && fullscreen && !fs_auto_mode) {
        fullscreen_reopen = 1;
    }
    fs_mode_id = val;
    return 0;
}

static int set_window_int(int val, void *param)
{
    *(int *)param = val;
    return 0;
}

static const resource_int_t resources_int[] =
{
    { "AmigaWindowLeft", -1, RES_EVENT_NO, NULL,
      &window_left, set_window_int, &window_left },
    { "AmigaWindowTop", -1, RES_EVENT_NO, NULL,
      &window_top, set_window_int, &window_top },
    { "AmigaWindowWidth", 0, RES_EVENT_NO, NULL,
      &window_inner_w, set_window_int, &window_inner_w },
    { "AmigaWindowHeight", 0, RES_EVENT_NO, NULL,
      &window_inner_h, set_window_int, &window_inner_h },
    { "AmigaFullscreenAutoMode", 1, RES_EVENT_NO, NULL,
      &fs_auto_mode, set_fs_auto_mode, NULL },
    { "AmigaFullscreenModeID", (int)INVALID_ID, RES_EVENT_NO, NULL,
      &fs_mode_id, set_fs_mode_id, NULL },
    RESOURCE_INT_LIST_END
};


/* reply what Workbench sent, nobody will handle it anymore */
static void drain_app_port(void)
{
    struct Message *msg;

    if (app_port != NULL) {
        while ((msg = GetMsg(app_port)) != NULL) {
            ReplyMsg(msg);
        }
    }
}

static void amiga_add_app_window(void)
{
    if (WorkbenchBase == NULL || amiga_window == NULL) {
        return;
    }
    if (app_port == NULL) {
        app_port = CreateMsgPort();
        if (app_port == NULL) {
            return;
        }
    }
    /* fails (NULL) when the window is not on the Workbench screen: fine */
    app_window = AddAppWindowA(0, 0, amiga_window, app_port, NULL);
}

static void amiga_remove_app_window(void)
{
    if (app_window != NULL) {
        RemoveAppWindow(app_window);
        app_window = NULL;
    }
    drain_app_port();
}

/* remember where the window is and its inner size */
static void amiga_sync_window_geometry(void)
{
    struct Window *w = amiga_window;
    int left, top, iw, ih;

    if (w == NULL || fullscreen) {
        return;
    }
    left = w->LeftEdge;
    top = w->TopEdge;
    iw = w->Width - w->BorderLeft - w->BorderRight;
    ih = w->Height - w->BorderTop - w->BorderBottom;
    if (left != window_left || top != window_top
            || iw != window_inner_w || ih != window_inner_h) {
        window_left = left;
        window_top = top;
        window_inner_w = iw;
        window_inner_h = ih;
        window_geometry_changed = 1;
    }
}

static void amiga_close_window(void)
{
    amiga_remove_app_window();
    AmigaMenu_Close(amiga_window);
    if (amiga_window != NULL) {
        if (!fullscreen) {
            amiga_sync_window_geometry();
        }
        CloseWindow(amiga_window);
        amiga_window = NULL;
        currentUIWaitBit = 0;
    }
    draw_rp = NULL;
    draw_width = 0;
    draw_height = 0;
}

static void amiga_close_fullscreen(void)
{
    if (fs_screen == NULL) {
        return;
    }
    if (amiga_window != NULL) {
        ClearPointer(amiga_window);
    }
    amiga_close_window();
    if (fs_pointer != NULL) {
        FreeVec(fs_pointer);
        fs_pointer = NULL;
    }
    if (!CloseScreen(fs_screen)) {
        /* a foreign window is still open on it: nothing more can be done */
        log_warning(amiga_video_log, "cannot close the fullscreen screen.");
    }
    fs_screen = NULL;
    fullscreen = 0;
    fs_planes = 0;
}

/** \brief  Free every Amiga video resource, safe to call more than once
 *
 * Registered with atexit() so a plain exit() anywhere in the core still
 * closes the window and the libraries.
 */
void amiga_video_close_all(void)
{
    AMIGA_TRACE(("%s", __func__));
    amiga_close_fullscreen();
    amiga_close_window();
    planarscale_free();
    if (app_port != NULL) {
        drain_app_port();
        DeleteMsgPort(app_port);
        app_port = NULL;
    }
    if (CyberGfxBase != NULL) {
        CloseLibrary(CyberGfxBase);
        CyberGfxBase = NULL;
    }
    AMIGA_TRACE(("%s done", __func__));
}

/* inner size limits: from half the emulated screen to the whole screen */
static void amiga_set_window_limits(void)
{
    struct Window *w = amiga_window;
    int bw = w->BorderLeft + w->BorderRight;
    int bh = w->BorderTop + w->BorderBottom;

    WindowLimits(w, (WORD)(base_width / 2 + bw), (WORD)(base_height / 2 + bh),
                 (UWORD)w->WScreen->Width, (UWORD)w->WScreen->Height);
}

/* window inner size = emulated screen size x scale, within the screen */
static void amiga_apply_scale(void)
{
    struct Window *w = amiga_window;
    int bw, bh, width, height, max_w, max_h;

    if (w == NULL || base_width == 0 || fullscreen) {
        return;
    }
    bw = w->BorderLeft + w->BorderRight;
    bh = w->BorderTop + w->BorderBottom;
    width = (int)base_width * display_scale + bw;
    height = (int)base_height * display_scale + bh;
    max_w = w->WScreen->Width - w->LeftEdge;
    max_h = w->WScreen->Height - w->TopEdge;
    if (width > max_w || height > max_h) {
        /* too big from here: move to the top left corner */
        max_w = w->WScreen->Width;
        max_h = w->WScreen->Height;
        if (width > max_w) {
            width = max_w;
        }
        if (height > max_h) {
            height = max_h;
        }
        ChangeWindowBox(w, 0, 0, (WORD)width, (WORD)height);
    } else {
        ChangeWindowBox(w, w->LeftEdge, w->TopEdge, (WORD)width, (WORD)height);
    }
    /* IDCMP_NEWSIZE will follow */
}

/* the real inner size, after a resize */
static void amiga_update_inner_size(void)
{
    struct Window *w = amiga_window;

    draw_rp = w->RPort;
    draw_x = w->BorderLeft;
    draw_y = w->BorderTop;
    draw_width = (unsigned int)(w->Width - w->BorderLeft - w->BorderRight);
    draw_height = (unsigned int)(w->Height - w->BorderTop - w->BorderBottom);
}

/** \brief  Window scale from the Display menu (1 to 3)
 */
void amiga_video_set_scale(int scale)
{
    if (scale < 1 || scale > 3) {
        return;
    }
    display_scale = scale;
    if (fullscreen) {
        /* kept for the window */
        return;
    }
    if (use_rtg && amiga_window != NULL) {
        log_warning(amiga_video_log, "no scaling with this cybergraphics pixel format.");
        return;
    }
    amiga_apply_scale();
}

int amiga_video_get_scale(void)
{
    return display_scale;
}

static void amiga_redraw_all(void);
static void amiga_compute_crop(video_canvas_t *canvas,
                               unsigned int *width, unsigned int *height);
static void amiga_open_window(unsigned int width, unsigned int height);
static void amiga_fullscreen_layout(void);

/** \brief  Border mode from the Display menu (AMIGA_BORDERS_*)
 */
void amiga_video_set_borders(int mode)
{
    unsigned int width, height;

    border_mode = mode;
    if (amiga_canvas == NULL) {
        return;
    }
    amiga_compute_crop(amiga_canvas, &width, &height);
    /* new 1x size: the window follows (scale kept) */
    amiga_open_window(width, height);
    amiga_redraw_all();
}

int amiga_video_get_borders(void)
{
    return border_mode;
}

/** \brief  Compute the shown part of the canvas from the border mode
 *
 * Uses the VICE geometry, so PAL and NTSC borders differ as they should.
 * Draw buffer coordinates, like video_canvas_refresh_all().
 */
static void amiga_compute_crop(video_canvas_t *canvas,
                               unsigned int *width, unsigned int *height)
{
    viewport_t *vp = canvas->viewport;
    geometry_t *g = canvas->geometry;
    int el = (int)g->extra_offscreen_border_left;
    /* the whole canvas */
    int cx = (int)vp->first_x + el - (int)vp->x_offset;
    int cy = (int)vp->first_line - (int)vp->y_offset;
    int cw = (int)canvas->draw_buffer->canvas_physical_width;
    int ch = (int)canvas->draw_buffer->canvas_physical_height;
    /* the 320x200 graphics area inside it */
    int gx = (int)g->gfx_position.x + el;
    int gy = (int)g->gfx_position.y;
    int left = gx - cx;
    int top = gy - cy;
    int right = (cx + cw) - (gx + (int)g->gfx_size.width);
    int bottom = (cy + ch) - (gy + (int)g->gfx_size.height);

    if (left < 0) {
        left = 0;
    }
    if (top < 0) {
        top = 0;
    }
    if (right < 0) {
        right = 0;
    }
    if (bottom < 0) {
        bottom = 0;
    }
    switch (border_mode) {
        case AMIGA_BORDERS_NONE:
            break;
        case AMIGA_BORDERS_HALF:
            left /= 2;
            top /= 2;
            right /= 2;
            bottom /= 2;
            break;
        default:
            left = top = right = bottom = 0;
            break;
    }
    canvas_x0 = cx;
    canvas_y0 = cy;
    crop_x = cx + left;
    crop_y = cy + top;
    *width = (unsigned int)(cw - left - right);
    *height = (unsigned int)(ch - top - bottom);
    AMIGA_TRACE(("borders %d: shown area %d,%d %ux%u", border_mode, crop_x, crop_y,
                 *width, *height));
}

/* Drawing route from the target bitmap, window or fullscreen alike */
static void amiga_select_route(struct BitMap *bm, struct ColorMap *cm)
{
    use_cgxscale = cgxscale_prepare(bm, cm);
    /* cybergraphics.library is optional: without it, or for a native
     * bitmap, the planar route is used */
    use_planarscale = !use_cgxscale && !AMIGA_HAVE_RTG(bm);
    use_rtg = !use_cgxscale && !use_planarscale;
    if (use_cgxscale) {
        log_message(amiga_video_log, "RTG screen: scaled direct drawing, pixel format %lu.",
                    (unsigned long)GetCyberMapAttr(bm, CYBRMATTR_PIXFMT));
        cgxscale_set_palette(amiga_ctab, (int)palette_entries);
    } else if (use_planarscale) {
        if (planarscale_prepare(cm)) {
            log_message(amiga_video_log, "native screen: scaled chunky buffer and WriteChunkyPixels().");
            planarscale_set_palette(amiga_ctab, (int)palette_entries);
        } else {
            log_error(amiga_video_log, "WriteChunkyPixels() needs graphics.library v40 (OS3.1).");
            use_planarscale = 0;
        }
    } else {
        log_message(amiga_video_log, "RTG screen: unknown pixel format, WriteLUTPixelArray() not scaled.");
    }
}

/** \brief  Open the emulator window, or adapt it to a new emulated size
 *
 * \param[in]   width   emulated screen width (1x)
 * \param[in]   height  emulated screen height (1x)
 */
static void amiga_open_window(unsigned int width, unsigned int height)
{
    int known_size;

    if (width == 0 || height == 0) {
        return;
    }
    if (fullscreen) {
        /* PAL <-> NTSC, borders: same screen, new layout */
        if (base_width != width || base_height != height) {
            base_width = width;
            base_height = height;
            amiga_fullscreen_layout();
        }
        return;
    }
    if (amiga_window != NULL) {
        if (base_width != width || base_height != height) {
            /* PAL <-> NTSC... keep the window, change its size */
            base_width = width;
            base_height = height;
            amiga_set_window_limits();
            if (use_cgxscale || use_planarscale) {
                amiga_apply_scale();
            } else {
                ChangeWindowBox(amiga_window, amiga_window->LeftEdge, amiga_window->TopEdge,
                                (WORD)(width + amiga_window->BorderLeft + amiga_window->BorderRight),
                                (WORD)(height + amiga_window->BorderTop + amiga_window->BorderBottom));
            }
        }
        return;
    }
    base_width = width;
    base_height = height;
    /* the last size (fullscreen switch, previous run), else 1x */
    known_size = (window_inner_w > 0 && window_inner_h > 0);

    AMIGA_TRACE(("open window %ux%u at %d,%d", known_size ? (unsigned)window_inner_w : width,
                 known_size ? (unsigned)window_inner_h : height, window_left, window_top));
    amiga_window = OpenWindowTags(NULL,
            WA_Title, (ULONG)LOC(MSG_WINDOW_TITLE),
            (window_left >= 0 ? WA_Left : TAG_IGNORE), window_left,
            (window_top >= 0 ? WA_Top : TAG_IGNORE), window_top,
            /* inner size: independent of the border sizes */
            WA_InnerWidth, known_size ? (unsigned)window_inner_w : width,
            WA_InnerHeight, known_size ? (unsigned)window_inner_h : height,
            /* the Workbench may have changed since: move and shrink to fit */
            WA_AutoAdjust, TRUE,
            WA_DragBar, TRUE,
            WA_DepthGadget, TRUE,
            WA_CloseGadget, TRUE,
            WA_SizeGadget, TRUE,
            WA_SizeBBottom, TRUE,
            WA_Activate, TRUE,
            WA_NewLookMenus, TRUE,
            /* simple refresh: amiga_cgxscale.c relies on it (no off-screen
             * cliprects), exposed parts are redrawn on IDCMP_REFRESHWINDOW */
            WA_SimpleRefresh, TRUE,
            WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_RAWKEY | IDCMP_INACTIVEWINDOW
                      | IDCMP_MENUPICK | IDCMP_NEWSIZE | IDCMP_REFRESHWINDOW
                      | IDCMP_CHANGEWINDOW,
            TAG_DONE);
    if (amiga_window == NULL) {
        log_error(amiga_video_log, "cannot open a %ux%u window.", width, height);
        return;
    }

    amiga_update_inner_size();
    amiga_set_window_limits();

    AmigaMenu_Create(amiga_window);
    amiga_add_app_window();

    currentUIWaitBit = (1UL << amiga_window->UserPort->mp_SigBit);
    if(app_port) currentUIWaitBit |= (1UL << app_port->mp_SigBit);

    amiga_select_route(amiga_window->RPort->BitMap, amiga_window->WScreen->ViewPort.ColorMap);
    if (use_cgxscale || use_planarscale) {
        if (!known_size && display_scale != 1) {
            amiga_apply_scale();
        }
    } else if (known_size) {
        /* not scaled: the window is the emulated screen size */
        ChangeWindowBox(amiga_window, amiga_window->LeftEdge, amiga_window->TopEdge,
                        (WORD)(width + amiga_window->BorderLeft + amiga_window->BorderRight),
                        (WORD)(height + amiga_window->BorderTop + amiga_window->BorderBottom));
    }
}

/* redraw the whole window: after a resize or when a hidden part shows */
static void amiga_redraw_all(void)
{
    if (amiga_canvas == NULL || draw_rp == NULL) {
        return;
    }
    if (use_rtg && !fullscreen) {
        /* not scaled: clear what the emulated screen does not cover */
        EraseRect(amiga_window->RPort, amiga_window->BorderLeft, amiga_window->BorderTop,
                  amiga_window->Width - amiga_window->BorderRight - 1,
                  amiga_window->Height - amiga_window->BorderBottom - 1);
    }
    video_canvas_refresh_all(amiga_canvas);
}

/* LoadRGB32() / SA_Colors32 table for the fullscreen palette */
static ULONG fs_colors[1 + 256 * 3 + 1];

static void fs_color(ULONG *entry, ULONG rgb)
{
    entry[0] = ((rgb >> 16) & 0xff) * 0x01010101UL;
    entry[1] = ((rgb >> 8) & 0xff) * 0x01010101UL;
    entry[2] = (rgb & 0xff) * 0x01010101UL;
}

/* Fullscreen palette, by screen kind:
 * - 5 planes (32 colors): emulator colors in pens 16-31, pen 0 is the
 *   emulator border color (also the Amiga overscan border), plane 5 is set
 *   to 1 in the drawing area and only planes 1-4 are written.
 * - 4 planes (16 colors): emulator colors in pens 0-15.
 * - others (RTG, deep native): emulator colors in pens 0-n, FindColor().
 * Returns the table, NULL if there is nothing to load. */
static ULONG *amiga_build_fs_palette(int max_pens)
{
    ULONG n = palette_entries;
    ULONG first = 0;
    ULONG i;

    if (n == 0) {
        return NULL;
    }
    if (fs_planes == 5) {
        first = 16;
    }
    if (first + n > (ULONG)max_pens) {
        n = (first < (ULONG)max_pens) ? (ULONG)max_pens - first : 0;
    }
    /* first entry: from pen 0 to fill pen 0 too in 32 color mode */
    if (fs_planes == 5) {
        fs_colors[0] = (first + n) << 16;
        for (i = 0; i < first; i++) {
            fs_color(&fs_colors[1 + i * 3], 0);
        }
        fs_color(&fs_colors[1], amiga_ctab[fs_border_index >= 0 ? fs_border_index : 0]);
    } else {
        fs_colors[0] = n << 16;
    }
    for (i = 0; i < n; i++) {
        fs_color(&fs_colors[1 + (first + i) * 3], amiga_ctab[i]);
    }
    fs_colors[1 + (first + n) * 3] = 0;
    return fs_colors;
}

static void amiga_load_screen_palette(void)
{
    ULONG *table;

    if (fs_screen == NULL) {
        return;
    }
    table = amiga_build_fs_palette(fs_screen->ViewPort.ColorMap->Count);
    if (table != NULL) {
        LoadRGB32(&fs_screen->ViewPort, table);
    }
}

/* 32 color screen: pen 0 follows the emulator border, read in the draw
 * buffer at the top left corner of the canvas (always border) */
static void amiga_update_fs_border(const draw_buffer_t *db)
{
    int index;
    ULONG rgb[3];

    if (fs_planes != 5 || canvas_x0 < 0 || canvas_y0 < 0) {
        return;
    }
    index = db->draw_buffer[(ULONG)canvas_y0 * db->draw_buffer_pitch + (ULONG)canvas_x0];
    if (index == fs_border_index || index >= (int)palette_entries) {
        return;
    }
    fs_border_index = index;
    fs_color(rgb, amiga_ctab[index]);
    SetRGB32(&fs_screen->ViewPort, 0, rgb[0], rgb[1], rgb[2]);
}

/* fullscreen drawing area: biggest integer scale that fits, else scaled down
 * keeping the aspect, centered. Not scaled on the WriteLUTPixelArray() route. */
static void amiga_fullscreen_layout(void)
{
    struct RastPort *srp = &fs_screen->RastPort;
    int sw = fs_screen->Width;
    int sh = fs_screen->Height;
    int bw = (int)base_width;
    int bh = (int)base_height;
    int w, h, scale;

    if (bw <= 0 || bh <= 0) {
        return;
    }
    if (use_rtg) {
        w = (bw < sw) ? bw : sw;
        h = (bh < sh) ? bh : sh;
    } else {
        scale = (sw / bw < sh / bh) ? sw / bw : sh / bh;
        if (scale >= 1) {
            w = bw * scale;
            h = bh * scale;
        } else if (sw * bh <= sh * bw) {
            w = sw;
            h = bh * sw / bw;
        } else {
            h = sh;
            w = bw * sh / bh;
        }
    }
    draw_x = (sw - w) / 2;
    draw_y = (sh - h) / 2;
    draw_width = (unsigned int)w;
    draw_height = (unsigned int)h;

    /* pen 0 around: emulator border (32 colors) or color 0 */
    SetRast(srp, 0);
    if (fs_planes == 5) {
        /* plane 5 set in the drawing area: pens 16-31 */
        SetAPen(srp, 16);
        RectFill(srp, draw_x, draw_y, draw_x + w - 1, draw_y + h - 1);
        /* a copy that only has planes 1-4: WriteChunkyPixels() with the
         * emulator color indexes 0-15 keeps plane 5 */
        fs_bitmap4 = *srp->BitMap;
        fs_bitmap4.Depth = 4;
        fs_rastport4 = *srp;
        fs_rastport4.BitMap = &fs_bitmap4;
        fs_rastport4.Layer = NULL;
        draw_rp = &fs_rastport4;
    } else {
        draw_rp = srp;
    }
    AMIGA_TRACE(("fullscreen %dx%d, drawing %dx%d at %d,%d", sw, sh, w, h, draw_x, draw_y));
}

/* open the screen and its input window, 0 on success */
static int amiga_open_fullscreen(void)
{
    ULONG mode_id;
    int manual = !fs_auto_mode && (ULONG)fs_mode_id != INVALID_ID;
    int native, depth, sw, sh;
    int colors = (palette_entries > 0) ? (int)palette_entries : 16;
    char name[64];
    ULONG *colors32;

    if (manual) {
        mode_id = (ULONG)fs_mode_id;
    } else {
        mode_id = amiga_screenmode_auto((int)base_width, (int)base_height, colors, &depth);
    }
    log_message(amiga_video_log, "fullscreen: %s mode 0x%08lx.",
                manual ? "chosen" : "automatic", (unsigned long)mode_id);
    if (mode_id == INVALID_ID || !amiga_screenmode_size(mode_id, &sw, &sh)) {
        log_error(amiga_video_log, "no usable fullscreen mode for %ux%u.", base_width, base_height);
        return -1;
    }
    /* no depth asked: 16 or 32 colors for a native palette mode, else the
     * mode's own depth (RTG) or 8 */
    native = !amiga_screenmode_is_rtg(mode_id);
    if (native && colors <= 16) {
        depth = amiga_screenmode_native_depth(mode_id);
        fs_planes = (depth >= 5) ? 5 : 4;
        depth = fs_planes;
    } else {
        depth = amiga_screenmode_depth(mode_id, 8);
        fs_planes = 0;
    }
    fs_border_index = -1;
    amiga_screenmode_name(mode_id, name, sizeof name);
    colors32 = amiga_build_fs_palette(1 << (depth > 8 ? 8 : depth));

    fs_screen = OpenScreenTags(NULL,
            SA_DisplayID, mode_id,
            SA_Width, sw,
            SA_Height, sh,
            SA_Depth, depth,
            /* palette from the start, no flash */
            (colors32 != NULL ? SA_Colors32 : TAG_IGNORE), (ULONG)colors32,
            SA_Title, (ULONG)LOC(MSG_WINDOW_TITLE),
            SA_ShowTitle, FALSE,
            SA_Quiet, TRUE,
            SA_Type, CUSTOMSCREEN,
            TAG_DONE);
    if (fs_screen == NULL) {
        log_error(amiga_video_log, "cannot open the fullscreen mode %s (%dx%d, %d bits).",
                  name, sw, sh, depth);
        fs_planes = 0;
        return -1;
    }
    /* input only: never drawn into, the screen bitmap is used directly */
    amiga_window = OpenWindowTags(NULL,
            WA_CustomScreen, (ULONG)fs_screen,
            WA_Left, 0,
            WA_Top, 0,
            WA_Width, sw,
            WA_Height, sh,
            WA_Borderless, TRUE,
            WA_Backdrop, TRUE,
            /* no menus: they would be drawn over */
            WA_RMBTrap, TRUE,
            WA_Activate, TRUE,
            WA_SimpleRefresh, TRUE,
            WA_NoCareRefresh, TRUE,
            WA_IDCMP, IDCMP_RAWKEY | IDCMP_INACTIVEWINDOW,
            TAG_DONE);
    if (amiga_window == NULL) {
        log_error(amiga_video_log, "cannot open the fullscreen input window.");
        CloseScreen(fs_screen);
        fs_screen = NULL;
        fs_planes = 0;
        return -1;
    }
    /* 1 line high blank sprite: control words, 1 line, end */
    fs_pointer = AllocVec(6 * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
    if (fs_pointer != NULL) {
        SetPointer(amiga_window, fs_pointer, 1, 16, 0, 0);
    }
    fullscreen = 1;

    if (fs_planes != 0) {
        /* emulator color i is written as i: no pen remapping */
        use_cgxscale = 0;
        use_rtg = 0;
        use_planarscale = planarscale_prepare(NULL);
        if (use_planarscale) {
            planarscale_set_palette(amiga_ctab, (int)palette_entries);
        } else {
            log_error(amiga_video_log, "WriteChunkyPixels() needs graphics.library v40 (OS3.1).");
        }
    } else {
        amiga_select_route(fs_screen->RastPort.BitMap, fs_screen->ViewPort.ColorMap);
    }
    amiga_fullscreen_layout();
    log_message(amiga_video_log, "fullscreen: %s, %dx%d, %d colors.", name, sw, sh,
                1 << (depth > 24 ? 24 : depth));

    currentUIWaitBit = 1UL << amiga_window->UserPort->mp_SigBit;


    return 0;
}

/* done outside of the IDCMP loop: it closes the window it reads */
static void amiga_process_fullscreen_request(void)
{
    int want = fullscreen_request;

    if (fullscreen_reopen && fullscreen && want == -1) {
        want = 1;
    }
    fullscreen_request = -1;
    if (want == -1 || (want == fullscreen && !fullscreen_reopen)) {
        fullscreen_reopen = 0;
        return;
    }
    fullscreen_reopen = 0;
    AMIGA_TRACE(("switch to %s", want ? "fullscreen" : "window"));

    if (fullscreen) {
        amiga_close_fullscreen();
    } else {
        amiga_close_window();
    }
    if (!want || amiga_open_fullscreen() != 0) {
        /* window asked, or fullscreen failed: back to the window */
        amiga_open_window(base_width, base_height);
    }
    /* key releases went to the old window */
    keyboard_key_clear();
    vsync_suspend_speed_eval();
    amiga_redraw_all();
}

/** \brief  Ask a switch between window and fullscreen, done at the next
 *          event round (Amiga+F, Display menu)
 */
void amiga_video_set_fullscreen(int on)
{
    fullscreen_request = on ? 1 : 0;
}

int amiga_video_is_fullscreen(void)
{
    return fullscreen;
}

/* rawkey codes */
#define AMIGA_RAWKEY_F9 0x58
#define AMIGA_RAWKEY_F 0x23

/** \brief  Signal mask of the emulator window IDCMP port, 0 if no window
 */
// ULONG amiga_video_signal_mask(void)
// {
//     ULONG mask = 0;

//     if (amiga_window != NULL) {
//         mask |= 1UL << amiga_window->UserPort->mp_SigBit;
//     }
//     if (app_window != NULL) {
//         mask |= 1UL << app_port->mp_SigBit;
//     }
//     return mask;
// }

/** \brief  The emulator window, for requesters (may be NULL)
 */
struct Window *amiga_video_window(void)
{
    /* requesters must not open on the fullscreen: it is drawn over */
    return fullscreen ? NULL : amiga_window;
}

/** \brief  Autostart the first icon dropped on the emulator window
 */
static void amiga_handle_app_messages(void)
{
    struct AppMessage *am;
    char *path = NULL;

    if (app_port == NULL) {
        return;
    }
    while ((am = (struct AppMessage *)GetMsg(app_port)) != NULL) {
        /* the locks belong to Workbench: get the path before replying */
        if (path == NULL && am->am_Type == AMTYPE_APPWINDOW && am->am_NumArgs > 0) {
            path = amiga_wbarg_path(&am->am_ArgList[0]);
        }
        ReplyMsg((struct Message *)am);
    }
    if (path != NULL) {
        amiga_autostart_file(path);
        lib_free(path);
        /* keys go to the emulator again */
        if (amiga_window != NULL) {
            ActivateWindow(amiga_window);
        }
    }
}

/** \brief  Convert Intuition qualifiers to VICE KBD_MOD_* flags
 */
static int amiga_key_mods(UWORD qualifier)
{
    int mods = 0;

    if (qualifier & IEQUALIFIER_LSHIFT) {
        mods |= KBD_MOD_LSHIFT;
    }
    if (qualifier & IEQUALIFIER_RSHIFT) {
        mods |= KBD_MOD_RSHIFT;
    }
    if (qualifier & IEQUALIFIER_CONTROL) {
        mods |= KBD_MOD_LCTRL;
    }
    if (qualifier & IEQUALIFIER_LALT) {
        mods |= KBD_MOD_LALT;
    }
    if (qualifier & IEQUALIFIER_RALT) {
        mods |= KBD_MOD_RALT;
    }
    if (qualifier & IEQUALIFIER_CAPSLOCK) {
        mods |= KBD_MOD_SHIFTLOCK;
    }
    return mods;
}

/** \brief  Handle the window IDCMP messages, called once per frame
 */
void amiga_video_handle_events(void)
{
    struct IntuiMessage *msg;
    int quit = 0;

    amiga_handle_app_messages();

    if (amiga_window == NULL) {
        amiga_process_fullscreen_request();
        return;
    }
    while ((msg = (struct IntuiMessage *)GetMsg(amiga_window->UserPort)) != NULL) {
        ULONG class = msg->Class;
        UWORD code = msg->Code;
        UWORD qualifier = msg->Qualifier;

        ReplyMsg((struct Message *)msg);

        switch (class) {
            case IDCMP_CLOSEWINDOW:
                quit = 1;
                break;
            case IDCMP_MENUPICK:
                /* menu browsing locks the screen layers and blocks our
                 * rendering: don't let vsync catch up the lost time */
                vsync_suspend_speed_eval();
                AmigaMenu_HandlePick(amiga_window, code);
                break;
            case IDCMP_NEWSIZE:
                amiga_update_inner_size();
                AMIGA_TRACE(("window inner size %ux%u", draw_width, draw_height));
                amiga_redraw_all();
                break;
            case IDCMP_CHANGEWINDOW:
                /* end of a window drag/resize, same layer lock issue */
                vsync_suspend_speed_eval();
                break;
            case IDCMP_REFRESHWINDOW:
                BeginRefresh(amiga_window);
                EndRefresh(amiga_window, TRUE);
                amiga_redraw_all();
                break;
            case IDCMP_RAWKEY:
                /* the C64 KERNAL does its own key repeat */
                if (qualifier & IEQUALIFIER_REPEAT) {
                    break;
                }
                /* F9 is not a C64 key: UI shortcut */
                if ((code & 0x7f) == AMIGA_RAWKEY_F9) {
                    if (!(code & IECODE_UP_PREFIX)) {
                        AmigaAction_Execute(AMIGA_ACTION_SETTINGS);
                    }
                    break;
                }
                /* Amiga+F: window <-> fullscreen. In the window it comes as
                 * the menu shortcut, the fullscreen window has no menu. */
                if ((code & 0x7f) == AMIGA_RAWKEY_F
                        && (qualifier & (IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND))) {
                    if (!(code & IECODE_UP_PREFIX)) {
                        fullscreen_request = !fullscreen;
                    }
                    break;
                }
                /*
                AMIGA_TRACE(("rawkey 0x%02x %s qualifier 0x%04x", code & 0x7f,
                             (code & IECODE_UP_PREFIX) ? "up" : "down", qualifier));
                */
                if (code & IECODE_UP_PREFIX) {
                    keyboard_key_released((signed long)(code & 0x7f), amiga_key_mods(qualifier));
                } else {
                    keyboard_key_pressed((signed long)(code & 0x7f), amiga_key_mods(qualifier));
                }
                break;
            case IDCMP_INACTIVEWINDOW:
                /* key releases go to the new active window: avoid stuck keys */
                keyboard_key_clear();
                break;
            default:
                break;
        }
    }
    if (quit) {
        AMIGA_TRACE(("close gadget"));
        archdep_vice_exit(0);
    }
    amiga_process_fullscreen_request();
}

/** \brief  Arch-sepcific function to check which chip is
 *          currently "active", or has the focus of the user.
 *
 * x64 only has the VIC-II.
 */
int video_arch_get_active_chip(void)
{
    return VIDEO_CHIP_VICII;
}

/** \brief  Arch-specific initialization for a video canvas
 *  \param[inout] canvas The canvas being initialized
 *  \sa video_canvas_create
 */
void video_arch_canvas_init(struct video_canvas_s *canvas)
{
    AMIGA_TRACE(("%s", __func__));
}


/** \brief  Initialize command line options for generic video resouces
 *
 * \return  0 on success, < 0 on failure
 */
int video_arch_cmdline_options_init(void)
{
    AMIGA_TRACE(("%s", __func__));

    if (machine_class != VICE_MACHINE_VSID) {
        return cmdline_register_options(cmdline_options);
    }
    return 0;
}


/** \brief  Initialize video-related resources
 *
 * \return  0 on success, < on failure
 */
int video_arch_resources_init(void)
{
    AMIGA_TRACE(("%s", __func__));

    if (machine_class != VICE_MACHINE_VSID) {
        return resources_register_int(resources_int);
    }
    return 0;
}

/** \brief Clean up any memory held by arch-specific video resources. */
void video_arch_resources_shutdown(void)
{
    AMIGA_TRACE(("%s", __func__));
}

/** \brief Query whether a canvas is resizable.
 *  \param canvas The canvas to query
 *  \return TRUE if the canvas can be resized.
 *
 * Yes: the window follows the size of the emulated screen.
 */
char video_canvas_can_resize(video_canvas_t *canvas)
{
    return 1;
}

/** \brief Create a new video_canvas_s.
 *  \param[inout] canvas A freshly allocated canvas object.
 *  \param[in]    width  Pointer to a width value. May be NULL if canvas
 *                       size is not yet known.
 *  \param[in]    height Pointer to a height value. May be NULL if canvas
 *                       size is not yet known.
 *  \param        mapped Unused.
 *  \return The completely initialized canvas.
 *
 * The window is opened by video_canvas_resize(), once the core knows the
 * visible size of the emulated screen.
 */
video_canvas_t *video_canvas_create(video_canvas_t *canvas,
                                    unsigned int *width, unsigned int *height,
                                    int mapped)
{
    AMIGA_TRACE(("%s %ux%u", __func__,
                 width != NULL ? *width : 0, height != NULL ? *height : 0));

    canvas->created = 1;
    amiga_canvas = canvas;

    return canvas;
}

/** \brief Free a previously created video canvas and all its
 *         components.
 *  \param[in] canvas The canvas to destroy.
 */
void video_canvas_destroy(struct video_canvas_s *canvas)
{
    AMIGA_TRACE(("%s", __func__));

    amiga_close_window();
    if (amiga_canvas == canvas) {
        amiga_canvas = NULL;
    }
}

/** \brief Update the display on a video canvas to reflect the machine
 *         state.
 * \param canvas The canvas to update.
 * \param xs     X coordinate in the draw buffer
 * \param ys     Y coordinate in the draw buffer
 * \param xi     X coordinate of the leftmost pixel to update in the window
 * \param yi     Y coordinate of the topmost pixel to update in the window
 * \param w      Width of the rectangle to update
 * \param h      Height of the rectangle to update
 *
 * The draw buffer is 8 bit palettized: on RTG screens WriteLUTPixelArray()
 * converts it with the palette table, on native screens it is remapped to
 * pens. No VICE renderer involved either way.
 */
/* draw a dirty rectangle (draw buffer coordinates) in the drawing area */
static void amiga_draw_rect(const draw_buffer_t *db, unsigned int xs, unsigned int ys,
                            unsigned int w, unsigned int h)
{
    /* dirty rectangle (draw buffer coordinates) inside the shown area */
    int sx0 = (int)xs;
    int sy0 = (int)ys;
    int sx1 = (int)(xs + w);
    int sy1 = (int)(ys + h);
    int bw = (int)base_width;
    int bh = (int)base_height;
    int bl = draw_x;
    int bt = draw_y;
    int rx0, ry0, rx1, ry1;

    if (sx0 < crop_x) {
        sx0 = crop_x;
    }
    if (sy0 < crop_y) {
        sy0 = crop_y;
    }
    if (sx1 > crop_x + bw) {
        sx1 = crop_x + bw;
    }
    if (sy1 > crop_y + bh) {
        sy1 = crop_y + bh;
    }
    if (sx0 >= sx1 || sy0 >= sy1 || bw <= 0 || bh <= 0) {
        return;
    }
    /* relative to the shown area */
    rx0 = sx0 - crop_x;
    ry0 = sy0 - crop_y;
    rx1 = sx1 - crop_x;
    ry1 = sy1 - crop_y;

    if (use_cgxscale || use_planarscale) {
        int dw = (int)draw_width;
        int dh = (int)draw_height;

        /* shown area -> whole window inner area, dirty part only */
        (use_cgxscale ? cgxscale_draw : planarscale_draw)(
                      draw_rp, db->draw_buffer, db->draw_buffer_pitch,
                      crop_x, crop_y, bw, bh,
                      bl, bt, dw, dh,
                      bl + rx0 * dw / bw,
                      bt + ry0 * dh / bh,
                      bl + (rx1 * dw + bw - 1) / bw,
                      bt + (ry1 * dh + bh - 1) / bh);
        return;
    }
    if (!use_rtg) {
        /* no drawing route (OS 3.0 without cybergraphics) */
        return;
    }

    /* 1x routes: clip to the window */
    if (rx0 >= (int)draw_width || ry0 >= (int)draw_height) {
        return;
    }
    if (rx1 > (int)draw_width) {
        rx1 = (int)draw_width;
    }
    if (ry1 > (int)draw_height) {
        ry1 = (int)draw_height;
    }
    WriteLUTPixelArray(db->draw_buffer, (UWORD)sx0, (UWORD)sy0,
                       (UWORD)db->draw_buffer_pitch,
                       draw_rp, amiga_ctab,
                       (UWORD)(bl + rx0), (UWORD)(bt + ry0),
                       (UWORD)(rx1 - rx0), (UWORD)(ry1 - ry0), CTABFMT_XRGB8);
}

void video_canvas_refresh(struct video_canvas_s *canvas,
                          unsigned int xs, unsigned int ys,
                          unsigned int xi, unsigned int yi,
                          unsigned int w, unsigned int h)
{
    draw_buffer_t *db;

/* per frame trace, too slow for real machine tests: enable when needed */
#if 0
    static unsigned int refresh_count = 0;

    if ((refresh_count++ % 50) == 0) {
        AMIGA_TRACE(("%s #%u src %u,%u dst %u,%u (%ux%u)", __func__,
                     refresh_count, xs, ys, xi, yi, w, h));
    }
#endif
    if (canvas == NULL || draw_rp == NULL) {
        return;
    }
    db = canvas->draw_buffer;
    if (db == NULL || db->draw_buffer == NULL) {
        return;
    }
    if (fs_planes == 5) {
        amiga_update_fs_border(db);
    }

    if (!fullscreen && amiga_window != NULL) {
        /* The size must be the current one: after a resize, Intuition has
         * already drawn the borders before IDCMP_NEWSIZE reaches us, and the
         * borders are in the same layer (not GimmeZeroZero): drawing with
         * the old bigger size would cover them. Locked, the layer cannot be
         * resized between reading the size and drawing. */
        struct Layer *layer = amiga_window->WLayer;

        LockLayer(0, layer);
        amiga_update_inner_size();
        amiga_draw_rect(db, xs, ys, w, h);
        UnlockLayer(layer);
    } else {
        amiga_draw_rect(db, xs, ys, w, h);
    }
}

/** \brief Update canvas size to match the draw buffer size requested
 *         by the emulation core.
 * \param canvas The video canvas to update.
 * \param resize_canvas Ignored - the canvas will always resize.
 */
void video_canvas_resize(struct video_canvas_s *canvas, char resize_canvas)
{
    unsigned int width, height;

    AMIGA_TRACE(("%s %ux%u", __func__, canvas->draw_buffer->canvas_physical_width,
                 canvas->draw_buffer->canvas_physical_height));

    amiga_canvas = canvas;
    amiga_compute_crop(canvas, &width, &height);
    amiga_open_window(width, height);
}

/** \brief Assign a palette to the canvas.
 * \param canvas The canvas to update the palette
 * \param palette The new palette to assign
 * \return Zero on success, nonzero on failure
 */
int video_canvas_set_palette(struct video_canvas_s *canvas,
                             struct palette_s *palette)
{
    unsigned int i;

    AMIGA_TRACE(("%s", __func__));

    canvas->palette = palette;
    if (palette == NULL) {
        return 0;
    }
    for (i = 0; i < palette->num_entries && i < 256; i++) {
        amiga_ctab[i] = ((ULONG)palette->entries[i].red << 16)
                      | ((ULONG)palette->entries[i].green << 8)
                      | (ULONG)palette->entries[i].blue;
    }
    palette_entries = i;
    if (fullscreen) {
        amiga_load_screen_palette();
    }
    if (use_cgxscale) {
        cgxscale_set_palette(amiga_ctab, (int)palette_entries);
    } else if (use_planarscale) {
        planarscale_set_palette(amiga_ctab, (int)palette_entries);
    }
    return 0;
}

/** \brief Perform any frontend-specific initialization.
 *  \return 0 on success, nonzero on failure
 */
int video_init(void)
{
    AMIGA_TRACE(("%s", __func__));

    amiga_video_log = log_open("AmigaVideo");

    if (!atexit_registered) {
        atexit(amiga_video_close_all);
        atexit_registered = 1;
    }

    /* optional, a NULL CyberGfxBase is accepted */
    CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 1);
    AMIGA_TRACE(("cybergraphics.library %s", CyberGfxBase != NULL ? "opened" : "not available"));
    return 0;
}

/* the vicerc VICE would save to (resources_save() logic) */
static char *amiga_vicerc_name(void)
{
    char *name;

    if (vice_config_file != NULL) {
        return lib_strdup(vice_config_file);
    }
    name = archdep_default_portable_resource_file_name();
    if (name != NULL && archdep_access(name, ARCHDEP_ACCESS_R_OK) == 0) {
        return name;
    }
    lib_free(name);
    return archdep_default_resource_file_name();
}

static int is_window_geometry_line(const char *line)
{
    static const char *const names[] = {
        "AmigaWindowLeft=", "AmigaWindowTop=", "AmigaWindowWidth=", "AmigaWindowHeight="
    };
    int i;

    for (i = 0; i < 4; i++) {
        if (strncmp(line, names[i], strlen(names[i])) == 0) {
            return 1;
        }
    }
    return 0;
}

/** \brief  Write the window geometry in vicerc, nothing else
 *
 * Not resources_save(): settings only applied with "Use" must not be saved
 * when quitting. The geometry lines of this machine section are replaced,
 * the rest of the file is copied as it is.
 */
static void amiga_save_window_geometry(void)
{
    char *fname;
    char *old = NULL;
    size_t old_len = 0;
    FILE *f;
    char section[64];
    char geometry[160];
    int in_section = 0;
    int written = 0;
    char *line, *next;

    if (!window_geometry_changed || window_inner_w <= 0 || window_inner_h <= 0) {
        return;
    }
    fname = amiga_vicerc_name();
    if (fname == NULL) {
        return;
    }
    snprintf(section, sizeof section, "[%s]", machine_name);
    snprintf(geometry, sizeof geometry,
             "AmigaWindowLeft=%d\nAmigaWindowTop=%d\nAmigaWindowWidth=%d\nAmigaWindowHeight=%d\n",
             window_left, window_top, window_inner_w, window_inner_h);

    /* the whole old file in memory */
    f = fopen(fname, "rb");
    if (f != NULL) {
        if (fseek(f, 0, SEEK_END) == 0) {
            long len = ftell(f);

            if (len > 0) {
                old = lib_malloc((size_t)len + 1);
                fseek(f, 0, SEEK_SET);
                old_len = fread(old, 1, (size_t)len, f);
                old[old_len] = '\0';
            }
        }
        fclose(f);
    }

    f = fopen(fname, "wb");
    if (f == NULL) {
        log_error(amiga_video_log, "cannot write the window geometry to %s.", fname);
        lib_free(old);
        lib_free(fname);
        return;
    }
    if (old == NULL) {
        /* no vicerc yet: VICE wants the version section */
        fprintf(f, "[Version]\nConfigVersion=%s\n\n", VERSION);
    } else {
        for (line = old; line != NULL && *line != '\0'; line = next) {
            size_t n;

            next = strchr(line, '\n');
            n = (next != NULL) ? (size_t)(next - line) : strlen(line);
            if (next != NULL) {
                next++;
            }
            if (line[0] == '[') {
                in_section = (strncmp(line, section, strlen(section)) == 0);
                fwrite(line, 1, n, f);
                fputc('\n', f);
                if (in_section && !written) {
                    fputs(geometry, f);
                    written = 1;
                }
                continue;
            }
            if (in_section && is_window_geometry_line(line)) {
                continue;
            }
            fwrite(line, 1, n, f);
            fputc('\n', f);
        }
    }
    if (!written) {
        fprintf(f, "%s%s\n%s", (old != NULL) ? "\n" : "", section, geometry);
    }
    fclose(f);
    AMIGA_TRACE(("window geometry saved: %d,%d %dx%d", window_left, window_top,
                 window_inner_w, window_inner_h));
    lib_free(old);
    lib_free(fname);
    window_geometry_changed = 0;
}

/** \brief Perform any frontend-specific uninitialization. */
void video_shutdown(void)
{
    AMIGA_TRACE(("%s", __func__));

    /* the window is closed by now (video_canvas_destroy()) */
    amiga_sync_window_geometry();
    amiga_save_window_geometry();

    amiga_video_close_all();
}
