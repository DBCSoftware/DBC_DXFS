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
#define INC_TIME
#define INC_WINDOWS
#include "includes.h"
#include "release.h"
#include "base.h"
#include "fsrun.h"
#include "fssql.h"
#include "fssqlx.h"
#include "fio.h"
#include "tcp.h"

/* global variables */
//extern INT fsflags;
CONNECTION connection;					/* the connection */
OFFSET filepos;							/* indexed read file position */

/* local variables */
static INT cid;								/* this connection id */
static char sqlerrcode[5];
static char sqlerrtext[512];
static INT sqlinfoflag = FALSE;			/* tells if result should be SQL_SUCCESS_WITH_INFO */

static void freeconnection(void);
static void buildresultsetstring(INT, INT, UCHAR *, INT *);


/**
 * @param logfile is input, it is passed to cfginit who in turn passes it to the rio system.
 *
 * return positive connection id if successful
 * return -1 otherwise
 */
INT sqlconnect(CHAR *cfgfile, CHAR *dbdfile, CHAR *user, CHAR *password, CHAR *logfile)
{
	cid = cfginit(cfgfile, dbdfile, user, password, logfile, &connection);
	if (cid == -1) {
		return sqlerrmsg(cfggeterror());
	}
	connection.hopenfilearray = (HOPENFILE) memalloc(32 * sizeof(OPENFILE), MEMFLAGS_ZEROFILL);
	if (connection.hopenfilearray == NULL) {
		freeconnection();
		cfgexit();
		return sqlerrnum(SQLERR_NOMEM);
	}
	connection.numopenfiles = 32;
	return cid;
}

/* disconnect */
INT sqldisconnect(INT connectid)
{
	if (connectid != cid) return sqlerrnum(SQLERR_BADCONNECTID);
	freeconnection();
	cfgexit();
	cid = 0;
	return 0;
}

/* perform catalog function */
/* return 2 if statement succeeded with empty result set, no result format */
/* return 3 if statement succeeded with result set, w/ data = result format */
/* return negative if error */
INT sqlcatalog(INT connectid, UCHAR *buffer, INT buflen, UCHAR *result, INT *resultsize)
{
	INT i1, nextoffset, offset, rc, rsid;
	CHAR columnname[256], function[256], tablename[256];

	if (connectid != cid) return sqlerrnum(SQLERR_BADCONNECTID);

	offset = 0;
	i1 = tcpnextdata(buffer, buflen, &offset, &nextoffset);
	if (i1 <= 0) return sqlerrnummsg(SQLERR_BADATTR, "catalog function not specified", NULL);
	memcpy(function, buffer + offset, i1);
	function[i1] = '\0';

	offset = nextoffset;
	i1 = tcpnextdata(buffer, buflen, &offset, &nextoffset);
	if (i1 < 0) return sqlerrnummsg(SQLERR_BADATTR, "table not specified", NULL);
	memcpy(tablename, buffer + offset, i1);
	tablename[i1] = '\0';

	i1 = tcpnextdata(buffer, buflen, &nextoffset, NULL);
	if (!strcmp(function, "COLUMNS")) {
		if (i1 > 0) memcpy(columnname, buffer + nextoffset, i1);
		else i1 = 0;
		columnname[i1] = '\0';
		rc = getcolumninfo(tablename, columnname, &rsid);
	}
	else if (!strcmp(function, "PRIMARY")) rc = getprimaryinfo(tablename, &rsid);
	else if (!strcmp(function, "BESTROWID")) rc = getbestrowid(tablename, &rsid);
	else if (!strcmp(function, "STATS")) {
		if (i1 == 1 && buffer[nextoffset] == '0') i1 = FALSE;
		else i1 = TRUE;
		rc = getindexinfo(tablename, i1, &rsid);
	}
	else if (!strcmp(function, "TABLES")) rc = gettableinfo(tablename, &rsid);
	else return sqlerrnummsg(SQLERR_BADATTR, "invalid catalog function", NULL);

	/* note: only 4, 6 and errors being returned */
	if (rc < 0) return rc;
	if (rc >= 5) {
		buildresultsetstring(rc, rsid, result, resultsize);
		if (rc == 5) discard(rsid);
	}
	else *resultsize = 0;
	return rc >> 1;
}

/**
 * execute an sql statement
 * return 0 if statement succeeded, w/wo data = update count
 * return 1 if statement succeeded, but data truncated, w/wo data = update count
 * return 2 if statement succeeded with empty result set, w/wo data = result format
 * return 3 if statement succeeded with result set, w/ data = result format
 * return negative if error
 *
 * Called only from doexecute in dbcfsrun
 */
