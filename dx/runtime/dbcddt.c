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
#define INC_STDLIB
#define INC_STRING
#define INC_CTYPE
#include "includes.h"
#include "base.h"
#include "dbc.h"
#include "evt.h"
#include "fio.h"
#include "tcp.h"
#include "release.h"

#define WATCHMAX 32
#define SERVER_TIMEOUT 120

#define BRKVAL_EQ 1
#define BRKVAL_NE 2
#define BRKVAL_LE 3
#define BRKVAL_GE 4
#define BRKVAL_LT 5
#define BRKVAL_GT 6
#define BRKVAL_PB 7

#define ADDBREAKPOINT "addbreakpoint"
#define ADDCOUNTERBREAKPOINT "addCounterBreakpoint"
#define REMOVECOUNTERBREAKPOINT "removeCounterBreakpoint"

#if OS_UNIX && defined(PTHREADS)
#include <pthread.h>
int				xmlAvailable;
pthread_mutex_t	xmlAvailable_mutex;
pthread_cond_t	xmlAvailable_cond;
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#define MAX_VLTYPECODE_FILTERS 10
typedef struct _vlfilters {
	int typecodefiltercount;
	int typecodes[MAX_VLTYPECODE_FILTERS];
	int globalOnly;
	/**
	 * If occupied, then limit variables to the following routine only.
	 * If not occupied, then exclude all variables inside of routines.
	 */
	CHAR routine[32];
} VLFILTERS;

/**
 * Used to resolve the address of a particular element of an array
 */
typedef struct arrayinfo_tag {
	UCHAR *adr;			/* address of the 1st element of the array */
	INT datamod;		/* data module number */
	CHAR ndxtype[3];	/* 'C' if constant, 'V' if variable, '\0' if not used */
	INT array[3];		/* If above is 'C' then this is the decimal index.
						 * If above is 'V' then this is offset into data area
						 * of the numeric variable to be used as the index
						 */
} ARRAYINFO;

typedef struct variableinfo_tag {
	INT datamod;		/* data module number */
	INT channel;
	CHAR module[32];
	CHAR instance[32];
	CHAR varname[32];
	CHAR rtname[32];		/* Routine name, if any */
	INT arraycnt;			/* Number of dimensions in an array */
	INT array[3];			/* Used if array element indexed by constants */
	UCHAR *address;
	CHAR *value;			/* New value for variable from Eclipse */
	INT op;					/* operation code for break-on-value [eq|ne|ge|le|gt|lt|pb] */
	CHAR class[32];			/* Class name, if any */
	CHAR varindex[3][32];	/* Used if array element indexed by variables, name of variable in same module */
	CHAR ndxtype[3];		/* '\0' if unused, C if constant, V if variable */
} VARINFO, *PVARINFO;

static VARINFO variableinfo;

typedef struct watchtable_tag {
	CHAR **moduleptr;	/* module name */
	CHAR **instanceptr;	/* instance name */
	CHAR **nameptr;		/* variable name */
	CHAR **rtnptr;		/* routine name */
	CHAR **clsptr;		/* class name */
	INT datamod;		/* data module number */
	INT offset;			/* offset of variable in the data area */
	INT arraycnt;		/* zero if this is not an array element, otherwise number of dimensions */

	/* For an array element being watched, each of the potentially three indices can
	 * be either a constant or a variable. If a constant, array[x] is the constant
	 * value. If a variable, array[x] is the offset of the variable in the
	 * same datamod.
	 */
	INT array[3];

	/*
	 * Used if array element indexed by variables, name of variable in same module.
	 * We need to keep this info around for the 'remove' function to work.
	 */
	CHAR varindex[3][32];

	CHAR ndxtype[3];	/* '\0' if unused, C if constant, V if variable */

} WATCHTABLE;

static WATCHTABLE watchtable[WATCHMAX];

static struct ddtbrkval_tag {
	INT datamod;		/* data module number */
	INT offset;			/* offset of variable in the data area */
	INT op;				/* boolean operation code */
	INT arraycnt;		/* zero if no array info provided */
	INT array[3];		/* array subscripts, if provided */
	CHAR **value;		/* value to test for */
} ddtbrkvaltable[WATCHMAX];

static INT brkhi;
static INT brkptr = -1;
static INT callstackvalid;
static CHAR dbgwork[256];
static INT ddtbrkvalhi;
static INT ddtconnected;
static INT ddtevent;
static SOCKET ddtsocket;
static INT watchvarhi;
static INT stopline;
static INT stopfile;
static PGMMOD **pgmtabptr;
static INT savecscount;			/* Number of entries on call stack, last time we checked */
static DATAMOD **datatabptr;

static void addValueDescAttrs(INT chan);
static INT addvartowatchtable(void);
static INT changevarvalue(void);
static int checkForClass(PVARINFO varinfo, int *dtstart, int *dtstop);
static int compareChar(UCHAR *str1, CHAR ** str2);
static int ddtcmpdbl(UCHAR *num1, CHAR **num2);
static int ddtcmpform(UCHAR *num1, CHAR ** num2);
static int ddtcmpint(UCHAR *num1, CHAR **num2);
static int ddtcmpnum(UCHAR *num1, CHAR ** num2);
static void ddtdoaltervar(void) ;
static int ddtdobreakonvalue(void);
static int ddtfindmod(CHAR *modname, CHAR *instance, CHAR *class, INT *pmod, INT *dmod);
static int ddtfindvarnum(PVARINFO variableinfo, INT *varnum);
static CHAR* ddtgetvardesc(PVARINFO varinfo, INT *);
static void processAddBp(INT chan);
static void processRemovebp(INT chan);
static void ddtremovebrkval(int slot);
static UCHAR* ddtresolveadrvar(UCHAR *adr);
static UCHAR* ddtresolvearray(ARRAYINFO *arrayinfo);
static int findVarnumByOffset(PVARINFO varinfo, INT *varnum);
static void getArrayType(UCHAR *adr, INT *typecode);
static int getInstanceDataNumber(INT pmodnum, CHAR *instance);
static int getModuleNumber(CHAR *name);
static PGMMOD* getModuleStruct(INT pmodnum);
static INT getNumericVariableValue(UCHAR *adr);
static CHAR* getVariableValue(PVARINFO varinfo);
static void invalidateCallStack(void);
static int isAdrVar(UCHAR *adr);
static INT isCallStackValid(void);
static void processaddCounterbp(INT channel);
static void processaddwatch(INT channel);
static void processalter(INT chan);
static void processDisplay(INT channel);
static void processIsLineExecutable(INT chan);
static void processListElements(INT channel);
static void processModlist(INT chan);
static void processRemoveCounterbp(INT chan);
static void processRemoveWatch(INT chan);
static void processvariableelement(void);
static INT removeVarFromWatchTable(void);
static void sendDisplay(void);
static void sendVarList(INT chan, CHAR* module, CHAR* class, VLFILTERS* filters);
static void sendCallStackComplete(void);
static void sendVariableElementForVarlist(INT chan, UCHAR *dataadr, INT dtstart, INT dtstop, VLFILTERS* filters);
static void sendwatchedvars(INT chan);
static INT setvartablevaluetocurrent(INT varnum, UCHAR *varaddress);
static void setStopFileLine(INT op) ;
static void testValue(void);
static void validateCallStack(void);
static void waitForMessage(void);
static void writemodnametoxml(INT chan, CHAR *mname);

/**
 *  assume for now that if we are not SINGLE then we are running
 */
static void ddtcallback(INT what)
{
	if (dbgflags & DBGFLAGS_SINGLE){
#if OS_UNIX && defined(PTHREADS)
		pthread_mutex_lock(&xmlAvailable_mutex);
		xmlAvailable = 1;
		pthread_cond_signal(&xmlAvailable_cond);
		pthread_mutex_unlock(&xmlAvailable_mutex);
#else
		evtset(ddtevent);
#endif
	}
	else dbgaction(TRUE);
}

/**
 * -z=<ipaddr>:<port>
 */
void ddtinit(CHAR *fname)
{
#if OS_UNIX && !defined(PTHREADS)
	setddtconnected(FALSE);
#else
	if ((ddtsocket = dbctcxconnect(1, fname, ddtcallback)) == INVALID_SOCKET) {
		dbcdeath(95);
	}
	dbctcxputtag(1, "hello");
	dbctcxputattr(1, "version", RELEASE);
	dbctcxput(1);
	setddtconnected(TRUE);
#endif
}

/**
 * -zs=<port>
 */
void ddtinitserver(CHAR *port)
{
#if OS_UNIX && !defined(PTHREADS)
	setddtconnected(FALSE);
#else
	if ((ddtsocket = dbctcxconnectserver(1, port, ddtcallback, SERVER_TIMEOUT)) == INVALID_SOCKET) {
		dbcdeath(95);
	}
	dbctcxputtag(1, "hello");
	dbctcxputattr(1, "version", RELEASE);
	dbctcxput(1);
	setddtconnected(TRUE);
#endif
}

void convertCounterBPToRealBP(INT channel, int indexToCounterBPTable) {
	INT i1;
	INT ptidx = dbgCounterbrk[indexToCounterBPTable].ptidx;
	// Find free slot
	if (brkhi != 0) {
		for (i1 = 0; i1 < brkhi; i1++) {
			if (dbgbrk[i1].ptidx == ptidx) {
				dbctcxputattr(channel, "e", "duplicate bp");
				return;
			}
		}
		for (i1 = 0; i1 < brkhi; i1++) {
			if (dbgbrk[i1].mod == -1) break;
		}
		if (i1 == brkhi) {
			if (brkhi + 1 > DEBUG_BRKMAX) {
				sprintf(dbgwork, "Max of %d breakpoints exceeded", DEBUG_BRKMAX);
				dbctcxputattr(channel, "e", dbgwork);
				return;
			}
			brkhi++;
		}
	}
	else {
		i1 = 0;
		brkhi = 1;
	}

	dbgbrk[i1].mod = dbgCounterbrk[indexToCounterBPTable].mod;
	dbgbrk[i1].ptidx = ptidx;
	dbgbrk[i1].pcnt = dbgCounterbrk[indexToCounterBPTable].pcnt;
	dbgbrk[i1].verb = dbgCounterbrk[indexToCounterBPTable].verb;
	dbgbrk[i1].type = BREAK_PERMANENT;

	dbgCounterbrk[indexToCounterBPTable].mod = -1;
	dbgCounterbrk[indexToCounterBPTable].ptidx = -1;
	dbgCounterbrk[indexToCounterBPTable].pcnt = -1;
}

/*
 * given a character string of at least 5 bytes,
 * return a C string like flagsave
 */
static CHAR* getFlags(CHAR* fs) {
	fs[0] = (dbcflags & DBCFLAG_EQUAL) ? '1' : '0';
	fs[1] = (dbcflags & DBCFLAG_LESS) ? '1' : '0';
	fs[2] = (dbcflags & DBCFLAG_OVER) ? '1' : '0';
	fs[3] = (dbcflags & DBCFLAG_EOS) ? '1' : '0';
	fs[4] = '\0';
	return fs;
}

/**
 * Source, line, pcount, module
 * and maybe instance and class
 */
static void sendCallStackComplete() {
	CHAR apcount[16], *mname, line[16];
	CHAR name[MAX_NAMESIZE];
	INT rscount = returnstackcount();
	INT pcount, i1, i2, modnum, datanum;
	DBGINFOSTRUCT dbginfo;
	PGMDEF *ptable;
	SRCDEF *stable;
	CHAR *ntable;

	dbctcxputtag(1, "cs");				/* Call Stack */
	for (i1 = 0; i1 < rscount; i1++) {
		pcount = returnstackpcount(i1);
		dbctcxputsub(1, "f");			/* frame */
		modnum = returnstackpgm(i1);
		datanum = returnstackdata(i1);
		mname = getmname(modnum, datanum, TRUE);
		writemodnametoxml(1, mname);
		mscitoa(pcount, apcount);
		dbctcxputattr(1, "p", apcount);
		getdbg(modnum, &dbginfo);
		if (dbgflags & DBGFLAGS_DBGOPEN) {
			ptable = (PGMDEF*) *dbginfo.ptabptr;
			stable = (SRCDEF*) *dbginfo.stabptr;
			ntable = (CHAR*) *dbginfo.ntabptr;
			for (i2 = 0; i2 < dbginfo.ptabsize && ptable[i2].pcnt < pcount; i2++);
			strcpy(name, &ntable[stable[ptable[i2].stidx].nametableptr]);
			dbctcxputattr(1, "source", name);
			mscitoa(ptable[i2].linenum, line);
			dbctcxputattr(1, "line", line);
		}
	}
	dbctcxput(1);
	savecscount = rscount - 1;
	validateCallStack();
	getdbg(pgmmodule, NULL);
}

/**
 * Send a <stopped> message to the DDT server.
 * Assumes that stopline, stopfile and dbgpcount are set.
 */
