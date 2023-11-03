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
#define INC_TIME
#define INC_ERRNO
#define INC_IPV6
#include "includes.h"
#include "release.h"

#if OS_UNIX
#include <unistd.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
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
#include "base.h"
#include "fio.h"
#include "fsrun.h"
#include "fssql.h"
#include "fssqlx.h"
#include "fsfile.h"
#include "tcp.h"
#include "cert.h"

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long) 0xFFFFFFFF)
#endif

#if OS_WIN32
#define ERRORVALUE() WSAGetLastError()
#else
#define ERRORVALUE() errno
#endif

#define	KEEPALIVEFLAG_SERVER	1
#define	KEEPALIVEFLAG_SERVERACK	2
#define KEEPALIVEFLAG_CLIENT	3

#define KEEPALIVE_TIMEMIN	5
#define KEEPALIVE_TIMEDEF	10
#define KEEPALIVE_RETRYMIN	2
#define KEEPALIVE_RETRYDEF	6

#define MSGFUNC_CONNECT		102
#define MSGFUNC_DISCONNECT	103
#define MSGFUNC_CATALOG		104
#define MSGFUNC_EXECNONE	105
#define MSGFUNC_EXECUTE		106
#define MSGFUNC_GETROWCOUNT	107
#define MSGFUNC_GETROW		108
#define MSGFUNC_PSUPDATE	109
#define MSGFUNC_PSDELETE	110
#define MSGFUNC_DISCARD		111
#define MSGFUNC_GETATTR		112
#define MSGFUNC_OPEN		113
#define MSGFUNC_CLOSE		114
#define MSGFUNC_FPOSIT		115
#define MSGFUNC_REPOSIT		116
#define MSGFUNC_READ		117
#define MSGFUNC_WRITE		118
#define MSGFUNC_INSERT		119
#define MSGFUNC_UPDATE		120
#define MSGFUNC_DELETE		121
#define MSGFUNC_DELETEK		122
#define MSGFUNC_UNLOCK		123
#define MSGFUNC_COMPARE		124
#define MSGFUNC_FLOCK		125
#define MSGFUNC_FUNLOCK		126
#define MSGFUNC_WEOF		127
#define MSGFUNC_SIZE		128
#define MSGFUNC_RENAME		129
#define MSGFUNC_COMMAND		130

#define MSGDATASIZE ((4 * 65536) + 1024)

/* global variables */
int fsflags = 0;					/* see FSFLAGS_ in fsrun.h */

/* local variables */
static char cfgfilename[MAX_NAMESIZE];
static CHAR certificatefilename[MAX_NAMESIZE];
static SOCKET sockethandle = INVALID_SOCKET;
static INT tcpflags;
static INT connectid;
static INT sqlconnection;
static INT fileconnection;
static INT keepaliveflag;
static INT keepalivecnt;
static INT keepaliveretry;
static CHAR *logfilename = "";
static UCHAR tcpbuffer[40 + MSGDATASIZE + 1];
static int usernum;
static char msgid[8];
static int msgfunc;
static char msgfunc1;
static char msgfunc2;
static int msgcnid;
static int msgfsid;
static int msgdatasize;
static UCHAR *msgindata = tcpbuffer + 40;
static UCHAR *msgoutdata = tcpbuffer + 24;
static int rc;
static FILE *outputfile;

#if OS_WIN32
static HANDLE shutdownevent = NULL;
#endif

#if OS_UNIX
static struct sigaction oldsigterm;
static struct sigaction oldsigint;
static struct sigaction oldsighup;
static struct sigaction oldsigpipe;
#endif

static void clearbuffer(void);
static void processmsg(void);
static int putmsgerr(int);
static INT putmsgfileerr(INT errnum);
static int putmsgsqlerr(void);
static int putmsgok(int);
static int putmsgtext(char *);
static int fromcntoint(UCHAR *, int, int *);
static int fromcntolong(UCHAR *, int, long *);
static int fromcntooff(UCHAR *, int, OFFSET *);
static void parsekwval(UCHAR *, int, CHAR **, CHAR **);
static int doconnect(void);
static int dodisconnect(int);
static int docatalog(void);
static int doexecnone(void);
static int doexecute(void);
static INT dorowcount(void);
static int dogetrow(void);
static int dopsupdate(void);
static int dopsdelete(void);
static int dodiscard(void);
static int dogetinfo(void);
static int doopen(void);
static int doclose(void);
static int dofposit(void);
static int doreposit(void);
static int doread(void);
static int dowrite(void);
static int doinsert(void);
static int doupdate(void);
static int dodelete(void);
static int dodeletek(void);
static int dounlock(void);
static int docompare(void);
static int doflock(void);
static int dofunlock(void);
static int doweof(void);
static int dosize(void);
static int dorename(void);
static int docommand(void);
static void writestart(void);
static void writeout(CHAR *, INT);
static void writefinish(void);
static void debug2(char *, int);
static void debug2s(char *, char *);
static void death1(char *);
static void death2(char *, int);

#if OS_WIN32
static BOOL sigevent(DWORD);
#endif

#if OS_UNIX
static void sigevent(int);
#endif


