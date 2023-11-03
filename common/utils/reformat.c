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

#define _DEBUG_REFORMAT 0

#define SELMAX   200
#define SELSIZE 2048
/**
 * Bumped from 800 to 1800 for 17.1
 */
#define FLDMAX  1800
#define FLDSIZE 4096
#define QSORT_THRESHOLD 10

#define EQUAL    0x0001
#define NOTEQUAL 0x0002
#define GREATER  0x0004
#define LESS     0x0008
#define STRING   0x0010
#define OR       0x0020

#define MAXTRANS 256	/* maximum size for translate map (-M option) */

#define DSPFLAGS_VERBOSE	0x01
#define DSPFLAGS_DSPXTRA	0x02

#define FIELD_SPEC		1
#define FIELD_STRING	2
#define FIELD_BLANK		3
#define FIELD_CONDITION	4
#define FIELD_DATE		5
#define FIELD_DATEALT1	6
#define FIELD_DATEREV	7
#define FIELD_DATEDUP	8
#define FIELD_REPLACE	9
#define FIELD_REPBLANK	10
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
#define DEATH_TOOMANYFIELD	10
#define DEATH_TOOMANYTAB	11
#define DEATH_TOOMANYSEL	12
#define DEATH_SPEED			13
#define DEATH_TABPOS		14
#define DEATH_TABINSERT		15
#define DEATH_BADTRANSLATE	16
#define DEATH_MUTX_JY		17
#define DEATH_BADEOF		18
#define DEATH_NOTSAMETYPE	19
#define DEATH_FRFILE_EMPTY  20
#define DEATH_FRFILE_BAD    21

struct seldef {
	INT pos;
	INT ptr;
	INT eqlflg;
};

struct field {
	INT type;
	INT pos;
	INT len;
	INT ptr;
	INT eqlflg;
	INT hndl;		 /* file handle, applies only to -fr and -frb optionns */
	UCHAR* tranrecs; /* translation records, applies only to -fr and -frb */
	INT trecct;		 /* translation record count */
	INT treclen;	 /* translation record length */
};

/* local declarations */
static INT dspflags;
static UCHAR inbuf[RIO_MAX_RECSIZE + 4], outbuf[RIO_MAX_RECSIZE + 4];
static UCHAR transmap[MAXTRANS];	/* input translate map record (-M option) */
static UCHAR **fldptrptr, **fldlitptr;
static size_t glength;// @suppress("Type cannot be resolved")
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
	"Too many field specifications",
	"Too many tab positions specified",
	"Too many selection parameters",
	"-R parameter is mutually exclusive with all other parameters",
	"-X parameter must be specified when using -I parameter",
	"-I parameter is mutually exclusive with DB/C type files or field parameters",
	"Bad translate file",
	"-J option is mutually exclusive with the -Y option",
	"Text file contains EOF character before physical EOF",
	"-A with -R option require input and output files to be the same type",
	"field replacement file empty",
	"field replacement file has bad format"
};


/* local routine declarations */
static void usage(void);
static void death(INT, INT, CHAR *);
static void quitsig(INT);
static void loadReplacementFile(INT fldnum);
static INT findTransMatch(INT fldnum, CHAR *repstring);
static int frcompare(const void *element1, const void *element2);


