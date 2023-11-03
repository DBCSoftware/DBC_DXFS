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

#define SELMAX 200
#define SELSIZE 2048

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
#define DEATH_INVPARMVAL	2
#define DEATH_INIT			3
#define DEATH_OPEN			4
#define DEATH_CLOSE			5
#define DEATH_READ			6
#define DEATH_TOOMANYSEL	7
#define DEATH_EXCLUSIVE		8
#define DEATH_FORMAT		9
#define DEATH_POSITION		10
#define DEATH_BADIX			11
#define DEATH_DELREC		12
#define DEATH_BIGAIMKEY		13
#define DEATH_INVALIDKEY	14

struct seldef {
	INT pos;
	INT ptr;
	INT eqlflg;
};

/* local declarations */
static INT dspflags;
static INT handle, xhandle;
static UCHAR record[RIO_MAX_RECSIZE + 32];
static INT fixflg, eorsize, reclen, recoff;
static OFFSET filepos;
static CHAR header[260];

static char *errormsg[] = {
	"HALTED - user interrupt",
	"Invalid parameter ->",
	"Invalid parameter value ->",
	"Unable to initialize",
	"Unable to open",
	"Unable to close file",
	"Unable to read from file",
	"Too many selection parameters",
	"-A, -I, -R and -S parameters are mutually exclusive",
	"-F parameter is mutually exclusive with -B, -H, -N, -P or -T parameters",
	"-E, -R, -S and -I=<key> parameters are mutually exclusive",
	"AIM or ISI file is invalid",
	"Attempted read of deleted record or EOF, index contains invalid key",
	"AIM keys too long for static buffer",
	"Invalid AIM key"
};

/* routine declarations */
static INT aimrec(INT aionextflag);
static void usage(void);
static void death(INT, INT, CHAR *);
static void quitsig(INT);


