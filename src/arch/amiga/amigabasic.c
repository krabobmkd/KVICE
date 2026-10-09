/** \file   amigabasic.c
 * \brief   AmigaOS 3.x port: BASIC programs as UTF-8 text (.bas)
 *
 * The PETSCII graphics are mapped like Unicode does it for the Commodore
 * 64 (Symbols for Legacy Computing, U+1FB00-U+1FBFF, Unicode 13, and the
 * older box drawing and block elements), upper case / graphics set.
 * Control codes use the petcat names, so petcat can read the text back.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "amigabasic.h"

/* PETSCII $60-$7F (same characters as $C0-$DF) */
static const unsigned long petscii_60[0x20] = {
    0x2500,  0x2660,  0x1FB72, 0x1FB78, 0x1FB77, 0x1FB76, 0x1FB7A, 0x1FB71,
    0x1FB74, 0x256E,  0x2570,  0x256F,  0x1FB7C, 0x2572,  0x2571,  0x1FB7D,
    0x1FB7E, 0x25CF,  0x1FB7B, 0x2665,  0x1FB70, 0x256D,  0x2573,  0x25CB,
    0x2663,  0x1FB75, 0x2666,  0x253C,  0x1FB8C, 0x2502,  0x03C0,  0x25E5
};

/* PETSCII $A0-$BF (same characters as $E0-$FF, $FF is pi) */
static const unsigned long petscii_a0[0x20] = {
    0x00A0,  0x258C,  0x2584,  0x2594,  0x2581,  0x258F,  0x2592,  0x2595,
    0x1FB8F, 0x25E4,  0x1FB87, 0x251C,  0x2597,  0x2514,  0x2510,  0x2582,
    0x250C,  0x2534,  0x252C,  0x2524,  0x258E,  0x258D,  0x1FB88, 0x1FB82,
    0x1FB83, 0x2583,  0x1FB7F, 0x2596,  0x259D,  0x2518,  0x2598,  0x259A
};

/* control codes $00-$1F and $80-$9F, petcat names */
static const char *const control_00[0x20] = {
    NULL,   NULL,   NULL,   "stop", NULL,   "wht",  NULL,   NULL,
    "dish", "ensh", NULL,   NULL,   NULL,   "return", "swlc", NULL,
    NULL,   "down", "rvon", "home", "del",  NULL,   NULL,   NULL,
    NULL,   NULL,   NULL,   "esc",  "red",  "rght", "grn",  "blu"
};

static const char *const control_80[0x20] = {
    NULL,   "orng", NULL,   NULL,   NULL,   "f1",   "f3",   "f5",
    "f7",   "f2",   "f4",   "f6",   "f8",   "sret", "swuc", NULL,
    "blk",  "up",   "rvof", "clr",  "inst", "brn",  "lred", "gry1",
    "gry2", "lgrn", "lblu", "gry3", "pur",  "left", "yel",  "cyn"
};

/* BASIC V2 tokens $80-$CB */
static const char *const tokens_v2[0x4c] = {
    "END",    "FOR",    "NEXT",   "DATA",   "INPUT#", "INPUT",  "DIM",    "READ",
    "LET",    "GOTO",   "RUN",    "IF",     "RESTORE", "GOSUB", "RETURN", "REM",
    "STOP",   "ON",     "WAIT",   "LOAD",   "SAVE",   "VERIFY", "DEF",    "POKE",
    "PRINT#", "PRINT",  "CONT",   "LIST",   "CLR",    "CMD",    "SYS",    "OPEN",
    "CLOSE",  "GET",    "NEW",    "TAB(",   "TO",     "FN",     "SPC(",   "THEN",
    "NOT",    "STEP",   "+",      "-",      "*",      "/",      "^",      "AND",
    "OR",     ">",      "=",      "<",      "SGN",    "INT",    "ABS",    "USR",
    "FRE",    "POS",    "SQR",    "RND",    "LOG",    "EXP",    "COS",    "SIN",
    "TAN",    "ATN",    "PEEK",   "LEN",    "STR$",   "VAL",    "ASC",    "CHR$",
    "LEFT$",  "RIGHT$", "MID$",   "GO"
};

