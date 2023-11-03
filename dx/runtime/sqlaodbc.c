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

#if defined(__unix)
__asm__(".symver memcpy,memcpy@GLIBC_2.2.5");
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#if defined(_WIN32)
#include <windows.h>
#include <tchar.h>
static HCURSOR waitCursor;
#endif

#if defined(__MACOSX) || defined(Linux) || defined(FREEBSD)
#include <inttypes.h>
typedef int INT;
typedef uint64_t ULONG_PTR;
#endif

#if defined (UNIX) || defined(unix)
#define OS_UNIX 1
#else
#define OS_UNIX 0
#endif

#include <sql.h>  /* From ODBC SDK */
#include <sqlext.h>  /* From ODBC SDK */

#define INC_UTIL
#define INC_TARGET
#include "dbcsql.h"

/* Cursor stucture */
union lengthType {
	SQLLEN dwLen;
};

struct cursearr {
	SQLCHAR cursorname[32];
	SQLCHAR **szColData;
	union lengthType *dwLen;// array of four or 16 byte values
	SQLULEN *udwColPrec;
	SQLSMALLINT *sDataType;	// array of two byte values
	SQLHSTMT hstmt;
	SWORD nResultCols;
};

static struct cursearr cursor_array[MAXCURSORS];
static INT sqlinitialized;

static CHAR errbuf[CMDSIZE];

/* Error variables */
static SQLCHAR szErrText[SQL_MAX_MESSAGE_LENGTH+1];
static SQLCHAR szErrState[SQL_SQLSTATE_SIZE+1];
static SQLSMALLINT wErrMsgLen;
static SQLINTEGER dwNativeErrCode;

/* Handles */
static SQLHENV henv;

static INT allocateCursor(struct cursearr *cursorArray);
static void calcWorkVarSize(SQLSMALLINT nColType, SQLULEN udwColPrec, INT *maxcol);
static void dbcsqlerr(SQLSMALLINT handelType, SQLHANDLE handle);
static INT sqlinit(void);

#ifndef _INCLUDES_INCLUDED
#define RC_ERROR (-1)
#endif

/*
 * We define our own proxy for SQLDescribeCol
 * to deal with the stupid Itanium data alignment requirements
 */
SQLRETURN dbcDescribeCol(
      SQLHSTMT       StatementHandle,		// [input]
      SQLUSMALLINT   ColumnNumber,			// [input]
      SQLCHAR *      ColumnName,			// [output] We always pass NULL for this!
      SQLSMALLINT    BufferLength,			// [input]  We always pass zero for this
      SQLSMALLINT *  NameLengthPtr,			// [output]
      SQLSMALLINT *  DataTypePtr,			// [output] <- this one we have to worry about on Itanium
      SQLULEN *      ColumnSizePtr,			// [output] <- this one we have to worry about on Itanium
      SQLSMALLINT *  DecimalDigitsPtr,		// [output] We do not use this one
      SQLSMALLINT *  NullablePtr);			// [output]

/*
 * GETCURSORNUM
 *
 * If found returns index into the cursor array.
 * If not found, a new entry is made and its index returned
 * If not found and table is full returns RC_ERROR
 */
INT getcursornum(CHAR *cursorname)
{
	INT cursornum;

	for (cursornum = 0; cursornum < MAXCURSORS; cursornum++) {
		if (!strcmp((CHAR *)cursor_array[cursornum].cursorname, cursorname)) break;
	}
	if (cursornum == MAXCURSORS) {
		/* New cursor so find an empty slot */
		for (cursornum = 0; cursornum < MAXCURSORS; cursornum++) {
			if (!cursor_array[cursornum].cursorname[0]) break;
		}
		if (cursornum == MAXCURSORS) return RC_ERROR;
		strcpy((CHAR *)cursor_array[cursornum].cursorname, cursorname);
	}
	return(cursornum);
}

/* SQLINIT
 *
 * Allocate the ODBC Environment Handle.
 * And tell ODBC that we expect version 3.0 behavior.
 * This should be done only once!
 *
 */
