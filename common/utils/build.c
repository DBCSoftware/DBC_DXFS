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
#define INC_LIMITS
#include "includes.h"
#include "release.h"
#include "dbccfg.h"
#include "arg.h"
#include "base.h"
#include "fio.h"

#define SELMAX   200
#define SELSIZE 2048
#define RECMAX   400

#define EQUAL    0x0001
#define NOTEQUAL 0x0002
#define GREATER  0x0004
#define LESS     0x0008
#define STRING   0x0010
#define OR       0x0020

#define MAXTRANS 256	/* maximum size for translate map (-M option) */
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
#define DEATH_TOOMANYSEL		10
#define DEATH_TOOMANYRECS	11
#define DEATH_BADTRANSLATE	12
#define DEATH_MUTX_JY		13
#define DEATH_BADEOF		14

#define DSPFLAGS_VERBOSE	0x01
#define DSPFLAGS_DSPXTRA	0x02

struct seldef {
	INT pos;
	INT ptr;
	INT eqlflg;
};

struct recdef {
	OFFSET from;
	OFFSET to;
};

/* local declarations */
static INT dspflags;
static UCHAR record[RIO_MAX_RECSIZE + 4];		/* input record buffer */
static UCHAR transmap[MAXTRANS];	/* input translate map record (-M option) */
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
	"Too many selection parameters",
	"Too many record specifications",
	"Bad translate file",
	"-J option is mutually exclusive with the -Y option",
	"Input file contains EOF character before physical EOF"
};


/* routine declarations */
static void usage(void);
static void death(INT, INT, CHAR *);
static void quitsig(INT);


