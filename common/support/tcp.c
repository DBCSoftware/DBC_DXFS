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

#if defined(WIN32) || defined(_WIN32) || defined(__NT__) || defined(__WINDOWS_386__)
#define OS_WIN32 1
#else
#define OS_WIN32 0
#endif

#if defined(UNIX) || defined(unix) || defined(__MACOSX)
#define OS_UNIX 1
#else
#define OS_UNIX 0
#endif

#ifndef DBC_SSL
#ifndef NO_DBC_SSL
#define DBC_SSL 1
#endif
#endif

#if OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#pragma comment(lib, "Ws2_32.lib")
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
typedef unsigned char BYTE;
typedef struct timeval TIMEVAL;
typedef int INT;
#ifdef FREEBSD
#define FIONBIO_CAST (unsigned long)
#else
#define FIONBIO_CAST (int)
#endif
#endif

#if DBC_SSL
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/opensslv.h>
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#if OS_WIN32
#define ERRORVALUE() WSAGetLastError()
#else
#define ERRORVALUE() errno
#endif

#include "tcp.h"

static char tcperrorstring[256];
static char sslerrorstring[256];
static int initflag = FALSE;
#if DBC_SSL
typedef struct SSL_SOCKET {
	struct SSL_SOCKET *next;
	SSL_CTX *ctx;
	SSL *ssl;
	SOCKET socket;
} SSLSOCK;

//static const char* PREFERRED_CIPHERS = "HIGH:!kRSA:!PSK:!SRP:!MD5:!RC4:!eNULL:!CAMELLIA:!aNULL:!SHA256:!AES128";

static const char* PREFERRED_CIPHERS =
	"ECDHE-ECDSA-AES256-GCM-SHA384:"
	"ECDHE-RSA-AES256-GCM-SHA384:"
	"ECDHE-ECDSA-AES128-GCM-SHA256:"
	"ECDHE-RSA-AES128-GCM-SHA256:"
	"ECDHE-ECDSA-AES256-SHA384:"
	"ECDHE-RSA-AES256-SHA384:"
	"ECDHE-ECDSA-AES128-SHA256:"
	"ECDHE-RSA-AES128-SHA256";

static SSLSOCK *firstsocket;
static FILE *logfile;
static int ssllog = FALSE;
static int sslinit = TRUE;
static SSLSOCK *getsslsock(SOCKET);
static DH *get_dh1024dsa(void);
#endif
static int sslInit(int tcpflags);
static int sslpending(SOCKET);
static int sslrecv(SOCKET, char *, int);
static int sslsend(SOCKET, char *, int);
static int tcpsslcinit(SOCKET socket  /*, BIO *input char *certificate*/);

#if OS_WIN32 == 1
int inet_aton(const char *cp, struct in_addr *inp) {
	inp->s_addr = inet_addr(cp);
	if (inp->s_addr == INADDR_NONE) {
		/* this check can fail if we are attaching to the broadcast address */
		if (strcmp(cp, "255.255.255.255") == 0) {
			return 1;
		} else {
			return 0;
		}
	} else {
		return 1;
	}
}
#endif

/**
 * tcpconnect creates a client socket that is connected to a server socket
 * the socket parameter can be in either <ipaddr> or <ipaddr>:<port> form
 *   <ipaddr> can be a DNS name or an nn.nn.nn.nn ip address
 * port only needs to be specified if the :<port> is not specified in the server
 * tcpflags is the logical or of various TCP_ flags
 * authfile is a file containing the SSL authority certificate
 * timeout is the number of seconds of timeout
 */
SOCKET tcpconnect(char *server, int port, int tcpflags, BIO *authfile /*char *authfile*/, int timeout)
{
	int i1, err, sockopt;
	char *ptr, workserver[256];
	BIO *bptr;
	SOCKET sockethandle, isock1;
	struct linger lingstr;
	struct sockaddr_in servaddr;
	struct hostent *host;
	fd_set fds, exceptfds;
	TIMEVAL tv;
#if OS_WIN32
	WSADATA versioninfo;
	unsigned long noblock;
#else
	int noblock;
#endif
#if (defined (__USLC__) || defined(_SEQUENT_)) && !defined(_M_SYS5)
	size_t sockoptsize;
#else
	int sockoptsize;
#endif

	if (!initflag) {
#if OS_WIN32
		if (WSAStartup(MAKEWORD(2, 2), &versioninfo)) {
			sprintf(tcperrorstring, "Unable to start WSOCK, error = %d", ERRORVALUE());
			return -1;
		}
#endif
		initflag = TRUE;
	}

	if (timeout == 0) return -1;

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;

	if (strlen(server) > 250) {
		strcpy(tcperrorstring, "server name is too long");
		return INVALID_SOCKET;
	}
	strcpy(workserver, server);
	ptr = strchr(workserver, ':');
	if (ptr != NULL) {
		*ptr++ = '\0';
		tcpntoi((unsigned char *) ptr, (int)strlen(ptr), &port);
	}
	else if (port < 0) {
		strcpy(tcperrorstring, "port number is invalid");
		return INVALID_SOCKET;
	}

/* figure out if workserver is a domain name or an ip address */
	for (i1 = 0; isdigit(workserver[i1]) || workserver[i1] == '.'; i1++);
	if (workserver[i1]) {  /* it is a domain name string */
		host = gethostbyname(workserver);
		if (host == NULL) {
			sprintf(tcperrorstring, "gethostbyname failed (invalid server name?), error = %d", ERRORVALUE());
			return INVALID_SOCKET;
		}
		memcpy((char *) &servaddr.sin_addr, host->h_addr, host->h_length);
	}
	else {  /* it is an ip address */
		/*if ((servaddr.sin_addr.s_addr = inet_addr(workserver)) == INADDR_NONE) { */
		if (!inet_aton(workserver, &servaddr.sin_addr)) {
			sprintf(tcperrorstring, "inet_addr failed (invalid ip address?), error = %d", ERRORVALUE());
			return INVALID_SOCKET;
		}
	}
	servaddr.sin_port = htons((unsigned short) port);

/* establish socket connection to the server to get FS version number */
	sockethandle = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockethandle == INVALID_SOCKET) {
		sprintf(tcperrorstring, "socket() failed, error = %d", ERRORVALUE());
		return INVALID_SOCKET;
	}
#if OS_UNIX
/* set close on exec */
	i1 = fcntl(sockethandle, F_GETFD, 0);
	if (i1 != -1) fcntl(sockethandle, F_SETFD, i1 | FD_CLOEXEC);
