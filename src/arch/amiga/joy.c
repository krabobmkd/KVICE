/** \file   joy.c
 * \brief   AmigaOS 3.x joystick driver
 *
 * Each Amiga controller port can be configured (resources AmigaJoyPort0..3,
 * lowlevel.library port numbering: 0 = mouse port, 1 = joystick port, 2/3 =
 * extra lowlevel ports) as:
 *
 * - 0: not used
 * - 1: digital joystick (2 buttons: red, blue), read with lowlevel.library
 *      ReadJoyPort()
 * - 3: CD32 pad (7 buttons), read with lowlevel.library ReadJoyPort()
 * - 2: analog paddles / proportional joystick on the DB9 POT pins of the
 *      joystick port (1) only, read by the VBlank interrupt of
 *      amiga_propjoy.c (code from AmigaMame). The mouse port belongs to
 *      input.device (Workbench mouse): taking it is dangerous, not done.
 * - 4: Amiga mouse as 2 paddles (X, Y), read with lowlevel.library
 *      ReadJoyPort() in mouse mode, on any port.
 *
 * The analog ones drive the C64 "Paddles" device, which reads them as
 * joystick axes (PaddlesInputN, set by the settings Input page).
 *
 * Each used port becomes one VICE host joystick device. Devices are
 * registered in the order port 1, port 0, port 2, port 3, so by default the
 * Amiga joystick port drives C64 control port 2 (JoyDevice2) and the Amiga
 * mouse port, if used, drives C64 control port 1 (JoyDevice1).
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

#include <exec/types.h>
#include <libraries/lowlevel.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/lowlevel.h>

#include "amigatrace.h"
#include "amiga_propjoy.h"
#include "amigajoy.h"
#include "cmdline.h"
#include "joyport.h"
#include "joystick.h"
#include "keyboard.h"
#include "lib.h"
#include "log.h"
#include "machine.h"
#include "resources.h"

/* opened here when needed: lowlevel.library is optional */
struct Library *LowLevelBase = NULL;

#define AMIGA_JOY_PORTS 4

#define AMIGA_JOY_TYPE_NONE     0
#define AMIGA_JOY_TYPE_JOYSTICK 1
#define AMIGA_JOY_TYPE_PADDLES  2
#define AMIGA_JOY_TYPE_CD32     3
#define AMIGA_JOY_TYPE_MOUSE    4
#define AMIGA_JOY_TYPE_MAX      AMIGA_JOY_TYPE_MOUSE

/* read with lowlevel.library ReadJoyPort() */
#define IS_LOWLEVEL_TYPE(t) ((t) == AMIGA_JOY_TYPE_JOYSTICK || (t) == AMIGA_JOY_TYPE_CD32 \
                             || (t) == AMIGA_JOY_TYPE_MOUSE)
/* C64 paddles: 2 axes, 2 buttons */
#define IS_ANALOG_TYPE(t) ((t) == AMIGA_JOY_TYPE_PADDLES || (t) == AMIGA_JOY_TYPE_MOUSE)

/* mouse as paddles: mouse counts per paddle step, as VICE does with a host
 * mouse (mouse_paddle.c PADDLE_DIV), else games are too fast */
#define MOUSE_PADDLE_DIV 2
#define MOUSE_PADDLE_MAX (256 * MOUSE_PADDLE_DIV - 1)

/* resources: controller type per Amiga port */
static int amiga_port_type[AMIGA_JOY_PORTS];

/* registration order: joystick port first, it becomes VICE device 0 */
static const int port_order[AMIGA_JOY_PORTS] = { 1, 0, 2, 3 };

/* lowlevel buttons, in VICE button order: the default mapping makes button
 * 0 the fire button, 1 and 2 the extra fire buttons on POTX/POTY (C64 2nd
 * and 3rd fire: the 2 button pads read by SID POTX). A joystick has the
 * first 2 (blue on pin 9), a CD32 pad all of them. */
