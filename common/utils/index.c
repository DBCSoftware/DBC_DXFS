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
#define INC_LIMITS
#define INC_ERRNO
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

#define SELMAX 100
#define SELSIZE 1024
#define KEYMAX 100
#define ARGSIZE 921
#define LEVELMAX 32
#define BLKMAX 32
#define MIN_BLKSIZE 512
#define MAX_BLKSIZE 16384
#if ((MAX_BLKSIZE * 4) > (RIO_MAX_RECSIZE + 4))
#define RECSIZE (MAX_BLKSIZE * 4)
#else
#define RECSIZE (RIO_MAX_RECSIZE + 4)
#endif

#define EQUAL    0x0001
#define NOTEQUAL 0x0002
#define GREATER  0x0004
#define LESS     0x0008
#define STRING   0x0010
#define OR       0x0020

#define FLAGS_VERBOSE	0x01
#define FLAGS_DSPXTRA	0x02

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
#define DEATH_COLLATE		10
#define DEATH_TOOMANYSEL	11
#define DEATH_TOOMANYKEY	12
#define DEATH_TOOMANYARG	13
#define DEATH_KEYTOOLONG	14
#define DEATH_BLKTOOSMALL	15
#define DEATH_NEEDFIX		16
#define DEATH_RECLENGTH		17
#define DEATH_NOTISI8		18
#define DEATH_INVINDEX		19
#define DEATH_MUTX_JY		20
#define DEATH_MUTX_DF		21
#define DEATH_BADEOF		22
#define DEATH_INTERNAL1		23
#define DEATH_INTERNAL2		24

struct sel {
	INT pos;
	INT ptr;
	INT eqlflg;
};

/* local declarations */
static INT dspflags;
static UCHAR arglistflg;					/* processing argument list from isi header */
static INT inhndl, outhndl;
static INT keycnt;
static SIOKEY *keyptr, *qkeyptr;
static INT selcnt;
static UCHAR xselflg;
static UCHAR *selchr;
static struct sel *selptr;
static SIOKEY **keyptrptr, **qkeyptrptr;
static UCHAR **selchrptr, **selptrptr;
static UCHAR dupflg, eofflg, fixflg, igndupflg, keyflg;
static INT highkey;
static INT fixlen;
static OFFSET dupcnt, reccnt;
static OFFSET topblk, highblk, delblk;
static INT size, size1, size2;
static INT blksize;
static UCHAR *blkbuf[BLKMAX];
static INT blkmax, blkhi, blkmod[BLKMAX];	/* buffer counters and modified flag */
static OFFSET blkpos[BLKMAX], blklru[BLKMAX], lastuse;	/* buffer fpos and lru count */
static INT leafblk, leafpos;				/* current leaf block and end of block pos */
static UCHAR lastkey[XIO_MAX_KEYSIZE + 2];
static UCHAR record[RECSIZE];
static INT createflg;
static INT duphndl;
static CHAR dupname[MAX_NAMESIZE];
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
	"Too many arguments",
	"Total key length is too long (255 is maximum)",
	"Block size is too small to hold the arguments",
	"-S=nnn option must be specified when text file is empty",
	"-K or -S option detects invalid record length",
	"-E option not valid on pre-version 8 ISI",
	"Invalid index",
	"-J option is mutually exclusive with the -Y option",
	"-D option is mutually exclusive with the -F option",
	"Text file contains EOF character before physical EOF",
	"INTERNAL ERROR 1 IN INDEX - CALL FOR HELP",
	"INTERNAL ERROR 2 IN INDEX - CALL FOR HELP"
};

/* routine declarations */
static INT ixin(UCHAR *);
static INT ixout(UCHAR *);
static INT ixins(UCHAR *);
static INT ixcompkey(UCHAR *, UCHAR *);
static INT ixnewblk(void);
static INT ixgetblk(OFFSET);
static void ixflush(void);
static int ixnextparm(char *, int);
static void usage(void);
#if OS_WIN32
__declspec (noreturn) static void death(INT, INT, CHAR *);
#else
static void death(INT, INT, CHAR *)  __attribute__((__noreturn__));
#endif

static void quitsig(INT);


