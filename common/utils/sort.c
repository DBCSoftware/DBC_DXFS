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
#include "arg.h"
#include "base.h"
#include "dbccfg.h"
#include "fio.h"
#include "sio.h"

#if OS_UNIX
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef S_IFDIR
#define S_IFDIR 0x4000
#endif
#ifndef S_IFMT
#define S_IFMT  0xF000
#endif

#ifndef S_ISDIR
#define S_ISDIR(mode)  ((mode & S_IFMT) == S_IFDIR)
#endif
#endif


#define SELMAX   200
#define SELSIZE 2048
#define KEYMAX   200

#define EQUAL    0x0001
#define NOTEQUAL 0x0002
#define GREATER  0x0004
#define LESS     0x0008
#define STRING   0x0010
#define OR       0x0020

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
#define DEATH_COLLATE		10
#define DEATH_TOOMANYSEL		11
#define DEATH_TOOMANYKEY		12
#define DEATH_MUTX_JY		13
#define DEATH_BADEOF		14

struct seldef {
	INT pos;
	INT ptr;
	INT eqlflg;
};

/* local declarations */
static INT dspflags;
static INT inhndl;
static INT outhndl = -1;
static CHAR inname[100];
static CHAR outname[100];
static INT openflg;
static INT createflg;
static INT keycnt, qkeycnt;
static SIOKEY *keyptr, *qkeyptr;
static INT selcnt;
static UCHAR *selchr;
static struct seldef *selptr;
static SIOKEY **keyptrptr, **qkeyptrptr;
static UCHAR **selchrptr, **selptrptr;
static INT stable;
static UCHAR quikflg, keyflg, eofflg, fposflg, uniqflg, xselflg;
static INT firstrec;
static INT maxlen;
static INT maxrec;
static INT recpos, recsqpos, recgpos;
static INT reclen;
static INT highkey;
static UCHAR record[RIO_MAX_RECSIZE + 4];
static OFFSET reccnt;
static INT gpos;
static UCHAR geqlflg;
static UCHAR gchr, gtype;
static UCHAR headpos[32];
static INT tagsize;
static UCHAR collateflg;
static UCHAR priority[UCHAR_MAX + 1];	/* holds priority of characters */
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
	"Error with collate file, unable to open, wrong size, or unable to read",
	"Too many selection parameters",
	"Too many keys",
	"-J option is mutually exclusive with the -Y option",
	"Text file contains EOF character before physical EOF"
};

/* routine declarations */
static INT sortin(UCHAR *);
static INT sortout(UCHAR *);
static INT compkey(UCHAR *, UCHAR *, INT);
static void dspreccnt(void);
static void usage(void);
static void death(INT, INT, CHAR *);
static void quitsig(INT);


