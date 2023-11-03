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
#define INC_STRING
#define INC_STDLIB
#define INC_CTYPE
#define INC_LIMITS
#define INC_SIGNAL
#define INC_TIME
#define INC_ERRNO
#define INC_IPV6

#include "includes.h"
#include "release.h"

#if OS_UNIX
#ifdef Linux
#include <syslog.h>
#endif
#include <unistd.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <netinet/tcp.h>
#ifndef FD_SET
#include <sys/select.h>
#endif
#include <sys/wait.h>
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
#define ARRAYSIZE(A) (sizeof(A)/sizeof((A)[0]))
#else
#if _MSC_VER >= 1800
#include <VersionHelpers.h>
#endif
#endif

#include "fio.h"
#include "tcp.h"
#include "xml.h"
#include "util.h"
#include "base.h"
#include "cert.h"

#if OS_WIN32
// Win32 Sleep function takes milliseconds
#include "svc.h"
#define ERRORVALUE() WSAGetLastError()
#define SLEEP(milliseconds) Sleep((DWORD)(milliseconds))
#endif
#if OS_UNIX
// Linux sleep function takes seconds
#define ERRORVALUE() errno
static INT sig_received = 0;
#define SLEEP(milliseconds) sleep((milliseconds) / 1000)
#endif

#define CONNECTPORT 9584

/**
 * Unless overridden by the config, the default is 1/4 second
 */
static long int startDelayInMilliseconds = 250;

/**
 * The loop looking for dead processes will run every 1/8 of a second
 */
#define DEAD_PROCESS_DELAY_IN_MILLISECONDS 125

#define FLAG_DEBUG		0x0001
#define FLAG_SSLONLY	0x0002
#define FLAG_NOSSL		0x0004
#define FLAG_NEWLOG		0x0008
#define FLAG_NOLOG		0x0010
#if defined(Linux)
#define FLAG_LOGSIGS	0x0020
#define FLAG_LOGSIGSX	0x0040
#define FLAG_LOGACCEPT  0x0080
static INT pipefd[2];
typedef struct pipe_message {
	int32_t size;
	siginfo_t si;
} PIPEMESSAGE;
#endif
#define FLAG_SHUTDOWN	0x0100
#define FLAG_DIED		0x0200
#define FLAG_VERIFY		0x0400
#define FLAG_SERVICE	0x1000
#define FLAG_DAEMON		0x2000
#define FLAG_MANUAL		0x4000

typedef struct {
	INT pid;
#if OS_WIN32
	HANDLE phandle;
#endif
	SOCKET sockethandle;
	INT flags;
	UCHAR messageid[8];
	CHAR user[21];
} USERINFO;

#define MAX_VOLUMESIZE	8
typedef struct {
	CHAR volume[MAX_VOLUMESIZE + 1];
	CHAR **path;
} VOLUME;

typedef struct cfgvolume_struct {
	INT next;
	CHAR volume[MAX_VOLUMESIZE + 1];
	CHAR path[1];
} CFGVOLUME;

static INT mainargc;
static CHAR **mainargv;

static INT fsflags;
static SOCKET sockethandle1, sockethandle2;
//static LCSHANDLE lcshandle;
static INT sportnum = 0;
static UCHAR verification[32];
static CHAR serverport[16];
static CHAR cfgfilename[256];
static CHAR logfilename[256];
static CHAR tempfilename[256];
static CHAR workdirname[256];
#if OS_WIN32
static HANDLE loghandle = INVALID_HANDLE_VALUE;
static HANDLE temphandle = INVALID_HANDLE_VALUE;
#else
#define INVALID_HANDLE_VALUE -1
static INT loghandle = INVALID_HANDLE_VALUE;
static INT temphandle = INVALID_HANDLE_VALUE;
#endif
static OFFSET tempfilepos = 0;
static CHAR fsbinary[512];
static INT oldProtocol = FALSE;
/*
 * The size of this array is set by the max user count in the license
 */
static USERINFO *userinfo;
/*
 * This is not actually the number of connected users.
 * It is the index (+1) of the highest non-zero-pid entry
 * in the USERINFO array.
 */
static INT usercount;
/*
 * The maximum number of connections allowed. Default to 10. Can be set by config
 */
static INT usermax = 10;
static INT debuglevel;
static CHAR adminpassword[32];
static CHAR showpassword[32];
static CHAR outputname[MAX_NAMESIZE];
static CHAR certificatefilename[MAX_NAMESIZE];
static FILE *outputfile;
static INT volcount = 0;					/* number of VOLUME entries */
static INT volalloc = 0;
static VOLUME **volumes;

#if OS_WIN32
static HANDLE serviceevent;
/*
 * Not really used, exists only to satisfy the call to CreateThread
 */
static DWORD threadid;
static HANDLE hUserArrayMutex;
#endif

#if OS_WIN32
#define MAXARGS 50
static SVCINFO serviceinfo = {
	"DBCFSService", "DB/C FS Service"
};
#endif

#if OS_UNIX
static struct sigaction oldsigterm;
static struct sigaction oldsigint;
static struct sigaction oldsighup;
static struct sigaction oldsigpipe;
static struct sigaction oldsigchld;
#ifdef Linux
static struct sigaction oldsigtrap;
#endif
#endif

static void closeAndCleanUpLogFile();
static INT dbcfsstart(INT, CHAR **);
static void processaccept(SOCKET, INT);
static INT renamelogfile(INT);
static INT flushlogfile(void);
static void parsecfgfile(CHAR *, CHAR *, CHAR *, CHAR *, CHAR *, CHAR *, INT *, INT *, FIOPARMS *);
static ELEMENT *getxmldata(CHAR *, INT);
static INT getnextkwval(CHAR *, CHAR **, CHAR **);
static INT volmapfnc(CHAR *, CHAR ***);
static INT translate(CHAR *, UCHAR *);
static void writestart(void);
static void writeout(CHAR *, INT);
static void writefinish(void);
static void death1(char *);
static void death2(char *, INT);
static void usage(void);
static INT writeStartingXML(FHANDLE loghndl);

#if OS_WIN32
static BOOL sigevent(DWORD);
static void dbcfsstop(void);
static DWORD threadproc(DWORD);
#endif

#if OS_UNIX
#ifdef Linux
static void sigevent3(int, siginfo_t *, void *);
static siginfo_t siginfo;
static void tryGetPidNameViaProc(pid_t pid, CHAR *name, INT cbname);
static void tryGetPidNameViaPopen(pid_t pid, CHAR *name, INT cbname);
#else
static void sigevent(INT);
#endif
static void checkchildren(void);
#endif

#if OS_UNIX
static INT _memicmp(void *, void *, INT);
#endif

