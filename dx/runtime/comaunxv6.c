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
 ******************************************************************************
 *
 * Special TCP module for machines that support IPv6
 *
 *******************************************************************************/

#define INC_ERRNO
#define INC_STRING
#define INC_CTYPE
#define INC_STDIO
#define INC_STDLIB
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
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#ifdef SUN8
#include <strings.h>
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

#include "base.h"
#include "com.h"
#include "comx.h"
#include "evt.h"

static INT getAddrAndPortStrings(CHANNEL *channel, struct sockaddr_storage *sockstore, CHAR *addrBuf, INT cchAddrBufLen,
		CHAR *portBuf, INT cchPortBufLen);

INT tcpclientopen_V6(CHAR *channelname, CHANNEL *channel)
{
	INT i1, aiErr, retcode;
	PORT savePort;
	CHAR work[256], *p1, address[128], service[8];
	struct addrinfo  hints, *aiHead, *ai;
	struct sockaddr_in6  *pSadrIn6;
	unsigned int scopeId = if_nametoindex(DFLT_SCOPE_ID);

	strcpy(work, channelname);
	p1 = strtok(work, " ");
	if (p1 == NULL) {
		comerror(channel, 0, "INVALID IP ADDRESS OR DOMAIN NAME", 0);
		return(753);
	}
	strcpy(address, p1);
	p1 = strtok(NULL, " ");
	strcpy(service, p1);
	for (i1 = 0; isdigit(*p1); p1++) i1 = i1 * 10 + *p1 - '0';
	if (*p1) {
		comerror(channel, 0, "INVALID PORT NUMBER", 0);
		return(753);
	}
	savePort = (PORT) i1;

	bzero(&hints, sizeof(hints));
#if defined(AI_NUMERICSERV)
	hints.ai_flags = AI_NUMERICSERV;   /* Prevent service name resolution */
#else
	hints.ai_flags = 0;
#endif
   	hints.ai_family = PF_INET6;
	hints.ai_socktype = SOCK_STREAM;   /* Connection-oriented byte stream.   */
	hints.ai_protocol = IPPROTO_TCP;   /* TCP transport layer protocol only. */


	if ((aiErr = getaddrinfo(address, service, &hints, &aiHead)) != 0)
	{
		snprintf(work, sizeof(work), "ERROR - %s", gai_strerror(aiErr));
		comerror(channel, 0, work, aiErr);
		return 753;
	}

	for (ai = aiHead, channel->socket = INVALID_SOCKET; (ai != NULL) && (channel->socket == INVALID_SOCKET);
			ai = ai->ai_next)
	{
		/*
		 * IPv6 kluge.  Make sure the scope ID is set.
		 */
		if (ai->ai_family == PF_INET6) {
			pSadrIn6 = (struct sockaddr_in6*) ai->ai_addr;
			if ( pSadrIn6->sin6_scope_id == 0 )
			{
				pSadrIn6->sin6_scope_id = scopeId;
			}  /* End IF the scope ID wasn't set. */
		}  /* End IPv6 kluge. */

		channel->socket = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (channel->socket < 0) {
			switch (errno) {
			case EAFNOSUPPORT:
			case EPROTONOSUPPORT:
				continue;
			default:
				comerror(channel, 0, "SOCKET FAILURE", errno);
				os_tcpclientclose(channel);
				freeaddrinfo(aiHead);
				return(753);
			}
		}
		/* set close on exec */
		i1 = fcntl(channel->socket, F_GETFD, 0);
		if (i1 != -1) fcntl(channel->socket, F_SETFD, i1 | FD_CLOEXEC);
		channel->flags |= COM_FLAGS_OPEN;

		if (clientkeepalive) {
			i1 = 1;
			if (setsockopt(channel->socket, SOL_SOCKET, SO_KEEPALIVE, &i1, sizeof(i1))) {
				comerror(channel, 0, "Set KEEPALIVE FAILURE", errno);
				goto errexit;
			}
		}

		if (setnonblock(channel->socket) == -1) {
			comerror(channel, 0, "NON-BLOCKING MODE FAILURE", errno);
			goto errexit;
		}
		channel->flags |= COM_FLAGS_NOBLOCK;

		retcode = evtdevinit(channel->socket, 0, tcpcallback, (void *) channel);
		if (retcode < 0) {
			comerror(channel, 0, "EVTDEVINIT FAILURE", errno);
			goto errexit;
		}
		channel->flags |= COM_FLAGS_DEVINIT;

		retcode = connect(channel->socket, ai->ai_addr, ai->ai_addrlen);
		if (retcode < 0 && errno == EINPROGRESS && waitForTCPConnect) {
			fd_set rset, wset;
			int n1;
			FD_ZERO(&rset);
			FD_SET(channel->socket, &rset);
			wset = rset;
			n1 = select(channel->socket + 1, &rset, &wset, NULL, NULL);
			if (n1 < 0 && errno != EINTR) {
				comerror(channel, 0, strerror(errno), errno);
				goto errexit;
			}
			if (n1 == 0) {  /* Timed Out */
				errno = ETIMEDOUT;
				comerror(channel, 0, strerror(errno), errno);
				goto errexit;
			}
			else {
				int error;
				int lon = sizeof(error);
				if (FD_ISSET(channel->socket, &rset) || FD_ISSET(channel->socket, &wset)) {
					if (getsockopt(channel->socket, SOL_SOCKET, SO_ERROR, (void*)&error,
#if defined(Linux) || defined(FREEBSD) || defined(__MACOSX)
							(socklen_t*)
#endif
							&lon) < 0) {
						comerror(channel, 0, "getsockopt error", errno);
						goto errexit;
					}
					if (error) {
						comerror(channel, 0, "CONNECT FAILURE", error);
						goto errexit;
					}
				}
				else {
					comerror(channel, 0, "select error, sockfd not set", 0);
					goto errexit;
				}
			}
		}
		if (retcode != -1
		#ifdef EINPROGRESS
			|| errno == EINPROGRESS
		#endif
			|| errno == EAGAIN)
		{
			memcpy(&channel->sendaddr, ai->ai_addr, ai->ai_addrlen);
			channel->devpoll |= EVENTS_WRITE | EVENTS_ERROR;
			evtdevset(channel->socket, channel->devpoll);
			channel->flags |= COM_FLAGS_CONNECT;
			freeaddrinfo(aiHead);
			return 0;
		}
	}
	comerror(channel, 0, "CONNECT FAILURE", errno);
errexit:
	os_tcpclientclose(channel);
	freeaddrinfo(aiHead);
	return(753);
}