static INT sqlinit(void)
{
	INT retcode;
	if ((retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv)) != SQL_SUCCESS) {
		dbcsqlerr(SQL_HANDLE_ENV, SQL_NULL_HANDLE);
		return(retcode);
	}
	retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
	if (retcode != SQL_SUCCESS) {
		dbcsqlerr(SQL_HANDLE_ENV, henv);
		return(retcode);
	}

#if defined(_WIN32)
	waitCursor = LoadCursor(NULL, IDC_WAIT);
#endif
	sqlinitialized = 1;
	return retcode;
}

/* DBCCONNECT */
INT dbcconnect(CHAR *cmdbuf, CHAR *uid, CHAR *passwd, CHAR *server, CHAR *string,
		INT connectnum)
{
	INT retcode, lenptr;
	SQLUSMALLINT flag;
#if defined(_WIN32)
	HCURSOR hOldCursor;
#endif

	if (!sqlinitialized) {
		retcode = sqlinit();
		if (retcode != SQL_SUCCESS) return(retcode);
	}

	if ((retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv,
			&connections[connectnum].hdbc)) != SQL_SUCCESS) {
		dbcsqlerr(SQL_HANDLE_ENV, henv);
		return(retcode);
	}
	/**
	 * Used to be SQL_CUR_USE_IF_NEEDED but this is deprected.
	 * MS intends to remove the ODBC cursor library in a future release.
	 * see http://msdn.microsoft.com/en-us/library/ms713605(v=vs.85).aspx
	 *
	 * JPR 02 OCT 2013
	 */
	SQLSetConnectAttr(connections[connectnum].hdbc,
			SQL_ATTR_ODBC_CURSORS, (SQLPOINTER)SQL_CUR_USE_DRIVER, 0);

#if defined(_WIN32)
	hOldCursor = SetCursor(waitCursor);
#endif

	if (!server[0] || string[0]) { /* If SERVER is not supplied OR STRING is */
		if (server[0] || uid[0] || passwd[0] || string[0]) {
			cmdbuf[0] = '\0';
			if (server[0]) {
				strcat(cmdbuf, "DSN=");
				strcat(cmdbuf, server);
				strcat(cmdbuf, ";");
			}
			if (uid[0]) {
				strcat(cmdbuf, "UID=");
				strcat(cmdbuf, uid);
				strcat(cmdbuf, ";");
			}
			if (passwd[0]) {
				strcat(cmdbuf, "PWD=");
				strcat(cmdbuf, passwd);
				strcat(cmdbuf, ";");
			}
			if (string[0]) strcat(cmdbuf, string);
		}

		SQLHWND hwnd = NULL;
#if defined(_WIN32)
		hwnd = GetActiveWindow();
		if (hwnd == NULL) {
			hwnd = GetDesktopWindow();
			if (hwnd == NULL) {
			}
		}
#endif
		retcode = SQLDriverConnect(connections[connectnum].hdbc,
		hwnd,
		(SQLCHAR *) cmdbuf, (SQLSMALLINT) strlen(cmdbuf),
		(SQLCHAR *) cmdbuf, (SQLSMALLINT) CMDSIZE, (SQLSMALLINT*) &lenptr,
		SQL_DRIVER_COMPLETE_REQUIRED);
	}
	else {	/* If SERVER is supplied AND STRING is not */
		retcode = SQLConnect(connections[connectnum].hdbc,
			(SQLCHAR *) server, SQL_NTS,
			(SQLCHAR *) uid, SQL_NTS,
			(SQLCHAR *) passwd, SQL_NTS);
	}

	if (retcode != SQL_SUCCESS) {
		dbcsqlerr(SQL_HANDLE_DBC, connections[connectnum].hdbc);
		if (retcode != SQL_SUCCESS_WITH_INFO) {
			SQLFreeHandle(SQL_HANDLE_DBC, connections[connectnum].hdbc);
			connections[connectnum].hdbc = SQL_NULL_HDBC;
		}
	}

	if (connections[connectnum].hdbc != SQL_NULL_HDBC) {
		/**
		 * Note: This call is handled by the ODBC manager.
		 * On Windows it seems to allocate two system handles
		 * and not release them.
		 * This can accumulate to the point of being a problem
		 * if the program runs for a *very, very* long time
		 */
		if (SQLGetFunctions(connections[connectnum].hdbc,
			SQL_API_SQLEXTENDEDFETCH, &flag) != SQL_ERROR && flag)
			connections[connectnum].extendedFetch = TRUE;
		else connections[connectnum].extendedFetch = FALSE;
	}

