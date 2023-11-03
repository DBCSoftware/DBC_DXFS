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
#define INC_STDLIB
#define INC_CTYPE
#define INC_LIMITS
#include "includes.h"
#include "base.h"
#include "dbc.h"
#include "fio.h"
#include "fsfileio.h"
#include "que.h"

/* disk input/output routines */

#if OS_WIN32
#define NIO_LOADED  0x01
#define NIO_FPOSIT  0x02
#define NIO_REPOSIT 0x04
#endif

typedef struct fileinfostruct {
	UCHAR type;			/* DAVB_xxxx  see dbc.h */
	UCHAR partflg;		/* 0 = no partial, 1 = partial read, 2 = partial write */
	INT flags;			/* FLAG_xxx   see below */
	INT handle;			/* text file handle */
	INT xhandle;		/* index or aim file handle */
	INT reclen;			/* record length */
	INT lastlen;		/* last record length read or written */
	INT partpos;		/* retain start position of partial reads/writes */
	INT module;			/* data module of DAVB */
	INT offset;			/* data offset of DAVB */
	INT fnbcount;		/* byte count of file name allocation, used only if debugging */
	CHAR **filename;	/* file name, used only if debugging is on */
	OFFSET aimrec;		/* aimdex record number, -1 if invalid */
	OFFSET aimpos;		/* next aimdex record position */
	OFFSET lastpos;		/* partial write last position */
} FILEINFO;

#define FLAG_VAR DAVB_FLAG_VAR  /* variable length records */
#define FLAG_CMP DAVB_FLAG_CMP  /* compressed records */
#define FLAG_DUP DAVB_FLAG_DUP  /* duplicate keys allowed in index */
#define FLAG_TXT DAVB_FLAG_TXT  /* text type file */
#define FLAG_VKY DAVB_FLAG_VKY  /* index key is a numeric variable */
#define FLAG_NAT DAVB_FLAG_NAT  /* native type file */
#define FLAG_VRS DAVB_FLAG_VRS  /* rec size is a numeric variable */
#define FLAG_KLN DAVB_FLAG_KLN  /* ifile declaration includes keylen */
#define FLAG_NOD DAVB_FLAG_NOD  /* no duplicate keys allowed in index */
#define FLAG_CBL DAVB_FLAG_CBL  /* cobol type file */
#define TYPE_MSK DAVB_TYPE_MSK
#define TYPE_TXT DAVB_TYPE_TXT  /* text type file */
#define TYPE_DAT DAVB_TYPE_DAT  /* data type file */
#define TYPE_DOS DAVB_TYPE_DOS  /* dos type file */
#define TYPE_BIN DAVB_TYPE_BIN  /* binary type file */
#define FLAG_RDO 0x00010000  /* file is read only (used, but not needed) */
#define FLAG_EXC 0x00020000  /* file is exclusive (used, but not needed) */
#define FLAG_AFX 0x00040000  /* fixed length aim access */
#define FLAG_ALK 0x00080000  /* auto locking */
#define FLAG_SLK 0x00100000  /* single locks */
#define FLAG_TLK 0x00200000  /* nowait on locks */
#define FLAG_UPD 0x00400000  /* position is ok for update */
#define FLAG_COM 0x00800000  /* file is common */
#define FLAG_SRV 0x01000000  /* file is accessed through a file server */
#define FLAG_INV 0x02000000  /* read access has been invalidated by an aim insert */

/* note: dioflags is currently an uchar */
#define DIOFLAG_NOSTATIC	0x01		/* disallow support of static record buffering */
#define DIOFLAG_WTTRUNC		0x02		/* truncate writes if record is too long */
#define DIOFLAG_OLDUPDT		0x04		/* updates do not blank fill if too short */
#define DIOFLAG_RESETFPI	0x08		/* reset fpi if active */
#define DIOFLAG_DSPERR		0x10		/* display additional error message */
#define DIOFLAG_ASCIIMP		0x20		/* use ascii code for *mp & cobol */
#define DIOFLAG_AIMINSERT	0x40		/* aim inserts do not invalid reads */

#define MAXKEYSIZE 512

int nativerror = 0;  /* error from native i/o */

/* local variables */
static INT filehi;			/* (highest used + 1) array index of open files */
static INT filemax;			/* size of open file arrays */
static FILEINFO **filetabptr;	/* memory pointer to open file table */
static int filepiscnt;
static int filepisarray[40];
static INT preprefnum;		/* reference number of last prep */
static UCHAR *record;		/* record buffer */
static INT recsize_f;			/* size of record in record buffer */
static UINT recpos_f;			/* position of variable in record buffer */
static OFFSET filepos;		/* return file position for all opens */
static UCHAR dioflags;		/* see above flags */

#if OS_WIN32
typedef int (_NIOOPEN)(char *, int, int);
typedef int (_NIOPREP)(char *, char *, int);
typedef int (_NIOCLOSE)(int);
typedef int (_NIORDSEQ)(int, unsigned char *, int);
typedef int (_NIOWTSEQ)(int, unsigned char *, int);
typedef int (_NIORDREC)(int, OFFSET, unsigned char *, int);
typedef int (_NIOWTREC)(int, OFFSET, unsigned char *, int);
typedef int (_NIORDKEY)(int, char *, unsigned char *, int);
typedef int (_NIORDKS)(int, unsigned char *, int);
typedef int (_NIORDKP)(int, unsigned char *, int);
typedef int (_NIOWTKEY)(int, unsigned char *, int);
typedef int (_NIOUPD)(int, unsigned char *, int);
typedef int (_NIODEL)(int, char *);
typedef int (_NIOLCK)(int *);
typedef int (_NIOULCK)(void);
typedef int (_NIOFPOS)(int, OFFSET *);
typedef int (_NIORPOS)(int, OFFSET);
typedef int (_NIOCLRU)(void);
typedef int (_NIOERROR)(void);
static _NIOOPEN *nioopen;
static _NIOPREP *nioprep;
static _NIOCLOSE *nioclose;
static _NIORDSEQ *niordseq;
static _NIOWTSEQ *niowtseq;
static _NIORDREC *niordrec;
static _NIOWTREC *niowtrec;
static _NIORDKEY *niordkey;
static _NIORDKS *niordks;
static _NIORDKP *niordkp;
static _NIOWTKEY *niowtkey;
static _NIOUPD *nioupd;
static _NIODEL *niodel;
static _NIOLCK *niolck;
static _NIOULCK *nioulck;
static _NIOFPOS *niofpos;
static _NIORPOS *niorpos;
static _NIOCLRU *nioclru;
static _NIOERROR *nioerror;
static UCHAR nioinitflag = 0;
static HINSTANCE niohandle;
#else
extern INT nioopen(CHAR *, INT, INT);
extern INT nioprep(CHAR *, CHAR *, INT);
extern INT nioclose(INT);
extern INT niordseq(INT, UCHAR *, INT);
extern INT niowtseq(INT, UCHAR *, INT);
extern INT niordrec(INT, OFFSET, UCHAR *, INT);
extern INT niowtrec(INT, OFFSET, UCHAR *, INT);
extern INT niordkey(INT, CHAR *, UCHAR *, INT);
extern INT niordks(INT, UCHAR *, INT);
extern INT niordkp(INT, UCHAR *, INT);
extern INT niowtkey(INT, UCHAR *, INT);
extern INT nioupd(INT, UCHAR *, INT);
extern INT niodel(INT, CHAR *);
extern INT niolck(INT *);
extern INT nioulck(void);
extern INT niofpos(INT, OFFSET *);
extern INT niorpos(INT, OFFSET);
extern INT nioclru(void);
extern INT nioerror(void);
#endif

/* local function prototypes */
static INT getfileinfo(INT);
static void closedavb(DAVB *, INT);
static void closeFile(FILEINFO *file, INT delflg);
static void savenameinfileinfo(INT refnum);

#if OS_WIN32
static void loadnio(void);
static void freenio(void);
#endif

void vopen()
{
	INT i1, handle, xhandle, fixflg, matchc, keylen, opts, reclen, refnum;
	INT bufs, flags, mask, mod, off, type;
	CHAR work[4096 * 2];
	UCHAR *adr;
	DAVB *davb;
	FILEINFO *file;

	davb = getdavb();
	getlastvar(&mod, &off);
	if (davb->refnum) closedavb(davb, FALSE);

	cvtoname(getvar(VAR_READ));

	type = davb->type;
	flags = davb->info.file.flags;

	bufs = davb->info.file.bufs;
	matchc = 0;
	if (vbcode == 0x2C) /* complex open */ {
		i1 = getbyte();
		if (i1 & 0x01) flags |= FLAG_RDO;
		else if (i1 & 0x02) flags |= FLAG_EXC;
		if (i1 & 0x10) {
			adr = getvar(VAR_READ);
			matchc = adr[fp + hl - 1];
		}
		if (i1 & 0x80) flags |= FLAG_ALK;
		if (i1 & 0x08) flags |= FLAG_SLK;
		if (i1 & 0x04) flags |= FLAG_TLK;
	}

	/* get record length */
	if (flags & FLAG_VRS) {
		adr = findvar(mod, davb->info.file.recsz);
		if (adr != NULL) reclen = nvtoi(adr);
		else reclen = 0;
		if (reclen < 1) {
			if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(610, (UCHAR *) name, (INT) strlen(name));
			dbcerror(610);
		}
	}
	else reclen = davb->info.file.recsz;
	if (reclen > RIO_MAX_RECSIZE) {
		if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(610, (UCHAR *) name, (INT) strlen(name));
		dbcerror(610);
	}

	/* get key length or index number */
	keylen = 0;
	if (type == DAVB_IFILE) {
		if (flags & FLAG_KLN) {
			if (flags & FLAG_VKY) {
				adr = findvar(mod, davb->info.file.keysz);
				if (adr != NULL) keylen = nvtoi(adr);
				else keylen = 0;
				if (keylen < 1) {
					if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(606, (UCHAR *) name, (INT) strlen(name));
					dbcerror(606);
				}
			}
			else keylen = davb->info.file.keysz;
		}
	}

	/* get reference number */
	refnum = getfileinfo(reclen);

	/* open native file */
	xhandle = 0;
	if (flags & FLAG_NAT) {
#if OS_WIN32
		if (!nioinitflag) loadnio();
#endif
		if (type == DAVB_AFILE) {
			if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(651, (UCHAR *) name, (INT) strlen(name));
			dbcerror(651);
		}
		if (flags & FLAG_EXC) i1 = 1;
		else i1 = 0;
		handle = nioopen(name, keylen, i1);
		/* reset memory pointers */
		setpgmdata();
		if (handle < 0) {
			nativerror = INT_MIN;
			if (handle == -2) i1 = 601;
			else if (handle == -3) i1 = 602;
			else if (handle == -99) i1 = 1630 - nioerror();
			else {
				if (handle == -98) nativerror = nioerror();
				i1 = 651;
			}
			if (dioflags & DIOFLAG_DSPERR) {
				if (nativerror == INT_MIN) dbcerrinfo(i1, (UCHAR *) name, -1);
				else {
					sprintf(work, "file name=%s, NIO error=%d", name, nativerror);
					dbcerrinfo(i1, (UCHAR *) work, -1);
				}
			}
			dbcerror(i1);
		}
		goto vopenok;
	}

	i1 = srvgetserver(name, FIO_OPT_OPENPATH);
	if (i1) {
		if (i1 == -1) {
			if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(657, (UCHAR *) name, (INT) strlen(name));
			dbcerror(657);
		}
		xhandle = i1;
		miofixname(name, "", 0);  /* need compat conversions */
		opts = 0;
		if (flags & FLAG_EXC) opts |= FS_EXCLUSIVE;
		else if (flags & FLAG_RDO) opts |= FS_READONLY;
		mask = flags & TYPE_MSK;
		if (mask == TYPE_TXT) opts |= FS_TEXT;
		else if (mask == TYPE_DAT) opts |= FS_DATA;
		else if (mask == TYPE_DOS) opts |= FS_CRLF;
		else if (mask == TYPE_BIN) opts |= FS_BINARY;
		if (flags & FLAG_VAR) opts |= FS_VARIABLE;
		if (flags & FLAG_CMP) opts |= FS_COMPRESSED;
		if (flags & FLAG_ALK) opts |= FS_LOCKAUTO;
		if (flags & FLAG_SLK) opts |= FS_LOCKSINGLE;
		if (flags & FLAG_TLK) opts |= FS_LOCKNOWAIT;
		if (type == DAVB_IFILE) {
			if (flags & FLAG_DUP) opts |= FS_DUP;
			else if (flags & FLAG_NOD) opts |= FS_NODUP;
			handle = fsopenisi(xhandle, name, opts, reclen, keylen);
		}
		else if (type == DAVB_AFILE) handle = fsopenaim(xhandle, name, opts, reclen, (char) matchc);
		else handle = fsopen(xhandle, name, opts, reclen);
		if (handle < 0) {
			if (handle < -650 && handle >= -699 && handle != -656) i1 = 1000 - handle;
			else if (handle < -750 && handle >= -799) i1 = 1000 - handle;
			else if (handle < -1000 && handle > -1049) i1 = 1650 - handle;
			else if (handle == -1) {
				i1 = 658;
				fsgeterror(name, sizeof(name));
			}
			else if (handle > -100) {  /* should not happen, but just in case */
				if (handle == ERR_FNOTF) i1 = 601;
				else if (handle == ERR_NOACC) i1 = 602;
				else if (handle == ERR_BADNM) i1 = 603;
				else if (handle == ERR_BADTP) i1 = 604;
				else if (handle == ERR_BADKL) i1 = 606;
				else if (handle == ERR_IXDUP) i1 = 656;
				else i1 = 1630 - handle;
			}
			else i1 = -handle;
			if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(i1, (UCHAR *) name, (INT) strlen(name));
			dbcerror(i1);
		}
		flags |= FLAG_SRV;
		goto vopenok;
	}

	/* directory open */
	if (type == DAVB_IFILE) {  /* index */
		if (flags & FLAG_EXC) opts = XIO_M_EXC | XIO_P_TXT;
		else if (flags & FLAG_RDO) opts = XIO_M_SRO | XIO_P_TXT;
		else opts = XIO_M_SHR | XIO_P_TXT;
		if ((flags & (FLAG_VAR & FLAG_CMP)) == 0) opts |= XIO_FIX | XIO_UNC;
		if (flags & FLAG_DUP) opts |= XIO_DUP;
		else if (flags & FLAG_NOD) opts |= XIO_NOD;
		xhandle = xioopen(name, opts, keylen, reclen, &filepos, work);
	}
	else if (type == DAVB_AFILE) {
		if (flags & FLAG_EXC) opts = AIO_M_EXC | AIO_P_TXT;
		else if (flags & FLAG_RDO) opts = AIO_M_SRO | AIO_P_TXT;
		else opts = AIO_M_SHR | AIO_P_TXT;
		if ((flags & (FLAG_VAR & FLAG_CMP)) == 0) opts |= AIO_FIX | AIO_UNC;
		xhandle = aioopen(name, opts, reclen, &filepos, work, &fixflg, NULL, 0, 0);
	}
	/* reset memory pointers */
	setpgmdata();

	if (type != DAVB_FILE) {  /* attempted an index or aim open */
		if (xhandle < 0) {
			if (xhandle == ERR_FNOTF) i1 = 601;
			else if (xhandle == ERR_NOACC) i1 = 602;
			else if (xhandle == ERR_BADNM) i1 = 603;
			else if (xhandle == ERR_BADKL) i1 = 606;
			else if (xhandle == ERR_IXDUP) i1 = 656;
			else i1 = 1630 - xhandle;
			if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(i1, (UCHAR *) name, (INT) strlen(name));
			dbcerror(i1);
		}

		strcpy(name, work);
		if (type == DAVB_AFILE) {
			if (matchc) aiomatch(xhandle, matchc);
			if (fixflg) flags |= FLAG_AFX;
		}
	}

	i1 = 0;
	if (flags & FLAG_EXC) {
		if (type == DAVB_FILE) {
			opts = RIO_M_EXC | RIO_P_TXT;
			if (!(dioflags & DIOFLAG_NOSTATIC)) i1 = bufs;
		}
		else opts = RIO_M_MXC | RIO_P_TXT;
	}
	else if (flags & FLAG_RDO) opts = RIO_M_SRO | RIO_P_TXT;
	else opts = RIO_M_SHR | RIO_P_TXT;
	mask = flags & TYPE_MSK;
	if (mask == TYPE_TXT) opts |= RIO_T_TXT;
	else if (mask == TYPE_DAT) opts |= RIO_T_DAT;
	else if (mask == TYPE_DOS) opts |= RIO_T_DOS;
	else if (mask == TYPE_BIN) opts |= RIO_T_BIN;
	else opts |= RIO_T_STD;
	if (!(flags & FLAG_VAR)) opts |= RIO_FIX;
	if (!(flags & FLAG_CMP)) opts |= RIO_UNC;
	handle = rioopen(name, opts, i1, reclen);

	/* reset memory pointers */
	setpgmdata();
	if (handle < 0) {
		if (type == DAVB_IFILE) xioclose(xhandle);  /* close index */
		else if (type == DAVB_AFILE) aioclose(xhandle);  /* close aimdex */
		if (handle == ERR_FNOTF) i1 = 601;
		else if (handle == ERR_NOACC) i1 = 602;
		else if (handle == ERR_BADNM) i1 = 603;
		else if (handle == ERR_BADTP) i1 = 604;
		else i1 = 1630 - handle;
		if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(i1, (UCHAR *) name, (INT) strlen(name));
		dbcerror(i1);
	}

vopenok:
	file = *filetabptr + refnum;
	file->type = type;
	file->flags = flags;
	file->handle = handle;
	file->xhandle = xhandle;
	file->reclen = reclen;
	file->module = mod;
	file->offset = off;
	file->aimrec = -1;
	if (dbcflags & DBCFLAG_DEBUG) savenameinfileinfo(refnum);
	(aligndavb(findvar(mod, off), NULL))->refnum = refnum + 1;
}

