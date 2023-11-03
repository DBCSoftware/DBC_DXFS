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
#include "dbc.h"

#if OS_UNIX
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifndef __MACOSX
#include <poll.h>
#endif
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

#if OS_WIN32
#define ERRORVALUE() WSAGetLastError()
#else
#define ERRORVALUE() errno
#endif
 
#ifndef INADDR_NONE
#define INADDR_NONE (unsigned long) 0xFFFFFFFF
#endif

#include "release.h"
#include "base.h"
#include "evt.h"
#include "vid.h"
#include "vidx.h"
#include "xml.h"
#include "tcp.h"


/* this module provides a generalized, but very simple, XML over TCP/IP communications interface */
/* limits of this module include the following */
/*  the connection can only take place as a client connecting to a TCP/IP server */
/*  the channel operand of each function allows for a fixed number of channels; only 1 channel is supported now */
/*  there is no support for XML text, comments, namespaces, or anything other than tags and attributes */
/*  an XML element can be only two levels deep at most */
/*  if there is no whitespace in an attribute value, the double quotes are omitted over the wire */
/*  the recv mechanism allows for only 1 element to be recieved at any given time - there is no recv queueing */
/*  a received element must be at most MAXRECVBUFSIZE bytes large in it's wire form */
/*  send is synchronous (only one send can be active) */
/*  the send element must be at most MAXSENDBUFSIZE bytes large in it's wire form */
/*  the only escape characters are &amp;, &quot;, &lt;, &gt;, &#n...; (base 10 only) */

#define MAXRECVBUFSIZE 8000		/* size of each recv buffer */
#define INITSENDBUFSIZE 16000	/* initial size of send buffer */
#define INCRSENDBUFSIZE 8000	/* send buffer increment */

static CHAR* sendbuf;			/* send buffer */
static INT sendbuflen;				/* working length of the send buffer */
static INT sendsubcount;			/* number of sub elements in the send buffer */
static CHAR sendmaintag[32];		/* save the main send tag */
static CHAR recvbuf[MAXRECVBUFSIZE]; /* recv buffer */
static INT recvbuflen;				/* work recv buffer length */
static CHAR *getbufptr;			/* pointer into recv buffer for associated with get functions */
static INT getstate;
static SOCKET sockethandle;		/* handle for the one socket */
static CHAR tcxwork[256];
static void(*callback)(INT);
static INT sendbufsize = INITSENDBUFSIZE;

/* function prototypes */
static void allocateSendbuf(void);
static void movetosendbuf(CHAR *);
static void checkchannel(INT);
static void sendbufoverflow(void);
static void badrecvmsg(INT);

/*
 * Connect to a TCP server socket
 * channel is the dbctcx channel number - now the code only supports channel == 1
 * addrport format is:  <ipaddr>:<port>
 * where <ipaddr> is either a DNS name or a nn.nn.nn.nn format IP address
 * and <port> is the port number
 * whitespace is allowed and is squeezed out of <ipaddr>:<port>
 */
SOCKET dbctcxconnect(INT channel, CHAR *addrport, void (*recvcallback)(INT))
{
	checkchannel(channel);
	sockethandle = tcpconnect(addrport, -1, TCP_CLNT, NULL, 20);
	if (sockethandle == (int) INVALID_SOCKET) return INVALID_SOCKET;
	allocateSendbuf();
	callback = recvcallback;
	return sockethandle;
}

SOCKET dbctcxconnectserver(INT channel, CHAR *port, void (*recvcallback)(INT), int timeout)
{
	SOCKET serversocket;
	int iport;
	int rc = tcpntoi((unsigned char *) port, (int)strlen(port), &iport);

	checkchannel(channel);
	if (rc) return INVALID_SOCKET;
	serversocket = tcplisten(iport, NULL);
	if (serversocket == INVALID_SOCKET) return INVALID_SOCKET;
	sockethandle = tcpaccept(serversocket, 0, NULL, timeout);
	if (sockethandle == (int) INVALID_SOCKET) return INVALID_SOCKET;
	allocateSendbuf();
	callback = recvcallback;
	return sockethandle;
}

static void allocateSendbuf() {
	sendbuf = malloc(sendbufsize);
	if (sendbuf == NULL) {
		tcpseterror("dbctcx: no memory for send buffer");
		dbcdeath(81);
	}
	sendbuf[0] = '<';
	getstate = -1;
}

/* close the socket */
void dbctcxdisconnect(INT channel)
{
	if (channel == 1) closesocket(sockethandle);
	sockethandle = 0;
	free(sendbuf);
}

