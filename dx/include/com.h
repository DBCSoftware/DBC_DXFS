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

#ifndef _COM_H_INCLUDED
#define _COM_H_INCLUDED 1

/* channel type */
#define CHANNELTYPE_UDP		1
#define CHANNELTYPE_UDP4	8
#define CHANNELTYPE_UDP6	9
/* Allow either IPv4 or IPv6 IP addresses */
#define CHANNELTYPE_TCPCLIENT	2
#define CHANNELTYPE_TCPSERVER	3
#define CHANNELTYPE_SERIAL	4
/* Limit to IPv4 IP addresses */
#define CHANNELTYPE_TCPCLIENT4	10
#define CHANNELTYPE_TCPSERVER4	5
/* Limit to IPv6 IP addresses */
#define CHANNELTYPE_TCPCLIENT6	11
#define CHANNELTYPE_TCPSERVER6	6
#define CHANNELTYPE_OTHER	7

/* status flags */
#define COM_SEND_PEND	0x00000001
#define COM_SEND_DONE	0x00000002
#define COM_SEND_TIME	0x00000004
#define COM_SEND_ERROR	0x00000008
#define COM_RECV_PEND	0x00000010
#define COM_RECV_DONE	0x00000020
#define COM_RECV_TIME	0x00000040
#define COM_RECV_ERROR	0x00000080
#define COM_PER_ERROR	0x00010000

/* status flag masks */
#define COM_SEND_MASK	0x0000000F
#define COM_RECV_MASK	0x000000F0

#if OS_WIN32
#include "xml.h"
#define DllExport   __declspec( dllexport )
typedef struct dllinitstruct {
	INT (*evtcreate)(void);
	void (*evtdestroy)(INT);
	void (*evtset)(INT);
	void (*evtclear)(INT);
	INT (*evttest)(INT);
	INT (*evtwait)(INT *, INT);
	void (*evtstartcritical)(void);
	void (*evtendcritical)(void);
	INT (*timset)(UCHAR *, INT);
	void (*timstop)(INT);
	INT (*timadd)(UCHAR *, INT);
	void (*msctimestamp)(UCHAR *);
	ELEMENT* (*cfggetxml)(void);
} DLLSTRUCT;
#else
#define DllExport
#endif

#if OS_WIN32 & !defined(COMDLL)
typedef void (_DLLINIT)(DLLSTRUCT *);
_DLLINIT *dllinit;
typedef INT (_COMINIT)(void);
_COMINIT *cominit;
typedef INT (_COMEXIT)(void);
_COMINIT *comexit;
typedef INT (_COMOPEN)(CHAR *, INT *);
_COMOPEN *comopen;
typedef INT (_COMCLOSE)(INT);
_COMCLOSE *comclose;
typedef INT (_COMSEND)(INT, INT, INT, UCHAR *, INT);
_COMSEND *comsend;
typedef INT (_COMRECV)(INT, INT, INT, INT);
_COMRECV *comrecv;
typedef INT (_COMRECVGET)(INT, UCHAR *, INT *);
_COMRECVGET *comrecvget;
typedef INT (_COMCLEAR)(INT, INT *);
_COMCLEAR *comclear;
typedef INT (_COMRCLEAR)(INT, INT *);
_COMRCLEAR *comrclear;
typedef INT (_COMSCLEAR)(INT, INT *); 
_COMSCLEAR *comsclear; 
typedef INT (_COMSTAT)(INT, INT *);
_COMSTAT *comstat;
typedef INT (_COMCTL)(INT, UCHAR *, INT, UCHAR *, INT *);
_COMCTL *comctl;
#else
extern DllExport INT cominit(void);
extern DllExport void comexit(void);
extern DllExport INT comopen(CHAR *, INT *);
extern DllExport INT comclose(INT);
extern DllExport INT comsend(INT, INT, INT, UCHAR *, INT);
extern DllExport INT comrecv(INT, INT, INT, INT);
extern DllExport INT comrecvget(INT, UCHAR *, INT *);
extern DllExport INT comclear(INT, INT *);
extern DllExport INT comrclear(INT, INT *);
extern DllExport INT comsclear(INT, INT *);
extern DllExport INT comstat(INT, INT *);
extern DllExport INT comctl(INT, UCHAR *, INT, UCHAR *, INT *);
#if OS_WIN32
extern DllExport void dllinit(DLLSTRUCT *);
#endif
#endif

#endif //_COM_H_INCLUDED