INT main(INT argc, CHAR *argv[])
{
	INT i1, i3, inhndl, outhndl, nonselhndl;
	UINT i2, i4, recsiz, selcnt;
	INT selhi, reccnt;
	INT selflg, selcmp, selpos, sellen, seleqlflg;
	INT createflg1, createflg2, openflg;
	OFFSET lwork, from, to, recnum, recproc;
	OFFSET eofpos, pos;
	CHAR cfgname[MAX_NAMESIZE], inname[MAX_NAMESIZE], outname[MAX_NAMESIZE];
	CHAR nonselname[MAX_NAMESIZE], transname[MAX_NAMESIZE], *ptr;
	UCHAR c1, c2, appflg, eofflg, nonselflg, orflg, transflg, xselflg;
	UCHAR *selchr, **recptrptr, **selchrptr, **selptrptr;
	struct recdef *recptr;
	struct seldef *selptr;
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

	recptrptr = memalloc(RECMAX * sizeof(struct recdef), 0);
	selchrptr = memalloc(SELSIZE * sizeof(UCHAR), 0);
	selptrptr = memalloc(SELMAX * sizeof(struct seldef), 0);
	if (recptrptr == NULL || selchrptr == NULL || selptrptr == NULL) death(DEATH_INIT, ERR_NOMEM, NULL);

	ptr = (CHAR *) record;
	recptr = (struct recdef *) *recptrptr;
	selchr = *selchrptr;
	selptr = (struct seldef *) *selptrptr;
	reccnt = selcnt = selhi = 0;
	appflg = eofflg = nonselflg = orflg = transflg = xselflg = FALSE;
	recproc = 0;
	openflg = RIO_M_ERO | RIO_P_TXT | RIO_T_ANY;
	createflg1 = RIO_M_PRP | RIO_P_TXT | RIO_T_STD | RIO_UNC;
	createflg2 = RIO_M_EXC | RIO_P_TXT | RIO_T_ANY | RIO_UNC;

	/* scan input and output file name */
	i1 = argget(ARG_FIRST, inname, sizeof(inname));
	if (!i1) i1 = argget(ARG_NEXT, outname, sizeof(outname));
	if (i1 < 0) death(DEATH_INIT, i1, NULL);
	if (i1 == 1) usage();

	/* get parameters */
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
				case 'A':
					appflg = TRUE;
					break;
				case 'C':
					if (!ptr[2]) {
						createflg1 = RIO_M_PRP | RIO_P_TXT | RIO_T_STD;
						createflg2 = RIO_M_EXC | RIO_P_TXT | RIO_T_STD;
					}
					else if (toupper(ptr[2]) != 'F' || toupper(ptr[3]) != 'G' || ptr[4] != '=' || !ptr[5]) death(DEATH_INVPARM, 0, ptr);
					break;
				case 'D':
					createflg1 = RIO_M_PRP | RIO_P_TXT | RIO_T_DAT | RIO_UNC;
					createflg2 = RIO_M_EXC | RIO_P_TXT | RIO_T_DAT | RIO_UNC;
					break;
				case 'J':
					if (toupper(ptr[2]) == 'R') openflg = RIO_M_SRO | RIO_P_TXT | RIO_T_ANY;
					else openflg = RIO_M_SHR | RIO_P_TXT | RIO_T_ANY;
					break;
				case 'N':
					transflg = TRUE;
					if (ptr[2] != '=' || !ptr[3]) death(DEATH_INVPARMVAL, 0, ptr);
					strncpy((CHAR *) transname, &ptr[3], 99);
					break;
				case 'P':
					if (selcnt == SELMAX) death(DEATH_TOOMANYSEL, 0, NULL);
					for (i1 = 2, i2 = 0; i1 < 7 && isdigit(ptr[i1]); )
						i2 = i2 * 10 + ptr[i1++] - '0';
					i4 = i2;
					if (ptr[i1] == '-') {
						for (i2 = 0, i3 = ++i1 + 5; i1 < i3 && isdigit(ptr[i1]); )
							i2 = i2 * 10 + ptr[i1++] - '0';
					}
					if (!i4 || i2 < i4 || i2 > RIO_MAX_RECSIZE) death(DEATH_INVPARMVAL, 0, ptr);
					selptr[selcnt].pos = i4 - 1;
					i2 -= i4 - 1;
					i3 = 0;
					c1 = (UCHAR) toupper(ptr[i1]);
					if (c1 == '=') i3 = EQUAL;
					else if (c1 == '#') i3 = NOTEQUAL;
					else if (c1) {
						i1++;
						c2 = (UCHAR) toupper(ptr[i1]);
						if (c1 == 'E' && c2 == 'Q') i3 = EQUAL;
						else if (c1 == 'N' && c2 == 'E') i3 = NOTEQUAL;
						else if (c1 == 'G') {
							if (c2 == 'T') i3 = GREATER;
							else if (c2 == 'E') i3 = GREATER | EQUAL;
						}
						else if (c1 == 'L') {
							if (c2 == 'T') i3 = LESS;
							else if (c2 == 'E') i3 = LESS | EQUAL;
						}
					}
					i4 = (INT)strlen(&ptr[++i1]);
					if (!i3 || !i4) death(DEATH_INVPARMVAL, 0, ptr);
					if (i2 > 1) i3 |= STRING;
					if (orflg) i3 |= OR;
					selptr[selcnt].eqlflg = i3;
					selptr[selcnt++].ptr = selhi;
					/* move string to select buffer */
					if (i2 > 1 && i4 > i2) i4 = i2;
					if (selhi + i4 >= SELSIZE) death(DEATH_TOOMANYSEL, 0, NULL);
					memcpy(&selchr[selhi], &ptr[i1], (UINT) i4);
					selhi += i4;
					if (i2 > i4) {
						memset(&selchr[selhi], ' ', i2 - i4);
						selhi += i2 - i4;
					}
					selchr[selhi++] = 0;
					orflg = FALSE;
					break;
				case 'S':
					if (ptr[2] != '=' || !ptr[3]) death(DEATH_INVPARMVAL, 0, ptr);
					strncpy(nonselname, &ptr[3], 99);
					nonselflg = TRUE;
					break;
				case 'T':
					if (ptr[2] == '=') {
						strcpy(cfgname, &ptr[3]);
						for (i1 = 0; cfgname[i1]; i1++) cfgname[i1] = (CHAR) toupper(cfgname[i1]);
						if (!strcmp(cfgname, "BIN")) i1 = RIO_T_BIN;
						else if (!strcmp(cfgname, "CRLF")) i1 = RIO_T_DOS;
						else if (!strcmp(cfgname, "DATA")) i1 = RIO_T_DAT;
						else if (!strcmp(cfgname, "MAC")) i1 = RIO_T_MAC;
						else if (!strcmp(cfgname, "STD")) i1 = RIO_T_STD;
						else if (!strcmp(cfgname, "TEXT")) i1 = RIO_T_TXT;
						else death(DEATH_INVPARMVAL, 0, ptr);
						createflg1 = (createflg1 & ~RIO_T_MASK) | i1;
						createflg2 = (createflg2 & ~RIO_T_MASK) | i1;
					}
					else {
						createflg1 = RIO_M_PRP | RIO_P_TXT | RIO_T_TXT | RIO_UNC;
						createflg2 = RIO_M_EXC | RIO_P_TXT | RIO_T_TXT | RIO_UNC;
					}
					break;
				case 'V':
					dspflags |= DSPFLAGS_VERBOSE;
					break;
				case 'X':
					xselflg = TRUE;
					break;
				case 'Y':
					eofflg = TRUE;
					break;
				default:
					death(DEATH_INVPARM, 0, ptr);
			}
		}
		else if (isdigit(ptr[0])) {
			if (reccnt == RECMAX) death(DEATH_TOOMANYRECS, 0, NULL);
			for (i1 = 0, lwork = 0; i1 < 12 && isdigit(ptr[i1]); )
				lwork = lwork * 10 + ptr[i1++] - '0';
			recptr[reccnt].from = lwork;
			if (ptr[i1] == '-') {
				for (i1++, lwork = 0L, i3 = 0; i3 < 12 && isdigit(ptr[i1]); i3++)
					lwork = lwork * 10 + ptr[i1++] - '0';
			}
			if (ptr[i1] || !recptr[reccnt].from || lwork < recptr[reccnt].from) death(DEATH_INVPARMVAL, 0, ptr);
			recptr[reccnt].from--;
			recptr[reccnt++].to = lwork;
		}
		else if (toupper(ptr[0]) == 'O' && toupper(ptr[1]) == 'R' && !ptr[2]) {
			if (selcnt) orflg = TRUE;
		}
		else death(DEATH_INVPARM, 0, ptr);
	}

	if (eofflg && (openflg & FIO_M_MASK) != RIO_M_ERO) death(DEATH_MUTX_JY, 0, NULL);
	if (!reccnt) {
		recptr[0].from = 0;
		/* do this because there is no reliable define for double longs */
		recptr[0].to = 0x7FFFFFFF;
		i1 = sizeof(recptr[0].to);
		if (i1 == 8) recptr[0].to <<= 17;
		reccnt = 1;
	}

	i1 = memchange(recptrptr, reccnt * sizeof(struct recdef), 0);
	if (!i1) i1 = memchange(selchrptr, selhi * sizeof(UCHAR), 0);
	if (!i1) i1 = memchange(selptrptr, selcnt * sizeof(struct seldef), 0);
	if (i1) death(DEATH_INIT, ERR_NOMEM, NULL);

	/* calculate size of rio buffers */
	memcompact();
	membase((UCHAR **) &ptr, &i2, &i3);
	i3 -= i2;
	i1 = 8;
	if (i3 < (80 << 10)) {
		do i3 <<= 1;
		while (--i1 > 1 && i3 < (96 << 10));
	}
	
	/* open the input file */
	inhndl = rioopen(inname, openflg, i1, RIO_MAX_RECSIZE);
	if (inhndl < 0) death(DEATH_OPEN, inhndl, inname);

	/* change prep directory to be same as input file */
	if (!prpget("file", "utilprep", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "dos")) {
		ptr = fioname(inhndl);
		if (ptr != NULL) {
			i1 = fioaslash(ptr) + 1;
			if (i1) memcpy(record, ptr, (UINT) i1);
			record[i1] = 0;
			fiosetopt(FIO_OPT_PREPPATH, record);
		}
	}

	/* open or create the output file */
