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

#include "tcp.h"

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long) 0xFFFFFFFF)
#endif

#if OS_WIN32
#define ERRORVALUE() WSAGetLastError()
#else
#define ERRORVALUE() errno
#endif

#define CONNECTPORT 9584

#define ADMIN_SHUTDOWN	1
#define ADMIN_NEWLOG	2
#define ADMIN_STARTLOG	3
#define ADMIN_STOPLOG	4
#define ADMIN_UTILITY	5

static INT portnum;
static INT tcpflags;
static CHAR *password;
static CHAR servername[256];

static void dbcfshello(void);
static void dbcfsadmin(CHAR *parameter, INT type);
static void dbcfsusers(void);
static void death1(CHAR *msg);
static void usage(INT flag);


INT main(INT argc, CHAR **argv)
{
	INT i1, i2, newlog, shutdown, startlog, stoplog, users, utility;
	CHAR work[256];

	newlog = portnum = shutdown = startlog = stoplog = users = utility = 0;
	tcpflags = TCP_UTF8 | TCP_SSL;
	servername[0] = '\0';
	password = NULL;
	for (i1 = 0; ++i1 < argc; ) {
		if (argv[i1][0] == '-') {
			for (i2 = 1; argv[i1][i2] && argv[i1][i2] != '='; i2++) work[i2 - 1] = (CHAR) toupper(argv[i1][i2]);
			work[i2 - 1] = '\0';
			switch (toupper(work[0])) {
			case '?':
				usage(TRUE);
				break;
			case 'G':
				if (!argv[i1][2]) newlog = i1;
				else usage(FALSE);
				break;
			case 'L':
				if (!argv[i1][2]) startlog = i1;
				else usage(FALSE);
				break;
			case 'N':
				if (!strcmp(work, "NOENCRYPT")) tcpflags &= ~TCP_SSL;
				else usage(FALSE);
				break;
			case 'P':
				if (argv[i1][2] == '=') password = argv[i1] + 3;
				else if (!strcmp(work, "PORT") && argv[i1][5] == '=') portnum = atoi(argv[i1] + 6);
				else usage(FALSE);
				break;
			case 'S':
				if (!argv[i1][2]) shutdown = i1;
				else usage(FALSE);
				break;
			case 'U':
				if (!argv[i1][2]) users = i1;
				else usage(FALSE);
				break;
			case 'X':
				if (!argv[i1][2]) stoplog = i1;
				else usage(FALSE);
				break;
			case 'C':
				if (argv[i1][2] == '=') utility = i1;
				else usage(FALSE);
				break;
			default:
				usage(FALSE);
				break;
			}
		}
		else if (!servername[0]) strcpy(servername, argv[i1]);
		else usage(TRUE);
	}
	if (!users && !shutdown && !newlog && !startlog && !stoplog && !utility) {
		usage(TRUE);
	}
	if (!portnum) {
		portnum = CONNECTPORT;
		if (tcpflags & TCP_SSL) portnum++;
	}
	if (!servername[0]) {
		strcpy(servername, "127.0.0.1");
	}
	dbcfshello();  /* correctly set tcp_utf8 based on version */
	if (users) {
		dbcfsusers();
	}
	if (newlog) {
		if (!(tcpflags & TCP_UTF8)) death1("new log option not supported with DB/C FS 2");
		dbcfsadmin(NULL, ADMIN_NEWLOG);
	}
	if (startlog) {
		if (!(tcpflags & TCP_UTF8)) death1("start log option not supported with DB/C FS 2");
		dbcfsadmin(NULL, ADMIN_STARTLOG);
	}
	if (stoplog) {
		if (!(tcpflags & TCP_UTF8)) death1("stop log option not supported with DB/C FS 2");
		dbcfsadmin(NULL, ADMIN_STOPLOG);
	}
	if (shutdown) {
		dbcfsadmin(NULL, ADMIN_SHUTDOWN);
	}
	if (utility) {
		if (!(tcpflags & TCP_UTF8)) death1("utility option not supported with DB/C FS 2");
		dbcfsadmin(argv[utility] + 3, ADMIN_UTILITY);
	}
	tcpcleanup();
	return 0;
}