/* start of program */
INT main(INT argc, CHAR **argv)
{
	INT i1, i2, i3, portnum, ppid, recvbufpos, sportflag, timeoutcnt;
	time_t timer1;
	CHAR work[256], *ptr;
	SOCKET workhandle;
#if OS_WIN32
	WSADATA versioninfo;
#endif
#if OS_UNIX
	struct sigaction act;
#endif

	if (argc < 5) {
		if (argc == 2 && !strcmp(argv[1], "-?")) {
			fputs("DBCFSRUN release " RELEASE, stdout);
			fputs("\n", stdout);
			exit(0);
		}
		fputs("Too few command line parameters", stdout);
		exit(1);
	}

	sportflag = FALSE;
	tcpflags = TCP_UTF8;
	outputfile = NULL;
	/* argv[1] = client address */
	/* argv[2] = client port number or dbcfsrun port number if sport */
	/* argv[3] = config file name */
	/* argv[4] = non-encrypted serverport : usernum */
	for (i1 = 4; ++i1 < argc; ) {
		if (argv[i1][0] == '-') {
			switch (toupper(argv[i1][1])) {
			case 'D':
				for (i3 = 0, i2 = 2; isdigit(argv[i1][i2]); i2++)
					i3 = i3 * 10 + argv[i1][i2] - '0';
				fsflags |= FSFLAGS_DEBUG1;
				if (i3 >= 2) fsflags |= FSFLAGS_DEBUG2;
				if (i3 >= 3) fsflags |= FSFLAGS_DEBUG3;
				if (i3 >= 4) fsflags |= FSFLAGS_DEBUG4;
				if (i3 >= 5) fsflags |= FSFLAGS_DEBUG5;
				if (i3 >= 6) fsflags |= FSFLAGS_DEBUG6;
				if (i3 >= 7) fsflags |= FSFLAGS_DEBUG7;
				if (i3 >= 8) fsflags |= FSFLAGS_DEBUG8;
				break;
#if OS_WIN32
			case 'E':
				if (!strncmp(argv[i1], "-event=", 7)) shutdownevent = (HANDLE) (ULONG_PTR) atoi(argv[i1] + 7);
				break;
#endif
			case 'L':
				if (argv[i1][2] == '=' && argv[i1][3]) logfilename = argv[i1] + 3;
				break;
			case 'S':
				if (!strcmp(argv[i1], "-sport")) sportflag = TRUE;
				else if (!strcmp(argv[i1], "-ssl")) tcpflags |= TCP_SSL;
				else if (!strncmp(argv[i1], "-stdout=", 8)) outputfile = fopen(argv[i1] + 8, "a");
				break;
			}
		}
	}
	portnum = atoi(argv[2]);
	strcpy(cfgfilename, argv[3]);
	if (outputfile == NULL) outputfile = stdout;

	/* get parent pid */
#if OS_WIN32
	GetEnvironmentVariable("_DBCFS", work, sizeof(work));
	for (ppid = 0, i1 = 8; --i1 >= 0; ) ppid = (ppid << 4) + ((UCHAR *) work)[i1] - 'A';
	if (fsflags & FSFLAGS_DEBUG3) {
		writestart();
		writeout("PPID: ", 6);
		writeout(work, 8);
		writefinish();
	}
#endif
#if OS_UNIX
	ppid = getppid();
#endif

	time(&timer1);

	/* start client port */
#if OS_WIN32
	if (WSAStartup(MAKEWORD(2,2), &versioninfo)) death2("WSAStartup failed", ERRORVALUE());
	SetConsoleCtrlHandler((PHANDLER_ROUTINE) sigevent, TRUE);
#endif

#if OS_UNIX
	act.sa_handler = sigevent;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
#ifdef SA_INTERRUPT
	act.sa_flags |= SA_INTERRUPT;
#endif
	if (sigaction(SIGTERM, &act, &oldsigterm) == -1) {
		oldsigterm.sa_handler = SIG_DFL;
		sigemptyset(&oldsigterm.sa_mask);
		oldsigterm.sa_flags = 0;
	}
	if (sigaction(SIGINT, &act, &oldsigint) == -1) {
		oldsigint.sa_handler = SIG_DFL;
		sigemptyset(&oldsigint.sa_mask);
		oldsigint.sa_flags = 0;
	}
	if (sigaction(SIGHUP, NULL, &oldsighup) == -1) {
		oldsighup.sa_handler = SIG_DFL;
		sigemptyset(&oldsighup.sa_mask);
		oldsighup.sa_flags = 0;
	}
	if (oldsighup.sa_handler != SIG_IGN) sigaction(SIGHUP, &act, NULL);
	if (sigaction(SIGPIPE, &act, &oldsigpipe) == -1) {
		oldsigpipe.sa_handler = SIG_DFL;
		sigemptyset(&oldsigpipe.sa_mask);
		oldsigpipe.sa_flags = 0;
	}
#endif

	if (sportflag) {  /* begin listening for connection from client */
		workhandle = tcplisten(portnum, &portnum);
		if (workhandle == INVALID_SOCKET) {
			strcpy(work, "tcplisten() failed: ");
			strcat(work, tcpgeterror());
			death1(work);
		}
	}

	/* connect to server for verification string */
	for (i1 = i2 = 0; isdigit(argv[4][i1]); i1++) i2 = i2 * 10 + argv[4][i1] - '0';
	usernum = 0;
	if (argv[4][i1] == ':')
		for (i1++; isdigit(argv[4][i1]); i1++) usernum = usernum * 10 + argv[4][i1] - '0';
	sockethandle = tcpconnect("127.0.0.1", i2, TCP_UTF8, NULL, 20);
	if (sockethandle == INVALID_SOCKET) {
		strcpy(work, "tcpconnect() failed: ");
		strcat(work, tcpgeterror());
		if (sportflag) closesocket(workhandle);
		death1(work);
	}

	memset(tcpbuffer, ' ', 40);
	tcpiton(usernum, tcpbuffer + 8, 8);
	memcpy(tcpbuffer + 24, "VERIFY  ", 8);
	tcpbuffer[39] = '0';
	if ((i1 = tcpsend(sockethandle, tcpbuffer, 40, TCP_UTF8, 30)) != 40) {
		closesocket(sockethandle);
		if (sportflag) closesocket(workhandle);
		death2("unable to communicate to server(A)", i1);
	}
	for (recvbufpos = 0; ; ) {
		i1 = tcprecv(sockethandle, tcpbuffer + recvbufpos, (sizeof(tcpbuffer) - 1) - recvbufpos, TCP_UTF8, 30);
		if (i1 <= 0) {
			if (sportflag) closesocket(workhandle);
			if (i1 == 0 || i1 == -1) {
				ptr = tcpgeterror();
				sprintf(work, "unable to communicate to server(B) tcperror='%s'", ptr);
				death1(work);
			}
			else {
				death2("unable to communicate to server(B)", i1);
			}
		}
		recvbufpos += i1;
		if (recvbufpos < 24) continue;
		tcpntoi(tcpbuffer + 16, 8, &msgdatasize);
		if (msgdatasize != 32) death2("unable to communicate to server(C)", msgdatasize);
		if (recvbufpos >= 24 + msgdatasize) {
			i1 = tcpbuffer[24 + 1];
			i1 += (INT) tcpbuffer[24 + 6] << 8;
			i1 += (INT) tcpbuffer[24 + 2] << 16;
			i1 += (INT) tcpbuffer[24 + 4] << 24;
			if (i1 != ppid) {
#ifndef _DEBUG
				if (sportflag) closesocket(workhandle);
				sprintf(work, "unable to communicate to server(D) i1=%i, ppid=%i", i1, ppid);
				death1(work);
#endif
			}
			i1 = tcpbuffer[24 + 23];
			i1 += (INT) tcpbuffer[24 + 18] << 8;
			i1 += (INT) tcpbuffer[24 + 21] << 16;
			i1 += (INT) tcpbuffer[24 + 22] << 24;
#if OS_WIN32

			if (i1 != (INT) GetCurrentProcessId()) {
#ifndef _DEBUG
				if (sportflag) closesocket(workhandle);
				if (fsflags & FSFLAGS_DEBUG2) {
					CHAR work1[128];
					tcpbuffer[24 + 32 + 1] = '\0';
					debug1(tcpbuffer);
					sprintf(work1, "E-stage fail -> (04) %#2.2x (02) %#2.2x (06) %#2.2x (01) %#2.2x "
							"(22) %#2.2x (21) %#2.2x (18) %#2.2x (23) %#2.2x ",
							tcpbuffer[24 + 4], tcpbuffer[24 + 2], tcpbuffer[24 + 6], tcpbuffer[24 + 1],
							tcpbuffer[24 + 22], tcpbuffer[24 + 21], tcpbuffer[24 + 18], tcpbuffer[24 + 23]
							);
				}
				sprintf(work, "unable to communicate to server(E) i1=%i, GetCurrentProcessId()=%i", i1, (INT) GetCurrentProcessId());
				death1(work);
#endif
			}
			else {
				if (fsflags & FSFLAGS_DEBUG2) {
					CHAR work1[128];
					tcpbuffer[24 + 32 + 1] = '\0';
					debug1(tcpbuffer);
					sprintf(work1, "E-stage success -> (04) %#2.2x (02) %#2.2x (06) %#2.2x (01) %#2.2x "
							"(22) %#2.2x (21) %#2.2x (18) %#2.2x (23) %#2.2x "
							", GetCurrentProcessId()=%lu"
							,tcpbuffer[24 + 4], tcpbuffer[24 + 2], tcpbuffer[24 + 6], tcpbuffer[24 + 1],
							tcpbuffer[24 + 22], tcpbuffer[24 + 21], tcpbuffer[24 + 18], tcpbuffer[24 + 23],
							GetCurrentProcessId());
					debug1(work1);
				}
			}
#endif
#if OS_UNIX
			if (i1 != getpid()) {
				if (sportflag) closesocket(workhandle);
				sprintf(work, "unable to communicate to server(E) i1=%i, getpid()=%i", i1, (INT) getpid());
				death1(work);
			}
#endif
			i1 = tcpbuffer[24 + 24];
			i1 += (INT) tcpbuffer[24 + 29] << 8;
			i1 += (INT) tcpbuffer[24 + 30] << 16;
			i1 += (INT) tcpbuffer[24 + 27] << 24;
#ifndef _DEBUG
			if ((time_t) i1 < timer1 || (time_t) i1 > time(NULL)) {
				if (sportflag) closesocket(workhandle);
				death2("unable to communicate to server(F)", i1);
			}
#endif
			break;
		}
	}
	closesocket(sockethandle);

	if ((tcpflags & TCP_SSL) && cfggetentry(cfgfilename, "certificatefilename", certificatefilename, sizeof(certificatefilename)) > 0) {
		if (sportflag) sockethandle = tcpaccept(workhandle, tcpflags | TCP_SERV, GetCertBio(certificatefilename), 60);
		else sockethandle = tcpconnect(argv[1], portnum, tcpflags | TCP_SERV, GetCertBio(certificatefilename), 60);
	}
	else {
		if (sportflag) sockethandle = tcpaccept(workhandle, tcpflags | TCP_SERV, NULL, 60);
		else sockethandle = tcpconnect(argv[1], portnum, tcpflags | TCP_SERV, NULL, 60);
	}
	if (sockethandle == INVALID_SOCKET) {
		if (sportflag) strcpy(work, "connection (tcpaccept) from client failed: ");
		else  sprintf(work, "connection (tcpconnect %s:%d) to client failed: ", argv[1], portnum);
		strcat(work, tcpgeterror());
		death1(work);
	}
	i1 = 1;
	setsockopt(sockethandle, SOL_SOCKET, SO_KEEPALIVE, (char *) &i1, sizeof(i1));

	connectid = recvbufpos = timeoutcnt = 0;
	while (!(fsflags & FSFLAGS_SHUTDOWN)) {
#if OS_WIN32
		if (shutdownevent != NULL && WaitForSingleObject(shutdownevent, 0) == WAIT_OBJECT_0) {
			fsflags |= FSFLAGS_SHUTDOWN;
			break;
		}
#endif
		i1 = tcprecv(sockethandle, tcpbuffer + recvbufpos, 40 - recvbufpos, tcpflags, KEEPALIVE_TIMEMIN);
		if (i1 <= 0) {
			if (i1 < 0) {
				if (fsflags & FSFLAGS_DEBUG2) {
					if (i1 < 0) {
						debug2("connection socket error(A)", ERRORVALUE());
						debug2(tcpgeterror(), 0);
					}
					else debug1("incomplete request packet");
				}
				break;
			}
			/* timeout */
			timeoutcnt++;
			if (!connectid) {
				if (timeoutcnt * KEEPALIVE_TIMEMIN >= 60) break;
			}
			else if (keepaliveflag && !(timeoutcnt % keepalivecnt)) {
				if (timeoutcnt >= keepalivecnt * keepaliveretry) {
					if (fsflags & FSFLAGS_DEBUG2) debug1("keepalive timeout");
					break;
				}
				if (keepaliveflag != KEEPALIVEFLAG_CLIENT) {
					// 26MAY2020 This should never happen
					if (keepaliveflag == KEEPALIVEFLAG_SERVERACK) {
						timeoutcnt = 0;
						ptr = "ALIVEACK";
					}
					else ptr = "ALIVECHK";
					if (tcpsend(sockethandle, (UCHAR *) ptr, 8, tcpflags, 600) != 8) {
						if (fsflags & FSFLAGS_DEBUG2) debug1("keepalive request failed");
						break;
					}
					if (fsflags & FSFLAGS_DEBUG8) {
						writestart();
						writeout("SND    8", 8);
						writeout(ptr, 8);
						writefinish();
					}
				}
			}
			continue;
		}
		recvbufpos += i1;
		if (recvbufpos >= 8) {  /* check for keep alive check or acknowledgement */
			if (!memcmp(tcpbuffer, "ALIVE", 5)) {
				if (!memcmp(tcpbuffer + 5, "CHK", 3)) {
					if (fsflags & FSFLAGS_DEBUG8) {
						writestart();
						writeout("RCV    8ALIVECHK", 16);
						writefinish();
					}
					i1 = 0;
				}
				else if (!memcmp(tcpbuffer + 5, "ACK", 3)) {
					if (fsflags & FSFLAGS_DEBUG8) {
						writestart();
						writeout("RCV    8ALIVEACK", 16);
						writefinish();
					}
					i1 = 1;
				}
				else i1 = 2;
				if (i1 != 2) {
					timeoutcnt = 0;
					if (!i1) {
						if (tcpsend(sockethandle, (UCHAR *) "ALIVEACK", 8, tcpflags, 30) != 8) {
							if (fsflags & FSFLAGS_DEBUG2) debug1("keepalive request failed");
							break;
						}
						if (fsflags & FSFLAGS_DEBUG8) {
							writestart();
							writeout("SND    8ALIVEACK", 16);
							writefinish();
						}
					}
					if (recvbufpos -= 8) memmove(tcpbuffer, tcpbuffer + 8, recvbufpos);
				}
			}
		}
		if (recvbufpos < 40) continue;  /* continue getting the header */
		timeoutcnt = 0;
		tcpntoi(tcpbuffer + 32, 8, &msgdatasize);
		if (msgdatasize > MSGDATASIZE) {
			clearbuffer();
			if (fsflags & FSFLAGS_DEBUG1) debug1("ERR: msg too big");
			putmsgerr(ERR_MSGHDRDATASIZE);
			recvbufpos = 0;
			continue;
		}
		while (recvbufpos < 40 + msgdatasize) {
			i1 = tcprecv(sockethandle, tcpbuffer + recvbufpos, (40 + msgdatasize) - recvbufpos, tcpflags, 30);
			if (i1 <= 0) {
				if (i1 < 0) {
					if (fsflags & FSFLAGS_DEBUG2) {
						debug2("connection socket error(B)", ERRORVALUE());
						debug2(tcpgeterror(), 0);
					}
					fsflags |= FSFLAGS_SHUTDOWN;
				}
				else if (recvbufpos) {
					if (fsflags & FSFLAGS_DEBUG1) debug1("ERR: msg incomplete");
					putmsgerr(ERR_MSGHDRDATASIZE);
				}
				recvbufpos = 0;
				break;
			}
			recvbufpos += i1;
		}
		if (!recvbufpos) continue;
		if (fsflags & FSFLAGS_DEBUG2) {
			for (i1 = 40; i1 < recvbufpos && ((tcpbuffer[i1] >= ' ' && tcpbuffer[i1] <= '~') || tcpbuffer[i1] == '\r' || tcpbuffer[i1] == '\n' || tcpbuffer[i1] == '\t'); i1++);
			if (i1 == recvbufpos) {
				ptr = (CHAR *)(tcpbuffer + 40);
				i1 = recvbufpos - 40;
				if (i1 > 8192) i1 = 8192;
			}
			else {
				ptr = "<BINARY/INTERNATIONAL DATA>";
				i1 = (INT)strlen(ptr);
			}
			msciton(40 + i1, (UCHAR *) work, 8);
			memcpy(work, "RCV", 3);
			memcpy(work + 8, tcpbuffer + 8, 32);
			writestart();
			writeout(work, 40);
			if (i1) {
				writeout(ptr, (i1 < 8192) ? i1 : 8188);
				if (i1 >= 8192) writeout(" ...", 4);
			}
			writefinish();
		}
		processmsg();
		recvbufpos = 0;
	}
	if (sqlconnection || fileconnection) dodisconnect(FALSE);
	if (tcpflags & TCP_SSL) tcpsslcomplete(sockethandle);
	closesocket(sockethandle);
#if OS_WIN32
	WSACleanup();
	SetConsoleCtrlHandler((PHANDLER_ROUTINE) sigevent, FALSE);
#endif

#if OS_UNIX
	sigaction(SIGTERM, &oldsigterm, NULL);
	sigaction(SIGINT, &oldsigint, NULL);
	if (oldsighup.sa_handler != SIG_IGN) sigaction(SIGHUP, &oldsighup, NULL);
	sigaction(SIGPIPE, &oldsigpipe, NULL);
#endif
	return 0;
}