static void sendstopmessage(CHAR *msg)
{
	CHAR name[MAX_NAMESIZE];
	CHAR *mname;
	CHAR line[16];
	CHAR flags[8];
	CHAR apcount [16];
	INT cscount;

	mname = getmname(pgmmodule, datamodule, TRUE);
	mscitoa(dbgpcount, apcount);
	dbctcxputtag(1, "stopped");

	writemodnametoxml(1, mname);

	dbctcxputattr(1, "p", apcount);
	/* We want to keep running in the face of a missing *.dbg
	 * so we will simply refrain from trying to send source file name and line info.
	 * We don't have it! */
	if (dbgflags & DBGFLAGS_DBGOPEN) {
		strcpy(name, &nametable[sourcetable[stopfile].nametableptr]);
		dbctcxputattr(1, "source", name);
		mscitoa(stopline, line);
		dbctcxputattr(1, "line", line);
	}
	if (msg != NULL) {
		dbctcxputattr(1, "msg", msg);
	}
	dbctcxputattr(1, "flags", getFlags(flags));
	cscount = returnstackcount();
	if (cscount != savecscount) {
		if (cscount < savecscount) dbctcxputattr(1, "cs", "d");		/* CallStack Decrement */
		else if (cscount > savecscount) dbctcxputattr(1, "cs", "i");/* CallStack Increment */
		savecscount = cscount;
	}
	dbctcxput(1);
	sendwatchedvars(1);
}

static void writemodnametoxml(INT chan, CHAR *mname) {
	CHAR *class, *instance;
	if ((instance = (CHAR*)strchr(mname, '<')) != NULL) {
		instance[0] = mname[(INT)strlen(mname) - 1] = '\0';
		dbctcxputattr(chan, "m", mname);
		if (strlen(++instance) > 0) dbctcxputattr(chan, "s", instance);
	}
	else if ((class = (CHAR*)strchr(mname, '!')) != NULL) {
		class[0] = '\0';	/* replace the ! with a null */
		dbctcxputattr(chan, "m", mname);
		class++;	/* point at 1st char after the ! */
		if ((instance = (CHAR*)strchr(class, '(')) != NULL) {
			instance[0] = class[strlen(class) - 1] = '\0';
		}
		if (strlen(class) > 0) dbctcxputattr(1, "c", class);
		if (strlen(++instance) > 0) dbctcxputattr(chan, "s", instance);
	}
	else dbctcxputattr(chan, "m", mname);
}

static void setStopFileLine(INT op) {
	INT i1;
	for (i1 = 0; i1 < ptsize && pgmtable[i1].pcnt < dbgpcount; i1++);
	if (i1 == ptsize) i1--;
	if (op == DEBUG_ERROR) i1--;
	stopline = pgmtable[i1].linenum;
	stopfile = srcfile = pgmtable[i1].stidx;;
}

/**
 * For the *.dbg file currently in memory,
 * search the data table and try to locate an entry by its offset (in address)
 * If found then varnum is set to the index into the data table.
 * If an error happens, dbgerrorstring will have a message.
 *
 * @param varnum OUT - The index we found the variable at, or unchanged if
 * 						we could not find it.
 *
 * @return 0 if success, 1 if error.
 */
static int findVarnumByOffset(PVARINFO varinfo, INT *varnum)
{
	int i1, dtstart, dtstop;
	if (varinfo->class[0] != '\0') {
		if (checkForClass(varinfo, &dtstart, &dtstop)) return 1;
	}
	else {
		dtstart = 0;
		dtstop = dtsize;
	}
	for (i1 = dtstart; i1 < dtstop; i1++) {
		if (varinfo->address != (UCHAR *)(ULONG_PTR)datatable[i1].offset) continue;
		*varnum = i1;
		break;
	}
	if (i1 == dtstop) {
		sprintf(dbgerrorstring, "Offset %p not found in module", varinfo->address);
		return 1;
	}
	return 0;
}

/**
 * For the *.dbg file currently in memory,
 * search the data table and try to locate variableinfo.varname.
 * If found then varnum is set to the index into the data table.
 * This search is currently hard wired to be case sensitive.
 * If an error happens, dbgerrorstring will have a message.
 *
 * @param varnum OUT - The index we found the variable at, or unchanged if
 * 						we could not find it.
 *
 * @return 0 if success, 1 if error.
 */
static int ddtfindvarnum(PVARINFO variableinfo, INT *varnum)
{
	int i1, rtidx, dtstart, dtstop;
	if (variableinfo->class[0] != '\0') {
		if (checkForClass(variableinfo, &dtstart, &dtstop)) return 1;
	}
	else {
		dtstart = 0;
		dtstop = dtsize;
	}
	for (i1 = dtstart; i1 < dtstop; i1++) {
		if (dbgstrcmp(variableinfo->varname, &nametable[datatable[i1].nptr], TRUE)) continue;
		if (variableinfo->rtname[0] != '\0') {
			if ((rtidx = datatable[i1].rtidx) == -1) continue;
			if (dbgstrcmp(variableinfo->rtname, &nametable[routinetable[rtidx].nptr], TRUE)) continue;
		}
		*varnum = i1;
		break;
	}
	if (i1 == dtstop) {
		strcpy(dbgerrorstring, variableinfo->varname);
		strcat(dbgerrorstring, " not found in that module");
		return 1;
	}
	return 0;
}

static int checkForClass(PVARINFO varinfo, int *dtstart, int *dtstop) {
	int i1;
	for (i1 = 0; i1 < ctsize; i1++) {
		if (!dbgstrcmp(varinfo->class, &nametable[classtable[i1].nptr], TRUE)) break;
	}
	if (i1 == ctsize) {
		strcpy(dbgerrorstring, "Class ");
		strcat(dbgerrorstring, varinfo->class);
		strcat(dbgerrorstring, " not found in that module");
		return 1;
	}
	*dtstart = classtable[i1].dtidx;
	*dtstop = *dtstart + classtable[i1].dtcnt;
	return 0;
}

/**
 * Process a request for a varlist
 */
static void processVarList(INT chan) {
	CHAR module[128], class[128];
	CHAR *attrtag, *attrval, *elementtag;
	VLFILTERS vlfilter;

	module[0] = class[0] = '\0';
	while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
		attrval = dbctcxgetattrvalue(chan);
		if (!strcmp(attrtag, "module")) strcpy(module, attrval);
		else if (!strcmp(attrtag, "c")) strcpy(class, attrval);
	}
	memset(&vlfilter, (int) 0, sizeof(VLFILTERS));
	while ((elementtag = dbctcxgetnextchild(chan)) != NULL) {
		if (!strcmp(elementtag, "filter")) {
			while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
				attrval = dbctcxgetattrvalue(chan);
				if (!strcmp(attrtag, "typecode") && vlfilter.typecodefiltercount < MAX_VLTYPECODE_FILTERS) {
					vlfilter.typecodes[vlfilter.typecodefiltercount++] = atoi(attrval);
				}
				else if (!strcmp(attrtag, "routine") && strlen(attrval) > 0) {
					strcpy(vlfilter.routine, attrval);
				}
				else if (!strcmp(attrtag, "globalonly") && !strcmp(attrval, "yes")) {
					vlfilter.globalOnly = TRUE;
				}
			}
		}
		else while (dbctcxgetnextattr(chan) != NULL) dbctcxgetattrvalue(chan);
	}
	if (module[0] != '\0') sendVarList(chan, module, class, &vlfilter);
}

/**
 * Send a <varlist...> message to the server.
 *
 * @param chan The xml channel, currently must be 1
 * @param module The module name to look up.
 * @param filters Any filters to apply.
 */
static void sendVarList(INT chan, CHAR* module, CHAR* class, VLFILTERS* filters)
{
	INT pmodnum, datanum;
	INT dtidx, dtcnt;
	INT i1;
	UCHAR *dataadr;

	dbctcxputtag(chan, "varlist");
	if ((pmodnum = getModuleNumber(module)) == -1) {
		sprintf(dbgerrorstring, "Could not find module '%s'", module);
		dbctcxputattr(chan, "e", dbgerrorstring);
	}
	else {
		if (dbgopendbg(pmodnum)) dbctcxputattr(chan, "e", dbgerrorstring);
		else {
			datanum = getInstanceDataNumber(pmodnum, NULL);
			if (datanum == -1) {
				if (dbgerrorstring[0] != '\0') dbctcxputattr(chan, "e", dbgerrorstring);
				else dbctcxputattr(chan, "e", "Cound not find a data module");
			}
			else {
				dataadr = *(*datatabptr)[datanum].dataptr;
				if (class == NULL || class[0] == '\0') sendVariableElementForVarlist(chan, dataadr, 0, dtsize, filters);
				else {
					for (i1 = 0; i1 < ctsize; i1++) {
						if (!strcmp(&nametable[classtable[i1].nptr], class)) break;
					}
					if (i1 == ctsize) {
						strcpy(dbgerrorstring, "Could not find dbg.clstable for ");
						strcat(dbgerrorstring, class);
						dbctcxputattr(chan, "e", dbgerrorstring);
					}
					else {
						dtidx = classtable[i1].dtidx;
						dtcnt = classtable[i1].dtcnt;
						sendVariableElementForVarlist(chan, dataadr, dtidx, dtidx + dtcnt, filters);
					}
				}
			}
		}
	}
	dbctcxput(chan);
}

/**
 * The filters are applied with an AND. They must all pass a variable before it is
 * allowed out of here.
 */
static void sendVariableElementForVarlist(INT chan, UCHAR *dataadr, INT dtstart, INT dtstop, VLFILTERS* filters) {
	INT varnum, rtidx, i1, applyfilters, f1;
	CHAR work[12];
	CHAR *routineName;

	applyfilters = filters->typecodefiltercount > 0 || filters->routine[0] != (CHAR)0 || filters->globalOnly;

	for (varnum = dtstart; varnum < dtstop; varnum++) {
		scanvar(dataadr + datatable[varnum].offset);
		rtidx = datatable[varnum].rtidx;
		if (rtidx != -1) routineName = &nametable[routinetable[rtidx].nptr];
		else routineName = NULL;
		if (applyfilters) {
			if (filters->typecodefiltercount > 0) {
				f1 = FALSE;
				for (i1 = 0; i1 < filters->typecodefiltercount; i1++) {
					if (filters->typecodes[i1] == vartype) {
						f1 = TRUE;
						break;
					}
				}
				if (!f1) continue;
			}
			if (filters->routine[0] != (CHAR)0) {
				if (routineName == NULL) f1 = TRUE;
				else f1 = !strcmp(routineName, filters->routine);
				if (!f1) continue;
			}
			if (filters->globalOnly) {
				if (routineName != NULL) continue;
			}
		}
		dbctcxputsub(chan, "variable");
		dbctcxputattr(chan, "name", &nametable[datatable[varnum].nptr]);
		if (routineName != NULL) dbctcxputattr(chan, "r", routineName);
		sprintf(work, "%i", vartype);
		dbctcxputattr(chan, "tc", work);
	}
}

static void doalterflags(INT chan) {
	CHAR *attrtag, *attrval;
	INT flag;
	while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
		attrval = dbctcxgetattrvalue(chan);
		if (!strcmp(attrtag, "z")) flag = DBCFLAG_EQUAL;
		else if (!strcmp(attrtag, "l")) flag = DBCFLAG_LESS;
		else if (!strcmp(attrtag, "o")) flag = DBCFLAG_OVER;
		else if (!strcmp(attrtag, "e")) flag = DBCFLAG_EOS;
		else flag = 0;
		if (!strcmp(attrval, "y")) dbcflags |= flag;
		else if (!strcmp(attrval, "n")) dbcflags &= ~flag;
	}
}

#if OS_WIN32
static DWORD WINAPI ddtreadthread(LPVOID lpParam)
#else
static void* ddtreadthread(void* arg)
#endif
{
	UCHAR buffer[512];
	int bufcnt;
	while(TRUE)
	{
		bufcnt = tcprecv(ddtsocket, buffer, 512, 0, -1);
		if (bufcnt == -1) dbcdeath(81);
		dbctcxprocessbytes(buffer, bufcnt);
	}
#if OS_WIN32
	return 0;
#else
#ifndef HP64
	return NULL;
#endif
#endif
}

