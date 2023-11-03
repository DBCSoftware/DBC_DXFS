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

#define DBGFLAGS_FIRST			0x00000001

#if OS_UNIX && !defined(MAX_PATH)
#define MAX_PATH 256
#endif

#ifdef DBCMAIN
#define EXTERN
INT dbcflags = 0;		/* various condition flags and db/c execution flags */
INT dbgflags = DBGFLAGS_FIRST;
INT srcfile = -1;
CHAR *GuiCtlfont = NULL;
INT counterBrkhi;
INT counterBrkptr = -1;
#else
#define EXTERN extern
EXTERN INT dbcflags;	/* various condition flags and db/c execution flags */
EXTERN INT dbgflags;
EXTERN INT srcfile;
EXTERN CHAR *GuiCtlfont;
EXTERN INT counterBrkhi;
EXTERN INT counterBrkptr;
#endif

#ifndef _PRT_INCLUDED
#include "prt.h"
#endif

EXTERN UCHAR *data;		/* pointer to data area location 0 of current module */
EXTERN UCHAR *pgm;		/* pointer to program location 0 of current module */
EXTERN CHAR name[4096];	/* parameter for several routines */
EXTERN CHAR DXAboutInfoString[512];
EXTERN INT datamodule;	/* current execution data module number */
EXTERN INT pgmmodule;	/* current execution program module number */
EXTERN INT pcount;		/* current execution program address */
EXTERN INT license;		/* license number */
EXTERN INT compat;		/* compatibility flag 1 = RMS[X|Y], 2 = DOS, 3 = DOSX, 4 = ANSMAS */
EXTERN INT fp, lp, pl, hl;  /* form and length pointers, physical and header lengths */
EXTERN INT vartype;		/* variable type flag */
EXTERN INT fpicnt;		/* FILEPI statements before expiration count */
EXTERN UINT license1;	/* license work number 1 */
EXTERN UINT license2;	/* license work number 2 */
EXTERN UCHAR vbcode, lsvbcode, vx;  /* current, previous, extended verb codes */
EXTERN UCHAR dbcbuf[65504];  /* DB/C buffer for miscellaneous use */
EXTERN UCHAR dsperrflg;
EXTERN UCHAR writeUntrappedErrorsFlag;
EXTERN CHAR writeUntrappedErrorsFile[MAX_PATH];


#define SMARTCLIENTISIOS (smartClientSubType[0] != '\0' \
						&& strlen(smartClientSubType) == 3 \
						&& !strcmp(smartClientSubType, "iOS"))
#define SMARTCLIENTISJAVA (smartClientSubType[0] != '\0' \
						&& strlen(smartClientSubType) == 4 \
						&& !strcmp(smartClientSubType, "Java"))

#define SUPPORTSTOOLBARTEXTPB (SMARTCLIENTISIOS || SMARTCLIENTISJAVA)
/*
 * If we are running in SC mode, and if the type was seen in the <hello/> message,
 * then put it in here for future reference.
 */
EXTERN CHAR smartClientSubType[MAX_SCSUBTYPE_CHARS + 1];
/*
 * If we are running in SC mode, and if the attribute 'utcoffset' was seen in the <smartclient/> message,
 * then put it in here for future reference.
 */
EXTERN CHAR smartClientUTCOffset[6];

/*
 * This info is kept to check for capabilities.
 * As of May 2015 it is used only for Unix Smart Clients that can support
 * ansi256 color mode for character mode programs.
 * This was introduced in version 16.3
 */
EXTERN INT smartClientMajorVersion;
EXTERN INT smartClientMinorVersion;


#define DBCSMALLICONNAME "DBCSMALL"
#define DBCLARGEICONNAME "DBCBIG"
#define DBCCSMALLICONNAME "DBCCSMAL"
#define DBCCLARGEICONNAME "DBCCBIG"

/* DB/C flags - do NOT change these values without changing dbcgetflags() */
#define DBCFLAG_EOS			0x0001
#define DBCFLAG_EQUAL			0x0002
#define DBCFLAG_LESS			0x0004
#define DBCFLAG_OVER			0x0008
#define DBCFLAG_FLAGS			0x000F

#define DBCFLAG_PRE_V7			0x00010
#define DBCFLAG_EXPRESSION		0x00020
#define DBCFLAG_DEBUG			0x00040

/**
 * This is only set if we are dbcc.exe and invoked via svcrun in svc.c
 * This usage is no longer documented but we keep it.
 * We allow dbcc to install and run itself as a service from a Windows command line.
 *
 * JPR 04 OCT 2013
 */
