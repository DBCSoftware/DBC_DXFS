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
#define INC_TYPES
#define INC_TIME
#define INC_LIMITS
#define INC_ERRNO
#define INC_IPV6
#include "includes.h"

#define COMDLL

#include "com.h"
#include "comx.h"
#include "fio.h"
#include "base.h"
#include "release.h"

#include <malloc.h>
#include <process.h>
#include <strsafe.h>

#ifndef SD_BOTH
#define SD_BOTH 0x02
#endif

/* Flags for use by comctl */
#define COMCTL_SPEC_R 0x00000001
#define COMCTL_SPEC_r 0x00000002
#define COMCTL_SPEC_T 0x00000004
#define COMCTL_SPEC_t 0x00000008
#define COMCTL_SPEC_C 0x00000010
#define COMCTL_SPEC_c 0x00000020
#define COMCTL_SPEC_S 0x00000040
#define COMCTL_SPEC_s 0x00000080
#define COMCTL_SPEC_X 0x00000100
#define COMCTL_SPEC_x 0x00000200
#define COMCTL_SPEC_L 0x00000400
#define COMCTL_SPEC_l 0x00000800
#define COMCTL_FLOWS  0x000003f0

static INT (*evtcreate)(void);
static void (*evtdestroy)(INT);
static void (*evtset)(INT);
static void (*evtclear)(INT);
static INT (*evttest)(INT);
static INT (*evtwait)(INT*, INT);
static void (*evtstartcritical)(void);
static void (*evtendcritical)(void);
static INT (*timset)(UCHAR *, INT);
static void (*timstop)(INT);
static INT (*timadd)(UCHAR *, INT);
static void (*pmsctimestamp)(UCHAR *);
static ELEMENT* (*cfggetxml)(void);

/* local variables */
//static OSVERSIONINFO osInfo;
static HINSTANCE hinst;
static HWND winhnd;
static INT winevent;
static CHAR winclass[] = "SOCKCLASS";
static INT udptcpinitflg;
static HANDLE udptcpthreadhandle;
static CHANNEL **channeltable;
static INT channeltablesize;
static HANDLE ghTCPClientConnect;
//static BOOL waitForTCPConnect = TRUE;
//static INT allowIPv6;	/* Can we use IPv6 on this Windows version? */


static void comerror(CHANNEL *, INT, CHAR *, INT);
static INT getchanneltableentry(CHANNEL *);
static INT getAddrAndPortStrings(CHANNEL *channel, SOCKADDR_STORAGE* sockstore, CHAR *addrBuf,
		INT cchAddrBufLen, CHAR *portBuf, INT cchPortBufLen);
static INT ipstrtoaddr(CHAR *channelname, UINT *address);
static INT setSendAddressInChannel(CHAR *, CHANNEL *);
static INT tcpclientopen_V6(CHANNEL *channel, CHAR *address, CHAR *service);
static INT tcpclientopen_V4(CHANNEL *channel, UINT sendaddr, PORT sendport);
static INT tcprecv(CHANNEL *);
static INT tcpsend(CHANNEL *);
static INT tcpserveraccept(CHANNEL *);
static INT tcpserveropen_V6(CHAR *channelname, CHANNEL *channel, INT found);
static INT tcpserveropen_V4(CHANNEL *channel, INT found);
static INT tcpserveropen_common(CHAR *channelname, CHANNEL *channel, INT *found);
static INT udptcpinit(CHANNEL *);
static void udptcpthread(void*);
LRESULT CALLBACK udptcpcallback(HWND, UINT, WPARAM, LPARAM);

typedef PCSTR (WSAAPI *_OS_INETNTOP) (INT Family, PVOID pAddr, PSTR pStringBuf, size_t StringBufSize);
static _OS_INETNTOP os_InetNtop = NULL;

BOOL WINAPI DllMain(HINSTANCE dllinst, DWORD reason, LPVOID notused)
{
	WNDCLASS class;
	HMODULE ws2_32lib;		/* Pointer to the ws2_32 library */

	switch (reason) {
		case DLL_PROCESS_ATTACH:
			hinst = dllinst;
			winhnd = NULL;

			ws2_32lib = LoadLibrary("Ws2_32.dll");
			if (ws2_32lib != NULL) {
				os_InetNtop = (_OS_INETNTOP)GetProcAddress(ws2_32lib, "inet_ntop");
				FreeLibrary(ws2_32lib);
			}

			class.style = 0;
			class.lpfnWndProc = (LPVOID) udptcpcallback;
			class.cbClsExtra = 0;
			class.cbWndExtra = 0;
			class.hInstance = hinst;
			class.hIcon = NULL;
			class.hCursor = NULL;
			class.hbrBackground = NULL;
			class.lpszMenuName = NULL;
			class.lpszClassName = (LPCSTR) winclass;
			RegisterClass(&class);
			break;

		case DLL_PROCESS_DETACH:
			UnregisterClass((LPCSTR) winclass, hinst);
			/**
			 * Do not call WSACleanup() here. This module applies to user TCP and UDP comms.
			 * There is quite a lot of TCP code in other modules for other purposes
			 * that uses WinSock
			 */
			break;
		default:
			break;
	}
	return(TRUE);
}

void dllinit(DLLSTRUCT *dllinitstr)
{

	evtcreate = dllinitstr->evtcreate;
	evtdestroy = dllinitstr->evtdestroy;
	evtset = dllinitstr->evtset;
	evtclear = dllinitstr->evtclear;
	evttest = dllinitstr->evttest;
	evtwait = dllinitstr->evtwait;
	evtstartcritical = dllinitstr->evtstartcritical;
	evtendcritical = dllinitstr->evtendcritical;
	timset = dllinitstr->timset;
	timstop = dllinitstr->timstop;
	timadd = dllinitstr->timadd;
	pmsctimestamp = dllinitstr->msctimestamp;
	cfggetxml = dllinitstr->cfggetxml;
	prpinit(cfggetxml(), CFG_PREFIX "cfg");
}

/* general communication routines */

/*
 * Is this ever called? I don't think so.
 */
INT os_cominit()
{
	return(0);
}

INT os_comopen(CHAR *channelname, CHANNEL *channel)
{
#pragma warning(suppress:4996)
	if (!_memicmp(channelname, "COM", 3)) {  /* backward compatibility support */
		channel->channeltype = CHANNELTYPE_SERIAL;
		return(os_seropen(channelname, channel));
	}
	comerror(channel, 0, "UNSUPPORTED DEVICE", 0);
	return(753);
}

void os_comclose(CHANNEL *channel)
{
}

INT os_comsend(CHANNEL *channel)
{
	comerror(channel, 0, "UNSUPPORTED DEVICE", 0);
	return(753);
}

INT os_comrecv(CHANNEL *channel)
{
	comerror(channel, 0, "UNSUPPORTED DEVICE", 0);
	return(753);
}

INT os_comclear(CHANNEL *channel, INT flag, INT *status)
{
	comerror(channel, 0, "UNSUPPORTED DEVICE", 0);
	return(753);
}

INT os_comctl(CHANNEL *channel, UCHAR *msgin, INT msginlen, UCHAR *msgout, INT *msgoutlen)
{
	comerror(channel, 0, "UNSUPPORTED DEVICE", 0);
	return(753);
}

/* serial communication routines */

#define COM_STATE_START		0x0001
#define COM_STATE_END		0x0002

static void sercallback(CHANNEL *);
static void serreadprocess(CHANNEL *);
static INT serparse(CHANNEL *, CHAR *);
static INT sergetparm(UCHAR *, CHAR *);
static INT serregister(HANDLE, void (*)(CHANNEL *), CHANNEL *);
static void serunregister(HANDLE);

INT os_seropen(CHAR *channelname, CHANNEL *channel)
{
	INT retcode;
	UINT i1;
	COMMTIMEOUTS commtimout;

	for (i1 = 0; channelname[i1] && !isspace(channelname[i1]) && channelname[i1] != ':' &&
	     channelname[i1] != ',' && channelname[i1] != ';'; i1++);

/*** CODE: DO WE NEED CHANNEL->NAME ??? ***/
/***       DO WE COMPARE TO OTHER FILES ALREADY OPENED BY OTHER CHANNELS. ***/
	if (i1 >= sizeof(channel->name)) {
		comerror(channel, 0, "DEVICE NAME TOO LONG", 0);
		return(753);
	}
	memcpy(channel->name, channelname, i1);
	channel->name[i1] = '\0';
	channelname += i1;

	/* set default parameters */
	channel->recvend[0] = 1;
	channel->recvend[1] = 0x0D;		/* ^M */
	channel->sendend[0] = 2;
	channel->sendend[1] = 0x0D;		/* ^M^J */
	channel->sendend[2] = 0x0A;
	channel->recvignore[0] = 1;
	channel->recvignore[1] = 0x0A;	/* ^J */

	/* get parameters */
	retcode = serparse(channel, channelname);
	if (retcode < 0) {
		comerror(channel, 0, "INVALID PARAMETER STRING", 0);
		return(753);
	}

/*** CODE: MAYBE ADD CODE TO TIME OUT IF DEVICE OFF LINE ***/
	channel->handle = CreateFile(channel->name, GENERIC_READ | GENERIC_WRITE,
						0, NULL, OPEN_EXISTING,
						FILE_FLAG_OVERLAPPED, NULL);
	if (channel->handle == INVALID_HANDLE_VALUE) {
		retcode = GetLastError();
		if (retcode == ERROR_ACCESS_DENIED) {
			Sleep(500);
			channel->handle = CreateFile(channel->name, GENERIC_READ | GENERIC_WRITE,
						0, NULL, OPEN_EXISTING,
						FILE_FLAG_OVERLAPPED, NULL);
		}
		if (channel->handle == INVALID_HANDLE_VALUE) {
/*** CODE: ADD SUPPORT FOR EMFILE && ENFILE ***/
			comerror(channel, 0, "CREATEFILE FAILURE", GetLastError());
			return(753);
		}
	}
	channel->flags |= COM_FLAGS_OPEN;

	retcode = SetupComm(channel->handle, 8192, 8192);
	if (!retcode) {
		comerror(channel, 0, "SETUPCOM FAILURE", GetLastError());
		os_serclose(channel);
		return(753);
	}

	channel->dcb.DCBlength = sizeof(DCB);
	retcode = GetCommState(channel->handle, (LPDCB) &channel->dcb);
	if (!retcode) {
		comerror(channel, 0, "GETCOMMSTATE FAILURE", GetLastError());
		os_serclose(channel);
		return(753);
	}

	/* set line setttings */
	if (channel->baud) channel->dcb.BaudRate = channel->baud;
	if (channel->parity == 'N') {
		channel->dcb.fParity = FALSE;
		channel->dcb.Parity = NOPARITY;
	}
	else if (channel->parity == 'E') {
		channel->dcb.fParity = TRUE;
		channel->dcb.Parity = EVENPARITY;
	}
	else if (channel->parity == 'O') {
		channel->dcb.fParity = TRUE;
		channel->dcb.Parity = ODDPARITY;
	}
	else if (channel->parity == 'M') {
		channel->dcb.fParity = TRUE;
		channel->dcb.Parity = MARKPARITY;
	}
	if (channel->databits >= 5 && channel->databits <= 8)
		channel->dcb.ByteSize = channel->databits;

	if (channel->stopbits == 1) channel->dcb.StopBits = ONESTOPBIT;
	else if (channel->stopbits == 2) {
		if (channel->databits != 5) channel->dcb.StopBits = TWOSTOPBITS;
		else channel->dcb.StopBits = ONE5STOPBITS;
	}

	/* set line control setttings */
	channel->dcb.fNull = FALSE;
	channel->dcb.fOutxCtsFlow = FALSE;
	channel->dcb.fOutxDsrFlow = FALSE; //TRUE;
	channel->dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
	channel->dcb.fRtsControl = RTS_CONTROL_ENABLE;
/*** CODE: IF FOLLOWING IS TRUE, THEN CHARACTERS ARE IGNORED IF DSR IS NOT SET ***/
	channel->dcb.fDsrSensitivity = FALSE; //TRUE;
	channel->dcb.fOutX = FALSE;
	channel->dcb.fInX = FALSE;
	channel->dcb.XonLim = 2048;
	channel->dcb.XoffLim = 2048;
	channel->dcb.fAbortOnError = FALSE;

	retcode = SetCommState(channel->handle, (LPDCB) &channel->dcb);
	if (!retcode) {
		comerror(channel, 0, "SETCOMMSTATE FAILURE", GetLastError());
		os_serclose(channel);
		return(753);
	}

/*** CODE: SAVE OLD TIMEOUT VALUES TO RESTORE WHEN CLOSING ***/

	/* write timeout will be infinite, read timeout immediately */
	ZeroMemory(&commtimout, sizeof(COMMTIMEOUTS));
	commtimout.ReadIntervalTimeout = MAXDWORD;
/*** CODE: USE FOLLOWING CODE IF WANT TO CAUSE ALMOST INFINITE TIMEOUT FOR FIRST ***/
/***       CHARACTER, BUT IMMEDIATE TIMEOUT FOR ADDITIONAL CHARACTERS ***/
//	commtimout.ReadTotalTimeoutMultiplier = MAXDWORD;
//	commtimout.ReadTotalTimeoutConstant = MAXDWORD - 1;
	retcode = SetCommTimeouts(channel->handle, &commtimout);
	if (!retcode) {
		comerror(channel, 0, "SETCOMMTIMEOUTS FAILURE", GetLastError());
		os_serclose(channel);
		return(753);
	}

	return(0);
}

void os_serclose(CHANNEL *channel)
{
	if (channel->flags & COM_SEND_MASK) os_serclear(channel, COM_CLEAR_SEND, NULL);
	if (channel->flags & COM_FLAGS_SENDREG) serunregister(channel->sendoverlap.hEvent);
	if (channel->sendoverlap.hEvent != NULL) CloseHandle(channel->sendoverlap.hEvent);

	if (channel->flags & COM_RECV_MASK) os_serclear(channel, COM_CLEAR_RECV, NULL);
	if (channel->flags & COM_FLAGS_RECVREG) serunregister(channel->recvoverlap.hEvent);
	if (channel->recvoverlap.hEvent != NULL) CloseHandle(channel->recvoverlap.hEvent);

	if (channel->flags & COM_FLAGS_OPEN) CloseHandle(channel->handle);
	channel->flags &= ~COM_FLAGS_OPEN;
}

