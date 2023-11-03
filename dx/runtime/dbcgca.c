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
#include "includes.h"
#include "base.h"
#include "fio.h"
#include "dbc.h"

#define CACHESTARTSIZE 16
#define CACHEGROWTHINCREMENT 16

static CHAR buffer[160];
static INT dtsav;
static INT inc[INCMAX],	/* Used as a stack for Source records */
		fnc[FNCMAX];	/* Used as a stack for Routine records */

static UCHAR **stabptr,	/* double pointer to Source table */
			**dtabptr,	/* double pointer to Data table */
			**ltabptr,	/* double pointer to Label table */
			**ptabptr,	/* double pointer to Program lines table */
			**rtabptr,	/* double pointer to Routines table */
			**ctabptr,	/* double pointer to Classes table */
			**ntabptr;	/* double pointer to Names table */

static SRCDEF	*stable;	/* single pointer to Source table */
static DATADEF	*dtable;	/* single pointer to Data table */
static LABELDEF	*ltable;	/* single pointer to Label table */
static PGMDEF	*ptable;	/* single pointer to Program lines table */
static RTNDEF	*rtable;	/* single pointer to Routines table */
static CLASSDEF	*ctable;	/* single pointer to Classes table */
static CHAR		*ntable;	/* single pointer to Names table */

static INT stabsize,	/* 'S'ource records seen */
		dtabsize,		/* 'D'ata records seen */
		ltabsize,		/* 'L'abel records seen */
		ptabsize,		/* 'P'rogram line records seen */
		rtabsize,		/* 'R'outine records seen */
		ctabsize,		/* 'C'lass records seen */
		ntabsize;		/* bytes used in the name table so far */
static INT realdsize;

typedef struct tag_cacheelement{
	CHAR **dbgname;		/* double pointer to a zstring, the name of the dbg file */
	UCHAR **stabptr;	/* double pointer to Source table */
	UCHAR **dtabptr;	/* double pointer to Data table */
	UCHAR **ltabptr;	/* double pointer to Label table */
	UCHAR **ptabptr;	/* double pointer to Program lines table */
	UCHAR **rtabptr;	/* double pointer to Routines table */
	UCHAR **ctabptr;	/* double pointer to Classes table */
	UCHAR **ntabptr;	/* double pointer to Names table */
	INT stabsize;		/* entries in the Source table */
	INT dtabsize;		/* non-class D records used, not the actual d table size */
	INT ltabsize;		/* entries in the Label table */
	INT ptabsize;		/* entries in the Program lines table */
	INT rtabsize;		/* entries in the Routine table */
	INT ctabsize;		/* entries in the Classes table */
	INT realdsize;		/* actual number of entries in the Data table */
} CACHEELEMENT, *PCACHEELEMENT;

static PCACHEELEMENT *cache;
static INT cachemax;			/* number of CACHEELEMENTS allocated */
static INT cachesize;			/* number of CACHEELEMENTS in use */

static INT initcache(void);
static INT loaddbg(CHAR *name, INT module);
static INT locateClass(INT module);
static INT processClassRecord(INT *clscnt, INT dtmax);
static void processDataRecord(void);
static void processLabelRecord(INT srccurrent);
static void processRoutineRecord(INT *rtncurrent, INT *rtnstacksize, INT clscnt);
static void processSourceRecord(INT *srccurrent, INT *srcstacksize);
static INT getCachedDbg(CHAR *name);
static INT cacheCurrentDbg(CHAR *name);
static CHAR* getlastsegment(CHAR *buf);
static CHAR* removevolid(CHAR *buf);

