/*******************************************************************************
 *
 * Copyright 2023 Portable Software Company
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/

/**
 * dbcsc.c
 *
 * Smart Client
 * TCP/IP client for remote execution of DB/C applications.
 *
 * Parameters:
 *		host                     Server's name (required)
 *		-hostport=<port number>  Server's port (optional)
 *		-localport=<port number> Local port for receiving server commands, dynamic if ommited (optional)
 *		-a=<memsize>             Number of K to allocate for memory
 *      -t=<tdb name>			 Terminal definition file specification (optional)
 *		-encrypt=<on|off>        Use SSL encryption (optional, on by default)
 *		-user=<username>		 Effective user name (optional)
 *		-curdir=<directory>      Directory to start dbc from (optional)
 *      -pl                      Client uses PC BIOS characters, server uses Latin 1
 *      -lp                      Client uses Latin 1 characters, server uses PC BIOS
 *		-sslstrength=[low | high]
 *		Plus DX runtime parameters
 *
 *		Undocumented options:
 *		-xmllog, Creates scxml.log which contains every xml element sent to and from
 *               the client.
 *		-bypass, Skips initial connection that passes arguments to DX server daemon
 *               -localport option must be used with -bypass.
 */

#define INC_STDIO
#define INC_STDLIB
#define INC_STRING
#define INC_CTYPE
#define INC_TIME
#define INC_SIGNAL
#define INC_ERRNO
#define INC_LIMITS
#include "includes.h"
#include "release.h"
#if OS_WIN32
#include <strsafe.h>
#pragma warning(disable : 4995)
#if _MSC_VER >= 1800
#include <VersionHelpers.h>
#endif
#endif

#if OS_WIN32
#define ERRORVALUE() WSAGetLastError()
#define sleep(a) Sleep((a) * 1000)
#endif

#if OS_UNIX
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <netinet/tcp.h>
#ifndef FD_SET
#include <sys/select.h>
#endif
#define closesocket(a) close(a)
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif
typedef struct timeval TIMEVAL;
#define ERRORVALUE() errno
#define EVENTS_READ (DEVPOLL_READ | DEVPOLL_READPRIORITY | DEVPOLL_READNORM | DEVPOLL_READBAND)
#define EVENTS_WRITE (DEVPOLL_WRITE)
#define EVENTS_ERROR (DEVPOLL_ERROR | DEVPOLL_HANGUP | DEVPOLL_NOFDES)
#define ARRAYSIZE(A) (sizeof(A)/sizeof((A)[0]))
#endif

#include "arg.h"
#include "base.h"
#include "evt.h"
#include "fio.h"
#include "gui.h"
#include "guix.h"
#include "imgs.h"
#include "prt.h"
#include "tim.h"
#include "tcp.h"
#include "vid.h"
#include "vidx.h"
#include "xml.h"
#include "scfxdefs.h"

#ifndef INADDR_NONE
#define INADDR_NONE (unsigned long) 0xFFFFFFFF
#endif

#define VID_TYPE_CHAR	0x00
#define VID_TYPE_WIN	0x01
#define VID_TYPE_STATE	0x02
#define VID_TYPE_SCRN	0x03

#define KYDS_COMPAT		0x00000001
#define KYDS_UPPER		0x00000002
#define KYDS_DCON		0x00000004
#define KYDS_OLDHOFF	0x00000020
#define KYDS_RESETSW	0x00000040
#define KYDS_EDITON		0x00000100
#define KYDS_FSHIFT		0x00000200
#define KYDS_FUNCKEY	0x00001000
#define KYDS_TIMEOUT	0x00002000
#define KYDS_TRAP		0x00004000
#define KYDS_DOWN		0x00008000
#define KYDS_BACKGND	0x00010000
#define KYDS_EDIT		0x00040000
#define KYDS_FORMAT		0x00080000
#define KYDS_LITERAL	0x00100000
#define KYDS_KCON		0x00200000
#define KYDS_KL			0x00800000
#define KYDS_RV			0x02000000

#define DSYS_ROLLOUTWIN 0x00000001

#define MAXPRTVALUES		   128

#define EVENT_BREAK        		 0
#define EVENT_TRAPCHAR     		 1
#define EVENT_DEVTIMER    		 2
#define EVENT_CANCEL        	 3
#define EVENT_SHUTDOWN			 4
#define EVENT_KATIMER           5
#define EVENT_MAXIMUM           6

#define KEYIN_CANCEL	0x01
#define KEYIN_REPLIED	0x02

#define BUFFER_SIZE		1024

static CHAR xkeymap[256];	/* key translate map for chars 0-255 */
static CHAR lkeymap[256];	/* lower case translation map */
static CHAR ukeymap[256];	/* upper case translation map */

#if OS_WIN32GUI
UCHAR dbcbuf[65504];
#endif

static INT latin1flag;		/* client wants to work with Latin 1, server uses PC BIOS */
static INT pcbiosflag;		/* client wants to work with PC BIOS, server uses Latin 1 */
static UCHAR latin1map[] = {	/* table to translate from PC BIOS characters to Latin 1 */
	0xC7, 0xFC, 0xE9, 0xE2, 0xE4, 0xE0, 0xE5, 0xE7,	0xEA, 0xEB,  '?', 0xEF, 0xEE, 0xEC, 0xC4, 0xC5, /* 0x80 - 0x8F */
	0xC9, 0xE6,	0xC6, 0xF4,	0xF6, 0xF2,	0xFB, 0xF9,	0xFF, 0xD6,	0xDC, 0xA2, 0xA3, 0xA5,  '?',  '?', /* 0x90 - 0x9F */
	0xE1, 0xED,	0xF3, 0xFA, 0xF1, 0xD1, 0xAA, 0xBA, 0xBF,  '?',	0xAC, 0xBD, 0xBC, 0xA1,	0xAB, 0xBB, /* 0xA0 - 0xAF */
	 '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?', /* 0xB0 - 0xBF */
	 '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?', /* 0xC0 - 0xCF */
	 '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?', /* 0xD0 - 0xDF */
	 '?', 0xDF,  '?',  '?',  '?',  '?', 0xB5,  '?',  '?',  '?',  '?',  '?',  '?', 0xF8,  '?',  '?', /* 0xE0 - 0xEF */
	 '?', 0xB1,	 '?',  '?',  '?',  '?', 0xF7,  '?', 0xB0,  '?', 0xB7,  '?',  '?', 0xB2,  '?',  '?'  /* 0xF0 - 0xFF */
};
static UCHAR pcbiosmap[] = {	/* table to translate from Latin 1 to PC BIOS characters */
 	 '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?', /* 0x80 - 0x8F */
	 '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?', /* 0x90 - 0x9F */
	 '?', 0xAD, 0x9B, 0x9C,  '?', 0x9D,	0x7C,  '?',	 '?',  '?', 0xA6, 0xAE, 0xAA, 0x2D,	 '?',  '?', /* 0xA0 - 0xAF */
	0xF8, 0xF1,	0xFD,  '?',  '?', 0xE6,	 '?', 0xFA,	 '?',  '?', 0xA7, 0xAF, 0xAC, 0xAB,	 '?', 0xA8, /* 0xB0 - 0xBF */
	 '?',  '?',	 '?',  '?', 0x8E, 0x8F, 0x92, 0x80,	 '?', 0x90,	 '?',  '?',  '?',  '?',	 '?',  '?', /* 0xC0 - 0xCF */
	 '?', 0xA5,	 '?',  '?',	 '?',  '?', 0x99, 0x78,	 '?',  '?',	 '?',  '?',	0x9A,  '?',	 '?', 0xE1,	/* 0xD0 - 0xDF */
	0x85, 0xA0, 0x83,  '?',	0x84, 0x86,	0x91, 0x87,  '?', 0x82, 0x88, 0x89,	0x8D, 0xA1,	0x8C, 0x8B, /* 0xE0 - 0xEF */
	 '?', 0xA4, 0x95, 0xA2, 0x93,  '?', 0x94, 0xF6, 0xED, 0x97, 0xA3, 0x96, 0x81,  '?',  '?', 0x98  /* 0xF0 - 0xFF */
};

typedef struct SOCKETBUF_STRUCT {
	struct SOCKETBUF_STRUCT *next;
	UCHAR flags;
	CHAR serial[8];
	INT socketBufSize;
	CHAR socketBuf[1];
} SOCKETBUF;

#if OS_WIN32
static INT sysflags;
#endif

static FILE *xmllog = NULL;
static CHAR lasterror[256];
static INT recvevent;						/* finished parsing, ready to receive */
static INT maineventids[EVT_MAX_EVENTS];
static INT maineventidcount = EVENT_MAXIMUM;
static INT server_major_version;
static CHAR ka_timstamp[16];	/* Keep Alive TimeStamp */
/**
 * Keep alive messages time interval, in hundredths of a second.
 * Hard coded at 30 seconds.
 *
 * So far, there is no config option to adjust this.
 */
static INT keepalivetime = 3000;


CHAR pgmname[13] = "Smart Client";

static INT cnsflag = FALSE;					/* true if console mode */
static INT conflag;							/* true after first connection has been established */
static INT guiflag;							/* true after guiinit has been called   */
static INT logflag;							/* log input and output			        */
static INT vidflag;							/* true after vidinit has been called	*/
static INT thrflag;							/* true after receive thread has been created */
static INT abtflag;							/* terminate application a.s.a.p. (abort flag) */
static INT tcpflag;							/* contains flag information for tcpsend and tcprecv */
static INT prtflag;							/* true after prtinit has been called */

/*
 * Memory allocation size rules;
 * If -a is supplied, then use it.
 * Else, if dbcdx.memalloc is supplied, then use it.
 * Else, use 4096
 */
static UINT config_memalloc;				/* Used to hold dbcdx.memalloc if supplied from server */
static UINT meminitsize;					/* amount of memory allocated before config processing */
static INT dash_a_supplied = FALSE;

/** Keyin & Display **/
static INT32 vidcmd[100];					/* commands for keyin and display		*/
static INT kydsflags;						/* keyin and display flags				*/
static UINT deffgcolor, defbgcolor;
static INT breakeventid;
static INT keyineventid;
static UCHAR trapcharmap[64];				/* keep track of all active traps					*/
static UCHAR trapnullmap[64];  				/* null character trap map (used during disable)	*/
static VIDPARMS vidparms;
static CHAR *prop_keyin_interrupt;			/* runtime property */
static CHAR *prop_keyin_ikeys;				/* runtime property */
static CHAR *prop_keyin_endkey;				/* runtime property */
static CHAR *prop_keyin_case;				/* runtime property */
static CHAR *prop_keyin_cancelkey;			/* runtime property */
static CHAR *prop_display_color;			/* runtime property */
static CHAR *prop_display_bgcolor;			/* runtime property */
static CHAR *prop_display_hoff;				/* runtime property */
static CHAR *prop_display_resetsw;			/* runtime property */
static CHAR *prop_display_autoroll;			/* runtime property */
static CHAR *prop_print_error;				/* runtime property */
static CHAR *prop_print_autoflush;			/* runtime property */
static CHAR *prop_print_language;			/* runtime property */
static CHAR *prop_print_translate;			/* runtime property */
static CHAR *prop_print_lock;				/* runtime property */
static UINT prop_print_pdf_oldtab;			/* runtime property */
#if OS_WIN32
static CHAR *prop_keyin_fkeyshift;			/* runtime property */
#endif

static INT sendresult(INT);

/** Printing **/
static INT sendprintresult(INT);
static INT srvgetserver(CHAR *, INT);

/** File Transfer **/
static INT clientGetFile(ELEMENT*);
static void ConvertFileXferErrorCodeToString(INT err, ATTRIBUTE *a1);
static INT sendClientPutFileError(INT xferErrCode, INT xtraErrCode);
static CHAR fileTransferLocaldir[MAX_PATH];
static INT sendClientGetFileResult(INT i1);

#if OS_WIN32
static HANDLE threadevent;
static HANDLE threadhandle;
static DWORD threadid;
#endif

static CHAR serial[8];
static CHAR errstr[512];				/* error information */
static CHAR *argbuf;					/* DB/C arguments */
static SOCKET sock;						/* Socket bound to the server */
static SOCKETBUF *msgqueuehead;			/* queue for messages received */

#if OS_WIN32
/* Structure for linked list of windows used */
typedef struct WINDOWREF_STRUCT {
	struct WINDOWREF_STRUCT **next;
	WINHANDLE handle;
	CHAR name[9];
} WINDOWREF;

/* Structure for linked list of images used */
typedef struct IMAGEREF_STRUCT {
	struct IMAGEREF_STRUCT **next;
	PIXHANDLE handle;
	CHAR name[MAXRESNAMELEN + 1];
} IMAGEREF;

/* Structure for linked list of menus used */
typedef struct RESOURCEREF_STRUCT {
	struct RESOURCEREF_STRUCT **next;
	RESHANDLE handle;
	CHAR name[MAXRESNAMELEN + 1];
} RESOURCEREF;

WINDOWREF **firstwindow;
IMAGEREF **firstimage;
RESOURCEREF **firsticon;
RESOURCEREF **firstmenu;
RESOURCEREF **firsttoolbar;
RESOURCEREF **firstpnldlg;
#endif

INT mainentry(INT argc, CHAR **, CHAR **);
static INT doprtopen(ELEMENT *);
static INT doprtclose(ELEMENT *);
static INT doprttext(ELEMENT *);
static INT doprtnoej(ELEMENT *);
static INT doprtrel(ELEMENT *);
static INT doprtalloc(ELEMENT *);
static INT doprtsetup(void);
static INT doprtexit(void);
static INT doprtget(ELEMENT *, INT);
static INT dovidgetwindow(void);
static INT doviddisplay(ELEMENT *, INT flags);
static INT doviddisplaycc(ELEMENT *, INT flags, INT *);
static INT dovidkeyin(ELEMENT *, INT flags);
static INT dovidrestore(INT, CHAR *);
static int sendAliveChk(void);
static void usage(void);
static INT kdsinit(void);
static INT kdspause(INT);
static INT terminate(void);
static void message(CHAR *, CHAR *);
static CHAR *genarglist(CHAR *);
static INT processrecv(void);
static INT recvelement(void);
static INT sendelement(ELEMENT *, INT);
static INT parsecommand(ELEMENT *, INT flags);
static void maptranslate(INT, CHAR *);
static void timstructToString(TIMSTRUCT *timstruct, CHAR *timestamp, INT size);
#if OS_WIN32
static INT doPrepCreateDialogRef(CHAR *name, UINT len, RESHANDLE reshandle);
static DWORD WINAPI winthreadstart(LPVOID);
static void lrtrim(CHAR *, UCHAR *, INT);
static void writeTimeToLog(void);
#else
static INT unixrecvcb(void *, INT);
#endif

static INT dosound(ELEMENT *);
static INT dorollout(CHAR *);
#if OS_WIN32
static INT doprep(ELEMENT *);
static INT dodraw(ELEMENT *);
static INT doshow(ELEMENT *);
static INT dohide(ELEMENT *);
static INT doclose(ELEMENT *);
static INT doquery(ELEMENT *);
static INT dogetimage(ELEMENT *);
static INT dogetclipboard(ELEMENT *);
static INT dosetclipboard(ELEMENT *);
static INT dostoreimage(ELEMENT *);
static INT dogetpos(ELEMENT *);
static INT dogetcolor(ELEMENT *);
static INT dochange(ELEMENT *);
static INT dochangedevice(ELEMENT *);
static INT dochangeresource(ELEMENT *);
static INT dochangeresnew(ELEMENT *e1);
static INT dochangeresold(ELEMENT *e1);
static IMAGEREF** findImageRef(ATTRIBUTE *a1);
static RESOURCEREF** findResourceRef(ATTRIBUTE *a1);
static INT sendimageelement(ELEMENT *, INT);
static INT parsepanelsubelements(ELEMENT *);
static INT parsemenusubelements(ELEMENT *);
static INT parsespeedkey(CHAR *);
static void parsefontelement(CHAR *font, size_t cbfont, ATTRIBUTE *, struct _guiFontParseControl *);
static void devicecbfnc(WINHANDLE, UCHAR *, INT);
static void resourcecbfnc(RESHANDLE, UCHAR *, INT);
static void guimessage(UCHAR *, INT);
static void initializeFontParseControl(INT isDraw);
static INT parsenumber(CHAR *);					/* used for boxtabs */
static INT ctlmemicmp(void *, void *, INT);		/* used for speed keys */
#endif

/*
 * Define these to keep the linker happy.
 * Not used by the SC.
 */
CHAR DXAboutInfoString[1];
#if OS_WIN32GUI
INT dbcflags = 0;
#endif

/*
 * UNIX and WIN CONSOLE Entry point
 */
#if !(defined(_WINDOWS) && defined(__GNUC__))
INT main(INT argc, CHAR *argv[], CHAR *envp[])
{
	cnsflag = TRUE;
	return mainentry(argc, argv, envp);
}
#endif

/*
 * WIN32 Entry point
 */
INT mainentry(INT argc, CHAR *argv[], CHAR *envp[])
{
	INT i1, i2, bypassflag, len, localport, pos, serverport;
	CHAR work[16], *buffer;
	CHAR host[256], username[64], tdbfile[256], hostdir[256], recvbuf[256];
	SOCKET tempsock;
	ELEMENT e1, e2, *eptr1;
	ATTRIBUTE a1, a2;
	TIMSTRUCT timstruct;
	CHAR *workptr;
#if OS_WIN32
	WSADATA versioninfo;
#if OS_WIN32GUI
	int true = 1;
#endif
	sysflags = 0;
#endif
	kydsflags = 0;
	host[0] = '\0';
	localport = -1;
	tdbfile[0] = '\0';
	hostdir[0] = '\0';
	username[0] = '\0';
	latin1flag = pcbiosflag = FALSE;
	bypassflag = logflag = guiflag = conflag = FALSE;
	vidflag = thrflag = abtflag = prtflag = FALSE;
	tcpflag = TCP_SSL | TCP_UTF8;
	serverport = 9735;

	/**
	 * Set various character mode color things to the 'OLD' values as a start
	 * They can only be changed by a config entry
	 */
	CELLWIDTH = 3;
	DSP_FG_COLORMAX = 16;
	DSP_BG_COLORMAX = 16;
	DSPATTR_FGCOLORMASK	= 0x00000F00;
	DSPATTR_BGCOLORMASK	= 0x0000F000;
	DSPATTR_FGCOLORSHIFT = 8;
	DSPATTR_BGCOLORSHIFT = 12;

	buffer = malloc(BUFFER_SIZE);
	if (buffer == NULL) {
		message("malloc fail", "Startup Failure");
		return terminate();
	}


	/*
	 * Initialize the font parse control structure to the 'out of box' defaults
	 */
#if OS_WIN32
	StringCbCopy(guiFontParseControl.initialFontName, sizeof(guiFontParseControl.initialFontName),
			"system");
	guiFontParseControl.initialBold = 0;
	guiFontParseControl.initialItalic = 0;
	guiFontParseControl.initialUnderline = 0;
	guiFontParseControl.initialSize = 9;
	guiFontParseControl.initialDrawSize = atoi(INITIAL_DRAW_TEXT_SIZE);
#endif

	/** Parse arguments **/
	arginit(argc, argv, &i1);
	while (!argget(ARG_NEXT | ARG_IGNOREOPT, buffer, BUFFER_SIZE)) {
		if (!host[0]) {
			if (buffer[0] != '-') {
				strcpy(host, buffer);
				continue;
			}
		}
		if (buffer[0] == '-') {
			if (buffer[1] == '?') {
				usage();
				return terminate();
			}
			else {
				if (!memcmp(buffer + 1, "hostport=", 9) || !memcmp(buffer + 1, "HOSTPORT=", 9)) {
					if (isdigit(buffer[10])) serverport = atoi(buffer + 10);
				}
				else if (!memcmp(buffer + 1, "localport=", 10) || !memcmp(buffer + 1, "LOCALPORT=", 10)) {
					if (isdigit(buffer[11])) localport = atoi(buffer + 11);
				}
				else if (!memcmp(buffer + 1, "xmllog", 6) || !memcmp(buffer + 1, "XMLLOG", 6)) {
					logflag = TRUE;
				}
				else if (!memcmp(buffer + 1, "bypass", 6) || !memcmp(buffer + 1, "BYPASS", 6)) {
					bypassflag = TRUE;
					tcpflag &= ~TCP_SSL;
				}
				else if (!memcmp(buffer + 1, "user=", 5) || !memcmp(buffer + 1, "USER=", 5)) {
					strcpy(username, buffer + 6);
				}
				else if (!memcmp(buffer + 1, "curdir=", 7) || !memcmp(buffer + 1, "CURDIR=", 7)) {
					strcpy(hostdir, buffer + 8);
				}
				else if (!memcmp(buffer + 1, "encrypt=", 8) || !memcmp(buffer + 1, "ENCRYPT=", 8)) {
					if (!memcmp(buffer + 9, "off", 3)) tcpflag &= ~TCP_SSL;
					else if (!memcmp(buffer + 9, "on", 2)) tcpflag |= TCP_SSL;
				}
				else if (!memcmp(buffer + 1, "t=", 2) || !memcmp(buffer + 1, "T=", 2)) {
					strcpy(tdbfile, buffer + 3);
				}
				else if (!memcmp(buffer + 1, "a=", 2) || !memcmp(buffer + 1, "A=", 2)) {
					meminitsize = atoi(buffer + 3);
					dash_a_supplied = TRUE;
				}
				else if ((!latin1flag && !memcmp(buffer + 1, "pl", 2)) || !memcmp(buffer + 1, "PL", 2)) {
					pcbiosflag = TRUE;
				}
				else if ((!pcbiosflag && !memcmp(buffer + 1, "lp", 2)) || !memcmp(buffer + 1, "LP", 2)) {
					latin1flag = TRUE;
				}

				/*
				 * Absorb options used by other SCs
				 */
				else if (guimemicmp("trace", buffer + 1, 5) == 0) /* do nothing */ ;
				else if (guimemicmp("traceob", buffer + 1, 7) == 0) /* do nothing */ ;

				else {
					/* store arguments so we can send them to the server */
					genarglist(buffer);
					genarglist(" ");
				}
			}
		}
		else {
			genarglist(buffer);
			genarglist(" ");
		}
	}

	if (logflag == TRUE) {
		xmllog = fopen("./scxml.log", "w");
	}

	if (!host[0] && !bypassflag) {
		usage();
		return terminate();
	}

	if (meminitsize <= 0) meminitsize = 4096;
	if (meminit(meminitsize << 10, 0, 4096) == -1) {
		message("Not enough memory available.", "Startup Failure");
		return terminate();
	}

	if (((maineventids[EVENT_BREAK] = evtcreate()) == -1) ||
       ((maineventids[EVENT_TRAPCHAR] = evtcreate()) == -1) ||
       ((maineventids[EVENT_CANCEL] = evtcreate()) == -1) ||
       ((maineventids[EVENT_DEVTIMER] = evtcreate()) == -1) ||
       ((maineventids[EVENT_SHUTDOWN] = evtcreate()) == -1) ||
       ((maineventids[EVENT_KATIMER] = evtcreate()) == -1) ||
       ((recvevent = evtcreate()) == -1)) {
		message("Event creation failed.", "Startup Failure");
		return terminate();
	}
	breakeventid = maineventids[EVENT_BREAK];
	memset(trapcharmap, 0, (VID_MAXKEYVAL >> 3) + 1);
	memset(trapnullmap, 0, (VID_MAXKEYVAL >> 3) + 1);
	memset(&vidparms, 0, sizeof(VIDPARMS));
	if (tdbfile[0]) vidparms.termdef = tdbfile;

	for (i1 = 0; i1 <= 255; i1++) xkeymap[i1] = (CHAR) i1;
	memcpy(ukeymap, xkeymap, 256);
	memcpy(lkeymap, xkeymap, 256);
	for (i1 = 'A'; i1 <= 'Z'; i1++) {
		ukeymap[tolower(i1)] = (CHAR) i1;
		lkeymap[i1] = (CHAR) tolower(i1);
	}

	/* Create server sockets dynamically or on localport, i1 is dynamic port number */
#if OS_WIN32
	if ((i1 = WSAStartup(MAKEWORD(2, 2), &versioninfo))) {
		sprintf(errstr, "Winsock startup failure, error = %d", i1);
		return terminate();
	}
#endif
	sock = INVALID_SOCKET;
	conflag = TRUE;
	if (localport) {
		if (localport == -1) {
			if (bypassflag) {
				message("You must use -localport option with -bypass option", "Parameter Error");
				return terminate();
			}
			localport = 0;
		}
		sock = tcplisten(localport, &localport);
		if (sock == INVALID_SOCKET) {
			sprintf(errstr, "Establishing Server Socket failed: %s", tcpgeterror());
			message(errstr, "Socket Error");
			return terminate();
		}
	}
	else if (bypassflag) {
		message("-bypass option not supported with -localport=0", "Parameter Error");
		return terminate();
	}

	if (!bypassflag) {
		/* communicate version to/from server */
		tempsock = tcpconnect(host, serverport, TCP_UTF8, NULL, 15);
		if (tempsock == INVALID_SOCKET) {
			sprintf(errstr, "Unable to connect with Smart Client server at %s %d: %s", host, serverport, tcpgeterror());
			message(errstr, "Socket Error");
			return terminate();
		}
		i1 = 16;
		strcpy(buffer + i1, "<hello>DB/C SC ");
		i1 += (INT)strlen(buffer + i1);
		strcpy(buffer + i1, RELEASE);
		i1 += (INT)strlen(buffer + i1);
		strcpy(buffer + i1, "</hello>");
		i1 += (INT)strlen(buffer + i1);
		memset(buffer, ' ', 8);
		msciton(i1 - 16, (UCHAR *) buffer + 8, 8);
		if (tcpsend(tempsock, (UCHAR *) buffer, i1, TCP_UTF8, -1) <= 0) {
			sprintf(errstr, "Sending data to the server failed. Error: %s", tcpgeterror());
			message(errstr, "Socket Error");
			closesocket(tempsock);
			return terminate();
		}
		if (logflag == TRUE && xmllog != NULL) {
			buffer[16 + i1] = '\0';
			fprintf(xmllog, "SND: ");
#if OS_WIN32
			writeTimeToLog();
#endif
			fprintf(xmllog, "%s\n", buffer + 16);
			fflush(xmllog);
		}
		pos = 0;
		for ( ; ; ) {
			i1 = tcprecv(tempsock, (UCHAR *) recvbuf + pos, sizeof(recvbuf) - 1 - pos, TCP_UTF8, 30);
			if (i1 <= 0) {
				sprintf(errstr, "Socket receive failed. Error: %s", tcpgeterror());
				message(errstr, "Socket Error");
				closesocket(tempsock);
				return terminate();
			}
			pos += i1;
			if (pos >= 16) {
				if (memcmp(recvbuf, buffer, 8)) {
					message("Serialization failure", "Socket Error");
					closesocket(tempsock);
					return terminate();
				}
				mscntoi((UCHAR *)recvbuf + 8, &len, 8);
				if (len <= 0 || len >= (INT) (sizeof(recvbuf) - 16)) {
					message("Corrupted packet", "Socket Error");
					closesocket(tempsock);
					return terminate();
				}
				if (pos >= 16 + len) break;
			}
		}
		closesocket(tempsock);
		if (logflag == TRUE && xmllog != NULL) {
			recvbuf[16 + len] = '\0';
			fprintf(xmllog, "RCV: ");
#if OS_WIN32
			writeTimeToLog();
#endif
			fprintf(xmllog, "%s\n", recvbuf + 16);
			fflush(xmllog);
		}
		i1 = xmlparse(recvbuf + 16, len, buffer, BUFFER_SIZE * sizeof(CHAR));
		if (i1) {
			recvbuf[16 + len] = '\0';
			sprintf(errstr, "XML Parsing failed: %s.  XML was: %s", xmlgeterror(), recvbuf + 16);
			message(errstr, "Client");
			return terminate();
		}
		eptr1 = (ELEMENT *) buffer;
		if (eptr1 == NULL || strcmp(eptr1->tag, "ok")) {
			if (eptr1 != NULL && eptr1->firstsubelement != NULL && eptr1->firstsubelement->cdataflag)
				message(eptr1->firstsubelement->tag, "Server Error");
			else message("Unknown reason", "Server Error");
			return terminate();
		}
		eptr1 = eptr1->firstsubelement;
		if (eptr1 == NULL || !eptr1->cdataflag || strncmp(eptr1->tag, "DB/C SS ", 8)) {
			message("Unrecognized Server 'ok' element.", "Server Error");
			return terminate();
		}
		workptr = eptr1->tag + 8;
		server_major_version = 0;
		while (isdigit(*workptr)) {
			server_major_version = server_major_version * 10 + (*workptr++ - '0');
		}
		if (server_major_version > DX_MAJOR_VERSION) {
			message("Server version must be equal or less than client", "Server Error");
			return terminate();
		}

		/* communicate start to server */
		tempsock = tcpconnect(host, serverport, TCP_UTF8, NULL, 15);
		if (tempsock == INVALID_SOCKET) {
			sprintf(errstr, "Creation of First Socket Failed: %s", tcpgeterror());
			message(errstr, "Socket Error");
			return terminate();
		}
		i1 = 16;
		strcpy(buffer + i1, "<start");
		i1 += (INT)strlen(buffer + i1);
		if (localport) {
			strcpy(buffer + i1, " port=");
			i1 += (INT)strlen(buffer + i1);
			i1 += mscitoa(localport, buffer + i1);
		}
		if (username[0]) {
			strcpy(buffer + i1, " user=");
			i1 += (INT)strlen(buffer + i1);
			strcpy(buffer + i1, username);
			i1 += (INT)strlen(buffer + i1);
		}
		if (hostdir[0]) {
			strcpy(buffer + i1, " dir=\"");
			i1 += (INT)strlen(buffer + i1);
			strcpy(buffer + i1, hostdir);
			i1 += (INT)strlen(buffer + i1);
			buffer[i1++] ='"';
		}
		if (tcpflag & TCP_SSL) {
			strcpy(buffer + i1, " encryption=y");
			i1 += (INT)strlen(buffer + i1);
		}
#if OS_WIN32GUI
		strcpy(buffer + i1, " gui=y");
		i1 += 6;
		/**
		 * Fix mostly for Jon E.
		 * 19 OCT 2012
		 * Since XP, MS has turned off the default visibility of mnemonics (access keys), those
		 * underscores that show for characters preceeded by an ampersand.
		 * They are supposed to show up when the ALT key is pressed. This is not working
		 * for us. Not known why,
		 * So the below call turns on the visibility all the time (just for us, not permanently).
		 */
		if (!SystemParametersInfo(SPI_SETKEYBOARDCUES, 0, &true, 0)) {
			sprintf(errstr, "SystemParametersInfo failed(SPI_SETKEYBOARDCUES) %d", (INT)GetLastError());
			message(errstr, "Internal fail");
			return terminate();
		}
#endif
		if (argbuf == NULL) {
			buffer[i1++] = '/';
			buffer[i1++] = '>';
		}
		else {
			buffer[i1++] = '>';
			strcpy(buffer + i1, argbuf);
			i1 += (INT)strlen(buffer + i1);
			strcpy(buffer + i1, "</start>");
			i1 += (INT)strlen(buffer + i1);
		}
		memset(buffer, ' ', 8);
		msciton(i1 - 16, (UCHAR *) buffer + 8, 8);
		if (tcpsend(tempsock, (UCHAR *) buffer, i1, TCP_UTF8, -1) <= 0) {
			sprintf(errstr, "Sending data to the server failed. Error: %s", tcpgeterror());
			message(errstr, "Socket Error");
			closesocket(tempsock);
			return terminate();
		}
		if (logflag == TRUE && xmllog != NULL) {
			fprintf(xmllog, "SND: ");
#if OS_WIN32
			writeTimeToLog();
#endif
			fprintf(xmllog, "%s\n", buffer + 16);
			fflush(xmllog);
		}
		pos = 0;
		for ( ; ; ) {
			i1 = tcprecv(tempsock, (UCHAR *) recvbuf + pos, sizeof(recvbuf) - 1 - pos, TCP_UTF8, 30);
			if (i1 <= 0) {
				sprintf(errstr, "Socket receive failed. Error: %s", tcpgeterror());
				message(errstr, "Socket Error");
				closesocket(tempsock);
				return terminate();
			}
			pos += i1;
			if (pos >= 16) {
				if (memcmp(recvbuf, buffer, 8)) {
					message("Serialization failure", "Socket Error");
					closesocket(tempsock);
					return terminate();
				}
				mscntoi((UCHAR *) recvbuf + 8, &len, 8);
				if (len <= 0 || len >= (INT) (sizeof(recvbuf) - 16)) {
					message("Corrupted packet", "Socket Error");
					closesocket(tempsock);
					return terminate();
				}
				if (pos >= 16 + len) break;
			}
		}
		closesocket(tempsock);
		if (logflag == TRUE && xmllog != NULL) {
			recvbuf[16 + len] = '\0';
			fprintf(xmllog, "RCV: ");
#if OS_WIN32
			writeTimeToLog();
#endif
			fprintf(xmllog, "%s\n", recvbuf + 16);
			fflush(xmllog);
		}
		i1 = xmlparse(recvbuf + 16, len, buffer, BUFFER_SIZE * sizeof(CHAR));
		if (i1) {
			recvbuf[16 + len] = '\0';
			sprintf(errstr, "XML Parsing failed: %s.  XML was: %s", xmlgeterror(), recvbuf + 16);
			message(errstr, "Client");
			return terminate();
		}
		eptr1 = (ELEMENT *) buffer;
		if (eptr1 == NULL || strcmp(eptr1->tag, "ok")) {
			if (eptr1 != NULL && eptr1->firstsubelement != NULL && eptr1->firstsubelement->cdataflag)
				message(eptr1->firstsubelement->tag, "Server Error");
			else message("Unknown reason", "Server Error");
			return terminate();
		}
	}
	if (!localport) {
		if (eptr1->firstsubelement != NULL && eptr1->firstsubelement->cdataflag)
			serverport = atoi(eptr1->firstsubelement->tag);
		else serverport = 0;
		if (serverport <= 0) {
			message("Sub-server port number is invalid", "Server Error");
			return terminate();
		}
		/* connect final time to dbc */
		sock = tcpconnect(host, serverport, tcpflag | TCP_CLNT, NULL, 60);
	}
	else sock = tcpaccept(sock, tcpflag | TCP_CLNT, NULL, 60); // This is where Scott DeRousse's problem starts, I think.
	if (sock == INVALID_SOCKET) {
		sprintf(errstr, "Failure to connect to server sub-process: %s", tcpgeterror());
		message(errstr, "Socket Error");
		return terminate();
	}
	if (argbuf != NULL) free(argbuf);
	free(buffer);

	/* Send client version and UTC offset */
	a1.tag = "version";
	a1.cbTag = 8;
	a1.value = RELEASE;
	a1.nextattribute = &a2;
	a2.tag = "utcoffset";
	a2.cbTag = 10;
	a2.value = getUTCOffset();
	a2.nextattribute = NULL;
	e1.tag = "smartclient";
	e1.cdataflag = 0;
	e1.firstattribute = &a1;
	e1.nextelement = NULL;
	e1.firstsubelement = NULL;
	memset(serial, ' ', sizeof(serial));
	if (sendelement(&e1, TRUE) < 0) {
		sprintf(errstr, "Sending greeting to the server failed. Error: %s", tcpgeterror());
		message(errstr, "Socket Error");
		return terminate();
	}

	/* Create Receive Thread */
#if OS_WIN32
	if ((threadevent = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL ||
		(threadhandle = CreateThread(NULL, 0, winthreadstart, NULL, 0, &threadid)) == NULL) {
#elif OS_UNIX
	if (evtdevinit(sock, EVENTS_READ | EVENTS_ERROR, unixrecvcb, (void *) (ULONG) sock) < 0) {
#endif
		sprintf(errstr, "Creation of processing thread failed. Error: %d", ERRORVALUE());
		message(errstr, "Thread Creation Failure");
		return 0;
	}

	thrflag = TRUE;

	if (server_major_version >= 14) {
		/* Get the current time in a timestamp format YYYYMMDDHHMMSS */
		timgettime(&timstruct);
		timstructToString(&timstruct, ka_timstamp, sizeof(ka_timstamp));

		/* add the keep alive interval to it. */
		timadd((UCHAR *)ka_timstamp, keepalivetime);

		/* start the timer */
		if (timset((UCHAR *)ka_timstamp, maineventids[EVENT_KATIMER]) == -1) {
			sprintf(errstr, "Could not create keep alive timer");
			message(errstr, "Fatal Internal Error");
			return terminate();
		}
	}

	for (i2 = 0; !abtflag; ) {
		maineventids[maineventidcount] = recvevent;
		for (i1 = 0; i1 <= maineventidcount; i1++) {
 			if (evttest(maineventids[i2]) == 1) break;
			if (++i2 > maineventidcount) i2 = 0;
		}
		if (i1 > maineventidcount) {
			evtwait(maineventids, maineventidcount + 1);
			continue;
		}
		if (i2 != maineventidcount) evtclear(maineventids[i2]);  /* clear any main event except recvevent */

		if (i2 == EVENT_BREAK) {
			e1.tag = "break";
			e1.cdataflag = 0;
			e1.firstattribute = NULL;
			e1.nextelement = NULL;
			e1.firstsubelement = NULL;
			i1 = sendelement(&e1, FALSE);
			if (i1 < 0) {
				sprintf(errstr, "Send break message failure (Communication to server)");
				message(errstr, "Fatal Internal Error");
				return terminate();
			}
		}
		else if (i2 == EVENT_TRAPCHAR) {
			vidtrapend(&i1);
			vidtrapstart(trapcharmap, maineventids[EVENT_TRAPCHAR]);
			/* notify server of trap */
			e1.tag = "t";
			e1.cdataflag = 0;
			e1.firstattribute = NULL;
			e1.nextelement = NULL;
			e1.firstsubelement = &e2;
			e2.cdataflag = mscitoa(i1, work);
			e2.tag = work;
			e2.firstattribute = NULL;
			e2.nextelement = NULL;
			e2.firstsubelement = NULL;
			i1 = sendelement(&e1, FALSE);
			if (i1 < 0) {
				sprintf(errstr, "Send break message failure (Communication to server)");
				message(errstr, "Fatal Internal Error");
				return terminate();
			}
		}
		else if (i2 == EVENT_SHUTDOWN) {
			/* should never get here because abtflag is */
			/* true whenever EVENT_SHUTDOWN is set */
			break;
		}
		else if (i2 == EVENT_KATIMER) {
			if (sendAliveChk() == -1) break;
		}
		else if (i2 == EVENT_CANCEL) {
		}
		else if (i2 == maineventidcount) { /* XML data received from server */
			evtclear(maineventids[EVENT_CANCEL]); /* make sure this is clear */
			if (processrecv() == -1) {
				break;
			}
		}
	}
	terminate();
	return 0;
}

static int sendAliveChk() {
	ELEMENT e1;
	INT i1;

	e1.tag = "alivechk";
	e1.cdataflag = 0;
	e1.firstattribute = NULL;
	e1.nextelement = NULL;
	e1.firstsubelement = NULL;
	i1 = sendelement(&e1, FALSE);
	if (i1 < 0) {
		sprintf(errstr, "Send keepalive message failure (Communication to server)");
		message(errstr, "Fatal Internal Error");
		return RC_ERROR;
	}
	timadd((UCHAR *)ka_timstamp, keepalivetime);
	timset((UCHAR *)ka_timstamp, maineventids[EVENT_KATIMER]);
	return 0;
}


static void usage()
{
#if OS_WIN32
	char buffer[512];
#endif
	if (cnsflag) {

			fprintf(stderr, "SMART CLIENT release %s%s\n", RELEASE, COPYRIGHT);
			fprintf(stderr, "Usage:  ");
#if OS_UNIX
			fprintf(stderr, "dbcsc");
#else
			fprintf(stderr, "dbcscc");
#endif
/*                 Usage  : dbcscc56789_123456789_123456789_123456789_123456789_123456789_123456789 */
		fprintf(stderr, " host [-hostport=port] [-localport=port] [-t=tdb file]\n");
		fprintf(stderr, "\t[-encrypt=<on|off>] [-user=username] [-curdir=directory]\n");
		fprintf(stderr, "\t-sslstrength=[low | high]\n");
		fprintf(stderr, "\t[-a=n] [-lp] [-pl] [DX runtime parameters]\n");
		fflush(stderr);
	}
#if OS_WIN32
	else {
		snprintf(buffer, sizeof(buffer), "SMART CLIENT release %s%s\nUsage:  dbcsc host [-hostport=port] [-localport=port]"
		" [-t=tdb file]\n\t[-encrypt=<on|off>] [-user=username] [-curdir=directory]\n"
		"\t-sslstrength=[low | high]\n"
		"\t[-a=n] [-lp] [-pl] [DX runtime parameters]\n", RELEASE, COPYRIGHT);
		message(buffer, "About SMART CLIENT");
	}
#endif
}

#if OS_WIN32
static DWORD WINAPI winthreadstart(LPVOID unused)
{
	int i1;
	for ( ; ; ) {
		if ((i1 = recvelement()) < 0) break;
	}
	abtflag = TRUE;	/* signal application termination */
	evtset(maineventids[EVENT_SHUTDOWN]); /* allows test for abtflag in main loop */
	ExitThread(0);
	return 0;
}

/**
 * Set the 'current' font state to the 'initial' state
 * Shound be used at the start of a prep and a draw verb
 */
static void initializeFontParseControl(INT isDraw) {
	guiFontParseControl.currentBold = guiFontParseControl.initialBold;
	guiFontParseControl.currentItalic = guiFontParseControl.initialItalic;
	guiFontParseControl.currentUnderline = guiFontParseControl.initialUnderline;
	guiFontParseControl.currentSize =
			isDraw ? guiFontParseControl.initialDrawSize : guiFontParseControl.initialSize;
	StringCbCopy(guiFontParseControl.currentFontName, sizeof(guiFontParseControl.currentFontName),
			guiFontParseControl.initialFontName);
}

#endif


#if OS_UNIX
static INT unixrecvcb(void *handle, INT polltype)
{
	INT i1;

	if (polltype & EVENTS_READ) {
		i1 = recvelement();
	}
	if (i1 < 0 || polltype & EVENTS_ERROR) {
		abtflag = TRUE;
		evtset(maineventids[EVENT_SHUTDOWN]);
		return 1;
	}
	return 0;
}
#endif

static INT recvelement()
{
	static INT bufsize, bufpos = 0;
	static CHAR *bufptr, recvbuf[4096];
	INT i1, len;
	CHAR *socketbuf;
	SOCKETBUF *msgqueueptr, *msgqueuetmp;

	msgqueuetmp = NULL;
	for (len = 0; ; ) {
		if (!len) {
			if (bufpos >= 16) {
				bufpos -= 16;
				tcpntoi((UCHAR *) bufptr + 8, 8, &len);
/*** CODE: IF tcpntoi RETURNS -1 OR LENGTH IS INVALID, PROBABLY SHOULD FLUSH ALL INCOMING DATA AND FLAG ERROR ***/
				if (!len) {  /* should not happen */
					if (bufpos) {
						bufptr += 16;
						bufsize -= 16;
					}
					continue;
				}
				msgqueuetmp = (SOCKETBUF *) malloc(sizeof(SOCKETBUF) + len);
				if (msgqueuetmp == NULL) {
					message("Memory allocation failed!", "Fatal Error");
					return RC_NO_MEM;
				}
				msgqueuetmp->socketBufSize = len;
				msgqueuetmp->flags = 0;
				msgqueuetmp->next = NULL;
				memcpy(msgqueuetmp->serial, bufptr, 8);
				socketbuf = msgqueuetmp->socketBuf;
				if (bufpos) {
					i1 = (bufpos > len) ? len : bufpos;
					memcpy(socketbuf, bufptr + 16, i1);
					if (bufpos >= len) {  /* have all of the data */
						bufpos -= len;
						if (bufpos) {
							bufptr += len + 16;
							bufsize -= len + 16;
						}
						break;
					}
				}
				bufptr = socketbuf;
				bufsize = len;
			}
			else if (!bufpos || bufptr != recvbuf) {
				if (bufpos) memmove(recvbuf, bufptr, bufpos);
				bufptr = recvbuf;
				bufsize = sizeof(recvbuf);
			}
		}
		else if (bufpos >= bufsize) {  /* done getting data, flag restoration of static variables */
			bufpos = 0;
			break;
		}
		i1 = tcprecv(sock, (UCHAR *) bufptr + bufpos, bufsize - bufpos, tcpflag, -1);
		if (i1 <= 0) {
			free(msgqueuetmp);
			bufpos = 0;
			return RC_ERROR;
		}
		bufpos += i1;
	}
	socketbuf[len] = '\0';  /* put NULL character at the end */

	if (len == 9 && !strcmp(socketbuf, "<cancel/>")) {
#if OS_WIN32
		pvistart();
#endif
		msgqueueptr = msgqueuehead;
		while (msgqueueptr != NULL) {  /* flag unprocessed messages as canceled */
			msgqueueptr->flags |= KEYIN_CANCEL;
			msgqueueptr = msgqueueptr->next;
		}
#if OS_WIN32
		pviend();
#endif
		evtset(maineventids[EVENT_CANCEL]);
		free(msgqueuetmp);
	}
	else {
#if OS_WIN32
		pvistart();
#endif
		if (msgqueuehead == NULL) msgqueuehead = msgqueuetmp;
		else {
			msgqueueptr = msgqueuehead;
			while (msgqueueptr->next != NULL) msgqueueptr = msgqueueptr->next;
			msgqueueptr->next = msgqueuetmp;
		}
		if (!evttest(recvevent)) {
			/* notify main loop that data has been received */
			evtset(recvevent);
#if OS_UNIX
			/* allow processing of data before reading new data */
			evtdevset(sock, 0);
#endif
		}
#if OS_WIN32
		pviend();
#endif
	}
	return 1;
}

static INT processrecv()
{
	static CHAR *parseBuf;
	static INT bufsize = 4096 * 4;
	INT i1, count, flags;
	CHAR *newbuf;
	SOCKETBUF *msgqueueptr;

	if (parseBuf == NULL) {
		parseBuf = (CHAR *) malloc(bufsize);
		if (parseBuf == NULL) {
			message("Memory allocation failed!", "Fatal Error");
			return RC_NO_MEM;
		}
	}

	for (count = 0; ; ) {
#if OS_WIN32
		pvistart();
#endif
		if (msgqueuehead == NULL) {
			evtclear(recvevent);
#if OS_UNIX
			/* allow recvelements to be called */
			evtdevset(sock, EVENTS_READ | EVENTS_ERROR);
#endif
#if OS_WIN32
			pviend();
#endif
			break;
		}
		msgqueueptr = msgqueuehead;
		msgqueuehead = msgqueueptr->next;
#if OS_WIN32
		pviend();
#endif
		flags = msgqueueptr->flags;
		memcpy(serial, msgqueueptr->serial, 8);

		for ( ; ; ) {
			i1 = xmlparse(msgqueueptr->socketBuf, msgqueueptr->socketBufSize, parseBuf, bufsize);
			if (i1 == -1) { /* parseBuf too small */
				newbuf = realloc(parseBuf, bufsize << 1);
				if (newbuf == NULL) {
					message("Memory allocation failed!", "Fatal Error");
					free(msgqueueptr);
					return RC_ERROR;
				}
				parseBuf = newbuf;
				bufsize <<= 1;
				continue;
			}
			if (logflag == TRUE && xmllog != NULL) {
				fprintf(xmllog, "RCV: ");
#if OS_WIN32
				writeTimeToLog();
#endif
				fprintf(xmllog, "%s\n", msgqueueptr->socketBuf);
				fflush(xmllog);
			}
			if (i1 != 0) {
				sprintf(errstr, "XML Parsing failed: %s.  XML was: %s", xmlgeterror(),  msgqueueptr->socketBuf);
				message(errstr, "Client");
				free(msgqueueptr);
				return RC_ERROR;
			}
			free(msgqueueptr);
			break;
		}
		if (parsecommand((ELEMENT *) parseBuf, flags) < 0) return RC_ERROR;
		if (!(++count & 0x03)) {  /* check for other events */
			for (i1 = 0; i1 < maineventidcount; i1++)
 				if (evttest(maineventids[i1])) return 1;
		}
	}
	return 0;
}

#if OS_WIN32
static void writeTimeToLog() {

	SYSTEMTIME systime;
	CHAR work[16];

	GetLocalTime(&systime);
	work[0] = systime.wSecond / 10 + '0';
	work[1] = systime.wSecond % 10 + '0';
	work[2] = '.';
	work[3] = systime.wMilliseconds / 100 + '0';
	work[4] = (systime.wMilliseconds % 100) / 10 + '0';
	work[5] = systime.wMilliseconds % 10 + '0';
	work[6] = '\0';
	fprintf(xmllog, "[%s] ", work);
}
#endif

/*
 * Always returns one
 */
static INT terminate()
{


	/* Finish up */
	if (logflag == TRUE && xmllog != NULL) {
		fflush(xmllog);
		fclose(xmllog);
	}
#if OS_WIN32
	timexit();
	if (thrflag) {
		TerminateThread(threadhandle, 0);  // DANGEROUS!
		WaitForSingleObject(threadhandle, 500);
		CloseHandle(threadhandle);
	}
#endif
	if (conflag) {
		if (sock != INVALID_SOCKET) {
#if OS_UNIX
			if (thrflag) evtdevexit(sock);
#endif
			if (tcpflag & TCP_SSL) {
				tcpsslcomplete(sock);
			}
			closesocket(sock);
		}
#if OS_WIN32
		WSACleanup();
#endif
	}
	if (vidflag) {
		videxit(); /* close console window */
	}
#if OS_WIN32
	if (guiflag) {
		guiexit(0);
	}
#endif
	if (prtflag) {
/*** NEED CODE ***/
	}
	return 1;
}

static CHAR *genarglist(CHAR *newtext)
{
	INT i1, i2, i3;
	CHAR *temp;
	static INT bufsize = 0;

	if (!bufsize) {
		argbuf = (CHAR *) malloc(sizeof(CHAR) * 32);
		if (argbuf == NULL) {
			message("malloc failure", "genarglist");
			return NULL;
		}
		bufsize = 32;
		i1 = 0;
	}
	else i1 = (INT)strlen(argbuf);
	i2 = (INT)strlen(newtext);
	i3 = i1 + i2;
	if (i3 > bufsize - 1) {
		temp = (CHAR *) realloc(argbuf, sizeof(CHAR) * (bufsize << 1));
		if (temp == NULL) {
			message("realloc failure", "genarglist");
			return NULL;
		}
		argbuf = temp;
		bufsize <<= 1;
	}
	for (i2 = 0; i1 < i3; i1++, i2++) argbuf[i1] = newtext[i2];
	argbuf[i3] = '\0';
	return argbuf;
}

static void message(CHAR *text, CHAR *title)
{
	if (cnsflag) {
		fprintf(stderr, "\n%s: %s\n", title, text);
		fflush(stderr);
	}
#if OS_WIN32
	else MessageBox(NULL, text, title, MB_OK);
#endif
}

#if OS_WIN32
static void lrtrim(CHAR *dest, UCHAR *src, INT len)
{
	INT i1, i2;

	for (i1 = i2 = 0; i1 < len; i1++) {
		if (src[i1] != ' ') break;
	}
	for (; i1 < len; i1++) {
		if (src[i1] == ' ') break;
		else dest[i2++] = src[i1];
	}
	dest[i2] = '\0';
}
#endif

static INT sendelement(ELEMENT *element, INT quote)
{
	static INT bufsize = 0;
	static CHAR *buf;
	INT len1;
	CHAR *temp;
#if OS_WIN32
	pvistart();
#endif

	if (!bufsize) {
		buf = (CHAR *) malloc(4096);
		if (buf == NULL) {
			message("Memory allocation failed!", "Fatal Error");
#if OS_WIN32
			pviend();
#endif
			return RC_NO_MEM;
		}
		bufsize = 4096;
	}
	for ( ; ; ) {
		len1 = xmlflatten(element, !quote, buf + 16, (bufsize - 17));
		if (len1 > 0) break;
		if (len1 != -1) {
			message("xmlflatten() failed!", "Fatal Error");
#if OS_WIN32
			pviend();
#endif
			return RC_ERROR;
		}
		temp = (CHAR *) realloc(buf, (bufsize << 1) * sizeof(CHAR));
		if (temp == NULL) {
			message("Memory allocation failed!", "Fatal Error");
#if OS_WIN32
			pviend();
#endif
			return RC_NO_MEM;
		}
		buf = temp;
		bufsize <<= 1;
	}
	memcpy(buf, serial, 8);
	msciton(len1, (UCHAR *) buf + 8, 8);
	if (tcpsend(sock, (UCHAR *) buf, 16 + len1, tcpflag, -1) <= 0) {
#if OS_WIN32
		pviend();
#endif
		return RC_ERROR;
	}
	if (logflag == TRUE && xmllog != NULL) {
		buf[16 + len1] = '\0';
		fprintf(xmllog, "SND: ");
#if OS_WIN32
		writeTimeToLog();
#endif
		fprintf(xmllog, "%s\n", buf + 16);
		fflush(xmllog);
	}
#if OS_WIN32
	pviend();
#endif
	return 0;
}

#if OS_WIN32
static INT sendimageelement(ELEMENT *element, INT imagesize)
{
	INT len1, bufsize;
	CHAR *buf, *newbuf;

	pvistart();
	bufsize = 500 + imagesize;
	buf = (CHAR *) malloc(bufsize * sizeof(CHAR));
	if (buf == NULL) {
		message("Memory allocation failed!", "Fatal Error");
		pviend();
		return RC_NO_MEM;
	}
	for ( ; ; ) {
		if ((len1 = xmlflatten(element, TRUE, buf + 16, (bufsize - 17) * sizeof(CHAR))) > 0) break;
		if (len1 != -1) {
			pviend();
			return RC_ERROR;
		}
		bufsize += bufsize;
		newbuf = (CHAR *) realloc(buf, bufsize * sizeof(CHAR));
		if (newbuf == NULL) {
			message("Memory allocation failed!", "Fatal Error");
			free(buf);
			pviend();
			return RC_NO_MEM;
		}
		buf = newbuf;
	}
	memcpy(buf, serial, 8);
	msciton(len1, (UCHAR *) buf + 8, 8);
	if (tcpsend(sock, (UCHAR *) buf, 16 + len1, tcpflag, -1) <= 0) {
		free(buf);
		pviend();
		return RC_ERROR;
	}
	if (logflag == TRUE && xmllog != NULL) {
		buf[16 + len1] = '\0';
		fprintf(xmllog, "SND: %s\n", buf);
		fflush(xmllog);
	}
	free(buf);
	pviend();
	return 0;
}
#endif

static INT sendresult(INT i1)
{
	ATTRIBUTE a1;
	ELEMENT e1;

	memset(&e1, 0, sizeof(ELEMENT));
	e1.tag = "r";
	if (i1 > 0) {
		a1.tag = "e";
		a1.nextattribute = NULL;
		a1.value = "0";
		e1.firstattribute = &a1;
	}
	return sendelement(&e1, TRUE);
}

/**
 * Bits are base-64 encoded
 * Calls memalloc, might move memory
 */
static INT clientGetFileWrite(CHAR *csfname, UCHAR *bits, UINT moretocome) {
	enum states {
		IDLE,
		BUSY
	};
	static enum states state = IDLE;
	static INT filehandle;
	static OFFSET offset;
	INT i1, inputLen, bytesToWrite;
	CHAR fname[MAX_PATH];
	UCHAR **binBuff;
#if OS_WIN32
	CHAR *slash = "\\";
#else
	CHAR *slash = "/";
#endif

	errstr[0] = '\0';

	inputLen = (INT)strlen((const char *)bits);

	/*
	 * If we are entered in an idle state, set up file name and open it
	 */
	if (state == IDLE) {
		strcpy(fname, fileTransferLocaldir);
		i1 = (INT) strlen(fname);
		if (fname[i1 - 1] != slash[0]) strcat(fname, slash);
		strcat(fname, csfname);
		filehandle = fioopen(fname, FIO_M_PRP);
		if (filehandle < 0) {
			sprintf(errstr, "fio error on open in clientGetFileWrite - '%s'", fioerrstr(filehandle));
			return XFERERR_FIO;
		}
		if (inputLen == 0) { /* Zero length file! */
			fioclose(filehandle);
			return 0;
		}
		offset = 0;
	}

	/* allocate buffer to hold bits converted back to binary */
	i1 = (((inputLen >> 2) + 1) * 3 + 32);
	binBuff = memalloc(i1, 0);
	if (binBuff == NULL) {
		sprintf(errstr, "memalloc fail in clientGetFileWrite (%d)", i1);
		return XFERERR_NOMEM;
	}
	/* convert from encoded to binary */
	fromprintable(bits, inputLen, *binBuff, &bytesToWrite);

	/* write to file */
	i1 = fiowrite(filehandle, offset, *binBuff, bytesToWrite);
	if (i1 < 0) {
		sprintf(errstr, "fio error on write in clientGetFileWrite - '%s'", fioerrstr(i1));
		return XFERERR_FIO;
	}
	memfree(binBuff);

	if (!moretocome) {
		fioclose(filehandle);
		state = IDLE;
	}
	else {
		state = BUSY;
		offset += bytesToWrite;
	}
	return 0;
}

/*
 * Putting a file to the server
 */
static INT clientPutFile(ELEMENT* element) {
	enum states {
		IDLE,
		BUSY
	};
	static enum states state = IDLE;
	static INT sourceFileHandle;
	static OFFSET fileSize, bytesRemaining, offset;
	static INT encodeBufferSize = (chunkSize / 3 + 1) * 4 + 128 /* for good luck */;
	static UCHAR **encodedFileBuffer = NULL;
	static UCHAR **fileBuffer;
	ATTRIBUTE a1, a2;
	ELEMENT e1;
	INT i1;
	UINT bytesToRead, encodedDataSize;

#if OS_WIN32
	CHAR *slash = "\\";
#else
	CHAR *slash = "/";
#endif

	errstr[0] = '\0';
	e1.tag = "r";
	e1.cdataflag = 0;
	e1.firstsubelement = NULL;
	e1.nextelement = NULL;
	e1.firstattribute = &a1;
	a1.tag = FILETRANSFERBITSTAG;
	a1.nextattribute = &a2;

	a2.tag = FILETRANSFERMORETOCOMETAG;
	a2.nextattribute = NULL;

	/*
	 * The buffer to hold the base-64 data converted from binary is always the
	 * same size, so allocate it once and keep it.
	 */
	if (encodedFileBuffer == NULL) {
		encodedFileBuffer = memalloc((INT)encodeBufferSize, 0);
		if (encodedFileBuffer == NULL) {
			sprintf(errstr, "memalloc fail in %s",
#ifndef NO__FUNCTION__
				__FUNCTION__);
#else
				"clientPutFile");
#endif
			return sendClientPutFileError(XFERERR_NOMEM, encodeBufferSize);
		}
	}

	if (state == IDLE) {
		CHAR *csfnameAttrVal;
		ATTRIBUTE *pa1;
		CHAR sourceFileName[MAX_PATH];

		/* First contact, open the file */
		csfnameAttrVal = NULL;
		pa1 = element->firstattribute;
		while (pa1 != NULL) {
			if (strcmp(pa1->tag, FILETRANSFERCLIENTSIDEFILENAMETAG) == 0) {
				csfnameAttrVal = pa1->value;
				break;
			}
			pa1 = pa1->nextattribute;
		}
		if (csfnameAttrVal == NULL) {
			strcpy(errstr, "Missing clientside filename in XML");
			return sendClientPutFileError(XFERERR_INTERNAL, 0);
		}
		strcpy(sourceFileName, fileTransferLocaldir);
		i1 = (INT) strlen(sourceFileName);
		if (sourceFileName[i1 - 1] != slash[0]) strcat(sourceFileName, slash);
		strcat(sourceFileName, csfnameAttrVal);
		sourceFileHandle = fioopen(sourceFileName, FIO_M_SRO);
		if (sourceFileHandle < 0) {
			sprintf(errstr, "fio error on open in %s",
#ifndef NO__FUNCTION__
				__FUNCTION__);
#else
				"clientPutFile");
#endif
			return sendClientPutFileError(XFERERR_FIO, sourceFileHandle);
		}
		i1 = fiogetsize(sourceFileHandle, &fileSize);
		if (i1 < 0) {
			sprintf(errstr, "fio error on getsize in %s",
#ifndef NO__FUNCTION__
				__FUNCTION__);
#else
				"clientPutFile");
#endif
			return sendClientPutFileError(XFERERR_FIO, i1);
		}

		if (fileSize > 0) {
			/* Allocate the file buffer */
			bytesRemaining = fileSize;
			bytesToRead = ((INT)fileSize <= chunkSize) ? (INT)fileSize : chunkSize;
			fileBuffer = memalloc(bytesToRead, 0);
			if (fileBuffer == NULL) {
				sprintf(errstr, "memalloc fail in %s",
#ifndef NO__FUNCTION__
				__FUNCTION__);
#else
				"clientPutFile");
#endif
				return sendClientPutFileError(XFERERR_NOMEM, (INT)fileSize);
			}

			/* Read in the first chunk, or maybe all of it if it is small enough */
			i1 = fioread(sourceFileHandle, 0, *fileBuffer, bytesToRead);
			if (i1 < 0) {
				sprintf(errstr, "fio error on read in %s",
#ifndef NO__FUNCTION__
				__FUNCTION__);
#else
				"clientPutFile");
#endif
				return sendClientPutFileError(XFERERR_FIO, i1);
			}
			bytesRemaining -= bytesToRead;

			/* Encode it */
			makeprintable(*fileBuffer, (INT)bytesToRead, *encodedFileBuffer, (INT*)&encodedDataSize);
			(*encodedFileBuffer)[encodedDataSize] = 0;

			/* Do we need to fragment this? */
			if (bytesRemaining < 1) /* NO */ {
				fioclose(sourceFileHandle);
				memfree(fileBuffer);
				a2.value = "N";
			}
			else {
				a2.value = "Y";
				state = BUSY;
				offset = bytesToRead;
			}
			a1.value = (CHAR*)*encodedFileBuffer;
		}
		else {  /* zero length file! */
			a1.value = "";
			fioclose(sourceFileHandle);
			a2.value = "N";
		}
	}
	else {
		/*
		 * File buffer is still allocated and has to be chunkSize
		 * File handle is still open
		 */
		bytesToRead = ((INT)bytesRemaining <= chunkSize) ? (INT)bytesRemaining : chunkSize;

		/* Read in the next chunk */
		i1 = fioread(sourceFileHandle, offset, *fileBuffer, bytesToRead);
		if (i1 < 0) {
			sprintf(errstr, "fio error on read in %s",
#ifndef NO__FUNCTION__
				__FUNCTION__);
#else
				"clientPutFile");
#endif
			return sendClientPutFileError(XFERERR_FIO, i1);
		}
		bytesRemaining -= bytesToRead;
		offset += bytesToRead;

		/* Encode it */
		makeprintable(*fileBuffer, (INT)bytesToRead, *encodedFileBuffer, (INT*)&encodedDataSize);
		(*encodedFileBuffer)[encodedDataSize] = 0;

		/* Is there more? */
		if (bytesRemaining < 1) /* NO */ {
			fioclose(sourceFileHandle);
			memfree(fileBuffer);
			a2.value = "N";
			state = IDLE;
		}
		else a2.value = "Y";
		a1.value = (CHAR*)*encodedFileBuffer;
	}
	/* Send it */
	i1 = sendelement(&e1, TRUE);
	if (i1 < 0) return -4000;
	return 0;
}

static INT sendClientPutFileError(INT xferErrCode, INT extraErrCode) {
	ELEMENT e1, e2;
	ATTRIBUTE a1, a2;
	CHAR work[128];
	INT i1;

	e1.tag = "r";
	e1.cdataflag = 0;
	e1.firstattribute = &a1;
	e1.nextelement = NULL;

	a1.tag = "e";
	a1.nextattribute = NULL;
	ConvertFileXferErrorCodeToString(xferErrCode, &a1);
	a2.tag = "xtra";
	a2.nextattribute = NULL;
	switch (xferErrCode) {
	case XFERERR_FIO:
		sprintf(work, "%d", extraErrCode);
		a2.value = work;
		a1.nextattribute = &a2;
		break;
	case XFERERR_NOMEM:
		sprintf(work, "%d", extraErrCode);
		a2.value = work;
		a1.nextattribute = &a2;
		break;
	}

	if (errstr[0] == '\0') e1.firstsubelement = NULL;
	else {
		e1.firstsubelement = &e2;
		e2.cdataflag = (INT) strlen(errstr);
		e2.tag = errstr;
		e2.firstsubelement = NULL;
		e2.nextelement = NULL;
		e2.firstattribute = NULL;
	}
	i1 = sendelement(&e1, FALSE);
	if (i1 < 0) return -4000;
	return 0;
}

/*
 * Getting a file from the server
 */
static INT clientGetFile(ELEMENT* element) {
	ATTRIBUTE *a1;
	UCHAR *bits;
	CHAR *csfname, *more;

	errstr[0] = '\0';
	if (fileTransferLocaldir[0] == '\0') {
		strcpy(errstr, "Missing local dir def in SC");
		return XFERERR_NOLOCALDIR;
	}
	a1 = element->firstattribute;
	bits = NULL;
	csfname = more = NULL;
	while (a1 != NULL) {
		if (strcmp(a1->tag, FILETRANSFERBITSTAG) == 0) bits = (UCHAR*)a1->value;
		else if (strcmp(a1->tag, FILETRANSFERCLIENTSIDEFILENAMETAG) == 0) csfname = a1->value;
		else if (strcmp(a1->tag, FILETRANSFERMORETOCOMETAG) == 0) more = a1->value;
		a1 = a1->nextattribute;
	}
	if (csfname == NULL) {
		strcpy(errstr, "Missing clientside filename in XML");
		return XFERERR_INTERNAL;
	}
	if (more == NULL) {
		strcpy(errstr, "Missing more-to-come attribute in XML");
		return XFERERR_INTERNAL;
	}
	if (bits == NULL) {
		strcpy(errstr, "Missing bits attribute in XML");
		return XFERERR_INTERNAL;
	}
	return clientGetFileWrite(csfname, bits, (more[0] == 'Y'));
}

/**
 * For clientgetfile, a special results function
 * i1 = 0 for no error, positive value for error code
 */
static INT sendClientGetFileResult(INT i1) {
	ELEMENT e1, e2;
	ATTRIBUTE a1;
#if defined(Linux)
	static CHAR errorCode[8] __attribute__ ((unused));
#else
	static CHAR errorCode[8];
#endif

	e1.tag = "r";
	e1.cdataflag = 0;
	e1.firstsubelement = NULL;
	e1.nextelement = NULL;

	if (!i1) {
		e1.firstattribute = NULL;
		i1 = sendelement(&e1, FALSE);
		if (i1 < 0) return -4000;
		return 0;
	}

	a1.tag = "e";
	a1.nextattribute = NULL;

	ConvertFileXferErrorCodeToString(i1, &a1);
	i1 = (INT)strlen(errstr);
	if (i1 > 0) {
		e2.tag = errstr;
		e2.cdataflag = i1;
		e2.firstsubelement = NULL;
		e2.nextelement = NULL;
		e2.firstattribute = NULL;
		e1.firstsubelement = &e2;
	}
	else e1.firstsubelement = NULL;
	e1.firstattribute = &a1;
	i1 = sendelement(&e1, TRUE);
	if (i1 < 0) return -4000;
	return 0;
}

/**
 * Sets the Attribute's value to a string corresponding to the error code
 */
static void ConvertFileXferErrorCodeToString(INT err, ATTRIBUTE *a1) {
	INT i2;
	CHAR errorCode[8];
	for (i2 = 0; (UINT)i2 < ARRAYSIZE(filexfererrortable); i2++) {
		if (err == (INT)filexfererrortable[i2].code) {
			a1->value = filexfererrortable[i2].stringValue;
			break;
		}
	}
	/*
	 * If we exhausted the table and could not match the code, just send the code itself
	 */
	if (i2 == ARRAYSIZE(filexfererrortable)) {
		mscitoa(err, errorCode);
		a1->value = errorCode;
	}
}

static INT sendprintresult(INT i1)
{
	CHAR errorstr[256];
	ATTRIBUTE a1;
	ELEMENT e1, e2;

	e1.tag = "r";
	e1.cdataflag = 0;
	e1.nextelement = NULL;

	if (!i1) {
		e1.firstsubelement = NULL;
		e1.firstattribute = NULL;
		i1 = sendelement(&e1, FALSE);
		if (i1 < 0) return -4000;
		return 0;
	}

	a1.tag = "e";
	a1.nextattribute = NULL;

	if (i1 == PRTERR_INUSE)	a1.value = "inuse";
	else if (i1 == PRTERR_EXIST) a1.value = "exist";
	else if (i1 == PRTERR_OFFLINE) a1.value = "offline";
	else if (i1 == PRTERR_CANCEL) a1.value = "cancel";
	else if (i1 == PRTERR_BADNAME) a1.value = "badname";
	else if (i1 == PRTERR_BADOPT) a1.value = "badopt";
	else if (i1 == PRTERR_NOTOPEN) a1.value = "notopen";
	else if (i1 == PRTERR_OPEN)	a1.value = "open";
	else if (i1 == PRTERR_CREATE) a1.value = "create";
	else if (i1 == PRTERR_ACCESS)	a1.value = "access";
	else if (i1 == PRTERR_WRITE) a1.value = "write";
	else if (i1 == PRTERR_NOSPACE) a1.value = "nospace";
	else if (i1 == PRTERR_NOMEM) a1.value = "nomem";
	else if (i1 == PRTERR_SERVER) a1.value = "server";
	else if (i1 == PRTERR_DIALOG)	a1.value = "dialog";
	else if (i1 == PRTERR_NATIVE)	a1.value = "native";
	else if (i1 == PRTERR_BADTRANS) a1.value = "badtrans";
	else if (i1 == PRTERR_TOOLONG) a1.value = "toolong";
	else if (i1 == PRTERR_SPLCLOSE) a1.value = "splclose";
	else a1.value = "client";

	errorstr[0] = '\0';
	if (prop_print_error != NULL && !strcmp(prop_print_error, "on")) {
		prtgeterror(errorstr, sizeof(errorstr));
	}
	i1 = (INT)strlen(errorstr);
	if (i1 > 0) {
		e2.tag = errorstr;
		e2.cdataflag = i1;
		e2.firstsubelement = NULL;
		e2.nextelement = NULL;
		e2.firstattribute = NULL;
		e1.firstsubelement = &e2;
	}
	else e1.firstsubelement = NULL;
	e1.firstattribute = &a1;
	i1 = sendelement(&e1, TRUE);
	if (i1 < 0) return -4000;
	return 0;
}

static INT parsecommand(ELEMENT *element, INT flags)
{
	INT error_code, i1;
#if OS_WIN32
	INT i2, i3;
	ATTRIBUTE *a1;
#endif
	CHAR error[2 * 8192], buf[12224], type[64], work[64];
	UCHAR kbdfmap[(VID_MAXKEYVAL >> 3) + 1];
	ELEMENT *e1, *e2, *e3, *e4;
	ELEMENT sync;

	sync.tag = "s";
	sync.cdataflag = 0;
	sync.firstattribute = NULL;
	sync.nextelement = NULL;
	sync.firstsubelement = NULL;

	while (element != NULL) {

		error_code = 0;

		/*** printing tags follow: ***/

		if (strcmp(element->tag, "prtopen") == 0) {
			error_code = doprtopen(element);
			if (error_code < 0) strcpy(type, "prtopen");
		}
		else if (strcmp(element->tag, "prtclose") == 0) {
			error_code = doprtclose(element);
			if (error_code < 0) strcpy(type, "prtclose");
		}
		else if (strcmp(element->tag, "prttext") == 0) {
			error_code = doprttext(element);
			if (error_code < 0) strcpy(type, "prttext");
		}
		else if (strcmp(element->tag, "prtnoej") == 0) {
			error_code = doprtnoej(element);
			if (error_code < 0) strcpy(type, "prtnoej");
		}
		else if (strcmp(element->tag, "prtrel") == 0) {
			error_code = doprtrel(element);
			if (error_code < 0) strcpy(type, "prtrel");
		}
		else if (strcmp(element->tag, "prtsetup") == 0) {
			error_code = doprtsetup();
			if (error_code < 0) strcpy(type, "prtsetup");
		}
		else if (strcmp(element->tag, "prtexit") == 0) {
			error_code = doprtexit();
			if (error_code < 0) strcpy(type, "prtexit");
		}
		else if (strcmp(element->tag, "prtget") == 0) {
			error_code = doprtget(element, GETPRINTINFO_PRINTERS);
			if (error_code < 0) strcpy(type, "prtget");
		}
		else if (strcmp(element->tag, "prtbin") == 0) {
			error_code = doprtget(element, GETPRINTINFO_PAPERBINS);
			if (error_code < 0) strcpy(type, "prtbin");
		}
		else if (strcmp(element->tag, "prtpnames") == 0) {
			error_code = doprtget(element, GETPRINTINFO_PAPERNAMES);
			if (error_code < 0) strcpy(type, "prtpnames");
		}
		else if (strcmp(element->tag, "prtalloc") == 0) {
			error_code = doprtalloc(element);
			if (error_code < 0) strcpy(type, "prtalloc");
		}

		/*** rollout ***/

		else if (strcmp(element->tag, "rollout") == 0) {
			if ((e3 = element->firstsubelement) != NULL) {
				error_code = dorollout(e3->tag);
				if (error_code < 0) strcpy(type, "rollout");
			}
		}

		/*** WIN32 GUI tags follow: ***/

#if OS_WIN32
		// CODE For version 15, we will allow the prep of an image in dbcscc.exe
		// and drawing to it, and printing it.
		else if (strcmp(element->tag, "prep") == 0) {
			error_code = doprep(element);
			if (error_code < 0) StringCbCopy(type, sizeof(type), "prep");
		}
		else if (strcmp(element->tag,"draw") == 0) {
			error_code = dodraw(element);
			if (error_code < 0) StringCbCopy(type, sizeof(type), "draw");
		}
		else if (strcmp(element->tag, "show") == 0) {
			error_code = doshow(element);
			if (error_code < 0) StringCbCopy(type, sizeof(type), "show");
		}
		else if (strcmp(element->tag, "hide") == 0) {
			error_code = dohide(element);
			if (error_code < 0)  StringCbCopy(type, sizeof(type), "hide");
		}
		else if (strcmp(element->tag, "close") == 0) {
			error_code = doclose(element);
			if (error_code < 0) StringCbCopy(type, sizeof(type), "close");
		}
		else if (strcmp(element->tag, "query") == 0) {
			error_code = doquery(element);
			if (error_code < 0) StringCbCopy(type, sizeof(type), "query");
		}
		else if (strcmp(element->tag, "change") == 0) {
			error_code = dochange(element);
			if (error_code < 0) StringCbCopy(type, sizeof(type), "change");
		}
		else if (strcmp(element->tag, "getclipboard") == 0) {
			error_code = dogetclipboard(element);
			if (error_code < 0) StringCbCopy(type, sizeof(type), "getclipboard");
		}
		else if (strcmp(element->tag, "setclipboard") == 0) {
			error_code = dosetclipboard(element);
			if (error_code < 0) StringCbCopy(type, sizeof(type), "setclipboard");
		}
		else if (strcmp(element->tag, "getimagebits") == 0) {
			error_code = dogetimage(element);
			if (error_code < 0) StringCbCopy(type, sizeof(type), "getimagebits");
		}
		else if (strcmp(element->tag, "storeimagebits") == 0) {
			error_code = dostoreimage(element);
			if (error_code < 0) StringCbCopy(type, sizeof(type), "storeimagebits");
		}
		else if (strcmp(element->tag, "getcolor") == 0) {
			error_code = dogetcolor(element);
			if (error_code < 0) StringCbCopy(type, sizeof(type), "getcolor");
		}
		else if (strcmp(element->tag, "getpos") == 0) {
			error_code = dogetpos(element);
			if (error_code < 0) StringCbCopy(type, sizeof(type), "getpos");
		}
		else if (strcmp(element->tag, "msgbox") == 0) {
			CHAR *text = "\0";
			CHAR *title = "\0";
			a1 = element->firstattribute;
			while(a1 != NULL)
			{
				if (!strcmp(a1->tag, "text")) text = a1->value;
				else if (!strcmp(a1->tag, "title")) title = a1->value;
				a1 = a1->nextattribute;
			}
			guimsgbox(title, text);
		}
#endif

		else if (strcmp(element->tag, "sound") == 0) {
			error_code = dosound(element);
			if (error_code < 0) strcpy(type, "sound");
		}

		/*** initialization tags follow: ***/

		else if (strcmp(element->tag, "smartserver") == 0) {
			e1 = element->firstsubelement;
			while (e1 != NULL) {
				/*
				 * Supported runtime properties:
				 *
				 *  keyin    : case, break, interrupt, ikeys, fkeyshift, cancelkey,
				 *             endkey, editmode, edit_special, translate,
				 *             uppertranslate, lowertranslate
				 *  display  : color, bgcolor, hoff, resetsw, autoroll, newcon, columns,
				 *             lines, font, console.noclose
				 *  print    : error, language, translate, lock, device
				 *  gui      : pandlgscale, ctlfont, clipboard, enterkey, focus, minsert.separator, edit.caret
				 *             listbox.insertbefore, bitblt.error, color=rgb
				 *             menu.icon=useTransparentBlt, editbox.style=useClientEdge
				 *  memalloc : If -a not supplied and this is, use this.
				**/
				if (strcmp(e1->tag, "memalloc") == 0) {
					if (!dash_a_supplied && (e2 = e1->firstsubelement) != NULL) {
						config_memalloc = atoi(e2->tag);
						if (config_memalloc > meminitsize) {
							if (meminit(meminitsize = (config_memalloc << 10), 0, 4096) == -1) {
								sprintf(work, "Not enough memory available. (%u)", config_memalloc);
								message(work, "Startup Failure");
								return terminate();
							}
						}
					}
				}
				else if (strcmp(e1->tag, FILETRANSFERCLIENTDIRTAGNAME) == 0) {
					if ((e2 = e1->firstsubelement) != NULL) {
						strcpy(fileTransferLocaldir, e2->tag);
					}
				}
				else if (strcmp(e1->tag, "keyin") == 0) {
					e2 = e1->firstsubelement;
					while (e2 != NULL) {
						if (strcmp(e2->tag, "case") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								prop_keyin_case = (CHAR *) malloc(sizeof(CHAR) * (strlen(e3->tag) + 1));
								if (prop_keyin_case == NULL) {
									error_code = -1603;
									strcpy(type, "malloc");
								}
								else strcpy(prop_keyin_case, e3->tag);
							}
						}
						else if (strcmp(e2->tag, "break") == 0) {
							if ((e3 = e2->firstsubelement) != NULL && e3->cdataflag && strcmp(e3->tag, "off") == 0) {
								breakeventid = maineventids[EVENT_BREAK] = 0;
							}
						}
						else if (strcmp(e2->tag, "interrupt") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								prop_keyin_interrupt = (CHAR *) malloc(sizeof(CHAR) * (strlen(e3->tag) + 1));
								if (prop_keyin_interrupt == NULL) {
									error_code = -1603;
									strcpy(type, "malloc");
								}
								else strcpy(prop_keyin_interrupt, e3->tag);
							}
						}
						else if (strcmp(e2->tag, "ikeys") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								prop_keyin_ikeys = (CHAR *) malloc(sizeof(CHAR) * (strlen(e3->tag) + 1));
								if (prop_keyin_ikeys == NULL) {
									error_code = -1603;
									strcpy(type, "malloc");
								}
								else strcpy(prop_keyin_ikeys, e3->tag);
							}
						}
#if OS_WIN32
						else if (strcmp(e2->tag, "fkeyshift") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								size_t size = (strlen(e3->tag) + 1) * sizeof(CHAR);
								prop_keyin_fkeyshift = (CHAR *) malloc(size);
								if (prop_keyin_fkeyshift == NULL) {
									error_code = -1603;
									StringCbCopy(type, sizeof(type), "malloc");
								}
								else StringCbCopy(prop_keyin_fkeyshift, size, e3->tag);
							}
						}
#endif
						else if (strcmp(e2->tag, "endkey") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								prop_keyin_endkey = (CHAR *) malloc(sizeof(CHAR) * (strlen(e3->tag) + 1));
								if (prop_keyin_endkey == NULL) {
									error_code = -1603;
									strcpy(type, "malloc");
								}
								else strcpy(prop_keyin_endkey, e3->tag);
							}
						}
						else if (strcmp(e2->tag, "cancelkey") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								prop_keyin_cancelkey = (CHAR *) malloc(sizeof(CHAR) * (strlen(e3->tag) + 1));
								if (prop_keyin_cancelkey == NULL) {
									error_code = -1603;
									strcpy(type, "malloc");
								}
								else strcpy(prop_keyin_cancelkey, e3->tag);
							}
						}
						else if (strcmp(e2->tag, "editmode") == 0) {
							if ((e3 = e2->firstsubelement) != NULL && strcmp(e3->tag, "on") == 0) {
								vidparms.flag |= VID_FLAG_EDITMODE;
							}
						}
						else if (strcmp(e2->tag, "edit_special") == 0) {
							if ((e3 = e2->firstsubelement) != NULL && strcmp(e3->tag, "on") == 0) {
								vidparms.flag |= VID_FLAG_EDITSPECIAL;
							}
						}
						else if (strcmp(e2->tag, "translate") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								if (prptranslate(e3->tag, (UCHAR *) xkeymap)) {
									error_code = -5000;
									break;
								}
								else vidparms.keymap = xkeymap;
							}
						}
						else if (strcmp(e2->tag, "lowertranslate") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								if (prptranslate(e3->tag, (UCHAR *) lkeymap)) {
									error_code = -5001;
									break;
								}
								else vidparms.lkeymap = lkeymap;
							}
						}
						else if (strcmp(e2->tag, "uppertranslate") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								if (prptranslate(e3->tag, (UCHAR *) ukeymap)) {
									error_code = -5002;
									break;
								}
								else vidparms.ukeymap = ukeymap;
							}
						}
						e2 = e2->nextelement;
					}
				}
				else if (strcmp(e1->tag, "display") == 0) {
					e2 = e1->firstsubelement;
					while (e2 != NULL) {
						kdsConfigColorModeSC(e2, &vidparms.flag);
						if (strcmp(e2->tag, "color") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								prop_display_color = (CHAR *) malloc(sizeof(CHAR) * (strlen(e3->tag) + 1));
								if (prop_display_color == NULL) {
									error_code = -1603;
									strcpy(type, "malloc");
								}
								else strcpy(prop_display_color, e3->tag);
							}
						}
						else if (strcmp(e2->tag, "bgcolor") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								prop_display_bgcolor = (CHAR *) malloc(sizeof(CHAR) * (strlen(e3->tag) + 1));
								if (prop_display_bgcolor == NULL) {
									error_code = -1603;
									strcpy(type, "malloc");
								}
								else strcpy(prop_display_bgcolor, e3->tag);
							}
						}
						else if (strcmp(e2->tag, "hoff") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								prop_display_hoff = (CHAR *) malloc(sizeof(CHAR) * (strlen(e3->tag) + 1));
								if (prop_display_hoff == NULL) {
									error_code = -1603;
									strcpy(type, "malloc");
								}
								else strcpy(prop_display_hoff, e3->tag);
							}
						}
						else if (strcmp(e2->tag, "resetsw") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								prop_display_resetsw = (CHAR *) malloc(sizeof(CHAR) * (strlen(e3->tag) + 1));
								if (prop_display_resetsw == NULL) {
									error_code = -1603;
									strcpy(type, "malloc");
								}
								else strcpy(prop_display_resetsw, e3->tag);
							}
						}
						else if (strcmp(e2->tag, "autoroll") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								prop_display_autoroll = (CHAR *) malloc(sizeof(CHAR) * (strlen(e3->tag) + 1));
								if (prop_display_autoroll == NULL) {
									error_code = -1603;
									strcpy(type, "malloc");
								}
								else strcpy(prop_display_autoroll, e3->tag);
							}
						}
#if OS_WIN32
						else if (strcmp(e2->tag, "newcon") == 0) {
							if ((e3 = e2->firstsubelement) != NULL && strcmp(e3->tag, "off") == 0) {
								vidparms.flag |= VID_FLAG_NOCON;
							}
						}
						else if (strcmp(e2->tag, "console") == 0) {
							if ((e3 = e2->firstsubelement) != NULL && strcmp(e3->tag, "closebutton") == 0) {
								if ((e4 = e3->firstsubelement) != NULL && strcmp(e4->tag, "off") == 0) {
									vidparms.flag |= VID_FLAG_NOCLOSE;
								}
							}
						}
#endif
						else if (strcmp(e2->tag, "columns") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								vidparms.numcolumns = atoi(e3->tag);
							}
						}
						else if (strcmp(e2->tag, "lines") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								vidparms.numlines = atoi(e3->tag);
							}
						}
						else if (strcmp(e2->tag, "font") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								vidparms.fontname = e3->tag;
							}
						}
						e2 = e2->nextelement;
					}
				}
				else if (strcmp(e1->tag, "print") == 0) {
					e2 = e1->firstsubelement;
					while (e2 != NULL) {
						if (strcmp(e2->tag, "error") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								prop_print_error = (CHAR *) malloc(sizeof(CHAR) * (strlen(e3->tag) + 1));
								if (prop_print_error == NULL) {
									error_code = -1603;
									strcpy(type, "malloc");
								}
								else strcpy(prop_print_error, e3->tag);
							}
						}
						else if (strcmp(e2->tag, "autoflush") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								prop_print_autoflush = (CHAR *) malloc(sizeof(CHAR) * (strlen(e3->tag) + 1));
								if (prop_print_autoflush == NULL) {
									error_code = -1603;
									strcpy(type, "malloc");
								}
								else strcpy(prop_print_autoflush, e3->tag);
							}
						}
						else if (strcmp(e2->tag, "language") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								prop_print_language = (CHAR *) malloc(sizeof(CHAR) * (strlen(e3->tag) + 1));
								if (prop_print_language == NULL) {
									error_code = -1603;
									strcpy(type, "malloc");
								}
								else strcpy(prop_print_language, e3->tag);
							}
						}
						else if (strcmp(e2->tag, "translate") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								prop_print_translate = (CHAR *) malloc(sizeof(CHAR) * (strlen(e3->tag) + 1));
								if (prop_print_translate == NULL) {
									error_code = -1603;
									strcpy(type, "malloc");
								}
								else strcpy(prop_print_translate, e3->tag);
							}
						}
						else if (strcmp(e2->tag, "lock") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								prop_print_lock = (CHAR *) malloc(sizeof(CHAR) * (strlen(e3->tag) + 1));
								if (prop_print_lock == NULL) {
									error_code = -1603;
									strcpy(type, "malloc");
								}
								else strcpy(prop_print_lock, e3->tag);
							}
						}
						else if (strcmp(e2->tag, "pdf") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								if (strcmp(e3->tag, "tab") == 0) {
									if ((e4 = e3->firstsubelement) != NULL) {
										prop_print_pdf_oldtab = (strcmp(e4->tag, "old") == 0);
									}
								}
							}
						}
						e2 = e2->nextelement;
					}
				}
#if OS_WIN32
				else if (strcmp(e1->tag, "gui") == 0) {
					e2 = e1->firstsubelement;
					while (e2 != NULL) {
						if (strcmp(e2->tag, "clipboard") == 0) {
							if ((e3 = e2->firstsubelement) != NULL && strcmp(e3->tag, "codepage") == 0) {
								if ((e4 = e3->firstsubelement) != NULL) {
									guisetcbcodepage(e4->tag);
								}
							}
						}
						else if (strcmp(e2->tag, "color") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								if (strcmp(e3->tag, "rgb") == 0) {
									guisetRGBformatFor24BitColor();
								}
							}
						}
						else if (strcmp(e2->tag, "draw") == 0) {
							if ((e3 = e2->firstsubelement) != NULL && strcmp(e3->tag, "stretchmode") == 0) {
								if ((e4 = e3->firstsubelement) != NULL) {
									guiSetImageDrawStretchMode(e4->tag);
								}
							}
						}
						else if (strcmp(e2->tag, "ctlfont") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								StringCbCopy(guiFontParseControl.initialFontName,
										sizeof(guiFontParseControl.initialFontName), e3->tag);
							}
						}
						else if (strcmp(e2->tag, "bitblt") == 0) {
							if ((e3 = e2->firstsubelement) != NULL && strcmp(e3->tag, "error") == 0) {
								if ((e4 = e3->firstsubelement) != NULL) {
									if (strcmp(e4->tag, "ignore") == 0) guisetBitbltErrorIgnore();
								}
							}
						}
						else if (strcmp(e2->tag, "edit") == 0) {
							if ((e3 = e2->firstsubelement) != NULL && strcmp(e3->tag, "caret") == 0) {
								if ((e4 = e3->firstsubelement) != NULL) {
									if (strcmp(e4->tag, "old") == 0) guisetEditCaretOld();
								}
							}
						}
						else if (strcmp(e2->tag, "fedit") == 0) {
							if ((e3 = e2->firstsubelement) != NULL && strcmp(e3->tag, "selectionbehavior") == 0) {
								if ((e4 = e3->firstsubelement) != NULL) {
									if (strcmp(e4->tag, "old") == 0) guisetfeditboxtextselectiononfocusold();
								}
							}
						}
						else if (strcmp(e2->tag, "editbox") == 0) {
							if ((e3 = e2->firstsubelement) != NULL && strcmp(e3->tag, "style") == 0) {
								if ((e4 = e3->firstsubelement) != NULL) {
									if (ctlmemicmp(e4->tag, "useClientEdge", 13) == 0) guisetUseEditBoxClientEdge(TRUE);
								}
							}
						}
						else if (strcmp(e2->tag, "listbox") == 0) {
							if ((e3 = e2->firstsubelement) != NULL && strcmp(e3->tag, "style") == 0) {
								if ((e4 = e3->firstsubelement) != NULL) {
									if (ctlmemicmp(e4->tag, "useClientEdge", 13) == 0) guisetUseListBoxClientEdge(TRUE);
								}
							}
						}
						else if (strcmp(e2->tag, "enterkey") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								guisetenterkey(e3->tag);
							}
						}
						else if (strcmp(e2->tag, "focus") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								if (strcmp(e3->tag, "old") == 0) guisetfocusold();
							}
						}
						else if (strcmp(e2->tag, IMAGEROTATE) == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
#if DX_MAJOR_VERSION >= 17
								if (strcmp(e3->tag, "old") == 0) guisetImageRotateOld();
#else
								if (strcmp(e3->tag, "new") == 0) guisetImageRotateNew();
#endif
							}
						}
						else if (strcmp(e2->tag, "listbox") == 0) {
							if ((e3 = e2->firstsubelement) != NULL && strcmp(e3->tag, "insertbefore") == 0) {
								if ((e4 = e3->firstsubelement) != NULL) {
									if (strcmp(e4->tag, "old") == 0) guisetlistinsertbeforeold();
								}
							}
						}
						else if (strcmp(e2->tag, "pandlgscale") == 0) {
							StringCbCopy(buf, sizeof(buf), e2->firstsubelement->tag);
							for (i3 = 0; i3 < (INT) strlen(buf) && buf[i3] != '/'; i3++);
							if (i3) {
								if (i3 == (INT) strlen(e2->firstsubelement->tag)) i3 = 0;
								else buf[i3] = 0;
								i1 = atoi(buf);
								if (i3) i2 = atoi(buf + i3 + 1);
							}
							guisetscalexy(i1, i2);
						}
						else if (strcmp(e2->tag, "minsert") == 0) {
							if ((e3 = e2->firstsubelement) != NULL && strcmp(e3->tag, "separator") == 0) {
								if ((e4 = e3->firstsubelement) != NULL) {
									guisetMinsertSeparator(e4->tag);
								}
							}
						}
						else if (strcmp(e2->tag, "menu") == 0) {
							if ((e3 = e2->firstsubelement) != NULL && strcmp(e3->tag, "icon") == 0) {
								if ((e4 = e3->firstsubelement) != NULL) {
									if (strcmp(e4->tag, "useTransparentBlt") == 0) guisetUseTransparentBlt(TRUE);
								}
							}
						}
						else if (strcmp(e2->tag, "quit") == 0) {
							if ((e3 = e2->firstsubelement) != NULL) {
								if (strcmp(e3->tag, "useTerminateProcess") == 0) guisetUseTerminateProcess();
							}
						}
						e2 = e2->nextelement;
					}
				}
				else if (strcmp(e1->tag, "beep") == 0) {
					if ((e2 = e1->firstsubelement) != NULL) {
						if (strcmp(e2->tag, "old") == 0) {
							guisetbeepold();
							vidparms.flag |= VID_FLAG_OLDBEEP;
						}
					}
				}
				else if (strcmp(e1->tag, "terminate") == 0) {
					if ((e2 = e1->firstsubelement) != NULL) {
						if (strcmp(e2->tag, "force") == 0) {
							guisetUseTerminateProcess();
						}
					}
				}
				else if (strcmp(e1->tag, "rolloutwindow") == 0) {
					if ((e2 = e1->firstsubelement) != NULL) {
						if (strcmp(e2->tag, "on") == 0) sysflags |= DSYS_ROLLOUTWIN;
						else if (strcmp(e2->tag, "off") == 0) sysflags &= ~DSYS_ROLLOUTWIN;
					}
				}
#endif
				e1 = e1->nextelement;
			}
		}

		/*** KEYIN and DISPLAY tags follow: ***/

		else if (strcmp(element->tag, "d") == 0) { /* display */
			error_code = doviddisplay(element, flags);
			if (error_code < 0) strcpy(type, "display");
		}
		else if (strcmp(element->tag, "k") == 0) { /* keyin */
			error_code = dovidkeyin(element, flags);
			if (error_code < 0) strcpy(type, "keyin");
		}
		else if (strcmp(element->tag, "ts") == 0) { /* trap */
			e1 = element->firstsubelement;
			while (e1 != NULL) {
				if (e1->cdataflag) {
					for (i1 = e1->cdataflag; --i1 >= 0; ) {
						trapcharmap[((int) e1->tag[i1]) >> 3] |= (1 << (((int) e1->tag[i1]) & 0x07));
						trapcharmap[VID_INTERRUPT >> 3] |= (1 << (VID_INTERRUPT & 0x07));
						vidtrapstart(trapcharmap, maineventids[EVENT_TRAPCHAR]);
					}
				}
				else if (e1->tag[0] == 'c') {
					if ((e3 = e1->firstsubelement) != NULL && e3->cdataflag) {
						i1 = atoi(e3->tag);
						trapcharmap[i1 >> 3] |= (1 << (i1 & 0x07));
						trapcharmap[VID_INTERRUPT >> 3] |= (1 << (VID_INTERRUPT & 0x07));
						vidtrapstart(trapcharmap, maineventids[EVENT_TRAPCHAR]);
					}
				}
				e1 = e1->nextelement;
			}
			if (error_code < 0) strcpy(type, "trap");
		}
		else if (strcmp(element->tag, "tc") == 0) { /* trap clear */
			e1 = element->firstsubelement;
			while (e1 != NULL) {
				if (e1->cdataflag) {
					for (i1 = e1->cdataflag; --i1 >= 0; ) {
						trapcharmap[((int) e1->tag[i1]) >> 3] &= ~(1 << (((int) e1->tag[i1]) & 0x07));
						vidtrapstart(trapcharmap, maineventids[EVENT_TRAPCHAR]);
					}
				}
				else if (!strcmp(e1->tag, "all")) {
						/* clear all keys */
						trapnullmap[VID_INTERRUPT >> 3] |= (1 << (VID_INTERRUPT & 0x07));
						memcpy(trapcharmap, trapnullmap, sizeof(trapcharmap));
						vidtrapstart(trapcharmap, maineventids[EVENT_TRAPCHAR]);
				}
				else if (e1->tag[0] == 'c') {
					if ((e3 = e1->firstsubelement) != NULL && e3->cdataflag) {
						i1 = atoi(e3->tag);
						trapcharmap[i1 >> 3] &= ~(1 << (i1 & 0x07));
						vidtrapstart(trapcharmap, maineventids[EVENT_TRAPCHAR]);
					}
				}
				e1 = e1->nextelement;
			}
			if (error_code < 0) strcpy(type, "trapclr");
		}
		else if (strcmp(element->tag, "se") == 0) { /* set end key */
			e1 = element->firstsubelement;
			while (e1 != NULL) {
				if (e1->cdataflag) {
					for (i1 = e1->cdataflag; --i1 >= 0; ) {
						vidkeyingetfinishmap(kbdfmap);
						kbdfmap[((int) e1->tag[i1]) >> 3] |= 1 << (((int) e1->tag[i1]) & 0x07);
						vidkeyinfinishmap(kbdfmap);
					}
				}
				else if (e1->tag[0] == 'c') {
					if ((e3 = e1->firstsubelement) != NULL && e3->cdataflag) {
						i1 = atoi(e3->tag);
						if (i1 <= 0 || i1 > VID_MAXKEYVAL) {
							error_code = -3005;
							break;
						}
						else {
							vidkeyingetfinishmap(kbdfmap);
							kbdfmap[i1 >> 3] |= 1 << (i1 & 0x07);
							vidkeyinfinishmap(kbdfmap);
						}
					}
				}
				e1 = e1->nextelement;
			}
			if (error_code < 0) strcpy(type, "setendkey");
		}
		else if (strcmp(element->tag, "ce") == 0) { /* clear end key */
			e1 = element->firstsubelement;
			while (e1 != NULL) {
				if (e1->cdataflag) {
					for (i1 = e1->cdataflag; --i1 >= 0; ) {
						vidkeyingetfinishmap(kbdfmap);
						kbdfmap[((int) e1->tag[i1]) >> 3] &= ~(1 << (((int) e1->tag[i1]) & 0x07));
						vidkeyinfinishmap(kbdfmap);
					}
				}
				else if (e1->tag[0] == 'c') {
					if ((e3 = e1->firstsubelement) != NULL && e3->cdataflag) {
						i1 = atoi(e3->tag);
						if (i1 < 0 || i1 > VID_MAXKEYVAL) {
							error_code = -3005;
							break;
						}
						else {
							vidkeyingetfinishmap(kbdfmap);
							if (i1 == 0) { /* clear all endkeys except enter */
								memset(kbdfmap, 0, (VID_MAXKEYVAL >> 3) + 1);
								kbdfmap[VID_ENTER >> 3] |= 1 << (VID_ENTER & 0x07);
							}
							else kbdfmap[i1 >> 3] &= ~(1 << (i1 & 0x07));
							vidkeyinfinishmap(kbdfmap);
						}
					}
				}
				else if (!strcmp(e1->tag, "all")) {
					/* clear all end keys */
					vidkeyingetfinishmap(kbdfmap);
					memset(kbdfmap, 0, (VID_MAXKEYVAL / 8) + 1);
					vidkeyinfinishmap(kbdfmap);
				}
				e1 = e1->nextelement;
			}
			if (error_code < 0) strcpy(type, "clearendkey");
		}
		else if (strcmp(element->tag, "quit") == 0) {
			if (element->firstsubelement != NULL && element->firstsubelement->cdataflag)
				message(element->firstsubelement->tag, "Server Subprocess Error");
			return RC_ERROR; /* end session - terminate client */
		}
		else if (strcmp(element->tag, "winrest") == 0) {
			error_code = dovidrestore(VID_TYPE_WIN, element->firstsubelement->tag);
			if (error_code < 0) strcpy(type, "winrest");
		}
		else if (strcmp(element->tag, "scrnrest") == 0) {
			error_code = dovidrestore(VID_TYPE_SCRN, element->firstsubelement->tag);
			if (error_code < 0) strcpy(type, "scrnrest");
		}
		else if (strcmp(element->tag, "charrest") == 0) {
			error_code = dovidrestore(VID_TYPE_CHAR, element->firstsubelement->tag);
			if (error_code < 0) strcpy(type, "charrest");
		}
		else if (strcmp(element->tag, "getwindow") == 0) {
			error_code = dovidgetwindow();
			if (error_code < 0) strcpy(type, "getwindow");
		}
		else if (strcmp(element->tag, "s") == 0) {
			if (sendelement(&sync, FALSE) < 0) {
				error_code = -4000;
				strcpy(type, "synchronization");
			}
		}

		/* File Transfers */
		else if (strcmp(element->tag, FILETRANSFERCLIENTGETELEMENTTAG) == 0) {
			error_code = sendClientGetFileResult(clientGetFile(element));
			if (error_code < 0) strcpy(type, FILETRANSFERCLIENTGETELEMENTTAG);
		}
		else if (strcmp(element->tag, FILETRANSFERCLIENTPUTELEMENTTAG) == 0) {
			error_code = clientPutFile(element);
			if (error_code < 0) strcpy(type, FILETRANSFERCLIENTPUTELEMENTTAG);
		}
		else { /* Skip if unknown tag */ }

		if (error_code < 0) {
			if (element != NULL) element->nextelement = NULL;
			i1 = xmlflatten(element, FALSE, buf, sizeof(buf));
			if (i1 < 0) buf[0] = '\0';
			else buf[i1] = '\0';
			switch (-error_code) {
				case 100: sprintf(error, "%s failure (Bad XML for change function) : XML : %s", type, buf);
					break;
				case 101: sprintf(error, "%s failure (Resource not found for change function) : XML : %s", type, buf);
					break;
				case 109: sprintf(error, "%s failure (Bad XML for window) : XML : %s", type, buf);
					break;
				case 200: sprintf(error, "%s failure (Bad XML for menu) : XML : %s", type, buf);
					break;
				case 300: sprintf(error, "%s failure (Bad XML for panel or dialog) : XML : %s", type, buf);
					break;
				case 500: sprintf(error, "%s failure (Bad XML for icon) : XML : %s", type, buf);
					break;
				case 600: sprintf(error, "%s failure (Bad XML for image) : XML : %s", type, buf);
					break;
				case 700: sprintf(error, "%s failure (Bad XML for toolbar) : XML : %s", type, buf);
					break;
				case 800: sprintf(error, "%s failure (Bad XML for saveasfiledlg) : XML : %s", type, buf);
					break;
				case 802: sprintf(error, "%s failure (Bad XML for openfiledlg) : XML : %s", type, buf);
					break;
				case 900: sprintf(error, "%s failure (Bad XML for fontdlg) : XML : %s", type, buf);
					break;
				case 1000: sprintf(error, "%s failure (Bad XML for colordlg) : XML : %s", type, buf);
					break;
				case 2000: sprintf(error, "%s failure (Bad XML) : XML : %s", type, buf);
					break;
				case 1603: sprintf(error, "%s failure (Out of memory) : XML : %s", type, buf);
					break;
				case 1804: sprintf(error, "%s failure (Image resource specified was not found) : XML : %s", type, buf);
					break;
				case 2001: sprintf(error, "%s failure (VIDPUT call failed) : XML : %s", type, buf);
					break;
				case 2002:
					if (lasterror[0]) {
						sprintf(error, "%s failure (Initializing keyin / display) : Error : %s", type, lasterror);
					}
					else sprintf(error, "%s failure (Initializing keyin / display) : Error : %s", type, buf);
					break;
				case 2003: sprintf(error, "%s failure (Initializing gui) : XML : %s", type, buf);
					break;
				case 2004: sprintf(error, "%s failure (Keyin) : XML : %s", type, buf);
					break;
				case 2005: sprintf(error, "%s failure (Invalid instruction or control code) : XML : %s", type, buf);
					break;
				case 2008: sprintf(error, "%s failure (Keyin out of memory) : XML : %s", type, buf);
					break;
				case 3000: sprintf(error, "%s failure (Invalid or unsupported trap) : XML : %s", type, buf);
					break;
				case 3002: sprintf(error, "%s failure (Invalid or unsupported trapclr) : XML : %s", type, buf);
					break;
				case 3005: sprintf(error, "%s failure (Invalid or unsupported key) : XML : %s", type, buf);
					break;
				case 3100: sprintf(error, "%s failure (Bad XML for sound) : XML : %s", type, buf);
					break;
				case 4000: sprintf(error, "%s failure (Communication to server)", type);
					break;
				case 5000: sprintf(error, "vidinit failure (dbcdx.keyin.translate property is invalid)");
					break;
				case 5001: sprintf(error, "vidinit failure (dbcdx.keyin.lowertranslate property is invalid)");
					break;
				case 5002: sprintf(error, "vidinit failure (dbcdx.keyin.uppertranslate property is invalid)");
					break;
				default: sprintf(error, "%s failure (Error %d) : XML : %s", type, (error_code * -1), buf);
					break;
			}
			message(error, "SC Fatal Internal Error");
			return RC_ERROR;
		} /* end of while loop */
		element = element->nextelement;
	}
	return 0;
}

static INT dorollout(CHAR *name) /* rollout to the command in name */
{
	INT result;
	ATTRIBUTE a1;
	ELEMENT e1;
#if OS_WIN32
	INT i1, len;
	DWORD rc, flag;
	CHAR work[600], *ptr;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	//OSVERSIONINFO osInfo;
#endif

	result = 0;
	memset(&e1, 0, sizeof(ELEMENT));
	memset(&a1, 0, sizeof(ATTRIBUTE));
	if (vidflag) vidsuspend(TRUE);
#if OS_WIN32
	//osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	//if (!GetVersionEx(&osInfo)) osInfo.dwPlatformId = VER_PLATFORM_WIN32_WINDOWS;  /* assume Windows 95 */
	ptr = getenv("COMSPEC");
	if (ptr != NULL && ptr[0]) {
		strcpy(work, ptr);
		len = (INT)strlen(work);
		for (i1 = len - 1; i1 > 0 && work[i1] != '.' && work[i1] != '\\' && work[i1] != '/'; i1--);
		if (work[i1] != '.') {
			if (IsWindowsVersionOrGreater(5, 0, 0)) {
				strcpy(&work[len], ".exe");
			}
			else strcpy(&work[len], ".com");
		}
	}
	else if (IsWindowsVersionOrGreater(5, 0, 0)) strcpy(work, "cmd.exe");
	else strcpy(work, "command.com");
	strcat(work, " /c ");
	strcat(work, name);
	memset(&si, 0, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	if (!vidflag && !(sysflags & DSYS_ROLLOUTWIN)) {
		flag = CREATE_NO_WINDOW;
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
	}
	else flag = 0;
/*** CODE: THIS CODE AND RELEVANT CODE BELOW FIXES WIN95 BUG OF CHANGING THE DBC TITLE ***/
//	if (osInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
//		i1 = (INT) GetConsoleTitle((LPTSTR) title, sizeof(title) - 1);
//	}

	/*
	 * CreateProcess and GetExitCodeProcess return non-zero on success
	 * and zero on failure.
	 * But the Process exit code is usually 1 for failure and zero for success
	 */
	if (CreateProcess(NULL, (LPTSTR) work, NULL, NULL, TRUE, flag, NULL, NULL, &si, &pi)) {
		CloseHandle(pi.hThread);
		if (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_FAILED && GetExitCodeProcess(pi.hProcess, &rc) && !rc)
		{
		   if (sendresult(0) < 0) {
			   result = -4000;
		   }
		}
		else { // Failure
			e1.tag = "r";
			a1.tag = "e";
			a1.value = "1";
			e1.firstattribute = &a1;
			if (sendelement(&e1, TRUE) < 0) {
				result = -4000;
			}
		}
		CloseHandle(pi.hProcess);
	}

//	if (osInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS && i1) {
//		title[i1] = '\0';
//		SetConsoleTitle((LPTSTR) title);
//	}
#else
	if (!system(name)) {
		if (sendresult(0) < 0) {
			result = -4000;
		}
	}
	else {
		e1.tag = "r";
		a1.tag = "e";
		a1.value = "1";
		e1.firstattribute = &a1;
		if (sendelement(&e1, TRUE) < 0) {
			result = -4000;
		}
	}
#endif
	if (vidflag) vidresume();
	return result;
}

/**
 * Called for an incoming winrest, scrnrest, or charrest command from the server
 */
static INT dovidrestore(INT type, CHAR *buffer)
{
	INT i1, i2, i3, attribflag, bottom, cols, dblflag;
	INT left, len, repeat, right, rows, top;
	/**
	 * The logical string length of the incoming 'buffer'
	 */
	INT size = (INT)strlen(buffer);
	INT32 cmd, vidcmd[100];
	UINT cmdcnt;
	CHAR *ptr;
	UCHAR chr;
	INT statesize = sizeof(STATESAVE);
	UCHAR savestate[sizeof(STATESAVE)];

	vidsavestate(savestate, &statesize);
	cmdcnt = 0;
	if (type == VID_TYPE_SCRN) {
		vidgetsize(&cols, &rows);
		vidcmd[cmdcnt++] = VID_WIN_RESET;
	}
	else {
		vidgetwin(&top, &bottom, &left, &right);
		cols = right - left + 1;
		rows = bottom - top + 1;
	}

	if (type != VID_TYPE_CHAR) {
		vidcmd[cmdcnt++] = VID_REV_OFF;
		vidcmd[cmdcnt++] = VID_BOLD_OFF;
		vidcmd[cmdcnt++] = VID_UL_OFF;
		vidcmd[cmdcnt++] = VID_BLINK_OFF;
		vidcmd[cmdcnt++] = VID_DIM_OFF;
		vidcmd[cmdcnt++] = VID_HDBL_OFF;
		vidcmd[cmdcnt++] = VID_VDBL_OFF;
	}
	attribflag = 0;
	dblflag = 0;
	len = repeat = 0;
	for (i1 = 0; i1 < rows && (size > 0 || repeat); i1++) {
		vidcmd[cmdcnt++] = VID_HORZ | 0;
		vidcmd[cmdcnt++] = VID_VERT | i1;
		for (i2 = 0; i2 < cols; ) {
			if (cmdcnt + 16 > ARRAYSIZE(vidcmd) /*sizeof(vidcmd) / sizeof(*vidcmd)*/) {
				vidcmd[cmdcnt] = VID_END_NOFLUSH;
				cmdcnt = 0;
				if (vidput(vidcmd) < 0) return RC_ERROR;
			}
			if (!repeat) {  /* normally this will be true unless repeat span lines */
				if (len && (i2 + len == cols || !size || *buffer == '~' || *buffer == '^'
						|| *buffer == '!' || *buffer == '`' || *buffer == '@')) {
					if (latin1flag || pcbiosflag) maptranslate(FALSE, ptr);
					if (len > 1) {
						vidcmd[cmdcnt++] = VID_DISPLAY | len;
						if (sizeof(void *) > sizeof(INT32)) {
							if (latin1flag || pcbiosflag) maptranslate(FALSE, ptr);
							memcpy((void *) &vidcmd[cmdcnt], (void *) &ptr, sizeof(void *));
							cmdcnt += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
						}
						else {
							if (latin1flag || pcbiosflag) maptranslate(FALSE, ptr);
							*(UCHAR **)(&vidcmd[cmdcnt++]) = (UCHAR *) ptr;
						}
					}
					else {
						vidcmd[cmdcnt++] = VID_DISP_CHAR | (UCHAR) *ptr;
					}
					i2 += len;
					len = 0;
					continue;
				}
				if (size >= 2 && (*buffer == '~' || *buffer == '^' || *buffer == '!')) {
					INT clr, indx;
					if (*buffer == '~') {
						i3 = (INT) (buffer[1] - '?') << DSPATTR_ATTRIBSHIFT;
						attribflag ^= i3;
						if (attribflag & DSPATTR_REV) {
							if (i3 & DSPATTR_REV) vidcmd[cmdcnt++] = VID_REV_ON;
							else vidcmd[cmdcnt++] = VID_REV_OFF;
						}
						if (attribflag & DSPATTR_BOLD) {
							if (i3 & DSPATTR_BOLD) vidcmd[cmdcnt++] = VID_BOLD_ON;
							else vidcmd[cmdcnt++] = VID_BOLD_OFF;
						}
						if (attribflag & DSPATTR_UNDERLINE) {
							if (i3 & DSPATTR_UNDERLINE) vidcmd[cmdcnt++] = VID_UL_ON;
							else vidcmd[cmdcnt++] = VID_UL_OFF;
						}
						if (attribflag & DSPATTR_BLINK) {
							if (i3 & DSPATTR_BLINK) vidcmd[cmdcnt++] = VID_BLINK_ON;
							else vidcmd[cmdcnt++] = VID_BLINK_OFF;
						}
						if (attribflag & DSPATTR_DIM) {
							if (i3 & DSPATTR_DIM) vidcmd[cmdcnt++] = VID_DIM_ON;
							else vidcmd[cmdcnt++] = VID_DIM_OFF;
						}
						attribflag = i3;
						buffer += 2;
						size -= 2;
					}
					else if (*buffer == '^') {
						if (!isdigit(buffer[1])) {
							vidcmd[cmdcnt++] = VID_FOREGROUND | (buffer[1] - '?');
							buffer += 2;
							size -= 2;
						}
						else {
							for(clr=0, indx=1; isdigit(buffer[indx]);) {
								clr = clr * 10 + buffer[indx++] - '0';
							};
							vidcmd[cmdcnt++] = VID_FOREGROUND | (clr & 0x00FF);
							buffer += indx;
							size -= indx;
						}
					}
					else if (*buffer == '!') {
						if (!isdigit(buffer[1])) {
							vidcmd[cmdcnt++] = VID_BACKGROUND | (buffer[1] - '?');
							buffer += 2;
							size -= 2;
						}
						else {
							for(clr=0, indx=1; isdigit(buffer[indx]);) {
								clr = clr * 10 + buffer[indx++] - '0';
							}
							vidcmd[cmdcnt++] = VID_BACKGROUND | (clr & 0x00FF);
							buffer += indx;
							size -= indx;
						}
					}
					continue;
				}
				if (size >= 2 && *buffer == '`') {
					repeat = buffer[1] - ' ' + 4;
					buffer += 2;
					size -= 2;
				}
				if (size >= 1 && *buffer == '@') {
					buffer++;
					size--;
				}
				if (size <= 0) {  /* should only happen on char restore */
					repeat = 0;
					break;
				}
				if (!repeat && !(attribflag & DSPATTR_GRAPHIC)) {
					if (!len++) ptr = buffer;
					buffer++;
					size--;
					continue;
				}
			}
			if (attribflag & DSPATTR_GRAPHIC) {
				chr = (UCHAR)(*buffer - '?');
				i3 = chr & 0x30;
				dblflag ^= i3;
				if (dblflag & 0x10) {
					if (i3 & 0x10) vidcmd[cmdcnt++] = VID_HDBL_ON;
					else vidcmd[cmdcnt++] = VID_HDBL_OFF;
				}
				if (dblflag & 0x20) {
					if (i3 & 0x20) vidcmd[cmdcnt++] = VID_VDBL_ON;
					else vidcmd[cmdcnt++] = VID_VDBL_OFF;
				}
				dblflag = i3;
				cmd = VID_DISP_SYM | (chr & 0x0F);
			}
			else {
				if (latin1flag || pcbiosflag) maptranslate(FALSE, buffer);
				cmd = VID_DISP_CHAR | (UCHAR) *buffer;
			}
			if (repeat) {
				i3 = repeat;
				if (i3 > cols - i2) i3 = cols - i2;
				vidcmd[cmdcnt++] = VID_REPEAT | i3;
				repeat -= i3;
			}
			else i3 = 1;
			if (!repeat) {
				buffer++;
				size--;
			}
			vidcmd[cmdcnt++] = cmd;
			i2 += i3;
		}
	}
	vidcmd[cmdcnt] = VID_END_FLUSH;
	if (vidput(vidcmd) < 0) return RC_ERROR;
	vidrestorestate(savestate, statesize);
	return 0;
}

static INT srvgetserver(CHAR *filename, INT path)
{
	return 0;
}

static INT doprtopen(ELEMENT *e1)
{
	INT i1, i2, prtnum;
	CHAR prtname[200], language[100], work[8];
	ATTRIBUTE *a1;
	ELEMENT *e2, e3, e4;
	PRTINITINFO initinfo;
	PRTOPTIONS options;

	if (!prtflag) {
		memset(&initinfo, 0, sizeof(initinfo));
		initinfo.getserver = srvgetserver;
#if OS_UNIX
		if (!strcmp(prop_print_lock, "fcntl")) initinfo.locktype = PRTLOCK_FCNTL;
		else initinfo.locktype = PRTLOCK_FILE;
#endif
		if (prop_print_translate) initinfo.translate = prop_print_translate;
		initinfo.pdfOldTabbing = prop_print_pdf_oldtab;
		prtinit(&initinfo);
		prtflag = TRUE;
	}

	if (e1->firstattribute == NULL) return -2000;
	strcpy(prtname, e1->firstattribute->value);
	language[0] = '\0';
	InitializePrintOptions(&options);
	e2 = e1->firstsubelement;
	while (e2 != NULL) {
		switch(e2->tag[0]) {
		case 'a':  /* allocate now or append */
			if (!strcmp(e2->tag, "alloc")) options.flags |= PRT_TEST;
			else if (!strcmp(e2->tag, "append")) options.flags |= PRT_APPD;
			else return -2000;
			break;
		case 'b':  /* printer bin source */
			if ((a1 = e2->firstattribute) == NULL) return -2000;
			if (a1->value != NULL && a1->tag[0] == 's') {
				strcpy(options.paperbin, a1->value);
			}
			else return -2000;
			break;
		case 'c':  /* compressed file or copies*/
			if (!strcmp(e2->tag, "comp")) {
				if (options.flags & (PRT_LCTL | PRT_WDRV)) {
					return sendprintresult(PRTERR_BADOPT);
				}
				options.flags |= PRT_COMP;
			}
			else if (!strcmp(e2->tag, "copy")) {
				if ((a1 = e2->firstattribute) == NULL) return -2000;
				if (a1->value != NULL && a1->tag[0] == 'n') {
					options.copies = 0;
					for (i1 = 0, i2 = (INT)strlen(a1->value); i1 < i2 && isdigit(a1->value[i1]); ) {
						options.copies = options.copies * 10 + a1->value[i1++] - '0';
					}
				}
				else return -2000;
			}
			else if (!strcmp(e2->tag, "ctrl")) {
				if (options.flags & (PRT_COMP | PRT_WDRV)) {
					return sendprintresult(PRTERR_BADOPT);
				}
				options.flags |= PRT_LCTL | PRT_WRAW;
			}
			else return -2000;
			break;
		case 'd': /* dpi, two sided, or document name */
			if (!strcmp(e2->tag, "doc")) {
				if ((a1 = e2->firstattribute) == NULL) return -2000;
				if (a1->value != NULL && a1->tag[0] == 's') {
					strcpy(options.docname, a1->value);
				}
				else return -2000;
			}
			else if (!strcmp(e2->tag, "dpi")) {
				if ((a1 = e2->firstattribute) == NULL) return -2000;
				if (a1->value != NULL && a1->tag[0] == 'n') {
					options.dpi = 0;
					for (i1 = 0, i2 = (INT)strlen(a1->value); i1 < i2 && isdigit(a1->value[i1]); ) {
						options.dpi = options.dpi * 10 + a1->value[i1++] - '0';
					}
				}
				else return -2000;
			}
			else if (!strcmp(e2->tag, "duplex")) {
				options.flags |= PRT_DPLX;
			}
			else return -2000;
			break;

		case 'm': /* User specified margins */
			if (!strcmp(e2->tag, "margins")) {
				if ((a1 = e2->firstattribute) == NULL) return -2000;
				do {
					DOUBLE d1 = strtod((const char *)a1->value, NULL);
					if (a1->tag[0] == 'm') options.margins.margin = d1;
					else if (a1->tag[0] == 't') options.margins.top_margin = d1;
					else if (a1->tag[0] == 'b') options.margins.bottom_margin = d1;
					else if (a1->tag[0] == 'l') options.margins.left_margin = d1;
					else if (a1->tag[0] == 'r') options.margins.right_margin = d1;
					else return -2000;
					a1 = a1->nextattribute;
				} while(a1 != NULL);
			}
			else return -2000;
			break;

		case 'n': /* no eject or no extension*/
			if (!strcmp(e2->tag, "noej")) options.flags |= PRT_NOEJ;
			else if (!strcmp(e2->tag, "noext"))	options.flags |= PRT_NOEX;
			else return -2000;
			break;
		case 'j':  /* job dialog at spool open */
			if (options.flags & (PRT_COMP | PRT_LCTL)) {
				return sendprintresult(PRTERR_BADOPT);
			}
			options.flags |= PRT_JDLG | PRT_WDRV;
			break;
		case 'l':  /* printer language */
			if ((a1 = e2->firstattribute) == NULL) return -2000;
			if (a1->value != NULL && a1->tag[0] == 's') {
				strcpy(language, a1->value);
			}
			else return -2000;
			break;
		case 'o':  /* orientation */
			if (options.flags & (PRT_LAND | PRT_PORT)) return sendprintresult(PRTERR_BADOPT);
			if ((a1 = e2->firstattribute) == NULL) return -2000;
			if (a1->value != NULL && a1->tag[0] == 's') {
				if (!strcmp(a1->value, "port")) options.flags |= PRT_PORT;
				else if (!strcmp(a1->value, "land")) options.flags |= PRT_LAND;
				else return -2000;
			}
			else return -2000;
			break;
		case 'p':  /* pipe to queue/spool or page size*/
#if OS_WIN32 | OS_UNIX
			if (!strcmp(e2->tag, "pipe")) options.flags |= PRT_PIPE;
			else
#endif
			if (!strcmp(e2->tag, "paper")) {
				if ((a1 = e2->firstattribute) == NULL) return -2000;
				if (a1->value == NULL) return sendprintresult(PRTERR_BADOPT);
				if (a1->tag[0] == 's') {
					strcpy(options.papersize, a1->value);
				}
				else return -2000;
			}
			else return -2000;
			break;
		case 'w':
			if ((a1 = e2->firstattribute) == NULL) return -2000;
			if (a1->value == NULL) return sendprintresult(PRTERR_BADOPT);
			if (a1->tag[0] == 's') {
				if (!strcmp(a1->value, "drv")) {
					if (options.flags & (PRT_COMP | PRT_LCTL | PRT_WRAW)) return sendprintresult(PRTERR_BADOPT);
					options.flags |= PRT_WDRV;
				}
				else if (!strcmp(a1->value, "raw")) {
					if (options.flags & (PRT_COMP | PRT_WDRV)) return sendprintresult(PRTERR_BADOPT);
					options.flags |= PRT_LCTL | PRT_WRAW;
				}
				else return -2000;
			}
			else return -2000;
			break;
		default:
			return -2000;
		}
		e2 = e2->nextelement;
	}
	if (!(options.flags & (PRT_LCTL | PRT_COMP | PRT_WDRV))) {
		if (!language[0] && prop_print_language != NULL) {
			strcpy(language, prop_print_language);
		}
		if (language[0]) {
			if (!strcmp(language, "ps")) options.flags |= PRT_LPSL;
			else if (!strcmp(language, "pcl")) options.flags |= PRT_LPCL;
			else if (!strcmp(language, "pcl(noreset)")) options.flags |= PRT_LPCL | PRT_NORE;
			else if (!strcmp(language, "pdf")) options.flags |= PRT_LPDF;
			else if (!strcmp(language, "native")) options.flags |= PRT_WDRV;
			else if (!strcmp(language, "none") || !strcmp(language, "ctl")) {
				options.flags |= PRT_WRAW;
			}
			else sendprintresult(PRTERR_BADOPT);
			if ((options.flags & PRT_APPD) && (options.flags & (PRT_LPCL | PRT_LPSL | PRT_LPDF))) {
				return sendprintresult(PRTERR_BADOPT);
			}
		}
		else if (!cnsflag) options.flags |= PRT_WDRV;
	}
	else if (language[0] && ((options.flags & PRT_COMP) ||
			((options.flags & PRT_LCTL) && strcmp(language, "none")) ||
			((options.flags & PRT_WDRV) && strcmp(language, "native")))) {
		sendprintresult(PRTERR_BADOPT);
	}

	i1 = prtopen(prtname, &options, &prtnum);
	if (i1) return sendprintresult(i1);

	e4.cdataflag = mscitoa(prtnum, work);
	e4.tag = work;
	e4.firstattribute = NULL;
	e4.nextelement = NULL;
	e3.tag = "r";
	e3.cdataflag = 0;
	e3.firstattribute = NULL;
	e3.nextelement = NULL;
	e3.firstsubelement = &e4;

	if (sendelement(&e3, FALSE) < 0) return -4000;
	return 0;
}

static INT doprtclose(ELEMENT *e1)
{
	INT i1, i2, prtnum;
	ELEMENT *e2;
	ATTRIBUTE *a1;
	PRTOPTIONS options;

	options.flags = 0;
	options.copies = 1;
	options.submit.prtname[0] = 0;

	prtnum = -1;
	if ((a1 = e1->firstattribute) != NULL) {
		if (a1->tag[0] == 'h') {
			prtnum = atoi(a1->value);
		}
	}
	if (prtnum < 0) return -2000;

	e2 = e1->firstsubelement;
	while (e2 != NULL) {
		switch(e2->tag[0]) {
		case 'b': /* ban */
			options.flags |= PRT_BANR;
			break;
		case 'd': /* del */
			options.flags |= PRT_REMV;
			break;
		case 'n': /* noej */
			options.flags |= PRT_NOEJ;
			break;
		case 'c': /* copy */
			a1 = e2->firstattribute;
			if (a1 == NULL) return -2000;
			options.copies = i1 = 0;
			while (a1->value[i1]) {
				options.copies = options.copies * 10 + a1->value[i1++] - '0';
			}
			options.flags |= PRT_SBMT;
			break;
		case 'p': /* printer */
			a1 = e2->firstattribute;
			if (a1 == NULL) return -2000;
			options.submit.prtname[0] = 0;
			for (i1 = i2 = 0; (size_t) i2 < sizeof(options.submit.prtname) - 1 && a1->value[i1] && a1->value[i1] != ',' && a1->value[i1] != ' '; ) {
				options.submit.prtname[i2++] = a1->value[i1++];
			}
			options.flags |= PRT_SBMT;
			break;
		case 's': /* sub */
			options.flags |= PRT_SBMT;
			break;
		default:
			return -2000;
		}
		e2 = e2->nextelement;
	}

	i1 = prtclose(prtnum, &options);

	return sendprintresult(i1);
}

static INT doprttext(ELEMENT *e1)
{
	INT i1, i2, i3, i4, prtnum, ctlvalue, ctlangle, ctltype;
	CHAR c1, *ptr;
	ELEMENT *e2;
	ATTRIBUTE *a1;
#if OS_WIN32
	IMAGEREF **imageptr;
#endif

	prtnum = -1;
	if ((a1 = e1->firstattribute) != NULL) {
		if (a1->tag[0] == 'h') {
			prtnum = atoi(a1->value);
		}
	}
	if (prtnum < 0) return -2000;

	e2 = e1->firstsubelement;
	while (e2 != NULL) {
		ctltype = ctlangle = ctlvalue = 0;
		switch(e2->tag[0]) {
		/*** NOTE: i2 must be assigned ***/
		case 'b': /* box, bigd */
			if (e2->tag[1] == 'o' && e2->tag[2] == 'x') {
				i1 = i2 = -1;
				a1 = e2->firstattribute;
				while (a1 != NULL) {
					if (a1->tag[0] == 'x') i1 = atoi(a1->value);
					else if (a1->tag[0] == 'y') i2 = atoi(a1->value);
					a1 = a1->nextattribute;
				}
				if (i1 < 0 || i2 < 0) return -2000;
				i2 = prtbox(prtnum, i1, i2);
			}
			else if (e2->tag[1] == 'i' && e2->tag[2] == 'g') {	/* assume bigd */
				if ((a1 = e2->firstattribute) != NULL) i1 = atoi(a1->value);
				else return -2000;
				i2 = prtbigdot(prtnum, i1);
			}
			else return -2000;
			break;
		case 'c': /* carriage return, color, circ */
			if (strlen(e2->tag) == 1) { /* carriage return */
				if ((a1 = e2->firstattribute) != NULL) i1 = (INT)strtol(a1->value, NULL, 0);
				else i1 = 1;
				i2 = prtcr(prtnum, i1);
			}
			else if (strcmp("color", e2->tag) == 0) {
				long n, r, g, b;
				for (a1 = e2->firstattribute; a1 != NULL; a1 = a1->nextattribute) {
					switch (a1->tag[0]) {
					case 'r':
						r = strtol(a1->value, NULL, 0);
						break;
					case 'g':
						g = strtol(a1->value, NULL, 0);
						break;
					case 'b':
						b = strtol(a1->value, NULL, 0);
						break;
					case 'n':
						n = strtol(a1->value, NULL, 0);
						r = n >> 16;
						g = (n & 0x00FF00) >> 8;
						b = n & 0x0000FF;
						break;
					}
				}
				i2 = prtcolor(prtnum, (r << 16) | (g << 8) | b);
			}
			else if (strcmp("circ", e2->tag) == 0) {
				if ((a1 = e2->firstattribute) != NULL) i1 = (INT)strtol(a1->value, NULL, 0);
				else return -2000;
				i2 = prtcircle(prtnum, i1);
			}
			else return -2000;
			break;
		case 'd': /* text or image */
			if (e2->tag[1] == 'i') { /* assume dimage */
#if OS_WIN32
				if ((a1 = e2->firstattribute) == NULL) return -2000;
				imageptr = findImageRef(a1);
				if (imageptr == NULL) return -2000;
				i2 = prtimage(prtnum, (void *) (*imageptr)->handle);
#endif
			}
			else {
				i3 = 0;
				for (a1 = e2->firstattribute; a1 != NULL; a1 = a1->nextattribute) {
					if (a1->tag[0] == 'r') ctltype |= PRT_TEXT_RIGHT;  /* rj */
					else if (a1->tag[0] == 'c') {  /* cj */
						ctlvalue = atoi(a1->value);
						ctltype |= PRT_TEXT_CENTER;
					}
					else if (a1->tag[0] == 'a') {  /* angled text */
						ctlangle = atoi(a1->value);
						ctltype |= PRT_TEXT_ANGLE;
					}
					else if (a1->tag[0] == 'b') i3 = atoi(a1->value);  /* append blanks */
				}
				if (e2->firstsubelement != NULL) {
					if (!(i2 = e2->firstsubelement->cdataflag)) return -2000;
					ptr = e2->firstsubelement->tag;
				}
				else i2 = 0;
				i2 = prttext(prtnum, (UCHAR *) ptr, 0, i2, i3, ctltype, ctlvalue, ctlangle);
			}
			break;
		case 'f': /* form feed, flush, fb, and font */
			if (e2->tag[1] == 0) { /* ff */
				if ((a1 = e2->firstattribute) != NULL) i1 = atoi(a1->value);
				else i1 = 1;
				i2 = prtff(prtnum, i1);
			}
			else if (e2->tag[1] == 'o') { /* font */
				if ((a1 = e2->firstattribute) == NULL) return -2000;
				i2 = prtfont(prtnum, a1->value);
			}
			else if (e2->tag[1] == 'l') i2 = prtflush(prtnum);
			else if (e2->tag[1] == 'b') { /* form feed with paper bin change */
				i2 = prtffbin(prtnum, a1->value);
			}
			else return -2000;
			break;
		case 'l': /* draw line, line feed, and line width */
			if (e2->tag[1] == 0) { /* lf */
				if ((a1 = e2->firstattribute) != NULL) i1 = atoi(a1->value);
				else i1 = 1;
				i2 = prtlf(prtnum, i1);
			}
			else if (e2->tag[1] == 'w') { /* line width */
				if ((a1 = e2->firstattribute) == NULL) return -2000;
				i2 = prtlinewidth(prtnum, atoi(a1->value));
			}
			else if (e2->tag[1] == 'i') { /* assume draw line */
				i1 = i2 = -1;
				a1 = e2->firstattribute;
				while (a1 != NULL) {
					if (a1->tag[0] == 'h') i1 = atoi(a1->value);
					else if (a1->tag[0] == 'v') i2 = atoi(a1->value);
					a1 = a1->nextattribute;
				}
				i2 = prtline(prtnum, i1, i2);
			}
			else return -2000;
			break;
		case 'n': /* new line */
			if (e2->tag[1] == '\0') { /* nl */
				if ((a1 = e2->firstattribute) != NULL) i1 = atoi(a1->value);
				else i1 = 1;
				i2 = prtnl(prtnum, i1);
			}
			else return -2000;
			break;
		case 'p': /* print position */
			if (e2->tag[1] == '\0') {
				i1 = i2 = -1;
				a1 = e2->firstattribute;
				while (a1 != NULL) {
					if (a1->tag[0] == 'h') i1 = atoi(a1->value);
					else if (a1->tag[0] == 'v') i2 = atoi(a1->value);
					a1 = a1->nextattribute;
				}
				i2 = prtpos(prtnum, i1, i2);
			}
			else return -2000;
			break;
		case 'r': /* rpt(repeat) or rect(rectangle) */
			if (e2->tag[1] == 'p') {
				c1 = 0;
				if (e2->firstsubelement != NULL) {
					if (!e2->firstsubelement->cdataflag) return -2000;
					c1 = (CHAR) atoi(e2->firstsubelement->tag);
				}
				i1 = 1;
				a1 = e2->firstattribute;
				while (a1 != NULL) {
					if (a1->tag[0] == 'n') i1 = atoi(a1->value);
					else if (a1->tag[0] == 'r') { /* rj */
						ctltype |= PRT_TEXT_RIGHT;
					}
					else if (a1->tag[0] == 'c') { /* cj */
						ctlvalue = atoi(a1->value);
						ctltype |= PRT_TEXT_CENTER;

					}
					else if (a1->tag[0] == 'a') { /* angled text */
						ctlangle = atoi(a1->value);
						ctltype |= PRT_TEXT_ANGLE;
					}
					a1 = a1->nextattribute;
				}
				i2 = prttext(prtnum, NULL, c1, i1, 0, ctltype, ctlvalue, ctlangle);
			}
			else if (e2->tag[1] == 'e') {
				i1 = i2 = -1;
				a1 = e2->firstattribute;
				while (a1 != NULL) {
					if (a1->tag[0] == 'x') i1 = atoi(a1->value);
					else if (a1->tag[0] == 'y') i2 = atoi(a1->value);
					a1 = a1->nextattribute;
				}
				if (i1 < 0 || i2 < 0) return -2000;
				i2 = prtrect(prtnum, i1, i2);
			}
			else return -2000;
			break;
		case 't': /* tab position, triangle */
			if (e2->tag[1] == '\0') {
				if ((a1 = e2->firstattribute) != NULL) i1 = atoi(a1->value);
				else i1 = 1;
				i2 = prttab(prtnum, i1);
			}
			else if (e2->tag[1] == 'r' && e2->tag[2] == 'i') {	/* assume tria */
				i1 = i2 = i3 = i4 = -1;
				a1 = e2->firstattribute;
				while (a1 != NULL) {
					if (a1->tag[0] == 'x' && a1->tag[1] == '1') i1 = atoi(a1->value);
					else if (a1->tag[0] == 'y' && a1->tag[1] == '1') i2 = atoi(a1->value);
					else if (a1->tag[0] == 'x' && a1->tag[1] == '2') i3 = atoi(a1->value);
					else if (a1->tag[0] == 'y' && a1->tag[1] == '2') i4 = atoi(a1->value);
					a1 = a1->nextattribute;
				}
				if (i1 < 0 || i2 < 0 || i3 < 0 || i4 < 0) return -2000;
				i2 = prttriangle(prtnum, i1, i2, i3, i4);
			}
			else return -2000;
			break;
		default:
			return -2000;
		}
		if (i2) return sendprintresult(i2);
		e2 = e2->nextelement;
	}

	if (prop_print_autoflush != NULL && !strcmp(prop_print_autoflush, "on")) {
		i2 = prtflush(prtnum);
		if (i2) return sendprintresult(i2);
	}

	return sendprintresult(0);
}

static INT doprtnoej(ELEMENT *e1)
{
	return sendprintresult(0);
}

static INT doprtrel(ELEMENT *e1)
{
	INT i1, prtnum;
	ATTRIBUTE *a1;

	prtnum = -1;
	if ((a1 = e1->firstattribute) != NULL) {
		if (a1->tag[0] == 'h') {
			prtnum = atoi(a1->value);
		}
	}
	if (prtnum < 0) return -2000;

	if (!prtisdevice(prtnum)) return sendprintresult(0);
	i1 = prtalloc(prtnum, 0);
	if (!i1) {
		i1 = prtalloc(prtnum, PRT_TEST);
		if (i1) sendprintresult(i1);
	}
	i1 = prtfree(prtnum);
	if (i1) return sendprintresult(i1);

	return sendprintresult(0);
}

static INT doprtexit()
{
	return sendprintresult(prtexit());
}

static INT doprtsetup()
{
	return sendprintresult(prtpagesetup());
}

static INT doprtalloc(ELEMENT *e1)
{
	INT i1, prtnum;
	ATTRIBUTE *a1;

	prtnum = -1;
	if ((a1 = e1->firstattribute) != NULL) {
		if (a1->tag[0] == 'h') {
			prtnum = atoi(a1->value);
		}
		else return -2000;
	}
	if (prtnum < 0) return -2000;

	i1 = prtalloc(prtnum, PRT_WAIT);
	return sendprintresult(i1);
}

static INT doprtget(ELEMENT *e0, INT flag)
{
	INT i1, i2;
	UINT size;
	CHAR *ptr, name[256];
	UCHAR **pptr;
	ELEMENT e1, e2[MAXPRTVALUES], e3[MAXPRTVALUES];

	size = GETPRINTINFO_MALLOC;
	pptr = memalloc(size, 0);
	if (pptr == NULL) return sendprintresult(PRTERR_NOMEM);

	name[0] = '\0';
	if (flag == GETPRINTINFO_PAPERBINS || flag == GETPRINTINFO_PAPERNAMES) {
		if (e0->firstsubelement != NULL && e0->firstsubelement->cdataflag) {
			strcpy(name, e0->firstsubelement->tag);
		}
	}

	for ( ; ; ) {
		switch (flag) {
		case GETPRINTINFO_PRINTERS:
			i1 = prtgetprinters((CHAR **) pptr, size);
			break;
		case GETPRINTINFO_PAPERBINS:
			i1 = prtgetpaperbins(name, (CHAR **) pptr, size);
			break;
		case GETPRINTINFO_PAPERNAMES:
			i1 = prtgetpapernames(name, (CHAR **) pptr, size);
			break;
		}
		if (i1 != -1) break;
		if (memchange(pptr, size <<= 1, 0) < 0) {
			memfree(pptr);
			return sendprintresult(PRTERR_NOMEM);
		}
	}
	if (i1) {
		memfree(pptr);
		return sendprintresult(PRTERR_NOMEM);
	}
	else {
		i1 = i2 = 0;
		e1.tag = "r";
		e1.cdataflag = 0;
		e1.firstsubelement = NULL;
		for (ptr = (CHAR *) *pptr; *ptr && i2 < MAXPRTVALUES; ptr += i1 + 1, i2++) {
			i1 = (INT)strlen(ptr);
			e3[i2].tag = ptr;
			e3[i2].cdataflag = i1;
			e3[i2].firstattribute = NULL;
			e3[i2].nextelement = NULL;
			e3[i2].firstsubelement = NULL;
			e2[i2].tag = "p";
			e2[i2].cdataflag = 0;
			e2[i2].firstattribute = NULL;
			e2[i2].firstsubelement = &e3[i2];
			e2[i2].nextelement = NULL;
			if (i2 > 0) {
				e2[i2 - 1].nextelement = &e2[i2];
			}
		}
		if (i2)	e1.firstsubelement = &e2[0];
		e1.firstattribute = NULL;
		e1.nextelement = NULL;
		if (sendelement(&e1, FALSE) < 0) {
			memfree(pptr);
			return -4000;
		}
	}
	memfree(pptr);
	return 0;
}

static INT dovidkeyin(ELEMENT *e1, INT flags)
{
	static UCHAR *work;
	static INT worksize = 0;
	INT i1 = 0, i2, cmdcnt, numeric, rgt, len, elen, kllen, dflag;
	INT fieldlen, endkey, fncendkey;
	CHAR *prev, work2[8];
	ELEMENT *e2, e3, e4;
	ATTRIBUTE *a1, a2;

	if (vidflag == FALSE) {
		if (kdsinit()) return -2002;
	}

	if (!worksize) {
		worksize = 256;
		work = (UCHAR *) malloc(sizeof(UCHAR) * worksize);
		if (work == NULL) return -2008;
	}

	endkey = cmdcnt = 0;
	e1 = e1->firstsubelement;
	while (e1 != NULL) {
		if (cmdcnt > 90) {
			vidcmd[cmdcnt] = VID_END_NOFLUSH;
			if (vidput(vidcmd) < 0) return -2005;
			cmdcnt = 0;
		}

		/* Keyin character field (cf) or numeric field (cn) */
		if (!e1->cdataflag && (memcmp(e1->tag, "cf", 2) == 0 || memcmp(e1->tag, "cn", 2) == 0)) {
			len = kllen = rgt = 0;
			numeric = FALSE;
			prev = NULL;
			if (e1->tag[1] == 'n') numeric = TRUE;

			kydsflags &= ~KYDS_DCON;
			a1 = e1->firstattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 'w') { /* width of field */
					if (a1->value == NULL) return -2000;
					len = atoi(a1->value);
					while (len >= worksize) {
						work = (UCHAR *) realloc(work, sizeof(UCHAR) * (worksize << 1));
						if (work == NULL) return -2008;
						worksize <<= 1;
					}
					memset(work, ' ', len);
				}
				else if (memcmp(a1->tag, "rv", 2) == 0) kydsflags |= KYDS_RV;
				else if (memcmp(a1->tag, "dc", 2) == 0) {
					kydsflags |= KYDS_DCON;
				}
				else if (a1->tag[0] == 'r') { /* digits to right of decimal (numeric only) */
					rgt = atoi(a1->value);
				}
				else if (memcmp(a1->tag, "kl", 2) == 0) {
					if (a1->value == NULL) return -2000;
					kllen = atoi(a1->value);
					kydsflags |= KYDS_EDIT;
					vidcmd[cmdcnt++] = VID_KBD_EDIT;
				}
				else if (memcmp(a1->tag, "edit", 4) == 0) {
					kydsflags |= KYDS_EDIT;
					vidcmd[cmdcnt++] = VID_KBD_EDIT;
				}
				else if (memcmp(a1->tag, "de", 2) == 0) vidcmd[cmdcnt++] = VID_KBD_DE;
				a1 = a1->nextattribute;
			}

			if (!len) return -2000;

			if (kydsflags & (KYDS_EDIT | KYDS_EDITON | KYDS_RV)) {
				if ((e2 = e1->firstsubelement) != NULL && e2->cdataflag) {
					elen = e2->cdataflag;
					prev = e2->tag;
					if (!(kydsflags & KYDS_RV)) {
						memcpy(work, prev, elen);
						if (latin1flag || pcbiosflag) maptranslate(FALSE, (CHAR *)work);
					}
				}
				else elen = 0;
			}

			/* PERFORM KEYIN */
			if ((kydsflags & KYDS_KCON) && !(kydsflags & (KYDS_EDIT | KYDS_EDITON))) vidcmd[cmdcnt++] = VID_KBD_KCON;
			else vidcmd[cmdcnt++] = VID_KBD_KCOFF;
			vidcmd[cmdcnt] = VID_END_FLUSH;
			if (vidput(vidcmd) < 0) return -2005;
			cmdcnt = 0;

			if (!(flags & KEYIN_CANCEL)) {
				if (!kllen) kllen = len;
				evtclear(keyineventid);
				if (numeric) {
					if (!(kydsflags & (KYDS_EDIT | KYDS_EDITON | KYDS_RV))) {
						/* initialize empty work variable for numeric keyin */
						memset(work, ' ', len - rgt - 1);
						if (rgt > 0) {
							work[len - rgt - 1] = '.';
							memset(&work[len - rgt], '0', rgt);
						}
						else work[len - 1] = '0';
					}
					if (vidkeyinstart(work, kllen, len, len, (kydsflags & KYDS_DCON) ? VID_KEYIN_NUMERIC | VID_KEYIN_COMMA : VID_KEYIN_NUMERIC, keyineventid)) {
						return -2004;
					}
				}
				else {
					if (vidkeyinstart(work, kllen, (kydsflags & (KYDS_EDIT | KYDS_EDITON)) ? elen : len, len, 0, keyineventid)) {
						return -2004;
					}
				}
				maineventids[maineventidcount] = keyineventid;
				for ( ; ; ) {
					i1 = evtwait(maineventids, maineventidcount + 1);
					if (i1 == EVENT_KATIMER) {
						evtclear(maineventids[EVENT_KATIMER]);
						sendAliveChk();
					}
					else if (i1 != EVENT_DEVTIMER) break;
				}
				vidkeyinend(work, &fieldlen, &endkey);

				if (evttest(maineventids[EVENT_CANCEL])) {  /* must recognize this condition as soon as possible */
/*** NOTE: if this occurs, then keyin and trap crossed paths and ideally no ***/
/***       characters should be passed back.  if possible, better to lose the ***/
/***       characters then to return them to the wrong keyin.  the server will ***/
/***       be waiting for a reply, ie. <r/> to continue. ***/
					evtclear(maineventids[EVENT_CANCEL]);
					fieldlen = endkey = 0;
					flags |= KEYIN_CANCEL;  /* prevent any additional keyins & waits */
				}
				else if (i1 != maineventidcount) {  /* break, trap char or shutdown occurred */
					if (i1 == EVENT_TRAPCHAR) {  /* some type of trap occured, process here */
						evtclear(maineventids[EVENT_TRAPCHAR]);
						vidtrapend(&endkey);
						vidtrapstart(trapcharmap, maineventids[EVENT_TRAPCHAR]);
						kydsflags |= KYDS_TRAP;
					}
					else endkey = 0;  /* let main loop handle event */
					flags |= KEYIN_CANCEL;  /* prevent any additional keyins & waits */
				}
			}
			else endkey = fieldlen = 0;
			if (!(flags & KEYIN_REPLIED)) {
				if (endkey == -1) {
					fncendkey = endkey = 0;
					kydsflags |= KYDS_TIMEOUT;
				}
				else if (!endkey && !i1) fncendkey = VID_ENTER;
				else {
					fncendkey = endkey;
					if (fncendkey && fncendkey != VID_ENTER) kydsflags |= KYDS_FUNCKEY;
				}

				if (kydsflags & KYDS_TRAP) {
					e3.tag = "t";
					e3.cdataflag = 0;
					e3.firstattribute = NULL;
					e3.nextelement = NULL;
					e3.firstsubelement = &e4;
					e4.cdataflag = mscitoa(endkey, work2);
					e4.tag = work2;
					e4.firstattribute = NULL;
					e4.nextelement = NULL;
					e4.firstsubelement = NULL;
					i2 = sendelement(&e3, FALSE);
					if (i2 < 0) return -4000;
				}

				/* Send characters to the server */
				e3.tag = "r";
				e3.cdataflag = 0;
				if (fieldlen > 0) {
					work[fieldlen] = 0;
					if (latin1flag || pcbiosflag) maptranslate(TRUE, (CHAR *) work);
					e4.tag = (CHAR *) work;
					e4.cdataflag = fieldlen;
					e4.firstattribute = NULL;
					e4.nextelement = NULL;
					e3.cdataflag = 0;
					e3.firstsubelement = &e4;
				}
				else e3.firstsubelement = NULL;
				e3.nextelement = NULL;
				if (kydsflags & KYDS_TRAP || (endkey == 0 && !(kydsflags & KYDS_TIMEOUT))) {
					e3.firstattribute = NULL;
				}
				else {
					a2.tag = "e";
					if (kydsflags & KYDS_FUNCKEY) {
						mscitoa(fncendkey, work2);
					}
					else if (kydsflags & KYDS_TIMEOUT) {
						work2[0] = '0';
						work2[1] = 0;
					}
					else mscitoa(endkey, work2);
					a2.value = work2;
					a2.nextattribute = NULL;
					e3.firstattribute = &a2;
				}
				i2 = sendelement(&e3, FALSE);
				if (i2 < 0) return -4000;
				if (flags & KEYIN_CANCEL) flags |= KEYIN_REPLIED;
			}

			kydsflags &= ~(KYDS_EDIT | KYDS_RV | KYDS_TRAP | KYDS_TIMEOUT | KYDS_FUNCKEY);
			vidcmd[cmdcnt++] = VID_KBD_RESET;

			e1 = e1->nextelement;
			continue;
		}  /* end keyin code */

		e2 = e1;
		if (!e1->cdataflag && e1->tag[0] == 'c' && e1->tag[1] == 0) e2 = e1->firstsubelement;
		if (e2->cdataflag) {
			/* Display simple text */
			vidcmd[cmdcnt++] = VID_DISPLAY | e2->cdataflag;
			if (sizeof(void *) > sizeof(INT32)) {
				if (latin1flag || pcbiosflag) maptranslate(FALSE, e2->tag);
				memcpy((void *) &vidcmd[cmdcnt], (void *) &e2->tag, sizeof(void *));
				cmdcnt += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
			}
			else {
				if (latin1flag || pcbiosflag) maptranslate(FALSE, e2->tag);
				*(UCHAR **)(&vidcmd[cmdcnt++]) = (UCHAR *) e2->tag;
			}
			e1 = e1->nextelement; /* yes e1, not e2 */
			continue;
		}

		dflag = FALSE;
		switch(e1->tag[0]) {
		case 'c':
			if (memcmp(e1->tag, "clickon", 7) == 0) vidcmd[cmdcnt++] = VID_KBD_CK_ON;
			else if (memcmp(e1->tag, "clickoff", 8) == 0) vidcmd[cmdcnt++] = VID_KBD_CK_OFF;
			else if (memcmp(e1->tag, "cl", 2) == 0) vidkeyinclear();
			else dflag = TRUE;
			break;
		case 'd':
			/** CODE - Check to see if the server actually ever sends this message **/
			if (memcmp(e1->tag, "dcon", 4) == 0) kydsflags |= KYDS_DCON;
			else if (memcmp(e1->tag, "dcoff", 5) == 0) kydsflags &= ~KYDS_DCON;
			else dflag = TRUE;
			break;
		case 'e':
			if (strcmp(e1->tag, "editon") == 0) {
				kydsflags |= KYDS_EDITON;
				vidcmd[cmdcnt++] = VID_KBD_ED_ON;
			}
			else if (strcmp(e1->tag, "editoff") == 0) {
				kydsflags &= ~(KYDS_EDIT | KYDS_EDITON);
				vidcmd[cmdcnt++] = VID_KBD_ED_OFF;
			}
			else if (strcmp(e1->tag, "eson") == 0) {
				vidcmd[cmdcnt++] = VID_KBD_ESON;
			}
			else if (strcmp(e1->tag, "esoff") == 0) {
				vidcmd[cmdcnt++] = VID_KBD_ESOFF;
			}
			else if (strcmp(e1->tag, "eschar") == 0) {
				if ((e2 = e1->firstsubelement) != NULL && e2->cdataflag) {
					vidcmd[cmdcnt++] = VID_KBD_ECHOCHR | e2->tag[0];
				}
			}
			else if (strcmp(e1->tag, "eon") == 0) vidcmd[cmdcnt++] = VID_KBD_EON;
			else if (strcmp(e1->tag, "eoff") == 0) vidcmd[cmdcnt++] = VID_KBD_EOFF;
			else dflag = TRUE;
			break;
		case 'i':
			if (strcmp(e1->tag, "insmode") == 0) {
				if (kydsflags & (KYDS_EDIT | KYDS_EDITON)) {
					vidcmd[cmdcnt++] = VID_KBD_OVS_OFF;
				}
			}
			else if (strcmp(e1->tag, "it") == 0) {
				if (kydsflags & KYDS_COMPAT) vidcmd[cmdcnt++] = VID_KBD_IN;
				else if (kydsflags & KYDS_UPPER) vidcmd[cmdcnt++] = VID_KBD_LC;
				else vidcmd[cmdcnt++] = VID_KBD_IC;
			}
			else if (strcmp(e1->tag, "in") == 0) {
				if (kydsflags & KYDS_COMPAT) vidcmd[cmdcnt++] = VID_KBD_IC;
				else if (kydsflags & KYDS_UPPER) vidcmd[cmdcnt++] = VID_KBD_UC;
				else vidcmd[cmdcnt++] = VID_KBD_IN;
			}
			else dflag = TRUE;
			break;
		case 'k':
			if (memcmp(e1->tag, "kcon", 4) == 0) {
				kydsflags |= KYDS_KCON;
			}
			else if (memcmp(e1->tag, "kcoff", 5) == 0) {
				kydsflags &= ~(KYDS_KCON);
			}
			else dflag = TRUE;
			break;
		case 'l':
			if (memcmp(e1->tag, "lc", 2) == 0) {
				vidcmd[cmdcnt++] = VID_KBD_LC;
			}
			else dflag = TRUE;
			break;
		case 'o':
			if (strcmp(e1->tag, "ovsmode") == 0) {
				if (kydsflags & (KYDS_EDIT | KYDS_EDITON)) {
					vidcmd[cmdcnt++] = VID_KBD_OVS_ON;
				}
			}
			else dflag = TRUE;
			break;
		case 't':
			if (memcmp(e1->tag, "timeout", 7) == 0) {
				if ((a1 = e1->firstattribute) == NULL || a1->tag[0] != 'n') return -2000;
				i1 = atoi(a1->value);
				/* if i1=65535, timeout will be cleared in vid */
				vidcmd[cmdcnt++] = VID_KBD_TIMEOUT | i1;
			}
			else dflag = TRUE;
			break;
		case 'u':
			if (memcmp(e1->tag, "uc", 2) == 0) vidcmd[cmdcnt++] = VID_KBD_UC;
			else dflag = TRUE;
			break;
		default: dflag = TRUE;
			break;
		}
		if (dflag) {
			/* Maybe it's a display control code */
			if (doviddisplaycc(e1, flags, &cmdcnt) < 0) return -2000;
		}
		e1 = e1->nextelement;
	}
	if (cmdcnt && vidcmd[cmdcnt - 1] == VID_KBD_RESET) cmdcnt--;
	vidcmd[cmdcnt] = VID_FINISH;
	if (vidput(vidcmd) < 0) return -2001;
	return 0;
}

static INT kdspause(INT secs)
{
	INT i1, eventid, timeid;
	UCHAR work[17];

	msctimestamp(work);
	timadd(work, secs * 100);
	if ((eventid = evtcreate()) == -1) return 0;
	timeid = timset(work, eventid);

	evtclear(maineventids[EVENT_TRAPCHAR]); /* reset trap occurred state */
	maineventids[maineventidcount] = eventid;
	i1 = evtwait(maineventids, maineventidcount + 1);

	timstop(timeid);
	evtdestroy(eventid);
	return i1;
}

static INT dovidgetwindow()
{
	INT i1, i2, i3, i4;
	CHAR work1[8], work2[8], work3[8], work4[8];
	ATTRIBUTE a1, a2, a3, a4;
	ELEMENT e1;

	if (vidflag == FALSE) {
		if (kdsinit()) return -2002;
	}

	vidgetwin(&i1, &i2, &i3, &i4);
	mscitoa(i1, work1);
	mscitoa(i2, work2);
	mscitoa(i3, work3);
	mscitoa(i4, work4);
	a1.tag = "t";
	a1.value = work1;
	a2.tag = "b";
	a2.value = work2;
	a3.tag = "l";
	a3.value = work3;
	a4.tag = "r";
	a4.value = work4;
	e1.tag = "getwindow";
	e1.cdataflag = 0;
	e1.nextelement = NULL;
	e1.firstsubelement = NULL;
	e1.firstattribute = &a1;
	a1.nextattribute = &a2;
	a2.nextattribute = &a3;
	a3.nextattribute = &a4;
	a4.nextattribute = NULL;
	i2 = sendelement(&e1, FALSE);
	if (i2 < 0) return -4000;

	return 0;
}

static INT doviddisplay(ELEMENT *e1, INT flags)
{
	INT cmdcnt;
	ELEMENT *e2;

	cmdcnt = 0;
	e1 = e1->firstsubelement;

	if (vidflag == FALSE) {
		if (!(strcmp(e1->tag, "b") == 0 && e1->nextelement == NULL)) {
			if (kdsinit()) return -2002;
		}
	}

	while (e1 != NULL) {
		if (cmdcnt > 90) {
			vidcmd[cmdcnt] = VID_END_NOFLUSH;
			if (vidput(vidcmd) < 0) return -2005;
			cmdcnt = 0;
		}

		e2 = e1;
		if (!e1->cdataflag && e1->tag[0] == 'c' && e1->tag[1] == 0) {
			e2 = e1->firstsubelement;
		}
		if (e2->cdataflag) {
			/* Display simple text */
			vidcmd[cmdcnt++] = VID_DISPLAY | e2->cdataflag;
			if (sizeof(void *) > sizeof(INT32)) {
				if (latin1flag || pcbiosflag) maptranslate(FALSE, e2->tag);
				memcpy((void *) &vidcmd[cmdcnt], (void *) &e2->tag, sizeof(void *));
				cmdcnt += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
			}
			else {
				if (latin1flag || pcbiosflag) maptranslate(FALSE, e2->tag);
				*(UCHAR **)(&vidcmd[cmdcnt++]) = (UCHAR *) e2->tag;
			}
			e1 = e1->nextelement; /* yes e1, not e2 */
			continue;
		}

		if (doviddisplaycc(e1, flags, &cmdcnt) < 0) return -2000;
		e1 = e1->nextelement;
	}

	vidcmd[cmdcnt] = VID_FINISH;
	if (vidput(vidcmd) < 0) return -2001;
	return 0;
}

static INT doviddisplaycc(ELEMENT *e1, INT flags, INT *cmdcnt)
{
	INT i1, i2;
	CHAR work2[8];
	UCHAR c1;
	ELEMENT *e2, e3, e4, *e5;
	ATTRIBUTE *a1;

	switch(e1->tag[0]) {
	case 'a':
		if (memcmp(e1->tag, "alloff", 6) == 0) { /* turn bold and reverse off */
			vidcmd[(*cmdcnt)++] = VID_REV_OFF;
			vidcmd[(*cmdcnt)++] = VID_BOLD_OFF;
			vidcmd[(*cmdcnt)++] = VID_UL_OFF;
			vidcmd[(*cmdcnt)++] = VID_BLINK_OFF;
			vidcmd[(*cmdcnt)++] = VID_DIM_OFF;
			if (kydsflags & KYDS_OLDHOFF) {
				vidcmd[(*cmdcnt)++] = deffgcolor;
				vidcmd[(*cmdcnt)++] = defbgcolor;
			}
		}
		else if (memcmp(e1->tag, "autorolloff", 11) == 0) {
			vidcmd[(*cmdcnt)++] = VID_NL_ROLL_OFF;
		}
		else if (memcmp(e1->tag, "autorollon", 10) == 0) {
			vidcmd[(*cmdcnt)++] = VID_NL_ROLL_ON;
		}
		break;
	case 'b':
		if (memcmp(e1->tag, "blinkon", 7) == 0) { /* blink on */
			vidcmd[(*cmdcnt)++] = VID_BLINK_ON;
		}
		else if (memcmp(e1->tag, "blinkoff", 8) == 0) { /* blinkoff */
			vidcmd[(*cmdcnt)++] = VID_BLINK_OFF;
		}
		else if (memcmp(e1->tag, "boldon", 6) == 0) { /* bold on */
			vidcmd[(*cmdcnt)++] = VID_BOLD_ON;
		}
		else if (memcmp(e1->tag, "boldoff", 7) == 0) { /* bold off */
			vidcmd[(*cmdcnt)++] = VID_BOLD_OFF;
		}
		else if (memcmp(e1->tag, "bgblack", 7) == 0) { /* bg color black */
			vidcmd[(*cmdcnt)++] = VID_BLACK | VID_BACKGROUND;
		}
		else if (memcmp(e1->tag, "bgblue", 6) == 0) { /* bg color blue */
			vidcmd[(*cmdcnt)++] = VID_BLUE | VID_BACKGROUND;
		}
		else if (memcmp(e1->tag, "bggreen", 7) == 0) { /* bg color green */
			vidcmd[(*cmdcnt)++] = VID_GREEN | VID_BACKGROUND;
		}
		else if (memcmp(e1->tag, "bgcyan", 6) == 0) { /* bg color cyan */
			vidcmd[(*cmdcnt)++] = VID_CYAN | VID_BACKGROUND;
		}
		else if (memcmp(e1->tag, "bgred", 5) == 0) { /* bg color red */
			vidcmd[(*cmdcnt)++] = VID_RED | VID_BACKGROUND;
		}
		else if (memcmp(e1->tag, "bgmagenta", 9) == 0) { /* bg color magenta */
			vidcmd[(*cmdcnt)++] = VID_MAGENTA | VID_BACKGROUND;
		}
		else if (memcmp(e1->tag, "bgyellow", 8) == 0) { /* bg color yellow */
			vidcmd[(*cmdcnt)++] = VID_YELLOW | VID_BACKGROUND;
		}
		else if (memcmp(e1->tag, "bgwhite", 7) == 0) { /* bg color white */
			vidcmd[(*cmdcnt)++] = VID_WHITE | VID_BACKGROUND;
		}
		else if (memcmp(e1->tag, "bgcolor", 7) == 0) { /* bg any color */
			if (e1->firstattribute == NULL || e1->firstattribute->tag[0] != 'v') {
				return -2000;
			}
			i1 = atoi(e1->firstattribute->value);
			vidcmd[(*cmdcnt)++] = VID_BACKGROUND | (i1 & 0xFF);
		}
		else if (memcmp(e1->tag, "black", 5) == 0) { /* color black */
			vidcmd[(*cmdcnt)++] = VID_BLACK | VID_FOREGROUND;
		}
		else if (memcmp(e1->tag, "blue", 4) == 0) { /* color blue */
			vidcmd[(*cmdcnt)++] = VID_BLUE | VID_FOREGROUND;
		}
		else if (memcmp(e1->tag, "b", 1) == 0) {
			vidcmd[(*cmdcnt)++] = VID_BEEP | 5;
		}
		break;
	case 'c':
		if (memcmp(e1->tag, "clslin", 6) == 0) { /* close line */
			vidcmd[(*cmdcnt)++] = VID_CL;
		}
		else if (memcmp(e1->tag, "click", 5) == 0) { /* click */
			vidcmd[(*cmdcnt)++] = VID_SOUND;
			vidcmd[(*cmdcnt)++] = 0x70000001;
		}
		else if (memcmp(e1->tag, "cursor", 6) == 0) { /* cursor state */
			if (e1->firstsubelement == NULL) return -2000;
			e2 = e1->firstsubelement;
			if (!e2->cdataflag) return -2000;
			if (memcmp(e2->tag, "on", 2) == 0) vidcmd[(*cmdcnt)++] = VID_CURSOR_ON;
			else if (memcmp(e2->tag, "off", 3) == 0) vidcmd[(*cmdcnt)++] = VID_CURSOR_OFF;
			else if (memcmp(e2->tag, "norm", 4) == 0) vidcmd[(*cmdcnt)++] = VID_CURSOR_NORM;
			else if (memcmp(e2->tag, "uline", 5) == 0) vidcmd[(*cmdcnt)++] = VID_CURSOR_ULINE;
			else if (memcmp(e2->tag, "half", 4) == 0) vidcmd[(*cmdcnt)++] = VID_CURSOR_HALF;
			else if (memcmp(e2->tag, "block", 5) == 0) vidcmd[(*cmdcnt)++] = VID_CURSOR_BLOCK;
		}
		else if (memcmp(e1->tag, "color", 5) == 0) { /* any color */
			if (e1->firstattribute == NULL || e1->firstattribute->tag[0] != 'v') {
				return -2000;
			}
			i1 = atoi(e1->firstattribute->value);
			vidcmd[(*cmdcnt)++] = VID_FOREGROUND | (i1 & 0xFF);
		}
		else if (memcmp(e1->tag, "cyan", 4) == 0) { /* color cyan */
			vidcmd[(*cmdcnt)++] = VID_CYAN | VID_FOREGROUND;
		}
		else if (memcmp(e1->tag, "crs", 3) == 0) { /* graphics character */
			vidcmd[(*cmdcnt)++] = VID_DISP_SYM | (USHORT) 2;
		}
		else if (memcmp(e1->tag, "cr", 2) == 0) { /* carriage return */
			vidcmd[(*cmdcnt)++] = VID_CR;
		}
		break;
	case 'd':
		if (memcmp(e1->tag, "dellin", 6) == 0) vidcmd[(*cmdcnt)++] = VID_DL; /* delete line */
		else if (memcmp(e1->tag, "dblon", 5) == 0) { /* graphics mode */
			vidcmd[(*cmdcnt)++] = VID_HDBL_ON;
			vidcmd[(*cmdcnt)++] = VID_VDBL_ON;
		}
		else if (memcmp(e1->tag, "dbloff", 6) == 0) { /* graphics mode */
			vidcmd[(*cmdcnt)++] = VID_HDBL_OFF;
			vidcmd[(*cmdcnt)++] = VID_VDBL_OFF;
		}
		else if (memcmp(e1->tag, "dtk", 3) == 0) { /* graphics character */
			vidcmd[(*cmdcnt)++] = VID_DISP_SYM | (USHORT) 8;
		}
		else if (memcmp(e1->tag, "dna", 3) == 0) { /* graphics character */
			vidcmd[(*cmdcnt)++] = VID_DISP_SYM | (USHORT)(VID_SYM_UPA + 2);
		}
		else if (memcmp(e1->tag, "delchr", 6) == 0) { /* delete char */
			i1 = i2 = 0;
			a1 = e1->firstattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 'h') i1 = atoi(a1->value);
				else if (a1->tag[0] == 'v') i2 = atoi(a1->value);
				a1 = a1->nextattribute;
			}
			if (!i1 || !i2) return -2000;
			if (e1->tag[0] == 'i') vidcmd[(*cmdcnt)++] = VID_INSCHAR;
			else vidcmd[(*cmdcnt)++] = VID_DELCHAR;
			vidcmd[(*cmdcnt)++] = (((UINT) i1) << 16) | (USHORT) i2;
		}
		break;
	case 'e':
		if (strcmp(e1->tag, "eu") == 0) { /* end up */
			vidcmd[(*cmdcnt)++] = VID_EU;
		}
		else if (strcmp(e1->tag, "ed") == 0) { /* end down */
			vidcmd[(*cmdcnt)++] = VID_ED;
		}
		else if (strcmp(e1->tag, "es") == 0) { /* erase screen */
			vidcmd[(*cmdcnt)++] = VID_ES;
		}
		else if (strcmp(e1->tag, "el") == 0) { /* erase line */
			vidcmd[(*cmdcnt)++] = VID_EL;
		}
		else if (strcmp(e1->tag, "ef") == 0) { /* erase from */
			vidcmd[(*cmdcnt)++] = VID_EF;
		}
		else if (strcmp(e1->tag, "editon") == 0) {
			kydsflags |= KYDS_EDITON;
			vidcmd[(*cmdcnt)++] = VID_KBD_ED_ON;
		}
		else if (strcmp(e1->tag, "editoff") == 0) {
			kydsflags &= ~(KYDS_EDIT | KYDS_EDITON);
			vidcmd[(*cmdcnt)++] = VID_KBD_ED_OFF;
		}
		break;
	case 'g':
		if (memcmp(e1->tag, "green", 5) == 0) { /* color green */
			vidcmd[(*cmdcnt)++] = VID_GREEN | VID_FOREGROUND;
		}
		break;
	case 'h':
		if (memcmp(e1->tag, "hdblon", 6) == 0) { /* graphics mode */
			vidcmd[(*cmdcnt)++] = VID_HDBL_ON;
		}
		else if (memcmp(e1->tag, "hdbloff", 7) == 0) { /* graphics mode */
			vidcmd[(*cmdcnt)++] = VID_HDBL_OFF;
		}
		else if (memcmp(e1->tag, "hln", 3) == 0) { /* graphics character */
			vidcmd[(*cmdcnt)++] = VID_DISP_SYM | (USHORT) 0;
		}
		else if (memcmp(e1->tag, "ha", 2) == 0) { /* horizontal adjust */
			if (e1->firstsubelement == NULL) return -2000;
			e2 = e1->firstsubelement;
			if (!e2->cdataflag) return -2000;
			i1 = atoi(e2->tag);
			vidcmd[(*cmdcnt)++] = VID_HORZ_ADJ | (USHORT) i1;
		}
		else if (memcmp(e1->tag, "hu", 2) == 0) { /* home up */
			vidcmd[(*cmdcnt)++] = VID_HU;
		}
		else if (memcmp(e1->tag, "hd", 2) == 0) { /* home down */
			vidcmd[(*cmdcnt)++] = VID_HD;
		}
		else if (memcmp(e1->tag, "h", 1) == 0) { /* horizontal position */
			if (e1->firstsubelement == NULL) return -2000;
			e2 = e1->firstsubelement;
			if (!e2->cdataflag) return -2000;
			i1 = atoi(e2->tag);
			vidcmd[(*cmdcnt)++] = VID_HORZ | (USHORT) i1;
		}
		break;
	case 'i':
		if (memcmp(e1->tag, "inslin", 6) == 0) { /* insert line */
			vidcmd[(*cmdcnt)++] = VID_IL;
		}
		else if (memcmp(e1->tag, "inschr", 6) == 0) { /* insert char */
			i1 = i2 = 0;
			a1 = e1->firstattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 'h') i1 = atoi(a1->value);
				else if (a1->tag[0] == 'v') i2 = atoi(a1->value);
				a1 = a1->nextattribute;
			}
			if (!i1 || !i2) return -2000;
			if (e1->tag[0] == 'i') vidcmd[(*cmdcnt)++] = VID_INSCHAR;
			else vidcmd[(*cmdcnt)++] = VID_DELCHAR;
			vidcmd[(*cmdcnt)++] = (((UINT) i1) << 16) | (USHORT) i2;
		}
		else if (strcmp(e1->tag, "in")) {
			if (kydsflags & KYDS_COMPAT) vidcmd[(*cmdcnt)++] = VID_KBD_IC;
			else if (kydsflags & KYDS_UPPER) vidcmd[(*cmdcnt)++] = VID_KBD_UC;
			else vidcmd[(*cmdcnt)++] = VID_KBD_IN;
		}
		else if (strcmp(e1->tag, "insmode") == 0) {
			if (kydsflags & (KYDS_EDIT | KYDS_EDITON)) {
				vidcmd[(*cmdcnt)++] = VID_KBD_OVS_OFF;
			}
		}
		break;
	case 'l':
		if (memcmp(e1->tag, "llc", 3) == 0) { /* graphics character */
			vidcmd[(*cmdcnt)++] = VID_DISP_SYM | (USHORT) 5;
		}
		else if (memcmp(e1->tag, "lrc", 3) == 0) { /* graphics character */
			vidcmd[(*cmdcnt)++] = VID_DISP_SYM | (USHORT) 6;
		}
		else if (memcmp(e1->tag, "ltk", 3) == 0) { /* graphics character */
			vidcmd[(*cmdcnt)++] = VID_DISP_SYM | (USHORT) 9;
		}
		else if (memcmp(e1->tag, "lfa", 3) == 0) { /* graphics character */
			vidcmd[(*cmdcnt)++] = VID_DISP_SYM | (USHORT)(VID_SYM_UPA + 3);
		}
		else if (memcmp(e1->tag, "lf", 2) == 0) { /* line feed */
			vidcmd[(*cmdcnt)++] = VID_LF;
		}
		break;
	case 'm':
		if (memcmp(e1->tag, "magenta", 7) == 0) { /* color magenta */
			vidcmd[(*cmdcnt)++] = VID_MAGENTA | VID_FOREGROUND;
		}
		break;
	case 'n':
		if (memcmp(e1->tag, "nl", 2) == 0) { /* new line */
			vidcmd[(*cmdcnt)++] = VID_NL;
		}
		break;
	case 'o':
		if (memcmp(e1->tag, "opnlin", 6) == 0) { /* open line */
			vidcmd[(*cmdcnt)++] = VID_OL;
		}
		break;
	case 'p':
			i1 = i2 = -1;
			a1 = e1->firstattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 'v') i1 = atoi(a1->value);
				else if (a1->tag[0] == 'h') i2 = atoi(a1->value);
				a1 = a1->nextattribute;
			}
			if (i2 >= 0) vidcmd[(*cmdcnt)++] = VID_HORZ | (USHORT) i2;
			if (i1 >= 0) vidcmd[(*cmdcnt)++] = VID_VERT | (USHORT) i1;
		break;
	case 'r':
		if (memcmp(e1->tag, "resetsw", 7) == 0) { /* reset subwindow */
			vidcmd[(*cmdcnt)++] = VID_WIN_RESET;
			if (!(kydsflags & KYDS_RESETSW)) vidcmd[(*cmdcnt)++] = VID_HU;
		}
		else if (memcmp(e1->tag, "rawon", 5) == 0) { /* raw on */
			vidcmd[(*cmdcnt)++] = VID_RAW_ON;
		}
		else if (memcmp(e1->tag, "rawoff", 6) == 0) { /* raw off */
			vidcmd[(*cmdcnt)++] = VID_RAW_OFF;
		}
		else if (memcmp(e1->tag, "revon", 5) == 0) { /* reverse on */
			vidcmd[(*cmdcnt)++] = VID_REV_ON;
		}
		else if (memcmp(e1->tag, "revoff", 6) == 0) { /* reverse off */
			vidcmd[(*cmdcnt)++] = VID_REV_OFF;
		}
		else if (memcmp(e1->tag, "rptdown", 7) == 0 || memcmp(e1->tag, "rptchar", 7) == 0) {
			/* repeat down, repeat char */
			if (e1->tag[3] == 'd' && !(kydsflags & KYDS_DOWN)) vidcmd[(*cmdcnt)++] = VID_DSPDOWN_ON;
			if ((a1 = e1->firstattribute) == NULL) return -2000;
			e2 = e1->firstsubelement;
			if (e2->cdataflag) {
				/* printable character */
				if (latin1flag || pcbiosflag) maptranslate(FALSE, e2->tag);
				c1 = (UCHAR) e2->tag[0];
				vidcmd[(*cmdcnt) + 1] = VID_DISP_CHAR | c1;
			}
			else {
				/* graphics symbol */
				if (!memcmp(e2->tag, "hln", 3)) vidcmd[(*cmdcnt) + 1] = VID_DISP_SYM | (USHORT) 0x0;
				else if (!memcmp(e2->tag, "vln", 3)) vidcmd[(*cmdcnt) + 1] = VID_DISP_SYM | (USHORT) 0x1;
				else if (!memcmp(e2->tag, "crs", 3)) vidcmd[(*cmdcnt) + 1] = VID_DISP_SYM | (USHORT) 0x2;
				else if (!memcmp(e2->tag, "ulc", 3)) vidcmd[(*cmdcnt) + 1] = VID_DISP_SYM | (USHORT) 0x3;
				else if (!memcmp(e2->tag, "urc", 3)) vidcmd[(*cmdcnt) + 1] = VID_DISP_SYM | (USHORT) 0x4;
				else if (!memcmp(e2->tag, "llc", 3)) vidcmd[(*cmdcnt) + 1] = VID_DISP_SYM | (USHORT) 0x5;
				else if (!memcmp(e2->tag, "lrc", 3)) vidcmd[(*cmdcnt) + 1] = VID_DISP_SYM | (USHORT) 0x6;
				else if (!memcmp(e2->tag, "rtk", 3)) vidcmd[(*cmdcnt) + 1] = VID_DISP_SYM | (USHORT) 0x7;
				else if (!memcmp(e2->tag, "dtk", 3)) vidcmd[(*cmdcnt) + 1] = VID_DISP_SYM | (USHORT) 0x8;
				else if (!memcmp(e2->tag, "ltk", 3)) vidcmd[(*cmdcnt) + 1] = VID_DISP_SYM | (USHORT) 0x9;
				else if (!memcmp(e2->tag, "utk", 3)) vidcmd[(*cmdcnt) + 1] = VID_DISP_SYM | (USHORT) 0xA;
				else if (!memcmp(e2->tag, "upa", 3)) vidcmd[(*cmdcnt) + 1] = VID_DISP_SYM | (USHORT) (VID_SYM_UPA + 0x0);
				else if (!memcmp(e2->tag, "rta", 3)) vidcmd[(*cmdcnt) + 1] = VID_DISP_SYM | (USHORT) (VID_SYM_UPA + 0x1);
				else if (!memcmp(e2->tag, "dna", 3)) vidcmd[(*cmdcnt) + 1] = VID_DISP_SYM | (USHORT) (VID_SYM_UPA + 0x2);
				else if (!memcmp(e2->tag, "lfa", 3)) vidcmd[(*cmdcnt) + 1] = VID_DISP_SYM | (USHORT) (VID_SYM_UPA + 0x3);
			}
			i2 = atoi(a1->value); /* assume n tag */
			if (i2 > 0) {
				vidcmd[(*cmdcnt)] = VID_REPEAT | i2;
				(*cmdcnt) += 2;
			}
			if (e1->tag[3] == 'd' && !(kydsflags & KYDS_DOWN)) vidcmd[(*cmdcnt)++] = VID_DSPDOWN_OFF;
			break;
		}
		else if (memcmp(e1->tag, "red", 3) == 0) { /* color red */
			vidcmd[(*cmdcnt)++] = VID_RED | VID_FOREGROUND;
		}
		else if (memcmp(e1->tag, "rtk", 3) == 0) { /* graphics character */
			vidcmd[(*cmdcnt)++] = VID_DISP_SYM | (USHORT) 7;
		}
		else if (memcmp(e1->tag, "rta", 3) == 0) { /* graphics character */
			vidcmd[(*cmdcnt)++] = VID_DISP_SYM | (USHORT)(VID_SYM_UPA + 1);
		}
		else if (memcmp(e1->tag, "ru", 2) == 0) { /* roll up */
			vidcmd[(*cmdcnt)++] = VID_RU;
		}
		else if (memcmp(e1->tag, "rd", 2) == 0) { /* roll down */
			vidcmd[(*cmdcnt)++] = VID_RD;
		}
		break;
	case 's':
		if (memcmp(e1->tag, "scrright", 8) == 0 || memcmp(e1->tag, "scrleft", 7) == 0) {
			kydsflags |= KYDS_DOWN;
			if (e1->tag[3] == 'r') { /* scroll right */
				vidcmd[(*cmdcnt)++] = VID_RR;
				vidcmd[(*cmdcnt)++] = VID_HU;
			}
			else { /* scroll left */
				vidcmd[(*cmdcnt)++] = VID_RL;
				vidcmd[(*cmdcnt)++] = VID_EU;
			}
			vidcmd[(*cmdcnt)++] = VID_DSPDOWN_ON;
			if (e1->firstsubelement != NULL) {
				e5 = e1->firstsubelement;
				while (e5 != NULL) {
					if ((*cmdcnt) > 90) {
						vidcmd[(*cmdcnt)] = VID_END_NOFLUSH;
						if (vidput(vidcmd) < 0) return -2005;
						(*cmdcnt) = 0;
					}

					e2 = e5;
					if (e2->cdataflag) {
						/* Display simple text */
						vidcmd[(*cmdcnt)++] = VID_DISPLAY | e2->cdataflag;
						if (sizeof(void *) > sizeof(INT32)) {
							if (latin1flag || pcbiosflag) maptranslate(FALSE, e2->tag);
							memcpy((void *) &vidcmd[(*cmdcnt)], (void *) &e2->tag, sizeof(void *));
							(*cmdcnt) += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
						}
						else {
							if (latin1flag || pcbiosflag) maptranslate(FALSE, e2->tag);
							*(UCHAR **)(&vidcmd[(*cmdcnt)++]) = (UCHAR *) e2->tag;
						}
						e5 = e5->nextelement;
						continue;
					}
					if (doviddisplaycc(e5, flags, &(*cmdcnt)) < 0) return -2000;
					e5 = e5->nextelement;
				}
			}
			kydsflags &= ~KYDS_DOWN;
			vidcmd[(*cmdcnt)++] = VID_DSPDOWN_OFF;
		}
		else if (memcmp(e1->tag, "setsw", 5) == 0) { /* set subwindow */
			a1 = e1->firstattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 't') {
					vidcmd[(*cmdcnt)++] = VID_WIN_TOP | (USHORT) atoi(a1->value);
				}
				else if (a1->tag[0] == 'b') {
					vidcmd[(*cmdcnt)++] = VID_WIN_BOTTOM | (USHORT) atoi(a1->value);
				}
				else if (a1->tag[0] == 'l') {
					vidcmd[(*cmdcnt)++] = VID_WIN_LEFT | (USHORT) atoi(a1->value);
				}
				else if (a1->tag[0] == 'r') {
					vidcmd[(*cmdcnt)++] = VID_WIN_RIGHT | (USHORT) atoi(a1->value);
				}
				a1 = a1->nextattribute;
			}
			vidcmd[(*cmdcnt)++] = VID_HU;
		}
		break;
	case 'u':
		if (memcmp(e1->tag, "ulon", 4) == 0) { /* underline on */
			vidcmd[(*cmdcnt)++] = VID_UL_ON;
		}
		else if (memcmp(e1->tag, "uloff", 5) == 0) { /* underline off */
			vidcmd[(*cmdcnt)++] = VID_UL_OFF;
		}
		else if (memcmp(e1->tag, "ulc", 3) == 0) { /* graphics character */
			vidcmd[(*cmdcnt)++] = VID_DISP_SYM | (USHORT) 3;
		}
		else if (memcmp(e1->tag, "urc", 3) == 0) { /* graphics character */
			vidcmd[(*cmdcnt)++] = VID_DISP_SYM | (USHORT) 4;
		}
		else if (memcmp(e1->tag, "utk", 3) == 0) { /* graphics character */
			vidcmd[(*cmdcnt)++] = VID_DISP_SYM | (USHORT) 10;
		}
		else if (memcmp(e1->tag, "upa", 3) == 0) { /* graphics character */
			vidcmd[(*cmdcnt)++] = VID_DISP_SYM | (USHORT)(VID_SYM_UPA + 0);
		}
		break;
	case 'v':
		if (memcmp(e1->tag, "vdblon", 6) == 0) { /* graphics mode */
			vidcmd[(*cmdcnt)++] = VID_VDBL_ON;
		}
		else if (memcmp(e1->tag, "vdbloff", 7) == 0) { /* graphics mode */
			vidcmd[(*cmdcnt)++] = VID_VDBL_OFF;
		}
		else if (memcmp(e1->tag, "vln", 3) == 0) { /* graphics character */
			vidcmd[(*cmdcnt)++] = VID_DISP_SYM | (USHORT) 1;
		}
		else if (memcmp(e1->tag, "va", 2) == 0) { /* vertical adjust */
			if (e1->firstsubelement == NULL) return -2000;
			e2 = e1->firstsubelement;
			if (!e2->cdataflag) return -2000;
			i1 = atoi(e2->tag);
			vidcmd[(*cmdcnt)++] = VID_VERT_ADJ | (USHORT) i1;
		}
		else if (memcmp(e1->tag, "v", 1) == 0) { /* vertical position */
			if (e1->firstsubelement == NULL) return -2000;
			e2 = e1->firstsubelement;
			if (!e2->cdataflag) return -2000;
			i1 = atoi(e2->tag);
			vidcmd[(*cmdcnt)++] = VID_VERT | (USHORT) i1;
		}
		break;
	case 'w':
		if (memcmp(e1->tag, "wait", 4) == 0) {
			if ((a1 = e1->firstattribute) != NULL && a1->tag[0] == 'n' && !(flags & KEYIN_CANCEL)) {
				vidcmd[(*cmdcnt)] = VID_END_FLUSH;
				if (vidput(vidcmd) < 0) return -2005;
				(*cmdcnt) = 0;
				i1 = atoi(a1->value);
				if (i1 > 0 && !(kydsflags & (KYDS_TRAP | KYDS_TIMEOUT | KYDS_FUNCKEY))) {
					if (kdspause(i1) == EVENT_TRAPCHAR) {  /* some type of trap occured */
						vidtrapend(&i1); /* i1 = the key that was pressed */
						vidtrapstart(trapcharmap, maineventids[EVENT_TRAPCHAR]);

						/* notify server of trap */
						e3.tag = "t";
						e3.cdataflag = 0;
						e3.firstattribute = NULL;
						e3.nextelement = NULL;
						e3.firstsubelement = &e4;
						e4.cdataflag = mscitoa(i1, work2);
						e4.tag = work2;
						e4.firstattribute = NULL;
						e4.nextelement = NULL;
						e4.firstsubelement = NULL;
						i2 = sendelement(&e3, FALSE);
						if (i2 < 0) return -4000;
					}
				}
			}
		}
		else if (memcmp(e1->tag, "white", 5) == 0) { /* color white */
			vidcmd[(*cmdcnt)++] = VID_WHITE | VID_FOREGROUND;
		}
		break;
	case 'y':
		if (memcmp(e1->tag, "yellow", 6) == 0) { /* color yellow */
			vidcmd[(*cmdcnt)++] = VID_YELLOW | VID_FOREGROUND;
		}
		break;
	default: return RC_ERROR;
	}
	return 0;
}

/*
 * Initialize keyin and character mode display
 *
 */
static INT kdsinit()
{
 	INT i1, intrchar, savecnt, cmdcnt;
	UCHAR kbdfmap[(VID_MAXKEYVAL >> 3) + 1];
	UCHAR kbdvmap[(VID_MAXKEYVAL >> 3) + 1];
	CHAR *ptr;

	lasterror[0] = '\0';

	if ( (i1 = vidinit(&vidparms, breakeventid)) ) {
		vidgeterror(lasterror, sizeof(lasterror));
		return i1;
	}
	if ((keyineventid = evtcreate()) == -1) return RC_ERROR;

	/* set up valid keys, temporarily use kbdfmap variable */
	kydsflags = 0;
	memset(kbdvmap, 0x00, (VID_MAXKEYVAL >> 3) + 1);
	for (i1 = 32; i1 < 127; i1++) kbdvmap[i1 >> 3] |= 1 << (i1 & 0x07);
	if (prop_keyin_ikeys != NULL && !memcmp(prop_keyin_ikeys, "on", 2)) {
		kbdvmap[21 >> 3] |= 1 << (21 & 0x07);
		for (i1 = 128; i1 < 256; i1++) kbdvmap[i1 >> 3] |= 1 << (i1 & 0x07);
	}
	vidkeyinvalidmap(kbdvmap);

	/* set up ending keys */
	memset(kbdfmap, 0x00, (VID_MAXKEYVAL >> 3) + 1);
	kbdfmap[VID_ENTER >> 3] |= 1 << (VID_ENTER & 0x07);
	for (i1 = VID_F1; i1 <= VID_F20; i1++) kbdfmap[i1 >> 3] |= 1 << (i1 & 0x07);

#if OS_WIN32
	if (prop_keyin_fkeyshift != NULL && !strcmp(prop_keyin_fkeyshift, "old")) {
		kydsflags |= KYDS_FSHIFT;
		for (i1 = VID_SHIFTF1; i1 <= VID_SHIFTF10; i1++) kbdfmap[i1 >> 3] |= 1 << (i1 & 0x07);
	}
#endif

	for (i1 = VID_UP; i1 <= VID_RIGHT; i1++) kbdfmap[i1 >> 3] |= 1 << (i1 & 0x07);
	if (prop_keyin_endkey != NULL && !memcmp(prop_keyin_endkey, "xkeys", 5)) {
		for (i1 = VID_INSERT; i1 <= VID_PGDN; i1++) kbdfmap[i1 >> 3] |= 1 << (i1 & 0x07);
		kbdfmap[VID_ESCAPE >> 3] |= 1 << (VID_ESCAPE & 0x07);
		kbdfmap[VID_TAB >> 3] |= 1 << (VID_TAB & 0x07);
		kbdfmap[VID_BKTAB >> 3] |= 1 << (VID_BKTAB & 0x07);
	}
	vidkeyinfinishmap(kbdfmap);

	/* set up default colors */
	deffgcolor = VID_FOREGROUND | VID_WHITE;
	if ((ptr = prop_display_color) != NULL) {
		if (isdigit(ptr[0])) deffgcolor = VID_FOREGROUND | (atoi(ptr) & 0xFF);
		else {
			if (!memcmp(ptr, "*black", 6)) deffgcolor = VID_BLACK | VID_FOREGROUND;
			else if (!memcmp(ptr, "*blue", 5)) deffgcolor = VID_BLUE | VID_FOREGROUND;
			else if (!memcmp(ptr, "*green", 6)) deffgcolor = VID_GREEN | VID_FOREGROUND;
			else if (!memcmp(ptr, "*cyan", 5)) deffgcolor = VID_CYAN | VID_FOREGROUND;
			else if (!memcmp(ptr, "*red", 4)) deffgcolor = VID_RED | VID_FOREGROUND;
			else if (!memcmp(ptr, "*magenta", 8)) deffgcolor = VID_MAGENTA | VID_FOREGROUND;
			else if (!memcmp(ptr, "*yellow", 7)) deffgcolor = VID_YELLOW | VID_FOREGROUND;
			else if (!memcmp(ptr, "*white", 6)) deffgcolor = VID_WHITE | VID_FOREGROUND;
		}
	}
	defbgcolor = VID_BACKGROUND | VID_BLACK;
	if ((ptr = prop_display_bgcolor) != NULL) {
		if (isdigit(ptr[0])) defbgcolor = VID_BACKGROUND | (atoi(ptr) & 0xFF);
		else {
			if (!memcmp(ptr, "*black", 6)) defbgcolor = VID_BLACK | VID_BACKGROUND;
			else if (!memcmp(ptr, "*blue", 5)) defbgcolor = VID_BLUE | VID_BACKGROUND;
			else if (!memcmp(ptr, "*green", 6)) defbgcolor = VID_GREEN | VID_BACKGROUND;
			else if (!memcmp(ptr, "*cyan", 5)) defbgcolor = VID_CYAN | VID_BACKGROUND;
			else if (!memcmp(ptr, "*red", 4)) defbgcolor = VID_RED | VID_BACKGROUND;
			else if (!memcmp(ptr, "*magenta", 8)) defbgcolor = VID_MAGENTA | VID_BACKGROUND;
			else if (!memcmp(ptr, "*yellow", 7)) defbgcolor = VID_YELLOW | VID_BACKGROUND;
			else if (!memcmp(ptr, "*white", 6)) defbgcolor = VID_WHITE | VID_BACKGROUND;
		}
	}
	cmdcnt = 0;
	vidcmd[cmdcnt++] = deffgcolor;
	vidcmd[cmdcnt++] = defbgcolor;

	/* set interrupt key */
	intrchar = 26;  /* default is ^Z */
	if ((ptr = prop_keyin_interrupt) != NULL) {
		if (!strcmp(ptr, "esc")) intrchar = VID_ESCAPE;
		else if (ptr[0] == '^' && (i1 = toupper(ptr[1])) >= 'A' && i1 <= '_') {
			intrchar = i1 - 'A' + 1;
			if (intrchar == 0x1B) intrchar = VID_ESCAPE;
			else if (intrchar == 0x09) intrchar = VID_TAB;
		}
		else if ((ptr[0] == 'f') &&	((i1 = atoi(&ptr[1])) >= 1 && i1 <= 20)) {
			intrchar = VID_F1 + i1 - 1;
		}
		else if (!strcmp(ptr, "none")) intrchar = 0xFFFF;
	}
	vidcmd[cmdcnt++] = VID_KBD_F_INTR | intrchar;

	/* set cancel key */
	savecnt = cmdcnt;
	if ((ptr = prop_keyin_cancelkey) != NULL) {
		if (!strcmp(ptr, "tab")) {
			vidcmd[cmdcnt++] = VID_KBD_F_CNCL1 | VID_TAB;
			vidcmd[cmdcnt++] = VID_KBD_F_CNCL2 | VID_BKTAB;
		}
		else if (!strcmp(ptr, "esc")) {
			vidcmd[cmdcnt++] = VID_KBD_F_CNCL1 | VID_ESCAPE;
		}
		else if (ptr[0] == '^') {
			i1 = toupper(ptr[1]);
			if (i1 >= 'A' && i1 <= '_') {
				i1 -= 'A' - 1;
				if (i1 == 0x1B) vidcmd[cmdcnt++] = VID_KBD_F_CNCL1 | VID_ESCAPE;
				else if (i1 == 0x09) vidcmd[cmdcnt++] = VID_KBD_F_CNCL1 | VID_TAB;
				else vidcmd[cmdcnt++] = VID_KBD_F_CNCL1 | (i1 - 'A' + 1);
			}
		}
		else if (ptr[0] == 'f') {
			i1 = atoi(&ptr[1]);
			if (i1 >= 1 && i1 <= 20) vidcmd[cmdcnt++] = VID_KBD_F_CNCL1 | (VID_F1 + i1 - 1);
		}
	}
	/* default = none */
	if (cmdcnt == savecnt) vidcmd[cmdcnt++] = VID_KBD_F_CNCL1 | 0xFFFF;
	if (cmdcnt == savecnt + 1) vidcmd[cmdcnt++] = VID_KBD_F_CNCL2 | 0xFFFF;

	if ((ptr = prop_display_hoff) != NULL && !strcmp(ptr, "old")) kydsflags |= KYDS_OLDHOFF;
	if ((ptr = prop_display_resetsw) != NULL && !strcmp(ptr, "old")) kydsflags |= KYDS_RESETSW;
	if ((ptr = prop_keyin_case) != NULL) {
		if (!strcmp("reverse", ptr)) {
			kydsflags |= KYDS_COMPAT;
			vidcmd[cmdcnt++] = VID_KBD_IC;
		}
		else if (!strcmp("upper", ptr)) {
			kydsflags |= KYDS_UPPER;
			vidcmd[cmdcnt++] = VID_KBD_UC;
		}
	}

	vidcmd[cmdcnt++] = VID_HORZ | 0;
	vidcmd[cmdcnt++] = VID_VERT | 0;

	if ((ptr = prop_display_autoroll) != NULL && !strncmp(ptr, "off", 3)) {
		vidcmd[cmdcnt++] = VID_NL_ROLL_OFF;
	}

	vidcmd[cmdcnt] = VID_FINISH;
	vidput(vidcmd);
	vidflag = TRUE;

	return 0;
}

#if OS_WIN32
static INT doprepimage(ELEMENT *e1)
{
	ATTRIBUTE *a1;
	UINT len;
	CHAR name[MAXRESNAMELEN + 1];
	INT horz, vert, i1, create;
	PIXHANDLE imagehandle;
	IMAGEREF **imageptr, **image;

	create = TRUE;
	a1 = e1->firstattribute;
	len = (INT)strlen(a1->value);
	if (len > MAXRESNAMELEN) return -600;
	strcpy((UCHAR *) name, a1->value);
	horz = vert = -1;
	a1 = a1->nextattribute;
	while (a1 != NULL) {
		if (a1->tag[0] == 'h') horz = atoi(a1->value);
		else if (a1->tag[0] == 'v') vert = atoi(a1->value);
		else if (a1->tag[0] == 'c') i1 = atoi(a1->value); /* assume colorbits tag */
		else return -600;
		a1 = a1->nextattribute;
	}
	if (horz == -1 || vert == -1) return -600;
	if (sendresult(guipixcreate(i1, horz, vert, &imagehandle)) < 0) {
		return -4000;
	}
	imageptr = firstimage;
	while (imageptr != NULL) {
		if (strcmp((*imageptr)->name, name) == 0) {
			/* named resource already exists */
			(*imageptr)->handle = imagehandle;
			create = FALSE;
			break;
		}
		imageptr = (*imageptr)->next;
	}
	if (create) {
		/* create image reference */
		image = (IMAGEREF **) memalloc(sizeof(IMAGEREF), 0);
		if (image == NULL) return -1603;
		strcpy((*image)->name, name);
		(*image)->handle = imagehandle;
		(*image)->next = NULL;
		/* add to linked list */
		if (firstimage == NULL) firstimage = image;
		else {
			imageptr = firstimage;
			while ((*imageptr)->next != NULL) imageptr = (*imageptr)->next;
			(*imageptr)->next = image;
		}
	}
	return 0;
}

static INT doprep(ELEMENT *e1)
{
	INT i1, i2, hpos, vpos, horz, vert, style, item, create;
	UINT len;
	CHAR name[MAXRESNAMELEN + 1], text[1024], icon[MAXRESNAMELEN + 1], owner[MAXRESNAMELEN + 1];
	size_t cb; /* count of bytes */
	ATTRIBUTE *a1;
	ELEMENT *e2;
	WINHANDLE winhandle;
	RESHANDLE reshandle;
	RESOURCEREF **menuptr, **iconptr, **toolbarptr, **pnldlgptr;
	RESOURCEREF **menu, **icn, **toolbar;
	WINDOWREF **winptr, **window;

	a1 = e1->firstattribute;

	/**
	 * Images get special treatment here, they can be prepped whether
	 * or not this is a gui program
	 */
	if (strcmp(a1->tag, "image") == 0) return doprepimage(e1);

	if (!guiflag) {
		if (cnsflag || guiCSCinit(breakeventid, &guiFontParseControl) != 0) {
			return -2003;
		}
		guiflag = TRUE;
	}
	initializeFontParseControl(FALSE);

	create = TRUE;
	memset(name, ' ', MAXRESNAMELEN);
	name[MAXRESNAMELEN] = '\0';
	if ((strcmp(a1->tag, "window") == 0) || (strcmp(a1->tag, "floatwindow") == 0)) {
		memcpy((UCHAR *) name, a1->value, strlen(a1->value));

		style = WINDOW_STYLE_AUTOSCROLL | WINDOW_STYLE_CENTERED;
		if (strcmp(a1->tag, "floatwindow") == 0) style |= WINDOW_STYLE_FLOATWINDOW;
		else style |= WINDOW_STYLE_SIZEABLE;
		text[0] = owner[0] = '\0';
		hpos = vpos = 0;
		horz = 200;
		vert = 100;
		a1 = a1->nextattribute;
		while (a1 != NULL) {
			if (strcmp(a1->tag, "posx") == 0) {
				hpos = atoi(a1->value);
				style &= ~WINDOW_STYLE_CENTERED;
			}
			else if (strcmp(a1->tag, "posy") == 0) {
				vpos = atoi(a1->value);
				style &= ~WINDOW_STYLE_CENTERED;
			}
			else if (strcmp(a1->tag, "hsize") == 0) horz = atoi(a1->value);
			else if (strcmp(a1->tag, "vsize") == 0) vert = atoi(a1->value);
			else if (strcmp(a1->tag, "title") == 0) StringCbCopy(text, sizeof(text), a1->value);
			else if (strcmp(a1->tag, "fixsize") == 0) {
				if (a1->value[0] == 'y') style &= ~WINDOW_STYLE_SIZEABLE;
			}
			else if (strcmp(a1->tag, "owner") == 0) {
				if (style & WINDOW_STYLE_FLOATWINDOW) StringCbCopy(owner, sizeof(owner), a1->value);
			}
			else if (strcmp(a1->tag, "noclose") == 0) {
				if (a1->value[0] == 'y') style |= WINDOW_STYLE_NOCLOSEBUTTON;
			}
			else if (strcmp(a1->tag, "scrollbars") == 0) {
				if (a1->value[0] == 'n') style &= ~WINDOW_STYLE_AUTOSCROLL;
			}
			else if (strcmp(a1->tag, "maximize") == 0) {
				if (a1->value[0] == 'y') style |= WINDOW_STYLE_MAXIMIZE;
			}
			else if (strcmp(a1->tag, "pandlgscale") == 0) {
				if (a1->value[0] == 'y') style |= WINDOW_STYLE_PANDLGSCALE;
			}
			else if (strcmp(a1->tag, "notaskbarbutton") == 0) {
				if (a1->value[0] == 'y') style |= WINDOW_STYLE_NOTASKBARBUTTON;
			}
			else if (strcmp(a1->tag, "nofocus") == 0) {
				if (a1->value[0] == 'y') style |= WINDOW_STYLE_NOFOCUS;
			}
			else if (strcmp(a1->tag, "statusbar") == 0) {
				if (a1->value[0] == 'y') style |= WINDOW_STYLE_STATUSBAR;
			}
			else return -109; /* Error */
			a1 = a1->nextattribute;
		}
		if (guiwincreate(name, text, style, hpos, vpos, horz, vert, &winhandle,
				owner) < 0) {
			sendresult(1);
			return RC_ERROR;
		}
		else sendresult(0);
		guiwinmsgfnc(winhandle, devicecbfnc);
		winptr = firstwindow;
		while (winptr != NULL) {
			if (strcmp((*winptr)->name, name) == 0) {
				/* named window already exists */
				(*winptr)->handle = winhandle;
				create = FALSE;
				break;
			}
			winptr = (*winptr)->next;
		}
		if (create) {
			/* create window reference */
			window = (WINDOWREF **) memalloc(sizeof(WINDOWREF), 0);
			if (window == NULL) return -1603;
			memcpy((*window)->name, name, sizeof((*window)->name));
			(*window)->handle = winhandle;
			(*window)->next = NULL;
			/* add to linked list */
			if (firstwindow == NULL) firstwindow = window;
			else {
				winptr = firstwindow;
				while ((*winptr)->next != NULL) winptr = (*winptr)->next;
				(*winptr)->next = window;
			}
		}
	}
	else if ((strcmp(a1->tag, "menu") == 0) || (strcmp(a1->tag, "popupmenu") == 0)) {
		guirescreate(&reshandle);
		guiresmsgfnc(reshandle, resourcecbfnc);
		menuptr = firstmenu;
		while (menuptr != NULL) {
			if (strcmp((*menuptr)->name, a1->value) == 0) {
				/* named resource already exists */
				(*menuptr)->handle = reshandle;
				create = FALSE;
				break;
			}
			menuptr = (*menuptr)->next;
		}
		len = (INT)strlen(a1->value);
		if (len > MAXRESNAMELEN) len = MAXRESNAMELEN;
		memcpy((UCHAR *) name, a1->value, len * sizeof(CHAR));
		if (strcmp(a1->tag, "menu") == 0) {
			if (guimenustart(reshandle, name, MAXRESNAMELEN)) return -201;
			e2 = e1->firstsubelement;
			while (e2 != NULL) {
				text[0] = '\0';
				item = 0;
				if (!(strcmp(e2->tag, "main") == 0)) return -202;
				a1 = e2->firstattribute;
				while (a1 != NULL) {
					if (a1->tag[0] == 't') {
						cb = (strlen(a1->value) + 1) * sizeof(CHAR);
						if (cb > sizeof(text)) {
							memcpy(text, a1->value, sizeof(text) - sizeof(CHAR));
							text[sizeof(text) - 1] = '\0';
						}
						else strcpy(text, a1->value);
					}
					else if (a1->tag[0] == 'i') item = atoi(a1->value);
					a1 = a1->nextattribute;
				}
				if (guimenumain(item, text, (INT)strlen(text))) return -201;
				if ((i1 = parsemenusubelements(e2->firstsubelement)) < 0) return i1;
				e2 = e2->nextelement;
			}
		}
		else {
			if (guipopupmenustart(reshandle, name, MAXRESNAMELEN)) return -201;
			if ((i1 = parsemenusubelements(e1->firstsubelement)) < 0) return i1;
		}
		if (guimenuend()) return sendresult(1);
		if (sendresult(0) < 0) return -4000;
		if (create) {
			/* create menu reference */
			menu = (RESOURCEREF **) memalloc(sizeof(RESOURCEREF), 0);
			if (menu == NULL) return -1603;
			memcpy((*menu)->name, name, sizeof(name));
			(*menu)->handle = reshandle;
			(*menu)->next = NULL;
			/* add to linked list */
			if (firstmenu == NULL) firstmenu = menu;
			else {
				menuptr = firstmenu;
				while ((*menuptr)->next != NULL) menuptr = (*menuptr)->next;
				(*menuptr)->next = menu;
			}
		}
	}
	else if ((strcmp(a1->tag, "panel") == 0) || (strcmp(a1->tag, "dialog") == 0)) {
		guirescreate(&reshandle);
		guiresmsgfnc(reshandle, resourcecbfnc);
		pnldlgptr = firstpnldlg;
		while (pnldlgptr != NULL) {
			if (strcmp((*pnldlgptr)->name, a1->value) == 0) {
				/* named resource already exists */
				(*pnldlgptr)->handle = reshandle;
				create = FALSE;
				break;
			}
			pnldlgptr = (*pnldlgptr)->next;
		}
		len = (INT)strlen(a1->value);
		memcpy((UCHAR *) name, a1->value, len);
		if (memcmp(a1->tag, "panel", 5) == 0) {
			if (guipanelstart(reshandle, name, 8)) return sendresult(1);
		}
		else {
			if (guidialogstart(reshandle, name, 8)) return sendresult(1);
			vert = horz = -1;
			a1 = a1->nextattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 't') { /* assume title tag */
					if (guipandlgtitle(a1->value, (INT)strlen(a1->value))) return sendresult(1);
				}
				else if (a1->tag[0] == 'v') vert = atoi(a1->value); /* assume vsize tag */
				else if (a1->tag[0] == 'h') horz = atoi(a1->value); /* assume hsize tag */
				else if (a1->tag[0] == 'n') { /* assume noclose tag */
					if (a1->value[0] == 'y') guipandlgnoclosebutton();
				}
				a1 = a1->nextattribute;
			}
			if (vert != -1 && horz != -1) {
				if (guipandlgsize(horz, vert)) return sendresult(1);
			}
		}
		if ((i1 = parsepanelsubelements(e1->firstsubelement)) < 0) {
			return sendresult(1);
		}
		if (guipandlgend()) return sendresult(1);
		if (sendresult(0) < 0) return -4000;
		if (create) {
			if ((i1 = doPrepCreateDialogRef(name, len, reshandle))) return i1;
		}
	}
	else if (strcmp(a1->tag, "icon") == 0) {
		guirescreate(&reshandle);
		guiresmsgfnc(reshandle, resourcecbfnc);
		iconptr = firsticon;
		while (iconptr != NULL) {
			if (strcmp((*iconptr)->name, a1->value) == 0) {
				/* named icon already exists */
				(*iconptr)->handle = reshandle;
				create = FALSE;
				break;
			}
			iconptr = (*iconptr)->next;
		}
		len = (INT)strlen(a1->value);
		memcpy((UCHAR *) name, a1->value, len);
		horz = vert = -1;
		if (guiiconstart(reshandle, name, 8)) return sendresult(1);
		a1 = a1->nextattribute;
		while (a1 != NULL) {
			if (a1->tag[0] == 'h') {
				horz = atoi(a1->value);
				if (guiiconhsize(horz)) return sendresult(1);
			}
			else if (a1->tag[0] == 'v') {
				vert = atoi(a1->value);
				if (guiiconvsize(vert)) return sendresult(1);
			}
			else if (a1->tag[0] == 'c') { /* assume colorbits tag */
				i1 = atoi(a1->value);
				if (guiiconcolorbits(i1)) return sendresult(1);
			}
			else if (a1->tag[0] == 'p') { /* assume pixels tag */
				if (horz && vert && i1) {
					i2 = horz * vert;
					if (i1 == 24) i2 *= 6;
					else if (i1 != 4 && i1 != 1) return sendresult(1);
					if (((INT) strlen(a1->value)) != i2) return sendresult(1);
				}
				else return -500;
				if (guiiconpixels((UCHAR *) a1->value, (INT)strlen(a1->value))) return sendresult(1);
			}
			else return -500;
			a1 = a1->nextattribute;
		}
		if (guiiconend()) return sendresult(1);
		if (sendresult(0) < 0) return -4000;
		if (create) {
			/* create icon reference */
			icn = (RESOURCEREF **) memalloc(sizeof(RESOURCEREF), 0);
			if (icn == NULL) return -1603;
			name[len] = 0;
			memcpy((*icn)->name, name, 9);
			(*icn)->handle = reshandle;
			(*icn)->next = NULL;
			/* add to linked list */
			if (firsticon == NULL) firsticon = icn;
			else {
				iconptr = firsticon;
				while ((*iconptr)->next != NULL) iconptr = (*iconptr)->next;
				(*iconptr)->next = icn;
			}
		}
	}
	else if (strcmp(a1->tag, "toolbar") == 0) {
		guirescreate(&reshandle);
		guiresmsgfnc(reshandle, resourcecbfnc);
		toolbarptr = firsttoolbar;
		while (toolbarptr != NULL) {
			if (strcmp((*toolbarptr)->name, a1->value) == 0) {
				/* named resource already exists */
				(*toolbarptr)->handle = reshandle;
				create = FALSE;
				break;
			}
			toolbarptr = (*toolbarptr)->next;
		}
		len = (INT)strlen(a1->value);
		if (len > MAXRESNAMELEN) len = MAXRESNAMELEN;
		memcpy((UCHAR *) name, a1->value, len);
		guitoolbarstart(reshandle, name, MAXRESNAMELEN);
		e2 = e1->firstsubelement;
		while (e2 != NULL) {
			if (e2->tag[0] == 'p' || e2->tag[0] == 'l') { /* assume Pushbutton or Lockbutton tag */
				text[0] = icon[0] = '\0';
				i1 = -1;
				a1 = e2->firstattribute;;
				while (a1 != NULL) {
					if (strcmp(a1->tag, "icon") == 0) {
						memset(icon, ' ', MAXRESNAMELEN);
						memcpy((UCHAR *) icon, a1->value, strlen(a1->value));
						icon[MAXRESNAMELEN] = '\0';
					}
					else if (a1->tag[0] == 'i') i1 = atoi(a1->value);
					else if (a1->tag[0] == 't') StringCbCopy(text, sizeof(text), a1->value);
					else return -700;
					a1 = a1->nextattribute;
				}
				if (i1 < 0 || icon[0] == '\0') return -700;
				if (e2->tag[0] == 'p') {
					if (guitoolbarpushbutton(i1, (UCHAR *) icon, (INT)strlen(icon), (UCHAR *) text, (INT)strlen(text)) < 0) {
						return sendresult(1);
					}
				}
				else {
					if (guitoolbarlockbutton(i1, (UCHAR *) icon, (INT)strlen(icon), (UCHAR *) text, (INT)strlen(text)) < 0) {
						return sendresult(1);
					}
				}
			}
			else if (e2->tag[0] == 's') { /* assume space tag */
				if (guitoolbarspace() < 0) return sendresult(1);
			}
			else if (e2->tag[0] == 'g') { /* assume gray tag */
				if (guitoolbargray() < 0) return -700;
			}
			else if (e2->tag[0] == 'd') { /* assume dropbox tag */
				i1 = horz = vert = -1;
				a1 = e2->firstattribute;;
				while (a1 != NULL) {
					if (a1->tag[0] == 'i') i1 = atoi(a1->value);
					else if (a1->tag[0] == 'h') horz = atoi(a1->value);
					else if (a1->tag[0] == 'v') vert = atoi(a1->value);
					else return -700;
					a1 = a1->nextattribute;
				}
				if (i1 < 0 || horz < 0 || vert < 0) return -700;
				if (guitoolbardropbox(i1, horz, vert) < 0) {
					return sendresult(1);
				}
			}
			else return -700;
			e2 = e2->nextelement;
		}
		if (guitoolbarend() < 0) return sendresult(1);
		if (sendresult(0) < 0) return -4000;
		if (create) {
			/* create toolbar reference */
			toolbar = (RESOURCEREF **) memalloc(sizeof(RESOURCEREF), 0);
			if (toolbar == NULL) return -1603;
			strcpy((*toolbar)->name, name);
			(*toolbar)->handle = reshandle;
			(*toolbar)->next = NULL;
			/* add to linked list */
			if (firsttoolbar == NULL) firsttoolbar = toolbar;
			else {
				toolbarptr = firsttoolbar;
				while ((*toolbarptr)->next != NULL) toolbarptr = (*toolbarptr)->next;
				(*toolbarptr)->next = toolbar;
			}
		}
	}
	else if (strcmp(a1->tag, "openfdlg") == 0 || strcmp(a1->tag, "saveasfdlg") == 0
			|| strcmp(a1->tag, "openddlg") == 0) {
		guirescreate(&reshandle);
		guiresmsgfnc(reshandle, resourcecbfnc);
		pnldlgptr = firstpnldlg;
		while (pnldlgptr != NULL) {
			if (strcmp((*pnldlgptr)->name, a1->value) == 0) {
				/* named resource already exists */
				(*pnldlgptr)->handle = reshandle;
				create = FALSE;
				break;
			}
			pnldlgptr = (*pnldlgptr)->next;
		}
		len = (INT)strlen(a1->value);
		if (len > MAXRESNAMELEN) len = MAXRESNAMELEN;
		memcpy((UCHAR *) name, a1->value, len);
		if (strcmp(a1->tag, "openfdlg") == 0) {
			if (guiopenfiledlgstart(reshandle, name, MAXRESNAMELEN)) return sendresult(1);
		}
		else if (strcmp(a1->tag, "openddlg") == 0) {
			if (guiopendirdlgstart(reshandle, name, MAXRESNAMELEN)) return sendresult(1);
		}
		else if (strcmp(a1->tag, "saveasfdlg") == 0) {
			if (guisaveasfiledlgstart(reshandle, name, MAXRESNAMELEN)) return sendresult(1);
		}

		while ((a1 = a1->nextattribute) != NULL) {
			if (strcmp(a1->tag, "default") == 0) {
				if (guifiledlgdefault((UCHAR *) a1->value, (INT)strlen(a1->value))) return sendresult(1);
			}
			else if (strcmp(a1->tag, "devicefilter") == 0) {
				if (guifiledlgDeviceFilter((UCHAR *) a1->value, (INT)strlen(a1->value))) return sendresult(1);
			}
			else if (strcmp(a1->tag, "title") == 0) {
				if (guifiledlgTitle((UCHAR *) a1->value, (INT)strlen(a1->value))) return sendresult(1);
			}
		}
		if (guifiledlgend()) return sendresult(1);
		if (sendresult(0) < 0) return -4000;
		if (create) {
			if ((i1 = doPrepCreateDialogRef(name, len, reshandle))) return i1;
		}
	}
	else if (strcmp(a1->tag, "alertdlg") == 0) {
		guirescreate(&reshandle);
		guiresmsgfnc(reshandle, resourcecbfnc);
		pnldlgptr = firstpnldlg;
		while (pnldlgptr != NULL) {
			if (strcmp((*pnldlgptr)->name, a1->value) == 0) {
				/* named resource already exists */
				(*pnldlgptr)->handle = reshandle;
				create = FALSE;
				break;
			}
			pnldlgptr = (*pnldlgptr)->next;
		}
		len = (INT)strlen(a1->value);
		if (len > MAXRESNAMELEN) len = MAXRESNAMELEN;
		memcpy((UCHAR *) name, a1->value, len);
		if (guialertdlgstart(reshandle, name, MAXRESNAMELEN)) return sendresult(1);
		a1 = a1->nextattribute;
		if (a1 != NULL && strcmp(a1->tag, "text") == 0) {
			if (guialertdlgtext((UCHAR *) a1->value, (INT)strlen(a1->value))) return sendresult(1);
		}
		if (guialertdlgend()) return sendresult(1);
		if (sendresult(0) < 0) return -4000;
		if (create) {
			if ((i1 = doPrepCreateDialogRef(name, len, reshandle))) return i1;
		}
	}
	else if (strcmp(a1->tag, "fontdlg") == 0) {
		guirescreate(&reshandle);
		guiresmsgfnc(reshandle, resourcecbfnc);
		pnldlgptr = firstpnldlg;
		while (pnldlgptr != NULL) {
			if (strcmp((*pnldlgptr)->name, a1->value) == 0) {
				/* named resource already exists */
				(*pnldlgptr)->handle = reshandle;
				create = FALSE;
				break;
			}
			pnldlgptr = (*pnldlgptr)->next;
		}
		len = (INT)strlen(a1->value);
		memcpy((UCHAR *) name, a1->value,len);
		if (guifontdlgstart(reshandle, name, 8)) return sendresult(1);
		if (guifontdlgend()) return sendresult(1);
		if (sendresult(0) < 0) return -4000;
		if (create) {
			if ((i1 = doPrepCreateDialogRef(name, len, reshandle))) return i1;
		}
	}
	else if (memcmp(a1->tag, "colordlg", 8) == 0) {
		guirescreate(&reshandle);
		guiresmsgfnc(reshandle, resourcecbfnc);
		pnldlgptr = firstpnldlg;
		while (pnldlgptr != NULL) {
			if (strcmp((*pnldlgptr)->name, a1->value) == 0) {
				/* named resource already exists */
				(*pnldlgptr)->handle = reshandle;
				create = FALSE;
				break;
			}
			pnldlgptr = (*pnldlgptr)->next;
		}
		len = (INT)strlen(a1->value);
		memcpy((UCHAR *) name, a1->value, len);
		if (guicolordlgstart(reshandle, name, 8)) return sendresult(1);
		if (guicolordlgend()) return sendresult(1);
		if (sendresult(0) < 0) return -4000;
		if (create) {
			if ((i1 = doPrepCreateDialogRef(name, len, reshandle))) return i1;
		}
	}
	else return RC_ERROR; /* Error */
	return 0;
}

/*
 * create dialog reference
 */
static INT doPrepCreateDialogRef(CHAR *name, UINT len, RESHANDLE reshandle) {
	RESOURCEREF **pnldlg, **pnldlgptr;
	pnldlg = (RESOURCEREF **) memalloc(sizeof(RESOURCEREF), 0);
	if (pnldlg == NULL) return -1603;
	name[len] = '\0';
	strcpy((*pnldlg)->name, name);
	(*pnldlg)->handle = reshandle;
	(*pnldlg)->next = NULL;
	/* add to linked list */
	if (firstpnldlg == NULL) firstpnldlg = pnldlg;
	else {
		pnldlgptr = firstpnldlg;
		while ((*pnldlgptr)->next != NULL) pnldlgptr = (*pnldlgptr)->next;
		(*pnldlgptr)->next = pnldlg;
	}
	return 0;
}
#endif

#if OS_WIN32
/*
 * Recursive function for parsing panels and dialogs.
 * Tab groups are what create the need for recursion.
 *
 */
static INT parsepanelsubelements(ELEMENT *element) {
	INT i1, i2, i3, i4, boxtabs[MAXBOXTABS], boxtabscnt, colcount;
	CHAR text[1024], mask[256], *token, boxjust[MAXBOXTABS];
	ATTRIBUTE *a1;
	ELEMENT *e1;
	INT insertorder, justify, noheader;

	while (element != NULL) {
		i1 = i2 = i3 = -1;
		text[0] = '\0';
		if (element->tag[0] == 'p' && element->tag[1] == '\0') {
			a1 = element->firstattribute;
			while (a1 != NULL) {
				if (strcmp(a1->tag, "ha") == 0 || strcmp(a1->tag, "va") == 0) {
					i1 = 1;
					if (a1->value[0] == '-') { /* value can be negative */
						i1 = -1;
						a1->value++;
					}
					i1 *= atoi(a1->value);
					if (strcmp(a1->tag, "ha") == 0) {
						if (guipandlgposha(i1)) return RC_ERROR;
					}
					else {
						if (guipandlgposva(i1)) return RC_ERROR;
					}
				}
				else if (a1->tag[0] == 'h') {
					if (guipandlgposh(atoi(a1->value))) return RC_ERROR;
				}
				else if (a1->tag[0] == 'v') {
					if (guipandlgposv(atoi(a1->value))) return RC_ERROR;
				}
				a1 = a1->nextattribute;
			}
		}
		else if (strcmp(element->tag, "buttongroup") == 0) {
			if (guipandlgbuttongroup()) return RC_ERROR;
			e1 = element->firstsubelement;
			while (e1 != NULL) {
				if (e1->tag[0] == 'p' && e1->tag[1] == '\0') {
					a1 = e1->firstattribute;
					while (a1 != NULL) {
						if (strcmp(a1->tag, "ha") == 0 || strcmp(a1->tag, "va") == 0) {
							i1 = 1;
							if (a1->value[0] == '-') { /* value can be negative */
								i1 = -1;
								a1->value++;
							}
							i1 *= atoi(a1->value);
							if (strcmp(a1->tag, "ha") == 0) {
								if (guipandlgposha(i1)) return RC_ERROR;
							}
							else {
								if (guipandlgposva(i1)) return RC_ERROR;
							}
						}
						else if (a1->tag[0] == 'h') {
							if (guipandlgposh(atoi(a1->value))) return RC_ERROR;
						}
						else if (a1->tag[0] == 'v') {
							if (guipandlgposv(atoi(a1->value))) return RC_ERROR;
						}
						a1 = a1->nextattribute;
					}
				}
				else if (strcmp(e1->tag, "button") == 0) {
					i1 = -1;
					text[0] = '\0';
					a1 = e1->firstattribute;
					while (a1 != NULL) {
						if (a1->tag[0] == 'i') i1 = atoi(a1->value);
						else if (a1->tag[0] == 't') strcpy(text, a1->value);
						a1 = a1->nextattribute;
					}
					if (i1 == -1) return RC_ERROR;
					if (guipandlgbutton(i1, text, (INT)strlen(text))) return RC_ERROR;
				}
				else if (strcmp(e1->tag, "helptext") == 0) {
					if (e1->firstattribute == NULL) return RC_ERROR;
					if (e1->firstattribute->tag[0] != 't' || e1->firstattribute->tag[1] != '\0') return RC_ERROR;
					strcpy(text, e1->firstattribute->value);
					if (guipandlghelptext(text, (INT)strlen(text))) return RC_ERROR;
				}
				e1 = e1->nextelement;
			}
			if (guipandlgendbuttongroup()) return RC_ERROR;
		}
		else if (strcmp(element->tag, "gray") == 0) {
			if (guipandlggray()) return RC_ERROR;
		}
		else if (strcmp(element->tag, "showonly") == 0) {
			if (guipandlgshowonly()) return RC_ERROR;
		}
		else if (strcmp(element->tag, "nofocus") == 0) {
			if (guipandlgnofocus()) return RC_ERROR;
		}
		else if (strcmp(element->tag, "readonly") == 0) {
			if (guipandlgreadonly()) return RC_ERROR;
		}
		else if (strcmp(element->tag, "textcolor") == 0) {
			if (element->firstattribute == NULL) return RC_ERROR;
			strcpy(text, element->firstattribute->value);
			if (guipandlgtextcolor(text, (INT)strlen(text))) return RC_ERROR;
		}
		else if (strcmp(element->tag, "helptext") == 0) {
			if (element->firstattribute == NULL) return RC_ERROR;
			if (element->firstattribute->tag[0] !='t' || element->firstattribute->tag[1] != '\0') return RC_ERROR;
			strcpy(text, element->firstattribute->value);
			if (guipandlghelptext(text, (INT)strlen(text))) return RC_ERROR;
		}
		else if (strcmp(element->tag, "insertorder") == 0) {
			if (guipandlginsertorder()) return RC_ERROR;
		}
		else if (strcmp(element->tag, "justifyleft") == 0) {
			if (guipandlgjustify(CTRLSTYLE_LEFTJUSTIFIED)) return RC_ERROR;
		}
		else if (strcmp(element->tag, "justifyright") == 0) {
			if (guipandlgjustify(CTRLSTYLE_RIGHTJUSTIFIED)) return RC_ERROR;
		}
		else if (strcmp(element->tag, "justifycenter") == 0) {
			if (guipandlgjustify(CTRLSTYLE_CENTERJUSTIFIED)) return RC_ERROR;
		}

		else if (strcmp(element->tag, "tabgroup") == 0) {
			a1 = element->firstattribute;
			i1 = i2 = -1;
			while (a1 != NULL) {
				if (a1->tag[0] == 'h') i1 = atoi(a1->value); /* assume hsize tag */
				else if (a1->tag[0] == 'v') i2 = atoi(a1->value); /* assume vsize tag */
				a1 = a1->nextattribute;
			}
			if (i1 == -1 || i2 == -1) return RC_ERROR;
			if (guipandlgtabgroup(i1, i2)) return RC_ERROR;
			e1 = element->firstsubelement;
			while (e1 != NULL) {
				if (strcmp(e1->tag, "tab") == 0) {
					i1 = -1;
					text[0] = '\0';
					a1 = e1->firstattribute;
					while (a1 != NULL) {
						if (a1->tag[0] == 'i') i1 = atoi(a1->value);
						else if (a1->tag[0] == 't') strcpy(text, a1->value);
						a1 = a1->nextattribute;
					}
					if (i1 == -1) return RC_ERROR;
					if (guipandlgtab(i1, text, (INT)strlen(text))) return RC_ERROR;
					if ((i1 = parsepanelsubelements(e1->firstsubelement)) < 0) return i1;
				}
				else return RC_ERROR; /* only tab is allowed */
				e1 = e1->nextelement;
			}
			if (guipandlgtabgroupend()) return RC_ERROR;
		}
		else if (strcmp(element->tag, "font") == 0) {
			parsefontelement(text, sizeof(text), element->firstattribute, &guiFontParseControl);
			i1 = (INT)strlen(text);
			if (i1 == 0) return RC_ERROR;
			if (guipandlgfont(text, i1)) return RC_ERROR;
		}
		else if (strcmp(element->tag, "icondefpushbutton") == 0 ||
				strcmp(element->tag, "iconpushbutton") == 0 ||
				strcmp(element->tag, "iconescpushbutton") == 0)
		{
			i1 = i2 = i3 = -1;
			text[0] = '\0';
			a1 = element->firstattribute;
			while (a1 != NULL) {
				if (strcmp(a1->tag, "image") == 0) {
					INT vLen = (INT) strlen(a1->value);
					if (vLen > MAXRESNAMELEN) vLen = MAXRESNAMELEN;
					memset(text, ' ', MAXRESNAMELEN);
					memcpy(text, a1->value, vLen);
					text[MAXRESNAMELEN] = '\0';
				}
				else if (a1->tag[0] == 'i') i3 = atoi(a1->value);
				else if (a1->tag[0] == 'h') i1 = atoi(a1->value); /* assume hsize tag */
				else if (a1->tag[0] == 'v') i2 = atoi(a1->value); /* assume vsize tag */
				a1 = a1->nextattribute;
			}
			if (i1 == -1 || i2 == -1 || i3 == -1 || text[0] == '\0') return RC_ERROR;
			if (strcmp(element->tag, "iconpushbutton") == 0) {
				if (guipandlgiconpushbutton(i3, text, MAXRESNAMELEN, i1, i2)) return RC_ERROR;
			}
			else if (strcmp(element->tag, "icondefpushbutton") == 0) {
				if (guipandlgicondefpushbutton(i3, text, MAXRESNAMELEN, i1, i2)) return RC_ERROR;
			}
			else {
				if (guipandlgiconescpushbutton(i3, text, MAXRESNAMELEN, i1, i2)) return RC_ERROR;
			}
		}
		else if (memcmp(element->tag, "icon", 4) == 0) {
			i1 = 0;
			text[0] = '\0';
			a1 = element->firstattribute;
			while (a1 != NULL) {
				if (strcmp(a1->tag, "image") == 0) {
					size_t vLen = strlen(a1->value);
					if (vLen > MAXRESNAMELEN) vLen = MAXRESNAMELEN;
					memset(text, ' ', MAXRESNAMELEN);
					memcpy(text, a1->value, vLen);
					text[MAXRESNAMELEN] = '\0';
				}
				else if (a1->tag[0] == 'i') { /* optional, used by vicon */
					i1 = atoi(a1->value);
				}
				a1 = a1->nextattribute;
			}
			if (text[0] == '\0' || guipandlgicon(i1, text, MAXRESNAMELEN)) return RC_ERROR;
		}
		else if (memcmp(element->tag, "boxtabs", 7) == 0) {
			if (element->firstattribute == NULL) return RC_ERROR;
			a1 = element->firstattribute;
			while (a1 != NULL) {
				if (!memcmp(a1->tag, "tabs", 4)) {
					strcpy(text, a1->value);
					token = strtok(text, ":");
					boxtabscnt = 0;
					while (token != NULL) {
						boxtabs[boxtabscnt++] = parsenumber(token);
						token = strtok(NULL, ":");
					}
				}
				else if (a1->tag[0] == 'j') strcpy(boxjust, a1->value);
				a1 = a1->nextattribute;
			}
			if (guipandlgboxtabs(boxtabs, boxjust, boxtabscnt)) return RC_ERROR;
		}
		else if (memcmp(element->tag, "popbox", 6) == 0) {
			a1 = element->firstattribute;
			i1 = i2 = -1;
			while (a1 != NULL) {
				if (a1->tag[0] == 'i') i1 = atoi(a1->value);
				else if (a1->tag[0] == 'h') i2 = atoi(a1->value); /* assume hsize tag */
				a1 = a1->nextattribute;
			}
			if (i1 == -1 || i2 == -1) return RC_ERROR;
			if (guipandlgpopbox(i1, i2)) return RC_ERROR;
		}
		else if (memcmp(element->tag, "box", 3) == 0) {
			a1 = element->firstattribute;
			i1 = i2 = -1;
			i3 = 0;
			text[0] = '\0';
			while (a1 != NULL) {
				if (a1->tag[0] == 'h') i1 = atoi(a1->value); /* assume hsize tag */
				else if (a1->tag[0] == 'v') i2 = atoi(a1->value); /* assume vsize tag */
				else if (a1->tag[0] == 't') { /* assume title tag */
					strcpy(text, a1->value);
					i3 = TRUE;
				}
				a1 = a1->nextattribute;
			}
			if (i1 == -1 || i2 == -1) return RC_ERROR;
			if (i3) {
				if (guipandlgboxtitle(text, (INT)strlen(text), i1, i2)) return RC_ERROR;
			}
			else {
				if (guipandlgbox(i1, i2)) return RC_ERROR;
			}
		}
		else if (memcmp(element->tag, "text", 4) == 0) {
			if (element->firstattribute == NULL) return RC_ERROR;
			StringCbCopy(text, sizeof(text), element->firstattribute->value);
			if (guipandlgtext(text, (INT)strlen(text))) return RC_ERROR;
		}
		else if (memcmp(element->tag, "checkbox", 8) == 0 ||
			memcmp(element->tag, "ltcheckbox", 10) == 0)
		{
			i1 = -1;
			text[0] = '\0';
			a1 = element->firstattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 'i') i1 = atoi(a1->value);
				else if (a1->tag[0] == 't') StringCbCopy(text, sizeof(text), a1->value);
				a1 = a1->nextattribute;
			}
			if (i1 == -1) return RC_ERROR;
			if (element->tag[0] == 'l') { /* ltcheckbox */
				if (guipandlgltcheckbox(i1, text, (INT)strlen(text))) return RC_ERROR;
			}
			else { /* checkbox */
				if (guipandlgcheckbox(i1, text, (INT)strlen(text))) return RC_ERROR;
			}
		}
		/*
		 * We treat atext as vtext, ratext as rvtext, and catext as cvtext
		 */
		else if (memcmp(element->tag, "vtext", 5) == 0 ||
			memcmp(element->tag, "atext", 5) == 0 ||
			memcmp(element->tag, "rvtext", 6) == 0 ||
			memcmp(element->tag, "ratext", 6) == 0 ||
			memcmp(element->tag, "cvtext", 6) == 0 ||
			memcmp(element->tag, "catext", 6) == 0 ||
			memcmp(element->tag, "ctext", 5) == 0)
		{
			i1 = i2 = -1;
			text[0] = '\0';
			a1 = element->firstattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 'i') i1 = atoi(a1->value);
				else if (a1->tag[0] == 't') StringCbCopy(text, sizeof(text), a1->value);
				else if (a1->tag[0] == 'h') {
					i2 = atoi(a1->value); /* assume ctext/cvtext hsize tag */
				}
				a1 = a1->nextattribute;
			}
			if (i1 == -1 && memcmp(element->tag, "ctext", 5) != 0) return RC_ERROR;
			if (element->tag[0] == 'v' || element->tag[0] == 'a') { /* vtext  || atext */
				if (guipandlgvtext(i1, text, (INT)strlen(text))) return RC_ERROR;
			}
			else if (element->tag[0] == 'c') {
				if (element->tag[1] == 'v' || element->tag[1] == 'a') { /* cvtext  || catext */
					if (guipandlgcvtext(i1, text, (INT)strlen(text), i2)) return RC_ERROR;
				}
				else { /* ctext */
					if (guipandlgctext(text, (INT)strlen(text), i2)) return RC_ERROR;
				}
			}
			else {
				if (guipandlgrvtext(i1, text, (INT)strlen(text))) return RC_ERROR;
			}
		}
		else if (memcmp(element->tag, "rtext", 5) == 0) {
			if (element->firstattribute == NULL) return RC_ERROR;
			strcpy(text, element->firstattribute->value);
			if (guipandlgrtext(text, (INT)strlen(text))) return RC_ERROR;
		}
		else if (memcmp(element->tag, "edit", 4) == 0 ||
			memcmp(element->tag, "fedit", 5) == 0 ||
			memcmp(element->tag, "pedit", 5) == 0 ||
			memcmp(element->tag, "mledit", 6) == 0 ||	/* includes mledit, mlediths, mleditvs, mledits */
			memcmp(element->tag, "pledit", 6) == 0 ||
			memcmp(element->tag, "ledit", 5) == 0 ||
			memcmp(element->tag, "medit", 5) == 0)		/* includes medit, mediths, meditvs, medits */
		{
			i1 = i2 = i3 = i4 = -1;
			text[0] = mask[0] = '\0';
			a1 = element->firstattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 'i') i1 = atoi(a1->value);
				else if (a1->tag[0] == 't') StringCbCopy(text, sizeof(text), a1->value);
				else if (a1->tag[0] == 'h') i2 = atoi(a1->value); /* assume hsize tag */
				else if (a1->tag[0] == 'v') i3 = atoi(a1->value); /* assume vsize tag */
				else if (memcmp(a1->tag, "maxchars", 8) == 0) i4 = atoi(a1->value);
				else if (memcmp(a1->tag, "mask", 4) == 0) strcpy(mask, a1->value);
				a1 = a1->nextattribute;
			}
			if (i1 == -1) return RC_ERROR;
			if (memcmp(element->tag, "edit", 4) == 0) {
				if (i2 == -1) return RC_ERROR;
				if (guipandlgedit(i1, text, (INT)strlen(text), i2)) return RC_ERROR;
			}
			else if (memcmp(element->tag, "fedit", 5) == 0) {
				if (i2 == -1) return RC_ERROR;
				if (guipandlgfedit(i1, (UCHAR *) text, (INT)strlen(text), (UCHAR *) mask, (INT)strlen(mask), i2))
					return RC_ERROR;
			}
			else if (memcmp(element->tag, "pedit", 5) == 0) {
				if (i2 == -1) return RC_ERROR;
				if (guipandlgpedit(i1, (UCHAR *) text, (INT)strlen(text), i2)) return RC_ERROR;
			}
			else if (memcmp(element->tag, "mlediths", 8) == 0) {
				if (i2 == -1 || i3 == -1 || i4 == -1) return RC_ERROR;
				if (guipandlgmledit(i1, (UCHAR *) text, (INT)strlen(text), i2, i3, i4, TRUE, FALSE)) return RC_ERROR;
			}
			else if (memcmp(element->tag, "mleditvs", 8) == 0) {
				if (i2 == -1 || i3 == -1 || i4 == -1) return RC_ERROR;
				if (guipandlgmledit(i1, (UCHAR *) text, (INT)strlen(text), i2, i3, i4, FALSE, TRUE)) return RC_ERROR;
			}
			else if (memcmp(element->tag, "mledits", 7) == 0) {
				if (i2 == -1 || i3 == -1 || i4 == -1) return RC_ERROR;
				if (guipandlgmledit(i1, (UCHAR *) text, (INT)strlen(text), i2, i3, i4, TRUE, TRUE)) return RC_ERROR;
			}
			else if (memcmp(element->tag, "mledit", 6) == 0) {
				if (i2 == -1 || i3 == -1 || i4 == -1) return RC_ERROR;
				if (guipandlgmledit(i1, (UCHAR *) text, (INT)strlen(text), i2, i3, i4, FALSE, FALSE)) return RC_ERROR;
			}
			else if (memcmp(element->tag, "ledit", 5) == 0) {
				if (i2 == -1 || i4 == -1) return RC_ERROR;
				if (guipandlgledit(i1, (UCHAR *) text, (INT)strlen(text), i2, i4)) return RC_ERROR;
			}
			else if (memcmp(element->tag, "pledit", 6) == 0) {
				if (i2 == -1 || i4 == -1) return RC_ERROR;
				if (guipandlgpledit(i1, (UCHAR *) text, (INT)strlen(text), i2, i4)) return RC_ERROR;
			}
			else if (memcmp(element->tag, "mediths", 7) == 0) {
				if (i2 == -1 || i3 == -1) return RC_ERROR;
				if (guipandlgmediths(i1, (UCHAR *) text, (INT)strlen(text), i2, i3)) return RC_ERROR;
			}
			else if (memcmp(element->tag, "meditvs", 7) == 0) {
				if (i2 == -1 || i3 == -1) return RC_ERROR;
				if (guipandlgmeditvs(i1, (UCHAR *) text, (INT)strlen(text), i2, i3)) return RC_ERROR;
			}
			else if (memcmp(element->tag, "medits", 6) == 0) {
				if (i2 == -1 || i3 == -1) return RC_ERROR;
				if (guipandlgmedits(i1, (UCHAR *) text, (INT)strlen(text), i2, i3)) return RC_ERROR;
			}
			else if (memcmp(element->tag, "medit", 5) == 0) {
				if (i2 == -1 || i3 == -1) return RC_ERROR;
				if (guipandlgmedit(i1, (UCHAR *) text, (INT)strlen(text), i2, i3)) return RC_ERROR;
			}
		}
		else if (memcmp(element->tag, "pushbutton", 10) == 0 ||
			memcmp(element->tag, "defpushbutton", 13) == 0 ||
			memcmp(element->tag, "escpushbutton", 13) == 0)
		{
			i1 = i2 = i3 = -1;
			text[0] = '\0';
			a1 = element->firstattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 't') StringCbCopy(text, sizeof(text), a1->value);
				else if (a1->tag[0] == 'i') i3 = atoi(a1->value);
				else if (a1->tag[0] == 'h') i1 = atoi(a1->value); /* assume hsize tag */
				else if (a1->tag[0] == 'v') i2 = atoi(a1->value); /* assume vsize tag */
				a1 = a1->nextattribute;
			}
			if (i1 == -1 || i2 == -1 || i3 == -1) return RC_ERROR;
			if (memcmp(element->tag, "pushbutton", 10) == 0) {
				if (guipandlgpushbutton(i3, text, (INT)strlen(text), i1, i2)) return RC_ERROR;
			}
			else if (memcmp(element->tag, "defpushbutton", 13) == 0) {
				if (guipandlgdefpushbutton(i3, text, (INT)strlen(text), i1, i2)) return RC_ERROR;
			}
			else {
				if (guipandlgescpushbutton(i3, text, (INT)strlen(text), i1, i2)) return RC_ERROR;
			}
		}
		else if (strcmp(element->tag, "dropbox") == 0 ||
			strcmp(element->tag, "combobox") == 0 ||
			strcmp(element->tag, "listboxhs") == 0 ||
			strcmp(element->tag, "listbox") == 0 ||
			strcmp(element->tag, "mlistboxhs") == 0 ||
			strcmp(element->tag, "mlistbox") == 0 ||
			strcmp(element->tag, "hscrollbar") == 0 ||
			strcmp(element->tag, "vscrollbar") == 0 ||
			strcmp(element->tag, "progressbar") == 0 ||
			strcmp(element->tag, "tree") == 0)
		{
			i1 = i2 = i3 = -1;
			a1 = element->firstattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 'i') i1 = atoi(a1->value);
				else if (a1->tag[0] == 'h') i2 = atoi(a1->value); /* assume hsize tag */
				else if (a1->tag[0] == 'v') i3 = atoi(a1->value); /* assume vsize tag */
				a1 = a1->nextattribute;
			}
			if (i1 == -1 || i2 == -1 || i3 == -1) return RC_ERROR;
			if (element->tag[0] == 'd') { /* dropbox */
				if (guipandlgdropbox(i1, i2, i3)) return RC_ERROR;
			}
			else if (!strcmp(element->tag, "listboxhs")) { /* listboxhs */
				if (guipandlglistboxhs(i1, i2, i3)) return RC_ERROR;
			}
			else if (!strcmp(element->tag, "listbox")) { /* listbox */
				if (guipandlglistbox(i1, i2, i3)) return RC_ERROR;
			}
			else if (!strcmp(element->tag, "mlistboxhs")) { /* mlistboxhs */
				if (guipandlgmlistboxhs(i1, i2, i3)) return RC_ERROR;
			}
			else if (!strcmp(element->tag, "mlistbox")) { /* mlistbox */
				if (guipandlgmlistbox(i1, i2, i3)) return RC_ERROR;
			}
			else if (element->tag[0] == 'h') { /* hscrollbar */
				if (guipandlghscrollbar(i1, i2, i3)) return RC_ERROR;
			}
			else if (element->tag[0] == 'v') { /* vscrollbar */
				if (guipandlgvscrollbar(i1, i2, i3)) return RC_ERROR;
			}
			else if (element->tag[0] == 'p') { /* progressbar */
				if (guipandlgprogressbar(i1, i2, i3)) return RC_ERROR;
			}
			else if (element->tag[0] == 't') { /* tree */
				if (guipandlgtree(i1, i2, i3)) return RC_ERROR;
			}
		}
		else if (strcmp(element->tag, "table") == 0) {
			i1 = i2 = i3 = i4 = -1;
			noheader = FALSE;
			a1 = element->firstattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 'i') i1 = atoi(a1->value);
				else if (a1->tag[0] == 'h') i2 = atoi(a1->value); /* assume hsize tag */
				else if (a1->tag[0] == 'v') i3 = atoi(a1->value); /* assume vsize tag */
				else if (strcmp(a1->tag, "nh") == 0) {
					if (strcmp(a1->value, "true") == 0) noheader = TRUE; /* no header */
				}
				a1 = a1->nextattribute;
			}
			if (i1 == -1 || i2 == -1 || i3 == -1) return RC_ERROR;
			if (guipandlgtable(i1, i2, i3)) return RC_ERROR;
			if (noheader) {
				if (guipandlgtablenoheader()) return RC_ERROR;
			}

			if (element->firstsubelement != NULL) {
				e1 = element->firstsubelement;
				colcount = 0;
				while (e1 != NULL) {
					if (strcmp(e1->tag, "c") == 0) {
						a1 = e1->firstattribute;
						i1 = i2 = i3 = i4 = -1;
						insertorder = FALSE;  /* This is the default */
						justify = -1;
						text[0] = '\0';
						while (a1 != NULL) {
							if (a1->tag[0] == 'i') i1 = atoi(a1->value);
							else if (a1->tag[0] == 'h') i2 = atoi(a1->value);
							else if (a1->tag[0] == 'c') i3 = atoi(a1->value);
							else if (a1->tag[0] == 't') StringCbCopy(text, sizeof(text), a1->value);
							else if (strcmp(a1->tag, "p3") == 0) i4 = atoi(a1->value);
							else if (strcmp(a1->tag, "order") == 0) {
								if (strcmp(a1->value, "insert") == 0) insertorder = TRUE;
							}
							else if (strcmp(a1->tag, "justify") == 0) {
								if (strcmp(a1->value, "left") == 0) justify = CTRLSTYLE_LEFTJUSTIFIED;
								else if (strcmp(a1->value, "right") == 0) justify = CTRLSTYLE_RIGHTJUSTIFIED;
								else if (strcmp(a1->value, "center") == 0) justify = CTRLSTYLE_CENTERJUSTIFIED;
							}
							a1 = a1->nextattribute;
						}
						if (i1 == -1 || i2 == -1 || i3 == -1) return RC_ERROR;
						if (guipandlgtablecolumntitle(text, (INT)strlen(text))) return RC_ERROR;
						if (guipandlgtablecolumn(colcount++, i1, i2, i3, i4, NULL, 0)) return RC_ERROR;
						if (insertorder) if (guipandlgtableinsertorder()) return RC_ERROR;
						if (justify != -1) guipandlgtablejustify(justify);
					}
					else if (strcmp(e1->tag, "insertorder") == 0) {
						if (guipandlgtableinsertorder()) return RC_ERROR;
					}
					else if (strcmp(e1->tag, "justifyleft") == 0) {
						if (guipandlgtablejustify(CTRLSTYLE_LEFTJUSTIFIED)) return RC_ERROR;
					}
					else if (strcmp(e1->tag, "justifyright") == 0) {
						if (guipandlgtablejustify(CTRLSTYLE_RIGHTJUSTIFIED)) return RC_ERROR;
					}
					else if (strcmp(e1->tag, "justifycenter") == 0) {
						if (guipandlgtablejustify(CTRLSTYLE_CENTERJUSTIFIED)) return RC_ERROR;
					}
					else if (strcmp(e1->tag, "noheader") == 0) {
						if (guipandlgtablenoheader()) return RC_ERROR;
					}
					e1 = e1->nextelement;
				}
			}
		}
		else return -300;
		element = element->nextelement;
	}
	return 0;
}
#endif

#if OS_WIN32
static void parsefontelement(CHAR *font, size_t cbFont, ATTRIBUTE *a1,
		struct _guiFontParseControl *fontParseControl) {
	CHAR work[4];
	INT i1;
	BOOL nameChanged = FALSE;
	BOOL ulChanged = FALSE;
	BOOL italChanged = FALSE;
	BOOL boldChanged = FALSE;
	BOOL sizeChanged = FALSE;
	BOOL needComma = FALSE;

	while (a1 != NULL) {
		if (a1->tag[0] == 'n') {
			if (_stricmp(fontParseControl->currentFontName, a1->value) != 0) {
				StringCbCopy(fontParseControl->currentFontName, sizeof(fontParseControl->currentFontName), a1->value);
				nameChanged = TRUE;
			}
		}
		else if (a1->tag[0] == 's') {
			i1 = atoi(a1->value);
			if (fontParseControl->currentSize != i1) {
				fontParseControl->currentSize = i1;
				sizeChanged = TRUE;
			}
		}
		else if (a1->tag[0] == 'i') /* assume italic tag */ {
			i1 = (a1->value[0] == 'y');
			if (fontParseControl->currentItalic != i1) {
				fontParseControl->currentItalic = i1;
				italChanged = TRUE;
			}
		}
		else if (a1->tag[0] == 'b') /* assume bold tag */ {
			i1 = (a1->value[0] == 'y');
			if (fontParseControl->currentBold != i1) {
				fontParseControl->currentBold = i1;
				boldChanged = TRUE;
			}
		}
		else if (a1->tag[0] == 'u') /* assume underline tag */ {
			i1 = (a1->value[0] == 'y');
			if (fontParseControl->currentUnderline != i1) {
				fontParseControl->currentUnderline= i1;
				ulChanged = TRUE;
			}
		}
		a1 = a1->nextattribute;
	}
	if (nameChanged) StringCbCopy(font, cbFont, fontParseControl->currentFontName);
	else font[0] = '\0';

#if 0
	/* A prep string item like 'font=default(underline)'
	 * works fine in the windows DX and so should work fine here.
	 * So do *not* return if the font name is 'default'.
	 */
	if (!stricmp("default", name)) return;
#endif
	StringCbCat(font, cbFont, "(");

	if (italChanged) {
		StringCbCat(font, cbFont,
				(fontParseControl->currentItalic) ? "italic" : "noitalic");
		needComma = TRUE;
	}

	if (boldChanged) {
		if (needComma) StringCbCat(font, cbFont, ",");
		StringCbCat(font, cbFont,
				(fontParseControl->currentBold) ? "bold" : "nobold");
		needComma = TRUE;
	}

	if (ulChanged) {
		if (needComma) StringCbCat(font, cbFont, ",");
		StringCbCat(font, cbFont,
				(fontParseControl->currentUnderline) ? "underline" : "nounderline");
		needComma = TRUE;
	}

	if (sizeChanged) {
		if (needComma) StringCbCat(font, cbFont, ",");
		mscitoa(fontParseControl->currentSize, work);
		StringCbCat(font, cbFont, work);
	}
	StringCbCat(font, cbFont, ")");
}
#endif

/*
 * Recursive function for parsing sub menus
 *
 */
#if OS_WIN32
static void getMenuElementAttributes(ELEMENT *element, CHAR *text, size_t cchText, INT *i1, INT *i2, CHAR *iconName){
	ATTRIBUTE *a1;
	a1 = element->firstattribute;
	while (a1 != NULL) {
		if (a1->tag[0] == 't') {
			if (strlen(a1->value) < cchText) StringCchCopy(text, cchText, a1->value);
			else {
				memcpy(text, a1->value, (cchText - 1) * sizeof(CHAR));
				text[cchText - 1] = '\0';
			}
		}
		else if (strcmp(a1->tag, "i") == 0) *i1 = atoi(a1->value);
		else if (a1->tag[0] == 'k') *i2 = parsespeedkey(a1->value);
		else if (strcmp(a1->tag, "icon") == 0) strcpy(iconName, a1->value);
		a1 = a1->nextattribute;
	}
}

static INT parsemenusubelements(ELEMENT *element)
{
	INT i1, i2;
	CHAR text[1024], iconName[12];
	size_t cchText = sizeof(text) / sizeof(CHAR);

	while (element != NULL) {
		i1 = i2 = 0;
		text[0] = '\0';
		iconName[0] = '\0';
		if (strcmp(element->tag, "mitem") == 0) {
			getMenuElementAttributes(element, text, cchText, &i1, &i2, iconName);
			if (guimenuitem(i1, text, (INT)strlen(text))) return RC_ERROR;
			if (i2 != 0) {
				if (guimenukey(i2)) return RC_ERROR;
			}
		}
		else if (strcmp(element->tag, "iconitem") == 0) {
			getMenuElementAttributes(element, text, cchText, &i1, &i2, iconName);
			if (iconName[0] == '\0') return RC_ERROR;
			if (guimenuiconitem(i1, text, (INT)strlen(text), iconName, (INT)strlen(iconName))) return RC_ERROR;
			if (i2 != 0) {
				if (guimenukey(i2)) return RC_ERROR;
			}
		}
		else if (strcmp(element->tag, "submenu") == 0 || strcmp(element->tag, "iconsubmenu") == 0) {
			getMenuElementAttributes(element, text, cchText, &i1, &i2, iconName);
			if (guimenusubmenu(i1, text, (INT)strlen(text), iconName, (INT)strlen(iconName))) return RC_ERROR;
			if (element->firstsubelement != NULL) {
				if ((i1 = parsemenusubelements(element->firstsubelement)) < 0) return i1;
			}
			if (guimenuendsubmenu()) return RC_ERROR;
		}
		else if (strcmp(element->tag, "line") == 0) {
			if (guimenuline()) return RC_ERROR;
		}
		else if (strcmp(element->tag, "gray") == 0) {
			if (guimenugray()) return -301;
		}
		element = element->nextelement;
	}
	return 0;
}
#endif

#if OS_WIN32
static INT parsespeedkey(CHAR *scanstring)
{
	INT i1 = 0;
	switch(toupper(scanstring[0])) {
		case '^':  /* Control key A-Z */
			i1 = toupper(scanstring[1]);
			if (i1 < 'A' || i1 > 'Z' || scanstring[2]) {
				i1 = 0;
			}
			else i1 = i1 - 'A' + 601;
			break;
		case 'A': /* Alt key F1-F12 */
			if (!ctlmemicmp(scanstring, "ALT", 3)) {
				i1 = toupper(scanstring[3]);
				if (i1 == 'F' && scanstring[4]) {
					i1 = atoi((CHAR *) &scanstring[4]);
					if (i1 > 12) i1 = 0;
					else (i1 += 360);
				}
				else i1 = 0;
			}
			else i1 = 0;
			break;
		case 'C': /* CTRL-SHIFT F1-F12, A-Z; CTRL A-Z */
			if (!ctlmemicmp(scanstring, "CTRLSHIFT", 9)) {
				i1 = toupper(scanstring[9]);
				if (i1 == 'F' && scanstring[10]) {
					i1 = atoi((CHAR *) &scanstring[10]);
					if (i1 > 12) i1 = 0;
					else (i1 += 519);
				}
				else {
					i1 = toupper(scanstring[9]);
					if (i1 < 'A' || i1 > 'Z' || scanstring[10]) {
						i1 = 0;
					}
					else i1 = i1 - 'A' + 540;
				}
			}
			else if (!ctlmemicmp(scanstring, "CTRL", 4)) {
				i1 = toupper(scanstring[4]);
				if (scanstring[5]) {
					switch (i1) {
					case 'C':
						if (!ctlmemicmp(scanstring, "CTRLCOMMA", 9)) i1 = KEYCOMMA;
						else i1 = 0;
						break;
					case 'P':
						if (!ctlmemicmp(scanstring, "CTRLPERIOD", 10)) i1 = KEYPERIOD;
						else
						i1 = 0;
						break;
					case 'F':
						if (!ctlmemicmp(scanstring, "CTRLFSLASH", 10)) i1 = KEYFSLASH;
						else if (isdigit(scanstring[5])) {
							/* code here for CTRLF1 thru CTRLF12 */
							i1 = atoi((CHAR *) &scanstring[5]);
							if (i1 > 12) i1 = 0;
							else (i1 += 340);
						}
						else i1 = 0;
						break;
					case 'S':
						if (!ctlmemicmp(scanstring, "CTRLSEMICOLON", 13)) i1 = KEYSEMICOLON;
						else i1 = 0;
						break;
					case 'L':
						if (!ctlmemicmp(scanstring, "CTRLLBRACKET", 12)) i1 = KEYLBRACKET;
						else i1 = 0;
						break;
					case 'R':
						if (!ctlmemicmp(scanstring, "CTRLRBRACKET", 12)) i1 = KEYRBRACKET;
						else i1 = 0;
						break;
					case 'B':
						if (!ctlmemicmp(scanstring, "CTRLBSLASH", 10)) i1 = KEYBSLASH;
						else i1 = 0;
						break;
					case 'M':
						if (!ctlmemicmp(scanstring, "CTRLMINUS", 9)) i1 = KEYMINUS;
						else i1 = 0;
						break;
					case 'E':
						if (!ctlmemicmp(scanstring, "CTRLEQUAL", 9)) i1 = KEYEQUAL;
						else i1 = 0;
						break;
					case 'Q':
						if (!ctlmemicmp(scanstring, "CTRLQUOTE", 9)) i1 = KEYQUOTE;
						else i1 = 0;
						break;
					default: i1 = 0;
					}
				}
				else if (i1 < 'A' || i1 > 'Z' || scanstring[5]) {
					i1 = 0;
				}
				else i1 = i1 - 'A' + 601;
			}
			else i1 = 0;
			break;
		case 'D': /* Delete */
			if (!ctlmemicmp(scanstring, "DELETE", 6)) i1 = KEYDELETE;
			else i1 = 0;
			break;
		case 'E': /* End */
			if (!ctlmemicmp(scanstring, "END", 3)) i1 = KEYEND;
			else i1 = 0;
			break;
		case 'F':  /* Function key */
			i1 = atoi((CHAR *) &scanstring[1]);
			if (i1 > 12) i1 = 0;
			else (i1 += 300);
			break;
		case 'H': /* Home */
			if (!ctlmemicmp(scanstring, "HOME", 4))	i1 = KEYHOME;
			else i1 = 0;
			break;
		case 'I': /* Insert */
			if (!ctlmemicmp(scanstring, "INSERT", 6)) i1 = KEYINSERT;
			else i1 = 0;
			break;
		case 'P': /* Page Up/Down */
			if (!ctlmemicmp(scanstring, "PGUP", 4)) i1 = KEYPGUP;
			else if (!ctlmemicmp(scanstring, "PGDN", 4)) i1 = KEYPGDN;
			else i1 = 0;
			break;
		case 'S': /* Shift key F1-F12 */
			if (!ctlmemicmp(scanstring, "SHIFT", 5)) {
				i1 = toupper(scanstring[5]);
				if (i1 == 'F' && scanstring[6]) {
					i1 = atoi((CHAR *) &scanstring[6]);
					if (i1 > 12) i1 = 0;
					else (i1 += 320);
				}
				else i1 = 0;
			}
			else i1 = 0;
			break;
		default:
			i1 = 0;
			break;
	}
	return i1;
}
#endif

#if OS_WIN32
static INT dodraw(ELEMENT *e1)
{
	INT i1, h1, h2, v1, v2, r1, g1, b1, r2, g2, b2;
	CHAR text[1024];

	ATTRIBUTE *a1 = e1->firstattribute, *imageAttr;
	IMAGEREF **imageptr, **imageptr2;

#if OS_WIN32GUI
	if (!guiflag) {
		if (cnsflag || guiCSCinit(breakeventid, &guiFontParseControl) != 0) {
			return -2003;
		}
		guiflag = TRUE;
	}
#endif

	imageptr = findImageRef(a1);
	if (imageptr == NULL) return 0;
	guipixdrawstart((*imageptr)->handle);
	e1 = e1->firstsubelement;
	initializeFontParseControl(TRUE);
	while (e1 != NULL) {
		a1 = e1->firstattribute;
		if (e1->tag[0] == 'p' && strlen(e1->tag) == 1) {
			while (a1 != NULL) {
				if (strcmp(a1->tag, "ha") == 0 || strcmp(a1->tag, "va") == 0) {
					i1 = strtol(a1->value, NULL, 0);
					if (a1->tag[0] == 'h') {
						guipixdrawhadj(i1);
					}
					else {
						guipixdrawvadj(i1);
					}
				}
				else if (a1->tag[0] == 'h') {
					guipixdrawhpos(strtol(a1->value, NULL, 0) - 1);
				}
				else if (a1->tag[0] == 'v') {
					guipixdrawvpos(strtol(a1->value, NULL, 0) - 1);
				}
				a1 = a1->nextattribute;
			}
		}
		else if (memcmp(e1->tag, "color", 5) == 0) {
			r1 = g1 = b1 = 0;
			while (a1 != NULL) {
				if (a1->tag[0] == 'r') r1 = atoi(a1->value);
				else if (a1->tag[0] == 'g') g1 = atoi(a1->value);
				else if (a1->tag[0] == 'b') b1 = atoi(a1->value);
				a1 = a1->nextattribute;
			}
			guipixdrawcolor(r1, g1, b1);
		}
		else if (memcmp(e1->tag, "erase", 5) == 0) {
			guipixdrawerase();
		}
		else if (memcmp(e1->tag, "dot", 3) == 0) {
			guipixdrawdot(0);
		}
		else if (memcmp(e1->tag, "bigdot", 6) == 0) {
			if (e1->firstattribute == NULL) return -2000;
			guipixdrawdot(atoi(e1->firstattribute->value)); /* assume s tag */
		}
		else if (memcmp(e1->tag, "brush", 5) == 0) {
			a1 = e1->firstattribute;
			if (a1->tag[0] == 'w') { /* line width */
				guipixdrawlinewidth(atoi(a1->value));
			}
			else if (a1->tag[0] == 't') { /* line type */
				if (memcmp(a1->value, "solid", 5) == 0) guipixdrawlinetype(DRAW_LINE_SOLID);
				else guipixdrawlinetype(DRAW_LINE_REVDOT);
			}
			else return -2000;
		}
		else if (strcmp(e1->tag, "line") == 0) {
			a1 = e1->firstattribute;
			if (a1 == NULL || a1->nextattribute == NULL) return -2000;
			/* assume h then v values */
			guipixdrawline(atoi(a1->value) - 1, atoi(a1->nextattribute->value) - 1);
		}
		else if (memcmp(e1->tag, "circle", 6) == 0) {
			if (e1->firstattribute == NULL) return -2000;
			guipixdrawcircle(atoi(e1->firstattribute->value));
		}
		else if (memcmp(e1->tag, "dbox", 4) == 0) {
			a1 = e1->firstattribute;
			if (a1 == NULL || a1->nextattribute == NULL) return -2000;
			/* assume h then v values */
			guipixdrawbox(atoi(a1->value) - 1, atoi(a1->nextattribute->value) - 1);
		}
		else if (strcmp(e1->tag, "rectangle") == 0) {
			a1 = e1->firstattribute;
			if (a1 == NULL || a1->nextattribute == NULL) return -2000;
			while (a1 != NULL) {
				if (a1->tag[0] == 'h') h1 = atoi(a1->value);
				else if (a1->tag[0] == 'v') v1 = atoi(a1->value);
				a1 = a1->nextattribute;
			}
			/* The h and v are zero-based over the wire */
			guipixdrawrectangle(h1, v1);
		}
		else if (memcmp(e1->tag, "triangle", 8) == 0) {
			h1 = h2 = v1 = v2 = 1;
			a1 = e1->firstattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 'h' && a1->tag[1] == '1') h1 = atoi(a1->value);
				else if (a1->tag[0] == 'h' && a1->tag[1] == '2') h2 = atoi(a1->value);
				else if (a1->tag[0] == 'v' && a1->tag[1] == '1') v1 = atoi(a1->value);
				else if (a1->tag[0] == 'v' && a1->tag[1] == '2') v2 = atoi(a1->value);
				a1 = a1->nextattribute;
			}
			/* allow negative values */
			/* if (h1 < 0 || h2 < 0 || v1 < 0 || v2 < 0) return -2000; */
			guipixdrawtriangle(h1, v1, h2, v2);
		}
		else if (memcmp(e1->tag, "font", 4) == 0) {
			parsefontelement(text, sizeof(text), e1->firstattribute, &guiFontParseControl);
			guipixdrawfont((UCHAR *) text, (INT)strlen(text));
		}
		else if (memcmp(e1->tag, "linefeed", 8) == 0) {
			guipixgetpos((*imageptr)->handle, &h1, &v1);
			guipixgetfontsize((*imageptr)->handle, &h1, &v2);
			guipixdrawvpos(v1 + h1);
		}
		else if (memcmp(e1->tag, "newline", 7) == 0) {
			guipixgetpos((*imageptr)->handle, &h1, &v1);
			guipixgetfontsize((*imageptr)->handle, &h1, &v2);
			guipixdrawpos(0, v1 + h1);
		}
		else if (memcmp(e1->tag, "text", 4) == 0) {
			a1 = e1->firstattribute;
			if (a1->tag[0] != 't') return -2000;
			guipixdrawtext((UCHAR *) a1->value, (INT)strlen(a1->value), DRAW_TEXT_LEFT);
		}
		else if (memcmp(e1->tag, "atext", 5) == 0) {
		    text[0] = '\0';
		    i1 = INT_MIN;
			a1 = e1->firstattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 't') StringCbCopy(text, sizeof(text), a1->value);
				else if (memcmp(a1->tag, "n", 5) == 0) i1 = atoi(a1->value);
				else return -2000;
				a1 = a1->nextattribute;
			}
			if (i1 == INT_MIN) return -2000;
			//if (a1->tag[0] != 't') return -2000;
			guipixdrawatext((UCHAR *) text, (INT)strlen(text), i1);
		}
		else if (memcmp(e1->tag, "ctext", 5) == 0) {
			/* t and hsize tags */
		    text[0] = '\0';
		    i1 = 0;
			a1 = e1->firstattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 't') StringCbCopy(text, sizeof(text), a1->value);
				else if (memcmp(a1->tag, "hsize", 5) == 0) i1 = atoi(a1->value);
				else return -2000;
				a1 = a1->nextattribute;
			}
			if (i1 == 0) return -2000;
			guipixgetpos((*imageptr)->handle, &h1, &v1);
			i1 /= 2;
			guipixdrawhadj(i1);
			guipixdrawtext((UCHAR *) text, (INT)strlen(text), DRAW_TEXT_CENTER);
			guipixdrawpos(h1, v1);
		}
		else if (memcmp(e1->tag, "rtext", 5) == 0) {
			a1 = e1->firstattribute;
			if (a1->tag[0] != 't') return -2000;
			guipixdrawtext((UCHAR *) a1->value, (INT)strlen(a1->value), DRAW_TEXT_RIGHT);
		}
		else if (memcmp(e1->tag, "dimage", 6) == 0) {
			strcpy(text, e1->firstattribute->value); /* assume image tag */
			imageptr2 = findImageRef(a1);
			if (imageptr2 == NULL) return -2000;
			guipixdrawcopyimage((*imageptr2)->handle);
		}
		else if (memcmp(e1->tag, "clipimage", 9) == 0) {
			h1 = h2 = v1 = v2 = -1;
			text[0] = '\0';
			a1 = e1->firstattribute;
			while (a1 != NULL) {
				if (memcmp(a1->tag, "image", 5) == 0) {
					strcpy(text, a1->value);
					imageAttr = a1;
				}
				else if (a1->tag[0] == 'h' && a1->tag[1] == '1') h1 = atoi(a1->value);
				else if (a1->tag[0] == 'h' && a1->tag[1] == '2') h2 = atoi(a1->value);
				else if (a1->tag[0] == 'v' && a1->tag[1] == '1') v1 = atoi(a1->value);
				else if (a1->tag[0] == 'v' && a1->tag[1] == '2') v2 = atoi(a1->value);
				a1 = a1->nextattribute;
			}
			//if (text[0] == 0 || h1 < 0 || h2 < 0 || v1 < 0 || v2 < 0) return -2000;
			if (text[0] == '\0' /* Allow negative positions */) return -2000;
			imageptr2 = findImageRef(imageAttr);
			if (imageptr2 == NULL) return -2000;
			guipixdrawclipimage((*imageptr2)->handle, h1 - 1, v1 - 1, h2 - 1, v2 - 1);
		}
		else if (memcmp(e1->tag, "stretchimage", 12) == 0) {
			h1 = v1 = -1;
			text[0] = '\0';
			a1 = e1->firstattribute;
			while (a1 != NULL) {
				if (memcmp(a1->tag, "image", 5) == 0) {
					strcpy(text, a1->value);
					imageAttr = a1;
				}
				else if (a1->tag[0] == 'h' && a1->tag[1] == '1') h1 = strtol(a1->value, NULL, 10);
				else if (a1->tag[0] == 'v' && a1->tag[1] == '1') v1 = strtol(a1->value, NULL, 10);
				a1 = a1->nextattribute;
			}
			if (text[0] == '\0' || h1 < 0 || v1 < 0) return -2000;
			imageptr2 = findImageRef(imageAttr);
			if (imageptr2 == NULL) return -2000;
			guipixdrawstretchimage((*imageptr2)->handle, h1, v1);
		}
		else if (memcmp(e1->tag, "irotate", 7) == 0) {
			a1 = e1->firstattribute;
			if (a1 == NULL || a1->tag == NULL || a1->tag[0] != 'n') {
				return -2000;
			}
			guipixdrawirotate(atoi(a1->value));
		}
		else if (memcmp(e1->tag, "replace", 7) == 0) {
			r1 = g1 = b1 = r2 = g2 = b2 = 0;
			a1 = e1->firstattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 'r') r1 = atoi(a1->value);
				else if (a1->tag[0] == 'g') g1 = atoi(a1->value);
				else if (a1->tag[0] == 'b') {
					b1 = atoi(a1->value);
					break;
				}
				a1 = a1->nextattribute;
			}
			a1 = a1->nextattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 'r') r2 = atoi(a1->value);
				else if (a1->tag[0] == 'g') g2 = atoi(a1->value);
				else if (a1->tag[0] == 'b') b2 = atoi(a1->value);
				a1 = a1->nextattribute;
			}
			guipixdrawreplace((b1 << 16) + (g1 << 8) + r1, (b2 << 16) + (g2 << 8) + r2);
		}
		else return -2000;
		e1 = e1->nextelement;
	}
	guipixdrawend();
	return 0;
}
#endif

#if OS_WIN32
static INT doshow(ELEMENT *e1)
{
	INT i1, i2, i3;

	WINDOWREF **winptr;
	IMAGEREF **imageptr;
	RESOURCEREF **menuptr, **toolbarptr, **pnldlgptr;
	ATTRIBUTE *a1;
	BOOL defpos;		/* use default position flag */

	if (!guiflag) {
		if (cnsflag || guiCSCinit(breakeventid, &guiFontParseControl) != 0) {
			return -2003;
		}
		guiflag = TRUE;
	}

	winptr = firstwindow;
	i1 = i2 = -1;
	a1 = e1->firstattribute;
	if (a1->nextattribute == NULL) return -2000;
	while (winptr != NULL) { /* search for window in linked list */
		/* WARNING:
		 * This code assumes that the second attribute will always be
		 * the window name
		 */
		if (strcmp((*winptr)->name, a1->nextattribute->value) == 0) break;
		winptr = (*winptr)->next;
	}
	if (winptr == NULL) return sendresult(1);
	if ((strcmp(a1->tag, "menu") == 0) || (strcmp(a1->tag, "popupmenu") == 0)) {
		menuptr = findResourceRef(a1);
		a1 = a1->nextattribute->nextattribute;
		while (a1 != NULL) {
			if (a1->tag[0] == 'h') i1 = atoi(a1->value);
			else if (a1->tag[0] == 'v') i2 = atoi(a1->value);
			a1 = a1->nextattribute;
		}
		if (menuptr == NULL) return sendresult(1);
		if (sendresult((INT)guiresshow((*menuptr)->handle, (*winptr)->handle, i1, i2)) < 0) {
			return -4000;
		}
	}
	else if	(strcmp(a1->tag, "toolbar") == 0) {
		toolbarptr = findResourceRef(a1);
		if (toolbarptr == NULL) return sendresult(1);
		if (sendresult((INT)guiresshow((*toolbarptr)->handle, (*winptr)->handle, i1, i2)) < 0) {
			return -4000;
		}
	}
	else if ((strcmp(a1->tag, "dialog") == 0) || (strcmp(a1->tag, "panel") == 0)) {
		pnldlgptr = findResourceRef(a1);
		defpos = TRUE;
		a1 = a1->nextattribute->nextattribute;
		while (a1 != NULL) {
			if (a1->tag[0] == 'h') {
				i1 = atoi(a1->value);
				defpos = FALSE;
			}
			else if (a1->tag[0] == 'v') {
				i2 = atoi(a1->value);
				defpos = FALSE;
			}
			a1 = a1->nextattribute;
		}
		if (pnldlgptr == NULL) return sendresult(1);
		if (defpos) {
			i1 = i2 = INT_MIN;
		}
		i3 = (INT)guiresshow((*pnldlgptr)->handle, (*winptr)->handle, i1, i2);
		if (sendresult(i3) < 0) return -4000;
	}
	else if (strcmp(a1->tag, "image") == 0) {
		i1 = i2 = 0;
		imageptr = findImageRef(a1);
		if (imageptr == NULL) return sendresult(1);
		a1 = a1->nextattribute->nextattribute;
		while (a1 != NULL) {
			if (a1->tag[0] == 'h') i1 = atoi(a1->value);
			else if (a1->tag[0] == 'v') i2 = atoi(a1->value);
			a1 = a1->nextattribute;
		}
		i3 = guipixshow((*imageptr)->handle, (*winptr)->handle, i1, i2);
		if (sendresult(i3) < 0) return -4000;
	}
	else if (strcmp(a1->tag, "openfdlg") == 0 || strcmp(a1->tag, "openddlg") == 0 ||
		strcmp(a1->tag, "saveasfdlg") == 0 ||
		strcmp(a1->tag, "fontdlg") == 0 ||
		strcmp(a1->tag, "alertdlg") == 0 ||
		strcmp(a1->tag, "colordlg") == 0) {
		pnldlgptr = findResourceRef(a1);
		a1 = a1->nextattribute->nextattribute;
		while (a1 != NULL) {
			if (a1->tag[0] == 'h') i1 = atoi(a1->value);
			else if (a1->tag[0] == 'v') i2 = atoi(a1->value);
			a1 = a1->nextattribute;
		}
		if (pnldlgptr == NULL) {
			return sendresult(1);
		}
		i1 = (INT)guiresshow((*pnldlgptr)->handle, (*winptr)->handle, i1, i2);
		if (sendresult(i1) < 0) {
			return -4000;
		}
	}
	return 0;
}
#endif

#if OS_WIN32
static INT docloseimage(ELEMENT *e1)
{
	IMAGEREF **imageptr, **previmage = NULL;
	ATTRIBUTE *a1;

	a1 = e1->firstattribute;
	imageptr = firstimage;
	while (imageptr != NULL) { /* search for image in linked list */
		if (strcmp((*imageptr)->name, a1->value) == 0) break;
		previmage = imageptr;
		imageptr = (*imageptr)->next;
	}
	if (imageptr == NULL) return 0;
	guipixdestroy((*imageptr)->handle);
	if (previmage == NULL) firstimage = (*imageptr)->next;
	else (*previmage)->next = (*imageptr)->next;
	memfree((UCHAR **) imageptr);
	return 0;
}

static INT doclose(ELEMENT *e1)
{
	WINDOWREF **winptr, **prevwin = NULL;
	RESOURCEREF **menuptr, **prevmenu = NULL;
	RESOURCEREF **toolbarptr, **prevtoolbar = NULL;
	RESOURCEREF **pnldlgptr, **prevpnldlg = NULL;
	RESOURCEREF **iconptr, **previcon = NULL;
	ATTRIBUTE *a1;
	INT i1;

	a1 = e1->firstattribute;
	if (strcmp(a1->tag, "image") == 0) return docloseimage(e1);

	if (!guiflag) {
		if (cnsflag || guiCSCinit(breakeventid, &guiFontParseControl) != 0) {
			return -2003;
		}
		guiflag = TRUE;
	}

	if ((strcmp(a1->tag, "menu") == 0) || (strcmp(a1->tag, "popupmenu") == 0)) {
		menuptr = firstmenu;
		while (menuptr != NULL) { /* search for menu in linked list */
			if (strcmp((*menuptr)->name, a1->value) == 0) break;
			prevmenu = menuptr;
			menuptr = (*menuptr)->next;
		}
		if (menuptr == NULL) return 0;
		guiresdestroy((*menuptr)->handle);
		if (prevmenu == NULL) firstmenu = (*menuptr)->next;
		else (*prevmenu)->next = (*menuptr)->next;
		memfree((UCHAR **) menuptr);
	}
	else if	(strcmp(a1->tag, "toolbar") == 0) {
		toolbarptr = firsttoolbar;
		while (toolbarptr != NULL) { /* search for toolbar in linked list */
			if (strcmp((*toolbarptr)->name, a1->value) == 0) break;
			prevtoolbar = toolbarptr;
			toolbarptr = (*toolbarptr)->next;
		}
		if (toolbarptr == NULL) return 0;
		guiresdestroy((*toolbarptr)->handle);
		if (prevtoolbar == NULL) firsttoolbar = (*toolbarptr)->next;
		else (*prevtoolbar)->next = (*toolbarptr)->next;
		memfree((UCHAR **) toolbarptr);
	}
	else if ((strcmp(a1->tag, "dialog") == 0) || (strcmp(a1->tag, "panel") == 0)
			|| strcmp(a1->tag, "alertdlg") == 0) {
		pnldlgptr = firstpnldlg;
		while (pnldlgptr != NULL) {
			if (strcmp((*pnldlgptr)->name, a1->value) == 0) break;
			prevpnldlg = pnldlgptr;
			pnldlgptr = (*pnldlgptr)->next;
		}
		if (pnldlgptr == NULL) return 0;
		i1 = guiresdestroy((*pnldlgptr)->handle);
		if (prevpnldlg == NULL) firstpnldlg = (*pnldlgptr)->next;
		else (*prevpnldlg)->next = (*pnldlgptr)->next;
		memfree((UCHAR **) pnldlgptr);
	}
	else if (strcmp(a1->tag, "window") == 0) {
		winptr = firstwindow;
		while (winptr != NULL) { /* search for window in linked list */
			if (strcmp((*winptr)->name, a1->value) == 0) break;
			prevwin = winptr;
			winptr = (*winptr)->next;
		}
		if (winptr == NULL) return 0;
		guiwindestroy((*winptr)->handle);
		if (prevwin == NULL) firstwindow = (*winptr)->next;
		else (*prevwin)->next = (*winptr)->next;
		memfree((UCHAR **) winptr);
	}
	else if (strcmp(a1->tag, "icon") == 0) {
		iconptr = firsticon;
		while (iconptr != NULL) { /* search for image in linked list */
			if (strcmp((*iconptr)->name, a1->value) == 0) break;
			previcon = iconptr;
			iconptr = (*iconptr)->next;
		}
		if (iconptr == NULL) return 0;
		guiresdestroy((*iconptr)->handle);
		if (previcon == NULL) firsticon = (*iconptr)->next;
		else (*previcon)->next = (*iconptr)->next;
		memfree((UCHAR **) iconptr);
	}
	else if (strcmp(a1->tag, "application") == 0) {
		guiAppDestroy();
	}
	else return -2000;
	return 0;
}
#endif

#if OS_WIN32
static INT dohide(ELEMENT *e1)
{
	RESOURCEREF **menuptr, **toolbarptr, **pnldlgptr;
	IMAGEREF **imageptr;
	ATTRIBUTE *a1;

	if (!guiflag) {
		if (cnsflag || guiCSCinit(breakeventid, &guiFontParseControl) != 0) {
			return -2003;
		}
		guiflag = TRUE;
	}

	a1 = e1->firstattribute;
	if ((strcmp(a1->tag, "menu") == 0) || (strcmp(a1->tag, "popupmenu") == 0)) {
		menuptr = findResourceRef(a1);
		if (menuptr != NULL) guireshide((*menuptr)->handle);
	}
	else if	(strcmp(a1->tag, "toolbar") == 0) {
		toolbarptr = findResourceRef(a1);
		if (toolbarptr != NULL) guireshide((*toolbarptr)->handle);
	}
	else if (strcmp(a1->tag, "image") == 0) {
		imageptr = findImageRef(a1);
		if (imageptr != NULL) guipixhide((*imageptr)->handle);
	}
	else if ((strcmp(a1->tag, "dialog") == 0) || (strcmp(a1->tag, "panel") == 0)) {
		pnldlgptr = firstpnldlg;
		while (pnldlgptr != NULL) {
			if (strcmp((*pnldlgptr)->name, a1->value) == 0) break;
			pnldlgptr = (*pnldlgptr)->next;
		}
		if (pnldlgptr != NULL) guireshide((*pnldlgptr)->handle);
	}
	else return -2000;
	if (sendresult(0) < 0) return -4000;
	return 0;
}
#endif

#if OS_WIN32
static INT doquery(ELEMENT *e1)
{
	static CHAR list[65504]; /** CODE - need a better solution here **/
	INT i1, i2, i3, dlgflag;
	CHAR name[MAXRESNAMELEN + 1], hval[6], vval[6], func[24];
	RESOURCEREF **menuptr, **toolbarptr, **pnldlgptr;
	WINDOWREF **winptr;
	WINDOW *win;
	ELEMENT e2;
	ATTRIBUTE *a1, a2, a3, a4;

	/*
	 * If the query element contains attribute f="2" then this is a getrowcount,
	 * If f="3" it is a statusbarheight,
	 * else it is a 'status'.
	 */
	strcpy(func, "status");
	e2.tag = "status";
	a1 = e1->firstattribute;
	while (a1 != NULL) {
		if (strcmp(a1->tag, "f") == 0) {
			if (strcmp(a1->value, "2") == 0) {
				StringCbCopy(func, sizeof(func), "getrowcount");
				e2.tag = "getrowcount";
				break;
			}
			else if (strcmp(a1->value, "3") == 0) {
				StringCbCopy(func, sizeof(func), "statusbarheight");
				e2.tag = "statusbarheight";
				break;
			}
		}
		a1 = a1->nextattribute;
	}

	if (!guiflag) {
		if (cnsflag || guiCSCinit(breakeventid, &guiFontParseControl) != 0) {
			return -2003;
		}
		guiflag = TRUE;
	}

	e2.cdataflag = 0;
	e2.firstsubelement = NULL;
	e2.nextelement = NULL;
	e2.firstattribute = &a2;
	a2.nextattribute = &a3;
	a3.nextattribute = &a4;
	a4.nextattribute = NULL;
	list[0] = '\0';
	name[0] = '\0';
	i2 = sizeof(list);
	a1 = e1->firstattribute;
	if (a1 == NULL) return -2000;
	strcpy(name, a1->value);

	if (strcmp(a1->tag, "window") == 0) {
		/*
		 * At this time we have only one possible type of query to a window,
		 * so for now just assume here that it is status bar height
		 */
		winptr = firstwindow;
		while (winptr != NULL) {
			if (strcmp((*winptr)->name, name) == 0) {
				/* named window found */
				break;
			}
			winptr = (*winptr)->next;
		}
		if (winptr == NULL) return -2000;
		win = *(*winptr)->handle;
		i1 = (win->statusbarshown) ? win->borderbtm : 0;
		a2.tag = "t";
		mscitoa(i1, hval);
		a2.value = hval;
		a2.nextattribute = NULL;
		i3 = sendelement(&e2, TRUE);
		if (i3 < 0) return -4000;
	}
	else if (strcmp(a1->tag, "menu") == 0 || strcmp(a1->tag, "popupmenu") == 0) {
		menuptr = findResourceRef(a1);
		if (menuptr == NULL) return -2000;
		a2.tag = a1->tag;
		a1 = a1->nextattribute;
		if (a1 == NULL || a1->tag[0] != 'n') return -2000;
		strcat(func, a1->value);
		if (guiresquery((*menuptr)->handle, (UCHAR *) func, (INT)strlen(func), (UCHAR *) list, &i2)) {
			if (sendresult(1) < 0) return -4000;
			return 0;
		}
		else {
			list[i2] = '\0';
			a2.value = name;
			a3.tag = "t";
			a3.value = list;
			a3.nextattribute = NULL;
			i3 = sendelement(&e2, TRUE);
			if (i3 < 0) return -4000;
		}
	}
	else if (strcmp(a1->tag, "toolbar") == 0) {
		toolbarptr = findResourceRef(a1);
		if (toolbarptr == NULL) return -2000;
		a1 = a1->nextattribute;
		if (a1 == NULL || a1->tag[0] != 'n') return -2000;
		strcat(func, a1->value);
		if (guiresquery((*toolbarptr)->handle, (UCHAR *) func, (INT)strlen(func), (UCHAR *) list, &i2)) {
			if (sendresult(1) < 0) return -4000;
			return 0;
		}
		else {
			list[i2] = '\0';
			a2.tag = "toolbar";
			a2.value = name;
			a3.tag = "t";
			a3.value = list;
			a3.nextattribute = NULL;
			i3 = sendelement(&e2, TRUE);
			if (i3 < 0) return -4000;
		}
	}
	else if (strcmp(a1->tag, "dialog") == 0 || strcmp(a1->tag, "panel") == 0) {
		dlgflag = (a1->tag[0] == 'd') ? TRUE : FALSE;
		pnldlgptr = findResourceRef(a1);
		if (pnldlgptr == NULL) return -2000;
		a1 = a1->nextattribute;
		if (a1 == NULL || a1->tag[0] != 'n') return -2000;
		strcat(func, a1->value);
		if (guiresquery((*pnldlgptr)->handle, (UCHAR *) func, (INT)strlen(func), (UCHAR *) list, &i2)) {
			if (sendresult(1) < 0) return -4000;
			return 0;
		}
		else {
			list[i2] = '\0';
			a2.tag = (dlgflag) ? "dialog" : "panel";
			a2.value = name;
			a3.tag = "t";
			a3.value = list;
			a3.nextattribute = NULL;
			i3 = sendelement(&e2, TRUE);
			if (i3 < 0) return -4000;
		}
	}
	else if (strcmp(a1->tag, "clipboard") == 0) {
		a1 = a1->nextattribute;
		if (a1 == NULL || a1->tag[0] != 'f') return -2000;
		a2.tag = "clipboard";
		a2.value = "0";
		a3.tag = "h";
		a4.tag = "v";
		if (strcmp(a1->value, "bitsperpixel") == 0) {
			if (guicbdbpp(&i1) < 0) {
				if (sendresult(1) < 0) return -4000;
				return 0;
			}
		}
		else if (strcmp(a1->value, "imagesize") == 0) {
			if (guicbdsize(&i1, &i2) < 0) {
				if (sendresult(1) < 0) return -4000;
				return 0;
			}
		}
		else if (strcmp(a1->value, "resolution") == 0) {
			if (guicbdres(&i1, &i2) < 0) {
				if (sendresult(1) < 0) return -4000;
				return 0;
			}
		}
		else return -2000;
		mscitoa(i1, hval);
		a3.value = hval;
		if (strcmp(a1->value, "bitsperpixel") == 0) {
			a3.nextattribute = NULL;
		}
		else {
			mscitoa(i2, vval);
			a4.value = vval;
		}
		i3 = sendelement(&e2, TRUE);
		if (i3 < 0) return -4000;
	}
	else return -2000;
	return 0;
}
#endif


#if OS_WIN32

static IMAGEREF** findImageRef(ATTRIBUTE *a1) {

	IMAGEREF** ptr = NULL;

	if (strcmp(a1->tag, "image") == 0) ptr = firstimage;
	while (ptr != NULL) {
		if (strcmp((*ptr)->name, a1->value) == 0) break;
		ptr = (*ptr)->next;
	}
	return ptr;
}

static RESOURCEREF** findResourceRef(ATTRIBUTE *a1) {

	RESOURCEREF** ptr = NULL;

	if (strcmp(a1->tag, "panel") == 0 || strcmp(a1->tag, "dialog") == 0 ||
		strcmp(a1->tag, "openfdlg") == 0 || strcmp(a1->tag, "saveasfdlg") == 0 ||
		strcmp(a1->tag, "fontdlg") == 0 || strcmp(a1->tag, "colordlg") == 0 ||
		strcmp(a1->tag, "openddlg") == 0 || strcmp(a1->tag, "alertdlg") == 0) {
		ptr = firstpnldlg;
	}
	else if ((strcmp(a1->tag, "menu") == 0) || (strcmp(a1->tag, "popupmenu") == 0)) {
		ptr = firstmenu;
	}
	else if (strcmp(a1->tag, "toolbar") == 0) {
		ptr = firsttoolbar;
	}
	while (ptr != NULL) {
		if (strcmp((*ptr)->name, a1->value) == 0) break;
		ptr = (*ptr)->next;
	}
	return ptr;
}

static INT dochange(ELEMENT *e1)
{
	ATTRIBUTE *a1;

	if (!guiflag) {
		if (cnsflag || guiCSCinit(breakeventid, &guiFontParseControl) != 0) {
			return -2003;
		}
		guiflag = TRUE;
	}
	a1 = e1->firstattribute;
	if (strcmp(a1->tag, "window") == 0 || strcmp(a1->tag, "application") == 0) return dochangedevice(e1);
	else return dochangeresource(e1);
}

static INT dochangedevice(ELEMENT *e1)
{
	ATTRIBUTE *a1;
	WINDOWREF **winptr;
	INT aflag, i1, i2, i3, i4;
	CHAR func[32], *list = NULL;
	ELEMENT *e2;

	a1 = e1->firstattribute;
	aflag = strcmp(a1->tag, "window");
	if (!aflag) {
		winptr = firstwindow;
		while (winptr != NULL) { /* search for window in linked list */
			if (strcmp((*winptr)->name, a1->value) == 0) break;
			winptr = (*winptr)->next;
		}
		if (winptr == NULL) return -2;
	}
	a1 = a1->nextattribute;
	while (a1 != NULL) {
		i1 = i2 = 0;
		if (strcmp(a1->tag, "title") == 0) { /* title */
			StringCbCopy(func, sizeof(func), a1->tag);
			//memcpy(func, "title", i1 = 5);
			i2 = (INT)strlen(a1->value);
			list = (CHAR *) malloc(sizeof(CHAR) * (i2 + 1));
			if (list == NULL) return -1603;
			StringCchCopy(list, i2 + 1, a1->value);
			//memcpy(list, a1->value, i2);
		}
		else if (memcmp(a1->tag, "hsize", 5) == 0) {
			list = (CHAR *) malloc(sizeof(CHAR) * (10 + 1));
			if (list == NULL) return -1603;
			StringCbCopy(func, sizeof(func), "size");
			//memcpy(func, "size", i1 = 4);
			i2 = 10;
			/* Format the two numbers into the format hhhhhvvvvv */
			for (i3 = i4 = 0; i3 < 5 - (INT) strlen(a1->value); i3++) list[i3] = ' ';
			while (i3 < 5) list[i3++] = a1->value[i4++];
			a1 = a1->nextattribute; /* assume vsize attribute is next */
			if (a1 == NULL) return -100;
			for (i4 = 0; i3 < 10 - (INT) strlen(a1->value); i3++) list[i3] = ' ';
			while (i3 < 10) list[i3++] = a1->value[i4++];
		}
		else if (memcmp(a1->tag, "statusbartext", 13) == 0) {
			StringCbCopy(func, sizeof(func), "statusbar");
			//memcpy(func, "statusbar", i1 = 9);
			i2 = (INT)strlen(a1->value);
			list = (CHAR *) malloc(sizeof(CHAR) * (i2 + 1));
			if (list == NULL) return -1603;
			StringCchCopy(list, i2 + 1, a1->value);
			//memcpy(list, a1->value, i2);
		}
		else if (memcmp(a1->tag, "statusbar", 9) == 0) {
			if (a1->value[0] == 'n') {
				StringCbCopy(func, sizeof(func), "nostatusbar");
				//memcpy(func, "nostatusbar", i1 = 11);
			}
			else {
				if (sendresult(0) < 0) return -4000;
				a1 = a1->nextattribute;
				continue;
			}
		}
		else if (memcmp(a1->tag, "mouse", 5) == 0) {
			if (a1->value[0] == 'y') StringCbCopy(func, sizeof(func), "mouseon");
			else StringCbCopy(func, sizeof(func), "mouseoff");
		}
		else if (memcmp(a1->tag, "pointer", 7) == 0) {
			StringCbCopy(func, sizeof(func), a1->tag);
			i2 = (INT)strlen(a1->value);
			list = (CHAR *) malloc(sizeof(CHAR) * (i2 + 1));
			if (list == NULL) return -1603;
			memcpy(list, a1->value, i2);
		}
		else if (memcmp(a1->tag, "hsbpos", 6) == 0) {
			list = (CHAR *) malloc(sizeof(CHAR) * (5 + 1));
			if (list == NULL) return -1603;
			StringCbCopy(func, sizeof(func), "hscrollbarpos");
			i2 = 5;
			/* Format the number into the format hhhhh */
			for (i3 = i4 = 0; i3 < 5 - (INT) strlen(a1->value); i3++) list[i3] = ' ';
			while (i3 < 5) list[i3++] = a1->value[i4++];
		}
		else if (memcmp(a1->tag, "hsbrange", 8) == 0) {
			list = (CHAR *) malloc(sizeof(CHAR) * (15 + 1));
			if (list == NULL) return -1603;
			StringCbCopy(func, sizeof(func), "hscrollbarrange");
			if (strlen(a1->value) != 15) return -100;
			memcpy(list, a1->value, i2 = 15);
		}
		else if (memcmp(a1->tag, "hsboff", 6) == 0) {
			StringCbCopy(func, sizeof(func), "hscrollbaroff");
		}
		else if (memcmp(a1->tag, "vsbpos", 6) == 0) {
			list = (CHAR *) malloc(sizeof(CHAR) * (5 + 1));
			if (list == NULL) return -1603;
			StringCbCopy(func, sizeof(func), "vscrollbarpos");
			i2 = 5;
			/* Format the number into the format hhhhh */
			for (i3 = i4 = 0; i3 < 5 - (INT) strlen(a1->value); i3++) list[i3] = ' ';
			while (i3 < 5) list[i3++] = a1->value[i4++];
		}
		else if (memcmp(a1->tag, "vsbrange", 8) == 0) {
			list = (CHAR *) malloc(sizeof(CHAR) * (15 + 1));
			if (list == NULL) return -1603;
			StringCbCopy(func, sizeof(func), "vscrollbarrange");
			if (strlen(a1->value) != 15) return -100;
			memcpy(list, a1->value, i2 = 15);
		}
		else if (memcmp(a1->tag, "vsboff", 6) == 0) {
			StringCbCopy(func, sizeof(func), "vscrollbaroff");
		}
		else if (memcmp(a1->tag, "focus", 5) == 0) {
			if (a1->value[0] == 'y') StringCbCopy(func, sizeof(func), "focus");
			else StringCbCopy(func, sizeof(func), "unfocus");
		}
		else if (memcmp(a1->tag, "caretsize", 9) == 0) {
			list = (CHAR *) malloc(sizeof(CHAR) * (5 + 1));
			if (list == NULL) return -1603;
			StringCbCopy(func, sizeof(func), a1->tag);
			i2 = 5;
			/* Format the number into the format hhhhh */
			for (i3 = i4 = 0; i3 < 5 - (INT) strlen(a1->value); i3++) list[i3] = ' ';
			while (i3 < 5) list[i3++] = a1->value[i4++];
		}
		else if (memcmp(a1->tag, "caretposh", 9) == 0) {
			list = (CHAR *) malloc(sizeof(CHAR) * (10 + 1));
			if (list == NULL) return -1603;
			StringCbCopy(func, sizeof(func), "caretpos");
			i2 = 10;
			/* Format the two numbers into the format hhhhhvvvvv */
			for (i3 = i4 = 0; i3 < 5 - (INT) strlen(a1->value); i3++) list[i3] = ' ';
			while (i3 < 5) list[i3++] = a1->value[i4++];
			a1 = a1->nextattribute; /* assume caretposv attribute is next */
			if (a1 == NULL) return -100;
			for (i4 = 0; i3 < 10 - (INT) strlen(a1->value); i3++) list[i3] = ' ';
			while (i3 < 10) list[i3++] = a1->value[i4++];
		}
		else if (memcmp(a1->tag, "caret", 5) == 0) {
			if (a1->value[0] == 'y') StringCbCopy(func, sizeof(func), "careton");
			else StringCbCopy(func, sizeof(func), "caretoff");
		}
		else if (memcmp(a1->tag, "ttwidth", 7) == 0) {
			list = (CHAR *) malloc(sizeof(CHAR) * (5 + 1));
			if (list == NULL) return -1603;
			StringCbCopy(func, sizeof(func), "helptextwidth");
			for (i3 = i4 = 0; i3 < 5 - (INT) strlen(a1->value); i3++) list[i3] = ' ';
			while (i3 < 5) list[i3++] = a1->value[i4++];
			i2 = 5;
		}
		else if (strlen(a1->tag) == 1 && a1->tag[0] == 'h') {
			list = (CHAR *) malloc(sizeof(CHAR) * (10 + 1));
			if (list == NULL) return -1603;
			StringCbCopy(func, sizeof(func), "position");
			i2 = 10;
			/* Format the two numbers into the format hhhhhvvvvv */
			for (i3 = i4 = 0; i3 < 5 - (INT) strlen(a1->value); i3++) list[i3] = ' ';
			while (i3 < 5) list[i3++] = a1->value[i4++];
			a1 = a1->nextattribute; /* assume v attribute is next */
			if (a1 == NULL) return -100;
			for (i4 = 0; i3 < 10 - (INT) strlen(a1->value); i3++) list[i3] = ' ';
			while (i3 < 10) list[i3++] = a1->value[i4++];
		}
		else if (strlen(a1->tag) == 1 && a1->tag[0] == 'f') {
			if (memcmp(a1->value, "consolewindow", 13) == 0) {
				a1 = a1->nextattribute;
				if (!aflag || a1 == NULL) return -100;
				if (memcmp(a1->value, "off", 3) == 0 && vidflag) {
					videxit();
					vidflag = FALSE;
				}
				return (sendresult(0) < 0) ? -4000 : 0;
			}
			else if (!strcmp(a1->value, "desktopicon") || !strcmp(a1->value, "taskbartext")
					|| !strcmp(a1->value, "dialogicon")) {
				StringCbCopy(func, sizeof(func), a1->value);
				a1 = a1->nextattribute;
				if (a1 == NULL) return -100;
				i2 = (INT)strlen(a1->value);
				list = (CHAR *) malloc(sizeof(CHAR) * (i2 + 1));
				if (list == NULL) return -1603;
				StringCchCopy(list, i2 + 1, a1->value);
				//memcpy(list, a1->value, i2);
			}
//			else if (!strcmp(a1->value, "taskbartext")) {
//				memcpy(func, a1->value, i1 = strlen(a1->value));
//				a1 = a1->nextattribute;
//				if (a1 == NULL) return -100;
//				i2 = (INT)strlen(a1->value);
//				list = (CHAR *) malloc(sizeof(CHAR) * (i2 + 1));
//				if (list == NULL) return -1603;
//				memcpy(list, a1->value, i2);
//			}
			else if (strcmp(a1->value, "maximize") == 0) {
				StringCbCopy(func, sizeof(func), "maximize");
			}
			else if (strcmp(a1->value, "minimize") == 0) {
				StringCbCopy(func, sizeof(func), "minimize");
			}
			else if (strcmp(a1->value, "restore") == 0) {
				StringCbCopy(func, sizeof(func), "restore");
			}
			else if (strcmp(a1->value, "consolefocus") == 0) {
				StringCbCopy(func, sizeof(func), "consolefocus");
			}
		}
		else return -100;
		if (list == NULL) {
			list = (CHAR *) malloc(sizeof(CHAR));
			if (list == NULL) return -1603;
			list[0] = '\0';
		}
		if (aflag) {
			if (sendresult(guiappchange((UCHAR *) func, (INT) strlen(func), (UCHAR *) list, i2)) < 0) {
				if (list != NULL) free(list);
				return -4000;
			}
		}
		else {
			i1 = guiwinchange((*winptr)->handle, (UCHAR *) func, (INT) strlen(func), (UCHAR *) list, i2);
			if (sendresult(i1 == 0 ? 0 : 1) < 0) {
				if (list != NULL) free(list);
				return -4000;
			}
		}
		if (list != NULL) {
			free(list);
			list = NULL;
		}
		a1 = a1->nextattribute;
	}

	/**
	 * Look for a child element.
	 * The undocumented scroll bar commands (e.g. vscrollbarup)
	 * will be sent as child elements to the <change window...> element
	 */
	if (e1->firstsubelement == NULL || aflag) return 0;
	e2 = e1->firstsubelement;
	i1 = guiwinchange((*winptr)->handle, (UCHAR *) e2->tag, (INT)strlen(e2->tag), (UCHAR *) NULL, 0);
	if (sendresult(i1 == 0 ? 0 : 1) < 0) {
		return -4000;
	}
	return 0;
}

static INT dochangeresource(ELEMENT *e1)
{
	if (e1->firstsubelement != NULL) return dochangeresnew(e1);
	else return dochangeresold(e1);
}

static INT dochangeresnew(ELEMENT *e1)
{
	INT i1, i2, i3, retcode, cmdcode, useritem, subuseritem;
	ELEMENT *e2;
	RESOURCEREF **resref;
	ATTRIBUTE *a1, *mainAttr, *dataAttr;
	UCHAR *data = NULL;
	INT datalen = 0;
	UINT pos, len;

	mainAttr = dataAttr = NULL;
	a1 = e1->firstattribute;
	while (a1 != NULL) {
		if (!strcmp(a1->tag, "l")) {
			if (dataAttr != NULL) return RC_ERROR;
			dataAttr = a1;
		}
		else if (!strcmp(a1->tag, "panel") || !strcmp(a1->tag, "dialog") || !strcmp(a1->tag, "toolbar")
			|| !strcmp(a1->tag, "popupmenu") || !strcmp(a1->tag, "openfdlg") || !strcmp(a1->tag, "saveasfdlg")
			|| !strcmp(a1->tag, "fontdlg") || !strcmp(a1->tag, "colordlg") || !strcmp(a1->tag, "menu")
			|| !strcmp(a1->tag, "openddlg"))
		{
			if (mainAttr != NULL) return RC_ERROR;
			mainAttr = a1;
		}
		a1 = a1->nextattribute;
	}
	if (mainAttr == NULL) return -100;
	resref = findResourceRef(mainAttr);
	if (resref == NULL) {
		return -101;
	}
	if (dataAttr != NULL) {
		data = dataAttr->value;
		datalen = (INT)strlen(data);
	}

	e2 = e1->firstsubelement;
	do {
		if (strcmp(e2->tag, "multiple") == 0) {
			retcode = guiMultiChange((*resref)->handle, data, datalen);
			if (retcode) break;
			continue;
		}
		i3 = (INT)strlen(e2->tag);
		retcode = -100;
		/* The tag of the element is the resource change command.
		 * Search for it in the table of valid resource change commands.
		 * If not found, return with an error code.
		 */
		for (i1 = 0; symtable[i1].name != NULL; i1++) {
			i2 = (INT)strlen(symtable[i1].name);
			if (i2 == i3 && !guimemicmp(e2->tag, symtable[i1].name, i2)) {
				retcode = 0;
				break;
			}
		}
		if (retcode) break;
		cmdcode = symtable[i1].value;

		/* Find the user item, if present */
		useritem = INT_MIN;
		subuseritem = INT_MIN;
		if (e2->firstattribute != NULL) {
			a1 = e2->firstattribute;
			if (!strcmp(a1->tag, "n") && isdigit(a1->value[0])) {
				pos = 0;
				len = (INT)strlen(a1->value);
				useritem = 0;
				do {
					useritem = useritem * 10 + a1->value[pos++] - '0';
				} while (pos < len && isdigit(a1->value[pos]));
				if (pos < len && a1->value[pos] == '[') {
					do pos++; while (pos < len && a1->value[pos] == ' ');
					subuseritem = 0;
					while (pos < len && isdigit(a1->value[pos])) {
						subuseritem = subuseritem * 10 + a1->value[pos++] - '0';
					}
				}
			}
		}
		if (useritem != INT_MIN) retcode = guireschange2((*resref)->handle, cmdcode, useritem, subuseritem, data, datalen);
		else retcode = guireschange3((*resref)->handle, cmdcode, data, datalen);
		if (retcode) break;
	}
	while ((e2 = e2->nextelement) != NULL);
	if (sendresult(retcode ? 1 : 0) < 0) return -4000;
	return 0;
}

static INT dochangeresold(ELEMENT *e1)
{
	INT i1, i2, tflag;
	CHAR func[32], *list, itemstring[6];

	RESOURCEREF **menuptr, **toolbarptr, **pnldlgptr;
	ATTRIBUTE *a1, *fattr, *focattr, *itemattr, *lineattr, *lattr, *nattr;

	if (!guiflag) {
		if (cnsflag || guiCSCinit(breakeventid, &guiFontParseControl) != 0) {
			return -2003;
		}
		guiflag = TRUE;
	}

	tflag = FALSE;
	list = NULL;
	a1 = e1->firstattribute;
	if ((strcmp(a1->tag, "panel") == 0) || (strcmp(a1->tag, "dialog") == 0)) {
		pnldlgptr = findResourceRef(a1);
		if (pnldlgptr == NULL) return -2;

		a1 = a1->nextattribute;
		fattr = nattr = lattr = focattr = lineattr = itemattr = NULL;
		while (a1 != NULL) {
			if (!strcmp(a1->tag, "f")) fattr = a1;
			else if (!strcmp(a1->tag, "n")) nattr = a1;
			else if (!strcmp(a1->tag, "l")) lattr = a1;
			else if (!strcmp(a1->tag, "focus")) focattr = a1;
			else if (!strcmp(a1->tag, "line")) lineattr = a1;
			else if (!strcmp(a1->tag, "item")) itemattr = a1;
			a1 = a1->nextattribute;
		}

		if (focattr != NULL) {
			if (focattr->value[0] == 'y') memcpy(func, "focuson", i1 = 7);
			else memcpy(func, "focusoff", i1 = 8);
		}
		else if (lineattr != NULL) {
			if (lineattr->value[0] == 'y') memcpy(func, "lineon", i1 = 6);
			else memcpy(func, "lineoff", i1 = 7);
		}
		else if (itemattr != NULL) {
			if (itemattr->value[0] == 'y') memcpy(func, "itemon", i1 = 6);
			else memcpy(func, "itemoff", i1 = 7);
		}
		else if (fattr != NULL) {
			if (memcmp(fattr->value, "focus", 5) == 0         || memcmp(fattr->value, "mark", 4) == 0 ||
				memcmp(fattr->value, "unmark", 6) == 0        || memcmp(fattr->value, "available", 9) == 0 ||
				memcmp(fattr->value, "gray", 4) == 0          || memcmp(fattr->value, "showonly", 8) == 0 ||
				memcmp(fattr->value, "nofocus", 7) == 0       || memcmp(fattr->value, "readonly", 8) == 0 ||
				memcmp(fattr->value, "textcolorline", 13) == 0 || memcmp(fattr->value, "textcolordata", 13) == 0 ||
				memcmp(fattr->value, "textstyleline", 13) == 0 || memcmp(fattr->value, "textstyledata", 13) == 0 ||
				memcmp(fattr->value, "textcolor", 9) == 0     || memcmp(fattr->value, "text", 4) == 0 ||
				memcmp(fattr->value, "defpushbutton", 13) == 0 || memcmp(fattr->value, "escpushbutton", 13) == 0 ||
				memcmp(fattr->value, "pushbutton", 10) == 0   || memcmp(fattr->value, "justifyleft", 11) == 0 ||
				memcmp(fattr->value, "justifycenter", 13) == 0 || memcmp(fattr->value, "justifyright", 12) == 0 ||
				memcmp(fattr->value, "erase", 5) == 0         || memcmp(fattr->value, "paste", 5) == 0 ||
				memcmp(fattr->value, "setmaxchars", 11) == 0  || memcmp(fattr->value, "deleteline", 10) == 0 ||
				memcmp(fattr->value, "delete", 6) == 0        || memcmp(fattr->value, "insertbefore", 12) == 0 ||
				memcmp(fattr->value, "insertafter", 11) == 0  || memcmp(fattr->value, "insertlineafter", 15) == 0 ||
				memcmp(fattr->value, "insert", 6) == 0        || memcmp(fattr->value, "insertlinebefore", 16) == 0 ||
				memcmp(fattr->value, "minsert", 7) == 0       || memcmp(fattr->value, "locateline", 10) == 0 ||
				memcmp(fattr->value, "locate", 6) == 0        || memcmp(fattr->value, "selectline", 10) == 0 ||
				memcmp(fattr->value, "selectall", 9) == 0     || memcmp(fattr->value, "select", 6) == 0 ||
				memcmp(fattr->value, "deselectline", 12) == 0 || memcmp(fattr->value, "deselectall", 11) == 0 ||
				memcmp(fattr->value, "deselect", 8) == 0      || memcmp(fattr->value, "range", 5) == 0 ||
				memcmp(fattr->value, "position", 8) == 0      || memcmp(fattr->value, "uplevel", 7) == 0 ||
				memcmp(fattr->value, "downlevel", 9) == 0     || memcmp(fattr->value, "rootlevel", 9) == 0 ||
				memcmp(fattr->value, "setline", 7) == 0       || memcmp(fattr->value, "collapse", 8) == 0 ||
				memcmp(fattr->value, "expand", 7) == 0        || memcmp(fattr->value, "icon", 4) == 0 ||
				memcmp(fattr->value, "feditmask", 9) == 0     || memcmp(fattr->value, "locatetab", 9) == 0 ||
				memcmp(fattr->value, "helptext", 8) == 0)
			{
				if (nattr == NULL) return -100;
				sprintf(func, "%s%5.5s", fattr->value, nattr->value);
			}
			else return -100;
		}
		else return -100;

		if (lattr != NULL) {
			list = lattr->value;
			i2 = (INT)strlen(list);
		}
		else i2 = 0;
		i1 = sendresult(guireschange((*pnldlgptr)->handle, (UCHAR *) func, (INT)strlen(func), (UCHAR *) list, i2));
		if (i1 < 0) return -4000;
	}

	else if ((strcmp(a1->tag, "menu") == 0) || (strcmp(a1->tag, "popupmenu") == 0)) {
		menuptr = findResourceRef(a1);
		if (menuptr == NULL) return -2;
		a1 = a1->nextattribute;
		fattr = nattr = NULL;
		while (a1 != NULL) {
			if (a1->tag[0] == 'f') fattr = a1;
			else if (a1->tag[0] == 'n') nattr = a1;
			else if (a1->tag[0] == 'l') {
				list = (CHAR *) malloc(sizeof(CHAR) * (strlen(a1->value) + 1));
				if (list == NULL) return -1603;
				strcpy(list, a1->value);
			}
			a1 = a1->nextattribute;
		}
		if (fattr == NULL || nattr == NULL) return -100;
		if (strcmp(fattr->value, "mark") == 0 ||
			strcmp(fattr->value, "unmark") == 0 ||
			strcmp(fattr->value, "available") == 0 ||
			strcmp(fattr->value, "gray") == 0 ||
			strcmp(fattr->value, "show") == 0 ||
			strcmp(fattr->value, "hide") == 0 ||
			strcmp(fattr->value, "text") == 0) {
			if (strcmp(func, "text") == 0) {
				if (list == NULL) return -100;
			}
			else i2 = 0;
			sprintf(func, "%s%5.5s", fattr->value, nattr->value);
			if (list == NULL) {
				list = (CHAR *) malloc(sizeof(CHAR));
				if (list == NULL) return -1603;
				list[0] = '\0';
			}
			i1 = guireschange((*menuptr)->handle, (UCHAR *) func, (INT)strlen(func), (UCHAR *) list, (INT)strlen(list));
			free(list);
			if (sendresult(i1) < 0) return -4000;
			list = NULL;
		}
		else return -100;
	}

	else if (strcmp(a1->tag, "toolbar") == 0) {
		toolbarptr = findResourceRef(a1);
		if (toolbarptr == NULL) return -2;
		func[0] = '\0';
		itemstring[0] = '\0';
		a1 = a1->nextattribute;
		while (a1 != NULL) {
			if (strlen(a1->tag) == 1) {
				if (a1->tag[0] == 'f') {
					if (FAILED(StringCbCopy(func, sizeof(func), a1->value))) return -100;
				}
				else if (a1->tag[0] == 'n') {
					if (FAILED(StringCbCopy(itemstring, sizeof(itemstring), a1->value))) return -100;
				}
				else if (a1->tag[0] == 'l') {
					list = (CHAR *) malloc(sizeof(CHAR) * (strlen(a1->value) + 1));
					if (list == NULL) return -1603;
					strcpy(list, a1->value);
				}
			}
			else {
				if (strcmp(a1->tag, "addspace") == 0) {
					StringCbCopy(func, sizeof(func), "addspace");
				}
				else if (strcmp(a1->tag, "item") == 0) {
					StringCbCopy(func, sizeof(func), a1->value[0] == 'y' ? "itemon" : "itemoff");
				}
				else if (strcmp(a1->tag, "line") == 0) {
					StringCbCopy(func, sizeof(func), a1->value[0] == 'y' ? "lineon" : "lineoff");
				}
			}
			a1 = a1->nextattribute;
		}
		if (func[0] == '\0') return -100;

		if (strcmp(func, "insertbefore") == 0 || strcmp(func, "insertafter") == 0) {
			if (itemstring[0] != '\0') {
				StringCbCat(func, sizeof(func), itemstring);
			}
		}
		else if (strcmp(func, "addpushbutton") == 0 ||
				strcmp(func, "addlockbutton") == 0 ||
				strcmp(func, "adddropbox") == 0 ||
				strcmp(func, "available") == 0 ||
				strcmp(func, "delete") == 0 ||
				strcmp(func, "deleteline") == 0 ||
				strcmp(func, "erase") == 0 ||
				strcmp(func, "focus") == 0 ||
				strcmp(func, "gray") == 0 ||
				strcmp(func, "helptext") == 0 ||
				strcmp(func, "icon") == 0 ||
				strcmp(func, "insert") == 0 ||
				strcmp(func, "insertlinebefore") == 0 ||
				strcmp(func, "insertlineafter") == 0 ||
				strcmp(func, "locate") == 0 ||
				strcmp(func, "locateline") == 0 ||
				strcmp(func, "mark") == 0 ||
				strcmp(func, "minsert") == 0 ||
				strcmp(func, "removespaceafter") == 0 ||
				strcmp(func, "removespacebefore") == 0 ||
				strcmp(func, "removecontrol") == 0 ||
				strcmp(func, "selectline") == 0 ||
				strcmp(func, "text") == 0 ||
				strcmp(func, "textcolor") == 0 ||
				strcmp(func, "textcolorline") == 0 ||
				strcmp(func, "textcolordata") == 0 ||
				strcmp(func, "textstyleline") == 0 ||
				strcmp(func, "textstyledata") == 0 ||
				strcmp(func, "unmark") == 0)
		{

			if (itemstring[0] == '\0') return -100;
			strcat(func, itemstring);
			if (strcmp(func, "addpushbutton") == 0 ||
					strcmp(func, "addlockbutton") == 0 ||
					strcmp(func, "adddropbox") == 0 ||
					strcmp(func, "deleteline") == 0 ||
					strcmp(func, "helptext") == 0 ||
					strcmp(func, "icon") == 0 ||
					strcmp(func, "insert") == 0 ||
					strcmp(func, "insertlinebefore") == 0 ||
					strcmp(func, "insertlineafter") == 0 ||
					strcmp(func, "locate") == 0 ||
					strcmp(func, "locateline") == 0 ||
					strcmp(func, "minsert") == 0 ||
					strcmp(func, "selectline") == 0 ||
					strcmp(func, "text") == 0 ||
					strcmp(func, "textcolor") == 0 ||
					strcmp(func, "textcolorline") == 0 ||
					strcmp(func, "textcolordata") == 0 ||
					strcmp(func, "textstyleline") == 0 ||
					strcmp(func, "textstyledata") == 0)
			{
				if (list == NULL) return -100;
			}

		}
		else if (strcmp(func, "addspace") == 0 ||
				strcmp(func, "itemon") == 0 ||
				strcmp(func, "itemoff") == 0 ||
				strcmp(func, "lineon") == 0 ||
				strcmp(func, "lineoff") == 0);
		else return -100;	/* unrecognized toolbar change function */

		if (list == NULL) {
			list = (CHAR *) malloc(sizeof(CHAR));
			if (list == NULL) return -1603;
			list[0] = '\0';
		}
		i1 = guireschange((*toolbarptr)->handle, (UCHAR *) func, (INT)strlen(func), (UCHAR *) list, (INT)strlen(list));
		free(list);
		if (sendresult(i1) < 0) return -4000;
		list = NULL;
	}

	else if (strcmp(a1->tag, "openfdlg") == 0 || strcmp(a1->tag, "saveasfdlg") == 0 ||
			strcmp(a1->tag, "fontdlg") == 0 || strcmp(a1->tag, "colordlg") == 0)
	{
		pnldlgptr = findResourceRef(a1);
		if (pnldlgptr == NULL) return -2;
		i1 = i2 = 0;
		if (memcmp(a1->tag, "fontdlg", 7) == 0 || memcmp(a1->tag, "colordlg", 8) == 0) {
			if (a1->tag[0] == 'f') memcpy(func, "fontname", i1 = 8);
			else if (a1->tag[0] == 'c') memcpy(func, "color", i1 = 5);
			a1 = a1->nextattribute;
			if (a1 == NULL || a1->tag[0] != 'l') return -100;
			i2 = (INT)strlen(a1->value);
			list = (CHAR *) malloc(sizeof(CHAR) * (i2 + 1));
			if (list == NULL) return -1603;
			memcpy(list, a1->value, i2);
		}
		else {
			a1 = a1->nextattribute;
			while (a1 != NULL) {
				if (a1->tag[0] == 'f') {
					i1 = (INT)strlen(a1->value);
					memcpy(func, a1->value, i1);
				}
				else if (a1->tag[0] == 'l' && list == NULL) {
					i2 = (INT)strlen(a1->value);
					list = (CHAR *) malloc(sizeof(CHAR) * (i2 + 1));
					if (list == NULL) return -1603;
					memcpy(list, a1->value, i2);
				}
				a1 = a1->nextattribute;
			}
		}
		if (list == NULL) {
			list = (CHAR *) malloc(sizeof(CHAR));
			if (list == NULL) return -1603;
			list[0] = '\0';
		}
		if (sendresult(guireschange((*pnldlgptr)->handle, (UCHAR *) func, i1, (UCHAR *) list, i2)) < 0) {
			if (list != NULL) free(list);
			return -4000;
		}
		if (list != NULL) {
			free(list);
			list = NULL;
		}
	}
	else return RC_ERROR; /* Error, device or resource type not recognized */
	return 0;
}


static INT dosetclipboard(ELEMENT *e1)
{
	ATTRIBUTE *a1;
	IMAGEREF **imageptr;

	a1 = e1->firstattribute;
	if (a1 == NULL) return -2000;
	if (!memcmp(a1->tag, "image", 5)) {
		imageptr = findImageRef(a1);
		if (imageptr == NULL) return -2000;
		guicbdputpixmap((*imageptr)->handle);
	}
	else { /* text */
		guicbdputtext((UCHAR *) a1->value, (INT)strlen(a1->value));
	}
	return 0;
}


static INT dogetclipboard(ELEMENT *e1)
{
	size_t cbText;
	CHAR *text;
	ATTRIBUTE *a1;
	ELEMENT e2, e3;
	IMAGEREF **imageptr;

	a1 = e1->firstattribute;
	if (a1 == NULL) return -2000;
	if (!memcmp(a1->tag, "image", 5)) {
		imageptr = findImageRef(a1);
		if (imageptr == NULL) return -2000;
		guicbdgetpixmap((*imageptr)->handle);
	}
	else { /* text */
		pvistart();
		cbText = 0;
		text = NULL;
		guicbdgettextlen(&cbText);
		if (cbText) {
			text = malloc(sizeof(CHAR) * (cbText + 1));
			if (text == NULL) return -1603;
			guicbdgettext(text, &cbText);
			text[cbText / sizeof(CHAR)] = '\0';
			e3.tag = text;
			e3.cdataflag = (INT) cbText;
			e3.firstsubelement = NULL;
			e3.nextelement = NULL;
			e3.firstattribute = NULL;
			e2.firstsubelement = &e3;
		}
		else e2.firstsubelement = NULL;
		pviend();

		e2.cdataflag = 0;
		e2.firstattribute = NULL;
		e2.nextelement = NULL;
		e2.tag = "r";

		if (sendelement(&e2, FALSE) < 0) {
			if (text != NULL) free(text);
			return -4000;
		}
		if (text != NULL) free(text);
	}
	return 0;
}


static INT dogetimage(ELEMENT *e1)
{
	INT i1;
	CHAR *bits, *bpp;
	IMAGEREF **imageptr;
	ATTRIBUTE *a1, a2, a3, a4;
	ELEMENT e2;

	a1 = e1->firstattribute;
	if (a1 == NULL) return -2000;
	imageptr = findImageRef(a1);
	if (imageptr == NULL) return -2000;
	bits = encodeimage((*imageptr)->handle, &i1);
	if (bits == NULL) return -1603; /* not enough memory */

	a4.tag = "bits";
	a4.value = bits;
	a4.nextattribute = NULL;
	a3.tag = "bpp";
	bpp = (CHAR *) malloc(sizeof(CHAR) * 3);
	if (bpp == NULL) return -1603;
	mscitoa((*(*imageptr)->handle)->bpp, bpp);
	a3.value = bpp;
	a3.nextattribute = &a4;
	a2.tag = "image";
	a2.value = (*imageptr)->name;
	a2.nextattribute = &a3;
	e2.tag = "image";
	e2.cdataflag = 0;
	e2.firstsubelement = NULL;
	e2.nextelement = NULL;
	e2.firstattribute = &a2;

	sendimageelement(&e2, i1);

	free(bits);
	free(bpp);
	return 0;
}


static INT dostoreimage(ELEMENT *e1)
{
	CHAR *bits = NULL;
	IMAGEREF **imageptr;
	ATTRIBUTE *a1;
#if OS_WIN32GUI
	PIXMAP *pix;
	PIXHANDLE pixhandle;
#endif

	a1 = e1->firstattribute;
	if (a1 == NULL) return -2000;
	imageptr = findImageRef(a1);
	if (imageptr == NULL) return -2000;
	a1 = a1->nextattribute;
	while (a1 != NULL) {
		if (memcmp(a1->tag, "bits", 4) == 0) bits = a1->value;
#if DX_MAJOR_VERSION >= 16 && OS_WIN32
		else if (strcmp(a1->tag, "usegrayscalepalette") == 0) (*(*imageptr)->handle)->isGrayScaleJpeg = TRUE;
#endif
		a1 = a1->nextattribute;
	}
	if (decodeimage((*imageptr)->handle, (UCHAR *) bits, (INT)strlen(bits)) == RC_NO_MEM) return -1603;

	/*
	 * If this is the Windows gui build of SC we now need to tell the OS
	 * to repaint the image.
	 */
#if OS_WIN32GUI
	pixhandle = (*imageptr)->handle;
	pix = *pixhandle;
	if (pix->winshow != NULL) forceRepaint(pixhandle);
#endif
	return 0;
}


static INT dogetpos(ELEMENT *e1)
{
	INT i1, hpos, vpos;
	CHAR work1[8], work2[8];
	IMAGEREF **imageptr;
	ATTRIBUTE *a1, a2, a3;
	ELEMENT e2;

	if (!guiflag) {
		if (cnsflag || guiCSCinit(breakeventid, &guiFontParseControl) != 0) {
			return -2003;
		}
		guiflag = TRUE;
	}

	a1 = e1->firstattribute;
	if (memcmp(a1->tag, "image", 5) == 0) {
		imageptr = findImageRef(a1);
		if (imageptr == NULL) return -1804;
		guipixgetpos((*imageptr)->handle, &hpos, &vpos);
		e2.tag = "sendpos";
		e2.cdataflag = 0;
		e2.firstsubelement = NULL;
		e2.nextelement = NULL;
		e2.firstattribute = &a2;
		a2.tag = "h";
		mscitoa(hpos, work1);
		a2.value = work1;
		a2.nextattribute = &a3;
		a3.tag = "v";
		mscitoa(vpos, work2);
		a3.value = work2;
		a3.nextattribute = NULL;
		i1 = sendelement(&e2, FALSE);
		if (i1 < 0) return -4000;
	}
	else return -2000;
	return 0;
}
#endif /* OS_WIN32 */

static INT dosound(ELEMENT *e1)
{
	ATTRIBUTE *a1, *a2;
	a1 = e1->firstattribute;
	if (a1 != NULL) a2 = a1->nextattribute;
	if (a1 == NULL || a2 == NULL) return -3100;

	if (guiflag) {
		guibeep(atoi(a1->value), atoi(a2->value), FALSE);
	}
	else {
		if (vidflag == FALSE) {
			if (kdsinit()) return -2002;
		}
		vidcmd[0] = VID_SOUND;
		vidcmd[1] = (((UINT) atoi(a1->value) << 16) | atoi(a2->value));
		vidcmd[2] = VID_END_FLUSH;
		if (vidput(vidcmd) < 0) return -5005;
	}

	return 0;
}

#if OS_WIN32
static INT dogetcolor(ELEMENT *e1)
{
	INT i1, hpos, vpos, color;
	CHAR work[12];
	IMAGEREF **imageptr;
	ATTRIBUTE *a1, a2;
	ELEMENT e2;

	if (!guiflag) {
		if (cnsflag || guiCSCinit(breakeventid, &guiFontParseControl) != 0) {
			return -2003;
		}
		guiflag = TRUE;
	}

	a1 = e1->firstattribute;
	if (memcmp(a1->tag, "image", 5) == 0) {
		imageptr = findImageRef(a1);
		if (imageptr == NULL) return -1804;
		guipixgetpos((*imageptr)->handle, &hpos, &vpos);
		guipixgetcolor((*imageptr)->handle, &color, hpos, vpos);
		e2.tag = "sendcolor";
		e2.cdataflag = 0;
		e2.firstsubelement = NULL;
		e2.nextelement = NULL;
		e2.firstattribute = &a2;
		a2.tag = "i";
		mscitoa(color, work);
		a2.value = work;
		a2.nextattribute = NULL;
		i1 = sendelement(&e2, FALSE);
		if (i1 < 0) return -4000;
	}
	else return -2000;
	return 0;
}
#endif

#if OS_WIN32
/* Resource call back function */
static void resourcecbfnc(RESHANDLE reshandle, UCHAR *data, INT datalen)
{
	guimessage(data, datalen);
}

/* Device call back function */
static void devicecbfnc(WINHANDLE winhandle, UCHAR *data, INT datalen)
{
	guimessage(data, datalen);
}

static void guimessage(UCHAR *data, INT datalen)
{
	static CHAR lastwindow[9], lastposx[5], lastposy[5];
	INT quote;
	CHAR func[5], value[9], val1[6], val2[6], val3[10], item[6];
	ELEMENT e1;
	ATTRIBUTE a1, a2, a3, a4, a5;

	pvistart();
	quote = FALSE;
	data[(datalen < 32000) ? datalen : datalen - 1] = '\0';
	memcpy(func, data + 8, 4); /* copy 4 char window message name */
	func[4] = '\0';
	e1.tag = func;
	e1.cdataflag = 0;
    e1.nextelement = e1.firstsubelement = NULL;
	a1.nextattribute = a2.nextattribute = a3.nextattribute = a4.nextattribute = a5.nextattribute = NULL;
	a1.value = a2.value = a3.value = a4.value = a5.value = NULL;

	/**
	 * window messages
	 *
	 * 01-08 resource name
	 * 09-12 function name
	 * 13-17 special
	 * 18-22 horizontal position
	 * 23-27 vertical position
	**/
	if (!memcmp(func, "POSN", 4) || !memcmp(func, "LBDN", 4) || !memcmp(func, "LBUP", 4) ||
		!memcmp(func, "LBDC", 4) || !memcmp(func, "MBDN", 4) || !memcmp(func, "MBUP", 4) ||
		!memcmp(func, "MBDC", 4) || !memcmp(func, "RBDN", 4) || !memcmp(func, "RBUP", 4) ||
		!memcmp(func, "RBDC", 4) || !memcmp(func, "WPOS", 4) || !memcmp(func, "POST", 4) ||
		!memcmp(func, "POSB", 4) || !memcmp(func, "POSL", 4) || !memcmp(func, "POSR", 4) ||
		!memcmp(func, "WSIZ", 4) || !memcmp(func, "WACT", 4) || !memcmp(func, "NKEY", 4) ||
		!memcmp(func, "CHAR", 4) || !memcmp(func, "VSTF", 4) || !memcmp(func, "WMIN", 4) ||
		!memcmp(func, "HSA-", 4) || !memcmp(func, "HSA+", 4) || !memcmp(func, "HSP-", 4) ||
		!memcmp(func, "HSP+", 4) || !memcmp(func, "HSTM", 4) || !memcmp(func, "HSTF", 4) ||
		!memcmp(func, "VSA-", 4) || !memcmp(func, "VSA+", 4) || !memcmp(func, "VSP-", 4) ||
		!memcmp(func, "VSP+", 4) || !memcmp(func, "VSTM", 4)
	) {
		lrtrim(value, data, 8);
		if (!memcmp(func, "POSN", 4)) {
			if (strcmp(lastwindow, value) != 0) {
				a1.tag = "n";
				a1.value = value;
				strcpy(lastwindow, value);
			}
		}
		else {
			a1.tag = "n";
			a1.value = value;
		}
		if (!memcmp(func, "POSN", 4)) {
			lrtrim(val1, data + 17, 5);
			lrtrim(val2, data + 22, 5);
			if (strcmp(val1, lastposx) != 0) {
				if (a1.value == NULL) {
					a1.tag = "h";
					a1.value = val1;
				}
				else {
					a2.tag = "h";
					a2.value = val1;
				}
				strcpy(lastposx, val1);
			}
			if (strcmp(val2, lastposy) != 0) {
				if (a1.value == NULL) {
					a1.tag = "v";
					a1.value = val2;
				}
				else if (a2.value == NULL) {
					a2.tag = "v";
					a2.value = val2;
				}
				else {
					a3.tag = "v";
					a3.value = val2;
				}
				strcpy(lastposy, val2);
			}
			if (a1.value == NULL) {
				pviend();
				return; /* h & v values are both the same */
			}
		}
		else if (!memcmp(func, "WACT", 4) || !memcmp(func, "WMIN", 4) || !memcmp(func, "POST", 4) ||
				!memcmp(func, "POSB", 4) || !memcmp(func, "POSL", 4) || !memcmp(func, "POSR", 4)) {
			/* do nothing */
		}
		else if (!memcmp(func, "CHAR", 4)) {
			a2.tag = "t";
			memcpy(val1, data + 17, 1);
			val1[1] = 0;
			a2.value = val1;
		}
		else if (!memcmp(func, "NKEY", 4)) {
			lrtrim(val1, data + 12, 5);
			lrtrim(val2, data + 17, 5);
			a2.tag = "t";
			a2.value = val2;
			if (val1[0]) {
				a3.tag = "i";
				a3.value = val1;
			}
		}
		else {
			/* Can't use + and - in XML element names */
			if (!memcmp(func, "HSA-", 4)) memcpy(e1.tag, "HSAM", 4);
			else if (!memcmp(func, "HSA+", 4)) memcpy(e1.tag, "HSAP", 4);
			else if (!memcmp(func, "HSP-", 4)) memcpy(e1.tag, "HSPM", 4);
			else if (!memcmp(func, "HSP+", 4)) memcpy(e1.tag, "HSPP", 4);
			else if (!memcmp(func, "VSA-", 4)) memcpy(e1.tag, "VSAM", 4);
			else if (!memcmp(func, "VSA+", 4)) memcpy(e1.tag, "VSAP", 4);
			else if (!memcmp(func, "VSP-", 4)) memcpy(e1.tag, "VSPM", 4);
			else if (!memcmp(func, "VSP+", 4)) memcpy(e1.tag, "VSPP", 4);
			lrtrim(val1, data + 17, 5);
			lrtrim(val2, data + 22, 5);
			a2.tag = "h";
			if (val1[0] == '\0') a2.value = "0";
			else a2.value = val1;
			a3.tag = "v";
			if (val2[0] == '\0') a3.value = "0";
			else a3.value = val2;
			if (!memcmp(func, "RB", 2)) {
				lrtrim(item, data + 12, 5);
				if (item[0]) {
					a4.tag = "i";
					a4.value = item;
				}
				lrtrim(val3, data + 27, 9);
				if (val3[0]) {
					a5.tag = "mkeys";
					if (!memcmp(val3, "SHIFT", 5)) a5.value = "2";
					else if (!memcmp(val3, "CTRLS", 5)) a5.value = "3";
					else a5.value = "1";
				}
			}
		}
	}

	/*
	 * other messages
	**/
	else if (!memcmp(func, "OK  ", 4)) {
		func[2] = '\0';
		a1.tag = "n";
		lrtrim(value, data, 8);
		a1.value = value;
		a2.tag = "t";
		/*data[datalen] = 0;  PROBLEM!*/
		a2.value = (CHAR *) data + 17;
	}
	else if (!memcmp(func, "CANC", 4)) {
		a1.tag = "n";
		lrtrim(value, data, 8);
		a1.value = value;
	}
	else if (!memcmp(func, "MENU", 4) || !memcmp(func, "FOCS", 4) ||
		!memcmp(func, "FOCL", 4) || !memcmp(func, "PUSH", 4))
	{
		a1.tag = "n";
		lrtrim(value, data, 8);
		a1.value = value;
		a2.tag = "i";
		lrtrim(val1, data + 12, 5);
		a2.value = val1;
		/*
		 * PUSH Data might be row number if this is from a popbox in a table
		 */
		if (datalen > 17) {
			a3.tag = "t";
			/*data[datalen] = 0;  PROBLEM!*/
			a3.value = (CHAR *) data + 17;
		}
	}
	else if (!memcmp(func, DBC_MSGITEM, 4) || !memcmp(func, DBC_MSGPICK, 4) ||
		!memcmp(func, DBC_MSGEDITSELECT, 4) || !memcmp(func, "DSEL", 4) ||
		!memcmp(func, DBC_MSGOPEN, 4) || !memcmp(func, DBC_MSGCLOSE, 4))
	{
		quote = TRUE;
		a1.tag = "n";
		lrtrim(value, data, 8);
		a1.value = value;
		if (!memcmp(func, DBC_MSGCLOSE, 4) && datalen <= 17) {
			/* do nothing - message is win CLOS not tree CLOS */
		}
		else {
			a2.tag = "i";
			lrtrim(val1, data + 12, 5);
			a2.value = val1;
			a3.tag = "t";
			/*data[datalen] = '\0';  PROBLEM!*/
			a3.value = (CHAR *) data + 17;
		}
	}
	else if (!memcmp(func, "SBA-", 4) || !memcmp(func, "SBA+", 4) ||
		!memcmp(func, "SBP-", 4) || !memcmp(func, "SBP+", 4) || !memcmp(func, "SBTM", 4) ||
		!memcmp(func, "SBTF", 4))
	{
		/* Can't use + and - in XML element names */
		if (!memcmp(func, "SBA-", 4)) memcpy(e1.tag, "SBAM", 4);
		else if (!memcmp(func, "SBA+", 4)) memcpy(e1.tag, "SBAP", 4);
		else if (!memcmp(func, "SBP-", 4)) memcpy(e1.tag, "SBPM", 4);
		else if (!memcmp(func, "SBP+", 4)) memcpy(e1.tag, "SBPP", 4);
		a1.tag = "n";
		lrtrim(value, data, 8);
		a1.value = value;
		a2.tag = "i";
		lrtrim(val1, data + 12, 5);
		a2.value = val1;
		a3.tag = "pos";
		lrtrim(val2, data + 17, 5);
		a3.value = val2;
	}
	else {
		pviend();
		return;
	}

	e1.tag[0] = tolower(e1.tag[0]);
	e1.tag[1] = tolower(e1.tag[1]);
	if (memcmp(e1.tag, "ok", 2) != 0) {
		e1.tag[2] = tolower(e1.tag[2]);
		e1.tag[3] = tolower(e1.tag[3]);
	}
	e1.firstattribute = &a1;
	if (a2.value != NULL) a1.nextattribute = &a2;
	if (a3.value != NULL) a2.nextattribute = &a3;
	if (a4.value != NULL) a3.nextattribute = &a4;
	if (a5.value != NULL) a4.nextattribute = &a5;

	pviend();
	sendelement(&e1, quote);
}
#endif

#if OS_WIN32
static INT parsenumber(CHAR *text)
{
	INT i1, pos, end;

	end = (INT)strlen(text);
	i1 = pos = 0;
	while (pos <= end && *text == ' ') {
		text++;
		pos++;
	}
	if (pos > end || !isdigit(*text)) return i1;
	while (pos <= end && isdigit(*text)) i1 = i1 * 10 + *text++ - '0';
	while (pos <= end && *text == ' ') text++;
	return i1;
}
#endif

#if OS_WIN32
static INT ctlmemicmp(void *src, void *dest, INT len)
{
	while (len--) if (toupper(((UCHAR *) src)[len]) != toupper(((UCHAR *) dest)[len])) return 1;
	return 0;
}
#endif

static void maptranslate(INT output, CHAR *data)
{
	INT i1;
	if (latin1flag) output = !output;
	for (i1 = 0; data[i1]; i1++) {
		if ((UCHAR)data[i1] >= 0x80) {
			if (output) data[i1] = latin1map[(UCHAR)data[i1] - 0x80];
			else data[i1] = pcbiosmap[(UCHAR)data[i1] - 0x80];
		}
	}
}

static void timstructToString(TIMSTRUCT *timstruct, CHAR *timestamp, INT size) {
	INT i, j;
	if (size < 4) return;
	i = timstruct->yr;
	for (j = 4; j--; i /= 10) timestamp[j] = (CHAR)(i % 10 + '0');
	if (size < 6) return;
	i = timstruct->mon;
	timestamp[4] = (CHAR)(i / 10 + '0');
	timestamp[5] = (CHAR)(i % 10 + '0');
	if (size < 8) return;
	i = timstruct->day;
	timestamp[6] = (CHAR)(i / 10 + '0');
	timestamp[7] = (CHAR)(i % 10 + '0');
	if (size < 10) return;
	i = timstruct->hr;
	timestamp[8] = (CHAR)(i / 10 + '0');
	timestamp[9] = (CHAR)(i % 10 + '0');
	if (size < 12) return;
	i = timstruct->min;
	timestamp[10] = (CHAR)(i / 10 + '0');
	timestamp[11] = (CHAR)(i % 10 + '0');
	if (size < 14) return;
	i = timstruct->sec;
	timestamp[12] = (CHAR)(i / 10 + '0');
	timestamp[13] = (CHAR)(i % 10 + '0');
	if (size < 16) return;
	i = timstruct->hun;
	timestamp[14] = (CHAR)(i / 10 + '0');
	timestamp[15] = (CHAR)(i % 10 + '0');
}
