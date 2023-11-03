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
#define INC_STRING
#define INC_STDLIB
#define INC_CTYPE
#define INC_TIME
#define INC_SIGNAL
#define INC_ERRNO
#include "includes.h"

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <poll.h>

#ifndef S_IFIFO
#define S_IFIFO 0x1000
#endif
#ifndef S_IFCHR
#define S_IFCHR 0x2000
#endif
#ifndef S_IFDIR
#define S_IFDIR 0x4000
#endif
#ifndef S_IFBLK
#define S_IFBLK 0x6000
#endif
#ifndef S_IFREG
#define S_IFREG 0x8000
#endif
#ifndef S_IFMT
#define S_IFMT  0xF000
#endif

#ifndef S_ISFIFO
#define S_ISFIFO(mode) ((mode & S_IFMT) == S_IFIFO)
#endif
#ifndef S_ISCHR
#define S_ISCHR(mode)  ((mode & S_IFMT) == S_IFCHR)
#endif
#ifndef S_ISDIR
#define S_ISDIR(mode)  ((mode & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISBLK
#define S_ISBLK(mode)  ((mode & S_IFMT) == S_IFBLK)
#endif
#ifndef S_ISREG
#define S_ISREG(mode)  ((mode & S_IFMT) == S_IFREG)
#endif

#ifdef POLLIN
#ifndef POLLRDNORM
#define POLLRDNORM 0
#endif
#ifndef POLLRDBAND
#define POLLRDBAND 0
#endif
#ifndef POLLPRI
#define POLLPRI 0
#endif
#ifndef POLLOUT
#define POLLOUT 0
#endif
#ifndef POLLWRNORM
#define POLLWRNORM 0
#endif
#ifndef POLLWRBAND
#define POLLWRBAND 0
#endif
#ifndef POLLERR
#define POLLERR 0
#endif
#ifndef POLLHUP
#define POLLHUP 0
#endif
#ifndef POLLNVAL
#define POLLNVAL 0
#endif
#endif

#ifndef INFTIM
#define INFTIM -1
#endif

#include "base.h"
#include "evt.h"

/*** needed for DGUX ***/
#if defined(NOSIGPOLL) && defined(SIGPOLL)
#undef SIGPOLL
#endif

typedef struct {
	INT typeflg;
	INT handle;
	INT polltype;
#ifdef POLLIN
	INT events;
#endif
	INT (*cbfnc)(void *, INT);
	void *arg;
} DEVICEINFO;

#define STATE_AVAIL 0x00  /* event flag is available */
#define STATE_CLEAR	0x01  /* event flag is allocated and clear */
#define STATE_SET	0x02  /* event flag is allocated and set */

#define REGMAX 1

#define DEVTYPE_STOP 0x80

#define DEVICEMAX 16

#define ACTION_COUNT	0x0080  /* values below this are reserved */
#define ACTION_REGCNT	0x1000
#define ACTION_SIGPOLL	0x2000
#define ACTION_POLL		0x4000
#define ACTION_RETRY	0x8000

static INT state[EVT_MAX_EVENTS];
static INT eventroof;
static INT multiplex[EVT_MAX_EVENTS];
static INT *multiplexflagptr;
static INT multiplexflagbit;
static INT eventflg;
static INT *actionflagptr = NULL;
static INT actionflagbit;
static INT actionflag;

static UINT devhi = 0;
static DEVICEINFO device[DEVICEMAX];
static INT devtypecnt[DEVTYPE_MAX + 1];  /* first element is not used */ // @suppress("Symbol is not resolved")

static INT sigflg = 0x00;
#if defined(SA_RESTART) || defined(SA_INTERRUPT) || defined(SA_RESETHAND)
static struct sigaction oldsigpoll;
static struct sigaction oldsigpipe;
#else
static void (*oldsigpoll)(INT);
static void (*oldsigpipe)(INT);
#endif

#ifdef POLLIN
/* poll variables */
static UINT pollcnt, polltab[DEVICEMAX];
static struct pollfd pollttys[DEVICEMAX];
static INT pollbuild;
static INT pollwait;
#else
#if defined(FD_ZERO) && defined(FD_SET) && defined(FD_ISSET)
static struct timeval zerotime = { 0, 0 };
static struct timeval *selectwait;
#endif
#endif
static CHAR evterrorstring[256];

static void evterror(CHAR *, INT);
static void sigevent(INT);
#if defined(SIGPOLL) && defined(I_SETSIG)
static INT setpollbits(INT);
#endif

INT evtcreate()
{
	INT i1;

	*evterrorstring = 0;
	for (i1 = 0; i1 < eventroof && state[i1] != STATE_AVAIL; i1++);
	if (i1 == EVT_MAX_EVENTS) {
		evtputerror("exceeding maximum number of events");
		return -1;
	}
	state[i1] = STATE_CLEAR;
	if (i1 >= eventroof) eventroof = i1 + 1;
	return EVTBASE + i1;
}

void evtdestroy(INT evtid)
{
	INT i1;

	i1 = evtid - EVTBASE;
	if (i1 < 0 || i1 >= EVT_MAX_EVENTS) return;
	state[i1] = STATE_AVAIL;
	multiplex[i1] = FALSE;
	while (eventroof-- && state[eventroof] == STATE_AVAIL);
	eventroof++;
}

void evtclear(INT evtid)
{
	INT i1;

	i1 = evtid - EVTBASE;
	if (i1 < 0 || i1 >= EVT_MAX_EVENTS) return;
	if (state[i1] != STATE_AVAIL) state[i1] = STATE_CLEAR;
	if (multiplex[i1] && (*multiplexflagptr & multiplexflagbit)) {
		for (i1 = 0; i1 < eventroof; i1++) {
			if (multiplex[i1] && state[i1] == STATE_SET) return;
		}
		if (i1 == eventroof) *multiplexflagptr &= ~multiplexflagbit;
	}
}

void evtset(INT evtid)
{
	INT i1;

	i1 = evtid - EVTBASE;
	if (i1 < 0 || i1 >= EVT_MAX_EVENTS) return;
	if (state[i1] != STATE_AVAIL) state[i1] = STATE_SET;
	if (multiplex[i1]) *multiplexflagptr |= multiplexflagbit;
	eventflg = TRUE;
#ifdef POLLIN
	pollwait = 0;
#else
#if defined(FD_ZERO) && defined(FD_SET) && defined(FD_ISSET)
	selectwait = &zerotime;
#endif
#endif
}

