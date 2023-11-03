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
#define INC_WINDOWS
#include "includes.h"
#include "base.h"
#include "fsrun.h"
#include "fssql.h"
#include "fssqlx.h"
#include "fio.h"

typedef struct expsrc_struct {
	INT setfuncref;
	INT numsetfunc;
	SETFUNC **setfuncarray;
	INT groupref;
	INT numgroup;
	UINT **grouparray;
} EXPSRC;

extern int fsflags;
/* following code is for debug purposes only */
#define FS_DEBUGCODE 1
extern void debugcode(char *str, int pcount, PCODE *pgm);

#define READ_INDEX_NONE			0
#define READ_INDEX_AIMDEX		1
#define READ_INDEX_RANGE		2
#define READ_INDEX_EXACT_DUP	3
#define READ_INDEX_EXACT		4
#define READ_INDEX_ALLEQUAL		0x08  /* this is a bit flag higher than other values */

#define EXPENTRY_TESTED	0x01

#define KEYTYPE_EQ		0x01
#define KEYTYPE_GE		0x02
#define KEYTYPE_GT		0x04
#define KEYTYPE_LE		0x08
#define KEYTYPE_LT		0x10
#define KEYTYPE_LIKE	0x20
#define KEYTYPE_INVALID	0x40

#define ORDER_TRYINDEX	0x01
#define ORDER_OTHER		0x02
#define ORDER_ORDERBY	0x04

#define LBL_FUNC_INIT		(LOWESTLABEL + 0)
#define LBL_FUNC_GETNEXT	(LOWESTLABEL + 1)
#define LBL_FUNC_GETPREV	(LOWESTLABEL + 2)
#define LBL_FUNC_GETFIRST	(LOWESTLABEL + 3)
#define LBL_FUNC_GETLAST	(LOWESTLABEL + 4)
#define LBL_FUNC_GETABS		(LOWESTLABEL + 5)
#define LBL_FUNC_GETREL		(LOWESTLABEL + 6)
#define LBL_FUNC_GETROWCNT	(LOWESTLABEL + 7)
#define LBL_FUNC_UPDATE		(LOWESTLABEL + 8)
#define LBL_FUNC_DELETE		(LOWESTLABEL + 9)
#define LBL_WORK1			(LOWESTLABEL + 20)
#define LBL_WORK2			(LOWESTLABEL + 21)
#define LBL_WORK3			(LOWESTLABEL + 22)
#define LBL_WORK4			(LOWESTLABEL + 23)
#define LBL_RETURN5			(LOWESTLABEL + 30)
#define LBL_RETURN7			(LOWESTLABEL + 31)
#define LBL_MOVEUPDATE		(LOWESTLABEL + 100)
#define LBL_MOVECOLUMNS		(LOWESTLABEL + 101)
#define LBL_FILLWORKSET		(LOWESTLABEL + 200)
#define LBL_READFIRST		(LOWESTLABEL + 1000)
#define LBL_READFINISH		(LOWESTLABEL + 1001)
#define LBL_READNEXT		(LBL_READFINISH)
#define LBL_READNEXTLOJ1	(LOWESTLABEL + 2000)
#define LBL_READNEXTLOJ2	(LOWESTLABEL + 3000)
#define LBL_READNEXTEXP		(LOWESTLABEL + 4000)

#define VAR_1		1
#define VAR_2		2
#define VAR_3		3
#define VAR_RECCNT	4
#define VAR_RECNUM	5
#define VAR_FUNC	6
#define VAR_FORWARD	7
#define VAR_WORKSET	8
#define VAR_LASTABS	9
#define VAR_UPDATE	10
#define VAR_NONDIST	11
#define VAR_DISTNCT	12
#define VAR_MAX		12

#define NEED_MOVECOLS_PLUS1	0x01
#define NEED_MOVECOLS_PLUS2	0x02
#define NEED_MOVECOLS_PLUS4	0x04
#define NEED_MOVECOLS_PLUS5	0x08

#define TABCOL_HIGHEST	0x01
#define TABCOL_LOWER		0x02
#define TABCOL_UPPER		0x04

#define EXPOPERATOR(a)          (a == OPLOWER || a == OPUPPER || a == OPCAST ||\
                                 a == OPCONCAT || a == OPSUBSTRPOS || a == OPSUBSTRLEN ||\
								 a == OPTRIML || a== OPTRIMT || a == OPTRIMB)

extern CONNECTION connection;
//extern OFFSET filepos;

static PCODE **hpgm;			/* handle of the program */
static INT pgmcount;			/* index in pgm */
static INT pgmalloc;			/* size of allocated pgm */
static UCHAR **literals;		/* literals */
static INT litallocsize;		/* bytes allocated in literals in curren plan */
static INT litsize;				/* bytes used in literals */
static S_WHERE **swhere;		/* handle of the S_WHERE structure used in genlexpcode */
static HPLAN parseshplan;		/* hplan for and term parsing */
static S_WHERE **parseswhere;	/* for and term parsing */
static INT parseswherepos;		/* position for and term parsing */
static UINT parsestabcolref;	/* table and column reference for term parsing */
static INT parsesreadflag;	/* flag if index file is an aimdex */
static INT parsescondition;		/* term parse condition */
static INT execerror;			/* flag to tell if an error occurred in build???plan() */

/* the following is used by addpgm, etc. for goto label fixup */
#define MAXLABELDEFS 100
#define MAXLABELREFS 200
static int labeldefname[MAXLABELDEFS];
static int labeldefvalue[MAXLABELDEFS];
static int labelrefname[MAXLABELREFS];
static int labelrefvalue[MAXLABELREFS];
#define LOWESTLABEL 100000		/* everything >= this value is an unresolved label name */
static int labeldefcount;		/* number of labeldefs */
static int labelrefcount;		/* number of labelrefs */

static int fixupandreordertables(S_SELECT **);
static INT checkorcondition(S_WHERE ***, HPLAN);
static void bestindex(S_WHERE **, INT, INT, INT *, INT *, USHORT *, INT *, INT *, INT, UINT **);
static INT buildindexkey(HPLAN , S_WHERE **, INT, INT, INT, USHORT *, INT, INT);
static void checkindexrange(HPLAN, S_WHERE **, INT, INT, INT, USHORT *, INT, INT, INT);
static void startsimpleandtermparse(HPLAN, S_WHERE **, UINT, INT, INT);
static INT getnextsimpleandterm(INT *, INT *, INT *, INT *);
static void genlexpcode(INT, HPLAN, EXPSRC *, INT);
static void genaexpcode(S_AEXP **, HPLAN, EXPSRC *, UINT);
static void aexptype(S_AEXP **, HPLAN, INT *, INT *, INT *);
static INT getexptabcolnum(S_AEXP **, INT *, INT);
static void addpgm0(int);
static void addpgm1(int, int);
static void addpgm2(int, int, int);
static void addpgm3(int, int, int, int);
static void defpgmlabel(int);
static void undefpgmlabel(int);
static INT makelitfromname(INT name);
static INT makelitfromsvaldata(S_VALUE **);
static INT makelitfromsvalescape(S_VALUE **);
static INT makevarfromtype(INT type, INT len, INT scale);