#endif

	if (timeout >= 0) {
#if OS_WIN32
		noblock = 1;
		ioctlsocket(sockethandle, FIONBIO, &noblock);
#else
#if defined(FIONBIO)
		noblock = 1;
		ioctl(sockethandle, FIONBIO_CAST FIONBIO, &noblock);
#else
		noblock = fcntl(sockethandle, F_GETFL, 0);
		if (noblock != -1) fcntl(sockethandle, F_SETFL, (noblock | O_NONBLOCK));
#endif
#endif
		if (connect(sockethandle, (struct sockaddr *) &servaddr, sizeof(servaddr)) == SOCKET_ERROR) {
			i1 = ERRORVALUE();

#if OS_WIN32
			if (i1 != WSAEWOULDBLOCK) {
#else
#ifdef EINPROGRESS
			if (i1 != EINPROGRESS) {
#endif
#ifdef EWOULDBLOCK
			if (i1 != EWOULDBLOCK) {
#endif
			if (i1 != EAGAIN) {
#endif
#if OS_WIN32
				if (i1 == WSAECONNREFUSED) strcpy(tcperrorstring, "connection refused");
				else
#endif
#if OS_UNIX
				if (i1 == ECONNREFUSED) strcpy(tcperrorstring, "connection refused");
				else
#endif
					sprintf(tcperrorstring, "connect failed, error = %d", i1);
#if OS_WIN32
				noblock = 0;
				ioctlsocket(sockethandle, FIONBIO, &noblock);
#else
#if defined(FIONBIO)
				noblock = 0;
				ioctl(sockethandle, FIONBIO_CAST FIONBIO, &noblock);
#else
				if (noblock != -1) fcntl(sockethandle, F_SETFL, noblock);
#endif
#endif
				closesocket(sockethandle);
				return INVALID_SOCKET;
#if !OS_WIN32
			}
#ifdef EWOULDBLOCK
			}
#endif
#ifdef EINPROGRESS
			}
#endif
#else
			}
#endif
			for (; timeout; ) {
				tv.tv_sec = timeout;
				tv.tv_usec = 0;
				FD_ZERO(&fds);
				FD_SET(sockethandle, &fds);
				FD_ZERO(&exceptfds);
				FD_SET(sockethandle, &exceptfds);
#if OS_WIN32
				isock1 = select(0, NULL, &fds, &exceptfds, &tv);
#else
				isock1 = select(sockethandle + 1, NULL, &fds, &exceptfds, &tv);
#endif
				if (isock1 == SOCKET_ERROR) {
					err = ERRORVALUE();
#if OS_WIN32
					if (err == WSAEINTR && !(tcpflags & TCP_EINTR)) continue;
#else
					if (err == EINTR && !(tcpflags & TCP_EINTR)) continue;
#endif
				}
				break;
			}
			if (isock1 == SOCKET_ERROR || isock1 == 0 || !FD_ISSET(sockethandle, &fds)) {
				if (isock1 == SOCKET_ERROR) sprintf(tcperrorstring, "select() failed, error = %d", err);
				else if (isock1 == 0) strcpy(tcperrorstring, "connection timed out");
				else {  /* assume exception */
					sockoptsize = sizeof(sockopt);
					if (getsockopt(sockethandle, SOL_SOCKET, SO_ERROR, (char *) &sockopt,
#if OS_UNIX
							(socklen_t *)
#endif
							&sockoptsize) == SOCKET_ERROR)
						/* On AIX6 the above is defined as a pointer to a 32 bit unsigned long */
						sprintf(tcperrorstring, "getsockopt() failed, error = %d", ERRORVALUE());
#if OS_WIN32
/*** NOTE: appears that getsockopt may return sockopt = 0, until select time has expired */
					else if (sockopt == WSAECONNREFUSED || !sockopt) strcpy(tcperrorstring, "connection refused");
#endif
#if OS_UNIX
					else if (sockopt == ECONNREFUSED) strcpy(tcperrorstring, "connection refused");
#endif
					else sprintf(tcperrorstring, "connect() failed, error = %d", sockopt);
				}
#if OS_WIN32
				noblock = 0;
				ioctlsocket(sockethandle, FIONBIO, &noblock);
#else
#if defined(FIONBIO)
				noblock = 0;
				ioctl(sockethandle, FIONBIO_CAST FIONBIO, &noblock);
#else
				if (noblock != -1) fcntl(sockethandle, F_SETFL, noblock);
#endif
#endif
				closesocket(sockethandle);
				return INVALID_SOCKET;
			}
		}
#if OS_WIN32
		noblock = 0;
		ioctlsocket(sockethandle, FIONBIO, &noblock);
#else
#if defined(FIONBIO)
		noblock = 0;
		ioctl(sockethandle, FIONBIO_CAST FIONBIO, &noblock);
#else
		if (noblock != -1) fcntl(sockethandle, F_SETFL, noblock);
#endif
#endif
	}
	else {
		for ( ; ; ) {
			i1 = connect(sockethandle, (struct sockaddr *) &servaddr, sizeof(servaddr));
			if (i1 == SOCKET_ERROR) {
				i1 = ERRORVALUE();
#if OS_WIN32
				if (i1 == WSAECONNREFUSED) strcpy(tcperrorstring, "connection refused");
				else if (i1 == WSAEINTR) {
#else
				if (i1 == ECONNREFUSED) strcpy(tcperrorstring, "connection refused");
				else if (i1 == EINTR) {
#endif
					if (!(tcpflags & TCP_EINTR)) continue;
					strcpy(tcperrorstring, "connect interrupted");
				}
				else sprintf(tcperrorstring, "connect() failed, error = %d", i1);
				closesocket(sockethandle);
				return INVALID_SOCKET;
			}
			break;
		}
	}

	/**
	 * Defeat the Nagle algorithm
	 */
	sockopt = 1;
	setsockopt(sockethandle, IPPROTO_TCP, TCP_NODELAY, (char *) &sockopt, sizeof(sockopt));

	lingstr.l_onoff = 1;
/*** NOTE: THIS IS SECONDS, EXCEPT LINUX MAY BE/WAS HUNDREDTHS ***/
	lingstr.l_linger = 3;
	setsockopt(sockethandle, SOL_SOCKET, SO_LINGER, (char *) &lingstr, sizeof(lingstr));
	if (tcpflags & TCP_SSL) {  /* init ssl */
		if (authfile == NULL) bptr = NULL;
		else bptr = authfile;
		i1 = sslInit(tcpflags);
		if (i1 == 0) {
			if (tcpflags & TCP_SERV) i1 = tcpsslsinit(sockethandle, bptr);
			else i1 = tcpsslcinit(sockethandle/* , NULL We never cert the client */);
		}
		if (i1 < 0) {
			tcpsslcomplete(sockethandle);
			closesocket(sockethandle);
			return INVALID_SOCKET;
		}
	}
	return sockethandle;
} // tcpconnect

/* tcplisten opens a server socket on the port specified by port */
/* return is immediate */
SOCKET tcplisten(int port, int *listenport)
{
	int i1;
	SOCKET sockethandle;
	struct sockaddr_in address;
#if (__USLC__ || defined(_SEQUENT_)) && !defined(_M_SYS5)
	size_t addrsize;
#else
	int addrsize;
#endif
#if OS_WIN32
	WSADATA versioninfo;
#endif

	if (!initflag) {
#if OS_WIN32
		if (WSAStartup(MAKEWORD(2, 2), &versioninfo)) {
			sprintf(tcperrorstring, "Unable to start WSOCK, error = %d", ERRORVALUE());
			return -1;
		}
#endif
		initflag = TRUE;
	}
	sockethandle = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockethandle == INVALID_SOCKET) {
		sprintf(tcperrorstring, "socket failed, error = %d", ERRORVALUE());
		return INVALID_SOCKET;
	}
#if OS_UNIX
/* set close on exec */
	i1 = fcntl(sockethandle, F_GETFD, 0);
	if (i1 != -1) fcntl(sockethandle, F_SETFD, i1 | FD_CLOEXEC);
