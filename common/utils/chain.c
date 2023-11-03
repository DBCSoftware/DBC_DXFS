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
#include "chain.h"
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
#define DEATH_NOLOOP		10
#define DEATH_INVWORK		11
#define DEATH_TOOMANYSYM		12
#define DEATH_CIRCULAR		13
#define DEATH_INVDIRECT		14
#define DEATH_INVNUM		15
#define DEATH_NOQUOTE		16
#define DEATH_NOPAREN		17
#define DEATH_INVCHAR		18
#define DEATH_TOOMANYEXP		19
#define DEATH_INVEXP		20
#define DEATH_PROG1			21
#define DEATH_NOOP			22
#define DEATH_NOFILE		23
#define DEATH_DOUNTIL		24
#define DEATH_WHILEEND		25
#define DEATH_IFXIF			26
#define DEATH_IFELSEXIF		27
#define DEATH_TOOMANYLOOP	28
#define DEATH_TOOMANYINC		29
#define DEATH_TOOMANYLAB		30
#define DEATH_DUPLAB		31
#define DEATH_INVOPER		32
#define DEATH_WRITEEOF		33
#define DEATH_WRITEDEL		34
#define DEATH_ZERODIV		35

static char *errormsg[] = {
	"HALTED - user interrupt",
	"Invalid parameter ->",
	"Invalid parameter value ->",
	"Unable to initialize",
	"Unable to allocate memory for buffers",
	"Unable to open",
	"Unable to create",
	"Unable to close file",
	"Unable to read from file",
	"Unable to write to file",
	"Unmatched DO/UNTIL or WHILE/END loop",
	"Invalid work file",
	"Too many symbol entries for available memory",
	"Circular replacement loop",
	"Invalid directive",
	"Invalid numeric value",
	"Unmatched quotes",
	"Unmatched parenthesis",
	"Invalid character or operator",
	"Too many expression evaluations",
	"Invalid expression",
	"Unable to determine operator",
	"No operand or expression",
	"File type symbol rquired or file not open",
	"Unmatched DO/UNTIL",
	"Unmatched WHILE/END",
	"Unmatched IF/XIF",
	"Unmatched IF/ELSE/XIF",
	"Too many nested DO/UNTIL or WHILE/END loops",
	"Too many nested includes",
	"Too many labels for available memory",
	"Duplicate labels",
	"Missing or invalid operand",
	"Attempted to rewrite record at EOF",
	"Attempted to rewrite deleted record",
	"Attempted to divide by zero"
};

/* local declarations */
static INT dspflags;

/* routine definitions */
static void quitsig(INT);


INT main(INT argc, CHAR *argv[])
{
	INT i1, execvalue;
	CHAR cfgname[MAX_NAMESIZE], workname[MAX_NAMESIZE], work[300], *ptr;
	UCHAR **pptr;
	FIOPARMS parms;

	arginit(argc, argv, &i1);
	if (!i1) dspsilent();
	signal(SIGINT, quitsig);

	/* initialize */
	if (meminit(512 << 10, 0, 16) == -1) death(DEATH_INIT, ERR_NOMEM, NULL, 0);
	cfgname[0] = 0;
	while (!argget(ARG_NEXT | ARG_IGNOREOPT, work, sizeof(work))) {
		if (work[0] == '-') {
			if (work[1] == '?') usage();
			if (toupper(work[1]) == 'C' && toupper(work[2]) == 'F' &&
			    toupper(work[3]) == 'G' && work[4] == '=') strcpy(cfgname, &work[5]);
		}
	}
	if (cfginit(cfgname, FALSE)) death(DEATH_INIT, 0, cfggeterror(), 0);
	if (prpinit(cfggetxml(), CFG_PREFIX "cfg")) ptr = fioinit(NULL, FALSE);
	else ptr = fioinit(&parms, FALSE);
	if (ptr != NULL) death(DEATH_INIT, 0, ptr, 0);

	ptr = work;
	execvalue = 0;
#if OS_WIN32
	strcpy(workname, ".\\CHAIN.WRK");
#else
	strcpy(workname, "CHAIN.WRK");
#endif

	/* get parameters */
	for (i1 = ARG_FIRST; ; i1 = ARG_NEXT) {
		i1 = argget(i1, ptr, sizeof(work));
		if (i1) {
			if (i1 < 0) death(DEATH_INIT, i1, NULL, 0);
			break;
		}
		if (ptr[0] == '-') {
	 		switch (toupper(ptr[1])) {
				case '!':
					dspflags |= DSPFLAGS_VERBOSE | DSPFLAGS_DSPXTRA;
					break;
				case 'C':
					if (!ptr[2]) execvalue |= COMPILE;
					else if (toupper(ptr[2]) != 'F' || toupper(ptr[3]) != 'G' || ptr[4] != '=' || !ptr[5]) death(DEATH_INVPARM, 0, ptr, 0);
					break;
				case 'N':
					execvalue |= RESTART2;
					break;
				case 'P':
					execvalue |= RESTART1;
					break;
				case 'S':
					execvalue |= SYSTEM;
					break;
				case 'V':
					dspflags |= DSPFLAGS_VERBOSE;
					break;
				case 'W':
					if (ptr[2] != '=' || !ptr[3]) death(DEATH_INVPARM, 0, ptr, 0);
					strcpy(workname, &ptr[3]);
					miofixname(workname, ".wrk", FIXNAME_EXT_ADD);
					break;
			}
		}
	}

	if (!(execvalue & (RESTART1 | RESTART2))) {
		if (dspflags & DSPFLAGS_VERBOSE) dspstring("CHAIN compilation\n");
		execvalue |= compile(workname);
	}
	if (!(execvalue & COMPILE)) {
		/* free some of the memory */
		pptr = memalloc(4096, 0);
		if (pptr != NULL && meminit(8 << 10, 0, 16) == -1) death(DEATH_INIT, ERR_NOMEM, NULL, 2);
		memfree(pptr);
		if (dspflags & DSPFLAGS_VERBOSE) dspstring("CHAIN execution\n");
		execution(execvalue, workname);
	}
	cfgexit();
	exit(0);
	return(0);
}

/* USAGE */
void usage()
{
	dspstring("CHAIN command  " RELEASEPROGRAM RELEASE COPYRIGHT);
	dspchar('\n');
	dspstring("Usage:  chain file [list] [-C] [-CFG=cfgfile] [-D] [-I] [-J[R]] [-N]\n");
	dspstring("              [-O=optfile] [-P] [-S] [-T] [-V] [-W=workfile]\n");
	exit(1);
}

/* DEATH */
void death(INT n, INT e, CHAR *s, INT phase)
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
	if (phase == 1) dspstring("CHAIN compilation aborted\n");
	else if (phase == 2) dspstring("CHAIN execution aborted\n");
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
