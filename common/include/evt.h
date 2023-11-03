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

#ifndef _EVT_INCLUDED
#define _EVT_INCLUDED

#define EVT_MAX_EVENTS 128
#define EVTBASE 953920

#if OS_UNIX
/* types returned by evtdevtype */
#define DEVTYPE_STRM 1
#define DEVTYPE_POLL 2
#define DEVTYPE_NULL 3
#define DEVTYPE_FILE 4
#define DEVTYPE_OTHR 5
#define DEVTYPE_MAX  5

#define DEVPOLL_READ			0x0001
#define DEVPOLL_READNORM		0x0002
#define DEVPOLL_READBAND		0x0004
#define DEVPOLL_READPRIORITY	0x0008
#define DEVPOLL_WRITE			0x0010
#define DEVPOLL_WRITENORM		0x0020
#define DEVPOLL_WRITEBAND		0x0040
#define DEVPOLL_ERROR			0x0100
#define DEVPOLL_HANGUP			0x0200
#define DEVPOLL_NOFDES			0x0400
#define DEVPOLL_BLOCK			0x1000
#define DEVPOLL_NOBLOCK			0x2000
#endif

extern INT evtcreate(void);
extern void evtdestroy(INT);
extern void evtclear(INT);
extern void evtset(INT);
extern INT evttest(INT);
extern INT evtwait(INT *, INT);
extern void evtmultiplex(INT *, INT, INT *, INT);
extern INT evtregister(void (*)(void));
extern void evtunregister(void (*)(void));
extern void evtactionflag(INT *, INT);
extern void evtpoll(void);
extern INT evtgeterror(CHAR *errorstr, INT size);
extern INT evtputerror(CHAR *errorstr);

#if OS_UNIX
extern INT evtdevinit(INT, INT, INT (*)(void *, INT), void *);
extern INT evtdevexit(INT);
extern INT evtdevstop(INT);
extern INT evtdevstart(INT);
extern INT evtdevset(INT, INT);
extern INT evtdevtype(INT, INT *);
#endif

#endif  /* _EVT_INCLUDED */