void ddtdebug(INT op)
{
	INT i1;
	CHAR *tag;
	CHAR ework[64];
	UCHAR *pgmptr;
#if OS_WIN32
	DWORD threadid;
#else
#if OS_UNIX && defined(PTHREADS)
	pthread_t threadid;
	int rc;
#endif
#endif
	if (!isddtconnected()) return;
	dbgpcount = pcount;
	if (op == DEBUG_INTERRUPT) dbgflags |= DBGFLAGS_CTRL_D;
	if (op == DEBUG_DEBUG) {	/* breakpoint or debug instruction encountered */
		dbgpcount = pcount - 1; /* point at the actual debug statement */
		for (i1 = 0; i1 < brkhi; i1++) {
			if (dbgbrk[i1].pcnt == dbgpcount && (pgmptr = getmpgm(dbgbrk[i1].mod)) == pgm) {
				pgmptr[dbgpcount] = dbgbrk[i1].verb;
				dbgaction(TRUE); // set the ACTION_DEBUG flag
				dbgflags |= DBGFLAGS_SINGLE;
				pcount = dbgpcount; // back up the pcount to point at the now restored 1st byte of the actual verb
				brkptr = i1;
				invalidateCallStack();
				return;
			}
		}
		if (brkptr == brkhi) brkptr = -1;
		if (dbgflags & DBGFLAGS_SINGLE) {
			dbgaction(TRUE);
			return;
		}
		else {
			op = DEBUG_CHECK;		/* we want to fall into the following if */
			dbgflags |= DBGFLAGS_SINGLE;
		}
	}
	if (op == DEBUG_CHECK) {  /* check for something to do */
		if (dbgflags & DBGFLAGS_FIRST) {
			dbgflags &= ~DBGFLAGS_FIRST;
			pgmtabptr = getpgmtabptr();	/* This is coming from dbcldr */
			datatabptr = getdatatabptr(); /* ditto */
			events[0] = dbcgetbrkevt();
			if ((ddtevent = evtcreate()) < 0) /* input event */
			{
				dbctcxputtag(1, "terminate");
				dbctcxputattr(1, "e", "Debugger Initialization Error: unable to create events");
				dbctcxput(1);
				dbcflags &= ~DBCFLAG_DEBUG;
				return;
			}
			dbgflags |= DBGFLAGS_SINGLE;
#if OS_WIN32
			if (CreateThread(NULL, 0, ddtreadthread, NULL, 0, &threadid) == NULL) {
				sprintf(ework, "Debugger Initialization Error: unable to create thread:%i", (int) GetLastError());
#else
#if defined(PTHREADS)
			if ( (rc = pthread_create(&threadid, NULL, ddtreadthread, NULL)) ) {
				sprintf(ework, "Debugger Initialization Error: unable to create thread:%i", rc);
#else
			{
#endif
#endif
				dbctcxputtag(1, "terminate");
				dbctcxputattr(1, "e", ework);
				dbctcxput(1);
				dbcflags &= ~DBCFLAG_DEBUG;
				return;
			}
		}
		if (dbgflags & DBGFLAGS_CTRL_D) {
			dbgflags &= ~DBGFLAGS_CTRL_D;
			dbgflags |= DBGFLAGS_SINGLE;
		}
		if (dbgflags & DBGFLAGS_SETBRK) dbgsetbrk(&brkptr);
		if (dbgflags & DBGFLAGS_TSTCALL) dbgtestcall();
		if (dbgflags & DBGFLAGS_TSTRETN) dbgtestreturn();
		if (dbgflags & DBGFLAGS_TSTVALU) testValue();
		if (!(dbgflags & DBGFLAGS_SINGLE))
		{
			if (dbctcxget(1)) {  /* got an element from DDT */
				tag = dbctcxgettag(1);
				if (!strcmp(tag, "display")) processDisplay(1);
				else if (!strcmp(tag, "suspend")) {
					setpgmdata();
					dbgsetmem();
					dbgdatamod = datamodule;
					sendCallStackComplete();
					/* verify that we are in the same program module */
					/* pgmmodule is declared in dbc.h and defined in dbcsys.c */
					if (pgmmodule != dbgpgmmod) {
						if (!dbgopendbg(pgmmodule)) {
							setStopFileLine(op);
							sendstopmessage(NULL);
						}
						else sendstopmessage(dbgerrorstring);
					}
					else {
						setStopFileLine(op);
						sendstopmessage(NULL);
					}
					dbgflags |= DBGFLAGS_SINGLE;
				}
				else if (!strcmp(tag, "terminate")) {
					dbcexit(0);
				}
				else dbctcxflushincoming(1);
			}
		}
		else {
			if (!dbgopendbg(pgmmodule)) {
				setpgmdata();
				dbgsetmem();
			}
			else {
				sendstopmessage(dbgerrorstring);
			}
			if (dbgflags & DBGFLAGS_DBGOPEN) {
				for (i1 = 0; i1 < ptsize && pgmtable[i1].pcnt < dbgpcount; i1++);
				if (i1 < ptsize && pgmtable[i1].pcnt != dbgpcount) {
					dbgaction(TRUE);
					return;
				}
			}
			setStopFileLine(op);
			if (vx == 0xE5 || vbcode == VERB_DEBUG) sendCallStackComplete();		/* Pushreturn  or Debug */
			else if (!isCallStackValid()) sendCallStackComplete();
			sendstopmessage(NULL);
		}
		waitForMessage();
	}
	if (op == DEBUG_ERROR) {  /* untrapped error occurred */
		i1 = sizeof(dbgwork) - 1;
		geterrmsg((UCHAR *) dbgwork, &i1, FALSE);
		dbgwork[i1] = '\0';
		if (!dbgopendbg(pgmmodule)) {
			setpgmdata();
			dbgsetmem();
		}
		setStopFileLine(op);
		sendCallStackComplete();
		sendstopmessage(dbgwork);
		dbgflags &= ~(DBGFLAGS_FIRST);
		dbgflags |= DBGFLAGS_SINGLE;
		waitForMessage();
	}
	if (brkptr != -1) dbgflags |= DBGFLAGS_SETBRK;
	if (dbgflags & (DBGFLAGS_SINGLE | DBGFLAGS_TSTCALL | DBGFLAGS_TSTRETN | DBGFLAGS_SETBRK | DBGFLAGS_CTRL_D | DBGFLAGS_TSTVALU)) {
		dbgaction(TRUE);
	}
	else dbgaction(FALSE);
}

static void waitForMessage() {
	CHAR *tag;
	while (dbgflags & DBGFLAGS_SINGLE)
	{
		if (dbctcxget(1)) {  /* got an element from DDT */
			tag = dbctcxgettag(1);
			if (!strcmp(tag, ADDBREAKPOINT)) processAddBp(1);
			else if (!strcmp(tag, "addwatch")) processaddwatch(1);
			else if (!strcmp(tag, "alter")) processalter(1);
			else if (!strcmp(tag, "display")) processDisplay(1);
			else if (!strcmp(tag, "go")) {
				dbctcxputtag(1, "running");
				dbctcxput(1);
				dbgflags &= ~DBGFLAGS_SINGLE;
				break;
			}
			else if (!strcmp(tag, "modlist")) processModlist(1);
			else if (!strcmp(tag, "removebreakpoint")) processRemovebp(1);
			else if (!strcmp(tag, "removewatch")) processRemoveWatch(1);
			else if (!strcmp(tag, "so")) {
				saveretptr = returnstackcount();
				dbgflags |= DBGFLAGS_TSTCALL;
				dbgflags &= ~DBGFLAGS_SINGLE;
				break;
			}
			else if (!strcmp(tag, "ss")) {
				break;
			}
			else if (!strcmp(tag, "sr")) {
				saveretptr = returnstackcount();
				if (saveretptr) {
					saveretptr--;
					dbgflags |= DBGFLAGS_TSTRETN;
					dbgflags &= ~DBGFLAGS_SINGLE;
					break;
				}
			}
			else if (!strcmp(tag, "terminate")) {
				dbcexit(0);
			}
			else if (!strcmp(tag, "varlist")) processVarList(1);
			else if (!strcmp(tag, "listelements")) processListElements(1);
			else if (!strcmp(tag, "isLineExecutable")) processIsLineExecutable(1);
			else if (!strcmp(tag, ADDCOUNTERBREAKPOINT)) processaddCounterbp(1);
			else if (!strcmp(tag, REMOVECOUNTERBREAKPOINT)) processRemoveCounterbp(1);
			else {
				// Unrecognized message
				dbctcxputtag(1, tag);
				dbctcxputattr(1, "e", "Unrecognized message");
				dbctcxput(1);
			}
		}
		else {
#if OS_UNIX
#if defined(PTHREADS)
			pthread_mutex_lock(&xmlAvailable_mutex);
			while(!xmlAvailable)
				pthread_cond_wait(&xmlAvailable_cond, &xmlAvailable_mutex);
			xmlAvailable = 0;
			pthread_mutex_unlock(&xmlAvailable_mutex);
#endif
#else
			evtclear(ddtevent);
			evtwait(&ddtevent, 1);
#endif
		}
	}
}

static void ddtchangecharvalue(UCHAR *adr, CHAR* value) {
	int newvallen = (int) strlen(value);
	if (newvallen) {
 		fp = 1;
		lp = newvallen > pl ? pl : newvallen;
		strncpy((CHAR *)adr + hl, value, lp);
	}
	else fp = lp = 0;
	setfplp(adr);
}

static void ddtchangenumvalue(UCHAR *adr, CHAR* value) {
	UCHAR work[34], i1;
	i1 = (UCHAR)strlen(value);
	if (i1 > 31) i1 = 31;
	work[0] = 1;
	work[1] = work[2] = i1;
	memcpy(&work[3], value, i1);
	movevar(work, adr);	/* this is in dbcmov */
}

static int changevarvalue() {
	UCHAR *adr = variableinfo.address;
	INT i1;
	ARRAYINFO arrayinfo;
	if (isAdrVar(adr) && (adr = ddtresolveadrvar(adr)) == NULL) {
		dbgerror(ERROR_WARNING, "cannot alter unreferenced address var");
		return -1;
	}
	scanvar(adr);
	if (vartype == TYPE_ARRAY) {
		arrayinfo.adr = adr;
		for (i1 = 0; i1 < variableinfo.arraycnt; i1++) {
			arrayinfo.array[i1] = variableinfo.array[i1];
			arrayinfo.ndxtype[i1] = variableinfo.ndxtype[i1];
		}
		if ((adr = ddtresolvearray(&arrayinfo)) == NULL) {
			dbgerror(ERROR_WARNING, "cannot resolve array indexes");
			return -1;
		}
		if (isAdrVar(adr) && (adr = ddtresolveadrvar(adr)) == NULL) {
			dbgerror(ERROR_WARNING, "cannot alter unreferenced address var");
			return -1;
		}
		scanvar(adr);
	}
	if (vartype == TYPE_CHAR) ddtchangecharvalue(adr, variableinfo.value);
	else if (vartype & TYPE_NUMERIC) ddtchangenumvalue(adr, variableinfo.value);
	else {
		dbgerror(ERROR_WARNING, "Unsupported type for alter");
		return -1;
	}
	return 0;
}

static void ddtdoaltervar() {
	INT pmodnum, datanum, varnum = 0;
	processvariableelement();
	if (ddtfindmod(variableinfo.module, variableinfo.instance, variableinfo.class, &pmodnum, &datanum)) {
		dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
	}
	else {
		if (dbgopendbg(pmodnum)) dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
		else {
			if (ddtfindvarnum(&variableinfo, &varnum) == 1) dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
			else {
				if ((variableinfo.address = getmdata(datanum)) == NULL) {
					dbctcxputattr(variableinfo.channel, "e", "error loading data module");
				}
				else {
					variableinfo.address += datatable[varnum].offset;
					scanvar(variableinfo.address);	/* set fp, lp, pl, hl */
					if (changevarvalue())
						dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
				}
			}
		}
	}
}

/**
 * 'line' is expected to be zero-based
 */
static INT ddtfindline(CHAR *srcname, CHAR *line, INT *ptidx) {
	INT matchline, i1;
	matchline = (int) strtol(line, NULL, 10);
	for (i1 = 0; i1 < ptsize; i1++) {
		if (pgmtable[i1].linenum != matchline) continue;
		if (strcmp(srcname, &nametable[sourcetable[pgmtable[i1].stidx].nametableptr])) continue;
		*ptidx = i1;
		return 0;
	}
	sprintf(dbgwork, "could not find %s:%s", srcname, line);
	dbgerror(ERROR_WARNING, dbgwork);
	return 1;
}

static void doRemoveCounterBp(CHAR * module, CHAR *srcfilename, CHAR *line) {
	INT pmodnum, datanum/*, lineoffset*/, ptidx, i1;
	UCHAR *pgmptr;
	dbctcxputtag(variableinfo.channel, "removebreakpoint");
	if (counterBrkhi == 0) {
		dbctcxputattr(variableinfo.channel, "e", "There are no counter breakpoints defined.");
		goto doremovepointbp1;
	}
	if (ddtfindmod(module, NULL, variableinfo.class, &pmodnum, &datanum)) dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
	else {
		if (dbgopendbg(pmodnum)) dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
		else {
			if (ddtfindline(srcfilename, line, &ptidx)) dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
			else {
				pgmptr = getmpgm(pmodnum);
				if (pgmptr == NULL) {
					sprintf(dbgwork, "Bad module number:%i", pmodnum);
					dbctcxputattr(variableinfo.channel, "e", dbgwork);
				}
				else {
					for (i1 = 0; i1 < counterBrkhi; i1++) {
						if (dbgCounterbrk[i1].mod == -1) continue;
						if (dbgCounterbrk[i1].ptidx == ptidx) {
							//lineoffset = pgmtable[ptidx].pcnt;
							dbgCounterbrk[i1].mod = -1;
							dbgCounterbrk[i1].ptidx = -1;
							dbgCounterbrk[i1].pcnt = -1;
							//pgmptr[lineoffset] = dbgCounterbrk[i1].verb;
							break;
						}
					}
					if (i1 == counterBrkhi) dbctcxputattr(variableinfo.channel, "e", "Could not find that b.p.");
				}
			}
		}
	}
doremovepointbp1:
	dbctcxput(variableinfo.channel);
}

/**
 * Does not insert a DEBUG opcode, just puts info in a table for the dbciex loop to query
 */
static void doaddCounterBrkPoint(CHAR* module, CHAR *srcfilename, CHAR *line, INT count) {
//#pragma warning(suppress:4101)
	INT pmodnum, datanum, lineoffset, ptidx, i1;
	UCHAR *pgmptr;
	if (ddtfindmod(module, NULL, NULL, &pmodnum, &datanum)) dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
	else {
		if (dbgopendbg(pmodnum)) dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
		else {
			if (ddtfindline(srcfilename, line, &ptidx)) dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
			else {
				pgmptr = getmpgm(pmodnum);
				if (pgmptr == NULL) {
					sprintf(dbgwork, "Bad module number:%i", pmodnum);
					dbctcxputattr(variableinfo.channel, "e", dbgwork);
				}
				else {
					if (counterBrkhi != 0) {
						for (i1 = 0; i1 < counterBrkhi; i1++) {
							if (dbgCounterbrk[i1].ptidx == ptidx) {
								dbctcxputattr(variableinfo.channel, "e", "duplicate counter bp");
								return;
							}
						}
						for (i1 = 0; i1 < counterBrkhi; i1++) {
							if (dbgCounterbrk[i1].mod == -1) break;
						}
						if (i1 == counterBrkhi) {
							if (counterBrkhi + 1 > DEBUG_BRKMAX) {
								sprintf(dbgwork, "Max of %d breakpoints exceeded", DEBUG_BRKMAX);
								dbctcxputattr(variableinfo.channel, "e", dbgwork);
								return;
							}
							counterBrkhi++;
						}
					}
					else {
						i1 = 0;
						counterBrkhi = 1;
					}
					lineoffset = pgmtable[ptidx].pcnt;
					dbgCounterbrk[i1].mod = pmodnum;
					strcpy(dbgCounterbrk[i1].module, module);
					dbgCounterbrk[i1].ptidx = ptidx;
					dbgCounterbrk[i1].pcnt = lineoffset;
					dbgCounterbrk[i1].line = strtol(line, NULL, 10);
					dbgCounterbrk[i1].count = count;
					dbgCounterbrk[i1].verb = pgmptr[lineoffset];
				}
			}
		}
	}
}

static void doaddbrkpnt(CHAR *srcfilename, CHAR *line, UCHAR breakpointType) {
	INT pmodnum, datanum, lineoffset, ptidx, i1;
	UCHAR *pgmptr;
	if (ddtfindmod(variableinfo.module, NULL, variableinfo.class, &pmodnum, &datanum)) dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
	else {
		if (dbgopendbg(pmodnum)) dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
		else {
			if (ddtfindline(srcfilename, line, &ptidx)) dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
			else {
				pgmptr = getmpgm(pmodnum);
				if (pgmptr == NULL) {
					sprintf(dbgwork, "Bad module number:%i", pmodnum);
					dbctcxputattr(variableinfo.channel, "e", dbgwork);
				}
				else {
					if (brkhi != 0) {
						for (i1 = 0; i1 < brkhi; i1++) {
							if (dbgbrk[i1].ptidx == ptidx) {
								dbctcxputattr(variableinfo.channel, "e", "duplicate bp");
								goto doaddbp1;
							}
						}
						for (i1 = 0; i1 < brkhi; i1++) {
							if (dbgbrk[i1].mod == -1) break;
						}
						if (i1 == brkhi) {
							if (brkhi + 1 > DEBUG_BRKMAX) {
								sprintf(dbgwork, "Max of %d breakpoints exceeded", DEBUG_BRKMAX);
								dbctcxputattr(variableinfo.channel, "e", dbgwork);
								goto doaddbp1;
							}
							brkhi++;
						}
					}
					else {
						i1 = 0;
						brkhi = 1;
					}
					lineoffset = pgmtable[ptidx].pcnt;
					dbgbrk[i1].mod = pmodnum;
					dbgbrk[i1].ptidx = ptidx;
					dbgbrk[i1].pcnt = lineoffset;
					dbgbrk[i1].verb = pgmptr[lineoffset];
					dbgbrk[i1].type = breakpointType;
					pgmptr[lineoffset] = VERB_DEBUG;
				}
			}
		}
	}
doaddbp1:
	return;
}

static void ddtdoremovevarbp(void) {
	INT pmodnum, datanum, varnum = 0, offset, slotnum;
	dbctcxputtag(variableinfo.channel, "removebreakpoint");
	if (brkhi == 0) {
		dbctcxputattr(variableinfo.channel, "e", "There are no breakpoints defined.");
		goto doremovevarbp1;
	}
	/* Get the data module number */
	if (ddtfindmod(variableinfo.module, variableinfo.instance, variableinfo.class, &pmodnum, &datanum)) {
		dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
		goto doremovevarbp1;
	}
	/* Open the dbg file */
	if (dbgopendbg(pmodnum)) {
		dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
		goto doremovevarbp1;
	}
	/* Get index of the var in the datatable */
	if (ddtfindvarnum(&variableinfo, &varnum) == 1) {
		dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
		goto doremovevarbp1;
	}
	offset = datatable[varnum].offset;
	for (slotnum = 0; slotnum < ddtbrkvalhi; slotnum++) {
		if (ddtbrkvaltable[slotnum].datamod != datanum) continue;
		if (ddtbrkvaltable[slotnum].offset != offset) continue;
		if (ddtbrkvaltable[slotnum].arraycnt != variableinfo.arraycnt) continue;
		if (ddtbrkvaltable[slotnum].op != variableinfo.op) continue;
		if (strcmp(*ddtbrkvaltable[slotnum].value, variableinfo.value)) continue;
		if (variableinfo.arraycnt > 0) {
			if (ddtbrkvaltable[slotnum].array[0] != variableinfo.array[0]) break;
			if (variableinfo.arraycnt > 1) {
				if (ddtbrkvaltable[slotnum].array[1] != variableinfo.array[1]) break;
				if (variableinfo.arraycnt > 2) {
					if (ddtbrkvaltable[slotnum].array[2] != variableinfo.array[2]) break;
				}
			}
		}
		ddtremovebrkval(slotnum);
		break;
	}
	if (slotnum == ddtbrkvalhi) dbctcxputattr(variableinfo.channel, "e", "that variable bp not found");

doremovevarbp1:
	dbctcxput(variableinfo.channel);
}

static void doRemoveStandardBp(CHAR *srcfilename, CHAR *line) {
	INT pmodnum, datanum, lineoffset, ptidx, i1;
	UCHAR *pgmptr;
	dbctcxputtag(variableinfo.channel, "removebreakpoint");
	if (brkhi == 0) {
		dbctcxputattr(variableinfo.channel, "e", "There are no breakpoints defined.");
		goto doremovepointbp1;
	}
	if (ddtfindmod(variableinfo.module, NULL, variableinfo.class, &pmodnum, &datanum)) dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
	else {
		if (dbgopendbg(pmodnum)) dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
		else {
			if (ddtfindline(srcfilename, line, &ptidx)) dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
			else {
				pgmptr = getmpgm(pmodnum);
				if (pgmptr == NULL) {
					sprintf(dbgwork, "Bad module number:%i", pmodnum);
					dbctcxputattr(variableinfo.channel, "e", dbgwork);
				}
				else {
					for (i1 = 0; i1 < brkhi; i1++) {
						if (dbgbrk[i1].mod == -1) continue;
						if (dbgbrk[i1].ptidx == ptidx) {
							lineoffset = pgmtable[ptidx].pcnt;
							dbgbrk[i1].mod = -1;
							dbgbrk[i1].ptidx = -1;
							dbgbrk[i1].pcnt = -1;
							pgmptr[lineoffset] = dbgbrk[i1].verb;
							break;
						}
					}
					if (i1 == brkhi) dbctcxputattr(variableinfo.channel, "e", "Could not find that b.p.");
				}
			}
		}
	}
doremovepointbp1:
	dbctcxput(variableinfo.channel);
}

// <addCounterBreakpoint module="T1"><point line="7" count="5" source="T1.prg"/></addCounterBreakpoint>
static void processaddCounterbp(INT chan) {
	CHAR *attrtag, *attrval, *tag;
	CHAR bpsrcname[64], line[10], module[32];
	int count;
	dbctcxputtag(chan, ADDCOUNTERBREAKPOINT);
	variableinfo.channel = chan;	/* convenience */
	module[0] = '\0';	/* required for both <point...> and <variable...> */
	bpsrcname[0] = '\0';
	line[0] = '\0';
	count = -1;
	while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
		attrval = dbctcxgetattrvalue(chan);
		if (!strcmp(attrtag, "module")) strcpy(module, attrval);
	}
	while ((tag = dbctcxgetnextchild(chan)) != NULL) {
		if (!strcmp(tag, "point")) {
			bpsrcname[0] = '\0';
			while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
				attrval = dbctcxgetattrvalue(chan);
				if (!strcmp(attrtag, "source")) strcpy(bpsrcname, attrval);
				if (!strcmp(attrtag, "line")) strcpy(line, attrval);
				if (!strcmp(attrtag, "count")) count = atoi(attrval);
			}
		}
	}
	while (dbctcxgetnextchild(chan) != NULL);
	doaddCounterBrkPoint(module, bpsrcname, line, count);
	dbctcxput(chan);
}