INT main(INT argc, CHAR *argv[])
{
	INT i1, i2, i3, i4, keytag, numsiz, prtflg, pagflg, isiflg, aimflg, aimnext;
	INT keylen, openflg, recsiz, totlin, headsz, linesz, column;
	INT lincnt, top, bottom, endflg, pagcnt, tabsize;
	INT selcnt, selhi, selflg, selcmp, selpos, sellen, seleqlflg;
	OFFSET cnt, recnum, startpos;
	CHAR cfgname[MAX_NAMESIZE], filename[MAX_NAMESIZE], key[1024], *ptr;
	UCHAR c1, c2, orflg, xselflg;
	UCHAR *selchr, **selchrptr, **selptrptr;
	struct seldef *selptr;
	FIOPARMS parms;

	arginit(argc, argv, &i1);
	if (!i1) dspsilent();
	signal(SIGINT, quitsig);

	/* initialize */
	if (meminit(128 << 10, 0, 128) == -1) death(DEATH_INIT, ERR_NOMEM, NULL);
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

	selchrptr = memalloc(SELSIZE * sizeof(UCHAR), 0);
	selptrptr = memalloc(SELMAX * sizeof(struct seldef), 0);
	if (selchrptr == NULL || selptrptr == NULL) death(DEATH_INIT, ERR_NOMEM, NULL);

	ptr = (CHAR *) record;
	selchr = *selchrptr;
	selptr = (struct seldef *) *selptrptr;
	selcnt = selhi = 0;
	keytag = numsiz = linesz = column = endflg = prtflg = 0;
	startpos = recnum = 0;
	pagflg = isiflg = aimflg = FALSE;
	orflg = xselflg = FALSE;
	key[0] = 0;
	keylen = 0;
	totlin = 57;
	bottom = 3;
	top = 6;
	tabsize = 1;
	openflg = RIO_M_ERO | RIO_P_TXT | RIO_T_ANY;

	/* scan input file name */
	i1 = argget(ARG_FIRST, filename, sizeof(filename));
	if (i1 < 0) death(DEATH_INIT, i1, NULL);
	if (i1 == 1) usage();
	strcpy(header, filename);

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
				case 'A':
					if (ptr[2] != '=') death(DEATH_INVPARMVAL, 0, ptr);
					i1 = (INT)strlen(&ptr[3]);
					if (keylen + i1 >= (INT) sizeof(key)) death(DEATH_BIGAIMKEY, 0, NULL);
					strcpy(key + keylen, &ptr[3]);
					if (i1 < 4 || (key[keylen] != ' ' && !isdigit(key[keylen])) || !isdigit(key[keylen + 1])) death(DEATH_INVPARMVAL, 0, ptr);
					key[keylen + 2] = (CHAR) toupper(key[keylen + 2]);
					if ((key[keylen + 2] != 'R' && key[keylen + 2] != 'L' && key[keylen + 2] != 'X' && key[keylen + 2] != 'F')) death(DEATH_INVPARMVAL, 0, ptr);
					keylen += i1 + 1;
					key[keylen] = '\0';
					aimflg = TRUE;
					break;
				case 'B':
					if (ptr[2] != '=') death(DEATH_INVPARMVAL, 0, ptr);
					bottom = atoi(&ptr[3]);
					pagflg = TRUE;
					break;
				case 'C':
					if (ptr[2] == '=') {
						column = atoi(&ptr[3]);
						if (column < 1 || column > RIO_MAX_RECSIZE) death(DEATH_INVPARMVAL, 0, ptr);
						column--;
					}
					else if (toupper(ptr[2]) != 'F' || toupper(ptr[3]) != 'G' || ptr[4] != '=' || !ptr[5]) death(DEATH_INVPARM, 0, ptr);
					break;
				case 'E':
					if (ptr[2] != '=') death(DEATH_INVPARMVAL, 0, ptr);
					endflg = atoi(&ptr[3]);
					if (endflg <= 0 || endflg > 999) death(DEATH_INVPARMVAL, 0, ptr);
					break;
				case 'F':
					prtflg = 1;
					break;
				case 'H':
					if (ptr[2] != '=') death(DEATH_INVPARMVAL, 0, ptr);
					strcpy(header, &ptr[3]);
					pagflg = TRUE;
					break;
				case 'I':
					if (ptr[2] == '=') strcpy(key, &ptr[3]);
					isiflg = TRUE;
					break;
				case 'J':
					if (toupper(ptr[2]) == 'R') openflg = RIO_M_SRO | RIO_P_TXT | RIO_T_ANY;
					else openflg = RIO_M_SHR | RIO_P_TXT | RIO_T_ANY;
					break;
				case 'K':
					if (ptr[2] == '=') keytag = atoi(&ptr[3]);
					else keytag = 12;
					break;
				case 'L':
					if (ptr[2] != '=') death(DEATH_INVPARMVAL, 0, ptr);
					linesz = atoi(&ptr[3]);
					if (linesz < 1 || linesz > RIO_MAX_RECSIZE + 14) death(DEATH_INVPARMVAL, 0, ptr);
					break;
				case 'N':
					if (ptr[2] == '=') {
						numsiz = atoi(&ptr[3]);
						if (numsiz < 1 || numsiz > 12) death(DEATH_INVPARMVAL, 0, ptr);
						numsiz += 2;
					}
					else numsiz = 8;
					break;
				case 'P':
					if (!ptr[2] || ptr[2] == '=') {
						if (ptr[2] == '=' && ptr[3]) totlin = atoi(&ptr[3]);
						pagflg = TRUE;
						break;
					}
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
					if (ptr[2] != '=' || !ptr[3]) death(DEATH_INVPARMVAL, 0, ptr);
					recnum = atol(&ptr[3]);
					break;
				case 'S':
					if (ptr[2] != '=' || !ptr[3]) death(DEATH_INVPARMVAL, 0, ptr);
					startpos = atol(&ptr[3]);
					break;
				case 'T':
					if (ptr[2] != '=') death(DEATH_INVPARMVAL, 0, ptr);
					top = atoi(&ptr[3]);
					pagflg = TRUE;
					break;
				case 'V':
					dspflags |= DSPFLAGS_VERBOSE;
					break;
				case 'X':
					if (ptr[2]) {
						if (ptr[2] != '=') death(DEATH_INVPARMVAL, 0, ptr);
						tabsize = atoi(&ptr[3]);
						if (tabsize < 1) tabsize = 1;
					}
					else xselflg = TRUE;
					break;
				default:
					death(DEATH_INVPARM, 0, ptr);
			}
		}
		else if (toupper(ptr[0]) == 'O' && toupper(ptr[1]) == 'R' && !ptr[2]) {
			if (selcnt) orflg = TRUE;
		}
		else death(DEATH_INVPARM, 0, ptr);
	}

	i1 = memchange(selchrptr, selhi * sizeof(UCHAR), 0);
	if (!i1) i1 = memchange(selptrptr, selcnt * sizeof(struct seldef), 0);
	if (i1) death(DEATH_INIT, ERR_NOMEM, NULL);

	i1 = 0;
	if (recnum) i1++;
	if (startpos) i1++;
	if (endflg && (i1 || (isiflg && key[0]))) death(DEATH_POSITION, 0, NULL);
	if (aimflg) i1++;
	if (isiflg) i1++;
	if (i1 > 1) death(DEATH_EXCLUSIVE, 0, NULL);
	if (prtflg && (keytag || numsiz || pagflg)) death(DEATH_FORMAT, 0, NULL);
	if (!linesz) {
		if (prtflg) linesz = 256;
		else linesz = 79;
	}
	if (linesz < 256) headsz = linesz;
	else headsz = 256;

	/* open the input file */
	reclen = RIO_MAX_RECSIZE;
	if (isiflg || aimflg) {
		/* must figure out if it is fixed length */
		if (isiflg) ptr = ".isi";
		else ptr = ".aim";
		miofixname(filename, ptr, FIXNAME_EXT_ADD);
		xhandle = fioopen(filename, openflg);
		if (xhandle < 0) death(DEATH_OPEN, xhandle, filename);
		i1 = fioread(xhandle, 0L, record, 512);
		if (i1 < 0) death(DEATH_READ, i1, NULL);
		if (i1 != 512) death(DEATH_BADIX, 0, NULL);
		fioclose(xhandle);
		fixflg = TRUE;
		if (isiflg && (record[57] == 'F' || record[57] == 'S')) mscntoi(&record[64], &reclen, 5);
		else if (aimflg && (record[59] == 'F' || record[59] == 'S')) mscntoi(&record[41], &reclen, 5);
		else fixflg = FALSE;
		filepos = 0;

		/* open the isi or aim file */
		if (isiflg) {
			if (fixflg) openflg |= XIO_FIX | XIO_UNC;
			if (record[99] <= '7' || record[56] == 'D') openflg |= XIO_DUP;
			i1 = record[59] - '0';
			if (record[58] != ' ') {
				if (record[58] >= 'A' && record[58] <= 'Z') i1 += (record[58] - 'A') * 10 + 100;
				else i1 += (record[58] - '0') * 10;
			}
			xhandle = xioopen(filename, openflg, 0, reclen, &filepos, (CHAR *) record);
			if (xhandle < 0) death(DEATH_OPEN, xhandle, filename);
			recsiz = 2;
			key[i1] = '\0';  /* truncate long user keys */
			i1 = (INT)strlen(key);
			if (endflg) {
				/* currently only support zero length keys, */
				/* there is no code to stop reading to end of file */
				recsiz = xiofindlast(xhandle, (UCHAR *) key, i1);
				if (recsiz < 0) {
					if (recsiz < -2) death(DEATH_READ, recsiz, NULL);
					death(DEATH_DELREC, 0, NULL);
				}
			}
			else if (i1) {
				recsiz = xiofind(xhandle, (UCHAR *) key, i1);
				if (recsiz < 0) {
					if (recsiz < -2) death(DEATH_READ, recsiz, NULL);
					death(DEATH_DELREC, 0, NULL);
				}
			}
		}
		else {
			if (fixflg) openflg |= AIO_FIX | AIO_UNC;
			xhandle = aioopen(filename, openflg, reclen, &filepos, (CHAR *) record, &i1, NULL, 0, 0);
			if (xhandle < 0) death(DEATH_OPEN, xhandle, filename);
			aionew(xhandle);
			for (ptr = key; *ptr; ptr += i1 + 1) {
				i1 = (INT)strlen(ptr);
				i2 = aiokey(xhandle, (UCHAR *) ptr, i1);
				if (i2) death(DEATH_INVALIDKEY, 0, ptr);
			}
			if (endflg) aimnext = AIONEXT_LAST;
			else aimnext = AIONEXT_NEXT;
		}
		strcpy(filename, (CHAR *) record);
		i1 = 0;
	}
	else i1 = 6;
	handle = rioopen(filename, openflg, i1, reclen);
	if (handle < 0) death(DEATH_OPEN, handle, filename);

	if (aimflg && fixflg) {
		eorsize = 1;
		if (riotype(handle) == RIO_T_ANY) {
			while (TRUE) {
				recsiz = rioget(handle, record, reclen);
				if (recsiz >= -1) break;
				if (recsiz == -2) continue;
				death(DEATH_READ, recsiz, NULL);
			}
#if OS_WIN32
			if (riotype(handle) == RIO_T_ANY) eorsize = 2;
#endif
		}
		if (riotype(handle) == RIO_T_DOS) eorsize = 2;
	}
	if (!isiflg && !aimflg) {
		if (endflg) {
			rioeofpos(handle, &startpos);
			riosetpos(handle, startpos);
		}
		else if (startpos) riosetpos(handle, startpos);
	}

	/* other initialization */
	lincnt = pagcnt = 0;
	cnt = 0;
	recoff = numsiz + keytag;
	if (pagflg) {
		i1 = (INT)strlen(header);
		if (i1 < headsz) memset(&header[i1], ' ', headsz - i1);
		header[headsz] = 0;
	}

	/* get each record */
	if (isiflg && recsiz == 1) goto listisi;
	for ( ; ; ) {
		if (isiflg) {
			if (endflg) {
				i1 = xioprev(xhandle);
				if (i1 == 1) goto listisi;
				if (i1 < 0) death(DEATH_READ, i4, NULL);
				endflg = 0;
			}
			i1 = xionext(xhandle);
			if (i1 != 1) {
				if (i1 < 0) death(DEATH_READ, i4, NULL);
				break;
			}
listisi:
			riosetpos(handle, filepos);
			recsiz = rioget(handle, record + recoff, reclen);
			if (recsiz < 0) {
				if (recsiz < -2) death(DEATH_READ, recsiz, NULL);
				death(DEATH_DELREC, 0, NULL);
			}
		}
		else if (aimflg) {
			recsiz = aimrec(aimnext);
			if (endflg) aimnext = AIONEXT_PREV;
			if (recsiz < 0) {
				if (recsiz != -1) death(DEATH_READ, recsiz, NULL);
				if (!endflg) break;
				endflg = 0;
				aimnext = AIONEXT_NEXT;
				continue;
			}
		}
		else {
			if (endflg) while ((recsiz = rioprev(handle, record + recoff, reclen)) == -2);
			else while ((recsiz = rioget(handle, record + recoff, reclen)) == -2);
			if (recsiz < 0) {
				if (recsiz != -1) death(DEATH_READ, recsiz, NULL);
				if (!endflg) break;
				endflg = 0;
				continue;
			}
		}

		/* record was read */
		if ((endflg <= 1 && ++cnt < recnum) || (prtflg && !recsiz)) continue;
		if (selcnt) {
			selchr = *selchrptr;
			selptr = (struct seldef *) *selptrptr;
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
				if (selpos + sellen > recsiz) selflg = 0;
				if (!selflg) continue;
				if (sellen > 1) selcmp = memcmp(&record[selpos + recoff], &selchr[i3], sellen);
				else {
					c1 = record[selpos + recoff];
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
			if (!selflg) continue;
		}
		if (endflg) {  /* still reading backwards */
			if (--endflg) continue;
			if (aimflg) aimnext = AIONEXT_NEXT;
		}

		for (i1 = recoff; i1 - recoff < recsiz; i1++) {
			if (!record[i1]) record[i1] = ' ';  /* replace null characters with blank */
			else if (record[i1] == '\t') {  /* expand tab characters */
				i3 = i1 - recoff;
				i2 = tabsize - (i3 % tabsize) - 1;
				if (i2) {
					if (i2 > RIO_MAX_RECSIZE - i3 - 1) i2 = RIO_MAX_RECSIZE - i3 - 1;
					if (recsiz > RIO_MAX_RECSIZE - i2) recsiz = RIO_MAX_RECSIZE - i2;
					memmove(&record[i1 + i2], &record[i1], recsiz - i3);
					memset(&record[i1], ' ', i2);
					recsiz += i2;
					i1 += i2;
				}
				record[i1] = ' ';
			}
		}

		if (column > recsiz) recsiz = 0;
		else recsiz -= column;
		recsiz += recoff;
		if (recsiz > linesz) recsiz = linesz;

		/* top and bottom margins */
		if (pagflg) {
			if (lincnt++ == totlin) {  /* bottom margin */
				for (i1 = 0; i1++ < bottom; ) dspchar('\n');
				lincnt = 1;
			}
			if (lincnt == 1) {  /* top margin */
				for (i1 = 0; i1++ < top; ) {
					if (i1 == 3) {
						if (headsz > 5) {
							msciton(++pagcnt, (UCHAR *) &header[headsz - 5], 5);
							dspstring(header);
						}
					}
					dspchar('\n');
				}
			}
		}

		if (prtflg) {
			c1 = record[recoff];
			if (c1 == '1') dspchar('\f');
			else if (c1 == '0') dspstring("\n\n");
			else if (c1 == '-') dspstring("\n\n\n");
			else if (c1 != '+') dspchar('\n');
		}
		else {
			if (numsiz) {
				mscoffton(cnt, &record[column], numsiz - 2);
				record[column + (numsiz - 2)] = '.';
				record[column + numsiz - 1] = ' ';
			}
			if (keytag) {
				riolastpos(handle, &startpos);
				mscoffton(startpos, &record[column + numsiz], keytag);
			}
		}
		i1 = column + recsiz;
		i2 = column + prtflg;
		while (i1-- > i2) if (record[i1] != ' ') break;
		if (!prtflg) record[++i1] = '\n';
		else if (c1 != '$') record[++i1] = '\r';
		record[++i1] = '\0';
		dspstring((CHAR *) &record[i2]);
	}

	if (pagflg && lincnt) {
		totlin += bottom;
		while (lincnt++ < totlin) dspchar('\n');
	}
	rioclose(handle);
	if (isiflg) xioclose(xhandle);
	else if (aimflg) aioclose(xhandle);
	cfgexit();
	exit(0);
	return(0);
}

