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
#define INC_ERRNO
#include "includes.h"
#include <assert.h>

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

#if OS_WIN32
#if _MSC_VER >= 1800
#include <shlwapi.h>
#include <VersionHelpers.h>
#endif
#endif

#include "base.h"
#include "fio.h"
#include "sio.h"
#include "util.h"

#define AIM_HEADER_SIZE		1024
#define ISI_HEADER_SIZE		1024
#define RIO_BUF_SIZE		6
#define BUFFER_SIZE			(32 << 10)
#define MIN_ISI_BLK_SIZE	512
#define MAX_ISI_BLK_SIZE	16384
#define KEYTAG_POS_SIZE		12

/* index specific define and variables */
#define IXLEVELMAX 32
#define IXBLKMAX 32

#define FIELD_SPEC		0x01
#define FIELD_STRING	0x02
#define FIELD_BLANKS	0x03
#define FIELD_DATE		0x04
#define FIELD_DATEALT1	0x05
#define FIELD_DATEREV	0x06
#define FIELD_DATEDUP	0x07
#define FIELD_EXCLUDE	0x10
#define FIELD_ASCEND	0x20
#define FIELD_DESCEND	0x40
#define FIELD_NUMERIC	0x80

#define SELECT_EQUAL	0x01
#define SELECT_GREATER	0x02
#define SELECT_LESS		0x04
#define SELECT_STRING	0x10
#define SELECT_OR		0x20

#define SHARE_READWRITE		1
#define SHARE_READONLY		2
#define SHARE_READACCESS	3

#define VERBOSE_NORMAL	0x01
#define VERBOSE_EXTRA	0x02

/* 'A' args */
#define ARG_APPEND		0x00000001
#define ARG_ALLOCATE	0x00000001
/* 'B' args */
#define ARG_BLKSIZE		0x00000002
/* 'C' args */
#define ARG_COMPRESSED	0x00000004
#define ARG_EDTCFGPATH	0x00000004
/* 'D' args */
#define ARG_DISTINCT	0x00000008
#define ARG_DELETE		0x00000008
#define ARG_DECODE		0x00000008
#define ARG_DBCPATH		0x00000008
#define ARG_DUPLICATES	0x00000008
/* 'E' args */
#define ARG_REINDEX		0x00000010
/* 'F' args */
#define ARG_FIXED		0x00000020
#define ARG_OPENPATH	0x00000020
#define ARG_IGNOREDUP	0x00000020
#define ARG_FIELD		0x00000020
#define ARG_POSNTAG		0x00000020
/* 'G' args */
#define ARG_GROUP		0x00000040
/* 'H' args */
#define ARG_HEADER		0x00000080
/* 'I' args */
#define ARG_TABINSERT	0x00000100
/* 'J' args */
#define ARG_SHARE		0x00000200
/* 'K' args */
#define ARG_KEYTAGFILE	0x00000400
#define ARG_KEYTAG		0x00000400
/* 'L' args */
#define ARG_LENGTH		0x00000800
/* 'M' args */
#define ARG_MATCH		0x00001000
#define ARG_MAXLENGTH	0x00001000
/* 'N' args */
#define ARG_PRIMARY		0x00002000
#define ARG_TRANSLATE	0x00002000
#define ARG_COUNT		0x00002000
/* 'P' args */
#define ARG_SELECT		0x00004000
#define ARG_PREPPATH	0x00004000
/* 'Q' args */
#define ARG_2PASSSORT	0x00008000
/* 'R' args */
#define ARG_RENAME		0x00010000
#define ARG_SPEED		0x00010000
#define ARG_REVERSE		0x00010000
/* 'S' args */
#define ARG_RECLAIM		0x00020000
#define ARG_NONSELECT	0x00020000
#define ARG_SRCPATH		0x00020000
#define ARG_STABLE		0x00020000
/* 'T' args */
#define ARG_TEXTSRCH	0x00040000
#define ARG_TYPEBIN		0x00040000
#define ARG_TYPEDATA	0x00080000
#define ARG_TYPEDOS		0x000C0000
#define ARG_TYPEMAC		0x00100000
#define ARG_TYPESTD		0x00140000
#define ARG_TYPETEXT	0x00180000
#define ARG_TYPEMASK	0x001C0000
/* 'U' args */
#define ARG_UNIQUE		0x00200000
/* 'V' args */
#define ARG_VERBOSE		0x00400000
/* 'W' args */
#define ARG_WORKPATH	0x00800000
#define ARG_WORKFILE	0x00800000
/* 'X' args */
#define ARG_REVSELECT	0x01000000
/* more 'X' args */
#define ARG_SECONDARY	0x02000000
#define ARG_TABOFFSET	0x02000000
/* 'Y' args */
#define ARG_EOFERROR	0x04000000
/* 'Z' args */
#define ARG_ZVALUE		0x08000000
/* digit args */
#define ARG_FLDSPEC		0x10000000
#define ARG_RECSPEC		0x10000000
/* file names */
#define ARG_1STFILE		0x20000000
#define ARG_2NDFILE		0x40000000
/* OR */
#define ARG_OR			0x80000000

typedef struct fieldinfo_struct {
	INT type;
	INT pos;
	INT len;
	INT off;
} FIELDINFO;

typedef struct recinfo_struct {
	OFFSET start;
	OFFSET end;
} RECINFO;

typedef struct utilargs_struct {
	UINT flags;
	INT addflag;
	INT allocate;
	INT blksize;
	INT datecutoff;
	INT groupchar;
	INT groupflag;
	INT grouppos;
	INT length;
	INT matchchar;
	INT maxlength;
	INT shareflag;
	INT verboseflag;
	INT zvalue;
	OFFSET count;
	OFFSET primary;
	OFFSET secondary;
	CHAR firstfile[MAX_NAMESIZE];
	CHAR secondfile[MAX_NAMESIZE];
	CHAR xtrafile1[MAX_NAMESIZE];
	CHAR xtrafile2[MAX_NAMESIZE];
	CHAR workfile[MAX_NAMESIZE];
	FIELDINFO **fieldinfo;
	INT fieldcnt;
	INT fieldsize;
	RECINFO **recinfo;
	INT reccnt;
	INT recsize;
	FIELDINFO **selectinfo;
	INT selectcnt;
	INT selectsize;
	CHAR **stringinfo;
	INT stringcnt;
	INT stringsize;
	INT **tabinfo;
	INT tabcnt;
	INT tabsize;
} UTILARGS;

typedef struct ixblkinfo_struct {
	UCHAR *buf;
	INT mod;
	INT lru;
	OFFSET pos;
} IXBLKINFO;

static UCHAR ixlastkey[XIO_MAX_KEYSIZE + 2];
static INT ixblksize;
static IXBLKINFO ixblk[IXBLKMAX];
static INT ixblkmax, ixblkhi;
static INT ixlastuse;
static OFFSET ixtopblk, ixhighblk;
static INT ixleafblk, ixleafpos;
static INT ixbranchhi;
static INT ixcollateflag;
static UCHAR ixpriority[UCHAR_MAX + 1];

static CHAR utilerrorstring[256];

static INT utilaimdex(UTILARGS *options);
static UINT axhash(INT h1, INT h2, INT h3, INT zvalue);
static INT utilbuild(UTILARGS *options);
static INT utilcopy(UTILARGS *options);
static INT utilcreate(UTILARGS *options);
static INT utildelete(UTILARGS *options);
static INT utilencode(UTILARGS *options);
static INT utilerase(UTILARGS *options);
static INT utilexist(UTILARGS *options);
static INT utilindex(UTILARGS *options);
static INT ixins(INT outhandle, UCHAR *key, INT size);
static INT ixcompkey(UCHAR *key1, UCHAR *key2, INT size);
static INT ixnewblk(INT outhandle, INT *blknum);
static INT ixgetblk(INT outhandle, OFFSET pos, INT *blknum);
static INT utilreformat(UTILARGS *options);
static INT utilrename(UTILARGS *options);
static INT utilsort(UTILARGS *options);
static INT utilselect(UTILARGS *options, UCHAR *buffer, INT recsize);
static INT utilstrtoargs(CHAR *buffer, INT length);
static INT utilparseargs(INT type, CHAR *args, UTILARGS *options);
static INT utilfieldparm(INT type, INT utiltype, CHAR *ptr, UTILARGS *options);
static INT utilrecparm(CHAR *ptr, UTILARGS *options);
static INT utilselectparm(CHAR *ptr, UTILARGS *options);


INT utilfixupargs(INT type, CHAR *srcbuf, INT srclen, CHAR *destbuf, INT destlen)
{
	INT backslashcnt, cnt, quoteflag, spaceflag;
	CHAR chr;

	if (srclen == -1) srclen = (INT)strlen(srcbuf);
	if (type & UTIL_FIXUP_CHOP) {
		type &= ~UTIL_FIXUP_CHOP;
		while (srclen > 0 && *srcbuf == ' ') {
			srcbuf++;
			srclen--;
		}
		if (srclen > 0) while (srcbuf[srclen - 1] == ' ') srclen--;
	}
	if (srclen <= 0) return 0;
	cnt = 0;
	if (type == UTIL_FIXUP_UNIX) {
		for (quoteflag = 0; srclen; srcbuf++, srclen--) {
			chr = *srcbuf;
			if (chr == '\\') {
				if (!--srclen) break;
				chr = *(++srcbuf);
				if (chr == '"') {
					if (cnt >= destlen) return -1;
					destbuf[cnt++] = '\\';
				}
				else if (isspace(chr) && !quoteflag) {
					if (cnt + 2 > destlen) return -1;
					destbuf[cnt++] = '"';
					destbuf[cnt++] = chr;
					chr = '"';
				}
			}
			else if (chr == '"') quoteflag ^= 1;
			if (cnt >= destlen) return -1;
			destbuf[cnt++] = chr;
		}
	}
	else if (type == UTIL_FIXUP_ARGV) {
		for (spaceflag = FALSE; srclen; srcbuf++, srclen--) {
			chr = *srcbuf;
			if (chr == '"') {
				if (cnt >= destlen) return -1;
				destbuf[cnt++] = '\\';
			}
			else if (isspace(chr) && !spaceflag) {
				spaceflag = TRUE;
				if (cnt >= destlen) return -1;
				destbuf[cnt++] = '"';
			}
			if (cnt >= destlen) return -1;
			destbuf[cnt++] = chr;
		}
		if (spaceflag) {
			if (cnt >= destlen) return -1;
			destbuf[cnt++] = '"';
		}
	}
	else if (type & UTIL_FIXUP_OLDWIN) {
		for (backslashcnt = 0; ; srcbuf++, srclen--) {
			if (srclen) {
				chr = *srcbuf;
				if (chr == '\\') {
					backslashcnt++;
					continue;
				}
			}
			if (backslashcnt) {
				if (!srclen || chr != '"') backslashcnt <<= 1;  /* backslashes are not considered escape character */
				else backslashcnt = ((backslashcnt - 1) << 1) + 1;  /* last backslash is to escape the quote */
				if (cnt + backslashcnt > destlen) return -1;
				memset(destbuf + cnt, '\\', backslashcnt);
				cnt += backslashcnt;
				backslashcnt = 0;
			}
			if (!srclen) break;
			if (cnt >= destlen) return -1;
			destbuf[cnt++] = chr;
		}
	}
	else {  /* copy src to dest */
		if (srclen > destlen) return -1;
		for ( ; cnt < srclen; cnt++) destbuf[cnt] = srcbuf[cnt];
	}
	return cnt;
}

/**
 * Can return UTILERR_xxxx or 0 for success
 */
INT utility(INT type, CHAR *args, INT length)
{
	INT i1, worktype;
	CHAR buffer[2048];
	UTILARGS options;

	*utilerrorstring = '\0';

	/* process arguments */
	if (length == -1) length = (INT)strlen(args);
	if (length > (INT) sizeof(buffer) - 2) {
		return UTILERR_LONGARG;
	}
	assert((UINT) length <= sizeof(buffer));
	memcpy(buffer, args, length);
	i1 = utilstrtoargs(buffer, length);
	if (i1) return i1;	
	if (type & (UTIL_BUILD | UTIL_COPY | UTIL_REFORMAT | UTIL_RENAME | UTIL_SORT)) worktype = type | UTIL_1STFILE | UTIL_2NDFILE;
	else if (type & (UTIL_AIMDEX | UTIL_ENCODE | UTIL_INDEX)) worktype = type | UTIL_1STFILE | UTIL_2NDCHECK;
	else worktype = type | UTIL_1STFILE;
	memset(&options, 0, sizeof(options));
	i1 = utilparseargs(worktype, buffer, &options);
	if (i1) return i1;
	if ((worktype & UTIL_1STFILE) && !(options.flags & ARG_1STFILE)) {
		return UTILERR_BADARG;
	}
	if ((worktype & UTIL_2NDFILE) && !(options.flags & ARG_2NDFILE)) {
		return UTILERR_BADARG;
	}
	if ((worktype & UTIL_2NDCHECK) && !(options.flags & ARG_2NDFILE)) {
		strcpy(options.secondfile, options.firstfile);
		miofixname(options.secondfile, "", FIXNAME_EXT_REPLACE);
		if (!options.secondfile[0]) {
			return UTILERR_BADARG;
		}
		options.flags |= ARG_2NDFILE;
	}

	switch (type) {
	case UTIL_AIMDEX:
		i1 = utilaimdex(&options);
		break;
	case UTIL_BUILD:
		i1 = utilbuild(&options);
		break;
	case UTIL_COPY:
		i1 = utilcopy(&options);
		break;
	case UTIL_CREATE:
		i1 = utilcreate(&options);
		break;
	case UTIL_DELETE:
		i1 = utildelete(&options);
		break;
	case UTIL_ENCODE:
		i1 = utilencode(&options);
		break;
	case UTIL_ERASE:
		i1 = utilerase(&options);
		break;
	case UTIL_EXIST:
		i1 = utilexist(&options);
		break;
	case UTIL_INDEX:
		i1 = utilindex(&options);
		break;
	case UTIL_REFORMAT:
		i1 = utilreformat(&options);
		break;
	case UTIL_RENAME:
		i1 = utilrename(&options);
		break;
	case UTIL_SORT:
		i1 = utilsort(&options);
		break;
	default:
		i1 = UTILERR_BADARG;
		break;
	}
	memfree((UCHAR **) options.fieldinfo);
	memfree((UCHAR **) options.recinfo);
	memfree((UCHAR **) options.selectinfo);
	memfree((UCHAR **) options.stringinfo);
	memfree((UCHAR **) options.tabinfo);
	return i1;
}

INT utilgeterrornum(INT errornum, CHAR *errorstr, INT size)
{
	INT length;
	CHAR work[64], *ptr;

	switch (errornum) {
	case UTILERR_OPEN:
		ptr = "open failure";
		break;
	case UTILERR_CREATE:
		ptr = "create failure";
		break;
	case UTILERR_FIND:
		ptr = "find failure";
		break;
	case UTILERR_CLOSE:
		ptr = "close failure";
		break;
	case UTILERR_DELETE:
		ptr = "delete failure";
		break;
	case UTILERR_READ:
		ptr = "read failure";
		break;
	case UTILERR_WRITE:
		ptr = "write failure";
		break;
	case UTILERR_CHANGED:
		ptr = "aimdex failure";
		break;
	case UTILERR_LONGARG:
		ptr = "argument(s) too long";
		break;
	case UTILERR_BADARG:
		ptr = "bad argument";
		break;
	case UTILERR_BADHDR:
		ptr = "invalid header";
		break;
	case UTILERR_BADFILE:
		ptr = "file failure";
		break;
	case UTILERR_INTERNAL:
		ptr = "internal error";
		break;
	case UTILERR_NOMEM:
		ptr = "memory allocation failure";
		break;
	case UTILERR_BADEOF:
		ptr = "bad EOF";
		break;
	case UTILERR_BADOPT:
		ptr = "option file failure";
		break;
	case UTILERR_TRANSLATE:
		ptr = "translation failure";
		break;
	case UTILERR_SORT:
		ptr = "sort failure";
		break;
	case UTILERR_DUPS:
		ptr = "duplicate keys failure";
		break;
	case UTILERR_RENAME:
		ptr = "rename failure";
		break;
	default:
		strcpy(work, "util error = ");
		mscitoa(errornum, work + 13);
		ptr = work;
		break;
	}
	length = (INT)strlen(ptr);
	if (--size >= 0) {
		if (size > length) size = length;
		if (size) strncpy(errorstr, ptr, size);
		errorstr[size] = '\0';
	}
	return length;
}

INT utilgeterror(CHAR *errorstr, INT size)
{
	INT length;

	length = (INT)strlen(utilerrorstring);
	if (--size >= 0) {
		if (size > length) size = length;
		if (size) memcpy(errorstr, utilerrorstring, size);
		errorstr[size] = '\0';
		utilerrorstring[0] = '\0';
	}
	return length;
}

INT utilputerror(CHAR *errorstr)
{
	INT length;

	length = (INT)strlen(errorstr);
	if (length > (INT) sizeof(utilerrorstring) - 1) length = sizeof(utilerrorstring) - 1;
	if (length) memcpy(utilerrorstring, errorstr, length);
	utilerrorstring[length] = '\0';
	return length;
}