static void processRemoveCounterbp(INT chan) {
	CHAR *attrtag, *attrval, *tag;
	CHAR bpsrcname[64], line[10], module[32];
	dbctcxputtag(chan, REMOVECOUNTERBREAKPOINT);
	module[0] = bpsrcname[0] = line[0] = '\0';
	while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
		attrval = dbctcxgetattrvalue(chan);
#if defined(_DEBUGDDT) && _DEBUGDDT
		sprintf(dbgwork, "dbcddt.processRemoveCounterbp, attrtag=%s, attrval=%s", attrtag, attrval);
		debugmessage(dbgwork, 0);
#endif
		if (!strcmp(attrtag, "module")) strcpy(module, attrval);
	}
	while ((tag = dbctcxgetnextchild(chan)) != NULL) {
		while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
			attrval = dbctcxgetattrvalue(chan);
			attrval = dbctcxgetattrvalue(chan);
			if (!strcmp(attrtag, "source")) strcpy(bpsrcname, attrval);
			if (!strcmp(attrtag, "line")) strcpy(line, attrval);
		}
	}
	while (dbctcxgetnextchild(chan) != NULL);
	doRemoveCounterBp(module, bpsrcname, line);
	dbctcxput(chan);
}

/**
 * Break down the XML for an addbreakpoint message
 * Always send only one acknowledgement
 * even if there are many 'point' and 'variable' children
 */
static void processAddBp(INT chan) {
	CHAR *attrtag, *attrval, *tag;
	CHAR bpsrcline[7], bpsrcname[64];
	dbctcxputtag(chan, ADDBREAKPOINT);
	variableinfo.channel = chan;	/* convenience */
	variableinfo.module[0] = '\0';	/* required for both <point...> and <variable...> */
	variableinfo.instance[0] = '\0';	/* only used if <variable..> children exist */
	while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
		attrval = dbctcxgetattrvalue(chan);
		if (!strcmp(attrtag, "module")) strcpy(variableinfo.module, attrval);
		if (!strcmp(attrtag, "instance")) strcpy(variableinfo.instance, attrval);
	}
	while ((tag = dbctcxgetnextchild(chan)) != NULL) {
		if (!strcmp(tag, "point")) {
			bpsrcline[0] = bpsrcname[0] = '\0';
			while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
				attrval = dbctcxgetattrvalue(chan);
				if (!strcmp(attrtag, "source")) strcpy(bpsrcname, attrval);
				if (!strcmp(attrtag, "line")) strcpy(bpsrcline, attrval);
			}
			if (variableinfo.module[0] != '\0' && bpsrcline[0] != '\0' && bpsrcname[0] != '\0') {
				doaddbrkpnt(bpsrcname, bpsrcline, BREAK_PERMANENT);
			}
		}
		else if (!strcmp(tag, "variable")) if (ddtdobreakonvalue()) break;
	}
	while (dbctcxgetnextchild(chan) != NULL);
	dbctcxput(chan);
}