/* BASIC 3.5 tokens $CC-$FD (Plus/4, C16) */
static const char *const tokens_v35[0x32] = {
    "RGR",      "RCLR",    "RLUM",    "JOY",     "RDOT",   "DEC",    "HEX$",      "ERR$",
    "INSTR",    "ELSE",    "RESUME",  "TRAP",    "TRON",   "TROFF",  "SOUND",     "VOL",
    "AUTO",     "PUDEF",   "GRAPHIC", "PAINT",   "CHAR",   "BOX",    "CIRCLE",    "GSHAPE",
    "SSHAPE",   "DRAW",    "LOCATE",  "COLOR",   "SCNCLR", "SCALE",  "HELP",      "DO",
    "LOOP",     "EXIT",    "DIRECTORY", "DSAVE", "DLOAD",  "HEADER", "SCRATCH",   "COLLECT",
    "COPY",     "RENAME",  "BACKUP",  "DELETE",  "RENUMBER", "KEY",  "MONITOR",   "USING",
    "UNTIL",    "WHILE"
};

#define TOKEN_DATA 0x83
#define TOKEN_REM  0x8f
#define TOKEN_PI   0xff

unsigned long amiga_petscii_to_unicode(unsigned char c)
{
    if (c < 0x20 || (c >= 0x80 && c < 0xa0)) {
        return 0;
    }
    if (c < 0x5b) {
        /* space, digits, punctuation, @, A-Z: ASCII */
        return c;
    }
    switch (c) {
        case 0x5b: return '[';
        case 0x5c: return 0x00A3;   /* pound */
        case 0x5d: return ']';
        case 0x5e: return 0x2191;   /* up arrow */
        case 0x5f: return 0x2190;   /* left arrow */
        case 0xff: return 0x03C0;   /* pi */
        default: break;
    }
    if (c < 0x80) {
        return petscii_60[c - 0x60];
    }
    if (c < 0xc0) {
        return petscii_a0[c - 0xa0];
    }
    if (c < 0xe0) {
        return petscii_60[c - 0xc0];
    }
    return petscii_a0[c - 0xe0];
}

const char *amiga_petscii_control_name(unsigned char c)
{
    if (c < 0x20) {
        return control_00[c];
    }
    if (c >= 0x80 && c < 0xa0) {
        return control_80[c - 0x80];
    }
    return NULL;
}

const char *amiga_basic_token_name(unsigned char token, int dialect)
{
    if (token >= 0x80 && token < 0xcc) {
        return tokens_v2[token - 0x80];
    }
    if (dialect == AMIGA_BASIC_V35 && token >= 0xcc && token < 0xfe) {
        return tokens_v35[token - 0xcc];
    }
    return NULL;
}

int amiga_unicode_to_petscii(unsigned long u)
{
    /* the codes the keyboard gives first: C= graphics, SHIFT graphics, pi */
    static const unsigned char ranges[][2] = {
        { 0xa0, 0xbf }, { 0xc0, 0xdf }, { 0xff, 0xff }, { 0x60, 0x7f }, { 0xe0, 0xfe }
    };
    unsigned int r, c;

    if (u >= 'a' && u <= 'z') {
        return (int)(u - 'a' + 'A');
    }
    if (u < 0x80) {
        switch (u) {
            case '~':  return 0xff;     /* pi */
            case '\\': return 0x5c;     /* pound */
            case '^':  return 0x5e;     /* up arrow */
            case '_':  return 0x5f;     /* left arrow */
            default: break;
        }
        if (u >= 0x20 && u <= 0x5d && u != '\\') {
            return (int)u;
        }
        return -1;
    }
    switch (u) {
        case 0x00A3: return 0x5c;
        case 0x2191: return 0x5e;
        case 0x2190: return 0x5f;
        case 0x03C0: return 0xff;
        default: break;
    }
    for (r = 0; r < sizeof ranges / sizeof ranges[0]; r++) {
        for (c = ranges[r][0]; c <= ranges[r][1]; c++) {
            if (amiga_petscii_to_unicode((unsigned char)c) == u) {
                return (int)c;
            }
        }
    }
    return -1;
}

