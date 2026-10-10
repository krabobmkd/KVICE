/** \file   rilist.h
 * \brief   KVICE Resource Installer: resourcelist.txt and the files on disk
 *
 * Plain C (stdio), tested on the host too.
 */

#ifndef RILIST_H
#define RILIST_H

#define RI_URLS_MAX 4
#define RI_PATH_MAX 256

/* Download column */
enum {
    RI_STATE_UNKNOWN = 0,   /* not checked yet */
    RI_STATE_PRESENT,       /* the file is there */
    RI_STATE_MISSING,       /* no file */
    RI_STATE_WAITING,       /* to download */
    RI_STATE_DOWNLOADING,
    RI_STATE_DOWNLOADED,
    RI_STATE_FAILED         /* no URL gave it */
};

/* Check column: the SHA-256 of the file on disk */
enum {
    RI_CHECK_NONE = 0,      /* no file, or not checked */
    RI_CHECK_OK,
    RI_CHECK_WRONG
};

typedef struct ri_resource_s {
    char path[64];                  /* "C64/basic-901226-01.bin" */
    unsigned char sha256[32];
    char *urls[RI_URLS_MAX];
    int url_count;
    int state;                      /* RI_STATE_* */
    int check;                      /* RI_CHECK_* */
} ri_resource_t;

/* Parse resourcelist.txt, # comments:
 *   base <url>              a server base: resources are at base + path,
 *                           several are tried in order
 *   <path> <sha256> [url]   a resource, URLs of its own after the bases
 * *out is malloc()ed, free it with ri_list_free(). Returns the number of
 * resources, -1 on a bad line (*bad_line set, 1...) or no memory. */
int ri_list_parse(const char *text, unsigned long len, ri_resource_t **out, int *bad_line);
void ri_list_free(ri_resource_t *list, int count);

/* drawer + "C64/x.bin" -> full path, AmigaDOS style ("Work:KVICE/C64/x.bin",
 * "PROGDIR:C64/x.bin") */
void ri_join_path(const char *drawer, const char *path, char *out, unsigned int size);

/* Check one resource on disk: sets state (present / missing) and check
 * (ok / wrong / none). */
void ri_check_resource(ri_resource_t *r, const char *drawer);

/* SHA-256 as 64 hex characters -> 32 bytes, 0 if it is not one */
int ri_parse_sha256(const char *hex, unsigned char out[32]);

#endif
