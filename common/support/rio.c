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
#include "includes.h"
#include "base.h"
#include "fio.h"
#if OS_UNIX
#include <unistd.h>
#endif

#define DBCSPC 0xF9
#define DBCEOR 0xFA
#define DBCEOF 0xFB
#define DBCDEL 0xFF

#define NATEOR 0x0A
#define NATDEL 0x7F

#define DBCWEOF 0x80000000

/* local declarations */
static struct rtab *r;	/* working pointer to rtab */
static UCHAR **riobuf;	/* rio work buffer */
static INT riobufsiz;	/* rio work buffer size */
static INT nocompressflag;  /* if TRUE, then no digit compression */
static INT randomwritelockflag; /* if TRUE, then protect random writes with a file lock */
static UCHAR dig1map[121] = {
	'0','0','0','0','0','0','0','0','0','0','0',
	'1','1','1','1','1','1','1','1','1','1','1',
	'2','2','2','2','2','2','2','2','2','2','2',
	'3','3','3','3','3','3','3','3','3','3','3',
	'4','4','4','4','4','4','4','4','4','4','4',
	'5','5','5','5','5','5','5','5','5','5','5',
	'6','6','6','6','6','6','6','6','6','6','6',
	'7','7','7','7','7','7','7','7','7','7','7',
	'8','8','8','8','8','8','8','8','8','8','8',
	'9','9','9','9','9','9','9','9','9','9','9',
	'.','.','.','.','.','.','.','.','.','.','.',
} ;
static UCHAR dig2map[121] = {
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
};
static FHANDLE loghandle;
static UCHAR logbuffer[8192];
static INT logbufcnt;
static CHAR logconnect[32];
static CHAR username[64];
static INT logconnectlen;
static INT loggingflags;

/* local routine declarations */
static INT riocomp(UCHAR *, INT);
static INT rioxgb(INT, UCHAR **, INT, INT, INT);
static INT rioxwb(INT);
static INT logput(CHAR *str, INT len);
static INT logputdata(UCHAR *str, INT len);
static INT logputtimestamp(void);
static INT logputusername(void);
static INT logflush(void);

INT riologstart(CHAR *logfile, CHAR *user, CHAR *database, INT flags)
{
	INT i1;
	strcpy(username, user);
	i1 = fioaopen(logfile, FIO_M_SHR, 0, &loghandle);
	if (i1) return i1;
	i1 = fioalock(loghandle, FIOA_FLLCK | FIOA_WRLCK, 0, 120);
	if (i1) {
		fioaclose(loghandle);
		loghandle = 0;
		return i1;
	}
	fioalseek(loghandle, 0, 2, NULL);
	logbufcnt = 0;
	logput("<connect><c>", 12);
#if OS_WIN32
	logconnectlen = mscitoa((INT) GetCurrentProcessId(), logconnect);
#endif
#if OS_UNIX
	logconnectlen = mscitoa((INT) getpid(), logconnect);
#endif
	logput(logconnect, logconnectlen);
	logput("</c><user>", 10);
	logputdata((UCHAR *) user, -1);
	logput("</user><database>", 17);
	logputdata((UCHAR *) database, -1);
	logput("</database></connect>", 21);
	i1 = logflush();
	fioalock(loghandle, FIOA_FLLCK | FIOA_UNLCK, 0, 0);
	if (i1) {
		fioaclose(loghandle);
		loghandle = 0;
	}
	logconnect[logconnectlen++] = '.';
	loggingflags = flags;
	return i1;
}

INT riologend()
{
	INT i1;

	if (loghandle <= 0) return 0;
	i1 = fioalock(loghandle, FIOA_FLLCK | FIOA_WRLCK, 0, 120);
	if (i1) {
		fioaclose(loghandle);
		loghandle = 0;
		return i1;
	}
	fioalseek(loghandle, 0, 2, NULL);
	logbufcnt = 0;
	logput("<disconnect><c>", 15);
	logput(logconnect, logconnectlen - 1);
	logput("</c></disconnect>", 17);
	i1 = logflush();
	fioalock(loghandle, FIOA_FLLCK | FIOA_UNLCK, 0, 0);
	fioaclose(loghandle);
	loghandle = 0;
	return i1;
}

/*
 * RIOOPEN
 * open name for record i/o processing
 * return positive integer file number if successful, else return error
 */