static void clearbuffer()
{
	int i1;

	do i1 = tcprecv(sockethandle, tcpbuffer, sizeof(tcpbuffer), tcpflags & ~TCP_UTF8, 3);
	while (i1 > 0);
}

void debug1(char *msg)
{
	char hdr[8];

	writestart();
	msciton((INT)strlen(msg) + 8, (UCHAR *) hdr, 8);
	memcpy(hdr, "DBG", 3);
	writeout(hdr, 8);
	writeout(msg, (INT)strlen(msg));
	writefinish();
}

static void debug2(char *msg, int num)
{
	char work[200];

	sprintf(work, "%s: %d", msg, num);
	debug1(work);
}

static void debug2s(char *msg, char *msg2)
{
	int i1, i2;
	char work[4096];

	strcpy(work, msg);
	strcat(work, ": ");
	i1 = (INT)strlen(work);
	i2 = (INT)strlen(msg2);
	if (i2 > (int) sizeof(work) - 1 - i1) i2 = sizeof(work) - 1 - i1;
	memcpy(&work[i1], msg2, i2);
	work[i1 + i2] = 0;
	debug1(work);
}

/**
 * Does NOT return
 */
static void death1(char *msg)
{
	debug2s("FATAL ERROR", msg);
#if OS_WIN32
	WSACleanup();
#endif
	exit(1);
}

static void death2(char *msg, int num)
{
	char work[512];

	sprintf(work, "%s: %d", msg, num);
	death1(work);
}

