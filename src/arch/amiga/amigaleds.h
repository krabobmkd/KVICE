/*
 * amigaleds.h - drive and tape LEDs: window title bar gadget, fullscreen
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

#ifndef VICE_AMIGALEDS_H
#define VICE_AMIGALEDS_H

#include <exec/types.h>
#include <intuition/intuition.h>

/* LED state bits */
#define AMIGA_LED_DRIVE_ON   0x01   /* drive 8 usable: emulated or virtual */
#define AMIGA_LED_DRIVE_ACT  0x02   /* drive 8 LED, or virtual drive bytes */
#define AMIGA_LED_DRIVE_JAM  0x04   /* drive 8 CPU jammed (until reset) */
#define AMIGA_LED_TAPE_MOTOR 0x08   /* datasette motor on */

/* pens given to amiga_leds_draw_fullscreen() */
enum {
    AMIGA_LED_PEN_OFF,      /* black */
    AMIGA_LED_PEN_GREEN,
    AMIGA_LED_PEN_RED,
    AMIGA_LED_PEN_YELLOW,
    AMIGA_LED_PENS
};
/* 0x00RRGGBB of each AMIGA_LED_PEN_* */
extern const ULONG amiga_led_rgb[AMIGA_LED_PENS];

/* from uistatusbar.c */
void amiga_leds_set(ULONG bits, int on);
/* from serial/serial-trap.c: a byte of the virtual drive \a device */
void amiga_leds_virtual_activity(unsigned int device);

/* once per frame: polls what has no UI call (drive jam, virtual drive),
 * returns the AMIGA_LED_* state */
ULONG amiga_leds_poll(void);

/* the title bar gadget of a window, left of its depth gadget */
void amiga_leds_window_open(struct Window *w);
void amiga_leds_window_close(struct Window *w);
/* redraws the gadget if the state changed since it was drawn: not with
 * the window layer locked */
void amiga_leds_window_update(struct Window *w);

/* fullscreen: the LEDs in a row, their bottom right corner at right,
 * bottom (included) */
void amiga_leds_draw_fullscreen(struct RastPort *rp, WORD right, WORD bottom,
                                WORD led_w, WORD led_h, const LONG *pens);

#endif