/* build the plan program from the statement */
INT buildselectplan(S_SELECT **stmt, HPLAN *phplan, INT resultonlyflag)
{
	INT i1, i2, i3, i4, numtables, tablenum, columnnum, indexnum;
	INT dynamicflag, dynamickeyflag, explabel, explevel, looplevel, orderflag, worksetref;
	INT length, scale, tableref;
	INT endoffilelabel, keyflag, orconditionflag, plancolrefsize, varcount;
	INT lojflag, lojlevel, lojvar, op, prevreadnextlabel, readnextlabel, readtype, type;
	INT needmovecols, tracecnt, **orderworksetref, **plancolrefinfo;
	USHORT trace[MAX_IDXKEYS];
	UINT tabcolnum, *tabcolnumptr, **orderarray;
	HPLAN hplan;
	TABLE *tab1;
	COLUMN *col1;
	COLREF *crf1, *crf2;
	HCOLREF plancolrefarray;
	S_GENERIC **sgeneric;
	S_VALUE **svalue;
	S_AEXP **saexp;
	S_WHERE **onclause;
	EXPSRC expsrc;
	struct {  /* result, set functions, group, sort */
		INT planstart;
		INT planresult;
		INT plansize;
		INT rowcount;
		INT workset;
		INT tabcolref;
		INT refoffset;
	} worksetinfo[4];

	/* allocate plan, code, literals and colrefarray */
	execerror = FALSE;
	hplan = (HPLAN) memalloc(sizeof(PLAN), MEMFLAGS_ZEROFILL);
	if (hplan == NULL) return sqlerrnum(SQLERR_NOMEM);

	/* all errors directed to buildselectplanerror */
	hpgm = NULL;
	literals = NULL;
	plancolrefarray = NULL;
	plancolrefinfo = NULL;
	orderworksetref = NULL;
	if (!resultonlyflag) {
		hpgm = (PCODE **) memalloc(100 * sizeof(PCODE), 0);
		literals = memalloc(300, 0);
		if (hpgm == NULL || literals == NULL) {
			sqlerrnum(SQLERR_NOMEM);
			goto buildselectplanerror;
		}
		pgmalloc = 100;
		litallocsize = 300;
		pgmcount = litsize = labeldefcount = labelrefcount = 0;
	}

	/* move tables to plan and fill in index info */
	(*hplan)->numtables = numtables = (*stmt)->numtables;
	for (i1 = 0; i1 < numtables; i1++) {
		tablenum = (*stmt)->tablenums[i1];
		(*hplan)->tablenums[i1] = tablenum;
		if (!resultonlyflag) {
			tab1 = tableptr(tablenum);
			if (!(tab1->flags & TABLE_FLAGS_COMPLETE) && maketablecomplete(tablenum)) goto buildselectplanerror;
		}
	}
	if (numtables > 1 || (*stmt)->numsetfunctions || (*stmt)->numgroupcolumns) (*hplan)->updatable = 'R';
	else (*hplan)->updatable = 'W';

	/* information about result set */
	worksetinfo[0].planresult = (*stmt)->numcolumns;
	worksetinfo[0].plansize = (*stmt)->numcolumns + (*stmt)->numordercolumns;
	worksetinfo[0].rowcount = 0;
	for (i1 = 0; (size_t) ++i1 < sizeof(worksetinfo) / sizeof(*worksetinfo); ) {
		worksetinfo[i1].plansize = 0;
		worksetinfo[i1].rowcount = 1;
	}
/* NOTE: technically, this should only be done if updateflag, but Visual Basic */
/*       has taken liberties with ODBC to determine if an update can be performed */
	if ((*stmt)->numsetfunctions || (*stmt)->numgroupcolumns) {
		worksetinfo[0].rowcount = 1;
		worksetinfo[3].planresult = 0;
		worksetinfo[3].rowcount = 0;
		if ((*stmt)->numsetfunctions) {
			/* information about set functions */
			worksetinfo[1].planresult = 0;
			worksetinfo[1].plansize = (*stmt)->numsetfunctions;
			if ((*stmt)->setfuncdistinctcolumn) {
				worksetinfo[1].plansize++;
				/* information about sort workset */
				worksetinfo[3].planresult++;
				worksetinfo[3].plansize++;
			}
		}
		if ((*stmt)->numgroupcolumns) {
			worksetinfo[0].rowcount = 0;
			/* information about group by */
			worksetinfo[2].planresult = 0;
			worksetinfo[2].plansize = (*stmt)->numgroupcolumns;
			/* information about sort workset */
			worksetinfo[3].planresult += (*stmt)->numgroupcolumns;
			worksetinfo[3].plansize += (*stmt)->numgroupcolumns + (*stmt)->numsetfunctions;
		}
	}
	else if ((*stmt)->forupdateflag) worksetinfo[0].plansize++;  /* save room for the file position */

	/* calculate size of worksets and finish workset info */
	worksetref = numtables + 1;
	plancolrefsize = 0;
	for (i1 = 0; (size_t) i1 < sizeof(worksetinfo) / sizeof(*worksetinfo); i1++) {
		if (!worksetinfo[i1].plansize) continue;
		worksetinfo[i1].planstart = plancolrefsize;
		plancolrefsize += worksetinfo[i1].plansize;
		worksetinfo[i1].workset = worksetref;
		worksetinfo[i1].tabcolref = gettabcolref(worksetref, 1);
		worksetinfo[i1].refoffset = 0;
		worksetref++;
		(*hplan)->numworksets++;
		if (resultonlyflag) break;
	}

	/* expressions need help to know where the source is coming from */
	memset(&expsrc, 0, sizeof(expsrc));
	if ((*stmt)->numsetfunctions) {
		expsrc.numsetfunc = (*stmt)->numsetfunctions;
		expsrc.setfuncref = worksetinfo[1].workset;
		expsrc.setfuncarray = (*stmt)->setfunctionarray;
	}
	if ((*stmt)->numgroupcolumns) {
		expsrc.numgroup = (*stmt)->numgroupcolumns;
		expsrc.groupref = worksetinfo[2].workset;
		expsrc.grouparray = (*stmt)->grouptabcolnumarray;
	}

	/* allocate the required memory */
	plancolrefarray = (HCOLREF) memalloc(plancolrefsize * sizeof(COLREF), MEMFLAGS_ZEROFILL);
	if (plancolrefarray == NULL) {
		sqlerrnum(SQLERR_NOMEM);
		goto buildselectplanerror;
	}
	(*hplan)->colrefarray = plancolrefarray;
	plancolrefinfo = (INT **) memalloc((*hplan)->numworksets * 3 * sizeof(INT), 0);
	if (plancolrefinfo == NULL) {
		sqlerrnum(SQLERR_NOMEM);
		goto buildselectplanerror;
	}
	(*hplan)->worksetcolrefinfo = plancolrefinfo;

	if ((*stmt)->nowaitflag) (*hplan)->flags |= PLAN_FLAG_LOCKNOWAIT;
			
	/* build the result workset columns and store in worksetcolrefinfo */
	if (!resultonlyflag) {
		/* write out the branch on call type goto statements */
		addpgm2(OP_MOVE, VAR_1, VAR_FUNC);
		if ((*stmt)->forupdateflag) {
			(*hplan)->flags |= PLAN_FLAG_FORUPDATE;
			i2 = LBL_FUNC_DELETE;
		}
		else i2 = LBL_FUNC_GETROWCNT;  /* no update/delete positioned unless for update specified */
		for (i1 = LBL_FUNC_INIT; i1 <= i2; i1++) {
			addpgm2(OP_GOTOIFZERO, i1, VAR_1);		/* if var1 == 0, goto the respective function */
			addpgm2(OP_INCR, VAR_1, -1);			/* sub 1 from var1 */
		}
		addpgm0(OP_TERMINATE);						/* should not occur */
/* LBL_MOVECOLUMNS */
		defpgmlabel(LBL_MOVECOLUMNS);
	}

	/* get column reference information for result workset and distinct workset */
	for (i2 = 0; i2 < worksetinfo[0].planresult; i2++) {
		crf1 = *plancolrefarray + worksetinfo[0].planstart + i2;
		tabcolnum = (*(*stmt)->tabcolnumarray)[i2];
		columnnum = getcolnum(tabcolnum);
		if (gettabref(tabcolnum) == TABREF_LITEXP) {  /* it's a special column */
			sgeneric = *(*(*stmt)->specialcolumns + columnnum);
			if ((*sgeneric)->stype == STYPE_VALUE) {
				svalue = (S_VALUE **) sgeneric;
				crf1->tabcolnum = tabcolnum;
				/* crf1->tablenum = 0; */
				/* crf1->columnnum = 0; */
				crf1->type = TYPE_CHAR;
				crf1->length = (*svalue)->length;
				/* crf1->scale = 0; */
				crf1->offset = worksetinfo[0].refoffset;
				worksetinfo[0].refoffset += crf1->length;
				if (!resultonlyflag) addpgm2(OP_COLMOVE, makelitfromsvaldata(svalue), worksetinfo[0].tabcolref++);
			}
			else if ((*sgeneric)->stype == STYPE_AEXP) {
				saexp = (S_AEXP **) sgeneric;
				aexptype(saexp, hplan, &type, &length, &scale);
				crf1->tabcolnum = tabcolnum;
				/* crf1->tablenum = 0; */
				/* crf1->columnnum = 0; */
				crf1->type = type;
				crf1->length = length;
				crf1->scale = scale;
				crf1->offset = worksetinfo[0].refoffset;
				worksetinfo[0].refoffset += length;
				if (!resultonlyflag) genaexpcode(saexp, hplan, &expsrc, worksetinfo[0].tabcolref++);
			}
		}
		else {  /* it's a regular column */
			tablenum = (*hplan)->tablenums[gettabref(tabcolnum) - 1];
			tab1 = tableptr(tablenum);
			col1 = columnptr(tab1, columnnum);
			crf1->tabcolnum = tabcolnum;
			crf1->tablenum = (USHORT) tablenum;  /* used for result set info */
			crf1->columnnum = (USHORT) columnnum;  /* used for result set info */
			crf1->type = col1->type;
			crf1->scale = col1->scale;
			crf1->length = col1->length;
			crf1->offset = worksetinfo[0].refoffset;
			worksetinfo[0].refoffset += crf1->length;
			if (!resultonlyflag) {
				if ((*stmt)->numgroupcolumns) {
					for (i1 = 0; i1 < (*stmt)->numgroupcolumns && tabcolnum != (*(*stmt)->grouptabcolnumarray)[i1]; i1++);
					if (i1 == (*stmt)->numgroupcolumns) {
						sqlerrnummsg(SQLERR_INTERNAL, "result column not of group by list", NULL);
						goto buildselectplanerror;
					}
					addpgm2(OP_COLMOVE, gettabcolref(worksetinfo[2].workset, i1 + 1), worksetinfo[0].tabcolref);
				}
				else addpgm2(OP_COLMOVE, tabcolnum, worksetinfo[0].tabcolref);
				worksetinfo[0].tabcolref++;
			}
		}
	}
	if (resultonlyflag) {
		(*hplan)->numworksets = 1;
		(*plancolrefinfo)[0] = worksetinfo[0].planstart;
		(*plancolrefinfo)[1] = worksetinfo[0].planresult;
		(*plancolrefinfo)[2] = worksetinfo[0].plansize;
		/* transfer the correlation (alias) table from statement to plan */
		(*hplan)->corrtable = (*stmt)->corrtable;
		(*stmt)->corrtable = NULL;
		*phplan = hplan;
		return 0;
	}
	if ((*stmt)->forupdateflag) {
		/* append 12 byte file position to result workset */
		crf1 = *plancolrefarray + worksetinfo[0].planstart + worksetinfo[0].planresult;
		/* crf1->tabcolnum = 0; */
		/* crf1->tablenum = 0; */
		/* crf1->columnnum = 0; */
		crf1->type = TYPE_POSNUM;
		/* crf1->scale = 0; */
		crf1->length = 12;
		crf1->offset = worksetinfo[0].refoffset;
		worksetinfo[0].refoffset += 12;
		/* move file position to work set */
		addpgm1(OP_FPOSTOCOL, worksetinfo[0].tabcolref++);
	}
	if ((*stmt)->numordercolumns) {
		/* append non-select list order columns to result workset */
		orderworksetref = (INT **) memalloc((*stmt)->numordercolumns * sizeof(INT), 0);
		if (orderworksetref == NULL) {
			sqlerrnum(SQLERR_NOMEM);
			goto buildselectplanerror;
		}
		orderflag = ORDER_ORDERBY | ORDER_TRYINDEX;
		for (i1 = 0, i2 = (*stmt)->numordercolumns; i1 < i2; i1++) {
			if (!(*(*stmt)->orderascflagarray)[i1]) orderflag &= ~ORDER_TRYINDEX;  /* not ascending */
			tabcolnum = (*(*stmt)->ordertabcolnumarray)[i1];
			if (gettabref(tabcolnum) != 1) orderflag &= ~ORDER_TRYINDEX;  /* not first table */
			for (i3 = 0, i4 = (*stmt)->numcolumns; i3 < i4; i3++)
				if (tabcolnum == (*(*stmt)->tabcolnumarray)[i3]) break;
			if (i3 < i4) {
				/* order by column is part of result set */
				if ((*plancolrefarray + i3)->type == TYPE_NUM) orderflag &= ~ORDER_TRYINDEX;  /* numeric type or not first table */
				(*orderworksetref)[i1] = gettabcolref(worksetinfo[0].workset, i3 + 1);
				continue;
			}
			if ((*stmt)->numgroupcolumns) {
				for (i3 = 0, i4 = (*stmt)->numgroupcolumns; i3 < i4; i3++)
					if (tabcolnum == (*(*stmt)->grouptabcolnumarray)[i3]) break;
				if (i3 == i4) {
					sqlerrnummsg(SQLERR_INTERNAL, "order by not detected in result or group by", NULL);
					goto buildselectplanerror;
				}
/*** CODE: IF FSSQL3 SUPPORTS 'ORDER BY' EXPRESSIONS, THEN SEARCH SET FUNCTION ARRAY ***/
			}
			else if (gettabref(tabcolnum) > (*stmt)->numcolumns) {
/*** CODE: FSSQL3 DOES NOT SUPPORT 'ORDER BY' EXPRESSIONS SO THIS SHOULD NOT HAPPEN ***/
				orderflag &= ~ORDER_TRYINDEX;
				sqlerrnummsg(SQLERR_INTERNAL, "order by expression not detected in result set", NULL);
				goto buildselectplanerror;
			}
			tablenum = (*hplan)->tablenums[gettabref(tabcolnum) - 1];
			columnnum = getcolnum(tabcolnum);
			tab1 = tableptr(tablenum);
			col1 = columnptr(tab1, columnnum);
			crf1 = *plancolrefarray + worksetinfo[0].planstart + getcolnum(worksetinfo[0].tabcolref) - 1;
			crf1->tabcolnum = tabcolnum;
			/* crf1->tablenum = (SHORT) tablenum; */
			/* crf1->columnnum = (SHORT) columnnum; */
			crf1->type = col1->type;
			if (col1->type == TYPE_NUM) orderflag &= ~ORDER_TRYINDEX;  /* numeric type */
			crf1->length = col1->length;
			crf1->scale = col1->scale;
			crf1->offset = worksetinfo[0].refoffset;
			worksetinfo[0].refoffset += crf1->length;
			if ((*stmt)->numgroupcolumns) tabcolnum = gettabcolref(worksetinfo[2].workset, i3 + 1);
			addpgm2(OP_COLMOVE, tabcolnum, worksetinfo[0].tabcolref);
			(*orderworksetref)[i1] = worksetinfo[0].tabcolref++;
		}
		/* adjust workset size for any order columns in result set */
		worksetinfo[0].plansize = getcolnum(worksetinfo[0].tabcolref) - 1;
	}
	else orderflag = 0x00;  /* no order by */
	/* column reference information for result workset is now complete */
	addpgm0(OP_RETURN);

	if ((*stmt)->distinctflag) {
		/* test if index can be used */
		orderflag |= ORDER_OTHER | ORDER_TRYINDEX;
		for (i1 = 0, i2 = (*stmt)->numcolumns; i1 < i2; i1++)
			if (gettabref((*(*stmt)->tabcolnumarray)[i1]) != 1) orderflag &= ~ORDER_TRYINDEX;  /* not first table */
		if ((*stmt)->numordercolumns && (*stmt)->numordercolumns <= i2) {
			for (i1 = 0, i2 = (*stmt)->numordercolumns; i1 < i2; i1++)
				if ((*(*stmt)->ordertabcolnumarray)[i1] != (*(*stmt)->tabcolnumarray)[i2]) break;
			if (i1 == i2) orderflag &= ~ORDER_ORDERBY;
		}
	}
	needmovecols = 0;
	if ((*stmt)->numsetfunctions) {
		/* build column reference information for 'set function' workset */
		crf1 = *plancolrefarray + worksetinfo[1].planstart;
		for (i1 = 0, i2 = (*stmt)->numsetfunctions; i1 < i2; crf1++, i1++) {
			op = (*(*stmt)->setfunctionarray)[i1].type;
			tabcolnum = (*(*stmt)->setfunctionarray)[i1].tabcolnum;
			if (op == OPCOUNTALL || op == OPCOUNT || op == OPCOUNTDISTINCT) {
				if (op != OPCOUNTDISTINCT) needmovecols |= NEED_MOVECOLS_PLUS2;
				else needmovecols |= NEED_MOVECOLS_PLUS5;
				type = TYPE_NUM;
				length = 10;
				scale = 0;
			}
			else {
				if (op == OPAVG) needmovecols |= NEED_MOVECOLS_PLUS1 | NEED_MOVECOLS_PLUS2;
				else if (op == OPSUM || op == OPMIN || op == OPMAX) needmovecols |= NEED_MOVECOLS_PLUS1;
				else if (op == OPAVGDISTINCT) needmovecols |= NEED_MOVECOLS_PLUS4 | NEED_MOVECOLS_PLUS5;
				else needmovecols |= NEED_MOVECOLS_PLUS4;
				tablenum = (*hplan)->tablenums[gettabref(tabcolnum) - 1];
				tab1 = tableptr(tablenum);
				col1 = columnptr(tab1, getcolnum(tabcolnum));
				type = col1->type;
				length = col1->length;
				if (op == OPAVG || op == OPAVGDISTINCT || op == OPSUM /* OPSUM added here as a quick fix on 19 AUG 09 */) {
					length <<= 1;
					if (length < 10) length = 10;
					else if (length > 31) {
						if (scale) length = 32;
						else length = 31;
					}
				}
				scale = col1->scale;
			}
			crf1->tabcolnum = tabcolnum;
			/* crf1->tablenum = 0; */
			/* crf1->columnnum = 0; */
			crf1->type = type;
			crf1->length = length;
			crf1->scale = scale;
			crf1->offset = worksetinfo[1].refoffset;
			worksetinfo[1].refoffset += length;
		}
		if ((*stmt)->setfuncdistinctcolumn) {
			/* append distinct column to 'set function' workset */
			tabcolnum = (*stmt)->setfuncdistinctcolumn;
			/* test if index can be used */
			orderflag |= ORDER_OTHER | ORDER_TRYINDEX;
			if (gettabref(tabcolnum) != 1) orderflag &= ~ORDER_TRYINDEX;  /* not first table */
			tablenum = (*hplan)->tablenums[gettabref(tabcolnum) - 1];
			columnnum = getcolnum(tabcolnum);
			tab1 = tableptr(tablenum);
			col1 = columnptr(tab1, columnnum);
			crf1->tabcolnum = tabcolnum;
			/* crf1->tablenum = (SHORT) tablenum; */
			/* crf1->columnnum = (SHORT) columnnum; */
			crf1->type = col1->type;
			crf1->scale = col1->scale;
			crf1->length = col1->length;
			crf1->offset = worksetinfo[1].refoffset;
			worksetinfo[1].refoffset += crf1->length;
		}
		/* column reference information for set function workset is now complete */
	}
	if ((*stmt)->numgroupcolumns || (*stmt)->setfuncdistinctcolumn) {
		/* do non-indexed distinct set functions/group by moves that occur for each record */
/* LBL_MOVECOLUMNS + 6 */
		defpgmlabel(LBL_MOVECOLUMNS + 6);
		/* build column reference information for unique/sort workset */
		crf2 = *plancolrefarray + worksetinfo[3].planstart;
		i1 = 0;
		if ((*stmt)->numgroupcolumns) {
			orderflag |= ORDER_OTHER | ORDER_TRYINDEX;
			/* build column reference information for 'group by' workset */
			crf1 = *plancolrefarray + worksetinfo[2].planstart;
			for (i2 = (*stmt)->numgroupcolumns; i1 < i2; crf1++, crf2++, i1++) {
				tabcolnum = (*(*stmt)->grouptabcolnumarray)[i1];
				if (gettabref(tabcolnum) != 1) orderflag &= ~ORDER_TRYINDEX;  /* not first table */
				tablenum = (*hplan)->tablenums[gettabref(tabcolnum) - 1];
				columnnum = getcolnum(tabcolnum);
				tab1 = tableptr(tablenum);
				col1 = columnptr(tab1, columnnum);
				crf1->tabcolnum = tabcolnum;
				/* crf1->tablenum = (SHORT) tablenum; */
				/* crf1->columnnum = (SHORT) columnnum; */
				crf1->length = col1->length;
				crf1->type = col1->type;
#if 0
				if (col1->type == TYPE_NUM) orderflag &= ~ORDER_TRYINDEX;  /* numeric type */
#endif
				crf1->scale = col1->scale;
				crf1->offset = worksetinfo[2].refoffset;
				worksetinfo[2].refoffset += crf1->length;
				/* append 'group by' columns to unique/sort workset */
				*crf2 = *crf1;
				crf2->offset = worksetinfo[3].refoffset;
				worksetinfo[3].refoffset += crf2->length;
				addpgm2(OP_COLMOVE, tabcolnum, worksetinfo[3].tabcolref++);
			}
			/* column reference information for group by workset is now complete */
		}
		if ((*stmt)->setfuncdistinctcolumn) {
			/* append distinct column to unique/sort workset */
			tabcolnum = (*stmt)->setfuncdistinctcolumn;
			tablenum = (*hplan)->tablenums[gettabref(tabcolnum) - 1];
			columnnum = getcolnum(tabcolnum);
			tab1 = tableptr(tablenum);
			col1 = columnptr(tab1, columnnum);
			crf2->tabcolnum = tabcolnum;
			/* crf2->tablenum = (SHORT) tablenum; */
			/* crf2->columnnum = (SHORT) columnnum; */
			crf2->type = col1->type;
			crf2->length = col1->length;
			crf2->scale = col1->scale;
			crf2->offset = worksetinfo[3].refoffset;
			worksetinfo[3].refoffset += crf2->length;
			crf2++;
			addpgm2(OP_COLMOVE, tabcolnum, worksetinfo[3].tabcolref++);
			i1++;
		}
		/* i1 contains current unique/sort count */
		if ((*stmt)->numgroupcolumns && (*stmt)->numsetfunctions) {
			for (i3 = 0, i4 = (*stmt)->numsetfunctions; i3 < i4; i3++) {
				op = (*(*stmt)->setfunctionarray)[i3].type;
				if (op >= OPSUM && op <= OPMAX) {
					tabcolnum = (*(*stmt)->setfunctionarray)[i3].tabcolnum;
					crf1 = *plancolrefarray + worksetinfo[3].planstart;
					for (i2 = 0; i2 < i1; crf1++, i2++)
						if (tabcolnum == crf1->tabcolnum) break;
					if (i2 < i1) continue;
					crf1->tabcolnum = tabcolnum;
					tablenum = (*hplan)->tablenums[gettabref(tabcolnum) - 1];
					columnnum = getcolnum(tabcolnum);
					tab1 = tableptr(tablenum);
					col1 = columnptr(tab1, columnnum);
					crf1->tabcolnum = tabcolnum;
					/* crf1->tablenum = (SHORT) tablenum; */
					/* crf1->columnnum = (SHORT) columnnum; */
					crf1->type = col1->type;
					crf1->length = col1->length;
					crf1->scale = col1->scale;
					crf1->offset = worksetinfo[3].refoffset;
					worksetinfo[3].refoffset += crf1->length;
					addpgm2(OP_COLMOVE, tabcolnum, worksetinfo[3].tabcolref++);
					i1++;
				}
			}
			/* adjust workset size for any set function columns in 'group by'/distinct */
			worksetinfo[3].plansize = i1;
		}
		/* column reference information for unique/sort workset is now complete */
		addpgm0(OP_RETURN);
	}

	for (i1 = i2 = 0; (size_t) i1 < sizeof(worksetinfo) / sizeof(*worksetinfo); i1++) {
		if (!worksetinfo[i1].plansize) continue;
		(*plancolrefinfo)[i2++] = worksetinfo[i1].planstart;
		(*plancolrefinfo)[i2++] = worksetinfo[i1].planresult;
		(*plancolrefinfo)[i2++] = worksetinfo[i1].plansize;
	}

	/* finish additional move columns calls to support different possible situations. */
	/* note: this code produces move labels for both index/non-index cases so */
	/*       it is possible that a move label may never be called.  */
	if ((*stmt)->numsetfunctions) {
		if (needmovecols & NEED_MOVECOLS_PLUS1) {
			/* do set functions that occur for each record */
/* LBL_MOVECOLUMNS + 1 */
			defpgmlabel(LBL_MOVECOLUMNS + 1);
			worksetinfo[1].tabcolref = gettabcolref(worksetinfo[1].workset, 1);
			for (i1 = 0, i2 = (*stmt)->numsetfunctions; i1 < i2; worksetinfo[1].tabcolref++, i1++) {
				op = (*(*stmt)->setfunctionarray)[i1].type;
				if (op >= OPSUM && op <= OPMAX) {
					tabcolnum = (*(*stmt)->setfunctionarray)[i1].tabcolnum;
					addpgm2(OP_GOTOIFNOTZERO, pgmcount + 3, VAR_NONDIST);
					addpgm2(OP_COLMOVE, tabcolnum, worksetinfo[1].tabcolref);
					if (op == OPSUM || op == OPAVG) {
						addpgm1(OP_GOTO, pgmcount + 2);
						addpgm3(OP_COLADD, worksetinfo[1].tabcolref, tabcolnum, worksetinfo[1].tabcolref);
					}
					else {
						addpgm1(OP_GOTO, pgmcount + 4);
						addpgm3(OP_COLCOMPARE, worksetinfo[1].tabcolref, tabcolnum, VAR_1);
						if (op == OPMIN) addpgm2(OP_GOTOIFNOTPOS, pgmcount + 2, VAR_1);
						else addpgm2(OP_GOTOIFNOTNEG, pgmcount + 2, VAR_1);
						addpgm2(OP_COLMOVE, tabcolnum, worksetinfo[1].tabcolref);
					}
				}
			}
			addpgm0(OP_RETURN);
		}
		if (needmovecols & NEED_MOVECOLS_PLUS2) {
			/* do set functions that occur at EOF */
/* LBL_MOVECOLUMNS + 2 */
			defpgmlabel(LBL_MOVECOLUMNS + 2);
			worksetinfo[1].tabcolref = gettabcolref(worksetinfo[1].workset, 1);
			for (i1 = 0, i2 = (*stmt)->numsetfunctions; i1 < i2; worksetinfo[1].tabcolref++, i1++) {
				op = (*(*stmt)->setfunctionarray)[i1].type;
				if (op == OPCOUNTALL || op == OPCOUNT)
					addpgm2(OP_MOVETOCOL, VAR_NONDIST, worksetinfo[1].tabcolref);
				else if (op == OPAVG) {
					addpgm2(OP_GOTOIFZERO, pgmcount + 3, VAR_NONDIST);
					addpgm2(OP_MOVETOCOL, VAR_NONDIST, gettabcolref(TABREF_WORKVAR, 1));
					addpgm3(OP_COLDIV, worksetinfo[1].tabcolref, gettabcolref(TABREF_WORKVAR, 1), worksetinfo[1].tabcolref);
				}
			}
			addpgm0(OP_RETURN);
		}

		if ((*stmt)->setfuncdistinctcolumn) {
			/* do indexed distinct set functions that occur for each record */
/* LBL_MOVECOLUMNS + 3 */
			defpgmlabel(LBL_MOVECOLUMNS + 3);
			tabcolnum = gettabcolref(worksetinfo[1].workset, (*stmt)->numsetfunctions + 1);
			addpgm2(OP_GOTOIFZERO, pgmcount + 3, VAR_DISTNCT);
			addpgm3(OP_COLCOMPARE, tabcolnum, (*stmt)->setfuncdistinctcolumn, VAR_1);
			addpgm2(OP_GOTOIFZERO, LBL_WORK1, VAR_1);
			addpgm2(OP_COLMOVE, (*stmt)->setfuncdistinctcolumn, tabcolnum);
			if (needmovecols & NEED_MOVECOLS_PLUS4) {
				worksetinfo[1].tabcolref = gettabcolref(worksetinfo[1].workset, 1);
				for (i1 = 0, i2 = (*stmt)->numsetfunctions; i1 < i2; worksetinfo[1].tabcolref++, i1++) {
					op = (*(*stmt)->setfunctionarray)[i1].type;
					if (op >= OPSUMDISTINCT && op <= OPMAXDISTINCT) {
						addpgm2(OP_GOTOIFNOTZERO, pgmcount + 3, VAR_DISTNCT);
						addpgm2(OP_COLMOVE, tabcolnum, worksetinfo[1].tabcolref);
						if (op == OPSUMDISTINCT || op == OPAVGDISTINCT) {
							addpgm1(OP_GOTO, pgmcount + 2);
							addpgm3(OP_COLADD, worksetinfo[1].tabcolref, tabcolnum, worksetinfo[1].tabcolref);
						}
						else {
							addpgm1(OP_GOTO, pgmcount + 4);
							addpgm3(OP_COLCOMPARE, worksetinfo[1].tabcolref, tabcolnum, VAR_1);
							if (op == OPMINDISTINCT) addpgm2(OP_GOTOIFNOTPOS, pgmcount + 2, VAR_1);
							else addpgm2(OP_GOTOIFNOTNEG, pgmcount + 2, VAR_1);
							addpgm2(OP_COLMOVE, tabcolnum, worksetinfo[1].tabcolref);
						}
					}
				}
			}
			addpgm2(OP_INCR, VAR_DISTNCT, 1);
/* LBL_WORK1 */
			defpgmlabel(LBL_WORK1);
			undefpgmlabel(LBL_WORK1);
			addpgm0(OP_RETURN);

			if (!(*stmt)->numgroupcolumns) {
				/* do non-indexed distinct set functions that occur for each unique/sort row */
/* LBL_MOVECOLUMNS + 4 */
				defpgmlabel(LBL_MOVECOLUMNS + 4);
				if (needmovecols & NEED_MOVECOLS_PLUS4) {
					tabcolnum = gettabcolref(worksetinfo[3].workset, 1);
					worksetinfo[1].tabcolref = gettabcolref(worksetinfo[1].workset, 1);
					for (i1 = 0, i2 = (*stmt)->numsetfunctions; i1 < i2; worksetinfo[1].tabcolref++, i1++) {
						op = (*(*stmt)->setfunctionarray)[i1].type;
						if (op >= OPSUMDISTINCT && op <= OPMAXDISTINCT) {
							addpgm2(OP_GOTOIFNOTZERO, pgmcount + 3, VAR_DISTNCT);
							addpgm2(OP_COLMOVE, tabcolnum, worksetinfo[1].tabcolref);
							if (op == OPSUMDISTINCT || op == OPAVGDISTINCT) {
								addpgm1(OP_GOTO, pgmcount + 2);
								addpgm3(OP_COLADD, worksetinfo[1].tabcolref, tabcolnum, worksetinfo[1].tabcolref);
							}
							else {
								addpgm1(OP_GOTO, pgmcount + 4);
								addpgm3(OP_COLCOMPARE, worksetinfo[1].tabcolref, tabcolnum, VAR_1);
								if (op == OPMINDISTINCT) addpgm2(OP_GOTOIFNOTPOS, pgmcount + 2, VAR_1);
								else addpgm2(OP_GOTOIFNOTNEG, pgmcount + 2, VAR_1);
								addpgm2(OP_COLMOVE, tabcolnum, worksetinfo[1].tabcolref);
							}
						}
					}
				}
				addpgm2(OP_INCR, VAR_DISTNCT, 1);
				addpgm0(OP_RETURN);
			}

			if (needmovecols & NEED_MOVECOLS_PLUS5) {
				/* do distinct set functions that occur at end of file or unique/sort workset */
/* LBL_MOVECOLUMNS + 5 */
				defpgmlabel(LBL_MOVECOLUMNS + 5);
				worksetinfo[1].tabcolref = gettabcolref(worksetinfo[1].workset, 1);
				for (i1 = 0, i2 = (*stmt)->numsetfunctions; i1 < i2; worksetinfo[1].tabcolref++, i1++) {
					op = (*(*stmt)->setfunctionarray)[i1].type;
					if (op == OPCOUNTDISTINCT)
						addpgm2(OP_MOVETOCOL, VAR_DISTNCT, worksetinfo[1].tabcolref);
					else if (op == OPAVGDISTINCT) {
						addpgm2(OP_GOTOIFZERO, pgmcount + 3, VAR_DISTNCT);
						addpgm2(OP_MOVETOCOL, VAR_DISTNCT, gettabcolref(TABREF_WORKVAR, 1));
						addpgm3(OP_COLDIV, worksetinfo[1].tabcolref, gettabcolref(TABREF_WORKVAR, 1), worksetinfo[1].tabcolref);
					}
				}
				addpgm0(OP_RETURN);
			}
		}

		if ((*stmt)->numgroupcolumns) {
			/* do non-indexed set functions that occur for each row of a group */
/* LBL_MOVECOLUMNS + 4 */
			defpgmlabel(LBL_MOVECOLUMNS + 4);
			if (needmovecols & NEED_MOVECOLS_PLUS1) {
				worksetinfo[1].tabcolref = gettabcolref(worksetinfo[1].workset, 1);
				for (i1 = 0, i2 = (*stmt)->numsetfunctions; i1 < i2; worksetinfo[1].tabcolref++, i1++) {
					op = (*(*stmt)->setfunctionarray)[i1].type;
					if (op >= OPSUM && op <= OPMAX) {
						tabcolnum = (*(*stmt)->setfunctionarray)[i1].tabcolnum;
						crf1 = *plancolrefarray + worksetinfo[3].planstart;
						for (i3 = 0, i4 = worksetinfo[3].plansize; i3 < i4; crf1++, i3++)
							if (tabcolnum == crf1->tabcolnum) break;
						if (i3 == i4) {
							sqlerrnummsg(SQLERR_INTERNAL, "set function column not found in group by columns", NULL);
							goto buildselectplanerror;
						}
						tabcolnum = gettabcolref(worksetinfo[3].workset, i3 + 1);
						addpgm2(OP_GOTOIFNOTZERO, pgmcount + 3, VAR_NONDIST);
						addpgm2(OP_COLMOVE, tabcolnum, worksetinfo[1].tabcolref);
						if (op == OPSUM || op == OPAVG) {
							addpgm1(OP_GOTO, pgmcount + 2);
							addpgm3(OP_COLADD, worksetinfo[1].tabcolref, tabcolnum, worksetinfo[1].tabcolref);
						}
						else {
							addpgm1(OP_GOTO, pgmcount + 4);
							addpgm3(OP_COLCOMPARE, worksetinfo[1].tabcolref, tabcolnum, VAR_1);
							if (op == OPMIN) addpgm2(OP_GOTOIFNOTPOS, pgmcount + 2, VAR_1);
							else addpgm2(OP_GOTOIFNOTNEG, pgmcount + 2, VAR_1);
							addpgm2(OP_COLMOVE, tabcolnum, worksetinfo[1].tabcolref);
						}
					}
				}
			}
			if ((*stmt)->setfuncdistinctcolumn) {
				tabcolnum = gettabcolref(worksetinfo[1].workset, (*stmt)->numsetfunctions + 1);
				addpgm2(OP_GOTOIFZERO, pgmcount + 3, VAR_DISTNCT);
				addpgm3(OP_COLCOMPARE, tabcolnum, gettabcolref(worksetinfo[3].workset, (*stmt)->numgroupcolumns + 1), VAR_1);
				addpgm2(OP_GOTOIFZERO, LBL_WORK1, VAR_1);
				addpgm2(OP_COLMOVE, gettabcolref(worksetinfo[3].workset, (*stmt)->numgroupcolumns + 1), tabcolnum);
				if (needmovecols & NEED_MOVECOLS_PLUS4) {
					worksetinfo[1].tabcolref = gettabcolref(worksetinfo[1].workset, 1);
					for (i1 = 0, i2 = (*stmt)->numsetfunctions; i1 < i2; worksetinfo[1].tabcolref++, i1++) {
						op = (*(*stmt)->setfunctionarray)[i1].type;
						if (op >= OPSUMDISTINCT && op <= OPMAXDISTINCT) {
							addpgm2(OP_GOTOIFNOTZERO, pgmcount + 3, VAR_DISTNCT);
							addpgm2(OP_COLMOVE, tabcolnum, worksetinfo[1].tabcolref);
							if (op == OPSUMDISTINCT || op == OPAVGDISTINCT) {
								addpgm1(OP_GOTO, pgmcount + 2);
								addpgm3(OP_COLADD, worksetinfo[1].tabcolref, tabcolnum, worksetinfo[1].tabcolref);
							}
							else {
								addpgm1(OP_GOTO, pgmcount + 4);
								addpgm3(OP_COLCOMPARE, worksetinfo[1].tabcolref, tabcolnum, VAR_1);
								if (op == OPMINDISTINCT) addpgm2(OP_GOTOIFNOTPOS, pgmcount + 2, VAR_1);
								else addpgm2(OP_GOTOIFNOTNEG, pgmcount + 2, VAR_1);
								addpgm2(OP_COLMOVE, tabcolnum, worksetinfo[1].tabcolref);
							}
						}
					}
				}
				addpgm2(OP_INCR, VAR_DISTNCT, 1);
/* LBL_WORK1 */
				defpgmlabel(LBL_WORK1);
				undefpgmlabel(LBL_WORK1);
			}
			addpgm0(OP_RETURN);
		}
	}

	if ((*stmt)->numgroupcolumns) {
		/* do indexed 'group by' moves that occur for each group */
/* LBL_MOVECOLUMNS + 7 */
		defpgmlabel(LBL_MOVECOLUMNS + 7);
		worksetinfo[2].tabcolref = gettabcolref(worksetinfo[2].workset, 1);
		for (i1 = 0, i2 = (*stmt)->numgroupcolumns; i1 < i2; i1++)
			addpgm2(OP_COLMOVE, (*(*stmt)->grouptabcolnumarray)[i1], worksetinfo[2].tabcolref++);
		addpgm0(OP_RETURN);

		/* do non-indexed 'group by' moves that occur for each group */
/* LBL_MOVECOLUMNS + 8 */
		defpgmlabel(LBL_MOVECOLUMNS + 8);
		worksetinfo[2].tabcolref = gettabcolref(worksetinfo[2].workset, 1);
		worksetinfo[3].tabcolref = gettabcolref(worksetinfo[3].workset, 1);
		for (i1 = 0, i2 = (*stmt)->numgroupcolumns; i1 < i2; i1++)
			addpgm2(OP_COLMOVE, worksetinfo[3].tabcolref++, worksetinfo[2].tabcolref++);
		addpgm0(OP_RETURN);
	}

	/* convert old style joins to new style by building ON clauses */
	/* and reorder the tables for a better plan */
	if (fixupandreordertables(stmt)) goto buildselectplanerror;

	if ((*stmt)->joinonconditions[0] == NULL && ((*stmt)->where != NULL || (*stmt)->having != NULL)) {
/* LBL_FILLWORKSET */
		defpgmlabel(LBL_FILLWORKSET);
		onclause = (*stmt)->where;
		orconditionflag = checkorcondition(&onclause, hplan);
		if (orconditionflag) {
			if (orconditionflag < 0) goto buildselectplanerror;
			addpgm0(OP_RETURN);
			/* move modified where condition to 1st table join condition */
			(*stmt)->joinonconditions[0] = onclause;
			freestmt((S_GENERIC **)(*stmt)->where);
			(*stmt)->where = NULL;
		}
		else {
			onclause = (*stmt)->having;
			orconditionflag = checkorcondition(&onclause, hplan);
			if (orconditionflag) {
				if (orconditionflag < 0) goto buildselectplanerror;
				addpgm0(OP_RETURN);
				/* move modified having condition to 1st table join condition */
				(*stmt)->joinonconditions[0] = onclause;
				freestmt((S_GENERIC **)(*stmt)->having);
				(*stmt)->having = NULL;
			}
		}
	}
	else orconditionflag = 0;

	/* var1 is input function, work variable, and return status */
	/* var2 is row number on input and update/delete count on output */
	readnextlabel = LBL_READFINISH;
	looplevel = 0;
	dynamicflag = (!(*stmt)->numsetfunctions && !(*stmt)->numgroupcolumns);
/* LBL_READFIRST */
	defpgmlabel(LBL_READFIRST);
	addpgm2(OP_SET, VAR_FORWARD, 1);
#if 0
/*** NOTE: fssql5.c initializes all variables to zero ***/
	if (!(*stmt)->numsetfunctions || (*stmt)->numgroupcolumns) addpgm2(OP_SET, VAR_RECNUM, 0);
	else {
		if (needmovecols & (NEED_MOVECOLS_PLUS1 | NEED_MOVECOLS_PLUS2)) addpgm2(OP_SET, VAR_NONDIST, 0);
		if (needmovecols & (NEED_MOVECOLS_PLUS4 | NEED_MOVECOLS_PLUS5)) addpgm2(OP_SET, VAR_DISTNCT, 0);
	}
#endif
	varcount = VAR_MAX;
	if (orconditionflag) {
		orderflag &= ~ORDER_TRYINDEX;  /* do not try to sort through index */
		if (orderflag /* || orconditionflag == 2 */) dynamicflag = FALSE;
		addpgm2(OP_SET, ++varcount, 0);
		endoffilelabel = LBL_READNEXT + looplevel;
		readnextlabel = LBL_READNEXT + ++looplevel;
		defpgmlabel(readnextlabel);
		if (dynamicflag) addpgm2(OP_GOTOIFNEG, pgmcount + 6, VAR_FORWARD);
		addpgm2(OP_WORKGETROWCOUNT, (*hplan)->numtables + (*hplan)->numworksets, VAR_1);
		/* check for EOF */
		addpgm3(OP_SUB, VAR_1, varcount, VAR_1);
		addpgm2(OP_GOTOIFNOTPOS, endoffilelabel, VAR_1);
		addpgm2(OP_INCR, varcount, 1);
		if (dynamicflag) {
			addpgm1(OP_GOTO, pgmcount + 5);
			/* check for BOF */
			addpgm2(OP_SET, VAR_1, 1);
			addpgm3(OP_SUB, VAR_1, varcount, VAR_1);
			addpgm2(OP_GOTOIFNOTNEG, endoffilelabel, VAR_1);
			addpgm2(OP_INCR, varcount, -1);
		}
		addpgm2(OP_WORKSETROWID, (*hplan)->numtables + (*hplan)->numworksets, varcount);
	}

	/* loop through each table */
	explevel = lojlevel = 0;
	for (tableref = 1; tableref <= numtables; tableref++) {
		onclause = (*stmt)->joinonconditions[tableref - 1];
		tablenum = (*hplan)->tablenums[tableref - 1];
		if (!(orderflag & ORDER_TRYINDEX)) {
			i1 = 0;
			orderarray = NULL;
		}
		else if ((*stmt)->numgroupcolumns) {
			i1 = (*stmt)->numgroupcolumns;
			orderarray = (*stmt)->grouptabcolnumarray;
			if ((*stmt)->setfuncdistinctcolumn) {
				for (i2 = 0; i2 < i1 && (*stmt)->setfuncdistinctcolumn != (*orderarray)[i2]; i2++);
				if (i2 == i1) i1++;  /* fssql3.c presets last + 1 to distinct column */
			}
		}
		else if ((*stmt)->setfuncdistinctcolumn) {
			tabcolnum = (*stmt)->setfuncdistinctcolumn;
			tabcolnumptr = &tabcolnum;
			orderarray = &tabcolnumptr;
			i1 = 1;
		}
		else if ((*stmt)->distinctflag) {
			i1 = (*stmt)->numcolumns;
			orderarray = (*stmt)->tabcolnumarray;
		}
		else {
			i1 = (*stmt)->numordercolumns;
			orderarray = (*stmt)->ordertabcolnumarray;
		}
		bestindex(onclause, tableref, tablenum, &indexnum, &readtype, trace, &tracecnt, &orderflag, i1, orderarray);
/*** NOTE: if 'select distinct' was not satisfied by index AND indexnum = 0, could write ***/
/***       function that checks for index containing the result columns.  if exact index ***/
/***       can be obtained with ANY of the result columns, then no column comparison (below) ***/
/***       would be needed.  If ALL columns (in any order) make up a 'dup/range' index key, then ***/
/***       can use column comparison (below); otherwise, unique sort is needed. ***/
		orderflag &= ~ORDER_TRYINDEX;  /* do not try to sort through index anymore */
		if (orderflag) dynamicflag = FALSE;

		/* establish the end of file and read next labels */
		prevreadnextlabel = readnextlabel;
		if (tableref > 1 && (*stmt)->tablejointypes[tableref - 2] == JOIN_LEFT_OUTER) {
			readnextlabel = endoffilelabel = LBL_READNEXTLOJ1 + ++lojlevel;
			lojvar = ++varcount;
			lojflag = TRUE;
		}
		else {
			readnextlabel = endoffilelabel = LBL_READNEXT + looplevel;
			lojflag = FALSE;
		}
		if (readtype == READ_INDEX_EXACT && dynamicflag) {
			addpgm2(OP_SET, ++varcount, 0);
			addpgm1(OP_GOTO, LBL_WORK1);
			readnextlabel = LBL_READNEXT + ++looplevel;
			defpgmlabel(readnextlabel);
			addpgm3(OP_ADD, varcount, VAR_FORWARD, varcount);
			addpgm2(OP_GOTOIFZERO, LBL_WORK1, varcount);
			/* now, either before of after record */
			addpgm2(OP_MOVE, VAR_FORWARD, varcount);
			if ((*stmt)->forupdateflag) addpgm1(OP_UNLOCK, 1);
			if (lojflag) addpgm1(OP_GOTO, prevreadnextlabel);
			else addpgm1(OP_GOTO, endoffilelabel);
/* LBL_WORK1 */
			/* read record after building key */
			defpgmlabel(LBL_WORK1);
			undefpgmlabel(LBL_WORK1);
		}
		if (lojflag) addpgm2(OP_SET, lojvar, 0);

		/* load the columns corresponding to the keys */
		dynamickeyflag = readtype != READ_INDEX_EXACT && dynamicflag && (orconditionflag || tableref > 1);
		keyflag = buildindexkey(hplan, onclause, tableref, indexnum, readtype, trace, tracecnt, dynamickeyflag);
		if (keyflag) {  /* using index to establish starting position */
			if (dynamickeyflag) {
				/* buildindexkey sets var1 if range key can be used for current direction */
				if (readtype == READ_INDEX_RANGE) addpgm2(OP_GOTOIFZERO, LBL_WORK2, VAR_1);
				addpgm2(OP_GOTOIFNEG, pgmcount + 3, VAR_FORWARD);
			}
			addpgm3(OP_READBYKEY, tableref, indexnum, VAR_1);  /* read by key, tableref, index, result var1 */
			if (dynamickeyflag) {
				addpgm1(OP_GOTO, pgmcount + 2);
				addpgm3(OP_READBYKEYREV, tableref, indexnum, VAR_1);  /* read by last key, tableref, index, result var1 */
			}
			addpgm2(OP_GOTOIFTRUE, LBL_WORK1, VAR_1);
			/* unsuccessful read */
			if (readtype != READ_INDEX_RANGE) addpgm1(OP_GOTO, endoffilelabel);
			else if (dynamickeyflag) addpgm1(OP_GOTO, LBL_WORK3);
		}
		if (!keyflag || (readtype == READ_INDEX_RANGE && dynamickeyflag)) {
/* LBL_WORK2 */
			defpgmlabel(LBL_WORK2);
			undefpgmlabel(LBL_WORK2);
			if (dynamickeyflag) addpgm2(OP_GOTOIFNEG, pgmcount + 3, VAR_FORWARD);
			addpgm2(OP_SETFIRST, tableref, indexnum);
			if (dynamickeyflag) {
				addpgm1(OP_GOTO, pgmcount + 2);
				addpgm2(OP_SETLAST, tableref, indexnum);
			}
/* LBL_WORK3 */
			defpgmlabel(LBL_WORK3);
			undefpgmlabel(LBL_WORK3);
		}
		/* set up readnext loop */
		if (readtype != READ_INDEX_EXACT) {
			readnextlabel = LBL_READNEXT + ++looplevel;
			defpgmlabel(readnextlabel);
			if (dynamicflag) addpgm2(OP_GOTOIFNEG, pgmcount + 3, VAR_FORWARD);
			addpgm3(OP_READNEXT, tableref, indexnum, VAR_1);  /* read next */
			if (dynamicflag) {
				addpgm1(OP_GOTO, pgmcount + 2);
				addpgm3(OP_READPREV, tableref, indexnum, VAR_1);  /* read previous */
			}
			addpgm2(OP_GOTOIFFALSE, endoffilelabel, VAR_1);
		}
		if (keyflag) {
/* LBL_WORK1 */
			defpgmlabel(LBL_WORK1);
			undefpgmlabel(LBL_WORK1);
		}
		/* break out of read loop when key range is exceeded */
		checkindexrange(hplan, onclause, tableref, indexnum, readtype, trace, tracecnt, dynamicflag, endoffilelabel);
		tab1 = tableptr(tablenum);
		if (tab1->filtercolumnnum) {
			addpgm3(OP_COLCOMPARE, makelitfromname(tab1->filtervalue), gettabcolref(tableref, tab1->filtercolumnnum), VAR_1);
			addpgm2(OP_GOTOIFNOTZERO, readnextlabel, VAR_1);  /* skip this row */
		}				
		if (onclause != NULL) {
			swhere = onclause;
			if (readnextlabel == endoffilelabel) {  /* reduce reference requirements */
				addpgm1(OP_GOTO, pgmcount + 2);
				explabel = LBL_READNEXTEXP + ++explevel;
				defpgmlabel(explabel);
				addpgm1(OP_GOTO, readnextlabel);
			}
			else explabel = readnextlabel;
			genlexpcode((*onclause)->numentries - 1, hplan, NULL, explabel);  /* generate expression code */
			if (readnextlabel == endoffilelabel) undefpgmlabel(explabel);
		}
		if (lojflag) {
			addpgm2(OP_SET, lojvar, 1);
			addpgm1(OP_GOTO, pgmcount + 6);
			defpgmlabel(LBL_READNEXTLOJ1 + lojlevel);
			addpgm2(OP_GOTOIFNOTZERO, prevreadnextlabel, lojvar);
			addpgm1(OP_CLEAR, tableref);  /* no matching right table record for left out join */
			addpgm1(OP_GOTO, pgmcount + 3);
			defpgmlabel(LBL_READNEXTLOJ2 + lojlevel);
			addpgm2(OP_GOTOIFZERO, prevreadnextlabel, lojvar);
			addpgm1(OP_GOTO, readnextlabel);
			readnextlabel = LBL_READNEXTLOJ2 + lojlevel;
		}
	}

	if ((*stmt)->where != NULL) {
		swhere = (*stmt)->where;
		if (readnextlabel == endoffilelabel) {  /* reduce reference requirements */
			addpgm1(OP_GOTO, pgmcount + 2);
			explabel = LBL_READNEXTEXP + ++explevel;
			defpgmlabel(explabel);
			addpgm1(OP_GOTO, readnextlabel);
		}
		else explabel = readnextlabel;
		genlexpcode((*swhere)->numentries - 1, hplan, NULL, explabel);
		if (readnextlabel == endoffilelabel) undefpgmlabel(explabel);
	}

	if (dynamicflag) {
		if ((*stmt)->distinctflag) {
			addpgm2(OP_GOTOIFZERO, LBL_WORK1, VAR_RECNUM);
			addpgm3(OP_SUB, VAR_RECNUM, VAR_RECCNT, VAR_1);
			addpgm2(OP_GOTOIFPOS, LBL_WORK1, VAR_1);
			worksetinfo[0].tabcolref = gettabcolref(worksetinfo[0].workset, 1);
			for (i1 = 0, i2 = (*stmt)->numcolumns; i1 < i2; i1++) {
				addpgm3(OP_COLCOMPARE, worksetinfo[0].tabcolref++, (*(*stmt)->tabcolnumarray)[i1], VAR_1);
				addpgm2(OP_GOTOIFNOTZERO, LBL_WORK1, VAR_1);
			}
/*** CODE: COULD OPTIMIZE IF USING INDEX BY INCREMENTING THE DISTINCT COLUMN ***/
/***       PORTION OF THE INDEX AND CAUSING AN INDEX READ KEY. IF FORWARD = -1, ***/
/***       THEN HAVE TO READ CURRENT KEY AND READ KEY PREVIOUS. SITUATION DOES ***/
/***       DOES NOT WARRANT THE AMOUNT OF WORK AT THIS TIME ***/
			addpgm1(OP_GOTO, readnextlabel);
/* LBL_WORK1 */
			defpgmlabel(LBL_WORK1);
			undefpgmlabel(LBL_WORK1);
		}
		/* increment/decrement current record number and check if record count needs to be incremented */
		addpgm3(OP_ADD, VAR_RECNUM, VAR_FORWARD, VAR_RECNUM);
		addpgm2(OP_GOTOIFPOS, pgmcount + 2, VAR_RECNUM);
		addpgm2(OP_SET, VAR_RECNUM, 1);
		addpgm3(OP_SUB, VAR_RECCNT, VAR_RECNUM, VAR_1);
		addpgm2(OP_GOTOIFNOTNEG, pgmcount + 2, VAR_1);
		addpgm2(OP_MOVE, VAR_RECNUM, VAR_RECCNT);
		if (!(*stmt)->forupdateflag) {
			addpgm2(OP_GOTOIFNOTZERO, pgmcount + 4, VAR_FUNC);
			/* func_init: create workset row */
			addpgm2(OP_INCR, VAR_WORKSET, 1);
			addpgm1(OP_WORKNEWROW, worksetinfo[0].workset);
			addpgm1(OP_GOTO, LBL_WORK1);
		}
		else {
			addpgm2(OP_GOTOIFNOTZERO, pgmcount + 3, VAR_WORKSET);
			/* first time, create row */
			addpgm2(OP_INCR, VAR_WORKSET, 1);
			addpgm1(OP_WORKNEWROW, worksetinfo[0].workset);
		}
		addpgm2(OP_MOVE, VAR_FUNC, VAR_1);
		addpgm2(OP_INCR, VAR_1, -4);
		addpgm2(OP_GOTOIFNEG, LBL_WORK1, VAR_1);
		/* if func_getlast continue reading to EOF */
		addpgm2(OP_GOTOIFZERO, readnextlabel, VAR_1);
		addpgm2(OP_INCR, VAR_1, -1);
		addpgm2(OP_GOTOIFNOTZERO, LBL_WORK2, VAR_1);
		/* func_getabs: see if done or read again */
		addpgm2(OP_GOTOIFNOTZERO, pgmcount + 4, VAR_2);
		/* read first record, now position in front of file */
		addpgm2(OP_SET, VAR_FUNC, 2);
		addpgm2(OP_SET, VAR_FORWARD, -1);
		addpgm1(OP_GOTO, readnextlabel);
		/* if negative absolute continue reading to EOF */
		addpgm2(OP_GOTOIFNEG, readnextlabel, VAR_2);
		addpgm3(OP_SUB, VAR_2, VAR_RECNUM, VAR_1);
		addpgm2(OP_GOTOIFNOTZERO, readnextlabel, VAR_1);
		addpgm1(OP_GOTO, LBL_WORK1);
/* LBL_WORK2 */
		defpgmlabel(LBL_WORK2);
		undefpgmlabel(LBL_WORK2);
		addpgm2(OP_INCR, VAR_1, -1);
		addpgm2(OP_GOTOIFNOTZERO, pgmcount + 3, VAR_1);
		/* func_getrel: see if done or read again */
		addpgm3(OP_SUB, VAR_2, VAR_FORWARD, VAR_2);
		addpgm2(OP_GOTOIFNOTZERO, readnextlabel, VAR_2);
/* LBL_WORK1 */
		defpgmlabel(LBL_WORK1);
		undefpgmlabel(LBL_WORK1);
		/* move the columns to the workset */
		addpgm1(OP_CALL, LBL_MOVECOLUMNS);
		if (!(*stmt)->forupdateflag) {
			addpgm2(OP_GOTOIFNOTZERO, LBL_RETURN7, VAR_FUNC);
#if 0
/*** CODE: DISABLED 'TRY FOR 2 RECORDS INSTEAD OF JUST 1' ***/
			/* func_init: if first record, try for one more */
			addpgm2(OP_MOVE, VAR_RECCNT, VAR_1);
			addpgm2(OP_INCR, VAR_1, -1);
			addpgm2(OP_GOTOIFZERO, readnextlabel, VAR_1);
#endif
			addpgm2(OP_SET, VAR_RECNUM, 0);
		}
		else addpgm2(OP_SET, VAR_UPDATE, 1);
/* LBL_RETURN7 */
		defpgmlabel(LBL_RETURN7);
		/* return code 7, means multi row result and planexec required */
		addpgm2(OP_SET, VAR_1, 7);
		addpgm0(OP_FINISH);

/* LBL_READFINISH */
		/* reached beginning or end of file */
		defpgmlabel(LBL_READFINISH);
		if (!(*stmt)->forupdateflag) {
			addpgm2(OP_GOTOIFNOTZERO, LBL_WORK1, VAR_FUNC);
			/* func_init */
#if 0
/*** CODE: DISABLED 'TRY FOR 2 RECORDS INSTEAD OF JUST 1' ***/
			addpgm2(OP_SET, VAR_RECNUM, 0);
			addpgm2(OP_GOTOIFZERO, pgmcount + 5, VAR_RECCNT);
			/* set rowid to 0 */
			addpgm2(OP_SET, VAR_1, 0);
			addpgm2(OP_WORKSETROWID, worksetinfo[0].workset, VAR_1);
			/* return code 6, means completed result set w/ 1 row */
			addpgm2(OP_SET, VAR_1, 6);
			addpgm0(OP_TERMINATE);
#endif
			/* return code 5, means it's a zero row result set */
			addpgm2(OP_SET, VAR_1, 5);
			addpgm0(OP_TERMINATE);
/* LBL_WORK1 */
			defpgmlabel(LBL_WORK1);
			undefpgmlabel(LBL_WORK1);
		}
		addpgm2(OP_MOVE, VAR_FUNC, VAR_1);
		addpgm2(OP_INCR, VAR_1, -1);
		addpgm2(OP_GOTOIFNOTZERO, pgmcount + 4, VAR_1);
		/* func_getnext reached EOF */
		addpgm2(OP_MOVE, VAR_RECNUM, VAR_RECCNT);
		addpgm2(OP_INCR, VAR_RECNUM, 1);
		addpgm1(OP_GOTO, LBL_RETURN5);
		addpgm2(OP_INCR, VAR_1, -1);
		addpgm2(OP_GOTOIFNOTZERO, pgmcount + 3, VAR_1);
		/* func_getprev reached BOF */
		addpgm2(OP_SET, VAR_RECNUM, 0);
		addpgm1(OP_GOTO, LBL_RETURN5);
		addpgm2(OP_INCR, VAR_1, -1);
		addpgm2(OP_GOTOIFNOTZERO, pgmcount + 4, VAR_1);
		/* func_getfirst reached EOF */
		addpgm2(OP_SET, VAR_RECNUM, 0);
		addpgm2(OP_SET, VAR_RECCNT, 0);
		addpgm1(OP_GOTO, LBL_RETURN5);
		addpgm2(OP_INCR, VAR_1, -1);
		addpgm2(OP_GOTOIFNOTZERO, pgmcount + 5, VAR_1);
		/* func_getlast reached EOF, go back one record */
		addpgm2(OP_INCR, VAR_RECNUM, 1);
		addpgm2(OP_SET, VAR_FUNC, 2);
		addpgm2(OP_SET, VAR_FORWARD, -1);
		addpgm1(OP_GOTO, readnextlabel);
		addpgm2(OP_INCR, VAR_1, -1);
		addpgm2(OP_GOTOIFNOTZERO, LBL_WORK1, VAR_1);
		/* func_getabs reached EOF */
		addpgm2(OP_GOTOIFNOTZERO, pgmcount + 4, VAR_2);
		/* no records to position in front of */
		addpgm2(OP_SET, VAR_RECNUM, 0);
		addpgm2(OP_SET, VAR_RECCNT, 0);
		addpgm1(OP_GOTO, LBL_RETURN5);
		/* start reading backwards if negative absolute */
		addpgm2(OP_GOTOIFPOS, pgmcount + 4, VAR_2);
		addpgm2(OP_SET, VAR_FUNC, 6);
		addpgm2(OP_SET, VAR_FORWARD, -1);
		addpgm1(OP_GOTO, readnextlabel);
		addpgm2(OP_MOVE, VAR_RECNUM, VAR_RECCNT);
		addpgm2(OP_INCR, VAR_RECNUM, 1);
		addpgm1(OP_GOTO, LBL_RETURN5);
/* LBL_WORK1 */
		defpgmlabel(LBL_WORK1);
		undefpgmlabel(LBL_WORK1);
		/* func_getrel reached EOF */
		addpgm2(OP_GOTOIFNEG, pgmcount + 3, VAR_FORWARD);
		addpgm2(OP_MOVE, VAR_RECNUM, VAR_RECCNT);
		addpgm2(OP_INCR, VAR_RECNUM, 1);
		addpgm1(OP_GOTO, LBL_RETURN5);
		addpgm2(OP_SET, VAR_RECNUM, 0);
/* LBL_RETURN5 */
		defpgmlabel(LBL_RETURN5);
		if ((*stmt)->forupdateflag) addpgm2(OP_SET, VAR_UPDATE, 0);
		/* return code 5, BOF or EOF */ 
		addpgm2(OP_SET, VAR_1, 5);
		addpgm0(OP_FINISH);

/* LBL_FUNC_INIT */
		/* init code */
		defpgmlabel(LBL_FUNC_INIT);
		/* initialize workset */
#if 0
/*** CODE: DISABLED 'TRY FOR 2 RECORDS INSTEAD OF JUST 1' ***/
		if (!(*stmt)->forupdateflag) addpgm2(OP_SET, VAR_1, 2);
		else
#endif
		addpgm2(OP_SET, VAR_1, 1);
		addpgm2(OP_WORKINIT, worksetinfo[0].workset, VAR_1);
		if (orconditionflag) addpgm1(OP_CALL, LBL_FILLWORKSET);
/*** NOTE: INITIALIZE ANY OTHER REQUIRED WORKSETS HERE ***/
#if 0
/*** NOTE: fssql5.c initializes all variables to zero ***/
		addpgm2(OP_SET, VAR_WORKSET, 0);
		addpgm2(OP_SET, VAR_RECCNT, 0);
#endif
		if (!(*stmt)->forupdateflag) addpgm1(OP_GOTO, LBL_READFIRST);
		else {
#if 0
/*** NOTE: fssql5.c initializes all variables to zero ***/
			addpgm2(OP_SET, VAR_UPDATE, 0);
#endif
			addpgm1(OP_GOTO, LBL_RETURN7);
		}

/* LBL_FUNC_GETNEXT */
		/* func_getnext entry point */
		defpgmlabel(LBL_FUNC_GETNEXT);
		if (!(*stmt)->forupdateflag) {
			addpgm3(OP_SUB, VAR_WORKSET, VAR_RECNUM, VAR_1);
			addpgm2(OP_GOTOIFNOTPOS, pgmcount + 4, VAR_1);
			addpgm2(OP_INCR, VAR_RECNUM, 1);
			addpgm2(OP_WORKSETROWID, worksetinfo[0].workset, VAR_RECNUM);
			addpgm1(OP_GOTO, LBL_RETURN7);
			/* done with preread row workset */
			addpgm2(OP_SET, VAR_WORKSET, 0);
#if 0
/*** CODE: DISABLED 'TRY FOR 2 RECORDS INSTEAD OF JUST 1' (WE COULD SKIP NEXT 2 LINES AND JUST LEAVE ON ROWID 2) ***/
/***       MAY WANT TO JUMP OVER THIS CODE IF WORKSET ALREADY 0 ***/
			addpgm2(OP_SET, VAR_1, 1);
			addpgm2(OP_WORKSETROWID, worksetinfo[0].workset), VAR_1);
#endif
		}
		else addpgm2(OP_GOTOIFZERO, LBL_READFIRST, VAR_RECCNT);
		addpgm3(OP_SUB, VAR_RECNUM, VAR_RECCNT, VAR_1);
		addpgm2(OP_GOTOIFNOTPOS, pgmcount + 2, VAR_1);
		addpgm2(OP_MOVE, VAR_RECCNT, VAR_RECNUM);
		addpgm2(OP_SET, VAR_FORWARD, 1);
		addpgm1(OP_GOTO, readnextlabel);

/* LBL_FUNC_GETPREV */
		/* func_getprev entry point */
		defpgmlabel(LBL_FUNC_GETPREV);
		if (!(*stmt)->forupdateflag) {
			addpgm2(OP_GOTOIFZERO, pgmcount + 6, VAR_WORKSET);
			/* have row in result set */
			addpgm2(OP_GOTOIFZERO, LBL_RETURN5, VAR_RECNUM);
			addpgm2(OP_INCR, VAR_RECNUM, -1);
			addpgm2(OP_GOTOIFZERO, LBL_RETURN5, VAR_RECNUM);
			addpgm2(OP_WORKSETROWID, worksetinfo[0].workset, VAR_RECNUM);
			addpgm1(OP_GOTO, LBL_RETURN7);
		}
		else addpgm2(OP_GOTOIFZERO, LBL_RETURN5, VAR_RECCNT);
		addpgm2(OP_SET, VAR_FORWARD, -1);
		addpgm1(OP_GOTO, readnextlabel);

/* LBL_FUNC_GETFIRST */
		/* func_getfirst entry point */
		defpgmlabel(LBL_FUNC_GETFIRST);
		if (!(*stmt)->forupdateflag) {
			addpgm2(OP_GOTOIFZERO, pgmcount + 4, VAR_WORKSET);
			addpgm2(OP_SET, VAR_RECNUM, 1);
			addpgm2(OP_WORKSETROWID, worksetinfo[0].workset, VAR_RECNUM);
			addpgm1(OP_GOTO, LBL_RETURN7);
		}
		addpgm1(OP_GOTO, LBL_READFIRST);

/* LBL_FUNC_GETLAST */
		/* func_getlast entry point */
		defpgmlabel(LBL_FUNC_GETLAST);
		if (!(*stmt)->forupdateflag) {
			addpgm2(OP_GOTOIFZERO, LBL_WORK1, VAR_WORKSET);
			/* done with preread row workset */
			addpgm2(OP_MOVE, VAR_WORKSET, VAR_RECNUM);
			addpgm2(OP_SET, VAR_WORKSET, 0);
#if 0
/*** CODE: DISABLED 'TRY FOR 2 RECORDS INSTEAD OF JUST 1' (WE COULD SKIP NEXT 2 LINES AND JUST LEAVE ON ROWID 2) ***/
			addpgm2(OP_SET, VAR_1, 1);
			addpgm2(OP_WORKSETROWID, worksetinfo[0].workset, VAR_1);
#endif
/* LBL_WORK1 */
			defpgmlabel(LBL_WORK1);
			undefpgmlabel(LBL_WORK1);
		}
		else addpgm2(OP_GOTOIFZERO, LBL_READFIRST, VAR_RECCNT);
		addpgm2(OP_SET, VAR_FORWARD, 1);
		addpgm1(OP_GOTO, readnextlabel);

/* LBL_FUNC_GETABS */
		/* func_getabs entry point */
		defpgmlabel(LBL_FUNC_GETABS);
		if (!(*stmt)->forupdateflag) {
			addpgm2(OP_GOTOIFZERO, LBL_WORK1, VAR_WORKSET);
			addpgm3(OP_SUB, VAR_WORKSET, VAR_2, VAR_1);
			addpgm2(OP_GOTOIFNEG, pgmcount + 5, VAR_1);
			addpgm2(OP_MOVE, VAR_2, VAR_RECNUM);
			addpgm2(OP_GOTOIFZERO, LBL_RETURN5, VAR_RECNUM);
			addpgm2(OP_WORKSETROWID, worksetinfo[0].workset, VAR_RECNUM);
			addpgm1(OP_GOTO, LBL_RETURN7);
			/* done with preread row workset */
			addpgm2(OP_MOVE, VAR_WORKSET, VAR_RECNUM);
			addpgm2(OP_SET, VAR_WORKSET, 0);
#if 0
/*** CODE: DISABLED 'TRY FOR 2 RECORDS INSTEAD OF JUST 1' (WE COULD SKIP NEXT 2 LINES AND JUST LEAVE ON ROWID 2) ***/
			addpgm2(OP_SET, VAR_1, 1);
			addpgm2(OP_WORKSETROWID, worksetinfo[0].workset, VAR_1);
#endif
/* LBL_WORK1 */
			defpgmlabel(LBL_WORK1);
			undefpgmlabel(LBL_WORK1);
		}
		else addpgm2(OP_GOTOIFZERO, LBL_READFIRST, VAR_RECCNT);
		addpgm2(OP_GOTOIFZERO, LBL_READFIRST, VAR_2);  /* position before first record */
		addpgm2(OP_GOTOIFPOS, pgmcount + 3, VAR_2);
		/* read from the end of file */
		addpgm2(OP_SET, VAR_FORWARD, 1);
		addpgm1(OP_GOTO, readnextlabel);
		addpgm3(OP_SUB, VAR_2, VAR_RECNUM, VAR_1);
		/* read from the beginning of the file */
		addpgm2(OP_GOTOIFNEG, LBL_READFIRST, VAR_1);
		/* read from current position and change to relative fetch */
		addpgm3(OP_SUB, VAR_RECNUM, VAR_RECCNT, VAR_1);
		addpgm2(OP_GOTOIFNOTPOS, pgmcount + 2, VAR_1);
		addpgm2(OP_MOVE, VAR_RECCNT, VAR_RECNUM);
		addpgm3(OP_SUB, VAR_2, VAR_RECNUM, VAR_2);
		addpgm2(OP_GOTOIFZERO, LBL_RETURN7, VAR_2);
		addpgm2(OP_SET, VAR_FUNC, 6);
		addpgm2(OP_SET, VAR_FORWARD, 1);
		addpgm1(OP_GOTO, readnextlabel);

/* LBL_FUNC_GETREL */
		/* func_getrel entry point */
		defpgmlabel(LBL_FUNC_GETREL);
		if (!(*stmt)->forupdateflag) {
			addpgm2(OP_GOTOIFZERO, LBL_WORK1, VAR_WORKSET);
			addpgm3(OP_ADD, VAR_RECNUM, VAR_2, VAR_1);
			addpgm2(OP_GOTOIFNOTNEG, pgmcount + 2, VAR_1);
			addpgm2(OP_SET, VAR_1, 0);
			/* VAR_1 contains absolute record number */
			addpgm3(OP_SUB, VAR_WORKSET, VAR_1, VAR_1);
			addpgm2(OP_GOTOIFNEG, pgmcount + 5, VAR_1);
			addpgm2(OP_MOVE, VAR_1, VAR_RECNUM);
			addpgm2(OP_GOTOIFZERO, LBL_RETURN5, VAR_RECNUM);
			addpgm2(OP_WORKSETROWID, worksetinfo[0].workset, VAR_RECNUM);
			addpgm1(OP_GOTO, LBL_RETURN7);
			/* done with preread row workset */
			addpgm3(OP_SUB, VAR_WORKSET, VAR_RECNUM, VAR_RECNUM);
			addpgm3(OP_SUB, VAR_2, VAR_RECNUM, VAR_2);
			addpgm2(OP_MOVE, VAR_WORKSET, VAR_RECNUM);
			addpgm2(OP_SET, VAR_WORKSET, 0);
#if 0
/*** CODE: DISABLED 'TRY FOR 2 RECORDS INSTEAD OF JUST 1' (WE COULD SKIP NEXT 2 LINES AND JUST LEAVE ON ROWID 2) ***/
			addpgm2(OP_SET, VAR_1, 1);
			addpgm2(OP_WORKSETROWID, worksetinfo[0].workset, VAR_1);
#endif
/* LBL_WORK1 */
			defpgmlabel(LBL_WORK1);
			undefpgmlabel(LBL_WORK1);
			addpgm2(OP_GOTOIFNOTNEG, pgmcount + 3, VAR_2);
			/* read backwards from current position */
		}
		else {
			addpgm2(OP_GOTOIFNOTNEG, pgmcount + 4, VAR_2);
			/* read backwards from current position */
			addpgm2(OP_GOTOIFZERO, LBL_RETURN5, VAR_RECCNT);
		}
		addpgm2(OP_SET, VAR_FORWARD, -1);
		addpgm1(OP_GOTO, readnextlabel);
		if ((*stmt)->forupdateflag) {
			addpgm2(OP_GOTOIFNOTZERO, pgmcount + 3, VAR_RECCNT);
			addpgm2(OP_GOTOIFZERO, LBL_RETURN5, VAR_2);
			addpgm1(OP_GOTO, LBL_READFIRST);
		}
		addpgm3(OP_SUB, VAR_RECNUM, VAR_RECCNT, VAR_1);
		addpgm2(OP_GOTOIFNOTPOS, pgmcount + 5, VAR_1);
		/* beyond end of file */
		addpgm2(OP_MOVE, VAR_RECCNT, VAR_RECNUM);
		addpgm2(OP_GOTOIFNOTZERO, pgmcount + 4, VAR_2);
		/* turn zero offset to one offset */
		addpgm2(OP_SET, VAR_2, 1);
		addpgm1(OP_GOTO, pgmcount + 2);
		addpgm2(OP_GOTOIFZERO, pgmcount + 3, VAR_2);
		/* read forward from current position */
		addpgm2(OP_SET, VAR_FORWARD, 1);
		addpgm1(OP_GOTO, readnextlabel);
		addpgm2(OP_GOTOIFZERO, LBL_RETURN5, VAR_RECNUM);
		addpgm1(OP_GOTO, LBL_RETURN7);

		/* done with return labels */
		undefpgmlabel(LBL_RETURN5);
		undefpgmlabel(LBL_RETURN7);

		/* get row count entry point */
/* LBL_FUNC_GETROWCNT */
		defpgmlabel(LBL_FUNC_GETROWCNT);
		addpgm2(OP_MOVE, VAR_RECCNT, VAR_2);
		addpgm2(OP_SET, VAR_1, 1);
		addpgm0(OP_FINISH);

		if ((*stmt)->forupdateflag) {
/* LBL_FUNC_UPDATE */
			/* update positioned code */
			defpgmlabel(LBL_FUNC_UPDATE);
			addpgm2(OP_GOTOIFNOTZERO, pgmcount + 3, VAR_UPDATE);
			addpgm2(OP_SET, VAR_2, 0);  /* set update count to 0 */
			addpgm1(OP_GOTO, pgmcount + 5);
			addpgm1(OP_CALL, LBL_MOVEUPDATE);
			addpgm1(OP_UPDATE, 1);
			addpgm2(OP_SET, VAR_UPDATE, 0);
			addpgm2(OP_SET, VAR_2, 1);  /* set update count to 1 */
			addpgm2(OP_SET, VAR_1, 1);  /* return code 1, means count in variable 2 */
			addpgm0(OP_FINISH);
/* LBL_FUNC_DELETE */
			/* delete positioned code */
			defpgmlabel(LBL_FUNC_DELETE);
			addpgm2(OP_GOTOIFNOTZERO, pgmcount + 3, VAR_UPDATE);
			addpgm2(OP_SET, VAR_2, 0);  /* set delete count to 0 */
			addpgm1(OP_GOTO, pgmcount + 4);
			addpgm1(OP_DELETE, 1);
			addpgm2(OP_SET, VAR_UPDATE, 0);
			addpgm2(OP_SET, VAR_2, 1);  /* set delete count to 1 */
			addpgm2(OP_SET, VAR_1, 1);  /* return code 1, means count in variable 2 */
			addpgm0(OP_FINISH);
/* LBL_MOVEUPDATE */
			/* update move column label & return */
			defpgmlabel(LBL_MOVEUPDATE);
			addpgm0(OP_RETURN);
		}
	}
	else {  /* static: sorting or set functions, get all rows */
		if ((*stmt)->forupdateflag && (*stmt)->distinctflag) {
			/* NOTE: decided to make this limitation as the "distinct" record */
			/*       could get modified/deleted and we would incorrectly not */
			/*       display the distinct data when it exists in other records */
			sqlerrnummsg(SQLERR_PARSE_ERROR, "FOR UPDATE not supported with DISTINCT not supported through an index", NULL);
			goto buildselectplanerror;
		}
		if ((*stmt)->numgroupcolumns) {
			if (!(orderflag & ORDER_OTHER)) {
				addpgm2(OP_GOTOIFZERO, LBL_WORK1, VAR_RECNUM);
				worksetinfo[2].tabcolref = gettabcolref(worksetinfo[2].workset, 1);
				for (i1 = 0, i2 = (*stmt)->numgroupcolumns; i1 < i2; i1++) {
					addpgm3(OP_COLCOMPARE, worksetinfo[2].tabcolref++, (*(*stmt)->grouptabcolnumarray)[i1], VAR_1);
					addpgm2(OP_GOTOIFNOTZERO, LBL_WORK2, VAR_1);
				}
				if (needmovecols & (NEED_MOVECOLS_PLUS1 | NEED_MOVECOLS_PLUS2)) {
					if (needmovecols & NEED_MOVECOLS_PLUS1) addpgm1(OP_CALL, LBL_MOVECOLUMNS + 1);
					addpgm2(OP_INCR, VAR_NONDIST, 1);
				}
				if (needmovecols & (NEED_MOVECOLS_PLUS4 | NEED_MOVECOLS_PLUS5)) addpgm1(OP_CALL, LBL_MOVECOLUMNS + 3);
				addpgm1(OP_GOTO, readnextlabel);
/* LBL_WORK2 */
				defpgmlabel(LBL_WORK2);
				undefpgmlabel(LBL_WORK2);
				if (needmovecols & NEED_MOVECOLS_PLUS2) addpgm1(OP_CALL, LBL_MOVECOLUMNS + 2);
				if (needmovecols & NEED_MOVECOLS_PLUS5) addpgm1(OP_CALL, LBL_MOVECOLUMNS + 5);
				if ((*stmt)->having != NULL) {  /* generate expression code */
					swhere = (*stmt)->having;
					genlexpcode((*swhere)->numentries - 1, hplan, &expsrc, LBL_WORK1);
				}
				addpgm1(OP_WORKNEWROW, worksetinfo[0].workset);
				addpgm1(OP_CALL, LBL_MOVECOLUMNS);
/* LBL_WORK1 */
				defpgmlabel(LBL_WORK1);
				undefpgmlabel(LBL_WORK1);
				if (needmovecols & (NEED_MOVECOLS_PLUS1 | NEED_MOVECOLS_PLUS2)) {
					addpgm2(OP_SET, VAR_NONDIST, 0);
					if (needmovecols & NEED_MOVECOLS_PLUS1) addpgm1(OP_CALL, LBL_MOVECOLUMNS + 1);
					addpgm2(OP_INCR, VAR_NONDIST, 1);
				}
				if (needmovecols & (NEED_MOVECOLS_PLUS4 | NEED_MOVECOLS_PLUS5)) {
					addpgm2(OP_SET, VAR_DISTNCT, 0);
					addpgm1(OP_CALL, LBL_MOVECOLUMNS + 3);
				}
				addpgm1(OP_CALL, LBL_MOVECOLUMNS + 7);
			}
			else {
				addpgm1(OP_WORKNEWROW, worksetinfo[3].workset);
				addpgm1(OP_CALL, LBL_MOVECOLUMNS + 6);
			}
			addpgm2(OP_INCR, VAR_RECNUM, 1);
		}
		else if ((*stmt)->numsetfunctions) {
			if (needmovecols & (NEED_MOVECOLS_PLUS1 | NEED_MOVECOLS_PLUS2)) {
				if (needmovecols & NEED_MOVECOLS_PLUS1) addpgm1(OP_CALL, LBL_MOVECOLUMNS + 1);
				addpgm2(OP_INCR, VAR_NONDIST, 1);
			}
			if ((*stmt)->setfuncdistinctcolumn) {
				if (!(orderflag & ORDER_OTHER)) addpgm1(OP_CALL, LBL_MOVECOLUMNS + 3);  /* using index */
				else {
					addpgm1(OP_WORKNEWROW, worksetinfo[3].workset);
					addpgm1(OP_CALL, LBL_MOVECOLUMNS + 6);
				}
			}
		}
		else {
			if ((*stmt)->distinctflag && !(orderflag & ORDER_OTHER)) {
				addpgm2(OP_GOTOIFZERO, LBL_WORK1, VAR_RECNUM);
				worksetinfo[0].tabcolref = gettabcolref(worksetinfo[0].workset, 1);
				for (i1 = 0, i2 = (*stmt)->numcolumns; i1 < i2; i1++) {
					addpgm3(OP_COLCOMPARE, worksetinfo[0].tabcolref++, (*(*stmt)->tabcolnumarray)[i1], VAR_1);
					addpgm2(OP_GOTOIFNOTZERO, LBL_WORK1, VAR_1);
				}
/*** CODE: COULD OPTIMIZE IF USING INDEX BY INCREMENTING THE DISTINCT COLUMN ***/
/***       PORTION OF THE INDEX AND CAUSING AN INDEX READ KEY. SITUATION DOES ***/
/***       DOES NOT WARRANT THE AMOUNT OF WORK AT THIS TIME ***/
				addpgm1(OP_GOTO, readnextlabel);
/* LBL_WORK1 */
				defpgmlabel(LBL_WORK1);
				undefpgmlabel(LBL_WORK1);
			}
			addpgm2(OP_INCR, VAR_RECNUM, 1);
			addpgm1(OP_WORKNEWROW, worksetinfo[0].workset);
			addpgm1(OP_CALL, LBL_MOVECOLUMNS);
		}
		addpgm1(OP_GOTO, readnextlabel);
/* LBL_READFINISH */
		defpgmlabel(LBL_READFINISH);
		if ((*stmt)->numsetfunctions && !(*stmt)->numgroupcolumns) {
			/* select is using set functions */
			if (needmovecols & NEED_MOVECOLS_PLUS2) addpgm1(OP_CALL, LBL_MOVECOLUMNS + 2);
			if ((*stmt)->setfuncdistinctcolumn) {
				if (orderflag & ORDER_OTHER) {  /* not using index */
					(*hplan)->flags |= PLAN_FLAG_SORT;
					addpgm2(OP_WORKGETROWCOUNT, worksetinfo[3].workset, VAR_RECCNT);
					addpgm2(OP_GOTOIFZERO, LBL_WORK1, VAR_RECCNT);
					addpgm2(OP_MOVE, VAR_RECCNT, VAR_1);
					addpgm2(OP_INCR, VAR_1, -1);
					addpgm2(OP_GOTOIFZERO, pgmcount + 4, VAR_1);
					/* need to sort distinct column */
					addpgm2(OP_SET, VAR_1, 0);
					addpgm2(OP_WORKUNIQUE, worksetinfo[3].workset, VAR_1);
					addpgm2(OP_WORKGETROWCOUNT, worksetinfo[3].workset, VAR_RECCNT);
					if (needmovecols & NEED_MOVECOLS_PLUS4) {
						/* now do set functions for distinct column */
						addpgm2(OP_SET, VAR_RECNUM, 0);
						/* beginning of parse unique work set loop */
						addpgm3(OP_SUB, VAR_RECCNT, VAR_RECNUM, VAR_1);
						addpgm2(OP_GOTOIFZERO, LBL_WORK1, VAR_1);
						addpgm2(OP_INCR, VAR_RECNUM, 1);
						addpgm2(OP_WORKSETROWID, worksetinfo[3].workset, VAR_RECNUM);
						addpgm1(OP_CALL, LBL_MOVECOLUMNS + 4);
						addpgm1(OP_GOTO, pgmcount - 5);
					}
					else addpgm2(OP_MOVE, VAR_RECCNT, VAR_DISTNCT);
/* LBL_WORK1 */
					defpgmlabel(LBL_WORK1);
					undefpgmlabel(LBL_WORK1);
					addpgm2(OP_SET, VAR_1, 0);
					addpgm2(OP_WORKFREE, worksetinfo[3].workset, VAR_1);
				}
				if (needmovecols & NEED_MOVECOLS_PLUS5) addpgm1(OP_CALL, LBL_MOVECOLUMNS + 5);
			}
			if ((*stmt)->having != NULL) {  /* generate expression code */
				swhere = (*stmt)->having;
				genlexpcode((*swhere)->numentries - 1, hplan, &expsrc, LBL_WORK1);
			}
/*** NOTE: IF ZERO RECORDS, THEN SUM AND AVG WILL BE NULL, PROBABLY OK ***/ 
			addpgm1(OP_CALL, LBL_MOVECOLUMNS);
			/* set rowid to 0 */
			addpgm2(OP_SET, VAR_1, 0);
			addpgm2(OP_WORKSETROWID, worksetinfo[0].workset, VAR_1);
			/* return code 6, means completed result set w/ 1 row */
			addpgm2(OP_SET, VAR_1, 6);
			addpgm0(OP_TERMINATE);
/* LBL_WORK1 */
			defpgmlabel(LBL_WORK1);
			undefpgmlabel(LBL_WORK1);
			/* return code 5 (zero row result set, w/ result id) */
			addpgm2(OP_SET, VAR_1, 5);
			addpgm0(OP_TERMINATE);
		}
		else {
			addpgm2(OP_MOVE, VAR_RECNUM, VAR_RECCNT);
			addpgm2(OP_GOTOIFNOTZERO, pgmcount + 3, VAR_RECCNT);
			/* return code 5 (zero row result set, w/ result id) */
			addpgm2(OP_SET, VAR_1, 5);
			addpgm0(OP_TERMINATE);
			if ((*stmt)->numgroupcolumns) {
				if (orderflag & ORDER_OTHER) {  /* non-indexed */
					orderflag &= ~ORDER_OTHER;
					(*hplan)->flags |= PLAN_FLAG_SORT;
					addpgm2(OP_MOVE, VAR_RECCNT, VAR_1);
					addpgm2(OP_INCR, VAR_1, -1);
					addpgm2(OP_GOTOIFZERO, LBL_WORK1, VAR_1);
					/* need to sort group by and distinct columns */
					addpgm2(OP_SET, VAR_1, 0);
					worksetinfo[3].tabcolref = gettabcolref(worksetinfo[3].workset, 1);
					for (i1 = 0, i2 = (*stmt)->numgroupcolumns + ((*stmt)->setfuncdistinctcolumn ? 1 : 0); i1 < i2; i1++) {
						addpgm2(OP_INCR, VAR_1, 1);
						addpgm2(OP_SORTSPEC, worksetinfo[3].tabcolref++, VAR_1);
					}
					addpgm2(OP_WORKSORT, worksetinfo[3].workset, VAR_1);
/* LBL_WORK1 */
					defpgmlabel(LBL_WORK1);
					undefpgmlabel(LBL_WORK1);
					addpgm2(OP_SET, VAR_RECNUM, 1);
					addpgm2(OP_WORKSETROWID, worksetinfo[3].workset, VAR_RECNUM);
					addpgm1(OP_GOTO, LBL_WORK1);
/* LBL_WORK3 */
					defpgmlabel(LBL_WORK3);
					/* beginning of parse sorted workset loop */
					addpgm3(OP_SUB, VAR_RECCNT, VAR_RECNUM, VAR_1);
					addpgm2(OP_GOTOIFZERO, LBL_WORK4, VAR_1);
					addpgm2(OP_INCR, VAR_RECNUM, 1);
					addpgm2(OP_WORKSETROWID, worksetinfo[3].workset, VAR_RECNUM);
					worksetinfo[2].tabcolref = gettabcolref(worksetinfo[2].workset, 1);
					worksetinfo[3].tabcolref = gettabcolref(worksetinfo[3].workset, 1);
					for (i1 = 0, i2 = (*stmt)->numgroupcolumns; i1 < i2; i1++) {
						addpgm3(OP_COLCOMPARE, worksetinfo[2].tabcolref++, worksetinfo[3].tabcolref++, VAR_1);
						addpgm2(OP_GOTOIFNOTZERO, LBL_WORK2, VAR_1);
					}
					if (needmovecols & (NEED_MOVECOLS_PLUS1 | NEED_MOVECOLS_PLUS4 | NEED_MOVECOLS_PLUS5)) addpgm1(OP_CALL, LBL_MOVECOLUMNS + 4);
					if (needmovecols & (NEED_MOVECOLS_PLUS1 | NEED_MOVECOLS_PLUS2)) addpgm2(OP_INCR, VAR_NONDIST, 1);
					addpgm1(OP_GOTO, LBL_WORK3);
/* LBL_WORK2 */
					defpgmlabel(LBL_WORK2);
					undefpgmlabel(LBL_WORK2);
					if (needmovecols & NEED_MOVECOLS_PLUS2) addpgm1(OP_CALL, LBL_MOVECOLUMNS + 2);
					if (needmovecols & NEED_MOVECOLS_PLUS5) addpgm1(OP_CALL, LBL_MOVECOLUMNS + 5);
					if ((*stmt)->having != NULL) {  /* generate expression code */
						swhere = (*stmt)->having;
						genlexpcode((*swhere)->numentries - 1, hplan, &expsrc, LBL_WORK1);
					}
					addpgm1(OP_WORKNEWROW, worksetinfo[0].workset);
					addpgm1(OP_CALL, LBL_MOVECOLUMNS);
/* LBL_WORK1 */
					defpgmlabel(LBL_WORK1);
					undefpgmlabel(LBL_WORK1);
					if (needmovecols) {
						if (needmovecols & (NEED_MOVECOLS_PLUS1 | NEED_MOVECOLS_PLUS2)) addpgm2(OP_SET, VAR_NONDIST, 0);
						if (needmovecols & (NEED_MOVECOLS_PLUS4 | NEED_MOVECOLS_PLUS5)) addpgm2(OP_SET, VAR_DISTNCT, 0);
						if (needmovecols & (NEED_MOVECOLS_PLUS1 | NEED_MOVECOLS_PLUS4 | NEED_MOVECOLS_PLUS5)) addpgm1(OP_CALL, LBL_MOVECOLUMNS + 4);
						if (needmovecols & (NEED_MOVECOLS_PLUS1 | NEED_MOVECOLS_PLUS2)) addpgm2(OP_INCR, VAR_NONDIST, 1);
					}
					addpgm1(OP_CALL, LBL_MOVECOLUMNS + 8);
					addpgm1(OP_GOTO, LBL_WORK3);
					undefpgmlabel(LBL_WORK3);
/* LBL_WORK4 */
					defpgmlabel(LBL_WORK4);
					undefpgmlabel(LBL_WORK4);
					addpgm2(OP_SET, VAR_1, 0);
					addpgm2(OP_WORKFREE, worksetinfo[3].workset, VAR_1);
				}
				/* finish last group */
				if (needmovecols & NEED_MOVECOLS_PLUS2) addpgm1(OP_CALL, LBL_MOVECOLUMNS + 2);
				if (needmovecols & NEED_MOVECOLS_PLUS5) addpgm1(OP_CALL, LBL_MOVECOLUMNS + 5);
				if ((*stmt)->having != NULL) {  /* generate expression code */
					swhere = (*stmt)->having;
					genlexpcode((*swhere)->numentries - 1, hplan, &expsrc, LBL_WORK1);
				}
				addpgm1(OP_WORKNEWROW, worksetinfo[0].workset);
				addpgm1(OP_CALL, LBL_MOVECOLUMNS);
/* LBL_WORK1 */
				defpgmlabel(LBL_WORK1);
				addpgm2(OP_WORKGETROWCOUNT, worksetinfo[0].workset, VAR_RECCNT);
				undefpgmlabel(LBL_WORK1);
				if ((*stmt)->having != NULL) {
					addpgm2(OP_GOTOIFNOTZERO, pgmcount + 3, VAR_RECCNT);
					/* return code 5 (zero row result set, w/ result id) */
					addpgm2(OP_SET, VAR_1, 5);
					addpgm0(OP_TERMINATE);
				}
			}
			if (orderflag) {
				(*hplan)->flags |= PLAN_FLAG_SORT;
				addpgm2(OP_MOVE, VAR_RECCNT, VAR_1);
				addpgm2(OP_INCR, VAR_1, -1);
				addpgm2(OP_GOTOIFZERO, LBL_WORK1, VAR_1);
				if (orderflag & ORDER_OTHER) {
					addpgm2(OP_SET, VAR_1, 0);
					addpgm2(OP_WORKUNIQUE, worksetinfo[0].workset, VAR_1);  /* select distinct */
				}
				if (orderflag & ORDER_ORDERBY) {
					addpgm2(OP_SET, VAR_1, 0);
					for (i1 = 0, i2 = (*stmt)->numordercolumns; i1 < i2; i1++) {
						addpgm2(OP_INCR, VAR_1, 1);
						if ((*((*stmt)->orderascflagarray))[i1]) addpgm2(OP_SORTSPEC, (*orderworksetref)[i1], VAR_1);
						else addpgm2(OP_SORTSPECD, (*orderworksetref)[i1], VAR_1);
					}
					addpgm2(OP_WORKSORT, worksetinfo[0].workset, VAR_1);
				}
				addpgm2(OP_WORKGETROWCOUNT, worksetinfo[0].workset, VAR_RECCNT);
				defpgmlabel(LBL_WORK1);
				undefpgmlabel(LBL_WORK1);
			}
			addpgm2(OP_WORKFREE, worksetinfo[0].workset, VAR_RECCNT);
			if (!(*stmt)->forupdateflag) {
				/* set rowid to 0 */
				addpgm2(OP_SET, VAR_1, 0);
				addpgm2(OP_WORKSETROWID, worksetinfo[0].workset, VAR_1);
				/* return code 6, means completed result set w/ 1 or more rows */
				addpgm2(OP_SET, VAR_1, 6);
				addpgm0(OP_TERMINATE);
			}
			else {
				addpgm2(OP_SET, VAR_RECNUM, 0);
				addpgm2(OP_SET, VAR_UPDATE, 0);
				addpgm2(OP_SET, VAR_LASTABS, 0);
				/* return code 7, means 1 or more row result and planexec required */
				addpgm2(OP_SET, VAR_1, 7);
				addpgm0(OP_FINISH);
			}
		}

/* LBL_FUNC_INIT */
		/* init code */
		defpgmlabel(LBL_FUNC_INIT);
		/* initialize workset */
		for (i1 = 0; (size_t) i1 < sizeof(worksetinfo) / sizeof(*worksetinfo); i1++) {
			if (!worksetinfo[i1].plansize) continue;
			addpgm2(OP_SET, VAR_1, worksetinfo[i1].rowcount);
			addpgm2(OP_WORKINIT, worksetinfo[i1].workset, VAR_1);
			if (worksetinfo[i1].rowcount == 1) addpgm1(OP_WORKNEWROW, worksetinfo[i1].workset);
			else if (!worksetinfo[i1].rowcount && (*hplan)->multirowcnt < 255) (*hplan)->multirowcnt++;
		}
		if (orconditionflag) addpgm1(OP_CALL, LBL_FILLWORKSET);
		addpgm1(OP_GOTO, LBL_READFIRST);

		if (!(*stmt)->forupdateflag) {
			/* Note: the 6 scrolling functions and get row count are externally supported */
			/* get next row code */
			defpgmlabel(LBL_FUNC_GETNEXT);
			addpgm2(OP_SET, VAR_1, -1);
			addpgm0(OP_TERMINATE);
			/* get prev row code */
			defpgmlabel(LBL_FUNC_GETPREV);
			addpgm2(OP_SET, VAR_1, -1);
			addpgm0(OP_TERMINATE);
			/* get first row code */
			defpgmlabel(LBL_FUNC_GETFIRST);
			addpgm2(OP_SET, VAR_1, -1);
			addpgm0(OP_TERMINATE);
			/* get last row code */
			defpgmlabel(LBL_FUNC_GETLAST);
			addpgm2(OP_SET, VAR_1, -1);
			addpgm0(OP_TERMINATE);
			/* get absolute row code */
			defpgmlabel(LBL_FUNC_GETABS);
			addpgm2(OP_SET, VAR_1, -1);
			addpgm0(OP_TERMINATE);
			/* get relative row code */
			defpgmlabel(LBL_FUNC_GETREL);
			addpgm2(OP_SET, VAR_1, -1);
			addpgm0(OP_TERMINATE);
			/* get row count code */
			defpgmlabel(LBL_FUNC_GETROWCNT);
			addpgm2(OP_SET, VAR_1, -1);
			addpgm0(OP_TERMINATE);
		}
		else {
/* LBL_RETURN5 */
			defpgmlabel(LBL_RETURN5);
			addpgm2(OP_SET, VAR_UPDATE, 0);
			addpgm2(OP_SET, VAR_LASTABS, 0);
			/* return code 5, BOF or EOF */ 
			addpgm2(OP_SET, VAR_1, 5);
			addpgm0(OP_FINISH);

/* LBL_RETURN7 */
			defpgmlabel(LBL_RETURN7);
			/* return code 7, means multi row result and planexec required */
			addpgm2(OP_SET, VAR_1, 7);
			addpgm0(OP_FINISH);
			
/* LBL_WORK1 */
			defpgmlabel(LBL_WORK1);
			addpgm2(OP_GOTOIFNEG, pgmcount + 6, VAR_2);
			addpgm3(OP_SUB, VAR_RECCNT, VAR_RECNUM, VAR_1);
			addpgm2(OP_GOTOIFNEG, LBL_RETURN5, VAR_1);
			addpgm2(OP_SET, VAR_FORWARD, 1);
			addpgm2(OP_GOTOIFZERO, LBL_WORK3, VAR_RECNUM);
			addpgm1(OP_GOTO, pgmcount + 5);
			addpgm2(OP_GOTOIFZERO, LBL_RETURN5, VAR_RECNUM);
			addpgm2(OP_SET, VAR_FORWARD, -1);
			addpgm3(OP_SUB, VAR_RECCNT, VAR_RECNUM, VAR_1);
			addpgm2(OP_GOTOIFNEG, LBL_WORK3, VAR_1);
			addpgm1(OP_UNLOCK, 1);
/* LBL_WORK3 */
			defpgmlabel(LBL_WORK3);
			/* get next row */
			addpgm3(OP_ADD, VAR_RECNUM, VAR_FORWARD, VAR_RECNUM);
			addpgm2(OP_GOTOIFZERO, LBL_RETURN5, VAR_RECNUM);
			addpgm3(OP_SUB, VAR_RECCNT, VAR_RECNUM, VAR_1);
			addpgm2(OP_GOTOIFNEG, LBL_RETURN5, VAR_1);
/* LBL_WORK2 */
			defpgmlabel(LBL_WORK2);
			addpgm2(OP_WORKSETROWID, worksetinfo[0].workset, VAR_RECNUM);
			addpgm2(OP_COLMOVE, gettabcolref(worksetinfo[0].workset, worksetinfo[0].planresult + 1), gettabcolref(TABREF_WORKVAR, 1));  /* establish size of temp variable */
			addpgm2(OP_COLBINSET, gettabcolref(TABREF_WORKVAR, 1), '9');
			addpgm3(OP_COLCOMPARE, gettabcolref(worksetinfo[0].workset, worksetinfo[0].planresult + 1), gettabcolref(TABREF_WORKVAR, 1), VAR_1);
			addpgm2(OP_GOTOIFZERO, LBL_WORK3, VAR_1);
			/* row is not deleted */
			addpgm3(OP_READPOS, 1, gettabcolref(worksetinfo[0].workset, worksetinfo[0].planresult + 1), VAR_1);
			addpgm2(OP_GOTOIFTRUE, pgmcount + 3, VAR_1);
			/* record has been deleted */
			addpgm2(OP_COLMOVE, gettabcolref(TABREF_WORKVAR, 1), gettabcolref(worksetinfo[0].workset, worksetinfo[0].planresult + 1));
			addpgm1(OP_GOTO, LBL_WORK3);
			/* check if record still meets selection criteria */
			if ((*stmt)->joinonconditions[0] != NULL || (*stmt)->where != NULL) {
				/* requires less labelref space to put label before genlexpcode even though it is not as clean looking */ 
				addpgm1(OP_GOTO, pgmcount + 3);
				defpgmlabel(LBL_WORK4);
				/* record does not meet the selection criteria */
				addpgm2(OP_COLMOVE, gettabcolref(TABREF_WORKVAR, 1), gettabcolref(worksetinfo[0].workset, worksetinfo[0].planresult + 1));
				addpgm1(OP_GOTO, LBL_WORK3);
				if ((*stmt)->joinonconditions[0] != NULL) {
					swhere = (*stmt)->joinonconditions[0];
					genlexpcode((*swhere)->numentries - 1, hplan, NULL, LBL_WORK4);  /* generate expression code */
				}
				if ((*stmt)->where != NULL) {
					swhere = (*stmt)->where;
					genlexpcode((*swhere)->numentries - 1, hplan, NULL, LBL_WORK4);  /* generate expression code */
				}
				undefpgmlabel(LBL_WORK4);
			}
			addpgm3(OP_ADD, VAR_LASTABS, VAR_FORWARD, VAR_LASTABS);
#if 0
/*** CODE: THIS SHOULD PROBABLY NOT BE REQUIRED ***/
			addpgm2(OP_GOTOIFNOTZERO, pgmcount + 2, VAR_LASTABS);
			/* prevent lastabs from becoming opposite (neg/pos) of what it was */
			addpgm3(OP_SUB, VAR_LASTABS, VAR_FORWARD, VAR_LASTABS);
#endif
			addpgm3(OP_SUB, VAR_2, VAR_FORWARD, VAR_2);
			addpgm2(OP_GOTOIFZERO, pgmcount + 3, VAR_2);
			/* not done, get next record */
			addpgm1(OP_UNLOCK, 1);
			addpgm1(OP_GOTO, LBL_WORK3);
			addpgm1(OP_CALL, LBL_MOVECOLUMNS);
			addpgm2(OP_SET, VAR_UPDATE, 1);
			addpgm1(OP_GOTO, LBL_RETURN7);
			undefpgmlabel(LBL_WORK3);

/* LBL_FUNC_GETNEXT */
			/* func_getnext entry point */
			defpgmlabel(LBL_FUNC_GETNEXT);
			addpgm2(OP_SET, VAR_2, 1);
			addpgm1(OP_GOTO, LBL_WORK1);

/* LBL_FUNC_GETPREV */
			/* func_getprev entry point */
			defpgmlabel(LBL_FUNC_GETPREV);
			addpgm2(OP_SET, VAR_2, -1);
			addpgm1(OP_GOTO, LBL_WORK1);

/* LBL_FUNC_GETFIRST */
			/* func_getfirst entry point */
			defpgmlabel(LBL_FUNC_GETFIRST);
			addpgm2(OP_GOTOIFZERO, pgmcount + 4, VAR_RECNUM);
			addpgm3(OP_SUB, VAR_RECCNT, VAR_RECNUM, VAR_1);
			addpgm2(OP_GOTOIFNEG, pgmcount + 2, VAR_1);
			addpgm1(OP_UNLOCK, 1);
			addpgm2(OP_SET, VAR_RECNUM, 0);
			addpgm2(OP_SET, VAR_LASTABS, 0);
			addpgm2(OP_SET, VAR_2, 1);
			addpgm1(OP_GOTO, LBL_WORK1);

/* LBL_FUNC_GETLAST */
			/* func_getlast entry point */
			defpgmlabel(LBL_FUNC_GETLAST);
			addpgm2(OP_GOTOIFZERO, pgmcount + 4, VAR_RECNUM);
			addpgm3(OP_SUB, VAR_RECCNT, VAR_RECNUM, VAR_1);
			addpgm2(OP_GOTOIFNEG, pgmcount + 2, VAR_1);
			addpgm1(OP_UNLOCK, 1);
			addpgm2(OP_MOVE, VAR_RECCNT, VAR_RECNUM);
			addpgm2(OP_INCR, VAR_RECNUM, 1);
			addpgm2(OP_SET, VAR_LASTABS, 0);
			addpgm2(OP_SET, VAR_2, -1);
			addpgm1(OP_GOTO, LBL_WORK1);

/* LBL_FUNC_GETABS */
			/* func_getabs entry point */
			defpgmlabel(LBL_FUNC_GETABS);
			addpgm2(OP_GOTOIFZERO, pgmcount + 4, VAR_RECNUM);
			addpgm3(OP_SUB, VAR_RECCNT, VAR_RECNUM, VAR_1);
			addpgm2(OP_GOTOIFNEG, pgmcount + 2, VAR_1);
			addpgm1(OP_UNLOCK, 1);
			addpgm2(OP_GOTOIFNOTZERO, pgmcount + 3, VAR_2);
			/* position before first record */
			addpgm2(OP_SET, VAR_RECNUM, 0);
			addpgm1(OP_GOTO, LBL_RETURN5);
			addpgm3(OP_SUB, VAR_RECCNT, VAR_2, VAR_1);
			addpgm2(OP_GOTOIFNOTNEG, pgmcount + 4, VAR_1);
			/* position after last record */
			addpgm2(OP_MOVE, VAR_RECCNT, VAR_RECNUM);
			addpgm2(OP_INCR, VAR_RECNUM, 1);
			addpgm1(OP_GOTO, LBL_RETURN5);
			addpgm2(OP_GOTOIFNEG, LBL_WORK3, VAR_2);
			/* absolute from beginning */
			addpgm2(OP_GOTOIFPOS, pgmcount + 4, VAR_LASTABS);
			/* read from beginning of workset */
			addpgm2(OP_SET, VAR_RECNUM, 0);
			addpgm2(OP_SET, VAR_LASTABS, 0);
			addpgm1(OP_GOTO, LBL_WORK1);
			/* use last absolute to convert into relative */
			addpgm3(OP_SUB, VAR_2, VAR_LASTABS, VAR_2);
			addpgm2(OP_GOTOIFNOTZERO, LBL_WORK1, VAR_2);
			/* re-read last row */
			addpgm2(OP_SET, VAR_2, 1);
			addpgm2(OP_SET, VAR_FORWARD, 1);
			addpgm2(OP_INCR, VAR_LASTABS, -1);
			addpgm1(OP_GOTO, LBL_WORK2);
/* LBL_WORK3 */
			defpgmlabel(LBL_WORK3);
			undefpgmlabel(LBL_WORK3);
			/* absolute from end */
			addpgm2(OP_GOTOIFNEG, pgmcount + 5, VAR_LASTABS);
			/* read from end of workset */
			addpgm2(OP_MOVE, VAR_RECCNT, VAR_RECNUM);
			addpgm2(OP_INCR, VAR_RECNUM, 1);
			addpgm2(OP_SET, VAR_LASTABS, 0);
			addpgm1(OP_GOTO, LBL_WORK1);
			/* use last absolute to convert into relative */
			addpgm3(OP_SUB, VAR_2, VAR_LASTABS, VAR_2);
			addpgm2(OP_GOTOIFNOTZERO, LBL_WORK1, VAR_2);
			/* re-read last row */
			addpgm2(OP_SET, VAR_2, -1);
			addpgm2(OP_SET, VAR_FORWARD, -1);
			addpgm2(OP_INCR, VAR_LASTABS, 1);
			addpgm1(OP_GOTO, LBL_WORK2);
			
/* LBL_FUNC_GETREL */
			/* func_getrel entry point */
			defpgmlabel(LBL_FUNC_GETREL);
			addpgm2(OP_GOTOIFNOTZERO, LBL_WORK1, VAR_2);
			/* re-read last row */
			addpgm2(OP_GOTOIFZERO, pgmcount + 4, VAR_RECNUM);
			addpgm3(OP_SUB, VAR_RECCNT, VAR_RECNUM, VAR_1);
			addpgm2(OP_GOTOIFNEG, pgmcount + 2, VAR_1);
			addpgm1(OP_UNLOCK, 1);
			addpgm2(OP_SET, VAR_2, 1);
			addpgm2(OP_SET, VAR_FORWARD, 1);
			addpgm2(OP_INCR, VAR_LASTABS, -1);
			addpgm1(OP_GOTO, LBL_WORK2);

			/* done with return labels */
			undefpgmlabel(LBL_RETURN5);
			undefpgmlabel(LBL_RETURN7);
			undefpgmlabel(LBL_WORK1);
			undefpgmlabel(LBL_WORK2);

			/* get row count entry point */
/* LBL_FUNC_GETROWCNT */
			defpgmlabel(LBL_FUNC_GETROWCNT);
			addpgm2(OP_MOVE, VAR_RECCNT, VAR_2);
			addpgm2(OP_SET, VAR_1, 1);
			addpgm0(OP_FINISH);

/* LBL_FUNC_UPDATE */
			/* update positioned code */
			defpgmlabel(LBL_FUNC_UPDATE);
			addpgm2(OP_GOTOIFNOTZERO, pgmcount + 3, VAR_UPDATE);
			addpgm2(OP_SET, VAR_2, 0);  /* set update count to 0 */
			addpgm1(OP_GOTO, pgmcount + 5);
			addpgm1(OP_CALL, LBL_MOVEUPDATE);
			addpgm1(OP_UPDATE, 1);
			/* remove from static workset if no longer meets selection criteria */
			if ((*stmt)->joinonconditions[0] != NULL || (*stmt)->where != NULL) {
				/* requires less labelref space to put label before genlexpcode even though it is not as clean looking */ 
				addpgm1(OP_GOTO, pgmcount + 3);
				defpgmlabel(LBL_WORK1);
				addpgm2(OP_COLBINSET, gettabcolref(worksetinfo[0].workset, worksetinfo[0].planresult + 1), '9');
				addpgm1(OP_GOTO, LBL_WORK2);
				if ((*stmt)->joinonconditions[0] != NULL) {
					swhere = (*stmt)->joinonconditions[0];
					genlexpcode((*swhere)->numentries - 1, hplan, NULL, LBL_WORK1);  /* generate expression code */
				}
				if ((*stmt)->where != NULL) {
					swhere = (*stmt)->where;
					genlexpcode((*swhere)->numentries - 1, hplan, NULL, LBL_WORK1);  /* generate expression code */
				}
				undefpgmlabel(LBL_WORK1);
				defpgmlabel(LBL_WORK2);
				undefpgmlabel(LBL_WORK2);
			}
			addpgm2(OP_SET, VAR_UPDATE, 0);
			addpgm2(OP_SET, VAR_2, 1);  /* set update count to 1 */
			addpgm2(OP_SET, VAR_1, 1);  /* return code 1, means count in variable 2 */
			addpgm0(OP_FINISH);

/* LBL_FUNC_DELETE */
			/* delete positioned code */
			defpgmlabel(LBL_FUNC_DELETE);
			addpgm2(OP_GOTOIFNOTZERO, pgmcount + 3, VAR_UPDATE);
			addpgm2(OP_SET, VAR_2, 0);  /* set delete count to 0 */
			addpgm1(OP_GOTO, pgmcount + 5);
			addpgm1(OP_DELETE, 1);
			addpgm2(OP_COLBINSET, gettabcolref(worksetinfo[0].workset, worksetinfo[0].planresult + 1), '9');
			addpgm2(OP_SET, VAR_UPDATE, 0);
			addpgm2(OP_SET, VAR_2, 1);  /* set delete count to 1 */
			addpgm2(OP_SET, VAR_1, 1);  /* return code 1, means count in variable 2 */
			addpgm0(OP_FINISH);

/* LBL_MOVEUPDATE */
			/* update move column label & return */
			defpgmlabel(LBL_MOVEUPDATE);
			addpgm0(OP_RETURN);
		}
	}

	if (execerror) goto buildselectplanerror;
	if (labelrefcount) {
		sqlerrnummsg(SQLERR_INTERNAL, "unresolved label references", NULL);
		goto buildselectplanerror;
	}
