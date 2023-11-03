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
#define INC_SIGNAL
#define INC_ERRNO
#include "includes.h"
#include "release.h"
#include "arg.h"
#include "base.h"
#include "dbccfg.h"
#include "fio.h"
#if OS_UNIX
#include <unistd.h>
#endif

#define DSPFLAGS_VERBOSE	0x01
#define DSPFLAGS_DSPXTRA	0x02
#define DEATH_INTERRUPT		0
#define DEATH_INVPARM		1
#define DEATH_INIT			2

#define PATH_OPEN		0x01
#define PATH_PREP		0x02
#define PATH_SOURCE		0x03
#define PATH_DBC		0x04
#define PATH_EDITCFG	0x05
#define PATH_EMPTY		0x06
#define PATH_IMAGE     0x07
#define NUMPATHS 7

static char *errormsg[] = {
	"HALTED - user interrupt",
	"Invalid parameter ->",
	"Unable to initialize"
};

/* local declarations */
static INT dspflags;
static INT pathlist[NUMPATHS];
static INT pathcnt;

/* routine declarations */
static INT existop(CHAR *, CHAR *, CHAR *, CHAR *);
static void usage(void);
static void death(INT, INT, CHAR *);
static void quitsig(INT);

INT main(INT argc, CHAR *argv[])
{
	INT i1, cnt;
	CHAR cfgname[MAX_NAMESIZE], filename[MAX_NAMESIZE];
	CHAR work[300], *ptr, *ptr1, *ptr2;
	FIOPARMS parms;

	arginit(argc, argv, &i1);
	if (!i1) dspsilent();
	signal(SIGINT, quitsig);

	/* initialize */
	if (meminit(4 << 10, 0, 16) == -1) death(DEATH_INIT, ERR_NOMEM, NULL);
	cfgname[0] = 0;
	while (!argget(ARG_NEXT | ARG_IGNOREOPT, work, sizeof(work))) {
		if (work[0] == '-') {
			if (work[1] == '?') usage();
			if (toupper(work[1]) == 'C' && toupper(work[2]) == 'F' &&
			    toupper(work[3]) == 'G' && work[4] == '=') strcpy(cfgname, &work[5]);
		}
	}
	if (cfginit(cfgname, FALSE)) death(DEATH_INIT, 0, cfggeterror());
	if (prpinit(cfggetxml(), CFG_PREFIX "cfg")) ptr = fioinit(NULL, FALSE);	
	else ptr = fioinit(&parms, FALSE);
	if (ptr != NULL) death(DEATH_INIT, 0, ptr);

	ptr = work;
	pathcnt = 0;

	/* scan file name */
	i1 = argget(ARG_FIRST, filename, sizeof(filename));
	if (i1 < 0) death(DEATH_INIT, i1, NULL);
	if (i1 == 1) usage();

	/* get other parameters */
	for ( ; ; ) {
		i1 = argget(ARG_NEXT, ptr, sizeof(work));
		if (i1) {
			if (i1 < 0) death(DEATH_INIT, i1, NULL);
			break;
		}
		if (ptr[0] == '-') {
			switch (toupper(ptr[1])) {
				case '!':
					dspflags |= DSPFLAGS_VERBOSE | DSPFLAGS_DSPXTRA;
					break;
				case 'A':
					pathlist[0] = PATH_OPEN;
					pathlist[1] = PATH_PREP;
					pathlist[2] = PATH_SOURCE;
					pathlist[3] = PATH_DBC;
					pathlist[4] = PATH_EDITCFG;
					pathlist[5] = PATH_EMPTY;
					pathlist[6] = PATH_IMAGE;
					pathcnt = NUMPATHS;
					break;
				case 'C':
					if (!ptr[2]) {
						if (pathcnt < NUMPATHS) pathlist[pathcnt++] = PATH_EDITCFG;
					}
					else if (toupper(ptr[2]) != 'F' || toupper(ptr[3]) != 'G' || ptr[4] != '=' || !ptr[5]) {
						death(DEATH_INVPARM, 0, ptr);
					}
					break;
				case 'D':
					if (pathcnt < NUMPATHS) pathlist[pathcnt++] = PATH_DBC;
					break;
				case 'F':
					if (pathcnt < NUMPATHS) pathlist[pathcnt++] = PATH_OPEN;
					break;
				case 'I':
					if (pathcnt < NUMPATHS) pathlist[pathcnt++] = PATH_IMAGE;
					break;
				case 'P':
					if (pathcnt < NUMPATHS) pathlist[pathcnt++] = PATH_PREP;
					break;
				case 'S':
					if (pathcnt < NUMPATHS) pathlist[pathcnt++] = PATH_SOURCE;
					break;
				case 'V':
					dspflags |= DSPFLAGS_VERBOSE;
					break;
				case 'W':
					if (pathcnt < NUMPATHS) pathlist[pathcnt++] = PATH_EMPTY;
					break;
				default:
					death(DEATH_INVPARM, 0, ptr);
			}
		}
		else death(DEATH_INVPARM, 0, ptr);
	}
	if (!pathcnt) {
		pathlist[0] = PATH_OPEN;
		pathcnt = 1;
	}

	miofixname(filename, ".txt", FIXNAME_EXT_ADD | FIXNAME_PAR_DBCVOL | FIXNAME_PAR_MEMBER);
	miogetname(&ptr1, &ptr2);
	if (*ptr1) {
		if (!existop(filename, "vol", ptr1, "dir")) {
			dspstring("File does not exist\n");
			exit(1);
		}
	}
	else {
		if (fioaslash(filename) >= 0) {
			pathlist[0] = PATH_EMPTY;
			pathcnt = 1;
		}
		cnt = 0;
		for (i1 = 0; i1 < pathcnt; i1++) {
			switch (pathlist[i1]) {
				case PATH_OPEN:
					cnt += existop(filename, "open", "dir", NULL);
					break;
				case PATH_PREP:
					cnt += existop(filename, "prep", "dir", NULL);
					break;
				case PATH_SOURCE:
					cnt += existop(filename, "source", "dir", NULL);
					break;
				case PATH_DBC:
					cnt += existop(filename, "dbc", "dir", NULL);
					break;
				case PATH_EDITCFG:
					cnt += existop(filename, "editcfg", "dir", NULL);
					break;
				case PATH_IMAGE:
					cnt += existop(filename, "image", "dir", NULL);
					break;
				case PATH_EMPTY:	
					cnt += existop(filename, NULL, NULL, NULL);
					break;
			}
		}
		if (!cnt) {
			dspstring("File does not exist\n");
			exit(1);
		}
	}
	exit(0);
	return(0);
}