/* process a client message */
static void processmsg(void)
{
	memcpy(msgid, tcpbuffer, 8);
	rc = tcpntoi(tcpbuffer + 8, 8, &msgcnid);
	if (rc || msgcnid < 0) {
		putmsgerr(ERR_BADINTVALUE);
		return;
	}
	rc = tcpntoi(tcpbuffer + 16, 8, &msgfsid);
	if (rc || msgfsid < 0) {
		putmsgerr(ERR_BADINTVALUE);
		return;
	}

	msgfunc = 0;
	switch (tcpbuffer[24]) {
	case 'C':
		if (!memcmp(tcpbuffer + 24, "CATALOG ", 8)) msgfunc = MSGFUNC_CATALOG;
		else if (!memcmp(tcpbuffer + 24, "CLOSE", 5)) {
			msgfunc = MSGFUNC_CLOSE;
			msgfunc1 = tcpbuffer[29];
		}
		else if (!memcmp(tcpbuffer + 24, "COMMAND ", 8)) msgfunc = MSGFUNC_COMMAND;
		else if (!memcmp(tcpbuffer + 24, "COMPARE ", 8)) msgfunc = MSGFUNC_COMPARE;
		else if (!memcmp(tcpbuffer + 24, "CONNECT ", 8)) msgfunc = MSGFUNC_CONNECT;
		break;
	case 'D':
		if (!memcmp(tcpbuffer + 24, "DISCARD ", 8)) msgfunc = MSGFUNC_DISCARD;
		else if (!memcmp(tcpbuffer + 24, "DELETE  ", 8)) msgfunc = MSGFUNC_DELETE;
		else if (!memcmp(tcpbuffer + 24, "DELETEK ", 8)) msgfunc = MSGFUNC_DELETEK;
		else if (!memcmp(tcpbuffer + 24, "DISCNCT ", 8)) msgfunc = MSGFUNC_DISCONNECT;
		break;
	case 'E':
		if (!memcmp(tcpbuffer + 24, "EXECUTE ", 8)) msgfunc = MSGFUNC_EXECUTE;
		else if (!memcmp(tcpbuffer + 24, "EXECNONE", 8)) msgfunc = MSGFUNC_EXECNONE;
		break;
	case 'F':
		if (!memcmp(tcpbuffer + 24, "FLOCK   ", 8)) msgfunc = MSGFUNC_FLOCK;
		else if (!memcmp(tcpbuffer + 24, "FUNLOCK ", 8)) msgfunc = MSGFUNC_FUNLOCK;
		else if (!memcmp(tcpbuffer + 24, "FPOSIT  ", 8)) msgfunc = MSGFUNC_FPOSIT;
		break;
	case 'G':
		if (!memcmp(tcpbuffer + 24, "GETROWCT", 8)) msgfunc = MSGFUNC_GETROWCOUNT;
		else if (!memcmp(tcpbuffer + 24, "GETROW", 6)) {
			msgfunc = MSGFUNC_GETROW;
			msgfunc1 = tcpbuffer[30];
		}
/*** CODE: REMOVE WHEN DRIVER USES "CATALOG" ***/
		else if (!memcmp(tcpbuffer + 24, "GETATTR ", 8)) msgfunc = MSGFUNC_CATALOG;
		break;
	case 'I':
		if (!memcmp(tcpbuffer + 24, "INSERT  ", 8)) msgfunc = MSGFUNC_INSERT;
		break;
	case 'O':
		if (!memcmp(tcpbuffer + 24, "OPEN", 4)) {
			msgfunc = MSGFUNC_OPEN;
			msgfunc1 = tcpbuffer[28];
			msgfunc2 = tcpbuffer[29];
		}
		break;
	case 'P':
		if (!memcmp(tcpbuffer + 24, "PSUPDATE", 8)) msgfunc = MSGFUNC_PSUPDATE;
		else if (!memcmp(tcpbuffer + 24, "PSDELETE", 8)) msgfunc = MSGFUNC_PSDELETE;
		break;
	case 'R':
		if (!memcmp(tcpbuffer + 24, "READ", 4)) {
			msgfunc = MSGFUNC_READ;
			msgfunc1 = tcpbuffer[28];
			msgfunc2 = tcpbuffer[29];
		}
		else if (!memcmp(tcpbuffer + 24, "RENAME  ", 8)) msgfunc = MSGFUNC_RENAME;
		else if (!memcmp(tcpbuffer + 24, "REPOSIT ", 8)) msgfunc = MSGFUNC_REPOSIT;
		break;
	case 'S':
		if (!memcmp(tcpbuffer + 24, "SIZE    ", 8)) msgfunc = MSGFUNC_SIZE;
		break;
		break;
	case 'U':
		if (!memcmp(tcpbuffer + 24, "UPDATE  ", 8)) msgfunc = MSGFUNC_UPDATE;
		else if (!memcmp(tcpbuffer + 24, "UNLOCK  ", 8)) msgfunc = MSGFUNC_UNLOCK;
		break;
	case 'W':
		if (!memcmp(tcpbuffer + 24, "WRITE", 5)) {
			msgfunc = MSGFUNC_WRITE;
			msgfunc1 = tcpbuffer[29];
		}
		else if (!memcmp(tcpbuffer + 24, "WEOF    ", 8)) msgfunc = MSGFUNC_WEOF;
		break;
	}
	if (!msgfunc) {
		putmsgerr(ERR_BADFUNC);
		return;
	}

	if (msgfunc == MSGFUNC_CONNECT) {
		if (sqlconnection || fileconnection) {
			putmsgerr(ERR_ALREADYCONNECTED);
			return;
		}
		doconnect();
		return;
	}
	if (!sqlconnection && !fileconnection) {
		putmsgerr(ERR_NOTCONNECTED);
		return;
	}
	if (sqlconnection) {
		sqlmsgclear();
		switch (msgfunc) {
		case MSGFUNC_DISCONNECT:
			rc = dodisconnect(TRUE);
			break;
		case MSGFUNC_CATALOG:
			rc = docatalog();
			break;
		case MSGFUNC_EXECNONE:
			rc = doexecnone();
			break;
		case MSGFUNC_EXECUTE:
			rc = doexecute();
			break;
		case MSGFUNC_GETROWCOUNT:
			rc = dorowcount();
			break;
		case MSGFUNC_GETROW:
			rc = dogetrow();
			break;
		case MSGFUNC_PSUPDATE:
			rc = dopsupdate();
			break;
		case MSGFUNC_PSDELETE:
			rc = dopsdelete();
			break;
		case MSGFUNC_DISCARD:
			rc = dodiscard();
			break;
		default:
			rc = sqlerrnummsg(ERR_BADFUNC, "not a recognized SQL function", NULL);
		}
		if (rc) putmsgsqlerr();
	}
	else {
		switch (msgfunc) {
		case MSGFUNC_DISCONNECT:
			rc = dodisconnect(TRUE);
			break;
		case MSGFUNC_GETATTR:
			rc = dogetinfo();
			break;
		case MSGFUNC_OPEN:
			rc = doopen();
			break;
		case MSGFUNC_CLOSE:
			rc = doclose();
			break;
		case MSGFUNC_FPOSIT:
			rc = dofposit();
			break;
		case MSGFUNC_REPOSIT:
			rc = doreposit();
			break;
		case MSGFUNC_READ:
			rc = doread();
			break;
		case MSGFUNC_WRITE:
			rc = dowrite();
			break;
		case MSGFUNC_INSERT:
			rc = doinsert();
			break;
		case MSGFUNC_UPDATE:
			rc = doupdate();
			break;
		case MSGFUNC_DELETE:
			rc = dodelete();
			break;
		case MSGFUNC_DELETEK:
			rc = dodeletek();
			break;
		case MSGFUNC_UNLOCK:
			rc = dounlock();
			break;
		case MSGFUNC_COMPARE:
			rc = docompare();
			break;
		case MSGFUNC_FLOCK:
			rc = doflock();
			break;
		case MSGFUNC_FUNLOCK:
			rc = dofunlock();
			break;
		case MSGFUNC_WEOF:
			rc = doweof();
			break;
		case MSGFUNC_SIZE:
			rc = dosize();
			break;
		case MSGFUNC_RENAME:
			rc = dorename();
			break;
		case MSGFUNC_COMMAND:
			rc = docommand();
			break;
		default:
			rc = ERR_BADFUNC;
		}
		if (rc) {
			if (rc == ERR_BADFUNC) putmsgerr(rc);
			else putmsgfileerr(rc);
		}
	}
}

static int putmsgerr(int errnum)
{
	char sendmsghdr[24];

	errnum = 0 - errnum;
	memcpy(sendmsghdr, msgid, 8);
	msciton(errnum, (UCHAR *)(sendmsghdr + 8), 8);
	memcpy(&sendmsghdr[8], "ERR", 3);
	memcpy(&sendmsghdr[16], "       0", 8);
	if (tcpsend(sockethandle, (UCHAR *) sendmsghdr, 24, tcpflags, 30) != 24) {
		fsflags |= FSFLAGS_SHUTDOWN;
	}
	if (fsflags & FSFLAGS_DEBUG1) {
		memcpy(sendmsghdr, "SND   24", 8);
		writestart();
		writeout(sendmsghdr, 24);
		writefinish();
	}
	return 0;
}

static INT putmsgfileerr(INT errnum)
{
	int errmsglen;
	char sendmsghdr[24], *errmsg;

	errnum = 0 - errnum;
	memcpy(sendmsghdr, msgid, 8);
	msciton(errnum, (UCHAR *)(sendmsghdr + 8), 8);
	memcpy(&sendmsghdr[8], "ERR", 3);
	errmsg = filemsg();
	errmsglen = 0;
	if (errmsg != NULL) errmsglen = (INT)strlen(errmsg);
	msciton(errmsglen, (UCHAR *)(sendmsghdr + 16), 8);
	if (tcpsend(sockethandle, (UCHAR *) sendmsghdr, 24, tcpflags, 30) != 24) {
		fsflags |= FSFLAGS_SHUTDOWN;
	}
	if (errmsglen && tcpsend(sockethandle, (UCHAR *) errmsg, errmsglen, tcpflags, 30) != errmsglen) {
		fsflags |= FSFLAGS_SHUTDOWN;
	}
	if (fsflags & FSFLAGS_DEBUG1) {
		msciton(errmsglen + 24, (UCHAR *) sendmsghdr, 8);
		memcpy(sendmsghdr, "SND", 3);
		writestart();
		writeout(sendmsghdr, 24);
		if (errmsglen) writeout(errmsg, errmsglen);
		writefinish();
	}
	return 0;
}

