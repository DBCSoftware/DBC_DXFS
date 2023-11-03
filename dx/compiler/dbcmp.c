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

#define INC_STDLIB
#define INC_STDIO
#define INC_STRING
#define INC_CTYPE
#define INC_TIME
#define INC_SIGNAL
#define INC_SETJMP
#define INC_LIMITS
#include "includes.h"
#include "release.h"
#include "arg.h"
#include "base.h"
#include "dbccfg.h"
#include "fio.h"
#define DBCMP
#include "dbcmp.h"

#if OS_UNIX
#include <sys/types.h>
#endif

#ifdef SPECIAL
static INT i2, i3;
static CHAR license[11];
static UCHAR specheader[] = {
	0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
	0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
	0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
	0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53,
	0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d,
	0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71,
	0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b,
	0x01, 0x02, 0x03
};
#endif

#if OS_WIN32
#define DIRSEPCHAR		'\\'
#endif
#if OS_UNIX
#define DIRSEPCHAR		'/'
#endif

#define PATHSEPCHAR			';'

#define MAKECLITMAXLEN 40  /* largest literal length to check for reuse */

#define MAXHDR 256	/* maximum size of header string -h=<string> */
#define MAXNAM 256	/* maximum size for srcname, dbcname, prtname strings */
#define MAXSRT 256	/* maximum size for sort command -r=<sort utility> */
#define MAXXLT 256	/* maximum size for translate table filename, table -t=<translate table> */
#define MAXPRM 512	/* maximum size of a command line parameter */
#define MAXPGLENGTH 51 /* maximum formatted page length */
#define MAXPGWIDTH 132 /* maximum formatted output page width */
#define MAXINC 32			/* maximum number of include levels */

#define XREFBUFSIZE 256
#define XMLTAG_STATISTICS "statistics"

static UINT mainbufcnt = 4;
static UINT mainbufsize = MAINBUFINCREMENT;
static UCHAR **mainbufpptr;
static INT mainbufhandle;
static OFFSET mainfilepos;
static UCHAR **memreserve1;
static UCHAR **memreserve2;
static CHAR mainwrkname[13];		/* temporary work file name (main code buf overflow) */

static INT databufhandle;		/* handle of temporary work file */
static CHAR datawrkname[13];		/* temporary work file name (main data buf overflow) */
static UCHAR **databuf;			/* main data area buffer, grows by DATABUFINCREMENT, overflows to work file */
static UINT databufsize = DATABUFINCREMENT;	/* size of databuf in bytes */
static UINT databufcnt = 7;		/* number of bytes currently written to databuf */
static OFFSET datafilepos;			/* current file offset within data buffer overflow file */

#define DBGBUFSIZE 180
static UCHAR dbgbuf[DBGBUFSIZE + 4];
static INT sreccnt = 0;	/* number of 'S' dbg recs written */
static INT dreccnt = 0;	/* number of 'D' dbg recs written */
static INT lreccnt = 0;	/* number of 'L' dbg recs written */
static INT preccnt = 0;	/* number of 'P' dbg recs written */
static INT rreccnt = 0;	/* number of 'R' dbg recs written */
static INT creccnt = 0;	/* number of 'C' dbg recs written */
static INT classdreccnt = 0;		/* number of 'D' dbg recs written in current class */
static OFFSET classrecpos;	/* file offset of the last 'C' record written */
CHAR internalError8Message[MAXINTERNALERROR8SIZE];

/* SYMBOL TABLE data structure definitions
 *
 *	how they are related:
 *		h = symhashfunct(symbol)
 *		symtableptr = symtablebase[h] + symtableoffset[h]
 *
 *		symtableptr will now point to a SYMINFO structure
 *		that may reference a matching symbol name, or
 *		symtableptr will equal NULL if no symbol matches
 *		in existing symbol table.
 *
 *		The table of SYMINFO structures may also be
 *		referenced by a number ranging from 0 to
 *		(MAXSYMBLOCK * SYMBLOCKSIZE - 1).  The first
 *		SYMBLOCKSHIFT bits of this number equals the offset
 *		within the SYMINFO structure block referenced by
 *		shifting the value SYMBLOCKSHIFT bits to the right.
 *
 *		Storage for the actual symbol names exists in
 *		the growing table symname.  A linked "overflow"
 *		list is maintained for symbols that hash to the
 *		same name (see nexthash member of syminfo struct).
 *		A small subset of that linked list is also maintained
 *		for symbols with same name but different scope
 *		(see nextscope member of syminfo struct).
 */

/*
 * the following value should be prime (not close to a power of two) for performance
 */
#define SYMHASHSIZE 8971		/* hash table size */

typedef struct syminfostruct {
	UCHAR type;					/* symtype value (TYPE_xxxx) */
	UCHAR rlevel;					/* routine level for variable name visibility */
	USHORT routineid;				/* routine level id number for variables */
	USHORT classid;				/* class identifier, 0 means symbol defined outside of class */
	USHORT len;					/* symbol length */
	CHAR *nameptr;					/* pointer to actual symbol name string */
	union {
		USHORT exelabelref;			/* exelabel table reference */
		USHORT classflag;			/* either CLASS_DEFINITION or CLASS_DECLARATION */
	} u1;
	union {
		INT dataadr;				/* data address */
		UINT simpleref;			/* simple program label table reference */
		CHAR *modname;				/* pointer to name of module that a class is defined in */
	} u2;
	struct syminfostruct *nexthash;	/* next symbol that hashed to same value */
	struct syminfostruct *next;		/* next symbol with same name but different scope */
} SYMINFO;

#define MAXSYMBLOCK 512		/* maximum number of SYMINFO structure blocks */
#define SYMBLOCKSHIFT 10		/* number of shifts left to get SYMBLOCKSIZE (see below) */
#define SYMBLOCKSIZE (1 << SYMBLOCKSHIFT)	/* size of SYMINFO structure blocks (structure count) */
#define SYMNAMEBLKSIZE 10240	/* size of symbol name storage blocks */

/*
 * sequential list of base addresses of growing SYMINFO structure blocks
 * Each entry in this 512 entry array points to the first SYMINFO
 * in an array of up to 1024
 */
static SYMINFO *symblklist[MAXSYMBLOCK];
/*
 * Reference into symblklist[], current SYMINFO structure block being added to
 */
static INT cursymblk;
static INT cursyminfo;					/* number of current SYMINFO structures allocated (in all blocks) */
static INT symblksize;					/* current SYMINFO structure block size (# of SYMINFO's in current block) */
/*
 * Symbol table base pointer (gives address of a block of SYMINFO structures)
 */
static SYMINFO *symtablebase[SYMHASHSIZE];
static INT symnamesize;					/* symbol name table size */
static CHAR *symname;					/* large symbol name table */
static USHORT symtableoffset[SYMHASHSIZE];	/* offset from base of SYMINFO structure in symbol table */

#define EXTERNALDEFINCREMENT 1024			/* size of external definition table incremental increases */

#define XML_ERROR		1
#define XML_WARNING		2
#define XML_FATAL		3
#define XML_TASK_TODO	4
#define XML_TASK_CODE   5
#define XML_TASK_FIXME	6

static UINT ashapecnt;			/* current array shape storage area count */
static UINT ashapemax;			/* current array shape storage area maximum */
static UCHAR **ashapetable;		/* pointer to pointer of array shape storage area */
static UCHAR createtime[20];		/* time .dbc was created */
static INT curfile;				/* current source file handle */
static INT dbcfilenum;			/* fio handle for .dbc file */
static OFFSET dbcfilepos = 0;		/* file position withing .dbc file */
static CHAR dbcmperrstr[256];
static CHAR dbcname[MAXNAM];		/* .dbc file name */
static INT dbgfilenum;			/* rio handle for .dbg file */
static OFFSET dbgoff;				/* offset in debug file */
static INT dspstart;			/* program area offset to start displaying */
static INT errcnt;				/* number of errors */
static INT **exelabeltable; 	 	/* pointer to pointer to executable label table */
static USHORT exelabelnext;		/* count of executable label table entries */
static UINT exelabelsize;		/* current allocation in bytes of executable label */
static UCHAR fmtflag;			/* formatted output flag */
static UCHAR nofixupflg; 		/* set to true if -x option to force label table is used */
static CHAR savname[MAXINC][MAXNAM];	/* save the current include files source name */
static CHAR srcname[MAXNAM];		/* source file name */
static CHAR prtname[MAXNAM];		/* print file name */
static CHAR parm[MAXPRM];		/* parameter from command line */
static UCHAR prtflag;			/* direct output to print file */
static UCHAR xlateflg;			/* source line translation option */
static UCHAR xlate[MAXXLT];		/* translate file name, then translate table */
static INT pagelen = MAXPGLENGTH;	/* formatted output page length */
static INT pagewdth = MAXPGWIDTH;	/* formatted output page width */
static CHAR header[MAXHDR];		/* formatted output header message */
static CHAR sortcmd[MAXSRT];		/* sort file name */
static UCHAR incopt;			/* 1, 2, 3, 4 special processing for include file names */
static FILE *outfile;			/* output file */
static INT prtlinenum;			/* formatted output print line number */
static UCHAR xrefflg;			/* cross reference flag */
static INT xrefhandle;			/* cross reference work file handle */
static UCHAR libcnt;			/* number of library files */
static CHAR libname[3][MAXNAM];	/* library file names */
static UINT totlines;			/* total number of lines compiled */
static UINT externalDefinitionsTableSizeInBytes;
static UINT externalReferencesTableSizeInBytes;
static UINT externalDefinitionsTableCountOfEntries;
static UINT externalReferencesTableCountOfEntries;
static USHORT executionLabelTableCount;
UINT linecounter[MAXINC + 1]; 	/* linecounter for current include level */
static INT filetable[MAXINC + 1];	/* include file handle for each level */
INT inclevel = -1;				/* current include level  0 = main program */
static INT incnum = 0;			/* current include number */
static UINT xdefsize = 4;		/* external definition table size used */
static UINT xdefalloc;			/* external definition table size allocated */
static UCHAR **xdeftable;		/* pointer to external definition table */
/**
 * simple fixup table, entries are 8 bytes
 * first 4 bytes are simple label number (UINT)
 * last 4 bytes are pgm offset to be fixed up
 */
static UCHAR **simplefixuptable;
static INT simplefixupnext;		/* byte offset of next fixup entry */
static INT simplefixupsize;		/* current allocation in bytes of fixup table */
static UINT simplelabelnext;	/* count of simple label table entries */
static UINT simplelabelsize;	/* current allocation in bytes of simple label table */
static INT **simplelabeltable;	/* simple label table, entries are INTs */
static UCHAR **xreftable;		/* external/method label reference table */
static UINT xrefsize = 4;		/* current count of bytes used in xreftable */
static UINT xrefalloc;			/* current count of bytes allocated in xreftable */
static INT openmode = RIO_M_ERO;	/* src file open mode: default exclusive read only opens */
static UCHAR linesav[LINESIZE + 1]; /* used to save the string line */
static INT linecntsav;			/* used to save the integer linecnt */
static CHAR xrefwrkname[13];		/* temporary work file name (cross ref) */
static UCHAR removeflg;
static CHAR xmlname[MAXNAM];
static INT xmlfilenum;
static OFFSET xmlfilepos;
#if OS_WIN32
static jmp_buf dlljmp;			/* stack address to return to if dll death error */
#endif
static jmp_buf xmljmp;			/* stack address to return to if xml death error */

#define CLITHASHSIZE 2999
#define CLITTABLEINCREMENT 4096
#define NLITREUSETABLESIZE 111
static UCHAR **clittable;			/* character literal reuse table */
static INT clithashtab[CLITHASHSIZE];	/* hash table used to reference clittable[] */
static INT clittablesize;			/* current size of bytes in use within clittable[] */
static INT clittablealloc;			/* current size of bytes allocated by clittable[] */
static INT nlitreuse[NLITREUSETABLESIZE];		/* holds data address of numeric literals -9 to 100 for reuse */

#define EXTENDEXETABLE() \
	{ \
		if (exelabelnext * sizeof(INT) >= exelabelsize) \
			dbcmem((UCHAR ***) &exelabeltable, exelabelsize += 512 * sizeof(INT));\
	}

#define EXTENDSIMPLETABLE() \
	{ \
		if (simplelabelnext * sizeof(INT) >= simplelabelsize) \
			dbcmem((UCHAR ***) &simplelabeltable, simplelabelsize += 512 * sizeof(INT));\
	}

#define EXTENDFIXUPTABLE() \
	{ \
		if (simplefixupnext >= simplefixupsize) { \
			dbcmem(&simplefixuptable, (UINT) (simplefixupsize += 3072)); \
		} \
	}

/* defines/variables used for CLASS support (DB/C OOP) */
#define CLASSHEADERSIZE 107		/* size (in bytes) of a class header (including tag byte, segment size, classname, parentname, modname, make/destroy labels & length of class data buffer */
#define METHODDEFINCREMENT 1024	/* size of method definition table incremental increases */
#define METHODREFINCREMENT 512	/* size of method reference table incremental increases */
#define DATAMBRINCREMENT 1024		/* size of data member name table incremental increases */
#define CLASSEXEINCREMENT 1536	/* size of class execution label table incremental increases */
static INT maindataadr;			/* save main data address count while compiling a class */
static UINT classjmplabel;		/* holds undefined jump label for goto around class code in main line */
static UCHAR **classhdr;			/* holds 4 byte segment tag, class name, parent name, parent mod, make label, destroy label */
static UINT classhdrsize;		/* holds size in bytes of above class header */
static INT clsdatabufout;		/* holds size (in bytes) of portion of class data buffer written to .DBC file */
static INT clsdatabufcnt;		/* holds size (in bytes) of class data buffer (not including portion written to .DBC file) */
static UCHAR **clsdatabuf;		/* current class data buffer */
static OFFSET clshdrfilepos;		/* class header file offset within .DBC output file */
static UINT mtddefsize;			/* method definition table size used */
static UINT mtddefalloc;			/* method definition table size allocated */
static UCHAR **mtddeftable;		/* pointer to method definition table */
static UINT clsxrefsize;			/* method definition table size used */
static UINT clsxrefalloc;		/* method definition table size allocated */
static UCHAR **clsxreftable;		/* pointer to method definition table */
static UCHAR **datambrtable;		/* data member name table */
static UINT datambrsize;			/* data member name table size used */
static UINT datambralloc;		/* data member name table size allocated */
static USHORT saveexelblnext;		/* save storage for count of executable label table entries */
static UINT saveexelblsize;		/* save storage for current allocation in bytes of executable label */
static INT **saveexelbltable;  	/* save storage for pointer to pointer to executable label table */
static UINT saverlevel;			/* current routine level */
static UINT saverlevelid[RLEVELMAX];	/* identifiers for all 16 routine levels */
static UINT saverlevelidcnt;		/* id number for each routine */
static UCHAR **saveclittable;		/* save character literal reuse table */
static INT classclithash[CLITHASHSIZE];	/* class hash table used to reference clittable[] */
static INT saveclitsize;		/* save current size of bytes in use within clittable[] */
static INT saveclitalloc;		/* save current size of bytes allocated by clittable[] */
static INT classnlitreuse[NLITREUSETABLESIZE];		/* holds data address of numeric literals -9 to 100 for class reuse */

#define REF_EXTERNAL 0
#define REF_METHOD 1

/* local function prototypes */
static INT addclsxref(CHAR *, INT, INT);
static void adddatambr(CHAR *, INT);
static void addmtddef(CHAR *, INT);
static void addxdef(CHAR *, INT);
static INT addxref(CHAR *, INT, INT);
static INT dbcmpinit(void);
static INT dbcmpmain(INT argc, CHAR **argv);
static void dbgclassrec(CHAR *);
static void dbgdatarec(INT, CHAR *);
static void dbgendclassrec(void);
static void dbgeofrec(INT);
static void dbgsrcrec(CHAR *);
static void freeallalloc(void);
static SYMINFO *getnextsymentry(CHAR *, USHORT);
static void newpage(void);
static void outputc(INT);
static void outputinc(void);
static void outputs(CHAR *);
static void outputns(INT, CHAR *);
static INT parsecmdline(void);
static void processWarningArgument(INT parmlen);
static void putdatalmh(INT);
static void putdataout(void);
static void putclsdata1(UINT);
static void putclsdatahhll(UINT);
static void putclsdatallhh(UINT);
static void putclsdata(UCHAR *, INT);
static void putclsdatalmh(INT);
static void putclsdataout(void);
static void quitsig(INT);
static void savemain(void);
static void startdbc(void);
static void stats(void);
static UINT symhashfunct(CHAR *, USHORT*);
static INT tempfile(CHAR *, INT);
static void usage(void);
static void writedbc(void);
static void writedbg(void);
static void xmlmessage(INT type, INT linenum, CHAR *filename, CHAR *msg);
static void xmlstats(void);
static void xrefout(CHAR *, UINT);
static void xrefdsp(void);


#if OS_WIN32
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			break;
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
			break;
		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
    //UNREFERENCED_PARAMETER(hinstDLL);
	//UNREFERENCED_PARAMETER(lpvReserved);
}

INT dbcmp(INT argc, CHAR **argv)
{
	INT i1;
	CHAR *argvwork[50];

	argvwork[0] = "dbcmp";
	for (i1 = 1; i1 <= argc && i1 + 1 < (INT) (sizeof(argvwork) / sizeof(*argvwork)); i1++)
		argvwork[i1] = argv[i1 - 1];
	if (i1 <= argc) {
		strcpy(dbcmperrstr, "too many arguments");
		return -1;
	}
	argvwork[++argc] = NULL;

	if (setjmp(dlljmp)) return -1;
	dspflags = 0;
	i1 = dbcmpmain(argc, argvwork);
	return i1;
}
#endif

/* MAIN */
INT main(INT argc, CHAR **argv)
{
	INT i1;

	dspflags = DSPFLAG_CONSOLE;
	signal(SIGINT, quitsig);
	i1 = dbcmpmain(argc, argv);
	cfgexit();
	exit(i1);
	return 0;
}

