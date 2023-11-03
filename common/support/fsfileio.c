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

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64) || defined(__NT__) || defined(__WINDOWS_386__)
#define OS_WIN32 1
#else
#define OS_WIN32 0
#endif

#if defined(UNIX) || defined(unix)
#define OS_UNIX 1
#else
#define OS_UNIX 0
#endif

#if OS_WIN32
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#define ERRORVALUE() WSAGetLastError()
#endif

#if OS_UNIX
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#if !defined(__MACOSX) && !defined(Linux) && !defined(FREEBSD)
// Is there anything left that uses this?
#include <stropts.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#ifdef __IBM390
#include <netinet/tcp_var.h>
#include <xti.h>
#include <arpa/inet.h>
#else
#include <netinet/tcp.h>
#endif
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
#define HANDLE INT
#endif

/* standard data type definitions */
#if (OS_WIN32 == 0)

#if !defined(FS_UNIX_ODBC) && !defined(__SQLTYPES_H) /* source files that include sqltypes.h will have these already defined */
typedef char CHAR;
typedef wchar_t WCHAR;
typedef unsigned char BYTE;
typedef unsigned char UCHAR;
typedef unsigned int UINT;
typedef unsigned long int ULONG;
typedef unsigned short int USHORT;
#endif
typedef short int SHORT;
typedef int INT;
typedef long int LONG;
typedef int INT32;
typedef unsigned int UINT32;
typedef double DOUBLE;
#endif
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#include "fsfileio.h"
#include "tcp.h"
#include "base.h"

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long) 0xFFFFFFFF)
#endif

#define CONNECTPORT		9584

#define CONNECTHANDLE_BASE	1001
#define FILEHANDLE_BASE		10001

#define MSID_SIZE	8
#define CNID_SIZE	8
#define FSID_SIZE	8
#define FUNC_SIZE	8
#define SIZE_SIZE	8
#define RTCD_SIZE	8

#define REQUEST_MSID_OFFSET	0
#define REQUEST_CNID_OFFSET	MSID_SIZE
#define REQUEST_FSID_OFFSET	(MSID_SIZE + CNID_SIZE)
#define REQUEST_FUNC_OFFSET	(MSID_SIZE + CNID_SIZE + FSID_SIZE)
#define REQUEST_SIZE_OFFSET	(MSID_SIZE + CNID_SIZE + FSID_SIZE + FUNC_SIZE)
#define REQUEST_SIZE		(MSID_SIZE + CNID_SIZE + FSID_SIZE + FUNC_SIZE + SIZE_SIZE)

#define RETURN_MSID_OFFSET	0
#define RETURN_RTCD_OFFSET	MSID_SIZE
#define RETURN_SIZE_OFFSET	(MSID_SIZE + RTCD_SIZE)
#define RETURN_SIZE			(MSID_SIZE + RTCD_SIZE + SIZE_SIZE)

#define CNID_NULL		"        "
#define FSID_NULL		"        "

#define FUNC_HELLO		"HELLO   "
#define FUNC_SHUTDOWN	"SHUTDOWN"
#define FUNC_SHOWUSRS	"SHOWUSRS"
#define FUNC_START		"START   "
#define FUNC_CONNECT	"CONNECT "
#define FUNC_DISCONNECT	"DISCNCT "
#define FUNC_OPEN		"OPEN    "
#define FUNC_CLOSE		"CLOSE   "
#define FUNC_FPOSIT		"FPOSIT  "
#define FUNC_REPOSIT	"REPOSIT "
#define FUNC_READ		"READ    "
#define FUNC_WRITE		"WRITE   "
#define FUNC_INSERT		"INSERT  "
#define FUNC_UPDATE		"UPDATE  "
#define FUNC_DELETE		"DELETE  "
#define FUNC_DELETEKEY	"DELETEK "
#define FUNC_RECUNLOCK	"UNLOCK  "
#define FUNC_COMPARE	"COMPARE "
#define FUNC_FILELOCK	"FLOCK   "
#define FUNC_FILEUNLOCK	"FUNLOCK "
#define FUNC_WRITEEOF	"WEOF    "
#define FUNC_GETSIZE	"SIZE    "
#define FUNC_RENAME		"RENAME  "
#define FUNC_COMMAND	"COMMAND "

#define RTCD_OK			"OK      "
#define RTCD_ATEOF		"ATEOF   "
#define RTCD_PASTEOF	"PASEEOF "
#define RTCD_NOREC		"NOREC   "
#define RTCD_LOCKED		"LOCKED  "
#define RTCD_LESS		"LESS    "
#define RTCD_GREATER	"GREATER "
#define RTCD_SAME		"SAME    "
#define RTCD_ERR		"ERR     "

#define COMMUNICATE_OK		0
#define COMMUNICATE_ATEOF	1
#define COMMUNICATE_PASTEOF	2
#define COMMUNICATE_NOREC	3
#define COMMUNICATE_LOCKED	4
#define COMMUNICATE_LESS	5
#define COMMUNICATE_GREATER	6
#define COMMUNICATE_SAME	7
#define COMMUNICATE_ERROR	8

#define FS_MAXREADSIZE	65536
#define FS_MAXWRITESIZE	65536

#ifndef _DEBUG
#define CONNECTION_TIMEOUT 10
#else
#define CONNECTION_TIMEOUT 3600
#endif

typedef struct {
	int count;
	int port;
	int majorver;
	int minorver;
	int tcpflags;
	SOCKET sockethandle;
	char cnid[CNID_SIZE];
	char computer[256];
} CONNECTTABLESTRUCT;

typedef struct {
	int inuse;
	int fshandle;
	char fsid[FSID_SIZE];
} FILETABLESTRUCT;

static int fstablehi, fstablesize;
static CONNECTTABLESTRUCT *fstable;
static int filetablehi, filetablesize;
static FILETABLESTRUCT *filetable;
static char fserrstr[256];

#if OS_WIN32
static int startupcnt;
#endif

static int newfileentry(int, FILETABLESTRUCT **, CONNECTTABLESTRUCT **);
static int getfileentry(int, FILETABLESTRUCT **, CONNECTTABLESTRUCT **);
static int communicate(SOCKET, int, char *, char *, char *, char *, int, char *, int, char *, int *);
static void fsntooff(unsigned char *, OFFSET *, int);
static int fsofftoa(OFFSET, char *);

int fsgetgreeting(char *server, int port, int encryptionflag, char *authfile, char *msg, int msglength)
{
	int i1, tcpflags;
	SOCKET sockethandle;
#if OS_WIN32
	WSADATA versioninfo;
#endif

	fserrstr[0] = '\0';
	if (--msglength < 0) {
		strcpy(fserrstr, "length parameter must be positive value");
		return -1;
	}
	if (encryptionflag) {
		if (!tcpissslsupported()) {
			strcpy(fserrstr, "encryption requested but ssl not linked in");
			return -1;
		}
		tcpflags = TCP_UTF8 | TCP_SSL;
	}
	else tcpflags = TCP_UTF8;
	if (!port) {
		if (encryptionflag) port = CONNECTPORT + 1;
		else port = CONNECTPORT;
	}

#if OS_WIN32
	if (!startupcnt) {
		if ( (i1 = WSAStartup(MAKEWORD(2,2), &versioninfo)) ) {
			sprintf(fserrstr, "WSAStartup() failed, error = %d", i1);
			return -1;
		}
	}
	startupcnt++;
#endif

	/* establish socket connection to the server */
	sockethandle = tcpconnect(server, port, tcpflags | TCP_CLNT, NULL /*authfile*/, CONNECTION_TIMEOUT);
	if (sockethandle == INVALID_SOCKET) {
		strcpy(fserrstr, tcpgeterror());
#if OS_WIN32
		if (!--startupcnt) WSACleanup();
#endif
		return -1;
	}

	i1 = communicate(sockethandle, tcpflags, CNID_NULL, FSID_NULL, FUNC_HELLO, NULL, 0, NULL, 0, msg, &msglength);
	if (tcpflags & TCP_SSL) tcpsslcomplete(sockethandle);
	closesocket(sockethandle);
#if OS_WIN32
	if (!--startupcnt) WSACleanup();
#endif
	if (i1 < 0) return i1;
	msg[msglength] = '\0';
	return 0;
}