/**
 * return 1 if error
 */
static int ddtdobreakonvalue() {
	INT pmodnum, datanum, varnum = 0, slotnum, i1;
	if (variableinfo.module[0] == '\0') {
		dbctcxputattr(variableinfo.channel, "e", "module attribute missing");
		return 1;
	}
	processvariableelement(); // Sets variableinfo.op
	if (variableinfo.value == NULL || strlen(variableinfo.value) < 1) {
		if (variableinfo.op != BRKVAL_PB) {
			dbctcxputattr(variableinfo.channel, "e", "(v)alue attribute missing");
			return 1;
		}
	}
	if (variableinfo.op == 0) {
		dbctcxputattr(variableinfo.channel, "e", "(o)peration attribute missing");
		return 1;
	}
	if (ddtfindmod(variableinfo.module, variableinfo.instance, variableinfo.class, &pmodnum, &datanum)) {
		dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
		return 1;
	}
	if (dbgopendbg(pmodnum)) {
		dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
		return 1;
	}
	if (ddtfindvarnum(&variableinfo, &varnum) == 1) {
		dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
		return 1;
	}

	/*
	 * Search the break-on-value table for the first empty slot
	 */
	for (slotnum = 0; slotnum < ddtbrkvalhi && ddtbrkvaltable[slotnum].op; slotnum++);
	if (slotnum == WATCHMAX) {
		dbctcxputattr(variableinfo.channel, "e", "Too many variables currently being tested");
		return 1;
	}
	if (slotnum == ddtbrkvalhi) ddtbrkvalhi++;

	ddtbrkvaltable[slotnum].offset = datatable[varnum].offset;
	ddtbrkvaltable[slotnum].datamod = datanum;
	ddtbrkvaltable[slotnum].op = variableinfo.op;
	for (i1 = 0; i1 < variableinfo.arraycnt; i1++) {
		ddtbrkvaltable[slotnum].array[i1] = variableinfo.array[i1];
	}
	ddtbrkvaltable[slotnum].arraycnt = variableinfo.arraycnt;
	if (variableinfo.op == BRKVAL_PB) {
		//Capture current value and put it into variableinfo.value
		if ((variableinfo.address = getmdata(datanum)) == NULL) {
			dbctcxputattr(variableinfo.channel, "e", "error loading data module");
			return 1;
		}
		else variableinfo.address += datatable[varnum].offset;
		variableinfo.value = getVariableValue(&variableinfo);
	}
	ddtbrkvaltable[slotnum].value = (CHAR **) memalloc((INT) strlen(variableinfo.value) + 1, 0);
	/* reset memory */
	setpgmdata();
	dbgsetmem();
	if (ddtbrkvaltable[slotnum].value == NULL) {
		dbctcxputattr(variableinfo.channel, "e", "Not enough memory to store break variable");
		return 1;
	}
	strcpy(*ddtbrkvaltable[slotnum].value, variableinfo.value);
	dbgflags |= DBGFLAGS_TSTVALU;
	return 0;
}

static void ddtremovebrkval(int slot) {
	if (ddtbrkvaltable[slot].value != NULL) memfree((UCHAR **) ddtbrkvaltable[slot].value);
	memset(&ddtbrkvaltable[slot], 0, sizeof(struct ddtbrkval_tag));
}

static void testValue() {
	int i1, i2, i3, hit;
	int nomoretests = TRUE;
	UCHAR *adr;
	ARRAYINFO arrayinfo;
	memset(&arrayinfo, 0x00, sizeof(ARRAYINFO));
	for (i1 = 0; i1 < ddtbrkvalhi; i1++) {
		if (ddtbrkvaltable[i1].op == 0) continue;
		if ((adr = getmdata(ddtbrkvaltable[i1].datamod)) == NULL) {
			ddtremovebrkval(i1);
			continue;
		}
		adr += ddtbrkvaltable[i1].offset;
		if (isAdrVar(adr) && (adr = ddtresolveadrvar(adr)) == NULL) {
			ddtremovebrkval(i1);
			continue;
		}
		scanvar(adr);
		if (vartype == TYPE_ARRAY) {
			for (i3 = 0; i3 < ddtbrkvaltable[i1].arraycnt; i3++) {
				arrayinfo.array[i3] = ddtbrkvaltable[i1].array[i3];
				arrayinfo.ndxtype[i3] = 'C';
				arrayinfo.datamod = ddtbrkvaltable[i1].datamod;	/* Not used yet, break table does not allow variable indexing */
			}
			if ((adr = ddtresolvearray(&arrayinfo)) == NULL) {
				ddtremovebrkval(i1);
				continue;
			}
			if (isAdrVar(adr) && (adr = ddtresolveadrvar(adr)) == NULL) {
				ddtremovebrkval(i1);
				continue;
			}
			scanvar(adr);
		}
		if (vartype == TYPE_CHAR) {
			i2 = compareChar(adr, ddtbrkvaltable[i1].value);
		}
		else if (vartype & TYPE_NUMVAR) i2 = ddtcmpnum(adr, ddtbrkvaltable[i1].value);
		else {
			ddtremovebrkval(i1);
			continue;
		}
		switch (ddtbrkvaltable[i1].op) {
		case BRKVAL_EQ:
			hit = i2 == 0;
			break;
		case BRKVAL_NE:
			hit = i2 != 0;
			break;
		case BRKVAL_LT:
			hit = i2 == -1;
			break;
		case BRKVAL_GT:
			hit = i2 == 1;
			break;
		case BRKVAL_LE:
			hit = i2 == -1 || i2 == 0;
			break;
		case BRKVAL_GE:
			hit = i2 == 1 || i2 == 0;
			break;
		case BRKVAL_PB:
			hit = i2 != 0;
			break;
		default:
			hit = FALSE;
		}
		if (hit) {
			if (ddtbrkvaltable[i1].op == BRKVAL_PB) {
				if (!setvartablevaluetocurrent(i1, adr)) {
					ddtremovebrkval(i1);
					continue;
				}
				dbgflags |= DBGFLAGS_SINGLE;
				nomoretests = FALSE;
			}
			else {
				ddtremovebrkval(i1);
				dbgflags |= DBGFLAGS_SINGLE;
			}
		}
		else nomoretests = FALSE;
	}
	if (nomoretests) {
		dbgflags &= ~DBGFLAGS_TSTVALU;
	}
}

/**
 * Put the current value of the variable (varaddress) in the ddtbrkvaltable[varnum]
 *
 * Assumes the variable is a char or a num, anything else will result
 * in undefined behavior.
 *
 * Assumes vartype, fp, lp, hl, pl are set for the variable
 *
 * Might move memory
 */
static INT setvartablevaluetocurrent(INT varnum, UCHAR *varaddress) {
	UCHAR *adr2;
	INT i3;
	CHAR **ptr;
	if (vartype & TYPE_CHARACTER) {  /* character variable */
		adr2 = &varaddress[hl];
		i3 = lp;
	}
	else {  /* numeric variable */
		adr2 = varaddress;
		i3 = hl + pl;
	}
	ptr = ddtbrkvaltable[varnum].value;
	if (memchange((UCHAR**)ptr, i3 + 1, 0)) {
		ddtremovebrkval(varnum);
		dbgerror(ERROR_FAILURE, "Not Enough Memory to Store Value");
		return FALSE;
	}
	setpgmdata();
	dbgsetmem();
	memcpy(*ptr, adr2, i3);
	*ptr[i3] = '\0';
	return TRUE;
}

/**
 * @param str1	The in-memory db/c character address.
 * 				we assume that scanvar has been called on this.
 * @param str2	The value from the value-break table, a C string.
 */
static int compareChar(UCHAR *str1, CHAR ** str2) {
	size_t len1 = fp == 0 ? 0 : lp - fp + 1;
	size_t len2 = strlen(*str2);
	int i1;
	if (len1 == 0) return -1;
	str1 += hl + fp - 1;
	i1 = memcmp(str1, *str2, min(len1, len2));
	if (len1 == len2) return i1;
	if (len1 < len2) return i1 == 0 ? -1 : i1;
	else return i1 == 0 ? 1 : i1;
}

/**
 * @param num1	The in-memory db/c numvar address.
 * 				we assume that scanvar has been called on this.
 * @param num2	The value from the value-break table
 */
static int ddtcmpnum(UCHAR *num1, CHAR **num2) {
	if (vartype == TYPE_INT) return ddtcmpint(num1, num2);
	else if (vartype == TYPE_FLOAT) return ddtcmpdbl(num1, num2);
	else if (vartype == TYPE_NUM) return ddtcmpform(num1, num2);
	return 0;
}

static int ddtcmpint(UCHAR *num1, CHAR **num2) {
	INT32 i1;
	char *ptr;
	long i2;
	i2 = strtol(*num2, &ptr, 0);
	if (ptr == *num2) return -1;
	memcpy((UCHAR*) &i1, &num1[2], sizeof(INT32));
	if (i1 == i2) return 0;
	else if (i1 < i2) return -1;
	else return 1;
}

static int ddtcmpdbl(UCHAR *num1, CHAR **num2) {
	DOUBLE64 y1;
	double y2;
	char *ptr;
	y2 = strtod(*num2, &ptr);
	if (ptr == *num2) return -1;
	memcpy((UCHAR*) &y1, &num1[2], sizeof(DOUBLE64));
	if (y1 == y2) return 0;
	else if (y1 < y2) return -1;
	else return 1;
}

static int ddtcmpform(UCHAR *num1, CHAR **num2) {
	CHAR wrkform[32];
	size_t i1 = strlen(*num2);
	int flagsave, retval;
	memcpy(&wrkform[1], *num2, i1);
	wrkform[0] = 0x80 | (char) i1;
	flagsave = dbcflags & DBCFLAG_FLAGS;
	mathop(0x00, num1, (UCHAR *) wrkform, NULL);
	if (dbcflags & DBCFLAG_EQUAL) retval = 0;
	else if (dbcflags & DBCFLAG_LESS) retval = -1;
	else retval = 1;
	dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | flagsave;
	return retval;
}

static void processRemovebp(INT chan) {
	CHAR *attrtag, *attrval, *tag;
	CHAR bpsrcline[7], bpsrcname[32];
	variableinfo.channel = chan;	/* convenience */
	variableinfo.module[0] = '\0';	/* required for both <point...> and <variable...> */
	variableinfo.instance[0] = '\0';	/* used for variable bp removal*/
	while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
		attrval = dbctcxgetattrvalue(chan);
		if (!strcmp(attrtag, "module")) strcpy(variableinfo.module, attrval);
		if (!strcmp(attrtag, "instance")) strcpy(variableinfo.instance, attrval);
	}
	while ((tag = dbctcxgetnextchild(chan)) != NULL) {
		if (!strcmp(tag, "point")) {
			bpsrcline[0] = bpsrcname[0] = '\0';
			while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
				attrval = dbctcxgetattrvalue(chan);
				if (!strcmp(attrtag, "source")) strcpy(bpsrcname, attrval);
				if (!strcmp(attrtag, "line")) strcpy(bpsrcline, attrval);
			}
			if (variableinfo.module[0] != '\0' && bpsrcline[0] != '\0' && bpsrcname[0] != '\0') {
				doRemoveStandardBp(bpsrcname, bpsrcline);
			}
		}
		else if (!strcmp(tag, "variable")) {
			processvariableelement();
			ddtdoremovevarbp();
		}
	}
}

static void processalter(INT chan) {
	CHAR *attrtag, *attrval, *tag;
	variableinfo.channel = chan;
	variableinfo.module[0] = '\0';
	variableinfo.instance[0] = '\0';
	dbctcxputtag(chan, "alter");
	while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
		attrval = dbctcxgetattrvalue(chan);
		if (!strcmp(attrtag, "module")) strcpy(variableinfo.module, attrval);
		if (!strcmp(attrtag, "instance")) strcpy(variableinfo.instance, attrval);
	}
	while ((tag = dbctcxgetnextchild(chan)) != NULL) {
		if (!strcmp(tag, "flags")) {
			doalterflags(chan);
			break;
		}
		else if (!strcmp(tag, "variable") && variableinfo.module[0] != '\0') ddtdoaltervar();
	}
	while (dbctcxgetnextchild(chan) != NULL);
	dbctcxput(chan);
	sendwatchedvars(chan);
}

static UCHAR* getNextListAddress(UCHAR *adr) {
	INT i1;
	UCHAR *tadr;
	if (*adr == 0xA4) return adr + skipvar(adr, TRUE);
	else if (*adr == 0xA5) return NULL;
	i1 = skipvar(adr, FALSE);
	tadr = adr + i1;
	if (*tadr == 0xA5) return NULL;
	return tadr;
}

