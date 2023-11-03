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
#define INC_SIGNAL
#define INC_CTYPE
#define INC_TYPES
#define INC_ERRNO
#include "includes.h"

#include <unistd.h>
#if defined(USE_POSIX_TERMINAL_IO)
#include <termios.h>
#else
#include <termio.h>
#endif
#include <sys/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#if (!defined(__MACOSX) && !defined(Linux))
#include <poll.h>
#endif
#include <netdb.h>
#include <sys/stat.h>

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

#include "base.h"
#include "com.h"
#include "comx.h"
#include "fio.h"
#include "evt.h"
#include "tim.h"

/* defines for getaddress() */
#define GETADDRESS_SETSEND 1

/* tcp communication routines */
static INT tcpsend(CHANNEL *);
static INT tcprecv(CHANNEL *);

INT os_tcpclientopen(CHAR *channelname, CHANNEL *channel)
{
#ifdef NO_IPV6
	return tcpclientopen_V4(channelname, channel);
#else
	if (channel->channeltype == CHANNELTYPE_TCPCLIENT6) {
		return tcpclientopen_V6(channelname, channel);
	}
	else {
		return tcpclientopen_V4(channelname, channel);
	}
#endif
}

INT os_tcpserveropen(CHAR *channelname, CHANNEL *channel)
{
#ifdef NO_IPV6
	return tcpserveropen_V4(channelname, channel);
#else
	if (channel->channeltype == CHANNELTYPE_TCPSERVER6) {
		return tcpserveropen_V6(channelname, channel);
	}
	else {
		return tcpserveropen_V4(channelname, channel);
	}
#endif
}

void os_tcpclientclose(CHANNEL *channel)
{
	os_tcpclear(channel, COM_CLEAR_SEND | COM_CLEAR_RECV, NULL);

	if (channel->flags & COM_FLAGS_DEVINIT) evtdevexit(channel->socket);
	if (channel->flags & COM_FLAGS_OPEN) close(channel->socket);
	channel->flags &=
		~(COM_FLAGS_OPEN | COM_FLAGS_BOUND | COM_FLAGS_NOBLOCK | COM_FLAGS_DEVINIT | COM_FLAGS_CONNECT);
}

void os_tcpserverclose(CHANNEL *channel)
{
	CHANNEL *ch, *ch1, *ch2;

	os_tcpclear(channel, COM_CLEAR_SEND | COM_CLEAR_RECV, NULL);

	if (channel->flags & (COM_FLAGS_LISTEN | COM_FLAGS_FDLISTEN)) {
		ch1 = ch2 = NULL;
		for (ch = getchannelhead(); ch != NULL; ch = ch->nextchannelptr) {
			if (!ISCHANNELTYPETCPSERVER(ch) || ch->ourport != channel->ourport || ch == channel) continue;
			if (!(ch->flags & COM_FLAGS_OPEN)) {
				if (ch1 == NULL) ch1 = ch;
			}
			else if ((ch->flags & COM_FLAGS_CONNECT) && !(ch->flags & COM_FLAGS_FDLISTEN) && ch2 == NULL) ch2 = ch;
		}
		if (ch1 != NULL) {  /* found a server waiting to listen */
			if (channel->flags & COM_FLAGS_LISTEN) {
				ch1->socket = channel->socket;
				channel->flags &= ~(COM_FLAGS_OPEN | COM_FLAGS_BOUND | COM_FLAGS_NOBLOCK | COM_FLAGS_DEVINIT | COM_FLAGS_LISTEN);
			}
			else {  /* should not happen, but support just in case */
				ch1->socket = channel->fdlisten;
				channel->flags &= ~COM_FLAGS_FDLISTEN;
			}
			ch1->flags |= COM_FLAGS_OPEN | COM_FLAGS_BOUND | COM_FLAGS_NOBLOCK | COM_FLAGS_DEVINIT | COM_FLAGS_LISTEN;
			if (evtdevinit(ch1->socket, EVENTS_READ | EVENTS_ERROR, tcpcallback, (void *) ch1) < 0) {
				comerror(ch1, COM_PER_ERROR, "EVTDEVINIT FAILED TO MODIFY CALLBACK", 0);
			}
			else {
				ch1->devpoll = EVENTS_READ | EVENTS_ERROR;
				if (tcpserveraccept(ch1) < 0) {
					/* comerror called by tcpserveraccept */
					ch1->status |= COM_PER_ERROR;
					evtset(ch1->sendevtid);
					evtset(ch1->recvevtid);
					evtdevset(ch1->socket, 0);
				}
			}
		}
		else if (ch2 != NULL) {  /* found a server that is connected */
			if (channel->flags & COM_FLAGS_LISTEN) {
				if (evtdevinit(channel->socket, 0, tcpcallback, (void *) ch2) >= 0) {
					ch2->fdlisten = channel->socket;
					channel->flags &= ~(COM_FLAGS_OPEN | COM_FLAGS_BOUND | COM_FLAGS_NOBLOCK | COM_FLAGS_DEVINIT | COM_FLAGS_LISTEN);
					ch2->flags |= COM_FLAGS_FDLISTEN;
				}
			}
			else {
				ch2->fdlisten = channel->fdlisten;
				channel->flags &= ~COM_FLAGS_FDLISTEN;
				ch2->flags |= COM_FLAGS_FDLISTEN;
			}
		}
	}

	if (channel->flags & COM_FLAGS_FDLISTEN) {
		evtdevexit(channel->fdlisten);
		close(channel->fdlisten);
	}
/*	if (channel->flags & COM_FLAGS_CONNECT) ; */
/*	if (channel->flags & COM_FLAGS_LISTEN) ; */
	if (channel->flags & COM_FLAGS_DEVINIT) evtdevexit(channel->socket);
/*	if (channel->flags & COM_FLAGS_NOBLOCK) ; */
/*	if (channel->flags & COM_FLAGS_BOUND) ; */
	if (channel->flags & COM_FLAGS_OPEN) close(channel->socket);
	channel->flags &= ~(COM_FLAGS_OPEN | COM_FLAGS_BOUND | COM_FLAGS_NOBLOCK | COM_FLAGS_DEVINIT | COM_FLAGS_LISTEN | COM_FLAGS_CONNECT | COM_FLAGS_FDLISTEN);
}