INT getdbg(INT module, DBGINFOSTRUCT *dbgfinfo) {
	INT i1, modtype;

	modtype = getmtype(module);
	strcpy(name, getpname(module));
	miofixname(name, ".dbg", FIXNAME_EXT_ADD);

	i1 = getCachedDbg(name);
	if (i1 == -1) {				/* getCachedDbg did not find a name match */
		i1 = loaddbg(name, module);
		if (!i1) i1 = cacheCurrentDbg(name);
		if (i1) return -1;
	}
	else if (i1 != 0) return -1;
	
	if (dbgfinfo != NULL) {
		if (modtype == MTYPE_CLASS) {
			if ((dbgfinfo->classnum = locateClass(module)) == -1) {
				/* locateClass called dbgerror */
				return -1;
			}
		}
		else dbgfinfo->classnum = -1;
		dbgfinfo->stabptr = stabptr;
		dbgfinfo->dtabptr = dtabptr;
		dbgfinfo->ltabptr = ltabptr;
		dbgfinfo->ptabptr = ptabptr;
		dbgfinfo->rtabptr = rtabptr;
		dbgfinfo->ctabptr = ctabptr;
		dbgfinfo->ntabptr = ntabptr;
		dbgfinfo->stabsize = stabsize;
		dbgfinfo->dtabsize = dtabsize;
		dbgfinfo->ltabsize = ltabsize;
		dbgfinfo->ptabsize = ptabsize;
		dbgfinfo->rtabsize = rtabsize;
		dbgfinfo->ctabsize = ctabsize;
		dbgfinfo->realdsize = realdsize;
	}
	return 0;
}

/**
 * Does a linear search through the cache elements matching on the dbg file name.
 * Returns zero if successful, -1 if name not found in cache, 
 * ERR_NOMEM if initcache fails
 */
static INT getCachedDbg(CHAR *name) {
	INT i1;
	if (cache == NULL && initcache()) return ERR_NOMEM;	/* dbgerror was called by initcache */
	for (i1 = 0; i1 < cachesize; i1++) {
		if (!strcmp(name, *(*cache)[i1].dbgname)) break;
	}
	if (i1 == cachesize) return -1;
	stabptr = (*cache)[i1].stabptr;
	dtabptr = (*cache)[i1].dtabptr;
	ltabptr = (*cache)[i1].ltabptr;
	ptabptr = (*cache)[i1].ptabptr;
	rtabptr = (*cache)[i1].rtabptr;
	ctabptr = (*cache)[i1].ctabptr;
	ntabptr = (*cache)[i1].ntabptr;
	stabsize = (*cache)[i1].stabsize;
	dtabsize = (*cache)[i1].dtabsize;
	ltabsize = (*cache)[i1].ltabsize;
	ptabsize = (*cache)[i1].ptabsize;
	rtabsize = (*cache)[i1].rtabsize;
	ctabsize = (*cache)[i1].ctabsize;
	realdsize = (*cache)[i1].realdsize;
	return 0;
}

/**
 * Stuff the current set of pointers into the next slot in the cache array.
 * 
 * Returns zero if successful, 
 * ERR_NOMEM if the memchange or memalloc calls fail
 */
static INT cacheCurrentDbg(CHAR *name) {
	if (cachesize == cachemax) {
		cachemax += CACHEGROWTHINCREMENT;
		if (memchange((UCHAR**) cache, cachemax * sizeof(CACHEELEMENT), 0) == -1) return ERR_NOMEM;
	}
	if (((*cache)[cachesize].dbgname = (CHAR **) memalloc((INT)strlen(name) + 1, 0)) == NULL) return ERR_NOMEM;
	strcpy(*(*cache)[cachesize].dbgname, name);
	(*cache)[cachesize].stabptr = stabptr;
	(*cache)[cachesize].dtabptr = dtabptr;
	(*cache)[cachesize].ltabptr = ltabptr;
	(*cache)[cachesize].ptabptr = ptabptr;
	(*cache)[cachesize].rtabptr = rtabptr;
	(*cache)[cachesize].ctabptr = ctabptr;
	(*cache)[cachesize].ntabptr = ntabptr;
	(*cache)[cachesize].stabsize = stabsize;
	(*cache)[cachesize].dtabsize = dtabsize;
	(*cache)[cachesize].ltabsize = ltabsize;
	(*cache)[cachesize].ptabsize = ptabsize;
	(*cache)[cachesize].rtabsize = rtabsize;
	(*cache)[cachesize].ctabsize = ctabsize;
	(*cache)[cachesize].realdsize = realdsize;
	cachesize++;
	return 0;
}

