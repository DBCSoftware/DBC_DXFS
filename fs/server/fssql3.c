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
#include "fio.h"
#include "fsrun.h"
#include "fssql.h"
#include "fssqlx.h"
#include "util.h"

#define MAX_SYM_SIZE 511
#define MAX_LIT_SIZE 32767
#define MAX_TABLES_IN_SELECT 15
#define MAX_ORDERBY 15
#define MAX_GROUPBY 15

/* flag arguments for scansymbol */
#define PARSE_PLUS	0x01
#define PARSE_MINUS	0x02

/* SQL symbol values. Returned from scansymbol and peeksymbol. */
#define SERROR			(-1)			/* error occurred during scanning */
#define SEND			0				/* end of SQL statement */

#define SADD			1
#define SAFTER			2
#define SID				3				/* symbol is an identifier but not an SQL keyword */
#define SALL			4
#define SALTER			5
#define SAND			6
#define SASSOCIATIVE	7
#define SAS				8
#define SASC			9
#define SAVG			10
#define SBETWEEN		11
#define SBOTH			12
#define SBY				13
#define SCAST			14
#define SCHANGE			15
#define SCOLUMN			16
#define SCOUNT			17
#define SCREATE         18
#define SDELETE			19
#define SDESC			20
#define SDISTINCT		21
#define SDROP           22
#define SESCAPE			23
#define SEXISTS			24
#define SFIRST			25
#define SFOR			26
#define SFROM			27
#define SGROUP			28
#define SHAVING			29
#define SIF				30
#define SIN				31
#define SINDEX			32
#define SINNER			33
#define SINSERT			34
#define SINTO			35
#define SIS				36
#define SJOIN			37
#define SLEADING		38
#define SLEFT			39
#define SLIKE			40
#define SLOCK			41
#define SLOWER			42
#define SMAX			43
#define SMIN			44
#define SMODIFY			45
#define SNOT			46
#define SNOWAIT			47
#define SNULL			48
#define SON				49
#define SOR				50
#define SORDER			51
#define SOUTER			52
#define SREAD			53
#define SRENAME			54
#define SSELECT			55
#define SSET			56
#define SSUBSTRING		57
#define SSUM			58
#define STABLE			59
#define STEXTFILETYPE   60
#define STEXTFILE		61
#define STO				62
#define STRAILING		63
#define STRIM			64
#define SUNIQUE			65
#define SUNLOCK			66
#define SUPDATE			67
#define SUPPER			68
#define SVALUES			69
#define SWHERE			70

#define SNUMBER			80				/* number */
#define SLITERAL		81				/* literal */

#define SPERIOD			82				/* . */
#define SCOMMA			83				/* , */
#define SLPAREN			84				/* ( */
#define SRPAREN			85				/* ) */
#define SQUESTION		86				/* ? */
#define SPLUS			87				/* + */
#define SMINUS			88				/* - */
#define SHYPHEN			SMINUS
#define SASTERISK		89				/* * */
#define SDIVIDE			90				/* / */
#define SSLASH			SDIVIDE
#define SEQUAL			91				/* = */
#define SLESS			92				/* < */
#define SGREATER		93				/* > */
#define SLESSEQUAL		94				/* <= */
#define SGREATEREQUAL	95				/* >= */
#define SNOTEQUAL		96				/* <> */
#define SCONCAT			97				/* || */
#define SCOLON			98				/* :  */

/* SQL category values. Set by scansymbol and peeksymbol. */
#define CERROR			(-1)			/* error */
#define CEND			0				/* end of SQL statement */
/*** CODE: NOT CONVINCED WE NEED CID, WHICH IS A SUPERSET OF SQL KEYWORDS & SID ***/
#define CID				1				/* identifier */
#define CNUMBER			2				/* number */
#define CLITERAL		3				/* literal */
#define COTHER			4				/* none of the above */

#define NUMSQLKEYWORDS (sizeof sqlsymbols / sizeof sqlsymbols[0])

#define LEXPSIZE 256
#define AEXPSIZE 64
#define MORELEXP(l, n) (n < (*l)->max || morelexp(l, n))
#define MOREAEXP(a, n) (n < (*a)->max || moreaexp(a, n))

#define PARSE_SELECT	0x01
#define PARSE_UPDATE	0x02
#define PARSE_DELETE	0x04
#define PARSE_ON		0x10
#define PARSE_WHERE		0x20
#define PARSE_HAVING	0x40

/* description of a column in SELECT column list, WHERE, ORDER BY, GROUP BY */
typedef struct COLDESC {
	char columnname[MAX_NAME_LENGTH + 1];
	char qualname[MAX_NAME_LENGTH + 1];	/* table name or correlation name */
} COLDESC;

/* description of temporary work variable for logical expression entries */
typedef struct LEXPWORK {
	int max;
	LEXPENTRY lexpentries[1];
} LEXPWORK;

/* description of temporary work variable for mathematical expression entries */
typedef struct AEXPWORK {
	int max;
	AEXPENTRY aexpentries[1];
} AEXPWORK;

/**
 * string containing all delimiter characters. Includes all punctuation and operator
 * characters plus the apostrophe for literals and a space, carriage return and
 * line feed. These characters are used to delimit words.
 */
#define SDELIMSTRING   ".,()?+-*/=<>|' \r\n"
/**
 * String containing characters that are allowed in a 'delimited identifier' in addition to letters and numbers
 */
#define SDELIMSTRING2 "_@#$~%-!{}^'()\\` "

/* global variables */
extern CONNECTION connection;

/* Normally we would keep a pointer to the current scan point in the SQL string. However,
   the string is stored in SW memory, so it can move. Instead we keep the pointer to
   the pointer to the statement and an offset to the current scan point. */

/* local variables */
static UCHAR *sqlstatement;			/* pointer to SQL statement, null-terminated */
static INT sqlstmtlength;			/* length of the sqlstatement */
static INT sqloffset;				/* offset to current scan point, 0 means beginning */
static INT sqllastoffset;			/* offset of last scanned symbol */
static INT sqlsymtype;				/* identifier of scanned symbol */
static INT sqlcattype;				/* category of scanned symbol */
static CHAR sqlsymbol[MAX_SYM_SIZE + 1];	/* scanned symbol */
static UCHAR sqlliteral[MAX_LIT_SIZE + 1];	/* literal or number */
static INT sqllength;				/* length of sqlsymbol or sqlliteral */


/* Table for determining SQL keywords */
static struct {
	INT symtype;
	INT length;
	CHAR *name;
} sqlsymbols[] = {
	SADD,          3, "ADD",
	SAFTER,        5, "AFTER",
	SALL,          3, "ALL",
	SALTER,        5, "ALTER",
	SAND,          3, "AND",
	SAS,           2, "AS",
	SASC,          3, "ASC",
	SASSOCIATIVE, 11, "ASSOCIATIVE",
	SAVG,          3, "AVG",
	SBETWEEN,      7, "BETWEEN",
	SBOTH,         4, "BOTH",
	SBY,           2, "BY",
	SCAST,         4, "CAST",
	SCHANGE,       6, "CHANGE",
	SCOLUMN,       6, "COLUMN",
	SCOUNT,        5, "COUNT",
	SCREATE,       6, "CREATE",
	SDELETE,       6, "DELETE",
	SDESC,         4, "DESC",
	SDISTINCT,     8, "DISTINCT",
	SDROP,         4, "DROP",
	SESCAPE,       6, "ESCAPE",
	SEXISTS,       6, "EXISTS",
	SFIRST,        5, "FIRST",
	SFOR,          3, "FOR",
	SFROM,         4, "FROM",
	SGROUP,        5, "GROUP",
	SHAVING,       6, "HAVING",
	SIF,           2, "IF",
	SIN,           2, "IN",
	SINDEX,        5, "INDEX",
	SINNER,        5, "INNER",
	SINSERT,       6, "INSERT",
	SINTO,         4, "INTO",
	SIS,           2, "IS",
	SJOIN,         4, "JOIN",
	SLEADING,      7, "LEADING",
	SLEFT,         4, "LEFT",
	SLIKE,         4, "LIKE",
	SLOCK,         4, "LOCK",
	SLOWER,        5, "LOWER",
	SMAX,          3, "MAX",
	SMIN,          3, "MIN",
	SMODIFY,       6, "MODIFY",
	SNOT,          3, "NOT",
	SNOWAIT,       6, "NOWAIT",
	SNULL,         4, "NULL",
	SON,           2, "ON",
	SOR,           2, "OR",
	SORDER,        5, "ORDER",
	SOUTER,        5, "OUTER",
	SREAD,         4, "READ",
	SRENAME,       6, "RENAME",
	SSELECT,       6, "SELECT",
	SSET,          3, "SET",
	SSUBSTRING,    9, "SUBSTRING",
	SSUM,          3, "SUM",
	STABLE,        5, "TABLE",
	STEXTFILETYPE, 12, "TEXTFILETYPE",
	STEXTFILE,     8, "TEXTFILE",
	STO,           2, "TO",
	STRAILING,     8, "TRAILING",
	STRIM,         4, "TRIM",
	SUNIQUE,       6, "UNIQUE",
	SUNLOCK,       6, "UNLOCK",
	SUPDATE,       6, "UPDATE",
	SUPPER,        5, "UPPER",
	SVALUES,       6, "VALUES",
	SWHERE,        5, "WHERE"
};

/* prototypes */
static int parseselect(S_GENERIC ***, INT);
static int parseinsert(S_GENERIC ***);
static int parseupdate(S_GENERIC ***);
static int parsedelete(S_GENERIC ***);
static int parsecreate(S_GENERIC ***);
static int parsedrop(S_GENERIC ***);
static int parsealter(S_GENERIC ***);
static int parselock(S_GENERIC ***, INT stype);
static int scansymbol(INT);
static int issqlkeyword(void);
static int findindexnum(TABLE *, CHAR *);
static int findtablenum(CHAR *);
static int findcolumnintable(int, CHAR *);
static UINT findcolumn(COLDESC *, CORRTABLE *, INT);
static int parsecolumnname(COLDESC *);
static int peeksymbol(void);
static S_WHERE **parsewhere(INT type, HCORRTABLE, INT *);
static INT parseors(INT type, HCORRTABLE, INT *, INT, S_WHERE *, LEXPWORK **);
static INT parseands(INT type, HCORRTABLE, INT *, INT, S_WHERE *, LEXPWORK **);
static INT parsenots(INT type, HCORRTABLE, INT *, INT, S_WHERE *, LEXPWORK **);
static INT parsepredicates(INT type, HCORRTABLE, INT *, INT, S_WHERE *, LEXPWORK **);
static INT parsevalues(INT type, HCORRTABLE, INT *, INT, S_WHERE *, LEXPWORK **);
static INT morelexp(LEXPWORK **, INT);
static S_AEXP **parseaexp(INT type, HCORRTABLE, INT *);
static INT aexp(INT type, HCORRTABLE, INT *, INT *, AEXPWORK **);
static INT aterm(INT type, HCORRTABLE, INT *, INT *, AEXPWORK **);
static INT asign(INT type, HCORRTABLE, INT *, INT *, AEXPWORK **);
static INT afactor(INT type, HCORRTABLE, INT *, INT *, AEXPWORK **);
static INT moreaexp(AEXPWORK **, INT);
static INT validatesqlname(CHAR *, INT, INT isDelimitedIdentifier);
static INT validatenumeric(COLUMN *, INT, S_VALUE **);
static void strucpy(CHAR *dest, CHAR *src);

/**
 * Return 0 if success
 * otherwise return SQLERR_PARSE_xxxx
 * Called from module fssql2 from two places, execstmt, and posupdate
 */
INT parsesqlstmt(UCHAR *stmtstring, INT stmtlen, S_GENERIC ***stmt)
{
	int rc, symtype;

	stmtstring[stmtlen] = '\0';
	sqlstatement = stmtstring;
	sqlstmtlength = stmtlen;
	sqloffset = 0;
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
	switch (symtype) {
	case SSELECT:
		rc = parseselect(stmt, FALSE);
		break;
	case SINSERT:
		rc = parseinsert(stmt);
		break;
	case SUPDATE:
		rc = parseupdate(stmt);
		break;
	case SDELETE:
		rc = parsedelete(stmt);
		break;
	case SLOCK:
		rc = parselock(stmt, STYPE_LOCK);
		break;
	case SUNLOCK:
		rc = parselock(stmt, STYPE_UNLOCK);
		break;
	case SCREATE:
		rc = parsecreate(stmt);
		break;
	case SDROP:
		rc = parsedrop(stmt);
		break;
	case SALTER:
		rc = parsealter(stmt);
		break;
	default:
		return sqlerrnummsg(SQLERR_PARSE_ERROR, "Invalid or unsupported SQL statement", (CHAR *) stmtstring);
	}
	return rc;
}

/**
 * Called only from execnonestmt in fssql2
 */
INT scanselectstmt(UCHAR *stmtstring, INT stmtlen, S_GENERIC ***stmt)
{
	int rc, symtype;

	stmtstring[stmtlen] = '\0';
	sqlstatement = stmtstring;
	sqlstmtlength = stmtlen;
	sqloffset = 0;
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
	if (symtype != SSELECT) return sqlerrnummsg(SQLERR_PARSE_ERROR, "Expecting SQL SELECT statement", (CHAR *) stmtstring);
	rc = parseselect(stmt, TRUE);
	return rc;
}

/* free the memory of a statement */
void freestmt(S_GENERIC **stmt)
{
	int i1;
	S_SELECT **hselect;
	S_INSERT **hinsert;
	S_UPDATE **hupdate;
	S_DELETE **hdelete;
	S_LOCK **hlock;
	S_SETCOLUMN **hsetcolumn, **setcolumnarray;
	S_WHERE **hwhere;
	S_AEXP **haexp;
	S_VALUE ****valuesarray;
	S_GENERIC ****specialcolumns;

	if (stmt == NULL) return;

	/* don't recurse for S_VALUE structures */
	switch ((*stmt)->stype) {
	case STYPE_SELECT:
		hselect = (S_SELECT **) stmt;
		for (i1 = 0; i1 < (*hselect)->numtables; i1++)
			freestmt((S_GENERIC **)(*hselect)->joinonconditions[i1]);
		memfree((UCHAR **)(*hselect)->tabcolnumarray);
		if ((*hselect)->specialcolumns != NULL) {
			specialcolumns = (*hselect)->specialcolumns;
			for (i1 = 0; i1 < (*hselect)->numspecialcolumns; i1++)
				freestmt((*specialcolumns)[i1]);
			memfree((UCHAR **) specialcolumns);
		}
		freestmt((S_GENERIC **)(*hselect)->where);
		memfree((UCHAR **)(*hselect)->ordertabcolnumarray);
		memfree((UCHAR **)(*hselect)->orderascflagarray);
		memfree((UCHAR **)(*hselect)->setfunctionarray);
		memfree((UCHAR **)(*hselect)->grouptabcolnumarray);
		freestmt((S_GENERIC **)(*hselect)->having);
		memfree((UCHAR **)(*hselect)->corrtable);
		break;
	case STYPE_INSERT:
		hinsert = (S_INSERT **) stmt;
		memfree((unsigned char **)(*hinsert)->tabcolnumarray);
		if ((*hinsert)->valuesarray != NULL) {
			valuesarray = (*hinsert)->valuesarray;
			for (i1 = 0; i1 < (*hinsert)->numvalues; i1++)
				memfree((unsigned char **)(*valuesarray)[i1]);
			memfree((unsigned char **) valuesarray);
		}
		break;
	case STYPE_UPDATE:
		hupdate = (S_UPDATE **) stmt;
		if ((*hupdate)->setcolumnarray != NULL) {
			setcolumnarray = (*hupdate)->setcolumnarray;
			for (i1 = 0; i1 < (*hupdate)->numcolumns; i1++) {
				memfree((unsigned char **)(*setcolumnarray)[i1].valuep);
				freestmt((S_GENERIC **)(*setcolumnarray)[i1].expp);
			}
			memfree((unsigned char **) setcolumnarray);
		}
		freestmt((S_GENERIC **)(*hupdate)->where);
		break;
	case STYPE_DELETE:
		hdelete = (S_DELETE **) stmt;
		freestmt((S_GENERIC **)(*hdelete)->where);
		break;
	case STYPE_LOCK:
	case STYPE_UNLOCK:
		hlock = (S_LOCK **) stmt;
		break;
	case STYPE_SETCOLUMN:
		hsetcolumn = (S_SETCOLUMN **) stmt;
		memfree((unsigned char **)(*hsetcolumn)->valuep);
		freestmt((S_GENERIC **)(*hsetcolumn)->expp);
		break;
	case STYPE_WHERE:
		hwhere = (S_WHERE **) stmt;
		for (i1 = 0; i1 < (*hwhere)->numentries; i1++) {
			if ((*hwhere)->lexpentries[i1].type == OPVALUE)
				memfree((UCHAR **)(*hwhere)->lexpentries[i1].valuep);
			else if ((*hwhere)->lexpentries[i1].type == OPEXP)
				freestmt((S_GENERIC **)(*hwhere)->lexpentries[i1].expp);
		}
		break;
	case STYPE_AEXP:
		haexp = (S_AEXP **) stmt;
		for (i1 = 0; i1 < (*haexp)->numentries; i1++) {
			if ((*haexp)->aexpentries[i1].type == OPVALUE)
				memfree((UCHAR **)(*haexp)->aexpentries[i1].valuep);
		}
		break;
	case STYPE_ALTER:
	case STYPE_CREATE:
	case STYPE_DROP:
		/* nothing to do */
		break;
	}
	memfree((UCHAR **) stmt);
}