/* start of program */
INT main(INT argc, CHAR **argv)
{
	INT i1;
#if OS_WIN32
	INT i2, i3, i4, svcflags;
	CHAR work[16], *password, *user, *argvwork[MAXARGS];
	//OSVERSIONINFO osInfo;
#endif

#if OS_UNIX && defined(Linux)
	memset(&siginfo, 0, sizeof(siginfo_t));
#endif
	/* check for usage */
	for (i1 = 0; ++i1 < argc; ) {
		if (argv[i1][0] == '-') {
			switch (toupper(argv[i1][1])) {
			case '?':
				fputs("Release: " RELEASE, stdout);
				fputs("\nMachine Type: ", stdout);
#if OS_WIN32
				fputs("WINI", stdout);
#elif OS_UNIX && defined(Linux)
				fputs("LINUXI", stdout);
#endif
				fputs("\n\n", stdout);
				usage();
				return 0;
			}
		}
	}

	mainargc = argc;
	mainargv = argv;

#if OS_WIN32
#define SVCFLAG_INSTALL		0x01
#define SVCFLAG_UNINSTALL	0x02
#define SVCFLAG_START		0x04
#define SVCFLAG_STOP		0x08
#define SVCFLAG_VERBOSE		0x10
	//osInfo.dwOSVersionInfoSize = sizeof(osInfo);
	if (IsWindows7OrGreater()) {
		svcflags = 0;
		password = user = NULL;
		cfgfilename[0] = '\0';
		for (i1 = 0; ++i1 < argc; ) {
			if (argv[i1][0] == '-') {
				for (i2 = 0; argv[i1][i2 + 1] && argv[i1][i2 + 1] != '=' && (UINT) i2 < sizeof(work) - 1; i2++)
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
			else if (!cfgfilename[0]) strcpy(cfgfilename, argv[i1]);
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
			argvwork[0] = argv[0];
			if (!cfgfilename[0]) strcpy(cfgfilename, "dbcfs.cfg");
			for (i1 = 0; cfgfilename[i1] && cfgfilename[i1] != ':' && cfgfilename[i1] != '\\' && cfgfilename[i1] != '/'; i1++);
			if (!cfgfilename[i1]) {
				strcpy(fsbinary, cfgfilename);
				i1 = GetCurrentDirectory(sizeof(cfgfilename), (LPTSTR) cfgfilename);
				if (i1 && cfgfilename[i1 - 1] != '\\' && cfgfilename[i1 - 1] != '/') cfgfilename[i1++] = '\\';
				strcpy(cfgfilename + i1, fsbinary);
			}
			argvwork[1] = cfgfilename;
			for (i2 = 2, i1 = i4 = 0; ++i1 < argc; ) {
				if (argv[i1][0] == '-') {
					for (i3 = 0; argv[i1][i3 + 1] && argv[i1][i3 + 1] != '=' && (UINT)i3 < sizeof(work) - 1; i3++)
						work[i3] = (CHAR) toupper(argv[i1][i3 + 1]);
					work[i3] = '\0';
					if (!strcmp(work, "INSTALL")) continue;
					else if (!strcmp(work, "PASSWORD")) continue;
					else if (!strcmp(work, "START")) continue;
					else if (!strcmp(work, "STOP")) continue;
					else if (!strcmp(work, "UNINSTALL")) continue;
					else if (!strcmp(work, "USER")) continue;
					else if (!strcmp(work, "VERBOSE")) continue;
				}
				else if (!i4++) continue;
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
					for (i3 = 0; argv[i1][i3 + 1] && argv[i1][i3 + 1] != '=' && (UINT)i3 < sizeof(work) - 1; i3++)
						work[i3] = (CHAR) toupper(argv[i1][i3 + 1]);
					work[i3] = '\0';
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
			fsflags |= FLAG_SERVICE;
			svcrun(&serviceinfo, dbcfsstart, dbcfsstop);
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
#ifdef Linux
	openlog("dbcfs", 0, LOG_USER);
#endif
	for (i1 = 0; ++i1 < argc; ) {
		if (argv[i1][0] == '-') {
			switch (toupper(argv[i1][1])) {
			case 'Y':
				fsflags |= FLAG_DAEMON;
				break;
			}
		}
	}

	/*
	 * A process may be intentionally orphaned so that it becomes detached from the user's session
	 * and left running in the background; usually to allow a long-running job to complete without
	 * further user attention, or to start an indefinitely running service. (wikipedia)
	 */
	if (fsflags & FLAG_DAEMON) {
		switch (fork()) {
		case -1:
			return -1;
		case 0:
			break;		/* When fork() returns 0, we are in the child process. */
		default:
			_exit(0);	/* Parent process intentionally kills itself */
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

	return dbcfsstart(0, NULL);
}

static INT dbcfsstart(INT argc, CHAR **argv)
{
	INT i1, i2, portnum, rc, sslportnum;
	CHAR work[256], *ptr;
	FIOPARMS fioparms;
	time_t lastflush, timework;
	fd_set fdset;
	TIMEVAL timeval;
	struct sockaddr_in servaddr;
#if OS_WIN32
	WSADATA versioninfo;
	CHAR *argvwork[MAXARGS];
#endif
#if OS_UNIX
	struct sigaction act;
#ifdef Linux
	syslog(LOG_INFO, "Starting");
#endif
#endif

#if OS_WIN32
	if (argc > 1) {  /* append additional arguments (can only happen from demand service startup) */
		for (i1 = i2 = 0; i1 < mainargc; i1++) if (i2 < MAXARGS - 1) argvwork[i2++] = mainargv[i1];
		for (i1 = 0; ++i1 < argc; ) if (i2 < MAXARGS - 1) argvwork[i2++] = argv[i1];
		argvwork[i2] = NULL;
		mainargc = i2;
		mainargv = argvwork;
	}
#endif

	/* get command line parameters */
	portnum = sslportnum = 0;
	cfgfilename[0] = '\0';
	outputname[0] = '\0';
	strcpy(fsbinary, mainargv[0]);
	for (i1 = (INT)strlen(fsbinary); i1 && fsbinary[i1 - 1] != '\\' && fsbinary[i1 - 1] != ':' && fsbinary[i1 - 1] != '/'; i1--);
	fsbinary[i1] = '\0';
#if OS_WIN32
	strcat(fsbinary, "dbcfsrun.exe");
#endif
#if OS_UNIX
	strcat(fsbinary, "dbcfsrun");
#endif
	for (i1 = 0; ++i1 < mainargc; ) {
		if (mainargv[i1][0] == '-') {
			for (i2 = 0; mainargv[i1][i2 + 1] && mainargv[i1][i2 + 1] != ' ' && i2 < (INT) (sizeof(work) - 1); i2++) {
				work[i2] = (CHAR) toupper(mainargv[i1][i2 + 1]);
			}
			work[i2] = '\0';
			switch (work[0]) {
			case 'D':
				if (!work[1]) {
					debuglevel = 1;
					fsflags |= FLAG_DEBUG;
					FILE *log = fopen("debug_ssl.txt", "w");
					tcpstartssllog(log);
				}
				else if (isdigit(work[1])) {
					debuglevel = (INT)strtol(work + 1, NULL, 10);
					fsflags |= FLAG_DEBUG;
				}
				else if (work[1] == '=' && isdigit(work[2])) {
					debuglevel = (INT)strtol(work + 2, NULL, 10);
					fsflags |= FLAG_DEBUG;
				}
				break;
			case 'F':
				if (work[1] == 'S' && mainargv[i1][3] == '=') {
					if (mainargv[i1][4]) strcpy(fsbinary, &mainargv[i1][4]);
				}
				break;
			case 'G':
				fsflags |= FLAG_NEWLOG;
				break;
			case 'M':
				fsflags |= FLAG_MANUAL;
				break;
			case 'O':
				if (!strcmp(work, "OLDPROTOCOL")) {
					oldProtocol = TRUE;
				}
				break;
			case 'X':
				fsflags |= FLAG_NOLOG;  // overrides logging entry in the config file
				break;
#if OS_UNIX
			case 'Y':
				break;
#endif
			}
		}
		else if (!cfgfilename[0]) strcpy(cfgfilename, mainargv[i1]);
	}
	if (!cfgfilename[0]) strcpy(cfgfilename, "dbcfs.cfg");
	if (outputname[0]) {
		outputfile = fopen(outputname, "w");
		if (outputfile == NULL) outputname[0] = '\0';
	}
	if (outputfile == NULL) outputfile = stdout;

#if OS_WIN32
	if (fsflags & FLAG_SERVICE) svcstatus(SVC_STARTING, 3000 + 1);
#endif
	memset(&fioparms, 0, sizeof(FIOPARMS));
	parsecfgfile(cfgfilename, adminpassword, work, logfilename, workdirname, showpassword, &portnum,
			&sslportnum, &fioparms);
	fioparms.cvtvolfnc = volmapfnc;
	ptr = fioinit(&fioparms, TRUE);
	if (ptr != NULL) death1(ptr);
	if (!portnum) portnum = CONNECTPORT;
	if (!sslportnum) sslportnum = CONNECTPORT + 1;
	if (!(fsflags & FLAG_NOSSL) && sslportnum == portnum) death1("Configuration conflict between nport and eport");

#if OS_WIN32
	if (fsflags & FLAG_SERVICE) svcstatus(SVC_STARTING, 60000 + 2);
#endif
	if (sportnum && ((portnum >= sportnum && portnum < sportnum + usermax) ||
		(!(fsflags & FLAG_NOSSL) && sslportnum >= sportnum && sslportnum < sportnum + usermax)))
	{
		death1("Configuration conflict between sport and nport or eport");
	}
	userinfo = (USERINFO *) malloc(usermax * sizeof(USERINFO));
	if (userinfo == NULL) death1("Insufficient memory");
	usercount = 0;

	/* set the signal handlers */
#if OS_WIN32
	if (fsflags & FLAG_SERVICE) svcstatus(SVC_STARTING, 2000 + 3);
	if ( (i1 = WSAStartup(MAKEWORD(2,2), &versioninfo)) ) death2("WSAStartup() failed", i1);
	if (!(fsflags & FLAG_SERVICE)) SetConsoleCtrlHandler((PHANDLER_ROUTINE) sigevent, TRUE);
	else {
		serviceevent = CreateEvent(NULL, TRUE, FALSE, NULL);
		SetHandleInformation(serviceevent, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
	}
	/*
	 * Create the Mutex to control access to the user USERINFO array
	 */
	hUserArrayMutex = CreateMutex(NULL, FALSE, NULL);
	if (hUserArrayMutex == NULL) {
		death1("Fatal Error - unable to create user array mutex");
	}
#endif

#if OS_UNIX
#ifdef Linux
	act.sa_sigaction = sigevent3;
	act.sa_flags = SA_SIGINFO;
#else
	act.sa_handler = sigevent;
	act.sa_flags = 0;
#endif
	sigemptyset(&act.sa_mask);
#ifdef SA_INTERRUPT
	act.sa_flags |= SA_INTERRUPT;
#endif
#ifdef Linux
	if (fsflags & FLAG_LOGSIGS) {
		if (sigaction(SIGTRAP, &act, &oldsigtrap) == -1) {
			oldsigtrap.sa_handler = SIG_DFL;
			sigemptyset(&oldsigtrap.sa_mask);
			oldsigtrap.sa_flags = 0;
		}
	}
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
#ifdef Linux
	act.sa_flags = SA_SIGINFO;
#else
	act.sa_flags = 0;
#endif
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

	/* open and listen on non-ssl port */
#if OS_WIN32
	if (fsflags & FLAG_SERVICE) svcstatus(SVC_STARTING, 2000 + 4);
#endif
	sockethandle1 = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockethandle1 == INVALID_SOCKET) death2("Unable to create main socket handle", ERRORVALUE());
#if OS_UNIX
	/* set close on exec */
	i1 = fcntl(sockethandle1, F_GETFD, 0);
	if (i1 != -1) fcntl(sockethandle1, F_SETFD, i1 | 0x01);
#endif
#if OS_WIN32
	if (fsflags & FLAG_SERVICE) svcstatus(SVC_STARTING, 2000 + 5);
#endif
	i1 = 1;
	setsockopt(sockethandle1, SOL_SOCKET, SO_REUSEADDR, (char *) &i1, sizeof(i1));
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = (unsigned short) htons((unsigned short) portnum);
	rc = bind(sockethandle1, (struct sockaddr *) &servaddr, sizeof(struct sockaddr_in));
	if (rc == -1) {
#if OS_WIN32
		rc = WSAGetLastError();
		if (rc == WSAEADDRINUSE) {
			sprintf(work, "Port number (%d) already in use", portnum);
			death1(work);
		}
		death2("dbcfs: bind failed", rc);
#else
		if (errno == EADDRINUSE) {
			sprintf(work, "Port number (%d) already in use", portnum);
			death1(work);
		}
		death2("dbcfs: bind failed", errno);
#endif
	}
#if OS_WIN32
	if (fsflags & FLAG_SERVICE) svcstatus(SVC_STARTING, 2000 + 6);
#endif
	rc = listen(sockethandle1, 100);
	if (rc == -1) death2("dbcfs: listen failed", ERRORVALUE());

	if (!(fsflags & FLAG_NOSSL)) {  /* open and listen on ssl port */
#if OS_WIN32
		if (fsflags & FLAG_SERVICE) svcstatus(SVC_STARTING, 2000 + 7);
#endif
		sockethandle2 = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sockethandle2 == INVALID_SOCKET) death2("Unable to create main socket handle", ERRORVALUE());
#if OS_UNIX
		/* set close on exec */
		i1 = fcntl(sockethandle2, F_GETFD, 0);
		if (i1 != -1) fcntl(sockethandle2, F_SETFD, i1 | 0x01);
#endif
#if OS_WIN32
		if (fsflags & FLAG_SERVICE) svcstatus(SVC_STARTING, 2000 + 8);
#endif
		i1 = 1;
		setsockopt(sockethandle2, SOL_SOCKET, SO_REUSEADDR, (char *) &i1, sizeof(i1));
		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = (unsigned short) htons((unsigned short) sslportnum);
		rc = bind(sockethandle2, (struct sockaddr *) &servaddr, sizeof(struct sockaddr_in));
		if (rc == -1) {
#if OS_WIN32
			rc = WSAGetLastError();
			if (rc == WSAEADDRINUSE) {
				sprintf(work, "Port number (%d) already in use", sslportnum);
				death1(work);
			}
			death2("dbcfs: bind failed", rc);
#else
			if (errno == EADDRINUSE) {
				sprintf(work, "Port number (%d) already in use", sslportnum);
				death1(work);
			}
			death2("dbcfs: bind failed", errno);
#endif
		}
#if OS_WIN32
		if (fsflags & FLAG_SERVICE) svcstatus(SVC_STARTING, 2000 + 9);
#endif
		rc = listen(sockethandle2, 100);
		if (rc == -1) death2("dbcfs: listen failed", ERRORVALUE());
	}
	else sockethandle2 = INVALID_SOCKET;

#if OS_WIN32
	/* create thread to wait for process terminations */
	if (CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) threadproc, NULL, 0, &threadid) == NULL) {
		death2("CreateThread failed", GetLastError());
	}
#endif

	if (logfilename[0]) {  /* open and write to log file */
#if OS_WIN32
		if (fsflags & FLAG_SERVICE) svcstatus(SVC_STARTING, 5000 + 10);
#endif

		/* create temp log file */
		if (workdirname[0]) strcpy(work, workdirname);
		else strcpy(work, ".");
#if OS_WIN32
		if (!GetTempFileName(work, "fs6", 0, tempfilename)) death1("unable to create temporary file");
		i1 = fioaopen(tempfilename, FIO_M_PRP, 1, &temphandle);
#elif OS_UNIX
		for (i2 = 0; i2 < 1000; i2++) {
			ptr = tempnam(work, "fs6");
			if (ptr == NULL) death1("unable to create temporary file");
			strcpy(tempfilename, ptr);
			free(ptr);
			/* exclusive read/write prepare
			 * fail if existing, create if new
			 */
			i1 = fioaopen(tempfilename, FIO_M_EFC, 1, &temphandle);
			if (i1 != ERR_EXIST) break;
		}
#endif
		if (i1 < 0) {
			CHAR t2work[128];
			strcpy(t2work, "unable to create/open temporary file, fioerr=");
			strcat(t2work, fioerrstr(i1));
			death1(t2work);
		}
		fioaclose(temphandle);
		i1 = fioaopen(tempfilename, FIO_M_SHR, 0, &temphandle);
		if (i1) {
			CHAR t2work[128];
			strcpy(t2work, "unable to open log file, fioerr=");
			strcat(t2work, fioerrstr(i1));
			death1(t2work);
		}
#if OS_UNIX
		/* set close on exec */
		i1 = fcntl(temphandle, F_GETFD, 0);
		if (i1 != -1) {
#ifdef FD_CLOEXEC
			fcntl(temphandle, F_SETFD, i1 | FD_CLOEXEC);
#else
			fcntl(temphandle, F_SETFD, i1 | 0x01);
#endif
		}
#endif

		if (!(fsflags & FLAG_NOLOG)) {  /* open real log file */
			/* exclusive read/write open */
			i1 = fioaopen(logfilename, FIO_M_EXC, 0, &loghandle);
			if (!i1) {
				fioalseek(loghandle, 0, 2, NULL);
				if ((fsflags & FLAG_NEWLOG) && renamelogfile(FALSE)) death1("unable to rename archive log file");
			}
			else {
				/* exclusive read/write prepare,
				 * truncate if existing, create if new
				 */
				i1 = fioaopen(logfilename, FIO_M_PRP, 1, &loghandle);
				if (i1) death1("unable to create log file");
				fsflags &= ~FLAG_NEWLOG;
			}
			if (!(fsflags & FLAG_NEWLOG)) {
#if OS_WIN32
				if (fsflags & FLAG_SERVICE) svcstatus(SVC_STARTING, 120000 + 11);
#endif
				i1 = writeStartingXML(loghandle);
//				memcpy(work, "<start>", 7);
//				msctimestamp((UCHAR *)(work + 7));
//				memcpy(work + 23, "</start>", 8);
//				i1 = fioawrite(loghandle, (UCHAR *) work, 31, -1, NULL);
				if (i1) death1("unable to write to log file");
			}
#if OS_UNIX
			/* set close on exec */
			i1 = fcntl(loghandle, F_GETFD, 0);
			if (i1 != -1) {
#ifdef FD_CLOEXEC
				fcntl(loghandle, F_SETFD, i1 | FD_CLOEXEC);
#else
				fcntl(loghandle, F_SETFD, i1 | 0x01);
#endif
			}
#endif
		}
		lastflush = time(NULL);
	}

	/* build verification string to pass back to dbcfsrun */
	/* 32 byte value, with first 16 bytes static with: */
	/*     [1] = pid, [2] = pid >> 16, [4] = pid >> 24, [6] = pid >> 8 (process id hash) */
	/*     [8-15] future use */
	/*     [18] = cid >> 16, [21] = cid >> 16, [22] = cid >> 24, [23] = cid (child id hash) */
	/*     [24] = time, [27] = time >> 24, [29] = time >> 8, [30] = time >> 16 (time id hash) */
	srand((unsigned int) time(NULL));
	for (i1 = 0; i1 < 32; i1 += 2) {
		i2 = rand();
		verification[i1] = (UCHAR) i2;
		verification[i1 + 1] = (UCHAR)(i2 >> 8);
	}
#if OS_WIN32
	/* set _DBCFS=<formatted pid> */
	i1 = GetCurrentProcessId();
	for (i2 = 0; i2 < 8; i1 >>= 4, i2++) work[i2] = (UCHAR)(i1 & 0x0F) + 'A';
	work[8] = '\0';
	if (!SetEnvironmentVariable("_DBCFS", work)) {
		int d2 = GetLastError();
		sprintf(work, "SetEnvironmentVariable(\"_DBCFS\", work) failed e=%d, work=%s", d2, work);
		death1(work);
	}
	i1 = GetCurrentProcessId();
#endif
#if OS_UNIX
	i1 = getpid();
#endif
	verification[1] = (UCHAR) i1;
	verification[6] = (UCHAR)(i1 >> 8);
	verification[2] = (UCHAR)(i1 >> 16);
	verification[4] = (UCHAR)(i1 >> 24);
	tcpitoa(portnum, serverport);

#if OS_WIN32
	if (fsflags & FLAG_SERVICE) svcstatus(SVC_RUNNING, 0);
#endif
	while (!(fsflags & FLAG_SHUTDOWN)) {
		if (temphandle != INVALID_HANDLE_VALUE) {
			timework = time(NULL);
			if ((INT) difftime(timework, lastflush) >= 30) {
				lastflush = timework;
				flushlogfile();
			}
		}
		if (fsflags & FLAG_DIED) {
			fsflags &= ~FLAG_DIED;
#if OS_WIN32
			WaitForSingleObject(hUserArrayMutex, INFINITE);
#endif
			for (i1 = 0; i1 < usercount; i1++) {
				if (!userinfo[i1].pid && userinfo[i1].sockethandle != INVALID_SOCKET) {
					memcpy(work, userinfo[i1].messageid, 8);
					memcpy(work + 8, "ERR99999", 8);
					strcpy(work + 24, "Server sub-process terminated during startup");
					i2 = (INT)strlen(work + 24);
					tcpiton(i2, (UCHAR *) work + 16, 8);
					if (fsflags & FLAG_DEBUG) {
						writestart();
						writeout("SEND: ", 6);
						writeout(work, 24 + i2);
						writefinish();
					}
					tcpsend(userinfo[i1].sockethandle, (UCHAR *) work, 24 + i2, userinfo[i1].flags, 10);
					if (userinfo[i1].flags & TCP_SSL) tcpsslcomplete(userinfo[i1].sockethandle);
					closesocket(userinfo[i1].sockethandle);
					userinfo[i1].sockethandle = INVALID_SOCKET;
				}
			}
#if OS_WIN32
			ReleaseMutex(hUserArrayMutex);
#endif
		}
		FD_ZERO(&fdset);
		FD_SET(sockethandle1, &fdset);
#ifdef Linux
		if (fsflags & FLAG_LOGSIGSX) FD_SET(pipefd[0], &fdset);
#endif
		INT tempsockethandle;
		// This cast is safe. The socket is a File Descriptor and must be a small number
		tempsockethandle = (INT) sockethandle1;
		if (sockethandle2 != INVALID_SOCKET) {
			FD_SET(sockethandle2, &fdset);
			if (sockethandle2 > (SOCKET)tempsockethandle) tempsockethandle = (INT) sockethandle2;
		}
		timeval.tv_sec = 5;
		timeval.tv_usec = 0;
		tempsockethandle = select(tempsockethandle + 1, &fdset, NULL, NULL, &timeval);
		/**
		 * select() returns zero if timeout expires.
		 * returns -1 if an error occurred.
		 * return >0 if any of the FDs are ready.
		 */
		if (tempsockethandle <= 0) continue;
		if (FD_ISSET(sockethandle1, &fdset)) {
			processaccept(sockethandle1, TCP_UTF8);
			if (tempsockethandle == 1) continue;
		}
		if (sockethandle2 != INVALID_SOCKET && FD_ISSET(sockethandle2, &fdset)) {
			processaccept(sockethandle2, TCP_UTF8 | TCP_SSL);
		}
#ifdef Linux
		if (fsflags & FLAG_LOGSIGSX && FD_ISSET(pipefd[0], &fdset)) {
			ssize_t i1;
			PIPEMESSAGE pm;
			UINT bytesRead = read(pipefd[0], &pm.size, sizeof(pm.size));
			if (fsflags & FLAG_DEBUG && debuglevel >= 4) {
				writestart();
				sprintf(work, "In main loop, pipe, bytesRead=%u", bytesRead);
				writeout(work, -1);
				writefinish();
			}
			if (bytesRead == sizeof(pm.size) && pm.size == sizeof(PIPEMESSAGE)) {
				UINT bytesToRead = sizeof(PIPEMESSAGE) - bytesRead;
				while (bytesToRead > 0) {
					i1 = read(pipefd[0], ((void*)&pm) + bytesRead, bytesToRead);
					bytesRead += i1;
					bytesToRead -= i1;
				}
				sprintf(work, "<signal code=\"%d\">%d</signal>", pm.si.si_code, pm.si.si_signo);
				i1 = fioawrite(loghandle, (UCHAR *) work, strlen(work), -1, NULL);
				if (i1) death1("unable to write to log file for signal");
			}
			else {
				/* Something went wrong, just empty the pipe */
				while (read(pipefd[0], &i1, sizeof(int32_t)) > 0);
			}
		}
#endif
	}
	/* The above While loop breaks only when FLAG_SHUTDOWN is set in fsflags */

	closeAndCleanUpLogFile();

	closesocket(sockethandle1);
	if (sockethandle2 != INVALID_SOCKET) closesocket(sockethandle2);

	/* stop any running dbcfsruns */
#if OS_WIN32
	if (fsflags & FLAG_SERVICE) {
		svcstatus(SVC_STOPPING, 3000 + 12);
		SetEvent(serviceevent);
	}
	WaitForSingleObject(hUserArrayMutex, INFINITE);
	for (i1 = i2 = 0; i1 < usercount; i1++) {
		if (!userinfo[i1].pid || userinfo[i1].pid == -1) continue;
		GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, (DWORD) userinfo[i1].pid);
		i2++;
	}
	ReleaseMutex(hUserArrayMutex);
#elif OS_UNIX
	for (i1 = i2 = 0; i1 < usercount; i1++) {
		if (!userinfo[i1].pid || userinfo[i1].pid == -1) continue;
		kill(userinfo[i1].pid, SIGTERM);
		i2++;
	}
#endif


	if (i2) {  /* at least one dbcfsrun */
#if OS_WIN32
		if (fsflags & FLAG_SERVICE) svcstatus(SVC_STOPPING, 35000 + 13);
		for (i2 = 0; i2 < 30; i2++) {  /* 30 second wait for dbcfsruns to shutdown */
			SLEEP(1000);
			WaitForSingleObject(hUserArrayMutex, INFINITE);
			for (i1 = 0; i1 < usercount && !userinfo[i1].pid; i1++);
			if (i1 == usercount) {
				ReleaseMutex(hUserArrayMutex);
				break;
			}
			if (i2 == 20) {  /* cause a more forceful shutdown to occur */
				for (i1 = 0; i1 < usercount; i1++) {
					if (!userinfo[i1].pid || userinfo[i1].pid == -1) continue;
					GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, (DWORD) userinfo[i1].pid);
				}
			}
			ReleaseMutex(hUserArrayMutex);
		}
		if (i2 == 30) {  /* resort to ungraceful kill */
			for (i1 = 0; i1 < usercount; i1++) {
				if (!userinfo[i1].pid || userinfo[i1].pid == -1) continue;
				TerminateProcess(userinfo[i1].phandle, 0);
				CloseHandle(userinfo[i1].phandle);
			}
		}
#endif
#if OS_UNIX
		for (i2 = 0; i2 < 30; i2++) {  /* 30 second wait for dbcfsruns to shutdown */
			SLEEP(1000);
			for (i1 = 0; i1 < usercount && !userinfo[i1].pid; i1++);
			if (i1 == usercount) break;
			if (i2 == 20) {  /* cause a more forceful shutdown to occur */
				for (i1 = 0; i1 < usercount; i1++) {
					if (userinfo[i1].pid > 0) kill(userinfo[i1].pid, SIGTERM);
				}
			}
		}
		if (i2 == 30) {  /* resort to ungraceful kill */
			for (i1 = 0; i1 < usercount; i1++) {
				if (!userinfo[i1].pid || userinfo[i1].pid == -1) continue;
				kill(userinfo[i1].pid, SIGKILL);
			}
		}
#endif
	}

	if (volumes != NULL) {
		for (i1 = 0; i1 < volcount; i1++) memfree((UCHAR **)(*volumes)[i1].path);
		memfree((UCHAR **)volumes);
		volumes = NULL;
	}
	fioexit();
#if OS_WIN32
	WSACleanup();
	if (fsflags & FLAG_SERVICE) svcstatus(SVC_STOPPED, 0);
	SetConsoleCtrlHandler((PHANDLER_ROUTINE) sigevent, FALSE);
#endif

#if OS_UNIX
	sigaction(SIGTERM, &oldsigterm, NULL);
	sigaction(SIGINT, &oldsigint, NULL);
	if (oldsighup.sa_handler != SIG_IGN) sigaction(SIGHUP, &oldsighup, NULL);
	sigaction(SIGPIPE, &oldsigpipe, NULL);
	sigaction(SIGCHLD, &oldsigchld, NULL);
#ifdef Linux
	if (fsflags & FLAG_LOGSIGS) {
		sigaction(SIGTRAP, &oldsigtrap, NULL);
	}
	closelog();
#endif
#endif
	return 0;
}

/*
 * close and clean up the log file
 *
 * <signal code="0" pid="2379" uid="0">15</signal>
 * <pidname>dbcfs</pidname>
 * <stop>2012112100102266</stop>
 */
static void closeAndCleanUpLogFile() {
	CHAR work[128];
	INT i1;
	if (temphandle != INVALID_HANDLE_VALUE) {
		if (!(fsflags & FLAG_NOLOG)) {
			if (loghandle == INVALID_HANDLE_VALUE || flushlogfile()) death1("unable to flush log data");
#if defined(Linux)
			if ((fsflags & FLAG_LOGSIGS) && sig_received) {
				CHAR pidname[64];
				sprintf(work, "<signal code=\"%d\" pid=\"%d\" uid=\"%u\">%d</signal>", siginfo.si_code,
						siginfo.si_pid, siginfo.si_uid, sig_received);
				i1 = fioawrite(loghandle, (UCHAR *) work, strlen(work), -1, NULL);
				if (i1) death1("unable to write to log file");
				tryGetPidNameViaProc(siginfo.si_pid, pidname, ARRAYSIZE(pidname));
				if (pidname[0] != '\0') {
					sprintf(work, "<pidname>%s</pidname>", pidname);
					i1 = fioawrite(loghandle, (UCHAR *) work, strlen(work), -1, NULL);
					if (i1) death1("unable to write to log file");
				}
				tryGetPidNameViaPopen(siginfo.si_pid, pidname, ARRAYSIZE(pidname));
				if (pidname[0] != '\0') {
					sprintf(work, "<pidcmdline>%s</pidcmdline>", pidname);
					i1 = fioawrite(loghandle, (UCHAR *) work, strlen(work), -1, NULL);
					if (i1) death1("unable to write to log file");
				}
			}
#endif
			memcpy(work, "<stop>", 6);
			msctimestamp((UCHAR *)(work + 6));
			memcpy(work + 22, "</stop>", 7);
			i1 = fioawrite(loghandle, (UCHAR *) work, 29, -1, NULL);
			if (i1) death1("unable to write to log file");
			fioaclose(loghandle);
		}
		fioaclose(temphandle);
		/* wait another 20 seconds to delete temp file */
		for (i1 = 0; i1 < 20 && fioadelete(tempfilename); i1++) SLEEP(1000);
	}
}

#if defined(Linux)
static char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace(*str)) str++;

  if(*str == '\0')  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;

  // Write new null terminator
  *(end+1) = '\0';

  return str;
}