INT os_sersend(CHANNEL *channel)
{
	INT retcode, timehandle;
	DWORD nbytes;
	UCHAR timestamp[16];

	if (channel->status & COM_SEND_MASK) os_serclear(channel, COM_CLEAR_SEND, NULL);

	if (channel->status & COM_PER_ERROR) return(753);

	if (channel->sendoverlap.hEvent == NULL) {
		channel->sendoverlap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (channel->sendoverlap.hEvent == NULL) {
			comerror(channel, 0, "CREATEEVENT FAILURE", GetLastError());
			return(753);
		}
	}

	channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_PEND;

	retcode = WriteFile(channel->handle, channel->sendbuf, channel->sendlength, &channel->sendbytes, &channel->sendoverlap);
	if (retcode && (INT) channel->sendbytes >= channel->sendlength) {  /* success */
		channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_DONE;
		evtset(channel->sendevtid);
		return(0);
	}
	if (!retcode) {
		if (GetLastError() != ERROR_IO_PENDING) {
			comerror(channel, COM_SEND_ERROR, "WRITEFILE FAILURE", GetLastError());
			channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_ERROR;
			evtset(channel->sendevtid);
			return(0);
		}
		channel->flags |= COM_FLAGS_SENDWAIT;
		nbytes = 0;
	}
/*** CODE: SYNCRONOUS RETURN (SHOULD NOT HAPPEN), SHOULD WE LOOP BACK UP UNTIL DONE, OR ASYNCRONOUS ***/
	else nbytes = channel->sendbytes;
	channel->senddone = nbytes;

/*** CODE: MAYBE MOVE TIMEOUT CODE TO COM.C ***/
	if (channel->sendtimeout == 0) {
/*** CODE: MAYBE DO A TEST TO SEE IF WRITEFILE HAS COMPLETED ***/
		PurgeComm(channel->handle, PURGE_TXABORT);
		channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_TIME;
		evtset(channel->sendevtid);
		return(0);
	}
	if (channel->sendtimeout > 0) {
		pmsctimestamp(timestamp);
		if (timadd(timestamp, channel->sendtimeout * 100) ||
		   (timehandle = timset(timestamp, channel->sendevtid)) < 0) {
			comerror(channel, 0, "SET TIMER FAILURE", 0);
			PurgeComm(channel->handle, PURGE_TXABORT);
			channel->status &= ~COM_SEND_MASK;
			return(753);
		}
		if (!timehandle) {
/*** CODE: MAYBE DO A TEST TO SEE IF WRITEFILE HAS COMPLETED ***/
			PurgeComm(channel->handle, PURGE_TXABORT);
			channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_TIME;
			evtset(channel->sendevtid);
			return(0);
		}
		channel->sendth = timehandle;
	}

	if (!(channel->flags & COM_FLAGS_SENDREG)) {
		if (serregister(channel->sendoverlap.hEvent, sercallback, channel)) {
			/* comerror called by serregister */
			PurgeComm(channel->handle, PURGE_TXABORT);
			channel->status &= ~COM_SEND_MASK;
			return(753);
		}
		channel->flags |= COM_FLAGS_SENDREG;
	}

	return(0);
}

INT os_serrecv(CHANNEL *channel)
{
	INT timehandle;
	UCHAR timestamp[16];

	if (channel->status & COM_RECV_MASK) os_serclear(channel, COM_CLEAR_RECV, NULL);

	if (channel->status & COM_PER_ERROR) return(753);

	if (channel->recvoverlap.hEvent == NULL) {
		channel->recvoverlap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (channel->recvoverlap.hEvent == NULL) {
			comerror(channel, 0, "CREATEEVENT FAILURE", GetLastError());
			return(753);
		}
	}

	if (channel->recvstart[0]) channel->recvstate = COM_STATE_START;
	else channel->recvstate = COM_STATE_END;

	channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_PEND;

	sercallback(channel);
	if (!(channel->status & COM_RECV_PEND)) return(0);

/*** CODE: MAYBE MOVE TIMEOUT CODE TO COM.C ***/
	if (channel->recvtimeout == 0) {
		channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_TIME;
		evtset(channel->recvevtid);
		return(0);
	}
	if (channel->recvtimeout > 0) {
		pmsctimestamp(timestamp);
		if (timadd(timestamp, channel->recvtimeout * 100) ||
		   (timehandle = timset(timestamp, channel->recvevtid)) < 0) {
			comerror(channel, 0, "SET TIMER FAILURE", 0);
			channel->status &= ~COM_RECV_MASK;
			return(753);
		}
		if (!timehandle) {
			channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_TIME;
			evtset(channel->recvevtid);
			return(0);
		}
		channel->recvth = timehandle;
	}

	if (!(channel->flags & COM_FLAGS_RECVREG)) {
		if (serregister(channel->recvoverlap.hEvent, sercallback, channel)) {
			/* comerror called by serregister */
			channel->status &= ~COM_RECV_MASK;
			return(753);
		}
		channel->flags |= COM_FLAGS_RECVREG;
	}

	return(0);
}

INT os_serclear(CHANNEL *channel, INT flag, INT *status)
{
	evtstartcritical();

	if (channel->status & COM_SEND_PEND) {
		if (channel->sendtimeout > 0 && evttest(channel->sendevtid)) {  /* timeout happened */
			channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_TIME;
		}
	}
	if (channel->status & COM_RECV_PEND) {
		if (channel->recvtimeout > 0 && evttest(channel->recvevtid)) {  /* timeout happened */
			channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_TIME;
		}
	}

	if (status != NULL) *status = channel->status;

	if (flag & COM_CLEAR_SEND) {
		if (channel->status & (COM_SEND_PEND | COM_SEND_TIME | COM_SEND_ERROR))
			PurgeComm(channel->handle, PURGE_TXABORT);
		channel->status &= ~COM_SEND_MASK;
		if (channel->sendth) {
			timstop(channel->sendth);
			channel->sendth = 0;
		}
		evtclear(channel->sendevtid);
	}
	if (flag & COM_CLEAR_RECV) {
		if (channel->status & (COM_RECV_PEND | COM_RECV_TIME | COM_RECV_ERROR))
			SetCommMask(channel->handle, 0);
		channel->status &= ~COM_RECV_MASK;
		if (channel->recvth) {
			timstop(channel->recvth);
			channel->recvth = 0;
		}
		evtclear(channel->recvevtid);
	}

/*** CODE: THIS TURNING OFF OF CALL BACK IS REALLY ONLY ***/
/***       NEEDED IN THE CALL BACK ROUTINE ***/
	if ((channel->flags & COM_FLAGS_SENDREG) && !(channel->status & COM_SEND_PEND)) {
		serunregister(channel->sendoverlap.hEvent);
		channel->flags &= ~COM_FLAGS_SENDREG;
	}
	if ((channel->flags & COM_FLAGS_RECVREG) && !(channel->status & COM_RECV_PEND)) {
		serunregister(channel->recvoverlap.hEvent);
		channel->flags &= ~COM_FLAGS_RECVREG;
	}

	evtendcritical();
	return(0);
}