INT main(INT argc, CHAR *argv[])
{
	INT i1, i2, i3, i4, arghi, namelen, selhi, version;
	INT memsize, openflg;
	CHAR cfgname[MAX_NAMESIZE], inname[MAX_NAMESIZE], outname[MAX_NAMESIZE];
	CHAR keytagname[MAX_NAMESIZE], workname[MAX_NAMESIZE], work[300], *ptr;
	CHAR *workdir, *workfile;

	/**
	 * 'Use Existing Arguments Flag' ( -E )
	 */
	UCHAR argflg;
	UCHAR orflg;

	/**
	 * 'Rename File Parameter' ( -R )
	 */
	UCHAR renflg;
	UCHAR txtflg;
	UCHAR c1, c2, *arglit, **arglitptr, **pptr;
	struct rtab *r;
	FIOPARMS parms;
	UINT initialHeapAllocation = -1;

	arginit(argc, argv, &i1);
	if (!i1) dspsilent();
	signal(SIGINT, quitsig);

 	/* initialize */
	// 100 << 11 == 204,800
	if (sizeof(void*) == 8) {
		initialHeapAllocation = 204800 * 2;
	}
	else {
		initialHeapAllocation = 204800;
	}
	if (meminit(initialHeapAllocation, 0, 32) == -1) death(DEATH_INIT, ERR_NOMEM, NULL);
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
	arglitptr = memalloc(ARGSIZE * sizeof(UCHAR), 0);
	keyptrptr = (SIOKEY **) memalloc(KEYMAX * sizeof(SIOKEY), 0);
	selchrptr = memalloc(SELSIZE * sizeof(UCHAR), 0);
	selptrptr = memalloc(SELMAX * sizeof(struct sel), 0);
	if (arglitptr == NULL || keyptrptr == NULL || selchrptr == NULL || selptrptr == NULL) death(DEATH_INIT, ERR_NOMEM, NULL);

	ptr = work;
	arglit = *arglitptr;
	keyptr = *keyptrptr;
	selchr = *selchrptr;
	selptr = (struct sel *) *selptrptr;
	/* MATCH CHANGES WITH BELOW */
	arghi = highkey = keycnt = selcnt = selhi = 0;
	memsize = 0;
	argflg = arglistflg = dupflg = eofflg = fixflg = FALSE;
	igndupflg = keyflg = orflg = renflg = txtflg = xselflg = FALSE;
	blksize = 1024;
	openflg = RIO_M_ERO | RIO_P_TXT | RIO_T_ANY;
	createflg = RIO_M_PRP | RIO_P_TXT | RIO_T_STD | RIO_UNC;
	dupname[0] = '\0';
	workname[0] = '\0';

	/* scan input and output file names */
	i1 = argget(ARG_FIRST, inname, sizeof(inname));
	if (!i1) i1 = argget(ARG_NEXT, outname, sizeof(outname));
	if (i1 < 0) death(DEATH_INIT, i1, NULL);
	if (i1 == 1) usage();
	namelen = (INT)strlen(inname);
	for (i1 = 0; isdigit(outname[i1]); i1++);
	if (i1 && outname[i1] == '-') for (i1++; isdigit(outname[i1]); i1++);
	if (outname[0] == '-' || !outname[i1]) {
		strcpy(ptr, outname);
		strcpy(outname, inname);
		miofixname(outname, ".isi", FIXNAME_EXT_ADD | FIXNAME_EXT_REPLACE);
		goto scanparm;
	}
	miofixname(outname, ".isi", FIXNAME_EXT_ADD);

	/* get next parameter */
	while (!ixnextparm(ptr, sizeof(work))) {
scanparm:
		if (ptr[0] == '-') {
			switch (toupper(ptr[1])) {
				case '!':
					dspflags |= FLAGS_VERBOSE | FLAGS_DSPXTRA;
					break;
				case 'A':
					if (ptr[2] != '=' || !ptr[3]) death(DEATH_INVPARM, 0, ptr);
					memsize = atoi(&ptr[3]) << 10;
					if (memsize < 1) death(DEATH_INVPARMVAL, 0, ptr);
					break;
				case 'B':
					if (ptr[2] != '=' || !ptr[3]) death(DEATH_INVPARM, 0, ptr);
					blksize = atoi(&ptr[3]);
					for (i1 = MIN_BLKSIZE; i1 != blksize && i1 < MAX_BLKSIZE; i1 <<= 1);
					if (i1 != blksize) death(DEATH_INVPARMVAL, 0, ptr);
					break;
				case 'C':
					if (toupper(ptr[2]) != 'F' || toupper(ptr[3]) != 'G' || ptr[4] != '=' || !ptr[5]) death(DEATH_INVPARM, 0, ptr);
					break;
				case 'D':
					dupflg = TRUE;
					break;
				case 'E':
					argflg = TRUE;
					break;
				case 'F':
					igndupflg = TRUE;
					if (ptr[2] == '=') {
						if (!ptr[3]) death(DEATH_INVPARM, 0, ptr);
						strcpy(dupname, &ptr[3]);
					}
					break;
				case 'J':
					if (toupper(ptr[2]) == 'R') openflg = RIO_M_SRO | RIO_P_TXT | RIO_T_ANY;
					else openflg = RIO_M_SHR | RIO_P_TXT | RIO_T_ANY;
					break;
				case 'K':
					if (ptr[2] != '=' || !ptr[3]) death(DEATH_INVPARM, 0, ptr);
					strcpy(keytagname, &ptr[3]);
					keyflg = TRUE;
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
					renflg = TRUE;
					break;
				case 'S':
					fixlen = 0;
					if (ptr[2] == '=' && ptr[3]) {
						fixlen = atoi(&ptr[3]);
						if (fixlen < 1 || fixlen > RIO_MAX_RECSIZE) death(DEATH_INVPARMVAL, 0, ptr);
					}
					fixflg = TRUE;
					break;
				case 'T':
					txtflg = TRUE;
					break;
				case 'V':
					dspflags |= FLAGS_VERBOSE;
					break;
				case 'W':
					if (ptr[2] != '=' || !ptr[3]) death(DEATH_INVPARM, 0, ptr);
					strcpy(workname, &ptr[3]);
					break;
				case 'X':
					xselflg = TRUE;
					break;
				case 'Y':
					eofflg = TRUE;
					break;
				default:
					death(DEATH_INVPARMVAL, 0, ptr);
			}
		}
		else if (isdigit(ptr[0])) {
			if (keycnt == KEYMAX) death(DEATH_TOOMANYKEY, 0, NULL);
			keyptr[keycnt].typeflg = SIO_ASCEND;
			for (i1 = 0, i2 = 0; i1 < 5 && isdigit(ptr[i1]); )
				i2 = i2 * 10 + ptr[i1++] - '0';
			keyptr[keycnt].start = i2 - 1;
			if (ptr[i1] == '-') {
				for (i1++, i2 = 0, i3 = 0; i3 < 5 && isdigit(ptr[i1]); i3++)
					i2 = i2 * 10 + ptr[i1++] - '0';
			}
			if (ptr[i1] || keyptr[keycnt].start < 0 || i2 <= keyptr[keycnt].start || i2 > RIO_MAX_RECSIZE) death(DEATH_INVPARMVAL, 0, ptr);
			if (i2 > highkey) highkey = i2;
			keyptr[keycnt].end = i2;
			keycnt++;
		}
		else if (toupper(ptr[0]) == 'O' && toupper(ptr[1]) == 'R' && !ptr[2]) {
			if (selcnt) orflg = TRUE;
		}
		else death(DEATH_INVPARM, 0, ptr);

		/* save the argument */
		i1 = (INT)strlen(ptr);
		if (namelen + arghi + i1 > ARGSIZE) death(DEATH_TOOMANYARG, 0, NULL);
		memcpy(&arglit[arghi], ptr, i1);
		arghi += i1;
		arglit[arghi++] = DBCEOR;
	}

	if (argflg || renflg) {
		outhndl = fioopen(outname, FIO_M_EXC | FIO_P_TXT);
		if (outhndl < 0) death(DEATH_OPEN, outhndl, outname);
		i1 = fioread(outhndl, 0L, record, 1024);
		if (i1 < 0) death(DEATH_READ, i1, NULL);

		/* check for what seems to be a valid index file */
		if (i1 < 512) death(DEATH_INVINDEX, 0, NULL);
		if (record[0] != 'I' || record[100] != DBCEOR) death(DEATH_INVINDEX, 0, NULL);
		if (record[99] != ' ') {
			version = record[99] - '0';
			if (record[98] != ' ') version += (record[98] - '0') * 10;
		}
		else version = 0;
		c1 = record[57];
		if (version > 10) c1 = DBCDEL;
		else if (version >= 9) {
			if (c1 != ' ' && c1 != 'S') c1 = DBCDEL;
		}
		else if (version >= 7) {
			if (c1 != 'V' && c1 != 'S') c1 = DBCDEL;
		}
		else if (version == 6) {
			if (c1 != 'V' && c1 != 'F') c1 = DBCDEL;
		}
		else if (c1 != 'D') c1 = DBCDEL;
		if (record[0] != 'I' || record[100] != DBCEOR || c1 == DBCDEL) death(DEATH_INVINDEX, 0, NULL);
		mscntoi(&record[41], &i2, 5);
		if (i1 > i2) i1 = i2;

		if (argflg) {
			record[i1] = DBCDEL;
			if (version < 8) death(DEATH_NOTISI8, 0, NULL);
			fioclose(outhndl);
			arghi = highkey = keycnt = selcnt = selhi = 0;
			memsize = 0;
			argflg = dupflg = eofflg = fixflg = FALSE;
			igndupflg = keyflg = orflg = renflg = txtflg = xselflg = FALSE;
			blksize = 1024;
			openflg = RIO_M_ERO | RIO_P_TXT | RIO_T_ANY;
			createflg = RIO_M_PRP | RIO_P_TXT | RIO_T_STD | RIO_UNC;
			dupname[0] = '\0';
			workname[0] = '\0';
			arglistflg = TRUE;
			ixnextparm(ptr, sizeof(work));  /* skip file name */
			ixnextparm(ptr, sizeof(work));
			goto scanparm;
		}

		/* rename */
		i3 = (INT)strlen(inname);
		if (version >= 9) {
			i4 = i3 + 101;
			for (i2 = 101; i2 < i1 && record[i2] != DBCEOR; i2++);
			if (i2 == i1) death(DEATH_INVINDEX, 0, NULL);
			memmove(&record[i4], &record[i2], i1);
			if (i4 < i2) memset(&record[i1 - (i2 - i4)], DBCDEL, i2 - i4);
		}
		else memset(&record[101], ' ', 64);
		memcpy(&record[101], inname, i3);

		if (txtflg) record[55] = 'T';
		i1 = fiowrite(outhndl, 0L, record, i1);
		if (i1) death(DEATH_WRITE, i1, NULL);
		fioclose(outhndl);
		cfgexit();
		exit(0);
	}

	if (!keycnt && !keyflg) usage();
	if (namelen + arghi + 102 > blksize) death(DEATH_BLKTOOSMALL, 0, NULL);
	if (eofflg && (openflg & FIO_M_MASK) != RIO_M_ERO) death(DEATH_MUTX_JY, 0, NULL);
	if (dupflg && igndupflg) death(DEATH_MUTX_DF, 0, NULL);

	i1 = arghi * sizeof(UCHAR);
	if (i1 < 512) i1 = 512;  /* reserve memory for fioopen of output file */
	i1 = memchange(arglitptr, i1, 0);
	if (!i1) i1 = memchange((UCHAR **) keyptrptr, keycnt * sizeof(SIOKEY), 0);
	if (!i1) i1 = memchange(selchrptr, selhi * sizeof(UCHAR), 0);
	if (!i1) i1 = memchange(selptrptr, selcnt * sizeof(struct sel), 0);
	if (!i1) {
		qkeyptrptr = keyptrptr;
		keyptrptr = (SIOKEY **) memalloc(2 * sizeof(SIOKEY), 0);
		if (keyptrptr == NULL) i1 = -1;
	}
	if (i1) death(DEATH_INIT, ERR_NOMEM, NULL);

	/* get the collate file */
	collateflg = FALSE;
	pptr = fiogetopt(FIO_OPT_COLLATEMAP);
	if (pptr != NULL) {
		memcpy(priority, *pptr, UCHAR_MAX + 1);
		collateflg = TRUE;
	}

	/* open the input file */
	if (!keyflg) ptr = inname;
	else ptr = keytagname;
	inhndl = rioopen(ptr, openflg, 7, RIO_MAX_RECSIZE);
	if (inhndl < 0) death(DEATH_OPEN, inhndl, ptr);

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

	if (fixflg || keyflg) {  /* fixed length records */
		while ((i1 = rioget(inhndl, record, RIO_MAX_RECSIZE)) == -2);
		if (i1 <= -3) death(DEATH_READ, i1, NULL);
		if (!fixlen) {
			if (i1 == -1) death(DEATH_NEEDFIX, 0, NULL);
			fixlen = i1;
		}
		else if (i1 != -1 && fixlen != i1) death(DEATH_RECLENGTH, 0, NULL);
		pptr = fiogetwptr(inhndl);
		if (pptr == NULL) death(DEATH_INIT, ERR_NOTOP, NULL);
		r = (struct rtab *) *pptr;
		if (r->type != 'R') death(DEATH_INIT, ERR_NOTOP, NULL);
		r->opts |= RIO_FIX | RIO_UNC;
		riosetpos(inhndl, 0L);
	}
	else fixlen = RIO_MAX_RECSIZE;

	/* compress the keys together */
	if (!keyflg) {
		qkeyptr = *qkeyptrptr;
		size = 0;
		for (i1 = 0; i1 < keycnt; i1++) {
			qkeyptr[i1].end -= qkeyptr[i1].start;
			size += qkeyptr[i1].end;
		}
	}
	else {
		size = fixlen - 12;
		if (size <= 0) death(DEATH_RECLENGTH, 0, NULL);
	}
	if (size > XIO_MAX_KEYSIZE) death(DEATH_KEYTOOLONG, 0, NULL);
	size1 = size + 6;
	size2 = size1 + 6;

	/* build the header block */
	record[0] = 'i';  /* will be 'I' if successful */
	memset(&record[1], 0, 24);
	memset(&record[25], ' ', 75);
	msciton(blksize, &record[41], 5);  /* block size */
	record[54] = 'L';
	if (txtflg) record[55] = 'T';
	if (dupflg) record[56] = 'D';
	if (fixflg && !keyflg) {
		record[57] = 'S';
		msciton(fixlen, &record[64], 5);
	}
	if (size > 99) record[58] = (UCHAR)((size - 100) / 10 + 'A');
	else if (size > 9) record[58] = (UCHAR)(size / 10 + '0');
	record[59] = (UCHAR)(size % 10 + '0');
	record[98] = '1';
	record[99] = '0';
	record[100] = DBCEOR;
	i1 = (INT)strlen(inname);
	memcpy(&record[101], inname, i1);
	i1 += 101;
	record[i1++] = DBCEOR;
	memcpy(&record[i1], *arglitptr, arghi);
	i1 += arghi;
	memset(&record[i1], DBCDEL, blksize - i1);
	memfree(arglitptr);

	/* create the index file */
	outhndl = fioopen(outname, FIO_M_PRP | FIO_P_TXT);
	if (outhndl < 0) death(DEATH_CREATE, outhndl, outname);

	/* write the header */
	i1 = fiowrite(outhndl, 0L, record, blksize);
	if (i1) death(DEATH_WRITE, i1, NULL);

	/* initialize variables */
	dupcnt = reccnt = 0L;
	topblk = highblk = delblk = 0;
	for (blkmax = 0, i1 = 0; blkmax < BLKMAX && i1 + blksize <= (INT) sizeof(record); blkmax++, i1 += blksize) blkbuf[blkmax] = &record[i1];
	keyptr = *keyptrptr;
	keyptr[0].start = 0;
	keyptr[0].end = size;
	keyptr[0].typeflg = SIO_ASCEND;
	keyptr[1].start = size;
	keyptr[1].end = size1;
	keyptr[1].typeflg = SIO_POSITION | SIO_ASCEND;

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
	i1 = SIOFLAGS_DSPERROR;
	if (dspflags & FLAGS_VERBOSE) i1 |= SIOFLAGS_DSPPHASE;
	if (dspflags & FLAGS_DSPXTRA) i1 |= SIOFLAGS_DSPEXTRA;
	if (siosort(size1, 2, keyptrptr, ixin, ixout, workdir, workfile, i1, memsize, dspstring) < 0) {
		cfgexit();	
		exit(1);
	}
	if (!reccnt) rioclose(inhndl);
	else ixflush();

	/* finalize the header block and write it */
	record[0] = 'I';
	mscoffto6x(0, &record[1]);  /* 0 */
	mscoffto6x(delblk, &record[7]);  /* offset of deleted records block */
	mscoffto6x(topblk, &record[13]);  /* offset of top branch block */
	mscoffto6x(highblk, &record[19]);  /* offset of last used block */
	i1 = fiowrite(outhndl, 0L, record, 25);
	if (i1) death(DEATH_WRITE, i1, NULL);
	fioclose(outhndl);

	if (dspflags & FLAGS_VERBOSE) {
		mscofftoa(reccnt, (CHAR *) record);
		dspstring("Index complete, ");
		dspstring((CHAR *) record);
		dspstring(" key(s) inserted\n");
	}
	if (dupcnt) {
		if (dspflags & FLAGS_VERBOSE) {
			mscofftoa(dupcnt, (CHAR *) record);
			dspstring((CHAR *) record);
			dspstring(" duplicate key(s) ignored\n");
		}
		if (!igndupflg) {
			cfgexit();
			exit(1);
		}
		if (duphndl > 0) rioclose(duphndl);
	}
	cfgexit();
	exit(0);
	return(0);
}

