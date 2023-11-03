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
#define INC_LIMITS
#define INC_SIGNAL
#define INC_ERRNO
#include "includes.h"
#include "release.h"
#include "arg.h"
#include "base.h"
#include "dbccfg.h"
#include "fio.h"

#if OS_UNIX
#include <sys/types.h>
#endif

#define NATEOR 0x0A
#define NATDEL 0x7F

#define VBLK 0x01
#define UBLK 0x02
#define DBLK 0x03

#define DSPFLAGS_VERBOSE	0x01
#define DSPFLAGS_DSPXTRA	0x02

#define BLK_SIZE 16384
#define DEATH_INTERRUPT		0
#define DEATH_INVPARM		1
#define DEATH_INVPARMVAL	2
#define DEATH_INIT			3
#define DEATH_NOMEM			4
#define DEATH_OPEN			5
#define DEATH_CLOSE			6
#define DEATH_READ			7
#define DEATH_WRITE			8
#define DEATH_TYPE			9
#define DEATH_NOEOF			10
#define DEATH_INVEOF		11
#define DEATH_INVDBC		12
#define DEATH_HDRSHORT		13
#define DEATH_ISIINV0		14
#define DEATH_HDRINVVER		15
#define DEATH_HDRINVNUM		16
#define DEATH_HDRINV57		17
#define DEATH_HDRINV100		18
#define DEATH_HDRINV165		19
#define DEATH_HDRINV511		20
#define DEATH_HDRINV1023	21
#define DEATH_HDRINVPARM	22
#define DEATH_ISIBLKSIZ		23
#define DEATH_ISIINVOFF		24
#define DEATH_ISIPASTEOF	25
#define DEATH_ISIINVSIZE	26
#define DEATH_ISITOOBIG		27
#define DEATH_ISIBADLEN		28
#define DEATH_ISIINVLINK	29
#define DEATH_ISIBADTYPE	30
#define DEATH_ISIBADBLK		31
#define DEATH_ISINOTUSED	32
#define DEATH_AIMINV0		33
#define DEATH_AIMINVEXT		34

