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
#define INC_SETJMP
#include "includes.h"
#include "release.h"
#include "arg.h"
#include "base.h"
#include "dbc.h"
#include "fio.h"
#include "fsfileio.h"
#include "tcp.h"

#if OS_UNIX && defined(Linux)
#define ARRAYSIZE(A) (sizeof(A)/sizeof((A)[0]))
#endif

#if OS_UNIX
#define closesocket(a) close(a)
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#define LISTMAX	10
#define MODBUFSIZE	16384

/* defines for datatype, 0x0000FF00 is reserved for future use */
#define DATA_GTYPEMASK	0x000000FF
#define DATA_GLOBAL     0x00010000
#define DATA_GLOBALOK	0x00020000
#define DATA_GTYPE		0x00040000
#define DATA_INIT		0x00080000
#define DATA_INIT2		0x00100000
#define DATA_ARRAY		0x00200000
#define DATA_COMMON     0x00400000
#define DATA_KEYVAR     0x00800000
#define DATA_ARRAYCHAR	0x01000000
#define DATA_ARRAYNUM	0x02000000
#define DATA_ARRAYINT	0x04000000
#define DATA_ARRAYFLOAT	0x08000000
#define DATA_ARRAYADR	0x10000000
#define DATA_GTYPESHIFT	0

/**
 * A singly linked list of structures defining global variables.
 */
typedef struct globalinfostruct {
	struct globalinfostruct **nextptr;
	INT offset;
	CHAR name[32]; // Must be last, its allocated size varies to accomodate the actual name length plus term-null
} GLOBALINFO;

/* global variables */
INT datachn, pgmchn;				/* chain data and program */

/* local variables */

/**
 * Pointer into memalloc memory of the first DATAMOD structure of an array of datamax structures
 */
static DATAMOD **datatabptr;
/**
 * Pointer into memalloc memory of the first PGMMOD structure of an array of pgmmax structures
 */
static PGMMOD **pgmtabptr;
static GLOBALINFO **globaltabptr;
/**
 * Index of highest unused entry in datatabptr
 * (Pointed to entry and all higher are unused)
 */
static INT datahi;
/**
 * Count of allocated DATAMOD structures in datatabptr.
 * Starts at 16 and doubles every time the table becomes full
 */
static INT datamax;
/**
 * Index of highest unused entry in pgmtabptr
 * (Pointed to entry and all higher are unused)
 */
static INT pgmhi;
/**
 * Count of allocated PGMMOD structures in pgmtabptr.
 * Starts at 8 and doubles every time the table becomes full
 */
static INT pgmmax;
/**
 * Deref from double to single pointer to datatabptr
 */
static DATAMOD *datatab;
/**
 * Deref from double to single pointer to pgmtabptr
 */
static PGMMOD *pgmtab;
static INT dataxmodule, pgmxmodule;	/* TRUE current module numbers */
static INT dataxchn;
static INT lastdatamod, lastdataxmod;
/**
 * Provides a unique ID for each DATAMOD ever used.
 * Starts at 1 the 1st time newmod is called.
 * For each Chain or Class, and if not seen before for each Loadmod and Ploadmod,
 * this is incremented by one and is moved to field datamod in the DATAMOD struct.
 * Loops back to one when it hits 0x7FFF
 */
static INT dataxcnt;
/**
 * Provides a unique ID for each PGMMOD ever used.
 * Starts at 1 the 1st time newmod is called.
 * For each Chain or Class, and if not seen before for each Loadmod and Ploadmod,
 * this is incremented by one and is moved to field pgmmod in the PGMMOD struct.
 * Loops back to one when it hits INT_MAX
 */
static INT pgmxcnt;
/**
 * FIO or FS handle to the *.dbc file (or library containing the dbc!)
 * Used by newmod and modread
 */
static INT handle;
static INT srvhandle, rpos;
/**
 * Used by newmod, loaddata, and modread
 */
static INT rbyte;

/*
 * Used by newmod and modread
 */
static OFFSET fpos;

static INT adrmod;					/* hold module of variable */
static INT adroff;					/* hold offset of variable */
static INT pcntsave = -1;			/* hold pcount used by getadrx */

/* local routines */
static INT loadclass(INT);
static INT loadlabel(INT, INT, INT, INT, UCHAR ***, UCHAR ***, UCHAR ***);
static INT loaddata(UCHAR ***, INT, INT, INT, INT *, INT *);
static INT modread(UCHAR *, INT);
static void unloadmod(INT, INT);
static INT destroy(INT, INT *);
static INT getdatax(INT);
static INT getpgmx(INT);
static INT getpcntx(INT, INT, INT *, INT *, INT);
static INT getxlabel(CHAR *, INT *, INT *, INT);
static INT adrtype(UCHAR *);
static UCHAR *getarray(void);

#define FIRST_GLOBAL_NAME "__gdim1"
#define INITIAL_GLOBAL_DATA_AREA_SIZE 256

/* NEWMOD
 * load a new module
 *
 * @param chnflg	the type of module to load
 * 					MTYPE_PRELOAD = preload
 * 					MTYPE_CHAIN   = result of a chain verb or this is the first program
 * 					MTYPE_LOADMOD = loadmod
 * 					MTYPE_CLASS   = a class, the make verb was executed
 *
 * @param pname		the 'module' name, this corresponds to the name of a dbc file (sans extension).
 * 					If chnflg is MTYPE_LOADMOD then this is exactly as coded in the source, it may have
 * 					an instance name like this "modulename<instancename>"
 * 					If chnflg is MTYPE_CLASS this comes from the 'module' attribute of the class statement.
 *
 * @param dname     NULL for loadmod and ploadmod
 *
 * @param mod       NULL for loadmod and ploadmod
 *
 * @param pcnt      -1 for loadmod and ploadmod
 *
 */