INT os_serctl(CHANNEL *channel, UCHAR *msgin, INT msginlen, UCHAR *msgout, INT *msgoutlen)
{
	INT i1, i2, ctlflag, dcbflag, len, retcode, value;
	DWORD status;
	CHAR work[256], *msgptr;
	UCHAR newstartend[MAXSTARTEND], *ptr;

	while (msginlen > 0 && isspace(msgin[msginlen - 1])) msginlen--;
	if (msginlen >= (INT) sizeof(work)) msginlen = sizeof(work) - 1;
	for (i1 = 0; i1 < msginlen && msgin[i1] != '='; i1++)
		work[i1] = (CHAR) toupper(msgin[i1]);
	work[i1] = 0;
	if (i1++ < msginlen) {  /* has '=', assume keyword */
		if (!strcmp(work, "OUTSTART")) ptr = channel->sendstart;
		else if (!strcmp(work, "OUTEND")) ptr = channel->sendend;
		else {
			if (!strcmp(work, "INSTART")) ptr = channel->recvstart;
			else if (!strcmp(work, "INEND")) ptr = channel->recvend;
			else if (!strcmp(work, "IGNORE")) ptr = channel->recvignore;
			else if (!strcmp(work, "LENGTH")) ptr = NULL;
			else {
				comerror(channel, 0, "INVALID COMCTL REQUEST", 0);
				return(753);
			}
			if (channel->status & COM_RECV_PEND) {
				comerror(channel, 0, "COMCTL INVALID DURING PENDING RECEIVE", 0);
				return(753);
			}
		}
		if (ptr != NULL) {  /* start/end/ignore string */
			memcpy(work, msgin + i1, msginlen - i1);
			work[msginlen - i1] = 0;
			if (sergetparm(newstartend, work) < 0) {
				comerror(channel, 0, "INVALID COMCTL CHARACTER SEQUENCE", 0);
				return(753);
			}
			for (i2 = len = 0; i2++ < (INT) ptr[0]; ) {
				value = ptr[i2];
				if (value >= 32 && value <= 126) {
					if (value == '\\' || value == '^') work[len++] = '\\';
					work[len++] = value;
				}
				else if (value >= 1 && value <= 31) {
					work[len++] = '^';
					work[len++] = (CHAR)(value - 1 + 'A');
				}
				else {
					work[len++] = '\\';
					work[len++] = (CHAR)(value / 100 + '0');
					work[len++] = (CHAR)((value % 100) / 10 + '0');
					work[len++] = (CHAR)(value / 100 + '0');
				}
			}
			memcpy(ptr, newstartend, MAXSTARTEND);
		}
		else {  /* length */
			for (len = i1, value = 0; len < msginlen; len++)
				if (isdigit(msgin[len])) value = value * 10 + msgin[len] - '0';
			i2 = channel->recvlimit;
			channel->recvlimit = value;
			len = 32;
			do work[--len] = (CHAR)(i2 % 10 + '0');
			while (i2 /= 10);
			memcpy(work, &work[len], 32 - len);
			len = 32 - len;
		}
		if (*msgoutlen >= i1 + len) {
			memmove(msgout, msgin, i1);
			memcpy(&msgout[i1], work, len);
			*msgoutlen = i1 + len;
		}
		else *msgoutlen = 0;
		return(0);
	}

/*** CODE: CANCEL ANY PENDING SENDS OR RECEIVES ? ***/
	msgptr = (CHAR *) msgin;
	if (*msgoutlen < 4) {
		comerror(channel, 0, "VARIABLE TOO SHORT", 0);
		return(753);
	}

	ctlflag = 0;
	while (msgptr < (CHAR *) msgin + msginlen) {
		switch (*msgptr++) {
		case 'R':
			ctlflag |= COMCTL_SPEC_R; break;
		case 'r':
			ctlflag |= COMCTL_SPEC_r; break;
		case 'T':
			ctlflag |= COMCTL_SPEC_T; break;
		case 't':
			ctlflag |= COMCTL_SPEC_t; break;
		case 'C':
			ctlflag |= COMCTL_SPEC_C; break;
		case 'c':
			ctlflag |= COMCTL_SPEC_c; break;
		case 'S':
			ctlflag |= COMCTL_SPEC_S; break;
		case 's':
			ctlflag |= COMCTL_SPEC_s; break;
		case 'X':
			ctlflag |= COMCTL_SPEC_X; break;
		case 'x':
			ctlflag |= COMCTL_SPEC_x; break;
		case 'L':
			ctlflag |= COMCTL_SPEC_L; break;
		case 'l':
			ctlflag |= COMCTL_SPEC_l; break;
		default:
			continue;
		}
	}

	dcbflag = FALSE;
	switch (ctlflag & COMCTL_FLOWS) {
		case 0:
			if (channel->dcb.fOutxCtsFlow == TRUE) {
				ctlflag |= COMCTL_SPEC_C;
				break;
			}
			if (channel->dcb.fOutxDsrFlow == TRUE) {
				ctlflag |= COMCTL_SPEC_S;
				break;
			}
			if (channel->dcb.fOutX == TRUE) {
				ctlflag |= COMCTL_SPEC_X;
				break;
			}
			break;
		case (COMCTL_SPEC_S | COMCTL_SPEC_C | COMCTL_SPEC_x):
		case (COMCTL_SPEC_S | COMCTL_SPEC_C):
			channel->dcb.fOutxCtsFlow = TRUE;
			channel->dcb.fOutxDsrFlow = TRUE;
			channel->dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
			channel->dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
			channel->dcb.fDsrSensitivity = TRUE;
			channel->dcb.fOutX = FALSE;
			channel->dcb.fInX = FALSE;
			dcbflag = TRUE;
			break;
		case (COMCTL_SPEC_S | COMCTL_SPEC_X | COMCTL_SPEC_c):
		case (COMCTL_SPEC_S | COMCTL_SPEC_X):
			channel->dcb.fOutxCtsFlow = FALSE;
			channel->dcb.fOutxDsrFlow = TRUE;
			channel->dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
			channel->dcb.fRtsControl = RTS_CONTROL_ENABLE;
			channel->dcb.fDsrSensitivity = TRUE;
			channel->dcb.fOutX = TRUE;
			channel->dcb.fInX = TRUE;
			dcbflag = TRUE;
			break;
		case (COMCTL_SPEC_C | COMCTL_SPEC_X | COMCTL_SPEC_s):
		case (COMCTL_SPEC_C | COMCTL_SPEC_X):
			channel->dcb.fOutxCtsFlow = TRUE;
			channel->dcb.fOutxDsrFlow = FALSE;
			channel->dcb.fDtrControl = DTR_CONTROL_ENABLE;
			channel->dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
			channel->dcb.fDsrSensitivity = TRUE;
			channel->dcb.fOutX = TRUE;
			channel->dcb.fInX = TRUE;
			dcbflag = TRUE;
			break;
		case (COMCTL_SPEC_C | COMCTL_SPEC_X | COMCTL_SPEC_S):
			channel->dcb.fOutxCtsFlow = TRUE;
			channel->dcb.fOutxDsrFlow = TRUE;
			channel->dcb.fDtrControl = DTR_CONTROL_ENABLE;
			channel->dcb.fRtsControl = RTS_CONTROL_ENABLE;
			channel->dcb.fDsrSensitivity = TRUE;
			channel->dcb.fOutX = TRUE;
			channel->dcb.fInX = TRUE;
			dcbflag = TRUE;
			break;
		case (COMCTL_SPEC_C | COMCTL_SPEC_s | COMCTL_SPEC_x):
		case (COMCTL_SPEC_C | COMCTL_SPEC_s):
		case (COMCTL_SPEC_C | COMCTL_SPEC_x):
		case (COMCTL_SPEC_C):
			channel->dcb.fOutxCtsFlow = TRUE;
			channel->dcb.fOutxDsrFlow = FALSE;
			channel->dcb.fDtrControl = DTR_CONTROL_ENABLE;
			channel->dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
			channel->dcb.fDsrSensitivity = FALSE;
			channel->dcb.fOutX = FALSE;
			channel->dcb.fInX = FALSE;
			dcbflag = TRUE;
			break;
		case (COMCTL_SPEC_S | COMCTL_SPEC_c | COMCTL_SPEC_x):
		case (COMCTL_SPEC_S | COMCTL_SPEC_c):
		case (COMCTL_SPEC_S | COMCTL_SPEC_x):
		case (COMCTL_SPEC_S):
			channel->dcb.fOutxCtsFlow = FALSE;
			channel->dcb.fOutxDsrFlow = TRUE;
			channel->dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
			channel->dcb.fRtsControl = RTS_CONTROL_ENABLE;
			channel->dcb.fDsrSensitivity = TRUE;
			channel->dcb.fOutX = FALSE;
			channel->dcb.fInX = FALSE;
			dcbflag = TRUE;
			break;
		case (COMCTL_SPEC_X | COMCTL_SPEC_s | COMCTL_SPEC_c):
		case (COMCTL_SPEC_X | COMCTL_SPEC_s):
		case (COMCTL_SPEC_X | COMCTL_SPEC_c):
		case (COMCTL_SPEC_X):
			channel->dcb.fOutxCtsFlow = FALSE;
			channel->dcb.fOutxDsrFlow = FALSE;
			channel->dcb.fDtrControl = DTR_CONTROL_ENABLE;
			channel->dcb.fRtsControl = RTS_CONTROL_ENABLE;
			channel->dcb.fDsrSensitivity = FALSE;
			channel->dcb.fOutX = TRUE;
			channel->dcb.fInX = TRUE;
			dcbflag = TRUE;
			break;
		case (COMCTL_SPEC_s):
			channel->dcb.fOutxDsrFlow = FALSE;
			channel->dcb.fDtrControl = DTR_CONTROL_ENABLE;
			channel->dcb.fDsrSensitivity = FALSE;
			dcbflag = TRUE;
			break;
		case (COMCTL_SPEC_c):
			channel->dcb.fOutxCtsFlow = FALSE;
			channel->dcb.fRtsControl = RTS_CONTROL_ENABLE;
			dcbflag = TRUE;
			break;
		case (COMCTL_SPEC_x):
			channel->dcb.fOutX = FALSE;
			channel->dcb.fInX = FALSE;
			dcbflag = TRUE;
			break;
		case (COMCTL_SPEC_s | COMCTL_SPEC_c):
			channel->dcb.fOutxCtsFlow = FALSE;
			channel->dcb.fOutxDsrFlow = FALSE;
			channel->dcb.fDtrControl = DTR_CONTROL_ENABLE;
			channel->dcb.fRtsControl = RTS_CONTROL_ENABLE;
			channel->dcb.fDsrSensitivity = FALSE;
			dcbflag = TRUE;
			break;
		case (COMCTL_SPEC_s | COMCTL_SPEC_x):
			channel->dcb.fOutxDsrFlow = FALSE;
			channel->dcb.fDtrControl = DTR_CONTROL_ENABLE;
			channel->dcb.fDsrSensitivity = FALSE;
			channel->dcb.fOutX = FALSE;
			channel->dcb.fInX = FALSE;
			dcbflag = TRUE;
			break;
		case (COMCTL_SPEC_c | COMCTL_SPEC_x):
			channel->dcb.fOutxCtsFlow = FALSE;
			channel->dcb.fRtsControl = RTS_CONTROL_ENABLE;
			channel->dcb.fOutX = FALSE;
			channel->dcb.fInX = FALSE;
			dcbflag = TRUE;
			break;
		case (COMCTL_SPEC_s | COMCTL_SPEC_x | COMCTL_SPEC_c):
			channel->dcb.fOutxCtsFlow = FALSE;
			channel->dcb.fOutxDsrFlow = FALSE;
			channel->dcb.fDtrControl = DTR_CONTROL_ENABLE;
			channel->dcb.fRtsControl = RTS_CONTROL_ENABLE;
			channel->dcb.fDsrSensitivity = FALSE;
			channel->dcb.fOutX = FALSE;
			channel->dcb.fInX = FALSE;
			dcbflag = TRUE;
			break;
		default:
			comerror(channel, 0, "INVALID COMCTL OPTION", 0);
			return(775);
	}
	if (dcbflag) {
		retcode = SetCommState(channel->handle, &channel->dcb);
		if (!retcode) {
			comerror(channel, 0, "SETCOMMSTATE FAILURE", GetLastError());
			return(753); 
		}
	}
	if ((ctlflag & (COMCTL_SPEC_R | COMCTL_SPEC_r)) && !(ctlflag & COMCTL_SPEC_C)) {
		if (ctlflag & COMCTL_SPEC_R) retcode = EscapeCommFunction(channel->handle, SETRTS);
		else retcode = EscapeCommFunction(channel->handle, CLRRTS);
		if (!retcode) {
			comerror(channel, 0, "ESCAPECOMMFUNCTION FAILURE", GetLastError());
			return(753); 
		}
	}
	if ((ctlflag & (COMCTL_SPEC_T | COMCTL_SPEC_t)) && !(ctlflag & COMCTL_SPEC_S)) {
		if (ctlflag & COMCTL_SPEC_T) retcode = EscapeCommFunction(channel->handle, SETDTR);
		else retcode = EscapeCommFunction(channel->handle, CLRDTR);
		if (!retcode) {
			comerror(channel, 0, "ESCAPECOMMFUNCTION FAILURE", GetLastError());
			return(753); 
		}
	}

/*** CODE: ADD SUPPORT FOR DCDFLAG HERE AND IN THE CODE ***/
//	if (ctlflag & COMCTL_SPEC_L) channel->dcdflag = TRUE;
//	else if (ctlflag & COMCTL_SPEC_l) channel->dcdflag = FALSE;

	retcode = GetCommModemStatus(channel->handle, &status);
	if (!retcode) {
		comerror(channel, 0, "GETCOMMMODEMSTATUS FAILURE", GetLastError());
		return(753); 
	}
	if (status & MS_CTS_ON) msgout[0] = 'C';
	else msgout[0] = 'c';
	if (status & MS_DSR_ON) msgout[1] = 'S';
	else msgout[1] = 's';
	if (status & MS_RLSD_ON) msgout[2] = 'L';
	else msgout[2] = 'l';
	if (status & MS_RING_ON) msgout[3] = 'B';
	else msgout[3] = 'b';
	*msgoutlen = 4;

	return(0);
}

static void sercallback(CHANNEL *channel)
{
	INT count, retcode;
	DWORD nbytes;

	if (channel->channeltype != CHANNELTYPE_SERIAL) return;

	evtstartcritical();

	if (channel->status & COM_SEND_PEND) {
		if (channel->flags & COM_FLAGS_SENDWAIT) {
			if (WaitForSingleObject(channel->sendoverlap.hEvent, 0) != WAIT_OBJECT_0) goto sercallback0;
			channel->flags &= ~COM_FLAGS_RECVWAIT;
			if (!GetOverlappedResult(channel->handle, &channel->sendoverlap, &channel->sendbytes, FALSE)) {
				channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_ERROR;
				evtset(channel->recvevtid);
				goto sercallback0;
			}
			if (channel->sendbytes > 0) channel->senddone += channel->sendbytes;
			if (channel->senddone >= channel->sendlength) {
				channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_DONE;
				evtset(channel->sendevtid);
				goto sercallback0;
			}
		}

		count = channel->sendlength - channel->senddone;
		retcode = WriteFile(channel->handle, &channel->sendbuf[channel->senddone], count, &channel->sendbytes, &channel->sendoverlap);
		if (retcode && (INT) channel->sendbytes >= count) {  /* success */
			channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_DONE;
			evtset(channel->sendevtid);
			goto sercallback0;
		}
		if (!retcode) {
			if (GetLastError() != ERROR_IO_PENDING) {
				channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_ERROR;
				evtset(channel->sendevtid);
				goto sercallback0;
			}
			channel->flags |= COM_FLAGS_SENDWAIT;
			nbytes = 0;
		}
/*** CODE: SYNCRONOUS RETURN (SHOULD NOT HAPPEN), SHOULD WE LOOP BACK UP UNTIL DONE, OR ASYNCRONOUS ***/
		else nbytes = channel->sendbytes;
		channel->senddone += nbytes;
	}

sercallback0:
	if (channel->status & COM_RECV_PEND) {
		if (channel->flags & COM_FLAGS_RECVWAIT) {
			if (WaitForSingleObject(channel->recvoverlap.hEvent, 0) != WAIT_OBJECT_0) goto sercallback2;
			channel->flags &= ~COM_FLAGS_RECVWAIT;
		}

sercallback1:
		if (channel->recvtail != channel->recvhead) serreadprocess(channel);
		if (channel->status & COM_RECV_PEND) {
			if (channel->recvhead > channel->recvdone) {
				if (channel->recvhead < channel->recvtail) {
					channel->recvtail -= channel->recvhead;
					memmove(&channel->recvbuf[channel->recvdone], &channel->recvbuf[channel->recvhead], channel->recvtail);
				}
				else channel->recvtail = 0;
				channel->recvhead = channel->recvdone;
				channel->recvtail += channel->recvdone;
			}
			ResetEvent(channel->recvoverlap.hEvent);
			channel->recvoverlap.Internal = 0;
			channel->recvoverlap.InternalHigh = 0;
			channel->recvoverlap.Offset = 0;
			channel->recvoverlap.OffsetHigh = 0;
			retcode = ReadFile(channel->handle, &channel->recvbuf[channel->recvtail], channel->recvbufsize - channel->recvtail, &channel->recvbytes, &channel->recvoverlap);
			if (!retcode) {
				if (GetLastError() != ERROR_IO_PENDING) {  /* error */
					channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_ERROR;
					evtset(channel->recvevtid);
					goto sercallback2;
				}
				/* this should not happen because we have immediate timeout */
				if (WaitForSingleObject(channel->recvoverlap.hEvent, INFINITE) != WAIT_OBJECT_0 ||
				    GetOverlappedResult(channel->handle, &channel->recvoverlap, &channel->recvbytes, FALSE)) {
					channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_ERROR;
					evtset(channel->recvevtid);
					goto sercallback2;
				}
			}
			if (channel->recvbytes > 0) {  /* successful read */
				channel->recvtail += channel->recvbytes;
				goto sercallback1;
			}
			if (!SetCommMask(channel->handle, EV_RXCHAR)) { 
				retcode = GetLastError();
				channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_ERROR;
				evtset(channel->recvevtid);
				goto sercallback2;
			}
			if (WaitCommEvent(channel->handle, &channel->recvbytes, &channel->recvoverlap)) goto sercallback1;
			if (GetLastError() != ERROR_IO_PENDING) {  /* error */
				channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_ERROR;
				evtset(channel->recvevtid);
				goto sercallback2;
			}
			channel->flags |= COM_FLAGS_RECVWAIT;
		}
	}

sercallback2:
	if ((channel->flags & COM_FLAGS_SENDREG) && !(channel->status & COM_SEND_PEND)) {
		serunregister(channel->sendoverlap.hEvent);
		channel->flags &= ~COM_FLAGS_SENDREG;
	}
	if ((channel->flags & COM_FLAGS_RECVREG) && !(channel->status & COM_RECV_PEND)) {
		serunregister(channel->recvoverlap.hEvent);
		channel->flags &= ~COM_FLAGS_RECVREG;
	}

	evtendcritical();
}

static void serreadprocess(CHANNEL *channel)
{
	INT i1, done, head, ignorelen, length, limit, matchlen, tail;
	UCHAR chr, matchchr, *buffer, *ignore, *match;

	head = channel->recvhead;
	tail = channel->recvtail;
	buffer = channel->recvbuf;

	if (channel->recvstate == COM_STATE_START) {
		matchlen = channel->recvstart[0];
		match = channel->recvstart + 1;
		matchchr = match[0];
		for ( ; head < tail; head++) {
			if (buffer[head] != matchchr) continue;
			if (head + matchlen > tail) break;
			for (i1 = 1; i1 < matchlen && buffer[head + i1] == match[i1]; i1++);
			if (i1 == matchlen) {
				head += matchlen;
				channel->recvstate = COM_STATE_END;
				break;
			}
		}
	}

	if (channel->recvstate == COM_STATE_END) {
		done = channel->recvdone;
		length = channel->recvlength;
/*** CODE: IF 0 MAYBE SET TO START STRING AND SEARCH FOR THAT ***/
		if ( (matchlen = channel->recvend[0]) ) {
			match = channel->recvend + 1;
			matchchr = match[0];
		}
		if ( (ignorelen = channel->recvignore[0]) )
			ignore = channel->recvignore + 1;
		if (!(limit = channel->recvlimit) && !matchlen) limit = length;
		for ( ; head < tail; head++) {
			chr = buffer[head];
			if (ignorelen) {
				for (i1 = 0; i1 < ignorelen && chr != ignore[i1]; i1++);
				if (i1 < ignorelen) continue;
			}
			if (matchlen && chr == matchchr) {
				if (head + matchlen > tail) break;
				for (i1 = 1; i1 < matchlen && buffer[head + i1] == match[i1]; i1++);
				if (i1 == matchlen) {
					head += matchlen;
					channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_DONE;
					evtset(channel->recvevtid);
					break;
				}
			}
			if (done < length || limit) buffer[done++] = chr;
			if (done == limit) {
				head++;
				channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_DONE;
				evtset(channel->recvevtid);
				break;
			}
		}
		channel->recvdone = done;
	}
	channel->recvhead = head;
}

