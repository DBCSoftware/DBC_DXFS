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
#define INC_CTYPE
#define INC_STDLIB
#define INC_STRING
#define INC_LIMITS
#include "includes.h"
#include "base.h"
#include "dbc.h"
#include "vid.h"
#include "evt.h"
#include "fio.h"
#include "release.h"

#define COMMAND_ALTERFLAGS		1
#define COMMAND_PERMBREAK		2
#define COMMAND_CLEARBREAK		3
#define COMMAND_DISPLAYVAR		4
#define COMMAND_CHANGESRC		5
#define COMMAND_GO				6
#define COMMAND_SETTAB			7
#define COMMAND_HELP			8
#define COMMAND_UPRETURN		9
#define COMMAND_DOWNRETURN		10
#define COMMAND_FINDLABEL		11
#define COMMAND_MODIFYVAR		12
#define COMMAND_CHANGEMOD		13
#define COMMAND_OUTPUTSCN		14
#define COMMAND_VARBREAK		15
#define COMMAND_QUIT			16
#define COMMAND_REMOVEVAR		17
#define COMMAND_SEARCH			18
#define COMMAND_TEMPBREAK		19
#define COMMAND_STEPOUT		20
#define COMMAND_VIEWVAR		21
#define COMMAND_NEXTWIN		22
#define COMMAND_STEPSINGLE		23
#define COMMAND_GOTOLINE		24
#define COMMAND_STEPOVER		25
#define COMMAND_SEARCHAGN		26
#define COMMAND_TOGGLELINE		27
#define COMMAND_TOGGLECASE		28
#define COMMAND_WRITESCN		29
#define COMMAND_PGMCODE		30
#define COMMAND_DATACODE		31
#define COMMAND_INCREASEWIN	32
#define COMMAND_DECREASEWIN	33
#define COMMAND_TOGGLEBREAK	34
#define COMMAND_CLEARGLOBAL	35
#define COMMAND_SLOWEXEC		36
#define COMMAND_SAVEGLOBAL		37
#define COMMAND_RESTGLOBAL		38
#define COMMAND_TOGGLEMORE		39
#define COMMAND_SAVEPREF		40
#define COMMAND_TOGGLERESULT	41
#define COMMAND_TOGGLEVIEW		42
#define COMMAND_SCROLLUP		43
#define COMMAND_SCROLLDOWN		44
#define COMMAND_SCROLLLEFT		45
#define COMMAND_SCROLLRIGHT	46
#define COMMAND_PAGEUP			47
#define COMMAND_PAGEDOWN		48
#define COMMAND_GOTOFIRST		49
#define COMMAND_GOTOEND		50
#define COMMAND_PREVWIN		51
#define COMMAND_VARBRKANYCHG	52

#define GLOBALS_INITIAL		0x01
#define GLOBALS_SAVE			0x02
#define GLOBALS_RESTORE		0x04
#define GLOBALS_CLEAR			0x08

#define PREFERENCE_INITIAL		0x01
#define PREFERENCE_SAVE		0x02
#define PREFERENCE_TABS		0x04
#define PREFERENCE_CASE		0x08
#define PREFERENCE_NUMBERS		0x10
#define PREFERENCE_MORE		0x20
#define PREFERENCE_BREAK		0x80

#define EXECUTE_STEP			0x01
#define EXECUTE_STEPOVER		0x02
#define EXECUTE_STEPOUT		0x04
#define EXECUTE_SLOW			0x08
#define EXECUTE_GO				0x10
#define EXECUTE_QUIT			0x20

#define FIND_NEW				0x01
#define FIND_AGAIN				0x02

#define FINDVAR_DISPLAY		0x01
#define FINDVAR_VIEW			0x02
#define FINDVAR_MODIFY			0x04
#define FINDVAR_BREAK			0x08

/* NOTE: item numbers are limited to 5 decimal digits */
#define SELECT_ALL				0x00000001
/*** CODE: GAP ***/
#define SELECT_EQUAL			0x00000200
#define SELECT_LESS			0x00000400
#define SELECT_GREATER			0x00000800
#define SELECT_VALUE			0x00001000
#define SELECT_CURRENT			0x00002000
#define SELECT_NULL			0x00004000
/*** CODE: GAP ***/
#define SELECT_MAX				0x04000000

#define WIN_FIRSTLINE			0x00000001
#define WIN_FIRSTCOLUMN		0x00000002
#define WIN_LINE				0x00000004
#define WIN_REDISPLAY			0x00000008
#define WIN_CENTERLINE			0x00000010
#define WIN_SCROLLBAR			0x00000020
#define WIN_SETCOL				0x00000040
#define WIN_ACTIVE				0x01000000
#define STATUS_FLAGS			0x02000000
#define COMMAND_LINE			0x04000000
#define INITIALIZE				0xFFFFFFFF
#define WIN_MASK				0x000000FF

#define MAIN_SHIFT				0
#define VIEW_SHIFT				8
#define RESULT_SHIFT			16

#define DEFAULT_RESULTWIN_SIZE	3
#define DEFAULT_VIEWWIN_SIZE	3

#define VAR_DSP				0x001
#define VAR_BRK				0x002
#define VAR_NUM				0x004
#define VAR_TMP				0x008

/* defines if VAR_BRK is set */
#define VAR_EQL				0x010
#define VAR_LES				0x020
#define VAR_GRT				0x040
#define VAR_STR            0x080

/**
 * New with DX 16.2
 * Asked for by PPro
 * Perm break on a variable changing at all
 */
#define VAR_PVBRK          0x100

#define FLG_OFF				0x01
#define FLG_HEX				0x02
#define FLG_WRP				0x04
#define FLG_LLN				0x08
#define FLG_XNM				0x10
#define FLG_ADR				0x20
#define FLG_ADX				0x40

#define VARMAX					32
#define WINWDTH				78
#define PWINMAX				16
#define LINESZ					256
#define LINENUMSZ				7
#define RWINMAX				200

#define MAXTABS 32
#define TEMP_SCREEN	0x00000001
#define TEMP_SAVE		0x00000002
#define TEMP_LISTTAB	0x00000004
#define TEMP_LISTTAB2	0x00000008
#define MENUX_CHAR		0x00000001
#define MENUX_STRING	0x00000002
#define MENUX_BOLD		0x00000004

typedef struct dbgdatainfo {
	UCHAR *data;
	INT size;
} DBGDATAINFO;

typedef struct {
	INT width;
	INT height;
	INT displaylines;
	INT displaycolumns;
	INT maxline;
	INT maxcolumn;
	INT firstline;
	INT firstcolumn;
	INT line;
	INT sourceline;
	INT firstsource;
	INT maxsource;
	CHAR *title;
	void (*loadline)(INT, INT, INT);
	void (*displayline)(INT, INT);
} WININFO; 

typedef struct {  /* source line structure */
	UCHAR line[LINESZ + 4];
	INT size;
	UCHAR bchar;
	UCHAR lchar;
} SRCLINE; 

typedef struct {  /* result line structure */
	UCHAR **line;
	INT size;
} RESULTLINE; 

/* global variables */
extern INT datachn;

/* local variables */
static struct vardef {
	UCHAR **nameptr;
	INT dspoff;
	INT arraycnt;
	INT array[3];
	INT offset;
	INT type;
	UCHAR flag;
} vartable[VARMAX];

static INT brkhi;
static INT brkptr = -1;
static CHAR dbgwork[256];
static INT varhi;
static UCHAR **stptr, **dtptr, **ltptr, **ptptr, **rtptr, **ctptr, **ntptr, **recptr;
static LABELDEF *labeltable;
static OFFSET *rec;
static INT ltsize, rtsize, stsize;
static UCHAR **srclineptr, **resultlineptr;
static SRCLINE *srclines;
static RESULTLINE *resultlines;
static INT dbghandle;
static INT stackptr;
static INT savepcount;
static INT stopline;
static INT stopfile;
static INT classnum = -1;
static INT dspflg;
static INT executeflag;
static INT linenumsize;
static CHAR leftbracket = '[';
static CHAR rightbracket = ']';
static WININFO wininfo[3];
static WININFO lastwininfo[3];
static UCHAR lastgoto[16];
static UCHAR lastvariable[32];
static UCHAR lastlabel[32];
static UCHAR laststring[100];
static UCHAR lastmodule[100];
static UCHAR lastsource[100];
static UCHAR lasttab[100];
static INT lastgotosize;
static INT lastvariablesize;
static INT lastlabelsize;
static INT laststringsize;
static INT lastmodulesize;
static INT lastsourcesize;
static INT lasttabsize;
static CHAR gblfile[100] = "DBGGLBL.CFG";
static CHAR prffile[100] = "DBGPREF.CFG";
static CHAR brkfile[100] = "DBGBRK.CFG";
static INT tabindent[MAXTABS];
static INT maxwidth;
static INT maxheight;
static UCHAR **userscrnptr;
static INT screensize;
static INT window;

/* local functions */
static void dbgalter(void);
static void dbgbreak(INT);
static void dbgcalctabs(void);
static void dbgchardisplay(INT);
static INT dbgcharedit(UCHAR *, INT *, INT, INT, INT, INT);
static INT dbgcharget(INT);
static void dbgcharmaindisplayline(INT, INT);
static void dbgcharput(UCHAR *, INT, INT, INT);
static void dbgcharresultdisplayline(INT, INT);
static void dbgcharresultwin(void);
static void dbgcharroll(INT, INT);
static void dbgcharviewdisplayline(INT, INT);
static void dbgcharviewwin(void);
static void dbgcharwindisplay(INT, INT);
static void dbgclear(void);
static void dbgclosesrc(void);
static void dbgcode(void);
static void dbgdata(void);
static void dbgexecute(INT);
static void dbgfindlabel(void);
static void dbgfindline(void);
static void dbgfindmod(void);
static void dbgfindsrc(void);
static void dbgfindstack(INT);
static void dbgfindstring(INT);
static void dbgfindvar(INT);
static void dbgfunction(INT, INT, INT);
static void dbgglobals(INT);
static void dbghelp(void);
static void dbgitox(INT, CHAR *);
static void dbgloadresult(INT, INT, INT);
static void dbgloadsrc(INT, INT, INT);
static void dbgloadview(INT, INT, INT);
static void dbgopensrc(void);
static void dbgpreference(INT);
static void dbgremovevar(void);
static void dbgresultdata(DBGDATAINFO *, INT);
static void dbgresultline(CHAR *);
static void dbgsetpermvarbrk(void);
static void dbgsettabs(void);
static void dbgtestvalue(void);
static INT dbgstart(void);
static INT dbgfvar(CHAR *);
static void dbgrvar(INT);
static UCHAR *dbggvar(INT, INT *, INT *);
static void dbgnvar(INT, CHAR *);
static void dbgvvar(INT, UCHAR *, INT, INT, DBGDATAINFO *, INT *);
static UCHAR *dbggadr(INT *, INT *);
static INT dbgcharprocess(void);
static void dbgwritescreen(void);
static void scrnback(void);
static INT setvartablevaluetocurrent(INT varnum, UCHAR *varaddress);

/* debug initialization */
void dbginit(INT op, CHAR* fname)
{
	if (op == DBGINIT_GLOBAL) {
		strcpy(gblfile, fname);
		dbgflags |= DBGFLAGS_INITGBL;
	}
	else if (op == DBGINIT_PREFERENCE) {
		strcpy(prffile, fname);
		dbgflags |= DBGFLAGS_INITPRF;
	}
	else if (op == DBGINIT_BREAK) {
		strcpy(brkfile, fname);
		dbgflags |= DBGFLAGS_INITBRK;
	}
	else if (op == DBGINIT_DDT) {
		dbgflags |= DBGFLAGS_DDT;
		ddtinit(fname);
	}
	else if (op == DBGINIT_DDT_SERVER) {
		dbgflags |= DBGFLAGS_DDT;
		ddtinitserver(fname);
	}
}

void vdebug(INT op)
{
	INT i1;

	if (!(dbcflags & DBCFLAG_DEBUG)) {
		dbgaction(FALSE);
		return;
	}
	if (dbgflags & DBGFLAGS_DDT) {
		ddtdebug(op);
		return;
	}
	if (op == DEBUG_RESTORE) {  /* change from debugger screen back to user screen */
		if (dbgflags & DBGFLAGS_SCREEN) scrnback();
		return;
	}
	if (op == DEBUG_INTERRUPT) {
		dbgflags |= DBGFLAGS_CTRL_D;
		dbgaction(TRUE);
		return;
	}
	if (op == DEBUG_EXTRAINFO) {
		dbgflags |= DBGFLAGS_EXTRAINFO;
		return;
	}
	dspflg = 0;
	if (op == DEBUG_CHECK) {
		if (dbgflags & DBGFLAGS_FIRST) {
			if (dbgstart()) {
				dbcflags &= ~DBCFLAG_DEBUG;
				goto vdebugend;
			}
		}
		if (dbgflags & DBGFLAGS_SETBRK) dbgsetbrk(&brkptr);
		if (dbgflags & DBGFLAGS_TSTCALL) dbgtestcall();
		if (dbgflags & DBGFLAGS_TSTRETN) dbgtestreturn();
		if (dbgflags & DBGFLAGS_TSTVALU) dbgtestvalue();
		if (!(dbgflags & (DBGFLAGS_FIRST | DBGFLAGS_SINGLE | DBGFLAGS_CTRL_D | DBGFLAGS_SLOW)))
		{
			if (dbgflags & (DBGFLAGS_SETBRK | DBGFLAGS_TSTCALL | DBGFLAGS_TSTRETN | DBGFLAGS_TSTVALU))
				dbgaction(TRUE);
			else dbgaction(FALSE);
			goto vdebugend;
		}
		if (pgm[pcount] == VERB_DEBUG) goto vdebugend;  /* let debug verb call vdebug */
		if (dbgflags & (DBGFLAGS_FIRST | DBGFLAGS_SINGLE | DBGFLAGS_CTRL_D)) dbgflags &= ~DBGFLAGS_SLOW;
	}
	dbgpcount = pcount;
	if (op == DEBUG_DEBUG) {  /* DEBUG or breakpoint */
		dbgpcount--;
		for (i1 = 0; i1 < brkhi; i1++) {
			if (dbgbrk[i1].pcnt == dbgpcount && getmpgm(dbgbrk[i1].mod) == pgm) {
				if (dbgbrk[i1].type & BREAK_PERMANENT) {
					if (!(dbgbrk[i1].type & BREAK_IGNORE)) brkptr = i1;
				}
				else {
					dbgbrk[i1].mod = -1;
					while (brkhi && dbgbrk[brkhi - 1].mod == -1) brkhi--;
				}
				if (dbgbrk[i1].type & BREAK_IGNORE) {
					if (dbgflags & (DBGFLAGS_FIRST | DBGFLAGS_SINGLE | DBGFLAGS_CTRL_D | DBGFLAGS_SLOW)) dbgaction(TRUE);
					goto vdebugend;
				}
				vbcode = lsvbcode;
				pcount = dbgpcount;
				pgm[pcount] = dbgbrk[i1].verb;
				dbgaction(TRUE);
				dbgflags |= DBGFLAGS_SINGLE;
				goto vdebugend;
			}
		}
		dbgflags &= ~DBGFLAGS_SLOW;
	}
	dbgsetmem();
	if (op == DEBUG_ERROR) {
		dbgflags &= ~DBGFLAGS_SLOW;
		i1 = sizeof(dbgwork) - 1;
		geterrmsg((UCHAR *) dbgwork, &i1, FALSE);
		dbgwork[i1] = '\0';
		dbgresultline(dbgwork);
	}
	dbgflags &= ~(DBGFLAGS_FIRST | DBGFLAGS_SINGLE | DBGFLAGS_CTRL_D);
	dbgaction(FALSE);

	/* verify that we are in the same program module */
	/* pgmmodule is declared in dbc.h and defined in dbcsys.c */
	if (pgmmodule != dbgpgmmod) {
		dbgopendbg(pgmmodule);
/*** REMOVE UNNEEDED BREAK POINTS ??? ***/
/*		brkhi = 0; */
/*		brkptr = -1; */
/*		varhi = 0; */
	}
	dbgdatamod = datamodule;
	if (dbgflags & DBGFLAGS_DBGOPEN) {
		for (i1 = 0; i1 < ptsize && pgmtable[i1].pcnt < dbgpcount; i1++);/*TODO Breaking HERE!*/
		if (op == DEBUG_ERROR) i1--;
		else if ((i1 < ptsize || pgm[dbgpcount] != VERB_STOP /* stop */) && pgmtable[i1].pcnt != dbgpcount) {
			if (!(dbgflags & DBGFLAGS_SLOW)) dbgflags |= DBGFLAGS_SINGLE;
			dbgaction(TRUE);
			goto vdebugend;
		}
		stopline = wininfo[0].line = pgmtable[i1].linenum;
		if (srcfile != pgmtable[i1].stidx) {
			srcfile = pgmtable[i1].stidx;
			dbgopensrc();
			dspflg |= WIN_REDISPLAY << MAIN_SHIFT;
		}
		dspflg |= (WIN_CENTERLINE | WIN_SETCOL) << MAIN_SHIFT;
		stopfile = srcfile;
	}
	/*
	 * New as of 07/22/2011, at the request of PPro.
	 * For character mode debugger only,
	 * If *.dbg file absent, ignore a debug statement
	 */
	/** Temporarily removed 05 JUL 2012 because it has unintended
	 * side effects. And it does not always work as it should. Needs to be
	 * smarter. Revisit for 16.1.2
	 */
	/* Turn back on slightly modified. My testing says it works.
	 * Waiting for Paul V.
	 * 03 Oct 2012
	 */
	else {
		if (op == DEBUG_DEBUG) goto vdebugend;
	}

	if (!(dbgflags & DBGFLAGS_SCREEN)) {
		i1 = screensize;
		vidsavescreen(*userscrnptr, &i1);
		dspflg = INITIALIZE;
		dbgflags |= DBGFLAGS_SCREEN;
	}
	dspflg |= (WIN_REDISPLAY << VIEW_SHIFT) | STATUS_FLAGS;

	stackptr = returnstackcount();
	savepcount = dbgpcount;
	executeflag = 0;
	i1 = dbgcharprocess();
	if (i1 & EXECUTE_QUIT) {
		dbgexit();
		if (i1 & EXECUTE_QUIT) dbcexit(0);
		dbcflags &= ~DBCFLAG_DEBUG;
		goto vdebugend;
	}
	if (i1 == EXECUTE_STEP) dbgflags |= DBGFLAGS_SINGLE;
	else if (i1 == EXECUTE_STEPOVER) {
		saveretptr = returnstackcount();
		dbgflags |= DBGFLAGS_TSTCALL;
	}
	else if (i1 == EXECUTE_STEPOUT) {
		saveretptr = returnstackcount();
		if (saveretptr) {
			saveretptr--;
			dbgflags |= DBGFLAGS_TSTRETN;
		}
		else dbgflags |= DBGFLAGS_SINGLE;
	}
	else if (i1 == EXECUTE_SLOW) dbgflags |= DBGFLAGS_SLOW;
	if (brkptr != -1) dbgflags |= DBGFLAGS_SETBRK;
	if (dbgflags & (DBGFLAGS_SINGLE | DBGFLAGS_SLOW | DBGFLAGS_SETBRK | DBGFLAGS_TSTCALL
			| DBGFLAGS_TSTRETN | DBGFLAGS_TSTVALU)) {
		dbgaction(TRUE);
	}

vdebugend:
	if (dbgflags & DBGFLAGS_TRAP) {
		resettrapmap();
		dbgflags &= ~DBGFLAGS_TRAP;
	}
	return;
}

void dbgexit()
{
	evtdestroy(events[1]);
	if (dbgflags & DBGFLAGS_DDT) {
		if (isddtconnected()) {
			dbctcxputtag(1, "terminate");
			dbctcxput(1);
			dbctcxdisconnect(1);
			setddtconnected(FALSE);
		}
	}
	else scrnback();
}

void dbgconsole(CHAR *s, INT msglength)
{
	if (dbgflags & DBGFLAGS_DDT && isddtconnected() && msglength > 0) {
		CHAR msg[256];
		memset(msg, 0, 256);
		strncpy(msg, s, msglength <= 255 ? msglength : 255);
		dbctcxputtag(1, "c");
		dbctcxputattr(1, "msg", msg);
		dbctcxput(1);
	}
}

void dbgsetbrk(int *bp)
{
	UCHAR *pgmptr;

	dbgflags &= ~DBGFLAGS_SETBRK;
	pgmptr = getmpgm(dbgbrk[*bp].mod);
	if (pgmptr != NULL && pgmptr[dbgbrk[*bp].pcnt] == dbgbrk[*bp].verb) pgmptr[dbgbrk[*bp].pcnt] = VERB_DEBUG;
	else dbgbrk[*bp].mod = -1;
	*bp = -1;
}

void dbgtestcall()
{
	INT i1;

	dbgflags &= ~DBGFLAGS_TSTCALL;
	if (returnstackcount() > saveretptr) dbgflags |= DBGFLAGS_TSTRETN;
	else if (pgmmodule == dbgpgmmod && (dbgflags & DBGFLAGS_DBGOPEN)) {
		dbgpcount = pcount;
		for (i1 = 0; i1 < ptsize && pgmtable[i1].pcnt < dbgpcount; i1++);
		if (i1 < ptsize && pgmtable[i1].pcnt != dbgpcount) dbgflags |= DBGFLAGS_TSTCALL;
		else dbgflags |= DBGFLAGS_SINGLE;
	}
	else dbgflags |= DBGFLAGS_SINGLE;
}

void dbgtestreturn()
{
	if (returnstackcount() <= saveretptr) {
		dbgflags &= ~DBGFLAGS_TSTRETN;
		dbgflags |= DBGFLAGS_SINGLE;
	}
}

