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
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

#include "base.h"
#include "com.h"
#include "comx.h"
#include "evt.h"
#include "tim.h"

#ifndef NO_IPV6
static INT setSendAddressInChannel(CHAR *, CHANNEL *);
#endif
static INT udpcallback(void *, INT);

/*
 * Old non-IPv6 version.
 * This was replaced on newer OSes with setSendAddressInChannel
 */
#ifdef NO_IPV6
static INT getaddress(CHAR *nameptr, CHANNEL *channel)
{
	CHAR *portptr;
	CHAR *terminator;
	CHAR *namebuf;
	struct hostent *host;
	struct in_addr *ip;

	portptr = nameptr;
	while (*portptr && *portptr != ' ' && *portptr != '\t') portptr++;
	if (!*portptr) return RC_ERROR;

	*portptr++ = 0;
	while (*portptr == ' ' || *portptr == '\t') portptr++;
	if (!(*portptr)) return RC_ERROR;

	terminator = portptr;
	while (isdigit(*terminator)) terminator++;
	if (*terminator && (*terminator == ' ' || *terminator == '\t')) {
		*terminator = 0;
		terminator++;
	}
	while(*terminator == ' ' || *terminator == '\t') terminator++;

	/*channel->sendport = atoi(portptr);*/

	if (isdigit(*nameptr)) {
		channel->sendaddr.sin_addr.s_addr = inet_addr(nameptr);
	}
	else {
		if ((namebuf = (CHAR *)malloc(strlen(nameptr) + 1)) == NULL) return RC_NO_MEM;
		strcpy(namebuf, nameptr);
		if ((host = gethostbyname(namebuf)) == NULL) {
			free(namebuf);
			return RC_ERROR;
		}
		ip = (struct in_addr *) host->h_addr;
		channel->sendaddr.sin_addr.s_addr = ip->s_addr;
		free(namebuf);
	}
	return 0;
}
#endif


/*
 * As of 04/01/11, after making tcp v6 ready, this routine
 * is used by udp only.
 *
 * Returns 0 if success
 * Returns RC_ERROR if the syntax is bad
 * Returns 753 and comerror called if system call failure
 */
#ifndef NO_IPV6
static INT setSendAddressInChannel(CHAR *nameptr, CHANNEL *channel)
{
	CHAR *portptr, work[64], *ptr;
	CHAR *terminator;
	//struct hostent *host;
	USHORT addrFamily;
	struct addrinfo hints, *AddrInfo;
	INT i1;
	USHORT numericPort;

	addrFamily = channel->ouraddr.ss_family;

	portptr = nameptr;
	while (*portptr && !isspace(*portptr)) portptr++;
	if (!*portptr) return RC_ERROR;

	*portptr++ = '\0';
	while (isspace(*portptr)) portptr++;
	if (!(*portptr)) return RC_ERROR;
	terminator = ptr = portptr;
	for (numericPort = 0; isdigit(*ptr); ptr++) numericPort = numericPort * 10 + *ptr - '0';

	while (isdigit(*terminator)) terminator++;
	if (*terminator && isspace(*terminator)) *terminator = '\0';


	bzero(&hints, sizeof(hints));
#if defined(AI_NUMERICSERV)
	hints.ai_flags = AI_NUMERICSERV;
#else
	hints.ai_flags = 0;
#endif
	hints.ai_family = addrFamily;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	if ( (i1 = getaddrinfo(nameptr, portptr, &hints, &AddrInfo) != 0) ) {
		snprintf(work, sizeof(work), "getaddrinfo - %s", gai_strerror(i1));
		comerror(channel, COM_PER_ERROR, work, i1);
		return(753);
	}
	if (addrFamily == AF_INET6) {
		((struct sockaddr_in6*)AddrInfo->ai_addr)->sin6_port = htons(numericPort);
	}
	else {
		((struct sockaddr_in*)AddrInfo->ai_addr)->sin_port = htons(numericPort);
	}
	memcpy(&channel->sendaddr, AddrInfo->ai_addr, AddrInfo->ai_addrlen);
	freeaddrinfo(AddrInfo);

	return 0;
}
#endif