INT evttest(INT evtid)
{
	INT i1;

	i1 = evtid - EVTBASE;
	if (i1 < 0 || i1 >= EVT_MAX_EVENTS || state[i1] != STATE_SET) return 0;
	return 1;
}

void evtmultiplex(INT *evtidarray, INT count, INT *flagptr, INT flagbit)
{
	INT i1;

	for (i1 = 0; i1 < eventroof; ) multiplex[i1++] = FALSE;
	if (count < 1) return;
	multiplexflagptr = flagptr;
	multiplexflagbit = flagbit;
	*multiplexflagptr &= ~multiplexflagbit;
	while (count--) {
		if (!evtidarray[count]) continue;
		i1 = evtidarray[count] - EVTBASE;
		if (i1 < 0 || i1 >= EVT_MAX_EVENTS || state[i1] == STATE_AVAIL) continue;
		multiplex[i1] = TRUE;
		if (state[i1] == STATE_SET) *multiplexflagptr |= multiplexflagbit;
	}
}

#if 0
INT evtregister(void (*cbfnc)(void))
{
	INT i1;

	*evterrorstring = 0;
	for (i1 = 0; i1 < regcnt && regcbfnc[i1] != NULL; i1++);
	if (i1 == REGMAX) {
		evtputerror("exceeding maximum number of callbacks");
		return -1;
	}

	regcbfnc[i1] = cbfnc;
	if (i1 == regcnt) regcnt++;
	actionflag |= ACTION_REGCNT;
	if (actionflagptr != NULL) *actionflagptr |= actionflagbit;
	return 0;
}

void evtunregister(void (*cbfnc)(void))
{
	INT i1;

	for (i1 = 0; i1 < regcnt; i1++) {
		if (regcbfnc[i1] == cbfnc) {
			regcbfnc[i1] = NULL;
			break;
		}
	}
	while (regcnt && regcbfnc[regcnt - 1] == NULL) regcnt--;
	if (!regcnt) actionflag &= ~ACTION_REGCNT;
}
#endif

void evtactionflag(INT *flagptr, INT flagbit)
{
	if (actionflagptr != NULL) *actionflagptr &= ~actionflagbit;
	actionflagptr = flagptr;
	actionflagbit = flagbit;
	if (actionflagptr != NULL && (actionflag & (ACTION_REGCNT | ACTION_SIGPOLL | ACTION_POLL | ACTION_RETRY))) *actionflagptr |= actionflagbit;
}