INT os_tcpsend(CHANNEL *channel)
{
	INT retcode, timehandle;
	UCHAR timestamp[16];

	if (channel->status & COM_SEND_MASK) os_tcpclear(channel, COM_CLEAR_SEND, NULL);

	if (channel->status & COM_PER_ERROR) return(753);

	if (channel->flags & COM_FLAGS_CONNECT) {
		retcode = tcpsend(channel);
 		if (retcode <= 0) return(0);  /* success or comerror w/ com_send_error called by tcpsend */
	}

	if (channel->sendtimeout == 0) {
		channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_TIME;
		evtset(channel->sendevtid);
		return(0);
	}

	channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_PEND;
	if (channel->sendtimeout > 0) {
		msctimestamp(timestamp);
		if (timadd(timestamp, channel->sendtimeout * 100) ||
		   (timehandle = timset(timestamp, channel->sendevtid)) < 0) {
			comerror(channel, 0, "SET TIMER FAILURE", 0);
			return(753);
		}
		if (!timehandle) {
			channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_TIME;
			evtset(channel->sendevtid);
			return(0);
		}
		channel->sendth = timehandle;
	}
	if (channel->flags & COM_FLAGS_CONNECT) {
		channel->devpoll |= EVENTS_WRITE | EVENTS_ERROR;
		evtdevset(channel->socket, channel->devpoll);
	}
	return(0);
}

INT os_tcprecv(CHANNEL *channel)
{
	INT retcode, timehandle;
	UCHAR timestamp[16];

	if (channel->status & COM_RECV_MASK) os_tcpclear(channel, COM_CLEAR_RECV, NULL);

	if (channel->status & COM_PER_ERROR) return(753);

	if (channel->flags & COM_FLAGS_CONNECT) {
		retcode = tcprecv(channel);
 		if (retcode <= 0) return(0);  /* success or comerror w/ com_recv_error called by tcprecv */
	}

	if (channel->recvtimeout == 0) {
		channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_TIME;
		evtset(channel->recvevtid);
		return(0);
	}

	channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_PEND;
	if (channel->recvtimeout > 0) {
		msctimestamp(timestamp);
		if (timadd(timestamp, channel->recvtimeout * 100) ||
		   (timehandle = timset(timestamp, channel->recvevtid)) < 0) {
			comerror(channel, 0, "SET TIMER FAILURE", 0);
			return(753);
		}
		if (!timehandle) {
			channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_TIME;
			evtset(channel->recvevtid);
			return(0);
		}
		channel->recvth = timehandle;
	}
	if (channel->flags & COM_FLAGS_CONNECT) {
		channel->devpoll |= EVENTS_READ | EVENTS_ERROR;
		evtdevset(channel->socket, channel->devpoll);
	}
	return(0);
}