static const struct {
    ULONG mask;
    const char *name;
} ll_buttons[] = {
    { JPF_BUTTON_RED,     "Red (fire)" },
    { JPF_BUTTON_BLUE,    "Blue" },
    { JPF_BUTTON_GREEN,   "Green" },
    { JPF_BUTTON_YELLOW,  "Yellow" },
    { JPF_BUTTON_PLAY,    "Play" },
    { JPF_BUTTON_FORWARD, "Forward" },
    { JPF_BUTTON_REVERSE, "Reverse" }
};
#define LL_NUM_BUTTONS (sizeof ll_buttons / sizeof ll_buttons[0])
#define LL_JOYSTICK_BUTTONS 2

/* per device private data */
typedef struct amiga_joy_priv_s {
    int port;           /**< Amiga port, lowlevel numbering */
    int type;           /**< AMIGA_JOY_TYPE_* */
    int index;          /**< VICE joystick device index */
    /* mouse as paddles: last lowlevel counters (-1: none read yet), and
     * the paddle positions, 0..MOUSE_PADDLE_MAX */
    int mouse_last[2];
    int mouse_pos[2];
    /* SPACE held by the blue button ("AmigaJoy<n>Bt2Space") */
    int space_down;
} amiga_joy_priv_t;

static log_t amiga_joy_log = LOG_DEFAULT;
static struct ProportionalSticks *propsticks = NULL;
static int lowlevel_ports_used = 0;     /* bitmask of ports set by us */
static int atexit_registered = 0;

/* ------------------------------------------------------------------------- */
/* resources and command line */

static int set_port_type(int val, void *param)
{
    int port = vice_ptr_to_int(param);

    if (val < AMIGA_JOY_TYPE_NONE || val > AMIGA_JOY_TYPE_MAX) {
        return -1;
    }
    if (val == AMIGA_JOY_TYPE_PADDLES && port != 1) {
        /* only the joystick port: the mouse port is input.device's, ports
           2/3 have no POT pins. An older configuration may have it: none. */
        val = AMIGA_JOY_TYPE_NONE;
    }
    /* taken into account at the start, or by amiga_joy_reconfigure() */
    amiga_port_type[port] = val;
    return 0;
}

/* "AmigaJoy1Bt2Space", "AmigaJoy2Bt2Space": the blue button of the
 * controller of emulated control port 1 / 2 presses SPACE (games that want
 * SPACE: smart bomb, start...) instead of the 2nd fire */
static int bt2_space[2] = { 0, 0 };

static int set_bt2_space(int val, void *param)
{
    bt2_space[vice_ptr_to_int(param)] = val ? 1 : 0;
    return 0;
}

void amiga_joy_set_bt2_space(int port, int on)
{
    resources_set_int_sprintf("AmigaJoy%dBt2Space", on ? 1 : 0, port);
}

int amiga_joy_get_bt2_space(int port)
{
    return (port == 1 || port == 2) ? bt2_space[port - 1] : 0;
}

static const resource_int_t resources_int[] = {
    { "AmigaJoy1Bt2Space", 0, RES_EVENT_NO, NULL,
      &bt2_space[0], set_bt2_space, (void *)0 },
    { "AmigaJoy2Bt2Space", 0, RES_EVENT_NO, NULL,
      &bt2_space[1], set_bt2_space, (void *)1 },
    { "AmigaJoyPort0", AMIGA_JOY_TYPE_NONE, RES_EVENT_NO, NULL,
      &amiga_port_type[0], set_port_type, (void *)0 },
    { "AmigaJoyPort1", AMIGA_JOY_TYPE_JOYSTICK, RES_EVENT_NO, NULL,
      &amiga_port_type[1], set_port_type, (void *)1 },
    { "AmigaJoyPort2", AMIGA_JOY_TYPE_NONE, RES_EVENT_NO, NULL,
      &amiga_port_type[2], set_port_type, (void *)2 },
    { "AmigaJoyPort3", AMIGA_JOY_TYPE_NONE, RES_EVENT_NO, NULL,
      &amiga_port_type[3], set_port_type, (void *)3 },
    RESOURCE_INT_LIST_END
};