/*	parseselect */
/*		Parse an SQL SELECT statement. */
/*		SELECT [ALL | DISTINCT] {* | columnref [,columnref] ...} FROM tableref [,tableref] ... */
/*		[WHERE search-condition] [ORDER BY {columnref | number} [ASC | DESC] ...] */
/*		[GROUP BY {columnref | number} ...] [FOR READ ONLY | FOR UPDATE] */
/* */
/*		columnref may be columnname or qualifier.columnname */
/*		tableref may be tablename or tablename [AS] correlationname */
/* */
/*	Exit: */
/*		statement parsed and described in the STATEMENT structure */
/* */
/*	Returns: */
/*		SQL_SUCCESS if okay, SQL_ERROR if syntax error */
static int parseselect(S_GENERIC ***stmt, INT scanonlyflag)
{
 	INT i1, i2, i3, i4, i5, op;
	INT symtype;					/* type of scanned symbol */
	INT numalloccolumns;			/* number of column references allocated */
	INT numcolumns;					/* number of columns selected */
	INT numtables;					/* number of tables given in FROM list */
	INT tablenum;					/* tablenum (number in connection array) */
	INT tableref;					/* table ref number (number in plan code) */
	INT columnnum;					/* columnnum */
	INT numexp, numgroupcolumns, numordercolumns, numsetfunc, numspecial;
	INT parencount, sqloffsetatbegin, sqloffsetaftertables, state;
	UINT tabcolnum;
	UINT orderbytabcolnums[MAX_ORDERBY];
	UINT groupbytabcolnums[MAX_GROUPBY];
	UINT **workarray;
	CHAR name[MAX_NAME_LENGTH + 1], *ptr1, *ptr2;
	UCHAR orderbyascflag[MAX_ORDERBY];
	UCHAR **cworkarray;
	S_SELECT **selectp;				/* pointer to SELECT structure */
	COLDESC columndesc;				/* column description */
	HCORRTABLE corrtable;
	S_GENERIC ****specialarray;
	S_VALUE **svalue;
	S_AEXP **saexp;
	S_WHERE **swhere;
	TABLE *tab1;
	LEXPENTRY *lexpentries;
	AEXPENTRY *aexpentries;
	SETFUNC **setfuncarray;

	/* allocate a structure to describe the SELECT statement */
	selectp = (S_SELECT **) memalloc(sizeof(S_SELECT), MEMFLAGS_ZEROFILL);
	if (selectp == NULL) return sqlerrnum(SQLERR_PARSE_NOMEM);
	(*selectp)->stype = STYPE_SELECT;

	/* this must be set here in case error occurs */
	numtables = 0;
	numspecial = 0;

	/* allocate memory for table/column aliases */
	corrtable = (HCORRTABLE) memalloc(sizeof(CORRTABLE) + (32 - 1) * sizeof(CORRINFO), 0);
	if (corrtable == NULL) {  /* memory allocation failed */
		sqlerrnum(SQLERR_PARSE_NOMEM);
		goto procerror;
	}
	(*selectp)->corrtable = corrtable;
	(*corrtable)->size = 32;
	(*corrtable)->count = 0;

	/* skip to the FROM so we can parse of the table names first */
	sqloffsetatbegin = sqloffset;
	for (parencount = state = 0; ; ) {
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
		if (symtype == SFROM) break;
		else if (symtype == SEND) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "no table reference, expected FROM tableref", NULL);
			goto procerror;
		}
		else if (symtype == SCAST || symtype == SSUBSTRING || symtype == STRIM) {
			/* These operators contain the keyword 'FROM', so parse pass them here */
			/* to keep finite state machine from breaking */
			if (scansymbol(PARSE_PLUS | PARSE_MINUS) != SLPAREN) {
				if (symtype == SCAST) sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid syntax for CAST", (CHAR *) sqlsymbol);
				else if (symtype == SSUBSTRING) sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid syntax for SUBSTRING", (CHAR *) sqlsymbol);
				else if (symtype == STRIM) sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid syntax for TRIM", (CHAR *) sqlsymbol);
				goto procerror;
			}
			for (i1 = 1; i1 > 0; ) {
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (symtype == SRPAREN) --i1;
				else if (symtype == SLPAREN) ++i1;
				else if (symtype == SERROR || symtype == SEND) {
					if (symtype == SCAST) sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid syntax for CAST", (CHAR *) sqlsymbol);
					else if (symtype == SSUBSTRING) sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid syntax for SUBSTRING", (CHAR *) sqlsymbol);
					else if (symtype == STRIM) sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid syntax for TRIM", (CHAR *) sqlsymbol);
					goto procerror;
				}
			}
			state = 0;
			continue;
		}
		/* save column alias names */
		if (!parencount) {
			if (symtype == SAS) state = 1;
			else if (sqlcattype == CID) {
				if (state == 1 && validatesqlname(sqlsymbol, sqllength, FALSE)) {
					if ((*corrtable)->count == (*corrtable)->size) {
						if (memchange((UCHAR **) corrtable, sizeof(CORRTABLE) + (((*corrtable)->size << 1) - 1) * sizeof(CORRINFO), 0) == -1) {
							sqlerrnum(SQLERR_PARSE_NOMEM);
							goto procerror;
						}
						(*corrtable)->size <<= 1;
					}
					(*corrtable)->info[(*corrtable)->count].tablenum = 0;
					(*corrtable)->info[(*corrtable)->count].tabcolnum = 0;
					strucpy((*corrtable)->info[(*corrtable)->count++].name, sqlsymbol);
					state = 0;
				}
				else state = 1;
			}
			else if (symtype == SNUMBER || symtype == SLITERAL) state = 1;
			else if (symtype == SLPAREN) parencount++;
			else state = 0;
		}
		else if (symtype == SRPAREN && !--parencount) state = 1;
		else if (symtype == SLPAREN) parencount++;
	}

	/* process comma-delimited list of table references */
	parencount = 0;
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get first table name */
	for ( ; ; ) {
		if (symtype == SLPAREN) {
			parencount++;
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			continue;
		}

		if (sqlcattype != CID && sqlcattype != CLITERAL) {  /* not an identifier */
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected table name after FROM keyword", NULL);
			goto procerror;
		}
		if (sqlcattype == CLITERAL) i1 = validatesqlname((CHAR*) sqlliteral, sqllength, TRUE);
		else i1 = validatesqlname(sqlsymbol, sqllength, FALSE);
		if (!i1) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid table name", (CHAR *) sqlsymbol);
			goto procerror;
		}
		if (numtables == MAX_SELECT_TABLES) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "exceeded maximum supported number of JOINS", NULL);
			goto procerror;
		}
		if (sqlcattype == CLITERAL) strucpy(name, (CHAR*) sqlliteral);
		else strucpy(name, sqlsymbol);
		/* see if the table is valid (exists in the data source) and get its table handle */
		tablenum = findtablenum(name);
		if (!tablenum) {
			sqlerrnummsg(SQLERR_PARSE_TABLENOTFOUND, sqlsymbol, NULL);
			goto procerror;
		}
		tab1 = tableptr(tablenum);
		if ((tab1->flags & TABLE_FLAGS_TEMPLATE) && maketemplate(name, &tablenum)) goto procerror;
		if ((*corrtable)->count == (*corrtable)->size) {
			if (memchange((UCHAR **) corrtable, sizeof(CORRTABLE) + (((*corrtable)->size << 1) - 1) * sizeof(CORRINFO), 0) == -1) {
				sqlerrnum(SQLERR_PARSE_NOMEM);
				goto procerror;
			}
			(*corrtable)->size <<= 1;
		}
		(*corrtable)->info[(*corrtable)->count].tablenum = tablenum;
		(*corrtable)->info[(*corrtable)->count].tabcolnum = 0;
		strcpy((*corrtable)->info[(*corrtable)->count++].name, name);
		numtables++;
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* check for AS or correlation name */
		/* if next keyword is WHERE/ORDER/FOR, consider this as the end of the table names,  */
		/* can't use these names as correlation names */
		if (symtype == SEND || symtype == SWHERE || symtype == SORDER || symtype == SGROUP || symtype == SFOR) {
			sqloffsetaftertables = sqllastoffset;  /* save this position to restart scanning after table list */
			break;
		}
		if (symtype == SAS) {
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get correlation name */
			if (sqlcattype != CID) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid table correlation name", NULL);
				goto procerror;
			}
		}
		if (symtype != SON && symtype != SCOMMA && symtype != SINNER && symtype != SJOIN  && symtype != SLEFT && symtype != SRPAREN) {
			if (sqlcattype == CID) {  /* correlation name */
				if (!validatesqlname(sqlsymbol, sqllength, FALSE)) {	/* validate the name */
					sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid table correlation name", sqlsymbol);
					goto procerror;
				}
				strucpy(name, sqlsymbol);
				for (i1 = 0, i2 = (*corrtable)->count - 1; i1 < i2; i1++) {
					if (!(*corrtable)->info[i1].tablenum) continue;
					if (!strcmp(name, (*corrtable)->info[i1].name)) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "table correlation/alias already being used", NULL);
						goto procerror;
					}
				}
				/* overwrite real table name */
				strcpy((*corrtable)->info[(*corrtable)->count - 1].name, name);
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			}
		}
		if (symtype == SON) {
			if (numtables < 2) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "At least 2 tables required for ON join condition", NULL);
				goto procerror;
			}
			swhere = parsewhere(PARSE_SELECT | PARSE_ON, corrtable, &symtype);
			if (swhere == NULL) goto procerror;
			(*selectp)->joinonconditions[numtables - 1] = swhere;
			ptr1 = "syntax error after ON join condition";
		}
		else ptr1 = "syntax error after last table name";
		if (symtype == SRPAREN) {
			if (!parencount) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "Unexpected right parenthesis", NULL);
				goto procerror;
			}
			parencount--;
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
		}
		if (symtype == SEND || symtype == SWHERE || symtype == SORDER || symtype == SGROUP || symtype == SFOR) {
			sqloffsetaftertables = sqllastoffset;  /* save this position to restart scanning after table list */
			break;
		}
		if (symtype == SCOMMA) (*selectp)->tablejointypes[numtables - 1] = JOIN_INNER;  /* old style INNER JOIN */
		else {
			if (symtype == SINNER) {
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (symtype != SJOIN) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword JOIN after keyword INNER", NULL);
					goto procerror;
				}
			}
			if (symtype == SJOIN) (*selectp)->tablejointypes[numtables - 1] = JOIN_INNER;
			else if (symtype == SLEFT) {
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (symtype != SOUTER) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword OUTER after keyword LEFT", NULL);
					goto procerror;
				}
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (symtype != SJOIN) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword JOIN after keywords LEFT OUTER", NULL);
					goto procerror;
				}
				(*selectp)->tablejointypes[numtables - 1] = JOIN_LEFT_OUTER;
			}
			else {
				sqlerrnummsg(SQLERR_PARSE_ERROR, ptr1, NULL);
				goto procerror;
			}
		}
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get next table name */
	}
	if (parencount) {
		sqlerrnummsg(SQLERR_PARSE_ERROR, "expected right parenthesis in table reference specification", NULL);
		goto procerror;
	}
	(*selectp)->numtables = numtables;	 /* number of tables */
	for (i1 = i3 = 0, i2 = (*corrtable)->count; i1 < i2; i1++) {
		if (!(*corrtable)->info[i1].tablenum) continue;
		(*selectp)->tablenums[i3++] = (*corrtable)->info[i1].tablenum;
	}
	if (i3 != numtables) {
		sqlerrnummsg(SQLERR_INTERNAL, "number of tables does not match correlation count", NULL);
		goto procerror;
	}

	/* go back and scan the columns */
	sqloffset = sqloffsetatbegin;
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
	if (symtype == SALL || symtype == SDISTINCT) {	/* SELECT ALL or DISTINCT */
		if (symtype == SDISTINCT) (*selectp)->distinctflag = TRUE;
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get next symbol, should be * or first column */
	}
	numcolumns = 0;
	if (symtype == SASTERISK) {  /* SELECT * */
		for (tableref = 0; tableref < numtables; tableref++) {	/* for each table in FROM list */
			tablenum = (*selectp)->tablenums[tableref];
			tab1 = tableptr(tablenum);
			numcolumns += tab1->numcolumns;
		}
		workarray = (UINT **) memalloc(numcolumns * sizeof(UINT), 0);
		if (workarray == NULL) {
			sqlerrnum(SQLERR_PARSE_NOMEM);
			goto procerror;
		}
		(*selectp)->tabcolnumarray = workarray;
		for (i1 = 0, tableref = 0; ++tableref <= numtables; ) {  /* for each table in FROM list */
			tablenum = (*selectp)->tablenums[tableref - 1];
			tab1 = tableptr(tablenum);
			for (columnnum = 0; ++columnnum <= tab1->numcolumns; )
				(*workarray)[i1++] = gettabcolref(tableref, columnnum);
		}
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
	}
	else {
		/* process comma-delimited column list. The columns can be specified as  */
		/* columnname or as qualifier.columnname */
		numalloccolumns = 20;
		workarray = (UINT **) memalloc(numalloccolumns * sizeof(UINT), 0);
		if (workarray == NULL) {
			sqlerrnum(SQLERR_PARSE_NOMEM);
			goto procerror;
		}
		(*selectp)->tabcolnumarray = workarray;
		for ( ; ; ) {	/* have next column name already scanned */
			if (numcolumns == numalloccolumns) {
				numalloccolumns += 20;
				if (memchange((UCHAR **) workarray, numalloccolumns * sizeof(UINT), 0)) {
					sqlerrnum(SQLERR_PARSE_NOMEM);
					goto procerror;
				}
			}
			sqloffsetatbegin = sqllastoffset;  /* save in case of expression with "as" */
			tabcolnum = 0;
			if (symtype == SLITERAL) {  /* column is a literal */
				if (!numspecial) {
					specialarray = (S_GENERIC ****) memalloc(sizeof(S_GENERIC **), 0);
					if (specialarray == NULL) {
						sqlerrnum(SQLERR_PARSE_NOMEM);
						goto procerror;
					}
					(*selectp)->specialcolumns = specialarray;
				}
				else if (memchange((UCHAR **) specialarray, (numspecial + 1) * sizeof(S_GENERIC **), 0)) {
					sqlerrnum(SQLERR_PARSE_NOMEM);
					goto procerror;
				}
				ptr1 = (char *) sqlstatement + sqloffset;
				while (*ptr1 == ' ' || *ptr1 == '\r' || *ptr1 == '\n') ptr1++;
				if (*ptr1 == '|' && *(ptr1 + 1) == '|') {
					/* literal is part of concatenation expression */
					saexp = parseaexp(PARSE_SELECT, corrtable, &symtype);
					if (saexp == NULL) goto procerror;
					(*workarray)[numcolumns++] = tabcolnum = gettabcolref(TABREF_LITEXP, numspecial);
					(*specialarray)[numspecial++] = (S_GENERIC **) saexp;
				}
				else {
					svalue = (S_VALUE **) memalloc(sizeof(S_VALUE) + sqllength - 1, 0);
					if (svalue == NULL) {
						sqlerrnum(SQLERR_PARSE_NOMEM);
						goto procerror;
					}
					(*svalue)->stype = STYPE_VALUE;
					(*svalue)->length = sqllength;
					(*svalue)->litoffset = 0;
					(*svalue)->escape[0] = '\0';
					memcpy((*svalue)->data, sqlliteral, sqllength);
					(*workarray)[numcolumns++] = tabcolnum = gettabcolref(TABREF_LITEXP, numspecial);
					(*specialarray)[numspecial++] = (S_GENERIC **) svalue;
					symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				}
			}
			else {
				/* i1 is used as a temporary flag to signify expression */
				if (symtype == SSUM || symtype == SAVG || symtype == SMIN || symtype == SMAX ||
						symtype == SCOUNT || symtype == SLOWER || symtype == SUPPER ||
						symtype == SCAST || symtype == SSUBSTRING || symtype == STRIM) {
					/* expression */
					i1 = TRUE;
				}
				else if (sqlcattype == CID) {
					ptr1 = (char *) sqlstatement + sqloffset;
					if (*ptr1 == '.' && isalpha(ptr1[1])) {  /* table.column */
						ptr1++;
						ptr2 = strpbrk(ptr1, SDELIMSTRING);
						ptr1 += (ptr2 == NULL) ? (ptrdiff_t)strlen(ptr1) : (ptr2 - ptr1);
					}
					while (*ptr1 == ' ' || *ptr1 == '\r' || *ptr1 == '\n') ptr1++;
					if (*ptr1 == '+' || *ptr1 == '-' || *ptr1 == '/' || *ptr1 == '*' || (*ptr1 == '|' && *(ptr1 + 1) == '|')) {
						/* column is part of expression */
						i1 = TRUE;
					}
					else i1 = FALSE;
				}
				else if (symtype == SNUMBER || symtype == SLPAREN) i1 = TRUE;
				else i1 = FALSE;
				if (i1) { /* expression */
					if (!numspecial) {
						specialarray = (S_GENERIC ****) memalloc(sizeof(S_GENERIC **), 0);
						if (specialarray == NULL) {
							sqlerrnum(SQLERR_PARSE_NOMEM);
							goto procerror;
						}
						(*selectp)->specialcolumns = specialarray;
					}
					else if (memchange((UCHAR **) specialarray, (numspecial + 1) * sizeof(S_GENERIC **), 0)) {
						sqlerrnum(SQLERR_PARSE_NOMEM);
						goto procerror;
					}
					saexp = parseaexp(PARSE_SELECT, corrtable, &symtype);
					if (saexp == NULL) goto procerror;
					(*workarray)[numcolumns++] = tabcolnum = gettabcolref(TABREF_LITEXP, numspecial);
					(*specialarray)[numspecial++] = (S_GENERIC **) saexp;
				}
				else {
					if (sqlcattype != CID) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "expected column name", NULL);
						goto procerror;
					}
					if (!validatesqlname(sqlsymbol, sqllength, FALSE)) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid column name", sqlsymbol);
						goto procerror;
					}
					if (parsecolumnname(&columndesc) != SQL_SUCCESS) goto procerror;
					if (columndesc.columnname[0] == '*') {  /* it is a table.* */
						for (tableref = 1, i1 = 0, i2 = (*corrtable)->count; i1 < i2; i1++) {
							if (!(*corrtable)->info[i1].tablenum) continue;
							if (!strcmp(columndesc.qualname, (*corrtable)->info[i1].name)) {
								tablenum = (*corrtable)->info[i1].tablenum;
								break;
							}
							tableref++;
						}
						if (i1 == i2) {
							sqlerrnummsg(SQLERR_PARSE_TABLENOTFOUND, columndesc.qualname, NULL);
							goto procerror;
						}
						tab1 = tableptr(tablenum);
						i1 = numcolumns + tab1->numcolumns;
						if (i1 > numalloccolumns) {
							if (memchange((UCHAR **) workarray, i1 * sizeof(UINT), 0)) {
								sqlerrnum(SQLERR_PARSE_NOMEM);
								goto procerror;
							}
							numalloccolumns = i1;
						}
						tab1 = tableptr(tablenum);
						for (columnnum = 1; columnnum <= tab1->numcolumns; columnnum++)
							(*workarray)[numcolumns++] = gettabcolref(tableref, columnnum);
					}
					else {  /* it is just a column */
						tabcolnum = findcolumn(&columndesc, *corrtable, FALSE);
						if (!tabcolnum) {
							sqlerrnummsg(SQLERR_PARSE_COLUMNNOTFOUND, columndesc.columnname, NULL);
							goto procerror;
						}
						(*workarray)[numcolumns++] = tabcolnum;
					}
					symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				}
			}
			if (symtype == SAS || (sqlcattype == CID && symtype != SFROM)) {
				if (!tabcolnum) sqlerrnummsg(SQLERR_PARSE_ERROR, "column correlation name on table.*", NULL);
				i3 = sqllastoffset;  /* save for below */
				if (symtype == SAS) {
					symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get correlation/alias name */
					if (sqlcattype != CID || symtype == SFROM) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid column correlation name", NULL);
						goto procerror;
					}
				}
				if (!validatesqlname(sqlsymbol, sqllength, FALSE)) {	/* validate the name */
					sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid column name", sqlsymbol);
					goto procerror;
				}
				strucpy(name, sqlsymbol);
				for (i1 = 0, i2 = (*corrtable)->count; i1 < i2; i1++) {
					if ((*corrtable)->info[i1].tablenum) continue;
					if (!strcmp(name, (*corrtable)->info[i1].name)) break;
				}
				if (i1 == i2) {  /* programming error if this happens */
					sqlerrnummsg(SQLERR_INTERNAL, "unable to find column alias", sqlsymbol);
					goto procerror;
				}
				if ((*corrtable)->info[i1].tabcolnum) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "duplicate column alias", sqlsymbol);
					goto procerror;
				}
				(*corrtable)->info[i1].tabcolnum = tabcolnum;
				(*corrtable)->info[i1].expstring = sqlstatement + sqloffsetatbegin;
				(*corrtable)->info[i1].explength = i3 - sqloffsetatbegin - 1;
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			}

			if (symtype != SCOMMA) break;
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
		}
	}

	/* fix up on clause and expressions that used column alias */
	for (i1 = 1; i1 < numtables; i1++) {
		if ((*selectp)->joinonconditions[i1] == NULL) continue;
		lexpentries = (*(*selectp)->joinonconditions[i1])->lexpentries;
		i3 = (*(*selectp)->joinonconditions[i1])->numentries;
		for (i2 = 0; i2 < i3; i2++) {
			if (lexpentries[i2].type != OPCOLUMN || gettabref(lexpentries[i2].tabcolnum)) continue;
			if (!(*corrtable)->info[lexpentries[i2].tabcolnum].tabcolnum) {  /* programming error if this happens */
				sqlerrnummsg(SQLERR_PARSE_ERROR, "column alias not found", sqlsymbol);
				goto procerror;
			}
			lexpentries[i2].tabcolnum = (*corrtable)->info[lexpentries[i2].tabcolnum].tabcolnum;
		}
	}
	for (i1 = 0; i1 < numspecial; i1++) {
		if ((*(S_GENERIC **)(*specialarray)[i1])->stype != STYPE_AEXP) continue;
		aexpentries = (*(S_AEXP **)(*specialarray)[i1])->aexpentries;
		i3 = (*(S_AEXP **)(*specialarray)[i1])->numentries;
		for (i2 = 0; i2 < i3; i2++) {
			if (aexpentries[i2].type != OPCOLUMN || gettabref(aexpentries[i2].tabcolnum)) continue;
			if (!(*corrtable)->info[aexpentries[i2].tabcolnum].tabcolnum) {  /* programming error if this happens */
				sqlerrnummsg(SQLERR_PARSE_ERROR, "column alias not found", sqlsymbol);
				goto procerror;
			}
			aexpentries[i2].tabcolnum = (*corrtable)->info[aexpentries[i2].tabcolnum].tabcolnum;
		}
	}
	/* remove any false column alias */
	for (i1 = i2 = 0; i1 < (*corrtable)->count; i1++) {
		if (!(*corrtable)->info[i1].tablenum && !(*corrtable)->info[i1].tabcolnum) continue;
		if (i1 != i2) (*corrtable)->info[i2] = (*corrtable)->info[i1];
		i2++;
	}
	(*corrtable)->count = i2;
	(*selectp)->numcolumns = numcolumns;
	(*selectp)->numspecialcolumns = numspecial;

	/* done processing columns, next symbol scanned, should be FROM */
	if (symtype != SFROM) {
		sqlerrnummsg(SQLERR_PARSE_ERROR, "expected FROM keyword after column reference specification", NULL);
		goto procerror;
	}

	if (!scanonlyflag) {
		/* skip FROM tables that we have already scan */
		sqloffset = sqloffsetaftertables;
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);

		/* process remaining clauses */
		while (symtype != SEND) {
			if (symtype == SWHERE) {
				/* process WHERE search conditions */
				if ((*selectp)->where != NULL) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "multiple WHERE's not supported", NULL);
					goto procerror;
				}
				swhere = parsewhere(PARSE_SELECT | PARSE_WHERE, corrtable, &symtype);
				if (swhere == NULL) goto procerror;
				(*selectp)->where = swhere;
			}
			else if (symtype == SORDER) {
				/* process ORDER BY column list */
				if ((*selectp)->numordercolumns) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "multiple ORDER BY's not supported", NULL);
					goto procerror;
				}
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get BY or first col spec */
				if (symtype == SBY) symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get first col spec */
				/* process comma-delimited column list. The columns can be specified as  */
				/* columnname, tablename.columnname or column number */
				numordercolumns = 0;
				for ( ; ; ) {	/* have next column name already scanned */
					if (symtype == SERROR || symtype == SEND) {	/* error or end of statement */
						sqlerrnummsg(SQLERR_PARSE_ERROR, "expected column reference after keywords ORDER BY", NULL);
						goto procerror;
					}
					if (numordercolumns == MAX_ORDERBY) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "exceeded maximum supported ORDER BY columns", NULL);
						goto procerror;
					}

					if (symtype == SNUMBER) { /* order column specified by index into selected columns */
						mscntoi(sqlliteral, &i1, sqllength);
						if (i1 < 1 || i1 > numcolumns) {
							sqlerrnummsg(SQLERR_PARSE_ERROR, "ORDER BY: invalid column number", NULL);
							goto procerror;
						}
						orderbytabcolnums[numordercolumns] = (*(*selectp)->tabcolnumarray)[i1 - 1];
					}
					else {
						if ((sqlcattype != CID) || (!validatesqlname(sqlsymbol, sqllength, FALSE))) {
							sqlerrnummsg(SQLERR_PARSE_ERROR, "ORDER BY: invalid column name", sqlsymbol);
							goto procerror;
						}
						if (parsecolumnname(&columndesc) != SQL_SUCCESS) goto procerror;
						/* see if the column is valid and get its column handle */
						tabcolnum = findcolumn(&columndesc, *corrtable, TRUE);
						if (!tabcolnum) {
							sqlerrnummsg(SQLERR_PARSE_COLUMNNOTFOUND, "ORDER BY", columndesc.columnname);
							goto procerror;
						}
						orderbytabcolnums[numordercolumns] = tabcolnum;
					}
					orderbyascflag[numordercolumns] = TRUE;
					symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* ASC/DESC, comma or end */
					if (symtype == SASC || symtype == SDESC) {
						if (symtype == SDESC) orderbyascflag[numordercolumns] = FALSE;
						symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* comma or end */
					}
					numordercolumns++;
					if (symtype != SCOMMA) break;  /* no more columns, get out */
					symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* another column, get it's name */
				}

				/* store number of order columns and pointers to arrays of order column handles
				   and asc/desc flags in select structure */
				workarray = (UINT **) memalloc(numordercolumns * sizeof(UINT), 0);
				cworkarray = (UCHAR **) memalloc(numordercolumns * sizeof(UCHAR), 0);
				if (workarray == NULL || cworkarray == NULL) {
					memfree((UCHAR **) workarray);
					memfree(cworkarray);
					sqlerrnum(SQLERR_PARSE_NOMEM);
					goto procerror;
				}
				for (i1 = 0; i1 < numordercolumns; i1++) {
					(*workarray)[i1] = orderbytabcolnums[i1];
					(*cworkarray)[i1] = orderbyascflag[i1];
				}
				(*selectp)->numordercolumns = numordercolumns;
				(*selectp)->ordertabcolnumarray = workarray;
				(*selectp)->orderascflagarray = cworkarray;
			}
			else if (symtype == SGROUP) {
				/* process GROUP BY column list */
				if ((*selectp)->numgroupcolumns) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "multiple GROUP BY's not supported", NULL);
					goto procerror;
				}
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get BY or first col spec */
				if (symtype == SBY) symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get first col spec */
				/* process comma-delimited column list. The columns can be specified as  */
				/* columnname, tablename.columnname or column number */
				numgroupcolumns = 0;
				for ( ; ; ) {	/* have next column name already scanned */
					if ((symtype == SERROR) || (symtype == SEND)) {	/* error or end of statement */
						sqlerrnummsg(SQLERR_PARSE_ERROR, "expected column reference after keywords GROUP BY", NULL);
						goto procerror;
					}
					if (numgroupcolumns == MAX_GROUPBY) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "exceeded maximum supported GROUP BY columns", NULL);
						goto procerror;
					}
					if (symtype == SNUMBER) { /* group column specified by index into selected columns */
						mscntoi(sqlliteral, &i1, sqllength);
						if (i1 < 1 || i1 > numcolumns) {
							sqlerrnummsg(SQLERR_PARSE_ERROR, "GROUP BY: invalid column number", NULL);
							goto procerror;
						}
						groupbytabcolnums[numgroupcolumns] = (*(*selectp)->tabcolnumarray)[i1 - 1];
					}
					else {
						if ((sqlcattype != CID) || (!validatesqlname(sqlsymbol, sqllength, FALSE))) {
							sqlerrnummsg(SQLERR_PARSE_ERROR, "GROUP BY: invalid column name: ", sqlsymbol);
							goto procerror;
						}
						if (parsecolumnname(&columndesc) != SQL_SUCCESS) goto procerror;
						/* see if the column is valid and get its column handle */
						tabcolnum = findcolumn(&columndesc, *corrtable, TRUE);
						if (!tabcolnum) {
							sqlerrnummsg(SQLERR_PARSE_COLUMNNOTFOUND, "GROUP BY", columndesc.columnname);
							goto procerror;
						}
						groupbytabcolnums[numgroupcolumns] = tabcolnum;
					}
					symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* comma or end */
					numgroupcolumns++;
					if (symtype != SCOMMA) break;  /* no more columns, get out */
					symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* another column, get it's name */
				}

				/* store number of group columns and pointers to arrays of group column handles */
				i1 = numgroupcolumns;
				if ((*selectp)->setfuncdistinctcolumn) i1++;  /* for use in buildselectplan */
				workarray = (UINT **) memalloc(i1 * sizeof(UINT), 0);
				if (workarray == NULL) {
					sqlerrnum(SQLERR_PARSE_NOMEM);
					goto procerror;
				}
				for (i1 = 0; i1 < numgroupcolumns; i1++) (*workarray)[i1] = groupbytabcolnums[i1];
				if ((*selectp)->setfuncdistinctcolumn) (*workarray)[i1] = (*selectp)->setfuncdistinctcolumn;  /* for use in buildselectplan */
				(*selectp)->numgroupcolumns = numgroupcolumns;
				(*selectp)->grouptabcolnumarray = workarray;
			}
			else if (symtype == SHAVING) {
				/* process HAVING search conditions */
				if ((*selectp)->having != NULL) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "multiple HAVING's not supported", NULL);
					goto procerror;
				}
				swhere = parsewhere(PARSE_SELECT | PARSE_HAVING, corrtable, &symtype);
				if (swhere == NULL) goto procerror;
				(*selectp)->having = swhere;
			}
			else if (symtype == SFOR) {
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get READ or UPDATE */
				if (symtype == SUPDATE || symtype == SREAD) {
					if (symtype == SUPDATE) {
						if ((*selectp)->numtables != 1) {
							sqlerrnummsg(SQLERR_PARSE_ERROR, "only one table reference allowed in SELECT FOR UPDATE", NULL);
							goto procerror;
						}
						(*selectp)->forupdateflag = TRUE;
						symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
						if (symtype == SNOWAIT) {
							(*selectp)->nowaitflag = TRUE;
						}
					}
					symtype = SEND;
				}
				else {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword UPDATE or READ after keyword FOR", NULL);
					goto procerror;
				}
 			}
			else if (symtype != SEND) {	/* extraneous material at end of statement */
				sqlerrnummsg(SQLERR_PARSE_ERROR, "extraneous material at end of statement", (char *) sqlstatement + sqllastoffset);
				goto procerror;
			}
		}
		if (numspecial || (*selectp)->having != NULL) {
			/* verify use of any set functions in select list or having clause */
			if ((*selectp)->having != NULL) {
				swhere = (*selectp)->having;
				numexp = (*swhere)->numentries;
			}
			else numexp = 0;
			numsetfunc = 0;
			for (i1 = i4 = 0; i1 < numspecial + numexp; i1++) {
				if (i1 < numspecial) {
					if ((*(S_GENERIC **)(*specialarray)[i1])->stype != STYPE_AEXP) continue;
					saexp = (S_AEXP **)(*specialarray)[i1];
				}
				else {
					if ((*swhere)->lexpentries[i1 - numspecial].type != OPEXP) {
						if ((*swhere)->lexpentries[i1 - numspecial].type == OPCOLUMN) {
							tabcolnum = (*swhere)->lexpentries[i1 - numspecial].tabcolnum;
							if ((*selectp)->numgroupcolumns) {
								workarray = (*selectp)->grouptabcolnumarray;
								for (i5 = 0; i5 < numgroupcolumns && tabcolnum != (*workarray)[i5]; i5++);
								if (i5 == numgroupcolumns) {
									sqlerrnummsg(SQLERR_PARSE_ERROR, "HAVING column is not a grouping column", NULL);
									goto procerror;
								}
							}
							else i4 = i1 + 1;
						}
						continue;
					}
					saexp = (*swhere)->lexpentries[i1 - numspecial].expp;
				}
				i3 = (*saexp)->numentries;
				for (i2 = 0; i2 < i3; i2++) {
					op = (*saexp)->aexpentries[i2].type;
					tabcolnum = (*saexp)->aexpentries[i2].tabcolnum;
					if (op == OPCOLUMN) {
						if ((*selectp)->numgroupcolumns) {
							workarray = (*selectp)->grouptabcolnumarray;
							for (i5 = 0; i5 < numgroupcolumns && tabcolnum != (*workarray)[i5]; i5++);
							if (i5 == numgroupcolumns) {
								if (i1 < numspecial) sqlerrnummsg(SQLERR_PARSE_ERROR, "SELECT column is not a grouping column", NULL);
								else sqlerrnummsg(SQLERR_PARSE_ERROR, "HAVING column is not a grouping column", NULL);
								goto procerror;
							}
						}
						else i4 = i1 + 1;
						continue;
					}
					if (op < OPCOUNTALL || op > OPMAXDISTINCT) continue;
					for (i5 = 0; i5 < numsetfunc; i5++)
						if (op == (*setfuncarray)[i5].type && tabcolnum == (*setfuncarray)[i5].tabcolnum) break;
					if (i5 == numsetfunc) {
						if (op >= OPCOUNTDISTINCT && op <= OPMAXDISTINCT) {
							if ((*selectp)->setfuncdistinctcolumn && tabcolnum != (*selectp)->setfuncdistinctcolumn) {
								sqlerrnummsg(SQLERR_PARSE_ERROR, "only one distinct set function column supported", NULL);
								goto procerror;
							}
							(*selectp)->setfuncdistinctcolumn = tabcolnum;
						}
						if (!numsetfunc) {
							setfuncarray = (SETFUNC **) memalloc(sizeof(SETFUNC), 0);
							if (setfuncarray == NULL) {
								sqlerrnum(SQLERR_PARSE_NOMEM);
								goto procerror;
							}
							(*selectp)->setfunctionarray = setfuncarray;
						}
						else if (memchange((UCHAR **) setfuncarray, (numsetfunc + 1) * sizeof(SETFUNC), 0)) {
							sqlerrnum(SQLERR_PARSE_NOMEM);
							goto procerror;
						}
						(*setfuncarray)[numsetfunc].type = op;
						(*setfuncarray)[numsetfunc++].tabcolnum = tabcolnum;
					}
				}
			}
			if (numsetfunc || (*selectp)->having != NULL) {
				if ((*selectp)->distinctflag) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "SELECT DISTINCT not supported with set functions or HAVING", NULL);
					goto procerror;
				}
				if ((*selectp)->numordercolumns && !(*selectp)->numgroupcolumns) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "ORDER BY without GROUP BY not supported with set functions or HAVING", NULL);
					goto procerror;
				}
				if ((*selectp)->forupdateflag) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "FOR UPDATE not supported with set functions or HAVING", NULL);
					goto procerror;
				}
				if (i4) {
					if (i4 <= numspecial) sqlerrnummsg(SQLERR_PARSE_ERROR, "SELECT column is not a set function", NULL);
					else sqlerrnummsg(SQLERR_PARSE_ERROR, "HAVING column is not a set function", NULL);
					goto procerror;
				}
				(*selectp)->numsetfunctions = numsetfunc;
			}
		}
		if ((*selectp)->numgroupcolumns) {
			/* check to verify that select column is part of order by (did set functions above) */
			workarray = (*selectp)->tabcolnumarray;
			for (i1 = 0; i1 < numcolumns; i1++) {
				if (gettabref((*workarray)[i1]) > numtables) continue;
				for (i2 = 0; i2 < numgroupcolumns && (*(*selectp)->grouptabcolnumarray)[i2] != (*workarray)[i1]; i2++);
				if (i2 == numgroupcolumns) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "SELECT column is not a grouping column", NULL);
					goto procerror;
				}
			}
			if ((*selectp)->numordercolumns) {
				/* verify if order by columns consist of result and group by */
				workarray = (*selectp)->ordertabcolnumarray;
				for (i1 = 0; i1 < numordercolumns; i1++) {
					if (gettabref((*workarray)[i1]) > numtables) {
						for (i2 = 0; i2 < numcolumns && (*(*selectp)->tabcolnumarray)[i2] != (*workarray)[i1]; i2++);
						if (i2 == numcolumns) break;
					}
					else {
						for (i2 = 0; i2 < numgroupcolumns && (*(*selectp)->grouptabcolnumarray)[i2] != (*workarray)[i1]; i2++);
						if (i2 == numgroupcolumns) break;
					}
				}
				if(i1 < numordercolumns) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "ORDER BY column is not a SELECT column or GROUP BY column", NULL);
					goto procerror;
				}
			}
			if ((*selectp)->forupdateflag) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "FOR UPDATE not supported with ORDER BY", NULL);
				goto procerror;
			}
		}
	}

	/* remove table alias that are the same as the table name */
	for (i1 = i2 = 0; i1 < (*corrtable)->count; i1++) {
		if ((*corrtable)->info[i1].tablenum) {
			tab1 = tableptr((*corrtable)->info[i1].tablenum);
			if (!strcmp(nameptr(tab1->name), (*corrtable)->info[i1].name)) continue;
		}
		if (i1 != i2) (*corrtable)->info[i2] = (*corrtable)->info[i1];
		i2++;
	}
	if (i2) {
		memchange((UCHAR **) corrtable, sizeof(CORRTABLE) + (i2 - 1) * sizeof(CORRINFO), 0);
		(*corrtable)->count = (*corrtable)->size = i2;
	}
	else {
		memfree((UCHAR **) corrtable);
		(*selectp)->corrtable = NULL;
	}
	*stmt = (S_GENERIC **) selectp;
	return 0;

