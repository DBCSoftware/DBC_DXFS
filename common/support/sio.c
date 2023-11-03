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
#define INC_TIME
#define INC_ERRNO
#define INC_LIMITS
#define INC_ASSERT
#include "includes.h"
#include "base.h"
#include "fio.h"
#include "sio.h"

#if OS_UNIX
#include <sys/types.h>
#include <unistd.h>
#endif

#define DEATH_INTERRUPT		0
#define DEATH_NOMEM			1
#define DEATH_CREATE		2
#define DEATH_READ			3
#define DEATH_WRITE			4
#define DEATH_COLLATE		5

static char *errormsg[] = {
	"HALTED - user interrupt",
	"Unable to allocate memory for sort buffers",
	"Unable to create work file",
	"Unable to read from work file",
	"Unable to write to work file",
	"Error with collate file, unable to open, wrong size, or unable to read"
};

/*** CODE: THIS IS ONLY SUPPORTING 2G RECORDS, ORDER NEEDS TO BE INCREASED TO OFFSET ***/
#define FLAGBIT (~(UINT) INT_MAX)

typedef struct mrgdef_struct {
	UCHAR *ptr;
	INT bpos;
	INT bsize;
	OFFSET fpos;
	OFFSET fsize;
} MERGEINFO;

#define MERGESIZ (64 << 10)
#define MAXMERGE 34

static UCHAR sioflags;
static CHAR eraseline[] = { "\r                         " };

static UCHAR priority[UCHAR_MAX + 1];	/* holds priority of characters */
static INT count[UCHAR_MAX + 1];		/* count of similar chararacters */

static UCHAR *buffer;			/* sort buffer */
static UINT *order = NULL;		/* order of records */
static UCHAR *numtype;			/* results of numinfo */

static INT keycnt;				/* number of keys */
static INT key;					/* current key */
static INT keypos;				/* current record position of key */
static SIOKEY *keyptr;			/* key structure */
static SIOKEY **keyptrptr;
static UCHAR collateflag;
static INT mergecnt;
static INT sortedflag;
static INT firstgetflag;

static INT maxrec;				/* maximum number of records in buffer */
static INT reclen;				/* length of record */
static INT reccnt;				/* number of records in buffer */
static UCHAR *nextrec, *lastrec;/* pointers into buffer used by sioput */

static INT memsize;				/* memory allocated */
static OFFSET writepos;			/* current position to write to work file */

static UCHAR *mergebuf;
static MERGEINFO *mrginfo;
static INT mergebufsize;

static INT workhandle = -1;		/* work file handle */
static CHAR workdir[MAX_NAMESIZE];
static CHAR workname[MAX_NAMESIZE];

static void (*dspcallback)(CHAR *);

static void process(void);
static void dispadrsort(INT, INT);
static void insertsort(INT, INT);
static INT comprec(UCHAR *, UCHAR *);
static int numinfo(UCHAR *, int);
static int numchar(UCHAR, int);
static INT writeout(void);
static INT bufinit(void);
static INT merge(void);
static void insertmerge(INT);
static void sioputerr(INT, INT, CHAR *);

extern INT fioadlerr;		/* variable to hold delete errors */



INT sioinit(INT len, INT cnt, SIOKEY **keypptr, CHAR *wrkdir, CHAR *wrkname, INT size, INT flags, void (*dspcb)(CHAR *))
{
	CHAR work[32];
	UCHAR **pptr;

	sioflags = 0;
	sioexit();  /* just in case it was not called previously */

	/* make arguments global */
	reclen = len;
	keycnt = cnt;
	keyptrptr = keypptr;
	memsize = size;
	if (wrkdir == NULL) workdir[0] = '\0';
	else strcpy(workdir, wrkdir);
	if (wrkname == NULL) workname[0] = '\0';
	else strcpy(workname, wrkname);
	sioflags = (UCHAR) flags;
	if (dspcb == NULL) sioflags &= ~(SIOFLAGS_DSPPHASE | SIOFLAGS_DSPEXTRA | SIOFLAGS_DSPERROR);
	else dspcallback = dspcb;

	/* initial memory */
	if (bufinit() == RC_NO_MEM) {
		sioputerr(DEATH_NOMEM, 0, NULL);
		return ERR_NOMEM;
	}

	/* other initialization */
	key = 0;
	keypos = (*keyptrptr)[0].start;
	sortedflag = TRUE;
	reccnt = 0;
	nextrec = lastrec = buffer;
	mergecnt = 0;
	firstgetflag = TRUE;
	if ((pptr = fiogetopt(FIO_OPT_COLLATEMAP)) != NULL) {
		memcpy(priority, *pptr, sizeof(priority));
		collateflag = TRUE;
	}
	else collateflag = FALSE;
	memset(count, 0, sizeof(count));

	if (sioflags & SIOFLAGS_DSPEXTRA) {
		mscitoa(memsize, work);
		dspcallback(work);
		dspcallback(" bytes of memory allocated for sort buffers\n");
		dspcallback("Input phase 1");
		dspcallback(NULL);
	}
	else if (sioflags & SIOFLAGS_DSPPHASE) {
		dspcallback("Input phase");
		dspcallback(NULL);
	}

	return 0;
}