static void dbcfshello()
{
	INT i1, bufpos, len, majorver;
	UCHAR workbuffer[256];
	SOCKET sockethandle;

	sockethandle = tcpconnect(servername, portnum, tcpflags | TCP_CLNT, NULL, 20);
	if (sockethandle == INVALID_SOCKET) death1(tcpgeterror());
	memset(workbuffer, ' ', 40);
	memcpy(workbuffer + 24, "HELLO", 5);
	workbuffer[39] = '0';
	i1 = tcpsend(sockethandle, workbuffer, 40, tcpflags, 15);
	if (i1 != 40) {
		if (tcpflags & TCP_SSL) tcpsslcomplete(sockethandle);
		closesocket(sockethandle);
		if (i1 == -1) death1(tcpgeterror());
		death1("dbcfs server failed to respond");
	}

	for (bufpos = 0; ; ) {
		i1 = tcprecv(sockethandle, workbuffer + bufpos, sizeof(workbuffer) - 1 - bufpos, tcpflags, 20);
		if (i1 <= 0) {
			if (tcpflags & TCP_SSL) tcpsslcomplete(sockethandle);
			closesocket(sockethandle);
			if (i1 == 0) death1("dbcfs server failed to respond");
			death1(tcpgeterror());
		}
		bufpos += i1;
		if (bufpos >= 24) {
			tcpntoi(workbuffer + 16, 8, &len);
			if (len < 0 || len > (INT) (sizeof(workbuffer) - 16)) {
				if (tcpflags & TCP_SSL) tcpsslcomplete(sockethandle);
				closesocket(sockethandle);
				death1("returned data appears to be corrupt");
			}
			if (bufpos >= len + 24) break;
		}
	}
	if (tcpflags & TCP_SSL) tcpsslcomplete(sockethandle);
	closesocket(sockethandle);

	if (memcmp(workbuffer + 8, "OK      ", 8)) {
		if (len) {
			workbuffer[24 + len] = '\0';
			fputs("ERROR: server failed hello request: ", stdout);
			fputs((CHAR *)(workbuffer + 24), stdout);
			fputs("\n", stdout);
		}
		else fputs("ERROR: server failed hello request, reason unknown\n", stdout);
		death1(NULL);
	}
	if (len < 9 || memcmp(workbuffer + 24, "DB/C FS ", 8) || !isdigit(workbuffer[32])) death1("invalid hello result");
	for (majorver = 0, i1 = 32; i1 < 24 + len && isdigit(workbuffer[i1]); i1++)
			majorver = majorver * 10 + workbuffer[i1] - '0';
	if (majorver < 3 || majorver > FS_MAJOR_VERSION) {
		sprintf((char *) workbuffer, "DB/C FS server must be version >=3 and <=%d", FS_MAJOR_VERSION);
		death1((CHAR*) workbuffer);
	}
}