static INT loaddbg(CHAR* name, INT module) {

	INT handle;		/* handle to the open dbg file */
	INT htime, 		/* YYYYMMDD of the dbg file */
		ltime,		/* HHMMSSPP of the dbg file */
		dbchtime, 	/* YYYYMMDD of the dbc file */
		dbcltime;	/* HHMMSSPP of the dbc file */
	INT stmax,		/* 'S'ource records in this dbg file */
		dtmax,		/* 'D'ata records in this dbg file */
		ltmax,		/* 'L'abel records in this dbg file */
		ptmax,		/* 'P'rogram line records in this dbg file */
		rtmax,		/* 'R'outine records in this dbg file */
		ctmax,		/* 'C'lass records in this dbg file */
		ntmax;		/* bytes required for the name table */

	INT i1, i2, i3, clscnt, rtncurrent, rtnstacksize, srccurrent, srcstacksize;

	handle = rioopen(name, RIO_M_SRO	/* Shared Read Only */
						| RIO_P_DBC		/* Use the DBC Path */
						| RIO_T_STD,	/* DB/C standard record type file */
						3,				/* number of buffers */
						80);			/* max record length */
	if (handle < 0) {
		strcpy(buffer, "Unable to open ");
		strcat(buffer, name);
		dbgerror(ERROR_FAILURE, buffer);
		return -1;
	}
	/* reset memory, rioopen can move it */
	setpgmdata();
	dbgsetmem();

	/* get the header record */
	i1 = rioget(handle, (UCHAR *) buffer, 160);
	if (i1 < 0) {
		dbgerror(ERROR_FAILURE, "Error reading .dbg file");
		goto loaddbg2;
	}

	/* do we recognize the dbg file version ? */
	if (buffer[0] != '8' && buffer[0] != '9') {
		dbgerror(ERROR_FAILURE, "The .dbg file is not version 8 or 9");
		goto loaddbg2;
	}
	/* consistency check */
	if ((buffer[0] == '8' && i1 != 48) || (buffer[0] == '9' && i1 != 54)) {
		dbgerror(ERROR_FAILURE, "The .dbg file has an invalid header record");
		goto loaddbg2;
	}
	/* do the timestamps match? */
	mscntoi((UCHAR *) &buffer[2], &htime, 8);
	mscntoi((UCHAR *) &buffer[10], &ltime, 8);
	getmtime(module, &dbchtime, &dbcltime);
	if (dbcltime != ltime || dbchtime != htime) {
		dbgerror(ERROR_FAILURE, "The .dbc file and the .dbg file do not match");
		goto loaddbg2;
	}
	
	/* Get counters for all the dbg record types */
	mscntoi((UCHAR *) &buffer[18], &stmax, 6);
	mscntoi((UCHAR *) &buffer[24], &dtmax, 6);
	mscntoi((UCHAR *) &buffer[30], &ltmax, 6);
	mscntoi((UCHAR *) &buffer[36], &ptmax, 6);
	mscntoi((UCHAR *) &buffer[42], &rtmax, 6);
	if (buffer[0] == '9') mscntoi((UCHAR *) &buffer[48], &ctmax, 6);
	else ctmax = 0;
	ntmax = stmax * MAXSNAME + dtmax * MAXDLABEL + ltmax * MAXPLABEL + rtmax * MAXPLABEL + ctmax * MAXPLABEL;
	if (ntmax < 1024) ntmax = 1024;
	realdsize = dtmax;	/* save this for debugging the debugger */

	/* grab some ram */
	stabptr = memalloc(stmax * sizeof(SRCDEF), 0);
	dtabptr = memalloc(dtmax * sizeof(DATADEF), 0);
	ltabptr = memalloc(ltmax * sizeof(LABELDEF), 0);
	ptabptr = memalloc((ptmax + 1) * sizeof(PGMDEF), 0);
	rtabptr = memalloc(rtmax * sizeof(RTNDEF), 0);
	ctabptr = memalloc(ctmax * sizeof(CLASSDEF), 0);
	do {
		ntabptr = memalloc(ntmax, 0);
		if (ntabptr != NULL) break;
		ntmax -= 1024;
	} while (ntmax >= 1024);
	/* reset memory */
	setpgmdata();
	dbgsetmem();
	if (stabptr == NULL || dtabptr == NULL || ltabptr == NULL || ptabptr == NULL || rtabptr == NULL
		|| ctabptr == NULL || ntabptr == NULL) {
		dbgerror(ERROR_FAILURE, "Not enough memory for the debugger");
		goto loaddbg1;
	}

	stabsize = dtabsize = ltabsize = ptabsize = rtabsize = ctabsize = ntabsize = 0;
	rtnstacksize = srcstacksize = rtncurrent = srccurrent = 0;
	clscnt = -1;

	/* dereference for convenience */
	stable = (SRCDEF *) *stabptr;
	dtable = (DATADEF *) *dtabptr;
	ltable = (LABELDEF *) *ltabptr;
	ptable = (PGMDEF *) *ptabptr;
	rtable = (RTNDEF *) *rtabptr;
	ctable = (CLASSDEF *) *ctabptr;
	ntable = (CHAR *) *ntabptr;

	for ( ; ; ) {
		i1 = rioget(handle, (UCHAR *) buffer, sizeof(buffer));
		if (i1 == -1) break;
		if (i1 < 1) {
			dbgerror(ERROR_FAILURE, "Error reading .dbg file");
			goto loaddbg1;
		}
		if (ntabsize > ntmax - 34) {
			dbgerror(ERROR_FAILURE, "Not enough memory for the name table");
			goto loaddbg1;
		}
		buffer[i1] = 0;
		switch (buffer[0]) {
			case 'C':
				if (ctabsize == ctmax) {
					dbgerror(ERROR_FAILURE, "Invalid .dbg, too many 'C' records");
					goto loaddbg1;
				}
				if (processClassRecord(&clscnt, dtmax)) goto loaddbg1;
				break;
			case 'D':
				if (dtabsize == dtmax) {
					dbgerror(ERROR_FAILURE, "Invalid .dbg, too many 'D' records");
					goto loaddbg1;
				}
				processDataRecord();
				break;
			case 'E':	/* end of a source file */
				stable[srccurrent].linecnt = (int) strtol(&buffer[1], NULL, 10);
				if (srcstacksize) srccurrent = inc[--srcstacksize];	/* pop the 'stack' */
				break;
			case 'L':
				if (ltabsize == ltmax) {
					dbgerror(ERROR_FAILURE, "Invalid .dbg, too many 'L' records");
					goto loaddbg1;
				}
				processLabelRecord(srccurrent);
				break;
			case 'P':
				if (ptabsize == ptmax) {
					dbgerror(ERROR_FAILURE, "Invalid .dbg, too many 'P' records");
					goto loaddbg1;
				}
				ptable[ptabsize].stidx = srccurrent;
				ptable[ptabsize].pcnt = (int) strtol(&buffer[7], NULL, 10);
				buffer[7] = 0;
				ptable[ptabsize++].linenum = (int) strtol(&buffer[1], NULL, 10) - 1;
				break;
			case 'R':
				if (rtabsize == rtmax) {
					dbgerror(ERROR_FAILURE, "Invalid .dbg, too many 'R' records");
					goto loaddbg1;
				}
				if (rtnstacksize == FNCMAX) {
					dbgerror(ERROR_FAILURE, "Too many Routine statements w/o endroutine");
					goto loaddbg1;
				}
				processRoutineRecord(&rtncurrent, &rtnstacksize, clscnt);
				break;
			case 'S':
				if (stabsize == stmax || srcstacksize == INCMAX) {
					dbgerror(ERROR_FAILURE, "Invalid .dbg, too many 'S' records");
					goto loaddbg1;
				}
				processSourceRecord(&srccurrent, &srcstacksize);
				break;
			case 'X':	/* end of routine scope */
				rtable[rtncurrent].dtcnt = dtabsize - rtable[rtncurrent].dtidx;
				rtable[rtncurrent].epcnt = ptabsize;
				if (rtnstacksize) rtncurrent = fnc[--rtnstacksize];
				break;
			case 'Y':	/* end of class and any embedded routines */
				ctable[ctabsize].epcnt = ptabsize;	/* end pcount */
				dtmax = ctable[ctabsize++].dtidx;	/* kludge */
				dtabsize = dtsav;
				clscnt = -1;
				break;
			default:
				dbgerror(ERROR_FAILURE, "Invalid .dbg, incorrect record header");
				goto loaddbg1;
		}
	}
	/* make last program record point to end of file */
	ptable[ptabsize].stidx = 0;
	ptable[ptabsize].linenum = 0x70000000;

	rioclose(handle);
	
	/* fix up pcount offsets for classes */
	for (i1 = 0; i1 < ctabsize; i1++) {
		ctable[i1].spcnt = ptable[ctable[i1].spcnt].pcnt;
		i2 = ctable[i1].epcnt;
		if (i2 && i2 < ptabsize) ctable[i1].epcnt = ptable[i2].pcnt;
		else ctable[i1].epcnt = 16 << 20;
	}

	/* fix up routine table */

	/* Step one, for all routines that did *not* have an endroutine, calculate the data count */	
	for (i1 = 0; i1 < rtabsize; i1++) {
		if (rtable[i1].dtcnt == -1) rtable[i1].dtcnt = dtabsize - rtable[i1].dtidx;
	}
	/* Step two, any routine table entry that has zero data entries is discarded */
	for (i1 = i2 = 0; i1 < rtabsize; i1++) {
		if (!rtable[i1].dtcnt) continue;
		if (i1 != i2) memcpy(&rtable[i2], &rtable[i1], sizeof(RTNDEF));
		i2++;
	}
	rtabsize = i2;
	/* Step three, fix up the data table to point to the correct routine table entry */
	for (i1 = 0; i1 < rtabsize; i1++) {
		i2 = rtable[i1].dtidx;
		i3 = i2 + rtable[i1].dtcnt;
		while (i2 < i3) dtable[i2++].rtidx = i1;
	}
	/* Step four, fix up the pcount start and end positions */
	for (i1 = 0; i1 < rtabsize; i1++) {
		rtable[i1].spcnt = ptable[rtable[i1].spcnt].pcnt;
		i2 = rtable[i1].epcnt;
		if (i2 && i2 < ptabsize) rtable[i1].epcnt = ptable[i2].pcnt;
		else rtable[i1].epcnt = 16 << 20;
	}
		
	memchange(rtabptr, rtabsize * sizeof(RTNDEF), 0);
	memchange(ntabptr, ntabsize, 0);
	/* reset memory */
	setpgmdata();
	dbgsetmem();
	return 0;

loaddbg1:
	memfree(stabptr);
	memfree(dtabptr);
	memfree(ltabptr);
	memfree(ptabptr);
	memfree(rtabptr);
	memfree(ctabptr);
	memfree(ntabptr);
	
loaddbg2:
	rioclose(handle);
	return -1;
}