static INT dbcmpmain(INT argc, CHAR **argv)
{
	INT i1;
	size_t i2;
	CHAR cfgname[MAX_NAMESIZE], work[300], *ptr1, *ptr2, **pptr;
	FIOPARMS parms;

	arginit(argc, argv, &i1);
	if (!i1 || !(dspflags & DSPFLAG_CONSOLE)) dspsilent();

	if (dbcmpinit() == RC_ERROR) death(DEATH_NOMEM);

#if defined(SPECIAL)
	/* checksum 90 character header */
	for (i1 = i2 = i3 = 0; i1 < 90; i1++) {
		if (!specheader[i1]) break;
		if (specheader[i1] & 0x15) i2 += specheader[i1];
		if (i1 & 0x06) i3 += (UCHAR)(i1 ^ specheader[i1]);
	}
	if (i1 != 90 || (i2 % 241) != (INT) specheader[91] || (i3 % 199) != (INT) specheader[92]) {
		death(DEATH_UNSTAMPED);
	}
	memcpy(license, &specheader[70], 10);
	license[10] = 0;
	for (i2 = 68; specheader[i2] == ' '; i2--);
	specheader[i2 + 1] = '\n';
	specheader[i2 + 2] = 0;
	outputs(specheader);
#endif

	/* initialize */
	if (meminit(16777216 /* 2^24 */ + 4194304 /* 2^22 */, 0, 256) == -1) death(DEATH_NOMEM);
	cfgname[0] = '\0';
	while (!argget(ARG_NEXT | ARG_IGNOREOPT, work, sizeof(work))) {
		if (work[0] == '-') {
			if (work[1] == '?') usage();
			if (toupper(work[1]) == 'C' && toupper(work[2]) == 'F' &&
			    toupper(work[3]) == 'G' && work[4] == '=') strcpy(cfgname, &work[5]);
		}
	}
	if (cfginit(cfgname, FALSE)) death(DEATH_BADCFGOTHER);
	if (prpinit(cfggetxml(), CFG_PREFIX "cfg")) death(DEATH_BADCFGFILE);
	ptr1 = fioinit(&parms, FALSE);
	if (ptr1 != NULL) death(DEATH_INITPARM);

	if (parsecmdline()) return 1;  /* parse the command line */
	if (dspflags & DSPFLAG_XML) {
		if (setjmp(xmljmp)) {
			if (xmlfilenum > 0) {
				fiowrite(xmlfilenum, xmlfilepos, (UCHAR *) "</compile>", 10);
				fioclose(xmlfilenum);
			}
			return 1;
		}
		if (xmlname[0]) xmlfilenum = fioopen(xmlname, FIO_M_PRP | FIO_P_WRK);
		else xmlfilenum = ERR_BADNM;
		if (xmlfilenum < 0) death(DEATH_BADXMLFILE);
		if (fiowrite(xmlfilenum, (OFFSET) 0, (UCHAR *) "<compile>", 9) < 0) death(DEATH_BADXMLWRITE);
		xmlfilepos = 9;
	}
	i1 = srcopen(srcname);
	if (i1 == 1) death(DEATH_SRCFILENOTFOUND);
	else if (i1) death(DEATH_BADSRCFILE);
	if (prtflag) {	/* initialize listing file */
		if (miofixname(prtname, ".prt", FIXNAME_EXT_ADD | FIXNAME_PAR_DBCVOL | FIXNAME_PAR_MEMBER) < 0) death(DEATH_BADPRTFILE);
		miogetname(&ptr1, &ptr2);
		if (ptr1[0] && !fiocvtvol(ptr1, &pptr)) {
			ptr1 = *pptr;
			for (i1 = 0; ptr1[i1] && ptr1[i1] != ';'; i1++);
			if (i1) {
				memcpy(linesav, ptr1, (size_t) i1);
				linesav[i1] = 0;
				fioaslashx((CHAR *) linesav);
				i2 = strlen((CHAR *) linesav);
				memmove(&prtname[i2], prtname, strlen(prtname) + 1);
				memcpy(prtname, linesav, i2);
				linesav[0] = '\0';
			}
		}
		if ((outfile = fopen(prtname, "w")) == NULL) death(DEATH_BADPRTFILE);
	}
	if (xrefflg) {  /* create cross reference work file */
		xrefhandle = tempfile(xrefwrkname, 1);
	}
	dbcfilenum = fioopen(dbcname, FIO_M_PRP | FIO_P_DBC);
	if (dbcfilenum < 0) death(DEATH_BADDBCFILE);
	if (dbgflg) {
		miofixname(dbcname, ".dbg", FIXNAME_EXT_ADD | FIXNAME_EXT_REPLACE);
		dbgfilenum = rioopen(dbcname, RIO_M_PRP | RIO_P_DBC | RIO_T_STD | RIO_UNC, 8, LINESIZE);
		if (dbgfilenum < 0) death(DEATH_BADDBGFILE);
		strcpy((CHAR *) dbgbuf, "000000000000000000000000000000000000000000000000000000");
		rioput(dbgfilenum, dbgbuf, (INT) strlen((CHAR *) dbgbuf));
		dbgsrcrec(srcname);
	}
	if ((memreserve1 = memalloc(512, 0)) == NULL) death(DEATH_NOMEM);
	if ((memreserve2 = memalloc(512, 0)) == NULL) death(DEATH_NOMEM);
	if ((databuf = memalloc((INT) databufsize, 0)) == NULL) death(DEATH_NOMEM);
	if ((mainbufpptr = memalloc((INT) mainbufsize, 0)) == NULL) death(DEATH_NOMEM);
	if (fmtflag) newpage();
	startdbc();
	compile();
	checknesting();
	writedbc();
	if (errcnt && removeflg) {
		if (dbgflg) riokill(dbgfilenum);
		fiokill(dbcfilenum);
	}
	else {
		if (dbgflg) writedbg();
		fioclose(dbcfilenum);
	}
	if (dspflags & DSPFLAG_XML) {
		i2 = strlen(XMLTAG_STATISTICS) + 2;
		if (fiowrite(xmlfilenum, xmlfilepos, (UCHAR *) "<" XMLTAG_STATISTICS ">", i2) < 0) death(DEATH_BADXMLWRITE);
		xmlfilepos += i2;
		xmlstats();
		i2 = strlen(XMLTAG_STATISTICS) + 3;
		if (fiowrite(xmlfilenum, xmlfilepos, (UCHAR *) "</" XMLTAG_STATISTICS ">", i2) < 0) death(DEATH_BADXMLWRITE);
		xmlfilepos += i2;
		if (fiowrite(xmlfilenum, xmlfilepos, (UCHAR *) "</compile>", 10) < 0) death(DEATH_BADXMLWRITE);
		fioclose(xmlfilenum);
	}
	else {
		stats();  /* display ending information */
		if (xrefflg) xrefdsp();  /* process and display cross reference */
		if (prtflag) fclose(outfile);
	}
	return (errcnt) ? 1 : 0;
}

/* DBCMPINIT */
/* This routine will allocate any buffers or arrays dynamically,	*/
/* initialize any static variables for this module, returning	*/
/* 0 if successful, RC_ERROR if unsuccessful.				*/
static INT dbcmpinit()
{
	INT i1;

	symblklist[0] = (SYMINFO *) malloc(SYMBLOCKSIZE * sizeof(SYMINFO));
	if (NULL == symblklist[0]) return RC_ERROR;
	symname = (CHAR *) malloc(SYMNAMEBLKSIZE * sizeof(CHAR));
	if (NULL == symname) return RC_ERROR;

	for (i1 = 0; i1 < CLITHASHSIZE; i1++) clithashtab[i1] = -1;
	for (i1 = 0; i1 < NLITREUSETABLESIZE; i1++) nlitreuse[i1] = -1;
	return 0;
}

#define MAXCMDEQUATE 4096	/* max # of bytes on command line used for -E labels */

/* PARSECMDLINE */
/* initialize flags and parse command line */
static INT parsecmdline()
{
	UCHAR c;
	INT i1, i2, n, equatelen, equatesize = 0;
	CHAR **equatelabels = NULL;

	dbcmpflags = 0;
	warninglevel = 0;
	upcaseflg = FALSE;
	nofixupflg = FALSE;
	removeflg = FALSE;
	liston = TRUE;
	filedflt = ifiledflt = afiledflt = 0;
	xmlname[0] = '\0';
	if (argget(ARG_FIRST, srcname, sizeof(srcname))) usage();  /* if no source program name, display help info */
	for (n = 0; ; n++) {	/* go through each parameter */
		i1 = argget(ARG_NEXT, parm, sizeof(parm));
		if (i1) {
			if (i1 < 0) death(DEATH_BADPARM);
			break;
		}
		if (!n && parm[0] != '-') {  /* .dbc name specified */
			strcpy(dbcname, parm);
			continue;
		}
		if (parm[0] != '-' || (i2 = (INT)strlen(parm)) < 2) death(DEATH_BADPARM);
		c = (UCHAR) toupper(parm[1]);
		if (i2 == 2) {  /* one character argument with no parameter */
			switch (c) {
			case '!': dspflags |= DSPFLAG_VERBOSE | DSPFLAG_PROGRAM | DSPFLAG_INCLUDE; continue;
			case '1': incopt = 1; continue;
			case '2': incopt = 2; continue;
			case '3': incopt = 3; continue;
			case '8': dbcmpflags |= DBCMPFLAG_VERSION8; continue;
			case '9': dbcmpflags |= DBCMPFLAG_VERSION9; continue;
			case 'C': upcaseflg = TRUE; continue;
			case 'D': dspflags |= DSPFLAG_COMPILE | DSPFLAG_STATS; continue;
			case 'F':
				fmtflag = TRUE;
				continue;
			case 'I':
				dspflags |= DSPFLAG_VERBOSE | DSPFLAG_PROGRAM;
				continue;
			case 'J':
				openmode = RIO_M_SHR; /* shared read/write */
				continue;
			case 'K':
				warninglevel |= WARNING_LEVEL_1;
				continue;
			case 'M': dspflags |= DSPFLAG_VERBOSE; continue;
			case 'P':
				prtname[0] = 0;
				prtflag = FALSE;
				continue;
			case 'R':
				strcpy(sortcmd, "sort");
				xrefflg = TRUE;
				continue;
			case 'S':
				dspflags |= DSPFLAG_STATS;
				continue;
			case 'V':
				dspflags |= DSPFLAG_VERBOSE | DSPFLAG_PROGRAM | DSPFLAG_INCLUDE;
				continue;
			case 'X': nofixupflg = TRUE; continue;
			case 'Z': dbgflg = TRUE; continue;
/*** CODE: THIS WAS V9.1 UNDOCUMENTED FEATURE TO CALL SETIFOPDIR, WHICH IS ***/
/***       NOW THE DEFAULT.  LEFT IN FOR COMPATIBILITY ***/
			case '~':
				continue;
			}
		}
		else if (parm[2] != '=') {  /* two or more character argument */
			if (parm[1] == 'W') {
				processWarningArgument(i2);
				continue;
			}
			for (i1 = 1; parm[i1] && parm[i1] != '='; i1++) parm[i1] = (UCHAR) toupper(parm[i1]);
			if (!strcmp(&parm[1], "JR")) {
				openmode = RIO_M_SRO; /* shared read only */
				continue;
			}
			if (!strncmp(&parm[1], "KX", 2)) {
				warninglevel |= WARNING_LEVEL_2;
				continue;
			}
			if (!strncmp(&parm[1], "SX", 2)) {
				dspflags |= DSPFLAG_STATS;
				dspflags |= DSPFLAG_XTRASTATS;
				continue;
			}
			if (!strncmp(&parm[1], "CFG=", 4)) continue;
			if (!strncmp(&parm[1], "XML", 3)) {
				if (parm[4] == '=') strcpy(xmlname, &parm[5]);
				dspflags |= DSPFLAG_XML;
				continue;
			}
			for ( ; parm[i1]; i1++) parm[i1] = (UCHAR) toupper(parm[i1]);
			if (!strncmp(&parm[1], "ERR=", 4)) {
				if (!strncmp(&parm[5], "DEL", 3)) removeflg = TRUE;
				else death(DEATH_BADPARM);
				continue;
			}
			if (!strncmp(&parm[1], "FILE=", 5)) i1 = 6;
			else if (!strncmp(&parm[1], "IFILE=", 6) || !strncmp(&parm[1], "AFILE=", 6)) i1 = 7;
			else death(DEATH_BADPARM);
			if (!strcmp(&parm[i1], "TEXT")) i1 = 1;
			else if (!strcmp(&parm[i1], "NATIVE")) i1 = 2;
			else if (!strcmp(&parm[i1], "BINARY")) i1 = 3;
			else if (!strcmp(&parm[i1], "DATA")) i1 = 4;
			else if (!strcmp(&parm[i1], "CRLF")) i1 = 5;
			else death(DEATH_BADPARMVALUE);
			if (c == 'F') filedflt = i1;
			else if (c == 'I') ifiledflt = i1;
			else if (c == 'A') afiledflt = i1;
			continue;
		}
		else {  /* one char argument with parameter */
			switch (c) {
			case 'D':
				dspstart = atol(&parm[3]);
				dspflags |= DSPFLAG_START | DSPFLAG_STATS;
				continue;
			case 'E':
				if (NULL == equatelabels) equatelabels = (CHAR **) memalloc(MAXCMDEQUATE, 0);
				equatelen = (INT)strlen(parm);
				if (equatelen > 34) death(DEATH_BADPARM);
				equatelen -= 2;
				if (equatesize + equatelen > MAXCMDEQUATE) death(DEATH_BADPARM);
				memcpy(&(*equatelabels)[equatesize], &parm[3], (size_t) equatelen);
				equatesize += equatelen;
				continue;
			case 'H':
				strcpy(header, &parm[3]);
				continue;
			case 'L':
				if (libcnt > 2) death(DEATH_LIBMAX);
				strcpy(libname[libcnt], &parm[3]);
				if (miofixname(libname[libcnt], ".lib", FIXNAME_EXT_ADD) < 0) death(DEATH_BADLIBFILE);
				if ((n = fioopen(libname[libcnt], FIO_M_SRO | FIO_P_SRC)) < 0) death(DEATH_BADLIBFILE);
				fioclose(n);
				strcat(libname[libcnt++], "(");
				continue;
			case 'N':
				if ((pagelen = atoi(&parm[3])) <= 7) death(DEATH_BADPARMVALUE);
				else pagelen -= 7;
				continue;
			case 'P':
				strcpy(prtname, &parm[3]);
				prtflag = TRUE;
				continue;
			case 'R':
				strcpy(sortcmd, &parm[3]);
				xrefflg = TRUE;
				continue;
			case 'T':
				strcpy((CHAR *) xlate, &parm[3]);
				xlateflg = TRUE;
				if (!(n = fioopen((CHAR *) xlate, FIO_M_SRO | FIO_P_SRC)) ||
					fioread(n, (OFFSET) 0, xlate, MAXXLT) != MAXXLT) death(DEATH_BADXLTFILE);
				fioclose(n);
				continue;
			case 'W':
				if ((pagewdth = atoi(&parm[3])) < 26 || pagewdth > MAXPGWIDTH) death(DEATH_BADPARMVALUE);
				continue;
			}
		}
		death(DEATH_BADPARM);
	}
	if (dspflags & DSPFLAG_XML) {
		dspflags &= DSPFLAG_XML | DSPFLAG_CONSOLE;  /* turn off all other flags */
		dspsilent();
		xrefflg = fmtflag = FALSE;
	}
	if (fmtflag && !header[0]) strcpy(header, srcname);
	if (equatesize) {  /* handle the -E= command line EQUATE definitions */
		if (upcaseflg) for (n = 0; n < equatesize; n++) (*equatelabels)[n] = toupper((*equatelabels)[n]);
		symtype = TYPE_EQUATE;
		symvalue = 1;
		for (n = 0; n < equatesize; n += (INT)strlen(&(*equatelabels)[n]) + 1) putdsym(&(*equatelabels)[n], FALSE);
		memfree((UCHAR **) equatelabels);
	}
	if (!dbcname[0]) {  /* .dbc name not specified */
		strcpy(dbcname, srcname);
		miofixname(dbcname, ".dbc", FIXNAME_EXT_ADD | FIXNAME_EXT_REPLACE);
	}
	else miofixname(dbcname, ".dbc", FIXNAME_EXT_ADD);
	if (!(dbcmpflags & (DBCMPFLAG_VERSION8 | DBCMPFLAG_VERSION9))) setifopdir(TRUE);
	/*
	 * If both -k and -kx appeared on the command line, turn off -k
	 */
	if ((warninglevel & WARNING_LEVEL_1) && (warninglevel & WARNING_LEVEL_2))
		warninglevel = WARNING_LEVEL_2;
	return 0;
}

/**
 * This is just the beginning.
 * In 14.0.x we have a special -Wsue for S Snyder of Syntonic
 * Make this more 'corporate' for 15.0
 */
static void processWarningArgument(INT parmlen) {
	if (parmlen == 5) {
		if (strncmp(&parm[2], "sue", 3) == 0) warning_2fewendroutine = TRUE;
	}
}


/* SAVELINE */
void saveline(UCHAR *linesrc, INT linecntsrc)
{
	memcpy(linesav, linesrc, LINESIZE + 1);
	linecntsav = linecntsrc;
}

/* RESTORELINE */
void restoreline(UCHAR *linedst, INT *linecntdst)
{
	memcpy(linedst, linesav, LINESIZE + 1);
	*linecntdst = linecntsav;
}

/**
 * GETLN
 * get the next line of input
 * returns 0 if OK, 1 if end of file, negative otherwise
 */
INT getln()
{
	INT i, n;
	CHAR work[16];

	while (TRUE) {
		while ((n = rioget(curfile, line, LINESIZE)) == -2);
		if (n == -1) {  /* eof */
			if (dbgflg) dbgeofrec((INT) linecounter[inclevel]);
#if 0
/*** CODE: PROBABLY SHOULD BE DSPFLAG_COUNT ***/
			if ((dspflags & DSPFLAG_INCLUDE) && !inclevel) {
				outputinc();
				outputc('\n');
			}
#endif
			if (!srcclose()) return(1);
			continue;
		}
		if (n < 0) {
			if (dspflags & DSPFLAG_XML) xmlmessage(XML_ERROR, (INT) linecounter[inclevel] + 1, savname[inclevel], fioerrstr(n));
			else {
				if (dspflags & DSPFLAG_COMPILE) dspstring("              ");
				msciton((INT) (linecounter[inclevel] + 1), (UCHAR *) work, 7);
				work[7] = '\0';
				dspstring(work);
				dspstring(".(");
				dspstring(savname[inclevel]);
				dspstring("): ");
				dspstring(fioerrstr(n));
				dspchar('\n');
			}
			death(DEATH_BADSRCREAD);
		}
		line[n] = '\0';
		if (xlateflg) for (i = n; i >= 0; i--) line[i] = xlate[line[i]];
		totlines++;
		linecounter[inclevel]++;
		if ((dspflags & DSPFLAG_START) && codeadr >= dspstart) {
			dspflags &= ~DSPFLAG_START;
			dspflags |= DSPFLAG_COMPILE;
		}
		if ((dspflags & DSPFLAG_COMPILE) || dbgflg) {
			savcodeadr = codeadr;
			savdataadr = dataadr;
		}
		if (!line[0] || line[0] == '.' || line[0] == '*' || line[0] == '+') {
			if (dspflags & DSPFLAG_XML) {
				if (n > 7 && !memcmp(line + 2, "TODO", 4) && (line[6] == ' ' || line[6] == '\t')) {
					xmlmessage(XML_TASK_TODO, (INT) linecounter[inclevel], savname[inclevel], (CHAR *)line + 7);
				}
				else if (n > 7 && !memcmp(line + 2, "CODE", 4) && (line[6] == ' ' || line[6] == '\t')) {
					xmlmessage(XML_TASK_CODE, (INT) linecounter[inclevel], savname[inclevel], (CHAR *)line + 7);
				}
				else if (n > 7 && !memcmp(line + 2, "FIXME", 5) && (line[7] == ' ' || line[7] == '\t')) {
					xmlmessage(XML_TASK_FIXME, (INT) linecounter[inclevel], savname[inclevel], (CHAR *)line + 8);
				}
			}
			if (dspflags & DSPFLAG_COMPILE) getlndsp();
			continue;
		}
		break;
	}
	if (dbgflg) riolastpos(curfile, &dbgoff);

	return(0);
}