/* local definitions */
static INT dspflags;
static INT handle;
static INT blksize, keylen, size0, size1, version;
static OFFSET maxpos;
static UCHAR blk[BLK_SIZE + 1];
static UCHAR *bptr, **bufptr;
static INT bsiz, blen;
static OFFSET lpos, npos, bpos;
static UCHAR eofc;
static UCHAR eorchr;
static INT cmpflg;
static OFFSET count;
static INT exitstatus;
static INT openflg;
static INT xtraflg;
static INT daystodate[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
static CHAR day[] = { "SunMonTueWedThuFriSat" };
static CHAR mth[] = { "JanFebMarAprMayJunJulAugSepOctNovDec" };
static char *errormsg[] = {
	"HALTED - user interrupt",
	"Invalid parameter ->",
	"Invalid parameter value ->",
	"Unable to initialize",
	"Unable to allocate memory for buffer",
	"Unable to open",
	"Unable to close file",
	"Unable to read from file",
	"Unable to write to file",
	"Invalid file type or EOF character",
	"Unable to read EOF character",
	"Invalid EOF character",
	"Invalid file format for dbc object file",
	"Header block is too short",
	"Index header block does not start with an 'I'",
	"Header block contains invalid version number",
	"Header block contains invalid numeric value",
	"Header block position 57 is invalid",
	"Header block position 100 is invalid",
	"Header block position 165 is invalid",
	"Header block position 511 is invalid",
	"Header block position 1023 is invalid",
	"Header block parameter list is invalid",
	"Index header block contains invalid block size",
	"Index header block contain file pointers with invalid offsets",
	"Index header block contain file pointers pointing past EOF",
	"Index file size is invalid",
	"Index too large / not enough memory for extra index information",
	"Bad length block at offset ",
	"Index block used multiple times in tree structure at offset ",
	"Invalid block type at offset ",
	"Invalid block or no end of block at offset ",
	"Block not used in index tree at offset ",
	"Aimdex header block does not start with an 'A'",
	"Aimdex extent is not valid"
};

/* routine declarations */
static INT fchktype(INT);
static INT fchkget(INT, UINT);
static INT fchkxgb(INT, INT, UCHAR **, INT, INT);
static void fixeof(INT);
static void fchkisi(INT);
static void isixtra(void);
static void dynscan(OFFSET);
static void fchkaim(INT);
static void aimxtra(void);
static void fchkdbc(void);
static void fputch5(UCHAR *);
static void fputint(INT);
static void fputoff(OFFSET);

#if OS_WIN32
__declspec (noreturn) static void badisi(INT, OFFSET);
__declspec (noreturn) static void usage(void);
__declspec (noreturn) static void death(INT, INT, CHAR *);
__declspec (noreturn) static void quitsig(INT);
#else
static void badisi(INT, OFFSET)  __attribute__((__noreturn__));
static void usage(void)  __attribute__((__noreturn__));
static void death(INT, INT, CHAR *)  __attribute__((__noreturn__));
static void quitsig(INT)  __attribute__((__noreturn__));
#endif


INT main(INT argc, CHAR *argv[])
{
	INT i1, recsz, chgflg, isiflg, aimflg, exitflg, dbcflg;
	INT minlen, maxlen, reclen, dspflg, fixflg, typeflg;
	CHAR cfgname[MAX_NAMESIZE], filename[MAX_NAMESIZE], *ptr;
	UCHAR cmpyn, delyn;
	FIOPARMS parms;

	arginit(argc, argv, &i1);
	if (!i1) dspsilent();
	signal(SIGINT, quitsig);

	/* initialize */
	if (meminit(68 << 10, 0, 32) == -1) death(DEATH_INIT, ERR_NOMEM, NULL);
	cfgname[0] = '\0';
	while (!argget(ARG_NEXT | ARG_IGNOREOPT, (char *) blk, sizeof(blk))) {
		if (blk[0] == '-') {
			if (blk[1] == '?') usage();
			if (toupper(blk[1]) == 'C' && toupper(blk[2]) == 'F' &&
			    toupper(blk[3]) == 'G' && blk[4] == '=') strcpy(cfgname, (char *) &blk[5]);
		}
	}
	if (cfginit(cfgname, FALSE)) death(DEATH_INIT, 0, cfggeterror());
	if (prpinit(cfggetxml(), CFG_PREFIX "cfg")) ptr = fioinit(NULL, FALSE);
	else ptr = fioinit(&parms, FALSE);
	if (ptr != NULL) death(DEATH_INIT, 0, ptr);

	ptr = (CHAR *) blk;
	xtraflg = isiflg = aimflg = chgflg = dbcflg = fixflg = FALSE;
	exitflg = TRUE;
	typeflg = reclen = 0;
	openflg = FIO_M_ERO | FIO_P_TXT;

	/* scan input file name */
	i1 = argget(ARG_FIRST, filename, sizeof(filename));
	if (i1 < 0) death(DEATH_INIT, i1, NULL);
	if (i1 == 1) usage();

	for ( ; ; ) {
		i1 = argget(ARG_NEXT, ptr, sizeof(blk));
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
					aimflg = TRUE;
					break;
				case 'C':
					if (!ptr[2]) chgflg = TRUE;
					else if (toupper(ptr[2]) != 'F' || toupper(ptr[3]) != 'G' || ptr[4] != '=' || !ptr[5]) death(DEATH_INVPARM, 0, ptr);
					break;
				case 'D':
					typeflg = RIO_T_STD;
					break;
				case 'E':
					exitflg = FALSE;
					break;
				case 'F':
					fixflg = TRUE;
					break;
				case 'I':
					isiflg = TRUE;
					break;
				case 'J':
					if (toupper(ptr[2]) == 'R') openflg = FIO_M_SRO | FIO_P_TXT;
					else openflg = FIO_M_SHR | FIO_P_TXT;
					break;
				case 'L':
					if (ptr[2] != '=') death(DEATH_INVPARMVAL, 0, ptr);
					reclen = atoi(&ptr[3]);
					if (reclen < 1 || reclen > RIO_MAX_RECSIZE) death(DEATH_INVPARMVAL, 0, ptr);
					break;
				case 'P':
					dbcflg = TRUE;
					break;
				case 'T':
					if (ptr[2] == '=') {
						strcpy(cfgname, &ptr[3]);
						for (i1 = 0; cfgname[i1]; i1++) cfgname[i1] = (CHAR) toupper(cfgname[i1]);
						if (!strcmp(cfgname, "CRLF")) typeflg = RIO_T_DOS;
						else if (!strcmp(cfgname, "DATA")) typeflg = RIO_T_DAT;
						else if (!strcmp(cfgname, "DOSZ")) {
							typeflg = RIO_T_DOS;
							eofc = 0x1A;
						}
						else if (!strcmp(cfgname, "MAC")) typeflg = RIO_T_MAC;
						else if (!strcmp(cfgname, "STD")) typeflg = RIO_T_STD;
						else if (!strcmp(cfgname, "TEXT")) typeflg = RIO_T_TXT;
						else death(DEATH_INVPARMVAL, 0, ptr);
					}
					else typeflg = RIO_T_TXT;
					break;
				case 'V':
					dspflags |= DSPFLAGS_VERBOSE;
					break;
				case 'X':
					xtraflg = TRUE;
					break;
				default:
					death(DEATH_INVPARM, 0, ptr);
			}
		}
		else death(DEATH_INVPARM, 0, ptr);
	}

	/* open the input file */
	if (isiflg) miofixname(filename, ".isi", FIXNAME_EXT_ADD);
	else if (aimflg) miofixname(filename, ".aim", FIXNAME_EXT_ADD);
	else if (dbcflg) miofixname(filename, ".dbc", FIXNAME_EXT_ADD);
	else {
		miofixname(filename, ".txt", FIXNAME_EXT_ADD);
		if (fixflg) {
			if ((openflg & FIO_M_MASK) == FIO_M_ERO) openflg = (openflg & ~FIO_M_MASK) | FIO_M_EXC;
			else if ((openflg & FIO_M_MASK) == FIO_M_SRO) openflg = (openflg & ~FIO_M_MASK) | FIO_M_SHR;
		}
	}
	handle = fioopen(filename, openflg);
	if (handle < 0) death(DEATH_OPEN, handle, filename);

	if (!typeflg && !isiflg && !aimflg && !dbcflg) {  /* see if it is a .dbc file */
		i1 = fioread(handle, 0, blk, 2);
		if (i1 < 0) death(DEATH_READ, i1, NULL);
		if (i1 == 2 && blk[0] >= 3 && (UCHAR)(blk[0] ^ blk[1]) == 0xFF) dbcflg = TRUE;
	}

	/* aim, isi, or dbc */
	if (isiflg || aimflg || dbcflg) {
		if (isiflg) fchkisi(xtraflg);
		else if (aimflg) fchkaim(xtraflg);
		else fchkdbc();
		fioclose(handle);
		dspstring("Filechk complete\n");
		cfgexit();
		exit(0);
	}

	/* allocate the buffer */
	bsiz = (64 << 10) + 4;  // 65540
	bufptr = memalloc(bsiz, 0);
	if (bufptr == NULL) death(DEATH_INIT, ERR_NOMEM, NULL);
	bptr = *bufptr;

	/* record file */
	if (fixflg) {
		fixeof(typeflg);
		fioclose(handle);
		cfgexit();
		exit(0);
	}

	if (!typeflg) {
		i1 = fchktype(handle);
		if (i1 < 0) {
			death(DEATH_TYPE, 0, filename);
			cfgexit();
			exit(1);
		}
	}
	else i1 = typeflg;
	if (typeflg) dspstring("Assume file is a ");
	else if (i1 == RIO_T_TXT) dspstring("Assumming file is a ");
	else dspstring("File is a ");
	if (i1 == RIO_T_STD) dspstring("DB/C standard type text file\n");
	else if (i1 == RIO_T_DAT) {
#if OS_UNIX
		if (!typeflg) dspstring("DATA (UNIX text) type text file\n");
		else
#endif
		dspstring("DATA type text file\n");
	}
	else if (i1 == RIO_T_DOS) {
#if OS_WIN32
		if (!typeflg) dspstring("CRLF (Windows text) type text file");
		else
#endif
		dspstring("CRLF type text file");
		if (eofc) dspstring(" with ^Z EOF character");
		dspchar('\n');
	}
	else if (i1 == RIO_T_MAC) dspstring("MAC type text file\n");
	else {
#if OS_WIN32
		dspstring("CRLF (Windows text) type text file\n");
		i1 = RIO_T_DOS;
#else
		dspstring("DATA (UNIX text) type text file\n");
		i1 = RIO_T_DAT;
#endif
	}
	openflg |= i1;
	lpos = npos = 0;
	blen = 0xFFFF;

	exitstatus = 0;
	count = maxlen = 0;
	minlen = RIO_MAX_RECSIZE;
	cmpyn = delyn = 'N';
	/* get records until end of file */
	while (TRUE) {
		cmpflg = dspflg = FALSE;
		recsz = fchkget(handle, RIO_MAX_RECSIZE);
		if (recsz < 0) {
			if (recsz == -1) {
				break;   /* end of file */
			}
			if (recsz == -2) {
				if (delyn == 'N') {
					delyn = 'Y';
					if (chgflg) {
						//dspflg = TRUE;  Not used again
						dspstring("Deleted record first encountered at file position : ");
						fputoff(lpos);
						dspchar('\n');
					}
				}
				continue;
			}
			if (recsz == -3) {
				dspstring("File contains EOF character before physical EOF at file position : ");
				fputoff(lpos);
				dspchar('\n');
				if (exitflg) exitstatus = 1;
				continue;
			}
			if (recsz == ERR_RANGE) {
				dspstring("File missing EOF mark at file position : ");
				fputoff(npos);
				dspchar('\n');
				exitstatus = 1;
				break;
			}
			if (recsz == ERR_BADCH) {
				dspstring("Record contains an invalid character at record offset : ");
				fputoff(npos - lpos - 1);
				dspchar('\n');
				recsz = (INT)(npos - lpos);
			}
			else if (recsz == ERR_NOEOR) {
				dspstring("Record missing EOR mark at record offset : ");
				fputoff(npos - lpos - 1);
				dspchar('\n');
				recsz = (INT)(npos - lpos);
			}
			else {
				dspstring("Operating system error or unspecified error at file position : ");
				fputoff(npos);
				dspchar('\n');
				exitstatus = 1;
				break;
			}
			dspflg = TRUE;
			exitstatus = 1;
		}
		count++;   /* increment record count */
		if (recsz > maxlen) {
			maxlen = recsz;
			if (chgflg) dspflg = TRUE;
		}
		if (recsz < minlen) {
			minlen = recsz;
			if (chgflg) dspflg = TRUE;
		}
		if (cmpflg && cmpyn == 'N') {
			cmpyn = 'Y';
			if (chgflg) {
				dspflg = TRUE;
				dspstring("Compression first encountered in :\n");
			}
		}
		if (reclen && recsz != reclen) {
			dspflg = TRUE;
			dspstring("Wrong length record :\n");
			exitstatus = 1;
		}
		if (dspflg) {
			dspstring("   record");
			fputoff(count);
			dspstring(", file pos =");
			fputoff(lpos);
			dspstring(", length =");
			fputint(recsz);
			dspchar('\n');
		}
	}
	fioclose(handle);
	if (count == 0) minlen = 0;   /* otherwise we get min = RIO_MAX_RECSIZE for 0 count! */

	dspstring("records=");
	fputoff(count);
	dspstring(", max size=");
	fputint(maxlen);
	dspstring(", min size=");
	fputint(minlen);
	dspstring(", compressed=");
	if (cmpyn == 'N') dspstring("N");
	else dspstring("Y");
	dspstring(", deleted=");
	if (delyn == 'N') dspstring("N\n");
	else dspstring("Y\n");
	dspstring("Filechk complete\n");
	cfgexit();
	exit(exitstatus);
}