#if FS_DEBUGCODE
	if (fsflags & FSFLAGS_DEBUG3) for (i1 = 0; i1 < pgmcount; i1++) debugcode("CODE", i1, &((*hpgm)[i1]));
#endif
	memfree((UCHAR **) orderworksetref);

	(*hplan)->pgm = hpgm;
	(*hplan)->literals = literals;
	(*hplan)->numvars = varcount;
	(*hplan)->pgmcount = pgmcount;
	/* transfer the correlation (alias) table from statement to plan */
	(*hplan)->corrtable = (*stmt)->corrtable;
	(*stmt)->corrtable = NULL;
	*phplan = hplan;
	return 0;

buildselectplanerror:
	memfree((UCHAR **) orderworksetref);
	memfree((UCHAR **)(*hplan)->worksetcolrefinfo);
	memfree((UCHAR **)(*hplan)->colrefarray);
	memfree(literals);
	memfree((UCHAR **) hpgm);
	memfree((UCHAR **) hplan);
	return -1;
}

/* build the plan program from the statement */
INT buildinsertplan(S_INSERT **stmt, HPLAN *phplan)
{
	INT i1, i2, tabrefcolnum;
	HPLAN hplan;
	S_VALUE **svalue;

	hplan = (HPLAN) memalloc(sizeof(PLAN), MEMFLAGS_ZEROFILL);
	hpgm = (PCODE **) memalloc(100 * sizeof(PCODE), 0);
	literals = memalloc(300, 0);
	if (hplan == NULL || hpgm == NULL || literals == NULL) {
		memfree(literals);
		memfree((UCHAR **) hpgm);
		memfree((UCHAR **) hplan);
		return sqlerrnum(SQLERR_NOMEM);
	}
	pgmalloc = 100;
	litallocsize = 300;

	execerror = FALSE;
	pgmcount = labeldefcount = labelrefcount = litsize = 0;
	addpgm1(OP_CLEAR, 1);
	if ((*stmt)->numcolumns) {  /* INSERT with column names specified */
		for (i1 = 0, i2 = (*stmt)->numcolumns; i1 < i2; i1++) {
			svalue = (*(*stmt)->valuesarray)[i1];
			addpgm2(OP_COLMOVE, makelitfromsvaldata(svalue), (*(*stmt)->tabcolnumarray)[i1]);
		}			
	}
	else {  /* INSERT without column names specified */
		tabrefcolnum = gettabcolref(1, 1);
		for (i1 = 0, i2 = (*stmt)->numvalues; i1 < i2; i1++) {
			svalue = (*(*stmt)->valuesarray)[i1];
			addpgm2(OP_COLMOVE, makelitfromsvaldata(svalue), tabrefcolnum++);
		}
	}
	addpgm1(OP_WRITE, 1);
	addpgm2(OP_SET, VAR_2, 1);  /* set insert counter to 1 */
	addpgm2(OP_SET, VAR_1, 1);  /* return code 1, means count in variable 2 */
	addpgm0(OP_TERMINATE);
	if (execerror) goto buildinsertplanerror;
#if FS_DEBUGCODE
	if (fsflags & FSFLAGS_DEBUG3) for (i1 = 0; i1 < pgmcount; i1++) debugcode("CODE", i1, &((*hpgm)[i1]));
#endif

	(*hplan)->pgm = hpgm;
	(*hplan)->literals = literals;
	(*hplan)->numvars = 2;
	(*hplan)->pgmcount = pgmcount;
	(*hplan)->numtables = 1;
	(*hplan)->tablenums[0] = (*stmt)->tablenum;
	*phplan = hplan;
	return 0;

buildinsertplanerror:
	memfree(literals);
	memfree((UCHAR **) hpgm);
	memfree((UCHAR **) hplan);
	return -1;
}