INT sioexit()
{
	INT i1;
	CHAR work[48];
	if (sioflags & (SIOFLAGS_DSPPHASE | SIOFLAGS_DSPEXTRA)) dspcallback("\r");
	sioflags = 0;
	if (workhandle > 0) {
		i1 = fiokill(workhandle);
		workhandle = -1;
		if (i1 == ERR_DLERR && (sioflags & SIOFLAGS_DSPEXTRA)) {
			sprintf(work, "\nError deleting work file, errno=%d\n", fioadlerr);
			dspcallback(work);
		}
	}
	if (order != NULL) {
		free(order);
		order = NULL;
	}
	return 0;
}

UCHAR *sioputstart()
{
	return nextrec;
}

INT sioputend()
{
	INT i1;
	CHAR work[32];

	if (sortedflag && keycnt > 0) {
		keyptr = *keyptrptr;  /* restore key pointer */
		if (comprec(nextrec, lastrec) < 0) sortedflag = FALSE;
	}
	order[reccnt] = (UINT) reccnt;
	if (++reccnt == maxrec) {
		if (!sortedflag) {  /* sort records if not already sorted */
			if (sioflags & SIOFLAGS_DSPEXTRA) {
				mscitoa(reccnt, work);
				dspcallback(eraseline);
				dspcallback("\rSorting ");
				dspcallback(work);
				dspcallback(" records");
				dspcallback(NULL);
			}
			keyptr = *keyptrptr;  /* restore key pointer */
			process();
			/* reset key values, changed by process() */
			key = 0;
			keypos = keyptr[0].start;
		}
		i1 = writeout();
		if (i1) return i1;

		/* reset global variables */
		sortedflag = TRUE;
		reccnt = 0;
		nextrec = lastrec = buffer;
		if (sioflags & SIOFLAGS_DSPEXTRA) {
			mscitoa(mergecnt + 1, work);
			dspcallback(eraseline);
			dspcallback("\rInput phase ");
			dspcallback(work);
			dspcallback(NULL);
		}
	}
	else {
		lastrec = nextrec;
		nextrec += reclen;
	}
	return 0;
}

INT sioget(UCHAR **ptr)
{
	static INT numrec;
	INT i1, i2;
	CHAR work[32];
	MERGEINFO *mrgptr;

	if (firstgetflag) {
		keyptr = *keyptrptr;  /* restore key pointer */
		if (reccnt) {
			if (!sortedflag) {  /* sort records if not already sorted */
				if (sioflags & SIOFLAGS_DSPEXTRA) {
					mscitoa(reccnt, work);
					dspcallback(eraseline);
					dspcallback("\rSorting ");
					dspcallback(work);
					dspcallback(" records");
					dspcallback(NULL);
				}
				process();
				/* reset key values, changed by process() */
				key = 0;
				keypos = keyptr[0].start;
			}
			if (mergecnt) {  /* add to work file */
				i1 = writeout();
				if (i1) return i1;
			}
		}
		if (mergecnt) {
			if (mergecnt > 1) {
				i1 = merge();
				if (i1) return i1;
			}
			else {  /* total records = maxrec */
				/* records have been sorted and written out by writeout, but read from memory */
				order[0] = maxrec;  /* first record was saved off */
				for (reccnt = 1; reccnt < maxrec; reccnt++) order[reccnt] = reccnt;
				mergecnt = 0;
			}
		}
		numrec = 0;
		if (sioflags & (SIOFLAGS_DSPPHASE | SIOFLAGS_DSPEXTRA)) {
			if (sioflags & SIOFLAGS_DSPEXTRA) dspcallback(eraseline);
			dspcallback("\rOutput phase");
			dspcallback(NULL);
		}
		firstgetflag = FALSE;
	}

	if (!mergecnt) {
		if (numrec >= reccnt) return 1;
		*ptr = buffer + (order[numrec++] & ~FLAGBIT) * reclen;
		return 0;
	}
	keyptr = *keyptrptr;  /* restore key pointer */
	for ( ; ; ) {
		mrgptr = mrginfo + order[0];
		if (mrgptr->bpos != mrgptr->bsize) break;
		if (!mrgptr->fsize) {
			if (!--mergecnt) return 1;
			memmove(order, order + 1, mergecnt * sizeof(UINT));
			continue;
		}
		i1 = mergebufsize;
		if (mrgptr->fsize < (OFFSET) i1) i1 = (INT) mrgptr->fsize;
		i2 = fioread(workhandle, mrgptr->fpos, mrgptr->ptr, i1);
		if (i2 != i1) {
			if (i2 >= 0) i2 = 0;
			sioputerr(DEATH_READ, i2, NULL);
			if (!i2) i2 = ERR_BADRL;
			return i2;
		}
		mrgptr->bpos = 0;
		mrgptr->bsize = i1;
		mrgptr->fpos += i1;
		mrgptr->fsize -= i1;
		if (mergecnt > 1) insertmerge(mergecnt);
	}
	*ptr = mrgptr->ptr + mrgptr->bpos;
	mrgptr->bpos += reclen;
	if (mrgptr->bpos != mrgptr->bsize && mergecnt > 1) insertmerge(mergecnt);
	return 0;
}