void evtpoll()
{
	INT i2, events, revents;
	UINT i1;
#if !defined(POLLIN) && defined(FD_ZERO) && defined(FD_SET) && defined(FD_ISSET)
	INT selectcnt;
	fd_set readset, writeset, errorset;
#endif

	if (!(++actionflag & (ACTION_REGCNT | ACTION_COUNT | ACTION_SIGPOLL))) return;
	if (actionflag & (ACTION_COUNT | ACTION_SIGPOLL)) {
		actionflag &= ~(ACTION_COUNT | ACTION_SIGPOLL | ACTION_RETRY);
		if (devtypecnt[DEVTYPE_OTHR] || devtypecnt[DEVTYPE_FILE]) {
			for (i1 = 0; i1 < devhi; i1++) {
				if ((device[i1].typeflg != DEVTYPE_OTHR && device[i1].typeflg != DEVTYPE_FILE) || !device[i1].polltype) continue;
				if ((i2 = device[i1].cbfnc(device[i1].arg, DEVPOLL_NOBLOCK)) == -1) actionflag |= ACTION_RETRY;
				else if (i2 == 1) {  /* EOF */
					devtypecnt[device[i1].typeflg]--;
					device[i1].typeflg = DEVTYPE_NULL;
					devtypecnt[DEVTYPE_NULL]++;
/*** CODE: MAY WANT TO BLOCK SIGNALS DURING THIS NEXT LINE ***/
					if (!devtypecnt[DEVTYPE_POLL] && !devtypecnt[DEVTYPE_OTHR]) actionflag &= ~ACTION_POLL;
				}
			}
		}
		if (devtypecnt[DEVTYPE_STRM] || devtypecnt[DEVTYPE_POLL]) {
#ifdef POLLIN
			if (pollbuild) {  /* rebuild poll table */
				for (i1 = 0, pollcnt = 0; i1 < devhi; i1++) {
					if (device[i1].typeflg != DEVTYPE_STRM && device[i1].typeflg != DEVTYPE_POLL) continue;
					events = device[i1].polltype;
					revents = 0;
					if (events & DEVPOLL_READ) revents |= POLLIN;
					if (events & DEVPOLL_READNORM) revents |= POLLRDNORM;
					if (events & DEVPOLL_READBAND) revents |= POLLRDBAND;
					if (events & DEVPOLL_READPRIORITY) revents |= POLLPRI;
					if (events & DEVPOLL_WRITE) revents |= POLLOUT;
					if (events & DEVPOLL_WRITENORM) revents |= POLLWRNORM;
					if (events & DEVPOLL_WRITEBAND) revents |= POLLWRBAND;
					if (events & DEVPOLL_ERROR) revents |= POLLERR;
					if (events & DEVPOLL_HANGUP) revents |= POLLHUP;
					if (events & DEVPOLL_NOFDES) revents |= POLLNVAL;
					if (!(device[i1].events = revents)) continue;
					pollttys[pollcnt].fd = device[i1].handle;
					pollttys[pollcnt].events = device[i1].events & ~(POLLERR | POLLHUP | POLLNVAL);
					pollttys[pollcnt].revents = 0;
					polltab[pollcnt++] = i1;
				}
				pollbuild = FALSE;
			}
			if (pollcnt && poll(pollttys, pollcnt, 0) > 0) {
				for (i1 = 0; i1 < pollcnt; i1++) {
					if ( ( events = (device[polltab[i1]].events & pollttys[i1].revents) ) ) {
						revents = 0;
						if (events & POLLIN) revents |= DEVPOLL_READ;
						if (events & POLLRDNORM) revents |= DEVPOLL_READNORM;
						if (events & POLLRDBAND) revents |= DEVPOLL_READBAND;
						if (events & POLLPRI) revents |= DEVPOLL_READPRIORITY;
						if (events & POLLOUT) revents |= DEVPOLL_WRITE;
						if (events & POLLWRNORM) revents |= DEVPOLL_WRITENORM;
						if (events & POLLWRBAND) revents |= DEVPOLL_WRITEBAND;
						if (events & POLLERR) revents |= DEVPOLL_ERROR;
						if (events & POLLHUP) revents |= DEVPOLL_HANGUP;
						if (events & POLLNVAL) revents |= DEVPOLL_NOFDES;
						if ((i2 = device[polltab[i1]].cbfnc(device[polltab[i1]].arg, revents)) == -1) actionflag |= ACTION_RETRY;
						else if (i2 == 1) {
							devtypecnt[device[i1].typeflg]--;
							device[i1].typeflg = DEVTYPE_NULL;
							devtypecnt[DEVTYPE_NULL]++;
/*** CODE: MAY WANT TO BLOCK SIGNALS DURING THIS NEXT LINE ***/
							if (!devtypecnt[DEVTYPE_POLL] && !devtypecnt[DEVTYPE_OTHR]) actionflag &= ~ACTION_POLL;
							pollbuild = TRUE;
						}
					}
				}
			}
#else
#if defined(FD_ZERO) && defined(FD_SET) && defined(FD_ISSET)
			FD_ZERO(&readset);
			FD_ZERO(&writeset);
			FD_ZERO(&errorset);
			selectcnt = 0;
			for (i1 = 0; i1 < devhi; i1++) {
				if ((device[i1].typeflg != DEVTYPE_STRM && device[i1].typeflg != DEVTYPE_POLL) ||
				    !(events = device[i1].polltype)) continue;
				if (events & (DEVPOLL_READ | DEVPOLL_READNORM | DEVPOLL_READBAND | DEVPOLL_READPRIORITY)) FD_SET(device[i1].handle, &readset);
				if (events & (DEVPOLL_WRITE | DEVPOLL_WRITENORM | DEVPOLL_WRITEBAND)) FD_SET(device[i1].handle, &writeset);
				if (events & (DEVPOLL_ERROR | DEVPOLL_HANGUP | DEVPOLL_NOFDES)) FD_SET(device[i1].handle, &errorset);
				if (device[i1].handle >= selectcnt) selectcnt = device[i1].handle + 1;
			}
			if (selectcnt && select(selectcnt, &readset, &writeset, &errorset, &zerotime) > 0) {
				for (i1 = 0; i1 < devhi; i1++) {
					if ((device[i1].typeflg != DEVTYPE_STRM && device[i1].typeflg != DEVTYPE_POLL) ||
					    !(revents = device[i1].polltype)) continue;
					if (!FD_ISSET(device[i1].handle, &readset)) revents &= ~(DEVPOLL_READ | DEVPOLL_READNORM | DEVPOLL_READBAND | DEVPOLL_READPRIORITY);
					if (!FD_ISSET(device[i1].handle, &writeset)) revents &= ~(DEVPOLL_WRITE | DEVPOLL_WRITENORM | DEVPOLL_WRITEBAND);
					if (!FD_ISSET(device[i1].handle, &errorset)) revents &= ~(DEVPOLL_ERROR | DEVPOLL_HANGUP | DEVPOLL_NOFDES);
					if (revents) {
						if ((i2 = device[i1].cbfnc(device[i1].arg, revents)) == -1) actionflag |= ACTION_RETRY;
						else if (i2 == 1) {
							devtypecnt[device[i1].typeflg]--;
							device[i1].typeflg = DEVTYPE_NULL;
							devtypecnt[DEVTYPE_NULL]++;
/*** CODE: MAY WANT TO BLOCK SIGNALS DURING THIS NEXT LINE ***/
							if (!devtypecnt[DEVTYPE_POLL] && !devtypecnt[DEVTYPE_OTHR]) actionflag &= ~ACTION_POLL;
						}
					}
				}
			}
#endif
#endif
		}
	}
	if (actionflagptr != NULL) {
/*** CODE: MAY NEED TO BLOCK SIGPOLL TO TURN FLAG OFF, BECAUSE IF SIGPOLL HAPPENS ***/
/***       BETWEEN TEST AND UNSET, THEN EVTPOLL WILL NOT BE CALLED AFTER SIGPOLL ***/
		if (actionflag & (ACTION_REGCNT | ACTION_SIGPOLL | ACTION_POLL | ACTION_RETRY)) *actionflagptr |= actionflagbit;
		else *actionflagptr &= ~actionflagbit;
	}
}