/* build the plan program from the statement */
INT buildupdateplan(S_UPDATE **stmt, HPLAN *phplan)
{
	INT i1, i2, i3, indexnum, looplevel, keyflag, orconditionflag;
	INT endoffilelabel, explabel, readnextlabel, readtype, tablenum, tracecnt, varcount;
	USHORT trace[MAX_IDXKEYS];
	HPLAN hplan;
	TABLE *tab1;
	S_SETCOLUMN *col;
	S_WHERE **where;

	execerror = FALSE;
	hplan = (HPLAN) memalloc(sizeof(PLAN), MEMFLAGS_ZEROFILL);
	hpgm = (PCODE **) memalloc(100 * sizeof(PCODE), 0);
	literals = memalloc(300, 0);
	if (hplan == NULL || hpgm == NULL || literals == NULL) {
		memfree(literals);
		memfree((UCHAR **) hpgm);
		memfree((UCHAR **) hplan);
		return sqlerrnum(SQLERR_NOMEM);
	}
	pgmalloc = 100;
	litallocsize = 300;
	pgmcount = litsize = labeldefcount = labelrefcount = 0;

	/* move tables to plan and fill in index info */
	tablenum = (*stmt)->tablenum;
	(*hplan)->numtables = 1;
	(*hplan)->tablenums[0] = tablenum;
	if (connection.updatelock == LOCK_RECORD) (*hplan)->flags |= PLAN_FLAG_FORUPDATE;
	tab1 = tableptr(tablenum);
	if (!(tab1->flags & TABLE_FLAGS_COMPLETE) && maketablecomplete(tablenum))
		goto buildupdateplanerror;

	where = (*stmt)->where;
	orconditionflag = checkorcondition(&where, hplan);
	if (orconditionflag) {
		if (orconditionflag < 0) goto buildupdateplanerror;
		freestmt((S_GENERIC **)(*stmt)->where);
		(*stmt)->where = where;
	}

	looplevel = 0;
	addpgm2(OP_SET, VAR_2, 0);  /* set update counter to 0 */
	if (connection.updatelock == LOCK_FILE) addpgm1(OP_TABLELOCK, 1);
	varcount = 2;
	if (orconditionflag) {
		addpgm2(OP_SET, ++varcount, 0);
		endoffilelabel = LBL_READNEXT + looplevel;
		readnextlabel = LBL_READNEXT + ++looplevel;
		defpgmlabel(readnextlabel);
		addpgm2(OP_WORKGETROWCOUNT, (*hplan)->numtables + (*hplan)->numworksets, VAR_1);
		/* check for EOF */
		addpgm2(OP_INCR, varcount, 1);
		addpgm3(OP_SUB, VAR_1, varcount, VAR_1);
		addpgm2(OP_GOTOIFNEG, endoffilelabel, VAR_1);
		addpgm2(OP_WORKSETROWID, (*hplan)->numtables + (*hplan)->numworksets, varcount);
	}
	bestindex((*stmt)->where, 1, tablenum, &indexnum, &readtype, trace, &tracecnt, NULL, 0, NULL);
	/* load the columns corresponding to the keys */
	keyflag = buildindexkey(hplan, (*stmt)->where, 1, indexnum, readtype, trace, tracecnt, FALSE);
	readnextlabel = endoffilelabel = LBL_READNEXT + looplevel;
	if (keyflag) {  /* using index to establish starting position */
		addpgm3(OP_READBYKEY, 1, indexnum, VAR_1);  /* read by key, tableref, index, result var1 */
		if (readtype == READ_INDEX_EXACT) i1 = 2;
		else if (readtype == READ_INDEX_RANGE) i1 = 3;
		else i1 = 4;
		addpgm2(OP_GOTOIFTRUE, pgmcount + i1, VAR_1);
		/* unsuccessful read */
		if (readtype != READ_INDEX_RANGE) addpgm1(OP_GOTO, endoffilelabel);
	}
	else addpgm2(OP_SETFIRST, 1, indexnum);
	/* set up readnext loop */
	if (readtype != READ_INDEX_EXACT) {
		readnextlabel = LBL_READNEXT + ++looplevel;
		defpgmlabel(readnextlabel);
		addpgm3(OP_READNEXT, 1, indexnum, 1);  /* read next, first table, no index, result var1 */
		addpgm2(OP_GOTOIFFALSE, endoffilelabel, 1);
	}
	/* break out of read loop when key range is exceeded */
	checkindexrange(hplan, (*stmt)->where, 1, indexnum, readtype, trace, tracecnt, FALSE, endoffilelabel);
	tab1 = tableptr(tablenum);
	if (tab1->filtercolumnnum) {
		addpgm3(OP_COLCOMPARE, makelitfromname(tab1->filtervalue), gettabcolref(1, tab1->filtercolumnnum), VAR_1);
		addpgm2(OP_GOTOIFNOTZERO, readnextlabel, VAR_1);  /* skip this row */
	}				
	if ((*stmt)->where != NULL) {
		swhere = (*stmt)->where;
		if (readnextlabel == endoffilelabel) {  /* reduce reference requirements */
			addpgm1(OP_GOTO, pgmcount + 2);
			explabel = LBL_READNEXTEXP + 1;
			defpgmlabel(explabel);
			addpgm1(OP_GOTO, readnextlabel);
		}
		else explabel = readnextlabel;
		genlexpcode((*swhere)->numentries - 1, hplan, NULL, explabel);
		if (readnextlabel == endoffilelabel) undefpgmlabel(explabel);
	}

	/* update the record */
	for (i1 = 0, i2 = (*stmt)->numcolumns; i1 < i2; i1++) {
		col = &(*(*stmt)->setcolumnarray)[i1];
		i3 = gettabcolref(1, col->columnnum);
		if (col->valuep != NULL) addpgm2(OP_COLMOVE, makelitfromsvaldata(col->valuep), i3);
		else genaexpcode(col->expp, hplan, NULL, i3);
	}
	addpgm1(OP_UPDATE, 1);
	addpgm2(OP_INCR, VAR_2, 1);  /* add 1 to update count */
	addpgm1(OP_GOTO, readnextlabel);  /* loop */
/* LBL_READFINISH */
	defpgmlabel(LBL_READFINISH);
	if (connection.updatelock == LOCK_FILE) addpgm1(OP_TABLEUNLOCK, 1);
	addpgm2(OP_SET, VAR_1, 1);  /* return code 1, means count in variable 2 */
	addpgm0(OP_TERMINATE); 
	if (execerror) goto buildupdateplanerror;
	if (labelrefcount) {
		sqlerrnummsg(SQLERR_INTERNAL, "unresolved label references", NULL);
		goto buildupdateplanerror;
	}
#if FS_DEBUGCODE
	if (fsflags & FSFLAGS_DEBUG3) for (i1 = 0; i1 < pgmcount; i1++) debugcode("CODE", i1, &((*hpgm)[i1]));
#endif

	(*hplan)->pgm = hpgm;
	(*hplan)->literals = literals;
	(*hplan)->numvars = varcount;
	(*hplan)->pgmcount = pgmcount;
	*phplan = hplan;
	return 0;

buildupdateplanerror:
	memfree((UCHAR **)(*hplan)->worksetcolrefinfo);
	memfree((UCHAR **)(*hplan)->colrefarray);
	memfree(literals);
	memfree((UCHAR **) hpgm);
	memfree((UCHAR **) hplan);
	return -1;
}

