/** \file   maincpuattention.h
 * \brief   Main CPU "attention" flags: the rare per opcode work
 *
 * The main CPU loop checks some rarely set flags after each opcode: the
 * profiler, autostart, the CP/M cartridge Z80, the cycle limit. Each flag is
 * one byte of a single 32 bit word, so the loop does one test of the word
 * (any) instead of one test or function call per flag. The flags are
 * written as plain variables through the macros below (profiler.h,
 * autostart.c, cpmcart.c), any value fits in a byte.
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

#ifndef VICE_MAINCPUATTENTION_H
#define VICE_MAINCPUATTENTION_H

#include <stdint.h>

typedef union maincpu_attention_u {
    uint32_t any;           /* != 0 when at least one flag is set */
    struct {
        uint8_t profiling;  /* monitor profiler running */
        uint8_t autostart;  /* autostart enabled, autostart_advance() */
        uint8_t z80;        /* CP/M cartridge Z80 running */
        uint8_t clk_limit;  /* maincpu_clk_limit != 0 */
    } flags;
} maincpu_attention_t;

/* defined by each main CPU (maincpu.c, mainc64cpu.c...) */
extern maincpu_attention_t maincpu_attention;

#endif