procerror:	/* an error occurred, undo memory allocation */
	/* reset numbers just in case */
	(*selectp)->numtables = numtables;
	(*selectp)->numspecialcolumns = numspecial;
	freestmt((S_GENERIC **) selectp);
	*stmt = NULL;
	return -1;
}

/*
 * parseinsert
 *		Parse an SQL INSERT statement.
 *		INSERT [INTO] table [(column [,column]... )] VALUES (value [,value]...)
 *	Exit:
 *		statement parsed and described in the STATEMENT structure
 *	Returns:
 *		SQL_SUCCESS if okay
 *		SQL_ERROR if invalid or integer digits truncated, error given
 */
static INT parseinsert(S_GENERIC ***stmt)
{
	INT i1, columnnum, maxvalues, minus, numcolumns, numvalues;
	INT symtype, tablenum, tabcolnum;
	UINT **tabcolnumsarray;
	CHAR name[MAX_NAME_LENGTH + 1];
	TABLE *tab1;
	COLUMN *col1;
	S_INSERT **insertp;
	S_VALUE **valuep, ****valuesarray;

	*stmt = NULL;
	numvalues = numcolumns = 0;
	tabcolnumsarray = NULL;
	valuesarray = NULL;

	/* allocate a structure to describe the INSERT statement */
	insertp = (S_INSERT **) memalloc(sizeof(S_INSERT), MEMFLAGS_ZEROFILL);
	if (insertp == NULL) return sqlerrnum(SQLERR_PARSE_NOMEM);
	(*insertp)->stype = STYPE_INSERT;

	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get first symbol, could be INTO or table name*/
	if (symtype == SINTO) {
		/* get table name */
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
		if (sqlcattype != CID) {  /* not an identifier */
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected table name after keyword INTO", NULL);
			goto procerror;
		}
	}
	else {
		if (sqlcattype != CID && sqlcattype != CLITERAL) {  /* not an identifier */
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected table name after keyword INSERT", NULL);
			goto procerror;
		}
	}
	if (!validatesqlname(sqlsymbol, sqllength, sqlcattype == CLITERAL)) {	/* validate the name */
		sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid table name", sqlsymbol);
		goto procerror;
	}

	strucpy(name, sqlsymbol);
	tablenum = findtablenum(name);
	if (!tablenum) {
		sqlerrnummsg(SQLERR_PARSE_TABLENOTFOUND, sqlsymbol, NULL);
		goto procerror;
	}
	tab1 = tableptr(tablenum);
	/* see if the table is read-only */
	if (tab1->flags & TABLE_FLAGS_READONLY) {
		sqlerrnummsg(SQLERR_PARSE_READONLY, name, NULL);
		goto procerror;
	}
	if ((tab1->flags & TABLE_FLAGS_TEMPLATE) && maketemplate(name, &tablenum)) goto procerror;
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
	if (symtype == SERROR || symtype == SEND) {	/* error or end of statement */
		sqlerrnummsg(SQLERR_PARSE_ERROR, "expected column reference specification or keyword VALUES after table reference specification", NULL);
		goto procerror;
	}
	if (symtype == SLPAREN) {  /* column list is specified */
		tabcolnumsarray = (UINT **) memalloc(tab1->numcolumns * sizeof(INT), 0);
		if (tabcolnumsarray == NULL) {
			sqlerrnum(SQLERR_PARSE_NOMEM);
			goto procerror;
		}
		tab1 = tableptr(tablenum);
		do {  /* process (colname [,colname]... ) */
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* get next column name */
			if (sqlcattype != CID) {  /* not an identifier */
				sqlerrnummsg(SQLERR_PARSE_ERROR, "expected column name", NULL);
				goto procerror;
			}
			if (numcolumns >= tab1->numcolumns) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "too many column names specified", NULL);
				goto procerror;
			}
			if (!validatesqlname(sqlsymbol, sqllength, FALSE)) {  /* validate the name */
				sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid column name", sqlsymbol);
				goto procerror;
			}
			strucpy(name, sqlsymbol);
			tabcolnum = findcolumnintable(tablenum, name);
			if (!tabcolnum) {
				sqlerrnummsg(SQLERR_PARSE_COLUMNNOTFOUND, name, NULL);
				goto procerror;
			}
			(*tabcolnumsarray)[numcolumns] = gettabcolref(1, tabcolnum);
			numcolumns++;
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* must be comma or right paren */
			if (symtype != SCOMMA && symtype != SRPAREN) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "expected comma or right parenthesis in column reference specification", NULL);
				goto procerror;
			}
		}
		while (symtype == SCOMMA);  /* while column names */
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* next symbol, must be VALUES */
	}

	/* done processing columns, next symbol scanned, should be VALUES */
	if (symtype != SVALUES) {
		sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword VALUES", NULL);
		goto procerror;
	}
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* next symbol must be left parenthesis */
	if (symtype != SLPAREN) {
		sqlerrnummsg(SQLERR_PARSE_ERROR, "expected (value list)", NULL);
		goto procerror;
	}
	maxvalues = (numcolumns) ? numcolumns : tab1->numcolumns;
	valuesarray = (S_VALUE ****) memalloc(maxvalues * sizeof(S_VALUE **), MEMFLAGS_ZEROFILL);
	tab1 = tableptr(tablenum);
	if (valuesarray == NULL) {  /* memory allocation failed */
		sqlerrnum(SQLERR_PARSE_NOMEM);
		goto procerror;
	}
	do {  /* process (value [,value]... ) */
/*** CODE: CHANGING PLUS/MINUS CODE IN NUMERIC SUPPORT COULD PARSE NUMBER IN ONE CALL ***/
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get next value name */
		if (symtype == SERROR || symtype == SEND) {  /* error or end of statement */
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected column value specification after keyword VALUES", NULL);
			goto procerror;
		}
		if (numvalues >= maxvalues) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "too many values specified", NULL);
			goto procerror;
		}
		/* get pointer to current column structure */
		if (tabcolnumsarray != NULL) columnnum = getcolnum((*tabcolnumsarray)[numvalues]);
		else columnnum = numvalues + 1;
		col1 = columnptr(tab1, columnnum);
		if (col1->type == TYPE_CHAR || col1->type == TYPE_DATE ||
			col1->type == TYPE_TIME || col1->type == TYPE_TIMESTAMP) {
			if (symtype != SLITERAL) {
/*** CODE: USE TO ALSO TEST FOR (&& symtype != SQUESTION), BUT IT WAS NOT CORRECTLY SUPPORTED BELOW ***/
				sqlerrnummsg(SQLERR_PARSE_ERROR, "expected character string for value of column", nameptr(col1->name));
				goto procerror;
			}
			/* allocate the S_VALUE structure for this column value. If it's longer than the column length, truncate it */
			valuep = (S_VALUE **) memalloc(sizeof(S_VALUE) + min(sqllength, (INT) col1->length) - 1, 0);
			if (valuep == NULL) {
				sqlerrnum(SQLERR_PARSE_NOMEM);
				goto procerror;
			}
			tab1 = tableptr(tablenum);
			col1 = columnptr(tab1, columnnum);
			(*valuesarray)[numvalues] = valuep;
			(*valuep)->stype = STYPE_VALUE;
			(*valuep)->length = min(sqllength, (INT) col1->length);
			(*valuep)->litoffset = 0;
			(*valuep)->escape[0] = '\0';
			memcpy((*valuep)->data, sqlliteral, (INT) min(sqllength, (INT) col1->length));
			/* if the literal is longer than the column, it's truncated. */
			if (sqllength > (INT) col1->length) sqlswi();
		}
		else {  /* NUMERIC column */
			minus = 0;
			if (symtype == SMINUS || symtype == SPLUS) {
				if (symtype == SMINUS) minus = 1;
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* get next value name */
			}
			if (symtype != SNUMBER && symtype != SNULL) {
/*** CODE: USE TO ALSO TEST FOR (&& symtype != SQUESTION), BUT IT WAS NOT CORRECTLY SUPPORTED BELOW ***/
				sqlerrnummsg(SQLERR_PARSE_ERROR, "expected number for value of column", nameptr(col1->name));
				goto procerror;
			}
			/* Allocate the S_VALUE structure for this column value based on the column length. */
			/* The value is modified to fit the column size. */
			if (symtype == SNUMBER) valuep = (S_VALUE **) memalloc(sizeof(S_VALUE) + (INT) col1->length - 1, 0);
			else valuep = (S_VALUE **) memalloc(sizeof(S_VALUE), 0);
			if (valuep == NULL) {
				sqlerrnum(SQLERR_PARSE_NOMEM);
				goto procerror;
			}
			tab1 = tableptr(tablenum);
			col1 = columnptr(tab1, columnnum);
			(*valuesarray)[numvalues] = valuep;
			(*valuep)->stype = STYPE_VALUE;
			if (symtype == SNUMBER) {
				(*valuep)->length = col1->length;
				if (validatenumeric(col1, minus, valuep) == SQL_ERROR)
					goto procerror;  /* invalid or too many integral digits */
			}
			else (*valuep)->length = 0;
			(*valuep)->litoffset = 0;
		}
		numvalues++;
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* must be comma or right paren */
		if (symtype != SCOMMA && symtype != SRPAREN) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected comma or right parenthesis", NULL);
			goto procerror;
		}
	} while (symtype == SCOMMA);  /* while values */
	if (numvalues < numcolumns) {
		sqlerrnummsg(SQLERR_PARSE_ERROR, "too few values, all column values must be specified", NULL);
		goto procerror;
	}
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* next symbol, must be end of statement */
	if (symtype != SEND) {	/* extraneous material at end of statement */
		sqlerrnummsg(SQLERR_PARSE_ERROR, "extraneous material after right parenthesis", (char *) sqlstatement + sqllastoffset);
		goto procerror;
	}

	(*insertp)->tablenum = tablenum;
	(*insertp)->numcolumns = numcolumns;
	(*insertp)->tabcolnumarray = tabcolnumsarray;
	(*insertp)->numvalues = numvalues;
	(*insertp)->valuesarray = valuesarray;
	*stmt = (S_GENERIC **) insertp;
	return 0;

procerror:	/* an error occurred, undo memory allocation */
	if (valuesarray != NULL) {
		for (i1 = 0; i1 < maxvalues; i1++) memfree((unsigned char **)(*valuesarray)[i1]);
		memfree((unsigned char **) valuesarray);
	}
	memfree((unsigned char **) tabcolnumsarray);
	memfree((unsigned char **) insertp);
	return -1;
}

/*	parseupdate */
/*		Parse an SQL UPDATE statement. */
/*		UPDATE table SET column = value [, column = value]... [WHERE search-condition] */
/*	Entry: */
/*	Exit: */
/*		statement parsed and described in the STATEMENT structure */
/*	Returns: */
/*		SQL_SUCCESS if okay */
/*		SQL_ERROR if invalid or integer digits truncated, error given, */
static int parseupdate(S_GENERIC ***stmt)
{
	int i1, i2, columnnum, symtype, tablenum;
	int numcolumns = 0;
	CHAR name[MAX_NAME_LENGTH + 1];
	CORRTABLE corrtable, *corrtablep;
	S_UPDATE **updatep;
	TABLE *tab1;
	S_SETCOLUMN **setcolumnarray = NULL;
	COLUMN *col1;
	S_VALUE **valuep;
	S_AEXP **expp;
	S_WHERE **swhere;

	*stmt = NULL;
	/* allocate a structure to describe the UPDATE statement */
	updatep = (S_UPDATE **) memalloc(sizeof(S_UPDATE), MEMFLAGS_ZEROFILL);
	if (updatep == NULL) return sqlerrnum(SQLERR_PARSE_NOMEM);
	/* get table name */
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
	if (sqlcattype != CID && sqlcattype != CLITERAL) {		/* not an identifier */
		sqlerrnummsg(SQLERR_PARSE_ERROR, "expected table reference after keyword UPDATE", NULL);
		goto procerror;
	}
	if (!validatesqlname(sqlsymbol, sqllength, sqlcattype == CLITERAL)) {	/* validate the name */
		sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid table name", sqlsymbol);
		goto procerror;
	}
	strucpy(name, sqlsymbol);
	tablenum = findtablenum(name);
	if (!tablenum) {
		sqlerrnummsg(SQLERR_PARSE_TABLENOTFOUND, sqlsymbol, NULL);
		goto procerror;
	}
	tab1 = tableptr(tablenum);
	/* see if the table is read-only */
	if (tab1->flags & (TABLE_FLAGS_READONLY | TABLE_FLAGS_NOUPDATE)) {
		if (tab1->flags & TABLE_FLAGS_READONLY) sqlerrnummsg(SQLERR_PARSE_READONLY, sqlsymbol, NULL);
		else sqlerrnummsg(SQLERR_PARSE_NOUPDATE, sqlsymbol, NULL);
		goto procerror;
	}
	if ((tab1->flags & TABLE_FLAGS_TEMPLATE) && maketemplate(name, &tablenum)) goto procerror;
	corrtable.count = 1;
	corrtable.info[0].tablenum = tablenum;
	strcpy(corrtable.info[0].name, name);
	corrtablep = &corrtable;
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);		/* get next sumbol, must be SET */
	if (symtype != SSET) {
		sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword SET after table reference", NULL);
		goto procerror;
	}
	do {  /* process column = value [, column = value]... */
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);		/* get next column name */
		if (sqlcattype != CID) {		/* not an identifier */
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected column reference", NULL);
			goto procerror;
		}
		if (!validatesqlname(sqlsymbol, sqllength, FALSE)) {	/* validate the name */
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid column name", sqlsymbol);
			goto procerror;
		}
		strucpy(name, sqlsymbol);
		columnnum = findcolumnintable(tablenum, name);
		if (!columnnum) {
			sqlerrnummsg(SQLERR_PARSE_COLUMNNOTFOUND, name, NULL);
			goto procerror;
		}
		if (numcolumns == 0) {  /* this is the first column, allocate column description array */
			setcolumnarray = (S_SETCOLUMN **) memalloc(sizeof(S_SETCOLUMN), MEMFLAGS_ZEROFILL);
			if (setcolumnarray == NULL) {
				sqlerrnum(SQLERR_PARSE_NOMEM);
				goto procerror;
			}
		}
		else {  /* not first column, resize the column description array */
			if (memchange((unsigned char **) setcolumnarray, (numcolumns + 1) * sizeof(S_SETCOLUMN), MEMFLAGS_ZEROFILL)) {
				sqlerrnum(SQLERR_PARSE_NOMEM);
				goto procerror;
			}
		}
		tab1 = tableptr(tablenum);
		(*setcolumnarray)[numcolumns].stype = STYPE_SETCOLUMN;
		(*setcolumnarray)[numcolumns].columnnum = columnnum;
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* next symbol must be equal sign */
		if (symtype != SEQUAL) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected equal symbol after column reference", NULL);
			goto procerror;
		}
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* next symbol is value for column */
		col1 = columnptr(tab1, columnnum);  /* get pointer to current column structure */
		if (col1->type == TYPE_CHAR || col1->type == TYPE_DATE ||
			col1->type == TYPE_TIME || col1->type == TYPE_TIMESTAMP) {
			if (col1->type == TYPE_CHAR && (symtype == SLOWER || symtype == SUPPER)) {
				expp = parseaexp(PARSE_UPDATE, &corrtablep, &symtype);
				if (expp == NULL) goto procerror;  /* invalid expression */
				(*setcolumnarray)[numcolumns].expp = expp;
				tab1 = tableptr(tablenum);
			}
			else {
				if (symtype != SLITERAL) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "expected character string for value of column", nameptr(col1->name));
					goto procerror;
				}
				/* allocate the S_VALUE structure for this column value. If it's longer than */
				/* the column length, truncate it. If shorter, pad with blanks */
				valuep = (S_VALUE **) memalloc(sizeof(S_VALUE) + (INT) col1->length - 1, 0);
				if (valuep == NULL) {
					sqlerrnum(SQLERR_PARSE_NOMEM);
					goto procerror;
				}
				tab1 = tableptr(tablenum);
				col1 = columnptr(tab1, columnnum);
				(*setcolumnarray)[numcolumns].valuep = valuep;
				(*valuep)->stype = STYPE_VALUE;
				(*valuep)->length = min(sqllength, (INT) col1->length);
				(*valuep)->litoffset = 0;
				(*valuep)->escape[0] = '\0';
				memcpy((*valuep)->data, sqlliteral, min(sqllength, (INT) col1->length));
				/* if the literal is longer than the column, it's truncated. */
				if (sqllength > (INT) col1->length) sqlswi();
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* must be comma or next symbol */
			}
		}
		else {  /* NUMERIC column */
			if (symtype != SNULL) {
				expp = parseaexp(PARSE_UPDATE, &corrtablep, &symtype);
				if (expp == NULL) goto procerror;  /* invalid expression */
				(*setcolumnarray)[numcolumns].expp = expp;
				tab1 = tableptr(tablenum);
			}
			else {
				valuep = (S_VALUE **) memalloc(sizeof(S_VALUE), 0);
				if (valuep == NULL) {
					sqlerrnum(SQLERR_PARSE_NOMEM);
					goto procerror;
				}
				tab1 = tableptr(tablenum);
				col1 = columnptr(tab1, columnnum);
				(*setcolumnarray)[numcolumns].valuep = valuep;
				(*valuep)->stype = STYPE_VALUE;
				(*valuep)->length = 0;
				(*valuep)->litoffset = 0;
				(*valuep)->escape[0] = '\0';
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* must be comma or next symbol */
			}
		}
		numcolumns++;
	} while (symtype == SCOMMA);  /* while another column updated */

	/* process WHERE search conditions, if present */
	if (symtype == SWHERE) {
		swhere = parsewhere(PARSE_UPDATE | PARSE_WHERE, &corrtablep, &symtype);
		if (swhere == NULL) goto procerror;
		(*updatep)->where = swhere;
	}
	if (symtype != SEND) {	/* extraneous material at end of statement */
		sqlerrnummsg(SQLERR_PARSE_ERROR, "extraneous material at end of statement", (char *) sqlstatement + sqllastoffset);
		goto procerror;
	}

	(*updatep)->stype = STYPE_UPDATE;			/* set structure type */
	(*updatep)->tablenum = tablenum;
	(*updatep)->numcolumns = numcolumns;
	(*updatep)->setcolumnarray = setcolumnarray;
	*stmt = (S_GENERIC **) updatep;
	return 0;

