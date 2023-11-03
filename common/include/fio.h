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

#ifndef _FIO_INCLUDED
#define _FIO_INCLUDED

/* maximum supported name size including terminating 0 */
#define MAX_NAMESIZE 256

/* maximum key size for index */
#define XIO_MAX_KEYSIZE 255	/* was 359, but this causes overflow in compression byte */

/* structure defines */
#if OS_UNIX && !defined(MAX_PATH)
#define MAX_PATH 256
#endif

#ifndef FHANDLE_DEFINED
#if OS_WIN32
typedef HANDLE FHANDLE;
#elif OS_UNIX
typedef INT FHANDLE;
#endif
#define FHANDLE_DEFINED
#endif


struct fioparmsstruct {
	INT flags;				/* or of FIO_FLAG_xxx */
	/*
	 * max num of files open to operating system, 0 means default to as many as will open
	 */
	INT openlimit;
	INT parmflags;			/* or of FIO_PARM_xxx */
	INT filetimeout;		/* filepi lock timeout */
	INT rectimeout;			/* record lock timeout */
	INT lockretrytime;		/* number of milliseconds to retry for lock */
	OFFSET fileoffset;		/* file offset of lock byte for filepi */
	OFFSET recoffset;		/* file offset of lock byte for record locking */
	OFFSET excloffset;		/* exclusive open file offset of lock byte */
	CHAR openpath[4096];	/* open directories */
	CHAR preppath[512];		/* prepare directory */
	CHAR srcpath[4096];		/* source directories */
	CHAR dbcpath[1024];		/* dbc file directories */
	CHAR editcfgpath[1024];	/* edit config directories */
	CHAR dbdpath[1024];		/* dbd file directories */
	CHAR prtpath[1024];		/* print file directories */
	CHAR tdfpath[512];		/* termdef source file directories */
	CHAR tdbpath[512];		/* termdef binary file directories */
	CHAR imgpath[1024];		/* image file directories */
	CHAR cftpath[MAX_PATH]; /* Smart Client file transfer directory */
	UCHAR casemap[256];		/* case map */
	UCHAR collatemap[256];	/* collating order */
	INT (*cvtvolfnc)(CHAR *, CHAR ***);	/* function to translate :VOLUME to directory */
};

struct ftab {			/* file access table */
	UCHAR **hptr;		/* pointer to handle table */
	UCHAR **lptr;		/* pointer to library table */
	UCHAR **wptr;		/* pointer to rio, xio or aio work struct */
};

struct htab {			/* operating system handle number table */
	FHANDLE hndl;			/* current handle (zero if temp closed) */
	SHORT opct;		/* open count (ftab structs using this struct) */
	SHORT mode;		/* corresponds to fio modes */
	SHORT lckflg;		/* 0x01 = low level lock, 0x02 = high level lock */
					/* 0x04 = record lock active */
	OFFSET fsiz;		/* last known file size */
	OFFSET npos;		/* next position on read or write */
	OFFSET lpos;		/* last position on read or write */
	UINT luse;		/* least recently used algorithm marker */
	UCHAR **pptr;		/* pointer to locked positions list */
	CHAR fnam[MAX_NAMESIZE + 1];	/* file name */
};

struct ltab {			/* library table */
	CHAR member[8];	/* member name */
	UCHAR createflg;	/* created entry */
	OFFSET filepos;		/* starting file position in library */
	OFFSET length;		/* size of member */
};

struct ptab {
	OFFSET pos;			/* position of lock */
	INT filenum;		/* filenum owning the lock */
	UCHAR **pptr;		/* pointer to next lock */
};

struct rtab {			/* record access information table */
	CHAR type;			/* 'R' */
	OFFSET lpos;		/* last position */
	OFFSET npos;		/* next position */
	OFFSET bpos;		/* buffer start file position */
	OFFSET fsiz;		/* last known file size */
	UINT opts;			/* options */
	UCHAR eofc;			/* end of file character */			
	UCHAR bflg;			/* buffer flag 0 = invalid, 1 = read, 2 = write */			
	INT bsiz;			/* buffer size */
	INT bwst;			/* buffer write start offset */
	INT blen;			/* buffer valid length (already read or to write) */
	UCHAR **bptr;		/* pointer to buffer */
};