INT os_tcpclear(CHANNEL *channel, INT flag, INT *status)
{
	INT devpoll;

	devpoll = channel->devpoll;

	if (channel->status & COM_SEND_PEND) {
		if (channel->sendtimeout > 0 && evttest(channel->sendevtid)) {  /* timeout happened */
			channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_TIME;
			if (channel->flags & COM_FLAGS_CONNECT) devpoll &= ~EVENTS_WRITE;
		}
	}
	if (channel->status & COM_RECV_PEND) {
		if (channel->recvtimeout > 0 && evttest(channel->recvevtid)) {  /* timeout happened */
			channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_TIME;
			if (channel->flags & COM_FLAGS_CONNECT) devpoll &= ~EVENTS_READ;
		}
	}

	if (status != NULL) *status = channel->status;

	if (flag & COM_CLEAR_SEND) {
		channel->status &= ~COM_SEND_MASK;
		if (channel->sendth) {
			timstop(channel->sendth);
			channel->sendth = 0;
		}
		evtclear(channel->sendevtid);
		if (channel->flags & COM_FLAGS_CONNECT) devpoll &= ~EVENTS_WRITE;
	}
	if (flag & COM_CLEAR_RECV) {
		channel->status &= ~COM_RECV_MASK;
		if (channel->recvth) {
			timstop(channel->recvth);
			channel->recvth = 0;
		}
		evtclear(channel->recvevtid);
		if (channel->flags & COM_FLAGS_CONNECT) devpoll &= ~EVENTS_READ;
	}

/*** CODE: THIS TURNING OFF OF CALL BACK IS REALLY ONLY ***/
/***       NEEDED IN THE CALL BACK ROUTINE ***/
	if (devpoll != channel->devpoll) {
		if (devpoll == EVENTS_ERROR) devpoll = 0;
		evtdevset(channel->socket, devpoll);
		channel->devpoll = devpoll;
	}
	return(0);
}

INT os_tcpctl(CHANNEL *channel, UCHAR *msgin, INT msginlen, UCHAR *msgout, INT *msgoutlen)
{
#ifdef NO_IPV6
	return tcpctl_V4(channel, msgin, msginlen, msgout, msgoutlen);
#else
	if (channel->channeltype == CHANNELTYPE_TCPCLIENT6) {
		return tcpctl_V6(channel, msgin, msginlen, msgout, msgoutlen);
	}
	else {
		return tcpctl_V4(channel, msgin, msginlen, msgout, msgoutlen);
	}
#endif
}


/* TCPCALLBACK */
INT tcpcallback(void *handle, INT polltype)
{
	INT arg, devpoll, retcode;
#if defined(ADDRSIZE_IS_SOCKLEN_T)
	socklen_t len;
#else
	INT len;
#endif
	CHANNEL *channel;

	channel = (CHANNEL *) handle;
	if (!ISCHANNELTYPETCPSERVER(channel) && !ISCHANNELTYPETCPCLIENT(channel)) {
		return(1);
	}

	devpoll = channel->devpoll;

	if (polltype & EVENTS_ERROR) {
/*** CODE: DOES A CONNECTED SERVER GET HANGUP WHEN CLIENT DISCONNECTS ?  IF SO ***/
/***       SERVER SHOULD TRY TO BECOME A LISTENING SERVER AGAIN ***/
		comerror(channel, COM_PER_ERROR, "DEVICE ERROR OR HANGUP", 0);
		return(1);
	}

	if (polltype & EVENTS_WRITE) {
		devpoll &= ~EVENTS_WRITE;
		if (ISCHANNELTYPETCPCLIENT(channel)) {
			if (!(channel->flags & COM_FLAGS_CONNECT)) {
				len = sizeof(INT);
#if (__USLC__ || defined(_SEQUENT_)) && !defined(_M_SYS5)
				retcode = getsockopt(channel->socket, SOL_SOCKET, SO_ERROR, (CHAR *) &arg, (size_t *) &len);
#elif defined(Linux)
				retcode = getsockopt(channel->socket, SOL_SOCKET, SO_ERROR, (CHAR *) &arg, (socklen_t *) &len);
#else
				retcode = getsockopt(channel->socket, SOL_SOCKET, SO_ERROR, (CHAR *) &arg, &len);
#endif
#ifdef ECONNREFUSED
				if (retcode == -1 || arg == ECONNREFUSED)
#else
				if (retcode == -1)
#endif
				{
					if (retcode == -1) comerror(channel, COM_PER_ERROR, "GETSOCKOPT FAILURE", errno);
					else comerror(channel, COM_PER_ERROR, "CONNECTION REFUSED", 0);
					return(1);
				}
				channel->flags |= COM_FLAGS_CONNECT;
				/* devpoll will get set for sending below */
				if (channel->status & COM_RECV_PEND) devpoll |= EVENTS_READ | EVENTS_ERROR;
			}
		}
		if (channel->flags & COM_FLAGS_CONNECT) {
			if (channel->status & COM_SEND_PEND) {
				retcode = tcpsend(channel);
				if (retcode == -2) {
					return(0);
				}
				if (retcode > 0) devpoll |= EVENTS_WRITE | EVENTS_ERROR;
				/* else success or comerror w/ com_send_error called by tcpsend */
			}
		}
	}

	if (polltype & EVENTS_READ) {
		devpoll &= ~EVENTS_READ;
		if (ISCHANNELTYPETCPSERVER(channel)) {
			if (channel->flags & COM_FLAGS_LISTEN) {
				retcode = tcpserveraccept(channel);
				if (retcode < 0) {
					/* comerror called by tcpserveraccept */
					channel->status |= COM_PER_ERROR;
					evtset(channel->sendevtid);
					evtset(channel->recvevtid);
					evtdevset(channel->socket, 0);
					return(1);
				}
				if (retcode == 0) {
					return(0);  /* connected */
				}
				devpoll |= EVENTS_READ | EVENTS_ERROR;
			}
		}
		if (channel->flags & COM_FLAGS_CONNECT) {
			if (channel->status & COM_RECV_PEND) {
				retcode = tcprecv(channel);
				if (retcode == -2) {
					return(0);
				}
				if (retcode > 0) devpoll |= EVENTS_READ | EVENTS_ERROR;
				/* else success or comerror w/ com_send_error called by tcpsend */
			}
		}
	}

	if (devpoll != channel->devpoll) {
		if (devpoll == EVENTS_ERROR) devpoll = 0;
		evtdevset(channel->socket, devpoll);
		channel->devpoll = devpoll;
	}
	return(0);
}