INT evtwait(INT *evtidarray, INT count)
{
	INT i2, events, revents;
	UINT i1;
#if !defined(POLLIN) && defined(FD_ZERO) && defined(FD_SET) && defined(FD_ISSET)
	INT selectcnt;
	fd_set readset, writeset, errorset;
#endif

	*evterrorstring = 0;
	if (count < 1) return -1;

	eventflg = FALSE;
	for (i2 = 0; i2 < count; i2++) {
		if (!evtidarray[i2]) continue;
		i1 = evtidarray[i2] - EVTBASE;
		if ((INT) i1 < 0 || i1 >= EVT_MAX_EVENTS || state[i1] == STATE_AVAIL) return -1;
		if (state[i1] == STATE_SET) {
			return i2;
		}
	}
	for ( ; ; ) {

#if 0
		if (actionflag & ACTION_REGCNT) {
			for (i1 = 0; i1 < regcnt; i1++) if (regcbfnc[i1] != NULL) regcbfnc[i1]();
		}
#endif

#ifdef POLLIN
		pollwait = INFTIM;
#else
#if defined(FD_ZERO) && defined(FD_SET) && defined(FD_ISSET)
		selectwait = NULL;
#endif
#endif
		if (!eventflg) {
			actionflag &= ~(ACTION_SIGPOLL | ACTION_RETRY);

			if (devtypecnt[DEVTYPE_FILE]) {
				for (i1 = 0; i1 < devhi; i1++) {
					if (device[i1].typeflg != DEVTYPE_FILE || !device[i1].polltype) continue;
					if (device[i1].cbfnc(device[i1].arg, DEVPOLL_NOBLOCK) == 1) {  /* EOF */
						devtypecnt[DEVTYPE_FILE]--;
						device[i1].typeflg = DEVTYPE_NULL;
						devtypecnt[DEVTYPE_NULL]++;
					}
					eventflg = TRUE;
#ifdef POLLIN
					pollwait = 0;
#else
#if defined(FD_ZERO) && defined(FD_SET) && defined(FD_ISSET)
					selectwait = &zerotime;
#endif
#endif
				}
			}
			if (!eventflg && devtypecnt[DEVTYPE_OTHR]) {
				for (i1 = 0; i1 < devhi; i1++) {
					if (device[i1].typeflg != DEVTYPE_OTHR || !device[i1].polltype) continue;
					if ((i2 = device[i1].cbfnc(device[i1].arg, DEVPOLL_BLOCK)) == -1) actionflag |= ACTION_RETRY;
					else if (i2 == 1) {  /* EOF */
						devtypecnt[DEVTYPE_OTHR]--;
						device[i1].typeflg = DEVTYPE_NULL;
						devtypecnt[DEVTYPE_NULL]++;
/*** CODE: MAY WANT TO BLOCK SIGNALS DURING THIS NEXT LINE ***/
						if (!devtypecnt[DEVTYPE_POLL] && !devtypecnt[DEVTYPE_OTHR]) actionflag &= ~ACTION_POLL;
					}
					eventflg = TRUE;
#ifdef POLLIN
					pollwait = 0;
#else
#if defined(FD_ZERO) && defined(FD_SET) && defined(FD_ISSET)
					selectwait = &zerotime;
#endif
#endif
				}
			}
			if (devtypecnt[DEVTYPE_STRM] || devtypecnt[DEVTYPE_POLL]) {
#ifdef POLLIN
				if (pollbuild) {  /* rebuild poll table */
					for (i1 = 0, pollcnt = 0; i1 < devhi; i1++) {
						if (device[i1].typeflg != DEVTYPE_STRM && device[i1].typeflg != DEVTYPE_POLL) continue;
						events = device[i1].polltype;
						revents = 0;
						if (events & DEVPOLL_READ) revents |= POLLIN;
						if (events & DEVPOLL_READNORM) revents |= POLLRDNORM;
						if (events & DEVPOLL_READBAND) revents |= POLLRDBAND;
						if (events & DEVPOLL_READPRIORITY) revents |= POLLPRI;
						if (events & DEVPOLL_WRITE) revents |= POLLOUT;
						if (events & DEVPOLL_WRITENORM) revents |= POLLWRNORM;
						if (events & DEVPOLL_WRITEBAND) revents |= POLLWRBAND;
						if (events & DEVPOLL_ERROR) revents |= POLLERR;
						if (events & DEVPOLL_HANGUP) revents |= POLLHUP;
						if (events & DEVPOLL_NOFDES) revents |= POLLNVAL;
						if (!(device[i1].events = revents)) continue;
						pollttys[pollcnt].fd = device[i1].handle;
						pollttys[pollcnt].events = device[i1].events & ~(POLLERR | POLLHUP | POLLNVAL);
						pollttys[pollcnt].revents = 0;
						polltab[pollcnt++] = i1;
					}
					pollbuild = FALSE;
				}

				if (pollcnt) {
/*** CODE: RACE CONDITION DOES EXIST HERE WITH SIGALARM.  SOLUTION TO CALL ***/
/***       INTO TIM.C AND GET NEXT TIMEOUT VALUE AND SET WAIT TO THAT WOULD ***/
/***       RESOLVE THIS PROBLEM.  NOTE: RACE CONDITION IS MINIMAL ***/


					if ((i1 = poll(pollttys, pollcnt, pollwait)) > 0) {
						for (i1 = 0; i1 < pollcnt; i1++) {
							if ( ( events = (device[polltab[i1]].events & pollttys[i1].revents) ) ) {
								revents = 0;
								if (events & POLLIN) revents |= DEVPOLL_READ;
								if (events & POLLRDNORM) revents |= DEVPOLL_READNORM;
								if (events & POLLRDBAND) revents |= DEVPOLL_READBAND;
								if (events & POLLPRI) revents |= DEVPOLL_READPRIORITY;
								if (events & POLLOUT) revents |= DEVPOLL_WRITE;
								if (events & POLLWRNORM) revents |= DEVPOLL_WRITENORM;
								if (events & POLLWRBAND) revents |= DEVPOLL_WRITEBAND;
								if (events & POLLERR) revents |= DEVPOLL_ERROR;
								if (events & POLLHUP) revents |= DEVPOLL_HANGUP;
								if (events & POLLNVAL) revents |= DEVPOLL_NOFDES;
								if ((i2 = device[polltab[i1]].cbfnc(device[polltab[i1]].arg, revents)) == -1) actionflag |= ACTION_RETRY;
								else if (i2 == 1) {
/*** CODE: THIS DOES NOT TURN OFF SIGPOLL.  MAYBE LEAVE TO APP. TO CALL BACK AND EVTDEVSET ***/
/***       TO 0.  MAKE SURE OTHER PLACES ARE FIXED ***/
									devtypecnt[device[i1].typeflg]--;
									device[i1].typeflg = DEVTYPE_NULL;
									devtypecnt[DEVTYPE_NULL]++;
/*** CODE: MAY WANT TO BLOCK SIGNALS DURING THIS NEXT LINE ***/
									if (!devtypecnt[DEVTYPE_POLL] && !devtypecnt[DEVTYPE_OTHR]) actionflag &= ~ACTION_POLL;
									pollbuild = TRUE;
								}
							}
						}
					}
					eventflg = TRUE;
				}
#else
#if defined(FD_ZERO) && defined(FD_SET) && defined(FD_ISSET)
				FD_ZERO(&readset);
				FD_ZERO(&writeset);
				FD_ZERO(&errorset);
				selectcnt = 0;
				for (i1 = 0; i1 < devhi; i1++) {
					if ((device[i1].typeflg != DEVTYPE_STRM && device[i1].typeflg != DEVTYPE_POLL) ||
					    !(events = device[i1].polltype)) continue;
					if (events & (DEVPOLL_READ | DEVPOLL_READNORM | DEVPOLL_READBAND | DEVPOLL_READPRIORITY)) FD_SET(device[i1].handle, &readset);
					if (events & (DEVPOLL_WRITE | DEVPOLL_WRITENORM | DEVPOLL_WRITEBAND)) FD_SET(device[i1].handle, &writeset);
					if (events & (DEVPOLL_ERROR | DEVPOLL_HANGUP | DEVPOLL_NOFDES)) FD_SET(device[i1].handle, &errorset);
					if (device[i1].handle >= selectcnt) selectcnt = device[i1].handle + 1;
				}
				if (selectcnt) {
/*** CODE: RACE CONDITION DOES EXIST HERE WITH SIGALARM.  SOLUTION TO CALL ***/
/***       INTO TIM.C AND GET NEXT TIMEOUT VALUE AND SET WAIT TO THAT WOULD ***/
/***       RESOLVE THIS PROBLEM.  NOTE: RACE CONDITION IS MINIMAL ***/
					if (select(selectcnt, &readset, &writeset, &errorset, selectwait) > 0) {
						for (i1 = 0; i1 < devhi; i1++) {
							if ((device[i1].typeflg != DEVTYPE_STRM && device[i1].typeflg != DEVTYPE_POLL) ||
							    !(revents = device[i1].polltype)) continue;
							if (!FD_ISSET(device[i1].handle, &readset)) revents &= ~(DEVPOLL_READ | DEVPOLL_READNORM | DEVPOLL_READBAND | DEVPOLL_READPRIORITY);
							if (!FD_ISSET(device[i1].handle, &writeset)) revents &= ~(DEVPOLL_WRITE | DEVPOLL_WRITENORM | DEVPOLL_WRITEBAND);
							if (!FD_ISSET(device[i1].handle, &errorset)) revents &= ~(DEVPOLL_ERROR | DEVPOLL_HANGUP | DEVPOLL_NOFDES);
							if (revents) {
								if ((i2 = device[i1].cbfnc(device[i1].arg, revents)) == -1) actionflag |= ACTION_RETRY;
								else if (i2 == 1) {
									devtypecnt[device[i1].typeflg]--;
									device[i1].typeflg = DEVTYPE_NULL;
									devtypecnt[DEVTYPE_NULL]++;
/*** CODE: MAY WANT TO BLOCK SIGNALS DURING THIS NEXT LINE ***/
									if (!devtypecnt[DEVTYPE_POLL] && !devtypecnt[DEVTYPE_OTHR]) actionflag &= ~ACTION_POLL;
								}
							}
						}
					}
					eventflg = TRUE;
				}
#endif
#endif
			}
			if (!eventflg) pause();  /* nothing to wait on */
		}
		eventflg = FALSE;
		for (i2 = 0; i2 < count; i2++) {
			if (!evtidarray[i2]) continue;
			i1 = evtidarray[i2] - EVTBASE;
			if (state[i1] == STATE_SET) goto evtwait1;
		}
	}

evtwait1:
	if (actionflagptr != NULL && (actionflag & (ACTION_REGCNT | ACTION_SIGPOLL | ACTION_POLL | ACTION_RETRY))) *actionflagptr |= actionflagbit;
	return i2;
}