INT main(INT argc, CHAR *argv[])
{
	INT i1, i2, i3, i4, i5, inhndl, outhndl;
	INT createflg1, createflg2, openflg, nonselhndl;
	INT len, recsiz, fldcnt, fldhi, selhi, selcnt, xpdcnt, xpdtab[30];
	INT selflg, selcmp, selpos, sellen, seleqlflg;
	INT incnt, outcnt, insize, inmax, outmax, tagsize;
	INT datecutoff, datetype;
	OFFSET inpos, outpos, pos1, pos2, reccnt;
	CHAR cfgname[MAX_NAMESIZE], inname[MAX_NAMESIZE], outname[MAX_NAMESIZE];
	CHAR nonselname[MAX_NAMESIZE], transname[MAX_NAMESIZE], work[300], *ptr;
	UCHAR appflg, delchr, eofchr, eofflg, insflg;
	UCHAR keyflg, nonselflg, orflg, spdflg, transflg, xselflg;
	UCHAR c1, c2, *outbufptr;
	UCHAR *fldlit, *selchr, **selchrptr, **selptrptr;
	struct field *fldptr;
	struct seldef *selptr;
	FIOPARMS parms;

	arginit(argc, argv, &i1);
	if (!i1) dspsilent();
	signal(SIGINT, quitsig);

	/* initialize */
	if (meminit(144 << 10, 0, 32) == -1) death(DEATH_INIT, ERR_NOMEM, NULL);
	cfgname[0] = '\0';
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

	/* allocate memory for structures */
	fldlitptr = memalloc(FLDSIZE * sizeof(UCHAR), 0);
	fldptrptr = memalloc(FLDMAX * sizeof(struct field), 0);
	selchrptr = memalloc(SELSIZE * sizeof(UCHAR), 0);
	selptrptr = memalloc(SELMAX * sizeof(struct seldef), 0);
	if (fldlitptr == NULL || fldptrptr == NULL || selchrptr == NULL || selptrptr == NULL) death(DEATH_INIT, ERR_NOMEM, NULL);

	ptr = work;
	fldlit = *fldlitptr;
	fldptr = (struct field *) *fldptrptr;
	selchr = *selchrptr;
	selptr = (struct seldef *) *selptrptr;
	fldcnt = fldhi = selcnt = selhi = 0;
	len = xpdcnt = 0;
	reccnt = 0L;
	appflg = eofflg = keyflg = insflg = spdflg = FALSE;
	nonselflg = orflg = transflg = xselflg = FALSE;
	datecutoff = 20;
	openflg = RIO_M_ERO | RIO_P_TXT | RIO_T_ANY;
	createflg1 = RIO_M_PRP | RIO_P_TXT | RIO_T_STD | RIO_UNC;
	createflg2 = RIO_M_EXC | RIO_P_TXT | RIO_T_ANY | RIO_UNC;

	/* scan input and output file names */
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
			switch (toupper(ptr[1])) {
				case '!':
					dspflags |= DSPFLAGS_VERBOSE | DSPFLAGS_DSPXTRA;
					break;
				case 'A':
					appflg = TRUE;
					break;
				case 'B':
					if (ptr[2] != '=' || !ptr[3]) death(DEATH_INVPARMVAL, 0, ptr);
					i1 = atoi(&ptr[3]);
					if (i1 < 1) death(DEATH_INVPARMVAL, 0, ptr);
					if (fldcnt == FLDMAX) death(DEATH_TOOMANYFIELD, 0, NULL);
					fldptr[fldcnt].len = i1;
					fldptr[fldcnt++].type = FIELD_BLANK;
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
				case 'F':
					if (fldcnt == FLDMAX) death(DEATH_TOOMANYFIELD, 0, NULL);
					c1 = (UCHAR) toupper(ptr[2]);
					if (c1 == 'R') {
						if (toupper(ptr[3]) == 'B') {
							fldptr[fldcnt].type = FIELD_REPBLANK;
							for (i1 = 4, i2 = 0; isdigit(ptr[i1]); ) i2 = i2 * 10 + ptr[i1++] - '0';
						}
						else {
							fldptr[fldcnt].type = FIELD_REPLACE;
							for (i1 = 3, i2 = 0; isdigit(ptr[i1]); ) i2 = i2 * 10 + ptr[i1++] - '0';
						}
						if (ptr[i1] == '-') {
							for (i1++, i4 = 0; isdigit(ptr[i1]); ) i4 = i4 * 10 + ptr[i1++] - '0';
						}
						else i4 = i2;
						if (!i2 || i4 < i2 || i4 > RIO_MAX_RECSIZE) death(DEATH_INVPARMVAL, 0, ptr);
						fldptr[fldcnt].pos = i2 - 1;
						fldptr[fldcnt].len = i4 - i2 + 1;
						if (ptr[i1] != '=' || !ptr[i1 + 1]) death(DEATH_INVPARMVAL, 0, ptr);
						i2 = (INT)strlen(&ptr[++i1]); /* i1 points at 1st char of filename */
					}
					else if (c1 == 'P') {
						for (i1 = 3, i2 = 0; isdigit(ptr[i1]); ) i2 = i2 * 10 + ptr[i1++] - '0';
						if (ptr[i1] == '-') {
							for (i1++, i4 = 0; isdigit(ptr[i1]); ) i4 = i4 * 10 + ptr[i1++] - '0';
						}
						else i4 = i2;
						if (!i2 || i4 < i2 || i4 > RIO_MAX_RECSIZE) death(DEATH_INVPARMVAL, 0, ptr);
						fldptr[fldcnt].pos = i2 - 1;
						i4 -= i2 - 1;
						fldptr[fldcnt].len = i4;
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
						i2 = (INT)strlen(&ptr[++i1]);
						if (!i3 || i2 <= i4) death(DEATH_INVPARMVAL, 0, ptr);
						fldptr[fldcnt].eqlflg = i3;
						fldptr[fldcnt].type = FIELD_CONDITION;
					}
					else {
						if (c1 != '=' || !ptr[3]) death(DEATH_INVPARMVAL, 0, ptr);
						i1 = 3;
						i2 = (INT)strlen(&ptr[3]);
						fldptr[fldcnt].len = i2;
						fldptr[fldcnt].type = FIELD_STRING;
					}
					if (fldhi + i2 + 1> FLDSIZE) death(DEATH_TOOMANYFIELD, 0, NULL);
					memcpy(&fldlit[fldhi], &ptr[i1], i2 + 1);
					fldptr[fldcnt++].ptr = fldhi;
					fldhi += i2 + 1;
					break;
				case 'I':
					createflg1 = RIO_M_PRP | RIO_P_TXT | RIO_T_TXT | RIO_UNC;
					createflg2 = RIO_M_EXC | RIO_P_TXT | RIO_T_TXT | RIO_UNC;
					insflg = TRUE;
					break;
				case 'J':
					if (toupper(ptr[2]) == 'R') openflg = RIO_M_SRO | RIO_P_TXT | RIO_T_ANY;
					else openflg = RIO_M_SHR | RIO_P_TXT | RIO_T_ANY;
					break;
				case 'K':
					keyflg = TRUE;
					break;
				case 'L':
					if (ptr[2] != '=') death(DEATH_INVPARMVAL, 0, ptr);
					len = atoi(&ptr[3]);
					if (len < 1 || len > RIO_MAX_RECSIZE) death(DEATH_INVPARMVAL, 0, ptr);
					break;
				case 'N':
					transflg = TRUE;
					if (ptr[2] != '=' || !ptr[3]) death(DEATH_INVPARMVAL, 0, ptr);
					strncpy(transname, &ptr[3], sizeof(transname) - 1);
					transname[sizeof(transname) - 1] = 0;
					break;
				case 'P':
					if (selcnt == SELMAX) death(DEATH_TOOMANYSEL, 0, NULL);
					for (i1 = 2, i2 = 0; isdigit(ptr[i1]); ) i2 = i2 * 10 + ptr[i1++] - '0';
					i4 = i2;
					if (ptr[i1] == '-') {
						for (i1++, i2 = 0; isdigit(ptr[i1]); ) i2 = i2 * 10 + ptr[i1++] - '0';
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
					memcpy(&selchr[selhi], &ptr[i1], i4);
					selhi += i4;
					if (i2 > i4) {
						memset(&selchr[selhi], ' ', i2 - i4);
						selhi += i2 - i4;
					}
					selchr[selhi++] = 0;
					orflg = FALSE;
					break;
				case 'R':
					spdflg = TRUE;
					break;
				case 'S':
					if (ptr[2] != '=' || !ptr[3]) death(DEATH_INVPARMVAL, 0, ptr);
					strncpy(nonselname, &ptr[3], sizeof(nonselname) - 1);
					nonselname[sizeof(nonselname) - 1] = 0;
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
					if (ptr[2]) {
						if (ptr[2] != '=') death(DEATH_INVPARMVAL, 0, ptr);
						i1 = atoi(&ptr[3]);
						if (i1 < 1 || i1 > RIO_MAX_RECSIZE) death(DEATH_INVPARMVAL, 0, ptr);
						if (i1 > 1) {
							if (xpdcnt == 30) death(DEATH_TOOMANYTAB, 0, ptr);
							for (i2 = xpdcnt++; i2-- > 0; ) {
								if (i1 > xpdtab[i2]) break;
								xpdtab[i2 + 1] = xpdtab[i2];
							}
							xpdtab[i2 + 1] = i1 - 1;
						}
					}
					else xselflg = TRUE;
					break;
				case 'Y':
					eofflg = TRUE;
					break;
				case 'Z':	
					if (ptr[2]) {
						datetype = 0;
						switch(toupper(ptr[2])) {
							case 'C':
								if (ptr[3] != '=') death(DEATH_INVPARMVAL, 0, ptr);
								if (isdigit(ptr[4])) {
									datecutoff = ptr[4] - '0';
									if (isdigit(ptr[5])) datecutoff = datecutoff * 10 + ptr[5] - '0';
									else if (ptr[5]) death(DEATH_INVPARMVAL, 0, ptr);
								}
								else death(DEATH_INVPARMVAL, 0, ptr);
								i1 = 0;
								break;
							case 'D':
								datetype = FIELD_DATEDUP;
								break;
							case 'R':
								datetype = FIELD_DATEREV;
								break;
							case 'X':
								datetype = FIELD_DATEALT1;
								break;
							case '=':
								datetype = FIELD_DATE;
								break;
							default:
								death(DEATH_INVPARMVAL, 0, ptr);
								break;
						}
						if (datetype) {
							if (fldcnt == FLDMAX) death(DEATH_TOOMANYFIELD, 0, NULL);
							if (datetype == FIELD_DATE) i1 = 3;
							else {
								if (ptr[3] != '=') death(DEATH_INVPARMVAL, 0, ptr);
								i1 = 4;
							}
							i1 = atoi(&ptr[i1]) - 1;
							if (i1 < 0 || i1 >= RIO_MAX_RECSIZE) death(DEATH_INVPARMVAL, 0, ptr);
							fldptr[fldcnt].pos = i1;
							fldptr[fldcnt].len = 2;
							fldptr[fldcnt].ptr = datecutoff;
							fldptr[fldcnt++].type = datetype;
						}
					}
					break;
				default:
					death(DEATH_INVPARM, 0, ptr);
			}
		}
		else if (isdigit(ptr[0])) {
			if (fldcnt == FLDMAX) death(DEATH_TOOMANYFIELD, 0, NULL);
			for (i1 = 0, i2 = 0; isdigit(ptr[i1]); )
				i2 = i2 * 10 + ptr[i1++] - '0';
			fldptr[fldcnt].pos = i2 - 1;
			if (ptr[i1] == '-') {
				for (i1++, i2 = 0; isdigit(ptr[i1]); ) i2 = i2 * 10 + ptr[i1++] - '0';
			}
			if (ptr[i1] || fldptr[fldcnt].pos < 0 || i2 <= fldptr[fldcnt].pos || i2 > RIO_MAX_RECSIZE) death(DEATH_INVPARMVAL, 0, ptr);
			fldptr[fldcnt].len = i2 - fldptr[fldcnt].pos;
			fldptr[fldcnt++].type = FIELD_SPEC;
		}
		else if (toupper(ptr[0]) == 'O' && toupper(ptr[1]) == 'R' && !ptr[2]) {
			if (selcnt) orflg = TRUE;
		}
		else death(DEATH_INVPARM, 0, ptr);
	}

	i1 = memchange(fldlitptr, fldhi * sizeof(UCHAR), 0);
	if (!i1) i1 = memchange(fldptrptr, fldcnt * sizeof(struct field), 0);
	if (!i1) i1 = memchange(selchrptr, selhi * sizeof(UCHAR), 0);
	if (!i1) i1 = memchange(selptrptr, selcnt * sizeof(struct seldef), 0);
	if (i1) death(DEATH_INIT, ERR_NOMEM, NULL);

	/* check for speed flag and any other flag */
	if (spdflg && (fldcnt || keyflg || xpdcnt || selcnt || createflg1 != (RIO_M_PRP | RIO_P_TXT | RIO_T_STD | RIO_UNC))) death(DEATH_SPEED, 0, NULL);

	/* check for insflg and tab specification */
	if (insflg && !xpdcnt) death(DEATH_TABPOS, 0, NULL);

	/* check for tab insert and compression or field specifications */
	if (insflg && ((createflg1 & RIO_T_MASK) == RIO_T_STD || fldcnt || keyflg)) death(DEATH_TABINSERT, 0, NULL);

	/* check for shared and eof verification */
	if (eofflg && (openflg & FIO_M_MASK) != RIO_M_ERO) death(DEATH_MUTX_JY, 0, NULL);

	/* open the input file */
	if (spdflg) i1 = i2 = 0;
	else {
		i1 = 7;
		i2 = RIO_MAX_RECSIZE;
	}
	inhndl = rioopen(inname, openflg, i1, i2);
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
	if (spdflg && !appflg) {
		miofixname(outname, ".txt", FIXNAME_EXT_ADD);
		createflg1 = FIO_M_PRP | FIO_P_TXT;
		outhndl = fioopen(outname, createflg1);
		if (outhndl < 0) death(DEATH_CREATE, outhndl, outname);
		outpos = 0L;
	}
	else {
reformat1:
		if (!appflg || (outhndl = rioopen(outname, createflg2, i1, RIO_MAX_RECSIZE)) == ERR_FNOTF) {
			outhndl = rioopen(outname, createflg1, i1, RIO_MAX_RECSIZE);
		}
		if (outhndl < 0) death(DEATH_CREATE, outhndl, outname);
		if (appflg) {
			if (riotype(outhndl) == RIO_T_ANY) {
				for ( ; ; ) {
					recsiz = rioget(outhndl, inbuf, RIO_MAX_RECSIZE);
					if (recsiz >= -1) break;
					if (recsiz == -2) continue;
					death(DEATH_READ, recsiz, NULL);
				}
				if (riotype(outhndl) == RIO_T_ANY) {
					rioclose(outhndl);
					appflg = FALSE;
					createflg1 = RIO_M_PRP | RIO_P_TXT | RIO_T_TXT | RIO_UNC;
					goto reformat1;
				}
			}
			rioeofpos(outhndl, &outpos);
			if (!spdflg) riosetpos(outhndl, outpos);
			else if (riotype(inhndl) != riotype(outhndl)) death(DEATH_NOTSAMETYPE, 0, NULL);
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
	}

	/* other initialization, depending on input file type */
	if (riotype(inhndl) == RIO_T_STD) {  /* set inflag type and cancel tab expand */
		delchr = DBCDEL;
		eofchr = DBCEOF;
		if (!insflg) xpdcnt = 0;
	}
	else {  /* local text operating system */
		delchr = 0x7F;
		if (eofflg && riotype(inhndl) == RIO_T_DOS) eofchr = 0x1A;
		else eofflg = FALSE;
	}

	if (dspflags & DSPFLAGS_VERBOSE) dspstring("Copying with reformat in progress\n");

	if (spdflg) {   /* speed option was specified */
		/* get each block and move characters from input block to output block */
		inmax = (sizeof(inbuf) >= (size_t)(32 << 10)) ? (size_t)(32 << 10) : (sizeof(inbuf) & ~0x0FFF); // @suppress("Symbol is not resolved")
		outmax = (sizeof(outbuf) >= (size_t)(32 << 10)) ? (size_t)(32 << 10) : (sizeof(outbuf) & ~0x0FFF); // @suppress("Symbol is not resolved")
		incnt = insize = 0;
		outcnt = 0;
		inpos = 0L;  /* outpos set above */
		for ( ; ; ) {
			if (incnt == insize) {
				insize = fioread(inhndl, inpos, inbuf, inmax);
				if (insize <= 0) {
					if (!insize) break;
					death(DEATH_READ, insize, NULL);
				}
				inpos += insize;
				incnt = 0;
			}
			if (outcnt == outmax) {
				i1 = fiowrite(outhndl, outpos, outbuf, outcnt);
				if (i1) death(DEATH_WRITE, i1, NULL);
				outpos += outcnt;
				outcnt = 0;
			}
			i1 = insize;
			if (i1 > outmax - outcnt + incnt) i1 = outmax - outcnt + incnt;
			if (!eofflg) {
				while (incnt < i1) {
					if (inbuf[incnt] != delchr) outbuf[outcnt++] = inbuf[incnt];
					incnt++;
				}
			}
			else {
				while (incnt < i1) {
					if (inbuf[incnt] == eofchr) {
						 fiogetsize(inhndl, &pos1);
						 if (inpos - insize + incnt + 1 != pos1) death(DEATH_BADEOF, 0, NULL);
					}
					if (inbuf[incnt] != delchr) outbuf[outcnt++] = inbuf[incnt];
					incnt++;
				}
			}
		}
		if (outcnt) {
			i1 = fiowrite(outhndl, outpos, outbuf, outcnt);
			if (i1) death(DEATH_WRITE, i1, NULL);
		}

		i1 = fioclose(outhndl);
		if (i1) death(DEATH_CLOSE, i1, NULL);
		i1 = rioclose(inhndl);
		if (i1) death(DEATH_CLOSE, i1, NULL);
		if (dspflags & DSPFLAGS_VERBOSE) dspstring("Reformat complete\n");
		cfgexit();
		exit(0);
	}

	/* restore the memory pointers */
	fldlit = *fldlitptr;
	fldptr = (struct field *) *fldptrptr;
	selchr = *selchrptr;
	selptr = (struct seldef *) *selptrptr;

	if (keyflg || fldcnt || xpdcnt) {
		if (keyflg) {  /* get key tag size */
			tagsize = 12;
			if (!prpget("keytag", NULL, NULL, NULL, &ptr, PRP_LOWER) && !strcmp("old", ptr)) tagsize = 8;
		}
		outbufptr = outbuf;
	}
	else outbufptr = inbuf;

	/* process the translate file for any -fr or -frb options */
	if (fldcnt) {
		for (i2 = 0; i2 < fldcnt; i2++) {
			if (fldptr[i2].type == FIELD_REPLACE || fldptr[i2].type == FIELD_REPBLANK) {
				fldptr[i2].hndl = rioopen((CHAR *)&fldlit[fldptr[i2].ptr], 
					RIO_M_SRO 		/* shared read only */
					| RIO_P_TXT		/* search open path */
					| RIO_T_ANY,	/* allow std, data, or text file type */
					7, RIO_MAX_RECSIZE);
				if (fldptr[i2].hndl < 0) death(DEATH_OPEN, fldptr[i2].hndl, (CHAR *)&fldlit[fldptr[i2].ptr]);
				loadReplacementFile(i2);
			}
		}
	}

	/* process the records */
	for ( ; ; ) {
		recsiz = rioget(inhndl, inbuf, RIO_MAX_RECSIZE);
		if (recsiz < 0) {
			if (recsiz == -1) {  /* end of file */
				if (eofflg) {
					riolastpos(inhndl, &pos1);
					rioeofpos(inhndl, &pos2);
					if (pos1 != pos2) death(DEATH_BADEOF, 0, NULL);
				}
				break;
			}
			if (recsiz == -2) continue;
			death(DEATH_READ, recsiz, NULL);
		}
		if (transflg) for (i1 = 0; i1 < recsiz; i1++) inbuf[i1] = transmap[inbuf[i1]];

		selflg = 1;
		if (selcnt) {
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
				if (selpos + sellen > recsiz) selflg = 0;
				if (!selflg) continue;
				if (sellen > 1) selcmp = memcmp(&inbuf[selpos], &selchr[i3], sellen);
				else {
					c1 = inbuf[selpos];
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
				    ((seleqlflg & LESS) && selcmp < 0)) /* do nothing here */ ; // @suppress("Suspicious semicolon")
				else selflg = 0;
			}
			if (xselflg) selflg = !selflg;
			if (!selflg && !nonselflg) continue;
		}

		/* tab expansion or compression */
		if (xpdcnt && recsiz) {
			if (!insflg) {  /* tabs are being expanded */
				for (i1 = 0, i2 = 0, i4 = 0; i2 < recsiz && i4 < RIO_MAX_RECSIZE; ) {
					c1 = inbuf[i2++];
					if (c1 == 0x09) {  /* tab expansion */
						while (i1 < xpdcnt && i4 >= xpdtab[i1]) i1++;
						if (i1 < xpdcnt) {
							i3 = xpdtab[i1++] - i4;
							memset(&outbuf[i4], ' ', i3);
							i4 += i3;
						}
						else outbuf[i4++] = ' ';
					}
					else outbuf[i4++] = c1;
				}
			}
			else {  /* tabs are being inserted */
				for (i1 = 0, i2 = 0, i3 = -1, i4 = 0; i2 < recsiz; i2++) {
					if (i3 != -1 && i2 == xpdtab[i1]) {
						i4 = i3;
						outbuf[i4++] = 0x09;
						i3 = -1;
					}
					c1 = inbuf[i2];
					if (c1 == ' ') {
						if (i3 == -1 && i1 < xpdcnt) {
							while (i1 < xpdcnt && i2 >= xpdtab[i1]) i1++;
							if (i1 < xpdcnt) i3 = i4;
						}
					}
					else i3 = -1;
					outbuf[i4++] = c1;
				}
			}
			recsiz = i4;
		}

		/* altering record format */
		if (keyflg || fldcnt) {
			if (keyflg) {
				riolastpos(inhndl, &pos1);
				mscoffton(pos1, outbuf, tagsize);
				i1 = tagsize;
				if (!fldcnt) {
					memcpy(&outbuf[tagsize], inbuf, recsiz);
					i1 += recsiz;
				}
			}
			else i1 = 0;
			for (i2 = 0; i2 < fldcnt; i2++) {
				i3 = fldptr[i2].len;
				if (fldptr[i2].type == FIELD_SPEC) {
					if (fldptr[i2].pos >= recsiz) continue;
					if (fldptr[i2].pos + i3 > recsiz) i3 = recsiz - fldptr[i2].pos;
					memcpy(&outbuf[i1], &inbuf[fldptr[i2].pos], i3);
				}
				else if (fldptr[i2].type == FIELD_STRING) memcpy(&outbuf[i1], &fldlit[fldptr[i2].ptr], i3);
				else if (fldptr[i2].type == FIELD_BLANK) memset(&outbuf[i1], ' ', i3);
				else if (fldptr[i2].type == FIELD_CONDITION) {
					seleqlflg = fldptr[i2].eqlflg;
					i4 = fldptr[i2].ptr;
					if (fldptr[i2].pos + i3 > recsiz) continue;
					selcmp = memcmp(&inbuf[fldptr[i2].pos], &fldlit[i4], i3);
					/* used the 'else' for readability */
					if (((seleqlflg & EQUAL) && !selcmp) ||
						((seleqlflg & NOTEQUAL) && selcmp) ||
						((seleqlflg & GREATER) && selcmp > 0) ||
						((seleqlflg & LESS) && selcmp < 0)) /* do nothing here */ ; // @suppress("Suspicious semicolon")
					else continue;
					i4 += i3;
					i3 = (INT)strlen((CHAR *) &fldlit[i4]);
					memcpy(&outbuf[i1], &fldlit[i4], i3);
				}
				else if (fldptr[i2].type == FIELD_REPLACE || fldptr[i2].type == FIELD_REPBLANK) {
					i3 = fldptr[i2].treclen - fldptr[i2].len;
					if (!findTransMatch(i2, work))
						memcpy(&outbuf[i1], work, i3);
					else if (fldptr[i2].type == FIELD_REPBLANK)
						memset(&outbuf[i1], ' ', i3);
					else {
						if (i3 <= fldptr[i2].len)
							memcpy(&outbuf[i1], &inbuf[fldptr[i2].pos], i3);
						else {
							memcpy(&outbuf[i1], &inbuf[fldptr[i2].pos], fldptr[i2].len);
							i1 += fldptr[i2].len;
							i3 -= fldptr[i2].len;
							memset(&outbuf[i1], ' ', i3);
						}
					}
				}
				else {  /* DATE type */
					datetype = fldptr[i2].type;
					if (fldptr[i2].pos + i3 > recsiz) {
						if (recsiz > fldptr[i2].pos) i4 = recsiz - fldptr[i2].pos;
						else i4 = 0;
						memset(&inbuf[fldptr[i2].pos + i4], ' ', i3 - i4);
					}
					i4 = fldptr[i2].pos;
					if (isdigit(inbuf[i4 + 1])) {
						i5 = inbuf[i4 + 1] - '0';
						if (isdigit(inbuf[i4])) i5 += (inbuf[i4] - '0') * 10;
						else if (inbuf[i4] != ' ') i5 = -1;
					}
					else i5 = -1;
					memcpy(&outbuf[i1 + 2], &inbuf[i4], i3);
					outbuf[i1] = outbuf[i1 + 2];
					outbuf[i1 + 1] = outbuf[i1 + 3];
					if (i5 == -1) {
						if (datetype != FIELD_DATEDUP) outbuf[i1] = outbuf[i1 + 1] = ' ';
					}
					else if (datetype == FIELD_DATEREV) {
						i5 = 99 - i5;  /* convert reversed year back */
						if (outbuf[i1 + 2] == ' ') outbuf[i1 + 2] = '0';
						if (i5 > fldptr[i2].ptr) {  /* 9999 - (1900 + i5) */
							outbuf[i1] = '8';
							outbuf[i1 + 1] = '0';
						}
						else {  /* 9999 - (2000 + i5) */
							outbuf[i1] = '7';
							outbuf[i1 + 1] = '9';
						}
					}
					else {
						if (datetype == FIELD_DATEALT1 && !i5) {
							if (outbuf[i1] == ' ') outbuf[i1 + 1] = ' ';
							/* else leave zero's alone */
						}
						else {
							if (outbuf[i1 + 2] == ' ') outbuf[i1 + 2] = '0';
							if (i5 > fldptr[i2].ptr) {
								outbuf[i1] = '1';
								outbuf[i1 + 1] = '9';
							}
							else {
								outbuf[i1] = '2';
								outbuf[i1 + 1] = '0';
							}
						}
					}
					i3 += 2;
				}
				i1 += i3;
			}
			recsiz = i1;
		}

		/* modify record length */
		if (len) {
			if (len > recsiz) memset(&outbufptr[recsiz], ' ', len - recsiz);
			recsiz = len;
		}
		if (selflg) i1 = rioput(outhndl, outbufptr, recsiz);
		else i1 = rioput(nonselhndl, outbufptr, recsiz);
		if (i1) death(DEATH_WRITE, i1, NULL);
		if ((dspflags & DSPFLAGS_DSPXTRA) && !(++reccnt & 0x01FF)) {
			mscofftoa(reccnt, (CHAR *) outbuf);
			dspchar('\r');
			dspstring((CHAR *) outbuf);
			dspstring(" records");
			dspflush();
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
		mscofftoa(reccnt, (CHAR *) outbuf);
		dspstring("\rReformat complete, ");
		dspstring((CHAR *) outbuf);
		dspstring(" records processed\n");
	}
	if (dspflags & DSPFLAGS_VERBOSE) dspstring("Reformat complete\n");
	cfgexit();
	exit(0);
	return(0);
}

