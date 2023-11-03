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
#include "includes.h"

#if OS_UNIX
#include <pwd.h>
#include <unistd.h>
#if (!defined(__MACOSX) && !defined(Linux) && !defined(FREEBSD))
#include <poll.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <grp.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#ifndef FD_SET
#include <sys/select.h>
#endif
#ifndef SIGCHLD
#define SIGCHLD SIGCLD
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

#include "release.h"
#include "tcp.h"
#include "xml.h"
#if OS_WIN32
#include "svc.h"
#if _MSC_VER >= 1800
#include <VersionHelpers.h>
#endif
#endif

#if defined(__MACOSX) || defined(Linux)
#define ADDRSIZE_IS_SOCKLEN_T
#endif

#if OS_WIN32
#define ERRORVALUE() WSAGetLastError()
#else
#define ERRORVALUE() errno
#endif

#define CONNECTPORT 9735
#define SERVSUBPROCTERMINATED "<err>Server sub-process terminated during startup</err>"

#define FLAG_DEBUG		0x0001
#define FLAG_SSLONLY	0x0002
#define FLAG_NOSSL		0x0004
#define FLAG_SHUTDOWN	0x0100
#define FLAG_DIED		0x0200
#define FLAG_VERIFY		0x0400
#define FLAG_SERVICE	0x1000
#define FLAG_DAEMON		0x2000

/**
 * Turned on by using the -M command line option
 */
#define FLAG_MANUAL		0x4000

typedef struct {
	INT pid;
#if OS_WIN32
	HANDLE phandle;
#endif
	SOCKET sockethandle;
	INT flags;
	UCHAR messageid[8];
	CHAR user[33];
} USERINFO;

static INT mainargc;
static CHAR **mainargv;

static INT dbcdflags = 0;
static SOCKET sockethandle;
static INT portnum = 0;
static INT sportnum = 0;
static CHAR dxbinary[512];
static USHORT verification[32];
static CHAR serverport[16];
static USERINFO *userinfo;
static INT usercount, usermax;
static CHAR smartClientSubType[MAX_SCSUBTYPE_CHARS + 1];

#if OS_WIN32
#define MAXARGS 50
static HANDLE serviceevent;
static SVCINFO serviceinfo = {
	"DBCDService", "DB/C DX Smart Client Service"
};
#endif

#if OS_UNIX
static struct sigaction oldsigterm;
static struct sigaction oldsigint;
static struct sigaction oldsighup;
static struct sigaction oldsigpipe;
static struct sigaction oldsigchld;
#endif

static int dbcdstart(int, char **);
static void processaccept(SOCKET sockethandle, INT tcpflags);
static void writestart(void);
static void writeout(CHAR *, INT);
static void writefinish(void);
static void death1(char *);
static void death2(char *, int);
static void usage(void);

#if OS_WIN32
static BOOL sigevent(DWORD);
static void dbcdstop(void);
#endif

#if OS_UNIX
static void sigevent(int);
#endif
static void checkchildren(void);