/* FCHKTYPE */
static INT fchktype(INT fnum)
{
	INT i1, i2, type;
	OFFSET fsiz;
	UCHAR c1;

	type = 0;
	eofc = 0;
	fiogetsize(fnum, &fsiz);
	if (fsiz) {
		if (fioread(fnum, fsiz - 1, &c1, 1) != 1) return(ERR_RDERR);
		if (c1 == DBCEOF) {
			type = RIO_T_STD;
			eofc = DBCEOF;
		}
		else if (c1 == NATEOR) {
			type = RIO_T_DAT;
			if (fsiz > 1) {
				if (fioread(fnum, fsiz - 2, &c1, 1) != 1) return(ERR_RDERR);
				if (c1 == 0x0D) type = RIO_T_DOS;
			}
		}
		else if (c1 == 0x0D) type = RIO_T_MAC;
		else if (c1 == NATDEL) {
			for ( ; ; ) {
				if (fsiz >= BLK_SIZE) i1 = BLK_SIZE;
				else i1 = (INT) fsiz;
				fsiz -= i1;
				if (fioread(fnum, fsiz, blk, i1) != i1) return(ERR_RDERR);
				for (i2 = i1 - 1; i2 >= 0 && blk[i2] == NATDEL; i2--);
				if (i2 >= 0) {
					c1 = blk[i2];
					if (c1 == NATEOR) {
						type = RIO_T_DAT;
						if (fsiz + i2 > 1) {
							if (i2 > 1) c1 = blk[i2 - 1];
							else if (fioread(fnum, fsiz - 1, &c1, 1) != 1) return(ERR_RDERR);
							if (c1 == 0x0D) type = RIO_T_DOS;
						}
					}
					else if (c1 == 0x0D) type = RIO_T_MAC;
					break;
				}
				if (!fsiz) {
					type = RIO_T_TXT;
					break;
				}
			}
		}
		else if (c1 == 0x1A) {
			type = RIO_T_DOS;
			eofc = 0x1A;
		}
	}
	else type = RIO_T_TXT;

	if (!type) return(ERR_BADTP);
	return(type);
}