#ifndef DX_SINGLEUSER
#define DBCFLAG_SERVICE			0x00080
#endif

#define DBCFLAG_KDSINIT			0x00100
#define DBCFLAG_VIDINIT			0x00200

/**
 * This is set after a successful call to guiinit in dbcinit in dbcctl.c
 */
#define DBCFLAG_GUIINIT			0x00400

/**
 * This is set by the presence of either SCPORT or SCSERVER on the command line
 */
#ifndef DX_SINGLEUSER
#define DBCFLAG_SERVER			0x00800
#endif

/**
 * This is set only if entered via the 'main' routine.
 * So only when fired up from a Unix console or this is dbcc on Windows
 */
#define DBCFLAG_CONSOLE			0x01000

/**
 * This is set if DBCFLAG_SERVER is set and we successfully connect to the smart client
 */
#ifndef DX_SINGLEUSER
#define DBCFLAG_CLIENTINIT		0x02000
#endif

/**
 * This is set if SCGUI appears on the command line
 */
#define DBCFLAG_CLIENTGUI		0x04000
#define DBCFLAG_DDTDEBUG		0x08000
#if OS_WIN32
#define DBCFLAG_TRAYSTOP		0x10000
#endif

/* vartype flags */
#define TYPE_INT				0x0001
#define TYPE_FLOAT				0x0002
#define TYPE_NUM				0x0004
#define TYPE_NUMLIT			0x0008
#define TYPE_CHAR				0x0010
#define TYPE_CHARLIT			0x0020
#define TYPE_NULL				0x0080
#define TYPE_LIST				0x0100
#define TYPE_ARRAY				0x0200
#define TYPE_ADDRESS			0x0400

/* additional types for makevar(), note: these are not bits */
#define TYPE_DEVICE			0x1000
#define TYPE_IMAGE				0x2000
#define TYPE_RESOURCE			0x3000
#define TYPE_OBJECT			0x4000

/* vartype masks */
#define TYPE_NUMVAR			0x0007
#define TYPE_NUMERIC			0x000F
#define TYPE_CHARACTER			0x0030
#define TYPE_LITERAL			0x0028

/* getvar masks */
#define VAR_READ				0x0001
#define VAR_WRITE				0x0002
#define VAR_ADR				0x0004
#define VAR_ADRX				0x0008

/* getlist masks */
#define LIST_NUM1				0x0000	/* store list info in table 1 */
#define LIST_NUM2				0x0001	/* store list info in table 2 */
#define LIST_NUM3				0x0002	/* store list info in table 3 */
#define LIST_NUM4				0x0003	/* store list info in table 4 */
#define LIST_READ				0x0010	/* return variable for reading */
#define LIST_WRITE				0x0020	/* return variable for writing */
#define LIST_ADR				0x0040
#define LIST_ADRX				0x0080
#define LIST_PROG				0x0100	/* read program area for variables */
#define LIST_END				0x0200	/* end processing list */
#define LIST_IGNORE			0x0400	/* skip over non-character, non-numeric and non-array fields */
#define LIST_LIST				0x1000	/* return elements of a list (all levels) */
#define LIST_LIST1				0x2000	/* return elements of a list (first level only) */
#define LIST_ARRAY				0x4000	/* return elements of an array (all levels) */
#define LIST_ARRAY1			0x8000	/* return elements of an array (first level only) */

#define LIST_NUMMASK			0x0003  /* this is both a mask and highest table number */

struct davb {
	UCHAR type;		/* DAVB_xxxx  see below */
	UCHAR fill1[3];
	INT refnum;		/* positive reference value, zero if closed */
	union {
		struct {
			UCHAR bufs;		/* static buffer count */
			UCHAR fill2;
			USHORT flags;		/* DAVB_FLAG_xxxx  see below */
			INT recsz;		/* record size or address */
			INT keysz;		/* key size or address */
		} file;
	} info;
};

typedef struct davb DAVB;

#define DAVB_FILE				0xA0
#define DAVB_IFILE				0xA1
#define DAVB_AFILE				0xA2
#define DAVB_COMFILE			0xA3
#define DAVB_PFILE				0xA9
#define DAVB_OBJECT			0xAA
#define DAVB_IMAGE				0xAB
#define DAVB_QUEUE				0xAC
#define DAVB_RESOURCE			0xAD
#define DAVB_DEVICE			0xAE