INT main(INT argc, CHAR **argv, CHAR **envp)
{
	INT i1;
#if OS_WIN32
	INT i2, i3, svcflags;
	CHAR work[16], *password, *user, *argvwork[MAXARGS];
	//OSVERSIONINFO osInfo;
#endif
	mainargc = argc;
	mainargv = argv;

#if OS_WIN32

#define SVCFLAG_INSTALL		0x01
#define SVCFLAG_UNINSTALL	0x02
#define SVCFLAG_START		0x04
#define SVCFLAG_STOP		0x08
#define SVCFLAG_VERBOSE		0x10
	//osInfo.dwOSVersionInfoSize = sizeof(osInfo);
	/**
	 * If the OS is Win 2000 or newer
	 */
	if (IsWindowsVersionOrGreater(5, 0, 0)) {
		svcflags = 0;
		password = user = NULL;
		for (i1 = 0; ++i1 < argc; ) {
			if (argv[i1][0] == '-') {
				for (i2 = 0; argv[i1][i2 + 1] && argv[i1][i2 + 1] != '=' && i2 < (INT) sizeof(work) - 1; i2++)
					work[i2] = (CHAR) toupper(argv[i1][i2 + 1]);
				work[i2] = '\0';
				if (!strcmp(work, "DISPLAY")) {
					if (argv[i1][i2 + 1] == '=' && argv[i1][i2 + 2]) serviceinfo.servicedisplayname = argv[i1] + i2 + 2;
				}
				else if (!strcmp(work, "INSTALL")) svcflags |= SVCFLAG_INSTALL;
				else if (!strcmp(work, "PASSWORD")) {
					if (argv[i1][i2 + 1] == '=') password = argv[i1] + i2 + 2;
				}
				else if (!strcmp(work, "SERVICE")) {
					if (argv[i1][i2 + 1] == '=' && argv[i1][i2 + 2]) serviceinfo.servicename = argv[i1] + i2 + 2;
				}
				else if (!strcmp(work, "START")) svcflags |= SVCFLAG_START;
				else if (!strcmp(work, "STOP")) svcflags |= SVCFLAG_STOP;
				else if (!strcmp(work, "UNINSTALL")) svcflags |= SVCFLAG_UNINSTALL;
				else if (!strcmp(work, "USER")) {
					if (argv[i1][i2 + 1] == '=') user = argv[i1] + i2 + 2;
				}
				else if (!strcmp(work, "VERBOSE")) svcflags |= SVCFLAG_VERBOSE;
			}
		}
		if (svcflags & SVCFLAG_STOP) {
			i1 = svcstop(&serviceinfo);
			if (i1 == -1) fprintf(stdout, "Unable to stop %s: %s\n", serviceinfo.servicename, svcgeterror());
			else if (svcflags & SVCFLAG_VERBOSE) {
				if (i1 == 1) fprintf(stdout, "%s is not running\n", serviceinfo.servicename);
				else fprintf(stdout, "%s stopped\n", serviceinfo.servicename);
			}
		}
		if (svcflags & SVCFLAG_UNINSTALL) {
			i1 = svcremove(&serviceinfo);
			if (i1 == -1) fprintf(stdout, "Unable to remove %s: %s\n", serviceinfo.servicename, svcgeterror());
			else if (svcflags & SVCFLAG_VERBOSE) {
				if (i1 == 1) fprintf(stdout, "%s is not installed\n", serviceinfo.servicename);
				else fprintf(stdout, "%s removed\n", serviceinfo.servicename);
			}
		}
		if (svcflags & SVCFLAG_INSTALL) {
			for (i1 = i2 = 0; i1 < argc; i1++) {
				if (i1 && argv[i1][0] == '-') {
					for (i3 = 0; argv[i1][i3 + 1] && argv[i1][i3 + 1] != '=' && i3 < (INT) sizeof(work) - 1; i3++) work[i3] = (CHAR) toupper(argv[i1][i3 + 1]);
					work[i3] = 0;
					if (!strcmp(work, "INSTALL")) continue;
					else if (!strcmp(work, "PASSWORD")) continue;
					else if (!strcmp(work, "START")) continue;
					else if (!strcmp(work, "STOP")) continue;
					else if (!strcmp(work, "UNINSTALL")) continue;
					else if (!strcmp(work, "USER")) continue;
					else if (!strcmp(work, "VERBOSE")) continue;
				}
				if (i2 < MAXARGS - 1) argvwork[i2++] = argv[i1];
			}
			argvwork[i2] = NULL;
			if (svcinstall(&serviceinfo, user, password, i2, argvwork) == -1)
				fprintf(stdout, "Unable to install %s: %s\n", serviceinfo.servicename, svcgeterror());
			else if (svcflags & SVCFLAG_VERBOSE) fprintf(stdout, "%s installed\n", serviceinfo.servicename);
		}
		if (svcflags & SVCFLAG_START) {
			i1 = svcstart(&serviceinfo);
			if (i1 == -1) fprintf(stdout, "Unable to start %s: %s\n", serviceinfo.servicename, svcgeterror());
			else if (svcflags & SVCFLAG_VERBOSE) {
				if (i1 == 1) fprintf(stdout, "%s has finished\n", serviceinfo.servicename);
				else if (svcflags & SVCFLAG_VERBOSE) fprintf(stdout, "%s started\n", serviceinfo.servicename);
			}
		}
		if (svcflags) exit(0);

		if (svcisstarting(&serviceinfo)) {
			for (i1 = i2 = 0; i1 < argc; i1++) {
				if (i1 && argv[i1][0] == '-') {
					for (i3 = 0; argv[i1][i3 + 1] && argv[i1][i3 + 1] != '=' && i3 < (INT) sizeof(work) - 1; i3++) work[i3] = (CHAR) toupper(argv[i1][i3 + 1]);
					work[i3] = 0;
					if (!strcmp(work, "DISPLAY")) continue;
					else if (!strcmp(work, "SERVICE")) continue;
				}
				if (i2 < MAXARGS - 1) argvwork[i2++] = argv[i1];
			}
			argvwork[i2] = NULL;
			if (i2 != argc) {
				mainargc = i2;
				mainargv = argvwork;  /* ok on stack, as svcrun does not return until service is done */
			}
			dbcdflags |= FLAG_SERVICE;
			svcrun(&serviceinfo, dbcdstart, dbcdstop);
			return 0;
		}
	}
#undef SVCFLAG_INSTALL
#undef SVCFLAG_UNINSTALL
#undef SVCFLAG_START
#undef SVCFLAG_STOP
#undef SVCFLAG_VERBOSE


#endif

#if OS_UNIX
	for (i1 = 0; ++i1 < argc; ) {
		if (argv[i1][0] == '-') {
			switch (toupper(argv[i1][1])) {
			case 'Y':
				dbcdflags |= FLAG_DAEMON;
				break;
			}
		}
	}

	if (dbcdflags & FLAG_DAEMON) {
		switch (fork()) {
		case -1:
			return -1;
		case 0:
			break;
		default:
			_exit(0);
		}

		if (setsid() == -1) return -1;

#ifndef STDIN_FILENO
#define STDIN_FILENO (fileno(stdin))
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO (fileno(stdout))
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO (fileno(stderr))
#endif

#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL "/dev/null"
#endif

		if (isatty(STDIN_FILENO) || isatty(STDOUT_FILENO) || isatty(STDERR_FILENO)) {
			i1 = open(_PATH_DEVNULL, O_RDWR, 0);
			if (i1 != -1) {
				if (isatty(STDIN_FILENO)) dup2(i1, STDIN_FILENO);
				if (isatty(STDOUT_FILENO)) dup2(i1, STDOUT_FILENO);
				if (isatty(STDERR_FILENO)) dup2(i1, STDERR_FILENO);
				if (i1 > 2) close(i1);
			}
		}
	}
#endif
	i1 = dbcdstart(0, NULL);
	exit(i1);
}