INT evtdevinit(INT handle, INT polltype, INT (*cbfnc)(void *, INT), void *arg)
{
	INT i2, type;
	UINT i1, newdevidx;
	struct stat fdstat, nullstat;
#if defined(SA_RESTART) || defined(SA_INTERRUPT) || defined(SA_RESETHAND)
	struct sigaction act;
#endif
#ifdef POLLIN
	INT events;
	struct pollfd ttys;
#else
#if defined(FD_ZERO) && defined(FD_SET) && defined(FD_ISSET)
	static struct timeval mintime = { 0, 1 };
	fd_set readset, writeset, errorset;
#endif
#endif

	*evterrorstring = 0;
	/* See if a device exists with the same fd */
	for (i1 = 0; i1 < devhi; i1++) {
		if (!device[i1].typeflg) continue;
		if (handle == device[i1].handle) {
			/* change current settings and callback of handle */
			if (evtdevset(handle, polltype) == -1) return -1;
			device[i1].cbfnc = cbfnc;
			device[i1].arg = arg;
			return 0;
		}
	}

	/* find first open slot in the device array */
	for (newdevidx = 0; newdevidx < DEVICEMAX && device[newdevidx].typeflg; newdevidx++);
	if (newdevidx == DEVICEMAX) {
		evtputerror("exceeding maximum number of devices");
		return -1;
	}

	/* determine if device is really a file or /dev/null */
	if (fstat(handle, &fdstat) != -1) {
		/* see mknod(2) for mode values */
		if (S_ISREG(fdstat.st_mode) || S_ISDIR(fdstat.st_mode)) {
			type = DEVTYPE_FILE;
/*** CODE: SHOULD WE TURN ON SUPPORT FOR POLLING ON A FILE ***/
/***			actionflag |= ACTION_POLL; ***/
			goto initend;
		}
		/* test if handle is to /dev/null */
		if (S_ISCHR(fdstat.st_mode) && stat("/dev/null", &nullstat) != -1) {
			if (nullstat.st_ino == fdstat.st_ino &&
			    nullstat.st_dev == fdstat.st_dev) {
				type = DEVTYPE_NULL;
				goto initend;
			}
		}
	}

	if (devtypecnt[DEVTYPE_OTHR]) {
		evtputerror("O/S does not support I/O multiplexing");
		return -1;
	}

	if (!(sigflg & 0x01)) {  /* initialize signal handler */
#if defined(SA_RESTART) || defined(SA_INTERRUPT) || defined(SA_RESETHAND)
		act.sa_handler = sigevent;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
/*** CODE: SHOULD THIS BE RESTART OR INTERRUPT ***/
#ifdef SA_RESTART
		act.sa_flags |= SA_RESTART;
#endif
		if (sigaction(SIGPIPE, &act, &oldsigpipe) == -1) {
			evterror("sigaction", errno);
			return -1;
		}
#else
		if ((oldsigpipe = sigset(SIGPIPE, sigevent)) == SIG_ERR) {
			evterror("sigset", errno);
			return -1;
		}
#endif
		sigflg |= 0x01;
	}

#if defined(SIGPOLL) && defined(I_SETSIG)
	if (!(sigflg & 0x02)) {  /* initialize signal handler */
#if defined(SA_RESTART) || defined(SA_INTERRUPT) || defined(SA_RESETHAND)
		act.sa_handler = sigevent;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
/*** CODE: SHOULD THIS BE RESTART OR INTERRUPT ***/
#ifdef SA_RESTART
		act.sa_flags |= SA_RESTART;
#endif
		if (sigaction(SIGPOLL, &act, &oldsigpoll) == -1) {
			evterror("sigaction", errno);
			return -1;
		}
#else
		if ((oldsigpoll = sigset(SIGPOLL, sigevent)) == SIG_ERR) {
			evterror("sigset", errno);
			return -1;
		}
#endif
		sigflg |= 0x02;
	}

	i2 = setpollbits(polltype);
	if (ioctl(handle, I_SETSIG, i2) != -1) {  /* STREAMS type */
		type = DEVTYPE_STRM;
		if (polltype) actionflag |= ACTION_SIGPOLL;
		else ioctl(handle, I_SETSIG, 0);
		goto initend;
	}
#else
#if defined(SIGIO) && defined(O_ASYNC)
	if (!(sigflg & 0x02)) {  /* initialize signal handler */
#if defined(SA_RESTART) || defined(SA_INTERRUPT) || defined(SA_RESETHAND)
		act.sa_handler = sigevent;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
/*** CODE: SHOULD THIS BE RESTART OR INTERRUPT ***/
#ifdef SA_RESTART
		act.sa_flags |= SA_RESTART;
#endif
		if (sigaction(SIGIO, &act, &oldsigpoll) == -1) {
			evterror("sigaction", errno);
			return -1;
		}
#else
		if ((oldsigpoll = sigset(SIGIO, sigevent)) == SIG_ERR) {
			evterror("sigset", errno);
			return -1;
		}
#endif
		sigflg |= 0x02;
	}

	if (fcntl(handle, F_SETOWN, getpid()) != -1) {
		i2 = fcntl(handle, F_GETFL, 0);
		if (i2 != -1 && fcntl(handle, F_SETFL, i2 | O_ASYNC) != -1) {  /* STREAMS type */
			type = DEVTYPE_STRM;
			if (polltype) actionflag |= ACTION_SIGPOLL;
			else fcntl(handle, F_SETFL, i2 & ~O_ASYNC);
			goto initend;
		}
	}
#endif
#endif

#ifdef POLLIN
	events = 0;
	if (!polltype || (polltype & DEVPOLL_READ)) events |= POLLIN;
	if (polltype & DEVPOLL_READNORM) events |= POLLRDNORM;
	if (polltype & DEVPOLL_READBAND) events |= POLLRDBAND;
	if (polltype & DEVPOLL_READPRIORITY) events |= POLLPRI;
	if (polltype & DEVPOLL_WRITE) events |= POLLOUT;
	if (polltype & DEVPOLL_WRITENORM) events |= POLLWRNORM;
	if (polltype & DEVPOLL_WRITEBAND) events |= POLLWRBAND;
	ttys.fd = handle;
	ttys.events = events;
	ttys.revents = 0;
	if (poll(&ttys, 1, 1) != -1) {
		if (!(ttys.revents & (POLLERR | POLLNVAL))) {  /* file descriptor can be polled */
			type = DEVTYPE_POLL;
			if (polltype) actionflag |= ACTION_POLL;
			goto initend;
		}
	}
#else
#if defined(FD_ZERO) && defined(FD_SET) && defined(FD_ISSET)
	FD_ZERO(&readset);
	FD_ZERO(&writeset);
	FD_ZERO(&errorset);
	if (!polltype || (polltype & (DEVPOLL_READ | DEVPOLL_READNORM | DEVPOLL_READBAND | DEVPOLL_READPRIORITY))) FD_SET(handle, &readset);
	if (polltype & (DEVPOLL_WRITE | DEVPOLL_WRITENORM | DEVPOLL_WRITEBAND)) FD_SET(handle, &writeset);
	if (polltype & (DEVPOLL_ERROR | DEVPOLL_HANGUP | DEVPOLL_NOFDES)) FD_SET(handle, &errorset);
	if (select(handle, &readset, &writeset, &errorset, &mintime) != -1) {
		if (!FD_ISSET(handle, &errorset)) {
			type = DEVTYPE_POLL;
			if (polltype) actionflag |= ACTION_POLL;
			goto initend;
		}
	}
#endif
#endif

	/* no stream or poll support, support one device using direct reads */
	if (devtypecnt[DEVTYPE_STRM] || devtypecnt[DEVTYPE_POLL]) {
		evtputerror("device does not support I/O multiplexing");
		return -1;
	}
	type = DEVTYPE_OTHR;
	if (polltype) actionflag |= ACTION_POLL;

initend:
	device[newdevidx].typeflg = type;
	device[newdevidx].handle = handle;
	device[newdevidx].polltype = polltype;
	device[newdevidx].cbfnc = cbfnc;
	device[newdevidx].arg = arg;
	if (newdevidx == devhi) devhi++;
/*** CODE: MAYBE NOT INCREMENT DEVTYPECNT IF POLLTYPE = 0 ***/
	devtypecnt[type]++;

	if (actionflagptr != NULL && (actionflag & (ACTION_SIGPOLL | ACTION_POLL | ACTION_RETRY))) *actionflagptr |= actionflagbit;
#ifdef POLLIN
	if (type == DEVTYPE_STRM || type == DEVTYPE_POLL) pollbuild = TRUE;
#endif
	return 0;
}