/* STARTCLASSDEF */
/* Start a new class definition block */
INT startclassdef(CHAR *classname, CHAR *parent, CHAR *makename, CHAR *destname)
{
	USHORT makelabel, destlabel;	/* OK for new labels, these are not simplelabels */
	INT curhash, strsize;
	UINT currlevel;
	CHAR module[32];

	if (classlevel) {
		error(ERROR_CLASSDEPTH);
		return(RC_ERROR);
	}

	/* reset temporary variable reuse */
	xtempvarreset();

	maindataadr = dataadr;	/* save main dataadr variable */
	dataadr = clsdatabufout = clsdatabufcnt = 0;
	classlevel = 1;
	curclassid++;
	if ((classhdr = memalloc(CLASSHEADERSIZE, 0)) == NULL) death(DEATH_NOMEM);
	if ((clsdatabuf = memalloc(CLASSDATASIZE, 0)) == NULL) death(DEATH_NOMEM);
	if ((mtddeftable = memalloc((INT) (mtddefalloc = METHODDEFINCREMENT), 0)) == NULL) death(DEATH_NOMEM);
	if ((clsxreftable = memalloc((INT) (clsxrefalloc = METHODREFINCREMENT), 0)) == NULL) death(DEATH_NOMEM);
	if ((datambrtable = memalloc((INT) (datambralloc = DATAMBRINCREMENT), 0)) == NULL) death(DEATH_NOMEM);
	mtddefsize = clsxrefsize = datambrsize = 4;	/* allocate 4 header bytes */

	saveexelblnext = exelabelnext;
	saveexelblsize = exelabelsize;
	saveexelbltable = exelabeltable;
	exelabelsize = exelabelnext = 0;
	exelabeltable = NULL;
	for (currlevel = 0; currlevel <= rlevel; currlevel++) {
		saverlevelid[currlevel] = rlevelid[currlevel];
	}
	saverlevel = rlevel;
	saverlevelidcnt = rlevelidcnt;
	saveclittable = clittable;
	saveclitsize = clittablesize;
	saveclitalloc = clittablealloc;
	for (curhash = 0; curhash < CLITHASHSIZE; curhash++) classclithash[curhash] = -1;
	for (curhash = 0; curhash < NLITREUSETABLESIZE; curhash++) classnlitreuse[curhash] = -1;
	clittable = NULL;
	clittablesize = clittablealloc = 0;
	rlevelid[0] = 0;
	rlevel = 0;
	rlevelidcnt = 0;

	(*classhdr)[0] = 0x05;	/* segment tag ID, class def segment */
	if (makename != NULL && makename[0]) {
		usepsym(makename, LABEL_REFFORCE);
		makelabel = (USHORT) symvalue;
	}
	else makelabel = USHRT_MAX;
	if (destname != NULL && destname[0]) {
		usepsym(destname, LABEL_REFFORCE);
		destlabel = (USHORT) symvalue;
	}
	else destlabel = USHRT_MAX;
	(*classhdr)[4] = (UCHAR) (makelabel >> 8);
	(*classhdr)[5] = (UCHAR) makelabel;
	(*classhdr)[6] = (UCHAR) (destlabel >> 8);
	(*classhdr)[7] = (UCHAR) destlabel;
	strsize = (INT)strlen(classname) + 1;
	memcpy(&(*classhdr)[8], classname, (size_t) strsize);
	classhdrsize = 8 + strsize;

	if (parent != NULL && parent[0]) {		/* this class is a derived class */
		if (getclass(parent, module) == RC_ERROR) {
			strcpy((CHAR *) token, parent);
			error(ERROR_CLASSNOTFOUND);
			return(RC_ERROR);
		}
		strsize = (INT)strlen(parent) + 1;
		memcpy(&(*classhdr)[classhdrsize], parent, (size_t) strsize);
		classhdrsize += strsize;
		strsize = (INT)strlen(module) + 1;
		memcpy(&(*classhdr)[classhdrsize], module, (size_t) strsize);
		classhdrsize += strsize;
		childclsflag = TRUE;
	}
	else {
		(*classhdr)[classhdrsize++] = '\0';	/* zero-length parent class name */
		(*classhdr)[classhdrsize++] = '\0';	/* zero-length parent mod name */
	}
	clshdrfilepos = dbcfilepos;
	/* reserve storage for 3 byte class data area size, 3 byte unexpanded storage size and 1 byte ID */
	classhdrsize += 7;
	dbcfilepos += classhdrsize;
	classjmplabel = getlabelnum();
	makegoto(classjmplabel);
	if (dbgflg) dbgclassrec(classname);
	return(0);
}

/* ENDCLASSDEF */
/* End the current class definition block */
void endclassdef()
{
	INT exelabel, symblk, symoffset;
	UINT totalsegsize;
	UINT sizeout, currlevel;
	USHORT curlabel;	/* OK for new labels, not a simplelabel */
	SYMINFO *symtableptr;

	if (!classlevel) {
		error(ERROR_MISSINGCLASSDEF);
		return;
	}
 	putmain1(0x64);		/* write implicit STOP code */
	deflabelnum(classjmplabel);
	putdata1(0xF5);		/* end of class data area indicator */
	dataadr++;

	(*classhdr)[classhdrsize - 7] = 0x00;			/* Class data area ID */

	sizeout = clsdatabufout + clsdatabufcnt + 3;		/* length of class data area storage plus 3 header bytes (data area run-time size) */
	(*classhdr)[classhdrsize - 6] = (UCHAR) sizeout;	/* length of data area storage in .DBC */
	(*classhdr)[classhdrsize - 5] = (UCHAR) (sizeout >> 8);
	(*classhdr)[classhdrsize - 4] = (UCHAR) (sizeout >> 16);

	(*classhdr)[classhdrsize - 3] = (UCHAR) dataadr;	/* class data area size (at run-time) */
	(*classhdr)[classhdrsize - 2] = (UCHAR) (dataadr >> 8);
	(*classhdr)[classhdrsize - 1] = (UCHAR) (dataadr >> 16);

	/* write out execution label table by adding to end of data area */
	putdata1(0x01); 				/* Class execution label table ID byte */
	putdatalmh(exelabelnext * 3);		/* table size */
	for (curlabel = 0; curlabel < exelabelnext; curlabel++) {
		exelabel = (*exelabeltable)[curlabel];
		if ((exelabel & 0xFF000000) == 0x01000000) {  /* undefined or external reference */
			symoffset = (exelabel & 0x00FFFFFF) - 1;
			symblk = symoffset >> SYMBLOCKSHIFT;
			symoffset -= (symblk << SYMBLOCKSHIFT);
			symtableptr = symblklist[symblk] + symoffset;
			if (symtableptr->type != TYPE_PLABELXREF && symtableptr->type != TYPE_PLABELMREF) {
				strcpy((CHAR *) token, symtableptr->nameptr);
				error(ERROR_LABELNOTFOUND);
			}
		}
		putdatalmh(exelabel);
	}

	/* write out class data area */
	if (clsdatabufcnt) putclsdataout();

	/* write out class method definition table */
	sizeout = mtddefsize - 4;	/* subtract 4 for the header bytes */
	if (sizeout) {
		(*mtddeftable)[0] = 0x02;
		(*mtddeftable)[1] = (UCHAR) sizeout;
		(*mtddeftable)[2] = (UCHAR) (sizeout >> 8);
		(*mtddeftable)[3] = (UCHAR) (sizeout >> 16);
		if (fiowrite(dbcfilenum, dbcfilepos, *mtddeftable, (INT) mtddefsize) < 0) death(DEATH_BADDBCWRITE);
		dbcfilepos += mtddefsize;
	}
	else mtddefsize = 0;

	/* write out class method reference table */
	sizeout = clsxrefsize - 4;	/* subtract 4 for the header bytes */
	if (sizeout) {
		(*clsxreftable)[0] = 0x03;
		(*clsxreftable)[1] = (UCHAR) sizeout;
		(*clsxreftable)[2] = (UCHAR) (sizeout >> 8);
		(*clsxreftable)[3] = (UCHAR) (sizeout >> 16);
		if (fiowrite(dbcfilenum, dbcfilepos, *clsxreftable, (INT) clsxrefsize) < 0) death(DEATH_BADDBCWRITE);
		dbcfilepos += clsxrefsize;
	}
	else clsxrefsize = 0;

	/* write out class data member name table */
	sizeout = datambrsize - 4;	/* subtract 4 for the header bytes */
	if (sizeout) {
		(*datambrtable)[0] = 0x06;
		(*datambrtable)[1] = (UCHAR) sizeout;
		(*datambrtable)[2] = (UCHAR) (sizeout >> 8);
		(*datambrtable)[3] = (UCHAR) (sizeout >> 16);
		if (fiowrite(dbcfilenum, dbcfilepos, *datambrtable, (INT) datambrsize) < 0) death(DEATH_BADDBCWRITE);
		dbcfilepos += datambrsize;
	}
	else datambrsize = 0;

	/* finish filling out class segment header & class data area size, write it out */
	totalsegsize = clsdatabufout + clsdatabufcnt + datambrsize +
				mtddefsize + clsxrefsize + classhdrsize - 4;
	(*classhdr)[1] = (UCHAR) totalsegsize;
	(*classhdr)[2] = (UCHAR) (totalsegsize >> 8);
	(*classhdr)[3] = (UCHAR) (totalsegsize >> 16);
	if (fiowrite(dbcfilenum, clshdrfilepos, *classhdr, (INT) classhdrsize) < 0) death(DEATH_BADDBCWRITE);

	classlevel = 0;
	childclsflag = FALSE;
	/* free all allocation made for this class definition compile */
	if (exelabelsize) memfree((UCHAR **) exelabeltable);
	memfree(datambrtable);
	memfree(clsxreftable);
	memfree(mtddeftable);
	memfree(clsdatabuf);
	memfree(classhdr);

	exelabelnext = saveexelblnext;
	exelabelsize = saveexelblsize;
	exelabeltable = saveexelbltable;
	clittable = saveclittable;
	clittablesize = saveclitsize;
	clittablealloc = saveclitalloc;
	datambrtable = mtddeftable = clsdatabuf = classhdr = NULL;
	dataadr = maindataadr;
	rlevel = saverlevel;
	rlevelidcnt = saverlevelidcnt;
	for (currlevel = 0; currlevel <= rlevel; currlevel++) {
		rlevelid[currlevel] = saverlevelid[currlevel];
	}
	if (dbgflg) dbgendclassrec();

	/* reset temporary variable reuse */
	xtempvarreset();
}

/* GETCLASS */
/* Find classname in symbol table, if found copy modname to module if */
/* one exists, otherwise set module to be a zero-length string */
INT getclass(CHAR *classname, CHAR *module)
{
	UINT hashval;
	USHORT symbollen;
	SYMINFO *symtableptr;

	/* find a hash value to reference the symbol table with */
	hashval = symhashfunct(classname, &symbollen);

	if (NULL == symtablebase[hashval]) return(RC_ERROR);
	else {
		symtableptr = symtablebase[hashval] + symtableoffset[hashval];
		while (symtableptr != NULL) {
			if (symtableptr->len == symbollen &&
			    !memcmp(symtableptr->nameptr, classname, symbollen)) {
				do {
					if (TYPE_CLASS == symtableptr->type) {
						if (NULL == symtableptr->u2.modname) module[0] = '\0';
						else strcpy(module, symtableptr->u2.modname);
						return(0);
					}
					symtableptr = symtableptr->next;	/* get next SYMINFO w/same name */
				} while (symtableptr != NULL);
				return(RC_ERROR);
			}
			symtableptr = symtableptr->nexthash;	/* get next SYMINFO w/same hash value */
		}
	}
	return(RC_ERROR);
}

/* PUTCLASS */
/* Put a new class name (w/module name) into the symbol table */
/* classflag is either CLASS_DEFINITION or CLASS_DECLARATION */
INT putclass(CHAR *classname, CHAR *module, INT classflag)
{
	INT newnamesize;
	UINT hashval;
	USHORT symbollen;
	SYMINFO *symtableptr, *lastsymptr;

	/* find a hash value to reference the symbol table with */
	hashval = symhashfunct(classname, &symbollen);

	if (NULL == symtablebase[hashval]) {
		symtableptr = getnextsymentry(classname, symbollen);
		symtablebase[hashval] = symtableptr;
	}
	else {
		symtableptr = symtablebase[hashval] + symtableoffset[hashval];
		do {
			if (symtableptr->len == symbollen &&
			    !memcmp(symtableptr->nameptr, classname, symbollen)) {
				do {
					if (TYPE_CLASS == symtableptr->type) {
						if (module != NULL || symtableptr->u2.modname != NULL) {
							error(ERROR_AMBIGUOUSCLASSMOD);
							return(RC_ERROR);
						}
						if (classflag & CLASS_DEFINITION) {
							if (symtableptr->u1.classflag & CLASS_DEFINITION) {
								error(ERROR_DUPCLASS);
								return(RC_ERROR);
							}
							symtableptr->u1.classflag |= (SHORT) classflag;
						}
						return(0);
					}
					lastsymptr = symtableptr;
					symtableptr = symtableptr->next;	/* get next SYMINFO w/same name */
				} while (symtableptr != NULL);
				symtableptr = getnextsymentry(classname, symbollen);
				lastsymptr->next = symtableptr;
				break;
			}
			lastsymptr = symtableptr;
			symtableptr = symtableptr->nexthash;	/* get next SYMINFO w/same hash value */
			if (NULL == symtableptr) {
				symtableptr = getnextsymentry(classname, symbollen);
				lastsymptr->nexthash = symtableptr;
				break;
			}
		} while (TRUE);
	}
	if (module != NULL) {
		symbollen = (USHORT)strlen(module) + 1;
		newnamesize = symnamesize + symbollen;
		if (newnamesize >= SYMNAMEBLKSIZE) {
			symname = (CHAR *) malloc(SYMNAMEBLKSIZE * sizeof(CHAR));
			if (NULL == symname) death(DEATH_NOMEM);
			symnamesize = 0;
			newnamesize = symbollen;
		}
		symtableptr->u2.modname = &symname[symnamesize];
		symnamesize = newnamesize;
		memcpy(symtableptr->u2.modname, module, symbollen);
	}
	else symtableptr->u2.modname = NULL;
	symtableptr->u1.classflag |= (SHORT) classflag;
	symtableptr->type = TYPE_CLASS;
	return(0);
}

/* SYMHASHFUNCT
 * Hash function used to convert a string to a reference to symbol hash table
 * return value will be zero to (SYMHASHSIZE - 1)
 */
static UINT symhashfunct(CHAR *symbol, USHORT *symlen)
{
	UINT hashval, prevval;
	UINT curshift;
	USHORT curlen;

	prevval = hashval = 0;
	for (curshift = curlen = 0; symbol[curlen]; curlen++) {
		hashval += ((UINT) symbol[curlen] << curshift++);
		if (hashval < prevval) {
			hashval = prevval % SYMHASHSIZE + (UINT) symbol[curlen];
			curshift = 1;
		}
		prevval = hashval;
	}
	hashval += curlen;
	if (hashval < prevval) {
		hashval = prevval % SYMHASHSIZE + (UINT) symbol[curlen] + curlen;
	}
	hashval %= SYMHASHSIZE;
	*symlen = curlen;
	return hashval;
}

/* GETNEXTSYMENTRY */
/* Return the next free SYMINFO structure in the current block.	*/
/* If the current block is full, allocate another block.  Also,	*/
/* allocate space for symbol name storage.					*/
static SYMINFO *getnextsymentry(CHAR *symbol, USHORT symlen)
{
	INT newnamesize;
	SYMINFO *newsymstruct;

	if (SYMBLOCKSIZE == symblksize) {
		if (++cursymblk >= MAXSYMBLOCK) death(DEATH_MAXSYMBOLS);
		symblklist[cursymblk] = (SYMINFO *) malloc(SYMBLOCKSIZE * sizeof(SYMINFO)); /* 1024 */
		if (NULL == symblklist[cursymblk]) death(DEATH_NOMEM);
		symblksize = 0;
	}
	newsymstruct = symblklist[cursymblk] + symblksize;
	symblksize++;
	cursyminfo++;
	newnamesize = symnamesize + symlen + 1;
	if (newnamesize >= SYMNAMEBLKSIZE) {
		symname = (CHAR *) malloc(SYMNAMEBLKSIZE * sizeof(CHAR));
		if (NULL == symname) death(DEATH_NOMEM);
		symnamesize = 0;
		newnamesize = symlen + 1;
	}
	newsymstruct->u1.classflag = 0;
	newsymstruct->u2.modname = NULL;
	newsymstruct->nexthash = newsymstruct->next = NULL;
	newsymstruct->len = symlen;
	newsymstruct->nameptr = &symname[symnamesize];
	symnamesize = newnamesize;
	memcpy(newsymstruct->nameptr, symbol, (size_t) symlen + 1);
	return(newsymstruct);
}

