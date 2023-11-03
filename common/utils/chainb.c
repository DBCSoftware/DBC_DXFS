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
#define INC_CTYPE
#define INC_STDLIB
#define INC_TIME
#define INC_SIGNAL
#define INC_ERRNO
#include "includes.h"
#include "base.h"
#include "chain.h"
#include "fio.h"

#if OS_WIN32
#include <sys\types.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <io.h>
#endif

#if OS_UNIX
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#endif

static INT stdhndl, stdhndl2, loghndl;
static INT wrkhndl;
static CHAR record[RECMAX + 4];
static UCHAR header[HDRSIZ + 4];
static OFFSET eofpos, lastpos, nextpos;
static CHAR *workptr;
static INT abortflg, abortbit, systemflg;

static void command(void);
static void comment(void);
static void vpause(void);
static void abortoff(void);
static void aborton(void);
static void abtif(void);
static void errabort(void);
static void errgotolab(void);
static void errgotopos(void);
static void gotolab(void);
static void gotopos(void);
static void keyboard(void);
static void logon(void);
static void logoff(void);
static void noabort(void);
static void soundr(void);
static void stamp(void);
static void systemoff(void);
static void systemon(void);
static void terminate(void);
static void chnuto10(UINT, UCHAR *);
static void quitsig(INT);


void execution(INT flag, CHAR* workname)
{
	INT i1;
	signal(SIGINT, SIG_IGN);
#if OS_WIN32
	stdhndl = _fileno(stdout);
	stdhndl2 = _dup(stdhndl);
#else
	stdhndl = fileno(stdout);
	stdhndl2 = dup(stdhndl);
#endif
	loghndl = -1;
	abortflg = TRUE;
	if (flag & SYSTEM) systemflg = TRUE;
	else systemflg = FALSE;
	workptr = workname;

	wrkhndl = rioopen(workptr, RIO_M_EXC | RIO_P_WRK | RIO_T_STD | RIO_UNC, 0, RECMAX);
	if (wrkhndl < 0) death(DEATH_OPEN, wrkhndl, workptr, 2);
	i1 = rioget(wrkhndl, header, HDRSIZ);
	if (i1 != HDRSIZ || (header[0] != COMPILED && header[0] != ABORTED)) death(DEATH_INVWORK, 0, fioname(wrkhndl), 2);
	msc9tooff(&header[1], &nextpos);
	msc9tooff(&header[10], &eofpos);
	if (header[0] == ABORTED) msc9tooff(&header[19], &lastpos);
	else lastpos = nextpos;
	if (flag & RESTART1) nextpos = lastpos;

	signal(SIGINT, quitsig);
	for ( ; ; ) {
		if (nextpos == 999999999L) {
			header[0] = FINISHED;
			riosetpos(wrkhndl, 0L);
			rioput(wrkhndl, header, HDRSIZ);
			riokill(wrkhndl);
			return;
		}
		riosetpos(wrkhndl, nextpos);
		i1 = rioget(wrkhndl, (UCHAR *) record, RECMAX);
		if (i1 < 1) death(DEATH_INVWORK, 0, fioname(wrkhndl), 2);
		if (record[0] == END_OF_EXTENT) {
			msc9tooff((UCHAR *) &record[1], &nextpos);
			msc9tooff((UCHAR *) &record[10], &eofpos);
			continue;
		}
		riolastpos(wrkhndl, &lastpos);
		rionextpos(wrkhndl, &nextpos);
		record[i1] = 0;
		switch (record[0]) {
			case COMMAND:
				command();
				break;
			case COMMENT:
				comment();
				break;
			case PAUSE:
				vpause();
				break;
			case ABORTOFF:
				abortoff();
				break;
			case ABORTON:
				aborton();
				break;
			case ABTIF:
				abtif();
				break;
			case ERRABORT:
				errabort();
				break;
			case ERRGOTO:
				errgotolab();
				break;
			case ERRGOTOX:
				errgotopos();
				break;
			case GOTO:
				gotolab();
				break;
			case GOTOX:
				gotopos();
				break;
			case KEYBOARD:
				keyboard();
				break;
			case LABEL:
				break;
			case LOG:
				logon();
				break;
			case LOGOFF:
				logoff();
				break;
			case NOABORT:
				noabort();
				break;
			case SOUNDR:
				soundr();
				break;
			case STAMP:
				stamp();
				break;
			case SYSTEMOFF:
				systemoff();
				break;
			case SYSTEMON:
				systemon();
				break;
			case TERMINATE:
				terminate();
				break;
			default:
				break;
		}
	}
}