/* flags field bit definitions */
#define DAVB_FLAG_VAR			0x0001  /* variable length records */
#define DAVB_FLAG_CMP			0x0002  /* compressed records */
#define DAVB_FLAG_DUP			0x0004  /* duplicate keys allowed in index */
#define DAVB_FLAG_TXT			0x0008  /* pre-v12 text type file */
#define DAVB_FLAG_VKY			0x0010  /* index key is a numeric variable */
#define DAVB_FLAG_NAT			0x0020  /* native type file */
#define DAVB_FLAG_VRS			0x0040  /* rec size is a numeric variable */
#define DAVB_FLAG_KLN			0x0080  /* ifile declaration includes keylen */
#define DAVB_FLAG_NOD			0x4000  /* no duplicate keys allowed in index */
#define DAVB_FLAG_CBL			0x8000  /* cobol type file */
#define DAVB_TYPE_MSK			0x3800  /* mask for file type */
#define DAVB_TYPE_STD			0x0000  /* standard file type */
#define DAVB_TYPE_TXT			0x0800  /* text file type */
#define DAVB_TYPE_DAT			0x1000  /* data file type */
#define DAVB_TYPE_DOS			0x1800  /* dos file type */
#define DAVB_TYPE_BIN			0x2000  /* binary file type */

/* flags used with recputv and recgetv */
#define RECFLAG_LOGICAL_LENGTH 0x01
#define RECFLAG_ZERO_FILL		0x02
#define RECFLAG_ZERO_SUPPRESS	0x04
#define RECFLAG_BLANK_SUPPRESS	0x08
#define RECFLAG_MINUS_OVERPUNCH 0x10
#define RECFLAG_COBOL			0x20
#define RECFLAG_FORMAT			0x40

/* flags used with makevar */
#define MAKEVAR_VARIABLE		0x01
#define MAKEVAR_ADDRESS		0x02
#define MAKEVAR_PFILE			0x04
#define MAKEVAR_NAME			0x08
#define MAKEVAR_GLOBAL			0x10
#define MAKEVAR_DATA			0x20

/* flags used with vdebug */
#define DEBUG_RESTORE			0x01
#define DEBUG_CHECK			0x02
#define DEBUG_DEBUG			0x04
#define DEBUG_ERROR			0x08
#define DEBUG_INTERRUPT		0x10
/* #define DEBUG_LOAD			0x20 */
/* #define DEBUG_UNLOAD			0x40 */
#define DEBUG_EXTRAINFO		0x80

/* flags used with dbginit */
#define DBGINIT_GLOBAL			0x01
#define DBGINIT_PREFERENCE		0x02
#define DBGINIT_BREAK			0x03
#define DBGINIT_DDT				0x04
#define DBGINIT_DDT_SERVER		0x05

/* flags used by dbcdbg and dbcddt */
#define DBGFLAGS_SINGLE			0x00000002
#define DBGFLAGS_CTRL_D			0x00000004
#define DBGFLAGS_SLOW			0x00000008
#define DBGFLAGS_SETBRK			0x00000010
#define DBGFLAGS_TSTCALL		0x00000020
#define DBGFLAGS_TSTRETN		0x00000040
#define DBGFLAGS_TSTVALU		0x00000080
#define DBGFLAGS_VIEWWIN		0x00000100
#define DBGFLAGS_RESULTWIN		0x00000200
#define DBGFLAGS_LINENUM		0x00000400
#define DBGFLAGS_SENSITIVE		0x00000800
#define DBGFLAGS_DBGOPEN		0x00001000
#define DBGFLAGS_SRCOPEN		0x00002000
#define DBGFLAGS_NOTUSED1		0x00004000
#define DBGFLAGS_NOTUSED2		0x00008000
#define DBGFLAGS_SCREEN			0x00010000	/* The debug character mode screen is currently displayed */
#define DBGFLAGS_KEYIN			0x00020000
#define DBGFLAGS_TRAP			0x00040000
#define DBGFLAGS_NOTUSED3		0x00080000
#define DBGFLAGS_INITPRF		0x00100000
#define DBGFLAGS_INITGBL		0x00200000
#define DBGFLAGS_INITBRK		0x00400000
#define DBGFLAGS_EXTRAINFO		0x00800000
#define DBGFLAGS_MORE			0x01000000
#define DBGFLAGS_MAINTBREAK		0x02000000
#define DBGFLAGS_NOTUSED4		0x04000000
#define DBGFLAGS_NOTUSED5		0x08000000
#define DBGFLAGS_DDT			0x80000000
#define ERROR_WARNING			0x01
#define ERROR_FAILURE			0x02
/* note: keep break_permanent and break_ignore <= 0x80 */
#define BREAK_INITIAL			0x01
#define BREAK_PERMANENT			0x08
#define BREAK_TEMPORARY			0x10
#define BREAK_IGNORE			0x20
#define DEBUG_BRKMAX			32
#define DDT_DEBUG				0
#define MAXSNAME				33
#define MAXDLABEL				33
#define MAXPLABEL				13
#define FNCMAX					128
#define INCMAX					32
#define MTYPE_PRELOAD  0x01
#define MTYPE_CHAIN    0x02
#define MTYPE_LOADMOD  0x03
#define MTYPE_CLASS    0x04
#define VERB_CHAIN     0x09
#define VERB_DEBUG     0x11    /*  17 */
#define VERB_DELETE    0x12    /*  18 */
#define VERB_INSERT    0x1F    /*  31 */
#define VERB_READKG    0x23    /* 35 */
#define VERB_CLOCKCLIENT 0x24  /*  36 */
#define VERB_PACK      0x2F		/*  47 */
#define VERB_MAKE      0xE2    /* (88) 226 */
#define VERB_PACKLEN   0xEC    /* (88) 236 */
#define VERB_STOP      0x64    /* 100 */
#define VERB__CONCAT   0x3E     /* (88) 62 */
#define VERB_CLIENTGETFILE 0x41 /* 65 */
#define VERB_CLIENTPUTFILE 0x42 /* 66 */

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif



struct queuestruct;
/* struct pixmapstruct; */

/* function prototypes */

/* dbc.c */
extern INT main(INT, CHAR **, CHAR **);
extern INT mainentry(INT, CHAR **, CHAR **);

/* dbcctl.c */
extern void dbcinit(INT, CHAR **, CHAR **);
extern INT dbcgetbrkevt(void);
#if OS_WIN32
__declspec (noreturn) extern void dbcerrinfo(INT, UCHAR *, INT);
__declspec (noreturn) extern void dbcexit(INT);
__declspec (noreturn) extern void dbcerrjmp(void);
__declspec (noreturn) extern void dbcerror(INT num);
__declspec (noreturn) extern void dbcdeath(INT);
__declspec (noreturn) extern void badanswer(INT);
#else
extern void dbcerrinfo(INT, UCHAR *, INT)  __attribute__((__noreturn__));
extern void dbcexit(INT)  __attribute__((__noreturn__));
extern void dbcerrjmp(void)  __attribute__((__noreturn__));
extern void dbcerror(INT num)  __attribute__((__noreturn__));
extern void dbcdeath(INT)  __attribute__((__noreturn__));
extern void badanswer(INT)  __attribute__((__noreturn__));
#endif

extern void dbcsetevent(void);
extern INT dbcwait(INT *, INT);
extern void vpause(void);
extern INT vtrapsize(void);
extern void vtrapsave(void);
extern void vtraprestore(void);
extern void vtrap(void);
extern void vtrapclr(void);
extern void trapclearall(void);
extern void trapclearmod(INT);
extern void disable(void);
extern void enable(void);
extern INT getdisable(void);
extern INT geterrmsg(UCHAR *, INT *, INT);
extern void errorexec(void);
extern void actionexec(void);
extern void resettrapmap(void);
extern INT ctlimageopen(INT, INT, INT, INT, INT, INT *);
extern INT ctlqueueopen(INT, INT, INT, INT, INT *);
extern void ctlclosemod(INT);
extern void dbcctl(INT);
extern void buildDXAboutInfoString(void);
#if OS_UNIX
extern void resetKaTimer(void);
#endif

/* dbcctl2.c */
extern void logUntrappedError(CHAR * errormsg, INT errornum);
#if OS_UNIX && defined(Linux)
extern void logVbcode(UCHAR code, INT pcount);
extern void logVxcode(UCHAR code);
#endif


/* dbciex.c */
extern void dbciex(void);
extern INT vstop(UCHAR *, INT);
extern void getaction(INT **, INT *);
extern void dbgaction(INT);
extern void dbcshutdown(void);
extern INT popreturnstack(void);
extern INT pushreturnstack(INT);
extern INT returnstackcount(void);
extern INT returnstackpgm(INT);
extern INT returnstackdata(INT);
extern INT returnstackpcount(INT);
extern INT returnstackfirstparmadr(INT);
extern INT returnstackparmadr(INT);
extern void setreturnstackparmadr(INT, INT);
extern void resetreturnstackparmadr(INT);

/* dbcldr.c*/

