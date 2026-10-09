/** \file   amigabasic.h
 * \brief   AmigaOS 3.x port: BASIC programs as UTF-8 text (.bas)
 *
 * Plain C, no Amiga or VICE dependency (tested on the host too).
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

#ifndef VICE_AMIGABASIC_H
#define VICE_AMIGABASIC_H

#include <stdio.h>

/* BASIC dialects: the tokens known */
#define AMIGA_BASIC_V2  0   /* C64, VIC-20: tokens $80-$CB */
#define AMIGA_BASIC_V35 1   /* Plus/4, C16: tokens $80-$FD */

/* Unicode code point of a PETSCII character, upper case / graphics
 * character set (the one at power on), 0 for the control codes. */
unsigned long amiga_petscii_to_unicode(unsigned char c);

/* name of a PETSCII control code, as petcat writes it between braces
 * ("clr" for $93), NULL if it has none */
const char *amiga_petscii_control_name(unsigned char c);

/* PETSCII code of a Unicode character, the reverse of
 * amiga_petscii_to_unicode() (the code the keyboard gives when a character
 * has two), -1 if there is none. ASCII lower case gives upper case, and
 * petcat's ASCII stand-ins are accepted: ~ pi, \ pound, ^ up arrow,
 * _ left arrow. */
int amiga_unicode_to_petscii(unsigned long u);

/* PETSCII control code of a petcat name ("clr", any case), -1 if unknown */
int amiga_petscii_control_code(const char *name);

/* name of a BASIC token, NULL if the dialect has none */
const char *amiga_basic_token_name(unsigned char token, int dialect);

/* Write the tokenized program in mem[start..end) as UTF-8 text, one line
 * per BASIC line ("10 PRINT ..."), as LIST shows it: keywords spelled out
 * (not inside quotes, after REM nor in DATA), PETSCII graphics as their
 * Unicode characters, control codes as {name} or {$xx}.
 * Returns the number of lines written, -1 on a write error. */
int amiga_basic_write_utf8(const unsigned char *mem, unsigned int start, unsigned int end,
                           int dialect, FILE *out);

/* problems found reading a .bas text: errors (nothing is loaded) first,
 * then warnings (loaded anyway) */
enum {
    AMIGA_BAS_ERR_UTF8 = 0,     /* invalid UTF-8 sequence */
    AMIGA_BAS_ERR_CHAR,         /* value: code point without PETSCII */
    AMIGA_BAS_ERR_CONTROL,      /* detail: unknown {name} */
    AMIGA_BAS_ERR_BRACE,        /* { without } */
    AMIGA_BAS_ERR_NUMBER,       /* value: line number above 63999 */
    AMIGA_BAS_ERR_TOO_LONG,     /* value: tokenized line size */
    AMIGA_BAS_ERR_MEMORY,       /* value: program size, value2: room */
    AMIGA_BAS_ERR_NO_LINES,     /* no numbered line at all */
    AMIGA_BAS_WARN_DUPLICATE,   /* value: line number given again */
    AMIGA_BAS_WARN_EMPTY,       /* value: line number without text */
    AMIGA_BAS_KIND_COUNT
};

#define AMIGA_BAS_IS_ERROR(kind) ((kind) < AMIGA_BAS_WARN_DUPLICATE)

/* longest tokenized line accepted (the line format allows a bit more, the
 * screen editor much less) */
#define AMIGA_BASIC_LINE_MAX 250

typedef struct amiga_basic_issue_s {
    int kind;                   /* AMIGA_BAS_* */
    int file_line;              /* 1..., 0: the whole file */
    long basic_line;            /* -1: not known */
    unsigned long value;
    unsigned long value2;
    char detail[20];
} amiga_basic_issue_t;

#define AMIGA_BASIC_ISSUES_MAX 12

typedef struct amiga_basic_report_s {
    int errors;
    int warnings;
    int count;                  /* issues kept below, the first ones */
    amiga_basic_issue_t issues[AMIGA_BASIC_ISSUES_MAX];
    int lines;                  /* BASIC lines loaded */
    unsigned int end;           /* end of the program loaded (VARTAB) */
} amiga_basic_report_t;

/* Read a .bas UTF-8 text into memory at start, as typing its lines would
 * (CRUNCH: keywords tokenized, in the ROM table order, not inside quotes,
 * after REM nor in DATA). Lines that do not start with a number are
 * comments, empty lines are skipped, the lines are sorted by number.
 * mem is written only when there is no error, below limit.
 * Returns 0 when loaded, -1 if not; the report says why. */
int amiga_basic_read_utf8(const char *text, unsigned long len, int dialect,
                          unsigned char *mem, unsigned int start, unsigned int limit,
                          amiga_basic_report_t *report);

#endif