static int dbcdstart(int argc, char **argv)
{
	INT i1, i2, rc;
	char work[256], directory[256];
	fd_set fdset;
	TIMEVAL timeval;
	struct sockaddr_in servaddr;
#if OS_WIN32
	WSADATA versioninfo;
	CHAR *argvwork[MAXARGS];
#endif
#if OS_UNIX
	struct sigaction act;
#endif

#if OS_WIN32
	if (argc > 1) {  /* append additional arguments (can only happen from demand serice startup) */
		for (i1 = i2 = 0; i1 < mainargc; i1++) if (i2 < MAXARGS - 1) argvwork[i2++] = mainargv[i1];
		for (i1 = 0; ++i1 < argc; ) if (i2 < MAXARGS - 1) argvwork[i2++] = argv[i1];
		argvwork[i2] = NULL;
		mainargc = i2;
		mainargv = argvwork;
	}
#endif

	/* get command line parameters */
	directory[0] = '\0';
	strcpy(dxbinary, mainargv[0]);
	for (i1 = (INT)strlen(dxbinary);
			i1 && dxbinary[i1 - 1] != '\\' && dxbinary[i1 - 1] != ':' && dxbinary[i1 - 1] != '/';
			i1--);
	dxbinary[i1] = '\0';
#if OS_WIN32
	strcat(dxbinary, "dbcc.exe");
#endif
#if OS_UNIX
	strcat(dxbinary, "dbc");
#endif
	for (i1 = 0; ++i1 < mainargc; ) {
		if (mainargv[i1][0] == '-') {
			for (i2 = 0; mainargv[i1][i2 + 1] && mainargv[i1][i2 + 1] != '=' && i2 < (INT) sizeof(work) - 1; i2++)
				work[i2] = (CHAR) toupper(mainargv[i1][i2 + 1]);
			work[i2] = 0;
			switch (work[0]) {
			case '?':
				usage();
				break;
			case 'C':
				if (work[1] == 'D' && mainargv[i1][3] == '=') {
					if (!mainargv[i1][4]) usage();
					strcpy(directory, &mainargv[i1][4]);
				}
				else usage();
				break;
			case 'D':
				if (work[1] == 'X' && mainargv[i1][3] == '=') {
					if (!mainargv[i1][4]) usage();
					strcpy(dxbinary, &mainargv[i1][4]);
				}
				else if (!work[1]) dbcdflags |= FLAG_DEBUG;
				else usage();
				break;
			case 'E':
				if (!strcmp(work, "ENCRYPT") && mainargv[i1][8] == '=') {
					if (!strcmp(&mainargv[i1][9], "off")) dbcdflags |= FLAG_NOSSL;
					else if (!strcmp(&mainargv[i1][9], "only")) dbcdflags |= FLAG_SSLONLY;
					else if (strcmp(&mainargv[i1][9], "on")) usage();
				}
				else usage();
				break;
			case 'M':
				dbcdflags |= FLAG_MANUAL;
				break;
			case 'P':
				if (!strcmp(work, "PORT") && mainargv[i1][5] == '=') {
					if (!isdigit(mainargv[i1][6])) usage();
					portnum = atoi(&mainargv[i1][6]);
				}
				else usage();
				break;
			case 'S':
				if (!strcmp(work, "SPORT")) {
					if (mainargv[i1][6] == '=') {
						if (!isdigit(mainargv[i1][7])) usage();
						sportnum = atoi(&mainargv[i1][7]);
					}
					if (!sportnum) sportnum = CONNECTPORT + 1;
				}
				else usage();
				break;
			case 'Y':
				break;
			}
		}
	}
	if (!portnum) portnum = CONNECTPORT;

	/* initialization */
#if OS_WIN32
	if (dbcdflags & FLAG_SERVICE) svcstatus(SVC_STARTING, 2000 + 1);
	if (directory[0] && !SetCurrentDirectory(directory)) death2("Unable to change directory", ERRORVALUE());
	if ( (i1 = WSAStartup(MAKEWORD(2, 2), &versioninfo)) ) {
		death2("Unable to start WSOCK", i1);
	}
	if (!(dbcdflags & FLAG_SERVICE)) SetConsoleCtrlHandler((PHANDLER_ROUTINE) sigevent, TRUE);
	else {
		serviceevent = CreateEvent(NULL, TRUE, FALSE, NULL);
		SetHandleInformation(serviceevent, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
	}
#endif
#if OS_UNIX
	if (directory[0] && chdir(directory) == -1) death2("Unable to change directory", ERRORVALUE());
#endif

	/* allocate table of active connections */
	userinfo = (USERINFO *) malloc(10 * sizeof(USERINFO));
	if (userinfo == NULL) death1("Insufficient memory");
	usermax = 10;
	usercount = 0;

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
	act.sa_flags = 0;
#ifdef SA_RESTART
	act.sa_flags |= SA_RESTART;
#endif
	if (sigaction(SIGPIPE, &act, &oldsigpipe) == -1) {
		oldsigpipe.sa_handler = SIG_DFL;
		sigemptyset(&oldsigpipe.sa_mask);
		oldsigpipe.sa_flags = 0;
	}
	if (sigaction(SIGCHLD, &act, &oldsigchld) == -1) {
		oldsigchld.sa_handler = SIG_DFL;
		sigemptyset(&oldsigchld.sa_mask);
		oldsigchld.sa_flags = 0;
	}
#endif

#if OS_WIN32
	if (dbcdflags & FLAG_SERVICE) svcstatus(SVC_STARTING, 2000 + 2);
#endif
	sockethandle = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockethandle == INVALID_SOCKET) death2("Unable to create main socket handle", ERRORVALUE());
#if OS_UNIX
	/* set close on exec */
	i1 = fcntl(sockethandle, F_GETFD, 0);
	if (i1 != -1) fcntl(sockethandle, F_SETFD, i1 | 0x01);
#endif
#if OS_WIN32
	if (dbcdflags & FLAG_SERVICE) svcstatus(SVC_STARTING, 2000 + 3);
#endif
	i1 = 1;
	setsockopt(sockethandle, SOL_SOCKET, SO_REUSEADDR, (char *) &i1, sizeof(i1));
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons((unsigned short) portnum);
	rc = bind(sockethandle, (struct sockaddr *) &servaddr, sizeof(struct sockaddr_in));
	if (rc == -1) {
#if OS_WIN32
		rc = WSAGetLastError();
		if (rc == WSAEADDRINUSE) {
			sprintf(work, "Port number (%d) already in use", portnum);
			death1(work);
		}
		death2("Unable to bind main socket address", rc);
#else
		if (errno == EADDRINUSE) {
			sprintf(work, "Port number (%d) already in use", portnum);
			death1(work);
		}
		death2("Unable to bind main socket address", errno);
#endif
	}
#if OS_WIN32
	if (dbcdflags & FLAG_SERVICE) svcstatus(SVC_STARTING, 2000 + 4);
#endif
	rc = listen(sockethandle, 100);
	if (rc == -1) death2("Unable to listen on main socket", ERRORVALUE());

	/* build verification string to pass back to dbc */
	/* 32 word value, with first 16 words static with: */
	/*     [1] = pid, [2] = pid >> 16, [4] = pid >> 24, [6] = pid >> 8 (process id hash) */
	/*     [8-15] future use */
	/*     [18] = cid >> 16, [21] = cid >> 16, [22] = cid >> 24, [23] = cid (child id hash) */
	/*     [24] = time, [27] = time >> 24, [29] = time >> 8, [30] = time >> 16 (time id hash) */
	srand((UINT)time(NULL));
	for (i1 = 0; i1 < 32; i1 += 2) {
		i2 = rand();
		verification[i1] = (UCHAR) (i2 & 0xff);
		verification[i1 + 1] = (UCHAR)((i2 >> 8) & 0xff);
	}
#if OS_WIN32
	/* set _DBCDX=<formatted pid> */
	i1 = GetCurrentProcessId();
	for (i2 = 0; i2 < 8; i1 >>= 4, i2++) work[i2] = (UCHAR)(i1 & 0x0F) + 'A';
	work[8] = '\0';
	i1 = GetCurrentProcessId();
	SetEnvironmentVariable("_DBCDX", work);
#endif
#if OS_UNIX
	i1 = getpid();
#endif
	verification[1] = (UCHAR) (i1 & 0xff);
	verification[6] = (UCHAR) ((i1 >> 8) & 0xff);
	verification[2] = (UCHAR) ((i1 >> 16) & 0xff);
	verification[4] = (UCHAR) ((i1 >> 24) & 0xff);
	for (i1 = 0; i1 < 32; i1++)
		verification[i1] = (((verification[i1] >> 4) + 'A') << 8) + (verification[i1] & 0x0F) + 'A';
	tcpitoa(portnum, serverport);

#if OS_WIN32
	if (dbcdflags & FLAG_SERVICE) {
		svcstatus(SVC_RUNNING, 0);
		svcloginfo("DB/C DX Dispatcher (dbcd) started");
	}
#endif
	while (!(dbcdflags & FLAG_SHUTDOWN)) {
		if (dbcdflags & FLAG_DIED) {
			dbcdflags &= ~FLAG_DIED;
			for (i1 = 0; i1 < usercount; i1++) {
				if (!userinfo[i1].pid && userinfo[i1].sockethandle != INVALID_SOCKET) {
					memcpy(work, userinfo[i1].messageid, 8);
					strcpy(work + 16, SERVSUBPROCTERMINATED);
					i2 = (INT)strlen(work + 16);
					tcpiton(i2, (UCHAR *) work + 8, 8);
					if (dbcdflags & FLAG_DEBUG) {
						writestart();
						writeout("SEND: ", 6);
						writeout(work, 16 + i2);
						writefinish();
					}
					tcpsend(userinfo[i1].sockethandle, (UCHAR *) work, 16 + i2, userinfo[i1].flags, 10);
					if (userinfo[i1].flags & TCP_SSL) tcpsslcomplete(userinfo[i1].sockethandle);
					closesocket(userinfo[i1].sockethandle);
					userinfo[i1].sockethandle = INVALID_SOCKET;
				}
			}
		}
		FD_ZERO(&fdset);
		FD_SET(sockethandle, &fdset);
		timeval.tv_sec = 5;
		timeval.tv_usec = 0;
		i1 = select((int)(sockethandle + 1), &fdset, NULL, NULL, &timeval);
		if (i1 <= 0) continue;
		if (FD_ISSET(sockethandle, &fdset)) processaccept(sockethandle, TCP_UTF8);
	}

#if OS_WIN32
	if (dbcdflags & FLAG_SERVICE) svcstatus(SVC_STOPPING, 2000 + 5);
#endif
#if OS_WIN32
	WSACleanup();
	if (dbcdflags & FLAG_SERVICE) svcstatus(SVC_STOPPED, 0);
#endif

#if OS_UNIX
	sigaction(SIGPIPE, &oldsigterm, NULL);
	sigaction(SIGPIPE, &oldsigint, NULL);
	sigaction(SIGPIPE, &oldsighup, NULL);
	sigaction(SIGPIPE, &oldsigpipe, NULL);
	sigaction(SIGPIPE, &oldsigchld, NULL);
#endif
	return 0;
}