#define PORT_TYPE_HELP "0: none, 1: joystick, 3: CD32 pad, 4: mouse as paddles (lowlevel.library), 2: analog paddles (joystick port only)"

static const cmdline_option_t cmdline_options[] = {
    { "-amigajoyport0", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "AmigaJoyPort0", NULL,
      "<type>", "Amiga mouse port (0) controller type. " PORT_TYPE_HELP },
    { "-amigajoyport1", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "AmigaJoyPort1", NULL,
      "<type>", "Amiga joystick port (1) controller type. " PORT_TYPE_HELP },
    { "-amigajoyport2", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "AmigaJoyPort2", NULL,
      "<type>", "lowlevel.library port 2 controller type (0, 1, 3 or 4)" },
    { "-amigajoyport3", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "AmigaJoyPort3", NULL,
      "<type>", "lowlevel.library port 3 controller type (0, 1, 3 or 4)" },
    CMDLINE_LIST_END
};

int amiga_joy_resources_init(void)
{
    return resources_register_int(resources_int);
}

int amiga_joy_cmdline_options_init(void)
{
    return cmdline_register_options(cmdline_options);
}

/* ------------------------------------------------------------------------- */
/* driver */

static bool amiga_joy_open(joystick_device_t *joydev)
{
    return true;
}

/* the blue button presses SPACE for a control port of this device */
static int bt2_does_space(const joystick_device_t *joydev)
{
    int p = joydev->joyport;
    int p2 = joydev->joyport2;

    return (p >= JOYPORT_1 && p <= JOYPORT_2 && bt2_space[p - JOYPORT_1])
           || (p2 >= JOYPORT_1 && p2 <= JOYPORT_2 && bt2_space[p2 - JOYPORT_1]);
}

/* SPACE in the keyboard matrix: row 0 on the VIC-20, 7 elsewhere */
static void space_key(amiga_joy_priv_t *priv, int down)
{
    if (down != priv->space_down) {
        keyboard_set_keyarr_any(machine_class == VICE_MACHINE_VIC20 ? 0 : 7, 4, down);
        priv->space_down = down;
    }
}

static void amiga_joy_poll_lowlevel(joystick_device_t *joydev, amiga_joy_priv_t *priv)
{
    int space;
    ULONG state;
    ULONG type;
    int32_t hat = JOYSTICK_DIRECTION_NONE;
    unsigned int i;

    if (LowLevelBase == NULL) {
        return;
    }
    state = ReadJoyPort((ULONG)priv->port);
    type = state & JP_TYPE_MASK;
    if (type != JP_TYPE_JOYSTK && type != JP_TYPE_GAMECTLR) {
        /* nothing (or a mouse) plugged: report everything released */
        state = 0;
    }

    if (state & JPF_JOY_UP) {
        hat |= JOYSTICK_DIRECTION_UP;
    }
    if (state & JPF_JOY_DOWN) {
        hat |= JOYSTICK_DIRECTION_DOWN;
    }
    if (state & JPF_JOY_LEFT) {
        hat |= JOYSTICK_DIRECTION_LEFT;
    }
    if (state & JPF_JOY_RIGHT) {
        hat |= JOYSTICK_DIRECTION_RIGHT;
    }
    joy_hat_event(joydev->hats[0], hat);

    space = bt2_does_space(joydev);
    for (i = 0; i < LL_NUM_BUTTONS && i < (unsigned int)joydev->num_buttons; i++) {
        int pressed = (state & ll_buttons[i].mask) ? 1 : 0;

        if (i == 1 && space) {
            /* blue: SPACE, not the 2nd fire */
            joy_button_event(joydev->buttons[i], 0);
            space_key(priv, pressed);
        } else {
            joy_button_event(joydev->buttons[i], pressed);
        }
    }
    if (!space) {
        /* turned off while held */
        space_key(priv, 0);
    }
}