static INT serparse(CHANNEL *channel, CHAR *pos1)
{
	CHAR *pos2;

	if (*pos1 == ':') pos1++;
	if (!*pos1) return(0);

	pos2 = strchr(pos1, ',');  /* BAUD RATE */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		channel->baud = 100 * atoi(pos1);
	}
	else pos2++;
	if ((pos1 = pos2) == NULL) return(1);

	pos2 = strchr(pos1, ',');  /* PARITY */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		channel->parity = (CHAR) toupper(*pos1);
	}
	else pos2++;
	if ((pos1 = pos2) == NULL) return(2);

	pos2 = strchr(pos1, ',');  /* DATA BITS */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		channel->databits = atoi(pos1);
	}
	else pos2++;
	if ((pos1 = pos2) == NULL) return(3);

	pos2 = strchr(pos1, ',');  /* STOP BITS */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		channel->stopbits = atoi(pos1);
	}
	else pos2++;
	if ((pos1 = pos2) == NULL) return(4);

	pos2 = strchr(pos1, ',');  /* RECV START */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		if (sergetparm(channel->recvstart, pos1) < 0) return RC_ERROR;
	}
	else {
		channel->recvstart[0] = 0;
		pos2++;
	}
	if ((pos1 = pos2) == NULL) return(5);

	pos2 = strchr(pos1, ',');  /* RECV END */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		if (sergetparm(channel->recvend, pos1) < 0) return RC_ERROR;
	}
	else {
		channel->recvend[0] = 0;
		pos2++;
	}
	if ((pos1 = pos2) == NULL) return(6);
	pos2 = strchr(pos1, ',');  /* SEND START */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		if (sergetparm(channel->sendstart, pos1) < 0) return RC_ERROR;
	}
	else {
		channel->sendstart[0] = 0;
		pos2++;
	}
	if ((pos1 = pos2) == NULL) return(7);

	pos2 = strchr(pos1, ',');  /* SEND END */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		if (sergetparm(channel->sendend, pos1) < 0) return RC_ERROR;
	}
	else {
		channel->sendend[0] = 0;
		pos2++;
	}
	if ((pos1 = pos2) == NULL) return(8);

	pos2 = strchr(pos1, ',');  /* RECV IGNORE */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		if (sergetparm(channel->recvignore, pos1) < 0) return RC_ERROR;
	} else {
		channel->recvignore[0] = 0;
		pos2++;
	}
	if (pos2 == NULL) return(9);

	channel->recvlimit = atoi(pos2);
	return(10);
}

static INT sergetparm(UCHAR *deststr, CHAR *strptr)
{
	INT i1, destpos, cnt;

	destpos = 0;
	while (*strptr) {
		destpos++;
		if (destpos == MAXSTARTEND) return RC_ERROR;
		if (*strptr == '\\') {
			strptr++;
			if (*strptr == '\\' || *strptr == '^') i1 = *strptr;
			else {
				i1 = 0;
				for (cnt = 0; cnt < 3; cnt++) {
					if (!isdigit(*strptr)) return RC_ERROR;
					i1 = i1 * 10 + *strptr - '0';
					strptr++;
				}
				strptr--;  /* incremented by one too many */
				if (i1 > 255) return RC_ERROR;
			}
			deststr[destpos] = (UCHAR) i1;
		}
		else if (*strptr == '^') {
			strptr++;
			i1 = toupper(*strptr) - 'A' + 1;
			if (i1 < 1 || i1 > 31) return RC_ERROR;
			deststr[destpos] = (UCHAR) i1;
		}
		else deststr[destpos] = *strptr;
		strptr++;
	}
	deststr[0] = destpos;  /* save length of deststr in first byte */
	return(0);
}

#define SERMAX 16

typedef struct SEREVENTS {
	HANDLE event;
	void (*callback)(CHANNEL *);
	CHANNEL *channel;
} SEREVENTS;

static SEREVENTS serevents[SERMAX];
static HANDLE serregevent;
static HANDLE serthreadhandle;

static DWORD serthread(LPVOID);

static INT serregister(HANDLE event, void (*callback)(CHANNEL *), CHANNEL *channel)
{
	INT i1;
	DWORD threadid;

	/* if error, call comerror on behalf of caller */
	for (i1 = 0; i1 < SERMAX && serevents[i1].event != NULL; i1++);
	if (i1 == SERMAX) {
		comerror(channel, 0, "TOO MANY SERIAL DEVICES OPENED", 0);
		return RC_ERROR;
	}

	if (serregevent == NULL) {
		serregevent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (serregevent == NULL) {
			comerror(channel, 0, "CREATEEVENT FAILED", GetLastError());
			return RC_ERROR;
		}
		serthreadhandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) serthread, (LPVOID) &serevents, 0, &threadid);
		if (serthreadhandle == NULL) {
			comerror(channel, 0, "CREATETHREAD FAILED", GetLastError());
			CloseHandle(serregevent);
			serregevent = NULL;
			return RC_ERROR;
		}
	}

	evtstartcritical();
	serevents[i1].event = event;
	serevents[i1].callback = callback;
	serevents[i1].channel = channel;
	evtendcritical();
	SetEvent(serregevent);

	return(0);
}

static void serunregister(HANDLE event)
{
	INT i1;

	for (i1 = 0; i1 < SERMAX && event != serevents[i1].event; i1++);
	if (i1 == SERMAX) return;

	evtstartcritical();
	serevents[i1].event = NULL;
	evtendcritical();
	SetEvent(serregevent);

/*** CODE, FIGURE OUT SOMEWAY TO RELEASE THREAD, ETC. INSTEAD OF AFTER EVERY SEND/RECV ***/
#if 0
	for (i1 = 0; i1 < SERMAX && serevents[i1] == NULL; i1++);
	if (i1 == SERMAX) {
		TerminateThread(serthreadhandle, 0);
		CloseHandle(serthreadhandle);
		CloseHandle(serregevent);
		serregevent = NULL;
	}
#endif
}

static DWORD serthread(LPVOID parm)
{
	INT i1, i2, workevent[SERMAX + 1];
	HANDLE workhandle[SERMAX + 1];
	SEREVENTS *events;

	events = (SEREVENTS *) parm;
	workhandle[0] = serregevent;
	for ( ; ; ) {
		ResetEvent(serregevent);
		evtstartcritical();
		for (i1 = 1, i2 = 0; i2 < SERMAX; i2++) {
			if (events[i2].event == NULL) continue;
			workhandle[i1] = events[i2].event;
			workevent[i1++] = i2;
		}
		evtendcritical();

		i2 = WaitForMultipleObjects(i1, workhandle, FALSE, INFINITE) - WAIT_OBJECT_0;
		if (i2 >= 1 && i2 < i1) {
			events[workevent[i2]].callback(events[workevent[i2]].channel);
		}
	}
	return(0);
}

/* udp communication routines */

INT os_udpopen(CHAR *channelname, CHANNEL *channel)
{
	INT retcode;
	UINT i1;
	SOCKADDR_STORAGE workAddr;
	ULONG32 v4anyaddr = htonl(INADDR_ANY);

	if (!udptcpinitflg && udptcpinit(channel) != 0) return(753);

	for (i1 = 0; isdigit(*channelname); channelname++) i1 = i1 * 10 + *channelname - '0';
	if (!i1 || (*channelname && !isspace(*channelname))) {
		comerror(channel, 0, "INVALID PORT NUMBER", 0);
		return(753);
	}
	channel->ourport = (PORT) i1;

	if (getchanneltableentry(channel) == RC_NO_MEM) return(753);  /* comerror called by getchanneltableentry */

	/*
	 * For 16.1 we will not be supporting UDP using V6, too complicated!  (as of 04/25/11 jpr)
	 */
	channel->socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (channel->socket == INVALID_SOCKET) {
		comerror(channel, COM_PER_ERROR, "SOCKET FAILURE", WSAGetLastError());
		os_udpclose(channel);
		return(753);
	}
	channel->flags |= COM_FLAGS_OPEN;

	/* bind the socket */
	ZeroMemory(&workAddr, sizeof(workAddr));
	memcpy(&((PSOCKADDR)&workAddr)->sa_data, &v4anyaddr, sizeof(IN_ADDR));
	((PSOCKADDR_IN)&workAddr)->sin_family = PF_INET;
	((PSOCKADDR_IN)&workAddr)->sin_port = htons(channel->ourport);
	if (bind(channel->socket, (PSOCKADDR)&workAddr, sizeof(workAddr)) == SOCKET_ERROR) {
		comerror(channel, COM_PER_ERROR, "BIND FAILURE", WSAGetLastError());
		os_udpclose(channel);
		return(753);
	}

	/*
	 * Save this for later use by comctl
	 */
	memcpy(&channel->ouraddr, &workAddr, sizeof(workAddr));


//	servaddr.spec_addr.sin_family = PF_INET;
//	servaddr.spec_addr.sin_addr.s_addr = channel->ouraddr.addr.s_addr;
//	servaddr.spec_addr.sin_port = htons(channel->ourport);
//	for (i1 = 0; i1 < sizeof(servaddr.spec_addr.sin_zero); i1++)
//		servaddr.spec_addr.sin_zero[i1] = 0;
//	if (bind(channel->socket, &servaddr.gen_addr, sizeof(servaddr.spec_addr)) == SOCKET_ERROR) {
//		comerror(channel, 0, "BIND FAILURE", WSAGetLastError());
//		os_udpclose(channel);
//		return(753);
//	}
//	channel->flags |= COM_FLAGS_BOUND;

	retcode = WSAAsyncSelect(channel->socket, winhnd, channel->channeltableentry + WM_USER + 1, FD_READ | FD_WRITE);
	if (retcode == SOCKET_ERROR) {
		comerror(channel, 0, "WSAASYNCSELECT FAILURE", WSAGetLastError());
		os_udpclose(channel);
		return(753);
	}
	channel->flags |= COM_FLAGS_SELECT;
	return(0);
}

void os_udpclose(CHANNEL *channel)
{
	os_udpclear(channel, COM_CLEAR_SEND | COM_CLEAR_RECV, NULL);

	if (channel->flags & COM_FLAGS_SELECT) WSAAsyncSelect(channel->socket, winhnd, 0, 0);
/*	if (channel->flags & COM_FLAGS_BOUND) ; */
	if (channel->flags & COM_FLAGS_OPEN) closesocket(channel->socket);
	if (channel->flags & COM_FLAGS_TABLE) channeltable[channel->channeltableentry] = 0;
	channel->flags &= ~(COM_FLAGS_OPEN | /*COM_FLAGS_BOUND |*/ COM_FLAGS_SELECT | COM_FLAGS_TABLE);
}

INT os_udpsend(CHANNEL *channel)
{
	INT retcode, timehandle;
	DWORD lasterr;
	UCHAR timestamp[16];

	if (channel->status & COM_SEND_MASK) os_udpclear(channel, COM_CLEAR_SEND, NULL);

	if (channel->status & COM_PER_ERROR) return(753);

	evtstartcritical();
	retcode = sendto(channel->socket, (CHAR *) channel->sendbuf, channel->sendlength,
			0, (PSOCKADDR)&channel->sendaddr, sizeof(SOCKADDR_IN));
	if (retcode == SOCKET_ERROR) lasterr = WSAGetLastError();

	if (retcode == SOCKET_ERROR) {
		if (lasterr == WSAEWOULDBLOCK) {
			if (channel->sendtimeout == 0) {
				channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_TIME;
				evtset(channel->sendevtid);
				evtendcritical();
				return(0);
			}
			channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_PEND;
			if (channel->sendtimeout > 0) {
				pmsctimestamp(timestamp);
				if (timadd(timestamp, channel->sendtimeout * 100) ||
				   (timehandle = timset(timestamp, channel->sendevtid)) < 0) {
					comerror(channel, 0, "SET TIMER FAILURE", 0);
					evtendcritical();
					return(753);
				}
				if (!timehandle) {
					channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_TIME;
					evtset(channel->sendevtid);
					evtendcritical();
					return(0);
				}
				channel->sendth = timehandle;
			}	
			evtendcritical();
			return(0);
		}
		if (lasterr == WSAEMSGSIZE) comerror(channel, COM_SEND_ERROR, "SEND MESSAGE TOO LONG", 0);
		else {
			comerror(channel, COM_SEND_ERROR, "SENDTO FAILURE", lasterr);
			return(753);
		}
	}
	else if (retcode != channel->sendlength) {
		comerror(channel, COM_SEND_ERROR, "BYTES SENT NOT EQUAL TO BYTES REQUESTED", 0);
		return(753);
	}
	else channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_DONE;

	evtset(channel->sendevtid);
	evtendcritical();
	return(0);
}