void vprep()
{
	INT i1, handle, xhandle, fixflg, matchc, keylen, opts, reclen, refnum;
	INT bufs, caseflg = FALSE, createflg, flags, keyoff = 0, mask, mod, off, type;
	CHAR filename[MAX_NAMESIZE], filename2[MAX_NAMESIZE], work[2 * 4096];
	UCHAR *adr;
	DAVB *davb;
	FILEINFO *file;

	davb = getdavb();
	getlastvar(&mod, &off);
	if (davb->refnum) closedavb(davb, FALSE);

	cvtoname(getvar(VAR_READ));
	strcpy(filename, name);

	type = davb->type;
	flags = davb->info.file.flags | FLAG_EXC;
	bufs = davb->info.file.bufs;
	createflg = 1;
	filename2[0] = '\0';
	if (vbcode != 0x32) {  /* complex prep */
		/* vbcode = N, where code was 0x58,N */
		if (type == DAVB_FILE && pgm[pcount] == 0xFF) {  /* file mode */
			createflg = pgm[pcount + 1];
			pcount += 2;
		}
		else {
			cvtoname(getvar(VAR_READ));
			if (type != DAVB_AFILE || vbcode != 0x1E) strcpy(filename2, name);
			if (type == DAVB_AFILE) {
				keyoff = 0;
				if (vbcode != 0x1E) cvtoname(getvar(VAR_READ));
				if (vbcode == 0x21) {
					matchc = name[0];
					cvtoname(getvar(VAR_READ));
					if (toupper(name[0]) == 'S') caseflg = TRUE;
					cvtoname(getvar(VAR_READ));
				}
				else if (toupper(name[0]) == 'S') {
					caseflg = TRUE;
					keyoff = 1;
				}
				else if (toupper(name[0]) == 'U') keyoff = 1;
			}
		}
		vbcode = 0x32;  /* set last code to be a prep */
	}

	/* get record length */
	if (flags & FLAG_VRS) {
		adr = findvar(mod, davb->info.file.recsz);
		if (adr != NULL) reclen = nvtoi(adr);
		else reclen = 0;
		if (reclen < 1) {
				if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(610, (UCHAR *) filename, (INT) strlen(filename));
				dbcerror(610);
		}
	}
	else reclen = davb->info.file.recsz;
	if (reclen > RIO_MAX_RECSIZE) {
		if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(610, (UCHAR *) filename, (INT) strlen(filename));
		dbcerror(610);
	}

	/* get key length or index number */
	keylen = 0;
	if (type == DAVB_IFILE) {
		if (flags & FLAG_KLN) {
			if (flags & FLAG_VKY) {
				adr = findvar(mod, davb->info.file.keysz);
				if (adr != NULL) keylen = nvtoi(adr);
				else keylen = 0;
				if (keylen < 1) {
					if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(609, (UCHAR *) filename, (INT) strlen(filename));
					dbcerror(609);
				}
			}
			else keylen = davb->info.file.keysz;
		}
	}

	/* get reference number */
	refnum = getfileinfo(reclen);

	/* prep native file */
	xhandle = 0;
	if (flags & FLAG_NAT) {
#if OS_WIN32
		if (!nioinitflag) loadnio();
#endif
		if (type == DAVB_AFILE) {
			if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(651, (UCHAR *) filename, (INT) strlen(filename));
			dbcerror(651);
		}
		handle = nioprep(filename, filename2, keylen);
		/* reset memory pointers */
		setpgmdata();
		if (handle < 0) {
			nativerror = INT_MIN;
			strcpy(name, filename);
			if (handle == -2) i1 = 601;
			else if (handle == -3) i1 = 602;
			else if (handle == -99) i1 = 1630 - nioerror();
			else {
				if (handle == -98) nativerror = nioerror();
				i1 = 651;
			}
			if (dioflags & DIOFLAG_DSPERR) {
				if (nativerror == INT_MIN) dbcerrinfo(i1, (UCHAR *) name, -1);
				else {
					sprintf(work, "file name=%s, NIO error=%d", name, nativerror);
					dbcerrinfo(i1, (UCHAR *) work, -1);
				}
			}
			dbcerror(i1);
		}
		goto vprepok;
	}

	i1 = srvgetserver(filename, FIO_OPT_OPENPATH);
	if (i1) {
		if (i1 == -1) {
			if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(657, (UCHAR *) filename, (INT) strlen(filename));
			dbcerror(657);
		}
		if (i1 != srvgetserver(filename, FIO_OPT_PREPPATH)) {
			if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(657, (UCHAR *) filename, (INT) strlen(filename));
			dbcerror(657);
		}
		xhandle = i1;
		miofixname(filename, "", 0);  /* need compat conversions */
		if (*filename2) miofixname(filename2, "", 0);  /* need compat conversions */
		opts = 0;
		mask = flags & TYPE_MSK;
		if (mask == TYPE_TXT) opts |= FS_TEXT;
		else if (mask == TYPE_DAT) opts |= FS_DATA;
		else if (mask == TYPE_DOS) opts |= FS_CRLF;
		else if (mask == TYPE_BIN) opts |= FS_BINARY;
		if (flags & FLAG_VAR) opts |= FS_VARIABLE;
		if (flags & FLAG_CMP) opts |= FS_COMPRESSED;
		if (type == DAVB_IFILE) {
			if (flags & FLAG_DUP) opts |= FS_DUP;
			else if (flags & FLAG_NOD) opts |= FS_NODUP;
			handle = fsprepisi(xhandle, filename2, filename, opts, reclen, keylen);
		}
		else if (type == DAVB_AFILE) {
			if (caseflg) opts |= FS_CASESENSITIVE;
			handle = fsprepaim(xhandle, filename2, filename, opts, reclen, name + keyoff, (char) matchc);
		}
		else {
			if (createflg == 2) opts |= FS_CREATEONLY;
			handle = fsprep(xhandle, filename, opts, reclen);
		}
		if (handle < 0) {
			if (handle < -650 && handle >= -699 && handle != -656) i1 = 1000 - handle;
			else if (handle < -750 && handle >= -799) i1 = 1000 - handle;
			else if (handle < -1000 && handle > -1049) i1 = 1650 - handle;
			else if (handle == -1) {
				i1 = 658;
				fsgeterror(filename, sizeof(filename));
			}
			else if (handle > -100) {  /* should not happen, but just in case */
				if (handle == ERR_NOACC) i1 = 602;
				else if (handle == ERR_BADNM || handle == ERR_FNOTF) i1 = 603;
				else if (handle == ERR_EXIST) i1 = 605;
				else if (handle == ERR_BADKL) i1 = 606;
				else i1 = 1630 - handle;
			}
			else i1 = -handle;
			if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(i1, (UCHAR *) filename, (INT) strlen(filename));
			dbcerror(i1);
		}
		flags |= FLAG_SRV;
		goto vprepok;
	}

	if (type == DAVB_IFILE) {  /* index */
		opts = XIO_M_PRP | XIO_P_TXT;
		if (!(flags & (FLAG_VAR | FLAG_CMP))) opts |= XIO_FIX | XIO_UNC;
		if (flags & FLAG_DUP) opts |= XIO_DUP;
		else if (flags & FLAG_NOD) opts |= XIO_NOD;
		xhandle = xioopen(filename, opts, keylen, reclen, &filepos, filename2);
	}
	else if (type == DAVB_AFILE) {  /* aimdex */
		opts = AIO_M_PRP | AIO_P_TXT;
		if (!(flags & (FLAG_VAR | FLAG_CMP))) opts |= AIO_FIX | AIO_UNC;
		xhandle = aioopen(filename, opts, reclen, &filepos, filename2, &fixflg, &name[keyoff], caseflg, matchc);
	}

	if (type != DAVB_FILE) {  /* attempted an index or aim prep */
		/* reset memory pointers */
		setpgmdata();

		if (xhandle < 0) {
			if (xhandle == ERR_NOACC) i1 = 602;
			else if (xhandle == ERR_BADNM || xhandle == ERR_FNOTF) i1 = 603;
			else if (xhandle == ERR_EXIST) i1 = 605;
			else if (xhandle == ERR_BADKL) i1 = 606;
			else i1 = 1630 - xhandle;
			if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(i1, (UCHAR *) filename, (INT) strlen(filename));
			dbcerror(i1);
		}

		strcpy(name, filename2);
		if (type == DAVB_AFILE && fixflg) flags |= FLAG_AFX;
	}

	i1 = 0;
	if (type == DAVB_FILE) {
		opts = RIO_M_PRP | RIO_P_TXT;
		if (createflg == 2) opts = RIO_M_EFC | RIO_P_TXT;
		if (!(dioflags & DIOFLAG_NOSTATIC)) i1 = bufs;
	}
	else opts = RIO_M_MTC | RIO_P_TXT;
	mask = flags & TYPE_MSK;
	if (mask == TYPE_TXT) opts |= RIO_T_TXT;
	else if (mask == TYPE_DAT) opts |= RIO_T_DAT;
	else if (mask == TYPE_DOS) opts |= RIO_T_DOS;
	else if (mask == TYPE_BIN) opts |= RIO_T_BIN;
	else opts |= RIO_T_STD;
	if (!(flags & FLAG_VAR)) opts |= RIO_FIX;
	if (!(flags & FLAG_CMP)) opts |= RIO_UNC;
	handle = rioopen(name, opts, i1, reclen);

	/* reset memory pointers */
	setpgmdata();

	if (handle < 0) {
		if (type == DAVB_IFILE) xioclose(xhandle);  /* close index */
		else if (type == DAVB_AFILE) aioclose(xhandle);  /* close aimdex */
		if (handle == ERR_NOACC) i1 = 602;
		else if (handle == ERR_BADNM || handle == ERR_FNOTF) i1 = 603;
		else if (handle == ERR_EXIST) i1 = 605;
		else i1 = 1630 - handle;
		if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(i1, (UCHAR *) name, (INT) strlen(name));
		dbcerror(i1);
	}

	preprefnum = refnum + 1;

vprepok:
	file = *filetabptr + refnum;
	file->type = type;
	file->flags = flags;
	file->handle = handle;
	file->xhandle = xhandle;
	file->reclen = reclen;
	file->module = mod;
	file->offset = off;
	file->aimrec = -1;
	if (dbcflags & DBCFLAG_DEBUG) savenameinfileinfo(refnum);
	(aligndavb(findvar(mod, off), NULL))->refnum = refnum + 1;
}

void vclos()
{
	INT delflg;
	DAVB *davb;

	davb = getdavb();
	if (!davb->refnum) return;

	if (((*filetabptr + davb->refnum - 1)->flags & FLAG_EXC) &&
	   (vbcode == 0x58 || (lsvbcode == 0x32 && davb->refnum == preprefnum))) delflg = TRUE;
	else delflg = FALSE;
	closedavb(davb, delflg);
}

/*
 * read, readks, readkp, readkg, readkgp and tabbed versions thereof
 */