INT newmod(INT chnflg, CHAR *pname, CHAR *dname, INT *mod, INT pcnt)
{
	/*
	 * flag for first call to newmod since dbc fired up.
	 * First call might be a ploadmod if the config has them.
	 * Otherwise it will be a CHAIN
	 */
	static UCHAR firstflg = TRUE;
	/*
	 * Separate flag for first chain, since first call might be
	 * for a ploadmod
	 */
	static INT firstchn = TRUE;

	/**
	 * Array of pointers to names of libraries as they appear
	 * on the startup command line with -L= options.
	 */
	static CHAR **libptr[17];

	static CODEINFO **dummycode;
	INT i1, i2, i3, i4, classsize, lextsize, libflg, tabcnt;
	/**
	 * Will always be -1 for CHAIN and CLASS
	 * For PRELOAD and LOADMOD will be -1 if this code module has not been seen before
	 * otherwise will be index into pgmtab of matching program
	 */
	INT pgmdupmod;
	INT childmod, classdmod, classpmod, classflg, dataxmod, pgmxmod;
	INT savedataxmod, savepgmxmod, retcnt;
	INT nwork, *nptr;
	OFFSET eofpos;
	CHAR libname[100], pgmname[100], dataname[50], *ptr1, *ptr2;
	UCHAR c1, c2, tables[7], *p, **pptr, **pptr1;
	GLOBALINFO *globaltab;
	/**
	 * Local stack variable
	 */
	DATAMOD datawrk;
	/**
	 * Local stack variable
	 */
	PGMMOD pgmwrk;
	CODEINFO codewrk, *code;

	/* first time initialization */
	if (firstflg) {
		/* get library name */
		for (i1 = 0, i2 = ARG_FIRST; i1 < (INT) (sizeof(libptr) / sizeof(CHAR **) - 1); i2 = ARG_NEXT) {
			i3 = argget(i2, (CHAR *) dbcbuf, sizeof(dbcbuf));
			if (i3) {
				if (i3 < 0) {
					if (i3 == ERR_NOMEM) {
						return 107;
					}
					return i3;
				}
				break;
			}
			if (dbcbuf[0] == '-' && toupper(dbcbuf[1]) == 'L' && dbcbuf[2] == '=' && dbcbuf[3]) {
				libptr[i1] = (CHAR **) memalloc((INT) strlen((CHAR *) &dbcbuf[3]) + 1, 0);
				if (libptr[i1] == NULL) {
					return(107);
				}
				strcpy(*libptr[i1++], (CHAR *) &dbcbuf[3]);
			}
		}
		libptr[i1] = NULL;

		/* allocate dummy code, program and name area, used when chain fails */
		dummycode = (CODEINFO **) memalloc(sizeof(CODEINFO), MEMFLAGS_ZEROFILL);
		pptr = memalloc(1, 0);
		pptr1 = memalloc(100, 0);
		if (dummycode == NULL || pptr == NULL || pptr1 == NULL) {
			return(107);
		}
		code = *dummycode;
		code->pgmptr = pptr;
		code->pgmsize = 1;
		code->nameptr = (CHAR **) pptr1;
		code->codecnt = 2;
		**pptr = VERB_STOP; /* stop */
		**pptr1 = 0;

		/*
		 * allocate an array of (initially) 16 DATAMOD structures, the data table
		 */
		datamax = 16;
		datatabptr = (DATAMOD **) memalloc(datamax * sizeof(DATAMOD), MEMFLAGS_ZEROFILL);
		if (datatabptr == NULL) {
			return(107);
		}

		/* allocate program table */
		pgmmax = 8;
		pgmtabptr = (PGMMOD **) memalloc(pgmmax * sizeof(PGMMOD), MEMFLAGS_ZEROFILL);
		if (pgmtabptr == NULL) {
			return(107);
		}

		/*
		 * Allocate global table and inititialize __gdim1
		 * Start with just one GLOBALINFO structure
		 */
		i1 = sizeof(globaltab->name) - sizeof(FIRST_GLOBAL_NAME);
		globaltabptr = (GLOBALINFO **) memalloc(sizeof(GLOBALINFO) - i1 * sizeof(UCHAR), 0);
		if (globaltabptr == NULL) {
			return(107);
		}
		globaltab = *globaltabptr;
		globaltab->nextptr = NULL; /* Indicate no more entries in the linked list */
		globaltab->offset = 0; /* We are the first variable in the runtime data area */
		strcpy(globaltab->name, FIRST_GLOBAL_NAME);

		/* allocate global data area and initialize __gdim1 variable */
		pptr = memalloc(INITIAL_GLOBAL_DATA_AREA_SIZE, 0);
		if (pptr == NULL) {
			return(107);
		}
		p = *pptr;
		/* define the __gdim1 */
		p[0] = p[1] = 0; /* FP and LL are zero */
		p[2] = 1;        /* PL is one */
		p[3] = ' ';      /* value is a single blank */
		p[4] = 0xF2;     /* end of data */

		datatab = *datatabptr;
		for (i1 = 0; i1 < datamax; i1++) datatab[i1].datamod = -1;
		datatab[0].datamod = 0;
		datatab[0].dataptr = pptr;
		datatab[0].size = 4;
		datatab[0].maxsize = INITIAL_GLOBAL_DATA_AREA_SIZE;
		datatab[0].pgmxmod = -1;
		datatab[0].nextmod = -1;
		dataxcnt = datahi = 1;

		pgmtab = *pgmtabptr;
		for (i1 = 0; i1 < pgmmax; i1++) pgmtab[i1].pgmmod = -1;
		pgmhi = 0;
		pgmxcnt = 1;
		firstflg = FALSE;
	} // end of 'first' block

	if (pcnt != -1) {  /* call destructors first */
		savedataxmod = dataxmodule;
		savepgmxmod = pgmxmodule;
		retcnt = returnstackcount();

/*** MAYBE HAVE COUNT OF CLASSES EFFECTED BY CHAIN/UNLOAD TO PREVENT THIS LOOP ***/
/*** OR MAYBE CREATE LINKED LIST OF LOADMODS & CHAINMOD TO PARSE MORE QUICKLY ***/
		for (i1 = pgmhi; i1--; ) {
			if (pgmtab[i1].typeflg != MTYPE_CHAIN && pgmtab[i1].typeflg != MTYPE_LOADMOD) continue;
			for (i2 = pgmtab[i1].dataxmod; i2 != -1; i2 = datatab[i2].nextmod) {
				/* call loaded classes' destructors */
				for (i3 = datatab[i2].classmod; i3 != -1; i3 = datatab[i3].nextclass) {
					i4 = destroy(i3, &pcnt);
					if (i4) {
						i1 = returnstackcount();
						while (i1-- > retcnt) popreturnstack();
						dataxmodule = savedataxmod;
						datamodule = datatab[dataxmodule].datamod;
						pgmxmodule = savepgmxmod;
						pgmmodule = pgmtab[pgmxmodule].pgmmod;
						setpgmdata();
						return(i4);
					}
				}
			}
		}
		if (returnstackcount() > retcnt) {
			setpgmdata();
			return(0);  /* call destructors first */
		}
	}
	if (!chnflg) return(0);

	/* get data name */
	dataname[0] = '\0';
	for (i1 = 0; pname[i1] && pname[i1] != '<'; i1++);
	if (pname[i1]) {
		pname[i1++] = '\0';
		if (chnflg == MTYPE_PRELOAD || chnflg == MTYPE_LOADMOD) {
			for (i2 = 0; i2 < 8 && pname[i1] && pname[i1] != '>'; i1++)
				if (!isspace(pname[i1])) dataname[i2++] = (CHAR) toupper(pname[i1]);
			dataname[i2] = '\0';
		}
	}
	if (chnflg == MTYPE_CLASS) {
		if ((*mod = getdatax(*mod)) == RC_ERROR) return(505);  /* get objects data module */
		if (!pname[0]) {
			strcpy(name, *(*pgmtab[pgmxmodule].codeptr)->fileptr);
			pname = name;
		}
		strcpy(dataname, dname);
		childmod = -1;
		classflg = FALSE;
	}

newmod0:
	/* get program name */
	libflg = FALSE;
	strcpy(pgmname, pname);
	miofixname(pgmname, ".dbc", FIXNAME_EXT_ADD | FIXNAME_PAR_DBCVOL);
	for (i1 = (INT) (strlen(pgmname) - 1); pgmname[i1] != '.' && pgmname[i1] != ')'; i1--);
	pgmname[i1--] = '\0';
	while (i1 >= 0 && pgmname[i1] != '\\' && pgmname[i1] != ':'
			&& pgmname[i1] != '/' && pgmname[i1] != ']' && pgmname[i1] != '(')
	{
#if OS_WIN32
		pgmname[i1] = (CHAR) toupper(pgmname[i1]);
#endif
		i1--;
	}
	if (i1 >= 0) memmove(pgmname, &pgmname[i1 + 1], strlen(&pgmname[i1]));
	else {
		miogetname(&ptr1, &ptr2);
		if (libptr[0] != NULL && !ptr2[0]) libflg = TRUE;
	}

	pgmxmod = pgmdupmod = -1;
	if (chnflg == MTYPE_PRELOAD || chnflg == MTYPE_LOADMOD) {
		/* determine if program and data already loaded */
		/* also find first un-used program slot */
		pgmxmod = pgmhi;
		for (i1 = pgmhi; i1--; ) {
			if (pgmtab[i1].pgmmod == -1) {
				pgmxmod = i1;
				continue;
			}
			if (!strcmp(pgmname, *(*pgmtab[i1].codeptr)->nameptr)) {
				pgmxmod = pgmdupmod = i1;
				break;
			}
		}

		/* found preload/loadmod program by same name */
		if (pgmdupmod != -1) {
			if (pgmtab[pgmdupmod].typeflg != chnflg) return(153);
			i1 = pgmtab[pgmdupmod].dataxmod;
			i2 = -1;
			while (i1 != -1) {
				/* if instance found, make module current */
				if (!strcmp(dataname, *datatab[i1].nameptr)) {
					if (i2 == -1) {
						return(0);
					}

					/* swap modules */
					datatab[i2].nextmod = datatab[i1].nextmod;
					datatab[i1].nextmod = pgmtab[pgmdupmod].dataxmod;
					pgmtab[pgmdupmod].dataxmod = i1;
					if (pgmxmodule == pgmdupmod) {
						dataxmodule = i1;
						datamodule = datatab[dataxmodule].datamod;
						data = *datatab[dataxmodule].dataptr;
					}
					return(0);
				}
				i2 = i1;
				i1 = datatab[i1].nextmod;
			}
		}
	}
	handle = ERR_FNOTF;
	srvhandle = 0;
	if (libflg) {
		for (i1 = 0; libptr[i1] != NULL; i1++) {
			strcpy(libname, *libptr[i1]);
			srvhandle = srvgetserver(libname, FIO_OPT_DBCPATH);
			i2 = (INT) strlen(libname);
			libname[i2++] = '(';
			strcpy(&libname[i2], pname);
			strcat(&libname[i2], ")");
			if (srvhandle) {
				if (srvhandle == -1) {
					i1 = 161;
					goto newmod4;
				}
				handle = fsopenraw(srvhandle, libname, FS_READONLY);
				if (handle < 0) {
					/* convert back to fio type error (required for pre-fs 2.1.3) */
					if (handle == -601) handle = ERR_FNOTF;
					else if (handle == -602 || handle == -607) handle = ERR_NOACC;
					else if (handle == -603) handle = ERR_BADNM;
					else if (handle == -614) handle = ERR_NOMEM;
					else if (handle < -100) handle = ERR_OPERR;
					else if (handle == -1) {
						i1 = (INT) strlen(name);
						fsgeterror(name + i1 + 2, sizeof(name) - i1 - 2);
						if (name[i1 + 2]) {
							name[i1] = ',';
							name[i1 + 1] = ' ';
						}
						i1 = 162;
						goto newmod4;
					}
				}
			}
			else handle = fioopen(libname, FIO_M_SRO | FIO_P_DBC);
			if (handle != ERR_FNOTF) break;
			srvhandle = 0;
		}
	}
	if (handle == ERR_FNOTF) {
		miofixname(pname, ".dbc", FIXNAME_EXT_ADD);
		srvhandle = srvgetserver(pname, FIO_OPT_DBCPATH);
		if (srvhandle) {
			if (srvhandle == -1) {
				i1 = 161;
				goto newmod4;
			}
			handle = fsopenraw(srvhandle, pname, FS_READONLY);
			if (handle < 0) {
				/* convert back to fio type error (required for pre-fs 2.1.3) */
				if (handle == -601) handle = ERR_FNOTF;
				else if (handle == -602 || handle == -607) handle = ERR_NOACC;
				else if (handle == -603) handle = ERR_BADNM;
				else if (handle == -614) handle = ERR_NOMEM;
				else if (handle < -100) handle = ERR_OPERR;
				else if (handle == -1) {
					i1 = (INT) strlen(name);
					fsgeterror(name + i1 + 2, sizeof(name) - i1 - 2);
					if (name[i1 + 2]) {
						name[i1] = ',';
						name[i1 + 1] = ' ';
					}
					i1 = 162;
					goto newmod4;
				}
			}
		}
		else {
			handle = fioopen(pname, FIO_M_SRO | FIO_P_DBC);
		}
	}

	/* reset memory pointers */
	datatab = *datatabptr;
	pgmtab = *pgmtabptr;

	if (handle < 0) {
		if (handle == ERR_FNOTF) i1 = 101;
		else if (handle == ERR_NOACC) i1 = 105;
		else if (handle == ERR_BADNM) i1 = 106;
		else if (handle == ERR_NOMEM) {
			i1 = 107;
		}
		else i1 = 1130 - handle;
		goto newmod4;
	}

	/* check for bad compile */
	rpos = rbyte = MODBUFSIZE;
	fpos = 0L;
	i1 = modread(NULL, -1); // Note, modifies rbyte
	if (i1) {
		if (i1 < 0) i1 = 1130 - i1;
		goto newmod4;
	}
#ifdef SPECIAL
	c2 = dbcbuf[0];
	c1 = dbcbuf[1];
#else
	c1 = dbcbuf[0]; // In the 'paid for' versions, this is the bitwise NOT of c2. i.e. c1 = ~c2;
	                // But in the 'free' versions, c1 must be equal to c2. The compiler does this.
	//
	// Update, the above comment is obsolete as of July 8 2022
	// Per Don, these two bytes are once again the bitwise NOT of each other and c1 is DX_18_MAJOR_VERSION
	//
	c2 = dbcbuf[1];
#endif
//	if (rbyte < 8 || c1 < 3 || c1 > DX_MAJOR_VERSION
//			|| c1 != c2 || (c1 > 7 && rbyte < 12))
	if (rbyte < 8 || c1 < 3 || c1 > DX_18_MAJOR_VERSION
			|| (UCHAR) (c1 ^ c2) != 0xFF || (c1 > 7 && rbyte < 12))
	{
		i1 = 102;
		goto newmod4;
	}

	memset((UCHAR *) &datawrk, 0, sizeof(datawrk));
	datawrk.classmod = -1;
	datawrk.nextclass = -1;
	datawrk.prevclass = -1;
	datawrk.makelabel = 0xFFFF;
	datawrk.destroylabel = 0xFFFF;
	memset(&pgmwrk, 0, sizeof(pgmwrk));
	pgmwrk.typeflg = (UCHAR) chnflg;
	pgmwrk.dataxmod = -1;
	memset(&codewrk, 0, sizeof(codewrk));
	codewrk.version = c1;

	codewrk.pcntsize = 2;
	rpos = 8;
	if (codewrk.version >= 7) {
		if (codewrk.version >= 8) {
			codewrk.pcntsize = 3;
			if (dbcbuf[3] & 0x02) {
				// Means compiled by STDCMP not DBCMP
				codewrk.stdcmp = TRUE;
			}
			mscntoi(&dbcbuf[4], &codewrk.htime, 8);
			mscntoi(&dbcbuf[12], &codewrk.ltime, 8);
			rpos = 22 + llhh(&dbcbuf[20]);
#ifdef SPECIAL
			mscntoi(&dbcbuf[25], &i1, 10);
			if (i1 != license) {
				i1 = 102;
				goto newmod4;
			}
#endif
		}
		else {
			if (dbcbuf[3] & 0x01) codewrk.pcntsize = 3;
			codewrk.ltime = (hhll(&dbcbuf[4]) << 16) + hhll(&dbcbuf[6]);
			codewrk.htime = codewrk.ltime / 100000000;
			codewrk.ltime %= 100000000;
		}
		dbcflags &= ~DBCFLAG_PRE_V7;  /* added 05-25-2000 */
	}
	else {
		if (codewrk.version < 4) rpos = 2;
		dbcflags |= DBCFLAG_PRE_V7;
	}

	if (chnflg == MTYPE_CHAIN && !firstchn) {  /* close files, initialize, etc */
		/* we are now committed to loading this program */
		/* save dataptr for common */
		datawrk.dataptr = datatab[dataxchn].dataptr;
		/* store program name incase of failure */
		strcpy(*(*dummycode)->nameptr, pgmname);

		/*
		 * Unload non-permanant secondary modules (LOADMOD),
		 * and the previous CHAIN (primary module) if found.
		 * CLASS types are not unloaded here.
		 */
		for (i1 = pgmhi; i1--; ) {
			if (pgmtab[i1].typeflg != MTYPE_CHAIN && pgmtab[i1].typeflg != MTYPE_LOADMOD) continue;
			unloadmod(i1, TRUE);
		}
		while (datahi && datatab[datahi - 1].datamod == -1) datahi--;
		while (pgmhi && pgmtab[pgmhi - 1].pgmmod == -1) pgmhi--;
		memcompact();  /* close memory gaps */
		/* reset memory pointers */
		datatab = *datatabptr;
		pgmtab = *pgmtabptr;
	}

	/*
	 * get data module
	 *
	 * Start at index 1 in datatab (zero is the special global area)
	 * and set dataxmod to the first unused one found.
	 */
	for (dataxmod = 1; dataxmod < datahi && datatab[dataxmod].datamod != -1; dataxmod++);
	if (dataxmod == datamax) {  /* expand data table if needed */
		i1 = datamax << 1;
		if (memchange((UCHAR **) datatabptr, i1 * sizeof(DATAMOD), MEMFLAGS_ZEROFILL)) {
			i1 = 107;
			goto newmod4;  /* won't happen on non-first chain */
		}
		/* reset memory pointers */
		pgmtab = *pgmtabptr;
		datatab = *datatabptr;
		/* Mark all the newly create DATAMOD structures as 'not in use' */
		while (datamax < i1) datatab[datamax++].datamod = -1;
	}

	/* get program module */
	if (pgmxmod == -1) {
		/**
		 * For loadmod and ploadmod, pgmxmod will be -1 if the code was never loaded.
		 * If this is a module already loaded, it will be the index into pgmtab of the code
		 */
		for (pgmxmod = 0; pgmxmod < pgmhi && pgmtab[pgmxmod].pgmmod != -1; pgmxmod++);
	}
	if (pgmxmod == pgmmax) {  /* expand program table if needed */
		/* won't happen on non-first chain */
		i1 = pgmmax << 1;
		if (memchange((UCHAR **) pgmtabptr, i1 * sizeof(PGMMOD), MEMFLAGS_ZEROFILL)) {
			i1 = 107;
			goto newmod4;  /* won't happen on non-first chain */
		}
		/* reset memory pointers */
		datatab = *datatabptr;
		pgmtab = *pgmtabptr;
		/* Mark all the newly created PGMMOD structures as 'not in use' */
		while (pgmmax < i1) pgmtab[pgmmax++].pgmmod = -1;
	}

	/* establish extended data and program module here in case an error occurs */
	do {
		i2 = dataxcnt;
		if (dataxcnt++ == 0x7FFF) {
			dataxcnt = 1;
		}
		for (i1 = 0; i1 < datahi && i2 != datatab[i1].datamod; i1++);
	} while (i1 < datahi);
	datawrk.datamod = i2;

	if (pgmdupmod == -1) {
		do {
			i2 = pgmxcnt;
			if (pgmxcnt++ == INT_MAX /* 2147483647 */) {
				pgmxcnt = 1;
			}
			for (i1 = 0; i1 < pgmhi && i2 != pgmtab[i1].pgmmod; i1++);
			/*
			 * If the above loop finds a pgmmod in pgmtab that equals the number we hope to assign,
			 * then i1 will be less than pgmhi and we go back and add one and try again.
			 * If the number is unique, the below while test will be false, we will fall out of do loop
			 * and the number will be used.
			 */
		} while (i1 < pgmhi);
		pgmwrk.pgmmod = i2;
	}
	else pgmwrk.pgmmod = pgmtab[pgmdupmod].pgmmod;

	if (pgmdupmod == -1) {
		/*
		 * Seems to be some redundancy here. For LOADMOD and PLOADMOD if we get here,
		 * the below search loop cannot succeed.
		 * So this must be for CLASS. CHAIN should not find a match either.
		 */
		int found = FALSE;
		for (i1 = pgmhi; i1--; ) {
			if (pgmtab[i1].pgmmod == -1) continue;
			if (!strcmp(pgmname, *(*pgmtab[i1].codeptr)->nameptr)) {
				pgmwrk.codeptr = pgmtab[i1].codeptr;
				found = TRUE;
				break;
			}
		}
	}
	else pgmwrk.codeptr = pgmtab[pgmxmod].codeptr;

	/* from this point on, an error must goto newmod2 or newmod3 */
	if (pgmwrk.codeptr != NULL) {
		if (codewrk.version != (*pgmwrk.codeptr)->version ||
		    codewrk.ltime != (*pgmwrk.codeptr)->ltime ||
		    codewrk.htime != (*pgmwrk.codeptr)->htime) i1 = 154;
		else i1 = 0;
		codewrk = **pgmwrk.codeptr;
		codewrk.codecnt++;
		if (i1) goto newmod3;
	}
	else {
		pgmwrk.codeptr = (CODEINFO **) memalloc(sizeof(CODEINFO), MEMFLAGS_ZEROFILL);
		if (pgmwrk.codeptr == NULL) {
			goto newmod2;
		}
		codewrk.codecnt = 1;

		/* allocate memory for the names */
		codewrk.nameptr = (CHAR **) memalloc((INT) strlen(pgmname) + 1, 0);
		codewrk.fileptr = (CHAR **) memalloc((INT) strlen(pname) + 1, 0);
		if (codewrk.nameptr == NULL || codewrk.fileptr == NULL) {
			goto newmod2;
		}
		/* reset memory pointers */
		datatab = *datatabptr;
		pgmtab = *pgmtabptr;
		strcpy(*codewrk.nameptr, pgmname);
		strcpy(*codewrk.fileptr, pname);
	}

	if (chnflg != MTYPE_CHAIN) {  /* store instance/class name */
		datawrk.nameptr = (CHAR **) memalloc((INT) strlen(dataname) + 1, 0);
		if (datawrk.nameptr == NULL) {
			goto newmod2;
		}
		/* reset memory pointers */
		datatab = *datatabptr;
		pgmtab = *pgmtabptr;
		strcpy(*datawrk.nameptr, dataname);
	}

	if (codewrk.version <= 8) {
		tables[0] = 0x00;  /* data */
		if (codewrk.version <= 7) {
			/* fill xdef, xref, ltab, lext */
			tables[1] = 0x02;
			tables[2] = 0x03;
			tables[3] = 0x01;
			tables[4] = 0x06;
			tables[5] = 0x7F;
			if (codewrk.version <= 5) tables[4] = 0x7F;
		}
		tabcnt = 0;
	}
	for ( ; ; ) {
		if (rpos + 7 > rbyte && rbyte == MODBUFSIZE) {  /* get more data */
			i1 = modread(NULL, -1);
			if (i1) {
				/* modread cannot return a 107 */
				goto newmod3;
			}
		}
		if (rpos == rbyte) break;
		if (codewrk.version >= 9) {
			//Tag byte, unique value for each segment type
			c1 = dbcbuf[rpos++];
			//Length of segment in LLMMHH format (not including segment tag bytes 0-3)
			nwork = llmmhh(&dbcbuf[rpos]);
			rpos += 3;
		}
		else if (codewrk.version == 8) {
			if (!tabcnt) c1 = tables[tabcnt++];
			else {
				c1 = dbcbuf[rpos++];
				if (c1 != 0x7F) {
					nwork = llmmhh(&dbcbuf[rpos]);
					rpos += 3;
				}
				else {
					c1 = 0x04;
					if (srvhandle) {
						i1 = fsfilesize(handle, &eofpos);
						if (i1 == -1) {
							i1 = (INT) strlen(name);
							fsgeterror(name + i1 + 2, sizeof(name) - i1 - 2);
							if (name[i1 + 2]) {
								name[i1] = ',';
								name[i1 + 1] = ' ';
							}
							i1 = 162;
						}
					}
					else i1 = fiogetsize(handle, &eofpos);
					if (i1) {
						/* fiogetsize cannot return a 107 */
						goto newmod3;
					}
					nwork = (INT)(eofpos - fpos) + (rbyte - rpos);
				}
			}
		}
		else {
			c1 = tables[tabcnt++];
			if (c1 != 0x00) {
				if (c1 != 0x7F) {
					nwork = llhh(&dbcbuf[rpos]);
					rpos += 2;
				}
				else {
					c1 = 0x04;
					if (srvhandle) {
						i1 = fsfilesize(handle, &eofpos);
						if (i1 == -1) {
							i1 = (INT) strlen(name);
							fsgeterror(name + i1 + 2, sizeof(name) - i1 - 2);
							if (name[i1 + 2]) {
								name[i1] = ',';
								name[i1 + 1] = ' ';
							}
							i1 = 162;
						}
					}
					else i1 = fiogetsize(handle, &eofpos);
					if (i1) goto newmod3;
					nwork = (INT)(eofpos - fpos) + (rbyte - rpos);
				}
			}
		}

		if (c1 == 0x00) {  /* data area */
			if (chnflg != MTYPE_CLASS) {
				if (codewrk.version >= 8) {
#ifdef SPECIAL
					datawrk.size = llmmhh(&dbcbuf[22]);
#else
					datawrk.size = llmmhh(&dbcbuf[rpos]);
#endif
					rpos += 3;
				}

				i1 = loaddata(&datawrk.dataptr, chnflg, codewrk.version, datawrk.datamod, &datawrk.size, &datawrk.maxsize);
				if (i1) goto newmod3;
			}
			else modread(NULL, nwork);
		}
		else if (c1 == 0x01) {  /* execution label table */
			if (chnflg != MTYPE_CLASS) {
				i1 = loadlabel(nwork, codewrk.pcntsize, codewrk.version, pgmwrk.pgmmod,
						&datawrk.ltabptr, &datawrk.lmodptr, &datawrk.lextptr);
				if (i1) goto newmod3;
			}
			else modread(NULL, nwork);
		}
		else if (c1 == 0x02) {  /* external definition table */
			if (chnflg != MTYPE_CLASS && codewrk.xdefptr == NULL) {
				codewrk.xdefptr = memalloc(nwork, 0);
				if (codewrk.xdefptr == NULL) goto newmod2;
				i1 = modread(*codewrk.xdefptr, nwork);
				if (i1) goto newmod3;
				codewrk.xdefsize = nwork;
			}
			else modread(NULL, nwork);
		}
		else if (c1 == 0x03) {  /* external reference table */
			if (chnflg != MTYPE_CLASS && codewrk.xrefptr == NULL) {
				codewrk.xrefptr = memalloc(nwork + 1, 0);
				if (codewrk.xrefptr == NULL) goto newmod2;
				if (codewrk.version <= 8) {
					(*codewrk.xrefptr)[0] = 0;
					i1 = 1;
				}
				else i1 = 0;
				i1 = modread(&(*codewrk.xrefptr)[i1], nwork);
				if (i1) goto newmod3;
			}
			else modread(NULL, nwork);
		}
		else if (c1 == 0x04) {  /* program area */
			if (codewrk.pgmptr == NULL) {
				codewrk.pgmptr = memalloc(nwork, 0);
				if (codewrk.pgmptr == NULL) goto newmod2;
				i1 = modread(*codewrk.pgmptr, nwork);
				if (i1) goto newmod3;
				codewrk.pgmsize = nwork;
			}
			else modread(NULL, nwork);
		}
		else if (c1 == 0x05) {  /* class definition segment */
			if (chnflg == MTYPE_CLASS) {
				if (rpos + 103 > rbyte && rbyte == MODBUFSIZE) {  /* get more data */
					i1 = modread(NULL, -1);
					if (i1) goto newmod3;
				}
/*** CODE: CAN OPTIMIZE BY SAVING POSITION OF PARENT ***/
				if (!classflg && !strcmp(dataname, (CHAR *) &dbcbuf[rpos + 4])) {
					classflg = TRUE;
					i1 = rpos;
					datawrk.makelabel = hhll(&dbcbuf[rpos]);
					datawrk.destroylabel = hhll(&dbcbuf[rpos + 2]);
					rpos += (INT) strlen(dataname) + 5;
					if (dbcbuf[rpos]) {  /* save parents name to load next */
						strcpy(dataname, (CHAR *) &dbcbuf[rpos]);
						rpos += (INT) strlen(dataname);
						strcpy(name, (CHAR *) &dbcbuf[rpos + 1]);
						if (!name[0]) strcpy(name, *codewrk.fileptr);
						pname = name;
					}
					else pname = NULL;
					rpos += (INT) strlen((CHAR *) &dbcbuf[rpos + 1]) + 2;
					classsize = nwork - (rpos - i1);
					while (classsize >= 4) {
						if (rpos + 7 > rbyte && rbyte == MODBUFSIZE) {  /* get more data */
							i1 = modread(NULL, -1);
							if (i1) goto newmod3;
						}
						if (rpos + 4 > rbyte) break;
						c1 = dbcbuf[rpos++];
						nwork = llmmhh(&dbcbuf[rpos]);
						rpos += 3;
						classsize -= 4 + nwork;
						if (!nwork) continue;

						if (c1 == 0x00) {  /* data area */
							datawrk.size = llmmhh(&dbcbuf[rpos]);
							rpos += 3;
							i1 = loaddata(&datawrk.dataptr, chnflg, codewrk.version, datawrk.datamod, &datawrk.size, &datawrk.maxsize);
							if (i1) goto newmod3;
						}
						else if (c1 == 0x01) {  /* execution label table */
							i1 = loadlabel(nwork, codewrk.pcntsize, codewrk.version, pgmwrk.pgmmod,
									&datawrk.ltabptr, &datawrk.lmodptr, &datawrk.lextptr);
							if (i1) goto newmod3;
						}
						else if (c1 == 0x02) {  /* external definition table */
							pgmwrk.xdefptr = memalloc(nwork, 0);
							if (pgmwrk.xdefptr == NULL) {
								goto newmod2;
							}
							i1 = modread(*pgmwrk.xdefptr, nwork);
							if (i1) goto newmod3;
							pgmwrk.xdefsize = nwork;
						}
						else if (c1 == 0x03) {  /* external reference table */
							pgmwrk.xrefptr = memalloc(nwork, 0);
							if (pgmwrk.xrefptr == NULL) {
								goto newmod2;
							}
							i1 = modread(*pgmwrk.xrefptr, nwork);
							if (i1) goto newmod3;
						}
						else if (c1 == 0x06) {  /* inherited variables */
							datawrk.varptr = memalloc(nwork, 0);
							if (datawrk.varptr == NULL) {
								goto newmod2;
							}
							i1 = modread(*datawrk.varptr, nwork);
							if (i1) goto newmod3;
							datawrk.varsize = nwork;
						}
					}
					if (classsize) {
						i1 = 102;
						goto newmod3;
					}
				}
				else modread(NULL, nwork);
			}
			else modread(NULL, nwork);
/*** CODE: CAN OPTIMIZE BY QUITING IF PROGRAM IS ALREADY LOADED ***/
		}
		else if (c1 == 0x06) {  /* version 6 & 7 label-external reference table */
			if (nwork) {
				lextsize = nwork >> 1;
				datawrk.lextptr = memalloc(lextsize * sizeof(USHORT), 0);
				if (datawrk.lextptr == NULL) {
					goto newmod2;
				}
				i1 = modread(*datawrk.lextptr, nwork);
				if (i1) goto newmod3;
				p = *datawrk.lextptr;
				nptr = (INT *) *datawrk.ltabptr;
				for (i2 = 0; i2 < lextsize; i2 += 2) {
					nptr[llhh(p)] = 0x00800000 + llhh(p + 2);
					p += 4;
				}
			}
		}
		else {
			i1 = 102;
			goto newmod3;
		}
	}
	if (chnflg == MTYPE_CLASS && datawrk.dataptr == NULL) {  /* did not find class */
		i1 = 157;
		goto newmod3;
	}
	i1 = 0;  /* set return code */

newmod1:
	if (srvhandle) {
		fsclose(handle);
	}
	else fioclose(handle);

	/* reset memory pointers */
	datatab = *datatabptr;
	pgmtab = *pgmtabptr;

	if (pgmdupmod == -1) {  /* finish initializing program structure */
		pgmtab[pgmxmod] = pgmwrk;
		**pgmwrk.codeptr = codewrk;
	}

	/* finish initializing data structure */
	datawrk.pgmxmod = pgmxmod;
	if (chnflg != MTYPE_CLASS) datawrk.nextmod = pgmtab[pgmxmod].dataxmod;
	else {
		if (childmod == -1) {  /* first child */
			classdmod = dataxmod;
			classpmod = pgmxmod;
		}
		datawrk.nextmod = childmod;
		childmod = dataxmod;
	}
	pgmtab[pgmxmod].dataxmod = dataxmod;
	datatab[dataxmod] = datawrk;

	if (dataxmod == datahi) datahi++;
	if (pgmxmod == pgmhi) pgmhi++;

	if (chnflg == MTYPE_CHAIN) {  /* its a chain to program */
		while (!popreturnstack());
		firstchn = FALSE;
		/* set starting points */
		dataxchn = dataxmodule = dataxmod;
		datachn = datatab[dataxmod].datamod;
		pgmxmodule = pgmxmod;
		pgmchn = pgmtab[pgmxmod].pgmmod;
		pcount = 0;
		dbcflags &= ~DBCFLAG_FLAGS;
	}
	else if (chnflg == MTYPE_CLASS) {
		if (pname != NULL) {
			classflg = FALSE;
			goto newmod0;  /* load parent */
		}

		/* resolve inherited variables and push make labels */
		i1 = loadclass(childmod);
		if (!i1) {
			datatab[classdmod].prevclass = *mod;
			datatab[classdmod].nextclass = datatab[*mod].classmod;
			if (datatab[*mod].classmod != -1) datatab[datatab[*mod].classmod].prevclass = classdmod;
			datatab[*mod].classmod = classdmod;
			*mod = pgmtab[classpmod].pgmmod;
		}
	}

	if (pgmxmod == pgmxmodule) dataxmodule = pgmtab[pgmxmodule].dataxmod;
	lastdataxmod = dataxmodule;
	lastdatamod = datamodule = datatab[dataxmodule].datamod;
	data = *datatab[dataxmodule].dataptr;
	pgmmodule = pgmtab[pgmxmodule].pgmmod;
	pgm = *(*pgmtab[pgmxmodule].codeptr)->pgmptr;
	return(i1);

newmod2:
	i1 = ERR_NOMEM;
newmod3:
	/* variable i1 contains the error */
	if (i1 < 0) {
		if (i1 == ERR_NOMEM) {
			i1 = 107;
		}
		else i1 = 1130 - i1;
	}

	/* free program memory pointers */
	memfree(pgmwrk.xrefptr);
	memfree(pgmwrk.xdefptr);
	if (codewrk.codecnt == 1) {
		memfree(codewrk.pgmptr);
		memfree(codewrk.xrefptr);
		memfree(codewrk.xdefptr);
		memfree((UCHAR **) codewrk.fileptr);
		memfree((UCHAR **) codewrk.nameptr);
		memfree((UCHAR **) pgmwrk.codeptr);
	}
	else if (codewrk.codecnt > 1) {
		codewrk.codecnt--;
		**pgmwrk.codeptr = codewrk;
	}

	/* free data memory pointers */
	memfree(datawrk.varptr);
	memfree(datawrk.lextptr);
	memfree(datawrk.lmodptr);
	memfree(datawrk.ltabptr);
	if (chnflg != MTYPE_CHAIN) memfree(datawrk.dataptr);
	memfree((UCHAR **) datawrk.nameptr);

	/* reset memory pointers */
	datatab = *datatabptr;
	pgmtab = *pgmtabptr;

	if (!firstchn) {
		if (chnflg == MTYPE_CHAIN) {  /* set up chain module pointing to the dummy code */
			datawrk.nameptr = NULL;
			datawrk.lmodptr = NULL;
			datawrk.ltabptr = NULL;
			datawrk.lextptr = NULL;
			pgmwrk.codeptr = dummycode;
			pgmwrk.xdefptr = NULL;
			pgmwrk.xrefptr = NULL;
			codewrk = **dummycode;
			codewrk.codecnt = 2;
			goto newmod1;
		}
	}

newmod4:
	if (chnflg == MTYPE_CLASS && childmod != -1) {  /* parent failed, unload children */
		datawrk = datatab[childmod];
		childmod = datawrk.nextmod;
		pgmwrk = pgmtab[datawrk.pgmxmod];
		codewrk = **pgmwrk.codeptr;
		goto newmod3;
	}

	if (handle >= 0) {
		if (srvhandle) {
			fsclose(handle);
		}
		else fioclose(handle);
	}
	if (!firstchn) setpgmdata();
	return(i1);
}