INT builddeleteplan(S_DELETE **stmt, HPLAN *phplan)
{
	int i1, indexnum, looplevel, keyflag, orconditionflag;
	int endoffilelabel, explabel, readnextlabel, readtype, tablenum, tracecnt, varcount;
	USHORT trace[MAX_IDXKEYS];
	HPLAN hplan;
	TABLE *tab1;
	S_WHERE **where;

	execerror = FALSE;
	hplan = (HPLAN) memalloc(sizeof(PLAN), MEMFLAGS_ZEROFILL);
	hpgm = (PCODE **) memalloc(100 * sizeof(PCODE), 0);
	literals = memalloc(300, 0);
	if (hplan == NULL || hpgm == NULL || literals == NULL) {
		memfree(literals);
		memfree((UCHAR **) hpgm);
		memfree((UCHAR **) hplan);
		return sqlerrnum(SQLERR_NOMEM);
	}
	pgmalloc = 100;
	litallocsize = 300;
	pgmcount = litsize = labeldefcount = labelrefcount = 0;

	/* move tables to plan and fill in index info */
	tablenum = (*stmt)->tablenum;
	(*hplan)->numtables = 1;
	(*hplan)->tablenums[0] = tablenum;
	if (connection.deletelock == LOCK_RECORD) (*hplan)->flags |= PLAN_FLAG_FORUPDATE;
	tab1 = tableptr(tablenum);
	if (!(tab1->flags & TABLE_FLAGS_COMPLETE) && maketablecomplete(tablenum))
		goto builddeleteplanerror;

	where = (*stmt)->where;
	orconditionflag = checkorcondition(&where, hplan);
	if (orconditionflag) {
		if (orconditionflag < 0) goto builddeleteplanerror;
		freestmt((S_GENERIC **)(*stmt)->where);
		(*stmt)->where = where;
	}

	looplevel = 0;
	addpgm2(OP_SET, VAR_2, 0);  /* set delete counter to 0 */
	if (connection.deletelock == LOCK_FILE) addpgm1(OP_TABLELOCK, 1);
	varcount = 2;
	if (orconditionflag) {
		addpgm2(OP_SET, ++varcount, 0);
		endoffilelabel = LBL_READNEXT + looplevel;
		readnextlabel = LBL_READNEXT + ++looplevel;
		defpgmlabel(readnextlabel);
		addpgm2(OP_WORKGETROWCOUNT, (*hplan)->numtables + (*hplan)->numworksets, VAR_1);
		/* check for EOF */
		addpgm2(OP_INCR, varcount, 1);
		addpgm3(OP_SUB, VAR_1, varcount, VAR_1);
		addpgm2(OP_GOTOIFNEG, endoffilelabel, VAR_1);
		addpgm2(OP_WORKSETROWID, (*hplan)->numtables + (*hplan)->numworksets, varcount);
	}
	bestindex((*stmt)->where, 1, tablenum, &indexnum, &readtype, trace, &tracecnt, NULL, 0, NULL);
	/* load the columns corresponding to the keys */
	keyflag = buildindexkey(hplan, (*stmt)->where, 1, indexnum, readtype, trace, tracecnt, FALSE);
	readnextlabel = endoffilelabel = LBL_READNEXT + looplevel;
	if (keyflag) {  /* using index to establish starting position */
		addpgm3(OP_READBYKEY, 1, indexnum, VAR_1);  /* read by key, tableref, index, result var1 */
		if (readtype == READ_INDEX_EXACT) i1 = 2;
		else if (readtype == READ_INDEX_RANGE) i1 = 3;
		else i1 = 4;
		addpgm2(OP_GOTOIFTRUE, pgmcount + i1, VAR_1);
		/* unsuccessful read */
		if (readtype != READ_INDEX_RANGE) addpgm1(OP_GOTO, endoffilelabel);
	}
	else addpgm2(OP_SETFIRST, 1, indexnum);
	/* set up readnext loop */
	if (readtype != READ_INDEX_EXACT) {
		readnextlabel = LBL_READNEXT + ++looplevel;
		defpgmlabel(readnextlabel);
		addpgm3(OP_READNEXT, 1, indexnum, 1);  /* read next, first table, no index, result var1 */
		addpgm2(OP_GOTOIFFALSE, endoffilelabel, 1);
	}
	/* break out of read loop when key range is exceeded */
	checkindexrange(hplan, (*stmt)->where, 1, indexnum, readtype, trace, tracecnt, FALSE, endoffilelabel);
	tab1 = tableptr(tablenum);
	if (tab1->filtercolumnnum) {
		addpgm3(OP_COLCOMPARE, makelitfromname(tab1->filtervalue), gettabcolref(1, tab1->filtercolumnnum), VAR_1);
		addpgm2(OP_GOTOIFNOTZERO, readnextlabel, VAR_1);  /* skip this row */
	}				
	if ((*stmt)->where != NULL) {
		swhere = (*stmt)->where;
		if (readnextlabel == endoffilelabel) {  /* reduce reference requirements */
			addpgm1(OP_GOTO, pgmcount + 2);
			explabel = LBL_READNEXTEXP + 1;
			defpgmlabel(explabel);
			addpgm1(OP_GOTO, readnextlabel);
		}
		else explabel = readnextlabel;
		genlexpcode((*swhere)->numentries - 1, hplan, NULL, explabel);
		if (readnextlabel == endoffilelabel) undefpgmlabel(explabel);
	}

	/* delete the record */
	addpgm1(OP_DELETE, 1);
	addpgm2(OP_INCR, VAR_2, 1);  /* add 1 to delete count */
	addpgm1(OP_GOTO, readnextlabel);  /* loop */
/* LBL_READFINISH */
	defpgmlabel(LBL_READFINISH);
	if (connection.deletelock == LOCK_FILE) addpgm1(OP_TABLEUNLOCK, 1);
	addpgm2(OP_SET, VAR_1, 1);  /* return code 1, means count in variable 2 */
	addpgm0(OP_TERMINATE); 
	if (execerror) goto builddeleteplanerror;
	if (labelrefcount) {
		sqlerrnummsg(SQLERR_INTERNAL, "unresolved label references", NULL);
		goto builddeleteplanerror;
	}
#if FS_DEBUGCODE
	if (fsflags & FSFLAGS_DEBUG3) for (i1 = 0; i1 < pgmcount; i1++) debugcode("CODE", i1, &((*hpgm)[i1]));
#endif

	(*hplan)->pgm = hpgm;
	(*hplan)->literals = literals;
	(*hplan)->numvars = varcount;
	(*hplan)->pgmcount = pgmcount;
	*phplan = hplan;
	return 0;

builddeleteplanerror:
	memfree((UCHAR **)(*hplan)->worksetcolrefinfo);
	memfree((UCHAR **)(*hplan)->colrefarray);
	memfree(literals);
	memfree((UCHAR **) hpgm);
	memfree((UCHAR **) hplan);
	return -1;
}

INT buildlockplan(S_LOCK **stmt, HPLAN *phplan)
{
	HPLAN hplan;
#if FS_DEBUGCODE
	INT i1;
#endif

	hplan = (HPLAN) memalloc(sizeof(PLAN), MEMFLAGS_ZEROFILL);
	hpgm = (PCODE **) memalloc(8 * sizeof(PCODE), 0);
	if (hplan == NULL || hpgm == NULL) {
		memfree((UCHAR **) hpgm);
		memfree((UCHAR **) hplan);
		return sqlerrnum(SQLERR_NOMEM);
	}
	pgmalloc = 8;
	pgmcount = 0;

	/* lock the table */
	execerror = FALSE;
	addpgm1(OP_TABLELOCK, 1);
	addpgm2(OP_SET, VAR_1, 0);
	addpgm0(OP_TERMINATE); 

	if (execerror) goto buildlockplanerror;
#if FS_DEBUGCODE
	if (fsflags & FSFLAGS_DEBUG3) for (i1 = 0; i1 < pgmcount; i1++) debugcode("CODE", i1, &((*hpgm)[i1]));
#endif

	(*hplan)->numtables = 1;
	(*hplan)->tablenums[0] = (*stmt)->tablenum;
	(*hplan)->pgm = hpgm;
	(*hplan)->pgmcount = pgmcount;
	(*hplan)->numvars = 2;
	*phplan = hplan;
	return 0;

buildlockplanerror:
	memfree((UCHAR **) hpgm);
	memfree((UCHAR **) hplan);
	return -1;
}

INT buildunlockplan(S_LOCK **stmt, HPLAN *phplan)
{
	HPLAN hplan;
#if FS_DEBUGCODE
	INT i1;
#endif

	hplan = (HPLAN) memalloc(sizeof(PLAN), MEMFLAGS_ZEROFILL);
	hpgm = (PCODE **) memalloc(8 * sizeof(PCODE), 0);
	if (hplan == NULL || hpgm == NULL) {
		memfree((UCHAR **) hpgm);
		memfree((UCHAR **) hplan);
		return sqlerrnum(SQLERR_NOMEM);
	}
	pgmalloc = 8;
	pgmcount = 0;

	/* unlock the table */
	execerror = FALSE;
	addpgm1(OP_TABLEUNLOCK, 1);
	addpgm2(OP_SET, VAR_1, 0);
	addpgm0(OP_TERMINATE); 

	if (execerror) goto buildunlockplanerror;
#if FS_DEBUGCODE
	if (fsflags & FSFLAGS_DEBUG3) for (i1 = 0; i1 < pgmcount; i1++) debugcode("CODE", i1, &((*hpgm)[i1]));
#endif

	(*hplan)->numtables = 1;
	(*hplan)->tablenums[0] = (*stmt)->tablenum;
	(*hplan)->pgm = hpgm;
	(*hplan)->pgmcount = pgmcount;
	(*hplan)->numvars = 2;
	*phplan = hplan;
	return 0;

buildunlockplanerror:
	memfree((UCHAR **) hpgm);
	memfree((UCHAR **) hplan);
	return -1;
}

/* append the set columns to existing plan */
INT appendupdateplan(S_UPDATE **stmt, HPLAN hplan)
{
	INT i1, i2, tabrefcolnum;
	S_SETCOLUMN *col1;

	hpgm = (*hplan)->pgm;
	pgmcount = pgmalloc = (*hplan)->pgmcount;
	if ((*hpgm)[--pgmcount].code != OP_RETURN) return sqlerrnum(SQLERR_INTERNAL);
	literals = (*hplan)->literals;
	execerror = FALSE;
	i2 = (*stmt)->numcolumns;
	/* start out by overwritting the RETURN */
	for (i1 = 0; i1 < i2; i1++) {
		col1 = *(*stmt)->setcolumnarray + i1;
		tabrefcolnum = gettabcolref(1, col1->columnnum);
		if (col1->valuep != NULL) addpgm2(OP_COLMOVE, makelitfromsvaldata(col1->valuep), tabrefcolnum);
		else genaexpcode(col1->expp, hplan, NULL, tabrefcolnum);
	}
	addpgm0(OP_RETURN);
	if (execerror) return -1;
#if 0
	if (fsflags & FSFLAGS_DEBUG3) for (i1 = 0; i1 < pgmcount; i1++) debugcode("CODE", i1, &((*hpgm)[i1]));
#endif
	(*hplan)->pgmcount = pgmcount;
	return 0;
}

/* free the memory of a plan program */
void freeplan(HPLAN hplan)
{
	int i1;
	PLAN *plan;
	OPENFILE *opf1;

	if (hplan == NULL) return;
	plan = *hplan;
	memfree((UCHAR **) plan->colrefarray);
	memfree((UCHAR **) plan->worksetcolrefinfo);
	memfree(plan->literals);
	memfree((UCHAR **) plan->pgm);
	memfree((UCHAR **) plan->savevars);
	freeworkset(plan->worksetnum);
	if (plan->filerefmap != NULL) {
		for (i1 = 0; i1 < plan->numtables; i1++) {  /* flag open files as not in use */
			if ((*plan->filerefmap)[i1] < 1 || (*plan->filerefmap)[i1] > connection.numopenfiles) continue;
			opf1 = openfileptr((*plan->filerefmap)[i1]);
			if ((plan->flags & (PLAN_FLAG_FORUPDATE | PLAN_FLAG_VALIDREC)) == (PLAN_FLAG_FORUPDATE | PLAN_FLAG_VALIDREC)) riounlock(opf1->textfilenum, -1);
			opf1->inuseflag = FALSE;
		}
		memfree((UCHAR **) plan->filerefmap);
	}
	memfree((UCHAR **) plan->corrtable);
	memfree((UCHAR **) hplan);
}

