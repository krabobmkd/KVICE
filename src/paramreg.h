/** \file   paramreg.h
 * \brief   Register parameters for the functions called by the CPU emulation
 *
 * The memory handlers in the CPU read/write tables (read_func_t and
 * store_func_t, mem.h) are called for each emulated memory access. On
 * AmigaOS 68k, PARAMREG() gives their parameters a register instead of the
 * stack: address in d0, value in d1.
 *
 *   uint8_t ram_read(uint16_t addr PARAMREG(d0));
 *   void ram_store(uint16_t addr PARAMREG(d0), uint8_t value PARAMREG(d1));
 *
 * The CIA code (cia.h, ciacore.c and the cia_context callbacks) uses:
 * cia_context in a0, a CLOCK in d0 (the d0:d1 pair), a small parameter in
 * d0, or after a CLOCK: a1 for an int, d2 for a uint8_t or bool (address
 * registers can not hold bytes; d2 is saved by the callee when it changes
 * it).
 *
 * The prototype, the definition and the function type must all agree, gcc
 * does not check what is stored in a table: a handler without PARAMREG()
 * would silently read its parameters from the stack. Check build: compile
 * with -DPARAMREG_CHECK (syntax only), PARAMREG() then adds a dummy
 * parameter, each handler without it becomes an incompatible pointer
 * warning where it is put in a table.
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

#ifndef VICE_PARAMREG_H
#define VICE_PARAMREG_H

#if defined(PARAMREG_CHECK)
# define PARAMREG(r) , int paramreg_check_##r
#elif defined(__GNUC__) && defined(__m68k__) && defined(__AMIGA__)
# define PARAMREG(r) __asm(#r)
#else
# define PARAMREG(r)
#endif

#endif