static int putmsgsqlerr()
{
	int errmsglen;
	char sendmsghdr[24], *errmsg;

	memcpy(sendmsghdr, msgid, 8);
/*	msciton(999, &sendmsghdr[8], 8); */
	memcpy(&sendmsghdr[8], "ERR", 3);
	memcpy(&sendmsghdr[11], sqlcode(), 5);
	errmsg = sqlmsg();
	errmsglen = 0;
	if (errmsg != NULL) errmsglen = (INT)strlen(errmsg);
	msciton(errmsglen, (UCHAR *)(sendmsghdr + 16), 8);
	if (tcpsend(sockethandle, (UCHAR *) sendmsghdr, 24, tcpflags, 30) != 24) {
		fsflags |= FSFLAGS_SHUTDOWN;
	}
	if (errmsglen && tcpsend(sockethandle, (UCHAR *) errmsg, errmsglen, tcpflags, 30) != errmsglen) {
		fsflags |= FSFLAGS_SHUTDOWN;
	}
	if (fsflags & FSFLAGS_DEBUG1) {
		msciton(errmsglen + 24, (UCHAR *) sendmsghdr, 8);
		memcpy(sendmsghdr, "SND", 3);
		writestart();
		writeout(sendmsghdr, 24);
		if (errmsglen) writeout(errmsg, errmsglen);
		writefinish();
	}
	return 0;
}

static int putmsgok(int timeout)
{
	int i1;
	char *ptr;

	/* msgid already in first 8 bytes of tcpbuffer */
	memcpy(tcpbuffer + 8, "OK      ", 8);
	msciton(msgdatasize, tcpbuffer + 16, 8);
	if ((i1 = tcpsend(sockethandle, tcpbuffer, 24 + msgdatasize, tcpflags, timeout)) != 24 + msgdatasize) {
		fsflags |= FSFLAGS_SHUTDOWN;
		if (fsflags & FSFLAGS_DEBUG2) debug1(tcpgeterror());
	}
	if (fsflags & FSFLAGS_DEBUG2) {
		for (i1 = 0; i1 < msgdatasize && ((msgoutdata[i1] >= ' ' && msgoutdata[i1] <= '~') || msgoutdata[i1] == '\r' || msgoutdata[i1] == '\n' || msgoutdata[i1] == '\t'); i1++);
		if (i1 == msgdatasize) {
			ptr = (CHAR *) msgoutdata;
			i1 = msgdatasize;
			if (i1 > 8192) i1 = 8192;
		}
		else {
			ptr = "<BINARY/INTERNATIONAL DATA>";
			i1 = (INT)strlen(ptr);
		}
		msciton(24 + i1, (UCHAR *) tcpbuffer, 8);
		memcpy(tcpbuffer, "SND", 3);
		writestart();
		writeout((CHAR *) tcpbuffer, 24);
		if (i1) {
			writeout(ptr, (i1 < 8192) ? i1 : 8188);
			if (i1 >= 8192) writeout(" ...", 4);
		}
		writefinish();
	}
	return 0;
}

static int putmsgtext(char *msg)
{
	int i1;
	char *ptr;
	/* msgid already in first 8 bytes of tcpbuffer */
	memcpy(tcpbuffer + 8, msg, 8);
	msciton(msgdatasize, tcpbuffer + 16, 8);
	if ((i1 = tcpsend(sockethandle, tcpbuffer, 24 + msgdatasize, tcpflags, 30)) != 24 + msgdatasize) {
		fsflags |= FSFLAGS_SHUTDOWN;
		if (fsflags & FSFLAGS_DEBUG2) debug1(tcpgeterror());
	}
	if (fsflags & FSFLAGS_DEBUG2) {
		for (i1 = 0; i1 < msgdatasize && ((msgoutdata[i1] >= ' ' && msgoutdata[i1] <= '~') || msgoutdata[i1] == '\r' || msgoutdata[i1] == '\n' || msgoutdata[i1] == '\t'); i1++);
		if (i1 == msgdatasize) {
			ptr = (CHAR *) msgoutdata;
			i1 = msgdatasize;
			if (i1 > 8192) i1 = 8192;
		}
		else {
			ptr = "<BINARY/INTERNATIONAL DATA>";
			i1 = (INT)strlen(ptr);
		}
		msciton(24 + i1, tcpbuffer, 8);
		memcpy(tcpbuffer, "SND", 3);
		writestart();
		writeout((CHAR *) tcpbuffer, 24);
		if (i1) {
			writeout(ptr, (i1 < 8192) ? i1 : 8188);
			if (i1 >= 8192) writeout(" ...", 4);
		}
		writefinish();
	}
	return 0;
}

/**
 * Read ascii digits starting at src for up to srclen bytes.
 * Convert to a binary integer and return in value.
 * A leading negative sign is allowed
 */
static int fromcntoint(UCHAR *src, int srclen, int *value)
{
	int i1, i2, negflg;

	negflg = FALSE;
	rc = 0;
	for (i1 = i2 = 0; i1 < srclen; i1++) {
		if (src[i1] >= '0' && src[i1] <= '9') i2 = i2 * 10 + src[i1] - '0';
		else if (src[i1] != ' ') {
			if (src[i1] != '-' || negflg || i2) rc = ERR_BADINTVALUE;
			negflg = TRUE;
		}
	}
	if (negflg) i2 = -i2;
	*value = i2;
	return rc;
}

static int fromcntolong(UCHAR *src, int srclen, long *value)
{
	int i1, negflg;
	long l1;

	negflg = FALSE;
	rc = 0;
	for (i1 = 0, l1 = 0; i1 < srclen; i1++) {
		if (src[i1] >= '0' && src[i1] <= '9') l1 = l1 * 10 + src[i1] - '0';
		else if (src[i1] != ' ') {
			if (src[i1] != '-' || negflg || l1) rc = ERR_BADINTVALUE;
			negflg = TRUE;
		}
	}
	if (negflg) l1 = -l1;
	*value = l1;
	return rc;
}

static int fromcntooff(UCHAR *src, int srclen, OFFSET *value)
{
	int i1, negflg;
	OFFSET l1;

	negflg = FALSE;
	rc = 0;
	for (i1 = 0, l1 = 0; i1 < srclen; i1++) {
		if (src[i1] >= '0' && src[i1] <= '9') l1 = l1 * 10 + src[i1] - '0';
		else if (src[i1] != ' ') {
			if (src[i1] != '-' || negflg || l1) rc = ERR_BADINTVALUE;
			negflg = TRUE;
		}
	}
	if (negflg) l1 = -l1;
	*value = l1;
	return rc;
}

static void parsekwval(UCHAR *string, INT stringlen, CHAR **kw, CHAR **val)
{
	INT i1;

	for (*kw = (CHAR *) string, i1 = 0; i1 < stringlen && string[i1] != '='; i1++)
		string[i1] = (UCHAR) toupper(string[i1]);
	if (i1 < stringlen) {  /* keyword with value */
		string[i1++] = '\0';  /* terminate key word */
		*val = (CHAR *)(string + i1);
	}
	else *val = "";
	string[stringlen] = '\0';  /* terminate key word or value */
}