/*** NOTE: SIOSORT IS OBSOLETE, DO NOT USE IN NEW CODE ***/
INT siosort(INT len, INT cnt, SIOKEY **keypptr, INT (*getrtn)(UCHAR *),
		INT (*putrtn)(UCHAR *), CHAR *wrkdir, CHAR *wrkname, INT flags, INT size, void (*dspcb)(CHAR *))
{
	INT i1;
	UCHAR *ptr;

	i1 = sioinit(len, cnt, keypptr, wrkdir, wrkname, size, flags, dspcb);
	if (i1) return i1;

	/* read records */
	while (!(i1 = (*getrtn)(sioputstart()))) {
		i1 = sioputend();
		if (i1) break;
	}
	if (i1 < 0) {
		sioexit();
		return i1;
	}

	/* write records */
	for ( ; ; ) {
		i1 = sioget(&ptr);
		if (i1) break;
		i1 = (*putrtn)(ptr);
		if (i1) break;
	}
	if (i1 < 0) {
		sioexit();
		return i1;
	}

	sioexit();
	return 0;
}

/* PROCESS */
/* initialize sorting fields and choose method of sort */
static void process()
{
	INT i1, i2, flag;

	/* first sort the entire group */
	i1 = reccnt - 1;
	order[i1] = (UINT) i1 | FLAGBIT;
	if (i1 > 48) dispadrsort(0, i1);
	else insertsort(0, i1);
	keypos++;

	for ( ; ; ) {
		while (keypos < keyptr[key].end) {
			/* assume all records are sorted until proven false */
			flag = TRUE;
		
			/* read through the order array finding groups larger than 1 */
			for (i1 = 0; i1 < reccnt; i1++) {
				if (order[i1] & FLAGBIT) continue;
				i2 = i1;
				while (!(order[++i1] & FLAGBIT));
				if (i1 - i2 > 48) dispadrsort(i2, i1);
				else insertsort(i2, i1);
				flag = FALSE;
			}
			if (flag) return;
			keypos++;
		}
		if (++key == keycnt) return;
		keypos = keyptr[key].start;
	}
}