/* USAGE */
static void usage()
{
	dspstring("REFORMAT command  " RELEASEPROGRAM RELEASE COPYRIGHT);
	dspchar('\n');
	dspstring("Usage:  reformat file1 file2 [field-spec...] [-A] [-B=n] [-C] [-CFG=cfgfile]\n");
	dspstring("                 [-D] [-F=string] [-I] [-J[R]] [-K] [-L=n] [-N=transfile]\n");
	dspstring("                 [-O=optfile] [OR] [-Pn[-n]EQc[c...]] [-Pn[-n]NEc[c...]]\n");
	dspstring("                 [-Pn[-n]GTc[c...]] [-Pn[-n]GEc[c...]] [-Pn[-n]LTc[c...]]\n");
	dspstring("                 [-Pn[-n]LEc[c...]] [-R] [-S=notselfile] [-V] [-X[=n]]\n");
	dspstring("                 [-FPn[-n]EQc[c...]r[r...]] [-FPn[-n]NEc[c...]r[r...]]\n");
	dspstring("                 [-FPn[-n]GTc[c...]r[r...]] [-FPn[-n]GEc[c...]r[r...]]\n");
	dspstring("                 [-FPn[-n]LTc[c...]r[r...]] [-FPn[-n]LEc[c...]r[r...]]\n");
	dspstring("                 [-FRn[-n]=tranfile] [-FRBn[-n]=tranfile]\n");
	dspstring("                 [-Y] [-!] [-T[=type]]\n");
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

/*
 * For each -fr or -frb we load the entire translate file into memory
 */
static void loadReplacementFile(INT fldnum) {
	UCHAR *records, *ptr, *memend;
	UCHAR *fldlit = *fldlitptr;
	struct field *fldptr = (struct field *) *fldptrptr;
	INT fnum = fldptr[fldnum].hndl;
	UCHAR ** rtabptrptr = fiogetwptr(fnum);
	struct rtab *rtab = (struct rtab *) *rtabptrptr;
	INT i1;
	size_t memsize, reclen, retval; // @suppress("Type cannot be resolved")
	CHAR *filename = (CHAR *)&fldlit[fldptr[fldnum].ptr];

	/* find the first record */
	for (;;) {
		retval = rioget(fnum, inbuf, RIO_MAX_RECSIZE);
		if (retval > 0) break;
		if ((INT)retval == -1) death(DEATH_FRFILE_EMPTY, (INT)retval, filename);
		if ((INT)retval == -2) continue;		/* skip deleted records */
		death(DEATH_FRFILE_BAD, (INT)retval, filename);
	}

	if (retval < (UINT_PTR)(fldptr[fldnum].len + 1)) death(DEATH_FRFILE_BAD, ERR_SHORT, filename);
	reclen = fldptr[fldnum].treclen = (INT)retval;
	memsize = (size_t) rtab->fsiz; // @suppress("Type cannot be resolved")
	records = (UCHAR *) malloc(memsize);
	if (records == NULL) death(DEATH_NOMEM, 1, filename);
	memend = records + memsize;
	
	/*
	 * Read in all records.
	 * Check record lengths for consistency.
	 */
	i1 = 1;
	if (reclen > memsize) {			/* could happen if only one record */
		memsize = (INT) (1.2 * memsize);	/* and it was compressed */
		records = realloc(records, memsize);
		if (records == NULL) death(DEATH_NOMEM, 1, filename);
		memend = records + memsize;
	}
	ptr = records;
	memcpy(ptr, inbuf, reclen);
	ptr += reclen;
	for (;;) {
		retval = rioget(fnum, inbuf, RIO_MAX_RECSIZE);
		if ((INT)retval == -2) continue;		/* skip deleted records */
		if ((INT)retval == -1) break;		/* EOF, we are done here */
		if ((INT)retval < 0) death(DEATH_FRFILE_BAD, (INT)retval, filename);
		if (retval > reclen) death(DEATH_FRFILE_BAD, ERR_NOEOR, filename);
		if (retval < reclen) death(DEATH_FRFILE_BAD, ERR_SHORT, filename);
		if (ptr + reclen  - 1 > memend) { /* could happen if file is compressed */
			memsize = (INT) (1.2 * memsize);
			records = realloc(records, memsize);
			if (records == NULL) death(DEATH_NOMEM, 1, filename);
			memend = records + memsize;
			ptr = records + (i1 * reclen);
		}
		i1++;
		memcpy(ptr, inbuf, reclen);
		ptr += reclen;
	}
	fldptr[fldnum].trecct = i1;
#if defined(_DEBUG_REFORMAT) && _DEBUG_REFORMAT
	printf("record count=%i\n", i1);
#endif
	rioclose(fnum);
	if (fldptr[fldnum].trecct > QSORT_THRESHOLD) {
		glength = fldptr[fldnum].len;
		qsort(records, fldptr[fldnum].trecct, fldptr[fldnum].treclen,
			frcompare);
	}
	fldptr[fldnum].tranrecs = records;
}

static int frcompare(const void *element1, const void *element2) {
	return memcmp(element1, element2, glength);
}

static INT findTransMatch(INT fldnum, CHAR *repstring) {
	struct field *fldptr = (struct field *) *fldptrptr;
	INT reclen = fldptr[fldnum].treclen;
	INT reccount = fldptr[fldnum].trecct;
	UCHAR *ptr, *end;
	UCHAR *matchto = &inbuf[fldptr[fldnum].pos];
	glength = fldptr[fldnum].len;
	if (reccount <= QSORT_THRESHOLD) {
		ptr = fldptr[fldnum].tranrecs;
		end = ptr + (reclen * reccount);
		for (; ptr < end; ptr += reclen) {
			if (!memcmp(matchto, ptr, glength)) {
				memcpy(repstring, ptr + glength, reclen - glength);
				return 0;
			}
		}
	}
	else {
		ptr = bsearch(matchto, fldptr[fldnum].tranrecs, reccount, reclen, frcompare);
		if (ptr == NULL) return 1;
		memcpy(repstring, ptr + glength, reclen - glength);
		return 0;
	}
	return 1;
}