static INT processClassRecord(INT *clscnt, INT dtmax) {
	INT drecords;
	ctable[ctabsize].nptr = ntabsize;
	strcpy(&ntable[ntabsize], &buffer[7]);
	ntabsize += (INT)strlen(&buffer[7]) + 1;
	mscntoi((UCHAR *) &buffer[1], &drecords, 6);	/* count of D records in this class */
	if (drecords > dtmax - dtabsize) {
		dbgerror(ERROR_FAILURE, "Invalid .dbg, in 'C' record, bad D count");
		return -1;
	}
	dtsav = dtabsize;
	dtabsize = dtmax - drecords;
	ctable[ctabsize].dtidx = dtabsize;
	ctable[ctabsize].dtcnt = drecords;
	ctable[ctabsize].spcnt = ptabsize;	/* start pcount */
	*clscnt = ctabsize;
	return 0;
}

static void processDataRecord() {
	dtable[dtabsize].nptr = ntabsize;
	strcpy(&ntable[ntabsize], &buffer[8]);
	ntabsize += (INT)strlen(&buffer[8]) + 1;
	buffer[8] = 0;
	dtable[dtabsize].offset = (int) strtol(&buffer[1], NULL, 10);
	dtable[dtabsize++].rtidx = -1;
}

static void processLabelRecord(INT srccurrent) {
	ltable[ltabsize].stidx = srccurrent;
	ltable[ltabsize].nptr = ntabsize;
	strcpy(&ntable[ntabsize], &buffer[7]);
	ntabsize += (INT)strlen(&buffer[7]) + 1;
	buffer[7] = 0;
	ltable[ltabsize++].linenum = (int) strtol(&buffer[1], NULL, 10) - 1;
}

