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

#define DSPFLAGS_VERBOSE	0x01
#define DSPFLAGS_DSPXTRA	0x02
#define DEATH_INTERRUPT		0
#define DEATH_INVPARM		1
#define DEATH_INVPARMVAL		2
#define DEATH_INIT			3
#define DEATH_NOMEM			4
#define DEATH_OPEN			5
#define DEATH_CREATE		6
#define DEATH_CLOSE			7
#define DEATH_READ			8
#define DEATH_WRITE			9
#define DEATH_DELETE		10

/* local declarations */
static INT dspflags;
static char *errormsg[] = {
	"HALTED - user interrupt",
	"Invalid parameter ->",
	"Invalid parameter value ->",
	"Unable to initialize",
	"Unable to allocate memory for buffer",
	"Unable to open",
	"Unable to create",
	"Unable to close file",
	"Unable to read from file",
	"Unable to write to file",
	"Unable to delete file"
};

/* routine declarations */
static void usage(void);
static void death(INT, INT, CHAR *);
static void quitsig(INT);


INT main(INT argc, CHAR *argv[])
{
	INT i1, i2, inhndl, outhndl, bufsize, delflg, createflg, openflg;
	OFFSET filepos;
	CHAR cfgname[MAX_NAMESIZE], inname[MAX_NAMESIZE], outname[MAX_NAMESIZE];
	CHAR work[300], *ptr;
	UCHAR *buffer, **bufptr;
	FIOPARMS parms;

	arginit(argc, argv, &i1);
	if (!i1) dspsilent();
	signal(SIGINT, quitsig);

	/* initialize */
	if (meminit(52 << 10, 0, 16) == -1) death(DEATH_INIT, ERR_NOMEM, NULL);
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
	delflg = FALSE;
	openflg = FIO_M_ERO | FIO_P_TXT;

	/* scan input and output file name */
	i1 = argget(ARG_FIRST, inname, sizeof(inname));
	if (!i1) i1 = argget(ARG_NEXT, outname, sizeof(outname));
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
			switch(toupper(ptr[1])) {
				case '!':
					dspflags |= DSPFLAGS_VERBOSE | DSPFLAGS_DSPXTRA;
					break;
				case 'C':
					if (toupper(ptr[2]) != 'F' || toupper(ptr[3]) != 'G' || ptr[4] != '=' || !ptr[5]) death(DEATH_INVPARM, 0, ptr);
					break;
				case 'D':
					delflg = TRUE;
					break;
				case 'J':
					if (toupper(ptr[2]) == 'R') openflg = FIO_M_SRO | FIO_P_TXT;
					else openflg = FIO_M_SHR | FIO_P_TXT;
					break;
				case 'V':
					dspflags |= DSPFLAGS_VERBOSE;
					break;
				default:
					death(DEATH_INVPARM, 0, ptr);
			}
		}
		else death(DEATH_INVPARM, 0, ptr);
	}

	/* open the input file */
	miofixname(inname, ".txt", FIXNAME_EXT_ADD);
	inhndl = fioopen(inname, openflg);
	if (inhndl < 0) death(DEATH_OPEN, inhndl, inname);

	/* change prep directory to be same as input file */
	if (!prpget("file", "utilprep", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "dos")) {
		ptr = fioname(inhndl);
		if (ptr != NULL) {
			i1 = fioaslash(ptr) + 1;
			if (i1) memcpy(work, ptr, i1);
			work[i1] = 0;
			fiosetopt(FIO_OPT_PREPPATH, (UCHAR *) work);
		}
	}

	/* create the output file */
	miofixname(outname, ".txt", FIXNAME_EXT_ADD);
	createflg = FIO_M_PRP | FIO_P_TXT;
	outhndl = fioopen(outname, createflg);
	if (outhndl < 0) {
		fioclose(inhndl);
		death(DEATH_CREATE, outhndl, outname);
	}

	/* allocate buffer */
	bufsize = 48 << 10;
	do {
		bufptr = memalloc(bufsize, 0);
		if (bufptr != NULL) break;
		bufsize -= 4096;
	} while (bufsize >= 4096);
	if (bufsize < 4096) death(DEATH_INIT, ERR_NOMEM, NULL);

	/* copy the file */
	buffer = *bufptr;
	filepos = 0;
	while ((i2 = fioread(inhndl, filepos, buffer, bufsize)) > 0) {
		i1 = fiowrite(outhndl, filepos, buffer, i2);
		if (i1) death(DEATH_WRITE, i1, NULL);
		filepos += i2;
		if (dspflags & DSPFLAGS_DSPXTRA) {
			mscofftoa(filepos, (CHAR *) buffer);
			dspchar('\r');
			dspstring((CHAR *) buffer);
			dspstring(" bytes copied");
			dspflush();
		}
	}
	if (i2 < 0) death(DEATH_READ, i2, NULL);

	i1 = fioclose(outhndl);
	if (i1) death(DEATH_CLOSE, i1, NULL);
	if (!delflg) {
		i1 = fioclose(inhndl);
		if (i1) death(DEATH_CLOSE, i1, NULL);
	}
	else {
		i1 = fiokill(inhndl);
		if (i1) death(DEATH_DELETE, i1, NULL);
	}
	if (dspflags & DSPFLAGS_VERBOSE) {
		if (dspflags & DSPFLAGS_DSPXTRA) dspchar('\n');
		dspstring("Copy complete");
		if (delflg) dspstring(", file deleted\n");
		else dspchar('\n');
	}
	cfgexit();
	exit(0);
	return(0);
}

/* USAGE */
static void usage()
{
	dspstring("COPY command  " RELEASEPROGRAM RELEASE COPYRIGHT);
	dspchar('\n');
	dspstring("Usage:  copy file1 file2 [-CFG=cfgfile] [-D] [-J[R]] [-O=optfile]\n");
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
	cfgexit();
	exit(1);
}

/* QUITSIG */
static void quitsig(INT sig)
{
	signal(sig, SIG_IGN);
	dspchar('\n');
	dspstring(errormsg[DEATH_INTERRUPT]);
	dspchar('\n');
	cfgexit();
	exit(1);
}