static INT loadclass(INT childmod)
{
	INT i1, i2, i3, label, mod, off, parentmod, pushcnt, savedataxmod, savepgmxmod;
	UCHAR *ptr1, *ptr2, *ptr3, *ptr4, *ptr5, *dataptr;

	pushcnt = 0;

	/* resolve the inherited variables */
	parentmod = -1;
	while (childmod != -1) {
		if (datatab[childmod].varsize) {
			dataptr = *datatab[childmod].dataptr;
			ptr1 = *datatab[childmod].varptr;
			ptr2 = ptr1 + datatab[childmod].varsize;
			while (ptr1 < ptr2) {
				off = llmmhh(ptr1);
				ptr1 += 3;
				if (dataptr[off] == 0xFF) {  /* unresolved address */
					for (i1 = parentmod; i1 != -1; i1 = datatab[i1].nextmod) {
						if (datatab[i1].varsize) {
							ptr3 = *datatab[i1].varptr;
							ptr4 = ptr3 + datatab[i1].varsize;
							while (ptr3 < ptr4) {
								ptr3 += 3;
								if (!strcmp((CHAR *) ptr1, (CHAR *) ptr3)) {
									i2 = llmmhh(ptr3 - 3);
									ptr5 = *datatab[i1].dataptr + i2;
									if (*ptr5 == 0xBE) {
										i1 = getdatax(llhh(ptr5 + 1));
										if (i1 == RC_ERROR) {
											i1 = 505;
											goto loadclass1;
										}
										i2 = llmmhh(ptr5 + 3);
										ptr5 = *datatab[i1].dataptr + i2;
									}
									i3 = adrtype(ptr5);
									if (i3 == -1 || (0x70 + i3) != (INT) dataptr[off + 3]) {
										i1 = 159;
										goto loadclass1;
									}
									i1 = datatab[i1].datamod;
									dataptr[off] = 0xBE;
									dataptr[off + 1] = (UCHAR) i1;
									dataptr[off + 2] = (UCHAR)(i1 >> 8);
									dataptr[off + 3] = (UCHAR) i2;
									dataptr[off + 4] = (UCHAR)(i2 >> 8);
									dataptr[off + 5] = (UCHAR)(i2 >> 16);
									break;
								}
								ptr3 += strlen((CHAR *) ptr3) + 1;
							}
							if (ptr3 < ptr4) break;
						}
					}
					if (i1 == -1) {
						i1 = 158;
						goto loadclass1;
					}
				}
				ptr1 += strlen((CHAR *) ptr1) + 1;
			}
		}

		/* reverse the pointers for this module */
		i1 = childmod;
		childmod = datatab[i1].nextmod;
		datatab[i1].nextmod = parentmod;
		parentmod = i1;
	}

	/* save data & program modules in case of error */
	savedataxmod = dataxmodule;
	savepgmxmod = pgmxmodule;

	/* push make labels, starting with child's */
	for (mod = parentmod; mod != -1; mod = datatab[mod].nextmod) {
		if (datatab[mod].makelabel != 0xFFFF) {
			label = datatab[mod].makelabel;
			if (pushreturnstack(-2)) {
				i1 = 501;
				goto loadclass1;
			}
			pushcnt++;

			i1 = datatab[mod].pgmxmod;
			i2 = ((INT *) *datatab[mod].lmodptr)[label];
			if (i2 != -1) i2 = ((INT *) *datatab[mod].ltabptr)[label];
			else if (getpcntx(label, mod, &i1, &i2, TRUE) == -1) {
				i1 = 562;
				goto loadclass1;
			}
			pgmxmodule = i1;
			pgmmodule = pgmtab[pgmxmodule].pgmmod;
			dataxmodule = pgmtab[pgmxmodule].dataxmod;
			datamodule = datatab[dataxmodule].datamod;
			pcount = i2;
		}
	}
	return(0);

	/*
	 * Error exit point
	 */
loadclass1:
	/* variable i1 contains the error */
	if (parentmod != -1) unloadmod(datatab[parentmod].pgmxmod, TRUE);
	if (childmod != -1) unloadmod(datatab[childmod].pgmxmod, TRUE);

	while (datahi && datatab[datahi - 1].datamod == -1) datahi--;
	while (pgmhi && pgmtab[pgmhi - 1].pgmmod == -1) pgmhi--;

	if (pushcnt) {
		while (pushcnt--) popreturnstack();
		dataxmodule = savedataxmod;
		pgmxmodule = savepgmxmod;
		/* newmod will restore program and data */
	}
	return(i1);
}

/**
 * Returns zero for success. Non-zero for error condition
 * Can move memory
 */
static INT loadlabel(INT size, INT pcntsize, INT version, INT pgmmod,
		UCHAR ***ltabptr, UCHAR ***lmodptr, UCHAR ***lextptr)
{
	INT i1, ltabsize, lextsize;
	INT nwork, *nptr;
	INT *intptr;
	UCHAR *ptr;

	ltabsize = size / pcntsize;
	*ltabptr = memalloc(ltabsize * sizeof(INT), 0);

	/*
	 * This 'label-module' table has a INT entry for every label referenced.
	 * It corresponds to the fact that the 'unique program module number'
	 * is an INT and cannot exceed INT_MAX.
	 * If it is local, then the entry in this table will be THIS module's unique number.
	 * If it is external, it is marked as -1 so the runtime knows when
	 * it is called that it needs to be resolved.
	 */
	*lmodptr = memalloc(ltabsize * sizeof(INT), 0);

	if (*ltabptr == NULL || *lmodptr == NULL) return(ERR_NOMEM);
	i1 = modread(**ltabptr, size);
	if (i1) return(i1);
	nptr = (INT *) **ltabptr;
	ptr = **ltabptr + size;
	intptr = (INT *) **lmodptr;
	lextsize = 0;
	if (version >= 8) {
		for (i1 = ltabsize; i1--; ) {
			ptr -= 3;
			nwork = llmmhh(ptr);
			if (nwork & 0x00800000) {
				/* If ALL of the bits are 1, it is a LABEL variable */
				if (nwork != 0x00FFFFFF) lextsize++;  /* external/method */
				/*
				 * For both a LABEL-variable and an external ref, set the label-module
				 * table entry to 0xFFFFFFFF
				 */
				intptr[i1] = -1;
			}
			else intptr[i1] = pgmmod;
			nptr[i1] = nwork;
		}
		if (lextsize) {
			lextsize <<= 1;
			*lextptr = memalloc(lextsize * sizeof(USHORT), 0);
			if (*lextptr == NULL) return(ERR_NOMEM);
		}
	}
	else if (version >= 6) {
		for (i1 = ltabsize; i1--; ) {
			ptr -= pcntsize;
			if (pcntsize == 2) {
				nwork = llhh(ptr);
				if (nwork == 0xFFFF) nwork = 0x00FFFFFF;
			}
			else nwork = llmmhh(ptr);
			if (nwork & 0x00800000) intptr[i1] = -1;
			else intptr[i1] = pgmmod;
			nptr[i1] = nwork;
		}
	}
	else {  /* pre-version 6 */
		for (i1 = ltabsize; i1--; ) {
			ptr -= 2;
			nwork = llhh(ptr);
			if (nwork >= 0xE000) {
				nwork += 0x007F2003;  /* 0x800000 - 0xE000 + 3 (3 unused header bytes) */
				lextsize++;
				intptr[i1] = -1;
			}
			else intptr[i1] = pgmmod;
			nptr[i1] = nwork;
		}
		if (lextsize) {
			lextsize <<= 1;
			*lextptr = memalloc(lextsize * sizeof(USHORT), 0);
			if (*lextptr == NULL) return(ERR_NOMEM);
		}
	}
	return(0);
}