INT tcpserveraccept(CHANNEL *channel)
{
	INT devpoll;
	SOCKET oldfd, newfd;
	CHANNEL *ch;

	/* if error occurs, comerror is called on behalf of caller */
	oldfd = channel->socket;
	newfd = accept(oldfd, NULL, NULL);
	if (newfd == -1) {
#ifdef EWOULDBLOCK
		if (errno == EWOULDBLOCK) return(1);
#else
		if (errno == EAGAIN) return(1);
#endif
		comerror(channel, 0, "ACCEPT FAILURE", errno);
		return RC_ERROR;
	}
	/* set close on exec */
	fcntl(newfd, F_SETFD, FD_CLOEXEC);

	if (setnonblock(newfd) == -1) {
		comerror(channel, 0, "NON-BLOCKING MODE FAILURE", errno);
		os_tcpclientclose(channel);
		return RC_ERROR;
	}

	devpoll = 0;
	if (channel->status & COM_SEND_PEND) devpoll |= EVENTS_WRITE | EVENTS_ERROR;
	if (channel->status & COM_RECV_PEND) devpoll |= EVENTS_READ | EVENTS_ERROR;
	if (evtdevinit(newfd, devpoll, tcpcallback, (void *) channel) < 0) {
		comerror(channel, 0, "EVTDEVINIT FAILURE", errno);
		close(newfd);
		return RC_ERROR;
	}
	channel->socket = newfd;
	channel->devpoll = devpoll;
	channel->flags = (channel->flags & ~COM_FLAGS_LISTEN) | COM_FLAGS_CONNECT;

	for (ch = getchannelhead(); ch != NULL; ch = ch->nextchannelptr) {
		if (!ISCHANNELTYPETCPSERVER(ch) || ch->ourport != channel->ourport || ch == channel) continue;
		if (!(ch->flags & COM_FLAGS_OPEN)) break;
	}
	if (ch == (CHANNEL *) NULL) {  /* no other servers with same port number */
		evtdevset(oldfd, 0);
		channel->fdlisten = oldfd;
		channel->flags |= COM_FLAGS_FDLISTEN;
	}
	else {  /* found another server with same port number */
		ch->socket = oldfd;
		ch->flags |= COM_FLAGS_OPEN | COM_FLAGS_BOUND | COM_FLAGS_NOBLOCK | COM_FLAGS_DEVINIT | COM_FLAGS_LISTEN;
		if (evtdevinit(oldfd, EVENTS_READ | EVENTS_ERROR, tcpcallback, (void *) ch) < 0) {
			comerror(ch, COM_PER_ERROR, "EVTDEVINIT FAILED TO MODIFY CALLBACK", 0);
			return(0);  /* new server errored, not the old server */
		}
		ch->devpoll = EVENTS_READ | EVENTS_ERROR;
		if (tcpserveraccept(ch) < 0) {
			/* comerror called by tcpserveraccept */
			ch->status |= COM_PER_ERROR;
			evtset(ch->sendevtid);
			evtset(ch->recvevtid);
			evtdevset(ch->socket, 0);
			return(0);  /* new server errored, not the old server */
		}
	}
	return(0);
} /* tcpserveraccept */