INT main(INT argc, CHAR *argv[])
{
	INT i1, i2, i3, i4, selhi, memsize;
	CHAR cfgname[MAX_NAMESIZE], workname[MAX_NAMESIZE], *ptr;
	CHAR *workdir, *workfile;
	UCHAR c1, c2, sortflg, orflg, **pptr;
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

	/* allocate memory for structures */
	keyptrptr = (SIOKEY **) memalloc((KEYMAX + 2) * sizeof(SIOKEY), 0);
	selchrptr = memalloc(SELSIZE * sizeof(UCHAR), 0);
	selptrptr = memalloc(SELMAX * sizeof(struct seldef), 0);
	if (keyptrptr == NULL || selchrptr == NULL || selptrptr == NULL) death(DEATH_INIT, ERR_NOMEM, NULL);

	ptr = (CHAR *) record;
	keyptr = *keyptrptr;
	selchr = *selchrptr;
	selptr = (struct seldef *) *selptrptr;
	selcnt = selhi = 0;
	highkey = keycnt = maxlen = memsize = 0;
	eofflg = fposflg = keyflg = orflg = FALSE;
	quikflg = uniqflg = xselflg = FALSE;
	gtype = 0;
	stable = 0;
	sortflg = SIO_ASCEND;
	openflg = RIO_M_ERO | RIO_P_TXT | RIO_T_ANY;
	createflg = RIO_M_PRP | RIO_P_TXT | RIO_T_STD | RIO_UNC;
	workname[0] = 0;

	/* scan input and output file names */
	i1 = argget(ARG_FIRST, inname, sizeof(inname));
	if (!i1) i1 = argget(ARG_NEXT, outname, sizeof(outname));
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
				case 'A':
					if (ptr[2] != '=') death(DEATH_INVPARMVAL, 0, ptr);
					memsize = atoi(&ptr[3]) << 10;
					break;
				case 'C':
					if (!ptr[2]) createflg = RIO_M_PRP | RIO_P_TXT | RIO_T_STD;
					else if (toupper(ptr[2]) != 'F' || toupper(ptr[3]) != 'G' || ptr[4] != '=' || !ptr[5]) death(DEATH_INVPARM, 0, ptr);
					break;
				case 'D':
					createflg = RIO_M_PRP | RIO_P_TXT | RIO_T_DAT | RIO_UNC;
					break;
				case 'F':
					fposflg = quikflg = TRUE;
					break;
				case 'G':
				case 'H':
					if (gtype) death(DEATH_INVPARM, 0, ptr);  /* -H or -G already done */
					gtype = (UCHAR) toupper(ptr[1]);
					if (gtype == 'H') quikflg = TRUE;
					for (i1 = 2, gpos = 0; i1 < 7 && isdigit(ptr[i1]); )
						gpos = gpos * 10 + ptr[i1++] - '0';
					if (gpos < 1 || gpos > RIO_MAX_RECSIZE) death(DEATH_INVPARMVAL, 0, ptr);
					gpos--;
					if (ptr[i1] == '=') geqlflg = TRUE;
					else if (ptr[i1] == '#') geqlflg = FALSE;
					else death(DEATH_INVPARMVAL, 0, ptr);
					gchr = ptr[i1 + 1];
					if (!gchr) death(DEATH_INVPARMVAL, 0, ptr);
					break;
				case 'J':
					if (toupper(ptr[2]) == 'R') openflg = RIO_M_SRO | RIO_P_TXT | RIO_T_ANY;
					else openflg = RIO_M_SHR | RIO_P_TXT | RIO_T_ANY;
					break;
				case 'K':
					keyflg = quikflg = TRUE;
					break;
				case 'M':
					if (ptr[2] != '=') death(DEATH_INVPARMVAL, 0, ptr);
					i1 = atoi(&ptr[3]);
					if (i1 < 1 || i1 > RIO_MAX_RECSIZE) death(DEATH_INVPARMVAL, 0, ptr);
					maxlen = i1;
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
				case 'Q':
					quikflg = TRUE;
					break;
				case 'R':
					sortflg = SIO_DESCEND;
					break;
				case 'S':
					stable = 1;
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
				case 'U':
					if (!ptr[2]) uniqflg = TRUE;
					break;
				case 'V':
					dspflags |= DSPFLAGS_VERBOSE;
					break;
				case 'W':
					if (ptr[2] != '=' || !ptr[3]) death(DEATH_INVPARMVAL, 0, ptr);
					strcpy(workname, &ptr[3]);
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
			if (keycnt == KEYMAX) death(DEATH_TOOMANYKEY, 0, NULL);
			for (i1 = 0, i2 = 0; i1 < 5 && isdigit(ptr[i1]); )
				i2 = i2 * 10 + ptr[i1++] - '0';
			keyptr[keycnt].start = i2 - 1;
			if (ptr[i1] == '-') {
				for (i1++, i2 = 0, i3 = 0; i3 < 5 && isdigit(ptr[i1]); i3++)
					i2 = i2 * 10 + ptr[i1++] - '0';
			}
			keyptr[keycnt].typeflg = 0;
			for ( ; ; ) {
				c1 = (UCHAR) toupper(ptr[i1]);
				if (c1 != 'A' && c1 != 'D' && c1 != 'N') break;
				if (c1 == 'A') {
					if (keyptr[keycnt].typeflg & SIO_DESCEND) death(DEATH_INVPARMVAL, 0, ptr);
					keyptr[keycnt].typeflg |= SIO_ASCEND;
				}
				else if (c1 == 'D') {
					if (keyptr[keycnt].typeflg & SIO_ASCEND) death(DEATH_INVPARMVAL, 0, ptr);
					keyptr[keycnt].typeflg |= SIO_DESCEND;
				}
				else keyptr[keycnt].typeflg |= SIO_NUMERIC;
				c1 = ptr[++i1];
			}
			if (c1 || keyptr[keycnt].start < 0 || i2 <= keyptr[keycnt].start || i2 > RIO_MAX_RECSIZE) death(DEATH_INVPARMVAL, 0, ptr);
			keyptr[keycnt].end = i2;
			keycnt++;
			if (i2 > highkey) highkey = i2;
		}
		else if (toupper(ptr[0]) == 'O' && toupper(ptr[1]) == 'R' && !ptr[2]) {
			if (selcnt) orflg = TRUE;
		}
		else death(DEATH_INVPARM, 0, ptr);
	}

	if (!keycnt) usage();
	if (eofflg && (openflg & FIO_M_MASK) != RIO_M_ERO) death(DEATH_MUTX_JY, 0, NULL);
	for (i1 = 0; i1 < keycnt; i1++) if (!keyptr[i1].typeflg) keyptr[i1].typeflg = sortflg;

	i1 = memchange((UCHAR **) keyptrptr, (keycnt + 2) * sizeof(SIOKEY), 0);
	if (!i1) i1 = memchange(selchrptr, selhi * sizeof(UCHAR), 0);
	if (!i1) i1 = memchange(selptrptr, selcnt * sizeof(struct seldef), 0);
	if (i1) death(DEATH_INIT, ERR_NOMEM, NULL);

	if (quikflg) {  /* allocate memory for compressing the keys together */
		qkeyptrptr = (SIOKEY **) memalloc(keycnt * sizeof(SIOKEY), 0);
		if (qkeyptrptr == NULL) death(DEATH_INIT, ERR_NOMEM, NULL);
	}

	/* get the collate file */
	collateflg = FALSE;
	pptr = fiogetopt(FIO_OPT_COLLATEMAP);
	if (pptr != NULL) {
		memcpy(priority, *pptr, UCHAR_MAX + 1);
		collateflg = TRUE;
	}

	/* reserve memory for fioopen of work file (careful to not use pptr till freed) */
	pptr = memalloc(256, 0);
	if (pptr == NULL) death(DEATH_INIT, ERR_NOMEM, NULL);

	/* open the input file */
	inhndl = rioopen(inname, openflg, 7, RIO_MAX_RECSIZE);
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

	/* figure out file type and maximum record size */
	if (quikflg) maxlen = RIO_MAX_RECSIZE;
	else if (!maxlen || highkey > maxlen) {
		if (!maxlen) {
			while ((maxlen = rioget(inhndl, record, RIO_MAX_RECSIZE)) == -2);
			if (maxlen <= -3) death(DEATH_READ, maxlen, NULL);
			if (maxlen == -1) maxlen = RIO_MAX_RECSIZE;
			riosetpos(inhndl, 0L);
		}
		if (highkey > maxlen) maxlen = highkey;
		mscitoa(maxlen, (CHAR *) record);
		if (dspflags & DSPFLAGS_VERBOSE) {
			dspstring("Maximum record length assumed to be ");
			dspstring((CHAR *) record);
			dspchar('\n');
		}
	}

	keyptr = *keyptrptr;
	reclen = maxlen;
	recpos = recsqpos = recgpos = 0;

	/* compress the keys together */
	if (quikflg) {
		qkeyptr = *qkeyptrptr;
		reclen = 0;
		for (i1 = 0; i1 < keycnt; i1++) {
			qkeyptr[i1].start = keyptr[i1].start;
			qkeyptr[i1].end = keyptr[i1].end - qkeyptr[i1].start;
			keyptr[i1].start = reclen;
			reclen += qkeyptr[i1].end;
			keyptr[i1].end = reclen;
		}
		qkeycnt = keycnt;
		maxrec = 0;
	}

	/* get key tag size */
	tagsize = 12;
	if (!prpget("keytag", NULL, NULL, NULL, &ptr, PRP_LOWER) && !strcmp("old", ptr)) tagsize = 9;

	/* prefix keytag size byte record position and make it's sort order be last */
	if (stable || quikflg) {
		for (i1 = 0; i1 < keycnt; i1++) {
			keyptr[i1].start += tagsize;
			keyptr[i1].end += tagsize;
		}
		if (stable) {
			keyptr[keycnt].start = 0;
			keyptr[keycnt].end = tagsize;
			keyptr[keycnt++].typeflg = SIO_POSITION | SIO_ASCEND;
		}
		reclen += tagsize;
		recpos += tagsize;
	}

	/* prefix keytag size bytes for position and flag and make it's sort order be first */
	/* bytes 1-keytag size is header filepos, byte keytag size+1=1 for head and =2 for group */
	if (gtype == 'G') {
		mscoffton(0, headpos, tagsize);
		headpos[tagsize] = ' ';
		for (i1 = 0; i1 < keycnt; i1++) {
			keyptr[i1].start += tagsize + 1;
			keyptr[i1].end += tagsize + 1;
		}
		memmove((UCHAR *) &keyptr[1], (UCHAR *) keyptr, keycnt * sizeof(SIOKEY));
		keycnt++;
		keyptr[0].start = 0;
		keyptr[0].end = tagsize + 1;
		keyptr[0].typeflg = SIO_POSITION | SIO_ASCEND;
		reclen += tagsize + 1;
		recpos += tagsize + 1;
		recsqpos += tagsize + 1;
	}
	else if (gtype == 'H') {
		if (keyflg || fposflg) firstrec = -1;
		else firstrec = 0;
	}

	/* prefix 2 byte record length */
	if (!quikflg) {
		for (i1 = 0; i1 < keycnt; i1++) {
			keyptr[i1].start += sizeof(USHORT);
			keyptr[i1].end += sizeof(USHORT);
		}
		reclen += sizeof(USHORT);
		recpos += sizeof(USHORT);
		recsqpos += sizeof(USHORT);
		recgpos += sizeof(USHORT);
	}

	/* if workname specified, determine if directory or file */
	workdir = workfile = NULL;
	if (workname[0]) {
#if OS_WIN32
		DWORD attrib;
		attrib = GetFileAttributes(workname);
		if (attrib != 0xFFFFFFFF && (attrib & FILE_ATTRIBUTE_DIRECTORY)) workdir = workname;
		else workfile = workname;
#endif
#if OS_UNIX
		struct stat statbuf;
		if (stat(workname, &statbuf) != -1 && S_ISDIR(statbuf.st_mode)) workdir = workname;
		else workfile = workname;
#endif
	}

	/* sort */
	reccnt = 0L;
	memfree(pptr);  /* free memory for fioopen of work file */

	i1 = SIOFLAGS_DSPERROR;
	if (dspflags & DSPFLAGS_VERBOSE) i1 |= SIOFLAGS_DSPPHASE;
	if (dspflags & DSPFLAGS_DSPXTRA) i1 |= SIOFLAGS_DSPEXTRA;
	if (siosort(reclen, keycnt, keyptrptr, sortin, sortout, workdir, workfile, i1, memsize, dspstring) < 0) {
		cfgexit();
		exit(1);
	}
	if (outhndl == -1) {  /* sortout never called */
		if (!quikflg || keyflg || fposflg) {
			i1 = rioclose(inhndl);
			if (i1) death(DEATH_CLOSE, i1, NULL);
		}
		/* create the output file */
		outhndl = rioopen(outname, createflg, 0, 0);
		if (outhndl < 0) death(DEATH_CREATE, outhndl, outname);
	}
	i1 = rioclose(outhndl);
	if (i1) death(DEATH_CLOSE, i1, NULL);
	if (quikflg && !keyflg && !fposflg) {
		i1 = rioclose(inhndl);
		if (i1) death(DEATH_CLOSE, i1, NULL);
	}

	if (dspflags & DSPFLAGS_VERBOSE) {
		mscofftoa(reccnt, (CHAR *) record);
		dspstring("Sort complete, ");
		dspstring((CHAR *) record);
		dspstring(" records sorted\n");
	}
	cfgexit();
	exit(0);
	return(0);
}