#endif

	i1 = 1;
	setsockopt(sockethandle, SOL_SOCKET, SO_REUSEADDR, (char *) &i1, sizeof(i1));
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	if (port) address.sin_port = htons((unsigned short) port);
	if (bind(sockethandle, (struct sockaddr *) &address, sizeof(struct sockaddr_in)) == -1) {
		sprintf(tcperrorstring, "bind failed, error = %d", ERRORVALUE());
		closesocket(sockethandle);
		return INVALID_SOCKET;
	}
	if (listen(sockethandle, 1) == -1) {
		sprintf(tcperrorstring, "listen failed, error = %d", ERRORVALUE());
		closesocket(sockethandle);
		return INVALID_SOCKET;
	}
	if (!port) {
		addrsize = sizeof(address);
		if (getsockname(sockethandle, (struct sockaddr *) &address,
#if OS_UNIX
				(socklen_t *)
#endif
				&addrsize)) {
			sprintf(tcperrorstring, "getsockname failed, error = %d", ERRORVALUE());
			closesocket(sockethandle);
			return INVALID_SOCKET;
		}
		port = ntohs(address.sin_port);
	}
	if (listenport != NULL) *listenport = port;
	return sockethandle;
}

/**
 * tcpaccept waits on a server socket in listen mode for a client
 * and returns when the client has completed the connection
 * <param>serversocket</param> is always closed
 */