struct xtab {			/* index access information table */
	CHAR type;		/* 'X' */
	UCHAR version;		/* version number */
	SHORT opts;		/* opts from open */
	SHORT blksize;		/* block size */
	SHORT keylen;		/* key length */
	/**
	 * 1 = on a key
	 * 2 = before first key
	 * 3 = between keys
	 * 4 = after last key
	 */
	UCHAR keyflg;
	SHORT errflg;		/* error during buffer flush */
	OFFSET curblk;		/* current key block file position */
	/**
	 * pointer to caller's txtpos variable
	 */
	OFFSET *ptxtpos;
	UCHAR curkey[XIO_MAX_KEYSIZE + 12];	/* pointer to current key position string */
};

struct atab {			/* aim access information table */
	CHAR type;		/* 'A' */
	UCHAR status;		/* current read status */
					/* 0 = an error has occurred */
					/* 1 = empty lookup key status */
					/* 2 = one or more lookup keys are valid */
					/* 3 = at least one lookup key is not excluded */
					/* 4 = position is valid for another read */
					/* 5 = at end of file */
					/* 6 = at beginning of file */
					/* 7 = after an aiosetpos */
	UCHAR match;		/* match character */
	UCHAR dstnct;		/* distinct flag TRUE or FALSE */
	UCHAR delslot;		/* 1 if deleted record slot is used, else 0 */
	UCHAR version;		/* version number */
	SHORT hdrsize;		/* header size */
	SHORT slots;		/* number of slots */
	SHORT keys;		/* number of keys */
	SHORT lupsiz;		/* current lookup key table size in bytes */
	SHORT lupmax;		/* total size available in lookup key table */
	SHORT hptsiz;		/* hash pattern table size in bytes */
	OFFSET fxnrec;		/* first extent number of records (constant) */
	OFFSET sxnrec;		/* second extent number of records (constant) */
	OFFSET recnum;		/* last matching record number */
	OFFSET anfrec;		/* current AND buffer first record number */
	OFFSET anlrec;		/* current AND buffer last record number + 1 */
	OFFSET exhdfp;		/* current extent header file position */
	OFFSET exfrec;		/* current extent first record number */
	OFFSET exnrec;		/* current extent number of records */
	OFFSET exslfp;		/* current extent slot 0 file position */
	OFFSET *precnum;		/* pointer to caller's record number */
	UCHAR **defptr;	/* key definition table */
	UCHAR **lupptr;	/* lookup key table */
	UCHAR **andptr;	/* and buffer */
	UCHAR hptable[250];	/* hash pattern table */
};

typedef struct fioparmsstruct FIOPARMS;

typedef struct {
	OFFSET fileoffset;		/* filepi file offset of lock */
	OFFSET recoffset;		/* record lock offset of record lock */
#if OS_UNIX
	OFFSET excloffset;		/* exclusive lock offset */
#endif
#if OS_WIN32
	INT lockretrytime;		/* time to sleep between lock retries */
#endif
} FIOAINITSTRUCT;

/* special byte values in DB/C type records */
#define DBCSPC 0xF9
#define DBCEOR 0xFA
#define DBCEOF 0xFB
#define DBCDEL 0xFF