static INT loaddata(UCHAR ***dataptr, INT chnflg, INT version, INT datamod,
		INT *datasize, INT *datamax)
{
	static INT lastsize = 0;
	INT i1, i2, i3, i4, flags, sizergt, type;
	INT datatype, dsize, dataoff, globaloff;
	INT arraycnt, arraysiz, arrayhdrsiz, size;
	UCHAR c1, c2, arrayhdr[9], gname[33], work[33];
	UCHAR *p, *pstart, *ptr;
	DAVB *davb;
	GLOBALINFO *globalptr, **globalpptr, **globalpptr1, **globalpptr2;

	/* allocate data area memory */
	i1 = *datasize;
	if (i1) {
		dsize = i1 + 280;
		if (dsize & 0x03FF) dsize = (dsize & ~0x03FF) + 0x0400;
	}
	else {
		i1 = 4096;
		dsize = 49 << 10;
	}
	do {
		if (*dataptr == NULL) {
			*dataptr = memalloc(dsize, 0);
			if (*dataptr != NULL) break;
		}
		else if (!memchange(*dataptr, dsize, 0)) break;
		dsize -= 4096;
	} while (dsize >= i1);
	if (dsize < i1) {
		return(ERR_NOMEM);
	}

	/* reset data table */
	datatab = *datatabptr;

	flags = dbcflags & DBCFLAG_FLAGS;
	p = pstart = **dataptr;
	datatype = 0;

nextdata1:
	if (datatype & DATA_GLOBAL) {
		if (!(datatype & DATA_GLOBALOK)) {
			return(102);
		}
		datatab[0].size = (INT) (p - pstart);
		*p = 0xF2;  /* new end of data */

		/* restore data pointer */
		p = pstart = **dataptr;
		p += dataoff;

		/* create the address variable */
		if (version >= 8) {
			p[0] = (UCHAR)(datatype >> DATA_GTYPESHIFT);
			p[1] = p[2] = 0;
			p += 3;
		}
		else *p++ = 0xCF;
		p[0] = (UCHAR) globaloff;
		p[1] = (UCHAR)(globaloff >> 8);
		p[2] = (UCHAR)(globaloff >> 16);
		p += 3;
	}
	datatype = 0;
	if (((ptrdiff_t)(p - pstart) + 280) > (ptrdiff_t)dsize) {
		return(ERR_NOMEM);  /* not enough memory */
	}

nextdata2:
	if (rpos + 280 > rbyte && rbyte == MODBUFSIZE) {  /* get more data */
		i1 = modread(NULL, -1);
		if (i1) return(i1);
	}

	c1 = dbcbuf[rpos++];
	if (c1 < 0x80) {  /* char */
		size = c1;
		goto mkchar;
	}
	if (c1 < 0xE0) {  /* num */
		if (c1 < 0xA0) {  /* num1 - num31 */
			size = c1 & 0x1F;
			sizergt = 0;
		}
		else if (c1 < 0xC0) {  /* numX.Y, X=0-15, Y=1 or 2 */
			sizergt = (c1 & 0x01) + 1;
			size = ((c1 >> 1) & 0x0F) + sizergt + 1;
		}
		else {  /* num X.Y, X=0-31, Y=1-30 (assume Y != 0) */
			sizergt = dbcbuf[rpos++];
			size = (c1 & 0x1F) + sizergt + 1;
		}
		goto mknum;
	}
	if (c1 < 0xFC) {
		switch(c1) {
			case 0xE0:  /* num literal */
				*p++ = 0xE0;
				size = dbcbuf[rpos++];
				*p++ = (UCHAR) size;
				goto mkdata;
			case 0xE1:  /* char literal */
				*p++ = 0xE1;
				size = dbcbuf[rpos++];
				*p++ = (UCHAR) size;
				goto mkdata;
			case 0xE2:  /* variable name */
				*p++ = 0xFE;
				size = dbcbuf[rpos++];
				memcpy(p, &dbcbuf[rpos], size);
				rpos += size;
				p += size;
				*p++ = 0xFE;
				goto nextdata2;
/*** CODE: MAYBE FILL THIS SO THERE IS NOT A GAP ***/
			case 0xF0:  /* file */
				c1 = DAVB_FILE;
				goto mkdavb;
			case 0xF1:  /* ifile */
				c1 = DAVB_IFILE;
				goto mkdavb;
			case 0xF2:  /* afile */
				c1 = DAVB_AFILE;
				goto mkdavb;
			case 0xF3:  /* global and pre-version 5 common */
				if (version >= 7) goto mkglobal;
				if (version <= 4) {  /* pre-version 5 common */
					size = dbcbuf[rpos++];
					goto mkcom;
				}
				return(102);
			case 0xF4:  /* listend */
				*p++ = 0xA5;
				goto nextdata1;
			case 0xF5:  /* end of data area */
				*p = 0xF2;
				i1 = (INT) (p - pstart);
				if (chnflg == 2) {
					lastsize = i1;
					dioclosemod(datachn);  /* close files */
					comclosemod(datachn);  /* close comfiles */
					prtclosemod(datachn);  /* close pfiles */
					diorenmod(datachn, datamod);
					comrenmod(datachn, datamod);
					prtrenmod(datachn, datamod);
				}
				/* release unused data area */
				if (version < 8 && i1 < dsize) {
					dsize = i1 + 1;
					if (dsize & 0x03FF) dsize = (dsize & ~0x03FF) + 0x0400;
					memchange(*dataptr, dsize, 0);
				}
				*datasize = i1;
				*datamax = dsize;
				dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | flags;
				return(0);
			case 0xF6:  /* initialized char */
				size = dbcbuf[rpos++];
				datatype |= DATA_INIT;
				goto mkchar;
			case 0xF7:  /* initialized number or int or float */
				c1 = dbcbuf[rpos++];
				size = c1 & 0x1F;
				if (c1 < 0x20) {  /* initialized num */
					datatype |= DATA_INIT;
					goto mknum;
				}
				if (c1 < 0x40) {  /* initialized int */
					datatype |= DATA_INIT;
					goto mkint;
				}
				if (c1 < 0x60) {  /* initialized float */
					datatype |= DATA_INIT;
					goto mkfloat;
				}
				if (c1 < 0xA0) return(102);
				if (c1 < 0xC0) goto mkint;  /* int */
				if (c1 < 0xE0) {  /* float */
					sizergt = dbcbuf[rpos++];
					if (sizergt) size += (sizergt + 1);
					goto mkfloat;
				}
				return(102);
			case 0xF8:  /* straight data copy? and pre-version 8 ifile with keylen variable */
				if (version >= 8) {
#if 0
/*** CODE: IT'S DOCUMENTED THAT THIS IS STRAIGHT DATA COPY FOR V8, THIS IS ***/
/***       TO BE SEEN. ALSO WANT TO VERIFY WHAT IS BEING COPIED ***/
					size = llhh(&dbcbuf[rpos]);
					rpos += 2;
					goto mkdata;
#else
					return(102);
#endif
				}
				datatype |= DATA_KEYVAR;
				c1 = 0xA1;
				goto mkdavb;
			case 0xF9:  /* list */
				*p++ = 0xA4;
				goto nextdata1;
			case 0xFA:  /* char address */
			case 0xFB:  /* num address */
				type = c1 - 0xFA;  /* 0 or 1 */
				goto mkadv;
		}
		return(102);
	}
	if (c1 == 0xFC) {  /* many things */
		c1 = dbcbuf[rpos++];
		if (c1 < 0x10) {  /* address variables */
			if (c1 < 0x0E) {
				type = c1;
				goto mkadv;
			}
			return(102);
		}
		if (c1 < 0x30) {
			switch(c1) {
				case 0x10:  /* comfile */
					c1 = DAVB_COMFILE;
					goto mkdavb;
				case 0x11:  /* image */
					c1 = DAVB_IMAGE;
					goto mkdavb;
				case 0x12:  /* queue */
					c1 = DAVB_QUEUE;
					goto mkdavb;
				case 0x13:  /* resource */
					c1 = DAVB_RESOURCE;
					goto mkdavb;
				case 0x14:  /* device */
					c1 = DAVB_DEVICE;
					goto mkdavb;
				case 0x15:  /* pfile */
					c1 = DAVB_PFILE;
					goto mkdavb;
				case 0x16:  /* pfile address */
					type = 0x0C;
					goto mkadv;
				case 0x17:  /* initialized large char */
					size = llhh(&dbcbuf[rpos]);
					rpos += 2;
					datatype |= DATA_INIT;
					goto mkchar;
				case 0x18:  /* object */
					c1 = DAVB_OBJECT;
					goto mkdavb;
				case 0x19:
				case 0x1A:
				case 0x1B:
				case 0x1C:
				case 0x1D:
				case 0x1E:
					return(102);
				case 0x1F:  /* large char */
					size = llhh(&dbcbuf[rpos]);
					rpos += 2;
					goto mkchar;
				case 0x21:
				case 0x22:
				case 0x23:
				case 0x25:
				case 0x26:
				case 0x27:  /* multi-dimensional array address variable */
					type = 0x10 | (c1 & 0x07);
					goto mkadv;
			}
			return(102);
		}
		if (c1 < 0x60) {  /* initialized address variable */
			if (c1 < 0x48) {
				type = c1 & 0x07;
				if (type & 0x03) type |= 0x10;
				else if (type) type = 0x01;
			}
			else if (c1 == 0x48) type = 0x05;
			else if (c1 < 0x52) {
				return(102);
			}
			else if (c1 < 0x5E) type = c1 - 0x50;
			else {
				return(102);
			}
			p[0] = (UCHAR)(0xB0 + type);
			p[1] = (UCHAR) datamod;
			p[2] = (UCHAR)(datamod >> 8);
			memcpy(&p[3], &dbcbuf[rpos], 3);
			rpos += 3;
			p += 6;
			goto nextdata1;
		}
		else if (c1 < 0x70) {
			return(102);
		}
		else if (c1 < 0xC0) {  /* inherited variable */
			memset(p, 0xFF, 6);
			p[3] = c1;
			p += 6;
			goto nextdata1;
		}
		return(102);
	}
	if (c1 == 0xFD) {  /* common leader byte */
		if (chnflg != 2 || (INT)(p - pstart) < lastsize) {
			if (chnflg == 2) scanvar(p);
			datatype |= DATA_COMMON;
		}
		if (version == 5 && dbcbuf[rpos] == 0xAF) {  /* version 5 was special */
			rpos++;
			size = llhh(&dbcbuf[rpos]);
			rpos += 2;
			goto mkchar;
		}
		goto nextdata2;
	}
	if (c1 == 0xFE) {  /* pre-version 8 varlist */
		p[0] = 0xCE;
		p[1] = dbcbuf[rpos++];
		p[2] = dbcbuf[rpos++];
		p[3] = 0;
		p += 4;
		goto nextdata1;
	}
	if (c1 == 0xFF) {  /* initialized char, array, initialized array */
		i1 = dbcbuf[rpos++];
		/*
		 * According to the DX internal design document,
		 * the byte after an 0xFF must be 1, 2, 3, 4, 5, or 6. Any other value is a bad compile
		 * A 1, 2, 3 indicates a 1, 2, or 3 dimensional array is coming.
		 * A 4, 5, 6 indicates the same as a 1, 2, 3, but in addition an initial clause appeared.
		 *
		 * But the code below allows a zero. I do not know what this means.
		 */
		if (!i1) {
			datatype |= DATA_INIT2;
			goto nextdata2;
		}
		if (i1 & 0x04) {
			i1 -= 0x03;
			datatype |= DATA_INIT;
		}
		if (i1 < 1 || 3 < i1) {
			return(102);
		}
		arrayhdr[0] = (UCHAR)(0xA6 + i1 - 1);
		for (i2 = 0, arraycnt = arrayhdrsiz = 1; i2 < i1; i2++) {
			arraycnt *= llhh(&dbcbuf[rpos]);
			arrayhdr[arrayhdrsiz++] = dbcbuf[rpos++];
			arrayhdr[arrayhdrsiz++] = dbcbuf[rpos++];
		}
		datatype |= DATA_ARRAY;
		goto nextdata2;
	}
	return(102);  /* bad object module format */

mkdata:  /* move data from .dbc to data area */
	i1 = modread(p, size);
	if (i1) return(i1);
	p += size;
	goto nextdata1;

mkchar:  /* make a char variable */
	if (datatype & DATA_ARRAY) {
		datatype |= DATA_ARRAYCHAR;
		if (size < 128) arraysiz = size + 3;
		else arraysiz = size + 7;
		goto mkarray;
	}
	if (datatype & DATA_GLOBAL) {
		if (datatype & DATA_GTYPE) {
			/**
			 * Removed for DX16. ANY size declaration after the 1st appearance should be totally ignored!
			 *
			 * if ((size < 128 && (UCHAR)(datatype >> DATA_GTYPESHIFT) >= 0x80) || (size >= 128 && (UCHAR)(datatype >> DATA_GTYPESHIFT) != 0xF0)){
			 * 	return(155);
			 * }
			 */
			datatype = (datatype & ~DATA_GTYPEMASK) | DATA_GLOBALOK | (0xB0 << DATA_GTYPESHIFT);
			if (datatype & DATA_INIT) modread(NULL, size);
			else if (datatype & DATA_INIT2) modread(NULL, dbcbuf[rpos++]);
			goto nextdata1;
		}
		datatype |= DATA_GLOBALOK | (0xB0 << DATA_GTYPESHIFT);
		if (size > 256 && datatab[0].size + size + 8 > datatab[0].maxsize) {
			i2 = datatab[0].size + size + 8;
			if (i2 > 0x00800000) return(103);
			if ((i1 = datatab[0].maxsize) < 0x0400) i1 = 0x0400;
			while (i2 > i1) i1 <<= 1;
			i2 = memchange(datatab[0].dataptr, i1, 0);
			if (i2) goto rmglobal;
			datatab[0].maxsize = i1;

			/* reset data table */
			datatab = *datatabptr;
			p = pstart = *datatab[0].dataptr;
			p += datatab[0].size;
		}
	}
	else if (datatype & DATA_COMMON) {
		if (chnflg != 2 || (hl > 2 && fp <= lp && lp <= pl && pl == size)) {  /* its a valid dim variable */
			if (chnflg != 2) {
				if (size < 128) hl = 3;
				else hl = 7;
			}
			if (datatype & DATA_INIT) goto mkcominit;
			if (datatype & DATA_INIT2) goto mkcominit2;
			size += hl;
			goto mkcom;
		}
	}
	if ((datatype & DATA_INIT) && size) {
		i1 = 1;
		i2 = size;
	}
	else if (datatype & DATA_INIT2) {
		i2 = i3 = dbcbuf[rpos++];
		if (i2 > size) i2 = size;
		if (i2) i1 = 1;
		else i1 = 0;
	}
	else i1 = i2 = 0;
	if (size < 128) {
		p[0] = (UCHAR) i1;
		p[1] = (UCHAR) i2;
		p[2] = (UCHAR) size;
		p += 3;
		if (size == 1 && !(datatype & DATA_INIT2)) {
			if (datatype & DATA_INIT) *p = dbcbuf[rpos++];
			else *p = ' ';
			p++;
			goto nextdata1;
		}
	}
	else {
		p[0] = 0xF0;  /* big dim marker */
		p[1] = (UCHAR) i1;
		p[2] = 0;
		p[3] = (UCHAR) i2;
		p[4] = (UCHAR)(i2 >> 8);
		p[5] = (UCHAR) size;
		p[6] = (UCHAR)(size >> 8);
		p += 7;
		if (!(datatype & DATA_GLOBAL) && (INT)(p - pstart) + size + 10 > dsize) return(ERR_NOMEM);  /* not enough memory */
	}
	if (datatype & DATA_INIT) goto mkdata;
	memset(p, ' ', size);
	if (datatype & DATA_INIT2) {
		memcpy(p, &dbcbuf[rpos], i2);
		rpos += i3;
	}
	p += size;
	goto nextdata1;

mknum:  /* make a num variable */
	if (datatype & DATA_ARRAY) {
		datatype |= DATA_ARRAYNUM;
		arraysiz = size + 1;
		goto mkarray;
	}
	if (datatype & DATA_GLOBAL) {
		if (datatype & DATA_GTYPE) {
			if ((UCHAR)(datatype >> DATA_GTYPESHIFT) < 0x80 || (UCHAR)(datatype >> DATA_GTYPESHIFT) >= 0xA0) return(155);
			datatype = (datatype & ~DATA_GTYPEMASK) | DATA_GLOBALOK | (0xB1 << DATA_GTYPESHIFT);
			if (datatype & DATA_INIT) rpos += size;
			goto nextdata1;
		}
		datatype |= DATA_GLOBALOK | (0xB1 << DATA_GTYPESHIFT);
	}
	else if (datatype & DATA_COMMON) {
		if (chnflg != 2 || (hl == 1 && lp == size)) {
			if (chnflg != 2) hl = 1;
			if (datatype & DATA_INIT) goto mkcominit;
			size += hl;
			goto mkcom;
		}
	}
	*p++ = (UCHAR)(0x80 | size);
	if (datatype & DATA_INIT) goto mkdata;
	if (!sizergt) {
		if (size > 1) {
			memset(p, ' ', size - 1);
			p += size - 1;
		}
		*p++ = '0';
	}
	else {
		i1 = size - sizergt - 1;
		memset(p, ' ', i1);
		p += i1;
		*p++ = '.';
		if (sizergt == 2) {
			*p++ = '0';
			*p++ = '0';
		}
		else if (sizergt == 1) *p++ = '0';
		else {
			memset(p, '0', sizergt);
			p += sizergt;
		}
	}
	goto nextdata1;

mkint:  /* make integer variable */
	if (datatype & DATA_ARRAY) {
		datatype |= DATA_ARRAYINT;
		arraysiz = 6;
		goto mkarray;
	}
	if (datatype & DATA_GLOBAL) {
		if (datatype & DATA_GTYPE) {
			if ((UCHAR)(datatype >> DATA_GTYPESHIFT) != 0xFC) return(155);
			datatype = (datatype & ~DATA_GTYPEMASK) | DATA_GLOBALOK | (0xB1 << DATA_GTYPESHIFT);
			if (datatype & DATA_INIT) rpos += size;
			goto nextdata1;
		}
		datatype |= DATA_GLOBALOK | (0xB1 << DATA_GTYPESHIFT);
	}
	else if (datatype & DATA_COMMON) {
		if (chnflg != 2 || (hl == 2 && fp == -1 && pl == 4)) {
			if (datatype & DATA_INIT) rpos += size;
			size = 6;
			goto mkcom;
		}
	}
	p[0] = 0xFC;
	p[1] = (UCHAR) size;
	if (datatype & DATA_INIT) {
		work[0] = (UCHAR)(0x80 | size);
		memcpy(&work[1], &dbcbuf[rpos], size);
		rpos += size;
		movevar(work, p);
	}
	else itonv(0, p);
	p += 6;
	goto nextdata1;

mkfloat:  /* make float variable */
	if (datatype & DATA_ARRAY) {
		datatype |= DATA_ARRAYFLOAT;
		arraysiz = 10;
		goto mkarray;
	}
	if (datatype & DATA_GLOBAL) {
		if (datatype & DATA_GTYPE) {
			if ((UCHAR)(datatype >> DATA_GTYPESHIFT) < 0xF8 || (UCHAR)(datatype >> DATA_GTYPESHIFT) > 0xFB) return(155);
			datatype = (datatype & ~DATA_GTYPEMASK) | DATA_GLOBALOK | (0xB1 << DATA_GTYPESHIFT);
			if (datatype & DATA_INIT) rpos += size;
			goto nextdata1;
		}
		datatype |= DATA_GLOBALOK | (0xB1 << DATA_GTYPESHIFT);
	}
	else if (datatype & DATA_COMMON) {
		if (chnflg != 2 || (hl == 2 && fp == -1 && pl == 8)) {
			if (datatype & DATA_INIT) rpos += size;
			size = 10;
			goto mkcom;
		}
	}
	i1 = size;
	if (sizergt) i1 -= sizergt + 1;
	p[0] = (UCHAR)(0xF8 | (i1 >> 3));
	p[1] = (UCHAR)((i1 << 5) | sizergt);
	if (datatype & DATA_INIT) {
		work[0] = (UCHAR)(0x80 | size);
		memcpy(&work[1], &dbcbuf[rpos], size);
		rpos += size;
		movevar(work, p);
	}
	else itonv(0, p);
	p += 10;
	goto nextdata1;

mkcominit2:  /* make initialized char common variable */
	/* skip over initialized char variable */
	rpos += dbcbuf[rpos] + 1;
	size += hl;
	goto mkcom;

mkcominit:  /* make initted common variable */
	/* skip over initted variable */
	modread(NULL, size);
	size += hl;

mkcom:  /* make common */
	if (chnflg != 2) memset(p, 0xFD, size);
	p += size;
	goto nextdata1;

mkadv:  /* make an address variable */
	if (version >= 8) c1 = (UCHAR)(0xB0 + type);
	else {
		c2 = (UCHAR) type;
		c1 = 0xCD;
	}
	if (datatype & DATA_ARRAY) {
		datatype |= DATA_ARRAYADR;
		if (version >= 8) arraysiz = 6;
		else arraysiz = 4;
		goto mkarray;
	}
	if (datatype & DATA_GLOBAL) {
		if (datatype & DATA_GTYPE) {
			if ((UCHAR)(datatype >> DATA_GTYPESHIFT) != c1) return(155);
			datatype = (datatype & ~DATA_GTYPEMASK) | DATA_GLOBALOK | (c1 << DATA_GTYPESHIFT);
			goto nextdata1;
		}
		datatype |= DATA_GLOBALOK | (c1 << DATA_GTYPESHIFT);
	}
	else if (datatype & DATA_COMMON) {
		if (chnflg != 2 || (c1 == p[0] && (version >= 8 || c2 == p[1]))) {
			if (version >= 8) size = 6;
			else size = 4;
			goto mkcom;
		}
	}
	p[0] = c1;
	if (version >= 8) {
		memset(&p[1], 0xFF, 5);
		p += 6;
	}
	else {
		p[1] = c2;
		p[2] = p[3] = 0x00;
		p += 4;
	}
	goto nextdata1;

	/*
	 * arraysiz is the total length of each item in the array. e.g. a NUM 7 would be 8
	 * arraycnt is the total number of elements in the array. If it is multidimensional,
	 * then it is the product of the size of each dimension. e.g. a [3,2] would be 6.
	 */
mkarray:
	arrayhdr[arrayhdrsiz++] = (UCHAR) arraysiz;
	arrayhdr[arrayhdrsiz++] = (UCHAR)(arraysiz >> 8);
	if (datatype & DATA_GLOBAL) {
		if (datatype & DATA_GTYPE) {

			/* The next statement captures the error of changing the dimensionality */
			if ((UCHAR)(datatype >> DATA_GTYPESHIFT) != arrayhdr[0]) return(155);

			if (datatype & DATA_ARRAYCHAR) c1 = (UCHAR)(0xC1 + (arrayhdr[0] - 0xA6));
			else if (datatype & (DATA_ARRAYNUM | DATA_ARRAYINT | DATA_ARRAYFLOAT)) c1 = (UCHAR)(0xC5 + (arrayhdr[0] - 0xA6));
			else c1 = 0xB7; /* array of var */
			datatype = (datatype & ~DATA_GTYPEMASK) | DATA_GLOBALOK | (c1 << DATA_GTYPESHIFT);
			/**
			 * If we get here we know that it is a global (DATA_GLOBAL),
			 * it was seen before (DATA_GTYPE), and it is an array (DATA_ARRAY).
			 * We check the size of each dimension, a mismatch is an error
			 */
			ptr = *datatab[0].dataptr;
			ptr += globalptr->offset;
			/* ptr is now pointing to the 1st byte of the existing global variable */
			if (*ptr == 0xA6 || *ptr == 0xA7 || *ptr == 0xA8) {
				if (llhh(ptr+1) != llhh(&arrayhdr[1])) return 155;
			}
			if (*ptr == 0xA7 || *ptr == 0xA8) {
				if (llhh(ptr+3) != llhh(&arrayhdr[3])) return 155;
			}
			if (*ptr == 0xA8) {
				if (llhh(ptr+5) != llhh(&arrayhdr[5])) return 155;
			}

			/**
			 * Now we need to match the type, a mismatch is an error.
			 * We do not care about the size, the second time we see a given global, size is ignored.
			 *
			 * There are only two possible kinds of global arrays;
			 *  numeric, which encompases num, form, int , and float,
			 *  and char, which includes both large and small.
			 */
			ptr += (*ptr - 0xA5) * 2 + 3; /* point ptr at the 1st byte of the first item in the array */
			if (datatype & (DATA_ARRAYNUM | DATA_ARRAYINT | DATA_ARRAYFLOAT)) {
				/* Error if new is numeric and old one isn't. Including a possible null num (0x80) */
				if (*ptr < 0x80 || 0x9F < *ptr) return 155;
			}
			else if (datatype & DATA_ARRAYCHAR) {
				if (*ptr > 0x7F && *ptr != 0xF0) return 155;
			}
			else return 1199; /* Previous type was invalid, internal error! */

			/**
			 * If this time around there is an initial clause (DATA_INIT), we ignore it. We must move rpos past it.
			 * At this point dbcbuf[rpos] is looking at the first byte of the initial list, a length value.
			 */
			if (datatype & DATA_INIT) {
				while (dbcbuf[rpos] != 0xFF) {
					rpos = rpos + dbcbuf[rpos] + 1;
					if (rpos + 280 > rbyte && rbyte == MODBUFSIZE) {  /* get more data */
						i1 = modread(NULL, -1);
						if (i1) return(i1);
					}
				}
				rpos++;		/* Exited loop sitting on the FF, move past it. */
			}
			goto nextdata1;
		}
		if (datatype & DATA_ARRAYCHAR) c1 = (UCHAR)(0xC1 + (arrayhdr[0] - 0xA6));
		else if (datatype & (DATA_ARRAYNUM | DATA_ARRAYINT | DATA_ARRAYFLOAT)) c1 = (UCHAR)(0xC5 + (arrayhdr[0] - 0xA6));
		else c1 = 0xB7;
		datatype |= DATA_GLOBALOK | (c1 << DATA_GTYPESHIFT);
		i1 = arraycnt * arraysiz + 11;
		if (i1 > 256 && datatab[0].size + i1 > datatab[0].maxsize) {
			i2 = datatab[0].size + i1;
			if (i2 > 0x00800000) return(103);
			if ((i1 = datatab[0].maxsize) < 0x0400) i1 = 0x0400;
			while (i2 > i1) i1 <<= 1;
			i2 = memchange(datatab[0].dataptr, i1, 0);
			if (i2) goto rmglobal;
			datatab[0].maxsize = i1;

			/* reset data table */
			datatab = *datatabptr;
			p = pstart = *datatab[0].dataptr;
			p += datatab[0].size;
		}
	}
	else if (datatype & DATA_COMMON) {
		if (chnflg == 2) {
			if (memcmp(p, arrayhdr, arrayhdrsiz)) datatype &= ~DATA_COMMON;
			else {
				scanvar(&p[arrayhdrsiz]);
				if (datatype & DATA_ARRAYCHAR) {
					if (hl < 3 || fp > lp || lp > pl || pl > size) datatype &= ~DATA_COMMON;
				}
				else if (datatype & DATA_ARRAYNUM) {
					if (hl != 1 || fp != 1 || pl > size) datatype &= ~DATA_COMMON;
				}
				else if (datatype & DATA_ARRAYINT) {
					if (hl != 2 || fp != -1 || pl != 4) datatype &= ~DATA_COMMON;
				}
				else if (datatype & DATA_ARRAYFLOAT) {
					if (hl != 2 || fp != -1 || pl != 8) datatype &= ~DATA_COMMON;
				}
			}
		}
	}

	if (datatype & DATA_COMMON) {
		if (chnflg != 2) memset(p, 0xFD, arrayhdrsiz);
	}
	else memcpy(p, arrayhdr, arrayhdrsiz);
	p += arrayhdrsiz;

	while (arraycnt--) {
		if (!(datatype & DATA_GLOBAL) && (INT)(p - pstart) + arraysiz + 10 > dsize) return(ERR_NOMEM);  /* not enough memory */
		if (datatype & DATA_INIT) {
			if (rpos + 280 > rbyte && rbyte == MODBUFSIZE) {  /* get more data */
				i1 = modread(NULL, -1);
				if (i1) return(i1);
			}
			if (datatype & DATA_ARRAYADR) {
				i3 = llmmhh(&dbcbuf[rpos]);
				rpos += 3;
				if (i3 == 0xFFFFFF) datatype &= ~DATA_INIT;
			}
			else {
				i3 = dbcbuf[rpos++];
				if (i3 == 0xFF) datatype &= ~DATA_INIT;
			}
		}

		if (datatype & DATA_COMMON) {
			if (chnflg != 2) memset(p, 0xFD, arraysiz);
		}
		else if (datatype & DATA_ARRAYCHAR) {
			if ((datatype & DATA_INIT) && i3) {
				i1 = 1;
				if (i3 > size) i2 = size;
				else i2 = i3;
			}
			else i1 = i2 = 0;
			if (size < 128) {
				p[0] = (UCHAR) i1;
				p[1] = (UCHAR) i2;
				p[2] = (UCHAR) size;
				i1 = 3;
			}
			else {
				p[0] = 0xF0;  /* big dim marker */
				p[1] = (UCHAR) i1;
				p[2] = (UCHAR)(i1 >> 8);
				p[3] = (UCHAR) i2;
				p[4] = (UCHAR)(i2 >> 8);
				p[5] = (UCHAR) size;
				p[6] = (UCHAR)(size >> 8);
				i1 = 7;
			}
			memset(&p[i1], ' ', size);
		}
		else if (datatype & DATA_ARRAYNUM) {
			p[0] = (UCHAR)(0x80 | size);
			memset(&p[1], ' ', size - 1);
			if (!sizergt) p[size] = '0';
			else {
				i1 = size - sizergt;
				p[i1] = '.';
				memset(&p[i1 + 1], '0', sizergt);
			}
		}
		else if (datatype & DATA_ARRAYINT) {
			p[0] = 0xFC;
			p[1] = (UCHAR) size;
			itonv(0, p);
		}
		else if (datatype & DATA_ARRAYFLOAT) {
			i1 = size;
			if (sizergt) i1 -= (sizergt + 1);
			p[0] = (UCHAR)(0xF8 | (i1 >> 3));
			p[1] = (UCHAR)((i1 << 5) | sizergt);
			itonv(0, p);
		}
		else {  /* address */
			p[0] = c1;
			if (datatype & DATA_INIT) {
				p[1] = (UCHAR) datamod;
				p[2] = (UCHAR)(datamod >> 8);
				p[3] = (UCHAR) i3;
				p[4] = (UCHAR)(i3 >> 8);
				p[5] = (UCHAR)(i3 >> 16);
			}
			else if (version >= 8) memset(&p[1], 0xFF, 5);
			else {
				p[1] = c2;
				p[2] = p[3] = 0x00;
			}
		}
		if ((datatype & DATA_INIT) && !(datatype & DATA_ARRAYADR)) {
			if (datatype & DATA_ARRAYCHAR) memcpy(&p[i1], &dbcbuf[rpos], i2);
			else {
				dbcbuf[rpos - 1] |= 0x80;
				movevar(&dbcbuf[rpos - 1], p);
				dbcbuf[rpos - 1] &= ~0x80;
			}
			rpos += i3;
		}
		p += arraysiz;
	}
	if (datatype & DATA_INIT) {
		for ( ; ; ) {
			if (datatype & DATA_ARRAYADR) {
				i3 = llmmhh(&dbcbuf[rpos]);
				rpos += 3;
				if (i3 == 0xFFFFFF) break;
			}
			else {
				i3 = dbcbuf[rpos++];
				if (i3 == 0xFF) break;
				rpos += i3;
			}
			if (rpos + 280 > rbyte && rbyte == MODBUFSIZE) {  /* get more data */
				i1 = modread(NULL, -1);
				if (i1) return(i1);
			}
		}
	}

	if (datatype & DATA_COMMON) {
		if (chnflg != 2) *p = 0xFD;
	}
	else *p = 0xA5;
	p++;
	goto nextdata1;

mkdavb:
	davb = aligndavb(p, &i1);
	if (datatype & DATA_GLOBAL) {
		if (datatype & DATA_GTYPE) {
			if ((UCHAR)(datatype >> DATA_GTYPESHIFT) != c1) return(155);
			datatype &= ~DATA_GTYPEMASK;
			if (c1 == DAVB_IMAGE) {
				datatype |= DATA_GLOBALOK | (0xB8 << DATA_GTYPESHIFT);
				rpos += 5;
			}
			else if (c1 == DAVB_QUEUE) {
				datatype |= DATA_GLOBALOK | (0xB9 << DATA_GTYPESHIFT);
				rpos += 4;
			}
			else if (c1 == DAVB_RESOURCE) datatype |= DATA_GLOBALOK | (0xBA << DATA_GTYPESHIFT);
			else if (c1 == DAVB_DEVICE) datatype |= DATA_GLOBALOK | (0xBB << DATA_GTYPESHIFT);
			else if (c1 == DAVB_PFILE) datatype |= DATA_GLOBALOK | (0xBC << DATA_GTYPESHIFT);
			else if (c1 == DAVB_OBJECT) datatype |= DATA_GLOBALOK | (0xBD << DATA_GTYPESHIFT);
			goto nextdata1;
		}
		if (c1 == DAVB_IMAGE) datatype |= DATA_GLOBALOK | (0xB8 << DATA_GTYPESHIFT);
		else if (c1 == DAVB_QUEUE) datatype |= DATA_GLOBALOK | (0xB9 << DATA_GTYPESHIFT);
		else if (c1 == DAVB_RESOURCE) datatype |= DATA_GLOBALOK | (0xBA << DATA_GTYPESHIFT);
		else if (c1 == DAVB_DEVICE) datatype |= DATA_GLOBALOK | (0xBB << DATA_GTYPESHIFT);
		else if (c1 == DAVB_PFILE) datatype |= DATA_GLOBALOK | (0xBC << DATA_GTYPESHIFT);
		else if (c1 == DAVB_OBJECT) datatype |= DATA_GLOBALOK | (0xBD << DATA_GTYPESHIFT);
	}
	else if (datatype & DATA_COMMON) {
		if (c1 == DAVB_FILE || c1 == DAVB_IFILE || c1 == DAVB_AFILE || c1 == DAVB_COMFILE || c1 == DAVB_PFILE) {
			if (chnflg != 2) {
				if (c1 == DAVB_FILE || c1 == DAVB_IFILE || c1 == DAVB_AFILE) {
					if (version >= 8) rpos += 9;
					else if (version == 7) rpos += 6;
					else {  /* pre-version 7 */
						if (c1 == DAVB_FILE && version >= 5) rpos++;  /* file */
						else if (c1 == DAVB_IFILE && !(datatype & DATA_KEYVAR)) {  /* ifile */
							if (dbcbuf[rpos++] & 0x10) rpos += 2;
						}
						rpos += 3;
					}
				}
				size = 44;
				goto mkcom;
			}
			i2 = 0;
			if (version >= 6 && davb->type == c1) {
				for ( ; i2 < i1 && p[i2] == c1; i2++);
				if (i2 == i1) for (i2 += sizeof(DAVB); i2 < 44 && p[i2] == 0xF3; i2++);
			}
			if (i2 == 44) {
				if (c1 == DAVB_FILE || c1 == DAVB_IFILE || c1 == DAVB_AFILE) {
					if (davb->info.file.flags & (DAVB_FLAG_VKY | DAVB_FLAG_VRS)) return(104);
					if (davb->refnum) dioflag(davb->refnum);
					if (version >= 8) rpos += 9;
					else if (version == 7) rpos += 6;
					else {  /* pre-version 7 */
						if (c1 == DAVB_FILE && version >= 5) rpos++;  /* file */
						else if (c1 == DAVB_IFILE && !(datatype & DATA_KEYVAR)) {  /* ifile */
							if (dbcbuf[rpos++] & 0x10) rpos += 2;
						}
						rpos += 3;
					}
				}
				else if (davb->refnum) {
					if (c1 == DAVB_COMFILE) comflag(davb->refnum);
					else prtflag(davb->refnum);
				}
				size = 44;
				goto mkcom;
			}
		}
	}
	if (i1) memset(p, c1, i1);
	p += 44;
	memset((UCHAR *) davb, 0, sizeof(DAVB));
	memset((UCHAR *)(davb + 1), 0xF3, p - (UCHAR *)(davb + 1));
	davb->type = c1;
	if (c1 == DAVB_FILE || c1 == DAVB_IFILE || c1 == DAVB_AFILE) {  /* file, ifile, afile */
		davb->info.file.flags = dbcbuf[rpos++];
		if (version >= 8) {
			davb->info.file.flags |= (USHORT) dbcbuf[rpos++] << 8;
			/* shift text bit into type mask */
			if (version < 12 && !(davb->info.file.flags & DAVB_TYPE_MSK)) davb->info.file.flags |= (davb->info.file.flags & DAVB_FLAG_TXT) << 8;
			davb->info.file.bufs = dbcbuf[rpos++];  /* static buffer size */
			davb->info.file.recsz = llmmhh(&dbcbuf[rpos]);  /* dbcbuf size or address */
			rpos += 3;
			davb->info.file.keysz = llmmhh(&dbcbuf[rpos]);  /* key size or address */
			rpos += 3;
		}
		else {
			if (c1 == DAVB_IFILE && (datatype & DATA_KEYVAR)) davb->info.file.flags |= DAVB_FLAG_VKY;
			davb->info.file.recsz = llhh(&dbcbuf[rpos]);
			rpos += 2;
			if (c1 == DAVB_FILE && version >= 5) {  /* file */
				davb->info.file.bufs = dbcbuf[rpos++];
				if (version == 7) rpos++;
			}
			else if (c1 == DAVB_IFILE) {  /* ifile */
				davb->info.file.keysz = dbcbuf[rpos++];
				if (version == 7) {
					if (davb->info.file.flags & DAVB_FLAG_VKY) davb->info.file.keysz += (INT) dbcbuf[rpos] << 8;
					rpos++;
				}
				else if (davb->info.file.flags & DAVB_FLAG_VKY) {
					davb->info.file.keysz = llhh(&dbcbuf[rpos]);
					rpos += 2;
				}
			}
			else if (c1 == DAVB_AFILE && version == 7) rpos += 2;  /* afile */
			if (version == 7) rpos++;
		}
	}
	else if (c1 == DAVB_IMAGE) {  /* image */
		i1 = llhh(&dbcbuf[rpos]);  /* Horizontal pels */
		rpos += 2;
		i2 = llhh(&dbcbuf[rpos]);  /* Verticle pels */
		rpos += 2;
		i3 = dbcbuf[rpos++]; /* Bits per pixel */
		if (datatype & DATA_GLOBAL) i4 = 0;
		else i4 = datamod;
		if ( (i1 = ctlimageopen(i3, i1, i2, i4, (INT)((UCHAR *) davb - pstart), &i3)) ) {
			return(160);
		}
		/* reset data table */
		datatab = *datatabptr;
		/* current data module may have been moved */
		if (datatype & DATA_GLOBAL) {
			p = *datatab[0].dataptr + (INT)(p - pstart);
			davb = (DAVB *)(*datatab[0].dataptr + (INT)((UCHAR *) davb - pstart));
			pstart = *datatab[0].dataptr;
		}
		else {
			p = **dataptr + (INT)(p - pstart);
			davb = (DAVB *)(**dataptr + (INT)((UCHAR *) davb - pstart));
			pstart = **dataptr;
		}
		davb->refnum = i3;
	}
	else if (c1 == DAVB_QUEUE) { /* queue */
		i1 = llhh(&dbcbuf[rpos]);  /* entries */
		rpos += 2;
		i2 = llhh(&dbcbuf[rpos]);  /* size */
		rpos += 2;
		if (datatype & DATA_GLOBAL) i4 = 0;
		else i4 = datamod;
		if (ctlqueueopen(i1, i2, i4, (INT)((UCHAR *) davb - pstart), &i3)) return(ERR_NOMEM);

		/* reset data table */
		datatab = *datatabptr;
		/* current data module may have been moved */
		if (datatype & DATA_GLOBAL) {
			p = *datatab[0].dataptr + (INT)(p - pstart);
			davb = (DAVB *)(*datatab[0].dataptr + (INT)((UCHAR *) davb - pstart));
			pstart = *datatab[0].dataptr;
		}
		else {
			p = **dataptr + (INT)(p - pstart);
			davb = (DAVB *)(**dataptr + (INT)((UCHAR *) davb - pstart));
			pstart = **dataptr;
		}
		davb->refnum = i3;
	}
	goto nextdata1;

mkglobal:
	dataoff = (INT) (p - pstart);
	i3 = (INT) strlen((CHAR *) &dbcbuf[rpos]) + 1;
	memcpy(gname, &dbcbuf[rpos], i3);
	/* Move rpos past the name and the terminating null */
	rpos += i3;

	i2 = 1;
	globalpptr1 = globaltabptr;
	globalptr = *globalpptr1;
	do {
		globalpptr = globalpptr1;
		globalpptr1 = globalptr->nextptr;
		if (globalpptr1 == NULL) break;
		globalptr = *globalpptr1;
		i2 = strcmp((CHAR *) gname, globalptr->name);
	} while (i2 > 0); /* break if i2 is zero or negative. that is, gname <= globalptr->name */

	if (!i2) {  /* global variable exists */
		p = *datatab[0].dataptr;
		datatype |= DATA_GTYPE | (p[globalptr->offset] << DATA_GTYPESHIFT);
	}
	else {
		/* reserve space for the global variable */
		if (datatab[0].size + 280 > datatab[0].maxsize) {
			i2 = datatab[0].size + 280;
			if (i2 > 0x00800000) { /* 8,388,608 */
				return(103);
			}
			if ((i1 = datatab[0].maxsize) < 0x0400) i1 = 0x0400;
			while (i2 > i1) i1 <<= 1;
			i2 = memchange(datatab[0].dataptr, i1, 0);
			if (i2) return(ERR_NOMEM);

			/* reset data table */
			datatab = *datatabptr;
			datatab[0].maxsize = i1;
		}

		globalpptr2 = (GLOBALINFO **) memalloc(sizeof(GLOBALINFO) - (sizeof(globalptr->name) - i3) * sizeof(UCHAR), 0);
		if (globalpptr2 == NULL) return(ERR_NOMEM);

		/* reset data table */
		datatab = *datatabptr;

		globalptr = *globalpptr;
		globalptr->nextptr = globalpptr2;
		/* Now for convenience, point globalptr at our newly created block */
		globalptr = *globalpptr2;
		globalptr->nextptr = globalpptr1;
		globalptr->offset = datatab[0].size;
		strcpy(globalptr->name, (CHAR *) gname);
	}
	globaloff = globalptr->offset;

	p = pstart = *datatab[0].dataptr;
	p += datatab[0].size;
	datatype |= DATA_GLOBAL;
	goto nextdata2;

rmglobal:
	globalpptr1 = globaltabptr;
	globalptr = *globalpptr1;
	do {
		globalpptr = globalpptr1;
		globalpptr1 = globalptr->nextptr;
		if (globalpptr1 == NULL) break;
		globalptr = *globalpptr1;
		i2 = !strcmp((CHAR *) gname, globalptr->name);
		if (!i2) {
			(*globalpptr)->nextptr = globalptr->nextptr;
			memfree((UCHAR **) globalpptr1);
		}
	} while (i2 > 0);
	return(ERR_NOMEM);
}