static void dbgtestvalue()
{
	INT i1, i2, i3, i4, i5, dmod, off;
	CHAR *ptr;
	UCHAR *adr1;
	DBGDATAINFO datainfo[4];

	i5 = TRUE;
	for (i1 = 0; i1 < varhi; i1++) {
		if (!(vartable[i1].type & (VAR_BRK | VAR_PVBRK))) continue;
		adr1 = dbggvar(i1, &dmod, &off);
		if (adr1 == NULL) continue;
		scanvar(adr1);
		ptr = (CHAR *) *vartable[i1].nameptr;
		while (*(ptr++));
		while (*(ptr++));
		while (*(ptr++));
		if ((vartype & TYPE_CHARACTER) && (vartable[i1].type & VAR_STR)) {  /* character */
			if (fp) {
				i2 = 0;
				if (vartable[i1].flag & FLG_LLN) {
					lp -= fp - 1;
					hl += fp - 1;
				}
			}
			else i2 = -1;
			i3 = (INT) strlen(ptr);
			if (!i3) i2++;
			else if (!i2) {
				i4 = (lp <= i3) ? lp : i3;
				i2 = memcmp(&adr1[hl], ptr, i4);
				if (!i2) i2 = lp - i3;
			}
		}
		else if ((vartype & TYPE_NUMERIC) && !(vartable[i1].type & VAR_STR)) {  /* numeric */
			i3 = dbcflags & DBCFLAG_FLAGS;
			mathop(0x00, (UCHAR *) ptr, adr1, NULL);
			if (dbcflags & DBCFLAG_EQUAL) i2 = 0;
			else if (dbcflags & DBCFLAG_LESS) i2 = -1;
			else i2 = 1;
			dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i3;
		}
		else {
			i5 = FALSE;
			continue;
		}

		if (!i2) i2 = VAR_EQL;
		else if (i2 < 0) i2 = VAR_LES;
		else i2 = VAR_GRT;
		if (vartable[i1].type & i2) {
			dbgnvar(i1, dbgwork);
			datainfo[0].data = (UCHAR *) dbgwork;
			datainfo[0].size = (INT) strlen(dbgwork);
			dbgvvar(i1, adr1, dmod, off, &datainfo[1], &i2);
			dbgresultdata(datainfo, i2);
			if (vartable[i1].type & VAR_PVBRK) {
				// Put the current value of the variable in the vartable
				scanvar(adr1);
				if (!setvartablevaluetocurrent(i1, adr1)) {
					break;
				}
				i5 = FALSE; // TSTVALU stays on
			}
			else dbgrvar(i1);
			dbgflags |= DBGFLAGS_SINGLE;
		}
		else i5 = FALSE;
	}
	if (i5) dbgflags &= ~DBGFLAGS_TSTVALU;
}

static void dbgfunction(INT fnc, INT win, INT cnt)
{
	static INT shift[3] = { MAIN_SHIFT, VIEW_SHIFT, RESULT_SHIFT };

	switch(fnc) {
	case COMMAND_ALTERFLAGS:  /* alter flags */
		dbgalter();
		break;
	case COMMAND_PERMBREAK:  /* set permanent break point */
		dbgbreak(BREAK_PERMANENT);
		break;
	case COMMAND_CLEARBREAK:  /* clear break points */
		dbgclear();
		break;
	case COMMAND_DISPLAYVAR:  /* display variable */
		dbgfindvar(FINDVAR_DISPLAY);
		break;
	case COMMAND_CHANGESRC:  /* change source file */
		dbgfindsrc();
		break;
	case COMMAND_GO:  /* go */
		dbgexecute(EXECUTE_GO);
		break;
	case COMMAND_SETTAB:  /* set tabs */
		dbgsettabs();
		break;
	case COMMAND_HELP:  /* help */
		dbghelp();
		break;
	case COMMAND_UPRETURN:  /* move up call stack */
		dbgfindstack(-1);
		break;
	case COMMAND_DOWNRETURN:  /* move down call stack */
		dbgfindstack(1);
		break;
	case COMMAND_FINDLABEL:  /* find label and change current line if found */
		dbgfindlabel();
		break;
	case COMMAND_MODIFYVAR:  /* modify variable */
		dbgfindvar(FINDVAR_MODIFY);
		break;
	case COMMAND_CHANGEMOD:  /* change module */
		dbgfindmod();
		break;
	case COMMAND_OUTPUTSCN:  /* flip screen */
		vidrestorescreen(*userscrnptr, screensize);
		dbgflags &= ~DBGFLAGS_KEYIN;
		dbgcharget(FALSE);
		dspflg = INITIALIZE;
		break;
	case COMMAND_VARBREAK:  /* break on value */
		dbgfindvar(FINDVAR_BREAK);
		break;
	case COMMAND_QUIT:  /* quit */
		dbgexecute(EXECUTE_QUIT);
		break;
	case COMMAND_REMOVEVAR:  /* remove a variable from view window */
		dbgremovevar();
		break;
	case COMMAND_SEARCH:  /* search for string and change current line if found */
		dbgfindstring(FIND_NEW);
		break;
	case COMMAND_TEMPBREAK:  /* set temporary break point */
		dbgbreak(BREAK_TEMPORARY);
		break;
	case COMMAND_STEPOUT:  /* step out of call */
		dbgexecute(EXECUTE_STEPOUT);
		break;
	case COMMAND_VIEWVAR:  /* view variable */
		dbgfindvar(FINDVAR_VIEW);
		break;
	case COMMAND_NEXTWIN:  /* change window */
		if (++win == 3) win = 0;
		if (win == 1 && !(dbgflags & DBGFLAGS_VIEWWIN)) win = 2;
		if (win == 2 && !(dbgflags & DBGFLAGS_RESULTWIN)) win = 0;
		window = win;
		dspflg |= WIN_ACTIVE;
		break;
	case COMMAND_STEPSINGLE:  /* execute a single step */
		dbgexecute(EXECUTE_STEP);
		break;
	case COMMAND_GOTOLINE:  /* find line number */
		dbgfindline();
		break;
	case COMMAND_STEPOVER:  /* step over call */
		dbgexecute(EXECUTE_STEPOVER);
		break;
	case COMMAND_SEARCHAGN:  /* search for string and change current line if found */
		dbgfindstring(FIND_AGAIN);
		break;
	case COMMAND_TOGGLELINE:  /* toggle display of line numbers */
		dbgpreference(PREFERENCE_NUMBERS);
		break;
	case COMMAND_TOGGLECASE:  /* toggle case sensitivity */
		dbgpreference(PREFERENCE_CASE);
		break;
	case COMMAND_WRITESCN:  /* write screen to disk */
		dbgwritescreen();
		break;
	case COMMAND_PGMCODE:  /* view program area */
		if (dbgflags & DBGFLAGS_EXTRAINFO) dbgcode();
		break;
	case COMMAND_DATACODE:  /* view program area */
		if (dbgflags & DBGFLAGS_EXTRAINFO) dbgdata();
		break;
	case COMMAND_INCREASEWIN:  /* increase current window */
		if (win == 0) {
			if ((dbgflags & DBGFLAGS_RESULTWIN) && wininfo[2].height > 1) wininfo[2].height--;
			else if ((dbgflags & DBGFLAGS_VIEWWIN) && wininfo[1].height > 1) wininfo[1].height--;
			else break;
			wininfo[0].height++;
			dspflg = INITIALIZE;
		}
		else if (win == 1) {
			if (wininfo[0].height > 1) wininfo[0].height--;
			else if ((dbgflags & DBGFLAGS_RESULTWIN) && wininfo[2].height > 1) wininfo[2].height--;
			else break;
			wininfo[1].height++;
			dspflg = INITIALIZE;
		}
		else {
			if (wininfo[0].height > 1) wininfo[0].height--;
			else if ((dbgflags & DBGFLAGS_VIEWWIN) && wininfo[1].height > 1) wininfo[1].height--;
			else break;
			wininfo[2].height++;
			dspflg = INITIALIZE;
		}
		break;
	case COMMAND_DECREASEWIN:  /* decrease current window */
		if (win == 0) {
			if (wininfo[0].height > 1) {
				if (dbgflags & DBGFLAGS_RESULTWIN) wininfo[2].height++;
				else if (dbgflags & DBGFLAGS_VIEWWIN) wininfo[1].height++;
				else break;
				wininfo[0].height--;
				dspflg = INITIALIZE;
			}
		}
		else if (win == 1) {
			if (wininfo[1].height > 1) {
				wininfo[1].height--;
				wininfo[0].height++;
				dspflg = INITIALIZE;
			}
		}
		else {
			if (wininfo[2].height > 1) {
				wininfo[2].height--;
				wininfo[0].height++;
				dspflg = INITIALIZE;
			}
		}
		break;
	case COMMAND_TOGGLEBREAK:
		dbgpreference(PREFERENCE_BREAK);
		break;
	case COMMAND_CLEARGLOBAL:
		dbgglobals(GLOBALS_CLEAR);
		break;
	case COMMAND_SLOWEXEC:  /* execute in (very) slow motion */
		dbgexecute(EXECUTE_SLOW);
		break;
	case COMMAND_SAVEGLOBAL:
		dbgglobals(GLOBALS_SAVE);
		break;
	case COMMAND_RESTGLOBAL:
		dbgglobals(GLOBALS_CLEAR | GLOBALS_RESTORE);
		break;
	case COMMAND_TOGGLEMORE:
		dbgpreference(PREFERENCE_MORE);
		break;
	case COMMAND_SAVEPREF:
		dbgpreference(PREFERENCE_SAVE);
		break;
	case COMMAND_TOGGLERESULT:
		dbgcharresultwin();
		break;
	case COMMAND_TOGGLEVIEW:
		dbgcharviewwin();
		break;
	case COMMAND_SCROLLUP:
		wininfo[win].line -= cnt;
		dspflg |= (WIN_LINE | WIN_SCROLLBAR) << shift[win];
		break;
	case COMMAND_SCROLLDOWN:
		wininfo[win].line += cnt;
		dspflg |= (WIN_LINE | WIN_SCROLLBAR) << shift[win];
		break;
	case COMMAND_SCROLLLEFT:
		wininfo[win].firstcolumn -= cnt;
		dspflg |= (WIN_FIRSTCOLUMN | WIN_SCROLLBAR) << shift[win];
		break;
	case COMMAND_SCROLLRIGHT:
		wininfo[win].firstcolumn += cnt;
		dspflg |= (WIN_FIRSTCOLUMN | WIN_SCROLLBAR) << shift[win];
		break;
	case COMMAND_PAGEUP:
		cnt *= wininfo[win].displaylines;
		wininfo[win].firstline -= cnt;
		wininfo[win].line -= cnt;
		dspflg |= (WIN_FIRSTLINE | WIN_LINE | WIN_SCROLLBAR) << shift[win];
		break;
	case COMMAND_PAGEDOWN:
		cnt *= wininfo[win].displaylines;
		wininfo[win].firstline += cnt;
		wininfo[win].line += cnt;
		dspflg |= (WIN_FIRSTLINE | WIN_LINE | WIN_SCROLLBAR) << shift[win];
		break;
	case COMMAND_GOTOFIRST:
		wininfo[win].firstline = wininfo[win].line = 0;
		dspflg |= (WIN_FIRSTLINE | WIN_LINE | WIN_SCROLLBAR) << shift[win];
		break;
	case COMMAND_GOTOEND:
		wininfo[win].firstline = wininfo[win].line = wininfo[win].maxline - 1;
		dspflg |= (WIN_FIRSTLINE | WIN_LINE | WIN_SCROLLBAR) << shift[win];
		break;
	case COMMAND_PREVWIN:
		if (window-- == 0) window = 2;
		if (window == 2 && !(dbgflags & DBGFLAGS_RESULTWIN)) window = 1;
		if (window == 1 && !(dbgflags & DBGFLAGS_VIEWWIN)) window = 0;
		dspflg |= WIN_ACTIVE;
		break;
	case COMMAND_VARBRKANYCHG:
		dbgsetpermvarbrk();
		break;
	}
}

static INT dbgstart(void)
{
	wininfo[0].title = " DB/C Debugger ";
	wininfo[1].title = " Variables ";
	wininfo[2].title = " Results ";
	wininfo[0].loadline = dbgloadsrc;
	wininfo[1].loadline = dbgloadview;
	wininfo[2].loadline = dbgloadresult;
	wininfo[0].displayline = dbgcharmaindisplayline;
	wininfo[1].displayline = dbgcharviewdisplayline;
	wininfo[2].displayline = dbgcharresultdisplayline;
	lastwininfo[0].firstline = INT_MAX;
	lastwininfo[1].firstline = INT_MAX;
	lastwininfo[2].firstline = INT_MAX;
	lastwininfo[0].line = -1;
	lastwininfo[1].line = -1;
	lastwininfo[2].line = -1;

	events[0] = dbcgetbrkevt();
	/*  input event */
	events[1] = evtcreate();
	if (events[1] < 0) {
		kdscmd(VID_HD);
		kdsdsp("Debugger Initialization Error: unable to create events");
		kdspause(2);
		return(-1);
	}
	dbgflags |= DBGFLAGS_VIEWWIN | DBGFLAGS_RESULTWIN;
	vidgetsize(&maxwidth, &maxheight);
	wininfo[0].height = maxheight - 12;
	wininfo[1].height = DEFAULT_VIEWWIN_SIZE;
	wininfo[2].height = DEFAULT_RESULTWIN_SIZE;
	wininfo[0].width = maxwidth;
	wininfo[1].width = maxwidth;
	wininfo[2].width = maxwidth;
	wininfo[0].maxsource = maxheight - 4;
	wininfo[1].maxsource = VARMAX;
	wininfo[2].maxsource = RWINMAX;

	resultlineptr = memalloc(wininfo[2].maxsource * sizeof(RESULTLINE), 0);
	dbgpreference(PREFERENCE_INITIAL);
	if (dbgflags & DBGFLAGS_INITGBL) dbgglobals(GLOBALS_INITIAL);
	if (dbgflags & DBGFLAGS_INITBRK) dbgglobals(BREAK_INITIAL);
	dbgflags &= ~(DBGFLAGS_INITPRF | DBGFLAGS_INITGBL | DBGFLAGS_INITBRK);
	dbgpgmmod = -1;

	screensize = vidscreensize();
	userscrnptr = memalloc(screensize * sizeof(UCHAR), 0);
	srclineptr = memalloc(wininfo[0].maxsource * sizeof(SRCLINE), 0);
	/* reset memory */
	setpgmdata();
	if (userscrnptr == NULL || srclineptr == NULL || resultlineptr == NULL) {
		memfree(resultlineptr);
		memfree(srclineptr);
		memfree(userscrnptr);
		kdscmd(VID_HD);
		kdsdsp("Debugger Initialization Error: unable to allocate memory for screen");
		kdspause(2);
		return(-1);
	}
	return(0);
}


INT dbgopendbg(INT mod) {
	INT modtype, i1;
	CHAR *ptr, buffer[84];
	DBGINFOSTRUCT dbginfo;

	modtype = getmtype(mod);
	if (dbgflags & DBGFLAGS_DBGOPEN) {
		if (!strcmp(getpname(dbgpgmmod), getpname(mod))) {
			dbgpgmmod = mod;
			if (modtype == MTYPE_CLASS && !(dbgflags & DBGFLAGS_DDT)) {
				ptr = getdname(getdatamod(dbgpgmmod));
				for (i1 = 0; i1 < ctsize && strcmp(ptr, &nametable[classtable[i1].nptr]); i1++);
				if (i1 == ctsize) {
					dbgclosesrc();
					strcpy(buffer, "Unable to locate information for class: ");
					strcat(buffer, ptr);
					dbgerror(ERROR_FAILURE, buffer);
					return RC_ERROR;
				}
				classnum = i1;
			}
			else classnum = -1;
			return 0;
		}
	}
	dbgflags &= ~DBGFLAGS_DBGOPEN;
	if (!(dbgflags & DBGFLAGS_DDT)) dbgclosesrc();
	dbgpgmmod = mod;
	i1 = getdbg(mod, &dbginfo);
	if (i1) {
		return RC_ERROR;
	}
	stptr = dbginfo.stabptr;
	dtptr = dbginfo.dtabptr;
	ltptr = dbginfo.ltabptr;
	ptptr = dbginfo.ptabptr;
	rtptr = dbginfo.rtabptr;
	ctptr = dbginfo.ctabptr;
	ntptr = dbginfo.ntabptr;
	stsize = dbginfo.stabsize;
	dtsize = dbginfo.dtabsize;
	ltsize = dbginfo.ltabsize;
	ptsize = dbginfo.ptabsize;
	rtsize = dbginfo.rtabsize;
	ctsize = dbginfo.ctabsize;
	classnum = dbginfo.classnum;
	dbgflags |= DBGFLAGS_DBGOPEN;
	setpgmdata();
	dbgsetmem();
	srcfile = -1;
	return 0;
}

static void dbgopensrc()
{
	INT i1, recmax;
	OFFSET eofpos;
	CHAR name[MAX_NAMESIZE];
	UCHAR buffer[LINESZ + 4];

	dbgclosesrc();

	strcpy(name, &nametable[sourcetable[srcfile].nametableptr]);
	miofixname(name, ".txt", FIXNAME_EXT_ADD);
	dbghandle = rioopen(name, RIO_M_SRO | RIO_P_SRC | RIO_T_ANY, 3, LINESZ);
	setpgmdata();
	dbgsetmem();
	if (dbghandle < 0) {
		strcpy((CHAR *) buffer, "Unable to open ");
		strcat((CHAR *) buffer, name);
		dbgerror(ERROR_FAILURE, (CHAR *) buffer);
		return;
	}

	rioeofpos(dbghandle, &eofpos);
	if (eofpos != sourcetable[srcfile].size) {  /* source file has been modified */
		strcpy((CHAR *) buffer, name);
		strcat((CHAR *) buffer, " - has been modified since compile");
		dbgerror(ERROR_WARNING, (CHAR *) buffer);
	}

	recmax = sourcetable[srcfile].linecnt;
	recptr = memalloc((recmax + 1) * sizeof(OFFSET), 0);
	/* reset memory */
	setpgmdata();
	dbgsetmem();
	if (recptr == NULL) {
		dbgerror(ERROR_FAILURE, "Not enough memory for the debugger");
		goto dbgopensrc1;
	}

	rec = (OFFSET *) *recptr;
	wininfo[0].maxline = wininfo[0].maxcolumn = 0;
	riosetpos(dbghandle, 0L);
	for (wininfo[0].maxline = 0; wininfo[0].maxline < recmax; wininfo[0].maxline++) {
		i1 = rioget(dbghandle, buffer, LINESZ);
		if (i1 < 0) {
			if (i1 == -1) break;
			dbgerror(ERROR_FAILURE, "Error reading source file");
			goto dbgopensrc1;
		}
		if (i1 > wininfo[0].maxcolumn) wininfo[0].maxcolumn = i1;
		riolastpos(dbghandle, &rec[wininfo[0].maxline]);
	}
	if (!srcfile) rionextpos(dbghandle, &rec[wininfo[0].maxline++]);
	mscitoa(wininfo[0].maxline, name);
	linenumsize = (INT) strlen(name);
	dbgflags |= DBGFLAGS_SRCOPEN;
	return;

dbgopensrc1:
	memfree(recptr);
	recptr = NULL;
	rioclose(dbghandle);
}

static void dbgclosesrc()
{
	wininfo[0].maxline = wininfo[0].maxcolumn = 0;
	wininfo[0].sourceline = -1;
	dspflg |= WIN_REDISPLAY << MAIN_SHIFT;
	if (!(dbgflags & DBGFLAGS_SRCOPEN)) return;
	dbgflags &= ~DBGFLAGS_SRCOPEN;

	memfree(recptr);
	recptr = NULL;
	rioclose(dbghandle);
}

static void dbgloadsrc(INT firstline, INT linecnt, INT flag)
{
	INT i1, i2, i3, start, end;
	UCHAR c1, *pgmptr;

	srclines = (SRCLINE *) *srclineptr;
	if (firstline == wininfo[0].sourceline && !(flag & WIN_SETCOL)) return;

	if (wininfo[0].sourceline == -1 || firstline >= wininfo[0].sourceline + linecnt ||
	   firstline + linecnt <= wininfo[0].sourceline) {
		start = 0;
		end = linecnt;
		wininfo[0].firstsource = 0;
	}
	else if (firstline > wininfo[0].sourceline) {
		i1 = firstline - wininfo[0].sourceline;
		start = linecnt - i1;
		end = linecnt;
		wininfo[0].firstsource += i1;
		if (wininfo[0].firstsource >= wininfo[0].maxsource) wininfo[0].firstsource -= wininfo[0].maxsource;
	}
	else if (firstline < wininfo[0].sourceline) {
		i1 = wininfo[0].sourceline - firstline;
		start = 0;
		end = i1;
		wininfo[0].firstsource -= i1;
		if (wininfo[0].firstsource < 0) wininfo[0].firstsource += wininfo[0].maxsource;
	}
	else start = end = linecnt;
	wininfo[0].sourceline = firstline;

	for (i1 = 0, i2 = wininfo[0].firstsource - 1; i1 < linecnt; firstline++, i1++) {
		if (++i2 == wininfo[0].maxsource) i2 = 0;
		if (i1 >= start && i1 < end) {
			if (dbgflags & DBGFLAGS_SRCOPEN) {
				riosetpos(dbghandle, rec[firstline]);
				i3 = rioget(dbghandle, srclines[i2].line, LINESZ);
				if (i3 < 0) i3 = 0;
			}
			else i3 = 0;
			srclines[i2].size = i3;
			srclines[i2].bchar = 0xFF;
		}
		else if (!(flag & WIN_SETCOL)) continue;

		c1 = 0;
		pgmptr = getmpgm(dbgpgmmod);
		for (i3 = 0; i3 < brkhi; i3++) {
			if (dbgbrk[i3].mod == -1 || getmpgm(dbgbrk[i3].mod) != pgmptr) continue;
			if (pgmtable[dbgbrk[i3].ptidx].linenum == firstline) {
				if (dbgbrk[i3].type & BREAK_PERMANENT) {
					if (dbgbrk[i3].type & BREAK_IGNORE) c1 = '#';
					else c1 = '*';
				}
				else if (dbgbrk[i3].type & BREAK_IGNORE) c1 = '-';
				else c1 = '+';
				break;
			}
		}
		if (pgmptr == pgm && stopfile == srcfile && stopline == firstline) {
			if (c1) c1 = '=';
			else c1 = '>';
		}
		srclines[i2].lchar = srclines[i2].bchar;
		srclines[i2].bchar = c1;
	}
}

#if defined(__GNUC__) && !defined(__DGUX__)
static void dbgloadview(INT firstline, __attribute__ ((unused)) INT linecnt, __attribute__ ((unused)) INT flag)
#else
static void dbgloadview(INT firstline, INT linecnt, INT flag)
#endif
{
	wininfo[1].firstsource = firstline;
}

#if defined(__GNUC__) && !defined(__DGUX__)
static void dbgloadresult(INT firstline, __attribute__ ((unused)) INT linecnt, __attribute__ ((unused)) INT flag)
#else
static void dbgloadresult(INT firstline, INT linecnt, INT flag)
#endif
{
	resultlines = (RESULTLINE *) *resultlineptr;

	wininfo[2].firstsource = wininfo[2].sourceline - wininfo[2].maxline + firstline;
	if (wininfo[2].firstsource < 0) wininfo[2].firstsource += wininfo[2].maxsource;
}