typedef struct datamodstruct {
	INT datamod;		/* unique data module number */
	INT pgmxmod;		/* program module (array offset to pgmtabptr) */
	INT nextmod;		/* pointer to next data instance or class parent module (array offset to datatabptr) */
	INT classmod;		/* pointer to linked list of classes */
	INT nextclass;		/* pointer to next class module */
	INT prevclass;		/* pointer to previous class module */
	INT makelabel;		/* make label (array offset to lmodptr)*/
	INT destroylabel;	/* destroy label */
	INT size;			/* data area size, not including the end-of-data byte */
	INT maxsize;		/* data area allocated size */
	/*
	 * label-external table (lextptr) size in USHORTs
	 * Each entry is TWO USHORTs
	 */
	INT lextsize;
	INT varsize;		/* class inherited variable pointers table size */
	CHAR **nameptr;		/* instance/class name (allocated 1st) */
	UCHAR **dataptr;	/* data (allocated 2nd) */
	UCHAR **ltabptr;	/* label table pointer (allocated 3rd) */
	/*
	 * Label-module table pointer (allocated 4th)
	 *
	 * This table has a INT entry for every label referenced.
	 * It corresponds to the fact that the 'unique program module number'
	 * cannot exceed INT_MAX. (PGMMOD.pgmmod)
	 * If it is local, then the entry in this table will be THIS module's unique number.
	 * (The PGMMOD indexed by pgmxmod)
	 * If it is external, it is marked as -1 (0xFFFFFFFF) so the runtime knows when
	 * it is called that it needs to be resolved.
	 */
	UCHAR **lmodptr;
	UCHAR **lextptr;	/* label-external table pointers (allocated 5th) */
	UCHAR **varptr;		/* class inherited variable pointers (allocated 6th) */
} DATAMOD;

typedef struct codeinfostruct {
	UCHAR **pgmptr;		/* program (allocated 5th) */
	/*
	 * External reference table pointers (allocated 4th)
	 * 1st byte of each entry is zero for EXTERNAL or one for METHOD
	 * Followed by null terminated label name
	 */
	UCHAR **xrefptr;
	UCHAR **xdefptr;	/* external definition table pointers (allocated 3rd) */
	CHAR **fileptr;		/* file name (allocated 2nd) */
	CHAR **nameptr;		/* program name (allocated 1st) */
	UCHAR version;		/* version number */
	UCHAR stdcmp;		/* standard compile */
	INT pgmsize;		/* size of program module */
	INT xdefsize;		/* external definition table size */
	INT pcntsize;		/* size of pcount references (2,3 bytes) */
	INT codecnt;		/* in use count */
	INT htime;		/* high time the .dbc was created */
	INT ltime;		/* low time the .dbc was created */
} CODEINFO;

typedef struct pgmmodstruct {
	INT pgmmod;			/* unique program module number */
	/**
	 * 0 = unused
	 * 1 = preload
	 * 2 = chain
	 * 3 = loadmod
	 * 4 = class
	 */
	UCHAR typeflg;
	INT dataxmod;		/* data module (array offset to datatabptr) */
	INT xdefsize;		/* class external definition table size */
	/**
	 * Pointer to CODEINFO (allocated 1st)
	 * One and only one, not an array
	 */
	CODEINFO **codeptr;
	UCHAR **xdefptr;	/* class external definition table pointers (allocated 2nd) */
	UCHAR **xrefptr;	/* class external reference table pointers (allocated 3rd) */
} PGMMOD;

extern INT newmod(INT, CHAR *, CHAR *, INT *, INT);
extern INT vunload(INT, INT);
extern INT unloadclass(INT, INT);
extern void setpgmdata(void);
extern void vreturn(void);
extern void vretcnt(void);
extern UCHAR *getmdata(INT);
extern UCHAR *getmpgm(INT);
extern INT getpgmsize(INT);
extern INT getdatamod(INT);
extern INT getpgmmod(INT);
extern INT getmver(INT);
extern void getmtime(INT, INT *, INT *);
extern INT getmtype(INT);
extern CHAR *getdname(INT);
extern CHAR *getpname(INT);
extern CHAR *getmname(INT, INT, INT);
extern void findmname(CHAR *, INT *, INT *);
extern struct daqb *getmdev(void);
extern void chgpcnt(INT);
extern void chgmethod(INT, INT);
extern INT getpcnt(INT, INT *, INT *);
extern void putpcnt(INT, INT, INT);
extern void setpcnt(INT, INT);
extern void movelabel(INT, INT, INT);
extern INT vgetlabel(INT);
extern INT vtestlabel(INT);
extern void clearlabel(INT);
extern void chkavar(UCHAR *, INT);
extern void devchn(void);
extern INT gethhll(void);
extern UCHAR *getvar(INT);
extern void getvarx(UCHAR *, INT *, int *, int *offptr);
extern DAVB *getdavb(void);
extern DAVB *aligndavb(UCHAR *, INT *);
extern UCHAR *getlist(INT);
extern INT getlistx(INT, INT *, INT *);
extern INT skipvar(UCHAR *, INT);
extern void skipadr(void);
extern INT gettype(INT *, INT *, INT *);
extern UCHAR *findvar(INT, INT);
extern INT makevar(CHAR *, UCHAR *, INT);
extern CHAR *getmodules(INT);
extern void getlastvar(INT *, INT *);
extern void setlastvar(INT, INT);
extern INT getglobal(INT, CHAR **, INT *);
extern PGMMOD** getpgmtabptr(void);
extern DATAMOD** getdatatabptr(void);
extern INT getpgmmax(void);
extern CHAR* getHttpErrorMsg(void);

