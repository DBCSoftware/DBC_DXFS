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
 * Special TCP module for machines that do not support IPv6
 * The code in here is basically identical to the 16.0.2+ code
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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

#include "com.h"
#include "comx.h"
#include "evt.h"

union addrstr {
	struct sockaddr gen_addr;
	struct sockaddr_in spec_addr;
};

static INT ipstrtoaddr(CHAR *, struct sockaddr_in*);

INT tcpclientopen_V4(CHAR *channelname, CHANNEL *channel)
{
	INT i1, retcode;
	CHAR work[200], *p1;
	union addrstr addr;

	/* scan ip address or domain name */
	strcpy(work, channelname);
	p1 = strtok(work, " ");
	if (p1 == NULL) {
		comerror(channel, 0, "INVALID IP ADDRESS OR DOMAIN NAME", 0);
		return(753);
	}
	if ((i1 = ipstrtoaddr(p1, (struct sockaddr_in*)&channel->sendaddr))) {
		if (i1 == RC_NO_MEM) {
			comerror(channel, 0, "MEMORY ALLOCATION FAILURE", 0);
		}
		else {
			comerror(channel, 0, "INVALID IP ADDRESS OR DOMAIN NAME", 0);
		}
		return(753);
	}

	p1 = strtok(NULL, " ");
 	for (i1 = 0; isdigit(*p1); p1++) i1 = i1 * 10 + *p1 - '0';
	if (*p1) {
		comerror(channel, 0, "INVALID PORT NUMBER", 0);
		return(753);
	}
	((struct sockaddr_in*)&channel->sendaddr)->sin_port = (PORT) i1;

	channel->socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (channel->socket == INVALID_SOCKET) {
		comerror(channel, 0, "SOCKET FAILURE", errno);
		return(753);
	}
	/* set close on exec */
	i1 = fcntl(channel->socket, F_GETFD, 0);
	if (i1 != -1) fcntl(channel->socket, F_SETFD, i1 | FD_CLOEXEC);
	channel->flags |= COM_FLAGS_OPEN;

	if (clientkeepalive) {
		i1 = 1;
		if (setsockopt(channel->socket, SOL_SOCKET, SO_KEEPALIVE, &i1, sizeof(i1))) {
			comerror(channel, 0, "Set KEEPALIVE FAILURE", errno);
			os_tcpclientclose(channel);
			return(753);
		}
	}

	if (setnonblock(channel->socket) == -1) {
		comerror(channel, 0, "NON-BLOCKING MODE FAILURE", errno);
		os_tcpclientclose(channel);
		return(753);
	}
	channel->flags |= COM_FLAGS_NOBLOCK;

	retcode = evtdevinit(channel->socket, 0, tcpcallback, (void *) channel);
	if (retcode < 0) {
		comerror(channel, 0, "EVTDEVINIT FAILURE", errno);
		os_tcpclientclose(channel);
		return(753);
	}
	channel->flags |= COM_FLAGS_DEVINIT;

	addr.spec_addr.sin_family = PF_INET;
	addr.spec_addr.sin_port = htons(((struct sockaddr_in*)&channel->sendaddr)->sin_port);
	addr.spec_addr.sin_addr.s_addr = ((struct sockaddr_in*)&channel->sendaddr)->sin_addr.s_addr;
	for (i1 = 0; i1 < (INT) sizeof(addr.spec_addr.sin_zero); i1++)
		addr.spec_addr.sin_zero[i1] = 0;
	retcode = connect(channel->socket, &addr.gen_addr, sizeof(addr.spec_addr));
	if (retcode == -1) {
#ifdef EINPROGRESS
		if (errno != EINPROGRESS && errno != EAGAIN)
#else
		if (errno != EAGAIN)
#endif
		{
			comerror(channel, 0, "CONNECT FAILURE", errno);
			os_tcpclientclose(channel);
			return(753);
		}
		channel->devpoll |= EVENTS_WRITE | EVENTS_ERROR;
		evtdevset(channel->socket, channel->devpoll);
	}
	else channel->flags |= COM_FLAGS_CONNECT;
	return(0);
}