static void dbgglobals(INT flag)
{
	static UCHAR x8130[2] = { 0x81, 0x30 };
	INT i1, i2, i3, i4, arrayflg, arraysize, cnt, firstflg;
	INT handle, lasttype, len, nullflg, offset;
	INT32 x1;
	DOUBLE64 y1;
	CHAR work[64], arraydimension[32], *nameptr;
	UCHAR work2[48], *data0, *adr, *ptr;

	if (flag & (GLOBALS_SAVE | GLOBALS_RESTORE)) {
		len = (INT) strlen(gblfile);
		dspflg |= COMMAND_LINE;
		dbgcharput((UCHAR *) "File:   ", 8, 0, 0);
		if (dbgcharedit((UCHAR *) gblfile, &len, sizeof(gblfile) - 1, 6, 0, 74) != VID_ENTER) return;
		if (!len) return;
		gblfile[len] = 0;
	}
	if (flag & (GLOBALS_INITIAL | GLOBALS_SAVE | GLOBALS_RESTORE)) miofixname(gblfile, ".cfg", FIXNAME_EXT_ADD);

	data0 = getmdata(0);
	if (flag & GLOBALS_SAVE) {
/*** CODE: DEAL WITH NULL VALUES ***/
		handle = rioopen(gblfile, RIO_M_PRP | RIO_P_DBC | RIO_T_STD, 3, RIO_MAX_RECSIZE);
		if (handle < 0) {
			strcpy(dbgwork, "Unable to Create: ");
			strcat(dbgwork, gblfile);
			dbgerror(ERROR_FAILURE, dbgwork);
			return;
		}
		for (cnt = 1; !getglobal(cnt, &nameptr, &offset); cnt++) {
			strcpy((CHAR *) work, nameptr);
			i1 = (INT) strlen(work);
			work[i1++] = ':';
			adr = data0 + offset;
			if (adr[0] >= 0xA6 && adr[0] <= 0xA8) {
				arraydimension[0] = leftbracket;
				for (i2 = 1, i3 = 0, i4 = adr[0] - 0xA6, adr++; i3 <= i4; i3++) {
					mscitoa(llhh(adr), &arraydimension[i2]);
					i2 += (INT) strlen(&arraydimension[i2]);
					adr += 2;
				}
				arraydimension[i2] = rightbracket;
				arraydimension[i2 + 1] = 0;
				arraysize = llhh(adr);
				adr += 3;
				arrayflg = TRUE;
			}
			else arrayflg = FALSE;
			firstflg = TRUE;
			do {
				scanvar(adr);
				if (!(vartype & (TYPE_CHAR | TYPE_NUMVAR))) break;

				if (firstflg) {
					firstflg = FALSE;
					if (vartype & (TYPE_CHAR | TYPE_NUM)) {
						len = pl;
						if (vartype & TYPE_CHAR) work[i1] = 'C';
						else work[i1] = 'N';
						while (adr[hl + len] == 0xF3) len++;
					}
					else if (vartype & TYPE_INT) {
						work[i1] = 'I';
						len = adr[1] & ~0x80;
					}
					else {
						work[i1] = 'F';
						len = ((adr[0] << 3) & 0x18) | (adr[1] >> 5);
					}
					mscitoa(len, &work[++i1]);
					i1 += (INT) strlen(&work[i1]);
					if (vartype & TYPE_FLOAT) {
						if ( (len = adr[1] & 0x1F) ) {
							work[i1++] = '.';
							mscitoa(len, &work[i1]);
							i1 += (INT) strlen(&work[i1]);
						}
					}
					if (arrayflg) {
						strcpy(&work[i1], arraydimension);
						i1 += (INT) strlen(arraydimension);
					}
					i2 = rioput(handle, (UCHAR *) work, i1);
					if (i2) {
						dbgerror(ERROR_FAILURE, "Unable to Write to Save File");
						goto global1;
					}
				}

				ptr = adr;
				if (vartype & TYPE_NUMVAR) {
					lasttype = vartype;
					if (vartype & (TYPE_INT | TYPE_FLOAT | TYPE_NULL)) {
						nvmkform(adr, work2);
						ptr = work2;
						vartype = lasttype;
					}
					if (vartype & TYPE_FLOAT) {
						fp = ((adr[0] << 3) & 0x18) | (adr[1] >> 5);
						lp = adr[1] & 0x1F;
					}
					else fp = lp = 0;
				}

				if (vartype & TYPE_NULL) work[0] = '#';
				else work[0] = '=';
				mscitoa(fp, &work[1]);
				i1 = (INT) strlen(work);
				work[i1++] = ',';
				mscitoa(lp, &work[i1]);
				i1 += (INT) strlen(&work[i1]);
				work[i1++] = ',';
				mscitoa(pl, &work[i1]);
				i1 += (INT) strlen(&work[i1]);
				work[i1++] = ',';
				i2 = rioparput(handle, (UCHAR *) work, i1);
				if (i2) {
					dbgerror(ERROR_FAILURE, "Unable to Write to Save File");
					goto global1;
				}
				i2 = rioput(handle, &ptr[hl], pl);
				if (i2) {
					dbgerror(ERROR_FAILURE, "Unable to Write to Save File");
					goto global1;
				}
				adr += arraysize;
			} while (arrayflg && adr[0] != 0xA5);
		}
global1:
		rioclose(handle);
	}
	if (flag & GLOBALS_CLEAR) {
		for (cnt = 1; !getglobal(cnt, &nameptr, &offset); cnt++) {
			adr = data0 + offset;
			if (adr[0] >= 0xA6 && adr[0] <= 0xA8) {
				adr += adr[0] - 0xA6 + 3;
				arraysize = llhh(adr);
				adr += 3;
				arrayflg = TRUE;
			}
			else arrayflg = FALSE;
			do {
				scanvar(adr);
				if (vartype & TYPE_CHAR) {
					memset(&adr[hl], ' ', pl);
					fp = lp = 0;
					setfplp(adr);
				}
				else if (vartype & TYPE_NUMVAR) {
					i2 = dbcflags & DBCFLAG_FLAGS;
					movevar(x8130, adr);
					dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i2;
				}
				else {
/*** CODE: CLOSE IMAGES, PFILE, ETC ***/
				}
				adr += arraysize;
			} while (arrayflg && adr[0] != 0xA5);
		}
		dspflg |= WIN_REDISPLAY << VIEW_SHIFT;
	}
	if (flag & (GLOBALS_INITIAL | GLOBALS_RESTORE)) {  /* Note: this destroys contents of record */
/*** CODE: DEAL WITH NULL VALUES ***/
		handle = rioopen(gblfile, RIO_M_SRO | RIO_P_DBC | RIO_T_STD, 3, RIO_MAX_RECSIZE);
		if (handle < 0) {
			strcpy(dbgwork, "Unable to Open: ");
			strcat(dbgwork, gblfile);
			dbgerror(ERROR_FAILURE, dbgwork);
			return;
		}
		lasttype = 0;
		while ((i2 = rioget(handle, dbcbuf, RIO_MAX_RECSIZE)) >= 0) {
			dbcbuf[i2] = 0;
			if (dbcbuf[0] != '=' && dbcbuf[0] != '#') {  /* name of variable */
				lasttype = 0;
				for (i1 = 0; dbcbuf[i1] && dbcbuf[i1] != ':'; i1++);
				if (dbcbuf[i1++] != ':') goto global2;
				if (dbcbuf[i1] == 'C') lasttype = TYPE_CHAR;
				else if (dbcbuf[i1] == 'N') lasttype = TYPE_NUM;
				else if (dbcbuf[i1] == 'I') lasttype = TYPE_INT;
				else if (dbcbuf[i1] == 'F') lasttype = TYPE_FLOAT;
				else goto global2;
				work2[0] = 0xB7;
				i2 = makevar((CHAR *) dbcbuf, work2, MAKEVAR_VARIABLE | MAKEVAR_NAME | MAKEVAR_GLOBAL);
				if (i2) {
					if (i2 == 1) {
						strcpy(dbgwork, "Conflict with Existing Global Variable: ");
						strcat(dbgwork, (CHAR *) dbcbuf);
						dbgerror(ERROR_FAILURE, dbgwork);
						lasttype = 0;
						continue;
					}
					else {
						dbgerror(ERROR_FAILURE, "Not Enough Memory for Global Variables");
						goto global3;
					}
				}
				adr = data0 + llmmhh(&work2[3]);
				if (adr[0] >= 0xA6 && adr[0] <= 0xA8) {
					adr += adr[0] - 0xA6 + 3;
					arraysize = llhh(adr);
					adr += 3;
					arrayflg = TRUE;
				}
				else arrayflg = FALSE;
			}
			else {  /* value of variable */
				if (!lasttype) {
					strcpy(dbgwork, "No Variable for Data: ");
					strcat(dbgwork, (CHAR *) dbcbuf);
					dbgerror(ERROR_FAILURE, dbgwork);
					continue;
				}
				scanvar(adr);
				if (vartype != lasttype) {
					strcpy(dbgwork, "Mismatch in Data Type: ");
					strcat(dbgwork, (CHAR *) dbcbuf);
					dbgerror(ERROR_FAILURE, dbgwork);
					lasttype = 0;
					continue;
				}
				len = pl;
				nullflg = (dbcbuf[0] == '#');
				for (i1 = 1; dbcbuf[i1] && dbcbuf[i1] != ','; i1++);
				if (!dbcbuf[i1]) goto global2;
				mscntoi(dbcbuf, &fp, i1);
				for (i3 = ++i1; dbcbuf[i1] && dbcbuf[i1] != ','; i1++);
				if (!dbcbuf[i1] || i1 == i3) goto global2;
				mscntoi(&dbcbuf[i3], &lp, i1 - i3);
				for (i3 = ++i1; dbcbuf[i1] && dbcbuf[i1] != ','; i1++);
				if (!dbcbuf[i1] || i1 == i3) goto global2;
				mscntoi(&dbcbuf[i3], &pl, i1 - i3);
				if (i2 - ++i1 != pl) goto global2;
				ptr = &dbcbuf[i1];
				if (vartype & (TYPE_CHAR | TYPE_NUM)) {
					if (pl > len) {
						while (pl > len && adr[hl + len] == 0xF3) len++;
						if (pl > len) {
							strcpy(dbgwork, "Data Longer than Variable: ");
							strcat(dbgwork, (CHAR *) dbcbuf);
							dbgerror(ERROR_FAILURE, dbgwork);
							continue;
						}
					}
					else if (pl < len) while (pl < len--) adr[hl + len] = 0xF3;
					if (vartype & TYPE_CHAR) {
						setfplp(adr);
						if (adr[0] < 0x80) adr[2] = (UCHAR) pl;
						else {
							adr[5] = (UCHAR) pl;
							adr[6] = (UCHAR)(pl >> 8);
						}
					}
					else adr[0] = (UCHAR)(0x80 | pl);
				}
				else if (vartype & TYPE_INT) {
					ptr = work2;
					ptr[0] = (UCHAR) pl;
					x1 = (int) strtol((CHAR *) &dbcbuf[i1], NULL, 10);
					memcpy(&ptr[1], (UCHAR *) &x1, sizeof(INT32));
					pl = 1 + sizeof(INT32);
					hl = 1;
				}
				else if (vartype & TYPE_FLOAT) {
					ptr = work2;
					ptr[0] = (UCHAR)(0xF8 | (fp >> 3));
					ptr[1] = (UCHAR)((fp << 5) | lp);
					y1 = atof((CHAR *) &dbcbuf[i1]);
					memcpy(&ptr[2], (UCHAR *) &y1, sizeof(DOUBLE64));
					pl = 2 + sizeof(DOUBLE64);
					hl = 0;
				}
				memcpy(&adr[hl], ptr, pl);
				if (nullflg) setnullvar(adr, TRUE);
				adr += arraysize;
				if (!arrayflg || adr[0] == 0xA5) lasttype = 0;
			}
			continue;
global2:
			strcpy(dbgwork, "Invalid Record: ");
			strcat(dbgwork, (CHAR *) dbcbuf);
			dbgerror(ERROR_FAILURE, dbgwork);
		}
		if (i2 != -1) dbgerror(ERROR_FAILURE, "Unable to Read from Save File");
global3:
		rioclose(handle);
		dspflg |= WIN_REDISPLAY << VIEW_SHIFT;
	}
	return;
}

static void dbgpreference(INT flag)
{
	INT i1, i2, handle, len;
	UCHAR work[132];
	static INT w0h = 0, w0v = 0, w0x = 0, w0y = 0, w1h = 0, w1v = 0;
	static INT w1x = 0, w1y = 0, w2h = 0, w2v = 0, w2x = 0, w2y = 0; 
	
	if (flag & PREFERENCE_SAVE) {
		len = (INT) strlen(prffile);
		dspflg |= COMMAND_LINE;
		dbgcharput((UCHAR *) "File:   ", 8, 0, 0);
		if (dbgcharedit((UCHAR *) prffile, &len, sizeof(prffile) - 1, 6, 0, 74) != VID_ENTER) return;
		if (!len) return;
		prffile[len] = 0;
	}
	if (flag & (PREFERENCE_INITIAL | PREFERENCE_SAVE)) miofixname(prffile, ".cfg", FIXNAME_EXT_ADD);

	if (flag & PREFERENCE_TABS) dbgsettabs();
	if (flag & PREFERENCE_CASE) {
		dbgflags ^= DBGFLAGS_SENSITIVE;
		if (dbgflags & DBGFLAGS_SENSITIVE) dbgresultline("Variables/Labels Case Sensitive");
		else dbgresultline("Variables/Labels Case Insensitive");
	}
	if (flag & PREFERENCE_NUMBERS) {
		dbgflags ^= DBGFLAGS_LINENUM;
		dspflg |= WIN_REDISPLAY << MAIN_SHIFT;
	}
	if (flag & PREFERENCE_MORE) {
		dbgflags ^= DBGFLAGS_MORE;
		if (dbgflags & DBGFLAGS_MORE) dbgresultline("Page Results");
		else dbgresultline("Do Not Page Results");
	}
	if (flag & PREFERENCE_BREAK) {
		dbgflags ^= DBGFLAGS_MAINTBREAK;
	}
	if (flag & PREFERENCE_SAVE) {
		handle = rioopen(prffile, RIO_M_PRP | RIO_P_DBC | RIO_T_STD, 1, 128);
		if (handle < 0) {
			strcpy(dbgwork, "Unable to Create: ");
			strcat(dbgwork, prffile);
			dbgerror(ERROR_FAILURE, dbgwork);
			return;
		}
		i1 = 0;
		if (dbgflags & DBGFLAGS_SENSITIVE) {
			work[0] = 'C';
			i1 = rioput(handle, work, 1);
		}
		if (!i1 && (dbgflags & DBGFLAGS_LINENUM)) {
			work[0] = 'N';
			i1 = rioput(handle, work, 1);
		}
		if (!i1 && lasttabsize) {
			work[0] = 'T';
			memcpy(&work[1], lasttab, lasttabsize);
			i1 = rioput(handle, work, lasttabsize + 1);
		}
		if (!i1 && (dbgflags & DBGFLAGS_MORE)) {
			work[0] = 'M';
			i1 = rioput(handle, work, 1);
		}
		if (!i1 && (dbgflags & DBGFLAGS_MAINTBREAK)) {
			work[0] = 'B';
			i1 = rioput(handle, work, 1);
		}
		if (!i1 && !(dbgflags & DBGFLAGS_RESULTWIN)) {
			work[0] = 'R';
			i1 = rioput(handle, work, 1);
		}
		if (!i1 && !(dbgflags & DBGFLAGS_VIEWWIN)) {
			work[0] = 'V';
			i1 = rioput(handle, work, 1);
		}
		if (!i1) { /* store gui window sizes */
			len = 0;
			work[len++] = 'W';
			len += mscitoa(w0h, (CHAR *) &work[len]);
			work[len++] = ',';
			len += mscitoa(w0v, (CHAR *) &work[len]);
			work[len++] = ',';
			len += mscitoa(w0x, (CHAR *) &work[len]);
			work[len++] = ',';
			len += mscitoa(w0y, (CHAR *) &work[len]);
			work[len++] = ',';						
			len += mscitoa(w1h, (CHAR *) &work[len]);
			work[len++] = ',';
			len += mscitoa(w1v, (CHAR *) &work[len]);
			work[len++] = ',';
			len += mscitoa(w1x, (CHAR *) &work[len]);
			work[len++] = ',';
			len += mscitoa(w1y, (CHAR *) &work[len]);
			work[len++] = ',';	
			len += mscitoa(w2h, (CHAR *) &work[len]);
			work[len++] = ',';
			len += mscitoa(w2v, (CHAR *) &work[len]);
			work[len++] = ',';
			len += mscitoa(w2x, (CHAR *) &work[len]);
			work[len++] = ',';
			len += mscitoa(w2y, (CHAR *) &work[len]);
			i1 = rioput(handle, work, len);	
		}
		if (!i1) { /* store non-gui window sizes */
			len = 0;
			work[len++] = 'X';
			len += mscitoa(wininfo[1].height, (CHAR *) &work[len]);
			work[len++] = ',';
			len += mscitoa(wininfo[2].height, (CHAR *) &work[len]);
			i1 = rioput(handle, work, len);	
		}
		if (i1) dbgerror(ERROR_FAILURE, "Unable to Write to Preference File");
		rioclose(handle);
	}
	if (flag & PREFERENCE_INITIAL) {
		handle = rioopen(prffile, RIO_M_SRO | RIO_P_DBC | RIO_T_STD, 1, 128);
		if (handle >= 0) {
			dbgflags &= ~(DBGFLAGS_SENSITIVE | DBGFLAGS_LINENUM | DBGFLAGS_MORE | DBGFLAGS_MAINTBREAK);
			while ((i1 = rioget(handle, work, 128)) >= 0) {
				work[i1] = 0;
				if (work[0] == 'C') dbgflags |= DBGFLAGS_SENSITIVE;
				else if (work[0] == 'N') dbgflags |= DBGFLAGS_LINENUM;
				else if (work[0] == 'T') {
					strcpy((CHAR *) lasttab, (CHAR *) &work[1]);
					lasttabsize = (INT) strlen((CHAR *) lasttab);
				}
				else if (work[0] == 'M') dbgflags |= DBGFLAGS_MORE;
				else if (work[0] == 'B') dbgflags |= DBGFLAGS_MAINTBREAK;
				else if (work[0] == 'R') {
					dbgflags &= ~DBGFLAGS_RESULTWIN;
				}
				else if (work[0] == 'V') { 
					dbgflags &= ~DBGFLAGS_VIEWWIN;
				}
				else if (work[0] == 'W') { /* load gui window sizes */
					len = 1;
					for (len = 1, i2 = 0; isdigit(work[len]); len++) i2 = i2 * 10 + work[len] - '0';
					w0h = i2;
					for (len++, i2 = 0; isdigit(work[len]); len++) i2 = i2 * 10 + work[len] - '0';
					w0v = i2;
					for (len++, i2 = 0; isdigit(work[len]); len++) i2 = i2 * 10 + work[len] - '0';
					w0x = i2;
					for (len++, i2 = 0; isdigit(work[len]); len++) i2 = i2 * 10 + work[len] - '0';
					w0y = i2;
					for (len++, i2 = 0; isdigit(work[len]); len++) i2 = i2 * 10 + work[len] - '0';
					w1h = i2;
					for (len++, i2 = 0; isdigit(work[len]); len++) i2 = i2 * 10 + work[len] - '0';
					w1v = i2;
					for (len++, i2 = 0; isdigit(work[len]); len++) i2 = i2 * 10 + work[len] - '0';
					w1x = i2;
					for (len++, i2 = 0; isdigit(work[len]); len++) i2 = i2 * 10 + work[len] - '0';
					w1y = i2;
					for (len++, i2 = 0; isdigit(work[len]); len++) i2 = i2 * 10 + work[len] - '0';
					w2h = i2;
					for (len++, i2 = 0; isdigit(work[len]); len++) i2 = i2 * 10 + work[len] - '0';
					w2v = i2;
					for (len++, i2 = 0; isdigit(work[len]); len++) i2 = i2 * 10 + work[len] - '0';
					w2x = i2;
					for (len++, i2 = 0; isdigit(work[len]); len++) i2 = i2 * 10 + work[len] - '0';
					w2y = i2;
				}
				else if (work[0] == 'X') { /* load non-gui window sizes */
					len = 1;
					for (len = 1, i2 = 0; isdigit(work[len]); len++) i2 = i2 * 10 + work[len] - '0';
					if (dbgflags & DBGFLAGS_VIEWWIN) {
						if (i2 > DEFAULT_VIEWWIN_SIZE) {
							i2 -= DEFAULT_VIEWWIN_SIZE;
							if ((wininfo[0].height - i2) >= 1) wininfo[0].height -= i2;
							else if ((dbgflags & DBGFLAGS_RESULTWIN) && (wininfo[2].height - i2) >= 1) {
								wininfo[2].height -= i2;
							}
							else i2 = 0;
							if (i2) wininfo[1].height += i2;
						}
						else if (i2 < DEFAULT_VIEWWIN_SIZE && i2 > 0) {
							i2 = DEFAULT_VIEWWIN_SIZE - i2;
							wininfo[0].height += i2;
							wininfo[1].height -= i2;
						}
					}
					else wininfo[0].height += (DEFAULT_VIEWWIN_SIZE + 1); 
					for (len++, i2 = 0; isdigit(work[len]); len++) i2 = i2 * 10 + work[len] - '0';
					if (dbgflags & DBGFLAGS_RESULTWIN) {
						if (i2 > DEFAULT_RESULTWIN_SIZE) {
							i2 -= DEFAULT_RESULTWIN_SIZE;
							if ((wininfo[0].height - i2) >= 1) wininfo[0].height -= i2;
							else if ((dbgflags & DBGFLAGS_VIEWWIN) && (wininfo[1].height - i2) >= 1) {
								wininfo[1].height -= i2;
							}
							else i2 = 0;
							if (i2) wininfo[2].height += i2;
						}
						else if (i2 < DEFAULT_RESULTWIN_SIZE && i2 > 0) {
							i2 = DEFAULT_RESULTWIN_SIZE - i2;
							wininfo[0].height += i2;
							wininfo[2].height -= i2;
						}					
					}
					else wininfo[0].height += (DEFAULT_RESULTWIN_SIZE + 1);
					
					if (wininfo[0].height <= 0) wininfo[0].height = 1;
					if (wininfo[1].height <= 0) wininfo[1].height = 1;
					if (wininfo[2].height <= 0) wininfo[2].height = 1;
				}
			}
			rioclose(handle);
		}
		else {
			if (handle != ERR_FNOTF || (dbgflags & DBGFLAGS_INITPRF)) {
				strcpy(dbgwork, "Unable to Open: ");
				strcat(dbgwork, prffile);
				dbgerror(ERROR_FAILURE, dbgwork);
			}
			dbgflags &= ~DBGFLAGS_SENSITIVE;
			dbgflags |= DBGFLAGS_LINENUM;
			strcpy((CHAR *) lasttab, "10,20:4");
			lasttabsize = 7;
		}

		dbgcalctabs();
		dspflg |= WIN_REDISPLAY << MAIN_SHIFT;
	}
	return;
}

static void dbgexecute(INT flag)
{
	executeflag = flag;
	return;
}

