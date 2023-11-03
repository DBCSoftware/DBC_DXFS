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
#define INC_LIMITS
#define INC_SIGNAL
#define INC_ERRNO
#include "includes.h"
#include "release.h"
#include "dbccfg.h"
#include "arg.h"
#include "base.h"
#include "fio.h"

#define SELMAX	  100
#define SELSIZE 1024
#define KEYMAX   100
#define ARGSIZE  921

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
#define DEATH_TOOMANYSEL		10
#define DEATH_TOOMANYKEY		11
#define DEATH_TOOMANYARG		12
#define DEATH_NEEDFIX1		13
#define DEATH_NEEDFIX2		14
#define DEATH_RECLENGTH		15
#define DEATH_TOOMANYREC		16
#define DEATH_NOTAIM8		17
#define DEATH_BADAIMHDR		18
#define DEATH_CASEMAP		19
#define DEATH_MUTX_JY		20
#define DEATH_BADEOF		21
#define DEATH_NEEDFIX3      22

struct keydef {
	INT from; /* zero based 'from' key position */
	INT len;  /* keylength minus one */
	INT xclflg;
};

struct seldef {
	INT pos;
	INT ptr;
	INT eqlflg;
};

/* local declarations */
static INT dspflags;
static UCHAR arglistflg;		/* processing argument list from aim header */
static UCHAR *buffer;			/* aim buffer */
static INT inhndl, outhndl;		/* input and output handles */
static UCHAR record[RIO_MAX_RECSIZE + 4];	/* input record buffer */
static UCHAR casemap[UCHAR_MAX + 1];	/* case map bytes */
static UCHAR dstnctflg;			/* distinct flag */
static INT memsize;				/* maximum memory allocation size */
static OFFSET recproc;			/* number of records processed */
static INT keycnt;				/* number of keys */
static struct keydef *keyptr;		/* pointer to key information structure */
static UINT hashbyte;			/* byte offset for hashing */
static UCHAR hashbit;			/* bit offset for hashing */
static INT zvalue;				/* z value */
static INT spcflg;				/* space reclaimation flag (0 or 1) */
static INT slots;				/* number of slots (zvalue + spcflg) */
static OFFSET recnum;				/* record number being hashed */
static OFFSET reccnt;				/* total number of records to hash */
static OFFSET rechi;				/* highest record + 1 for this pass */
static OFFSET slotsiz;			/* size of slot */
static INT worksiz;				/* working size of slot for this pass */
static OFFSET workoff;			/* working offset in slot for this pass */

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
	"Too many keys",
	"Too many arguments",
	"-F option must be specified with the -S option",
	"-F=nnn option must be specified when text file is empty",
	"-F option detects invalid record length",
	"More records in input file than expected",
	"-E option not valid on pre-version 8 AIM",
	"Invalid AIM header block",
	"Unable to read 256 bytes from DBC_CASEMAP",
	"-J option is mutually exclusive with the -Y option",
	"Input file contains EOF character before physical EOF",
	"-F option must be specified with the -P option",
};

/* routine definitions */
static INT axrec(void);
static void axhash(INT, INT, INT);
static INT axdel(void);
static INT axinit(void);
static INT axend(void);
static INT axwrite(void);
static int axnextparm(char *, int);

#if OS_WIN32
__declspec (noreturn) static void death(INT, INT, CHAR *);
__declspec (noreturn) static void quitsig(INT);
__declspec (noreturn) static void usage(void);
#else
static void death(INT, INT, CHAR *)  __attribute__((__noreturn__));
static void quitsig(INT)  __attribute__((__noreturn__));
static void usage(void)  __attribute__((__noreturn__));
#endif