INT tcpserveropen_V4(CHAR *channelname, CHANNEL *channel)
{
	INT i1, retcode, reuse;
	CHAR work[200], *p1;
	CHANNEL *ch;
	union addrstr servaddr;
	struct linger lingstr;
#if (__USLC__ || defined(_SEQUENT_)) && !defined(_M_SYS5)
	size_t addrsize;
#elif defined(ADDRSIZE_IS_SOCKLEN_T)
	socklen_t addrsize;
#else
	int addrsize;
#endif

	strcpy(work, channelname);
	p1 = strtok(work, " ");
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

	if (ch == NULL) {  /* unable to find another server with same port */
		channel->socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (channel->socket == INVALID_SOCKET) {
			comerror(channel, 0, "SOCKET FAILURE", errno);
			return(753);
		}
		/* set close on exec */
		fcntl(channel->socket, F_SETFD, FD_CLOEXEC);
		channel->flags |= COM_FLAGS_OPEN;

		reuse = 1;
		setsockopt(channel->socket, SOL_SOCKET, SO_REUSEADDR, (void *) &reuse, sizeof(reuse));

		lingstr.l_onoff = 1;
#ifdef SCO6
		lingstr.l_linger = 3;
#else
		lingstr.l_linger = 0;
#endif
		setsockopt(channel->socket, SOL_SOCKET, SO_LINGER, (void *) &lingstr, sizeof(lingstr));

		servaddr.spec_addr.sin_family = PF_INET;
		servaddr.spec_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		servaddr.spec_addr.sin_port = htons(channel->ourport);
		for (i1 = 0; i1 < (INT) sizeof(servaddr.spec_addr.sin_zero); i1++)
			servaddr.spec_addr.sin_zero[i1] = 0;
		retcode = bind(channel->socket, (struct sockaddr *) &servaddr, sizeof(struct sockaddr_in));
		if (retcode == -1) {
			comerror(channel, 0, "BIND FAILURE", errno);
			os_tcpserverclose(channel);
			return(753);
		}
		channel->flags |= COM_FLAGS_BOUND;

		if (channel->ourport == 0) {
			addrsize = sizeof(servaddr.gen_addr);
			retcode = getsockname(channel->socket, &servaddr.gen_addr, &addrsize);
			if (retcode == -1) {
				comerror(channel, 0, "GETSOCKNAME FAILURE", errno);
				os_tcpserverclose(channel);
				return(753);
			}
			channel->ourport = ntohs(servaddr.spec_addr.sin_port);
		}

		if (setnonblock(channel->socket) == -1) {
			comerror(channel, 0, "NON-BLOCKING MODE FAILURE", errno);
			os_tcpserverclose(channel);
			return(753);
		}
		channel->flags |= COM_FLAGS_NOBLOCK;

		retcode = listen(channel->socket, 5);		/* try a larger value, it was 1 prior to 11-2003 */
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

static INT ipstrtoaddr(CHAR *channelname, struct sockaddr_in *address)
{
	CHAR *namebuf;
	struct hostent *host;
	struct in_addr *ip;

	if (isdigit(*channelname)) address->sin_addr.s_addr = inet_addr(channelname);
	else {
/*** CODE: WHY DO WE NEED THIS MALLOC ? ***/
		if ((namebuf = (CHAR *) malloc(strlen(channelname) + 1)) == NULL) return RC_NO_MEM;
		strcpy(namebuf, channelname);

		if ((host = gethostbyname(namebuf)) == NULL) {
			free(namebuf);
			return RC_ERROR;
		}
		ip = (struct in_addr *) host->h_addr;
		address->sin_addr.s_addr = (ULONG) ip->s_addr;
		free(namebuf);
	}
	return(0);
}

INT tcpctl_V4(CHANNEL *channel, UCHAR *msgin, INT msginlen, UCHAR *msgout, INT *msgoutlen)
{
	INT i1;
	CHAR work[256];
	struct sockaddr_in address;
#if (__USLC__ || defined(_SEQUENT_)) && !defined(_M_SYS5)
	size_t addrsize;
#elif defined(ADDRSIZE_IS_SOCKLEN_T)
	socklen_t addrsize;
#else
	int addrsize;
#endif

	while ((msginlen > 0 && msgin[msginlen - 1] == ' ') || msgin[msginlen - 1] == '\t') msginlen--;
	if (msginlen >= (INT) sizeof(work)) msginlen = sizeof(work) - 1;
	for (i1 = 0; i1 < msginlen && msgin[i1] != '=' && !isspace(msgin[i1]); i1++)
		work[i1] = (CHAR) toupper(msgin[i1]);
	work[i1] = 0;

	if (!strcmp(work, "GETLOCALADDR"))  {
		if (channel->flags & COM_FLAGS_OPEN) {
			addrsize = sizeof(address);
			i1 = getsockname(channel->socket, (struct sockaddr *) &address, &addrsize);
			if (i1 == -1) {
				comerror(channel, 0, "GETSOCKNAME FAILURE", errno);
				return(753);
			}
		}
		else memset(&address, 0, sizeof(address));
		i1 = sprintf(work, "%s %d", (CHAR *) inet_ntoa(address.sin_addr), (int) address.sin_port);
		if (i1 > *msgoutlen) i1 = *msgoutlen;
		memcpy(msgout, work, i1);
		*msgoutlen = i1;
	}
	else if (!strcmp(work, "GETREMOTEADDR"))  {
		if (channel->flags & COM_FLAGS_OPEN) {
			addrsize = sizeof(address);
			i1 = getpeername(channel->socket, (struct sockaddr *) &address, &addrsize);
			if (i1 == -1) {
				comerror(channel, 0, "GETPEERNAME FAILURE", errno);
				return(753);
			}
		}
		else memset(&address, 0, sizeof(address));
		i1 = sprintf(work, "%s %d", (CHAR *) inet_ntoa(address.sin_addr), (int) address.sin_port);
		if (i1 > *msgoutlen) i1 = *msgoutlen;
		memcpy(msgout, work, i1);
		*msgoutlen = i1;
	}
	else {
		comerror(channel, 0, "INVALID COMCTL REQUEST", 0);
		return(753);
	}
	return(0);
}