/* dbcmov.c */
extern void vmove(INT);
extern void movevar(UCHAR *, UCHAR *);
extern void vmoveexp(INT);
extern void vsetop(INT);
extern void vnumop(INT);
extern void vnummove(INT);
extern void vedit(INT);

/* dbcmth.c */
extern void vmathexp(INT);
extern void mathop(INT, UCHAR *, UCHAR *, UCHAR *);
extern void vgetsize(UCHAR *, INT *, INT *);
extern void arraymath(INT, UCHAR *, UCHAR *, UCHAR *);

/* dbcdio.c */
extern void vopen(void);
extern void vprep(void);
extern void vclos(void);
extern void vfilepi(void);
extern void vread(INT);
extern void vwrite(void);
extern void vdiotwo(void);
extern void vflush(void);
extern void filepi0(void);
extern void vunlock(INT);
extern void verase(void);
extern void vrename(void);
extern INT recgetv(UCHAR *, INT, UCHAR *, INT *, INT);
extern INT recputv(UCHAR *, INT, UCHAR *, UINT *, UINT *, UINT, UCHAR *);
extern void dioflag(INT);
extern void dioclosemod(INT);
extern void diorenmod(INT, INT);
extern CHAR *diofilename(INT);
extern CHAR *dioindexname(INT);

/* dbckds.c */
extern INT kdsinit(void);
extern void kdsexit(void);
extern void kdschain(void);
extern void vkyds(void);
extern INT chkcond(INT);
extern void dbckds(INT);
extern void vscrnsave(INT);
extern void vscrnrest(INT);
extern void vbeep(void);
extern void kdssound(INT, INT);
extern void kdscmd(INT32);
extern void kdsdsp(CHAR *);
extern INT kdspause(INT);

/* dbcprt.c */
extern void vsplopen(void);
extern void vsplclose(void);
extern void vprint(void);
extern void vrelease(void);
extern void vsplopt(void);
extern void vnoeject(void);
extern void vgetprintinfo(INT flag);
extern void prtflag(INT);
extern void prtclosemod(INT);
extern void prtrenmod(INT, INT);

/* dbccom.c */
extern void vcom(void);
extern void comevents(INT, INT *, INT *);
extern void comflag(INT);
extern void comclosemod(INT);
extern void comrenmod(INT, INT);

/* dbcmsc.c */
extern void vnformat(void);
extern void vsformat(void);
extern void vsearch(void);
extern void vcount(void);
extern void vbrpf(void);
extern INT typ(UCHAR *);
extern void chk1011(INT, UCHAR *, UCHAR *, UCHAR *);
extern void vscan(void);
extern void vpack(INT);
extern void vunpack(void);
extern void vrep(void);
extern void vformat(UCHAR *adr1, UCHAR *adr2, UCHAR *adr3);
extern void vunpacklist(void);
extern void vsql(INT);
extern UCHAR *getint(UCHAR *);
extern void nvmkform(UCHAR *, UCHAR *);
extern INT nvtoi(UCHAR *);
extern OFFSET nvtooff(UCHAR *);
extern DOUBLE64 nvtof(UCHAR *);
extern void itonv(INT, UCHAR *);
extern void cvtoname(UCHAR *);
extern void cvtodbcbuf(UCHAR *);
extern void scanvar(UCHAR *);
extern void setnullvar(UCHAR *, INT);
extern int atoi_internal(const char *);

/* dbcdbg.c */
extern void dbginit(INT, CHAR *);
extern void dbgexit(void);
extern void vdebug(INT);
extern void dbgconsole(CHAR *s, INT msglength);
extern INT appendfplpplclause(CHAR* prefix);
extern void dbgerror(INT, CHAR *);
extern CHAR* dbggetfiletypedesc(CHAR *prefix, UCHAR *adr);
extern CHAR* dbggetspecialtypedesc(CHAR *prefix, UCHAR *adr);
extern INT dbgopendbg(INT);
extern void dbgsetbrk(int*);
extern void dbgsetmem(void);
extern INT dbgstrcmp(CHAR *, CHAR *, INT);
extern void dbgtestcall(void);
extern void dbgtestreturn(void);

