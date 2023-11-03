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
#define INC_TIME
#define INC_SIGNAL
#define INC_LIMITS
#include "includes.h"
#include "base.h"
#include "tim.h"
#include "evt.h"

#if OS_UNIX
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#endif

 /* changed from 20 on 10 NOV 2011 */
#define MAXTIMERS 128

static INT daysinmonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static struct TMINFO {
	INT handle;
	INT eventid;
	TIMSTRUCT expiretime;
} tminfo[MAXTIMERS];
static INT handlecnt = 0;
static INT timhi = -1;

#if OS_WIN32
static HANDLE threadhandle;
static DWORD threadid;
static HANDLE threadevent;
static DWORD threadwait = INFINITE;

static DWORD WINAPI timerproc(LPVOID);
#endif

#if OS_UNIX
static void timtimecallback(INT);
#endif

static INT timchkstamp(UCHAR *, TIMSTRUCT *);
static INT timcmp(TIMSTRUCT *, TIMSTRUCT *);
static INT timdiff(TIMSTRUCT *, TIMSTRUCT *);

INT timinit()
{
	return 0;
}

void timexit()
{
#if OS_WIN32
	if (threadhandle != NULL) {
		TerminateThread(threadhandle, 0);
		WaitForSingleObject(threadhandle, 500);
		CloseHandle(threadhandle);
		threadhandle = NULL;
	}
	if (threadevent == NULL) {
		CloseHandle(threadevent);
		threadevent = NULL;
	}
#endif
}

INT timset(UCHAR *timestamp, INT eventid)
{
	static INT firstflag = TRUE;
	INT i1, i2, centiseconds;
	TIMSTRUCT nexttimeout, twork1, twork2;

#if OS_UNIX
	INT sigerr;
	struct itimerval itimer, oldtimer;
#if defined(SA_RESTART) | defined(SA_INTERRUPT) | defined(SA_RESETHAND)
	struct sigaction act;
#endif
#endif

	if (firstflag) {
#if OS_WIN32
		threadevent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (threadevent == NULL) return RC_ERROR;
		threadhandle = CreateThread(NULL, 0, timerproc, NULL, 0, &threadid);
		if (threadhandle == NULL) {
			CloseHandle(threadevent);
			threadevent = NULL;
			return RC_ERROR;
		}
#endif

#if OS_UNIX
#if defined(SA_RESTART) | defined(SA_INTERRUPT) | defined(SA_RESETHAND)
		act.sa_handler = timtimecallback;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
/*** CODE: MAY WANT TO RESTART INTERRUPT ROUTINE INSTEAD ***/
#ifdef SA_INTERRUPT
		act.sa_flags |= SA_INTERRUPT;
#endif
		sigerr = sigaction(SIGALRM, &act, NULL);
#else
		sigerr = (sigset(SIGALRM, timtimecallback) == SIG_ERR) ? -1 : 0;
#endif
		if (sigerr == -1) return RC_ERROR;
#endif

		firstflag = FALSE;
	}

	if (timchkstamp(timestamp, &twork1) == -1 || eventid < 1) return RC_ERROR;
	timgettime(&twork2);
	if (timcmp(&twork1, &twork2) <= 0) {  /* time has already expired */
		evtset(eventid);
		return 0;
	}
	nexttimeout.yr = 10000;
	pvistart();
	for (i1 = 0, i2 = -1; i1 <= timhi; i1++) {
		if (tminfo[i1].handle == 0) {
			if (i2 == -1) i2 = i1;
		}
		else if (timcmp(&tminfo[i1].expiretime, &nexttimeout) < 0) nexttimeout = tminfo[i1].expiretime;
	}
	if (i2 == -1) {
		if (i1 == MAXTIMERS) {  /* too many active timers */
			pviend();
			return RC_ERROR;
		}
		i2 = timhi = i1;
	}

	if (++handlecnt < 1) handlecnt = 1;
	tminfo[i2].handle = handlecnt;
	tminfo[i2].eventid = eventid;
	tminfo[i2].expiretime = twork1;
	if (timcmp(&twork1, &nexttimeout) < 0) {
		centiseconds = timdiff(&twork1, &twork2);
#if OS_WIN32
		if (centiseconds > INT_MAX / 10) centiseconds = INT_MAX / 10;
		threadwait = centiseconds * 10;
		if (!SetEvent(threadevent)) {
			pviend();
			return RC_ERROR;
		}
#endif

#if OS_UNIX
		timerclear(&itimer.it_interval);
		timerclear(&itimer.it_value);
		itimer.it_value.tv_sec = centiseconds / 100;
		itimer.it_value.tv_usec = (centiseconds % 100) * 1000;
		setitimer(ITIMER_REAL, &itimer, &oldtimer);
#endif
	}

	pviend();
	return handlecnt;
}

