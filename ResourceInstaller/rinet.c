/** \file   rinet.c
 * \brief   KVICE Resource Installer: the download process (AmiSSL v5)
 *
 * The AmiSSL setup and the raw HTTP/1.1 GET with "Connection: close" come
 * from FriendSh3ep (network_fs3e/fs3enet_http.c): bsdsocket.library and
 * amisslmaster.library opened by hand, AmiSSL_ErrNoPtr on libnix' errno
 * (libamisslauto.a does not fit libnix), redirects followed by hand
 * (GitHub answers with them). Added here: chunked transfer decoding and
 * the Content-Length check, then the file is kept only with the expected
 * SHA-256, so a wrong or cut answer is never written.
 *
 * The process only uses dos.library for the files (not libnix stdio, which
 * belongs to the main process).
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/amissl.h>
#include <proto/amisslmaster.h>

#include <amissl/amissl.h>
#include <libraries/amisslmaster.h>
#include <libraries/amissl.h>

#include "rinet.h"
#include "risha256.h"

#define RINET_PROC_NAME     "KVICE Resource Installer net"
#define RINET_STACK_SIZE    65536
#define RINET_USER_AGENT    "Amiga KVICE Resource Installer"
#define RINET_MAX_PATH      2048
#define RINET_MAX_REDIRECTS 5
/* the resources are 2 to 32 KB: anything bigger is not one */
#define RINET_MAX_RESPONSE  (512UL * 1024UL)

struct Library *AmiSSLMasterBase = NULL, *SocketBase = NULL;
struct Library *AmiSSLBase = NULL, *AmiSSLExtBase = NULL;

static SSL_CTX *g_SSLCtx = NULL;

/* ------------------------------------------------------------------------- */
/* AmiSSL */

static BOOL http_init(void)
{
    if (!(SocketBase = OpenLibrary((CONST_STRPTR)"bsdsocket.library", 4))) {
        return FALSE;
    }
    if (!(AmiSSLMasterBase = OpenLibrary((CONST_STRPTR)"amisslmaster.library", AMISSLMASTER_MIN_VERSION))) {
        CloseLibrary(SocketBase);
        SocketBase = NULL;
        return FALSE;
    }
    if (OpenAmiSSLTags(AMISSL_CURRENT_VERSION,
            AmiSSL_UsesOpenSSLStructs, FALSE,
            AmiSSL_GetAmiSSLBase, (ULONG)&AmiSSLBase,
            AmiSSL_GetAmiSSLExtBase, (ULONG)&AmiSSLExtBase,
            AmiSSL_SocketBase, (ULONG)SocketBase,
            AmiSSL_ErrNoPtr, (ULONG)&errno,
            TAG_DONE) != 0) {
        CloseLibrary(AmiSSLMasterBase);
        AmiSSLMasterBase = NULL;
        CloseLibrary(SocketBase);
        SocketBase = NULL;
        return FALSE;
    }
    /* no certificate check: what is kept is checked by its SHA-256 */
    g_SSLCtx = SSL_CTX_new(TLS_client_method());
    if (!g_SSLCtx) {
        CloseAmiSSL();
        CloseLibrary(AmiSSLMasterBase);
        AmiSSLMasterBase = NULL;
        CloseLibrary(SocketBase);
        SocketBase = NULL;
        return FALSE;
    }
    return TRUE;
}

static void http_cleanup(void)
{
    if (g_SSLCtx) {
        SSL_CTX_free(g_SSLCtx);
        g_SSLCtx = NULL;
        CloseAmiSSL();
    }
    if (AmiSSLMasterBase) {
        CloseLibrary(AmiSSLMasterBase);
        AmiSSLMasterBase = NULL;
    }
    if (SocketBase) {
        CloseLibrary(SocketBase);
        SocketBase = NULL;
    }
}

/* ------------------------------------------------------------------------- */
/* HTTP GET */