/* dbcddt.c */
extern void ddtdebug(INT op);
extern void ddtinit(CHAR *);
extern void ddtinitserver(CHAR *);
extern int isddtconnected(void);
extern void setddtconnected(int);
extern void convertCounterBPToRealBP(INT, int);

/* dbgldr.c */

/**
 * This struct is the information passed from dbgldr to dbcdbg.
 * It represents a dbg file.
 * The classnum is not cached, it is calculated each time
 * because there can be more than one class in a dbg file.
 */
typedef struct {
	UCHAR **stabptr;	/* double pointer to Source table */
	UCHAR **dtabptr;	/* double pointer to Data table */
	UCHAR **ltabptr;	/* double pointer to Label table */
	UCHAR **ptabptr;	/* double pointer to Program lines table */
	UCHAR **rtabptr;	/* double pointer to Routines table */
	UCHAR **ctabptr;	/* double pointer to Classes table */
	UCHAR **ntabptr;	/* double pointer to Names table */
	INT stabsize;		/* entries in the Source table */
	INT dtabsize;		/* non-class D records used */
	INT ltabsize;		/* entries in the Label table */
	INT ptabsize;		/* entries in the Program table */
	INT rtabsize;		/* entries in the Routine table */
	INT ctabsize;		/* entries in the Classes table */
	INT classnum;		/* if the module requested is a class, index into ctable, else -1 */
	INT realdsize;		/* actual number of entries in the Data table */
} DBGINFOSTRUCT;

extern INT getdbg(INT module, DBGINFOSTRUCT *dbgfinfo);

/* dbcsrv.c */
extern int srvinit(void);
extern void srvexit(void);

#ifdef DX_SINGLEUSER
#if OS_UNIX
__attribute__((unused)) static int srvgetserver(char *filename, int path) { return 0; }
__attribute__((unused)) static int srvgetprimary() { return 0; }
__attribute__((unused)) static int srvgeterror(char *msg, int msglength) { return 0; }
#else
static int srvgetserver(char *filename, int path) { return 0; }
static int srvgetprimary() { return 0; }
static int srvgeterror(char *msg, int msglength) { return 0; }
#endif
#else
extern int srvgetserver(char *, int);
extern int srvgetprimary(void);
extern int srvgeterror(char *msg, int msglength);
#endif
//#ifndef DX_SINGLEUSER
//extern int srvgetserver(char *, int);
//extern int srvgetprimary(void);
//extern int srvgeterror(char *msg, int msglength);
//#endif

/* dbcsys.c */
extern INT sysinit(INT, CHAR **, CHAR **);
extern void sysexit(void);
extern void vexecute(void);
extern void vrollout(void);
extern INT utilexec(CHAR *);
extern void vconsole(void);
extern void vclock(void);
extern void vclockClient(void);

/* dbccsf.c */
extern void vccall(void);
extern void vcverb(void);
extern INT dbcgetflags(void);
extern void dbcseteos(INT);
extern void dbcsetequal(INT);
extern void dbcsetless(INT);
extern void dbcsetover(INT);
extern INT dbcgetparm(UCHAR **);
extern void dbcresetparm(void);
extern UCHAR *dbcgetdata(INT);

/* dbctcx.c */
extern SOCKET dbctcxconnect(INT, CHAR *,void (*recvcallback)(INT));
extern SOCKET dbctcxconnectserver(INT channel, CHAR *port,void (*recvcallback)(INT), int timeout);
extern void dbctcxdisconnect(INT channel);
extern void dbctcxflushincoming(INT channel);
extern void dbctcxputtag(INT, CHAR *);
extern void dbctcxputattr(INT, CHAR *, CHAR *);
extern void dbctcxputsub(INT, CHAR *);
extern void dbctcxput(INT);
extern INT dbctcxget(INT);
extern CHAR *dbctcxgettag(INT);
extern CHAR *dbctcxgetnextattr(INT);
extern CHAR *dbctcxgetattrvalue(INT);
extern CHAR *dbctcxgetnextchild(INT);
extern CHAR *dbctcxerror(void);
extern void dbctcxprocessbytes(UCHAR *mem, INT len);

#if OS_WIN32
#define fcvt(a,b,c,d) _fcvt(a,b,c,d)
#endif

#define getbyte() (pgm[pcount++])