/**
 * Given a PID, try to get the name of the process via the proc mechanism
 */
static void tryGetPidNameViaProc(pid_t pid, CHAR *name, INT cbname)
{
	char buff[1048576];
	char fname[128];
	FILE *fp;
	name[0] = '\0';
    sprintf(fname, "/proc/%d/status", (int)pid);
    fp = fopen(fname, "r");
    if(fp == NULL) return;
    while(fgets(buff, 1048576, fp)) {
    	if(!strncmp(buff, "Name:", 5)) {
    		strncpy(name, trimwhitespace(buff + 5), cbname);
    		break;
    	}
    }
    fclose(fp);
    return;
}

/**
 * Given a PID, try to get the name of the process via popen function
 */
static void tryGetPidNameViaPopen(pid_t pid, CHAR *name, INT cbname)
{
	char buff[2048];
	char work[64];
	FILE *fp;
	name[0] = '\0';
	sprintf(work, "/bin/ps -fwp %u|cut -c 49-120", (unsigned int) pid);
	fp = popen(work, "r");
    if(fp == NULL) return;
    fgets(buff, 2048, fp);
    fgets(buff, 2048, fp);
	pclose(fp);
    strncpy(name, trimwhitespace(buff), cbname);
    return;
}

#endif

/* process a client message */
static void processaccept(SOCKET sockethandle, INT tcpflags)
{
	INT i1, i2, bufpos, cnt, firstflag, len, nextoffset, offset, port, usernum, type;
	CHAR cmdline[768], errormsg[128], work[256];
	UCHAR workbuffer[4096 * 2], *ptr, *ptr1;
	SOCKET worksockethandle;
	struct sockaddr_in address;
	struct linger lingstr;
#if (__USLC__ || defined(_SEQUENT_))
	size_t addrsize;
#else
	INT addrsize;
#endif
#if OS_WIN32
	STARTUPINFO sinfo;
	PROCESS_INFORMATION pinfo;
#endif
#if OS_UNIX
	INT argc, i3;
	CHAR *argv[32];
	pid_t pid;
	CHAR tstamp[17];
#endif

	worksockethandle = accept(sockethandle, NULL, NULL);
#if defined(Linux)
	if (fsflags & FLAG_LOGACCEPT && loghandle != INVALID_HANDLE_VALUE) {
		memset(tstamp, '\0', sizeof(tstamp));
		msctimestamp((UCHAR *)tstamp);
		if (worksockethandle == INVALID_SOCKET) {
			sprintf(work, "<accept>%s failed: %i</accept>", tstamp, ERRORVALUE());
		}
		else sprintf(work, "<accept>%s</accept>", tstamp);
		i1 = fioawrite(loghandle, (UCHAR *) work, strlen(work), -1, NULL);
		if (i1) {
			strcpy(errormsg, "Unable to write to log file");
			goto processerror;
		}
	}
#endif
	if (worksockethandle == INVALID_SOCKET) {
		if (fsflags & FLAG_DEBUG) {
			writestart();
			sprintf(work, "In processaccept, accept failed: %i", ERRORVALUE());
			writeout(work, -1);
			writefinish();
		}
		return;
	}
	lingstr.l_onoff = 1;
/*** NOTE: THIS IS SECONDS, EXCEPT LINUX MAY BE/WAS HUNDREDTHS ***/
	lingstr.l_linger = 3;
	setsockopt(worksockethandle, SOL_SOCKET, SO_LINGER, (char *) &lingstr, sizeof(lingstr));
	i1 = 1;
	setsockopt(worksockethandle, IPPROTO_TCP, TCP_NODELAY, (char *) &i1, sizeof(i1));

	if (tcpflags & TCP_SSL) {
		i1 = tcpsslsinit(worksockethandle, GetCertBio(certificatefilename) /*NULL*/);
		if (i1 < 0) {
			if (fsflags & FLAG_DEBUG) {
				writestart();
				writeout("tcpsslsinit() failed: ", -1);
				writeout(tcpgeterror(), -1);
				writefinish();
			}
			tcpsslcomplete(worksockethandle);
			closesocket(worksockethandle);
			return;
		}
	}

	for (bufpos = 0; ; ) {
		i1 = tcprecv(worksockethandle, workbuffer + bufpos, sizeof(workbuffer) - 1 - bufpos, tcpflags, 10);
		if (i1 <= 0) {
			if (fsflags & FLAG_DEBUG) {
				writestart();
				if (i1 == 0) writeout("receive timed out on connecting socket", -1);
				else {
					writeout("receive failed: ", -1);
					writeout(tcpgeterror(), -1);
				}
				writefinish();
			}
			if (tcpflags & TCP_SSL) tcpsslcomplete(worksockethandle);
			closesocket(worksockethandle);
			return;
		}
		bufpos += i1;
		if (bufpos >= 40) {
			tcpntoi(workbuffer + 32, 8, &len);
			if (len < 0 || len > (INT) (sizeof(workbuffer) - 41)) {
				if (fsflags & FLAG_DEBUG) {
					writestart();
					writeout("invalid data received", -1);
					writefinish();
				}
				strcpy(errormsg, "Invalid data received");
				goto processerror;
			}
			if (bufpos >= 40 + len) break;
		}
	}
	if ((fsflags & FLAG_DEBUG) && memcmp(workbuffer + 24, "VERIFY  ", 8)) {
		writestart();
		writeout("RECV: ", 6);
		writeout((CHAR *) workbuffer, 40 + len);
		writefinish();
	}

	/* process request message */
	if (!memcmp(workbuffer + 24, "VERIFY  ", 8)) {
		/* finish verification string to pass back to dbcfsrun */
		/* 32 byte value, with first 16 bytes static with: */
		/*     [1] = pid, [2] = pid >> 16, [4] = pid >> 24, [6] = pid >> 8 (process id hash) */
		/*     [8-15] future use */
		/*     [18] = cid >> 16, [21] = cid >> 16, [22] = cid >> 24, [23] = cid (child id hash) */
		/*     [24] = time, [27] = time >> 24, [29] = time >> 8, [30] = time >> 16 (time id hash) */
		tcpntoi(workbuffer + 8, 8, &usernum);
		usernum--;
		if (usernum >= 0 && usernum < usercount) i1 = userinfo[usernum].pid;
		else i1 = 0;
		verification[23] = (UCHAR) i1;
		verification[18] = (UCHAR)(i1 >> 8);
		verification[21] = (UCHAR)(i1 >> 16);
		verification[22] = (UCHAR)(i1 >> 24);
		i1 = (INT) time(NULL);
		verification[24] = (UCHAR) i1;
		verification[29] = (UCHAR)(i1 >> 8);
		verification[30] = (UCHAR)(i1 >> 16);
		verification[27] = (UCHAR)(i1 >> 24);
		memcpy(workbuffer + 8, "OK            32", 16);
		memcpy(workbuffer + 24, verification, 32);
#ifdef _DEBUG
		if (fsflags & FLAG_DEBUG) {
			CHAR work1[128];
			CHAR work2[256];
			writestart();
			writeout("SEND: ", 6);
			writeout((CHAR *) workbuffer, 24);
			writefinish();
			writestart();
			work2[0] = '\0';
			sprintf(work1, "(04) %#2.2x, (02) %#2.2x, (06) %#2.2x, (01) %#2.2x;  "
					"(22) %#2.2x, (21) %#2.2x, (18) %#2.2x, (23) %#2.2x, ",
					verification[4], verification[2], verification[6], verification[1],
					verification[22], verification[21], verification[18], verification[23]
					);
			strcat(work2, "      ");
			strcat(work2, work1);
			writeout(work2, (int)strlen(work2));
			sprintf(work1, "userinfo[%d]=%d", usernum, userinfo[usernum].pid);
			work2[0] = '\0';
			strcat(work2, "      ");
			strcat(work2, work1);
			writefinish();
			writestart();
			writeout(work2, (int)strlen(work2));
			writefinish();
		}
#endif
		i1 = tcpsend(worksockethandle, workbuffer, 24 + 32, tcpflags, 10);
		if (i1 < 0) {
			ptr = (UCHAR*)tcpgeterror();
			sprintf(errormsg, "In processaccept(A), tcperror=%s", ptr);
			goto processerror;
		}
		if (usernum >= 0 && usernum < usercount) {
			if (sportnum && userinfo[usernum].sockethandle != INVALID_SOCKET) {
				memcpy(workbuffer, userinfo[usernum].messageid, 8);
				if (userinfo[usernum].pid) {
					memcpy(workbuffer + 8, "OK      ", 8);
					tcpitoa(sportnum + usernum, (CHAR *) workbuffer + 24);
				}
				else {  /* failed during this transmission (unlikely) */
					memcpy(workbuffer + 8, "ERR99999", 8);
					strcpy((CHAR *) workbuffer + 24, "Server sub-process terminated during startup");
				}
				i1 = (INT)strlen((CHAR *) workbuffer + 24);
				tcpiton(i1, workbuffer + 16, 8);
				if (fsflags & FLAG_DEBUG) {
					writestart();
					writeout("SEND: ", 6);
					writeout((CHAR *) workbuffer, 24 + i1);
					writefinish();
				}
				i2 = tcpsend(userinfo[usernum].sockethandle, workbuffer, 24 + i1, userinfo[usernum].flags, 10);
				if (i2 < 0) {
					ptr = (UCHAR*)tcpgeterror();
					sprintf(errormsg, "In processaccept(B), tcperror=%s", ptr);
					goto processerror;
				}
				if (userinfo[usernum].flags & TCP_SSL) tcpsslcomplete(userinfo[usernum].sockethandle);
				closesocket(userinfo[usernum].sockethandle);
				userinfo[usernum].sockethandle = INVALID_SOCKET;
			}
			userinfo[usernum].flags = 0;
		}
		if (fsflags & FLAG_VERIFY) {
			fsflags &= ~FLAG_VERIFY;
			for (i1 = 0; i1 < usercount; i1++) {
				if (!userinfo[i1].pid) continue;
				if (userinfo[i1].sockethandle != INVALID_SOCKET || userinfo[i1].flags == FLAG_VERIFY)
					fsflags &= ~FLAG_VERIFY;
			}
		}
	}

	else if ((fsflags & FLAG_SSLONLY) && !(tcpflags & TCP_SSL)) {
		/* message came in on non-ssl port */
		strcpy(errormsg, "Non-encrypted connections not supported");
		goto processerror;
	}
	else if (!memcmp(workbuffer + 24, "HELLO   ", 8 * sizeof(CHAR))) {
		memcpy(workbuffer + 8, "OK      ", 8 * sizeof(CHAR));
		memcpy(workbuffer + 24, "DB/C FS ", 8 * sizeof(CHAR));
		strcpy((CHAR *)(workbuffer + 32), oldProtocol ? "05.00" : FS_ODBC_VERSION);
		i1 = (INT)strlen((CHAR *) workbuffer + 24);
		tcpiton(i1, workbuffer + 16, 8);
		if (fsflags & FLAG_DEBUG) {
			writestart();
			writeout("SEND: ", 6);
			writeout((CHAR *) workbuffer, 24 + i1);
			writefinish();
		}
		tcpsend(worksockethandle, workbuffer, 24 + i1, tcpflags, 10);
	}
	else if (!memcmp(workbuffer + 24, "START   ", 8 * sizeof(CHAR))) {
		/* get port number */
		len += 40;
		offset = 40;
		i1 = tcpnextdata(workbuffer, len, &offset, &nextoffset);
		if (i1 > 0) tcpntoi(workbuffer + offset, i1, &port);
		if (i1 <= 0 || port < 0) {
			strcpy(errormsg, "Invalid port number");
			goto processerror;
		}

		if (!port && !sportnum) {
			strcpy(errormsg, "Client port number specified as zero, but \"sport\" not defined");
			goto processerror;
		}

		/*
		 * This is to make sure that the thread scanning for dead processes (threadproc) has run at least
		 * once before we start a new process. It should not be set lower than 1/8 second
		 */
		SLEEP(startDelayInMilliseconds); /* default value is 1/4 second */

		/* Examine table of processes to set usercount */
#if OS_WIN32
		WaitForSingleObject(hUserArrayMutex, INFINITE);
#endif
		/* find first unused position */
		for (usernum = 0; usernum < usercount && userinfo[usernum].pid; usernum++);
#if OS_WIN32
		ReleaseMutex(hUserArrayMutex);
#elif OS_UNIX
		if (usernum == usermax) {
			checkchildren();
			for (usernum = 0; usernum < usercount && userinfo[usernum].pid; usernum++);
		}
#endif
		if (usernum == usermax) {
			usermax += 10;
			void* ptr2 = realloc(userinfo, usermax * sizeof(USERINFO));
			if (ptr2 == NULL) death1("Insufficient memory");
			userinfo = (USERINFO*) ptr2;
			//strcpy(errormsg, "Exceeded user license");
			//goto processerror;
		}
		if (usernum < usercount && userinfo[usernum].sockethandle != INVALID_SOCKET) {  /* unlikely, but just in case */
			memcpy(work, userinfo[usernum].messageid, 8);
			memcpy(work + 8, "ERR99999", 8);
			strcpy(work + 24, "Server sub-process terminated during startup");
			i1 = (INT)strlen(work + 24);
			tcpiton(i1, (UCHAR *) work + 16, 8);
			if (fsflags & FLAG_DEBUG) {
				writestart();
				writeout("SEND: ", 6);
				writeout(work, 24 + i1);
				writefinish();
			}
			tcpsend(userinfo[usernum].sockethandle, (UCHAR *) work, 24 + i1, userinfo[usernum].flags, 10);
			if (userinfo[usernum].flags & TCP_SSL) tcpsslcomplete(userinfo[usernum].sockethandle);
			closesocket(userinfo[usernum].sockethandle);
		}

		/* get user (informational only) */
		i1 = tcpnextdata(workbuffer, len, &nextoffset, NULL);
		if (i1 > 0) {
			if ((size_t) i1 > (sizeof(userinfo->user) - 1) / sizeof(CHAR)) i1 = (sizeof(userinfo->user) - 1) / sizeof(CHAR);
			memcpy(userinfo[usernum].user, workbuffer + nextoffset, i1);
		}
		else i1 = 0;
		userinfo[usernum].user[i1] = '\0';
		userinfo[usernum].sockethandle = INVALID_SOCKET;
		addrsize = sizeof(address);

		/**
		 * getpeername is in sys/socket.h or winsock2.h
		 * The third parameter differs between Windows and Unix. Sheesh!
		 */
		i1 = getpeername(worksockethandle, (struct sockaddr *) &address,
#if OS_UNIX
				(socklen_t *)
#else
				(int*)
#endif
				&addrsize);
		if (i1 == SOCKET_ERROR) {
			sprintf(errormsg, "getpeername() failed, error = %d", ERRORVALUE());
			goto processerror;
		}

		strcpy(cmdline, fsbinary);
		i1 = (INT)strlen(cmdline);
		cmdline[i1++] = ' ';
#ifdef NO_INET_NTOA
		{
			static char bufferab[16];
			unsigned int q24 = (unsigned int)address.sin_addr.s_addr;
			unsigned short c1 = q24 >> 24;
			unsigned short c2 = (q24 & 0x00FF0000) >> 16;
			unsigned short c3 = (q24 & 0x0000FF00) >> 8;
			unsigned short c4 = q24 & 0x000000FF;
			sprintf(bufferab, "%hu.%hu.%hu.%hu", c1, c2, c3, c4);
			strcpy(cmdline + i1, bufferab);
		}
#else
		strcpy(cmdline + i1, inet_ntoa(address.sin_addr));
#endif
		i1 += (INT)strlen(cmdline + i1);
		cmdline[i1++] = ' ';
		if (!port) i1 += tcpitoa(sportnum + usernum, cmdline + i1);
		else i1 += tcpitoa(port, cmdline + i1);
		cmdline[i1++] = ' ';
		strcpy(cmdline + i1, cfgfilename);
		i1 += (INT)strlen(cmdline + i1);
		cmdline[i1++] = ' ';
		strcpy(cmdline + i1, serverport);
		i1 += (INT)strlen(cmdline + i1);
		cmdline[i1++] = ':';
		i1 += tcpitoa(usernum + 1, cmdline + i1);
		if (tcpflags & TCP_SSL) {
			strcpy(cmdline + i1, " -ssl");
			i1 += (INT)strlen(cmdline + i1);
		}
		if (!port) {
			strcpy(cmdline + i1, " -sport");
			i1 += (INT)strlen(cmdline + i1);
		}
		if (temphandle != INVALID_HANDLE_VALUE) {
			if (!(fsflags & FLAG_NOLOG) && (loghandle == INVALID_HANDLE_VALUE || tempfilepos)) {
				strcpy(errormsg, "operational problems with logfile");  /* Might be out of disk space */
				goto processerror;
			}
			strcpy(cmdline + i1, " -l=");
			i1 += (INT)strlen(cmdline + i1);
			strcpy(cmdline + i1, tempfilename);
			i1 += (INT)strlen(cmdline + i1);
		}
#if OS_WIN32
		if (fsflags & FLAG_SERVICE) {
			strcpy(cmdline + i1, " -event=");
			i1 += (INT)strlen(cmdline + i1);
			i1 += tcpitoa((intptr_t) serviceevent, cmdline + i1);
		}
#endif

		if (fsflags & FLAG_DEBUG) {
			if (debuglevel > 0) {
				strcpy(cmdline + i1, " -d");
				i1 += (INT)strlen(cmdline + i1);
				i1 += tcpitoa(debuglevel, cmdline + i1);
			}
			if (!(fsflags & FLAG_MANUAL) && debuglevel < 0) {
				writestart();
				writeout("EXEC: ", -1);
				writeout(cmdline, -1);
				writefinish();
			}
		}

		if (!(fsflags & FLAG_MANUAL)) {
#if OS_WIN32
			GetStartupInfo(&sinfo);
			if (fsflags & FLAG_DEBUG) {
				writestart();
				writeout("CPDB: ", 6);
				writeout((CHAR *) cmdline, (INT)strlen(cmdline));
				writefinish();
			}
			if (!CreateProcess(NULL, cmdline, NULL, NULL, TRUE, CREATE_NEW_PROCESS_GROUP, NULL, NULL, &sinfo, &pinfo)) {
				sprintf(errormsg, "CreateProcess() failed, error = %d", (int)GetLastError());
				if (fsflags & FLAG_DEBUG) {
					writestart();
					writeout("CPDB: ", 6);
					writeout(errormsg, (INT)strlen(errormsg));
					writefinish();
				}
				goto processerror;
			}
			CloseHandle(pinfo.hThread);
			WaitForSingleObject(hUserArrayMutex, INFINITE);
			userinfo[usernum].pid = pinfo.dwProcessId;
			if ((fsflags & FLAG_DEBUG) && debuglevel >= 2) {
				writestart();
				writeout("DBG:  ", 6);
				sprintf(errormsg, "After CreateProcess() success, userinfo[%d].pid = %d", usernum, userinfo[usernum].pid);
				writeout(errormsg, (INT)strlen(errormsg));
				writefinish();
			}
			userinfo[usernum].phandle = pinfo.hProcess;
			if (usernum == usercount) {
				usercount++;
				if ((fsflags & FLAG_DEBUG) && debuglevel >= 2) {
					writestart();
					writeout("DBG:  ", 6);
					sprintf(errormsg, "After CreateProcess() success, usercount is now %d", usercount);
					writeout(errormsg, (INT)strlen(errormsg));
					writefinish();
				}
			}
			ReleaseMutex(hUserArrayMutex);
			//SetEvent(threadevent);
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
				cmdline[i2] = '\0';
			}
			argv[argc] = NULL;

/*** CODE: VFORK IS MORE EFFIECIENT, BUT IT'S USE IS NOT AS COMMON ***/
			if ((pid = fork()) == (pid_t) -1) {  /* fork failed */
				sprintf(errormsg, "fork() failed, error = %d", errno);
				goto processerror;
			}
			if (pid == (pid_t) 0) {  /* child */
				closesocket(worksockethandle);
				if (sockethandle1 != INVALID_SOCKET) closesocket(sockethandle1);
				if (sockethandle2 != INVALID_SOCKET) closesocket(sockethandle2);
				if (temphandle != -1) fioaclose(temphandle);
				if (loghandle != -1) fioaclose(loghandle);
				execvp(argv[0], argv);
				if (fsflags & FLAG_DEBUG) {
					writestart();
					writeout("execvp failed", -1);
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
			userinfo[usernum].pid = -1;
			if (usernum == usercount) usercount++;
		}
		fsflags |= FLAG_VERIFY;
		if (!port) {
			userinfo[usernum].sockethandle = worksockethandle;
			userinfo[usernum].flags = tcpflags;
			memcpy(userinfo[usernum].messageid, workbuffer, 8);
			return;
		}
		userinfo[usernum].flags = FLAG_VERIFY;
		memcpy(workbuffer + 8, "OK             0", 16);
		if (fsflags & FLAG_DEBUG) {
			writestart();
			writeout("SEND: ", 6);
			writeout((CHAR *) workbuffer, 24);
			writefinish();
		}
		tcpsend(worksockethandle, workbuffer, 24, tcpflags, 10);
	}
	else if (!memcmp(workbuffer + 24, "SHUTDOWN", 8)) {
		offset = 40;
		i1 = tcpnextdata(workbuffer, len + 40, &offset, NULL);
		if (i1 < 0 || !adminpassword[0]) i1 = 0;
		ptr = workbuffer + offset;
		ptr[i1] = '\0';
		for (i2 = 0; i2 < i1 && toupper(ptr[i2]) == toupper(adminpassword[i2]); i2++);
		if (ptr[i2] != adminpassword[i2]) {
			strcpy(errormsg, "Invalid admin password");
			goto processerror;
		}
		memcpy(workbuffer + 8, "OK             0", 16);
		if (fsflags & FLAG_DEBUG) {
			writestart();
			writeout("SEND: ", 6);
			writeout((CHAR *) workbuffer, 24);
			writefinish();
		}
		tcpsend(worksockethandle, workbuffer, 24, tcpflags, 10);
		fsflags |= FLAG_SHUTDOWN;
	}
	else if (!memcmp(workbuffer + 24, "SHOWUSRS", 8)) {
		offset = 40;
		i1 = tcpnextdata(workbuffer, len + 40, &offset, NULL);
		if (i1 < 0 || !showpassword[0]) i1 = 0;
		ptr = workbuffer + offset;
		ptr[i1] = '\0';
		for (i2 = 0; i2 < i1 && toupper(ptr[i2]) == toupper(showpassword[i2]); i2++);
		if (ptr[i2] != showpassword[i2]) {
			strcpy(errormsg, "Invalid show password");
			goto processerror;
		}
#if UNIX
		checkchildren();
#elif OS_WIN32
		WaitForSingleObject(hUserArrayMutex, INFINITE);
#endif
		for (len = -1, cnt = 0; cnt < usercount; cnt++)
			if (userinfo[cnt].pid) len += tcpquotedlen((UCHAR *) userinfo[cnt].user, -1) + 1;
		if (len != -1) {
			memcpy(workbuffer + 8, "OK      ", 8);
			tcpiton(len, workbuffer + 16, 8);
			if (fsflags & FLAG_DEBUG) {
				writestart();
				writeout("SEND: ", 6);
			}
			for (firstflag = TRUE, i2 = 24, cnt = 0; cnt <= usercount; cnt++) {
				if ((size_t) i2 > sizeof(workbuffer) - (sizeof(userinfo->user) << 1) - 2 || cnt == usercount) {
					if (fsflags & FLAG_DEBUG) writeout((CHAR *) workbuffer, i2);
					i1 = tcpsend(worksockethandle, workbuffer, i2, tcpflags, 20);
					if (i1 != i2) {
						if (fsflags & FLAG_DEBUG) {
							writeout(" *** FAILED ***", -1);
							writefinish();
						}
						if (tcpflags & TCP_SSL) tcpsslcomplete(worksockethandle);
						closesocket(worksockethandle);
						return;
					}
					len -= i2;
					if (cnt == usercount) {
						if (len > 0) {  /* users disconnected during this call, blank fill rest of length */
							i2 = len;
							if (i2 > (INT) sizeof(workbuffer)) i2 = sizeof(workbuffer);
							memset(workbuffer, ' ', i2);
							cnt--;
							continue;
						}
						break;
					}
					i2 = 0;
				}
				if (userinfo[cnt].pid) {
					if (!firstflag) workbuffer[i2++] = ' ';
					else firstflag = FALSE;
					i2 += tcpquotedcopy(workbuffer + i2, (UCHAR *) userinfo[cnt].user, -1);
				}
			}
			if (fsflags & FLAG_DEBUG) writefinish();
		}
		else {
			memcpy(workbuffer + 8, "OK             0", 16);
			if (fsflags & FLAG_DEBUG) {
				writestart();
				writeout("SEND: ", 6);
				writeout((CHAR *) workbuffer, 24);
				writefinish();
			}
			tcpsend(worksockethandle, workbuffer, 24, tcpflags, 15);
		}
#if OS_WIN32
		ReleaseMutex(hUserArrayMutex);
#endif
	}
	else if (!memcmp(workbuffer + 24, "NEWLOG  ", 8)) {
		offset = 40;
		i1 = tcpnextdata(workbuffer, len + 40, &offset, NULL);
		if (i1 < 0 || !adminpassword[0]) i1 = 0;
		ptr = workbuffer + offset;
		ptr[i1] = '\0';
		for (i2 = 0; i2 < i1 && toupper(ptr[i2]) == toupper(adminpassword[i2]); i2++);
		if (ptr[i2] != adminpassword[i2]) {
			strcpy(errormsg, "Invalid admin password");
			goto processerror;
		}
		if (!logfilename[0]) {
			strcpy(errormsg, "Log file not specified in CFG file");
			goto processerror;
		}
		if (flushlogfile() && !(fsflags & FLAG_NOLOG)) {
			strcpy(errormsg, "Unable to create/write archive log file");
			goto processerror;
		}
		if (loghandle == INVALID_HANDLE_VALUE) {  /* open/create real log file */
			i1 = fioaopen(logfilename, FIO_M_EXC, 0, &loghandle);
			if (!i1) {
				fioalseek(loghandle, 0, 2, NULL);
				if (renamelogfile(FALSE)) {
					strcpy(errormsg, "Unable to create/write archive log file");
					goto processerror;
				}
			}
			else {
				i1 = fioaopen(logfilename, FIO_M_PRP, 1, &loghandle);
				if (i1) {
					strcpy(errormsg, "Unable to create log file");
					goto processerror;
				}
				i1 = writeStartingXML(loghandle);
				if (i1) {
					fioaclose(loghandle);
					loghandle = INVALID_HANDLE_VALUE;
					strcpy(errormsg, "Unable to write to log file");
					goto processerror;
				}
			}
		}
		else if (renamelogfile(TRUE)) {
			strcpy(errormsg, "Unable to create/write archive log file");
			goto processerror;
		}
#if OS_UNIX
		/* set close on exec */
		i1 = fcntl(loghandle, F_GETFD, 0);
		if (i1 != -1) fcntl(loghandle, F_SETFD, i1 | 0x01);
#endif
		fsflags &= ~FLAG_NOLOG;
		memcpy(workbuffer + 8, "OK             0", 16);
		if (fsflags & FLAG_DEBUG) {
			writestart();
			writeout("SEND: ", 6);
			writeout((CHAR *) workbuffer, 24);
			writefinish();
		}
		tcpsend(worksockethandle, workbuffer, 24, tcpflags, 10);
	}
	else if (!memcmp(workbuffer + 24, "STARTLOG", 8)) {
		offset = 40;
		i1 = tcpnextdata(workbuffer, len + 40, &offset, NULL);
		if (i1 < 0 || !adminpassword[0]) i1 = 0;
		ptr = workbuffer + offset;
		ptr[i1] = '\0';
		for (i2 = 0; i2 < i1 && toupper(ptr[i2]) == toupper(adminpassword[i2]); i2++);
		if (ptr[i2] != adminpassword[i2]) {
			strcpy(errormsg, "Invalid admin password");
			goto processerror;
		}
		if (!logfilename[0]) {
			strcpy(errormsg, "Log file not specified in CFG file");
			goto processerror;
		}
		flushlogfile();
		if (loghandle == INVALID_HANDLE_VALUE) {  /* open/create real log file */
			i1 = fioaopen(logfilename, FIO_M_EXC, 0, &loghandle);
			if (!i1) fioalseek(loghandle, 0, 2, NULL);
			else {
				i1 = fioaopen(logfilename, FIO_M_PRP, 1, &loghandle);
				if (i1) {
					strcpy(errormsg, "Unable to create log file");
					goto processerror;
				}
				i1 = writeStartingXML(loghandle);
				if (i1) {
					fioaclose(loghandle);
					loghandle = INVALID_HANDLE_VALUE;
					strcpy(errormsg, "Unable to write to log file");
					goto processerror;
				}
			}
#if OS_UNIX
			/* set close on exec */
			i1 = fcntl(loghandle, F_GETFD, 0);
			if (i1 != -1) fcntl(loghandle, F_SETFD, i1 | 0x01);
#endif
		}
		fsflags &= ~FLAG_NOLOG;
		memcpy(workbuffer + 8, "OK             0", 16);
		if (fsflags & FLAG_DEBUG) {
			writestart();
			writeout("SEND: ", 6);
			writeout((CHAR *) workbuffer, 24);
			writefinish();
		}
		tcpsend(worksockethandle, workbuffer, 24, tcpflags, 10);
	}
	else if (!memcmp(workbuffer + 24, "STOPLOG ", 8)) {
		offset = 40;
		i1 = tcpnextdata(workbuffer, len + 40, &offset, NULL);
		if (i1 < 0 || !adminpassword[0]) i1 = 0;
		ptr = workbuffer + offset;
		ptr[i1] = '\0';
		for (i2 = 0; i2 < i1 && toupper(ptr[i2]) == toupper(adminpassword[i2]); i2++);
		if (ptr[i2] != adminpassword[i2]) {
			strcpy(errormsg, "Invalid admin password");
			goto processerror;
		}
		if (!logfilename[0]) {
			strcpy(errormsg, "Log file not specified in CFG file");
			goto processerror;
		}
		if (!(fsflags & FLAG_NOLOG)) {
			if (loghandle == INVALID_HANDLE_VALUE || flushlogfile()) {
				strcpy(errormsg, "Unable to create/write archive log file");
				goto processerror;
			}
			memcpy(work, "<stop>", 6);
			msctimestamp((UCHAR *)(work + 6));
			memcpy(work + 22, "</stop>", 7);
			i1 = fioawrite(loghandle, (UCHAR *) work, 29, -1, NULL);
			if (i1) {
				strcpy(errormsg, "Unable to write archive log file");
				goto processerror;
			}
			fioaclose(loghandle);
			loghandle = INVALID_HANDLE_VALUE;
		}
		fsflags |= FLAG_NOLOG;
		memcpy(workbuffer + 8, "OK             0", 16);
		if (fsflags & FLAG_DEBUG) {
			writestart();
			writeout("SEND: ", 6);
			writeout((CHAR *) workbuffer, 24);
			writefinish();
		}
		tcpsend(worksockethandle, workbuffer, 24, tcpflags, 10);
	}
	else if (!memcmp(workbuffer + 24, "COMMAND ", 8)) {
		offset = 40;
		if (tcpnextdata(workbuffer, len + 40, &offset, NULL) < 0) {
			strcpy(errormsg, "Please specify a command line for the -c option");
			goto processerror;
		}
		ptr = workbuffer + offset;
		ptr[len] = '\0';
		for (ptr1 = ptr, i1 = 0; i1 < len; i1++) {
			if (*(ptr + i1) == 0x0B) {
				ptr1 = ptr + i1;
				break;
			}
		}
		if (ptr1 != ptr) { /* password found in message */
			*ptr1 = '\0';
			if (adminpassword[0]) {
				/* password specified in config, so check for match */
				for (i1 = 0; ptr + i1 < ptr1 && toupper(*(ptr + i1)) == toupper(adminpassword[i1]); i1++);
				if (*(ptr + i1) != adminpassword[i1]) {
					strcpy(errormsg, "Invalid admin password");
					goto processerror;
				}
			}
			ptr1++;
		}
		else {
			if (adminpassword[0]) {
				strcpy(errormsg, "Invalid admin password");
				goto processerror;
			}
		}
		/* ptr1 points to utility command line */
#ifndef HPUX11
#pragma warning(disable : 4996)
#endif
		if (!_memicmp(ptr1, "AIMDEX ", 7)) {
			type = UTIL_AIMDEX;
			ptr1 += 7;
		}
		else if (!_memicmp(ptr1, "BUILD ", 6)) {
			type = UTIL_BUILD;
			ptr1 += 6;
		}
		else if (!_memicmp(ptr1, "COPY ", 5)) {
			type = UTIL_COPY;
			ptr1 += 5;
		}
		else if (!_memicmp(ptr1, "CREATE ", 7)) {
			type = UTIL_CREATE;
			ptr1 += 7;
		}
		else if (!_memicmp(ptr1, "DELETE ", 7)) {
			type = UTIL_DELETE;
			ptr1 += 7;
		}
		else if (!_memicmp(ptr1, "ENCODE ", 7)) {
			type = UTIL_ENCODE;
			ptr1 += 7;
		}
		else if (!_memicmp(ptr1, "ERASE ", 6)) {
			type = UTIL_ERASE;
			ptr1 += 6;
		}
		else if (!_memicmp(ptr1, "EXIST ", 6)) {
			type = UTIL_EXIST;
			ptr1 += 6;
		}
		else if (!_memicmp(ptr1, "INDEX ", 6)) {
			type = UTIL_INDEX;
			ptr1 += 6;
		}
		else if (!_memicmp(ptr1, "REFORMAT ", 9)) {
			type = UTIL_REFORMAT;
			ptr1 += 9;
		}
		else if (!_memicmp(ptr1, "RENAME ", 7)) {
			type = UTIL_RENAME;
			ptr1 += 7;
		}
		else if (!_memicmp(ptr1, "SORT ", 5)) {
			type = UTIL_SORT;
			ptr1 += 5;
		}
		else {
			strcpy(errormsg, "Unknown utility or no utility arguments specified");
			goto processerror;
		}
		if (strlen((CHAR *)ptr1) == 0) {
			strcpy(errormsg, "Utility arguments are missing");
			goto processerror;
		}

		if (type == UTIL_INDEX || type == UTIL_SORT) {
			/* append -w to override any passed in values */
			if (workdirname[0]) {
				strcpy((CHAR *)(ptr1 + strlen((CHAR *)ptr1)), " -w=");
				strcpy((CHAR *)(ptr1 + strlen((CHAR *)ptr1)), workdirname);
			}
			else {
				strcpy((CHAR *)(ptr1 + strlen((CHAR *)ptr1)), " -w=.");
			}
		}
		i1 = utility(type, (CHAR *)ptr1, (INT)strlen((CHAR *)ptr1));
		if (i1) {
			utilgeterrornum(i1, errormsg, sizeof(errormsg));
			goto processerror;
		}
		memcpy(workbuffer + 8, "OK             0", 16);
		if (fsflags & FLAG_DEBUG) {
			writestart();
			writeout("SEND: ", 6);
			writeout((CHAR *) workbuffer, 24);
			writefinish();
		}
		tcpsend(worksockethandle, workbuffer, 24, tcpflags, 10);
	}
	else {
		strcpy(errormsg, "Invalid function");
		goto processerror;
	}
	if (tcpflags & TCP_SSL) tcpsslcomplete(worksockethandle);
	closesocket(worksockethandle);
	return;

processerror:
	memcpy(workbuffer + 8, "ERR99999", 8);
	i1 = (INT)strlen(errormsg);
	tcpiton(i1, workbuffer + 16, 8);
	memcpy(workbuffer + 24, errormsg, i1);
	if (fsflags & FLAG_DEBUG) {
		writestart();
		writeout("SEND: ", 6);
		writeout((CHAR *) workbuffer, 24 + i1);
		writefinish();
	}
	tcpsend(worksockethandle, workbuffer, 24 + i1, tcpflags, 10);
	if (tcpflags & TCP_SSL) tcpsslcomplete(worksockethandle);
	closesocket(worksockethandle);
}

static INT renamelogfile(INT stopflag)
{
	INT i1;
	CHAR timestamp[32], work[256], work2[64];

	/* create archive log file */
	msctimestamp((UCHAR *) timestamp);
	strcpy(work, logfilename);
	for (i1 = (INT)strlen(work) - 1; i1 >= 0 && work[i1] != '.' && work[i1] != '/' && work[i1] != '\\' && work[i1] != ':'; i1--);
	if (i1 < 0 || work[i1] != '.') i1 = (INT)strlen(work);
	memmove(work + i1 + 17, work + i1, strlen(work + i1) + 1);
	work[i1++] = '_';
	memcpy(work + i1, timestamp, 16);
	fioadelete(work);
	if (stopflag) {
		/* add stop time to archive log file */
		memcpy(work2, "<stop>", 6);
		memcpy(work2 + 6, timestamp, 16);
		memcpy(work2 + 22, "</stop>", 7);
		fioawrite(loghandle, (UCHAR *) work2, 29, -1, NULL);
	}
	fioaclose(loghandle);
	loghandle = INVALID_HANDLE_VALUE;
	i1 = fioarename(logfilename, work);
	if (i1) return i1;
	i1 = fioaopen(logfilename, FIO_M_EOC, 1, &loghandle);
	if (i1) return i1;
	fioalseek(loghandle, 0, 2, NULL);
	i1 = writeStartingXML(loghandle);
	return i1;
}

static INT writeStartingXML(FHANDLE loghndl) {
	CHAR work[128];
#ifdef Linux
	CHAR work2[32];
	pid_t mypid = getpid();
	uid_t myuid = getuid();
	uid_t myeuid = geteuid();
	strcpy(work, "<start");
	sprintf(work2, " pid=\"%u\"", (unsigned int)mypid);
	strcat(work, work2);
	sprintf(work2, " uid=\"%u\"", (unsigned int)myuid);
	strcat(work, work2);
	sprintf(work2, " euid=\"%u\"", (unsigned int)myeuid);
	strcat(work, work2);
	strcat(work, ">");
	msctimestamp((UCHAR *)work2);
	work2[16] = '\0';
	strcat(work, work2);
	strcat(work, "</start>");
#else
	memcpy(work, "<start>", 7);
	msctimestamp((UCHAR *)(work + 7));
	memcpy(work + 23, "</start>\0", 9);
#endif
	return fioawrite(loghndl, (UCHAR *) work, strlen(work), -1, NULL);
}

/**
 *
 * Using temphandle and loghandle, move all data
 * from temp to log.
 * Then clear the temp file
 */
static INT flushlogfile()
{
	INT i1, i2;
	size_t i3;
	UCHAR buffer[8192 * 2];

	/* copy log file to archive log file */
	if (!(fsflags & FLAG_NOLOG)) {
		if (loghandle == INVALID_HANDLE_VALUE) return ERR_NOTOP;
		i1 = fioalock(temphandle, FIOA_FLLCK | FIOA_WRLCK, 0, 120);
		if (i1) return i1;
		do {
			i1 = fioaread(temphandle, buffer, sizeof(buffer), tempfilepos, &i2);
			if (i1) {
				fioalock(temphandle, FIOA_FLLCK | FIOA_UNLCK, 0, 0);
				return i1;
			}
			if (!i2) break;
			i1 = fioawrite(loghandle, buffer, i2, -1, &i3);
			tempfilepos += i3;
			if (i1) {
				fioalock(temphandle, FIOA_FLLCK | FIOA_UNLCK, 0, 0);
				return i1;
			}
		} while (i2 == sizeof(buffer));
	}
	i1 = fioatrunc(temphandle, 0);
	if (i1) fioalseek(temphandle, 0, 2, &tempfilepos);
	else tempfilepos = 0;
	fioalock(temphandle, FIOA_FLLCK | FIOA_UNLCK, 0, 0);
	return i1;
}

/**
 * @param logfile is filled in from (if xml) <logchanges><logfile>....</logfile>...</logchanges>
 * 				or if a flat file "logfile=..."
 * @param workdir is filled in from the 'workdir' element
 * @param fioparms used for; casemap, collatemap, compat, excloffset, extcase, filepath, fileoffset,
 * 				namecase, nodigitcompression, openexclusive, pathcase, preppath, recoffset
 */
static void parsecfgfile(CHAR *cfgfile, CHAR *adminpwd, CHAR *license, CHAR *logfile, CHAR *workdir,
		CHAR *showpwd, INT *portnum, INT *sslportnum, FIOPARMS *fioparms)
{
	INT i1, i2, i3, filesize, length, linecnt, nextoffset, xmlflag;
	INT volpos, volsize, exclusiveflag, prepflag;
	CHAR work[256], *filebuffer, *kw, *val;
	OFFSET off;
	FILE *file, *f1;
	ELEMENT *element, *element2, *element3, *nextelement, *xmlbuffer;
	CHAR **hmem;

	file = fopen(cfgfile, "rb");
	if (file == NULL) death1("CFG file open failed");
	fseek(file, 0, 2);
	filesize = (INT) ftell(file);
	fseek(file, 0, 0);
	filebuffer = (CHAR *) malloc(filesize + sizeof(CHAR));
	if (filebuffer == NULL) {
		fclose(file);
		death1("insufficient memory");
	}
	i1 = (INT)fread(filebuffer, 1, filesize, file);
	fclose(file);
	if (i1 != filesize) {
		free(filebuffer);
		death1("error reading cfg file");
	}
	for (i1 = 0; i1 < filesize && (isspace(filebuffer[i1]) || filebuffer[i1] == '\n' || filebuffer[i1] == '\r'); i1++);
	if (i1 < filesize && filebuffer[i1] == '<') {
		xmlbuffer = getxmldata(filebuffer + i1, filesize - i1);
		free(filebuffer);
		for (element = xmlbuffer; element != NULL && (element->cdataflag || strcmp(element->tag, "dbcfscfg")); element = element->nextelement);
		if (element == NULL) {
			free(xmlbuffer);
			death1("CFG error: 'dbcfscfg' element missing");
		}
		nextelement = element->firstsubelement;
		xmlflag = TRUE;
	}
	else {
		nextoffset = length = 0;
		xmlflag = FALSE;
	}
	exclusiveflag = TRUE;
	prepflag = FALSE;
	adminpwd[0] = logfile[0] = workdir[0] = showpwd[0] = license[0] = '\0';
	for (linecnt = volpos = volsize = 0; ; ) {
		if (xmlflag) {
			if (nextelement == NULL) break;
			element = nextelement;
			nextelement = element->nextelement;
			if (element->cdataflag) continue;
			kw = element->tag;
			if (element->firstsubelement != NULL && element->firstsubelement->cdataflag) val = element->firstsubelement->tag;
			else val = "";
		}
		else {
			if (nextoffset >= length) {
				if (nextoffset >= filesize) break;
				for (length = nextoffset; length < filesize && filebuffer[length] != '\n' && filebuffer[length] != '\r'; length++);
				if (filebuffer[length] == '\r' && filebuffer[length + 1] == '\n') filebuffer[length + 1] = ' ';
				filebuffer[length] = '\0';
				linecnt++;
			}
			i1 = getnextkwval(filebuffer + nextoffset, &kw, &val);
			nextoffset += i1;
		}
		if (!*kw) continue;
		if (*kw == 'a') {
			if (!strcmp(kw, "adminpassword")) {
				if (strlen(val) >= 32) death1("CFG error: admin password too long");
				strcpy(adminpwd, val);
				continue;
			}
		}
		if (*kw == 'c') {
			if (!strcmp(kw, "casemap")) {
				if (!val[0]) {
					death1("invalid casemap specification");
					break;
				}
				for (i1 = 0; i1 <= UCHAR_MAX; i1++) fioparms->casemap[i1] = (UCHAR) toupper(i1);
				if (translate(val, fioparms->casemap)) {
					f1 = fopen(val, "rb");
					if (f1 == NULL) {
						death1("CFG error: casemap open failure");
						break;
					}
					i1 = (INT)fread(fioparms->casemap, (INT)sizeof(CHAR), 256, f1);
					fclose(f1);
					if (i1 != 256) {
						death1("CFG error: casemap read failure");
						break;
					}
				}
				continue;
			}
			if (!strcmp(kw, "checkretry")) continue;
			if (!strcmp(kw, "checktime")) continue;
			if (!strcmp(kw, "collatemap")) {
				if (!val[0]) {
					death1("CFG error: invalid collatemap specification");
					break;
				}
				for (i1 = 0; i1 <= UCHAR_MAX; i1++) fioparms->collatemap[i1] = (UCHAR) i1;
				if (translate(val, fioparms->collatemap)) {
					f1 = fopen(val, "rb");
					if (f1 == NULL) {
						death1("CFG error: collatemap open failure");
						break;
					}
					i1 = (INT)fread(fioparms->collatemap, (INT)sizeof(CHAR), 256, f1);
					fclose(f1);
					if (i1 != 256) {
						death1("CFG error: collatemap read failure");
						break;
					}
				}
				continue;
			}
			if (!strcmp(kw, "compat")) {
				for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
				if (!strcmp(val, "dos")) fioparms->flags |= FIO_FLAG_COMPATDOS;
				else if (!strcmp(val, "rms")) fioparms->flags |= FIO_FLAG_COMPATRMS;
				else if (!strcmp(val, "rmsx")) fioparms->flags |= FIO_FLAG_COMPATRMSX;
				else if (!strcmp(val, "rmsy")) fioparms->flags |= FIO_FLAG_COMPATRMSY;
				else {
					death1("CFG error: invalid compat specification");
					break;
				}
				continue;
			}
			if (!strcmp(kw, "certificatefilename")) {
				strcpy(certificatefilename, val);
				continue;
			}
		}
		if (*kw == 'd') {
			if (!strcmp(kw, "dbdpath")) continue;
			if (!strcmp(kw, "deletelock")) continue;
		}
		if (*kw == 'e') {
			if (!strcmp(kw, "encryption")) {
				fsflags &= ~(FLAG_NOSSL | FLAG_SSLONLY);
				for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) toupper(val[i1]);
				if (!strcmp(val, "OFF")) fsflags |= FLAG_NOSSL;
				else if (!strcmp(val, "ONLY")) fsflags |= FLAG_SSLONLY;
				else if (strcmp(val, "ON")) death1("CFG error: invalid encyption option");
				continue;
			}
			if (!strcmp(kw, "eport")) {
				if (!*sslportnum) *sslportnum = atoi(val);
				continue;
			}
			if (!strcmp(kw, "excloffset")) {
				mscatooff(val, &off);
				fioparms->excloffset = off;
				fioparms->parmflags |= FIO_PARM_EXCLOFFSET;
				continue;
			}
			if (!strcmp(kw, "extcase")) {
				for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
				if (!strcmp(val, "upper")) fioparms->flags |= FIO_FLAG_EXTCASEUPPER;
				else {
					death1("CFG error: invalid extcase specification");
					break;
				}
				continue;
			}
		}
		if (*kw == 'f') {
			if (!strcmp(kw, "filepath")) {
				if (xmlflag) {
					for (i2 = 0, element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement) {
						if (element2->cdataflag) continue;
						if (!strcmp(element2->tag, "dir")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								if (i2) fioparms->openpath[i2++] = ';';
								else if (!prepflag) strcpy(fioparms->preppath, element2->firstsubelement->tag);
								strcpy(fioparms->openpath + i2, element2->firstsubelement->tag);
								i2 += (INT)strlen(element2->firstsubelement->tag);
							}
						}
					}
				}
				else {
					strcpy(fioparms->openpath, val);
					if (!prepflag) {
						for (i1 = 0; val[i1] && val[i1] != ';'; i1++);
						val[i1] = 0;
						strcpy(fioparms->preppath, val);
					}
				}
				continue;
			}
			if (!strcmp(kw, "fileoffset")) {
				mscatooff(val, &off);
				fioparms->fileoffset = off;
				fioparms->parmflags |= FIO_PARM_FILEOFFSET;
				continue;
			}
		}
		if (*kw == 'l') {
			if (!strcmp(kw, "licensekey")) {
				strcpy(license, val);
				continue;
			}
			if (xmlflag) {
				if (!strcmp(kw, "logchanges")) {
					for (element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement) {
						if (element2->cdataflag) continue;
						/**
						 * This 'archive' appears to be dead code, not documented, and it does not seem to really do anything
						 */
						if (!strcmp(element2->tag, "archive")) {
							for (element3 = element2->firstsubelement; element3 != NULL; element3 = element3->nextelement) {
								if (element3->cdataflag) continue;
								/*** CODE: NEED ARCHIVE TIME ??? ***/
							}
						}
						else if (!strcmp(element2->tag, "logfile")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag)
								strcpy(logfile, element2->firstsubelement->tag);
						}
						/*
						 * Added 11 April 2012 to help Sean solve a problem
						 * NEVER document this!
						 */
#if defined(Linux)
						else if (!strcmp(element2->tag, "signal")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								if (_memicmp("on", element2->firstsubelement->tag, 2) == 0) fsflags |= FLAG_LOGSIGS;
								if (_memicmp("all", element2->firstsubelement->tag, 3) == 0) {
									fsflags |= FLAG_LOGSIGS;
									fsflags |= FLAG_LOGSIGSX;
								}
								if (fsflags & FLAG_LOGSIGSX) {
									if (pipe(pipefd) == -1) {
										death1("CFG error: pipe creation failed (signal=all)");
									}
								}
							}
						}
						else if (!strcmp(element2->tag, "accept")) {
							fsflags |= FLAG_LOGACCEPT;
						}
#endif
					}
					continue;
				}
			}
			else if (!strcmp(kw, "logfile")) {
				strcpy(logfile, val);
				continue;
			}
			else if (!strcmp(kw, "lognames")) continue;
			else if (!strcmp(kw, "logopenclose")) continue;
			else if (!strcmp(kw, "logtimestamp")) continue;
			else if (!strcmp(kw, "logusername")) continue;
		}
		if (*kw == 'm') {
			if (!strcmp(kw, "memalloc")) continue;
			if (!strcmp(kw, "memresult")) continue;
		}
		if (*kw == 'n') {
			if (!strcmp(kw, "namecase")) {
				for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
				if (!strcmp(val, "upper")) fioparms->flags |= FIO_FLAG_NAMECASEUPPER;
				else if (!strcmp(val, "lower")) fioparms->flags |= FIO_FLAG_NAMECASELOWER;
				else {
					death1("CFG error: invalid namecase specification");
					break;
				}
				continue;
			}
			if (!strcmp(kw, "nodigitcompression")) {
				fioparms->flags |= FIO_FLAG_NOCOMPRESS;
				continue;
			}
			if (!strcmp(kw, "noexclusivesupport")) {
				exclusiveflag = FALSE;
				continue;
			}
			if (!strcmp(kw, "nport")) {
				if (!*portnum) *portnum = atoi(val);
				continue;
			}
			if (!strcmp(kw, "newconnectiondelay")) {
				startDelayInMilliseconds = strtol(val, NULL, 10);
				continue;
			}
		}
		if (*kw == 'o') {
/*** CODE: WHY DO WE HAVE THIS ??? ***/
			if (!strcmp(kw, "openexclusive")) {
				fioparms->flags |= FIO_FLAG_SINGLEUSER;
				continue;
			}
		}
		if (*kw == 'p') {
			if (!strcmp(kw, "passwordfile")) continue;
			if (!strcmp(kw, "pathcase")) {
				for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
				if (!strcmp(val, "upper")) fioparms->flags |= FIO_FLAG_PATHCASEUPPER;
				else if (!strcmp(val, "lower")) fioparms->flags |= FIO_FLAG_PATHCASELOWER;
				else {
					death1("CFG error: invalid pathcase specification");
					break;
				}
				continue;
			}
			if (!strcmp(kw, "preppath")) {
				if (xmlflag) {
					for (element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement) {
						if (element2->cdataflag) continue;
						if (!strcmp(element2->tag, "dir")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								strcpy(fioparms->preppath, element2->firstsubelement->tag);
								break;
							}
						}
					}
				}
				else strcpy(fioparms->preppath, val);
				prepflag = TRUE;
				continue;
			}
		}
		if (*kw == 'r') {
			if (!strcmp(kw, "recoffset")) {
				mscatooff(val, &off);
				fioparms->recoffset = off;
				fioparms->parmflags |= FIO_PARM_RECOFFSET;
				continue;
			}
		}
		if (*kw == 's') {
			if (!strcmp(kw, "showpassword")) {
				if (strlen(val) >= 32) death1("CFG error: show password too long");
				strcpy(showpwd, val);
				continue;
			}
			if (!strcmp(kw, "sport")) {
				if (!sportnum) sportnum = atoi(val);
				continue;
			}
			if (!strcmp(kw, "sqlstatistics")) continue;
		}
		if (*kw == 'u') {
			if (!strcmp(kw, "updatelock")) continue;
			if (!strcmp(kw, "usermax")) {
				usermax = atoi(val);
				continue;
			}
		}
		if (*kw == 'v') {
			if (!strcmp(kw, "volume")) {
				if (xmlflag) {
					for (i2 = 0, element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement) {
						if (element2->cdataflag) continue;
						if (!strcmp(element2->tag, "dir")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								if (i2) i2++;
								i2 += (INT)strlen(element2->firstsubelement->tag);
							}
						}
						else if (!strcmp(element2->tag, "name")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) val = element2->firstsubelement->tag;
							else val = "";
						}
					}
				}
				else {
					for (i1 = 0; val[i1] && !isspace(val[i1]); i1++);
					if (val[i1]) val[i1++] = '\0';
					for ( ; isspace(val[i1]); i1++);
					i2 = (INT)strlen(val + i1);
				}
				if (!val[0]) {
					death1("CFG error: invalid volume specification");
					break;
				}
				if (strlen(val) > MAX_VOLUMESIZE) {
					strcpy(work, "CFG error: volume name too long: ");
					strcat(work, val);
					death1(work);
					break;
				}
				if (!i2) {
					strcpy(work, "CFG error: volume path missing: ");
					strcat(work, val);
					death1(work);
					break;
				}
				for (i3 = 0; i3 < volcount && strcmp((*volumes)[i3].volume, val); i3++);
				if (i3 < volcount) {
					if (memchange((UCHAR **)(*volumes)[i3].path, i2 + 1, 0) == -1) {
						death1("insufficient memory");
						break;
					}
					hmem = (*volumes)[i3].path;
				}
				else {
					if (volcount == volalloc) {
						if (!volalloc) {
							volumes = (VOLUME **) memalloc(8 * sizeof(VOLUME), 0);
							if (volumes == NULL) {
								death1("insufficient memory");
								break;
							}
						}
						else if (memchange((UCHAR **) volumes, (volalloc + 8) * sizeof(VOLUME), 0) == -1) {
							death1("insufficient memory");
							break;
						}
						volalloc += 8;
					}
					strcpy((*volumes)[volcount].volume, val);
					hmem = (CHAR **) memalloc(i2 + 1, 0);
					if (hmem == NULL) {
						death1("insufficient memory");
						break;
					}
					(*volumes)[volcount++].path = hmem;
				}
				if (xmlflag) {
					for (i2 = 0, element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement) {
						if (element2->cdataflag) continue;
						if (!strcmp(element2->tag, "dir")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								if (i2) (*hmem)[i2++] = ';';
								strcpy(*hmem + i2, element2->firstsubelement->tag);
								i2 += (INT)strlen(element2->firstsubelement->tag);
							}
						}
					}
				}
				else strcpy(*hmem, val + i1);
				continue;
			}
		}
		if (*kw == 'w') {
			if (!strcmp(kw, "workdir")) {
				strcpy(workdir, val);
				continue;
			}
		}
		strcpy(work, "CFG error: unrecognized keyword: ");
		strcat(work, kw);
		death1(work);
		break;
	}
	if (xmlflag) free(xmlbuffer);
	else free(filebuffer);