INT os_udpclear(CHANNEL *channel, INT flag, INT *status)
{
	INT devpoll;

	devpoll = channel->devpoll;

	if (channel->status & COM_SEND_PEND) {
		if (channel->sendtimeout > 0 && evttest(channel->sendevtid)) {  /* timeout happened */
			channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_TIME;
			devpoll &= ~EVENTS_WRITE;
		}
	}
	if (channel->status & COM_RECV_PEND) {
		if (channel->recvtimeout > 0 && evttest(channel->recvevtid)) {  /* timeout happened */
			channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_TIME;
			devpoll &= ~EVENTS_READ;
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
		devpoll &= ~EVENTS_WRITE;
	}
	if (flag & COM_CLEAR_RECV) {
		channel->status &= ~COM_RECV_MASK;
		if (channel->recvth) {
			timstop(channel->recvth);
			channel->recvth = 0;
		}
		evtclear(channel->recvevtid);
		devpoll &= ~EVENTS_READ;
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

/**
 * There is a similar version of this function in comaunx.c for TCP use. That other one
 * supports IPv6. This one does not.
 */
static INT getAddrAndPortStrings(CHANNEL *channel, struct sockaddr_in *sockstore, CHAR *addrBuf,
		INT cchAddrBufLen, CHAR *portBuf, INT cchPortBufLen)
{
	CHAR addrWork[128];
#ifdef NO_IPV6
	CHAR *ptr;
#endif
	CHAR portWork[32];
	size_t i2 = ARRAYSIZE(addrWork);
	switch (sockstore->sin_family) {
		case AF_INET:
#ifndef NO_IPV6
			inet_ntop(sockstore->sin_family, &sockstore->sin_addr, addrWork, i2);
#else
			ptr = inet_ntoa(sockstore->sin_addr);
			snprintf(addrWork, i2, "%s", ptr);
#endif
			break;
		default:
			comerror(channel, 0, "Internal failure in getAddrAndPortStrings", sockstore->sin_family);
			return 753;
	}
	strncpy(addrBuf, addrWork, cchAddrBufLen);
	snprintf(portWork, sizeof(portWork), "%hu", ntohs(sockstore->sin_port));
	strncpy(portBuf, portWork, cchPortBufLen);
	return 0;
}


INT os_udpctl(CHANNEL *channel, UCHAR *msgin, INT msginlen, UCHAR *msgout, INT *msgoutlen)
{
	INT i1;
	PORT port;
	struct sockaddr *address;
	ULONG addr;
	CHAR work[256], addrWork[64], portWork[32];

	while (msginlen > 0 && isspace(msgin[msginlen - 1])) msginlen--;
	if (msginlen >= (INT) sizeof(work)) msginlen = sizeof(work) - 1;
	for (i1 = 0; i1 < msginlen && msgin[i1] != '=' && !isspace(msgin[i1]); i1++)
		work[i1] = (CHAR) toupper(msgin[i1]);
	work[i1] = '\0';

	if (!strcmp(work, "GETRECVADDR"))  {
		address = (struct sockaddr *)&channel->recvaddr;
		port = ntohs(((struct sockaddr_in*)address)->sin_port);
		if (!port) goto nullmsgoutreturn;
		addr = ntohl(((struct sockaddr_in*)address)->sin_addr.s_addr);
		if (!addr) goto nullmsgoutreturn;

		i1 = getAddrAndPortStrings(channel, (struct sockaddr_in*)address, addrWork, ARRAYSIZE(addrWork),
				portWork, ARRAYSIZE(portWork));
		if (i1) return i1;
		i1 = snprintf(work, sizeof(work), "%s %s", addrWork, portWork);
		if (i1 > *msgoutlen) i1 = *msgoutlen;
		memcpy(msgout, work, i1);
		*msgoutlen = i1;
	}
	else if (!strcmp(work, "GETLOCALADDR")) {
		port = channel->ourport;
		i1 = snprintf(work, sizeof(work), "0.0.0.0 %d", (port & 0xFFFF));
		if (i1 > *msgoutlen) i1 = *msgoutlen;
		memcpy(msgout, work, i1);
		*msgoutlen = i1;
	}
	else if (!strcmp(work, "SETSENDADDR"))  {
		if (channel->status & COM_SEND_PEND) {
			comerror(channel, 0, "SETSENDADDR NOT ALLOWED WHILE SEND IS PENDING", 0);
			return(753);
		}
		if (++i1 >= msginlen) {
			comerror(channel, 0, "MISSING SEND ADDRESS", 0);
			return(753);
		}
		memcpy(work, msgin + i1, msginlen - i1);
		work[msginlen - i1] = '\0';
#ifndef NO_IPV6
		if ((i1 = setSendAddressInChannel(work, channel)) != 0) {
#else
		if ((i1 = getaddress(work, channel)) != 0) {
#endif
			if (i1 == RC_NO_MEM) {
				comerror(channel, COM_SEND_ERROR, "MEMORY ALLOCATION FAILURE", 0);
			}
			return(753);
		}
		*msgoutlen = 0;
	}
	else {
		comerror(channel, 0, "INVALID COMCTL REQUEST", 0);
		return(753);
	}
	return(0);

nullmsgoutreturn:
	if (*msgoutlen) {
		*msgout = '0';
		*msgoutlen = 1;
	}
	return(0);
}

INT os_udpopen(CHAR *channelname, CHANNEL *channel)
{
	INT i1, retcode;
	struct sockaddr_in workAddr;
	UINT32 v4anyaddr = htonl(INADDR_ANY);

	for (i1 = 0; isdigit(*channelname); channelname++) i1 = i1 * 10 + *channelname - '0';
	if (!i1 || (*channelname && !isspace(*channelname))) {
		comerror(channel, 0, "INVALID PORT NUMBER", 0);
		return(753);
	}
	channel->ourport = (PORT) i1;

	/*
	 * For 16.1 we will not be supporting UDP using V6, too complicated!  (as of 04/25/11 jpr)
	 */
	channel->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (channel->socket == INVALID_SOCKET) {
		comerror(channel, COM_PER_ERROR, "SOCKET FAILURE", errno);
		return(753);
	}
	channel->flags |= COM_FLAGS_OPEN;
	/* set close on exec */
	i1 = fcntl(channel->socket, F_GETFD, 0);
	if (i1 != -1) fcntl(channel->socket, F_SETFD, i1 | FD_CLOEXEC);
	/* bind the socket */
	memset(&workAddr, 0, sizeof(workAddr));
	memcpy(&((SA*)&workAddr)->sa_data, &v4anyaddr, sizeof(struct in_addr));
	workAddr.sin_family = PF_INET;
	workAddr.sin_port = htons(channel->ourport);
	if (bind(channel->socket, (SA*)&workAddr, sizeof(workAddr)) != 0) {
		comerror(channel, COM_PER_ERROR, "BIND FAILURE", errno);
		os_udpclose(channel);
		return(753);
	}
	/*
	 * Save this for later use by comctl
	 */
	memcpy(&channel->ouraddr, &workAddr, sizeof(workAddr));
	retcode = setnonblock(channel->socket);
	if (retcode == -1) {
		comerror(channel, COM_PER_ERROR, "NON-BLOCKING MODE FAILURE", errno);
		os_udpclose(channel);
		return(753);
	}
	channel->flags |= COM_FLAGS_NOBLOCK;

	retcode = evtdevinit(channel->socket, 0, udpcallback, (void *) channel);
	if (retcode < 0) {
		comerror(channel, COM_PER_ERROR, "EVTDEVINIT FAILURE", errno);
		os_udpclose(channel);
		return(753);
	}
	channel->flags |= COM_FLAGS_DEVINIT;
	return(0);
}

void os_udpclose(CHANNEL *channel)
{
	os_udpclear(channel, COM_CLEAR_SEND | COM_CLEAR_RECV, NULL);

	if (channel->flags & COM_FLAGS_DEVINIT) evtdevexit(channel->socket);
/*	if (channel->flags & COM_FLAGS_NOBLOCK) ; */
/*	if (channel->flags & COM_FLAGS_BOUND) ; */
	if (channel->flags & COM_FLAGS_OPEN) close(channel->socket);
	channel->flags &= ~(COM_FLAGS_OPEN | COM_FLAGS_BOUND | COM_FLAGS_NOBLOCK | COM_FLAGS_DEVINIT);
}

INT os_udpsend(CHANNEL *channel)
{
	INT retcode, timehandle;
	UCHAR timestamp[16];
/*	union addrstr saddr; */

	if (channel->status & COM_SEND_MASK) os_udpclear(channel, COM_CLEAR_SEND, NULL);

	if (channel->status & COM_PER_ERROR) return(753);
	retcode = sendto(channel->socket, (CHAR *) channel->sendbuf, channel->sendlength,
			0, (SA*)&channel->sendaddr, sizeof(struct sockaddr_in));
	/*
			(channel->sendaddr.ss_family == AF_INET) ? sizeof(struct sockaddr_in)
												: sizeof(struct sockaddr_in6));
     */
	if (retcode < 0) {
#ifdef EWOULDBLOCK
		if (errno == EWOULDBLOCK) {
#else
		if (errno == EAGAIN) {
#endif
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
			channel->devpoll |= EVENTS_WRITE | EVENTS_ERROR;
			evtdevset(channel->socket, channel->devpoll);
			return(0);
		}
#ifdef EMSGSIZE
		else if (errno == EMSGSIZE) comerror(channel, COM_SEND_ERROR, "SEND MESSAGE TOO LONG", 0);
#endif
		else comerror(channel, COM_SEND_ERROR, "SENDTO FAILURE", errno);
	}
	else if (retcode != channel->sendlength)
		comerror(channel, COM_SEND_ERROR, "BYTES SENT NOT EQUAL TO BYTES REQUESTED", 0);
	else channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_DONE;

	evtset(channel->sendevtid);
	return(0);
}

INT os_udprecv(CHANNEL *channel)
{
	INT addrlen, retcode, timehandle;
	UCHAR timestamp[16];
#ifndef NO_IPV6
	struct sockaddr_storage raddr;
#else
	struct sockaddr_in raddr;
#endif

	if (channel->status & COM_RECV_MASK) os_udpclear(channel, COM_CLEAR_RECV, NULL);

	if (channel->status & COM_PER_ERROR) return(753);

	addrlen = sizeof(struct sockaddr_in);
	retcode = recvfrom(channel->socket, (CHAR *) channel->recvbuf, channel->recvlength,
			0, (SA*)&raddr,
#if defined(Linux) || defined(HPUX11) || defined(FREEBSD) || defined(__MACOSX)
			(socklen_t*)
#elif defined(SVR5) || defined(SCO6)
			(size_t*)
#endif
			&addrlen);
	if (retcode < 0) {
#ifdef EWOULDBLOCK
		if (errno == EWOULDBLOCK) {
#else
		if (errno == EAGAIN) {
#endif
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
			channel->devpoll |= EVENTS_READ | EVENTS_ERROR;
			evtdevset(channel->socket, channel->devpoll);
			return(0);
		}
#ifdef EMSGSIZE
		else if (errno == EMSGSIZE) {
			channel->recvdone = channel->recvlength;
			channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_DONE;
		}
#endif
		else comerror(channel, COM_RECV_ERROR, "RECVFROM FAILURE", errno);
	}
	else {
		channel->recvdone = retcode;
		channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_DONE;
	}

	if (channel->status & COM_RECV_DONE) {
#ifndef NO_IPV6
 		if (raddr.ss_family != PF_INET && raddr.ss_family != PF_INET6)
#else
 		if (raddr.sin_family != PF_INET)
#endif
			comerror(channel, COM_RECV_ERROR, "RECVFROM ADDRESS FAMILY NOT PF_INET", 0);
		else {
			channel->recvaddr = raddr;
		}
	}

	evtset(channel->recvevtid);
	return(0);
}


static INT udpcallback(void *handle, INT polltype)
{
	INT addrlen, devpoll, retcode;
	CHANNEL *channel;
#ifndef NO_IPV6
	struct sockaddr_storage raddr;
#else
	struct sockaddr_in raddr;
#endif

	channel = (CHANNEL *) handle;
	if (!ISCHANNELTYPEUDP(channel)) {
		evtdevset(channel->socket, 0);
		return(1);
	}

	devpoll = channel->devpoll;

	if (polltype & (DEVPOLL_ERROR | DEVPOLL_HANGUP | DEVPOLL_NOFDES)) {
		comerror(channel, COM_PER_ERROR, "DEVICE ERROR OR HANGUP", 0);
		return(1);
	}

	if (polltype & DEVPOLL_WRITE) {
		devpoll &= ~EVENTS_WRITE;
		if (channel->status & COM_SEND_PEND) {
			addrlen = sizeof(struct sockaddr_in);
			retcode = sendto(channel->socket, (CHAR *) channel->sendbuf, channel->sendlength,
					0, (SA*)&channel->sendaddr, addrlen);
			if (retcode < 0) {
#ifdef EWOULDBLOCK
				if (errno == EWOULDBLOCK) devpoll |= EVENTS_WRITE | EVENTS_ERROR;
#else
				if (errno == EAGAIN) devpoll |= EVENTS_WRITE | EVENTS_ERROR;
#endif
#ifdef EMSGSIZE
				else if (errno == EMSGSIZE)
					comerror(channel, COM_SEND_ERROR, "SEND MESSAGE TOO LONG", 0);
#endif
				else comerror(channel, COM_SEND_ERROR, "SENDTO FAILURE", errno);
			}
			else if (retcode != channel->sendlength)
				comerror(channel, COM_SEND_ERROR, "BYTES SENT NOT EQUAL TO BYTES REQUESTED", 0);
			else channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_DONE;

			if (!(channel->status & COM_SEND_PEND)) evtset(channel->sendevtid);
		}
	}

	if (polltype & (DEVPOLL_READ | DEVPOLL_READPRIORITY | DEVPOLL_READNORM | DEVPOLL_READBAND)) {
		devpoll &= ~EVENTS_READ;
		if (channel->status & COM_RECV_PEND) {
			addrlen = sizeof(raddr);
			retcode = recvfrom(channel->socket, (CHAR *) channel->recvbuf, channel->recvlength,
					0, (SA*)&raddr,
#if defined(Linux) || defined(FREEBSD) || defined(__MACOSX)
					(socklen_t*)
#endif
					&addrlen);
			if (retcode < 0) {
#ifdef EWOULDBLOCK
				if (errno == EWOULDBLOCK) devpoll |= EVENTS_READ | EVENTS_ERROR;
#else
				if (errno == EAGAIN) devpoll |= EVENTS_READ | EVENTS_ERROR;
#endif
#ifdef EMSGSIZE
				else if (errno == EMSGSIZE) {
					channel->recvdone = channel->recvlength;
					channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_DONE;
				}
#endif
				else comerror(channel, COM_RECV_ERROR, "RECVFROM FAILURE", errno);
			}
			else {
				channel->recvdone = retcode;
				channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_DONE;
			}

			if (channel->status & COM_RECV_DONE) {
#ifndef NO_IPV6
				switch (raddr.ss_family) {
				case PF_INET6:
#else
				switch (raddr.sin_family) {
#endif
				case PF_INET:
					channel->recvaddr = raddr;
					break;
				default:
					comerror(channel, COM_RECV_ERROR, _T("RECVFROM ADDRESS FAMILY NOT PF_INETx"), 0);
				}
			}

			if (!(channel->status & COM_RECV_PEND)) evtset(channel->recvevtid);
		}
	}

	if (devpoll != channel->devpoll) {
		if (devpoll == EVENTS_ERROR) devpoll = 0;
		evtdevset(channel->socket, devpoll);
		channel->devpoll = devpoll;
	}
	return(0);
}
