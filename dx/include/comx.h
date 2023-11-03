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

#if OS_UNIX
typedef INT BOOL;
typedef in_port_t PORT;
#define ARRAYSIZE(A) (sizeof(A)/sizeof((A)[0]))
/* Following shortens all the type casts of pointer arguments */
#define SA  struct sockaddr
#if defined(__MACOSX) || (defined(Linux) && defined(__x86_64__)) \
	|| (defined(__socklen_t_defined) && defined(__PPC__))
#define ADDRSIZE_IS_SOCKLEN_T
#endif

/*
 * Used for IPv6 sockets just in case getaddrinfo does not return a scope id
 */
#ifdef __MACOSX
#define DFLT_SCOPE_ID  "en0"
#else
#define DFLT_SCOPE_ID  "eth0"
#endif

#endif

#if OS_WIN32
#include <process.h>
typedef USHORT PORT;
#endif

/* defines for os_xxxclear() */
#define COM_CLEAR_SEND 0x01
#define COM_CLEAR_RECV 0x02

#define MAXCHNLNAME 32  /* Maximum size of a pipe or device name */
#define MAXSTARTEND 32  /* Maximum size of a in start, outstart, inend, outend string */
#define ERRORMSGSIZE 96

/* defines for UDP and TCP */
#define EVENTS_READ (DEVPOLL_READ | DEVPOLL_READPRIORITY | DEVPOLL_READNORM | DEVPOLL_READBAND)
#define EVENTS_WRITE (DEVPOLL_WRITE)
#define EVENTS_ERROR (DEVPOLL_ERROR | DEVPOLL_HANGUP | DEVPOLL_NOFDES)

/* defines for channel->flags */
#if OS_UNIX
#define COM_FLAGS_OPEN		0x0001
#define COM_FLAGS_TTY		0x0002
#define COM_FLAGS_IOCTL		0x0004
#define COM_FLAGS_NOBLOCK	0x0008
#define COM_FLAGS_DEVINIT	0x0010
#define COM_FLAGS_BOUND		0x0020
#define COM_FLAGS_LISTEN		0x0040
#define COM_FLAGS_CONNECT	0x0080
#define COM_FLAGS_FDLISTEN	0x0100
#else
/* defines for channel->flags */
#define COM_FLAGS_OPEN		0x0001  /* used for all comms */
#define COM_FLAGS_SENDWAIT	0x0002  /* used for serial comm */
#define COM_FLAGS_SENDREG	0x0004  /* used for serial comm */
#define COM_FLAGS_RECVWAIT	0x0008  /* used for serial comm */
#define COM_FLAGS_RECVREG	0x0010  /* used for serial comm */
//#define COM_FLAGS_BOUND		0x0020  /* used for udp and tcp, not actually tested anywhere */
#define COM_FLAGS_SELECT		0x0040  /* used for udp and tcp */
#define COM_FLAGS_LISTEN		0x0080  /* used for tcp server */
#define COM_FLAGS_CONNECT	0x0100  /* used for tcp */
#define COM_FLAGS_FDLISTEN	0x0200  /* tcp server */
#define COM_FLAGS_TABLE		0x0400  /* used for udp and tcp */

#endif


#ifdef _COM_C_
INT clientkeepalive = FALSE;
INT serverkeepalive = FALSE;
BOOL waitForTCPConnect = TRUE;
#ifndef NO_IPV6
INT allowIPv6;	/* Can we use IPv6 on this Windows version? */
#endif
#else
extern INT clientkeepalive;
extern INT serverkeepalive;
extern BOOL waitForTCPConnect;
#ifndef NO_IPV6
extern INT allowIPv6;
#endif
#endif

#define ISCHANNELTYPETCPSERVER(c) \
	((c)->channeltype == CHANNELTYPE_TCPSERVER \
	|| (c)->channeltype == CHANNELTYPE_TCPSERVER4 \
	|| (c)->channeltype == CHANNELTYPE_TCPSERVER6)


#define ISCHANNELTYPETCPCLIENT(c) \
	((c)->channeltype == CHANNELTYPE_TCPCLIENT \
	|| (c)->channeltype == CHANNELTYPE_TCPCLIENT4 \
	|| (c)->channeltype == CHANNELTYPE_TCPCLIENT6)


#define ISCHANNELTYPEUDP(c) \
	((c)->channeltype == CHANNELTYPE_UDP \
	|| (c)->channeltype == CHANNELTYPE_UDP4 \
	|| (c)->channeltype == CHANNELTYPE_UDP6)