static int doconnect()
{
	INT i1, badargsflag, nextoffset, offset;
	CHAR dbdfile[256], keepalive[32], password[128], type[32], user[128];

	sqlconnection = fileconnection = FALSE;

	badargsflag = TRUE;
	offset = 0;
	i1 = tcpnextdata(msgindata, msgdatasize, &offset, &nextoffset);
	if (i1 > 0) {
		memcpy(user, msgindata + offset, i1);
		user[i1] = '\0';
		offset = nextoffset;
		i1 = tcpnextdata(msgindata, msgdatasize, &offset, &nextoffset);
		if (i1 > 0) {
			memcpy(password, msgindata + offset, i1);
			password[i1] = '\0';
			offset = nextoffset;
			i1 = tcpnextdata(msgindata, msgdatasize, &offset, &nextoffset);
			if (i1 > 0) {
				memcpy(type, msgindata + offset, i1);
				type[i1] = '\0';
				offset = nextoffset;
				i1 = tcpnextdata(msgindata, msgdatasize, &offset, &nextoffset);
				if (i1 > 0) {
					memcpy(dbdfile, msgindata + offset, i1);
					dbdfile[i1] = '\0';
					badargsflag = FALSE;
					/* check for optional keepalive */
					keepaliveflag = 0;
					offset = nextoffset;
					i1 = tcpnextdata(msgindata, msgdatasize, &offset, &nextoffset);
					if (i1 > 0) {
						memcpy(keepalive, msgindata + offset, i1);
						keepalive[i1] = '\0';
						if (!strcmp(keepalive, "CLIENT")) {
							// This seems to be the only one we have happening now 26MAY2020
							keepaliveflag = KEEPALIVEFLAG_CLIENT;
						}
						else if (!strcmp(keepalive, "SERVER")) keepaliveflag = KEEPALIVEFLAG_SERVER;
						else if (!strcmp(keepalive, "SERVERACK")) keepaliveflag = KEEPALIVEFLAG_SERVERACK;
						else if (i1) badargsflag = TRUE;
					}
				}
			}
		}
	}
	if (badargsflag) {
		putmsgerr(ERR_INVALIDKEYWORD);
		return -1;
	}

	if (!strcmp(type, "DATABASE")) {
		sqlmsgclear();
		connectid = sqlconnect(cfgfilename, dbdfile, user, password, logfilename);
		if (connectid <= 0) {
			putmsgsqlerr();
			return -1;
		}
		sqlconnection = TRUE;
	}
	else if (!strcmp(type, "FILE")) {
		connectid = fileconnect(cfgfilename, dbdfile, user, password, logfilename);
		if (connectid <= 0) {
			putmsgfileerr(ERR_INITFAILED);
			return -1;
		}
		fileconnection = TRUE;
	}
	else {
		putmsgerr(ERR_INVALIDKEYWORD);
		return -1;
	}
	msciton(connectid, msgoutdata, 8);
	msgdatasize = 8;
	if (keepaliveflag) {
		if (cfggetentry(cfgfilename, "checktime", keepalive, sizeof(keepalive)) > 0) {
			keepalivecnt = atoi(keepalive);
			if (keepalivecnt && keepalivecnt <= KEEPALIVE_TIMEMIN) keepalivecnt = KEEPALIVE_TIMEMIN;
		}
		else keepalivecnt = KEEPALIVE_TIMEDEF;
		if (keepalivecnt) {
			if (keepalivecnt % KEEPALIVE_TIMEMIN) keepalivecnt += KEEPALIVE_TIMEMIN;
			keepalivecnt /= KEEPALIVE_TIMEMIN;
			if (cfggetentry(cfgfilename, "checkretry", keepalive, sizeof(keepalive)) > 0) {
				keepaliveretry = atoi(keepalive);
				if (keepaliveretry <= KEEPALIVE_RETRYMIN) keepaliveretry = KEEPALIVE_RETRYMIN;
			}
			else keepaliveretry = KEEPALIVE_RETRYDEF;
		}
		else keepaliveflag = 0;
		msciton(keepalivecnt * KEEPALIVE_TIMEMIN, msgoutdata + msgdatasize, 8);
		msgdatasize += 8;
	}
	putmsgok(30);
#if 0
/*** CODE: IF DEBUG PUT OUT CONNECTION ID ***/
	if (connectid > 0) {
		memcpy(controlbuffer, "CONNECT ", 8);
		i1 = 8;
	}
	else {
		i1 = strlen(&controlbuffer[8]) + 8;
		msciton(i1, controlbuffer, 8);
		memcpy(controlbuffer, "CER", 3);
	}
	writestart();
	writeout(controlbuffer, i1);
	writefinish();
#endif
	return 0;
}

static int dodisconnect(int replyflag)
{
	if (sqlconnection) {
		sqlmsgclear();
		sqldisconnect(connectid);
		sqlconnection = FALSE;
	}
	else if (fileconnection) {
		filedisconnect(connectid);
		fileconnection = FALSE;
	}
	connectid = 0;
	if (replyflag) {
		msgdatasize = 0;
		putmsgok(30);
	}
	if (fsflags & FSFLAGS_DEBUG1) {
		writestart();
		writeout("DISCNCT ", 8);
		writefinish();
	}
	fsflags |= FSFLAGS_SHUTDOWN;
	return 0;
}

static int docatalog(void)
{
	int size;

	if (!msgdatasize || msgindata[0] == ' ') {
		sqlerrnummsg(ERR_INVALIDATTR, "zero length or invalid data", NULL);
		return ERR_INVALIDATTR;
	}
	size = msgdatasize;
	msgdatasize = MSGDATASIZE;
	rc = sqlcatalog(connectid, msgindata, size, msgoutdata, &msgdatasize);
	if (rc < 0) return rc;
	if (rc == 0) putmsgok(30);
	else if (rc == 1) putmsgtext("OKTRUNC ");
	else putmsgtext("SET     ");
	return 0;
}

static int doexecnone(void)
{
	int size;

	if (!msgdatasize) return sqlerrnummsg(ERR_INVALIDVALUE, "zero length data", NULL);
	size = msgdatasize;
	msgdatasize = MSGDATASIZE;
	rc = sqlexecnone(connectid, msgindata, size, msgoutdata, &msgdatasize);
	if (rc < 0) {
		return rc;
	}
	if (rc == 0) putmsgok(30);
	else if (rc == 1) putmsgtext("OKTRUNC ");
	else putmsgtext("SET     ");
	return 0;
}

/**
 * Called only from processmsg in this module
 */
static int doexecute(void)
{
	int size;

	if (!msgdatasize) return sqlerrnummsg(ERR_INVALIDVALUE, "zero length data", NULL);
	size = msgdatasize;
	msgdatasize = MSGDATASIZE;
	rc = sqlexecute(connectid, msgindata, size, msgoutdata, &msgdatasize);
	if (rc < 0) return rc;
	if (rc == 0) putmsgok(30);
	else if (rc == 1) putmsgtext("OKTRUNC ");
	else putmsgtext("SET     ");
	return 0;
}

static INT dorowcount(void)
{
	msgdatasize = MSGDATASIZE;
	rc = sqlrowcount(connectid, msgfsid, msgoutdata, &msgdatasize);
	if (rc < 0) return rc;
	else putmsgok(30);
	return 0;
}

static int dogetrow(void)
{
	INT type;
	long i1, count, row;

	if (msgfunc1 == 'N' || msgfunc1 == 'C') type = 1;
	else if (msgfunc1 == 'P') type = 2;
	else if (msgfunc1 == 'F') type = 3;
	else if (msgfunc1 == 'L') type = 4;
	else if (msgfunc1 == 'A') type = 5;
	else if (msgfunc1 == 'R') type = 6;
	else return sqlerrnummsg(ERR_BADFUNC, "not a recognized fetch type", NULL);
	count = 1;
	if (msgfunc1 == 'A' || msgfunc1 == 'R' || msgfunc1 == 'C') {
		if ((rc = fromcntolong(msgindata, msgdatasize, &row)) < 0) {
			return sqlerrnummsg(rc, "bad numeric in data string", NULL);
		}
		if (msgfunc1 == 'C') {
			count = row;
			row = 0;
		}
	}
	msgdatasize = MSGDATASIZE;
	for (i1 = 0; i1 < count; i1++) { /* continuous read of records if GETROWC */
		rc = sqlgetrow(connectid, msgfsid, type, row, msgoutdata, &msgdatasize);
		if (rc < 0) return rc;
		else if (rc > 0) {
			putmsgtext("SQL20000"); /* no data found */
			break;
		}
		else {
			if (msgfunc1 == 'C') putmsgok(-1); /* no time out & use blocking */
			else putmsgok(30);
		}
	}
	return 0;
}

static int dopsupdate(void)
{
	if (!msgdatasize) return sqlerrnummsg(ERR_INVALIDVALUE, "zero length data", NULL);
	rc = sqlposupdate(connectid, msgfsid, msgindata, msgdatasize);
	if (rc < 0) return rc;
	msgdatasize = 1;
	msgoutdata[0] = '1';
	putmsgok(30);
	return 0;
}

static int dopsdelete(void)
{
	rc = sqlposdelete(connectid, msgfsid);
	if (rc < 0) return rc;
	msgdatasize = 1;
	msgoutdata[0] = '1';
	putmsgok(30);
	return 0;
}

static int dodiscard(void)
{
	rc = sqldiscard(connectid, msgfsid);
	if (rc < 0) return rc;
	msgdatasize = 0;
	putmsgok(30);
	return 0;
}

/*** CODE: NOT BEING USED ***/
static int dogetinfo(void)
{
	int size;

	if (!msgdatasize || msgindata[0] == ' ') {
		sqlerrnummsg(ERR_INVALIDATTR, "zero length or invalid data", NULL);
		return ERR_INVALIDATTR;
	}
	size = msgdatasize;
	msgdatasize = MSGDATASIZE;
	rc = filegetinfo((CHAR *) msgindata, size, (CHAR *) msgoutdata, &msgdatasize);
	if (rc < 0) return rc;
	if (rc == 0) putmsgok(30);
	else if (rc == 1) putmsgtext("OKTRUNC ");
	else putmsgtext("SET     ");
	return 0;
}