#if OS_UNIX
	if (exclusiveflag || (fioparms->flags & FIO_FLAG_SINGLEUSER)) fioparms->flags |= FIO_FLAG_EXCLOPENLOCK;
#endif
}

static ELEMENT *getxmldata(CHAR *filebuffer, INT filesize)
{
	INT i1, i2, i3;
	CHAR work[256], *ptr;
	ELEMENT *xmlbuffer;

	for (i1 = i2 = i3 = 0; i1 < filesize; ) {
		for ( ; i1 < filesize && filebuffer[i1] != '\n' && filebuffer[i1] != '\r' && (UCHAR) filebuffer[i1] != DBCEOR && (UCHAR) filebuffer[i1] != DBCEOF; i1++) {
			if (filebuffer[i1] == '<' && i1 + 1 < filesize && (filebuffer[i1 + 1] == '!' || filebuffer[i1 + 1] == '?')) {
				/* comment or meta data */
				if (filebuffer[++i1] == '!') {  /* <!-- anything --> */
					for (i1++; i1 + 2 < filesize && (filebuffer[i1] != '-' || filebuffer[i1 + 1] != '-' || filebuffer[i1 + 2] != '>'); i1++);
					if ((i1 += 2) >= filesize)  /* invalid comment */
						death1("CFG contains invalid XML comment");
				}
				else {  /* <?tagname [attribute="value" ...] ?> */
					for (i1++; i1 + 1 < filesize && (filebuffer[i1] != '?' || filebuffer[i1 + 1] != '>'); i1++);
					if (++i1 >= filesize)  /* invalid meta data */
						death1("CFG contains invalid XML meta data");
				}
				continue;
			}
			filebuffer[i2++] = filebuffer[i1];
			if (!isspace(filebuffer[i1])) i3 = i2;
		}
		for (i2 = i3; i1 < filesize && (isspace(filebuffer[i1]) || filebuffer[i1] == '\n' || filebuffer[i1] == '\r' || (UCHAR) filebuffer[i1] == DBCEOR || (UCHAR) filebuffer[i1] == DBCEOF); i1++);
	}
	for (i3 = 512; i3 < i2; i3 <<= 1);
	xmlbuffer = (ELEMENT *) malloc(i3);
	if (xmlbuffer == NULL) death1("insufficient memory");
	while ((i1 = xmlparse(filebuffer, i2, xmlbuffer, i3)) == -1) {
		ptr = (CHAR *) realloc(xmlbuffer, i3 << 1);
		if (ptr == NULL) {
			free(xmlbuffer);
			death1("insufficient memory");
		}
		xmlbuffer = (ELEMENT *) ptr;
		i3 <<= 1;
	}
	if (i1 < 0) {
		free(xmlbuffer);
		strcpy(work, "CFG file contains invalid XML format: ");
		strcat(work, xmlgeterror());
		death1(work);
	}
	return xmlbuffer;
}