/* start the send of XML element */
/* channel is the channel number specified in dbctxconnect */
/* tag is the tag name of the element */
void dbctcxputtag(INT channel, CHAR *tag)
{
	checkchannel(channel);
	if (strlen(tag) > 31) {
		tcpseterror("dbctcxputtag: tag too long");
		dbcdeath(81);
	}
	sendbuflen = 1;
	movetosendbuf(tag);
	strcpy(sendmaintag, tag);
	sendsubcount = 0;
}

/* add an attribute/value to the current send element or subelement */
/* channel is the channel number specified in dbctxconnect */
/* attr is the attribute tag, value is the attribute value */
void dbctcxputattr(INT channel, CHAR *attr, CHAR *value)
{
	if (value == NULL || value[0] == '\0') return;
	checkchannel(channel);
	if (sendbuflen + 64 > sendbufsize) sendbufoverflow();
	sendbuf[sendbuflen++] = ' ';
	movetosendbuf(attr);
	sendbuf[sendbuflen++] = '=';
	movetosendbuf(value);
}

/* add a subelement to the current send element */
/* channel is the channel number specified in dbctxconnect */
/* tag is the tag name of the subelement */
void dbctcxputsub(INT channel, CHAR *tag)
{
	checkchannel(channel);
	if (sendbuflen + 32 > sendbufsize) sendbufoverflow();
	if (sendsubcount > 0) sendbuf[sendbuflen++] = '/';
	sendsubcount++;
	sendbuf[sendbuflen++] = '>';
	sendbuf[sendbuflen++] = '<';
	movetosendbuf(tag);
}

/* actually send the element in the send buffer */
/* channel is the channel number specified in dbctxconnect */
void dbctcxput(INT channel)
{
	checkchannel(channel);
	if (sendbuflen + 16 > sendbufsize) sendbufoverflow();
	sendbuf[sendbuflen++] = '/';
	sendbuf[sendbuflen++] = '>';
	if (sendsubcount > 0) {
		sendbuf[sendbuflen++] = '<';
		sendbuf[sendbuflen++] = '/';
		movetosendbuf(sendmaintag);
		sendbuf[sendbuflen++] = '>';
	}
	if (sendbuflen != tcpsend(sockethandle, (unsigned char *)sendbuf, sendbuflen, 0, 10)) {
		tcpcleanup();
		tcpseterror("dbctcxput: unable to send");
		dbcdeath(81);
	}
}

/**
 * Process one character of the input stream
 */