static void dbcfsadmin(CHAR *parameter, INT type)
{
	INT i1, i2, bufpos, len;
	UCHAR workbuffer[256];
	SOCKET sockethandle;

	sockethandle = tcpconnect(servername, portnum, tcpflags | TCP_CLNT, NULL, 20);
	if (sockethandle == INVALID_SOCKET) death1(tcpgeterror());
	memset(workbuffer, ' ', 24);
	if (type == ADMIN_SHUTDOWN) memcpy(workbuffer + 24, "SHUTDOWN", 8 * sizeof(CHAR));
	else if (type == ADMIN_NEWLOG) memcpy(workbuffer + 24, "NEWLOG  ", 8 * sizeof(CHAR));
	else if (type == ADMIN_STARTLOG) memcpy(workbuffer + 24, "STARTLOG", 8 * sizeof(CHAR));
	else if (type == ADMIN_STOPLOG) memcpy(workbuffer + 24, "STOPLOG ", 8 * sizeof(CHAR));
	else if (type == ADMIN_UTILITY) memcpy(workbuffer + 24, "COMMAND ", 8 * sizeof(CHAR));
	else return;
	if (parameter != NULL) {
		if (password != NULL) {
			strcpy((CHAR *)(workbuffer + 40), password);
			i1 = (INT)strlen(password);
			workbuffer[40 + i1++] = (CHAR) 0x0B; /* delimiter */
			strcpy((CHAR *)(workbuffer + 40 + i1), parameter);
			i1 += (INT)strlen(parameter);
		}		
		else {
			strcpy((CHAR *)(workbuffer + 40), parameter);	
			i1 = (INT)strlen(parameter);
		}
	}
	else if (password != NULL) {
		strcpy((CHAR *)(workbuffer + 40), password);
		i1 = (INT)strlen(password);
	}
	else i1 = 0;
	tcpiton(i1, workbuffer + 32, 8);
	i2 = tcpsend(sockethandle, workbuffer, 40 + i1, tcpflags, 15);
	if (i2 != 40 + i1) {
		if (tcpflags & TCP_SSL) tcpsslcomplete(sockethandle);
		closesocket(sockethandle);
		if (i2 == -1) death1(tcpgeterror());
		death1("dbcfs server failed to respond");
	}
	for (bufpos = 0; ; ) {
		i1 = tcprecv(sockethandle, workbuffer + bufpos, sizeof(workbuffer) - 1 - bufpos, tcpflags, 20);
		if (i1 <= 0) {
			if (tcpflags & TCP_SSL) tcpsslcomplete(sockethandle);
			closesocket(sockethandle);
			if (i1 == 0) death1("dbcfs server failed to respond to request");
			death1(tcpgeterror());
		}
		bufpos += i1;
		if (bufpos >= 24) {
			tcpntoi(workbuffer + 16, 8, &len);
			if (len < 0 || len > (INT) (sizeof(workbuffer) - 16)) {
				if (tcpflags & TCP_SSL) tcpsslcomplete(sockethandle);
				closesocket(sockethandle);
				death1("returned data appears to be corrupt");
			}
			if (bufpos >= len + 24) break;
		}
	}
	if (tcpflags & TCP_SSL) tcpsslcomplete(sockethandle);
	closesocket(sockethandle);

	if (memcmp(workbuffer + 8, "OK      ", 8)) {
		if (len) {
			workbuffer[24 + len] = '\0';
			if (type == ADMIN_SHUTDOWN) fputs("ERROR: server failed shutdown request: ", stdout);
			else if (type == ADMIN_NEWLOG) fputs("ERROR: server failed create new log file request: ", stdout);
			else if (type == ADMIN_STARTLOG) fputs("ERROR: server failed start log file request: ", stdout);
			else if (type == ADMIN_STOPLOG) fputs("ERROR: server failed stop log file request: ", stdout);
			else if (type == ADMIN_UTILITY) fputs("ERROR: execution of utility returned an error: ", stdout);
			fputs((CHAR *)(workbuffer + 24), stdout);
			fputs("\n", stdout);
		}
		else fputs("ERROR: server failed create new log file request, reason unknown\n", stdout);
		death1(NULL);
	}
	if (type == ADMIN_SHUTDOWN) fputs("Shutdown communicated to dbcfs server\n", stdout);
	else if (type == ADMIN_NEWLOG) fputs("Create new log file communicated to dbcfs server\n", stdout);
	else if (type == ADMIN_STARTLOG) fputs("Start log file communicated to dbcfs server\n", stdout);
	else if (type == ADMIN_STOPLOG) fputs("Stop log file communicated to dbcfs server\n", stdout);
	else if (type == ADMIN_UTILITY) fputs("Utility executed successfully on server\n", stdout);
}