#if OS_WIN32
static DWORD WINAPI timerproc(LPVOID parm)
{
	INT i1, centiseconds;
	TIMSTRUCT now, nexttimeout;

	for ( ; ; ) {
		ResetEvent(threadevent);
		if (WaitForSingleObject(threadevent, threadwait) != WAIT_OBJECT_0) {
			timgettime(&now);
			nexttimeout.yr = 10000;
			pvistart();
			for (i1 = 0; i1 <= timhi; i1++) {
				if (tminfo[i1].handle == 0) continue;
				if (timcmp(&tminfo[i1].expiretime, &now) <= 0) {
					evtset(tminfo[i1].eventid);
					tminfo[i1].handle = 0;
				}
				else if (timcmp(&tminfo[i1].expiretime, &nexttimeout) < 0) nexttimeout = tminfo[i1].expiretime;
			}
			if (nexttimeout.yr != 10000) {
				centiseconds = timdiff(&nexttimeout, &now);
				if (centiseconds > INT_MAX / 10) centiseconds = INT_MAX / 10;
				threadwait = centiseconds * 10;
			}
			else threadwait = INFINITE;
			pviend();
		}
	}
	return 0;
}
#endif

#if OS_UNIX
static void timtimecallback(
#ifdef __GNUC__
		__attribute__ ((unused)) INT sig
#else
		 INT sig
#endif
)
{
	INT i1, centiseconds;
	TIMSTRUCT now, nexttimeout;
	struct itimerval itimer, oldtimer;

	timgettime(&now);
	nexttimeout.yr = 10000;
	for (i1 = 0; i1 <= timhi; i1++) {
		if (tminfo[i1].handle == 0) continue;
		if (timcmp(&tminfo[i1].expiretime, &now) <= 0) {
			evtset(tminfo[i1].eventid);
			tminfo[i1].handle = 0;
		}
		else if (timcmp(&tminfo[i1].expiretime, &nexttimeout) < 0) nexttimeout = tminfo[i1].expiretime;
	}

	if (nexttimeout.yr != 10000) {
		centiseconds = timdiff(&nexttimeout, &now);
		/*
		 * if (centiseconds < 100) centiseconds = 100;
		 * else if (centiseconds % 100 >= 50) centiseconds += 100;
		 * alarm(centiseconds / 100);
		 */
		timerclear(&itimer.it_interval);
		timerclear(&itimer.it_value);
		itimer.it_value.tv_sec = centiseconds / 100;
		itimer.it_value.tv_usec = (centiseconds % 100) * 1000;
		setitimer(ITIMER_REAL, &itimer, &oldtimer);
	}
}
#endif

void timstop(INT timerhandle)
{
	INT i1;
#if OS_UNIX
	struct itimerval itimer, oldtimer;
#endif

	for (i1 = timhi; i1 >= 0; i1--) {
		if (timerhandle == tminfo[i1].handle) {
			tminfo[i1].handle = 0;
			if (i1 == timhi) timhi--;
			break;
		}
		if (i1 == timhi && tminfo[i1].handle == 0) timhi--;
	}
#if OS_UNIX
	if (timhi == -1) {
		/*alarm(0);*/
		timerclear(&itimer.it_interval);
		timerclear(&itimer.it_value);
		setitimer(ITIMER_REAL, &itimer, &oldtimer);
	}
#endif
}