/* lowlevel mouse mode: no position, the 8 bit hardware counters of each
 * axis (bits 0-7 X, 8-15 Y), wrapping. Their change since the last frame
 * (wrapped back: a frame never moves 128 counts) moves the paddles, held
 * at their ends. */
static void amiga_joy_poll_mouse(joystick_device_t *joydev, amiga_joy_priv_t *priv)
{
    ULONG state;
    int axis;

    if (LowLevelBase == NULL) {
        return;
    }
    state = ReadJoyPort((ULONG)priv->port);
    if ((state & JP_TYPE_MASK) != JP_TYPE_MOUSE) {
        return;
    }
    for (axis = 0; axis < 2; axis++) {
        int count = (int)((state >> (axis * 8)) & 0xff);

        if (priv->mouse_last[axis] >= 0) {
            int delta = count - priv->mouse_last[axis];
            int pos;

            if (delta < -128) {
                delta += 256;
            } else if (delta > 128) {
                delta -= 256;
            }
            pos = priv->mouse_pos[axis] + delta;
            if (pos < 0) {
                pos = 0;
            } else if (pos > MOUSE_PADDLE_MAX) {
                pos = MOUSE_PADDLE_MAX;
            }
            priv->mouse_pos[axis] = pos;
        }
        priv->mouse_last[axis] = count;
        /* right (down) turns the paddle down, as VICE with a host mouse */
        joystick_set_axis_value((unsigned int)priv->index, (unsigned int)axis,
                                (uint8_t)(255 - priv->mouse_pos[axis] / MOUSE_PADDLE_DIV));
    }
    joy_button_event(joydev->buttons[0], (state & JPF_BUTTON_RED) ? 1 : 0);
    joy_button_event(joydev->buttons[1], (state & JPF_BUTTON_BLUE) ? 1 : 0);
}

static void amiga_joy_poll_paddles(joystick_device_t *joydev, amiga_joy_priv_t *priv)
{
    struct PPSticksValues *v;

    if (propsticks == NULL) {
        return;
    }
    /* cheap: just reads what the VBlank interrupt stored */
    ProportionalSticksUpdate(propsticks);
    v = &propsticks->_values[priv->port];
    if (!v->valid) {
        return;
    }
    joystick_set_axis_value((unsigned int)priv->index, 0, (uint8_t)v->x);
    joystick_set_axis_value((unsigned int)priv->index, 1, (uint8_t)v->y);
    joy_button_event(joydev->buttons[0], (v->bt & 1) ? 1 : 0);
    joy_button_event(joydev->buttons[1], (v->bt & 2) ? 1 : 0);
}

static void amiga_joy_poll(joystick_device_t *joydev)
{
    amiga_joy_priv_t *priv = joydev->priv;

    if (priv->type == AMIGA_JOY_TYPE_MOUSE) {
        amiga_joy_poll_mouse(joydev, priv);
    } else if (IS_LOWLEVEL_TYPE(priv->type)) {
        amiga_joy_poll_lowlevel(joydev, priv);
    } else if (priv->type == AMIGA_JOY_TYPE_PADDLES) {
        amiga_joy_poll_paddles(joydev, priv);
    }
}

static void amiga_joy_close(joystick_device_t *joydev)
{
}

static void amiga_joy_priv_free(void *priv)
{
    /* no SPACE left held by a device gone (ports reconfigured) */
    space_key(priv, 0);
    lib_free(priv);
}