INT sqlexecute(INT connectid, UCHAR *stmt, INT stmtlen, UCHAR *result, INT *resultsize)
{
	INT rc, rsid;

	if (connectid != cid) return sqlerrnum(SQLERR_BADCONNECTID);
	rc = execstmt(stmt, stmtlen, &rsid);
	if (rc < 0) return rc;
	if (sqlinfoflag && (rc == 0 || rc == 1)) {
		rc += 2;
		sqlmsgclear();
	}
	if (rc == 1 || rc == 3) *resultsize = mscitoa(rsid, (CHAR *) result);
	else if (rc >= 5) {
		buildresultsetstring(rc, rsid, result, resultsize);
		if (rc == 5) discard(rsid);
	}
	else *resultsize = 0;
	return rc >> 1;
}

/**
 * get result set format of an sql statement
 * return 0 if statement succeeded, w/ data = result format
 * return negative if error
 * Called only by doexecnone in dbcfsrun
 */
INT sqlexecnone(INT connectid, UCHAR *stmt, INT stmtlen, UCHAR *result, INT *resultsize)
{
	INT rc;

	if (connectid != cid) return sqlerrnum(SQLERR_BADCONNECTID);
	rc = execnonestmt(stmt, stmtlen, result, resultsize);
	rc = rc >> 1;
	return rc;
}

INT sqlrowcount(INT connectid, INT rsid, UCHAR *result, INT *resultsize)
{
	INT rc;
	LONG rowcount;

	if (connectid != cid) return sqlerrnum(SQLERR_BADCONNECTID);

	rc = getrsrowcount(rsid, &rowcount);
	if (!rc) *resultsize = mscofftoa(rowcount,(CHAR *) result);
	return rc;
}

/* get a row from the result set */
/* type: 1 = next, 2 = prev, 3 = first, 4 = last, 5 = absolute, 6 = relative */
INT sqlgetrow(INT connectid, INT rsid, INT type, LONG row, UCHAR *result, INT *resultsize)
{
	if (connectid != cid) return sqlerrnum(SQLERR_BADCONNECTID);

	return getrow(rsid, type, row, result, resultsize);
}

/* do the update positioned operation */
INT sqlposupdate(INT connectid, INT rsid, UCHAR *stmt, INT stmtlen)
{
	if (connectid != cid) return sqlerrnum(SQLERR_BADCONNECTID);
	return posupdate(rsid, stmt, stmtlen);
}

/* do the delete positioned operation */
INT sqlposdelete(INT connectid, INT rsid)
{
	if (connectid != cid) return sqlerrnum(SQLERR_BADCONNECTID);
	return posdelete(rsid);
}

/* discard the result set */
INT sqldiscard(INT connectid, INT rsid)
{
	if (connectid != cid) return sqlerrnum(SQLERR_BADCONNECTID);
	return discard(rsid);
}

static void freeconnection()
{
	INT i1, tablenum;
	TABLE *tab1;
	COLUMN *col1;
	INDEX *idx1;

	for (i1 = 1; i1 <= connection.numworksets; i1++) freeworkset(i1);
	memfree((UCHAR **) connection.hworksetarray);
	closetablelocks();
	memfree((UCHAR **) connection.hlockfilearray);
	closefilesinalltables();
	memfree((UCHAR **) connection.hopenfilearray);
	for (tablenum = 0; ++tablenum <= connection.numtables; ) {
		tab1 = tableptr(tablenum);
		for (i1 = 0; ++i1 <= tab1->numindexes; ) {
			idx1 = indexptr(tab1, i1);
			memfree((UCHAR **) idx1->hkeys);
			memfree((UCHAR **) idx1->hcolkeys);
		}
		for (i1 = 0; i1 < tab1->numcolumns; i1++) {
			col1 = (*(tab1->hcolumnarray)) + i1;
			memfree((UCHAR **) col1->hdbdproparray);
		}
		memfree((UCHAR **) tab1->hcolumnarray);
		memfree((UCHAR **) tab1->hindexarray);
		memfree((UCHAR **) tab1->hdbdproparray);
	}
	for ( ; tablenum <= connection.numalltables; tablenum++) {
		tab1 = tableptr(tablenum);
		for (i1 = 0; ++i1 <= tab1->numindexes; ) {
			idx1 = indexptr(tab1, i1);
			memfree((UCHAR **) idx1->hkeys);
			memfree((UCHAR **) idx1->hcolkeys);
		}
		memfree((UCHAR **) tab1->hindexarray);
	}
	memfree((UCHAR **) connection.htablearray);
	memfree((UCHAR **) connection.nametable);
	memfree((UCHAR **) connection.dbdfilename);
	memset(&connection, 0, sizeof(connection));
}