static INT aimrec(INT aionextflag)
{
#define AIMREC_FLAGS_FORWARD	0x01
#define AIMREC_FLAGS_FIXED		0x02
#define AIMREC_FLAGS_CHECKPOS	0x04
	static OFFSET aimlastpos, aimnextpos, aimrec = -1;
	INT flags, retcode;
	OFFSET eofpos, offset;

	flags = 0;
	if (aionextflag == AIONEXT_NEXT) flags |= AIMREC_FLAGS_FORWARD;
	if (fixflg) flags |= AIMREC_FLAGS_FIXED;
	if (aimrec >= 0) {
		if (aionextflag == AIONEXT_NEXT) riosetpos(handle, aimnextpos);
		else riosetpos(handle, aimlastpos);
		flags |= AIMREC_FLAGS_CHECKPOS;
	}
	for ( ; ; ) {
		if (aimrec == -2) {
			if (aionextflag == AIONEXT_LAST) {
				retcode = fioflck(handle);
				if (retcode < 0) return retcode;
				rioeofpos(handle, &offset);
				fiofulk(handle);
				riosetpos(handle, offset);
				aionextflag = AIONEXT_PREV;
			}
			if (aionextflag == AIONEXT_NEXT) {
				for ( ; ; ) {
					retcode = rioget(handle, record + recoff, reclen);
					if (retcode != -2) break;
				}
			}
			else {
				for ( ; ; ) {
					retcode = rioprev(handle, record + recoff, reclen);
					if (retcode < 0) {
						if (retcode != -2) break;
						continue;
					}
					break;
				}
			}
			return retcode;
		}
		/* read an aim record and see if it matches the criteria */
		if (aimrec == -1) {
			retcode = aionext(xhandle, aionextflag);
			if (retcode != 1) {
				if (retcode == 3) {  /* not enough key information */
					aimrec = -2;
					continue;
				}
				if (retcode == 2) retcode = -1;
				return retcode;
			}
			if (aionextflag == AIONEXT_LAST) aionextflag = AIONEXT_PREV;
	
			/* a possible record exists */
			if (flags & AIMREC_FLAGS_FIXED) {  /* fixed length records */
				offset = filepos * (reclen + eorsize);
				riosetpos(handle, offset);
			}
			else {  /* variable length records */
				offset = filepos << 8;
				if (aionextflag == AIONEXT_PREV) {
					offset += 256;
					rioeofpos(handle, &eofpos);
					if (offset > eofpos) offset = eofpos;
				}
				riosetpos(handle, offset);
				if (offset) {
					rioget(handle, record + recoff, reclen);
					if (aionextflag == AIONEXT_PREV) {
						rionextpos(handle, &offset);
						riosetpos(handle, offset);
					}
				}
				aimrec = filepos;
			}
			flags &= ~AIMREC_FLAGS_CHECKPOS;
		}
		do {
			if (flags & (AIMREC_FLAGS_FORWARD | AIMREC_FLAGS_FIXED)) {
				if ((flags & AIMREC_FLAGS_CHECKPOS) && (aimnextpos - 1) >> 8 != aimrec) {
					aimrec = -1;
					break;
				}
				retcode = rioget(handle, record + recoff, reclen);
			}
			else retcode = rioprev(handle, record + recoff, reclen);
			if (retcode < 0 && retcode != -2) {
				aimrec = -1;
				return retcode;
			}
			if (!(flags & AIMREC_FLAGS_FIXED)) {
				riolastpos(handle, &offset);
				aimlastpos = offset;
				if (offset) offset = (offset - 1) >> 8;
				if (offset != aimrec) {
					aimrec = -1;
					break;
				}
				offset = aimlastpos;
				rionextpos(handle, &aimnextpos);
			}
			if (retcode == -2 || aiochkrec(xhandle, record + recoff, retcode)) continue;
			return retcode;
		} while (!((flags |= AIMREC_FLAGS_CHECKPOS) & AIMREC_FLAGS_FIXED));
	}
#undef AIMREC_FLAGS_FORWARD
#undef AIMREC_FLAGS_FIXED
#undef AIMREC_FLAGS_CHECKPOS
}

/* USAGE */
static void usage()
{
	dspstring("LIST command  " RELEASEPROGRAM RELEASE COPYRIGHT);
	dspchar('\n');
	dspstring("Usage:  list file [-A=key] [-B=n] [-C=n] [-CFG=cfgfile] [-E=n] [-F] [-H=string]\n");
	dspstring("             [-I[=key]] [-J[R]] [-L=n] [-N[=n]] [-O=optfile] [OR] [-P=n]\n");
	dspstring("             [-Pn[-n]EQc[c...]] [-Pn[-n]NEc[c...]] [-Pn[-n]GTc[c...]]\n");
	dspstring("             [-Pn[-n]GEc[c...]] [-Pn[-n]LTc[c...]] [-Pn[-n]LEc[c...]] [-R=n]\n");
	dspstring("             [-S=n] [-T=n] [-X[=n]]\n");
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