static void dbgbreak(INT flag)
{
	INT i1, i2, i3;
	CHAR *msg;
	UCHAR *ptr, *pgmptr;

	if (flag & (BREAK_PERMANENT | BREAK_TEMPORARY)) {
		/* set permanent or temporary break point */
		if (!dbgflags & DBGFLAGS_DBGOPEN) {
			dbgerror(ERROR_FAILURE, ".DBG Information not loaded");
			return;
		}

		for (i1 = 0; i1 < ptsize && (pgmtable[i1].linenum != wininfo[0].line || pgmtable[i1].stidx != srcfile); i1++);
		if (i1 == ptsize) {
			dbgerror(ERROR_FAILURE, "Current Line is Invalid for a Breakpoint");
			return;
		}
		if (classnum != -1) {
			if (pgmtable[i1].pcnt < classtable[classnum].spcnt || pgmtable[i1].pcnt >= classtable[classnum].epcnt) {
				dbgerror(ERROR_FAILURE, "Current Line is not in the Current Class");
				return;
			}
		}
		else {
			for (i2 = 0; i2 < ctsize; i2++) {
				if (pgmtable[i1].pcnt >= classtable[i2].spcnt && pgmtable[i1].pcnt < classtable[i2].epcnt) {
					dbgerror(ERROR_FAILURE, "Current Line is not in the Main Code");
					return;
				}
			}
		}

		if ((pgmptr = getmpgm(dbgpgmmod)) == NULL) return;
		i2 = brkhi;
		for (i3 = 0; i3 < brkhi; i3++) {
			if (dbgbrk[i3].mod == -1) {
				if (i2 == brkhi) i2 = i3;
			}
			else if (dbgbrk[i3].pcnt == pgmtable[i1].pcnt && getmpgm(dbgbrk[i3].mod) == pgmptr) break;
		}
		if (i3 < brkhi) {  /* found existing breakpoint */
			if (((UCHAR) flag & BREAK_PERMANENT) == (dbgbrk[i3].type & BREAK_PERMANENT)) {
				if (dbgbrk[i3].type & BREAK_IGNORE) msg = "Breakpoint Set";
				else {
					if (dbgbrk[i3].pcnt == dbgpcount && pgmptr == pgm) brkptr = -1;
					pgmptr += dbgbrk[i3].pcnt;
					*pgmptr = dbgbrk[i3].verb;
					msg = "Breakpoint Cleared";
				}
				dbgbrk[i3].mod = -1;
				if (i3 == brkhi - 1) brkhi = i3;
			}
			else {
				dbgbrk[i3].type ^= BREAK_PERMANENT;
				msg = "Breakpoint Changed";
			}
		}
		else {
			if (i2 == DEBUG_BRKMAX) {
				dbgerror(ERROR_FAILURE, "Too Many Breakpoints");
				return;
			}

			ptr = pgmptr + pgmtable[i1].pcnt;
			dbgbrk[i2].pcnt = pgmtable[i1].pcnt;
			dbgbrk[i2].ptidx = i1;
			dbgbrk[i2].mod = dbgpgmmod;
			dbgbrk[i2].verb = *ptr;
			dbgbrk[i2].type = (UCHAR)(flag & BREAK_PERMANENT);
			if (*ptr == VERB_DEBUG) {
				dbgbrk[i2].type |= BREAK_IGNORE;
				msg = "Breakpoint Cleared";
			}
			else {
				if (dbgbrk[i2].pcnt == dbgpcount && pgmptr == pgm) brkptr = i2;
				else *ptr = VERB_DEBUG;
				msg = "Breakpoint Set";
			}
			if (i2 == brkhi) brkhi++;
		}
		dbgresultline(msg);
	}
	dspflg |= WIN_SETCOL << MAIN_SHIFT;
	return;
}

static void dbgclear()
{
	INT i1, i2, i3, i4;
	CHAR *ptr;
	UCHAR work[sizeof(dbgwork)], *pgmptr;
	INT32 vidcmd[2];

	i3 = 0;

	for (i1 = 0; i1 < brkhi; i1++) {
		if (dbgbrk[i1].mod == -1) continue;
		if (!(i3 & SELECT_ALL)) {
/*** CODE: STORE PROGRAM AND LINE IN DBGBRK INSTEAD ***/
			if (dbgbrk[i1].mod == dbgpgmmod) {
				strcpy(dbgwork, &nametable[sourcetable[pgmtable[dbgbrk[i1].ptidx].stidx].nametableptr]);
				i2 = (INT) strlen(dbgwork);
				dbgwork[i2++] = ':';
				mscitoa(pgmtable[dbgbrk[i1].ptidx].linenum + 1, &dbgwork[i2]);
			}
			else {
/*** CODE: DBGBRK NEEDS TO STORE MORE INFORMATION SO BREAKS ARE SUPPORTED ON ***/
/***       MODULES THAT ARE UNLOADED AND LOADED AGAIN ***/
				strcpy(dbgwork, getpname(dbgbrk[i1].mod));
				i2 = (INT) strlen(dbgwork);
				dbgwork[i2++] = '%';
				mscitoa(dbgbrk[i1].pcnt, &dbgwork[i2]);
			}
			dspflg |= COMMAND_LINE;
			strcpy((CHAR *) work, "Remove \"");
			if (strlen(dbgwork) > 57) strcpy(&dbgwork[54], "...");
			strcat((CHAR *) work, dbgwork);
			strcat((CHAR *) work, "\" (Y/N/All): ");
			dbgcharput(work, (INT) strlen((CHAR *) work), 0, 0);
			vidcmd[0] = VID_EL;
			vidcmd[1] = VID_END_NOFLUSH;
			vidput(vidcmd);
			i2 = dbgcharget(TRUE);
			if (i2 == VID_ESCAPE) return;
			if (i2 == 'A' || i2 == 'a') i3 = SELECT_ALL;
			else if (i2 != 'Y' && i2 != 'y') continue;
		}
		pgmptr = getmpgm(dbgbrk[i1].mod);
		if (pgmptr != NULL) {
			if (!(dbgbrk[i1].type & BREAK_IGNORE) &&
			    dbgbrk[i1].pcnt == dbgpcount && pgmptr == pgm) brkptr = -1;
			pgmptr += dbgbrk[i1].pcnt;
			*pgmptr = dbgbrk[i1].verb;
		}
		dbgbrk[i1].mod = -1;
		if (!(i3 & SELECT_ALL)) {
			dbgchardisplay(WIN_SETCOL << MAIN_SHIFT);
		}
		else dspflg |= WIN_SETCOL << MAIN_SHIFT;
	}
	while (brkhi && dbgbrk[brkhi - 1].mod == -1) brkhi--;

	if (dbgflags & DBGFLAGS_TSTVALU) {
		dbgflags &= ~DBGFLAGS_TSTVALU;
		for (i1 = 0; i1 < varhi; i1++) {
			//if (!(vartable[i1].type & VAR_BRK)) continue;
			if (!(vartable[i1].type & (VAR_BRK | VAR_PVBRK))) continue;
			if (!(i3 & SELECT_ALL)) {
				dbgnvar(i1, dbgwork);
				i2 = (INT) strlen(dbgwork);
				dbgwork[i2++] = ' ';
				if (vartable[i1].type & VAR_LES) dbgwork[i2++] = '<';
				if (vartable[i1].type & VAR_GRT) dbgwork[i2++] = '>';
				if (vartable[i1].type & VAR_EQL) dbgwork[i2++] = '=';
				dbgwork[i2++] = ' ';
				ptr = (CHAR *) *vartable[i1].nameptr;
				while (*(ptr++));
				while (*(ptr++));
				while (*(ptr++));
				if (!(vartable[i1].type & VAR_STR)) ptr++;
				i4 = (INT) strlen(ptr);
				if (i4 > (INT) sizeof(dbgwork) - i2) {
					i4 = sizeof(dbgwork) - i2 - 3;
					memcpy(&dbgwork[i2], ptr, i4);
					i2 += i4;
					ptr = "...";
					i4 = 3;
				}
				memcpy(&dbgwork[i2], ptr, i4);
				i2 += i4;
				dspflg |= COMMAND_LINE;
				strcpy((CHAR *) work, "Remove \"");
				if (i2 > 57) strcpy(&dbgwork[54], "...");
				else dbgwork[i2] = 0;
				strcat((CHAR *) work, dbgwork);
				strcat((CHAR *) work, "\" (Y/N/All): ");
				dbgcharput(work, (INT) strlen((CHAR *) work), 0, 0);
				vidcmd[0] = VID_EL;
				vidcmd[1] = VID_END_NOFLUSH;
				vidput(vidcmd);
				i2 = dbgcharget(TRUE);
				if (i2 == VID_ESCAPE) return;
				if (i2 == 'A' || i2 == 'a') i3 = SELECT_ALL;
				else if (i2 != 'Y' && i2 != 'y') {
					dbgflags |= DBGFLAGS_TSTVALU;
					continue;
				}
			}
			dbgrvar(i1);
		}
	}

	return;
}

static void dbgfindstring(INT flag)
{
	INT i1, i2, linenum;
	CHAR c1, buffer[LINESZ + 4];

	if (!(dbgflags & DBGFLAGS_SRCOPEN)) return;

	if (flag & FIND_NEW) {
		dspflg |= COMMAND_LINE;
		dbgcharput((UCHAR *) "String: ", 8, 0, 0);
		if (dbgcharedit(laststring, &laststringsize, sizeof(laststring), 8, 0, 72) != VID_ENTER) return;
	}
	if (!laststringsize) return;

	c1 = laststring[0];
	i1 = laststringsize - 1;
	linenum = wininfo[0].line;
	do {
		if (++linenum == sourcetable[srcfile].linecnt) linenum = 0;
		riosetpos(dbghandle, rec[linenum]);
		i2 = rioget(dbghandle, (UCHAR *) buffer, LINESZ);
		if (i2 <= 0) continue;
		i2 -= i1;
		while (i2-- > 0) {
			if (c1 == buffer[i2] && !memcmp(&buffer[i2 + 1], &laststring[1], i1)) {
				wininfo[0].line = linenum;
				dspflg |= WIN_CENTERLINE << MAIN_SHIFT;
				window = 0;
				dspflg |= WIN_ACTIVE;
				return;
			}
		}
	} while (linenum != wininfo[0].line);

	memcpy(dbgwork, laststring, laststringsize);
	strcpy(&dbgwork[laststringsize], ": String Not Found");
	dbgerror(ERROR_WARNING, dbgwork);
	return;
}

static void dbgfindlabel()
{
	INT i1, i2, i3, pgmmodsave, pcountsave;

	if (!(dbgflags & DBGFLAGS_DBGOPEN)) {
		dbgerror(ERROR_FAILURE, ".DBG Information not loaded");
		return;
	}

	dspflg |= COMMAND_LINE;
	dbgcharput((UCHAR *) "Label:  ", 8, 0, 0);
	if (dbgcharedit(lastlabel, &lastlabelsize, sizeof(lastlabel), 7, 0, 32) != VID_ENTER) return;
	if (!lastlabelsize) return;

	memcpy(dbgwork, lastlabel, lastlabelsize);
	dbgwork[lastlabelsize] = 0;

	if ((dbgflags & DBGFLAGS_EXTRAINFO) && isdigit(dbgwork[0])) {  /* support a label number */
		for (i3 = 0; dbgwork[i3] && dbgwork[i3] != ':'; i3++);
		if (dbgwork[i3] == ':') {
			dbgwork[i3++] = 0;
			i1 = (int) strtol(dbgwork, NULL, 10);
		}
		else {
			i3 = 0;
			i1 = dbgpgmmod;
		}
		i3 = (int) strtol(&dbgwork[i3], NULL, 10);
		pgmmodsave = pgmmodule;
		pcountsave = pcount;
		setpcnt(i1, 0);
		i3 = getpcnt(i3, &i1, &i2);
		setpcnt(pgmmodsave, pcountsave);
		if (i3) {
			strcat(dbgwork, ": Label Not Found");
			dbgerror(ERROR_WARNING, dbgwork);
			return;
		}
		dbgpcount = i2;
		i2 = getdatamod(i1);

		if (i1 != dbgpgmmod) dbgopendbg(i1);
		dbgdatamod = i2;
		if (dbgflags & DBGFLAGS_DBGOPEN) {
			for (i1 = 0; i1 < ptsize && pgmtable[i1].pcnt < dbgpcount; i1++);
			if (i1 == ptsize) i1--;
			wininfo[0].line = pgmtable[i1].linenum;
			if (srcfile != pgmtable[i1].stidx) {
				srcfile = pgmtable[i1].stidx;
				dbgopensrc();
				dspflg |= WIN_REDISPLAY << MAIN_SHIFT;
			}
			dspflg |= WIN_CENTERLINE << MAIN_SHIFT;
		}
		dspflg |= STATUS_FLAGS;
		return;
	}

	for (i1 = 0; i1 < ltsize && dbgstrcmp(dbgwork, &nametable[labeltable[i1].nptr], dbgflags & DBGFLAGS_SENSITIVE); i1++);
	if (i1 < ltsize) {
		wininfo[0].line = labeltable[i1].linenum;
		if (srcfile != labeltable[i1].stidx) {
			srcfile = labeltable[i1].stidx;
			dbgopensrc();
			dspflg |= WIN_REDISPLAY << MAIN_SHIFT;
		}
		dspflg |= WIN_CENTERLINE << MAIN_SHIFT;
		window = 0;
		dspflg |= WIN_ACTIVE;
	}
	else {
		strcat(dbgwork, ": Label Not Found");
		dbgerror(ERROR_WARNING, dbgwork);
	}
	return;
}

static void dbgfindline()
{
	INT i1;

	dspflg |= COMMAND_LINE;
	dbgcharput((UCHAR *) "Line:   ", 8, 0, 0);
	if (dbgcharedit(lastgoto, &lastgotosize, sizeof(lastgoto), 6, 0, 10) != VID_ENTER) return;
	if (!lastgotosize) return;

	mscntoi(lastgoto, &i1, lastgotosize);
	if (!i1) {
		dbgfindstack(0x8000);
		return;
	}
	wininfo[0].line = i1 - 1;
	dspflg |= WIN_CENTERLINE << MAIN_SHIFT;
	window = 0;
	dspflg |= WIN_ACTIVE;
	return;
}

static void dbgfindstack(INT cnt)
{
	INT i1, i2;

	if (stackptr == 0 && cnt < 0) return;
	i1 = returnstackcount();
	if (stackptr == i1 && cnt > 0 && stopline == wininfo[0].line) return;

	stackptr += cnt;
	if (stackptr >= i1) {
		stackptr = i1;
		i1 = pgmmodule;
		i2 = datamodule;
		dbgpcount = savepcount;
	}
	else {
		if (stackptr < 0) stackptr = 0;
		i1 = returnstackpgm(stackptr);
		dbgpcount = returnstackpcount(stackptr);
		i2 = getdatamod(i1);
	}
	if (i1 != dbgpgmmod) dbgopendbg(i1);
	dbgdatamod = i2;
	if (dbgflags & DBGFLAGS_DBGOPEN) {
		for (i1 = 0; i1 < ptsize && pgmtable[i1].pcnt < dbgpcount; i1++);
		if (i1 == ptsize) i1--;
		wininfo[0].line = pgmtable[i1].linenum;
		if (srcfile != pgmtable[i1].stidx) {
			srcfile = pgmtable[i1].stidx;
			dbgopensrc();
			dspflg |= WIN_REDISPLAY << MAIN_SHIFT;
		}
		dspflg |= WIN_CENTERLINE << MAIN_SHIFT;
	}
	dspflg |= STATUS_FLAGS;
	return;
}

static void dbgfindsrc()
{
	INT i1;

	if (!(dbgflags & DBGFLAGS_DBGOPEN)) {
		dbgerror(ERROR_FAILURE, ".DBG Information not loaded");
		return;
	}

	dspflg |= COMMAND_LINE;
	dbgcharput((UCHAR *) "Source: ", 8, 0, 0);
	if (dbgcharedit(lastsource, &lastsourcesize, sizeof(lastsource), 8, 0, 72) != VID_ENTER) return;
	if (!lastsourcesize) return;

	memcpy(dbgwork, lastsource, lastsourcesize);
	dbgwork[lastsourcesize] = 0;
	miofixname(dbgwork, ".txt", FIXNAME_EXT_ADD);
	for (i1 = 0; i1 < stsize && dbgstrcmp(dbgwork, &nametable[sourcetable[i1].nametableptr], FALSE); i1++);
	if (i1 < stsize) {
		wininfo[0].line = 0;
		if (srcfile != i1) {
			srcfile = i1;
			dbgopensrc();
			dspflg |= WIN_REDISPLAY << MAIN_SHIFT;
		}
		dspflg |= WIN_CENTERLINE << MAIN_SHIFT;
	}
	else {
		strcat(dbgwork, ": Source Not Found");
		dbgerror(ERROR_FAILURE, dbgwork);
	}
	return;
}

static void dbgfindmod()
{
	INT i1, i2;
	CHAR *ptr;

	ptr = getmname(dbgpgmmod, dbgdatamod, TRUE);
	lastmodulesize = (INT) strlen(ptr);
	memcpy(lastmodule, ptr, lastmodulesize);

	dspflg |= COMMAND_LINE;
	dbgcharput((UCHAR *) "Module: ", 8, 0, 0);
	if (dbgcharedit(lastmodule, &lastmodulesize, sizeof(lastmodule), 8, 0, 72) != VID_ENTER) return;
	if (!lastmodulesize) return;

	memcpy(dbgwork, lastmodule, lastmodulesize);
	dbgwork[lastmodulesize] = 0;
	i1 = i2 = -1;
	findmname(dbgwork, &i1, &i2);
	if (i1 == -1) {
		strcat(dbgwork, ": Module Not Currently Loaded");
		dbgerror(ERROR_FAILURE, dbgwork);
		return;
	}
	if (i2 == -1) {
		strcpy(dbgwork, ": Data Module Not Currently Loaded");
		dbgerror(ERROR_FAILURE, dbgwork);
		return;
	}

	dbgopendbg(i1);
	dbgdatamod = i2;
	dbgpcount = -1;
	if (dbgflags & DBGFLAGS_DBGOPEN) {
		srcfile = 0;
		dbgopensrc();
		dspflg |= WIN_REDISPLAY << MAIN_SHIFT;
	}
	return;
}

static void dbgalter()
{
	INT i1;
	INT32 vidcmd[2];

	dspflg |= COMMAND_LINE;
	dbgcharput((UCHAR *) "EQUAL (Set/Clear): ", 19, 0, 0);
	dbgwork[0] = (dbcflags & DBCFLAG_EQUAL) ? 'S' : 'C';
	dbgcharput((UCHAR *) dbgwork, 1, 19, 0);
	vidcmd[0] = VID_HORZ | 19;
	vidcmd[1] = VID_END_NOFLUSH;
	vidput(vidcmd);
	i1 = dbgcharget(TRUE);
	if (i1 == VID_ESCAPE) return;
	if (i1 == 'S' || i1 == 's') dbcflags |= DBCFLAG_EQUAL;
	else if (i1 == 'C' || i1 == 'c') dbcflags &= ~DBCFLAG_EQUAL;
	dbgchardisplay(STATUS_FLAGS);

	dbgcharput((UCHAR *) "LESS (Set/Clear):   ", 20, 0, 0);
	dbgwork[0] = (dbcflags & DBCFLAG_LESS) ? 'S' : 'C';
	dbgcharput((UCHAR *) dbgwork, 1, 18, 0);
	vidcmd[0] = VID_HORZ | 18;
	vidcmd[1] = VID_END_NOFLUSH;
	vidput(vidcmd);
	i1 = dbgcharget(TRUE);
	if (i1 == VID_ESCAPE) return;
	if (i1 == 'S' || i1 == 's') dbcflags |= DBCFLAG_LESS;
	else if (i1 == 'C' || i1 == 'c') dbcflags &= ~DBCFLAG_LESS;
	dbgchardisplay(STATUS_FLAGS);

	dbgcharput((UCHAR *) "OVER (Set/Clear):  ", 19, 0, 0);
	dbgwork[0] = (dbcflags & DBCFLAG_OVER) ? 'S' : 'C';
	dbgcharput((UCHAR *) dbgwork, 1, 18, 0);
	vidcmd[0] = VID_HORZ | 18;
	vidcmd[1] = VID_END_NOFLUSH;
	vidput(vidcmd);
	i1 = dbgcharget(TRUE);
	if (i1 == VID_ESCAPE) return;
	if (i1 == 'S' || i1 == 's') dbcflags |= DBCFLAG_OVER;
	else if (i1 == 'C' || i1 == 'c') dbcflags &= ~DBCFLAG_OVER;
	dbgchardisplay(STATUS_FLAGS);

	dbgcharput((UCHAR *) "EOS (Set/Clear):   ", 19, 0, 0);
	dbgwork[0] = (dbcflags & DBCFLAG_EOS) ? 'S' : 'C';
	dbgcharput((UCHAR *) dbgwork, 1, 17, 0);
	vidcmd[0] = VID_HORZ | 17;
	vidcmd[1] = VID_END_NOFLUSH;
	vidput(vidcmd);
	i1 = dbgcharget(TRUE);
	if (i1 == VID_ESCAPE) return;
	if (i1 == 'S' || i1 == 's') dbcflags |= DBCFLAG_EOS;
	else if (i1 == 'C' || i1 == 'c') dbcflags &= ~DBCFLAG_EOS;
	dspflg |= STATUS_FLAGS;
	return;
}

static INT common1_getvar()
{
	INT i1;
	CHAR *ptr = getmname(dbgpgmmod, dbgdatamod, TRUE);
	lastmodulesize = (INT) strlen(ptr);
	memcpy(lastmodule, ptr, lastmodulesize);

	dspflg |= COMMAND_LINE;
	dbgcharput((UCHAR *) "Variable: ", 10, 0, 0);
	if (dbgcharedit(lastvariable, &lastvariablesize, sizeof(lastvariable), 10, 0, 72)
			!= VID_ENTER) return RC_ERROR;
	if (!lastvariablesize) return RC_ERROR;

	if (lastmodulesize) {
		for (i1 = 0; i1 < lastvariablesize && lastvariable[i1] != ':'; i1++);
		if (i1 == lastvariablesize) {
			memcpy(dbgwork, lastmodule, lastmodulesize);
			i1 = lastmodulesize;
			dbgwork[i1++] = ':';
		}
	}
	else i1 = 0;
	memcpy(&dbgwork[i1], lastvariable, lastvariablesize);
	dbgwork[i1 + lastvariablesize] = '\0';
	i1 = dbgfvar(dbgwork);
	return i1;
}

/**
 * New type of variable break as of DX 16.2
 * Requested by PPro
 *
 * This will break whenever a variable changes its value,
 * from the current value when the break is set, and then
 * from the value it has when a break is taken.
 */
static void dbgsetpermvarbrk()
{
	INT varnum, dmod, off;
	UCHAR *adr1;

	if ((varnum = common1_getvar()) == RC_ERROR) return;

	vartable[varnum].type = VAR_PVBRK | VAR_LES | VAR_GRT;
	adr1 = dbggvar(varnum, &dmod, &off);
	if (adr1 == NULL) {
		dbgrvar(varnum);
		dbgerror(ERROR_FAILURE, dbgwork);
		return;
	}
	scanvar(adr1);
	if (!(vartype & (TYPE_CHARACTER | TYPE_NUMERIC))) {
		dbgrvar(varnum);
		dbgerror(ERROR_FAILURE, "Unsupported variable");
		return;
	}
	if (!setvartablevaluetocurrent(varnum, adr1)) return;
	dbgflags |= DBGFLAGS_TSTVALU;
}