/* DISPADRSORT */
/* displacement address sort for 50 or more records */
static void dispadrsort(INT ilo, INT  ihi)
{
	INT i1, i2 = 0, address, location, nonzero, number, pointer, temp;
	INT zloc[UCHAR_MAX + 1];
	UCHAR typeflag, *ptr;

	assert(ilo < ihi);
	order[ihi] &= ~FLAGBIT;
	typeflag = keyptr[key].typeflg;
	ptr = buffer + keypos;

	if (typeflag & SIO_NUMERIC) {
		if (keypos == keyptr[key].start)  /* get numeric information */
			for (i1 = ilo; i1 <= ihi; i1++)
				numtype[order[i1]] = (UCHAR) numinfo(buffer + order[i1] * reclen, key);
		for (i1 = ilo; i1 <= ihi; i1++) {
			i2 = numchar(ptr[order[i1] * reclen], numtype[order[i1]]);
			if (typeflag & SIO_DESCEND) i2 = UCHAR_MAX - i2;
			count[i2]++;
		}
	}
	else if (!collateflag || (typeflag & SIO_POSITION)) {
		for (i1 = ilo; i1 <= ihi; ) {
			i2 = ptr[order[i1++] * reclen];
			if (typeflag & SIO_DESCEND) i2 = UCHAR_MAX - i2;
			count[i2]++;
		}
	}
	else {
		for (i1 = ilo; i1 <= ihi; ) {
			i2 = priority[ptr[order[i1++] * reclen]];
			if (typeflag & SIO_DESCEND) i2 = UCHAR_MAX - i2;
			count[i2]++;
		}
	}
	if (count[i2] > ihi - ilo) {  /* all were the same */ 
		count[i2] = 0;
		order[ihi] |= FLAGBIT;
		return;
	}

	address = ilo;
	nonzero = 0;
	for (i1 = 0; i1 < UCHAR_MAX + 1; i1++) {
		if (count[i1]) {
			number = count[i1];
			count[i1] = address;
			address += number;
			zloc[nonzero++] = i1;
		}
	}

	if (typeflag & SIO_NUMERIC) {
		for (i1 = ilo; i1 <= ihi; i1++) {
			if (!(order[i1] & FLAGBIT)) {
				pointer = (INT) order[i1];
				order[i1] = FLAGBIT;
				while (!(pointer & FLAGBIT)) {
					location = numchar(ptr[pointer * reclen], numtype[pointer]);
					if (typeflag & SIO_DESCEND) location = UCHAR_MAX - location;
					address = count[location]++;
					temp = (INT) order[address];
					order[address] = (UINT) pointer | FLAGBIT;
					pointer = temp;
				}
			}
		}
	}
	else if (!collateflag || (typeflag & SIO_POSITION)) {
		for (i1 = ilo; i1 <= ihi; i1++) {
			if (!(order[i1] & FLAGBIT)) {
				pointer = (INT) order[i1];
				order[i1] = FLAGBIT;
				while (!(pointer & FLAGBIT)) {
					location = ptr[pointer * reclen];
					if (typeflag & SIO_DESCEND) location = UCHAR_MAX - location;
					address = count[location]++;
					temp = (INT) order[address];
					order[address] = (UINT) pointer | FLAGBIT;
					pointer = temp;
				}
			}
		}
	}
	else {
		for (i1 = ilo; i1 <= ihi; i1++) {
			if (!(order[i1] & FLAGBIT)) {
				pointer = (INT) order[i1];
				order[i1] = FLAGBIT;
				while (!(pointer & FLAGBIT)) {
					location = priority[ptr[pointer * reclen]];
					if (typeflag & SIO_DESCEND) location = UCHAR_MAX - location;
					address = count[location]++;
					temp = (INT) order[address];
					order[address] = (UINT) pointer | FLAGBIT;
					pointer = temp;
				}
			}
		}
	}

	for (i1 = ilo; i1 <= ihi; i1++) order[i1] &= ~FLAGBIT;

	for (i1 = 0; i1 < nonzero; ) {
		i2 = zloc[i1++];
		address = count[i2] - 1;
		count[i2] = 0;
		order[address] |= FLAGBIT;
	}
}

/* INSERTSORT */
/* insertion sort for 49 or less records */
static void insertsort(INT ilo, INT ihi)
{
	INT i1, i2, i3;
	UCHAR *ptr1, *ptr2;

	order[ihi] &= ~FLAGBIT;
	for (i2 = ihi; i2-- > ilo; ) {
		i1 = i2;
		i3 = (INT) order[i2];
		ptr1 = buffer + i3 * reclen;
		while (i1++ < ihi) {
			ptr2 = buffer + order[i1] * reclen;
			if (comprec(ptr1, ptr2) <= 0) break;
			order[i1 - 1] = order[i1];
		}
		order[i1 - 1] = (UINT) i3;
	}
	for (i1 = ilo; i1 <= ihi; i1++) order[i1] |= FLAGBIT;
}

/* COMPREC */
/* compare two records */
static INT comprec(UCHAR *rec1, UCHAR *rec2)
{
	INT i1, i2, i3, i4 = 0, numflag1, numflag2;

	i1 = key;
	i2 = keypos;
	while (TRUE) {
		i3 = keyptr[i1].end;
		if (keyptr[i1].typeflg & SIO_NUMERIC) {
			numflag1 = numinfo(rec1, i1);
			numflag2 = numinfo(rec2, i1);
			for ( ; i2 < i3; i2++) {
				i4 = numchar(rec1[i2], numflag1) - numchar(rec2[i2], numflag2);
				if (i4) break;
			}
			if (i2 < i3) {
				if (keyptr[i1].typeflg & SIO_DESCEND) return -i4;
				return i4;
			}
		}
		else if (!collateflag || (keyptr[i1].typeflg & SIO_POSITION)) {
			i4 = memcmp(rec1 + i2, rec2 + i2, i3 - i2);
			if (i4) {
				if (keyptr[i1].typeflg & SIO_DESCEND) return -i4;
				return i4;
			}
		}
		else {
			while (i2 < i3) {
				if (rec1[i2] != rec2[i2]) {
					i4 = (INT) priority[rec1[i2]] - (INT) priority[rec2[i2]];
					if (i4) {
						if (keyptr[i1].typeflg & SIO_DESCEND) return -i4;
						return i4;
					}
				}
				i2++;
			}
		}
		if (++i1 == keycnt) return 0;
		i2 = keyptr[i1].start;
	}
}