static int doopen(void)
{
	int i1, fileid, keylen, matchchar, nextoffset, offset, options, reclen;
	char datafilename[512], indexfilename[512], keyinfo[512], *keyword, *value;

	reclen = 256;
	options = keylen = matchchar = 0;
	datafilename[0] = indexfilename[0] = keyinfo[0] = 0;
	if (msgfunc1 == 'F') options |= FILEOPTIONS_FILEOPEN;
	else if (msgfunc1 == 'I') options |= FILEOPTIONS_IFILEOPEN;
	else if (msgfunc1 == 'A') options |= FILEOPTIONS_AFILEOPEN;
	else if (msgfunc1 == 'H') options |= FILEOPTIONS_RAWOPEN;
	else return ERR_BADFUNC;
	if (msgfunc2 == 'S') /* do nothing */ ; // @suppress("Suspicious semicolon")
	else if (msgfunc2 == 'R') options |= FILEOPTIONS_READONLY;
	else if (msgfunc2 == 'A') options |= FILEOPTIONS_READACCESS;
	else if (msgfunc2 == 'E') options |= FILEOPTIONS_EXCLUSIVE;
	else if (msgfunc2 == 'P') options |= FILEOPTIONS_CREATEFILE;
	else if (msgfunc2 == 'Q') options |= FILEOPTIONS_CREATEONLY;
	else return ERR_BADFUNC;

	for (offset = 0; ; offset = nextoffset) {
		i1 = tcpnextdata(msgindata, msgdatasize, &offset, &nextoffset);
		if (i1 <= 0) break;
		parsekwval(msgindata + offset, i1, &keyword, &value);
		if (!strcmp(keyword, "TEXTFILENAME")) {
			strncpy(datafilename, value, sizeof(datafilename) - 1);
			datafilename[sizeof(datafilename) - 1] = 0;
		}
		else if (!strcmp(keyword, "INDEXFILENAME")) {
			strncpy(indexfilename, value, sizeof(indexfilename) - 1);
			indexfilename[sizeof(indexfilename) - 1] = 0;
		}
		else if (!strcmp(keyword, "FIXLEN")) {
			rc = fromcntoint((UCHAR *) value, (INT)strlen(value), &reclen);
			if (rc < 0) return rc;
		}
		else if (!strcmp(keyword, "VARLEN")) {
			rc = fromcntoint((UCHAR *) value, (INT)strlen(value), &reclen);
			if (rc < 0) return rc;
			options |= FILEOPTIONS_VARIABLE;
		}
		else if (!strcmp(keyword, "COMPRESSED")) options |= FILEOPTIONS_COMPRESSED;
		else if (!strcmp(keyword, "KEYLEN")) {
			rc = fromcntoint((UCHAR *) value, (INT)strlen(value), &keylen);
			if (rc < 0) return rc;
		}
		else if (!strcmp(keyword, "TEXT")) options |= FILEOPTIONS_TEXT;
		else if (!strcmp(keyword, "DATA")) options |= FILEOPTIONS_DATA;
		else if (!strcmp(keyword, "CRLF")) options |= FILEOPTIONS_CRLF;
		else if (!strcmp(keyword, "EBCDIC")) options |= FILEOPTIONS_EBCDIC;
		else if (!strcmp(keyword, "BINARY")) options |= FILEOPTIONS_BINARY;
		else if (!strcmp(keyword, "ANY")) options |= FILEOPTIONS_ANY;
		else if (!strcmp(keyword, "AIMKEY")) {
			strncpy(keyinfo, value, sizeof(keyinfo) - 1);
			keyinfo[sizeof(keyinfo) - 1] = 0;
		}
		else if (!strcmp(keyword, "NOEXTENSION")) options |= FILEOPTIONS_NOEXTENSION;
		else if (!strcmp(keyword, "MATCH")) matchchar = value[0];
		else if (!strcmp(keyword, "CASESENSITIVE")) options |= FILEOPTIONS_CASESENSITIVE;
		else if (!strcmp(keyword, "DUP")) options |= FILEOPTIONS_DUP;
		else if (!strcmp(keyword, "NODUP")) options |= FILEOPTIONS_NODUP;
		else if (!strcmp(keyword, "LOCKAUTO")) options |= FILEOPTIONS_LOCKAUTO;
		else if (!strcmp(keyword, "LOCKSINGLE")) options |= FILEOPTIONS_LOCKSINGLE;
		else if (!strcmp(keyword, "LOCKNOWAIT")) options |= FILEOPTIONS_LOCKNOWAIT;
		else if (!strcmp(keyword, "NEWCONNECTIONDELAY")); // @suppress("Suspicious semicolon")
		else return ERR_INVALIDKEYWORD;
	}
	rc = fileopen(connectid, datafilename, options, reclen, indexfilename, keylen, keyinfo, matchchar, &fileid);
	if (rc < 0) {
		return rc;
	}
	msciton(fileid, msgoutdata, 8);
	msgdatasize = 8;
	putmsgok(30);
	return 0;
}

static int doclose(void)
{
	int deleteflag;

	if (msgfunc1 == 'D') deleteflag = TRUE;
	else deleteflag = FALSE;
	rc = fileclose(connectid, msgfsid, deleteflag);
	if (rc < 0) return rc;
	msgdatasize = 0;
	putmsgok(30);
	return 0;
}

static int dofposit(void)
{
	OFFSET filepos;

	rc = filefposit(connectid, msgfsid, &filepos);
	if (rc < 0) return rc;
	mscoffton(filepos, msgoutdata, 12);
	msgdatasize = 12;
	putmsgok(30);
	return 0;
}

static int doreposit(void)
{
	OFFSET filepos;

	rc = fromcntooff(msgindata, msgdatasize, &filepos);
	if (rc < 0) return rc;
	rc = filereposit(connectid, msgfsid, filepos);
	if (rc < 0) return rc;
	msgdatasize = 0;
	if (rc == 1) putmsgtext("ATEOF   ");
	else if (rc == 2) putmsgtext("PASTEOF ");
	else putmsgok(30);
	return 0;
}

static int doread(void)
{
	int i1, i2, i3, lockflag, keysize, keycount;
	OFFSET recnum;
	UCHAR *key[128];
	int keylen[128];

	if (msgfunc2 == ' ') lockflag = FALSE;
	else if (msgfunc2 == 'L') lockflag = TRUE;
	else return ERR_BADFUNC;
	keysize = msgdatasize;
	msgdatasize = MSGDATASIZE;
	if (msgfunc1 == 'S') rc = filereadrec(connectid, msgfsid, lockflag, -1, msgoutdata, &msgdatasize);
	else if (msgfunc1 == 'B') rc = filereadrec(connectid, msgfsid, lockflag, -3, msgoutdata, &msgdatasize);
	else if (msgfunc1 == 'N' || msgfunc1 == 'O') rc = filereadnext(connectid, msgfsid, lockflag, TRUE, msgoutdata, &msgdatasize);
	else if (msgfunc1 == 'P' || msgfunc1 == 'Q') rc = filereadnext(connectid, msgfsid, lockflag, FALSE, msgoutdata, &msgdatasize);
	else if (msgfunc1 == 'K') rc = filereadkey(connectid, msgfsid, lockflag, msgindata, keysize, msgoutdata, &msgdatasize);
	else if (msgfunc1 == 'R') {
		rc = fromcntooff(msgindata, keysize, &recnum);
		if (rc < 0) return rc;
		rc = filereadrec(connectid, msgfsid, lockflag, recnum, msgoutdata, &msgdatasize);
	}
	else if (msgfunc1 == 'A') {
/*** CODE: DOES NOT PARSE OUT '"' CHARACTER.  PROBABLY NOT THE ONLY PLACE ***/
/***       CHECK IF THIS IS STILL TRUE ***/
		for (i1 = 0; i1 < keysize && msgindata[i1] != ' '; i1++);
		if (i1 == 0) return ERR_INVALIDSIZESPEC;
		rc = fromcntoint(msgindata, i1, &keycount);
		if (rc < 0) return rc;
		for (i2 = 0; i2 < keycount; i2++) {
			if (i1++ >= keysize) return ERR_INVALIDSIZESPEC;
			for (i3 = i1; i3 < keysize && msgindata[i3] != ' '; i3++);
			rc = fromcntoint(msgindata + i1, i3 - i1, &keylen[i2]);
			if (rc < 0) return rc;
			i1 = i3 + 1;
			if (i1 + keylen[i2] > keysize) return ERR_INVALIDSIZESPEC;
			key[i2] = msgindata + i1;
			i1 += keylen[i2];
		}
		rc = filereadaim(connectid, msgfsid, lockflag, keycount, key, keylen, msgoutdata, &msgdatasize);
	}
	else if (msgfunc1 == 'Z') rc = filereadrec(connectid, msgfsid, lockflag, -2, msgoutdata, &msgdatasize);
	else if (msgfunc1 == 'H') {
		for (i1 = 0; i1 < keysize && msgindata[i1] != ' '; i1++);
		if (i1 == 0 || i1 == keysize) return ERR_INVALIDSIZESPEC;
		rc = fromcntooff(msgindata, i1, &recnum);
		if (rc < 0) return rc;
		if (++i1 == keysize) return ERR_INVALIDSIZESPEC;
		rc = fromcntoint(msgindata + i1, keysize - i1, &msgdatasize);
		if (msgdatasize > MSGDATASIZE) msgdatasize = MSGDATASIZE;
		if (rc < 0) return rc;
		rc = filereadraw(connectid, msgfsid, recnum, msgoutdata, &msgdatasize);
	}
	else return ERR_BADFUNC;
	if (!rc) putmsgok(30);
	else if (rc == -1) {
		msgdatasize = 0;
		putmsgtext("NOREC   ");
	}
	else if (rc == -2) {
		msgdatasize = 0;
		putmsgtext("LOCKED  ");
	}
	else {
		return rc;
	}
	return 0;
}

#if OS_UNIX
static CHAR dbgbasename[] = "debug_fs_nulls.txt";
static FILE *debugfile;