/* PUTDSYM */
/* Store data area variable with name symbol, symvalue, symtype, class	*/
/* and scope information. 										*/
/* inheritable flag set to TRUE if variable is considered inheritable by child classes */
void putdsym(CHAR *symbol, INT inheritable)
{
	UINT hashval, workrlevel;
	USHORT symbollen;
	USHORT workclsid;
	SYMINFO *symtableptr, *lastsymptr, *reuseptr;

	if(!symbol[0]) return;

	if (xrefflg) xrefout(symbol, 1);		/* cross reference */
 	if (dbgflg && symtype != TYPE_EQUATE) dbgdatarec(savdataadr, symbol);	/* debug record */

	/* find a hash value to reference the symbol table with */
	hashval = symhashfunct(symbol, &symbollen);

	/* equate labels are visible to rest of source module */
	/* (they have no scope)						    */
	if (TYPE_EQUATE == symtype) {
		workrlevel = workclsid = 0;
	}
	else {
		workrlevel = rlevel;
		if (classlevel) workclsid = curclassid;
		else workclsid = 0;
	}

	if (NULL == symtablebase[hashval]) {
		symtableptr = getnextsymentry(symbol, symbollen);
		symtablebase[hashval] = symtableptr;
	}
	else {
		symtableptr = symtablebase[hashval] + symtableoffset[hashval];
		while (TRUE) {
			if (symtableptr->len == symbollen &&
			    !memcmp(symtableptr->nameptr, symbol, symbollen)) {
				reuseptr = NULL;
				do {
					if (symtableptr->type < TYPE_MAXDATAVAR) {
						if (symtableptr->rlevel <= workrlevel &&
						    symtableptr->classid == workclsid) {
							if (rlevelid[symtableptr->rlevel] == symtableptr->routineid) {
								error(ERROR_DUPVAR);
								return;
							}
							if (symtableptr->rlevel == workrlevel) {  /* reuse SYMINFO struct */
								reuseptr = symtableptr;
							}
						}
					}
					lastsymptr = symtableptr;
					symtableptr = symtableptr->next;	/* get next SYMINFO w/same name */
				} while (symtableptr != NULL);
				if (reuseptr != NULL) symtableptr = reuseptr;
				else {
					symtableptr = getnextsymentry(symbol, symbollen);
					lastsymptr->next = symtableptr;
				}
				break;
			}
			lastsymptr = symtableptr;
			symtableptr = symtableptr->nexthash;	/* get next SYMINFO w/same hash value */
			if (NULL == symtableptr) {
				symtableptr = getnextsymentry(symbol, symbollen);
				lastsymptr->nexthash = symtableptr;
				break;
			}
		}
	}
	if (inheritable && classlevel && !rlevel) adddatambr(symbol, (INT) symbollen);
	symtableptr->u2.dataadr = symvalue;
	symtableptr->type = symtype;
	symtableptr->rlevel = workrlevel;
	symtableptr->classid = workclsid;
	symtableptr->routineid = rlevelid[workrlevel];
	if (symvalue >= 0x800000) error(DEATH_DATATOOBIG);
}

/* GETDSYM */
/* Retrieve data area variable information and store in symvalue and symtype. */
/* Return RC_ERROR if var not found, else return 0 				   */
INT getdsym(CHAR *symbol)
{
	UINT hashval;
	USHORT symbollen;
	USHORT workclsid;
	SYMINFO *symtableptr;

	if(!symbol[0]) return(RC_ERROR);

	/* find a hash value to reference the symbol table with */
	hashval = symhashfunct(symbol, &symbollen);

	if (classlevel) workclsid = curclassid;
	else workclsid = 0;

	if (NULL == symtablebase[hashval]) return(RC_ERROR);
	else {
		symtableptr = symtablebase[hashval] + symtableoffset[hashval];
		while (symtableptr != NULL) {
			if (symtableptr->len == symbollen &&
			    !memcmp(symtableptr->nameptr, symbol, symbollen)) {
				do {
					if (symtableptr->type < TYPE_MAXDATAVAR) {
						if (TYPE_EQUATE == symtableptr->type ||
						    (symtableptr->rlevel <= rlevel &&
						    symtableptr->classid == workclsid)) {
							if (rlevelid[symtableptr->rlevel] == symtableptr->routineid) {
								symvalue = symtableptr->u2.dataadr;
								symtype = symtableptr->type;
								if (xrefflg) xrefout(symbol, 0);
								return(0);
							}
						}
					}
					symtableptr = symtableptr->next;	/* get next SYMINFO w/same name */
				} while (symtableptr != NULL);
				return(RC_ERROR);
			}
			symtableptr = symtableptr->nexthash;	/* get next SYMINFO w/same hash value */
		}
	}
	return(RC_ERROR);
}

/**
 * USEPSYM
 * Reference and/or define program execution label.
 * symbol is label name, codeadr is label value for flag != LABEL_REF
 * valid flag values are:
 *   LABEL_REF			= label reference
 *   LABEL_REFFORCE		= label reference, force label table
 *   LABEL_DEF			= label definition
 *   LABEL_VARDEF		= LABEL var def
 *   LABEL_EXTERNAL		= EXTERNAL
 *   LABEL_ROUTINE		= ROUTINE
 *   LABEL_METHOD		= METHOD
 *   LABEL_METHODREF	= method label reference
 *   LABEL_METHODREFDEF	= method label reference; define it if undefined
 * return symtype and symvalue for flag = LABEL_REF or LABEL_REFFORCE
 */