/* the whole answer, up to the server closing the connection */
static UBYTE *read_all(BIO *bio, ULONG *out_len)
{
    ULONG cap = 16384, len = 0;
    UBYTE *buf = AllocVec(cap, MEMF_ANY);

    if (!buf) {
        return NULL;
    }
    for (;;) {
        int n;

        if (len + 4096 + 1 > cap) {
            ULONG newcap = cap * 2;
            UBYTE *newbuf;

            if (newcap > RINET_MAX_RESPONSE) {
                FreeVec(buf);
                return NULL;
            }
            newbuf = AllocVec(newcap, MEMF_ANY);
            if (!newbuf) {
                FreeVec(buf);
                return NULL;
            }
            CopyMem(buf, newbuf, len);
            FreeVec(buf);
            buf = newbuf;
            cap = newcap;
        }
        /* n <= 0: end (see fs3enet_http.c FS3EHttp_ReadBody()) */
        n = BIO_read(bio, buf + len, 4096);
        if (n <= 0) {
            break;
        }
        len += (ULONG)n;
    }
    buf[len] = '\0';
    *out_len = len;
    return buf;
}

/* the value of a header, any case, in [text, end), NULL if none */
static char *find_header(char *text, char *end, const char *name)
{
    size_t n = strlen(name);
    char *p = strstr(text, "\r\n");

    while (p != NULL && p < end) {
        char *line = p + 2;
        size_t i;

        for (i = 0; i < n; i++) {
            char c = line[i];

            if (c >= 'A' && c <= 'Z') {
                c = (char)(c - 'A' + 'a');
            }
            if (c != name[i]) {
                break;
            }
        }
        if (i == n && line[n] == ':') {
            line += n + 1;
            while (*line == ' ' || *line == '\t') {
                line++;
            }
            return line;
        }
        p = strstr(line, "\r\n");
    }
    return NULL;
}

/* chunked transfer: the chunks put together in place, their size, -1 if
 * the body is cut or malformed */
static long dechunk(UBYTE *body, ULONG len)
{
    ULONG in = 0, out = 0;

    for (;;) {
        unsigned long size = 0;
        char *end;

        if (in >= len) {
            return -1;
        }
        size = strtoul((char *)body + in, &end, 16);
        if ((UBYTE *)end == body + in) {
            return -1;
        }
        end = strstr(end, "\r\n");
        if (end == NULL) {
            return -1;
        }
        in = (ULONG)((UBYTE *)end - body) + 2;
        if (size == 0) {
            return (long)out;
        }
        if (in + size > len) {
            return -1;
        }
        memmove(body + out, body + in, size);
        out += size;
        in += size + 2;     /* CRLF after the data */
    }
}

/* GET url: the body (AllocVec'd, FreeVec() it) and its size, NULL on a
 * network error or a status other than 200. *status: the last one seen. */