#define NUMFLAG_NULL	0x01
#define NUMFLAG_INVALID	0x15
#define NUMFLAG_NEGATIVE	0x20

static int numinfo(UCHAR *rec, int keynum)
{
	int i1, i2, numflag;

	numflag = NUMFLAG_NULL;
	for (i1 = keyptr[keynum].start, i2 = keyptr[keynum].end; i1 < i2; i1++) {
		if (rec[i1] == ' ') {
			if (numflag != NUMFLAG_NULL) return NUMFLAG_INVALID;
		}
		else if (isdigit(rec[i1])) {
			if (numflag != NUMFLAG_NEGATIVE) numflag = 0;
		}
		else if (rec[i1] == '.') {
			if (numflag != NUMFLAG_NEGATIVE) numflag = 0;
		}
		else if (rec[i1] == '-' || rec[i1] == '+') {
			if (numflag != NUMFLAG_NULL || i1 + 1 == i2) return NUMFLAG_INVALID;
			if (rec[i1] == '-') numflag = NUMFLAG_NEGATIVE;
			else numflag = 0;
		}
		else return NUMFLAG_INVALID;
	}
	return numflag;
}

static int numchar(UCHAR chr, int numflag)
{
	if (numflag & (NUMFLAG_NULL | NUMFLAG_INVALID)) return numflag;

	if (isdigit(chr)) {
		chr -= '0';
		if (numflag & NUMFLAG_NEGATIVE) return 0x0B - chr;
		return 0x0B + chr;
	}
	return 0x0B;
}

#undef NUMFLAG_NULL
#undef NUMFLAG_INVALID
#undef NUMFLAG_NEGATIVE


static INT getTempWorkHandle(CHAR *work) {
#if OS_UNIX && !defined(__linux)
	UCHAR *ptr1;
#endif
#if OS_WIN32
	if (!GetTempFileName(workdir, "srt", 0, work)) {
		sioputerr(DEATH_CREATE, ERR_BADNM, workdir);
		return ERR_BADNM;
	}
	miofixname(work, ".wrk", FIXNAME_EXT_ADD);
	workhandle = fioopen(work, FIO_M_PRP | FIO_P_WRK);
#endif

#if OS_UNIX
#ifdef __linux
	strcpy(work, workdir);
	if (work[strlen(work) - 1] != '/') strcat(work, "/");
	strcat(work, "srtXXXXXX");
	workhandle = mkstemp(work);  // returns an fd
	if (workhandle == -1) {
		sioputerr(DEATH_CREATE, ERR_BADNM, workdir);
		return ERR_BADNM;
	}
	close(workhandle);
	unlink(work);
#else
	ptr1 = (UCHAR *) tempnam(workdir, "srt");
	if (ptr1 == NULL) {
		sioputerr(DEATH_CREATE, ERR_BADNM, workdir);
		return ERR_BADNM;
	}
	strcpy(work, (CHAR *) ptr1);
	free(ptr1);
	miofixname(work, ".wrk", FIXNAME_EXT_ADD);
#endif
	workhandle = fioopen(work, FIO_M_PRP | FIO_P_WRK);
#endif
	return workhandle;
}