static void dbgfindvar(INT flag)
{
	INT i1, i2, i3, i4, i5, i6, dmod, rwincnt;
	INT off, off1, off2, off3, off4, size, array[3];
	CHAR *ptr;
	UCHAR *adr1, *adr2, *adr3;
	DBGDATAINFO datainfo[4];
	INT32 vidcmd[2];

	if ((i1 = common1_getvar()) == RC_ERROR) return;

	if (flag & FINDVAR_VIEW) {
		vartable[i1].type = VAR_DSP;
		wininfo[1].line = wininfo[1].maxline++;
		dspflg |= (WIN_REDISPLAY | WIN_LINE) << VIEW_SHIFT;
	}
	else if (flag & FINDVAR_MODIFY) {
		adr1 = dbggvar(i1, &dmod, &off);
		if (adr1 != NULL) {
			i2 = 0;
			dbgcharput((UCHAR *) "Value:", 6, 0, 0);
			vidcmd[0] = VID_EL;
			vidcmd[1] = VID_END_NOFLUSH;
			vidput(vidcmd);
			if (dbgcharedit((UCHAR *) &dbgwork[3], &i2, sizeof(dbgwork) - 3, 7, 0, 72) != VID_ENTER) {
				dbgrvar(i1);
				return;
			}
			if (i2 > 255) i2 = 255;
			if (i2) dbgwork[0] = 1;
			else dbgwork[0] = 0;
			dbgwork[1] = dbgwork[2] = (CHAR) i2;
			i3 = dbcflags & DBCFLAG_FLAGS;
			movevar((UCHAR *) dbgwork, adr1);
			dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i3;
			dbgnvar(i1, dbgwork);
			datainfo[0].data = (UCHAR *) dbgwork;
			datainfo[0].size = (INT) strlen(dbgwork);
			dbgvvar(i1, adr1, dmod, off, &datainfo[1], &i2);
			dbgresultdata(datainfo, i2);
			dspflg |= WIN_REDISPLAY << VIEW_SHIFT;
		}
		else dbgerror(ERROR_FAILURE, dbgwork);
		dbgrvar(i1);
	}
	else if (flag & FINDVAR_BREAK) {
		vartable[i1].type = VAR_BRK;
		adr1 = dbggvar(i1, &dmod, &off);
		if (adr1 == NULL) {
			dbgrvar(i1);
			dbgerror(ERROR_FAILURE, dbgwork);
			return;
		}
		scanvar(adr1);
		if ((vartype & TYPE_CHARACTER)
				/* a CHAR address variable */
				|| *adr1 == 0xB0) i2 = 1;
		else if ((vartype & TYPE_NUMERIC)
				/* a numeric address variable */
				|| *adr1 == 0xB1) i2 = 2;
		else {
			dbgrvar(i1);
			dbgerror(ERROR_FAILURE, "Unsupported variable");
			return;
		}
		dbgcharput((UCHAR *) "=, <>, >, >=, <, <=: ", 21, 0, 0);
		vidcmd[0] = VID_EL;
		vidcmd[1] = VID_END_NOFLUSH;
		vidput(vidcmd);
		i4 = i3 = 0;
		if (dbgcharedit((UCHAR *) dbgwork, &i4, 2, 21, 0, 3) == VID_ENTER) {
			while (--i4 >= 0) {
				if (dbgwork[i4] == '=') i3 |= SELECT_EQUAL;
				else if (dbgwork[i4] == '<') i3 |= SELECT_LESS;
				else if (dbgwork[i4] == '>') i3 |= SELECT_GREATER;
			}
			dbgcharput((UCHAR *) "Value:", 6, 0, 0);
			vidcmd[0] = VID_EL;
			vidcmd[1] = VID_END_NOFLUSH;
			vidput(vidcmd);
			size = 0;
			if (dbgcharedit((UCHAR *) dbgwork, &size, sizeof(dbgwork) - 1, 7, 0, 73) == VID_ENTER) {
				if (!size) {
					dbgcharput((UCHAR *) "Use current value (Y/N):", 24, 0, 0);
					vidcmd[0] = VID_HORZ | 25;
					vidcmd[1] = VID_END_NOFLUSH;
					vidput(vidcmd);
					i4 = dbgcharget(TRUE);
					if (i4 == 'Y' || i4 == 'y') i3 |= SELECT_CURRENT;
					else if (i4 != VID_ESCAPE) i3 |= SELECT_NULL;
					else i3 = 0;
				}
				else i3 |= SELECT_VALUE;
			}
			else i3 = 0;
		}
		else i3 = 0;
		if (i3 & SELECT_EQUAL) vartable[i1].type |= VAR_EQL;
		if (i3 & SELECT_LESS) vartable[i1].type |= VAR_LES;
		if (i3 & SELECT_GREATER) vartable[i1].type |= VAR_GRT;
		if (!(vartable[i1].type & (VAR_EQL | VAR_LES | VAR_GRT))) {
			dbgrvar(i1);
			return;
		}

		dbgwork[size] = '\0';
		if (i3 & SELECT_VALUE) {
			if (i2 == 1) i3 = (INT) strlen(dbgwork);
			else {
				/* remove spaces */
				for (i3 = 0, i4 = 0; dbgwork[i3]; i3++) if (!isspace(dbgwork[i3])) dbgwork[i4++] = dbgwork[i3];
				dbgwork[i4] = 0;
				i3 = 0;
				if (dbgwork[i3] == '-') i3++;
				while (isdigit(dbgwork[i3]) || dbgwork[i3] == '.') i3++;
				if (!i3 || i3 > 31 || dbgwork[i3] || !isdigit(dbgwork[i3 - 1])) {
					dbgrvar(i1);
					dbgerror(ERROR_FAILURE, "Invalid numeric value");
					return;
				}
				memmove(&dbgwork[1], dbgwork, i3);
				dbgwork[0] = (CHAR)(0x80 | i3++);
			}
			adr2 = (UCHAR *) dbgwork;
		}
		else if ((i3 & SELECT_CURRENT) && (vartype & (TYPE_CHARACTER | TYPE_NUMERIC)) && fp) {
			if (i2 == 1) {  /* character variable */
				adr2 = &adr1[hl];
				i3 = lp;
				if (vartable[i1].flag & FLG_LLN) {
					adr2 += fp - 1;
					i3 -= fp - 1;
				}
			}
			else {  /* numeric variable */
				adr2 = adr1;
				i3 = hl + pl;
			}
		}
		else {
			adr2 = (UCHAR *) dbgwork;
			if (i2 == 2) {  /* initialize numeric variable to zero */
				adr2[0] = 0x81;
				adr2[1] = '0';
				i3 = 2;
			}
			else i3 = 0;
		}
		if (i2 == 1) vartable[i1].type |= VAR_STR;
		ptr = (CHAR *) *vartable[i1].nameptr;
		i2 = 0;
		while (ptr[i2++]);
		while (ptr[i2++]);
		while (ptr[i2++]);
		if (memchange(vartable[i1].nameptr, i2 + i3 + 1, 0)) {
			dbgrvar(i1);
			dbgerror(ERROR_FAILURE, "Not Enough Memory to Store Value");
			return;
		}
		memcpy(&ptr[i2], adr2, i3);
		ptr[i2 + i3] = '\0';
		dbgflags |= DBGFLAGS_TSTVALU;
	}
	else {  /* display */
		adr1 = dbggvar(i1, &dmod, &off);
		if (adr1 != NULL) {
			if (*adr1 == 0xFE) {  /* jump over "\xFE[record.]name\xFE" */
				for (i2 = 1; i2 < 67 && adr1[i2] != 0xFE; i2++);
				if (i2 < 67) {
					adr1 += i2 + 1;
					off += i2 + 1;
				}
			}
			rwincnt = RWINMAX;
			dbgnvar(i1, dbgwork);
			if (*adr1 >= 0xA6 && *adr1 <= 0xA8) {
				for (i2 = *adr1++, i3 = 0; i3 < 3; i3++) {
					if (i2-- >= 0xA6) {
						array[i3] = llhh(adr1);
						adr1 += 2;
						off += 2;
					}
					else array[i3] = 0;
				}
				size = llhh(adr1);
				off += 3;
				off1 = (INT) strlen(dbgwork);
				dbgwork[off1++] = leftbracket;
				for (i2 = 1; rwincnt && i2 <= array[0]; i2++) {
					off2 = off1;
					mscitoa(i2, &dbgwork[off2]);
					off2 += (INT) strlen(&dbgwork[off2]);
					for (i3 = (array[1]) ? 1 : 0; rwincnt && i3 <= array[1]; i3++) {
						off3 = off2;
						if (i3) {
							dbgwork[off3++] = ',';
							mscitoa(i3, &dbgwork[off3]);
							off3 += (INT) strlen(&dbgwork[off3]);
						}
						for (i4 = (array[2]) ? 1 : 0; rwincnt && i4 <= array[2]; i4++, rwincnt--) {
							off4 = off3;
							if (i4) {
								dbgwork[off4++] = ',';
								mscitoa(i4, &dbgwork[off4]);
								off4 += (INT) strlen(&dbgwork[off4]);
							}
							dbgwork[off4++] = rightbracket;
							dbgwork[off4] = 0;
							i5 = dmod;
							i6 = off;
							adr1 = dbggadr(&i5, &i6);
							if (adr1 != NULL) {
								datainfo[0].data = (UCHAR *) dbgwork;
								datainfo[0].size = (INT) strlen(dbgwork);
								dbgvvar(i1, adr1, i5, i6, &datainfo[1], &i5);
								dbgresultdata(datainfo, i5);
							}
							else {
								strcpy(&dbgwork[off4], " - error loading data module");
								dbgresultline(dbgwork);
							}
							off += size;
						}
					}
				}
			}
			else if (*adr1 == 0xA4 && (!(vartable[i1].flag & FLG_ADR) || (vartable[i1].flag & FLG_ADX))) {
				off1 = (INT) strlen(dbgwork);
				dbgwork[off1++] = '.';
				setlastvar(dmod, off);
				i5 = LIST_ADR | LIST_LIST1 | LIST_NUM1;
				for (i2 = 1; rwincnt--; i2++) {
					adr2 = getlist(i5);
					if (adr2 == NULL) break;
					i5 = LIST_ADR | LIST_NUM1;
					off += (INT) (adr2 - adr1);
					adr1 = adr2;
					i3 = dmod;
					i4 = off;
					adr2 = dbggadr(&i3, &i4);
					if (adr2 != NULL) {
						i6 = 0;
						if (*(adr2 - 1) == 0xFE) {
							for (adr3 = adr2 - 2; i6 < 33 && *adr3 != 0xFE && *adr3 != '.'; adr3--, i6++);
						}
						if (i6 && i6 < 33) {
							memcpy(&dbgwork[off1], adr3 + 1, i6);
							dbgwork[off1 + i6] = 0;
						}
						else mscitoa(i2, &dbgwork[off1]);
						datainfo[0].data = (UCHAR *) dbgwork;
						datainfo[0].size = (INT) strlen(dbgwork);
						dbgvvar(i1, adr2, i3, i4, &datainfo[1], &i3);
						dbgresultdata(datainfo, i3);
					}
					else {
						mscitoa(i2, &dbgwork[off1]);
						strcat(dbgwork, " - error loading data module");
						dbgresultline(dbgwork);
					}
				}
			}
			else {
				datainfo[0].data = (UCHAR *) dbgwork;
				datainfo[0].size = (INT) strlen(dbgwork);
				dbgvvar(i1, adr1, dmod, off, &datainfo[1], &i2);
				dbgresultdata(datainfo, i2);
			}
		}
		else dbgerror(ERROR_FAILURE, dbgwork);
		dbgrvar(i1);
	}
	return;
}

/**
 * Put the current value of the variable (varaddress) in the vartable[varnum]
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
	INT i2 = 0, valueLength;
	CHAR *ptr = (CHAR *) *vartable[varnum].nameptr;
	if (vartype & TYPE_CHARACTER) {  /* character variable */
		adr2 = &varaddress[hl];
		valueLength = (fp != 0) ? lp : 0;
		if (vartable[varnum].flag & FLG_LLN && fp != 0) {
			adr2 += fp - 1;
			valueLength -= fp - 1;
		}
		vartable[varnum].type |= VAR_STR;
	}
	else {  /* numeric variable */
		adr2 = varaddress;
		valueLength = hl + pl;
	}
	while (ptr[i2++]); // scan past program module name
	while (ptr[i2++]); // scan past routine name
	while (ptr[i2++]); // scan past variable name
	if (memchange(vartable[varnum].nameptr, i2 + valueLength + 1, 0)) {
		dbgrvar(varnum);
		dbgerror(ERROR_FAILURE, "Not Enough Memory to Store Value");
		return FALSE;
	}
	setpgmdata();
	dbgsetmem();
	if (valueLength != 0) memcpy(&ptr[i2], adr2, valueLength);
	ptr[i2 + valueLength] = '\0';
	return TRUE;
}

static void dbgremovevar()
{
	INT i1, i2, i3, cnt, size;
	UCHAR work[sizeof(dbgwork)];
	INT32 vidcmd[2];

	i2 = cnt = 0;
	for (i1 = 0; i1 < varhi; i1++) {
		if (!(vartable[i1].type & VAR_DSP)) continue;
		if (cnt++ == i2) {
			dbgnvar(i1, dbgwork);
			size = (INT) strlen(dbgwork);
			dspflg |= COMMAND_LINE;
			strcpy((CHAR *) work, "Remove \"");
			if (size > 57) strcpy(&dbgwork[54], "...");
			strcat((CHAR *) work, dbgwork);
			strcat((CHAR *) work, "\" (Y/N/All): ");
			dbgcharput(work, (INT) strlen((CHAR *) work), 0, 0);
			vidcmd[0] = VID_EL;
			vidcmd[1] = VID_END_NOFLUSH;
			vidput(vidcmd);
			i3 = dbgcharget(TRUE);
			if (i3 == VID_ESCAPE) return;
			if (i3 == 'A' || i3 == 'a') goto dbgremovevar1;
			if (i3 == 'Y' || i3 == 'y') {
				dbgrvar(i1);
				wininfo[1].maxline--;
				if (--cnt < wininfo[1].line) wininfo[1].line--;
				dbgchardisplay((WIN_REDISPLAY | WIN_LINE) << VIEW_SHIFT);
				i1--;  /* dbgrvar shifts displayed variables */
			}
			i2 = cnt;
		}
	}
	return;

dbgremovevar1:
	size = 0;
	i3 = SELECT_ALL;
	for (i1 = cnt = 0; i1 < varhi; i1++) {
		if (!(vartable[i1].type & VAR_DSP)) continue;
		if (!(i3 & SELECT_ALL)) {
			dbgnvar(i1, dbgwork);
			if (size != (INT) strlen(dbgwork) || memcmp(work, dbgwork, size)) {
				cnt++;
				continue;
			}
		}
		dbgrvar(i1);
		wininfo[1].maxline--;
		if (cnt < wininfo[1].line) wininfo[1].line--;
		dspflg |= (WIN_REDISPLAY | WIN_LINE) << VIEW_SHIFT;
		if (!(i3 & SELECT_ALL)) {
			size = 0;
			break;
		}
		i1--;  /* dbgrvar shifts displayed variables */
	}
	if (!(i3 & SELECT_ALL) && size) {
		strcat(dbgwork, ": View Variable Not Found");
		dbgerror(ERROR_FAILURE, dbgwork);
	}
	return;
}

static void dbgcode()
{
	INT i1, hexflg, len, offset;
	UINT i2;
	INT32 vidcmd[2];
	CHAR work[256];
	UCHAR c1;

	dspflg |= COMMAND_LINE;
	dbgcharput((UCHAR *) "OFFSET: ", 8, 0, 0);
	if (dbgpcount != -1) mscitoa(dbgpcount, dbgwork);
	else dbgwork[0] = 0;
	i1 = (INT) strlen(dbgwork);
	if (dbgcharedit((UCHAR *) dbgwork, &i1, 9, 8, 0, 10) != VID_ENTER) return;
	dbgwork[i1] = 0;
	offset = (int) strtol(dbgwork, NULL, 10);

	dbgcharput((UCHAR *) "LENGTH:          ", 17, 0, 0);
	strcpy(dbgwork, "20");
	i1 = (INT) strlen(dbgwork);
	if (dbgcharedit((UCHAR *) dbgwork, &i1, 3, 8, 0, 4) != VID_ENTER) return;
	dbgwork[i1] = 0;
	len = atoi(dbgwork);

	dbgcharput((UCHAR *) "HEX:       ", 10, 0, 0);
	dbgwork[0] = 'Y';
	dbgcharput((UCHAR *) dbgwork, 1, 5, 0);
	vidcmd[0] = VID_HORZ | 5;
	vidcmd[1] = VID_END_NOFLUSH;
	vidput(vidcmd);
	i1 = dbgcharget(TRUE);
	if (i1 == VID_ESCAPE) return;
	if (i1 == 'n' || i1 == 'N') hexflg = FALSE;
	else hexflg = TRUE;

	for (i1 = i2 = 0; i1 < len && i2 < sizeof(work) - 5; i1++) {
		c1 = pgm[offset++];
		if (hexflg) {
			work[i2] = c1 >> 4;
			if (work[i2] < 10) work[i2] += '0';
			else work[i2] += 'A' - 10;
			i2++;
			work[i2] = c1 & 0x0F;
			if (work[i2] < 10) work[i2] += '0';
			else work[i2] += 'A' - 10;
			i2++;
		}
		else {
			mscitoa(c1, &work[i2]);
			i2 += (INT) strlen(&work[i2]);
		}
		work[i2++] = ' ';
	}
	if (i2) {
		work[i2] = 0;
		dbgresultline(work);
	}
	return;
}

static void dbgdata()
{
	INT i1, hexflg, len, offset;
	UINT i2;
	INT32 vidcmd[2];
	CHAR work[256];
	UCHAR c1;

	dspflg |= COMMAND_LINE;
	dbgcharput((UCHAR *) "OFFSET: ", 8, 0, 0);
	i1 = 0;
	if (dbgcharedit((UCHAR *) dbgwork, &i1, 9, 8, 0, 10) != VID_ENTER) return;
	dbgwork[i1] = 0;
	offset = atoi(dbgwork);

	dbgcharput((UCHAR *) "LENGTH:          ", 17, 0, 0);
	strcpy(dbgwork, "20");
	i1 = (INT) strlen(dbgwork);
	if (dbgcharedit((UCHAR *) dbgwork, &i1, 3, 8, 0, 4) != VID_ENTER) return;
	dbgwork[i1] = 0;
	len = atoi(dbgwork);

	dbgcharput((UCHAR *) "HEX:       ", 10, 0, 0);
	dbgwork[0] = 'Y';
	dbgcharput((UCHAR *) dbgwork, 1, 5, 0);
	vidcmd[0] = VID_HORZ | 5;
	vidcmd[1] = VID_END_NOFLUSH;
	vidput(vidcmd);
	i1 = dbgcharget(TRUE);
	if (i1 == VID_ESCAPE) return;
	if (i1 == 'n' || i1 == 'N') hexflg = FALSE;
	else hexflg = TRUE;

	for (i1 = i2 = 0; i1 < len && i2 < sizeof(work) - 5; i1++) {
		c1 = data[offset++];
		if (hexflg) {
			work[i2] = c1 >> 4;
			if (work[i2] < 10) work[i2] += '0';
			else work[i2] += 'A' - 10;
			i2++;
			work[i2] = c1 & 0x0F;
			if (work[i2] < 10) work[i2] += '0';
			else work[i2] += 'A' - 10;
			i2++;
		}
		else {
			mscitoa(c1, &work[i2]);
			i2 += (INT) strlen(&work[i2]);
		}
		work[i2++] = ' ';
	}
	if (i2) {
		work[i2] = 0;
		dbgresultline(work);
	}
	return;
}

static void dbgsettabs()
{
	dspflg |= COMMAND_LINE;
	dbgcharput((UCHAR *) "Tabs (n[,n]...[:n]): ", 21, 0, 0);
	if (dbgcharedit(lasttab, &lasttabsize, sizeof(lasttab), 21, 0, 50) != VID_ENTER) return;
	dbgcalctabs();
	dspflg |= WIN_REDISPLAY << MAIN_SHIFT;
}

static void dbgcalctabs()
{
	INT i1, i2, i3, i4, i5;

	for (i1 = i2 = i3 = i4 = i5 = 0; i1 < lasttabsize && i2 < MAXTABS; i1++) {
		if (isdigit(lasttab[i1])) i3 = i3 * 10 + lasttab[i1] - '0';
		else if (lasttab[i1] == ',' || lasttab[i1] == ':') {
			if (--i3 > i4) {
				tabindent[i2++] = i3;
				i4 = i3;
			}
			i3 = 0;
			if (lasttab[i1] == ':') {
				i1++;
				while (i1 < lasttabsize && isdigit(lasttab[i1])) i5 = i5 * 10 + lasttab[i1++] - '0';
				break;
			}
		}
	}
	if (--i3 > i4) tabindent[i2++] = i3;
	if (!i5) i5 = 1;
	if (!i2) tabindent[i2++] = i5;
	while (i2 < MAXTABS) {
		tabindent[i2] = tabindent[i2 - 1] + i5;
		i2++;
	}
}

void dbgerror(INT type, CHAR *msg)
{
	CHAR *ptr;
	if (type == ERROR_WARNING) ptr = "Debugger Warning";
	else ptr = "Debugger Failure";
	strcpy(dbgerrorstring, ptr);
	strcat(dbgerrorstring, " - ");
	strcat(dbgerrorstring, msg);
	if (!(dbgflags & DBGFLAGS_DDT)) dbgresultline(dbgerrorstring);
}

static void dbgresultline(CHAR *line)
{
	INT i1, size;

	resultlines = (RESULTLINE *) *resultlineptr;

	i1 = wininfo[2].sourceline;
	if (wininfo[2].maxline == wininfo[2].maxsource) {
		wininfo[2].maxline--;
		if (wininfo[2].firstline == lastwininfo[2].firstline) lastwininfo[2].firstline--;
		memfree(resultlines[i1].line);
	}
	size = (INT) strlen(line);
	resultlines[i1].line = memalloc(size, 0);
	/* reset memory */
	setpgmdata();
	dbgsetmem();
	if (resultlines[i1].line != NULL) {
		memcpy(*resultlines[i1].line, line, size);
		wininfo[2].maxline++;
		if (++wininfo[2].sourceline == wininfo[2].maxsource) wininfo[2].sourceline = 0;
	}
	else size = 0;
	resultlines[i1].size = size;

	wininfo[2].firstline = wininfo[2].maxline - 1;
	if (wininfo[2].firstline < wininfo[2].displaylines) dspflg |= WIN_REDISPLAY << RESULT_SHIFT;
	dspflg |= WIN_FIRSTLINE << RESULT_SHIFT;
}