typedef struct channelstruct {
	struct channelstruct *nextchannelptr;  /* linked list of channel structures */
	INT refnum;				/* DO NOT USE, RESERVED FOR USE BY COM.C */
	INT channeltype;			/* CHANNELTYPE_xxx value */
	INT flags;				/* flags for clean-up */
	CHAR name[MAXCHNLNAME];		/* channel name */
#if OS_WIN32
	HANDLE handle;				/* handle to device */
#else
	int handle;				/* handle to device */
#endif
	INT status;				/* COM_xxx values */
	CHAR errormsg[ERRORMSGSIZE + 1];  /* error message string */
	INT baud;					/* serial baud rate */
	CHAR parity;				/* serial parity */
	INT databits;				/* serial data bits */
	INT stopbits;				/* serial stop bits */
	UCHAR *sendbuf;			/* address of send data buffer */
	INT sendbufsize;			/* size of send data buffer */
	INT sendlength;			/* length of data in send data buffer */
	INT senddone;				/* length of data sent */
	INT sendtimeout;			/* send timeout */
	INT sendth;				/* Timer handle for send */
	INT sendevtid;				/* send event id */
	UCHAR sendstart[MAXSTARTEND];  /* send start string, 1st byte is length */
	UCHAR sendend[MAXSTARTEND];	/* send end string, 1st byte is length */
	CHAR senderrormsg[ERRORMSGSIZE + 1];  /* send error message string */
	UCHAR *recvbuf;			/* address of recv data buffer */
	INT recvbufsize;			/* size of recv data buffer */
	INT recvlength;			/* max length of data in recv buffer */
	INT recvdone;				/* length of data received */
	INT recvhead;				/* oldest character in receive buffer */
	INT recvtail;				/* newest character in receive buffer */
	INT recvtimeout;			/* recv timeout */
	INT recvth;				/* Timer handle for receive */
	INT recvevtid;				/* recv event id */
	UCHAR recvstart[MAXSTARTEND];  /* recv start string, 1st byte is length */
	UCHAR recvend[MAXSTARTEND];	/* recv end string, 1st byte is length */
	UCHAR recvignore[MAXSTARTEND];  /* recv ignore, 1st byte is length */
	INT recvlimit;				/* recv fix length */
	INT recvstate;				/* current receive state */
	CHAR recverrormsg[ERRORMSGSIZE + 1];  /* receive error message string */

	SOCKET socket;				/* socket file descriptor */
	SOCKET fdlisten;			/* socket file descriptor used for listening */
	PORT ourport;				/* our port number */
#if OS_WIN32
	/*
	 * Note, except for sin*_family, contents are expressed in network byte order.
	 *
	 */
	SOCKADDR_STORAGE recvaddr;	/* last message received IP address/port/family */
	SOCKADDR_STORAGE sendaddr;	/* IP address of destination of message */
	SOCKADDR_STORAGE ouraddr;	/* Used by UDP */
#else
#ifdef NO_IPV6
	struct sockaddr_in recvaddr;	/* last message received IP address/port/family */
	struct sockaddr_in sendaddr;	/* IP address of destination of message */
	struct sockaddr_in ouraddr;		/* Used by UDP */
#else
	struct sockaddr_storage recvaddr;	/* last message received IP address/port/family */
	struct sockaddr_storage sendaddr;	/* IP address of destination of message */
	struct sockaddr_storage ouraddr;	/* Used by UDP */
#endif
#endif

#if OS_WIN32
	INT channeltableentry;		/* entry in socket table + 1 */
	DWORD sendbytes;
	OVERLAPPED sendoverlap;
	DWORD recvbytes;
	OVERLAPPED recvoverlap;
	DCB dcb;
#endif

#if OS_UNIX
	INT devpoll;				/* current devpoll settings */
#if defined(USE_POSIX_TERMINAL_IO)
	struct termios termold;		/* old terminal setting */
	struct termios termnew;		/* new terminal setting */
#else
	struct termio termold;		/* old terminal setting */
	struct termio termnew;		/* new terminal setting */
#endif
#endif
} CHANNEL;

extern void comerror(CHANNEL *, INT, CHAR *, INT);
extern CHANNEL *getchannelhead(void);
extern INT os_cominit(void);
extern INT os_comopen(CHAR *, CHANNEL *);
extern void os_comclose(CHANNEL *);
extern INT os_comsend(CHANNEL *);
extern INT os_comrecv(CHANNEL *);
extern INT os_comclear(CHANNEL *, INT, INT *);
extern INT os_comctl(CHANNEL *, UCHAR *, INT, UCHAR *, INT *);
extern INT os_seropen(CHAR *, CHANNEL *);
extern void os_serclose(CHANNEL *);
extern INT os_sersend(CHANNEL *);
extern INT os_serrecv(CHANNEL *);
extern INT os_serclear(CHANNEL *, INT, INT *);
extern INT os_serctl(CHANNEL *, UCHAR *, INT, UCHAR *, INT *);
extern INT os_udpopen(CHAR *, CHANNEL *);
extern void os_udpclose(CHANNEL *);
extern INT os_udpsend(CHANNEL *);
extern INT os_udprecv(CHANNEL *);
extern INT os_udpclear(CHANNEL *, INT, INT *);
extern INT os_udpctl(CHANNEL *, UCHAR *, INT, UCHAR *, INT *);
extern INT os_tcpclientopen(CHAR *, CHANNEL *);
extern void os_tcpclientclose(CHANNEL *);
extern INT os_tcpserveropen(CHAR *, CHANNEL *);
extern void os_tcpserverclose(CHANNEL *);
extern INT os_tcpsend(CHANNEL *);
extern INT os_tcprecv(CHANNEL *);
extern INT os_tcpclear(CHANNEL *, INT, INT *);
extern INT os_tcpctl(CHANNEL *, UCHAR *, INT, UCHAR *, INT *);
#if OS_UNIX
extern INT setnonblock(INT);
INT tcpcallback(void *, INT);
INT tcpserveraccept(CHANNEL *);
INT tcpclientopen_V4(CHAR *channelname, CHANNEL *channel);
INT tcpclientopen_V6(CHAR *channelname, CHANNEL *channel);
INT tcpserveropen_V4(CHAR *channelname, CHANNEL *channel);
INT tcpserveropen_V6(CHAR *channelname, CHANNEL *channel);
INT tcpctl_V4(CHANNEL *, UCHAR *, INT, UCHAR *, INT *);
INT tcpctl_V6(CHANNEL *, UCHAR *, INT, UCHAR *, INT *);
#endif