static void dbcfsusers()
{
	INT i1, i2, bufpos, cnt, len, nextoffset, offset;
	CHAR user[64];
	UCHAR workbuffer[4096 * 2];
	SOCKET sockethandle;

	sockethandle = tcpconnect(servername, portnum, tcpflags | TCP_CLNT, NULL, 20);
	if (sockethandle == INVALID_SOCKET) death1(tcpgeterror());
	memset(workbuffer, ' ', 24);
	memcpy(workbuffer + 24, "SHOWUSRS", 8 * sizeof(CHAR));
	if (password != NULL) {
		i1 = (INT)strlen(password);
		strcpy((CHAR *)(workbuffer + 40), password);
	}
	else i1 = 0;
	tcpiton(i1, workbuffer + 32, 8);
	i2 = tcpsend(sockethandle, workbuffer, 40 + i1, tcpflags, 15);
	if (i2 != 40 + i1) {
		if (tcpflags & TCP_SSL) tcpsslcomplete(sockethandle);
		closesocket(sockethandle);
		if (i2 == -1) death1(tcpgeterror());
		death1("dbcfs server failed to respond");
	}
	for (bufpos = cnt = len = 0; ; ) {
		i1 = tcprecv(sockethandle, workbuffer + bufpos, sizeof(workbuffer) - 1 - bufpos, tcpflags, 20);
		if (i1 <= 0) {
			if (tcpflags & TCP_SSL) tcpsslcomplete(sockethandle);
			closesocket(sockethandle);
			if (i1 == 0) death1("dbcfs server failed to respond to show user request");
			death1(tcpgeterror());
		}
		bufpos += i1;
		if (!len) {
			if (bufpos < 24) continue;
			tcpntoi(workbuffer + 16, 8, &len);
			if (len < 0 || (size_t) len > sizeof(workbuffer) - 16) {
				if (tcpflags & TCP_SSL) tcpsslcomplete(sockethandle);
				closesocket(sockethandle);
				death1("returned data appears to be corrupt");
			}
			if (memcmp(workbuffer + 8, "OK      ", 8)) {
				if (tcpflags & TCP_SSL) tcpsslcomplete(sockethandle);
				closesocket(sockethandle);
				if (len) {
					workbuffer[24 + len] = '\0';
					fputs("ERROR: server failed show users request: ", stdout);
					fputs((CHAR *)(workbuffer + 24), stdout);
					fputs("\n", stdout);
				}
				else fputs("ERROR: server failed show users request, reason unknown\n", stdout);
				death1(NULL);
			}
			if (!len) break;
			len += 24;
			nextoffset = 24;
		}
		if (bufpos < len && (size_t) bufpos < sizeof(workbuffer) - 32) continue;

		/* workbuffer is either complete or full */
		if (!(tcpflags & TCP_UTF8) && !cnt) {  /* check for v2 "No users" */
			if (len == 32 && !memcmp(workbuffer + nextoffset, "No users", 8)) break;
		}
		while ((bufpos >= len && nextoffset < bufpos) || nextoffset + 128 < bufpos) {  /* display a user */
			offset = nextoffset;
			if (!(tcpflags & TCP_UTF8)) {  /* dbcfs version 2 */
				for ( ; nextoffset < bufpos && workbuffer[nextoffset] != ';'; nextoffset++) {}
				i1 = nextoffset - offset;
				if (nextoffset < bufpos) nextoffset++;
				if ((size_t) i1 >= sizeof(user) - 16) death1("invalid show user data format");
				memcpy(user, workbuffer + offset, i1);
				user[i1] = '\n';
				user[i1 + 1] = '\0';
				cnt++;
			}
			else {
				i1 = tcpnextdata(workbuffer, bufpos, &offset, &nextoffset);
				if ((size_t) i1 >= sizeof(user) - 16) death1("invalid show user data format");
				i2 = tcpitoa(++cnt, user);
				user[i2++] = '.';
				user[i2++] = ' ';
				if (i1) {
					if (i1 == -1) break;
					memcpy(user + i2, workbuffer + offset, i1);
					user[i1 + i2] = '\n';
					user[i1 + i2 + 1] = '\0';
				}
				else strcpy(user + i2, "<unknown>\n");
			}
			fputs(user, stdout);
		}
		if (nextoffset >= len) break;
		len -= nextoffset;
		memmove(workbuffer, workbuffer + nextoffset, bufpos - nextoffset);
		bufpos = nextoffset = 0;
	}
	if (tcpflags & TCP_SSL) tcpsslcomplete(sockethandle);
	closesocket(sockethandle);
	if (!cnt) fputs("No users\n", stdout);
}

static void death1(CHAR *msg)
{
	if (msg != NULL) fprintf(stdout, "ERROR: %s\n", msg);
	fflush(stdout);
#if OS_WIN32
	WSACleanup();
#endif
	exit(1);
}

static void usage(INT headerflag)
{
	/*     12345678901234567890123456789012345678901234567890123456789012345678901234567890 */
	if (headerflag) fputs("DBCFSADM command  " RELEASEPROGRAM RELEASE COPYRIGHT "\n", stdout);
	fputs("Usage:  dbcfsadm [servername] [-port=n] [-noencrypt] [-p=password]\n", stdout);
	fputs("                 [-s] [-u] [-l] [-g] [-x] [-c=\"utility & arguments\"] [-?]\n\n", stdout);
	fputs("        -port        Server port\n", stdout);
	fputs("        -noencrypt   Disable encryption\n", stdout);
	fputs("        -p           Password for privileged commands\n", stdout);
	fputs("        -s           Shutdown FS\n", stdout);
	fputs("        -u           Show users\n", stdout);
	fputs("        -l           Start server logging\n", stdout);
	fputs("        -g           Rename server log\n", stdout);	
	fputs("        -x           Stop server logging\n", stdout);
	fputs("        -c           Execute utility on server\n", stdout);		
	fputs("\n", stdout);
	exit(0);
}