#if OS_WIN32 & defined(I386)
#define setfp(p) ((*(p)<128) ? (INT)(*(p)=(UCHAR)fp) : (INT)(*(USHORT *)(p+1)=(USHORT)fp))
#define setfplp(p) ((*(p)<128) ? (INT)(*(p)=(UCHAR)fp, *(p+1)=(UCHAR)lp) : (INT)(*(USHORT *)((p+1))=(USHORT)fp, *(USHORT *)((p+3))=(USHORT)lp))
#define llhh(p) (INT)(*(USHORT *)(p))
#define hhll(p) (((INT)(*(p))<<8) + *(p+1))
#define hhmmll(p) ((INT)(*(p))<<16) + hhll(p+1)
#define llmmhh(p) ((INT)(*(p+2))<<16) + llhh(p)
#else
#define setfp(p) ((*(p)<128) ? (INT)(*(p)=(UCHAR)fp) : (INT)(*(p+1)=(UCHAR)fp, *(p+2)=(UCHAR)(fp>>8)))
#define setfplp(p) ((*(p)<128) ? (INT)(*(p)=(UCHAR)fp, *(p+1)=(UCHAR)lp) : (INT)(*(p+1)=(UCHAR)fp, *(p+2)=(UCHAR)(fp>>8), *(p+3)=(UCHAR)lp, *(p+4)=(UCHAR)(lp>>8)))
#define llhh(p) (((INT)(*(p+1))<<8) + *(p))
#define hhll(p) (((INT)(*(p))<<8) + *(p+1))
#define hhmmll(p) ((INT)(*(p))<<16) + hhll(p+1)
#define llmmhh(p) ((INT)(*(p+2))<<16) + llhh(p)
#endif

/* types used by dbcdbg and dbcddt */
typedef struct {	/* data table */
	INT nptr;		/* offset in name table of data variable name */
	INT rtidx;		/* index into routine table */
	INT offset;		/* offset in data area of data variable */
} DATADEF;

typedef struct {	/* label table */
	INT nptr;		/* offset in name table of label name */
	INT stidx;		/* index into source table */
	INT linenum;	/* file position in source file */
} LABELDEF;

typedef struct {	/* program table */
	INT pcnt;		/* pcount of source */
	INT stidx;		/* index into source table */
	/**
	 * line number of source, zero-based
	 */
	INT linenum;
} PGMDEF;

typedef struct {		/* source file information table */
	INT nametableptr;	/* offset in name table of file name */
	INT linecnt;		/* lines in file */
	OFFSET size;		/* file size */
} SRCDEF;

typedef struct {
	INT mod;		/* module number of break point */
	INT ptidx;		/* index to program line */
	INT pcnt;		/* pcount of break point */
	UCHAR verb;		/* verb that was replaced */
	UCHAR type;		/* type of break point */
} BREAKDEF;

typedef struct {
	INT mod;		/* module number of break point */
	CHAR module[32];/* module name of breakpoint */
	INT ptidx;		/* index to program line */
	INT pcnt;		/* pcount of break point */
	INT line;		/* zero based line number */
	INT count;		/* count of executions until break happens */
	UCHAR verb;		/* verb that was replaced */
} COUNTERBREAKDEF;

typedef struct {	/* routine table */
	INT nptr;		/* offset in name table of routine name */
	INT dtidx;	/* index into data table of starting data variable */
	INT dtcnt;	/* count of data variables */
	INT spcnt;	/* starting pcount of source */
	INT epcnt;	/* ending pcount of source */
	INT class;	/* corresponding class */
} RTNDEF;

typedef struct {	/* class table */
	INT nptr;		/* offset in name table of class name */
	INT dtidx;	/* index into data table of starting data variable */
	INT dtcnt;	/* count of data variables */
	INT spcnt;	/* starting pcount of source */
	INT epcnt;	/* ending pcount of source */
} CLASSDEF;

/* fields used by dbcdbg and dbcddt */
EXTERN DATADEF *datatable;
EXTERN BREAKDEF dbgbrk[DEBUG_BRKMAX];
EXTERN COUNTERBREAKDEF dbgCounterbrk[DEBUG_BRKMAX];
EXTERN CHAR dbgerrorstring[256];
EXTERN INT dbgpgmmod, dbgdatamod;
EXTERN INT dbgpcount;
EXTERN INT dtsize;
EXTERN INT ctsize;
EXTERN INT events[2];
EXTERN CHAR *nametable;
EXTERN PGMDEF *pgmtable;
EXTERN INT ptsize;
EXTERN RTNDEF *routinetable;
EXTERN CLASSDEF *classtable;
EXTERN INT saveretptr;
EXTERN SRCDEF *sourcetable;