procerror:  /* an error occurred, undo memory allocation */
	if (setcolumnarray != NULL) {
		for (i1 = 0; i1 < numcolumns; i1++) {
			if ((*setcolumnarray)[i1].valuep != NULL)
				memfree((UCHAR **)(*setcolumnarray)[i1].valuep);
			else if ((*setcolumnarray)[i1].expp != NULL) {
				expp = (*setcolumnarray)[i1].expp;
				for (i2 = 0; i2 < (*expp)->numentries; i2++) {
					if ((*expp)->aexpentries[i2].type == OPVALUE)
						memfree((UCHAR **)(*expp)->aexpentries[i2].valuep);
				}
			}
		} /* for each column in the update setcolumnarray */
		memfree((UCHAR **) setcolumnarray);
	}
	freestmt((S_GENERIC **)(*updatep)->where);
	memfree((unsigned char **) updatep);
	return -1;
}

/*	parsedelete */
/*		Parse an SQL DELETE statement. */
/*		DELETE FROM table [WHERE search-condition] */
/*	Returns: 0 if success, -1 if syntax or memory error */
static INT parsedelete(S_GENERIC ***stmt)
{
	INT symtype, tablenum;
	CHAR name[MAX_NAME_LENGTH + 1];
	TABLE *tab1;
	CORRTABLE corrtable, *corrtablep;
	S_DELETE **deletep;
	S_WHERE **swhere;

	*stmt = NULL;
	/* allocate a structure to describe the DELETE statement */
	deletep = (S_DELETE **) memalloc(sizeof(S_DELETE), MEMFLAGS_ZEROFILL);
	if (deletep == NULL) return sqlerrnum(SQLERR_PARSE_NOMEM);
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* get first symbol, must be FROM */
	if (symtype != SFROM) {
		sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword FROM after keywords DELETE", NULL);
		goto procerror;
	}

	/* get table name */
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
	if (sqlcattype != CID && sqlcattype != CLITERAL) {
		sqlerrnummsg(SQLERR_PARSE_ERROR, "expected table reference after keywords DELETE FROM", NULL);
		goto procerror;
	}
	if (!validatesqlname(sqlsymbol, sqllength, sqlcattype == CLITERAL)) {
		sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid table name", sqlsymbol);
		goto procerror;
	}
	strucpy(name, sqlsymbol);
	tablenum = findtablenum(name);
	if (!tablenum) {
		sqlerrnummsg(SQLERR_PARSE_TABLENOTFOUND, sqlsymbol, NULL);
		goto procerror;
	}
	tab1 = tableptr(tablenum);
	/* see if the table is read-only */
	if (tab1->flags & TABLE_FLAGS_READONLY) {
		sqlerrnummsg(SQLERR_PARSE_READONLY, sqlsymbol, NULL);
		goto procerror;
	}
	if ((tab1->flags & TABLE_FLAGS_TEMPLATE) && maketemplate(name, &tablenum)) goto procerror;
	corrtable.count = 1;
	corrtable.info[0].tablenum = tablenum;
	strcpy(corrtable.info[0].name, name);
	corrtablep = &corrtable;
	/* process WHERE search conditions, if present */
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
	if (symtype == SWHERE) {
		swhere = parsewhere(PARSE_DELETE | PARSE_WHERE, &corrtablep, &symtype);
		if (swhere == NULL) goto procerror;
		(*deletep)->where = swhere;
	}
	if (symtype != SEND) {	/* extraneous material at end of statement */
		sqlerrnummsg(SQLERR_PARSE_ERROR, "extraneous material at end of statement", (char *) sqlstatement + sqllastoffset);
		goto procerror;
	}

	(*deletep)->stype = STYPE_DELETE;			/* set structure type */
	(*deletep)->tablenum = tablenum;
	*stmt = (S_GENERIC **)deletep;
	return 0;

procerror:
	freestmt((S_GENERIC **)(*deletep)->where);
	memfree((unsigned char **) deletep);
	return -1;
}

/*	parselock */
/*		Parse an SQL LOCK/UNLOCK statement. */
/*		LOCK/UNLOCK TABLE table */
/*	Exit: */
/*		statement parsed and described in the STATEMENT structure */
/*	Returns: */
/*		SQL_SUCCESS if okay */
/*		SQL_ERROR if invalid or integer digits truncated, error given, */
static INT parselock(S_GENERIC ***stmt, INT stype)
{
	INT symtype, tablenum;
	CHAR name[MAX_NAME_LENGTH + 1];
	TABLE *tab1;
	S_LOCK **lockp;

	/* allocate a structure to describe the LOCK statement */
	lockp = (S_LOCK **) memalloc(sizeof(S_INSERT), MEMFLAGS_ZEROFILL);
	if (lockp == NULL) return sqlerrnum(SQLERR_PARSE_NOMEM);

	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get first symbol, must be TABLE */
	if (symtype != STABLE) {
		sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword TABLE after keyword LOCK/UNLOCK", NULL);
		goto procerror;
	}

	/* get table name */
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
	if (sqlcattype != CID && sqlcattype != CLITERAL) {  /* not an identifier */
		sqlerrnummsg(SQLERR_PARSE_ERROR, "expected table name after keyword TABLE", NULL);
		goto procerror;
	}
	if (!validatesqlname(sqlsymbol, sqllength, sqlcattype == CLITERAL)) {	/* validate the name */
		sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid table name", sqlsymbol);
		goto procerror;
	}
	strucpy(name, sqlsymbol);
	tablenum = findtablenum(name);
	if (!tablenum) {
		sqlerrnummsg(SQLERR_PARSE_TABLENOTFOUND, sqlsymbol, NULL);
		goto procerror;
	}
	tab1 = tableptr(tablenum);
	/* see if the table is read-only */
	if (tab1->flags & TABLE_FLAGS_READONLY) {
		sqlerrnummsg(SQLERR_PARSE_READONLY, sqlsymbol, NULL);
		goto procerror;
	}
	if ((tab1->flags & TABLE_FLAGS_TEMPLATE) && maketemplate(name, &tablenum)) goto procerror;
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* next symbol, must be end of statement */
	if (symtype != SEND) {	/* extraneous material at end of statement */
		sqlerrnummsg(SQLERR_PARSE_ERROR, "extraneous material after table name", (char *) sqlstatement + sqllastoffset);
		goto procerror;
	}

	(*lockp)->stype = stype;
	(*lockp)->tablenum = tablenum;
	*stmt = (S_GENERIC **) lockp;
	return 0;

procerror:	/* an error occurred, undo memory allocation */
	memfree((UCHAR **) lockp);
	return -1;
}

/* parse and execute create statement */
static INT parsecreate(S_GENERIC ***stmt) {
	INT i1, i2, allowdups, aimdex, filenum;
	INT symtype, tablenum, coloffset;
	CHAR name[MAX_NAME_LENGTH + 1], indexname[64], format[32], datatype[64];
	TABLE *tab1, table;
	COLUMN column;
	INDEX index;
	IDXKEY idxkey[255]; /* impossible to have more than 255 keys */
	CHAR buffer[1025];
	S_GENERIC **createp;
	CHAR *cptr;		/* generic working pointer to a string */
	NAMETABLE tempntable;

	tempntable.table = NULL;
	*stmt = NULL;
	/* allocate a structure to describe the CREATE statement */
	createp = (S_GENERIC **) memalloc(sizeof(S_GENERIC), MEMFLAGS_ZEROFILL);
	if (createp == NULL) return sqlerrnum(SQLERR_PARSE_NOMEM);
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* get first symbol, must be TABLE */
	if (symtype == STABLE) {
/**	CREATE TABLE table_name ( { column_name datatype(size) [,] }... ) [TEXTFILETYPE=type] | [TEXTFILE=name] **/

		/* scan table name, store it in variable 'name', then make sure that it is unique */
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
		if (sqlcattype != CID && sqlcattype != CLITERAL) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected table reference after keywords CREATE TABLE", NULL);
			goto procerror;
		}
		if (!validatesqlname(sqlsymbol, sqllength, sqlcattype == CLITERAL)) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid table name", sqlsymbol);
			goto procerror;
		}
		strucpy(name, sqlsymbol);
		tablenum = findtablenum(name);
		if (tablenum) {
			sqlerrnummsg(SQLERR_PARSE_TABLEALREADYEXISTS, sqlsymbol, NULL);
			goto procerror;
		}

		/* scan LPAREN which is start of column definitions */
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
		if (symtype != SLPAREN) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected '('", NULL);
			goto procerror;
		}
		memset(&table, 0, sizeof(TABLE));
		table.textfilename = -1;

		/* make temporary name table, since additions to real name table cannot be undone in case of error */
		tempntable.alloc = 4096;
		tempntable.table = (CHAR **) memalloc(tempntable.alloc, 0);
		if (tempntable.table == NULL) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "insufficient memory", NULL);
			goto procerror;
		}
		tempntable.size = 1; /* 0 for index is ambiguous, so make first index start at position 1 */

		/* parse through column list */
		for (coloffset = 0;;) {
			/* parse column name */
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (symtype == SRPAREN) break;
			if (sqlcattype != CID) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "expected column name", NULL);
				if (table.hcolumnarray != NULL) memfree((UCHAR **)table.hcolumnarray);
				goto procerror;
			}
			if (!validatesqlname(sqlsymbol, sqllength, FALSE)) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid column name", sqlsymbol);
				if (table.hcolumnarray != NULL) memfree((UCHAR **)table.hcolumnarray);
				goto procerror;
			}

			/* initialize column structure and store name in temp name table */
			memset(&column, 0, sizeof(COLUMN));
			i2 = (INT)strlen(sqlsymbol);
			i1 = tempntable.size;
			while (i1 + i2 >= tempntable.alloc) {
				if (memchange((UCHAR **) tempntable.table, tempntable.alloc + 4096, 0) == -1) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "insufficient memory", NULL);
					if (table.hcolumnarray != NULL) memfree((UCHAR **)table.hcolumnarray);
					goto procerror;
				}
				tempntable.alloc += 4096;
			}
			strcpy(*tempntable.table + i1, sqlsymbol);
			tempntable.size += i2 + 1;
			column.name = i1;

			/* scan data type */
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (sqlcattype != CID) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "expected column data type", NULL);
				if (table.hcolumnarray != NULL) memfree((UCHAR **)table.hcolumnarray);
				goto procerror;
			}
			strcpy(datatype, sqlsymbol);

			/* scan everything from LPAREN up to RPAREN and store in variable 'datatype' */
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (symtype != SLPAREN) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "expected '('", NULL);
				if (table.hcolumnarray != NULL) memfree((UCHAR **)table.hcolumnarray);
				goto procerror;
			}
			strcat(datatype, "(");
			for (;;) {
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (symtype == SERROR || symtype == SEND) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid data type specification for column",
							*tempntable.table + column.name);
					if (table.hcolumnarray != NULL) memfree((UCHAR **)table.hcolumnarray);
					goto procerror;
				}
				else if (symtype == SRPAREN) {
					strcat(datatype, ")");
					break;
				}
				else if (symtype == SCOMMA) strcat(datatype, ",");
				else if (symtype == SSLASH) strcat(datatype, "/");
				else if (symtype == SCOLON) strcat(datatype, ":");
				else if (symtype == SHYPHEN) strcat(datatype, "-");
				else if (symtype == SPERIOD) strcat(datatype, ".");
				else if (symtype == SNUMBER || symtype == SLITERAL) {
					sqlliteral[sqllength] = '\0';
					strcat(datatype, (CHAR *)sqlliteral);
				}
				else if (sqlcattype == CID) {
					strcat(datatype, sqlsymbol);
				}
				else {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid data type specification for column",
							*tempntable.table + column.name);
					if (table.hcolumnarray != NULL) memfree((UCHAR **)table.hcolumnarray);
					goto procerror;
				}
			}

			if (coltype(datatype, 0, &column, format) == -1) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, cfggeterror(), NULL);
				if (table.hcolumnarray != NULL) memfree((UCHAR **)table.hcolumnarray);
				goto procerror;
			}
			if (format[0]) {
				i2 = (INT)strlen(format);
				i1 = tempntable.size;
				while (i1 + i2 >= tempntable.alloc) {
					if (memchange((UCHAR **) tempntable.table, tempntable.alloc + 4096, 0) == -1) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "insufficient memory", NULL);
						if (table.hcolumnarray != NULL) memfree((UCHAR **)table.hcolumnarray);
						goto procerror;
					}
					tempntable.alloc += 4096;
				}
				strcpy(*tempntable.table + i1, format);
				tempntable.size += i2 + 1;
				column.format = i1;
			}

			column.offset = (USHORT) coloffset;
			coloffset = column.offset + column.fieldlen;
			if (!table.numcolumns) {
				table.hcolumnarray = (HCOLUMN) memalloc(sizeof(COLUMN), 0);
				if (table.hcolumnarray == NULL) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "insufficient memory", NULL);
					if (table.hcolumnarray != NULL) memfree((UCHAR **)table.hcolumnarray);
					goto procerror;
				}
			}
			else if (memchange((UCHAR **) table.hcolumnarray, (table.numcolumns + 1) * sizeof(COLUMN), 0) < 0) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "insufficient memory", NULL);
				if (table.hcolumnarray != NULL) memfree((UCHAR **)table.hcolumnarray);
				goto procerror;
			}

			*(*table.hcolumnarray + table.numcolumns++) = column; /* copy */

			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (symtype == SCOMMA) continue;
			else if (symtype == SRPAREN) break;
			else {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid column definition", NULL);
				if (table.hcolumnarray != NULL) memfree((UCHAR **)table.hcolumnarray);
				goto procerror;
			}
		}

		table.reclength = coloffset;
		table.rioopenflags = RIO_P_TXT | RIO_T_STD | RIO_M_SHR | RIO_FIX | RIO_UNC;

		for (;;) {
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (symtype == SEND) break;
			else if (symtype == STEXTFILETYPE) {
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (symtype != SEQUAL) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "expected '='", NULL);
					memfree((UCHAR **)table.hcolumnarray);
					goto procerror;
				}
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (sqlcattype != CID) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "error processing TEXTFILETYPE", NULL);
					memfree((UCHAR **)table.hcolumnarray);
					goto procerror;
				}
				sqlsymbol[sqllength] = '\0';
				for (i1 = 0; i1 < sqllength; i1++) sqlsymbol[i1] = toupper(sqlsymbol[i1]);
				if (!strcmp(sqlsymbol, "STANDARD")) table.rioopenflags |= RIO_T_STD;
				else if (!strcmp(sqlsymbol, "TEXT")) table.rioopenflags |= RIO_T_TXT;
				else if (!strcmp(sqlsymbol, "DATA")) table.rioopenflags |= RIO_T_DAT;
				else if (!strcmp(sqlsymbol, "CRLF")) table.rioopenflags |= RIO_T_DOS;
				else if (!strcmp(sqlsymbol, "MAC")) table.rioopenflags |= RIO_T_MAC;
				else if (!strcmp(sqlsymbol, "BINARY")) table.rioopenflags |= RIO_T_BIN;
				else {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid TEXTFILETYPE specified", NULL);
					memfree((UCHAR **)table.hcolumnarray);
					goto procerror;
				}
			}
			else if (symtype == STEXTFILE) {
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (symtype != SEQUAL) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "expected '='", NULL);
					memfree((UCHAR **)table.hcolumnarray);
					goto procerror;
				}
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (sqlcattype != CID) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "error processing TEXTFILE", NULL);
					memfree((UCHAR **)table.hcolumnarray);
					goto procerror;
				}
				sqlsymbol[sqllength] = '\0';
				miofixname(sqlsymbol, ".txt", FIXNAME_EXT_ADD);
				table.textfilename = cfgaddname(sqlsymbol, &connection.nametable, &connection.nametablesize,
						&connection.nametablealloc);
				if (table.textfilename == RC_ERROR) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, cfggeterror(), NULL);
					memfree((UCHAR **)table.hcolumnarray);
					goto procerror;
				}
			}
			else {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "expected TEXTFILETYPE or TEXTFILE keyword", NULL);
				memfree((UCHAR **)table.hcolumnarray);
				goto procerror;
			}
		}
		if (table.textfilename == -1) {
			/* create text file name from table name */
			strcpy(sqlsymbol, name);
			sqllength = (INT)strlen(sqlsymbol);
			for (i1 = 0; i1 < sqllength; i1++) sqlsymbol[i1] = tolower(sqlsymbol[i1]);
			strcat(sqlsymbol, ".txt");
			table.textfilename = cfgaddname(sqlsymbol, &connection.nametable, &connection.nametablesize,
					&connection.nametablealloc);
			if (table.textfilename == RC_ERROR) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, cfggeterror(), NULL);
				memfree((UCHAR **)table.hcolumnarray);
				goto procerror;
			}
		}

		/* parsing complete, verify text file and create it if it does not exist */
		filenum = rioopen(*connection.nametable + table.textfilename, table.rioopenflags, 0, table.reclength);
		if (filenum < 0) {
			if (filenum == ERR_FNOTF) {
				i1 = (table.rioopenflags & ~RIO_M_SHR) | RIO_M_PRP; /* set create flag */
				filenum = rioopen(*connection.nametable + table.textfilename, i1, 0, table.reclength);
				if (filenum < 0) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "failed to create text file", *connection.nametable + table.textfilename);
					memfree((UCHAR **)table.hcolumnarray);
					goto procerror;
				}
			}
			else {
				if (filenum == ERR_BADTP) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "file type does not match existing text file", *connection.nametable + table.textfilename);
				}
				else if (filenum == ERR_BADRL) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "wrong record length in existing text file", *connection.nametable + table.textfilename);
				}
				else {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "failure using existing text file", *connection.nametable + table.textfilename);
				}
				memfree((UCHAR **)table.hcolumnarray);
				goto procerror;
			}
		}
		rioclose(filenum);

		/* store table */
		i1 = (INT)strlen(name);
		if (i1 > connection.maxtablename) connection.maxtablename = i1;
		table.name = cfgaddname(name, &connection.nametable, &connection.nametablesize, &connection.nametablealloc);
		if (table.name == RC_ERROR) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, cfggeterror(), NULL);
			memfree((UCHAR **)table.hcolumnarray);
			goto procerror;
		}
		if (!connection.numtables) {
			connection.htablearray = (HTABLE) memalloc(sizeof(TABLE), 0);
			if (connection.htablearray == NULL) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "insufficient memory", NULL);
				memfree((UCHAR **)table.hcolumnarray);
				goto procerror;
			}
		}
		else if (memchange((unsigned char **) connection.htablearray, connection.numtables + 1 * sizeof(TABLE), 0) < 0) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "insufficient memory", NULL);
			memfree((UCHAR **)table.hcolumnarray);
			goto procerror;
		}

		*(*connection.htablearray + connection.numtables++) = table;  /* copy */

		/* store column names from temp name table into real name table */
		for (i1 = 0; i1 < table.numcolumns; i1++) {
			cptr = *tempntable.table + (*(table.hcolumnarray))[i1].name;
			i2 = (INT)strlen(cptr);
			if (i2 > connection.maxcolumnname) connection.maxcolumnname = i2;
			(*(table.hcolumnarray))[i1].name =
				cfgaddname(cptr, &connection.nametable, &connection.nametablesize, &connection.nametablealloc);
			if ((*(table.hcolumnarray))[i1].format) {
				cptr = *tempntable.table + (*(table.hcolumnarray))[i1].format;
				(*(table.hcolumnarray))[i1].format =
					cfgaddname(cptr, &connection.nametable, &connection.nametablesize, &connection.nametablealloc);
			}
			if ((*(table.hcolumnarray))[i1].name == RC_ERROR || (*(table.hcolumnarray))[i1].format == RC_ERROR) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, cfggeterror(), NULL);
				memfree((UCHAR **)table.hcolumnarray);
				connection.numtables--;
				goto procerror;
			}
		}
		memfree((UCHAR **)tempntable.table);

		/* replace old dbd file with new one, file name is stored in connection structure */
		if (writedbdfile(&connection) != 0) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, cfggeterror(), NULL);
			memfree((UCHAR **)table.hcolumnarray);
			connection.numtables--;
			goto procerror;
		}
		connection.numalltables++;
	}
	else {
/** CREATE [UNIQUE | ASSOCIATIVE] INDEX index_name ON table_name (column_name[(length)] [,n]) **/
		allowdups = TRUE;
		aimdex = FALSE;
		if (symtype == SUNIQUE) {
			allowdups = FALSE;
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (symtype != SINDEX) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword INDEX after keyword UNIQUE", NULL);
				goto procerror;
			}
		}
		else if (symtype == SASSOCIATIVE) {
			aimdex = TRUE;
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (symtype != SINDEX) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword INDEX after keyword ASSOCIATIVE", NULL);
				goto procerror;
			}
		}
		if (symtype != SINDEX) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword INDEX or TABLE after keyword CREATE", NULL);
			goto procerror;
		}

		/* get index name, followed by 'ON' */
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
		if (sqlcattype != CID) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected index name after CREATE INDEX", NULL);
			goto procerror;
		}
		strcpy(indexname, sqlsymbol);

		/* get table name */
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
		if (symtype != SON) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword ON after index name", NULL);
			goto procerror;
		}
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
		if (sqlcattype != CID && sqlcattype != CLITERAL) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected table reference after keywords CREATE INDEX", NULL);
			goto procerror;
		}
		if (!validatesqlname(sqlsymbol, sqllength, sqlcattype == CLITERAL)) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid table name", sqlsymbol);
			goto procerror;
		}
		strucpy(name, sqlsymbol);
		tablenum = findtablenum(name);
		if (!tablenum) {
			sqlerrnummsg(SQLERR_PARSE_TABLENOTFOUND, sqlsymbol, NULL);
			goto procerror;
		}
		tab1 = tableptr(tablenum);
		if (tab1->flags & TABLE_FLAGS_READONLY) {
			sqlerrnummsg(SQLERR_PARSE_READONLY, sqlsymbol, NULL);
			goto procerror;
		}
		if (aimdex) miofixname(indexname, ".aim", FIXNAME_EXT_ADD);
		else miofixname(indexname, ".isi", FIXNAME_EXT_ADD);
		if (findindexnum(tab1, indexname)) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "index already exists with the name", indexname);
			goto procerror;
		}

		/* scan LPAREN which is start of index range definitions */
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
		if (symtype != SLPAREN) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected '('", NULL);
			goto procerror;
		}

		i1 = (INT)strlen(indexname);
		if (i1 > connection.maxtableindex) connection.maxtableindex = i1;
		if (tab1->numindexes == 0) {
			tab1->hindexarray = (HINDEX) memalloc(sizeof(INDEX), MEMFLAGS_ZEROFILL);
			if (tab1->hindexarray == NULL) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "insufficient memory", NULL);
				goto procerror;
			}
		}
		else if (memchange((UCHAR **) tab1->hindexarray, (tab1->numindexes + 1) * sizeof(INDEX), MEMFLAGS_ZEROFILL) < 0) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "insufficient memory", NULL);
			goto procerror;
		}

		memset(&index, 0, sizeof(INDEX));
		for (i2 = 0;;) {
			/* parse column name */
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (sqlcattype != CID) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "expected column name", NULL);
				goto procerror;
			}
			i1 = findcolumnintable(tablenum, sqlsymbol);
			if (!i1) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "column not found", sqlsymbol);
				goto procerror;
			}
			/* temporarily store column offset and size for building utility arguments */
			column = *(*tab1->hcolumnarray + (i1 - 1));
			idxkey[i2].len = column.fieldlen;
			idxkey[i2].pos = column.offset;
			/* look for column length override */
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (symtype == SLPAREN) {
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (symtype != SNUMBER) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "expected key length", NULL);
					goto procerror;
				}
				idxkey[i2].len = atoi((CHAR *)sqlliteral); /* override length */
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (symtype != SRPAREN) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "expected ')'", NULL);
					goto procerror;
				}
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			}
			i2++;
			if (symtype == SRPAREN) break;
			else if (symtype != SCOMMA) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "expected ','", NULL);
				goto procerror;
			}
		}
		i1 = findindexnum(tab1, indexname);
		closetableindex(tablenum, i1, indexptr(tab1, i1));
		filenum = fioopen(indexname, (tab1->rioopenflags & FIO_M_MASK) | FIO_P_TXT);
		if (filenum > 0) fiokill(filenum); /* if index file already exists, delete it */
		else fioclose(filenum);
		/* generate aimdex/index command line and execute */
		sprintf(buffer, "%s %s ", *connection.nametable + tab1->textfilename, indexname);
		for (i1 = 0; i1 < i2; i1++) {
			mscitoa(idxkey[i1].pos + 1, format);
			strcat(buffer, format);
			strcat(buffer, "-");
			mscitoa(idxkey[i1].pos + idxkey[i1].len, format);
			strcat(buffer, format);
			strcat(buffer, " ");

		}
		strcat(buffer, "-j ");
		if (!aimdex && allowdups) strcat(buffer, "-d"); /* duplicates allowed */
		if (utility(aimdex ? UTIL_AIMDEX : UTIL_INDEX, buffer, -1) != 0) {
			utilgeterror(buffer, sizeof(buffer));
			sqlerrnummsg(SQLERR_OTHER, buffer, NULL);
			goto procerror;
		}
		/* add index file name permanently to the name table */
		index.indexfilename = cfgaddname(indexname, &connection.nametable, &connection.nametablesize, &connection.nametablealloc);
		if (index.indexfilename == RC_ERROR) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, cfggeterror(), NULL);
			goto procerror;
		}
		*(*tab1->hindexarray + tab1->numindexes++) = index; /* copy & increment count */
		/* re-rewite dbd file to store new index on disk */
		if (writedbdfile(&connection) != 0) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, cfggeterror(), NULL);
			tab1->numindexes--;
			goto procerror;
		}
	}

	(*createp)->stype = STYPE_CREATE; /* set structure type */
	*stmt = createp;
	return 0;

