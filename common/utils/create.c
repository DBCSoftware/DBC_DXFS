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
#define DEATH_CREATE		5
#define DEATH_CLOSE			6
#define DEATH_WRITE			7

/* local declarations */
static INT dspflags;
static UCHAR record[RIO_MAX_RECSIZE + 4];

static char *errormsg[] = {
	"HALTED - user interrupt",
	"Invalid parameter ->",
	"Invalid parameter value ->",
	"Unable to initialize",
	"Unable to allocate memory for buffer",
	"Unable to create",
	"Unable to close file",
	"Unable to write to file"
};

/* routine declarations */
static void usage(void);
static void death(INT, INT, CHAR *);
static void quitsig(INT);


INT main(INT argc, CHAR *argv[])
{
	INT i1, createflg, handle, len;
	OFFSET num;
	CHAR cfgname[MAX_NAMESIZE], filename[MAX_NAMESIZE], *ptr;
	FIOPARMS parms;

	arginit(argc, argv, &i1);
	if (!i1) dspsilent();
	signal(SIGINT, quitsig);

	/* initialize */
	if (meminit(116 << 10, 0, 16) == -1) death(DEATH_INIT, ERR_NOMEM, NULL);
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
	len = 80;
	num = 0L;
	createflg = RIO_M_PRP | RIO_P_TXT | RIO_T_STD | RIO_UNC;

	/* scan file name */
	i1 = argget(ARG_FIRST, filename, sizeof(filename));
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
			switch (toupper(ptr[1])) {
				case '!':
					dspflags |= DSPFLAGS_VERBOSE | DSPFLAGS_DSPXTRA;
					break;
				case 'C':
					if (toupper(ptr[2]) != 'F' || toupper(ptr[3]) != 'G' || ptr[4] != '=' || !ptr[5]) death(DEATH_INVPARM, 0, ptr);
					break;
				case 'D':
					createflg = RIO_M_PRP | RIO_P_TXT | RIO_T_DAT | RIO_UNC;
					break;
				case 'L':
					if (ptr[2] != '=') death(DEATH_INVPARMVAL, 0, ptr);
					len = atoi(&ptr[3]);
					if (len < 1 || len > RIO_MAX_RECSIZE) death(DEATH_INVPARMVAL, 0, ptr);
					break;
				case 'N':
					if (ptr[2] != '=') death(DEATH_INVPARMVAL, 0, ptr);
					mscatooff(&ptr[3], &num);
					break;
				case 'T':
					if (ptr[2] == '=') {
						strcpy(cfgname, &ptr[3]);
						for (i1 = 0; cfgname[i1]; i1++) cfgname[i1] = (CHAR) toupper(cfgname[i1]);
						if (!strcmp(cfgname, "BIN")) createflg = RIO_M_PRP | RIO_P_TXT | RIO_T_BIN | RIO_UNC;
						else if (!strcmp(cfgname, "CRLF")) createflg = RIO_M_PRP | RIO_P_TXT | RIO_T_DOS | RIO_UNC;
						else if (!strcmp(cfgname, "DATA")) createflg = RIO_M_PRP | RIO_P_TXT | RIO_T_DAT | RIO_UNC;
						else if (!strcmp(cfgname, "MAC")) createflg = RIO_M_PRP | RIO_P_TXT | RIO_T_MAC | RIO_UNC;
						else if (!strcmp(cfgname, "STD")) createflg = (createflg & ~RIO_T_MASK) | RIO_T_STD;
						else if (!strcmp(cfgname, "TEXT")) createflg = RIO_M_PRP | RIO_P_TXT | RIO_T_TXT | RIO_UNC;
						else death(DEATH_INVPARMVAL, 0, ptr);
					}
					else createflg = RIO_M_PRP | RIO_P_TXT | RIO_T_TXT | RIO_UNC;
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

	if (num) {
		memset(record, ' ', len);
		i1 = 8;
	}
	else i1 = len = 0;
	handle = rioopen(filename, createflg, i1, len);
	if (handle < 0) death(DEATH_CREATE, handle, filename);

	/* write records */
	while (num--) {
		i1 = rioput(handle, record, len);
		if (i1) death(DEATH_WRITE, i1, NULL);
	}

	i1 = rioclose(handle);
	if (i1) death(DEATH_CLOSE, i1, NULL);
	if (dspflags & DSPFLAGS_VERBOSE) dspstring("Created\n");
	cfgexit();
	exit(0);
	return(0);
}

/* USAGE */
static void usage()
{
	dspstring("CREATE command  " RELEASEPROGRAM RELEASE COPYRIGHT);
	dspchar('\n');
	dspstring("Usage:  create file [-CFG=cfgfile] [-D] [-L=n] [-N=n] [-O=optfile] [-T[=type]] [-V]\n");
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