static INT getnextkwval(CHAR *record, CHAR **kw, CHAR **val)
{
	INT i2, i3, cnt;

	*kw = *val = "";
	for (cnt = 0; isspace(record[cnt]); cnt++);
	if (!record[cnt]) return cnt + 1;
	if (record[cnt] == '-' && record[cnt + 1] == '-') return cnt + (INT)strlen(record + cnt) + 1;

	for (*kw = record + cnt; record[cnt] && record[cnt] != '=' && !isspace(record[cnt]) && record[cnt] != ','; cnt++)
		record[cnt] = (CHAR) tolower(record[cnt]);
	for (i2 = cnt; isspace(record[cnt]); cnt++);
	if (!record[cnt]) {  /* only keyword */
		record[i2] = '\0';  /* truncate any spaces */
		return cnt + 1;
	}

	if (record[cnt] == '=') {
		record[i2] = '\0';  /* terminate key word */
		for (cnt++; isspace(record[cnt]); cnt++);
/*** SHOULD KEYWORD= WITH NO VALUE BE CONSIDERED AN ERROR ? ***/
		if (!record[cnt] || record[cnt] == ',') return cnt + 1;
		*val = record + cnt;
		for (i2 = cnt; ; ) {
			if (record[cnt] == '"') {
				if (record[++cnt] != '"') {
					for ( ; ; ) {
						if (!record[cnt]) death1("CFG file missing closing quote");
						if (record[cnt] == '"') {
							if (record[++cnt] != '"') break;
						}
						record[i2++] = record[cnt++];
					}
					continue;
				}
			}
			i3 = cnt;
			if (isspace(record[cnt])) while (isspace(record[cnt])) cnt++;
			if (!record[cnt] || record[cnt] == ',' ||
				(record[cnt] == '-' && record[cnt + 1] == '-')) break;
			if (i3 != cnt) cnt--;  /* just keep one space */
			record[i2++] = record[cnt++];
		}
	}
	if (record[cnt] != ',') cnt += (INT)strlen(record + cnt);  /* ignoring rest of line, even if not comment */
	record[i2] = '\0';  /* terminate key word or value */
	return cnt + 1;
}