/* fix up and reorder the tables in the select statement */
/* the fixup is to move old style join information in the where to */
/* the appropriate ON condition */
static INT fixupandreordertables(S_SELECT **stmt)
{
	INT i1, i2, i3, i4, andpos, left, lefttabcolnum, numtables, op1, op2;
	INT removeflag, right, righttabcolnum, tableref, wherepos;
	S_WHERE **where1, **where2;
	LEXPENTRY *lexp1, *lexp2;

/*** NOTE: THIS CODE DOES NOT REORDER TABLES TO TAKE ADVANTAGE OF JOINS WHERE ONE TABLE CAN ***/
/***       USE AN INDEX AND THE OTHER CAN NOT. IE, REVERSE TABLES IF LEFT TABLE CAN USE AN ***/
/***       INDEX AND RIGHT TABLE CAN NOT USE AN INDEX ***/ 
/* transfer old style joins and move comparisons as far left (lowest table reference) as possible */

	numtables = (*stmt)->numtables;

	for (tableref = 0; ++tableref <= numtables + 2; ) {
		if (tableref <= numtables) where1 = (*stmt)->joinonconditions[tableref - 1];
		else if (tableref == numtables + 1) where1 = (*stmt)->where;
		else where1 = (*stmt)->having;
		if (where1 == NULL) continue;
		lexp1 = (*where1)->lexpentries;
		wherepos = (*where1)->numentries - 1;
		removeflag = FALSE;
		for ( ; ; ) {
			while (lexp1[wherepos].type == OPAND) wherepos = lexp1[wherepos].left;
			i1 = lexp1[wherepos].type;
			if ((i1 >= OPEQ && i1 <= OPNE) || i1 == OPLIKE) {
				left = lexp1[wherepos].left;
				right = lexp1[wherepos].right;
				lefttabcolnum = lexp1[left].tabcolnum;
				op1 = lexp1[left].type;
				if (op1 == OPEXP) op1 = getexptabcolnum(lexp1[left].expp, &lefttabcolnum, TABCOL_HIGHEST);
				righttabcolnum = lexp1[right].tabcolnum;
				op2 = lexp1[right].type;
				if (op2 == OPEXP) op2 = getexptabcolnum(lexp1[right].expp, &righttabcolnum, TABCOL_HIGHEST);
				i1 = 0;
				if (op1 == OPCOLUMN) {
					if (op2 == OPCOLUMN) {
						i1 = gettabref(lefttabcolnum);
						if (gettabref(righttabcolnum) > i1) i1 = gettabref(righttabcolnum);
					}
					else if (op2 == OPVALUE) i1 = gettabref(lefttabcolnum);
				}
				else if (op2 == OPCOLUMN && op1 == OPVALUE) i1 = gettabref(lexp1[right].tabcolnum);
				if (i1 && i1 != tableref && i1 <= numtables) {  /* move this entry to corresponding table reference */
					where2 = (*stmt)->joinonconditions[i1 - 1];
					if (where2 == NULL) {
						where2 = (S_WHERE **) memalloc(sizeof(S_WHERE) + 2 * sizeof(LEXPENTRY), 0);
						if (where2 == NULL) return sqlerrnum(SQLERR_NOMEM);
						(*stmt)->joinonconditions[i1 - 1] = where2;
						(*where2)->stype = STYPE_WHERE;  /* set structure type */
						(*where2)->numentries = 0;
					}
					else if (memchange((unsigned char **) where2, sizeof(S_WHERE) + ((*where2)->numentries + 3) * sizeof(LEXPENTRY), 0) == -1)
						return sqlerrnum(SQLERR_NOMEM);
					/* memory may have moved */
					lexp1 = &(*where1)->lexpentries[0];
					lexp2 = &(*where2)->lexpentries[0];
					i1 = (*where2)->numentries;
					lexp2[i1] = lexp1[left];
					lexp2[i1].parent = i1 + 2;
					lexp2[i1 + 1] = lexp1[right];
					lexp2[i1 + 1].parent = i1 + 2;
					lexp2[i1 + 2] = lexp1[wherepos];
					lexp2[i1 + 2].left = i1;
					lexp2[i1 + 2].right = i1 + 1;
					if (i1) {
						lexp2[i1 - 1].parent = i1 + 3;
						lexp2[i1 + 2].parent = i1 + 3;
						lexp2[i1 + 3].type = OPAND;
						lexp2[i1 + 3].left = i1 - 1;
						lexp2[i1 + 3].right = i1 + 2;
						lexp2[i1 + 3].parent = -1;
						(*where2)->numentries += 4;
					}
					else {
						lexp2[i1 + 2].parent = -1;
						(*where2)->numentries = 3;
					}
					andpos = lexp1[wherepos].parent;
					if (andpos == -1) {  /* this condition is the only one */ 
						(*where1)->numentries = 0;
						break;
					}
					/* flag blocks being removed for later removal */
					removeflag = TRUE;
					lexp1[left].type = -1;
					lexp1[right].type = -1;
					lexp1[wherepos].type = -1;
					lexp1[andpos].type = -1;
					i1 = lexp1[andpos].parent;
					if (i1 == -1) {  /* opposite child becomes first entry */
						if (lexp1[andpos].left == wherepos) lexp1[lexp1[andpos].right].parent = -1;  /* removing left child */
						else lexp1[lexp1[andpos].left].parent = -1;  /* removing right child */
					}
					else {
						if (lexp1[i1].left == andpos) {  /* collapsing left branch */
							if (lexp1[andpos].left == wherepos) lexp1[i1].left = lexp1[andpos].right;  /* removing left child */
							else wherepos = lexp1[i1].left = lexp1[andpos].left;  /* removing right child */
							lexp1[lexp1[i1].left].parent = i1;
						}
						else {  /* collapsing right branch */
							if (lexp1[andpos].left == wherepos) lexp1[i1].right = lexp1[andpos].right;  /* removing left child */
							else wherepos = lexp1[i1].right = lexp1[andpos].left;  /* removing right child */
							lexp1[lexp1[i1].right].parent = i1;
						}
					}
				}
			}
			for ( ; ; ) {
				i1 = lexp1[wherepos].parent;
				if (i1 == -1) break;
				if (lexp1[i1].left == wherepos) {
					wherepos = lexp1[i1].right;
					break;
				}
				wherepos = i1;
			}
			if (i1 == -1) break;
		}
		if (removeflag) {  /* remove unused entries */
			for (i1 = 0, i2 = (*where1)->numentries; i1 < i2; i1++) {
				if (lexp1[i1].type == -1) continue;
				for (i3 = 0, i4 = lexp1[i1].left; i3 < i4; i3++) if (lexp1[i3].type == -1) lexp1[i1].left--;
				for (i3 = 0, i4 = lexp1[i1].right; i3 < i4; i3++) if (lexp1[i3].type == -1) lexp1[i1].right--;
				for (i3 = 0, i4 = lexp1[i1].parent; i3 < i4; i3++) if (lexp1[i3].type == -1) lexp1[i1].parent--;
			}
			for (i1 = i3 = 0; i1 < i2; i1++)
				if (lexp1[i1].type != -1) {
					if (i1 != i3) lexp1[i3] = lexp1[i1];
					i3++;
				}
			(*where1)->numentries = i3;
/*** CODE: FREE EXCESS MEMORY HERE OR BY CALLER IF THIS BECOMES A ROUTINE (DON'T FOR NOW) ***/
		}
		if (!(*where1)->numentries) {
			memfree((unsigned char **) where1);
			if (tableref <= numtables) (*stmt)->joinonconditions[tableref - 1] = NULL;
			else if (tableref == numtables + 1) (*stmt)->where = NULL;
			else (*stmt)->having = NULL;
		}
	}
	return 0;
}

static INT checkorcondition(S_WHERE ***whereptr, HPLAN hplan)
{
	INT i1, i2, i3, column1count, column2count, indexnum, left, lefttabcolnum, numentries;
	INT numworksets, plancolrefsize, readtype, right, righttabcolnum, tablenum;
	INT wherepos1, wherepos2, worksetbufoffset, worksetref, worksetsize, **plancolrefinfo;
	SHORT columns1[100], columns2[100];
	UCHAR op, optype1[100], optype2[100];
	S_WHERE **where1, **where2;
	LEXPENTRY *lexp1, *lexp2;
	S_AEXP **aexp;
	TABLE *tab1;
	COLUMN *col1;
	COLREF *crf1;
	HCOLREF plancolrefarray;

	/* check for simple consistent 'or' condition to create temporary result set */
	if (*whereptr == NULL) return 0;

	where1 = *whereptr;
	lexp1 = (*where1)->lexpentries;
	wherepos1 = (*where1)->numentries - 1;
	if (lexp1[wherepos1].type != OPOR) return 0;
	column1count = worksetsize = 0;
	for ( ; ; ) {
		while (lexp1[wherepos1].type == OPOR) wherepos1 = lexp1[wherepos1].left;
		worksetsize++;
		column2count = 0;
		for (wherepos2 = wherepos1; ; ) {
			while (lexp1[wherepos2].type == OPAND) wherepos2 = lexp1[wherepos2].left;
			if (lexp1[wherepos2].type != OPEQ) return 0;
			left = lexp1[wherepos2].left;
			right = lexp1[wherepos2].right;
			lefttabcolnum = lexp1[left].tabcolnum;
			i2 = lexp1[left].type;
			if (i2 == OPEXP) i2 = getexptabcolnum(lexp1[left].expp, &lefttabcolnum, TABCOL_UPPER | TABCOL_LOWER);
			righttabcolnum = lexp1[right].tabcolnum;
			i3 = lexp1[right].type;
			if (i3 == OPEXP) i3 = getexptabcolnum(lexp1[right].expp, &righttabcolnum, TABCOL_UPPER | TABCOL_LOWER);
			if (i2 == OPCOLUMN || EXPOPERATOR(i2)) {
				if (i3 != OPVALUE || gettabref(lefttabcolnum) != 1) return 0;
				op = (UCHAR) i2;
				i2 = getcolnum(lefttabcolnum);
			}
			else if (i3 == OPCOLUMN || EXPOPERATOR(i3)) {
				if (i2 != OPVALUE || gettabref(righttabcolnum) != 1) return 0;
				op = (UCHAR) i3;
				i2 = getcolnum(righttabcolnum);
			}
			else return 0;
			if (column2count == sizeof(columns2) / sizeof(*columns2)) return 0;
			for (i1 = 0; i1 < column2count; i1++) {
				if (i2 < columns2[i1]) {
					memmove(columns2 + i1 + 1, columns2 + i1, (column2count - i1) * sizeof(*columns2));
					memmove(optype2 + i1 + 1, optype2 + i1, (column2count - i1) * sizeof(*optype2));
					break;
				}
				if (i2 == columns2[i1]) return 0;
			}
			columns2[i1] = i2;
			optype2[i1] = op;
			column2count++;
			for ( ; ; ) {
				if (wherepos2 == wherepos1) break;
				i1 = lexp1[wherepos2].parent;
				if (lexp1[i1].left == wherepos2) {
					wherepos2 = lexp1[i1].right;
					break;
				}
				wherepos2 = i1;
			}
			if (wherepos2 == wherepos1) break;
		}
		if (!column1count) {
			for (column1count = 0; column1count < column2count; column1count++) {
				columns1[column1count] = columns2[column1count];
				optype1[column1count] = optype2[column1count];
			}
		}
		else {
			if (column1count != column2count) return 0;
			for (i1 = 0; i1 < column1count; i1++)
				if (columns1[i1] != columns2[i1] || optype1[i1] != optype2[i1]) return 0;
		}
		for ( ; ; ) {
			i1 = lexp1[wherepos1].parent;
			if (i1 == -1) break;
			if (lexp1[i1].left == wherepos1) {
				wherepos1 = lexp1[i1].right;
				break;
			}
			wherepos1 = i1;
		}
		if (i1 == -1) break;
	}
	if (!worksetsize) return 0;

	/* calculate worksetref */
	worksetref = (*hplan)->numtables + (*hplan)->numworksets + 1;

	/* build where condition based on single OR condition */
	where2 = (S_WHERE **) memalloc(sizeof(S_WHERE) + (column1count * 4 - 2) * sizeof(LEXPENTRY), MEMFLAGS_ZEROFILL);
	if (where2 == NULL) return sqlerrnum(SQLERR_NOMEM);
	(*where2)->stype = STYPE_WHERE;  /* set structure type */
	lexp2 = (*where2)->lexpentries;
	numentries = 0;
	for (i1 = 0; i1 < column1count; i1++) {
		if (optype1[i1] == OPCOLUMN) {
			lexp2[numentries].type = OPCOLUMN;
			lexp2[numentries].tabcolnum = gettabcolref(1, columns1[i1]);
		}
		else {
			aexp = (S_AEXP **) memalloc(sizeof(S_AEXP) + 1 * sizeof(AEXPENTRY), MEMFLAGS_ZEROFILL);
			if (aexp == NULL) {
				(*where2)->numentries = numentries;
				freestmt((S_GENERIC **) where2);
				return sqlerrnum(SQLERR_NOMEM);
			}
			(*aexp)->stype = STYPE_AEXP;  /* set structure type */
			(*aexp)->numentries = 2;
			(*aexp)->aexpentries[0].type = OPCOLUMN;
			(*aexp)->aexpentries[0].tabcolnum = gettabcolref(1, columns1[i1]);
			(*aexp)->aexpentries[1].type = optype1[i1];
			lexp2 = (*where2)->lexpentries;  /* restore */
			lexp2[numentries].type = OPEXP;
			lexp2[numentries].expp = aexp;
		}
		lexp2[numentries].left = -1;
		lexp2[numentries].right = -1;
		lexp2[numentries].parent = numentries + 2;
		numentries++;
		lexp2[numentries].type = OPCOLUMN;
		lexp2[numentries].left = -1;
		lexp2[numentries].right = -1;
		lexp2[numentries].parent = numentries + 1;
		lexp2[numentries++].tabcolnum = gettabcolref(worksetref, i1 + 1);
		lexp2[numentries].type = OPEQ;
		lexp2[numentries].left = numentries - 2;
		lexp2[numentries].right = numentries - 1;
		if (!i1) {
			if (i1 + 1 < column1count) lexp2[numentries].parent = numentries + 4;
			else lexp2[numentries].parent = -1;
		}
		else lexp2[numentries].parent = numentries + 1;
		numentries++;
		if (i1) {
			lexp2[numentries].type = OPAND;
			lexp2[numentries].left = numentries - 4;
			lexp2[numentries].right = numentries - 1;
			if (i1 + 1 < column1count) lexp2[numentries].parent = numentries + 4;
			else lexp2[numentries].parent = -1;
			numentries++;
		}
	}
	(*where2)->numentries = numentries;

	/* test if where condition can use index */
	bestindex(where2, 1, (*hplan)->tablenums[0], &indexnum, &readtype, NULL, NULL, NULL, 0, NULL);
/*** CODE: MAY NEED TO CHANGE THIS TO READ_INDEX_EXACT AND MAYBE READ_INDEX_EXACT_DUP ***/
/***       IF MULTIPLE READ_INDEX_RANGE OR READ_AIMDEX_RANGE READS ARE SLOWER THAN STRAIGHT SEQUENTIAL READ. ***/
/***       MAYBE BASE DECISION ON WORKSETSIZE ***/
	if (readtype == READ_INDEX_NONE) {
		freestmt((S_GENERIC **) where2);
		return 0;
	}

	numworksets = (*hplan)->numworksets;
	if (!numworksets) {
		plancolrefarray = (HCOLREF) memalloc(column1count * sizeof(COLREF), MEMFLAGS_ZEROFILL);
		plancolrefinfo = (INT **) memalloc(3 * sizeof(INT), 0);
		if (plancolrefarray == NULL || plancolrefinfo == NULL) {
			memfree((UCHAR **) plancolrefinfo);
			memfree((UCHAR **) plancolrefarray);
			freestmt((S_GENERIC **) where2);
			return sqlerrnum(SQLERR_NOMEM);
		}
		(*hplan)->colrefarray = plancolrefarray;
		(*hplan)->worksetcolrefinfo = plancolrefinfo;
		plancolrefsize = 0;
	}
	else {
		plancolrefarray = (*hplan)->colrefarray;
		plancolrefinfo = (*hplan)->worksetcolrefinfo;
		plancolrefsize = (*plancolrefinfo)[(numworksets - 1) * 3] + (*plancolrefinfo)[(numworksets - 1) * 3 + 2];
		if (memchange((UCHAR **) plancolrefarray, (plancolrefsize + column1count) * sizeof(COLREF), MEMFLAGS_ZEROFILL) == -1 || 
			memchange((UCHAR **) plancolrefinfo, (numworksets + 1) * 3 * sizeof(INT), 0) == -1) {
			freestmt((S_GENERIC **) where2);
			return sqlerrnum(SQLERR_NOMEM);
		}
	}
	(*plancolrefinfo)[numworksets * 3] = plancolrefsize;
	(*plancolrefinfo)[numworksets * 3 + 1] = column1count;
	(*plancolrefinfo)[numworksets * 3 + 2] = column1count;
	(*hplan)->numworksets++;
	(*hplan)->flags |= PLAN_FLAG_SORT;

	worksetbufoffset = 0;
	tablenum = (*hplan)->tablenums[0];
	tab1 = tableptr(tablenum);
	for (i1 = 0; i1 < column1count; i1++) {
		col1 = columnptr(tab1, columns1[i1]);
		crf1 = *plancolrefarray + plancolrefsize;
		crf1->tabcolnum = gettabcolref(1, columns1[i1]);
		/* crf1->tablenum = (SHORT) tablenum; */
		/* crf1->columnnum = (SHORT) columns1[i1]; */
		crf1->type = col1->type;
		crf1->length = col1->length;
		crf1->scale = col1->scale;
		crf1->offset = worksetbufoffset;
		worksetbufoffset += crf1->length;
		plancolrefsize++;
	}

	lexp1 = (*where1)->lexpentries;  /* restore */
	addpgm2(OP_SET, VAR_1, worksetsize);
	addpgm2(OP_WORKINIT, worksetref, VAR_1);
	for ( ; ; ) {
		while (lexp1[wherepos1].type == OPOR) wherepos1 = lexp1[wherepos1].left;
		addpgm1(OP_WORKNEWROW, worksetref);
		for (wherepos2 = wherepos1; ; ) {
			while (lexp1[wherepos2].type == OPAND) wherepos2 = lexp1[wherepos2].left;
			left = lexp1[wherepos2].left;
			right = lexp1[wherepos2].right;
			lefttabcolnum = lexp1[left].tabcolnum;
			i2 = lexp1[left].type;
			if (i2 == OPEXP) i2 = getexptabcolnum(lexp1[left].expp, &lefttabcolnum, TABCOL_UPPER | TABCOL_LOWER);
			if (i2 == OPCOLUMN || EXPOPERATOR(i2)) {
				i2 = getcolnum(lefttabcolnum);
				i3 = right;
			}
			else {
				righttabcolnum = lexp1[right].tabcolnum;
				i2 = lexp1[right].type;
				if (i2 == OPEXP) i2 = getexptabcolnum(lexp1[right].expp, &righttabcolnum, TABCOL_UPPER | TABCOL_LOWER);
				i2 = getcolnum(righttabcolnum);
				i3 = left;
			}
			for (i1 = 0; i1 < column1count && i2 != columns1[i1]; i1++);
			if (lexp1[i3].type == OPEXP) genaexpcode(lexp1[i3].expp, hplan, NULL, gettabcolref(worksetref, i1 + 1));
			else addpgm2(OP_COLMOVE, makelitfromsvaldata(lexp1[i3].valuep), gettabcolref(worksetref, i1 + 1));
			lexp1 = (*where1)->lexpentries;  /* restore */
			for ( ; ; ) {
				if (wherepos2 == wherepos1) break;
				i1 = lexp1[wherepos2].parent;
				if (lexp1[i1].left == wherepos2) {
					wherepos2 = lexp1[i1].right;
					break;
				}
				wherepos2 = i1;
			}
			if (wherepos2 == wherepos1) break;
		}
		for ( ; ; ) {
			i1 = lexp1[wherepos1].parent;
			if (i1 == -1) break;
			if (lexp1[i1].left == wherepos1) {
				wherepos1 = lexp1[i1].right;
				break;
			}
			wherepos1 = i1;
		}
		if (i1 == -1) break;
	}
	addpgm2(OP_SET, VAR_1, 0);
	addpgm2(OP_WORKUNIQUE, worksetref, VAR_1);

	*whereptr = where2;
	if (readtype == READ_INDEX_EXACT) return 1;
	return 2;
}

/**
 * indexnum is returned as a ONE-BASED index into the tables index array
 * readtype is returned as one of the READ_INDEX bits defined in this file
 */
static void bestindex(S_WHERE **where, INT tableref, INT tablenum, INT *indexnum, INT *readtype,
		USHORT *trace, INT *tracecnt, INT *orderflag, INT numordercolumns, UINT **orderarray)
{
	INT i1, i2, i3, aimkeytype, checkorderflag, cnt, colref1, colref2, eqflag, index;
	INT isikeytype, keylen, keytype, left, lefttabcolnum, newcolref1, newcolref2, newtype;
	INT nextref, op, op1, op2, right, righttabcolnum, startpos, type, wherepos;
	USHORT tracework[MAX_IDXKEYS];
	TABLE *tab1;
	COLUMN *col1;
	INDEX *idx1;
	LEXPENTRY *lexp;
	COLKEY *colkeys;
	IDXKEY *keys;
	S_VALUE *svalue;

	checkorderflag = (orderflag != NULL && (*orderflag & ORDER_TRYINDEX));
	/* zero out keytypes */
	tab1 = tableptr(tablenum);
	for (i1 = 0; i1 < tab1->numindexes; i1++) {
		idx1 = *tab1->hindexarray + i1;
		if (idx1->hcolkeys == NULL) continue;
		idx1->flags |= INDEX_ALLEQUAL;
		for (colkeys = *idx1->hcolkeys, i2 = 0; i2 < idx1->numcolkeys; colkeys++, i2++)
			colkeys->keytypes = 0;
	}
	if (where != NULL) {  /* fill in type of search for each column */
		lexp = (*where)->lexpentries;
		wherepos = (*where)->numentries - 1;
		for ( ; ; ) {
			while (lexp[wherepos].type == OPAND) wherepos = lexp[wherepos].left;
			op = lexp[wherepos].type;
			if ((op >= OPEQ && op <= OPGE) || op == OPLIKE) {
				left = lexp[wherepos].left;
				right = lexp[wherepos].right;
				lefttabcolnum = lexp[left].tabcolnum;
				op1 = lexp[left].type;
				if (op1 == OPEXP) op1 = getexptabcolnum(lexp[left].expp, &lefttabcolnum, TABCOL_UPPER | TABCOL_LOWER);
				righttabcolnum = lexp[right].tabcolnum;
				op2 = lexp[right].type;
				if (op2 == OPEXP) op2 = getexptabcolnum(lexp[right].expp, &righttabcolnum, TABCOL_UPPER | TABCOL_LOWER);
				i1 = (op1 == OPCOLUMN || EXPOPERATOR(op1)) ? gettabref(lefttabcolnum) : 0;
				i2 = (op2 == OPCOLUMN || EXPOPERATOR(op2)) ? gettabref(righttabcolnum) : 0;
				i3 = 0;
				if (i1 != i2) {
					if (i1 == tableref) {
						if (op2 == OPEXP) {
							op2 = getexptabcolnum(lexp[right].expp, &righttabcolnum, TABCOL_HIGHEST);
							if (tableref == gettabref(righttabcolnum)) op2 = OPEXP;
						}
						if (op2 == OPCOLUMN || op2 == OPVALUE || EXPOPERATOR(op2)) {
							i3 = getcolnum(lefttabcolnum);
						}
					}
					else if (i2 == tableref) {
						if (op1 == OPEXP) {
							op1 = getexptabcolnum(lexp[left].expp, &lefttabcolnum, TABCOL_HIGHEST);
							if (tableref == gettabref(righttabcolnum)) op1 = OPEXP;
						}
						if ((op1 == OPCOLUMN || op1 == OPVALUE || EXPOPERATOR(op1)) && op != OPLIKE) {
							op1 = op2;
							i3 = getcolnum(righttabcolnum);
						}
					}
				}
				if (i3) {
					switch (op) {
					case OPEQ:
						keytype = KEYTYPE_EQ;
						break;
					case OPLT:
						keytype = KEYTYPE_LT;
						break;
					case OPGT:
						keytype = KEYTYPE_GT;
						break;
					case OPLE:
						keytype = KEYTYPE_LE;
						break;
					case OPGE:
						keytype = KEYTYPE_GE;
						break;
					case OPLIKE:
						keytype = aimkeytype = KEYTYPE_LIKE;
						isikeytype = 0;
#if 1
						if (lexp[right].type == OPVALUE) {
							svalue = *lexp[right].valuep;
							if (svalue->length && svalue->data[0] != '%' && svalue->data[0] != '_') isikeytype = KEYTYPE_LIKE;
						}
#else
/*** CODE: DO THIS IF WE WANT TO OPTIMIZE FOR SOMEONE USING LIKE WITH NO PATTERNS ***/
/*** NOTE: THIS CODE HAD ASSUMED ISIKEYTYPE = KEYTYPE_LIKE ***/
						if (lexp[right].type == OPVALUE) {
							svalue = *lexp[right].valuep;
							if (i2 = svalue->length) {
								for (i1 = 0; i1 < i2; i1++) {
									if (svalue->data[i1] == '%' || svalue->data[i1] == '_') break;
									if (svalue->data[i1] == svalue->escape && i1 + 1 < i2 && (svalue->data[i1 + 1] == svalue->escape || svalue->data[i1 + 1] == '%' || svalue->data[i1+ 1] == '_')) i1++;
								}
								if (i1 == i2) aimkeytype = isikeytype = KEYTYPE_LIKE | KEYTYPE_EQ;
								else if (!i1) isikeytype = 0;
							}
							else aimkeytype = isikeytype = 0;
						}
						else if (lexp[right].type == OPCOLUMN) aimkeytype = isikeytype = KEYTYPE_LIKE | KEYTYPE_EQ;
#endif
						break;
					default:
						keytype = 0;
						break;
					}
					for (i1 = 0; i1 < tab1->numindexes; i1++) {
						idx1 = *tab1->hindexarray + i1;
						if (idx1->hcolkeys == NULL) continue;
#if 0
/*** NOTE: this would be better than the below KEYTYPE_INVALID, but getnextsimpleandterm ***/
/***       does not support where col1 = 'ABC' and upper(col1) = 'ABC' because it ***/
/***       will attempt to process the upper(col1) code.  also incorporate found = false ***/
						if (op1 != OPCOLUMN && (idx1->flags & INDEX_DISTINCT)) continue;  /* lower/upper */
#endif
						eqflag = FALSE;
						for (colkeys = *idx1->hcolkeys, i2 = 0; i2 < idx1->numcolkeys; colkeys++, i2++) {
							if (colkeys->colnum == i3) {
								if (op1 != OPCOLUMN && (idx1->flags & INDEX_DISTINCT)) colkeys->keytypes |= KEYTYPE_INVALID;  /* lower/upper */
								else if (keytype == KEYTYPE_LIKE) {
									if (idx1->type == INDEX_TYPE_ISAM) {
										colkeys->keytypes |= isikeytype;
									}
									else colkeys->keytypes |= aimkeytype;
								}
								else {
									colkeys->keytypes |= keytype;
									if (keytype == KEYTYPE_EQ) eqflag = TRUE;
								}
							}
						}
						if (!eqflag) idx1->flags &= ~INDEX_ALLEQUAL;
					}
				}
			}
			for ( ; ; ) {
				i1 = lexp[wherepos].parent;
				if (i1 == -1) break;
				if (lexp[i1].left == wherepos) {
					wherepos = lexp[i1].right;
					break;
				}
				wherepos = i1;
			}
			if (i1 == -1) break;
		}
	}
	else if (!checkorderflag) {
		*indexnum = 0;
		*readtype = READ_INDEX_NONE;
		return;
	}

	colref1 = colref2 = index = 0;
	type = READ_INDEX_NONE << 1;
	for (i1 = 0; i1 < tab1->numindexes; i1++) {
		idx1 = *tab1->hindexarray + i1;
		if (idx1->hcolkeys == NULL) continue;
		colkeys = *idx1->hcolkeys;
		for (nextref = keylen = cnt = 0; nextref != -1; nextref = colkeys[nextref].alt) {
			do {
				tracework[cnt++] = nextref;
				if (colkeys[nextref].keytypes & KEYTYPE_INVALID) colkeys[nextref].keytypes = 0;
				keylen += colkeys[nextref].len;
				nextref = colkeys[nextref].next;
			} while (nextref != -1);

			newtype = READ_INDEX_NONE << 1;
			newcolref1 = newcolref2 = 0;
			if (idx1->type == INDEX_TYPE_ISAM) {
				for ( ; newcolref1 < cnt; newcolref1++)
					if (!(colkeys[tracework[newcolref1]].keytypes & KEYTYPE_EQ)) break;
				if (newcolref1 == cnt && keylen == idx1->keylength) {
					if (idx1->flags & INDEX_DUPS) newtype = READ_INDEX_EXACT_DUP << 1;
					else newtype = READ_INDEX_EXACT << 1;
				}
				else {
					for (newcolref2 = newcolref1; newcolref2 < cnt; newcolref2++)
						if (!(colkeys[tracework[newcolref2]].keytypes & KEYTYPE_EQ) && (!colkeys[tracework[newcolref2]].keytypes || !colkeys[tracework[0]].asciisortflag)) break;
					if (newcolref2) newtype = READ_INDEX_RANGE << 1;
				}
				if ((newtype && (idx1->flags & INDEX_ALLEQUAL)) || newtype == (READ_INDEX_EXACT << 1)) newtype |= READ_INDEX_ALLEQUAL << 1;
				if (checkorderflag) {  /* try to find index that matches sort order */
					for (i2 = 0; i2 < numordercolumns && i2 < cnt; i2++)
						if (gettabcolref(tableref, colkeys[tracework[i2]].colnum) != (*orderarray)[i2] || !colkeys[tracework[i2]].asciisortflag) break;
					if (i2 == numordercolumns) newtype |= 0x01;
				}
				if (newtype == type) {
					if ((newtype & ~(READ_INDEX_ALLEQUAL | 0x01)) == (READ_INDEX_EXACT << 1) || (newtype & ~(READ_INDEX_ALLEQUAL | 0x01)) == (READ_INDEX_EXACT_DUP << 1)) {
						if (newcolref1 < colref1) type = READ_INDEX_NONE << 1;
					}
					else if (newcolref1 > colref1 || (newcolref1 == colref1 && newcolref2 > colref2)) type = READ_INDEX_NONE << 1;
				}
			}
			else if (idx1->type == INDEX_TYPE_AIM) {
				keys = *idx1->hkeys;
				for ( ; newcolref1 < cnt; newcolref1++) {
					if (colkeys[tracework[newcolref1]].keytypes & (KEYTYPE_EQ | KEYTYPE_LIKE)) {
						col1 = columnptr(tab1, colkeys[tracework[newcolref1]].colnum);
						startpos = col1->offset;
						for (i2 = 0; i2 < idx1->numkeys; i2++) {
							if (startpos >= (INT) keys[i2].pos && startpos < (INT)(keys[i2].pos + keys[i2].len)) {
								newtype = READ_INDEX_AIMDEX << 1;
								newcolref2++;
								break;
							}
						}
					}
				}
				if (newtype && (idx1->flags & INDEX_ALLEQUAL) && type <= (READ_INDEX_AIMDEX << 1)) newtype |= READ_INDEX_ALLEQUAL << 1;
				if (newtype == type && newcolref2 > colref2) type = READ_INDEX_NONE << 1;
			}
			if (newtype > type) {
				type = newtype;
				colref1 = newcolref1;
				colref2 = newcolref2;
				index = i1 + 1;
				if (trace != NULL) for (i2 = 0; i2 < cnt; i2++) trace[i2] = tracework[i2];
				if (tracecnt != NULL) *tracecnt = cnt;
			}

			/* back up to next alternate */
			do keylen -= colkeys[tracework[--cnt]].len;
			while (cnt && colkeys[tracework[cnt]].alt == -1);
			nextref = tracework[cnt];
		}
	}
	*indexnum = index;
	*readtype = (type >> 1) & ~READ_INDEX_ALLEQUAL;
	if (type & 0x01) {
		if (*orderflag & ORDER_OTHER) *orderflag &= ~ORDER_OTHER;
		else *orderflag &= ~ORDER_ORDERBY;
	}
}