static void writeBinaryZeroLogEntry() {
	char func[7], tmdest[13], output[64];
	struct timeval tv;
	struct tm *today;
	time_t timer;
	struct flock lock;
	int i1, i2;

	memcpy(func, tcpbuffer + 24, 6);
	func[6] = '\0';
	gettimeofday(&tv, NULL);
	timer = tv.tv_sec;
	today = localtime(&timer);
	for (i1 = 4, i2 = today->tm_year + 1900; --i1 >= 0; i2 /= 10) tmdest[i1] = (UCHAR)(i2 % 10 + '0');
	tmdest[4] = (UCHAR)((today->tm_mon + 1) / 10 + '0');
	tmdest[5] = (UCHAR)((today->tm_mon + 1) % 10 + '0');
	tmdest[6] = (UCHAR)(today->tm_mday / 10 + '0');
	tmdest[7] = (UCHAR)(today->tm_mday % 10 + '0');
	tmdest[8] = (UCHAR)(today->tm_hour / 10 + '0');
	tmdest[9] = (UCHAR)(today->tm_hour % 10 + '0');
	tmdest[10] = (UCHAR)(today->tm_min / 10 + '0');
	tmdest[11] = (UCHAR)(today->tm_min % 10 + '0');
	tmdest[12] = '\0';

	if (debugfile == NULL) {
		debugfile = fopen(dbgbasename, "a");
		if (debugfile == NULL) return;
	}
	sprintf(output, "%s %s", tmdest, func);

	memset(&lock, 0, sizeof(lock));
	lock.l_type = F_WRLCK;
	fcntl(fileno(debugfile), F_SETLKW, &lock);
	fputs(output, debugfile);
	fputc('\n', debugfile);
	fflush(debugfile);
	memset(&lock, 0, sizeof(lock));
	lock.l_type = F_UNLCK;
	fcntl(fileno(debugfile), F_SETLK, &lock);

}
#endif

static int dowrite(void)
{
	int i1, keylen;
	OFFSET recnum;

#if OS_UNIX
	for (i1 = 0; i1 < msgdatasize; i1++) {
		if (msgindata[i1] == 0x00) {
			writeBinaryZeroLogEntry(msgdatasize);
			break;
		}
	}
#endif

	if (msgfunc1 == 'S') rc = filewriterec(connectid, msgfsid, -1, msgindata, msgdatasize);
	else if (msgfunc1 == 'E') rc = filewriterec(connectid, msgfsid, -3, msgindata, msgdatasize);
	else if (msgfunc1 == 'A') rc = filewritekey(connectid, msgfsid, (UCHAR *) "", 0, msgindata, msgdatasize);
	else {
		for (i1 = 0; i1 < msgdatasize && msgindata[i1] != ' '; i1++);
		if (i1 == 0 || i1 == msgdatasize) return ERR_INVALIDSIZESPEC;
		else if (msgfunc1 == 'R') {
			rc = fromcntooff(msgindata, i1++, &recnum);
			if (rc < 0) return rc;
			rc = filewriterec(connectid, msgfsid, recnum, msgindata + i1, msgdatasize - i1);
		}
		else if (msgfunc1 == 'K') {
			rc = fromcntoint(msgindata, i1++, &keylen);
			if (rc < 0) return rc;
			rc = filewritekey(connectid, msgfsid, msgindata + i1, keylen, msgindata + i1 + keylen + 1, msgdatasize - i1 - keylen - 1);
		}
		else if (msgfunc1 == 'H') {
			rc = fromcntooff(msgindata, i1++, &recnum);
			if (rc < 0) return rc;
			rc = filewriteraw(connectid, msgfsid, recnum, msgindata + i1, msgdatasize - i1);
		}
		else return ERR_BADFUNC;
	}
	if (rc < 0) return rc;
	msgdatasize = 0;
	putmsgok(30);
	return 0;
}

static int doinsert(void)
{
	rc = fileinsert(connectid, msgfsid, msgindata, msgdatasize);
	if (rc < 0) return rc;
	msgdatasize = 0;
	putmsgok(30);
	return 0;
}

static int doupdate(void)
{
	rc = fileupdate(connectid, msgfsid, msgindata, msgdatasize);
	if (rc < 0) return rc;
	msgdatasize = 0;
	putmsgok(30);
	return 0;
}

static int dodelete(void)
{
	rc = filedelete(connectid, msgfsid, msgindata, msgdatasize);
	if (rc < 0) return rc;
	msgdatasize = 0;
	if (rc == 1) putmsgtext("NOREC   ");
	else putmsgok(30);
	return 0;
}

static int dodeletek(void)
{
	rc = filedeletek(connectid, msgfsid, msgindata, msgdatasize);
	if (rc < 0) return rc;
	msgdatasize = 0;
	if (rc == 1) putmsgtext("NOREC   ");
	else putmsgok(30);
	return 0;
}

static int dounlock(void)
{
	OFFSET filepos;

	if (msgdatasize == 0) rc = fileunlockall(connectid, msgfsid);
	else {
		rc = fromcntooff(msgindata, msgdatasize, &filepos);
		if (rc < 0) return rc;
		rc = fileunlockrec(connectid, msgfsid, filepos);
	}
	if (rc < 0) return rc;
	msgdatasize = 0;
	putmsgok(30);
	return 0;
}

static int docompare(void)
{
	int fsid2;

	rc = fromcntoint(msgindata, msgdatasize, &fsid2);
	if (rc < 0) return rc;
	rc = filecompare(connectid, msgfsid, fsid2);
	msgdatasize = 0;
	if (rc == -1) putmsgtext("LESS    ");
	else if (rc == 0) putmsgtext("SAME    ");
	else if (rc == 1) putmsgtext("GREATER ");
	else return rc;
	return 0;
}

static int doflock(void)
{
	rc = filelock(connectid, msgfsid);
	if (rc < 0) return rc;
	msgdatasize = 0;
	putmsgok(30);
	return 0;
}

static int dofunlock(void)
{
	rc = fileunlock(connectid, msgfsid);
	if (rc < 0) return rc;
	msgdatasize = 0;
	putmsgok(30);
	return 0;
}

static int doweof(void)
{
	OFFSET filepos;

	rc = fromcntooff(msgindata, msgdatasize, &filepos);
	if (rc < 0) return rc;
	rc = fileweof(connectid, msgfsid, filepos);
	if (rc < 0) return rc;
	msgdatasize = 0;
	putmsgok(30);
	return 0;
}

static int dosize(void)
{
	OFFSET filepos;

	rc = filesize(connectid, msgfsid, &filepos);
	if (rc < 0) return rc;
	mscoffton(filepos, msgoutdata, 12);
	msgdatasize = 12;
	putmsgok(30);
	return 0;
}

static int dorename(void)
{
	rc = filerename(connectid, msgfsid, msgindata, msgdatasize);
	if (rc < 0) return rc;
	msgdatasize = 0;
	putmsgok(30);
	return 0;
}

static int docommand(void)
{
	rc = filecommand(connectid, msgindata, msgdatasize);
	if (rc < 0) return rc;
	msgdatasize = 0;
	putmsgok(30);
	return 0;
}

static void writestart()
{
	CHAR work[32];

	writeout("[", 1);
	msctimestamp((UCHAR *)work);
	writeout(work, 16);
	writeout("] ", 2);

	fseek(outputfile, 0, 2);
	work[0] = '(';
	mscitoa(usernum, work + 1);
	strcat(work, "): ");
	writeout(work, -1);
}

static void writeout(CHAR *string, INT length)
{
	if (length == -1) length = (INT)strlen(string);
	fwrite(string, sizeof(CHAR), length, outputfile);
}

static void writefinish(void)
{
	writeout("\n", 1);
	fflush(outputfile);
}

#if OS_WIN32
static BOOL sigevent(DWORD sig)
{
	static time_t lastbreak = 0;

	switch (sig) {
	case CTRL_C_EVENT:
		fsflags |= FSFLAGS_SHUTDOWN;
		return TRUE;
	case CTRL_BREAK_EVENT:
		if (lastbreak == 0 || (INT) difftime(time(NULL), lastbreak) < 10) {
			lastbreak = time(NULL);
			fsflags |= FSFLAGS_SHUTDOWN;
			return TRUE;
		}
		break;
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		/* let default exit handler call ExitProcess */
		break;
	}
	return FALSE;
}
#endif

#if OS_UNIX
static void sigevent(INT sig)
{
	static time_t lastbreak = 0;
	time_t worktime;

	switch(sig) {
	case SIGINT:
		fsflags |= FSFLAGS_SHUTDOWN;
		break;
	case SIGTERM:
		worktime = time(NULL);
		if (lastbreak == 0 || (INT) difftime(worktime, lastbreak) < 10) {
			lastbreak = worktime;
			fsflags |= FSFLAGS_SHUTDOWN;
			break;
		}
	case SIGHUP:
	case SIGPIPE:  /* socket disconnected */
		exit(1);
	}
}
#endif