static int name_equal(const char *a, const char *b)
{
    for (; *a != '\0' && *b != '\0'; a++, b++) {
        int ca = (*a >= 'A' && *a <= 'Z') ? *a - 'A' + 'a' : *a;
        int cb = (*b >= 'A' && *b <= 'Z') ? *b - 'A' + 'a' : *b;

        if (ca != cb) {
            return 0;
        }
    }
    return *a == *b;
}

int amiga_petscii_control_code(const char *name)
{
    int i;

    for (i = 0; i < 0x20; i++) {
        if (control_00[i] != NULL && name_equal(control_00[i], name)) {
            return i;
        }
        if (control_80[i] != NULL && name_equal(control_80[i], name)) {
            return 0x80 + i;
        }
    }
    return -1;
}

/* one code point as UTF-8 */
static int put_utf8(unsigned long u, FILE *out)
{
    unsigned char b[4];
    size_t n;

    if (u < 0x80) {
        b[0] = (unsigned char)u;
        n = 1;
    } else if (u < 0x800) {
        b[0] = (unsigned char)(0xc0 | (u >> 6));
        b[1] = (unsigned char)(0x80 | (u & 0x3f));
        n = 2;
    } else if (u < 0x10000) {
        b[0] = (unsigned char)(0xe0 | (u >> 12));
        b[1] = (unsigned char)(0x80 | ((u >> 6) & 0x3f));
        b[2] = (unsigned char)(0x80 | (u & 0x3f));
        n = 3;
    } else {
        b[0] = (unsigned char)(0xf0 | (u >> 18));
        b[1] = (unsigned char)(0x80 | ((u >> 12) & 0x3f));
        b[2] = (unsigned char)(0x80 | ((u >> 6) & 0x3f));
        b[3] = (unsigned char)(0x80 | (u & 0x3f));
        n = 4;
    }
    return fwrite(b, 1, n, out) == n ? 0 : -1;
}

/* a character of the program text: graphics as Unicode, control codes
 * as {name} or {$xx} */
static int put_petscii(unsigned char c, FILE *out)
{
    unsigned long u = amiga_petscii_to_unicode(c);
    const char *name;

    if (u != 0) {
        return put_utf8(u, out);
    }
    name = amiga_petscii_control_name(c);
    if (name != NULL) {
        return fprintf(out, "{%s}", name) < 0 ? -1 : 0;
    }
    return fprintf(out, "{$%02x}", (unsigned int)c) < 0 ? -1 : 0;
}

int amiga_basic_write_utf8(const unsigned char *mem, unsigned int start, unsigned int end,
                           int dialect, FILE *out)
{
    unsigned int line = start;
    int lines = 0;

    /* each line: link to the next one, line number, text, 0 */
    while (line + 4 <= end) {
        unsigned int next = mem[line] | (mem[line + 1] << 8);
        unsigned int number = mem[line + 2] | (mem[line + 3] << 8);
        unsigned int p = line + 4;
        int quote = 0;      /* inside "..." */
        int rem = 0;        /* after REM: text up to the end of the line */
        int data = 0;       /* after DATA: text up to ':' out of quotes */

        /* end link, or a link that does not go forward (damaged program) */
        if (next == 0 || next <= line || next > end) {
            break;
        }
        if (fprintf(out, "%u ", number) < 0) {
            return -1;
        }
        for (; p < next && mem[p] != 0; p++) {
            unsigned char c = mem[p];
            int r;

            if (c == '"') {
                quote = !quote;
                r = put_utf8('"', out);
            } else if (quote || rem || (data && c != ':')) {
                r = put_petscii(c, out);
            } else if (c >= 0x80) {
                const char *keyword = amiga_basic_token_name(c, dialect);

                if (keyword != NULL) {
                    r = fputs(keyword, out) < 0 ? -1 : 0;
                    rem = (c == TOKEN_REM);
                    data = (c == TOKEN_DATA);
                } else if (c == TOKEN_PI) {
                    r = put_utf8(0x03C0, out);
                } else {
                    /* unknown token (BASIC extension): kept as a byte */
                    r = fprintf(out, "{$%02x}", (unsigned int)c) < 0 ? -1 : 0;
                }
            } else {
                if (c == ':') {
                    data = 0;
                }
                r = put_petscii(c, out);
            }
            if (r < 0) {
                return -1;
            }
        }
        if (fputc('\n', out) == EOF) {
            return -1;
        }
        lines++;
        line = next;
    }
    return lines;
}