INT os_udprecv(CHANNEL *channel)
{
	INT addrlen, retcode, timehandle;
	DWORD lasterr;
	UCHAR timestamp[16];
	SOCKADDR_STORAGE raddr;

	if (channel->status & COM_RECV_MASK) os_udpclear(channel, COM_CLEAR_RECV, NULL);

	if (channel->status & COM_PER_ERROR) return(753);

	evtstartcritical();
	addrlen = sizeof(SOCKADDR_STORAGE);
	retcode = recvfrom(channel->socket, (CHAR *) channel->recvbuf, channel->recvlength,
			0, (PSOCKADDR)&raddr, &addrlen);
	if (retcode == SOCKET_ERROR) {
		if ((lasterr = WSAGetLastError()) == WSAEWOULDBLOCK) {
			if (channel->recvtimeout == 0) {
				channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_TIME;
				evtset(channel->recvevtid);
				evtendcritical();
				return(0);
			}
			channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_PEND;
			if (channel->recvtimeout > 0) {
				pmsctimestamp(timestamp);
				if (timadd(timestamp, channel->recvtimeout * 100) ||
				   (timehandle = timset(timestamp, channel->recvevtid)) < 0) {
					comerror(channel, 0, "SET TIMER FAILURE", 0);
					evtendcritical();
					return(753);
				}
				if (!timehandle) {
					channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_TIME;
					evtset(channel->recvevtid);
					evtendcritical();
					return(0);
				}
				channel->recvth = timehandle;
			}
			evtendcritical();
			return(0);
		}
		if (lasterr == WSAEMSGSIZE) {
			channel->recvdone = channel->recvlength;
			channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_DONE;
		}
		else comerror(channel, COM_RECV_ERROR, "RECVFROM FAILURE", lasterr);
	}
	else {
		channel->recvdone = retcode;
		channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_DONE;
	}

	if (channel->status & COM_RECV_DONE) {
 		if (raddr.ss_family != PF_INET && raddr.ss_family != PF_INET6)
			comerror(channel, COM_RECV_ERROR, "RECVFROM ADDRESS FAMILY NOT PF_INETx", 0);
		else {
			channel->recvaddr = raddr;
		}
	}

	evtset(channel->recvevtid);
	evtendcritical();
	return(0);
}

INT os_udpclear(CHANNEL *channel, INT flag, INT *status)
{
	evtstartcritical();
	if (channel->status & COM_SEND_PEND) {
		if (channel->sendtimeout > 0 && evttest(channel->sendevtid)) {  /* timeout happened */
			channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_TIME;
		}
	}
	if (channel->status & COM_RECV_PEND) {
		if (channel->recvtimeout > 0 && evttest(channel->recvevtid)) {  /* timeout happened */
			channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_TIME;
		}
	}

	if (status != NULL) *status = channel->status;

	if ((flag & COM_CLEAR_SEND)) {
		channel->status &= ~COM_SEND_MASK;
		if (channel->sendth) {
			timstop(channel->sendth);
			channel->sendth = 0;
		}
		evtclear(channel->sendevtid);
	}
	if ((flag & COM_CLEAR_RECV)) {
		channel->status &= ~COM_RECV_MASK;
		if (channel->recvth) {
			timstop(channel->recvth);
			channel->recvth = 0;
		}
		evtclear(channel->recvevtid);
	}
	evtendcritical();
	return(0);
}