/* WRITEOUT */
/* write out the records */
static INT writeout()
{
	INT i1, cnt, tmpcnt, wrkcnt;
	OFFSET posinfo[2];
	CHAR work[MAX_NAMESIZE];
	UCHAR *ptr1, *ptr2, *saveptr;

	if (!mergecnt) {  /* first pass processing */
		/* create work file */
		if (!workdir[0]) strcpy(workdir, ".");
		if (!workname[0]) {
			workhandle = getTempWorkHandle(work);
			if (workhandle == ERR_BADNM) return ERR_BADNM;
		}
		else {
			strcpy(work, workname);
			miofixname(work, "", FIXNAME_PAR_DBCVOL);
			miogetname((CHAR **) &ptr1, (CHAR **) &ptr2);
			if (!*ptr1 && fioaslash(workname) < 0) {
				strcpy(work, workdir);
				fioaslashx(work);
			}
			else work[0] = 0;
			strcat(work, workname);
			miofixname(work, ".wrk", FIXNAME_EXT_ADD);
			workhandle = fioopen(work, FIO_M_PRP | FIO_P_WRK);
		}
		if (workhandle < 0) {
			sioputerr(DEATH_CREATE, workhandle, work);
			return workhandle;
		}
		if (sioflags & SIOFLAGS_DSPEXTRA) {
			dspcallback(eraseline);
			dspcallback("\rCreated work file ");
			dspcallback(work);
			dspcallback("\n");
		}
		writepos = 0;
		keyptr = *keyptrptr;  /* restore key pointer */
	}

	if (!sortedflag) {  /* rearrange buffer into sorted order */
		saveptr = buffer + reccnt * reclen;
		for (cnt = 0; cnt < reccnt; cnt++) {
			if ((INT)(order[cnt] & ~FLAGBIT) == cnt) continue;
			wrkcnt = cnt;
			ptr1 = saveptr;
			do {
				ptr2 = ptr1;
				ptr1 = buffer + wrkcnt * reclen;
				memcpy(ptr2, ptr1, reclen);
				tmpcnt = (INT)(order[wrkcnt] & ~FLAGBIT);
				order[wrkcnt] = (UINT) wrkcnt;
				wrkcnt = tmpcnt;
			} while (wrkcnt != cnt);
			memcpy(ptr1, saveptr, reclen);
		}
	}

	if (sioflags & SIOFLAGS_DSPEXTRA) {
		dspcallback(eraseline);
		dspcallback("\rWriting to work file");
		dspcallback("\n");
	}
	posinfo[1] = (OFFSET) reccnt * reclen;
	posinfo[0] = writepos + sizeof(posinfo) + posinfo[1];
	if (posinfo[0] & 0x0FFF) posinfo[0] = (posinfo[0] & ~0x0FFF) + 0x1000;
	memcpy(buffer - sizeof(posinfo), posinfo, sizeof(posinfo));
	i1 = fiowrite(workhandle, writepos, buffer - sizeof(posinfo), (INT)(sizeof(posinfo) + posinfo[1]));
	if (i1) {
		sioputerr(DEATH_WRITE, i1, NULL);
		return i1;
	}
	writepos = posinfo[0];
	/* if first merge, save off first record to optimize sioget if there are no more sioputend */
	if (++mergecnt == 1) memcpy(buffer + maxrec * reclen, buffer, reclen);
	return 0;
}