static UBYTE *http_get(const char *url, ULONG *out_len, int *status)
{
    const char *cur = url;
    char redirect[RINET_MAX_PATH];
    int redirects_left = RINET_MAX_REDIRECTS;

    *status = 0;
    for (;;) {
        int use_ssl, port_num;
        char *user = NULL, *host = NULL, *port = NULL;
        char *path = NULL, *query = NULL, *frag = NULL;
        char path_buf[RINET_MAX_PATH], port_buf[16], host_port[300], req[1024];
        BIO *bio = NULL;
        SSL *ssl = NULL;
        UBYTE *raw = NULL, *body = NULL;
        ULONG raw_len = 0;
        BOOL is_redirect = FALSE;
        int n;

        if (!OSSL_HTTP_parse_url(cur, &use_ssl, &user, &host, &port, &port_num,
                                 &path, &query, &frag)) {
            return NULL;
        }
        if (!port) {
            snprintf(port_buf, sizeof port_buf, "%d", port_num);
            port = port_buf;
        }
        if (query && query[0]) {
            snprintf(path_buf, sizeof path_buf, "%s?%s", path ? path : "/", query);
        } else {
            snprintf(path_buf, sizeof path_buf, "%s", path ? path : "/");
        }
        snprintf(host_port, sizeof host_port, "%s:%s", host, port);

        if (use_ssl) {
            bio = BIO_new_ssl_connect(g_SSLCtx);
            if (bio) {
                BIO_set_conn_hostname(bio, host_port);
                BIO_get_ssl(bio, &ssl);
                if (ssl) {
                    /* SNI, needed by GitHub's servers */
                    SSL_set_tlsext_host_name(ssl, host);
                }
            }
        } else {
            bio = BIO_new(BIO_s_connect());
            if (bio) {
                BIO_set_conn_hostname(bio, host_port);
            }
        }
        if (!bio || BIO_do_connect(bio) <= 0) {
            goto next;
        }
        /* snprintf, not BIO_printf (a fixed arity macro in this SDK) */
        n = snprintf(req, sizeof req,
                     "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s\r\n"
                     "Accept: */*\r\nConnection: close\r\n\r\n",
                     path_buf, host, RINET_USER_AGENT);
        if (n <= 0 || n >= (int)sizeof req || BIO_write(bio, req, n) != n) {
            goto next;
        }
        raw = read_all(bio, &raw_len);
        if (raw) {
            char *text = (char *)raw;
            char *head_end = strstr(text, "\r\n\r\n");
            int code = 0;

            if (head_end && sscanf(text, "HTTP/%*d.%*d %d", &code) == 1) {
                *status = code;
                if (code >= 300 && code < 400 && redirects_left > 0) {
                    char *loc = find_header(text, head_end, "location");
                    char *loc_end = loc ? strstr(loc, "\r\n") : NULL;

                    if (loc_end && loc_end <= head_end && (ULONG)(loc_end - loc) < sizeof redirect) {
                        memcpy(redirect, loc, (size_t)(loc_end - loc));
                        redirect[loc_end - loc] = '\0';
                        if (strncmp(redirect, "http://", 7) != 0
                                && strncmp(redirect, "https://", 8) != 0) {
                            char resolved[RINET_MAX_PATH];

                            snprintf(resolved, sizeof resolved, "%s://%s%s%s",
                                     use_ssl ? "https" : "http", host,
                                     redirect[0] == '/' ? "" : "/", redirect);
                            snprintf(redirect, sizeof redirect, "%s", resolved);
                        }
                        is_redirect = TRUE;
                    }
                } else if (code == 200) {
                    UBYTE *start = (UBYTE *)head_end + 4;
                    ULONG len = raw_len - (ULONG)(start - raw);
                    char *te = find_header(text, head_end, "transfer-encoding");
                    char *cl = find_header(text, head_end, "content-length");
                    long size = (long)len;

                    if (te && strncmp(te, "chunked", 7) == 0) {
                        size = dechunk(start, len);
                    } else if (cl && (ULONG)strtoul(cl, NULL, 10) != len) {
                        /* cut on the way */
                        size = -1;
                    }
                    if (size >= 0) {
                        body = AllocVec((ULONG)size + 1, MEMF_ANY);
                        if (body) {
                            CopyMem(start, body, (ULONG)size);
                            *out_len = (ULONG)size;
                        }
                    }
                }
            }
            FreeVec(raw);
        }
next:
        if (bio) {
            BIO_free_all(bio);
        }
        if (user) {
            OPENSSL_free(user);
        }
        if (host) {
            OPENSSL_free(host);
        }
        if (port != port_buf) {
            OPENSSL_free(port);
        }
        if (path) {
            OPENSSL_free(path);
        }
        if (query) {
            OPENSSL_free(query);
        }
        if (frag) {
            OPENSSL_free(frag);
        }
        if (is_redirect) {
            redirects_left--;
            cur = redirect;
            continue;
        }
        return body;
    }
}

/* ------------------------------------------------------------------------- */
/* files */

/* the drawers of path ("Work:KVICE/C64/x.bin": Work:KVICE/C64) */
static void make_parent_drawers(const char *path)
{
    char buf[RI_PATH_MAX];
    char *p;

    snprintf(buf, sizeof buf, "%s", path);
    p = strchr(buf, ':');
    p = p ? p + 1 : buf;
    while ((p = strchr(p, '/')) != NULL) {
        BPTR lock;

        *p = '\0';
        lock = Lock((STRPTR)buf, ACCESS_READ);
        if (lock) {
            UnLock(lock);
        } else {
            lock = CreateDir((STRPTR)buf);
            if (lock) {
                UnLock(lock);
            }
        }
        *p++ = '/';
    }
}

static BOOL write_file(const char *path, const UBYTE *data, ULONG len)
{
    BPTR fh;
    BOOL ok;

    make_parent_drawers(path);
    fh = Open((STRPTR)path, MODE_NEWFILE);
    if (!fh) {
        return FALSE;
    }
    ok = Write(fh, (APTR)data, (LONG)len) == (LONG)len;
    Close(fh);
    if (!ok) {
        DeleteFile((STRPTR)path);
    }
    return ok;
}