/* called by the core after its default mapping: paddles are not directions */
static void amiga_joy_customize(joystick_device_t *joydev)
{
    amiga_joy_priv_t *priv = joydev->priv;
    int i;

    if (!IS_ANALOG_TYPE(priv->type)) {
        return;
    }
    for (i = 0; i < joydev->num_axes && i < 2; i++) {
        joystick_axis_t *axis = joydev->axes[i];

        axis->mapping.negative.action = JOY_ACTION_NONE;
        axis->mapping.positive.action = JOY_ACTION_NONE;
        axis->mapping.pot = (unsigned int)(i + 1);
    }
    /* The C64 paddle fire buttons are the joystick left/right lines, but
     * the VICE Paddles device reading joystick axes (PaddlesInputN) takes
     * them from fire 1 / fire 2 and moves them there itself
     * (mouse_paddle.c joyport_paddles_value(): (value & 0x30) >> 2): mapped
     * to left/right here, they would be lost. */
    joydev->buttons[0]->mapping.action = JOY_ACTION_JOYSTICK;
    joydev->buttons[0]->mapping.value.joy_pin = JOYPORT_FIRE;
    joydev->buttons[1]->mapping.action = JOY_ACTION_JOYSTICK;
    joydev->buttons[1]->mapping.value.joy_pin = JOYPORT_FIRE_POTX;
}

static const joystick_driver_t amiga_joy_driver = {
    .open      = amiga_joy_open,
    .poll      = amiga_joy_poll,
    .close     = amiga_joy_close,
    .priv_free = amiga_joy_priv_free,
    .customize = amiga_joy_customize
};

/* ------------------------------------------------------------------------- */
/* init / shutdown */

/** \brief  Free every Amiga input resource, safe to call more than once
 *
 * Registered with atexit(): the paddle code installs a VBlank interrupt that
 * must never outlive the program.
 */
static void amiga_joy_close_all(void)
{
    int port;

    if (propsticks != NULL) {
        closeProportionalSticks(propsticks);
        propsticks = NULL;
    }
    if (LowLevelBase != NULL) {
        for (port = 0; port < AMIGA_JOY_PORTS; port++) {
            if (lowlevel_ports_used & (1 << port)) {
                SetJoyPortAttrs((ULONG)port, SJA_Reinitialize, 0, TAG_DONE);
            }
        }
        lowlevel_ports_used = 0;
        CloseLibrary(LowLevelBase);
        LowLevelBase = NULL;
    }
}

static void propjoy_log(int elevel, const char *message)
{
    log_warning(amiga_joy_log, "%s", message);
}

/* port name as on the Amiga and in the settings, not lowlevel numbers */
static const char *port_name(int port)
{
    switch (port) {
        case 0:  return "Amiga mouse port";
        case 1:  return "Amiga joystick port";
        case 2:  return "Amiga extra port 3";
        default: return "Amiga extra port 4";
    }
}

static void register_device(int port, int type)
{
    joystick_device_t *joydev;
    amiga_joy_priv_t *priv;
    unsigned int i;

    joydev = joystick_device_new();
    priv = lib_malloc(sizeof *priv);
    priv->port = port;
    priv->type = type;
    priv->index = joystick_device_count();
    priv->mouse_last[0] = priv->mouse_last[1] = -1;
    priv->mouse_pos[0] = priv->mouse_pos[1] = MOUSE_PADDLE_MAX / 2;
    priv->space_down = 0;
    joydev->priv = priv;
    joydev->disable_sort = true;

    if (IS_LOWLEVEL_TYPE(type) && type != AMIGA_JOY_TYPE_MOUSE) {
        unsigned int buttons = type == AMIGA_JOY_TYPE_CD32 ? LL_NUM_BUTTONS : LL_JOYSTICK_BUTTONS;

        joydev->name = lib_msprintf("%s %s", port_name(port),
                                    type == AMIGA_JOY_TYPE_CD32 ? "CD32 pad" : "joystick");
        joystick_device_add_hat(joydev, joystick_hat_new("Directions"));
        for (i = 0; i < buttons; i++) {
            joystick_button_t *button = joystick_button_new(ll_buttons[i].name);

            button->code = i;
            joystick_device_add_button(joydev, button);
        }
    } else {
        joystick_axis_t *axis;

        joydev->name = lib_msprintf("%s %s", port_name(port),
                                    type == AMIGA_JOY_TYPE_MOUSE ? "mouse" : "analog");
        for (i = 0; i < 2; i++) {
            axis = joystick_axis_new(i == 0 ? "Paddle X" : "Paddle Y");
            axis->code = i;
            axis->minimum = 0;
            axis->maximum = 255;
            joystick_device_add_axis(joydev, axis);
        }
        joystick_device_add_button(joydev, joystick_button_new("Paddle X button"));
        joystick_device_add_button(joydev, joystick_button_new("Paddle Y button"));
    }

    if (!joystick_device_register(joydev)) {
        log_error(amiga_joy_log, "cannot register %s.", joydev->name);
        joystick_device_free(joydev);
        return;
    }
    log_message(amiga_joy_log, "VICE joystick device %d: %s.", priv->index, joydev->name);
}

