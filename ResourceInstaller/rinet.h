/** \file   rinet.h
 * \brief   KVICE Resource Installer: the download process (AmiSSL v5)
 *
 * Same scheme as FriendSh3ep's network process: an AmigaDOS process of its
 * own opens bsdsocket.library and AmiSSL, and answers messages put on its
 * port. Downloads block that process, never the window.
 */

#ifndef RINET_H
#define RINET_H

#include <exec/types.h>
#include <exec/ports.h>

#include "rilist.h"

enum {
    RINETQ_DOWNLOAD = 1,
    RINETQ_SHUTDOWN
};

enum {
    RINETR_OK = 0,          /* downloaded, SHA-256 right, written */
    RINETR_NETWORK,         /* no URL answered with the file */
    RINETR_WRONG,           /* downloaded, but a wrong SHA-256 from every URL */
    RINETR_WRITE            /* the file could not be written */
};

typedef struct RINetMessage {
    struct Message rim_Msg;
    int rim_Type;                       /* RINETQ_* */
    /* download: tried in order until one gives the right SHA-256 */
    const char *rim_Urls[RI_URLS_MAX];
    int rim_UrlCount;
    const unsigned char *rim_Sha256;    /* 32 bytes */
    const char *rim_Dest;               /* full path of the file to write */
    int rim_Index;                      /* list entry, for the caller */
    /* reply */
    int rim_Result;                     /* RINETR_* */
    int rim_HttpStatus;                 /* last HTTP status seen, 0 if none */
} RINetMessage;

/* Start the process: its request port, NULL if it could not start (no
 * TCP/IP stack, no AmiSSL v5). */
struct MsgPort *RINet_Start(void);

/* Stop it (waits for the download under way, if any). replyPort: any free
 * port of the caller. */
void RINet_Stop(struct MsgPort *requestPort, struct MsgPort *replyPort);

#endif