/* SORTIN */
/* provide a record to sort */
static INT sortin(UCHAR *rec) // @suppress("No return")
{
	INT i1, i2, i3, recsiz, selflg, selcmp, selpos, sellen, seleqlflg;
	OFFSET pos, eofpos;
	UCHAR c1, *ptr;

	for ( ; ; ) {
		ptr = &rec[recpos];
		if (stable || quikflg) {
			if (quikflg) ptr = record;
			recsiz = rioget(inhndl, ptr, maxlen);
			riolastpos(inhndl, &pos);
			mscoffton(pos, &rec[recsqpos], tagsize);
		}
		else recsiz = rioget(inhndl, ptr, maxlen);
		if (recsiz < highkey) {
			if (recsiz == -1) {  /* end of file */
				if (eofflg) {
					riolastpos(inhndl, &pos);
					rioeofpos(inhndl, &eofpos);
					if (pos != eofpos) {
						siokill();
						death(DEATH_BADEOF, 0, NULL);
					}
				}
				return(1);
			}
			if (recsiz == -2) continue;  /* deleted record */
			if (recsiz <= -3) {
				siokill();
				death(DEATH_READ, recsiz, NULL);
			}
			memset(&ptr[recsiz], ' ', highkey - recsiz);
		}
		if (selcnt) {
			selchr = *selchrptr;
			selptr = (struct seldef *) *selptrptr;
			selflg = 1;
			for (i3 = 0; i3 < selcnt; i3++) {
				if (selptr[i3].eqlflg & OR) {
					if (selflg) break;
					selflg = 1;
				}
				selpos = selptr[i3].pos;
				seleqlflg = selptr[i3].eqlflg;
				i2 = selptr[i3].ptr;
				if (seleqlflg & STRING) sellen = (INT)strlen((CHAR *) &selchr[i2]);
				else sellen = 1;
				if (selpos + sellen > recsiz) selflg = 0;
				if (!selflg) continue;
				if (sellen > 1) selcmp = memcmp(&ptr[selpos], &selchr[i2], sellen);
				else {
					c1 = ptr[selpos];
					if (seleqlflg & (GREATER | LESS)) selcmp = (INT) c1 - (INT) selchr[i2];
					else {
						for ( ; selchr[i2] && c1 != selchr[i2]; i2++);
						selcmp = !selchr[i2];
					}
				}
				/* used the 'else' for readability */
				if (((seleqlflg & EQUAL) && !selcmp) ||
				    ((seleqlflg & NOTEQUAL) && selcmp) ||
				    ((seleqlflg & GREATER) && selcmp > 0) ||
				    ((seleqlflg & LESS) && selcmp < 0)) /* do nothing here */ ; // @suppress("Suspicious semicolon")
				else selflg = 0;
			}
			if (xselflg) selflg = !selflg;
			if (!selflg) continue;
		}
		if (gtype == 'G') {
			c1 = ptr[gpos];
			if ((geqlflg && (gpos >= recsiz || c1 != gchr))
			   || (!geqlflg && gpos < recsiz && c1 == gchr)) {
				riolastpos(inhndl, &pos);
				mscoffton(pos, headpos, tagsize);
				headpos[tagsize] = '1';
			}
			else headpos[tagsize] = '2';
			memcpy(&rec[recgpos], headpos, tagsize + 1);
		}
		if (quikflg) {
			if (recsiz > maxrec) maxrec = recsiz;
			if (gtype == 'H') {
				if ((geqlflg && (gpos >= recsiz || record[gpos] != gchr))
				   || (!geqlflg && gpos < recsiz && record[gpos] == gchr)) {
					if (!firstrec) firstrec = 1;
					continue;
				}
				if (!firstrec) firstrec = -1;
			}
			qkeyptr = *qkeyptrptr;
			for (i1 = 0, recsiz = recpos; i1 < qkeycnt; i1++, recsiz += i2) {
				i2 = qkeyptr[i1].end;
				memcpy(&rec[recsiz], &record[qkeyptr[i1].start], (UINT) i2);
			}
		}
#if OS_WIN32
		else *(USHORT *) rec = (USHORT) recsiz;
#else
		else {
			rec[0] = (UCHAR)(recsiz >> 8);
			rec[1] = (UCHAR) recsiz;
		}
#endif
		return(0);
	}
}