static void handle_download(RINetMessage *m)
{
    int u;
    BOOL got_wrong = FALSE;

    m->rim_Result = RINETR_NETWORK;
    m->rim_HttpStatus = 0;
    for (u = 0; u < m->rim_UrlCount; u++) {
        ULONG len = 0;
        int status = 0;
        UBYTE *body = http_get(m->rim_Urls[u], &len, &status);
        unsigned char digest[32];

        if (status != 0) {
            m->rim_HttpStatus = status;
        }
        if (body == NULL) {
            continue;
        }
        ri_sha256(body, len, digest);
        if (memcmp(digest, m->rim_Sha256, 32) != 0) {
            /* not the expected file: the next URL may have it */
            got_wrong = TRUE;
            FreeVec(body);
            continue;
        }
        m->rim_Result = write_file(m->rim_Dest, body, len) ? RINETR_OK : RINETR_WRITE;
        FreeVec(body);
        return;
    }
    if (got_wrong) {
        m->rim_Result = RINETR_WRONG;
    }
}

/* ------------------------------------------------------------------------- */
/* the process */

/* startup handshake, on RINet_Start()'s stack (it waits for the reply) */
struct RINetStartup {
    struct Message ris_Msg;
    struct MsgPort *ris_RequestPort;
};

/* no NP_UserData on OS3: handed over through this */
static struct RINetStartup *g_startup;

static void rinet_entry(void)
{
    struct RINetStartup *startup = g_startup;
    struct MsgPort *port = CreateMsgPort();
    RINetMessage *shutdown = NULL;

    /* bsdsocket and AmiSSL belong to the task that opens them */
    if (port && !http_init()) {
        DeleteMsgPort(port);
        port = NULL;
    }
    startup->ris_RequestPort = port;
    PutMsg(startup->ris_Msg.mn_ReplyPort, &startup->ris_Msg);
    if (!port) {
        return;
    }
    while (shutdown == NULL) {
        RINetMessage *m;

        WaitPort(port);
        while ((m = (RINetMessage *)GetMsg(port)) != NULL) {
            if (m->rim_Type == RINETQ_SHUTDOWN) {
                /* replied after the cleanup: the main process may unload
                 * the program code as soon as it gets it */
                shutdown = m;
                continue;
            }
            if (shutdown == NULL && m->rim_Type == RINETQ_DOWNLOAD) {
                handle_download(m);
            } else {
                m->rim_Result = RINETR_NETWORK;
            }
            ReplyMsg(&m->rim_Msg);
        }
    }
    http_cleanup();
    DeleteMsgPort(port);
    Forbid();
    ReplyMsg(&shutdown->rim_Msg);
}

struct MsgPort *RINet_Start(void)
{
    struct RINetStartup startup;
    struct MsgPort *reply = CreateMsgPort();
    struct Process *proc;

    if (!reply) {
        return NULL;
    }
    memset(&startup, 0, sizeof startup);
    startup.ris_Msg.mn_ReplyPort = reply;
    startup.ris_Msg.mn_Length = sizeof startup;
    g_startup = &startup;
    proc = CreateNewProcTags(NP_Entry, (ULONG)rinet_entry,
                             NP_Name, (ULONG)RINET_PROC_NAME,
                             NP_StackSize, (ULONG)RINET_STACK_SIZE,
                             TAG_DONE);
    if (!proc) {
        DeleteMsgPort(reply);
        return NULL;
    }
    WaitPort(reply);
    GetMsg(reply);
    DeleteMsgPort(reply);
    return startup.ris_RequestPort;
}

void RINet_Stop(struct MsgPort *requestPort, struct MsgPort *replyPort)
{
    RINetMessage m;

    if (!requestPort) {
        return;
    }
    memset(&m, 0, sizeof m);
    m.rim_Msg.mn_ReplyPort = replyPort;
    m.rim_Msg.mn_Length = sizeof m;
    m.rim_Type = RINETQ_SHUTDOWN;
    PutMsg(requestPort, &m.rim_Msg);
    /* other replies may still come before it (downloads queued) */
    for (;;) {
        struct Message *got;

        WaitPort(replyPort);
        while ((got = GetMsg(replyPort)) != NULL) {
            if (got == &m.rim_Msg) {
                return;
            }
        }
    }
}