/**
 * Read bytes from module, uses global variables: handle, rpos, rbyte, fpos & dbcbuf.
 * rpos, rbyte, fpos are modified.
 * if len < 0, fill variable 'dbcbuf'
 * if pptr = NULL, set global variables to skip 'len' bytes
 * if pptr != NULL, fill buf with 'len' bytes
 * returns negative value if error
 */
static INT modread(UCHAR *buf, INT len)
{
	INT i1, i2;

	if (!len) return(0);

	if (len > 0) {  /* skip or read len bytes */
		i1 = rbyte - rpos;  /* number of bytes left in buffer */
		if (len < i1) i1 = len;
		if (buf != NULL) {
			memcpy(buf, &dbcbuf[rpos], i1);
			buf += i1;
		}
		rpos += i1;
		len -= i1;
		if (len) {
			if (rbyte != MODBUFSIZE) {
				return(102);
			}
			if (buf == NULL || len >= MODBUFSIZE) {
				if (buf != NULL) {
					if (srvhandle) {
						i2 = fsreadraw(handle, fpos, (CHAR *) buf, len);
						if (i2 == -1) {
							i2 = (INT) strlen(name);
							fsgeterror(name + i2 + 2, sizeof(name) - i2 - 2);
							if (name[i2 + 2]) {
								name[i2] = ',';
								name[i2 + 1] = ' ';
							}
							return 162;
						}
					}
					else i2 = fioread(handle, fpos, buf, len);
					if (i2 < 0) return(i2);
					if (i2 != len) {
						return(102);
					}
				}
				fpos += len;
				len = 0;
			}
		}
		else if (rpos != rbyte) {
			return(0);
		}
	}

	/* fill buffer */
	if (rbyte -= rpos) {  /* number of bytes left in buffer */
		if (rbyte <= rpos) memcpy(dbcbuf, &dbcbuf[rpos], rbyte);
		else memmove(dbcbuf, &dbcbuf[rpos], rbyte);
	}
	if (srvhandle) {
		i1 = fsreadraw(handle, fpos, (CHAR *) dbcbuf + rbyte, rpos);
		if (i1 == -1) {
			i1 = (INT) strlen(name);
			fsgeterror(name + i1 + 2, sizeof(name) - i1 - 2);
			if (name[i1 + 2]) {
				name[i1] = ',';
				name[i1 + 1] = ' ';
			}
			return 162;
		}
	}
	else i1 = fioread(handle, fpos, dbcbuf + rbyte, rpos);
	if (i1 < 0) return(i1);
	fpos += i1;
	rbyte += i1;
	if (rbyte < MODBUFSIZE) memset(&dbcbuf[rbyte], 0, MODBUFSIZE - rbyte);
	rpos = 0;

	if (len > 0) {  /* finish reading into buffer */
		if (len > rbyte) {
			return(102);
		}
		memcpy(buf, dbcbuf, len);
		rpos += len;
	}
	return(0);
}

/* unload one or all modules, singleflg = single module flag */
INT vunload(INT singleflg, INT pcnt)
{
	INT i1, i2, i3, i4, allflg, retcnt, savedataxmod, savepgmxmod;
	CHAR dataname[50];

	allflg = TRUE;
	if (singleflg) {
		/* get instance name */
		dataname[0] = '\0';
		for (i1 = 0; name[i1] && name[i1] != '<'; i1++);
		if (name[i1]) {
			name[i1++] = '\0';
			for (i2 = 0; i2 < 8 && name[i1] && name[i1] != '>'; i1++)
				if (!isspace(name[i1])) dataname[i2++] = (CHAR) toupper(name[i1]);
			dataname[i2] = '\0';
			allflg = FALSE;
		}

		/* strip out the module name */
		miofixname(name, ".dbc", FIXNAME_EXT_ADD | FIXNAME_PAR_DBCVOL);
		for (i1 = (INT) (strlen(name) - 1); name[i1] != '.' && name[i1] != ')'; i1--);
		name[i1--] = '\0';
		while (i1 >= 0 && name[i1] != '\\' && name[i1] != ':' && name[i1] != '/' && name[i1] != ']' && name[i1] != '(') {
#if OS_WIN32
			name[i1] = (CHAR) toupper(name[i1]);
#endif
			i1--;
		}
		if (i1 >= 0) memmove(name, &name[i1 + 1], strlen(&name[i1]));
	}

	/* first call destructors */
	savedataxmod = dataxmodule;
	savepgmxmod = pgmxmodule;
	retcnt = returnstackcount();

/*** CODE: ASK DON ABOUT PUSHING SOME OR ALL DESTRUCTORS ON STACK, CURRENTLY ALL ***/
/***       DON SAYS TO PUSH ONLY ONE MODULE AND RETURN ***/
/***       ALSO SHOULD USE LINKED LIST FOR MODULES ***/
	for (i1 = pgmhi; i1--; ) {
		if (pgmtab[i1].typeflg != 3 || (singleflg && strcmp(name, *(*pgmtab[i1].codeptr)->nameptr))) continue;
		for (i2 = pgmtab[i1].dataxmod; i2 != -1; i2 = datatab[i2].nextmod) {
			if (i2 != dataxmodule && (allflg || !strcmp(dataname, *datatab[i2].nameptr))) {
				/* call loaded classes' destructors */
				for (i3 = datatab[i2].classmod; i3 != -1; i3 = datatab[i3].nextclass) {
					i4 = destroy(i3, &pcnt);
					if (i4) {
						i1 = returnstackcount();
						while (i1-- > retcnt) popreturnstack();
						dataxmodule = savedataxmod;
						datamodule = datatab[dataxmodule].datamod;
						pgmxmodule = savepgmxmod;
						pgmmodule = pgmtab[pgmxmodule].pgmmod;
						setpgmdata();
						return(i4);
					}
				}
				if (!allflg) goto vunload1;
			}
		}
		if (singleflg) break;
	}

vunload1:
	if (returnstackcount() > retcnt) {
		setpgmdata();
		return(0);  /* call destructors first */
	}

	/* now unload modules */
	for (i1 = pgmhi; i1--; ) {
		if (pgmtab[i1].typeflg != MTYPE_LOADMOD || (singleflg && strcmp(name, *(*pgmtab[i1].codeptr)->nameptr))) continue;
		i3 = -1;
		i2 = pgmtab[i1].dataxmod;
		while (i2 != -1) {
			i4 = datatab[i2].nextmod;
			if (i2 != dataxmodule && (allflg || !strcmp(dataname, *datatab[i2].nameptr))) {
				if (i3 == -1 && i4 == -1) unloadmod(i1, TRUE);
				else {
					unloadmod(i2, FALSE);
					if (i3 == -1) pgmtab[i1].dataxmod = i4;
					else datatab[i3].nextmod = i4;
				}
				if (!allflg) goto vunload2;
			}
			else i3 = i2;
			i2 = i4;
		}
		if (singleflg) break;
	}

vunload2:
	lastdataxmod = dataxmodule;
	lastdatamod = datatab[dataxmodule].datamod;
	return(0);
}

INT unloadclass(INT mod, INT pcnt)
{
	INT i1, i2, datamod, nextclass, prevclass, retcnt, savedataxmod, savepgmxmod;

	if ((mod = getpgmx(mod)) == RC_ERROR) return(0);  /* may happen if make fails */

/*** CODE: SHOULD CODE BE ADDED TO PREVENT UNLOAD OF CLASS THAT EQUALS PGMMODULE ***/

	datamod = pgmtab[mod].dataxmod;
	if (pcnt != -1) {  /* first call destructors */
		savedataxmod = dataxmodule;
		savepgmxmod = pgmxmodule;
		retcnt = returnstackcount();

		i2 = destroy(datamod, &pcnt);
		if (i2) {
			i1 = returnstackcount();
			while (i1-- > retcnt) popreturnstack();
			dataxmodule = savedataxmod;
			datamodule = datatab[dataxmodule].datamod;
			pgmxmodule = savepgmxmod;
			pgmmodule = pgmtab[pgmxmodule].pgmmod;
			setpgmdata();
			return(i2);
		}
		if (returnstackcount() > retcnt) {
			setpgmdata();
			return(0);  /* let destructors execute */
		}
	}

	/* now unload class */
	prevclass = datatab[datamod].prevclass;
	nextclass = datatab[datamod].nextclass;
	unloadmod(mod, TRUE);
	if (datatab[prevclass].classmod == datamod) datatab[prevclass].classmod = nextclass;
	else datatab[prevclass].nextclass = nextclass;
	if (nextclass != -1) datatab[nextclass].prevclass = prevclass;
	return(0);
}

static void unloadmod(INT mod, INT pgmflg)
{
	INT i1, i2, dataflg;
	CODEINFO *code;

	dataflg = FALSE;
	do {
		if (pgmflg) {
/*** CODE: NOW THAT THIS HAS BEEN ADDED FOR CHAIN, MAYBE SUPPORT TRAPS ***/
/***       IN PRELOADS - NO IMPLICIT TRAPCLR ALL (CHANGE DOCUMENTATION) ***/
			if (pgmtab[mod].typeflg != 4) pgmflg = FALSE;
			trapclearmod(pgmtab[mod].pgmmod);
			code = *pgmtab[mod].codeptr;

			if (!--code->codecnt) {
				memfree(code->pgmptr);
				memfree(code->xrefptr);
				memfree(code->xdefptr);
				memfree((UCHAR **) code->fileptr);
				memfree((UCHAR **) code->nameptr);
				memfree((UCHAR **) pgmtab[mod].codeptr);
			}
			memfree(pgmtab[mod].xrefptr);
			memfree(pgmtab[mod].xdefptr);
			i1 = pgmtab[mod].dataxmod;
			memset((CHAR *) &pgmtab[mod], 0, sizeof(PGMMOD));
			pgmtab[mod].pgmmod = -1;
			if (mod + 1 == pgmhi) pgmhi = mod;
			mod = i1;
			dataflg = TRUE;
		}

		/* recursively unload dependent classes */
		for (i1 = datatab[mod].classmod; i1 != -1; i1 = i2) {
			i2 = datatab[i1].nextclass;
			unloadmod(datatab[i1].pgmxmod, TRUE);
		}

		i1 = datatab[mod].datamod;
		ctlclosemod(i1);  /* close all devices */
		memfree(datatab[mod].varptr);
		memfree(datatab[mod].lextptr);
		memfree(datatab[mod].lmodptr);
		memfree(datatab[mod].ltabptr);
		if (mod != dataxchn) {
			dioclosemod(i1);  /* close files */
			prtclosemod(i1);  /* close pfiles */
			comclosemod(i1);  /* close comfiles */
			memfree(datatab[mod].dataptr);
		}
		memfree((UCHAR **) datatab[mod].nameptr);
		i1 = datatab[mod].nextmod;
		memset((CHAR *) &datatab[mod], 0, sizeof(DATAMOD));
		datatab[mod].datamod = -1;
		if (mod + 1 == datahi) datahi = mod;
		if (i1 == -1) dataflg = FALSE;
		else if (pgmflg) mod = datatab[i1].pgmxmod;
		else mod = i1;
	} while (dataflg);
}

static INT destroy(INT parentmod, INT *pcnt)
{
	INT i1, i2, childmod, label, mod;

	/* reverse the parent list so parent's destructors are pushed first */
	childmod = -1;
	while (parentmod != -1) {
		i1 = parentmod;
		parentmod = datatab[i1].nextmod;
		datatab[i1].nextmod = childmod;
		childmod = i1;
	}

	/* push destructor labels, starting with parent's */
	while (childmod != -1) {
		/* push destructor */
		if (datatab[childmod].destroylabel != 0xFFFF) {
			label = datatab[childmod].destroylabel;
			datatab[childmod].destroylabel = 0xFFFF;
			pcount = *pcnt;
			if (pushreturnstack(-3)) {
				i1 = 501;
				goto destroy1;
			}

			i1 = datatab[childmod].pgmxmod;
			i2 = ((INT *) *datatab[childmod].lmodptr)[label];
			if (i2 != -1) i2 = ((INT *) *datatab[childmod].ltabptr)[label];
			else if (getpcntx(label, childmod, &i1, &i2, TRUE) == -1) {
				i1 = 562;
				goto destroy1;
			}
			pgmxmodule = i1;
			pgmmodule = pgmtab[pgmxmodule].pgmmod;
			dataxmodule = pgmtab[pgmxmodule].dataxmod;
			datamodule = datatab[dataxmodule].datamod;
			*pcnt = pcount = i2;
		}

		/* recurse for dependent class destructors */
		for (mod = datatab[childmod].classmod; mod != -1; mod = datatab[mod].nextclass) {
			i1 = destroy(mod, pcnt);
			if (i1) goto destroy1;
		}

		/* reverse the parent list back to normal */
		i1 = childmod;
		childmod = datatab[i1].nextmod;
		datatab[i1].nextmod = parentmod;
		parentmod = i1;
	}
	return(0);

destroy1:
	/* variable i1 contains the error */
	/* reverse the parent list back to normal */
	while (childmod != -1) {
		i2 = childmod;
		childmod = datatab[i2].nextmod;
		datatab[i2].nextmod = parentmod;
		parentmod = i2;
	}
	return(i1);
}

void setpgmdata()
{
	datatab = *datatabptr;
	pgmtab = *pgmtabptr;
	if (datatab[dataxmodule].dataptr != NULL) data = *datatab[dataxmodule].dataptr;
	if (pgmtab[pgmxmodule].codeptr != NULL) pgm = *(*pgmtab[pgmxmodule].codeptr)->pgmptr;
}

static INT getdatax(INT mod)
{
	INT xmod;

	if (mod == lastdatamod) xmod = lastdataxmod;
	else {
		if (mod == datamodule) xmod = dataxmodule;
		else if (mod >= 0 && mod < 0xFFFF) {
			for (xmod = 0; xmod < datahi && mod != datatab[xmod].datamod; xmod++);
			if (xmod == datahi) return RC_ERROR;
		}
		else return RC_ERROR;
		lastdatamod = mod;
		lastdataxmod = xmod;
	}
	return(xmod);
}

static INT getpgmx(INT mod)
{
	static INT lastpgmmod = -1;
	static INT lastpgmxmod = -1;
	INT xmod;

	if (mod == lastpgmmod && mod == pgmtab[lastpgmxmod].pgmmod) {
		xmod = lastpgmxmod;
	}
	else {
		if (mod == pgmmodule) xmod = pgmxmodule;
		else if (mod >= 0 && mod <= INT_MAX) {
			for (xmod = 0; xmod < pgmhi && mod != pgmtab[xmod].pgmmod; xmod++);
			if (xmod == pgmhi) return RC_ERROR;
		}
		else return RC_ERROR;
		lastpgmmod = mod;
		lastpgmxmod = xmod;
	}
	return(xmod);
}

void vreturn()
{
	INT i1, savepgmmod, savepcount;

	savepgmmod = pgmmodule;
	savepcount = pcount;
	if (popreturnstack()) dbcerror(503);
	else if (savepgmmod != pgmmodule) {
		/* can not use getpgmx() because pgmmodule & pgmxmodule do not match */
		for (i1 = 0; i1 < pgmhi && pgmmodule != pgmtab[i1].pgmmod; i1++);
		if (i1 == pgmhi) {
			pgmmodule = savepgmmod;
			pcount = savepcount;
			dbcerrinfo(558, (UCHAR*)"On a return", -1);
		}
		pgmxmodule = i1;
		pgm = *(*pgmtab[pgmxmodule].codeptr)->pgmptr;
		dataxmodule = pgmtab[pgmxmodule].dataxmod;
		datamodule = datatab[dataxmodule].datamod;
		data = *datatab[dataxmodule].dataptr;
	}
}

void vretcnt()
{
	INT i1;

	i1 = returnstackcount();
	if (!i1) dbcflags |= DBCFLAG_EQUAL;
	else dbcflags &= ~DBCFLAG_EQUAL;
	itonv(i1, getvar(VAR_WRITE));
}

UCHAR *getmdata(INT mod)
{
	if (mod == datamodule) mod = dataxmodule;
	else if ((mod = getdatax(mod)) == RC_ERROR) return(NULL);

	return(*datatab[mod].dataptr);
}

UCHAR *getmpgm(INT mod)
{
	if (mod == pgmmodule) mod = pgmxmodule;
	else if ((mod = getpgmx(mod)) == RC_ERROR) return(NULL);

	return(*(*pgmtab[mod].codeptr)->pgmptr);
}

INT getpgmsize(INT mod)
{
	if (mod == pgmmodule) mod = pgmxmodule;
	else if ((mod = getpgmx(mod)) == RC_ERROR) return RC_ERROR;

	return((*pgmtab[mod].codeptr)->pgmsize);
}

INT getdatamod(INT mod)
{
	if (mod == pgmmodule) mod = pgmxmodule;
	else if ((mod = getpgmx(mod)) == RC_ERROR) return RC_ERROR;

	return(datatab[pgmtab[mod].dataxmod].datamod);
}

INT getpgmmod(INT mod)
{
	if (mod == datamodule) mod = dataxmodule;
	else if ((mod = getdatax(mod)) == RC_ERROR) return RC_ERROR;

	return(pgmtab[datatab[mod].pgmxmod].pgmmod);
}

INT getmver(INT mod)
{
	if (mod == pgmmodule) mod = pgmxmodule;
	else if ((mod = getpgmx(mod)) == RC_ERROR) return(0);

	return((INT)(*pgmtab[mod].codeptr)->version);
}

void getmtime(INT mod, INT *htime, INT *ltime)
{
	if (mod == pgmmodule) mod = pgmxmodule;
	else if ((mod = getpgmx(mod)) == RC_ERROR) {
		*htime = *ltime = 0;
		return;
	}
	*htime = (*pgmtab[mod].codeptr)->htime;
	*ltime = (*pgmtab[mod].codeptr)->ltime;
}

INT getmtype(INT mod)
{
	if (mod == pgmmodule) mod = pgmxmodule;
	else if ((mod = getpgmx(mod)) == RC_ERROR) return RC_ERROR;
	return((INT) pgmtab[mod].typeflg);
}

CHAR *getdname(INT mod)
{
	if (mod == datamodule) mod = dataxmodule;
	else if ((mod = getdatax(mod)) == RC_ERROR) return("");

	if (datatab[mod].nameptr == NULL) return("");
	return(*datatab[mod].nameptr);
}