INT timadd(UCHAR *timestamp, INT centiseconds)
{
	INT i1;
	TIMSTRUCT twork;

	if (timchkstamp(timestamp, &twork) == RC_ERROR || centiseconds < 0) return RC_ERROR;

	twork.hun += centiseconds;
	if (twork.hun >= 100) {
		twork.sec += twork.hun / 100;
		twork.hun %= 100;
		if (twork.sec >= 60) {
			twork.min += twork.sec / 60;
			twork.sec %= 60;
			if (twork.min >= 60) {
				twork.hr += twork.min / 60;
				twork.min %= 60;
				if (twork.hr >= 24) {
					twork.day += twork.hr / 24;
					twork.hr %= 24;
					for ( ; ; ) {
						i1 = daysinmonth[twork.mon - 1];
						if (twork.mon == 2 && !(twork.yr & 0x03)) i1 = 29;
						if (twork.day <= i1) break;
						twork.day -= i1;
						if (++twork.mon > 12) {
							twork.mon = 1;
							twork.yr++;
						}
					}
				}
			}
		}
	}

	for (i1 = 4; --i1 >= 0; twork.yr /= 10) timestamp[i1] = (UCHAR)(twork.yr % 10 + '0');
	timestamp[4] = (UCHAR)(twork.mon / 10 + '0');
	timestamp[5] = (UCHAR)(twork.mon % 10 + '0');
	timestamp[6] = (UCHAR)(twork.day / 10 + '0');
	timestamp[7] = (UCHAR)(twork.day % 10 + '0');
	timestamp[8] = (UCHAR)(twork.hr / 10 + '0');
	timestamp[9] = (UCHAR)(twork.hr % 10 + '0');
	timestamp[10] = (UCHAR)(twork.min / 10 + '0');
	timestamp[11] = (UCHAR)(twork.min % 10 + '0');
	timestamp[12] = (UCHAR)(twork.sec / 10 + '0');
	timestamp[13] = (UCHAR)(twork.sec % 10 + '0');
	timestamp[14] = (UCHAR)(twork.hun / 10 + '0');
	timestamp[15] = (UCHAR)(twork.hun % 10 + '0');
	return 0;
}

static INT timchkstamp(UCHAR *timestamp, TIMSTRUCT *dest)
{
	INT i1, i2, yr, mon, day, hr, min, sec, hun;

	yr = mon = day = hr = min = sec = hun = 0;
	for (i1 = 0; i1 < 16; i1++) {
		if (!isdigit(timestamp[i1])) return RC_ERROR;
		i2 = timestamp[i1] - '0';
		if (i1 < 4) yr = yr * 10 + i2;
		else if (i1 < 6) mon = mon * 10 + i2;
		else if (i1 < 8) day = day * 10 + i2;
		else if (i1 < 10) hr = hr * 10 + i2;
		else if (i1 < 12) min = min * 10 + i2;
		else if (i1 < 14) sec = sec * 10 + i2;
		else hun = hun * 10 + i2;
	}

	if (yr < 1901 || yr > 2099) return RC_ERROR;
	if (mon == 0 || mon > 12) return RC_ERROR;
	i1 = daysinmonth[mon - 1];
	if (mon == 2 && !(yr & 0x03)) i1 = 29;
	if (day == 0 || day > i1) return RC_ERROR;
	if (hr > 23) return RC_ERROR;
	if (min > 59) return RC_ERROR;
	if (sec > 59) return RC_ERROR;

	dest->yr = yr;
	dest->mon = mon;
	dest->day = day;
	dest->hr = hr;
	dest->min = min;
	dest->sec = sec;
	dest->hun = hun;
	return 0;
}

void timgettime(TIMSTRUCT *dest)
{
#if OS_WIN32
	SYSTEMTIME systime;

	GetLocalTime(&systime);
	dest->yr = systime.wYear;
	dest->mon = systime.wMonth;
	dest->day = systime.wDay;
	dest->hr = systime.wHour;
	dest->min = systime.wMinute;
	dest->sec = systime.wSecond;
	dest->hun = systime.wMilliseconds / 10;
#endif

#if OS_UNIX
	struct timeval tv;
	struct timezone tz;
	struct tm *today;
	time_t timer;

	/*time(&timer);*/
	gettimeofday(&tv, &tz);
	timer = tv.tv_sec;
	today = localtime(&timer);
	dest->yr = today->tm_year + 1900;
	dest->mon = today->tm_mon + 1;
	dest->day = today->tm_mday;
	dest->hr = today->tm_hour;
	dest->min = today->tm_min;
	dest->sec = today->tm_sec;
	dest->hun = tv.tv_usec / 10000;
#endif
}