INT main(INT argc, CHAR **argv)
{
	INT i1, i2, i3, i4, arecsiz = 0, reclen, recsiz, highkey, arghi;
	/*
	 * Count of -P options
	 */
	INT selcnt;
	INT selhi, selflg, selcmp, selpos, sellen, seleqlflg;
	INT namelen, openflg, version;
	OFFSET eofpos, pos, prirec, secrec;
	CHAR cfgname[MAX_NAMESIZE], inname[MAX_NAMESIZE], outname[MAX_NAMESIZE];
	CHAR work[300], *ptr;
	UCHAR c1, c2, umatch;
	/*
	 * TRUE if the -F option is used
	 */
	UCHAR fixflg;
	UCHAR addpriflg, argflg, delflg, eofflg, orflg, renflg;
	UCHAR txtflg, xselflg;
	UCHAR *arglit, *selchr;
	UCHAR **arglitptr, **keyptrptr, **selchrptr, **selptrptr, **rptr, **pptr;
	struct seldef *selptr;
	struct rtab *r;
	FIOPARMS parms;

	arginit(argc, argv, &i1);
	if (!i1) dspsilent();
	signal(SIGINT, quitsig);

	/* initialize */
	if (meminit(100 << 10, 0, 32) == -1) death(DEATH_INIT, ERR_NOMEM, NULL);
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

	arglitptr = memalloc(ARGSIZE * sizeof(UCHAR), 0);
	keyptrptr = memalloc(KEYMAX * sizeof(struct keydef), 0);
	selchrptr = memalloc(SELSIZE * sizeof(UCHAR), 0);
	selptrptr = memalloc(SELMAX * sizeof(struct seldef), 0);
	if (arglitptr == NULL || keyptrptr == NULL || selchrptr == NULL || selptrptr == NULL)
		death(DEATH_INIT, ERR_NOMEM, NULL);

	ptr = work;
	arglit = *arglitptr;
	keyptr = (struct keydef *) *keyptrptr;
	selchr = *selchrptr;
	selptr = (struct seldef *) *selptrptr;
	/* MATCH CHANGES WITH BELOW */
	arghi = highkey = keycnt = memsize = reclen = selcnt = selhi = spcflg = 0;
	prirec = secrec = 0L;
	addpriflg = argflg = arglistflg = dstnctflg = FALSE;
	eofflg = fixflg = orflg = renflg = txtflg = xselflg = FALSE;
	umatch = '?';
	zvalue = 199;
	openflg = RIO_M_ERO | RIO_P_TXT | RIO_T_ANY;

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
		miofixname(outname, ".aim", FIXNAME_EXT_ADD | FIXNAME_EXT_REPLACE);
		goto scanparm;
	}
	miofixname(outname, ".aim", FIXNAME_EXT_ADD);

	/* get next parameter */
	while (!axnextparm(ptr, sizeof(work))) {
scanparm:
		if (ptr[0] == '-') {
			switch(toupper(ptr[1])) {
				case '!':
					dspflags |= DSPFLAGS_VERBOSE | DSPFLAGS_DSPXTRA;
					break;
				case 'A':
					if (ptr[2] != '=' || !ptr[3]) death(DEATH_INVPARM, 0, ptr);
					memsize = atol(&ptr[3]) << 10;
					if (memsize < 1) death(DEATH_INVPARMVAL, 0, ptr);
					break;
				case 'C':
					if (toupper(ptr[2]) != 'F' || toupper(ptr[3]) != 'G' || ptr[4] != '=' || !ptr[5]) death(DEATH_INVPARM, 0, ptr);
					break;
				case 'D':
					dstnctflg = TRUE;
					break;
				case 'E':
					argflg = TRUE;
					break;
				case 'F':
					if (ptr[2] == '=') {
						reclen = atoi(&ptr[3]);
						if (reclen < 1 || reclen > RIO_MAX_RECSIZE) death(DEATH_INVPARMVAL, 0, ptr);
					}
					fixflg = TRUE;
					break;
				case 'J':
					if (toupper(ptr[2]) == 'R') openflg = RIO_M_SRO | RIO_P_TXT | RIO_T_ANY;
					else openflg = RIO_M_SHR | RIO_P_TXT | RIO_T_ANY;
					break;
				case 'M':
					if (ptr[2] != '=' || !ptr[3]) death(DEATH_INVPARM, 0, ptr);
					umatch = ptr[3];
					break;
				case 'N':
					if (ptr[2] != '=') death(DEATH_INVPARM, 0, ptr);
					if (ptr[3] == '+') {
						addpriflg = TRUE;
						i1 = 4;
					}
					else i1 = 3;
					prirec = atol(&ptr[i1]);
					if (prirec < 1L) death(DEATH_INVPARMVAL, 0, ptr);
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
						memset(&selchr[selhi], ' ', (UINT) (i2 - i4));
						selhi += i2 - i4;
					}
					selchr[selhi++] = 0;
					orflg = FALSE;
					break;
				case 'R':
					renflg = TRUE;
					break;
				case 'S':
					spcflg = 1;
					break;
				case 'T':
					txtflg = TRUE;
					break;
				case 'V':
					dspflags |= DSPFLAGS_VERBOSE;
					break;
				case 'X':
					if (ptr[2]) {
						if (ptr[2] != '=') death(DEATH_INVPARM, 0, ptr);
						secrec = atol(&ptr[3]);
						if (secrec < 1L) death(DEATH_INVPARMVAL, 0, ptr);
					}
					else xselflg = TRUE;
					break;
				case 'Y':
					eofflg = TRUE;
					break;
				case 'Z':
					if (ptr[2] != '=') death(DEATH_INVPARM, 0, ptr);
					zvalue = atoi(&ptr[3]);
					if (zvalue < 40 || zvalue > 2000) death(DEATH_INVPARMVAL, 0, ptr);
					break;
				default:
					death(DEATH_INVPARM, 0, ptr);
			}
		}
		else if (isdigit(ptr[0])) {
			if (keycnt == KEYMAX) death(DEATH_TOOMANYKEY, 0, NULL);
			keyptr[keycnt].xclflg = FALSE;
			for (i1 = 0, i2 = 0; i1 < 5 && isdigit(ptr[i1]); )
				i2 = i2 * 10 + ptr[i1++] - '0';
			keyptr[keycnt].from = i2;
			if (ptr[i1] == '-') {
				for (i1++, i2 = 0, i3 = 0; i3 < 5 && isdigit(ptr[i1]); i3++)
					i2 = i2 * 10 + ptr[i1++] - '0';
			}
			c1 = (UCHAR) toupper(ptr[i1]);
			if (c1 == 'X') {
				keyptr[keycnt].xclflg = TRUE;
				ptr[i1++] = 'X';  /* force to be upper case */
				c1 = ptr[i1];
			}
			if (c1 || !keyptr[keycnt].from || i2 < keyptr[keycnt].from || i2 > RIO_MAX_RECSIZE) death(DEATH_INVPARMVAL, 0, ptr);
			if (i2 > highkey) highkey = i2;
			keyptr[keycnt].len = i2 -= keyptr[keycnt].from--;  /* len is length - 1 */
			keycnt++;
		}
		else if (toupper(ptr[0]) == 'O' && toupper(ptr[1]) == 'R' && !ptr[2]) {
			if (selcnt) orflg = TRUE;
		}
		else death(DEATH_INVPARM, 0, ptr);

		/* save the argument */
		i1 = (INT)strlen(ptr);
		if (namelen + arghi + i1 > ARGSIZE) death(DEATH_TOOMANYARG, 0, NULL);
		memcpy(&arglit[arghi], ptr, (UINT) i1);
		arghi += i1;
		arglit[arghi++] = DBCEOR;
	}

	if (argflg || renflg) {
		outhndl = fioopen(outname, FIO_M_EXC | FIO_P_TXT);
		if (outhndl < 0) death(DEATH_OPEN, outhndl, outname);
		i1 = fioread(outhndl, 0L, record, 1024);
		if (i1 < 0) death(DEATH_READ, i1, NULL);

		/* check for what seems to be a valid aim file */
		if (i1 != 1024) death(DEATH_BADAIMHDR, 0, NULL);
		if (record[99] != ' ') {
			version = record[99] - '0';
			if (record[98] != ' ') version += (record[98] - '0') * 10;
		}
		else version = 0;
		c1 = record[59];
		if (version > 10) c1 = DBCDEL;
		else if (version >= 7) {
			if (c1 != 'V' && c1 != 'F' && c1 != 'S') c1 = DBCDEL;
		}
		else {
			if (c1 != 'V' && c1 != 'F') c1 = DBCDEL;
			if (!version) i1 = 512;
		}
		if (record[0] != 'A' || record[100] != DBCEOR || c1 == DBCDEL) death(DEATH_BADAIMHDR, 0, NULL);

		if (argflg) {
			record[i1] = DBCDEL;
			if (version < 8) death(DEATH_NOTAIM8, 0, NULL);
			fioclose(outhndl);
			arghi = highkey = keycnt = memsize = reclen = selcnt = selhi = spcflg = 0;
			prirec = secrec = 0L;
			addpriflg = argflg = dstnctflg = FALSE;
			eofflg = fixflg = orflg = renflg = txtflg = xselflg = FALSE;
			umatch = '?';
			zvalue = 199;
			openflg = RIO_M_ERO | RIO_P_TXT | RIO_T_ANY;
			arglistflg = TRUE;
			axnextparm(ptr, sizeof(work));  /* skip file name */
			axnextparm(ptr, sizeof(work));
			goto scanparm;
		}

		/* rename */
		i3 = (INT)strlen(inname);
		if (version >= 9) {
			i4 = i3 + 101;
			for (i2 = 101; i2 < i1 && record[i2] != DBCEOR; i2++);
			if (i2 == i1) death(DEATH_BADAIMHDR, 0, NULL);
			memmove(&record[i4], &record[i2], (UINT) i1);
			if (i4 < i2) memset(&record[i1 - (i2 - i4)], DBCDEL, i2 - i4);
		}
		else memset(&record[101], ' ', 64);
		memcpy(&record[101], inname, (UINT) i3);

		if (txtflg) record[55] = 'T';
		i1 = fiowrite(outhndl, 0L, record, i1);
		if (i1) death(DEATH_WRITE, i1, NULL);
		fioclose(outhndl);
		exit(0);
	}

	if (!keycnt) usage();
	if (spcflg && !fixflg) death(DEATH_NEEDFIX1, 0, NULL);
	if (selcnt && !fixflg) death(DEATH_NEEDFIX3, 0, NULL);
	if (eofflg && (openflg & FIO_M_MASK) != RIO_M_ERO) death(DEATH_MUTX_JY, 0, NULL);

	i1 = arghi * sizeof(UCHAR);
	if (i1 < 256) i1 = 256;  /* reserve memory for fioopen of output file */
	i1 = memchange(arglitptr, i1, 0);
	if (!i1) i1 = memchange(keyptrptr, keycnt * sizeof(struct keydef), 0);
	if (!i1) i1 = memchange(selchrptr, selhi * sizeof(UCHAR), 0);
	if (!i1) i1 = memchange(selptrptr, selcnt * sizeof(struct seldef), 0);
	if (i1) death(DEATH_INIT, ERR_NOMEM, NULL);

	if (!dstnctflg) {
		pptr = fiogetopt(FIO_OPT_CASEMAP);
		if (pptr != NULL) memcpy(casemap, *pptr, (UCHAR_MAX + 1) * sizeof(UCHAR));
		else for (i1 = 0; i1 <= UCHAR_MAX; i1++) casemap[i1] = (UCHAR) toupper(i1);
	}

	/* open the input file */
	inhndl = rioopen(inname, openflg, 7, RIO_MAX_RECSIZE);
	if (inhndl < 0) death(DEATH_OPEN, inhndl, inname);

	/* change prep directory to be same as input file */
	if (!prpget("file", "utilprep", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "dos")) {
		ptr = fioname(inhndl);
		if (ptr != NULL) {
			i1 = fioaslash(ptr) + 1;
			if (i1) memcpy(work, ptr, (UINT) i1);
			work[i1] = 0;
			fiosetopt(FIO_OPT_PREPPATH, (UCHAR *) work);
		}
	}

	/* get file size, record size and record count */
	rioeofpos(inhndl, &pos);
	if (fixflg) {  /* fixed length records */
		/* Skip over any leading deleted records */
		while ((recsiz = rioget(inhndl, record, RIO_MAX_RECSIZE)) == -2);
		if (recsiz <= -3) death(DEATH_READ, recsiz, NULL);
		if (!reclen) {
			if (recsiz == -1) death(DEATH_NEEDFIX2, 0, NULL);
			reclen = recsiz;
		}
		else if (recsiz != -1 && reclen != recsiz) death(DEATH_RECLENGTH, 0, NULL);
		riosetpos(inhndl, 0L);
		arecsiz = reclen + 1;
		if (riotype(inhndl) == RIO_T_DOS) arecsiz++;
		if (pos % arecsiz) {
			if (riotype(inhndl) != RIO_T_ANY || (pos % (arecsiz + 1))) death(DEATH_RECLENGTH, 0, NULL);
			arecsiz++;
		}
		reccnt = pos / arecsiz;

		rptr = fiogetwptr(inhndl);
		if (rptr == NULL) death(DEATH_INIT, ERR_NOTOP, NULL);
		r = (struct rtab *) *rptr;
		if (r->type != 'R') death(DEATH_INIT, ERR_NOTOP, NULL);
		r->opts |= RIO_FIX | RIO_UNC;
	}
	else reccnt = (pos >> 8) + 1;

	/* calculate the number of records in the primary & secondary extent */
	if (reccnt < 128) reccnt = 128;
	if (addpriflg) reccnt += prirec;
	else if (reccnt < prirec) reccnt = prirec;
	if (reccnt & 0x1F) reccnt = (reccnt & ~0x1F) + 0x20;  /* round up to next even 32 */

	/* initialize aim buffers */
	if (axinit() == -1) death(DEATH_NOMEM, 0, NULL);

	/* build the header block */
	record[0] = 'a';  /* will be 'A' if successful */
	memset(&record[1], 0, 27);
	mscoffto6x(reccnt, &record[13]);  /* current extension record count */
	memset(&record[28], ' ', 72);

	/**
	 * The Z value is stored as a five character string right justified at record[32]
	 */
	msciton(zvalue, &record[32], 5);  /* number of slots (z value) */
	if (fixflg) i1 = reclen;
	else i1 = 256;
	msciton(i1, &record[41], 5);  /* record length */
	if (txtflg) record[55] = 'T';
	if (dstnctflg) record[57] = 'Y';
	else record[57] = 'N';
	record[58] = umatch;
	if (spcflg) record[59] = 'S';
	else if (fixflg) record[59] = 'F';
	else record[59] = 'V';
	mscoffto6x(secrec, &record[60]);  /* cmd line secondary rec count */
	record[99] = '9';
	record[100] = DBCEOR;
	i1 = (INT)strlen(inname);
	memcpy(&record[101], inname, (UINT) i1);
	i1 += 101;
	record[i1++] = DBCEOR;
	memcpy(&record[i1], *arglitptr, (UINT) arghi);
	i1 += arghi;
	memset(&record[i1], DBCDEL, 1024 - i1);
	memfree(arglitptr);

	/* create the aimdex file */
	openflg = FIO_M_PRP | FIO_P_TXT;
	outhndl = fioopen(outname, openflg);
	if (outhndl < 0) death(DEATH_CREATE, outhndl, outname);

	/* write the header */
	i1 = fiowrite(outhndl, 0L, record, 1024);
	if (i1) death(DEATH_WRITE, i1, NULL);

	/* restore the memory pointers */
	keyptr = (struct keydef *) *keyptrptr;
	selchr = *selchrptr;
	selptr = (struct seldef *) *selptrptr;

	/* process the records */
	delflg = FALSE;  /* use to flag first deleted record */
	if (!reclen) reclen = RIO_MAX_RECSIZE;
	for ( ; ; ) {
		recsiz = rioget(inhndl, record, reclen);
		riolastpos(inhndl, &pos);
		if (recsiz < highkey) {
			if (recsiz == -1) {  /* end of file */
				if (eofflg) {
					rioeofpos(inhndl, &eofpos);
					if (pos != eofpos) death(DEATH_BADEOF, 0, NULL);
				}
				break;
			}
			if (recsiz == -2) {  /* deleted record */
				if (!spcflg) continue;
				recnum = pos / arecsiz;
				if (!delflg) {  /* modify header block */
					mscoffto6x(recnum + 1, record);
					i1 = fiowrite(outhndl, 7L, record, 6);
					if (i1) death(DEATH_WRITE, i1, NULL);
					delflg = TRUE;
				}
				i1 = axdel();
				if (i1) death(DEATH_WRITE, i1, NULL);
				continue;
			}
			if (recsiz <= -3) death(DEATH_READ, recsiz, NULL);
			memset(&record[recsiz], ' ', highkey - recsiz);
		}
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
				if (selpos + sellen > recsiz) selflg = 0;
				if (!selflg) continue;
				if (sellen > 1) selcmp = memcmp(&record[selpos], &selchr[i3], sellen);
				else {
					c1 = record[selpos];
					if (seleqlflg & (GREATER | LESS)) selcmp = (INT) c1 - (INT) selchr[i3];
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
			if (!selflg) continue;
		}
		if (fixflg) recnum = pos / arecsiz;
		else if (pos) recnum = (pos - 1) / 256;
		else recnum = 0L;
		if (recnum >= reccnt) death(DEATH_TOOMANYREC, 0, NULL);
		i1 = axrec();
		if (i1) death(DEATH_WRITE, i1, NULL);
	}
	i1 = axend();
	if (i1) death(DEATH_WRITE, i1, NULL);

	/* all done */
	record[0] = 'A';
	i1 = fiowrite(outhndl, 0L, record, 1);
	if (i1) death(DEATH_WRITE, i1, NULL);
	i1 = fioclose(outhndl);
	if (i1) death(DEATH_CLOSE, i1, NULL);
	i1 = rioclose(inhndl);
	if (i1) death(DEATH_CLOSE, i1, NULL);
	if (dspflags & DSPFLAGS_DSPXTRA) {
		mscofftoa(recproc, (CHAR *) record);
		dspstring("\rAimdex complete, ");
		dspstring((CHAR *) record);
		dspstring(" records processed\n");
	}
	else if (dspflags & DSPFLAGS_VERBOSE) dspstring("Aimdex complete\n");
	cfgexit();
	exit(0);
	return(0);
}

