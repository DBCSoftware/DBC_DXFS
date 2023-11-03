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
#define DEATH_INVPARMVAL	2
#define DEATH_INIT			3
#define DEATH_NOMEM			4
#define DEATH_OPEN			5
#define DEATH_CREATE		6
#define DEATH_CLOSE			7
#define DEATH_READ			8
#define DEATH_WRITE			9
#define DEATH_INVALREC		10

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
	"Invalid record length or deleted record"
};

/* local declarations */
static INT dspflags;
static INT inhndl, outhndl;
static INT bufsize;
static UCHAR record[300], *buffer, **bufptr;

/* routine declarations */
static void encode(void);
static void decode(void);
static void usage(void);
static void death(INT, INT, CHAR *);
static void quitsig(INT);


INT main(INT argc, CHAR *argv[])
{
	INT i1, decflg, openflg;
	CHAR cfgname[MAX_NAMESIZE], inname[MAX_NAMESIZE], outname[MAX_NAMESIZE];
	CHAR *ptr;
	FIOPARMS parms;

	arginit(argc, argv, &i1);
	if (!i1) dspsilent();
	signal(SIGINT, quitsig);

	/* initialize */
	if (meminit(100 << 10, 0, 32) == -1) death(DEATH_INIT, ERR_NOMEM, NULL);
	cfgname[0] = 0;
	while (!argget(ARG_NEXT | ARG_IGNOREOPT, (char *) record, sizeof(record))) {
		if (record[0] == '-') {
			if (record[1] == '?') usage();
			if (toupper(record[1]) == 'C' && toupper(record[2]) == 'F' &&
			    toupper(record[3]) == 'G' && record[4] == '=') strcpy(cfgname, (char *) &record[5]);
		}
	}
	if (cfginit(cfgname, FALSE)) death(DEATH_INIT, 0, cfggeterror());
	if (prpinit(cfggetxml(), CFG_PREFIX "cfg")) ptr = fioinit(NULL, FALSE);
	else ptr = fioinit(&parms, FALSE);
	if (ptr != NULL) death(DEATH_INIT, 0, ptr);

	ptr = (CHAR *) record;
	decflg = FALSE;
	openflg = RIO_M_ERO | RIO_P_TXT | RIO_T_ANY;
	outname[0] = 0;

	/* scan input file name */
	i1 = argget(ARG_FIRST, inname, sizeof(inname));
	if (i1 < 0) death(DEATH_INIT, i1, NULL);
	if (i1 == 1) usage();

	/* get other parameters */
	for ( ; ; ) {
		i1 = argget(ARG_NEXT, ptr, sizeof(record));
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
					decflg = TRUE;
					break;
				case 'J':
					if (toupper(ptr[2]) == 'R') openflg = RIO_M_SRO | RIO_P_TXT | RIO_T_ANY;
					else openflg = RIO_M_SHR | RIO_P_TXT | RIO_T_ANY;
					break;
				case 'V':
					dspflags |= DSPFLAGS_VERBOSE;
					break;
				default:
					death(DEATH_INVPARM, 0, ptr);
			}
		}
		else if (!outname[0]) strcpy(outname, ptr);
		else death(DEATH_INVPARM, 0, ptr);
	}

	/* fix the names first */
	if (!decflg) ptr = ".dbc";
	else ptr = ".enc";
	miofixname(inname, ptr, FIXNAME_EXT_ADD);
	if (!outname[0]) {
		strcpy(outname, inname);
		i1 = FIXNAME_EXT_ADD | FIXNAME_EXT_REPLACE;
	}
	else i1 = FIXNAME_EXT_ADD;
	if (!decflg) ptr = ".enc";
	else ptr = ".dbc";
	miofixname(&outname[i1], ptr, i1);

	/* allocate buffer, bufsize must be a factor of 240 */
	bufsize = 36000;
	do {
		bufptr = memalloc(bufsize + 3, 0);
		if (bufptr != NULL) break;
		bufsize -= 2400;
	} while (bufsize >= 2400);
	if (bufsize < 2400) death(DEATH_INIT, ERR_NOMEM, NULL);

	/* open the input file */
	if (!decflg) inhndl = fioopen(inname, openflg);
	else inhndl = rioopen(inname, openflg, 7, 80);
	if (inhndl < 0) death(DEATH_OPEN, inhndl, inname);

	/* create the output file */
	if (decflg) outhndl = fioopen(outname, FIO_M_PRP | FIO_P_TXT);
	else outhndl = rioopen(outname, RIO_M_PRP | RIO_P_TXT | RIO_T_TXT, 7, 80);
	if (outhndl < 0) death(DEATH_CREATE, outhndl, outname);

	buffer = *bufptr;
	if (!decflg) {  /* encode */
		if (dspflags & DSPFLAGS_VERBOSE) {
			dspstring("Encoding in progress");
			dspflush();
		}
		encode();
		i1 = rioclose(outhndl);
		if (i1) death(DEATH_CLOSE, i1, NULL);
		i1 = fioclose(inhndl);
		if (i1) death(DEATH_CLOSE, i1, NULL);
		if (dspflags & DSPFLAGS_VERBOSE) dspstring("\rEncoding complete   \n");
	}
	else {  /* decode */
		if (dspflags & DSPFLAGS_VERBOSE) {
			dspstring("Decoding in progress");
			dspflush();
		}
		decode();
		i1 = fioclose(outhndl);
		if (i1) death(DEATH_CLOSE, i1, NULL);
		i1 = rioclose(inhndl);
		if (i1) death(DEATH_CLOSE, i1, NULL);
		if (dspflags & DSPFLAGS_VERBOSE) dspstring("\rDecoding complete   \n");
	}
	cfgexit();
	exit(0);
	return(0);
}