INT usepsym(CHAR *symbol, UINT flag)
{
	UINT hashval, workrlevel;
	USHORT workclsid, symbollen;
	SYMINFO *symtableptr, *lastsymptr, *reuseptr;

	if(!symbol[0]) return(RC_ERROR);
	if (nofixupflg && flag == LABEL_REF) flag = LABEL_REFFORCE;

	/* find a hash value to reference the symbol table with */
	hashval = symhashfunct(symbol, &symbollen);


	workrlevel = rlevel;
	if (classlevel) workclsid = curclassid;
	else workclsid = 0;

	if (NULL == symtablebase[hashval]) {
		symtableptr = getnextsymentry(symbol, symbollen);
		symtablebase[hashval] = symtableptr;
	}
	else {
		symtableptr = symtablebase[hashval] + symtableoffset[hashval];
		while (TRUE) {
			if (symtableptr->len == symbollen && !memcmp(symtableptr->nameptr, symbol, symbollen))
			{
				reuseptr = NULL;
				do {
					symtype = symtableptr->type;
					if (symtype > TYPE_MAXDATAVAR && symtype != TYPE_CLASS &&
					    symtableptr->classid == workclsid) {
						if (TYPE_PLABELVAR != symtype ||
						   (symtableptr->rlevel <= workrlevel && rlevelid[symtableptr->rlevel] == symtableptr->routineid)) {
							switch (flag) {
								case LABEL_REF:
									if (TYPE_PLABELMREF == symtype) break;  /* continue searching for non-method reference */
									if (TYPE_PLABELXREF == symtype) {
										symvalue = symtableptr->u1.exelabelref;
										if (((*exelabeltable)[symvalue] & 0xFF000000) == 0x01000000) {
											(*exelabeltable)[symvalue] = addxref(symbol, (INT) symbollen, REF_EXTERNAL);
										}
									}
									else if (TYPE_PLABEL == symtype || TYPE_PLABELXDEF == symtype || TYPE_PLABELMDEF == symtype) {
										if (symtableptr->u2.simpleref == (UINT) 0xFFFFFFFF) {
											EXTENDSIMPLETABLE();
											(*simplelabeltable)[simplelabelnext] = (*exelabeltable)[symtableptr->u1.exelabelref];
											symtableptr->u2.simpleref = simplelabelnext++;
										}
										symvalue = SIMPLELABELBIAS +  symtableptr->u2.simpleref;
									}
									else symvalue = symtableptr->u1.exelabelref;
									if (xrefflg) xrefout(symbol, flag);
									return(0);
								case LABEL_REFFORCE:
									if (TYPE_PLABELMREF == symtype) break;  /* continue searching for non-method reference */
									if (symtableptr->u1.exelabelref != 0xFFFF) {
										symvalue = symtableptr->u1.exelabelref;
										if (((*exelabeltable)[symvalue] & 0xFF000000) == 0x01000000) {
#if 0
/*** CODE: BECAUSE OF ADDED ABOVE IF, TOOK THIS CODE OUT ***/
											if (TYPE_PLABELMREF == symtype) {
												(*exelabeltable)[symvalue] = addxref(symbol, symbollen, REF_METHOD);
											}
											else
#endif
											if (TYPE_PLABELXREF == symtype) {
												(*exelabeltable)[symvalue] = addxref(symbol, (INT) symbollen, REF_EXTERNAL);
											}
										}
										if (xrefflg) xrefout(symbol, LABEL_REF);
										return(0);
									}
									EXTENDEXETABLE();
									(*exelabeltable)[exelabelnext] = (*simplelabeltable)[symtableptr->u2.simpleref];
									symvalue = symtableptr->u1.exelabelref = exelabelnext++;
									if (xrefflg) xrefout(symbol, LABEL_REF);
									return(0);
								case LABEL_DEF:
								case LABEL_ROUTINE:
									if (symtype != TYPE_PLABEL) {
										if (TYPE_PLABELMREF == symtype) break;  /* continue searching for non-method reference */
										error(ERROR_DUPLABEL);
										return(RC_ERROR);
									}
									if (symtableptr->u1.exelabelref != 0xFFFF) {
										if ((*exelabeltable)[symtableptr->u1.exelabelref] < 0x01000000) {
											error(ERROR_DUPLABEL);
											return(RC_ERROR);
										}
										(*exelabeltable)[symtableptr->u1.exelabelref] = codeadr;
									}
									if (symtableptr->u2.simpleref != (UINT) 0xFFFFFFFF) {
										if ((*simplelabeltable)[symtableptr->u2.simpleref] < 0x01000000) {
											error(ERROR_DUPLABEL);
											return(RC_ERROR);
										}
										(*simplelabeltable)[symtableptr->u2.simpleref] = codeadr;
									}
									if (LABEL_ROUTINE == flag) {
										if (classlevel) addmtddef(symbol, (INT) symbollen);
										else addxdef(symbol, (INT) symbollen);
										symtableptr->type = symtype;
									}
									if (xrefflg) xrefout(symbol, flag);
									return(0);
								case LABEL_VARDEF:
									error(ERROR_DUPLABEL);
									return(RC_ERROR);
								case LABEL_METHOD:
									if (symtype != TYPE_PLABELMREF) break;  /* continue searching for method reference */
#if 0
/*** CODE:  IF KNEW PREVIOUS WAS FROM LABEL_METHOD AND NOT LABEL_METHODREFDEF ***/
/***        THEN GIVE DUP, BUT CODE BELOW DOES NOT PREVENT METHOD, CALL METHOD, ***/
/***        METHOD SO IT WOULD BE INCONSISTANT ***/
									symvalue = symtableptr->u1.exelabelref;
									if (((*exelabeltable)[symvalue] & 0xFF000000) == 0x01000000) {
										error(ERROR_DUPLABEL);
										return(RC_ERROR);
									}
#endif
									return(0);
								case LABEL_EXTERNAL:
#if 0
									/*** CODE: IN THEORY, IT SHOULD BE THIS AS "X EXTERNAL", "X ROUTINE" IS NOT SUPPORTED ***/
									if (TYPE_PLABELMREF == symtype || TYPE_PLABELDREF == symtype) {  /* do not allow external w/ method or routine */
#endif
								    /*
								     * do not allow both external & (method || routine)
								     */
									if (symtype == TYPE_PLABELMREF || symtype == TYPE_PLABELXDEF) {
										error(ERROR_DUPLABEL);
										return(RC_ERROR);
									}
									if (!nofixupflg && symtype == TYPE_PLABEL) {
										error(ERROR_BADEXTERNAL);
										return(RC_ERROR);
									}
									symtableptr->type = symtype = TYPE_PLABELXREF;
									if (xrefflg) xrefout(symbol, flag);
									return(0);
								case LABEL_METHODREF:
								case LABEL_METHODREFDEF:
									if (TYPE_PLABELMREF != symtype) {
										if (TYPE_PLABELXREF == symtype) {
											error(ERROR_DUPLABEL);
											return(RC_ERROR);
										}
										break;  /* continue searching for method reference */
									}
									symvalue = symtableptr->u1.exelabelref;
									if (((*exelabeltable)[symvalue] & 0xFF000000) == 0x01000000) {
										(*exelabeltable)[symvalue] = addxref(symbol, (INT) symbollen, REF_METHOD);
									}
									if (xrefflg) xrefout(symbol, LABEL_REF);
									return(0);
								default:
									death(DEATH_INTERNAL4);
							} // end of switch(flag)
						}
						else {  /* a PLABELVAR not in scope */
							if (flag != LABEL_VARDEF) {
								error(ERROR_DUPLABEL);
								return(RC_ERROR);
							}
							if (symtableptr->rlevel == workrlevel) {  /* reuse SYMINFO struct */
								reuseptr = symtableptr;
							}
						}
					}
					lastsymptr = symtableptr;
					symtableptr = symtableptr->next;	/* get next SYMINFO w/same name */
				} while (symtableptr != NULL);
				if (reuseptr != NULL) symtableptr = reuseptr;
				else {
					symtableptr = getnextsymentry(symbol, symbollen);
					lastsymptr->next = symtableptr;
				}
				break;
			}
			lastsymptr = symtableptr;
			symtableptr = symtableptr->nexthash;	/* get next SYMINFO w/same hash value */
			if (NULL == symtableptr) {
				symtableptr = getnextsymentry(symbol, symbollen);
				lastsymptr->nexthash = symtableptr;
				break;
			}
		} // If the name matched end
	} // 'while(TRUE)' loop end

	switch (flag) {
		case LABEL_REF:		/* first use is a simple reference */
			EXTENDSIMPLETABLE();
			symtype = symtableptr->type = TYPE_PLABEL;
			symtableptr->u1.exelabelref = 0xFFFF;
			(*simplelabeltable)[simplelabelnext] = 0x01000000 + cursyminfo;
			symtableptr->u2.simpleref = simplelabelnext;
			symvalue =  SIMPLELABELBIAS + simplelabelnext++;
			break;
		case LABEL_REFFORCE:
			symtableptr->u2.simpleref = 0xFFFFFFFF;
			EXTENDEXETABLE();
			symvalue = symtableptr->u1.exelabelref = exelabelnext++;
			(*exelabeltable)[symvalue] = 0x01000000 + cursyminfo;
			symtype = symtableptr->type = TYPE_PLABEL;
			flag = LABEL_REF;
			break;
		case LABEL_DEF:		/* first use is a simple definition */
			EXTENDSIMPLETABLE();
			symtableptr->type = symtype = TYPE_PLABEL;
			symtableptr->u1.exelabelref = 0xFFFF;
			(*simplelabeltable)[simplelabelnext] = codeadr;
			symtableptr->u2.simpleref = simplelabelnext++;
			break;
		case LABEL_VARDEF:
			symtableptr->u2.simpleref = 0xFFFFFFFF;
			EXTENDEXETABLE();
			symvalue = symtableptr->u1.exelabelref = exelabelnext++;
			(*exelabeltable)[symvalue] = 0x00FFFFFF;
			symtableptr->type = symtype = TYPE_PLABELVAR;
			symtableptr->rlevel = workrlevel;
			symtableptr->routineid = rlevelid[workrlevel];
			break;
		case LABEL_METHOD:
		case LABEL_METHODREFDEF:
			symtableptr->u2.simpleref = 0xFFFFFFFF;
			EXTENDEXETABLE();
			symvalue = symtableptr->u1.exelabelref = exelabelnext++;
			symtableptr->type = symtype = TYPE_PLABELMREF;
			if (flag == LABEL_METHOD) (*exelabeltable)[symvalue] = 0x01000000 + cursyminfo;
			else (*exelabeltable)[symvalue] = addxref(symbol, (INT) symbollen, REF_METHOD);
			flag = LABEL_REF;
			break;
		case LABEL_EXTERNAL:
			symtableptr->u2.simpleref = 0xFFFFFFFF;
			EXTENDEXETABLE();
			symvalue = symtableptr->u1.exelabelref = exelabelnext++;
			(*exelabeltable)[symvalue] = 0x01000000 + cursyminfo;
			symtableptr->type = symtype = TYPE_PLABELXREF;
			flag = LABEL_REF;
			break;
		case LABEL_ROUTINE:
			symtableptr->u2.simpleref = 0xFFFFFFFF;
			EXTENDEXETABLE();
			symvalue = symtableptr->u1.exelabelref = exelabelnext++;
			(*exelabeltable)[symvalue] = codeadr;
			if (classlevel) addmtddef(symbol, (INT) symbollen);
			else addxdef(symbol, (INT) symbollen);
			symtableptr->type = symtype;
			break;
		case LABEL_METHODREF:
			error(ERROR_UNKNOWNMETHOD);
			return(RC_ERROR);
		default:
			death(DEATH_INTERNAL4);
	} // end of switch on 'flag'
	symtableptr->classid = workclsid;
	if (xrefflg) xrefout(symbol, flag);
	return(0);
}

/* GETPSYM */
/* determine if a program execution label is defined */
/* return RC_ERROR if symbol not found, else return 0 */
INT getpsym(CHAR *symbol)
{
	UINT hashval;
	USHORT workclsid, symbollen;
	SYMINFO *symtableptr;

	if(!symbol[0]) return(RC_ERROR);

	/* find a hash value to reference the symbol table with */
	hashval = symhashfunct(symbol, &symbollen);

	if (classlevel) workclsid = curclassid;
	else workclsid = 0;

	if (NULL == symtablebase[hashval]) return(RC_ERROR);
	else {
		symtableptr = symtablebase[hashval] + symtableoffset[hashval];
		while (symtableptr != NULL) {
			if (symtableptr->len == symbollen &&
			    !memcmp(symtableptr->nameptr, symbol, symbollen)) {
				do {
					if (symtableptr->type > TYPE_MAXDATAVAR &&
					    symtableptr->type != TYPE_CLASS &&
					    symtableptr->classid == workclsid) {
						if (symtableptr->type == TYPE_PLABEL) {
							if (symtableptr->u1.exelabelref != 0xFFFF &&
							    (*exelabeltable)[symtableptr->u1.exelabelref] < 0x01000000) {
								symtype = symtableptr->type;
								return(0);
							}
							if (symtableptr->u2.simpleref != (UINT) 0xFFFFFFFF &&
							    (*simplelabeltable)[symtableptr->u2.simpleref] < 0x01000000) {
								symtype = symtableptr->type;
								return(0);
							}
							return(RC_ERROR);
						}
						if (symtableptr->type != TYPE_PLABELVAR) return(0);
						if (symtableptr->rlevel <= rlevel && rlevelid[symtableptr->rlevel] == symtableptr->routineid) {
							symvalue = symtableptr->u1.exelabelref;
							symtype = TYPE_PLABELVAR;
							return(0);
						}
					}
					symtableptr = symtableptr->next;	/* get next SYMINFO w/same name */
				} while (symtableptr != NULL);
				return(RC_ERROR);
			}
			symtableptr = symtableptr->nexthash;	/* get next SYMINFO w/same hash value */
		}
	}
	return(RC_ERROR);
}

/* ADDMTDDEF */
/* Add a symbol to method definition table with current codeadr value */
static void addmtddef(CHAR *symbol, INT symlen)
{
	if (mtddefsize + symlen + 4 > mtddefalloc) dbcmem(&mtddeftable, mtddefalloc += METHODDEFINCREMENT);
	(*mtddeftable)[mtddefsize] = (UCHAR) codeadr;
	(*mtddeftable)[mtddefsize + 1] = (UCHAR) (codeadr >> 8);
	(*mtddeftable)[mtddefsize + 2] = (UCHAR) (codeadr >> 16);
	mtddefsize += 3;
	memcpy((*mtddeftable) + mtddefsize, symbol, (size_t) ++symlen);
	mtddefsize += symlen;
	symtype = TYPE_PLABELMDEF;
}

/* ADDCLSXREF */
/* Add a symbol to class method/external reference table */
static INT addclsxref(CHAR *symbol, INT symlen, INT refflag)
{
	INT retval;

	symlen++;
	if (clsxrefsize + symlen + 1 > clsxrefalloc) dbcmem(&clsxreftable, clsxrefalloc += METHODREFINCREMENT);
	retval = 0x00800000 + clsxrefsize - 4;
	(*clsxreftable)[clsxrefsize++] = refflag;
	memcpy((*clsxreftable) + clsxrefsize, symbol, (size_t) symlen);
	clsxrefsize += symlen;
	return(retval);
}

/* ADDDATAMBR */
/* Add a variable to the data member table that belongs to the current class */
/* NOTE: symvalue must contain the correct run-time address of data variable */
/* in the class data area										  */
static void adddatambr(CHAR *symbol, INT symlen)
{
	if (datambrsize + symlen + 4 >= datambralloc) dbcmem(&datambrtable, datambralloc += DATAMBRINCREMENT);
	(*datambrtable)[datambrsize] = (UCHAR) symvalue;
	(*datambrtable)[datambrsize + 1] = (UCHAR) (symvalue >> 8);
	(*datambrtable)[datambrsize + 2] = (UCHAR) (symvalue >> 16);
	datambrsize += 3;
	memcpy((*datambrtable) + datambrsize, symbol, (size_t) ++symlen);
	datambrsize += symlen;
}

/* ADDXDEF */
/* Add a symbol to external definition table with current codeadr value */
static void addxdef(CHAR *symbol, INT symlen)
{
	if (xdefsize + symlen + 4 >= xdefalloc) dbcmem(&xdeftable, xdefalloc += EXTERNALDEFINCREMENT);
	(*xdeftable)[xdefsize] = (UCHAR) codeadr;
	(*xdeftable)[xdefsize + 1] = (UCHAR) (codeadr >> 8);
	(*xdeftable)[xdefsize + 2] = (UCHAR) (codeadr >> 16);
	memcpy((*xdeftable) + xdefsize + 3, symbol, (size_t) symlen + 1);
	xdefsize += symlen + 4;
	symtype = TYPE_PLABELXDEF;
	externalDefinitionsTableCountOfEntries++;
}

/* ADDXREF */
/* Add a symbol to external reference table */
static INT addxref(CHAR *symbol, INT symlen, INT refflag)
{
	INT retval;

	if (classlevel) return(addclsxref(symbol, symlen, refflag));
 	symlen++;
	if (NULL == xreftable || xrefsize + symlen + 1 >= xrefalloc) {
		dbcmem(&xreftable, xrefalloc += 512);
	}
	retval = 0x00800000 + xrefsize - 4;
	(*xreftable)[xrefsize++] = refflag;
	memcpy((*xreftable) + xrefsize, symbol, (size_t) symlen);
	xrefsize += symlen;
	externalReferencesTableCountOfEntries++;
	return(retval);
}

/* VEXPAND */
/* expand codebuf to expand verb code from 1 to 2 bytes */
void vexpand()
{
	memmove(&codebuf[2], &codebuf[1], (size_t) codebufcnt - 1);
	codebufcnt++;
}

/**
 * GETLABELNUM
 *
 * Return a new simple label number.
 * Which is an index into simplelabeltable biased by SIMPLELABELBIAS.
 * Return values can range from 0x8000 to 0xffff7fff (UINT_MAX - 0x8000)
 */
UINT getlabelnum()
{
	if (simplelabelnext > UINT_MAX - SIMPLELABELBIAS) death(DEATH_INTERNAL3);
	EXTENDSIMPLETABLE();
	(*simplelabeltable)[simplelabelnext] = 0x01000000;
	return (SIMPLELABELBIAS + simplelabelnext++);
}

/* DEFLABELNUM */
/* define the offset of a simple label */
void deflabelnum(UINT labelnum)
{
	if (labelnum < SIMPLELABELBIAS) death(DEATH_INTERNAL3);
	(*simplelabeltable)[labelnum - SIMPLELABELBIAS] = codeadr + codebufcnt;
}

/* MAKEGOTO */
/* create the unconditional goto statement code */
void makegoto(UINT labelnum)
{
	INT offset;
	if (labelnum < SIMPLELABELBIAS) {  /* label table type label */
		if (labelnum < 1023) putcodehhll(0x7400 + labelnum);
		else {
			putcodehhll(0x77FF);
			putcodehhll(labelnum);
		}
	}
	else {
		if (labelnum == UINT_MAX) return;  /* can happen in certain error situations */
		putcode1(201);		/* _GOTO <pgm offset> */
		labelnum -= SIMPLELABELBIAS;  /* 32768 */
		offset = (*simplelabeltable)[labelnum];
		if (offset >= 0x01000000) {
			EXTENDFIXUPTABLE();
			move4hhll(labelnum, *simplefixuptable + simplefixupnext);
			move4hhll(codeadr + codebufcnt, *simplefixuptable + simplefixupnext + sizeof(UINT));
			simplefixupnext += 2 * sizeof(UINT);
		}
		putcodelmh(offset);
	}
}

/* MAKEMETHODREF */
/* create the method reference code */
INT makemethodref(UINT labelnum)
{
	if (symtype != TYPE_PLABELMREF) {
		error(ERROR_UNKNOWNMETHOD);
		return(RC_ERROR);
	}
	putcode1(207);
	putcodehhll(labelnum);
	return(0);
}

/* MAKEPOFFSET */
void makepoffset(UINT labelnum)
{
	INT offset;
	labelnum -= SIMPLELABELBIAS;
	offset = (*simplelabeltable)[labelnum];
	if (offset >= 0x01000000) {
		EXTENDFIXUPTABLE();
		move4hhll(labelnum, *simplefixuptable + simplefixupnext);
		move4hhll(codeadr + codebufcnt, *simplefixuptable + simplefixupnext + sizeof(UINT));
		simplefixupnext += 2 * sizeof(UINT);
	}
	putcodelmh(offset);
}

/* MAINPOFFSET */
void mainpoffset(UINT labelnum)
{
	INT offset;

	labelnum -= SIMPLELABELBIAS;
	offset = (*simplelabeltable)[labelnum];
	if (offset >= 0x01000000 /* 32,768 */) {
		EXTENDFIXUPTABLE();
		move4hhll(labelnum, *simplefixuptable + simplefixupnext);
		move4hhll(codeadr, *simplefixuptable + simplefixupnext + sizeof(UINT));
		simplefixupnext += 2 * sizeof(UINT);
	}
	putmainlmh(offset);
}

/* MAKECONDGOTO */
/* create the goto statement code */
void makecondgoto(UINT labelnum, INT cond)
{
	USHORT n;
	INT offset;

	if (labelnum < SIMPLELABELBIAS) {  /* label table type label */
		if (cond > 7) n = 0x7C00;
			else n = 0xA000 + (USHORT) (cond << 10);
		if (labelnum < 1023) putcodehhll(n + labelnum);
		else {
			putcodehhll((USHORT) (n + 0x03FF));
			putcodehhll(labelnum);
		}
		if (cond > 7) putcode1((UCHAR) cond);
	}
	else {  /* simple label */
		putcode1(202);
		putcode1((UCHAR) cond);
		labelnum -= SIMPLELABELBIAS;
		offset = (*simplelabeltable)[labelnum];
		if (offset >= 0x01000000) {
			EXTENDFIXUPTABLE();
			move4hhll(labelnum, *simplefixuptable + simplefixupnext);
			move4hhll(codeadr + codebufcnt, *simplefixuptable + simplefixupnext + sizeof(UINT));
			simplefixupnext += 2 * sizeof(UINT);
		}
		putcodelmh(offset);
	}
}

/* MAINCONDGOTO */
/* create the goto statement code */
void maincondgoto(UINT labelnum, INT cond)
{
	USHORT n;
	INT offset;

	if (labelnum < SIMPLELABELBIAS) {  /* label table type label */
		if (cond > 7) n = 0x7C00;
			else n = 0xA000 + (USHORT) (cond << 10);
		if (labelnum < 1023) putmainhhll(n + labelnum);
		else {
			putmainhhll((USHORT) (n + 0x03FF));
			putmainhhll(labelnum);
		}
		if (cond > 7) putmain1((UCHAR) cond);
	}
	else {  /* simple label */
		putmain1(202);
		putmain1((UCHAR) cond);
		labelnum -= SIMPLELABELBIAS;
		offset = (*simplelabeltable)[labelnum];
		if (offset >= 0x01000000) {
			EXTENDFIXUPTABLE();
			move4hhll(labelnum, *simplefixuptable + simplefixupnext);
			move4hhll(codeadr, *simplefixuptable + simplefixupnext + sizeof(UINT));
			simplefixupnext += 2 * sizeof(UINT);
		}
		putmainlmh(offset);
	}
}

/**
 * MAKENLIT
 * make a numeric literal, reuse "0" to "100" and "-1" to "-9"
 */
INT makenlit(CHAR *s)
{
	INT m, n;
	INT x, *reuse;

	if (classlevel) reuse = classnlitreuse;
	else reuse = nlitreuse;
	n = (INT)strlen(s);
	if (n && s[n - 1] == '.') n--;
	if (n == 1) {
		m = (UCHAR) s[0] - '0';
		if (reuse[m] != -1) return(reuse[m]);
		reuse[m] = dataadr;
	}
	else if (n == 2) {
		if (s[0] >= '1' && s[0] <= '9') {
			m = ((UCHAR) s[0] - '0') * 10 + (UCHAR) s[1] - '0';
			if (reuse[m] != -1) return(reuse[m]);
			reuse[m] = dataadr;
		}
		else if (s[0] == '-') {
			m = (UCHAR) s[1] - '0';
			if (reuse[m + 101] != -1) return(reuse[m + 101]);
			reuse[m + 101] = dataadr;
		}
	}
	else if (n == 3 && !strcmp(s, "100")) {
		if (reuse[100] != -1) return(reuse[100]);
		reuse[100] = dataadr;
	}
	putdatahhll((USHORT) (0xE000 + n));
	putdata((UCHAR *) s, n);
	x = dataadr;
	dataadr += n + 2;
	return(x);
}

/* VMAKENLIT */
INT vmakenlit(INT value)  /* same as makenlit, except uses a value instead of string */
{
	CHAR work[12];
	INT i;

	work[11] = 0;
	if (value < 0) {
		value = -value;
		if (value < 10) {
			work[9] = '-';
			work[10] = (CHAR) value + '0';
			return(makenlit(&work[9]));
		}
		else {
			for (i = 10; i > 0 && value; i--) {
				work[i] = (CHAR) ((value % 10) + '0');
				value /= 10;
			}
			work[i] = '-';
			return(makenlit(&work[i]));
		}
	}
	else {
		if (value < 10) {
			work[10] = (CHAR) value + '0';
			return(makenlit(&work[10]));
		}
		else {
			for (i = 10; i >= 0 && value; i--) {
				work[i] = (CHAR) ((value % 10) + '0');
				value /= 10;
			}
			return(makenlit(&work[i+1]));
		}
	}
}

/* MAKECLIT */
/* make a character literal, reuse existing literal if applicable */
INT makeclit(UCHAR *charlit)
{
	UINT hashval, prevval, clitlen, curshift;
	INT clitadr, clitref, prevrefpos;
	INT *hashtab;

	prevval = hashval = 0;
	for (curshift = clitlen = 0; charlit[clitlen]; clitlen++) {
		hashval += ((UINT) charlit[clitlen] << curshift++);
		if (hashval < prevval) {
			hashval = prevval % CLITHASHSIZE + (UINT) charlit[clitlen];
			curshift = 1;
		}
		prevval = hashval;
	}
	hashval += clitlen;
	if (hashval < prevval) {
		hashval = prevval % CLITHASHSIZE + (UINT) charlit[clitlen] + clitlen;
	}
	hashval %= CLITHASHSIZE;
	clitlen++;
	if (classlevel) hashtab = classclithash;
	else hashtab = clithashtab;

	if ((clitref = hashtab[hashval]) >= 0) {
		while ((INT)(clitref + clitlen) < clittablesize) {
			if (!memcmp(charlit, &(*clittable)[clitref], clitlen)) {
				clitadr = load4hhll(&(*clittable)[clitref + clitlen]);
				return(clitadr);
			}
			prevrefpos = clitref + (INT)strlen((CHAR *)(&(*clittable)[clitref])) + 5;
			clitref = load4hhll(&(*clittable)[prevrefpos]);
			if (!clitref) {
				move4hhll(clittablesize, &(*clittable)[prevrefpos]);
				break;
			}
		}
	}
	else hashtab[hashval] = clittablesize;
	if ((INT)(clittablesize + clitlen + 8) > clittablealloc) dbcmem(&clittable, (UINT) (clittablealloc += CLITTABLEINCREMENT));
	memcpy(&(*clittable)[clittablesize], charlit, clitlen);
	clittablesize += clitlen;
	move4hhll(dataadr, &(*clittable)[clittablesize]);
	clittablesize += 4;
	move4hhll(0, &(*clittable)[clittablesize]);
	clittablesize += 4;

	clitlen--;
	putdatahhll((USHORT) (0xE100 + clitlen));
	putdata((UCHAR *) charlit, (INT) clitlen);
	clitadr = dataadr;
	dataadr += clitlen + 2;
	return(clitadr);
}

/* PUTARRAYSHAPE */
/* save an array shape by identifier v */
void putarrayshape(INT v, UINT *shape)
{
	INT i1;
	UCHAR *ptr;

 	if (!recdefonly) {
		if (ashapecnt >= ashapemax) {
			ashapemax += 50 * (sizeof(INT) + 4 * sizeof(UINT));
			dbcmem(&ashapetable, ashapemax);
		}
		ptr = (*ashapetable) + ashapecnt;
		move4hhll(v, ptr);
		ptr += 4;
		memcpy(ptr, shape, 4 * sizeof (UINT));
		ashapecnt += (sizeof(INT) + 4 * sizeof(UINT));
	}
	if (recordlevel) for (i1 = 0; i1 < 4; i1++) putdefllhh(shape[i1]);
}

/* GETARRAYSHAPE */
/* lookup an array shape by identifier v */
void getarrayshape(INT v, UINT *shape)
{
	UCHAR *ptr, *endptr;
	INT u;

	ptr = (*ashapetable);
	endptr = (*ashapetable) + ashapecnt;
	while (ptr != endptr) {
		u = load4hhll(ptr);
		if (v == u) {
			ptr += 4;
			memcpy(shape, ptr, 4 * sizeof(UINT));
			return;
		}
		ptr += (4 + 4 * sizeof(UINT));
	}
	death(DEATH_INTERNAL1);
}

/* PUTDATA1 */
/* write one byte to data area */
void putdata1(UINT c)
{
 	if (!recdefonly) {
		if (classlevel) putclsdata1(c);
		else {
			if (databufcnt >= databufsize) putdataout();
			(*databuf)[databufcnt++] = c;
		}
	}
	if (recordlevel) putdef1(c);	/* update record definition */
}

/* PUTDATAHHLL */
/* write two bytes to data area */
void putdatahhll(UINT n)
{
	if (!recdefonly) {
		if (classlevel) putclsdatahhll(n);
		else {
			if (databufcnt + 1 >= databufsize) putdataout();
			(*databuf)[databufcnt] = (UCHAR) (n >> 8);
			(*databuf)[databufcnt + 1] = (UCHAR) n;
			databufcnt += 2;
		}
	}
	if (recordlevel) putdefhhll(n);	/* update record definition */
}

/* PUTDATALLHH */
/* write two bytes to data area */
void putdatallhh(UINT n)
{
	if (!recdefonly) {
		if (classlevel) putclsdatallhh(n);
		else {
			if (databufcnt + 1 >= databufsize) putdataout();
			(*databuf)[databufcnt] = (UCHAR) n;
			(*databuf)[databufcnt + 1] = (UCHAR) (n >> 8);
			databufcnt += 2;
		}
	}
	if (recordlevel) putdefllhh(n);	/* update record definition */
}

/* PUTDATA */
/* write n bytes to data area */
void putdata(UCHAR *data, INT n)
{
	INT i1, i2;

	if (!recdefonly) {
		if (classlevel) putclsdata(data, n);
		else {
			for (i1 = 0; ; ) {
				i2 = n - i1;
				if ((UINT) i2 > databufsize - databufcnt) i2 = databufsize - databufcnt;
				memcpy(*databuf + databufcnt, data + i1, (size_t) i2);
				databufcnt += i2;
				if ((i1 += i2) == n) break;
				putdataout();
			}
		}
	}
	if (recordlevel) putdef(data, (UINT) n);	/* update record definition */
}

/* PUTDATALMH */
/* write three bytes to data area */
static void putdatalmh(INT n)
{
	if (!recdefonly) {
		if (classlevel) putclsdatalmh(n);
		else {
			if (databufcnt + 2 >= databufsize) putdataout();
			(*databuf)[databufcnt] = (UCHAR) n;
			(*databuf)[databufcnt + 1] = (UCHAR) ((UINT) n >> 8);
			(*databuf)[databufcnt + 2] = (UCHAR) (n >> 16);
			databufcnt += 3;
		}
	}
	if (recordlevel) putdeflmh(n);	/* update record definition */
}

/* PUTDATAOUT */
/* write out the data area buffer */
static void putdataout()
{
	UINT newbufsize;

	if (!databufhandle) {
		newbufsize = (UINT) databufsize + DATABUFINCREMENT;
		if (newbufsize < UINT_MAX && !memchange(databuf, (INT) newbufsize, 0)) {
			databufsize = (UINT) newbufsize;
			return;
		}
		memfree(memreserve1);
		memreserve1 = NULL;
		databufhandle = tempfile(datawrkname, 2);
		datafilepos = 0;
	}
	if (databufhandle) {
		if (databufcnt) {
			if (fiowrite(databufhandle, datafilepos, *databuf, (INT) databufcnt) < 0) death(DEATH_BADWRKWRITE);
			datafilepos += databufcnt;
			databufcnt = 0;
		}
		if (databufsize != MAINBUFINCREMENT) memchange(databuf, (INT) (databufsize = DATABUFINCREMENT), 0);
	}
}

/* PUTCLSDATA1 */
/* write one byte to class data area */
static void putclsdata1(UINT data)
{
	if (clsdatabufcnt >= CLASSDATASIZE) putclsdataout();
	(*clsdatabuf)[clsdatabufcnt++] = data;
}

/* PUTCLSDATAHHLL */
/* write two bytes to class data area */
static void putclsdatahhll(UINT data)
{
	if (clsdatabufcnt >= CLASSDATASIZE - 1) putclsdataout();
	(*clsdatabuf)[clsdatabufcnt] = (UCHAR) (data >> 8);
	(*clsdatabuf)[clsdatabufcnt + 1] = (UCHAR) data;
	clsdatabufcnt += 2;
}

/* PUTCLSDATALLHH */
/* write two bytes to class data area */
static void putclsdatallhh(UINT data)
{
	if (clsdatabufcnt >= CLASSDATASIZE - 1) putclsdataout();
	(*clsdatabuf)[clsdatabufcnt] = (UCHAR) data;
	(*clsdatabuf)[clsdatabufcnt + 1] = (UCHAR) (data >> 8);
	clsdatabufcnt += 2;
}

/* PUTCLSDATA */
/* write n bytes to class data area */
static void putclsdata(UCHAR *data, INT n)
{
	if (clsdatabufcnt + n >= CLASSDATASIZE) putclsdataout();
	memcpy(&(*clsdatabuf)[clsdatabufcnt], data, (size_t) n);
	clsdatabufcnt += n;
}

/* PUTCLSDATALMH */
/* write three bytes to class data area */
static void putclsdatalmh(INT data)
{
	if (clsdatabufcnt > CLASSDATASIZE - 2) putclsdataout();
	(*clsdatabuf)[clsdatabufcnt] = (UCHAR) data;
	(*clsdatabuf)[clsdatabufcnt + 1] = (UCHAR) ((UINT) data >> 8);
	(*clsdatabuf)[clsdatabufcnt + 2] = (UCHAR) (data >> 16);
	clsdatabufcnt += 3;
}

/* PUTCLSDATAOUT */
/* write clsdatabuf[] buffer to .DBC file */
static void putclsdataout()
{
	if (fiowrite(dbcfilenum, dbcfilepos, *clsdatabuf, (INT) clsdatabufcnt) < 0) death(DEATH_BADDBCWRITE);
	dbcfilepos += clsdatabufcnt;
	clsdatabufout += clsdatabufcnt;
	clsdatabufcnt = 0;
}

/* PUTCODE1 */
/* put one byte into the secondary code buffer */
void putcode1(UINT c)
{
	codebuf[codebufcnt++] = c;
}

/* PUTCODELLHH */
/* put two bytes into the secondary code buffer */
void putcodellhh(UINT n)
{
	codebuf[codebufcnt] = (UCHAR) n;
	codebuf[codebufcnt + 1] = (UCHAR) (n >> 8);
	codebufcnt += 2;
}

/* PUTCODELMH */
/* put three bytes into the secondary code buffer */
void putcodelmh(INT n)
{
	codebuf[codebufcnt] = (UCHAR) n;
	codebuf[codebufcnt + 1] = (UCHAR) ((UINT) n >> 8);
	codebuf[codebufcnt + 2] = (UCHAR) (n >> 16);
	codebufcnt += 3;
}

/* PUTCODEHHLL */
/* put two bytes into the secondary code buffer */
void putcodehhll(UINT n)
{
	codebuf[codebufcnt] = (UCHAR) (n >> 8);
	codebuf[codebufcnt + 1] = (UCHAR) n;
	codebufcnt += 2;
}

/* PUTCODEADR */
/* put two or four bytes into the secondary code buffer */
void putcodeadr(INT adr)
{
	if (adr < 0xC000) {
		codebuf[codebufcnt] = (UCHAR) (adr >> 8);
		codebuf[codebufcnt + 1] = (UCHAR) adr;
		codebufcnt += 2;
	}
	else {
		codebuf[codebufcnt] = 0xC1;
		codebuf[codebufcnt + 1] = (UCHAR) (adr >> 16);
		codebuf[codebufcnt + 2] = (UCHAR) (adr >> 8);
		codebuf[codebufcnt + 3] = (UCHAR) adr;
		codebufcnt += 4;
	}
}

/* PUTMAIN1 */
/* put one byte into the main code buffer */
void putmain1(UINT c)
{
	if (mainbufcnt >= mainbufsize) savemain();
	(*mainbufpptr)[mainbufcnt++] = c;
	codeadr++;
}

/* PUTMAINHHLL */
/* put two bytes into the main code buffer */
void putmainhhll(UINT n)
{
	UCHAR *mainbufptr;

	if (mainbufcnt >= mainbufsize - 1) savemain();
	mainbufptr = *mainbufpptr;
	mainbufptr[mainbufcnt] = (UCHAR) (n >> 8);
	mainbufptr[mainbufcnt + 1] = (UCHAR) n;
	mainbufcnt += 2;
	codeadr += 2;
}

/* PUTMAINLMH */
/* put three bytes into the main code buffer */
void putmainlmh(INT n)
{
	UCHAR *mainbufptr;

	if (mainbufcnt >= mainbufsize - 2) savemain();
	mainbufptr = *mainbufpptr;
	mainbufptr[mainbufcnt] = (UCHAR) n;
	mainbufptr[mainbufcnt + 1] = (UCHAR) (n >> 8);
	mainbufptr[mainbufcnt + 2] = (UCHAR) (n >> 16);
	mainbufcnt += 3;
	codeadr += 3;
}

/* PUTMAINADR */
/* put two or four bytes into the main code buffer */
void putmainadr(INT adr)
{
	UCHAR *mainbufptr;

	if (mainbufcnt >= mainbufsize - 3) savemain();
	mainbufptr = *mainbufpptr;
	if (adr < 0xC000) {
		mainbufptr[mainbufcnt] = (UCHAR) (adr >> 8);
		mainbufptr[mainbufcnt + 1] = (UCHAR) adr;
		mainbufcnt += 2;
		codeadr += 2;
	}
	else {
		mainbufptr[mainbufcnt] = 0xC1;
		mainbufptr[mainbufcnt + 1] = (UCHAR) (adr >> 16);
		mainbufptr[mainbufcnt + 2] = (UCHAR) (adr >> 8);
		mainbufptr[mainbufcnt + 3] = (UCHAR) adr;
		mainbufcnt += 4;
		codeadr += 4;
	}
}

/* PUTCODEOUT */
/* write out code buffer	into the main code buffer */
void putcodeout()
{
	if ((INT) mainbufcnt + codebufcnt	>= (INT) mainbufsize + 1) savemain();
	memcpy(&((*mainbufpptr)[mainbufcnt]), codebuf, (size_t) codebufcnt);
	mainbufcnt += codebufcnt;
	codeadr += codebufcnt;
	codebufcnt = 0;
}

/* SAVEMAIN */
/* mainbuf is full, expand it or save it to work file */
static void savemain()
/* return 1 if workfile is being used, else return 0 */
{
	UINT newbufsize;

	if (!mainbufhandle) {
		newbufsize = (UINT) mainbufsize + MAINBUFINCREMENT;
		if (newbufsize < UINT_MAX && !memchange(mainbufpptr, (INT) newbufsize, 0)) {
			mainbufsize = (UINT) newbufsize;
			return;
		}
		memfree(memreserve2);
		memreserve2 = NULL;
		mainbufhandle = tempfile(mainwrkname, 2);
		mainfilepos = 0;
	}
	if (mainbufhandle) {
		if (mainbufcnt) {
			if (fiowrite(mainbufhandle, mainfilepos, *mainbufpptr, (INT) mainbufcnt) < 0) death(DEATH_BADWRKWRITE);
			mainfilepos += mainbufcnt;
			mainbufcnt = 0;
		}
		if (mainbufsize != MAINBUFINCREMENT) memchange(mainbufpptr, (INT) (mainbufsize = MAINBUFINCREMENT), 0);
	}
}

/* DBCMEM */
/* allocate memory with memalloc or memchange */
void dbcmem(UCHAR ***ppptr, UINT size)
{
	while (TRUE) {
		if (*ppptr == NULL) {
			if ((*ppptr = memalloc((INT) size, 0)) != NULL) return;
		}
		else if (!memchange(*ppptr, (INT) size, 0)) return;
		if (!databufhandle) {
			memfree(memreserve1);
			memreserve1 = NULL;
			databufhandle = tempfile(datawrkname, 2);
			if (fiowrite(databufhandle, (OFFSET) 0, *databuf, (INT) databufcnt) < 0) death(DEATH_BADWRKWRITE);
			datafilepos = databufcnt;
			if (databufsize != DATABUFINCREMENT) memchange(databuf, (INT) (databufsize = DATABUFINCREMENT), 0);
			databufcnt = 0;
		}
		else if (!mainbufhandle) {
			memfree(memreserve2);
			memreserve2 = NULL;
			mainbufhandle = tempfile(mainwrkname, 2);
			if (fiowrite(mainbufhandle, (OFFSET) 0, *mainbufpptr, (INT) mainbufcnt) < 0) death(DEATH_BADWRKWRITE);
			mainfilepos = mainbufcnt;
			if (mainbufsize != MAINBUFINCREMENT) memchange(mainbufpptr, (INT) (mainbufsize = MAINBUFINCREMENT), 0);
			mainbufcnt = 0;
		}
		else break;
	}
	death(DEATH_NOMEM);
}

/* STARTDBC */
/* start writing the .dbc file */
static void startdbc()
{
	UCHAR dbchdr[22];

	dbchdr[0] = dbchdr[1] = dbchdr[2] = dbchdr[3] = 0xFF;	/* mark as bad compile */
	msctimestamp(createtime);
	memcpy(&dbchdr[4], createtime, 16);
	dbchdr[20] = 0;
	dbchdr[21] = 0;
	if (fiowrite(dbcfilenum, dbcfilepos, dbchdr, 22) < 0) death(DEATH_BADDBCWRITE);
	dbcfilepos += 22;
#ifdef SPECIAL
	putdata("             ", 13);
#endif
}

/**
 * WRITEDBC
 * finish writing the .dbc file
 */
static void writedbc()
{
	INT i1, curlabel, coderef, fixuppos;
	INT bytesread, totalsegsize, exelabel, symblk, symoffset;
	UINT simpleref;
	SYMINFO *symtableptr;
	UCHAR work16[16];
	CHAR workIE8[MAXINTERNALERROR8SIZE];

	putdata1(0xF5);		/* end of main data area indicator */
	/* add execution label table segment to the end of the main data area segment */
	totalsegsize = (INT) datafilepos + databufcnt - 4;
	if (exelabelnext) {
		putdata1(0x01);			/* segment identifier byte */
		putdatalmh(exelabelnext * 3);	/* segment size */
		executionLabelTableCount = exelabelnext;
	}
	for (curlabel = 0; curlabel < (INT) exelabelnext; curlabel++) {
		exelabel = (*exelabeltable)[curlabel];
		if ((exelabel & 0xFF000000) == 0x01000000) {  /* undefined or external reference */
			i1 = (exelabel & 0x00FFFFFF) - 1;
			symblk = i1 >> SYMBLOCKSHIFT;
			symoffset = i1 - (symblk << SYMBLOCKSHIFT);
			symtableptr = symblklist[symblk] + symoffset;
			if (symtableptr->type == TYPE_PLABELXREF) i1 = REF_EXTERNAL;
			else if (symtableptr->type == TYPE_PLABELMREF) i1 = REF_METHOD;
			else {
				strcpy((CHAR *) token, symtableptr->nameptr);
				error(ERROR_LABELNOTFOUND);
			}
			exelabel = addxref(symtableptr->nameptr, symtableptr->len, i1);
		}
		putdatalmh(exelabel);
	}

	/* write out main data area (along with execution label table) */
	dataadr++;
	if (databufhandle) {
		if (databufcnt) {
			if (fiowrite(databufhandle, datafilepos, *databuf, (INT) databufcnt) < 0) death(DEATH_BADWRKWRITE);
			datafilepos += databufcnt;
 		}
		datafilepos = 0;
		do {
			bytesread = fioread(databufhandle, datafilepos, *databuf, (INT) databufsize);
			if (bytesread <= 0) break;
			if (!datafilepos) {
				(*databuf)[0] = 0x00;				/* tag byte for main data segment */
				(*databuf)[1] = (UCHAR) totalsegsize;	/* size, in bytes, of data area in .DBC file */
				(*databuf)[2] = (UCHAR) (totalsegsize >> 8);
				(*databuf)[3] = (UCHAR) (totalsegsize >> 16);
				(*databuf)[4] = (UCHAR) dataadr;		/* size, in bytes, of data area at run-time */
				(*databuf)[5] = (UCHAR) (dataadr >> 8);
				(*databuf)[6] = (UCHAR) (dataadr >> 16);
			}
			if (fiowrite(dbcfilenum, dbcfilepos, *databuf, bytesread) < 0) death(DEATH_BADDBCWRITE);
			dbcfilepos += bytesread;
			datafilepos += bytesread;
		} while (bytesread == (INT) databufsize);
		fiokill(databufhandle);
	}
	else {
		(*databuf)[0] = 0x00;				/* tag byte for main data segment */
		(*databuf)[1] = (UCHAR) totalsegsize;	/* size, in bytes, of data area in .DBC file */
		(*databuf)[2] = (UCHAR) (totalsegsize >> 8);
		(*databuf)[3] = (UCHAR) (totalsegsize >> 16);
		(*databuf)[4] = (UCHAR) dataadr;		/* size, in bytes, of data area at run-time */
		(*databuf)[5] = (UCHAR) (dataadr >> 8);
		(*databuf)[6] = (UCHAR) (dataadr >> 16);
		if (fiowrite(dbcfilenum, dbcfilepos, *databuf, (INT) databufcnt) < 0) death(DEATH_BADDBCWRITE);
		dbcfilepos += databufcnt;
	}

	/* write out external definition table segment */
	if (xdefsize > 4) {  /* external definition table is non-null */
		(*xdeftable)[0] = 2;  /* id */
		xdefsize -= 4;
		(*xdeftable)[1] = (UCHAR) xdefsize;
		(*xdeftable)[2] = (UCHAR) ((UINT) xdefsize >> 8);
		(*xdeftable)[3] = (UCHAR) (xdefsize >> 16);
		externalDefinitionsTableSizeInBytes = xdefsize;
		if (fiowrite(dbcfilenum, dbcfilepos, *xdeftable, (INT) xdefsize + 4) < 0) death(DEATH_BADDBCWRITE);
		dbcfilepos += xdefsize + 4;
	}

	/* wriet out external reference table segment */
	if (xrefsize > 4) {  /* external reference table is non-null */
		(*xreftable)[0] = 3;  /* id */
		xrefsize -= 4;
		externalReferencesTableSizeInBytes = xrefsize;
		(*xreftable)[1] = (UCHAR) xrefsize;
		(*xreftable)[2] = (UCHAR) ((UINT) xrefsize >> 8);
		(*xreftable)[3] = (UCHAR) (xrefsize >> 16);
		if (fiowrite(dbcfilenum, dbcfilepos, *xreftable, (INT) xrefsize + 4) < 0) death(DEATH_BADDBCWRITE);
		dbcfilepos += xrefsize + 4;
	}

	/* write out program code area segment */
 	putmain1(0x64);	/* write implicit STOP code */
	if (mainbufhandle) {
		if (mainbufcnt) {
			if (fiowrite(mainbufhandle, mainfilepos, *mainbufpptr, (INT) mainbufcnt) < 0) death(DEATH_BADDBCWRITE);
			mainfilepos += mainbufcnt;
		}
		totalsegsize = (INT) mainfilepos - 4;
		mainfilepos = 0;
		fixuppos = 0;
		while (TRUE) {
			bytesread = fioread(mainbufhandle, mainfilepos, *mainbufpptr, (INT) mainbufsize);
			if (bytesread <= 0) break;
			if (!mainfilepos) {
				(*mainbufpptr)[0] = 0x04;				/* tag byte for program code segment */
				(*mainbufpptr)[1] = (UCHAR) totalsegsize;	/* size, in bytes, of data area in .DBC file */
				(*mainbufpptr)[2] = (UCHAR) (totalsegsize >> 8);
				(*mainbufpptr)[3] = (UCHAR) (totalsegsize >> 16);
			}
			for ( ; fixuppos < simplefixupnext; fixuppos += 2 * sizeof(UINT)) {
				coderef = load4hhll(*simplefixuptable + fixuppos + sizeof(UINT)) + 4;
				if (coderef >= mainfilepos + bytesread) break;
				if (coderef + 2 >= mainfilepos + bytesread) {
					bytesread -= 2;
					break;
				}
				simpleref = load4hhll(*simplefixuptable + fixuppos);
				if ((*simplelabeltable)[simpleref] < 0x01000000) {
					(*mainbufpptr)[coderef - mainfilepos] = (UCHAR) (*simplelabeltable)[simpleref];
	 				(*mainbufpptr)[coderef - mainfilepos + 1] = (UCHAR) ((*simplelabeltable)[simpleref] >> 8);
	 				(*mainbufpptr)[coderef - mainfilepos + 2] = (UCHAR) ((*simplelabeltable)[simpleref] >> 16);
				}
				else {
					symoffset = ((*simplelabeltable)[simpleref] & 0x00FFFFFF) - 1;
					symblk = symoffset >> SYMBLOCKSHIFT;
					symoffset -= (symblk << SYMBLOCKSHIFT);
					symtableptr = symblklist[symblk] + symoffset;
					if ((INT) strlen(symtableptr->nameptr) > 0) {
						strcpy((CHAR *) token, symtableptr->nameptr);
						error(ERROR_LABELNOTFOUND);
					}
				}
			}
			if (bytesread && fiowrite(dbcfilenum, dbcfilepos, *mainbufpptr, bytesread) < 0) death(DEATH_BADDBCWRITE);
			dbcfilepos += bytesread;
			mainfilepos += bytesread;
		}
		fiokill(mainbufhandle);
	}
	else {
		for (fixuppos = 0; fixuppos < simplefixupnext; fixuppos += 2 * sizeof(UINT)) {
			simpleref = load4hhll(*simplefixuptable + fixuppos);
			coderef = load4hhll(*simplefixuptable + fixuppos + sizeof(UINT)) + 4;
			/*assert(simpleref < simplelabelsize);*/
			if (simpleref >= simplelabelsize) {
				sprintf(workIE8, "INTERNAL ERROR 8: simpleref=%u is >= simplelabelsize=%u, fixuppos=%d",
						simpleref, simplelabelsize, fixuppos);
				setInteralError8Message(workIE8);
				death(DEATH_INTERNAL8);
			}
			if ((*simplelabeltable)[simpleref] < 0x01000000) {
				(*mainbufpptr)[coderef] = (UCHAR) (*simplelabeltable)[simpleref];
 				(*mainbufpptr)[coderef + 1] = (UCHAR) ((*simplelabeltable)[simpleref] >> 8);
 				(*mainbufpptr)[coderef + 2] = (UCHAR) ((*simplelabeltable)[simpleref] >> 16);
			}
			else if ((*simplelabeltable)[simpleref] != 0x01000000) {
				i1 = ((*simplelabeltable)[simpleref] & 0x00FFFFFF) - 1;
				symblk = i1 >> SYMBLOCKSHIFT;
				symoffset = i1 - (symblk << SYMBLOCKSHIFT);
				symtableptr = symblklist[symblk] + symoffset;
				if ((INT) strlen(symtableptr->nameptr) > 0) {
					strcpy((CHAR *) token, symtableptr->nameptr);
					error(ERROR_LABELNOTFOUND);
				}
			}
		}
		totalsegsize = mainbufcnt - 4;
		(*mainbufpptr)[0] = 0x04;				/* tag byte for program code segment */
		(*mainbufpptr)[1] = (UCHAR) totalsegsize;	/* size, in bytes, of data area in .DBC file */
		(*mainbufpptr)[2] = (UCHAR) (totalsegsize >> 8);
		(*mainbufpptr)[3] = (UCHAR) (totalsegsize >> 16);
		if (fiowrite(dbcfilenum, dbcfilepos, *mainbufpptr, (INT) mainbufcnt) < 0) death(DEATH_BADDBCWRITE);
		dbcfilepos += mainbufcnt;
	}
	if (errcnt) return;
#ifdef SPECIAL
	work16[0] = 0x0D;					/* size of extra header info (13 bytes) */
	work16[1] = 0x00;
	work16[2] = (UCHAR) dataadr;			/* move data area size to extra header info */
	work16[3] = (UCHAR) (dataadr >> 8);
	work16[4] = (UCHAR) (dataadr >> 16);
	memcpy(&work16[5], license, 10);		/* add license number to extra header info */
	work16[15] = 0x30;
	if (fiowrite(dbcfilenum, 20, work16, 16) < 0) death(DEATH_BADDBCWRITE);
	work16[0] = ~DX_MAJOR_VERSION;  /* XOR of major release number */
	work16[1] = DX_MAJOR_VERSION;   /* major release number */
#else
	/*
	work16[0] = DX_MAJOR_VERSION;   // major release number
	work16[1] = ~DX_MAJOR_VERSION;  // XOR of major release number
	*/
	// For 101 onwards, these two bytes will be indentical
	// work16[0] = work16[1] = DX_MAJOR_VERSION;   /* major release number */
	/*
	 * As of July 8 2022, Don wants this to be marked as '18' so the commercial runtime will accept it
	 */
	work16[0] = DX_18_MAJOR_VERSION;   // major release number
	work16[1] = ~DX_18_MAJOR_VERSION;  // XOR of major release number
#endif
	work16[2] = DX_MINOR_VERSION;   /* minor release number */
	work16[3] = 0;		/* compiled by DBCMP */
	if (fiowrite(dbcfilenum, (OFFSET) 0, work16, 4) < 0) death(DEATH_BADDBCWRITE);
}

/**
 * SRCOPEN
 * open a source program file
 * return 0 if ok, return 1 if file not found
 * return 2 for include too deep, return 3 for other errors
 */
INT srcopen(CHAR *filename)
{
	CHAR name1[MAXNAM], name2[MAXNAM + 20], outinc[7];
	UCHAR i, j, k;
	INT n;

	if (inclevel == MAXINC) {
		return(2);
	}
	strcpy(name1, filename);
	if (incopt) {
		for (i = 0; name1[i] && name1[i] != '/'; i++);
		if (name1[i] == '/') {
			if (incopt == 1) name1[i] = '\0';
			else if (incopt == 2) name1[i] = '.';
			else {
				for (j = i; name1[j] && name1[j] != '.'; j++);
				if (name1[j]) {  /* format is FILE/EXT.MEMBER */
					if (incopt == 3) {
						i = (UCHAR) strlen(&name1[++j]);
						memmove(name1, &name1[j], i);
						name1[i] = '\0';
					}
				}
			}
		}
	}
	if (inclevel == 1) j = 4;
	else if (inclevel > 1) j = 2;
	else j = 5;
	k = 0;
	strcpy(filename, name1);
	while (TRUE) {
		n = rioopen(name1, openmode | RIO_P_SRC | RIO_T_ANY, j, LINESIZE);
		if (n > 0) break;
		if (n == ERR_NOMEM) {
			if (--j) continue;
			death(DEATH_NOMEM);
		}
		if (n == ERR_FNOTF) {
			if (!k) {
				for (n = 0; isalnum(name1[n]); n++);
				if (name1[n]) k = 3;
				strcpy(name2, name1);
			}
			if (k < libcnt) {
				strcpy(name1, libname[k++]);
				strcat(name1, name2);
				strcat(name1, ")");
				continue;
			}
			return(1);
		}
		printf("%s\n", fioerrstr(n));
		return(3);
	}
	curfile = filetable[inclevel + 1] = n;
	inclevel++;
	if (inclevel > 0) {
		if (incnum == 999) incnum = 1;
		else incnum++;
		if (dspflags & (DSPFLAG_INCLUDE | DSPFLAG_COMPILE | DSPFLAG_START)) {
			if (fmtflag && prtlinenum > pagelen) newpage();
			outputs("INCLUSION ");
			mscitoa((INT) incnum, outinc);
			outputs(outinc);
			outputs(": ");
			outputs(filename);
			outputc('\n');
			prtlinenum++;
		}
		if (dbgflg) dbgsrcrec(filename);
	}
	linecounter[inclevel] = 0;
	strcpy(savname[inclevel], filename);
	return(0);
}

/* SRCCLOSE */
/* close a source program file */
INT srcclose()
/* returns the file level closed (0 if main prog) */
{
	rioclose(filetable[inclevel--]);
	if (inclevel != -1) {
		curfile = filetable[inclevel];
	}
	return(inclevel + 1);
}

/* GETLNDSP */
/* display a text line, expanding tabs */
void getlndsp()
{
	INT i, j;
	CHAR linex[LINESIZE + 1];

	if (!liston) return;
	if (fmtflag && prtlinenum > pagelen) newpage();
	outputns(savdataadr, NULL);
	outputns(savcodeadr, NULL);
	outputinc();
	for (i = 0, j = 0; line[i] && j < LINESIZE - 1; i++) {
		if (line[i] != 9) linex[j++] = (CHAR) line[i];
		else {
			linex[j++] = ' ';
			while (j != 14 && j < 24) linex[j++] = ' ';
		}
	}
	linex[j++] = '\n';
	linex[j] = 0;
	outputs(linex);
	prtlinenum++;
}

/* DBGSRCREC */
static void dbgsrcrec(CHAR *srcname)
{
	OFFSET srcsize;

	sreccnt++;
	dbgbuf[0] = 'S';
	rioeofpos(curfile, &srcsize);
	mscoffton(srcsize, dbgbuf + 1, 7);
	dbgbuf[8] = '\0';
	strcat((CHAR *) dbgbuf, srcname);
	rioput(dbgfilenum, dbgbuf, (INT) strlen((CHAR *) dbgbuf));
}

/* DBGDATAREC */
static void dbgdatarec(INT datadr, CHAR *varname)
{
	if (classlevel) classdreccnt++;
	dreccnt++;
	dbgbuf[0] = 'D';
	msciton(datadr, dbgbuf + 1, 7);
	dbgbuf[8] = '\0';
	strcat((CHAR *) dbgbuf, varname);
	rioput(dbgfilenum, dbgbuf, (INT) strlen((CHAR *) dbgbuf));
}

/* DBGLBLREC */
void dbglblrec(INT linenum, CHAR *lblname)
{
	lreccnt++;
	dbgbuf[0] = 'L';
	msciton(linenum, dbgbuf + 1, 6);
	dbgbuf[7] = '\0';
	strcat((CHAR *) dbgbuf, lblname);
	rioput(dbgfilenum, dbgbuf, (INT) strlen((CHAR *) dbgbuf));
}

/* DBGRTNREC */
void dbgrtnrec(CHAR *rtnname)
{
	rreccnt++;
	dbgbuf[0] = 'R';
	strcpy((CHAR *) dbgbuf+1, rtnname);
	rioput(dbgfilenum, dbgbuf, (INT) strlen((CHAR *) dbgbuf));
}

/* DBGENDREC */
void dbgendrec()
{
	dbgbuf[0] = 'X';
	rioput(dbgfilenum, dbgbuf, 1);
}

/* DBGLINEREC */
void dbglinerec(INT linenum, INT prgadr)
{
	preccnt++;
	dbgbuf[0] = 'P';
	msciton(linenum, dbgbuf + 1, 6);
	msciton(prgadr, dbgbuf + 7, 7);
	rioput(dbgfilenum, dbgbuf, 14);
}

/* DBGEOFREC */
static void dbgeofrec(INT srclines)
{
	dbgbuf[0] = 'E';
	msciton(srclines, dbgbuf + 1, 6);
	rioput(dbgfilenum, dbgbuf, 7);
}

/* DBGCLASSREC */
static void dbgclassrec(CHAR *classname)
{
	classdreccnt = 0;
	creccnt++;
	strcpy((CHAR *) dbgbuf, "C000000");
	strcat((CHAR *) dbgbuf, classname);
	rionextpos(dbgfilenum, &classrecpos);
	rioput(dbgfilenum, dbgbuf, (INT) strlen((CHAR *) dbgbuf));
}

/* DBGENDCLASSREC */
static void dbgendclassrec()
{
	INT i1;
	OFFSET saverecpos;

	if (savcodeadr != codeadr) dbglinerec((INT) savlinecnt, savcodeadr);
	savcodeadr = codeadr;
	dbgbuf[0] = 'Y';
	rioput(dbgfilenum, dbgbuf, 1);
	if (classdreccnt) {
		dbgbuf[0] = 'C';
		msciton(classdreccnt, &dbgbuf[1], 6);
		for (i1 = 1; dbgbuf[i1] == ' '; ) dbgbuf[i1++] = '0';
		rionextpos(dbgfilenum, &saverecpos);
		riosetpos(dbgfilenum, classrecpos);
		rioparput(dbgfilenum, dbgbuf, 7);
		riosetpos(dbgfilenum, saverecpos);
	}
}

/* WRITEDBG */
/* Finish writing information to debug DBG file */
static void writedbg()
{
	int i;

	dbgbuf[0] = '9';
	dbgbuf[1] = '1';
	for (i = 0; i < 16; i++) dbgbuf[2 + i] = createtime[i];
	msciton(sreccnt, dbgbuf + 18, 6);
	msciton(dreccnt, dbgbuf + 24, 6);
	msciton(lreccnt, dbgbuf + 30, 6);
	msciton(preccnt, dbgbuf + 36, 6);
	msciton(rreccnt, dbgbuf + 42, 6);
	msciton(creccnt, dbgbuf + 48, 6);
	for (i = 0; i < 54; i++) if (dbgbuf[i] == ' ') dbgbuf[i] = '0';
	riosetpos(dbgfilenum, (OFFSET) 0);
	rioparput(dbgfilenum, dbgbuf, 54);
	rioclose(dbgfilenum);
}

/* XREFOUT */
/* output the cross reference record */
/* flag: non-zero if definition, zero if reference */
static void xrefout(CHAR *s, UINT flag)
{
	INT i;
	UCHAR rec[52];

	i = (INT)strlen(s);
	if (i > LABELSIZ) i = LABELSIZ;
	memcpy(rec, s, (size_t) i);
	memset(&rec[i], ' ', (size_t) LABELSIZ - i);
	if (flag) {
		if (symtype > TYPE_MAXDATAVAR) rec[LABELSIZ] = (UCHAR) (symtype - 10);
		else rec[LABELSIZ] = symtype;
	}
	else rec[LABELSIZ] = '~';
	if (!inclevel) memcpy(rec + LABELSIZ + 1, ".  ", 3);
	else msciton((INT) incnum, rec + LABELSIZ + 1, 3);
	msciton((INT) linecounter[inclevel], rec + LABELSIZ + 4, 8);
	msciton((INT) rlevel, rec + LABELSIZ + 12, 2);
	msciton((INT) rlevelid[rlevel], rec + LABELSIZ + 14, 5);
	rioput(xrefhandle, rec, LABELSIZ + 19);
}

/* FREEALLALLOC */
/* memfree all allocations made by DBCMP */
static void freeallalloc()
{
	if (memreserve1 != NULL) {
		memfree(memreserve1);
		memreserve1 = NULL;
	}
	if (memreserve2 != NULL) {
		memfree(memreserve2);
		memreserve2 = NULL;
	}
	if (mainbufpptr != NULL) {
		memfree(mainbufpptr);
		mainbufpptr = NULL;
	}
	if (exelabeltable != NULL) {
		memfree((UCHAR **) exelabeltable);
		exelabeltable = NULL;
	}
	if (simplelabeltable != NULL) {
		memfree((UCHAR **) simplelabeltable);
		simplelabeltable = NULL;
	}
	if (simplefixuptable != NULL) {
		memfree(simplefixuptable);
		simplefixuptable = NULL;
	}
	if (xdeftable != NULL) {
		memfree(xdeftable);
		xdeftable = NULL;
	}
	if (xreftable != NULL) {
		memfree(xreftable);
		xreftable = NULL;
	}
}

/* XREFDSP */
/* print cross reference */
static void xrefdsp()
{
	INT i, first, recsz, linesz;
	UCHAR c, adrchar, record[XREFBUFSIZE], *unusedptr;
	CHAR xline[133];
	CHAR ptr[15];
	INT memsize, memmax;

	rioclose(xrefhandle);
	xrefhandle = 0;
	freeallalloc();
	memcompact();
	membase(&unusedptr, (UINT*)&memsize, &memmax);
	if (meminit(memsize + 16384, 0, 256) < 0) death(DEATH_NOMEM);
#if OS_WIN32
	strcat(sortcmd, " .\\");
	strcat(sortcmd, xrefwrkname);
	strcat(sortcmd, " .\\");
	strcat(sortcmd, xrefwrkname);
	strcat(sortcmd, " 1-32 47-51 33 37-44 -S --");
	system(sortcmd);
#endif
#if OS_UNIX
	strcat(sortcmd, " ./");
	strcat(sortcmd, xrefwrkname);
	strcat(sortcmd, " ./");
	strcat(sortcmd, xrefwrkname);
	strcat(sortcmd, " 1-32 47-51 33 37-44 -S --");
	system(sortcmd);
#endif
	if ((xrefhandle = rioopen(xrefwrkname, RIO_M_EXC | RIO_P_WRK | RIO_T_STD | RIO_FIX | RIO_UNC, 4, XREFBUFSIZE)) < 0) {
		printf("%s\n", fioerrstr(xrefhandle));
		death(DEATH_BADWRKFILE);
	}
	if (fmtflag && prtlinenum > pagelen) newpage();
	outputs("---------------NAME------------- -L- ------DEF----- ---TYPE--- -------USE-------\n");
	prtlinenum++;
	first = TRUE;
	linesz = parm[0] = 0;
	while (TRUE) {
		recsz = rioget(xrefhandle, record, 51);
		if (recsz == -1) break;
		if (recsz < -2) {
			printf("%s\n", fioerrstr(recsz));
			death(DEATH_BADWRKREAD);
		}
		if (record[32] != '~') {	/* it is a new variable */
			if (!first) {
				if (fmtflag && prtlinenum > pagelen) newpage();
				if (linesz) {
					xline[--linesz] = 0;
					outputs(xline);
				}
				outputc('\n');
				prtlinenum++;
			}
			else first = FALSE;
			memset(xline, ' ', 132);
			memcpy(xline, record, 32);
			memcpy(&xline[33], &record[44], 2);
			memcpy(&xline[37], &record[36], 8);
			if (record[33] != '.') {
				xline[45] = '.';
				xline[46] = '(';
				linesz = 47;
				for (i = 33; i < 36; i++) if (record[i] != ' ') xline[linesz++] = record[i];
				xline[linesz] = ')';
			}
			c = record[32];
			if (c & TYPE_PTR) adrchar = '@';
			else adrchar = ' ';
			c &= ~TYPE_PTR;
			switch (c) {
				case TYPE_CVAR:
				case TYPE_CVARPTR: strcpy(ptr, "CHAR "); break;
				case TYPE_CARRAY1: strcpy(ptr, "CHAR []"); break;
				case TYPE_CARRAY2: strcpy(ptr, "CHAR [,]"); break;
				case TYPE_CARRAY3: strcpy(ptr, "CHAR [,,]"); break;
				case (TYPE_CVARPTRARRAY1 & ~TYPE_PTR):
					strcpy(ptr, "CHAR []@"); break;
				case (TYPE_CVARPTRARRAY2 & ~TYPE_PTR):
					strcpy(ptr, "CHAR [,]@"); break;
				case (TYPE_CVARPTRARRAY3 & ~TYPE_PTR):
					strcpy(ptr, "CHAR [,,]@"); break;
				case TYPE_NVAR:
				case TYPE_NVARPTR: strcpy(ptr, "NUM "); break;
				case TYPE_NARRAY1: strcpy(ptr, "NUM []"); break;
				case TYPE_NARRAY2: strcpy(ptr, "NUM [,]"); break;
				case TYPE_NARRAY3: strcpy(ptr, "NUM [,,]"); break;
				case (TYPE_NVARPTRARRAY1 & ~TYPE_PTR):
					strcpy(ptr, "NUM []@"); break;
				case (TYPE_NVARPTRARRAY2 & ~TYPE_PTR):
					strcpy(ptr, "NUM [,]@"); break;
				case (TYPE_NVARPTRARRAY3 & ~TYPE_PTR):
					strcpy(ptr, "NUM [,,]@"); break;
				case (TYPE_VVARPTR & ~TYPE_PTR): strcpy(ptr, "VAR"); break;
				case (TYPE_VVARPTRARRAY1 & ~TYPE_PTR):
					strcpy(ptr, "VAR []@"); break;
				case (TYPE_VVARPTRARRAY2 & ~TYPE_PTR):
					strcpy(ptr, "VAR [,]@"); break;
				case (TYPE_VVARPTRARRAY3 & ~TYPE_PTR):
					strcpy(ptr, "VAR [,,]@"); break;
				case TYPE_LIST:
				case TYPE_CVARLIST: strcpy(ptr, "LIST "); break;
				case TYPE_FILE: strcpy(ptr, "FILE "); break;
				case TYPE_IFILE: strcpy(ptr, "IFILE "); break;
				case TYPE_AFILE: strcpy(ptr, "AFILE "); break;
				case TYPE_COMFILE: strcpy(ptr, "COMFILE "); break;
				case TYPE_PFILE: strcpy(ptr, "PFILE "); break;
				case TYPE_DEVICE: strcpy(ptr, "DEVICE "); break;
				case TYPE_QUEUE: strcpy(ptr, "QUEUE "); break;
				case TYPE_RESOURCE: strcpy(ptr, "RESOURCE "); break;
				case TYPE_IMAGE: strcpy(ptr, "IMAGE "); break;
				case TYPE_EQUATE: strcpy(ptr, "EQUATE "); break;
				case TYPE_PLABELVAR - 10: strcpy(ptr, "LABEL "); break;
				case TYPE_PLABEL - 10:
				case TYPE_PLABELXREF - 10:
				case TYPE_PLABELXDEF - 10:
					strcpy(ptr, "LABEL ");
					adrchar = ' ';
					break;
				default:
					strcpy(ptr, "UNKNOWN"); break;
			}
			i = (INT)strlen(ptr);
			if (adrchar == '@') ptr[i++] = '@';
			if (ptr[0] != '\0') memcpy(&xline[52], ptr, (size_t) i);
			linesz = 62;
		}
		else {  /* same variable */
			if (linesz > 115) {
				if (fmtflag && prtlinenum > pagelen) newpage();
				xline[linesz++] = '\n';
				xline[linesz] = '\0';
				outputs(xline);
				memset(xline, ' ', 132);
				linesz = 62;
				prtlinenum++;
			}
			for (i = 36; i <= 43; i++) xline[linesz++] = record[i];
			if (record[33] != '.') {
				xline[linesz++] = '.';
				xline[linesz++] = '(';
				for (i = 33; i < 36; i++) if (record[i] != ' ') xline[linesz++] = record[i];
				xline[linesz++] = ')';
			}
			linesz += 2;
		}
	}
	if (fmtflag && prtlinenum > pagelen) newpage();
	xline[linesz - 1] = '\n';
	xline[linesz] = '\0';
	outputs(xline);
	prtlinenum++;
	riokill(xrefhandle);
}

/*
 * Output statistics to an xml file for the DDT to use
 */
static void xmlstats() {
	CHAR statwrk[64];
	INT i1;
	sprintf(statwrk, "<mplns>%i</mplns>", linecounter[0]);
	i1 = (INT)strlen(statwrk);
	fiowrite(xmlfilenum, xmlfilepos, (UCHAR *) statwrk, i1);
	xmlfilepos += i1;
	sprintf(statwrk, "<tlns>%i</tlns>", totlines);
	i1 = (INT)strlen(statwrk);
	fiowrite(xmlfilenum, xmlfilepos, (UCHAR *) statwrk, i1);
	xmlfilepos += i1;
	sprintf(statwrk, "<datasz>%i</datasz>", dataadr);
	i1 = (INT)strlen(statwrk);
	fiowrite(xmlfilenum, xmlfilepos, (UCHAR *) statwrk, i1);
	xmlfilepos += i1;
	sprintf(statwrk, "<codesz>%i</codesz>", codeadr);
	i1 = (INT)strlen(statwrk);
	fiowrite(xmlfilenum, xmlfilepos, (UCHAR *) statwrk, i1);
	xmlfilepos += i1;
	sprintf(statwrk, "<errcnt>%i</errcnt>", errcnt);
	i1 = (INT)strlen(statwrk);
	fiowrite(xmlfilenum, xmlfilepos, (UCHAR *) statwrk, i1);

	sprintf(statwrk, "<exrefb>%i</exrefb>", externalReferencesTableSizeInBytes);
	i1 = (INT)strlen(statwrk);
	fiowrite(xmlfilenum, xmlfilepos, (UCHAR *) statwrk, i1);
	xmlfilepos += i1;

	sprintf(statwrk, "<exrefc>%i</exrefc>", externalReferencesTableCountOfEntries);
	i1 = (INT)strlen(statwrk);
	fiowrite(xmlfilenum, xmlfilepos, (UCHAR *) statwrk, i1);
	xmlfilepos += i1;

	sprintf(statwrk, "<exdefb>%i</exdefb>", externalDefinitionsTableSizeInBytes);
	i1 = (INT)strlen(statwrk);
	fiowrite(xmlfilenum, xmlfilepos, (UCHAR *) statwrk, i1);
	xmlfilepos += i1;

	sprintf(statwrk, "<exdefc>%i</exdefc>", externalDefinitionsTableCountOfEntries);
	i1 = (INT)strlen(statwrk);
	fiowrite(xmlfilenum, xmlfilepos, (UCHAR *) statwrk, i1);
	xmlfilepos += i1;
}

/* STATS */
/* display statistics */
static void stats()
{
	if (dspflags & DSPFLAG_STATS) {
		if (fmtflag && prtlinenum + 6 > pagelen) {
			for (; prtlinenum <= pagelen; prtlinenum++) outputs("\n");
			newpage();
		}
		outputs("Statistics for ");
		outputs(srcname);
		outputs("\n");
		outputns((INT) linecounter[0], " main program lines\n");
		outputns((INT) totlines, " total lines\n");
		outputns(dataadr, " data size\n");
		outputns(codeadr, " program size\n");
		if (dspflags & DSPFLAG_XTRASTATS) {
			outputns(externalDefinitionsTableSizeInBytes, " external definitions table size in bytes\n");
			outputns(externalDefinitionsTableCountOfEntries, " external definitions count\n");
			outputns(externalReferencesTableSizeInBytes, " external references table size in bytes\n");
			outputns(externalReferencesTableCountOfEntries, " external references count\n");
			outputns((INT)executionLabelTableCount * 3, " execution labels table size in bytes\n");
			outputns((INT)executionLabelTableCount, " execution labels count\n");
		}
	}
	if (dspflags & (DSPFLAG_VERBOSE | DSPFLAG_STATS)) {
		if (errcnt == 1) outputns(errcnt, " ERROR\n");
		else if (errcnt > 1) outputns(errcnt, " ERRORS\n");
		else if ((dspflags & DSPFLAG_PROGRAM) && !(dspflags & DSPFLAG_STATS)) {
			outputs(srcname);
			outputs(" compiled successfully\n");
		}
		else outputs("Compiled successfully\n");
	}
}

/* NEWPAGE */
/* page break */
static void newpage()
{
	static INT prtpagecnt = 0;
	static CHAR s[MAXPGWIDTH + 1];
	INT i, j, k;
	time_t timer;
	struct tm *today;

	outputc('\n');
	memset(s, ' ', MAXPGWIDTH);
	s[MAXPGWIDTH] = '\0';
	time(&timer);
	today = localtime(&timer);
	i = today->tm_hour;
	s[0] = (CHAR) (i / 10 + '0');
	s[1] = (CHAR) (i % 10 + '0');
	i = today->tm_min;
	s[3] = (CHAR) (i / 10 + '0');
	s[4] = (CHAR) (i % 10 + '0');
	i = today->tm_sec;
	s[6] = (CHAR) (i / 10 + '0');
	s[7] = (CHAR) (i % 10 + '0');
	s[2] = s[5] = ':';
	i = today->tm_mon + 1;
	s[11] = (CHAR) (i / 10 + '0');
	s[12] = (CHAR) (i % 10 + '0');
	i = today->tm_mday;
	s[14] = (CHAR) (i / 10 + '0');
	s[15] = (CHAR) (i % 10 + '0');
	i = today->tm_year % 100;
	s[17] = (CHAR) (i / 10 + '0');
	s[18] = (CHAR) (i % 10 + '0');
	s[13] = s[16] = '/';

	if (header[0]) {
		i = pagewdth - 31;
		j = (INT)strlen(header);
		if ((k = i - j) < 0) {
			j = i;
			k = 0;
		}
		else k /= 2;
		memcpy(&s[20 + k], header, (size_t) j);
	}
	strcpy(&s[pagewdth - 11], "PAGE");
	outputs(s);
	outputns((INT) ++prtpagecnt, "\n\n\n");
	prtlinenum = 0;
}

/* OUTPUTC */
/* output the character c */
static void outputc(INT c)
{
	if (prtflag) fputc((CHAR) c, outfile);
	else dspchar((CHAR) c);
}

/* OUTPUTS */
/* output the string s */
static void outputs(CHAR *s)
{
	if (prtflag) fputs(s, outfile);
	else dspstring(s);
}

/* OUTPUTNS */
/* output a 8 digit number, following by string s */
static void outputns(INT n, CHAR *s)
{
	INT i;
	CHAR c[9];

	strcpy(c, "        ");
	for (i = 7; n || i == 7; i--) {
		c[i] = (CHAR) (n % 10 + '0');
		n /= 10;
	}
	if (prtflag) {
		fputs(c, outfile);
		if (s != NULL) fputs(s, outfile);
	}
	else {
		dspstring(c);
		if (s != NULL) dspstring(s);
	}
}

/* OUTPUTINC */
static void outputinc()
{
	char outinc[10];

	outinc[0] = '.';
	outinc[1] = '(';
	if (inclevel > 0) mscitoa((INT) incnum, outinc+2);
	else {
		outinc[2] = '0';
		outinc[3] = '\0';
	}
	strcat(outinc, ") ");
	outputns((INT) linecounter[inclevel], outinc);
}

/* WARNING */
/* display source program warning */
void warning(INT n)
{
	if (dspflags & DSPFLAG_XML) xmlmessage(XML_WARNING, (INT) linecounter[inclevel], savname[inclevel], errormsg(n));
	else {
		if (fmtflag && prtlinenum > pagelen) newpage();
		if (dspflags & DSPFLAG_COMPILE) getlndsp();
		else if (n != WARNING_NOTENOUGHENDROUTINES) {
			outputns((INT) linecounter[inclevel], ".(");
			outputs(savname[inclevel]);
			outputc(')');
			outputc(' ');
			outputs((CHAR *) line);
			outputc('\n');
			prtlinenum++;
		}
		outputs(errormsg(WARNING_WARNING));
		outputs(errormsg(n));
		outputc('\n');
		prtlinenum++;
	}
}

/* ERROR */
/* display source program error */
void error(INT n)
{
	INT i1;
	CHAR work[256], *ptr;

	errcnt++;
	curerrcnt++;
	if (dspflags & DSPFLAG_XML) {
		if (n != ERROR_LABELNOTFOUND && n != ERROR_MISSINGENDIF &&
			n != ERROR_MISSINGREPEAT && n != ERROR_MISSINGENDSWITCH &&
			n != ERROR_MISSINGRECORDEND && n != ERROR_MISSINGLISTEND &&
			n != ERROR_MISSINGCCDENDIF && n != ERROR_MISSINGCLASSEND) {
			i1 = linecounter[inclevel];
			ptr = savname[inclevel];
		}
		else {
			i1 = 0;
			ptr = NULL;
		}
		strcpy(work, errormsg(n));
		if ((n >= ERROR_VARNOTFOUND && n <= ERROR_BADLABEL) ||
			n == ERROR_EQNOTFOUND ||
			n == ERROR_BADPREP ||
			n == ERROR_RECNOTFOUND ||
			n == ERROR_BADKEYWORD ||
			n == ERROR_CLASSNOTFOUND) strcat(work, (CHAR *) token);
		xmlmessage(XML_ERROR, i1, ptr, work);
	}
	else {
		if (errcnt == 1 && (dspflags & DSPFLAG_PROGRAM) && !(dspflags & DSPFLAG_STATS)) {
			outputs(srcname);
			outputs("\n");
		}
		if (fmtflag && prtlinenum > pagelen) newpage();
		if (dspflags & DSPFLAG_COMPILE) {
			if (n != ERROR_LABELNOTFOUND) getlndsp();
		}
		else if (n != ERROR_LABELNOTFOUND && n != ERROR_MISSINGENDIF &&
			n != ERROR_MISSINGREPEAT && n != ERROR_MISSINGENDSWITCH &&
			n != ERROR_MISSINGRECORDEND && n != ERROR_MISSINGLISTEND &&
			n != ERROR_MISSINGCCDENDIF && n != ERROR_MISSINGCLASSEND) {
			outputns((INT) linecounter[inclevel], ".(");
			outputs(savname[inclevel]);
			outputc(')');
			outputc(' ');
			outputs((CHAR *) line);
			outputc('\n');
			prtlinenum++;
		}
		outputs(errormsg(ERROR_ERROR));
		outputs(errormsg(n));
		if ((n >= ERROR_VARNOTFOUND && n <= ERROR_BADLABEL) ||
			n == ERROR_EQNOTFOUND ||
			n == ERROR_BADPREP ||
			n == ERROR_RECNOTFOUND ||
			n == ERROR_BADKEYWORD ||
			n == ERROR_CLASSNOTFOUND) outputs((CHAR *) token);
		outputc('\n');
		prtlinenum++;
	}
	scrlparseflg = FALSE;   /* reset in case error occured in scroll list */
}

/* DEATH */
/* fatal error */
void death(INT n)
{
	if (dspflags & DSPFLAG_XML) {
		strcpy(dbcmperrstr, errormsg(n));
		if (n == DEATH_BADPARM || n == DEATH_BADPARMVALUE || n == DEATH_BADLIBFILE) strcat(dbcmperrstr, parm);
		if (xmlfilenum > 0) xmlmessage(XML_FATAL, 0, NULL, dbcmperrstr);
		longjmp(xmljmp, 1);
	}
#if OS_WIN32
	if (!(dspflags & DSPFLAG_CONSOLE)) longjmp(dlljmp, 1);
#endif
	prtflag = FALSE;
	outputs(errormsg(n));
	if (n == DEATH_BADPARM || n == DEATH_BADPARMVALUE || n == DEATH_BADLIBFILE) outputs(parm);
	outputc('\n');
	cfgexit();
	exit(1);
}

static void xmlmessage(INT type, INT linenum, CHAR *filename, CHAR *msg)
{
	INT i1, i2;
	CHAR work[1024], *ptr;

	if (type == XML_TASK_CODE || type == XML_TASK_FIXME || type == XML_TASK_TODO) ptr = "task";
	else if (type == XML_ERROR) ptr = "error";
	else if (type == XML_WARNING) ptr = "warning";
	else ptr = "fatal";
	work[0] = '<';
	strcpy(&work[1], ptr);
	i1 = (INT)strlen(work);
	if (type == XML_TASK_TODO) {
		strcpy(&work[i1], " priority=\"normal\"");
		i1 += (INT)strlen(&work[i1]);
	}
	else if (type == XML_TASK_CODE) {
		strcpy(&work[i1], " priority=\"low\"");
		i1 += (INT)strlen(&work[i1]);
	}
	else if (type == XML_TASK_FIXME) {
		strcpy(&work[i1], " priority=\"high\"");
		i1 += (INT)strlen(&work[i1]);
	}
	if (linenum) {
		strcpy(&work[i1], " line=");
		i1 += (INT)strlen(&work[i1]);
		i1 += mscitoa(linenum, &work[i1]);
	}
	if (filename != NULL && *filename) {
#if 0
		for (i2 = 0; filename[i2] && !isspace(filename[i2]) && filename[i2] != '/'; i2++);
#endif
		strcpy(&work[i1], " file=");
		i1 += (INT)strlen(&work[i1]);
#if 0
		if (filename[i2])
#endif
			work[i1++] = '"';
		strcpy(&work[i1], filename);
		i1 += (INT)strlen(&work[i1]);
#if 0
		if (filename[i2])
#endif
			work[i1++] = '"';
	}
	if (msg != NULL && *msg) {
		work[i1++] = '>';
		for (i2 = 0; msg[i2]; i2++) {
			if (msg[i2] == '<') {
				strcpy(&work[i1], "&lt;");
				i1 += 4;
			}
			else if (msg[i2] == '>') {
				strcpy(&work[i1], "&gt;");
				i1 += 4;
			}
			else if (msg[i2] == '&') {
				strcpy(&work[i1], "&amp;");
				i1 += 5;
			}
			else if (msg[i2] == '"') {
				strcpy(&work[i1], "&quot;");
				i1 += 6;
			}
			else if (msg[i2] == '\'') {
				strcpy(&work[i1], "&apos;");
				i1 += 6;
			}
			else if ((UCHAR) msg[i2] < 0x20 || (UCHAR) msg[i2] >= 0x7F) {  /* don't expect this to happen */
				work[i1++] = '&';
				work[i1++] = '#';
				i1 += mscitoa((UCHAR) msg[i2], &work[i1]);
				work[i1++] = ';';
			}
			else work[i1++] = msg[i2];
		}
		work[i1++] = '<';
		work[i1++] = '/';
		strcpy(&work[i1], ptr);
		i1 += (INT)strlen(&work[i1]);
		work[i1++] = '>';
	}
	else {
		work[i1++] = '/';
		work[i1++] = '>';
	}
	if (fiowrite(xmlfilenum, xmlfilepos, (UCHAR *) work, i1) >= 0) xmlfilepos += i1;
	else if (type != XML_FATAL) death(DEATH_BADXMLWRITE);
}

/* TEMPFILE */
/* opentype: 1 for rioopen, 2 for fioopen */
static INT tempfile(CHAR *tmpfile, INT opentype)
{
	int i, j, tmphandle;

	strcpy(tmpfile, "DBCMP");
	i = opentype;
	do {
		j = 5;
		if (i >= 1000) death(DEATH_BADWRKFILE);
		if (i >= 100) tmpfile[j++] = (i / 100) % 10 + '0';
		if (i >= 10) tmpfile[j++] = (i / 10) % 10 + '0';
		tmpfile[j++] = i % 10 + '0';
		tmpfile[j] = '\0';
		strcat(tmpfile, ".WRK");
		if (opentype == 1) tmphandle = rioopen(tmpfile, RIO_M_EFC | RIO_P_WRK | RIO_T_STD | RIO_FIX | RIO_UNC, 3, XREFBUFSIZE);
		else tmphandle = fioopen(tmpfile, FIO_M_EFC | FIO_P_WRK);
		i += 2;
	} while (ERR_EXIST == tmphandle || ERR_NOACC == tmphandle);
	if (tmphandle < 0) death(DEATH_BADWRKFILE);
	return(tmphandle);
}

/* USAGE */
static void usage()
{
#if OS_WIN32
	if (!(dspflags & DSPFLAG_CONSOLE)) longjmp(dlljmp, 1);
#endif
	dspstring("DBCMP command  ");
	dspstring(RELEASE);
	dspstring(COPYRIGHT);
	dspchar('\n');
	/*         12345678901234567890123456789012345678901234567890123456789012345678901234567890 */
	dspstring("Usage:  dbcmp file1 [file2] [-AFILE=[TEXT][NATIVE][BINARY][DATA][CRLF]] [-C]\n");
	dspstring("              [-CFG=cfgfile] [-D[=nnn]] [-E=name] [-ERR=DEL] [-F]\n");
	dspstring("              [-FILE=[TEXT][NATIVE][BINARY][DATA][CRLF]] [-H=header] [-I]\n");
	dspstring("              [-IFILE=[TEXT][NATIVE][BINARY][DATA][CRLF]] [-J] [-JR]\n");
	dspstring("              [-K] [-L=library name] [-N=nnn] [-O=filename] [-P[=filename]]\n");
	dspstring("              [-R[=filename]] [-S[X]] [-T=filename] [-V] [-W=nnn] [-X] [-Z]\n");
	dspstring("              [-1] [-2] [-3] [-8] [-9]\n");
	exit(1);
}

/* QUITSIG */
static void quitsig(INT sig)
{
	signal(sig, SIG_IGN);
	if (databufhandle) fiokill(databufhandle);
	if (mainbufhandle) fiokill(mainbufhandle);
	if (xrefhandle) riokill(xrefhandle);
	if (dbgflg && dbgfilenum) {
		if (removeflg) riokill(dbgfilenum);
		else rioclose(dbgfilenum);
	}
	if (dbcfilenum) {
		if (removeflg) fiokill(dbcfilenum);
		else fioclose(dbcfilenum);
	}
	if (dspflags & DSPFLAG_VERBOSE) outputs(errormsg(1000));
	cfgexit();
	exit(1);
}

CHAR* getInteralError8Message() {return internalError8Message;}

void setInteralError8Message(CHAR* msg) {
	strncpy(internalError8Message, msg, MAXINTERNALERROR8SIZE);
}