static INT utilaimdex(UTILARGS *options)
{
	INT i1, i2, i3, i4, arecsize = 1, delflag, hashbyte, highkey, inhandle, memsize;
	INT openflag, outhandle, rc, reclen, recsize, slots, version, worksize, zvalue;
	OFFSET memory, pos, pos2, reccnt, rechigh, recnum, slotsize, workoff;
	CHAR *ptr;
	UCHAR c1, c2, c3, hashbit, casemap[UCHAR_MAX + 1], work[MAX_NAMESIZE];
	UCHAR *buffer, *hashbuf, **bufptr, **pptr;
	FIELDINFO *fieldinfo;
	struct rtab *r;

	bufptr = memalloc(RIO_MAX_RECSIZE + 4, 0);
	if (bufptr == NULL) return UTILERR_NOMEM;
	hashbuf = NULL;
	rc = inhandle = outhandle = 0;

	miofixname(options->secondfile, ".aim", FIXNAME_EXT_ADD);
	if (options->flags & (ARG_REINDEX | ARG_RENAME)) {
		outhandle = fioopen(options->secondfile, FIO_M_EXC | FIO_P_TXT);
		if (outhandle < 0) {
			utilputerror(fioerrstr(outhandle));
			rc = UTILERR_OPEN;
			goto utilaimdexend;
		}
		buffer = *bufptr;
		i1 = fioread(outhandle, 0, buffer, AIM_HEADER_SIZE);
		if (i1 < 0) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_READ;
			goto utilaimdexend;
		}

		/* check for what seems to be a valid aim file */
		if (i1 != AIM_HEADER_SIZE) {
			utilputerror("incomplete aim header");
			rc = UTILERR_BADHDR;
			goto utilaimdexend;
		}
		if (buffer[99] != ' ') {
			version = buffer[99] - '0';
			if (buffer[98] != ' ') version += (buffer[98] - '0') * 10;
		}
		else version = 0;
		c1 = buffer[59];
		if (version > 10) c1 = DBCDEL;
		else if (version >= 7) {
			if (c1 != 'V' && c1 != 'F' && c1 != 'S') c1 = DBCDEL;
		}
		else {
			if (c1 != 'V' && c1 != 'F') c1 = DBCDEL;
			if (!version) i1 = 512;
		}
		if (buffer[0] != 'A' || buffer[100] != DBCEOR || c1 == DBCDEL) {
			utilputerror("invalid character");
			rc = UTILERR_BADHDR;
			goto utilaimdexend;
		}

		if (options->flags & ARG_RENAME) {
			i3 = (INT)strlen(options->firstfile);
			if (version >= 9) {
				i4 = i3 + 101;
				for (i2 = 101; i2 < i1 && buffer[i2] != DBCEOR; i2++);
				if (i2 == i1) {
					utilputerror("invalid character");
					rc = UTILERR_BADHDR;
					goto utilaimdexend;
				}
				memmove(buffer + i4, buffer + i2, i1);
				if (i4 < i2) memset(buffer + i1 - (i2 - i4), DBCDEL, i2 - i4);
			}
			else {
				if (i3 > 64) {
					utilputerror("text name too long for pre-v9 aim");
					rc = UTILERR_BADARG;
					goto utilaimdexend;
				}
				memset(buffer + 101, ' ', 64);
			}
			memcpy(buffer + 101, options->firstfile, i3);

			if (options->flags & ARG_TEXTSRCH) buffer[55] = 'T';
			i1 = fiowrite(outhandle, 0, buffer, i1);
			if (i1) {
				utilputerror(fioerrstr(i1));
				rc = UTILERR_WRITE;
			}
			goto utilaimdexend;
		}
		fioclose(outhandle);
		outhandle = 0;

		/* get arguments */
		if (version < 8) {
			utilputerror("not valid option with pre-v8 aim");
			rc = UTILERR_BADARG;
			goto utilaimdexend;
		}
		buffer[i1] = DBCDEL;
		for (i1 = 101; buffer[i1] != DBCEOR && buffer[i1] != DBCDEL; i1++);  /* skip name */
		if (buffer[i1] == DBCDEL) {
			utilputerror("invalid character");
			rc = UTILERR_BADHDR;
			goto utilaimdexend;
		}
		for (i2 = ++i1; buffer[i2] != DBCDEL; i2++)
			if (buffer[i2] == DBCEOR) buffer[i2] = 0;
		buffer[i2] = 0;  /* just in case */
		options->flags &= ARG_1STFILE | ARG_2NDFILE;
		options->fieldcnt = 0;
		options->selectcnt = 0;
		options->stringcnt = 0;
		rc = utilparseargs(UTIL_AIMDEX, (CHAR *)(buffer + i1), options);
		if (rc) goto utilaimdexend;
	}

	if (!(options->flags & ARG_FLDSPEC)) {
		utilputerror("no keyspec specified");
		rc = UTILERR_BADARG;
		goto utilaimdexend;
	}
	if ((options->flags & ARG_RECLAIM) && !(options->flags & ARG_FIXED)) {
		utilputerror("fixed must be specified to use space reclamation");
		rc = UTILERR_BADARG;
		goto utilaimdexend;
	}
	if ((options->flags & ARG_EOFERROR) && (options->flags & ARG_SHARE) && options->shareflag != SHARE_READONLY) {
		utilputerror("-Y and -J options are mutually exclusive");
		rc = UTILERR_BADARG;
		goto utilaimdexend;
	}

	if (!(options->flags & ARG_ALLOCATE)) options->allocate = 2 << 10;
	if (!(options->flags & ARG_DISTINCT)) {
		pptr = fiogetopt(FIO_OPT_CASEMAP);
		if (pptr != NULL) memcpy(casemap, *pptr, (UCHAR_MAX + 1) * sizeof(UCHAR));
		else for (i1 = 0; i1 <= UCHAR_MAX; i1++) casemap[i1] = (UCHAR) toupper(i1);
	}
	if (!(options->flags & ARG_MATCH)) options->matchchar = '?';
	if (!(options->flags & ARG_SECONDARY)) options->secondary = 0;
	if (options->flags & ARG_SHARE) {
		if (options->shareflag == SHARE_READONLY) openflag = RIO_M_SRO | RIO_P_TXT | RIO_T_ANY;
		else if (options->shareflag == SHARE_READACCESS) openflag = RIO_M_SRA | RIO_P_TXT | RIO_T_ANY;
		else openflag = RIO_M_SHR | RIO_P_TXT | RIO_T_ANY;
	}
	else openflag = RIO_M_ERO | RIO_P_TXT | RIO_T_ANY;
	if (options->flags & ARG_ZVALUE) zvalue = options->zvalue;
	else zvalue = 199;

	/* open the input file */
	inhandle = rioopen(options->firstfile, openflag, RIO_BUF_SIZE, RIO_MAX_RECSIZE);
	if (inhandle < 0) {
		utilputerror(fioerrstr(inhandle));
		rc = UTILERR_OPEN;
		goto utilaimdexend;
	}

	/* get file size, record size and record count */
	rioeofpos(inhandle, &pos);
	if (options->flags & ARG_FIXED) {  /* fixed length records */
		while ((recsize = rioget(inhandle, *bufptr, RIO_MAX_RECSIZE)) == -2);
		if (recsize <= -3) {
			utilputerror(fioerrstr(recsize));
			rc = UTILERR_READ;
			goto utilaimdexend;
		}
		if (!(options->flags & ARG_LENGTH)) {
			if (recsize == -1) {
				utilputerror("fixed length must be supplied on an empty file");
				rc = UTILERR_BADARG;
				goto utilaimdexend;
			}
			options->length = recsize;
		}
		else if (recsize != -1 && recsize != options->length) {
			utilputerror("fixed length does not match file");
			rc = UTILERR_BADARG;
			goto utilaimdexend;
		}
		reclen = options->length;
		riosetpos(inhandle, 0);

		arecsize = options->length + 1;
		if (riotype(inhandle) == RIO_T_DOS) arecsize++;
		if (pos % arecsize) {
			if (riotype(inhandle) != RIO_T_ANY || (pos % (arecsize + 1))) {
				utilputerror("fixed length does not match file size");
				rc = UTILERR_BADFILE;
				goto utilaimdexend;
			}
			arecsize++;
		}
		reccnt = pos / arecsize;

		pptr = fiogetwptr(inhandle);
		if (pptr == NULL) {
			utilputerror("fiogetwptr failed");
			rc = UTILERR_INTERNAL;
			goto utilaimdexend;
		}
		r = (struct rtab *) *pptr;
		if (r->type != 'R') {
			utilputerror("not rtab type");
			rc = UTILERR_INTERNAL;
			goto utilaimdexend;
		}
		r->opts |= RIO_FIX | RIO_UNC;
	}
	else {
		reclen = RIO_MAX_RECSIZE;
		reccnt = (pos >> 8) + 1;
	}

	/* calculate the number of records in the primary & secondary extent */
	if (reccnt < 128) reccnt = 128;
	if (options->flags & ARG_PRIMARY) {
		if (options->addflag) reccnt += options->primary;
		else if (reccnt < options->primary) reccnt = options->primary;
	}
	if (reccnt & 0x1F) reccnt = (reccnt & ~0x1F) + 0x20;  /* round up to next even 32 */

	/* allocate memory (use malloc as this will reduce MEMALLOC requirements */
	slots = zvalue + ((options->flags & ARG_RECLAIM) ? 1 : 0);
	slotsize = reccnt >> 3;
	if (options->flags & ARG_ALLOCATE) memsize = options->allocate << 10;
	else memsize = 2 << 20;
	if (memsize < slots) memsize = slots;
	memory = slotsize * slots;
	if ((OFFSET) memsize > memory) memsize = (INT) memory;
	while (memsize >= slots) {
		i1 = (INT)(memory / memsize);
		if (memory % memsize) i1++;
		memsize = (INT)(memory / i1);
		if (memory % i1) memsize++;
		if (memsize % slots) i1 = slots - (memsize % slots);
		else i1 = 0;
		hashbuf = (UCHAR *) malloc(memsize + i1);
		if (hashbuf != NULL) break;
		memsize -= slots - i1;
	}
	if (memsize < slots) {
		utilputerror("malloc failed");
		rc = UTILERR_NOMEM;
		goto utilaimdexend;
	}
	memsize += i1;
	worksize = (INT)(memsize / slots);
	rechigh = (OFFSET) worksize << 3;
	workoff = 0;
	memset(hashbuf, 0, slots * worksize);

	/* build the header block */
	buffer = *bufptr;
	buffer[0] = 'a';  /* will be 'A' if successful */
	memset(buffer + 1, 0, 27);
	mscoffto6x(reccnt, buffer + 13);  /* current extension record count */
	memset(buffer + 28, ' ', 72);
	msciton(zvalue, buffer + 32, 5);  /* number of slots (z value) */
	if (options->flags & ARG_FIXED) i1 = options->length;
	else i1 = 256;
	msciton(i1, buffer + 41, 5);  /* record length */
	if (options->flags & ARG_TEXTSRCH) buffer[55] = 'T';
	if (options->flags & ARG_DISTINCT) buffer[57] = 'Y';
	else buffer[57] = 'N';
	buffer[58] = (UCHAR) options->matchchar;
	if (options->flags & ARG_RECLAIM) buffer[59] = 'S';
	else if (options->flags & ARG_FIXED) buffer[59] = 'F';
	else buffer[59] = 'V';
	mscoffto6x(options->secondary, buffer + 60);  /* cmd line secondary rec count */
	buffer[99] = '9';  /* kept at 9 to be more backward compatable */
	buffer[100] = DBCEOR;
	i1 = (INT)strlen(options->firstfile);
	memcpy(buffer + 101, options->firstfile, i1);
	i1 += 101;
	buffer[i1++] = DBCEOR;

	/* insert arguments */
	for (highkey = 0, i2 = 0; i2 < options->fieldcnt; i2++) {
		fieldinfo = *options->fieldinfo + i2;
		i1 += mscitoa(fieldinfo->pos + 1, (CHAR *)(buffer + i1));
		if (fieldinfo->len > 1) {
			buffer[i1++] = '-';
			i1 += mscitoa(fieldinfo->pos + fieldinfo->len, (CHAR *)(buffer + i1));
		}
		if (fieldinfo->type & FIELD_EXCLUDE) buffer[i1++] = 'X';
		else if (highkey < fieldinfo->pos + fieldinfo->len) highkey = fieldinfo->pos + fieldinfo->len;
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_ALLOCATE) {
		buffer[i1++] = '-';
		buffer[i1++] = 'A';
		buffer[i1++] = '=';
		i1 += mscitoa(options->allocate, (CHAR *)(buffer + i1));
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_DISTINCT) {
		buffer[i1++] = '-';
		buffer[i1++] = 'D';
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_FIXED) {
		buffer[i1++] = '-';
		buffer[i1++] = 'F';
		if (options->flags & ARG_LENGTH) {
			buffer[i1++] = '=';
			i1 += mscitoa(options->length, (CHAR *)(buffer + i1));
		}
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_SHARE) {
		buffer[i1++] = '-';
		buffer[i1++] = 'J';
		if (options->shareflag == SHARE_READONLY) buffer[i1++] = 'R';
		else if (options->shareflag == SHARE_READACCESS) buffer[i1++] = 'A';
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_MATCH) {
		buffer[i1++] = '-';
		buffer[i1++] = 'M';
		buffer[i1++] = '=';
		buffer[i1++] = (UCHAR) options->matchchar;
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_PRIMARY) {
		buffer[i1++] = '-';
		buffer[i1++] = 'N';
		buffer[i1++] = '=';
		if (options->addflag) buffer[i1++] = '+';
		i1 += mscofftoa(options->primary, (CHAR *)(buffer + i1));
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_SELECT) {
		for (i2 = 0; i2 < options->selectcnt; i2++) {
			fieldinfo = *options->selectinfo + i2;
			if (fieldinfo->type & SELECT_OR) {
				buffer[i1++] = 'O';
				buffer[i1++] = 'R';
				buffer[i1++] = DBCEOR;
			}
			buffer[i1++] = '-';
			buffer[i1++] = 'P';
			i1 += mscitoa(fieldinfo->pos + 1, (CHAR *)(buffer + i1));
			if (fieldinfo->type & SELECT_STRING) {
				buffer[i1++] = '-';
				i1 += mscitoa(fieldinfo->pos + fieldinfo->len, (CHAR *)(buffer + i1));
			}
			if (fieldinfo->type & (SELECT_LESS | SELECT_GREATER)) {
				if (!(fieldinfo->type & SELECT_LESS)) buffer[i1++] = 'G';
				else if (!(fieldinfo->type & SELECT_GREATER)) buffer[i1++] = 'L';
				else buffer[i1++] = 'N';
			}
			if ((fieldinfo->type & SELECT_EQUAL) || (fieldinfo->type & (SELECT_LESS | SELECT_GREATER)) == (SELECT_LESS | SELECT_GREATER)) {
				buffer[i1++] = 'E';
				if (!(fieldinfo->type & (SELECT_LESS | SELECT_GREATER))) buffer[i1++] = 'Q';
			}
			else buffer[i1++] = 'T';
			memcpy(buffer + i1, (*options->stringinfo) + fieldinfo->off, fieldinfo->len);
			i1 += fieldinfo->len;
			buffer[i1++] = DBCEOR;
		}
	}
	if (options->flags & ARG_RECLAIM) {
		buffer[i1++] = '-';
		buffer[i1++] = 'S';
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_TEXTSRCH) {
		buffer[i1++] = '-';
		buffer[i1++] = 'T';
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_REVSELECT) {
		buffer[i1++] = '-';
		buffer[i1++] = 'X';
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_SECONDARY) {
		buffer[i1++] = '-';
		buffer[i1++] = 'X';
		buffer[i1++] = '=';
		i1 += mscofftoa(options->secondary, (CHAR *)(buffer + i1));
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_EOFERROR) {
		buffer[i1++] = '-';
		buffer[i1++] = 'Y';
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_ZVALUE) {
		buffer[i1++] = '-';
		buffer[i1++] = 'Z';
		buffer[i1++] = '=';
		i1 += mscitoa(zvalue, (CHAR *)(buffer + i1));
		buffer[i1++] = DBCEOR;
	}
	if (i1 > AIM_HEADER_SIZE) {
		utilputerror("arguments too long to store in header");
		rc = UTILERR_LONGARG;
		goto utilaimdexend;
	}
	memset(buffer + i1, DBCDEL, AIM_HEADER_SIZE - i1);

	/* create the aimdex file */
	i2 = fiogetflags() & FIO_FLAG_UTILPREPDOS;
	if (i2) {  /* change prep directory to be same as input file */
		ptr = fioname(inhandle);
		if (ptr != NULL) {
			i1 = fioaslash(ptr) + 1;
			if (i1) memcpy(work, ptr, i1);
			work[i1] = 0;
			pptr = fiogetopt(FIO_OPT_PREPPATH);
			if (pptr != NULL) strcpy((CHAR *) buffer + AIM_HEADER_SIZE, (CHAR *) *pptr);
			fiosetopt(FIO_OPT_PREPPATH, work);
		}
		else i2 = 0;
	}
	outhandle = fioopen(options->secondfile, FIO_M_PRP | FIO_P_TXT);
	buffer = *bufptr;
	if (i2) {  /* restore prep directory */
		if (pptr != NULL) fiosetopt(FIO_OPT_PREPPATH, buffer + AIM_HEADER_SIZE);
		else fiosetopt(FIO_OPT_PREPPATH, NULL);
	}
	if (outhandle < 0) {
		utilputerror(fioerrstr(outhandle));
		rc = UTILERR_CREATE;
		goto utilaimdexend;
	}

	/* write the header */
	i1 = fiowrite(outhandle, 0, buffer, AIM_HEADER_SIZE);
	if (i1) {
		utilputerror(fioerrstr(i1));
		rc = UTILERR_WRITE;
		goto utilaimdexend;
	}

	/* process the records */
	delflag = FALSE;  /* use to flag first deleted record */
	for ( ; ; ) {
		recsize = rioget(inhandle, buffer, reclen);
		riolastpos(inhandle, &pos);
		if (recsize < highkey) {
			if (recsize == -1) {  /* end of file */
				if (options->flags & ARG_EOFERROR) {
					rioeofpos(inhandle, &pos2);
					if (pos != pos2) {
						rc = UTILERR_BADEOF;
						goto utilaimdexend;
					}
				}
				break;
			}
			if (recsize == -2) {  /* deleted record */
				if (!(options->flags & ARG_RECLAIM)) continue;
				recnum = pos / arecsize;
				if (!delflag) {  /* modify header block */
					mscoffto6x(recnum + 1, buffer);
					i1 = fiowrite(outhandle, 7, buffer, 6);
					if (i1) {
						utilputerror(fioerrstr(i1));
						rc = UTILERR_WRITE;
						goto utilaimdexend;
					}
					delflag = TRUE;
				}
				while (recnum >= rechigh) {
					if ((OFFSET) worksize == slotsize) {
						i1 = fiowrite(outhandle, 1024, hashbuf, (INT)(slots * slotsize));
						if (i1) {
							utilputerror(fioerrstr(i1));
							rc = UTILERR_WRITE;
							goto utilaimdexend;
						}
						workoff = slotsize;
					}
					else {
						pos2 = workoff + 1024;
						for (i1 = i3 = 0; i1 < slots; i1++) {
							i2 = fiowrite(outhandle, pos2, hashbuf + i3, worksize);
							if (i2) {
								utilputerror(fioerrstr(i2));
								rc = UTILERR_WRITE;
								goto utilaimdexend;
							}
							i3 += worksize;
							pos2 += slotsize;
						}
						workoff += worksize;
						if (workoff + worksize > slotsize) worksize = (INT)(slotsize - workoff);
						rechigh = (workoff + worksize) << 3;
						memset(hashbuf, 0, slots * worksize);
					}
				}

				/* set the Z+1 slot on */
				i1 = zvalue * worksize + (INT)((recnum >> 3) - workoff);
				hashbuf[i1] |= 1 << ((UINT) recnum & 0x07);
				continue;
			}
			if (recsize <= -3) {
				utilputerror(fioerrstr(recsize));
				rc = UTILERR_READ;
				goto utilaimdexend;
			}
			memset(buffer + recsize, ' ', highkey - recsize);
		}
		if ((options->flags & ARG_SELECT) && !utilselect(options, buffer, recsize)) continue;
		if (options->flags & ARG_FIXED) recnum = pos / arecsize;
		else if (pos) recnum = (pos - 1) / 256;
		else recnum = 0L;
		if (recnum >= reccnt) {
			utilputerror("too many records added while processing");
			rc = UTILERR_CHANGED;
			goto utilaimdexend;
		}

		while (recnum >= rechigh) {
			if ((OFFSET) worksize == slotsize) {
				i1 = fiowrite(outhandle, 1024, hashbuf, (INT)(slots * slotsize));
				if (i1) {
					utilputerror(fioerrstr(i1));
					rc = UTILERR_WRITE;
					goto utilaimdexend;
				}
				workoff = slotsize;
			}
			else {
				pos2 = workoff + 1024;
				for (i1 = i3 = 0; i1 < slots; i1++) {
					i2 = fiowrite(outhandle, pos2, hashbuf + i3, worksize);
					if (i2) {
						utilputerror(fioerrstr(i2));
						rc = UTILERR_WRITE;
						goto utilaimdexend;
					}
					i3 += worksize;
					pos2 += slotsize;
				}
				workoff += worksize;
				if (workoff + worksize > slotsize) worksize = (INT)(slotsize - workoff);
				rechigh = (workoff + worksize) << 3;
				memset(hashbuf, 0, slots * worksize);
			}
		}

		hashbyte = (INT)((recnum >> 3) - workoff);
		hashbit = (UCHAR)(1 << ((UINT) recnum & 0x07));
		for (fieldinfo = *options->fieldinfo, i1 = 0; i1 < options->fieldcnt; fieldinfo++, i1++) {
			if (fieldinfo->type & FIELD_EXCLUDE) continue;
			i2 = fieldinfo->pos;
			i3 = fieldinfo->len;
			if (!(options->flags & ARG_DISTINCT)) for (i4 = i2; i4 < i3; i4++) buffer[i4] = casemap[buffer[i4]];
			if ((c1 = buffer[i2]) != ' ') {  /* leftmost character non-blank */
				hashbuf[axhash(i1, 31, c1, zvalue) * worksize + hashbyte] |= hashbit;
				if (i3 >= 3  && ((c2 = buffer[i2 + 1]) != ' '))
					hashbuf[axhash(i1, c1, c2, zvalue) * worksize + hashbyte] |= hashbit;
			}
			if (i3 >= 2 && ((c1 = buffer[i2 + i3 - 1]) != ' ')) {
				hashbuf[axhash(i1, 30, c1, zvalue) * worksize + hashbyte] |= hashbit;
				if (i3 >= 3 && ((c2 = buffer[i2 + i3 - 2]) != ' ')) 
					hashbuf[axhash(i1, c2, c1, zvalue) * worksize + hashbyte] |= hashbit;
			}
			if (i3 >= 4) {
				i3 += i2 - 2;
				while (i2 < i3) {
					if ((c1 = buffer[i2++]) == ' ') continue;
					if ((c2 = buffer[i2]) != ' ') {
						if ((c3 = buffer[i2 + 1]) == ' ') i2 += 2;
						else hashbuf[axhash(c1, c2, c3, zvalue) * worksize + hashbyte] |= hashbit;
					}
					else i2++;
				}
			}
		}
	}
	while (workoff < slotsize) {
		if ((OFFSET) worksize == slotsize) {
			i1 = fiowrite(outhandle, 1024, hashbuf, (INT)(slots * slotsize));
			if (i1) {
				utilputerror(fioerrstr(i1));
				rc = UTILERR_WRITE;
				goto utilaimdexend;
			}
			workoff = slotsize;
		}
		else {
			pos2 = workoff + 1024;
			for (i1 = i3 = 0; i1 < slots; i1++) {
				i2 = fiowrite(outhandle, pos2, hashbuf + i3, worksize);
				if (i2) {
					utilputerror(fioerrstr(i2));
					rc = UTILERR_WRITE;
					goto utilaimdexend;
				}
				i3 += worksize;
				pos2 += slotsize;
			}
			workoff += worksize;
			if (workoff + worksize > slotsize) worksize = (INT)(slotsize - workoff);
			rechigh = (workoff + worksize) << 3;
			memset(hashbuf, 0, slots * worksize);
		}
	}
	i1 = fiowrite(outhandle, 0, (UCHAR *) "A", 1);
	if (i1) {
		utilputerror(fioerrstr(i1));
		rc = UTILERR_WRITE;
	}

	/* clean up */
utilaimdexend:
	if (hashbuf != NULL) free(hashbuf);
	if (outhandle > 0) {
		i1 = fioclose(outhandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_CLOSE;
		}
	}
	if (inhandle > 0) {
		i1 = rioclose(inhandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_CLOSE;
		}
	}
	memfree(bufptr);
	return rc;
}

static UINT axhash(INT h1, INT h2, INT h3, INT zvalue)
{
	UINT i1;

	i1 = ((h1 & 0x1F) << 10) | ((h2 & 0x1F) << 5) | (h3 & 0x1F);
	return i1 % zvalue;
}