static void processIsLineExecutable(INT chan) {
	CHAR *tag, *attrtag, *attrval;
	CHAR bpsrcline[7], bpsrcname[64];
	INT pmodnum, ptidx;
	INT datanum; // NOT USED
	variableinfo.channel = chan;
	variableinfo.module[0] = '\0';
	variableinfo.instance[0] = '\0';
	dbctcxputtag(chan, "isLineExecutable");
	while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
		attrval = dbctcxgetattrvalue(chan);
		if (!strcmp(attrtag, "module")) strcpy(variableinfo.module, attrval);
	}
	while ((tag = dbctcxgetnextchild(chan)) != NULL) {
		if (!strcmp(tag, "point")) {
			bpsrcline[0] = bpsrcname[0] = '\0';
			while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
				attrval = dbctcxgetattrvalue(chan);
				if (!strcmp(attrtag, "source")) strcpy(bpsrcname, attrval);
				if (!strcmp(attrtag, "line")) strcpy(bpsrcline, attrval);
			}
			if (ddtfindmod(variableinfo.module, NULL, variableinfo.class, &pmodnum, &datanum)) {
				dbctcxputattr(chan, "e", dbgerrorstring);
			}
			else {
				if (dbgopendbg(pmodnum)) {
					dbctcxputattr(chan, "e", dbgerrorstring);
				}
				else if (ddtfindline(bpsrcname, bpsrcline, &ptidx)) {
					dbctcxputattr(chan, "e", dbgerrorstring);
				}
			}
		}
	}
	dbctcxput(chan);
}

/**
 * There should be exactly one 'variable' element child of the 'listelements' Element.
 * It should be a list or record.
 * The return will have a 'variable' child for each field in the list/record.
 * Each of these will need only two attributes, name and DXType.
 */
static void processListElements(INT chan) {
	CHAR *tag, *attrtag, *attrval, work[64];
	INT pmodnum, datanum, varnum = 0, rtidx;
	UCHAR *adr, *dataadr, *tadr;

	dbctcxputtag(chan, "listelements");
	variableinfo.channel = chan;
	variableinfo.varname[0] = '\0';
	variableinfo.value = NULL;
	variableinfo.rtname[0] = '\0';
	variableinfo.op = 0;
	variableinfo.arraycnt = 0;
	variableinfo.array[0] = variableinfo.array[1] = variableinfo.array[2] = 0;
	variableinfo.varindex[0][0] = variableinfo.varindex[1][0] = variableinfo.varindex[2][0] = '\0';
	variableinfo.ndxtype[0] = variableinfo.ndxtype[1] = variableinfo.ndxtype[2] = '\0';
	while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
		attrval = dbctcxgetattrvalue(chan);
		if (!strcmp(attrtag, "module")) strcpy(variableinfo.module, attrval);
		if (!strcmp(attrtag, "instance")) strcpy(variableinfo.instance, attrval);
		if (!strcmp(attrtag, "c")) strcpy(variableinfo.class, attrval);
	}
	while ((tag = dbctcxgetnextchild(chan)) != NULL) {
		if (!strcmp(tag, "variable")) {
			processvariableelement();
			if (ddtfindmod(variableinfo.module, variableinfo.instance, variableinfo.class, &pmodnum, &datanum)) {
				dbctcxputattr(chan, "e", dbgerrorstring);
			}
			else {
				if (dbgopendbg(pmodnum)) dbctcxputattr(chan, "e", dbgerrorstring);
				else {
					if (ddtfindvarnum(&variableinfo, &varnum) == 1) dbctcxputattr(chan, "e", dbgerrorstring);
					else {
						if ((dataadr = getmdata(datanum)) == NULL) {
							dbctcxputattr(chan, "e", "error loading data module");
						}
						else {
							adr = dataadr + datatable[varnum].offset;
							scanvar(adr);	/* set fp, lp, pl, hl */
							if (vartype != TYPE_LIST) {
								dbctcxputattr(chan, "e", "Not a List!");
								break;
							}
							adr = getNextListAddress(adr);
							do {
								if (isAdrVar(adr)) {
									tadr = ddtresolveadrvar(adr);
									if (tadr == NULL) {
										dbctcxputattr(chan, "e", "Unable to resolve adr var");
										break;
									}
								}
								else tadr = adr;
								variableinfo.address = (UCHAR*)(tadr - dataadr);
								if (findVarnumByOffset(&variableinfo, &varnum)) {
									dbctcxputattr(chan, "e", dbgerrorstring);
									break;
								}
								dbctcxputsub(chan, "variable");
								dbctcxputattr(chan, "name", &nametable[datatable[varnum].nptr]);
								if ((rtidx = datatable[varnum].rtidx) != -1) {
									dbctcxputattr(chan, "r", &nametable[routinetable[rtidx].nptr]);
								}
								scanvar(adr);
								sprintf(work, "%i", vartype);
								dbctcxputattr(chan, "tc", work);
							} while ((adr = getNextListAddress(adr)) != NULL);
						}
					}
				}
			}
			break;
		}
	}
	dbctcxflushincoming(chan);
	dbctcxput(chan);
}

static void processvariableelement() {
	CHAR *attrtag, *attrval;
	int isArrayElement, i1, i2;
	variableinfo.varname[0] = '\0';
	variableinfo.value = NULL;
	variableinfo.rtname[0] = '\0';
	variableinfo.op = 0;
	variableinfo.arraycnt = 0;
	variableinfo.array[0] = variableinfo.array[1] = variableinfo.array[2] = 0;
	variableinfo.varindex[0][0] = variableinfo.varindex[1][0] = variableinfo.varindex[2][0] = '\0';
	variableinfo.ndxtype[0] = variableinfo.ndxtype[1] = variableinfo.ndxtype[2] = '\0';
	isArrayElement = FALSE;
	while ((attrtag = dbctcxgetnextattr(variableinfo.channel)) != NULL) {
		attrval = dbctcxgetattrvalue(variableinfo.channel);
		i1 = (INT) strlen(attrval);
		if (!strcmp(attrtag, "name")) {
			i2 = sizeof(variableinfo.varname) - 1;
			if (i1 <= i2) strcpy(variableinfo.varname, attrval);
			else {
				strncpy(variableinfo.varname, attrval, i2);
				variableinfo.varname[i2] = '\0';
			}
		}
		else if (!strcmp(attrtag, "r")) strcpy(variableinfo.rtname, attrval);
		else if (!strcmp(attrtag, "v")) variableinfo.value = attrval;
		else if (!strcmp(attrtag, "array") && !strcmp(attrval, "y")) isArrayElement = TRUE;
		else if (!strcmp(attrtag, "i")) {
			if (isdigit(attrval[0])) {
				variableinfo.array[0] = (INT) strtol(attrval, NULL, 10);
				variableinfo.ndxtype[0] = 'C';
			}
			else {
				strcpy(variableinfo.varindex[0], attrval);
				variableinfo.ndxtype[0] = 'V';
			}
		}
		else if (!strcmp(attrtag, "j")) {
			if (isdigit(attrval[0])) {
				variableinfo.array[1] = (INT) strtol(attrval, NULL, 10);
				variableinfo.ndxtype[1] = 'C';
			}
			else {
				strcpy(variableinfo.varindex[1], attrval);
				variableinfo.ndxtype[1] = 'V';
			}
		}
		else if (!strcmp(attrtag, "k")) {
			if (isdigit(attrval[0])) {
				variableinfo.array[2] = (INT) strtol(attrval, NULL, 10);
				variableinfo.ndxtype[2] = 'C';
			}
			else {
				strcpy(variableinfo.varindex[2], attrval);
				variableinfo.ndxtype[2] = 'V';
			}
		}
		else if (!strcmp(attrtag, "o")) {
			if (!strcmp(attrval, "eq")) variableinfo.op = BRKVAL_EQ;
			else if (!strcmp(attrval, "ne")) variableinfo.op = BRKVAL_NE;
			else if (!strcmp(attrval, "lt")) variableinfo.op = BRKVAL_LT;
			else if (!strcmp(attrval, "gt")) variableinfo.op = BRKVAL_GT;
			else if (!strcmp(attrval, "le")) variableinfo.op = BRKVAL_LE;
			else if (!strcmp(attrval, "ge")) variableinfo.op = BRKVAL_GE;
			else if (!strcmp(attrval, "pb")) variableinfo.op = BRKVAL_PB;
		}
	}
	if (isArrayElement) {
		if (variableinfo.ndxtype[0] != '\0') {
			variableinfo.arraycnt++;
			if (variableinfo.ndxtype[1] != '\0') {
				variableinfo.arraycnt++;
				if (variableinfo.ndxtype[2] != '\0') {
					variableinfo.arraycnt++;
				}
			}
		}
	}
}

static void processaddwatch(INT chan) {
	CHAR *tag, *attrtag, *attrval;
	variableinfo.channel = chan;
	variableinfo.module[0] = '\0';
	variableinfo.instance[0] = '\0';
	variableinfo.class[0] = '\0';
	variableinfo.arraycnt = 0;
	dbctcxputtag(chan, "addwatch");
	while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
		attrval = dbctcxgetattrvalue(chan);
		if (!strcmp(attrtag, "module")) strcpy(variableinfo.module, attrval);
		if (!strcmp(attrtag, "instance")) strcpy(variableinfo.instance, attrval);
		if (!strcmp(attrtag, "c")) strcpy(variableinfo.class, attrval);
	}
	if (variableinfo.module[0] != '\0') {
		while ((tag = dbctcxgetnextchild(chan)) != NULL) {
			if (strcmp(tag, "variable")) continue;
			processvariableelement();
			if (variableinfo.varname[0] != '\0') {
				if (addvartowatchtable()) dbctcxputattr(chan, "e", dbgerrorstring);
			}
		}
	}
	dbctcxput(chan);
	sendwatchedvars(chan);
}


static INT addvartowatchtable() {
	int i1, varnum = 0, vnlen, modlen, instlen, rtnlen, clslen;
	INT pmodnum, datanum, offset, tempvarnum;
	VARINFO tempvarinfo;

	/* Convert the module and instance names into a program module number
	 * and a data area number
	 */
	if (ddtfindmod(variableinfo.module, variableinfo.instance, variableinfo.class, &pmodnum, &datanum)) return -1;

	/*
	 * Load the *.dbg file
	 */
	if (dbgopendbg(pmodnum)) return -1;

	/*
	 * Search the dbg tables to get the variable's index
	 * Grab the offset of this variable in the data area.
	 * We save that, it can't change.
	 */
	if (ddtfindvarnum(&variableinfo, &varnum) == 1) return RC_ERROR;
	offset = datatable[varnum].offset;

	/*
	 * Search the ddt watch table for the first empty slot
	 */
	for (varnum = 0; varnum < watchvarhi && watchtable[varnum].nameptr; varnum++);
	if (varnum == WATCHMAX) {
		dbgerror(ERROR_WARNING, "Too many variables currently being watched");
		return -1;
	}
	if (varnum == watchvarhi) watchvarhi++;

	watchtable[varnum].offset = offset;
	watchtable[varnum].datamod = datanum;
	vnlen = (INT) strlen(variableinfo.varname);
	modlen = (INT) strlen(variableinfo.module);
	if (variableinfo.instance[0] != '\0') instlen = (INT) strlen(variableinfo.instance);
	else instlen = 0;
	if (variableinfo.rtname[0] != '\0') rtnlen = (INT) strlen(variableinfo.rtname);
	else rtnlen = 0;
	if (variableinfo.class[0] != '\0') clslen = (INT) strlen(variableinfo.class);
	else clslen = 0;
	watchtable[varnum].nameptr = (CHAR **) memalloc(vnlen + 1, 0);
	watchtable[varnum].moduleptr = (CHAR **) memalloc(modlen + 1, 0);
	watchtable[varnum].instanceptr = (instlen > 0) ? (CHAR **) memalloc(instlen + 1, 0) : NULL;
	watchtable[varnum].rtnptr = (rtnlen > 0) ? (CHAR **) memalloc(rtnlen + 1, 0) : NULL;
	watchtable[varnum].clsptr = (clslen > 0) ? (CHAR **) memalloc(clslen + 1, 0) : NULL;
	/* reset memory */
	setpgmdata();
	dbgsetmem();
	if (watchtable[varnum].nameptr == NULL
		|| watchtable[varnum].moduleptr == NULL
		|| (instlen > 0 && watchtable[varnum].instanceptr == NULL)
		|| (rtnlen > 0 && watchtable[varnum].rtnptr == NULL)
		|| (clslen > 0 && watchtable[varnum].clsptr == NULL)) {
		dbgerror(ERROR_WARNING, "Not enough memory to store watch variable");
		return -1;
	}
	watchtable[varnum].arraycnt = variableinfo.arraycnt;
	strcpy(*watchtable[varnum].nameptr, variableinfo.varname);
	strcpy(*watchtable[varnum].moduleptr, variableinfo.module);
	if (instlen > 0) strcpy(*watchtable[varnum].instanceptr, variableinfo.instance);
	if (rtnlen > 0) strcpy(*watchtable[varnum].rtnptr, variableinfo.rtname);
	if (clslen > 0) strcpy(*watchtable[varnum].clsptr, variableinfo.class);
	for (i1 = 0; i1 < variableinfo.arraycnt; i1++) {
		watchtable[varnum].ndxtype[i1] = variableinfo.ndxtype[i1];
		if (variableinfo.ndxtype[i1] == 'C') watchtable[varnum].array[i1] = variableinfo.array[i1];
		else if (variableinfo.ndxtype[i1] == 'V') {
			tempvarinfo = variableinfo;
			strcpy(tempvarinfo.varname, variableinfo.varindex[i1]);
			strcpy(watchtable[varnum].varindex[i1], tempvarinfo.varname);
			if (ddtfindvarnum(&tempvarinfo, &tempvarnum) == 1) return RC_ERROR;
			watchtable[varnum].array[i1] = datatable[tempvarnum].offset;
		}
	}
	return 0;
}