#if defined(_WIN32)
	SetCursor(hOldCursor);
#endif
	return(retcode);
}

/**
 * Allocate memory for a cursor structure
 *
 * Returns 0 if success, RC_ERROR otherwise
 */
static INT allocateCursor(struct cursearr *cursorArray) {
	SWORD ncols;
	ncols = cursorArray->nResultCols;
	cursorArray->szColData = (SQLCHAR **)malloc(ncols * sizeof(SQLCHAR *));
	cursorArray->udwColPrec = (SQLULEN *) malloc(ncols * sizeof(SQLULEN));
	cursorArray->sDataType = (SQLSMALLINT *) malloc(ncols * sizeof(SQLSMALLINT));
	cursorArray->dwLen = (union lengthType *)malloc(ncols * sizeof(union lengthType));
	if (cursorArray->szColData == NULL || cursorArray->udwColPrec == NULL
			|| cursorArray->dwLen == NULL || cursorArray->sDataType == NULL) {
		strcpy((CHAR *)szErrState, "HY001");
		sprintf((CHAR *)szErrText, "malloc fail in allocateCursor, ncols=%i", (INT)ncols);
		wErrMsgLen = (SQLSMALLINT)strlen((CHAR *)szErrText);
		dwNativeErrCode = 0;
		return RC_ERROR;
	}
	return 0;
}

/**
 * free previous allocations of memory in a cursor structure
 */
static void freeCursorAllocation(struct cursearr *cursorArray) {
	SWORD ncols;
	INT loop;
	if (cursorArray->szColData != NULL) {
		ncols = cursorArray->nResultCols;
		for (loop = 0; loop < ncols; loop++) free(cursorArray->szColData[loop]);
		free(cursorArray->szColData);
		free(cursorArray->udwColPrec);
		free(cursorArray->sDataType);
		free(cursorArray->dwLen);
		cursorArray->szColData = NULL;
	}
}

/* DBCCLOSE */
INT dbcclose(INT cursornum)
{
	SQLHSTMT* stmtptr;
	struct cursearr *cu;

	cu = &cursor_array[cursornum];

	stmtptr = &cu->hstmt;

	if (*stmtptr != NULL) {
		SQLFreeStmt(*stmtptr, SQL_DROP);
		*stmtptr = NULL;
	}

	cu->cursorname[0] = '\0';
	freeCursorAllocation(cu);
	return 0;
}

/* DBCDISCONNECT */
INT dbcdisconnect(INT connectionnum)
{
	INT cursornum, retcode;
	SQLHSTMT* stmtptr;
	SQLHDBC hdbc = connections[connectionnum].hdbc;
	struct cursearr *cu;

	/*
	 * A cursor can be associated with only one connection at a time, but a connection
	 * can be associated with more than one cursor.
	 * This association is kept track of in the cursor2connection array.
	 */
	for (cursornum = 0; cursornum < MAXCURSORS; cursornum++) {
		if (cursor2connection[cursornum] == connectionnum) {
			cu = &cursor_array[cursornum];
			stmtptr = &cu->hstmt;
			if (*stmtptr != NULL) dbcclose(cursornum);
		}
	}

	SQLDisconnect(hdbc);
	retcode = SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	if (retcode != SQL_SUCCESS) dbcsqlerr(SQL_HANDLE_DBC, hdbc);
	connections[connectionnum].hdbc = SQL_NULL_HDBC;
	return(0);
}