static INT utilbuild(UTILARGS *options)
{
	INT i1, i2, createflag1, createflag2, inhandle, nonselhandle;
	INT openflag, outhandle, rc, recsize;
	OFFSET eofpos, from, pos, recnum, to = 0;
	CHAR *ptr;
	UCHAR transmap[256], work[MAX_NAMESIZE], *buffer, **bufptr, **pptr;

	if ((options->flags & ARG_COMPRESSED) && (options->flags & ARG_TYPEMASK) && (options->flags & ARG_TYPEMASK) != ARG_TYPESTD) {
		utilputerror("-C are mutually exclusive with non-STD file type");
		return UTILERR_BADARG;
	}
	if ((options->flags & ARG_EOFERROR) && (options->flags & ARG_SHARE) && options->shareflag != SHARE_READONLY) {
		utilputerror("-Y and -J options are mutually exclusive");
		return UTILERR_BADARG;
	}

	if (!(options->flags & ARG_SELECT)) options->flags &= ~ARG_NONSELECT;
	if (options->flags & ARG_SHARE) {
		if (options->shareflag == SHARE_READONLY) openflag = RIO_M_SRO | RIO_P_TXT | RIO_T_ANY;
		else if (options->shareflag == SHARE_READACCESS) openflag = RIO_M_SRA | RIO_P_TXT | RIO_T_ANY;
		else openflag = RIO_M_SHR | RIO_P_TXT | RIO_T_ANY;
	}
	else openflag = RIO_M_ERO | RIO_P_TXT | RIO_T_ANY;
	if ((options->flags & ARG_TYPEMASK) == ARG_TYPEBIN) {
		createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_BIN | RIO_UNC;
		createflag2 = RIO_M_EXC | RIO_P_TXT | RIO_T_BIN | RIO_UNC;
	}
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPEDATA) {
		createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_DAT | RIO_UNC;
		createflag2 = RIO_M_EXC | RIO_P_TXT | RIO_T_DAT | RIO_UNC;
	}
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPEDOS) {
		createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_DOS | RIO_UNC;
		createflag2 = RIO_M_EXC | RIO_P_TXT | RIO_T_DOS | RIO_UNC;
	}
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPEMAC) {
		createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_MAC | RIO_UNC;
		createflag2 = RIO_M_EXC | RIO_P_TXT | RIO_T_MAC | RIO_UNC;
	}
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPETEXT) {
		createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_TXT | RIO_UNC;
		createflag2 = RIO_M_EXC | RIO_P_TXT | RIO_T_TXT | RIO_UNC;
	}
	else if (options->flags & ARG_COMPRESSED) {
		createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_STD;
		createflag2 = RIO_M_EXC | RIO_P_TXT | RIO_T_STD;
	}
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPESTD) {
		createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_STD | RIO_UNC;
		createflag2 = RIO_M_EXC | RIO_P_TXT | RIO_T_STD | RIO_UNC;
	}
	else {
		createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_STD | RIO_UNC;
		createflag2 = RIO_M_EXC | RIO_P_TXT | RIO_T_ANY | RIO_UNC;
	}
	if (options->flags & ARG_TRANSLATE) {  /* read translate map file */
		if ((i1 = fioopen(options->xtrafile1, FIO_M_SRO | FIO_P_SRC)) < 0) {
			utilputerror(fioerrstr(i1));
			return UTILERR_TRANSLATE;
		}
		if (fioread(i1, 0, transmap, UCHAR_MAX + 1) != UCHAR_MAX + 1) {
			utilputerror("unable to read 256 bytes");
			fioclose(i1);
			return UTILERR_TRANSLATE;
		}
		fioclose(i1);
	}

	bufptr = memalloc(RIO_MAX_RECSIZE + 4, 0);
	if (bufptr == NULL) return UTILERR_NOMEM;
	rc = inhandle = nonselhandle = outhandle = 0;

	/* open the input file */
	inhandle = rioopen(options->firstfile, openflag, RIO_BUF_SIZE, RIO_MAX_RECSIZE);
	if (inhandle < 0) {
		utilputerror(fioerrstr(inhandle));
		rc = UTILERR_OPEN;
		goto utilbuildend;
	}

	/* open or create the output file */
	i2 = fiogetflags() & FIO_FLAG_UTILPREPDOS;
utilbuild1:
	if (i2) {  /* change prep directory to be same as input file */
		ptr = fioname(inhandle);
		if (ptr != NULL) {
			i1 = fioaslash(ptr) + 1;
			if (i1) memcpy(work, ptr, i1);
			work[i1] = 0;
			pptr = fiogetopt(FIO_OPT_PREPPATH);
			if (pptr != NULL) strcpy((CHAR *) *bufptr, (CHAR *) *pptr);
			fiosetopt(FIO_OPT_PREPPATH, work);
		}
		else i2 = 0;
	}
	if (!(options->flags & ARG_APPEND) || (outhandle = rioopen(options->secondfile, createflag2, RIO_BUF_SIZE, RIO_MAX_RECSIZE)) == ERR_FNOTF)
		outhandle = rioopen(options->secondfile, createflag1, RIO_BUF_SIZE, RIO_MAX_RECSIZE);
	if (i2) {  /* restore prep directory */
		if (pptr != NULL) fiosetopt(FIO_OPT_PREPPATH, *bufptr);
		else fiosetopt(FIO_OPT_PREPPATH, NULL);
	}
	if (outhandle < 0) {
		utilputerror(fioerrstr(outhandle));
		rc = UTILERR_CREATE;
		goto utilbuildend;
	}
	if (options->flags & ARG_APPEND) {
		if (riotype(outhandle) == RIO_T_ANY) {
			for ( ; ; ) {
				recsize = rioget(outhandle, *bufptr, RIO_MAX_RECSIZE);
				if (recsize >= -1) break;
				if (recsize == -2) continue;
				utilputerror(fioerrstr(recsize));
				rc = UTILERR_READ;
				goto utilbuildend;
			}
			if (riotype(outhandle) == RIO_T_ANY) {
				rioclose(outhandle);
				options->flags &= ~ARG_APPEND;
				createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_TXT | RIO_UNC;
				goto utilbuild1;
			}
		}
		rioeofpos(outhandle, &pos);
		riosetpos(outhandle, pos);
	}

	/* create non-selection criteria output file */
	if (options->flags & ARG_NONSELECT) {
		if (i2) {  /* change prep directory to be same as input file */
			ptr = fioname(inhandle);
			if (ptr != NULL) {
				i1 = fioaslash(ptr) + 1;
				if (i1) memcpy(work, ptr, i1);
				work[i1] = 0;
				pptr = fiogetopt(FIO_OPT_PREPPATH);
				if (pptr != NULL) strcpy((CHAR *) *bufptr, (CHAR *) *pptr);
				fiosetopt(FIO_OPT_PREPPATH, work);
			}
			else i2 = 0;
		}
		nonselhandle = rioopen(options->xtrafile2, createflag1, RIO_BUF_SIZE - 1, RIO_MAX_RECSIZE);
		if (i2) {  /* restore prep directory */
			if (pptr != NULL) fiosetopt(FIO_OPT_PREPPATH, *bufptr);
			else fiosetopt(FIO_OPT_PREPPATH, NULL);
		}
		if (nonselhandle < 0) {
			utilputerror(fioerrstr(nonselhandle));
			rc = UTILERR_CREATE;
			goto utilbuildend;
		}
	}

	/* loop on each record spec */
	buffer = *bufptr;
	recnum = 0;
	i1 = 0;
	do {
		if (options->flags & ARG_RECSPEC) {
			from = (*options->recinfo)[i1].start;
			to = (*options->recinfo)[i1].end;
			if (recnum > from) {
				riosetpos(inhandle, 0);
				recnum = 0;
			}
		}
		do {
			recsize = rioget(inhandle, buffer, RIO_MAX_RECSIZE);
			if (recsize < 0) {
				if (recsize == -1) {  /* end of file */
					if (options->flags & ARG_EOFERROR) {
						riolastpos(inhandle, &pos);
						rioeofpos(inhandle, &eofpos);
						if (pos != eofpos) {
							rc = UTILERR_BADEOF;
							goto utilbuildend;
						}
					}
					break;
				}
				if (recsize == -2) continue;
				utilputerror(fioerrstr(recsize));
				rc = UTILERR_READ;
				goto utilbuildend;
			}
			if (!(options->flags & ARG_RECSPEC) || ++recnum > from) {
/*** CODE: SHOULD TRANSLATE HAPPEN BEFORE OR AFTER SELECT ***/
				if (options->flags & ARG_TRANSLATE)
					for (i2 = 0; i2 < recsize; i2++) buffer[i2] = transmap[buffer[i2]];
				if ((options->flags & ARG_SELECT) && !utilselect(options, buffer, recsize)) {
					if (!(options->flags & ARG_NONSELECT)) continue;
					i2 = rioput(nonselhandle, buffer, recsize);
				}
				else i2 = rioput(outhandle, buffer, recsize);
				if (i2) {
					utilputerror(fioerrstr(i2));
					rc = UTILERR_WRITE;
					goto utilbuildend;
				}
			}
		} while (!(options->flags & ARG_RECSPEC) || recnum < to);
	} while ((options->flags & ARG_RECSPEC) && ++i1 < options->reccnt);

	/* clean up */
utilbuildend:
	if (nonselhandle > 0) {
		i1 = rioclose(nonselhandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_CLOSE;
		}
	}
	if (outhandle > 0) {
		i1 = rioclose(outhandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_CLOSE;
		}
	}
	if (inhandle > 0) {
		i1 = rioclose(inhandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_CLOSE;
		}
	}
	memfree(bufptr);
	return rc;
}

static INT utilcopy(UTILARGS *options)
{
	INT i1, i2, inhandle, outhandle, openflag, rc;
	OFFSET filepos;
	CHAR *ptr;
	UCHAR work[MAX_NAMESIZE], *buffer, **bufptr, **pptr;

	if (options->flags & ARG_SHARE) {
		if (options->shareflag == SHARE_READONLY) openflag = FIO_M_SRO | FIO_P_TXT;
		else if (options->shareflag == SHARE_READACCESS) openflag = FIO_M_SRA | FIO_P_TXT;
		else openflag = FIO_M_SHR | FIO_P_TXT;
	}
	else openflag = FIO_M_ERO | FIO_P_TXT;

	bufptr = memalloc(BUFFER_SIZE, 0);
	if (bufptr == NULL) return UTILERR_NOMEM;
	rc = inhandle = outhandle = 0;

	/* open the input file */
	miofixname(options->firstfile, ".txt", FIXNAME_EXT_ADD);
	inhandle = fioopen(options->firstfile, openflag);
	if (inhandle < 0) {
		utilputerror(fioerrstr(inhandle));
		rc = UTILERR_OPEN;
		goto utilcopyend;
	}

	/* create the output file */
	i2 = fiogetflags() & FIO_FLAG_UTILPREPDOS;
	if (i2) {  /* change prep directory to be same as input file */
		ptr = fioname(inhandle);
		if (ptr != NULL) {
			i1 = fioaslash(ptr) + 1;
			if (i1) memcpy(work, ptr, i1);
			work[i1] = 0;
			pptr = fiogetopt(FIO_OPT_PREPPATH);
			if (pptr != NULL) strcpy((CHAR *) *bufptr, (CHAR *) *pptr);
			fiosetopt(FIO_OPT_PREPPATH, work);
		}
		else i2 = 0;
	}
	miofixname(options->secondfile, ".txt", FIXNAME_EXT_ADD);
	outhandle = fioopen(options->secondfile, FIO_M_PRP | FIO_P_TXT);
	if (i2) {  /* restore prep directory */
		if (pptr != NULL) fiosetopt(FIO_OPT_PREPPATH, *bufptr);
		else fiosetopt(FIO_OPT_PREPPATH, NULL);
	}
	if (outhandle < 0) {
		utilputerror(fioerrstr(outhandle));
		rc = UTILERR_CREATE;
		goto utilcopyend;
	}

	/* copy the file */
	buffer = *bufptr;
	filepos = 0;
	while ((i2 = fioread(inhandle, filepos, buffer, BUFFER_SIZE)) > 0) {
		i1 = fiowrite(outhandle, filepos, buffer, i2);
		if (i1) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_WRITE;
			goto utilcopyend;
		}
		filepos += i2;
	}
	if (i2 < 0) {
		utilputerror(fioerrstr(i2));
		rc = UTILERR_READ;
	}

	/* clean up */
utilcopyend:
	if (outhandle > 0) {
		i1 = fioclose(outhandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_CLOSE;
		}
	}
	if (inhandle > 0) {
		if (!rc && (options->flags & ARG_DELETE)) i1 = fiokill(inhandle);
		else i1 = fioclose(inhandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			if (options->flags & ARG_DELETE) rc = UTILERR_DELETE;
			else rc = UTILERR_CLOSE;
		}
	}
	memfree(bufptr);
	return rc;
}

static INT utilcreate(UTILARGS *options)
{
	INT i1, createflag, outhandle, rc;
	UCHAR **bufptr = NULL;

	if ((options->flags & ARG_COMPRESSED) && (options->flags & ARG_TYPEMASK) && (options->flags & ARG_TYPEMASK) != ARG_TYPESTD) {
		utilputerror("-C are mutually exclusive with non-STD file type");
		return UTILERR_BADARG;
	}
	if (options->flags & ARG_COUNT) {
		if (!(options->flags & ARG_LENGTH)) options->length = 80;
	}
	else options->count = options->length = 0;
	if ((options->flags & ARG_TYPEMASK) == ARG_TYPEBIN) createflag = RIO_M_PRP | RIO_P_TXT | RIO_T_BIN | RIO_UNC;
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPEDATA) createflag = RIO_M_PRP | RIO_P_TXT | RIO_T_DAT | RIO_UNC;
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPEDOS) createflag = RIO_M_PRP | RIO_P_TXT | RIO_T_DOS | RIO_UNC;
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPEMAC) createflag = RIO_M_PRP | RIO_P_TXT | RIO_T_MAC | RIO_UNC;
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPETEXT) createflag = RIO_M_PRP | RIO_P_TXT | RIO_T_TXT | RIO_UNC;
	else if (options->flags & ARG_COMPRESSED) createflag = RIO_M_PRP | RIO_P_TXT | RIO_T_STD;
	else createflag = RIO_M_PRP | RIO_P_TXT | RIO_T_STD | RIO_UNC;

	if (options->flags & ARG_COUNT) {
		bufptr = memalloc(options->length + 4, 0);
		if (bufptr == NULL) return UTILERR_NOMEM;
		memset(*bufptr, ' ', options->length);
		i1 = RIO_BUF_SIZE;
	}
	else i1 = 0;
	rc = outhandle = 0;

	/* create the output file */
	outhandle = rioopen(options->firstfile, createflag, i1, options->length);
	if (outhandle < 0) {
		utilputerror(fioerrstr(outhandle));
		rc = UTILERR_CREATE;
		goto utilcreateend;
	}

	/* possibly write records */
	if (bufptr != NULL)
		while (options->count-- > 0) {
			i1 = rioput(outhandle, *bufptr, options->length);
			if (i1) {
				utilputerror(fioerrstr(i1));
				rc = UTILERR_WRITE;
				break;
			}
		}

	/* clean up */
utilcreateend:
	if (outhandle > 0) {
		i1 = rioclose(outhandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_CLOSE;
		}
	}
	if (bufptr != NULL) memfree(bufptr);
	return rc;
}

static INT utilencode(UTILARGS *options)
{
	INT i1, i2, i3, i4, bufpos, endflag, inhandle, openflag, outhandle, rc, recsize;
	OFFSET filepos;
	UCHAR c1, c2, c3, c4, record[128], *buffer, **bufptr;

	/* allocate buffer, bufsize must be a factor of 240 */
	bufptr = memalloc((BUFFER_SIZE / 240) * 240, 0);
	if (bufptr == NULL) return UTILERR_NOMEM;
	rc = inhandle = outhandle = 0;

	/* open the input file */
	if (options->flags & ARG_DECODE) {
		if (options->flags & ARG_SHARE) {
			if (options->shareflag == SHARE_READONLY) openflag = RIO_M_SRO | RIO_P_TXT | RIO_T_ANY;
			else if (options->shareflag == SHARE_READACCESS) openflag = RIO_M_SRA | RIO_P_TXT | RIO_T_ANY;
			else openflag = RIO_M_SHR | RIO_P_TXT | RIO_T_ANY;
		}
		else openflag = RIO_M_ERO | RIO_P_TXT | RIO_T_ANY;
		miofixname(options->firstfile, ".enc", FIXNAME_EXT_ADD);
		inhandle = rioopen(options->firstfile, openflag, RIO_BUF_SIZE, 80);
	}
	else {
		if (options->flags & ARG_SHARE) {
			if (options->shareflag == SHARE_READONLY) openflag = FIO_M_SRO | FIO_P_TXT;
			else if (options->shareflag == SHARE_READACCESS) openflag = FIO_M_SRA | FIO_P_TXT;
			else openflag = FIO_M_SHR | FIO_P_TXT;
		}
		else openflag = FIO_M_ERO | FIO_P_TXT;
		miofixname(options->firstfile, ".dbc", FIXNAME_EXT_ADD);
		inhandle = fioopen(options->firstfile, openflag);
	}
	if (inhandle < 0) {
		utilputerror(fioerrstr(inhandle));
		rc = UTILERR_OPEN;
		goto utilencodeend;
	}

	/* create the output file */
	if (options->flags & ARG_DECODE) {
		miofixname(options->secondfile, ".dbc", FIXNAME_EXT_ADD);
		outhandle = fioopen(options->secondfile, FIO_M_PRP | FIO_P_TXT);
	}
	else {
		miofixname(options->secondfile, ".enc", FIXNAME_EXT_ADD);
		outhandle = rioopen(options->secondfile, RIO_M_PRP | RIO_P_TXT | RIO_T_TXT, RIO_BUF_SIZE, 80);
	}
	if (outhandle < 0) {
		utilputerror(fioerrstr(outhandle));
		rc = UTILERR_CREATE;
		goto utilencodeend;
	}

	buffer = *bufptr;
	if (options->flags & ARG_DECODE) {
		bufpos = 0;
		filepos = 0;
		endflag = FALSE;
		for ( ; ; ) {
			recsize = rioget(inhandle, record, 80);
			if (recsize == -1) break;
			if (recsize == -2 || endflag) {
				utilputerror("deleted or short record");
				rc = UTILERR_BADFILE;
				goto utilencodeend;
			}
			if (recsize < -2) {
				utilputerror(fioerrstr(recsize));
				rc = UTILERR_READ;
				goto utilencodeend;
			}
			if (recsize < 80) {
				if (!recsize || (recsize & 0x03) == 0x01) {
					utilputerror("short record");
					rc = UTILERR_BADFILE;
					goto utilencodeend;
				}
				memset(record + recsize, ' ', 4);
				endflag = TRUE;
			}
			i1 = 0;
			do {
				c1 = (UCHAR)(record[i1++] - ' ');
				c2 = (UCHAR)(record[i1++] - ' ');
				c3 = (UCHAR)(record[i1++] - ' ');
				c4 = (UCHAR)(record[i1++] - ' ');
				buffer[bufpos++] = (UCHAR)((c1 & 0x3F) + ((c2 & 0x03) << 6));
				buffer[bufpos++] = (UCHAR)(((UINT)(c2 & 0x3C) >> 2) + ((c3 & 0x0F) << 4));
				buffer[bufpos++] = (UCHAR)(((UINT)(c3 & 0x30) >> 4) + ((c4 & 0x3F) << 2));
			} while (i1 < recsize);
			if (bufpos == (BUFFER_SIZE / 240) * 240 || endflag) {
				if (i1 > recsize) bufpos -= i1 - recsize;
				i1 = fiowrite(outhandle, filepos, buffer, bufpos);
				if (i1) {
					utilputerror(fioerrstr(i1));
					rc = UTILERR_WRITE;
					goto utilencodeend;
				}
				filepos += bufpos;
				bufpos = 0;
			}
		}
		if (bufpos) {
			i1 = fiowrite(outhandle, filepos, buffer, bufpos);
			if (i1) {
				utilputerror(fioerrstr(i1));
				rc = UTILERR_WRITE;
				goto utilencodeend;
			}
		}
	}
	else {  /* encode */
		filepos = 0;
		do {
			i2 = fioread(inhandle, filepos, buffer, (BUFFER_SIZE / 240) * 240);
			if (!i2) break;
			if (i2 < 0) {
				utilputerror(fioerrstr(i2));
				rc = UTILERR_READ;
				goto utilencodeend;
			}
			filepos += i2;
			if (i2 % 3) memset(buffer + i2, 0, 3);
			i3 = i4 = 0;
			do {
				c1 = buffer[i3++];
				c2 = buffer[i3++];
				c3 = buffer[i3++];
				record[i4++] = (UCHAR)((c1 & 0x3F) + ' ');
				record[i4++] = (UCHAR)((c1 >> 6) + ((c2 & 0x0F) << 2) + ' ');
				record[i4++] = (UCHAR)((c2 >> 4) + ((c3 & 0x03) << 4) + ' ');
				record[i4++] = (UCHAR)((c3 >> 2) + ' ');
				if (i4 == 80 || i3 >= i2) {
					if (i3 > i2) i4 -= i3 - i2;
					i1 = rioput(outhandle, record, i4);
					if (i1) {
						utilputerror(fioerrstr(i1));
						rc = UTILERR_WRITE;
						goto utilencodeend;
					}
					i4 = 0;
				}
			} while (i3 < i2);
		} while (i2 == (BUFFER_SIZE / 240) * 240);
	}

	/* clean up */
utilencodeend:
	if (outhandle > 0) {
		if (options->flags & ARG_DECODE) i1 = fioclose(outhandle);
		else i1 = rioclose(outhandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_CLOSE;
		}
	}
	if (inhandle > 0) {
		if (options->flags & ARG_DECODE) i1 = rioclose(inhandle);
		else i1 = fioclose(inhandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_CLOSE;
		}
	}
	memfree(bufptr);
	return rc;
}

static INT utilerase(UTILARGS *options)
{
	INT i1, inhandle;

	miofixname(options->firstfile, ".txt", FIXNAME_EXT_ADD);
	inhandle = fioopen(options->firstfile, FIO_M_EXC | FIO_P_TXT);
	if (inhandle < 0) {
		if (inhandle != ERR_FNOTF) {
			utilputerror(fioerrstr(inhandle));
			return UTILERR_OPEN;
		}
	}
	else {
		i1 = fiokill(inhandle);
		if (i1 < 0) {
			utilputerror(fioerrstr(i1));
			return UTILERR_DELETE;
		}
	}
	return 0;
}