CHAR *getpname(INT mod)
{
	if (mod == pgmmodule) mod = pgmxmodule;
	else if ((mod = getpgmx(mod)) == RC_ERROR) return("");

	return(*(*pgmtab[mod].codeptr)->nameptr);
}

CHAR *getmname(INT pmod, INT dmod, INT classnumflag)
{
	static CHAR mname[MAX_NAMESIZE];
	INT i1;

	setpgmdata();
	if (pmod == pgmmodule) pmod = pgmxmodule;
	else pmod = getpgmx(pmod);
	if (pmod != RC_ERROR) strcpy(mname, *(*pgmtab[pmod].codeptr)->nameptr);
	else mname[0] = '\0';
	i1 = (INT) strlen(mname);

	if (pmod != -1) {
		if (pgmtab[pmod].typeflg == 1 || pgmtab[pmod].typeflg == 3) mname[i1++] = '<';
		else if (pgmtab[pmod].typeflg == 4) mname[i1++] = '!';
	}
	if (dmod == datamodule) dmod = dataxmodule;
	else dmod = getdatax(dmod);
	if (dmod != RC_ERROR && datatab[dmod].nameptr != NULL) {
		strcpy(&mname[i1], *datatab[dmod].nameptr);
		i1 += (INT) strlen(*datatab[dmod].nameptr);
	}
	if (pmod != -1) {
		if (pgmtab[pmod].typeflg == 1 || pgmtab[pmod].typeflg == 3) mname[i1++] = '>';
		else if (classnumflag && pgmtab[pmod].typeflg == 4 && dmod != -1) {
			mname[i1++] = '(';
			mscitoa(datatab[dmod].datamod, &mname[i1]);
			i1 += (INT) strlen(&mname[i1]);
			mname[i1++] = ')';
		}
	}
	mname[i1] = '\0';
	return(mname);
}

void findmname(CHAR *mname, INT *pmod, INT *dmod)
{
	INT i1, i2, i3, pgmmod, datamod;
	CHAR typeflg, dname[64], dname2[64], pname[128];

	/* parse program name */
	for (i1 = i2 = 0; mname[i1] && mname[i1] != '<' && mname[i1] != '!'; i1++) {
		if (mname[i1] == ' ') continue;
#if OS_WIN32
		pname[i2++] = (CHAR) toupper(mname[i1]);
#else
		pname[i2++] = mname[i1];
#endif
	}
	pname[i2] = '\0';
	if (!i2) {
		if (*pmod == pgmmodule) pgmmod = pgmxmodule;
		else pgmmod = getpgmx(*pmod);
		if (pgmmod != RC_ERROR) strcpy(pname, *(*pgmtab[pgmmod].codeptr)->nameptr);
	}
	else pgmmod = -1;

	/* parse data name */
	datamod = -1;
	i2 = 0;
	if ( (typeflg = mname[i1]) ) {
		for (i1++; mname[i1] && mname[i1] != '>' && mname[i1] != '('; i1++) {
			if (mname[i1] == ' ') continue;
			dname[i2] = mname[i1];
			dname2[i2++] = (CHAR) toupper(mname[i1]);
		}
		if (typeflg == '!' && mname[i1] == '(') {
			for (i3 = 0, i1++; isdigit(mname[i1]); i1++) i3 = i3 * 10 + mname[i1] - '0';
			if (i3) {
				if (i3 == datamodule) datamod = dataxmodule;
				else datamod = getdatax(i3);
			}
		}
	}
	dname[i2] = dname2[i2] = '\0';

	/* find program and data names */
	*pmod = *dmod = -1;
	if (pgmmod != -1) {
		i1 = pgmmod;
		i2 = pgmmod + 1;
	}
	else {
		i1 = 0;
		i2 = pgmhi;
	}
	for ( ; i1 < i2; i1++) {
		if (pgmtab[i1].pgmmod == -1) continue;
		if (!strcmp(pname, *(*pgmtab[i1].codeptr)->nameptr)) {
			if (typeflg == '<' && pgmtab[i1].typeflg != MTYPE_PRELOAD && pgmtab[i1].typeflg != MTYPE_LOADMOD) continue;
			else if (typeflg == '!' && pgmtab[i1].typeflg != MTYPE_CLASS) continue;
			*pmod = pgmtab[i1].pgmmod;
			i3 = pgmtab[i1].dataxmod;
			if (typeflg) {
				if (typeflg == '<') {
					while (i3 != -1) {
						if (datatab[i3].nameptr != NULL && !strcmp(dname2, *datatab[i3].nameptr)) break;
						i3 = datatab[i3].nextmod;
					}
					if (i3 == -1) continue;
				}
				else if (datatab[i3].nameptr == NULL || strcmp(dname, *datatab[i3].nameptr)
						|| (datamod != -1 && i3 != datamod)) continue;
			}
			else {
				if (pgmtab[i1].typeflg == MTYPE_PRELOAD || pgmtab[i1].typeflg == MTYPE_LOADMOD) {
					typeflg = '<';
					if (datatab[i3].nameptr != NULL) strcpy(dname2, *datatab[i3].nameptr);
				}
				else if (pgmtab[i1].typeflg == MTYPE_CLASS) {
					typeflg = '!';
					if (datatab[i3].nameptr != NULL) strcpy(dname, *datatab[i3].nameptr);
				}
			}
			*dmod = datatab[i3].datamod;
			break;
		}
	}

	/* build module name */
	strcpy(mname, pname);
	if (typeflg) {
		i1 = (INT) strlen(mname);
		mname[i1++] = typeflg;
		if (typeflg == '<') {
			strcpy(&mname[i1], dname2);
			i1 += (INT) strlen(dname2);
			mname[i1++] = '>';
			mname[i1] = '\0';
		}
		else {
			strcpy(&mname[i1], dname);
			i1 += (INT) strlen(dname);
			if (*dmod != -1) {
				mname[i1++] = '(';
				mscitoa(*dmod, &mname[i1]);
				i1 += (INT) strlen(&mname[i1]);
				mname[i1++] = ')';
				mname[i1] = '\0';
			}
		}
	}
}

/**
 * Set pcount and module from label table entry number
 *
 * label is index into DATAMOD.lmod and DATAMOD.ltab
 */
void chgpcnt(INT label)
{
	INT mod, off;
	datatab = *datatabptr;		// 06-16-2014 - just a wild guess
	pgmtab = *pgmtabptr;		// 06-23-2014 - just another wild guess
	mod = ((INT *) *datatab[dataxmodule].lmodptr)[label];
	off = ((INT *) *datatab[dataxmodule].ltabptr)[label];
	if (mod != pgmmodule) {  /* not current module */
		if (mod == -1 || (mod = getpgmx(mod)) == RC_ERROR) {  /* unresolved label */
			if (getpcntx(label, dataxmodule, &mod, &off, FALSE) == -1) dbcerror(551);
		}
		pgmxmodule = mod;
		pgmmodule = pgmtab[pgmxmodule].pgmmod;
		pgm = *(*pgmtab[pgmxmodule].codeptr)->pgmptr;
		dataxmodule = pgmtab[pgmxmodule].dataxmod;
		datamodule = datatab[dataxmodule].datamod;
		data = *datatab[dataxmodule].dataptr;
	}
	pcount = off;
}

/*
 * Set pcount and module from label table entry number and method module
 */
void chgmethod(INT label, INT methodmod)
{
	INT mod, off, xmod;

	mod = ((INT *) *datatab[dataxmodule].lmodptr)[label];
	off = ((INT *) *datatab[dataxmodule].ltabptr)[label];
	if (mod != methodmod || mod != pgmmodule) {
		if ((xmod = getpgmx(methodmod)) == RC_ERROR) dbcerror(505);
		if (mod != methodmod && getpcntx(label, dataxmodule, &xmod, &off, TRUE) == -1) dbcerror(551);
		pgmxmodule = xmod;
		pgmmodule = pgmtab[pgmxmodule].pgmmod;
		pgm = *(*pgmtab[pgmxmodule].codeptr)->pgmptr;
		dataxmodule = pgmtab[pgmxmodule].dataxmod;
		datamodule = datatab[dataxmodule].datamod;
		data = *datatab[dataxmodule].dataptr;
	}
	pcount = off;
}

/* get module number and offset pcount for label */
/* if an error occurs return -1; otherwise return 0 */
INT getpcnt(INT label, INT *modptr, INT *offptr)
{
	INT mod;
	INT *lmod;

	lmod = (INT *) *datatab[dataxmodule].lmodptr;
	mod = lmod[label];
	if (mod != pgmmodule && (mod == -1 || getpgmx(mod) == RC_ERROR)) {  /* unresolved label */
		if (getpcntx(label, dataxmodule, &mod, offptr, FALSE) == -1) return RC_ERROR;
		mod = (INT) lmod[label];
	}
	else *offptr = ((INT *) *datatab[dataxmodule].ltabptr)[label];
	*modptr = mod;
	return(0);
}

/* put module number and offset pcount for label */
void putpcnt(INT label, INT mod, INT off)
{
	/* assume compiler does not give label that is an external/method label */
	((INT *) *datatab[dataxmodule].lmodptr)[label] = mod;
	((INT *) *datatab[dataxmodule].ltabptr)[label] = off;
}

/* set pcount and module from offset pcount and module number */
void setpcnt(INT mod, INT off)
{
	if (mod != pgmmodule) {
		if ((mod = getpgmx(mod)) == RC_ERROR) dbcerror(558);  /* reference is unresolved */
		pgmxmodule = mod;
		pgmmodule = pgmtab[pgmxmodule].pgmmod;
		pgm = *(*pgmtab[pgmxmodule].codeptr)->pgmptr;
		dataxmodule = pgmtab[pgmxmodule].dataxmod;
		datamodule = datatab[dataxmodule].datamod;
		data = *datatab[dataxmodule].dataptr;
	}
	pcount = off;
}

/**
 *  move label entry 'srclab' in data module 'srcmod'
 *  to label entry 'destlab' in data module 'datamodule'
 */
void movelabel(INT srcmod, INT srclab, INT destlab)
{
	INT dataxmod, mod, off;
	INT *lmod;

	if (srcmod == datamodule) dataxmod = dataxmodule;
	else if ((dataxmod = getdatax(srcmod)) == RC_ERROR) dbcerror(552);

	lmod = (INT *) *datatab[dataxmod].lmodptr;
	mod = lmod[srclab];
	if (mod != pgmmodule && (mod == -1 || getpgmx(mod) == RC_ERROR)) {  /* unresolved label */
		if (getpcntx(srclab, dataxmod, &mod, &off, FALSE) == -1) dbcerror(551);
		mod = lmod[srclab];
	}
	else off = ((INT *) *datatab[dataxmod].ltabptr)[srclab];

	((INT *) *datatab[dataxmodule].lmodptr)[destlab] = mod;
	((INT *) *datatab[dataxmodule].ltabptr)[destlab] = off;
}

INT vgetlabel(INT label)
{
	INT mod, off, *ltab;
	INT *lmod;

	lmod = (INT *) *datatab[dataxmodule].lmodptr;
	ltab = (INT *) *datatab[dataxmodule].ltabptr;
	if (getxlabel(name, &mod, &off, FALSE) == -1) {
		if (lmod[label] != -1) {
			lmod[label] = -1;
			ltab[label] = 0x00FFFFFF;
		}
		return(1);
	}
	lmod[label] = pgmtab[mod].pgmmod;
	ltab[label] = off;
	return(0);
}

INT vtestlabel(INT label)
{
	INT mod, off;
	INT *lmod;

	lmod = (INT *) *datatab[dataxmodule].lmodptr;
	mod = lmod[label];
	if (mod != pgmmodule && (mod == -1 || getpgmx(mod) == RC_ERROR)) {  /* unresolved label */
		if (getpcntx(label, dataxmodule, &mod, &off, FALSE) == -1) return(1);
	}
	return(0);
}

void clearlabel(INT label)
{
	INT *lmod;

	lmod = (INT *) *datatab[dataxmodule].lmodptr;
	if (lmod[label] != -1) {
		lmod[label] = -1;
		((INT *) *datatab[dataxmodule].ltabptr)[label] = 0x00FFFFFF;
	}
}

/**
 * Resolve an external reference or method
 *
 * returns zero for success or RC_ERROR
 */
static INT getpcntx(INT label, INT dataxmod, INT *pgmxmodptr, INT *offptr, INT method)
{
	INT i1, i2, i3, pgmxmod, *ltab;
	INT *lmod;
	USHORT *lext;
	UCHAR *ptr;

	lmod = (INT *) *datatab[dataxmod].lmodptr;
	ltab = (INT *) *datatab[dataxmod].ltabptr;
	if (lmod[label] != -1) {
		lmod[label] = -1;
		ltab[label] = 0x00FFFFFF;
	}

	if (datatab[dataxmod].lextptr == NULL) return RC_ERROR;
	lext = (USHORT *) *datatab[dataxmod].lextptr;
	if ((ltab[label] & 0x00FF0000) == 0x00800000) {
		/*
		 * First time, low order 2 bytes = offset of name in xref table:
		 * CODEINFO.xrefptr
		 */
		i1 = (USHORT) ltab[label];
		lext[datatab[dataxmod].lextsize++] = (USHORT) label;
		lext[datatab[dataxmod].lextsize++] = (USHORT) i1;
		ltab[label] = 0x00FFFFFF;
	}
	else {  /* not first time, offset of name in xref table has been moved to lext table */
		i1 = USHRT_MAX + 1;
		i3 = datatab[dataxmod].lextsize;
		for (i2 = 0; i2 < i3; i2 += 2) {
			if (label == (INT) lext[i2]) {
				i1 = lext[i2 + 1];
				break;
			}
		}
		if (i1 > USHRT_MAX) return RC_ERROR;
	}

	/* have offset of name in external reference table */
	pgmxmod = datatab[dataxmod].pgmxmod;
	if (pgmtab[pgmxmod].typeflg == 4) ptr = *pgmtab[pgmxmod].xrefptr;
	else ptr = *(*pgmtab[pgmxmod].codeptr)->xrefptr;
	ptr += i1;
	if (!method && *ptr) {  /* method without object */
		*pgmxmodptr = pgmxmod;
		method = TRUE;
	}
	if (getxlabel((CHAR *)(ptr + 1), pgmxmodptr, offptr, method)) return RC_ERROR;
	lmod[label] = pgmtab[*pgmxmodptr].pgmmod;
	ltab[label] = *offptr;
	return (0);
}

static INT getxlabel(CHAR *labelname, INT *modptr, INT *offptr, INT method)
{
	INT i1, i2, mod, off, pcntsize;
	UCHAR *ptr1, *ptr2;

	if (method) mod = *modptr;
	else mod = pgmhi - 1;
	i1 = (INT) strlen(labelname);
	for ( ; ; ) {  /* loop through xdef tables */
		if (pgmtab[mod].pgmmod != -1) {
			if (method) i2 = pgmtab[mod].xdefsize;
			else {
				if (pgmtab[mod].typeflg == 4) i2 = 0;
				else i2 = (*pgmtab[mod].codeptr)->xdefsize;
			}
			if (i2) {
				pcntsize = (*pgmtab[mod].codeptr)->pcntsize;
				if (method) ptr1 = *pgmtab[mod].xdefptr;  /* class external definition table */
				else ptr1 = *(*pgmtab[mod].codeptr)->xdefptr;  /* external definition table */
				ptr2 = ptr1 + i2;
				do {
					ptr1 += pcntsize;
					i2 = (INT) strlen((CHAR *) ptr1);
					if (i2 == i1) {  /* lengths match */
						if (!memcmp(labelname, ptr1, i1)) {
							*modptr = mod;
							if (pcntsize == 2) off = llhh(ptr1 - 2);
							else off = llmmhh(ptr1 - 3);
							*offptr = off;
							return(0);
						}
					}
					ptr1 += i2 + 1;
				} while (ptr1 < ptr2);
			}
		}
		else if (method) return RC_ERROR;
		if (method) {
			i2 = datatab[pgmtab[mod].dataxmod].nextmod;
			if (i2 == -1) return(-1);
			mod = datatab[i2].pgmxmod;
		}
		else if (!mod--) return RC_ERROR;
	}
}

/* check the validity of an address variable */
/* if indirection is invalid, set the address variable to null */
void chkavar(UCHAR *avar, INT fatalflg)
{
	INT i1, i2, mod;

	/* test for unresolved or data area not in use */
	if ((mod = getdatax(llhh(&avar[1]))) != RC_ERROR) {
		i1 = avar[0];
		i2 = llmmhh(&avar[3]);
		if (!(i2 & 0x00800000)) {  /* not label */
			i2 = adrtype(*datatab[mod].dataptr + i2);
			if ((i2 & 0xF0) == 0x20) {
				if (i2 <= 0x27) i2 -= 0x10;
				else i2 = 0x07;
			}
			if (i2 != -1 && (i1 == (0xB0 + i2) || i1 == 0xB7 || i1 == 0xBE)) return;
		}
		else if (i1 == 0xB7) return;
	}
	memset(&avar[1], 0xFF, 5);
	if (fatalflg) dbcerror(552);
}

/* returns variable type. value returned is between 0x00-0x4F, corresponding */
/* to the additional class data area coding - 0x70. returns -1 if not valid */
static INT adrtype(UCHAR *var)
{
	UCHAR c1, c2;

	c1 = *var;
	if (c1 < 0x80 || c1 == 0xE1 || c1 == 0xF0) return(0x00);
	if (c1 < 0xA0 || c1 == 0xE0 || (c1 >= 0xF4 && c1 <= 0xFC)) return(0x01);
	switch(c1) {
		case 0xA0:
			return(0x02);
		case 0xA1:
			return(0x03);
		case 0xA2:
			return(0x04);
		case 0xA3:
			return(0x06);
		case 0xA4:
			return(0x05);
		case 0xA5:
			return(-1);
		case 0xA6:
		case 0xA7:
		case 0xA8:
			c1 -= 0xA6;
			c2 = var[(c1 << 1) + 5];
			if (c2 < 0x80 || c2 == 0xF0) return(0x11 + c1);
			if (c2 < 0xA0 || (c2 >= 0xF4 && c2 <= 0xFC)) return(0x15 + c1);
			if (c2 == 0xB0) return(0x21 + c1);
			if (c2 == 0xB1) return(0x25 + c1);
			if (c2 == 0xB7) return(0x29 + c1);
			return(-1);
		case 0xA9:
			return(0x0C);
		case 0xAA:
			return(0x0D);
		case 0xAB:
			return(0x08);
		case 0xAC:
			return(0x09);
		case 0xAD:
			return(0x0A);
		case 0xAE:
			return(0x0B);
	}
	if ((c1 >= 0xB0 && c1 <= 0xBD) || (c1 >= 0xC1 && c1 <= 0xC7 && c1 != 0xC4))
		return(c1 - 0x80);  /* return(0x30 - 0x47) */
	return(-1);
}

/* return the hhll value at pgm[pcount] and add 2 to pcount */
INT gethhll()
{
	INT i1;

	i1 = hhll(&pgm[pcount]);
	pcount += 2;
	return(i1);
}