static const char *lowlevel_type_name(ULONG type)
{
    switch (type) {
        case JP_TYPE_GAMECTLR: return "CD32 pad";
        case JP_TYPE_MOUSE:    return "mouse";
        case JP_TYPE_JOYSTK:   return "joystick";
        case JP_TYPE_NOTAVAIL: return "nothing yet";
        default:               return "unknown";
    }
}

/* The controller type of each lowlevel port is forced as chosen: autosense
 * (SJA_TYPE_AUTOSENSE, the default) does not work well and misreads
 * controllers. lowlevel.library needs a couple of frames before the first
 * reads are right. */
static void amiga_joy_set_lowlevel_types(void)
{
    int port;

    if (LowLevelBase == NULL || lowlevel_ports_used == 0) {
        return;
    }
    for (port = 0; port < AMIGA_JOY_PORTS; port++) {
        if (lowlevel_ports_used & (1 << port)) {
            ULONG type = SJA_TYPE_JOYSTK;

            if (amiga_port_type[port] == AMIGA_JOY_TYPE_CD32) {
                type = SJA_TYPE_GAMECTLR;
            } else if (amiga_port_type[port] == AMIGA_JOY_TYPE_MOUSE) {
                type = SJA_TYPE_MOUSE;
            }
            SetJoyPortAttrs((ULONG)port, SJA_Type, type, TAG_DONE);
        }
    }
    WaitTOF();
    WaitTOF();
    for (port = 0; port < AMIGA_JOY_PORTS; port++) {
        if (lowlevel_ports_used & (1 << port)) {
            log_message(amiga_joy_log, "%s: %s.", port_name(port),
                        lowlevel_type_name(ReadJoyPort((ULONG)port) & JP_TYPE_MASK));
        }
    }
}

/* open lowlevel.library and the analog reader for the ports as configured,
 * and register a VICE joystick device for each used port */