/* AXREC */
/* aim index record handling */
static INT axrec()
{
	INT i1, i2, i3, i4;
	UCHAR c1, c2, c3;

	while (recnum >= rechi) {
		i1 = axwrite();
		if (i1) return(i1);
	}

	hashbyte = (INT)((recnum >> 3) - workoff);
	hashbit = (UCHAR)(1 << ((UINT) recnum & 0x07));
	for (i1 = 0; i1 < keycnt; i1++) {
		if (keyptr[i1].xclflg) continue;
		i2 = keyptr[i1].from;
		i3 = i2 + keyptr[i1].len;
		if (!dstnctflg) for (i4 = i2; i4 <= i3; i4++) record[i4] = casemap[record[i4]];
		i3 = keyptr[i1].len;  /* i3 is keylength minus 1 */
		if ((c1 = record[i2]) != ' ') {  /* leftmost character non-blank */
			axhash(i1, 31, c1);
			if (i3 > 1  && ((c2 = record[i2 + 1]) != ' ')) axhash(i1, c1, c2);
		}

		if (i3 > 0 && ((c1 = record[i2 + i3]) != ' ')) {
			axhash(i1, 30, c1);
			if (i3 > 1 && ((c2 = record[i2 + i3 - 1]) != ' ')) axhash(i1, c2, c1);
		}
		if (i3 > 2) {
			i3 += i2 - 1;
			while (i2 < i3) {
				if ((c1 = record[i2++]) == ' ') continue;
				if ((c2 = record[i2]) != ' ') {
					if ((c3 = record[i2 + 1]) != ' ') axhash(c1, c2, c3);
					else i2 += 2;
				}
				else i2++;
			}
		}
	}
	if (dspflags & DSPFLAGS_DSPXTRA) {
		recproc++;
		if (!(recproc & 0x01FF)) {
			mscofftoa(recproc, (CHAR *) record);
			dspchar('\r');
			dspstring((CHAR *) record);
			dspstring(" records processed");
			dspflush();
		}
	}
	return(0);
}