/* DBCFETCH */
INT dbcfetch(INT cursornum, INT fetchtype, SQLLEN index, UCHAR **destarr,
		INT connectionnum)
{
	INT extendedflag = connections[connectionnum].extendedFetch;
	SWORD ncols;
	SQLHSTMT *stmtptr;
	UCHAR **dbcvar;
	INT flag;
	INT fnotfound;
	INT loop;
	struct cursearr *cu;
	INT retcode = 0;

	cu = &cursor_array[cursornum];

	ncols = cu->nResultCols;
	stmtptr = &(cu->hstmt);
	if (ncols) {
		fnotfound = FALSE;
		flag = FALSE;
		switch (fetchtype) {
		case DBC_FET_PREV:
			if (extendedflag) retcode = SQLFetchScroll(*stmtptr, SQL_FETCH_PRIOR, 0);
			else flag = TRUE;
			break;
		case DBC_FET_FIRST:
			if (extendedflag) retcode = SQLFetchScroll(*stmtptr, SQL_FETCH_FIRST, 0);
			else flag = TRUE;
			break;
		case DBC_FET_LAST:
			if (extendedflag) retcode = SQLFetchScroll(*stmtptr, SQL_FETCH_LAST, 0);
			else flag = TRUE;
			break;
		case DBC_FET_REL:
			if (extendedflag) retcode = SQLFetchScroll(*stmtptr, SQL_FETCH_RELATIVE, index);
			else flag = TRUE;
			break;
		case DBC_FET_ABS:
			if (extendedflag) retcode = SQLFetchScroll(*stmtptr, SQL_FETCH_ABSOLUTE, index);
			else flag = TRUE;
			break;
		case DBC_FET_NEXT:
			if (extendedflag) retcode = SQLFetchScroll(*stmtptr, SQL_FETCH_NEXT, 0);
			else retcode = SQLFetch(*stmtptr);
			break;
		}

		if (flag) {
			strcpy((CHAR *)szErrState, "IM001");
			strcpy((CHAR *)szErrText, "Driver does not support SQLExtendedFetch");
			wErrMsgLen = (SQLSMALLINT)strlen((CHAR *)szErrText);
			dwNativeErrCode = 0;
			retcode = SQL_ERROR;
		}
		else if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			if (retcode == SQL_SUCCESS_WITH_INFO)
				dbcsqlerr(SQL_HANDLE_STMT, *stmtptr);
			dbcvar = destarr;
			for (loop = 0; loop < ncols && dbcvar[loop] != NULL; loop++) {
				if (cu->dwLen[loop].dwLen == SQL_NULL_DATA /*SQL_NO_TOTAL is -4 */) {
					dbcsetnull(dbcvar[loop]);
				}
				else {
					strtodbc((CHAR *)cu->szColData[loop],
							(ULONG)cu->dwLen[loop].dwLen,
							dbcvar[loop]);
				}
			}
		}
		else if (retcode < 0) dbcsqlerr(SQL_HANDLE_STMT, *stmtptr);
	}
	else fnotfound = TRUE;
	return(((fnotfound) ? 100 : retcode));
}