static INT tcpserveropen(CHANNEL *channel, struct addrinfo *addrinfo) {
	INT reuse = 1;
	struct linger lingstr;

	channel->socket = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
	if (channel->socket < 0) {
		return channel->socket;
	}
	/* set close on exec */
	fcntl(channel->socket, F_SETFD, FD_CLOEXEC);

	setsockopt(channel->socket, SOL_SOCKET, SO_REUSEADDR, (void *) &reuse, sizeof(reuse));

	lingstr.l_onoff = 1;
	lingstr.l_linger = 0;
	setsockopt(channel->socket, SOL_SOCKET, SO_LINGER, (void *) &lingstr, sizeof(lingstr));

	if (bind(channel->socket, addrinfo->ai_addr, addrinfo->ai_addrlen) == 0) {
		return 0;
	}
	return -1;
}

INT tcpserveropen_V6(CHAR *channelname, CHANNEL *channel)
{
	INT i1, retcode;
	CHAR work[200], *p1, *strport;
	CHANNEL *ch;
	struct addrinfo *AI, hints, *AddrInfo;

	strcpy(work, channelname);
	p1 = strtok(work, " ");
	strport = p1;
	for (i1 = 0; isdigit(*p1); p1++) i1 = i1 * 10 + *p1 - '0';
	if (*p1) {
		comerror(channel, 0, "INVALID PORT NUMBER", 0);
		return(753);
	}
	channel->ourport = (PORT) i1;

	for (ch = getchannelhead(); ch != NULL; ch = ch->nextchannelptr) {
		if (!ISCHANNELTYPETCPSERVER(ch) || ch->ourport != i1 || ch == channel) continue;
		/* check for another server that is waiting or is listening */
		if (!(ch->flags & COM_FLAGS_OPEN) || (ch->flags & COM_FLAGS_LISTEN)) break;
		if (ch->flags & COM_FLAGS_FDLISTEN) {  /* found connected server also holding listening socket */
			ch->flags &= ~COM_FLAGS_FDLISTEN;
			channel->socket = ch->fdlisten;
			channel->flags |= COM_FLAGS_OPEN | COM_FLAGS_BOUND | COM_FLAGS_NOBLOCK | COM_FLAGS_DEVINIT | COM_FLAGS_LISTEN;
			if (evtdevinit(channel->socket, EVENTS_READ | EVENTS_ERROR, tcpcallback, (void *) channel) < 0) {
				comerror(channel, 0, "EVTDEVINIT FAILED TO MODIFY CALLBACK", 0);
				os_tcpserverclose(channel);
				return(753);
			}
			channel->devpoll = EVENTS_READ | EVENTS_ERROR;
			break;
		}
	}

	if (ch == NULL) {
		bzero(&hints, sizeof(hints));
		switch (channel->channeltype) {
		case CHANNELTYPE_TCPSERVER:
	    	hints.ai_family = PF_UNSPEC;
	    	break;
		case CHANNELTYPE_TCPSERVER4:
	    	hints.ai_family = PF_INET;
	    	break;
		case CHANNELTYPE_TCPSERVER6:
	    	hints.ai_family = PF_INET6;
	    	break;
		}
	    hints.ai_socktype = SOCK_STREAM;
	    hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;
#if defined(AI_NUMERICSERV)
	    hints.ai_flags |= AI_NUMERICSERV;   /* Prevent service name resolution */
#endif

/*	    hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE | AI_NUMERICSERV; */
	    retcode = getaddrinfo(NULL, strport, &hints, &AddrInfo);
		if (retcode != 0) {
			snprintf(work, sizeof(work), _T("getaddrinfo - %s"), gai_strerror(retcode));
			comerror(channel, 0, work, retcode);
			os_tcpserverclose(channel);
			return(753);
		}
		for (AI = AddrInfo; AI != NULL; AI = AI->ai_next) {
			if (tcpserveropen(channel, AI) == 0) break;
		}
		if (AI == NULL) {
			comerror(channel, 0, "Unable to create socket and bind it", -1);
			os_tcpserverclose(channel);
			freeaddrinfo(AddrInfo);
			return(753);
		}
		channel->flags |= COM_FLAGS_OPEN;
		channel->flags |= COM_FLAGS_BOUND;
		freeaddrinfo(AddrInfo);

		if (channel->ourport == 0) {  /* What does this mean? Is this capability used? */
			struct sockaddr_storage ss;
			u_short port = 0;
			int slen = sizeof(struct sockaddr_storage);
			retcode = getsockname(channel->socket, (SA *)&ss,
#if defined(Linux) || defined(FREEBSD) || defined(__MACOSX)
					(socklen_t*)
#endif
					&slen);
			if (retcode == -1) {
				comerror(channel, 0, "GETSOCKNAME FAILURE", errno);
				os_tcpserverclose(channel);
				return(753);
			}
			if (ss.ss_family == AF_INET) port = ntohs(((struct sockaddr_in *) &ss)->sin_port);
			else if (ss.ss_family == AF_INET6) {
				port = ntohs(((struct sockaddr_in6 *) &ss)->sin6_port);
			}
			channel->ourport = port;
		}
		if (setnonblock(channel->socket) == -1) {
			comerror(channel, 0, "NON-BLOCKING MODE FAILURE", errno);
			os_tcpserverclose(channel);
			return(753);
		}
		channel->flags |= COM_FLAGS_NOBLOCK;

		retcode = listen(channel->socket, 5);
		if (retcode == -1) {
			comerror(channel, 0, "LISTEN FAILURE", errno);
			os_tcpserverclose(channel);
			return(753);
		}
		channel->flags |= COM_FLAGS_LISTEN;

		retcode = evtdevinit(channel->socket, EVENTS_READ | EVENTS_ERROR, tcpcallback, (void *) channel);
		if (retcode < 0) {
			comerror(channel, 0, "EVTDEVINIT FAILURE", errno);
			os_tcpserverclose(channel);
			return(753);
		}
		channel->devpoll = EVENTS_READ | EVENTS_ERROR;
		channel->flags |= COM_FLAGS_DEVINIT;
	}

	if (channel->flags & COM_FLAGS_LISTEN) {
		retcode = tcpserveraccept(channel);
		if (retcode < 0) {
			/* comerror called by tcpserveraccept */
			os_tcpserverclose(channel);
			return(753);
		}
		if (serverkeepalive) {
			i1 = 1;
			if (setsockopt(channel->socket, SOL_SOCKET, SO_KEEPALIVE, &i1, sizeof(i1))) {
				comerror(channel, 0, "Set KEEPALIVE FAILURE", errno);
				os_tcpserverclose(channel);
				return(753);
			}
		}
	}
	return(0);
}