INT os_udpctl(CHANNEL *channel, UCHAR *msgin, INT cbMsgin, UCHAR *msgout, INT *msgoutlen)
{
	INT i1;
	USHORT port;
	SOCKADDR_STORAGE *address;
	ULONG addr;
	TCHAR work[256], addrWork[64], portWork[32];

	while (cbMsgin > 0 && isspace(msgin[cbMsgin - 1])) cbMsgin--;
	if (cbMsgin >= (INT) sizeof(work)) cbMsgin = sizeof(work) - 1;
	for (i1 = 0; i1 < cbMsgin && msgin[i1] != '=' && !isspace(msgin[i1]); i1++)
		work[i1] = (TCHAR) toupper(msgin[i1]);
	work[i1] = '\0';

	if (!strcmp(work, "GETRECVADDR"))  {
		address = &channel->recvaddr;
		port = ntohs(((PSOCKADDR_IN)address)->sin_port);
		if (!port) goto nullmsgoutreturn;
		addr = ntohl(((PSOCKADDR_IN)address)->sin_addr.s_addr);
		if (!addr) goto nullmsgoutreturn;

		i1 = getAddrAndPortStrings(channel, address, addrWork, ARRAYSIZE(addrWork),
				portWork, ARRAYSIZE(portWork));
		if (i1) {
			return i1;
		}
		i1 = sprintf_s(work, ARRAYSIZE(work), "%s %s", addrWork, portWork);
		if (i1 > *msgoutlen) i1 = *msgoutlen;
		memcpy(msgout, work, i1 * sizeof(TCHAR));
		*msgoutlen = i1;
	}
	else if (!strcmp(work, "GETLOCALADDR")) {
		port = channel->ourport;
		i1 = sprintf_s(portWork, ARRAYSIZE(portWork), "0.0.0.0 %d", (port & 0xFFFF));
		if (i1 > *msgoutlen) i1 = *msgoutlen;
		memcpy(msgout, portWork, i1 * sizeof(TCHAR));
		*msgoutlen = i1;
	}
	else if (!strcmp(work, "SETSENDADDR"))  {
		if (channel->status & COM_SEND_PEND) {
			comerror(channel, 0, "SETSENDADDR NOT ALLOWED WHILE SEND IS PENDING", 0);
			return(753); 
		}
		if (++i1 >= cbMsgin) {
			comerror(channel, 0, "MISSING SEND ADDRESS", 0);
			return(753);
		}
		memcpy(work, msgin + i1, cbMsgin - i1);
		work[cbMsgin - i1] = '\0';
		if ((i1 = setSendAddressInChannel(work, channel)) != 0) {
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
		*msgout = '\0';
		*msgoutlen = 1;
	}
	return(0);
}

/* tcp communication routines */

INT os_tcpclientopen(CHAR *channelname, CHANNEL *channel)
{
	CHAR work[256], *p1, address[256], service[8];
	PORT sendport;
	UINT sendaddr;

	if (!udptcpinitflg && udptcpinit(channel) < 0) return(753);
	if (getchanneltableentry(channel) == RC_NO_MEM) return(753);  /* comerror called by getchanneltableentry */
	strcpy_s(work, ARRAYSIZE(work), channelname);
	p1 = strtok(work, " ");
	if (p1 == NULL) {
		comerror(channel, 0, "INVALID IP ADDRESS OR DOMAIN NAME", 0);
		return(753);
	}
	strcpy_s(address, ARRAYSIZE(address), p1);
	p1 = strtok(NULL, " ");

	if (channel->channeltype == CHANNELTYPE_TCPCLIENT6) {
		if (allowIPv6) {
			strcpy_s(service, ARRAYSIZE(service), p1);
			return tcpclientopen_V6(channel, address, service);
		}
		else {
			comerror(channel, 0, "NO IPv6 Support on this system", 0);
			return(753);
		}
	}
	/*
	 * Use the old, V4-only code
	 */
	for (sendport = 0; isdigit(*p1); p1++) sendport = sendport * 10 + *p1 - '0';
	if (*p1) {
		comerror(channel, 0, "INVALID PORT NUMBER", 0);
		return(753);
	}
	if (ipstrtoaddr(address, &sendaddr)) {
		comerror(channel, 0, "INVALID IP ADDRESS OR DOMAIN NAME", 0);
		return(753);
	}
	return tcpclientopen_V4(channel, sendaddr, sendport);
}

static INT tcpclientopen_V4(CHANNEL *channel, UINT sendaddr, PORT sendport)
{
	INT retcode;
	UINT i1;
	DWORD lasterr;
	SOCKADDR_IN addr;

	channel->socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (channel->socket == INVALID_SOCKET) {
		comerror(channel, 0, "SOCKET FAILURE", WSAGetLastError());
		os_tcpclientclose(channel);
		return(753);
	}
	channel->flags |= COM_FLAGS_OPEN;

	if (clientkeepalive) {
		i1 = 1;
		if (setsockopt(channel->socket, SOL_SOCKET, SO_KEEPALIVE, (const void *) &i1, sizeof(i1))) {
			comerror(channel, 0, "Set KEEPALIVE FAILURE", errno);
			os_tcpclientclose(channel);
			return(753);
		}
	}

	addr.sin_family = PF_INET;
	addr.sin_addr.s_addr = 0;
	addr.sin_port = 0;
	for (i1 = 0; i1 < sizeof(addr.sin_zero); i1++)
		addr.sin_zero[i1] = 0;
	if (bind(channel->socket, (PSOCKADDR)&addr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR) {
		comerror(channel, 0, "BIND FAILURE", WSAGetLastError());
		os_tcpclientclose(channel);
		return(753);
	}
	//channel->flags |= COM_FLAGS_BOUND;

	retcode = WSAAsyncSelect(channel->socket, winhnd, channel->channeltableentry + WM_USER + 1,
			FD_READ | FD_WRITE | FD_CONNECT | FD_CLOSE);
	if (retcode == SOCKET_ERROR) {
		comerror(channel, 0, "WSAASYNCSELECT FAILURE", WSAGetLastError());
		os_tcpclientclose(channel);
		return(753);
	}
	channel->flags |= COM_FLAGS_SELECT;

	evtstartcritical();
	addr.sin_family = PF_INET;
	addr.sin_port = htons(sendport);
	addr.sin_addr.s_addr = sendaddr;
	for (i1 = 0; i1 < sizeof(addr.sin_zero); i1++)
		addr.sin_zero[i1] = 0;
	retcode = connect(channel->socket, (PSOCKADDR)&addr, sizeof(SOCKADDR_IN));
	if (retcode == SOCKET_ERROR) {
		if ((lasterr = WSAGetLastError()) != WSAEWOULDBLOCK) {
			comerror(channel, 0, "CONNECT FAILURE", lasterr);
			os_tcpclientclose(channel);
			evtendcritical();
			return(753);
		}
	}
	else channel->flags |= COM_FLAGS_CONNECT;
	memcpy(&channel->sendaddr, &addr, sizeof(SOCKADDR_IN));
	evtendcritical();

	return 0;
}

static INT tcpclientopen_V6(CHANNEL *channel, CHAR *address, CHAR *service)
{
	INT i1, aiErr, retcode;
	CHAR work[256];
	DWORD lasterr;
	ADDRINFO  hints, *aiHead, *ai;
	PSOCKADDR_IN6 pSadrIn6;
	SOCKADDR_STORAGE binder;

	ZeroMemory(&hints, sizeof(hints));
   	hints.ai_family = PF_INET6;
	hints.ai_flags = AI_NUMERICSERV;   /* db/c programmers always use a numeric port (service) */
	hints.ai_socktype = SOCK_STREAM;   /* Connection-oriented byte stream.   */
	hints.ai_protocol = IPPROTO_TCP;   /* TCP transport layer protocol only. */

	if ((aiErr = getaddrinfo(address, service, &hints, &aiHead)) != 0)
	{
		sprintf_s(work, ARRAYSIZE(work), "ERROR - %s", gai_strerror(aiErr));
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
			pSadrIn6 = (PSOCKADDR_IN6) ai->ai_addr;
			if ( pSadrIn6->sin6_scope_id == 0 )
			{
				pSadrIn6->sin6_scope_id = 1;
			}  /* End IF the scope ID wasn't set. */
		} /* End IPv6 kluge. */
		channel->socket = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (channel->socket < 0) {
			switch (errno) {
			case WSAEAFNOSUPPORT:
			case WSAEPROTONOSUPPORT:
				continue;
			default:
				comerror(channel, 0, "SOCKET FAILURE", WSAGetLastError());
				os_tcpclientclose(channel);
				freeaddrinfo(aiHead);
				return(753);
			}
		}
		channel->flags |= COM_FLAGS_OPEN;

		if (clientkeepalive) {
			i1 = 1;
			if (setsockopt(channel->socket, SOL_SOCKET, SO_KEEPALIVE, (const void *) &i1, sizeof(i1))) {
				comerror(channel, 0, "Set KEEPALIVE FAILURE", WSAGetLastError());
				os_tcpclientclose(channel);
				freeaddrinfo(aiHead);
				return(753);
			}
		}

		ZeroMemory(&binder, sizeof(SOCKADDR_STORAGE));
		binder.ss_family = ai->ai_family;
		//((PSOCKADDR)&binder)->sa_data = ((ai->ai_family == PF_INET6) ? in6addr_any : INADDR_ANY);
		if (bind(channel->socket, (PSOCKADDR)&binder,
				(ai->ai_family == PF_INET6) ? sizeof(SOCKADDR_IN6) : sizeof(SOCKADDR_IN)) == SOCKET_ERROR) {
			comerror(channel, 0, "BIND FAILURE", WSAGetLastError());
			os_tcpclientclose(channel);
			return(753);
		}
		retcode = WSAAsyncSelect(channel->socket, winhnd, channel->channeltableentry + WM_USER + 1,
				FD_READ | FD_WRITE | FD_CONNECT | FD_CLOSE);
		if (retcode == SOCKET_ERROR) {
			comerror(channel, 0, "WSAASYNCSELECT FAILURE", WSAGetLastError());
			os_tcpclientclose(channel);
			freeaddrinfo(aiHead);
			return(753);
		}
		channel->flags |= COM_FLAGS_SELECT;
		evtstartcritical();
		retcode = connect(channel->socket, ai->ai_addr, (socklen_t) ai->ai_addrlen);
		memcpy(&channel->sendaddr, ai->ai_addr,
				(ai->ai_family == PF_INET6) ? sizeof(SOCKADDR_IN6) : sizeof(SOCKADDR_IN));
		if (retcode != SOCKET_ERROR) {
			channel->flags |= COM_FLAGS_CONNECT;
			evtendcritical();
			freeaddrinfo(aiHead);
			return 0;
		}
		else if ((lasterr = WSAGetLastError()) == WSAEWOULDBLOCK) {
			if (waitForTCPConnect) {
				if (!ResetEvent(ghTCPClientConnect)) {
					evtendcritical();
					freeaddrinfo(aiHead);
					comerror(channel, 0, "ResetEvent", GetLastError());
					os_tcpclientclose(channel);
					return(753);
				}
			}
			evtendcritical();
			if (waitForTCPConnect) {
				// wait here, then check for error
				DWORD retval = WaitForSingleObject(ghTCPClientConnect, INFINITE);
				if (retval == WAIT_FAILED) {
					comerror(channel, 0, "WaitForSingleObject", GetLastError());
					os_tcpclientclose(channel);
					freeaddrinfo(aiHead);
					return(753);
				}
				if (channel->status & COM_PER_ERROR) {
					if (ai->ai_next != NULL) {
						WSAAsyncSelect(channel->socket, winhnd, 0, 0);
						evtstartcritical();
						closesocket(channel->socket);
						channel->flags &= ~(COM_FLAGS_OPEN | COM_FLAGS_SELECT);
						evtendcritical();
						channel->socket = INVALID_SOCKET;
						continue;
					}
					os_tcpclientclose(channel);
					freeaddrinfo(aiHead);
					return(753);
				}
			}
			freeaddrinfo(aiHead);
			return 0;
		}
		evtendcritical();
	}
	comerror(channel, 0, "CONNECT FAILURE", lasterr);
	os_tcpclientclose(channel);
	freeaddrinfo(aiHead);
	return(753);
}

void os_tcpclientclose(CHANNEL *channel)
{
	os_tcpclear(channel, COM_CLEAR_SEND | COM_CLEAR_RECV, NULL);

	if (channel->flags & COM_FLAGS_CONNECT) {
		evtstartcritical();
		shutdown(channel->socket, SD_BOTH);
		channel->flags &= ~COM_FLAGS_CONNECT;
		evtendcritical();
	}
	if (channel->flags & COM_FLAGS_SELECT) WSAAsyncSelect(channel->socket, winhnd, 0, 0);
/*	if (channel->flags & COM_FLAGS_BOUND) ; */
	if (channel->flags & COM_FLAGS_OPEN) {
		evtstartcritical();
		closesocket(channel->socket);
		channel->flags &= ~COM_FLAGS_OPEN;
		evtendcritical();
	}
	if (channel->flags & COM_FLAGS_TABLE) channeltable[channel->channeltableentry] = 0;
	channel->flags &= ~(COM_FLAGS_SELECT | /*COM_FLAGS_BOUND |*/ COM_FLAGS_TABLE);
}


static INT tcpserveropen(CHANNEL *channel, ADDRINFO *ainfo) {
	INT reuse, retcode, addrlen;
	struct linger lingstr;

	channel->socket = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
		if (channel->socket == INVALID_SOCKET) {
		comerror(channel, 0, "SOCKET FAILURE", WSAGetLastError());
		os_tcpserverclose(channel);
		return(753);
	}
	channel->flags |= COM_FLAGS_OPEN;

	reuse = 1;
	setsockopt(channel->socket, SOL_SOCKET, SO_REUSEADDR, (CHAR *) &reuse, sizeof(reuse));

	lingstr.l_onoff = 1;
	lingstr.l_linger = 0;
	setsockopt(channel->socket, SOL_SOCKET, SO_LINGER, (CHAR *) &lingstr, sizeof(lingstr));

	retcode = bind(channel->socket, ainfo->ai_addr, (socklen_t) ainfo->ai_addrlen);
	if (retcode == SOCKET_ERROR) {
		comerror(channel, 0, "BIND FAILURE", WSAGetLastError());
		os_tcpserverclose(channel);
		return(753);
	}

	if (channel->ourport == 0) {
		SOCKADDR_STORAGE address;
		u_short port = 0;
		addrlen = sizeof(address);
		retcode = getsockname(channel->socket, (PSOCKADDR) &address, &addrlen);
		if (retcode == SOCKET_ERROR) {
			comerror(channel, 0, "GETSOCKNAME FAILURE", WSAGetLastError());
			os_tcpserverclose(channel);
			return(753);
		}
		port = (address.ss_family == AF_INET) ? ((PSOCKADDR_IN) &address)->sin_port
				: ((PSOCKADDR_IN6) &address)->sin6_port;
		channel->ourport = ntohs(port);
	}

	retcode = WSAAsyncSelect(channel->socket, winhnd, channel->channeltableentry + WM_USER + 1,
			FD_READ | FD_WRITE | FD_ACCEPT | FD_CLOSE);
	if (retcode == SOCKET_ERROR) {
		comerror(channel, 0, "WSAASYNCSELECT FAILURE", WSAGetLastError());
		os_tcpserverclose(channel);
		return(753);
	}
	channel->flags |= COM_FLAGS_SELECT;

	evtstartcritical();
	retcode = listen(channel->socket, 1);
	if (retcode == SOCKET_ERROR) {
		comerror(channel, 0, "LISTEN FAILURE", WSAGetLastError());
		evtendcritical();
		os_tcpserverclose(channel);
		return(753);
	}
	channel->flags |= COM_FLAGS_LISTEN;
	evtendcritical();
	return 0;
}

INT os_tcpserveropen(CHAR *channelname, CHANNEL *channel)
{
	INT found, retcode;

	if (!udptcpinitflg && udptcpinit(channel) < 0) return(753);
	retcode = tcpserveropen_common(channelname, channel, &found);
	if (retcode) return retcode;
	if (channel->channeltype == CHANNELTYPE_TCPSERVER6) {
		if (allowIPv6) {
			return tcpserveropen_V6(channelname, channel, found);
		}
		else {
			comerror(channel, 0, "NO IPv6 Support on this system", 0);
			return(753);
		}
	}
	return tcpserveropen_V4(channel, found);
}

/**
 * @param found Found connected server also holding listening socket
 */
static INT tcpserveropen_common(CHAR *channelname, CHANNEL *channel, INT *found)
{
	UINT i1;
	INT retcode;
	CHAR work[200], *p1;
	CHANNEL *ch;

	*found = FALSE;
	strcpy(work, channelname);
	p1 = strtok(work, " ");
	if (p1 != NULL) {
		for (i1 = 0; isdigit(*p1); p1++) i1 = i1 * 10 + *p1 - '0';
	}
	if (p1 == NULL || *p1) {
		comerror(channel, 0, "INVALID PORT NUMBER", 0);
		return(753);
	}
	channel->ourport = (PORT) i1;
	if (getchanneltableentry(channel) == RC_NO_MEM) return(753);  /* comerror called by getchanneltableentry */

	for (ch = getchannelhead(); ch != NULL; ch = ch->nextchannelptr) {
		if (!ISCHANNELTYPETCPSERVER(ch) || ch->ourport != i1 || ch == channel) continue;
		/* check for another sever that is waiting or is listening */
		if (!(ch->flags & COM_FLAGS_OPEN) || (ch->flags & COM_FLAGS_LISTEN)) break;
		if (ch->flags & COM_FLAGS_FDLISTEN) {  /* found connected server also holding listening socket */
			ch->flags &= ~COM_FLAGS_FDLISTEN;
			channel->socket = ch->fdlisten;
			channel->flags |= COM_FLAGS_OPEN | /*COM_FLAGS_BOUND |*/ COM_FLAGS_LISTEN;
			retcode = WSAAsyncSelect(channel->socket, winhnd, channel->channeltableentry + WM_USER + 1,
					FD_READ | FD_WRITE | FD_ACCEPT | FD_CLOSE);
			if (retcode == SOCKET_ERROR) {
				comerror(channel, 0, "WSAASYNCSELECT FAILURE", WSAGetLastError());
				os_tcpserverclose(channel);
				return(753);
			}
			channel->flags |= COM_FLAGS_SELECT;
			*found = TRUE;
			break;
		}
	}

	return 0;
}

/**
 * Used only on Win XP
 */
static INT tcpserveropen_V4(CHANNEL *channel, INT found)
{
	INT i1, i2, reuse, addrlen, retcode;
	struct linger lingstr;
	SOCKADDR_IN servaddr;

	if (!found) {  /* unable to find another server with same port */
		channel->socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (channel->socket == INVALID_SOCKET) {
			comerror(channel, 0, "SOCKET FAILURE", WSAGetLastError());
			os_tcpserverclose(channel);
			return(753);
		}
		channel->flags |= COM_FLAGS_OPEN;

		reuse = 1;
		setsockopt(channel->socket, SOL_SOCKET, SO_REUSEADDR, (CHAR *) &reuse, sizeof(reuse));

		lingstr.l_onoff = 1;
		lingstr.l_linger = 0;
		setsockopt(channel->socket, SOL_SOCKET, SO_LINGER, (CHAR *) &lingstr, sizeof(lingstr));

		servaddr.sin_family = PF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		servaddr.sin_port = htons(channel->ourport);
		for (i1 = 0; i1 < sizeof(servaddr.sin_zero); i1++) 	servaddr.sin_zero[i1] = 0;
		retcode = bind(channel->socket, (PSOCKADDR) &servaddr, sizeof(SOCKADDR_IN));
		if (retcode == SOCKET_ERROR) {
			comerror(channel, 0, "BIND FAILURE", WSAGetLastError());
			os_tcpserverclose(channel);
			return(753);
		}
		if (channel->ourport == 0) {
			addrlen = sizeof(SOCKADDR_IN);
			retcode = getsockname(channel->socket, (PSOCKADDR) &servaddr, &addrlen);
			if (retcode == SOCKET_ERROR) {
				comerror(channel, 0, "GETSOCKNAME FAILURE", WSAGetLastError());
				os_tcpserverclose(channel);
				return(753);
			}
			channel->ourport = ntohs(servaddr.sin_port);
		}

		retcode = WSAAsyncSelect(channel->socket, winhnd, channel->channeltableentry + WM_USER + 1, FD_READ | FD_WRITE | FD_ACCEPT | FD_CLOSE);
		if (retcode == SOCKET_ERROR) {
			comerror(channel, 0, "WSAASYNCSELECT FAILURE", WSAGetLastError());
			os_tcpserverclose(channel);
			return(753);
		}
		channel->flags |= COM_FLAGS_SELECT;

		evtstartcritical();
		retcode = listen(channel->socket, 1);
		if (retcode == SOCKET_ERROR) {
			comerror(channel, 0, "LISTEN FAILURE", WSAGetLastError());
			evtendcritical();
			os_tcpserverclose(channel);
			return(753);
		}
		channel->flags |= COM_FLAGS_LISTEN;
	}
	else evtstartcritical();

	if (channel->flags & COM_FLAGS_LISTEN) {
		retcode = tcpserveraccept(channel);
		if (retcode < 0) {
			evtendcritical();
			/* comerror called by tcpserveraccept */
			os_tcpserverclose(channel);
			return(753);
		}
	}
	evtendcritical();
	if (channel->flags & COM_FLAGS_OPEN && serverkeepalive) {
		i2 = 1;
		if (setsockopt(channel->socket, SOL_SOCKET, SO_KEEPALIVE, (const void *) &i2, sizeof(i2))) {
			comerror(channel, 0, "Set KEEPALIVE FAILURE", errno);
			os_tcpserverclose(channel);
			return(753);
		}
	}
	return 0;
}

static INT tcpserveropen_V6(CHAR *channelname, CHANNEL *channel, INT found)
{
	INT retcode, i2;
	CHAR work[128];
	ADDRINFO *AI, hints, *AddrInfo;

	if (!found) {  /* unable to find another server with same port */
		ZeroMemory(&hints, sizeof (hints));
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
	    hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE | AI_NUMERICSERV;
	    sprintf(work, "%d", (INT)channel->ourport);
	    retcode = GetAddrInfo(NULL, work, &hints, &AddrInfo);
		if (retcode == SOCKET_ERROR) {
			sprintf_s(work, ARRAYSIZE(work), "GetAddrInfo - %s", gai_strerror(retcode));
			comerror(channel, 0, work, retcode);
			os_tcpserverclose(channel);
			return(753);
		}

		for (AI = AddrInfo; AI != NULL; AI = AI->ai_next) {
			retcode = tcpserveropen(channel, AI);
			if (retcode == 0) break;
		}
		if (retcode != 0 || AI == NULL) return retcode;

	}
	else evtstartcritical();

	if (channel->flags & COM_FLAGS_LISTEN) {
		retcode = tcpserveraccept(channel);
		if (retcode < 0) {
			evtendcritical();
			/* comerror called by tcpserveraccept */
			os_tcpserverclose(channel);
			return(753);
		}
	}
	evtendcritical();
	if (channel->flags & COM_FLAGS_OPEN && serverkeepalive)
	{
		i2 = 1;
		if (setsockopt(channel->socket, SOL_SOCKET, SO_KEEPALIVE, (const void *) &i2, sizeof(i2))) {
			comerror(channel, 0, "Set KEEPALIVE FAILURE", errno);
			os_tcpserverclose(channel);
			return(753);
		}
	}
	return(0);
}

void os_tcpserverclose(CHANNEL *channel)
{
	INT retcode;
	CHANNEL *ch, *ch1, *ch2;

	os_tcpclear(channel, COM_CLEAR_SEND | COM_CLEAR_RECV, NULL);

 	evtstartcritical();
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
				channel->flags &= ~(COM_FLAGS_OPEN | /*COM_FLAGS_BOUND |*/ COM_FLAGS_LISTEN);
			}
			else {  /* should not happen, but support just in case */
				ch1->socket = channel->fdlisten;
				channel->flags &= ~COM_FLAGS_FDLISTEN;
			}
			ch1->flags |= COM_FLAGS_OPEN | /*COM_FLAGS_BOUND |*/ COM_FLAGS_LISTEN;
			retcode = WSAAsyncSelect(ch1->socket, winhnd, ch1->channeltableentry + WM_USER + 1,
					FD_READ | FD_WRITE | FD_ACCEPT | FD_CLOSE);
			if (retcode == SOCKET_ERROR) {
				comerror(ch1, COM_PER_ERROR, "WSAASYNCSELECT FAILURE", WSAGetLastError());
			}
			else {
				ch1->flags |= COM_FLAGS_SELECT;
				if (tcpserveraccept(ch1) < 0) {
					/* comerror called by tcpserveraccept */
					ch1->status |= COM_PER_ERROR;
					evtset(ch1->sendevtid);
					evtset(ch1->recvevtid);
				}
			}
		}
		else if (ch2 != NULL) {  /* found a server that is connected */
			if (channel->flags & COM_FLAGS_LISTEN) {
				ch2->fdlisten = channel->socket;
				channel->flags &= ~(COM_FLAGS_OPEN | /*COM_FLAGS_BOUND |*/ COM_FLAGS_LISTEN);
				ch2->flags |= COM_FLAGS_FDLISTEN;
			}
			else {
				ch2->fdlisten = channel->fdlisten;
				channel->flags &= ~COM_FLAGS_FDLISTEN;
				ch2->flags |= COM_FLAGS_FDLISTEN;
			}
		}
	}

	if (channel->flags & COM_FLAGS_FDLISTEN) closesocket(channel->fdlisten);
	if (channel->flags & COM_FLAGS_CONNECT) shutdown(channel->socket, SD_BOTH);
