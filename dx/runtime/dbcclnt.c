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

#define INC_STDIO
#define INC_STDLIB
#define INC_STRING
#define INC_CTYPE
#define INC_SIGNAL
#define INC_ERRNO
#include "includes.h"

#if OS_UNIX
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifndef __MACOSX
#include <poll.h>
#endif
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
#endif

#if OS_WIN32
#define ERRORVALUE() WSAGetLastError()
#else
#define ERRORVALUE() errno
#endif

#ifndef INADDR_NONE
#define INADDR_NONE (unsigned long) 0xFFFFFFFF
#endif

#include "release.h"
#include "base.h"
#include "cert.h"
#include "dbc.h"
#include "dbcclnt.h"
#include "evt.h"
#include "vid.h"
#include "vidx.h"
#include "xml.h"
#include "tcp.h"
#include "tim.h"

#define EVENTS_READ (DEVPOLL_READ | DEVPOLL_READPRIORITY | DEVPOLL_READNORM | DEVPOLL_READBAND)
#define EVENTS_WRITE (DEVPOLL_WRITE)
#define EVENTS_ERROR (DEVPOLL_ERROR | DEVPOLL_HANGUP | DEVPOLL_NOFDES)

#define SEND_INITSIZE	2048
#define SEND_SNUMOFF	0
#define SEND_SNUMLEN	8
#define SEND_SIZEOFF	(SEND_SNUMOFF + SEND_SNUMLEN)
#define SEND_SIZELEN	8
#define SEND_HEADER		(SEND_SNUMLEN + SEND_SIZELEN)

#define RECV_INITSIZE	2048
#define RECV_SNUMOFF	0
#define RECV_SNUMLEN	8
#define RECV_SIZEOFF	(RECV_SNUMOFF + RECV_SNUMLEN)
#define RECV_SIZELEN	8
#define RECV_HEADER		(RECV_SNUMLEN + RECV_SIZELEN)

#define TRAP_INITSIZE	128
#define ELEM_INITSIZE	2048

#define CLIENT_FLAG_INIT		0x01
#define CLIENT_FLAG_CONNECT		0x02
#define CLIENT_FLAG_SENDNOMEM	0x04
#define CLIENT_FLAG_ELEMENT		0x08
#define CLIENT_FLAG_SHUTDOWN	0x10

//#define CELLWIDTH 3

/* cursor mode */
#define CURSMODE_NORMAL	(UCHAR) 0
#define CURSMODE_OFF	(UCHAR) 1
#define CURSMODE_ON		(UCHAR) 2

#define KYNATTR_EDIT			0x00000001
#define KYNATTR_EDITON			0x00000002
#define KYNATTR_EDITMODE		0x00000004
#define KYNATTR_EDITOVERSTRIKE	0x00000008
#define KYNATTR_EDITNUM			0x00000010
#define KYNATTR_EDITFIRSTNUM	0x00000020
#define KYNATTR_EDITSPECIAL		0x00000040
#define KYNATTR_DIGITENTRY		0x00000080
#define KYNATTR_JUSTRIGHT		0x00000100
#define KYNATTR_JUSTLEFT		0x00000200
#define KYNATTR_ZEROFILL 		0x00000400
#define KYNATTR_AUTOENTER		0x00000800
#define KYNATTR_RIGHTTOLEFT		0x00001000
#define KYNATTR_CLICK			0x00002000
#define KYNATTR_NOECHO			0x00004000
#define KYNATTR_ECHOSECRET		0x00008000
#define KYNATTR_KEYCASENORMAL	0x01000000
#define KYNATTR_KEYCASEUPPER	0x02000000
#define KYNATTR_KEYCASELOWER	0x04000000
#define KYNATTR_KEYCASEINVERT	0x08000000
#define KYNATTR_KEYCASEMASK		0x0F000000

/* local variables */
/*** CODE: SHOULD MAYBE PUT PVISTART/END AROUND ALL REFERENCES FOR OS_WIN32 ***/
static INT clientflags = 0;
static INT tcpflags = 0;
static SOCKET sockethandle = INVALID_SOCKET;
static INT clientRolloutInEffect;
static INT clientRolloutTime; 	// in seconds
static int serialnum = 1;
static int shutdowneventid;
static int breakeventid;
static int recveventid;
static int workeventid;
static void (*guicallback)(ELEMENT *) = NULL;
static FILE *logfile = NULL;
extern LONG keepalivetime;		/* keep alive messages time interval, in seconds */

static char trapactivemap[(VID_MAXKEYVAL >> 3) + 1];
static int trapeventid;
static int trapbuffer[TRAP_INITSIZE];
static int traphead = 0;
static int traptail = 0;

static char *sendbuffer;
static int sendbufcnt = SEND_HEADER;
static int sendbufsize = 0;

static char *recvbuffer;
static int recvbufcnt = 0;
static int recvbufsize = 0;

static char *elembuffer;
static int elembufsize;
static ELEMENT *elementptr = NULL;

static CHAR clienterror[256];

/* vid emulation variables */
static INT maxlines, maxcolumns;
static INT vidh, vidv;
//static INT wsctop, wscbot, wsclft, wscrgt;
static INT kynattr;
static INT viddblflag;
static UCHAR keyinechochar;
static UCHAR vidcurmode;
static UCHAR vidcurtype;
static INT vidflags;
#if OS_WIN32
static INT **clientbuffer;
typedef INT ATTR;
#else
static UINT64 **clientbuffer;
typedef UINT64 ATTR;
#endif
static UCHAR finishmap[(VID_MAXKEYVAL >> 3) + 1];

#if OS_WIN32
static HANDLE threadevent;
static HANDLE threadhandle;
static DWORD threadid;
#endif

static void buferaserect(INT top, INT bottom, INT left, INT right, ATTR attr);
static void bufmove(INT direction, INT top, INT bottom, INT left, INT right, ATTR attr);
static void bufinsertchar(INT h, INT v, INT top, INT bottom, INT left, INT right, ATTR charattr);
static void bufdeletechar(INT h, INT v, INT top, INT bottom, INT left, INT right, ATTR charattr);
static void bufgetrect(UCHAR *mem, INT top, INT bottom, INT left, INT right, INT charonly);
static void bufputrect(UCHAR *mem, INT top, INT bottom, INT left, INT right, INT charonly, ATTR attr);
static void clientPutOptions(CHAR *key1);
static void processrecv(void);
static void clearbuffer(void);
static INT ntoi(UCHAR *, INT, INT *);
static int vidputColor(INT32 cmd, CHAR* ptr, INT vidCode);

#if OS_WIN32
static DWORD WINAPI recvproc(LPVOID lpParam);
#endif

#if OS_UNIX
static INT recvcallback(void *, INT);
#endif

/**
 * Called from only one place. dbcinit() int dbcctl.c
 */