static void dbgresultdata(DBGDATAINFO datainfo[], INT hexflg)
{
	INT i1, i2, i3, i4, size;
	UCHAR c1, *ptr, *adr;

	resultlines = (RESULTLINE *) *resultlineptr;

	i1 = wininfo[2].sourceline;
	if (wininfo[2].maxline == wininfo[2].maxsource) {
		wininfo[2].maxline--;
		if (wininfo[2].firstline == lastwininfo[2].firstline) lastwininfo[2].firstline--;
		memfree(resultlines[i1].line);
	}

	if (hexflg) i2 = 2;
	else i2 = 1;

	size = datainfo[0].size + datainfo[1].size + datainfo[2].size * i2 + datainfo[3].size;
	resultlines[i1].line = memalloc(size, 0);
	/* reset memory */
	setpgmdata();
	dbgsetmem();
	if (resultlines[i1].line != NULL) {
		ptr = *resultlines[i1].line;
		memcpy(ptr, datainfo[0].data, datainfo[0].size);
		i2 = datainfo[0].size;
		memcpy(&ptr[i2], datainfo[1].data, datainfo[1].size);
		i2 += datainfo[1].size;
		adr = datainfo[2].data;
		i3 = datainfo[2].size;
		if (!hexflg) {
			while (--i3 >= 0) {
				if ((c1 = *(adr++)) < ' ') c1 = '?';
				ptr[i2++] = c1;
			}
		}
		else {
			while (--i3 >= 0) {
				i4 = *adr >> 4;
				if (i4 < 10) ptr[i2] = (CHAR)(i4 + '0');
				else ptr[i2] = (CHAR)(i4 - 10 + 'A');
				i2++;
				i4 = *adr++ & 0x0F;
				if (i4 < 10) ptr[i2] = (CHAR)(i4 + '0');
				else ptr[i2] = (CHAR)(i4 - 10 + 'A');
				i2++;
			}
		}
		memcpy(&ptr[i2], datainfo[3].data, datainfo[3].size);

		wininfo[2].maxline++;
		if (++wininfo[2].sourceline == wininfo[2].maxsource) wininfo[2].sourceline = 0;
	}
	else size = 0;
	resultlines[i1].size = size;

	wininfo[2].firstline = wininfo[2].maxline - 1;
	if (wininfo[2].firstline < wininfo[2].displaylines) dspflg |= WIN_REDISPLAY << RESULT_SHIFT;
	dspflg |= WIN_FIRSTLINE << RESULT_SHIFT;
}

static void dbghelp()  /* display help in the results window */
{
	dbgresultline("Commands:");
	dbgresultline("A - alter flags            L - find program label     V - view variable");
	dbgresultline("B - permanent breakpoint   M - modify variable        W - goto next window");
	dbgresultline("C - clear breakpoints      N - change module          X - single step");
	dbgresultline("D - display variable       O - output screen          Y - goto line number");
	dbgresultline("E - extended commands      P - break on value         Z - step over call");
	dbgresultline("F - change source file     Q - shutdown               / - search again");
	dbgresultline("G - go                     R - remove view variable   # - toggle line numbers");
	dbgresultline("I - set tab indent         S - search for string      % - toggle case sensitive");
	dbgresultline("J - one up return stack    T - temporary breakpoint   + - increase window size");
	dbgresultline("K - one down return stack  U - step out of call       - - decrease window size");
	dbgresultline("");
	dbgresultline("Extended commands:");
	dbgresultline("A - persistent break on value");
	dbgresultline("C - clear globals          M - toggle more option     S - toggle case sensitive");
	dbgresultline("G - save globals           P - save preference        V - toggle view window");
	dbgresultline("L - restore globals        R - toggle result window");
	dbgresultline("");
	dbgresultline("Special keys:");
	dbgresultline("UP    - scroll up          PGUP - scroll up 1 page    TAB   - next window");
	dbgresultline("DOWN  - scroll down        PGDN - scroll down 1 page  BKTAB - previous window");
	dbgresultline("LEFT  - scroll left        HOME - goto first line");
	dbgresultline("RIGHT - scroll right       END  - goto last line");
}

void dbgsetmem()
{
	if (dbgflags & DBGFLAGS_DBGOPEN) {
		sourcetable = (SRCDEF *) *stptr;
		datatable = (DATADEF *) *dtptr;
		labeltable = (LABELDEF *) *ltptr;
		pgmtable = (PGMDEF *) *ptptr;
		routinetable = (RTNDEF *) *rtptr;
		nametable = (CHAR *) *ntptr;
		classtable = (CLASSDEF *) *ctptr;
	}
	if (dbgflags & DBGFLAGS_SRCOPEN) rec = (OFFSET *) *recptr;
}

static INT dbgfvar(CHAR *var)
{
	INT i1, i2, i3, handle, clscnt, datamod, pgmmod, offset, rtncnt;
	INT state, varnum, varoff;
	CHAR c1, *ptr;
	CHAR cname[64], oname[64], pname[128];
	CHAR rname1[64], rname2[64], vname[64];

	for (varnum = 0; varnum < varhi && vartable[varnum].type; varnum++);
	if (varnum == VARMAX) {
		dbgerror(ERROR_FAILURE, "Too many variables currently being used");
		return RC_ERROR;
	}

	vartable[varnum].type = VAR_TMP;
	if (varnum == varhi) varhi++;
	/* remove spaces */
	for (i1 = 0, i2 = 0; var[i1]; i1++) if (!isspace(var[i1])) var[i2++] = var[i1];
	var[i2] = '\0';
	oname[0] = pname[0] = rname1[0] = rname2[0] = vname[0] = '\0';
	state = 0;
	for (i1 = i2 = 0; ; i1++) {
		c1 = var[i1];
		if (c1 == ':') {
			if (state <= 1) {
				if (i1 != i2) {
					memcpy(pname, &var[i2], i1 - i2);
					pname[i1 - i2] = 0;
				}
				state = 2;
			}
			else if (state == 2) {
				if (i1 != i2) {
					memcpy(oname, &var[i2], i1 - i2);
					oname[i1 - i2] = 0;
				}
				if (rname2[0]) {
					strcpy(rname1, rname2);
					rname2[0] = 0;
				}
				state = 3;
			}
			i2 = i1 + 1;
			continue;
		}
		if (state == 1) {
			if (!c1) {
				ptr = " - invalid variable name";
				goto dbgfvar1;
			}
			continue;
		}
		if (c1 == '!' && !state) {
			state = 1;
			continue;
		}
		if (c1 == '/') {
			if (i1 != i2 && !rname2[0]) {
				i3 = i1 - i2;
				if (i3 > 32) i3 = 32;
				memcpy(rname2, &var[i2], i3);
				rname2[i3] = 0;
			}
			i2 = i1 + 1;
			continue;
		}
		if (!c1 || c1 == '[' || c1 == '(' || c1 == '#' || c1 == '~' || c1 == '*' || c1 == '+' || c1 == '%' || c1 == '&') {
			if (c1 == '[') {
				leftbracket = '[';
				rightbracket = ']';
			}
			else if (c1 == '(') {
				leftbracket = '(';
				rightbracket = ')';
			}
			if (i1 != i2 && !vname[0]) {
				i3 = i1 - i2;
				if (i3 > 32) i3 = 32;
				memcpy(vname, &var[i2], i3);
				vname[i3] = 0;
				varoff = i2;
			}
			if (!c1) break;
			if (c1 == leftbracket) {  /* parse array */
				for (i2 = ++i1, i3 = 0; var[i1]; i1++) {
					c1 = var[i1];
					if (c1 == leftbracket) {
						i3++;
						continue;
					}
					if (c1 == rightbracket && i3) {
						i3--;
						continue;
					}
					if (i3) continue;
					if (vartable[varnum].arraycnt < 3) {
						if (c1 == rightbracket || c1 == ',') {
							if (i1 != i2) {
								for (i3 = i2; i3 < i1 && isdigit(var[i3]); i3++);
								if (i3 < i1) {
									var[i1] = 0;
									i3 = dbgfvar(&var[i2]);
									var[i1] = c1;
									if (i3 == -1) goto dbgfvar2;
									vartable[i3].type = VAR_NUM;
									i3 |= 0x8000;
								}
								else {
									i3 = 0;
									while (i2 < i1) i3 = i3 * 10 + var[i2++] - '0';
								}
								vartable[varnum].array[vartable[varnum].arraycnt++] = i3;
								i2 = i1 + 1;
								i3 = 0;
							}
						}
					}
					if (c1 == rightbracket) break;
				}
				c1 = var[i1];
			}
			while (c1) {
				if (c1 == '#') {
					for (i2 = ++i1, i3 = 0; ; i1++) {
						c1 = var[i1];
						if (c1 == leftbracket) {
							i3++;
							continue;
						}
						if (c1 == rightbracket && i3--) continue;
						if (c1 && i3) continue;
						if (!c1 || c1 == '~' || c1 == '*' || c1 == '&') {
							if (i1 != i2) {
								for (i3 = i2; i3 < i1 && isdigit(var[i3]); i3++);
								if (i3 != i1) {
									var[i1] = '\0';
									i3 = dbgfvar(&var[i2]);
									var[i1] = c1;
									if (i3 == -1) goto dbgfvar2;
									vartable[i3].type = VAR_NUM;
									i3 |= 0x8000;
								}
								else {
									i3 = 0;
									while (i2 < i1) i3 = i3 * 10 + var[i2++] - '0';
								}
								vartable[varnum].dspoff = i3;
							}
							break;
						}
					}
					continue;
				}
				if (c1 == '~') vartable[varnum].flag |= FLG_HEX;
				else if (c1 == '*') vartable[varnum].flag |= FLG_WRP;
				else if (c1 == '+') vartable[varnum].flag |= FLG_LLN;
				else if (c1 == '%') vartable[varnum].flag |= FLG_XNM;
				else if (c1 == '&') {
					if (vartable[varnum].flag & FLG_ADR) vartable[varnum].flag |= FLG_ADX;
					else vartable[varnum].flag |= FLG_ADR;
				}
				c1 = var[++i1];
			}
			break;
		}
	}

	if (!vname[0]) {
		ptr = " - invalid variable name";
		goto dbgfvar1;
	}

	/* get program and data */
	pgmmod = dbgpgmmod;
	datamod = dbgdatamod;
	findmname(pname, &pgmmod, &datamod);
	if (pgmmod == -1) {
		ptr = " - program module not found";
		goto dbgfvar1;
	}
	if (datamod == -1) {
		ptr = " - data module not found";
		goto dbgfvar1;
	}

	if (oname[0]) {  /* referenced through an object variable */
/*** CODE: STARTED TO ADD CODE TO SUPPORT VIEWING A VARIABLE IN A CLASS BY ***/
/***       SPECIFYING A OBJECT VARIABLE, ie. D  "module:obj:var". MANY ***/
/***       CHANGES NEED TO OCCUR AND HOW VARIABLES ARE STORED AND LOOKED ***/
/***       UP WILL CHANGE. RNAME1 CONTAINS ROUTINE FOR OBJECT ***/
	}

	offset = -1;
	if (isdigit(vname[0])) {
		if (vname[0] == '0' && toupper(vname[1]) == 'X') {
			for (offset = 0, i1 = 2; ; i1++) {
				if (isdigit(vname[i1])) offset = offset * 16 + vname[i1] - '0';
				else {
					c1 = (CHAR) toupper(vname[i1]);
					if (c1 >= 'A' && c1 <= 'F') offset = offset * 16 + c1 - 'A' + 10;
					else break;
				}
			}
		}
		else offset = atoi(vname);
		rname2[0] = 0;
	}
	else if (pgmmod == dbgpgmmod || getpname(pgmmod) == getpname(dbgpgmmod)) {
		if (!(dbgflags & DBGFLAGS_DBGOPEN)) {
			strcpy(var, ".DBG Information not loaded");
			ptr = "";
			goto dbgfvar1;
		}
		if (pgmmod == dbgpgmmod) clscnt = classnum;
		else {
			if (getmtype(pgmmod) == MTYPE_CLASS) {
				ptr = getdname(getdatamod(pgmmod));
				for (i1 = 0; i1 < ctsize && strcmp(ptr, &nametable[classtable[i1].nptr]); i1++);
				if (i1 == ctsize) {
					strcpy(var, "Unable to locate information for class: ");
					goto dbgfvar1;
				}
				clscnt = i1;
			}
			else clscnt = -1;
		}
		if (clscnt != -1) {
			i1 = classtable[clscnt].dtidx;
			i2 = i1 + classtable[clscnt].dtcnt;
		}
		else {
			i1 = 0;
			i2 = dtsize;
		}
		if (rname2[0]) {
			for (i3 = 0; i3 < rtsize && (routinetable[i3].class != clscnt
				|| dbgstrcmp(rname2, &nametable[routinetable[i3].nptr], dbgflags & DBGFLAGS_SENSITIVE)); i3++);
			if (i3 == rtsize) {
				ptr = " - unable to find routine";
				goto dbgfvar1;
			}
			i1 = routinetable[i3].dtidx;
			i2 = i1 + routinetable[i3].dtcnt;
		}
		for ( ; i1 < i2; i1++) {
			if (dbgstrcmp(vname, &nametable[datatable[i1].nptr], dbgflags & DBGFLAGS_SENSITIVE)) continue;
			if (datatable[i1].rtidx != -1) {
				if (rname2[0]) {
					if (i3 != datatable[i1].rtidx) continue;
				}
				else {
					i3 = datatable[i1].rtidx;
					if (dbgpcount < routinetable[i3].spcnt || dbgpcount >= routinetable[i3].epcnt) continue;
					strcpy(rname2, &nametable[routinetable[i3].nptr]);
				}
			}
			offset = datatable[i1].offset;
			break;
		}
	}
	else {
		strcpy(name, getpname(pgmmod));
		miofixname(name, ".dbg", FIXNAME_EXT_ADD);
		handle = rioopen(name, RIO_M_SRO | RIO_P_DBC | RIO_T_STD, 3, 80);
		setpgmdata();
		dbgsetmem();
		if (handle < 0) {
			strcpy(var, "Unable to open ");
			ptr = name;
			goto dbgfvar1;
		}
		if (getmtype(pgmmod) == MTYPE_CLASS) {
			strcpy(cname, getdname(datamod));
			for ( ; ; ) {
				i1 = rioget(handle, (UCHAR *) pname, 80);
				if (i1 < 0) {
					ptr = " - unable to find class";
					goto dbgfvar1;
				}
				pname[i1] = 0;
				if (pname[0] == 'C' && !strcmp(cname, &pname[7])) break;
			}
		}
		else cname[0] = '\0';
		if (rname2[0]) {
			for ( ; ; ) {
				i1 = rioget(handle, (UCHAR *) pname, 80);
				if (i1 < 0 || (cname[0] && i1 >= 1 && pname[0] == 'Y')) {
					ptr = " - unable to find routine";
					goto dbgfvar1;
				}
				pname[i1] = '\0';
				if (pname[0] == 'R' && !dbgstrcmp(rname2, &pname[1], dbgflags & DBGFLAGS_SENSITIVE)) break;
			}
		}
		for (rtncnt = clscnt = 0; ; ) {
			i1 = rioget(handle, (UCHAR *) pname, 80);
			if (i1 < 0) break;
			pname[i1] = '\0';
			if (clscnt) {
				if (pname[0] == 'Y') clscnt = 0;
				continue;
			}
			if (rtncnt) {
				if (pname[0] == 'X') rtncnt--;
				else if (pname[0] == 'R') rtncnt++;
				continue;
			}
			if (pname[0] == 'D') {  /* data label */
				if (dbgstrcmp(vname, &pname[8], dbgflags & DBGFLAGS_SENSITIVE)) continue;
				pname[8] = '\0';
				offset = atoi(&pname[1]);
				break;
			}
			if (pname[0] == 'R') rtncnt++;
			else if (pname[0] == 'C') clscnt = 1;
			else if (pname[0] == 'X' || (cname[0] && pname[0] == 'Y')) break;
		}
		rioclose(handle);
	}
	if (offset == -1) {
		ptr = " - unable to find";
		goto dbgfvar1;
	}
	vartable[varnum].offset = offset;

	for (i1 = i2 = 0; i2 < 3; i2++) {
		if (i2 == 0) ptr = getmname(pgmmod, datamod, TRUE);
		else if (i2 == 1) ptr = rname2;
		else if (i2 == 2) ptr = &var[varoff];
		i3 = (INT) strlen(ptr) + 1;
		memcpy(&pname[i1], ptr, i3);
		i1 += i3;
	}
	vartable[varnum].nameptr = memalloc(i1, 0);
	/* reset memory */
	setpgmdata();
	dbgsetmem();
	if (vartable[varnum].nameptr == NULL) {
		dbgerror(ERROR_FAILURE, "Not enough memory to store variable name");
		goto dbgfvar2;
	}
	memcpy(*vartable[varnum].nameptr, pname, i1);
	return(varnum);

dbgfvar1:
	strcat(var, ptr);
	dbgerror(ERROR_FAILURE, var);

dbgfvar2:
	dbgrvar(varnum);
	return RC_ERROR;
}

static void dbgrvar(INT varnum)
{
	INT i1;

	for (i1 = 0; i1 < vartable[varnum].arraycnt; i1++)
		if (vartable[varnum].array[i1] & 0x8000) dbgrvar(vartable[varnum].array[i1] & ~0x8000);
	if (vartable[varnum].dspoff & 0x8000) dbgrvar(vartable[varnum].dspoff & ~0x8000);
	memfree(vartable[varnum].nameptr);
	for (i1 = varnum; ++i1 < varhi; ) {
		if (!(vartable[i1].type & VAR_DSP)) continue;
		memcpy((CHAR *) &vartable[varnum], (CHAR *) &vartable[i1], sizeof(struct vardef));
		varnum = i1;
	}
	memset((CHAR *) &vartable[varnum], 0, sizeof(struct vardef));
	if (varnum + 1 == varhi) {
		while (varnum && !vartable[varnum - 1].type) varnum--;
		varhi = varnum;
	}
}

static UCHAR *dbggvar(INT varnum, INT *dmod, INT *off)
{
	INT i1, i2, i3, i4, datamod, pgmmod, mod, module, offset, firstflg;
	CHAR *ptr, pname[128];
	UCHAR *adr1, *adr2;

	strcpy(pname, (CHAR *) *vartable[varnum].nameptr);
	pgmmod = datamod = -1;
	findmname(pname, &pgmmod, &datamod);
	if (pgmmod == -1) {
		ptr = " - program module not loaded";
		goto dbggvar2;
	}
	if (datamod == -1) {
		ptr = " - data module not loaded";
		goto dbggvar2;
	}

	firstflg = TRUE;
	module = datamod;
	offset = vartable[varnum].offset;

dbggvar1:	
	for ( ; ; ) {
		if ((adr1 = getmdata(module)) == NULL) {
			ptr = " - error loading data module";
			goto dbggvar2;
		}
		adr1 += offset;
		if (*adr1 < 0xAF || *adr1 == 0xE0 || *adr1 == 0xE1 || *adr1 == 0xF0 || (*adr1 >= 0xF4 && *adr1 <= 0xFC)) break;
		mod = module;
		if (*adr1 >= 0xB0 && *adr1 <= 0xC7) {  /* address variable */
			mod = llhh(&adr1[1]);
			offset = llmmhh(&adr1[3]);
		}
		else if (*adr1 == 0xCE) offset = llmmhh(&adr1[1]);
		else if (*adr1 == 0xCF) {
			mod = 0;
			offset = llmmhh(&adr1[1]);
		}
		else if (*adr1 == 0xFD) mod = datachn;  /* common redirection */
		else break;
		if (getmdata(mod) == NULL) break;  /* points to nothing */
		if (offset & 0x00800000) break;  /* points to a label */
		module = mod;
	}
	if (firstflg && vartable[varnum].arraycnt) {
		if (vartable[varnum].arraycnt != *adr1 - 0xA6 + 1) {
			ptr = " - unmatched dimension for array index";
			goto dbggvar2;
		}
		adr1++;
		i2 = llhh(adr1);
		adr1 += 2;
		offset += 3;
		i4 = 0;
		for (i1 = 0; i1 < vartable[varnum].arraycnt; i1++) {
			if (vartable[varnum].array[i1] & 0x8000) {
				adr2 = dbggvar(vartable[varnum].array[i1] & ~0x8000, NULL, NULL);
				if (adr2 != NULL) scanvar(adr2);
				if (adr2 == NULL || !(vartype & TYPE_NUMERIC)) {
					ptr = " - unable to determine array index";
					goto dbggvar2;
				}
				i3 = nvtoi(adr2);
			}
			else i3 = vartable[varnum].array[i1];
			if (i3 < 1 || i3 > i2) {
				ptr = " - invalid array index value";
				goto dbggvar2;
			}
			i2 = llhh(adr1);
			adr1 += 2;
			offset += 2;
			i4 += i3 - 1;
			i4 *= i2;
		}
		//adr1 += i4;  was not doing anything
		offset += i4;
		firstflg = FALSE;
		goto dbggvar1;
	}
	if (dmod != NULL) *dmod = module;
	if (off != NULL) *off = offset;
	return(adr1);

dbggvar2:
	dbgnvar(varnum, dbgwork);
	strcat(dbgwork, ptr);
	return(NULL);
}

static void dbgnvar(INT varnum, CHAR *dest)
{
	INT i1, nameflg;
	CHAR *ptr;

	nameflg = !(vartable[varnum].flag & FLG_XNM);
	ptr = (CHAR *) *vartable[varnum].nameptr;

	/* module name */
	i1 = (INT) strlen(ptr);
	if (nameflg) {
		memcpy(dest, ptr, i1);
		dest += i1;
		*dest++ = ':';
	}
	ptr += i1 + 1;

	/* routine name */
	i1 = (INT) strlen(ptr);
	if (i1 && nameflg) {
		memcpy(dest, ptr, i1);
		dest += i1;
		*dest++ = '/';
	}
	ptr += i1 + 1;

	/* variable name */
	strcpy(dest, ptr);
}

INT appendfplpplclause(CHAR* prefix)
{
	INT i1;
	strcpy(prefix, "(FP=");
	i1 = 4;
	mscitoa(fp, &prefix[i1]);
	i1 += (INT) strlen(&prefix[i1]);
	strcpy(&prefix[i1], ",LP=");
	i1 += 4;
	mscitoa(lp, &prefix[i1]);
	i1 += (INT) strlen(&prefix[i1]);
	strcpy(&prefix[i1], ",PL=");
	i1 += 4;
	mscitoa(pl, &prefix[i1]);
	i1 += (INT) strlen(&prefix[i1]);
	prefix[i1++] = ')';
	return i1;
}