static void processchar(UCHAR c1)
{
	char work[64];
	
	if (getstate >= 0) {
		tcpseterror("processchar: recv overflow (0)");
		dbcdeath(81);
	}
	switch (getstate) {
	case -1:	/* state is waiting for leading '<' */
		if (c1 == '<') {
			recvbuf[0] = '<';
			recvbuflen = 1;
			getstate = -2;
		}
		break;
	case -2:	/* grabbing first character of primary element tag */
		if (!isalpha(c1)) badrecvmsg(getstate);
		recvbuf[recvbuflen++] = c1;
		getstate = -3;
		break;
	case -3:	/* in primary element tag */
		recvbuf[recvbuflen++] = c1;
		if (isalpha(c1)) ;
		else if (c1 == '/') getstate = -30;
		else if (c1 == ' ') getstate = -4;
		else if (c1 == '>') getstate = -10;
		else badrecvmsg(getstate);
		break;
	case -4:	/* looking for first character of primary element attribute */
		if (!isspace(c1)) {
			if (!isalpha(c1)) badrecvmsg(getstate);
			recvbuf[recvbuflen++] = c1;
			getstate = -5;
		}
		break;
	case -5:	/* in primary element attribute tag */
		recvbuf[recvbuflen++] = c1;
		if (isalpha(c1));
		else if (c1 == '=') getstate = -6;
		else badrecvmsg(getstate);
		break;
	case -6:	/* grabbing primary element attribute value first character */
		recvbuf[recvbuflen++] = c1;
		if (c1 == '"') getstate = -7;
		else getstate = -9;
		break;
	case -7:	/* in primary element attribute value looking for quote mark end */
		recvbuf[recvbuflen++] = c1;
		if (c1 == '"') getstate = -8;
		break;
	case -8:	/* looking for first character after quote mark in primary element */
		recvbuf[recvbuflen++] = c1;
		if (c1 == '/') getstate = -30;
		else if (c1 == '>') getstate = -10;
		else if (c1 == ' ') getstate = -4;
		else badrecvmsg(getstate);
		break;
	case -9:	/* in primary element attribute value looking for non-quote mark end */
		recvbuf[recvbuflen++] = c1;
		if (c1 == '/') getstate = -30;
		else if (c1 == '>') getstate = -10;
		else if (c1 == ' ') getstate = -4;
		break;
	case -10:	/* grabbing first character of a secondary element */
		recvbuf[recvbuflen++] = c1;
		if (c1 == '<') getstate = -11;
		else badrecvmsg(getstate);
		break;
	case -11:	/* grabbing first character of subelement tag or '/' */
		recvbuf[recvbuflen++] = c1;
		if (c1 == '/') getstate = -25;
		else if (isalpha(c1)) getstate = -12;
		else badrecvmsg(getstate);
		break;
	case -12:	/* in secondary element tag */
		recvbuf[recvbuflen++] = c1;
		if (isalpha(c1));
		else if (c1 == ' ') getstate = -14;  /* no 13 */
		else if (c1 == '/') getstate = -20;
		else badrecvmsg(getstate);
		break;
	case -14:	/* looking for 1st char of secondary attribute */
		if (!isspace(c1)) {
			if (!isalpha(c1)) badrecvmsg(getstate);
			recvbuf[recvbuflen++] = c1;
			getstate = -15;
		}
		break;
	case -15:	/* In a secondary attribute tag */
		recvbuf[recvbuflen++] = c1;
		if (isalpha(c1));
		else if (c1 == '=') getstate = -16;
		else badrecvmsg(getstate);
		break;
	case -16:	/* grabbing secondary element attribute value first character */
		recvbuf[recvbuflen++] = c1;
		if (c1 == '"') getstate = -17;
		else getstate = -19;
		break;
	case -17: /* in secondary element attribute value looking for quote mark end */
		recvbuf[recvbuflen++] = c1;
		if (c1 == '"') getstate = -18;
		break;
	case -18: /* looking for first character after quote mark in secondary element */
		recvbuf[recvbuflen++] = c1;
		if (c1 == '/') getstate = -20;
		else if (c1 == ' ') getstate = -14;
		else badrecvmsg(getstate);
		break;
	case -19: /* in secondary element attribute value looking for non-quote mark end */
		recvbuf[recvbuflen++] = c1;
		if (c1 == '/') getstate = -20;
		else if (c1 == ' ') getstate = -14;
		else if (c1 == '>') badrecvmsg(getstate);
		break;
	case -20:	/* secondary tag scanning saw /, can't be any attributes, c1 must be > */
		recvbuf[recvbuflen++] = c1;
		if (c1 != '>') badrecvmsg(getstate);
		getstate = -10;
		break;
	case -25:	/* we saw ></ so we must be closing the primary */
		recvbuf[recvbuflen++] = c1;
		if (c1 == '>') {
			getstate = 0;
			recvbuf[recvbuflen--] = '\0';
		}
		break;
	case -30:	/* we were scanning primary and saw a /, next char must be > */
		if (c1 == '>') recvbuf[recvbuflen++] = c1;
		else badrecvmsg(getstate);
		recvbuf[recvbuflen] = '\0';
		getstate = 0;
		break;
	default:
		sprintf(work, "processchar: recv overflow (%i)", getstate);
		tcpseterror(work);
		dbcdeath(81);
	}
	if (getstate == 0 && callback != NULL) callback(0);
}

/* this function is called whenever 1 or more bytes */
/* have been received by the receiving mechanism */
/* mem points to the bytes and len is the number of bytes */
void dbctcxprocessbytes(UCHAR *mem, INT len) {
	INT i1;
	for (i1 = 0; i1 < len; i1++)
		processchar(mem[i1]);
	return;
}

/**
 * Test for an xml document for processing.
 * 
 * Channel is the channel number specified in dbctxconnect.
 * 
 * If getstate is >0 then we are in the middle of scanning an
 * xml document, in that case, calling this routine is an error.
 * 
 * If getstate <0 then we either have nothing at all or we are in the middle
 * of receiveing the characters of an xml document.
 * 
 * if the value returned zero, then no element is available
 * if the value returned is one, then an element has been received and is available
 */
INT dbctcxget(INT channel)
{
	checkchannel(channel);
	if (getstate > 0) {
		sprintf(tcxwork, "dbctcxget: bad state:%i", getstate);
		tcpseterror(tcxwork);
		dbcdeath(81);
	}
 	return getstate == 0;
}

void dbctcxflushincoming(INT channel)
{
	checkchannel(channel);
	getstate = -1;
	getbufptr = NULL;
	return;
}