/* return redirected address and set fp, lp, pl, hl */
UCHAR *getvar(INT flag)
{
	INT adrflg;
	UCHAR *adr;
	CHAR work[128]; // Only used for error messages

	adrflg = FALSE;
	adrmod = dataxmodule;
	adroff = hhll(&pgm[pcount]);

	/* typical variable with 2 byte address */
	if (adroff < 0xC000) {
		adr = data + adroff;
		pcount += 2;
		goto getvar1;
	}

	/* qualified array */
	if (adroff < 0xC100) {
		adr = getarray();
		goto getvar1;
	}

	pcount += 2;
	/* variable with 3 byte address */
	if (adroff < 0xC180) {
		adroff = ((adroff & 0xFF) << 16) | hhll(&pgm[pcount]);
		adr = data + adroff;
		pcount += 2;
		goto getvar1;
	}

	if ((flag & (VAR_READ | VAR_WRITE)) && adroff == 0xFFFF) return(NULL);
	sprintf(work, "Fail in getvar, adroff=%#x", (UINT)adroff);

getvar0:
	if (pcntsave != -1) {
		pcount = pcntsave;
		pcntsave = -1;
	}
	dbcerrinfo(505, (UCHAR*)work, -1);

getvar1:
	if (adroff >= datatab[adrmod].size) {
		sprintf(work, "Fail in getvar, adroff=%#x, datatab[adrmod].size=%d", (UINT)adroff, datatab[adrmod].size);
		goto getvar0;
	}
	if (adr[0] < 0x80) {  /* small char */
		fp = adr[0];
		lp = adr[1];
		pl = adr[2];
		hl = 3;
		vartype = TYPE_CHAR;
		if (!fp && lp == 0xFF) {  /* null */
			lp = 0;
			if (flag & VAR_WRITE) adr[1] = 0;
			else vartype |= TYPE_NULL;
		}
	}
	else if (adr[0] < 0xA0) {  /* num */
		pl = lp = adr[0] & 0x1F;
		fp = hl = 1;
		vartype = TYPE_NUM;
		if (!pl) {  /* null */
			pl = lp = adr[1] & 0x1F;
			if (flag & VAR_WRITE) {
				adr[0] |= adr[1];
				if (adr[1] & 0x80) adr[1] = '.';
				else if (pl == 1) adr[1] = '0';
				else adr[1] = ' ';
			}
			else vartype |= TYPE_NULL;
		}
	}
	else {
		vartype = 0;
		switch(adr[0]) {
			case 0xA0:
			case 0xA1:
			case 0xA2:
			case 0xA3:
			case 0xA9:
			case 0xAA:
			case 0xAB:
			case 0xAC:
			case 0xAD:
			case 0xAE:
				fp = lp = pl = hl = 0;
				vartype = 0;
				break;
			case 0xA4:
				fp = lp = pl = hl = 0;
				vartype = TYPE_LIST;
				break;
			case 0xA5:
				fp = lp = pl = hl = 0;
				vartype = 0;
				break;
			case 0xA6:
			case 0xA7:
			case 0xA8:
				fp = lp = pl = hl = 0;
				vartype = TYPE_ARRAY;
				break;
			case 0xB0:
			case 0xB1:
			case 0xB2:
			case 0xB3:
			case 0xB4:
			case 0xB5:
			case 0xB6:
			case 0xB7:
			case 0xB8:
			case 0xB9:
			case 0xBA:
			case 0xBB:
			case 0xBC:
			case 0xBD:
			case 0xBE:
			case 0xC1:
			case 0xC2:
			case 0xC3:
			case 0xC5:
			case 0xC6:
			case 0xC7:
				/* its an address variable */
				if (!(flag & VAR_ADR) || adr[0] == 0xBE) {
					if ((adrmod = getdatax(llhh(&adr[1]))) != RC_ERROR) {  /* points to something */
						adroff = llmmhh(&adr[3]);
						adrflg = TRUE;
						if ((adroff & 0x00FF0000) != 0x00800000) {  /* not a label */
							adr = *datatab[adrmod].dataptr + adroff;
							goto getvar1;
						}
					}
					else if (flag & VAR_ADRX) {
						adrmod = 0xFFFF;
						adroff = 0xFFFFFF;
						adrflg = TRUE;
					}
					else {
						if (pcntsave != -1) {
							pcount = pcntsave;
							pcntsave = -1;
						}
						dbcerror(552);
					}
				}
				fp = lp = pl = hl = 0;
				vartype = 0;
				break;
			case 0xCD:
				/* first access to a pre-version 8 address variable */
				if (makevar(NULL, adr, MAKEVAR_ADDRESS | MAKEVAR_DATA)) dbcerror(556);
				/* reset adr because makevar may have moved memory */
				adr = *datatab[adrmod].dataptr + adroff;
				// fall through
			case 0xCE:
			case 0xCF:
				/* it's a pre-version 8 address variable */
				if (adr[0] == 0xCF) adrmod = 0;
				adroff = llmmhh(&adr[1]);
				adr = *datatab[adrmod].dataptr + adroff;
				adrflg = TRUE;
				goto getvar1;
			case 0xE0:
			case 0xE1:
				/* num or char literal */
				if (flag & VAR_WRITE) {
					if (pcntsave != -1) {
						pcount = pcntsave;
						pcntsave = -1;
					}
					dbcerror(557);
				}
				if (adr[0] == 0xE1) vartype = TYPE_CHARLIT;
				else vartype = TYPE_NUMLIT;
				pl = lp = adr[1];
				if (lp) fp = 1;
				else fp = 0;
				hl = 2;
				break;
			case 0xF0:
				/* large char */
				fp = llhh(&adr[1]);
				lp = llhh(&adr[3]);
				pl = llhh(&adr[5]);
				hl = 7;
				vartype = TYPE_CHAR;
				if (!fp && lp == 0xFFFF) {  /* null */
					lp = 0;
					if (flag & VAR_WRITE) adr[3] = adr[4] = 0;
					else vartype |= TYPE_NULL;
				}
				break;
			case 0xF4:
			case 0xF5:
			case 0xF6:
			case 0xF7:
				/* null float */
				if (flag & VAR_WRITE) adr[0] += 0x04;
				else vartype = TYPE_NULL;
				// fall through
			case 0xF8:
			case 0xF9:
			case 0xFA:
			case 0xFB:
				/* float */
				fp = lp = -1;
				hl = 2;
				pl = 8;
				vartype |= TYPE_FLOAT;
				break;
			case 0xFC:
				/* integer */
				fp = lp = -1;
				hl = 2;
				pl = 4;
				vartype = TYPE_INT;
				if (adr[1] & 0x80) {  /* null */
					if (flag & LIST_WRITE) adr[1] &= ~0x80;
					vartype |= TYPE_NULL;
				}
				break;
			case 0xFD:
				/* common redirection */
				adr = *datatab[adrmod = dataxchn].dataptr + adroff;
				goto getvar1;
			case 0xAF:
			case 0xBF:
			case 0xC0:
			case 0xC4:
			case 0xC8:
			case 0xC9:
			case 0xCA:
			case 0xCB:
			case 0xCC:
			case 0xD0:
			case 0xD1:
			case 0xD2:
			case 0xD3:
			case 0xD4:
			case 0xD5:
			case 0xD6:
			case 0xD7:
			case 0xD8:
			case 0xD9:
			case 0xDA:
			case 0xDB:
			case 0xDC:
			case 0xDD:
			case 0xDE:
			case 0xDF:
			case 0xE2:
			case 0xE3:
			case 0xE4:
			case 0xE5:
			case 0xE6:
			case 0xE7:
			case 0xE8:
			case 0xE9:
			case 0xEA:
			case 0xEB:
			case 0xEC:
			case 0xED:
			case 0xEE:
			case 0xEF:
			case 0xF1:
			case 0xF2:
			case 0xF3:
			case 0xFE:
			case 0xFF:
				sprintf(work, "Fail in getvar, adroff=%#x, adr[0]=%#.2x", (UINT)adroff, (UINT)adr[0]);
				goto getvar0;
		}
	}
	if (adrflg) vartype |= TYPE_ADDRESS;
	return(adr);
}

/**
 * get module, data offset, and return actual address
 */
void getvarx(UCHAR *ptr, INT *pcnt, _Out_ int *modptr, _Out_ int *offptr)
{
	INT i1, i2, mod, datamodsav, dataxmodsav;
	UCHAR *adr, *datasave, *pgmsave;

	/* save & initialize data, pgm & pcount */
	datamodsav = datamodule;
	dataxmodsav = dataxmodule;
	datasave = data;
	mod = *modptr;
	if (mod != datamodule) {
		if (mod != -1) for (i1 = 0; i1 < datahi && mod != datatab[i1].datamod; i1++);
		else i1 = datahi;
		if (i1 == datahi) dbcerror(555);
		datamodule = mod;
		dataxmodule = i1;
		data = *datatab[dataxmodule].dataptr;
	}
	pgmsave = pgm;
	pgm = ptr;
	pcntsave = pcount;
	pcount = *pcnt;

	/* get variable and calculate address */
	adr = getvar(VAR_READ);
	if (adr != NULL) {
		*modptr = datatab[adrmod].datamod;
		*offptr = adroff;
	}
	else {
		*modptr = 0xFFFF;
		*offptr = 0xFFFFFF;
	}

	/* restore data, pgm & pcount */
	datamodule = datamodsav;
	dataxmodule = dataxmodsav;
	data = datasave;
	pgm = pgmsave;
	i2 = pcount;
	pcount = pcntsave;
	pcntsave = -1;
	*pcnt = i2;
}

/* return aligned pointer to data area file block */
DAVB *getdavb()
{
	intptr_t i1;
	UCHAR *adr;

	adr = getvar(VAR_WRITE);
/*** CODE: MAYBE PERFORM TEST TO VERIFY THAT IT IS A DAVB BLOCK ***/
	i1 = 0; //(intptr_t)adr & 0x07;
	if (i1) adr += (ptrdiff_t) ((intptr_t*) 0x08 - i1);  /* align to 8 byte address */
	return((DAVB *) adr);
}

/**
 * Return aligned pointer to data area file block,
 * and bytes required for alignment
 *
 * Does not move memory
 */
DAVB *aligndavb(UCHAR *adr, INT *bytes)
{
	if (bytes != NULL) *bytes = 0;
	return (DAVB *)adr;
}

/**
 * return each variable of a list and set fp, lp, pl, hl
 */
UCHAR *getlist(INT flag)
{
	INT i1, adrflg, cnt, num;
	UCHAR *adr;
	static INT listcnt[LIST_NUMMASK + 1];
	static INT listmod[LIST_NUMMASK + 1][LISTMAX];
	static INT listoff[LIST_NUMMASK + 1][LISTMAX];

	num = flag & LIST_NUMMASK;
	setpgmdata();
	if (flag & LIST_END) {
		listcnt[num] = 0;
		if (flag & LIST_PROG) {
			while (pgm[pcount] <= 0xC1) skipadr();
			pcount++;
		}
		return(NULL);
	}

	adrflg = 0;
	cnt = listcnt[num];
getlist1:
	if (!cnt) {  /* get next variable from list */
		if (flag & LIST_PROG) {
			adrmod = dataxmodule;
			adroff = hhll(&pgm[pcount]);

			/* typical variable with 2 byte address */
			if (adroff < 0xC000) {
				pcount += 2;
				adr = data + adroff;
				goto getlist2;
			}

			/* qualified array */
			if (adroff < 0xC100) {
				adr = getarray();
				goto getlist2;
			}

			pcount += 2;
			/* variable with 3 byte address */
			if (adroff < 0xC180) {
				adroff = ((adroff & 0xFF) << 16) | hhll(&pgm[pcount]);
				adr = data + adroff;
				pcount += 2;
				goto getlist2;
			}


			pcount--;
			return(NULL);
		}

		if (adrmod == dataxmodule) adr = data;
		else adr = *datatab[adrmod].dataptr;
		adr += adroff;
		if ((flag & (LIST_LIST | LIST_LIST1)) && *adr == 0xA4) goto getlist2;
		if ((flag & (LIST_ARRAY | LIST_ARRAY1)) && *adr >= 0xA6 && *adr <= 0xA8) goto getlist2;
		i1 = skipvar(adr, FALSE);
		adroff += i1;
		adr += i1;
	}
	else {
		adrmod = listmod[num][cnt];
		adroff = listoff[num][cnt];
		if (adrmod == dataxmodule) adr = data;
		else adr = *datatab[adrmod].dataptr;
		adr += adroff;
		if (*adr == 0xA5) {  /* listend */
			if (!(cnt = --listcnt[num]) && !(flag & LIST_PROG)) {
				return(NULL);
			}
			if (listoff[num][cnt] == -1) {
				i1 = 1;
				if (adr[1] == 0xFE) {
					while (adr[++i1] != 0xFE);
					i1++;
				}
				listoff[num][cnt] = adroff + i1;
			}
			goto getlist1;
		}
	}

getlist2:
	if (adr[0] < 0x80) {  /* small char */
		fp = adr[0];
		lp = adr[1];
		pl = adr[2];
		hl = 3;
		vartype = TYPE_CHAR;
		if (!fp && lp == 0xFF) {  /* null */
			lp = 0;
			if (flag & VAR_WRITE) adr[1] = 0;
			else vartype |= TYPE_NULL;
		}
	}
	else if (adr[0] < 0xA0) {  /* num */
		pl = lp = adr[0] & 0x1F;
		fp = hl = 1;
		vartype = TYPE_NUM;
		if (!pl) {  /* null */
			pl = lp = adr[1] & 0x1F;
			if (flag & LIST_WRITE) {
				adr[0] |= adr[1];
				if (adr[1] & 0x80) adr[1] = '.';
				else if (pl == 1) adr[1] = '0';
				else adr[1] = ' ';
			}
			else vartype |= TYPE_NULL;
		}
	}
	else {
		vartype = 0;
		switch(adr[0]) {
			case 0xA0:
			case 0xA1:
			case 0xA2:
			case 0xA3:
			case 0xA9:
			case 0xAA:
			case 0xAB:
			case 0xAC:
			case 0xAD:
			case 0xAE:
				fp = lp = pl = hl = 0;
				break;
			case 0xA4:
				if (!(flag & (LIST_LIST | LIST_LIST1))) {
					fp = lp = pl = hl = 0;
					vartype = TYPE_LIST;
					break;
				}
				i1 = 1;
				goto getlist3;
			case 0xA5:
				fp = lp = pl = hl = 0;
				break;

			/**
			 * One, two, or three dimensional array start
			 */
			case 0xA6:
			case 0xA7:
			case 0xA8:
				if (!(flag & (LIST_ARRAY | LIST_ARRAY1))) {
					fp = lp = pl = hl = 0;
					vartype = TYPE_ARRAY;
					break;
				}
				i1 = ((adr[0] - 0xA6) << 1) + 5;
getlist3:
				if (adr[i1] == 0xFE) {
					while (adr[++i1] != 0xFE);
					i1++;
				}
				adroff += i1;
				adr += i1;
				if (*adr == 0xA5) {  /* don't push empty list/array */
					if (cnt) {
						if (adrflg) i1 = adrflg;
						else i1++;
						listoff[num][cnt] += i1;
					}
					else if (!(flag & LIST_PROG)) return(NULL);
					goto getlist1;
				}
				flag &= ~(LIST_LIST1 | LIST_ARRAY1);
				if (!adrflg) listoff[num][cnt] = -1;
				else listoff[num][cnt] += adrflg;
				if ((cnt = ++listcnt[num]) == LISTMAX) dbcerror(559);
				listmod[num][cnt] = adrmod;
				listoff[num][cnt] = adroff;
				adrflg = 0;
				goto getlist2;
			case 0xB0:
			case 0xB1:
			case 0xB2:
			case 0xB3:
			case 0xB4:
			case 0xB5:
			case 0xB6:
			case 0xB7:
			case 0xB8:
			case 0xB9:
			case 0xBA:
			case 0xBB:
			case 0xBC:
			case 0xBD:
			case 0xBE:
			case 0xC1:
			case 0xC2:
			case 0xC3:
			case 0xC5:
			case 0xC6:
			case 0xC7:
				/* its an address variable */
				if (!(flag & LIST_ADR) || adr[0] == 0xBE) {
					if ((adrmod = getdatax(llhh(&adr[1]))) != RC_ERROR) {  /* points to something */
						adroff = llmmhh(&adr[3]);
						if (!adrflg) {
							adrflg = 6;
							if (cnt && adr[6] == 0xFE) {
								while (adr[++adrflg] != 0xFE);
								adrflg++;
							}
						}
						if ((adroff & 0x00FF0000) != 0x00800000) {  /* not a label */
							adr = *datatab[adrmod].dataptr + adroff;
							goto getlist2;
						}
					}
					else if (flag & LIST_ADRX) {
						adrmod = 0xFFFF;
						adroff = 0xFFFFFF;
						if (!adrflg) {
							adrflg = 6;
							if (cnt && adr[6] == 0xFE) {
								while (adr[++adrflg] != 0xFE);
								adrflg++;
							}
						}
					}
					else dbcerror(552);
				}
				fp = lp = pl = hl = 0;
				break;
			case 0xCD:
				/* first access to a pre-version 8 address variable */
				if (makevar(NULL, adr, MAKEVAR_ADDRESS | MAKEVAR_DATA)) dbcerror(556);
				/* reset adr because makevar may have moved memory */
				adr = *datatab[adrmod].dataptr + adroff;
				// fall through
			case 0xCE:
				// fall through
			case 0xCF:
				/* it's a pre-version 8 address variable */
				if (adr[0] == 0xCF) adrmod = 0;
				adroff = llmmhh(&adr[1]);
				adr = *datatab[adrmod].dataptr + adroff;
				if (!adrflg) adrflg = 4;
				goto getlist2;
			case 0xE0:
				// fall through
			case 0xE1:
				/* num or char literal */
				if (adr[0] == 0xE1) vartype = TYPE_CHARLIT;
				else vartype = TYPE_NUMLIT;
				pl = lp = adr[1];
				if (lp) fp = 1;
				else fp = 0;
				hl = 2;
				break;
			case 0xF0:
				/* large char */
				fp = llhh(&adr[1]);
				lp = llhh(&adr[3]);
				pl = llhh(&adr[5]);
				hl = 7;
				vartype = TYPE_CHAR;
				if (!fp && lp == 0xFFFF) {  /* null */
					lp = 0;
					if (flag & VAR_WRITE) adr[3] = adr[4] = 0;
					else vartype |= TYPE_NULL;
				}
				break;
			case 0xF4:
			case 0xF5:
			case 0xF6:
			case 0xF7:
				/* null float */
				if (flag & LIST_WRITE) adr[0] += 0x04;
				else vartype = TYPE_NULL;
				// fall through
			case 0xF8:
			case 0xF9:
			case 0xFA:
			case 0xFB:
				/* float */
				fp = lp = -1;
				hl = 2;
				pl = 8;
				vartype |= TYPE_FLOAT;
				break;
			case 0xFC:
				/* integer */
				fp = lp = -1;
				hl = 2;
				pl = 4;
				vartype = TYPE_INT;
				if (adr[1] & 0x80) {  /* null */
					if (flag & LIST_WRITE) adr[1] &= ~0x80;
					vartype |= TYPE_NULL;
				}
				break;
			case 0xFD:
				/* common redirection */
				adr = *datatab[adrmod = dataxchn].dataptr + adroff;
				goto getlist2;
			case 0xAF:
			case 0xBF:
			case 0xC0:
			case 0xC4:
			case 0xC8:
			case 0xC9:
			case 0xCA:
			case 0xCB:
			case 0xCC:
			case 0xD0:
			case 0xD1:
			case 0xD2:
			case 0xD3:
			case 0xD4:
			case 0xD5:
			case 0xD6:
			case 0xD7:
			case 0xD8:
			case 0xD9:
			case 0xDA:
			case 0xDB:
			case 0xDC:
			case 0xDD:
			case 0xDE:
			case 0xDF:
			case 0xE2:
			case 0xE3:
			case 0xE4:
			case 0xE5:
			case 0xE6:
			case 0xE7:
			case 0xE8:
			case 0xE9:
			case 0xEA:
			case 0xEB:
			case 0xEC:
			case 0xED:
			case 0xEE:
			case 0xEF:
			case 0xF1:
			case 0xF2:
			case 0xF3:
			case 0xFE:
			case 0xFF:
				{
					CHAR work[64];
					sprintf(work, "Fail in getlist, adr[0] = %#x", (UINT)adr[0]);
					dbcerrinfo(505, (UCHAR*)work, -1);
				}
		}
	}

	if ((flag & LIST_IGNORE) && !(vartype & (TYPE_NUMERIC | TYPE_CHARACTER | TYPE_ARRAY))) {
		if (!cnt) {
			if (!(flag & LIST_PROG)) return(NULL);
		}
		else {
			if (adrflg) i1 = adrflg;
			else i1 = skipvar(adr, FALSE);
			listoff[num][cnt] += i1;
		}
		goto getlist1;
	}
	if ((flag & LIST_WRITE) && !(vartype & (TYPE_NUMVAR | TYPE_CHAR | TYPE_ARRAY))) dbcerror(557);

	if (cnt) {
		if (adrflg) i1 = adrflg;
		else if (hl) {
			i1 = hl + pl;
			while (adr[i1] == 0xF3) i1++;
			if (adr[i1] == 0xFE) {
				while (adr[++i1] != 0xFE);
				i1++;
			}
		}
		else i1 = skipvar(adr, FALSE);
		listoff[num][cnt] += i1;
	}
	if (adrflg) vartype |= TYPE_ADDRESS;
	return(adr);
}

INT getlistx(INT flag, INT *mod, INT *off)
{
	if (getlist(flag) == NULL) return(FALSE);
	if (adrmod == 0xFFFF) *mod = 0xFFFF;
	else *mod = datatab[adrmod].datamod;
	*off = adroff;
	return(TRUE);
}

/* skip over a variable */
INT skipvar(UCHAR *adr, INT listflg)
{
	INT i1, i2, i3;
	UCHAR c1, formatflg;

	formatflg = FALSE;
	c1 = *adr;
	if (c1 < 0x80) {  /* small dim */
		i3 = adr[2] + 3;
		formatflg = TRUE;
	}
	else if (c1 < 0xA0) {  /* form */
		if (c1 != 0x80) i3 = (c1 & 0x1F) + 1;
		else i3 = (adr[1] & 0x1F) + 1;
		formatflg = TRUE;
	}
	else if (c1 == 0xF0) {  /* large dim */
		i3 = llhh(&adr[5]) + 7;
		formatflg = TRUE;
	}
	else if (c1 >= 0xF4 && c1 <= 0xFB) i3 = 10;  /* float */
	else if (c1 == 0xFC) i3 = 6;  /* integer */
	else if (c1 >= 0xB0 && c1 <= 0xC7) i3 = 6;  /* address variable */
	else if (c1 >= 0xA0 && c1 <= 0xAE) switch(c1) {
		case 0xA0:  /* file */
		case 0xA1:  /* ifile */
		case 0xA2:  /* afile */
		case 0xA3:  /* comfile */
		case 0xA9:  /* pfile */
		case 0xAA:  /* object */
		case 0xAB:  /* image */
		case 0xAC:  /* queue */
		case 0xAD:  /* resource */
		case 0xAE:  /* device */
			i3 = 44;
			break;
		case 0xA4:  /* list */
			i3 = 1;
			if (adr[i3] == 0xFE) {
				while (adr[++i3] != 0xFE);
				i3++;
			}
			if (!listflg) {
				while ( (i1 = skipvar(&adr[i3], FALSE)) ) i3 += i1;
				i3++;
			}
			break;
		case 0xA5:  /* listend */
			if (!listflg) return(0);
			i3 = 1;
			break;
		case 0xA6:  /* one dimensional array */
		case 0xA7:  /* two dimensional array */
		case 0xA8:  /* three dimensional array */
			i2 = (c1 - 0xA6 + 2) << 1;
			if (!listflg) {
				for (i1 = i3 = 1; i1 < i2; i1 += 2)
					i3 *= llhh(&adr[i1]);
				i3 += i1 + 1;
			}
			else i3 = i2 + 1;
			break;
	}
	else if (c1 == 0xE0 || c1 == 0xE1) i3 = adr[1] + 2;  /* num and char literal */
	else if (c1 >= 0xCD && c1 <= 0xCF) i3 = 4;  /* pre-version 8 address variables */
	else {
		CHAR work[64];
		sprintf(work, "Unknown variable type in skipvar (%x)", (UINT)c1);
		dbcerrinfo(505, (UCHAR*)work, -1);  /* invalid variable */
	}

	if (formatflg) while (adr[i3] == 0xF3) i3++;
	if (adr[i3] == 0xFE) {
		while (adr[++i3] != 0xFE);
		i3++;
	}
	return(i3);
}

/* skip over the program reference to a variable */
void skipadr()
{
	INT i1, off;
	UCHAR *adr;

	if (pgm[pcount] < 0xC0 || pgm[pcount] == 0xFF) pcount += 2;  /* 2 byte address */
	else if (pgm[pcount] == 0xC0) {  /* qualified array */
		pcount++;
		off = hhll(&pgm[pcount]);
		pcount += 2;

		/* typical array variable with 2 byte address */
		if (off < 0xC000) {
			adr = data + off;
		}
		else {  /* array variable with 3 byte address */
			off = ((off & 0xFF) << 16) | hhll(&pgm[pcount]);
			adr = data + off;
			pcount += 2;
		}

		if (*adr == 0xFD) adr = *datatab[dataxchn].dataptr + off;  /* common redirection */
		if ((*adr & 0xF8) == 0xC0) i1 = *adr & 0x03;  /* its an address array variable */
		else i1 = *adr - 0xA6 + 1;

		/* skip over the array element values */
		while (i1--) {
			if (pgm[pcount] < 0xC0 || pgm[pcount] == 0xFF) pcount += 2;
			else if (pgm[pcount] == 0xC0) skipadr();
			else if (pgm[pcount] == 0xC1) pcount += 4;
			else dbcerror(505);
		}
	}
	else if (pgm[pcount] == 0xC1) pcount += 4;  /* 3 byte address */
	else {
		UCHAR work[64];
		sprintf((CHAR*)work, "Unknown variable type in skipadr, pgm[pcount]=%x", (UINT)pgm[pcount]);
		dbcerrinfo(505, work, -1);  /* invalid variable */
	}
}