/*** CODE: TO MAKE DBCUTIL WORK, WILL NEED TO DISPLAY FILES ***/
static INT utilexist(UTILARGS *options)
{
	INT i1, i2, cnt, pathcnt, pathlist[16];
	CHAR work[MAX_NAMESIZE], *path, *ptr1, *ptr2, **pathpptr, **pptr, **volpptr = NULL;

#if OS_WIN32
	DWORD err;
	HANDLE filehandle;
	WIN32_FIND_DATA fileinfo;
	PVOID FsRedirValue = NULL;
#endif

	if (!(options->flags & (ARG_EDTCFGPATH | ARG_DBCPATH | ARG_OPENPATH | ARG_PREPPATH | ARG_SRCPATH | ARG_WORKPATH)))
		options->flags |= ARG_OPENPATH;
	pathcnt = 0;
	if (options->flags & ARG_OPENPATH) pathlist[pathcnt++] = FIO_OPT_OPENPATH;
	if (options->flags & ARG_PREPPATH) pathlist[pathcnt++] = FIO_OPT_PREPPATH;
	if (options->flags & ARG_SRCPATH) pathlist[pathcnt++] = FIO_OPT_SRCPATH;
	if (options->flags & ARG_DBCPATH) pathlist[pathcnt++] = FIO_OPT_DBCPATH;
	if (options->flags & ARG_EDTCFGPATH) pathlist[pathcnt++] = FIO_OPT_EDITCFGPATH;
	if (options->flags & ARG_WORKPATH) pathlist[pathcnt++] = 0;

	miofixname(options->firstfile, ".txt", FIXNAME_EXT_ADD | FIXNAME_PAR_DBCVOL | FIXNAME_PAR_MEMBER);
	miogetname(&ptr1, &ptr2);
	if (*ptr1) {
		if (fiocvtvol(ptr1, &volpptr)) {
			utilputerror("volume not specified");
			return UTILERR_FIND;
		}
		pathlist[0] = -1;
		pathcnt = 1;
	}
	else if (fioaslash(options->firstfile) >= 0) {
		pathlist[0] = 0;
		pathcnt = 1;
	}

	for (cnt = 0, i1 = 0; i1 < pathcnt; i1++) {
		if (pathlist[i1]) {
			if (pathlist[i1] != -1) pathpptr = (CHAR **) fiogetopt(pathlist[i1]);
			else pathpptr = volpptr;
			if (pathpptr != NULL) i1 = (INT)strlen(*pathpptr);
			else i1 = 0;
		}
		else i1 = 0;
		pptr = (CHAR **) memalloc(i1 + 1, 0);
		if (pptr == NULL) return UTILERR_NOMEM;
		if (i1) strcpy(*pptr, *pathpptr);
		else **pptr = '\0';
		path = *pptr;

#if 0
		dspflag = FALSE;
#endif
		miostrscan(path, work);
		do {
			fioaslashx(work);
			strcat(work, options->firstfile);

#if OS_WIN32
			if (IsOS(OS_WOW6432)) {Wow64DisableWow64FsRedirection(&FsRedirValue);}
			filehandle = FindFirstFile(work, &fileinfo);
			if (IsOS(OS_WOW6432)) {Wow64RevertWow64FsRedirection(FsRedirValue);}
			if (filehandle != INVALID_HANDLE_VALUE) {
				FindClose(filehandle);
				i2 = 0;
			}
			else {
				err = GetLastError();
				if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND ||
					err == ERROR_INVALID_DRIVE) continue;
				i2 = -1;
			}
#endif

#if OS_UNIX
			i2 = access(work, 000);
			if (i2 == -1) {
				if (errno == ENOENT) continue;
				if (errno == ENOTDIR) continue;
			}
#endif

			if (i2 != -1) cnt++;
		} while (!miostrscan(path, work));
		memfree((UCHAR **) pptr);
	}
	if (!cnt) {
		utilputerror("file does not exist");
		return UTILERR_FIND;
	}
	return 0;
}

static INT utilindex(UTILARGS *options)
{
	INT i1, i2, i3, i4, deloff, dupcnt, duphandle, firstflag, highkey, inhandle;
	INT openflag, outhandle, rc, reclen, recsize, size = 0, size1, version;
	OFFSET delblk, eofpos, pos;
	OFFSET reccnt;
	CHAR *ptr, *workdir, *workfile;
	UCHAR c1, work[MAX_NAMESIZE];
	UCHAR *buffer, *rec, **bufptr, **pptr;
	FIELDINFO *fieldinfo;
	SIOKEY sortkey[2], *sortkeyptr;
	struct rtab *r;
#if OS_WIN32
	DWORD attrib;
#endif
#if OS_UNIX
	struct stat statbuf;
#endif

#if ((MAX_ISI_BLK_SIZE * 4) > (RIO_MAX_RECSIZE + MAX_ISI_BLK_SIZE))
	bufptr = memalloc(MAX_ISI_BLK_SIZE * 4, 0);
#else
	bufptr = memalloc(RIO_MAX_RECSIZE + MAX_ISI_BLK_SIZE, 0);
#endif
	if (bufptr == NULL) return UTILERR_NOMEM;
	rc = inhandle = outhandle = duphandle = dupcnt = 0;
	miofixname(options->secondfile, ".isi", FIXNAME_EXT_ADD);

	if (options->flags & (ARG_REINDEX | ARG_RENAME)) {
		outhandle = fioopen(options->secondfile, FIO_M_EXC | FIO_P_TXT);
		if (outhandle < 0) {
			utilputerror(fioerrstr(outhandle));
			rc = UTILERR_OPEN;
			goto utilindexend;
		}
		buffer = *bufptr;
		i1 = fioread(outhandle, 0, buffer, ISI_HEADER_SIZE);
		if (i1 < 0) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_READ;
			goto utilindexend;
		}

		/* check for what seems to be a valid isi file */
		if (i1 < 512) {
			utilputerror("incomplete isi header");
			rc = UTILERR_BADHDR;
			goto utilindexend;
		}
		if (buffer[99] != ' ') {
			version = buffer[99] - '0';
			if (buffer[98] != ' ') version += (buffer[98] - '0') * 10;
		}
		else version = 0;
		c1 = buffer[57];
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
		if (buffer[0] != 'I' || buffer[100] != DBCEOR || c1 == DBCDEL) {
			utilputerror("invalid character");
			rc = UTILERR_BADHDR;
			goto utilindexend;
		}
		mscntoi(buffer + 41, &i2, 5);
		if (i1 > i2) i1 = i2;

		if (options->flags & ARG_RENAME) {
			i3 = (INT)strlen(options->firstfile);
			if (version >= 9) {
				i4 = i3 + 101;
				for (i2 = 101; i2 < i1 && buffer[i2] != DBCEOR; i2++);
				if (i2 == i1) {
					utilputerror("invalid character");
					rc = UTILERR_BADHDR;
					goto utilindexend;
				}
				memmove(buffer + i4, buffer + i2, i1);
				if (i4 < i2) memset(buffer + i1 - (i2 - i4), DBCDEL, i2 - i4);
			}
			else {
				if (i3 > 64) {
					utilputerror("text name too long for pre-v9 aim");
					rc = UTILERR_BADARG;
					goto utilindexend;
				}
				memset(buffer + 101, ' ', 64);
			}
			memcpy(buffer + 101, options->firstfile, i3);

			if (options->flags & ARG_TEXTSRCH) buffer[55] = 'T';
			i1 = fiowrite(outhandle, 0, buffer, i1);
			if (i1) {
				utilputerror(fioerrstr(i1));
				rc = UTILERR_WRITE;
			}
			goto utilindexend;
		}
		fioclose(outhandle);
		outhandle = 0;

		/* get arguments */
		if (version < 8) {
			utilputerror("not valid option with pre-v8 isi");
			rc = UTILERR_BADARG;
			goto utilindexend;
		}
		buffer[i1] = DBCDEL;
		for (i1 = 101; buffer[i1] != DBCEOR && buffer[i1] != DBCDEL; i1++);  /* skip name */
		if (buffer[i1] == DBCDEL) {
			utilputerror("invalid character");
			rc = UTILERR_BADHDR;
			goto utilindexend;
		}
		for (i2 = ++i1; buffer[i2] != DBCDEL; i2++)
			if (buffer[i2] == DBCEOR) buffer[i2] = 0;
		buffer[i2] = 0;  /* just in case */
		options->flags &= ARG_1STFILE | ARG_2NDFILE;
		options->xtrafile1[0] = 0;
		options->fieldcnt = 0;
		options->selectcnt = 0;
		options->stringcnt = 0;
		rc = utilparseargs(UTIL_INDEX, (CHAR *)(buffer + i1), options);
		if (rc) goto utilindexend;
	}

	if (!(options->flags & ARG_FLDSPEC) && !(options->flags & ARG_KEYTAGFILE)) {
		utilputerror("no keyspec specified");
		rc = UTILERR_BADARG;
		goto utilindexend;
	}
	if ((options->flags & ARG_DUPLICATES) && (options->flags & ARG_IGNOREDUP)) {
		utilputerror("-D and -F options are mutually exclusive");
		rc = UTILERR_BADARG;
		goto utilindexend;
	}
	if ((options->flags & ARG_KEYTAGFILE) && (options->flags & ARG_RECLAIM) && !(options->flags & ARG_LENGTH)) {
		utilputerror("-S requires record length with -K");
		rc = UTILERR_BADARG;
		goto utilindexend;
	}
	if ((options->flags & ARG_EOFERROR) && (options->flags & ARG_SHARE) && options->shareflag != SHARE_READONLY) {
		utilputerror("-Y and -J options are mutually exclusive");
		rc = UTILERR_BADARG;
		goto utilindexend;
	}

	if (!(options->flags & ARG_ALLOCATE)) options->allocate = 0;
	if (!(options->flags & ARG_BLKSIZE)) options->blksize = 1024;
	if (options->flags & ARG_SHARE) {
		if (options->shareflag == SHARE_READONLY) openflag = RIO_M_SRO | RIO_P_TXT | RIO_T_ANY;
		else if (options->shareflag == SHARE_READACCESS) openflag = RIO_M_SRA | RIO_P_TXT | RIO_T_ANY;
		else openflag = RIO_M_SHR | RIO_P_TXT | RIO_T_ANY;
	}
	else openflag = RIO_M_ERO | RIO_P_TXT | RIO_T_ANY;
	pptr = fiogetopt(FIO_OPT_COLLATEMAP);
	if (pptr != NULL) {
		memcpy(ixpriority, *pptr, (UCHAR_MAX + 1) * sizeof(UCHAR));
		ixcollateflag = TRUE;
	}
	else ixcollateflag = FALSE;

	/* open the input file */
	if (options->flags & ARG_KEYTAGFILE) ptr = options->xtrafile2;
	else ptr = options->firstfile;
	inhandle = rioopen(ptr, openflag, RIO_BUF_SIZE, RIO_MAX_RECSIZE);
	if (inhandle < 0) {
		utilputerror(fioerrstr(inhandle));
		rc = UTILERR_OPEN;
		goto utilindexend;
	}
	/* get file size, record size and record count */
	if ((options->flags & ARG_RECLAIM) || (options->flags & ARG_KEYTAGFILE)) {  /* fixed length records or get key tag length */
		while ((recsize = rioget(inhandle, *bufptr, RIO_MAX_RECSIZE)) == -2);
		if (recsize <= -3) {
			utilputerror(fioerrstr(recsize));
			rc = UTILERR_READ;
			goto utilindexend;
		}
		if (!(options->flags & ARG_KEYTAGFILE)) {  /* fixed length records */
			if (!(options->flags & ARG_LENGTH)) {
				if (recsize == -1) {
					utilputerror("fixed length must be supplied on an empty file");
					rc = UTILERR_BADARG;
					goto utilindexend;
				}
				options->length = recsize;
			}
			else if (recsize != -1 && recsize != options->length) {
				utilputerror("fixed length does not match file");
				rc = UTILERR_BADARG;
				goto utilindexend;
			}
			reclen = options->length;
		}
		else if (recsize != -1) {
			if (recsize <= KEYTAG_POS_SIZE) {
				utilputerror("invalid keytag record length");
				rc = UTILERR_BADARG;
				goto utilindexend;
			}
			reclen = recsize;
		}
		else reclen = KEYTAG_POS_SIZE;
		riosetpos(inhandle, 0);

		pptr = fiogetwptr(inhandle);
		if (pptr == NULL) {
			utilputerror("fiogetwptr failed");
			rc = UTILERR_INTERNAL;
			goto utilindexend;
		}
		r = (struct rtab *) *pptr;
		if (r->type != 'R') {
			utilputerror("not rtab type");
			rc = UTILERR_INTERNAL;
			goto utilindexend;
		}
		r->opts |= RIO_FIX | RIO_UNC;
	}
	else reclen = RIO_MAX_RECSIZE;

	/* build the header block */
	buffer = *bufptr;
	buffer[0] = 'i';  /* will be 'I' if successful */
	memset(buffer + 1, 0, 24);
	memset(buffer + 25, ' ', 75);
	msciton(options->blksize,  buffer + 41, 5);  /* block size */
	buffer[54] = 'L';
	if (options->flags & ARG_TEXTSRCH) buffer[55] = 'T';
	if (options->flags & ARG_DUPLICATES) buffer[56] = 'D';
	if (options->flags & ARG_RECLAIM) {
		buffer[57] = 'S';
		msciton(options->length, buffer + 64, 5);
	}
	/* fill in size & version information after parsing keys */
	buffer[100] = DBCEOR;
	i1 = (INT)strlen(options->firstfile);
	memcpy(buffer + 101, options->firstfile, i1);
	i1 += 101;
	buffer[i1++] = DBCEOR;

	/* insert arguments */
	if (options->flags & ARG_FLDSPEC) {
		for (highkey = size = 0, i2 = 0; i2 < options->fieldcnt; i2++) {
			fieldinfo = *options->fieldinfo + i2;
			i1 += mscitoa(fieldinfo->pos + 1, (CHAR *)(buffer + i1));
			if (fieldinfo->len > 1) {
				buffer[i1++] = '-';
				i1 += mscitoa(fieldinfo->pos + fieldinfo->len, (CHAR *)(buffer + i1));
			}
			if (highkey < fieldinfo->pos + fieldinfo->len) highkey = fieldinfo->pos + fieldinfo->len;
			size += fieldinfo->len;
			buffer[i1++] = DBCEOR;
		}
	}
	if (options->flags & ARG_ALLOCATE) {
		buffer[i1++] = '-';
		buffer[i1++] = 'A';
		buffer[i1++] = '=';
		i1 += mscitoa(options->allocate, (CHAR *)(buffer + i1));
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_BLKSIZE) {
		buffer[i1++] = '-';
		buffer[i1++] = 'B';
		buffer[i1++] = '=';
		i1 += mscitoa(options->blksize, (CHAR *)(buffer + i1));
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_DUPLICATES) {
		buffer[i1++] = '-';
		buffer[i1++] = 'D';
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_IGNOREDUP) {
		buffer[i1++] = '-';
		buffer[i1++] = 'F';
		if (options->xtrafile1[0]) {
			buffer[i1++] = '=';
			i2 = (INT)strlen(options->xtrafile1);
			memcpy(buffer + i1, options->xtrafile1, i2);
			i1 += i2;
		}
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_SHARE) {
		buffer[i1++] = '-';
		buffer[i1++] = 'J';
		if (options->shareflag == SHARE_READONLY) buffer[i1++] = 'R';
		else if (options->shareflag == SHARE_READACCESS) buffer[i1++] = 'A';
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_KEYTAGFILE) {
		buffer[i1++] = '-';
		buffer[i1++] = 'K';
		buffer[i1++] = '=';
		i2 = (INT)strlen(options->xtrafile2);
		memcpy(buffer + i1, options->xtrafile2, i2);
		i1 += i2;
		buffer[i1++] = DBCEOR;
		if (options->flags & ARG_FLDSPEC) {
			if (size != reclen - KEYTAG_POS_SIZE) {
				utilputerror("keytag record length does not match key spec specified");
				rc = UTILERR_LONGARG;
				goto utilindexend;
			}
		}
		else size = reclen - KEYTAG_POS_SIZE;
	}
	if (options->flags & ARG_SELECT) {
		for (i2 = 0; i2 < options->selectcnt; i2++) {
			fieldinfo = *options->selectinfo + i2;
			if (fieldinfo->type & SELECT_OR) {
				buffer[i1++] = 'O';
				buffer[i1++] = 'R';
				buffer[i1++] = DBCEOR;
			}
			buffer[i1++] = '-';
			buffer[i1++] = 'P';
			i1 += mscitoa(fieldinfo->pos + 1, (CHAR *)(buffer + i1));
			if (fieldinfo->type & SELECT_STRING) {
				buffer[i1++] = '-';
				i1 += mscitoa(fieldinfo->pos + fieldinfo->len, (CHAR *)(buffer + i1));
			}
			if (fieldinfo->type & (SELECT_LESS | SELECT_GREATER)) {
				if (!(fieldinfo->type & SELECT_LESS)) buffer[i1++] = 'G';
				else if (!(fieldinfo->type & SELECT_GREATER)) buffer[i1++] = 'L';
				else buffer[i1++] = 'N';
			}
			if ((fieldinfo->type & SELECT_EQUAL) || (fieldinfo->type & (SELECT_LESS | SELECT_GREATER)) == (SELECT_LESS | SELECT_GREATER)) {
				buffer[i1++] = 'E';
				if (!(fieldinfo->type & (SELECT_LESS | SELECT_GREATER))) buffer[i1++] = 'Q';
			}
			else buffer[i1++] = 'T';
			memcpy(buffer + i1, (*options->stringinfo) + fieldinfo->off, fieldinfo->len);
			i1 += fieldinfo->len;
			buffer[i1++] = DBCEOR;
		}
	}
	if (options->flags & ARG_RECLAIM) {
		buffer[i1++] = '-';
		buffer[i1++] = 'S';
		if (options->flags & ARG_LENGTH) {
			buffer[i1++] = '=';
			i1 += mscitoa(options->length, (CHAR *)(buffer + i1));
		}
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_TEXTSRCH) {
		buffer[i1++] = '-';
		buffer[i1++] = 'T';
		buffer[i1++] = DBCEOR;
	}
	workdir = workfile = NULL;
	if (options->flags & ARG_WORKFILE) {
		buffer[i1++] = '-';
		buffer[i1++] = 'W';
		buffer[i1++] = '=';
		i2 = (INT)strlen(options->workfile);
		memcpy(buffer + i1, options->workfile, i2);
		i1 += i2;
		buffer[i1++] = DBCEOR;
#if OS_WIN32
		attrib = GetFileAttributes(options->workfile);
		if (attrib != 0xFFFFFFFF && (attrib & FILE_ATTRIBUTE_DIRECTORY)) workdir = options->workfile;
		else workfile = options->workfile;
#endif
#if OS_UNIX
		if (stat(options->workfile, &statbuf) != -1 && S_ISDIR(statbuf.st_mode)) workdir = options->workfile;
		else workfile = options->workfile;
#endif
	}
	if (options->flags & ARG_REVSELECT) {
		buffer[i1++] = '-';
		buffer[i1++] = 'X';
		buffer[i1++] = DBCEOR;
	}
	if (options->flags & ARG_EOFERROR) {
		buffer[i1++] = '-';
		buffer[i1++] = 'Y';
		buffer[i1++] = DBCEOR;
	}
	if (i1 > options->blksize) {
		utilputerror("arguments too long to store in header");
		rc = UTILERR_LONGARG;
		goto utilindexend;
	}
	memset(buffer + i1, DBCDEL, options->blksize - i1);

	if (size > XIO_MAX_KEYSIZE) {
		utilputerror("key spec exceeds maximum length");
		rc = UTILERR_LONGARG;
		goto utilindexend;
	}
	size1 = size + 6;
	if (size > 99) buffer[58] = (UCHAR)((size - 100) / 10 + 'A');
	else if (size > 9) buffer[58] = (UCHAR)(size / 10 + '0');
	buffer[59] = (UCHAR)(size % 10 + '0');
#if 0
	if (size > 99) {
#endif
		buffer[98] = '1';
		buffer[99] = '0';
#if 0
	}
	else buffer[99] = '9';
#endif

	/* create the index file */
	i2 = fiogetflags() & FIO_FLAG_UTILPREPDOS;
	if (i2) {  /* change prep directory to be same as input file */
		ptr = fioname(inhandle);
		if (ptr != NULL) {
			i1 = fioaslash(ptr) + 1;
			if (i1) memcpy(work, ptr, i1);
			work[i1] = 0;
			pptr = fiogetopt(FIO_OPT_PREPPATH);
			if (pptr != NULL) strcpy((CHAR *) buffer + AIM_HEADER_SIZE, (CHAR *) *pptr);
			fiosetopt(FIO_OPT_PREPPATH, work);
		}
		else i2 = 0;
	}
	outhandle = fioopen(options->secondfile, FIO_M_PRP | FIO_P_TXT);
	buffer = *bufptr;
	if (i2) {  /* restore prep directory */
		if (pptr != NULL) fiosetopt(FIO_OPT_PREPPATH, buffer + AIM_HEADER_SIZE);
		else fiosetopt(FIO_OPT_PREPPATH, NULL);
	}
	if (outhandle < 0) {
		utilputerror(fioerrstr(outhandle));
		rc = UTILERR_CREATE;
		goto utilindexend;
	}
	/* write the header */
	i1 = fiowrite(outhandle, 0, buffer, options->blksize);
	if (i1) {
		utilputerror(fioerrstr(i1));
		rc = UTILERR_WRITE;
		goto utilindexend;
	}
	/* additional initialization */
	sortkey[0].start = 0;
	sortkey[0].end = size;
	sortkey[0].typeflg = SIO_ASCEND;
	sortkey[1].start = size;
	sortkey[1].end = size1;
	sortkey[1].typeflg = SIO_POSITION | SIO_ASCEND;
	sortkeyptr = sortkey;

	i1 = sioinit(size1, 2, &sortkeyptr, workdir, workfile, options->allocate, 0, NULL);
	if (i1) {
		utilputerror(siogeterr());
		rc = UTILERR_SORT;
		goto utilindexend;
	}

	/* get the records */
	deloff = 0;
	ixblksize = options->blksize;
	ixhighblk = 0;
	for (reccnt = 0; ; ) {
		rec = sioputstart();
		recsize = rioget(inhandle, buffer, reclen);
		if (options->flags & ARG_KEYTAGFILE) {
			if (recsize <= KEYTAG_POS_SIZE) {
				if (recsize == -1) {  /* end of file */
					if (options->flags & ARG_EOFERROR) {
						rioeofpos(inhandle, &eofpos);
						if (pos != eofpos) {
							sioexit();
							rc = UTILERR_BADEOF;
							goto utilindexend;
						}
					}
					break;
				}
				if (recsize == -2) continue;  /* deleted record */
				if (recsize >= 0) recsize = ERR_BADRL;
				utilputerror(fioerrstr(recsize));
				sioexit();
				rc = UTILERR_READ;
				goto utilindexend;
			}
			memcpy(rec, buffer + 12, reclen - 12);
			mscntooff(buffer, &pos, 12);
			mscoffto6x(pos,  rec + reclen - 12);
			i1 = sioputend();
			if (i1) {
				utilputerror(siogeterr());
				sioexit();
				rc = UTILERR_SORT;
				goto utilindexend;
			}
			buffer = *bufptr;  /* sioputend may call fioopen */
			continue;
		}
		riolastpos(inhandle, &pos);
		if (recsize < highkey) {
			if (recsize == -1) {  /* end of file */
				if (options->flags & ARG_EOFERROR) {
					rioeofpos(inhandle, &eofpos);
					if (pos != eofpos) {
						sioexit();
						rc = UTILERR_BADEOF;
						goto utilindexend;
					}
				}
				break;
			}
			if (recsize == -2) {  /* deleted record */
				if (!(options->flags & ARG_RECLAIM)) continue;
				if (!deloff || deloff + 6 > ixblksize) {  /* 'F' block will overflow */
					if (deloff) {
						i1 = fiowrite(outhandle, ixhighblk, buffer + RIO_MAX_RECSIZE, ixblksize);
						if (i1) {
							utilputerror(fioerrstr(i1));
							sioexit();
							rc = UTILERR_WRITE;
							goto utilindexend;
						}
					}
					memset(buffer + RIO_MAX_RECSIZE, DBCDEL, ixblksize);
					buffer[RIO_MAX_RECSIZE] = 'F';
					mscoffto6x(ixhighblk, buffer + RIO_MAX_RECSIZE + 1);
					ixhighblk += ixblksize;
					deloff = 7;
				}
				mscoffto6x(pos, buffer + RIO_MAX_RECSIZE + deloff);
				continue;
			}
			if (recsize <= -3) {
				utilputerror(fioerrstr(recsize));
				sioexit();
				rc = UTILERR_READ;
				goto utilindexend;
			}
			if (!recsize) continue;
			memset(buffer + recsize, ' ', highkey - recsize);
		}
		if ((options->flags & ARG_SELECT) && !utilselect(options, buffer, recsize)) continue;
		for (i1 = 0, recsize = 0; i1 < options->fieldcnt; recsize += i2, i1++) {
			fieldinfo = *options->fieldinfo + i1;
			i2 = fieldinfo->len;
			memcpy(rec + recsize, buffer + fieldinfo->pos, i2);
		}
		mscoffto6x(pos, rec + size);
		i1 = sioputend();
		if (i1) {
			utilputerror(siogeterr());
			sioexit();
			rc = UTILERR_SORT;
			goto utilindexend;
		}
		buffer = *bufptr;  /* sioputend may call fioopen */
		reccnt++;
	}
	if ((options->flags & ARG_RECLAIM) && deloff) {
		i1 = fiowrite(outhandle, ixhighblk, buffer + RIO_MAX_RECSIZE, ixblksize);
		if (i1) {
			utilputerror(fioerrstr(i1));
			sioexit();
			rc = UTILERR_READ;
			goto utilindexend;
		}
	}
	delblk = ixhighblk;
	i1 = rioclose(inhandle);
	inhandle = 0;
	if (i1) {
		utilputerror(fioerrstr(i1));
		rc = UTILERR_CLOSE;
	}

	/* insert keys */
