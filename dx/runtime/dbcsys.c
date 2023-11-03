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
#define INC_ERRNO
#define DBCMAIN
#include "includes.h"
#include "release.h"
#include "base.h"

#if OS_WIN32
#include <wsipx.h>
#if _MSC_VER >= 1800
#include <VersionHelpers.h>
#endif
#endif

#if OS_UNIX
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#ifndef __MACOSX
#include <sys/sem.h>
#endif
#endif

#include "dbc.h"
#include "fio.h"
#include "gui.h"
#include "tim.h"
#include "vid.h"
#include "dbcclnt.h"

#define DBCSYSFLAG_ROLLOUTCLOSE		0x0001
#define DBCSYSFLAG_ROLLOUTWINDOW	0x0002
/* Unix only */
#define DBCSYSFLAG_NEWEXECUTE		0x0004

extern CHAR utilerror[256];

static INT arc;
static CHAR **arv;
static CHAR **env;
static INT dbcsysflags;
static CHAR conname[MAX_NAMESIZE];
static CHAR day[22] = { "SunMonTueWedThuFriSat" };
static CHAR mth[37] = { "JanFebMarAprMayJunJulAugSepOctNovDec" };

/* local routine declarations */
static void getport(CHAR *);


INT sysinit(INT argc, CHAR *argv[], CHAR *envp[])  /* initialize a few things */
{
	CHAR *ptr;

	arc = argc;
	arv = argv;
	env = envp;
	dbcsysflags = 0;
	if (!prpget("clock", "day", NULL, NULL, &ptr, 0) && strlen(ptr) == 21) strcpy(day, ptr);
	if (!prpget("clock", "month", NULL, NULL, &ptr, 0) && strlen(ptr) == 36) strcpy(mth, ptr);
	if (!prpget("console", NULL, NULL, NULL, &ptr, 0) && (strlen(ptr) < MAX_NAMESIZE)) strcpy(conname, ptr);
	else conname[0] = '\0';
	if (!prpget("file", "rolloutclose", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) dbcsysflags |= DBCSYSFLAG_ROLLOUTCLOSE;
#if OS_WIN32
	if (!prpget("rolloutwindow", NULL, NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) dbcsysflags |= DBCSYSFLAG_ROLLOUTWINDOW;
#endif
#if OS_UNIX
	if (!prpget("execute", NULL, NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "new")) dbcsysflags |= DBCSYSFLAG_NEWEXECUTE;
#endif
	return(0);
}

void sysexit()
{
}

void vclockClient()
{
	CHAR work[512];
	INT n = getbyte();
	UCHAR *adr = getvar(VAR_WRITE);

	dbcflags &= ~DBCFLAG_EOS;
	if (!(dbcflags & DBCFLAG_CLIENTINIT)) {
		fp = 0;
		setfplp(adr);
		dbcflags |= DBCFLAG_EOS;
		return;
	}
	switch (n)
	{
	case 5:	/* version */
		clientClockVersion(work, sizeof(work));
		break;
	default:
		dbcerrinfo(505, (UCHAR*)"Bad CLOCKCLIENT opcode", -1);
	}
}

void vclock()
{
	INT i, j, n, hsec;
	CHAR work[512], *ptr, **pptr;
	UCHAR *adr;
	struct tm today;

#if OS_WIN32
	__time64_t timer;
	DWORD wrklen;
	struct tm today2;
	SYSTEMTIME systime;
	TIME_ZONE_INFORMATION timezoneinfo;
	DWORD tzid;
	WCHAR *wcptr;
	errno_t err;
#endif

#if OS_UNIX
#ifndef __MACOSX
	struct passwd *ppasswd;
#endif
	struct timeval tv;
	time_t timer;
#endif

	n = getbyte();
	adr = getvar(VAR_WRITE);
	fp = 1;
	lp = pl;

	dbcflags &= ~DBCFLAG_EOS;
	/**
	 * 1 = Time
	 * 2 = Date
	 * 3 = Year
	 * 4 = Day
	 * 9 = Calendar
	 * 10 = Timestamp
	 * 14 = Weekday
	 * 20 = UTC Calendar
	 * 21 = UTC Timestamp
	 */
	if (n < 5 || n == 9 || n == 10 || n == 14 || n == 20 || n == 21) {
		hsec = 0;
#if OS_WIN32
		if (n == 4) {
			_time64(&timer);
			err = _localtime64_s(&today, &timer);
			if (err != 0) {
				dbcflags |= DBCFLAG_EOS;
				ZeroMemory(&today, sizeof(today));
				today.tm_yday = err - 1;
			}
		}
		else {
			if (n == 20 || n == 21) GetSystemTime(&systime);
			else GetLocalTime(&systime);
			today2.tm_year = systime.wYear - 1900;
			today2.tm_mon = systime.wMonth - 1;
			today2.tm_wday = systime.wDayOfWeek;
			today2.tm_mday = systime.wDay;
			today2.tm_hour = systime.wHour;
			today2.tm_min = systime.wMinute;
			today2.tm_sec = systime.wSecond;
			hsec = systime.wMilliseconds / 10;
			today = today2;
		}
#endif
#if OS_UNIX
		/* Use gettimeofday here just to fill in hsec. */
		gettimeofday(&tv, NULL);
		hsec = tv.tv_usec / 10000;
		if (n == 20 || n == 21) memcpy(&today, gmtime(&tv.tv_sec), sizeof(struct tm));
		else memcpy(&today, localtime(&tv.tv_sec), sizeof(struct tm));
#endif
		if (n == 20) n = 9;
		else if (n == 21) n = 10;
		if (n != 4 && n != 14) {
			i = today.tm_year + 1900;
			for (j = 4; j--; i /= 10) work[j] = (CHAR)(i % 10 + '0');
			i = today.tm_mon + 1;
			work[4] = (CHAR)(i / 10 + '0');
			work[5] = (CHAR)(i % 10 + '0');
			i = today.tm_mday;
			work[6] = (CHAR)(i / 10 + '0');
			work[7] = (CHAR)(i % 10 + '0');
			i = today.tm_hour;
			work[8] = (CHAR)(i / 10 + '0');
			work[9] = (CHAR)(i % 10 + '0');
			i = today.tm_min;
			work[10] = (CHAR)(i / 10 + '0');
			work[11] = (CHAR)(i % 10 + '0');
			i = today.tm_sec;
			work[12] = (CHAR)(i / 10 + '0');
			work[13] = (CHAR)(i % 10 + '0');
			work[14] = (CHAR)(hsec / 10 + '0');
			work[15] = (CHAR)(hsec % 10 + '0');
		}

		if (n == 1) {
			if (pl < 8) dbcflags |= DBCFLAG_EOS;
			else lp = 8;
			name[2] = name[5] = ':';
			name[0] = work[8];
			name[1] = work[9];
			name[3] = work[10];
			name[4] = work[11];
			name[6] = work[12];
			name[7] = work[13];
		}
		else if (n == 2) {
			if (pl < 6) dbcflags |= DBCFLAG_EOS;
			else lp = 6;
			memcpy(name, &work[4], 4);
			name[4] = work[2];
			name[5] = work[3];
		}
		else if (n == 3) {
			if (pl < 2) dbcflags |= DBCFLAG_EOS;
			else lp = 2;
			name[0] = work[2];
			name[1] = work[3];
		}
		else if (n == 4) {
			if (pl < 3) dbcflags |= DBCFLAG_EOS;
			else lp = 3;
			i = today.tm_yday + 1;
			name[0] = (CHAR)(i / 100 + '0');
			i %= 100;
			name[1] = (CHAR)(i / 10 + '0');
			name[2] = (CHAR)(i % 10 + '0');
		}
		else if (n == 9) {
			if (pl < 24) dbcflags |= DBCFLAG_EOS;
			else lp = 24;
			name[3] = name[7] = name[8] = name[10] = name[15] = ' ';
			name[18] = name[21] = ':';
			memcpy(name, &day[today.tm_wday * 3], 3);
			memcpy(&name[4], &mth[today.tm_mon * 3], 3);
			name[8] = work[6];
			name[9] = work[7];
			memcpy(&name[11], work, 4);
			name[16] = work[8];
			name[17] = work[9];
			name[19] = work[10];
			name[20] = work[11];
			name[22] = work[12];
			name[23] = work[13];
		}
		else if (n == 10) {
			if (pl < 16) dbcflags |= DBCFLAG_EOS;
			else lp = 16;
			memcpy(name, work, 16);
		}
		else if (n == 14) {
			if (pl < 1) dbcflags |= DBCFLAG_EOS;
			else lp = 1;
			name[0] = (CHAR)(today.tm_wday + '1');
		}
		memcpy(&adr[hl], name, lp);
		setfplp(adr);
		return;
	}
	if (n == 8 || n == 12) {  /* env, cmdline */
		if (n == 8 && !prpget("clock", "env", NULL, NULL, &ptr, 0)) {
			lp = (INT)strlen(ptr);
			if (lp > pl) {
				lp = pl;
				dbcflags |= DBCFLAG_EOS;
			}
			memcpy(&adr[hl], ptr, lp);
		}
		else {
			if (n == 8) pptr = env;
			else pptr = arv;
#ifdef CODE_WARRIOR
			for (i = 0, lp = 0; pptr != NULL && ((n == 8 && pptr[i] != NULL) || (n == 12 && i < arc)); i++, lp += j) {
#else
			for (i = 0, lp = 0; (n == 8 && pptr[i] != NULL) || (n == 12 && i < arc); i++, lp += j) {
#endif
				if (n == 12 && pptr[i][0] == '-' && toupper(pptr[i][1]) == 'S' && toupper(pptr[i][2]) == 'C') {
					j = 0;
					continue;
				}
				if (i) {
					if (pl <= lp) {
						dbcflags |= DBCFLAG_EOS;
						break;
					}
					adr[hl + lp++] = ' ';
				}
				j = (INT)strlen(pptr[i]);
				if (pl < lp + j) {
					j = pl - lp;
					dbcflags |= DBCFLAG_EOS;
				}
				memcpy(&adr[hl + lp], pptr[i], j);
			}
		}
		if (!lp) fp = 0;
		setfplp(adr);
		return;
	}

	work[0] = '\0';
	if (n == 5) {  /* version */
#if OS_WIN32
		strcpy(work, "Windows");
#endif
#if OS_UNIX
		strcpy(work, "UNIX");
#endif
#if __MACOSX
		strcpy(work, "Mac");
#endif
	}
	else if (n == 6) {  /* port */
		getport(work);
	}
	else if (n == 7) {  /* user */
#if OS_WIN32
/*** CODE: VERIFY THIS WORKS FOR ALL NETWORKS ***/
/*** MAYBE CALL WNetGetCaps WITH WNNC_USER TO SEE IF 1 IS RETURNED BEFORE ***/
/*** CALLING WNetGetUser ***/
		wrklen = sizeof(work) - 1;
		if (WNetGetUser((LPSTR) NULL, (LPSTR) work, &wrklen) == NO_ERROR) work[wrklen] = 0;
		else strcpy(work, "USER");
#endif
#if OS_UNIX
#ifdef __MACOSX
		strcpy(work, getlogin());
#else
		ppasswd = getpwuid(geteuid());
		if (ppasswd != NULL) strcpy(work, ppasswd->pw_name);
#endif
#endif
	}
	else if (n == 11) {  /* error */
		i = (INT)(sizeof(work) - 1);
		geterrmsg((UCHAR *) work, &i, TRUE);
		work[i] = '\0';
	}
	else if (n == 13) {  /* license */
		mscitoa(license, work);
	}
	else if (n == 15) {  /* release */
		// as of 101, release is treated in a special way, output blanks if it does not fit.
		if (pl < 3) strcpy(work, "   ");
		else strcpy(work, RELEASE);
	}
	else if (n == 16) {  /* pid */
#if OS_WIN32
		mscitoa((INT) GetCurrentProcessId(), work);
#endif
#if OS_UNIX
		mscitoa(getpid(), work);
#endif
	}
	else if (n == 17) {  /* utilerror */
		strcpy(work, utilerror);
	}
	else if (n == 18) {  /* timezone */
#if OS_WIN32
		tzid = GetTimeZoneInformation(&timezoneinfo);
		if (tzid == TIME_ZONE_ID_DAYLIGHT) wcptr = &(timezoneinfo.DaylightName[0]);
		else wcptr = &(timezoneinfo.StandardName[0]);
		for (j = 0; j < 32 && ( (work[j] = (CHAR) wcptr[j]) != 0 ); j++);
#endif
#if OS_UNIX
		time(&timer);
		memcpy(&today, localtime(&timer), sizeof(struct tm));
		strftime(work, 50, "%Z", &today);
#endif
	}
	else if (n == 19) {  /* utcoffset */
		strcpy(work, getUTCOffset());
	}
	else if (n == 22) {		/* UI */
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			if (dbcflags & DBCFLAG_CLIENTGUI) {
				if (smartClientSubType[0] != '\0')
				{
					if (strlen(smartClientSubType) == 7 && !strcmp(smartClientSubType, "Servlet"))
					{
						strcpy(work, "WSC");
					}
					else if (strlen(smartClientSubType) == 3 && !strcmp(smartClientSubType, "iOS"))
					{
						strcpy(work, "ISC");
					}
					else if (strlen(smartClientSubType) == 4 && !strcmp(smartClientSubType, "Java"))
					{
						strcpy(work, "JSC");
					}
					else if (strlen(smartClientSubType) == 7 && !strcmp(smartClientSubType, "Android"))
					{
						strcpy(work, "ASC");
					}
					else strcpy(work, "GSC"); /* If the SC type string is not recognized */
				}
				else strcpy(work, "GSC"); /* If SCTYPE did not appear on the command line */
			}
			else strcpy(work, "CSC");
		}
		else {
			if (dbcflags & DBCFLAG_CONSOLE) strcpy(work, "CUI");
			else strcpy(work, "GUI");
		}
	}

	lp = (INT)strlen(work);
	if (lp > pl) {
		lp = pl;
		dbcflags |= DBCFLAG_EOS;
	}
	if (lp) {
		memcpy(&adr[hl], work, lp);
		fp = 1;
	}
	else fp = 0;
	setfplp(adr);
}

void vconsole()  /* console verb */
{
	static INT conhndl = -1;
	static OFFSET confpos;
	static UCHAR conport[8];
	INT i, j;
	OFFSET eofpos = 0;
	UCHAR work[81], *adr;

	memcpy(work, conport, 8);
	i = 8;
	dbcflags &= ~DBCFLAG_EOS;
	while ((adr = getlist(LIST_READ | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1)) != NULL) {
		if ((vartype & TYPE_NUMERIC) || !fp) continue;
		cvtoname(adr);
		j = (INT)strlen(name);
		if (i + j > 80) {
			j = 80 - i;
			dbcflags |= DBCFLAG_EOS;
		}
		memcpy(&work[i], name, j);
		i += j;
	}
	if (dbcflags & DBCFLAG_DEBUG) dbgconsole((CHAR *)&work[8], i-8);
	if (conname[0] == '\0') return;
	if (i < 80) memset(&work[i], ' ', 80 - i);
	if (conhndl == -1) {
		getport(name);
		memset(conport, ' ', 8);
		i = (INT)strlen(name);
		if (i > 8) i = 8;
		memcpy(conport, name, i);
		memcpy(work, conport, 8);
vcon1:
		conhndl = rioopen(conname, RIO_M_EFC | RIO_P_TXT | RIO_T_STD | RIO_FIX | RIO_UNC, 0, 80);
		if (conhndl < 0 && conhndl != ERR_NOACC && conhndl != ERR_EXIST) {
			if (conhndl == ERR_BADNM || conhndl == ERR_FNOTF) dbcerror(603);
			else dbcerror(1630 - conhndl);
		}
		if (conhndl >= 0) {
			rioput(conhndl, work, 80);
			riolastpos(conhndl, &confpos);
			rioclose(conhndl);
		}
		else confpos = -1L;
		conhndl = rioopen(conname, RIO_M_SHR | RIO_P_TXT | RIO_T_STD | RIO_FIX | RIO_UNC, 0, 80);
		if (conhndl < 0) {
			if (conhndl == ERR_FNOTF) dbcerror(601);
			else if (conhndl == ERR_NOACC) dbcerror(602);
			else if (conhndl == ERR_BADNM) dbcerror(603);
			else dbcerror(1630 - conhndl);
		}
	}
	if (fiotouch(conhndl)) {
		rioclose(conhndl);
		goto vcon1;
	}
	if (confpos != -1) rioeofpos(conhndl, &eofpos);
	if (confpos == -1 || confpos >= eofpos) {
		if (fioflck(conhndl)) dbcerror(1771);
		for (;;) {
			i = rioget(conhndl, (UCHAR *) name, 80);
			if (i != 80) {
				if (i == -1) {
					rioput(conhndl, work, 80);
					riolastpos(conhndl, &confpos);
					break;
				}
				if (i < -2) {
					fiofulk(conhndl);
					dbcerror(1730 - i);
				}
			}
			else if (!memcmp(name, conport, 8)) {
				riolastpos(conhndl, &confpos);
				break;
			}
		}
		fiofulk(conhndl);
	}
	riosetpos(conhndl, confpos);
	rioput(conhndl, work, 80);
}

static void getport(char *str)
{
	CHAR *ptr;

#if OS_UNIX
	INT i, j, k;
#endif

	if (!prpget("clock", "port", NULL, NULL, &ptr, 0)) strcpy(str, ptr);
	else {
#if OS_WIN32
/*** CODE: ASSUME THAT THIS CODE WILL BE DEPENDENT ON NETWARE API FOR WIN32 ***/
		strcpy(str, "001");
#endif

#if OS_UNIX
		strcpy(str, "000");
		ptr = (CHAR *) ttyname(fileno(stdout));
		if (ptr != NULL && !memcmp(ptr, "/dev/", 5)) {
			if (!memcmp(&ptr[5], "con", 3)) strcpy(str, "999");
			else {
				i = strlen(ptr);
				for (j = 5; ptr[j] && (ptr[j] != 't' || ptr[j + 1] != 't' || ptr[j + 2] != 'y'); j++);
				if (ptr[j]) {
					j += 3;
					if (i - 3 > j) j = i - 3;
					k = 2;
					while (i-- > j) {
						if (ptr[i] == '/') break;
						str[k--] = ptr[i];
					}
				}
				else {
					j = i - 3;
					while (i-- > j) {
						if (!isdigit(ptr[i])) break;
						str[i - j] = ptr[i];
					}
				}
			}
		}
#endif
	}
}

#if OS_WIN32
void vexecute()  /* execute the command in name */
{
	INT i1 = 0, len;
	CHAR work[600], *ptr;
	UCHAR *adr1;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	//OSVERSIONINFO osInfo;

	adr1 = getvar(VAR_READ);
	dbcflags |= DBCFLAG_OVER;
	if (fp) {
		cvtoname(adr1);
		//osInfo.dwOSVersionInfoSize = sizeof(osInfo);
		//if (!GetVersionEx(&osInfo)) osInfo.dwPlatformId = VER_PLATFORM_WIN32_WINDOWS;  /* assume Windows 95 */
		ptr = getenv("COMSPEC");
		if (ptr != NULL && ptr[0]) {
			strcpy(work, ptr);
			len = (INT)strlen(work);
			for (i1 = len - 1; i1 > 0 && work[i1] != '.' && work[i1] != '\\' && work[i1] != '/'; i1--);
			if (work[i1] != '.') {
				if (IsWindowsVersionOrGreater(5, 0, 0))
				/*if (osInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) */ strcpy(&work[len], ".exe");
				else strcpy(&work[len], ".com");
			}
		}
		else if (
				/*osInfo.dwPlatformId == VER_PLATFORM_WIN32_NT*/
				IsWindowsVersionOrGreater(5, 0, 0)) strcpy(work, "cmd.exe");
		else strcpy(work, "command.com");
		strcat(work, " /c ");
		strcat(work, name);

		memset(&si, 0, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
/*** CODE: THIS CODE AND RELEVANT CODE BELOW FIXES WIN95 BUG OF CHANGING THE DBC TITLE. ***/
/***       FOR EXECUTE, THIS WILL MOST LIKELY NOT WORK BECAUSE THE NEW PROCESS WILL ***/
/***       PROBABLY START AFTER THE SETCONSOLETITLE IS PERFORMED ***/
		// This is asking if the Windows ver is < Win2000. We do not support those anymore.
//		if (osInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
//			i1 = (INT) GetConsoleTitle((LPSTR) title, sizeof(title) - 1);
//		}
		if (CreateProcess(NULL, (LPSTR) work, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			dbcflags &= ~DBCFLAG_OVER;
		}
//		if (osInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS && i1) {
//			title[i1] = '\0';
//			SetConsoleTitle((LPTSTR) title);
//		}
	}
}

/**
 * rollout
 */
void vrollout()
{
	INT i1 = 0, len;
	DWORD rc, flag;

	/**
	 * The Windows limit for 7 and up is 32767
	 * So make our work buffer 16 less than 32768
	 */
	CHAR work[32768 - 16];

	CHAR *ptr;
	UCHAR *adr1;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	if (dbcflags & DBCFLAG_DEBUG) vdebug(DEBUG_RESTORE);
	if (fpicnt) filepi0();  /* cancel any filepi */

	adr1 = getvar(VAR_READ);
	dbcflags |= DBCFLAG_OVER;
	if (fp) {
		cvtodbcbuf(adr1);
		if (dbcsysflags & DBCSYSFLAG_ROLLOUTCLOSE) while (!fioclru(-1));
		if (dbcflags & DBCFLAG_VIDINIT) vidsuspend(TRUE);
		ptr = getenv("COMSPEC");
		if (ptr != NULL && ptr[0]) {
			strcpy(work, ptr);
			len = (INT)strlen(work);
			for (i1 = len - 1; i1 > 0 && work[i1] != '.' && work[i1] != '\\' && work[i1] != '/'; i1--);
			if (work[i1] != '.') {
				strcpy(&work[len], ".exe");
			}
		}
		else strcpy(work, "cmd.exe");
		strcat(work, " /c ");
		strcat(work, dbcbuf);

		memset(&si, 0, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		if (!(dbcflags & DBCFLAG_VIDINIT) && !(dbcsysflags & DBCSYSFLAG_ROLLOUTWINDOW)) {
			flag = CREATE_NO_WINDOW;
			si.dwFlags = STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_HIDE;
		}
		else flag = 0;
		if (CreateProcess(NULL, (LPTSTR) work, NULL, NULL, TRUE, flag, NULL, NULL, &si, &pi)) {
			CloseHandle(pi.hThread);
			if (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_FAILED &&
			    GetExitCodeProcess(pi.hProcess, &rc) && !rc) dbcflags &= ~DBCFLAG_OVER;
			CloseHandle(pi.hProcess);
		}
		if (dbcflags & DBCFLAG_VIDINIT) vidresume();
	}
}

/**
 * Windows only function
 *
 * Returns TRUE for success, FALSE for failure
 */
INT utilexec(CHAR *s)
{
	INT i1, i2 = 0;
	DWORD rc;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	if (dbcflags & DBCFLAG_DEBUG) vdebug(DEBUG_RESTORE);
	if (fpicnt) filepi0();  /* cancel any filepi */

	if (dbcsysflags & DBCSYSFLAG_ROLLOUTCLOSE) while (!fioclru(-1));
	if (dbcflags & DBCFLAG_VIDINIT) vidsuspend(FALSE);
	i1 = TRUE;
	memset(&si, 0, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	if (CreateProcess(NULL, (LPTSTR) s, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
		CloseHandle(pi.hThread);
		if (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_FAILED &&
		    GetExitCodeProcess(pi.hProcess, &rc) && !rc) i1 = FALSE;
		CloseHandle(pi.hProcess);
	}
	if (dbcflags & DBCFLAG_VIDINIT) vidresume();
	return(i1);
}
#endif

#if OS_UNIX
void vexecute()  /* execute the command in name */
{
	INT i, pid;
	UCHAR *adr1;

	adr1 = getvar(VAR_READ);
	dbcflags |= DBCFLAG_OVER;
	if (fp) {
		cvtoname(adr1);
		if (!(dbcsysflags & DBCSYSFLAG_NEWEXECUTE)) {
			if ((pid = fork()) == 0) {  /* child process */
				execl("/bin/sh", "sh", "-c", name, (char *) NULL);
				_exit(1);  /* exec died */
			}
			else if (pid > 0) dbcflags &= ~DBCFLAG_OVER;  /* fork succeeded */
		}
		else {  /* code to avoid zombie <defunct> procs */
			if ((pid = fork()) == 0) {
				if (fork() == 0) {  /* child process */
					execl("/bin/sh", "sh", "-c", name, (char *) NULL);
					_exit(1);  /* exec died */
				}
				_exit(0);
			}
			if (pid > 0) {
				while ((i = wait((INT *) NULL)) != pid && i != -1);
				dbcflags &= ~DBCFLAG_OVER;
			}
		}
	}
}

/**
 * rollout to the command in name
 */
void vrollout()
{
	UCHAR *adr1;

	if (dbcflags & DBCFLAG_DEBUG) vdebug(DEBUG_RESTORE);
	if (fpicnt) filepi0();  /* cancel any filepi */

	adr1 = getvar(VAR_READ);
	dbcflags |= DBCFLAG_OVER;
	if (fp) {
		cvtoname(adr1);
		if (dbcsysflags & DBCSYSFLAG_ROLLOUTCLOSE) while (!fioclru(-1));
		if (dbcflags & DBCFLAG_VIDINIT) vidsuspend(TRUE);
		if (!system(name)) dbcflags &= ~DBCFLAG_OVER;
		if (dbcflags & DBCFLAG_VIDINIT) vidresume();
	}
}

INT utilexec(CHAR *s)
{
	INT i;

	if (dbcflags & DBCFLAG_DEBUG) vdebug(DEBUG_RESTORE);
	if (fpicnt) filepi0();  /* cancel any filepi */

	if (dbcsysflags & DBCSYSFLAG_ROLLOUTCLOSE) while (!fioclru(-1));
	if (dbcflags & DBCFLAG_VIDINIT) vidsuspend(FALSE);
	i = system(s);
	if (dbcflags & DBCFLAG_VIDINIT) vidresume();
	return(i);
}
#endif