procerror:
	memfree((unsigned char **) createp);
	if (tempntable.table != NULL) memfree((UCHAR **)tempntable.table);
	return RC_ERROR;
}

static INT parsedrop(S_GENERIC ***stmt) {
 	INT i1, tablenum, filenum, symtype, errorflag, aimdex, openfilenum;
 	CHAR name[MAX_NAME_LENGTH + 1], indexname[64];
 	TABLE *tab1;
 	INDEX *idx1;
 	S_GENERIC **dropp;
 	OPENFILE *opf1;

	*stmt = NULL;
	/* allocate a structure to describe the DROP statement */
	dropp = (S_GENERIC **) memalloc(sizeof(S_GENERIC), MEMFLAGS_ZEROFILL);
	if (dropp == NULL) return sqlerrnum(SQLERR_PARSE_NOMEM);
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* get first symbol, must be INDEX or TABLE */
	if (symtype == STABLE) {
/** DROP TABLE [IF EXISTS] table_name **/
		errorflag = TRUE;
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
		if (symtype == SIF) {
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (symtype == SEXISTS) {
				errorflag = FALSE;
			}
			else {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword EXISTS", NULL);
				goto procerror;
			}
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
		}
		if (sqlcattype != CID) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected table reference after keywords DROP TABLE", NULL);
			goto procerror;
		}
		strucpy(name, sqlsymbol);
		tablenum = findtablenum(name);
		if (tablenum == 0) {
			if (errorflag) {
				sqlerrnummsg(SQLERR_PARSE_TABLENOTFOUND, sqlsymbol, NULL);
				goto procerror;
			}
			else goto noerror;
		}

		for (openfilenum = 0; ++openfilenum <= connection.numopenfiles; ) {
			opf1 = openfileptr(openfilenum);
			if (opf1->tablenum == tablenum) {
				closetablefiles(openfilenum); /* closes all open text files and indexes in the table */
				break;
			}
		}

		tab1 = tableptr(tablenum);
		filenum = fioopen(*connection.nametable + tab1->textfilename, (tab1->rioopenflags & FIO_M_MASK) | FIO_P_TXT);
		if (filenum > 0) fiokill(filenum);
		else fioclose(filenum);

		memfree((UCHAR **) tab1->hcolumnarray);
		for (i1 = 0; ++i1 <= tab1->numindexes; ) {
			idx1 = indexptr(tab1, i1);
			filenum = fioopen(*connection.nametable + idx1->indexfilename, (tab1->rioopenflags & FIO_M_MASK) | FIO_P_TXT);
			if (filenum > 0) fiokill(filenum);
			else fioclose(filenum);
			memfree((UCHAR **) idx1->hkeys);
			memfree((UCHAR **) idx1->hcolkeys);
		}
		memfree((UCHAR **) tab1->hindexarray);
		memmove(*connection.htablearray + (tablenum - 1), *connection.htablearray + tablenum, sizeof(struct TABLE_STRUCT) * (connection.numtables - tablenum));

		connection.numtables--;
		connection.numalltables--;
	}
	else if (symtype == SINDEX || symtype == SUNIQUE || symtype == SASSOCIATIVE) {
/** DROP INDEX table_name.index_name **/
		aimdex = FALSE;
		if (symtype == SASSOCIATIVE || symtype == SUNIQUE) {
			if (symtype == SASSOCIATIVE) aimdex = TRUE;
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (symtype != SINDEX) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword INDEX during parsing of DROP", NULL);
				goto procerror;
			}
		}
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
		if (sqlcattype != CID) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected table reference after keywords DROP TABLE", NULL);
			goto procerror;
		}
		strucpy(name, sqlsymbol);
		tablenum = findtablenum(name);
		if (tablenum == 0) {
			sqlerrnummsg(SQLERR_PARSE_TABLENOTFOUND, sqlsymbol, NULL);
			goto procerror;
		}
		tab1 = tableptr(tablenum);

		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
		if (symtype != SPERIOD) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected '.' after table name in DROP INDEX", NULL);
			goto procerror;
		}
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
		if (sqlcattype != CID) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "expected index name after '.' in DROP INDEX", NULL);
			goto procerror;
		}
		strcpy(indexname, sqlsymbol);
		if (aimdex) miofixname(indexname, ".aim", FIXNAME_EXT_ADD);
		else miofixname(indexname, ".isi", FIXNAME_EXT_ADD);
		i1 = findindexnum(tab1, indexname);
		if (!i1) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "index file not found", indexname);
			goto procerror;
		}
		idx1 = indexptr(tab1, i1);
		closetableindex(tablenum, i1, idx1);
		filenum = fioopen(*connection.nametable + idx1->indexfilename, (tab1->rioopenflags & FIO_M_MASK) | FIO_P_TXT);
		if (filenum > 0) fiokill(filenum);
		else fioclose(filenum);
		memfree((UCHAR **) idx1->hkeys);
		memfree((UCHAR **) idx1->hcolkeys);
		memmove(*tab1->hindexarray + (i1 - 1), *tab1->hindexarray + i1, sizeof(struct INDEX_STRUCT) * (tab1->numindexes - i1));

		tab1->numindexes--;
	}
	else {
		sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword TABLE or INDEX after keyword DROP", NULL);
		goto procerror;
	}
	/* write out new dbd file */
	if (writedbdfile(&connection) != 0) {
		sqlerrnummsg(SQLERR_PARSE_ERROR, cfggeterror(), NULL);
		goto procerror;
	}

noerror:
 	(*dropp)->stype = STYPE_DROP; /* set structure type */
	*stmt = dropp;
	return 0;

procerror:
	memfree((unsigned char **) dropp);
	return -1;
}

static INT parsealter(S_GENERIC ***stmt) {
 	INT i1, i2, i3, i4, type;
 	INT filenum, coloffset, symtype, tablenum, aimdex, allowdups;
 	CHAR format[32], datatype[64], buffer[1025], name[MAX_NAME_LENGTH + 1], indexname[64], **nametable, *ptr;
 	INT nametablesize, nametablealloc;
 	OFFSET size;
 	S_GENERIC **alterp;
 	TABLE *tab1;
 	INDEX *idx1, index;
 	COLUMN column;
	IDXKEY *idxkey1, idxkey[255];
	CHAR *cptr;		/* generic working pointer to a string */

	*stmt = NULL;
	/* allocate a structure to describe the ALTER statement */
	alterp = (S_GENERIC **) memalloc(sizeof(S_GENERIC), MEMFLAGS_ZEROFILL);
	if (alterp == NULL) return sqlerrnum(SQLERR_PARSE_NOMEM);
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* get first symbol, must be TABLE */
	if (symtype != STABLE) {
		sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword TABLE after keyword ALTER", NULL);
		goto procerror;
	}
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
	if (sqlcattype != CID) {
		sqlerrnummsg(SQLERR_PARSE_ERROR, "expected table name", NULL);
		goto procerror;
	}
	strucpy(name, sqlsymbol);
	tablenum = findtablenum(name);
	if (!tablenum) {
		sqlerrnummsg(SQLERR_PARSE_TABLEALREADYEXISTS, sqlsymbol, NULL);
		goto procerror;
	}
	tab1 = tableptr(tablenum);
	symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
	for (;;) {
altertop:
		if (symtype == SADD) {
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (symtype == SCOLUMN) symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			else if (symtype == SINDEX || symtype == SUNIQUE || symtype == SASSOCIATIVE) {
/** ALTER TABLE ADD [UNIQUE | ASSOCIATIVE] INDEX index_name (column_name[(length)] [,n]) **/
				allowdups = TRUE;
				aimdex = FALSE;
				if (symtype == SUNIQUE) {
					allowdups = FALSE;
					symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
					if (symtype != SINDEX) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword INDEX after keyword UNIQUE", NULL);
						goto procerror;
					}
				}
				else if (symtype == SASSOCIATIVE) {
					aimdex = TRUE;
					symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
					if (symtype != SINDEX) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword INDEX after keyword ASSOCIATIVE", NULL);
						goto procerror;
					}
				}
				if (symtype != SINDEX) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword INDEX or TABLE after keyword CREATE", NULL);
					goto procerror;
				}

				/* get index name */
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (sqlcattype != CID) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "expected index name after CREATE INDEX", NULL);
					goto procerror;
				}
				strcpy(indexname, sqlsymbol);

				if (tab1->flags & TABLE_FLAGS_READONLY) {
					sqlerrnummsg(SQLERR_PARSE_READONLY, sqlsymbol, NULL);
					goto procerror;
				}
				if (aimdex) miofixname(indexname, ".aim", FIXNAME_EXT_ADD);
				else miofixname(indexname, ".isi", FIXNAME_EXT_ADD);
				if (findindexnum(tab1, indexname)) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "index already exists with the name", indexname);
					goto procerror;
				}

				/* scan LPAREN which is start of index range definitions */
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (symtype != SLPAREN) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "expected '('", NULL);
					goto procerror;
				}

				i1 = (INT)strlen(indexname);
				if (i1 > connection.maxtableindex) connection.maxtableindex = i1;
				if (tab1->numindexes == 0) {
					tab1->hindexarray = (HINDEX) memalloc(sizeof(INDEX), MEMFLAGS_ZEROFILL);
					if (tab1->hindexarray == NULL) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "insufficient memory", NULL);
						goto procerror;
					}
				}
				else if (memchange((UCHAR **) tab1->hindexarray, (tab1->numindexes + 1) * sizeof(INDEX), MEMFLAGS_ZEROFILL) < 0) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "insufficient memory", NULL);
					goto procerror;
				}

				memset(&index, 0, sizeof(INDEX));
				for (i2 = 0;;) {
					/* parse column name */
					symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
					if (sqlcattype != CID) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "expected column name", NULL);
						goto procerror;
					}
					i1 = findcolumnintable(tablenum, sqlsymbol);
					if (!i1) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "column not found", sqlsymbol);
						goto procerror;
					}
					/* temporarily store column offset and size for building utility arguments */
					column = *(*tab1->hcolumnarray + (i1 - 1));
					idxkey[i2].len = column.fieldlen;
					idxkey[i2].pos = column.offset;
					/* look for column length override */
					symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
					if (symtype == SLPAREN) {
						symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
						if (symtype != SNUMBER) {
							sqlerrnummsg(SQLERR_PARSE_ERROR, "expected key length", NULL);
							goto procerror;
						}
						idxkey[i2].len = atoi((CHAR *)sqlliteral); /* override length */
						symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
						if (symtype != SRPAREN) {
							sqlerrnummsg(SQLERR_PARSE_ERROR, "expected ')'", NULL);
							goto procerror;
						}
						symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
					}
					i2++;
					if (symtype == SRPAREN) break;
					else if (symtype != SCOMMA) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "expected ','", NULL);
						goto procerror;
					}
				}
				i1 = findindexnum(tab1, indexname);
				closetableindex(tablenum, i1, indexptr(tab1, i1));
				filenum = fioopen(indexname, (tab1->rioopenflags & FIO_M_MASK) | FIO_P_TXT);
				if (filenum > 0) fiokill(filenum); /* if index file already exists, delete it */
				else fioclose(filenum);
				/* generate aimdex/index command line and execute */
				sprintf(buffer, "%s %s ", *connection.nametable + tab1->textfilename, indexname);
				for (i1 = 0; i1 < i2; i1++) {
					mscitoa(idxkey[i1].pos + 1, format);
					strcat(buffer, format);
					strcat(buffer, "-");
					mscitoa(idxkey[i1].pos + idxkey[i1].len, format);
					strcat(buffer, format);
					strcat(buffer, " ");

				}
				strcat(buffer, "-j ");
				if (!aimdex && allowdups) strcat(buffer, "-d"); /* duplicates allowed */
				if (utility(aimdex ? UTIL_AIMDEX : UTIL_INDEX, buffer, -1) != 0) {
					utilgeterror(buffer, sizeof(buffer));
					sqlerrnummsg(SQLERR_OTHER, buffer, NULL);
					goto procerror;
				}
				/* add index file name permanently to the name table */
				index.indexfilename =
					cfgaddname(indexname, &connection.nametable, &connection.nametablesize, &connection.nametablealloc);
				if (index.indexfilename == RC_ERROR) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, cfggeterror(), NULL);
					goto procerror;
				}
				*(*tab1->hindexarray + tab1->numindexes++) = index; /* copy & increment count */
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				continue; /* done */
			}
/** ALTER TABLE ADD COLUMN column_definition **/
			if (!(tab1->rioopenflags & RIO_FIX)) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "cannot add columns to table with variable length records", NULL);
				goto procerror;
			}

			/* parse through column list */
			for (;;) {
				/* make temporary name table, since additions to real name table cannot be undone in case of error */
				nametablealloc = 128;
				nametable = (CHAR **) memalloc(nametablealloc, 0);
				if (nametable == NULL) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "insufficient memory", NULL);
					goto procerror;
				}
				nametablesize = 1; /* 0 for index is ambiguous, so make first index start at position 1 */

				/* parse column name */
				if (sqlcattype != CID) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "expected column name", NULL);
					memfree((UCHAR **)nametable);
					goto procerror;
				}
				if (!validatesqlname(sqlsymbol, sqllength, FALSE)) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid column name", sqlsymbol);
					memfree((UCHAR **)nametable);
					goto procerror;
				}

				/* initialize column structure and store name in temp name table */
				memset(&column, 0, sizeof(COLUMN));
				i2 = (INT)strlen(sqlsymbol);
				i1 = nametablesize;
				while (i1 + i2 >= nametablealloc) {
					if (memchange((UCHAR **) nametable, nametablealloc + 4096, 0) == -1) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "insufficient memory", NULL);
						memfree((UCHAR **)nametable);
						goto procerror;
					}
					nametablealloc += 4096;
				}
				strucpy(*nametable + i1, sqlsymbol);
				if (findcolumnintable(tablenum, *nametable + i1) > 0) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "column with that name already exists", sqlsymbol);
					memfree((UCHAR **)nametable);
					goto procerror;
				}
				nametablesize += i2 + 1;
				column.name = i1;

				/* scan data type */
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (sqlcattype != CID) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "expected column data type", NULL);
					memfree((UCHAR **)nametable);
					goto procerror;
				}
				strcpy(datatype, sqlsymbol);

				/* scan everything from LPAREN up to RPAREN and store in variable 'datatype' */
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (symtype != SLPAREN) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "expected '('", NULL);
					memfree((UCHAR **)nametable);
					goto procerror;
				}
				strcat(datatype, "(");
				for (;;) {
					symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
					if (symtype == SERROR || symtype == SEND) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid data type specification for column", *nametable + column.name);
						memfree((UCHAR **)nametable);
						goto procerror;
					}
					else if (symtype == SRPAREN) {
						strcat(datatype, ")");
						break;
					}
					else if (symtype == SCOMMA) strcat(datatype, ",");
					else if (symtype == SSLASH) strcat(datatype, "/");
					else if (symtype == SCOLON) strcat(datatype, ":");
					else if (symtype == SHYPHEN) strcat(datatype, "-");
					else if (symtype == SPERIOD) strcat(datatype, ".");
					else if (symtype == SNUMBER || symtype == SLITERAL) {
						sqlliteral[sqllength] = '\0';
						strcat(datatype, (CHAR *)sqlliteral);
					}
					else if (sqlcattype == CID) {
						strcat(datatype, sqlsymbol);
					}
					else {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid data type specification for column", *nametable + column.name);
						memfree((UCHAR **)nametable);
						goto procerror;
					}
				}

				if (coltype(datatype, 0, &column, format) == -1) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, cfggeterror(), NULL);
					memfree((UCHAR **)nametable);
					goto procerror;
				}
				if (format[0]) {
					i2 = (INT)strlen(format);
					i1 = nametablesize;
					while (i1 + i2 >= nametablealloc) {
						if (memchange((UCHAR **) nametable, nametablealloc + 4096, 0) == -1) {
							sqlerrnummsg(SQLERR_PARSE_ERROR, "insufficient memory", NULL);
							memfree((UCHAR **)nametable);
							goto procerror;
						}
						nametablealloc += 4096;
					}
					strcpy(*nametable + i1, format);
					nametablesize += i2 + 1;
					column.format = i1;
				}

				if (!tab1->numcolumns) {
					tab1->hcolumnarray = (HCOLUMN) memalloc(sizeof(COLUMN), 0);
					if (tab1->hcolumnarray == NULL) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "insufficient memory", NULL);
						memfree((UCHAR **)nametable);
						goto procerror;
					}
				}
				else if (memchange((UCHAR **) tab1->hcolumnarray, (tab1->numcolumns + 1) * sizeof(COLUMN), 0) < 0) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "insufficient memory", NULL);
					memfree((UCHAR **)nametable);
					goto procerror;
				}

				/* set i1 to 0 based column index */
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
#if 0
				if (symtype == SFIRST) {
					/* copy to first position */
					i1 = 0;
					coloffset = 0;
					/* generate reformat command line and execute */
					sprintf(buffer, "%s %s.wrk -b=%d 1-%d",
						*connection.nametable + tab1->textfilename,
						*connection.nametable + tab1->textfilename,
						column.fieldlen, tab1->reclength < 0 ? tab1->reclength * -1 : tab1->reclength
					);
					/* read next symbol */
					symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				}
				else if (symtype == SAFTER) {
					/* copy after specified position (middle or last) */
					symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
					if (sqlcattype != CID) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "expected column name for ADD COLUMN", NULL);
						memfree((UCHAR **)nametable);
						goto procerror;
					}
					i1 = findcolumnintable(tablenum, sqlsymbol);
					if (i1 == 0) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "column was not found in table", sqlsymbol);
						memfree((UCHAR **)nametable);
						goto procerror;
					}
					if (i1 != tab1->numcolumns) { /* middle */
						coloffset = ((COLUMN)*(*tab1->hcolumnarray + (i1 - 1))).offset + ((COLUMN)*(*tab1->hcolumnarray + (i1 - 1))).fieldlen;
						/* generate reformat command line and execute */
						sprintf(buffer, "%s %s.wrk 1-%d -b=%d %d-%d",
							*connection.nametable + tab1->textfilename,
							*connection.nametable + tab1->textfilename,
							coloffset, column.fieldlen, coloffset + 1,
							tab1->reclength < 0 ? tab1->reclength * -1 : tab1->reclength
						);

					}
					else {
						/* copy to last position */
						coloffset = tab1->reclength < 0 ? tab1->reclength * -1 : tab1->reclength;
						/* generate reformat command line and execute */
						sprintf(buffer, "%s %s.wrk 1-%d -b=%d",
							*connection.nametable + tab1->textfilename,
							*connection.nametable + tab1->textfilename,
							tab1->reclength < 0 ? tab1->reclength * -1 : tab1->reclength, column.fieldlen
						);
					}
					/* read next symbol */
					symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				}
				else {
#endif
					/* copy to last position */
					i1 = tab1->numcolumns;
					/* copy to last position */
					coloffset = tab1->reclength < 0 ? tab1->reclength * -1 : tab1->reclength;
					/* generate reformat command line and execute */
					sprintf(buffer, "%s %s.wrk 1-%d -b=%d",
						*connection.nametable + tab1->textfilename,
						*connection.nametable + tab1->textfilename,
						tab1->reclength < 0 ? tab1->reclength * -1 : tab1->reclength, column.fieldlen
					);
#if 0
				}
#endif
				type = tab1->rioopenflags & RIO_T_MASK;
				if (type == RIO_T_TXT) strcat(buffer, " -t=text");
				else if (type == RIO_T_DAT) strcat(buffer, " -t=data");
				else if (type == RIO_T_DOS) strcat(buffer, " -t=crlf");
				else if (type == RIO_T_MAC) strcat(buffer, " -t=mac");
				else if (type == RIO_T_BIN) strcat(buffer, " -t=bin");

				/* POINT OF NO RETURN */

				filenum = fioopen(*connection.nametable + tab1->textfilename, (tab1->rioopenflags & FIO_M_MASK) | FIO_P_TXT);
				if (filenum <= 0) {
					fioclose(filenum);
					sqlerrnummsg(SQLERR_PARSE_ERROR, "could not open data file", NULL);
					memfree((UCHAR **)nametable);
					goto procerror;
				}
				fiogetsize(filenum, &size);
				fioclose(filenum);
				if (size > 1) {	/* only reformat if file has records in it */
					if (utility(UTIL_REFORMAT, buffer, -1) != 0) {
						utilgeterror(buffer, sizeof(buffer));
						sqlerrnummsg(SQLERR_OTHER, buffer, NULL);
						memfree((UCHAR **)nametable);
						goto procerror;
					}

					filenum = fioopen(*connection.nametable + tab1->textfilename, (tab1->rioopenflags & FIO_M_MASK) | FIO_P_TXT);
					if (filenum > 0) {
						fiokill(filenum);
					}
					else {
						fioclose(filenum);
						sqlerrnummsg(SQLERR_PARSE_ERROR, "could not delete old data file", NULL);
						memfree((UCHAR **)nametable);
						goto procerror;
					}

					sprintf(buffer, "%s.wrk %s", *connection.nametable + tab1->textfilename, *connection.nametable + tab1->textfilename);
					if (utility(UTIL_RENAME, buffer, -1) != 0) {
						utilgeterror(buffer, sizeof(buffer));
						sqlerrnummsg(SQLERR_OTHER, buffer, NULL);
						memfree((UCHAR **)nametable);
						goto procerror;
					}
				}
				for (i2 = 0; i2 < tab1->numindexes; i2++) {
					idx1 = *tab1->hindexarray + i2;
					filenum = fioopen(*connection.nametable + idx1->indexfilename, (tab1->rioopenflags & FIO_M_MASK) | FIO_P_TXT);
					if (filenum < 1) {
						sqlerrnummsg(filenum, "error in opening index file", *connection.nametable + idx1->indexfilename);
						memfree((UCHAR **)nametable);
						goto procerror;
					}
					/* read index header to get command line and index type */
					i3 = fioread(filenum, 0, (UCHAR *) buffer, 1024);
					if (i3 < 512) {
						if (i3 >= 0) sqlerrnummsg(SQLERR_BADINDEX, "index was smaller than minimum index size of 512 bytes", *connection.nametable + idx1->indexfilename);
						else sqlerrnummsg(i3, "error in reading index file", *connection.nametable + idx1->indexfilename);
						memfree((UCHAR **)nametable);
						goto procerror;
					}
					fioclose(filenum);
					aimdex = (buffer[0] == 'A');
					memmove(buffer, buffer + 101, sizeof(buffer) - (sizeof(CHAR) * 101));
					for (ptr = buffer; (UCHAR) *ptr != DBCDEL; ptr++) {
						if ((UCHAR) *ptr == DBCEOR) *ptr = ' ';
					}
					*ptr = '\0';
					if (utility(aimdex ? UTIL_AIMDEX : UTIL_INDEX, buffer, -1) != 0) {
						utilgeterror(buffer, sizeof(buffer));
						sqlerrnummsg(SQLERR_OTHER, buffer, NULL);
						memfree((UCHAR **)nametable);
						goto procerror;
					}

				}
				if (i1 == 0) memmove(*tab1->hcolumnarray + 1, *tab1->hcolumnarray, sizeof(HCOLUMN) * tab1->numcolumns);
				else if (i1 != tab1->numcolumns) memmove(*tab1->hcolumnarray + (i1 + 1), *tab1->hcolumnarray + i1, sizeof(struct COLUMN_STRUCT) * (tab1->numcolumns - i1));
#if 0
				/* adjust offset for remaining columns - required for when column is inserted at postion other than the end */
				for (i2 = i1 + 1; i2 < (tab1->numcolumns + 1); i2++) {
					((COLUMN)*(*tab1->hcolumnarray + i2)).offset += column.fieldlen;
				}