int clientstart(char *client, int clientport, int encryptflag, int serverport, int user, FILE *log, char* certificatefilename)
{
	INT i1, len, pos;
	SOCKET tempsockethandle;
	BIO* certFileBio = NULL;

	clienterror[0] = '\0';
	logfile = log;
	tcpflags = TCP_UTF8;
	if (encryptflag) {
		tcpflags |= TCP_SSL;
		certFileBio = GetCertBio(certificatefilename);
		if (certFileBio == NULL) {
			strcpy(clienterror, GetCertBioErrorMsg());
			return RC_ERROR;
		}
	}

	sendbuffer = (CHAR *) malloc(SEND_INITSIZE + 2);
	recvbuffer = (CHAR *) malloc(RECV_INITSIZE + 2);
	elembuffer = (CHAR *) malloc(ELEM_INITSIZE);
	if (sendbuffer == NULL || recvbuffer == NULL || elembuffer == NULL) {
		if (elembuffer != NULL) free(elembuffer);
		if (recvbuffer != NULL) free(recvbuffer);
		if (sendbuffer != NULL) free(sendbuffer);
		strcpy(clienterror, "memory allocation failed");
		return RC_NO_MEM;
	}
	sendbufsize = SEND_INITSIZE;
	recvbufsize = RECV_INITSIZE;
	elembufsize = ELEM_INITSIZE;

	if (!*client) {
		tempsockethandle = tcplisten(clientport, &clientport);
		if (tempsockethandle == INVALID_SOCKET) strcpy(clienterror, tcpgeterror());
		/*CODE   remove hard coded address below, replace with ??? */
		sockethandle = tcpconnect("127.0.0.1", serverport, TCP_UTF8, NULL, 15);
		if (sockethandle == INVALID_SOCKET) {
			strcpy(clienterror, tcpgeterror());
			if (tempsockethandle != INVALID_SOCKET) closesocket(tempsockethandle);
			tcpcleanup();
			return RC_ERROR;
		}
		memset(sendbuffer, ' ' , 8);
		i1 = SEND_HEADER;
		if (tempsockethandle == INVALID_SOCKET) {
			strcpy(sendbuffer + i1, "<err user=");
			i1 += (INT) strlen(sendbuffer + i1);
			i1 += tcpitoa(user, sendbuffer + i1);
			sendbuffer[i1++] = '>';
			strcpy(sendbuffer + i1, clienterror);
			i1 += (INT) strlen(sendbuffer + i1);
			strcpy(sendbuffer + i1, "</err>");
			i1 += (INT) strlen(sendbuffer + i1);
		}
		else {
			strcpy(sendbuffer + i1, "<verify user=");
			i1 += (INT) strlen(sendbuffer + i1);
			i1 += tcpitoa(user, sendbuffer + i1);
			sendbuffer[i1++] = '/';
			sendbuffer[i1++] = '>';
		}
		tcpiton(i1 - SEND_HEADER, (UCHAR *) sendbuffer + 8, 8);
		i1 = tcpsend(sockethandle, (UCHAR *) sendbuffer, i1, TCP_UTF8, -1);
		if (i1 == -1 || tempsockethandle == INVALID_SOCKET) {
			if (tempsockethandle != INVALID_SOCKET) {
				strcpy(clienterror, tcpgeterror());
				closesocket(tempsockethandle);
			}
			closesocket(sockethandle);
			tcpcleanup();
			return RC_ERROR;
		}
		pos = 0;
		for ( ; ; ) {
			i1 = tcprecv(sockethandle, (UCHAR *) recvbuffer + pos, recvbufsize - pos, TCP_UTF8, 30);
			if (i1 <= 0) {
				strcpy(clienterror, tcpgeterror());
				closesocket(sockethandle);
				closesocket(tempsockethandle);
				tcpcleanup();
				return RC_ERROR;
			}
			pos += i1;
			if (pos >= 16) {
				if (memcmp(recvbuffer, sendbuffer, 8)) {
					strcpy(clienterror, "serialization failure");
					closesocket(sockethandle);
					closesocket(tempsockethandle);
					tcpcleanup();
					return RC_ERROR;
				}
				tcpntoi((UCHAR *) recvbuffer + 8, 8, &len);
				if (len <= 0 || len > recvbufsize - 16) {
					strcpy(clienterror, "corrupted packet");
					closesocket(sockethandle);
					closesocket(tempsockethandle);
					tcpcleanup();
					return RC_ERROR;
				}
				if (pos >= 16 + len) break;
			}
		}
		closesocket(sockethandle);
		i1 = xmlparse(recvbuffer + 16, len, elembuffer, elembufsize);
		if (i1) {
			recvbuffer[16 + len] = '\0';
			sprintf(clienterror, "XML Parsing failed (%d) : XML : %s", i1, recvbuffer + 16);
			closesocket(tempsockethandle);
			tcpcleanup();
			return RC_ERROR;
		}
		sockethandle = tcpaccept(tempsockethandle, tcpflags | TCP_SERV, certFileBio, 30);
	}
	else {
		sockethandle = tcpconnect(client, clientport, tcpflags | TCP_SERV, certFileBio, 30);
	}
	if (sockethandle == INVALID_SOCKET) {
		strcpy(clienterror, tcpgeterror());
		tcpcleanup();
		return RC_ERROR;
	}
	i1 = 1;
	setsockopt(sockethandle, SOL_SOCKET, SO_KEEPALIVE, (char *) &i1, sizeof(i1));
/*** UNIX NOTE: BY SETTING KEEPALIVE, SIGPIPE WILL BE SIGNALED IF DISCONNECT IS ***/
/***            DETECTED. SIGPIPE IS CURRENTLY BE CAUGHT IN EVT AND IGNORED. ***/
/***            PROBLEM IS THAT SIGNAL COULD BE FOR ANOTHER SOCKET. ***/
	clientflags |= CLIENT_FLAG_CONNECT;

#if OS_WIN32
	if ((threadevent = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL ||
	    (threadhandle = CreateThread(NULL, 0, recvproc, NULL, 0, &threadid)) == NULL) {
		sprintf(clienterror, "unable to start receive thread, error = %d", ERRORVALUE());
		if (threadevent != NULL) CloseHandle(threadevent);
		clientend(clienterror);
		return RC_ERROR;
	}
#endif

#if OS_UNIX
	if (evtdevinit(sockethandle, EVENTS_READ | EVENTS_ERROR, recvcallback, (void *) (ULONG) sockethandle) < 0) {
		sprintf(clienterror, "unable to initialize socket device, error = %d", ERRORVALUE());
		clientend(clienterror);
		return RC_ERROR;
	}
#endif
	clientflags |= CLIENT_FLAG_INIT;
	return 0;
}

int clientevents(int shutdownid, int breakid, void (*guicb)(ELEMENT *))
{
	clienterror[0] = '\0';
	shutdowneventid = shutdownid;
	breakeventid = breakid;
	guicallback = guicb;
	workeventid = evtcreate();
	if (!workeventid) {
		strcpy(clienterror, "evtcreate failed");
		return RC_ERROR;
	}
	return 0;
}

void clientend(CHAR *message)
{

	if (clientflags & CLIENT_FLAG_INIT) {
#if OS_WIN32
		TerminateThread(threadhandle, 0);
		WaitForSingleObject(threadhandle, 500);
		CloseHandle(threadhandle);
#endif
#if OS_UNIX
		evtdevexit(sockethandle);
#endif
	}

	if (clientflags & CLIENT_FLAG_CONNECT) {
		sendbufcnt = SEND_HEADER;
		if (*message) {
			clientput("<quit>", 6);
			clientputdata(message, -1);
			clientput("</quit>", 7);
		}
		else clientput("<quit/>", 7);
		clientsend(0, 0);
		if (tcpflags & TCP_SSL) tcpsslcomplete(sockethandle);
		closesocket(sockethandle);
		tcpcleanup();
	}
	clientflags &= ~(CLIENT_FLAG_INIT | CLIENT_FLAG_CONNECT);
}

/**
 * When set to TRUE, the normal keepalivetime is extended.
 * This was added to deal with very long clientrollouts.
 * And the fact that the C Smart Client (and only the CSC) cannot
 * send the <alivechk/> message while waiting for the rollout.
 */
void setClientrollout(INT val) {
	clientRolloutInEffect = val;
	if (val) clientRolloutTime = 30 * 60; 	// in seconds, give them a half hour
	else clientRolloutTime = 0;
}


void clientput(CHAR *str, int len)
{
	INT size;
	CHAR *ptr;

	if (clientflags & CLIENT_FLAG_SENDNOMEM) return;
	if (len == -1) len = (INT) strlen(str);
	if (sendbufcnt + len > sendbufsize) {
		for (size = sendbufsize << 1; sendbufcnt + len > size; size <<= 1);
		ptr = (CHAR *) realloc(sendbuffer, size + 2);
		if (ptr == NULL) {
			clientflags |= CLIENT_FLAG_SENDNOMEM;
			sendbufcnt = SEND_HEADER;
			return;
		}
		sendbuffer = ptr;
		sendbufsize = size;
	}
	memcpy(sendbuffer + sendbufcnt, str, len);
	sendbufcnt += len;
}

/**
 * Convert special characters in str to built-in entities
 */
void clientputdata(char *str, int len)
{
	INT i1, i2, i3;
	CHAR escape[16];
	UCHAR chr;

	if (len == -1) len = (INT) strlen(str);
	for (i1 = i2 = 0; i1 <= len; i1++) {
		if (i1 == len || str[i1] == '<' || str[i1] == '>' || str[i1] == '&' || str[i1] == '"'
			|| str[i1] == '\'' || (UCHAR) str[i1] < 0x20 || (UCHAR) str[i1] >= 0x7F)
		{
			clientput(str + i2, i1 - i2);
			if (i1 == len) break;
			chr = (UCHAR) str[i1];
			if (chr == '<') clientput("&lt;", 4);
			else if (chr == '>') clientput("&gt;", 4);
			else if (chr == '&') clientput("&amp;", 5);
			else if (chr == '"') clientput("&quot;", 6);
			else if (chr == '\'') clientput("&apos;", 6);
			else {
				escape[sizeof(escape) - 1] = ';';
				i3 = sizeof(escape) - 1;
				do escape[--i3] = (CHAR)(chr % 10 + '0');
				while (chr /= 10);
				escape[--i3] = '#';
				escape[--i3] = '&';
				clientput(&escape[i3], sizeof(escape) - i3);
			}
			i2 = i1 + 1;
		}
	}
}

void clientputnum(int num)
{
	CHAR work[32];

	mscitoa(num, work);
	clientput(work, -1);
}

int clientsendgreeting() {
	CHAR *ptr;

	/* process keyin/vid options */
	clientput("<smartserver version=\"", 22);
	clientput(RELEASE, -1);
	clientput("\">", 2);

	clientPutOptions("keyin");

	clientPutOptions("display");

	clientPutOptions("gui");

	if (!prpget("beep", NULL, NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) {
		clientput("<beep>old</beep>", -1);
	}
	if (!prpget("rolloutwindow", NULL, NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) {
		clientput("<rolloutwindow>on</rolloutwindow>", -1);
	}
	if (!prpget("client", "memalloc", NULL, NULL, &ptr, PRP_LOWER)) {
		clientput("<memalloc>", -1);
		clientput(ptr, -1);
		clientput("</memalloc>", -1);
	}
	checkClientFileTransferOptions();
	if (!prpget("client", "terminate", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "force")) {
		clientput("<terminate>force</terminate>", -1);
	}
	clientPutOptions("print");

	if (smartClientSubType[0] != '\0') {
		if (strlen(smartClientSubType) == 7 && !strcmp(smartClientSubType, "Servlet"))
			clientPutOptions("websc");
		else if (strlen(smartClientSubType) == 4 && !strcmp(smartClientSubType, "Java"))
			clientPutOptions("javasc");
		else if (strlen(smartClientSubType) == 7 && !strcmp(smartClientSubType, "Android"))
			clientPutOptions("andrsc");
	}

	clientput("</smartserver>", -1);
	if (clientsend(0, 0) < 0) return RC_ERROR;

	return 0;
}

static void clientPutOptions(CHAR *key1) {
	CHAR *ptr, *name, *name2, work[256];
	ELEMENT *e1;

	if (!prpget(key1, NULL, NULL, NULL, &ptr, PRP_GETCHILD)) {
		sprintf(work, "<%s>", key1);
		clientput(work, -1);

		do {
			prpname(&name);
			sprintf(work, "<%s>", name);
			clientput(work, -1);
			e1 = prpgetprppos();
			if (!prpget(key1, name, NULL, NULL, &ptr, PRP_GETCHILD)) {
				do {
					prpname(&name2);
					sprintf(work, "<%s>", name2);
					clientput(work, -1);
					clientputdata(ptr, -1);
					sprintf(work, "</%s>", name2);
					clientput(work, -1);
				} while (!prpget(key1, name, NULL, NULL, &ptr, PRP_NEXT));
			}
			else {
				clientput(ptr, -1);
			}
			sprintf(work, "</%s>", name);
			clientput(work, -1);
			prpsetprppos(e1);
		} while (!prpget(key1, NULL, NULL, NULL, &ptr, PRP_NEXT));

		sprintf(work, "</%s>", key1);
		clientput(work, -1);
	}
}

/**
 *  initialize video and ending keys map
 */
int clientvidinit()
{
#if OS_WIN32
	INT *buf;
#else
	UINT64 *buf;
#endif
	INT i1, i2;
	CHAR *ptr;
	ATTRIBUTE *a1;

	/* set up ending keys */
	memset(finishmap, 0x00, sizeof(finishmap));
	finishmap[VID_ENTER >> 3] |= 1 << (VID_ENTER & 0x07);
	for (i1 = VID_F1; i1 <= VID_F20; i1++) finishmap[i1 >> 3] |= 1 << (i1 & 0x07);
	if (!prpget("keyin", "fkeyshift", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) {
		for (i1 = VID_SHIFTF1; i1 <= VID_SHIFTF10; i1++) finishmap[i1 >> 3] |= 1 << (i1 & 0x07);
	}
	for (i1 = VID_UP; i1 <= VID_RIGHT; i1++) finishmap[i1 >> 3] |= 1 << (i1 & 0x07);
	if (!prpget("keyin", "endkey", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "xkeys")) {
		for (i1 = VID_INSERT; i1 <= VID_PGDN; i1++) finishmap[i1 >> 3] |= 1 << (i1 & 0x07);
		finishmap[VID_ESCAPE >> 3] |= 1 << (VID_ESCAPE & 0x07);
		finishmap[VID_TAB >> 3] |= 1 << (VID_TAB & 0x07);
		finishmap[VID_BKTAB >> 3] |= 1 << (VID_BKTAB & 0x07);
	}

#ifdef NOCLIENT
	maxlines = 25
	maxcolumns = 80;
#else

	clientput("<getwindow/>", 12);
	if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) return RC_ERROR;
	if (elementptr != NULL && !strcmp(elementptr->tag, "getwindow")) {
		a1 = elementptr->firstattribute;
		while (a1 != NULL) {
			if (!strcmp(a1->tag, "b")) maxlines = strtol(a1->value, NULL, 10) + 1;
			else if (!strcmp(a1->tag, "r")) maxcolumns = strtol(a1->value, NULL, 10) + 1;
			a1 = a1->nextattribute;
		}
	}
	clientrelease();
#endif
	if (!maxlines || !maxcolumns) return RC_ERROR;

	i2 = maxlines * maxcolumns;
#if OS_WIN32
	clientbuffer = (INT **) memalloc(i2 * sizeof(INT), 0);
#else
	clientbuffer = (UINT64 **) memalloc(i2 * sizeof(UINT64), 0);
#endif
	if (clientbuffer == NULL) return RC_ERROR;
	buf = *clientbuffer;
	kdsConfigColorMode(&vidflags);
	for (i1 = 0; i1 < i2; )
		buf[i1++] = (DSPATTR_WHITE << DSPATTR_FGCOLORSHIFT) | (DSPATTR_BLACK << DSPATTR_BGCOLORSHIFT) | ' ';

	dspattr = DSPATTR_ROLL + (DSPATTR_WHITE << DSPATTR_FGCOLORSHIFT);
	kynattr = KYNATTR_KEYCASENORMAL;

	keyinechochar = '*';

	vidh = vidv = 0;
	wscbot = maxlines - 1;
	wscrgt = maxcolumns - 1;
	wsclft = wsctop = 0;

	if (vidflags & VID_FLAG_COLORMODE_ANSI256) {
		VID_BLACK	= 0x0000;
		VID_RED		= 0x0001;
		VID_GREEN	= 0x0002;
		VID_YELLOW	= 0x0003;
		VID_BLUE	= 0x0004;
		VID_MAGENTA	= 0x0005;
		VID_CYAN	= 0x0006;
		VID_WHITE	= 0x0007;
	}
	else {
		VID_BLACK	= 0x0000;
		VID_BLUE	= 0x0001;
		VID_GREEN	= 0x0002;
		VID_CYAN	= 0x0003;
		VID_RED		= 0x0004;
		VID_MAGENTA	= 0x0005;
		VID_YELLOW	= 0x0006;
		VID_WHITE	= 0x0007;
	}
	return 0;
}

/**
 * Does not need a size argument. The usage of this function guarantees
 * that the size of 'memory' is big enough.
 * It is sizeof(STATESAVE) which is a bit larger than we need here
 *
 * Order;
 * lft, top, rgt, bot, h, v, dspattr, viddblflg, vidcurmode, vidcurtype, keyinechochar, kynattr,
 * validmap (zeroed), finishmap
 */
void clientstatesave(UCHAR *memory)
{
	STATESAVE savearea;

	// Step 1  Move components to the structure
	savearea.wsclft = (USHORT)wsclft;
	savearea.wsctop = (USHORT)wsctop;
	savearea.wscrgt = (USHORT)wscrgt;
	savearea.wscbot = (USHORT)wscbot;
	savearea.vidh = (USHORT)vidh;
	savearea.vidv = (USHORT)vidv;
	savearea.dspattr = dspattr;
	savearea.viddblflag = (UCHAR)viddblflag;
	savearea.vidcurmode = vidcurmode;
	savearea.vidcurtype = vidcurtype;
	savearea.keyinechochar = keyinechochar;
	savearea.kynattr = kynattr;
	memset((void*)savearea.validmap, 0, sizeof(savearea.validmap));
	memcpy((void*)savearea.finishmap, finishmap, sizeof(savearea.finishmap));
	// Step 2  Move the structure to the caller's memory
	memcpy((void*)memory, &savearea, sizeof(STATESAVE));
}

/**
 * Does not need a size argument. The usage of this function guarantees
 * that the size of 'memory' is big enough.
 * It is sizeof(STATESAVE) which is a bit larger than we need here
 */
void clientstaterestore(UCHAR *memory)
{
	STATESAVE savearea;

	// Step 1  Move from caller's memory to the structure
	memcpy((void*)&savearea, (void*)memory, sizeof(STATESAVE));
	// Step 2  Move the structure components to our fields
	wsclft = savearea.wsclft;
	wsctop = savearea.wsctop;
	wscrgt = savearea.wscrgt;
	wscbot = savearea.wscbot;
	vidh = savearea.vidh;
	vidv = savearea.vidv;
	dspattr = savearea.dspattr;
	viddblflag = savearea.viddblflag;
	vidcurmode = savearea.vidcurmode;
	vidcurtype = savearea.vidcurtype;
	keyinechochar = savearea.keyinechochar;
	kynattr = savearea.kynattr;
}

int clientvidput(INT32 *cmd, INT flags)
{
	static INT32 finish[] = { VID_KBD_EON, VID_KBD_ESOFF, VID_END_NOFLUSH };
	static CHAR *scrollend = NULL;
#if OS_WIN32
	INT *buf;
#else
	UINT64 *buf;
#endif
	INT i1, i2, i3, i4, dataflag, len, repeatcnt;
	INT32 *repeatcmd;
	CHAR escape[16], work[65], *ptr;
	UCHAR chr;

	repeatcnt = 0;
	for ( ; ; ) {
		dataflag = FALSE;
		ptr = work;
		len = 0;
		switch(((*cmd) >> 16)) {
		case (VID_FINISH >> 16):
			kynattr &= ~(KYNATTR_JUSTRIGHT | KYNATTR_JUSTLEFT | KYNATTR_ZEROFILL);
			/*** IGNORE TURNING OFF KYNATTR_RIGHTTOLEFT (UNSUPPORTED) AND KYNATTR_AUTOENTER (SET ON EVERY KEYIN) ***/
			clientvidput(finish, flags);
			scrollend = NULL;
			/* fall through */
		case (VID_END_FLUSH >> 16):
		case (VID_END_NOFLUSH >> 16):
			return 0;
		case (VID_DISPLAY >> 16):
			len = *cmd++ & 0xFFFF;
			if (sizeof(CHAR *) > sizeof(INT32)) {
				memcpy((CHAR *) &ptr, (CHAR *) cmd, sizeof(CHAR *));
				cmd += ((sizeof(CHAR *) + sizeof(INT32) - 1) / sizeof(INT32)) - 1;
			}
			else ptr = (CHAR *) (ULONG_PTR) *cmd;
			if (len) {
				if (!(dspattr & (DSPATTR_RAWMODE | DSPATTR_AUXPORT))) {
					if (!(dspattr & DSPATTR_DISPDOWN)) {
						if (!(dspattr & DSPATTR_DISPLEFT)) {
							i2 = wscrgt - vidh + 1;
							i3 = 1;
						}
						else {
							i2 = vidh - wsclft + 1;
							i3 = -1;
						}
					}
					else if (!(dspattr & DSPATTR_DISPLEFT)) {
						i2 = wscbot - vidv + 1;
						i3 = maxcolumns;
					}
					else {
						i2 = vidv - wsctop + 1;
						i3 = -maxcolumns;
					}
					if (len > i2) len = i2;
					if (flags & CLIENTVIDPUT_SAVE) {
						for (i1 = 0, buf = *clientbuffer + vidh + vidv * maxcolumns; i1 < len; i1++, buf += i3)
							*buf = (dspattr & (DSPATTR_ATTRIBMASK | DSPATTR_FGCOLORMASK | DSPATTR_BGCOLORMASK)) | (UCHAR) ptr[i1];
					}
					i1 = len;
					if (i1 == i2) i1--;
					if (!(dspattr & DSPATTR_DISPDOWN)) {
						if (!(dspattr & DSPATTR_DISPLEFT)) vidh += i1;
						else vidh -= i1;
					}
					else if (!(dspattr & DSPATTR_DISPLEFT)) vidv += i1;
					else vidv -= i1;
				}
				dataflag = TRUE;
			}
			break;
		case (VID_REPEAT >> 16):
			repeatcnt = *cmd++ & 0xFFFF;
			if (repeatcnt) repeatcmd = cmd;
			break;
		case (VID_DISP_CHAR >> 16):
			len = repeatcnt + 1;
			repeatcnt = 0;
			chr = (UCHAR) *cmd;
			if (!(dspattr & (DSPATTR_RAWMODE | DSPATTR_AUXPORT))) {
				if (!(dspattr & DSPATTR_DISPDOWN)) {
					if (!(dspattr & DSPATTR_DISPLEFT)) {
						i2 = wscrgt - vidh + 1;
						i3 = 1;
					}
					else {
						i2 = vidh - wsclft + 1;
						i3 = -1;
					}
				}
				else if (!(dspattr & DSPATTR_DISPLEFT)) {
					i2 = wscbot - vidv + 1;
					i3 = maxcolumns;
				}
				else {
					i2 = vidv - wsctop + 1;
					i3 = -maxcolumns;
				}
				if (len > i2) len = i2;
				if (flags & CLIENTVIDPUT_SAVE) {
					i4 = (dspattr & (DSPATTR_ATTRIBMASK | DSPATTR_FGCOLORMASK | DSPATTR_BGCOLORMASK)) | chr;
					for (i1 = 0, buf = *clientbuffer + vidh + vidv * maxcolumns; i1 < len; i1++, buf += i3)
						*buf = i4;
				}
				i1 = len;
				if (i1 == i2) i1--;
				if (!(dspattr & DSPATTR_DISPDOWN)) {
					if (!(dspattr & DSPATTR_DISPLEFT)) vidh += i1;
					else vidh -= i1;
				}
				else if (!(dspattr & DSPATTR_DISPLEFT)) vidv += i1;
				else vidv -= i1;
			}
			if (chr == '<') {
				ptr = "&lt;";
				i1 = 4;
			}
			else if (chr == '>') {
				ptr = "&gt;";
				i1 = 4;
			}
			else if (chr == '&') {
				ptr = "&amp;";
				i1 = 5;
			}
			else if (chr == '"') {
				ptr = "&quot;";
				i1 = 6;
			}
			else if (chr == '\'') {
				ptr = "&apos;";
				i1 = 6;
			}
			else if (chr < 0x20 || chr >= 0x7F) {
				escape[sizeof(escape) - 1] = ';';
				i2 = sizeof(escape) - 1;
				do escape[--i2] = (CHAR)(chr % 10 + '0');
				while (chr /= 10);
				escape[--i2] = '#';
				escape[--i2] = '&';
				ptr = &escape[i2];
				i1 = sizeof(escape) - i2;
			}
			else {
				ptr = (CHAR *) &chr;
				i1 = 1;
			}
			if (!(dspattr & DSPATTR_DISPDOWN) && (len * i1) < 30) {
				if (i1 == 1) memset(work, (UCHAR) *ptr, len);
				else {
					len *= i1;
					for (i2 = 0; i2 < len; i2 += i1) memcpy(work + i2, ptr, i1);
				}
			}
			else {
				if (!(dspattr & DSPATTR_DISPDOWN)) memcpy(work, "<rptchar n=", 11);
				else memcpy(work, "<rptdown n=", 11);
				len = mscitoa(len, work + 11) + 11;
				work[len++] = '>';
				memcpy(work + len, ptr, i1);
				len += i1;
				if (!(dspattr & DSPATTR_DISPDOWN)) memcpy(work + len, "</rptchar>", 10);
				else memcpy(work + len, "</rptdown>", 10);
				len += 10;
			}
#ifdef _DEBUG
			work[len] = '\0';
#endif
			ptr = work;
			break;
		case (VID_DISP_SYM >> 16):
			len = repeatcnt + 1;
			repeatcnt = 0;
			chr = (UCHAR) *cmd;
			if (!(dspattr & (DSPATTR_RAWMODE | DSPATTR_AUXPORT))) {
				if (!(dspattr & DSPATTR_DISPDOWN)) {
					if (!(dspattr & DSPATTR_DISPLEFT)) {
						i2 = wscrgt - vidh + 1;
						i3 = 1;
					}
					else {
						i2 = vidh - wsclft + 1;
						i3 = -1;
					}
				}
				else if (!(dspattr & DSPATTR_DISPLEFT)) {
					i2 = wscbot - vidv + 1;
					i3 = maxcolumns;
				}
				else {
					i2 = vidv - wsctop + 1;
					i3 = -maxcolumns;
				}
				if (len > i2) len = i2;
				if (flags & CLIENTVIDPUT_SAVE) {
					i4 = (dspattr & (DSPATTR_ATTRIBMASK | DSPATTR_FGCOLORMASK | DSPATTR_BGCOLORMASK)) | DSPATTR_GRAPHIC | (((viddblflag << 4) | chr) + '?');
					for (i1 = 0, buf = *clientbuffer + vidh + vidv * maxcolumns; i1 < len; i1++, buf += i3)
						*buf = i4;
				}
				i1 = len;
				if (i1 == i2) i1--;
				if (!(dspattr & DSPATTR_DISPDOWN)) {
					if (!(dspattr & DSPATTR_DISPLEFT)) vidh += i1;
					else vidh -= i1;
				}
				else if (!(dspattr & DSPATTR_DISPLEFT)) vidv += i1;
				else vidv -= i1;
			}
			i1 = 6;
			switch(chr) {
			case VID_SYM_HLN:
				ptr = "<hln/>";
				break;
			case VID_SYM_VLN:
				ptr = "<vln/>";
				break;
			case VID_SYM_CRS:
				ptr = "<crs/>";
				break;
			case VID_SYM_ULC:
				ptr = "<ulc/>";
				break;
			case VID_SYM_URC:
				ptr = "<urc/>";
				break;
			case VID_SYM_LLC:
				ptr = "<llc/>";
				break;
			case VID_SYM_LRC:
				ptr = "<lrc/>";
				break;
			case VID_SYM_RTK:
				ptr = "<rtk/>";
				break;
			case VID_SYM_DTK:
				ptr = "<dtk/>";
				break;
			case VID_SYM_LTK:
				ptr = "<ltk/>";
				break;
			case VID_SYM_UTK:
				ptr = "<utk/>";
				break;
			case VID_SYM_UPA:
				ptr = "<upa/>";
				break;
			case VID_SYM_RTA:
				ptr = "<rta/>";
				break;
			case VID_SYM_DNA:
				ptr = "<dna/>";
				break;
			case VID_SYM_LFA:
				ptr = "<lfa/>";
				break;
			default:  /* should not happen */
				ptr = " ";
				i1 = 1;
				break;
			}
			if (!(dspattr & DSPATTR_DISPDOWN) && (len * i1) < 30) {
				if (i1 == 1) memset(work, (UCHAR) *ptr, len);
				else {
					len *= i1;
					for (i2 = 0; i2 < len; i2 += i1) memcpy(work + i2, ptr, i1);
				}
			}
			else {
				if (!(dspattr & DSPATTR_DISPDOWN)) memcpy(work, "<rptchar n=", 11);
				else memcpy(work, "<rptdown n=", 11);
				len = mscitoa(len, work + 11) + 11;
				work[len++] = '>';
				memcpy(work + len, ptr, i1);
				len += i1;
				if (!(dspattr & DSPATTR_DISPDOWN)) memcpy(work + len, "</rptchar>", 10);
				else memcpy(work + len, "</rptdown>", 10);
				len += 10;
			}
#ifdef _DEBUG
			work[len] = '\0';
#endif
			ptr = work;
			break;
		case (VID_DSP_BLANKS >> 16):
			len = *cmd & 0xFFFF;
			if (len) {
				if (!(dspattr & (DSPATTR_RAWMODE | DSPATTR_AUXPORT))) {
					if (!(dspattr & DSPATTR_DISPDOWN)) {
						if (!(dspattr & DSPATTR_DISPLEFT)) {
							i2 = wscrgt - vidh + 1;
							i3 = 1;
						}
						else {
							i2 = vidh - wsclft + 1;
							i3 = -1;
						}
					}
					else if (!(dspattr & DSPATTR_DISPLEFT)) {
						i2 = wscbot - vidv + 1;
						i3 = maxcolumns;
					}
					else {
						i2 = vidv - wsctop + 1;
						i3 = -maxcolumns;
					}
					if (len > i2) len = i2;
					if (flags & CLIENTVIDPUT_SAVE) {
						i4 = (dspattr & (DSPATTR_ATTRIBMASK | DSPATTR_FGCOLORMASK | DSPATTR_BGCOLORMASK)) | ' ';
						for (i1 = 0, buf = *clientbuffer + vidh + vidv * maxcolumns; i1 < len; i1++, buf += i3)
							*buf = i4;
					}
					i1 = len;
					if (i1 == i2) i1--;
					if (!(dspattr & DSPATTR_DISPDOWN)) {
						if (!(dspattr & DSPATTR_DISPLEFT)) vidh += i1;
						else vidh -= i1;
					}
					else if (!(dspattr & DSPATTR_DISPLEFT)) vidv += i1;
					else vidv -= i1;
				}
				if (!(dspattr & DSPATTR_DISPDOWN) && len < 30) memset(work, ' ', len);
				else {
					if (!(dspattr & DSPATTR_DISPDOWN)) memcpy(work, "<rptchar n=", 11);
					else memcpy(work, "<rptdown n=", 11);
					len = mscitoa(len, work + 11) + 11;
					work[len++] = '>';
					work[len++] = ' ';
					if (!(dspattr & DSPATTR_DISPDOWN)) memcpy(work + len, "</rptchar>", 10);
					else memcpy(work + len, "</rptdown>", 10);
					len += 10;
				}
#ifdef _DEBUG
				work[len] = '\0';
#endif
			}
			break;
		case (VID_HORZ >> 16):
			i1 = *cmd & 0xFFFF;
			if (wsclft + i1 <= wscrgt) {
				vidh = wsclft + i1;
				if ((*(cmd + 1) >> 16) == (VID_VERT >> 16)) {
					memcpy(work, "<p h=", 5);
					mscitoa(i1, work + 5);
					len = (INT) strlen(work + 5) + 5;
					i1 = *++cmd & 0xFFFF;
					if (wsctop + i1 <= wscbot) {
						vidv = wsctop + i1;
						memcpy(work + len, " v=", 3);
						len += 3;
						mscitoa(i1, work + len);
						len += (INT) strlen(work + len);
					}
					work[len++] = '/';
					work[len++] = '>';
				}
				else {
					memcpy(work, "<h>", 3);
					mscitoa(i1, work + 3);
					len = (INT) strlen(work + 3) + 3;
					memcpy(work + len, "</h>", 4);
					len += 4;
				}
#ifdef _DEBUG
				work[len] = '\0';
#endif
			}
			break;
		case (VID_VERT >> 16):
			i1 = *cmd & 0xFFFF;
			if (wsctop + i1 <= wscbot) {
				vidv = wsctop + i1;
				memcpy(work, "<v>", 3);
				mscitoa(i1, work + 3);
				len = (INT) strlen(work + 3) + 3;
				memcpy(work + len, "</v>", 4);
				len += 4;
#ifdef _DEBUG
				work[len] = '\0';
#endif
			}
			break;
		case (VID_HORZ_ADJ >> 16):
			i1 = (SHORT)(*cmd & 0xFFFF);
			if (vidh + i1 >= wsclft && vidh + i1 <= wscrgt) {
				vidh += i1;
				memcpy(work, "<ha>", 4);
				mscitoa(i1, work + 4);
				len = (INT) strlen(work + 4) + 4;
				memcpy(work + len, "</ha>", 5);
				len += 5;
#ifdef _DEBUG
				work[len] = '\0';
#endif
			}
			break;
		case (VID_VERT_ADJ >> 16):
			i1 = (SHORT)(*cmd & 0xFFFF);
			if (vidv + i1 >= wsctop && vidv + i1 <= wscbot) {
				vidv += i1;
				memcpy(work, "<va>", 4);
				mscitoa(i1, work + 4);
				len = (INT) strlen(work + 4) + 4;
				memcpy(work + len, "</va>", 5);
				len += 5;
#ifdef _DEBUG
				work[len] = '\0';
#endif
			}
			break;
		case (VID_WIN_TOP >> 16):
			i1 = (*cmd & 0xFFFF);
			if (i1 >= 0 && i1 < maxlines) {
				wsctop = i1;
				if (wsctop > wscbot) wscbot = wsctop;
				memcpy(work, "<setsw t=", 9);
				mscitoa(i1, work + 9);
				len = (INT) strlen(work + 9) + 9;
				if ((*(cmd + 1) >> 16) == (VID_WIN_BOTTOM >> 16)) {
					i1 = *++cmd & 0xFFFF;
					if (i1 >= 0 && i1 < maxlines) {
						wscbot = i1;
						if (wscbot < wsctop) wsctop = wscbot;
						memcpy(work + len, " b=", 3);
						len += 3;
						mscitoa(i1, work + len);
						len += (INT) strlen(work + len);
						if ((*(cmd + 1) >> 16) == (VID_WIN_LEFT >> 16)) {
							i1 = *++cmd & 0xFFFF;
							if (i1 >= 0 && i1 < maxcolumns) {
								wsclft = i1;
								if (wsclft > wscrgt) wscrgt = wsclft;
								memcpy(work + len, " l=", 3);
								len += 3;
								mscitoa(i1, work + len);
								len += (INT) strlen(work + len);
								if ((*(cmd + 1) >> 16) == (VID_WIN_RIGHT >> 16)) {
									i1 = *++cmd & 0xFFFF;
									if (i1 >= 0 && i1 < maxcolumns) {
										wscrgt = i1;
										if (wscrgt < wsclft) wsclft = wscrgt;
										memcpy(work + len, " r=", 3);
										len += 3;
										mscitoa(i1, work + len);
										len += (INT) strlen(work + len);
									}
								}
							}
						}
					}
				}
				work[len++] = '/';
				work[len++] = '>';
#ifdef _DEBUG
				work[len] = '\0';
#endif
			}
			break;
		case (VID_WIN_BOTTOM >> 16):
			i1 = (*cmd & 0xFFFF);
			if (i1 >= 0 && i1 < maxlines) {
				wscbot = i1;
				if (wscbot < wsctop) wsctop = wscbot;
				memcpy(work, "<setsw b=", 9);
				mscitoa(i1, work + 9);
				len = (INT) strlen(work + 9) + 9;
				work[len++] = '/';
				work[len++] = '>';
#ifdef _DEBUG
				work[len] = '\0';
#endif
			}
			break;
		case (VID_WIN_LEFT >> 16):
			i1 = (*cmd & 0xFFFF);
			if (i1 >= 0 && i1 < maxcolumns) {
				wsclft = i1;
				if (wsclft > wscrgt) wscrgt = wsclft;
				memcpy(work, "<setsw l=", 9);
				mscitoa(i1, work + 9);
				len = (INT) strlen(work + 9) + 9;
				if ((*(cmd + 1) >> 16) == (VID_WIN_RIGHT >> 16)) {
					i1 = *++cmd & 0xFFFF;
					if (i1 >= 0 && i1 < maxcolumns) {
						wscrgt = i1;
						if (wscrgt < wsclft) wsclft = wscrgt;
						memcpy(work + len, " r=", 3);
						len += 3;
						mscitoa(i1, work + len);
						len += (INT) strlen(work + len);
					}
				}
				work[len++] = '/';
				work[len++] = '>';
#ifdef _DEBUG
				work[len] = '\0';
#endif
			}
			break;
		case (VID_WIN_RIGHT >> 16):
			i1 = (*cmd & 0xFFFF);
			if (i1 >= 0 && i1 < maxcolumns) {
				wscrgt = i1;
				if (wscrgt < wsclft) wsclft = wscrgt;
				memcpy(work, "<setsw r=", 9);
				mscitoa(i1, work + 9);
				len = (INT) strlen(work + 9) + 9;
				work[len++] = '/';
				work[len++] = '>';
#ifdef _DEBUG
				work[len] = '\0';
#endif
			}
			break;
		case (VID_WIN_RESET >> 16):
			wsclft = wsctop = 0;
			wscbot = maxlines - 1;
			wscrgt = maxcolumns - 1;
			ptr = "<resetsw/>";
			len = 10;
			break;
		case (VID_REV_ON >> 16):
			if (!(dspattr & DSPATTR_REV)) {
				dspattr |= DSPATTR_REV;
				ptr = "<revon/>";
				len = 8;
			}
			break;
		case (VID_REV_OFF >> 16):
			if (dspattr & DSPATTR_REV) {
				dspattr &= ~DSPATTR_REV;
				ptr = "<revoff/>";
				len = 9;
			}
			break;
		case (VID_UL_ON >> 16):
			if (!(dspattr & DSPATTR_UNDERLINE)) {
				dspattr |= DSPATTR_UNDERLINE;
				ptr = "<ulon/>";
				len = 7;
			}
			break;
		case (VID_UL_OFF >> 16):
			if (dspattr & DSPATTR_UNDERLINE) {
				dspattr &= ~DSPATTR_UNDERLINE;
				ptr = "<uloff/>";
				len = 8;
			}
			break;
		case (VID_BLINK_ON >> 16):
			if (!(dspattr & DSPATTR_BLINK)) {
				dspattr |= DSPATTR_BLINK;
				ptr = "<blinkon/>";
				len = 10;
			}
			break;
		case (VID_BLINK_OFF >> 16):
			if (dspattr & DSPATTR_BLINK) {
				dspattr &= ~DSPATTR_BLINK;
				ptr = "<blinkoff/>";
				len = 11;
			}
			break;
		case (VID_BOLD_ON >> 16):
			if (!(dspattr & DSPATTR_BOLD)) {
				dspattr |= DSPATTR_BOLD;
				ptr = "<boldon/>";
				len = 9;
			}
			break;
		case (VID_BOLD_OFF >> 16):
			if (dspattr & DSPATTR_BOLD) {
				dspattr &= ~DSPATTR_BOLD;
				ptr = "<boldoff/>";
				len = 10;
			}
			break;
		case (VID_DIM_ON >> 16):
			if (!(dspattr & DSPATTR_DIM)) {
				dspattr |= DSPATTR_DIM;
/*** CODE: NOT DOCUMENTED AS BEING SUPPORTED ***/
			}
			break;
		case (VID_DIM_OFF >> 16):
			if (dspattr & DSPATTR_DIM) {
				dspattr &= ~DSPATTR_DIM;
/*** CODE: NOT DOCUMENTED AS BEING SUPPORTED ***/
			}
			break;
		case (VID_RL_ON >> 16):
			if (!(dspattr & DSPATTR_DISPLEFT)) {
				dspattr |= DSPATTR_DISPLEFT;
/*** CODE: NOT DOCUMENTED AS BEING SUPPORTED ***/
			}
			break;
		case (VID_RL_OFF >> 16):
			if (dspattr & DSPATTR_DISPLEFT) {
				dspattr &= ~DSPATTR_DISPLEFT;
/*** CODE: NOT DOCUMENTED AS BEING SUPPORTED ***/
			}
			break;
		case (VID_DSPDOWN_ON >> 16):
			dspattr |= DSPATTR_DISPDOWN;
			break;
		case (VID_DSPDOWN_OFF >> 16):
			dspattr &= ~DSPATTR_DISPDOWN;
			if (scrollend != NULL) {
				ptr = scrollend;
				len = (INT) strlen(ptr);
				scrollend = NULL;
			}
			break;
		case (VID_NL_ROLL_ON >> 16):
			if (!(dspattr & DSPATTR_ROLL)) {
				dspattr |= DSPATTR_ROLL;
				ptr = "<autorollon/>";
				len = 13;
			}
			break;
		case (VID_NL_ROLL_OFF >> 16):
			if (dspattr & DSPATTR_ROLL) {
				dspattr &= ~DSPATTR_ROLL;
				ptr = "<autorolloff/>";
				len = 14;
			}
			break;
		case (VID_RAW_ON >> 16):
			if (!(dspattr & DSPATTR_RAWMODE)) {
				dspattr |= DSPATTR_RAWMODE;
				ptr = "<rawon/>";
				len = 8;
			}
			break;
		case (VID_RAW_OFF >> 16):
			if (dspattr & DSPATTR_RAWMODE) {
				dspattr &= ~DSPATTR_RAWMODE;
				ptr = "<rawoff/>";
				len = 9;
			}
			break;
		case (VID_PRT_ON >> 16):
			if (!(dspattr & DSPATTR_AUXPORT)) {
				dspattr |= DSPATTR_AUXPORT;
#if 0
/*** CODE: HAVE XML AND CLIENT SUPPORT THIS ***/
				ptr = "<auxon/>";
				len = 8;
#endif
			}
			break;
		case (VID_PRT_OFF >> 16):
			if (dspattr & DSPATTR_AUXPORT) {
				dspattr &= ~DSPATTR_AUXPORT;
#if 0
/*** CODE: HAVE XML AND CLIENT SUPPORT THIS ***/
				ptr = "<auxoff/>";
				len = 9;
#endif
			}
			break;
		case (VID_CURSOR_ON >> 16):
			if (vidcurmode != CURSMODE_ON) {
				vidcurmode = CURSMODE_ON;
				ptr = "<cursor>on</cursor>";
				len = 19;
			}
			break;
		case (VID_CURSOR_OFF >> 16):
			if (vidcurmode != CURSMODE_OFF) {
				vidcurmode = CURSMODE_OFF;
				ptr = "<cursor>off</cursor>";
				len = 20;
			}
			break;
		case (VID_CURSOR_NORM >> 16):
			if (vidcurmode != CURSMODE_NORMAL) {
				vidcurmode = CURSMODE_NORMAL;
				ptr = "<cursor>norm</cursor>";
				len = 21;
			}
			break;
		case (VID_CURSOR_ULINE >> 16):
			if (vidcurtype != CURSOR_ULINE) {
				vidcurtype = CURSOR_ULINE;
				ptr = "<cursor>uline</cursor>";
				len = 22;
			}
			break;
		case (VID_CURSOR_HALF >> 16):
			if (vidcurtype != CURSOR_HALF) {
				vidcurtype = CURSOR_HALF;
				ptr = "<cursor>half</cursor>";
				len = 21;
			}
			break;
		case (VID_CURSOR_BLOCK >> 16):
			if (vidcurtype != CURSOR_BLOCK) {
				vidcurtype = CURSOR_BLOCK;
				ptr = "<cursor>block</cursor>";
				len = 22;
			}
			break;
		case (VID_HDBL_ON >> 16):
			if (!(viddblflag & 0x01)) {
				viddblflag |= 0x01;
				ptr = "<hdblon/>";
				len = 9;
			}
			break;
		case (VID_HDBL_OFF >> 16):
			if (viddblflag & 0x01) {
				viddblflag &= ~0x01;
				ptr = "<hdbloff/>";
				len = 10;
			}
			break;
		case (VID_VDBL_ON >> 16):
			if (!(viddblflag & 0x02)) {
				viddblflag |= 0x02;
				ptr = "<vdblon/>";
				len = 9;
			}
			break;
		case (VID_VDBL_OFF >> 16):
			if (viddblflag & 0x02) {
				viddblflag &= ~0x02;
				ptr = "<vdbloff/>";
				len = 10;
			}
			break;
		case (VID_KBD_RESET >> 16):
			kynattr &= ~(KYNATTR_JUSTRIGHT | KYNATTR_JUSTLEFT | KYNATTR_ZEROFILL);
			/* kynattr_digitentry & kynattr_edit are never set */
			break;
		case (VID_KBD_DE >> 16):
			/* not required */
			break;
		case (VID_KBD_JR >> 16):
			kynattr &= ~KYNATTR_JUSTLEFT;
			kynattr |= KYNATTR_JUSTRIGHT;
			break;
		case (VID_KBD_JL >> 16):
			kynattr &= ~KYNATTR_JUSTRIGHT;
			kynattr |= KYNATTR_JUSTLEFT;
			break;
		case (VID_KBD_ZF >> 16):
			kynattr |= KYNATTR_ZEROFILL;
			break;
		case (VID_KBD_EDIT >> 16):
			/* not required */
			break;
		case (VID_KBD_UC >> 16):
			if (!(kynattr & KYNATTR_KEYCASEUPPER)) {
				kynattr &= ~KYNATTR_KEYCASEMASK;
				kynattr |= KYNATTR_KEYCASEUPPER;
				ptr = "<uc/>";
				len = 5;
			}
			break;
		case (VID_KBD_LC >> 16):
			if (!(kynattr & KYNATTR_KEYCASELOWER)) {
				kynattr &= ~KYNATTR_KEYCASEMASK;
				kynattr |= KYNATTR_KEYCASELOWER;
				ptr = "<lc/>";
				len = 5;
			}
			break;
		case (VID_KBD_IC >> 16):
			if (!(kynattr & KYNATTR_KEYCASEINVERT)) {
				kynattr &= ~KYNATTR_KEYCASEMASK;
				kynattr |= KYNATTR_KEYCASEINVERT;
				ptr = "<it/>";
				len = 5;
			}
			break;
		case (VID_KBD_IN >> 16):
			if (!(kynattr & KYNATTR_KEYCASENORMAL)) {
				kynattr &= ~KYNATTR_KEYCASEMASK;
				kynattr |= KYNATTR_KEYCASENORMAL;
				ptr = "<in/>";
				len = 5;
			}
			break;
		case (VID_KBD_ESON >> 16):
			if (!(kynattr & KYNATTR_ECHOSECRET)) {
				kynattr |= KYNATTR_ECHOSECRET;
				ptr = "<eson/>";
				len = 7;
			}
			break;
		case (VID_KBD_ESOFF >> 16):
			if (kynattr & KYNATTR_ECHOSECRET) {
				kynattr &= ~KYNATTR_ECHOSECRET;
				ptr = "<esoff/>";
				len = 8;
			}
			break;
		case (VID_KBD_EON >> 16):
			if (kynattr & KYNATTR_NOECHO) {
				kynattr &= ~KYNATTR_NOECHO;
				ptr = "<eon/>";
				len = 6;
			}
			break;
		case (VID_KBD_EOFF >> 16):
			if (!(kynattr & KYNATTR_NOECHO)) {
				kynattr |= KYNATTR_NOECHO;
				ptr = "<eoff/>";
				len = 7;
			}
			break;
		case (VID_KBD_KCON >> 16):
			if (!(kynattr & KYNATTR_AUTOENTER)) {
				kynattr |= KYNATTR_AUTOENTER;
				ptr = "<kcon/>";
				len = 7;
			}
			break;
		case (VID_KBD_KCOFF >> 16):
			if (kynattr & KYNATTR_AUTOENTER) {
				kynattr &= ~KYNATTR_AUTOENTER;
				ptr = "<kcoff/>";
				len = 8;
			}
			break;
		case (VID_KBD_ED_ON >> 16):
			if (!(kynattr & KYNATTR_EDITON)) {
				kynattr |= KYNATTR_EDITON;
				ptr = "<editon/>";
				len = 9;
			}
			break;
		case (VID_KBD_ED_OFF >> 16):
			if (kynattr & KYNATTR_EDITON) {
				kynattr &= ~KYNATTR_EDITON;
				ptr = "<editoff/>";
				len = 10;
			}
			break;
		case (VID_KBD_CK_ON >> 16):
			if (!(kynattr & KYNATTR_CLICK)) {
				kynattr |= KYNATTR_CLICK;
				ptr = "<clickon/>";
				len = 10;
			}
			break;
		case (VID_KBD_CK_OFF >> 16):
			if (kynattr & KYNATTR_CLICK) {
				kynattr &= ~KYNATTR_CLICK;
				ptr = "<clickoff/>";
				len = 11;
			}
			break;
		case (VID_KBD_OVS_ON >> 16):
			if (!(kynattr & KYNATTR_EDITOVERSTRIKE)) {
				kynattr |= KYNATTR_EDITOVERSTRIKE;
				ptr = "<ovsmode/>";
				len = 10;
			}
			break;
		case (VID_KBD_OVS_OFF >> 16):
			if (kynattr & KYNATTR_EDITOVERSTRIKE) {
				kynattr &= ~KYNATTR_EDITOVERSTRIKE;
				ptr = "<insmode/>";
				len = 10;
			}
			break;
		case (VID_KBD_ECHOCHR >> 16):
			if (keyinechochar != (UCHAR) *cmd) {
				keyinechochar = (UCHAR) *cmd;
				memcpy(work, "<eschar>", 8);
				work[8] = (CHAR) keyinechochar;
				memcpy(work + 9, "</eschar>", 9);
				len = 18;
#ifdef _DEBUG
				work[len] = '\0';
#endif
			}
			break;
		case (VID_KBD_TIMEOUT >> 16):
			i1 = (*cmd & 0xFFFF);
			memcpy(work, "<timeout n=", 11);
			mscitoa(i1, work + 11);
			len = (INT) strlen(work + 11) + 11;
			work[len++] = '/';
			work[len++] = '>';
#ifdef _DEBUG
			work[len] = '\0';
#endif
			break;
		case (VID_KBD_F_CNCL1 >> 16):
/*** CODE: NOT CURRENTLY CHANGEABLE AFTER INITIALIZATION ***/
			break;
		case (VID_KBD_F_CNCL2 >> 16):
/*** CODE: NOT CURRENTLY CHANGEABLE AFTER INITIALIZATION ***/
			break;
		case (VID_KBD_F_INTR >> 16):
/*** CODE: NOT CURRENTLY CHANGEABLE AFTER INITIALIZATION ***/
			break;
		case (VID_ES >> 16):
			vidv = wsctop;
			vidh = wsclft;
			if (flags & CLIENTVIDPUT_SAVE) buferaserect(wsctop, wscbot, wsclft, wscrgt, dspattr);
			ptr = "<es/>";
			len = 5;
			break;
		case (VID_EF >> 16):
			if (flags & CLIENTVIDPUT_SAVE) {
				if (vidh == wsclft) buferaserect(vidv, wscbot, wsclft, wscrgt, dspattr);
				else {
					buferaserect(vidv, vidv, vidh, wscrgt, dspattr);
					if (vidv < wscbot) buferaserect(vidv + 1, wscbot, wsclft, wscrgt, dspattr);
				}
			}
			ptr = "<ef/>";
			len = 5;
			break;
		case (VID_EL >> 16):
			if (flags & CLIENTVIDPUT_SAVE) buferaserect(vidv, vidv, vidh, wscrgt, dspattr);
			ptr = "<el/>";
			len = 5;
			break;
		case (VID_RU >> 16):
			if (flags & CLIENTVIDPUT_SAVE) bufmove(ROLL_UP, wsctop, wscbot, wsclft, wscrgt, dspattr);
			ptr = "<ru/>";
			len = 5;
			break;
		case (VID_RD >> 16):
			if (flags & CLIENTVIDPUT_SAVE) bufmove(ROLL_DOWN, wsctop, wscbot, wsclft, wscrgt, dspattr);
			ptr = "<rd/>";
			len = 5;
			break;
		case (VID_RL >> 16):
			if (flags & CLIENTVIDPUT_SAVE) bufmove(ROLL_LEFT, wsctop, wscbot, wsclft, wscrgt, dspattr);
			ptr = "<scrleft>";
			len = 9;
			scrollend = "</scrleft>";
			break;
		case (VID_RR >> 16):
			if (flags & CLIENTVIDPUT_SAVE) bufmove(ROLL_RIGHT, wsctop, wscbot, wsclft, wscrgt, dspattr);
			ptr = "<scrright>";
			len = 10;
			scrollend = "</scrright>";
			break;
		case (VID_CR >> 16):
			if (!(dspattr & DSPATTR_AUXPORT)) vidh = wsclft;
			ptr = "<cr/>";
			len = 5;
			break;
		case (VID_NL >> 16):
			if (!(dspattr & DSPATTR_AUXPORT)) {
				vidh = wsclft;
				if (vidv != wscbot) vidv++;
				else if ((dspattr & DSPATTR_ROLL) && (flags & CLIENTVIDPUT_SAVE)) bufmove(ROLL_UP, wsctop, wscbot, wsclft, wscrgt, dspattr);
			}
			ptr = "<nl/>";
			len = 5;
			break;
		case (VID_LF >> 16):
			if (!(dspattr & DSPATTR_AUXPORT)) {
				if (vidv != wscbot) vidv++;
				else if ((dspattr & DSPATTR_ROLL) && (flags & CLIENTVIDPUT_SAVE)) bufmove(ROLL_UP, wsctop, wscbot, wsclft, wscrgt, dspattr);
			}
			ptr = "<lf/>";
			len = 5;
			break;
		case (VID_HU >> 16): /* home up */
			vidv = wsctop;
			vidh = wsclft;
			if (scrollend == NULL) {
				ptr = "<hu/>";
				len = 5;
			}
			break;
		case (VID_HD >> 16): /* home down */
			vidv = wscbot;
			vidh = wsclft;
			ptr = "<hd/>";
			len = 5;
			break;
		case (VID_EU >> 16): /* end up */
			vidv = wsctop;
			vidh = wscrgt;
			if (scrollend == NULL) {
				ptr = "<eu/>";
				len = 5;
			}
			break;
		case (VID_ED >> 16): /* end down */
			vidv = wscbot;
			vidh = wscrgt;
			ptr = "<ed/>";
			len = 5;
			break;
		case (VID_OL >> 16):
			if (flags & CLIENTVIDPUT_SAVE) {
				bufmove(ROLL_DOWN, vidv + 1, wscbot, wsclft, wscrgt, dspattr);
				bufmove(ROLL_DOWN, vidv, vidv + 1, vidh, wscrgt, dspattr);
			}
			ptr = "<opnlin/>";
			len = 9;
			break;
		case (VID_CL >> 16):
			if (flags & CLIENTVIDPUT_SAVE) {
				bufmove(ROLL_UP, vidv, vidv + 1, vidh, wscrgt, dspattr);
				bufmove(ROLL_UP, vidv + 1, wscbot, wsclft, wscrgt, dspattr);
			}
			ptr = "<clslin/>";
			len = 9;
			break;
		case (VID_IL >> 16):
			if (flags & CLIENTVIDPUT_SAVE) bufmove(ROLL_DOWN, vidv, wscbot, wsclft, wscrgt, dspattr);
			ptr = "<inslin/>";
			len = 9;
			break;
		case (VID_DL >> 16):
			if (flags & CLIENTVIDPUT_SAVE) bufmove(ROLL_UP, vidv, wscbot, wsclft, wscrgt, dspattr);
			ptr = "<dellin/>";
			len = 9;
			break;
		case (VID_INSCHAR >> 16):
			i1 = wsclft + ((*++cmd >> 16) & 0xFFFF);
			i2 = wsctop + (*cmd & 0xFFFF);
			if (i2 >= vidv && (i2 > vidv || i1 > vidh) && i2 <= wscbot && i1 <= wscrgt) {
				if (flags & CLIENTVIDPUT_SAVE) bufinsertchar(i1, i2, wsctop, wscbot, wsclft, wscrgt, dspattr + ' ');
				memcpy(work, "<inschr h=", 10);
				mscitoa(i1 - wsclft, work + 10);
				len = (INT) strlen(work + 10) + 10;
				memcpy(work + len, " v=", 3);
				len += 3;
				mscitoa(i2 - wsctop, work + len);
				len += (INT) strlen(work + len);
				work[len++] = '/';
				work[len++] = '>';
#ifdef _DEBUG
				work[len] = '\0';
#endif
			}
			break;
		case (VID_DELCHAR >> 16):
			i1 = wsclft + ((*++cmd >> 16) & 0xFFFF);
			i2 = wsctop + (*cmd & 0xFFFF);
			if (i2 >= vidv && (i2 > vidv || i1 > vidh) && i2 <= wscbot && i1 <= wscrgt) {
				if (flags & CLIENTVIDPUT_SAVE) bufdeletechar(i1, i2, wsctop, wscbot, wsclft, wscrgt, dspattr + ' ');
				memcpy(work, "<delchr h=", 10);
				mscitoa(i1 - wsclft, work + 10);
				len = (INT) strlen(work + 10) + 10;
				memcpy(work + len, " v=", 3);
				len += 3;
				mscitoa(i2 - wsctop, work + len);
				len += (INT) strlen(work + len);
				work[len++] = '/';
				work[len++] = '>';
#ifdef _DEBUG
				work[len] = '\0';
#endif
			}
			break;
		case (VID_BEEP >> 16):
			ptr = "<b/>";
			len = 4;
			break;
		case (VID_SOUND >> 16):
			cmd++;
			if (*cmd == 0x70000001) {
				ptr = "<click/>";
				len = 8;
			}
/*** CODE: ELSE SUPPORT SOUND ***/
			break;
		case (VID_FOREGROUND >> 16):
			len = vidputColor(*cmd, ptr, VID_FOREGROUND);
			break;
		case (VID_BACKGROUND >> 16):
			len = vidputColor(*cmd, ptr, VID_BACKGROUND);
			break;
		default:
			return RC_ERROR;
		}
		if (len && (flags & CLIENTVIDPUT_SEND)) {
			if (!dataflag) clientput(ptr, len);
			else clientputdata(ptr, len);
		}
		if (repeatcnt) {
			repeatcnt--;
			cmd = repeatcmd;
		}
		else cmd++;
	}
}

/**
 * cmd contains the color number in the lower 8 bits
 * ptr is where we put the XML to send
 * vidCode must be VID_FOREGROUND or VID_BACKGROUND
 *
 * Return the number of characters placed in ptr
 */
static int vidputColor(INT32 cmd, CHAR* ptr, INT vidCode) {
	static CHAR *nonAnsiFG[] =
	{
		"<black/>", "<blue/>", "<green/>", "<cyan/>", "<red/>", "<magenta/>", "<yellow/>", "<white/>"
	};
	static CHAR *nonAnsiBG[] =
	{
		"<bgblack/>", "<bgblue/>", "<bggreen/>", "<bgcyan/>", "<bgred/>", "<bgmagenta/>", "<bgyellow/>", "<bgwhite/>"
	};
	static CHAR *ansiFG[] =
	{
		"<black/>", "<red/>", "<green/>", "<yellow/>", "<blue/>", "<magenta/>", "<cyan/>", "<white/>"
	};
	static CHAR *ansiBG[] =
	{
		"<bgblack/>", "<bgred/>", "<bggreen/>", "<bgyellow/>", "<bgblue/>", "<bgmagenta/>", "<bgcyan/>", "<bgwhite/>"
	};
	UINT64 i64 = cmd & 0x00FF;

	if (vidCode == VID_FOREGROUND) {
		if (((i64 << DSPATTR_FGCOLORSHIFT) ^ dspattr) & DSPATTR_FGCOLORMASK) {
			dspattr &= ~DSPATTR_FGCOLORMASK;
			dspattr |= (i64 << DSPATTR_FGCOLORSHIFT) & DSPATTR_FGCOLORMASK;
		}
	}
	else if (vidCode == VID_BACKGROUND) {
		if (((i64 << DSPATTR_BGCOLORSHIFT) ^ dspattr) & DSPATTR_BGCOLORMASK) {
			dspattr &= ~DSPATTR_BGCOLORMASK;
			dspattr |= (i64 << DSPATTR_BGCOLORSHIFT) & DSPATTR_BGCOLORMASK;
		}
	}

	if (vidCode == VID_FOREGROUND) {
		if (i64 <= 7) {
			if (vidflags & VID_FLAG_COLORMODE_OLD) strcpy(ptr, nonAnsiFG[i64]);
			else strcpy(ptr, ansiFG[i64]);
			return (INT) strlen(ptr);
		}
		else return sprintf(ptr, "<color v=%d/>", (INT)i64);
	}
	else if (vidCode == VID_BACKGROUND) {
		if (i64 <= 7) {
			if (vidflags & VID_FLAG_COLORMODE_OLD) strcpy(ptr, nonAnsiBG[i64]);
			else strcpy(ptr, ansiBG[i64]);
			return (INT) strlen(ptr);
		}
		else return sprintf(ptr, "<bgcolor v=%d/>", (INT)i64);
	}
	return 0;
}

void clientvidreset()
{
	static INT32 vidcmd[] = {
		VID_REV_OFF,
		VID_UL_OFF,
		VID_BLINK_OFF,
		VID_BOLD_OFF,
		VID_DIM_OFF,
		VID_RAW_OFF,
		VID_PRT_OFF,
		VID_CURSOR_NORM,
		VID_CURSOR_BLOCK,
		VID_HDBL_OFF,
		VID_VDBL_OFF,
		VID_KBD_CK_OFF,
		VID_KBD_OVS_OFF,
		VID_KBD_ECHOCHR | '*',
		VID_FINISH
	};

	/* assumes caller will add prefix/postfix and call clientsend */
	clientvidput(vidcmd, CLIENTVIDPUT_SAVE | CLIENTVIDPUT_SEND);
}

void clientcancelputs()
{
	clientflags &= ~CLIENT_FLAG_SENDNOMEM;
	sendbufcnt = SEND_HEADER;
}

int clientsend(INT flags, INT eventid)
{
	int i1, cnt, events[2];
	fd_set wrtset, errset;

	if (clientflags & CLIENT_FLAG_SHUTDOWN) {
		if (logfile != NULL) {
#if OS_WIN32
			pvistart();
#endif
			fputs("NOS: ", logfile);
			memcpy(sendbuffer + sendbufcnt, "\n\0", 2);
			fputs(sendbuffer, logfile);
			fflush(logfile);
#if OS_WIN32
			pviend();
#endif
		}
		sendbufcnt = SEND_HEADER;
		if (flags & CLIENTSEND_RELEASE) clientrelease();
		return 0;
	}
	if (clientflags & CLIENT_FLAG_SENDNOMEM) {
		if (logfile != NULL) {
#if OS_WIN32
			pvistart();
#endif
			fputs("ERR: NO MEMORY\n", logfile);
			fflush(logfile);
#if OS_WIN32
			pviend();
#endif
		}
		clientflags &= ~CLIENT_FLAG_SENDNOMEM;
		return RC_ERROR;
	}
	if (sendbufcnt <= SEND_HEADER) {
		sendbufcnt = SEND_HEADER;  /* just in case */
		return 0;
	}


	if (flags & (CLIENTSEND_WAIT | CLIENTSEND_SERIAL | CLIENTSEND_EVENT)) {
#if OS_WIN32
		pvistart();
#endif
		if ((flags & CLIENTSEND_SERIAL) && ++serialnum == 99999999) serialnum = 1;
		if (flags & (CLIENTSEND_EVENT | CLIENTSEND_WAIT)) {
			if (flags & CLIENTSEND_EVENT) recveventid = eventid;
			else recveventid = workeventid;
			evtclear(recveventid);
		}
#if OS_WIN32
		pviend();
#endif
	}

	msciton(serialnum, (UCHAR *)(sendbuffer + SEND_SNUMOFF), SEND_SNUMLEN);
	msciton(sendbufcnt - SEND_HEADER, (UCHAR *)(sendbuffer + SEND_SIZEOFF), SEND_SIZELEN);
	if (logfile != NULL) {
#if OS_WIN32
		pvistart();
#endif
		fputs("SND: ", logfile);
		memcpy(sendbuffer + sendbufcnt, "\n\0", 2);
		fputs(sendbuffer, logfile);
		fflush(logfile);
#if OS_WIN32
		pviend();
#endif
	}
#ifdef NOCLIENT
	if (flags & (CLIENTSEND_EVENT | CLIENTSEND_WAIT | CLIENTSEND_CONTWAIT)) evtset(recveventid);
	sendbufcnt = SEND_HEADER;
	return 0;
#endif
	for (cnt = 0; ; ) {
		i1 = tcpsend(sockethandle, (UCHAR *) sendbuffer + cnt, sendbufcnt - cnt, tcpflags, -1);
		if (i1 == SOCKET_ERROR) {
/*** CODE: DEBUG STUFF HERE ***/
			i1 = ERRORVALUE();
			clientflags |= CLIENT_FLAG_SHUTDOWN;
			evtset(shutdowneventid);
			sendbufcnt = SEND_HEADER;
			clientrelease();
			return RC_ERROR;
		}
		cnt += i1;
		if (cnt >= sendbufcnt) break;
		FD_ZERO(&wrtset);
		FD_SET(sockethandle, &wrtset);
		FD_ZERO(&errset);
		FD_SET(sockethandle, &errset);
		if ((i1 = select((INT) (sockethandle + 1), NULL, &wrtset, &errset, NULL)) > 0 && FD_ISSET(sockethandle, &errset)) {
			clientflags |= CLIENT_FLAG_SHUTDOWN;
			evtset(shutdowneventid);
			sendbufcnt = SEND_HEADER;
			clientrelease();
			return RC_ERROR;
		}
	}
	sendbufcnt = SEND_HEADER;

	if (flags & (CLIENTSEND_WAIT | CLIENTSEND_CONTWAIT)) {
		events[0] = shutdowneventid;
		events[1] = recveventid;
		i1 = evtwait(events, 2);
		if (flags & CLIENTSEND_RELEASE) clientrelease();
		if (i1 != 1) return RC_ERROR;
	}
	return 0;
}

void clientrelease()
{
#if OS_WIN32
	pvistart();
#endif
	if (++serialnum == 99999999) serialnum = 1;
	recveventid = 0;
#if OS_WIN32
	pviend();
#endif
	if (clientflags & CLIENT_FLAG_ELEMENT) {
		clientflags &= ~CLIENT_FLAG_ELEMENT;
		processrecv();
	}
}

int clientkeyin(UCHAR *buffer, int bufsize, int *endkey, int dsplen, int flags)
{
#if OS_WIN32
	INT *buf;
#else
	UINT64 *buf;
#endif
	int i1, i2, fieldoffset, filllength, filloffset, keyinlength, len;
	char fillchr, *ptr;
	ELEMENT *element;
	ATTRIBUTE *attribute;
#ifdef NOCLIENT
	static int count;
	if (++count == 1) {
		buffer[0] = 'A';
		*endkey = 256;
		return 1;
	}
	*endkey = 256;
	return 0;
#endif

	if (clientflags & CLIENT_FLAG_SHUTDOWN) {
		if (clientflags & CLIENT_FLAG_ELEMENT) {
			evtclear(recveventid);
			clientflags &= ~CLIENT_FLAG_ELEMENT;
			processrecv();
		}
		return 0;
	}
	if (!(clientflags & CLIENT_FLAG_ELEMENT)) return RC_ERROR;
	element = elementptr->firstsubelement;
	if (element != NULL && element->cdataflag) {
		len = element->cdataflag;
		if (len > bufsize) len = bufsize;  /* should not happen */
		/**
		 * At this point, if the text child element had any special characters in it
		 * like ampersand or <, the entities ( e.g. &amp; ) have been replaced with normal
		 * characters
		 */
		ptr = element->tag;
	}
	else len = 0;

	if (len && !(kynattr & KYNATTR_NOECHO)) {  /* save to client buffer */
		if (dsplen > len) dsplen = len;
		i2 = wscrgt - vidh + 1;
		if (dsplen > i2) dsplen = i2;
		if (kynattr & KYNATTR_ECHOSECRET) ptr = (CHAR *) &keyinechochar;
		for (i1 = 0, buf = *clientbuffer + vidh + vidv * maxcolumns; i1 < dsplen; i1++, buf++) {
			*buf = (dspattr & (DSPATTR_ATTRIBMASK | DSPATTR_FGCOLORMASK | DSPATTR_BGCOLORMASK)) | (UCHAR) *ptr;
			if (!(kynattr & KYNATTR_ECHOSECRET)) ptr++;
		}
		if (dsplen == i2) dsplen--;
		vidh += dsplen;
	}

	keyinlength = len;
	fillchr = '0';
	fieldoffset = filllength = filloffset = 0;
	if (flags & CLIENTKEYIN_NUM) {
		if (len && !(flags & CLIENTKEYIN_NUMRGT) && (kynattr & KYNATTR_JUSTLEFT)) {
			keyinlength = bufsize;
			filllength = bufsize - len;
			filloffset = len;
		}
	}
	else {
		if (!(kynattr & KYNATTR_ZEROFILL)) fillchr = ' ';
		if (len || !(flags & CLIENTKEYIN_RV)) filllength = bufsize - len;
		if (len) {
			if (kynattr & (KYNATTR_JUSTRIGHT | KYNATTR_JUSTLEFT | KYNATTR_ZEROFILL)) keyinlength = bufsize;
			if (kynattr & KYNATTR_JUSTRIGHT) fieldoffset = filllength;
			else filloffset = len;
		}
	}
	if (len) memcpy(buffer + fieldoffset, element->tag, len);
	if (filllength) memset(buffer + filloffset, fillchr, filllength);

	*endkey = 0;
	attribute = elementptr->firstattribute;
	if (attribute != NULL) {
		if (!strcmp(attribute->tag, "e")) {
			i1 = atoi(attribute->value);
			if (!i1) i1 = -1;
			*endkey = i1;
		}
	}
	evtclear(recveventid);
	clientflags &= ~CLIENT_FLAG_ELEMENT;
	processrecv();
	return keyinlength;
}

int clientsetendkey(UCHAR *keymap)
{
	int i1, i2, i3, setflag;
	char chr;
	UCHAR bits;

	setflag = FALSE;
	for (i1 = 0; i1 < (VID_MAXKEYVAL >> 3) + 1; i1++) {
		bits = (finishmap[i1] ^ keymap[i1]) & keymap[i1];
		if (bits) {
			finishmap[i1] |= bits;
			if (!setflag) {
				setflag = TRUE;
				clientput("<se>", 4);
			}
			for (i2 = 0; bits; i2++, bits >>= 1) {
				if (bits & 0x01) {
					i3 = (i1 << 3) + i2;
					if (i3 > ' ' && i3 <= '~') {
						if (i3 == '<') clientput("&lt;", 4);
						else if (i3 == '>') clientput("&gt;", 4);
						else if (i3 == '&') clientput("&amp;", 5);
						else if (i3 == '"') clientput("&quot;", 6);
						else if (i3 == '\'') clientput("&apos;", 6);
						else {
							chr = (CHAR) i3;
							clientput(&chr, 1);
						}
					}
					else {
						clientput("<c>", 3);
						clientputnum(i3);
						clientput("</c>", 4);
					}
				}
			}
		}
	}
	if (setflag) {
		clientput("</se>", 5);
		if (clientsend(0, 0) < 0) return RC_ERROR;
	}
	return 0;
}

int clientclearendkey(UCHAR *keymap)
{
	int i1, i2, i3, clearflag;
	char chr;
	UCHAR bits;

	clearflag = FALSE;
	if (keymap != NULL) {
		for (i1 = 0; i1 < (VID_MAXKEYVAL >> 3) + 1; i1++) {
			bits = finishmap[i1] & keymap[i1];
			if (bits) {
				finishmap[i1] ^= bits;
				if (!clearflag) {
					clearflag = TRUE;
					clientput("<ce>", 4);
				}
				for (i2 = 0; bits; i2++, bits >>= 1) {
					if (bits & 0x01) {
						i3 = (i1 << 3) + i2;
						if (i3 > ' ' && i3 <= '~') {
							if (i3 == '<') clientput("&lt;", 4);
							else if (i3 == '>') clientput("&gt;", 4);
							else if (i3 == '&') clientput("&amp;", 5);
							else if (i3 == '"') clientput("&quot;", 6);
							else if (i3 == '\'') clientput("&apos;", 6);
							else {
								chr = (CHAR) i3;
								clientput(&chr, 1);
							}
						}
						else {
							clientput("<c>", 3);
							clientputnum(i3);
							clientput("</c>", 4);
						}
					}
				}
			}
		}
		if (clearflag) clientput("</ce>", 5);
	}
	else {
		memset(finishmap, 0, sizeof(finishmap));
		clearflag = TRUE;
		clientput("<ce><all/></ce>", 15);
	}
	if (clearflag && clientsend(0, 0) < 0) return RC_ERROR;
	return 0;
}

int clienttrapstart(UCHAR *trapmap, INT eventid)
{
	int i1, i2, i3, i4, i5, i6, clearcnt, setcnt, trapcnt;
	char chr;

#if OS_WIN32
	pvistart();
#endif
	trapeventid = eventid;
	/* determine if "clear all" should be used */
	clearcnt = setcnt = trapcnt = 0;
	for (i1 = 0; i1 < (int) sizeof(trapactivemap); i1++) {
		if (trapmap != NULL) {
			if (trapmap[i1] != trapactivemap[i1]) {
				i3 = trapmap[i1];
				i4 = trapactivemap[i1];
				for (i2 = 1; i2 < 256; i2 <<= 1) {
					if (i2 & i3) {
						if (!(i2 & i4)) setcnt++;
						trapcnt++;
					}
					else if (i2 & i4) clearcnt++;
				}
			}
			else if (trapmap[i1]) {
				i3 = trapmap[i1];
				for (i2 = 1; i2 < 256; i2 <<= 1) if (i2 & i3) trapcnt++;
			}
		}
		else if (trapactivemap[i1]) {
			clearcnt = 8;  /* assume all 8 bits */
			break;  /* get out as setcnt and trapcnt will remain zero */
		}
	}
	if (clearcnt) {
		if (clearcnt + setcnt < trapcnt) {
			clientput("<tc>", 4);
			for (i1 = 0; i1 < (int) sizeof(trapactivemap); i1++) {
				if (trapmap[i1] != trapactivemap[i1]) {
					i3 = trapmap[i1];
					i4 = trapactivemap[i1];
					for (i5 = 0, i2 = 1; i2 < 256; i5++, i2 <<= 1) {
						if ((i2 & i4) && !(i2 & i3)) {
							i6 = (i1 << 3) + i5;
							if (i6 > ' ' && i6 <= '~') {
								if (i6 == '<') clientput("&lt;", 4);
								else if (i6 == '>') clientput("&gt;", 4);
								else if (i6 == '&') clientput("&amp;", 5);
								else if (i6 == '"') clientput("&quot;", 6);
								else if (i6 == '\'') clientput("&apos;", 6);
								else {
									chr = (CHAR) i6;
									clientput(&chr, 1);
								}
							}
							else {
								clientput("<c>", 3);
								clientputnum(i6);
								clientput("</c>", 4);
							}
						}
					}
				}
			}
			clientput("</tc>", 5);
		}
		else {
			clientput("<tc><all/></tc>", 15);
			if (clientsend(0, 0) < 0) {
#if OS_WIN32
				pviend();
#endif
				return RC_ERROR;
			}
			memset(trapactivemap, 0, sizeof(trapactivemap));
			clearcnt = 0;
			setcnt = trapcnt;
		}
	}
	if (setcnt) {
		clientput("<ts>", 4);
		for (i1 = 0; i1 < (int) sizeof(trapactivemap); i1++) {
			if (trapmap[i1] != trapactivemap[i1]) {
				i3 = trapmap[i1];
				i4 = trapactivemap[i1];
				for (i5 = 0, i2 = 1; i2 < 256; i5++, i2 <<= 1) {
					if ((i2 & i3) && !(i2 & i4)) {
						i6 = (i1 << 3) + i5;
						if (i6 > ' ' && i6 <= '~') {
							if (i6 == '<') clientput("&lt;", 4);
							else if (i6 == '>') clientput("&gt;", 4);
							else if (i6 == '&') clientput("&amp;", 5);
							else if (i6 == '"') clientput("&quot;", 6);
							else if (i6 == '\'') clientput("&apos;", 6);
							else {
								chr = (CHAR) i6;
								clientput(&chr, 1);
							}
						}
						else {
							clientput("<c>", 3);
							clientputnum(i6);
							clientput("</c>", 4);
						}
					}
				}
			}
		}
		clientput("</ts>", 5);
	}
	if ((clearcnt || setcnt) && clientsend(0, 0) < 0) {
#if OS_WIN32
		pviend();
#endif
		return RC_ERROR;
	}
	if (trapmap != NULL) memcpy(trapactivemap, trapmap, sizeof(trapactivemap));
	evtclear(trapeventid);
	while (traphead != traptail) {
		i1 = trapbuffer[traphead];
		if (trapactivemap[i1 >> 3] & (1 << (i1 & 0x07))) {
			evtset(trapeventid);
			break;
		}
		if (++traphead == sizeof(trapbuffer) / sizeof(*trapbuffer)) traphead = 0;
	}
#if OS_WIN32
	pviend();
#endif
	return 0;
}

int clienttrapget(int *trapval)
{
	int i1;
#if OS_WIN32
	pvistart();
#endif
	if (traphead == traptail) {
#if OS_WIN32
		pviend();
#endif
		return RC_ERROR;
	}
	*trapval = trapbuffer[traphead++];
	if (traphead == sizeof(trapbuffer) / sizeof(*trapbuffer)) traphead = 0;
	evtclear(trapeventid);
	while (traphead != traptail) {
		i1 = trapbuffer[traphead];
		if (trapactivemap[i1 >> 3] & (1 << (i1 & 0x07))) {
			evtset(trapeventid);
			break;
		}
		if (++traphead == sizeof(trapbuffer) / sizeof(*trapbuffer)) traphead = 0;
	}
#if OS_WIN32
	pviend();
#endif
	return 0;
}

int clienttrappeek(int *trapval)
{
#if OS_WIN32
	pvistart();
#endif
	if (traphead == traptail) {
#if OS_WIN32
		pviend();
#endif
		return RC_ERROR;
	}
	*trapval = trapbuffer[traphead];
#if OS_WIN32
	pviend();
#endif
	return 0;
}

/**
 * On input, size is the number of bytes available in memory
 *
 * If success, return will be zero and size will be the number of bytes used.
 * Otherwise RC_ERROR is returned and size will be set to zero.
 */
INT clientvidsave(INT type, UCHAR *memory, INT *size)
{
	INT i1, i2;

	if (type == CLIENT_VID_CHAR) {
		i2 = (wscrgt - wsclft + 1);
		i1 = i2 * (wscbot - wsctop + 1);
		if (i1 <= *size) {
			*size = i1;
			bufgetrect(memory, wsctop, wscbot, wsclft, wscrgt, TRUE);
			return 0;
		}
		if (*size > 0) {
			i1 = *size / i2;
			i2 = *size - i1 * i2;
			if (i1) bufgetrect(memory, wsctop, wsctop + i1 - 1, wsclft, wscrgt, TRUE);
			if (i2) bufgetrect(memory, wsctop + i1, wsctop + i1, wsclft, wsclft + i2 - 1, TRUE);
		}
		else *size = 0;
	}
	else if (type == CLIENT_VID_WIN) {
		i1 = (wscrgt - wsclft + 1) * (wscbot - wsctop + 1) * CELLWIDTH;
		if (i1 <= *size) {
			*size = i1;
			bufgetrect(memory, wsctop, wscbot, wsclft, wscrgt, FALSE);
			return 0;
		}
		*size = 0;
	}
	else if (type == CLIENT_VID_STATE) {
		if ((UINT)(*size) >= sizeof(STATESAVE)) {
			clientstatesave(memory);
			*size = sizeof(STATESAVE);
			return 0;
		}
		return RC_ERROR;
	}
	else if (type == CLIENT_VID_SCRN) {
		i1 = maxlines * maxcolumns * CELLWIDTH + sizeof(STATESAVE);
		if (*size >= i1) {
			clientstatesave(memory);
			bufgetrect(memory + sizeof(STATESAVE), 0, maxlines - 1, 0, maxcolumns - 1, FALSE);
			*size = i1;
			return 0;
		}
	}
	return RC_ERROR;
}

#if defined(Linux) || defined(__MACOSX) || defined(FREEBSD)
typedef BYTE BOOLEAN;
#endif

int clientvidrestore(INT type, UCHAR *memory, INT size)
{
	INT i1, i2, cmdcnt, count, incsize, len;
	BOOLEAN sendAnsi256Color = FALSE;
	INT testkynattr;
	STATESAVE savearea;
#if OS_WIN32
	INT testattr;
	INT attr;
	INT newchr, savechr;
#else
	UINT64 testattr;
	UINT64 attr;
	UINT64 newchr, savechr;
	UINT64 aTtrs, fclr, bclr;
	UCHAR theChar;
#endif
	INT32 vidcmd[32];
	CHAR *ptr, work[256];
	UCHAR bits, mapclear[(VID_MAXKEYVAL >> 3) + 1], mapset[(VID_MAXKEYVAL >> 3) + 1], *mapptr;

#if OS_UNIX
	if (vidflags & VID_FLAG_COLORMODE_ANSI256) {
		if (smartClientMajorVersion > 16 || (smartClientMajorVersion == 16 && smartClientMinorVersion >= 3))
			sendAnsi256Color = TRUE;
	}
#endif

	if (type == CLIENT_VID_CHAR) {
		i2 = (wscrgt - wsclft + 1);
		i1 = i2 * (wscbot - wsctop + 1);
		if (size >= i1) bufputrect(memory, wsctop, wscbot, wsclft, wscrgt, TRUE, dspattr);
		else if (size > 0) {
			i1 = size / i2;
			i2 = size - i1 * i2;
			if (i1) bufputrect(memory, wsctop, wsctop + i1 - 1, wsclft, wscrgt, TRUE, dspattr);
			if (i2) bufputrect(memory, wsctop + i1, wsctop + i1, wsclft, wsclft + i2 - 1, TRUE, dspattr);
		}
		ptr = "charrest";
	}
	else if (type == CLIENT_VID_WIN) {
		if (size < (wscrgt - wsclft + 1) * (wscbot - wsctop + 1) * CELLWIDTH) return 1;
		bufputrect(memory, wsctop, wscbot, wsclft, wscrgt, FALSE, 0);
		ptr = "winrest";
	}
	else if (type == CLIENT_VID_SCRN) {
		if ((UINT)size < maxlines * maxcolumns * CELLWIDTH + sizeof(STATESAVE)) return 1;
		bufputrect(memory + sizeof(STATESAVE), 0, maxlines - 1, 0, maxcolumns - 1, FALSE, 0);
		ptr = "scrnrest";
	}
	else if (type == CLIENT_VID_STATE) {
		if ((UINT)size < sizeof(STATESAVE)) return 1;
	}
	else return 1; // 'type' is not one of the recognized types.

	if (type == CLIENT_VID_STATE || type == CLIENT_VID_SCRN) {

		// Move from caller's memory to the structure
		memcpy((void*)&savearea, (void*)memory, sizeof(STATESAVE));
		keyinechochar = savearea.keyinechochar;
		kynattr = savearea.kynattr;

		cmdcnt = 0;
		if (savearea.wsclft != wsclft) vidcmd[cmdcnt++] = VID_WIN_LEFT | savearea.wsclft;
		if (savearea.wsctop != wsctop) vidcmd[cmdcnt++] = VID_WIN_TOP | savearea.wsctop;
		if (savearea.wscrgt != wscrgt) vidcmd[cmdcnt++] = VID_WIN_RIGHT | savearea.wscrgt;
		if (savearea.wscbot != wscbot) vidcmd[cmdcnt++] = VID_WIN_BOTTOM | savearea.wscbot;
		if (savearea.vidh != vidh) vidcmd[cmdcnt++] = VID_HORZ | (USHORT)(savearea.vidh - wsclft);
		if (savearea.vidv != vidv) vidcmd[cmdcnt++] = VID_VERT | (USHORT)(savearea.vidv - wsctop);

		//memcpy((UCHAR*)&attr, &memory[12], sizeof(dspattr));
		testattr = savearea.dspattr ^ dspattr;
		if (testattr & DSPATTR_AUXPORT) {
			if (dspattr & DSPATTR_AUXPORT) vidcmd[cmdcnt++] = VID_PRT_OFF;
			else vidcmd[cmdcnt++] = VID_PRT_ON;
		}
		if (testattr & DSPATTR_RAWMODE) {
			if (dspattr & DSPATTR_RAWMODE) vidcmd[cmdcnt++] = VID_RAW_OFF;
			else vidcmd[cmdcnt++] = VID_RAW_ON;
		}
		if (testattr & DSPATTR_ROLL) {
			if (dspattr & DSPATTR_ROLL) vidcmd[cmdcnt++] = VID_NL_ROLL_OFF;
			else vidcmd[cmdcnt++] = VID_NL_ROLL_ON;
		}
		if (testattr & DSPATTR_DISPLEFT) {
			if (dspattr & DSPATTR_DISPLEFT) vidcmd[cmdcnt++] = VID_RL_OFF;
			else vidcmd[cmdcnt++] = VID_RL_ON;
		}
		if (testattr & DSPATTR_DISPDOWN) {
			if (dspattr & DSPATTR_DISPDOWN) vidcmd[cmdcnt++] = VID_DSPDOWN_OFF;
			else vidcmd[cmdcnt++] = VID_DSPDOWN_ON;
		}
		if (testattr & DSPATTR_REV) {
			if (dspattr & DSPATTR_REV) vidcmd[cmdcnt++] = VID_REV_OFF;
			else vidcmd[cmdcnt++] = VID_REV_ON;
		}
		if (testattr & DSPATTR_DIM) {
			if (dspattr & DSPATTR_DIM) vidcmd[cmdcnt++] = VID_DIM_OFF;
			else vidcmd[cmdcnt++] = VID_DIM_ON;
		}
		if (testattr & DSPATTR_BOLD) {
			if (dspattr & DSPATTR_BOLD) vidcmd[cmdcnt++] = VID_BOLD_OFF;
			else vidcmd[cmdcnt++] = VID_BOLD_ON;
		}
		if (testattr & DSPATTR_BLINK) {
			if (dspattr & DSPATTR_BLINK) vidcmd[cmdcnt++] = VID_BLINK_OFF;
			else vidcmd[cmdcnt++] = VID_BLINK_ON;
		}
		if (testattr & DSPATTR_UNDERLINE) {
			if (dspattr & DSPATTR_UNDERLINE) vidcmd[cmdcnt++] = VID_UL_OFF;
			else vidcmd[cmdcnt++] = VID_UL_ON;
		}
		if (testattr & DSPATTR_FGCOLORMASK)
			vidcmd[cmdcnt++] = VID_FOREGROUND
			| (USHORT)((savearea.dspattr & DSPATTR_FGCOLORMASK) >> DSPATTR_FGCOLORSHIFT);
		if (testattr & DSPATTR_BGCOLORMASK)
			vidcmd[cmdcnt++] = VID_BACKGROUND
			| (USHORT)((savearea.dspattr & DSPATTR_BGCOLORMASK) >> DSPATTR_BGCOLORSHIFT);

		i1 = savearea.viddblflag ^ viddblflag;
		if (i1 & 0x01) {
			if (viddblflag & 0x01) vidcmd[cmdcnt++] = VID_HDBL_OFF;
			else vidcmd[cmdcnt++] = VID_HDBL_ON;
		}
		if (i1 & 0x02) {
			if (viddblflag & 0x02) vidcmd[cmdcnt++] = VID_VDBL_OFF;
			else vidcmd[cmdcnt++] = VID_VDBL_ON;
		}

		if (savearea.vidcurmode != vidcurmode) {
			if (savearea.vidcurmode == CURSMODE_ON) vidcmd[cmdcnt++] = VID_CURSOR_ON;
			else if (savearea.vidcurmode == CURSMODE_OFF) vidcmd[cmdcnt++] = VID_CURSOR_OFF;
			else vidcmd[cmdcnt++] = VID_CURSOR_NORM;
		}

		if (savearea.vidcurtype != vidcurtype) {
			if (savearea.vidcurtype == CURSOR_ULINE) vidcmd[cmdcnt++] = VID_CURSOR_ULINE;
			else if (savearea.vidcurtype == CURSOR_HALF) vidcmd[cmdcnt++] = VID_CURSOR_HALF;
			else vidcmd[cmdcnt++] = VID_CURSOR_BLOCK;
		}

		if (savearea.keyinechochar != keyinechochar)
			vidcmd[cmdcnt++] = VID_KBD_ECHOCHR | savearea.keyinechochar;

		//memcpy((UCHAR*)&testkynattr, &memory[ri1], sizeof(kynattr));
		testkynattr = savearea.kynattr;

		//attr = memory[28] + ((INT) memory[29] << 8) + ((INT) memory[30] << 16) + ((INT) memory[31] << 24);
		i1 = testkynattr ^ kynattr;
		if (i1 & KYNATTR_EDITON) {
			if (kynattr & KYNATTR_EDITON) vidcmd[cmdcnt++] = VID_KBD_ED_OFF;
			else vidcmd[cmdcnt++] = VID_KBD_ED_ON;
		}
		if (i1 & KYNATTR_EDITOVERSTRIKE) {
			if (kynattr & KYNATTR_EDITOVERSTRIKE) vidcmd[cmdcnt++] = VID_KBD_OVS_OFF;
			else vidcmd[cmdcnt++] = VID_KBD_OVS_ON;
		}
		/* skip KYNATTR_AUTOENTER as it is set before each keyin */
		/* skip KYNATTR_RIGHTTOLEFT as it is not supported */
		if (i1 & KYNATTR_CLICK) {
			if (kynattr & KYNATTR_CLICK) vidcmd[cmdcnt++] = VID_KBD_CK_OFF;
			else vidcmd[cmdcnt++] = VID_KBD_CK_ON;
		}
		if (i1 & KYNATTR_NOECHO) {
			if (kynattr & KYNATTR_NOECHO) vidcmd[cmdcnt++] = VID_KBD_EON;
			else vidcmd[cmdcnt++] = VID_KBD_EOFF;
		}
		if (i1 & KYNATTR_ECHOSECRET) {
			if (kynattr & KYNATTR_ECHOSECRET) vidcmd[cmdcnt++] = VID_KBD_ESOFF;
			else vidcmd[cmdcnt++] = VID_KBD_ESON;
		}
		if (i1 & KYNATTR_KEYCASEMASK) {
			i1 = testkynattr & KYNATTR_KEYCASEMASK;
			if (i1 == KYNATTR_KEYCASEUPPER) vidcmd[cmdcnt++] = VID_KBD_UC;
			else if (i1 == KYNATTR_KEYCASEINVERT) vidcmd[cmdcnt++] = VID_KBD_IC;
			else if (i1 == KYNATTR_KEYCASELOWER) vidcmd[cmdcnt++] = VID_KBD_LC;
			else vidcmd[cmdcnt++] = VID_KBD_IN;
		}
		if (cmdcnt) {
			clientput("<k>", 3);
			vidcmd[cmdcnt] = VID_END_NOFLUSH;
			if (clientvidput(vidcmd, CLIENTVIDPUT_SAVE | CLIENTVIDPUT_SEND) < 0) return RC_ERROR;
			clientput("</k>", 4);
			if (clientsend(0, 0) < 0) return RC_ERROR;
		}
		mapptr = savearea.finishmap;
		memset(mapclear, 0, sizeof(mapclear));
		memset(mapset, 0, sizeof(mapset));
		for (i1 = 0; i1 < (INT)sizeof(finishmap); i1++) {
			bits = finishmap[i1] ^ mapptr[i1];
			mapclear[i1] = bits & finishmap[i1];
			mapset[i1] = bits & mapptr[i1];
		}
		if (clientclearendkey(mapclear) < 0) return RC_ERROR;
		if (clientsetendkey(mapset) < 0) return RC_ERROR;
		memory += sizeof(STATESAVE);
		size -= sizeof(STATESAVE);
	}
	if (type != CLIENT_VID_STATE) // Must be SCRN or WIN
	{
		clientput("<", 1);
		clientput(ptr, -1);
		clientput(">", 1);
		if (type == CLIENT_VID_CHAR) {
			incsize = 1;
			attr = 0;
		}
		else {
			incsize = CELLWIDTH;
			// Grab the attributes of the first character
			if (vidflags & VID_FLAG_COLORMODE_OLD) {
				attr = ~(((INT) memory[1] << 8) + ((INT) memory[2] << 16));
			}
			else {
				//*mem++ = (UCHAR)(chr64 >> 16);	// The attributes
				//*mem++ = (UCHAR)(chr64 >> DSPATTR_FGCOLORSHIFT);	// The fore color
				//*mem++ = (UCHAR)(chr64 >> DSPATTR_BGCOLORSHIFT);	// The back color
				attr = ~(((UINT64) memory[1] << 16) + ((UINT64) memory[2] << DSPATTR_FGCOLORSHIFT)
						+ ((UINT64) memory[3] << DSPATTR_BGCOLORSHIFT));
			}
		}
		for (count = len = 0; ; memory += incsize, size -= incsize) {
			if (size) {
				if (type == CLIENT_VID_CHAR) newchr = memory[0];
				else {
					if (vidflags & VID_FLAG_COLORMODE_OLD)
						newchr = memory[0] + ((INT) memory[1] << 8) + ((INT) memory[2] << 16);
#if OS_UNIX
					else if (vidflags & VID_FLAG_COLORMODE_ANSI256) {
						theChar = memory[0];
						aTtrs = memory[1];
						fclr = memory[2];
						bclr = memory[3];
						newchr = (UINT64)theChar | (aTtrs << 16) | (fclr << DSPATTR_FGCOLORSHIFT) | (bclr << DSPATTR_BGCOLORSHIFT);
					}
#endif
				}
			}
			else newchr = ~savechr;
			if (count && (newchr != savechr || count == 98)) {
				attr ^= savechr;
				if (attr & (DSPATTR_ATTRIBMASK | DSPATTR_FGCOLORMASK | DSPATTR_BGCOLORMASK)) {
					if (attr & DSPATTR_ATTRIBMASK) {
						work[len++] = '~';
						work[len++] = (CHAR)(((savechr & DSPATTR_ATTRIBMASK) >> DSPATTR_ATTRIBSHIFT) + '?');
					}
					if (attr & DSPATTR_FGCOLORMASK) {
						work[len++] = '^';
						if (sendAnsi256Color) {
							i1 = (savechr & DSPATTR_FGCOLORMASK) >> DSPATTR_FGCOLORSHIFT;
							len += sprintf(&work[len], "%d", i1);
						}
						else {
							work[len++] = (CHAR)(((savechr & DSPATTR_FGCOLORMASK) >> DSPATTR_FGCOLORSHIFT) + '?');
						}
					}
					if (attr & DSPATTR_BGCOLORMASK) {
						work[len++] = '!';
						if (sendAnsi256Color) {
							i1 = (savechr & DSPATTR_BGCOLORMASK) >> DSPATTR_BGCOLORSHIFT;
							len += sprintf(&work[len], "%d", i1);
						}
						else {
							work[len++] = (CHAR)(((savechr & DSPATTR_BGCOLORMASK) >> DSPATTR_BGCOLORSHIFT) + '?');
						}
					}
				}
				attr = savechr;
				if (count >= 4) {
					work[len++] = '`';
					work[len++] = (CHAR)(count - 4 + ' ');
					count = 1;
				}
				savechr &= 0xFF;
				do {
					if (savechr == '~' || savechr == '^' || savechr == '!' || savechr == '@' || savechr == '`')
						work[len++] = '@';
					work[len++] = (CHAR) savechr;
				} while (--count);
			}
			if (!size || len >= (int) sizeof(work) - 32) {
				clientputdata(work, len);
				if (!size) break;
				len = 0;
			}
			savechr = newchr;
			count++;
		}
		clientput("</", 2);
		clientput(ptr, -1);
		clientput(">", 1);
		if (clientsend(0, 0) < 0) return RC_ERROR;
	}
	return 0;
}

INT clientvidsize(INT type)
{
	if (type == CLIENT_VID_WIN) return (wscrgt - wsclft + 1) * (wscbot - wsctop + 1) * CELLWIDTH;
	if (type == CLIENT_VID_SCRN) return (maxcolumns * maxlines * CELLWIDTH) + sizeof(STATESAVE); //VID_STATESIZE;
	if (type == CLIENT_VID_STATE) return sizeof(STATESAVE); //VID_STATESIZE;
	return 0;
}

void clientvidgetpos(INT *h, INT *v)
{
	*h = vidh - wsclft;
	*v = vidv - wsctop;
}

void clientvidgetwin(INT *top, INT *bottom, INT *left, INT *right)
{
	*top = wsctop;
	*bottom = wscbot;
	*left = wsclft;
	*right = wscrgt;
}

ELEMENT *clientelement()
{
	return elementptr;
}

int clientgeterror(char *msg, int length)
{
	int i1;

	if (--length < 0) return RC_ERROR;

	i1 = (INT) strlen(clienterror);
	if (i1 > length) i1 = length;
	memcpy(msg, clienterror, i1);
	msg[i1] = '\0';
	return 0;
}

static void buferaserect(INT top, INT bottom, INT left, INT right, ATTR attr)
{
#if OS_WIN32
	INT *buf;
#else
	UINT64 *buf;
#endif
	INT i1, i2, len;

	if (attr & DSPATTR_AUXPORT) return;

	attr = (attr & (DSPATTR_ATTRIBMASK | DSPATTR_FGCOLORMASK | DSPATTR_BGCOLORMASK)) | ' ';
	len = right - left + 1;
	for (i2 = top; i2 <= bottom; i2++) {
		buf = *clientbuffer + i2 * maxcolumns + left;
		for (i1 = len; i1--; ) *buf++ = attr;
	}
}

static void bufmove(INT direction, INT top, INT bottom, INT left, INT right, ATTR attr)
{
#if OS_WIN32
	INT *src, *dest;
#else
	UINT64 *src, *dest;
#endif
	INT i1, i2;

	if ((top == bottom && (direction == ROLL_UP || direction == ROLL_DOWN)) ||
	    (left == right && (direction == ROLL_LEFT || direction == ROLL_RIGHT))) {
		buferaserect(top, bottom, left, right, attr);
		return;
	}

	if (attr & DSPATTR_AUXPORT) return;

	attr = (attr & (DSPATTR_ATTRIBMASK | DSPATTR_FGCOLORMASK | DSPATTR_BGCOLORMASK)) | ' ';
	if (direction == ROLL_UP) {
		for (i1 = top; i1 < bottom; i1++) {
			dest = *clientbuffer + i1 * maxcolumns + left;
			src = dest + maxcolumns;
			for (i2 = left; i2 <= right; i2++) *dest++ = *src++;
		}
		dest = *clientbuffer + bottom * maxcolumns + left;
		for (i2 = left; i2 <= right; i2++) *dest++ = attr;
	}
	else if (direction == ROLL_DOWN) {
		for (i1 = bottom; i1 > top; i1--) {
			dest = *clientbuffer + i1 * maxcolumns + left;
			src = dest - maxcolumns;
			for (i2 = left; i2 <= right; i2++) *dest++ = *src++;
		}
		dest = *clientbuffer + top * maxcolumns + left;
		for (i2 = left; i2 <= right; i2++) *dest++ = attr;
	}
	else if (direction == ROLL_LEFT) {
		for (i1 = top; i1 <= bottom; i1++) {
			dest = *clientbuffer + i1 * maxcolumns + left;
			src = dest + 1;
			for (i2 = left; i2 < right; i2++) *dest++ = *src++;
			*dest = attr;
		}
	}
	else if (direction == ROLL_RIGHT) {
		for (i1 = top; i1 <= bottom; i1++) {
			dest = *clientbuffer + i1 * maxcolumns + right;
			src = dest - 1;
			for (i2 = left; i2 < right; i2++) *dest-- = *src--;
			*dest = attr;
		}
	}
}

static void bufinsertchar(INT h, INT v, INT top, INT bottom, INT left, INT right, ATTR charattr)
{
#if OS_WIN32
	INT *buf, *src, *dest;
#else
	UINT64 *buf, *src, *dest;
#endif

	INT len;

	if (charattr & DSPATTR_AUXPORT) return;

	if (--h < left) {
		h = right;
		v--;
	}
	buf = *clientbuffer;
	while (v > vidv || (v == vidv && h == right)) {
		if (h == right) {
			src = buf + v * maxcolumns + right;
			dest = buf + (v + 1) * maxcolumns + left;
			*dest = *src;
			if (--h < left) {
				h = right;
				v--;
			}
		}
		else {
			src = buf + v * maxcolumns + h;
			dest = src + 1;
			for (len = h - left + 1; len--; ) *dest-- = *src--;
			h = right;
			v--;
		}
	}
	if (vidh != right) {
		src = buf + v * maxcolumns + h;
		dest = src + 1;
		for (len = h - vidh + 1; len--; ) *dest-- = *src--;
	}
	dest = buf + vidv * maxcolumns + vidh;
	*dest = charattr & (DSPATTR_ATTRIBMASK | DSPATTR_FGCOLORMASK | DSPATTR_BGCOLORMASK);
}

static void bufdeletechar(INT h, INT v, INT top, INT bottom, INT left, INT right, ATTR charattr)
{
#if OS_WIN32
	INT *buf, *src, *dest;
#else
	UINT64 *buf, *src, *dest;
#endif
	INT endh, endv, len;

	if (charattr & DSPATTR_AUXPORT) return;

	endh = h;
	endv = v;
	h = vidh;
	v = vidv;
	if (++h > right) {
		h = left;
		v++;
	}
	buf = *clientbuffer;
	while (v < endv || (v == endv && h == left)) {
		if (h == left) {
			src = buf + v * maxcolumns + left;
			dest = buf + (v - 1) * maxcolumns + right;
			*dest = *src;
			if (++h > right) {
				h = left;
				v++;
			}
		}
		else {
			src = buf + v * maxcolumns + h;
			dest = src - 1;
			for (len = right - h + 1; len--; ) *dest++ = *src++;
			h = left;
			v++;
		}
	}
	if (endh != left) {
		src = buf + v * maxcolumns + h;
		dest = src - 1;
		for (len = endh - h + 1; len--; ) *dest++ = *src++;
	}
	dest = buf + endv * maxcolumns + endh;
	*dest = charattr & (DSPATTR_ATTRIBMASK | DSPATTR_FGCOLORMASK | DSPATTR_BGCOLORMASK);
}

/**
 * Move from our internal screen buffer (clientbuffer) to the user's DX memory
 */
static void bufgetrect(UCHAR *mem, INT top, INT bottom, INT left, INT right, INT charonly)
{
#if OS_WIN32
	INT *buf;
#else
	UINT64 *buf;
#endif
	INT i1, i2, chr;
	UINT64 chr64;

	if (charonly) {
		for (i1 = top; i1 <= bottom; i1++) {
			buf = *clientbuffer + i1 * maxcolumns + left;
			for (i2 = left; i2 <= right; i2++) {
				chr = *buf++;
				if (chr & DSPATTR_GRAPHIC) chr = ' ';
				*mem++ = (UCHAR) chr;
			}
		}
	}
	else {
		for (i1 = top; i1 <= bottom; i1++) {
			buf = *clientbuffer + i1 * maxcolumns + left;
			for (i2 = left; i2 <= right; i2++) {
				if (vidflags & VID_FLAG_COLORMODE_OLD) {
					chr = *buf++;
					*mem++ = (UCHAR) chr;
					*mem++ = (UCHAR)(chr >> 8);
					*mem++ = (UCHAR)(chr >> 16);
				}
				else if (vidflags & VID_FLAG_COLORMODE_ANSI256) {
					chr64 = *buf++;
					*mem++ = (UCHAR) chr64;			// The ascii CHAR
					*mem++ = (UCHAR)(chr64 >> 16);	// The attributes
					*mem++ = (UCHAR)(chr64 >> DSPATTR_FGCOLORSHIFT);	// The fore color
					*mem++ = (UCHAR)(chr64 >> DSPATTR_BGCOLORSHIFT);	// The back color
				}
			}
		}
	}
}

/**
 * Move from the user's DX memory to our internal screen buffer (clientbuffer)
 */
static void bufputrect(UCHAR *mem, INT top, INT bottom, INT left, INT right, INT charonly, ATTR attr)
{
#if OS_WIN32
	INT *buf;
#else
	UINT64 *buf;
#endif
	INT i1, chr;
#if OS_UNIX
	UCHAR theChar;
	UINT64 aTtrs, fclr, bclr;
#endif

	if (attr & DSPATTR_AUXPORT) return;

	attr &= (DSPATTR_ATTRIBMASK | DSPATTR_FGCOLORMASK | DSPATTR_BGCOLORMASK);
	if (charonly) {
		attr &= ~DSPATTR_GRAPHIC;
		for ( ; top <= bottom; top++) {
			buf = *clientbuffer + top * maxcolumns + left;
			for (i1 = left; i1 <= right; i1++)
				*buf++ = *mem++ | attr;
		}
	}
	else {
		for ( ; top <= bottom; top++) {
			buf = *clientbuffer + top * maxcolumns + left;
			for (i1 = left; i1 <= right; i1++) {
				if (vidflags & VID_FLAG_COLORMODE_OLD) {
					chr = *mem++;
					chr |= (INT)(*mem++) << 8;
					chr |= (INT)(*mem++) << 16;
					*buf++ = chr;
				}
#if OS_UNIX
				else if (vidflags & VID_FLAG_COLORMODE_ANSI256) {
					theChar = *mem++;
					aTtrs = *mem++;
					fclr = *mem++;
					bclr = *mem++;
					*buf++ = theChar | (aTtrs << 16) | (fclr << 32) | (bclr << 40);
				}
#endif
			}
		}
	}
}

#if OS_WIN32
static DWORD WINAPI recvproc(LPVOID lpParam)
{
	int i1;
	for ( ; ; ) {
		i1 = tcprecv(sockethandle, (UCHAR *) &recvbuffer[recvbufcnt], recvbufsize - recvbufcnt, tcpflags,
			keepalivetime);
		if (i1 == SOCKET_ERROR) {
			clientflags |= CLIENT_FLAG_SHUTDOWN;
			while (!shutdowneventid) Sleep(1000);
			evtset(shutdowneventid);
			Sleep(10000);
			continue;
		}
		if (!i1) {
			if (clientRolloutInEffect) {
				clientRolloutTime -= keepalivetime;
				if (clientRolloutTime > 1) continue;
			}
			clientflags |= CLIENT_FLAG_SHUTDOWN;
			while (!shutdowneventid) Sleep(1000);
			evtset(shutdowneventid);
			Sleep(10000);
			continue;
		}
		recvbufcnt += i1;
		processrecv();
		WaitForSingleObject(threadevent, INFINITE);
	}
	return 0;
}
#endif

#if OS_UNIX
static INT recvcallback(void *handle, INT polltype)
{
	INT i1;

	if ((polltype & EVENTS_ERROR) || (clientflags & CLIENT_FLAG_SHUTDOWN)) {
		evtset(shutdowneventid);
		return 1;
	}

	if (polltype & EVENTS_READ) {
		do {
			i1 = tcprecv(sockethandle, (UCHAR *) &recvbuffer[recvbufcnt], recvbufsize - recvbufcnt,
					tcpflags, keepalivetime);
			if (i1 == SOCKET_ERROR) {
				clientflags |= CLIENT_FLAG_SHUTDOWN;
				evtset(shutdowneventid);
				return 1;
			}
			if (!i1) {
				if (clientRolloutInEffect) {
					clientRolloutTime -= keepalivetime;
					if (clientRolloutTime > 1) continue;
				}
				clientflags |= CLIENT_FLAG_SHUTDOWN;
				evtset(shutdowneventid);
				return 1;
			}
			break;
		} while(TRUE);
		recvbufcnt += i1;
		processrecv();
	}
	return 0;
}
#endif

static void processrecv()
{
	static int packetnum;
	int i1, i2, datasize, matchflag, newtail, pos, size;
	char work[2], *ptr;
	ELEMENT *element;

#if OS_WIN32
	pvistart();
#endif
	for (pos = 0; !(clientflags & CLIENT_FLAG_ELEMENT); ) {
		if (elementptr != NULL) elementptr = elementptr->nextelement;
		if (elementptr == NULL) {
			if (recvbufcnt < pos + RECV_HEADER) break;
			i1 = ntoi((UCHAR *)(recvbuffer + pos + RECV_SIZEOFF), RECV_SIZELEN, &datasize);
			if (i1 < 0 || datasize <= 0) {  /* invalid data size */
/*** CODE: LOG ERROR ***/
				clearbuffer();
				break;
			}
			if (RECV_HEADER + datasize > recvbufsize) {
				for (size = recvbufsize << 1; RECV_HEADER + datasize > size; size <<= 1);
				ptr = (CHAR *) realloc(recvbuffer, size + 2);
				if (ptr == NULL) {
/*** CODE: LOG ERROR ***/
					clearbuffer();
					break;
				}
				recvbuffer = ptr;
				recvbufsize = size;
			}
			if (recvbufcnt < pos + RECV_HEADER + datasize) break;
#ifdef _DEBUG
			recvbuffer[recvbufcnt] = '\0';  /* debugging only */
#endif

			i1 = xmlparse(recvbuffer + pos + RECV_HEADER, datasize, elembuffer, elembufsize);
			if (i1 < 0) {
				if (i1 == -1) {
					ptr = (CHAR *) realloc(elembuffer, elembufsize << 1);
					if (ptr == NULL) {
/*** CODE: LOG ERROR ***/
						clearbuffer();
						break;
					}
					elembuffer = ptr;
					elembufsize <<= 1;
					continue;
				}
/*** CODE: LOG ERROR ***/
				clearbuffer();
				break;
			}
			if (logfile != NULL) {
				fputs("RCV: ", logfile);
				memcpy(work, recvbuffer + pos + RECV_HEADER + datasize, 2);
				memcpy(recvbuffer + pos + RECV_HEADER + datasize, "\n\0", 2);
				fputs(recvbuffer + pos, logfile);
				memcpy(recvbuffer + pos + RECV_HEADER + datasize, work, 2);
				fflush(logfile);
			}
			mscntoi((UCHAR *)(recvbuffer + pos + RECV_SNUMOFF), &packetnum, RECV_SNUMLEN);
			pos += RECV_HEADER + datasize;
			elementptr = (ELEMENT *) elembuffer;
		}
#define CLIENT_MATCH_SETEVENT	1
#define CLIENT_MATCH_GUIMSG	2
		matchflag = 0;
		switch (elementptr->tag[0]) {
		case 'a':
			if (!strcmp(elementptr->tag, "alivechk")) {
#if OS_UNIX
				resetKaTimer();
#endif
			}
			else matchflag = CLIENT_MATCH_GUIMSG;
			break;
		case 'b':
			if (!strcmp(elementptr->tag, "break")) {
				if (breakeventid != 0) evtset(breakeventid);
			}
			else matchflag = CLIENT_MATCH_GUIMSG;
			break;
		case 'g':
			if (!strcmp(elementptr->tag, "getwindow")) matchflag = CLIENT_MATCH_SETEVENT;
			else if (!strcmp(elementptr->tag, "getrowcount")) matchflag = CLIENT_MATCH_SETEVENT;
			else matchflag = CLIENT_MATCH_GUIMSG;
			break;
		case 'i':
			if (!strcmp(elementptr->tag, "image")) matchflag = CLIENT_MATCH_SETEVENT;
			else matchflag = CLIENT_MATCH_GUIMSG;
			break;
		case 'q':
			if (!strcmp(elementptr->tag, "quit")) {
				clientflags |= CLIENT_FLAG_SHUTDOWN;
				evtset(shutdowneventid);
			}
			else matchflag = CLIENT_MATCH_GUIMSG;
			break;
		case 'r':
			if (!elementptr->tag[1]) matchflag = CLIENT_MATCH_SETEVENT;
			else matchflag = CLIENT_MATCH_GUIMSG;
			break;
		case 's':
			if (!elementptr->tag[1]) matchflag = CLIENT_MATCH_SETEVENT;
			else if (!strcmp(elementptr->tag, "status")) matchflag = CLIENT_MATCH_SETEVENT;
			else if (!strcmp(elementptr->tag, "sendcolor")) matchflag = CLIENT_MATCH_SETEVENT;
			else if (!strcmp(elementptr->tag, "sendpos")) matchflag = CLIENT_MATCH_SETEVENT;
			else if (!strcmp(elementptr->tag, "statusbarheight")) matchflag = CLIENT_MATCH_SETEVENT;
			else if (!strcmp(elementptr->tag, "smartclient")) {
				/*
				 * <smartserver version="16.2+"></smartserver>
				 */
				ATTRIBUTE *a1 = elementptr->firstattribute;
				while(a1 != NULL) {
					if (!strcmp(a1->tag, "utcoffset")) {
						strcpy(smartClientUTCOffset, a1->value);
					}
					else if (!strcmp(a1->tag, "version")) {
						i1 = 0;
						i2 = 0;
						while (a1->value[i1] != '\0') {
							switch (i2) {
							case 0: // Look for 1st digit
								if (!(isdigit(a1->value[i1]))) i1++;
								else {
									smartClientMajorVersion = a1->value[i1++] - '0';
									i2 = 1;
								}
								break;
							case 1: // Scanning major version
								if (isdigit(a1->value[i1])) {
									smartClientMajorVersion = (smartClientMajorVersion * 10) + (a1->value[i1++] - '0');
								}
								else {
									if (a1->value[i1] == '.') {
										i1++;
										i2 = 2;
									}
									else i2 = 9;
								}
								break;
							case 2: // Scanning minor version
								if (isdigit(a1->value[i1])) {
									smartClientMinorVersion = (smartClientMinorVersion * 10) + (a1->value[i1] - '0');
									i1++;
								}
								else i2 = 9;
								break;
							case 9: // Junk, keep scanning until over
								i1++;
								break;
							default:
								break;
							}
						}
					}
					a1 = a1->nextattribute;
				}
			}
			else matchflag = CLIENT_MATCH_GUIMSG;
			break;
		case 't':
			if (!elementptr->tag[1]) {
				element = elementptr->firstsubelement;
				if (element != NULL && element->cdataflag) {
					newtail = traptail;
					if (++newtail == sizeof(trapbuffer) / sizeof(*trapbuffer)) newtail = 0;
					if (newtail != traphead) {
						i1 = atoi(element->tag);
						if (i1 > 0 && i1 < (int) (sizeof(trapactivemap) << 3) && (trapactivemap[i1 >> 3] & (1 << (i1 & 0x07)))) {
							trapbuffer[traptail] = i1;
							traptail = newtail;
							if (trapeventid != 0) evtset(trapeventid);
						}
					}
/***	CODE: ELSE LOG ERROR MESSAGE ABOUT OVERFLOW ***/
				}
			}
			else matchflag = CLIENT_MATCH_GUIMSG;
			break;
		default:
			matchflag = CLIENT_MATCH_GUIMSG;
			break;
		}
		if (matchflag == CLIENT_MATCH_SETEVENT) {
			if (packetnum == serialnum) {
				clientflags |= CLIENT_FLAG_ELEMENT;
				if (recveventid != 0) evtset(recveventid);
			}
		}
		else if (matchflag == CLIENT_MATCH_GUIMSG) {  /* assume window device messages */
			if (guicallback != NULL) {
				guicallback(elementptr);
			}
		}
#undef CLIENT_MATCH_SETEVENT
#undef CLIENT_MATCH_GUIMSG
	}
	if (recvbufcnt > pos) {
		if (pos) memmove(recvbuffer, recvbuffer + pos, recvbufcnt -= pos);
	}
	else recvbufcnt = 0;

#if OS_WIN32
	if (clientflags & CLIENT_FLAG_ELEMENT) ResetEvent(threadevent);  /* suspend receive thread to allow processing of data */
	else SetEvent(threadevent);
#endif
#if OS_UNIX
	if (clientflags & CLIENT_FLAG_ELEMENT) evtdevset(sockethandle, 0);  /* allow processing of data before reading new data */
	else evtdevset(sockethandle, EVENTS_READ | EVENTS_ERROR);
#endif
#if OS_WIN32
	pviend();
#endif
}

static void clearbuffer()
{
	int i1;
#if OS_WIN32
	unsigned long ioctlopt;
#endif

#if OS_WIN32
	ioctlopt = 1;
	ioctlsocket(sockethandle, FIONBIO, &ioctlopt);
#else
#if defined(FIONBIO)
	i1 = 1;
	ioctl(sockethandle, (int)FIONBIO, &i1);
#else
	i1 = fcntl(sockethandle, F_GETFL, 0);
	if (i1 != -1) fcntl(sockethandle, F_SETFL, i1 | O_NONBLOCK);
#endif
#endif

	if (logfile != NULL) {
#if OS_WIN32
		pvistart();
#endif
		fputs("BAD: ", logfile);
		recvbuffer[recvbufcnt] = 0;
		fputs(recvbuffer, logfile);
	}
	for ( ; ; ) {
		i1 = tcprecv(sockethandle, (UCHAR *) recvbuffer, recvbufsize, tcpflags, 2);
		if (i1 == SOCKET_ERROR || !i1) break;
		if (logfile != NULL) {
			recvbuffer[i1] = 0;
			fputs(recvbuffer, logfile);
		}
	}
	if (logfile != NULL) {
		fputs("\n", logfile);
		fflush(logfile);
#if OS_WIN32
		pviend();
#endif
	}

#if OS_WIN32
	ioctlopt = 0;
	ioctlsocket(sockethandle, FIONBIO, &ioctlopt);
#else
#if defined(FIONBIO)
	i1 = 0;
	ioctl(sockethandle, (int)FIONBIO, &i1);
#else
	i1 = fcntl(sockethandle, F_GETFL, 0);
	if (i1 != -1) fcntl(sockethandle, F_SETFL, i1 & ~O_NONBLOCK);
#endif
#endif
	recvbufcnt = 0;
}

static INT ntoi(UCHAR *src, INT n, INT *dest)
{
	INT i1, negflg, num;

	for (i1 = -1; ++i1 < n; ) if (src[i1] != ' ') break;
	if (src[i1] == '-') {
		i1++;
		negflg = TRUE;
	}
	else negflg = FALSE;
	if (i1 >= n || !isdigit(src[i1])) return RC_ERROR;
	for (num = 0; i1 < n && isdigit(src[i1]); i1++) num = num * 10 + src[i1] - '0';
	if (i1 < n) return RC_ERROR;
	*dest = (negflg) ? -num : num;
	return 0;
}
