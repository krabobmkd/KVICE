/** \file   rimain.c
 * \brief   KVICE Resource Installer: the MUI window
 *
 * Lists the resource files of resourcelist.txt, checks them (SHA-256) and
 * downloads the missing or wrong ones, one by one, through the download
 * process (rinet.c, AmiSSL v5). The window never waits for the network.
 *
 * The program is installed in the KVICE drawer, next to the emulators:
 * the files are in PROGDIR: (PROGDIR:C64/..., PROGDIR:DRIVES/...).
 *
 * Words of the interface: "resource", never "ROM" nor "Commodore".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/classusr.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/alib.h>

#include "rilist.h"
#include "rinet.h"

#define APP_TITLE   "KVICE Resource Installer"
#define APP_VERSION "1.0"
#define LIST_FILE   "PROGDIR:resourcelist.txt"

static const char version_tag[] = "$VER: KVICEResourceInstaller " APP_VERSION " (09.10.2026)";

struct Library *MUIMasterBase = NULL;

#define MUI_MIN_VERSION 16

/* GCC-safe MUI_NewObject wrapper (noinline forces a real m68k stack frame),
 * as in KVICE amigamui.c */
static Object * __attribute__((noinline))
MUI_NewObjectB(const char *cl, Tag tags, ...)
{
    return MUI_NewObjectA((char *)cl, (struct TagItem *)&tags);
}

#ifndef MAKE_ID
#define MAKE_ID(a, b, c, d) \
    ((ULONG)(a) << 24 | (ULONG)(b) << 16 | (ULONG)(c) << 8 | (ULONG)(d))
#endif

/* MUIM_Application_ReturnID values */
#define RID_CHECK    1
#define RID_DOWNLOAD 2

/* where the resources are installed: the emulators' drawer */
#define TARGET_DRAWER "PROGDIR:"

static Object *app, *win, *list, *status_text;
static Object *bt_check, *bt_download, *bt_quit;

static ri_resource_t *resources = NULL;
static int resource_count = 0;

/* the download process, started at the first download */
static struct MsgPort *net_port = NULL;
static struct MsgPort *reply_port = NULL;
static RINetMessage net_msg;
static int downloading = 0;     /* net_msg is out */
static int download_total = 0;  /* of this round */
static int download_done = 0;
static char dest_path[RI_PATH_MAX];
static char status[256];

/* ------------------------------------------------------------------------- */
/* list */

static const char *state_text(int state)
{
    switch (state) {
        case RI_STATE_PRESENT:     return "present";
        case RI_STATE_MISSING:     return "\33bmissing";
        case RI_STATE_WAITING:     return "waiting";
        case RI_STATE_DOWNLOADING: return "\33bdownloading...";
        case RI_STATE_DOWNLOADED:  return "downloaded";
        case RI_STATE_FAILED:      return "\33bfailed";
        default:                   return "";
    }
}

static const char *check_text(int check)
{
    switch (check) {
        case RI_CHECK_OK:    return "ok";
        case RI_CHECK_WRONG: return "\33bwrong";
        default:             return "";
    }
}

static struct Hook display_hook;

static ULONG list_display(register struct Hook *hook __asm("a0"),
                          register char **array __asm("a2"),
                          register ri_resource_t *r __asm("a1"))
{
    if (r == NULL) {
        array[0] = (char *)"\33bResource";
        array[1] = (char *)"\33bDownload";
        array[2] = (char *)"\33bCheck";
    } else {
        array[0] = r->path;
        array[1] = (char *)state_text(r->state);
        array[2] = (char *)check_text(r->check);
    }
    return 0;
}

static void redraw(int index)
{
    DoMethod(list, MUIM_List_Redraw, (ULONG)index);
}

static void set_status(const char *text)
{
    snprintf(status, sizeof status, "%s", text);
    set(status_text, MUIA_Text_Contents, (ULONG)status);
}

static void show_summary(void)
{
    int i, ok = 0;

    for (i = 0; i < resource_count; i++) {
        if (resources[i].check == RI_CHECK_OK) {
            ok++;
        }
    }
    if (ok == resource_count) {
        snprintf(status, sizeof status,
                 "All %d resources are ok: the emulators are ready.", resource_count);
    } else {
        snprintf(status, sizeof status,
                 "%d of %d resources ok, %d missing or wrong: use \"Download missing\".",
                 ok, resource_count, resource_count - ok);
    }
    set(status_text, MUIA_Text_Contents, (ULONG)status);
}

static void check_all(void)
{
    int i;

    set(app, MUIA_Application_Sleep, TRUE);
    set_status("Checking...");
    for (i = 0; i < resource_count; i++) {
        ri_check_resource(&resources[i], TARGET_DRAWER);
        redraw(i);
    }
    set(app, MUIA_Application_Sleep, FALSE);
    show_summary();
}

/* ------------------------------------------------------------------------- */
/* downloads */

static void set_busy(int busy)
{
    set(bt_check, MUIA_Disabled, busy ? TRUE : FALSE);
    set(bt_download, MUIA_Disabled, busy ? TRUE : FALSE);
}