/*** NOTE: MUST RESET IF MOVEABLE MEMORY IS CALLED ***/
#if ((MAX_ISI_BLK_SIZE * 4) > (RIO_MAX_RECSIZE + MAX_ISI_BLK_SIZE))
	for (ixblkmax = 0, i1 = 0; ixblkmax < IXBLKMAX && i1 + ixblksize <= MAX_ISI_BLK_SIZE * 4; ixblkmax++, i1 += ixblksize) ixblk[blkmax].buf = buffer + i1;
#else
	for (ixblkmax = 0, i1 = 0; ixblkmax < IXBLKMAX && i1 + ixblksize <= RIO_MAX_RECSIZE + MAX_ISI_BLK_SIZE; ixblkmax++, i1 += ixblksize) ixblk[ixblkmax].buf = buffer + i1;
#endif
	ixblkhi = 0;

	ixbranchhi = 0;
	ixlastuse = 0;
	ixtopblk = 0;
	firstflag = TRUE;
	for ( ; ; ) {
		i1 = sioget(&rec);
		if (i1) {
			if (i1 == 1) break;
			utilputerror(siogeterr());
			sioexit();
			rc = UTILERR_SORT;
			goto utilindexend;
		}
		if (firstflag) {
			firstflag = FALSE;

			/* first record, create first leaf */
			rc = ixnewblk(outhandle, &ixleafblk);
			if (rc) {
				sioexit();
				goto utilindexend;
			}
			/* ixleafblk will always be valid without calls to ixgetblk */
			ixtopblk = ixblk[ixleafblk].pos;  /* its the top block */
			ixblk[ixleafblk].buf[0] = 'V';  /* low level block indicator */
			memcpy(ixblk[ixleafblk].buf + 1, rec, size1);
			ixleafpos = size1 + 1;
			memcpy(ixlastkey, rec, size);
			ixlastkey[size] = '\n';
			ixlastkey[size + 1] = 0;
			//reccnt = 1;
			continue;
		}
		if (!(options->flags & ARG_DUPLICATES) && ixcompkey(rec, ixlastkey, size) == size) {
			if (options->flags & ARG_IGNOREDUP) {
				if (options->xtrafile1[0]) {
					if (!duphandle) {
						duphandle = rioopen(options->xtrafile1, RIO_M_PRP | RIO_P_TXT | RIO_T_STD | RIO_UNC, 4, XIO_MAX_KEYSIZE);
						if (duphandle < 0) {
							utilputerror(fioerrstr(duphandle));
							sioexit();
							rc = UTILERR_OPEN;
							goto utilindexend;
						}
					}
					i1 = rioput(duphandle, ixlastkey, size);
					if (i1) {
						utilputerror(fioerrstr(i1));
						sioexit();
						rc = UTILERR_WRITE;
						goto utilindexend;
					}
				}
			}
			dupcnt++;
			continue;
		}

		i1 = ixcompkey(rec, ixlastkey, size);
		if (i1 > 255) i1 = 255;
		if (ixleafpos + size1 - i1 < ixblksize) {  /* low level block will not overflow when this key is added */
			ixblk[ixleafblk].buf[ixleafpos++] = (UCHAR) i1;
			memcpy(ixblk[ixleafblk].buf + ixleafpos, rec + i1, size1 - i1);
			ixleafpos += size1 - i1;
		}
		else {  /* overflow will happen */
			rc = ixins(outhandle, rec, size);
			if (rc) {
				sioexit();
				goto utilindexend;
			}
		}
		memcpy(ixlastkey, rec, size);
	}
	sioexit();
	for (i1 = 0; i1 < ixblkhi; i1++) {
		if (ixblk[i1].mod) {
			i2 = fiowrite(outhandle, ixblk[i1].pos, ixblk[i1].buf, ixblksize);
			if (i2) {
				utilputerror(fioerrstr(i2));
				rc = UTILERR_WRITE;
				goto utilindexend;
			}
		}
	}

	/* finalize the header block and write it */
	buffer[0] = 'I';
	mscoffto6x(0, buffer + 1);  /* 0 */
	mscoffto6x(delblk, buffer + 7);  /* offset of deleted records block */
	mscoffto6x(ixtopblk, buffer + 13);  /* offset of top branch block */
	mscoffto6x(ixhighblk, buffer + 19);  /* offset of last used block */
	i1 = fiowrite(outhandle, 0, buffer, 25);
	if (i1) {
		utilputerror(fioerrstr(i1));
		rc = UTILERR_WRITE;
	}

	/* clean up */
utilindexend:
	if ((options->flags & ARG_IGNOREDUP) && duphandle > 0) {
		i1 = rioclose(duphandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_CLOSE;
		}
	}
	if (outhandle > 0) {
		i1 = fioclose(outhandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_CLOSE;
		}
	}
	if (inhandle > 0) {
		i1 = rioclose(inhandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_CLOSE;
		}
	}
	memfree(bufptr);
	if (dupcnt && !(options->flags & ARG_IGNOREDUP) && !rc) rc = UTILERR_DUPS;
	return rc;
}

/* IXINS */
/* insert a key into a leaf block using overflow or split */
/* returns with ixleafblk and ixleafpos correctly set */
static INT ixins(INT outhandle, UCHAR *key, INT size)
{
	static struct BRANCHDEF {
		OFFSET pos;
		OFFSET lftpos;
		INT eob;
		INT lfteob;
		UCHAR lftrpt;
	} branch[IXLEVELMAX];
	INT i1, i2, i3, branchcnt, leafflg, size1, size2;
	INT blk1, blk2, blk3;  /* left block, parent block, right block */
	INT eob1, eob2, eob3;  /* left eob, parent eob, right eob */
	OFFSET block;
	UCHAR workkey[XIO_MAX_KEYSIZE + 20];

	size1 = size + 6;
	size2 = size1 + 6;
	leafflg = TRUE;
	eob3 = ixleafpos;
	branchcnt = ixbranchhi;
	block = ixblk[ixleafblk].pos;
	for ( ; ; ) {
		i1 = ixgetblk(outhandle, block, &blk3);
		if (i1) return i1;
		if (!leafflg) {
			eob3 = branch[branchcnt].eob;
			if (eob3 + size2 <= ixblksize) {  /* no overflow will happen */
				memcpy(ixblk[blk3].buf + eob3, workkey, size2);
				ixblk[blk3].mod = TRUE;
				branch[branchcnt].eob += size2;
				return(0);
			}
		}

		/* overflow will happen */
		if (branchcnt) { /* try to move a key to left brother */
			i1 = ixgetblk(outhandle, branch[branchcnt - 1].pos, &blk2);  /* get parent */
			if (i1) return i1;
			if (ixblk[blk2].buf[0] != 'U') {
				utilputerror("not branch block on reread");
				return UTILERR_INTERNAL;
			}

			if (branch[branchcnt - 1].lftpos != -1) {  /* this block has a left brother, add to it */
				i1 = ixgetblk(outhandle, branch[branchcnt - 1].lftpos, &blk1);  /* blk1 is block number of left brother */
				if (i1) return i1;

				/* won't use left brother again*/
				ixblk[blk1].lru = 0;
				branch[branchcnt - 1].lftpos = -1;

				eob1 = branch[branchcnt - 1].lfteob;
				eob2 = branch[branchcnt - 1].eob - size2;
				i1 = branch[branchcnt - 1].lftrpt;
				ixblk[blk1].mod = ixblk[blk2].mod = ixblk[blk3].mod = TRUE;
				if (leafflg) {  /* its a leaf left overflow */
					ixblk[blk1].buf[eob1++] = i1;
					memcpy(ixblk[blk1].buf + eob1, ixblk[blk2].buf + eob2 + i1, size1 - i1);
					memcpy(ixblk[blk2].buf + eob2, ixblk[blk3].buf + 1, size1);
					i1 = ixblk[blk3].buf[size1 + 1];
					memmove(ixblk[blk3].buf + i1 + 1, ixblk[blk3].buf + size1 + 2, eob3 - (size1 + 2));
					eob3 -= size1 - i1 + 1;
					memset(ixblk[blk3].buf + eob3, DBCDEL, ixblksize - eob3);
					i1 = ixcompkey(key, ixlastkey, size);
					if (i1 > 255) i1 = 255;
					if (eob3 + size1 - i1 + 1 > ixblksize) goto ixins1;  /* still won't fit */
					ixblk[blk3].buf[eob3++] = (UCHAR) i1;
					memcpy(ixblk[blk3].buf + eob3, key + i1, size1 - i1);
					ixleafpos = eob3 + size1 - i1;
				}
				else {  /* its a branch left overflow */
					memcpy(ixblk[blk1].buf + eob1, ixblk[blk2].buf + eob2, size1);
					memcpy(ixblk[blk1].buf + eob1 + size1, ixblk[blk3].buf + 1, 6);
					memcpy(ixblk[blk2].buf + eob2, ixblk[blk3].buf + 7, size1);
					memmove(ixblk[blk3].buf + 1, ixblk[blk3].buf + size2 + 1, eob3 - (size2 + 1));
					memcpy(ixblk[blk3].buf + eob3 - size2, workkey, size2);
				}
				return 0;
			}
		}
		else {  /* block is top block, split & create new top block */
			if (ixbranchhi == IXLEVELMAX) {
				utilputerror("branch exceeded maximum level");
				return UTILERR_INTERNAL;
			}
			i1 = ixnewblk(outhandle, &blk2);  /* get new parent */
			if (i1) return i1;
			ixblk[blk2].buf[0] = 'U';
			mscoffto6x(block, ixblk[blk2].buf + 1);
			ixtopblk = ixblk[blk2].pos;
			memmove(branch + 1, branch, ixbranchhi++ * sizeof(struct BRANCHDEF));
			branch[0].pos = ixtopblk;
			branch[0].eob = 7;
			branchcnt = 1;
		}

		/* this block has full/no left brother, must split */
ixins1:
		blk1 = blk3;
		eob1 = eob3;
		i1 = ixnewblk(outhandle, &blk3);  /* will be new current leaf (new right block) */
		if (i1) return i1;

		branch[branchcnt - 1].lftpos = block;
		ixblk[blk3].buf[0] = ixblk[blk1].buf[0];
		if (leafflg) {  /* leaf block split */
			memcpy(ixblk[blk3].buf + 1, key, size1);
			for (i1 = 1, i2 = 0, i3 = 1; ; ) {
				memcpy(workkey + i2, ixblk[blk1].buf + i1, size - i2);
				i1 += size1 - i2;
				if (i1 >= eob1) {
					memcpy(workkey + size, ixblk[blk1].buf + i1 - 6, 6);
					break;
				}
				i3 = i1;
				i2 = ixblk[blk1].buf[i1++];
			}
			branch[branchcnt - 1].lftrpt = (UCHAR) i2;
			eob1 = i3;

			/* new leafblk */
			ixleafblk = blk3;
			ixleafpos = size1 + 1;
			leafflg = FALSE;
		}
		else {  /* branch block split */
			memcpy(ixblk[blk3].buf + 1, ixblk[blk1].buf + eob1 - 6, 6);
			memcpy(ixblk[blk3].buf + 7, workkey, size2);
			branch[branchcnt].eob = size2 + 7;
			branch[branchcnt].pos = ixblk[blk3].pos;
			eob1 -= size2;
			memcpy(workkey, ixblk[blk1].buf + eob1, size1);
		}

		branch[branchcnt - 1].lfteob = eob1;
		memset(ixblk[blk1].buf + eob1, DBCDEL, ixblksize - eob1);
		ixblk[blk1].mod = TRUE;
		mscoffto6x(ixblk[blk3].pos, workkey + size1);
		branchcnt--;
		block = ixblk[blk2].pos;
	}
}

static INT ixcompkey(UCHAR *key1, UCHAR *key2, INT size)
{
	INT i1;

	for (i1 = 0; i1 < size; i1++) {
		if (key1[i1] == key2[i1]) continue;
		if (!ixcollateflag || ixpriority[key1[i1]] != ixpriority[key2[i1]]) break;
	}
	return i1;
}

/* IXNEWBLK */
/* return buffer number of newly created block */
static INT ixnewblk(INT outhandle, INT *blknum)
{
	INT i1, i2 = 0, lru;

	if (ixblkhi == ixblkmax) {  /* must force out a least recently used buffer */
		for (i1 = 0, lru = ixlastuse + 1; i1 < ixblkmax; i1++) {
			if (ixblk[i1].lru < lru && i1 != ixleafblk) {  /* never force out ixleafblk */
				lru = ixblk[i1].lru;
				i2 = i1;
			}
		}
		if (ixblk[i2].mod) {
			i1 = fiowrite(outhandle, ixblk[i2].pos, ixblk[i2].buf, ixblksize);
			if (i1) {
				utilputerror(fioerrstr(i1));
				return UTILERR_WRITE;
			}
		}
	}
	else i2 = ixblkhi++;
	ixhighblk += ixblksize;
	memset(ixblk[i2].buf, DBCDEL, ixblksize);
	ixblk[i2].mod = TRUE;
	ixblk[i2].lru = ++ixlastuse;
	ixblk[i2].pos = ixhighblk;
	*blknum = i2;
	return 0;
}

/* IXGETBLK */
/* get a block into a buffer, return buffer number */
static INT ixgetblk(INT outhandle, OFFSET pos, INT *blknum)
{
	INT i1, i2, lru;

	/* try to find block in buffer pool */
	for (i1 = 0; i1 < ixblkhi && ixblk[i1].pos != pos; i1++);
	if (i1 != ixblkhi) {
		ixblk[i1].lru = ++ixlastuse;
		*blknum = i1;
		return 0;
	}

	/* must force out a least recently used buffer */
	for (i1 = i2 = 0, lru = ixlastuse + 1; i1 < ixblkhi; i1++) {
		if (ixblk[i1].lru < lru && i1 != ixleafblk) {  /* never force out ixleafblk */
			lru = ixblk[i1].lru;
			i2 = i1;
		}
	}
	if (ixblk[i2].mod) {
		i1 = fiowrite(outhandle, ixblk[i2].pos, ixblk[i2].buf, ixblksize);
		if (i1) {
			utilputerror(fioerrstr(i1));
			return UTILERR_WRITE;
		}
	}
	i1 = fioread(outhandle, pos, ixblk[i2].buf, ixblksize);
	if (i1 != ixblksize) {
		if (i1 < 0) utilputerror(fioerrstr(i1));
		else utilputerror("unable to reread index file");
		return UTILERR_READ;
	}
	ixblk[i2].mod = FALSE;
	ixblk[i2].lru = ++ixlastuse;
	ixblk[i2].pos = pos;
	*blknum = i2;
	return 0;
}

static INT utilreformat(UTILARGS *options)
{
	INT i1, i2, i3, i4, i5, createflag1, createflag2, datetype, handle, incnt;
	INT inhandle, insize, nonselhandle, openflag, outcnt, outhandle, rc, recsize;
	OFFSET eofpos, inpos, outpos, pos;
	CHAR *ptr;
	UCHAR c1, delchr, eofchr, transmap[256], work[MAX_NAMESIZE];
	UCHAR *inbuf, *outbuf, **bufptr, **pptr;
	FIELDINFO *fieldinfo;

	if ((options->flags & ARG_COMPRESSED) && (options->flags & ARG_TYPEMASK) && (options->flags & ARG_TYPEMASK) != ARG_TYPESTD) {
		utilputerror("-C are mutually exclusive with non-STD file type");
		return UTILERR_BADARG;
	}
	if ((options->flags & ARG_SPEED) && (options->flags & (ARG_FLDSPEC | ARG_TABOFFSET | ARG_SELECT | ARG_COMPRESSED | ARG_TYPEMASK))) {
		utilputerror("-R option mutually exclusive with field specs, -X=n, -P, -C and file type");
		return UTILERR_BADARG;
	}
	if (options->flags & ARG_TABINSERT) {
		if (!(options->flags & ARG_TABOFFSET)) {
			utilputerror("-I requires -X=n");
			return UTILERR_BADARG;
		}
		if (options->flags & (ARG_FLDSPEC | ARG_KEYTAG | ARG_COMPRESSED)) {
			utilputerror("-I option mutually exclusive with field specs, -K and -C");
			return UTILERR_BADARG;
		}
		if (!(options->flags & ARG_TYPEMASK)) options->flags |= ARG_TYPETEXT;
	}
	if ((options->flags & ARG_EOFERROR) && (options->flags & ARG_SHARE) && options->shareflag != SHARE_READONLY) {
		utilputerror("-Y and -J options are mutually exclusive");
		return UTILERR_BADARG;
	}

	if (!(options->flags & ARG_SELECT)) options->flags &= ~ARG_NONSELECT;
	if (options->flags & ARG_SHARE) {
		if (options->shareflag == SHARE_READONLY) openflag = RIO_M_SRO | RIO_P_TXT | RIO_T_ANY;
		else if (options->shareflag == SHARE_READACCESS) openflag = RIO_M_SRA | RIO_P_TXT | RIO_T_ANY;
		else openflag = RIO_M_SHR | RIO_P_TXT | RIO_T_ANY;
	}
	else openflag = RIO_M_ERO | RIO_P_TXT | RIO_T_ANY;
	if ((options->flags & ARG_TYPEMASK) == ARG_TYPEBIN) {
		createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_BIN | RIO_UNC;
		createflag2 = RIO_M_EXC | RIO_P_TXT | RIO_T_BIN | RIO_UNC;
	}
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPEDATA) {
		createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_DAT | RIO_UNC;
		createflag2 = RIO_M_EXC | RIO_P_TXT | RIO_T_DAT | RIO_UNC;
	}
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPEDOS) {
		createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_DOS | RIO_UNC;
		createflag2 = RIO_M_EXC | RIO_P_TXT | RIO_T_DOS | RIO_UNC;
	}
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPEMAC) {
		createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_MAC | RIO_UNC;
		createflag2 = RIO_M_EXC | RIO_P_TXT | RIO_T_MAC | RIO_UNC;
	}
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPETEXT) {
		createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_TXT | RIO_UNC;
		createflag2 = RIO_M_EXC | RIO_P_TXT | RIO_T_TXT | RIO_UNC;
	}
	else if (options->flags & ARG_COMPRESSED) {
		createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_STD;
		createflag2 = RIO_M_EXC | RIO_P_TXT | RIO_T_STD;
	}
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPESTD) {
		createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_STD | RIO_UNC;
		createflag2 = RIO_M_EXC | RIO_P_TXT | RIO_T_STD | RIO_UNC;
	}
	else {
		createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_STD | RIO_UNC;
		createflag2 = RIO_M_EXC | RIO_P_TXT | RIO_T_ANY | RIO_UNC;
	}
	if (options->flags & ARG_TRANSLATE) {  /* read translate map file */
		if ((i1 = fioopen(options->xtrafile1, FIO_M_SRO | FIO_P_SRC)) < 0) {
			utilputerror(fioerrstr(i1));
			return UTILERR_TRANSLATE;
		}
		if (fioread(i1, 0, transmap, UCHAR_MAX + 1) != UCHAR_MAX + 1) {
			utilputerror("unable to read 256 bytes");
			fioclose(i1);
			return UTILERR_TRANSLATE;
		}
		fioclose(i1);
	}

	/* allocate buffer memory */
	if (options->flags & ARG_SPEED) i1 = BUFFER_SIZE * 2;
	else if (options->flags & (ARG_KEYTAG | ARG_FLDSPEC | ARG_TABOFFSET)) i1 = RIO_MAX_RECSIZE * 2 + 4;
	else i1 = RIO_MAX_RECSIZE + 4;
	bufptr = memalloc(i1, 0);
	if (bufptr == NULL) return UTILERR_NOMEM;
	rc = inhandle = nonselhandle = outhandle = 0;

	/* open the input file */
	if (options->flags & ARG_SPEED) i1 = i2 = 0;
	else {
		i1 = RIO_BUF_SIZE;
		i2 = RIO_MAX_RECSIZE;
	}
	inhandle = rioopen(options->firstfile, openflag, i1, i2);
	if (inhandle < 0) {
		utilputerror(fioerrstr(inhandle));
		rc = UTILERR_OPEN;
		goto utilreformatend;
	}

	/* open or create the output file */
	i2 = fiogetflags() & FIO_FLAG_UTILPREPDOS;