void vread(INT readflag)
{
	CHAR **keybuf, **pptr;
	INT keybuflen, keybufsize;
	INT i3;
	INT i1, i2, errnum, flags, partflg, readflg, recflags, refnum;
	INT lockflg, type;
	OFFSET eofpos, pos;
	CHAR *ptr;
	UCHAR c, clrflg, errflg, flckflg, parmflg, rlckflg, key[MAXKEYSIZE + 1], *adr;
	DAVB *davb;
	FILEINFO *file;

	lockflg = readflag & 0x01;
	recflags = 0;
	clrflg = TRUE;
	errflg = parmflg = 0;
	flckflg = FALSE;
	dbcflags &= ~DBCFLAG_OVER;
	partflg = 0;
	readflg = -1;
	errnum = 0;
	davb = getdavb();
	type = davb->type;
	if (!davb->refnum) {
		file = NULL;
		errnum = 701;
		goto vreadclr;
	}
	refnum = davb->refnum - 1;
	file = *filetabptr + refnum;
	flags = file->flags;
	file->flags &= ~FLAG_UPD;
	if (flags & FLAG_ALK) lockflg |= 0x02;
	if (lockflg) dbcflags &= ~DBCFLAG_LESS;

	if (flags & FLAG_NAT) {  /* native file (use nio routines) */
		if (vbcode >= 0x36) {  /* readks and readkp */
			if (vbcode >= 0x38) recsize_f = niordks(file->handle, record, file->reclen);
			else recsize_f = niordkp(file->handle, record, file->reclen);
		}
		else {
			adr = getvar(VAR_READ);
			parmflg = 2;
			if (vartype & TYPE_NUMERIC) {  /* numeric variable */
				pos = nvtooff(adr);
				if (pos >= 0L) recsize_f = niordrec(file->handle, pos, record, file->reclen);
				else recsize_f = niordseq(file->handle, record, file->reclen);
			}
			else {  /* keyed */
				if (compat == 2 || compat == 3) clrflg = FALSE;
				if (fp) {
					i1 = lp - --fp;
					if (i1 > (INT) (sizeof(key) - 1)) i1 = sizeof(key) - 1;
					memcpy(key, &adr[hl + fp], (UINT) i1);
				}
				else i1 = 0;
				key[i1] = 0;
				recsize_f = niordkey(file->handle, (CHAR *) key, record, file->reclen);
			}
		}
		/* reset memory pointers */
		setpgmdata();
		file = *filetabptr + refnum;

		nativerror = INT_MIN;
		if (recsize_f < 0) {
			if (recsize_f != -2) {
				if (recsize_f == -99) {
					i1 = nioerror();
					if (i1 == ERR_NOTOP) i1 = 701;
					else if (i1 == ERR_RANGE) i1 = 707;
					else if (i1 == ERR_SHORT) i1 = 715;
					else if (i1 == ERR_NOEOR) i1 = 716;
					else if (i1 == ERR_BADKY) i1 = 718;
					else i1 = 1730 - i1;
				}
				else {
					if (recsize_f == -98) nativerror = nioerror();
					i1 = 1730 - ERR_RDERR;	/* 1692 */
				}
				errnum = i1;
			}
			goto vreadclr;
		}
		goto vreadok;
	}

	if (flags & FLAG_SRV) {  /* file server file (use fsfileio routines) */
		if (vbcode >= 0x36) {  /* readks and readkp */
			if (vbcode >= 0x38) {  /* readks */
				if (lockflg & 0x01) recsize_f = fsreadkeynextlock(file->handle, (CHAR *) record, file->reclen);
				else recsize_f = fsreadkeynext(file->handle, (CHAR *) record, file->reclen);
			}
			else {  /* readkp */
				if (lockflg & 0x01) recsize_f = fsreadkeyprevlock(file->handle, (CHAR *) record, file->reclen);
				else recsize_f = fsreadkeyprev(file->handle, (CHAR *) record, file->reclen);
			}
		}
		else if (vbcode >= 0x34) {  /* read <var>/<list of vars> */
			if (type != DAVB_AFILE) {
				adr = getvar(VAR_READ);
				parmflg = 2;
			}
			else {
				adr = getlist(LIST_READ | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1);
				parmflg = 1;
			}
			if (vartype & TYPE_NUMERIC) {  /* numeric variable */
				pos = nvtooff(adr);
				if (pos >= 0) {  /* random */
/*** CODE: MAY WANT TO MOVE THIS TEST TO FSFILEIO.C ***/
					if (pos > 0 && (flags & FLAG_VAR)) {
						errnum = 703;
						goto vreadclr;
					}
					if (lockflg & 0x01) recsize_f = fsreadrandomlock(file->handle, pos, (CHAR *) record, file->reclen);
					else recsize_f = fsreadrandom(file->handle, pos, (CHAR *) record, file->reclen);
				}
				else if (pos == -3) {  /* seq at file end */
					fssetposateof(file->handle);
					goto vreadclr;
				}
				else if (pos == -4L) {  /* read previous */
					if (lockflg & 0x01) recsize_f = fsreadprevlock(file->handle, (CHAR *) record, file->reclen);
					else recsize_f = fsreadprev(file->handle, (CHAR *) record, file->reclen);
				}
				else if (file->partflg == 1 || (file->partflg == 2 && (compat == 2 || compat == 3))) {  /* sequential after partial i/o */
					partflg = 1;
					if (lockflg & 0x01) recsize_f = fsreadsamelock(file->handle, (CHAR *) record, file->reclen);
					else recsize_f = fsreadsame(file->handle, (CHAR *) record, file->reclen);
				}
				else {  /* sequential */
					if (lockflg & 0x01) recsize_f = fsreadnextlock(file->handle, (CHAR *) record, file->reclen);
					else recsize_f = fsreadnext(file->handle, (CHAR *) record, file->reclen);
				}
			}
			else if (type == DAVB_IFILE) {  /* indexed read */
				if (fp) {
					lp -= --fp;
					if (lp >= (INT) sizeof(key)) lp = sizeof(key) - 1;
					memcpy(key, &adr[hl + fp], (UINT) lp);
					key[lp] = 0;
				}
/*** CODE: MAYBE JUST LET FSFILEIO.C DO THIS TEST ***/
				else if (!(flags & FLAG_UPD)) {  /* null key read */
					errnum = 718;
					goto vreadclr;
				}
				else key[0] = 0;
				if (lockflg & 0x01) recsize_f = fsreadkeylock(file->handle, (CHAR *) key, (CHAR *) record, file->reclen);
				else recsize_f = fsreadkey(file->handle, (CHAR *) key, (CHAR *) record, file->reclen);
			}
			else {  /* aim read */
				keybufsize = 0;
				for (i1 = 0; ; ) {
					if (vartype & TYPE_NUMERIC) {
						errnum = 712;
						goto vreadclr;
					}
					if (fp) {
						i2 = lp - --fp;
						if (i2 >= (INT) sizeof(key)) i2 = sizeof(key) - 1;
						if (++i1 == 1) {
							memcpy(key, &adr[hl + fp], (UINT) i2);
							keybuflen = i2;
							key[keybuflen++] = 0;
						}
						else {
							i3 = keybuflen + i2 + 1 + (sizeof(CHAR *) * i1);
							if (i3 > keybufsize) {
								for (keybufsize = 1024; keybufsize < i3; keybufsize <<= 1);
								if (i1 == 2) {
									keybuf = (CHAR **) memalloc(keybufsize, 0);
									if (keybuf != NULL) memcpy(*keybuf, key, (UINT) keybuflen);
									else i3 = -1;
								}
								else i3 = memchange((UCHAR **) keybuf, keybufsize, 0);
								/* reset memory pointers */
								setpgmdata();
								file = *filetabptr + refnum;
								if (i3 == -1) {
									if (keybuf != NULL) memfree((UCHAR **) keybuf);
									errnum = 1730 - ERR_NOMEM;
									goto vreadclr;
								}
							}
							memcpy(*keybuf + keybuflen, &adr[hl + fp], (UINT) i2);
							keybuflen += i2;
							(*keybuf)[keybuflen++] = 0;
						}
					}
					adr = getlist(LIST_READ | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1);
					if (adr == NULL) {
						pcount++;  /* skip second 0xFF */
						parmflg = 2;
						break;
					}
				}
				if (!i1) {  /* null key read */
/*** CODE: MAYBE JUST LET FSFILEIO.C DO THIS TEST ***/
					if (!(flags & FLAG_UPD)) {
						errnum = 718;
						goto vreadclr;
					}
					if (lockflg & 0x01) recsize_f = fsreadsamelock(file->handle, (CHAR *) record, file->reclen);
					else recsize_f = fsreadsame(file->handle, (CHAR *) record, file->reclen);
				}
				else if (i1 == 1) {  /* single key read */
					file->flags &= ~FLAG_INV;
					if (lockflg & 0x01) recsize_f = fsreadaimkeylock(file->handle, (CHAR *) key, (CHAR *) record, file->reclen);
					else recsize_f = fsreadaimkey(file->handle, (CHAR *) key, (CHAR *) record, file->reclen);
				}
				else {
					file->flags &= ~FLAG_INV;
					pptr = (CHAR **)(*keybuf + keybufsize - i1 * sizeof(CHAR *));
					for (i2 = i3 = 0; i2 < i1; i2++) {
						pptr[i2] = *keybuf + i3;
						i3 += (INT)strlen(*keybuf + i3) + 1;
					}
					if (lockflg & 0x01) recsize_f = fsreadaimkeyslock(file->handle, pptr, i1, (CHAR *) record, file->reclen);
					else recsize_f = fsreadaimkeys(file->handle, pptr, i1, (CHAR *) record, file->reclen);
					memfree((UCHAR **) keybuf);
				}
			}
		}
		else {  /* readkg or readgp */
			if (flags & FLAG_INV) {
				errnum = 714;
				goto vreadclr;
			}
			if (vbcode > 0x22) {
				if (lockflg & 0x01) recsize_f = fsreadaimnextlock(file->handle, (CHAR *) record, file->reclen);
				else recsize_f = fsreadaimnext(file->handle, (CHAR *) record, file->reclen);
			}
			else {
				if (lockflg & 0x01) recsize_f = fsreadaimprevlock(file->handle, (CHAR *) record, file->reclen);
				else recsize_f = fsreadaimprev(file->handle, (CHAR *) record, file->reclen);
			}
		}
		if (recsize_f < 0) {
			if (recsize_f == -2) dbcflags |= DBCFLAG_LESS;
			else if (recsize_f != -3) {
				if (recsize_f == -759) errnum = 715;
				else if (recsize_f == -758) errnum = 716;
				else if (recsize_f == -761) errnum = 301;
				else if (recsize_f == -787) errnum = 1730 - ERR_NOEOR;
				else if (recsize_f < -750 && recsize_f >= -799) errnum = 1000 - recsize_f;
				else if (recsize_f < -1000 && recsize_f >= -1049) {
					// 1767 result if recsize_f == -17
					errnum = 1750 - recsize_f;
				}
				else if (recsize_f == -1) errnum = 781;
				else errnum = -recsize_f;
			}
			goto vreadclr;
		}
		goto vreadok;
	}

	if (lockflg && (flags & FLAG_SLK)) riounlock(file->handle, (OFFSET) -1);
	errflg = 2;
	if (vbcode >= 0x36) {  /* readks and readkp */
		if (vbcode >= 0x38) i1 = -1;  /* readks */
		else i1 = -2;  /* readkp */
		goto vreadisi;
	}
	if (vbcode >= 0x34) {  /* read <var>/<list of vars> */
		if (type != DAVB_AFILE) {
			adr = getvar(VAR_READ);
			parmflg = 2;
		}
		else {
			adr = getlist(LIST_READ | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1);
			parmflg = 1;
		}
		if (vartype & TYPE_NUMERIC) {  /* numeric variable */
			errflg = 1;
			pos = nvtooff(adr);
			if (pos >= 0L) {  /* random */
				if (pos > 0L && (flags & FLAG_VAR)) {
					errnum = 703;
					goto vreadclr;
				}
				pos *= file->reclen + rioeorsize(file->handle);
				readflg = 0;
				goto vreadrec;
			}
			if (pos == -3L) {  /* seq at file end */
				rioeofpos(file->handle, &pos);
				riosetpos(file->handle, pos);
				goto vreadclr;
			}
			if (pos == -4L) {  /* read previous */
				readflg = 2;
				goto vreadseq;  /* sequential backwards */
			}
			readflg = 1;
			if (file->partflg == 1 || (file->partflg == 2 && (compat == 2 || compat == 3))) {  /* sequential after partial read */
				partflg = 1;
				riolastpos(file->handle, &pos);
				goto vreadrec;
			}
			if (file->partflg == 2) rioget(file->handle, record, file->reclen);  /* sequential, but read EOR if last access was a partial write or update */
			goto vreadseq;  /* sequential */
		}
		if (type == DAVB_IFILE) {  /* indexed read */
			if (fp) {
				i1 = lp - --fp;
				if (i1 > (INT) sizeof(key)) i1 = sizeof(key);
				memcpy(key, &adr[hl + fp], (UINT) i1);
				goto vreadisi;
			}
			/* null key read */
			if (!(flags & FLAG_UPD) || xiofind(file->xhandle, NULL, 0) != 1) {
				errnum = 718;
				goto vreadclr;
			}
			riolastpos(file->handle, &pos);
			goto vreadrec;
		}
		/* aim read */
		for (i1 = FALSE; ; ) {
			if (vartype & TYPE_NUMERIC) {
				errnum = 712;
				goto vreadclr;
			}
			if (fp) {
				file->flags &= ~FLAG_INV;
				if (!i1) {
					file->aimrec = -1L;
					i2 = aionew(file->xhandle);
					if (i2) {
						errnum = 1730 - i2;
						goto vreadclr;
					}
					i1 = TRUE;
				}
				i2 = lp - --fp;
				if (i2 > (INT) sizeof(key)) i2 = sizeof(key);
				memcpy(key, &adr[hl + fp], (UINT) i2);
				i2 = aiokey(file->xhandle, key, i2);
				/* reset memory pointers */
				setpgmdata();
				file = *filetabptr + refnum;
				if (i2) {
					if (i2 == ERR_BADKY) errnum = 712;
					else errnum = 1730 - i2;
					goto vreadclr;
				}
			}
			adr = getlist(LIST_READ | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1);
			if (adr == NULL) {
				pcount++;  /* skip second 0xFF */
				parmflg = 2;
				break;
			}
		}
		if (!i1) {  /* null key read */
			if (!(flags & FLAG_UPD)) {
				errnum = 718;
				goto vreadclr;
			}
			riolastpos(file->handle, &pos);
			goto vreadrec;
		}
	}
	else {  /* readkg or readgp */
		if (flags & FLAG_INV) {
			errnum = 714;
			goto vreadclr;
		}
		if (file->aimrec != -1) {
			riosetpos(file->handle, file->aimpos);
			rioget(file->handle, record, file->reclen);
		}
	}

vreadaim:  /* aim read, readkg or readgp */
	/* read an aim record and see if it matches the criteria */
	if (file->aimrec == -1) {
		if (vbcode > 0x22) i1 = AIONEXT_NEXT;
		else i1 = AIONEXT_PREV;
		i1 = aionext(file->xhandle, i1);
		/* reset memory pointers */
		setpgmdata();
		file = *filetabptr + refnum;
	}
	else i1 = 1;
	if (i1 == 1) {  /* a possible record exists */
		if (!(flags & FLAG_AFX)) {  /* variable length records */
			if (file->aimrec == -1) {  /* set position */
				pos = filepos << 8;
				if (vbcode < 0x23) {
					pos += 256;
					rioeofpos(file->handle, &eofpos);
					if (pos > eofpos) pos = eofpos;
				}
				riosetpos(file->handle, pos);
				if (pos) rioget(file->handle, record, file->reclen);
				if (vbcode < 0x23) {
					rionextpos(file->handle, &pos);
					riosetpos(file->handle, pos);
				}
				file->aimrec = filepos;
			}
			for ( ; ; ) {
				if (vbcode > 0x22) recsize_f = rioget(file->handle, record, file->reclen);
				else recsize_f = rioprev(file->handle, record, file->reclen);
				if (recsize_f < 0) {
					if (recsize_f != -2) {
						file->aimrec = -1;
						if (recsize_f == -1) goto vreadaim;
						goto vreaderr;
					}
				}
				riolastpos(file->handle, &pos);
				file->aimpos = pos;
				if (pos) pos = (pos - 1) / 256;
				if (pos != file->aimrec) {
					file->aimrec = -1;
					goto vreadaim;
				}
				if (recsize_f == -2) continue;
				if (aiochkrec(file->xhandle, record, recsize_f)) continue;
				if (!lockflg) goto vreadok;

				/* must lock the record */
				riosetpos(file->handle, file->aimpos);
				recsize_f = riolock(file->handle, flags & FLAG_TLK);
				if (recsize_f != 0) {
					if (recsize_f == 1) {
						dbcflags |= DBCFLAG_LESS;
						goto vreadclr;
					}
					goto vreaderr;
				}
				recsize_f = rioget(file->handle, record, file->reclen);
				if (recsize_f >= 0 && !aiochkrec(file->xhandle, record, recsize_f)) goto vreadok;
				riounlock(file->handle, file->aimpos);
				if (recsize_f < 0) {
					if (recsize_f != -2) {
						file->aimrec = -1;
						if (recsize_f == -1) goto vreadclr;
						goto vreaderr;
					}
				}
			}
		}
		else {  /* fixed length records */
			pos = filepos * (file->reclen + rioeorsize(file->handle));
			riosetpos(file->handle, pos);
			recsize_f = rioget(file->handle, record, file->reclen);
			if (recsize_f < 0) {
				if (recsize_f == -1) goto vreadclr;
				if (recsize_f == -2) goto vreadaim;
				goto vreaderr;
			}
			if (aiochkrec(file->xhandle, record, recsize_f)) goto vreadaim;
			if (!lockflg) goto vreadok;

			/* must lock the record */
			riosetpos(file->handle, pos);
			recsize_f = riolock(file->handle, flags & FLAG_TLK);
			if (recsize_f != 0) {
				if (recsize_f == 1) {
					dbcflags |= DBCFLAG_LESS;
					goto vreadclr;
				}
				goto vreaderr;
			}
			recsize_f = rioget(file->handle, record, file->reclen);
			if (recsize_f >= 0 && !aiochkrec(file->xhandle, record, recsize_f)) goto vreadok;
			riounlock(file->handle, pos);
			if (recsize_f < 0) {
				if (recsize_f == -1) goto vreadclr;
				if (recsize_f <= -3) goto vreaderr;
			}
			goto vreadaim;
		}
	}
	else if (i1 == 3) errnum = 713;
	else if (i1 < 0) {
		if (i1 == ERR_BADKY) errnum = 712;
		else if (i1 == ERR_RANGE) errnum = 714;
		else errnum = 1730 - i1;
	}
	goto vreadclr;

vreadisi:
	if (compat == 2 || compat == 3) clrflg = FALSE;
	i2 = fioflck(file->handle);
	if (i2) {
		errnum = 1730 - i2;
		errflg = 1;
		goto vreadclr;
	}
	if (i1 == -1) i1 = xionext(file->xhandle);
	else if (i1 == -2) i1 = xioprev(file->xhandle);
	else i1 = xiofind(file->xhandle, key, i1);
	/* reset memory pointers */
	setpgmdata();
	file = *filetabptr + refnum;
	if (i1 != 1 || lockflg) fiofulk(file->handle);
	else flckflg = TRUE;
	if (i1 != 1) {
		if (i1 < 0) {
			if (i1 == ERR_BADKY) errnum = 721;
			else errnum = 1730 - i1;
		}
		goto vreadclr;
	}
	pos = filepos;

vreadrec:  /* read record at pos */
	riosetpos(file->handle, pos);

vreadseq:  /* read file sequentially */
	do {
		rlckflg = FALSE;
		if (readflg == 2) recsize_f = rioprev(file->handle, record, file->reclen);
		if (lockflg && (readflg != 2 || recsize_f >= 0)) {
			if (readflg == 2) {
				riolastpos(file->handle, &pos);
				riosetpos(file->handle, pos);
			}
			i1 = riolock(file->handle, flags & FLAG_TLK);
			if (i1 == 1) {
				dbcflags |= DBCFLAG_LESS;
				goto vreadclr;
			}
			if (i1 != 0) {
				recsize_f = i1;
				goto vreaderr;
			}
			rlckflg = TRUE;
		}
		if (readflg != 2 || (lockflg && recsize_f >= 0)) recsize_f = rioget(file->handle, record, file->reclen);
		if (recsize_f >= 0) goto vreadok;
		if (rlckflg) {
			riolastpos(file->handle, &pos);
			riounlock(file->handle, pos);
		}
	} while (recsize_f == -2 && readflg > 0);
	if (recsize_f == -2 || (recsize_f == -1 && readflg == -1)) {
		errnum = 720;
		errflg = 1;
	}

vreaderr:  /* possible error during read */
	if (recsize_f <= -3) {
		if (recsize_f == ERR_SHORT) errnum = 715;
		else if (recsize_f == ERR_NOEOR) errnum = 716;
		else if (recsize_f == ERR_RANGE) errnum = 301;
		else if (recsize_f == ERR_NOEOF) errnum = 1730 - ERR_NOEOR;
		else {
			// 1767 result if recsize_f == -37, which is ERR_NOMEM
			errnum = 1730 - recsize_f;
		}
		errflg = 1;
	}

vreadclr:  /* set over and clear list */
	if (!parmflg && (vbcode == 0x34 || vbcode == 0x35)) {  /* read with variable */
		if (type != DAVB_AFILE) getvar(VAR_READ);
		else parmflg = 1;
	}
	if (!lockflg || !(dbcflags & DBCFLAG_LESS)) dbcflags |= DBCFLAG_OVER;
	recsize_f = 0;

vreadok:  /* record was read, fill variables and return */
	if (flckflg) fiofulk(file->handle);
	if (parmflg == 1) {  /* finish aim key list */
		getlist(LIST_PROG | LIST_END | LIST_NUM1);
		pcount++;  /* skip second 0xFF */
	}
	if (partflg) recpos_f = file->partpos;
	else recpos_f = 0;
	if (flags & FLAG_CBL) recflags |= RECFLAG_COBOL;
	for ( ; ; ) {
		while ((adr = getlist(LIST_WRITE | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1)) != NULL)
			if ((clrflg || !(dbcflags & DBCFLAG_OVER)) && (i1 = recgetv(adr, recflags, record, (INT *)&recpos_f, recsize_f)) && !errnum) errnum = i1;
		c = pgm[pcount - 1];
		if (c == 0xFA) {
			c = getbyte();
			if (c == 0x7F) {
				recpos_f = (INT) gethhll();
				goto vread1;
			}
		}
		else if (c == 0xFC || c == 0xFD) {
			if (c == 0xFC) recpos_f = getbyte();
			else recpos_f = nvtoi(getvar(VAR_READ));
vread1:
			if (errnum) recpos_f = 1;
			else if (recpos_f < 1 || recpos_f > (UINT) file->reclen) {
				errnum = 704;
				recpos_f = 1;
			}
			recpos_f--;
		}
		else break;
	}

	if (file != NULL) file->partflg = 0;
	if (!(dbcflags & DBCFLAG_OVER) && (!lockflg || !(dbcflags & DBCFLAG_LESS))) {
		file->flags |= FLAG_UPD;
		file->lastlen = recsize_f;
		if (c == 0xFE) {
			file->partflg = 1;
			file->partpos = recpos_f;
		}
	}

	if (errnum) {
		if (dioflags & DIOFLAG_DSPERR) {
			if (flags & FLAG_NAT) {
				/*
				 * This code block was doing NOTHING!
				 * Fixed on 06 DEC 2017
				 * DX 20.0 (prerelease)
				 */
				if (nativerror != INT_MIN) {
					sprintf(name, "NIO error = %d", nativerror);
					dbcerrinfo(errnum, (UCHAR *) name, (INT) strlen(name));
				}
			}
			else if (flags & FLAG_SRV) {  /* file server file */
				fsgeterror(name, sizeof(name));
				if (name[0]) dbcerrinfo(errnum, (UCHAR *) name, (INT) strlen(name));
			}
			else if (errflg) {
				if (errflg == 1 || errnum == 201) i1 = file->handle;
				else i1 = file->xhandle;
				ptr = fioname(i1);
				if (ptr != NULL) {
					strcpy(name, ptr);
					dbcerrinfo(errnum, (UCHAR *) name, (INT) strlen(name));
				}
			}
		}
		dbcerror(errnum);
	}
}