static void processRemoveWatch(INT chan) {
	CHAR *tag, *attrtag, *attrval;

	variableinfo.channel = chan;
	variableinfo.module[0] = '\0';
	variableinfo.instance[0] = '\0';
	variableinfo.class[0] = '\0';
	dbctcxputtag(chan, "removewatch");
	while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
		attrval = dbctcxgetattrvalue(chan);
		if (!strcmp(attrtag, "module")) strcpy(variableinfo.module, attrval);
		if (!strcmp(attrtag, "instance")) strcpy(variableinfo.instance, attrval);
		if (!strcmp(attrtag, "c")) strcpy(variableinfo.class, attrval);
	}
	if (variableinfo.module[0] != '\0') {
		while ((tag = dbctcxgetnextchild(chan)) != NULL) {
			if (!strcmp(tag, "variable")) {
				processvariableelement();
				if (variableinfo.varname[0] != '\0') {
					if (removeVarFromWatchTable()) dbctcxputattr(chan, "e", dbgerrorstring);
				}
			}
			else {
				sprintf(dbgwork, "Unknown element tag:%s", tag);
				dbctcxputattr(chan, "e", dbgwork);
				break;
			}
		}
	}
	else dbctcxputattr(chan, "e", "module attribute missing");
	dbctcxput(chan);
}

/**
 * We could have multiple entries in the watch table for the same array,
 * just with different subscripts.
 * We must match not just the name, but the subscripts.
 */
static INT removeVarFromWatchTable() {
	int varnum, i1, match;
	CHAR **instptr, **rtnptr, **clsptr;
	for (varnum = 0; varnum < watchvarhi; varnum++) {
		if (watchtable[varnum].nameptr == 0) continue;
		if (!strcmp(variableinfo.varname, *watchtable[varnum].nameptr)) {
			match = TRUE;
			for (i1 = 0; i1 < variableinfo.arraycnt; i1++) {
				if (watchtable[varnum].ndxtype[i1] != variableinfo.ndxtype[i1]
					|| (variableinfo.ndxtype[i1] == 'C' && watchtable[varnum].array[i1] != variableinfo.array[i1])
                    || (variableinfo.ndxtype[i1] == 'V' && strcmp(watchtable[varnum].varindex[i1], variableinfo.varindex[i1])))
				{
					match = FALSE;
					break;
				}
			}
			if (!match) continue;
			if (variableinfo.rtname[0] != '\0') {
				if (watchtable[varnum].rtnptr == NULL
					|| strcmp(variableinfo.rtname, *watchtable[varnum].rtnptr)) continue;
			}
			else if (watchtable[varnum].rtnptr != NULL) continue;
			if (variableinfo.class[0] != '\0') {
				if (watchtable[varnum].clsptr == NULL
					|| strcmp(variableinfo.class, *watchtable[varnum].clsptr)) continue;
			}
			else if (watchtable[varnum].clsptr != NULL) continue;
			if (variableinfo.instance[0] != '\0') {
				if (watchtable[varnum].instanceptr == NULL
					|| strcmp(variableinfo.instance, *watchtable[varnum].instanceptr)) continue;
			}
			else if (watchtable[varnum].instanceptr != NULL) continue;
			break;
		}
	}
	if (varnum == watchvarhi) {
		sprintf(dbgwork, "could not find '%s' in watch table", variableinfo.varname);
		dbgerror(ERROR_WARNING, dbgwork);
		return -1;
	}
	else {
		instptr = watchtable[varnum].instanceptr;
		rtnptr = watchtable[varnum].rtnptr;
		clsptr = watchtable[varnum].clsptr;
		memfree((UCHAR **)watchtable[varnum].nameptr);
		memfree((UCHAR **)watchtable[varnum].moduleptr);
		if (instptr != 0) memfree((UCHAR **)instptr);
		if (rtnptr != 0) memfree((UCHAR **)rtnptr);
		if (clsptr != 0) memfree((UCHAR **)clsptr);
		memset(&watchtable[varnum], 0, sizeof(WATCHTABLE));
	}
	return 0;
}

static void sendwatchedvars(INT chan) {
	int varnum, i1;
	CHAR **instptr, **nameptr, **rtnptr, **clsptr, awrk[32];

	for (varnum = 0; varnum < watchvarhi; varnum++) {
		nameptr = watchtable[varnum].nameptr;
		if (nameptr != 0) {
			instptr = watchtable[varnum].instanceptr;
			rtnptr = watchtable[varnum].rtnptr;
			clsptr = watchtable[varnum].clsptr;
			dbctcxputtag(chan, "w");
			dbctcxputattr(chan, "m", *watchtable[varnum].moduleptr);
			if (instptr != 0) dbctcxputattr(chan, "s", *instptr);
			if (rtnptr != 0) dbctcxputattr(chan, "r", *rtnptr);
			if (clsptr != 0) dbctcxputattr(chan, "c", *clsptr);
			dbctcxputattr(chan, "n", *nameptr);
			variableinfo.datamod = watchtable[varnum].datamod;
			if ((variableinfo.address = getmdata(variableinfo.datamod)) == NULL) {
				dbctcxputattr(chan, "e", "error loading data module");
			}
			else {
				variableinfo.address += watchtable[varnum].offset;
				variableinfo.arraycnt = watchtable[varnum].arraycnt;
				for (i1 = 0; i1 < variableinfo.arraycnt; i1++) {
					variableinfo.array[i1] = watchtable[varnum].array[i1];
					variableinfo.ndxtype[i1] = watchtable[varnum].ndxtype[i1];
				}
				addValueDescAttrs(chan);
				if (variableinfo.arraycnt) {
					dbctcxputattr(chan, "array", "y");
					for (i1 = 0; i1 < variableinfo.arraycnt; i1++) {
						if (variableinfo.ndxtype[i1] == 'C') mscitoa(variableinfo.array[0], awrk);
						else strcpy(awrk, variableinfo.varindex[i1]);
						if (i1 == 0) dbctcxputattr(chan, "i", awrk);
						else if (i1 == 1) dbctcxputattr(chan, "j", awrk);
						else if (i1 == 2) dbctcxputattr(chan, "k", awrk);
					}
				}
			}
			dbctcxput(chan);
		}
	}

}

/**
 * Add a v(alue), a d(escription) attribute, and a t(ype) code
 */
static void addValueDescAttrs(INT chan) {
	UCHAR *adr;
	CHAR *value;
	INT typecode = -1;
	CHAR work[8];

	if (isAdrVar(variableinfo.address)) {
		if ((adr = ddtresolveadrvar(variableinfo.address)) == NULL) {
			dbctcxputattr(chan, "t", "0");
			return;
		}
		variableinfo.address = adr;
	}
	else adr = variableinfo.address;

	dbctcxputattr(chan, "d", ddtgetvardesc(&variableinfo, &typecode));
	if ((value = getVariableValue(&variableinfo)) != NULL) dbctcxputattr(chan, "v", value);
	sprintf(work, "%d", typecode);
	dbctcxputattr(chan, "t", work);
	scanvar(adr);
	if (vartype == TYPE_ARRAY && (typecode == 3 || typecode == 4)) {
		if (*adr >= 0xA6) {
			mscitoa(llhh(&adr[1]), work);
			dbctcxputattr(chan, "i", work);
		}
		if (*adr >= 0xA7) {
			mscitoa(llhh(&adr[3]), work);
			dbctcxputattr(chan, "j", work);
		}
		if (*adr == 0xA8) {
			mscitoa(llhh(&adr[5]), work);
			dbctcxputattr(chan, "k", work);
		}
	}
}

static void processDisplay(INT chan) {
	CHAR *tag, *attrtag, *attrval;

	variableinfo.channel = chan;
	variableinfo.module[0] = '\0';
	variableinfo.instance[0] = '\0';
	variableinfo.class[0] = '\0';
	while ((attrtag = dbctcxgetnextattr(chan)) != NULL) {
		attrval = dbctcxgetattrvalue(chan);
		if (!strcmp(attrtag, "module")) strcpy(variableinfo.module, attrval);
		if (!strcmp(attrtag, "instance")) strcpy(variableinfo.instance, attrval);
		if (!strcmp(attrtag, "c")) strcpy(variableinfo.class, attrval);
	}
	if (variableinfo.module[0] != '\0') {
		dbctcxputtag(variableinfo.channel, "display");
		while ((tag = dbctcxgetnextchild(chan)) != NULL) {
			if (strcmp(tag, "variable")) continue;
			processvariableelement();
			if (variableinfo.varname[0] != '\0') sendDisplay();
		}
		dbctcxput(variableinfo.channel);
	}
}

static void processModlist(INT chan) {
	INT pgmmax = getpgmmax(), dmod, i1;	/* getpgmmax is in dbcldr */
	PGMMOD *pgmtab;
	DATAMOD *datatab;
	UCHAR typeflag;
	CHAR *pname, *iname, datamod[12];

	dbctcxputtag(chan, "modlist");
	pgmtab = *pgmtabptr;
	datatab = *datatabptr;
	for (i1 = 0; i1 < pgmmax; i1++) {
		if (pgmtab[i1].pgmmod == -1) continue;
		dmod = pgmtab[i1].dataxmod;
		typeflag = pgmtab[i1].typeflg;
		pname = *(*pgmtab[i1].codeptr)->nameptr;
		if (typeflag == MTYPE_PRELOAD || typeflag == MTYPE_LOADMOD) {
			do {
				dbctcxputsub(chan, "module");
				dbctcxputattr(chan, "name", pname);
				if (datatab[dmod].nameptr != NULL) {
					iname = *datatab[dmod].nameptr;
					if (strlen(iname) > 0) dbctcxputattr(chan, "instance", iname);
				}
			}
			while ((dmod = datatab[dmod].nextmod) != -1);
		}
		else if (typeflag == MTYPE_CLASS) {
			do {
				dbctcxputsub(chan, "module");
				dbctcxputattr(chan, "name", pname);
				if (datatab[dmod].nameptr != NULL) {
					iname = *datatab[dmod].nameptr;
					if (strlen(iname) > 0) dbctcxputattr(chan, "c", iname);
				}
				mscitoa(datatab[dmod].datamod, datamod);
				dbctcxputattr(chan, "instance", datamod);
			}
			while ((dmod = datatab[dmod].nextmod) != -1);
		}
		else {
			dbctcxputsub(chan, "module");
			dbctcxputattr(chan, "name", pname);
		}
	}
	dbctcxput(chan);
}

static void sendDisplay()
{
	INT pmodnum, datanum, varnum = 0, tempvarnum = 0, i1;
	UCHAR *adr;
	VARINFO tempvarinfo;
	CHAR work[256];
	if (ddtfindmod(variableinfo.module, variableinfo.instance, variableinfo.class, &pmodnum, &datanum))
		dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
	else {
		if (dbgopendbg(pmodnum)) dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
		else {
			if (ddtfindvarnum(&variableinfo, &varnum) == 1)
				dbctcxputattr(variableinfo.channel, "e", dbgerrorstring);
			else {
				if ((adr = getmdata(datanum)) == NULL) {
					dbctcxputattr(variableinfo.channel, "e", "error loading data module");
				}
				else {
					variableinfo.address = adr + datatable[varnum].offset;
					dbctcxputsub(variableinfo.channel, "variable");
					dbctcxputattr(variableinfo.channel, "name", variableinfo.varname);
					for (i1 = 0; i1 < variableinfo.arraycnt; i1++) {
						if (variableinfo.ndxtype[i1] != 'V') continue;
						tempvarinfo = variableinfo;
						strcpy(tempvarinfo.varname, variableinfo.varindex[i1]);
						if (ddtfindvarnum(&tempvarinfo, &tempvarnum) == 1) {
							sprintf(work, "error finding %s", tempvarinfo.varname);
							dbctcxputattr(variableinfo.channel, "e", work);
							return;
						}
						variableinfo.array[i1] = datatable[tempvarnum].offset;
					}
					addValueDescAttrs(variableinfo.channel);
				}
			}
		}
	}
}