/* ------------------------------------------------------------------------- */
/* .bas text to memory */

static void add_issue(amiga_basic_report_t *report, int kind, int file_line,
                      long basic_line, unsigned long value, unsigned long value2,
                      const char *detail)
{
    amiga_basic_issue_t *issue;

    if (AMIGA_BAS_IS_ERROR(kind)) {
        report->errors++;
    } else {
        report->warnings++;
    }
    if (report->count >= AMIGA_BASIC_ISSUES_MAX) {
        return;
    }
    issue = &report->issues[report->count++];
    issue->kind = kind;
    issue->file_line = file_line;
    issue->basic_line = basic_line;
    issue->value = value;
    issue->value2 = value2;
    issue->detail[0] = '\0';
    if (detail != NULL) {
        strncpy(issue->detail, detail, sizeof issue->detail - 1);
        issue->detail[sizeof issue->detail - 1] = '\0';
    }
}

/* next code point of p[0..n), its byte count in *size; -1 if invalid */
static long utf8_next(const unsigned char *p, unsigned long n, unsigned int *size)
{
    unsigned long u;
    unsigned int len, i;

    if (p[0] < 0x80) {
        *size = 1;
        return p[0];
    }
    if ((p[0] & 0xe0) == 0xc0) {
        len = 2;
        u = p[0] & 0x1f;
    } else if ((p[0] & 0xf0) == 0xe0) {
        len = 3;
        u = p[0] & 0x0f;
    } else if ((p[0] & 0xf8) == 0xf0) {
        len = 4;
        u = p[0] & 0x07;
    } else {
        *size = 1;
        return -1;
    }
    if (n < len) {
        *size = (unsigned int)n;
        return -1;
    }
    for (i = 1; i < len; i++) {
        if ((p[i] & 0xc0) != 0x80) {
            *size = i;
            return -1;
        }
        u = (u << 6) | (p[i] & 0x3f);
    }
    *size = len;
    return (long)u;
}

/* the PETSCII text of a line (after its number) */
#define PETSCII_LINE_MAX 1024

/* UTF-8 text -> PETSCII, {name} and {$xx} resolved. Returns the PETSCII
 * length, -1 on errors (reported). */
static int decode_line(const unsigned char *p, unsigned long n, unsigned char *out,
                       int file_line, long basic_line, amiga_basic_report_t *report)
{
    int len = 0;
    int bad = 0;
    unsigned long i = 0;

    while (i < n) {
        unsigned int size;
        long u = utf8_next(p + i, n - i, &size);
        int c;

        if (u < 0) {
            add_issue(report, AMIGA_BAS_ERR_UTF8, file_line, basic_line, 0, 0, NULL);
            return -1;
        }
        i += size;
        if (u == '{') {
            char name[20];
            unsigned int k = 0;

            while (i < n && p[i] != '}' && k < sizeof name - 1) {
                name[k++] = (char)p[i++];
            }
            name[k] = '\0';
            if (i >= n || p[i] != '}') {
                add_issue(report, AMIGA_BAS_ERR_BRACE, file_line, basic_line, 0, 0, name);
                return -1;
            }
            i++;
            if (name[0] == '$' && k == 3 && strspn(name + 1, "0123456789abcdefABCDEF") == 2) {
                c = (int)strtol(name + 1, NULL, 16);
            } else {
                c = amiga_petscii_control_code(name);
            }
            if (c < 0) {
                add_issue(report, AMIGA_BAS_ERR_CONTROL, file_line, basic_line, 0, 0, name);
                bad = 1;
                continue;
            }
        } else if (u == '\t') {
            c = ' ';
        } else {
            c = amiga_unicode_to_petscii((unsigned long)u);
            if (c < 0) {
                add_issue(report, AMIGA_BAS_ERR_CHAR, file_line, basic_line,
                          (unsigned long)u, 0, NULL);
                bad = 1;
                continue;
            }
        }
        if (len >= PETSCII_LINE_MAX) {
            add_issue(report, AMIGA_BAS_ERR_TOO_LONG, file_line, basic_line,
                      (unsigned long)len, 0, NULL);
            return -1;
        }
        out[len++] = (unsigned char)c;
    }
    return bad ? -1 : len;
}