SOCKET tcpaccept(SOCKET serversocket, int tcpflags, BIO *authfile /*char *authfile*/, int timeout)
{
	int i1, sockopt;
	BIO *bptr;
	SOCKET newsockethandle;
	struct linger lingstr;
	TIMEVAL tv, *tvptr;
	fd_set fds;

	for ( ; ; ) {
		if (timeout >= 0) {
			tv.tv_sec = timeout;
			tv.tv_usec = 0;
			tvptr = &tv;
		}
		else tvptr = NULL;
		FD_ZERO(&fds);
		FD_SET(serversocket, &fds);
#if OS_WIN32
		i1 = select(0, &fds, NULL, NULL, tvptr);
#else
		i1 = select(serversocket + 1, &fds, NULL, NULL, tvptr);
#endif
		if (i1 == SOCKET_ERROR) {
			i1 = ERRORVALUE();
#if OS_WIN32
			if (i1 == WSAEINTR) {
#else
			if (i1 == EINTR) {
#endif
				if (!(tcpflags & TCP_EINTR)) continue;
				strcpy(tcperrorstring, "accept interrupted");
			}
			else sprintf(tcperrorstring, "select failed, error = %d", i1);
			closesocket(serversocket);
			return INVALID_SOCKET;
		}
		if (i1 == 0) {
			strcpy(tcperrorstring, "accept timed out");
			closesocket(serversocket);
			return INVALID_SOCKET;
		}
		break;
	}
	newsockethandle = accept(serversocket, NULL, NULL);
	if (newsockethandle == INVALID_SOCKET) {
		sprintf(tcperrorstring, "accept failed, error = %d", ERRORVALUE());
		closesocket(serversocket);
		return INVALID_SOCKET;
	}
#if OS_UNIX
/* set close on exec */
	i1 = fcntl(newsockethandle, F_GETFD, 0);
	if (i1 != -1) fcntl(newsockethandle, F_SETFD, i1 | FD_CLOEXEC);
#endif
	closesocket(serversocket);

	/**
	 * Defeat the Nagle algorithm
	 */
	sockopt = 1;
	setsockopt(newsockethandle, IPPROTO_TCP, TCP_NODELAY, (char *) &sockopt, sizeof(sockopt));
	lingstr.l_onoff = 1;
/*** NOTE: THIS IS SECONDS, EXCEPT LINUX MAY BE/WAS HUNDREDTHS ***/
	lingstr.l_linger = 3;
	setsockopt(newsockethandle, SOL_SOCKET, SO_LINGER, (char *) &lingstr, sizeof(lingstr));
	if (tcpflags & TCP_SSL) {  /* init ssl */
		if (authfile == NULL) bptr = NULL;
		else bptr = authfile;
		i1 = sslInit(tcpflags);
		if (i1 == 0) {
			if (tcpflags & TCP_SERV) i1 = tcpsslsinit(newsockethandle, bptr);
			else i1 = tcpsslcinit(newsockethandle/*, NULL  We never cert the client */);
		}
		if (i1 < 0) {
			tcpsslcomplete(newsockethandle);
			closesocket(newsockethandle);
			return INVALID_SOCKET;
		}
	}
	return newsockethandle;
} // tcpaccept

/**
 * tcprecv gets however many bytes are available and moves them into buffer
 * Returns zero if invalid socket or argument 'length' is <= 0 or timeout
 * Returns -1 if error, message in tcperrorstring
 * If success, returns the non-zero length of the message placed in 'buffer'
 */
int tcprecv(SOCKET sockethandle, unsigned char *buffer, int length, int tcpflags, int timeout)
{
	int i1, bufcnt, bytecnt, cnt, needmoreflag;
	unsigned char c1;
	BYTE bytebuffer[4096], *byteptr;
	fd_set rdset, errset;
	TIMEVAL tv, *tvptr;

/*** CODE: LOOK INTO USING SO_SNDTIMEO AND SO_RCVTIMEO INSTEAD OF SELECT ***/
/***       AS THE PERFOMANCE IS SUPPOSE TO BE BETTER ***/

	tcperrorstring[0] = '\0';
	if (length <= 0 || sockethandle == INVALID_SOCKET) return 0;

	if (tcpflags & TCP_UTF8) {
		bytecnt = sizeof(bytebuffer);
		if (bytecnt > length) bytecnt = length;
		byteptr = bytebuffer;
	}
	else {
		bytecnt = length * sizeof(unsigned char);
		byteptr = (BYTE *) buffer;
	}
	for (bufcnt = 0, needmoreflag = FALSE; ; ) {
		if (tcpflags & TCP_SSL) {
			cnt = sslpending(sockethandle);
			if (cnt == SOCKET_ERROR) {
				sprintf(tcperrorstring, "sslpending failed");
				return -1;
			}
			if (cnt == (int) INVALID_SOCKET) {
				sprintf(tcperrorstring, "sslpending failed, ssl initialization function was not called or failed");
				return -1;
			}
		}
		else cnt = 0;
		if (!cnt) {
			for (i1 = 0; timeout; ) {
				if (timeout >= 0) {
					tv.tv_sec = timeout;
					tv.tv_usec = 0;
					tvptr = &tv;
				}
				else tvptr = NULL;
				FD_ZERO(&rdset);
				FD_SET(sockethandle, &rdset);
				FD_ZERO(&errset);
				FD_SET(sockethandle, &errset);
#if OS_WIN32
				i1 = select(0, &rdset, NULL, &errset, tvptr);
#else
				i1 = select(sockethandle + 1, &rdset, NULL, &errset, tvptr);
#endif
				if (i1 == SOCKET_ERROR) {
					i1 = ERRORVALUE();
#if OS_WIN32
					if (i1 == WSAEINTR) {
#else
					if (i1 == EINTR) {
#endif
						if (!(tcpflags & TCP_EINTR)) continue;
						strcpy(tcperrorstring, "receive interrupted");
					}
					else sprintf(tcperrorstring, "select failed, error = %d", i1);
					return -1;
				}
				break;
			}
			if (i1 == 0) {  /* timeout */
				strcpy(tcperrorstring, "receive timed out");
				return 0;
			}
			if (FD_ISSET(sockethandle, &errset)) {
				strcpy(tcperrorstring, "select signaled exception on socket");
				return -1;
			}
		}
		if (tcpflags & TCP_SSL) {
			cnt = sslrecv(sockethandle, (char *) byteptr, bytecnt);
			if (cnt == SOCKET_ERROR) {
				sprintf(tcperrorstring, "sslrecv failed");
				return -1;
			}
			if (cnt == (int) INVALID_SOCKET) {
				sprintf(tcperrorstring, "sslrecv failed, ssl initialization function was not called or failed");
				return -1;
			}
		}
		else {
			cnt = recv(sockethandle, (char *) byteptr, bytecnt, 0);
			if (cnt == SOCKET_ERROR) {
				i1 = ERRORVALUE();
#if OS_WIN32
				if (i1 == WSAEINTR) {
#else
				if (i1 == EINTR) {
#endif
					if (!(tcpflags & TCP_EINTR)) continue;
					strcpy(tcperrorstring, "receive interrupted");
				}
				else sprintf(tcperrorstring, "recv failed, error = %d", i1);
				return -1;
			}
		}
		if (cnt == 0) {  /* graceful disconnect */
			strcpy(tcperrorstring, "socket disconnected");
			return -1;
		}

/*** NOTE: no checks for integrety are being done (optimization choice) */
		if (needmoreflag) {  /* in the middle of multibyte character */
			if (tcpflags & TCP_UTF8) {
				if (bytecnt == 1) c1 += (unsigned char) bytebuffer[0] - 0x80;
				else {
					c1 += ((unsigned char) bytebuffer[0] - 0x80) << 6;
					if (cnt == 1) {
						bytecnt = 1;
						continue;
					}
					c1 += (unsigned char) bytebuffer[1] - 0x80;
				}
				buffer[bufcnt++] = c1;
			}
			else {
				bytecnt -= cnt;
				if (bytecnt) continue;
				bufcnt++;
			}
			break;
		}
		if (tcpflags & TCP_UTF8) {
			for (i1 = 0; i1 < cnt; ) {
				c1 = (unsigned char) bytebuffer[i1++];
				if (c1 >= 0x80) {
					if (c1 < 0xE0) {
						c1 = (c1 - 0xC0) << 6;
						if (i1 == cnt) {
							bytecnt = 1;
							needmoreflag = TRUE;
							break;
						}
						c1 += (unsigned char) bytebuffer[i1++] - 0x80;
					}
					else {
						c1 = (unsigned char)(((unsigned short) c1 - 0xE0) << 12);
						if (i1 == cnt) {
							bytecnt = 2;
							needmoreflag = TRUE;
							break;
						}
						c1 += ((unsigned char) bytebuffer[i1++] - 0x80) << 6;
						if (i1 == cnt) {
							bytecnt = 1;
							needmoreflag = TRUE;
							break;
						}
						c1 += (unsigned char) bytebuffer[i1++] - 0x80;
					}
				}
				buffer[bufcnt++] = c1;
			}
		}
		else {
			bufcnt = cnt / sizeof(unsigned char);
			if (cnt % sizeof(unsigned char)) {
				bytecnt = sizeof(unsigned char) - cnt % sizeof(unsigned char);
				byteptr += cnt;
				needmoreflag = TRUE;
			}
		}
		if (!needmoreflag) break;
	}
	return bufcnt;
}

/**
 * tcpsend sends the bytes from buffer
 * return is immediate unless the outgoing system buffer is full
 * in which case it waits for the buffer
 *
 * Returns the number of bytes sent if ok. Zero if failure
 */
int tcpsend(SOCKET sockethandle, unsigned char *buffer, int length, int tcpflags, int timeout)
{
	int i1, bufcnt, bytecnt, cnt;
	unsigned short c1;
	BYTE bytebuffer[4096], *byteptr;
	fd_set wrtset, errset;
	TIMEVAL tv, *tvptr;
#if OS_WIN32
	unsigned long noblock;
#else
	int noblock;
#endif

/*** CODE: LOOK INTO USING SO_SNDTIMEO AND SO_RCVTIMEO INSTEAD OF SELECT ***/
/***       AS THE PERFOMANCE IS SUPPOSE TO BE BETTER ***/

	tcperrorstring[0] = '\0';
	if (length == -1) length = (INT)strlen((char *) buffer);
	if (length <= 0 || sockethandle == INVALID_SOCKET) return 0;

	if (tcpflags & TCP_UTF8) {
		bytecnt = bufcnt = 0;
		byteptr = bytebuffer;
	}
	else {
		bufcnt = length;
		bytecnt = length * sizeof(unsigned char);
		byteptr = (BYTE *) buffer;
	}
	if (timeout >= 0) {  /* set to non-blocking mode */
#if OS_WIN32
		noblock = 1;
		ioctlsocket(sockethandle, FIONBIO, &noblock);
#else
#if defined(FIONBIO)
		noblock = 1;
		ioctl(sockethandle, FIONBIO_CAST FIONBIO, &noblock);
#else
		noblock = fcntl(sockethandle, F_GETFL, 0);
		if (noblock != -1) fcntl(sockethandle, F_SETFL, (noblock | O_NONBLOCK));
#endif
#endif
	}

	while (bufcnt <= length) {
		if (bytecnt > (int) sizeof(bytebuffer) - 3 || bufcnt == length) {
/* flush byteptr */
			for (cnt = 0; cnt < bytecnt; ) {
				if (tcpflags & TCP_SSL) {
					i1 = sslsend(sockethandle, (char *)(byteptr + cnt), bytecnt - cnt);
					if (i1 == SOCKET_ERROR) {  /* tcperrorstring is filled in in this case by sslsend */
						bufcnt = length = -1;
						break;
					}
					if (i1 == (int) INVALID_SOCKET) {
						sprintf(tcperrorstring, "sslsend failed, ssl initialization function was not called or failed");
						bufcnt = length = -1;
						break;
					}
				}
				else {
					i1 = send(sockethandle, (char *)(byteptr + cnt), bytecnt - cnt, 0);
					if (i1 == SOCKET_ERROR) {
						i1 = ERRORVALUE();
#if OS_WIN32
						if ((i1 != WSAEINTR || (tcpflags & TCP_EINTR)) && i1 != WSAEWOULDBLOCK) {
							if (i1 == WSAEINTR)
#else
						if (i1 != EINTR || (tcpflags & TCP_EINTR)) {
#ifdef EINPROGRESS
							if (i1 != EINPROGRESS) {
#endif
#ifdef EWOULDBLOCK
							if (i1 != EWOULDBLOCK) {
#endif
							if (i1 != EAGAIN) {
								if (i1 == EINTR)
#endif
									strcpy(tcperrorstring, "send interrupted");
								else sprintf(tcperrorstring, "send failed, error = %d", i1);
								bufcnt = length = -1;
								break;
#if !OS_WIN32
							}
#ifdef EWOULDBLOCK
							}
#endif
#ifdef EINPROGRESS
							}
#endif
#endif
						}
						i1 = 0;
					}
				}
				cnt += i1;
				if (cnt >= bytecnt) break;
				for (i1 = 0; timeout; ) {
					FD_ZERO(&wrtset);
					FD_SET(sockethandle, &wrtset);
					FD_ZERO(&errset);
					FD_SET(sockethandle, &errset);
					if (timeout >= 0) {
						tv.tv_sec = timeout;
						tv.tv_usec = 0;
						tvptr = &tv;
					}
					else tvptr = NULL;
#if OS_WIN32
					i1 = select(0, NULL, &wrtset, &errset, tvptr);
#else
					i1 = select(sockethandle + 1, NULL, &wrtset, &errset, tvptr);
#endif
					if (i1 == SOCKET_ERROR) {
						i1 = ERRORVALUE();
#if OS_WIN32
						if (i1 == WSAEINTR) {
#else
						if (i1 == EINTR) {
#endif
							if (!(tcpflags & TCP_EINTR)) continue;
							strcpy(tcperrorstring, "send interrupted");
						}
						else sprintf(tcperrorstring, "select failed, error = %d", i1);
						bufcnt = length = -1;
					}
					break;
				}
				if (i1 == 0) {  /* timeout */
					strcpy(tcperrorstring, "send timed out");
					bufcnt = length = 0;
					break;
				}
				if (FD_ISSET(sockethandle, &errset)) {
					strcpy(tcperrorstring, "select signaled exception on socket");
					bufcnt = length = -1;
					break;
				}
/* assume at least room to write one byte */
			}
			if (bufcnt == length) break;
			bytecnt = 0;
		}
/* do TCP_UTF8 conversion */
		c1 = buffer[bufcnt++];
		if (c1 == 0) {
			bytebuffer[bytecnt++] = 0xC0;
			bytebuffer[bytecnt++] = 0x80;
		}
		else if (c1 <= 0x7F) bytebuffer[bytecnt++] = (BYTE) c1;
		else if (c1 <= 0x07FF) {
			bytebuffer[bytecnt++] = (BYTE)(0xC0 + (c1 >> 6));
			bytebuffer[bytecnt++] = (BYTE)(0x80 + (c1 & 0x3F));
		}
		else {
			bytebuffer[bytecnt++] = (BYTE)(0xE0 + (c1 >> 12));
			bytebuffer[bytecnt++] = (BYTE)(0x80 + ((c1 >> 6) & 0x3F));
			bytebuffer[bytecnt++] = (BYTE)(0x80 + (c1 & 0x3F));
		}
	}
	if (timeout >= 0) {
#if OS_WIN32
		noblock = 0;
		ioctlsocket(sockethandle, FIONBIO, &noblock);
#else
#if defined(FIONBIO)
		noblock = 0;
		ioctl(sockethandle, FIONBIO_CAST FIONBIO, &noblock);
#else
		if (noblock != -1) fcntl(sockethandle, F_SETFL, noblock);
#endif
#endif
	}
	return bufcnt;
}

int tcpitoa(intptr_t src, char *dest)
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

void tcpiton(int src, unsigned char *dest, int n)
{
	int negflg;

	if (n <= 0) return;
/*** CODE: PROBABLY DO NOT HAVE TO SUPPORT NEGATIVES ***/
	if (src < 0) {
		src = -src;
		negflg = TRUE;
	}
	else negflg = FALSE;

	do dest[--n] = (char)(src % 10 + '0');
	while ((src /= 10) && n);
	if (negflg && n) dest[--n] = '-';
	while (n) dest[--n] = ' ';
}

int tcpntoi(unsigned char *src, int n, int *dest)
{
	int i1, negflg, num, rc;

	rc = 0;
/*** CODE: PROBABLY DO NOT HAVE TO SUPPORT NEGATIVES ***/
	negflg = FALSE;
	for (i1 = -1, num = 0; ++i1 < n; ) {
		if (isdigit(src[i1])) num = num * 10 + src[i1] - '0';
		else if (src[i1] != ' ') {
			if (src[i1] == '-') negflg = TRUE;
			else rc = -1;
		}
	}
	if (negflg) *dest = -num;
	else *dest = num;
	return rc;
}

int tcpquotedlen(unsigned char *string, int length)
{
	int cnt, quotecnt;

	if (length == -1) length = (INT)strlen((char *) string);
	if (length == 0) return 2;

	for (quotecnt = cnt = 0; cnt < length; cnt++) {
		if (string[cnt] == '\\' || string[cnt] == '"') cnt++;
		else if (string[cnt] == ' ') quotecnt = 2;
	}
	return cnt + quotecnt;
}

int tcpquotedcopy(unsigned char *dest, unsigned char *src, int srclen)
{
	int cnt, quoteflag, destlen;

	if (srclen == -1) srclen = (INT)strlen((char *) src);
	if (srclen == 0) {
		dest[0] = '"';
		dest[1] = '"';
		return 2;
	}

	for (quoteflag = FALSE, destlen = cnt = 0; cnt < srclen; cnt++) {
		if (src[cnt] == '\\' || src[cnt] == '"') dest[destlen++] = '\\';
		else if (src[cnt] == ' ' && !quoteflag) {
			quoteflag = TRUE;
			dest[destlen++] = '"';
		}
		dest[destlen++] = src[cnt];
	}
	if (quoteflag) dest[destlen++] = '"';
	return destlen;
}

int tcpnextdata(unsigned char *data, int datalen, int *offset, int *nextoffset)
{
	int i1, cnt, quoteflag;

	if (datalen == -1) datalen = (INT)strlen((char *) data);
	if (datalen <= 0) return 0;

	while (*offset < datalen && data[*offset] == ' ') (*offset)++;
	if (*offset == datalen) {
		if (nextoffset != NULL) *nextoffset = *offset;
		return -1;
	}
	for (quoteflag = FALSE, i1 = cnt = *offset; cnt < datalen; cnt++) {
		if (data[cnt] == '\\') {
			if (++cnt == datalen) break;
		}
		else if (data[cnt] == ' ' && !quoteflag) {
			cnt++;
			break;
		}
		else if (data[cnt] == '"') {
			quoteflag = !quoteflag;
			continue;
		}
		if (i1 != cnt) data[i1] = data[cnt];
		i1++;
	}
	if (nextoffset != NULL) *nextoffset = cnt;
	return i1 - *offset;
}

void tcpcleanup() {
#if OS_WIN32
	WSACleanup();
#endif
}

char *tcpgeterror()
{
	return tcperrorstring;
}

void tcpseterror(char* str) {
	strcpy(tcperrorstring, str);
}

int tcpissslsupported()
{
#if DBC_SSL
	return 1;
#else
	return 0;
#endif
}

void tcpstartssllog(FILE *log)
{
#if DBC_SSL
	ssllog = TRUE;
	logfile = log;
#endif
}

/**
 * Called from tcpsslsinit and tcpsslcinit
 */
static int sslInit(int tcpflags) {
	if (sslinit) {
#if OPENSSL_VERSION_NUMBER < 0x1010000fL
//  OPENSSL_VERSION_NUMBER   0x1010008fL

		SSL_load_error_strings();
		SSL_library_init();
#endif
#if OS_UNIX
		if (RAND_status() == 0) {
			void *ptr;
			/* prevent PRNG not seeded error on machines without /dev/random */
			ptr = malloc(256);
			if (ptr == NULL) {
				strcpy(tcperrorstring, "tcpsslsinit, malloc failed");
				if (ssllog) fprintf(logfile, "%s", tcperrorstring);
				return -1;
			}
			RAND_seed((const void *) ptr, 256);
			/*
			 * Don't free ptr here. Not sure if it is OK to do so.
			 * Not even sure if this ever happens on a modern *nix
			 */
		}
#endif
		sslinit = FALSE;
	}
	return 0;
}

/*
 * SSL Client initialization
 *
 * This function performs the handshake with the server and
 * exchanges cipher suites.  Server verification takes place
 * here.  Call this function after client socket connect() call.
 * This must be called before sslsend() and sslrecv() can be used.
 *
 * Parameters:
 *   SOCKET sd		- The established client socket descriptor
 *	 char *CAfile	- The Certificate Authority's certification file,
 *					  used to validate the server.
 *                    This parameter can be NULL if you don't care about
 * 					  server verification.
 *
**/
static int tcpsslcinit(SOCKET sd  /*, BIO *CAfile char *CAfile*/)
{
#if DBC_SSL
	int err;
	//char *str;
	DH *dh;
	SSL_CIPHER *ciph;
	SSLSOCK *sslsock, *tmpsock;
#if OS_WIN32
	WSADATA versioninfo;
#endif

	if (!initflag) {
#if OS_WIN32
		if (WSAStartup(MAKEWORD(2, 2), &versioninfo)) {
			sprintf(tcperrorstring, "Unable to start WSOCK, error = %d", ERRORVALUE());
			return -1;
		}
#endif
		initflag = TRUE;
	}
	sslsock = (SSLSOCK *) malloc(sizeof(SSLSOCK));
	if (sslsock == NULL) {
		strcpy(tcperrorstring, "tcpsslcinit, malloc failed");
		if (ssllog) fprintf(logfile, "%s", tcperrorstring);
		return -1;
	}
	sslsock->next = NULL;
	sslsock->ctx = SSL_CTX_new(TLS_method());
	sslsock->socket = sd;
	if (sslsock->ctx == NULL) {
		free(sslsock);
		strcpy(tcperrorstring, "tcpsslcinit, SSL_CTX_new(");
		strcat(tcperrorstring, "TLS_method()");
		strcat(tcperrorstring, "):call failed");
		if (ssllog) fprintf(logfile, "%s", tcperrorstring);
		return -1;
	}

	SSL_CTX_set_cipher_list(sslsock->ctx, PREFERRED_CIPHERS);
	SSL_CTX_sess_set_cache_size(sslsock->ctx, 128);
	SSL_CTX_set_options(sslsock->ctx, SSL_OP_SINGLE_DH_USE);
	dh = get_dh1024dsa();
	SSL_CTX_set_tmp_dh(sslsock->ctx, dh);
	DH_free(dh);

	if ((sslsock->ssl = SSL_new(sslsock->ctx)) == NULL) {
		free(sslsock);
		strcpy(tcperrorstring, "tcpsslcinit, SSL_new:call failed");
		if (ssllog) fprintf(logfile, "%s", tcperrorstring);
		return -1;
	}

	SSL_set_fd(sslsock->ssl, (int)sd);
	SSL_set_connect_state(sslsock->ssl);
	err = SSL_connect(sslsock->ssl); // This is where Scott DeRousse's problem is happening, I think.
	if (err == -1) {
		free(sslsock);
		//err = SSL_get_error(sslsock->ssl, err);
		strcpy(tcperrorstring, ERR_error_string(ERR_get_error(), NULL));
		if (ssllog) fprintf(logfile, "%s", tcperrorstring);
		return -1;
	}

	if (ssllog) {
		ciph = (SSL_CIPHER*) SSL_get_current_cipher(
#if defined(Linux)
						(SSL*)
#else
						(const SSL*)
#endif
						sslsock->ssl);
		fprintf(logfile, "{ Version: %s, Cipher: %s }\n", SSL_CIPHER_get_version(ciph), SSL_get_cipher(sslsock->ssl));
	}
	if (firstsocket == NULL) firstsocket = sslsock;
	else {
		tmpsock = firstsocket;
		while (tmpsock->next != NULL) tmpsock = tmpsock->next;
		tmpsock->next = sslsock;
	}
#endif
	return 0;
}

/*
 * SSL Server initialization
 *
 * This function performs the handshake with the client,
 * exchanges cipher suites, and sends the server certificate
 * if one is used.  Call this function after accept() call.
 * This must be called before sslsend() and sslrecv() can be used.
 *
 * Parameters:
 *   SOCKET sd			- The established server socket descriptor
 *	 char *certificate	- The server's certificate file.
 *                    	  This parameter can be NULL for anonymous, but
 *                        secure connection
 *
**/
int tcpsslsinit(SOCKET sd, BIO *certinput /*char *certificate*/)
{
#if DBC_SSL
	int err;
  	DH *dh;
	SSLSOCK *sslsock, *tmpsock;
	SSL_CIPHER *ciph;
	EVP_PKEY *pkey=NULL;

	sslsock = (SSLSOCK *) malloc(sizeof(SSLSOCK));
	if (sslsock == NULL) {
		strcpy(tcperrorstring, "tcpsslsinit, malloc failed");
		if (ssllog) fprintf(logfile, "%s", tcperrorstring);
		return -1;
	}
	sslsock->next = NULL;
	sslInit(0);
	sslsock->ctx = SSL_CTX_new(TLS_method());
	sslsock->socket = sd;
	if (!sslsock->ctx) {
		free(sslsock);
		strcpy(tcperrorstring, "tcpsslsinit, SSL_CTX_new(");
		strcat(tcperrorstring, "TLS_method()");
		strcat(tcperrorstring, "):call failed");
		if (ssllog) fprintf(logfile, "%s", tcperrorstring);
		return -1;
	}

	if (certinput != NULL) {
		X509 *x = NULL;
		if (PEM_read_bio_X509(certinput, &x, 0, NULL) == NULL) {
			strcpy(tcperrorstring, "tcpsslsinit, PEM_read_bio_X509:returned NULL");
			if (ssllog) {
				fprintf(logfile, "%s", tcperrorstring);
				fprintf(logfile, "\n%s", ERR_error_string(ERR_get_error(), NULL));
			}
			free(sslsock);
			return -1;
		}

		if (SSL_CTX_use_certificate(sslsock->ctx,x) <= 0) {
			strcpy(tcperrorstring, "tcpsslsinit, SSL_CTX_use_certificate:missing or corrupt server certificate");
			if (ssllog) {
				fprintf(logfile, "%s", tcperrorstring);
				fprintf(logfile, "\n%s", ERR_error_string(ERR_get_error(), NULL));
			}
			X509_free(x);
			free(sslsock);
			return -1;
		}
		X509_free(x);

/* Password callback function places private key password into buffer */
		/*SSL_CTX_set_default_passwd_cb(sslsock->ctx, (pem_password_cb *) pass_callback);*/
		pkey=PEM_read_bio_PrivateKey(certinput,NULL,
#if OPENSSL_VERSION_NUMBER < 0x1010000fL
			// Should not happen. We are now using only 1.1.0 and newer.
			sslsock->ctx->default_passwd_callback,
			sslsock->ctx->default_passwd_callback_userdata
#else
			SSL_CTX_get_default_passwd_cb(sslsock->ctx),
			SSL_CTX_get_default_passwd_cb_userdata(sslsock->ctx)
#endif
				);
		if (SSL_CTX_use_PrivateKey(sslsock->ctx,pkey) <= 0) {
			strcpy(tcperrorstring, "tcpsslsinit, SSL_CTX_use_PrivateKey:missing or corrupt server key");
			if (ssllog) fprintf(logfile, "%s", tcperrorstring);
			EVP_PKEY_free(pkey);
			free(sslsock);
			return -1;
		}
		EVP_PKEY_free(pkey);

		if (!SSL_CTX_check_private_key(sslsock->ctx)) {
			strcpy(tcperrorstring, "SSL_CTX_check_private_key:invalid private server key");
			if (ssllog) fprintf(logfile, "%s", tcperrorstring);
			free(sslsock);
			return -1;
		}

		if (!SSL_CTX_set_default_verify_paths(sslsock->ctx)) {
			strcpy(tcperrorstring, "tcpsslsinit, SSL_CTX_set_default_verify_paths:call failed");
			if (ssllog) fprintf(logfile, "%s", tcperrorstring);
			free(sslsock);
			return -1;
		}
		SSL_CTX_set_cipher_list(sslsock->ctx, PREFERRED_CIPHERS);
	}
	else {
		SSL_CTX_set_cipher_list(sslsock->ctx, PREFERRED_CIPHERS);
	}

	SSL_CTX_sess_set_cache_size(sslsock->ctx, 128);
	SSL_CTX_set_options(sslsock->ctx, SSL_OP_SINGLE_DH_USE);
	dh = get_dh1024dsa();
	SSL_CTX_set_tmp_dh(sslsock->ctx, dh);
	DH_free(dh);
	sslsock->ssl = SSL_new(sslsock->ctx);
	if (sslsock->ssl == NULL) {
		strcpy(tcperrorstring, "tcpsslsinit, SSL_new:call failed");
		if (ssllog) fprintf(logfile, "%s", tcperrorstring);
		free(sslsock);
		return -1;
	}
	err = SSL_set_fd(sslsock->ssl, (int)sd);
	if (err >= 0) {
		SSL_set_accept_state(sslsock->ssl);
		err = SSL_accept(sslsock->ssl);
	}
	if (err < 0) {
		//err = SSL_get_error(sslsock->ssl, err);
		strcpy(tcperrorstring, ERR_error_string(ERR_get_error(), NULL));
		if (ssllog) fprintf(logfile, "%s", tcperrorstring);
		free(sslsock);
		return -1;
	}

	if (ssllog) {
		ciph =
#if defined(Linux) || OS_WIN32 || defined(__MACOSX)
			(SSL_CIPHER*)
#endif
			SSL_get_current_cipher(sslsock->ssl);
		fprintf(logfile, "{ Version: %s, Cipher: %s }\n", SSL_CIPHER_get_version(ciph), SSL_get_cipher(sslsock->ssl));
	}

	if (firstsocket == NULL) firstsocket = sslsock;
	else {
		tmpsock = firstsocket;
		while (tmpsock->next != NULL) tmpsock = tmpsock->next;
		tmpsock->next = sslsock;
	}
#endif
	return 0;
}

/*
 * SSL Session complete
 *
 * This function frees up any memory allocated and does some
 * clean up calls with the OpenSSL library.  It should be
 * called before any sockets are closed.
 *
**/
int tcpsslcomplete(SOCKET sock)
{
#if DBC_SSL
	SSLSOCK *sslsock, *prvsock;
/* search through linked list */
	prvsock = NULL;
	sslsock = firstsocket;
	while (sslsock != NULL) {
		if (sslsock->socket == sock) break;
		prvsock = sslsock;
		sslsock = sslsock->next;
	}
	if (sslsock == NULL) return (int) INVALID_SOCKET;
	SSL_shutdown(sslsock->ssl);
	SSL_free(sslsock->ssl);
	SSL_CTX_free(sslsock->ctx);
	if (prvsock != NULL) prvsock->next = sslsock->next;
	else firstsocket = sslsock->next;
	free(sslsock);
#endif
	return 0;
}

/*
 * SSL Socket peek
 *
**/
static int sslpending(SOCKET sock)
{
#if DBC_SSL
	int i1;
	SSLSOCK *sslsock;
	if ((sslsock = getsslsock(sock)) != NULL) {
		i1 = SSL_pending(sslsock->ssl);
		if (i1 < 0) return SOCKET_ERROR;
		else return i1;
	}
#endif
	return (int) INVALID_SOCKET;
}

/*
 * SSL Socket receive
 *
**/
static int sslrecv(SOCKET sock, char *data, int len)
{
#if DBC_SSL
	int i1;
	SSLSOCK *sslsock;
	if ((sslsock = getsslsock(sock)) != NULL) {
		i1 = SSL_read(sslsock->ssl, data, len);
		if (i1 < 0) return SOCKET_ERROR;
		else return i1;
	}
#endif
	return (int) INVALID_SOCKET;
}

/*
 * SSL Socket send
 *
**/
static int sslsend(SOCKET sock, char *data, int len)
{
#if DBC_SSL
	int i1;
	SSLSOCK *sslsock;
	if ((sslsock = getsslsock(sock)) != NULL) {
		ERR_clear_error();		/* flush ssl error queue */
		i1 = SSL_write(sslsock->ssl, data, len);
		if (i1 < 0) {
			ERR_error_string_n(ERR_get_error(), sslerrorstring, sizeof(sslerrorstring));
			strcpy(tcperrorstring, "sslsend failed, ");
			strcat(tcperrorstring, sslerrorstring);
			if (ssllog) fprintf(logfile, "%s", tcperrorstring);
			return SOCKET_ERROR;
		}
		else return i1;
	}
#endif
	return (int) INVALID_SOCKET;
}

#if DBC_SSL
static SSLSOCK *getsslsock(SOCKET sock)
{
	SSLSOCK *sslsock;
/* search through linked list */
	sslsock = firstsocket;
	while (sslsock != NULL) {
		if (sslsock->socket == sock) break;
		sslsock = sslsock->next;
	}
	return sslsock;
}
#endif

#if DBC_SSL
static DH *get_dh1024dsa()
{
	DH *dh;

	static unsigned char dh1024_p[] = {
		0xC8, 0x00, 0xF7, 0x08, 0x07, 0x89, 0x4D, 0x90, 0x53, 0xF3, 0xD5, 0x00,
		0x21, 0x1B, 0xF7, 0x31, 0xA6, 0xA2, 0xDA, 0x23, 0x9A, 0xC7, 0x87, 0x19,
		0x3B, 0x47, 0xB6, 0x8C, 0x04, 0x6F, 0xFF, 0xC6, 0x9B, 0xB8, 0x65, 0xD2,
		0xC2, 0x5F, 0x31, 0x83, 0x4A, 0xA7, 0x5F, 0x2F, 0x88, 0x38, 0xB6, 0x55,
		0xCF, 0xD9, 0x87, 0x6D, 0x6F, 0x9F, 0xDA, 0xAC, 0xA6, 0x48, 0xAF, 0xFC,
		0x33, 0x84, 0x37, 0x5B, 0x82, 0x4A, 0x31, 0x5D, 0xE7, 0xBD, 0x52, 0x97,
		0xA1, 0x77, 0xBF, 0x10, 0x9E, 0x37, 0xEA, 0x64, 0xFA, 0xCA, 0x28, 0x8D,
		0x9D, 0x3B, 0xD2, 0x6E, 0x09, 0x5C, 0x68, 0xC7, 0x45, 0x90, 0xFD, 0xBB,
		0x70, 0xC9, 0x3A, 0xBB, 0xDF, 0xD4, 0x21, 0x0F, 0xC4, 0x6A, 0x3C, 0xF6,
		0x61, 0xCF, 0x3F, 0xD6, 0x13, 0xF1, 0x5F, 0xBC, 0xCF, 0xBC, 0x26, 0x9E,
		0xBC, 0x0B, 0xBD, 0xAB, 0x5D, 0xC9, 0x54, 0x39
	};

	static unsigned char dh1024_g[] = {
		0x3B, 0x40, 0x86, 0xE7, 0xF3, 0x6C, 0xDE, 0x67, 0x1C, 0xCC, 0x80, 0x05,
		0x5A, 0xDF, 0xFE, 0xBD, 0x20, 0x27, 0x74, 0x6C, 0x24, 0xC9, 0x03, 0xF3,
		0xE1, 0x8D, 0xC3, 0x7D, 0x98, 0x27, 0x40, 0x08, 0xB8, 0x8C, 0x6A, 0xE9,
		0xBB, 0x1A, 0x3A, 0xD6, 0x86, 0x83, 0x5E, 0x72, 0x41, 0xCE, 0x85, 0x3C,
		0xD2, 0xB3, 0xFC, 0x13, 0xCE, 0x37, 0x81, 0x9E, 0x4C, 0x1C, 0x7B, 0x65,
		0xD3, 0xE6, 0xA6, 0x00, 0xF5, 0x5A, 0x95, 0x43, 0x5E, 0x81, 0xCF, 0x60,
		0xA2, 0x23, 0xFC, 0x36, 0xA7, 0x5D, 0x7A, 0x4C, 0x06, 0x91, 0x6E, 0xF6,
		0x57, 0xEE, 0x36, 0xCB, 0x06, 0xEA, 0xF5, 0x3D, 0x95, 0x49, 0xCB, 0xA7,
		0xDD, 0x81, 0xDF, 0x80, 0x09, 0x4A, 0x97, 0x4D, 0xA8, 0x22, 0x72, 0xA1,
		0x7F, 0xC4, 0x70, 0x56, 0x70, 0xE8, 0x20, 0x10, 0x18, 0x8F, 0x2E, 0x60,
		0x07, 0xE7, 0x68, 0x1A, 0x82, 0x5D, 0x32, 0xA2
	};

	if ((dh = DH_new()) == NULL) return(NULL);
#if OPENSSL_VERSION_NUMBER < 0x1010000fL
	dh->p = BN_bin2bn(dh1024_p, sizeof(dh1024_p), NULL);
	dh->g = BN_bin2bn(dh1024_g, sizeof(dh1024_g), NULL);
	if ((dh->p == NULL) || (dh->g == NULL))	{
		DH_free(dh);
		return(NULL);
	}
	dh->length = 160;
#else
	BIGNUM *p = BN_bin2bn(dh1024_p, sizeof(dh1024_p), NULL);
	BIGNUM *g = BN_bin2bn(dh1024_g, sizeof(dh1024_g), NULL);
	if (!DH_set0_pqg(dh, p, NULL, g)) {
		DH_free(dh);
		return(NULL);
	}
	DH_set_length(dh, 160);
#endif
	return(dh);
}
#endif

#if 0 && DBC_SSL
static int pass_callback(char *buf, int len, int verify)
{
	verify = len = 0;
	buf[0] = (char) 0x78 - 1;
	buf[1] = (char) 0x6A - 1;
	buf[2] = (char) 0x6D - 1;
	buf[3] = (char) 0x62 + 1;
	buf[4] = (char) 0x71 - 2;
	buf[5] = (char) 0x39 - 0;
	buf[6] = (char) 0x38 + 1;
	buf[7] = (char) 0x00 + 0;
	return 7;
}
#endif

#if 0
#if DBC_SSL
static int verify_callback(int ok, X509_STORE_CTX *ctx)
{
	char buf[256];
	X509 *err_cert;
	int err, depth;

	err_cert = X509_STORE_CTX_get_current_cert(ctx);
	err = X509_STORE_CTX_get_error(ctx);
	depth = X509_STORE_CTX_get_error_depth(ctx);

	if (ssllog) {
		X509_NAME_oneline(X509_get_subject_name(err_cert), buf, 256);
		fprintf(logfile, "depth = %d %s\n", depth, buf);
	}

	if (!ok) {
		switch (ctx->error)	{
			case X509_V_ERR_CERT_HAS_EXPIRED:						/* 10 */
			case X509_V_ERR_CRL_HAS_EXPIRED:						/* 12 */
				/* allow valid expired certificates */
				ok = 1;
				break;
			case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:				/* 2  */
			case X509_V_ERR_UNABLE_TO_GET_CRL:						/* 3  */
			case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:	 	/* 4  */
			case X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE:		/* 5  */
			case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:	/* 6  */
			case X509_V_ERR_CERT_SIGNATURE_FAILURE:				/* 7  */
			case X509_V_ERR_CRL_SIGNATURE_FAILURE:					/* 8  */
			case X509_V_ERR_CERT_NOT_YET_VALID:					/* 9  */
			case X509_V_ERR_CRL_NOT_YET_VALID:						/* 11 */
			case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:		/* 13 */
			case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:			/* 14 */
			case X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD:		/* 15 */
			case X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD:		/* 16 */
			case X509_V_ERR_OUT_OF_MEM:							/* 17 */
			case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:			/* 18 */
			case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:				/* 19 */
			case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:		/* 20 */
			case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:		/* 21 */
			case X509_V_ERR_CERT_CHAIN_TOO_LONG:					/* 22 */
			case X509_V_ERR_CERT_REVOKED:							/* 23 */
			case X509_V_ERR_INVALID_CA:							/* 24 */
			case X509_V_ERR_PATH_LENGTH_EXCEEDED:					/* 25 */
			case X509_V_ERR_INVALID_PURPOSE:						/* 26 */
			case X509_V_ERR_CERT_UNTRUSTED:						/* 27 */
			case X509_V_ERR_CERT_REJECTED:							/* 28 */
			case X509_V_ERR_SUBJECT_ISSUER_MISMATCH:				/* 29 */
			case X509_V_ERR_AKID_SKID_MISMATCH:					/* 30 */
			case X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH:			/* 31 */
			case X509_V_ERR_KEYUSAGE_NO_CERTSIGN:					/* 32 */
				verify_error = ctx->error;
				if (ssllog) {
					fprintf(logfile, "verify error: num = %d : %s\n", err, X509_verify_cert_error_string(err));
				}
				break;
		}
	}
	if (ssllog) {
		fprintf(logfile, "verify return: %d\n",ok);
	}
	return ok;
}
#endif
#endif