/* ENCODE */
/* encode the input file and write logical records */
static void encode()
{
	INT i1, i2, i3, i4;
	OFFSET filepos;
	UCHAR c1, c2, c3;

	filepos = 0;
	do {
		i2 = fioread(inhndl, filepos, buffer, bufsize);
		if (!i2) break;
		if (i2 < 0) death(DEATH_READ, i2, NULL);
		filepos += i2;
		if (i2 % 3) memset(&buffer[i2], 0, 3);
		i3 = i4 = 0;
		do {
			c1 = buffer[i3++];
			c2 = buffer[i3++];
			c3 = buffer[i3++];
			record[i4++] = (UCHAR)((c1 & 0x3f) + ' ');
			record[i4++] = (UCHAR)((c1 >> 6) + ((c2 & 0x0f) << 2) + ' ');
			record[i4++] = (UCHAR)((c2 >> 4) + ((c3 & 0x03) << 4) + ' ');
			record[i4++] = (UCHAR)((c3 >> 2) + ' ');
			if (i4 == 80 || i3 >= i2) {
				if (i3 > i2) i4 -= i3 - i2;
				i1 = rioput(outhndl, record, i4);
				if (i1) death(DEATH_WRITE, i1, NULL);
				i4 = 0;
			}
		} while (i3 < i2);
	} while (i2 == bufsize);
}

/* DECODE */
/* decode the logical records from the input file */
static void decode()
{
	INT i1, bufpos, endflg, recsize;
	OFFSET filepos;
	UCHAR c1, c2, c3, c4;

	bufpos = 0;
	filepos = 0;
	endflg = FALSE;
	while (TRUE) {
		recsize = rioget(inhndl, record, 80);
		if (recsize == -1) break;
		if (recsize == -2 || endflg) death(DEATH_INVALREC, 0, NULL);
		if (recsize < -2) death(DEATH_READ, recsize, NULL);
		if (recsize < 80) {
			if (!recsize || (recsize & 0x03) == 0x01) death(DEATH_INVALREC, 0, NULL);
			memset(&record[recsize], ' ', 4);
			endflg = TRUE;
		}
		i1 = 0;
		do {
			c1 = (UCHAR)(record[i1++] - ' ');
			c2 = (UCHAR)(record[i1++] - ' ');
			c3 = (UCHAR)(record[i1++] - ' ');
			c4 = (UCHAR)(record[i1++] - ' ');
			buffer[bufpos++] = (UCHAR)((c1 & 0x3f) + ((c2 & 0x03) << 6));
			buffer[bufpos++] = (UCHAR)(((UINT)(c2 & 0x3c) >> 2) + ((c3 & 0x0f) << 4));
			buffer[bufpos++] = (UCHAR)(((UINT)(c3 & 0x30) >> 4) + ((c4 & 0x3f) << 2));
		} while (i1 < recsize);
		if (bufpos == bufsize || endflg) {
			if (i1 > recsize) bufpos -= i1 - recsize;
			fiowrite(outhndl, filepos, buffer, bufpos);
			filepos += bufpos;
			bufpos = 0;
		}
	}
	if (bufpos) fiowrite(outhndl, filepos, buffer, bufpos);
}

/* USAGE */
static void usage()
{
	dspstring("ENCODE command  " RELEASEPROGRAM RELEASE COPYRIGHT);
	dspchar('\n');
	dspstring("Usage:  encode file1 [file2] [-CFG=cfgfile] [-D] [-J[R]] [-O=optfile] [-V]\n");
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