/* return variable type and size of indexes if array type */
INT gettype(INT *size1, INT *size2, INT *size3)
{
	UCHAR c1, *adr;

	dbcflags &= ~DBCFLAG_OVER;
	*size1 = *size2 = *size3 = 0;
	c1 = pgm[pcount];
	if (getmver(datamodule) <= 7) {
		/* this is no longer supported */
		if (c1 >= 0xD0 && c1 <= 0xEF) dbcerror(560);
	}

	adr = getvar(VAR_ADRX);
	if (vartype & TYPE_CHARACTER) return(1);  /* char, char literal */
	if (vartype & TYPE_NUMERIC) return(2);  /* num, int, float, num literal */
	if (vartype & TYPE_LIST) return(5);  /* list */
	c1 = *adr;
	if (vartype & TYPE_ARRAY) {
		*size1 = llhh(&adr[1]);
		if (c1 >= 0xA7) {
			*size2 = llhh(&adr[3]);
			if (c1 == 0xA8) *size3 = llhh(&adr[5]);
		}
		adr += 5 + ((c1 - 0xA6) << 1);
		c1 = *adr;
		if (c1 < 0x80 || c1 == 0xF0 || c1 == 0xB0) return(3);  /* dim */
		if (c1 < 0xA0 || (c1 >= 0xF4 && c1 <= 0xFC) || c1 == 0xB1) return(4);  /* form, float, integer */
		if (c1 == 0xB7) return(17);
		dbcflags |= DBCFLAG_OVER;
		return(0);
	}
	if (c1 == 0xA0) return(6);  /* file */
	if (c1 == 0xA1) return(7);  /* ifile */
	if (c1 == 0xA2) return(8);  /* afile */
	if (c1 == 0xA3) return(9);  /* cfile */
	if (c1 == 0xAB) return(10);  /* image */
	if (c1 == 0xAE) return(11);  /* device */
	if (c1 == 0xAD) return(12);  /* resource */
	if (c1 == 0xAC) return(13);  /* queue */
	if (c1 == 0xA9) return(15);  /* pfile */
	if (c1 == 0xAA) return(16);  /* object */
	if (c1 == 0xB7) {  /* var type variable, test if label */
		if (getdatax(llhh(&adr[1])) != RC_ERROR && (llmmhh(&adr[3]) & 0x00FF0000) == 0x00800000)
			return(14);  /* label */
	}
	dbcflags |= DBCFLAG_OVER;
	return(0);
}

/* return redirected address */
UCHAR *findvar(INT mod, INT off)
{
	UCHAR *adr;

	if ((mod = getdatax(mod)) == RC_ERROR) return(NULL);  /* data area not in use */
	while (TRUE) {
		adr = *datatab[mod].dataptr + off;
		if (*adr < 0xAF || *adr == 0xE0 || *adr == 0xE1 || *adr == 0xF0 || (*adr >= 0xF4 && *adr <= 0xFC))
			return(adr);
		if (*adr >= 0xB0 || *adr <= 0xC7) {  /* address variable */
			if ((mod = getdatax(llhh(&adr[1]))) == RC_ERROR) return(NULL);  /* points to nothing */
			off = llmmhh(&adr[3]);
		}
		else return(NULL);  /* unsupported type */
	}
}

/**
 * Get pointer to a array element
 * It is assumed that adrmod is set to the current module (dataxmodule)
 *
 * Does not move memory
 */
static UCHAR *getarray()
{
	INT i1, i2, i3, savemod, saveoff, offset, size, element[3];
	UCHAR *adr;

	pcount++;
	adroff = hhll(&pgm[pcount]);
	pcount += 2;

	/* typical variable with 2 byte address */
	if (adroff < 0xC000) adr = data + adroff;
	else {  /* variable with 3 byte address */
		adroff = ((adroff & 0xFF) << 16) | hhll(&pgm[pcount]);
		adr = data + adroff;
		pcount += 2;
	}

	for (;;) {
		if (adroff >= datatab[adrmod].size) {
			if (pcntsave != -1) {
				pcount = pcntsave;
				pcntsave = -1;
			}
			dbcerrinfo(505, (UCHAR*)"Fail in getarray, adroff too big", -1);
		}
		if (*adr == 0xFD) adr = *datatab[adrmod = dataxchn].dataptr + adroff;
		else if (*adr >= 0xB0) {  /* its an address variable */
			if (*adr == 0xCE) adroff = llmmhh(&adr[1]);
			else if (*adr == 0xCF) {
				adrmod = 0;
				adroff = llmmhh(&adr[1]);
			}
			else {
				if (*adr <= 0xC7) {
					i1 = llhh(&adr[1]);
					adroff = llmmhh(&adr[3]);
				}
				else i1 = 0xFFFF;
				if ((adrmod = getdatax(i1)) == RC_ERROR || (adroff & 0x00FF0000) == 0x00800000)
				{  /* points to nothing */
					if (pcntsave != -1) {
						pcount = pcntsave;
						pcntsave = -1;
					}
					dbcerror(552);
				}
			}
			adr = *datatab[adrmod].dataptr + adroff;
		}
		else break;
	}

	/*
	 * A6 is a one-dimensioanl array start.
	 * A7 is a two-dimensioanl array start.
	 * A8 is a three-dimensioanl array start.
	 */
	i1 = *adr - 0xA6 + 1;
	if (i1 < 1 || i1 > 3) {
		if (pcntsave != -1) {
			pcount = pcntsave;
			pcntsave = -1;
		}
		dbcerrinfo(505, (UCHAR*)"Fail in getarray, bad dimension byte", -1);
	}
	savemod = adrmod;
	saveoff = adroff;
	for (i2 = 0; i2 < i1; ) {
		if (pgm[pcount] == 0xFF) {
			i3 = pgm[pcount + 1];
			pcount += 2;
		}
		else i3 = nvtoi(getvar(VAR_READ));
		element[i2++] = i3;
	}
	adrmod = savemod;
	adroff = saveoff;
	offset = 3;
	size = llhh(adr + (i1 << 1) + 1);
	for (i2 = i1; i2--; ) {
		i3 = llhh(adr + (i2 << 1) + 1);
		if (element[i2] < 1 || element[i2] > i3) {
			if (pcntsave != -1) {
				pcount = pcntsave;
				pcntsave = -1;
			}
			dbcerror(507);
		}
		offset += (element[i2] - 1) * size + 2;
		size *= i3;
	}
	adroff += offset;
	adr += offset;
	return(adr);
}

/**
 * if flag & MAKEVAR_VARIABLE, str contains variable type to create
 * if flag & MAKEVAR_ADDRESS, create v7 -> v8 address variable
 * if flag & MAKEVAR_PFILE, create pfile address variable
 * if flag & MAKEVAR_NAME, str contains name of variable
 * if flag & MAKEVAR_GLOBAL, variable is a global type
 * if flag & MAKEVAR_DATA, adr is pointer to data, (adrmod & adroff are correctly set)
 */
INT makevar(CHAR *str, UCHAR *adr, INT flag)
{
	INT i1, i2, i3, i4, lft, rgt, datamod, dataxmod, varflg;
	INT arraysiz, arrayx, size = 0, elements[3];
	INT horz, vert, bits, imgrefnum;
	INT32 x1;
	DOUBLE64 y1;
	CHAR varname[64];
	UCHAR c1, c2, *ptr;
	DAVB *davb;
	GLOBALINFO *globalptr, **globalpptr, **globalpptr1, **globalpptr2;

	arrayx = 0;
	varname[0] = '\0';
	if (flag & (MAKEVAR_VARIABLE | MAKEVAR_NAME)) {
		/* remove spaces */
		for (i1 = 0, i2 = 0; str[i1]; i1++) if (!isspace(str[i1])) varname[i2++] = str[i1];
		varname[i2] = '\0';
		if (flag & MAKEVAR_VARIABLE) {
			if (flag & MAKEVAR_NAME) {
				for (i1 = 0; varname[i1] && varname[i1] != ':'; i1++) ;
				if (!varname[i1]) return(1);
				varname[i1++] = '\0';
			}
			else i1 = 0;

			c1 = (UCHAR) toupper(varname[i1]);
			if (c1 == 'C') varflg = TYPE_CHAR;
			else if (c1 == 'N') {
				c1 = (UCHAR) toupper(varname[i1 + 1]);
				if (c1 == 'I') {
					varflg = TYPE_INT;
					i1++;
				}
				else if (c1 == 'F') {
					varflg = TYPE_FLOAT;
					i1++;
				}
				else varflg = TYPE_NUM;
			}
			else if (c1 == 'D') varflg = TYPE_DEVICE;
			else if (c1 == 'I') varflg = TYPE_IMAGE;
			else if (c1 == 'R') varflg = TYPE_RESOURCE;
			else if (c1 == 'O') varflg = TYPE_OBJECT;
			else return(1);

			if (varflg & (TYPE_CHAR | TYPE_NUM | TYPE_INT | TYPE_FLOAT)) {
				i1++;
				for (lft = 0; isdigit(varname[i1]); ) lft = lft * 10 + varname[i1++] - '0';
				rgt = 0;
				if (varname[i1] == '.') {
					i1++;
					while (isdigit(varname[i1])) rgt = rgt * 10 + varname[i1++] - '0';
				}
				if (varname[i1] == '[' || varname[i1] == '(') {
					i1++;
					arraysiz = 1;
					while (arrayx < 3) {
						for (elements[arrayx] = 0; isdigit(varname[i1]); )
							elements[arrayx] = elements[arrayx] * 10 + varname[i1++] - '0';
						if (!elements[arrayx]) return(1);
						arraysiz *= elements[arrayx++];
						if (varname[i1] == ']' || varname[i1] == ')') break;
						if (varname[i1++] != ',') return(1);
					}
				}
				if (varflg & TYPE_CHAR) {  /* character variable */
					size = lft;
					if (size > 65500) return(1);
					if (lft < 128) size += 3;
					else size += 7;
				}
				else if (varflg & TYPE_NUM) {  /* numeric variable */
					size = lft + rgt;
					if (rgt) size++;
					if (!size || size > 31) return(1);
					size++;
				}
				else if (varflg & TYPE_INT) {  /* integer variable */
					size = lft + rgt;
					if (rgt) size++;
					if (!size || size > 31) return(1);
					size = 2 + sizeof(INT32);
				}
				else {  /* float variable */
					size = lft + rgt;
					if (rgt) size++;
					if (!size || size > 31) return(1);
					size = 2 + sizeof(DOUBLE64);
				}
				if (arrayx) {
					if (varflg & TYPE_CHAR) c1 = (UCHAR)(0xC0 + arrayx);
					else c1 = (UCHAR)(0xC4 + arrayx);
				}
				else if (varflg & TYPE_CHAR) c1 = 0xB0;
				else c1 = 0xB1;
			}
			else {
				size = 44;
				if (varflg == TYPE_DEVICE) c1 = 0xBB;
				else if (varflg == TYPE_IMAGE) {
					i1++;
					for (horz = 0; isdigit(varname[i1]); ) horz = horz * 10 + varname[i1++] - '0';
					vert = 0;
					if (varname[i1] == ',') {
						i1++;
						while (isdigit(varname[i1])) vert = vert * 10 + varname[i1++] - '0';
					}
					bits = 0;
					if (varname[i1] == ',') {
						i1++;
						while (isdigit(varname[i1])) bits = bits * 10 + varname[i1++] - '0';
					}
					if (!horz || !vert) return(1);

					c1 = 0xB8;
				}
				else if (varflg == TYPE_RESOURCE) c1 = 0xBA;
				else c1 = 0xBD;  /* object */
			}
			if (*adr != c1 && *adr != 0xB7) return(1);
		}
	}
	else if (flag & MAKEVAR_ADDRESS) size = 6;
	else if (flag & MAKEVAR_PFILE) size = 44;
	else return(505);

	if (flag & MAKEVAR_NAME) {
		if (!varname[0]) return(1);
		varname[31] = '\0';

		i1 = 1;
		globalpptr1 = globaltabptr;
		globalptr = *globalpptr1;
		do {
			globalpptr = globalpptr1;
			globalpptr1 = globalptr->nextptr;
			if (globalpptr1 == NULL) break;
			globalptr = *globalpptr1;
			i1 = strcmp(varname, globalptr->name);
		} while (i1 > 0); /* break if i1 is zero or negative. that is, varname <= globalptr->name */

		if (!i1) {  /* global variable exists */
			if (flag & MAKEVAR_VARIABLE) {
				ptr = *datatab[0].dataptr;
				ptr += globalptr->offset;
				c2 = *ptr;
				if (c2 < 0x80 || c2 == 0xF0) c2 = 0xB0;  /* dim */
				else if (c2 < 0xA0 || (c2 >= 0xF4 && c2 <= 0xFC)) c2 = 0xB1;  /* form, float, integer */
				else if (c2 >= 0xA6 && c2 <= 0xA8) {  /* array */
					if (c2 == 0xA6 + (UCHAR)((c1 & 0x03) - 1)) c2 = c1;
				}
				else if (c2 == 0xAA) c2 = 0xBD;  /* object */
				else if (c2 == 0xAB) c2 = 0xB8;  /* image */
				else if (c2 == 0xAD) c2 = 0xBA;  /* resource */
				else if (c2 == 0xAE) c2 = 0xBB;  /* device */
				else return(1);
				if (c1 != c2) return(1);
			}
			adr[1] = 0;
			adr[2] = 0;
			adr[3] = (UCHAR) globalptr->offset;
			adr[4] = (UCHAR)(globalptr->offset >> 8);
			adr[5] = (UCHAR)(globalptr->offset >> 16);
			chkavar(adr, TRUE);
			return(0);
		}
		if (!(flag & (MAKEVAR_VARIABLE | MAKEVAR_ADDRESS | MAKEVAR_PFILE))) {
			memset(&adr[1], 0xFF, 5);
			return(2);
		}
	}

	if (flag & MAKEVAR_GLOBAL) datamod = dataxmod = 0;
	else {
		dataxmod = dataxmodule;
		datamod = datamodule;
	}

	/* create the variable */
	if (arrayx) i3 = arraysiz * size + 4 + (arrayx << 1);
	else i3 = size;
	i4 = datatab[dataxmod].size + i3 + 1;
	if (i4 > datatab[dataxmod].maxsize) {
		i4 = (i4 + 0x0400) & ~0x03FF;
		if (i4 >= 0x04000000) return(103);		/* 67,108,864 */
		i1 = memchange(datatab[dataxmod].dataptr, i4, 0);
		setpgmdata();
		if (i1) return(556);
		datatab[dataxmod].maxsize = i4;
	}

	/* create the global table entry */
	if (flag & MAKEVAR_NAME) {
		i1 = (INT) strlen(varname) + 1;
		globalpptr2 = (GLOBALINFO **) memalloc(sizeof(GLOBALINFO) - (sizeof(globalptr->name) - i1) * sizeof(UCHAR), 0);
		setpgmdata();
		if (globalpptr2 == NULL) return(556);

		globalptr = *globalpptr;
		globalptr->nextptr = globalpptr2;
		globalptr = *globalpptr2;
		globalptr->nextptr = globalpptr1;
		globalptr->offset = datatab[0].size;
		memcpy(globalptr->name, varname, i1);
	}

	/* reset adr because of memory allocations */
	if (flag & MAKEVAR_DATA) adr = *datatab[adrmod].dataptr + adroff;

	/* get pointer to new address */
	ptr = *datatab[dataxmod].dataptr;
	ptr += datatab[dataxmod].size;

	if (flag & (MAKEVAR_VARIABLE | MAKEVAR_PFILE)) {
		if ((flag & MAKEVAR_VARIABLE) && (varflg & (TYPE_CHAR | TYPE_NUM | TYPE_INT | TYPE_FLOAT))) {
			if (arrayx) {
				*ptr++ = (UCHAR)(0xA6 + arrayx - 1);
				for (i1 = 0; i1 < arrayx; i1++) {
					*ptr++ = (UCHAR) elements[i1];
					*ptr++ = (UCHAR)(elements[i1] >> 8);
				}
				*ptr++ = (UCHAR) size;
				*ptr++ = (UCHAR)(size >> 8);
			}
			do {
				if (varflg & TYPE_CHAR) {
					if (lft < 128) {
						*ptr++ = 0;
						*ptr++ = 0;
						*ptr++ = (UCHAR) lft;
					}
					else {
						*ptr++ = 0xF0;
						*ptr++ = 0;
						*ptr++ = 0;
						*ptr++ = 0;
						*ptr++ = 0;
						*ptr++ = (UCHAR) lft;
						*ptr++ = (UCHAR)(lft >> 8);
					}
					memset(ptr, ' ', lft);
					ptr += lft;
				}
				else if (varflg & TYPE_NUM) {
					*ptr++ = (UCHAR) 0x80 | (UCHAR)(size - 1);
					memset(ptr, ' ', lft);
					ptr += lft;
					if (!rgt) *(ptr - 1) = '0';
					else {
						*ptr++ = '.';
						memset(ptr, '0', rgt);
						ptr += rgt;
					}
				}
				else if (varflg & TYPE_INT) {
					*ptr++ = 0xFC;
					*ptr++ = (UCHAR) lft;
					x1 = 0;
					memcpy(ptr, (UCHAR *) &x1, sizeof(INT32));
					ptr += sizeof(INT32);
				}
				else {
					*ptr++ = (UCHAR)(0xF8 | (lft >> 3));
					*ptr++ = (UCHAR)((lft << 5) | rgt);
					y1 = 0;
					memcpy(ptr, (UCHAR *) &y1, sizeof(DOUBLE64));
					ptr += sizeof(DOUBLE64);
				}
				if (!arrayx) break;
			} while (--arraysiz);
			if (arrayx) *ptr++ = 0xA5;
		}
		else {  /* pfile, device, image, object, resource */
			if (flag & MAKEVAR_PFILE) c1 = 0xA9;
			else if (varflg == TYPE_DEVICE) c1 = 0xAE;
			else if (varflg == TYPE_IMAGE) {
				i1 = ctlimageopen(bits, horz, vert, datamod, datatab[dataxmod].size, &imgrefnum);
				setpgmdata();
				if (i1 == RC_INVALID_VALUE) return 1;
				if (i1) return 556;
				/* reset adr and ptr because of memory allocations */
				adr = *datatab[adrmod].dataptr + adroff;
				ptr = *datatab[dataxmod].dataptr;
				ptr += datatab[dataxmod].size;
				c1 = 0xAB;
			}
			else if (varflg == TYPE_RESOURCE) c1 = 0xAD;
			else c1 = 0xAA;  /* object */

			davb = aligndavb(ptr, &i1);
			if (i1) {
				memset(ptr, c1, i1);
#if 0
/*** CODE: COULD ADD THIS CODE IF WANT TO STORE ALIGNED ADDRESS ***/
				if (flag & MAKEVAR_NAME) globalptr->offset += i1;
				datatab[dataxmod].size += i1;
				i3 -= i1;
#endif
			}
			ptr += 44;
			memset((UCHAR *) davb, 0, sizeof(DAVB));
			memset((UCHAR *)(davb + 1), 0xF3, ptr - (UCHAR *)(davb + 1));
			davb->type = c1;
			if ((flag & MAKEVAR_VARIABLE) && varflg == TYPE_IMAGE) davb->refnum = imgrefnum;
		}

		adr++;
		*adr++ = (UCHAR) datamod;
		*adr++ = (UCHAR)(datamod >> 8);
	}
	else {  /* address */
		*ptr++ = 0xB0 + adr[1];
		memset(ptr, 0xFF, 5);
		ptr += 5;
		*adr++ = 0xCE;
	}
	*ptr = 0xF2;  /* end of data character */

	*adr++ = (UCHAR) datatab[dataxmod].size;
	*adr++ = (UCHAR)(datatab[dataxmod].size >> 8);
	*adr++ = (UCHAR)(datatab[dataxmod].size >> 16);
	datatab[dataxmod].size += i3;
	return(0);
}

CHAR *getmodules(INT flag)
{
	static INT dmod = -1;
	static INT pmod = INT_MAX;
	INT i1;
	UCHAR typeflg;

	if (!flag) {
		pmod = pgmxmodule;
		dmod = dataxmodule;
	}
	else if (dmod == -1) {
		while (pmod < pgmhi && (pgmtab[pmod].pgmmod == -1 || pmod == pgmxmodule)) pmod++;
		if (pmod >= pgmhi) return(NULL);
		dmod = pgmtab[pmod].dataxmod;
	}

	typeflg = pgmtab[pmod].typeflg;
	strcpy(name, *(*pgmtab[pmod].codeptr)->nameptr);
	i1 = (INT) strlen(name);
	if (typeflg == 1 || typeflg == 3) name[i1++] = '<';
	else if (typeflg == 4) name[i1++] = '!';
	if (datatab[dmod].nameptr != NULL) {
		strcpy(&name[i1], *datatab[dmod].nameptr);
		i1 = (INT) strlen(name);
	}
	if (typeflg == 1 || typeflg == 3) name[i1++] = '>';
	name[i1] = '\0';

	if (typeflg == 1 || typeflg == 3) dmod = datatab[dmod].nextmod;
	else dmod = -1;
	if (dmod == -1) {
		if (pmod == pgmxmodule) pmod = 0;
		else pmod++;
	}
	return(name);
}

void getlastvar(INT *mod, INT *off)
{
	if (adrmod == 0xFFFF) *mod = 0xFFFF;
	else *mod = datatab[adrmod].datamod;
	*off = adroff;
}

void setlastvar(INT mod, INT off)
{
	if ((mod = getdatax(mod)) == RC_ERROR) return;  /* data area not valid */
	adrmod = mod;
	adroff = off;
}

/**
 * Find a global in the global table by its zero-based index
 * Returns name and offset
 */
INT getglobal(INT num, CHAR **nameptr, INT *offset)
{
	INT i1 = 0;
	GLOBALINFO *globalptr = *globaltabptr;
	for (;;) {
		if (i1++ == num) {
			*nameptr = globalptr->name;
			*offset = globalptr->offset;
			return 0;
		}
		if (globalptr->nextptr == NULL) break;
		globalptr = *globalptr->nextptr;
	}
	return 1;
}

PGMMOD** getpgmtabptr() {
	return pgmtabptr;
}

DATAMOD** getdatatabptr() {
	return datatabptr;
}

INT getpgmmax() {
	return pgmmax;
}