/* AXHASH */
/* hash the three parameters into the correct slot bit */
static void axhash(INT h1, INT h2, INT h3)
{
	UINT i1;
	i1 = ((h1 & 0x1F) << 10) | ((h2 & 0x1F) << 5) | (h3 & 0x1F);
	i1 %= zvalue;
	buffer[i1 * worksiz + hashbyte] |= hashbit;
}

/* AXDEL */
/* add deleted record bit */
static INT axdel()
{
	INT i1;

	while (recnum >= rechi) {
		i1 = axwrite();
		if (i1) return(i1);
	}

	/* set the Z+1 slot on */
	i1 = zvalue * worksiz + (INT)((recnum >> 3) - workoff);
	buffer[i1] |= 1 << ((UINT) recnum & 0x07);
	return(0);
}

/* AXINIT */
/* aim index handling initialization */
static INT axinit()
{
	INT i1 = 0, npass;
	OFFSET memory;

	slots = zvalue + spcflg;
	slotsiz = reccnt >> 3;

#if OS_WIN32
	if (!memsize) memsize = 8 << 20;
#else
	if (!memsize) memsize = 2 << 20;
#endif
	if (memsize < slots) memsize = slots;
	memory = slotsiz * slots;
	if ((OFFSET) memsize > memory) memsize = (INT) memory;
	while (memsize >= slots) {
		npass = (INT)(memory / memsize);
		if (memory % memsize) npass++;
		memsize = (INT)(memory / npass);
		if (memory % npass) memsize++;
		if (memsize % slots) i1 = slots - (memsize % slots);
		else i1 = 0;
		buffer = (UCHAR *) malloc(memsize + i1);
		if (buffer != NULL) break;
		memsize -= slots - i1;
	}
	if (memsize < slots) return(-1);
	memsize += i1;

	worksiz = (INT)(memsize / slots);
	rechi = (OFFSET) worksiz << 3;
	workoff = 0;
	memset(buffer, 0, slots * worksiz);

	if (dspflags & DSPFLAGS_DSPXTRA) {
		npass = (INT)(slotsiz / worksiz);
		if (slotsiz % worksiz) npass++;
		mscitoa(memsize, (CHAR *) record);
		dspstring("Allocated ");
		dspstring((CHAR *) record);
		dspstring(" bytes for buffers, ");
		mscitoa(npass, (CHAR *) record);
		dspstring((CHAR *) record);
		if (npass == 1L) dspstring(" output pass required\n");
		else dspstring(" output passes required\n");
	}
	return(0);
}