static void amiga_joy_open_ports(void)
{
    ULONG paddle_flags = 0;
    ULONG retcode = PROPJOYRET_OK;
    int i, port;

    for (port = 0; port < AMIGA_JOY_PORTS; port++) {
        if (IS_LOWLEVEL_TYPE(amiga_port_type[port]) && LowLevelBase == NULL) {
            LowLevelBase = OpenLibrary((CONST_STRPTR)"lowlevel.library", 0);
            if (LowLevelBase == NULL) {
                log_error(amiga_joy_log, "cannot open lowlevel.library, no digital joysticks.");
            }
        }
        if (amiga_port_type[port] == AMIGA_JOY_TYPE_PADDLES) {
            paddle_flags |= (ULONG)PROPJOYFLAGS_PORT1 << port;
        }
    }
    if (paddle_flags != 0) {
        /* the paddle ports may still be lowlevel's (as joystick or pad
         * before, or by another program): SJA_Reinitialize gives back its
         * potgo bits, else gameport.device and AllocPotBits() fail.
         * lowlevel.library is opened just for that if no port needs it. */
        struct Library *ll = LowLevelBase;

        if (ll == NULL) {
            ll = OpenLibrary((CONST_STRPTR)"lowlevel.library", 0);
        }
        if (ll != NULL) {
            struct Library *saved = LowLevelBase;

            LowLevelBase = ll;
            for (port = 0; port < AMIGA_JOY_PORTS; port++) {
                if (amiga_port_type[port] == AMIGA_JOY_TYPE_PADDLES) {
                    SetJoyPortAttrs((ULONG)port, SJA_Reinitialize, 0, TAG_DONE);
                }
            }
            LowLevelBase = saved;
            if (ll != saved) {
                CloseLibrary(ll);
            }
        }
        propsticks = createProportionalSticks(paddle_flags, PROPJOYTIMER_VBLANK_ADDINT,
                                              &retcode, propjoy_log);
        if (propsticks == NULL) {
            log_error(amiga_joy_log, "analog paddles: %s",
                      getProportionalStickErrorMessage(retcode));
        }
    }

    for (i = 0; i < AMIGA_JOY_PORTS; i++) {
        port = port_order[i];
        switch (amiga_port_type[port]) {
            case AMIGA_JOY_TYPE_JOYSTICK:
            case AMIGA_JOY_TYPE_CD32:
            case AMIGA_JOY_TYPE_MOUSE:
                if (LowLevelBase != NULL) {
                    lowlevel_ports_used |= 1 << port;
                    register_device(port, amiga_port_type[port]);
                }
                break;
            case AMIGA_JOY_TYPE_PADDLES:
                /* one gameport unit may fail while the other works */
                if (propsticks != NULL && propsticks->_ports[port]._deviceresult == 0) {
                    register_device(port, AMIGA_JOY_TYPE_PADDLES);
                }
                break;
            default:
                break;
        }
    }
    amiga_joy_set_lowlevel_types();
    AMIGA_TRACE(("%s: %d device(s)", __func__, joystick_device_count()));
}

void joystick_arch_init(void)
{
    amiga_joy_log = log_open("AmigaJoy");
    if (!atexit_registered) {
        atexit(amiga_joy_close_all);
        atexit_registered = 1;
    }
    joystick_driver_register(&amiga_joy_driver);
    amiga_joy_open_ports();
}

/* the Amiga ports again as configured now (settings "Apply"): the inputs
 * are closed, then opened again. The C64 ports must be given their device
 * again after this (JoyDevice1/2), see amiga_joy_device_index(). */
void amiga_joy_reconfigure(void)
{
    log_message(amiga_joy_log, "reconfiguring the Amiga ports.");
    joystick_devices_unregister_all();
    amiga_joy_close_all();
    amiga_joy_open_ports();
}

/* VICE joystick device index of an Amiga port (lowlevel numbering), -1 if
 * the port has none (not used, or it could not be opened) */
int amiga_joy_device_index(int port)
{
    int i;

    for (i = 0; i < joystick_device_count(); i++) {
        joystick_device_t *joydev = joystick_device_by_index(i);

        if (joydev != NULL && joydev->priv != NULL
                && ((amiga_joy_priv_t *)joydev->priv)->port == port) {
            return i;
        }
    }
    return -1;
}

/* Amiga port (lowlevel numbering) of a VICE joystick device, -1 if none */
int amiga_joy_device_port(int index)
{
    joystick_device_t *joydev;

    if (index < 0 || index >= joystick_device_count()) {
        return -1;
    }
    joydev = joystick_device_by_index(index);
    if (joydev == NULL || joydev->priv == NULL) {
        return -1;
    }
    return ((amiga_joy_priv_t *)joydev->priv)->port;
}

/* 1 when the device of an Amiga port reads analog paddles */
int amiga_joy_port_is_analog(int port)
{
    int index = amiga_joy_device_index(port);

    return index >= 0
           && IS_ANALOG_TYPE(((amiga_joy_priv_t *)joystick_device_by_index(index)->priv)->type);
}

void joystick_arch_shutdown(void)
{
    AMIGA_TRACE(("%s", __func__));
    amiga_joy_close_all();
}