/* SORTOUT */
/* process an output record from sort */
static INT sortout(UCHAR *rec)
{
	static UCHAR firstflg = TRUE;
	INT i1, i2, i3, recsiz;
	OFFSET pos, eofpos;
	UCHAR *ptr;

	if (firstflg) {  /* create the output file */
		i1 = rioclose(inhndl);
		if (i1) {
			siokill();
			death(DEATH_CLOSE, i1, NULL);
		}
		if (quikflg && !keyflg && !fposflg) {
			maxlen = maxrec;
			inhndl = rioopen(inname, openflg, 0, maxlen);
			if (inhndl < 0) {
				siokill();
				death(DEATH_OPEN, inhndl, inname);
			}
		}

		/* create the output file */
		outhndl = rioopen(outname, createflg, 7, maxlen + tagsize);
		if (outhndl < 0) {
			siokill();
			death(DEATH_CREATE, outhndl, outname);
		}
		if (gtype == 'H' && firstrec == 1) {  /* Primary sort had files start without a header */
			riosetpos(inhndl, 0L);
			for ( ; ; ) {
				recsiz = rioget(inhndl, record, maxlen);
				if (recsiz < 0) {
					if (recsiz == -1) {
						if (eofflg) {
							riolastpos(inhndl, &pos);
							rioeofpos(inhndl, &eofpos);
							if (pos != eofpos) {
								siokill();
								death(DEATH_BADEOF, 0, NULL);
							}
						}
						break;
					}
					if (recsiz == -2) continue;
					siokill();
					death(DEATH_READ, recsiz, NULL);
				}
				if ((geqlflg && (gpos >= recsiz || record[gpos] != gchr))
				   || (!geqlflg && gpos < recsiz && record[gpos] == gchr)) {
					i1 = rioput(outhndl, record, recsiz);
					if (i1) {
						siokill();
						death(DEATH_WRITE, i1, NULL);
					}
					reccnt++;
					if ((dspflags & DSPFLAGS_DSPXTRA) && !(reccnt & 0x03FF)) dspreccnt();
				}
				else break;
			}
		}
		keyptr = *keyptrptr;
		firstflg = FALSE;
	}
	else if (uniqflg) {
		for (i1 = (gtype != 'G') ? 0 : 1, i2 = keycnt - stable; i1 < i2; i1++) {
			i3 = keyptr[i1].start;
			if (compkey(&rec[i3], &record[i3], keyptr[i1].end - i3)) break;
		}
		if (i1 == i2) return 0;
	}
	if (keyflg) {
		ptr = &rec[recsqpos];
		recsiz = reclen - recsqpos;
	}
	else if (fposflg) {
		ptr = &rec[recsqpos];
		recsiz = tagsize;
	}
	else if (quikflg) {
		mscntooff(&rec[recsqpos], &pos, tagsize);
		riosetpos(inhndl, pos);
		recsiz = rioget(inhndl, record, maxlen);
		if (recsiz < 0) {
			if (recsiz == -1) recsiz = ERR_RANGE;
			if (recsiz == -2) recsiz = ERR_ISDEL;
			siokill();
			death(DEATH_READ, recsiz, NULL);
		}
		i1 = rioput(outhndl, record, recsiz);
		if (i1) {
			siokill();
			death(DEATH_WRITE, i1, NULL);
		}
		reccnt++;
		if ((dspflags & DSPFLAGS_DSPXTRA) && !(reccnt & 0x03FF)) dspreccnt();
		while (gtype == 'H') {
			recsiz = rioget(inhndl, record, maxlen);
			if (recsiz < 0) {
				if (recsiz == -1) return 0;
				if (recsiz == -2) continue;
				siokill();
				death(DEATH_READ, recsiz, NULL);
			}
			if ((geqlflg && (gpos >= recsiz || record[gpos] != gchr))
			   || (!geqlflg && gpos < recsiz && record[gpos] == gchr)) {
				i1 = rioput(outhndl, record, recsiz);
				if (i1) {
					siokill();
					death(DEATH_WRITE, i1, NULL);
				}
				reccnt++;
				if ((dspflags & DSPFLAGS_DSPXTRA) && !(reccnt & 0x03FF)) dspreccnt();
			}
			else break;
		}
		return 0;
	}
	else {
		ptr = &rec[recpos];
#if OS_WIN32
		recsiz = *(USHORT *) rec;
#else
		recsiz = ((INT) rec[0] << 8) + rec[1];
#endif
	}
	i1 = rioput(outhndl, ptr, recsiz);
	if (i1) {
		siokill();
		death(DEATH_WRITE, i1, NULL);
	}
	if (uniqflg) memcpy(record, rec, (UINT) reclen);
	reccnt++;
	if ((dspflags & DSPFLAGS_DSPXTRA) && !(reccnt & 0x03FF)) dspreccnt();
	return 0;
}