int fsgeterror(char *msg, int msglength)
{
	size_t i1; // @suppress("Type cannot be resolved")

	if (--msglength < 0) return -1;

	i1 = strlen(fserrstr);
	if ((int)i1 > msglength) i1 = msglength;
	memcpy(msg, fserrstr, i1);
	msg[i1] = '\0';
	return 0;
}

int fsconnect(char *server, int serverport, int localport, int encryptionflag, char *authfile, char *database, char *user, char *password)
{
	int i1;
	int connecthandle, majorver, minorver, tcpflags, buflen;
	char buf[2048], *ptr;
	SOCKET mainsocket, tempsocket;
	CONNECTTABLESTRUCT *tableptr;
#if OS_WIN32
	WSADATA versioninfo;
#endif

	fserrstr[0] = '\0';
	if (encryptionflag) {
		if (!tcpissslsupported()) {
			strcpy(fserrstr, "encryption requested but ssl not linked in");
			return -1;
		}
		tcpflags = TCP_UTF8 | TCP_SSL;
	}
	else tcpflags = TCP_UTF8;
	if (!serverport) {
		if (encryptionflag) serverport = CONNECTPORT + 1;
		else serverport = CONNECTPORT;
	}
	if (user == NULL || !*user) user = "DEFAULTUSER";
	if (password == NULL || !*password) password = "PASSWORD";

	for (i1 = 0, connecthandle = fstablehi; (int)i1 < fstablehi; i1++) {
		if (fstable[i1].count) {
			if (!strcmp(server, fstable[i1].computer) && serverport == fstable[i1].port) {
				fstable[i1].count++;
				return (int)i1 + CONNECTHANDLE_BASE;
			}
		}
		else if ((int)i1 < connecthandle) connecthandle = (int)i1;
	}
	if (connecthandle == fstablesize) {
		if (!fstablesize) tableptr = (CONNECTTABLESTRUCT *) malloc(4 * sizeof(CONNECTTABLESTRUCT));
		else tableptr = (CONNECTTABLESTRUCT *) realloc(fstable, (fstablesize + 4) * sizeof(CONNECTTABLESTRUCT));
		if (tableptr != NULL) i1 = 0;
		else i1 = -1;
		if (i1 == -1) {
			strcpy(fserrstr, "insufficient memory to allocate for file server table");
			return -1;
		}
		fstable = tableptr;
		fstablesize += 4;
	}

#if OS_WIN32
	if (!startupcnt) {
		if ( (i1 = WSAStartup(MAKEWORD(2,2), &versioninfo)) ) {
			sprintf(fserrstr, "WSAStartup() failed, error = %d", i1);
			return -1;
		}
	}
	startupcnt++;
#endif

	/* establish socket connection to the server to get FS version number */
	mainsocket = tcpconnect(server, serverport, tcpflags | TCP_CLNT, NULL /*authfile*/, CONNECTION_TIMEOUT);
	if (mainsocket == INVALID_SOCKET) {
		strcpy(fserrstr, tcpgeterror());
#if OS_WIN32
		if (!--startupcnt) WSACleanup();
#endif
		return -1;
	}

	buflen = (int)sizeof(buf);
	i1 = communicate(mainsocket, tcpflags, CNID_NULL, FSID_NULL, FUNC_HELLO, NULL, 0, NULL, 0, buf, &buflen);
	/* server closes socket after hello */
	if (tcpflags & TCP_SSL) tcpsslcomplete(mainsocket);
	closesocket(mainsocket);
	if (i1 < 0) {
#if OS_WIN32
		if (!--startupcnt) WSACleanup();
#endif
		return (int)i1;
	}
	if (buflen < 9 || memcmp(buf, "DB/C FS ", 8) || !isdigit(buf[8])) {
		sprintf(fserrstr, "DB/C FS server returned unrecognized hello data");
#if OS_WIN32
		if (!--startupcnt) WSACleanup();
#endif
		return -1;
	}
	for (majorver = 0, i1 = 8; (int)i1 < buflen && isdigit(buf[i1]); i1++)
		majorver = majorver * 10 + buf[i1] - '0';
	for (minorver = 0, ++i1; (int)i1 < buflen && isdigit(buf[i1]); i1++)
		minorver = minorver * 10 + buf[i1] - '0';
	if (majorver == 2) tcpflags = 0;
	else if (localport != -1) {  /* set up listening socket */
		/* begin listening for connection from server */
		tempsocket = tcplisten(localport, &localport);
		if (tempsocket == INVALID_SOCKET) {
			strcpy(fserrstr, tcpgeterror());
#if OS_WIN32
			if (!--startupcnt) WSACleanup();
#endif
			return -1;
		}
	}
	else localport = 0;

	/* re-connect and send connection request */
	mainsocket = tcpconnect(server, serverport, tcpflags | TCP_CLNT, NULL /*authfile*/, CONNECTION_TIMEOUT);
	if (mainsocket == INVALID_SOCKET) {
		strcpy(fserrstr, tcpgeterror());
#if OS_WIN32
		if (!--startupcnt) WSACleanup();
#endif
		return -1;
	}

	if (majorver == 2) {
		/* pack connection data buffer */
		strcpy(buf, "NAME=");
		strcat(buf, user);
		strcat(buf, " PASSWORD=");
		strcat(buf, password);
		strcat(buf, " FILE=");
		strcat(buf, database);
		i1 = (int)strlen(buf);
		ptr = FUNC_CONNECT;
	}
	else {  /* start connection */
		i1 = tcpitoa(localport, buf);
		buf[i1++] = ' ';
		i1 += tcpquotedcopy((unsigned char *)(buf + i1), (unsigned char *) user, -1);
		ptr = FUNC_START;
	}

	/* request connection or start */
	buflen = (int)sizeof(buf);
	i1 = communicate(mainsocket, tcpflags, CNID_NULL, FSID_NULL, ptr, buf, (int)i1, NULL, 0, buf, &buflen);
	/* server closes socket after connect/start */
	if (tcpflags & TCP_SSL) tcpsslcomplete(mainsocket);
	closesocket(mainsocket);
	if (i1 < 0) {
		if (majorver != 2 && localport != 0) closesocket(tempsocket);
#if OS_WIN32
		if (!--startupcnt) WSACleanup();
#endif
		return (int)i1;
	}

	if (majorver == 2) {
		/* get the new port number to connect to */
		serverport = 0;
		for (i1 = CNID_SIZE; (int)i1 < buflen && buf[i1] != ':'; i1++);
		if ((int)i1 < buflen) {
			buf[i1] = 0;
			for (i1++; ; i1++) {
				if (isspace(buf[i1])) continue;
				else if (isdigit(buf[i1])) serverport = serverport * 10 + buf[i1] - '0';
				else break;
			}
		}
		if (!serverport) {
			strcpy(fserrstr, "communication error with server, invalid return connect packet");
#if OS_WIN32
			if (!--startupcnt) WSACleanup();
#endif
			return -1;
		}

		/* re-establish socket connection to the returned port number */
		mainsocket = tcpconnect(server, serverport, tcpflags | TCP_CLNT, NULL /*authfile*/, CONNECTION_TIMEOUT);
		if (mainsocket == INVALID_SOCKET) {
			strcpy(fserrstr, tcpgeterror());
#if OS_WIN32
			if (!--startupcnt) WSACleanup();
#endif
			return -1;
		}
	}
	else {  /* not version 2 server, wait for connection back from server */
		if (localport) {
			mainsocket = tcpaccept(tempsocket, tcpflags | TCP_CLNT, NULL /*authfile*/, CONNECTION_TIMEOUT);
			if (mainsocket == INVALID_SOCKET) {
				strcpy(fserrstr, tcpgeterror());
#if OS_WIN32
				if (!--startupcnt) WSACleanup();
#endif
				return -1;
			}
		}
		else {  /* s-port connection */
			for (serverport = 0, i1 = 0; (int)i1 < buflen; i1++)
				if (isdigit(buf[i1])) serverport = serverport * 10 + buf[i1] - '0';
			if (!serverport) {
				strcpy(fserrstr, "communication error with server, invalid return connect packet");
#if OS_WIN32
				if (!--startupcnt) WSACleanup();
#endif
				return -1;
			}

			/* re-establish socket connection to the returned port number */
			mainsocket = tcpconnect(server, serverport, tcpflags | TCP_CLNT, NULL /*authfile*/, CONNECTION_TIMEOUT);
			if (mainsocket == INVALID_SOCKET) {
				strcpy(fserrstr, tcpgeterror());
#if OS_WIN32
				if (!--startupcnt) WSACleanup();
#endif
				return -1;
			}
		}

		/* pack connection data buffer */
		i1 = tcpquotedcopy((unsigned char *) buf, (unsigned char *) user, -1);

		buf[i1++] = ' ';
		i1 += tcpquotedcopy((unsigned char *)(buf + i1), (unsigned char *) password, -1);

		buf[i1++] = ' ';
		memcpy(buf + i1, "FILE", 4);
		i1 += 4;

		buf[i1++] = ' ';
		i1 += tcpquotedcopy((unsigned char *)(buf + i1), (unsigned char *) database, -1);

		/* request connection */
		buflen = (int)sizeof(buf);
		i1 = communicate(mainsocket, tcpflags, CNID_NULL, FSID_NULL, FUNC_CONNECT, buf, (int)i1, NULL, 0, buf, &buflen);
		if (i1 < 0) {
			if (tcpflags & TCP_SSL) tcpsslcomplete(mainsocket);
			closesocket(mainsocket);
#if OS_WIN32
			if (!--startupcnt) WSACleanup();
#endif
			return (int)i1;
		}
	}

	strcpy(fstable[connecthandle].computer, server);
	fstable[connecthandle].port = serverport;
	memcpy(fstable[connecthandle].cnid, buf, CNID_SIZE);
	fstable[connecthandle].sockethandle = mainsocket;
	fstable[connecthandle].count = 1;
	fstable[connecthandle].majorver = majorver;
	fstable[connecthandle].minorver = minorver;
	fstable[connecthandle].tcpflags = tcpflags;
	if (connecthandle == fstablehi) fstablehi++;
	return connecthandle + CONNECTHANDLE_BASE;
}