utilreformat1:
	if (i2) {  /* change prep directory to be same as input file */
		ptr = fioname(inhandle);
		if (ptr != NULL) {
			i1 = fioaslash(ptr) + 1;
			if (i1) memcpy(work, ptr, i1);
			work[i1] = 0;
			pptr = fiogetopt(FIO_OPT_PREPPATH);
			if (pptr != NULL) strcpy((CHAR *) *bufptr, (CHAR *) *pptr);
			fiosetopt(FIO_OPT_PREPPATH, work);
		}
		else i2 = 0;
	}
	if ((options->flags & ARG_SPEED) && !(options->flags & ARG_APPEND)) {
		outhandle = fioopen(options->secondfile, FIO_M_PRP | FIO_P_TXT);
		outpos = 0;
	}
	else if (!(options->flags & ARG_APPEND) || (outhandle = rioopen(options->secondfile, createflag2, RIO_BUF_SIZE, RIO_MAX_RECSIZE)) == ERR_FNOTF)
		outhandle = rioopen(options->secondfile, createflag1, RIO_BUF_SIZE, RIO_MAX_RECSIZE);
	if (i2) {  /* restore prep directory */
		if (pptr != NULL) fiosetopt(FIO_OPT_PREPPATH, *bufptr);
		else fiosetopt(FIO_OPT_PREPPATH, NULL);
	}
	if (outhandle < 0) {
		utilputerror(fioerrstr(outhandle));
		rc = UTILERR_CREATE;
		goto utilreformatend;
	}
	if (options->flags & ARG_APPEND) {
		if (riotype(outhandle) == RIO_T_ANY) {
			for ( ; ; ) {
				recsize = rioget(outhandle, *bufptr, RIO_MAX_RECSIZE);
				if (recsize >= -1) break;
				if (recsize == -2) continue;
				utilputerror(fioerrstr(recsize));
				rc = UTILERR_READ;
				goto utilreformatend;
			}
			if (riotype(outhandle) == RIO_T_ANY) {
				rioclose(outhandle);
				options->flags &= ~ARG_APPEND;
				createflag1 = RIO_M_PRP | RIO_P_TXT | RIO_T_TXT | RIO_UNC;
				goto utilreformat1;
			}
		}
		rioeofpos(outhandle, &outpos);
		if (!(options->flags & ARG_SPEED)) riosetpos(outhandle, outpos);
		else if (riotype(inhandle) != riotype(outhandle)) {
			utilputerror("-A and -R option only valid on similar types of files");
			rc = UTILERR_OPEN;
			goto utilreformatend;
		}
	}

	/* create non-selection criteria output file */
	if (options->flags & ARG_NONSELECT) {
		if (i2) {  /* change prep directory to be same as input file */
			ptr = fioname(inhandle);
			if (ptr != NULL) {
				i1 = fioaslash(ptr) + 1;
				if (i1) memcpy(work, ptr, i1);
				work[i1] = 0;
				pptr = fiogetopt(FIO_OPT_PREPPATH);
				if (pptr != NULL) strcpy((CHAR *) *bufptr, (CHAR *) *pptr);
				fiosetopt(FIO_OPT_PREPPATH, work);
			}
			else i2 = 0;
		}
		nonselhandle = rioopen(options->xtrafile2, createflag1, RIO_BUF_SIZE - 1, RIO_MAX_RECSIZE);
		if (i2) {  /* restore prep directory */
			if (pptr != NULL) fiosetopt(FIO_OPT_PREPPATH, *bufptr);
			else fiosetopt(FIO_OPT_PREPPATH, NULL);
		}
		if (nonselhandle < 0) {
			utilputerror(fioerrstr(nonselhandle));
			rc = UTILERR_CREATE;
			goto utilreformatend;
		}
	}

	/* other initialization, depending on input file type */
	if (riotype(inhandle) == RIO_T_STD) {  /* set inflag type and cancel tab expand */
		delchr = DBCDEL;
		eofchr = DBCEOF;
		if (!(options->flags & ARG_TABINSERT)) options->flags &= ~ARG_TABOFFSET;
	}
	else {  /* local text operating system */
		delchr = 0x7F;
		if ((options->flags & ARG_EOFERROR) && riotype(inhandle) == RIO_T_DOS) eofchr = 0x1A;
		else options->flags &= ~ARG_EOFERROR;
	}

	if (options->flags & ARG_SPEED) {  /* get each block and move characters from input block to output block */
		/* first half of buffer is for input, second half is for output */
		inbuf = *bufptr;
		outbuf = inbuf + BUFFER_SIZE;
		incnt = insize = 0;
		outcnt = 0;
		inpos = 0;  /* outpos set above */
		for ( ; ; ) {
			if (incnt == insize) {
				insize = fioread(inhandle, inpos, inbuf, BUFFER_SIZE);
				if (insize < 0) {
					utilputerror(fioerrstr(insize));
					rc = UTILERR_READ;
					goto utilreformatend;
				}
				inpos += insize;
				incnt = 0;
			}
			if (outcnt == BUFFER_SIZE || !insize) {
				if (outcnt) {
					i1 = fiowrite(outhandle, outpos, outbuf, outcnt);
					if (i1) {
						utilputerror(fioerrstr(insize));
						rc = UTILERR_WRITE;
						goto utilreformatend;
					}
				}
				if (!insize) break;
				outpos += outcnt;
				outcnt = 0;
			}
			i1 = insize;
			if (i1 > BUFFER_SIZE - outcnt + incnt) i1 = BUFFER_SIZE - outcnt + incnt;
			if (!(options->flags & ARG_EOFERROR)) {
				while (incnt < i1) {
					if (inbuf[incnt] != delchr) outbuf[outcnt++] = inbuf[incnt];
					incnt++;
				}
			}
			else {
				while (incnt < i1) {
					if (inbuf[incnt] == eofchr) {
						 fiogetsize(inhandle, &pos);
						 if (inpos - insize + incnt + 1 != pos) {
							rc = UTILERR_BADEOF;
							goto utilreformatend;
						 }
					}
					if (inbuf[incnt] != delchr) outbuf[outcnt++] = inbuf[incnt];
					incnt++;
				}
			}
		}
		goto utilreformatend;
	}

	/* process the records */
	if (options->flags & (ARG_KEYTAG | ARG_FLDSPEC | ARG_TABOFFSET)) {
		inbuf = *bufptr;
		outbuf = inbuf + RIO_MAX_RECSIZE;
	}
	else inbuf = outbuf = *bufptr;
	for ( ; ; ) {
		recsize = rioget(inhandle, inbuf, RIO_MAX_RECSIZE);
		if (recsize < 0) {
			if (recsize == -1) {  /* end of file */
				if (options->flags & ARG_EOFERROR) {
					riolastpos(inhandle, &pos);
					rioeofpos(inhandle, &eofpos);
					if (pos != eofpos) {
						rc = UTILERR_BADEOF;
						goto utilreformatend;
					}
				}
				break;
			}
			if (recsize == -2) continue;
			utilputerror(fioerrstr(recsize));
			rc = UTILERR_READ;
			goto utilreformatend;
		}
/*** CODE: SHOULD TRANSLATE HAPPEN BEFORE OR AFTER SELECT ***/
		if (options->flags & ARG_TRANSLATE)
			for (i2 = 0; i2 < recsize; i2++) inbuf[i2] = transmap[inbuf[i2]];
		if ((options->flags & ARG_SELECT) && !utilselect(options, inbuf, recsize)) {
			if (!(options->flags & ARG_NONSELECT)) continue;
			handle = nonselhandle;
		}
		else handle = outhandle;

		/* tab expansion or compression */
		if ((options->flags & ARG_TABOFFSET) && recsize) {
			if (!(options->flags & ARG_TABINSERT)) {  /* tabs are being expanded */
				for (i1 = 0, i2 = 0, i4 = 0; i2 < recsize && i4 < RIO_MAX_RECSIZE; ) {
					c1 = inbuf[i2++];
					if (c1 == 0x09) {  /* tab expansion */
						while (i1 < options->tabcnt && i4 >= (*options->tabinfo)[i1]) i1++;
						if (i1 < options->tabcnt) {
							i3 = (*options->tabinfo)[i1++] - i4;
							memset(outbuf + i4, ' ', i3);
							i4 += i3;
						}
						else outbuf[i4++] = ' ';
					}
					else outbuf[i4++] = c1;
				}
			}
			else {  /* tabs are being inserted */
				for (i1 = 0, i2 = 0, i3 = -1, i4 = 0; i2 < recsize; i2++) {
					if (i3 != -1 && i2 == (*options->tabinfo)[i1]) {
						i4 = i3;
						outbuf[i4++] = 0x09;
						i3 = -1;
					}
					c1 = inbuf[i2];
					if (c1 == ' ') {
						if (i3 == -1 && i1 < options->tabcnt) {
							while (i1 < options->tabcnt && i2 >= (*options->tabinfo)[i1]) i1++;
							if (i1 < options->tabcnt) i3 = i4;
						}
					}
					else i3 = -1;
					outbuf[i4++] = c1;
				}
			}
			recsize = i4;
		}

		/* altering record format */
		if (options->flags & (ARG_KEYTAG | ARG_FLDSPEC)) {
			if (options->flags & ARG_KEYTAG) {
				riolastpos(inhandle, &pos);
				mscoffton(pos, outbuf, KEYTAG_POS_SIZE);
				i1 = KEYTAG_POS_SIZE;
			}
			else i1 = 0;
			if (options->flags & ARG_FLDSPEC) {
				for (i2 = 0; i2 < options->fieldcnt; i2++) {
					fieldinfo = *options->fieldinfo + i2;
					i3 = fieldinfo->len;
					if (fieldinfo->type == FIELD_SPEC) {
						if (fieldinfo->pos >= recsize) continue;
						if (fieldinfo->pos + i3 > recsize) i3 = recsize - fieldinfo->pos;
						memcpy(outbuf + i1, inbuf + fieldinfo->pos, i3);
					}
					else if (fieldinfo->type == FIELD_STRING) memcpy(outbuf + i1, *options->stringinfo + fieldinfo->off, i3);
					else if (fieldinfo->type == FIELD_BLANKS) memset(outbuf + i1, ' ', i3);
					else {  /* DATE type */
						datetype = fieldinfo->type;
						if (fieldinfo->pos + i3 > recsize) {
							if (recsize > fieldinfo->pos) i4 = recsize - fieldinfo->pos;
							else i4 = 0;
							memset(inbuf + fieldinfo->pos + i4, ' ', i3 - i4);
						}
						i4 = fieldinfo->pos;
						if (isdigit(inbuf[i4 + 1])) {
							i5 = inbuf[i4 + 1] - '0';
							if (isdigit(inbuf[i4])) i5 += (inbuf[i4] - '0') * 10;
							else if (inbuf[i4] != ' ') i5 = -1;
						}
						else i5 = -1;
						memcpy(outbuf + i1 + 2, inbuf + i4, i3);
						outbuf[i1] = outbuf[i1 + 2];
						outbuf[i1 + 1] = outbuf[i1 + 3];
						if (i5 == -1) {
							if (datetype != FIELD_DATEDUP) outbuf[i1] = outbuf[i1 + 1] = ' ';
						}
						else if (datetype == FIELD_DATEREV) {
							i5 = 99 - i5;  /* convert reversed year back */
							if (outbuf[i1 + 2] == ' ') outbuf[i1 + 2] = '0';
							if (i5 > fieldinfo->off) {  /* 9999 - (1900 + i5) */
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
								if (i5 > fieldinfo->off) {
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
				recsize = i1;
			}
			else {
				memcpy(outbuf + i1, inbuf, recsize);
				//i1 += recsize;
			}
		}

		/* modify record length */
		if (options->flags & ARG_LENGTH) {
			if (options->length > recsize) memset(outbuf + recsize, ' ', options->length - recsize);
			recsize = options->length;
		}
		i1 = rioput(handle, outbuf, recsize);
		if (i1) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_WRITE;
			goto utilreformatend;
		}
	}

	/* clean up */
utilreformatend:
	if (nonselhandle > 0) {
		i1 = rioclose(nonselhandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_CLOSE;
		}
	}
	if (outhandle > 0) {
		if ((options->flags & ARG_SPEED) && !(options->flags & ARG_APPEND)) i1 = fioclose(outhandle);
		else i1 = rioclose(outhandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_CLOSE;
		}
	}
	if (inhandle > 0) {
		i1 = rioclose(inhandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_CLOSE;
		}
	}
	memfree(bufptr);
	return rc;
}

static INT utilrename(UTILARGS *options)
{
	INT i1, inhandle;

	/* open the file */
	miofixname(options->firstfile, ".txt", FIXNAME_EXT_ADD);
	inhandle = fioopen(options->firstfile, FIO_M_EXC | FIO_P_TXT);
	if (inhandle < 0) {
		utilputerror(fioerrstr(inhandle));
		return UTILERR_OPEN;
	}

	/* rename the file */
	miofixname(options->secondfile, ".txt", FIXNAME_EXT_ADD);
	i1 = fiorename(inhandle, options->secondfile);
	if (i1) return UTILERR_RENAME;
	return 0;
}

static INT utilsort(UTILARGS *options)
{
	INT i1, i2, i3, collateflag, createflag, firstflag, highkey, inhandle, keycnt, keytagsize;
	INT maxlen, maxrec, openflag, outhandle, rc, recgrpoff, reclen, recpos, recsize, rectagoff;
	OFFSET eofpos, pos;
	CHAR *workdir, *workfile;
	UCHAR c1, headpos[KEYTAG_POS_SIZE + 1], priority[UCHAR_MAX + 1], work[MAX_NAMESIZE];
	UCHAR *buffer, *rec, *ptr, **bufptr, **pptr;
	FIELDINFO *fieldinfo;
	SIOKEY *key, **keyptr;
#if OS_WIN32
	DWORD attrib;
#endif
#if OS_UNIX
	struct stat statbuf;
#endif

	if ((options->flags & ARG_COMPRESSED) && (options->flags & ARG_TYPEMASK) && (options->flags & ARG_TYPEMASK) != ARG_TYPESTD) {
		utilputerror("-C are mutually exclusive with non-STD file type");
		return UTILERR_BADARG;
	}
	if (!(options->flags & ARG_FLDSPEC)) {
		utilputerror("no sortspec specified");
		rc = UTILERR_BADARG;
		goto utilsortend;
	}
	if ((options->flags & ARG_EOFERROR) && (options->flags & ARG_SHARE) && options->shareflag != SHARE_READONLY) {
		utilputerror("-Y and -J options are mutually exclusive");
		rc = UTILERR_BADARG;
		goto utilsortend;
	}

	if (!(options->flags & ARG_ALLOCATE)) options->allocate = 0;
	if (options->flags & ARG_MAXLENGTH) maxlen = options->maxlength;
	else maxlen = 0;
	if (options->flags & ARG_SHARE) {
		if (options->shareflag == SHARE_READONLY) openflag = RIO_M_SRO | RIO_P_TXT | RIO_T_ANY;
		else if (options->shareflag == SHARE_READACCESS) openflag = RIO_M_SRA | RIO_P_TXT | RIO_T_ANY;
		else openflag = RIO_M_SHR | RIO_P_TXT | RIO_T_ANY;
	}
	else openflag = RIO_M_ERO | RIO_P_TXT | RIO_T_ANY;
	if ((options->flags & ARG_TYPEMASK) == ARG_TYPEBIN) createflag = RIO_M_PRP | RIO_P_TXT | RIO_T_BIN | RIO_UNC;
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPEDATA) createflag = RIO_M_PRP | RIO_P_TXT | RIO_T_DAT | RIO_UNC;
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPEDOS) createflag = RIO_M_PRP | RIO_P_TXT | RIO_T_DOS | RIO_UNC;
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPEMAC) createflag = RIO_M_PRP | RIO_P_TXT | RIO_T_MAC | RIO_UNC;
	else if ((options->flags & ARG_TYPEMASK) == ARG_TYPETEXT) createflag = RIO_M_PRP | RIO_P_TXT | RIO_T_TXT | RIO_UNC;
	else if (options->flags & ARG_COMPRESSED) createflag = RIO_M_PRP | RIO_P_TXT | RIO_T_STD;
	else createflag = RIO_M_PRP | RIO_P_TXT | RIO_T_STD | RIO_UNC;
	pptr = fiogetopt(FIO_OPT_COLLATEMAP);
	if (pptr != NULL) {
		memcpy(priority, *pptr, (UCHAR_MAX + 1) * sizeof(UCHAR));
		collateflag = TRUE;
	}
	else collateflag = FALSE;
	if (options->flags & (ARG_POSNTAG | ARG_KEYTAG)) {
/*** CODE: MAYBE ALLOW TO BE VARIABLE AS IN SORT.EXE USING DBCDX.KEYTAG=OLD ***/
		keytagsize = KEYTAG_POS_SIZE;
	}
	else if (options->flags & (ARG_HEADER | ARG_2PASSSORT | ARG_STABLE)) keytagsize = KEYTAG_POS_SIZE;
	else keytagsize = 0;

	bufptr = memalloc(KEYTAG_POS_SIZE + 1 + keytagsize + RIO_MAX_RECSIZE + 4, 0);
	if (bufptr == NULL) return UTILERR_NOMEM;
	rc = inhandle = outhandle = 0;

	/* transfer fldspecs to siokey and other transformations */ 
	keyptr = (SIOKEY **) memalloc((options->fieldcnt + 2) * sizeof(SIOKEY), 0);
	if (keyptr == NULL) {
		rc = UTILERR_NOMEM;
		goto utilsortend;
	}
	keycnt = recpos = 0;
	if (!(options->flags & (ARG_POSNTAG | ARG_HEADER | ARG_KEYTAG | ARG_2PASSSORT))) recpos += sizeof(USHORT);
	recgrpoff = recpos;
	key = *keyptr;
	if (options->flags & ARG_GROUP) {
		key[keycnt].typeflg = SIO_POSITION | SIO_ASCEND;
		key[keycnt].start = recpos;
		key[keycnt++].end = recpos + KEYTAG_POS_SIZE + 1;
		recpos += KEYTAG_POS_SIZE + 1;
	}
	rectagoff = recpos;
	recpos += keytagsize;
	reclen = recpos;
	for (highkey = 0, i1 = 0; i1 < options->fieldcnt; keycnt++, i1++) {
		fieldinfo = *options->fieldinfo + i1;
		if (fieldinfo->type & FIELD_ASCEND) key[keycnt].typeflg = SIO_ASCEND;
		else if (fieldinfo->type & FIELD_DESCEND) key[keycnt].typeflg = SIO_DESCEND;
		else key[keycnt].typeflg = (options->flags & ARG_REVERSE) ? SIO_DESCEND : SIO_ASCEND;
		if (fieldinfo->type & FIELD_NUMERIC) key[keycnt].typeflg |= SIO_NUMERIC;
		if (options->flags & (ARG_POSNTAG | ARG_HEADER | ARG_KEYTAG | ARG_2PASSSORT)) {
			key[keycnt].start = reclen;
			reclen += fieldinfo->len;
			key[keycnt].end = reclen;
		}
		else {
			key[keycnt].start = recpos + fieldinfo->pos;
			key[keycnt].end = recpos + fieldinfo->pos + fieldinfo->len;
		}
		if (highkey < fieldinfo->pos + fieldinfo->len) highkey = fieldinfo->pos + fieldinfo->len;
	}
	if (options->flags & (ARG_POSNTAG | ARG_HEADER | ARG_KEYTAG | ARG_2PASSSORT | ARG_STABLE)) {
		key[keycnt].typeflg = SIO_POSITION | SIO_ASCEND;
		key[keycnt].start = recpos - keytagsize;
		key[keycnt++].end = recpos;
	}

	/* open the input file */
	inhandle = rioopen(options->firstfile, openflag, RIO_BUF_SIZE, RIO_MAX_RECSIZE);
	if (inhandle < 0) {
		utilputerror(fioerrstr(inhandle));
		rc = UTILERR_OPEN;
		goto utilsortend;
	}

	/* figure out file type and maximum record size */
	if (options->flags & (ARG_POSNTAG | ARG_HEADER | ARG_KEYTAG | ARG_2PASSSORT)) {
		maxlen = RIO_MAX_RECSIZE;
		/* reclen set above */
	}
	else {
		if (!maxlen || highkey > maxlen) {
			if (!maxlen) {
				while ((maxlen = rioget(inhandle, *bufptr, RIO_MAX_RECSIZE)) == -2);
				if (maxlen <= -3) {
					utilputerror(fioerrstr(maxlen));
					rc = UTILERR_READ;
					goto utilsortend;
				}
				if (maxlen == -1) maxlen = RIO_MAX_RECSIZE;
				riosetpos(inhandle, 0);
			}
			if (highkey > maxlen) maxlen = highkey;
		}
		reclen = recpos + maxlen;
	}

	workdir = workfile = NULL;
	if (options->flags & ARG_WORKFILE) {
#if OS_WIN32
		attrib = GetFileAttributes(options->workfile);
		if (attrib != 0xFFFFFFFF && (attrib & FILE_ATTRIBUTE_DIRECTORY)) workdir = options->workfile;
		else workfile = options->workfile;
#endif
#if OS_UNIX
		if (stat(options->workfile, &statbuf) != -1 && S_ISDIR(statbuf.st_mode)) workdir = options->workfile;
		else workfile = options->workfile;
#endif
	}

	i1 = sioinit(reclen, keycnt, keyptr, workdir, workfile, options->allocate, 0, NULL);
	if (i1) {
		utilputerror(siogeterr());
		rc = UTILERR_SORT;
		goto utilsortend;
	}

	/* additional initialization */
	maxrec = 0;
	if (options->flags & ARG_GROUP) {
		mscoffton(0, headpos, KEYTAG_POS_SIZE);
		headpos[KEYTAG_POS_SIZE] = ' ';
	}

	/* get the records */
	for (firstflag = 0; ; ) {
		rec = sioputstart();
		if (options->flags & (ARG_POSNTAG | ARG_HEADER | ARG_KEYTAG | ARG_2PASSSORT)) ptr = *bufptr;
		else ptr = rec + recpos;
		recsize = rioget(inhandle, ptr, maxlen);
		if (recsize < highkey) {
			if (recsize == -1) {  /* end of file */
				if (options->flags & ARG_EOFERROR) {
					riolastpos(inhandle, &pos);
					rioeofpos(inhandle, &eofpos);
					if (pos != eofpos) {
						sioexit();
						rc = UTILERR_BADEOF;
						goto utilsortend;
					}
				}
				break;
			}
			if (recsize == -2) continue;
			if (recsize <= -3) {
				utilputerror(fioerrstr(recsize));
				sioexit();
				rc = UTILERR_READ;
				goto utilsortend;
			}
			if (!recsize) continue;
			memset(ptr + recsize, ' ', highkey - recsize);
		}
		if (recsize > maxrec) maxrec = recsize;  /* to optimize re-reading of input file */
		if ((options->flags & ARG_SELECT) && !utilselect(options, ptr, recsize)) continue;
		if (options->flags & (ARG_POSNTAG | ARG_HEADER | ARG_KEYTAG | ARG_2PASSSORT | ARG_STABLE)) {
			riolastpos(inhandle, &pos);
			mscoffton(pos, rec + rectagoff, keytagsize);
		}
		if (options->flags & ARG_GROUP) {
			c1 = ptr[options->grouppos];
			if ((options->groupflag && (options->grouppos >= recsize || c1 != (UCHAR) options->groupchar))
			   || (!options->groupflag && options->grouppos < recsize && c1 == (UCHAR) options->groupchar)) {
				riolastpos(inhandle, &pos);
				mscoffton(pos, headpos, KEYTAG_POS_SIZE);
				headpos[KEYTAG_POS_SIZE] = '1';
			}
			else headpos[KEYTAG_POS_SIZE] = '2';
			memcpy(rec + recgrpoff, headpos, KEYTAG_POS_SIZE + 1);
		}
		if (options->flags & (ARG_POSNTAG | ARG_HEADER | ARG_KEYTAG | ARG_2PASSSORT)) {
			if (options->flags & ARG_HEADER) {
				if ((options->groupflag && (options->grouppos >= recsize || ptr[options->grouppos] != (UCHAR) options->groupchar))
				   || (!options->groupflag && options->grouppos < recsize && ptr[options->grouppos] == (UCHAR) options->groupchar)) {
					if (!firstflag) firstflag = ARG_GROUP;
					continue;
				}
				if (!firstflag) firstflag = ARG_HEADER;
			}
			for (i1 = 0, recsize = recpos; i1 < options->fieldcnt; recsize += i2, i1++) {
				i2 = (*options->fieldinfo)[i1].len;
				memcpy(rec + recsize, ptr + (*options->fieldinfo)[i1].pos, i2);
			}
		}
#if OS_WIN32
		else *(USHORT *) rec = (USHORT) recsize;
#else
		else {
			rec[0] = (UCHAR)(recsize >> 8);
			rec[1] = (UCHAR) recsize;
		}
#endif
		i1 = sioputend();
		if (i1) {
			utilputerror(siogeterr());
			sioexit();
			rc = UTILERR_SORT;
			goto utilsortend;
		}
	}
	i1 = rioclose(inhandle);
	inhandle = 0;
	if (i1) {
		utilputerror(fioerrstr(i1));
		sioexit();
		rc = UTILERR_CLOSE;
	}

	if ((options->flags & (ARG_HEADER | ARG_2PASSSORT)) && !(options->flags & (ARG_POSNTAG & ARG_KEYTAG))) {  /* reopen input file with no buffering */
		if (options->flags & ARG_HEADER) i1 = 4;  /* 4K buffer for group records */
		else i1 = 0;
		inhandle = rioopen(options->firstfile, openflag, i1, maxrec);
		if (inhandle < 0) {
			utilputerror(fioerrstr(inhandle));
			sioexit();
			rc = UTILERR_OPEN;
			goto utilsortend;
		}
	}
	else firstflag = 0;  /* prevent (firstflag == ARG_GROUP) code below */

	/* reduce rio buffer requirements */
	if (options->flags & (ARG_POSNTAG & ARG_KEYTAG)) maxlen = reclen;
	else maxlen = maxrec;

	/* create the output file */
	i2 = fiogetflags() & FIO_FLAG_UTILPREPDOS;
	if (i2) {  /* change prep directory to be same as input file */
		ptr = (UCHAR *) fioname(inhandle);
		if (ptr != NULL) {
			i1 = fioaslash((CHAR *) ptr) + 1;
			if (i1) memcpy(work, ptr, i1);
			work[i1] = 0;
			pptr = fiogetopt(FIO_OPT_PREPPATH);
			if (pptr != NULL) strcpy((CHAR *) *bufptr, (CHAR *) *pptr);
			fiosetopt(FIO_OPT_PREPPATH, work);
		}
		else i2 = 0;
	}
	outhandle = rioopen(options->secondfile, createflag, RIO_BUF_SIZE, maxlen);
	if (i2) {  /* restore prep directory */
		if (pptr != NULL) fiosetopt(FIO_OPT_PREPPATH, *bufptr);
		else fiosetopt(FIO_OPT_PREPPATH, NULL);
	}
	if (outhandle < 0) {
		utilputerror(fioerrstr(outhandle));
		sioexit();
		rc = UTILERR_CREATE;
		goto utilsortend;
	}

	buffer = *bufptr;
	if (firstflag == ARG_GROUP) {
		/* first record of input file was not a header */
		for ( ; ; ) {
			recsize = rioget(inhandle, buffer, maxrec);
			if (recsize < 0) {
				if (recsize == -1) {
					if (options->flags & ARG_EOFERROR) {
						riolastpos(inhandle, &pos);
						rioeofpos(inhandle, &eofpos);
						if (pos != eofpos) {
							sioexit();
							rc = UTILERR_BADEOF;
							goto utilsortend;
						}
					}
					break;
				}
				if (recsize == -2) continue;
				utilputerror(fioerrstr(recsize));
				sioexit();
				rc = UTILERR_READ;
				goto utilsortend;
			}
#if 0
/*** CODE: WANTED TO ADD THIS BUT PREVIOUS VERSIONS DID NOT CHECK ***/
			if ((options->flags & ARG_SELECT) && !utilselect(options, buffer, recsize)) continue;
#endif
			if ((options->groupflag && (options->grouppos >= recsize || buffer[options->grouppos] != (UCHAR) options->groupchar))
			   || (!options->groupflag && options->grouppos < recsize && buffer[options->grouppos] == (UCHAR) options->groupchar)) {
				i1 = rioput(outhandle, buffer, recsize);
				if (i1) {
					utilputerror(fioerrstr(i1));
					sioexit();
					rc = UTILERR_WRITE;
					goto utilsortend;
				}
			}
			else break;
		}
	}

	/* write out the records */
	key = *keyptr;
	if (options->flags & ARG_UNIQUE) {
		if (options->flags & ARG_GROUP) key++;
		if (options->flags & ARG_STABLE) keycnt--;
		firstflag = TRUE;
	}
	for ( ; ; ) {
		i1 = sioget(&rec);
		if (i1) {
			if (i1 == 1) break;
			utilputerror(siogeterr());
			sioexit();
			rc = UTILERR_SORT;
			goto utilsortend;
		}
		if (options->flags & ARG_UNIQUE) {
			if (!firstflag) {
				for (i1 = 0; i1 < keycnt; i1++) {
					for (i2 = key[i1].start, i3 = key[i1].end; i2 < i3; i2++) {
						if (rec[i2] == buffer[i2]) continue;
						if (!collateflag || priority[rec[i2]] != priority[buffer[i2]]) break;
					}
					if (i2 != i3) break;
				}
				if (i1 == keycnt) continue;
			}
			else firstflag = FALSE;
		}
		if (options->flags & (ARG_KEYTAG | ARG_POSNTAG)) {
			ptr = rec + rectagoff;
			if (options->flags & ARG_KEYTAG) recsize = reclen - rectagoff;
			else recsize = keytagsize;
		}
		else if (options->flags & (ARG_HEADER | ARG_2PASSSORT)) {
			mscntooff(rec + rectagoff, &pos, KEYTAG_POS_SIZE);
			riosetpos(inhandle, pos);
			recsize = rioget(inhandle, buffer, maxrec);
			if (recsize < 0) {
				if (recsize == -1) recsize = ERR_RANGE;
				if (recsize == -2) recsize = ERR_ISDEL;
				utilputerror(fioerrstr(recsize));
				sioexit();
				rc = UTILERR_READ;
				goto utilsortend;
			}
			i1 = rioput(outhandle, buffer, recsize);
			if (i1) {
				utilputerror(fioerrstr(i1));
				sioexit();
				rc = UTILERR_WRITE;
				goto utilsortend;
			}
			while (options->flags & ARG_HEADER) {
				recsize = rioget(inhandle, buffer, maxrec);
				if (recsize < 0) {
					if (recsize == -1) {
						if (options->flags & ARG_EOFERROR) {
							riolastpos(inhandle, &pos);
							rioeofpos(inhandle, &eofpos);
							if (pos != eofpos) {
								sioexit();
								rc = UTILERR_BADEOF;
								goto utilsortend;
							}
						}
						break;
					}
					if (recsize == -2) continue;
					utilputerror(fioerrstr(recsize));
					sioexit();
					rc = UTILERR_READ;
					goto utilsortend;
				}
#if 0
/*** CODE: WANTED TO ADD THIS BUT PREVIOUS VERSIONS DID NOT CHECK ***/
				if ((options->flags & ARG_SELECT) && !utilselect(options, buffer, recsize)) continue;
#endif
				if ((options->groupflag && (options->grouppos >= recsize || buffer[options->grouppos] != (UCHAR) options->groupchar))
				   || (!options->groupflag && options->grouppos < recsize && buffer[options->grouppos] == (UCHAR) options->groupchar)) {
					i1 = rioput(outhandle, buffer, recsize);
					if (i1) {
						utilputerror(fioerrstr(i1));
						sioexit();
						rc = UTILERR_WRITE;
						goto utilsortend;
					}
				}
				else break;
			}
			if (options->flags & ARG_UNIQUE) memcpy(buffer, rec, reclen);
			continue;
		}
		else {
			ptr = rec + recpos;
#if OS_WIN32
			recsize = *(USHORT *) rec;
#else
			recsize = ((INT) rec[0] << 8) + rec[1];
#endif
		}
		i1 = rioput(outhandle, ptr, recsize);
		if (i1) {
			utilputerror(fioerrstr(i1));
			sioexit();
			rc = UTILERR_WRITE;
			goto utilsortend;
		}
		if (options->flags & ARG_UNIQUE) memcpy(buffer, rec, reclen);
	}

	sioexit();

	/* clean up */
utilsortend:
	if (outhandle > 0) {
		i1 = rioclose(outhandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_CLOSE;
		}
	}
	if (inhandle > 0) {
		i1 = rioclose(inhandle);
		if (i1 && !rc) {
			utilputerror(fioerrstr(i1));
			rc = UTILERR_CLOSE;
		}
	}
	memfree((UCHAR **) keyptr);
	memfree(bufptr);
	return rc;
}