/*
 * compare (subtract src2 from src1) and return 1, 0 or -1
 */
static INT timcmp(TIMSTRUCT *src1, TIMSTRUCT *src2)
{
	if (src1->yr > src2->yr) return 1;
	if (src1->yr < src2->yr) return -1;
	if (src1->mon > src2->mon) return 1;
	if (src1->mon < src2->mon) return -1;
	if (src1->day > src2->day) return 1;
	if (src1->day < src2->day) return -1;
	if (src1->hr > src2->hr) return 1;
	if (src1->hr < src2->hr) return -1;
	if (src1->min > src2->min) return 1;
	if (src1->min < src2->min) return -1;
	if (src1->sec > src2->sec) return 1;
	if (src1->sec < src2->sec) return -1;
	if (src1->hun > src2->hun) return 1;
	if (src1->hun < src2->hun) return -1;
	return 0;
}

/* subtract src2 from src1 and return time difference in centiseconds */
static INT timdiff(TIMSTRUCT *src1, TIMSTRUCT *src2)
{
	INT i1, centiseconds, days, negflg;
	TIMSTRUCT *tmpsrc;

	negflg = timcmp(src1, src2);
	if (!negflg) return 0;
	if (negflg == -1) {
		tmpsrc = src1;
		src1 = src2;
		src2 = tmpsrc;
	}
	centiseconds = src1->hun - src2->hun;
	centiseconds += (src1->sec - src2->sec) * 100;
	centiseconds += (src1->min - src2->min) * 6000;
	centiseconds += (src1->hr - src2->hr) * 360000;
	for (days = src1->day - src2->day; ; ) {
		if (src1->mon == src2->mon && src1->yr == src2->yr) break;
		i1 = daysinmonth[src2->mon - 1];
		if (src2->mon == 2 && !(src2->yr & 0x03)) i1 = 29;
		days += i1;
		if (++src2->mon > 12) {
			src2->mon = 1;
			src2->yr++;
		}
	}
	if (days > INT_MAX / 8640000 - 1) days = INT_MAX / 8640000 - 1;  /* prevent integer overflow */
	centiseconds += days * 8640000;
	return centiseconds * negflg;
}

/**
 * Returns a pointer to a statically allocated null-terminated string
 * that will contain the UTC offset.  e.g. -0600 for CST
 */
CHAR* getUTCOffset() {
	static CHAR work[6];
#if OS_WIN32
	INT i, j;
	DWORD tzid;
	TIME_ZONE_INFORMATION timezoneinfo;
#endif
#if OS_UNIX
	INT hmin, days;
	time_t timer;
	struct tm *today;
	struct tm gtm;
	struct tm ltm;
#endif

#if OS_WIN32
	tzid = GetTimeZoneInformation(&timezoneinfo);
	i = (INT) timezoneinfo.Bias;
	if (tzid == TIME_ZONE_ID_STANDARD) i += timezoneinfo.StandardBias;
	else if (tzid == TIME_ZONE_ID_DAYLIGHT) i += timezoneinfo.DaylightBias;
	if (i < 0) {
		work[0] = '+';
		i = (0 - i);
	}
	else work[0] = '-';
	j = i / 60;
	work[1] = (CHAR) (j / 10 + '0');
	work[2] = (CHAR) (j % 10 + '0');
	i -= j * 60;
	work[3] = (CHAR) (i / 10 + '0');
	work[4] = (CHAR) (i % 10 + '0');
	work[5] = '\0';
#endif

#if OS_UNIX
	time(&timer);
	today = localtime(&timer);
	work[0] = '\0';
	if (today->tm_isdst >= 0) {
		memcpy(&ltm, today, sizeof(struct tm));
		memcpy(&gtm, gmtime(&timer), sizeof(struct tm));
		days = (ltm.tm_year == gtm.tm_year) ? ltm.tm_yday - gtm.tm_yday : ltm.tm_year - gtm.tm_year;
		hmin = 60 * (24 * days + ltm.tm_hour - gtm.tm_hour) + ltm.tm_min - gtm.tm_min;
		if (hmin < 0) {
			work[0] = '-';
			hmin = -hmin;
		}
		else {
			work[0] = '+';
		}
		sprintf(&work[1], "%04d", (hmin / 60) * 100 + hmin % 60);
	}
#endif

	return work;
}
