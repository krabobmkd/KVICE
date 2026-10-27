/** \file   maincpuclk.h
 * \brief   Fast maincpu_clk additions for the main CPU loop
 *
 * maincpu_clk is a 64 bit global. On the 68k gcc adds to it with add.l on
 * the low word, then loads, addx and stores the high word (addx has no
 * immediate and no absolute memory form): 6 instructions and 4 memory
 * accesses for each cycle count update of the CPU core. The carry into the
 * high word only happens once every 73 minutes of emulated time, so the
 * 68k version adds to the low word and skips the high word with a branch
 * that is almost never taken: 2 instructions, 2 memory accesses.
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

#ifndef VICE_MAINCPUCLK_H
#define VICE_MAINCPUCLK_H

#include "types.h"

extern CLOCK maincpu_clk;

#if defined(AMIGA_M68K) && defined(__GNUC__)

/* maincpu_clk += n. The "+m" operand tells gcc that maincpu_clk changes,
   the instructions name it directly (big endian: low word at +4). gas
   turns add.l #1..8 into addq. n must not be negative: the main CPU core
   never subtracts with CLK_ADD (only the drive CPU does, with its own
   clock). */
static inline __attribute__((always_inline)) void maincpu_clk_add(unsigned int n)
{
    if (__builtin_constant_p(n) && n == 0) {
        return;
    }
    __asm__ volatile ("add.l %1,_maincpu_clk+4\n\t"
                      "bcc.s 1f\n\t"
                      "addq.l #1,_maincpu_clk\n"
                      "1:"
                      : "+m" (maincpu_clk)
                      : "dn" (n)
                      : "cc");
}

#else

static inline void maincpu_clk_add(unsigned int n)
{
    maincpu_clk += n;
}

#endif

#endif