static INT utildelete(UTILARGS *options) {
	INT i1, inhandle;
	
	miofixname(options->firstfile, ".txt", FIXNAME_EXT_ADD);
	inhandle = fioopen(options->firstfile, FIO_M_EXC | FIO_P_TXT);
	if (inhandle < 0) {
		utilputerror(fioerrstr(inhandle));
		return UTILERR_OPEN;
	}
	else {
		i1 = fiokill(inhandle);
		if (i1 < 0) return UTILERR_DELETE;
	}
	return 0;
}

static INT utilselect(UTILARGS *options, UCHAR *buffer, INT recsize)
{
	INT i1, selcmp, selflag, sellen, selpos, seltype;
	UCHAR c1, *ptr;
	FIELDINFO *selectinfo;

	selflag = 1;
	for (selectinfo = *options->selectinfo, i1 = options->selectcnt; --i1 >= 0; selectinfo++) {
		seltype = selectinfo->type;
		if (seltype & SELECT_OR) {
			if (selflag) break;
			selflag = 1;
		}
		selpos = selectinfo->pos;
		if (seltype & SELECT_STRING) sellen = selectinfo->len;
		else sellen = 1;
		if (selpos + sellen > recsize) selflag = 0;
		if (!selflag) continue;
		ptr = (UCHAR *)(*options->stringinfo + selectinfo->off);
		if (sellen > 1) selcmp = memcmp(buffer + selpos, ptr, sellen);
		else {
			c1 = buffer[selpos];
			if (!(seltype & (SELECT_GREATER | SELECT_LESS)) || ((seltype & (SELECT_GREATER | SELECT_LESS)) == (SELECT_GREATER | SELECT_LESS))) {
				for ( ; *ptr && c1 != *ptr; ptr++);
				selcmp = !*ptr;
			}
			else selcmp = (INT) c1 - (INT) *ptr;
		}
		/* used the 'else' for readability */
		if (((seltype & SELECT_EQUAL) && !selcmp) ||
			((seltype & SELECT_GREATER) && selcmp > 0) ||
			((seltype & SELECT_LESS) && selcmp < 0)) /* do nothing here */ ; // @suppress("Suspicious semicolon")
		else selflag = 0;
	}
	if (options->flags & ARG_REVSELECT) selflag = !selflag;
	return selflag;
}

/* converts string in place to zero terminated args with zero length last arg */
/* NOTE: requires buffer to be two chars larger than length */
static INT utilstrtoargs(CHAR *buffer, INT length)
{
	INT i1, i2, i3, quoteflag;

	if (length == -1) length = (INT)strlen(buffer);
	for (i1 = i2 = 0; i1 < length; i1++) {
		if (isspace(buffer[i1])) continue;
		for (i3 = i2, quoteflag = 0; i1 < length; i1++) {
			if (isspace(buffer[i1])) {
				if (!quoteflag) break;
			}
			else if (buffer[i1] == '\\') {
				if (i1 + 1 < length && buffer[i1 + 1] == '"') i1++;
			}
			else if (buffer[i1] == '"') {
				quoteflag ^= 1;
				continue;
			}
			buffer[i2++] = buffer[i1];
		}
		if (i2 != i3) buffer[i2++] = '\0';
	}
	buffer[i2] = '\0';
	return 0;
}