/* EXISTOP */
static INT existop(CHAR *name, CHAR *key1, CHAR *key2, CHAR *key3)
{
	INT i1, i2, cnt, dspflg, handle;
	CHAR work[MAX_NAMESIZE], path[2048], *ptr, ptr2[1024];
#if OS_WIN32
	DWORD err;
	HANDLE filehandle;
	WIN32_FIND_DATA fileinfo;
#endif

	path[0] = 0;
	if (key1 != NULL) {
		if (strcmp(key1, "vol") != 0) {
			for (i1 = 0, i2 = sizeof(path) - 1; ; i1 = PRP_NEXT) {
				if (prpget("file", key1, key2, key3, &ptr, i1)) break;
				i2 -= (INT)strlen(ptr);
				if (i1) {
					if (--i2 > 0) strcat(path, ";");
				}
				if (i2 > 0) strcat(path, ptr);
			}	
			i1 = (INT)strlen(path);
		}
		else {
			if (prpgetvol(key2, ptr2, sizeof(ptr2))) return 0;	/*fail*/
			strcat(path, ptr2);
		}
	}
	cnt = 0;
	dspflg = FALSE;
	miostrscan(path, work);
	do {
		fioaslashx(work);
		strcat(work, name);
#if OS_WIN32
		for (i1 = 0; work[i1]; i1++) work[i1] = (CHAR) toupper(work[i1]);
		filehandle = FindFirstFile(work, &fileinfo);
		if (filehandle != INVALID_HANDLE_VALUE) {
			FindClose(filehandle);
			handle = 0;
		}
		else {
			err = GetLastError();
			if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND ||
			    err == ERROR_INVALID_DRIVE) continue;
			handle = -1;
		}
#endif

#if OS_UNIX
		handle = access(work, 000);
		if (handle == -1) {
			if (errno == ENOENT) continue;
			if (errno == ENOTDIR) continue;
		}
#endif

		if (!dspflg && pathcnt > 1) {
			if (key1 != NULL) {
				dspstring(key1);
				dspstring(" path");
			}
			else {
				dspstring("Current directory");
			}
			dspstring(":\n");
		}
		if (pathcnt > 1) dspstring("     ");
		if (handle == -1) dspstring("File name or path error: ");
		else cnt++;
		dspstring(work);
		dspchar('\n');
		dspflg = TRUE;
	} while (miostrscan(path, work) == 0);
	return(cnt);
}

/* USAGE */
static void usage()
{
	dspstring("EXIST command  " RELEASEPROGRAM RELEASE COPYRIGHT);
	dspchar('\n');
	dspstring("Usage:  exist file [-A] [-C] [-CFG=cfgfile] [-D] [-F] [-I] [-P] [-S] [-W]\n");
	exit(1);
}

/* DEATH */
static void death(INT n, INT e, CHAR *s)
{
	CHAR work[17];

	if (n < (INT) (sizeof(errormsg) / sizeof(*errormsg))) dspstring(errormsg[n]);
	else {
		mscitoa(n, work);
		dspstring("*** UNKNOWN ERROR ");
		dspstring(work);
		dspstring("***");
	}
	if (e) {
		dspstring(": ");
		dspstring(fioerrstr(e));
	}
	if (s != NULL) {
		dspstring(": ");
		dspstring(s);
	}
	dspchar('\n');
	exit(1);
}

/* QUITSIG */
static void quitsig(INT sig)
{
	signal(sig, SIG_IGN);
	dspchar('\n');
	dspstring(errormsg[DEATH_INTERRUPT]);
	dspchar('\n');
	exit(1);
}
