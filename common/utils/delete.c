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
#define DEATH_INIT			2
#define DEATH_OPEN			3
#define DEATH_DELETE		4

/* local declarations */
static INT dspflags;
static char *errormsg[] = {
	"HALTED - user interrupt",
	"Invalid parameter ->",
	"Unable to initialize",
	"Unable to open",
	"Unable to delete file"
};

/* routine declarations */
static void usage(void);
static void death(INT, INT, CHAR *);
static void quitsig(INT);


INT main(INT argc, CHAR *argv[])
{
	INT i1, handle;
	CHAR cfgname[MAX_NAMESIZE], filename[MAX_NAMESIZE], work[300], *ptr;
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
				case 'C':
					if (toupper(ptr[2]) != 'F' || toupper(ptr[3]) != 'G' || ptr[4] != '=' || !ptr[5]) death(DEATH_INVPARM, 0, ptr);
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

	miofixname(filename, ".txt", FIXNAME_EXT_ADD);
	handle = fioopen(filename, FIO_M_EXC | FIO_P_TXT);
	if (handle < 0) {
		if (handle != ERR_FNOTF) death(DEATH_OPEN, handle, filename);
		dspstring("Unable to find ");
		dspstring(filename);
		dspchar('\n');
	}
	else {
		i1 = fiokill(handle);
		if (i1 < 0) death(DEATH_DELETE, i1, NULL);
		if (dspflags & DSPFLAGS_VERBOSE) dspstring("Deleted\n");
	}
	cfgexit();
	exit(0);
	return(0);
}

/* USAGE */
static void usage()
{
	dspstring("DELETE command  " RELEASEPROGRAM RELEASE COPYRIGHT);
	dspchar('\n');
	dspstring("Usage:  delete file [-CFG=cfgfile] [-V]\n");
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
