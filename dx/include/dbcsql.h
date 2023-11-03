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

#define MAXCONNS 100
#define DEFAULTCONN MAXCONNS
#define CONNAMELENGTH 20
#define MAXCURSORS 99
#define TABLE_CURSOR_NAME "table_cursor"

#ifndef CMDSIZE
#define CMDSIZE 32752
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef DBC_CUR_DYN
#define DBC_CUR_DYN 1
#define DBC_CUR_KEY 2
#define DBC_CUR_FOR 3
#define DBC_CON_LOCK 1
#define DBC_CON_OPTTIME 2
#define DBC_CON_OPTVAL 3
#define DBC_CON_RDONLY 4
#endif

#ifndef DBC_UPD
#define DBC_UPD 1
#define DBC_DEL 2
#endif

#ifndef DBC_FET_NEXT
#define DBC_FET_NEXT 1
#define DBC_FET_PREV 2
#define DBC_FET_FIRST 3
#define DBC_FET_LAST 4
#define DBC_FET_REL 5
#define DBC_FET_ABS 6
#endif

/* Utility Functions */
#ifdef INC_UTIL
#if !defined(__MACOSX) && !defined(__SQLTYPES_H)
typedef char CHAR;
#endif
extern INT dbctostr(UCHAR *, CHAR *, INT);
extern void strtodbc(CHAR *, ULONG, UCHAR *);
extern INT getcursorname(CHAR * commandString, CHAR *cursorNameBuffer, INT cbNameBufferLength);
extern INT buildsql(CHAR *, UCHAR **);
extern INT getconnectionname(CHAR *cmdbuf, CHAR *connectname);
extern INT getcursornum(CHAR *cursorname);
extern void dbcsetnull(UCHAR *dbcvar);
extern INT dbcisnull(UCHAR *dbcvar);
#endif /* INC_UTIL */

/**
 * This is copied from sqltypes.h from the MS SDK v6.1
 * It is here to facilitate conversion to 64 bit.
 * I wanted to define the third param of dbcfetch as SQLLEN since that
 * is the type passed on to ODBC driver functions.
 */
#if OS_WIN32
#if !defined(__SQLTYPES)
typedef long            SQLINTEGER;
#ifdef _WIN64
typedef INT64           SQLLEN;
#else
#define SQLLEN          SQLINTEGER
#endif /* _WIN64 */
#endif /* !defined(__SQLTYPES) */
#else /* Not OS_WIN32 */
/**
 * And this part is basically copied from sqltypes.h from Fedora 9 64-bit
 */
#if !defined(__SQLTYPES_H) && !defined(_SQLTYPES_H) && !defined(__SQLTYPES)
typedef int            SQLINTEGER;
#if defined(__x86_64__) || (sizeof(long int) == 8)
typedef long           SQLLEN;
#else
#define SQLLEN         SQLINTEGER
#endif /* 64 bit */
#endif /* sqltypes.h */
#endif /* OS_WIN32 */

/* Connection structure */
typedef struct Connection CONNECTION;
struct Connection {
	SQLHDBC hdbc;
	INT extendedFetch;
	INT connected;
	CHAR name[CONNAMELENGTH + 1];
};

#ifdef INC_TARGET
/**
 * The index is a cursornumber
 * The value is a connection number
 */
extern INT cursor2connection[MAXCURSORS];

/**
 * Filled with NULLs automatically, we rely on this fact
 */
extern CONNECTION connections[MAXCONNS + 1];
#else
INT cursor2connection[MAXCURSORS];
CONNECTION connections[MAXCONNS + 1];
#endif

/* Functions peculiar to target */
#ifdef INC_TARGET
extern INT getcursornum(CHAR *);
extern INT dbcconnect(CHAR *, CHAR *, CHAR *, CHAR *, CHAR *, INT connectionNum);
extern INT dbcclose(INT);
extern INT dbcdisconnect(INT connectionNum);
extern INT dbcfetch(INT, INT, SQLLEN, UCHAR **, INT connectionNum);
extern INT dbcselect(CHAR *, INT, INT, INT, INT connectionNum);
extern INT dbcupddel(CHAR *, INT connectionNum);
extern INT dbcexecdir(CHAR *, INT cursorNum, INT connectionNum);
extern CHAR *dbcgeterr(void);
#endif