/**
 * Given a module number, and an instance name,
 * find the data module number, that is the index into the datamod table.
 */
static int getInstanceDataNumber(INT pmodnum, CHAR *instance) {
	PGMMOD *pgmtab = getModuleStruct(pmodnum);
	DATAMOD *datatab;
	UCHAR typeflag;
	INT dmod;
	CHAR *iname;
	if (pgmtab == NULL) {
		sprintf(dbgerrorstring, "Bad module number:%i", pmodnum);
		return -1;
	}
	datatab = *datatabptr;
	typeflag = pgmtab->typeflg;
	dmod = pgmtab->dataxmod;
	if (typeflag == MTYPE_CHAIN || instance == NULL || instance[0] == '\0') return dmod;
	dbgerrorstring[0] = '\0';
	if (typeflag == MTYPE_PRELOAD || typeflag == MTYPE_LOADMOD) {
		do {
			if (datatab[dmod].nameptr != NULL) {
				iname = *datatab[dmod].nameptr;
				if (!strcmp(instance, iname)) return dmod;
			}
		} while ((dmod = datatab[dmod].nextmod) != -1);
		return -1;
	}
	sprintf(dbgerrorstring, "Unsupported module type:%i", typeflag);
	return -1;
}

/**
 * Given a module number, return a pointer to the PGMMOD structure
 */
static PGMMOD* getModuleStruct(INT pmodnum) {
	INT pgmmax = getpgmmax(), i1;
	PGMMOD *pgmtab = *pgmtabptr;
	for (i1 = 0; i1 < pgmmax; i1++) {
		if (pgmtab[i1].pgmmod == -1) continue;
		if (pgmtab[i1].pgmmod == pmodnum) break;
	}
	if (i1 == pgmmax) return NULL;
	pgmtab += i1;
	return pgmtab;
}

/**
 * Given a module name, find the module number.
 * Note that this is not an index into the pgmmod table.
 *
 * @param name The module name.
 * @return The module number.
 *
 */
static int getModuleNumber(CHAR *name) {
	INT pgmmax = getpgmmax(), i1;
	PGMMOD *pgmtab = *pgmtabptr;
	CHAR *pname;
	for (i1 = 0; i1 < pgmmax; i1++) {
		if (pgmtab[i1].pgmmod == -1) continue;
		pname = *(*pgmtab[i1].codeptr)->nameptr;
		if (!strcmp(pname, name)) break;
	}
	return i1 == pgmmax ? -1 : pgmtab[i1].pgmmod;
}

/**
 * Lookup the module number and datamod number
 * given the module and instance name.
 *
 * @param modname IN, the module name
 * @param instance IN, the instance name, NULL or zero-length string if none.
 * @param pmod OUT, the program module code, index into pgmtabptr
 * @param dmod OUT, the data module code, index into datatabptr
 *
 * @return 0 if successful, 1 if failure
 * 			if 1 then dbgerrorstring contains message.
 */
static int ddtfindmod(CHAR *modname, CHAR *instance, CHAR *class, INT *pmod, INT *dmod) {
	CHAR tmname[128], dwork[64];

	*pmod = *dmod = -1;
	/* the findmname (in dbcldr) appends <>, so we save and restore the module name */
	if (instance == NULL || instance[0] == '\0') {
		strcpy(tmname, modname);
		findmname(modname, pmod, dmod);
		strcpy(modname, tmname);
	}
	else if (class != NULL && class[0] != '\0') {
		strcpy(dwork, modname);
		strcat(dwork, "!");
		strcat(dwork, class);
		strcat(dwork, "(");
		strcat(dwork, instance);
		strcat(dwork, ")");
		findmname(dwork, pmod, dmod);
	}
	else {
		strcpy(dwork, modname);
		strcat(dwork, "<");
		strcat(dwork, instance);
		strcat(dwork, ">");
		findmname(dwork, pmod, dmod);
	}
	if (*pmod == -1) {
		strcpy(dbgerrorstring, "Module Not Currently Loaded");
		return 1;
	}
	if (*dmod == -1) {
		strcpy(dbgerrorstring, "Data Module Not Currently Loaded");
		return 1;
	}
	return 0;
}

/**
 * Takes array information and returns the address
 * of the indexed element
 */
static UCHAR* ddtresolvearray(ARRAYINFO *arrayinfo) {
	int i, j, k, jlen, klen, ndx[3], i1;
	UCHAR *adr, *dataadr, *datamod;

	ndx[0] = ndx[1] = ndx[2] = 0;
	datamod = getmdata(arrayinfo->datamod);
	for (i1 = 0; i1 < 3; i1++) {
		if (arrayinfo->ndxtype[i1] == '\0') break;
		if (arrayinfo->ndxtype[i1] == 'C') ndx[i1] = arrayinfo->array[i1];
		else if (arrayinfo->ndxtype[i1] == 'V') {
			/*
			 * arrayinfo->array[x] is the offset of the variable in the data area.
			 */
			dataadr = datamod + arrayinfo->array[i1];
			ndx[i1] = getNumericVariableValue(dataadr);
		}
	}
	adr = arrayinfo->adr;
	switch (adr[0]) {
	case 0xA6:
		i = ndx[0];
		if (1 <= i && i <= llhh(&adr[1])) adr += 5 + --i * llhh(&adr[3]);
		else adr = NULL;
		break;
	case 0xA7:
		i = ndx[0];
		j = ndx[1];
		jlen = llhh(&adr[3]);
		if (1 <= i && i <= llhh(&adr[1]) && 1 <= j && j <= jlen) {
			adr += 7 + (--i * jlen + --j) * llhh(&adr[5]);
		}
		else adr = NULL;
		break;
	case 0xA8:
		i = ndx[0];
		j = ndx[1];
		k = ndx[2];
		jlen = llhh(&adr[3]);
		klen = llhh(&adr[5]);
		if (1 <= i && i <= llhh(&adr[1]) && 1 <= j && j <= jlen && 1 <= k && k <= klen) {
			adr += 9 + (--i * jlen * klen + --j * klen + --k) * llhh(&adr[7]);
		}
		else adr = NULL;
		break;
	default:
		adr = NULL;
	}
	return adr;
}

static int isAdrVar(UCHAR *adr) {
	return (*adr >= 0xB0 && *adr <= 0xC7);
}

static UCHAR* ddtresolveadrvar(UCHAR *adr) {
	UCHAR *dataadr;
	if ((dataadr = getmdata(llhh(&adr[1]))) == NULL) adr = NULL;
	else adr = dataadr + llmmhh(&adr[3]);
	return adr;
}

/**
 * Expects the address field of the VARINFO to be set
 */
static CHAR* getVariableValue(PVARINFO varinfo) {
	static CHAR value[RIO_MAX_RECSIZE + 4];
	static UCHAR work[64];
	ARRAYINFO arrayinfo;
	UCHAR *adr;
	INT i1;

	adr = varinfo->address;
	scanvar(adr);
	if (vartype == TYPE_ARRAY) {
		/*
		 * If this is an array, and not an array element,
		 * send back NULL for the value.
		 * Otherwise, Fill in elements of arrayinfo
		 */
		if (varinfo->arraycnt < 1) return NULL;
		arrayinfo.adr = varinfo->address;
		arrayinfo.datamod = varinfo->datamod;
		for (i1 = 0; i1 < varinfo->arraycnt; i1++) {
			arrayinfo.array[i1] = varinfo->array[i1];
			arrayinfo.ndxtype[i1] = varinfo->ndxtype[i1];
		}
		if ((adr = ddtresolvearray(&arrayinfo)) == NULL) return NULL;
		if (isAdrVar(adr)) if ((adr = ddtresolveadrvar(adr)) == NULL) return NULL;
		scanvar(adr);
	}
	if (vartype & TYPE_CHARACTER) {
		if (vartype & TYPE_NULL) value[0] = '\0';
		else {
			strncpy(value, (CHAR *) adr + hl, lp);
			value[lp] = '\0';
		}
		return value;
	}
	if (vartype & TYPE_NUMERIC) {
		if (vartype & TYPE_NULL) value[0] = '\0';
		else {
			if (vartype & (TYPE_INT | TYPE_FLOAT)) {  /* integer / float */
				nvmkform(adr, work);
				strncpy(value, (CHAR *) &work[1], pl);
				value[pl] = '\0';
			}
			else {
				strncpy(value, (CHAR *) adr + hl, lp);
				value[lp] = '\0';
			}
		}
		return value;
	}
	return NULL;
}

static INT getNumericVariableValue(UCHAR *adr) {
	UCHAR work[64];
	UCHAR value[32];

	scanvar(adr);
	if (vartype & TYPE_NULL) {
		return 0;
	}
	else {
		if (vartype & (TYPE_INT | TYPE_FLOAT)) {  /* integer / float */
			nvmkform(adr, work);
			strncpy((CHAR*)value, (CHAR *) &work[1], pl);
			value[pl] = '\0';
		}
		else if (vartype & TYPE_NUM) {
			strncpy((CHAR*)value, (CHAR *) adr + hl, lp);
			value[lp] = '\0';
		}
		else {
			return 0;
		}
	}
	return strtol((CHAR*)value, NULL, 10);
}

/**
 *
 */
static CHAR* ddtgetvardesc(PVARINFO varinfo, INT *typecode)
{
	static CHAR desc[128];
	UCHAR work[MAX_NAMESIZE];
	UCHAR *adr;
	ARRAYINFO arrayinfo;
	VARINFO tempvarinfo;
	INT i1;

	memset(&arrayinfo, 0x00, sizeof(ARRAYINFO));
	adr = varinfo->address;
	scanvar(adr);
	if (vartype == TYPE_ARRAY) {
		if (varinfo->arraycnt) {
			/* Fill in elements of arrayinfo */
			arrayinfo.adr = varinfo->address;
			arrayinfo.datamod = varinfo->datamod;
			for (i1 = 0; i1 < varinfo->arraycnt; i1++) {
				arrayinfo.array[i1] = varinfo->array[i1];
				arrayinfo.ndxtype[i1] = varinfo->ndxtype[i1];
			}
			if ((adr = ddtresolvearray(&arrayinfo)) == NULL) strcpy(desc, "?");
			else {
				tempvarinfo = *varinfo;
				tempvarinfo.address = adr;
				return ddtgetvardesc(&tempvarinfo, typecode);
			}
		}
		else {
			switch (adr[0]) {
			case 0xA6:
			case 0xA7:
			case 0xA8:
				desc[0] = '\0';
				getArrayType(adr, typecode);
				break;
			}
		}
	}
	else {
		if (vartype & TYPE_CHARACTER) {
			if (vartype & TYPE_NULL) strcpy(desc, "character (NULL)");
			else desc[appendfplpplclause(desc)] = '\0';
		}
		else if (vartype & TYPE_NUMERIC) {
			if (vartype & TYPE_INT) strcpy(desc, "integer");
			else if (vartype & TYPE_FLOAT) strcpy(desc, "float");
			else strcpy(desc, "number");
			if (vartype & TYPE_NULL) strcat(desc, " (NULL)");
		}
		else if (vartype == TYPE_LIST) strcpy(desc, "LIST variable"); /* *adr = 0xA4 */
		/* FILE, IFILE, AFILE, COMFILE, PFILE */
		else if ((*adr >= 0xA0 && *adr <= 0xA3) || *adr == 0xA9) strcpy(desc, dbggetfiletypedesc((CHAR*)work, adr));
		/* object, image, queue, resource, device */
		else if (*adr >= 0xAA && *adr <= 0xAE) strcpy(desc, dbggetspecialtypedesc((CHAR*)work, adr));
		else strcpy(desc, "type not supported");
	}
	return desc;
}

/*
 * adr points to the first character of the array in memory
 * return the type code in typecode
 */
static void getArrayType(UCHAR *adr, INT *typecode) {
	UCHAR c1;
	UCHAR *a2;
	c1 = adr[0];
	a2 = adr + 5 + ((c1 - 0xA6) << 1);
	c1 = *a2;
	if (c1 < 0x80 || c1 == 0xF0 || c1 == 0xB0) *typecode = 3;	/* dim */
	else if (c1 < 0xA0 || (c1 >= 0xF4 && c1 <= 0xFC) || c1 == 0xB1)  *typecode = 4;	/* form, float, integer */
	else *typecode = 17;		/* unknown array type */
	return;
}

int isddtconnected() {return ddtconnected;}

void setddtconnected(int bool) {
	ddtconnected = bool;
}

static void invalidateCallStack() {
	callstackvalid = FALSE;
}

static void validateCallStack() {
	callstackvalid = TRUE;
}

static INT isCallStackValid() {
	return callstackvalid;
}