#endif
				column.offset = (USHORT) coloffset;
				*(*tab1->hcolumnarray + i1) = column; /* copy */
				tab1->numcolumns++;
				if (tab1->reclength < 0) tab1->reclength -= column.fieldlen;
				else tab1->reclength += column.fieldlen;

				/* store column names from temp name table into real name table */

				cptr = *nametable + (*(tab1->hcolumnarray))[i1].name;
				(*(tab1->hcolumnarray))[i1].name =
					cfgaddname(cptr, &connection.nametable, &connection.nametablesize, &connection.nametablealloc);
				if ((*(tab1->hcolumnarray))[i1].format) {
					cptr = *nametable + (*(tab1->hcolumnarray))[i1].format;
					(*(tab1->hcolumnarray))[i1].format =
						cfgaddname(cptr, &connection.nametable, &connection.nametablesize, &connection.nametablealloc);
				}
				if ((*(tab1->hcolumnarray))[i1].name == RC_ERROR || (*(tab1->hcolumnarray))[i1].format == RC_ERROR) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, cfggeterror(), NULL);
					memfree((UCHAR **)nametable);
					goto procerror;
				}
				memfree((UCHAR **)nametable);

				if (symtype == SEND) goto altertop;
				else if (symtype == SCOMMA) {
					symtype = peeksymbol();
					if (symtype == SADD || symtype == SCHANGE || symtype == SMODIFY || symtype == SDROP || symtype == SRENAME) {
						symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
						goto altertop; /* done with column specifications, parse another alter clause */
					}
					/* continue */
				}
				else {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "incorrect syntax for TABLE ALTER ADD COLUMN", NULL);
					goto procerror;
				}
			}
		}
		else if (symtype == SDROP) {
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (symtype == SINDEX || symtype == SUNIQUE || symtype == SASSOCIATIVE) {
				aimdex = FALSE;
				if (symtype == SASSOCIATIVE || symtype == SUNIQUE) {
					if (symtype == SASSOCIATIVE) aimdex = TRUE;
					symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
					if (symtype != SINDEX) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "expected keyword INDEX during parsing of DROP", NULL);
						goto procerror;
					}
				}
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (sqlcattype != CID) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "expected index name for DROP INDEX", NULL);
					goto procerror;
				}
				/* find index */
				strcpy(indexname, sqlsymbol);
				if (aimdex) miofixname(indexname, ".aim", FIXNAME_EXT_ADD);
				else miofixname(indexname, ".isi", FIXNAME_EXT_ADD);
				i1 = findindexnum(tab1, indexname);
				if (!i1) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "DROP INDEX failed, index not found", indexname);
					goto procerror;
				}
				idx1 = indexptr(tab1, i1);
				closetableindex(tablenum, i1, idx1);
				/* delete file */
				filenum = fioopen(*connection.nametable + idx1->indexfilename, (tab1->rioopenflags & FIO_M_MASK) | FIO_P_TXT);
				if (filenum > 0) fiokill(filenum);
				else fioclose(filenum);
				/* delete data structures */
				memfree((UCHAR **) idx1->hkeys);
				memfree((UCHAR **) idx1->hcolkeys);
				memmove(*tab1->hindexarray + (i1 - 1), *tab1->hindexarray + i1, sizeof(struct INDEX_STRUCT) * (tab1->numindexes - i1));
				tab1->numindexes--;
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			}
			else {
				if (symtype == SCOLUMN) symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (sqlcattype != CID) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "expected column name for DROP", NULL);
					goto procerror;
				}
				for (i1 = (INT)strlen(sqlsymbol) - 1; i1 >= 0; i1--) sqlsymbol[i1] = toupper(sqlsymbol[i1]);
				i1 = findcolumnintable(tablenum, sqlsymbol);
				if (i1 == 0) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "ALTER DROP COLUMN error, column was not found in table", sqlsymbol);
					goto procerror;
				}
				if (!(tab1->flags & TABLE_FLAGS_COMPLETE)) {
					if (maketablecomplete(tablenum) != 0) goto procerror;
				}
	/** TODO: This code could be for more efficient **/
				/* check indexes and make sure that column is not part of one */
				for (i2 = 0; i2 < tab1->numindexes; i2++) {
					idx1 = *tab1->hindexarray + i2;
					for (i3 = 0; i3 < idx1->numkeys; i3++) {
						idxkey1 = *idx1->hkeys + i3;
						column = *(*tab1->hcolumnarray + (i1 - 1));
						for (i4 = idxkey1->pos; i4 < (idxkey1->len + idxkey1->pos); i4++) {
							if (i4 >= column.offset && i4 < (column.offset + column.fieldlen)) {
								break;
							}
						}
						if (i4 != (idxkey1->len + idxkey1->pos)) {
							break; /* broke out of loop early */
						}
					}
					if (i3 != idx1->numkeys) { /* broke out of loop early */
						sqlerrnummsg(SQLERR_PARSE_ERROR, "DROP COLUMN failed, an index exists on column", NULL);
						goto procerror;
					}
				}
				memmove(*tab1->hcolumnarray + (i1 - 1), *tab1->hcolumnarray + i1, sizeof(struct COLUMN_STRUCT) * (tab1->numcolumns - i1));
				tab1->numcolumns--;
				symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			}
		}
		else if (symtype == SRENAME) {
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (symtype == STO) symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (sqlcattype != CID && sqlcattype != CLITERAL) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "expected table name for rename", NULL);
				goto procerror;
			}
			if (!validatesqlname(sqlsymbol, sqllength, sqlcattype == CLITERAL)) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid table name for rename", sqlsymbol);
				goto procerror;
			}
			if (findtablenum(sqlsymbol)) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "table rename failed, name already exists", sqlsymbol);
				goto procerror;
			}
			if (strlen(*connection.nametable + tab1->name) <= strlen(sqlsymbol)) {
				strcpy(*connection.nametable + tab1->name, sqlsymbol);
			}
			else {
				tab1->name = cfgaddname(sqlsymbol, &connection.nametable, &connection.nametablesize, &connection.nametablealloc);
			}
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
		}
		else if (symtype == SCOMMA) {
			symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);
			continue;
		}
		else if (symtype == SEND) break;
		else {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "incorrect syntax in TABLE ALTER statement", NULL);
			goto procerror;
		}
	}

	/* replace old dbd file with new one, file name is stored in connection structure */
	if (writedbdfile(&connection) != 0) {
		sqlerrnummsg(SQLERR_PARSE_ERROR, cfggeterror(), NULL);
		goto procerror;
	}

 	(*alterp)->stype = STYPE_ALTER; /* set structure type */
	*stmt = alterp;
	return 0;

procerror:
	memfree((unsigned char **) alterp);
	return RC_ERROR;
}

/*
 * 	scansymbol
 *		Scan the next symbol from the SQL statement and identify it.
 *	Entry:
 *		sqlstatement  = pointer to SQL statement, null-terminated
 *		sqlstmtlength = length of the SQL statement
 *		sqloffset     = offset to current scan point, 0 means beginning
 *	Exit:
 *		next symbol or punctuation scanned
 *		sqlsymtype = type of SQL item found: punctuation, keyword, literal, error or unknown
 *		sqlsymbol  = value if keyword or identifier
 *		sqlliteral = value if literal or number scanned
 *		sqllength  = length of keyword, identifier, literal or number
 *	Returns:
 *		sqlsymtype, identifier of symbol
 */
static int scansymbol(int flags)
{
	INT i1, length;
	UCHAR *outp, *scanp, *scanp1;

	sqlsymbol[0] = '\0';
	sqlcattype = COTHER;  /* init category to other */

	length = 1;
	scanp = sqlstatement + sqloffset;
	/**
	 * jpr 16SEP2015
	 * The below was looking for space, '\n' and '\r'.
	 * I changed it to use the isspace() function so that it skips tabs.
	 * One of PPro's users liked to put tabs in for readability.
	 */
	while (isspace(*scanp)) {
		scanp++;
		sqloffset++;
	}
	if (*scanp == '\0') {
		sqlsymtype = SEND;
		sqlcattype = CEND;
		sqllastoffset = sqloffset;
		return SEND;
	}
	switch (*scanp) {
	/* check for punctuation and operator characters first */
	case ',': sqlsymtype = SCOMMA; break;
	case '(': sqlsymtype = SLPAREN; break;
	case ')': sqlsymtype = SRPAREN; break;
	case '?': sqlsymtype = SQUESTION; break;
	case '*': sqlsymtype = SASTERISK; break;
	case '/': sqlsymtype = SDIVIDE; break;
	case '=': sqlsymtype = SEQUAL; break;
	case ':': sqlsymtype = SCOLON; break;
	case '<':
		switch (*(scanp + 1)) {
		case '=': sqlsymtype = SLESSEQUAL; length = 2; break;
		case '>': sqlsymtype = SNOTEQUAL; length = 2; break;
		default:  sqlsymtype = SLESS; break;
		}
		break;
	case '>':
		if (*(scanp + 1) == '=') {
			sqlsymtype = SGREATEREQUAL;
			length = 2;
		}
		else sqlsymtype = SGREATER;
		break;
	case '|':
		if (*(scanp + 1) == '|') {
			sqlsymtype = SCONCAT;
			length = 2;
		}
		else {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid character", NULL);
			sqlsymtype = SERROR;
			sqlcattype = CERROR;
			break;
		}
		break;
	case '\'':
		/* Literal, look for end quote but look for \ as escape character */
		scanp1 = scanp + 1;
		outp = sqlliteral;
		i1 = 0;
		while (*scanp1 != '\'' && *scanp1 != '\0') {
			if (*scanp1 == '\\' && *(scanp1 + 1) != '\0') scanp1++;
			*outp++ = *scanp1++;
			if (++i1 == MAX_LIT_SIZE) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "literal too long", NULL);
				sqlsymtype = SERROR;
				sqlcattype = CERROR;
				length = i1;
				i1 = -1;
				break;
			}
		}
		if (i1 == -1) break;
		if (*scanp1 != '\'') {				/* no closing apostrophe, invalid literal */
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid literal, no closing apostrophe", NULL);
			sqlsymtype = SERROR;
			sqlcattype = CERROR;
		}
		else {
			scanp1++;  /* skip past closing apostrophe */
			sqllength = (INT)(ptrdiff_t)(outp - sqlliteral);
			length = (INT)(ptrdiff_t)(scanp1 - scanp);
			sqlsymtype = SLITERAL;
			sqlcattype = CLITERAL;
		}
		break;
	default:  /* word, number or error */
		if (isalpha(*scanp)) {
			/* alpha char. place the word in sqlsymbol and see if it's an SQL keyword */
			scanp1 = (UCHAR *) strpbrk((CHAR *) scanp, SDELIMSTRING);
			length = (INT)((scanp1 == NULL) ? (INT)strlen((CHAR *) scanp) : (INT)(ptrdiff_t)(scanp1 - scanp));
			if (length > MAX_SYM_SIZE) {
				/* word too long, syntax error */
				sqlerrnummsg(SQLERR_PARSE_ERROR, "word too long", NULL);
				sqlsymtype = SERROR;
				sqlcattype = CERROR;
			}
			else {
				memcpy(sqlsymbol, scanp, (size_t) length);
				sqlsymbol[length] = '\0';
				sqllength = length;
				sqlsymtype = issqlkeyword();	/* see if SQL keyword or identifier */
				sqlcattype = CID;			/* category is identifier */
			}
			break;
		}
		if (*scanp == '.' || *scanp == '-' || *scanp == '+') {  /* check for number */
			if (!isdigit(*(scanp + 1)) || (*scanp != '.' && (flags & (PARSE_PLUS | PARSE_MINUS)))) {
				if (*scanp == '.') sqlsymtype = SPERIOD;
				else if (*scanp == '-') sqlsymtype = SMINUS;
				else sqlsymtype = SPLUS;
				break;
			}
		}
		else if (!isdigit(*scanp)) {  /* not a valid punctutation character, literal, alpha char or number, syntax error */
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid character", NULL);
			sqlsymtype = SERROR;
			sqlcattype = CERROR;
			break;
		}

		/* number */
		outp = sqlliteral;
		if (*scanp == '+') {
			scanp++;
			sqloffset++;
		}
		scanp1 = scanp;
		if (*scanp1 == '-') *outp++ = *scanp1++;
		while (isdigit(*scanp1)) *outp++ = *scanp1++;  /* handle integer portion */
		if (*scanp1 == '.') *outp++ = *scanp1++;  /* if decimal point, copy it */
		while (isdigit(*scanp1)) *outp++ = *scanp1++;  /* handle fractional part */
		sqllength = length = (INT)(ptrdiff_t)(scanp1 - scanp);
		sqlsymtype = SNUMBER;
		sqlcattype = CNUMBER;
	}
	sqllastoffset = sqloffset;
	sqloffset += length;	/* update current scan point for next time */
	return sqlsymtype;		/* return identification */
}

/*	issqlkeyword */
/*		Check if sqlsymbol is an SQL keyword. */
/*	Entry: */
/*		sqlsymbol = the keyword to check */
/*		sqllength = length of symbol */
/*	Exit: */
/*		The symbol type, SID if not SQL keyword */
static int issqlkeyword(void)
{
	unsigned int count;
	CHAR symbol[MAX_SYM_SIZE + 1];

	strucpy(symbol, sqlsymbol);
	for (count = 0; count < NUMSQLKEYWORDS; count++) {
		if (sqllength == sqlsymbols[count].length &&
		    memcmp(symbol, sqlsymbols[count].name, sqllength) == 0) return sqlsymbols[count].symtype;
	}
	return SID;
}

/* new findtable */
/* return tablenum or zero if not found */
static INT findtablenum(CHAR *tablename)
{
	INT i1, i2, i3;
	CHAR *name;
	TABLE *tab1;

	for (i1 = 0, i2 = connection.numtables; ++i1 <= i2; ) {
		tab1 = tableptr(i1);
		name = nameptr(tab1->name);
		if (tab1->flags & TABLE_FLAGS_TEMPLATE) {
			for (i3 = 0; name[i3] && ((name[i3] == '?' && tablename[i3] && tablename[i3] != '?') || name[i3] == tablename[i3]); i3++);
			if (!name[i3] && !tablename[i3]) return i1;
		}
		else if (!strcmp(name, tablename)) return i1;
	}
	return 0;
}

/* return index of index, or zero if index not found in table */
static INT findindexnum(TABLE *tab1, CHAR *indexname)
{
	INT i1, i2, i3;
	INDEX *index;
	for (i1 = 0, i3 = (INT)strlen(indexname); i1 < tab1->numindexes; i1++) {
		index = *tab1->hindexarray + i1;
		if ((INT)strlen(*connection.nametable + index->indexfilename) < i3) continue;
		for (i2 = 0; i2 < i3; i2++) {
			if (toupper(indexname[i2]) != toupper((*connection.nametable + index->indexfilename)[i2])) {
				break;
			}
		}
		if (i2 == i3) break; /* match */
	}
	if (i1 != tab1->numindexes) {
		return ++i1;
	}
	return 0;
}

/* new findcolumnintable */
/* return column number or zero if not found */
static int findcolumnintable(int tablenum, char *columnname)
{
	int i1, i2;
	TABLE *tab1;
	COLUMN *col1;

	if (tablenum < 1 || tablenum > connection.numalltables) return 0;
	tab1 = tableptr(tablenum);
	for (i1 = 0, i2 = tab1->numcolumns; ++i1 <= i2; ) {
		col1 = columnptr(tab1, i1);
		if (!strcmp(nameptr(col1->name), columnname)) return i1;
	}
	return 0;
}

/*	findcolumn */
/*		Find a column name in the selected list of tables and determine its table number */
/*		and column number. */
/*		The column may be specified as columnname or as qualifier.columnname. The qualifier */
/*		can be a table name or a correlation name. If the qualifier name is specified, the */
/*		table indicated must be named in the FROM list of the SELECT statement. If the qualifier */
/*		name is not specified, the column must exist in exactly one of the selected tables. */
/*	Entry: */
/*		coldescp    = pointer to column description */
/*		corrtable   = pointer to pointer an array of table correlations for the tables */
/*	Exit: */
/*		tableref	= table reference is index in corrtable (regarding tables) + 1 */
/*		columnnum	= column number */
/*	Returns: */
/*		gettabcolref(tableref, columnnum) if valid, or zero if invalid column */
static UINT findcolumn(COLDESC *coldescp, CORRTABLE *corrtable, INT colcorrflag)
{
	int i1, i2, count, tableref;

	count = corrtable->count;
	if (coldescp->qualname[0]) {  /* column specified as qualifier.columnmname */
		/* check the selected tables for one with the specified table or correlation name */
		for (tableref = 1, i1 = 0; i1 < count; i1++) {
			if (!corrtable->info[i1].tablenum) continue;
			if (!strcmp(coldescp->qualname, corrtable->info[i1].name)) {
				/* found the table, see if the specified column is in the table */
				i2 = findcolumnintable(corrtable->info[i1].tablenum, coldescp->columnname);
				if (i2 == 0) return 0;
				return gettabcolref(tableref, i2);
			}
			tableref++;
		}
		return 0;
	}

	if (colcorrflag) {
		/* check for column correlation/alias */
		for (i1 = 0; i1 < count; i1++) {
			if (corrtable->info[i1].tablenum) continue;
			if (!strcmp(coldescp->columnname, corrtable->info[i1].name)) {
				/* found the column */
				if (corrtable->info[i1].tabcolnum) return corrtable->info[i1].tabcolnum;
				return i1 + 1;
			}
		}
	}

	/* search each selected table for the column name. It must exist in exactly one of the tables. */
	for (tableref = 1, i1 = 0; i1 < count; i1++) {
		if (!corrtable->info[i1].tablenum) continue;
		i2 = findcolumnintable(corrtable->info[i1].tablenum, coldescp->columnname);
		if (i2 > 0) return gettabcolref(tableref, i2);
		tableref++;
	}
	return 0;
}

/*	parsecolumnname */
/*		Parse a column name. */
/*		The name can be specified as columnname or as qualifier.columnname. */
/*		The qualifier can be a table name or a correlation name. */
/*	Entry: */
/*		first name already scanned */
/*	Exit: */
/*		*columndescp = filled with column name */
/*	Returns: */
/*		SQL_SUCCESS if okay, SQL_ERROR if syntax error */
static int parsecolumnname(COLDESC *columndescp)
{
	int symtype;

	strucpy(columndescp->columnname, sqlsymbol);
	columndescp->qualname[0] = 0;

	symtype = peeksymbol();  /* check for qualifier.columnname */
	if (symtype == SPERIOD) {
/* move the name just stored from the column field to the qualifier field and get the column name */
		strcpy(columndescp->qualname, columndescp->columnname);
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* eat the period */
		symtype = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get the column name */
		if (symtype == SASTERISK) strcpy(columndescp->columnname, "*");
		else {
			if (sqlcattype != CID) {  /* not an identifier */
				return sqlerrnummsg(SQLERR_PARSE_ERROR, "expected column name", NULL);
			}
			if (!validatesqlname(sqlsymbol, sqllength, FALSE)) {  /* validate the name */
				return sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid column name", sqlsymbol);
			}
			strucpy(columndescp->columnname, sqlsymbol);
		}
	}
	return 0;
}

/*	peeksymbol */
/*		Peek at the next symbol from the SQL statement and identify it. */
/*	Entry: */
/*		sqlstmtlength = length of the SQL statement */
/*		sqloffset     = offset to current scan point, 0 means beginning */
/*	Exit: */
/*		next symbol or punctuation scanned with scansymbol but the */
/*		offset is restored to it's previous value */
/*		sqlsymtype = type of SQL item found: punctuation, keyword, literal, error or unknown */
/*		sqlsymbol  = value if punctuation or keyword */
/*		sqlliteral = literal value if literal scanned */
/*	Returns: */
/*		sqlsymtype, identifier of symbol */
static int peeksymbol()
{
	int result, saveoffset;

	saveoffset = sqllastoffset;
	result = scansymbol(PARSE_PLUS | PARSE_MINUS);
	sqloffset = saveoffset;
	scansymbol(PARSE_PLUS | PARSE_MINUS);
	return result;
}

/**
 * 	parsewhere
 *
 * 	Parse an SQL WHERE clause for a SELECT, UPDATE or DELETE statement
 *
 *	Entry:
 *		numtables  = number of tables used
 *		corrtable  = pointer to pointer to array of table descriptions for the tables
 *		symtypep   = pointer to scansymbol result
 *
 *	Exit:
 *		WHERE predicates parsed and described
 *		*symtypep  = type of next scanned symbol
 *
 *	Returns:
 *		pointer to pointer to S_WHERE structure if okay, NULL if syntax error
 *
 */
static S_WHERE **parsewhere(INT type, HCORRTABLE corrtable, INT *symtypep)
{
	INT i1;
	S_WHERE **wherep = NULL;
	S_WHERE wherehead;
	LEXPWORK **lexpwork;

	lexpwork = (LEXPWORK **) memalloc(sizeof(LEXPWORK) + (LEXPSIZE - 1) * sizeof(LEXPENTRY), MEMFLAGS_ZEROFILL);
	if (lexpwork == NULL) {	/* memory allocation failure */
		sqlerrnum(SQLERR_PARSE_NOMEM);
		return(NULL);
	}
	(*lexpwork)->max = LEXPSIZE;
	memset(&wherehead, 0, sizeof(wherehead));
	wherehead.stype = STYPE_WHERE;  /* set structure type */
	*symtypep = scansymbol(0);  /* get the next symbol */
	if (!parseors(type, corrtable, symtypep, FALSE, &wherehead, lexpwork)) goto procerror;	/* error in expression */
	wherep = (S_WHERE **) memalloc(sizeof(S_WHERE) + (wherehead.numentries - 1) * sizeof(LEXPENTRY), MEMFLAGS_ZEROFILL);
	if (wherep == NULL) {
		sqlerrnum(SQLERR_PARSE_NOMEM);
		goto procerror;
	}
	**wherep = wherehead;  /* copy in header */
	memcpy((*wherep)->lexpentries, (*lexpwork)->lexpentries, (int)(wherehead.numentries * sizeof(LEXPENTRY)));
	memfree((UCHAR **) lexpwork);
	return wherep;

procerror:
	for (i1 = 0; i1 < wherehead.numentries; i1++) {
		if ((*lexpwork)->lexpentries[i1].type == OPVALUE)
			memfree((UCHAR **)(*lexpwork)->lexpentries[i1].valuep);
		else if ((*lexpwork)->lexpentries[i1].type == OPEXP)
			freestmt((S_GENERIC **)(*lexpwork)->lexpentries[i1].expp);
	}
	memfree((UCHAR **) lexpwork);
	return NULL;
}

/*		Parse an "or" expression and output it in postfix form. */
/* */
/*	Entry: */
/*		numtables    = number of tables used */
/*		corrtable    = pointer to pointer to array of table descriptions for the tables */
/*		symtypep     = pointer to scansymbol result */
/*		whereheadp   = pointer to S_WHERE header */
/*		lexpwork     = pointer to array of entries in expression */
/* */
/*	Exit: */
/*		logical expression parsed and described */
/*		*numentries  = number of entries (operands and operators) in expression */
/*		*lexpwork    = array of entries defining the expression */
/*		*symtypep    = type of next scanned symbol */
/* */
/*	Returns: */
/*		TRUE if expression valid, FALSE if invalid, error given */
/* */
static INT parseors(INT type, HCORRTABLE corrtable,
	INT *symtypep, INT notflag, S_WHERE *whereheadp, LEXPWORK **lexpwork)
{
	INT left, op;

	if (!parseands(type, corrtable, symtypep, notflag, whereheadp, lexpwork)) return FALSE;
	for ( ; ; ) {
		if (*symtypep != SOR) return TRUE;		/* if not OR, end */
		left = whereheadp->numentries - 1;		/* save index of first operand */
		*symtypep = scansymbol(0);  /* get the next symbol */
		if (!parseands(type, corrtable, symtypep, notflag, whereheadp, lexpwork)) return FALSE;
		if (!MORELEXP(lexpwork, whereheadp->numentries)) return FALSE;
		if (notflag) op = OPAND;
		else op = OPOR;
		(*lexpwork)->lexpentries[whereheadp->numentries].type = op;
		(*lexpwork)->lexpentries[whereheadp->numentries].left = left;
		(*lexpwork)->lexpentries[whereheadp->numentries].right = whereheadp->numentries - 1;
		(*lexpwork)->lexpentries[whereheadp->numentries].parent = -1;
		(*lexpwork)->lexpentries[left].parent = whereheadp->numentries;
		(*lexpwork)->lexpentries[whereheadp->numentries - 1].parent = whereheadp->numentries;
		whereheadp->numentries++;
	}
	return TRUE;
}

/*		Parse an "and" for a logical expression and output it in postfix form. */
/* */
/*	Entry: */
/*		numtables    = number of tables used */
/*		corrtable    = pointer to pointer to array of table descriptions for the tables */
/*		symtypep     = pointer to scansymbol result */
/*		whereheadp   = pointer to S_WHERE header */
/*		lexpwork     = pointer to array of entries in expression */
/* */
/*	Exit: */
/*		logical expression parsed and described */
/*		*numentries  = number of entries (operands and operators) in expression */
/*		*lexpwork    = array of entries defining the expression */
/*		*symtypep    = type of next scanned symbol */
/* */
/*	Returns: */
/*		TRUE if expression valid, FALSE if invalid, error given */
static INT parseands(INT type, HCORRTABLE corrtable, // @suppress("No return")
	INT *symtypep, INT notflag, S_WHERE *whereheadp, LEXPWORK **lexpwork)
{
	INT left, op;

	if (!parsenots(type, corrtable, symtypep, notflag, whereheadp, lexpwork)) return FALSE;
	for ( ; ; ) {
		if (*symtypep != SAND) return TRUE;		/* if not AND, end */
		left = whereheadp->numentries - 1;		/* save index of first operand */
		*symtypep = scansymbol(0);  /* get the next symbol */
		if (!parsenots(type, corrtable, symtypep, notflag, whereheadp, lexpwork)) return FALSE;
		if (!MORELEXP(lexpwork, whereheadp->numentries)) return FALSE;
		if (notflag) op = OPOR;
		else op = OPAND;
		(*lexpwork)->lexpentries[whereheadp->numentries].type = op;
		(*lexpwork)->lexpentries[whereheadp->numentries].left = left;
		(*lexpwork)->lexpentries[whereheadp->numentries].right = whereheadp->numentries - 1;
		(*lexpwork)->lexpentries[whereheadp->numentries].parent = -1;
		(*lexpwork)->lexpentries[left].parent = whereheadp->numentries;
		(*lexpwork)->lexpentries[whereheadp->numentries - 1].parent = whereheadp->numentries;
		whereheadp->numentries++;
	}
}