static INT tcpsend(CHANNEL *channel)
{
	INT len, retcode;

	/* if error occurs, comerror w/ com_send_error is called on behalf of caller */
	for ( ; ; ) {
		len = channel->sendlength - channel->senddone;
		retcode = write(channel->socket, (CHAR *) channel->sendbuf + channel->senddone, len);
		if (retcode < 0) {
#ifdef EWOULDBLOCK
			if (errno == EWOULDBLOCK) return(1);
#else
			if (errno == EAGAIN) return(1);
#endif
			if (errno == EPIPE) {  /* assume disconnect */
				comerror(channel, COM_SEND_ERROR, "DISCONNECT", 0);
				if (channel->status & COM_RECV_PEND) comerror(channel, COM_RECV_ERROR, "DISCONNECT", 0);
				comerror(channel, COM_PER_ERROR, "DISCONNECT", 0);
			}
			else comerror(channel, COM_SEND_ERROR, "WRITE FAILURE", errno);
			return RC_ERROR;
		}
		if (retcode >= len) {
			channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_DONE;
			evtset(channel->sendevtid);
			return(0);
		}
		channel->senddone += retcode;
	}
}

static INT tcprecv(CHANNEL *channel)
{
	INT retcode;

	/* if error occurs, comerror w/ com_recv_error is called on behalf of caller */
	retcode = recv(channel->socket, (CHAR *) channel->recvbuf, channel->recvlength, 0);
#ifdef ECONNRESET
	if (retcode == -1 && errno == ECONNRESET) retcode = 0;
#endif
	if (retcode == -1) {
#ifdef EWOULDBLOCK
		if (errno == EWOULDBLOCK) return(1);
#else
		if (errno == (EAGAIN | EPROTO)) return(1);
#endif
/*** CODE: IS THERE AN ERROR FOR MESSAGES LONGER THEN RECVLENGTH ? ***/
		comerror(channel, COM_RECV_ERROR, "RECV FAILURE", errno);
		return RC_ERROR;
	}
	if (retcode == 0) {  /* assume eof / disconnect */
		comerror(channel, COM_RECV_ERROR, "DISCONNECT", 0);
		if (channel->status & COM_SEND_PEND) comerror(channel, COM_SEND_ERROR, "DISCONNECT", 0);
		comerror(channel, COM_PER_ERROR, "DISCONNECT", 0);
		return RC_ERROR;
	}
	channel->recvdone = retcode;
	channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_DONE;
	evtset(channel->recvevtid);
	return(0);
}

/*
 * Routines that are shared by Unix systems, whether v6 capable or not.
 */

/*
 * SETNONBLOCK
 */
INT setnonblock(INT handle)
{
	INT flag;
	if ( (flag = fcntl(handle, F_GETFL)) < 0) return -1;
	if (fcntl(handle, F_SETFL, (flag | O_NONBLOCK)) < 0) return -1;
	return 0;
}

void comerror(CHANNEL *channel, INT type, CHAR *str, INT err)
{
	INT i1, i2;
	CHAR work[32], *ptr;

	if (type == COM_SEND_ERROR) ptr = channel->senderrormsg;
	else if (type == COM_RECV_ERROR) ptr = channel->recverrormsg;
	else ptr = channel->errormsg;

	i1 = strlen(str);
	if (i1 > ERRORMSGSIZE) i1 = ERRORMSGSIZE;
	memcpy(ptr, str, i1);
	if (err) {
		i2 = snprintf(work, sizeof(work), ", ERRNO = %d", err);
		if (i2 > ERRORMSGSIZE) i2 = ERRORMSGSIZE;
		if (i1 > ERRORMSGSIZE - i2) i1 = ERRORMSGSIZE - i2;
		memcpy(&ptr[i1], work, i2);
		i1 += i2;
	}
	ptr[i1] = '\0';

	if (type == COM_SEND_ERROR) {
		channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_ERROR;
		evtset(channel->sendevtid);
	}
	else if (type == COM_RECV_ERROR) {
		channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_ERROR;
		evtset(channel->recvevtid);
	}
	else if (type == COM_PER_ERROR) {
		channel->status |= COM_PER_ERROR;
		evtset(channel->sendevtid);
		evtset(channel->recvevtid);
		evtdevset(channel->socket, 0);
	}
}