static void buildresultsetstring(INT rc, INT rsid, UCHAR *result, INT *resultsize)
{
	INT i1, colcount;
	LONG rowcount;
	UCHAR *p1;

	if (rc < 6) {
		memcpy(result, "NONE", 4);
		i1 = 4;
	}
	else i1 = mscitoa(rsid, (CHAR *) result);
	p1 = result + i1;
	*p1++ = ' ';
	getrsupdatable(rsid, (CHAR *) p1++);
	*p1++ = ' ';
	if (rc == 6) {
		getrsrowcount(rsid, &rowcount);
		p1 += mscofftoa(rowcount, (CHAR *) p1);
	}
	else if (rc == 7) *p1++ = '*';
	else *p1++ = '0';
	if (rc > 4) {
		*p1++ = ' ';
		colcount = getrscolcount(rsid);
		p1 += mscitoa(colcount, (CHAR *) p1);
		for (i1 = 0; ++i1 <= colcount; ) {
			*p1++ = ' ';
			strcpy((CHAR *) p1, getrscolinfo(rsid, i1));
			p1 += strlen((CHAR *) p1);
		}
	}
	*resultsize = (INT)(ptrdiff_t)(p1 - result);
}

/* set error message, return -1 */
INT sqlerrnum(INT num)
{
	INT i1, i2;
	char *code, *msg;

	if (!num) {
		memcpy(sqlerrcode, "     ", 5);
		sqlerrtext[0] = 0;
		return 0;
	}

	code = "HY000";
	if (num >= ERR_OTHER && num <= ERR_NOTOP) {
		msg = fioerrstr(num);
		if (num == ERR_NOACC)
			code = "42000";
		else if (num == ERR_NOMEM)
			code = "HY001";
	}
	else {
		switch (num) {
		case SQLERR_DBDOPENERROR:
			msg = "Error opening DBD file";
			break;
		case SQLERR_DBDSYNTAX:
			msg = "DBD file syntax error";
			break;
		case SQLERR_DBDAMBIGUOUS:
			msg = "DBD keyword is ambiguous";
			break;
		case SQLERR_CFGNOTFOUND:
			msg = "CFG file not found";
			break;
		case SQLERR_CFGSYNTAX:
			msg = "CFG file syntax error";
			break;
		case SQLERR_NAMETOOBIG:
			msg = "Name too big";
			break;
		case SQLERR_BADATTR:
			msg = "Bad attribute";
			code = "HY091";
			break;
		case SQLERR_TABLENOTFOUND:
			msg = "Table not found";
			code = "42S02";
			break;
		case SQLERR_COLUMNNOTFOUND:
			msg = "Column not found";
			code = "42S22";
			break;
		case SQLERR_BADRSID:
			msg = "Bad result set id";
			break;
		case SQLERR_BADROWID:
			msg = "Bad row id";
			break;
		case SQLERR_BADCMD:
			msg = "Bad command";
			break;
		case SQLERR_INTERNAL:
			msg = "Internal error";
			break;
		case SQLERR_NOACCESSCODE:
			msg = "Access not allowed";
			code = "42000";
			break;
		case SQLERR_NOMEM:
			msg = "Insufficient memory";
			code = "HY001";
			break;
		case SQLERR_PARSE_NOMEM:
			msg = "Insufficient memory for parsing statement";
			code = "HY001";
			break;
		case SQLERR_PARSE_TABLEALREADYEXISTS:
			msg = "Table already exists";
			code = "42S01";
			break;
		case SQLERR_PARSE_TABLENOTFOUND:
			msg = "Table not found";
			code = "42S02";
			break;
		case SQLERR_PARSE_COLUMNNOTFOUND:
			msg = "Column not found";
			code = "42S22";
			break;
		case SQLERR_PARSE_BADNUMERIC:
			msg = "Bad numeric value";
			break;
		case SQLERR_PARSE_STRINGTRUNCATED:
			msg = "String value truncated";
			break;
		case SQLERR_PARSE_READONLY:
			msg = "Table is read only";
			break;
		case SQLERR_PARSE_NOUPDATE:
			msg = "Updates prohibited on variable and/or compressed table";
			break;
		case SQLERR_PARSE_NOFORUPDATE:
			msg = "'FOR UPDATE' required on select statement for positional update/delete";
			break;
		case SQLERR_PARSE_ERROR:
			msg = "Parsing error";
			break;
		case SQLERR_EXEC_TOOBIG:
			msg = "Exec program too big";
			break;
		case SQLERR_EXEC_BADTABLE:
			msg = "Table error during execution";
			break;
		case SQLERR_EXEC_BADWORKSET:
			msg = "Working set error during execution";
			break;
		case SQLERR_EXEC_BADPGM:
			msg = "Exec program bad opcode";
			break;
		case SQLERR_EXEC_BADCOL:
			msg = "Column error during execution";
			break;
		case SQLERR_EXEC_BADDATAFILE:
			msg = "Error reading data file";
			break;
		case SQLERR_EXEC_BADWORKFILE:
			msg = "Error reading work file";
			break;
		case SQLERR_EXEC_BADROWID:
			msg = "Invalid row position";
			break;
		case SQLERR_EXEC_BADPARM:
			msg = "Invalid parameter";
			break;
		case SQLERR_EXEC_DUPKEY:
			msg = "Duplicate key";
			break;
		case SQLERR_EXEC_NOWRITE:
			msg = "Unable to write";
			break;
		case SQLERR_BADCONNECTID:
			msg = "Bad connect ID";
			break;
		case SQLERR_BADINDEX:
			msg = "Bad index";
			break;
		case SQLERR_OTHER:
			msg = "Unspecified error";
			break;
/* DBCFSRUN error messages */
		case ERR_BADINTVALUE:
			msg = "Bad integer value";
			break;
		case ERR_BADFUNC:
			msg = "Bad function keyword";
			break;
		case ERR_INVALIDATTR:
			msg = "Invalid attribute";
			break;
		case ERR_INVALIDVALUE:
			msg = "Invalid value";
			break;
		default:
			msg = "Unknown error";
			break;
		}
	}
	i1 = (INT)strlen(msg);
	if ((size_t) i1 >= sizeof(sqlerrtext)) i1 = sizeof(sqlerrtext) - 1;
	if (sqlerrtext[0] && (size_t) i1 < sizeof(sqlerrtext) - 3) {
		i2 = (INT)strlen(sqlerrtext);
		if ((unsigned) i2 > sizeof(sqlerrtext) - 3 - i1) i2 = sizeof(sqlerrtext) - 3 - i1;
		memmove(&sqlerrtext[i1 + 2], sqlerrtext, i2);
		sqlerrtext[i1 + 2 + i2] = 0;
		sqlerrtext[i1] = ':';
		sqlerrtext[i1 + 1] = ' ';
	}
	else sqlerrtext[i1] = 0;
	memcpy(sqlerrtext, msg, i1);
	memcpy(sqlerrcode, code, 5);
	return -1;
}