static INT compkey(UCHAR *key1, UCHAR *key2, INT size)
{
	INT i1;

	if (!collateflg) for (i1 = 0; i1 < size && key1[i1] == key2[i1]; i1++);
	else for (i1 = 0; i1 < size; i1++) {
		if (key1[i1] == key2[i1]) continue;
		if (priority[key1[i1]] != priority[key2[i1]]) break;
	}
	return(i1 != size);
}

/* DSPRECCNT */
static void dspreccnt()
{
	CHAR work[17];

	mscofftoa(reccnt, work);
	dspchar('\r');
	dspstring(work);
	dspstring(" records sorted");
	dspflush();
}

/* USAGE */
static void usage()
{
	dspstring("SORT command  " RELEASEPROGRAM RELEASE COPYRIGHT);
	dspchar('\n');
	dspstring("Usage:  sort file1 file2 sort-spec[A][D][N] [sort-spec[A][D][N]...] [-A=n] [-C]\n");
	dspstring("             [-CFG=cfgfile] [-D] [-F] [-Gn=c] [-Gn#c] [-Hn=c] [-Hn#c] [-J[R]]\n");
	dspstring("             [-K] [-M=n] [-O=optfile] [OR] [-Pn[-n]EQc[c...]]\n");
	dspstring("             [-Pn[-n]NEc[c...]] [-Pn[-n]GTc[c...]] [-Pn[-n]GEc[c...]]\n");
	dspstring("             [-Pn[-n]LTc[c...]] [-Pn[-n]LEc[c...]] [-Q] [-R] [-S] [-T[=type]]\n");
	dspstring("             [-U] [-V] [-W=workfile] [-X] [-Y] [-!]\n");
	exit(1);
}

/* DEATH */
static void death(INT n, INT e, CHAR *s)
{
	CHAR work[17];

	dspchar('\r');
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
	siokill();
	dspchar('\n');
	dspstring(errormsg[DEATH_INTERRUPT]);
	dspchar('\n');
	cfgexit();
	exit(1);
}