static void dbgvvar(INT varnum, UCHAR *adr, INT dmod, INT off, DBGDATAINFO *datainfo, INT *hexflg)
{
	static CHAR prefix[512], postfix[2];
	INT i1, i2, flag;
	INT32 x1;
	UCHAR *adr1;
	UCHAR work[MAX_NAMESIZE];

	postfix[0] = '\0';
	flag = vartable[varnum].flag;
	if (flag & FLG_ADR) {
		strcpy(prefix, " = ");
		if (!(flag & FLG_ADX) || (off & 0x00800000)) {
			if (!(flag & FLG_HEX)) mscitoa(vartable[varnum].offset, &prefix[3]);
			else dbgitox(vartable[varnum].offset, &prefix[3]);
		}
		else {
			strcat(prefix, getmname(getpgmmod(dmod), dmod, TRUE));
			i1 = (INT) strlen(prefix);
			prefix[i1++] = ':';
			if (!(flag & FLG_HEX)) mscitoa(off, &prefix[i1]);
			else dbgitox(off, &prefix[i1]);
		}
		lp = 0;
	}
	else {
		if (*adr == 0xFE) {  /* jump over "\xFE[record.]name\xFE" */
			for (i1 = 1; i1 < 67 && adr[i1] != 0xFE; i1++);
			if (i1 < 67) adr += i1 + 1;
		}
		scanvar(adr);
		if (vartype & TYPE_NULL) {
			strcpy(prefix, " = NULL");
			lp = 0;
		}
		else if (vartype & TYPE_CHARACTER) {
			if (vartype & TYPE_CHAR) i1 = appendfplpplclause(prefix);
			else i1 = 0;
			strcpy(&prefix[i1], " = ");
			i1 += 3;
			if (!(flag & FLG_HEX)) {
				prefix[i1++] = '"';
				prefix[i1] = '\0';
				strcpy(postfix, "\"");
			}
			if (vartable[varnum].dspoff) {
				if (vartable[varnum].dspoff & 0x8000) {
					adr1 = dbggvar(vartable[varnum].dspoff & ~0x8000, &i1, &i2);
					if (adr1 != NULL) scanvar(adr1);
					if (adr1 == NULL || !(vartype & TYPE_NUMERIC)) {
						strcpy(prefix, " - unable to determine starting position");
						postfix[0] = 0;
						lp = 0;
					}
					else fp = nvtoi(adr1) - 1;
				}
				else fp = vartable[varnum].dspoff - 1;
				if (fp < 0) fp = 0;
				else if (fp > lp) fp = lp;
				lp -= fp;
			}
			else {
				if (!fp) lp = 0;
				else if (flag & FLG_LLN) lp -= --fp;
				else fp = 0;
			}
			adr += hl + fp;
		}
		else if (vartype & TYPE_NUMERIC) {
			strcpy(prefix, " = ");
			if (vartype & (TYPE_INT | TYPE_FLOAT)) {  /* integer / float */
				if ((vartype & TYPE_FLOAT) || !(flag & FLG_HEX)) {
					nvmkform(adr, (UCHAR *) &prefix[2]);
					prefix[(UCHAR) prefix[2] - 0x80 + 3] = 0;
					prefix[2] = ' ';
				}
				else {
					memcpy(&x1, &adr[2], sizeof(INT32));
					dbgitox((INT) x1, &prefix[3]);
				}
				lp = 0;
			}
			else {
				adr += hl;
				flag &= ~FLG_HEX;
			}
		}
		else if (*adr >= 0xB0 && *adr <= 0xC7) {  /* address variable */
			i1 = llhh(&adr[1]);
			if (i1 != 0xFFFF && adr[5] == 0x80) {
				strcpy(prefix, " - LABEL variable");
				if (dbgflags & DBGFLAGS_EXTRAINFO) {
					i2 = (INT) strlen(prefix);
					prefix[i2++] = ' ';
					prefix[i2++] = '=';
					prefix[i2++] = ' ';
					mscitoa(i1, &prefix[i2]);
					i2 += (INT) strlen(&prefix[i2]);
					prefix[i2++] = ':';
					i1 = llhh(&adr[3]);
					mscitoa(i1, &prefix[i2]);
				}
			}
			else {
				strcpy(prefix, " - unreferenced address variable");
			}
			lp = 0;
		}
		else if (*adr >= 0xA0 && *adr <= 0xAE) {
			switch(*adr) {
				case 0xA0:  /* file */
				case 0xA1:  /* ifile */
				case 0xA2:  /* afile */
				case 0xA3:  /* comfile */
				case 0xA9:  /* pfile */
					strcpy(prefix, " - ");
					strcat(prefix, dbggetfiletypedesc((CHAR *)work, adr));
					break;
				case 0xA4:  /* list */
					strcpy(prefix, " - LIST variable");
					break;
				case 0xA5:  /* listend */
					strcpy(prefix, " - LISTEND");
					break;
				case 0xA6:  /* one dimensional array */
					strcpy(prefix, " - one dimension array variable");
					break;
				case 0xA7:  /* two dimensional array */
					strcpy(prefix, " - two dimension array variable");
					break;
				case 0xA8:  /* three dimensional array */
					strcpy(prefix, " - three dimension array variable");
					break;
				case 0xAA:  /* object */
				case 0xAB:  /* image */
				case 0xAC:  /* queue */
				case 0xAD:  /* resource */
				case 0xAE:  /* device */
					strcpy(prefix, " - ");
					strcat(prefix, dbggetspecialtypedesc((CHAR *)work, adr));
					break;
			}
			lp = 0;
		}
		else if (*adr == 0xCD) {  /* pre-version 8 address variables */
			strcpy(prefix, " - pre-version 8 unreferenced variable");
			lp = 0;
		}
		else if (*adr >= 0xCE && *adr <= 0xCF) {  /* local or global address variable */
			strcpy(prefix, " - local or global address variable");
			lp = 0;
		}
		else if (*adr == 0xF3) {  /* nformat & sformat */
			strcpy(prefix, " - NFORMAT or SFORMAT character");
			lp = 0;
		}
		else if (*adr == 0xFD) {  /* common area */
			strcpy(prefix, " - common area");
			lp = 0;
		}
		else {
			strcpy(prefix, " - unsupported variable type");
			lp = 0;
		}
	}

	datainfo[0].data = (UCHAR *) prefix;
	datainfo[0].size = (INT) strlen(prefix);
	datainfo[1].data = adr;
	datainfo[1].size = lp;
	datainfo[2].data = (UCHAR *) postfix;
	datainfo[2].size = (INT) strlen(postfix);
	*hexflg = flag & FLG_HEX;
}


CHAR* dbggetspecialtypedesc(CHAR *prefix, UCHAR *adr)
{
	DAVB *davb;
	davb = aligndavb(adr, NULL);
	if (davb->refnum) strcpy(prefix, "initialized");
	else strcpy(prefix, "uninitialized");
	if (davb->type == DAVB_OBJECT) strcat(prefix, " - OBJECT variable");
	else if (davb->type == DAVB_IMAGE) strcat(prefix, " - IMAGE variable");
	else if (davb->type == DAVB_QUEUE) strcat(prefix, " - QUEUE variable");
	else if (davb->type == DAVB_RESOURCE) strcat(prefix, " - RESOURCE variable");
	else strcat(prefix, " - DEVICE variable");
	return prefix;
}

CHAR* dbggetfiletypedesc(CHAR *prefix, UCHAR *adr)
{
	DAVB *davb;
	CHAR *ptr = NULL;
	
	davb = aligndavb(adr, NULL);
	if (davb->refnum) strcpy(prefix, "opened ");
	else strcpy(prefix, "closed ");
	if (davb->type == DAVB_FILE) {
		strcat(prefix, "FILE variable");
		ptr = diofilename(davb->refnum);
	}
	else if (davb->type == DAVB_IFILE) {
		strcat(prefix, "IFILE variable");
		ptr = dioindexname(davb->refnum);
	}
	else if (davb->type == DAVB_AFILE) {
		strcat(prefix, "AFILE variable");
		ptr = dioindexname(davb->refnum);
	}
	else if (davb->type == DAVB_COMFILE) strcat(prefix, "COMFILE variable");
	else strcat(prefix, "PFILE variable");
	if (ptr != NULL) {
		strcat(prefix, " (");
		strcat(prefix, ptr);
		strcat(prefix, ")");
	}
	return prefix;
}


static UCHAR *dbggadr(INT *dmod, INT *doff)
{
	INT module, off = 0, offset;
	UINT mod;
	UCHAR *adr;

	module = *dmod;
	offset = *doff;
	for ( ; ; ) {
		if ((adr = getmdata(module)) == NULL) return(NULL);
		adr += offset;
		if (*adr < 0xAF || *adr == 0xE0 || *adr == 0xE1 || *adr == 0xF0 || (*adr >= 0xF4 && *adr <= 0xFC)) break;
		mod = module;
		if (*adr >= 0xB0 && *adr <= 0xC7) {  /* address variable */
			mod = llhh(&adr[1]);
			off = llmmhh(&adr[3]);
		}
		else if (*adr == 0xCE) off = llmmhh(&adr[1]);
		else if (*adr == 0xCF) {
			mod = 0;
			off = llmmhh(&adr[1]);
		}
		else if (*adr == 0xFD) mod = datachn;  /* common redirection */
		else break;
		if (getmdata(mod) == NULL) break;  /* points to nothing */
		offset = off;
		if (offset & 0x00800000) break;  /* points to a label */
		module = mod;
	}
	*dmod = module;
	*doff = offset;
	return(adr);
}

INT dbgstrcmp(CHAR *s1, CHAR *s2, INT sensitive)
{
	int i1, i2;

	if (sensitive) return(strcmp(s1, s2));
	for (i1 = 0; ; i1++) {
		if ( (i2 = toupper(s1[i1]) - toupper(s2[i1])) ) return(i2);
		if (!s1[i1]) break;
	}
	return(0);
}

static void dbgitox(INT value, CHAR *string)
{
	int i1, i2;
	char work[33];

	work[32] = 0;
	i1 = 32;
	do {
		i2 = value & 0x0F;
		i1--;
		if (i2 < 10) work[i1] = (CHAR)(i2 + '0');
		else work[i1] = (CHAR)('A' + i2 - 10);
	} while ( (value = (UINT) value >> 4) );
	strcpy(string, &work[i1]);
}

static void scrnback()  /* restore screen */
{
	if (dbgflags & DBGFLAGS_SCREEN) {
		vidrestorescreen(*userscrnptr, screensize);
	}
	dbgflags &= ~(DBGFLAGS_SCREEN | DBGFLAGS_KEYIN);
}

static INT dbgcharprocess()
{
	static INT commands[26] = {
		COMMAND_ALTERFLAGS, COMMAND_PERMBREAK, COMMAND_CLEARBREAK,
		COMMAND_DISPLAYVAR, 0, COMMAND_CHANGESRC,
		COMMAND_GO, COMMAND_HELP, COMMAND_SETTAB,
		COMMAND_UPRETURN, COMMAND_DOWNRETURN, COMMAND_FINDLABEL,
		COMMAND_MODIFYVAR, COMMAND_CHANGEMOD, COMMAND_OUTPUTSCN,
		COMMAND_VARBREAK, COMMAND_QUIT, COMMAND_REMOVEVAR,
		COMMAND_SEARCH, COMMAND_TEMPBREAK, COMMAND_STEPOUT,
		COMMAND_VIEWVAR, COMMAND_NEXTWIN, COMMAND_STEPSINGLE,
		COMMAND_GOTOLINE, COMMAND_STEPOVER
	};
	static INT xcommands[26] = {
		COMMAND_VARBRKANYCHG, COMMAND_TOGGLEBREAK, COMMAND_CLEARGLOBAL,
		0, COMMAND_SLOWEXEC, 0,
		COMMAND_SAVEGLOBAL, 0, 0,
		0, 0, COMMAND_RESTGLOBAL,
		COMMAND_TOGGLEMORE, COMMAND_TOGGLELINE, 0,
		COMMAND_SAVEPREF, 0, COMMAND_TOGGLERESULT,
		COMMAND_TOGGLECASE, 0, 0,
		COMMAND_TOGGLEVIEW, 0, 0,
		0, 0
	};
	INT chr, cmd, cnt, nextchr;
	INT32 vidcmd[3];

	nextchr = 0;
	while (!executeflag) {
		dbgchardisplay(dspflg);
		dspflg = 0;
		if (dbgflags & DBGFLAGS_SLOW) {
			vidcmd[0] = VID_END_FLUSH;
			vidput(vidcmd);
			kdspause(1);
			return(EXECUTE_SLOW);
		}
		if (nextchr > 0) {
			chr = nextchr;
			nextchr = 0;
		}
		else {
			vidcmd[0] = VID_HORZ | 5;
			vidcmd[1] = VID_VERT | 0;
			vidcmd[2] = VID_END_NOFLUSH;
			vidput(vidcmd);
			chr = dbgcharget(TRUE);
		}
		if (chr == -1) {
			return(EXECUTE_QUIT);
		}
		cnt = 1;
		if (chr == VID_UP || chr == VID_DOWN || chr == VID_PGUP || chr == VID_PGDN || chr == VID_LEFT || chr == VID_RIGHT) {
			vidcmd[0] = VID_KBD_TIMEOUT;
			vidcmd[1] = VID_END_NOFLUSH;
			vidput(vidcmd);
			while ((nextchr = dbgcharget(FALSE)) == chr) cnt++;
			vidcmd[0] = VID_KBD_TIMEOUT | 0xFFFF;
			vidcmd[1] = VID_END_NOFLUSH;
			vidput(vidcmd);
		}
		cmd = 0;
		if (chr < 256) {
			chr = toupper(chr);
			if (chr == 'E') {
				dspflg |= COMMAND_LINE;
				dbgcharput((UCHAR *) "EXTCMD: ", 8, 0, 0);
				chr = dbgcharget(TRUE);
				if (chr == -1) {
					return(EXECUTE_QUIT);
				}
				if (chr < 256) {
					chr = toupper(chr);
					if (chr >= 'A' && chr <= 'Z') cmd = xcommands[chr - 'A'];
				}
			}
			else if (chr >= 'A' && chr <= 'Z') cmd = commands[chr - 'A'];
			else {
				switch(chr) {
				case '?': cmd = COMMAND_HELP; break;
				case '/': cmd = COMMAND_SEARCHAGN; break;
				case '#': cmd = COMMAND_TOGGLELINE; break;
				case '%': cmd = COMMAND_TOGGLECASE; break;
				case '|': cmd = COMMAND_WRITESCN; break;
				case '~': cmd = COMMAND_PGMCODE; break;
				case '`': cmd = COMMAND_DATACODE; break;
				case '+': cmd = COMMAND_INCREASEWIN; break;
				case '-': cmd = COMMAND_DECREASEWIN; break;
				}
			}
		}
		else {
			switch(chr) {
			case VID_TAB: cmd = COMMAND_NEXTWIN; break;
			case VID_BKTAB: cmd = COMMAND_PREVWIN; break;
			case VID_UP: cmd = COMMAND_SCROLLUP; break;
			case VID_DOWN: cmd = COMMAND_SCROLLDOWN; break;
			case VID_LEFT: cmd = COMMAND_SCROLLLEFT; break;
			case VID_RIGHT: cmd = COMMAND_SCROLLRIGHT; break;
			case VID_PGUP: cmd = COMMAND_PAGEUP; break;
			case VID_PGDN: cmd = COMMAND_PAGEDOWN; break;
			case VID_HOME: cmd = COMMAND_GOTOFIRST; break;
			case VID_END: cmd = COMMAND_GOTOEND; break;
			}
		}
		if (cmd) dbgfunction(cmd, window, cnt);
	}
	return(executeflag);
}

static void dbgwritescreen()
{
	static CHAR scrnfile[100] = { "DBC.SCR" };
	static INT scrnfilesize = 7;
	INT handle, size;
	OFFSET eofpos;

	dspflg |= COMMAND_LINE;
	dbgcharput((UCHAR *) "File: ", 6, 0, 0);
	if (dbgcharedit((UCHAR *) scrnfile, &scrnfilesize, sizeof(scrnfile), 6, 0, 72) != VID_ENTER) return;
	if (!scrnfilesize) return;

	scrnfile[scrnfilesize] = 0;
	/* handle = fioopen(scrnfile, FIO_M_EXC | FIO_P_TXT); */
	/* if (handle == ERR_FNOTF) */
		handle = fioopen(scrnfile, FIO_M_PRP | FIO_P_TXT);
	if (handle < 0) {
		dbgerror(ERROR_FAILURE, "Unable to open/create file");
		return;
	}
	fiogetsize(handle, &eofpos);
	size = vidstatesize();
	if (fiowrite(handle, eofpos, *userscrnptr + size, screensize - size))
		dbgerror(ERROR_FAILURE, "Error writing to file");
	else dbgresultline("Written");
	fioclose(handle);
}

static void dbgcharviewwin()
{
	INT i1;

	if (!(dbgflags & DBGFLAGS_VIEWWIN)) {
		dbgflags |= DBGFLAGS_VIEWWIN;
		i1 = wininfo[1].height + 1;
		if (i1 > wininfo[0].height - 1) {
			wininfo[2].height -= i1 - (wininfo[0].height - 1);
			i1 = wininfo[0].height - 1;
		}
		wininfo[0].height -= i1;
		lastwininfo[1].firstline = INT_MAX;
		lastwininfo[1].line = -1;
	}
	else {
		dbgflags &= ~DBGFLAGS_VIEWWIN;
		wininfo[0].height += wininfo[1].height + 1;
		if (window == 1) {
			if (dbgflags & DBGFLAGS_VIEWWIN) window = 2;
			else window = 0;
		}
	}
	dspflg = INITIALIZE;
	return;
}

static void dbgcharresultwin()
{
	INT i1;

	if (!(dbgflags & DBGFLAGS_RESULTWIN)) {
		dbgflags |= DBGFLAGS_RESULTWIN;
		i1 = wininfo[2].height + 1;
		if (i1 > wininfo[0].height - 1) {
			wininfo[1].height -= i1 - (wininfo[0].height - 1);
			i1 = wininfo[0].height - 1;
		}
		wininfo[0].height -= i1;
		lastwininfo[1].firstline = INT_MAX;
		lastwininfo[1].line = -1;
	}
	else {
		dbgflags &= ~DBGFLAGS_RESULTWIN;
		wininfo[0].height += wininfo[2].height + 1;
		if (window == 2) window = 0;
	}
	dspflg = INITIALIZE;
	return;
}

static void dbgchardisplay(INT flag)
{
	static INT lastwindow;
	INT i1, i2, win, vidcnt;
	INT32 vidcmd[12];

	if (flag == (INT) INITIALIZE) {
		vidreset();
		vidcmd[0] = VID_WIN_RESET;
		vidcmd[1] = VID_FOREGROUND | VID_WHITE;
		vidcmd[2] = VID_BACKGROUND | VID_BLACK;
		vidcmd[3] = VID_ES;
		vidcmd[4] = VID_END_NOFLUSH;
		vidput(vidcmd);
		dbgcharput((UCHAR *) "CMD:", 4, 0, 0);
		vidcnt = 0;
		vidcmd[vidcnt++] = VID_FOREGROUND | VID_GREEN;
		i1 = 1;
/*** CODE: DO NOT DISPLAY NON-ACTIVE WINDOWS, MAYBE REMOVE LOWER BAR ***/
		for (win = 0; win < 3; win++) {
			if (win == 1) {
				if (!(dbgflags & DBGFLAGS_VIEWWIN)) continue;
			}
			else if (win == 2 && !(dbgflags & DBGFLAGS_RESULTWIN)) continue;
			vidcmd[vidcnt++] = VID_HORZ | 0;
			vidcmd[vidcnt++] = VID_VERT | i1;
			vidcmd[vidcnt++] = VID_REPEAT | maxwidth;
			vidcmd[vidcnt++] = VID_DISP_SYM | VID_SYM_HLN;
			if (win == window) vidcmd[vidcnt++] = VID_REV_ON;
			vidcmd[vidcnt] = VID_END_NOFLUSH;
			vidput(vidcmd);
			vidcnt = 0;
			i2 = (INT) strlen(wininfo[win].title);
			dbgcharput((UCHAR *) wininfo[win].title, i2, (maxwidth - i2) / 2, i1);
			if (win == window) vidcmd[vidcnt++] = VID_REV_OFF;
			i1 += wininfo[win].height + 1;
			if (win == 0) {
				vidcmd[vidcnt++] = VID_HORZ | 0;
				vidcmd[vidcnt++] = VID_VERT | i1;
				vidcmd[vidcnt++] = VID_REPEAT | maxwidth;
				vidcmd[vidcnt++] = VID_DISP_SYM | VID_SYM_HLN;
				i1 += 2;
			}
		}
		vidcmd[vidcnt++] = VID_FOREGROUND | VID_WHITE;
		vidcmd[vidcnt] = VID_END_NOFLUSH;
		vidput(vidcmd);
		dspflg = ((WIN_REDISPLAY | WIN_CENTERLINE | WIN_SETCOL) << MAIN_SHIFT) |
				((WIN_REDISPLAY | WIN_FIRSTLINE) << VIEW_SHIFT) |
				((WIN_REDISPLAY | WIN_FIRSTLINE) << RESULT_SHIFT);
	}
	if (flag & COMMAND_LINE) {
		dbgcharput((UCHAR *) "CMD:", 4, 0, 0);
		vidcmd[0] = VID_EL;
		vidcmd[1] = VID_END_NOFLUSH;
		vidput(vidcmd);
	}
	if ((flag >> MAIN_SHIFT) & WIN_MASK) dbgcharwindisplay(0, (flag >> MAIN_SHIFT) & WIN_MASK);
	if (flag & STATUS_FLAGS) {
		strcpy(dbgwork, "FLAGS: zloe  PCOUNT: ");
		if (dbcflags & DBCFLAG_EQUAL) dbgwork[7] = 'Z';
		if (dbcflags & DBCFLAG_LESS) dbgwork[8] = 'L';
		if (dbcflags & DBCFLAG_OVER) dbgwork[9] = 'O';
		if (dbcflags & DBCFLAG_EOS) dbgwork[10] = 'E';
		if (dbgpcount != -1) mscitoa(dbgpcount, &dbgwork[21]);
		else dbgwork[11] = 0;
		strcat(dbgwork, "  MODULE: ");
		strcat(dbgwork, getmname(dbgpgmmod, dbgdatamod, TRUE));
		if (dbgflags & DBGFLAGS_DBGOPEN) {
			strcat(dbgwork, "  SOURCE: ");
			strcat(dbgwork, &nametable[sourcetable[srcfile].nametableptr]);
		}
		i1 = (INT) strlen(dbgwork);
		dbgcharput((UCHAR *) dbgwork, i1, 0, wininfo[0].height + 3);
		if (i1 < maxwidth) {
			vidcmd[0] = VID_EL;
			vidcmd[1] = VID_END_NOFLUSH;
			vidput(vidcmd);
		}
	}
	if ((flag >> VIEW_SHIFT) & WIN_MASK) dbgcharwindisplay(1, (flag >> VIEW_SHIFT) & WIN_MASK);
	if ((flag >> RESULT_SHIFT) & WIN_MASK) dbgcharwindisplay(2, (flag >> RESULT_SHIFT) & WIN_MASK);
	if ((flag & WIN_ACTIVE) && window != lastwindow) {
		if (lastwindow == 0) i1 = 1;
		else {
			i1 = wininfo[0].height + 4;
			if (lastwindow == 2 && (dbgflags & DBGFLAGS_VIEWWIN)) i1 += wininfo[1].height + 1;
		}
		vidcmd[0] = VID_FOREGROUND | VID_GREEN;
		vidcmd[1] = VID_END_NOFLUSH;
		vidput(vidcmd);
		i2 = (INT) strlen(wininfo[lastwindow].title);
		dbgcharput((UCHAR *) wininfo[lastwindow].title, i2, (maxwidth - i2) / 2, i1);

		if (window == 0) i1 = 1;
		else {
			i1 = wininfo[0].height + 4;
			if (window == 2 && (dbgflags & DBGFLAGS_VIEWWIN)) i1 += wininfo[1].height + 1;
		}
		vidcmd[0] = VID_REV_ON;
		vidcmd[1] = VID_END_NOFLUSH;
		vidput(vidcmd);
		i2 = (INT) strlen(wininfo[window].title);
		dbgcharput((UCHAR *) wininfo[window].title, i2, (maxwidth - i2) / 2, i1);
		vidcmd[0] = VID_REV_OFF;
		vidcmd[1] = VID_FOREGROUND | VID_WHITE;
		vidcmd[2] = VID_END_NOFLUSH;
		vidput(vidcmd);
	}
	lastwindow = window;
}