static void processRoutineRecord(INT *rtncurrent, INT *rtnstacksize, INT clscnt) {
	rtable[rtabsize].nptr = ntabsize;
	strcpy(&ntable[ntabsize], &buffer[1]);
	ntabsize += (INT)strlen(&buffer[1]) + 1;
	rtable[rtabsize].dtidx = dtabsize;
	rtable[rtabsize].dtcnt = -1;
	rtable[rtabsize].spcnt = ptabsize;
	rtable[rtabsize].epcnt = 0;
	rtable[rtabsize].class = clscnt;
	fnc[(*rtnstacksize)++] = *rtncurrent;
	*rtncurrent = rtabsize++;
}

static void processSourceRecord(INT *srccurrent, INT *srcstacksize) {
	stable[stabsize].nametableptr = ntabsize;
	if (dbgflags & DBGFLAGS_DDT) {
		strcpy(&ntable[ntabsize], getlastsegment(removevolid(&buffer[8])));
		ntabsize += (INT)strlen(&ntable[ntabsize]) + 1;
	}
	else {
		miofixname(&buffer[8], ".txt", FIXNAME_EXT_ADD);
		strcpy(&ntable[ntabsize], &buffer[8]);
		ntabsize += (INT)strlen(&buffer[8]) + 1;
	}
	buffer[8] = 0;
	stable[stabsize].size = (int) strtol(&buffer[1], NULL, 10);
	inc[(*srcstacksize)++] = *srccurrent;		/* used to deal with nested source (i.e. includes) */
	*srccurrent = stabsize++;
}