static INT parsenots(INT type, HCORRTABLE corrtable,
	INT *symtypep, INT notflag, S_WHERE *whereheadp, LEXPWORK **lexpwork)
{
	while (*symtypep == SNOT) {
		notflag = !notflag;
		*symtypep = scansymbol(0);  /* get the next symbol */
	}
	return parsepredicates(type, corrtable, symtypep, notflag, whereheadp, lexpwork);
}

/*		Parse a predicate for logical expression and output it in postfix form. */
/* */
/*	Entry: */
/*		numtables    = number of tables used */
/*		corrtable    = pointer to pointer to array of table descriptions for the tables */
/*		symtypep     = pointer to scansymbol result */
/*		whereheadp   = pointer to S_WHERE header */
/*		lexpwork     = pointer to array of entries in expression */
/* */
/*	Exit: */
/*		logical expression parsed and described */
/*		*numentries  = number of entries (operands and operators) in expression */
/*		*lexpwork    = array of entries defining the expression */
/*		*symtypep    = type of next scanned symbol */
/*
 *	Returns:
 *		TRUE if expression valid, FALSE if invalid, error given
 */
static int parsepredicates(INT type, HCORRTABLE corrtable,
	INT *symtypep, INT notflag, S_WHERE *whereheadp, LEXPWORK **lexpwork)
{
	INT left, left2, op, savesqloffset1, savesqloffset2, savesymtype;

	savesqloffset1 = sqllastoffset;
	if (!parsevalues(type, corrtable, symtypep, notflag, whereheadp, lexpwork)) return FALSE;
	if (*symtypep != SEQUAL && *symtypep != SLESS && *symtypep != SGREATER &&
	    *symtypep != SLESSEQUAL && *symtypep != SGREATEREQUAL &&
	    *symtypep != SNOTEQUAL && *symtypep != SLIKE && *symtypep != SIS &&
		*symtypep != SBETWEEN && *symtypep != SIN && *symtypep != SNOT) {
		if (*symtypep == SRPAREN || !whereheadp->numentries || ((*lexpwork)->lexpentries[whereheadp->numentries - 1].type != OPCOLUMN
			&& (*lexpwork)->lexpentries[whereheadp->numentries - 1].type != OPVALUE && (*lexpwork)->lexpentries[whereheadp->numentries - 1].type != OPEXP)) return TRUE;
		sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid expression, comparison predicate expected in expression", NULL);
		return FALSE;
	}

	if (*symtypep == SNOT) {
		/* check for between or in */
		do {
			notflag = !notflag;
			*symtypep = scansymbol(0);  /* get the next symbol */
		} while (*symtypep == SNOT);
		if (*symtypep != SBETWEEN && *symtypep != SIN) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid expression, comparison predicate expected in expression", NULL);
			return FALSE;
		}
	}
	savesymtype = *symtypep;
	left = whereheadp->numentries - 1;  /* save index of first operand */
	*symtypep = scansymbol(0);  /* get the next symbol */
	if (savesymtype == SIS) {
		while (*symtypep == SNOT) {
			notflag = !notflag;
			*symtypep = scansymbol(0);  /* get the next symbol */
		}
		if (*symtypep != SNULL) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid expression, NULL predicate expected in expression", NULL);
			return FALSE;
		}
		*symtypep = scansymbol(0);  /* get the next symbol */
		op = OPNULL;
	}
	else if (savesymtype == SBETWEEN) {
		if (!parsevalues(type, corrtable, symtypep, notflag, whereheadp, lexpwork)) return FALSE;
		if (!whereheadp->numentries || ((*lexpwork)->lexpentries[whereheadp->numentries - 1].type != OPCOLUMN
			&& (*lexpwork)->lexpentries[whereheadp->numentries - 1].type != OPVALUE && (*lexpwork)->lexpentries[whereheadp->numentries - 1].type != OPEXP)) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid BETWEEN expression, missing column or value", NULL);
			return FALSE;
		}
		if (notflag) op = OPLT;
		else op = OPGE;
		if (!MORELEXP(lexpwork, whereheadp->numentries)) return FALSE;
		(*lexpwork)->lexpentries[whereheadp->numentries].type = op;
		(*lexpwork)->lexpentries[whereheadp->numentries].left = left;
		(*lexpwork)->lexpentries[whereheadp->numentries].right = whereheadp->numentries - 1;
		(*lexpwork)->lexpentries[whereheadp->numentries].parent = -1;
		(*lexpwork)->lexpentries[left].parent = whereheadp->numentries;
		(*lexpwork)->lexpentries[whereheadp->numentries - 1].parent = whereheadp->numentries;
		left2 = whereheadp->numentries++;
		if (*symtypep != SAND) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid BETWEEN expression, missing AND condition", NULL);
			return FALSE;
		}
		savesqloffset2 = sqloffset;  /* skips AND */
		/* retrieve left side again */
		sqloffset = savesqloffset1;
		*symtypep = scansymbol(0);
		if (!parsevalues(type, corrtable, symtypep, notflag, whereheadp, lexpwork)) return FALSE;
		left = whereheadp->numentries - 1;  /* save index of first operand */
		/* continue getting right side */
		sqloffset = savesqloffset2;
		*symtypep = scansymbol(0);  /* get the next symbol */
		if (!parsevalues(type, corrtable, symtypep, notflag, whereheadp, lexpwork)) return FALSE;
		if (!whereheadp->numentries || ((*lexpwork)->lexpentries[whereheadp->numentries - 1].type != OPCOLUMN
			&& (*lexpwork)->lexpentries[whereheadp->numentries - 1].type != OPVALUE && (*lexpwork)->lexpentries[whereheadp->numentries - 1].type != OPEXP)) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid BETWEEN expression, missing column or value", NULL);
			return FALSE;
		}
		if (notflag) op = OPGT;
		else op = OPLE;
		if (!MORELEXP(lexpwork, whereheadp->numentries)) return FALSE;
		(*lexpwork)->lexpentries[whereheadp->numentries].type = op;
		(*lexpwork)->lexpentries[whereheadp->numentries].left = left;
		(*lexpwork)->lexpentries[whereheadp->numentries].right = whereheadp->numentries - 1;
		(*lexpwork)->lexpentries[whereheadp->numentries].parent = -1;
		(*lexpwork)->lexpentries[left].parent = whereheadp->numentries;
		(*lexpwork)->lexpentries[whereheadp->numentries - 1].parent = whereheadp->numentries;
		whereheadp->numentries++;
		left = left2;
		if (notflag) {
			notflag = FALSE;
			op = OPOR;
		}
		else op = OPAND;
	}
	else if (savesymtype == SIN) {
		if (*symtypep != SLPAREN) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid IN expression, missing left parenthesis", NULL);
			return FALSE;
		}
		*symtypep = scansymbol(0);  /* get the next symbol */
		for (left2 = 0; ; ) {
			if (!parsevalues(type, corrtable, symtypep, notflag, whereheadp, lexpwork)) return FALSE;
			if (!whereheadp->numentries || ((*lexpwork)->lexpentries[whereheadp->numentries - 1].type != OPCOLUMN
				&& (*lexpwork)->lexpentries[whereheadp->numentries - 1].type != OPVALUE && (*lexpwork)->lexpentries[whereheadp->numentries - 1].type != OPEXP)) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid IN expression, missing column or value", NULL);
				return FALSE;
			}
			if (notflag) op = OPNE;
			else op = OPEQ;
			if (!left2 && *symtypep != SCOMMA) break;
			if (!MORELEXP(lexpwork, whereheadp->numentries)) return FALSE;
			(*lexpwork)->lexpentries[whereheadp->numentries].type = op;
			(*lexpwork)->lexpentries[whereheadp->numentries].left = left;
			(*lexpwork)->lexpentries[whereheadp->numentries].right = whereheadp->numentries - 1;
			(*lexpwork)->lexpentries[whereheadp->numentries].parent = -1;
			(*lexpwork)->lexpentries[left].parent = whereheadp->numentries;
			(*lexpwork)->lexpentries[whereheadp->numentries - 1].parent = whereheadp->numentries;
			whereheadp->numentries++;
			if (left2) {
				left = left2;
				if (notflag) op = OPAND;
				else op = OPOR;
				if (*symtypep != SCOMMA) break;
				if (!MORELEXP(lexpwork, whereheadp->numentries)) return FALSE;
				(*lexpwork)->lexpentries[whereheadp->numentries].type = op;
				(*lexpwork)->lexpentries[whereheadp->numentries].left = left;
				(*lexpwork)->lexpentries[whereheadp->numentries].right = whereheadp->numentries - 1;
				(*lexpwork)->lexpentries[whereheadp->numentries].parent = -1;
				(*lexpwork)->lexpentries[left2].parent = whereheadp->numentries;
				(*lexpwork)->lexpentries[whereheadp->numentries - 1].parent = whereheadp->numentries;
				left2 = whereheadp->numentries++;
			}
			else left2 = whereheadp->numentries - 1;
			savesqloffset2 = sqloffset;  /* skips comma */
			/* retrieve left side again */
			sqloffset = savesqloffset1;
			*symtypep = scansymbol(0);
			if (!parsevalues(type, corrtable, symtypep, notflag, whereheadp, lexpwork)) return FALSE;
			left = whereheadp->numentries - 1;  /* save index of first operand */
			/* continue getting right side */
			sqloffset = savesqloffset2;
			*symtypep = scansymbol(0);  /* get the next symbol */
		}
		if (*symtypep != SRPAREN) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid IN expression, missing right parenthesis", NULL);
			return FALSE;
		}
		*symtypep = scansymbol(0);  /* get the next symbol */
		notflag = FALSE;
	}
	else {
		if (!parsevalues(type, corrtable, symtypep, notflag, whereheadp, lexpwork)) return FALSE;
		switch (savesymtype) {
		case SEQUAL:
			if (notflag) op = OPNE;
			else op = OPEQ;
			break;
		case SLESS:
			if (notflag) op = OPGE;
			else op = OPLT;
			break;
		case SGREATER:
			if (notflag) op = OPLE;
			else op = OPGT;
			break;
		case SLESSEQUAL:
			if (notflag) op = OPGT;
			else op = OPLE;
			break;
		case SGREATEREQUAL:
			if (notflag) op = OPLT;
			else op = OPGE;
			break;
		case SNOTEQUAL:
			if (notflag) op = OPEQ;
			else op = OPNE;
			break;
		case SLIKE:
			op = OPLIKE;
			break;
		}
		if (op != OPLIKE) notflag = FALSE;
	}
	if (!MORELEXP(lexpwork, whereheadp->numentries)) return FALSE;
	(*lexpwork)->lexpentries[whereheadp->numentries].type = op;
	(*lexpwork)->lexpentries[whereheadp->numentries].left = left;
	(*lexpwork)->lexpentries[whereheadp->numentries].right = whereheadp->numentries - 1;
	(*lexpwork)->lexpentries[whereheadp->numentries].parent = -1;
	(*lexpwork)->lexpentries[left].parent = whereheadp->numentries;
	(*lexpwork)->lexpentries[whereheadp->numentries - 1].parent = whereheadp->numentries;
	whereheadp->numentries++;
	if (notflag) {
		if (!MORELEXP(lexpwork, whereheadp->numentries)) return FALSE;
		(*lexpwork)->lexpentries[whereheadp->numentries].type = OPNOT;
		(*lexpwork)->lexpentries[whereheadp->numentries].left = whereheadp->numentries - 1;
		(*lexpwork)->lexpentries[whereheadp->numentries].parent = -1;
		(*lexpwork)->lexpentries[whereheadp->numentries - 1].parent = whereheadp->numentries;
		whereheadp->numentries++;
	}
	return TRUE;
}

/*		Parse a literal or column for a logical expression and output it in postfix form. */
/* */
/*	Entry: */
/*		numtables    = number of tables used */
/*		corrtable    = pointer to pointer to array of table descriptions for the tables */
/*		symtypep     = pointer to scansymbol result */
/*		whereheadp   = pointer to S_WHERE header */
/*		lexpwork     = pointer to array of entries in expression */
/* */
/*	Exit: */
/*		logical expression parsed and described */
/*		*numentries  = number of entries (operands and operators) in expression */
/*		*lexpwork    = array of entries defining the expression */
/*		*symtypep    = type of next scanned symbol */
/* */
/*	Returns: */
/*		TRUE if expression valid, FALSE if invalid, error given */
static INT parsevalues(INT type, HCORRTABLE corrtable,
	INT *symtypep, INT notflag, S_WHERE *whereheadp, LEXPWORK **lexpwork)
{
	UINT tabcolnum;
	CHAR name[MAX_NAME_LENGTH + 1], *ptr;
	COLDESC coldesc;
	S_AEXP **aexpp;
	S_VALUE **valuep;

	switch (*symtypep) {
	case SLPAREN:
		*symtypep = scansymbol(0);  /* get the next symbol */
		if (!parseors(type, corrtable, symtypep, notflag, whereheadp, lexpwork)) return FALSE;
		if (*symtypep != SRPAREN) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid expression, expected right parenthesis", NULL);
			return FALSE;
		}
		*symtypep = scansymbol(0);  /* get the next symbol */
		break;
	case SLITERAL:
	case SNUMBER:
		if (!MORELEXP(lexpwork, whereheadp->numentries)) return FALSE;
		ptr = (char *) sqlstatement + sqloffset;
		while (*ptr == ' ' || *ptr == '\r' || *ptr == '\n') ptr++;
		if (*ptr == '+' || *ptr == '-' || *ptr == '/' || *ptr == '*') {
			aexpp = parseaexp(type, corrtable, symtypep);
			if (aexpp == NULL) return FALSE;
			(*lexpwork)->lexpentries[whereheadp->numentries].type = OPEXP;
			(*lexpwork)->lexpentries[whereheadp->numentries].expp = aexpp;
		}
		else {
			valuep = (S_VALUE **) memalloc(sizeof(S_VALUE) + sqllength - 1, 0);
			if (valuep == NULL) {	/* memory allocation failure */
				sqlerrnum(SQLERR_PARSE_NOMEM);
				return FALSE;
			}
			(*lexpwork)->lexpentries[whereheadp->numentries].type = OPVALUE;
			(*lexpwork)->lexpentries[whereheadp->numentries].valuep = valuep;
			(*valuep)->stype = STYPE_VALUE;
			(*valuep)->length = sqllength;
			(*valuep)->litoffset = 0;
			memcpy((*valuep)->data, sqlliteral, sqllength);
			(*valuep)->escape[0] = '\0';
			*symtypep = scansymbol(0);  /* get the next symbol */
			if (*symtypep == SESCAPE) {
				*symtypep = scansymbol(0);  /* get the next symbol */
				if (*symtypep != SLITERAL) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid escape clause for LIKE, expected literal value", NULL);
					return FALSE;
				}
				if (sqllength != 1) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid escape clause for LIKE, expected a single character", NULL);
					return FALSE;
				}
				(*valuep)->escape[0] = sqlliteral[0];
				*symtypep = scansymbol(0);
			}
		}
		(*lexpwork)->lexpentries[whereheadp->numentries].left = -1;
		(*lexpwork)->lexpentries[whereheadp->numentries].right = -1;
		(*lexpwork)->lexpentries[whereheadp->numentries].parent = -1;
		whereheadp->numentries++;
		break;
	case SLOWER:
	case SUPPER:
	case SCAST:
	case SSUBSTRING:
	case STRIM:
		if (!MORELEXP(lexpwork, whereheadp->numentries)) return FALSE;
		aexpp = parseaexp(type, corrtable, symtypep);
		if (aexpp == NULL) return FALSE;
		(*lexpwork)->lexpentries[whereheadp->numentries].type = OPEXP;
		(*lexpwork)->lexpentries[whereheadp->numentries].left = -1;
		(*lexpwork)->lexpentries[whereheadp->numentries].right = -1;
		(*lexpwork)->lexpentries[whereheadp->numentries].parent = -1;
		(*lexpwork)->lexpentries[whereheadp->numentries].expp = aexpp;
		whereheadp->numentries++;
		break;
	case SCOUNT:
	case SSUM:
	case SAVG:
	case SMIN:
	case SMAX:
		if (type & PARSE_HAVING) {
			if (!MORELEXP(lexpwork, whereheadp->numentries)) return FALSE;
			aexpp = parseaexp(type, corrtable, symtypep);
			if (aexpp == NULL) return FALSE;
			(*lexpwork)->lexpentries[whereheadp->numentries].type = OPEXP;
			(*lexpwork)->lexpentries[whereheadp->numentries].left = -1;
			(*lexpwork)->lexpentries[whereheadp->numentries].right = -1;
			(*lexpwork)->lexpentries[whereheadp->numentries].parent = -1;
			(*lexpwork)->lexpentries[whereheadp->numentries].expp = aexpp;
			whereheadp->numentries++;
			break;
		}
		break;
	default:
		if (sqlcattype != CID) {			/* not an identifier */
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid expression, identifier expected", NULL);
			return FALSE;
		}
		if (!validatesqlname(sqlsymbol, sqllength, FALSE)) {  /* validate the name */
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid column name", sqlsymbol);
			return FALSE;
		}
		if (parsecolumnname(&coldesc) != SQL_SUCCESS) return FALSE;
		tabcolnum = findcolumn(&coldesc, *corrtable, TRUE);
		if (!tabcolnum) {
			if (coldesc.qualname[0]) {
				strcpy(name, coldesc.qualname);
				strcat(name, ".");
			}
			else name[0] = 0;
			strcat(name, coldesc.columnname);
			sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid column name", (char *) name);
			return FALSE;
		}
		if (!MORELEXP(lexpwork, whereheadp->numentries)) return FALSE;
		if (gettabref(tabcolnum) != TABREF_LITEXP) {
			ptr = (char *) sqlstatement + sqloffset;
			while (*ptr == ' ' || *ptr == '\r' || *ptr == '\n') ptr++;
		}
		else ptr = "+";
		if (*ptr == '+' || *ptr == '-' || *ptr == '/' || *ptr == '*') {
			aexpp = parseaexp(type, corrtable, symtypep);
			if (aexpp == NULL) return FALSE;
			(*lexpwork)->lexpentries[whereheadp->numentries].type = OPEXP;
			(*lexpwork)->lexpentries[whereheadp->numentries].expp = aexpp;
		}
		else {
			(*lexpwork)->lexpentries[whereheadp->numentries].type = OPCOLUMN;
			(*lexpwork)->lexpentries[whereheadp->numentries].tabcolnum = tabcolnum;
			*symtypep = scansymbol(0);  /* get the next symbol */
		}
		(*lexpwork)->lexpentries[whereheadp->numentries].left = -1;
		(*lexpwork)->lexpentries[whereheadp->numentries].right = -1;
		(*lexpwork)->lexpentries[whereheadp->numentries].parent = -1;
		whereheadp->numentries++;
		break;
	}
	return TRUE;
}

/*	Returns: */
/*		TRUE if success, FALSE if unable to allocate memory. If FALSE, */
/*		statementerror is called. */
static INT morelexp(LEXPWORK **lexpwork, INT num)
{
	INT max;

	if ((max = (*lexpwork)->max) > num) return TRUE;
	while (num >= max) max += LEXPSIZE;
	if (memchange((unsigned char **) lexpwork, sizeof(LEXPWORK) + (max - 1) * sizeof(LEXPENTRY), MEMFLAGS_ZEROFILL) == -1) {
		sqlerrnum(SQLERR_PARSE_NOMEM);
		return FALSE;
	}
	(*lexpwork)->max = max;
	return TRUE;
}

/*	parseaexp */
/*		Parse an arithmetic expression for the column value in an UPDATE.  */
/*		Output it in postfix form. */
/*	Entry: */
/*		corrtable  = table */
/*		symtypep   = pointer to scansymbol result */
/*	Exit: */
/*		arithmetic expressions parsed and described */
/*		*symtypep  = type of next scanned symbol */
/*	Returns: */
/*		pointer to pointer to S_AEXP structure if okay, NULL if syntax error */
static S_AEXP **parseaexp(INT type, HCORRTABLE corrtable, INT *symtypep)
{
	INT count, numentries;
	S_AEXP **expp;
	AEXPWORK **aexpwork;

	numentries = 0;
	expp = NULL;
/*** CODE: DO NOT USE AEXPWORK AS CAN DIRECTLY GOTO S_AEXP. ***/
/***       WILL NEED MEMCHANGE BELOW.  SAME FOR WHERE PARSING. ***/
	aexpwork = (AEXPWORK **) memalloc(sizeof(AEXPWORK) + (AEXPSIZE - 1) * sizeof(AEXPENTRY), MEMFLAGS_ZEROFILL);
	if (aexpwork == NULL) {
		sqlerrnum(SQLERR_PARSE_NOMEM);
		return(NULL);
	}
	(*aexpwork)->max = AEXPSIZE;
	if (!aexp(type, corrtable, symtypep, &numentries, aexpwork)) goto procerror;	/* error in expression */
	expp = (S_AEXP **) memalloc(sizeof(S_AEXP) + (numentries - 1) * sizeof(AEXPENTRY), 0);
	if (expp == NULL) {
		sqlerrnum(SQLERR_PARSE_NOMEM);
		goto procerror;
	}
	(*expp)->stype = STYPE_AEXP;  /* set structure type */
	(*expp)->numentries = numentries;
	memcpy((*expp)->aexpentries, (*aexpwork)->aexpentries, (INT)(numentries * sizeof(AEXPENTRY)));
	memfree((UCHAR **) aexpwork);
	return expp;

procerror:
	for (count = 0; count < numentries; count++) {
		if ((*aexpwork)->aexpentries[count].type == OPVALUE)
			memfree((UCHAR **)(*aexpwork)->aexpentries[count].valuep);
	}
	memfree((UCHAR **) aexpwork);
	return NULL;
}

/*	aexp */
/*		Parse an arithmetic expression or concatenation expression and output it in postfix form. */
/*	Entry: */
/*		corrtable	= table */
/*		symtypep	= pointer to scansymbol result */
/*		numentries	= pointer to number of entries in expression */
/*		aexpwork	= pointer to array of entries in expression */
/*	Exit: */
/*		arithmetic or concatenation expression parsed and described */
/*		*numentries	= number of entries (operands and operators) in expression */
/*		*aexpwork	= array of entries defining the expression */
/*		*symtypep	= type of next scanned symbol */
/*	Returns: */
/*		TRUE if expression valid, FALSE if invalid, error given */
static INT aexp(INT type, HCORRTABLE corrtable, INT *symtypep, INT *numentriesp, AEXPWORK **aexpwork) // @suppress("No return")
{
	INT savesymtype;

/* this processes exp ::= term, exp ::= exp + term, exp ::= exp - term, exp ::= exp || term */
	if (!aterm(type, corrtable, symtypep, numentriesp, aexpwork)) return FALSE;
	for ( ; ; ) {
		if (*symtypep != SPLUS && *symtypep != SMINUS && *symtypep != SCONCAT) return TRUE;  /* if not + or -, end */
		savesymtype = *symtypep;	/* save the + or - */
		*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* get the next symbol */
		if (!aterm(type, corrtable, symtypep, numentriesp, aexpwork)) return FALSE;
		if (!MOREAEXP(aexpwork, *numentriesp)) return FALSE;
		if (savesymtype == SPLUS) (*aexpwork)->aexpentries[*numentriesp].type = OPADD;
		else if (savesymtype == SMINUS) (*aexpwork)->aexpentries[*numentriesp].type = OPSUB;
		else if (savesymtype == SCONCAT) (*aexpwork)->aexpentries[*numentriesp].type = OPCONCAT;
		(*numentriesp)++;
	}
}