INT tcpctl_V6(CHANNEL *channel, UCHAR *msgin, INT msginlen, UCHAR *msgout, INT *msgoutlen)
{
	INT i1, addrlen;
	CHAR work[256], portWork[32], addrWork[64];
	struct sockaddr_storage address;

	while ((msginlen > 0 && msgin[msginlen - 1] == ' ') || msgin[msginlen - 1] == '\t') msginlen--;
	if (msginlen >= (INT) sizeof(work)) msginlen = sizeof(work) - 1;
	for (i1 = 0; i1 < msginlen && msgin[i1] != '=' && !isspace(msgin[i1]); i1++)
		work[i1] = (CHAR) toupper(msgin[i1]);
	work[i1] = '\0';

	addrlen = sizeof(address);
	if (!strcmp(work, "GETLOCALADDR"))  {
		if (channel->flags & COM_FLAGS_OPEN) {
			i1 = getsockname(channel->socket, (SA *) &address,
#if defined(Linux) || defined(FREEBSD) || defined(__MACOSX)
					(socklen_t*)
#endif
					&addrlen);
			if (i1 == INVALID_SOCKET) {
				comerror(channel, 0, "GETSOCKNAME FAILURE", errno);
				return(753);
			}
		}
		else bzero(&address, sizeof(address));
	}
	else if (!strcmp(work, "GETREMOTEADDR"))  {
		if (channel->flags & COM_FLAGS_OPEN) {
			i1 = getpeername(channel->socket, (SA *) &address,
#if defined(Linux) || defined(FREEBSD) || defined(__MACOSX)
					(socklen_t*)
#endif
					&addrlen);
			if (i1 == INVALID_SOCKET) {
				comerror(channel, 0, "GETPEERNAME FAILURE", errno);
				return(753);
			}
		}
		else bzero(&address, sizeof(address));
	}
	else {
		comerror(channel, 0, "INVALID COMCTL REQUEST", 0);
		return(753);
	}

	i1 = getAddrAndPortStrings(channel, &address, addrWork, ARRAYSIZE(addrWork),
			portWork, ARRAYSIZE(portWork));
	if (i1) return i1;
	i1 = snprintf(work, sizeof(work), _T("%s %s"), addrWork, portWork);

	if (i1 > *msgoutlen) i1 = *msgoutlen;
	memcpy(msgout, work, i1);
	*msgoutlen = i1;
	return(0);
}