build1:
	if (!appflg || (outhndl = rioopen(outname, createflg2, i1, RIO_MAX_RECSIZE)) == ERR_FNOTF) {
		outhndl = rioopen(outname, createflg1, i1, RIO_MAX_RECSIZE);
	}
	if (outhndl < 0) death(DEATH_CREATE, outhndl, outname);
	if (appflg) {
		if (riotype(outhndl) == RIO_T_ANY) {
			for ( ; ; ) {
				recsiz = rioget(outhndl, record, RIO_MAX_RECSIZE);
				if ((INT)recsiz >= -1) break;
				if ((INT)recsiz == -2) continue;
				death(DEATH_READ, recsiz, NULL);
			}
			if (riotype(outhndl) == RIO_T_ANY) {
				rioclose(outhndl);
				appflg = FALSE;
				createflg1 = RIO_M_PRP | RIO_P_TXT | RIO_T_TXT | RIO_UNC;
				goto build1;
			}
		}
		rioeofpos(outhndl, &pos);
		riosetpos(outhndl, pos);
	}

	/* open non-selection criteria output file */
	if (!selcnt) nonselflg = FALSE;
	else if (nonselflg && (nonselhndl = rioopen(nonselname, createflg1, 7, RIO_MAX_RECSIZE)) < 0) death(DEATH_CREATE, nonselhndl, nonselname);

	/* read translate map file */
	if (transflg) {
		if ((i1 = fioopen(transname, FIO_M_SRO | FIO_P_SRC)) < 0) death(DEATH_OPEN, i1, transname);
		if (fioread(i1, 0L, transmap, MAXTRANS) != MAXTRANS) death(DEATH_BADTRANSLATE, 0, transname);
		fioclose(i1);
	}

	/* restore the memory pointers */
	recptr = (struct recdef *) *recptrptr;
	selchr = *selchrptr;
	selptr = (struct seldef *) *selptrptr;

	if (dspflags & DSPFLAGS_VERBOSE) dspstring("Build in progress\n");

	/* loop on each record spec */
	recnum = 0;
	for (i1 = 0; i1 < reccnt; i1++) {
		from = recptr[i1].from;
		to = recptr[i1].to;
		if (recnum > from) {
			riosetpos(inhndl, 0L);
			recnum = 0;
		}

		while (recnum < to) {  /* skip to right record in input file */
			recsiz = rioget(inhndl, record, RIO_MAX_RECSIZE);
			if ((INT)recsiz < 0) {
				if ((INT)recsiz == -1) {  /* end of file */
					if (eofflg) {
						riolastpos(inhndl, &pos);
						rioeofpos(inhndl, &eofpos);
						if (pos != eofpos) death(DEATH_BADEOF, 0, NULL);
					}
					break;
				}
				if ((INT)recsiz < -2) death(DEATH_READ, recsiz, NULL);
				continue;
			}
			if (transflg) {
				for (i2 = 0; i2 < recsiz; i2++) record[i2] = transmap[record[i2]];
			}
			if (++recnum > from) {
				if (selcnt) {
					selflg = 1;
					for (i4 = 0; i4 < selcnt; i4++) {
						if (selptr[i4].eqlflg & OR) {
							if (selflg) break;
							selflg = 1;
						}
						selpos = selptr[i4].pos;
						seleqlflg = selptr[i4].eqlflg;
						i3 = selptr[i4].ptr;
						if (seleqlflg & STRING) sellen = (INT)strlen((CHAR *) &selchr[i3]);
						else sellen = 1;
						if ((UINT)(selpos + sellen) > recsiz) selflg = 0;
						if (!selflg) continue;
						if (sellen > 1) selcmp = memcmp(&record[selpos], &selchr[i3], sellen);
						else {
							c1 = record[selpos];
							if (seleqlflg & (GREATER | LESS)) selcmp = (INT) c1 - selchr[i3];
							else {
								for ( ; selchr[i3] && c1 != selchr[i3]; i3++);
								selcmp = !selchr[i3];
							}
						}
						/* used the 'else' for readability */
						if (((seleqlflg & EQUAL) && !selcmp) ||
						    ((seleqlflg & NOTEQUAL) && selcmp) ||
						    ((seleqlflg & GREATER) && selcmp > 0) ||
						    ((seleqlflg & LESS) && selcmp < 0)) /* do nothing here */ ;
						else selflg = 0;
					}
					if (xselflg) selflg = !selflg;
					if (!selflg && !nonselflg) continue;
					if (selflg) i2 = rioput(outhndl, record, recsiz);
					else i2 = rioput(nonselhndl, record, recsiz);
				}
				else i2 = rioput(outhndl, record, recsiz);
				if (i2) death(DEATH_WRITE, i2, NULL);
				if (dspflags & DSPFLAGS_DSPXTRA) {
					recproc++;
					if (!(recproc & 0x01ff)) {
						mscofftoa(recproc, (CHAR *) record);
						dspchar('\r');
						dspstring((CHAR *) record);
						dspstring(" records processed");
						dspflush();
					}
				}
			}
		}
	}

	if (nonselflg) {
		i1 = rioclose(nonselhndl);
		if (i1) death(DEATH_CLOSE, i1, NULL);
	}
	i1 = rioclose(outhndl);
	if (i1) death(DEATH_CLOSE, i1, NULL);
	i1 = rioclose(inhndl);
	if (i1) death(DEATH_CLOSE, i1, NULL);
	if (dspflags & DSPFLAGS_DSPXTRA) {
		mscofftoa(recproc, (CHAR *) record);
		dspstring("\rBuild complete, ");
		dspstring((CHAR *) record);
		dspstring(" records processed\n");
	}
	else if (dspflags & DSPFLAGS_VERBOSE) dspstring("Build complete\n");
	cfgexit();
	exit(0);
	return(0);
}

/* USAGE */
static void usage()
{
	dspstring("BUILD command  " RELEASEPROGRAM RELEASE COPYRIGHT);
	dspchar('\n');
	dspstring("Usage:  build file1 file2 [rec-spec...] [-A] [-C] [-CFG=cfgfile] [-D] [-J[R]]\n");
	dspstring("              [-N=transfile] [-O=optfile] [OR] [-Pn[-n]EQc[c...]]\n");
	dspstring("              [-Pn[-n]NEc[c...]] [-Pn[-n]GTc[c...]] [-Pn[-n]GEc[c...]]\n");
	dspstring("              [-Pn[-n]LTc[c...]] [-Pn[-n]LEc[c...]] [-S=notselfile] [-V]\n");
	dspstring("              [-T[=BIN|CRLF|DATA|STD|MAC|TEXT]] [-X] [-Y] [-!]\n");
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
	signal(sig, SIG_IGN); // @suppress("Type cannot be resolved")
	dspchar('\n');
	dspstring(errormsg[DEATH_INTERRUPT]);
	dspchar('\n');
	cfgexit();
	exit(1);
}