/*
 * write, update and tabbed versions thereof
 * OpCodes 0x50	UPDATE
 *         0x51 UPDATAB
 *         0x53 WRITE
 *         0x54 WRITETAB
 */
void vwrite()
{
#define VWRITE_SEQUENTIAL	0x0001
#define VWRITE_RANDOM		0x0002
#define VWRITE_ENDOFFILE		0x0004
#define VWRITE_INDEX		0x0008
#define VWRITE_AIMDEX		0x0010
#define VWRITE_UPDATE		0x0020
#define VWRITE_TAB			0x0040
#define VWRITE_PART_READ		0x0080
#define VWRITE_PART_WRITE	0x0100
#define VWRITE_PREREAD		0x0200
#define VWRITE_PARTIAL		0x0400
#define VWRITE_RETRY		0x0800
#define VWRITE_LOCK			0x1000
	INT i1, errnum, flags, listflags, preread, psave, saverecsize;
	INT recflags, refnum, keylen, truncflag, writeflags;
	UINT reclen;
	OFFSET pos;
	CHAR *ptr;
	UCHAR c1, errflag, key[MAXKEYSIZE + 1], *adr, *fmtadr;
	DAVB *davb;
	FILEINFO *file;

	recflags = 0;
	errflag = 0;
	recpos_f = 0;
	errnum = 0;
	fmtadr = NULL;
	writeflags = 0;

	davb = getdavb();
	if (!davb->refnum)
	{
		if (vbcode >= 0x53) getvar(VAR_READ); /* If it's a write of some kind, and not an update */
		file = NULL;
		reclen = 0;
		errnum = 701;
		goto vwrite3;
	}
	refnum = davb->refnum - 1;
	file = *filetabptr + refnum;
	flags = file->flags;
	file->flags &= ~FLAG_UPD;
	reclen = file->reclen;
	if (!(flags & FLAG_UPD)) file->partflg = 0;
	if (vbcode <= 0x51) {  /* update and updatab */
		if (vbcode == 0x51) writeflags |= VWRITE_TAB;  /* updatab */
		writeflags |= VWRITE_UPDATE;
		if (!(flags & FLAG_UPD)) {
			errnum = 710;
			goto vwrite3;
		}
		if (flags & FLAG_CMP) {
			errnum = 711;
			goto vwrite3;
		}
	}
	else {  /* write and writab */
		if (vbcode == 0x54) writeflags |= VWRITE_TAB;  /* writab */
		adr = getvar(VAR_READ);
		if (file->type == DAVB_AFILE && adr == NULL) writeflags = VWRITE_AIMDEX | VWRITE_ENDOFFILE;  /* aim write */
		else {  /* not an aim write */
			if (vartype & TYPE_NUMERIC) {  /* numeric write */
				pos = nvtooff(adr);
				if (pos >= 0) {  /* random write */
					writeflags |= VWRITE_RANDOM;
					if (flags & FLAG_VAR) {
						errnum = 703;
						goto vwrite3;
					}
				}
				else if (pos == -3) writeflags |= VWRITE_ENDOFFILE;  /* eof */
				else writeflags |= VWRITE_SEQUENTIAL;  /* sequential */
			}
			else {  /* index write */
				writeflags = VWRITE_INDEX | VWRITE_ENDOFFILE;
				if (fp) {
					keylen = lp - --fp;
					if (keylen >= (INT) sizeof(key)) keylen = sizeof(key) - 1;
					memcpy(key, &adr[hl + fp], (UINT) keylen);
				}
				else keylen = 0;
				key[keylen] = '\0';
			}
		}
	}

	if (flags & FLAG_NAT) {  /* native update or write */
		psave = pcount;
		if (writeflags & VWRITE_SEQUENTIAL) {  /* sequential write and writab */
			if (file->partflg == 2 || (file->partflg == 1 && (compat == 2 || compat == 3)))
				writeflags |= VWRITE_UPDATE | VWRITE_PART_READ;
		}

		if ((writeflags & (VWRITE_TAB | VWRITE_PART_READ))) {  /* read the record first to handle tabbing/partial */
vwrite1:
			key[0] = '\0';
			preread = niordkey(file->handle, (CHAR *) key, record, (INT) reclen);
			/* reset memory pointers */
			setpgmdata();
			file = *filetabptr + refnum;
			nativerror = INT_MIN;
			if (preread < 0) {
				if (preread == -2) i1 = 718;
				else if (preread == -99) {
					i1 = nioerror();
					if (i1 == ERR_NOTOP) i1 = 701;
					else if (i1 == ERR_BADKY) i1 = 710;
					else i1 = 1730 - i1;
				}
				else {
					if (preread == -98) nativerror = nioerror();
					i1 = 710;
				}
				errnum = i1;
			}
			else writeflags |= VWRITE_PREREAD;
			if (!errnum) {
				if (writeflags & VWRITE_PREREAD) {
					if (writeflags & VWRITE_PART_READ) recpos_f = file->partpos;
				}
				else if (writeflags & (VWRITE_PART_READ | VWRITE_UPDATE)) errnum = 720;
				else if (writeflags & VWRITE_TAB) memset(record, ' ', reclen);
			}
		}
		goto vwrite3;
	}

	if (flags & FLAG_SRV) {  /* file server file (use fsfileio routines) */
		psave = pcount;
		if (writeflags & VWRITE_SEQUENTIAL) {  /* sequential write and writab */
			if (file->partflg == 2 || (file->partflg == 1 && (compat == 2 || compat == 3)))
				writeflags |= VWRITE_UPDATE | VWRITE_PART_READ;
		}

		if (writeflags & (VWRITE_TAB | VWRITE_PART_READ)) {  /* read the record first to handle tabbing/partial */
vwrite2:
			if (!(writeflags & VWRITE_ENDOFFILE)) {
				i1 = fsfilelock(file->handle);
				if (i1 < 0) {
					if (i1 < -750 && i1 >= -799) errnum = 1000 - i1;
					else if (i1 < -1000 && i1 >= -1049) errnum = 1750 - i1;
					else if (i1 == -1) errnum = 781;
					else errnum = -i1;
					goto vwrite3;
				}
				writeflags |= VWRITE_LOCK;
				if (writeflags & VWRITE_UPDATE) preread = fsreadsame(file->handle, (CHAR *) record, (INT) reclen);
				else if (writeflags & VWRITE_RANDOM) preread = fsreadrandom(file->handle, pos, (CHAR *) record, (INT) reclen);
				else preread = fsreadsame(file->handle, (CHAR *) record, (INT) reclen);
				if (preread < 0) {
					if (preread != -3 && preread != -301 && preread != -761) {  /* ignore range */
						if (preread == -759) errnum = 715;
						else if (preread == -758) errnum = 716;
						else if (preread == -768 || preread == -1768) errnum = 706;
						else if (preread == -787) errnum = 1730 - ERR_NOEOR;
						else if (preread < -750 && preread >= -799) errnum = 1000 - preread;
						else if (preread < -1000 && preread >= -1049) errnum = 1750 - preread;
						else if (preread == -1) errnum = 781;
						else errnum = -preread;
					}
				}
				else if (preread && (flags & FLAG_CMP)) errnum = 711;
				else writeflags |= VWRITE_PREREAD | VWRITE_UPDATE;
			}
			if (!errnum) {
				if (writeflags & VWRITE_PREREAD) {
					if (writeflags & VWRITE_PART_READ) recpos_f = file->partpos;
				}
				else if (writeflags & (VWRITE_PART_READ | VWRITE_UPDATE)) errnum = 720;
				else if (flags & FLAG_VAR) errnum = 792;
				else if (writeflags & VWRITE_TAB) memset(record, ' ', reclen);
			}
		}
		goto vwrite3;
	}

	/* non-native/file server */
	errflag = 1;  /* assume error will occur on text file */
	if (writeflags & VWRITE_UPDATE) riolastpos(file->handle, &pos);  /* update and updatab */
	else if (writeflags & VWRITE_RANDOM) pos *= reclen + rioeorsize(file->handle);  /* random write and writab */
	else if (writeflags & VWRITE_SEQUENTIAL) {  /* sequential write and writab */
		if (file->partflg == 2 || (file->partflg == 1 && (compat == 2 || compat == 3))) {
			if (file->partflg == 1) {
				writeflags |= VWRITE_PART_READ;
				riolastpos(file->handle, &pos);
			}
			else if (writeflags & VWRITE_TAB) {
				writeflags |= VWRITE_PART_READ;
				pos = file->lastpos;
			}
			else {
				writeflags |= VWRITE_PART_WRITE;
				rionextpos(file->handle, &pos);
			}
		}
		else rionextpos(file->handle, &pos);
	}

	if (writeflags & (VWRITE_TAB | VWRITE_PART_READ)) {  /* read the record first to handle tabbing/partial */
		if (!(writeflags & VWRITE_ENDOFFILE)) {
			riosetpos(file->handle, pos);
			preread = rioget(file->handle, record, (INT) reclen);
			if (preread < 0) {
				if (preread <= -3 && preread != ERR_RANGE) {
					if (preread == ERR_SHORT) errnum = 715;
					else if (preread == ERR_NOEOR) errnum = 716;
					else if (preread == ERR_RDERR) errnum = 706;
					else if (preread == ERR_NOEOF) errnum = 1730 - ERR_NOEOR;
					else errnum = 1730 - preread;
				}
			}
			else if (preread && (flags & FLAG_CMP)) errnum = 711;
			else writeflags |= VWRITE_PREREAD;
		}
		if (!errnum) {
			if (writeflags & VWRITE_PREREAD) {
				if (writeflags & VWRITE_PART_READ) recpos_f = file->partpos;
			}
			else if (writeflags & VWRITE_PART_READ) errnum = 720;
			else memset(record, ' ', reclen);  /* tabbed */
		}
	}

vwrite3:
	if (file != NULL) file->partflg = 0;
	recsize_f = 0;

	/* get the list of variables */
	listflags = LIST_READ | LIST_PROG | LIST_NUM1;
	if (flags & FLAG_CBL) recflags |= RECFLAG_COBOL;
	truncflag = FALSE;
	for ( ; ; ) {
		if ((adr = getlist(listflags)) != NULL) {  /* it's a variable */
			if (vartype & (TYPE_LIST | TYPE_ARRAY)) {  /* list or array variable */
				listflags = (listflags & ~LIST_PROG) | LIST_LIST | LIST_ARRAY;
				continue;
			}
			truncflag |= recputv(adr, recflags, record, (UINT *)&recpos_f, (UINT *)&recsize_f, reclen, fmtadr);
			if ((listflags & LIST_PROG) && !(vartype & TYPE_LITERAL)) recflags &= ~(RECFLAG_ZERO_FILL | RECFLAG_ZERO_SUPPRESS | RECFLAG_MINUS_OVERPUNCH | RECFLAG_FORMAT);
			continue;
		}
		if (!(listflags & LIST_PROG)) {
			listflags = (listflags & ~(LIST_LIST | LIST_ARRAY)) | LIST_PROG;
			recflags &= ~(RECFLAG_ZERO_FILL | RECFLAG_ZERO_SUPPRESS | RECFLAG_MINUS_OVERPUNCH | RECFLAG_FORMAT);
			continue;
		}

		c1 = pgm[pcount - 1];
		if (c1 == 0xD2) recflags |= RECFLAG_ZERO_FILL;
		else if (c1 == 0xF1) {
			recflags |= RECFLAG_LOGICAL_LENGTH;
			recflags &= ~RECFLAG_BLANK_SUPPRESS;
		}
		else if (c1 == 0xF2) recflags &= ~(RECFLAG_LOGICAL_LENGTH | RECFLAG_BLANK_SUPPRESS);
		else if (c1 == 0xF3) recflags |= RECFLAG_MINUS_OVERPUNCH;
		else if (c1 == 0xFA) {
			c1 = getbyte();
			if (c1 == 0x31) {
				fmtadr = getlist(listflags);
				recflags |= RECFLAG_FORMAT;
			}
			else if (c1 == 0x37) {
				recflags |= RECFLAG_BLANK_SUPPRESS;
				recflags &= ~RECFLAG_LOGICAL_LENGTH;
			}
			else if (c1 == 0x38) recflags |= RECFLAG_ZERO_SUPPRESS;
			else if (c1 == 0x7F) {
				recpos_f = (INT) gethhll();
				goto vwrite4;
			}
		}
		else if (c1 == 0xFB) {
			c1 = getbyte();
			if (recpos_f < reclen) {
				record[recpos_f++] = c1;
				if ((INT) recpos_f > recsize_f) recsize_f = recpos_f;
			}
		}
		else if (c1 == 0xFC || c1 == 0xFD) {
			if (c1 == 0xFC) recpos_f = getbyte();
			else recpos_f = nvtoi(getvar(VAR_READ));
vwrite4:
			if (errnum) recpos_f = 1;
			else if (recpos_f < 1 || recpos_f > reclen) {
				errnum = 704;
				recpos_f = 1;
			}
			recpos_f--;
		}
		else {
			if (c1 == 0xFE || ((writeflags & VWRITE_TAB) && compat == 3)) writeflags |= VWRITE_PARTIAL;  /* also convert writab/updatab to partial when compat=dosx */
			break;
		}
	}
	if (errnum) goto vwrite5;

	saverecsize = recsize_f;
	if (truncflag) {
		if (!(dioflags & DIOFLAG_WTTRUNC)) {
			errnum = 716;
			goto vwrite5;
		}
	}
	if (writeflags & VWRITE_PART_WRITE) {
		if (recsize_f > (INT) (reclen - file->partpos)) {
			if (!(dioflags & DIOFLAG_WTTRUNC)) {
				errnum = 716;
				goto vwrite5;
			}
			saverecsize = recsize_f = reclen - file->partpos;
		}
	}
	else if (recsize_f > (INT) reclen) {
		if (!(dioflags & DIOFLAG_WTTRUNC)) {
			errnum = 716;
			goto vwrite5;
		}
		saverecsize = recsize_f = reclen;
	}
	if (writeflags & VWRITE_PREREAD) {
/*** CODE: ADDED ALL CASES, CHANGE BACK TO JUST UPDATE IF COMPLAINTS ***/
/***		if (recsize > preread && !(writeflags & (VWRITE_SEQUENTIAL | VWRITE_RANDOM)) && !(dioflags & DIOFLAG_WTTRUNC)) { ***/
		if (recsize_f > preread && !(dioflags & DIOFLAG_WTTRUNC)) {
			errnum = 716;
			goto vwrite5;
		}
		if (recsize_f < preread && !(writeflags & (VWRITE_TAB | VWRITE_PARTIAL)) && ((writeflags & (VWRITE_SEQUENTIAL | VWRITE_RANDOM)) || !(dioflags & DIOFLAG_OLDUPDT)))
			memset(&record[recsize_f], ' ', (UINT) (preread - recsize_f));
		recsize_f = preread;
		if (saverecsize > preread) saverecsize = preread;
	}
	else if (recsize_f < (INT) reclen && !(flags & FLAG_VAR) && !(writeflags & VWRITE_TAB) && (!(writeflags & VWRITE_PARTIAL) || (flags & (FLAG_NAT | FLAG_SRV))))
		memset(&record[recsize_f], ' ', reclen - recsize_f);

	if (flags & FLAG_NAT) {  /* native file (use nio routines) */
		if (recsize_f < (INT) reclen && !(writeflags & VWRITE_PREREAD)) {
			if (!(writeflags & (VWRITE_TAB | VWRITE_RETRY))) {  /* have not attempted to read the record */
				if ((writeflags & VWRITE_UPDATE) && ((writeflags & VWRITE_PARTIAL) || (dioflags & DIOFLAG_OLDUPDT) || (flags & FLAG_VAR))) {
					writeflags |= VWRITE_RETRY;
					recpos_f = 0;
					pcount = psave;
					goto vwrite1;
				}
			}
			if (!(flags & FLAG_VAR)) recsize_f = reclen;
		}
		if (writeflags & VWRITE_PARTIAL) {  /* partial write */
			file->partflg = 2;
			file->partpos = saverecsize;
		}

		if (writeflags & VWRITE_UPDATE) i1 = nioupd(file->handle, record, recsize_f);
		else if (writeflags & VWRITE_INDEX) i1 = niowtkey(file->handle, record, recsize_f);
		else if (writeflags & VWRITE_RANDOM) i1 = niowtrec(file->handle, pos, record, recsize_f);
		else i1 = niowtseq(file->handle, record, recsize_f);
		/* reset memory pointers */
		setpgmdata();
		file = *filetabptr + refnum;
		nativerror = INT_MIN;
		if (i1 < 0) {
			if (i1 == -2) i1 = 709;
			else if (i1 == -99) {
				i1 = nioerror();
				if (i1 == ERR_NOTOP) i1 = 701;
				else if (i1 == ERR_BADKY) i1 = 710;
				else i1 = 1730 - i1;
			}
			else {
				//
				// Fixed a bug in the next statement. i1 was recsize_f
				// 06 DEC 2017
				// DX 20.0 (prerelease)
				//
				if (i1 == -98) nativerror = nioerror();
				i1 = 706;
			}
			errnum = i1;
		}
		if (!errnum) file->flags |= FLAG_UPD;
	}
	else if (flags & FLAG_SRV) {  /* file server file (use fsfileio routines) */
		if (recsize_f < (INT) reclen && !(writeflags & VWRITE_PREREAD)) {
			if (!(writeflags & (VWRITE_TAB | VWRITE_RETRY))) {  /* have not attempted to read the record */
				if ((writeflags & VWRITE_PARTIAL) || ((writeflags & VWRITE_UPDATE) && ((dioflags & DIOFLAG_OLDUPDT) || (flags & FLAG_VAR)))) {
					writeflags |= VWRITE_RETRY;
					recpos_f = 0;
					pcount = psave;
					goto vwrite2;
				}
			}
			if (!(flags & FLAG_VAR)) recsize_f = reclen;
		}
		if (writeflags & VWRITE_PARTIAL) {  /* partial write */
			file->partflg = 2;
			file->partpos = saverecsize;
		}

		if (writeflags & VWRITE_UPDATE) i1 = fsupdate(file->handle, (CHAR *) record, recsize_f);
		else if (writeflags & VWRITE_INDEX) i1 = fswritekey(file->handle, (CHAR *) key, (CHAR *) record, recsize_f);
		else if (writeflags & VWRITE_AIMDEX) i1 = fswriteaim(file->handle, (CHAR *) record, recsize_f);
		else if (writeflags & VWRITE_ENDOFFILE) i1 = fswriteateof(file->handle, (CHAR *) record, recsize_f);
		else if (writeflags & VWRITE_RANDOM) i1 = fswriterandom(file->handle, pos, (CHAR *) record, recsize_f);
		else i1 = fswritenext(file->handle, (CHAR *) record, recsize_f);
		if (i1 < 0) {
			if (i1 < -750 && i1 >= -799) errnum = 1000 - i1;
			else if (i1 < -1000 && i1 >= -1049) errnum = 1750 - i1;
			else if (i1 == -1) errnum = 781;
			else errnum = -i1;
		}
		else file->flags |= FLAG_UPD;
	}
	else {  /* DB/C and text type file */
		if (writeflags & VWRITE_ENDOFFILE) {
			i1 = fioflck(file->handle);
			if (i1) {
				errnum = 1730 - i1;
				goto vwrite5;
			}
			writeflags |= VWRITE_LOCK;

			/* get position to write record */
			if (writeflags & VWRITE_INDEX) {  /* index */
				if (!(flags & FLAG_VAR)) {
					i1 = xiogetrec(file->xhandle);
					/* reset memory pointers */
					setpgmdata();
					file = *filetabptr + refnum;
					if (!i1) {
						writeflags &= ~VWRITE_ENDOFFILE;
						pos = filepos;
					}
				}
			}
			else if (writeflags & VWRITE_AIMDEX) {  /* aimdex */
				if (!(flags & FLAG_VAR) && (flags & FLAG_AFX)) {
					i1 = aiogetrec(file->xhandle);
					/* reset memory pointers */
					setpgmdata();
					file = *filetabptr + refnum;
					if (!i1) {
						writeflags &= ~VWRITE_ENDOFFILE;
						pos = filepos * (reclen + rioeorsize(file->handle));
					}
				}
			}
			if (writeflags & VWRITE_ENDOFFILE) rioeofpos(file->handle, &pos);

			if (writeflags & VWRITE_INDEX) {  /* indexed write */
				if (keylen) {
					filepos = pos;
					i1 = xioinsert(file->xhandle, key, keylen);
					/* reset memory pointers */
					setpgmdata();
					file = *filetabptr + refnum;
					if (i1) {
						errflag = 2;
						if (i1 < 0) {
							if (i1 == ERR_BADKY) errnum = 721;
							else errnum = 1730 - i1;
						}
						else if (i1 == 1) errnum = 709;
						else if (i1 == 2) errnum = 719;
					}
				}
				else {
					errflag = 2;
					errnum = 708;
				}
			}
			else if (writeflags & VWRITE_AIMDEX) {  /* aimdex write */
				if (!(dioflags & DIOFLAG_AIMINSERT)) file->flags |= FLAG_INV;
				filepos = pos;
				if (flags & FLAG_AFX) filepos /= reclen + rioeorsize(file->handle);
				else if (filepos) filepos = (filepos - 1) / 256;
				i1 = aioinsert(file->xhandle, record);
				/* reset memory pointers */
				setpgmdata();
				file = *filetabptr + refnum;
				if (i1) {
					if (i1 == ERR_WRERR) errnum = 706;
					else errnum = 1730 - i1;
					errflag = 2;
				}
			}
			if (errnum) goto vwrite5;
		}

		riosetpos(file->handle, pos);
		if (!(writeflags & VWRITE_PARTIAL) && (!(writeflags & VWRITE_UPDATE) || !(dioflags & DIOFLAG_OLDUPDT))) {  /* non-partial write */
			if (writeflags & VWRITE_UPDATE) {
				if (recsize_f > file->lastlen && !(dioflags & DIOFLAG_WTTRUNC)) {
					errnum = 716;
					goto vwrite5;
				}
				recsize_f = file->lastlen;
			}
			else if (!(flags & FLAG_VAR)) {
				recsize_f = reclen;
				if (writeflags & VWRITE_PART_WRITE) recsize_f -= file->partpos;
			}
			i1 = rioput(file->handle, record, recsize_f);
		}
		else {  /* partial write or update with updtfill = old */
/*** CODE: FOLLOWING CODE SHOULD NOT BE DONE IF PARTIAL WRITE.  BUT ***/
/***       DO NEED A WAY TO SIGNAL THAT AN RIOGET MUST BE PERFORMED ***/
/***       BEFORE A -1 READ/WRITE (SAME BUG EXISTS IN DB/C 9.1) ***/
			file->partflg = 2;
			if (!(writeflags & VWRITE_PART_WRITE)) {
				file->partpos = saverecsize;
				file->lastpos = pos;
			}
			else file->partpos += saverecsize;
			i1 = rioparput(file->handle, record, recsize_f);
		}
		if (!i1) {
			file->flags |= FLAG_UPD;
/*** CODE: THIS IS NOT TECHNICALLY CORRECT FOR VWRITE_PART_WRITE ***/
			if (!(writeflags & VWRITE_UPDATE)) file->lastlen = recsize_f;
		}
		else {
			if (i1 == ERR_WRERR) errnum = 706;
			else errnum = 1730 - i1;
		}
		if (flags & FLAG_ALK) riounlock(file->handle, pos);
	}

vwrite5:
	if (writeflags & VWRITE_LOCK) {
		if (flags & FLAG_NAT);
#ifndef DX_SINGLEUSER
		else if (flags & FLAG_SRV) fsfileunlock(file->handle);
#endif
		else fiofulk(file->handle);
	}
	if (errnum > 0 && errnum <= 1799) {
		if (dioflags & DIOFLAG_DSPERR) {
			if (flags & FLAG_NAT) {
				/*
				 * This code block was doing NOTHING!
				 * Fixed on 06 DEC 2017
				 * DX 20.0 (prerelease)
				 */
				if (nativerror != INT_MIN) {
					sprintf(name, "NIO error = %d", nativerror);
					dbcerrinfo(errnum, (UCHAR *) name, -1);
				}
			}
#ifndef DX_SINGLEUSER
			else if (flags & FLAG_SRV) {  /* file server file */
				fsgeterror(name, sizeof(name));
				if (name[0]) dbcerrinfo(errnum, (UCHAR *) name, (INT) strlen(name));
			}
#endif
			else if (errflag) {
				if (errflag == 1) i1 = file->handle;
				else i1 = file->xhandle;
				ptr = fioname(i1);
				if (ptr != NULL) {
					strcpy(name, ptr);
					dbcerrinfo(errnum, (UCHAR *) name, (INT) strlen(name));
				}
			}
		}
		dbcerror(errnum);
	}
}