INT evtdevexit(INT handle)
{
	INT type;
	UINT i1;
#if defined(SIGIO) && defined(O_ASYNC) && !(defined(SIGPOLL) && defined(I_SETSIG))
	INT i2;
#endif

	*evterrorstring = 0;
	for (i1 = 0; i1 < devhi && (!device[i1].typeflg || handle != device[i1].handle); i1++);
	if (i1 == devhi) {
		evtputerror("handle is not in table");
		return -1;
	}
	if (device[i1].typeflg & DEVTYPE_STOP) evtdevstart(handle);
	type = device[i1].typeflg;
	device[i1].typeflg = 0;
	while (devhi && !device[devhi - 1].typeflg) devhi--;
	devtypecnt[type]--;

#if defined(SIGPOLL) && defined(I_SETSIG)
	if (type == DEVTYPE_STRM) ioctl(handle, I_SETSIG, 0);

/*** CODE: COULD REMOVE MOST OF THIS CODE IF WANT TO LEAVE SIGNAL HANDLER INSTALLED ***/
	if (!devtypecnt[DEVTYPE_STRM]) {
		if (sigflg & 0x02) {
#if defined(SA_RESTART) || defined(SA_INTERRUPT) || defined(SA_RESETHAND)
			sigaction(SIGPOLL, &oldsigpoll, NULL);
#else
			sigset(SIGPOLL, oldsigpoll);
#endif
			sigflg &= ~0x02;
		}
		actionflag &= ~ACTION_SIGPOLL;
	}
#else
#if defined(SIGIO) && defined(O_ASYNC)
	if (type == DEVTYPE_STRM) {
		i2 = fcntl(handle, F_GETFL, 0);
		if (i2 != -1) fcntl(handle, F_SETFL, i2 & ~O_ASYNC);
	}

/*** CODE: COULD REMOVE MOST OF THIS CODE IF WANT TO LEAVE SIGNAL HANDLER INSTALLED ***/
	if (!devtypecnt[DEVTYPE_STRM]) {
		if (sigflg & 0x02) {
#if defined(SA_RESTART) || defined(SA_INTERRUPT) || defined(SA_RESETHAND)
			sigaction(SIGIO, &oldsigpoll, NULL);
#else
			sigset(SIGIO, oldsigpoll);
#endif
			sigflg &= ~0x02;
		}
		actionflag &= ~ACTION_SIGPOLL;
	}
#endif
#endif

	if (!devhi && (sigflg & 0x01)) {
#if defined(SA_RESTART) || defined(SA_INTERRUPT) || defined(SA_RESETHAND)
		sigaction(SIGPIPE, &oldsigpipe, NULL);
#else
		sigset(SIGPIPE, oldsigpipe);
#endif
		sigflg &= ~0x01;
	}

/*** CODE: MAY WANT TO BLOCK SIGNALS DURING THIS NEXT LINE ***/
	if (!devtypecnt[DEVTYPE_POLL] && !devtypecnt[DEVTYPE_OTHR]) actionflag &= ~ACTION_POLL;

#ifdef POLLIN
	if (type == DEVTYPE_STRM || type == DEVTYPE_POLL) pollbuild = TRUE;
#endif

	return 0;
}