static INT utilparseargs(INT type, CHAR *args, UTILARGS *options)
{
	INT i1, i2, i3, i4, handle, quoteflag, rc;
	OFFSET offset;
	CHAR work[MAX_NAMESIZE], *optargs, *ptr;
	UCHAR c1;

	for (ptr = args; *ptr; ptr += strlen(ptr) + 1) {
		rc = 0; 
		if (*ptr == '-') {
			switch (toupper(ptr[1])) {
			case '!':
				options->flags |= ARG_VERBOSE;
				options->verboseflag |= VERBOSE_EXTRA;
				break;
			case 'A':
				if (!ptr[2]) {
					if (type & (UTIL_BUILD | UTIL_REFORMAT)) options->flags |= ARG_APPEND;
					else if (type & UTIL_EXIST) options->flags |= ARG_EDTCFGPATH | ARG_DBCPATH | ARG_OPENPATH | ARG_PREPPATH | ARG_SRCPATH | ARG_WORKPATH;
					else rc = UTILERR_BADARG;
				}
				else if (ptr[2] == '=' && isdigit(ptr[3]) && (type & (UTIL_AIMDEX | UTIL_INDEX | UTIL_SORT))) {
					options->flags |= ARG_ALLOCATE;
					options->allocate = atoi(ptr + 3);
				}
				else rc = UTILERR_BADARG;
				break;
			case 'B':
				if (ptr[2] == '=' && isdigit(ptr[3])) {
					if (type & UTIL_INDEX) {
						options->flags |= ARG_BLKSIZE;
						options->blksize = atoi(ptr + 3);
						for (i1 = MIN_ISI_BLK_SIZE; i1 < MAX_ISI_BLK_SIZE && i1 != options->blksize; i1 <<= 1);
						if (i1 != options->blksize) rc = UTILERR_BADARG;
					}
					else if (type & UTIL_REFORMAT) rc = utilfieldparm(FIELD_BLANKS, 0, ptr + 3, options);
					else rc = UTILERR_BADARG;
				}
				else rc = UTILERR_BADARG;
				break;
			case 'C':
				if (!ptr[2]) {
					if (type & (UTIL_BUILD | UTIL_REFORMAT | UTIL_SORT)) options->flags |= ARG_COMPRESSED;
					else if (type & UTIL_EXIST) options->flags |= ARG_EDTCFGPATH;
					else rc = UTILERR_BADARG;
				}
				else rc = UTILERR_BADARG;
				break;
			case 'D':
				if (!ptr[2]) {
					if (type & UTIL_AIMDEX) options->flags |= ARG_DISTINCT;
					else if (type & (UTIL_BUILD | UTIL_CREATE | UTIL_REFORMAT | UTIL_SORT)) options->flags = (options->flags & ~ARG_TYPEMASK) | ARG_TYPEDATA;
					else if (type & UTIL_COPY) options->flags |= ARG_DELETE;
					else if (type & UTIL_ENCODE) options->flags |= ARG_DECODE;
					else if (type & UTIL_EXIST) options->flags |= ARG_DBCPATH;
					else if (type & UTIL_INDEX) options->flags |= ARG_DUPLICATES;
					else rc = UTILERR_BADARG;
				}
				else rc = UTILERR_BADARG;
				break;
			case 'E':
				if (!ptr[2] && (type & (UTIL_AIMDEX | UTIL_INDEX))) options->flags |= ARG_REINDEX;
				else rc = UTILERR_BADARG;
				break;
			case 'F':
				if (!ptr[2]) {
					if (type & UTIL_AIMDEX) options->flags |= ARG_FIXED;
					else if (type & UTIL_EXIST) options->flags |= ARG_OPENPATH;
					else if (type & UTIL_INDEX) options->flags |= ARG_IGNOREDUP;
					else if (type & UTIL_SORT) options->flags |= ARG_POSNTAG;
					else rc = UTILERR_BADARG;
				}
				else if (ptr[2] == '=') {
					if ((type & UTIL_AIMDEX) && isdigit(ptr[3])) {
						options->flags |= ARG_FIXED | ARG_LENGTH;
/*** CODE: MAYBE CHECK FOR VALID RANGE (1 TO MAX) ***/
						options->length = atoi(ptr + 3);
					}
					else if (ptr[3]) {
						if (type & UTIL_INDEX) {
							options->flags |= ARG_IGNOREDUP;
							strcpy(options->xtrafile1, ptr + 3);
						}
						else if (type & UTIL_REFORMAT) rc = utilfieldparm(FIELD_STRING, 0, ptr + 3, options);
						else rc = UTILERR_BADARG;
					}
					else rc = UTILERR_BADARG;
				}
				else rc = UTILERR_BADARG;
				break;
			case 'G':
				if (isdigit(ptr[2]) && (type & UTIL_SORT) && !(options->flags & (ARG_GROUP | ARG_HEADER))) {
					for (i1 = 2, i2 = 0; isdigit(ptr[i1]); ) i2 = i2 * 10 + ptr[i1++] - '0';
					if (i2 >= 1 && i2 <= RIO_MAX_RECSIZE && (ptr[i1] == '=' || ptr[i1] == '#') && ptr[i1 + 1]) {
						if (ptr[i1] == '=') options->groupflag = TRUE;
						else if (ptr[i1] == '#') options->groupflag = FALSE;
						options->grouppos = i2 - 1;
						options->groupchar = (UCHAR) ptr[i1 + 1];
						options->flags |= ARG_GROUP;
					}
					else rc = UTILERR_BADARG;
				}
				else rc = UTILERR_BADARG;
				break;
			case 'H':
				if (isdigit(ptr[2]) && (type & UTIL_SORT) && !(options->flags & (ARG_GROUP | ARG_HEADER))) {
					for (i1 = 2, i2 = 0; isdigit(ptr[i1]); ) i2 = i2 * 10 + ptr[i1++] - '0';
					if (i2 >= 1 && i2 <= RIO_MAX_RECSIZE && (ptr[i1] == '=' || ptr[i1] == '#') && ptr[i1 + 1]) {
						if (ptr[i1] == '=') options->groupflag = TRUE;
						else if (ptr[i1] == '#') options->groupflag = FALSE;
						options->grouppos = i2 - 1;
						options->groupchar = ptr[i1 + 1];
						options->flags |= ARG_HEADER;
					}
					else rc = UTILERR_BADARG;
				}
				else rc = UTILERR_BADARG;
				break;
			case 'I':
				if (!ptr[2] && (type & UTIL_REFORMAT)) options->flags |= ARG_TABINSERT;
				else rc = UTILERR_BADARG;
				break;
			case 'J':
				if (type & (UTIL_AIMDEX | UTIL_BUILD | UTIL_COPY | UTIL_ENCODE | UTIL_INDEX | UTIL_REFORMAT | UTIL_SORT)) {
					options->flags |= ARG_SHARE;
					if (!ptr[2]) options->shareflag = SHARE_READWRITE;
					else if (toupper(ptr[2]) == 'R' && !ptr[3]) options->shareflag = SHARE_READONLY;
					else if (toupper(ptr[2]) == 'A' && !ptr[3]) options->shareflag = SHARE_READACCESS;
					else rc = UTILERR_BADARG;
				}
				else rc = UTILERR_BADARG;
				break;
			case 'K':
				if (!ptr[2] && (type & (UTIL_REFORMAT | UTIL_SORT))) options->flags |= ARG_KEYTAG;
				else if (ptr[2] == '=' && ptr[3] && (type & UTIL_INDEX)) {
					options->flags |= ARG_KEYTAGFILE;
					strcpy(options->xtrafile2, ptr + 3);
				}
				else rc = UTILERR_BADARG;
				break;
			case 'L':
				if (ptr[2] == '=' && isdigit(ptr[3]) && (type & (UTIL_CREATE | UTIL_REFORMAT))) {
					options->flags |= ARG_LENGTH;
					options->length = atoi(ptr + 3);
				}
				else rc = UTILERR_BADARG;
				break;
			case 'M':
				if (ptr[2] == '=') {
					if ((type & UTIL_AIMDEX) && ptr[3]) {
						options->flags |= ARG_MATCH;
						options->matchchar = (UCHAR) ptr[3];
					}
					else if ((type & UTIL_SORT) && isdigit(ptr[3])) {
						options->flags |= ARG_MAXLENGTH;
						options->maxlength = atoi(ptr + 3);
					}
					else rc = UTILERR_BADARG;
				}
				else rc = UTILERR_BADARG;
				break;
			case 'N':
				if (ptr[2] == '=' && ptr[3]) {
					if (type & UTIL_AIMDEX) {
						options->flags |= ARG_PRIMARY;
						if (ptr[3] == '+') {
							options->addflag = TRUE;
							i1 = 4;
						}
						else {
							options->addflag = FALSE;
							i1 = 3;
						}
						if (isdigit(ptr[i1])) mscatooff(ptr + i1, &options->primary);
						else rc = UTILERR_BADARG;
					}
					else if (type & (UTIL_BUILD | UTIL_REFORMAT)) {
						options->flags |= ARG_TRANSLATE;
						strcpy(options->xtrafile1, ptr + 3);
					}
					else if (type & UTIL_CREATE) {
						options->flags |= ARG_COUNT;
						if (isdigit(ptr[3])) mscatooff(ptr + 3, &options->count);
						else rc = UTILERR_BADARG;
					}
					else rc = UTILERR_BADARG;
				}
				else rc = UTILERR_BADARG;
				break;
			case 'O':
				if (ptr[2] == '=' && ptr[3]) {
					/* open file through fio to use search path */
					strcpy(work, ptr + 3);
					miofixname(work, ".txt", FIXNAME_EXT_ADD);
					handle = fioopen(work, FIO_M_SRO | FIO_P_TXT);
					if (handle < 0) {
						utilputerror(ptr);
						rc = UTILERR_BADOPT;
						break;
					}
					fiogetsize(handle, &offset);
					i1 = (int) offset;
					optargs = (char *) malloc(i1 + 1);
					if (optargs == NULL) {
						fioclose(handle);
						rc = UTILERR_NOMEM;
						break;
					}
					i2 = fioread(handle, 0, (UCHAR *) optargs, i1);
					fioclose(handle);
					if (i2 != i1) {
						free(optargs);
						rc = UTILERR_BADOPT;
						break;
					}

					/* clean-up options file */
					for (i2 = i3 = i4 = 0, quoteflag = FALSE; i2 < i1; ) {
						c1 = (UCHAR) optargs[i2++];
						if (c1 == '"') {
							quoteflag = !quoteflag;
							continue;
						}
						if (c1 == '\\' && optargs[i2] == '"') c1 = (UCHAR) optargs[i2++];
						else if (c1 == 0x0D || c1 == 0x0A || c1 == 0x1A || c1 == 0xFA || c1 == 0xFB || (!quoteflag && isspace(c1))) {
							if (i4 != i3) {
								optargs[i4++] = 0;
								i3 = i4;
							}
							quoteflag = FALSE;
							continue;
						}
						optargs[i4++] = (CHAR) c1;
					}
					if (i4 != i3) optargs[i4++] = 0;
					optargs[i4] = 0;
					rc = utilparseargs(type, optargs, options);
					free(optargs);
				}
				else rc = UTILERR_BADARG;
				break;
			case 'P':
				if (!ptr[2] && (type & UTIL_EXIST)) options->flags |= ARG_PREPPATH;
				else if (isdigit(ptr[2]) && (type & (UTIL_AIMDEX | UTIL_BUILD | UTIL_INDEX | UTIL_REFORMAT | UTIL_SORT)))
					rc = utilselectparm(ptr + 2, options);
				else rc = UTILERR_BADARG;
				break;
			case 'Q':
				if (!ptr[2] && (type & UTIL_SORT)) options->flags |= ARG_2PASSSORT;
				else rc = UTILERR_BADARG;
				break;
			case 'R':
				if (!ptr[2]) {
					if (type & (UTIL_AIMDEX | UTIL_INDEX)) options->flags |= ARG_RENAME;
					else if (type & UTIL_REFORMAT) options->flags |= ARG_SPEED;
					else if (type & UTIL_SORT) options->flags |= ARG_REVERSE;
					else rc = UTILERR_BADARG;
				}
				else rc = UTILERR_BADARG;
				break;
			case 'S':
				if (!ptr[2]) {
					if (type & (UTIL_AIMDEX | UTIL_INDEX)) options->flags |= ARG_RECLAIM;
					else if (type & UTIL_EXIST) options->flags |= ARG_SRCPATH;
					else if (type & UTIL_SORT) options->flags |= ARG_STABLE;
					else rc = UTILERR_BADARG;
				}
				else if (ptr[2] == '=' && ptr[3]) {
					if (type & (UTIL_BUILD | UTIL_REFORMAT)) {
						options->flags |= ARG_NONSELECT;
						strcpy(options->xtrafile1, ptr + 3);
					}
					else if (isdigit(ptr[3]) && (type & UTIL_INDEX)) {
						options->flags |= ARG_RECLAIM | ARG_LENGTH;
						options->length = atoi(ptr + 3);
					}
					else rc = UTILERR_BADARG;
				}
				else rc = UTILERR_BADARG;
				break;
			case 'T':
				if (!ptr[2]) {
					if (type & (UTIL_AIMDEX | UTIL_INDEX)) options->flags |= ARG_TEXTSRCH;
					else if (type & (UTIL_BUILD | UTIL_CREATE | UTIL_REFORMAT | UTIL_SORT)) options->flags = (options->flags & ~ARG_TYPEMASK) | ARG_TYPETEXT;
					else rc = UTILERR_BADARG;
				}
				else if (ptr[2] == '=') {
					strcpy(work, &ptr[3]);
					for (i1 = 0; work[i1]; i1++) work[i1] = (CHAR) toupper(work[i1]);
					if (!strcmp(work, "BIN")) i1 = ARG_TYPEBIN;
					else if (!strcmp(work, "DATA")) i1 = ARG_TYPEDATA;
					else if (!strcmp(work, "DOS")) i1 = ARG_TYPEDOS;
					else if (!strcmp(work, "MAC")) i1 = ARG_TYPEMAC;
					else if (!strcmp(work, "STD")) i1 = ARG_TYPESTD;
					else if (!strcmp(work, "TEXT")) i1 = ARG_TYPETEXT;
					else rc = UTILERR_BADARG;
					options->flags = (options->flags & ~ARG_TYPEMASK) | i1;
				}
				else rc = UTILERR_BADARG;
				break;
			case 'U':
				if (!ptr[2] && (type & UTIL_SORT)) options->flags |= ARG_UNIQUE;
				else rc = UTILERR_BADARG;
				break;
			case 'V':
				options->flags |= ARG_VERBOSE;
				options->verboseflag |= VERBOSE_NORMAL;
				break;
			case 'W':
				if (!ptr[2] && (type & UTIL_EXIST)) options->flags |= ARG_WORKPATH;
				else if (ptr[2] == '=' && ptr[3] && (type & (UTIL_INDEX | UTIL_SORT))) {
					options->flags |= ARG_WORKFILE;
					strcpy(options->workfile, ptr + 3);
				}
				else rc = UTILERR_BADARG;
				break;
			case 'X':
				if (!ptr[2] && (type & (UTIL_AIMDEX | UTIL_BUILD | UTIL_INDEX | UTIL_REFORMAT | UTIL_SORT)))
					options->flags |= ARG_REVSELECT;
				else if (ptr[2] == '=' && isdigit(ptr[3])) {
					if (type & UTIL_AIMDEX) {
						options->flags |= ARG_SECONDARY;
						mscatooff(ptr + 3, &options->secondary);
					}
					else if (type & UTIL_REFORMAT) {
						i1 = atoi(ptr + 3);
						if (i1 > 1 && i1 <= RIO_MAX_RECSIZE) {
							if (options->tabinfo == NULL) {
								options->tabinfo = (INT **) memalloc(sizeof(INT) * 16, 0);
								if (options->tabinfo == NULL) {
									rc = UTILERR_NOMEM;
									break;
								}
								options->tabsize = 16;
							}
							if (options->tabcnt == options->tabsize) {
								if (memchange((UCHAR **) options->tabinfo,
										sizeof(INT) * (size_t)(options->tabsize << 1), 0)) { // @suppress("Symbol is not resolved")
									rc = UTILERR_NOMEM;
									break;
								}
								options->tabsize <<= 1;
							}
							for (i2 = options->tabcnt++; i2-- > 0; ) {
								if (i1 > (*options->tabinfo)[i2]) break;
								(*options->tabinfo)[i2 + 1] = (*options->tabinfo)[i2];
							}
							(*options->tabinfo)[i2 + 1] = i1 - 1;
						}
						else if (i1 < 1) rc = UTILERR_BADARG;
					}
					else rc = UTILERR_BADARG;
				}
				else rc = UTILERR_BADARG;
				break;
			case 'Y':
				if (!ptr[2] && (type & (UTIL_AIMDEX | UTIL_BUILD | UTIL_INDEX | UTIL_REFORMAT | UTIL_SORT)))
					options->flags |= ARG_EOFERROR;
				else rc = UTILERR_BADARG;
				break;
			case 'Z':
				if (ptr[2] == '=' && isdigit(ptr[3]) && (type & (UTIL_AIMDEX | UTIL_REFORMAT))) {
					if (type & UTIL_AIMDEX) {
						i1 = atoi(ptr + 3);
						if (i1 >= 40 && i1 <= 2000) {
							options->flags |= ARG_ZVALUE;
							options->zvalue = i1;
						}
						else rc = UTILERR_BADARG;
					}
					else rc = utilfieldparm(FIELD_DATE, 0, ptr + 3, options);
				}
				else if (ptr[3] == '=' && isdigit(ptr[4]) && (type & UTIL_REFORMAT)) {
					switch (toupper(ptr[2])) {
					case 'C':
						i1 = atoi(ptr + 4);
						if (i1 >= 1 && i1 <= 99) options->datecutoff = i1;  /* zero is reserved as not set (use default) */
						else rc = UTILERR_BADARG;
						break;
					case 'D':
						rc = utilfieldparm(FIELD_DATEDUP, 0, ptr + 4, options);
						break;
					case 'R':
						rc = utilfieldparm(FIELD_DATEREV, 0, ptr + 4, options);
						break;
					case 'X':
						rc = utilfieldparm(FIELD_DATEALT1, 0, ptr + 4, options);
						break;
					default:
						rc = UTILERR_BADARG;
						break;
					}
				}
				else rc = UTILERR_BADARG;
				break;
			default:
				rc = UTILERR_BADARG;
				break;
			}
		}
		else if ((type & UTIL_1STFILE) && !(options->flags & ARG_1STFILE)) {
			type &= ~UTIL_1STFILE;
			options->flags |= ARG_1STFILE;
			strcpy(options->firstfile, ptr);
		}
		else if ((type & UTIL_2NDFILE) && !(options->flags & ARG_2NDFILE)) {
			type &= ~(UTIL_1STFILE | UTIL_2NDFILE);
			options->flags |= ARG_2NDFILE;
			strcpy(options->secondfile, ptr);
		}
		else {
			if ((type & UTIL_2NDCHECK) && !(options->flags & ARG_2NDFILE)) {
				type &= ~(UTIL_1STFILE | UTIL_2NDFILE | UTIL_2NDCHECK);
				for (i1 = 0; isdigit(ptr[i1]); i1++);
				if (i1 && ptr[i1] == '-') for (i1++; isdigit(ptr[i1]); i1++);
				if (i1 && isalpha(ptr[i1])) i1++;
				if (ptr[i1]) {  /* not field specification */
					options->flags |= ARG_2NDFILE;
					strcpy(options->secondfile, ptr);
					continue;
				}
			}
			if (isdigit(ptr[0])) {
				if (type & (UTIL_AIMDEX | UTIL_INDEX | UTIL_REFORMAT | UTIL_SORT)) rc = utilfieldparm(FIELD_SPEC, type, ptr, options);
				else if (type & UTIL_BUILD) rc = utilrecparm(ptr, options);
				else rc = UTILERR_BADARG;
			}
			else if (toupper(ptr[0]) == 'O' && toupper(ptr[1]) == 'R' && !ptr[2] &&
				(type & (UTIL_AIMDEX | UTIL_BUILD | UTIL_INDEX | UTIL_REFORMAT | UTIL_SORT))) {
				if (options->flags & ARG_SELECT) options->flags |= ARG_OR;
			}
			else rc = UTILERR_BADARG;
		}
		if (rc) {
			if (rc == UTILERR_BADARG) utilputerror(ptr);
			memfree((UCHAR **) options->fieldinfo);
			memfree((UCHAR **) options->recinfo);
			memfree((UCHAR **) options->selectinfo);
			memfree((UCHAR **) options->stringinfo);
			memfree((UCHAR **) options->tabinfo);
			memset(options, 0, sizeof(UTILARGS));
			return rc;
		}
	}
	return 0;
}

static INT utilfieldparm(INT type, INT utiltype, CHAR *ptr, UTILARGS *options)
{
	INT i1, i2, i3;
	UCHAR c1;
	FIELDINFO *fieldinfo;

	if (options->fieldinfo == NULL) {
		options->fieldinfo = (FIELDINFO **) memalloc(sizeof(FIELDINFO) * 32, 0);
		if (options->fieldinfo == NULL) return UTILERR_NOMEM;
		options->fieldsize = 32;
	}
	if (options->fieldcnt == options->fieldsize) {
		if (memchange((UCHAR **) options->fieldinfo,
			sizeof(FIELDINFO) * (size_t)(options->fieldsize << 1), 0)) return UTILERR_NOMEM; // @suppress("Symbol is not resolved")
		options->fieldsize <<= 1;
	}
	options->flags |= ARG_FLDSPEC;
	fieldinfo = *options->fieldinfo + options->fieldcnt;
	fieldinfo->type = type;
	if (type == FIELD_BLANKS) fieldinfo->len = atoi(ptr);
	else if (type == FIELD_STRING) {
		if (options->stringinfo == NULL) {
			options->stringinfo = (CHAR **) memalloc(sizeof(CHAR) * 512, 0);
			if (options->stringinfo == NULL) return UTILERR_NOMEM;
			options->stringsize = 512;
		}
		i1 = (INT)strlen(ptr) + 1;
		if (options->stringcnt + i1 > options->stringsize) {
			for (i2 = 512; i2 < options->stringcnt + i1; i2 <<= 1);
			if (memchange((UCHAR **) options->stringinfo, sizeof(CHAR) * i2, 0)) return UTILERR_NOMEM;
			options->stringsize = i2;
		}
		fieldinfo = *options->fieldinfo + options->fieldcnt;
		fieldinfo->len = i1 - 1;
		fieldinfo->off = options->stringcnt;
		memcpy(*options->stringinfo + options->stringcnt, ptr, i1);
		options->stringcnt += i1;
	}
	else if (type == FIELD_SPEC) {
		for (i1 = i2 = 0; isdigit(ptr[i1]); ) i2 = i2 * 10 + ptr[i1++] - '0';
		if (ptr[i1] == '-') {
			for (i1++, i3 = 0; isdigit(ptr[i1]); ) i3 = i3 * 10 + ptr[i1++] - '0';
		}
		else i3 = i2;
		if (ptr[i1]) {
			if ((utiltype & UTIL_AIMDEX) && toupper(ptr[i1]) == 'X') {
				fieldinfo->type |= FIELD_EXCLUDE;
				i1++;
			}
			if (utiltype & UTIL_SORT) {
				while (ptr[i1]) {
					c1 = (UCHAR) toupper(ptr[i1]);
					if (c1 == 'A') fieldinfo->type |= FIELD_ASCEND;
					else if (c1 == 'D') fieldinfo->type |= FIELD_DESCEND;
					else if (c1 == 'N') fieldinfo->type |= FIELD_NUMERIC;
					else break;
					i1++;
				}
			}
		}
		if (ptr[i1] || i2 < 1 || i2 > i3 || i3 > RIO_MAX_RECSIZE) return UTILERR_BADARG;
		fieldinfo->pos = --i2;
		fieldinfo->len = i3 - i2;
	}
	else {  /* date type */
		i1 = atoi(ptr);
		if (i1 < 1 || i1 > RIO_MAX_RECSIZE) return UTILERR_BADARG;
		fieldinfo->pos = i1 - 1;
		fieldinfo->len = 2;
		if (options->datecutoff) fieldinfo->off = options->datecutoff;
		else fieldinfo->off = 20;
	}
	options->fieldcnt++;
	return 0;
}

static INT utilrecparm(CHAR *ptr, UTILARGS *options)
{
	INT i1;
	OFFSET off1, off2;
	RECINFO *recinfo;

	if (options->recinfo == NULL) {
		options->recinfo = (RECINFO **) memalloc(sizeof(RECINFO) * 16, 0);
		if (options->recinfo == NULL) return UTILERR_NOMEM;
		options->recsize = 16;
	}
	if (options->reccnt == options->recsize) {
		if (memchange((UCHAR **) options->recinfo,
			sizeof(FIELDINFO) * (size_t)(options->recsize << 1), 0)) return UTILERR_NOMEM; // @suppress("Symbol is not resolved")
		options->recsize <<= 1;
	}
	recinfo = *options->recinfo + options->reccnt;
	options->flags |= ARG_RECSPEC;
	for (i1 = 0, off1 = 0; isdigit(ptr[i1]); ) off1 = off1 * 10 + ptr[i1++] - '0';
	if (ptr[i1] == '-') {
		for (i1++, off2 = 0; isdigit(ptr[i1]); ) off2 = off2 * 10 + ptr[i1++] - '0';
	}
	else off2 = off1;
	if (ptr[i1] || off1 < 1 || off1 > off2) return UTILERR_BADARG;
	recinfo->start = off1 - 1;
	recinfo->end = off2;
	options->reccnt++;
	return 0;
}

static INT utilselectparm(CHAR *ptr, UTILARGS *options)
{
	INT i1, i2, i3, i4, i5;
	UCHAR c1, c2;
	FIELDINFO *selectinfo;

	if (options->selectinfo == NULL) {
		options->selectinfo = (FIELDINFO **) memalloc(sizeof(FIELDINFO) * 16, 0);
		if (options->selectinfo == NULL) return UTILERR_NOMEM;
		options->selectsize = 16;
	}
	if (options->selectcnt == options->selectsize) {
		if (memchange((UCHAR **) options->selectinfo,
			sizeof(FIELDINFO) * (size_t)(options->selectsize << 1), 0)) return UTILERR_NOMEM; // @suppress("Symbol is not resolved")
		options->selectsize <<= 1;
	}
	options->flags |= ARG_SELECT;
	if (options->stringinfo == NULL) {
		options->stringinfo = (CHAR **) memalloc(sizeof(CHAR) * 512, 0);
		if (options->stringinfo == NULL) return UTILERR_NOMEM;
		options->stringsize = 512;
	}
	selectinfo = *options->selectinfo + options->selectcnt;

	/* parse select condition */
	for (i1 = i2 = 0; isdigit(ptr[i1]); ) i2 = i2 * 10 + ptr[i1++] - '0';
	if (ptr[i1] == '-') {
		for (i1++, i3 = 0; isdigit(ptr[i1]); ) i3 = i3 * 10 + ptr[i1++] - '0';
	}
	else i3 = i2;
	if (i2 < 1 || i2 > i3 || i3 > RIO_MAX_RECSIZE) return UTILERR_BADARG;
	selectinfo->pos = --i2;
	i3 -= i2;
	i2 = 0;
	c1 = (UCHAR) toupper(ptr[i1]);
	if (c1 == '=') i2 = SELECT_EQUAL;
	else if (c1 == '#') i2 = SELECT_LESS | SELECT_GREATER;
	else if (c1) {
		i1++;
		c2 = (UCHAR) toupper(ptr[i1]);
		if (c1 == 'E' && c2 == 'Q') i2 = SELECT_EQUAL;
		else if (c1 == 'N' && c2 == 'E') i2 = SELECT_LESS | SELECT_GREATER;
		else if (c1 == 'G') {
			if (c2 == 'T') i2 = SELECT_GREATER;
			else if (c2 == 'E') i2 = SELECT_GREATER | SELECT_EQUAL;
		}
		else if (c1 == 'L') {
			if (c2 == 'T') i2 = SELECT_LESS;
			else if (c2 == 'E') i2 = SELECT_LESS | SELECT_EQUAL;
		}
	}
	i4 = (INT)strlen(ptr + ++i1);
	if (!i2 || !i4) return UTILERR_BADARG;
	if (i3 > 1) {
		if (i4 > i3) i4 = i3;
		i5 = i3;
		i2 |= SELECT_STRING;
	}
	else i5 = i4;
	if (options->flags & ARG_OR) {
		options->flags &= ~ARG_OR;
		i2 |= SELECT_OR;
	}
	selectinfo->type = i2;
	selectinfo->off = options->stringcnt;
	selectinfo->len = i5;
	if (options->stringcnt + i5 + 1 > options->stringsize) {
		for (i2 = 512; i2 < options->stringcnt + i5 + 1; i2 <<= 1);
		if (memchange((UCHAR **) options->stringinfo, sizeof(CHAR) * i2, 0)) return UTILERR_NOMEM;
		options->stringsize = i2;
	}
	memcpy(*options->stringinfo + options->stringcnt, ptr + i1, i4);
	options->stringcnt += i4;
	if (i3 > i4) {
		memset(*options->stringinfo + options->stringcnt, ' ', i3 - i4);
		options->stringcnt += i3 - i4;
	}
	(*options->stringinfo)[options->stringcnt++] = 0;
	options->selectcnt++;
	return 0;
}