static void dbgcharwindisplay(INT win, INT flag)
{
	static INT lastdisplaylines[3];
	INT i1, i2, i3, displaylines, start[3], end[3];
	INT32 vidcmd[6];

	if (win == 1) {
		if (!(dbgflags & DBGFLAGS_VIEWWIN)) {
			return;
		}
	}
	else if (win == 2 && !(dbgflags & DBGFLAGS_RESULTWIN)) {
		return;
	}

	if (wininfo[win].height != lastwininfo[win].height || wininfo[win].maxline != lastwininfo[win].maxline) {
		wininfo[win].displaylines = wininfo[win].height;
		lastwininfo[win].height = wininfo[win].height;
		lastwininfo[win].maxline = wininfo[win].maxline;
		if (win == 0) wininfo[0].sourceline = -1;
	}

	if (flag & (WIN_CENTERLINE | WIN_LINE)) {
		if (wininfo[win].line >= wininfo[win].maxline) wininfo[win].line = wininfo[win].maxline - 1;
		if (wininfo[win].line < 0) wininfo[win].line = 0;
		if (win != 0) wininfo[win].firstline = wininfo[win].line;
	}
	if (flag & WIN_CENTERLINE) {
		wininfo[win].firstline = wininfo[win].line - ((wininfo[win].displaylines - 1) >> 1);
		wininfo[win].firstcolumn = 0;
	}
	if (flag & WIN_LINE) {
		if (wininfo[win].line - wininfo[win].displaylines >= wininfo[win].firstline) wininfo[win].firstline = wininfo[win].line - wininfo[win].displaylines + 1;
		if (wininfo[win].line < wininfo[win].firstline) wininfo[win].firstline = wininfo[win].line;
	}
	if (flag & (WIN_CENTERLINE | WIN_LINE | WIN_FIRSTLINE)) {
		if (wininfo[win].firstline > wininfo[win].maxline - wininfo[win].displaylines) wininfo[win].firstline = wininfo[win].maxline - wininfo[win].displaylines;
		if (wininfo[win].firstline < 0) wininfo[win].firstline = 0;
		if (win != 0) wininfo[win].line = wininfo[win].firstline;
	}
	if (flag & (WIN_CENTERLINE | WIN_FIRSTCOLUMN)) {
		if (wininfo[win].firstcolumn > wininfo[win].maxcolumn - wininfo[win].displaycolumns) wininfo[win].firstcolumn = wininfo[win].maxcolumn - wininfo[win].displaycolumns;
		if (wininfo[win].firstcolumn < 0) wininfo[win].firstcolumn = 0;
		if (wininfo[win].firstcolumn != lastwininfo[win].firstcolumn) flag |= WIN_REDISPLAY;
	}

	displaylines = wininfo[win].displaylines;
	if (displaylines > wininfo[win].maxline - wininfo[win].firstline) displaylines = wininfo[win].maxline - wininfo[win].firstline;
	wininfo[win].loadline(wininfo[win].firstline, displaylines, flag);

	if (!(flag & WIN_REDISPLAY)) {
		if (lastwininfo[win].line < wininfo[win].firstline || lastwininfo[win].line >= wininfo[win].firstline + displaylines) lastwininfo[win].line = -1;
		if (wininfo[win].firstline == lastwininfo[win].firstline + 1) {  /* move screen up one line */
			dbgcharroll(win, VID_RU);
			start[1] = displaylines - 1;
			end[1] = displaylines;
		}
		else if (wininfo[win].firstline == lastwininfo[win].firstline - 1) {  /* move screen down one line */
			dbgcharroll(win, VID_RD);
			start[1] = 0;
			end[1] = 1;
		}
		else if (wininfo[win].firstline != lastwininfo[win].firstline) flag |= WIN_REDISPLAY;
		else start[1] = end[1] = displaylines;
	}
	start[0] = end[0] = start[2] = end[2] = displaylines;
	if (flag & WIN_REDISPLAY) {
		start[1] = 0;
		end[1] = displaylines;
		lastwininfo[win].line = -1;
	}
	if (win == 0) {
		if (wininfo[win].line != lastwininfo[win].line) {
			if (wininfo[win].line >= wininfo[win].firstline && wininfo[win].line < wininfo[win].firstline + displaylines) {
				start[0] = wininfo[win].line - wininfo[win].firstline;
				end[0] = start[0] + 1;
			}
			if (lastwininfo[win].line != -1) {
				start[2] = lastwininfo[win].line - wininfo[win].firstline;
				end[2] = start[2] + 1;
				lastwininfo[win].line = -1;
			}
		}
	}

/*** CODE: COULD FIGURE OUT LOWEST START & HIGHEST END TO LOOP LESS IF NOT WIN_SETCOL ***/
	for (i1 = 0, i3 = wininfo[win].firstsource - 1; i1 < displaylines; i1++) {
		if (++i3 == wininfo[win].maxsource) i3 = 0;
		if (win == 0) {
			if ((flag & WIN_REDISPLAY) || srclines[i3].bchar != srclines[i3].lchar) {
				srclines[i3].lchar = srclines[i3].bchar;
				vidcmd[0] = VID_HORZ | 0;
				vidcmd[1] = VID_VERT | (i1 + 2);
				if (srclines[i3].bchar) vidcmd[2] = VID_DISP_CHAR | srclines[i3].bchar;
				else vidcmd[2] = VID_DISP_CHAR | ' ';
				vidcmd[3] = VID_END_NOFLUSH;
				vidput(vidcmd);
			}
			if (i1 >= start[0] && i1 < end[0]) {
				vidcmd[0] = VID_REV_ON;
				vidcmd[1] = VID_END_NOFLUSH;
				vidput(vidcmd);
			}
			else if ((i1 < start[1] || i1 >= end[1]) && (i1 < start[2] || i1 >= end[2])) continue;
			dbgcharmaindisplayline(i1, i3);
			if (i1 >= start[0] && i1 < end[0]) {
				vidcmd[0] = VID_REV_OFF;
				vidcmd[1] = VID_END_NOFLUSH;
				vidput(vidcmd);
				lastwininfo[0].line = wininfo[0].line;
			}
		}
		else {
			if (i1 < start[1] || i1 >= end[1]) continue;
			wininfo[win].displayline(i1, i3);
		}
	}
	if (displaylines < lastdisplaylines[win] && displaylines < wininfo[win].height) {
		i2 = lastdisplaylines[win];
		if (i2 > wininfo[win].height) i2 = wininfo[win].height;
		if (win == 0) i3 = 2;
		else {
			i3 = wininfo[0].height + 5;
			if (win == 2 && (dbgflags & DBGFLAGS_VIEWWIN)) i3 += wininfo[1].height + 1;
		}
		for (i1 = displaylines; i1 < i2; i1++) {
			vidcmd[0] = VID_HORZ | 0;
			vidcmd[1] = VID_VERT | (i3 + i1);
			vidcmd[2] = VID_EL;
			vidcmd[3] = VID_END_NOFLUSH;
			vidput(vidcmd);
		}
	}
	lastdisplaylines[win] = displaylines;

	if (wininfo[win].width != lastwininfo[win].width || wininfo[win].maxcolumn != lastwininfo[win].maxcolumn) {
		wininfo[win].displaycolumns = wininfo[win].width - (linenumsize + 3);
		lastwininfo[win].width = wininfo[win].width;
		lastwininfo[win].maxcolumn = wininfo[win].maxcolumn;
	}

	lastwininfo[win].firstline = wininfo[win].firstline;
	lastwininfo[win].firstcolumn = wininfo[win].firstcolumn;
}

static void dbgcharmaindisplayline(INT linenum, INT src)
{
	INT i1, i2, column, tabcnt, len, hpos, vpos;
	UINT vidcnt;
	INT32 vidcmd[16];
	CHAR work[16];
	UCHAR *line;

	vpos = linenum + 2;
	if (dbgflags & DBGFLAGS_LINENUM) {
		mscitoa(wininfo[0].firstline + linenum + 1, work);
		i1 = (INT) strlen(work);
		work[i1++] = ':';
		memset(&work[i1], ' ' , linenumsize + 2 - i1);
		dbgcharput((UCHAR *) work, linenumsize + 2, 1, vpos);
		hpos = linenumsize + 3;
	}
	else hpos = 1;

	line = srclines[src].line;
	len = srclines[src].size;
	vidcnt = 0;
	column = 0;
	tabcnt = 0;
	for (i1 = i2 = 0; ; i1++) {
		if (i1 < len && line[i1] != '\t') continue;
		if (i2 != i1) {
			if (column < wininfo[0].firstcolumn) {
				if (i1 - i2 > wininfo[0].firstcolumn - column) {
					i2 += wininfo[0].firstcolumn - column;
					column = wininfo[0].firstcolumn;
				}
			}
			if (column >= wininfo[0].firstcolumn && hpos < maxwidth) {
				if (vidcnt) {
					vidcmd[vidcnt] = VID_END_NOFLUSH;
					vidput(vidcmd);
					vidcnt = 0;
				}
				dbgcharput(&line[i2], i1 - i2, hpos, vpos);
				hpos += i1 - i2;
			}
			column += i1 - i2;
		}
		if (i1 == len) break;

		i2 = column;
		while (tabcnt < MAXTABS && column >= tabindent[tabcnt]) tabcnt++;
		if (tabcnt < MAXTABS) column = tabindent[tabcnt];
		else column++;
		if (column > wininfo[0].firstcolumn && hpos < maxwidth) {
			if (i2 < wininfo[0].firstcolumn) i2 = wininfo[0].firstcolumn;
			i2 = column - i2;
			if (hpos == 1) {
				vidcmd[vidcnt++] = VID_HORZ | 1;
				vidcmd[vidcnt++] = VID_VERT | vpos;
			}
			if (vidcnt + 3 > (sizeof(vidcmd) / sizeof(*vidcmd))) {
				vidcmd[vidcnt] = VID_END_NOFLUSH;
				vidput(vidcmd);
				vidcnt = 0;
			}
			if (i2 > 1) vidcmd[vidcnt++] = VID_REPEAT | i2;
			vidcmd[vidcnt++] = VID_DISP_CHAR | ' ';
			hpos += i2;
		}
		i2 = i1 + 1;
	}
	if (hpos < maxwidth) vidcmd[vidcnt++] = VID_EL;
	if (vidcnt) {
		vidcmd[vidcnt] = VID_END_NOFLUSH;
		vidput(vidcmd);
	}
	if (column > wininfo[0].maxcolumn) wininfo[0].maxcolumn = column;
}

static void dbgcharviewdisplayline(INT linenum, INT src)
{
	INT i1, i2, i3, i4, i5, cnt, column, dmod, hexflg, hpos, off, vpos;
	INT32 vidcmd[2];
	UCHAR c1, work[129], *adr1;
	DBGDATAINFO datainfo[4];

	for (i1 = 0; i1 < varhi; i1++) {
		if (!(vartable[i1].type & VAR_DSP)) continue;
		if (!src--) break;
	}

	adr1 = dbggvar(i1, &dmod, &off);
	if (adr1 != NULL) dbgnvar(i1, dbgwork);
	datainfo[0].data = (UCHAR *) dbgwork;
	datainfo[0].size = (INT) strlen(dbgwork);
	if (adr1 != NULL) dbgvvar(i1, adr1, dmod, off, &datainfo[1], &hexflg);
	else datainfo[1].size = datainfo[2].size = datainfo[3].size = 0;

	vpos = linenum + wininfo[0].height + 5;
	column = hpos = 0;
	for (cnt = 0; cnt < 4; cnt++) {
		i1 = datainfo[cnt].size;
		if (i1) {
			if (cnt == 2 && hexflg) i1 <<= 1;
			i2 = 0;
			if (column < wininfo[1].firstcolumn) i2 += wininfo[1].firstcolumn - column;
			column += i1;
			if (column > wininfo[1].firstcolumn) {
				if (hpos < maxwidth) {
					if (cnt != 2 || !hexflg) {
						if (cnt != 2) {
							dbgcharput(&datainfo[cnt].data[i2], i1 - i2, hpos, vpos);
							hpos += i1 - i2;
						}
						else {
							adr1 = datainfo[2].data;
							for (i3 = 0; ; ) {
								if ((c1 = adr1[i2++]) < ' ') c1 = '?';
								work[i3++] = c1;
								if (i2 == i1 || i3 >= (INT) sizeof(work)) {
									dbgcharput(work, i3, hpos, vpos);
									hpos += i3;
									if (i2 == i1 || hpos >= maxwidth) break;
									i3 = 0;
								}
							}
						}
					}
					else {
						adr1 = datainfo[2].data;
						for (i3 = 0; ; ) {
							i4 = adr1[i2 >> 1];
							if (!(i2 & 0x01)) {
								if ((i5 = (i4 >> 4)) < 10) i5 += '0';
								else i5 += 'A' - 10;
								work[i3++] = (UCHAR) i5;
								i2++;
							}
							if ((i5 = (i4 & 0x0F)) < 10) i5 += '0';
							else i5 += 'A' - 10;
							work[i3++] = (UCHAR) i5;
							i2++;
							if (i2 == i1 || i3 >= (INT) (sizeof(work) - 1)) {
								dbgcharput(work, i3, hpos, vpos);
								hpos += i3;
								if (i2 == i1 || hpos >= maxwidth) break;
								i3 = 0;
							}
						}
					}
				}
			}
		}
	}
	if (hpos < maxwidth) {
		vidcmd[0] = VID_EL;
		vidcmd[1] = VID_END_NOFLUSH;
		vidput(vidcmd);
	}
	if (column > wininfo[1].maxcolumn) wininfo[1].maxcolumn = column;
}

static void dbgcharresultdisplayline(INT linenum, INT src)
{
	INT hpos, vpos;
	INT32 vidcmd[2];

	vpos = linenum + wininfo[0].height + 5;
	if (dbgflags & DBGFLAGS_VIEWWIN) vpos += wininfo[1].height + 1;
	if (wininfo[2].firstcolumn < resultlines[src].size) {
		hpos = resultlines[src].size - wininfo[2].firstcolumn;
		dbgcharput(*resultlines[src].line + wininfo[2].firstcolumn, hpos, 0, vpos);
	}
	else hpos = 0;
	if (hpos < maxwidth) {
		vidcmd[0] = VID_EL;
		vidcmd[1] = VID_END_NOFLUSH;
		vidput(vidcmd);
	}
	if (resultlines[src].size > wininfo[2].maxcolumn) wininfo[2].maxcolumn = resultlines[src].size;
}

static void dbgcharput(UCHAR *string, INT len, INT hpos, INT vpos)
{
	INT vidcnt;
	INT32 vidcmd[8];

	if (len == -1) len = (INT)strlen((CHAR*)string);
	vidcmd[0] = VID_HORZ | hpos;
	vidcmd[1] = VID_VERT | vpos;
	vidcmd[2] = VID_DISPLAY | len;
	vidcnt = 3;
	if (sizeof(UCHAR *) > sizeof(INT32)) {
		memcpy((UCHAR *) &vidcmd[vidcnt], (UCHAR *) &string, sizeof(UCHAR *));
		vidcnt += (sizeof(UCHAR *) + sizeof(INT32) - 1) / sizeof(INT32);
	}
	else *(UCHAR **)(&vidcmd[vidcnt++]) = string;
	vidcmd[vidcnt] = VID_END_NOFLUSH;
	vidput(vidcmd);
}

static INT dbgcharedit(UCHAR *string, INT *sizeptr, INT maxsize, INT hpos, INT vpos, INT dsplen)
{
	INT i1, blank, chr, cursorflg, dsppos, insertflg, lastpos, lastsize;
	INT pos, size, start, strpos, vidcnt;
	INT32 vidcmd[6];

	pos = -1;
	size = *sizeptr;
	if (size) {
		i1 = (size < dsplen) ? size : dsplen;
		vidcmd[0] = VID_REV_ON;
		vidcmd[1] = VID_END_NOFLUSH;
		vidput(vidcmd);
		dbgcharput(string, i1, hpos, vpos);
		vidcmd[0] = VID_REV_OFF;
		vidcmd[1] = VID_END_NOFLUSH;
		vidput(vidcmd);
		cursorflg = FALSE;
	}
	else {
		vidcmd[0] = VID_HORZ | hpos;
		vidcmd[1] = VID_VERT | vpos;
		vidcmd[2] = VID_END_NOFLUSH;
		vidput(vidcmd);
		cursorflg = TRUE;
	}
	insertflg = TRUE;
	for ( ; ; ) {
		chr = dbgcharget(cursorflg);
		if (chr == VID_DOWN) chr = VID_TAB;
		else if (chr == VID_UP) chr = VID_BKTAB;
		if (chr == VID_ENTER || chr == VID_ESCAPE || chr == VID_TAB || chr == VID_BKTAB || chr == -1) break;

		if (pos == -1) {
			if (chr < 256 || chr == VID_BKSPC) size = 0;
			else if (chr == VID_RIGHT) chr = VID_END;
			i1 = size;
			if (i1 > dsplen) i1 = dsplen;
			dbgcharput(string, i1, hpos, vpos);
			vidcnt = 0;
			if (i1 < dsplen) {
				vidcmd[vidcnt++] = VID_REPEAT | dsplen - i1;
				vidcmd[vidcnt++] = VID_DISP_CHAR | ' ';
			}
			vidcmd[vidcnt++] = VID_HORZ | hpos;
			vidcmd[vidcnt] = VID_END_NOFLUSH;
			vidput(vidcmd);
			start = pos = 0;
			cursorflg = TRUE;
		}
		lastpos = pos;
		lastsize = size;
		if (chr >= 256) {
			switch(chr) {
				case VID_BKSPC:
					if (pos) {
						memmove(&string[pos - 1], &string[pos], size - pos);
						pos--;
						size--;
					}
					break;
				case VID_LEFT:
					if (pos) pos--;
					break;
				case VID_RIGHT:
					if (pos < size) pos++;
					break;
				case VID_INSERT:
					insertflg = !insertflg;
					break;
				case VID_DELETE:
					if (pos < size) {
						size--;
						memmove(&string[pos], &string[pos + 1], size - pos);
					}
					break;
				case VID_HOME:
					pos = 0;
					break;
				case VID_END:
					pos = size;
					break;
			}
		}
		else if (chr >= 0x20) {
			if (size < maxsize || (pos < size && !insertflg)) {
				if (pos == size || insertflg) {
					if (pos != size) memmove(&string[pos + 1], &string[pos], size - pos);
					size++;
				}
				string[pos++] = chr;
			}
			else {
				vidcmd[0] = VID_BEEP | 3;
				vidcmd[1] = VID_END_NOFLUSH;
				vidput(vidcmd);
			}
		}
		if (pos != lastpos || size != lastsize) {
			blank = 0;
			strpos = -1;
			dsppos = 0;
			if (pos < start) {
				if (size == lastsize) strpos = start = pos;
			}
			else if (start < pos - dsplen + 1) {
				strpos = start = pos - dsplen + 1;
				blank = ' ';
			}
			else if (size < lastsize) {
				strpos = pos;
				dsppos = pos - start;
				blank = ' ';
			}
			else if (size > lastsize && pos < size) {
				strpos = lastpos;
				dsppos = lastpos - start;
			}
			else if (chr < 256) blank = string[lastpos];
			if (strpos >= 0) {
				i1 = size - strpos;
				if (i1 > dsplen - dsppos) i1 = dsplen - dsppos;
				dbgcharput(&string[strpos], i1, hpos + dsppos, vpos);
			}
			else i1 = 0;
			vidcnt = 0;
			if (blank && i1 < dsplen - dsppos) vidcmd[vidcnt++] = VID_DISP_CHAR | blank;
			vidcmd[vidcnt++] = VID_HORZ | (hpos + pos - start);
			vidcmd[vidcnt++] = VID_END_NOFLUSH;
			vidput(vidcmd);
		}
	}

	*sizeptr = size;
	return(chr);
}

static void dbgcharroll(INT win, INT roll)
{
	INT top, bottom;
	INT32 vidcmd[6];

	if (win == 0) {
		top = 2;
		bottom = wininfo[0].height + 1;
	}
	else {
		top = wininfo[0].height + 5;
		if (win == 1) bottom = top + wininfo[1].height - 1;
		else {
			if (dbgflags & DBGFLAGS_VIEWWIN) top += wininfo[1].height + 1;
			bottom = top + wininfo[2].height - 1;
		}
	}
	
	vidcmd[0] = VID_WIN_TOP | top;
	vidcmd[1] = VID_WIN_BOTTOM | bottom;
	vidcmd[2] = roll;
	vidcmd[3] = VID_WIN_RESET;
	vidcmd[4] = VID_END_NOFLUSH;
	vidput(vidcmd);
}

static INT dbgcharget(INT cursflg)
{
	INT i1, i2;
	INT32 vidcmd[6];
	UCHAR work[(VID_MAXKEYVAL >> 3) + 1];

	if (!(dbgflags & DBGFLAGS_KEYIN)) {
		memset(work, 0xFF, sizeof(work));
		vidkeyinfinishmap(work);
		vidcmd[0] = VID_KBD_RESET;
		vidcmd[1] = VID_KBD_ED_OFF;
		vidcmd[2] = VID_KBD_IN;
		if (cursflg) vidcmd[3] = VID_CURSOR_NORM;
		else vidcmd[3] = VID_CURSOR_OFF;
		vidcmd[4] = VID_END_NOFLUSH;
		vidput(vidcmd);
		dbgflags |= DBGFLAGS_KEYIN;
	}
	else {
		if (cursflg) vidcmd[0] = VID_CURSOR_NORM;
		else vidcmd[0] = VID_CURSOR_OFF;
		vidcmd[1] = VID_END_NOFLUSH;
		vidput(vidcmd);
	}
	if (!(dbgflags & DBGFLAGS_TRAP)) {
		vidtrapstart(NULL, 0);
		dbgflags |= DBGFLAGS_TRAP;
	}

	evtclear(events[1]);
	if ( (i1 = vidkeyinstart(work, 1, 1, 1, FALSE, events[1])) ) {
/*** ERROR CODE ***/
	}
	if (evtwait(events, 2) == 0) {
		return RC_ERROR;
	}
	vidkeyinend(work, &i1, &i2);
	return(i2);
}