INT evtdevstop(INT handle)
{
	INT err;
	UINT i1;
#if !(defined(SIGPOLL) && defined(I_SETSIG)) && defined(SIGIO) && defined(O_ASYNC)
	INT i2;
#endif

	*evterrorstring = 0;
	if (handle != -1) {
		for (i1 = 0; i1 < devhi && (!device[i1].typeflg || handle != device[i1].handle); i1++);
		if (i1 == devhi) {
			evtputerror("handle is not in table");
			return -1;
		}
		handle = i1;
	}
	else i1 = 0;

	err = 0;
	for ( ; i1 < devhi && (handle == -1 || handle == (INT) i1); i1++) {
		if (!device[i1].typeflg || (device[i1].typeflg & DEVTYPE_STOP)) continue;
		if (device[i1].typeflg == DEVTYPE_POLL) devtypecnt[DEVTYPE_POLL]--;
#if defined(SIGPOLL) && defined(I_SETSIG)
		else if (device[i1].typeflg == DEVTYPE_STRM) {
			if (ioctl(device[i1].handle, I_SETSIG, 0) == -1) {
				evterror("ioctl", errno);
				err = -1;
			}
		}
#else
#if defined(SIGIO) && defined(O_ASYNC)
		else if (device[i1].typeflg == DEVTYPE_STRM) {
			i2 = fcntl(device[i1].handle, F_GETFL, 0);
			if (i2 == -1 || fcntl(device[i1].handle, F_SETFL, i2 & ~O_ASYNC) == -1) {
				evterror("fcntl", errno);
				err = -1;
			}
		}
#endif
#endif
		device[i1].typeflg |= DEVTYPE_STOP;
	}

/*** CODE: MAY WANT TO BLOCK SIGNALS DURING THIS NEXT LINE ***/
	if (!devtypecnt[DEVTYPE_POLL] && !devtypecnt[DEVTYPE_OTHR]) actionflag &= ~ACTION_POLL;

#ifdef POLLIN
	pollbuild = TRUE;
#endif
	return err;
}

INT evtdevstart(INT handle)
{
#if defined(SIGPOLL) && defined(I_SETSIG)
	INT polltype;
#endif
	INT i2, err;
	UINT i1;
	*evterrorstring = 0;
	if (handle != -1) {
		for (i1 = 0; i1 < devhi && (!device[i1].typeflg || handle != device[i1].handle); i1++);
		if (i1 == devhi) {
			evtputerror("handle is not in table");
			return -1;
		}
		handle = i1;
	}
	else i1 = 0;

	err = 0;
	for ( ; i1 < devhi && (handle == -1 || handle == (INT) i1); i1++) {
		if (!(device[i1].typeflg & DEVTYPE_STOP)) continue;
		device[i1].typeflg &= ~DEVTYPE_STOP;
		if (device[i1].typeflg == DEVTYPE_POLL) devtypecnt[DEVTYPE_POLL]++;
#if defined(SIGPOLL) && defined(I_SETSIG)
		else if (device[i1].typeflg == DEVTYPE_STRM) {
			polltype = device[i1].polltype;
			i2 = setpollbits(polltype);
			if (i2) {
				if (ioctl(device[i1].handle, I_SETSIG, i2) == -1) {
					evterror("ioctl", errno);
					err = -1;
				}
				actionflag |= ACTION_SIGPOLL;
			}
		}
#else
#if defined(SIGIO) && defined(O_ASYNC)
		else if (device[i1].typeflg == DEVTYPE_STRM && device[i1].polltype) {
			i2 = fcntl(device[i1].handle, F_GETFL, 0);
			if (i2 == -1 || fcntl(device[i1].handle, F_SETFL, i2 | O_ASYNC) == -1) {
				evterror("fcntl", errno);
				err = -1;
			}
			actionflag |= ACTION_SIGPOLL;
		}
#endif
#endif
	}

/*** CODE: THIS NEXT LINE DOES NOT SUPPORT HAVING polltype = 0 ***/
	if (devtypecnt[DEVTYPE_POLL] || devtypecnt[DEVTYPE_OTHR]) actionflag |= ACTION_POLL;
	if (actionflagptr != NULL && (actionflag & (ACTION_SIGPOLL | ACTION_POLL | ACTION_RETRY))) *actionflagptr |= actionflagbit;
#ifdef POLLIN
	pollbuild = TRUE;
#endif

	return err;
}