/**
 * tableref is ONE-BASED
 * indexnum is ONE-BASED
 */
static INT buildindexkey(HPLAN hplan, S_WHERE **clause, INT tableref, INT indexnum, INT readtype, USHORT *trace, INT tracecnt, INT dynamickeyflag)
{
	INT i1, colref, escref, firstflag, indexcolref, keyflag, keymask, keytype;
	TABLE *tab1;
	INDEX *idx1;
	HCOLKEY hcolkeys;

	if (clause == NULL || !indexnum) return FALSE;

	keyflag = FALSE;
/*** NOTE: THIS CODE DOES NOT HANDLE THE UNUSUAL CASE OF C1 = F AND C1 > K, WHICH WILL ***/
/***       CAUSE ALL OF THE RECORDS WITH C1 = F TO BE READ EVEN THOUGH CONDITION CAN ***/
/***       NOT BE SATISFIED.  THIS IS BECAUSE THE CODE ONLY FOCUSES ON THE '=' CASES. ***/
/***       ANYONE, DOING THE ABOVE CASE PROBABLY DESERVES FOR IT TO RUN SLOW ***/
	i1 = 0;
	if (readtype == READ_INDEX_EXACT || readtype == READ_INDEX_EXACT_DUP) keymask = KEYTYPE_EQ;
	else if (readtype == READ_INDEX_AIMDEX) keymask = KEYTYPE_EQ | KEYTYPE_LIKE;
	else if (dynamickeyflag) {
		i1 = VAR_FORWARD;
		keymask = KEYTYPE_EQ | KEYTYPE_GE | KEYTYPE_GT | KEYTYPE_LE | KEYTYPE_LT | KEYTYPE_LIKE;
		addpgm2(OP_SET, VAR_1, 0);
	}
	else keymask = KEYTYPE_EQ | KEYTYPE_GE | KEYTYPE_GT | KEYTYPE_LIKE;
	addpgm3(OP_KEYINIT, tableref, indexnum, i1);  /* initialize aim keys and set table and index number */
	tab1 = tableptr((*hplan)->tablenums[tableref - 1]);
	idx1 = *tab1->hindexarray + indexnum - 1;
	hcolkeys = idx1->hcolkeys;
	for (i1 = 0; i1 < tracecnt; i1++) {
		keytype = (*hcolkeys)[trace[i1]].keytypes;
		if (!(keytype &= keymask)) {
			if (readtype == READ_INDEX_AIMDEX) continue;
			break;
		}
		indexcolref = gettabcolref(tableref, (*hcolkeys)[trace[i1]].colnum);
		if (keytype & KEYTYPE_EQ) keytype = KEYTYPE_EQ;
		else if (keytype & KEYTYPE_LIKE) keytype = KEYTYPE_LIKE;
		startsimpleandtermparse(hplan, clause, indexcolref, readtype, keytype);
		for (firstflag = TRUE; getnextsimpleandterm(&keytype, NULL, &colref, &escref); ) {
			keyflag = TRUE;
			if (readtype == READ_INDEX_AIMDEX) {
				if (keytype == KEYTYPE_LIKE) addpgm2(OP_KEYLIKE, indexcolref, colref);
				else if (firstflag) {
					addpgm2(OP_COLMOVE, colref, indexcolref);
					firstflag = FALSE;
				}
				continue;
			}
			/* index */
			if (keytype & (KEYTYPE_EQ | KEYTYPE_LIKE)) {
				if (keytype == KEYTYPE_LIKE) addpgm3(OP_KEYLIKE, indexcolref, colref, escref);
				else addpgm2(OP_COLMOVE, colref, indexcolref);
				if (dynamickeyflag) addpgm2(OP_SET, VAR_1, 1);
				break;
			}
			if (keytype & (KEYTYPE_GT | KEYTYPE_GE)) {
				if (dynamickeyflag || !firstflag) {
					if (dynamickeyflag) {
						addpgm2(OP_GOTOIFNEG, LBL_WORK1, VAR_FORWARD);
						addpgm2(OP_GOTOIFZERO, pgmcount + 3, VAR_1);
					}
					addpgm3(OP_COLCOMPARE, indexcolref, colref, VAR_1);
					/* skip around the move code */
					if (keytype == KEYTYPE_GT) addpgm2(OP_GOTOIFPOS, pgmcount + 3, VAR_1);
					else addpgm2(OP_GOTOIFNOTNEG, pgmcount + 2, VAR_1);
				}
				addpgm2(OP_COLMOVE, colref, indexcolref);
				/* turn KEYTYPE_GT into KEYTYPE_GE */
				if (keytype == KEYTYPE_GT) addpgm2(OP_COLBININCR, indexcolref, 1);
				if (dynamickeyflag) addpgm2(OP_SET, VAR_1, 1);
/* LBL_WORK1 */
				defpgmlabel(LBL_WORK1);
				undefpgmlabel(LBL_WORK1);
				firstflag = FALSE;
			}
			else if (keytype & (KEYTYPE_LT | KEYTYPE_LE)) {  /* dynamic key type */
				addpgm2(OP_GOTOIFPOS, LBL_WORK1, VAR_FORWARD);
				addpgm2(OP_GOTOIFZERO, pgmcount + 3, VAR_1);
				addpgm3(OP_COLCOMPARE, indexcolref, colref, VAR_1);
				/* skip around the move code */
				if (keytype == KEYTYPE_LT) addpgm2(OP_GOTOIFNEG, pgmcount + 3, VAR_1);
				else addpgm2(OP_GOTOIFNOTPOS, pgmcount + 2, VAR_1);
				addpgm2(OP_COLMOVE, colref, indexcolref);
				/* turn KEYTYPE_LT into KEYTYPE_LE */
				if (keytype == KEYTYPE_GT) addpgm2(OP_COLBININCR, indexcolref, -1);
				addpgm2(OP_SET, VAR_1, 1);
/* LBL_WORK1 */
				defpgmlabel(LBL_WORK1);
				undefpgmlabel(LBL_WORK1);
			}
		}
	}
	return keyflag;
}

static void checkindexrange(HPLAN hplan, S_WHERE **clause, INT tableref, INT indexnum, INT readtype, USHORT *trace, INT tracecnt, INT dynamicflag, INT eoflabel)
{
	INT i1, colref1, colref2, indexcolref, indexexp, keymask, keytype, length, scale, type;
	TABLE *tab1;
	INDEX *idx1;
	HCOLKEY hcolkeys;
	LEXPENTRY *lexp;

	if (clause == NULL || (readtype != READ_INDEX_RANGE && readtype != READ_INDEX_EXACT_DUP)) return;

	/* code to break out of read loop when key range is exceeded */
	if (dynamicflag) keymask = KEYTYPE_EQ | KEYTYPE_LE | KEYTYPE_LT | KEYTYPE_GE | KEYTYPE_GT | KEYTYPE_LIKE;
	else keymask = KEYTYPE_EQ | KEYTYPE_LE | KEYTYPE_LT | KEYTYPE_LIKE;
	tab1 = tableptr((*hplan)->tablenums[tableref - 1]);
	idx1 = &(*tab1->hindexarray)[indexnum - 1];
	hcolkeys = idx1->hcolkeys;
	for (i1 = 0;  i1 < tracecnt; i1++) {
		keytype = (*hcolkeys)[trace[i1]].keytypes;
		if (!(keytype &= keymask)) break;
		indexcolref = gettabcolref(tableref, (*hcolkeys)[trace[i1]].colnum);
		startsimpleandtermparse(hplan, clause, indexcolref, READ_INDEX_EXACT, keytype);
		keymask = 0;
		while (getnextsimpleandterm(&keytype, &indexexp, &colref2, NULL)) {
			if (indexexp != -1) {
				lexp = (*clause)->lexpentries;
				if (!lexp[indexexp].tabcolnum) {
					aexptype(lexp[indexexp].expp, hplan, &type, &length, &scale);
					colref1 = makevarfromtype(type, length, scale);
					if (colref1 < 0) return;
					lexp = (*clause)->lexpentries;
					lexp[indexexp].tabcolnum = colref1 + 1;
				}
				else colref1 = lexp[indexexp].tabcolnum - 1;
				genaexpcode(lexp[indexexp].expp, hplan, NULL, colref1);
			}
			else colref1 = indexcolref;
			if (keytype == KEYTYPE_LIKE) {
				addpgm3(OP_COLLIKE, colref1, colref2, VAR_1);
				addpgm2(OP_GOTOIFNOTZERO, eoflabel, VAR_1);
			}
			else if (keytype == KEYTYPE_EQ) {
				addpgm3(OP_COLCOMPARE, colref1, colref2, VAR_1);
				addpgm2(OP_GOTOIFNOTZERO, eoflabel, VAR_1);
				if (dynamicflag) keymask |= KEYTYPE_EQ | KEYTYPE_LE | KEYTYPE_LT | KEYTYPE_GE | KEYTYPE_GT | KEYTYPE_LIKE;
				else keymask |= KEYTYPE_EQ | KEYTYPE_LE | KEYTYPE_LT | KEYTYPE_LIKE;
			}
			else if (keytype & (KEYTYPE_LE | KEYTYPE_LT)) {
				if (dynamicflag) addpgm2(OP_GOTOIFNEG, pgmcount + 3, VAR_FORWARD);
				addpgm3(OP_COLCOMPARE, colref1, colref2, VAR_1);
				if (keytype == KEYTYPE_LE) addpgm2(OP_GOTOIFPOS, eoflabel, VAR_1);
				else addpgm2(OP_GOTOIFNOTNEG, eoflabel, VAR_1);
			}
			else if ((keytype & (KEYTYPE_GE | KEYTYPE_GT)) && dynamicflag) {
				addpgm2(OP_GOTOIFNOTNEG, pgmcount + 3, VAR_FORWARD);
				addpgm3(OP_COLCOMPARE, colref1, colref2, VAR_1);
				if (keytype == KEYTYPE_GE) addpgm2(OP_GOTOIFNEG, eoflabel, VAR_1);
				else addpgm2(OP_GOTOIFNOTPOS, eoflabel, VAR_1);
			}
		}
	}
}

/* intialize for and term parsing */
static void startsimpleandtermparse(HPLAN hplan, S_WHERE **whereclause, UINT tabcolref, INT readflag, INT condition)
{
	parseshplan = hplan;
	parseswhere = whereclause;
	parseswherepos = -1;
	parsestabcolref = tabcolref;
	parsesreadflag = readflag;
	parsescondition = condition;
}

/* return the next simple and term in the parameters */
/* return TRUE if found */
/* return FALSE if no more terms */
static INT getnextsimpleandterm(INT *keytype, INT *colexpp, INT *colref, INT *escref)
{
	int i1, left, lefttabcolnum, length, op, op1, op2, right, righttabcolnum, scale, type;
	UINT colref1, colref2;
	LEXPENTRY *lexp;
	S_VALUE *svalue;

	if (execerror) return FALSE;
	if (parseswhere == NULL) return FALSE;

	lexp = (*parseswhere)->lexpentries;
	for ( ; ; ) {
		if (parseswherepos == -1) parseswherepos = (*parseswhere)->numentries - 1;
		else {
			for ( ; ; ) {
				i1 = lexp[parseswherepos].parent;
				if (i1 == -1) return FALSE;
				if (lexp[i1].left == parseswherepos) {
					parseswherepos = lexp[i1].right;
					break;
				}
				parseswherepos = i1;
			}
		}
		while (lexp[parseswherepos].type == OPAND) parseswherepos = lexp[parseswherepos].left;
		op = lexp[parseswherepos].type;
		switch (op) {
		case OPEQ:
			if (!(parsescondition & KEYTYPE_EQ)) continue;
			*keytype = KEYTYPE_EQ;
			break;
		case OPLT:
			if (!(parsescondition & KEYTYPE_LT)) continue;
			*keytype = KEYTYPE_LT;
			break;
		case OPGT:
			if (!(parsescondition & KEYTYPE_GT)) continue;
			*keytype = KEYTYPE_GT;
			break;
		case OPLE:
			if (!(parsescondition & KEYTYPE_LE)) continue;
			*keytype = KEYTYPE_LE;
			break;
		case OPGE:
			if (!(parsescondition & KEYTYPE_GE)) continue;
			*keytype = KEYTYPE_GE;
			break;
		case OPLIKE:
#if 1
			if (!(parsescondition & KEYTYPE_LIKE)) continue;
#else
/*** CODE: DO THIS IF WE WANT TO OPTIMIZE FOR SOMEONE USING LIKE WITH NO PATTERNS ***/
			if (!(parsescondition & (KEYTYPE_LIKE | KEYTYPE_EQ))) continue;
#endif
			*keytype = KEYTYPE_LIKE;
			break;
		default:
			continue;
		}
		left = lexp[parseswherepos].left;
		right = lexp[parseswherepos].right;
		lefttabcolnum = lexp[left].tabcolnum;
		op1 = lexp[left].type;
		if (op1 == OPEXP) op1 = getexptabcolnum(lexp[left].expp, &lefttabcolnum, TABCOL_LOWER | TABCOL_UPPER);
		colref1 = (op1 == OPCOLUMN || EXPOPERATOR(op1)) ? lefttabcolnum : 0;
		righttabcolnum = lexp[right].tabcolnum;
		op2 = lexp[right].type;
		if (op2 == OPEXP) op2 = getexptabcolnum(lexp[right].expp, &righttabcolnum, TABCOL_LOWER | TABCOL_UPPER);
		colref2 = (op2 == OPCOLUMN || EXPOPERATOR(op2)) ? righttabcolnum : 0;
		if (gettabref(colref1) == gettabref(colref2)) continue;
		if (colref1 == parsestabcolref) {
			if (colexpp != NULL) {
				if (lexp[left].type == OPEXP) *colexpp = left;
				else *colexpp = -1;
			}
			if (lexp[right].type == OPEXP) {
				if (op == OPLIKE && parsesreadflag != READ_INDEX_AIMDEX) continue;
				if (op2 == OPEXP) {
					op2 = getexptabcolnum(lexp[right].expp, &righttabcolnum, TABCOL_HIGHEST);
					if (op2 == OPEXP || colref1 == (UINT) gettabref(righttabcolnum)) continue;
				}
				if (!lexp[right].tabcolnum) {
					aexptype(lexp[right].expp, parseshplan, &type, &length, &scale);
					*colref = makevarfromtype(type, length, scale);
					if (*colref < 0) return FALSE;
					lexp = (*parseswhere)->lexpentries;
					lexp[right].tabcolnum = *colref + 1;
				}
				else *colref = lexp[right].tabcolnum - 1;
				genaexpcode(lexp[right].expp, parseshplan, NULL, *colref);
				lexp = (*parseswhere)->lexpentries;
				break;
			}
			if (op2 == OPVALUE) {
				if (op == OPLIKE) {
#if 1
					if (parsesreadflag != READ_INDEX_AIMDEX) {
						svalue = *lexp[right].valuep;
						if (!svalue->length || svalue->data[0] == '%' || svalue->data[0] == '_') continue;
					}
#else
/*** CODE: DO THIS IF WE WANT TO OPTIMIZE FOR SOMEONE USING LIKE WITH NO PATTERNS ***/
/***       ALSO GET #IF 0 BELOW ***/
					svalue = *lexp[right].valuep;
					if (!(i2 = svalue->length)) continue;
					if (parsesreadflag != READ_INDEX_AIMDEX || (parsescondition & KEYTYPE_EQ)) {
						for (i1 = 0; i1 < i2; i1++) {
							if (svalue->data[i1] == '%' || svalue->data[i1] == '_') break;
							if (svalue->data[i1] == svalue->escape && i1 + 1 < i2 && (svalue->data[i1 + 1] == svalue->escape || svalue->data[i1 + 1] == '%' || svalue->data[i1 + 1] == '_')) i1++;
						}
						if (i1 == i2 && (parsescondition & KEYTYPE_EQ)) *keytype = KEYTYPE_EQ;
						else if (parsesreadflag != READ_INDEX_AIMDEX && !i1) continue;
					}
#endif
				}
				*colref = makelitfromsvaldata(lexp[right].valuep);
				if (*colref < 0) return FALSE;
				if (escref != NULL) {
					*escref = makelitfromsvalescape(lexp[right].valuep);
					if (*escref < 0) return FALSE;
				}
				lexp = (*parseswhere)->lexpentries;
				break;
			}
			if (op2 == OPCOLUMN) {
				*colref = lexp[right].tabcolnum;
#if 0
/*** CODE: DO THIS IF WE WANT TO OPTIMIZE FOR SOMEONE USING LIKE WITH NO PATTERNS ***/
				if (op == OPLIKE && (parsescondition & KEYTYPE_EQ)) *keytype = KEYTYPE_EQ;
#endif
				break;
			}
		}
		else if (colref2 == parsestabcolref && op != OPLIKE) {
			if (colexpp != NULL) {
				if (lexp[right].type == OPEXP) *colexpp = right;
				else *colexpp = -1;
			}
			if (lexp[left].type == OPEXP) {
				if (op1 == OPEXP) {
					op1 = getexptabcolnum(lexp[left].expp, &lefttabcolnum, TABCOL_HIGHEST);
					if (op1 == OPEXP || colref1 == (UINT) gettabref(lefttabcolnum)) continue;
				}
				if (!lexp[left].tabcolnum) {
					aexptype(lexp[left].expp, parseshplan, &type, &length, &scale);
					*colref = makevarfromtype(type, length, scale);
					if (*colref < 0) return FALSE;
					lexp = (*parseswhere)->lexpentries;
					lexp[left].tabcolnum = *colref + 1;
				}
				else *colref = lexp[left].tabcolnum - 1;
				genaexpcode(lexp[left].expp, parseshplan, NULL, *colref);
				lexp = (*parseswhere)->lexpentries;
				break;
			}
			if (op1 == OPVALUE) {
				*colref = makelitfromsvaldata(lexp[left].valuep);
				if (*colref < 0) return FALSE;
				lexp = (*parseswhere)->lexpentries;
				break;
			}
			if (op1 == OPCOLUMN) {
				*colref = lexp[left].tabcolnum;
				break;
			}
		}
	}
	if (parsesreadflag == READ_INDEX_EXACT) lexp[parseswherepos].flags |= EXPENTRY_TESTED;
	return TRUE;
}

/* generate code for a logical expression */
static void genlexpcode(INT lexpindex, HPLAN hplan, EXPSRC *expsrc, INT failgoto)
{
	INT i1, i2, i3, left, len, right, scale, type;
	LEXPENTRY *lexp, *lexpleft, *lexpright;

	lexp = (*swhere)->lexpentries + lexpindex;
	left = lexp->left;
	right = lexp->right;
	switch (lexp->type) {
	case OPEQ:
	case OPLIKE:
		if (lexp->flags & EXPENTRY_TESTED) return;
		i1 = OP_GOTOIFNOTZERO;
		break;
	case OPLT:
		if (lexp->flags & EXPENTRY_TESTED) return;
		i1 = OP_GOTOIFNOTNEG;
		break;
	case OPGT:
		if (lexp->flags & EXPENTRY_TESTED) return;
		i1 = OP_GOTOIFNOTPOS;
		break;
	case OPLE:
		if (lexp->flags & EXPENTRY_TESTED) return;
		i1 = OP_GOTOIFPOS;
		break;
	case OPGE:
		if (lexp->flags & EXPENTRY_TESTED) return;
		i1 = OP_GOTOIFNEG;
		break;
	case OPNE:
		i1 = OP_GOTOIFZERO;
		break;
	case OPNULL:
		i1 = OP_GOTOIFZERO;
		break;
	default:
		i1 = 0;
		break;
	}
	if (i1) {
		lexpleft = (*swhere)->lexpentries + left;
		if (lexpleft->type == OPVALUE) {
			i2 = makelitfromsvaldata(lexpleft->valuep);
			lexp = (*swhere)->lexpentries + lexpindex;
		}
		else if (lexpleft->type == OPCOLUMN) i2 = lexpleft->tabcolnum;
		else if (lexpleft->type == OPEXP) {
			if (!lexpleft->tabcolnum) {
				aexptype(lexpleft->expp, hplan, &type, &len, &scale);
				i2 = makevarfromtype(type, len, scale);
				lexp = (*swhere)->lexpentries + lexpindex;
				lexpleft = (*swhere)->lexpentries + left;
				lexpleft->tabcolnum = i2 + 1;
			}
			else i2 = lexpleft->tabcolnum - 1;
			genaexpcode(lexpleft->expp, hplan, expsrc, i2);
			lexp = (*swhere)->lexpentries + lexpindex;
		}
		else {
/*** CODE: IS THIS CORRECT ?? ***/
			execerror = TRUE;
			sqlerrnummsg(SQLERR_PARSE_ERROR, "IS NULL not supported with expressions", NULL);
		}
		if (lexp->type != OPNULL) {
			lexpright = (*swhere)->lexpentries + right;
			if (lexpright->type == OPVALUE) {
				i3 = makelitfromsvaldata(lexpright->valuep);
				lexp = (*swhere)->lexpentries + lexpindex;
			}
			else if (lexpright->type == OPCOLUMN) i3 = lexpright->tabcolnum;
			else if (lexpright->type == OPEXP) {
				if (!lexpright->tabcolnum) {
					aexptype(lexpright->expp, hplan, &type, &len, &scale);
					i3 = makevarfromtype(type, len, scale);
					lexp = (*swhere)->lexpentries + lexpindex;
					lexpright = (*swhere)->lexpentries + right;
					lexpright->tabcolnum = i3 + 1;
				}
				else i3 = lexpright->tabcolnum - 1;
				genaexpcode(lexpright->expp, hplan, expsrc, i3);
				lexp = (*swhere)->lexpentries + lexpindex;
			}
			else {
/*** CODE: IS THIS CORRECT ?? ***/
				execerror = TRUE;
				sqlerrnummsg(SQLERR_PARSE_ERROR, "IS NULL not supported with expressions", NULL);
			}
		}
		if (lexp->type == OPLIKE) addpgm3(OP_COLLIKE, i2, i3, VAR_1);
		else if (lexp->type == OPNULL) addpgm2(OP_COLISNULL, i2, VAR_1);
		else addpgm3(OP_COLCOMPARE, i2, i3, VAR_1);
		addpgm2(i1, failgoto, VAR_1);
	}
	else if (lexp->type == OPAND) {
		genlexpcode(left, hplan, expsrc, failgoto);
		genlexpcode(right, hplan, expsrc, failgoto);
	}
	else if (lexp->type == OPOR) {
		addpgm1(OP_GOTO, pgmcount + 2);			/* skip around */
		i1 = pgmcount;							/* OR failure label */
		addpgm1(OP_GOTO, 0xEEEEEEEE);			/* fill in below */
		genlexpcode(left, hplan, expsrc, i1);
		i2 = pgmcount;							/* success label */
		addpgm1(OP_GOTO, 0xEEEEEEEE);			/* goto success, fill in later */
		(*hpgm)[i1].op1 = pgmcount;				/* fill in */
		genlexpcode(right, hplan, expsrc, failgoto);
		(*hpgm)[i2].op1 = pgmcount;				/* fill in */
	}
	else if (lexp->type == OPNOT) {
		addpgm1(OP_GOTO, pgmcount + 2);			/* skip around */
		i1 = pgmcount;							/* NOT failure label */
		addpgm1(OP_GOTO, 0xEEEEEEEE);			/* fill in below */
		genlexpcode(left, hplan, expsrc, i1);
		addpgm1(OP_GOTO, failgoto);
		(*hpgm)[i1].op1 = pgmcount;				/* fill in */
	}
}

