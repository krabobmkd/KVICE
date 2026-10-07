/** \file   joy.c
 * \brief   AmigaOS 3.x joystick driver
 *
 * Each Amiga controller port can be configured (resources AmigaJoyPort0..3,
 * lowlevel.library port numbering: 0 = mouse port, 1 = joystick port, 2/3 =
 * extra lowlevel ports) as:
 *
 * - 0: not used
 * - 1: digital joystick or CD32 pad, read with lowlevel.library ReadJoyPort()
 * - 2: analog paddles / proportional joystick on the DB9 POT pins of the
 *      joystick port (1) only, read by the VBlank interrupt of
 *      amiga_propjoy.c (code from AmigaMame). The mouse port belongs to
 *      input.device (Workbench mouse): taking it is dangerous, not done. To use them in the emulation, select the "Paddles"
 *      device on the C64 control port and "-paddlesNinputjoyaxis".
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
#include <proto/lowlevel.h>

#include "amigatrace.h"
#include "amiga_propjoy.h"
#include "amigajoy.h"
#include "cmdline.h"
#include "joyport.h"
#include "joystick.h"
#include "lib.h"
#include "log.h"
#include "resources.h"

/* opened here when needed: lowlevel.library is optional */
struct Library *LowLevelBase = NULL;

#define AMIGA_JOY_PORTS 4

#define AMIGA_JOY_TYPE_NONE     0
#define AMIGA_JOY_TYPE_LOWLEVEL 1
#define AMIGA_JOY_TYPE_PADDLES  2
#define AMIGA_JOY_TYPE_MAX      AMIGA_JOY_TYPE_PADDLES

/* resources: controller type per Amiga port */
static int amiga_port_type[AMIGA_JOY_PORTS];

/* registration order: joystick port first, it becomes VICE device 0 */
static const int port_order[AMIGA_JOY_PORTS] = { 1, 0, 2, 3 };

/* lowlevel buttons, in VICE button order: the default mapping makes button
 * 0 the fire button, 1 and 2 the extra fire buttons on POTX/POTY */
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

/* per device private data */
typedef struct amiga_joy_priv_s {
    int port;           /**< Amiga port, lowlevel numbering */
    int type;           /**< AMIGA_JOY_TYPE_* */
    int index;          /**< VICE joystick device index */
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

static const resource_int_t resources_int[] = {
    { "AmigaJoyPort0", AMIGA_JOY_TYPE_NONE, RES_EVENT_NO, NULL,
      &amiga_port_type[0], set_port_type, (void *)0 },
    { "AmigaJoyPort1", AMIGA_JOY_TYPE_LOWLEVEL, RES_EVENT_NO, NULL,
      &amiga_port_type[1], set_port_type, (void *)1 },
    { "AmigaJoyPort2", AMIGA_JOY_TYPE_NONE, RES_EVENT_NO, NULL,
      &amiga_port_type[2], set_port_type, (void *)2 },
    { "AmigaJoyPort3", AMIGA_JOY_TYPE_NONE, RES_EVENT_NO, NULL,
      &amiga_port_type[3], set_port_type, (void *)3 },
    RESOURCE_INT_LIST_END
};

#define PORT_TYPE_HELP "0: none, 1: joystick/CD32 pad (lowlevel.library), 2: analog paddles (joystick port only)"

static const cmdline_option_t cmdline_options[] = {
    { "-amigajoyport0", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "AmigaJoyPort0", NULL,
      "<type>", "Amiga mouse port (0) controller type. " PORT_TYPE_HELP },
    { "-amigajoyport1", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "AmigaJoyPort1", NULL,
      "<type>", "Amiga joystick port (1) controller type. " PORT_TYPE_HELP },
    { "-amigajoyport2", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "AmigaJoyPort2", NULL,
      "<type>", "lowlevel.library port 2 controller type (0 or 1)" },
    { "-amigajoyport3", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "AmigaJoyPort3", NULL,
      "<type>", "lowlevel.library port 3 controller type (0 or 1)" },
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

static void amiga_joy_poll_lowlevel(joystick_device_t *joydev, amiga_joy_priv_t *priv)
{
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

    for (i = 0; i < LL_NUM_BUTTONS && i < (unsigned int)joydev->num_buttons; i++) {
        joy_button_event(joydev->buttons[i], (state & ll_buttons[i].mask) ? 1 : 0);
    }
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

    if (priv->type == AMIGA_JOY_TYPE_LOWLEVEL) {
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
    lib_free(priv);
}

/* called by the core after its default mapping: paddles are not directions */
static void amiga_joy_customize(joystick_device_t *joydev)
{
    amiga_joy_priv_t *priv = joydev->priv;
    int i;

    if (priv->type != AMIGA_JOY_TYPE_PADDLES) {
        return;
    }
    for (i = 0; i < joydev->num_axes && i < 2; i++) {
        joystick_axis_t *axis = joydev->axes[i];

        axis->mapping.negative.action = JOY_ACTION_NONE;
        axis->mapping.positive.action = JOY_ACTION_NONE;
        axis->mapping.pot = (unsigned int)(i + 1);
    }
    /* C64 paddle fire buttons are the joystick left/right lines */
    joydev->buttons[0]->mapping.action = JOY_ACTION_JOYSTICK;
    joydev->buttons[0]->mapping.value.joy_pin = JOYSTICK_DIRECTION_LEFT;
    joydev->buttons[1]->mapping.action = JOY_ACTION_JOYSTICK;
    joydev->buttons[1]->mapping.value.joy_pin = JOYSTICK_DIRECTION_RIGHT;
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
    joydev->priv = priv;
    joydev->disable_sort = true;

    if (type == AMIGA_JOY_TYPE_LOWLEVEL) {
        joydev->name = lib_msprintf("%s joystick", port_name(port));
        joystick_device_add_hat(joydev, joystick_hat_new("Directions"));
        for (i = 0; i < LL_NUM_BUTTONS; i++) {
            joystick_button_t *button = joystick_button_new(ll_buttons[i].name);

            button->code = i;
            joystick_device_add_button(joydev, button);
        }
    } else {
        joystick_axis_t *axis;

        joydev->name = lib_msprintf("%s analog", port_name(port));
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

/* open lowlevel.library and the analog reader for the ports as configured,
 * and register a VICE joystick device for each used port */
static void amiga_joy_open_ports(void)
{
    ULONG paddle_flags = 0;
    ULONG retcode = PROPJOYRET_OK;
    int i, port;

    for (port = 0; port < AMIGA_JOY_PORTS; port++) {
        if (amiga_port_type[port] == AMIGA_JOY_TYPE_LOWLEVEL && LowLevelBase == NULL) {
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
            case AMIGA_JOY_TYPE_LOWLEVEL:
                if (LowLevelBase != NULL) {
                    lowlevel_ports_used |= 1 << port;
                    register_device(port, AMIGA_JOY_TYPE_LOWLEVEL);
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
           && ((amiga_joy_priv_t *)joystick_device_by_index(index)->priv)->type
              == AMIGA_JOY_TYPE_PADDLES;
}

void joystick_arch_shutdown(void)
{
    AMIGA_TRACE(("%s", __func__));
    amiga_joy_close_all();
}