/* the ROM CRUNCH routine: PETSCII line -> tokenized line, returns its size */
static int crunch_line(const unsigned char *in, int n, int dialect, unsigned char *out)
{
    int last = (dialect == AMIGA_BASIC_V35) ? 0xfd : 0xcb;
    int quote = 0, rem = 0, data = 0;
    int i = 0, len = 0;

    while (i < n) {
        unsigned char c = in[i];
        int t, found = 0;

        if (rem) {
            out[len++] = c;
            i++;
            continue;
        }
        if (c == '"') {
            quote = !quote;
            out[len++] = c;
            i++;
            continue;
        }
        if (quote || c >= 0x80) {
            out[len++] = c;
            i++;
            continue;
        }
        if (data) {
            if (c == ':') {
                data = 0;
            }
            out[len++] = c;
            i++;
            continue;
        }
        if (c == '?') {
            out[len++] = 0x99;      /* PRINT */
            i++;
            continue;
        }
        if (c < 0x30 || c > 0x3b) {
            /* the first keyword of the table that matches, as the ROM does */
            for (t = 0x80; t <= last; t++) {
                const char *k = amiga_basic_token_name((unsigned char)t, dialect);
                int kl = (int)strlen(k);

                if (kl <= n - i && memcmp(k, in + i, (size_t)kl) == 0) {
                    out[len++] = (unsigned char)t;
                    i += kl;
                    rem = (t == TOKEN_REM);
                    data = (t == TOKEN_DATA);
                    found = 1;
                    break;
                }
            }
        }
        if (!found) {
            out[len++] = c;
            i++;
        }
    }
    return len;
}

typedef struct basic_line_s {
    long number;
    unsigned long offset;       /* in the tokenized bytes buffer */
    int size;
    int file_line;
} basic_line_t;

static int line_compare(const void *a, const void *b)
{
    const basic_line_t *la = (const basic_line_t *)a;
    const basic_line_t *lb = (const basic_line_t *)b;

    if (la->number != lb->number) {
        return la->number < lb->number ? -1 : 1;
    }
    /* same number: file order, the last one is kept */
    return la->file_line < lb->file_line ? -1 : 1;
}