/* fio flags */
#define FIO_FLAG_SINGLEUSER		  0x00000001  /* single user, no locking, dbcdx.file.sharing=off */
#define FIO_FLAG_SEMLOCK		  0x00000002  /* use semaphore locking */
#define FIO_FLAG_NOCOMPRESS		  0x00000004  /* don't use space/digit compression in rio */
#define FIO_FLAG_KEYTRUNCATE	  0x00000008  /* truncate xio key length mismatches instead of error */
#define FIO_FLAG_EXCLOPENLOCK	  0x00000010  /* use record locking for exclusive opens */
#define FIO_FLAG_COMPATDOS		  0x00000020  /* miofixname compat=dos */
#define FIO_FLAG_COMPATRMS		  0x00000040  /* miofixname compat=rms */
#define FIO_FLAG_COMPATRMSX		  0x00000080  /* miofixname compat=rmsx */
#define FIO_FLAG_COMPATRMSY		  0x00000100  /* miofixname compat=rmsy */
#define FIO_FLAG_EXTCASEUPPER	  0x00000200  /* miofixname extcase=upper */
#define FIO_FLAG_NAMECASEUPPER	  0x00000400  /* miofixname namecase=upper */
#define FIO_FLAG_NAMECASELOWER	  0x00000800  /* miofixname namecase=lower */
#define FIO_FLAG_NAMEBLANKSSQUEEZE 0x00001000 /* miofixname squeeze all blanks */
#define FIO_FLAG_NAMEBLANKSNOCHOP 0x00002000 /* miofixname don't chop trailing blanks */
#define FIO_FLAG_PATHCASEUPPER	  0x00004000  /* miofixname pathcase=upper */
#define FIO_FLAG_PATHCASELOWER	  0x00008000  /* miofixname pathcase=lower */
#define FIO_FLAG_UTILPREPDOS	  0x00010000  /* aim insert does not invalidate lookup table */
#define FIO_FLAG_RANDWRTLCK       0x00020000  /* random writes lock the file for duration of process */

#define FIO_PARM_FILETIMEOUT	0x00000001  /* filetimeout parm specified */
#define FIO_PARM_RECTIMEOUT		0x00000002  /* rectimeout parm specified */
#define FIO_PARM_LOCKRETRYTIME	0x00000004 /* lockretrytime parm specified */
#define FIO_PARM_FILEOFFSET		0x00000008  /* fileoffset parm specified */
#define FIO_PARM_RECOFFSET		0x00000010  /* recoffset parm specified */
#define FIO_PARM_EXCLOFFSET		0x00000020  /* excloffset parm specified */

#define FIO_OPT_CASEMAP		1
#define FIO_OPT_COLLATEMAP	2
#define FIO_OPT_OPENPATH	11
#define FIO_OPT_PREPPATH	12
#define FIO_OPT_SRCPATH		13
#define FIO_OPT_DBCPATH		14
#define FIO_OPT_EDITCFGPATH	15
#define FIO_OPT_DBDPATH		16
#define FIO_OPT_PRTPATH		17
#define FIO_OPT_TDFPATH		18
#define FIO_OPT_TDBPATH		19
#define FIO_OPT_IMGPATH		20

/* error defines */
#define ERR_NOTOP (-21)	/* file not open */
#define ERR_FNOTF (-22)	/* file not found */
#define ERR_NOACC (-23)	/* access denied */
#define ERR_EXIST (-24)	/* file already exists */
#define ERR_EXCMF (-25)	/* exceed maximum files open */
#define ERR_BADNM (-26)	/* invalid name */
#define ERR_BADTP (-27)	/* invalid file type */
#define ERR_NOEOR (-28)	/* no end of record mark or record too long */
#define ERR_SHORT (-29)	/* record too short */
#define ERR_BADCH (-30)	/* invalid character encountered */
#define ERR_RANGE (-31)	/* beyond end of file */
#define ERR_ISDEL (-32)	/* record has already been deleted */
#define ERR_BADIX (-33)	/* index file is invalid */
#define ERR_BADKL (-34)	/* wrong key length */
#define ERR_BADRL (-35)	/* wrong record length */
#define ERR_BADKY (-36)	/* invalid key */
#define ERR_NOMEM (-37)	/* unable to allocate memory */
#define ERR_RDERR (-38)	/* unable to read */
#define ERR_WRERR (-39)	/* unable to write */
#define ERR_DLERR (-40)	/* unable to delete */
#define ERR_LKERR (-41)	/* unable to lock file or record */
#define ERR_BADLN (-42)	/* invalid character buffer length */
#define ERR_NOENV (-43)	/* unable to open environment file */
#define ERR_RDENV (-44)	/* unable to read environment file */
#define ERR_NOOPT (-45)	/* unable to open options file */
#define ERR_RDOPT (-46)	/* unable to read options file */
#define ERR_NOPRM (-47)	/* command line parameters not initialized */
#define ERR_RENAM (-48)	/* unable to rename */
#define ERR_CLERR (-49)	/* unable to close */
#define ERR_SKERR (-50)	/* unable to seek */
#define ERR_BADLB (-51)	/* bad library */
#define ERR_FHNDL (-52)	/* invalid value for file handle */
#define ERR_RONLY (-53)	/* attempted write on read-only file */
#define ERR_OPERR (-54)	/* unable to open */
#define ERR_INVAR (-55)	/* invalid argument */
#define ERR_NOSEM (-56)	/* no semaphores */
#define ERR_NOEOF (-57)	/* no EOF */
#define ERR_COLAT (-58)	/* error opening or reading collate file */
#define ERR_CASMP (-59)	/* error opening or reading casemap file */
#define ERR_IXDUP (-60) /* file indexed with duplicates */
#define ERR_PROGX (-68)	/* programming error */
#define ERR_OTHER (-69)	/* unspecified error */