/*	if (channel->flags & COM_FLAGS_LISTEN) ; */
/*	if (channel->flags & COM_FLAGS_BOUND) ; */
	if (channel->flags & COM_FLAGS_SELECT) WSAAsyncSelect(channel->socket, winhnd, 0, 0);
	if (channel->flags & COM_FLAGS_OPEN) closesocket(channel->socket);
	if (channel->flags & COM_FLAGS_TABLE) channeltable[channel->channeltableentry] = NULL;
	channel->flags &= ~(COM_FLAGS_OPEN | /*COM_FLAGS_BOUND |*/ COM_FLAGS_SELECT
			| COM_FLAGS_LISTEN | COM_FLAGS_CONNECT | COM_FLAGS_FDLISTEN | COM_FLAGS_TABLE);
	evtendcritical();
}

INT os_tcpsend(CHANNEL *channel)
{
	INT retcode, timehandle;
	UCHAR timestamp[16];

	if (channel->status & COM_SEND_MASK) os_tcpclear(channel, COM_CLEAR_SEND, NULL);

	if (channel->status & COM_PER_ERROR) return(753);

	evtstartcritical();
	if (channel->flags & COM_FLAGS_CONNECT) {
		retcode = tcpsend(channel);
 		if (retcode <= 0) {  /* success or comerror w/ com_send_error called by tcpsend */
			evtendcritical();
			return(0);
		}
	}

	if (channel->sendtimeout == 0) {
		channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_TIME;
		evtset(channel->sendevtid);
		evtendcritical();
		return(0);
	}

	channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_PEND;
	if (channel->sendtimeout > 0) {
		pmsctimestamp(timestamp);
		if (timadd(timestamp, channel->sendtimeout * 100) ||
		   (timehandle = timset(timestamp, channel->sendevtid)) < 0) {
			comerror(channel, 0, "SET TIMER FAILURE", 0);
			evtendcritical();
			return(753);
		}
		if (!timehandle) {
			channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_TIME;
			evtset(channel->sendevtid);
			evtendcritical();
			return(0);
		}
		channel->sendth = timehandle;
	}
	evtendcritical();
	return(0);
}

INT os_tcprecv(CHANNEL *channel)
{
	INT retcode, timehandle;
	UCHAR timestamp[16];

	if (channel->status & COM_RECV_MASK) os_tcpclear(channel, COM_CLEAR_RECV, NULL);

	if (channel->status & COM_PER_ERROR) return(753);

	evtstartcritical();
	if (channel->flags & COM_FLAGS_CONNECT) {
		retcode = tcprecv(channel);
  		if (retcode <= 0) {  /* success or comerror w/ com_recv_error called by tcprecv */
			evtendcritical();
			return(0);
		}
	}

	if (channel->recvtimeout == 0) {
		channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_TIME;
		evtset(channel->recvevtid);
		evtendcritical();
		return(0);
	}

	channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_PEND;
	if (channel->recvtimeout > 0) {
		pmsctimestamp(timestamp);
		if (timadd(timestamp, channel->recvtimeout * 100) ||
		   (timehandle = timset(timestamp, channel->recvevtid)) < 0) {
			comerror(channel, 0, "SET TIMER FAILURE", 0);
			evtendcritical();
			return(753);
		}
		if (!timehandle) {
			channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_TIME;
			evtset(channel->recvevtid);
			evtendcritical();
			return(0);
		}
		channel->recvth = timehandle;
	}
	evtendcritical();
	return(0);
}

INT os_tcpclear(CHANNEL *channel, INT flag, INT *status)
{
	evtstartcritical();
	if (channel->status & COM_SEND_PEND) {
		if (channel->sendtimeout > 0 && evttest(channel->sendevtid)) {  /* timeout happened */
			channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_TIME;
		}
	}
	if (channel->status & COM_RECV_PEND) {
		if (channel->recvtimeout > 0 && evttest(channel->recvevtid)) {  /* timeout happened */
			channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_TIME;
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
	}
	if (flag & COM_CLEAR_RECV) {
		channel->status &= ~COM_RECV_MASK;
		if (channel->recvth) {
			timstop(channel->recvth);
			channel->recvth = 0;
		}
		evtclear(channel->recvevtid);
	}
	evtendcritical();
	return(0);
}

static CHAR* v6binary_to_HexColon(PIN6_ADDR addr)
{
	static CHAR v6string[8 * 4 + 7 + 1];
	CHAR hex[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
			'A', 'B', 'C', 'D', 'E', 'F'};
	INT i1, i2 = 0;
	for (i1 = 0; i1 < 8; i1++) {
		USHORT ushrt = addr->s6_words[i1];
		v6string[i2++] = hex[ushrt >> 12];
		v6string[i2++] = hex[(ushrt & 0x0F00) >> 8];
		v6string[i2++] = hex[(ushrt & 0x00F0) >> 4];
		v6string[i2++] = hex[ushrt & 0x000F];
		v6string[i2++] = ':';
	}
	v6string[--i2] = '\0';
	return v6string;
}

INT os_tcpctl(CHANNEL *channel, UCHAR *msgin, INT msginlen, UCHAR *msgout, INT *msgoutlen)
{
	INT i1, addrlen;
	TCHAR work[128], portWork[32], addrWork[64];
	SOCKADDR_STORAGE address;

	if (!(channel->flags & COM_FLAGS_OPEN)) {
		strcpy(work, "0 0");
		i1 = (INT)strlen(work);
		if (i1 > *msgoutlen) i1 = *msgoutlen;
		memcpy(msgout, work, i1);
		*msgoutlen = i1;
		return 0;
	}

	while (msginlen > 0 && msgin[msginlen - 1] == ' ' || msgin[msginlen - 1] == '\t') msginlen--;
	for (i1 = 0; i1 < msginlen && msgin[i1] != '=' && !isspace(msgin[i1]); i1++)
		work[i1] = (CHAR) toupper(msgin[i1]);
	work[i1] = '\0';

	addrlen = sizeof(address);
	if (!strcmp(work, "GETLOCALADDR"))  {
		i1 = getsockname(channel->socket, (PSOCKADDR) &address, &addrlen);
		if (i1 == SOCKET_ERROR) {
			comerror(channel, 0, "GETSOCKNAME FAILURE", WSAGetLastError());
			return(753);
		}
	}
	else if (!strcmp(work, "GETREMOTEADDR"))  {
		i1 = getpeername(channel->socket, (PSOCKADDR) &address, &addrlen);
		if (i1 == SOCKET_ERROR) {
			comerror(channel, 0, "GETPEERNAME FAILURE", WSAGetLastError());
			return(753);
		}
	}
	else {
		comerror(channel, 0, "INVALID COMCTL REQUEST", 0);
		return(753);
	}

	i1 = getAddrAndPortStrings(channel, &address, addrWork, ARRAYSIZE(addrWork),
			portWork, ARRAYSIZE(portWork));
	if (i1) return i1;
	i1 = sprintf_s(work, ARRAYSIZE(work), "%s %s", addrWork, portWork);

	if (i1 > *msgoutlen) i1 = *msgoutlen;
	memcpy(msgout, work, i1);
	*msgoutlen = i1;
	return(0);
}

static INT getAddrAndPortStrings(CHANNEL *channel, SOCKADDR_STORAGE* sockstore, CHAR *addrBuf,
		INT cchAddrBufLen, CHAR *portBuf, INT cchPortBufLen)
{
	CHAR addrWork[128];
	PCTSTR ptr;
	CHAR portWork[32];
	size_t i2 = ARRAYSIZE(addrWork);
	PSOCKADDR_IN sockstoreV4 = (PSOCKADDR_IN)sockstore;
	PSOCKADDR_IN6 sockstoreV6 = (PSOCKADDR_IN6)sockstore;

	if (os_InetNtop == NULL) { /* Will be NULL on any Windows earlier than Vista */
		if (sockstore->ss_family == AF_INET6) {
			ptr = v6binary_to_HexColon(&sockstoreV6->sin6_addr);
		}
		else {
			/* Use inet_ntoa, which does not do v6 */
			ptr = inet_ntoa(sockstoreV4->sin_addr);
			if (ptr == NULL) {
				comerror(channel, 0, "inet_ntoa FAILURE", WSAGetLastError());
				return(753);
			}
		}
		StringCchCopy(addrWork, i2, ptr);
	}
	else { /* On Vista and newer we can use inet_ntop which works with v4 or v6 */
		switch (sockstore->ss_family) {
			case AF_INET:
				ptr = os_InetNtop(AF_INET, &sockstoreV4->sin_addr, addrWork, i2);
				break;
			case AF_INET6:
				ptr = os_InetNtop(AF_INET6, &sockstoreV6->sin6_addr, addrWork, i2);
				break;
			default:
				comerror(channel, 0, "Internal Failure in os_tcpctl", sockstore->ss_family);
				return(753);
		}
		if (ptr == NULL) {
			comerror(channel, 0, "inet_ntop FAILURE", WSAGetLastError());
			return(753);
		}
	}
	StringCchCopy(addrBuf, cchAddrBufLen, addrWork);
	sprintf_s(portWork, ARRAYSIZE(portWork), "%hu",
			ntohs((sockstore->ss_family == AF_INET) ? sockstoreV4->sin_port : sockstoreV6->sin6_port));
	StringCchCopy(portBuf, cchPortBufLen, portWork);
	return 0;
}

static INT tcpserveraccept(CHANNEL *channel)
{
	INT retcode;
	DWORD lasterr;
	SOCKET fd, newfd;
	CHANNEL *ch;

	/* if error occurs, comerror is called on behalf of caller */
	fd = channel->socket;
	newfd = accept(fd, NULL, NULL);
	if (newfd == INVALID_SOCKET) {
		if ((lasterr = WSAGetLastError()) == WSAEWOULDBLOCK) {
			return(1);
		}
		comerror(channel, 0, "ACCEPT FAILURE", lasterr);
		return RC_ERROR;
	}

	retcode = WSAAsyncSelect(newfd, winhnd, channel->channeltableentry + WM_USER + 1, FD_READ | FD_WRITE | FD_CLOSE);
	if (retcode == SOCKET_ERROR) {
		comerror(channel, 0, "WSAASYNCSELECT FAILURE", WSAGetLastError());
		closesocket(newfd);
		return RC_ERROR;
	}
	channel->socket = newfd;
	channel->flags = (channel->flags & ~COM_FLAGS_LISTEN) | COM_FLAGS_CONNECT;

	for (ch = getchannelhead(); ch != NULL; ch = ch->nextchannelptr) {
		if (!ISCHANNELTYPETCPSERVER(ch) || ch->ourport != channel->ourport || ch == channel) continue;
		if (!(ch->flags & COM_FLAGS_OPEN)) break;
	}
	if (ch == (CHANNEL *) NULL) {  /* no other servers with same port number */
		WSAAsyncSelect(fd, winhnd, 0, 0);
		channel->fdlisten = fd;
		channel->flags |= COM_FLAGS_FDLISTEN;
	}
	else {  /* found another server with same port number */
		ch->socket = fd;
		ch->flags |= COM_FLAGS_OPEN | /*COM_FLAGS_BOUND |*/ COM_FLAGS_LISTEN;
		retcode = WSAAsyncSelect(ch->socket, winhnd, ch->channeltableentry + WM_USER + 1, FD_READ | FD_WRITE | FD_ACCEPT | FD_CLOSE);
		if (retcode == SOCKET_ERROR) {
			comerror(ch, COM_PER_ERROR, "WSAASYNCSELECT FAILURE", WSAGetLastError());
			return(0);
		}
		ch->flags |= COM_FLAGS_SELECT;
		if (tcpserveraccept(ch) < 0) {
			/* comerror called by tcpserveraccept */
			ch->status |= COM_PER_ERROR;
			evtset(ch->sendevtid);
			evtset(ch->recvevtid);
			return(0);  /* new server errored, not the old server */
		}
	}
	return(0);
}

/*
  This is called from os_tcpsend and udptcpcallback.
  In both of those cases, it is inside a evtstartcritical()
  Returns 1 if WOULDBLOCK, -1 if error, 0 if success
*/
static INT tcpsend(CHANNEL *channel)
{
	INT errval, len, retcode;

	/* if error occurs, comerror w/ com_send_error is called on behalf of caller */
	for ( ; ; ) {
		len = channel->sendlength - channel->senddone;
		retcode = send(channel->socket, (CHAR *) channel->sendbuf + channel->senddone, len, 0);
		if (retcode == SOCKET_ERROR) {
			if ((errval = WSAGetLastError()) == WSAEWOULDBLOCK) return(1);
			comerror(channel, COM_SEND_ERROR, "SEND FAILURE", errval);
			return RC_ERROR;
		}
		if (retcode >= len) {
			channel->status &= ~COM_SEND_MASK;
			channel->status |= COM_SEND_DONE;
			evtset(channel->sendevtid);
			return(0);
		}
		channel->senddone += retcode;
	}
}

/**
 * This is called from os_tcprecv and udptcpcallback.
 * In both of those cases, it is inside a evtstartcritical()
 */
static INT tcprecv(CHANNEL *channel)
{
	INT errval, retcode;

	/* if error occurs, comerror w/ com_recv_error is called on behalf of caller */
	retcode = recv(channel->socket, (CHAR *) channel->recvbuf, channel->recvlength, 0);
	if (retcode == SOCKET_ERROR) {
		if ((errval = WSAGetLastError()) == WSAEWOULDBLOCK) {
			return(1);
		}
/*** CODE: IS THERE AN ERROR FOR MESSAGES LONGER THEN RECVLENGTH ? ***/
		comerror(channel, COM_RECV_ERROR, "RECV FAILURE", errval);
		return RC_ERROR;
	}
	if (retcode == 0) {  /* assume eof / disconnect */
		comerror(channel, COM_RECV_ERROR, "DISCONNECT", -1);
		if (channel->status & COM_SEND_PEND) comerror(channel, COM_SEND_ERROR, "DISCONNECT", -2);
		comerror(channel, COM_PER_ERROR, "DISCONNECT", -3);
		return RC_ERROR;
	}
	channel->recvdone = retcode;
	channel->status &= ~COM_RECV_MASK;
	channel->status |= COM_RECV_DONE;
	evtset(channel->recvevtid);   
	return(0);
}

/**
 * Returns 0 if success, RC_ERROR if failure after calling comerror
 *
 */
static INT udptcpinit(CHANNEL *channel)
{
	INT eventid, retcode, timeid, eventarray[2];
	DWORD threadid;
	WSADATA wsaData;
	TCHAR timerwork[17];

	retcode = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (retcode != 0) {
		comerror(channel, 0, "WSASTARTUP FAILURE", retcode);
		return RC_ERROR;
	}

	channel->errormsg[0] = '\0';
	winevent = evtcreate();
	udptcpthreadhandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) udptcpthread, (LPVOID) channel, 0, &threadid);
	if (udptcpthreadhandle == NULL) {
		comerror(channel, 0, "CREATETHREAD FAILURE", GetLastError());
		return RC_ERROR;
	}

	if (waitForTCPConnect) {
		ghTCPClientConnect = CreateEvent(
			NULL,               // default security attributes
			TRUE,               // manual-reset event
			FALSE,              // initial state is nonsignaled
			"TCPClientConnectEvent"  // object name
		);
		if (ghTCPClientConnect == NULL)
		{
			comerror(channel, 0, "CREATEEVENT FAILURE", GetLastError());
			return RC_ERROR;
	    }
	}
	eventarray[0] = winevent;
	eventarray[1] = evtcreate();
	pmsctimestamp(timerwork);
	timadd(timerwork, 400);
	timeid = timset(timerwork, eventarray[1]);
	eventid = evtwait(eventarray, 2);
	timstop(timeid);
	evtdestroy(eventarray[1]);
	evtdestroy(winevent);
	if (eventid != 0) {  /* timeout or error */
		TerminateThread(udptcpthreadhandle, 0);
/*** CODE: MAYBE DESTROY WINEVENT HERE INSTEAD ***/
		if (eventid != 1) comerror(channel, 0, "EVTWAIT IN UDPTCPINIT FAILED", 0);
		else if (!channel->errormsg[0]) comerror(channel, 0, "WAIT FOR UDPTCPTHREAD FAILED", 0);
		return RC_ERROR;
	}

	udptcpinitflg = TRUE;
	return(0);
}