/* generate code for an arithmetic expression */
static void genaexpcode(S_AEXP **aexp, HPLAN hplan, EXPSRC *expsrc, UINT desttabcolnum)
{
	INT i1, i2, i3, level, op, tablenum, worktabcolnum;
	UCHAR *ptr;
	AEXPENTRY *entry;
	TABLE *tab1;
	COLUMN *col1;
	struct {
		INT tabcolref;
		INT tabcolnum;
	} stack[16];

	level = 0;
	worktabcolnum = gettabcolref(TABREF_WORKVAR, 1);
	for (i1 = 0, i2 = (*aexp)->numentries; i1 < i2; ) {
		entry = (*aexp)->aexpentries + i1++;
		op = entry->type;
		if (op == OPCOLUMN) {
			if (level == sizeof(stack) / sizeof(*stack)) break;
			if (expsrc != NULL && expsrc->numgroup) {
				for (i3 = 0; i3 < expsrc->numgroup; i3++)
					if (entry->tabcolnum == (*expsrc->grouparray)[i3]) break;
				if (i3 == expsrc->numgroup) {
					execerror = TRUE;
					sqlerrnummsg(SQLERR_INTERNAL, "unable to locate group column in expression", NULL);
					return;
				}
				i3 = gettabcolref(expsrc->groupref, i3 + 1);
			}
			else i3 = entry->tabcolnum;
			stack[level].tabcolnum = entry->tabcolnum;
			stack[level++].tabcolref = i3;
			if (i1 == i2) addpgm2(OP_COLMOVE, stack[level - 1].tabcolref, desttabcolnum);
		}
		else if (op == OPVALUE) {
			if (level == sizeof(stack) / sizeof(*stack)) break;
			stack[level++].tabcolref = makelitfromsvaldata(entry->valuep);
			if (i1 == i2) addpgm2(OP_COLMOVE, stack[level - 1].tabcolref, desttabcolnum);
		}
		else if (op >= OPCOUNTALL && op <= OPMAXDISTINCT) {
			if (level == sizeof(stack) / sizeof(*stack)) break;
			if (expsrc == NULL || !expsrc->numsetfunc) {
				execerror = TRUE;
				sqlerrnummsg(SQLERR_INTERNAL, "unexpected set function in expression", NULL);
				return;
			}
			for (i3 = 0; i3 < expsrc->numsetfunc; i3++)
				if (op == (*expsrc->setfuncarray)[i3].type && entry->tabcolnum == (*expsrc->setfuncarray)[i3].tabcolnum) break;
			if (i3 == expsrc->numsetfunc) {
				execerror = TRUE;
				sqlerrnummsg(SQLERR_INTERNAL, "unable to locate set function column in expression", NULL);
				return;
			}
			stack[level].tabcolnum = entry->tabcolnum;
			stack[level++].tabcolref = gettabcolref(expsrc->setfuncref, i3 + 1);
			if (i1 == i2) addpgm2(OP_COLMOVE, stack[level - 1].tabcolref, desttabcolnum);
		}
		else {
			if (i1 == i2) worktabcolnum = desttabcolnum;
			if (op == OPNEGATE) addpgm2(OP_COLNEGATE, stack[--level].tabcolref, worktabcolnum);
			else if (op == OPLOWER || op == OPUPPER || op == OPCAST || op == OPSUBSTRLEN || op == OPSUBSTRPOS || 
					op == OPTRIMB || op == OPTRIML || op == OPTRIMT || op == OPCONCAT) {
				if (op == OPLOWER || op == OPUPPER || op == OPCAST) level--;
				else level -= 2;
				if (i1 == i2) i3 = desttabcolnum;
				else {			
					if (op == OPCONCAT) {
						/* create largest work literal possible, since final size is unknown */	
						i3 = (*hplan)->maxexpsize; 					
					}
					else {
						i3 = gettabref(stack[level].tabcolref);
						if (i3 == TABREF_LITBUF) {
							if (op == OPTRIMB || op == OPTRIML || op == OPTRIMT || op == OPSUBSTRLEN || op == OPSUBSTRPOS) {				
								/* create largest work literal possible, since final size is unknown */	
								i3 = (*hplan)->maxexpsize; 	
							}
							else {
								i3 = getcolnum(i3);
								ptr = *literals + i3 + 1;
								i3 = ((INT) ptr[0] << 8) + ptr[1];
							}
						}
	/*** CODE: DON'T KNOW THE SIZE OF THE TEMPORARY WORK VARIABLE ***/
						else if (i3 == TABREF_WORKVAR) i3 = 0;
						else if (i3 <= (*hplan)->numtables) {
							tablenum = (*hplan)->tablenums[gettabref(stack[level].tabcolnum) - 1];
							tab1 = tableptr(tablenum);
							col1 = columnptr(tab1, getcolnum(stack[level].tabcolnum));
							i3 = col1->length;
						}
						else {
							execerror = TRUE;
							sqlerrnummsg(SQLERR_INTERNAL, "unable to determine column type with lower/upper expression", NULL);
							return;
						}
					}
/*** NOTE: THIS IS SLIGHTLY FLAWED AS PERFORMING THE SAME EXPRESSION ***/
/***       CAUSES MULTIPLE VARIABLES TO BE ALLOCATED, WHEN ONE WILL DO ***/
					i3 = makevarfromtype(TYPE_CHAR, i3, 0);
				}
				if (op == OPLOWER) addpgm2(OP_COLLOWER, stack[level].tabcolref, i3);
				else if (op == OPUPPER) addpgm2(OP_COLUPPER, stack[level].tabcolref, i3);
				else if (op == OPCAST) addpgm2(OP_COLCAST, stack[level].tabcolref, i3);
				else if (op == OPSUBSTRLEN) {
					addpgm3(OP_COLSUBSTR_LEN, stack[level].tabcolref, stack[level + 1].tabcolref, i3);
				}
				else if (op == OPSUBSTRPOS) {
					addpgm3(OP_COLSUBSTR_POS, stack[level].tabcolref, stack[level + 1].tabcolref, i3);
				}
				else if (op == OPTRIML) {
					addpgm3(OP_COLTRIM_L, stack[level].tabcolref, stack[level + 1].tabcolref, i3); 
				}
				else if (op == OPTRIMT) {
					addpgm3(OP_COLTRIM_T, stack[level].tabcolref, stack[level + 1].tabcolref, i3); 
				}
				else if (op == OPTRIMB) {
					addpgm3(OP_COLTRIM_B, stack[level].tabcolref, stack[level + 1].tabcolref, i3); 
				}
				else if (op == OPCONCAT) {
					addpgm3(OP_COLCONCAT, stack[level].tabcolref, stack[level + 1].tabcolref, i3);
				}
				stack[level++].tabcolref = i3;
				continue;
			}
			else {
				if (op == OPADD) i3 = OP_COLADD;
				else if (op == OPSUB) i3 = OP_COLSUB;
				else if (op == OPMULT) i3 = OP_COLMULT;
				else if (op == OPDIV) i3 = OP_COLDIV;
				else {
					execerror = TRUE;
					sqlerrnummsg(SQLERR_INTERNAL, "unrecognized expression op code", NULL);
					return;
				}
				level -= 2;
				addpgm3(i3, stack[level].tabcolref, stack[level + 1].tabcolref, worktabcolnum);
			}
			if (level < 0) {
				execerror = TRUE;
				sqlerrnummsg(SQLERR_INTERNAL, "expression stack underflow", NULL);
				return;
			}
			stack[level++].tabcolref = worktabcolnum++;
		}
	}
	if (level == sizeof(stack) / sizeof(*stack)) {
		execerror = TRUE;
		sqlerrnummsg(SQLERR_INTERNAL, "expression stack overflow", NULL);
		return;
	}
}

static void aexptype(S_AEXP **aexp, HPLAN hplan, INT *type, INT *len, INT *scale)
{
	INT i1, i2, i3, i4, level, lft1, lft2, op, rgt1, rgt2, tablenum;
	UCHAR *ptr;
	AEXPENTRY *entry;
	TABLE *tab1;
	COLUMN *col1;
	struct {
		INT type;
		INT len;
		INT scale;
	} stack[16];

	*type = *len = *scale = 0;
	level = 0;
	for (i1 = 0, i2 = (*aexp)->numentries; i1 < i2; ) {
		entry = (*aexp)->aexpentries + i1++;
		op = entry->type;
		if (op == OPCOLUMN) {
			if (level == sizeof(stack) / sizeof(*stack)) break;
			tablenum = (*hplan)->tablenums[gettabref(entry->tabcolnum) - 1];
			tab1 = tableptr(tablenum);
			col1 = columnptr(tab1, getcolnum(entry->tabcolnum));
			stack[level].type = col1->type;
			stack[level].len = col1->length;
			stack[level++].scale = col1->scale;
		}
		else if (op == OPVALUE) {
			if (level == sizeof(stack) / sizeof(*stack)) break;
			stack[level].type = TYPE_CHAR;
			stack[level].len = (*entry->valuep)->length;
			stack[level].scale = 0;
			ptr = (*entry->valuep)->data;
			if (stack[level].len && (ptr[0] == '-' || isdigit(ptr[0]))) {
				/* try to get scale */
				for (i3 = 1, i4 = stack[level].len; i3 < i4 && isdigit(ptr[i3]); i3++);
				if (i3 < i4 && ptr[i3] == '.') {
					stack[level].scale = i4 - ++i3;
					for ( ; i3 < i4 && isdigit(ptr[i3]); i3++);
					if (i3 < i4) stack[level].scale = 0;
					else stack[level].type = TYPE_NUM;
				}
			}
			level++;
		}
		else if (op >= OPCOUNTALL && op <= OPMAXDISTINCT) {
			if (op == OPCOUNTALL || op == OPCOUNT || op == OPCOUNTDISTINCT) {
				stack[level].type = TYPE_POSNUM;
				stack[level].len = 10;
				stack[level].scale = 0;
			}
			else {
				tablenum = (*hplan)->tablenums[gettabref(entry->tabcolnum) - 1];
				tab1 = tableptr(tablenum);
				col1 = columnptr(tab1, getcolnum(entry->tabcolnum));
				stack[level].type = col1->type;
				stack[level].len = col1->length;
				stack[level].scale = col1->scale;
			}
			level++;
		}
		else {
			if (!level) {
				execerror = TRUE;
				sqlerrnummsg(SQLERR_INTERNAL, "expression stack underflow", NULL);
				return;
			}
			if (op == OPLOWER || op == OPUPPER) {
				if (stack[level - 1].type == TYPE_LITERAL) stack[level - 1].scale = 0;
			}
			else if (op == OPSUBSTRLEN || op == OPSUBSTRPOS || op == OPTRIMB || op == OPTRIML || op == OPTRIMT) {
				/* pop top two arguments off of stack and push result */
				if (level < 2) {
					execerror = TRUE;
					sqlerrnummsg(SQLERR_INTERNAL, "expression stack underflow", NULL);
					return;
				}
				level -= 2; 
				stack[level].type = TYPE_CHAR;
				if (op == OPTRIMB || op == OPTRIML || op == OPTRIMT) {
					/* second arg on stack has length, make result have same length */
					stack[level].len = stack[level + 1].len;
				}
				stack[level].scale = 0;
				level++;
			}
			else if (op == OPCAST) {
				/* NOTE: if length or scale were not specified (-1), */
				/* then length and scale of src is used for dest */
				stack[level - 1].type = entry->info.cast.type;
				if (entry->info.cast.length != -1) stack[level - 1].len = entry->info.cast.length;
				if (entry->info.cast.scale != -1) stack[level - 1].scale = entry->info.cast.scale;		
			}
			else if (op == OPCONCAT) {
				if (level < 2) {
					execerror = TRUE;
					sqlerrnummsg(SQLERR_INTERNAL, "expression stack underflow", NULL);
					return;
				}
				/* pop top two stack items and push concatenation */
				level -= 2; 
				stack[level].type = TYPE_CHAR;
				stack[level].len += stack[level + 1].len;
				stack[level].scale = 0;
				level++;
			}
			else if (op != OPNEGATE) {
				if (level < 2) {
					execerror = TRUE;
					sqlerrnummsg(SQLERR_INTERNAL, "expression stack underflow", NULL);
					return;
				}
				level -= 2;
				lft1 = stack[level].len;
				if (stack[level].scale) {
					rgt1 = stack[level].scale;
					lft1 -= rgt1 + 1;
				}
				else rgt1 = 0;
				lft2 = stack[level + 1].len;
				if (stack[level + 1].scale) {
					rgt2 = stack[level + 1].scale;
					lft2 -= rgt2 + 1;
				}
				else rgt2 = 0;
				if (op == OPADD || op == OPSUB) {
					if (lft2 > lft1) lft1 = lft2;
					lft1++;
					if (rgt2 > rgt1) rgt1 = rgt2;
				}
				else if (op == OPMULT) {
					lft1 += lft2;
					rgt1 += rgt2;
				}
				else if (op == OPDIV) {
					i3 = lft2 + rgt1;
					rgt1 = lft1 + rgt2;
					lft1 = i3;
				}
				else {
					execerror = TRUE;
					sqlerrnummsg(SQLERR_INTERNAL, "unrecognized expression op code", NULL);
					return;
				}
				if (lft1 > 31) lft1 = 31;
				if (rgt1) {
					if (rgt1 > 31) rgt1 = 31;
					lft1 += rgt1 + 1;
				}
				stack[level].type = TYPE_NUM;
				stack[level].len = lft1;
				stack[level++].scale = rgt1;
			}
		}
	}
	if (level != 1) {
		execerror = TRUE;
		if (level < 0) sqlerrnummsg(SQLERR_INTERNAL, "expression stack underflow", NULL);
		if (!level) sqlerrnummsg(SQLERR_INTERNAL, "empty expression", NULL);
		if (level == sizeof(stack) / sizeof(*stack)) sqlerrnummsg(SQLERR_INTERNAL, "expression stack overflow", NULL);
		sqlerrnummsg(SQLERR_INTERNAL, "expression did not result in a single value", NULL);
		return;
	}
	*type = stack[0].type;
	*len = stack[0].len;
	*scale = stack[0].scale;
	(*hplan)->maxexpsize = *len;
}

static INT getexptabcolnum(S_AEXP **aexp, INT *tabcolnump, INT flags)
{
	INT i1, i2, i3, expflag, op, valueflag;
	UINT tabcolnum;
	AEXPENTRY *entry;

	*tabcolnump = tabcolnum = 0;
	expflag = valueflag = FALSE;
	for (i1 = 0, i2 = (*aexp)->numentries; i1 < i2; ) {
		entry = (*aexp)->aexpentries + i1++;
		op = entry->type;
		if (op == OPCOLUMN) {
			if (tabcolnum) {
				if (!(flags & TABCOL_HIGHEST)) return OPEXP;
				if (entry->tabcolnum > tabcolnum) tabcolnum = entry->tabcolnum;
			}
			else tabcolnum = entry->tabcolnum;
		}
		else if (op == OPVALUE) valueflag = TRUE;
		else if (op >= OPCOUNTALL && op <= OPMAXDISTINCT) return OPEXP;
		else if (op == OPLOWER || op == OPUPPER) {
			if (op == OPLOWER) i3 = TABCOL_LOWER;
			else if (op == OPUPPER) i3 = TABCOL_UPPER;
			if ((flags & i3) && i2 == 2 && tabcolnum) {
				*tabcolnump = tabcolnum;
				return op;
			}
			expflag = TRUE;
		}
		else expflag = TRUE;
	}
	if (tabcolnum) {
		if (!expflag || (flags & TABCOL_HIGHEST)) {
			*tabcolnump = tabcolnum;
			return OPCOLUMN;
		}
		return OPEXP;
	}
	return OPVALUE;
}

static INT makelitfromname(INT name)
{
	int i1, len;

	if (execerror) return -1;

	if (litsize > COLNUM_MASK) {
		execerror = TRUE;
		return sqlerrnummsg(SQLERR_INTERNAL, "literals too large", NULL);
	}
	len = (int)strlen(nameptr(name));
	if (len > 65500) len = 65500;
	if (litsize + len + 4 > litallocsize) {
		i1 = ((litsize + len + 4) & ~0xFF) + 0x0100;
		if (memchange(literals, i1, 0)) {
			execerror = TRUE;
			return sqlerrnum(SQLERR_NOMEM);
		}
		litallocsize = i1;
	}
	i1 = litsize;
	*(*literals + litsize++) = TYPE_LITERAL;
	*(*literals + litsize++) = (UCHAR)(len >> 8);
	*(*literals + litsize++) = (UCHAR) len;
	*(*literals + litsize++) = 0;
	memcpy(*literals + litsize, nameptr(name), len);
	litsize += len;
	return gettabcolref(TABREF_LITBUF, i1);
}

static INT makelitfromsvaldata(S_VALUE **svalue)
{
	int i1, len;

	if (execerror) return -1;

	/* check if literal already created */
	if ((*svalue)->litoffset) return (*svalue)->litoffset - 1;

	if (litsize > COLNUM_MASK) {
		execerror = TRUE;
		return sqlerrnummsg(SQLERR_INTERNAL, "literals too large", NULL);
	}
	len = (*svalue)->length;
	if (len > 65500) len = 65500;
	if (litsize + len + 4 > litallocsize) {
		i1 = ((litsize + len + 4) & ~0xFF) + 0x0100;
		if (memchange(literals, i1, 0)) {
			execerror = TRUE;
			return sqlerrnum(SQLERR_NOMEM);
		}
		litallocsize = i1;
	}
	i1 = litsize;
	(*svalue)->litoffset = litsize + 1;
	*(*literals + litsize++) = (UCHAR) TYPE_LITERAL;
	*(*literals + litsize++) = (UCHAR)(len >> 8);
	*(*literals + litsize++) = (UCHAR) len;
	*(*literals + litsize++) = 0;
	memcpy(*literals + litsize, (*svalue)->data, len);
	litsize += len;
	return gettabcolref(TABREF_LITBUF, i1);
}

static INT makelitfromsvalescape(S_VALUE **svalue)
{
	int i1, len;

	if (execerror) return -1;

	if (litsize > COLNUM_MASK) {
		execerror = TRUE;
		return sqlerrnummsg(SQLERR_INTERNAL, "literals too large", NULL);
	}
	len = 1;
	if (litsize + len + 4 > litallocsize) {
		i1 = ((litsize + len + 4) & ~0xFF) + 0x0100;
		if (memchange(literals, i1, 0)) {
			execerror = TRUE;
			return sqlerrnum(SQLERR_NOMEM);
		}
		litallocsize = i1;
	}
	i1 = litsize;
	*(*literals + litsize++) = (UCHAR) TYPE_LITERAL;
	*(*literals + litsize++) = (UCHAR)(len >> 8);
	*(*literals + litsize++) = (UCHAR) len;
	*(*literals + litsize++) = 0;
	memcpy(*literals + litsize, (*svalue)->escape, len);
	litsize += len;
	return gettabcolref(TABREF_LITBUF, i1);
}

static INT makevarfromtype(INT type, INT len, INT scale)
{
	int i1;

	if (execerror) return -1;

	if (litsize > COLNUM_MASK) {
		execerror = TRUE;
		return sqlerrnummsg(SQLERR_INTERNAL, "literals too large", NULL);
	}
	if (len > 65500) len = 65500;
	if (litsize + len + 4 > litallocsize) {
		i1 = ((litsize + len + 4) & ~0xFF) + 0x0100;
		if (memchange(literals, i1, 0)) {
			execerror = TRUE;
			return sqlerrnum(SQLERR_NOMEM);
		}
		litallocsize = i1;
	}
	i1 = litsize;
	*(*literals + litsize++) = (UCHAR) type;
	*(*literals + litsize++) = (UCHAR)(len >> 8);
	*(*literals + litsize++) = (UCHAR) len;
	*(*literals + litsize++) = (UCHAR) scale;
	memset(*literals + litsize, ' ', len - scale);
	if ((type == TYPE_NUM || type == TYPE_POSNUM) && scale) {
		(*literals)[litsize + len - scale - 1] = '.';
		memset(*literals + litsize + len - scale, '0', scale);
	}
	litsize += len;
	return gettabcolref(TABREF_LITBUF, i1);
}

static void defpgmlabel(int labeldef)
{
	INT i1, i2;

	if (execerror) return;

	if (labeldefcount >= MAXLABELDEFS) {
		sqlerrnummsg(SQLERR_NOMEM, "too many label definitions", NULL);
		execerror = TRUE;
		return;
	}
	for (i1 = 0; i1 < labeldefcount; i1++) {
		if (labeldefname[i1] == labeldef) {
			sqlerrnummsg(SQLERR_INTERNAL, "attempted definition of existing label", NULL);
			execerror = TRUE;
			return;
		}
	}
	labeldefname[labeldefcount] = labeldef;
	labeldefvalue[labeldefcount++] = pgmcount;

	/* fix up the label references */
	for (i1 = i2 = 0; i1 < labelrefcount; i1++) {
		if (labelrefname[i1] == labeldef) (*hpgm)[labelrefvalue[i1]].op1 = pgmcount;
		else {
			if (i1 != i2) {
				labelrefname[i2] = labelrefname[i1];
				labelrefvalue[i2] = labelrefvalue[i1];
			}
			i2++;
		}
	}
	labelrefcount = i2;
}

static void undefpgmlabel(int labeldef)
{
	INT i1, i2;

	if (execerror) return;

	for (i1 = i2 = 0; i1 < labeldefcount; i1++) {
		if (labeldef == labeldefname[i1]) continue;
		if (i1 != i2) {
			labeldefname[i2] = labeldefname[i1];
			labeldefvalue[i2] = labeldefvalue[i1];
		}
		i2++;
	}
	labeldefcount = i2;
}

/* following code is for debug purposes only */
void debugcode(char *str, int pcount, PCODE *pgm)
{
	char debugarea[256], *ptr;

	switch (pgm->code) {
	case OP_CLEAR:
		ptr = "OP_CLEAR          ";
		break;
	case OP_KEYINIT:
		ptr = "OP_KEYINIT        ";
		break;
	case OP_KEYLIKE:
		ptr = "OP_KEYLIKE        ";
		break;
	case OP_SETFIRST:
		ptr = "OP_SETFIRST       ";
		break;
	case OP_SETLAST:
		ptr = "OP_SETLAST        ";
		break;
	case OP_READNEXT:
		ptr = "OP_READNEXT       ";
		break;
	case OP_READPREV:
		ptr = "OP_READPREV       ";
		break;
	case OP_READBYKEY:
		ptr = "OP_READBYKEY      ";
		break;
	case OP_READBYKEYREV:
		ptr = "OP_READBYKEYREV   ";
		break;
	case OP_READPOS:
		ptr = "OP_READPOS        ";
		break;
	case OP_UNLOCK:
		ptr = "OP_UNLOCK         ";
		break;
	case OP_WRITE:
		ptr = "OP_WRITE          ";
		break;
	case OP_UPDATE:
		ptr = "OP_UPDATE         ";
		break;
	case OP_DELETE:
		ptr = "OP_DELETE         ";
		break;
	case OP_TABLELOCK:
		ptr = "OP_TABLELOCK      ";
		break;
	case OP_TABLEUNLOCK:
		ptr = "OP_TABLEUNLOCK    ";
		break;
	case OP_WORKINIT:
		ptr = "OP_WORKINIT       ";
		break;
	case OP_WORKNEWROW:
		ptr = "OP_WORKNEWROW     ";
		break;
	case OP_WORKFREE:
		ptr = "OP_WORKFREE       ";
		break;
	case OP_WORKGETROWID:
		ptr = "OP_WORKGETROWID   ";
		break;
	case OP_WORKSETROWID:
		ptr = "OP_WORKSETROWID   ";
		break;
	case OP_WORKGETROWCOUNT:
		ptr = "OP_WORKGETROWCOUNT";
		break;
	case OP_WORKSORT:
		ptr = "OP_WORKSORT       ";
		break;
	case OP_WORKUNIQUE:
		ptr = "OP_WORKUNIQUE     ";
		break;
	case OP_SORTSPEC:
		ptr = "OP_SORTSPEC       ";
		break;
	case OP_SORTSPECD:
		ptr = "OP_SORTSPECD      ";
		break;
	case OP_SET:
		ptr = "OP_SET            ";
		break;
	case OP_INCR:
		ptr = "OP_INCR           ";
		break;
	case OP_MOVE:
		ptr = "OP_MOVE           ";
		break;
	case OP_ADD:
		ptr = "OP_ADD            ";
		break;
	case OP_SUB:
		ptr = "OP_SUB            ";
		break;
	case OP_MULT:
		ptr = "OP_MULT           ";
		break;
	case OP_DIV:
		ptr = "OP_DIV            ";
		break;
	case OP_MOVETOCOL:
		ptr = "OP_MOVETOCOL      ";
		break;
	case OP_FPOSTOCOL:
		ptr = "OP_FPOSTOCOL      ";
		break;
	case OP_COLMOVE:
		ptr = "OP_COLMOVE        ";
		break;
	case OP_COLADD:
		ptr = "OP_COLADD         ";
		break;
	case OP_COLSUB:
		ptr = "OP_COLSUB         ";
		break;
	case OP_COLMULT:
		ptr = "OP_COLMULT        ";
		break;
	case OP_COLDIV:
		ptr = "OP_COLDIV         ";
		break;
	case OP_COLNEGATE:
		ptr = "OP_COLNEGATE      ";
		break;
	case OP_COLLOWER:
		ptr = "OP_COLLOWER       ";
		break;
	case OP_COLUPPER:
		ptr = "OP_COLUPPER       ";
		break;
	case OP_COLTRIM_T:
		ptr = "OP_COLTRIM_T      ";
		break;		
	case OP_COLTRIM_B:
		ptr = "OP_COLTRIM_B      ";
		break;	
	case OP_COLTRIM_L:
		ptr = "OP_COLTRIM_L      ";
		break;					
	case OP_COLSUBSTR_LEN:
		ptr = "OP_COLSUBSTR_LEN  ";
		break;		
	case OP_COLSUBSTR_POS:
		ptr = "OP_COLSUBSTR_POS  ";
		break;
	case OP_COLCAST:
		ptr = "OP_COLCAST        ";
		break;
	case OP_COLNULL:
		ptr = "OP_COLNULL        ";
		break;
	case OP_COLTEST:
		ptr = "OP_COLTEST        ";
		break;
	case OP_COLCOMPARE:
		ptr = "OP_COLCOMPARE     ";
		break;
	case OP_COLISNULL:
		ptr = "OP_COLISNULL      ";
		break;
	case OP_COLLIKE:
		ptr = "OP_COLLIKE        ";
		break;
	case OP_COLBININCR:
		ptr = "OP_COLBININCR     ";
		break;
	case OP_COLBINSET:
		ptr = "OP_COLBINSET      ";
		break;
	case OP_GOTO:
		ptr = "OP_GOTO           ";
		break;
	case OP_GOTOIFNOTZERO:
		ptr = "OP_GOTOIFNOTZERO  ";
		break;
	case OP_GOTOIFZERO:
		ptr = "OP_GOTOIFZERO     ";
		break;
	case OP_GOTOIFPOS:
		ptr = "OP_GOTOIFPOS      ";
		break;
	case OP_GOTOIFNOTPOS:
		ptr = "OP_GOTOIFNOTPOS   ";
		break;
	case OP_GOTOIFNEG:
		ptr = "OP_GOTOIFNEG      ";
		break;
	case OP_GOTOIFNOTNEG:
		ptr = "OP_GOTOIFNOTNEG   ";
		break;
	case OP_CALL:
		ptr = "OP_CALL           ";
		break;
	case OP_RETURN:
		ptr = "OP_RETURN         ";
		break;
	case OP_FINISH:
		ptr = "OP_FINISH         ";
		break;
	case OP_TERMINATE:
		ptr = "OP_TERMINATE      ";
		break;
	default:
		ptr = "UNKNOWN           ";
		break;
	}	
	sprintf(debugarea, "%s: 0x%04X %s 0x%08X 0x%08X 0x%08X", str, pcount, ptr, pgm->op1, pgm->op2, pgm->op3);
	debug1(debugarea);
}

static void addpgm0(int code)
{
	addpgm3(code, 0, 0, 0);
}

static void addpgm1(int code, int op1)
{
	addpgm3(code, op1, 0, 0);
}

static void addpgm2(int code, int op1, int op2)
{
	addpgm3(code, op1, op2, 0);
}

static void addpgm3(int code, int op1, int op2, int op3)
{
	INT i1;

	if (execerror) return;
	switch (code) {
	case OP_GOTO:
	case OP_GOTOIFNOTZERO:
	case OP_GOTOIFZERO:
	case OP_GOTOIFPOS:
	case OP_GOTOIFNOTPOS:
	case OP_GOTOIFNEG:
	case OP_GOTOIFNOTNEG:
	case OP_CALL:
		if (op1 >= LOWESTLABEL) {
			/* see if label has been defined */
			for (i1 = 0; i1 < labeldefcount && op1 != labeldefname[i1]; i1++);
			if (i1 < labeldefcount) op1 = labeldefvalue[i1];
			else {  /* not defined */
				if (labelrefcount >= MAXLABELREFS) {
					sqlerrnummsg(SQLERR_NOMEM, "too many label references", NULL);
					execerror = TRUE;
					return;
				}
				labelrefname[labelrefcount] = op1;
				labelrefvalue[labelrefcount++] = pgmcount;
			}
		}
	}
	if (pgmcount == pgmalloc) {
		if (memchange((unsigned char **) hpgm, (pgmalloc + 100) * sizeof(PCODE), 0) == -1) {
			sqlerrnum(SQLERR_NOMEM);
			execerror = TRUE;
			return;
		}
		pgmalloc += 100;
	}
	(*hpgm)[pgmcount].code = code;
	(*hpgm)[pgmcount].op1 = op1;
	(*hpgm)[pgmcount].op2 = op2;
	(*hpgm)[pgmcount].op3 = op3;
#if FS_DEBUGCODE & defined(_DEBUG)
	if (fsflags & FSFLAGS_DEBUG3) debugcode("WORK", pgmcount, &((*hpgm)[pgmcount]));
#endif
	pgmcount++;
}