/* DBCSELECT */
INT dbcselect(CHAR *cmdbuf, INT cursornum, INT scroll, INT lock, INT connectionnum)
{
	SQLHSTMT *stmtptr;
	SQLSMALLINT ncols, nColNameLen, nScale, fNullable;
	SQLHDBC hdbc = connections[connectionnum].hdbc;
	INT retcode, retcode2, maxcol;
	SQLSMALLINT loop;
	UDWORD odbcscroll;
	SQLPOINTER odbclock;
	SQLCHAR cursorName[20];
	struct cursearr *cursorArray;

#if defined(_WIN32)
	HCURSOR hOldCursor;
#endif

#if defined(_WIN32)
	hOldCursor = SetCursor(waitCursor);
#endif

	/*
	 * Sanity check that cursornum is valid
	 */
	if (cursornum < 0 || MAXCURSORS <= cursornum || cursor_array[cursornum].cursorname[0] == '\0') {
		strcpy((CHAR *)szErrState, "HY013");
		sprintf((CHAR *)szErrText, "bad cursor number passed into dbcselect, %i", cursornum);
		wErrMsgLen = (SQLSMALLINT)strlen((CHAR *)szErrText);
		dwNativeErrCode = 0;
		retcode = RC_ERROR;
		goto exit;
	}

	cursorArray = &cursor_array[cursornum];
	stmtptr = &cursorArray->hstmt;

	/* Was this statement/cursor already allocated ? */
	if (*stmtptr != NULL) {
		SQLFreeStmt(*stmtptr, SQL_DROP);
		*stmtptr = NULL;
	}
	retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, stmtptr);
	if (retcode == SQL_ERROR || retcode == SQL_INVALID_HANDLE) {
		dbcsqlerr(SQL_HANDLE_DBC, hdbc);
		retcode = RC_ERROR;
		goto exit;
	}
	freeCursorAllocation(cursorArray);

	/* Set cursor name */
	strcpy((char*)cursorName, (const char*)cursorArray->cursorname);
	retcode = SQLSetCursorName(*stmtptr, cursorName, SQL_NTS);
	if (retcode == SQL_ERROR || retcode == SQL_INVALID_HANDLE) {
		dbcsqlerr(SQL_HANDLE_STMT, *stmtptr);
		retcode = RC_ERROR;
		goto exit;
	}

	/* cursor options */
	if (lock) {
		switch (lock) {
		case DBC_CON_OPTTIME:
			odbclock = (SQLPOINTER)SQL_CONCUR_ROWVER;
			break;
		case DBC_CON_OPTVAL:
			odbclock = (SQLPOINTER)SQL_CONCUR_VALUES;
			break;
		case DBC_CON_RDONLY:
			odbclock = (SQLPOINTER)SQL_CONCUR_READ_ONLY;
			break;
		default:				/* DBC_CON_LOCK */
			odbclock = (SQLPOINTER)SQL_CONCUR_LOCK;
			break;
		}
/*		retcode = SQLSetStmtOption(*stmtptr, SQL_CONCURRENCY, odbclock); */
		retcode = SQLSetStmtAttr(*stmtptr, SQL_ATTR_CONCURRENCY, odbclock, 0);
		if (retcode == SQL_ERROR || retcode == SQL_INVALID_HANDLE) {
			dbcsqlerr(SQL_HANDLE_STMT, *stmtptr);
			retcode = RC_ERROR;
			goto exit;
		}
	}
	if (scroll) {
		switch (scroll) {
		case DBC_CUR_FOR:
			odbcscroll = SQL_CURSOR_FORWARD_ONLY;
			break;
		case DBC_CUR_KEY:
			odbcscroll = SQL_CURSOR_KEYSET_DRIVEN;
			break;
		default:				/* DBC_CUR_DYN */
			odbcscroll = SQL_CURSOR_DYNAMIC;
			break;
		}
		retcode = SQLSetStmtAttr(*stmtptr, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) (ULONG_PTR) odbcscroll, 0);
		if (retcode == SQL_ERROR || retcode == SQL_INVALID_HANDLE) {
			dbcsqlerr(SQL_HANDLE_STMT, *stmtptr);
			retcode = RC_ERROR;
			goto exit;
		}
	}
	/* Execute the Select */
	retcode = SQLExecDirect(*stmtptr, (SQLCHAR*) cmdbuf, SQL_NTS);
	if (retcode != SQL_SUCCESS)
		dbcsqlerr(SQL_HANDLE_STMT, *stmtptr);
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		/* Allocate memory for a single result row */
		if (SQLNumResultCols(*stmtptr, &ncols) != SQL_SUCCESS) ncols = 0;
		cursorArray->nResultCols = ncols;
		if (ncols > 0) {
			if ( (retcode = allocateCursor(cursorArray)) != 0) goto exit;
		}

		/* Bind result columns to work variables */
		for (loop = 0; loop < ncols; loop++) {
			/* Get the scale info for column nCol */

			retcode2 = dbcDescribeCol(
					*stmtptr,							// Statement handle
					(SQLUSMALLINT)(loop + 1),			// Column number, one-based
					NULL,								// Column name
					0,									// Length of the column name buffer
					&nColNameLen,						// total number of bytes returned in column name, Not used
					&cursorArray->sDataType[loop],		// The SQL data type of the column
					&cursorArray->udwColPrec[loop],		// The size of the column on the data source
					&nScale,							// Number of decimal digits, Not used
					&fNullable);						// Nullable flag, Not used
			if (retcode2 == SQL_ERROR) {
				dbcsqlerr(SQL_HANDLE_STMT, *stmtptr);
			}

			/* Allocate work variable for column loop */
			calcWorkVarSize((SQLSMALLINT)cursorArray->sDataType[loop], cursorArray->udwColPrec[loop], &maxcol);

			cursorArray->szColData[loop] = (SQLCHAR *)malloc(sizeof(SQLCHAR) * maxcol);
			if (cursorArray->szColData[loop] == NULL) {
				strcpy((CHAR *)szErrState, "HY001");
				sprintf((CHAR *)szErrText, "malloc fail during Select processing, maxcol=%i", maxcol);
				wErrMsgLen = (SQLSMALLINT)strlen((CHAR *)szErrText);
				dwNativeErrCode = 0;
				retcode = SQL_ERROR;
				break;
			}

			/* Bind column nCol to work variable */
			retcode2 = SQLBindCol(*stmtptr,
					(SQLUSMALLINT)(loop + 1),
					SQL_C_CHAR,
					cursorArray->szColData[loop],
					(SQLLEN) (maxcol -1),
					&cursorArray->dwLen[loop].dwLen);
			if (retcode2 == SQL_ERROR) dbcsqlerr(SQL_HANDLE_STMT, *stmtptr);
		}
	}
	else cursorArray->nResultCols = 0;