/**
 * Return -1 if the class name was not found in the name table.
 * Return a non-negative number if successful, the index into the class table.
 */
static INT locateClass(INT module) {
	CHAR *ptr;
	INT i1;
	ptr = getdname(getdatamod(module));
	ctable = (CLASSDEF *) *ctabptr;
	ntable = (CHAR *) *ntabptr;
	for (i1 = 0; i1 < ctabsize && strcmp(ptr, &ntable[ctable[i1].nptr]); i1++);
	if (i1 == ctabsize) {
		strcpy(buffer, "Unable to locate information for class ");
		strcat(buffer, ptr);
		dbgerror(ERROR_FAILURE, buffer);
#if defined(DBCGCA_DEBUG) && DBCGCA_DEBUG > 2
		tableDump();
#endif
		return -1;
	}
	return i1;
}

/**
 * Return zero if successful, ERR_NOMEM if could not get memory
 */
static INT initcache() {
	cachemax = CACHESTARTSIZE;
	cache = (PCACHEELEMENT *) memalloc(cachemax * sizeof(CACHEELEMENT), 0);
	if (cache == NULL) {
		dbgerror(ERROR_FAILURE, "Not enough memory for the debugger");
		return ERR_NOMEM;
	}
	cachesize = 0;
	return 0;
}

static CHAR* getlastsegment(CHAR *buf) {
#if OS_WIN32
	CHAR *p = strrchr(buf, '\\');
#else
	CHAR *p = strrchr(buf, '/');
#endif
	if (p == NULL) return buf;
	return ++p;
}

static CHAR* removevolid(CHAR *buf) {
	CHAR *p = strrchr(buf, ':');
	if (p != NULL && p != &buf[1]) *p = '\0';
	return buf;
}