static void udptcpthread(LPVOID channel)
{
	MSG msg;
	CHAR window[] = "SOCKWIN";

	winhnd = CreateWindow((LPCSTR)winclass, (LPCSTR)window,
				0, CW_USEDEFAULT, CW_USEDEFAULT,
				CW_USEDEFAULT, CW_USEDEFAULT,
				NULL, NULL, hinst, NULL);
	if (winhnd == NULL) {
		comerror((CHANNEL *) channel, 0, "CREATEWINDOW FAILURE", GetLastError());
		ExitThread(1);
	}

	evtset(winevent);
	while (GetMessage(&msg, NULL, 0, 0)) {
		DispatchMessage(&msg);
	}
	winhnd = NULL;
	ExitThread(0);
}

/* UDPTCPPROC */
LRESULT CALLBACK udptcpcallback(HWND hwnd, UINT nmsg, WPARAM wParam, LPARAM lParam)
{
	INT addrlen, retcode, errval;
	DWORD lasterr;
	CHANNEL *channel;
	SOCKADDR_STORAGE raddr;
	WORD wsaEvent;

	if (nmsg > WM_USER && (INT) nmsg <= channeltablesize + WM_USER) {
		evtstartcritical();
		channel = channeltable[nmsg - WM_USER - 1];
		if (channel == NULL || !(channel->flags & COM_FLAGS_OPEN) || channel->socket != (SOCKET) wParam) {
			evtendcritical();
			return(0);
		}
		wsaEvent = WSAGETSELECTEVENT(lParam);
		if ( (errval = WSAGETSELECTERROR(lParam)) ) {
			if (errval == WSAECONNABORTED || errval == WSAECONNRESET) {
				if (channel->status & COM_SEND_PEND) comerror(channel, COM_SEND_ERROR, "DISCONNECT", errval);
				if (channel->status & COM_RECV_PEND) comerror(channel, COM_RECV_ERROR, "DISCONNECT", errval);
				comerror(channel, COM_PER_ERROR, "DISCONNECT", errval);
			}
			else {
				comerror(channel, COM_PER_ERROR, "WSAGETSELECTERROR", errval);
			}
			evtendcritical();
			if (waitForTCPConnect && wsaEvent == FD_CONNECT && ISCHANNELTYPETCPCLIENT(channel)) {
				// os_tcpclientopen s/b waiting, signal it
				if (!SetEvent(ghTCPClientConnect) )
				{
					comerror(channel, COM_PER_ERROR, "SetEvent", GetLastError());
					return 0;
				}
			}
			return(0);
		}
		switch (wsaEvent) {
		case FD_READ:
			if (ISCHANNELTYPEUDP(channel)) {
				if (channel->status & COM_RECV_PEND) {
					addrlen = sizeof(raddr);
					retcode = recvfrom(channel->socket, (CHAR *) channel->recvbuf,
							channel->recvlength, 0, (PSOCKADDR)&raddr, &addrlen);
					if (retcode == SOCKET_ERROR) {
						if ((lasterr = WSAGetLastError()) == WSAEWOULDBLOCK) ;
						else if (lasterr == WSAEMSGSIZE) {
							channel->recvdone = channel->recvlength;
							channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_DONE;
						}
						else comerror(channel, COM_RECV_ERROR, "RECVFROM FAILURE", lasterr);
					}
					else {
						channel->recvdone = retcode;
						channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_DONE;
					}

					if (channel->status & COM_RECV_DONE) {
						switch (raddr.ss_family) {
						case PF_INET:
						case PF_INET6:
							channel->recvaddr = raddr;
							break;
						default:
							comerror(channel, COM_RECV_ERROR, "RECVFROM ADDRESS FAMILY NOT PF_INETx", 0);
						}
					}

					if (!(channel->status & COM_RECV_PEND)) evtset(channel->recvevtid);
				}
			}
			else if (ISCHANNELTYPETCPSERVER(channel) || ISCHANNELTYPETCPCLIENT(channel)) {
				if (channel->flags & COM_FLAGS_CONNECT) {
					if (channel->status & COM_RECV_PEND) tcprecv(channel);
				}
			}
			break;
		case FD_WRITE:
			if (ISCHANNELTYPEUDP(channel)) {
				if (channel->status & COM_SEND_PEND) {
					addrlen = sizeof(SOCKADDR_IN);
					retcode = sendto(channel->socket, (CHAR *) channel->sendbuf, channel->sendlength,
							0, (PSOCKADDR)&channel->sendaddr, addrlen);
					if (retcode == SOCKET_ERROR) {
						if ((lasterr = WSAGetLastError()) == WSAEWOULDBLOCK) ;
						else if (lasterr == WSAEMSGSIZE)
							comerror(channel, COM_SEND_ERROR, "SEND MESSAGE TOO LONG", 0);
						else comerror(channel, COM_SEND_ERROR, "SENDTO FAILURE", lasterr);
					}
					else if (retcode != channel->sendlength)
						comerror(channel, COM_SEND_ERROR, "BYTES SENT NOT EQUAL TO BYTES REQUESTED", 0);
					else channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_DONE;

					if (!(channel->status & COM_SEND_PEND)) evtset(channel->sendevtid);
				}
			}
			else if (ISCHANNELTYPETCPSERVER(channel) || ISCHANNELTYPETCPCLIENT(channel)) {
				if (channel->flags & COM_FLAGS_CONNECT) {
					if (channel->status & COM_SEND_PEND) tcpsend(channel);
				}
			}
			break;
		case FD_ACCEPT:
			if (ISCHANNELTYPETCPSERVER(channel)) {
				if (channel->flags & COM_FLAGS_LISTEN) {
					retcode = tcpserveraccept(channel);
					if (retcode < 0) {
						/* comerror called by tcpserveraccept */
						channel->status |= COM_PER_ERROR;
						evtset(channel->sendevtid);
						evtset(channel->recvevtid);
					}
				}
			}
			break;
		case FD_CONNECT:
			if (ISCHANNELTYPETCPCLIENT(channel)) {
				channel->flags |= COM_FLAGS_CONNECT;
				if (waitForTCPConnect) {
					// os_tcpclientopen s/b waiting, signal it
					if (!SetEvent(ghTCPClientConnect) )
					{
						comerror(channel, COM_PER_ERROR, "SetEvent", GetLastError());
					}
				}
			}
			break;
		case FD_CLOSE:
			if (channel->status & COM_SEND_PEND) comerror(channel, COM_SEND_ERROR, "DISCONNECT", -4);
			if (channel->status & COM_RECV_PEND) comerror(channel, COM_RECV_ERROR, "DISCONNECT", -5);

#if 0		/*Removing the next line solves a problem with the FTP protocol, does not seem
			to introduce any new problems.*/
			comerror(channel, COM_PER_ERROR, "DISCONNECT", 0);
#endif
			break;
		}
		evtendcritical();
	}
	else if (nmsg == WM_USER) {
		DestroyWindow(winhnd);
		winhnd = NULL;
		ExitThread(0);
	}
	else return(DefWindowProc(hwnd, nmsg, wParam, lParam));
	return(0);
}

/* shared communication routines */

/*
 * As of 04/01/11, after making tcp v6 ready, this routine
 * is used by udp only.
 *
 * Returns 0 if success
 * Returns RC_ERROR if the syntax is bad
 * Returns 753 and comerror called if system call failure
 */
static INT setSendAddressInChannel(CHAR *nameptr, CHANNEL *channel)
{
	CHAR *portptr, *ptr, *terminator;
	ADDRESS_FAMILY addrFamily;
	ADDRINFO hints, *AddrInfo;
	INT retcode;
	CHAR work[128];
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

	//TODO Test the heck out of this
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = addrFamily;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	if ( (retcode = GetAddrInfo(nameptr, portptr, &hints, &AddrInfo) != 0) ) {
		sprintf_s(work, ARRAYSIZE(work), "GetAddrInfo - %s", gai_strerror(retcode));
		comerror(channel, COM_SEND_ERROR, work, retcode);
		return(753);
	}
	if (AddrInfo->ai_family == AF_INET6) {
		((PSOCKADDR_IN6)AddrInfo->ai_addr)->sin6_port = htons(numericPort);
	}
	else {
		((PSOCKADDR_IN)AddrInfo->ai_addr)->sin_port = htons(numericPort);
	}
	memcpy(&channel->sendaddr, AddrInfo->ai_addr, AddrInfo->ai_addrlen);

	freeaddrinfo(AddrInfo);
	return(0);
}

/**
 * Sets channel->channeltableentry to the index into the static channel table
 * Turns on flag COM_FLAGS_TABLE
 *
 * Returns 0 if success. RC_NO_MEM is only possible error, comerror will have been called
 */
static INT getchanneltableentry(CHANNEL *channel)
{
	INT i1;
	CHANNEL **channelptr;

	for (i1 = 0; i1 < channeltablesize && channeltable[i1] != 0; i1++);
	if (i1 == channeltablesize) {
		if (!i1) channelptr = (CHANNEL **) malloc(128 * sizeof(CHANNEL *));
		else channelptr = (CHANNEL **) realloc(channeltable, (channeltablesize + 128) * sizeof(CHANNEL *));
		channeltable = channelptr;
		if (channelptr == NULL) { 
			comerror(channel, COM_PER_ERROR, "CHANNELTABLE ALLOCATION FAILURE", 0);
			return RC_NO_MEM;
		}
		ZeroMemory(&channeltable[channeltablesize], 128 * sizeof(CHANNEL *));
		channeltablesize += 128;
	}
	channel->channeltableentry = i1;
	channeltable[i1] = channel;
	channel->flags |= COM_FLAGS_TABLE;
	return 0;
}

static void comerror(CHANNEL *channel, INT type, CHAR *str, INT err)
{
	size_t i1, i2;
	CHAR work[32], *ptr;

	if (type == COM_SEND_ERROR) ptr = channel->senderrormsg;
	else if (type == COM_RECV_ERROR) ptr = channel->recverrormsg;
	else ptr = channel->errormsg;

	StringCbLength(str, ERRORMSGSIZE + 1, &i1);  // Ensures that i1 cannot be too large
	//if (i1 > ERRORMSGSIZE) i1 = ERRORMSGSIZE;
	StringCbCopy(ptr, ERRORMSGSIZE + 1, str);
	if (err) {
		i2 = sprintf_s(work, ARRAYSIZE(work), ", ERROR = %d", err);
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
	}
}

/**
 * Used only on Win XP for TCP client
 */
static INT ipstrtoaddr(CHAR *channelname, UINT *address)
{
	struct hostent *host;
	struct in_addr *ip;

	if (isdigit(*channelname)) *address = inet_addr(channelname);
	else {
		if ((host = gethostbyname(channelname)) == NULL) return(-1);
		ip = (struct in_addr *) host->h_addr;
		*address = (UINT) ip->s_addr;
	}
	return(0);
}