/**
 * Get the tag of the primary element.
 * This function is only valid when getstate is zero.
 * 
 * After scanning the primary tag...
 * If we see a space, then there are attributes and getstate = 1.
 * If we see / then there can't be anything else and getstate = -1.
 * If we see > then there are no attributes but there may be children,
 *     getstate = 3.
 */
CHAR *dbctcxgettag(INT channel)
{
	checkchannel(channel);
	if (getstate != 0) {
		tcpseterror("dbctcxgettag: bad state");
		dbcdeath(81);
	}
	getbufptr = strpbrk(&recvbuf[1], " />");
	if (*getbufptr == '/') getstate = -1;		/* That's all folks */
	else if (*getbufptr == '>') getstate = 3;	/* There are sub-elements */
	else getstate = 1;							/* There are attributes */
	*getbufptr++ = '\0';
	return(&recvbuf[1]);
}

/**
 * Find the next attribute and return its tag, return NULL if no more to be found.
 * 
 * If getstate is one on entry, a space was seen and an attribute tag is expected.
 * In that case we exit with getstate=2, meaning that an attribute value
 * needs to be scanned.
 *
 * If getstate is -1 on entry, we have finished scanning a complete xml document
 * and nothing new has come in yet.
 * There is nothing to return. We return NULL.
 *  
 * If getstate is zero on entry, then a complete xml doc has come in and
 * we have not started scanning it. That is an error.
 * 
 * If getstate is one on entry, we expect an attribute, scan the tag,
 * set getstate=2 and return the tag.
 * 
 * If getstate is two on entry that means we have scanned an attribute tag
 * in the primary element and should have scanned it's value, that is an error.
 * 
 * If getstate is three on entry, we have just scanned an attribute value
 * and hit the end of the element, there are no more, return NULL.
 * 
 */
CHAR *dbctcxgetnextattr(INT channel)
{
	CHAR *p1, work[64];

	checkchannel(channel);
	if (getstate == 0 || getstate == 2 || getstate == 5) {
		sprintf(work, "dbctcxgetnextattr: bad state (%i)", getstate);
		tcpseterror(work);
		dbcdeath(81);
	}
	if (getstate == 3 || getstate == -1) return(NULL);
	while (*getbufptr <= ' ') getbufptr++;
	p1 = getbufptr;
	getbufptr = strchr(p1, '=');
	*getbufptr++ = '\0';
	getstate++;
	return(p1);
}

/* return the value associated with the current attribute, return NULL if none */
CHAR *dbctcxgetattrvalue(INT channel)
{
	INT n1, quoteflag;
	CHAR c1, *p1, *p2, *value;

	checkchannel(channel);
	if (getstate != 2 && getstate != 5) {
		tcpseterror("dbctcxgetattrvalue: bad state");
		dbcdeath(81);
	}
	while (*getbufptr <= ' ') getbufptr++;
	value = p1 = p2 = getbufptr;
	if (*p2 == '"') {
		p2++;
		quoteflag = TRUE;
	}
	else quoteflag = FALSE;
	while (TRUE) {
		c1 = *p2++;
		if (quoteflag && c1 == '"') {
			c1 = *p2++;
			break;
		}
		else if (c1 < ' ' || c1 == '/' || c1 == '>') break;
		else if (c1 == ' ' && !quoteflag) break;
		if (c1 == '&') {
			if (!strcmp(p2, "amp;")) {
				p2 += 4;
				c1 = '&';
			}
			else if (!strcmp(p2, "quot;")) {
				p2 += 5;
				c1 = '"';
			}
			else if (!strcmp(p2, "lt;")) {
				p2 += 3;
				c1 = '<';
			}
			else if (!strcmp(p2, "gt;")) {
				p2 += 3;
				c1 = '>';
			}
			else if (*p2 == '#') {
				n1 = 0;
				for (p2++; *p2 != ';'; p2++) n1 = n1 * 10 + *p2 - '0';
				c1 = (CHAR) n1;
				p2++;
			}
			else {
				tcpseterror("dbctcxgetattrvalue: invalid escape");
				dbcdeath(81);
			}
		}
		*p1++ = c1;
	}
	while (*p2 <= ' ') p2++;
	/* If we see a slash, what does that mean? */
	/* If getstate was 2 then we were in the primary element */
	/* and so we are done, set getstate to -1 */
	/* If getstate was 5 then we were in a secondary element and it is done. */
	/* set the state to look for any more secondary elements */
	if (c1 == '/') {
		if (getstate == 2) {getstate = -1; getbufptr = NULL;}
		else {
			getstate = 3;
			getbufptr = ++p2;	/* should be pointing at '<' */
		}
	}
	/* If we see a >, what then? */
	/* This should only happen when scanning a primary attribute value */
	/* If this happens in a secondary element, we are in deep doodoo */
	/* There are no more primary attributes but there might be sub-elements */
	else if (c1 == '>') {
		getstate = 3;
		getbufptr = p2;
	}
	/* We saw a space, there are more attributes */
	/* set getstate to 1 or 4 */
	else {
		getstate--;
		getbufptr = p2;
	}
	while (p1 < p2) *p1++ = '\0';
	return(value);
}