INT evtdevtype(INT handle, INT *typeptr)
{
	UINT i1;

	*evterrorstring = 0;
	for (i1 = 0; i1 < devhi && (!device[i1].typeflg || handle != device[i1].handle); i1++);
	if (i1 == devhi) {
		evtputerror("handle is not in table");
		return -1;
	}

	*typeptr = device[i1].typeflg;
	return 0;
}

/**
 * Finds the entry in the device table whose fd equals handle.
 * Changes the polltype bits.
 * Sets pollbuild to TRUE;
 *
 * @param handle The fd of interest, we search our device table for a match
 * @return zero if OK, -1 if anything went wrong.
 */
INT evtdevset(INT handle, INT polltype)
{
	INT i2, err;
	UINT i1;
	*evterrorstring = 0;
	for (i1 = 0; i1 < devhi && (!device[i1].typeflg || handle != device[i1].handle); i1++);
	if (i1 == devhi) {
		evtputerror("handle is not in table");
		return -1;
	}

	/* check for trivial case, polltype does not need changing */
	if (device[i1].polltype == polltype) return 0;

	err = 0;
#if defined(SIGPOLL) && defined(I_SETSIG)
	if (device[i1].typeflg == DEVTYPE_STRM) {
		i2 = setpollbits(polltype);
		if (ioctl(device[i1].handle, I_SETSIG, i2) == -1) {
			evterror("ioctl", errno);
			err = -1;
		}
		if (i2) actionflag |= ACTION_SIGPOLL;
	}
#else
#if defined(SIGIO) && defined(O_ASYNC)
	if (device[i1].typeflg == DEVTYPE_STRM) {
		i2 = fcntl(device[i1].handle, F_GETFL, 0);
		if (polltype) {
			if (i2 == -1 || fcntl(device[i1].handle, F_SETFL, i2 | O_ASYNC) == -1) {
				evterror("fcntl", errno);
				err = -1;
			}
			actionflag |= ACTION_SIGPOLL;
		}
		else if (i2 == -1 || fcntl(handle, F_SETFL, i2 & ~O_ASYNC) == -1) {
			evterror("fcntl", errno);
			err = -1;
		}
	}
#endif
#endif

	device[i1].polltype = polltype;
	if (actionflagptr != NULL && (actionflag & ACTION_SIGPOLL)) *actionflagptr |= actionflagbit;
#ifdef POLLIN
	if (device[i1].typeflg == DEVTYPE_STRM || device[i1].typeflg == DEVTYPE_POLL) pollbuild = TRUE;
#endif
	return err;
}

INT evtgeterror(CHAR *errorstr, INT size)
{
	INT length;

	length = strlen(evterrorstring);
	if (--size >= 0) {
		if (size > length) size = length;
		if (size) memcpy(errorstr, evterrorstring, size);
		errorstr[size] = 0;
		evterrorstring[0] = 0;
	}
	return length;
}

INT evtputerror(CHAR *errorstr)
{
	UINT length;
	length = strlen(errorstr);
	if (length > sizeof(evterrorstring) - 1) length = sizeof(evterrorstring) - 1;
	if (length) memcpy(evterrorstring, errorstr, length);
	evterrorstring[length] = 0;
	return length;
}

static void evterror(CHAR *function, INT err)
{
	CHAR num[32], work[256];

	strcpy(work, function);
	strcat(work, " failure");
	if (err) {
		mscitoa(err, num);
		strcat(work, ", error = ");
		strcat(work, num);
	}
	evtputerror(work);
}

static void sigevent(INT sig)
{
	switch(sig) {
#if defined(SIGPOLL) && defined(I_SETSIG)
	case SIGPOLL:
		actionflag |= ACTION_SIGPOLL;
		if (actionflagptr != NULL) *actionflagptr |= actionflagbit;
		break;
#else
#if defined(SIGIO) && defined(O_ASYNC)
	case SIGIO:
		actionflag |= ACTION_SIGPOLL;
		if (actionflagptr != NULL) *actionflagptr |= actionflagbit;
		break;
#endif
#endif
	case SIGPIPE:
/*** CODE: POSSIBLY A SOCKET DISCONNECT (KEEPALIVE).  BUT WHICH SOCKET?? ***/
		break;
	}
}

#if defined(SIGPOLL) && defined(I_SETSIG)
static INT setpollbits(INT polltype) {
	register INT i2 = 0;
	if (!polltype || (polltype & DEVPOLL_READ)) i2 |= S_INPUT;
#ifdef S_RDNORM
	if (polltype & DEVPOLL_READNORM) i2 |= S_RDNORM;
#endif
#ifdef S_RDBAND
	if (polltype & DEVPOLL_READBAND) i2 |= S_RDBAND;
#endif
#ifdef S_HIPRI
	if (polltype & DEVPOLL_READPRIORITY) i2 |= S_HIPRI;
#endif
#ifdef S_OUTPUT
	if (polltype & DEVPOLL_WRITE) i2 |= S_OUTPUT;
#endif
#ifdef S_WRNORM
	if (polltype & DEVPOLL_WRITENORM) i2 |= S_WRNORM;
#endif
#ifdef S_WRBAND
	if (polltype & DEVPOLL_WRITEBAND) i2 |= S_WRBAND;
#endif
#ifdef S_ERROR
	if (polltype & DEVPOLL_ERROR) i2 |= S_ERROR;
#endif
#ifdef S_HANGUP
	if (polltype & DEVPOLL_HANGUP) i2 |= S_HANGUP;
#endif
	return i2;
}
#endif