/* bit masks for fio open mode */
/**
 * shared read only open, read only access
 */
#define FIO_M_SRO 0x00000001
/**
 * shared read only open, read/write access
 */
#define FIO_M_SRA 0x00000002
/**
 * shared read/write open
 */
#define FIO_M_SHR 0x00000003
#define FIO_M_ERO 0x00000004	/* exclusive read only open */
#define FIO_M_EXC 0x00000005	/* exclusive read/write open */
#define FIO_M_MXC 0x00000006	/* multiple exclusive read/write open */
/**
 * Exclusive read/write prepare,
 * truncate if existing, create if new
 */
#define FIO_M_PRP 0x00000007
#define FIO_M_MTC 0x00000008	/* multiple exclusive read/write prepare, */
								/* open if already opened, */
								/* truncate if existing, create if new */
#define FIO_M_EFC 0x00000009	/* exclusive read/write prepare, */
								/* fail if existing, create if new */
#define FIO_M_EOC 0x0000000A	/* exclusive read/write open/prepare, */
								/* open if existing, create if new */

/* bit masks for fio open path */
#define FIO_P_WRK 0x00000010	/* search/create current path only */
#define FIO_P_TXT 0x00000020	/* search OPENPATH, create PREPPATH */
#define FIO_P_SRC 0x00000030	/* search/create SRCPATH */
#define FIO_P_DBC 0x00000040	/* search/create DBCPATH */
#define FIO_P_CFG 0x00000050	/* search/create EDITCFG */
#define FIO_P_DBD 0x00000060	/* search/create DBDPATH */
#define FIO_P_PRT 0x00000070	/* search/create PRTPATH */
#define FIO_P_TDF 0x00000080	/* search/create TDFPATH */
#define FIO_P_TDB 0x00000090	/* search/create TDBPATH */
#define FIO_P_IMG 0x000000A0	/* search/create IMGPATH */
#define FIO_P_CFT 0x000000B0	/* search/create CFTPATH (Smart Client File Transfer) */

/* bit AND masks for fio open */
#define FIO_M_MASK 0x0000000F	/* mode */
#define FIO_P_MASK 0x000000F0	/* path */

/* bit masks for rio open mode */
#define RIO_M_SRO FIO_M_SRO		/* shared read only open, read only access */
#define RIO_M_SRA FIO_M_SRA		/* shared read only open, read/write access */
#define RIO_M_SHR FIO_M_SHR		/* shared read/write open */
#define RIO_M_ERO FIO_M_ERO		/* exclusive read only open */
#define RIO_M_EXC FIO_M_EXC		/* exclusive read/write open */
#define RIO_M_MXC FIO_M_MXC		/* multiple exclusive read/write open */
#define RIO_M_PRP FIO_M_PRP		/* exclusive read/write prepare, */
								/* truncate if existing, create if new */
#define RIO_M_MTC FIO_M_MTC		/* multiple exclusive read/write prepare, */
								/* open if already opened, */
								/* truncate if existing, create if new */
#define RIO_M_EFC FIO_M_EFC		/* exclusive read/write prepare, */
								/* fail if existing, create if new */
#define RIO_M_EOC FIO_M_EOC		/* exclusive read/write open/prepare, */
								/* open if existing, create if new */