/* skip to the next subelement and return the tag */
/* if there is no next subelement, return NULL */
CHAR *dbctcxgetnextchild(INT channel)
{
	CHAR *tag, work[64];

	checkchannel(channel);
	if (getstate == -1) {
		getbufptr = NULL;
		return(NULL);
	}
	if (getstate != 3 || *getbufptr++ != '<') {
		sprintf(work, "dbctcxgetnextchild: bad state (%i,'%s')", getstate, getbufptr);
		tcpseterror(work);
		dbcdeath(81);
	}
	if (*getbufptr == '/') {
		getstate = -1;
		getbufptr = NULL;
		return(NULL);
	}
	/* We have another secondary element */
	tag = getbufptr;
	getbufptr = strpbrk(getbufptr, " />");
	/* If we see a slash after the tag, we have no attributes */
	/* We might have more secondary elements */
	/* So we leave getstate = 3 */
	if (*getbufptr == '/') {
		*getbufptr++ = '\0';
		getbufptr++;			/* point at the next < */
	}
	else getstate = 4;
	*getbufptr++ = '\0';
	return(tag);
}

/* move the zero delimited string to the send buffer expanding special characters and handling whitespace */
static void movetosendbuf(CHAR *text)
{
	INT n1, oldlen, srclen, srcindex, quoteflag;
	UCHAR c1;
	CHAR badchars[] = "\\,.@/=|?-()*:";

	oldlen = sendbuflen;
	srclen = (INT)strlen(text);
	if (srclen < 1) {
		sendbuf[sendbuflen++] = '"';
		quoteflag = TRUE;
	}
	else quoteflag = FALSE;
movestart:
	for (srcindex = 0; srcindex < srclen; srcindex++) {
		if (sendbuflen + 6 > sendbufsize) sendbufoverflow();
		c1 = text[srcindex];
		if (!quoteflag && (c1 <= ' ' || c1 > '~' || strchr(badchars, c1) != NULL))
		{
			sendbuflen = oldlen;
			sendbuf[sendbuflen++] = '"';
			quoteflag = TRUE;
			goto movestart;
		}
		if (c1 == '"') {
			memcpy(sendbuf + sendbuflen, "&quot;", 6);
			sendbuflen += 6;
		}
		else if (c1 == '<') {
			memcpy(sendbuf + sendbuflen, "&lt;", 4);
			sendbuflen += 4;
		}
		else if (c1 == '>') {
			memcpy(sendbuf + sendbuflen, "&gt;", 4);
			sendbuflen += 4;
		}
		else if (c1 == '&') {
			memcpy(sendbuf + sendbuflen, "&amp;", 5);
			sendbuflen += 5;
		}
		else if (c1 < ' ' || c1 > '~') {
			sendbuf[sendbuflen++] = '&';
			sendbuf[sendbuflen++] = '#';
			n1 = c1;
			if (n1 > 99) sendbuf[sendbuflen++] = (char) (n1 / 100 + '0');
			if (n1 > 9) sendbuf[sendbuflen++] = (char) ((n1 / 10) % 10 + '0');
			sendbuf[sendbuflen++] = (char) ((n1 % 10) + '0');
			sendbuf[sendbuflen++] = ';';
		}
		else sendbuf[sendbuflen++] = c1;
	}
	if (quoteflag) sendbuf[sendbuflen++] = '"';
	if (sendbuflen > sendbufsize) sendbufoverflow();
}

static void checkchannel(INT channel)
{
	if (channel != 1) {
		tcpseterror("dbctcx: invalid channel");
		dbcdeath(81);
	}
}

/*
 * Attempt to get more memory
 */
static void sendbufoverflow()
{
	sendbufsize += INCRSENDBUFSIZE;
	sendbuf = realloc(sendbuf, sendbufsize);
	if (sendbuf == NULL) {
		tcpseterror("dbctcx: send buffer overflow");
		dbcdeath(81);
	}
}

static void badrecvmsg(INT state)
{
	char work[64];
	sprintf(work, "dbctcx: bad recv msg, state=%i, len=%i", state, recvbuflen);
	tcpseterror(work);
	dbcdeath(81);
}