int amiga_basic_read_utf8(const char *text, unsigned long len, int dialect,
                          unsigned char *mem, unsigned int start, unsigned int limit,
                          amiga_basic_report_t *report)
{
    const unsigned char *t = (const unsigned char *)text;
    unsigned char petscii[PETSCII_LINE_MAX];
    unsigned char tokens[PETSCII_LINE_MAX * 2];
    basic_line_t *lines = NULL;
    unsigned char *bytes = NULL;
    unsigned long bytes_size = 0, bytes_max = 0;
    int count = 0, count_max = 0;
    unsigned long pos = 0, size;
    unsigned int addr;
    int file_line = 0;
    int i, kept;

    memset(report, 0, sizeof *report);
    /* UTF-8 byte order mark */
    if (len >= 3 && t[0] == 0xef && t[1] == 0xbb && t[2] == 0xbf) {
        pos = 3;
    }
    while (pos < len) {
        unsigned long eol = pos, end;
        long number = 0;
        int digits = 0, n, tl;

        while (eol < len && t[eol] != '\n') {
            eol++;
        }
        file_line++;
        end = eol;
        if (end > pos && t[end - 1] == '\r') {
            end--;
        }
        while (pos < end && (t[pos] == ' ' || t[pos] == '\t')) {
            pos++;
        }
        /* empty lines and lines without a number: comments */
        while (pos < end && t[pos] >= '0' && t[pos] <= '9') {
            if (number <= 999999L) {
                number = number * 10 + (t[pos] - '0');
            }
            digits++;
            pos++;
        }
        if (digits == 0) {
            pos = eol + 1;
            continue;
        }
        /* the spaces after the number are not stored, like when typing */
        while (pos < end && (t[pos] == ' ' || t[pos] == '\t')) {
            pos++;
        }
        if (number > 63999L) {
            add_issue(report, AMIGA_BAS_ERR_NUMBER, file_line, -1, (unsigned long)number, 0, NULL);
            pos = eol + 1;
            continue;
        }
        n = decode_line(t + pos, end - pos, petscii, file_line, number, report);
        pos = eol + 1;
        if (n < 0) {
            continue;
        }
        if (n == 0) {
            add_issue(report, AMIGA_BAS_WARN_EMPTY, file_line, number, (unsigned long)number, 0, NULL);
            continue;
        }
        tl = crunch_line(petscii, n, dialect, tokens);
        if (tl > AMIGA_BASIC_LINE_MAX) {
            add_issue(report, AMIGA_BAS_ERR_TOO_LONG, file_line, number, (unsigned long)tl, 0, NULL);
            continue;
        }
        /* keep it */
        if (count == count_max) {
            basic_line_t *more;

            count_max = count_max ? count_max * 2 : 256;
            more = realloc(lines, (size_t)count_max * sizeof *lines);
            if (more == NULL) {
                free(lines);
                free(bytes);
                add_issue(report, AMIGA_BAS_ERR_MEMORY, 0, -1, 0, 0, NULL);
                return -1;
            }
            lines = more;
        }
        if (bytes_size + (unsigned long)tl > bytes_max) {
            unsigned char *more;

            bytes_max = bytes_max ? bytes_max * 2 : 16384;
            more = realloc(bytes, bytes_max);
            if (more == NULL) {
                free(lines);
                free(bytes);
                add_issue(report, AMIGA_BAS_ERR_MEMORY, 0, -1, 0, 0, NULL);
                return -1;
            }
            bytes = more;
        }
        memcpy(bytes + bytes_size, tokens, (size_t)tl);
        lines[count].number = number;
        lines[count].offset = bytes_size;
        lines[count].size = tl;
        lines[count].file_line = file_line;
        bytes_size += (unsigned long)tl;
        count++;
    }

    if (count == 0 && report->errors == 0) {
        add_issue(report, AMIGA_BAS_ERR_NO_LINES, 0, -1, 0, 0, NULL);
    }
    if (count > 1) {
        qsort(lines, (size_t)count, sizeof *lines, line_compare);
    }
    /* the same number again: the last one replaces it, as when typing */
    kept = 0;
    size = 2;
    for (i = 0; i < count; i++) {
        if (i + 1 < count && lines[i + 1].number == lines[i].number) {
            add_issue(report, AMIGA_BAS_WARN_DUPLICATE, lines[i + 1].file_line,
                      lines[i].number, (unsigned long)lines[i].number, 0, NULL);
            continue;
        }
        lines[kept++] = lines[i];
        size += 4 + (unsigned long)lines[i].size + 1;
    }
    if (report->errors == 0 && (unsigned long)start + size > limit) {
        add_issue(report, AMIGA_BAS_ERR_MEMORY, 0, -1, size,
                  limit > start ? (unsigned long)(limit - start) : 0, NULL);
    }
    if (report->errors > 0) {
        free(lines);
        free(bytes);
        return -1;
    }

    /* link, number, text, 0 for each line, then a 0 link */
    addr = start;
    for (i = 0; i < kept; i++) {
        unsigned int next = addr + 4 + (unsigned int)lines[i].size + 1;

        mem[addr] = (unsigned char)(next & 0xff);
        mem[addr + 1] = (unsigned char)(next >> 8);
        mem[addr + 2] = (unsigned char)(lines[i].number & 0xff);
        mem[addr + 3] = (unsigned char)(lines[i].number >> 8);
        memcpy(mem + addr + 4, bytes + lines[i].offset, (size_t)lines[i].size);
        mem[next - 1] = 0;
        addr = next;
    }
    mem[addr] = 0;
    mem[addr + 1] = 0;
    report->lines = kept;
    report->end = addr + 2;
    free(lines);
    free(bytes);
    return 0;
}