/* bit masks for rio open path */
#define RIO_P_WRK FIO_P_WRK		/* search/create current path only */
/*
 * search OPENPATH, create PREPPATH
 */
#define RIO_P_TXT FIO_P_TXT
#define RIO_P_SRC FIO_P_SRC		/* search/create SRCPATH */
#define RIO_P_DBC FIO_P_DBC		/* search/create DBCPATH */
#define RIO_P_DBD FIO_P_DBD		/* search/create DBDPATH */
#define RIO_P_PRT FIO_P_PRT		/* search/create PRTPATH */
#define RIO_P_TDF FIO_P_TDF		/* search/create TDFPATH */

/* bit masks for rio open type */
#define RIO_T_STD 0x00010000	/* read/write DB/C standard record type file */
#define RIO_T_TXT 0x00020000	/* read/write run-time system text file */
#define RIO_T_DAT 0x00030000	/* read/write data type file */
#define RIO_T_DOS 0x00040000	/* read/write msdos type file */
#define RIO_T_MAC 0x00050000	/* read/write mac type file */
#define RIO_T_BIN 0x00070000	/* read/write binary file (implies RIO_FIX) */
#define RIO_T_ANY 0x00080000	/* allow _T_STD, _T_TXT or _T_DAT file */

/* miscellaneous bit masks for rio open */
#define RIO_UNC 0x00100000		/* write/expect uncompressed records (T_STD only) */
#define RIO_FIX 0x00200000		/* write/expect fixed records */
#define RIO_NOX 0x00400000		/* do not append .txt extension or modify name */
#define RIO_LOG 0x00800000		/* log record changes */

/* bit AND masks for rio open */
#define RIO_M_MASK FIO_M_MASK	/* mode */
#define RIO_P_MASK FIO_P_MASK	/* path */
#define RIO_T_MASK 0x000F0000	/* file type */

/* bit masks for rio logging */
#define RIO_L_USR 0x00000010	/* log username */
#define RIO_L_OPN 0x00000020	/* log open operations */
#define RIO_L_CLS 0x00000040	/* log close operations */
#define RIO_L_TIM 0x00000080	/* log timestamp */
#define RIO_L_NAM 0x00000100    /* log file names */

/* bit masks for xio open mode */
#define XIO_M_SRO FIO_M_SRO		/* shared read only open, read only access */
#define XIO_M_SRA FIO_M_SRA		/* shared read only open, read/write access */
#define XIO_M_SHR FIO_M_SHR		/* shared read/write open */
#define XIO_M_ERO FIO_M_ERO		/* exclusive read only open */
#define XIO_M_EXC FIO_M_EXC		/* exclusive read/write open */
#define XIO_M_PRP FIO_M_PRP		/* exclusive read/write create, */
								/* truncate if exists, make empty .isi file */

/* bit masks for xio open path */
#define XIO_P_WRK FIO_P_WRK		/* search/create current path only */
#define XIO_P_TXT FIO_P_TXT		/* search OPENPATH, create PREPPATH */

/* bit masks for xio open type */
#define XIO_T_STD RIO_T_STD		/* read/write DB/C standard record type file */
#define XIO_T_TXT RIO_T_TXT		/* read/write run-time system text file */
#define XIO_T_DAT RIO_T_DAT		/* read/write data text file */
#define XIO_T_BIN RIO_T_BIN		/* read/write binary file (implies RIO_FIX) */
#define XIO_T_ANY RIO_T_ANY		/* allow _T_STD, _T_TXT, or _T_DAT file */

/* miscellaneous bit masks for xio open */
#define XIO_UNC RIO_UNC			/* write/expect uncompressed records (T_STD only) */
#define XIO_FIX RIO_FIX			/* write/expect fixed records */
#define XIO_NOD 0x01000000		/* duplicate records not allowed */
#define XIO_DUP 0x02000000		/* duplicate records allowed */

/* bit AND masks for xio open */
#define XIO_M_MASK FIO_M_MASK	/* mode */
#define XIO_P_MASK FIO_P_MASK	/* path */
#define XIO_T_MASK RIO_T_MASK	/* file type */