/* MERGE */
/* merge the sorted records */
static INT merge()
{
	INT i1, i2, i3, bufcnt, bufsize, mrgflag;
	INT lwork, cnt, passcnt, passnum, savecnt;
	OFFSET firstpos, filesize, lastpos, mergesize, nextpos, savesize, posinfo[2];
	CHAR work[17];
	UCHAR *ptr;
	MERGEINFO *mrgptr;

	if ((sioflags & (SIOFLAGS_DSPPHASE | SIOFLAGS_DSPEXTRA)) == SIOFLAGS_DSPPHASE) {
		dspcallback("\rMerge phase");
		dspcallback(NULL);
	}
	firstpos = 0;
	filesize = writepos;
	for (cnt = 1; ; cnt++) {
		/* initialize merge buffers */
		i1 = (memsize - 0x10) / (reclen + sizeof(MERGEINFO) + sizeof(UINT));
		if (i1 < mergecnt || mergecnt > MAXMERGE) i1 = (memsize - MERGESIZ - 0x10) / (reclen + sizeof(MERGEINFO) + sizeof(UINT));
		if (i1 > mergecnt) i1 = mergecnt;
		if (i1 > MAXMERGE) passcnt = MAXMERGE;
		else passcnt = i1;
		mrgflag = 0;
		if (passcnt < mergecnt) {
			while ((passcnt - 1) * (passcnt - 1) >= mergecnt) passcnt--;
			mrgflag = 1;
		}
		i2 = passcnt * sizeof(UINT);
		if (i2 & 0x0F) i2 = (i2 & ~0x0F) + 0x10;  /* align mrginfo on 16 byte boundary */
		mrginfo = (MERGEINFO *)((UCHAR *) order + i2);
		i2 += passcnt * sizeof(MERGEINFO);
		if (mrgflag) {
			mergebuf = (UCHAR *) order + i2;
			i2 += MERGESIZ;
			bufsize = MERGESIZ;
		}
		mergebufsize = (memsize - i2) / passcnt;
		mergebufsize -= mergebufsize % reclen;
		i1 = memsize;
		for (i3 = 0; i3 < passcnt; i3++) {
			i1 -= mergebufsize;
			mrginfo[i3].ptr = (UCHAR *) order + i1;
		}
		if (mrgflag) {
			/* try to increase merge buffer with left over memory */
			bufsize += (UINT)((i1 - i2) & ~0x0FFF);
			if (sioflags & SIOFLAGS_DSPEXTRA) {
				mscitoa(cnt, work);
				dspcallback(eraseline);
				dspcallback("\rMerge phase ");
				dspcallback(work);
				dspcallback(NULL);
			}
		}

		/* merge pass */
		savecnt = passcnt;
		nextpos = firstpos;
		writepos = filesize;
		for (lwork = mergecnt; lwork > mrgflag; lwork -= passcnt) {
			if (passcnt > lwork) passcnt = lwork;

			/* fill merge buffers */
			mergesize = 0;
			for (passnum = 0; passnum < passcnt; passnum++) {
				i1 = fioread(workhandle, nextpos, (UCHAR *) posinfo, sizeof(posinfo));
				if (i1 != sizeof(posinfo)) {
					if (i1 >= 0) i1 = 0;
					sioputerr(DEATH_READ, i1, NULL);
					if (!i1) i1 = ERR_BADRL;
					return i1;
				}
				mrgptr = mrginfo + passnum;
				mrgptr->fpos = nextpos + sizeof(posinfo);
				mrgptr->fsize = posinfo[1];
				mergesize += posinfo[1];
				nextpos = posinfo[0];
				i1 = mergebufsize;
				if (posinfo[1] < (OFFSET) i1) i1 = (INT) posinfo[1];
				i2 = fioread(workhandle, mrgptr->fpos, mrgptr->ptr, i1);
				if (i2 != i1) {
					if (i2 >= 0) i2 = 0;
					sioputerr(DEATH_READ, i2, NULL);
					if (!i2) i2 = ERR_BADRL;
					return i2;
				}
				mrgptr->bpos = 0;
				mrgptr->bsize = i1;
				mrgptr->fpos += i1;
				mrgptr->fsize -= i1;
				for (i2 = 0; i2 < passnum; i2++) if (comprec(mrgptr->ptr, mrginfo[order[i2]].ptr) < 0) break;
				if (i2 < passnum) memmove(order + i2 + 1, order + i2, (passnum - i2) * sizeof(UINT));
				order[i2] = (INT) passnum;
			}
			if (!mrgflag) return 0;

			/* write header information for merge block */
			if (passcnt + 1 < lwork) {  /* not last merge block */
				posinfo[0] = writepos + sizeof(posinfo) + mergesize;
				if (posinfo[0] & 0x0FFF) posinfo[0] = (posinfo[0] & ~0x0FFF) + 0x1000;
				if (lwork == mergecnt) {  /* first merge block */
					firstpos = 0;
					savesize = sizeof(posinfo) + mergesize;
				}
			}
			else if (lwork == mergecnt) {  /* both first and last merge block */
				posinfo[0] = nextpos;
				firstpos = writepos;
			}
			else {  /* last merge block */
				posinfo[0] = filesize;
				lastpos = writepos;
			}
			posinfo[1] = mergesize;
			memcpy(mergebuf, posinfo, sizeof(posinfo));
			bufcnt = sizeof(posinfo);

			/* merge the records */
			while (passnum) {
				mrgptr = mrginfo + order[0];
				ptr = mrgptr->ptr + mrgptr->bpos;
				i2 = reclen;
				do {
					i3 = bufsize - bufcnt;
					if (i2 < i3) i3 = i2;
					memcpy(mergebuf + bufcnt, ptr, i3);
					bufcnt += i3;
					if (bufcnt != bufsize) break;
					i1 = fiowrite(workhandle, writepos, mergebuf, bufcnt);
					if (i1) {
						sioputerr(DEATH_WRITE, i1, NULL);
						return i1;
					}
					writepos += bufcnt;
					bufcnt = 0;
					ptr += i3;
					i2 -= i3;
				} while (i2);
				mrgptr->bpos += reclen;
				if (mrgptr->bpos == mrgptr->bsize) {
					if (!mrgptr->fsize) {
						if (--passnum) memmove(order, order + 1, passnum * sizeof(UINT));
						continue;
					}
					i1 = mergebufsize;
					if (mrgptr->fsize < (OFFSET) i1) i1 = (INT) mrgptr->fsize;
					i2 = fioread(workhandle, mrgptr->fpos, mrgptr->ptr, i1);
					if (i2 != i1) {
						if (i2 >= 0) i2 = 0;
						sioputerr(DEATH_READ, i2, NULL);
						if (!i2) i2 = ERR_BADRL;
						return i2;
					}
					mrgptr->bpos = 0;
					mrgptr->bsize = i1;
					mrgptr->fpos += i1;
					mrgptr->fsize -= i1;
				}
				if (passnum > 1) insertmerge(passnum);
			}

			/* flush merge buffer */
			if (bufcnt) {
				i1 = fiowrite(workhandle, writepos, mergebuf, bufcnt);
				if (i1) {
					sioputerr(DEATH_WRITE, i1, NULL);
					return i1;
				}
				writepos += bufcnt;
			}
			if (lwork == mergecnt) writepos = 0;
			else if (writepos & 0x0FFF) writepos = (writepos & ~0x0FFF) + 0x1000;

			mergecnt -= passcnt - 1;
		}

		/* need to point to last merge block that was not merged */
		if (lwork == 1) {
			posinfo[0] = nextpos;
			i1 = fiowrite(workhandle, filesize, (UCHAR *) posinfo, sizeof(posinfo[0]));
			if (i1) {
				sioputerr(DEATH_WRITE, i1, NULL);
				return i1;
			}
		}

		/* move the first merge block before the original end of file */
		if (savecnt < mergecnt) {  /* another merge phase will follow */
			/* modify last block written to point to new position, not old eof */
			posinfo[0] = writepos;
			i1 = fiowrite(workhandle, lastpos, (UCHAR *) posinfo, sizeof(posinfo[0]));
			if (i1) {
				sioputerr(DEATH_WRITE, i1, NULL);
				return i1;
			}

			/* move block */
			ptr = (UCHAR *) order;
			for (nextpos = 0; nextpos < savesize; nextpos += i2) {
				if ((OFFSET) memsize < savesize - nextpos) i2 = memsize;
				else i2 = (INT)(savesize - nextpos);
				i3 = fioread(workhandle, filesize + nextpos, ptr, i2);
				if (i3 != i2) {
					if (i3 >= 0) i3 = 0;
					sioputerr(DEATH_READ, i3, NULL);
					if (!i3) i3 = ERR_BADRL;
					return i3;
				}
				i1 = fiowrite(workhandle, writepos, ptr, i2);
				if (i1) {
					sioputerr(DEATH_WRITE, i1, NULL);
					return i1;
				}
				writepos += i2;
			}
			if (!lwork) {
				filesize = writepos;
				if (filesize & 0x0FFF) filesize = (filesize & ~0x0FFF) + 0x1000;
			}
		}
	}
}