exit:
#if defined(_WIN32)
	SetCursor(hOldCursor);
#endif
	return(retcode);
}

/*
 * Calculate the size of work variable for given column
 */
static void calcWorkVarSize(SQLSMALLINT nColType, SQLULEN udwColPrec, INT *maxcol) {
	if (nColType == SQL_CHAR || nColType == SQL_VARCHAR || nColType == SQL_LONGVARCHAR)
		*maxcol = (INT)udwColPrec + 3;
	else if (nColType == SQL_REAL || nColType == SQL_FLOAT || nColType == SQL_DOUBLE || nColType == SQL_TYPE_TIMESTAMP)
		*maxcol = (INT)udwColPrec + 10;
	else if (nColType == SQL_BINARY || nColType == SQL_VARBINARY || nColType == SQL_LONGVARBINARY)
		*maxcol = (INT)(udwColPrec * 2) + 4;
	else if (nColType == SQL_INTEGER) *maxcol = 13;
	else *maxcol = (INT)udwColPrec + 4;
}

/* DBCUPDDEL */
INT dbcupddel(CHAR *cmdbuf, INT connectionnum)
{
	SQLHSTMT stmt;
	SQLHDBC hdbc = connections[connectionnum].hdbc;
	INT retcode;

	retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &stmt);
	if (retcode == SQL_ERROR || retcode == SQL_INVALID_HANDLE) {
		dbcsqlerr(SQL_HANDLE_DBC, hdbc);
		return RC_ERROR;
	}

	retcode = SQLExecDirect(stmt, (SQLCHAR*) cmdbuf, SQL_NTS);
	if (retcode != SQL_SUCCESS) dbcsqlerr(SQL_HANDLE_STMT, stmt);

	SQLFreeStmt(stmt, SQL_DROP);
	return(retcode);
}

