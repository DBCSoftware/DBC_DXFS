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
#ifndef _COM_C_
#define _COM_C_
#endif

#define INC_STDIO
#define INC_STDLIB
#define INC_STRING
#define INC_CTYPE
#define INC_IPV6
#include "includes.h"

#if OS_WIN32
#define COMDLL
#endif

#if OS_UNIX
#include <unistd.h>
#if defined(__MACOSX) || defined(USE_POSIX_TERMINAL_IO)
#include <termios.h>
#else
#include <termio.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#if _MSC_VER >= 1800
#include <VersionHelpers.h>
#endif
#include "base.h"
#include "com.h"
#include "comx.h"
#include "fio.h"

static CHANNEL *channelheadptr = NULL;
static CHANNEL *freechannelheadptr = NULL;
static INT nextrefnum = 0x14657687;

/* Initialize communications routines */
DllExport INT cominit()
{
//#if OS_WIN32
//	OSVERSIONINFO osInfo;
//#endif
	char *ptr;
	channelheadptr = freechannelheadptr = NULL;
	if (!prpget("comm", "tcp", "clientkeepalive", NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) {
		clientkeepalive = TRUE;
	}
	if (!prpget("comm", "tcp", "serverkeepalive", NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) {
		serverkeepalive = TRUE;
	}
	if (!prpget("comm", "tcp", "clientconnectwait", NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) {
		waitForTCPConnect = FALSE;
	}

#ifndef NO_IPV6
#if OS_WIN32
	/**
	 * Set flag to allow the use of the newer TCP system calls that allow IPv6
	 * IF the OS is newer than XP
	 */
	//osInfo.dwOSVersionInfoSize = sizeof(osInfo);
	//GetVersionEx(&osInfo);
	allowIPv6 = IsWindowsXPSP3OrGreater() /*((osInfo.dwMajorVersion == 5 && osInfo.dwMinorVersion >= 2)
			|| osInfo.dwMajorVersion >= 6)*/;
#else
	allowIPv6 = TRUE;
#endif
	if (allowIPv6 && !prpget("comm", "tcp", "allowIPv6", NULL, &ptr, PRP_LOWER) && (
			!strcmp(ptr, "no") || !strcmp(ptr, "off") || !strcmp(ptr, "false"))) {
		allowIPv6 = FALSE;
	}
#endif

	return(0);
}

/* cleanup and uninitialize communications routines */
DllExport void comexit()
{
	CHANNEL *ch;

	while (channelheadptr != NULL) {
		ch = channelheadptr;
		comclose(ch->refnum);
	}
	while (freechannelheadptr != NULL) {
		ch = freechannelheadptr;
		freechannelheadptr = ch->nextchannelptr;
		free(ch);
	}
}

/* open the communications channel */
DllExport INT comopen(CHAR *channelname, INT *refnum)
{
	INT i1, retcode;
	CHAR work[128];
	CHANNEL *ch;

	if (channelname == NULL || refnum == NULL) return(754);

	if (freechannelheadptr != NULL) {
		ch = freechannelheadptr;
		freechannelheadptr = ch->nextchannelptr;
	}
	else {
		ch = (CHANNEL *) malloc(sizeof (CHANNEL));
		if (ch == NULL) return(ERR_NOMEM);
	}
	memset(ch, 0, sizeof(CHANNEL));
	
	/* parse channelname and match upcased first keyword from channelname */
	for (i1 = 0; channelname[i1] && !isspace(channelname[i1]); i1++) work[i1] = (CHAR) toupper(channelname[i1]);
	work[i1] = '\0';
	if (!strcmp(work, "UDP")) ch->channeltype = CHANNELTYPE_UDP;
	else if (!strcmp(work, "UDP4")) ch->channeltype = CHANNELTYPE_UDP4;
	/*
	 * For 16.1 we will not be supporting UDP using V6, too complicated!  (as of 04/25/11 jpr)
	 */
	/*else if (!strcmp(work, "UDP6")) ch->channeltype = CHANNELTYPE_UDP6;*/
	else if (!strcmp(work, "TCPCLIENT")) ch->channeltype = CHANNELTYPE_TCPCLIENT;
	else if (!strcmp(work, "TCPCLIENT4")) ch->channeltype = CHANNELTYPE_TCPCLIENT4;
	else if (!strcmp(work, "TCPCLIENT6")) ch->channeltype = CHANNELTYPE_TCPCLIENT6;
	else if (!strcmp(work, "TCPSERVER")) ch->channeltype = CHANNELTYPE_TCPSERVER;
	else if (!strcmp(work, "TCPSERVER4")) ch->channeltype = CHANNELTYPE_TCPSERVER4;
	else if (!strcmp(work, "TCPSERVER6")) ch->channeltype = CHANNELTYPE_TCPSERVER6;
	else if (!strcmp(work, "SERIAL")) ch->channeltype = CHANNELTYPE_SERIAL;
	else ch->channeltype = CHANNELTYPE_OTHER;  /* it's not TCP, UDP, SERIAL or USER; must be OS dependent */

	if (ch->channeltype != CHANNELTYPE_OTHER) {
		while (isspace(channelname[i1])) i1++;
		channelname += i1;
	}

	if (ISCHANNELTYPETCPSERVER(ch)) retcode = os_tcpserveropen(channelname, ch);
	else if (ISCHANNELTYPETCPCLIENT(ch)) retcode = os_tcpclientopen(channelname, ch);
	else if (ISCHANNELTYPEUDP(ch)) retcode = os_udpopen(channelname, ch);
	else {
		switch (ch->channeltype) {
		case CHANNELTYPE_SERIAL:
			retcode = os_seropen(channelname, ch);
			break;
		default:
			retcode = os_comopen(channelname, ch);
			break;
		}
	}

	if (retcode) {
		ch->nextchannelptr = freechannelheadptr;
		freechannelheadptr = ch;
		return(retcode);
	}

	ch->nextchannelptr = channelheadptr;
	channelheadptr = ch;
	ch->refnum = nextrefnum++;

	*refnum = ch->refnum;
	return(0);
}

/* close a communications channel */
DllExport INT comclose(INT refnum)
{
	CHANNEL *ch, *prevch;

	for (prevch = NULL, ch = channelheadptr; ch != NULL && ch->refnum != refnum; ch = ch->nextchannelptr) prevch = ch;
	if (ch == NULL) return(754);

	if (ISCHANNELTYPETCPSERVER(ch)) os_tcpserverclose(ch);
	else if (ISCHANNELTYPETCPCLIENT(ch)) os_tcpclientclose(ch);
	else if (ISCHANNELTYPEUDP(ch)) os_udpclose(ch);
	else {
		switch (ch->channeltype) {
			case CHANNELTYPE_SERIAL:
				os_serclose(ch);
				break;
			default:
				os_comclose(ch);
				break;
			}
	}

	if (ch->sendbuf != NULL) free(ch->sendbuf);
	if (ch->recvbuf != NULL) free(ch->recvbuf);

	if (prevch != NULL) prevch->nextchannelptr = ch->nextchannelptr;
	else channelheadptr = ch->nextchannelptr;
	ch->nextchannelptr = freechannelheadptr;
	freechannelheadptr = ch;
	return(0);
}

/* send a message */
DllExport INT comsend(INT refnum, INT evtid, INT timeout, UCHAR *sendbuf, INT count)
{
	INT i1, retcode;
	UCHAR *ptr;
	CHANNEL *ch;

	for (ch = channelheadptr; ch != NULL && ch->refnum != refnum; ch = ch->nextchannelptr);
	if (ch == NULL || sendbuf == NULL || count < 0) return(754);

	if (!(i1 = count + ch->sendstart[0] + ch->sendend[0])) i1 = 1;
	if (ch->sendbuf == NULL || i1 > ch->sendbufsize) {
		i1 = (((i1 - 1) / 1024) + 1) * 1024;
		if (ch->sendbuf != NULL) ptr = (UCHAR *) realloc(ch->sendbuf, i1);
		else ptr = (UCHAR *) malloc(i1);
		if (ptr == NULL) {
			ch->status = COM_PER_ERROR;
			return(ERR_NOMEM);
		}
		ch->sendbuf = ptr;
		ch->sendbufsize = i1;
	}

	i1 = 0;
	if (ch->sendstart[0]) {
		memcpy(ch->sendbuf, &ch->sendstart[1], ch->sendstart[0]);
		i1 = ch->sendstart[0];
	}
	memcpy(&ch->sendbuf[i1], sendbuf, count);
	i1 += count;
	if (ch->sendend[0]) {
		memcpy(&ch->sendbuf[i1], &ch->sendend[1], ch->sendend[0]);
		i1 += ch->sendend[0];
	}
	ch->sendlength = i1;
	ch->senddone = 0;
	ch->sendtimeout = timeout;
	ch->sendevtid = evtid;

	if (ISCHANNELTYPETCPSERVER(ch)) retcode = os_tcpsend(ch);
	else if (ISCHANNELTYPETCPCLIENT(ch)) retcode = os_tcpsend(ch);
	else if (ISCHANNELTYPEUDP(ch)) retcode = os_udpsend(ch);
	else {
		switch (ch->channeltype) {
		case CHANNELTYPE_SERIAL:
			retcode = os_sersend(ch);
			break;
		default:
			retcode = os_comsend(ch);
			break;
		}
	}
	return(retcode);
}

/* start receive of a message */
DllExport INT comrecv(INT refnum, INT evtid, INT timeout, INT count)
{
	INT i1, retcode;
	UCHAR *ptr;
	CHANNEL *ch;

	for (ch = channelheadptr; ch != NULL && ch->refnum != refnum; ch = ch->nextchannelptr);
	if (ch == NULL || count < 1) return(754);
	if ((i1 = count) < ch->recvlimit) i1 = ch->recvlimit;
	if (!(i1 += ch->recvstart[0] + ch->recvend[0])) i1 = 1;
	if (ch->recvbuf == NULL || i1 > ch->recvbufsize) {
		i1 = (((i1 - 1) / 1024) + 1) * 1024;
		if (ch->recvbuf != NULL) ptr = (UCHAR *) realloc(ch->recvbuf, i1);
		else ptr = (UCHAR *) malloc(i1);
		if (ptr == NULL) {
			ch->status = COM_PER_ERROR;
			return(ERR_NOMEM);
		}
		ch->recvbuf = ptr;
		ch->recvbufsize = i1;
	}

	ch->recvlength = count;
	ch->recvdone = 0;
	ch->recvtimeout = timeout;
	ch->recvevtid = evtid;

	if (ISCHANNELTYPETCPSERVER(ch)) retcode = os_tcprecv(ch);
	else if (ISCHANNELTYPETCPCLIENT(ch)) retcode = os_tcprecv(ch);
	else if (ISCHANNELTYPEUDP(ch)) retcode = os_udprecv(ch);
	else {
		switch (ch->channeltype) {
		case CHANNELTYPE_SERIAL:
			retcode = os_serrecv(ch);
			break;
		default:
			retcode = os_comrecv(ch);
			break;
		}
	}

/*** CODE: MAYBE IF PENDING, SET TIMER HERE INSTEAD OF IN COMA???.C ***/

	return(retcode);
}	

/* retrieve a received message */
DllExport INT comrecvget(INT refnum, UCHAR *buffer, INT *count)
{
	CHANNEL *ch;

	for (ch = channelheadptr; ch != NULL && ch->refnum != refnum; ch = ch->nextchannelptr);
	if (ch == NULL || *count < 1) return(754);

	if (ch->recvbuf == NULL) return(753);
	if (!(ch->status & (COM_RECV_DONE | COM_RECV_TIME | COM_RECV_ERROR))) return(753);

	if (*count > ch->recvdone) *count = ch->recvdone;
	memcpy(buffer, ch->recvbuf, *count);

	return(0);
}

/* return channel status */
DllExport INT comstat(INT refnum, INT *status)
{
	INT retcode;
	CHANNEL *ch;

	for (ch = channelheadptr; ch != NULL && ch->refnum != refnum; ch = ch->nextchannelptr);
	if (ch == NULL || status == NULL) return(754);

	if (ISCHANNELTYPETCPSERVER(ch)) retcode = os_tcpclear(ch, 0, status);
	else if (ISCHANNELTYPETCPCLIENT(ch)) retcode = os_tcpclear(ch, 0, status);
	else if (ISCHANNELTYPEUDP(ch)) retcode = os_udpclear(ch, 0, status);
	else {
		switch (ch->channeltype) {
		case CHANNELTYPE_SERIAL:
			retcode = os_serclear(ch, 0, status);
			break;
		default:
			retcode = os_comclear(ch, 0, status);
			break;
		}
	}

	return(retcode);
}


/* clear send and receive state */
DllExport INT comclear(INT refnum, INT *status)
{
	INT retcode;
	CHANNEL *ch;
	INT flag = COM_CLEAR_SEND | COM_CLEAR_RECV;

	for (ch = channelheadptr; ch != NULL && ch->refnum != refnum; ch = ch->nextchannelptr);
	if (ch == NULL) return(754);

	if (ISCHANNELTYPETCPSERVER(ch)) retcode = os_tcpclear(ch, flag, status);
	else if (ISCHANNELTYPETCPCLIENT(ch)) retcode = os_tcpclear(ch, flag, status);
	else if (ISCHANNELTYPEUDP(ch)) retcode = os_udpclear(ch, flag, status);
	else {
		switch (ch->channeltype) {
		case CHANNELTYPE_SERIAL:
			retcode = os_serclear(ch, flag, status);
			break;
		default:
			retcode = os_comclear(ch, flag, status);
			break;
		}
	}

	return(retcode);
}

/* clear send state */
DllExport INT comsclear(INT refnum, INT *status)
{
	INT retcode;
	CHANNEL *ch;

	for (ch = channelheadptr; ch != NULL && ch->refnum != refnum; ch = ch->nextchannelptr);
	if (ch == NULL) return(754);

	if (ISCHANNELTYPETCPSERVER(ch)) retcode = os_tcpclear(ch, COM_CLEAR_SEND, status);
	else if (ISCHANNELTYPETCPCLIENT(ch)) retcode = os_tcpclear(ch, COM_CLEAR_SEND, status);
	else if (ISCHANNELTYPEUDP(ch)) retcode = os_udpclear(ch, COM_CLEAR_SEND, status);
	else {
		switch (ch->channeltype) {
		case CHANNELTYPE_SERIAL:
			retcode = os_serclear(ch, COM_CLEAR_SEND, status);
			break;
		default:
			retcode = os_comclear(ch, COM_CLEAR_SEND, status);
			break;
		}
	}

	return(retcode);
}

/* clear receive state */
DllExport INT comrclear(INT refnum, INT *status)
{
	INT retcode;
	CHANNEL *ch;

	for (ch = channelheadptr; ch != NULL && ch->refnum != refnum; ch = ch->nextchannelptr);
	if (ch == NULL) return(754);

	if (ISCHANNELTYPETCPSERVER(ch)) retcode = os_tcpclear(ch, COM_CLEAR_RECV, status);
	else if (ISCHANNELTYPETCPCLIENT(ch)) retcode = os_tcpclear(ch, COM_CLEAR_RECV, status);
	else if (ISCHANNELTYPEUDP(ch)) retcode = os_udpclear(ch, COM_CLEAR_RECV, status);
	else {
		switch (ch->channeltype) {
		case CHANNELTYPE_SERIAL:
			retcode = os_serclear(ch, COM_CLEAR_RECV, status);
			break;
		default:
			retcode = os_comclear(ch, COM_CLEAR_RECV, status);
			break;
		}
	}

	return(retcode);
}

/* control */
DllExport INT comctl(INT refnum, UCHAR *msgin, INT msginlen, UCHAR *msgout, INT *msgoutlen)
{
	INT i1, i2, flag, retcode;
	CHAR work[16], *ptr1, *ptr2;
	CHANNEL *ch;

	/* convert msgin to upper case for geterror comparison */
	for (i1 = 0; i1 < 13 && i1 < msginlen && !isspace(msgin[i1]); i1++) work[i1] = (CHAR) toupper(msgin[i1]);
	work[i1] = '\0';

	for (ch = channelheadptr; ch != NULL && ch->refnum != refnum; ch = ch->nextchannelptr);
	if (ch == NULL) {
		if (strcmp("GETERROR", work)) return(754);
		if (freechannelheadptr == NULL) {
			if (*msgoutlen > 20) *msgoutlen = 20;
			memcpy(msgout, "NO ERROR INFORMATION", *msgoutlen);
			return(0);
		}
		ch = freechannelheadptr;
	}

	if (!strcmp("GETERROR", work)) flag = COM_PER_ERROR;
	else if (!strcmp("GETSENDERROR", work)) flag = COM_SEND_MASK;
	else if (!strcmp("GETRECVERROR", work)) flag = COM_RECV_MASK;
	else flag = 0;
	if (flag) {
		ptr1 = ptr2 = NULL;
		flag |= ch->status & COM_PER_ERROR;
		if (flag & COM_PER_ERROR) {
			if (ch->status & COM_PER_ERROR) ptr1 = "PERMANENT ERROR: ";
			ptr2 = ch->errormsg;
		}
		else {
			i1 = flag & ch->status;
			if (i1 & COM_SEND_ERROR) ptr2 = ch->senderrormsg;
			else if (i1 & COM_RECV_ERROR) ptr2 = ch->recverrormsg;
			else if (i1 & (COM_SEND_TIME | COM_RECV_TIME)) ptr1 = "TIMEOUT";
		}
		i1 = i2 = 0;
		if (ptr1 != NULL) {
			i1 = (INT) strlen(ptr1);
			if (i1 > *msgoutlen) i1 = *msgoutlen;
			memcpy(msgout, ptr1, i1);
		}
		if (ptr2 != NULL) {
			i2 = (INT) strlen(ptr2);
			if (i2 > *msgoutlen - i1) i2 = *msgoutlen - i1;
			memcpy(&msgout[i1], ptr2, i2);
			ptr2[0] = '\0';
		}
		*msgoutlen = i1 + i2;
		return(0);
	}

	if (ISCHANNELTYPETCPSERVER(ch)) retcode = os_tcpctl(ch, msgin, msginlen, msgout, msgoutlen);
	else if (ISCHANNELTYPETCPCLIENT(ch)) retcode = os_tcpctl(ch, msgin, msginlen, msgout, msgoutlen);
	else if (ISCHANNELTYPEUDP(ch)) retcode = os_udpctl(ch, msgin, msginlen, msgout, msgoutlen);
	else {
		switch (ch->channeltype) {
		case CHANNELTYPE_SERIAL:
			retcode = os_serctl(ch, msgin, msginlen, msgout, msgoutlen);
			break;
		default:
			retcode = os_comctl(ch, msgin, msginlen, msgout, msgoutlen);
			break;
		}
	}
	return(retcode);
}

CHANNEL *getchannelhead()
{
	return(channelheadptr);
}