/* INSERTMERGE */
static void insertmerge(INT size)
{
	INT i1, i2, start, end;
	UCHAR *ptr1, *ptr2;
	MERGEINFO *mrgptr;

	i2 = (INT) order[0];
	mrgptr = mrginfo + i2;
	ptr1 = mrgptr->ptr + mrgptr->bpos;

	i1 = start = 1;
	end = size - 1;
	while (TRUE) {
		mrgptr = mrginfo + order[i1];
		ptr2 = mrgptr->ptr + mrgptr->bpos;
		if (comprec(ptr1, ptr2) < 0) {
			if (i1-- == start) break;
			end = i1;
		}
		else {
			if (i1 == end) break;
			start = i1 + 1;
		}
		i1 = (start + end) >> 1;
	}
	if (i1) {
		memmove(order, order + 1, i1 * sizeof(UINT));
		order[i1] = (UINT) i2;
	}
}

/* BUFINIT */
/* initialize buffer and limits */
static INT bufinit()
{
	INT i1, i2, size;
	UCHAR *ptr = NULL;

	size = sizeof(UINT);
	for (i1 = 0; i1 < keycnt && !((*keyptrptr)[i1].typeflg & SIO_NUMERIC); i1++);
	if (i1 < keycnt) size += sizeof(UCHAR);

	i1 = reclen + size;
	i2 = ((reclen + sizeof(MERGEINFO) + size) << 1) + MERGESIZ;

	if (!memsize) memsize = 2 << 20;
	if (memsize < i2) memsize = i2;
	while (memsize >= i2) {
		ptr = (UCHAR *) malloc(memsize);
		if (ptr != NULL) break;
		memsize -= 8 << 10;
	}
	if (memsize < i2) {
		if (ptr != NULL) free(ptr);
		return RC_NO_MEM;
	}
	memsize -= 4;  /* reserve 4 bytes for overflow by rioget & rioput */
	order = (UINT *) ptr;
	maxrec = ((memsize - (sizeof(OFFSET) << 1)) / i1) - 1;
	numtype = ptr + maxrec * sizeof(UINT);
	buffer = ptr + maxrec * size + (sizeof(OFFSET) << 1);

	return 0;
}

static char sioerrorstring[256];

CHAR *siogeterr()
{
	return sioerrorstring;
}

static void sioputerr(INT n, INT e, CHAR *s)
{
	CHAR work[17];

	if (n < (INT) (sizeof(errormsg) / sizeof(*errormsg))) strcpy(sioerrorstring, errormsg[n]);
	else {
		mscitoa(n, work);
		strcpy(sioerrorstring, "*** UNKNOWN ERROR ");
		strcat(sioerrorstring, work);
		strcat(sioerrorstring, " ***");
	}
	if (e) {
		strcat(sioerrorstring, ": ");
		strcat(sioerrorstring, fioerrstr(e));
	}
	if (s != NULL) {
		strcat(sioerrorstring, ": ");
		strcat(sioerrorstring, s);
	}
	if (sioflags & SIOFLAGS_DSPERROR) {
		if (sioflags & (SIOFLAGS_DSPPHASE | SIOFLAGS_DSPEXTRA)) dspcallback("\r");
		dspcallback(sioerrorstring);
		dspcallback("\n");
	}
	sioexit();
}