/* DBCEXECDIR */
INT dbcexecdir(CHAR *cmdbuf, INT cursornum, INT connectionnum)
{
	INT loop, maxcol, retcode, retcode2, idx;
	SWORD ncols;
	SWORD nScale, fNullable;
	SQLSMALLINT nColNameLen;
	SQLHSTMT stmt, *stmtptr;
	SQLHDBC hdbc;
	struct cursearr *cu;
#if defined(_WIN32)
	HCURSOR hOldCursor;
#endif

#if defined(_WIN32)
	hOldCursor = SetCursor(waitCursor);
#endif

	/*
	 * Count the number of connections and verify that connectionnum is valid
	 */
	if (connectionnum != DEFAULTCONN) {
		for (idx = 0; idx < MAXCONNS; idx++) {
			if (!connections[idx].name[0]) break;
		}
		if (connectionnum < 0 || idx <= connectionnum) {
			strcpy((CHAR *)szErrState, "HY013");
			sprintf((CHAR *)szErrText, "bad connection number passed into dbcexecdir, %i", connectionnum);
			wErrMsgLen = (SQLSMALLINT)strlen((CHAR *)szErrText);
			dwNativeErrCode = 0;
			retcode = RC_ERROR;
			goto exit;
		}
	}

	hdbc = connections[connectionnum].hdbc;

	/*
	 * Count the number of cursors and verify that cursornum is valid
	 * (A -1 value is valid and means that the user did not specify a cursor)
	 */
	if (cursornum != -1) {
		for (idx = 0; idx < MAXCURSORS; idx++) {
			if (!cursor_array[idx].cursorname[0]) break;
		}
		if (cursornum < 0 || idx <= cursornum) {
			strcpy((CHAR *)szErrState, "HY013");
			sprintf((CHAR *)szErrText, "bad cursor number passed into dbcexecdir, %i", cursornum);
			wErrMsgLen = (SQLSMALLINT)strlen((CHAR *)szErrText);
			dwNativeErrCode = 0;
			retcode = RC_ERROR;
			goto exit;
		}
	}

	if (cursornum >= 0) {
		cu = &cursor_array[cursornum];
		stmtptr = &cu->hstmt;

		/* Was this statement/cursor already allocated ? */
		if (*stmtptr != NULL) {
			SQLFreeStmt(*stmtptr, SQL_DROP);
			*stmtptr = NULL;
		}
		freeCursorAllocation(cu);
	}
	else stmtptr = &stmt;

	/* Allocate a statement handle for sql command */
	retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, stmtptr);
	if (retcode == SQL_ERROR || retcode == SQL_INVALID_HANDLE) {
		dbcsqlerr(SQL_HANDLE_DBC, hdbc);
		retcode = RC_ERROR;
		goto exit;
	}

	if (cursornum >= 0) {  /* Set cursor name */
		retcode = SQLSetCursorName(*stmtptr, cu->cursorname, SQL_NTS);
		if (retcode == SQL_ERROR || retcode == SQL_INVALID_HANDLE) {
			dbcsqlerr(SQL_HANDLE_STMT, *stmtptr);
			retcode = RC_ERROR;
			goto exit;
		}
	}

	/* Execute the statement */
	retcode = SQLExecDirect(*stmtptr, (SQLCHAR*) cmdbuf, SQL_NTS);
	if (retcode != SQL_SUCCESS) {
		dbcsqlerr(SQL_HANDLE_STMT, *stmtptr);
	}

	if (cursornum >= 0) {
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			/* Allocate memory for a single result row */
			if (SQLNumResultCols(*stmtptr, &ncols) != SQL_SUCCESS) ncols = 0;
			cu->nResultCols = ncols;
			if (ncols > 0) {
				if ( (retcode = allocateCursor(cu)) != 0) goto exit;
			}

			/* Bind result columns to work variables */
			for (loop = 0; loop < ncols; loop++) {
				/* Get the scale info for column nCol */
				retcode2 = dbcDescribeCol(*stmtptr,
						(SQLSMALLINT)(loop + 1),
						NULL,
						0,
						&nColNameLen,
						(SQLSMALLINT*)&cu->sDataType[loop],
						&cu->udwColPrec[loop],
						&nScale,
						&fNullable);
				if (retcode2 == SQL_ERROR) {
					dbcsqlerr(SQL_HANDLE_STMT, *stmtptr);
					retcode = RC_ERROR;
					break;
				}

				/* Allocate work variable for column loop */
				calcWorkVarSize((SQLSMALLINT)cu->sDataType[loop], cu->udwColPrec[loop], &maxcol);

				cu->szColData[loop] = (SQLCHAR *)malloc(sizeof(SQLCHAR) * maxcol);
				if (cu->szColData[loop] == NULL) {
					strcpy((CHAR *)szErrState, "HY001");
					sprintf((CHAR *)szErrText, "malloc fail in dbcexecdir, maxcol=%i", maxcol);
					wErrMsgLen = (SQLSMALLINT)strlen((CHAR *)szErrText);
					dwNativeErrCode = 0;
					retcode = SQL_ERROR;
					break;
				}

				/* Bind column nCol to work variable */
				retcode2 = SQLBindCol(*stmtptr,
						(SQLSMALLINT)(loop + 1),
						SQL_C_CHAR,
						cu->szColData[loop],
						(SQLLEN)(maxcol - 1),
						&cu->dwLen[loop].dwLen);
				if (retcode2 == SQL_ERROR) {
					dbcsqlerr(SQL_HANDLE_STMT, *stmtptr);
					retcode = RC_ERROR;
					break;
				}
			}
		}
		else cu->nResultCols = 0;
	}
	else SQLFreeStmt(stmt, SQL_DROP);