static INT getAddrAndPortStrings(CHANNEL *channel, struct sockaddr_storage *sockstore, CHAR *addrBuf, INT cchAddrBufLen,
		CHAR *portBuf, INT cchPortBufLen)
{
	CHAR addrWork[128];
	CHAR portWork[32];
	size_t i2 = ARRAYSIZE(addrWork);
	struct sockaddr_in *sockstoreV4 = (struct sockaddr_in *)sockstore;
	struct sockaddr_in6 *sockstoreV6 = (struct sockaddr_in6 *)sockstore;
	switch (sockstore->ss_family) {
		case AF_INET:
			inet_ntop(sockstore->ss_family, &sockstoreV4->sin_addr, addrWork, i2);
			break;
		case AF_INET6:
			inet_ntop(sockstore->ss_family, &sockstoreV6->sin6_addr, addrWork, i2);
			break;
		default:
			comerror(channel, 0, "Internal failure in getAddrAndPortStrings", sockstore->ss_family);
			return 753;
	}
	strncpy(addrBuf, addrWork, cchAddrBufLen);
	snprintf(portWork, sizeof(portWork), "%hu",
			ntohs((sockstore->ss_family == AF_INET) ? sockstoreV4->sin_port : sockstoreV6->sin6_port));
	strncpy(portBuf, portWork, cchPortBufLen);
	return 0;
}