static void command()
{
	time_t twork;
	INT i1;

#if OS_WIN32
	INT len;
	DWORD rc;
	CHAR work[256], *ptr;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
#endif

	signal(SIGINT, SIG_IGN);
	header[0] = EXECUTING;
	mscoffto9(nextpos, &header[1]);
	mscoffto9(eofpos, &header[10]);
	mscoffto9(lastpos, &header[19]);
	time(&twork);
	chnuto10((UINT) twork, &header[28]);
	riosetpos(wrkhndl, 0L);
	i1 = rioput(wrkhndl, header, HDRSIZ);
	if (i1) death(DEATH_WRITE, i1, fioname(wrkhndl), 2);
	rioclose(wrkhndl);

	dspstring(&record[1]);
	dspchar('\n');
	dspflush();
#if OS_WIN32
	if (systemflg) {
		ptr = getenv("COMSPEC");
		if (ptr != NULL && ptr[0]) {
			strcpy(work, ptr);
			len = (INT)strlen(work);
			for (i1 = len - 1; i1 > 0 && work[i1] != '.' && work[i1] != '\\' && work[i1] != '/'; i1--);
			if (work[i1] != '.') strcpy(&work[len], ".exe");
		}
		else strcpy(work, "cmd.exe");
		strcat(work, " /c ");
		strcat(work, &record[1]);
		ptr = work;
	}
	else {
		ptr = &record[1];
		while (isspace(*ptr)) ptr++;
	}

	abortbit = 1;
	memset(&si, 0, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	if (CreateProcess(NULL, (LPTSTR) ptr, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
		CloseHandle(pi.hThread);
		if (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_FAILED &&
		    GetExitCodeProcess(pi.hProcess, &rc) && !rc) abortbit = 0;
		CloseHandle(pi.hProcess);
	}
#endif

#if OS_UNIX
	abortbit = system(&record[1]);
#endif

	signal(SIGINT, SIG_IGN);
	if (systemflg) abortbit = 0;

	wrkhndl = rioopen(workptr, RIO_M_EXC | RIO_P_WRK | RIO_T_STD | RIO_UNC, 0, RECMAX);
	if (wrkhndl < 0) death(DEATH_OPEN, wrkhndl, workptr, 2);
	i1 = rioget(wrkhndl, header, HDRSIZ);
	if (i1 != HDRSIZ) death(DEATH_INVWORK, 0, fioname(wrkhndl), 2);
	if (abortbit) {
		if (!abortflg) abortflg = TRUE;
		else {
			header[0] = ABORTED;
			riosetpos(wrkhndl, 0L);
			i1 = rioput(wrkhndl, header, HDRSIZ);
			if (i1) death(DEATH_WRITE, i1, fioname(wrkhndl), 2);
			rioclose(wrkhndl);
			dspstring("CHAIN execution aborted\n");
			exit(1);
		}
	}
	msc9tooff(&header[1], &nextpos);
	msc9tooff(&header[10], &eofpos);
	msc9tooff(&header[19], &lastpos);
	signal(SIGINT, quitsig);
}

static void comment()
{
	INT i1;

	i1 = 1;
	if (record[1] == 'B' || record[1] == 'C') {
		dspchar('\007');
		i1 = 2;
	}
	dspstring(&record[i1]);
	dspchar('\n');
	dspflush();
}

static void vpause()
{
	INT i1;

	if (loghndl != -1) return;

	i1 = 1;
	if (record[1] == 'B' || record[1] == 'C') {
		dspstring("\007\007\007\007\007\007\007\007\007\007");
		i1 = 2;
	}
	if (isspace(record[i1])) i1++;
	dspstring(&record[i1]);
	dspchar('\n');
	dspflush();
	fgets(record, sizeof(record) - 1, stdin);
}

static void abortoff()
{
	abortbit = FALSE;
}

static void aborton()
{
	abortbit = TRUE;
}

static void abtif()
{
	INT i1;

	if (abortbit) {
		signal(SIGINT, SIG_IGN);
		header[0] = ABORTED;
		mscoffto9(nextpos, &header[1]);
		mscoffto9(eofpos, &header[10]);
		mscoffto9(lastpos, &header[19]);
		riosetpos(wrkhndl, 0L);
		i1 = rioput(wrkhndl, header, HDRSIZ);
		if (i1) death(DEATH_WRITE, i1, fioname(wrkhndl), 2);
		rioclose(wrkhndl);
		dspstring("CHAIN execution aborted\n");
		exit(1);
	}
}

static void errabort()
{
	abortflg = TRUE;
}

static void errgotolab()
{
	INT i1;
	CHAR name[SYMNAMSIZ + 1];

	if (abortbit) {
		strcpy(name, &record[1]);
		while (TRUE) {
			riosetpos(wrkhndl, nextpos);
			i1 = rioget(wrkhndl, (UCHAR *) record, RECMAX);
			if (i1 < 1) death(DEATH_INVWORK, 0, fioname(wrkhndl), 2);
			if (record[0] == END_OF_EXTENT) return;
			riolastpos(wrkhndl, &lastpos);
			rionextpos(wrkhndl, &nextpos);
			record[i1] = 0;
			if (record[0] == LABEL && !strcmp(name, &record[1])) return;
		}
	}
}

static void errgotopos()
{
	if (abortbit) msc9tooff((UCHAR *) &record[1], &nextpos);
}

static void gotolab()
{
	INT i1;
	CHAR name[SYMNAMSIZ + 1];

	strcpy(name, &record[1]);
	while (TRUE) {
		riosetpos(wrkhndl, nextpos);
		i1 = rioget(wrkhndl, (UCHAR *) record, RECMAX);
		if (i1 < 1) death(DEATH_INVWORK, 0, fioname(wrkhndl), 2);
		if (record[0] == END_OF_EXTENT) return;
		riolastpos(wrkhndl, &lastpos);
		rionextpos(wrkhndl, &nextpos);
		record[i1] = 0;
		if (record[0] == LABEL && !strcmp(name, &record[1])) return;
	}
}

static void gotopos()
{
	msc9tooff((UCHAR *) &record[1], &nextpos);
}

static void keyboard()
{
	INT i1;

	if (loghndl != -1) return;

	dspstring(&record[1]);
	dspchar('\n');
	dspflush();
	if (fgets(&record[1], sizeof(record) - 2, stdin) != NULL) {
		i1 = (INT)strlen(&record[1]) + 1;
		if (i1 > 1 && record[i1 - 1] == '\n') record[i1 - 1] = 0;
	}
	else record[1] = 0;
	for (i1 = 1; record[i1] && isspace(record[i1]); i1++);
	if (record[i1]) command();
}

static void logon()
{
	dspflush();
	if (loghndl != -1) {
#if OS_WIN32
		_close(loghndl);
		_dup2(stdhndl2, stdhndl);
#else
		close(loghndl);
		dup2(stdhndl2, stdhndl);
#endif
	}
/*** REMOVE THIS LINE WHEN ALL SUPPORTED ***/
	loghndl = -1;
#if OS_WIN32
	loghndl = _open(&record[1], _O_RDWR | _O_CREAT, _S_IREAD | _S_IWRITE);
#endif
#if OS_UNIX
	loghndl = open(&record[1], O_RDWR | O_CREAT, 0x1b6);
#endif
/*** IS THIS AN ERROR TO ABORT ***/
	if (loghndl == -1) dspstring("unable to open/create log file\n");
	else {
#if OS_WIN32
		_close(stdhndl);
		_dup2(loghndl, stdhndl);
#else
		close(stdhndl);
		dup2(loghndl, stdhndl);
#endif
		fseek(stdout, 0L, 2);
	}
}

static void logoff()
{
	if (loghndl != -1) {
		dspflush();
#if OS_WIN32
		_close(loghndl);
		_dup2(stdhndl2, stdhndl);
#else
		close(loghndl);
		dup2(stdhndl2, stdhndl);
#endif
		loghndl = -1;
	}
}

static void noabort()
{
	abortflg = FALSE;
}

static void soundr()
{
	INT num;
	time_t twork1, twork2;

	num = atoi(&record[1]);
	if (!num) num = 1;
	time(&twork1);
	twork2 = twork1 + num;
	while (twork1 < twork2) {
		dspchar(0x07);
		dspflush();
		time(&twork1);
	}
}

static void stamp()
{
	static CHAR stamp[] = { "+STAMP+ 0000/00/00 00:00:00 + + + + + + + + + + +\n" };
	CHAR work[16];

	msctimestamp((UCHAR *) work);
	memcpy(&stamp[8], work, 4);
	stamp[13] = work[4];
	stamp[14] = work[5];
	stamp[16] = work[6];
	stamp[17] = work[7];
	stamp[19] = work[8];
	stamp[20] = work[9];
	stamp[22] = work[10];
	stamp[23] = work[11];
	stamp[25] = work[12];
	stamp[26] = work[13];
	dspstring(stamp);
}

static void systemoff()
{
	systemflg = FALSE;
}

static void systemon()
{
	systemflg = TRUE;
}

static void terminate()
{
	INT i1;

	while (TRUE) {
		riosetpos(wrkhndl, nextpos);
		i1 = rioget(wrkhndl, (UCHAR *) record, RECMAX);
		if (i1 < 1) death(DEATH_INVWORK, 0, fioname(wrkhndl), 2);
		if (record[0] == END_OF_EXTENT) return;
		riolastpos(wrkhndl, &lastpos);
		rionextpos(wrkhndl, &nextpos);
	}
}

static void chnuto10(UINT src, UCHAR* dest)
{
	INT i;
	memset(dest, ' ', 10);
	i = 10;
	do dest[--i] = (CHAR)(src % 10 + '0');
	while ((src /= 10) && i);
}

static void quitsig(INT sig)
{
	INT i1;

	signal(sig, SIG_IGN);
	header[0] = ABORTED;
	mscoffto9(nextpos, &header[1]);
	mscoffto9(eofpos, &header[10]);
	mscoffto9(lastpos, &header[19]);
	riosetpos(wrkhndl, 0L);
	i1 = rioput(wrkhndl, header, HDRSIZ);
	dspstring("\nHALTED - user interrupt\n");
	if (i1) death(DEATH_WRITE, i1, fioname(wrkhndl), 2);
	rioclose(wrkhndl);
	exit(1);
}