INT rioopen(CHAR *name, INT opts, INT bufs, INT maxlen)
{
	static INT firstflg = TRUE;

	INT i1, bsiz, fnum, type;
	CHAR filename[MAX_NAMESIZE + 1];
	UCHAR c1, **bptr, **rptr;

	/* on first open, set compression flag */
	if (firstflg) {
		if (fiogetflags() & FIO_FLAG_NOCOMPRESS) nocompressflag = TRUE;
		if (fiogetflags() & FIO_FLAG_RANDWRTLCK) randomwritelockflag = TRUE;
		firstflg = FALSE;
	}

	if (maxlen + 4 > riobufsiz) {
		if (maxlen > RIO_MAX_RECSIZE) return(ERR_INVAR);
		maxlen += 4;
		if (maxlen & 0x03FF) maxlen = (maxlen & ~0x03FF) + 0x0400;
		if (!riobufsiz) {
			riobuf = (UCHAR **) memalloc(maxlen, 0);
			if (riobuf == NULL) return(ERR_NOMEM);
		}
		else if (memchange(riobuf, maxlen, 0)) return(ERR_NOMEM);
		riobufsiz = maxlen;
	}

	if ((opts & RIO_T_MASK) < RIO_T_STD || (opts & RIO_T_MASK) > RIO_T_ANY || bufs < 0 || (opts & DBCWEOF)) return(ERR_INVAR);

	/* open the file with txt extension */
	strncpy(filename, name, sizeof(filename) - 1);
	filename[sizeof(filename) - 1] = '\0';
	if (!(opts & RIO_NOX)) miofixname(filename, ".txt", FIXNAME_EXT_ADD);
	fnum = fioopen(filename, opts);
	if (fnum < 0) {
		return(fnum);
	}

	/* allocate rtab structure and buffer */
	rptr = memalloc(sizeof(struct rtab), MEMFLAGS_ZEROFILL);
	if (rptr == NULL) {
		fioclose(fnum);
		return(ERR_NOMEM);
	}
	bsiz = 0;
	bptr = NULL;
	while (bufs) {
		if (bufs < 8) bsiz = 256 << bufs;  /* formula works for bufs = 1 thru 7 */
		else bsiz = 48 << 10;
		bptr = memalloc(bsiz + 2, 0);
		if (bptr != NULL) break;
		bufs--;
	}

	r = (struct rtab *) *rptr;
	r->bsiz = bsiz;
	r->bptr = bptr;
	r->eofc = 0;
	i1 = fiosetwptr(fnum, rptr);
	if (i1) {
		memfree(bptr);
		memfree(rptr);
		fioclose(fnum);
		return(i1);
	}

	if ((opts & RIO_T_MASK) == RIO_T_BIN) {
		opts |= RIO_UNC;
		r->opts = (UINT) opts;
		fiogetsize(fnum, &r->fsiz);
		r->type = 'R';
		return(fnum);
	}

	/* handle prepare and not prepare initialization */
	i1 = fioflck(fnum);
	if (i1) {
		memfree(bptr);
		memfree(rptr);
		fioclose(fnum);
		return(i1);
	}
	if ( (i1 = fiogetsize(fnum, &r->fsiz)) ) /* deliberate assignment and test */ {
		goto rioope1;
	}

	if ((opts & RIO_M_MASK) < RIO_M_PRP) {  /* not prepare, check file type */
		type = 0;
		if (r->fsiz) {
			if (fioread(fnum, r->fsiz - 1, &c1, 1) != 1) goto rioope0;
			if (c1 == DBCEOF) {
				type = RIO_T_STD;
				r->eofc = DBCEOF;
			}
			else if (c1 == NATEOR) {
				type = RIO_T_DAT;
#if OS_WIN32
				if ((opts & RIO_T_MASK) != RIO_T_DAT && r->fsiz > 1) {
#else
				if ((opts & RIO_T_MASK) != RIO_T_DAT && (opts & RIO_T_MASK) != RIO_T_TXT && r->fsiz > 1) {
#endif
					if (fioread(fnum, r->fsiz - 2, &c1, 1) != 1) goto rioope0;
					if (c1 == 0x0D) type = RIO_T_DOS;
				}
			}
			else if (c1 == 0x0D) type = RIO_T_MAC;
			else if (c1 == NATDEL) type = RIO_T_ANY;
			else if (c1 == 0x1A) {
				type = RIO_T_DOS;
				r->eofc = 0x1A;
			}
		}
		else type = RIO_T_ANY;

		if ((opts & RIO_T_MASK) == RIO_T_STD) {
			if (type != RIO_T_STD) type = 0;
		}
		else if ((opts & RIO_T_MASK) == RIO_T_TXT) {
#if OS_WIN32
			if (type != RIO_T_DOS && type != RIO_T_ANY) type = 0;
			else type = RIO_T_DOS;
#else
			if (type != RIO_T_DAT && type != RIO_T_ANY) type = 0;
			else type = RIO_T_DAT;
#endif
		}
		else if ((opts & RIO_T_MASK) == RIO_T_DAT) {
			if (type != RIO_T_DAT && type != RIO_T_ANY) type = 0;
			else type = RIO_T_DAT;
		}
		else if ((opts & RIO_T_MASK) == RIO_T_DOS) {
			if (type != RIO_T_DOS && type != RIO_T_ANY) type = 0;
			else type = RIO_T_DOS;
		}
		else if ((opts & RIO_T_MASK) == RIO_T_MAC) {
			if (type != RIO_T_MAC && type != RIO_T_ANY) type = 0;
			else type = RIO_T_MAC;
		}
		else if (type == RIO_T_ANY && !r->fsiz)
#if OS_WIN32
			type = RIO_T_DOS;
#else
			type = RIO_T_DAT;
#endif
		if (!type) {
			i1 = ERR_BADTP;
			goto rioope1;
		}
		opts &= ~RIO_T_MASK;
		opts |= type;
	}
	else {  /* it was a prepare */
		if ((opts & RIO_T_MASK) == RIO_T_TXT || (opts & RIO_T_MASK) == RIO_T_ANY) {  /* prepare and type not specified is an error */
			opts &= ~RIO_T_MASK;
#if OS_WIN32
			opts |= RIO_T_DOS;
#else
			opts |= RIO_T_DAT;
#endif
		}
		else if ((opts & RIO_T_MASK) == RIO_T_STD) {  /* write eof */
			r->eofc = DBCEOF;
			i1 = fiowrite(fnum, 0L, &r->eofc, 1);
			if (i1) goto rioope1;
			r->fsiz = 1;
		}
	}
	fiofulk(fnum);

	if ((opts & RIO_LOG) && loghandle > 0 && (loggingflags & RIO_L_OPN)) {
		/* write open to log file */
		i1 = fioalock(loghandle, FIOA_FLLCK | FIOA_WRLCK, 0, 120);
		if (i1) {
			memfree(r->bptr);
			memfree(rptr);
			fioclose(fnum);
			return i1;
		}
		fioalseek(loghandle, 0, 2, NULL);
		logbufcnt = 0;
		logput("<open><c>", 9);
		logput(logconnect, logconnectlen - 1);
		logput("</c><f>", 7);
		i1 = mscitoa(fnum, logconnect + logconnectlen);
		logput(logconnect, logconnectlen + i1);
		logput("</f><name>", 10);
		logputdata((UCHAR *) filename, -1);
		logput("</name>", 7);
		if (loggingflags & RIO_L_USR) logputusername();
		if (loggingflags & RIO_L_TIM) logputtimestamp();
		logput("</open>", 7);
		i1 = logflush();
		fioalock(loghandle, FIOA_FLLCK | FIOA_UNLCK, 0, 0);
		if (i1) {
			memfree(r->bptr);
			memfree(rptr);
			fioclose(fnum);
			return i1;
		}
	}

	if ((opts & RIO_T_MASK) != RIO_T_STD) opts |= RIO_UNC;
	r->opts = (UINT) opts;
	r->type = 'R';
	return(fnum);

	/* error returns */
rioope0:
	i1 = ERR_RDERR;
rioope1:
	fiofulk(fnum);
	memfree(bptr);
	memfree(rptr);
	fioclose(fnum);
	return(i1);
}

static INT logputtimestamp() {
	UCHAR work[16];
	logput("<stamp>", 7);
	msctimestamp(work);
	logput((CHAR *) work, 16);
	return logput("</stamp>", 8);
}

static INT logputfilename(UCHAR *work) {
	if (work != NULL && work[0] >= ' ') {
		logput("<name>", 6);
		logputdata(work, -1);
		logput("</name>", 7);
	}
	return 0;
}

static INT logputusername() {
	logput("<user>", 6);
	logput(username, -1);
	return logput("</user>", 7);
}

/* RIOCLOSE */
/* record i/o close */
INT rioclose(INT fnum)
{
	INT i1, i2;
	CHAR *ptr, work[MAX_NAMESIZE];
	UCHAR **rptr;

	rptr = fiogetwptr(fnum);
	if (rptr == NULL) return(ERR_NOTOP);
	r = (struct rtab *) *rptr;
	if (r->type != 'R') return(ERR_NOTOP);

	if (r->bflg == 2) i1 = rioxwb(fnum);
	else i1 = 0;
	memfree(r->bptr);
	memfree(rptr);
	work[0] = '\0';
	if ((r->opts & RIO_LOG) && loghandle > 0 && (loggingflags & RIO_L_NAM)) {
		ptr = fioname(fnum);
		if (ptr != NULL) strcpy(work, ptr);
	}
	i2 = fioclose(fnum);
	if (!i1) i1 = i2;
	if ((r->opts & RIO_LOG) && loghandle > 0 && (loggingflags & RIO_L_CLS)) {
		/* write close to log file */
		i2 = fioalock(loghandle, FIOA_FLLCK | FIOA_WRLCK, 0, 120);
		if (i2) return i2;
		fioalseek(loghandle, 0, 2, NULL);
		logbufcnt = 0;
		logput("<close><f>", 10);
		i2 = mscitoa(fnum, logconnect + logconnectlen);
		logput(logconnect, logconnectlen + i2);
		logput("</f>", 4);
		if (loggingflags & RIO_L_NAM) logputfilename((UCHAR *)work);
		if (loggingflags & RIO_L_USR) logputusername();
		if (loggingflags & RIO_L_TIM) logputtimestamp();
		logput("</close>", 8);
		i2 = logflush();
		fioalock(loghandle, FIOA_FLLCK | FIOA_UNLCK, 0, 0);
		if (i2) return i2;
	}
	return(i1);
}

/* RIOKILL */
/* record i/o close and delete */
INT riokill(INT fnum)
{
	UCHAR **rptr;

	rptr = fiogetwptr(fnum);
	if (rptr == NULL) return(ERR_NOTOP);
	r = (struct rtab *) *rptr;
	if (r->type != 'R') return(ERR_NOTOP);

	memfree(r->bptr);
	memfree(rptr);
	return(fiokill(fnum));
}

/**
 *  RIOGET
 *
 * Get a record
 * return record length, eof indicator (-1), deleted record (-2) or error
 */
INT rioget(INT fnum, UCHAR *record, INT recsize)
{
	INT i1, i2, i3, i4, i5, recpos, eorsize;
	INT typeflg;
	OFFSET eofpos;
	UCHAR c1, eorchr, delchr, spaceflg, save2[] = {0, 0}; //[2];
	UCHAR *p, *psave, *riowork, **rptr;

	if (recsize + 4 > riobufsiz) return(ERR_INVAR);

	/* initialize structure pointers */
	rptr = fiogetwptr(fnum);
	if (rptr == NULL) return(ERR_NOTOP);
	r = (struct rtab *) *rptr;
	if (r->type != 'R') return(ERR_NOTOP);

	/* flush write buffers */
	if (r->bflg == 2) {
		i1 = rioxwb(fnum);
		if (i1) return(i1);
	}

	/* other initializations */
	eorsize = 1;
	typeflg = r->opts & RIO_T_MASK;
	if (typeflg == RIO_T_STD) {
		eorchr = DBCEOR;
		delchr = DBCDEL;
	}
	else if (typeflg != RIO_T_BIN) {
		if (typeflg == RIO_T_DOS || typeflg == RIO_T_ANY) eorsize = 2;
		if (typeflg == RIO_T_MAC) eorchr = 0x0D;
		else eorchr = NATEOR;
		delchr = NATDEL;
	}
	else eorsize = 0;

	/* fill read buffer or read record */
	i5 = recsize + eorsize;
	if (r->bsiz) {
		i2 = rioxgb(fnum, &p, i5, 0, TRUE);
		psave = p;
	}
	else {
		save2[0] = record[recsize];
		save2[1] = record[recsize + 1];
		i2 = fioread(fnum, r->npos, record, i5);
		psave = p = record;
	}
	if (i2 < 1) {  /* error or no characters */
		if (i2 < 0) return(i2);
		/* verify end of file */
		if (!r->eofc) {
			fiogetsize(fnum, &eofpos);
			if (r->npos <= eofpos) {
				r->lpos = r->npos;
				return(-1);
			}
		}
		return(ERR_RANGE);
	}

	/* check for deleted record */
	if (typeflg != RIO_T_BIN && *p == delchr) {
		if (typeflg != RIO_T_ANY && (r->opts & (RIO_FIX | RIO_UNC)) == (RIO_FIX | RIO_UNC) && (r->npos % i5)) {
			i5 -= (INT)(r->npos % i5);
			if (i2 > i5) i2 = i5;
		}
		i1 = 0;
		for ( ; ; ) {
			c1 = *(psave + i2);
			*(psave + i2) = 0x00;
			if (typeflg == RIO_T_STD) while (*p++ == DBCDEL);
			else while (*p++ == NATDEL);
			*(psave + i2) = c1;

			/* break if another read buffer is not needed */
			i3 = (INT)(--p - psave);
			if (i3 < i2 || !r->bsiz || i1 + i2 == i5) break;

			/* fill read buffer */
			i1 += i2;
			i2 = rioxgb(fnum, &p, i5 - i1, i1, TRUE);
			if (i2 < 1) {  /* error or no characters */
				if (i2 < 0) goto rioget1;
				if (r->eofc) {
					i2 = ERR_NOEOF;
					goto rioget1;
				}
				i3 = 0;
				break;
			}
			psave = p;
		}
		i1 += i3;

		/* check if fix length is correct */
		if (typeflg != RIO_T_ANY && (r->opts & (RIO_FIX | RIO_UNC)) == (RIO_FIX | RIO_UNC) && i1 != i5) {
			i2 = ERR_SHORT;
			goto rioget1;
		}

		/* restore record buffer from overflow */
		if (!r->bsiz) {
			record[recsize] = save2[0];
			record[recsize + 1] = save2[1];
		}

		/* set last and next position */
		r->lpos = r->npos;
		r->npos += i1;
		return(-2);
	}

	/* check for end of file */
	if (r->eofc && *p == r->eofc) {
		/* restore record buffer from overflow */
		if (!r->bsiz) {
			record[recsize] = save2[0];
			record[recsize + 1] = save2[1];
		}

		/* set last position */
		r->lpos = r->npos;
		return(-1);
	}

	/* parse through record */
	i1 = 0;
	for ( ; ; ) {
		if (typeflg != RIO_T_BIN) {
			c1 = *(psave + i2);
			if (typeflg == RIO_T_STD) {
				*(psave + i2) = DBCDEL;
				if (!nocompressflag) while (*p++ < 0x80);
				else while (*p++ < DBCSPC);
			}
			else {
				*(psave + i2) = 0x00;
				while (*p > 0x1A && *p != NATDEL) p++;
				p++;
			}
			*(psave + i2) = c1;
			i3 = (INT)(--p - psave);
		}
		else i3 = i2;

		if (i3 < i2) {
			c1 = *p;
			if (typeflg == RIO_T_STD) {
				if (c1 <= DBCSPC && !(r->opts & RIO_UNC)) {
					i2 -= i3;
					if (!r->bsiz) {
						riowork = *riobuf;
						if (i2) memcpy(riowork, p, i2);
						p = riowork;
					}
					else if (i3) memcpy(&record[i1], psave, i3);
					i1 += i3;
					goto rioget2;
				}
			}
			else if (c1 != eorchr && c1 != NATDEL && c1 != 0x0D && c1 != 0x1A) {
				p++;
				continue;
			}
			else if (c1 == 0x0D && typeflg == RIO_T_DAT) {
				p++;
				continue;
			}
			else if (c1 == 0x1A && typeflg != RIO_T_DOS && typeflg != RIO_T_ANY) {
				p++;
				continue;
			}
			break;
		}
		if (!r->bsiz || i1 + i2 == i5) break;

		/* fill read buffer */
		memcpy(&record[i1], psave, i2);
		i1 += i2;
		i2 = rioxgb(fnum, &p, i5 - i1, i1, TRUE);
		if (i2 < 1) {  /* error or no characters */
			if (i2 < 0) goto rioget1;
			if (typeflg == RIO_T_BIN) {
				i3 = 0;
				break;
			}
			i2 = ERR_NOEOF;
			goto rioget1;
		}
		psave = p;
	}
	if (r->bsiz && i3) memcpy(&record[i1], psave, i3);
	i1 += i3;

	if (i1 > recsize) {
		i2 = ERR_NOEOR;
		goto rioget1;
	}

	if (typeflg != RIO_T_BIN) {
		if (i2 == i3) {
			i2 = ERR_NOEOF;
			goto rioget1;
		}
		i1++;
		if (typeflg == RIO_T_DOS || typeflg == RIO_T_ANY) {
			if (c1 == 0x0D) {
				i2 -= i3 + 1;
				if (i2) p++;
				else if (r->bsiz && i1 + i2 != i5) {
					i2 = rioxgb(fnum, &p, i5 - i1, i1, TRUE);
					if (i2 < 0) goto rioget1;
				}
				if (i2 && *p == NATEOR) {
					i1++;
					if (typeflg == RIO_T_ANY) {
						//typeflg = RIO_T_DOS; Not used anywhere after this point
						r->opts &= ~RIO_T_MASK;
						r->opts |= RIO_T_DOS;
					}
				}
				else {
					eorsize = 1;
					if (typeflg != RIO_T_ANY) {
						i2 = ERR_BADCH;
						goto rioget1;
					}
					//typeflg = RIO_T_MAC; Not used anywhere after this point
					r->opts &= ~RIO_T_MASK;
					r->opts |= RIO_T_MAC;
				}
				c1 = NATEOR;
			}
			else {
				if (typeflg != RIO_T_ANY) {
					i2 = ERR_BADCH;
					goto rioget1;
				}
				//typeflg = RIO_T_DAT; NOT USED
				r->opts &= ~RIO_T_MASK;
				r->opts |= RIO_T_DAT;
				eorsize = 1;
				i5 = recsize + eorsize;
			}
		}
		if (c1 != eorchr) {
			i2 = ERR_BADCH;
			goto rioget1;
		}
	}

	/* check if fix length is correct */
	if ((r->opts & RIO_FIX) && i1 != i5) {
		i2 = ERR_SHORT;
		goto rioget1;
	}

	/* restore record buffer from overflow */
	if (!r->bsiz) {
		record[recsize] = save2[0];
		record[recsize + 1] = save2[1];
	}

	/* set last and next position */
	r->lpos = r->npos;
	fiosetlpos(fnum, r->lpos);
	r->npos += i1;
	return(i1 - eorsize);

	/* it is DB/C type file and have compression */
rioget2:
	recpos = i1;
	i3 = 0;
	spaceflg = FALSE;
	for ( ; ; ) {
		while (i3++ < i2) {
			c1 = *p++;
			if (c1 == DBCSPC) {  /* space compression */
rioget0:
				if (i3++ == i2) {
					spaceflg = TRUE;
					continue;
				}
				i4 = *p++;
				if (!i4--) continue;
				if (recpos + i4 > recsize) i4 = recsize - recpos;
				memset(&record[recpos], ' ', i4);
				recpos += i4;
				c1 = ' ';
			}
			else if (!nocompressflag && c1 >= 0x80 && c1 < DBCSPC) {  /* digit compression */
				if (recpos != recsize) record[recpos++] = dig1map[c1 - 128];
				c1 = dig2map[c1 - 128];
			}
			else if (c1 == DBCEOR) {  /* EOR character */
				i1 += i3;
				if ((r->opts & RIO_FIX) && recpos != recsize) {
					i2 = ERR_SHORT;
					goto rioget1;
				}
				/* restore record buffer from overflow */
				if (!r->bsiz) {
					record[recsize] = save2[0];
					record[recsize + 1] = save2[1];
				}
				/* set last and next position */
				r->lpos = r->npos;
				fiosetlpos(fnum, r->lpos);
				r->npos += i1;
				return(recpos);
			}
			else if (c1 > DBCEOR) {  /* invalid character */
				i1 += i3;
				i2 = ERR_BADCH;
				goto rioget1;
			}

			/* prevent overflowing record buffer */
			if (recpos == recsize) {
				i1 += i3;
				i2 = ERR_NOEOR;
				goto rioget1;
			}
			record[recpos++] = c1;
		}
		if (!r->bsiz || i1 + i2 == i5) break;

		/* fill read buffer */
		i1 += i2;
		i2 = rioxgb(fnum, &p, i5 - i1, i1, TRUE);
		if (i2 < 1) {  /* error or no characters */
			if (i2 < 0) goto rioget1;
			i2 = ERR_NOEOF;
			goto rioget1;
		}
		i3 = 0;
		if (spaceflg) {
			spaceflg = FALSE;
			goto rioget0;
		}
	}
	i1 += i2;
	i2 = ERR_NOEOR;

rioget1:
	/* restore record buffer from overflow */
	if (!r->bsiz) {
		record[recsize] = save2[0];
		record[recsize + 1] = save2[1];
	}
	r->lpos = r->npos;
	r->npos += i1;
	return(i2);
}

/* RIOPREV */
/* get a record */
/* return record length, bof indicator (-1), deleted record (-2) or error */
/* or error */
INT rioprev(INT fnum, UCHAR *record, INT recsize)
{
	INT i1, i2, i3, i4, i5, recpos, eorsize, verflg;
	INT typeflg;
	OFFSET offset;
	UCHAR c1, eorchr, delchr, *p, *psave, *riowork, **rptr;

	if (recsize + 4 > riobufsiz) return(ERR_INVAR);

	/* initialize structure pointers */
	rptr = fiogetwptr(fnum);
	if (rptr == NULL) return(ERR_NOTOP);
	r = (struct rtab *) *rptr;
	if (r->type != 'R') return(ERR_NOTOP);

	/* flush write buffers */
	if (r->bflg == 2) {
		i1 = rioxwb(fnum);
		if (i1) return(i1);
	}

	/* other initializations */
	riowork = *riobuf;
	eorsize = 1;
	typeflg = r->opts & RIO_T_MASK;
	if (typeflg == RIO_T_STD) {
		eorchr = DBCEOR;
		delchr = DBCDEL;
	}
	else if (typeflg != RIO_T_BIN) {
		if (typeflg == RIO_T_DOS || typeflg == RIO_T_ANY) eorsize = 2;
		if (typeflg == RIO_T_MAC) eorchr = 0x0D;
		else eorchr = NATEOR;
		delchr = NATDEL;
	}
	else eorsize = 0;

	if (r->lpos == 0) {
		r->npos = 0;
		return(-1);
	}

	i5 = recsize + eorsize;
	if (typeflg != RIO_T_BIN) verflg = 1;
	else verflg = 0;
	if ((OFFSET)(i5 + verflg) > r->lpos) {
		i5 = (INT) r->lpos;
		verflg = 0;
	}

	if (r->bsiz) {
		i2 = rioxgb(fnum, &p, i5 + verflg, 0, FALSE);
		psave = p;
	}
	else {
		offset = r->lpos - i5 - verflg;
		i2 = fioread(fnum, offset, &riowork[1], i5 + verflg);
		if (i2 > 0) {
			psave = p = &riowork[i2];
		}
	}
	if (i2 < 1) {  /* error or no characters */
		if (i2 < 0) return(i2);
		return(ERR_RANGE);
	}

	/* check for deleted record */
	if (typeflg != RIO_T_BIN && *p == delchr) {
		if ((r->opts & (RIO_FIX | RIO_UNC)) == (RIO_FIX | RIO_UNC) && (r->lpos % i5)) {
			i5 = (INT)(r->lpos % i5);
			if (i2 > i5 + verflg) i2 = i5 + verflg;
		}
		i1 = 0;
		for ( ; ; ) {
			c1 = *(psave - i2);
			*(psave - i2) = 0x00;
			if (typeflg == RIO_T_STD) while (*p-- == DBCDEL);
			else while (*p-- == NATDEL);
			*(psave - i2) = c1;

			/* break if another read buffer is not needed */
			i3 = (INT)(psave - ++p);
			if (i3 < i2 || !r->bsiz || i1 + i2 == i5 + verflg) break;

			/* fill read buffer */
			i1 += i2;
			i2 = rioxgb(fnum, &p, i5 + verflg - i1, i1, FALSE);
			if (i2 < 1) {  /* error or no characters */
				if (i2 < 0) goto rioprev1;
				i2 = ERR_RANGE;
				goto rioprev1;
			}
			psave = p;
		}
		i1 += i3;

		if (verflg || (typeflg != RIO_T_BIN && i1 < i5)) {
			if (i1 <= i5) {
				c1 = *p;
				if (c1 != eorchr && (c1 != 0x0D || typeflg != RIO_T_ANY)) {
					i2 = ERR_NOEOR;
					goto rioprev1;
				}
			}
			else i1 = i5;
		}

		if ((r->opts & (RIO_FIX | RIO_UNC)) == (RIO_FIX | RIO_UNC) && i1 != i5) {
			i2 = ERR_SHORT;
			goto rioprev1;
		}

		r->npos = r->lpos;
		r->lpos -= i1;
		return(-2);
	}

	/* verify the eor exists */
	i1 = 0;
	if (typeflg != RIO_T_BIN) {
		c1 = *p--;
		if (typeflg == RIO_T_DOS || typeflg == RIO_T_ANY) {
			if (c1 == NATEOR) {
				if (i2 > 1) c1 = *p;
				else if (r->bsiz && i5 != 1) {
					i1 = 1;
					i2 = rioxgb(fnum, &p, i5 + verflg - 1, 1, FALSE);
					if (i2 < 1) {
						if (i2 < 0) goto rioprev1;
					}
					else {
						psave = p;
						c1 = *p;
					}
				}
				if (c1 == 0x0D) {
					c1 = NATEOR;
					p--;
					if (typeflg == RIO_T_ANY) {
						typeflg = RIO_T_DOS;
						r->opts &= ~RIO_T_MASK;
						r->opts |= RIO_T_DOS;
					}
				}
				else {
					if (i5 > recsize + 1) {
						i5 = recsize + 1;
						verflg = 1;
					}
					eorsize = 1;
					if (typeflg != RIO_T_ANY) {
						i2 = ERR_BADCH;
						goto rioprev1;
					}
					typeflg = RIO_T_DAT;
					r->opts &= ~RIO_T_MASK;
					r->opts |= RIO_T_DAT;
				}
			}
			else if (c1 == 0x0D) {
				if (i5 > recsize + 1) {
					i5 = recsize + 1;
					verflg = 1;
				}
				eorsize = 1;
				if (typeflg != RIO_T_ANY) {
					i2 = ERR_BADCH;
					goto rioprev1;
				}
				typeflg = RIO_T_MAC;
				r->opts &= ~RIO_T_MASK;
				r->opts |= RIO_T_MAC;
				eorchr = 0x0D;
			}
		}
		if (c1 != eorchr) {
			i1 = 1;
			i2 = ERR_BADCH;
			goto rioprev1;
		}
	}

	/* parse through record */
	for ( ; ; ) {
		if (typeflg != RIO_T_BIN) {
			c1 = *(psave - i2);
			if (typeflg == RIO_T_STD) {
				*(psave - i2) = DBCDEL;
				while (*p-- < DBCEOR);
			}
			else {
				*(psave - i2) = 0x00;
				while (*p > 0x1A && *p != NATDEL) p--;
				p--;
			}
			*(psave - i2) = c1;
			i3 = (INT)(psave - ++p);
		}
		else {
			p -= i2;
			i3 = i2;
		}

		if (i3 < i2) {
			if (typeflg != RIO_T_STD) {
				if (*p != eorchr && *p != NATDEL && *p != 0x0D && *p != 0x1A) {
					p--;
					continue;
				}
				if (*p == 0x0D && typeflg == RIO_T_DAT) {
					p--;
					continue;
				}
				if (*p == 0x1A && typeflg != RIO_T_DOS && typeflg != RIO_T_ANY) {
					p--;
					continue;
				}
			}
			break;
		}
		if (!r->bsiz || i1 + i2 == i5 + verflg) break;

		/* fill read buffer */
		i1 += i2;
		memcpy(&riowork[i5 + verflg - i1], p + 1, i2);
		i2 = rioxgb(fnum, &p, i5 + verflg - i1, i1, FALSE);
		if (i2 < 1) {  /* error or no characters */
			if (i2 < 0) goto rioprev1;
			i2 = ERR_RANGE;
			goto rioprev1;
		}
		psave = p;
	}
	i1 += i3;

	/* verify the end of the previous record */
	if (verflg || (typeflg != RIO_T_BIN && i1 < i5)) {
		if (i2 == i3) {
			i2 = ERR_RANGE;
			goto rioprev1;
		}
		if (i1 < i5 + verflg) c1 = *p;
		else c1 = 0x00;
		if (c1 != eorchr && c1 != delchr) {
			i2 = ERR_NOEOR;
			goto rioprev1;
		}
	}

	/* copy from buffer to riowork */
	if (r->bsiz) {
		if (i3) memcpy(&riowork[i5 + verflg - i1], p + 1, i3);
		p = &riowork[i5 + verflg - i1];
	}
	else p++;
	if (!(r->opts & RIO_UNC)) goto rioprev2;  /* may have compression */

	/* check if fix length is correct */
	if ((r->opts & RIO_FIX) && i1 != recsize + eorsize) {
		i2 = ERR_SHORT;
		goto rioprev1;
	}
	memcpy(record, p, i1 - eorsize);

	/* set last and next position */
	r->npos = r->lpos;
	r->lpos -= i1;
	fiosetlpos(fnum, r->lpos);
	return(i1 - eorsize);

	/* it is DB/C type file and may have compression */
rioprev2:
	recpos = 0;
	i3 = 0;
	while (++i3 < i1) {
		c1 = *p++;
		if (c1 == DBCSPC) {  /* space compression */
			if (++i3 == i1) {
				i2 = ERR_NOEOR;
				goto rioprev1;
			}
			i4 = *p++;
			if (!i4--) continue;
			if (recpos + i4 > recsize) i4 = recsize - recpos;
			memset(&record[recpos], ' ', i4);
			recpos += i4;
			c1 = ' ';
		}
		else if (!nocompressflag && c1 >= 0x80 && c1 < DBCSPC) {  /* digit compression */
			if (recpos != recsize) record[recpos++] = dig1map[c1 - 128];
			c1 = dig2map[c1 - 128];
		}
		else if (c1 >= DBCEOR) {  /* invalid character */
			i2 = ERR_BADCH;
			goto rioprev1;
		}

		/* prevent overflowing record buffer */
		if (recpos == recsize) {
			i2 = ERR_NOEOR;
			goto rioprev1;
		}
		record[recpos++] = c1;
	}
	if ((r->opts & RIO_FIX) && recpos != recsize) {
		i2 = ERR_SHORT;
		goto rioprev1;
	}
	r->npos = r->lpos;
	r->lpos -= i1;
	fiosetlpos(fnum, r->lpos);
	return(recpos);

rioprev1:
	r->npos = r->lpos;
	r->lpos -= i1;
	return(i2);
}

/*
 * RIOPUT
 * put a record
 */
INT rioput(INT fnum, UCHAR *record, INT recsize)
{
	INT i1, i2, i3, i4, datalen, eorsize, typeflg;
	OFFSET logpos = LONG_MIN, offset;
	UCHAR c1, delchr, eorchr, eofflg, digits[2], save3[3], work[256];
	UCHAR *p, *bptr, **rptr, flckflg;

	if (recsize + 4 > riobufsiz) return(ERR_INVAR);

	/* initialize structure pointers */
	rptr = fiogetwptr(fnum);
	if (rptr == NULL) return(ERR_NOTOP);
	r = (struct rtab *) *rptr;
	if (r->type != 'R') return(ERR_NOTOP);

	/* other initializations */
	typeflg = r->opts & RIO_T_MASK;
	eorsize = 1;
	if (typeflg == RIO_T_STD) eorchr = DBCEOR;
	else if (typeflg != RIO_T_BIN) {
		if (typeflg == RIO_T_ANY) return(ERR_BADTP);
		if (typeflg == RIO_T_DOS) eorsize = 2;
		if (typeflg == RIO_T_MAC) { /*eorchr = 0x0D*/ }
		//else eorchr = NATEOR;
		eorchr = NATEOR;
	}
	else eorsize = 0;

	if ((r->opts & RIO_LOG) && loghandle > 0) {
		if (typeflg == RIO_T_STD) delchr = DBCDEL;
		else delchr = NATDEL;
		p = *riobuf;
		/* get current record */
		i2 = fioread(fnum, r->npos, p, recsize + eorsize);
		if (i2 < 0) return(i2);
		if (i2 && *p != delchr && (!r->eofc || *p != r->eofc)) {
			/* updating record */
			if (typeflg == RIO_T_STD) {
				for (i1 = 0; i1 < i2; i1++) {
					if (p[i1] < DBCSPC) continue;
					if (p[i1] == DBCSPC) {
						if (!(r->opts & RIO_UNC)) i1++;
						continue;
					}
					break;
				}
				c1 = p[i1];
			}
			else if (typeflg != RIO_T_BIN) {
				for (i1 = 0; i1 < i2; i1++) {
					c1 = p[i1];
					if (c1 > 0x1A && c1 != NATDEL) continue;
					if (c1 != eorchr && c1 != NATDEL && c1 != 0x0D && c1 != 0x1A) continue;
					if (c1 == 0x0D && typeflg == RIO_T_DAT) continue;
					if (c1 == 0x1A && typeflg != RIO_T_DOS) continue;
					break;
				}
			}
			else i1 = i2;
			if (i1 > recsize) return(ERR_NOEOR);
			datalen = i1;
			if (typeflg == RIO_T_DOS) {
				if (c1 != 0x0D || p[i1 + 1] != NATEOR) return(ERR_BADCH);
				i1++;
				c1 = NATEOR;
			}

			/* check for invalid character */
			if (eorsize) {
				if (c1 != eorchr) return(ERR_BADCH);
				i1++;
			}

			/* check if fix length is correct */
			if ((r->opts & (RIO_FIX | RIO_UNC)) == (RIO_FIX | RIO_UNC) && i1 != recsize + eorsize) return(ERR_SHORT);

			/* write old record to log file */
			i2 = fioalock(loghandle, FIOA_FLLCK | FIOA_WRLCK, 0, 120);
			if (i2) return i2;
			fioalseek(loghandle, 0, 2, &logpos);
			logbufcnt = 0;
			logput("<u><f>", 6);
			i2 = mscitoa(fnum, logconnect + logconnectlen);
			logput(logconnect, logconnectlen + i2);
			logput("</f>", 4);
			if (loggingflags & RIO_L_NAM) logputfilename((UCHAR *)fioname(fnum));
			if (loggingflags & RIO_L_USR) logputusername();
			if (loggingflags & RIO_L_TIM) logputtimestamp();
			logput("<o>", 3);
			if (typeflg == RIO_T_STD && !(r->opts & RIO_UNC)) {
				memset(work, ' ', 255);
				i2 = 0;
				if (!nocompressflag) {
					for (i3 = i4 = 0; i3 < datalen; i3++) {
						if (p[i3] < 0x80) continue;
						if (i4 != i3) {
							i2 = logputdata(p + i4, i3 - i4);
							if (i2) break;
						}
						if (p[i3] < DBCSPC) {
							digits[0] = dig1map[p[i3] - 128];
							digits[1] = dig2map[p[i3] - 128];
							i2 = logput((CHAR *) digits, 2);
						}
						else if (p[i3] == DBCSPC) {
							if (++i3 == datalen) break;
							i2 = logput((CHAR *) work, p[i3]);
						}
						else i2 = 0;  /* should not happen */
						if (i2) break;
						i4 = i3 + 1;
					}
				}
				else {
					for (i3 = i4 = 0; i3 < datalen; i3++) {
						if (p[i3] < DBCSPC) continue;
						if (i4 != i3) {
							i2 = logputdata(p + i4, i3 - i4);
							if (i2) break;
						}
						if (p[i3] == DBCSPC) {
							if (++i3 == datalen) break;
							i2 = logput((CHAR *) work, p[i3]);
						}
						else i2 = 0;  /* should not happen */
						if (i2) break;
						i4 = i3 + 1;
					}
				}
				if (i4 != i3 && !i2) i2 = logputdata(p + i4, i3 - i4);
			}
			else i2 = logputdata(p, datalen);
			if (!i2) i2 = logput("</o>", 4);
			c1 = 'u';
		}
		else {
			/* write new record to log file */
			i2 = fioalock(loghandle, FIOA_FLLCK | FIOA_WRLCK, 0, 120);
			if (i2) return i2;
			fioalseek(loghandle, 0, 2, &logpos);
			logbufcnt = 0;
			logput("<w><f>", 6);
			i2 = mscitoa(fnum, logconnect + logconnectlen);
			logput(logconnect, logconnectlen + i2);
			logput("</f>", 4);
			if (loggingflags & RIO_L_NAM) logputfilename((UCHAR *)fioname(fnum));
			if (loggingflags & RIO_L_USR) logputusername();
			if (loggingflags & RIO_L_TIM) logputtimestamp();
			i2 = 0;
			c1 = 'w';
		}
		if (!i2) i2 = logput("<n>", 3);
		if (!i2) i2 = logputdata(record, recsize);
		if (!i2) i2 = logput("</n></", 6);
		if (!i2) i2 = logput((CHAR *) &c1, 1);
		if (!i2) i2 = logput(">", 1);
		if (!i2) i2 = logflush();
		if (i2) {
			if (logpos == LONG_MIN) return ERR_PROGX;
			fioatrunc(loghandle, logpos);
			fioalock(loghandle, FIOA_FLLCK | FIOA_UNLCK, 0, 0);
			return i2;
		}
	}

	if (r->opts & RIO_UNC) p = record;  /* no compression */
	else {  /* compression */
		recsize = riocomp(record, recsize);
		p = *riobuf;
	}

	/* append eor to record */
	if (eorsize) {
		save3[0] = record[recsize];
		if (eorsize == 2) {
			p[recsize++] = 0x0D;
			save3[1] = record[recsize];
		}
		p[recsize++] = eorchr;
	}

	/* append eof to record */
	eofflg = FALSE;
	flckflg = FALSE;
	if (r->eofc) {
		if (!(r->opts & DBCWEOF) && r->npos + recsize >= r->fsiz) {

			if (randomwritelockflag) {
				i1 = fioflck(fnum);
				if (i1) return i1;
				flckflg = TRUE;
			}

			fiogetsize(fnum, &offset);
			if (offset > r->fsiz) r->fsiz = offset;
		}
		if (r->npos + recsize >= r->fsiz) {
			save3[eorsize] = record[recsize];
			p[recsize++] = r->eofc;
			eofflg = TRUE;
		}
		else if (flckflg) {
			fiofulk(fnum);
			flckflg = FALSE;
		}
	}

	if (r->bsiz) {  /* buffered */
		bptr = *r->bptr;
		offset = r->npos;
		i4 = recsize;
		while (i4) {
			/* flush write buffer is necessary */
			if (r->bflg == 2) {
				if (offset < r->bpos + r->bwst || offset > r->bpos + r->blen || offset == r->bpos + r->bsiz) {
					i1 = rioxwb(fnum);
					if (i1) {
						if ((r->opts & RIO_LOG) && loghandle > 0) {
							if (logpos == LONG_MIN) return ERR_PROGX;
							fioatrunc(loghandle, logpos);
							fioalock(loghandle, FIOA_FLLCK | FIOA_UNLCK, 0, 0);
						}
						return(i1);
					}
				}
			}

			/* start a new write buffer */
			if (r->bflg != 2) {
				r->bflg = 2;
				r->bpos = (offset / r->bsiz) * r->bsiz;
				r->blen = r->bwst = (INT)(offset - r->bpos);
			}

			/* put record into write buffer */
			i2 = (INT)(offset - r->bpos);
			i3 = r->bsiz - i2;
			if (i4 < i3) i3 = i4;
			memcpy(bptr + i2, p, i3);
			i2 += i3;
			if (i2 > r->blen) r->blen = i2;
			p += i3;
			offset += i3;
			i4 -= i3;
		}
	}
	else {  /* no buffering */
		i1 = fiowrite(fnum, r->npos, p, recsize);
		if (i1) {
			if ((r->opts & RIO_LOG) && loghandle > 0) {
				if (logpos == LONG_MIN) return ERR_PROGX;
				fioatrunc(loghandle, logpos);
				fioalock(loghandle, FIOA_FLLCK | FIOA_UNLCK, 0, 0);
			}
			return(i1);
		}
	}
	if ((r->opts & RIO_LOG) && loghandle > 0) fioalock(loghandle, FIOA_FLLCK | FIOA_UNLCK, 0, 0);

	/* set last and next positions */
	r->lpos = r->npos;
	fiosetlpos(fnum, r->lpos);
	r->npos += recsize;
	if (r->npos > r->fsiz) r->fsiz = r->npos;
	if (eofflg) r->npos--;
	if (flckflg) fiofulk(fnum);

	/* restore original record buffer */
	if (eofflg) record[--recsize] = save3[eorsize];
	if (eorsize) {
		record[--recsize] = save3[--eorsize];
		if (eorsize) record[--recsize] = save3[0];
	}
	return(0);
}

/* RIOPARPUT */
/* partial put of a record (with no eor) */
INT rioparput(INT fnum, UCHAR *record, INT recsize)
{
	INT i1, i2, i3, i4;
	OFFSET offset;
	UCHAR *p, *bptr, **rptr;

	if (recsize + 4 > riobufsiz) return(ERR_INVAR);

	/* initialize structure pointers */
	rptr = fiogetwptr(fnum);
	if (rptr == NULL) return(ERR_NOTOP);
	r = (struct rtab *) *rptr;
	if (r->type != 'R') return(ERR_NOTOP);

	if (r->opts & RIO_UNC) p = record;  /* no compression */
	else {  /* compression */
		recsize = riocomp(record, recsize);
		p = *riobuf;
	}

	if (r->bsiz) {  /* buffered */
		bptr = *r->bptr;
		offset = r->npos;
		i4 = recsize;
		while (i4) {
			/* flush write buffer is necessary */
			if (r->bflg == 2) {
				if (offset < r->bpos + r->bwst || offset > r->bpos + r->blen || offset == r->bpos + r->bsiz) {
					i1 = rioxwb(fnum);
					if (i1) return(i1);
				}
			}

			/* start a new write buffer */
			if (r->bflg != 2) {
				r->bflg = 2;
				r->bpos = (offset / r->bsiz) * r->bsiz;
				r->blen = r->bwst = (INT)(offset - r->bpos);
			}

			/* put record into write buffer */
			i2 = (INT)(offset - r->bpos);
			i3 = r->bsiz - i2;
			if (i4 < i3) i3 = i4;
			memcpy(bptr + i2, p, i3);
			i2 += i3;
			if (i2 > r->blen) r->blen = i2;
			p += i3;
			offset += i3;
			i4 -= i3;
		}
	}
	else {  /* no buffering */
		i1 = fiowrite(fnum, r->npos, p, recsize);
		if (i1) return(i1);
	}

	/* set last and next positions */
	r->lpos = r->npos;
	fiosetlpos(fnum, r->lpos);
	r->npos += recsize;
	if (r->npos > r->fsiz) r->fsiz = r->npos;
	return(0);
}

/* RIOCOMP */
/* compress record into riowork, return length */
static INT riocomp(UCHAR *record, INT recsize)
{
	INT i1;
	UCHAR c1, c2, *p, *pend, *q, *riowork;

	q = riowork = *riobuf;
	p = record;
	pend = record + recsize;
	while (p < pend) {
		c1 = *p++;
		if (c1 == ' ') {
			/* start space compression */
			for (i1 = 0; ++i1 < 248 && p < pend && *p == ' '; p++);
			if (i1 > 2) {
				*q++ = DBCSPC;
				c1 = (UCHAR) i1;
			}
			else if (i1 == 2) *q++ = ' ';
		}
		else if (!nocompressflag && ((c1 >= '0' && c1 <= '9') || c1 == '.') && p < pend) {
			/* start digit compression */
			c2 = *p;
			if ((c2 >= '0' && c2 <= '9') || c2 == '.') {
				if (c1 == '.') c1 = 238;
				else c1 = (UCHAR)(128 + (c1 - '0') * 11);
				if (c2 == '.') c1 += 10;
				else c1 += c2 - '0';
				p++;
			}
		}
		*q++ = c1;
	}
	return((INT)(q - riowork));
}

/* RIODELETE */
/* delete the record at the current file position */
INT riodelete(INT fnum, INT recsize)
{
	INT i1, i2, i3, i4, datalen, eorsize, typeflg;
	OFFSET logpos;
	UCHAR c1, delchr, eorchr, digits[2], work[256], *riowork, **rptr;

	if (recsize + 4 > riobufsiz) return(ERR_INVAR);

	/* initialize structure pointers */
	rptr = fiogetwptr(fnum);
	if (rptr == NULL) return(ERR_NOTOP);
	r = (struct rtab *) *rptr;
	if (r->type != 'R') return(ERR_NOTOP);

	/* flush write buffers */
	if (r->bflg == 2) {
		i1 = rioxwb(fnum);
		if (i1) return(i1);
	}
	r->bflg = 0;

	/* other initializations */
	riowork = *riobuf;
	typeflg = r->opts & RIO_T_MASK;
	if (typeflg == RIO_T_BIN) return(0);
	eorsize = 1;
	if (typeflg == RIO_T_STD) {
		eorchr = DBCEOR;
		delchr = DBCDEL;
	}
	else {
		if (typeflg == RIO_T_ANY) return(ERR_BADTP);
		if (typeflg == RIO_T_DOS) eorsize = 2;
		//if (typeflg == RIO_T_MAC) eorchr = 0x0D;
		//else eorchr = NATEOR;
		eorchr = NATEOR;
		delchr = NATDEL;
	}

	/* get current record */
	i2 = fioread(fnum, r->npos, riowork, recsize + eorsize);
	if (i2 < 0) return(i2);

	/* don't delete if at EOF or already deleted */
	if (!i2 || (r->eofc && *riowork == r->eofc)) return(ERR_RANGE);
	if (*riowork == delchr) return(ERR_ISDEL);

	/* parse record */
	if (typeflg == RIO_T_STD) {
		for (i1 = 0; i1 < i2; i1++) {
			if (riowork[i1] < DBCSPC) continue;
			if (riowork[i1] == DBCSPC) {
				if (!(r->opts & RIO_UNC)) i1++;
				continue;
			}
			break;
		}
		c1 = riowork[i1];
	}
	else {
		for (i1 = 0; i1 < i2; i1++) {
			c1 = riowork[i1];
			if (c1 > 0x1A && c1 != NATDEL) continue;
			if (c1 != eorchr && c1 != NATDEL && c1 != 0x0D && c1 != 0x1A) continue;
			if (c1 == 0x0D && typeflg == RIO_T_DAT) continue;
			if (c1 == 0x1A && typeflg != RIO_T_DOS) continue;
			break;
		}
	}
	if (i1 > recsize) return(ERR_NOEOR);
	datalen = i1;

	if (typeflg == RIO_T_DOS) {
		if (c1 != 0x0D || riowork[i1 + 1] != NATEOR) return(ERR_BADCH);
		i1++;
		c1 = NATEOR;
	}

	/* check for invalid character */
	if (c1 != eorchr) return(ERR_BADCH);
	i1++;

	/* check if fix length is correct */
	if ((r->opts & (RIO_FIX | RIO_UNC)) == (RIO_FIX | RIO_UNC) && i1 != recsize + eorsize) return(ERR_SHORT);

	if ((r->opts & RIO_LOG) && loghandle > 0) {
		/* write delete to log file */
		i2 = fioalock(loghandle, FIOA_FLLCK | FIOA_WRLCK, 0, 120);
		if (i2) return i2;
		fioalseek(loghandle, 0, 2, &logpos);
		logbufcnt = 0;
		logput("<d><f>", 6);
		i2 = mscitoa(fnum, logconnect + logconnectlen);
		logput(logconnect, logconnectlen + i2);
		logput("</f>", 4);
		if (loggingflags & RIO_L_NAM) logputfilename((UCHAR *)fioname(fnum));
		if (loggingflags & RIO_L_USR) logputusername();
		if (loggingflags & RIO_L_TIM) logputtimestamp();
		logput("<o>", 3);
		if (typeflg == RIO_T_STD && !(r->opts & RIO_UNC)) {
			memset(work, ' ', 255);
			i2 = 0;
			if (!nocompressflag) {
				for (i3 = i4 = 0; i3 < datalen; i3++) {
					if (riowork[i3] < 0x80) continue;
					if (i4 != i3) {
						i2 = logputdata(riowork + i4, i3 - i4);
						if (i2) break;
					}
					if (riowork[i3] < DBCSPC) {
						digits[0] = dig1map[riowork[i3] - 128];
						digits[1] = dig2map[riowork[i3] - 128];
						i2 = logput((CHAR *) digits, 2);
					}
					else if (riowork[i3] == DBCSPC) {
						if (++i3 == datalen) break;
						i2 = logput((CHAR *) work, riowork[i3]);
					}
					else i2 = 0;  /* should not happen */
					if (i2) break;
					i4 = i3 + 1;
				}
			}
			else {
				for (i3 = i4 = 0; i3 < datalen; i3++) {
					if (riowork[i3] < DBCSPC) continue;
					if (i4 != i3) {
						i2 = logputdata(riowork + i4, i3 - i4);
						if (i2) break;
					}
					if (riowork[i3] == DBCSPC) {
						if (++i3 == datalen) break;
						i2 = logput((CHAR *) work, riowork[i3]);
					}
					else i2 = 0;  /* should not happen */
					if (i2) break;
					i4 = i3 + 1;
				}
			}
			if (i4 != i3 && !i2) i2 = logputdata(riowork + i4, i3 - i4);
		}
		else i2 = logputdata(riowork, datalen);
		if (!i2) i2 = logput("</o></d>", 8);
		if (!i2) i2 = logflush();
		if (i2) fioatrunc(loghandle, logpos);
		fioalock(loghandle, FIOA_FLLCK | FIOA_UNLCK, 0, 0);
		if (i2) return i2;
	}

	/* write out deleted record */
	memset(riowork, delchr, i1);
	return(fiowrite(fnum, r->npos, riowork, i1));
}

/*
 * RIOSETPOS
 * set the current file position
 */
INT riosetpos(INT fnum, OFFSET pos)
{
	INT i1;
	UCHAR **rptr;

	/* initialize structure pointers */
	rptr = fiogetwptr(fnum);
	if (rptr == NULL) return(ERR_NOTOP);
	r = (struct rtab *) *rptr;
	if (r->type != 'R') return(ERR_NOTOP);

	/* flush read/write buffers if needed */
	if (r->bflg == 1 && (pos < r->bpos || pos >= r->bpos + r->blen)) r->bflg = 0;
	else if (r->bflg == 2 && (pos < r->bpos + r->bwst || pos > r->bpos + r->blen || pos == r->bpos + r->bsiz)) {
		i1 = rioxwb(fnum);
		if (i1) return(i1);
	}

	r->lpos = r->npos = pos;
	return(0);
}

/*
 * RIONEXTPOS
 * return file position of next record to be read or written
 */
INT rionextpos(INT fnum, OFFSET *pos)
{
	UCHAR **rptr;

	/* initialize structure pointers */
	rptr = fiogetwptr(fnum);
	if (rptr == NULL) return(ERR_NOTOP);
	r = (struct rtab *) *rptr;
	if (r->type != 'R') return(ERR_NOTOP);

	*pos = r->npos;
	return(0);
}

/*
 * RIOLASTPOS
 * return file position of last record read or written
 */
INT riolastpos(INT fnum, OFFSET *pos)
{
	UCHAR **rptr;

	/* initialize structure pointers */
	rptr = fiogetwptr(fnum);
	if (rptr == NULL) return(ERR_NOTOP);
	r = (struct rtab *) *rptr;
	if (r->type != 'R') return(ERR_NOTOP);

	*pos = r->lpos;
	return(0);
}

/* RIOGRPLPOS */
/* return group file position of last record read or written */
INT riogrplpos(INT fnum, OFFSET *pos)
{
	return(fiogetlpos(fnum, pos));
}

/* RIOEOFPOS */
/* return file position of end of file */
INT rioeofpos(INT fnum, OFFSET *pos)
{
	INT i1;
	OFFSET offset;
	UCHAR **rptr;

	/* initialize structure pointers */
	rptr = fiogetwptr(fnum);
	if (rptr == NULL) return(ERR_NOTOP);
	r = (struct rtab *) *rptr;
	if (r->type != 'R') return(ERR_NOTOP);

	/* flush write buffers */
	if (r->bflg == 2) {
		i1 = rioxwb(fnum);
		if (i1) return(i1);
	}

	if (!(r->opts & DBCWEOF)) fiogetsize(fnum, &r->fsiz);
	offset = r->fsiz;
	if (r->eofc && offset > 0) offset--;
	*pos = offset;
	return(0);
}

/* RIOWEOF */
/* write an end of file character at the current file position */
INT rioweof(INT fnum)
{
	INT i1;
	OFFSET offset;
	UCHAR c1, **rptr;

	/* initialize structure pointers */
	rptr = fiogetwptr(fnum);
	if (rptr == NULL) return(ERR_NOTOP);
	r = (struct rtab *) *rptr;
	if (r->type != 'R') return(ERR_NOTOP);

	/* flush write buffers */
	if (r->bflg == 2) {
		i1 = rioxwb(fnum);
		if (i1) return(i1);
	}
	r->bflg = 0;

	/* write eof */
	offset = r->npos;
	c1 = r->eofc;
	if (c1) {
		i1 = fiowrite(fnum, r->npos, &c1, 1);
		if (i1) return(i1);
		offset++;
	}
	fiotrunc(fnum, offset);

	/* calculate new eof if not at eof */
	fiogetsize(fnum, &r->fsiz);
	if (c1) r->fsiz--;
	if (r->npos < r->fsiz) {
		r->fsiz = r->npos;
		r->opts |= DBCWEOF;
	}
	if (c1) r->fsiz++;

	/* version 5 compatibility */
	r->lpos = r->npos;
	return(0);
}

/* RIOFLUSH */
/* flush the buffers */
INT rioflush(INT fnum)
{
	INT i1;
	UCHAR **rptr;

	/* initialize structure pointers */
	rptr = fiogetwptr(fnum);
	if (rptr == NULL) return(ERR_NOTOP);
	r = (struct rtab *) *rptr;
	if (r->type != 'R') return(ERR_NOTOP);

	/* flush write buffers */
	if (r->bflg == 2) {
		i1 = rioxwb(fnum);
		if (i1) return(i1);
	}
	r->bflg = 0;
	return(0);
}

/* RIOTYPE */
/* return file type */
INT riotype(INT fnum)
{
	UCHAR **rptr;

	/* initialize structure pointers */
	rptr = fiogetwptr(fnum);
	if (rptr == NULL) return(ERR_NOTOP);
	r = (struct rtab *) *rptr;
	if (r->type != 'R') return(ERR_NOTOP);

	return(r->opts & RIO_T_MASK);
}

/* RIOEORSIZE */
/* get the number of bytes in the end of record mark */
INT rioeorsize(INT fnum)
{
	int typeflg;
	UCHAR **rptr;

	rptr = fiogetwptr(fnum);
	if (rptr == NULL) return ERR_NOTOP;
	r = (struct rtab *) *rptr;
	if (r->type != 'R') return ERR_NOTOP;

	typeflg = r->opts & RIO_T_MASK;
	if (typeflg == RIO_T_ANY) return ERR_BADTP;
	if (typeflg == RIO_T_DOS) return 2;
	if (typeflg == RIO_T_BIN) return 0;
	return 1;
}

/* RIOLOCK */
INT riolock(INT fnum, INT testlckflg)
{
	UCHAR **rptr;

	/* initialize structure pointers */
	rptr = fiogetwptr(fnum);
	if (rptr == NULL) return(ERR_NOTOP);
	r = (struct rtab *) *rptr;
	if (r->type != 'R') return(ERR_NOTOP);

	return(fiolckpos(fnum, r->npos, testlckflg));
}

/* RIOUNLOCK */
void riounlock(INT fnum, OFFSET pos)
{
	UCHAR **rptr;

	/* initialize structure pointers */
	rptr = fiogetwptr(fnum);
	if (rptr == NULL) return;
	r = (struct rtab *) *rptr;
	if (r->type != 'R') return;

	fioulkpos(fnum, pos);
}

/* RIOXGB */
/* return number of bytes that can be moved from buffer */
static INT rioxgb(INT fnum, UCHAR **pptr, INT bytes, INT off, INT fwdflg)
{
	INT i1;
	OFFSET offset;

	if (fwdflg) offset = r->npos + off;
	else {
		offset = r->lpos - off - 1;
		if (offset < 0) return(0);
	}

	if (r->bflg != 1 || offset < r->bpos || offset >= r->bpos + r->bsiz) {
		r->bpos = (offset / r->bsiz) * r->bsiz;
		i1 = fioflck(fnum);
		if (i1) return(i1);
		i1 = fioread(fnum, r->bpos, *r->bptr + 1, r->bsiz);
		fiofulk(fnum);
		if (i1 < 0) {
			r->blen = 0;
			return(i1);
		}
		r->blen = i1;
	}
	r->bflg = 1;

	if (fwdflg) {
		if (offset >= r->bpos + r->blen) return(0);
		i1 = (INT)(r->bpos + r->blen - offset);
		*pptr = *r->bptr + (r->blen - i1) + 1;
	}
	else {
		if (offset >= r->bpos + r->blen) return(ERR_RANGE);
		i1 = (INT)(offset - r->bpos + 1);
		*pptr = *r->bptr + i1;
	}
	if (i1 < bytes) bytes = i1;
	return(bytes);
}

/* RIOXWB */
/* write out buffer, return error if error */
static INT rioxwb(INT fnum)
{
	INT i1, i2;
	OFFSET offset;

	r->bflg = 0;
	i2 = r->blen - r->bwst;
	if (!i2) return(0);

	/* write out buffer */
	offset = r->bpos + r->bwst;
	i1 = fioflck(fnum);
	if (i1) return(i1);
	i1 = fiowrite(fnum, offset, *r->bptr + r->bwst, i2);
	fiofulk(fnum);
	if (i1) return(i1);

	/* set filesize if written beyond eof */
	offset += i2;
	if (offset > r->fsiz) r->fsiz = offset;
	return(0);
}

static INT logput(CHAR *str, INT len)
{
	INT i1;

	if (len == -1) len = (INT)strlen(str);
	while (len > (INT)(sizeof(logbuffer) - logbufcnt)) {
		i1 = sizeof(logbuffer) - logbufcnt;
		memcpy(logbuffer + logbufcnt, str, i1);
		logbufcnt += i1;
		str += i1;
		len -= i1;
		i1 = logflush();
		if (i1) return i1;
	}
	memcpy(logbuffer + logbufcnt, str, len);
	logbufcnt += len;
	return 0;
}

static INT logputdata(UCHAR *str, INT len)
{
	INT i1, i2, i3;

	if (len == -1) len = (INT)strlen((CHAR *) str);
	for (i2 = i3 = 0; i2 <= len; i2++) {
		if (i2 == len || str[i2] == '<' || str[i2] == '>' || str[i2] == '&' || str[i2] == '"' || str[i2] == '\'') {
			i1 = logput((CHAR *)(str + i3), i2 - i3);
			if (i1) return i1;
			if (i2 == len) break;
			if (str[i2] == '<') i1 = logput("&lt;", 4);
			else if (str[i2] == '>') i1 = logput("&gt;", 4);
			else if (str[i2] == '&') i1 = logput("&amp;", 5);
			else if (str[i2] == '"') i1 = logput("&quot;", 6);
			else i1 = logput("&apos;", 6);
			if (i1) return i1;
			i3 = i2 + 1;
		}
	}
	return 0;
}

static INT logflush()
{
	INT i1;

	i1 = fioawrite(loghandle, logbuffer, logbufcnt, -1, NULL);
	logbufcnt = 0;
	return i1;
}
