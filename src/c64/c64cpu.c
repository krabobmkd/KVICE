/*
 * c64cpu.c - Emulation of the main 6510 processor used for x64
 *
 * Written by groepaz <groepaz@gmx.net>
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


#include "maincpu.h"
#include "mem.h"

#include "c64mem.h"
#include "cpmcart.h"

#ifdef FEATURE_CPUMEMHISTORY
#include "monitor.h"
#include "c64pla.h"
#endif

/* ------------------------------------------------------------------------- */

/* MACHINE_STUFF should define/undef

 - NEED_REG_PC
 - TRACE

 The following are optional:

 - PAGE_ZERO
 - PAGE_ONE
 - STORE_IND
 - LOAD_IND
 - DMA_FUNC
 - DMA_ON_RESET
 - CHECK_AND_RUN_ALTERNATE_CPU
 - ALTERNATE_CPU_ON_ATTENTION

*/

#ifdef FEATURE_CPUMEMHISTORY
/* FIXME do proper ROM/RAM/IO tests */

/* FIXME: the following functions should handle IO/RAM/ROM and -dummy accesses
 * for the memmap feature - see mainc64cpu.c */

inline static void memmap_mem_update(unsigned int addr, int write)
{
    unsigned int type = MEMMAP_RAM_R;

    if (write) {
        if ((addr >= 0xd000) && (addr <= 0xdfff)) {
            type = MEMMAP_I_O_W;
        } else {
            type = MEMMAP_RAM_W;
        }
    } else {
        switch (addr >> 12) {
            case 0xa:
            case 0xb:
            case 0xe:
            case 0xf:
                if (pport.data_read & (1 << ((addr >> 14) & 1))) {
                    type = MEMMAP_ROM_R;
                } else {
                    type = MEMMAP_RAM_R;
                }
                break;
            case 0xd:
                type = MEMMAP_I_O_R;
                break;
            default:
                type = MEMMAP_RAM_R;
                break;
        }
        if (memmap_state & MEMMAP_STATE_OPCODE) {
            /* HACK: transform R to X */
            type >>= 2;
            memmap_state &= ~(MEMMAP_STATE_OPCODE);
        } else if (memmap_state & MEMMAP_STATE_INSTR) {
            /* ignore operand reads */
            type = 0;
        }
    }
    monitor_memmap_store(addr, type);
}

static void memmap_mem_store(unsigned int addr, unsigned int value)
{
    memmap_mem_update(addr, 1);
    (*_mem_write_tab_ptr[(addr) >> 8])((uint16_t)(addr), (uint8_t)(value));
}

static uint8_t memmap_mem_read(unsigned int addr)
{
    memmap_mem_update(addr, 0);
    return (*_mem_read_tab_ptr[(addr) >> 8])((uint16_t)(addr));
}

static void memmap_mark_read(unsigned int addr)
{
    memmap_mem_update(addr, 0);
}

static void memmap_mem_store_dummy(unsigned int addr, unsigned int value)
{
    memmap_mem_update(addr, 1);
    (*_mem_write_tab_ptr_dummy[(addr) >> 8])((uint16_t)(addr), (uint8_t)(value));
}

static uint8_t memmap_mem_read_dummy(unsigned int addr)
{
    memmap_mem_update(addr, 0);
    return (*_mem_read_tab_ptr_dummy[(addr) >> 8])((uint16_t)(addr));
}
#else

/* Plain RAM/ROM pages are read and written directly, the others call their
   handler (I/O, CPU port, VIC-II bank writes, cartridges...), see
   mem_read_direct_get() in c64mem.c. Functions, not macros: the address is
   evaluated once. */
static inline uint8_t c64cpu_load(unsigned int addr)
{
    uint8_t *p = _mem_read_direct_ptr[addr >> 8];

    if (p != NULL) {
        return p[addr];
    }
    return (*_mem_read_tab_ptr[addr >> 8])((uint16_t)addr);
}

static inline void c64cpu_store(unsigned int addr, uint8_t value)
{
    uint8_t *p = _mem_write_direct_ptr[addr >> 8];

    if (p != NULL) {
        p[addr] = value;
    } else {
        (*_mem_write_tab_ptr[addr >> 8])((uint16_t)addr, value);
    }
}

#define LOAD(addr) c64cpu_load((unsigned int)(addr))
#define STORE(addr, value) c64cpu_store((unsigned int)(addr), (uint8_t)(value))

/* zero page: $02-$ff directly in RAM when possible, $00/$01 is the CPU
   port (c64mem.c, mem_update_zero_direct_ptrs()) */
static inline uint8_t c64cpu_load_zero(unsigned int addr)
{
    unsigned int a = addr & 0xff;

    if (a >= 2 && _mem_zero_read_direct != NULL) {
        return _mem_zero_read_direct[a];
    }
    return (*_mem_read_tab_ptr[0])((uint16_t)addr);
}

static inline void c64cpu_store_zero(unsigned int addr, uint8_t value)
{
    unsigned int a = addr & 0xff;

    if (a >= 2 && _mem_zero_write_direct != NULL) {
        _mem_zero_write_direct[a] = value;
    } else {
        (*_mem_write_tab_ptr[0])((uint16_t)addr, value);
    }
}

#define LOAD_ZERO(addr) c64cpu_load_zero((unsigned int)(addr))
#define STORE_ZERO(addr, value) c64cpu_store_zero((unsigned int)(addr), (uint8_t)(value))

#endif

/* the CP/M cartridge Z80 runs from the maincpu_attention epilogue, after an
   opcode, only while it is started: no call before each opcode */
#define ALTERNATE_CPU_ON_ATTENTION cpmcart_check_and_run_z80();

#define HAVE_Z80_REGS

/* the N and Z flags in one variable: one store per instruction less */
#define CPU_FLAG_NZ_MERGED

#include "../maincpu.c"