/* the next waiting resource to the download process, or the end */
static void send_next(void)
{
    int i, u;

    for (i = 0; i < resource_count; i++) {
        ri_resource_t *r = &resources[i];

        if (r->state != RI_STATE_WAITING) {
            continue;
        }
        r->state = RI_STATE_DOWNLOADING;
        redraw(i);
        DoMethod(list, MUIM_List_Jump, (ULONG)i);
        snprintf(status, sizeof status, "Downloading %s (%d of %d)...",
                 r->path, download_done + 1, download_total);
        set(status_text, MUIA_Text_Contents, (ULONG)status);

        ri_join_path(TARGET_DRAWER, r->path, dest_path, sizeof dest_path);
        memset(&net_msg, 0, sizeof net_msg);
        net_msg.rim_Msg.mn_ReplyPort = reply_port;
        net_msg.rim_Msg.mn_Length = sizeof net_msg;
        net_msg.rim_Type = RINETQ_DOWNLOAD;
        for (u = 0; u < r->url_count; u++) {
            net_msg.rim_Urls[u] = r->urls[u];
        }
        net_msg.rim_UrlCount = r->url_count;
        net_msg.rim_Sha256 = r->sha256;
        net_msg.rim_Dest = dest_path;
        net_msg.rim_Index = i;
        PutMsg(net_port, &net_msg.rim_Msg);
        downloading = 1;
        return;
    }
    downloading = 0;
    set_busy(0);
    show_summary();
}

static void download_missing(void)
{
    int i;

    if (net_port == NULL) {
        set_status("Starting the network...");
        net_port = RINet_Start();
        if (net_port == NULL) {
            set_status("Online resource installation needs AmiSSLv5 and a running TCP/IP stack.");
            return;
        }
    }
    download_total = 0;
    download_done = 0;
    for (i = 0; i < resource_count; i++) {
        if (resources[i].check != RI_CHECK_OK) {
            resources[i].state = RI_STATE_WAITING;
            download_total++;
            redraw(i);
        }
    }
    if (download_total == 0) {
        show_summary();
        return;
    }
    set_busy(1);
    send_next();
}

/* a download ended */
static void download_reply(void)
{
    struct Message *m;

    while ((m = GetMsg(reply_port)) != NULL) {
        RINetMessage *nm = (RINetMessage *)m;
        ri_resource_t *r;

        if (nm != &net_msg || nm->rim_Index < 0 || nm->rim_Index >= resource_count) {
            continue;
        }
        r = &resources[nm->rim_Index];
        download_done++;
        /* what is on the disk now */
        ri_check_resource(r, TARGET_DRAWER);
        if (nm->rim_Result == RINETR_OK && r->check == RI_CHECK_OK) {
            r->state = RI_STATE_DOWNLOADED;
        } else {
            r->state = RI_STATE_FAILED;
            if (nm->rim_Result == RINETR_WRITE) {
                /* the drawer is not writable: the others would fail too */
                int i;

                for (i = 0; i < resource_count; i++) {
                    if (resources[i].state == RI_STATE_WAITING) {
                        resources[i].state = RI_STATE_FAILED;
                        redraw(i);
                    }
                }
                redraw(nm->rim_Index);
                downloading = 0;
                set_busy(0);
                snprintf(status, sizeof status, "Cannot write %s.", dest_path);
                set(status_text, MUIA_Text_Contents, (ULONG)status);
                return;
            }
        }
        redraw(nm->rim_Index);
        send_next();
    }
}

/* ------------------------------------------------------------------------- */
/* window */

static Object *make_button(const char *text)
{
    return MUI_NewObjectB(MUIC_Text,
                          MUIA_Text_Contents, (ULONG)text,
                          MUIA_Text_PreParse, (ULONG)"\33c",
                          MUIA_Frame, MUIV_Frame_Button,
                          MUIA_Background, MUII_ButtonBack,
                          MUIA_InputMode, MUIV_InputMode_RelVerify,
                          MUIA_CycleChain, 1,
                          TAG_DONE);
}