void vdiotwo()  /* two address i/o verbs */
{
	INT i1, errnum, keylen = 0, refnum;
	OFFSET pos, eofpos;
	CHAR *ptr;
	UCHAR errflg, lock, work[32], key[MAXKEYSIZE + 1], *adr, work2[128];
	DAVB *davb;
	FILEINFO *file;

	errflg = 0;
	errnum = 0;

	davb = getdavb();
	if (!davb->refnum) {
		skipadr();
		dbcerror(701);
	}
	refnum = davb->refnum - 1;
	file = *filetabptr + refnum;

	if (vbcode != 0x1D) file->partflg = 0;
	adr = getvar(VAR_READ);
	if (adr != NULL && (vartype & TYPE_CHARACTER)) {
		if (fp) {
			keylen = lp - --fp;
			if (keylen >= (INT) sizeof(key)) keylen = sizeof(key) - 1;
			memcpy(key, &adr[hl + fp], (UINT) keylen);
		}
		else keylen = 0;
		key[keylen] = 0;
	}
	dbcflags &= ~DBCFLAG_OVER;
	lock = FALSE;

	if (file->flags & FLAG_NAT) {
		nativerror = INT_MIN;
		if (vbcode == VERB_DELETE) {  /* delete */
			file->flags &= ~FLAG_UPD;
			key[keylen] = 0;
			i1 = niodel(file->handle, (CHAR *) key);
			/* reset memory pointers */
			setpgmdata();
			if (i1 < 0) {
				if (i1 == -2) dbcflags |= DBCFLAG_OVER;
				else if (i1 == -3) dbcerror(717);
				else {
					if (i1 == -99) {
						i1 = nioerror();
						if (i1 == ERR_NOTOP) i1 = 701;
						else if (i1 == ERR_BADKY) i1 = 710;
						else i1 = 1730 - i1;
					}
					else {
						if (i1 == -98) nativerror = nioerror();
						i1 = 1730 - ERR_BADIX;
					}
					if (nativerror != INT_MIN) {
						sprintf((CHAR*)work2, "NIO error=%d", nativerror);
						dbcerrinfo(i1, work2, -1);
					}
					dbcerror(i1);
				}
			}
			return;
		}
#if OS_WIN32
		if (vbcode == 0x1D && (nioinitflag & NIO_FPOSIT)) {  /* fposit */
#else
		if (vbcode == 0x1D) {  /* fposit */
#endif
			i1 = niofpos(file->handle, &pos);
			if (i1 < 0) {
				if (i1 == -99) {
					i1 = nioerror();
					if (i1 == ERR_NOTOP) i1 = 701;
					else i1 = 1730 - i1;
				}
				else {
					if (i1 == -98) nativerror = nioerror();
					i1 = 1730 - ERR_OTHER;
				}
				if (nativerror != INT_MIN) {
					sprintf((CHAR*)work2, "NIO error=%d", nativerror);
					dbcerrinfo(i1, work2, -1);
				}
				dbcerror(i1);
			}
			/* make a form variable */
			mscofftoa(pos, (CHAR *) &work[2]);
			work[0] = 0xE0;
			work[1] = (UCHAR) strlen((CHAR *) &work[2]);
			i1 = dbcflags & (DBCFLAG_EQUAL | DBCFLAG_LESS);
			movevar(work, adr);
			dbcflags = (dbcflags & ~(DBCFLAG_EQUAL | DBCFLAG_LESS)) | i1;
			return;
		}
#if OS_WIN32
		if (vbcode == 0x3C && (nioinitflag & NIO_REPOSIT)) {  /* reposit */
#else
		if (vbcode == 0x3C) {  /* reposit */
#endif
			dbcflags &= ~(DBCFLAG_OVER | DBCFLAG_EQUAL);
			i1 = niorpos(file->handle, nvtooff(adr));
			if (i1 < 0) {
				if (i1 == -1) dbcflags |= DBCFLAG_OVER;
				else if (i1 == -2) dbcflags |= DBCFLAG_EQUAL;  /* past end of file */
				else {
					if (i1 == -99) {
						i1 = nioerror();
						if (i1 == ERR_NOTOP) i1 = 701;
						else i1 = 1730 - i1;
					}
					else {
						if (i1 == -98) nativerror = nioerror();
						i1 = 1730 - ERR_OTHER;		/* 1661 */
					}
					if (nativerror != INT_MIN) {
						sprintf((CHAR*)work2, "NIO error=%d", nativerror);
						dbcerrinfo(i1, work2, -1);
					}
					dbcerror(i1);
				}
			}
		}
		return;
	}

#ifndef DX_SINGLEUSER
	if (file->flags & FLAG_SRV) {  /* file server file (use fsfileio routines) */
		if (vbcode == VERB_INSERT) {  /* insert */
			if (file->type == DAVB_IFILE) i1 = fsinsertkey(file->handle, (CHAR *) key);
			else {  /* afile insert */
				if (recsize_f < (INT) file->reclen) memset(&record[recsize_f], ' ', (UINT) (file->reclen - recsize_f));
				i1 = fsinsertkeys(file->handle, (CHAR *) record, recsize_f);
			}
			if (i1 < 0) {
				if (i1 < -750 && i1 >= -799) errnum = 1000 - i1;
				else if (i1 < -1000 && i1 >= -1049) errnum = 1750 - i1;
				else if (i1 == -1) errnum = 781;
				else errnum = -i1;
				if (dioflags & DIOFLAG_DSPERR) {
					fsgeterror(name, sizeof(name));
					if (name[0]) dbcerrinfo(errnum, (UCHAR *) name, (INT) strlen(name));
				}
				dbcerror(errnum);
			}
		}
		else if (vbcode == VERB_DELETE) {  /* delete */
			if (file->type == DAVB_IFILE) i1 = fsdeletekey(file->handle, (CHAR *) key);
			else {  /* afile delete */
/*** CODE: SHOULD THIS BE AN ERROR ***/
				if (!(file->flags & FLAG_UPD)) return;
				i1 = fsdelete(file->handle);
			}
			if (i1) {
				if (i1 == 1) dbcflags |= DBCFLAG_OVER;
				else if (i1 < 0) {
					if (i1 < -750 && i1 >= -799) errnum = 1000 - i1;
					/*
					 * e.g.
					 * ERR_ALREADYCONNECTED is -1001
					 * ERR_BADCONNECTID is -1010
					 * ERR_BADFILEID is 1013
					 */
					else if (i1 < -1000 && i1 >= -1049) errnum = 1750 - i1;
					else if (i1 == -1) errnum = 781;
					else errnum = -i1;
					if (dioflags & DIOFLAG_DSPERR) {
						fsgeterror(name, sizeof(name));
						if (name[0]) dbcerrinfo(errnum, (UCHAR *) name, (INT) strlen(name));
					}
					dbcerror(errnum);
				}
			}
		}
		else if (vbcode == 0x13) {  /* deletek */
			i1 = fsdeletekeyonly(file->handle, (CHAR *) key);
			if (i1) {
				if (i1 == 1) dbcflags |= DBCFLAG_OVER;
				else if (i1 < 0) {
					if (i1 < -750 && i1 >= -799) errnum = 1000 - i1;
					else if (i1 < -1000 && i1 >= -1049) errnum = 1750 - i1;
					else if (i1 == -1) errnum = 781;
					else errnum = -i1;
					if (dioflags & DIOFLAG_DSPERR) {
						fsgeterror(name, sizeof(name));
						if (name[0]) dbcerrinfo(errnum, (UCHAR *) name, (INT) strlen(name));
					}
					dbcerror(errnum);
				}
			}
		}
		else if (vbcode == 0x1D) {  /* fposit */
			i1 = fsfposit(file->handle, &pos);
			if (i1 < 0) {
				if (i1 < -750 && i1 >= -799) errnum = 1000 - i1;
				else if (i1 < -1000 && i1 >= -1049) errnum = 1750 - i1;
				else if (i1 == -1) errnum = 781;
				else errnum = -i1;
				if (dioflags & DIOFLAG_DSPERR) {
					fsgeterror(name, sizeof(name));
					if (name[0]) dbcerrinfo(errnum, (UCHAR *) name, (INT) strlen(name));
				}
				dbcerror(errnum);
			}
			/* make a form variable */
			mscofftoa(pos, (CHAR *) &work[2]);
			work[0] = 0xE0;
			work[1] = (UCHAR) strlen((CHAR *) &work[2]);
			i1 = dbcflags & (DBCFLAG_EQUAL | DBCFLAG_LESS);
			movevar(work, adr);
			dbcflags = (dbcflags & ~(DBCFLAG_EQUAL | DBCFLAG_LESS)) | i1;
		}
		else {
			pos = nvtooff(adr);
			if (vbcode == 0x3C) {  /* reposit */
				dbcflags &= ~(DBCFLAG_OVER | DBCFLAG_EQUAL);
				i1 = fsreposit(file->handle, pos);
				if (i1) {
					if (i1 == 1) dbcflags |= DBCFLAG_OVER;
					else if (i1 == 2) dbcflags |= DBCFLAG_EQUAL;  /* past end of file */
					else if (i1 < 0) {
						if (i1 < -750 && i1 >= -799) errnum = 1000 - i1;
						else if (i1 < -1000 && i1 >= -1049) errnum = 1750 - i1;
						else if (i1 == -1) errnum = 781;
						else errnum = -i1;
						if (dioflags & DIOFLAG_DSPERR) {
							fsgeterror(name, sizeof(name));
							if (name[0]) dbcerrinfo(errnum, (UCHAR *) name, (INT) strlen(name));
						}
						dbcerror(errnum);
					}
				}
			}
			else if (vbcode == 0x52) {  /* weof */
				if (pos == -3) return;  /* meaningless */
				i1 = fsweof(file->handle, pos);
				if (i1 < 0) {
					if (i1 < -750 && i1 >= -799) errnum = 1000 - i1;
					else if (i1 < -1000 && i1 >= -1049) errnum = 1750 - i1;
					else if (i1 == -1) errnum = 781;
					else errnum = -i1;
					if (dioflags & DIOFLAG_DSPERR) {
						fsgeterror(name, sizeof(name));
						if (name[0]) dbcerrinfo(errnum, (UCHAR *) name, (INT) strlen(name));
					}
					dbcerror(errnum);
				}
			}
		}
		return;
	}
#endif

	errflg = 1;  /* assume error will occur with text file */
	if (vbcode == VERB_INSERT || vbcode == VERB_DELETE || vbcode == 0x13) {
		i1 = fioflck(file->handle);
		if (i1) goto vtwoerrx;
		lock = TRUE;
	}
	if (vbcode == VERB_INSERT) {  /* insert */
		errflg = 2;  /* assume error will occur with index file */
		riogrplpos(file->handle, &filepos);
		if (file->type == DAVB_IFILE) {  /* ifile insert */
			i1 = xioinsert(file->xhandle, key, keylen);
			/* reset memory pointers */
			setpgmdata();
			file = *filetabptr + refnum;
			if (i1) {
				if (i1 < 0) {
					if (i1 == ERR_BADKY) errnum = 721;
					else {
						errnum = 1730 - i1;
					}
				}
				else if (i1 == 1) errnum = 709;
				else if (i1 == 2) errnum = 719;
			}
		}
		else {  /* afile insert */
			if (!(dioflags & DIOFLAG_AIMINSERT)) file->flags |= FLAG_INV;
			if (recsize_f < (INT) file->reclen) memset(&record[recsize_f], ' ', (UINT) (file->reclen - recsize_f));
			if (file->flags & FLAG_AFX) filepos /= file->reclen + rioeorsize(file->handle);
			else if (filepos) filepos = (filepos - 1) / 256;
			i1 = aioinsert(file->xhandle, record);
			/* reset memory pointers */
			setpgmdata();
			file = *filetabptr + refnum;
			if (i1) {
				if (i1 == ERR_WRERR || i1 == ERR_OTHER) errnum = 706;
				else errnum = 1730 - i1;
			}
		}
		goto vtwoend;
	}
	if (vbcode == VERB_DELETE) {  /* delete */
		if (file->type == DAVB_IFILE) {  /* ifile delete */
			i1 = xiodelete(file->xhandle, key, keylen, FALSE);  /* also sets filepos */
			/* reset memory pointers */
			setpgmdata();
			file = *filetabptr + refnum;
			if (i1) {
				if (i1 == 1) goto vtwoover;
				errflg = 2;
				if (i1 < 0) {
					if (i1 == ERR_BADKY) errnum = 721;
					else errnum = 1730 - i1;
				}
				else errnum = 717;
				goto vtwoend;
			}
		}
		else {  /* afile delete */
			if (!(file->flags & FLAG_UPD)) goto vtwoend;
			riolastpos(file->handle, &filepos);
		}
		riosetpos(file->handle, filepos);
		i1 = riodelete(file->handle, file->reclen);
		riounlock(file->handle, filepos);
		if (i1) {
			if (i1 == ERR_ISDEL) goto vtwoend;
			goto vtwoerrx;
		}
		if (!(file->flags & FLAG_VAR) && file->type == DAVB_IFILE) {
			xiodelrec(file->xhandle);
			/* reset memory pointers */
			setpgmdata();
			file = *filetabptr + refnum;
		}
		else if (!(file->flags & FLAG_VAR)) {
			filepos /= file->reclen + rioeorsize(file->handle);
			aiodelrec(file->xhandle);
		}
		goto vtwoend;
	}
	if (vbcode == 0x13) {  /* deletek */
		i1 = xiodelete(file->xhandle, key, keylen, FALSE);
		/* reset memory pointers */
		setpgmdata();
		file = *filetabptr + refnum;
		if (i1) {
			if (i1 == 1) goto vtwoover;
			errflg = 2;
			if (i1 < 0) {
				if (i1 == ERR_BADKY) errnum = 721;
				else errnum = 1730 - i1;
			}
			else errnum = 717;
		}
		goto vtwoend;
	}
	if (vbcode == 0x1D) {  /* fposit */
		riolastpos(file->handle, &pos);
		/* make a form variable */
		mscofftoa(pos, (CHAR *) &work[2]);
		work[0] = 0xE0;
		work[1] = (UCHAR) strlen((CHAR *) &work[2]);
		i1 = dbcflags & (DBCFLAG_EQUAL | DBCFLAG_LESS);
		movevar(work, adr);
		dbcflags = (dbcflags & ~(DBCFLAG_EQUAL | DBCFLAG_LESS)) | i1;
		return;
	}
	pos = nvtooff(adr);
	if (vbcode == 0x3C) {  /* reposit */
		dbcflags &= ~(DBCFLAG_OVER | DBCFLAG_EQUAL);
		rioeofpos(file->handle, &eofpos);
		if (eofpos < pos) dbcflags |= DBCFLAG_EQUAL;  /* past end of file */
		else {
			if (eofpos == pos) dbcflags |= DBCFLAG_OVER;
			riosetpos(file->handle, pos);
		}
	}
	if (vbcode == 0x52) {  /* weof */
		if (pos == -3L) return;  /* meaningless */
		if (pos >= 0L) {
			pos *= file->reclen + rioeorsize(file->handle);
			riosetpos(file->handle, pos);
		}
		rioweof(file->handle);
	}
	return;
vtwoover:
	dbcflags |= DBCFLAG_OVER;
	goto vtwoend;

vtwoerrx:
	errnum = 1730 - i1;

vtwoend:
	if (lock) fiofulk(file->handle);
	if (errnum) {
		if (dioflags & DIOFLAG_DSPERR) {
			if (errflg == 1) i1 = file->handle;
			else i1 = file->xhandle;
			ptr = fioname(i1);
			if (ptr != NULL) {
				strcpy(name, ptr);
				dbcerrinfo(errnum, (UCHAR *) name, (INT) strlen(name));
			}
		}
		dbcerror(errnum);
	}
}

void vflush()  /* flush verb */
{
	INT i1;
	DAVB *davb;
	FILEINFO *file;

	davb = getdavb();
	if (!davb->refnum) dbcerror(701);
	file = *filetabptr + davb->refnum - 1;

	if (file->flags & FLAG_NAT) return;
	if (file->flags & FLAG_SRV) return;

	i1 = rioflush(file->handle);
	if (!i1) i1 = fioflush(file->handle);
	if (file->type == DAVB_IFILE || file->type == DAVB_AFILE) {
		if (file->type == DAVB_IFILE) {
			if (!i1) i1 = xioflush(file->xhandle);
		}
		if (!i1) i1 = fioflush(file->xhandle);
	}
	if (i1) dbcerror(1730 - i1);
}

void vfilepi()
{
#ifndef DX_SINGLEUSER
	INT i2;
#endif
	INT i1, cnt, errnum, ncnt, scnt, value, array[40], narray[40], sarray[40];
	CHAR *ptr, work2[128];
	DAVB *davb;
	FILEINFO *file;

	value = getbyte();

	errnum = 0;
	name[0] = '\0';
	for (cnt = ncnt = scnt = 0; pgm[pcount] != 0xFF; ) {
		davb = getdavb();
		if (!value) continue;
		if (davb->refnum) {
			file = *filetabptr + davb->refnum - 1;
			if (file->flags & FLAG_NAT) {
				if (ncnt < 39) narray[ncnt++] = file->handle;
			}
			else if (file->flags & FLAG_SRV) {
				if (scnt < 39) sarray[scnt++] = file->handle;
			}
			else if (cnt < 39) array[cnt++] = file->handle;
		}
		else errnum = 701;
	}
	pcount++;

	if (!value) {
		filepi0();
		return;
	}
	if (errnum) dbcerror(errnum);

	if (fpicnt) {  /* previous filepi active */
		filepi0();
		if (!(dioflags & DIOFLAG_RESETFPI)) dbcerror(504);
	}
	if (value == 1) return;
	if (ncnt) {
		nativerror = INT_MIN;
		narray[ncnt] = 0;
		i1 = niolck(narray);
		if (i1 < 0) {
			if (i1 == -99) i1 = nioerror();
			else {
				if (i1 == -98) nativerror = nioerror();
				i1 = ERR_LKERR;
			}
			if (nativerror != INT_MIN) {
				sprintf(work2, "NIO error=%d", nativerror);
				dbcerrinfo(1730 - i1, (UCHAR*)work2, -1);
			}
			dbcerror(1730 - i1);
		}
	}
#ifndef DX_SINGLEUSER
	if (scnt) {
/*** CODE: ADD CODE HERE TO SORT ARRAY (CAN'T SORT DIFFERENT SERVERS) ***/
		for (i2 = 0; i2 < scnt; i2++) {
			i1 = fsfilelock(sarray[i2]);
			if (i1 < 0) {
				if (ncnt) nioulck();
				if (i1 < -750 && i1 >= -799) i1 = 1000 - i1;
				else if (i1 < -1000 && i1 >= -1049) i1 = 1750 - i1;
				else if (i1 == -1) i1 = 781;
				else i1 = -i1;
				if (dioflags & DIOFLAG_DSPERR) {
					fsgeterror(name, sizeof(name));
					if (name[0]) dbcerrinfo(errnum, (UCHAR *) name, (INT) strlen(name));
				}
				dbcerror(i1);
			}
		}
	}
#endif
	if (cnt) {
		array[cnt] = 0;
		i1 = fiolock(array);
		if (i1) {
			if (ncnt) nioulck();
#ifndef DX_SINGLEUSER
			if (scnt) for (i2 = 0; i2 < scnt; i2++) fsfileunlock(sarray[i2]);
#endif
			if (dioflags & DIOFLAG_DSPERR) {
				ptr = fioname(array[0]);
				if (ptr != NULL) {
					strcpy(name, ptr);
					if (cnt > 1) strcat(name, ", ...");
					dbcerrinfo(1730 - i1, (UCHAR *) name, (INT) strlen(name));
				}
			}
			dbcerror(1730 - i1);
		}
	}
	fpicnt = value + 1;
	if (scnt) memcpy(filepisarray, sarray, scnt * sizeof(int));
	filepiscnt = scnt;
}

/* turn off the filepi locks */
void filepi0()
{
	fiounlock(-1);
#if OS_WIN32
	if (nioinitflag)
#endif
		nioulck();
#ifndef DX_SINGLEUSER
	if (filepiscnt) {
		for (int i1 = 0; i1 < filepiscnt; i1++) fsfileunlock(filepisarray[i1]);
		filepiscnt = 0;
	}
#endif
	fpicnt = 0;
}

/* unlock all records */
void vunlock(INT vx_p)
{
#ifdef DX_SINGLEUSER
	return;
#else
	OFFSET pos;
	DAVB *davb;
	FILEINFO *file;

	davb = getdavb();
	if (!davb->refnum) dbcerror(701);
	file = *filetabptr + davb->refnum - 1;
	if (vx_p == 0xE8) pos = nvtooff(getvar(VAR_READ));
	else pos = -1;
	if (file->flags & FLAG_NAT) return;
	if (file->flags & FLAG_SRV) {
		if (pos >= 0) fsunlock(file->handle, pos);
		else fsunlockall(file->handle);
		return;
	}
	riounlock(file->handle, pos);
#endif
}

void verase()
{
	INT i1, handle;
	UCHAR *adr;

	adr = getvar(VAR_READ);
	cvtoname(adr);
	dbcflags &= ~DBCFLAG_OVER;
	if (name[0]) {
		miofixname(name, ".txt", FIXNAME_EXT_ADD);
#ifndef DX_SINGLEUSER
		i1 = srvgetprimary();
		if (i1) {
			if (i1 == -1) {
				if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(657, (UCHAR *) name, (INT) strlen(name));
				dbcerror(657);
			}
			handle = fsopenraw(i1, name, FS_EXCLUSIVE);
			if (handle >= 0) handle = fsclosedelete(handle);
			if (handle < 0) {
				/* convert back to fio type error (required for pre-fs 2.1.3) */
				if (handle == -601) handle = ERR_FNOTF;
				else if (handle == -602 || handle == -607) handle = ERR_NOACC;
				else if (handle == -603) handle = ERR_BADNM;
				else if (handle == -614) handle = ERR_NOMEM;
				else if (handle < -100) {
					i1 = (INT)strlen(name);
					strcpy(name + i1, ", fs error = ");
					i1 += (INT)strlen(name + i1);
					mscitoa(-handle, name + i1);
					handle = ERR_OPERR;
				}
				else if (handle == -1) {
					i1 = (INT)strlen(name);
					fsgeterror(name + i1 + 2, (INT) (sizeof(name) - i1 - 2));
					if (name[i1 + 2]) {
						name[i1] = ',';
						name[i1 + 1] = ' ';
					}
					handle = ERR_OPERR;
				}
			}
		}
		else
#endif
		{
			miofixname(name, ".txt", FIXNAME_EXT_ADD);
			handle = fioopen(name, FIO_M_EXC | FIO_P_TXT);
			/* reset memory pointers */
			setpgmdata();
			if (handle >= 0) handle = fiokill(handle);
		}
		if (handle < 0) {
			if (handle == ERR_FNOTF) dbcflags |= DBCFLAG_OVER;
			else {
				if (handle == ERR_NOACC) i1 = 602;
				else if (handle == ERR_BADNM) i1 = 603;
				else i1 = 1630 - handle;
				if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(i1, (UCHAR *) name, (INT) strlen(name));
				else dbcerror(i1);
			}
		}
	}
}

void vrename()
{
#ifndef DX_SINGLEUSER
	INT i2, i3;
#endif
	INT i1, handle;
	CHAR work1[MAX_NAMESIZE];
	UCHAR *adr;

	adr = getvar(VAR_READ);
	cvtoname(adr);
	strcpy(work1, name);
	adr = getvar(VAR_READ);
	cvtoname(adr);
	if (work1[0] && name[0]) {
		miofixname(work1, ".txt", FIXNAME_EXT_ADD);
		miofixname(name, ".txt", FIXNAME_EXT_ADD);
#ifndef DX_SINGLEUSER
		i1 = srvgetprimary();
		if (i1) {
			if (i1 == -1) {
				if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(657, (UCHAR *) work1, (INT) strlen(work1));
				dbcerror(657);
			}
			if (fsversion(i1, &i2, &i3) || i2 < 3 || (i2 == 3 && i3 < 1)) {  /* check for pre-FS 3.1 */
				CHAR work[1024];
				strcpy(work, "RENAME ");
				strcat(work, work1);
				strcat(work, " ");
				strcat(work, name);
				i1 = fsexecute(i1, work);
				if (i1 < 0) {
					if (i1 < -750 && i1 >= -799) i1 = 1000 - i1;
					else if (i1 < -1000 && i1 >= -1049) i1 = 1750 - i1;
					else if (i1 == -1) i1 = 781;
					else i1 = -i1;
					if ((dioflags & DIOFLAG_DSPERR) && i1 == 781) {
						fsgeterror(name, sizeof(name));
						if (name[0]) dbcerrinfo(i1, (UCHAR *) name, (INT) strlen(name));
					}
					dbcerror(i1);
				}
				return;
			}
			handle = fsopenraw(i1, work1, FS_EXCLUSIVE);
			if (handle < 0) {
				/* convert back to fio type error (required for pre-fs 2.1.3) */
				if (handle == -601) handle = ERR_FNOTF;
				else if (handle == -602 || handle == -607) handle = ERR_NOACC;
				else if (handle == -603) handle = ERR_BADNM;
				else if (handle == -614) handle = ERR_NOMEM;
				else if (handle < -100) {
					i1 = (INT)strlen(work1);
					strcpy(work1 + i1, ", fs error = ");
					i1 += (INT)strlen(work1 + i1);
					mscitoa(-handle, work1 + i1);
					handle = ERR_OPERR;
				}
				else if (handle == -1) {
					i1 = (INT)strlen(work1);
					fsgeterror(work1 + i1 + 2, (INT) (sizeof(work1) - i1 - 2));
					if (work1[i1 + 2]) {
						work1[i1] = ',';
						work1[i1 + 1] = ' ';
					}
					handle = ERR_OPERR;
				}
			}
		}
		else
#endif
		{
			handle = fioopen(work1, FIO_M_EXC | FIO_P_TXT);
			/* reset memory pointers */
			setpgmdata();
		}
		if (handle < 0) {
			if (handle == ERR_FNOTF) i1 = 601;
			else if (handle == ERR_NOACC) i1 = 602;
			else if (handle == ERR_BADNM) i1 = 603;
			else i1 = 1630 - handle;
			if (dioflags & DIOFLAG_DSPERR) {
				strcpy(name, work1);
				dbcerrinfo(i1, (UCHAR *) name, (INT) strlen(name));
			}
			dbcerror(i1);
		}
#ifndef DX_SINGLEUSER
		if (i1) {  /* file server */
			i1 = fsrename(handle, name);
			if (i1 < 0) {
				/* convert back to fio type error (required for pre-fs 2.1.3) */
				if (i1 == -601) i1 = ERR_FNOTF;
				else if (i1 == -605) i1 = ERR_NOACC;
				else if (i1 == -605) i1 = ERR_BADNM;
				else if (i1 < -100) {
					i2 = (INT)strlen(name);
					strcpy(name + i2, ", fs error = ");
					i2 += (INT)strlen(name + i2);
					mscitoa(-i1, name + i2);
					i1 = ERR_OPERR;
				}
				else if (i1 == -1) {
					i1 = (INT)strlen(name);
					fsgeterror(name + i1 + 2, (INT) (sizeof(name) - i1 - 2));
					if (name[i1 + 2]) {
						name[i1] = ',';
						name[i1 + 1] = ' ';
					}
					i1 = ERR_OPERR;
				}
			}
		}
		else
#endif
			i1 = fiorename(handle, name);
		if (i1) {
			if (i1 == ERR_FNOTF) i1 = 601;
			else if (i1 == ERR_NOACC) i1 = 605;
			else i1 = 1630 - i1;
			if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(i1, (UCHAR *) name, (INT) strlen(name));
			dbcerror(i1);
		}
	}
}

/*
 * move data from record into the variable var
 */
INT recgetv(UCHAR *var, INT flags, UCHAR *rec, INT *recpos_p, INT recsize_p)
{
	INT i1, i2, i3, i4, state, decpt;
	UCHAR c, *ptr, *startptr, work[32], cobolwork[32];

	if ((i4 = recsize_p - *recpos_p) <= 0) {  /* no more data to move */
		if (vartype & TYPE_CHAR) {  /* dim variable */
			memset(&var[hl], ' ', (UINT) pl);
			fp = lp = 0;
			setfplp(var);
		}
		else if (vartype & TYPE_NUMVAR) itonv(0, var);
		return(0);
	}
	if (vartype & TYPE_CHAR) {  /* dim variable */
		i1 = pl;
		if (i1 > i4) i1 = i4;
		memcpy(&var[hl], &rec[*recpos_p], (UINT) i1);
		fp = 1;
		lp = i1;
		setfplp(var);
		*recpos_p += i1;
		return(0);
	}
	if (vartype & TYPE_NUM) {  /* form */
		for (i1 = 1; i1 < lp && var[i1] != '.'; i1++);
		i2 = lp - i1;
	}
	else if (vartype & TYPE_INT) {  /* integer variable */
		i1 = var[1];
		i2 = 0;
	}
	else if (vartype & TYPE_FLOAT) {  /* float variable */
		i1 = ((var[0] & 0x03) << 3) | (var[1] >> 5);
		i2 = var[1] & 0x1F;
		if (i2) i1++;
	}
	else return(0);
	ptr = &rec[*recpos_p];
	i3 = i1 + i2;
	if (i3 > i4) memset(&rec[*recpos_p + i4], '0', (UINT) i3 - i4);  /* fill with zeroes */
	*recpos_p += i3;
	if ((flags & RECFLAG_COBOL) && i2) {  /* create temporary storage with decimal inserted */
		(*recpos_p)--;
		memcpy(cobolwork, ptr, (UINT) i1 - 1);
		cobolwork[i1 - 1] = '.';
		memcpy(&cobolwork[i1], &ptr[i1 - 1], (UINT) i2);
		for (i4 = 0; i4 < i2; i4++) if (cobolwork[i1 + i4] == ' ') cobolwork[i1 + i4] = '0';
		ptr = cobolwork;
	}
	startptr = ptr;
	if (vartype & TYPE_NUM) {  /* attempt no movevar for well formed reads into form variables */
		if (i3 == 1 && *ptr == '.') goto fmterror;  /* "." */
		if (i3 == 2 && *ptr == '-' && *(ptr + 1) == '.') goto fmterror;  /* "-." */
		for (state = decpt = 0; i3; i3--) {
			c = *ptr++;
			if (state == 0) {
				if (c == ' ') continue;
				else if (c >= '1' && c <= '9') state = 2;
				else if (c == '0') {
					if (i3 == 1) {
						if (i2) goto fmterror;
						goto fmtok;
					}
					else goto fmtalter;
				}
				else if (c == '-') state = 1;
				else if (c == '.') {
					decpt = i3;
					state = 3;
				}
				else if ((c == '{' || c == '}' || (c >= 'A' && c <= 'R') || (c >= 'p' && c <= 'y')) && i3 == 1) goto fmtalter;
				else goto fmterror;
			}
			else if (state == 1) {
				if (c >= '1' && c <= '9') state = 2;
				else if (c == '.') {
					decpt = i3;
					state = 3;
				}
				else if (c == '0' || c == ' ') goto fmtalter;
				else goto fmterror;
			}
			else if (state == 2) {
				if (c >= '0' && c <= '9') continue;
				else if (c == '.') {
					decpt = i3;
					state = 3;
				}
				else if (c == ' ' || ((c == '{' || c == '}' || (c >= 'A' && c <= 'R') || (c >= 'p' && c <= 'y')) && i3 == 1)) goto fmtalter;
				else goto fmterror;
			}
			else if (state == 3) {
				if (c >= '0' && c <= '9') continue;
				else if (c == ' ' || ((c == '{' || c == '}' || (c >= 'A' && c <= 'R') || (c >= 'p' && c <= 'y')) && i3 == 1)) goto fmtalter;
				else goto fmterror;
			}
		}
		if (state == 0) goto fmtalter;
		if (decpt) {
			if (decpt-- == 1) goto fmterror;  /* decimal point in last position */
		}
		if (i2 != decpt) goto fmterror;  /* decimal point location is wrong */
		goto fmtok;
	}

fmtalter:  /* format might be ok after cleaning up */
	i3 = i1 + i2;
	work[0] = (UCHAR)(0x80 + i3);
	memcpy(&work[1], startptr, (UINT) i3);
	if (!typ(work)) {  /* check for strange formats that are valid */
		for (i4 = 1; i4 <= i3 && work[i4] == ' '; i4++);
		if (i4 == i3 + 1) goto fmtzero;
		c = work[i3];
		if (c == '{') work[i3] = '0';
		else if (c >= 'A' && c <= 'I') work[i3] = (UCHAR)((c - 'A') + '1');
		else if ((c >= 'p' && c <= 'y') || c == '}' || (c >= 'J' && c <= 'R')) {
			if (c >= 'p' && c <= 'y') work[i3] = (UCHAR)((c - 'p') + '0');
			else if (c == '}') work[i3] = '0';
			else work[i3] = (UCHAR)((c - 'J') + '1');
			for (i1 = 1; work[i1] == ' ' || work[i1] == '0'; i1++)
				if (work[i1] == '0') work[i1] = ' ';
			if (i1 == 1) goto fmterror;
			work[i1 - 1] = '-';
		}
		if (!typ(work)) goto fmterror;  /* try once more */
	}
	for (i4 = 1; i4 < i3 && work[i4] != '.'; i4++);
	i4 = i3 - i4;
	if (i2 != i4) goto fmterror;

fmtmove:  /* all is well, move the variable to destination */
	i1 = dbcflags & DBCFLAG_FLAGS;
	movevar(work, var);
	dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1;
	return(0);

fmtzero:
	work[0] = 0x81;
	work[1] = '0';
	goto fmtmove;

fmtok:  /* format is ok */
	memcpy(&var[1], startptr, (UINT) (i1 + i2));
	return(0);

fmterror:  /* format error */
	return(201);
}

/* move data from the variable var into record */
INT recputv(UCHAR *var, INT flags, UCHAR *rec, UINT *recpos_p, UINT *recsize_p, UINT recmax, UCHAR *fmt)
{
	INT i1, i2, i3, truncflag, type;
	UCHAR c, work[131], *ptr;

	type = vartype;
	truncflag = FALSE;
	if (*recpos_p > recmax) *recpos_p = recmax;
	if ((flags & RECFLAG_FORMAT) && !(type & TYPE_LITERAL)) {
		work[0] = work[1] = '\0';
		work[2] = 0x7F;
		i2 = vartype;
		i1 = dbcflags & DBCFLAG_FLAGS;
		vformat(var, fmt, work);
		dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1;
		work[2] = work[1];
		scanvar(work);
		vartype = i2;
		type = TYPE_CHARLIT;
		var = work;
	}
	if ((type & TYPE_LITERAL) || ((type & TYPE_NUM) && !(flags & (RECFLAG_ZERO_FILL | RECFLAG_ZERO_SUPPRESS | RECFLAG_MINUS_OVERPUNCH | RECFLAG_COBOL)))) {  /* form variable with nothing special */
		if (*recpos_p + lp > recmax) {
			lp = recmax - *recpos_p;
			truncflag = TRUE;
		}
		memcpy(&rec[*recpos_p], &var[hl], (UINT) lp);
		if ((*recpos_p += lp) > *recsize_p) *recsize_p = *recpos_p;
		return(truncflag);
	}
	if (type & TYPE_CHAR) {  /* character variable */
		i3 = hl;
		if (fp) {
			if (flags & RECFLAG_LOGICAL_LENGTH) {
				i1 = lp - fp + 1;
				i3 += fp - 1;
			}
			else i1 = lp;
		}
		else i1 = 0;
		if (flags & (RECFLAG_LOGICAL_LENGTH | RECFLAG_BLANK_SUPPRESS)) i2 = 0;
		else i2 = pl - i1;
		if (*recpos_p + i1 > recmax) {
			i1 = recmax - *recpos_p;
			truncflag = TRUE;
		}
		memcpy(&rec[*recpos_p], &var[i3], (UINT) i1);
		*recpos_p += i1;
		if (i2) {
			if (*recpos_p + i2 > recmax) {
				i2 = recmax - *recpos_p;
				truncflag = TRUE;
			}
			memset(&rec[*recpos_p], ' ', (UINT) i2);
		}
		if ((*recpos_p += i2) > *recsize_p) *recsize_p = *recpos_p;
		return(truncflag);
	}
	nvmkform(var, work);
	if (flags & (RECFLAG_COBOL | RECFLAG_ZERO_SUPPRESS | RECFLAG_ZERO_FILL | RECFLAG_MINUS_OVERPUNCH)) {
		for (i1 = 1; i1 < lp && work[i1] == ' '; i1++);
		if (work[i1] == '-') {  /* assume that i1 != lp */
			flags &= ~RECFLAG_ZERO_SUPPRESS;
			if (flags & (RECFLAG_COBOL | RECFLAG_MINUS_OVERPUNCH)) {
				work[i1++] = ' ';
				c = work[lp];
				if (dioflags & DIOFLAG_ASCIIMP) work[lp] = (UCHAR)((c - '0') + 'p');
				else if (c == '0') work[lp] = '}';
				else work[lp] = (UCHAR)((c - '1') + 'J');
			}
		}
		if (flags & RECFLAG_COBOL) {
			for (i2 = i1; i2 < lp && work[i2] != '.'; i2++);
			if (i2 < lp) {
				memmove(&work[i2], &work[i2 + 1], (UINT) (lp - i2));
				lp--;
			}
		}
		else if (flags & RECFLAG_ZERO_SUPPRESS) {
			for (i2 = i1; i2 <= lp && (work[i2] == '.' || work[i2] == '0'); i2++);
			if (i2 > lp) {
				memset(&work[i1], ' ', (UINT) (lp - (i1 - 1)));
				flags &= ~RECFLAG_ZERO_FILL;
			}
		}
		if ((flags & (RECFLAG_COBOL | RECFLAG_ZERO_FILL)) && i1 > 1) {
			ptr = &work[1];
			if (work[i1] == '-') *ptr++ = '-';
			memset(ptr, '0', (UINT) i1 - 1);
		}
	}
	if (*recpos_p + lp > recmax) {
		lp = recmax - *recpos_p;
		truncflag = TRUE;
	}
	memcpy(&rec[*recpos_p], &work[1], (UINT) lp);
	if ((*recpos_p += lp) > *recsize_p) *recsize_p = *recpos_p;
	return(truncflag);
}

/* flag an open file as common (don't close) */
void dioflag(INT refnum)
{
	(*filetabptr + refnum - 1)->flags |= FLAG_COM;
}

/*
 * close files for a given module. if mod <= 0, close all files.
 * if mod == 0, clear refnum in corresponding davb
 */
void dioclosemod(INT mod)
{
	INT i1, newfilehi;
	FILEINFO *file;

	if (!filemax) return;
	newfilehi = 0;
	for (i1 = 0, file = *filetabptr; i1++ < filehi; file++) {
		if (!file->type) continue;
		if (file->flags & FLAG_COM) {
			file->flags &= ~FLAG_COM;
			newfilehi = i1;
			continue;
		}
		if (mod <= 0 || mod == file->module) {
			closeFile(file, FALSE);
//			if (file->filename != NULL) memfree((UCHAR **)file->filename);
//			if (file->flags & FLAG_NAT) nioclose(file->handle);
//			else if (file->flags & FLAG_SRV) fsclose(file->handle);
//			else {
//				rioclose(file->handle);
//				if (file->type == DAVB_IFILE) xioclose(file->xhandle);
//				else if (file->type == DAVB_AFILE) aioclose(file->xhandle);
//			}
//			file->type = 0;
			/* clear refnum in davb */
			if (mod == 0) (aligndavb(findvar(file->module, file->offset), NULL))->refnum = 0;
		}
		else newfilehi = i1;
	}
	filehi = newfilehi;
}

static void closeFile(FILEINFO *file, INT delflg) {
	if (file->filename != NULL) memfree((UCHAR **)file->filename);
	if (file->flags & FLAG_NAT) nioclose(file->handle);
#ifndef DX_SINGLEUSER
	else if (file->flags & FLAG_SRV) {
		if (delflg) fsclosedelete(file->handle);
		else fsclose(file->handle);
	}
#endif
	else if (!delflg) {
		rioclose(file->handle);
		if (file->type == DAVB_IFILE) xioclose(file->xhandle);
		else if (file->type == DAVB_AFILE) aioclose(file->xhandle);
	}
	else {
		riokill(file->handle);
		if (file->type == DAVB_IFILE) xiokill(file->xhandle);
		else if (file->type == DAVB_AFILE) aiokill(file->xhandle);
	}
	file->type = 0;
}

/*
 * close a davb
 */
static void closedavb(DAVB *davb, INT delflg)
{
	FILEINFO *file;
	file = *filetabptr + davb->refnum - 1;
	closeFile(file, delflg);
//	if (file->filename != NULL) memfree((UCHAR **)file->filename);
//	if (file->flags & FLAG_NAT) nioclose(file->handle);
//	else if (file->flags & FLAG_SRV) {
//		if (delflg) fsclosedelete(file->handle);
//		else fsclose(file->handle);
//	}
//	else if (!delflg) {
//		rioclose(file->handle);
//		if (file->type == DAVB_IFILE) xioclose(file->xhandle);
//		else if (file->type == DAVB_AFILE) aioclose(file->xhandle);
//	}
//	else {
//		riokill(file->handle);
//		if (file->type == DAVB_IFILE) xiokill(file->xhandle);
//		else if (file->type == DAVB_AFILE) aiokill(file->xhandle);
//	}
//	file->type = 0;
	if (davb->refnum == filehi) filehi--;
	davb->refnum = 0;
}

/* rename old module number to new module number */
void diorenmod(INT oldmod, INT newmod)
{
	INT i1;
	FILEINFO *file;

	if (!filemax) return;

	for (i1 = 0, file = *filetabptr; i1++ < filehi; file++)
		if (file->type && oldmod == file->module) file->module = newmod;
}

/* get the name of the associated file name (for the debugger) */
CHAR *diofilename(INT refnum)
{
	FILEINFO *file;
	if (!refnum) return(NULL);
	file = *filetabptr + refnum - 1;
	return(*file->filename);
}

/* get the name of the associated indexfile name (for the debugger) */
CHAR *dioindexname(INT refnum)
{
	FILEINFO *file;

	if (!refnum) return(NULL);
	file = *filetabptr + refnum - 1;
	if (file->type != DAVB_IFILE && file->type != DAVB_AFILE) return(NULL);
	return(fioname(file->xhandle));
}

static void savenameinfileinfo(INT refnum) {
	INT i1;
	FILEINFO *file;
	CHAR **work;
	i1 = (INT)strlen(name) + 1;
	file = *filetabptr + refnum;
	if (file->filename == NULL) {
		work = (CHAR**) memalloc(i1, 0);
		if (work == NULL) dbcerror(1630 - ERR_NOMEM);
		file = *filetabptr + refnum;
		file->filename = work;
		file->fnbcount = i1;
	}
	else if (i1 > file->fnbcount) {
		if (memchange((UCHAR**) file->filename, i1, 0)) dbcerror(1630 - ERR_NOMEM);
		file = *filetabptr + refnum;
		file->fnbcount = i1;
	}
	/* reset memory pointers */
	setpgmdata();
	memcpy(*file->filename, name, i1);
}

static INT getfileinfo(INT reclen)
{
	static INT recmax = 0;
	INT i1, refnum;
	CHAR *ptr;
	FILEINFO *file;

	if (!filemax) {  /* perform initializations */
		filetabptr = (FILEINFO **) memalloc(64 * sizeof(FILEINFO), 0);
		/* reset memory pointers */
		setpgmdata();
		if (filetabptr == NULL) dbcerror(1630 - ERR_NOMEM);
		filemax = 64;

		dioflags = 0;
		if (!prpget("file", "static", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "off")) dioflags |= DIOFLAG_NOSTATIC;
		if (!prpget("file", "wttrunc", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) dioflags |= DIOFLAG_WTTRUNC;
		if (!prpget("file", "updtfill", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) dioflags |= DIOFLAG_OLDUPDT;
		if (!prpget("file", "filepi", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "noerr")) dioflags |= DIOFLAG_RESETFPI;
		if (!prpget("file", "error", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) dioflags |= DIOFLAG_DSPERR;
		if (!prpget("file", "minusoverpunch", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "ascii")) dioflags |= DIOFLAG_ASCIIMP;
		if (!prpget("file", "aiminsert", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) dioflags |= DIOFLAG_AIMINSERT;
	}

	if (reclen > recmax) {  /* allocate memory for record buffer */
		i1 = (!recmax) ? 8192 : recmax;
		while (reclen > i1) i1 <<= 1;
		ptr = (CHAR *)((!recmax) ? malloc((UINT) i1 + 4) : realloc(record, (UINT) (i1 + 4)));
		if (ptr == NULL) dbcerror(1630 - ERR_NOMEM);
		record = (UCHAR *) ptr;
		recmax = i1;
	}

	for (refnum = 0, file = *filetabptr; refnum < filehi && file->type; refnum++, file++);
	if (refnum == filemax) {
		i1 = memchange((UCHAR **) filetabptr, (INT) ((filemax << 1) * sizeof(FILEINFO)), 0);
		setpgmdata();
		if (i1) dbcerror(1630 - ERR_NOMEM);
		filemax <<= 1;
		file = *filetabptr + refnum;
	}
	memset((UCHAR *) file, 0, sizeof(FILEINFO));
	if (refnum == filehi) filehi++;
	return(refnum);
}

#if OS_WIN32
static void loadnio()
{
	CHAR *ptr;
	niohandle = LoadLibrary("DBCNIO");
	if (!prpget("file", "error", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) dioflags |= DIOFLAG_DSPERR;
	if (niohandle == NULL) {
		if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(653, "Missing DBCNIO", -1);
		else dbcerror(653);
	}

	nioopen = (_NIOOPEN *) GetProcAddress(niohandle, "nioopen");
	nioprep = (_NIOPREP *) GetProcAddress(niohandle, "nioprep");
	nioclose = (_NIOCLOSE *) GetProcAddress(niohandle, "nioclose");
	niordseq = (_NIORDSEQ *) GetProcAddress(niohandle, "niordseq");
	niowtseq = (_NIOWTSEQ *) GetProcAddress(niohandle, "niowtseq");
	niordrec = (_NIORDREC *) GetProcAddress(niohandle, "niordrec");
	niowtrec = (_NIOWTREC *) GetProcAddress(niohandle, "niowtrec");
	niordkey = (_NIORDKEY *) GetProcAddress(niohandle, "niordkey");
	niordks = (_NIORDKS *) GetProcAddress(niohandle, "niordks");
	niordkp = (_NIORDKP *) GetProcAddress(niohandle, "niordkp");
	niowtkey = (_NIOWTKEY *) GetProcAddress(niohandle, "niowtkey");
	nioupd = (_NIOUPD *) GetProcAddress(niohandle, "nioupd");
	niodel = (_NIODEL *) GetProcAddress(niohandle, "niodel");
	niolck = (_NIOLCK *) GetProcAddress(niohandle, "niolck");
	nioulck = (_NIOULCK *) GetProcAddress(niohandle, "nioulck");
	niofpos = (_NIOFPOS *) GetProcAddress(niohandle, "niofpos");
	niorpos = (_NIORPOS *) GetProcAddress(niohandle, "niorpos");
	nioclru = (_NIOCLRU *) GetProcAddress(niohandle, "nioclru");
	nioerror = (_NIOERROR *) GetProcAddress(niohandle, "nioerror");
	if (nioopen == NULL || nioprep == NULL || nioclose == NULL ||
	    niordseq == NULL || niowtseq == NULL || niordrec == NULL ||
	    niowtrec == NULL || niordkey == NULL || niordks == NULL ||
	    niordkp == NULL || niowtkey == NULL || nioupd == NULL ||
	    niodel == NULL || niolck == NULL || nioulck == NULL ||
	    nioclru == NULL || nioerror == NULL) {
		FreeLibrary(niohandle);
		if (dioflags & DIOFLAG_DSPERR) dbcerrinfo(655, "Missing proc in DBCNIO", -1);
		else dbcerror(655);
	}

	nioinitflag = NIO_LOADED;
	if (niofpos != NULL) nioinitflag |= NIO_FPOSIT;
	if (niorpos != NULL) nioinitflag |= NIO_REPOSIT;
	atexit(freenio);
}

static void freenio()
{
	FreeLibrary(niohandle);
}
#endif