int fsdisconnect(int connecthandle)
{
	int i1;

	fserrstr[0] = 0;
	connecthandle -= CONNECTHANDLE_BASE;
	if (connecthandle < 0 || connecthandle > fstablehi || !fstable[connecthandle].count) {
		strcpy(fserrstr, "attempted disconnect with invalid server handle");
		return -1;
	}
	if (--fstable[connecthandle].count) return 0;

	i1 = communicate(fstable[connecthandle].sockethandle, fstable[connecthandle].tcpflags,
			fstable[connecthandle].cnid, FSID_NULL, FUNC_DISCONNECT, NULL, 0, NULL, 0, NULL, NULL);
	if (fstable[connecthandle].tcpflags & TCP_SSL) tcpsslcomplete(fstable[connecthandle].sockethandle);
	closesocket(fstable[connecthandle].sockethandle);
	if (connecthandle == fstablehi - 1) fstablehi--;
#if OS_WIN32
	if (startupcnt) {
		if (!--startupcnt) WSACleanup();
	}
#endif
	if (i1 < 0) return i1;
	return 0;
}

int fsversion(int connecthandle, int *majorver, int *minorver)
{
	fserrstr[0] = 0;
	connecthandle -= CONNECTHANDLE_BASE;
	if (connecthandle < 0 || connecthandle > fstablehi || !fstable[connecthandle].count) {
		strcpy(fserrstr, "attempted version with invalid server handle");
		return -1;
	}

	if (majorver != NULL) *majorver = fstable[connecthandle].majorver;
	if (minorver != NULL) *minorver = fstable[connecthandle].minorver;
	return 0;
}

