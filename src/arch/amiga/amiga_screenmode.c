/** \file   amiga_screenmode.c
 * \brief   AmigaOS 3.x port: fullscreen display mode helpers
 *
 * The RTG search is the AmigaMame one (OwnCGXBestModeID): BestCModeIDTagList()
 * gives odd results with recent P96 drivers, so the modes are scanned here,
 * height first, then width.
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

#include <exec/types.h>
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>

#include "amiga_screenmode.h"

extern struct Library *CyberGfxBase;

/* RTG modes only, without the P96 "ghost" modes (low word 0) */
static int rtg_mode_ok(ULONG id, ULONG depth)
{
    if (!IsCyberModeID(id) || (id & 0x0000ffff) == 0) {
        return 0;
    }
    return GetCyberIDAttr(CYBRIDATTR_DEPTH, id) == depth;
}

/* smallest RTG mode of this depth at least w x h: nearest height first */
static ULONG rtg_best_mode(ULONG w, ULONG h, ULONG depth)
{
    ULONG min_height_excluded = 0;

    for (;;) {
        ULONG id;
        ULONG best_height = 0;
        ULONG best_height_err = 0xffffffffUL;
        ULONG best_mode = INVALID_ID;
        ULONG best_width_err = 0xffffffffUL;

        for (id = NextDisplayInfo(INVALID_ID); id != INVALID_ID; id = NextDisplayInfo(id)) {
            ULONG hm;

            if (!rtg_mode_ok(id, depth)) {
                continue;
            }
            hm = GetCyberIDAttr(CYBRIDATTR_HEIGHT, id);
            if (hm >= h && hm > min_height_excluded && hm - h < best_height_err) {
                best_height_err = hm - h;
                best_height = hm;
            }
        }
        if (best_height == 0) {
            return INVALID_ID;
        }
        for (id = NextDisplayInfo(INVALID_ID); id != INVALID_ID; id = NextDisplayInfo(id)) {
            ULONG wm, pixfmt, err;

            if (!rtg_mode_ok(id, depth)
                    || GetCyberIDAttr(CYBRIDATTR_HEIGHT, id) != best_height) {
                continue;
            }
            wm = GetCyberIDAttr(CYBRIDATTR_WIDTH, id);
            if (wm < w) {
                continue;
            }
            err = (wm - w) * 2;
            /* same size: prefer the big endian formats, faster to write */
            pixfmt = GetCyberIDAttr(CYBRIDATTR_PIXFMT, id);
            if ((depth == 16 && pixfmt != PIXFMT_RGB16)
                    || (depth == 32 && pixfmt != PIXFMT_ARGB32)) {
                err++;
            }
            if (err < best_width_err) {
                best_width_err = err;
                best_mode = id;
            }
        }
        if (best_mode != INVALID_ID) {
            return best_mode;
        }
        /* no mode wide enough at this height: try the next height */
        min_height_excluded = best_height;
    }
}

ULONG amiga_screenmode_auto(int width, int height, int colors_needed, int *depth)
{
    static const int rtg_depths[] = { 8, 16, 32 };
    static const int native_depths[] = { 8, 5, 4 };
    ULONG id;
    int i;

    if (CyberGfxBase != NULL) {
        for (i = 0; i < 3; i++) {
            id = rtg_best_mode((ULONG)width, (ULONG)height, (ULONG)rtg_depths[i]);
            if (id != INVALID_ID) {
                *depth = rtg_depths[i];
                return id;
            }
        }
    }

    /* native: 32 colors (16 on ECS hires) for a 16 color palette */
    for (i = (colors_needed <= 16) ? 1 : 0; i < 3; i++) {
        id = BestModeID(BIDTAG_NominalWidth, width,
                        BIDTAG_NominalHeight, height,
                        BIDTAG_Depth, native_depths[i],
                        TAG_DONE);
        if (id != INVALID_ID) {
            *depth = native_depths[i];
            return id;
        }
    }
    return INVALID_ID;
}

int amiga_screenmode_is_rtg(ULONG mode_id)
{
    return CyberGfxBase != NULL && mode_id != INVALID_ID && IsCyberModeID(mode_id);
}

int amiga_screenmode_native_depth(ULONG mode_id)
{
    struct DimensionInfo dims;

    if (GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof dims, DTAG_DIMS, mode_id) == 0) {
        return 4;
    }
    /* every mode does 32 colors, but OCS/ECS hires: 16 */
    return (dims.MaxDepth >= 5) ? 5 : 4;
}

int amiga_screenmode_size(ULONG mode_id, int *width, int *height)
{
    struct DimensionInfo dims;

    if (mode_id == INVALID_ID
            || GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof dims, DTAG_DIMS, mode_id) == 0) {
        return 0;
    }
    *width = dims.Nominal.MaxX - dims.Nominal.MinX + 1;
    *height = dims.Nominal.MaxY - dims.Nominal.MinY + 1;
    return (*width > 0 && *height > 0);
}

int amiga_screenmode_depth(ULONG mode_id, int asked)
{
    struct DimensionInfo dims;
    int max_depth;

    if (GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof dims, DTAG_DIMS, mode_id) == 0) {
        return asked;
    }
    max_depth = (int)dims.MaxDepth;
    /* hi/true color modes only open with their own depth (ASL may also
     * return an old depth for them) */
    if (max_depth > 8) {
        return (max_depth > 24) ? 24 : max_depth;
    }
    if (asked < 1 || asked > max_depth) {
        return max_depth;
    }
    return asked;
}

void amiga_screenmode_name(ULONG mode_id, char *buf, int size)
{
    struct NameInfo ni;

    if (mode_id != INVALID_ID
            && GetDisplayInfoData(NULL, (UBYTE *)&ni, sizeof ni, DTAG_NAME, mode_id) != 0) {
        snprintf(buf, (size_t)size, "%s", (const char *)ni.Name);
    } else {
        snprintf(buf, (size_t)size, "0x%08lx", (unsigned long)mode_id);
    }
}