/* IXIN */
/* provide a record to sort */
static INT ixin(UCHAR *rec)
{
	INT i1, i2, i3, recsiz;
	INT selflg, selcmp, selpos, sellen, seleqlflg;
	OFFSET eofpos, pos;
	UCHAR c1;

	for ( ; ; ) {
		recsiz = rioget(inhndl, record, fixlen);
		if (keyflg) {  /* input from keytag file */
			if (recsiz < 12) {
				if (recsiz == -1) {  /* end of file */
					if (eofflg) {
						rioeofpos(inhndl, &eofpos);
						if (pos != eofpos) {
							siokill();
							death(DEATH_BADEOF, 0, NULL);
						}
					}
					return(1);
				}
				if (recsiz == -2) continue;  /* deleted record */
				if (recsiz >= 0) recsiz = ERR_BADRL;
				siokill();
				death(DEATH_READ, recsiz, NULL);
			}
			memcpy(rec, &record[12], fixlen - 12);
			mscntooff((UCHAR *) record, &pos, 12);
			mscoffto6x(pos, (UCHAR *) &rec[fixlen - 12]);
			return(0);
		}
		riolastpos(inhndl, &pos);
		if (recsiz < highkey) {
			if (recsiz == -1) {  /* end of file */
				if (eofflg) {
					rioeofpos(inhndl, &eofpos);
					if (pos != eofpos) {
						siokill();
						death(DEATH_BADEOF, 0, NULL);
					}
				}
				return(1);
			}
			if (recsiz == -2) {  /* deleted record */
				if (!fixflg) continue;
				if (delblk) {
/*** CODE OPTIMIZE: ADD 2048 BYTES TO RECORD AND BUFFER THIS UP ***/
					i1 = fioread(outhndl, delblk, record, blksize);
					if (i1 != blksize || record[0] != 'F') {
						siokill();
						if (i1 < 0) death(DEATH_READ, i1, NULL);
						death(DEATH_INVINDEX, 0, NULL);
					}
					for (i1 = 1; i1 < blksize && record[i1] != DBCDEL; i1 += 6);
				}
				else i1 = blksize;
				if (i1 + 6 > blksize) {  /* 'F' block will overflow */
					memset(record, DBCDEL, blksize);
					record[0] = 'F';
					mscoffto6x(delblk, &record[1]);
					highblk = (delblk += blksize);
					i1 = 7;
				}
				mscoffto6x(pos, &record[i1]);
				i1 = fiowrite(outhndl, delblk, record, blksize);
				if (i1) {
					siokill();
					death(DEATH_WRITE, i1, NULL);
				}
				continue;
			}
			if (recsiz <= -3) {
				siokill();
				death(DEATH_READ, recsiz, NULL);
			}
			if (!recsiz) {
				if (dspflags & FLAGS_VERBOSE) dspstring("\rZero length record encountered - ignored\n");
				continue;
			}
			memset(&record[recsiz], ' ', highkey - recsiz);
		}
		if (selcnt) {
			selchr = *selchrptr;
			selptr = (struct sel *) *selptrptr;
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
				if (sellen > 1) selcmp = memcmp(&record[selpos], &selchr[i2], sellen);
				else {
					c1 = record[selpos];
					if (seleqlflg & (GREATER | LESS)) selcmp = (INT) c1 - selchr[i2];
					else {
						for ( ; selchr[i2] && c1 != selchr[i2]; i2++);
						selcmp = !selchr[i2];
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
		qkeyptr = *qkeyptrptr;
		for (i1 = 0, recsiz = 0; i1 < keycnt; i1++, recsiz += i2) {
			i2 = qkeyptr[i1].end;
			memcpy(&rec[recsiz], &record[qkeyptr[i1].start], i2);
		}
		mscoffto6x(pos, &rec[size]);
		return(0);
	}
}

/* IXOUT */
/* get an output record from sort and process it */
static INT ixout(UCHAR *rec)
{
	static INT firstflg = TRUE;
	INT i1;
	CHAR work[17];

	if (firstflg) {
		firstflg = FALSE;
		rioclose(inhndl);

		/* first record, put into blkbuf[0] */
		leafblk = ixnewblk();
		if (leafblk < 0) {
			siokill();
			death(DEATH_WRITE, (INT) leafblk, NULL);
		}
		/* leafblk will always be valid without calls to ixgetblk */
		topblk = blkpos[leafblk];  /* its the top block */
		blkbuf[leafblk][0] = 'V';  /* low level block indicator */
		memcpy(&blkbuf[leafblk][1], rec, size1);
		leafpos = size1 + 1;
		memcpy(lastkey, rec, size);
		lastkey[size] = '\n';
		lastkey[size + 1] = 0;
		reccnt = 1;
		return 0;
	}

	if (!dupflg && ixcompkey(rec, lastkey) == size) {
		memcpy(lastkey, rec, size);
		if (!igndupflg) {
			if (dspflags & FLAGS_DSPXTRA) dspstring("\r                       ");
			dspstring("\rDuplicate key ->");
			dspstring((CHAR *) lastkey);
		}
		else if (dupname[0]) {
			if (!duphndl) {
				duphndl = rioopen(dupname, createflg, 1, 100);
				if (duphndl < 0) {
					siokill();
					death(DEATH_CREATE, duphndl, dupname);
				}
			}
			i1 = rioput(duphndl, lastkey, size);
			if (i1) {
				siokill();
				death(DEATH_WRITE, i1, NULL);
			}
		}
		dupcnt++;
		return 0;
	}

	i1 = ixcompkey(rec, lastkey);
	if (i1 > 255) i1 = 255;
	if (leafpos + size1 - i1 < blksize) {  /* low level block will not overflow when this key is added */
		blkbuf[leafblk][leafpos++] = (UCHAR) i1;
		memcpy(&blkbuf[leafblk][leafpos], &rec[i1], size1 - i1);
		leafpos += size1 - i1;
	}
	else {  /* overflow will happen */
		i1 = ixins(rec);
		if (i1) {
			siokill();
			death(DEATH_WRITE, i1, NULL);
		}
	}
	memcpy(lastkey, rec, size);
	reccnt++;
	if ((dspflags & FLAGS_DSPXTRA) && !(reccnt & 0x03FF)) {
		mscofftoa(reccnt, work);
		dspchar('\r');
		dspstring(work);
		dspstring(" keys inserted");
		dspflush();
	}
	return 0;
}

/* IXINS */
/* insert a key into a leaf block using overflow or split */
/* returns with leafblk and leafpos correctly set */
static INT ixins(UCHAR *key)
{
	static INT branchhi = 0;
	static struct BRANCHDEF {
		OFFSET pos;
		OFFSET lftpos;
		INT eob;
		INT lfteob;
		UCHAR lftrpt;
	} branch[LEVELMAX];
	INT i1, i2, i3, branchcnt, leafflg;
	INT blk1, blk2, blk3;  /* left block, parent block, right block */
	INT eob1, eob2, eob3;  /* left eob, parent eob, right eob */
	OFFSET block;
	UCHAR workkey[XIO_MAX_KEYSIZE + 20];

	leafflg = TRUE;
	eob3 = leafpos;
	branchcnt = branchhi;
	block = blkpos[leafblk];
	for ( ; ; ) {
		blk3 = ixgetblk(block);
		if (blk3 < 0) return(blk3);
		if (!leafflg) {
			eob3 = branch[branchcnt].eob;
			if (eob3 + size2 <= blksize) {  /* no overflow will happen */
				memcpy(&blkbuf[blk3][eob3], workkey, size2);
				blkmod[blk3] = TRUE;
				branch[branchcnt].eob += size2;
				return(0);
			}
		}

		/* overflow will happen */
		if (branchcnt) { /* try to move a key to left brother */
			blk2 = ixgetblk(branch[branchcnt - 1].pos);  /* get parent */
			if (blk2 < 0) return(blk2);
			if (blkbuf[blk2][0] != 'U') {
				siokill();
				death(DEATH_INTERNAL1, 0, NULL);
			}

			if (branch[branchcnt - 1].lftpos != -1) {  /* this block has a left brother, add to it */
				blk1 = ixgetblk(branch[branchcnt - 1].lftpos);  /* blk1 is block number of left brother */
				if (blk1 < 0) return(blk1);

				/* won't use left brother again*/
				blklru[blk1] = 0;
				branch[branchcnt - 1].lftpos = -1;

				eob1 = branch[branchcnt - 1].lfteob;
				eob2 = branch[branchcnt - 1].eob - size2;
				i1 = branch[branchcnt - 1].lftrpt;
				blkmod[blk1] = blkmod[blk2] = blkmod[blk3] = TRUE;
				if (leafflg) {  /* its a leaf left overflow */
					blkbuf[blk1][eob1++] = i1;
					memcpy(&blkbuf[blk1][eob1], &blkbuf[blk2][eob2 + i1], size1 - i1);
					memcpy(&blkbuf[blk2][eob2], &blkbuf[blk3][1], size1);
					i1 = blkbuf[blk3][size1 + 1];
					memmove(&blkbuf[blk3][i1 + 1], &blkbuf[blk3][size1 + 2], eob3 - (size1 + 2));
					eob3 -= size1 - i1 + 1;
					memset(&blkbuf[blk3][eob3], DBCDEL, blksize - eob3);
					i1 = ixcompkey(key, lastkey);
					if (i1 > 255) i1 = 255;
					if (eob3 + size1 - i1 + 1 > blksize) goto ixins1;  /* still won't fit */
					blkbuf[blk3][eob3++] = (UCHAR) i1;
					memcpy(&blkbuf[blk3][eob3], &key[i1], size1 - i1);
					leafpos = eob3 + size1 - i1;
				}
				else {  /* its a branch left overflow */
					memcpy(&blkbuf[blk1][eob1], &blkbuf[blk2][eob2], size1);
					memcpy(&blkbuf[blk1][eob1 + size1], &blkbuf[blk3][1], 6);
					memcpy(&blkbuf[blk2][eob2], &blkbuf[blk3][7], size1);
					memmove(&blkbuf[blk3][1], &blkbuf[blk3][size2 + 1], eob3 - (size2 + 1));
					memcpy(&blkbuf[blk3][eob3 - size2], workkey, size2);
				}
				return(0);
			}
		}
		else {  /* block is top block, split & create new top block */
			if (branchhi == LEVELMAX) {
				siokill();
				death(DEATH_INTERNAL2, 0, NULL);
			}
			blk2 = ixnewblk();  /* get new parent */
			if (blk2 < 0) return(blk2);
			blkbuf[blk2][0] = 'U';
			mscoffto6x(block, &blkbuf[blk2][1]);
			topblk = blkpos[blk2];
			memmove((UCHAR *) &branch[1], (UCHAR *) branch, branchhi++ * sizeof(struct BRANCHDEF));
			branch[0].pos = topblk;
			branch[0].eob = 7;
			branchcnt = 1;
		}

		/* this block has full/no left brother, must split */
ixins1:
		blk1 = blk3;
		eob1 = eob3;
		blk3 = ixnewblk();  /* will be new current leaf (new right block) */
		if (blk3 < 0) return(blk3);

		branch[branchcnt - 1].lftpos = block;
		blkbuf[blk3][0] = blkbuf[blk1][0];
		if (leafflg) {  /* leaf block split */
			memcpy(&blkbuf[blk3][1], key, size1);
			for (i1 = 1, i2 = 0, i3 = 1; ; ) {
				memcpy(&workkey[i2], &blkbuf[blk1][i1], size - i2);
				i1 += size1 - i2;
				if (i1 >= eob1) {
					memcpy(&workkey[size], &blkbuf[blk1][i1 - 6], 6);
					break;
				}
				i3 = i1;
				i2 = blkbuf[blk1][i1++];
			}
			branch[branchcnt - 1].lftrpt = (UCHAR) i2;
			eob1 = i3;

			/* new leafblk */
			leafblk = blk3;
			leafpos = size1 + 1;
			leafflg = FALSE;
		}
		else {  /* branch block split */
			memcpy(&blkbuf[blk3][1], &blkbuf[blk1][eob1 - 6], 6);
			memcpy(&blkbuf[blk3][7], workkey, size2);
			branch[branchcnt].eob = size2 + 7;
			branch[branchcnt].pos = blkpos[blk3];
			eob1 -= size2;
			memcpy(workkey, &blkbuf[blk1][eob1], size1);
		}

		branch[branchcnt - 1].lfteob = eob1;
		memset(&blkbuf[blk1][eob1], DBCDEL, blksize - eob1);
		blkmod[blk1] = TRUE;
		mscoffto6x(blkpos[blk3], &workkey[size1]);
		branchcnt--;
		block = blkpos[blk2];
	}
}

static INT ixcompkey(UCHAR* key1, UCHAR* key2)
{
	INT i1;

	if (!collateflg) for (i1 = 0; i1 < size && key1[i1] == key2[i1]; i1++);
	else for (i1 = 0; i1 < size; i1++) {
		if (key1[i1] == key2[i1]) continue;
		if (priority[key1[i1]] != priority[key2[i1]]) break;
	}
	return(i1);
}

/* IXNEWBLK */
/* return buffer number of newly created block */
static INT ixnewblk()
{
	INT i1, i2;
	OFFSET lwork;

	if (blkhi == blkmax) {  /* must force out a least recently used buffer */
		for (i1 = i2 = 0, lwork = lastuse + 1; i1 < blkmax; i1++) {
			if (blklru[i1] < lwork && i1 != leafblk) {  /* never force out leafblk */
				lwork = blklru[i1];
				i2 = i1;
			}
		}
		if (blkmod[i2]) {
			i1 = fiowrite(outhndl, blkpos[i2], blkbuf[i2], blksize);
			if (i1) return(i1);
		}
	}
	else i2 = blkhi++;
	memset(blkbuf[i2], DBCDEL, blksize);
	highblk += blksize;
	blkpos[i2] = highblk;
	blklru[i2] = ++lastuse;
	blkmod[i2] = TRUE;
	return(i2);
}

/* IXGETBLK */
/* get a block into a buffer, return buffer number */
static INT ixgetblk(OFFSET block)
{
	INT i1, i2;
	OFFSET lwork;

	/* try to find block in buffer pool */
	for (i1 = 0; i1 < blkhi && blkpos[i1] != block; i1++);
	if (i1 != blkhi) {
		blklru[i1] = ++lastuse;
		return(i1);
	}

	/* must force out a least recently used buffer */
	for (i1 = i2 = 0, lwork = lastuse + 1; i1 < blkhi; i1++) {
		if (blklru[i1] < lwork && i1 != leafblk) {  /* never force out leafblk */
			lwork = blklru[i1];
			i2 = i1;
		}
	}
	if (blkmod[i2]) {
		i1 = fiowrite(outhndl, blkpos[i2], blkbuf[i2], blksize);
		if (i1) return(i1);
	}
	blkpos[i2] = block;
	blklru[i2] = ++lastuse;
	blkmod[i2] = FALSE;
	i1 = fioread(outhndl, block, blkbuf[i2], blksize);
	if (i1 != blksize) {
		if (i1 < 0) return(i1);
		return(ERR_RDERR);
	}
	return(i2);
}

/* IXFLUSH */
/* flush all bufers */
static void ixflush()
{
	INT i1, i2;

	for (i1 = 0; i1 < blkhi; i1++) {
		if (blkmod[i1]) {
			i2 = fiowrite(outhndl, blkpos[i1], blkbuf[i1], blksize);
			if (i2) death(DEATH_WRITE, i2, NULL);
		}
	}
}

/* IXNEXTPARM */
/* get next command line parameter */
static int ixnextparm(char *parm, int size)
{
	static INT parmptr = 101;
	int i1;

	if (!arglistflg) {
		i1 = argget(ARG_NEXT, parm, size);
		if (i1 < 0) death(DEATH_INIT, i1, NULL);
		return i1;
	}

	/* using previous arguments */
	if (record[parmptr] == DBCDEL) {
		*parm = 0;
		return(1);
	}
	while (record[parmptr] != DBCEOR && record[parmptr] != DBCDEL) *parm++ = record[parmptr++];
	if (record[parmptr] == DBCDEL) death(DEATH_INVINDEX, 0, NULL);
	*parm = 0;
	parmptr++;
	return(0);
}

/* USAGE */
static void usage()
{
	dspstring("INDEX command  " RELEASEPROGRAM RELEASE COPYRIGHT);
	dspchar('\n');
	dspstring("Usage:  index file1 [file2] key-spec [key-spec...] [-A=n] [-B=n] [-CFG=cfgfile]\n");
	dspstring("              [-D] [-E] [-F=dupfile] [-K] [-J[R]] [-O=optfile] [OR]\n");
	dspstring("              [-Pn[-n]EQc[c...]] [-Pn[-n]NEc[c...]] [-Pn[-n]GTc[c...]]\n");
	dspstring("              [-Pn[-n]GEc[c...]] [-Pn[-n]LTc[c...]] [-Pn[-n]LEc[c...]] [-R]\n");
	dspstring("              [-S[=n]] [-T] [-V] [-W=workfile] [-X] [-Y] [-!]\n");
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
	signal(sig, SIG_IGN);
	siokill();
	dspchar('\n');
	dspstring(errormsg[DEATH_INTERRUPT]);
	dspchar('\n');
	cfgexit();
	exit(1);
}