/*	aterm */
/*		Parse a term for an arithmetic expression and output it in postfix form. */
/*	Entry: */
/*		corrtable    = table */
/*		symtypep     = pointer to scansymbol result */
/*		numentriesp  = pointer to number of entries in expression */
/*		aexpwork     = pointer to array of entries in expression */
/*	Exit: */
/*		arithmetic expression parsed and described */
/*		*numentriesp = number of entries (operands and operators) in expression */
/*		*aexpwork    = array of entries defining the expression */
/*		*symtypep    = type of next scanned symbol */
/*	Returns: */
/*		TRUE if expression valid, FALSE if invalid, error given */
static INT aterm(INT type, HCORRTABLE corrtable, INT *symtypep, INT *numentriesp, AEXPWORK **aexpwork) // @suppress("No return")
{
	INT savesymtype;

/* this processes term ::= factor, term ::= term * factor, term ::= term / factor */
	if (!asign(type, corrtable, symtypep, numentriesp, aexpwork)) return FALSE;
	for ( ; ; ) {
		if (*symtypep != SASTERISK && *symtypep != SDIVIDE) return TRUE; /* if not * or /, end */
		savesymtype = *symtypep;	/* save the * or / */
		*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* get the next symbol */
		if (!afactor(type, corrtable, symtypep, numentriesp, aexpwork)) return FALSE;
		if (!MOREAEXP(aexpwork, *numentriesp)) return FALSE;
		(*aexpwork)->aexpentries[*numentriesp].type = (savesymtype == SASTERISK) ? OPMULT : OPDIV;
		(*numentriesp)++;
	}
}

/*	asign */
/*		Parse a sign for an arithmetic expression and output it in postfix form. */
/*	Entry: */
/*		corrtable    = table */
/*		symtypep     = pointer to scansymbol result */
/*		numentriesp  = pointer to number of entries in expression */
/*		aexpwork     = pointer to array of entries in expression */
/*	Exit: */
/*		arithmetic expression parsed and described */
/*		*numentriesp = number of entries (operands and operators) in expression */
/*		*aexpwork    = array of entries defining the expression */
/*		*symtypep    = type of next scanned symbol */
/*	Returns: */
/*		TRUE if expression valid, FALSE if invalid, error given */
static INT asign(INT type, HCORRTABLE corrtable, INT *symtypep, INT *numentriesp, AEXPWORK **aexpwork)
{
	INT savesymtype;

/* this processes term ::= factor, term ::= term * factor, term ::= term / factor */
	if (*symtypep != SMINUS && *symtypep != SPLUS) return afactor(type, corrtable, symtypep, numentriesp, aexpwork);
	savesymtype = *symtypep;	/* save the - or + */
	*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);	/* get the next symbol */
	if (!asign(type, corrtable, symtypep, numentriesp, aexpwork)) return FALSE;
	if (savesymtype == SMINUS) {
		if (!MOREAEXP(aexpwork, *numentriesp)) return FALSE;
		(*aexpwork)->aexpentries[*numentriesp].type = OPNEGATE;
		(*numentriesp)++;
	}
	return TRUE;
}

/*	afactor */
/*		Parse a factor for an arithmetic expression and output it in postfix form. */
/*	Entry: */
/*		corrtable    = table */
/*		symtypep     = pointer to scansymbol result */
/*		numentriesp  = pointer to number of entries in expression */
/*		aexpwork     = pointer to array of entries in expression */
/*	Exit: */
/*		arithmetic expression parsed and described */
/*		*numentriesp = number of entries (operands and operators) in expression */
/*		*aexpwork    = array of entries defining the expression */
/*		*symtypep    = type of next scanned symbol */
/*	Returns: */
/*		TRUE if expression valid, FALSE if invalid, error given */
static INT afactor(INT type, HCORRTABLE corrtable, INT *symtypep, INT *numentriesp, AEXPWORK **aexpwork)
{
	INT i1, i2, op, savelastoffset, savelength, saveoffset, datatype, length, scale;
	UINT tabcolnum;
	CHAR name[MAX_NAME_LENGTH + 1], work[16];
	UCHAR *savestatement;
	COLDESC columndesc;
	TABLE *tab1;
	COLUMN *col1;
	S_VALUE **valuep;

/* this processes factor ::= ( exp ), factor ::= column ref, factor ::= number */
	switch (*symtypep) {
	case SLPAREN:
		*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get the next symbol */
		if (!aexp(type, corrtable, symtypep, numentriesp, aexpwork)) return FALSE;
		if (*symtypep != SRPAREN) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "right parenthesis expected", NULL);
			return FALSE;
		}
		*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get the next symbol */
		break;
	case SLITERAL:
	case SNUMBER:
		if (!MOREAEXP(aexpwork, *numentriesp)) return FALSE;
		valuep = (S_VALUE **) memalloc(sizeof(S_VALUE) + sqllength - 1, 0);
		if (valuep == NULL) {
			sqlerrnum(SQLERR_PARSE_NOMEM);
			return FALSE;
		}
		(*aexpwork)->aexpentries[*numentriesp].type = OPVALUE;
		(*aexpwork)->aexpentries[*numentriesp].valuep = valuep;
		(*valuep)->stype = STYPE_VALUE;
		(*valuep)->length = sqllength;
		(*valuep)->litoffset = 0;
		memcpy((*valuep)->data, sqlliteral, sqllength);
		(*numentriesp)++;
		*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get the next symbol */
		break;
	default:
		i1 = *symtypep;
		if ((type & PARSE_SELECT) && !(type & (PARSE_ON | PARSE_WHERE)) &&
			(i1 == SCOUNT || i1 == SSUM || i1 == SAVG || i1 == SMIN || i1 == SMAX)) {
			*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (*symtypep != SLPAREN) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "left parenthesis expected after set function", NULL);
				return FALSE;
			}
			*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (i1 == SCOUNT && *symtypep == SASTERISK) op = OPCOUNTALL;
			else if (*symtypep == SDISTINCT) {
				if (i1 == SCOUNT) op = OPCOUNTDISTINCT;
				else if (i1 == SAVG) op = OPAVGDISTINCT;
				else if (i1 == SMIN) op = OPMINDISTINCT;
				else if (i1 == SMAX) op = OPMAXDISTINCT;
				else op = OPSUMDISTINCT;
				*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			}
			else {
				if (i1 == SCOUNT) op = OPCOUNT;
				else if (i1 == SAVG) op = OPAVG;
				else if (i1 == SMIN) op = OPMIN;
				else if (i1 == SMAX) op = OPMAX;
				else op = OPSUM;
				if (*symtypep == SALL) *symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			}
		}
		else if (i1 == SLOWER || i1 == SUPPER) {
			*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (*symtypep != SLPAREN) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "left parenthesis expected after upper/lower function", NULL);
				return FALSE;
			}
			*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (!aexp(type, corrtable, symtypep, numentriesp, aexpwork)) return FALSE;
			if (!MOREAEXP(aexpwork, *numentriesp)) return FALSE;
			(*aexpwork)->aexpentries[*numentriesp].type = (i1 == SLOWER) ? OPLOWER : OPUPPER;
			(*numentriesp)++;
			*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS); /* get the next symbol after RPAREN */
			break;
		}
		else if (i1 == STRIM) {
			/**
			 * TRIM([LEADING | TRAILING | BOTH] [charexp] FROM charexp)
			 * Completed with one operation.  The expression stack is built as follows:
			 *
			 * OPVALUE (character to trim)
			 * OPVALUE (source string)
			 * OPTRIMx
			 */
			*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (*symtypep != SLPAREN) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "left parenthesis expected after TRIM function", NULL);
				return FALSE;
			}
			*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			op = OPTRIMB;
			if (*symtypep == SBOTH) {
				*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			}
			else if (*symtypep == SLEADING) {
				op = OPTRIML;
				*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			}
			else if (*symtypep == STRAILING) {
				op = OPTRIMT;
				*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			}
			if (*symtypep == SFROM) {
				/* put default trim character, ' ', on expression stack */
				valuep = (S_VALUE **) memalloc(sizeof(S_VALUE) + 1, 0);
				if (valuep == NULL) {	/* memory allocation failure */
					sqlerrnum(SQLERR_PARSE_NOMEM);
					return FALSE;
				}
				(*aexpwork)->aexpentries[*numentriesp].type = OPVALUE;
				(*aexpwork)->aexpentries[*numentriesp].valuep = valuep;
				(*numentriesp)++;
				(*valuep)->stype = STYPE_VALUE;
				(*valuep)->length = 2;
				(*valuep)->litoffset = 0;
				memcpy((*valuep)->data, " \0", 2);
			}
			else {
				if (!aexp(type, corrtable, symtypep, numentriesp, aexpwork)) return FALSE;
				if (!MOREAEXP(aexpwork, *numentriesp)) return FALSE;
				if (*symtypep != SFROM) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "keyword FROM expected in TRIM function", NULL);
					return FALSE;
				}
			}
			/* parse string that needs trimed */
			*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (!aexp(type, corrtable, symtypep, numentriesp, aexpwork)) return FALSE;
			if (!MOREAEXP(aexpwork, *numentriesp)) return FALSE;
			(*aexpwork)->aexpentries[*numentriesp].type = op;
			(*numentriesp)++;
			*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS); /* get the next symbol after RPAREN */
			break;
		}
		else if (i1 == SSUBSTRING) {
			/**
			 * SUBSTRING(charexp FROM start [FOR length])
			 * Completed with two operations.  The expression stack is built as follows:
			 *
			 * OPVALUE (source string)
			 * OPVALUE (position)
			 * OPSUBSTRPOS
			 * OPVALUE (length)
			 * OPSUBSTRLEN
			 */
			*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (*symtypep != SLPAREN) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "left parenthesis expected after SUBSTRING operation", NULL);
				return FALSE;
			}
			*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			/* source expression */
			if (!aexp(type, corrtable, symtypep, numentriesp, aexpwork)) return FALSE;
			if (!MOREAEXP(aexpwork, *numentriesp)) return FALSE;
			if (*symtypep != SFROM) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "keyword FROM expected in SUBSTRING operation", NULL);
				return FALSE;
			}
			if (scansymbol(0) != SNUMBER) { /* offset - allow negative numbers */
				sqlerrnummsg(SQLERR_PARSE_ERROR, "SUBSTRING was not correctly formatted", NULL);
				return FALSE;
			}
			/* put position on expression stack */
			valuep = (S_VALUE **) memalloc(sizeof(S_VALUE) + sqllength - 1, 0);
			if (valuep == NULL) {	/* memory allocation failure */
				sqlerrnum(SQLERR_PARSE_NOMEM);
				return FALSE;
			}
			(*aexpwork)->aexpentries[*numentriesp].type = OPVALUE;
			(*aexpwork)->aexpentries[*numentriesp].valuep = valuep;
			(*numentriesp)++;
			(*valuep)->stype = STYPE_VALUE;
			(*valuep)->length = sqllength;
			(*valuep)->litoffset = 0;
			memcpy((*valuep)->data, sqlliteral, sqllength);
			/* put substring position operation on expression stack */
			(*aexpwork)->aexpentries[*numentriesp].type = OPSUBSTRPOS;
			(*numentriesp)++;
			*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (*symtypep == SRPAREN) {
				sqlliteral[0] = '0';
				sqllength = 1;
			}
			else {
				if (*symtypep != SFOR) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "SUBSTRING was not correctly formatted", NULL);
					return FALSE;
				}
				if (scansymbol(PARSE_PLUS | PARSE_MINUS) != SNUMBER) { /* length */
					sqlerrnummsg(SQLERR_PARSE_ERROR, "SUBSTRING was not correctly formatted", NULL);
					return FALSE;
				}
				if (sqllength == 1 && sqlliteral[0] == '0') { /* don't allow '0' for length spec */
					sqlerrnummsg(SQLERR_PARSE_ERROR, "length for SUBSTRING operation was invalid", NULL);
					return FALSE;
				}
				*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
				if (*symtypep != SRPAREN) {
					sqlerrnummsg(SQLERR_PARSE_ERROR, "SUBSTRING was not correctly formatted", NULL);
					return FALSE;
				}
			}
			/* put substring length on expression stack */
			valuep = (S_VALUE **) memalloc(sizeof(S_VALUE) + sqllength - 1, 0);
			if (valuep == NULL) {	/* memory allocation failure */
				sqlerrnum(SQLERR_PARSE_NOMEM);
				return FALSE;
			}
			(*aexpwork)->aexpentries[*numentriesp].type = OPVALUE;
			(*aexpwork)->aexpentries[*numentriesp].valuep = valuep;
			(*numentriesp)++;
			(*valuep)->stype = STYPE_VALUE;
			(*valuep)->length = sqllength;
			(*valuep)->litoffset = 0;
			memcpy((*valuep)->data, sqlliteral, sqllength);
			/* put substring length operation on expression stack */
			(*aexpwork)->aexpentries[*numentriesp].type = OPSUBSTRLEN;
			(*numentriesp)++;
			*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS); /* get the next symbol after RPAREN */
			break;
		}
		else if (i1 == SCAST) {
			*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (*symtypep != SLPAREN) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "left parenthesis expected after CAST operation", NULL);
				return FALSE;
			}
			*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (!aexp(type, corrtable, symtypep, numentriesp, aexpwork)) return FALSE;
			if (!MOREAEXP(aexpwork, *numentriesp)) return FALSE;
			if (*symtypep != SAS) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "keyword AS expected in CAST operation", NULL);
				return FALSE;
			}
			if (scansymbol(PARSE_PLUS | PARSE_MINUS) != SID) { /* datatype */
				sqlerrnummsg(SQLERR_PARSE_ERROR, "CAST was not correctly formatted", NULL);
				return FALSE;
			}
			if (strlen(sqlsymbol) >= sizeof(work)) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "an invalid datatype was used for CAST", NULL);
				return FALSE;
			}
			strucpy(work, sqlsymbol);
			if (!strcmp(work, "CHAR") || !strcmp(work, "CHARACTER")) datatype = TYPE_CHAR;
			else if (!strcmp(work, "NUM") || !strcmp(work, "NUMERIC")) datatype = TYPE_NUM;
			else if (!strcmp(work, "DEC") || !strcmp(work, "DECIMAL")) datatype = TYPE_NUM;
			else if (!strcmp(work, "DATE")) datatype = TYPE_DATE;
			else if (!strcmp(work, "TIME")) datatype = TYPE_TIME;
			else if (!strcmp(work, "TIMESTAMP")) datatype = TYPE_TIMESTAMP;
			else {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "an invalid datatype was used for CAST", NULL);
				return FALSE;
			}
			*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
			if (*symtypep == SRPAREN) {
				if (datatype == TYPE_DATE) length = 10; /* 'YYYY-MM-DD' */
				else if (datatype == TYPE_TIME) length = 12; /* 'HH:MM:SS.FFF' */
				else if (datatype == TYPE_TIMESTAMP) length = 23; /* 'YYYY-MM-DD HH:MM:SS.FFF' */
				else length = -1; /* length not specified */
				scale = -1;  /* scale not specified */
			}
			else if (*symtypep == SLPAREN) {
				for (length = scale = i2 = 0;;) {
					*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);
					if (*symtypep == SRPAREN) break;
					else if (*symtypep == SNUMBER) {
						if (datatype != TYPE_CHAR && datatype != TYPE_NUM) {
							sqlerrnummsg(SQLERR_PARSE_ERROR, "CAST was not correctly formatted", NULL);
							return FALSE;
						}
						if (i2 == TRUE) {
							sqlliteral[sqllength] = 0;
							scale = atoi((CHAR *)sqlliteral); /* scale */
							length++; /* must add one for decimal point */
						}
						else {
							sqlliteral[sqllength] = 0;
							length = atoi((CHAR *)sqlliteral);
						}
					}
					else if (*symtypep == SLITERAL) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "CAST was not correctly formatted", NULL);
						return FALSE;
					}
					else if (*symtypep == SCOMMA) {
						if (datatype != TYPE_NUM) {
							sqlerrnummsg(SQLERR_PARSE_ERROR, "CAST was not correctly formatted", NULL);
							return FALSE;
						}
						i2 = TRUE;
					}
				}
				if (!length || scansymbol(PARSE_PLUS | PARSE_MINUS) != SRPAREN) { /* ')' */
					sqlerrnummsg(SQLERR_PARSE_ERROR, "CAST was not correctly formatted", NULL);
					return FALSE;
				}
			}
			else {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "CAST was not correctly formatted", NULL);
				return FALSE;
			}
			(*aexpwork)->aexpentries[*numentriesp].type = OPCAST;
			(*aexpwork)->aexpentries[*numentriesp].info.cast.type = datatype;
			(*aexpwork)->aexpentries[*numentriesp].info.cast.length = length;
			(*aexpwork)->aexpentries[*numentriesp].info.cast.scale = scale;
			(*numentriesp)++;
			*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS); /* get the next symbol after RPAREN */
			break;
		}
		else op = OPCOLUMN;
		if (op != OPCOUNTALL) {
			if (sqlcattype != CID) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid expression, identifier expected", NULL);
				return FALSE;
			}
			if (!validatesqlname(sqlsymbol, sqllength, FALSE)) {
				sqlerrnummsg(SQLERR_PARSE_ERROR, "invalid column name", sqlsymbol);
				return FALSE;
			}
			if (type & PARSE_SELECT) {
				if (parsecolumnname(&columndesc) != SQL_SUCCESS) return FALSE;
				tabcolnum = findcolumn(&columndesc, *corrtable, op == OPCOLUMN);
				if (op == OPCOLUMN && gettabref(tabcolnum) == TABREF_LITEXP) {
					/* process expression which is an alias */
					i2 = (*corrtable)->count;
					for (i1 = 0; i1 < i2; i1++) {
						if ((*corrtable)->info[i1].tablenum) continue;
						if (tabcolnum == (*corrtable)->info[i1].tabcolnum) break;
					}
					if (i1 == i2) {
						sqlerrnummsg(SQLERR_INTERNAL, "unable to locate alias expression", NULL);
						return FALSE;
					}
					savestatement = sqlstatement;
					savelength = sqlstmtlength;
					saveoffset = sqloffset;
					savelastoffset = sqllastoffset;
					sqlstatement = (*corrtable)->info[i1].expstring;
					sqlstmtlength = (*corrtable)->info[i1].explength;
					sqloffset = sqllastoffset = 0;
					*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get the next symbol */
					if (!aexp(type, corrtable, symtypep, numentriesp, aexpwork)) return FALSE;
					sqlstatement = savestatement;
					sqlstmtlength = savelength;
					sqloffset = saveoffset;
					sqllastoffset = savelastoffset;
					*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get the next symbol */
					break;
				}
				if (tabcolnum && (op == OPSUM || op == OPAVG || op == OPSUMDISTINCT || op == OPAVGDISTINCT)) {
					for (i1 = i2 = 0; i1 < (*corrtable)->count; i1++) {
						if (!(*corrtable)->info[i1].tablenum) continue;
						if (++i2 == gettabref(tabcolnum)) break;
					}
					if (i1 == (*corrtable)->count) {
						sqlerrnummsg(SQLERR_INTERNAL, "unable to locate sum/avg table", NULL);
						return FALSE;
					}
					tab1 = tableptr((*corrtable)->info[i1].tablenum);
					col1 = columnptr(tab1, getcolnum(tabcolnum));
					if (col1->type != TYPE_NUM && col1->type != TYPE_POSNUM) {
						sqlerrnummsg(SQLERR_PARSE_ERROR, "SUM and AVG only supported on numeric columns", sqlsymbol);
						return FALSE;
					}
				}
			}
			else {
				strucpy(name, sqlsymbol);
				tabcolnum = findcolumnintable((*corrtable)->info[0].tablenum, name);
				if (tabcolnum) tabcolnum = gettabcolref(1, tabcolnum);
			}
			if (!tabcolnum) {
				sqlerrnummsg(SQLERR_PARSE_COLUMNNOTFOUND, sqlsymbol, NULL);
				return FALSE;
			}
		}
		else tabcolnum = 0;
		if (op != OPCOLUMN && scansymbol(PARSE_PLUS | PARSE_MINUS) != SRPAREN) {
			sqlerrnummsg(SQLERR_PARSE_ERROR, "right parenthesis expected after set function", NULL);
			return FALSE;
		}
		if (!MOREAEXP(aexpwork, *numentriesp)) return FALSE;
		(*aexpwork)->aexpentries[*numentriesp].type = op;
		(*aexpwork)->aexpentries[*numentriesp].tabcolnum = tabcolnum;
		(*numentriesp)++;
		*symtypep = scansymbol(PARSE_PLUS | PARSE_MINUS);  /* get the next symbol */
		break;
	}
	return TRUE;
}

/*	Returns: */
/*		TRUE if success, FALSE if unable to allocate memory. If FALSE, */
/*		statementerror is called. */
static INT moreaexp(AEXPWORK **aexpwork, int num)
{
	INT max;

	if ((max = (*aexpwork)->max) > num) return TRUE;
	while (num >= max) max += AEXPSIZE;
	if (memchange((UCHAR **) aexpwork, sizeof(AEXPWORK) + (max - 1) * sizeof(AEXPENTRY), MEMFLAGS_ZEROFILL) == -1) {
		sqlerrnum(SQLERR_PARSE_NOMEM);
		return FALSE;
	}
	(*aexpwork)->max = max;
	return TRUE;
}

/**
 * If isDelimitedIdentifier is true, allows additional characters defined in SDELIMSTRING2
 * Returns: TRUE if valid, FALSE if invalid
 */
static int validatesqlname(CHAR *stringp, int length, int isDelimitedIdentifier)
{
	if (length < 1 || length > MAX_NAME_LENGTH) return FALSE;  /* if invalid length, error */
	if (!isalpha(*stringp)) return FALSE;  /* must start with letter */
	stringp++;
	length--;
	while (length-- > 0) {
		if (isDelimitedIdentifier) {
			if (!isalnum(*stringp) && strchr(SDELIMSTRING2, *stringp) == NULL) return FALSE;
		}
		else {
			/* rest is letters, digits, underscore, at, octothorp */
			if (!isalnum(*stringp) && *stringp != '_' && *stringp != '@' && *stringp != '#') return FALSE;
		}
		stringp++;
	}
	return TRUE;
}

/**
 * 	validatenumeric
 *		Validate the value for a NUMERIC column.
 *	Entry:
 *		col1	    = pointer to COLUMN structure
 *		minus      = 1 if number is negative, 0 if not
 *		sqlliteral = number (without the minus sign if negative)
 *		sqllength  = length of number
 *	Exit:
 *		valuep     = numeric value adjusted to column precision and scale, if valid
 *	Returns:
 *		SQL_SUCCESS if okay
 *		SQL_ERROR if invalid or integer digits truncated, error given
 */
static int validatenumeric(COLUMN *col1, int minus, S_VALUE **valuep) {
	int i1, colintdigits, decpt, fracdigits, intdigits, zeroflag;
	unsigned char *destp;
	char work[64];

	colintdigits = (col1->scale) ? col1->length - col1->scale - 1 : col1->length;
	/* check integer digits */
	zeroflag = TRUE;
	for (i1 = 0; i1 < sqllength && isdigit(sqlliteral[i1]); i1++)
		if (sqlliteral[i1] != '0')
			zeroflag = FALSE;
	intdigits = i1;
	/* check for too many integer digits */
	if (intdigits + minus > colintdigits) {
		sprintf(work, "%.*s", sqllength, sqlliteral);
		return sqlerrnummsg(SQLERR_PARSE_BADNUMERIC,
				"too many digits to the left of the decimal place", work);
	}
	/* if not at end of value, check for decimal point */
	if (i1 < sqllength && sqlliteral[i1] == '.') {
		i1++;
		decpt = TRUE;
	} else
		decpt = FALSE;
	/* check fractional digits */
	for (fracdigits = 0; i1 < sqllength && isdigit(sqlliteral[i1]); fracdigits++, i1++)
		if (sqlliteral[i1] != '0' && fracdigits < (INT) col1->scale)
			zeroflag = FALSE;
	if (i1 < sqllength) { /* didn't process whole value, found an illegal character */
		sprintf(work, "%.*s", sqllength, sqlliteral);
		return sqlerrnummsg(SQLERR_PARSE_BADNUMERIC, work, NULL);
	}
	/* check for too many fractional digits. This isn't an error, just a warning */
	if (fracdigits > (INT) col1->scale) {
		fracdigits = col1->scale; /* truncate excess fractional digits */
		if (!fracdigits)
			decpt = FALSE;
		sqlswi();
	}
	if (zeroflag)
		minus = 0;
	/* number valid and not too large, copy the number into the value structure */
	destp = (*valuep)->data;
	/* if integer digits less than column integer digits, set leading blanks */
	if (intdigits + minus < colintdigits) {
		memset(destp, ' ', colintdigits - intdigits);
		destp += colintdigits - intdigits;
	} else if (minus)
		*destp++ = ' ';
	i1 = intdigits + fracdigits;
	if (decpt) i1++;
	memcpy(destp, sqlliteral, i1);
	/* change leading integer zeros to spaces */
	for (i1 = (decpt || fracdigits < (INT) col1->scale) ? 0 : 1; intdigits > i1 && *destp == '0'; intdigits--)
		*destp++ = ' ';
	if (minus)
		*(destp - 1) = '-';
	destp += intdigits + decpt + fracdigits;
	/* if fractional digits less than column fractional digits, fill with zeros */
	if (fracdigits < (INT) col1->scale) {
		if (!decpt)
			*destp++ = '.';
		memset(destp, '0', col1->scale - fracdigits);
	}
	return 0;
}

static void strucpy(CHAR *dest, CHAR *src)
{
	for ( ; *src; src++) *dest++ = (CHAR) toupper(*src);
	*dest = '\0';
}