/* set error message text, return -1 */
INT sqlerrmsg(char *msg)
{
	INT i1, i2;

	i1 = (INT)strlen(msg);
	if ((size_t) i1 >= sizeof(sqlerrtext)) i1 = sizeof(sqlerrtext) - 1;
	if (sqlerrtext[0] && (size_t) i1 < sizeof(sqlerrtext) - 3) {
		i2 = (INT)strlen(sqlerrtext);
		if ((unsigned) i2 > sizeof(sqlerrtext) - 3 - i1) i2 = sizeof(sqlerrtext) - 3 - i1;
		memmove(&sqlerrtext[i1 + 2], sqlerrtext, i2);
		sqlerrtext[i1 + 2 + i2] = '\0';
		sqlerrtext[i1] = ':';
		sqlerrtext[i1 + 1] = ' ';
	}
	else sqlerrtext[i1] = '\0';
	memcpy(sqlerrtext, msg, i1);
	memcpy(sqlerrcode, "HY000", 5);
	return -1;
}

/*
 * set error number and two message texts, return SQL_ERROR
 */
INT sqlerrnummsg(INT num, char *msg, char *msg2)
{
	if(msg2) sqlerrmsg(msg2);
	sqlerrmsg(msg);
	if (num) sqlerrnum(num);
	return SQL_ERROR;
}

/* set success and return 0 */
INT sqlsuccess()
{
	memcpy(sqlerrcode, "     ", 5);
	sqlerrtext[0] = 0;
	return 0;
}

/* return the sql code */
char *sqlcode()
{
	return sqlerrcode;
}

/* return the sql error message */
char *sqlmsg()
{
	return sqlerrtext;
}

void sqlmsgclear()
{
	sqlerrtext[0] = 0;
	memcpy(sqlerrcode, "     ", 5);
	sqlinfoflag = FALSE;
}

void sqlswi()
{
	sqlinfoflag = TRUE;
}