/* AXEND */
/* end of processing */
static INT axend()
{
	INT i1;

	while (workoff < slotsiz) {
		i1 = axwrite();
		if (i1) return(i1);
	}
	return(0);
}

/* AXWRITE */
/* write buffers to aim file */
static INT axwrite()
{
	static OFFSET passnum;
	INT i1, i2, bufpos;
	OFFSET pos;
	CHAR work[32];

	if (dspflags & DSPFLAGS_DSPXTRA) {
		passnum++;
		mscofftoa(passnum, work);
		dspstring("\r                           \rOutput pass ");
		dspstring(work);
		dspflush();
	}

	if ((OFFSET) worksiz == slotsiz) {
		i1 = fiowrite(outhndl, 1024, buffer, (INT)(slots * slotsiz));
		if (i1) return(i1);
		workoff = slotsiz;
	}
	else {
		bufpos = 0;
		pos = workoff + 1024;
		for (i1 = 0; i1 < slots; i1++) {
			i2 = fiowrite(outhndl, pos, &buffer[bufpos], worksiz);
			if (i2) return(i2);
			bufpos += worksiz;
			pos += slotsiz;
		}
		workoff += worksiz;
		if (workoff + worksiz > slotsiz) worksiz = (INT)(slotsiz - workoff);
		rechi = (workoff + worksiz) << 3;

		memset(buffer, 0, slots * worksiz);
	}
	return(0);
}

/* AXNEXTPARM */
/* get next command line parameter */
static int axnextparm(char *parm, int size)
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
	if (record[parmptr] == DBCDEL) death(DEATH_BADAIMHDR, 0, NULL);
	*parm = 0;
	parmptr++;
	return(0);
}

/* USAGE */
static void usage()
{
	dspstring("AIMDEX command  " RELEASEPROGRAM RELEASE COPYRIGHT);
	dspchar('\n');
	dspstring("Usage:  aimdex file1 [file2] key-spec [key-spec...] [-A=n] [-CFG=cfgfile] [-D]\n");
	dspstring("               [-E] [-F[=n]] [-J[R]] [-M=c] [-N=[+]n] [-O=optfile] [OR]\n");
	dspstring("               [-Pn[-n]EQc[c...]] [-Pn[-n]NEc[c...]] [-Pn[-n]GTc[c...]]\n");
	dspstring("               [-Pn[-n]GEc[c...]] [-Pn[-n]LTc[c...]] [-Pn[-n]LEc[c...]] [-R]\n");
	dspstring("               [-S] [-T] [-V] [-X[=n]] [-Y] [-Z=n] [-!]\n");
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