/* bit masks for aio open mode */
#define AIO_M_SRO FIO_M_SRO		/* shared read only open, read only access */
#define AIO_M_SRA FIO_M_SRA		/* shared read only open, read/write access */
#define AIO_M_SHR FIO_M_SHR		/* shared read/write open */
#define AIO_M_ERO FIO_M_ERO		/* exclusive read only open */
#define AIO_M_EXC FIO_M_EXC		/* exclusive read/write open */
#define AIO_M_PRP FIO_M_PRP		/* exclusive read/write create, */
							/* truncate if exists, make empty .aim file */

/* bit masks for aio open path */
#define AIO_P_WRK FIO_P_WRK		/* search/create current path only */
#define AIO_P_TXT FIO_P_TXT		/* search OPENPATH, create PREPPATH */

/* bit masks for aio open type */
#define AIO_T_STD RIO_T_STD		/* read/write DB/C standard record type file */
#define AIO_T_TXT RIO_T_TXT		/* read/write run-time system text file */
#define AIO_T_DAT RIO_T_DAT		/* read/write data text file */
#define AIO_T_BIN RIO_T_BIN		/* read/write binary file (implies RIO_FIX) */
#define AIO_T_ANY RIO_T_ANY		/* allow _T_STD, _T_TXT, or _T_DAT file */

/* miscellaneous bit masks for aio open */
#define AIO_UNC RIO_UNC			/* write/expect uncompressed records (T_STD only) */
#define AIO_FIX RIO_FIX			/* write/expect fixed records */

/* bit AND masks for aio open */
#define AIO_M_MASK FIO_M_MASK	/* mode */
#define AIO_P_MASK FIO_P_MASK	/* path */
#define AIO_T_MASK RIO_T_MASK	/* file type */

/* bit masks for miofixname */
#define FIXNAME_EXT_ADD		0x0001
#define FIXNAME_EXT_REPLACE	0x0002
#define FIXNAME_PAR_DBCVOL	0x0010
#define FIXNAME_PAR_MEMBER	0x0020


/* miscellaneous defines for rio */
#define RIO_MAX_RECSIZE 65500

/* options for aionext */
#define AIONEXT_PREV	0
#define AIONEXT_NEXT	1
#define AIONEXT_LAST	2
#define AIONEXT_FIRST	3

/* fioa lock bits */
#define FIOA_RDLCK 0x01
#define FIOA_WRLCK 0x02
#define FIOA_UNLCK 0x04
#define FIOA_FLLCK 0x10
#define FIOA_RCLCK 0x20
#define FIOA_EXLCK 0x40

/* bit masks for htab.lckflg */
#define FIOX_LLK 0x0001
#define FIOX_HLK 0x0002

/* function prototypes */

/* fio.c */
extern CHAR *fioinit(FIOPARMS *, INT);
extern void fioexit(void);
extern void fiosetflags(INT);
extern INT fiogetflags(void);
extern FHANDLE fiogetOSHandle(INT fnum);
extern INT fiosetopt(INT, UCHAR *);
extern UCHAR **fiogetopt(INT);
extern INT fiocvtvol(CHAR *, CHAR ***);
extern INT fioopen(CHAR *, INT);
extern INT fioclose(INT);
extern INT fiokill(INT);
extern INT fioread(INT, OFFSET, UCHAR *, INT);
extern INT fiowrite(INT, OFFSET, UCHAR *, size_t);
extern INT fiogetsize(INT, OFFSET *);
extern INT fioclru(INT);
extern INT fiotouch(INT);
extern INT fioflush(INT);
extern INT fiotrunc(INT, OFFSET);
extern CHAR *fioname(INT);
extern INT fiolock(INT *);
extern void fiounlock(INT);
extern INT fioflck(INT);
extern void fiofulk(INT);
extern INT fiolckpos(INT, OFFSET, INT);
extern void fioulkpos(INT, OFFSET);
extern INT fiorename(INT fnum, CHAR *newname);
extern INT fiofindfirst(CHAR *name, INT search, CHAR **found);
extern INT fiofindnext(CHAR **found);
extern INT fiofindclose(void);
extern CHAR *fioerrstr(INT);
extern INT fiogetlpos(INT, OFFSET *);
extern INT fiosetlpos(INT, OFFSET);
extern UCHAR **fiogetwptr(INT);
extern INT fiosetwptr(INT, UCHAR **);