int fsopen(int connecthandle, char *txtfilename, int options, int recsize)
{
	int i1, buflen, filehandle, reslen;
	char buf[2048], function[FUNC_SIZE + 1], *ptr;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (txtfilename == NULL || !*txtfilename) {
		strcpy(fserrstr, "text file name not specified");
		return -1;
	}
	filehandle = newfileentry(connecthandle, &file, &connect);
	if (filehandle == -1) return -1;

	strcpy(function, FUNC_OPEN);
	function[4] = 'F';
	if (options & FS_EXCLUSIVE) function[5] = 'E';
	else if (options & FS_READONLY) function[5] = 'R';
	else function[5] = 'S';

	strcpy(buf, "TEXTFILENAME=");
	buflen = (int)strlen(buf);
	if (connect->majorver == 2) {
		buf[buflen++] = '"';
		strcpy(buf + buflen, txtfilename);
		buflen += (int)strlen(txtfilename);
		buf[buflen++] = '"';
	}
	else buflen += tcpquotedcopy((unsigned char *)(buf + buflen), (unsigned char *) txtfilename, -1);
	if (options & (FS_TEXT | FS_DATA | FS_CRLF | FS_EBCDIC | FS_BINARY)) {
		if (options & FS_TEXT) ptr = " TEXT";
		else if (options & FS_DATA) ptr = " DATA";
		else if (options & FS_CRLF) ptr = " CRLF";
		else if (options & FS_EBCDIC) ptr = " EBCDIC";
		else ptr = " BINARY";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & FS_VARIABLE) {
		ptr = " VARLEN=";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	else {
		ptr = " FIXLEN=";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (!recsize) recsize = 256;
	buflen += tcpitoa(recsize, buf + buflen);
	if (options & FS_COMPRESSED) {
		ptr = " COMPRESSED";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & FS_LOCKAUTO) {
		ptr = " LOCKAUTO";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & FS_LOCKSINGLE) {
		ptr = " LOCKSINGLE";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & FS_LOCKNOWAIT) {
		ptr = " LOCKNOWAIT";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & FS_NOEXTENSION) {
		ptr = " NOEXTENSION";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}

	reslen = (int)sizeof(buf);
	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid,
			FSID_NULL, function, buf, buflen, NULL, 0, buf, &reslen);
	if (i1 < 0) return i1;
	if (reslen < FSID_SIZE) {
		strcpy(fserrstr, "communication error with server, invalid return open packet");
		return -1;
	}

	memcpy(file->fsid, buf, FSID_SIZE);
	file->inuse = TRUE;
	return filehandle;
}

int fsprep(int connecthandle, char *txtfilename, int options, int recsize)
{
	int i1, buflen, filehandle, reslen;
	char buf[2048], function[FUNC_SIZE + 1], *ptr;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (txtfilename == NULL || !*txtfilename) {
		strcpy(fserrstr, "text file name not specified");
		return -1;
	}
	filehandle = newfileentry(connecthandle, &file, &connect);
	if (filehandle == -1) return -1;

	strcpy(function, FUNC_OPEN);
	function[4] = 'F';
	if (options & FS_CREATEONLY) function[5] = 'Q';
	else function[5] = 'P';

	strcpy(buf, "TEXTFILENAME=");
	buflen = (int)strlen(buf);
	if (connect->majorver == 2) {
		buf[buflen++] = '"';
		strcpy(buf + buflen, txtfilename);
		buflen += (int)strlen(txtfilename);
		buf[buflen++] = '"';
	}
	else buflen += tcpquotedcopy((unsigned char *)(buf + buflen), (unsigned char *) txtfilename, -1);
	if (options & (FS_TEXT | FS_DATA | FS_CRLF | FS_EBCDIC | FS_BINARY)) {
		if (options & FS_TEXT) ptr = " TEXT";
		else if (options & FS_DATA) ptr = " DATA";
		else if (options & FS_CRLF) ptr = " CRLF";
		else if (options & FS_EBCDIC) ptr = " EBCDIC";
		else ptr = " BINARY";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & FS_VARIABLE) {
		ptr = " VARLEN=";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	else {
		ptr = " FIXLEN=";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (!recsize) recsize = 256;
	buflen += tcpitoa(recsize, buf + buflen);
	if (options & FS_COMPRESSED) {
		ptr = " COMPRESSED";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & FS_NOEXTENSION) {
		ptr = " NOEXTENSION";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}

	reslen = (int)sizeof(buf);
	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, FSID_NULL, function, buf, buflen, NULL, 0, buf, &reslen);
	if (i1 < 0) return i1;
	if (reslen < FSID_SIZE) {
		strcpy(fserrstr, "communication error with server, invalid return open packet");
		return -1;
	}

	memcpy(file->fsid, buf, FSID_SIZE);
	file->inuse = TRUE;
	return filehandle;
}

int fsopenisi(int connecthandle, char *isifilename, int options, int recsize, int keysize)
{
	int i1, buflen, filehandle, reslen;
	char buf[2048], function[FUNC_SIZE + 1], *ptr;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (isifilename == NULL || !*isifilename) {
		strcpy(fserrstr, "index file name not specified");
		return -1;
	}
	filehandle = newfileentry(connecthandle, &file, &connect);
	if (filehandle == -1) return -1;

	strcpy(function, FUNC_OPEN);
	function[4] = 'I';
	if (options & FS_EXCLUSIVE) function[5] = 'E';
	else if (options & FS_READONLY) function[5] = 'R';
	else function[5] = 'S';

	strcpy(buf, "INDEXFILENAME=");
	buflen = (int)strlen(buf);
	if (connect->majorver == 2) {
		buf[buflen++] = '"';
		strcpy(buf + buflen, isifilename);
		buflen += (int)strlen(isifilename);
		buf[buflen++] = '"';
	}
	else buflen += tcpquotedcopy((unsigned char *)(buf + buflen), (unsigned char *) isifilename, -1);
	if (options & (FS_TEXT | FS_DATA | FS_CRLF | FS_EBCDIC | FS_BINARY)) {
		if (options & FS_TEXT) ptr = " TEXT";
		else if (options & FS_DATA) ptr = " DATA";
		else if (options & FS_CRLF) ptr = " CRLF";
		else if (options & FS_EBCDIC) ptr = " EBCDIC";
		else ptr = " BINARY";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & FS_VARIABLE) {
		ptr = " VARLEN=";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	else {
		ptr = " FIXLEN=";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (!recsize) recsize = 256;
	buflen += tcpitoa(recsize, buf + buflen);
	if (options & FS_COMPRESSED) {
		ptr = " COMPRESSED";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & FS_LOCKAUTO) {
		ptr = " LOCKAUTO";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & FS_LOCKSINGLE) {
		ptr = " LOCKSINGLE";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & FS_LOCKNOWAIT) {
		ptr = " LOCKNOWAIT";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & (FS_DUP | FS_NODUP)) {
		if (options & FS_DUP) ptr = " DUP";
		else ptr = " NODUP";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (keysize) {
		ptr = " KEYLEN=";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
		buflen += tcpitoa(keysize, buf + buflen);
	}

	reslen = (int)sizeof(buf);
	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, FSID_NULL, function, buf, buflen, NULL, 0, buf, &reslen);
	if (i1 < 0) return i1;
	if (reslen < FSID_SIZE) {
		strcpy(fserrstr, "communication error with server, invalid return open packet");
		return -1;
	}

	memcpy(file->fsid, buf, FSID_SIZE);
	file->inuse = TRUE;
	return filehandle;
}

int fsprepisi(int connecthandle, char *txtfilename, char *isifilename, int options, int recsize, int keysize)
{
	int i1, buflen, filehandle, reslen;
	char buf[2048], function[FUNC_SIZE + 1], *ptr;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (isifilename == NULL || !*isifilename) {
		strcpy(fserrstr, "index file name not specified");
		return -1;
	}
	if (!keysize) {
		strcpy(fserrstr, "keysize not specified");
		return -1;
	}
	filehandle = newfileentry(connecthandle, &file, &connect);
	if (filehandle == -1) return -1;

	strcpy(function, FUNC_OPEN);
	function[4] = 'I';
	function[5] = 'P';

	strcpy(buf, "INDEXFILENAME=");
	buflen = (int)strlen(buf);
	if (connect->majorver == 2) {
		buf[buflen++] = '"';
		strcpy(buf + buflen, isifilename);
		buflen += (int)strlen(isifilename);
		buf[buflen++] = '"';
	}
	else buflen += tcpquotedcopy((unsigned char *)(buf + buflen), (unsigned char *) isifilename, -1);
	if (txtfilename != NULL && *txtfilename) {
		ptr = " TEXTFILENAME=";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
		if (connect->majorver == 2) {
			buf[buflen++] = '"';
			strcpy(buf + buflen, txtfilename);
			buflen += (int)strlen(txtfilename);
			buf[buflen++] = '"';
		}
		else buflen += tcpquotedcopy((unsigned char *)(buf + buflen), (unsigned char *) txtfilename, -1);
	}
	if (options & (FS_TEXT | FS_DATA | FS_CRLF | FS_EBCDIC | FS_BINARY)) {
		if (options & FS_TEXT) ptr = " TEXT";
		else if (options & FS_DATA) ptr = " DATA";
		else if (options & FS_CRLF) ptr = " CRLF";
		else if (options & FS_EBCDIC) ptr = " EBCDIC";
		else ptr = " BINARY";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & FS_VARIABLE) {
		ptr = " VARLEN=";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	else {
		ptr = " FIXLEN=";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (!recsize) recsize = 256;
	buflen += tcpitoa(recsize, buf + buflen);
	if (options & FS_COMPRESSED) {
		ptr = " COMPRESSED";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & (FS_DUP | FS_NODUP)) {
		if (options & FS_DUP) ptr = " DUP";
		else ptr = " NODUP";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	ptr = " KEYLEN=";
	strcpy(buf + buflen, ptr);
	buflen += (int)strlen(ptr);
	buflen += tcpitoa(keysize, buf + buflen);

	reslen = (int)sizeof(buf);
	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, FSID_NULL, function, buf, buflen, NULL, 0, buf, &reslen);
	if (i1 < 0) return i1;
	if (reslen < FSID_SIZE) {
		strcpy(fserrstr, "communication error with server, invalid return open packet");
		return -1;
	}

	memcpy(file->fsid, buf, FSID_SIZE);
	file->inuse = TRUE;
	return filehandle;
}

int fsopenaim(int connecthandle, char *aimfilename, int options, int recsize, char matchchar)
{
	int i1, buflen, filehandle, reslen;
	char buf[2048], function[FUNC_SIZE + 1], *ptr;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (aimfilename == NULL || !*aimfilename) {
		strcpy(fserrstr, "aim file name not specified");
		return -1;
	}
	filehandle = newfileentry(connecthandle, &file, &connect);
	if (filehandle == -1) return -1;

	strcpy(function, FUNC_OPEN);
	function[4] = 'A';
	if (options & FS_EXCLUSIVE) function[5] = 'E';
	else if (options & FS_READONLY) function[5] = 'R';
	else function[5] = 'S';

	strcpy(buf, "INDEXFILENAME=");
	buflen = (int)strlen(buf);
	if (connect->majorver == 2) {
		buf[buflen++] = '"';
		strcpy(buf + buflen, aimfilename);
		buflen += (int)strlen(aimfilename);
		buf[buflen++] = '"';
	}
	else buflen += tcpquotedcopy((unsigned char *)(buf + buflen), (unsigned char *) aimfilename, -1);
	if (options & (FS_TEXT | FS_DATA | FS_CRLF | FS_EBCDIC | FS_BINARY)) {
		if (options & FS_TEXT) ptr = " TEXT";
		else if (options & FS_DATA) ptr = " DATA";
		else if (options & FS_CRLF) ptr = " CRLF";
		else if (options & FS_EBCDIC) ptr = " EBCDIC";
		else ptr = " BINARY";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & FS_VARIABLE) {
		ptr = " VARLEN=";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	else {
		ptr = " FIXLEN=";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (!recsize) recsize = 256;
	buflen += tcpitoa(recsize, buf + buflen);
	if (options & FS_COMPRESSED) {
		ptr = " COMPRESSED";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (matchchar) {
		ptr = " MATCH=";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
		buf[buflen++] = matchchar;
	}
	if (options & FS_LOCKAUTO) {
		ptr = " LOCKAUTO";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & FS_LOCKSINGLE) {
		ptr = " LOCKSINGLE";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & FS_LOCKNOWAIT) {
		ptr = " LOCKNOWAIT";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}

	reslen = (int)sizeof(buf);
	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, FSID_NULL, function, buf, buflen, NULL, 0, buf, &reslen);
	if (i1 < 0) return i1;
	if (reslen < FSID_SIZE) {
		strcpy(fserrstr, "communication error with server, invalid return open packet");
		return -1;
	}

	memcpy(file->fsid, buf, FSID_SIZE);
	file->inuse = TRUE;
	return filehandle;
}

int fsprepaim(int connecthandle, char *txtfilename, char *aimfilename, int options, int recsize, char *keyinfo, char matchchar)
{
	int i1, buflen, filehandle, reslen;
	char buf[2048], function[FUNC_SIZE + 1], *ptr;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (aimfilename == NULL || !*aimfilename) {
		strcpy(fserrstr, "index file name not specified");
		return -1;
	}
	if (keyinfo == NULL || !*keyinfo) {
		strcpy(fserrstr, "key information not specified");
		return -1;
	}
	filehandle = newfileentry(connecthandle, &file, &connect);
	if (filehandle == -1) return -1;

	strcpy(function, FUNC_OPEN);
	function[4] = 'A';
	function[5] = 'P';

	strcpy(buf, "INDEXFILENAME=");
	buflen = (int)strlen(buf);
	if (connect->majorver == 2) {
		buf[buflen++] = '"';
		strcpy(buf + buflen, aimfilename);
		buflen += (int)strlen(aimfilename);
		buf[buflen++] = '"';
	}
	else buflen += tcpquotedcopy((unsigned char *)(buf + buflen), (unsigned char *) aimfilename, -1);
	if (txtfilename != NULL && *txtfilename) {
		ptr = " TEXTFILENAME=";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
		if (connect->majorver == 2) {
			buf[buflen++] = '"';
			strcpy(buf + buflen, txtfilename);
			buflen += (int)strlen(txtfilename);
			buf[buflen++] = '"';
		}
		else buflen += tcpquotedcopy((unsigned char *)(buf + buflen), (unsigned char *) txtfilename, -1);
	}
	if (options & (FS_TEXT | FS_DATA | FS_CRLF | FS_EBCDIC | FS_BINARY)) {
		if (options & FS_TEXT) ptr = " TEXT";
		else if (options & FS_DATA) ptr = " DATA";
		else if (options & FS_CRLF) ptr = " CRLF";
		else if (options & FS_EBCDIC) ptr = " EBCDIC";
		else ptr = " BINARY";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & FS_VARIABLE) {
		ptr = " VARLEN=";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	else {
		ptr = " FIXLEN=";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (!recsize) recsize = 256;
	buflen += tcpitoa(recsize, buf + buflen);
	if (options & FS_COMPRESSED) {
		ptr = " COMPRESSED";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	if (options & FS_CASESENSITIVE) {
		ptr = " CASESENSITIVE";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}
	ptr = " AIMKEY=";
	strcpy(buf + buflen, ptr);
	buflen += (int)strlen(ptr);
	if (connect->majorver == 2) {
		buf[buflen++] = '"';
		strcpy(buf + buflen, keyinfo);
		buflen += (int)strlen(keyinfo);
		buf[buflen++] = '"';
	}
	else buflen += tcpquotedcopy((unsigned char *)(buf + buflen), (unsigned char *) keyinfo, -1);
	if (matchchar) {
		ptr = " MATCH=";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
		buf[buflen++] = matchchar;
	}

	reslen = (int)sizeof(buf);
	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, FSID_NULL, function, buf, buflen, NULL, 0, buf, &reslen);
	if (i1 < 0) return i1;
	if (reslen < FSID_SIZE) {
		strcpy(fserrstr, "communication error with server, invalid return open packet");
		return -1;
	}

	memcpy(file->fsid, buf, FSID_SIZE);
	file->inuse = TRUE;
	return filehandle;
}

int fsopenraw(int connecthandle, char *txtfilename, int options)
{
	int i1, buflen, filehandle, reslen;
	char buf[2048], function[FUNC_SIZE + 1], *ptr;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;


	fserrstr[0] = 0;
	if (txtfilename == NULL || !*txtfilename) {
		strcpy(fserrstr, "text file name not specified");
		return -1;
	}
	filehandle = newfileentry(connecthandle, &file, &connect);
	if (filehandle == -1) return -1;

	strcpy(function, FUNC_OPEN);
	function[4] = 'H';
	if (options & FS_EXCLUSIVE) function[5] = 'E';
	else if (options & FS_READONLY) function[5] = 'R';
	else function[5] = 'S';

	strcpy(buf, "TEXTFILENAME=");
	buflen = (int)strlen(buf);
	if (connect->majorver == 2) {
		buf[buflen++] = '"';
		strcpy(buf + buflen, txtfilename);
		buflen += (int)strlen(txtfilename);
		buf[buflen++] = '"';
	}
	else buflen += tcpquotedcopy((unsigned char *)(buf + buflen), (unsigned char *) txtfilename, -1);
	if (options & FS_NOEXTENSION) {
		ptr = " NOEXTENSION";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}

	reslen = (int)sizeof(buf);
	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, FSID_NULL, function, buf, buflen, NULL, 0, buf, &reslen);
	if (i1 < 0) return i1;
	if (reslen < FSID_SIZE) {
		strcpy(fserrstr, "communication error with server, invalid return open packet");
		return -1;
	}

	memcpy(file->fsid, buf, FSID_SIZE);
	file->inuse = TRUE;
	return filehandle;
}

int fsprepraw(int connecthandle, char *txtfilename, int options)
{
	int i1, buflen, filehandle, reslen;
	char buf[2048], function[FUNC_SIZE + 1], *ptr;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (txtfilename == NULL || !*txtfilename) {
		strcpy(fserrstr, "text file name not specified");
		return -1;
	}
	filehandle = newfileentry(connecthandle, &file, &connect);
	if (filehandle == -1) return -1;

	strcpy(function, FUNC_OPEN);
	function[4] = 'H';
	if (options & FS_CREATEONLY) function[5] = 'Q';
	else function[5] = 'P';

	strcpy(buf, "TEXTFILENAME=");
	buflen = (int)strlen(buf);
	if (connect->majorver == 2) {
		buf[buflen++] = '"';
		strcpy(buf + buflen, txtfilename);
		buflen += (int)strlen(txtfilename);
		buf[buflen++] = '"';
	}
	else buflen += tcpquotedcopy((unsigned char *)(buf + buflen), (unsigned char *) txtfilename, -1);
	if (options & FS_NOEXTENSION) {
		ptr = " NOEXTENSION";
		strcpy(buf + buflen, ptr);
		buflen += (int)strlen(ptr);
	}

	reslen = sizeof(buf);
	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, FSID_NULL, function, buf, buflen, NULL, 0, buf, &reslen);
	if (i1 < 0) return i1;
	if (reslen < FSID_SIZE) {
		strcpy(fserrstr, "communication error with server, invalid return open packet");
		return -1;
	}

	memcpy(file->fsid, buf, FSID_SIZE);
	file->inuse = TRUE;
	return filehandle;
}

int fsclose(int filehandle)
{
	int i1;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, FUNC_CLOSE, NULL, 0, NULL, 0, NULL, NULL);
	file->inuse = FALSE;
	if (i1 < 0) return i1;
	return 0;
}

int fsclosedelete(int filehandle)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_CLOSE);
	function[5] = 'D';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, NULL, 0, NULL, 0, NULL, NULL);
	file->inuse = FALSE;
	if (i1 < 0) return i1;
	return 0;
}

int fsfposit(int filehandle, OFFSET *offset)
{
	int i1, reslen;
	char buf[32];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	reslen = sizeof(buf);
	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, FUNC_FPOSIT, NULL, 0, NULL, 0, buf, &reslen);
	if (i1 < 0) return i1;
	if (!reslen) {
		strcpy(fserrstr, "communication error with server, invalid return fposit packet");
		return -1;
	}
	fsntooff((unsigned char *) buf, offset, reslen);
	return 0;
}

int fsreposit(int filehandle, OFFSET offset)
{
	int i1, buflen;
	char buf[32];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (offset < 0) {
		strcpy(fserrstr, "invalid offset value specified");
		return -1;
	}
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	buflen = fsofftoa(offset, buf);

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, FUNC_REPOSIT, buf, buflen, NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_ATEOF) return 1;
	if (i1 == COMMUNICATE_PASTEOF) return 2;
	return 0;
}

int fssetposateof(int filehandle)
{
	int i1;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, FUNC_REPOSIT, "-1", 2, NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	return 0;
}

int fsreadrandom(int filehandle, OFFSET recnum, char *record, int length)
{
	int i1, buflen;
	char buf[64], function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;
	if (recnum < 0) {
		strcpy(fserrstr, "invalid record number specified");
		return -1;
	}

	strcpy(function, FUNC_READ);
	function[4] = 'R';
	buflen = fsofftoa(recnum, buf);

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, buf, buflen, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadrandomlock(int filehandle, OFFSET recnum, char *record, int length)
{
	int i1, buflen;
	char buf[64], function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;
	if (recnum < 0) {
		strcpy(fserrstr, "invalid record number specified");
		return -1;
	}

	strcpy(function, FUNC_READ);
	function[4] = 'R';
	function[5] = 'L';
	buflen = fsofftoa(recnum, buf);

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, buf, buflen, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadnext(int filehandle, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'S';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, NULL, 0, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadnextlock(int filehandle, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'S';
	function[5] = 'L';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, NULL, 0, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadprev(int filehandle, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'B';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, NULL, 0, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadprevlock(int filehandle, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'B';
	function[5] = 'L';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, NULL, 0, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadsame(int filehandle, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'Z';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, NULL, 0, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadsamelock(int filehandle, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'Z';
	function[5] = 'L';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, NULL, 0, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadkey(int filehandle, char *key, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'K';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, key,
			(int)strlen(key), NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadkeylock(int filehandle, char *key, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'K';
	function[5] = 'L';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, key,
			(int)strlen(key), NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadkeynext(int filehandle, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'N';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, NULL, 0, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadkeynextlock(int filehandle, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'N';
	function[5] = 'L';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, NULL, 0, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadkeyprev(int filehandle, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'P';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, NULL, 0, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadkeyprevlock(int filehandle, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'P';
	function[5] = 'L';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, NULL, 0, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadaimkey(int filehandle, char *aimkey, char *record, int length)
{
	int i1, len1, len2;
	char function[FUNC_SIZE + 1], keywork[32];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (aimkey == NULL || strlen(aimkey) < 4) {
		strcpy(fserrstr, "invalid aim key specified");
		return -1;
	}
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'A';

	len2 = (int)strlen(aimkey);
	keywork[0] = '1';
	keywork[1] = ' ';
	len1 = tcpitoa(len2, keywork + 2) + 2;
	keywork[len1++] = ' ';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, keywork, len1, aimkey, len2, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadaimkeylock(int filehandle, char *aimkey, char *record, int length)
{
	int i1, len1, len2;
	char function[FUNC_SIZE + 1], keywork[32];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (aimkey == NULL || strlen(aimkey) < 4) {
		strcpy(fserrstr, "invalid aim key specified");
		return -1;
	}
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'A';
	function[5] = 'L';

	len2 = (int)strlen(aimkey);
	keywork[0] = '1';
	keywork[1] = ' ';
	len1 = tcpitoa(len2, keywork + 2) + 2;
	keywork[len1++] = ' ';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, keywork, len1, aimkey, len2, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadaimkeys(int filehandle, char **aimkeys, int aimkeyscount, char *record, int length)
{
	int i1, i2, len;
	char function[FUNC_SIZE + 1], keywork[4096];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'A';

	len = tcpitoa(aimkeyscount, keywork);
	for (i1 = 0; i1 < aimkeyscount; i1++) {
		if (aimkeys[i1] == NULL) i2 = 0;
		else i2 = (int)strlen(aimkeys[i1]);
		if (i2 < 4) {
			strcpy(fserrstr, "invalid aim key specified");
			return -1;
		}
		if (len + i2 + 6 > (int) sizeof(keywork)) {  /* 2 spaces and 4 digit number */
			strcpy(fserrstr, "static work buffer for aimdex key is too small for key specification");
			return -1;
		}
		keywork[len++] = ' ';
		len += tcpitoa(i2, keywork + len);
		keywork[len++] = ' ';
		memcpy(keywork + len, aimkeys[i1], i2);
		len += i2;
	}

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, keywork, len, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadaimkeyslock(int filehandle, char **aimkeys, int aimkeyscount, char *record, int length)
{
	int i1, i2, len;
	char function[FUNC_SIZE + 1], keywork[4096];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'A';
	function[5] = 'L';

	len = tcpitoa(aimkeyscount, keywork);
	for (i1 = 0; i1 < aimkeyscount; i1++) {
		if (aimkeys[i1] == NULL) i2 = 0;
		else i2 = (int)strlen(aimkeys[i1]);
		if (i2 < 4) {
			strcpy(fserrstr, "invalid aim key specified");
			return -1;
		}
		if (len + i2 + 6 > (int) sizeof(keywork)) {  /* 2 spaces and 4 digit number */
			strcpy(fserrstr, "static work buffer for aimdex key is too small for key specification");
			return -1;
		}
		keywork[len++] = ' ';
		len += tcpitoa(i2, keywork + len);
		keywork[len++] = ' ';
		memcpy(keywork + len, aimkeys[i1], i2);
		len += i2;
	}

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, keywork, len, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadaimnext(int filehandle, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'O';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, NULL, 0, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadaimnextlock(int filehandle, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'O';
	function[5] = 'L';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, NULL, 0, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadaimprev(int filehandle, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'Q';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, NULL, 0, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadaimprevlock(int filehandle, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_READ);
	function[4] = 'Q';
	function[5] = 'L';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, NULL, 0, NULL, 0, record, &length);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return -3;
	if (i1 == COMMUNICATE_LOCKED) return -2;

	return length;
}

int fsreadraw(int filehandle, OFFSET offset, char *buffer, int length)
{
	int i1, buflen, readlen, readpos;
	char buf[64], function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;
	if (offset < -1) {
		strcpy(fserrstr, "invalid file offset specified");
		return -1;
	}

	strcpy(function, FUNC_READ);
	function[4] = 'H';

	for (readpos = 0; readpos < length; ) {
		buflen = fsofftoa(offset + readpos, buf);
		buf[buflen++] = ' ';
		readlen = length - readpos;
		if (readlen > FS_MAXREADSIZE) readlen = FS_MAXREADSIZE;
		buflen += tcpitoa(readlen, buf + buflen);

		i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, buf, buflen, NULL, 0, buffer + readpos, &readlen);
		if (i1 < 0) {
			return i1;
		}
		readpos += readlen;
		if (readlen != FS_MAXREADSIZE) break;
	}

	return readpos;
}

int fswriterandom(int filehandle, OFFSET recnum, char *record, int length)
{
	int i1, buflen;
	char buf[64], function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;
	if (recnum < 0) {
		strcpy(fserrstr, "invalid record number specified");
		return -1;
	}

	strcpy(function, FUNC_WRITE);
	function[5] = 'R';
	buflen = fsofftoa(recnum, buf);
	buf[buflen++] = ' ';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, buf, buflen, record, length, NULL, NULL);
	if (i1 < 0) return i1;
	return 0;
}

int fswritenext(int filehandle, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_WRITE);
	function[5] = 'S';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, record, length, NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	return 0;
}

int fswriteateof(int filehandle, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_WRITE);
	function[5] = 'E';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, record, length, NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	return 0;
}

int fswritekey(int filehandle, char *key, char *record, int length)
{
	int i1, buflen, keylen;
	char buf[1024], function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;
	if (key == NULL || !*key) {
		strcpy(fserrstr, "invalid key specified");
		return -1;
	}

	strcpy(function, FUNC_WRITE);
	function[5] = 'K';
	keylen = (int)strlen(key);
	if (keylen + 6 > (int) sizeof(buf)) {  /* 2 spaces and 4 digit number */
		strcpy(fserrstr, "static work buffer for index key is too small for key specification");
		return -1;
	}
	buflen = tcpitoa(keylen, buf);
	buf[buflen++] = ' ';
	memcpy(buf + buflen, key, keylen);
	buflen += keylen;
	buf[buflen++] = ' ';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, buf, buflen, record, length, NULL, NULL);
	if (i1 < 0) return i1;
	return 0;
}

int fswriteaim(int filehandle, char *record, int length)
{
	int i1;
	char function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	strcpy(function, FUNC_WRITE);
	function[5] = 'A';

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, record, length, NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	return 0;
}

int fswriteraw(int filehandle, OFFSET offset, char *buffer, int length)
{
	int i1, buflen, writelen, writepos;
	char buf[64], function[FUNC_SIZE + 1];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;
	if (offset < -1) {
		strcpy(fserrstr, "invalid file offset specified");
		return -1;
	}

	strcpy(function, FUNC_WRITE);
	function[5] = 'H';

	for (writepos = 0; writepos < length; ) {
		buflen = fsofftoa(offset + writepos, buf);
		buf[buflen++] = ' ';
		writelen = length - writepos;
		if (writelen > FS_MAXWRITESIZE) writelen = FS_MAXWRITESIZE;

		i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, function, buf, buflen, buffer + writepos, writelen, NULL, NULL);
		if (i1 < 0) return i1;
		writepos += writelen;
	}
	return 0;
}

int fsinsertkey(int filehandle, char *key)
{
	int i1;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;
	if (key == NULL || !*key) {
		strcpy(fserrstr, "invalid key specified");
		return -1;
	}

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, FUNC_INSERT, key,
			(int)strlen(key), NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	return 0;
}

int fsinsertkeys(int filehandle, char *record, int length)
{
	int i1;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;
	if (record == NULL || !length) {
		strcpy(fserrstr, "invalid record specified");
		return -1;
	}

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, FUNC_INSERT, record, length, NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	return 0;
}

int fsupdate(int filehandle, char *record, int length)
{
	int i1;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, FUNC_UPDATE, record, length, NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	return 0;
}

int fsdelete(int filehandle)
{
	int i1;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, FUNC_DELETE, NULL, 0, NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return 1;
	return 0;
}

int fsdeletekey(int filehandle, char *key)
{
	int i1;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, FUNC_DELETE, key,
			(int)strlen(key), NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return 1;
	return 0;
}

int fsdeletekeyonly(int filehandle, char *key)
{
	int i1;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, FUNC_DELETEKEY, key,
			(int)strlen(key), NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_NOREC) return 1;
	return 0;
}

int fsunlock(int filehandle, OFFSET offset)
{
	int i1, buflen;
	char buf[64];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	buflen = fsofftoa(offset, buf);

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, FUNC_RECUNLOCK, buf, buflen, NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	return 0;
}

int fsunlockall(int filehandle)
{
	int i1;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, FUNC_RECUNLOCK, NULL, 0, NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	return 0;
}

int fsweof(int filehandle, OFFSET offset)
{
	int i1, buflen;
	char buf[64];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	buflen = fsofftoa(offset, buf);

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, FUNC_WRITEEOF, buf, buflen, NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	return 0;
}

int fscompare(int filehandle1, int filehandle2)
{
	int i1;
	CONNECTTABLESTRUCT *connect1, *connect2;
	FILETABLESTRUCT *file1, *file2;

	fserrstr[0] = 0;
	if (getfileentry(filehandle1, &file1, &connect1) == -1) return -1;
	if (getfileentry(filehandle2, &file2, &connect2) == -1) return -1;

	if (connect1 != connect2) {
		strcpy(fserrstr, "attempted compare against 2 different servers");
		return -1;
	}

	i1 = communicate(connect1->sockethandle, connect1->tcpflags, connect1->cnid, file1->fsid, FUNC_COMPARE, file2->fsid, FSID_SIZE, NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	if (i1 == COMMUNICATE_SAME) return 0;
	if (i1 == COMMUNICATE_LESS) return 1;
	if (i1 == COMMUNICATE_GREATER) return 2;
	return -1;
}

int fsfilelock(int filehandle)
{
	int i1;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, FUNC_FILELOCK,
			NULL, 0, NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	return 0;
}

int fsfileunlock(int filehandle)
{
	int i1;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, FUNC_FILEUNLOCK, NULL, 0, NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	return 0;
}

int fsfilesize(int filehandle, OFFSET *size)
{
	int i1, reslen;
	char buf[32];
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	reslen = sizeof(buf);
	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, FUNC_GETSIZE, NULL, 0, NULL, 0, buf, &reslen);
	if (i1 < 0) return i1;
	if (!reslen) {
		strcpy(fserrstr, "communication error with server, invalid return getsize packet");
		return -1;
	}
	fsntooff((unsigned char *) buf, size, reslen);
	return 0;
}

int fsrename(int filehandle, char *newname)
{
	int i1;
	CONNECTTABLESTRUCT *connect;
	FILETABLESTRUCT *file;

	fserrstr[0] = 0;
	if (getfileentry(filehandle, &file, &connect) == -1) return -1;

	i1 = communicate(connect->sockethandle, connect->tcpflags, connect->cnid, file->fsid, FUNC_RENAME, newname,
			(int)strlen(newname), NULL, 0, NULL, NULL);
	file->inuse = FALSE;
	if (i1 < 0) return i1;
	return 0;
}

int fsexecute(int connecthandle, char *command)
{
	int i1;

	fserrstr[0] = 0;
	connecthandle -= CONNECTHANDLE_BASE;
	if (connecthandle < 0 || connecthandle > fstablehi || !fstable[connecthandle].count) {
		strcpy(fserrstr, "attempted execute with invalid server handle");
		return -1;
	}

	i1 = communicate(fstable[connecthandle].sockethandle, fstable[connecthandle].tcpflags, fstable[connecthandle].cnid,
			FSID_NULL, FUNC_COMMAND, command, (int)strlen(command), NULL, 0, NULL, NULL);
	if (i1 < 0) return i1;
	return 0;
}

static int newfileentry(int connecthandle, FILETABLESTRUCT **file, CONNECTTABLESTRUCT **connect)
{
	int i1, filehandle;
	FILETABLESTRUCT *tableptr;

	connecthandle -= CONNECTHANDLE_BASE;
	if (connecthandle < 0 || connecthandle > fstablehi || !fstable[connecthandle].count) {
		strcpy(fserrstr, "attempted operation with invalid connection handle");
		return -1;
	}

	for (filehandle = 0; filehandle < filetablehi && filetable[filehandle].inuse; filehandle++);
	if (filehandle == filetablesize) {
		if (!filetablesize) tableptr = (FILETABLESTRUCT *) malloc(128 * sizeof(FILETABLESTRUCT));
		else tableptr = (FILETABLESTRUCT *) realloc(filetable, (filetablesize + 128) * sizeof(FILETABLESTRUCT));
		if (tableptr != NULL) i1 = 0;
		else i1 = -1;
		if (i1 == -1) {
			strcpy(fserrstr, "insufficient memory to allocate for file table");
			return -1;
		}
		filetable = tableptr;
		filetablesize += 128;
	}
	else while (filetablehi > filehandle && filetable[filetablehi - 1].inuse == FALSE) filetablehi--;

	*connect = &fstable[connecthandle];
	*file = &filetable[filehandle];
	(*file)->fshandle = connecthandle;
	if (filehandle == filetablehi) filetablehi++;
	return filehandle + FILEHANDLE_BASE;
}

static int getfileentry(int filehandle, FILETABLESTRUCT **file, CONNECTTABLESTRUCT **connect)
{
	filehandle -= FILEHANDLE_BASE;
	if (filehandle < 0 || filehandle > filetablehi || !filetable[filehandle].inuse) {
		strcpy(fserrstr, "attempted operation with invalid file handle");
		return -1;
	}
	*file = &filetable[filehandle];
	*connect = &fstable[(*file)->fshandle];
	return 0;
}

static int communicate(SOCKET sockethandle, int tcpflags, char *cnid, char *fsid, char *func,
	char *data1, int datalen1, char *data2, int datalen2, char *result, int *resultlen)
{
	static int serialid = 0;
	int i1, headerflag, invalidflag, len, rc, recvlen, recvpos, reslen;
	char buffer[REQUEST_SIZE + 2048 + 1], error[RTCD_SIZE + 1], *ptr;

	if (!sockethandle) {
		strcpy(fserrstr, "internal error, invalid sockethandle");
		return -1;
	}

	if (data1 == NULL || datalen1 < 0) datalen1 = 0;
	if (data2 == NULL || datalen2 < 0) datalen2 = 0;
	if (++serialid >= 100000000) serialid = 1;

	/* pack message to server */
	tcpiton(serialid, (unsigned char *) buffer + REQUEST_MSID_OFFSET, MSID_SIZE);
	memcpy(buffer + REQUEST_CNID_OFFSET, cnid, CNID_SIZE);
	memcpy(buffer + REQUEST_FSID_OFFSET, fsid, FSID_SIZE);
	memcpy(buffer + REQUEST_FUNC_OFFSET, func, FUNC_SIZE);
	tcpiton(datalen1 + datalen2, (unsigned char *) buffer + REQUEST_SIZE_OFFSET, SIZE_SIZE);
	len = REQUEST_SIZE;	/* REQUEST_SIZE is 40 */
	if (datalen1 && datalen1 <= (int) sizeof(buffer) - len) {
		memcpy(buffer + len, data1, datalen1);
		len += datalen1;
		datalen1 = 0;
	}
	if (!datalen1 && datalen2 && datalen2 <= (int) sizeof(buffer) - len) {
		memcpy(buffer + len, data2, datalen2);
		len += datalen2;
		datalen2 = 0;
	}

	if (tcpsend(sockethandle, (unsigned char *) buffer, len, tcpflags, -1) == -1) {
		strncpy(fserrstr, tcpgeterror(), sizeof(fserrstr));
		fserrstr[sizeof(fserrstr) - 1] = '\0';
		return -1;
	}

	if (datalen1 && tcpsend(sockethandle, (unsigned char *) data1, datalen1, tcpflags, -1) == -1) {
		strncpy(fserrstr, tcpgeterror(), sizeof(fserrstr));
		fserrstr[sizeof(fserrstr) - 1] = '\0';
		return -1;
	}

	if (datalen2 && tcpsend(sockethandle, (unsigned char *) data2, datalen2, tcpflags, -1) == -1) {
		strncpy(fserrstr, tcpgeterror(), sizeof(fserrstr));
		fserrstr[sizeof(fserrstr) - 1] = '\0';
		return -1;
	}

	invalidflag = FALSE;
	recvlen = sizeof(buffer) - (REQUEST_MSID_OFFSET + MSID_SIZE) - 1;
	ptr = buffer + (REQUEST_MSID_OFFSET + MSID_SIZE);
	for (headerflag = TRUE, recvpos = 0; ; ) {
		i1 = tcprecv(sockethandle, (unsigned char *)(ptr + recvpos), recvlen - recvpos, tcpflags, -1);
		if (i1 <= 0) {  /* assume disconnect or timeout */
			if (!i1) strcpy(fserrstr, "receive timed out");
			else {
				strncpy(fserrstr, tcpgeterror(), sizeof(fserrstr));
				fserrstr[sizeof(fserrstr) - 1] = '\0';
			}
			return -1;
		}
		recvpos += i1;
		if (headerflag) {  /* receiving the header */
			if (recvpos < RETURN_SIZE) continue;  /* not complete */
			/* message synchronization id */
			if (memcmp(buffer + REQUEST_MSID_OFFSET, ptr + RETURN_MSID_OFFSET, MSID_SIZE)) {
				strcpy(fserrstr, "message id mismatch from server");
				invalidflag = TRUE;
				rc = -1;
				break;
			}
			tcpntoi((unsigned char *) ptr + RETURN_SIZE_OFFSET, SIZE_SIZE, &len);
			ptr += RETURN_RTCD_OFFSET;
			if (!memcmp(ptr, RTCD_OK, RTCD_SIZE)) {
				rc = COMMUNICATE_OK;
			}
			else if (!memcmp(ptr, RTCD_ATEOF, RTCD_SIZE)) rc = COMMUNICATE_ATEOF;
			else if (!memcmp(ptr, RTCD_PASTEOF, RTCD_SIZE)) rc = COMMUNICATE_PASTEOF;
			else if (!memcmp(ptr, RTCD_NOREC, RTCD_SIZE)) rc = COMMUNICATE_NOREC;
			else if (!memcmp(ptr, RTCD_LOCKED, RTCD_SIZE)) rc = COMMUNICATE_LOCKED;
			else if (!memcmp(ptr, RTCD_LESS, RTCD_SIZE)) rc = COMMUNICATE_LESS;
			else if (!memcmp(ptr, RTCD_GREATER, RTCD_SIZE)) rc = COMMUNICATE_GREATER;
			else if (!memcmp(ptr, RTCD_SAME, RTCD_SIZE)) rc = COMMUNICATE_SAME;
			else if (!memcmp(ptr, RTCD_ERR, 3)) {
				rc = COMMUNICATE_ERROR;
				memcpy(error, ptr + 3, RTCD_SIZE - 3);
				error[RTCD_SIZE - 3] = 0;
			}
			else {
				ptr[RTCD_SIZE] = 0;
				sprintf(fserrstr, "invalid return code returned from server: %s", ptr);
				invalidflag = TRUE;
				rc = -1;
				break;
			}
			ptr += RETURN_SIZE - RETURN_RTCD_OFFSET;
			recvpos -= RETURN_SIZE;  /* RETURN_SIZE is 24 */
			if (resultlen != NULL) {
				reslen = *resultlen;
				*resultlen = len;
			}
			else reslen = len;
			if (result != NULL && rc == COMMUNICATE_OK) {
				recvlen = (reslen < len) ? reslen : len;
				if (recvpos) memcpy(result, ptr, (recvpos < recvlen) ? recvpos : recvlen);
				ptr = result;
			}
			else if (rc == COMMUNICATE_ERROR) {
				if (recvpos) memmove(buffer, ptr, recvpos);
				recvlen = sizeof(buffer) - 1;
				ptr = buffer;
			}
			else {
				len -= recvpos;
				recvpos = 0;
				recvlen = sizeof(buffer);
				ptr = buffer;
			}
			headerflag = FALSE;
		}
		/* receiving the data */
		if (recvpos < recvlen && recvpos < len) continue;  /* not complete */
		if (rc == COMMUNICATE_ERROR) {
			rc = 0 - atoi(error);
			if (!rc) rc = -1;
			buffer[recvpos] = 0;
			buffer[sizeof(fserrstr) - 1] = 0;
			strcpy(fserrstr, buffer);
		}
		len -= recvpos;
		if (len <= 0) break;
		recvpos = 0;
		recvlen = sizeof(buffer);
		ptr = buffer;
	}
	if (invalidflag) {  /* invalid format, empty the socket buffer */
		do i1 = tcprecv(sockethandle, (unsigned char *) buffer, sizeof(buffer), 0, 3);
		while (i1 > 0);
	}
	return(rc);
}

static void fsntooff(unsigned char *src, OFFSET *dest, int n)
{
	int i1, negflg;
	OFFSET num;

	negflg = FALSE;
	for (i1 = -1, num = 0L; ++i1 < n; )
		if (isdigit(src[i1])) num = num * 10 + src[i1] - '0';
		else if (src[i1] == '-') negflg = TRUE;
	if (negflg) *dest = -num;
	else *dest = num;
}

static int fsofftoa(OFFSET src, char *dest)
{
	int i1, negflg;
	char work[32];

	if (src < 0) {
		src = -src;
		negflg = TRUE;
	}
	else negflg = FALSE;

	work[sizeof(work) - 1] = 0;
	i1 = sizeof(work) - 1;
	do work[--i1] = (char)(src % 10 + '0');
	while (src /= 10);
	if (negflg) work[--i1] = '-';
	strcpy(dest, &work[i1]);
	return sizeof(work) - 1 - i1;
}