/* process a client message */
static void processaccept(SOCKET sockethandle, INT tcpflags)
{
	INT i1, bufpos, len;
	INT encryptflag, guiclient, port, usernum, helloOk;
	CHAR cmdline[1024], directory[128], errormsg[1024], username[33];
	CHAR work[64], workbuffer[1024], workbuffer2[1024], *ptr;
	SOCKET worksockethandle;
	ELEMENT *e1;
	ATTRIBUTE *a1;
	struct sockaddr_in address;
	struct linger lingstr;
#if defined(ADDRSIZE_IS_SOCKLEN_T)
	socklen_t addrsize;
#else
	int addrsize;
#endif
#if OS_WIN32
	STARTUPINFO sinfo;
	PROCESS_INFORMATION pinfo;
#endif
#if OS_UNIX
	int argc, i2, i3;
	char *argv[32];
	struct passwd *passwdinfo;
	pid_t pid;
	uid_t uid;
	gid_t gid;
#endif

	worksockethandle = accept(sockethandle, NULL, NULL);
	if (worksockethandle == INVALID_SOCKET) {
/*** CODE: WRITE DEBUGGING MESSAGE ***/
		return;
	}
	lingstr.l_onoff = 1;
/*** NOTE: THIS IS SECONDS, EXCEPT LINUX MAY BE/WAS HUNDREDTHS ***/
	lingstr.l_linger = 3;
	setsockopt(worksockethandle, SOL_SOCKET, SO_LINGER, (char *) &lingstr, sizeof(lingstr));
	i1 = 1;
	/**
	 * Defeat the Nagle algorithm
	 */
	setsockopt(worksockethandle, IPPROTO_TCP, TCP_NODELAY, (char *) &i1, sizeof(i1));

	for (bufpos = 0; ; ) {
		i1 = tcprecv(worksockethandle, (UCHAR *) workbuffer + bufpos, sizeof(workbuffer) - 1 - bufpos, tcpflags, 15);
		if (i1 <= 0) {
			if (dbcdflags & FLAG_DEBUG) {
				writestart();
				if (i1 == 0) writeout("receive timed out on connecting socket", -1);
				else {
					writeout("receive failed: ", -1);
					writeout(tcpgeterror(), -1);
				}
				writefinish();
			}
			closesocket(worksockethandle);
			return;
		}
		bufpos += i1;
		if (bufpos >= 16) {
			tcpntoi((UCHAR *) workbuffer + 8, 8, &len);
			if (len < 0 || len > (INT) sizeof(workbuffer) - 17) {
				if (dbcdflags & FLAG_DEBUG) {
					writestart();
					writeout("invalid data received", -1);
					writefinish();
				}
				strcpy(errormsg, "Invalid data received");
				goto processerror;
			}
			if (bufpos >= 16 + len) break;
		}
	}
#ifdef _DEBUG
	if (dbcdflags & FLAG_DEBUG) {
#else
	if ((dbcdflags & FLAG_DEBUG) && len > 8 && memcmp(workbuffer + 16, "<verify>", 8)) {
#endif
		writestart();
		writeout("RECV: ", 6);
		writeout((CHAR *) workbuffer, 16 + len);
		writefinish();
	}

	/* process request message */
	i1 = xmlparse((CHAR *) workbuffer + 16, len, (UCHAR *) workbuffer2, sizeof(workbuffer2));
	if (i1) {
		workbuffer[16 + len] = '\0';
		strcpy(errormsg, "XML failed: ");
		strcat(errormsg, workbuffer + 16);
		goto processerror;
	}
	e1 = (ELEMENT *) workbuffer2;
	if (!strcmp(e1->tag, "verify")) {
		/* finish verification string to pass back to dbc */
		/* 32 word value, with first 16 words static with: */
		/*     [1] = pid, [2] = pid >> 16, [4] = pid >> 24, [6] = pid >> 8 (process id hash) */
		/*     [8-15] future use */
		/*     [18] = cid >> 16, [21] = cid >> 16, [22] = cid >> 24, [23] = cid (child id hash) */
		/*     [24] = time, [27] = time >> 24, [29] = time >> 8, [30] = time >> 16 (time id hash) */
		usernum = 0;
		for (a1 = e1->firstattribute; a1 != NULL; a1 = a1->nextattribute) {
			if (!strcmp(a1->tag, "user")) usernum = atoi(a1->value);
		}
		usernum--;
		if (usernum >= 0 && usernum < usercount) i1 = userinfo[usernum].pid;
		else i1 = 0;
		verification[23] = (UCHAR) (i1 & 0xff);
		verification[18] = (UCHAR) ((i1 >> 8) & 0xff);
		verification[21] = (UCHAR) ((i1 >> 16) & 0xff);
		verification[22] = (UCHAR) ((i1 >> 24) & 0xff);
		i1 = (INT) time(NULL);
		verification[24] = (UCHAR) (i1 & 0xff);
		verification[29] = (UCHAR) ((i1 >> 8) & 0xff);
		verification[30] = (UCHAR) ((i1 >> 16) & 0xff);
		verification[27] = (UCHAR) ((i1 >> 24) & 0xff);
		for (i1 = 16; i1 < 32; i1++) {
			if (i1 != 18 && i1 != 21 && i1 != 22 && i1 != 23 && i1 != 24 && i1 != 27 && i1 != 29 && i1 != 30) continue;
			verification[i1] = (((verification[i1] >> 4) + 'A') << 8) + (verification[i1] & 0x0F) + 'A';
		}
		i1 = 16;
		strcpy(workbuffer + i1, "<ok>");
		i1 += (INT)strlen(workbuffer + i1);
		memcpy(workbuffer + i1, verification, sizeof(verification));
		i1 += sizeof(verification);
		strcpy(workbuffer + i1, "</ok>");
		i1 += (INT)strlen(workbuffer + i1);
		tcpiton(i1 - 16, (UCHAR *) workbuffer + 8, 8);
#ifdef _DEBUG
		if (dbcdflags & FLAG_DEBUG) {
			writestart();
			writeout("SEND: ", 6);
			writeout((CHAR *) workbuffer, i1);
			writefinish();
		}
#endif
		tcpsend(worksockethandle, (UCHAR *) workbuffer, i1, tcpflags, 10);
		if (usernum >= 0 && usernum < usercount) {
			if (sportnum && userinfo[usernum].sockethandle != INVALID_SOCKET) {
				memcpy(workbuffer, userinfo[usernum].messageid, 8);
				i1 = 16;
				if (userinfo[usernum].pid) {
					strcpy(workbuffer + i1, "<ok>");
					i1 += (INT)strlen(workbuffer + i1);
					i1 += tcpitoa(sportnum + usernum, workbuffer + i1);
					strcpy(workbuffer + i1, "</ok>");
				}
				else strcpy(workbuffer + i1, SERVSUBPROCTERMINATED);  /* failed during this transmission (unlikely) */
				i1 += (INT)strlen(workbuffer + i1);
				tcpiton(i1 - 16, (UCHAR *) workbuffer + 8, 8);
				if (dbcdflags & FLAG_DEBUG) {
					writestart();
					writeout("SEND: ", 6);
					writeout((CHAR *) workbuffer, i1);
					writefinish();
				}
				tcpsend(userinfo[usernum].sockethandle, (UCHAR *) workbuffer, i1, userinfo[usernum].flags, 10);
				if (userinfo[usernum].flags & TCP_SSL) tcpsslcomplete(userinfo[usernum].sockethandle);
				closesocket(userinfo[usernum].sockethandle);
				userinfo[usernum].sockethandle = INVALID_SOCKET;
			}
			userinfo[usernum].flags = 0;
		}
		if (dbcdflags & FLAG_VERIFY) {
			dbcdflags &= ~FLAG_VERIFY;
			for (i1 = 0; i1 < usercount; i1++) {
				if (!userinfo[i1].pid) continue;
				if (userinfo[i1].sockethandle != INVALID_SOCKET || userinfo[i1].flags == FLAG_VERIFY) dbcdflags &= ~FLAG_VERIFY;
			}
		}
	}
	else if (!strcmp(e1->tag, "hello")) {
		i1 = 16;
		helloOk = FALSE;
		if (e1->firstsubelement != NULL && e1->firstsubelement->cdataflag &&
			strncmp(e1->firstsubelement->tag, "DB/C SC ", 8)) {
			strcpy(workbuffer + i1, "<err>Connection to Smart Client server</err>");
		}
		else {
			sprintf(workbuffer + i1, "<ok>DB/C SS %s</ok>", RELEASE);
//			strcpy(workbuffer + i1, "<ok>DB/C SS ");
//			i1 += (INT)strlen(workbuffer + i1);
//			strcpy(workbuffer + i1, RELEASE);
//			i1 += (INT)strlen(workbuffer + i1);
//			strcpy(workbuffer + i1, "</ok>");
			i1 += (INT)strlen(workbuffer + i1);
			helloOk = TRUE;
		}
		i1 += (INT)strlen(workbuffer + i1);
		tcpiton(i1 - 16, (UCHAR *) workbuffer + 8, 8);
		if (dbcdflags & FLAG_DEBUG) {
			writestart();
			writeout("SEND: ", 6);
			writeout((CHAR *) workbuffer, i1);
			writefinish();
		}
		tcpsend(worksockethandle, (UCHAR *) workbuffer, i1, tcpflags, 10);

		/*
		 * If the characters following 'DB/C SC ' are non-numeric, save them.
		 * For some of the SCs it identifies the type.
		 * We may use this later to inform the runtime.
		 */
		smartClientSubType[0] = '\0';
		if (helloOk)
		{
			ptr =  e1->firstsubelement->tag + 8;
			if (isalpha(ptr[0]))
			{
				i1 = MAX_SCSUBTYPE_CHARS;
				do
				{
					strncat(smartClientSubType, ptr, 1);
					ptr++;
					i1--;
				} while(i1 && isalpha(ptr[0]));
				if (dbcdflags & FLAG_DEBUG) {
					writestart();
					sprintf(work, "SC Type=%s", smartClientSubType);
					writeout(work, -1);
					writefinish();
				}
			}
		}
	}
	else if (!strcmp(e1->tag, "start")) {
		port = 0;
		encryptflag = FALSE;
		guiclient = FALSE;
		directory[0] = username[0] = '\0';
		strcpy(cmdline, dxbinary);
		//len = (INT)strlen(cmdline);
		/* get start options */
		for (a1 = e1->firstattribute; a1 != NULL; a1 = a1->nextattribute) {
			if (!strcmp(a1->tag, "port")) port = atoi(a1->value);
			else if (!strcmp(a1->tag, "user")) strcpy(username, a1->value);
			else if (!strcmp(a1->tag, "dir")) strcpy(directory, a1->value);
			else if (!strcmp(a1->tag, "encryption")) {
				if (!strcmp(a1->value, "y")) encryptflag = TRUE;
			}
			else if (!strcmp(a1->tag, "gui")) {
				if (!strcmp(a1->value, "y")) guiclient = TRUE;
			}
		}
		if (!encryptflag && (dbcdflags & FLAG_SSLONLY)) {
			strcpy(errormsg, "Server only supporting encrypted connections");
			goto processerror;
		}
		else if (encryptflag && (dbcdflags & FLAG_NOSSL)) {
			strcpy(errormsg, "Server not supporting encrypted connections");
			goto processerror;
		}

		/* append dbc args to command line */
		if (e1->firstsubelement != NULL && e1->firstsubelement->cdataflag) {
			strcat(cmdline, " ");
			strcat(cmdline, e1->firstsubelement->tag);
			//cmdline[len++] = ' ';
			//strcpy(cmdline + len, e1->firstsubelement->tag);
			//len += (INT)strlen(cmdline + len);
		}

		/* find first unused position */
#if OS_WIN32
		checkchildren(); /* close finished child process handles (needed frequently) */
		for (usernum = 0; usernum < usercount && userinfo[usernum].pid; usernum++);
#else
		while (usercount && !userinfo[usercount - 1].pid) usercount--;
		for (usernum = 0; usernum < usercount && userinfo[usernum].pid; usernum++);
		if (usernum == usermax) {
			checkchildren();
			for (usernum = 0; usernum < usercount && userinfo[usernum].pid; usernum++);
		}
#endif
		if (!port) {
			if (!sportnum) {
				strcpy(errormsg, "Server not supporting sport connections");
				goto processerror;
			}
			if (portnum == sportnum + usernum) {
				strcpy(errormsg, "Server sport range conflicts with port value");
				goto processerror;
			}
		}
		if (usernum == usermax) {
			void *tmp = (CHAR *) realloc(userinfo, (usermax << 1) * sizeof(USERINFO));
			if (tmp == NULL) {
				strcpy(errormsg, "Server unable to extend process information table");
				goto processerror;
			}
			userinfo = (USERINFO *) tmp;
			usermax <<= 1;
		}
		if (usernum < usercount && userinfo[usernum].sockethandle != INVALID_SOCKET) {  /* unlikely, but just in case */
			memcpy(workbuffer2, userinfo[usernum].messageid, 8);
			strcpy(workbuffer2 + 16, SERVSUBPROCTERMINATED);
			i1 = (INT)strlen(workbuffer2 + 16);
			tcpiton(i1, (UCHAR *) workbuffer2 + 8, 8);
			if (dbcdflags & FLAG_DEBUG) {
				writestart();
				writeout("SEND: ", 6);
				writeout(work, 16 + i1);
				writefinish();
			}
			tcpsend(userinfo[usernum].sockethandle, (UCHAR *) workbuffer2, 16 + i1, userinfo[usernum].flags, 10);
			if (userinfo[usernum].flags & TCP_SSL) tcpsslcomplete(userinfo[usernum].sockethandle);
			closesocket(userinfo[usernum].sockethandle);
		}

		/* get user (informational only) */
		strncpy(userinfo[usernum].user, username, sizeof(userinfo->user) - 1);
		userinfo[usernum].user[sizeof(userinfo->user) - 1] = '\0';
		userinfo[usernum].sockethandle = INVALID_SOCKET;

		/* finish build command line */
		if (port) {
			addrsize = sizeof(address);
			i1 = getpeername(worksockethandle, (struct sockaddr *) &address, &addrsize);
			if (i1 == SOCKET_ERROR) {
				strcpy(errormsg, "getpeername() failed");
				goto processerror;
			}
			strcat(cmdline, " -schost=");
			strcat(cmdline, (CHAR *) inet_ntoa(address.sin_addr));
			sprintf(work, " -scport=%d", port);
			strcat(cmdline, work);
		}
		else {
			sprintf(work, " -scport=%d", sportnum + usernum);
			strcat(cmdline, work);
		}
		if (encryptflag) {
			strcat(cmdline, " -scssl");
		}
		if (guiclient) {
			strcat(cmdline, " -scgui");
		}
		if (smartClientSubType[0] != '\0') {
			strcat(cmdline, " -sctype=");
			strcat(cmdline, smartClientSubType);
		}
		sprintf(work, " -scserver=%s:%d", serverport, usernum + 1);
		strcat(cmdline, work);
#if OS_WIN32
		if (dbcdflags & FLAG_SERVICE) {
			strcat(cmdline, " -scevent");
			/**
			 * SCEVENT is looked for on the command line in dbcctl.dbcinit
			 * But the value is ignored. So don't write it.
			 * Converting a HANDLE to an INT is problematic anyway.
			 *
			 * jpr 28 OCT 2015
			 */
			//len += (INT)strlen(cmdline + len);
			//len += tcpitoa((INT)serviceevent, cmdline + len);
		}
#endif
		if ((dbcdflags & FLAG_DEBUG) && !(dbcdflags & FLAG_MANUAL)) {
			writestart();
			writeout("EXEC: ", -1);
			writeout(cmdline, -1);
			writefinish();
		}

		if (!(dbcdflags & FLAG_MANUAL)) {
#if OS_WIN32
			GetStartupInfo(&sinfo);
			if (!CreateProcess(NULL, cmdline, NULL, NULL, 0, DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
					NULL, directory[0] ? directory : NULL, &sinfo, &pinfo))
			{
				int i5 = (int) GetLastError();
				if (dbcdflags & FLAG_SERVICE) svclogerror("DBCD, CreateProcess() failed", i5);
				sprintf(errormsg, "CreateProcess() failed, error = %i", i5);
				goto processerror;
			}
			CloseHandle(pinfo.hThread);
			userinfo[usernum].pid = pinfo.dwProcessId;
			userinfo[usernum].phandle = pinfo.hProcess;
			if (usernum == usercount) usercount++;
#endif

#if OS_UNIX
			for (i1 = argc = 0, len = strlen(cmdline); i1 < len; i1++) {
				if (isspace(cmdline[i1])) continue;
				if (argc == 31) break;
				argv[argc++] = cmdline + i1;
				for (i2 = i1, i3 = FALSE; cmdline[i1] && (!isspace(cmdline[i1]) || i3); i1++) {
					if (cmdline[i1] == '"') {
						i3 = !i3;
						continue;
					}
					if (cmdline[i1] == '\\' && (cmdline[i1 + 1] == '"' || cmdline[i1 + 1] == '\\')) i1++;
					cmdline[i2++] = cmdline[i1];
				}
				cmdline[i2] = 0;
			}
			argv[argc] = NULL;
			uid = gid = -1;
			if (username[0]) {
				passwdinfo = getpwnam(username);
				if (passwdinfo == NULL) {
					sprintf(errormsg, "getpwnam() failed, effective user name %s is probably invalid, error = %d", username, errno);
					goto processerror;
				}
				else {
					uid = passwdinfo->pw_uid;
					gid = passwdinfo->pw_gid;
				}
			}
			if ((pid = fork()) == (pid_t) -1) {  /* fork failed */
				sprintf(errormsg, "fork() failed, errno = %d", errno);
				goto processerror;
			}
			if (pid == (pid_t) 0) {  /* child */
				closesocket(worksockethandle);
				closesocket(sockethandle);

				/*
				 * This is where the -user Smart Client option
				 * really happens.
				 */
				if ((signed int) uid >= 0 && (signed int) gid >= 0) {
					if (initgroups(username, gid) == -1) {
						if (dbcdflags & FLAG_DEBUG) {
							sprintf(errormsg, "initgroups() failed, gid = %d, error = %d", gid, errno);
							writestart();
							writeout(errormsg, -1);
							writefinish();
						}
						_exit(1);
					}

					if (setregid(-1, gid) < 0) {
						if (dbcdflags & FLAG_DEBUG) {
							sprintf(errormsg, "setregid() failed, gid = %d, error = %d", gid, errno);
							writestart();
							writeout(errormsg, -1);
							writefinish();
						}
						_exit(1);
					}
					if (setreuid(-1, uid) < 0) {
						if (dbcdflags & FLAG_DEBUG) {
							sprintf(errormsg, "setreuid() failed, uid = %d, error = %d", uid, errno);
							writestart();
							writeout(errormsg, -1);
							writefinish();
						}
						_exit(1);
					}

				}

				if (directory[0]) {
					if (chdir(directory) < 0) {
						if (dbcdflags & FLAG_DEBUG) {
							sprintf(errormsg, "chdir() to '%s' failed, error = %d", directory, errno);
							writestart();
							writeout(errormsg, -1);
							writefinish();
						}
						_exit(1);
					}
				}
				execvp(argv[0], argv);
				if (dbcdflags & FLAG_DEBUG) {
					sprintf(errormsg, "execvp() failed, error = %d", errno);
					writestart();
					writeout(errormsg, -1);
					writefinish();
				}
				_exit(1);
			}
			userinfo[usernum].pid = (INT) pid;
			if (usernum == usercount) usercount++;
#endif
		}
		else {
			writestart();
			writeout("EXEC: \"", -1);
			writeout(cmdline, -1);
			writeout("\"", -1);
			writefinish();
			userinfo[usernum].pid = usernum + 1;
			if (usernum == usercount) usercount++;
		}
		dbcdflags |= FLAG_VERIFY;
		if (!port) {
			userinfo[usernum].sockethandle = worksockethandle;
			userinfo[usernum].flags = tcpflags;
			memcpy(userinfo[usernum].messageid, workbuffer, 8);
			return;
		}
		userinfo[usernum].flags = FLAG_VERIFY;
		memcpy(workbuffer + 8, "       5<ok/>", 13);
		if (dbcdflags & FLAG_DEBUG) {
			writestart();
			writeout("SEND: ", 6);
			writeout((CHAR *) workbuffer, 21);
			writefinish();
		}
		tcpsend(worksockethandle, (UCHAR *) workbuffer, 21, tcpflags, 10);
	}
	else {
		strcpy(errormsg, "Invalid function");
		goto processerror;
	}
	closesocket(worksockethandle);
	return;

processerror:
	i1 = 16;
	strcpy(workbuffer + i1, "<err>");
	i1 += (INT)strlen(workbuffer + i1);
	strcpy(workbuffer + i1, errormsg);
	i1 += (INT)strlen(workbuffer + i1);
	strcpy(workbuffer + i1, "</err>");
	i1 += (INT)strlen(workbuffer + i1);
	tcpiton(i1 - 16, (UCHAR *) workbuffer + 8, 8);
	if (dbcdflags & FLAG_DEBUG) {
		writestart();
		writeout("SEND: ", 6);
		writeout((CHAR *) workbuffer, i1);
		writefinish();
	}
	tcpsend(worksockethandle, (UCHAR *) workbuffer, i1, tcpflags, 10);
	closesocket(worksockethandle);
}

static void writestart()
{
}

static void writeout(CHAR *string, INT length)
{
	if (length == -1) length = (INT)strlen(string);
	fwrite(string, sizeof(CHAR), length, stdout);
}

static void writefinish(void)
{
	writeout("\n", 1);
	fflush(stdout);
}

static void death1(char *msg)
{
	if (!(dbcdflags & FLAG_SERVICE)) {
		fprintf(stdout, "ERROR: %s\n", msg);
		fflush(stdout);
	}
#if OS_WIN32
	WSACleanup();
	if (dbcdflags & FLAG_SERVICE) {
		svcstatus(SVC_STOPPED, 0);
		svclogerror(msg, 0);
	}
#endif
	exit(1);
}

static void death2(char *msg, int num)
{
	if (!(dbcdflags & FLAG_SERVICE) && (dbcdflags & FLAG_DEBUG)) {
		fprintf(stdout, "ERROR: %s: error = %d\n", msg, num);
		fflush(stdout);
	}
#if OS_WIN32
	WSACleanup();
	if (dbcdflags & FLAG_SERVICE) {
		svcstatus(SVC_STOPPED, 0);
		svclogerror(msg, num);
	}
#endif
	exit(1);
}

static void usage()
{
	/*     12345678901234567890123456789012345678901234567890123456789012345678901234567890 */
	fputs("DBCD command  " RELEASEPROGRAM RELEASE COPYRIGHT "\n", stdout);
	fputs("Usage:  dbcd [-dx=binary] [-port=n] [-sport[=n]] [-encrypt=<on|off|only>]\n", stdout);
	fputs("             [-cd=directory] [-?]\n", stdout);
#if OS_WIN32
	fputs("        Windows service options: [-display=servicedisplayname] [-install]\n", stdout);
	fputs("                               [-password=logonpassword] [-service=servicename]\n", stdout);
	fputs("                               [-start] [-stop] [-uninstall] [-user=logonuser]\n", stdout);
	fputs("                               [-verbose]\n", stdout);
#endif
#if OS_UNIX
	fputs("        UNIX deamon options: [-y]\n", stdout);
#endif
	exit(0);
}

#if OS_WIN32
static BOOL sigevent(DWORD sig)
{
	switch (sig) {
	case CTRL_C_EVENT:
		dbcdflags |= FLAG_SHUTDOWN;
		return TRUE;
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		dbcdflags |= FLAG_SHUTDOWN;
		return TRUE;
	}
	return FALSE;
}

static void dbcdstop()
{
	dbcdflags |= FLAG_SHUTDOWN;
	svcstatus(SVC_STOPPING, 15000 + 6);
}
#endif

#if OS_UNIX
static void sigevent(INT sig)
{
	INT i1, pid;

	switch(sig) {
	case SIGINT:
		dbcdflags |= FLAG_SHUTDOWN;
		break;
	case SIGTERM:
	case SIGHUP:
		dbcdflags |= FLAG_SHUTDOWN;
		break;
	case SIGPIPE:  /* socket disconnected */
		break;
	case SIGCHLD:
		/* release zombie process */
#ifdef WNOHANG
		while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
#else
		if ((pid = wait(NULL)) > 0) {
#endif
			for (i1 = 0; i1 < usercount; i1++) {
				if (pid == userinfo[i1].pid) {
					if (sportnum && userinfo[i1].sockethandle != INVALID_SOCKET) dbcdflags |= FLAG_DIED;
					userinfo[i1].pid = 0;
					break;
				}
			}
		}
		break;
	}
}
#endif

static void checkchildren()
{
	INT cnt;
#if OS_WIN32
	DWORD exitval;
#else
	INT i1, pid;
#endif
	for (cnt = 0; cnt < usercount; cnt++) {
		if (!userinfo[cnt].pid) {
			continue;
		}
#if OS_WIN32
		if (GetExitCodeProcess(userinfo[cnt].phandle, &exitval)) {
			if (exitval != STILL_ACTIVE) {
				if (sportnum && userinfo[cnt].sockethandle != INVALID_SOCKET) dbcdflags |= FLAG_DIED;
				CloseHandle(userinfo[cnt].phandle);
				userinfo[cnt].pid = 0;
			}
		}
#else
		/* check for any zombies */
		if (kill(userinfo[cnt].pid, 0) == -1 && errno == ESRCH) {
			do {
#ifdef WNOHANG
				while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
#else
/*** CODE: DO WE HAVE TO BLOCK SIGCHLD DURING THIS CALL ??? ***/
				if ((pid = wait(NULL)) > 0) {
#endif
					if (pid == userinfo[cnt].pid) {
						if (sportnum && userinfo[cnt].sockethandle != INVALID_SOCKET) dbcdflags |= FLAG_DIED;
						userinfo[cnt].pid = 0;
					}
					else for (i1 = 0; i1 < usercount; i1++) {
						if (pid == userinfo[i1].pid) {
							if (sportnum && userinfo[i1].sockethandle != INVALID_SOCKET) dbcdflags |= FLAG_DIED;
							userinfo[i1].pid = 0;
							break;
						}
					}
				}
			} while (pid > 0 && userinfo[cnt].pid);
		}
#endif
	}
	while (usercount && !userinfo[usercount - 1].pid) usercount--;
}