/* mio.c */
extern INT miofixname(CHAR *, CHAR *, INT);
extern INT miogetenv(CHAR *, CHAR **);
extern INT miostrscan(CHAR *, CHAR *);
extern INT mioinitparm(INT, CHAR **);
extern INT mionextparm(CHAR *);
extern INT miogetfilebytes(CHAR *, UCHAR *, INT);
extern INT miogetfilechgtime(CHAR *, UCHAR *);
extern INT miogettextrec(CHAR **, CHAR *, INT);
extern void miogetname(CHAR **, CHAR **);

/* rio.c */
extern INT riologstart(CHAR *, CHAR *, CHAR *, INT);
extern INT riologend(void);
extern INT rioopen(CHAR *name, INT opts, INT bufs, INT maxlen);
extern INT rioclose(INT);
extern INT riokill(INT);
extern INT rioget(INT, UCHAR *, INT);
extern INT rioprev(INT, UCHAR *, INT);
extern INT rioput(INT, UCHAR *, INT);
extern INT rioparput(INT, UCHAR *, INT);
extern INT riodelete(INT, INT);
extern INT riosetpos(INT, OFFSET);
extern INT rionextpos(INT, OFFSET *);
extern INT riolastpos(INT, OFFSET *);
extern INT riogrplpos(INT, OFFSET *);
extern INT rioeofpos(INT, OFFSET *);
extern INT rioweof(INT);
extern INT rioflush(INT);
extern INT riotype(INT);
extern INT rioeorsize(INT);
extern INT riolock(INT, INT);
extern void riounlock(INT, OFFSET);

/* xio.c */
extern INT xioopen(CHAR *, INT, INT, INT, OFFSET *, CHAR *);
extern INT xioclose(INT);
extern INT xiokill(INT);
extern INT xiofind(INT, UCHAR *, INT);
extern INT xiofindlast(INT, UCHAR *, INT);
extern INT xionext(INT);
extern INT xioprev(INT);
extern INT xioinsert(INT, UCHAR *, INT);
extern INT xiodelete(INT, UCHAR *, INT, INT);
extern INT xiogetrec(INT);
extern INT xiodelrec(INT);
extern INT xioflush(INT);

/* aio.c */
extern INT aioopen(CHAR *, INT, INT, OFFSET *, CHAR *, INT *, CHAR *, INT, INT);
extern INT aioclose(INT);
extern INT aiokill(INT);
extern INT aionew(INT);
extern INT aiokey(INT, UCHAR *, INT);
extern INT aionext(INT, INT);
extern INT aiochkrec(INT, UCHAR *, INT);
extern INT aioinsert(INT, UCHAR *);
extern INT aiogetrec(INT);
extern INT aiodelrec(INT);
extern void aiomatch(INT, INT);

/* fioa???.c */
extern INT fioainit(INT *, FIOAINITSTRUCT *);
extern INT fioaopen(CHAR *, INT, INT, FHANDLE *);
extern INT fioaclose(FHANDLE);
extern INT fioaread(FHANDLE, UCHAR *, INT, OFFSET, INT *);
extern INT fioawrite(FHANDLE, UCHAR *, size_t, OFFSET, size_t *);
extern INT fioalseek(FHANDLE, OFFSET, INT, OFFSET *);
extern INT fioalock(FHANDLE, INT, OFFSET, INT);
extern INT fioaflush(FHANDLE);
extern INT fioatrunc(FHANDLE, OFFSET);
extern INT fioadelete(CHAR *);
extern INT fioarename(CHAR *oldname, CHAR *newname);
extern INT fioafindfirst(CHAR *path, CHAR *file, CHAR **found);
extern INT fioafindnext(CHAR **found);
extern INT fioafindclose(void);
extern INT fioaslash(CHAR *);
extern void fioaslashx(CHAR *);

#endif  /* _FIO_INCLUDED */
