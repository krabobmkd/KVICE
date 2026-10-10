/** \file   rilist.c
 * \brief   KVICE Resource Installer: resourcelist.txt and the files on disk
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rilist.h"
#include "risha256.h"

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

int ri_parse_sha256(const char *hex, unsigned char out[32])
{
    int i;

    for (i = 0; i < 32; i++) {
        int hi = hex_value(hex[i * 2]);
        int lo = (hi < 0) ? -1 : hex_value(hex[i * 2 + 1]);

        if (lo < 0) {
            return 0;
        }
        out[i] = (unsigned char)(hi * 16 + lo);
    }
    return hex[64] == '\0' || hex[64] == ' ' || hex[64] == '\t';
}

void ri_list_free(ri_resource_t *list, int count)
{
    int i, u;

    if (list == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        for (u = 0; u < list[i].url_count; u++) {
            free(list[i].urls[u]);
        }
    }
    free(list);
}

/* the next word of a line, NUL terminated in place */
static char *next_word(char **p)
{
    char *s = *p, *w;

    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (*s == '\0') {
        *p = s;
        return NULL;
    }
    w = s;
    while (*s != '\0' && *s != ' ' && *s != '\t') {
        s++;
    }
    if (*s != '\0') {
        *s++ = '\0';
    }
    *p = s;
    return w;
}

/* a new string: a + b */
static char *str_concat(const char *a, const char *b)
{
    char *s = malloc(strlen(a) + strlen(b) + 1);

    if (s != NULL) {
        strcpy(s, a);
        strcat(s, b);
    }
    return s;
}

int ri_list_parse(const char *text, unsigned long len, ri_resource_t **out, int *bad_line)
{
    ri_resource_t *list = NULL;
    char *bases[RI_URLS_MAX];
    int base_count = 0, b;
    int count = 0, max = 0, line_no = 0;
    unsigned long pos = 0;
    char line[1024];

    *out = NULL;
    *bad_line = 0;
    while (pos < len) {
        unsigned long eol = pos, n;
        char *p, *path, *hash, *url;
        ri_resource_t *r;

        while (eol < len && text[eol] != '\n') {
            eol++;
        }
        line_no++;
        n = eol - pos;
        if (n > 0 && text[pos + n - 1] == '\r') {
            n--;
        }
        if (n >= sizeof line) {
            n = sizeof line - 1;
        }
        memcpy(line, text + pos, n);
        line[n] = '\0';
        pos = eol + 1;

        p = line;
        path = next_word(&p);
        if (path == NULL || path[0] == '#') {
            continue;
        }
        hash = next_word(&p);
        if (strcmp(path, "base") == 0) {
            /* server base: the resources below are at base + path */
            if (hash != NULL && base_count < RI_URLS_MAX) {
                bases[base_count] = str_concat(hash, "");
                if (bases[base_count] == NULL) {
                    goto fail;
                }
                base_count++;
            }
            continue;
        }
        if (count == max) {
            ri_resource_t *more;

            max = max ? max * 2 : 32;
            more = realloc(list, (size_t)max * sizeof *list);
            if (more == NULL) {
                ri_list_free(list, count);
                return -1;
            }
            list = more;
        }
        r = &list[count];
        memset(r, 0, sizeof *r);
        if (hash == NULL || strlen(path) >= sizeof r->path || !ri_parse_sha256(hash, r->sha256)) {
            *bad_line = line_no;
            goto fail;
        }
        strcpy(r->path, path);
        count++;
        /* each server base, then the URLs given on the line */
        for (b = 0; b < base_count && r->url_count < RI_URLS_MAX; b++) {
            if ((r->urls[r->url_count] = str_concat(bases[b], path)) == NULL) {
                goto fail;
            }
            r->url_count++;
        }
        while ((url = next_word(&p)) != NULL && r->url_count < RI_URLS_MAX) {
            if ((r->urls[r->url_count] = str_concat(url, "")) == NULL) {
                goto fail;
            }
            r->url_count++;
        }
    }
    for (b = 0; b < base_count; b++) {
        free(bases[b]);
    }
    *out = list;
    return count;

fail:
    for (b = 0; b < base_count; b++) {
        free(bases[b]);
    }
    ri_list_free(list, count);
    return -1;
}

void ri_join_path(const char *drawer, const char *path, char *out, unsigned int size)
{
    size_t n = strlen(drawer);

    if (n == 0 || drawer[n - 1] == ':' || drawer[n - 1] == '/') {
        snprintf(out, size, "%s%s", drawer, path);
    } else {
        snprintf(out, size, "%s/%s", drawer, path);
    }
}

void ri_check_resource(ri_resource_t *r, const char *drawer)
{
    char full[RI_PATH_MAX];
    unsigned char buf[4096];
    unsigned char digest[32];
    ri_sha256_t c;
    FILE *f;
    size_t n;

    ri_join_path(drawer, r->path, full, sizeof full);
    f = fopen(full, "rb");
    if (f == NULL) {
        r->state = RI_STATE_MISSING;
        r->check = RI_CHECK_NONE;
        return;
    }
    ri_sha256_init(&c);
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        ri_sha256_update(&c, buf, n);
    }
    fclose(f);
    ri_sha256_final(&c, digest);
    r->state = RI_STATE_PRESENT;
    r->check = memcmp(digest, r->sha256, 32) == 0 ? RI_CHECK_OK : RI_CHECK_WRONG;
}