#if 0
static INT getlicensenum()
{
	INT i1, i2, i3;
	for (i1 = 10, i2 = i3 = 0; i1 < 190; i1++) {
		if (companyinfo[i1] & 0x15) i2 += companyinfo[i1];
		if (i1 & 0x06) i3 += (UCHAR)(i1 ^ companyinfo[i1]);
	}
	if (companyinfo[109] || companyinfo[169] || companyinfo[189]) return(-1);
	if ((i2 % 241) != (INT) companyinfo[190] || (i3 % 199) != (INT) companyinfo[191]) return(-1);
	return atoi((char *) &companyinfo[100]);
}
#endif

static void writestart()
{
	CHAR work[32];
	writeout("[", 1);
	msctimestamp((UCHAR *)work);
	writeout(work, 16);
	writeout("] ", 2);
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

static void death1(char *msg)
{
	INT i1;
	if (volumes != NULL) {
		for (i1 = 0; i1 < volcount; i1++) memfree((UCHAR **)(*volumes)[i1].path);
		memfree((UCHAR **)volumes);
		volumes = NULL;
	}
	fioexit(); /* NOTE: Safe to call before fioinit() */
//	lcsend(lcshandle);
	if (!(fsflags & FLAG_SERVICE) || outputfile != stdout) {
		fprintf(outputfile, "ERROR: %s\n", msg);
		fflush(outputfile);
	}
#if OS_WIN32
	if (fsflags & FLAG_SERVICE) {
		char errormsg[512];
		strcpy(errormsg, "ERROR: ");
		strcat(errormsg, msg);
		svclogerror(errormsg, 0);
	}
	WSACleanup();
	if (fsflags & FLAG_SERVICE) svcstatus(SVC_STOPPED, 0);
#endif
	exit(1);
}

static void death2(char *msg, INT num)
{
	INT i1;
	if (volumes != NULL) {
		for (i1 = 0; i1 < volcount; i1++) memfree((UCHAR **)(*volumes)[i1].path);
		memfree((UCHAR **)volumes);
		volumes = NULL;
	}
	fioexit(); /* Note: safe to call before fioinit() */
//	lcsend(lcshandle);
	if (!(fsflags & FLAG_SERVICE) || outputfile != stdout) {
		fprintf(outputfile, "ERROR: %s: %d\n", msg, num);
		fflush(outputfile);
	}
#if OS_WIN32
	if (fsflags & FLAG_SERVICE) {
		char errormsg[512];
		sprintf(errormsg, "ERROR: %s: %d", msg, num);
		svclogerror(errormsg, 0);
	}
	WSACleanup();
	if (fsflags & FLAG_SERVICE) svcstatus(SVC_STOPPED, 0);
#endif
	exit(1);
}

static void usage()
{
	fputs("   Usage:  dbcfs [configfile] [-g] [-x] [-?]\n", stdout);
	fputs("                 [-fs=binary]\n", stdout);
#if OS_WIN32
	fputs("      Windows service options: [-display=servicedisplayname] [-install]\n", stdout);
	fputs("                               [-password=logonpassword] [-service=servicename]\n", stdout);
	fputs("                               [-start] [-stop] [-uninstall] [-user=logonuser]\n", stdout);
	fputs("                               [-verbose]\n", stdout);
#endif
#if OS_UNIX
	fputs("      UNIX daemon options: [-y]\n", stdout);
#endif
	exit(0);
}

#if OS_WIN32
static BOOL sigevent(DWORD sig)
{
	switch (sig) {
	case CTRL_C_EVENT:
		fsflags |= FLAG_SHUTDOWN;
		return TRUE;
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		fsflags |= FLAG_SHUTDOWN;
		return TRUE;
	}
	return FALSE;
}

static void dbcfsstop()
{
	fsflags |= FLAG_SHUTDOWN;
	svcstatus(SVC_STOPPING, 15000 + 13);
}

static DWORD threadproc(DWORD parm) // @suppress("No return")
{
	INT i1;
	DWORD ecode;
	CHAR errormsg[128];

	for ( ; ; ) {
		if (WaitForSingleObject(hUserArrayMutex, 1000) != WAIT_OBJECT_0) {
			ReleaseMutex(hUserArrayMutex);
			SLEEP(50);
			continue;
		}
		for (i1 = 0; i1 < usercount; i1++) {
			if (userinfo[i1].pid == 0) continue;
			if (!GetExitCodeProcess(userinfo[i1].phandle, &ecode)) continue;
			if (ecode == STILL_ACTIVE) continue;
			if (sportnum && userinfo[i1].sockethandle != INVALID_SOCKET) fsflags |= FLAG_DIED;
			CloseHandle(userinfo[i1].phandle);
			userinfo[i1].pid = 0;
		}
		while (usercount && !userinfo[usercount - 1].pid) usercount--;
		if ((fsflags & FLAG_DEBUG) && debuglevel >= 5) {
			writestart();
			writeout("DBG:  ", 6);
			sprintf(errormsg, "In threadproc, usercount is now %d", usercount);
			writeout(errormsg, (INT)strlen(errormsg));
			writefinish();
		}
		ReleaseMutex(hUserArrayMutex);
		SLEEP(DEAD_PROCESS_DELAY_IN_MILLISECONDS); /* 1/8 second */
	}
}

//static DWORD threadproc(DWORD parm)
//{
//	INT i1, arraysize;
//	DWORD cnt, event, ecode;
//	HANDLE phandle, arraybuf[1024], *arrayptr, *ptr;
//
//	arrayptr = arraybuf;
//	arraysize = sizeof(arraybuf) / sizeof(*arraybuf);
//	for ( ; ; ) {
//		if (usercount >= arraysize) {
//			if (arraysize == sizeof(arraybuf) / sizeof(*arraybuf))
//				ptr = (HANDLE *) malloc((arraysize << 1) * sizeof(HANDLE));
//			else ptr = (HANDLE *) realloc(arrayptr, (arraysize << 1) * sizeof(HANDLE));
//			if (ptr != NULL) {
//				arrayptr = ptr;
//				arraysize <<= 1;
//			}
//		}
//		arrayptr[0] = threadevent;
//		for (cnt = 1, i1 = 0; i1 < usercount && (INT) cnt < arraysize; i1++) {
//			if (!userinfo[i1].pid || userinfo[i1].pid == -1) continue;
//			arrayptr[cnt++] = userinfo[i1].phandle;
//		}
//		event = WaitForMultipleObjects(cnt, arrayptr, FALSE,
//				10000 /* ten seconds */ );
//		ResetEvent(threadevent);
//		if (event > WAIT_OBJECT_0 && event - WAIT_OBJECT_0 < cnt) {
//			phandle = arrayptr[event - WAIT_OBJECT_0];
//			for (i1 = 0; i1 < usercount; i1++) {
//				if (phandle == userinfo[i1].phandle) {
//					if (sportnum && userinfo[i1].sockethandle != INVALID_SOCKET) fsflags |= FLAG_DIED;
//					CloseHandle(userinfo[i1].phandle);
//					userinfo[i1].pid = 0;
//					break;
//				}
//			}
//		}
//	}
//	return 0;
//}
#endif

#if OS_UNIX
static void sigevent(INT sig)
{
	INT i1, pid;

	switch(sig) {
	case SIGINT:
		fsflags |= FLAG_SHUTDOWN;
		sig_received = sig;
		break;
	case SIGTERM:
	case SIGHUP:
		fsflags |= FLAG_SHUTDOWN;
		sig_received = sig;
		break;
	case SIGPIPE:  /* socket disconnected */
		break;
	case SIGCHLD:
		/* release zombie process */
#ifdef WNOHANG
		while ((pid = waitpid(
				-1, /* Wait for any child process */
				NULL,
				WNOHANG /* return immediately if no child has exited. */
				)) > 0)
		{
#else
		if ((pid = wait(NULL)) > 0) {
#endif
			for (i1 = 0; i1 < usercount; i1++) {
				if (pid == userinfo[i1].pid) {
					if (sportnum && userinfo[i1].sockethandle != INVALID_SOCKET) fsflags |= FLAG_DIED;
					userinfo[i1].pid = 0;
					break;
				}
			}
		}
		break;
	}
}

#ifdef Linux
static void sigevent3(int sig, siginfo_t *info, void *ignore)
{
	CHAR work[64];
	memcpy(&siginfo, info, sizeof(siginfo_t));
	siginfo.si_signo = sig; /* because this struct field is not set on Linux !! */
	if (fsflags & FLAG_DEBUG && debuglevel >= 3) {
		writestart();
		sprintf(work, "SIG: sig=%d, code=%d", siginfo.si_signo, siginfo.si_code);
		writeout(work, -1);
		writefinish();
	}
	if (!(fsflags & FLAG_NOLOG) && loghandle != INVALID_HANDLE_VALUE && (fsflags & FLAG_LOGSIGSX)) {
		/* Write to pipefd[1] */
		PIPEMESSAGE pm;
		pm.size = sizeof(PIPEMESSAGE);
		memcpy(&pm.si, &siginfo, sizeof(siginfo_t));
		write(pipefd[1], &pm, sizeof(PIPEMESSAGE));
		fdatasync(pipefd[1]);
	}
	sigevent(sig);
}
#endif

static void checkchildren()
{
	INT i1, cnt, pid;

	/* check for any zombies */
	for (cnt = 0; cnt < usercount; cnt++) {
		if (!userinfo[cnt].pid || userinfo[cnt].pid == -1) continue;
		if (kill(userinfo[cnt].pid, 0) == -1 && errno == ESRCH) {
			do {
#ifdef WNOHANG
				while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
#else
/*** CODE: DO WE HAVE TO BLOCK SIGCHLD DURING THIS CALL ??? ***/
				if ((pid = wait(NULL)) > 0) {
#endif
					if (pid == userinfo[cnt].pid) {
						if (sportnum && userinfo[cnt].sockethandle != INVALID_SOCKET) fsflags |= FLAG_DIED;
						userinfo[cnt].pid = 0;
					}
					else for (i1 = 0; i1 < usercount; i1++) {
						if (pid == userinfo[i1].pid) {
							if (sportnum && userinfo[i1].sockethandle != INVALID_SOCKET) fsflags |= FLAG_DIED;
							userinfo[i1].pid = 0;
							break;
						}
					}
				}
			} while (pid > 0 && userinfo[cnt].pid);
		}
	}
	while (usercount && !userinfo[usercount - 1].pid) usercount--;
}
#endif

/**
 * Does not move memory
 */
static INT volmapfnc(char *vol, char ***path)
{
	INT i1;
	for (i1 = 0; i1 < volcount; i1++) {
		if (strcmp(vol, (*volumes)[i1].volume)) continue;
		if (path != NULL) *path = (*volumes)[i1].path;
		return 0;
	}
	return -1;
}

/**
 * Does not move memory
 */
static INT translate(CHAR *ptr, UCHAR *map) // @suppress("No return")
{
	INT i1, i2, i3;
	for ( ; ; ) {
		while (isspace(*ptr)) ptr++;
		if (*ptr == ';') {
			ptr++;
			continue;
		}
		if (!*ptr) return 0;
		if (!isdigit(*ptr)) return -1;
		for (i1 = 0; isdigit(*ptr); ptr++) i1 = i1 * 10 + *ptr - '0';
		while (isspace(*ptr)) ptr++;
		if (*ptr == '-') {
			ptr++;
			while (isspace(*ptr)) ptr++;
			if (!isdigit(*ptr)) return -1;
			for (i2 = 0; isdigit(*ptr); ptr++) i2 = i2 * 10 + *ptr - '0';
			while (isspace(*ptr)) ptr++;
		}
		else i2 = i1;
		if (*ptr != ':') return -1;
		ptr++;
		while (isspace(*ptr)) ptr++;
		if (!isdigit(*ptr)) return -1;
		for (i3 = 0; isdigit(*ptr); ptr++) i3 = i3 * 10 + *ptr - '0';
		if (i1 > UCHAR_MAX || i2 > UCHAR_MAX || i3 + (i2 - i1) > UCHAR_MAX) return -1;
		while (i1 <= i2) map[i1++] = (UCHAR) i3++;
	}
}

#if OS_UNIX
static INT _memicmp(void *src, void *dest, INT len)
{
	while (len--) if (toupper(((UCHAR *) src)[len]) != toupper(((UCHAR *) dest)[len])) return 1;
	return 0;
}
#endif