static int create_app(void)
{
    display_hook.h_Entry = (ULONG (*)())list_display;
    list = MUI_NewObjectB(MUIC_List,
                          MUIA_Frame, MUIV_Frame_ReadList,
                          MUIA_List_Format, (ULONG)"BAR,BAR,",
                          MUIA_List_Title, TRUE,
                          MUIA_List_DisplayHook, (ULONG)&display_hook,
                          TAG_DONE);
    status_text = MUI_NewObjectB(MUIC_Text,
                                 MUIA_Frame, MUIV_Frame_Text,
                                 MUIA_Background, MUII_TextBack,
                                 MUIA_Text_Contents, (ULONG)"",
                                 TAG_DONE);
    bt_check = make_button("_Check");
    bt_download = make_button("_Download missing");
    bt_quit = make_button("_Quit");

    win = MUI_NewObjectB(MUIC_Window,
            MUIA_Window_Title, (ULONG)APP_TITLE,
            MUIA_Window_ID, MAKE_ID('K', 'R', 'E', 'S'),
            MUIA_Window_RootObject, (ULONG)MUI_NewObjectB(MUIC_Group,
                MUIA_Group_Child, (ULONG)MUI_NewObjectB(MUIC_Text,
                    MUIA_Text_Contents, (ULONG)
                        "\33cInstall resource files needed by the KVICE emulators.\n"
                        "\33cThey are checked in the KVICE drawer, and the missing\n"
                        "\33cones are downloaded there.",
                    TAG_DONE),
                MUIA_Group_Child, (ULONG)MUI_NewObjectB(MUIC_Listview,
                    MUIA_Listview_List, (ULONG)list,
                    MUIA_Listview_Input, FALSE,
                    TAG_DONE),
                MUIA_Group_Child, (ULONG)status_text,
                MUIA_Group_Child, (ULONG)MUI_NewObjectB(MUIC_Group,
                    MUIA_Group_Horiz, TRUE,
                    MUIA_Group_SameSize, TRUE,
                    MUIA_Group_Child, (ULONG)bt_check,
                    MUIA_Group_Child, (ULONG)bt_download,
                    MUIA_Group_Child, (ULONG)bt_quit,
                    TAG_DONE),
                TAG_DONE),
            TAG_DONE);

    app = MUI_NewObjectB(MUIC_Application,
            MUIA_Application_Title, (ULONG)APP_TITLE,
            MUIA_Application_Version, (ULONG)version_tag,
            MUIA_Application_Author, (ULONG)"krabobmkd",
            MUIA_Application_Description, (ULONG)"KVICE resource files check and download",
            MUIA_Application_Base, (ULONG)"KVICERESINST",
            MUIA_Application_Window, (ULONG)win,
            TAG_DONE);
    if (app == NULL) {
        return -1;
    }
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             (ULONG)app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(bt_quit, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(bt_check, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_CHECK);
    DoMethod(bt_download, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)app, 2, MUIM_Application_ReturnID, RID_DOWNLOAD);
    return 0;
}

/* resourcelist.txt next to the program */
static int load_list(void)
{
    FILE *f = fopen(LIST_FILE, "rb");
    char *text;
    long size;
    int bad = 0, i;

    if (f == NULL) {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > 256 * 1024 || (text = malloc((size_t)size)) == NULL) {
        fclose(f);
        return -1;
    }
    if (fread(text, 1, (size_t)size, f) != (size_t)size) {
        free(text);
        fclose(f);
        return -1;
    }
    fclose(f);
    resource_count = ri_list_parse(text, (unsigned long)size, &resources, &bad);
    free(text);
    if (resource_count <= 0) {
        resource_count = 0;
        return -1;
    }
    for (i = 0; i < resource_count; i++) {
        DoMethod(list, MUIM_List_InsertSingle, (ULONG)&resources[i], MUIV_List_Insert_Bottom);
    }
    return 0;
}

static void message(const char *text)
{
    if (app != NULL) {
        MUI_RequestA(app, win, 0, (char *)APP_TITLE, (char *)"_OK", (char *)text, NULL);
    } else {
        printf("%s\n", text);
    }
}

int main(void)
{
    ULONG sigs = 0, reply_sig;
    int running = 1;

    (void)version_tag;
    MUIMasterBase = OpenLibrary((CONST_STRPTR)MUIMASTER_NAME, MUI_MIN_VERSION);
    if (MUIMasterBase == NULL) {
        printf(APP_TITLE " needs MUI (muimaster.library).\n");
        return 20;
    }
    reply_port = CreateMsgPort();
    if (reply_port == NULL || create_app() != 0) {
        message("Cannot create the window (MUI).");
        if (reply_port) {
            DeleteMsgPort(reply_port);
        }
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    set(win, MUIA_Window_Open, TRUE);
    if (load_list() != 0) {
        message("Cannot read resourcelist.txt, the list of the\n"
                "resources, next to " APP_TITLE ".");
        running = 0;
    } else {
        check_all();
    }
    reply_sig = 1UL << reply_port->mp_SigBit;

    while (running) {
        LONG id = (LONG)DoMethod(app, MUIM_Application_NewInput, (ULONG)&sigs);

        switch (id) {
            case MUIV_Application_ReturnID_Quit:
                running = 0;
                continue;
            case RID_CHECK:
                if (!downloading) {
                    check_all();
                }
                break;
            case RID_DOWNLOAD:
                if (!downloading) {
                    download_missing();
                }
                break;
            default:
                break;
        }
        if (sigs) {
            sigs = Wait(sigs | reply_sig | SIGBREAKF_CTRL_C);
            if (sigs & SIGBREAKF_CTRL_C) {
                running = 0;
            }
            if (sigs & reply_sig) {
                download_reply();
            }
        }
    }

    if (net_port != NULL) {
        if (downloading) {
            set_status("Quitting: waiting for the download under way...");
        }
        /* also takes the reply of a download under way */
        RINet_Stop(net_port, reply_port);
        net_port = NULL;
    }
    set(win, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(app);
    ri_list_free(resources, resource_count);
    DeleteMsgPort(reply_port);
    CloseLibrary(MUIMasterBase);
    return 0;
}