/* FCHKGET */
static INT fchkget(INT fnum, UINT recsize)
{
	static INT eorsize, typeflg;
	static UCHAR firstflg = TRUE;
	static UCHAR delchr, ichrflg;
	UINT i1, recpos;
	LONG i2, i4, i5;
	OFFSET pos;
	CHAR *ptr;
	UCHAR c1, spaceflg;
	UCHAR *p, *psave;
	ptrdiff_t i3;

	if (firstflg) {
		firstflg = FALSE;
		ichrflg = FALSE;
		if (!prpget("file", "ichrs", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) ichrflg = TRUE;
		eorsize = 1;
		typeflg = openflg & RIO_T_MASK;
		if (typeflg == RIO_T_STD) {
			eorchr = DBCEOR;
			delchr = DBCDEL;
		}
		else {
			if (typeflg == RIO_T_DOS) eorsize = 2;
			if (typeflg == RIO_T_MAC) eorchr = 0x0D;
			else eorchr = NATEOR;
			delchr = NATDEL;
		}
	}

	/* fill read buffer or read record */
	i5 = recsize + eorsize;
	if (bsiz) {
		i2 = fchkxgb(fnum, typeflg, &p, i5, 0);
		psave = p;
	}
	else {
		i2 = fioread(fnum, npos, bptr, i5);
		psave = p = bptr;
	}
	if (i2 < 1) {  /* error or no characters */
		if (i2 < 0) return(i2);
		/* verify end of file */
		if (!eofc) {
			fiogetsize(fnum, &pos);
			if (npos == pos) {
				lpos = npos;
				return(-1);
			}
		}
		return(ERR_RANGE);
	}

	/* check for deleted record */
	if (*p == delchr) {
		i1 = 0;
		while (TRUE) {
			c1 = *(psave + i2);
			*(psave + i2) = 0x00;
			if (typeflg == RIO_T_STD) while (*p++ == DBCDEL);
			else while (*p++ == NATDEL);
			*(psave + i2) = c1;

			/* break if another read buffer is not needed */
			i3 = --p - psave;
			if (i3 < i2 || !bsiz
					|| (LONG)(i1 + i2) == i5) break;

			/* fill read buffer */
			i1 += i2;
			i2 = fchkxgb(fnum, typeflg, &p, i5 - i1, i1);
			if (i2 < 1) {  /* error or no characters */
				if (i2 < 0) goto fchkget1;
				if (eofc) {
					i2 = ERR_NOEOF;
					goto fchkget1;
				}
				i3 = 0;
				break;
			}
			psave = p;
		}
		i1 += (UINT)i3;

		/* set last and next position */
		lpos = npos;
		npos += i1;
		return(-2);
	}

	/* check for end of file */
	if (eofc && *p == eofc) {
		/* set last position */
		lpos = npos++;
		fiogetsize(fnum, &pos);
		if (npos == pos) return(-1);  /* end of file */
		return(-3);
	}

	/* parse through record */
	i1 = 0;
	while (TRUE) {
		c1 = *(psave + i2);
		if (typeflg == RIO_T_STD) {
			*(psave + i2) = DBCDEL;
			if (!xtraflg) {
				if (!ichrflg) while (*p++ < 0x80);
				else while (*p++ < DBCSPC);
			}
			else {
				if (!ichrflg) while (*p < 0x80 && *p != 0x00) p++;
				else while (*p < DBCSPC && *p != 0x00) p++;
				p++;
			}
		}
		else {
			*(psave + i2) = 0x00;
			while (*p > 0x1A && *p != NATDEL) p++;
			p++;
		}
		*(psave + i2) = c1;
		i3 = --p - psave;

		if (i3 < i2) {
			c1 = *p;
			if (typeflg == RIO_T_STD) {
				if (c1 <= DBCSPC && c1 != 0x00) {
					cmpflg = TRUE;
					i2 -= (INT)i3;
					i1 += (UINT)i3;
					goto fchkget2;
				}
			}
			else if (c1 != eorchr && c1 != NATDEL && c1 != 0x0D && c1 != 0x1A) {
				if (!xtraflg || c1 != 0x00) {
					p++;
					continue;
				}
			}
			else if (c1 == 0x0D && typeflg == RIO_T_DAT) {
				p++;
				continue;
			}
			else if (c1 == 0x1A && typeflg != RIO_T_DOS) {
				p++;
				continue;
			}
			break;
		}
		if (!bsiz || (LONG)(i1 + i2) == i5) break;

		/* fill read buffer */
		i1 += i2;
		i2 = fchkxgb(fnum, typeflg, &p, i5 - i1, i1);
		if (i2 < 1) {  /* error or no characters */
			if (i2 < 0) goto fchkget1;
			i2 = ERR_NOEOR;
			goto fchkget1;
		}
		psave = p;
	}
	i1 += (UINT)i3;

	if (i1 > recsize) {
		i2 = ERR_NOEOR;
		goto fchkget1;
	}

	if (i2 == i3) {
		i2 = ERR_NOEOF;
		goto fchkget1;
	}
	i1++;
	if (typeflg == RIO_T_DOS) {
		if (c1 == 0x0D) {
			i2 -= (INT)i3 + 1;
			if (i2) p++;
			else if (bsiz && (LONG)(i1 + i2) != i5) {
				i2 = fchkxgb(fnum, typeflg, &p, i5 - i1, i1);
				if (i2 < 0) goto fchkget1;
			}
			if (i2 && *p == NATEOR) i1++;
			else {
				i2 = ERR_BADCH;
				goto fchkget1;
			}
			c1 = NATEOR;
		}
		else {
			i2 = ERR_BADCH;
			goto fchkget1;
		}
	}
	if (c1 != eorchr) {
		i2 = ERR_BADCH;
		goto fchkget1;
	}

	/* set last and next position */
	lpos = npos;
	npos += i1;
	return(i1 - eorsize);

	/* it is DB/C type file and have compression */
fchkget2:
	recpos = i1;
	i3 = 0;
	spaceflg = FALSE;
	while(TRUE) {
		while (i3++ < i2) {
			c1 = *p++;
			if (c1 == DBCSPC) {  /* space compression */
fchkget0:
				if (i3++ == i2) {
					spaceflg = TRUE;
					continue;
				}
				i4 = *p++;
				if (!i4--) continue;
				if (recpos + i4 > recsize) i4 = recsize - recpos;
				recpos += i4;
			}
			else if (!ichrflg && c1 >= 0x80 && c1 < DBCSPC) recpos++;  /* digit compression */
			else if (c1 == DBCEOR) {  /* EOR character */
				/* set last and next position */
				lpos = npos;
				npos += i1 + i3;
				return(recpos);
			}
			else if (c1 > DBCEOR) {  /* invalid character */
				i1 += (UINT)i3;
				i2 = ERR_BADCH;
				goto fchkget1;
			}

			/* prevent overflowing record buffer */
			if (recpos == recsize) {
				i1 += (UINT)(i2 - i3);
				i2 = ERR_NOEOR;
				goto fchkget1;
			}
			recpos++;
		}
		if (!bsiz || (LONG)(i1 + i2) == i5) break;

		/* fill read buffer */
		i1 += i2;
		i2 = fchkxgb(fnum, typeflg, &p, i5 - i1, i1);
		if (i2 < 1) {  /* error or no characters */
			if (i2 < 0) goto fchkget1;
			i2 = ERR_NOEOR;
			goto fchkget1;
		}
		i3 = 0;
		if (spaceflg) {
			spaceflg = FALSE;
			goto fchkget0;
		}
	}
	i1 += i2;
	i2 = ERR_NOEOR;

fchkget1:
	/* restore record buffer from overflow */
	lpos = npos;
	npos += i1;
	return(i2);
}

/* FCHKXGB */
/* return number of bytes that can be moved from buffer */
static INT fchkxgb(INT fnum, INT typeflg, UCHAR **pptr, INT n, INT offset)
{
	INT i1;
	OFFSET lwork;

	lwork = npos + offset;
	if (blen == 0xFFFF || lwork >= bpos + bsiz) {
		bpos = (lwork / bsiz) * bsiz;
		i1 = fioflck(fnum);
		if (i1) return(i1);
		i1 = fioread(fnum, bpos, bptr, bsiz);
		fiofulk(fnum);
		if (i1 < 0) {
			blen = 0;
			return(i1);
		}
		blen = i1;
	}

	if (lwork >= bpos + blen) return(0);
	i1 = (INT)(bpos + blen - lwork);
	*pptr = bptr + (blen - i1);
	if (i1 < n) n = i1;
	return(n);
}

static void fixeof(INT typeflg)
{
	INT i1, i2, i3;
	INT dbcflg, chgflg, zeroflg, eorflg, eorflg2, delflg, spcflg, eofflg;
	OFFSET filepos, eofpos;

	fiogetsize(handle, &eofpos);
	if (eofpos < RIO_MAX_RECSIZE) i1 = (INT) eofpos;
	else i1 = RIO_MAX_RECSIZE;
	dbcflg = TRUE;
	chgflg = eorflg = eorflg2 = spcflg = delflg = eofflg = FALSE;
	filepos = eofpos - i1;
	i2 = fioread(handle, filepos, bptr, i1);
	if (i2 < 0) death(DEATH_READ, i2, NULL);
	if (i1 != i2) {
		dspstring("ERROR: reading file\n");
		cfgexit();
		exit(1);
	}

	while (i1--) {
		if (!typeflg || typeflg == RIO_T_STD) {
			if (bptr[i1] == 0xFB) {
				eofflg = TRUE;
				break;
			}
			if (bptr[i1] == 0xFA) {
				eorflg = TRUE;
				break;
			}
			if (bptr[i1] == 0xFF) {
				delflg = TRUE;
				break;
			}
			if (bptr[i1] == 0xF9) {
				if (i1 + 1 == i2 || !bptr[i1 + 1]) spcflg = TRUE;
				break;
			}
		}
		if (!typeflg || typeflg != RIO_T_STD) {
			if (bptr[i1] == 0x1A) {
				eofflg = TRUE;
				dbcflg = FALSE;
				break;
			}
			if (bptr[i1] == 0x0A && (!i1 || bptr[i1 - 1] != 0xF9)) {
				eorflg = TRUE;
				dbcflg = FALSE;
				break;
			}
#if OS_WIN32
			if ((!typeflg || typeflg == RIO_T_TXT || typeflg == RIO_T_DOS) && bptr[i1] == 0x0D && (!i1 || bptr[i1 - 1] != 0xF9)) {
#else
			if (typeflg == RIO_T_DOS && bptr[i1] == 0x0D && (!i1 || bptr[i1 - 1] != 0xF9)) {
#endif
				eorflg2 = TRUE;
				dbcflg = FALSE;
				break;
			}
			if (bptr[i1] == 0x7F) {
				delflg = TRUE;
				dbcflg = FALSE;
				break;
			}
		}
	}
	if (i1++ < 0) {
		if (!typeflg) {
			dspstring("Unable to determine the type of file\n");
			dspstring("No modifications made to file\n");
			cfgexit();
			exit(1);
		}
		if (typeflg != RIO_T_STD) dbcflg = FALSE;
	}

	if (!i1) eorflg = TRUE;
	i3 = i1;
	if (i3 < i2) zeroflg = TRUE;
	else zeroflg = FALSE;
	while (i3 < i2) if (bptr[i3++] != 0x00) {
		zeroflg = FALSE;
		break;
	}
	if (dbcflg) {
		if (i1 == i2 && spcflg && (i1 == 1 || bptr[i1 - 2] == 0xFA || bptr[i1 - 2] == 0xFF)) {
			spcflg = FALSE;
			eorflg = TRUE;
			i1--;
		}
		if (i1 + 1 == i2 && (eorflg || delflg)) {
			bptr[i1] = 0xFB;
			chgflg = TRUE;
		}
		else if (i1 < i2) {
			if (eofflg) {
				if (i1 > 1 && bptr[i1 - 2] != 0xFA && bptr[i1 - 2] != 0xFF) {
					if (bptr[i1 - 2] == 0xF9) i1--;
					bptr[i1 - 1] = 0xFA;
				}
				else i1--;
				//eofflg = FALSE; Not used again
				eorflg = zeroflg = TRUE;
			}
			if (zeroflg) {
				if (spcflg) i1--;
				i3 = i1;
				if (!eorflg && !delflg) bptr[i3++] = 0xFA;
				while (i3 < i2 - 1) bptr[i3++] = 0xFF;
				bptr[i3++] = 0xFB;
				i2 = i3;
			}
			else {
				i1 = i2;
				bptr[i2++] = 0xFA;
				bptr[i2++] = 0xFB;
			}
			chgflg = TRUE;
		}
		else {
			/*
			 * Paul V. and the boys at PPRO have this bad habit of damaging a T=STD file so that
			 * the last two bytes are FAFF. We mess this up. We make the last three FAFFFB. Should be FAFB.
			 * So, here is a very special, very specific block of code to watch for this and deal
			 *
			 * Conditions
			 * dbcflg=delflg=TRUE, spcflg=eofflg=eorflg=FALSE
			 * bptr[i1 - 2] == 0xFA and bptr[i1 - 1] == 0xFF
			 */
			if (dbcflg && !spcflg && delflg && !eofflg && !eorflg && bptr[i1 - 2] == 0xFA && bptr[i1 - 1] == 0xFF) {
				bptr[i1] = 0xFB;
				i1 = fiowrite(handle, filepos + i1 - 1, &bptr[i1], 1);
				if (i1) death(DEATH_WRITE, i1, NULL);
				dspstring("EOF modified (special)\n");
				return;
			}

			if (spcflg) i1 = --i2;
			if (!eorflg && !delflg && !eofflg) bptr[i2++] = 0xFA;
			if (!eofflg) {
				bptr[i2++] = 0xFB;
				chgflg = TRUE;
			}
		}
	}
	else {
		if (i1 + 1 == i2 && (eorflg || delflg)) {
			if (typeflg == RIO_T_DOS && eofc) bptr[i1] = 0x1A;
			else bptr[i1] = 0x7F;
			chgflg = TRUE;
		}
		else if (i1 < i2) {
			if (eofflg) {
				if (i1 > 1 && bptr[i1 - 2] != 0x0A && bptr[i1 - 2] != 0x7F) {
					if (bptr[i1 - 2] != 0x0D) bptr[i1++ - 1] = 0x0D;
					bptr[i1 - 1] = 0x0A;
				}
				else i1--;
				//eofflg = FALSE; Not used again
				eorflg = zeroflg = TRUE;
			}
			if (zeroflg) {
				i3 = i1;
				if (eorflg2) {
					bptr[i3++] = 0x0A;
					eorflg = TRUE;
				}
				if (!eorflg && !delflg) {
					if (typeflg == RIO_T_DOS) bptr[i3++] = 0x0D;
#if OS_WIN32
					else if (!typeflg || typeflg == RIO_T_TXT) bptr[i3++] = 0x0D;
#endif
					bptr[i3++] = 0x0A;
				}
				if (typeflg == RIO_T_DOS && eofc) {
					while (i3 < i2 - 1) bptr[i3++] = 0x7F;
					bptr[i3++] = 0x1A;
				}
				else while (i3 < i2) bptr[i3++] = 0x7F;
				i2 = i3;
			}
			else {
				i1 = i2;
#if OS_WIN32
				if (!typeflg || typeflg == RIO_T_TXT || typeflg == RIO_T_DOS) bptr[i2++] = 0x0D;
#else
				if (typeflg == RIO_T_DOS) bptr[i2++] = 0x0D;
#endif
				bptr[i2++] = 0x0A;
				if (typeflg == RIO_T_DOS && eofc) bptr[i2++] = 0x1A;
			}
			chgflg = TRUE;
		}
		else {
			if (!eorflg && !delflg && !eofflg) {
#if OS_WIN32
				if (!eorflg2 && (!typeflg || typeflg == RIO_T_TXT || typeflg == RIO_T_DOS)) bptr[i2++] = 0x0D;
#else
				if (!eorflg2 && typeflg == RIO_T_DOS) bptr[i2++] = 0x0D;
#endif
				bptr[i2++] = 0x0A;
			}
#if OS_WIN32
			if (eofflg && typeflg && typeflg != RIO_T_TXT && typeflg != RIO_T_DOS) bptr[--i1] = 0x7F;
#else
			if (eofflg && typeflg != RIO_T_DOS) bptr[--i1] = 0x7F;
#endif
			if (i1 != i2) chgflg = TRUE;
		}
	}

	if (chgflg) {
		i1 = fiowrite(handle, filepos + i1, &bptr[i1], i2 - i1);
		if (i1) death(DEATH_WRITE, i1, NULL);
		dspstring("EOF modified\n");
	}
	else dspstring("No modifications made to file\n");
}

static void fchkisi(INT xtraflg)
{
	INT i1, i2;
	CHAR work[17];
	UCHAR c1;

	/* validate isi header */
	i1 = fioread(handle, 0, blk, 1024);
	if (i1 < 0) death(DEATH_READ, i1, NULL);
	if (i1 < 512) death(DEATH_HDRSHORT, 0, NULL);

	/* test validity of header block */
	if (blk[0] != 'I') death(DEATH_ISIINV0, 0, NULL);
	if (blk[99] != ' ') {
		if (!isdigit(blk[99])) death(DEATH_HDRINVVER, 0, NULL);
		version = blk[99] - '0';
		if (blk[98] != ' ') {
			if (!isdigit(blk[98])) death(DEATH_HDRINVVER, 0, NULL);
			version += (blk[98] - '0') * 10;
		}
		if (version > 12) {
			dspstring("Version ");
			dspchar(blk[98]);
			dspchar(blk[99]);
			dspstring(" index file not supported\n");
			cfgexit();
			exit(1);
		}
	}
	else version = 0;
	if ((version <= 8
			&& (!isdigit(blk[9]) || !isdigit(blk[18])
					|| !isdigit(blk[27]) || !isdigit(blk[36])))
					|| !isdigit(blk[45]) || !isdigit(blk[59])) death(DEATH_HDRINVNUM, 0, NULL);

	mscntoi(&blk[41], &i2, 5);
	if (i1 > i2) i1 = i2;
	blk[i1] = DBCDEL;

	c1 = (CHAR) blk[57];
	if (version >= 9) {
		if (c1 != ' ' && c1 != 'S') c1 = DBCDEL;
	}
	else if (version >= 7) {
		if (c1 != 'V' && c1 != 'S') c1 = DBCDEL;
	}
	else if (version == 6) {
		if (c1 != 'V' && c1 != 'F') c1 = DBCDEL;
	}
	else if (c1 != 'D' && c1 != 'Y' && c1 != 'N') c1 = DBCDEL;
	if (c1 == DBCDEL) death(DEATH_HDRINV57, 0, NULL);

	keylen = blk[59] - '0';
	if (blk[58] != ' ') {
		if (blk[58] >= 'A' && blk[58] <= 'Z') keylen += (blk[58] - 'A') * 10 + 100;
		else keylen += (blk[58] - '0') * 10;
	}

	if (blk[100] != DBCEOR) death(DEATH_HDRINV100, 0, NULL);
	if (version <= 8 && blk[165] != DBCEOR) death(DEATH_HDRINV165, 0, NULL);

	for (i2 = 101; blk[i2] != DBCDEL; i2++) if (blk[i2] == DBCEOR) blk[i2] = 0;
	if (blk[i2 - 1] != 0) death(DEATH_HDRINVPARM, 0, NULL);
	i2 = (INT)strlen((CHAR *) &blk[101]) + 102;
	
	dspstring("File type is");
	if (version) {
		dspstring(" version ");
		if (version >= 10) dspchar(blk[98]);
		dspchar(blk[99]);
	}
	dspstring(" index file");
	dspstring("\nText file is ");
	dspstring((CHAR *) &blk[101]);
	dspstring("\nBlock size is ");
	fputch5(&blk[41]);
	if (version >= 6) {
		if (c1 == 'F' || c1 == 'S') {
			dspstring("\nFixed length text records, length is ");
			fputch5(&blk[64]);
			dspstring("\nDeleted space reclaimation enabled");
		}
	}
	else if (c1 == 'Y' || c1 == 'N') {
		dspstring("\nStatic index");
		dspstring("\n*** Warning *** static indexes are not supported");
		xtraflg = FALSE;
	}
	mscitoa(keylen, work);
	dspstring("\nKey length is ");
	dspstring(work);
	dspstring("\nParameter(s) = ");
	if (blk[i2] != DBCDEL) {
		dspstring((CHAR *) &blk[i2]);
		if (version >= 8) {
			for (i2 += (INT)strlen((CHAR *) &blk[i2]) + 1; blk[i2] != DBCDEL; i2 += (INT)strlen((CHAR *) &blk[i2]) + 1) {
				dspchar(' ');
				dspstring((CHAR *) &blk[i2]);
			}
		}
	}
	else dspstring("not available");
	dspchar('\n');
	if (xtraflg) isixtra();  /* extra info */
}

static void isixtra()
{
	INT i1;
	OFFSET l1, l2, l3, lwork, delpos, eofpos, freepos, pos, toppos;
	UCHAR c1;

	if (version >= 9) {
		msc6xtooff(&blk[1], &delpos);
		msc6xtooff(&blk[7], &freepos);
		msc6xtooff(&blk[13], &toppos);
		msc6xtooff(&blk[19], &maxpos);
		size0 = 6;
		eorchr = DBCDEL;
	}
	else {
		msc9tooff(&blk[1], &delpos);
		msc9tooff(&blk[10], &freepos);
		msc9tooff(&blk[19], &toppos);
		msc9tooff(&blk[28], &maxpos);
		size0 = 9;
		eorchr = DBCEOR;
	}
	mscntoi(&blk[41], &blksize, 5);
	for (i1 = 512; i1 < blksize; i1 <<= 1);
	if (i1 != blksize) death(DEATH_ISIBLKSIZ, 0, NULL);
	if (delpos % blksize || freepos % blksize || toppos % blksize || maxpos % blksize) badisi(DEATH_ISIINVOFF, 0);
	if (delpos > maxpos || freepos > maxpos || toppos > maxpos) badisi(DEATH_ISIPASTEOF, 0);

	fiogetsize(handle, &eofpos);
	if (eofpos % blksize) death(DEATH_ISIINVSIZE, 0, NULL);
	if (maxpos >= eofpos) badisi(DEATH_ISIPASTEOF, 0);

	lwork = ((maxpos / blksize) >> 2) + 1;
	if (lwork < 0 || lwork > INT_MAX) death(DEATH_ISITOOBIG, 0, NULL);
	bsiz = (INT) lwork;
	bptr = (UCHAR *) malloc(bsiz);
	if (bptr == NULL) death(DEATH_ISITOOBIG, ERR_NOMEM, NULL);
	memset(bptr, 0, bsiz);

	size1 = keylen + size0;

	/* trace through deleted and free block lists */
	if (delpos) {
		pos = delpos;
		c1 = 'D';
	}
	else {
		pos = freepos;
		freepos = 0;
		c1 = 'F';
	}
	while (pos) {  /* follow deleted/free block linked list */
		i1 = fioread(handle, pos, blk, blksize);
		if (i1 < 0) death(DEATH_READ, i1, NULL);
		if (i1 != blksize) badisi(DEATH_ISIBADLEN, pos);
		lwork = pos / blksize;
		if (bptr[(INT)(lwork >> 2)] & (UCHAR)(DBLK << (((INT) lwork & 0x03) << 1))) badisi(DEATH_ISIINVLINK, lwork);
		bptr[(INT)(lwork >> 2)] |= (UCHAR)(DBLK << (((INT) lwork & 0x03) << 1));
		if (blk[0] != c1) badisi(DEATH_ISIBADTYPE, pos);
		for (i1 = 1; i1 < blksize && blk[i1] != eorchr; i1 += size0);
		if (i1 == 1 || (c1 == 'F' && i1 == size0 + 1) || i1 > blksize ||
		   (version <= 8 && (i1 == blksize || blk[i1] != DBCEOR))) badisi(DEATH_ISIBADBLK, pos);
		for (i1++; i1 < blksize && blk[i1] == DBCDEL; i1++);
		if (i1 < blksize) badisi(DEATH_ISIBADBLK, pos);
		lwork = pos;
		if (version >= 9) msc6xtooff(&blk[1], &pos);
		else msc9tooff(&blk[1], &pos);
		if (pos % blksize) badisi(DEATH_ISIINVOFF, lwork);
		if (pos > maxpos) badisi(DEATH_ISIPASTEOF, lwork);
		if (!pos) {
			pos = freepos;
			freepos = 0;
			c1 = 'F';
		}
	}

/*** CODE: SUPPORT DBC_COLLATE AND VERIFY KEYS ARE IN ORDER ***/
	if (toppos) dynscan(toppos);
	else dspstring("Index is empty\n");

	l1 = l2 = l3 = 0;
	maxpos /= blksize;
	for (lwork = 1; lwork <= maxpos; lwork++) {
		c1 = (UCHAR)((bptr[(INT)(lwork >> 2)] >> (((INT) lwork & 0x03) << 1)) & 0x03);
		if (c1 == UBLK) l1++;
		else if (c1 == VBLK) l2++;
		else if (c1 == DBLK) l3++;
		else badisi(DEATH_ISINOTUSED, lwork * blksize);
	}

	fputoff(l1);
	dspstring(" upper level blocks\n");
	fputoff(l2);
	dspstring(" lowest level blocks\n");
	fputoff(l3);
	dspstring(" deleted or space reclamation blocks\n");
}

static void dynscan(OFFSET pos)  /* recurse thru the index, marking used blocks */
{
	static INT depth;
	INT i1, i2;
	OFFSET lwork;

	if (depth != 1000) depth++;
	i1 = fioread(handle, pos, blk, blksize);
	if (i1 < 0) death(DEATH_READ, i1, NULL);
	if (i1 != blksize) badisi(DEATH_HDRSHORT, pos);
	lwork = pos / blksize;
	if (bptr[(INT)(lwork >> 2)] & (UCHAR)(0x03 << (((INT) lwork & 0x03) << 1))) badisi(DEATH_ISIINVLINK, lwork);
	if (blk[0] == 'U') {
		bptr[(INT)(lwork >> 2)] |= (UCHAR)(UBLK << (((INT) lwork & 0x03) << 1));
		for (i1 = 1; i1 < blksize; i1 += size1) {
			if (version >= 9) msc6xtooff(&blk[i1], &lwork);
			else msc9tooff(&blk[i1], &lwork);
			if (lwork % blksize) badisi(DEATH_ISIINVOFF, pos);
			if (lwork > maxpos) badisi(DEATH_ISIPASTEOF, pos);
/*** CODE: IT WOULD BE BETTER NOT TO USE RECURSION AND SET UP A LINKED LIST ***/
/***       AND TRY NOT TO REREAD THE BLOCKS ***/
			dynscan(lwork);
			i2 = fioread(handle, pos, blk, blksize);
			if (i2 < 0) death(DEATH_READ, i2, NULL);
			if (i2 != blksize) badisi(DEATH_ISIBADLEN, pos);
			i1 += size0;
			if (blk[i1] == eorchr) break;
		}
		if (i1 == size0 + 1 || i1 > blksize || (version <= 8 && (i1 == blksize || blk[i1] != DBCEOR))) badisi(DEATH_ISIBADBLK, pos);
		for (i1++; i1 < blksize && blk[i1] == DBCDEL; i1++);
		if (i1 < blksize) badisi(DEATH_ISIBADBLK, pos);
	}
	else if (blk[0] == 'V') {
		bptr[(UINT)(lwork >> 2)] |= (UCHAR)(VBLK << (((UINT) lwork & 0x03) << 1));
		if (depth != 1000) {
			fputint(depth);
			dspstring(" level index tree\n");
			depth = 1000;
		}
		if (version >= 9) {
			for (i1 = 1; i1 < blksize && blk[i1] != DBCDEL; )
				if (i1 == 1) i1 += size1;
				else {
					if ((INT) blk[i1] > keylen) badisi(DEATH_ISIBADBLK, pos);
					i1 += size1 - blk[i1] + 1;
				}
		}
		else for (i1 = 1; i1 < blksize && blk[i1] != DBCEOR; i1 += size1);
		if (i1 == 1 || i1 > blksize || (version <= 8 && (i1 == blksize || blk[i1] != DBCEOR))) badisi(DEATH_ISIBADBLK, pos);
	}
	else badisi(DEATH_ISIBADTYPE, pos);
}

static void fchkaim(INT xtraflg)
{
	INT i1, i2;

	i1 = fioread(handle, 0, blk, 1024);
	if (i1 < 0) death(DEATH_READ, i1, NULL);
	if (i1 != 1024) death(DEATH_HDRSHORT, 0, NULL);

	/* test validity of header block */
	if (blk[0] != 'A') death(DEATH_AIMINV0, 0, NULL);
	if (blk[57] != 'Y' && blk[57] != 'N') death(DEATH_HDRINV57, 0, NULL);
	if (blk[99] != ' ') {
		if (!isdigit(blk[99])) death(DEATH_HDRINVVER, 0, NULL);
		version = blk[99] - '0';
		if (blk[98] != ' ') {
			if (!isdigit(blk[98])) death(DEATH_HDRINVVER, 0, NULL);
			version += (blk[98] - '0') * 10;
		}
		if (version > 12) {
			dspstring("Version ");
			dspchar(blk[98]);
			dspchar(blk[99]);
			dspstring(" aim file not supported\n");
			cfgexit();
			exit(1);
		}
	}
	else version = 0;
	if ((version <= 8 && (!isdigit(blk[9]) || !isdigit(blk[18]) || !isdigit(blk[27])))
			|| !isdigit(blk[36])) death(DEATH_HDRINVNUM, 0, NULL);

	if (blk[100] != DBCEOR) death(DEATH_HDRINV100, 0, NULL);
	if (version <= 8 && blk[165] != DBCEOR) death(DEATH_HDRINV165, 0, NULL);
	if (version >= 6) {
		if (blk[1023] != DBCDEL) death(DEATH_HDRINV1023, 0, NULL);
	}
	else if (blk[511] != DBCDEL) death(DEATH_HDRINV511, 0, NULL);

	for (i2 = 101; blk[i2] != DBCDEL; i2++) if (blk[i2] == DBCEOR) blk[i2] = 0;
	if (blk[i2 - 1] != 0) death(DEATH_HDRINVPARM, 0, NULL);
	i2 = (INT)strlen((CHAR *) &blk[101]) + 102;

	dspstring("File type is");
	if (version) {
		dspstring(" version ");
		if (version >= 10) dspchar(blk[98]);
		dspchar(blk[99]);
	}
	dspstring(" aim file");
	dspstring("\nText file is ");
	dspstring((CHAR *) &blk[101]);
	dspstring("\nZ value is ");
	fputch5(&blk[32]);
	if (blk[59] == 'V') dspstring("\nVariable length record handling enabled");
	else {
		dspstring("\nFixed length text records, length is ");
		fputch5(&blk[41]);
		if ((version == 6 && blk[59] == 'F') || (version >= 7 && blk[59] == 'S')) dspstring("\nDeleted space reclaimation enabled");
	}
	dspstring("\nParameter(s) = ");
	if (blk[i2] != DBCDEL) {
		dspstring((CHAR *) &blk[i2]);
		if (version >= 8) {
			for (i2 += (INT)strlen((CHAR *) &blk[i2]) + 1; blk[i2] != DBCDEL; i2 += (INT)strlen((CHAR *) &blk[i2]) + 1)
			{
				dspchar(' ');
				dspstring((CHAR *) &blk[i2]);
			}
		}
	}
	else dspstring("not available");
	dspchar('\n');
	if (xtraflg) aimxtra();  /* extra info */
}

static void aimxtra()  /* provide extra information about the aim file */
{
	INT i1, i2, i3, i4, zvalue, hdrsize;
	OFFSET l1, bps, pos, recnum;

	dspstring("*** Primary extent information only ***\n");
	mscntoi(&blk[32], &zvalue, 5);  /* z value, number of slots */
	if (version >= 9) msc6xtooff(&blk[13], &bps);
	else msc9tooff(&blk[19], &bps);
	bps >>= 3;
	if (version >= 6) hdrsize = 1024;
	else hdrsize = 512;
	for (i1 = 0; i1 < zvalue; i1++) {
		pos = bps * i1 + hdrsize;
		for (l1 = 0; l1 < bps; l1 += 4096) {
			i2 = fioread(handle, pos + l1, blk, 4096);
			if (i2 < 0) death(DEATH_READ, i2, NULL);
			if (i2 < 4096 && l1 + i2 < bps) death(DEATH_AIMINVEXT, 0, NULL);
			for (i3 = 0; i3 < i2; i3++) {
				if (!blk[i3]) continue;
				for (i4 = 0; i4 < 8; i4++) {
					if (blk[i3] & (1 << i4)) {
						dspstring("Slot ");
						fputint(i1);
						dspstring(" shows record");
						recnum = ((l1 + i3) << 3) + i4;
						fputoff(recnum);
						dspchar('\n');
					}
				}
			}
		}
	}
}

static void fchkdbc()
{
	INT i1, days, mon;
	CHAR date[32], *ptr, verwork[4];
	UCHAR c1, c2;

	i1 = fioread(handle, 0, blk, 20);
	if (i1 < 0) death(DEATH_READ, i1, NULL);
	if (i1 < 20) death(DEATH_INVDBC, 0, NULL);
	c1 = blk[0];
	c2 = blk[1];
	if (c1 < 3 || (UCHAR) (c1 ^ c2) != 0xFF) death(DEATH_INVDBC, 0, NULL);
	date[0] = '\0';
	mscntoi(&blk[4], &i1, 4);
	mon = (blk[8] - '0') * 10 + blk[9] - '0';
	if (i1 >= 1980 && i1 <= 2047) {
		i1 -= 1980;
		days = (i1 >> 2) * 1461;
		i1 &= 0x03;
		if (i1) days += i1 * 365 + 1;
		else if (mon > 2) days++;
		i1 = (blk[10] - '0') * 10 + blk[11] - '0';
		days += daystodate[mon - 1] + i1 - 1;
		if (!prpget("clock", "day", NULL, NULL, &ptr, 0) && strlen(ptr) == 21) strcpy(day, ptr);
		if (!prpget("clock", "month", NULL, NULL, &ptr, 0) && strlen(ptr) == 36) strcpy(mth, ptr);
		memset(date, ' ', 24);
		memcpy(date, &day[((days + 2) % 7) * 3], 3);
		memcpy(&date[4], &mth[(mon - 1) * 3], 3);
		memcpy(&date[8], &blk[10], 2);
		memcpy(&date[11], &blk[4], 4);
		memcpy(&date[16], &blk[12], 2);
		memcpy(&date[19], &blk[14], 2);
		memcpy(&date[22], &blk[16], 2);
		date[18] = date[21] = ':';
		date[24] = '\n';
		date[25] = '\0';
	}

	dspstring("File type is version ");
	//itoa(c1, verwork, 10);
	sprintf(verwork, "%u", (unsigned int)c1);
	dspstring(verwork);
	dspstring(" dbc object file\n");
	if (date[0]) {
		dspstring("Object was compiled ");
		dspstring(date);
	}
}

static void fputch5(UCHAR *s)  /* left squeeze s of blanks and display it */
{
	UCHAR work[6];

	memcpy(work, s, 5);
	work[5] = '\0';
	s = work;
	while (*s == ' ') s++;
	dspstring((CHAR *) s);
}

static void fputoff(OFFSET n)
{
	CHAR s[32];

	mscofftoa(n, s);
	dspstring(s);
}

static void fputint(INT n)
{
	CHAR s[17];

	mscitoa(n, s);
	dspstring(s);
}

/* BADISI */
static void badisi(INT e, OFFSET pos)
{
	CHAR work[17];

	mscofftoa(pos, work);
	dspstring(errormsg[e]);
	dspstring(work);
	dspchar('\n');
	cfgexit();
	exit(1);
}

/* USAGE */
static void usage()
{
	dspstring("FILECHK command  " RELEASEPROGRAM RELEASE COPYRIGHT);
	dspchar('\n');
	dspstring("Usage:  filechk file [-A] [-C] [-CFG=cfgfile] [-D] [-E] [-F] [-I] [-J[R]]\n");
	dspstring("                [-L=n] [-O=optfile] [-P] [-T] [-X]\n");
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