exit:

#if defined(_WIN32)
	SetCursor(hOldCursor);
#endif
	return(retcode);
}

/* DBCGETERR */
CHAR *dbcgeterr()
{
	CHAR temp[20];
	long longtemp;

	if (!wErrMsgLen) errbuf[0] = '\0';
	else {
		longtemp = dwNativeErrCode;
		strcpy(errbuf, (CHAR *)szErrState);
		strcat(errbuf, ": ");
		strcat(errbuf, (CHAR *)szErrText);
		if (dwNativeErrCode) {
			strcat(errbuf, ": (#");
			sprintf(temp, "%ld", longtemp);
			strcat(errbuf, temp);
			strcat(errbuf, ")");
			dwNativeErrCode = 0;
		}
		wErrMsgLen = 0;
	}
	return(errbuf);
}

static void dbcsqlerr(SQLSMALLINT handleType, SQLHANDLE handle)
{
	SQLGetDiagRec(handleType, handle, 1,
		szErrState,
		&dwNativeErrCode,
		szErrText, (SQLSMALLINT) sizeof(szErrText),
		&wErrMsgLen);
}

SQLRETURN dbcDescribeCol(
      SQLHSTMT       StatementHandle,		// [input]
      SQLUSMALLINT   ColumnNumber,			// [input]
      SQLCHAR *      ColumnName,			// [output] We always pass NULL for this!
      SQLSMALLINT    BufferLength,			// [input]  We always pass zero for this
      SQLSMALLINT *  NameLengthPtr,			// [output]
      SQLSMALLINT *  DataTypePtr,			// [output] <- this one we have to worry about
      SQLULEN *      ColumnSizePtr,			// [output] <- this one we have to worry about
      SQLSMALLINT *  DecimalDigitsPtr,		// [output] We do not use this one
      SQLSMALLINT *  NullablePtr)			// [output]
{
	return SQLDescribeCol(StatementHandle, ColumnNumber, NULL, 0, NameLengthPtr,
			DataTypePtr, ColumnSizePtr, DecimalDigitsPtr, NullablePtr);
}
