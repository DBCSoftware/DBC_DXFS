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
#define INC_SQL
#define INC_SQLEXT
#include "includes.h"
#include "fsodbc.h"
#include "tcp.h"
#include "base.h"

#if OS_UNIX
#define _memicmp(a, b, c) strncasecmp(a, b, c)
#define _itoa(a, b, c) mscitoa(a, b)
#define _ltoa(a, b, c) mscitoa(a, b)
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#endif

#define SERVER_V2	0x01
#define SERVER_V3	0x02

#define TYPE_OTHER		0
#define TYPE_UPDATE		1
#define TYPE_DELETE		2
#define TYPE_CREATE     3
#define TYPE_ALTER      4
#define TYPE_DROP       5

/* note: access to workbuffer must be protected with entersync/exitsync */
extern CHAR workbuffer[65536];

typedef struct columns_struct {
	CHAR *name;				/* ODBC defined name (may change with ODBC versions) */
	SQLSMALLINT	type;		/* default SQL_C type if not SQL_C_CHAR */
	SQLSMALLINT nullable;	/* colunn is nullable */
	CHAR *colinfo;			/* replacement column info to parse if column name not found */
	INT versions;			/* server versions this column is included in result set */
} COLUMNS;

static COLUMNS columns[] = {
	{ "TABLE_CAT", 0, SQL_NULLABLE, "(C31)", SERVER_V2 },
	{ "TABLE_SCHEM", 0, SQL_NULLABLE, "(C31)", SERVER_V2 },
	{ "TABLE_NAME", 0, SQL_NO_NULLS, "(C31)", SERVER_V2 | SERVER_V3 },
	{ "COLUMN_NAME", 0, SQL_NO_NULLS, "(C31)", SERVER_V2 | SERVER_V3 },
/*** NOTE: V2 gets (N4), V3 gets (C1), descriptor record should be (N4) ***/
	{ "DATA_TYPE", SQL_C_SHORT, SQL_NO_NULLS, "(C1)", SERVER_V2 | SERVER_V3 },
	{ "TYPE_NAME", 0, SQL_NO_NULLS, "(C9)", SERVER_V2 },
	{ "COLUMN_SIZE", SQL_C_LONG, SQL_NULLABLE, "(N6)", SERVER_V2 | SERVER_V3 },
	{ "BUFFER_LENGTH", SQL_C_LONG, SQL_NULLABLE, "(N6)", SERVER_V2 },
	{ "DECIMAL_DIGITS", SQL_C_SHORT, SQL_NULLABLE, "(N2)", SERVER_V2 | SERVER_V3 },
	{ "NUM_PREC_RADIX", SQL_C_SHORT, SQL_NULLABLE, "(N6)", SERVER_V2 },
	{ "NULLABLE", SQL_C_SHORT, SQL_NO_NULLS, "(N1)", SERVER_V2 },
	{ "REMARKS", 0, SQL_NO_NULLS, "(C63)", SERVER_V2 | SERVER_V3 },
	{ "COLUMN_DEF", 0, SQL_NULLABLE, "(C1)", SERVER_V2 },
	{ "SQL_DATA_TYPE", SQL_C_SHORT, SQL_NO_NULLS, "(N6)", SERVER_V2 },
	{ "SQL_DATETIME_SUB", SQL_C_SHORT, SQL_NULLABLE, "(N6)", SERVER_V2 },
	{ "CHAR_OCTET_LENGTH", SQL_C_ULONG, SQL_NULLABLE, "(N6)", SERVER_V2 },
	{ "ORDINAL_POSITION", SQL_C_LONG, SQL_NO_NULLS, "(N6)", SERVER_V2 | SERVER_V3 },
	{ "IS_NULLABLE", 0, SQL_NULLABLE, "(C3)", SERVER_V2 }
};

static COLUMNS gettypeinfo[] = {
	{ "TYPE_NAME", 0, SQL_NO_NULLS, "(C20)", 0 },
/*** NOTE: internally create row with correct SQL value ***/
	{ "DATA_TYPE", SQL_C_SHORT, SQL_NO_NULLS, "(N4)", SERVER_V2 | SERVER_V3 },
	{ "COLUMN_SIZE", SQL_C_LONG, SQL_NULLABLE, "(N10)", 0 },
	{ "LITERAL_PREFIX", 0, SQL_NULLABLE, "(C1)", 0 },
	{ "LITERAL_SUFFIX", 0, SQL_NULLABLE, "(C1)", 0 },
	{ "CREATE_PARAMS", 0, SQL_NULLABLE, "(C20)", 0 },
	{ "NULLABLE", SQL_C_SHORT, SQL_NO_NULLS, "(N1)", 0 },
	{ "CASE_SENSITIVE", SQL_C_SHORT, SQL_NO_NULLS, "(N1)", 0 },
	{ "SEARCHABLE", SQL_C_SHORT, SQL_NO_NULLS, "(N1)", 0 },
	{ "UNSIGNED_ATTRIBUTE", SQL_C_SHORT, SQL_NULLABLE, "(N1)", 0 },
	{ "FIXED_PREC_SCALE", SQL_C_SHORT, SQL_NO_NULLS, "(N1)", 0 },
	{ "AUTO_UNIQUE_VALUE", SQL_C_SHORT, SQL_NULLABLE, "(N1)", 0 },
	{ "LOCAL_TYPE_NAME", 0, SQL_NULLABLE, "(C1)", 0 },
	{ "MINIMUM_SCALE", SQL_C_SHORT, SQL_NULLABLE, "(N6)", 0 },
	{ "MAXIMUM_SCALE", SQL_C_SHORT, SQL_NULLABLE, "(N6)", 0 },
	{ "SQL_DATA_TYPE", SQL_C_SHORT, SQL_NO_NULLS, "(N6)", 0 },
	{ "SQL_DATETIME_SUB", SQL_C_SHORT, SQL_NULLABLE, "(N6)", 0 },
	{ "NUM_PREC_RADIX", SQL_C_LONG, SQL_NULLABLE, "(N6)", 0 },
	{ "INTERVAL_PRECISION", SQL_C_SHORT, SQL_NULLABLE, "(N5)", 0 }
};

static COLUMNS primarykeys[] = {
	{ "TABLE_CAT", 0, SQL_NULLABLE, "(C31)", 0 },
	{ "TABLE_SCHEM", 0, SQL_NULLABLE, "(C31)", 0 },
	{ "TABLE_NAME", 0, SQL_NO_NULLS, "(C31)", SERVER_V3 },
	{ "COLUMN_NAME", 0, SQL_NO_NULLS, "(C31)", SERVER_V3 },
	{ "KEY_SEQ", SQL_C_SHORT, SQL_NO_NULLS, "(N4)", SERVER_V3 },
	{ "PK_NAME", 0, SQL_NULLABLE, "(C31)", 0 }
};

static COLUMNS specialcolumns[] = {
	{ "SCOPE", SQL_C_SHORT, SQL_NULLABLE, "(N4)", SERVER_V2 },
	{ "COLUMN_NAME", 0, SQL_NO_NULLS, "(C31)", SERVER_V2 | SERVER_V3 },
/*** NOTE: V2 gets (N4), V3 gets (C1), descriptor record should be (N4) ***/
	{ "DATA_TYPE", SQL_C_SHORT, SQL_NO_NULLS, "(C1)", SERVER_V2 | SERVER_V3 },
	{ "TYPE_NAME", 0, SQL_NO_NULLS, "(C9)", SERVER_V2 },
	{ "COLUMN_SIZE", SQL_C_LONG, SQL_NULLABLE, "(N6)", SERVER_V2 | SERVER_V3 },
	{ "BUFFER_LENGTH", SQL_C_LONG, SQL_NULLABLE, "(N6)", SERVER_V2 },
	{ "DECIMAL_DIGITS", SQL_C_SHORT, SQL_NULLABLE, "(N4)", SERVER_V2 | SERVER_V3 },
	{ "PSEUDO_COLUMN", SQL_C_SHORT, SQL_NULLABLE, "(N4)", SERVER_V2 }
};

static COLUMNS statistics[] = {
/*** CODE: THESE NEED TO BE RESEARCHED AND DETERMINE WHICH VALUES NEED TO COME  ***/
/***       BACK FROM V3.  ALSO, MISSING SOME TYPES, LOOK AT WHAT V2 RETURNS ***/
	{ "TABLE_CAT", 0, SQL_NULLABLE, "(C31)", SERVER_V2 },
	{ "TABLE_SCHEM", 0, SQL_NULLABLE, "(C31)", SERVER_V2 },
	{ "TABLE_NAME", 0, SQL_NO_NULLS, "(C31)", SERVER_V2 | SERVER_V3 },
	{ "NON_UNIQUE", SQL_C_SHORT, SQL_NULLABLE, "(N1)", SERVER_V2 | SERVER_V3 },
	{ "INDEX_QUALIFIER", 0, SQL_NULLABLE, "(C31)", SERVER_V2 },
	{ "INDEX_NAME", 0, SQL_NULLABLE, "(C256)", SERVER_V2 | SERVER_V3 },
/*** NOTE: V2 gets (N1), V3 gets (C1), descriptor record should be (N4) ***/
	{ "TYPE", SQL_C_SHORT, SQL_NO_NULLS, "(C1)", SERVER_V2 | SERVER_V3 },
	{ "ORDINAL_POSITION", SQL_C_SHORT, SQL_NULLABLE, "(N4)", SERVER_V2 | SERVER_V3 },
	{ "COLUMN_NAME", 0, SQL_NULLABLE, "(C31)", SERVER_V2 | SERVER_V3 },
	{ "ASC_OR_DESC", 0, SQL_NULLABLE, "(C1)", SERVER_V2 | SERVER_V3 },
	{ "CARDINALITY", SQL_C_LONG, SQL_NULLABLE, "(N10)", SERVER_V2 },
	{ "PAGES", SQL_C_LONG, SQL_NULLABLE, "(N10)", SERVER_V2 },
	{ "FILTER_CONDITION", 0, SQL_NULLABLE, "(C63)", SERVER_V2 }
};

static COLUMNS tables[] = {
	{ "TABLE_CAT", 0, SQL_NULLABLE, "(C31)", SERVER_V2 },
	{ "TABLE_SCHEM", 0, SQL_NULLABLE, "(C31)", SERVER_V2 },
	{ "TABLE_NAME", 0, SQL_NULLABLE, "(C31)", SERVER_V2 | SERVER_V3 },
/*** CODE: MAY WANT V3 TO ALWAYS ASSUME 'TABLE' ***/
	{ "TABLE_TYPE", 0, SQL_NULLABLE, "(C31)", SERVER_V2 },
	{ "REMARKS", 0, SQL_NULLABLE, "(C63)", SERVER_V2 | SERVER_V3 }
};

static SQLRETURN fixupexec(STATEMENT *, ERRORS *, UCHAR **, INT *, INT);
static int nextword(char *, int, int *, int *);
static int findword(char *, int, char *, int, int *);
static void asctocnum(char *, int, int, SQL_NUMERIC_STRUCT *);
static void cnumtoasc(SQL_NUMERIC_STRUCT *num, int precision, int scale, char *str);
static INT char_to_int32(CHAR *str, INT len);
static INT cvttodatetime(CHAR *src, INT srclen, INT desttype, CHAR *dest);

/*
 * Called from SQLExecDirect and SQLPrepare
 * Calls allocmem/reallocmem and needs thread protection
 */
SQLRETURN exec_prepare(STATEMENT *stmt, SQLCHAR *text, SQLINTEGER length)
{
	INT i1, size;
	CHAR *ptr;

	/*** CODE: MAYBE VERIFY THERE ARE MATCHING '\'' AND GIVE 42000 IF NOT (THIS IS BEING DONE IN EXECUTE()) ***/
	while (length > 0 && isspace(text[length - 1])) length--;
	if (length >= stmt->textsize) {
		size = length + 0x0100;
		if (size & 0x01FF) size += 0x200 - (size & 0x1FF);
		if (!stmt->textsize) {
			stmt->text = (UCHAR *) allocmem(size * sizeof(CHAR), 0);
			if (stmt->text == NULL) {
				error_record(&stmt->errors, "HY001", TEXT_HY001, NULL, 0);
				return SQL_ERROR;
			}
		}
		else {
			ptr = reallocmem(stmt->text, size * sizeof(CHAR), 0);
			if (ptr == NULL) {
				error_record(&stmt->errors, "HY001", TEXT_HY001, NULL, 0);
				return SQL_ERROR;
			}
			stmt->text = (UCHAR *)ptr;
		}
		stmt->textsize = size;
	}
	memcpy(stmt->text, text, length);
	stmt->textlength = length;
	text = stmt->text;
	for (i1 = 0; i1 < length; i1++) {
		if (text[i1] == '\'') {
			i1++;
			if (i1 < length && text[i1] != '\'') {  /* not null string */
				do {  /* find matching quote */
					if (text[i1] == '\'') {
						if (i1 + 1 == length || text[i1 + 1] != '\'') break;
						text[i1++] = '\\';
					}
					else if (text[i1] == '\\') {
						if (++i1 == length) break;
					}
				} while (++i1 < length);
			}
			if (i1 == length) {  /* end of statement - error */
				error_record(&stmt->errors, "42000", "Syntax error: no closing apostrophe", NULL, 0);
				return SQL_ERROR;
			}
		}
	}
	return SQL_SUCCESS;
}

/**
 * Called from fsosql; SQLExecDirect, SQLExecute, SQLParamData
 * This function calls entersync and its invocation should not be inside a critical section
 */
SQLRETURN exec_execute(STATEMENT *stmt)
{
	INT i1, i2, i3, i4, len, reslen, savepos, type;
	SQLRETURN rc, rc2;
	CHAR errmsg[512], *cmdptr, *rsidptr;
	UCHAR *text;
	STATEMENT *stmt2;
	APP_DESC_RECORD *apd_rec;
	IMP_PARAM_DESC_RECORD *ipd_rec;

	entersync();  /* protect workbuffer */
	if (stmt->sql_attr_app_param_desc->sql_desc_count) {  /* check for data at exec */
		i2 = stmt->sql_attr_app_param_desc->sql_desc_count;
		i3 = stmt->sql_attr_app_param_desc->dataatexecrecnum;
		for (apd_rec = stmt->sql_attr_app_param_desc->firstrecord.appdrec, i1 = 0; ++i1 <= i2; apd_rec = apd_rec->nextrecord) {
			if ((stmt->executeflags & EXECUTE_NEEDDATA) && i1 <= i3) continue;
			if (apd_rec->sql_desc_octet_length_ptr == NULL || *apd_rec->sql_desc_octet_length_ptr >= 0 ||
				*apd_rec->sql_desc_octet_length_ptr == SQL_NTS || *apd_rec->sql_desc_octet_length_ptr == SQL_NULL_DATA) continue;
			if (*apd_rec->sql_desc_octet_length_ptr == SQL_DEFAULT_PARAM) {
				sprintf(errmsg, "%s, bound parameter uses SQL_DEFAULT_PARAM, but procedures are not supported", TEXT_07S01);
				error_record(&stmt->errors, "07S01", errmsg, NULL, 0);
				return SQL_ERROR;
			}
			if (*apd_rec->sql_desc_octet_length_ptr != SQL_DATA_AT_EXEC && *apd_rec->sql_desc_octet_length_ptr > SQL_LEN_DATA_AT_EXEC_OFFSET) {
				sprintf(errmsg, "%s, bound parameter length = %li is less than 0", TEXT_HY090,
						(long int)*apd_rec->sql_desc_octet_length_ptr);
				error_record(&stmt->errors, "HY090", errmsg, NULL, 0);
				return SQL_ERROR;
			}
			if (stmt->executeflags & EXECUTE_NEEDDATA) {
				if (apd_rec->dataatexecptr == NULL) {
					type = apd_rec->sql_desc_concise_type;
					if (type == SQL_C_CHAR || type == SQL_C_DEFAULT) {
						i2 = 256;
						if (i1 <= stmt->sql_attr_app_param_desc->sql_desc_count) {
							ipd_rec = (IMP_PARAM_DESC_RECORD *) desc_get(stmt->sql_attr_imp_param_desc, i1, FALSE);
							if (type == SQL_C_DEFAULT) {
								if (ipd_rec->sql_desc_concise_type == SQL_TYPE_DATE) type = SQL_C_TYPE_DATE;
								else if (ipd_rec->sql_desc_concise_type == SQL_TYPE_TIME) type = SQL_C_TYPE_TIME;
								else if (ipd_rec->sql_desc_concise_type == SQL_TYPE_TIMESTAMP) type = SQL_C_TYPE_TIMESTAMP;
								else type = SQL_C_CHAR;
							}
							if (type == SQL_C_CHAR) {
								type = ipd_rec->sql_desc_concise_type;
								if (type == SQL_CHAR || type == SQL_VARCHAR || type == SQL_LONGVARCHAR ||
									type == SQL_BINARY || type == SQL_VARBINARY || type == SQL_LONGVARBINARY)
									i2 = ipd_rec->sql_desc_length;
							}
						}
					}
					if (type != SQL_C_CHAR) {
					    if (type == SQL_C_LONG || type == SQL_C_SLONG || type == SQL_C_ULONG) i2 = sizeof(SQLINTEGER);
						else if (type == SQL_C_SHORT || type == SQL_C_SSHORT || type == SQL_C_USHORT) i2 = sizeof(SQLSMALLINT);
						else if (type == SQL_C_TINYINT || type == SQL_C_STINYINT || type == SQL_C_UTINYINT) i2 = sizeof(SQLSCHAR);
					    else if (type == SQL_C_DOUBLE) i2 = sizeof(SQLDOUBLE);
						else if (type == SQL_C_FLOAT) i2 = sizeof(SQLDOUBLE);
						else if (type == SQL_C_NUMERIC) i2 = sizeof(SQL_NUMERIC_STRUCT);
						else if (type == SQL_C_TYPE_DATE) i2 = sizeof(SQL_DATE_STRUCT);
						else if (type == SQL_C_TYPE_TIME) i2 = sizeof(SQL_TIME_STRUCT);
						else if (type == SQL_C_TYPE_TIMESTAMP) i2 = sizeof(SQL_TIMESTAMP_STRUCT);
						else {
							sprintf(errmsg, "%s: Binding parameters: Unexpected C data type: %d", TEXT_HY003, (int) apd_rec->sql_desc_concise_type);
							error_record(&stmt->errors, "HY003", errmsg, NULL, 0);
							return SQL_ERROR;
						}
					}
					else i2 *= sizeof(CHAR);
					apd_rec->dataatexecptr = (UCHAR *) allocmem(i2, 0);
					if (apd_rec->dataatexecptr == NULL) {
						error_record(&stmt->errors, "HY001", TEXT_HY001, NULL, 0);
						return SQL_ERROR;
					}
					apd_rec->dataatexectype = type;
					apd_rec->dataatexecsize = i2;
				}
				apd_rec->dataatexeclength = 0;
			}
			else i1--;
			stmt->sql_attr_app_param_desc->dataatexecrecnum = i1;
			stmt->executeflags |= EXECUTE_NEEDDATA;
			return SQL_NEED_DATA;
		}
	}
	stmt->executeflags &= ~EXECUTE_NEEDDATA;

#if OS_WIN32
	__try { // @suppress("Statement has no effect") // @suppress("Symbol is not resolved")
#endif
	text = stmt->text;
	len = stmt->textlength;
	if (fixupexec(stmt, &stmt->errors, &text, &len, TRUE) == SQL_ERROR) {
		exitsync();
		return SQL_ERROR;
	}

	stmt2 = stmt;
	cmdptr = EXECUTE;
	rsidptr = EMPTY;
	nextword((char*)text, len, &i1, &i2);
#ifndef HPUX11
#pragma warning(disable : 4996)
#endif
	if (i2 == 4 && !_memicmp((const void *)&text[i1], "DROP", 4)) type = TYPE_DROP;
	else if (i2 == 5 && !_memicmp((const void *)&text[i1], "ALTER", 5)) type = TYPE_ALTER;
	else if (i2 == 6) {
		if (!_memicmp((const void *)&text[i1], "UPDATE", 6)) {
			type = TYPE_UPDATE;
#if 0
			tablepos = i1 + 6;
#endif
		}
		else if (!_memicmp((const void *)&text[i1], "DELETE", 6)) type = TYPE_DELETE;
		else if (!_memicmp((const void *)&text[i1], "CREATE", 6)) type = TYPE_CREATE;
		else type = TYPE_OTHER;
		if (type == TYPE_UPDATE || type == TYPE_DELETE) {  /* check for "where current of" */
			i3 = i1 + 6;
			if (findword("WHERE", 5, (char*)&text[i3], len - i3, &i2)) {
				savepos = i3 + i2;
				i3 += i2 + 5;
				nextword((char*)&text[i3], len - i3, &i1, &i2);
				if (i2 == 7 && !_memicmp((const void *)&text[i3 + i1], "CURRENT", 7)) {
					i3 += i1 + 7;
					nextword((char*)&text[i3], len - i3, &i1, &i2);
					if (i2 != 2 || _memicmp((const void *)&text[i3 + i1], "OF", 2)) {
						error_record(&stmt->errors, "42000", "Syntax error: missing keyword 'OF'", NULL, 0);
						exitsync();
						return SQL_ERROR;
					}
					i3 += i1 + 2;
					nextword((char*)&text[i3], len - i3, &i1, &i2);
					if (!i2) {
						error_record(&stmt->errors, "42000", "Syntax error: missing cursor-name", NULL, 0);
						exitsync();
						return SQL_ERROR;
					}
					i3 += i1;
					/* entersync already active */
					for (stmt2 = stmt->cnct->firststmt; stmt2 != NULL; stmt2 = stmt2->nextstmt) {
						if (stmt2 == stmt) continue;
						i4 = (INT)strlen(stmt2->cursor_name);
						if (i4 == i2 && !_memicmp(stmt2->cursor_name, (const void *)(text + i3), i4)) break;
					}
					if (stmt2 == NULL) {
/*** CODE: LOOKUP CURSOR NOT FOUND ***/
						error_record(&stmt->errors, "42000", "Syntax error: no matching cursor-name", NULL, 0);
						exitsync();
						return SQL_ERROR;
					}
					if (!(stmt2->executeflags & EXECUTE_RESULTSET)) {
/*** CODE: LOOKUP SELECTED CURSOR NOT CURRENT ***/
						error_record(&stmt->errors, "42000", "Syntax error: no current record", NULL, 0);
						exitsync();
						return SQL_ERROR;
					}
					if (type == TYPE_UPDATE) {
/*** CODE: MAYBE DO A MEMMOVE INSTEAD OF A TRUNCATION, BUT I DON'T EXPECT ANYTHING TO FOLLOW ***/
						len = savepos;
#if 0
						nextword(&text[tablepos], len - tablepos, &i1, &i2);
						if (!i2) {
							error_record(&stmt->errors, "42000", "Syntax error: missing table name", NULL, 0);
							exitsync();
							return SQL_ERROR;
						}
						tablepos += i1 + i2;
						nextword(&text[tablepos], len - tablepos, &i1, &i2);
						if (i2 != 3 || _memicmp(&text[tablepos + i1], "SET", 3)) {
							error_record(&stmt->errors, "42000", "Syntax error: missing keyword 'SET' after single table name", NULL, 0);
							exitsync();
							return SQL_ERROR;
						}
						text += tablepos + i1 + 3;
						len -= tablepos + i1 + 3;
#endif
						cmdptr = PSUPDATE;
					}
					else {
/*** CODE: MAYBE CHECK FOR 'FOR TABLE-NAME' ***/
						len = 0;
						cmdptr = PSDELETE;
					}
					rsidptr = stmt2->rsid;
				}
			}
		}
	}

	rc2 = SQL_SUCCESS;
	reslen = sizeof(workbuffer);
	rc = server_communicate(stmt2->cnct, (UCHAR*)rsidptr, (UCHAR*)cmdptr,
		text, len, &stmt->errors, (UCHAR*)workbuffer, &reslen);
	switch(rc) {
	case SERVER_OKTRUNC:
		error_record(&stmt->errors, "01004", TEXT_01004, "Numeric or character truncation", 0);
		rc2 = SQL_SUCCESS_WITH_INFO;
		//no break here
	case SERVER_OK:
		if (type == TYPE_ALTER || type == TYPE_CREATE || type == TYPE_DROP) break;
		if (reslen) i1 = char_to_int32(workbuffer, reslen);  /* update count was returned */
/*** CODE: IF STMT != STMT2; THEN SHOULD THIS BE SET TO 1 (SEE DOCS) ***/
		else i1 = 0;
		stmt->errors.rowcount = stmt->row_count = i1;
		if (!i1 && stmt->cnct->env->sql_attr_odbc_version != SQL_OV_ODBC2) rc2 = SQL_NO_DATA;
		break;
	case SERVER_NONE:
	case SERVER_SET:
		rc2 = exec_resultset(stmt, &stmt->errors, workbuffer, reslen, 0);
		if (rc2 == SQL_ERROR) {
			if (stmt->executeflags & EXECUTE_RSID) {
				stmt->executeflags &= ~EXECUTE_RSID;
				server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD, NULL, 0, NULL, NULL, NULL);
			}
		}
		break;
	default:  /* SERVER_FAIL or error */
		rc2 = SQL_ERROR;
	}
#if OS_WIN32
	}
	__except( EXCEPTION_EXECUTE_HANDLER ) {
		int i1 = GetExceptionCode();
		exitsync();
		error_record(&stmt->errors, "HY000", TEXT_HY000, "In exec_execute", i1);
		return SQL_ERROR;
	}
#endif
	exitsync();  /* done with workbuffer */
	if (rc2 != SQL_ERROR) stmt->executeflags |= EXECUTE_EXECUTED;
	return rc2;
}

/**
 * Protects itself with enter/exit sync
 */
SQLRETURN exec_columninfo(STATEMENT *stmt, ERRORS *errors)
{
	INT len, reslen;
	SQLRETURN rc, rc2;
	CHAR *ptr;
	UCHAR *text;

	entersync();  /* protect workbuffer */
#if OS_WIN32
	__try {
#endif
	text = stmt->text;
	len = stmt->textlength;
	if (fixupexec(stmt, errors, &text, &len, FALSE) == SQL_ERROR) {
		exitsync();
		return SQL_ERROR;
	}
	if (stmt->cnct->server_majorver == 2) ptr = GETRS;
	else ptr = EXECNONE;
	reslen = sizeof(workbuffer);
	rc = server_communicate(stmt->cnct, (UCHAR*)EMPTY, (UCHAR*)ptr,
		text, len, errors, (UCHAR*)workbuffer, &reslen);
	if (rc == SERVER_OK || rc == SERVER_SET) {
		rc = exec_resultset(stmt, errors, workbuffer, reslen, 0);
		if (stmt->executeflags & EXECUTE_RSID) {
			stmt->executeflags &= ~EXECUTE_RSID;
			rc2 = server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD, NULL, 0, NULL, NULL, NULL);
		}
		if (rc == SQL_ERROR) stmt->executeflags &= EXECUTE_PREPARED;
	}
	else rc = SQL_ERROR;
#if OS_WIN32
	}
	__except( EXCEPTION_EXECUTE_HANDLER ) {
		int i1 = GetExceptionCode();
		exitsync();
		error_record(&stmt->errors, "HY000", TEXT_HY000, "In exec_columninfo", i1);
		return SQL_ERROR;
	}
#endif
	exitsync();  /* done with workbuffer */
	return rc;
}

SQLRETURN exec_resultset(STATEMENT *stmt, ERRORS *errors, CHAR *buffer, INT len, INT catalogtype)
{
	INT i1, i2, i3, i4, nrows, rowinc, rowoffset, savei1, savelen, version;
	SQLUSMALLINT ncols, updatable;
	CHAR ctype, scaleseperator, basetable[MAX_NAME_LENGTH + 1], table[MAX_NAME_LENGTH + 1], *ptr;
	DESCRIPTOR *ird;
	IMP_ROW_DESC_RECORD *ird_rec;
	COLUMNS *colinfo;
#ifdef __MACOSX
	char * errmsg;
#else
	CHAR errmsg[MAX_ERROR_MSG];
#endif

	/* parse result set and populate ird records */
	/* parse rsid */
	for (i1 = 0; i1 < len && buffer[i1] == ' '; i1++);
	if (i1 + 4 <= len && !memcmp(buffer + i1, "NONE", 4)) i1 += 4;
	else {
		for (i2 = i1; i1 < len && isdigit(buffer[i1]); i1++);
		memset(stmt->rsid, ' ', FIELD_LENGTH - (i1 - i2));
		memcpy(&stmt->rsid[FIELD_LENGTH - (i1 - i2)], &buffer[i2], i1 - i2);
		if (i1 != i2) stmt->executeflags |= EXECUTE_RSID;
	}

	/* parse updatable */
	if (catalogtype) updatable = SQL_ATTR_READONLY;
	else updatable = SQL_ATTR_WRITE;  /* go ahead and assume WRITE if FS 2 or old FS 3 */
	while (i1 < len && buffer[i1] == ' ') i1++;
	if (i1 < len && (buffer[i1] == 'W' || buffer[i1] == 'R' || buffer[i1] == 'U')) {
		if (buffer[i1] == 'W') updatable = SQL_ATTR_WRITE;
		else if (buffer[i1] == 'R') updatable = SQL_ATTR_READONLY;
		else updatable = SQL_ATTR_READWRITE_UNKNOWN;
		i1++;
	}

	/* parse numrows */
	while (i1 < len && buffer[i1] == ' ') i1++;
	if (i1 < len && buffer[i1] == '*') {
		i1++;
		nrows = -1;  /* unknown number of rows */
		stmt->executeflags |= EXECUTE_DYNAMIC;
	}
	else for (nrows = 0; i1 < len && isdigit(buffer[i1]); i1++)
		nrows = nrows * 10 + buffer[i1] - '0';
/*** CODE: RESULTROWS IS NOT BEING USED, BUT IT IS INTENDED TO BE USED TO ***/
/***       PREVENT UNECESSARY COMMUNICATIONS TO SERVER, IE. DON'T FETCH WHEN ***/
/***       IT IS KNOWN THAT NEXT ROW DOES NOT EXIST ***/
	stmt->row_count = nrows;
	stmt->lastcolumn = -1;
	stmt->sql_attr_row_number = stmt->row_number = 0;

	/* parse numcols */
	while (i1 < len && buffer[i1] == ' ') i1++;
	for (ncols = 0; i1 < len && isdigit(buffer[i1]); i1++)
		ncols = ncols * 10 + buffer[i1] - '0';
	if (catalogtype) {
		switch (catalogtype) {
		case EXECUTE_SQLCOLUMNS:
			colinfo = columns;
			ncols = sizeof(columns) / sizeof(*columns);
			break;
		case EXECUTE_SQLGETTYPEINFO:
			colinfo = gettypeinfo;
			ncols = sizeof(gettypeinfo) / sizeof(*gettypeinfo);
			break;
		case EXECUTE_SQLPRIMARYKEYS:
			colinfo = primarykeys;
			ncols = sizeof(primarykeys) / sizeof(*primarykeys);
			break;
		case EXECUTE_SQLSPECIALCOLUMNS:
			colinfo = specialcolumns;
			ncols = sizeof(specialcolumns) / sizeof(*specialcolumns);
			break;
		case EXECUTE_SQLSTATISTICS:
			colinfo = statistics;
			ncols = sizeof(statistics) / sizeof(*statistics);
			break;
		case EXECUTE_SQLTABLES:
			colinfo = tables;
			ncols = sizeof(tables) / sizeof(*tables);
			break;
		default:
			error_record(errors, "HY000", TEXT_HY000, "unrecognized execute type", 0);
			return SQL_ERROR;
		}
	}
	if (stmt->cnct->server_majorver == 2) {
		version = SERVER_V2;
		scaleseperator = ',';
	}
	else {
		version = SERVER_V3;
		scaleseperator = ':';
	}

	/* allocate ird records */
	ird = stmt->sql_attr_imp_row_desc;
	if (ncols > ird->sql_desc_count && desc_get(ird, ncols, TRUE) == NULL) {
		error_record(errors, "HY001", TEXT_HY001, NULL, 0);
		return SQL_ERROR;
	}
	ird->sql_desc_count = ncols;

	/* parse columninfo */
	basetable[0] = table[0] = '\0';
	rowoffset = 0;
	for (ird_rec = ird->firstrecord.irdrec, i2 = 0; i2 < ncols; ird_rec = ird_rec->nextrecord, i2++) {
		ird_rec->default_type = 0;
		ird_rec->rowbufoffset = rowoffset;
		ptr = buffer;

		/* get column name */
		while (i1 < len && ptr[i1] == ' ') i1++;
		if (catalogtype) {
			strcpy((char*)ird_rec->sql_desc_base_column_name, colinfo[i2].name);
			ird_rec->sql_desc_unnamed = SQL_NAMED;
			ird_rec->sql_desc_searchable = SQL_PRED_NONE;
			ird_rec->default_type = colinfo[i2].type;
			if (!(colinfo[i2].versions & version)) {  /* not in result set from server */
				ptr = colinfo[i2].colinfo;
				savelen = len;
				len = (INT)strlen(ptr);
				savei1 = i1;
				i1 = 0;
			}
		}
		else {
			for (i3 = i1; i1 < len && ptr[i1] != '('; i1++);
			memcpy(ird_rec->sql_desc_base_column_name, ptr + i3, min(i1 - i3, MAX_NAME_LENGTH));
			ird_rec->sql_desc_base_column_name[min(i1 - i3, MAX_NAME_LENGTH)] = '\0';
			if (i1 != i3) ird_rec->sql_desc_unnamed = SQL_NAMED;
			else ird_rec->sql_desc_unnamed = SQL_UNNAMED;
			ird_rec->sql_desc_searchable = SQL_PRED_SEARCHABLE;
		}
		strcpy((char*)ird_rec->sql_desc_name, (char*)ird_rec->sql_desc_base_column_name);  /* alias */
		strcpy((char*)ird_rec->sql_desc_label, (char*)ird_rec->sql_desc_base_column_name);  /* title */
		ird_rec->sql_desc_auto_unique_value = SQL_FALSE;
		ird_rec->sql_desc_rowver = SQL_FALSE;
		ird_rec->sql_desc_updatable = updatable;

		/* get column type and size */
		rowinc = 0;
		if (i1 < len && ptr[i1] == '(') {
			/* parse escapes as labels can have ')' */
			for (i3 = ++i1; i1 < len && (ptr[i1] != ')' || ptr[i1 - 1] == '\\'); i1++);
			if (i3 < i1) {
				ctype = ptr[i3++];
				switch (ctype) {
				case 'C':
					for (i4 = 0; i3 < i1 && isdigit(ptr[i3]); i3++)
						i4 = i4 * 10 + ptr[i3] - '0';
					rowinc = i4;
					/* fixup of FS 3 incompatablity issue */
					if ((catalogtype == EXECUTE_SQLCOLUMNS && i2 == 4) ||
						(catalogtype == EXECUTE_SQLSPECIALCOLUMNS && i2 == 2) ||
						(catalogtype == EXECUTE_SQLSTATISTICS && i2 == 6)) {
						ird_rec->sql_desc_concise_type = ird_rec->sql_desc_type = (colinfo[i2].type == SQL_C_SHORT) ? SQL_SMALLINT : SQL_NUMERIC;
						ird_rec->sql_desc_precision = 4;
						ird_rec->sql_desc_display_size = 4;
						ird_rec->sql_desc_num_prec_radix = 10;
						ird_rec->sql_desc_case_sensitive = SQL_FALSE;
						ird_rec->sql_desc_literal_prefix[0] = ird_rec->sql_desc_literal_suffix[0] = '\0';
						ird_rec->sql_desc_local_type_name = ird_rec->sql_desc_type_name = (SQLCHAR*)"NUMERIC";
						ird_rec->sql_desc_nullable = SQL_NULLABLE;
						ird_rec->sql_desc_unsigned = SQL_FALSE;
					}
					else {
						ird_rec->sql_desc_concise_type = ird_rec->sql_desc_type = SQL_CHAR;
						ird_rec->sql_desc_display_size = ird_rec->sql_desc_length = ird_rec->sql_desc_octet_length = i4;
						ird_rec->sql_desc_case_sensitive = SQL_TRUE;
						ird_rec->sql_desc_literal_prefix[0] = ird_rec->sql_desc_literal_suffix[0] = '\'';
						ird_rec->sql_desc_local_type_name = ird_rec->sql_desc_type_name = (SQLCHAR*)"CHAR";
						ird_rec->sql_desc_nullable = SQL_NO_NULLS;
						ird_rec->sql_desc_unsigned = SQL_TRUE;
					}
					ird_rec->sql_desc_datetime_interval_code = 0;
					break;
				case 'N':
				case 'U':  /*** NOTE: FOR FUTURE SUPPORT ***/
					for (i4 = 0; i3 < i1 && isdigit(ptr[i3]); i3++)
						i4 = i4 * 10 + ptr[i3] - '0';
					rowinc = i4;
					/* fixup of FS 2 to more reasonable length */
					if (catalogtype == EXECUTE_SQLSTATISTICS && i2 == 6) i4 = 4;
					if (catalogtype && colinfo[i2].type == SQL_C_SHORT) {
						ird_rec->sql_desc_concise_type = ird_rec->sql_desc_type = SQL_SMALLINT;
					}
					else {
						ird_rec->sql_desc_concise_type = ird_rec->sql_desc_type = SQL_NUMERIC;
					}
					ird_rec->sql_desc_datetime_interval_code = 0;
					ird_rec->sql_desc_precision = i4;
					ird_rec->sql_desc_display_size = i4;
					ird_rec->sql_desc_num_prec_radix = 10;
					if (i3 + 1 < i1 && ptr[i3] == scaleseperator) {
						for (i3++, i4 = 0; i3 < i1 && isdigit(ptr[i3]); i3++)
							i4 = i4 * 10 + ptr[i3] - '0';
						if (i4) {
							rowinc++;
							ird_rec->sql_desc_display_size++;
							ird_rec->sql_desc_scale = i4;
							ird_rec->sql_desc_fixed_prec_scale = SQL_TRUE;
						}
					}
					ird_rec->sql_desc_case_sensitive = SQL_FALSE;
					ird_rec->sql_desc_literal_prefix[0] = ird_rec->sql_desc_literal_suffix[0] = '\0';
					ird_rec->sql_desc_local_type_name = ird_rec->sql_desc_type_name = (SQLCHAR*)"NUMERIC";
					ird_rec->sql_desc_nullable = SQL_NULLABLE;
					if (ctype == 'N') ird_rec->sql_desc_unsigned = SQL_FALSE;
					else ird_rec->sql_desc_unsigned = SQL_TRUE;
					break;
				case 'D':
					rowinc = 10;
					ird_rec->sql_desc_type = SQL_DATETIME;
					ird_rec->sql_desc_concise_type = SQL_TYPE_DATE;
					ird_rec->sql_desc_datetime_interval_code = SQL_CODE_DATE;
					ird_rec->sql_desc_display_size = ird_rec->sql_desc_length = rowinc;
					ird_rec->sql_desc_octet_length = sizeof(SQL_DATE_STRUCT);
					ird_rec->sql_desc_case_sensitive = SQL_FALSE;
					ird_rec->sql_desc_literal_prefix[0] = ird_rec->sql_desc_literal_suffix[0] = '\'';
					ird_rec->sql_desc_local_type_name = ird_rec->sql_desc_type_name = (SQLCHAR*)"DATE";
					ird_rec->sql_desc_nullable = SQL_NULLABLE;
					ird_rec->sql_desc_unsigned = SQL_TRUE;
					break;
				case 'T':
					rowinc = 12;
					ird_rec->sql_desc_type = SQL_DATETIME;
					ird_rec->sql_desc_concise_type = SQL_TYPE_TIME;
					ird_rec->sql_desc_datetime_interval_code = SQL_CODE_TIME;
					ird_rec->sql_desc_display_size = ird_rec->sql_desc_length = rowinc;	/* hh:mm:ss.ppp */
					ird_rec->sql_desc_octet_length = sizeof(SQL_TIME_STRUCT);
					ird_rec->sql_desc_precision = 3;
					ird_rec->sql_desc_case_sensitive = SQL_FALSE;
					ird_rec->sql_desc_literal_prefix[0] = ird_rec->sql_desc_literal_suffix[0] = '\'';
					ird_rec->sql_desc_local_type_name = ird_rec->sql_desc_type_name = (SQLCHAR*)"TIME";
					ird_rec->sql_desc_nullable = SQL_NULLABLE;
					ird_rec->sql_desc_unsigned = SQL_TRUE;
					break;
				case 'S':
					rowinc = TIMESTAMP_LENGTH;
					ird_rec->sql_desc_type = SQL_DATETIME;
					ird_rec->sql_desc_concise_type = SQL_TYPE_TIMESTAMP;
					ird_rec->sql_desc_datetime_interval_code = SQL_CODE_TIMESTAMP;
					ird_rec->sql_desc_display_size = ird_rec->sql_desc_length = rowinc;
					ird_rec->sql_desc_octet_length = sizeof(SQL_TIMESTAMP_STRUCT);
					ird_rec->sql_desc_precision = 3;
					ird_rec->sql_desc_case_sensitive = SQL_FALSE;
					ird_rec->sql_desc_literal_prefix[0] = ird_rec->sql_desc_literal_suffix[0] = '\'';
					ird_rec->sql_desc_local_type_name = ird_rec->sql_desc_type_name = (SQLCHAR*)"TIMESTAMP";
					ird_rec->sql_desc_nullable = SQL_NULLABLE;
					ird_rec->sql_desc_unsigned = SQL_TRUE;
					break;
				default:
					snprintf(errmsg, MAX_ERROR_MSG, "unrecognized column type returned from server (0x%02X)", (int) ctype);
					error_record(errors, "HY000", TEXT_HY000, errmsg, 0);
					return SQL_ERROR;
				}
			}
			if (version != SERVER_V2) {
				if (i3 < i1 && ptr[i3] == ',') {  /* get column alias */
					for (i4 = ++i3; i3 < i1 && ptr[i3] != ','; i3++);
					if (i3 > i4) {
						memcpy(ird_rec->sql_desc_name, ptr + i4, min(i3 - i4, MAX_NAME_LENGTH));
						ird_rec->sql_desc_name[min(i3 - i4, MAX_NAME_LENGTH)] = '\0';
						strcpy((char*)ird_rec->sql_desc_label, (const char*)ird_rec->sql_desc_name);  /* title */
						ird_rec->sql_desc_unnamed = SQL_NAMED;
					}
				}
				if (i3 < i1 && ptr[i3] == ',') {  /* get column label */
					/* parse escapes as labels can have ',' */
					for (i4 = 0, ++i3; i3 < i1 && ptr[i3] != ','; i3++) {
						if (ptr[i3] == '\\' && i3 + 1 < i1) i3++;
						if (i4 < MAX_NAME_LENGTH) ird_rec->sql_desc_label[i4++] = ptr[i3];
					}
					if (i4) ird_rec->sql_desc_label[i4] = '\0';
				}
				if (i3 < i1 && ptr[i3] == ',') {  /* get table name */
					for (i4 = ++i3; i3 < i1 && ptr[i3] != ','; i3++);
					if (i3 > i4) {
						if (i3 > i4 + 1 || ptr[i4] != '*') {
							memcpy(basetable, ptr + i4, min(i3 - i4, MAX_NAME_LENGTH));
							basetable[min(i3 - i4, MAX_NAME_LENGTH)] = '\0';
						}
						else basetable[0] = '\0';
					}
				}
				if (basetable[0]) {
					strcpy((char*)ird_rec->sql_desc_base_table_name, basetable);
					strcpy((char*)ird_rec->sql_desc_table_name, basetable);
				}
				if (i3 < i1 && ptr[i3] == ',') {  /* get table alias */
/*** CODE: NOT SURE WHAT THE DIFFERENCE BETWEEN BASE_TABLE_NAME AND TABLE_NAME ***/
					for (i4 = ++i3; i3 < i1 && ptr[i3] != ','; i3++);
					if (i3 > i4) {
						if (i3 > i4 + 1 || ptr[i4] != '*') {
							memcpy(table, ptr + i4, min(i3 - i4, MAX_NAME_LENGTH));
							table[min(i3 - i4, MAX_NAME_LENGTH)] = '\0';
						}
						else table[0] = '\0';
					}
				}
				if (table[0]) strcpy((char*)ird_rec->sql_desc_table_name, table);
			}
			if (i1 < len) i1++;
		}
		if (catalogtype && !(colinfo[i2].versions & version)) {  /* restore values */
			len = savelen;
			i1 = savei1;
			rowinc = 0;
		}
		else rowoffset += rowinc;
		if (catalogtype) {
			ird_rec->sql_desc_nullable = colinfo[i2].nullable;
			if (!(colinfo[i2].versions & SERVER_V3)) rowinc = 0;  /* substitute in our own value */
		}
		ird_rec->fieldlength = rowinc;
	}

	stmt->executeflags |= EXECUTE_RESULTSET | catalogtype;
	return SQL_SUCCESS;
}


/**
 * Called only from SQLGetData in fsosql where it is protected by entersync
 */
SQLRETURN exec_sqltoc(
	STATEMENT *stmt,
	IMP_ROW_DESC_RECORD *ird_rec,
	SQLSMALLINT column,
	CHAR *sqlbuffer,
	size_t sqllen,
	INT type,
	INT sqlprecision,
	INT sqlscale,
	SQLPOINTER cbuffer,
	SQLLEN cbuflen,   /* <- This can be zero!! */
	SQLLEN *cdatalen,
	SQLLEN *cnullind,
	SQLUINTEGER coffset,
	SQLUINTEGER crownum,
	SQLUINTEGER crowsize)
{
	INT i1, datatype, nullable, rc, version;
	CHAR errmsg[512], work[65], work2[32];
	IMP_ROW_DESC_RECORD *ird_rec2;
	union {
		SQLSMALLINT c_sshort;
		SQLUSMALLINT c_ushort;
		SQLINTEGER c_slong;
		SQLUINTEGER c_ulong;
		SQLREAL c_float;
		SQLDOUBLE c_double;
		SQLCHAR c_bit;
		SQLSCHAR c_stinyint;
		SQLCHAR c_utinyint;
		SQLBIGINT c_sbigint;
		SQLUBIGINT c_ubigint;
		SQL_NUMERIC_STRUCT c_numeric;
		SQL_DATE_STRUCT c_date;
		SQL_TIME_STRUCT c_time;
		SQL_TIMESTAMP_STRUCT c_timestamp;
	} ctype;

	if (stmt->executeflags & EXECUTE_SQLCOLUMNS) {  /* add server => ODBC conversion */

/*** CODE: The following few lines of code are for a hack to remedy a Crystal Reports version   ***/
/***       9 & 10 bug.  Crystal Reports is using SQL_C_DOUBLE as the data type for COLUMN_SIZE, ***/
/***       even though the driver tells Crystal Reports to use SQL_NUMERIC.  To further         ***/
/***       complicate matters, Crystal Reports stores the resulting column size into a variable ***/
/***       defined as SQLINTEGER                                                                ***/
		if (type == SQL_C_DOUBLE && (column == 7 || column == 8)) {
			type = SQL_C_LONG; /* COLUMN_SIZE & BUFFER_LENGTH, Crystal Reports HACK, REMOVE THIS CODE */
		}

		switch (column) {
		case 5:  /* DATA_TYPE */
		case 6:  /* TYPE_NAME */
		case 8:  /* BUFFER_LENGTH */
		case 10:  /* NUM_PREC_RADIX */
		case 11:  /* NULLABLE */
		case 14:  /* SQL_DATA_TYPE */
		case 15:  /* SQL_DATETIME_SUB */
		case 16:  /* CHAR_OCTET_LENGTH */
		case 18:  /* IS_NULLABLE */
			if (column != 5) {  /* get data_type */
				ird_rec2 = (IMP_ROW_DESC_RECORD *) desc_get(stmt->sql_attr_imp_row_desc, 5, FALSE);
				sqllen = ird_rec2->fieldlength;
				sqlbuffer = (CHAR*)(stmt->row_buffer + ird_rec2->rowbufoffset);
			}
			if ((size_t)sqllen >= sizeof(work)) sqllen = sizeof(work) - 1;
			memcpy(work, sqlbuffer, sqllen);
			work[sqllen] = 0;
			version = stmt->cnct->env->sql_attr_odbc_version;
			if (stmt->cnct->server_majorver == 2) {
				switch (atoi(work)) {
				case 2:
					datatype = SQL_NUMERIC;
					break;
				case 91:
					datatype = (version == SQL_OV_ODBC2) ? SQL_DATE : SQL_TYPE_DATE;
					break;
				case 92:
					datatype = (version == SQL_OV_ODBC2) ? SQL_TIME : SQL_TYPE_TIME;
					break;
				case 93:
					datatype = (version == SQL_OV_ODBC2) ? SQL_TIMESTAMP : SQL_TYPE_TIMESTAMP;
					break;
				default:
					datatype = SQL_CHAR;
					break;
				}
			}
			else {
				switch (work[0]) {
				case 'N':
					datatype = SQL_NUMERIC;
					break;
				case 'D':
					datatype = (version == SQL_OV_ODBC2) ? SQL_DATE : SQL_TYPE_DATE;
					break;
				case 'T':
					datatype = (version == SQL_OV_ODBC2) ? SQL_TIME : SQL_TYPE_TIME;
					break;
				case 'S':
					datatype = (version == SQL_OV_ODBC2) ? SQL_TIMESTAMP : SQL_TYPE_TIMESTAMP;
					break;
				default:
					datatype = SQL_CHAR;
					break;
				}
			}
			switch (column) {
			case 5:  /* DATA_TYPE */
				_itoa(datatype, work2, 10);
				sqlbuffer = work2;
				break;
			case 6:  /* TYPE_NAME */
				if (datatype == SQL_CHAR) sqlbuffer = "CHAR";
				else if (datatype == SQL_NUMERIC) sqlbuffer = "NUMERIC";
				else if (datatype == SQL_DATE || datatype == SQL_TYPE_DATE) sqlbuffer = "DATE";
				else if (datatype == SQL_TIME || datatype == SQL_TYPE_TIME) sqlbuffer = "TIME";
				else sqlbuffer = "TIMESTAMP";
				break;
			case 8:  /* BUFFER_LENGTH */
				if (datatype == SQL_CHAR || datatype == SQL_NUMERIC) {
					ird_rec2 = (IMP_ROW_DESC_RECORD *) desc_get(stmt->sql_attr_imp_row_desc, 7, FALSE);
					tcpntoi(stmt->row_buffer + ird_rec2->rowbufoffset, ird_rec2->fieldlength, &i1);
				}
				else if (datatype == SQL_DATE || datatype == SQL_TYPE_DATE) i1 = sizeof(SQL_DATE_STRUCT);
				else if (datatype == SQL_TIME || datatype == SQL_TYPE_TIME) i1 = sizeof(SQL_TIME_STRUCT);
				else i1 = sizeof(SQL_TIMESTAMP_STRUCT);
				_itoa(i1, work2, 10);
				sqlbuffer = work2;
				break;
			case 10:  /* NUM_PREC_RADIX */
				if (datatype == SQL_NUMERIC) sqlbuffer="10";
				else sqlbuffer = "";
				break;
			case 11:  /* NULLABLE */
				if (datatype == SQL_CHAR) i1 = SQL_FALSE;
				else i1 = SQL_TRUE;
				_itoa(i1, work2, 10);
				sqlbuffer = work2;
				break;
				break;
			case 14:  /* SQL_DATA_TYPE */
				if (datatype == SQL_CHAR || datatype == SQL_NUMERIC) i1 = datatype;
				else i1 = SQL_DATETIME;
				_itoa(i1, work2, 10);
				sqlbuffer = work2;
				break;
			case 15:  /* SQL_DATETIME_SUB */
				if (datatype == SQL_CHAR || datatype == SQL_NUMERIC) sqlbuffer= "";
				else {
					if (datatype == SQL_DATE || datatype == SQL_TYPE_DATE) i1 = SQL_CODE_DATE;
					else if (datatype == SQL_TIME || datatype == SQL_TYPE_TIME) i1 = SQL_CODE_TIME;
					else i1 = SQL_CODE_TIMESTAMP;
					_itoa(i1, work2, 10);
					sqlbuffer = work2;
				}
				break;
			case 16:  /* CHAR_OCTET_LENGTH */
				if (datatype == SQL_CHAR) sqlbuffer= "65500";
				else sqlbuffer = "";
				break;
			case 18:  /* IS_NULLABLE */
				if (datatype == SQL_CHAR) sqlbuffer= "NO";
				else sqlbuffer = "YES";
				break;
			}
			sqllen = strlen(sqlbuffer);
			break;
		case 13:  /* COLUMN_DEF */
			sqllen = 0;
			break;
		}
	}
	else if (stmt->executeflags & EXECUTE_SQLGETTYPEINFO) {  /* add server => ODBC conversion */
		if (column != 2) {  /* get data_type */
			ird_rec2 = (IMP_ROW_DESC_RECORD *) desc_get(stmt->sql_attr_imp_row_desc, 2, FALSE);
			sqllen = ird_rec2->fieldlength;
			sqlbuffer = (CHAR*)(stmt->row_buffer + ird_rec2->rowbufoffset);
		}
		if ((size_t)sqllen >= sizeof(work)) sqllen = sizeof(work) - 1;
		memcpy(work, sqlbuffer, sqllen);
		work[sqllen] = '\0';
		datatype = atoi(work);  /* already corrected for ODBC version */
		switch (column) {
		case 1:  /* TYPE_NAME */
			if (datatype == SQL_CHAR) sqlbuffer = "CHAR";
			else if (datatype == SQL_NUMERIC) sqlbuffer = "NUMERIC";
			else if (datatype == SQL_DATE || datatype == SQL_TYPE_DATE) sqlbuffer = "DATE";
			else if (datatype == SQL_TIME || datatype == SQL_TYPE_TIME) sqlbuffer = "TIME";
			else if (datatype == SQL_TIMESTAMP || datatype == SQL_TYPE_TIMESTAMP)
				sqlbuffer = "TIMESTAMP";
			else sqlbuffer = "?";
			break;
		case 2:  /* DATA_TYPE */
			_itoa(datatype, work2, 10);
			sqlbuffer = work2;
			break;
		case 3:  /* COLUMN_SIZE */
			switch(datatype){
			case SQL_CHAR:
				sqlbuffer = "65500";
				break;
			case SQL_NUMERIC:
				sqlbuffer = "31";
				break;
			case SQL_DATE:
			case SQL_TYPE_DATE:
				sqlbuffer = "10";
				break;
			case SQL_TIME:
			case SQL_TYPE_TIME:
				sqlbuffer = "12";	/*hh:mm:ss.ppp		per Don, could go to 1000's of a second*/
				break;
			case SQL_TIMESTAMP:
			case SQL_TYPE_TIMESTAMP:
				_itoa(TIMESTAMP_LENGTH, work2, 10);		/*yyyy-mm-dd hh:mm:ss.ppp*/
				sqlbuffer = work2;
				break;
			default:
				sqlbuffer = "?";
			}
			break;
		case 4:  /* LITERAL_PREFIX */
		case 5:  /* LITERAL_SUFFIX */
			if (datatype == SQL_CHAR || datatype == SQL_TYPE_TIMESTAMP || datatype == SQL_TIMESTAMP)
				sqlbuffer = "'";
			else
				sqlbuffer = "";
			break;
		case 6:  /* CREATE_PARAMS */
			switch(datatype){
			case SQL_CHAR:
				sqlbuffer = "length";
				break;
			case SQL_NUMERIC:
				sqlbuffer = "precision,scale";
				break;
			case SQL_DATE:
			case SQL_TYPE_DATE:
				sqlbuffer = "";
				break;
			case SQL_TIME:
			case SQL_TIMESTAMP:
			case SQL_TYPE_TIME:
			case SQL_TYPE_TIMESTAMP:
				sqlbuffer = "scale";
				break;
			}
			break;
		case 7:  /* NULLABLE */
			if (datatype == SQL_CHAR)
				_itoa(SQL_NO_NULLS, work2, 10);
			else
				_itoa(SQL_NULLABLE, work2, 10);
			sqlbuffer = work2;
			break;
		case 8:  /* CASE_SENSITIVE */
			if (datatype == SQL_CHAR)
				_itoa(SQL_TRUE, work2, 10);
			else
				_itoa(SQL_FALSE, work2, 10);
			sqlbuffer = work2;
			break;
		case 9:  /* SEARCHABLE */
			_itoa(SQL_SEARCHABLE, work2, 10);
			sqlbuffer = work2;
			break;
		case 10:  /* UNSIGNED_ATTRIBUTE */
			if (datatype == SQL_NUMERIC)
				_itoa(SQL_FALSE, work2, 10);
			else
				work2[0] = '\0';
			sqlbuffer = work2;
			break;
		case 11:  /* FIXED_PREC_SCALE */
			_itoa(SQL_FALSE, work2, 10);
			sqlbuffer = work2;
			break;
		case 12:  /* AUTO_UNIQUE_VALUE */
			if (datatype == SQL_NUMERIC)
				_itoa(SQL_FALSE, work2, 10);
			else
				work2[0] = '\0';
			sqlbuffer = work2;
			break;
		case 13:  /* LOCAL_TYPE_NAME */
			sqlbuffer = "";
			break;
		case 14:  /* MINIMUM_SCALE */
			if (datatype == SQL_CHAR || datatype == SQL_TYPE_DATE || datatype == SQL_DATE)
				work2[0] = '\0';
			else
				_itoa(0, work2, 10);
			sqlbuffer = work2;
			break;
		case 15:  /* MAXIMUM_SCALE */
			if (datatype == SQL_CHAR || datatype == SQL_TYPE_DATE || datatype == SQL_DATE)
				work2[0] = '\0';
			else if (datatype == SQL_NUMERIC)
				_itoa(30, work2, 10);
			else
				_itoa(3, work2, 10);
			sqlbuffer = work2;
			break;
		case 16:  /* SQL_DATA_TYPE */
			if (datatype == SQL_CHAR || datatype == SQL_NUMERIC) i1 = datatype;
			else i1 = SQL_DATETIME;
			_itoa(i1, work2, 10);
			sqlbuffer = work2;
			break;
		case 17:  /* SQL_DATETIME_SUB */
			if (datatype == SQL_CHAR || datatype == SQL_NUMERIC) sqlbuffer= "";
			else {
				if (datatype == SQL_DATE || datatype == SQL_TYPE_DATE) i1 = SQL_CODE_DATE;
				else if (datatype == SQL_TIME || datatype == SQL_TYPE_TIME) i1 = SQL_CODE_TIME;
				else i1 = SQL_CODE_TIMESTAMP;
				_itoa(i1, work2, 10);
				sqlbuffer = work2;
			}
			break;
		case 18:  /* NUM_PREC_RADIX */
			if (datatype == SQL_NUMERIC)
				_itoa(10, work2, 10);
			else
				work2[0] = '\0';
			sqlbuffer = work2;
			break;
		case 19:  /* INTERVAL_PRECISION */
			sqlbuffer = "";
			break;
		}
		sqllen = strlen(sqlbuffer);
	}
	/* no conversion needed for EXECUTE_PRIMARYKEYS */
	else if (stmt->executeflags & EXECUTE_SQLSPECIALCOLUMNS) {  /* add server => ODBC conversion */
		version = stmt->cnct->env->sql_attr_odbc_version;
		if (column == 3 || column == 4 || column == 6) {
			if (column != 3) {  /* get data_type */
				ird_rec2 = (IMP_ROW_DESC_RECORD *) desc_get(stmt->sql_attr_imp_row_desc, 3, FALSE);
				sqllen = ird_rec2->fieldlength;
				sqlbuffer = (CHAR*)(stmt->row_buffer + ird_rec2->rowbufoffset);
			}
			if ((size_t)sqllen >= sizeof(work)) sqllen = sizeof(work) - 1;
			memcpy(work, sqlbuffer, sqllen);
			work[sqllen] = 0;
			if (stmt->cnct->server_majorver == 2) {
				switch (atoi(work)) {
				case 2:
					datatype = SQL_NUMERIC;
					break;
				case 91:
					datatype = (version == SQL_OV_ODBC2) ? SQL_DATE : SQL_TYPE_DATE;
					break;
				case 92:
					datatype = (version == SQL_OV_ODBC2) ? SQL_TIME : SQL_TYPE_TIME;
					break;
				case 93:
					datatype = (version == SQL_OV_ODBC2) ? SQL_TIMESTAMP : SQL_TYPE_TIMESTAMP;
					break;
				default:
					datatype = SQL_CHAR;
					break;
				}
			}
			else {
				switch (work[0]) {
				case 'N':
					datatype = SQL_NUMERIC;
					break;
				case 'D':
					datatype = (version == SQL_OV_ODBC2) ? SQL_DATE : SQL_TYPE_DATE;
					break;
				case 'T':
					datatype = (version == SQL_OV_ODBC2) ? SQL_TIME : SQL_TYPE_TIME;
					break;
				case 'S':
					datatype = (version == SQL_OV_ODBC2) ? SQL_TIMESTAMP : SQL_TYPE_TIMESTAMP;
					break;
				default:
					datatype = SQL_CHAR;
					break;
				}
			}
		}
		switch (column) {
		case 1:  /* SCOPE */
			_itoa(SQL_SCOPE_CURROW, work2, 10);
			sqlbuffer = work2;
			sqllen = strlen(sqlbuffer);
			break;
		case 3:  /* DATA_TYPE */
			_itoa(datatype, work2, 10);
			sqlbuffer = work2;
			sqllen = strlen(sqlbuffer);
			break;
		case 4:  /* TYPE_NAME */
			if (datatype == SQL_CHAR) sqlbuffer = "CHAR";
			else if (datatype == SQL_NUMERIC) sqlbuffer = "NUMERIC";
			else if (datatype == SQL_DATE || datatype == SQL_TYPE_DATE) sqlbuffer = "DATE";
			else if (datatype == SQL_TIME || datatype == SQL_TYPE_TIME) sqlbuffer = "TIME";
			else sqlbuffer = "TIMESTAMP";
			sqllen = strlen(sqlbuffer);
			break;
		case 6:  /* BUFFER_LENGTH */
			if (datatype == SQL_CHAR || datatype == SQL_NUMERIC) {
				ird_rec2 = (IMP_ROW_DESC_RECORD *) desc_get(stmt->sql_attr_imp_row_desc, 5, FALSE);
				tcpntoi(stmt->row_buffer + ird_rec2->rowbufoffset, ird_rec2->fieldlength, &i1);
			}
			else if (datatype == SQL_DATE || datatype == SQL_TYPE_DATE) i1 = sizeof(SQL_DATE_STRUCT);
			else if (datatype == SQL_TIME || datatype == SQL_TYPE_TIME) i1 = sizeof(SQL_TIME_STRUCT);
			else i1 = sizeof(SQL_TIMESTAMP_STRUCT);
			_itoa(i1, work2, 10);
			sqlbuffer = work2;
			sqllen = strlen(sqlbuffer);
			break;
		case 8:  /* PSEUDO_COLUMN */
			_itoa(SQL_PC_NOT_PSEUDO, work2, 10);
			sqlbuffer = work2;
			sqllen = strlen(work2);
			break;
		}
	}
	else if (stmt->executeflags & EXECUTE_SQLSTATISTICS) {  /* add server => ODBC conversion */
		switch (column) {
		case 4:  /* NON_UNIQUE */
			for (i1 = 0; (size_t)i1 < sqllen && sqlbuffer[i1] == ' '; i1++);
			if ((size_t)i1 < sqllen) {
				if (sqllen >= sizeof(work)) sqllen = sizeof(work) - 1;
				memcpy(work, sqlbuffer, sqllen);
				work[sqllen] = '\0';
				switch (atoi(work)) {
				case 0:
					i1 = SQL_FALSE;
					break;
				default:
					i1 = SQL_TRUE;
					break;
				}
				_itoa(i1, work2, 10);
				sqlbuffer = work2;
				sqllen = strlen(work2);
			}
			break;
		case 7:  /* TYPE */
			if ((size_t)sqllen >= sizeof(work)) sqllen = sizeof(work) - 1;
			memcpy(work, sqlbuffer, sqllen);
			work[sqllen] = '\0';
			switch (work[0]) {
			case '0':  /* v2 server */
			case ' ':  /* v3 server */
				i1 = SQL_TABLE_STAT;
				break;
			case 'A':  /* v3 server */
				i1 = SQL_INDEX_HASHED;
				break;
			case '3':  /* v2 server */
			case 'I':  /* v3 server */
			default:
				i1 = SQL_INDEX_OTHER;
				break;
			}
			_itoa(i1, work2, 10);
			sqlbuffer = work2;
			sqllen = strlen(work2);
			break;
		}
	}
	else if (stmt->executeflags & EXECUTE_SQLTABLES) {  /* add server => ODBC conversion */
		if (column == 4) {
			sqlbuffer = "TABLE";
			sqllen = 5;
		}
	}

	nullable = (ird_rec->sql_desc_nullable == SQL_NULLABLE);
	if (stmt->executeflags & (EXECUTE_SQLCOLUMNS | EXECUTE_SQLGETTYPEINFO | EXECUTE_SQLPRIMARYKEYS | EXECUTE_SQLSPECIALCOLUMNS | EXECUTE_SQLSTATISTICS | EXECUTE_SQLTABLES)) {
		while (sqllen > 0 && sqlbuffer[sqllen - 1] == ' ') sqllen--;
		if (sqllen) nullable = FALSE;
	}
	else if (nullable) {
		for (i1 = 0; (size_t)i1 < sqllen && sqlbuffer[i1] == ' '; i1++);
		if ((size_t)i1 < sqllen) nullable = FALSE;
	}
	if (nullable) {
		if (cnullind == NULL) {
			error_record(&stmt->errors, "22002", ERROR_22002, NULL, 0);
			return SQL_ERROR;
		}
/*** NOTE: some confusion in doc as SQLBindCol says that column binds, ie. crowsize = 0 ***/
/***       assumes coffset = 0, but Visual Basic sets bind_offset without setting ***/
/***       bind_type if driver only supports one row at a time.  Any changes need ***/\
/***       to be carried down below ***/
		if (crownum) {
			if (!crowsize) crowsize = sizeof(SQLINTEGER);
			coffset += crownum * crowsize;
		}
		if (coffset) {
			cnullind = cnullind + coffset;
#if OS_WIN32
			if (IsBadWritePtr(cnullind, sizeof(SQLINTEGER))) {
				error_record(&stmt->errors, "HY000", TEXT_HY000, "invalid indicator offset", 0);
				return SQL_ERROR;
			}
#else
			/* TODO CODE HERE TO PREVENT UNIX CORE DUMP */
#endif
		}
		*cnullind = SQL_NULL_DATA;
		return SQL_SUCCESS;
	}

	rc = SQL_SUCCESS;
	if (type == SQL_C_DEFAULT) {
		if (ird_rec->default_type) type = ird_rec->default_type;
		if (type == SQL_C_DEFAULT) {
			if (ird_rec->sql_desc_concise_type == SQL_TYPE_DATE) type = SQL_C_TYPE_DATE;
			else if (ird_rec->sql_desc_concise_type == SQL_TYPE_TIME) type = SQL_C_TYPE_TIME;
			else if (ird_rec->sql_desc_concise_type == SQL_TYPE_TIMESTAMP) type = SQL_C_TYPE_TIMESTAMP;
			else type = SQL_C_CHAR;
		}
	}
	if (type == SQL_C_CHAR) {
		if (ird_rec->sql_desc_concise_type == SQL_NUMERIC) {
			while (sqllen && *sqlbuffer == ' ') {
				sqlbuffer++;
				sqllen--;
			}
		}
	}
	else if (type == SQL_C_LONG || type == SQL_C_SLONG || type == SQL_C_ULONG ||
		type == SQL_C_SHORT || type == SQL_C_SSHORT || type == SQL_C_USHORT ||
		type == SQL_C_TINYINT || type == SQL_C_STINYINT || type == SQL_C_UTINYINT ||
		type == SQL_C_BIT || type == SQL_C_SBIGINT || type == SQL_C_UBIGINT ||
		type == SQL_C_DOUBLE || type == SQL_C_FLOAT || type == SQL_C_NUMERIC) {
		if (ird_rec->sql_desc_concise_type != SQL_CHAR && ird_rec->sql_desc_concise_type != SQL_NUMERIC) {
			if (!(((stmt->executeflags & EXECUTE_SQLCOLUMNS) ||
				(stmt->executeflags & EXECUTE_SQLGETTYPEINFO) ||
				(stmt->executeflags & EXECUTE_SQLPRIMARYKEYS) ||
				(stmt->executeflags & EXECUTE_SQLSPECIALCOLUMNS) ||
				(stmt->executeflags & EXECUTE_SQLSTATISTICS) ||
				(stmt->executeflags & EXECUTE_SQLTABLES)) && ird_rec->sql_desc_concise_type == SQL_SMALLINT))
			{
				error_record(&stmt->errors, "07006", TEXT_07006, "SQL type is not char or numeric", 0);
				return SQL_ERROR;
			}
		}
/*** CODE: MAYBE SET 01004 ERROR ***/
		if ((size_t)sqllen >= sizeof(work)) sqllen = sizeof(work) - 1;
		memcpy(work, sqlbuffer, sqllen);
		work[sqllen] = 0;
		if (type == SQL_C_LONG || type == SQL_C_SLONG) {
			ctype.c_slong = atol(work);
			sqllen = sizeof(ctype.c_slong);
		}
		else if (type == SQL_C_ULONG) {
			ctype.c_ulong = (SQLUINTEGER) atol(work);
			sqllen = sizeof(ctype.c_ulong);
		}
		else if (type == SQL_C_SHORT || type == SQL_C_SSHORT) {
			ctype.c_sshort = atoi(work);
			sqllen = sizeof(ctype.c_sshort);
		}
		else if (type == SQL_C_USHORT) {
			ctype.c_ushort = (SQLUSMALLINT) atoi(work);
			sqllen = sizeof(ctype.c_ushort);
		}
		else if (type == SQL_C_TINYINT || type == SQL_C_STINYINT) {
			ctype.c_stinyint = (SQLSCHAR) atoi(work);
			sqllen = sizeof(ctype.c_stinyint);
		}
		else if (type == SQL_C_UTINYINT || type == SQL_C_BIT) {
			ctype.c_utinyint = (SQLCHAR) atoi(work);
			sqllen = sizeof(ctype.c_utinyint);
		}
#if OS_WIN32
		else if (type == SQL_C_SBIGINT) {
			for (sqllen = 1, i1 = 0; work[i1] == ' '; i1++);
			if (work[i1] == '-') {
				sqllen = -1;
				i1++;
			}
			for (ctype.c_sbigint = (SQLBIGINT) 0; isdigit(work[i1]); i1++)
				ctype.c_sbigint = ctype.c_sbigint * 10 + work[i1] - '0';
			ctype.c_sbigint = (SQLBIGINT) ctype.c_sbigint * sqllen;
			sqllen = sizeof(ctype.c_sbigint);
		}
		else if (type == SQL_C_UBIGINT) {
			for (sqllen = 1, i1 = 0; work[i1] == ' '; i1++);
			if (work[i1] == '-') {
				sqllen = -1;
				i1++;
			}
			for (ctype.c_ubigint = 0; isdigit(work[i1]); i1++)
				ctype.c_ubigint = ctype.c_ubigint * 10 + work[i1] - '0';
			ctype.c_ubigint *= sqllen;
			sqllen = sizeof(ctype.c_ubigint);
		}
#endif
		else if (type == SQL_C_DOUBLE) {
			ctype.c_double = atof(work);
			sqllen = sizeof(ctype.c_double);
		}
		else if (type == SQL_C_FLOAT) {
			ctype.c_float = (SQLREAL) atof(work);
			sqllen = sizeof(ctype.c_float);
		}
		else if (type == SQL_C_NUMERIC) {
			asctocnum(work, sqlprecision, sqlscale, &ctype.c_numeric);
			sqllen = sizeof(ctype.c_numeric);
		}
	}
	else if (type == SQL_C_TYPE_DATE) {
		memcpy(work, "0000-00-00", 10);
		if (ird_rec->sql_desc_concise_type == SQL_CHAR) {
			/* Database thinks this is a character string, user wants a date from it */
			if (cvttodatetime(sqlbuffer, (INT)sqllen, SQL_C_TYPE_DATE, work) == -1) {
				error_record(&stmt->errors, "22018", TEXT_22018, "Could not convert CHAR to DATE", 0);
				return SQL_ERROR;
			}
		}
		else if (ird_rec->sql_desc_concise_type == SQL_TYPE_DATE || ird_rec->sql_desc_concise_type == SQL_TYPE_TIMESTAMP)
			memcpy(work, sqlbuffer, ird_rec->sql_desc_length);
		else {
			error_record(&stmt->errors, "07006", TEXT_07006, "SQL type is not char, date, or timestamp", 0);
			return SQL_ERROR;
		}
		work[4] = work[7] = work[10] = '\0';
		ctype.c_date.year = atoi(work);
		ctype.c_date.month = atoi(work + 5);
		ctype.c_date.day = atoi(work + 8);
		sqllen = sizeof(ctype.c_date);
	}
	else if (type == SQL_C_TYPE_TIME) {
		memcpy(work, "00:00:00", 8);
		if (ird_rec->sql_desc_concise_type == SQL_CHAR) {
			/* Database thinks this is a character string, user wants a time from it */
			if (cvttodatetime(sqlbuffer, (INT)sqllen, SQL_C_TYPE_TIME, work) == -1) {
				error_record(&stmt->errors, "22018", TEXT_22018, "Could not convert CHAR to TIME", 0);
				return SQL_ERROR;
			}
		}
		else if (ird_rec->sql_desc_concise_type == SQL_TYPE_TIME)
			memcpy(work, sqlbuffer, ird_rec->sql_desc_length);
		else if (ird_rec->sql_desc_concise_type == SQL_TYPE_TIMESTAMP) {
			if (ird_rec->sql_desc_length > 11) memcpy(work, sqlbuffer + 11, ird_rec->sql_desc_length - 11);
		}
		else {
			error_record(&stmt->errors, "07006", TEXT_07006, "SQL type is not char, date or timestamp", 0);
			return SQL_ERROR;
		}
		work[2] = work[5] = work[8] = '\0';
		ctype.c_time.hour = atoi(work);
		ctype.c_time.minute = atoi(work + 3);
		ctype.c_time.second = atoi(work + 6);
		sqllen = sizeof(ctype.c_time);
	}
	else if (type == SQL_C_TYPE_TIMESTAMP) {
		memcpy(work, "0000-00-00 00:00:00.000000", 26);
		if (ird_rec->sql_desc_concise_type == SQL_CHAR) {
			/* Database thinks this is a character string, user wants a timestamp from it */
			if (cvttodatetime(sqlbuffer, (INT)sqllen, SQL_C_TYPE_TIMESTAMP, work) == -1) {
				error_record(&stmt->errors, "22018", TEXT_22018, "Could not convert CHAR to TIMESTAMP", 0);
				return SQL_ERROR;
			}
		}
		else if (ird_rec->sql_desc_concise_type == SQL_TYPE_DATE || ird_rec->sql_desc_concise_type == SQL_TYPE_TIMESTAMP)
			memcpy(work, sqlbuffer, ird_rec->sql_desc_length);
		else if (ird_rec->sql_desc_concise_type == SQL_TYPE_TIME)
			memcpy(work + 11, sqlbuffer, ird_rec->sql_desc_length);
		else {
			error_record(&stmt->errors, "07006", TEXT_07006, "SQL type is not char, date, time or timestamp", 0);
			return SQL_ERROR;
		}
		work[4] = work[7] = work[10] = work[13] = work[16] = work[19] = work[TIMESTAMP_LENGTH] = '\0';
		ctype.c_timestamp.year = atoi(work);
		ctype.c_timestamp.month = atoi(work + 5);
		ctype.c_timestamp.day = atoi(work + 8);
		ctype.c_timestamp.hour = atoi(work + 11);
		ctype.c_timestamp.minute = atoi(work + 14);
		ctype.c_timestamp.second = atoi(work + 17);
		ctype.c_timestamp.fraction = atoi(work + 20);
		for (i1 = TIMESTAMP_LENGTH - 20; i1 < 9; i1++) ctype.c_timestamp.fraction *= 10;  /*** Field is BILLIONTHS! ***/
		sqllen = sizeof(ctype.c_timestamp);
	}
	else {
		sprintf(errmsg, "Target parameter: Unsupported C data type: %d", (int) type);
		error_record(&stmt->errors, "07006", TEXT_07006, errmsg, 0);
		return SQL_ERROR;
	}

	if (cbuffer != NULL) {
		if (crownum || coffset) {
			if (crownum) {
				if (!crowsize) {
					if (type == SQL_C_CHAR) cbuffer = (CHAR *) cbuffer + cbuflen;
					else cbuffer = (CHAR*) cbuffer + sqllen;
				}
				else cbuffer = (CHAR *) cbuffer + crowsize;
			}
			cbuffer = (CHAR *) cbuffer + coffset;
#if OS_WIN32
			if (IsBadWritePtr(cbuffer, (type == SQL_C_CHAR) ? cbuflen : sqllen)) {
				error_record(&stmt->errors, "HY000", TEXT_HY000, "invalid buffer offset", 0);
				return SQL_ERROR;
			}
#else
			/* TODO CODE HERE TO PREVENT UNIX CORE DUMP */
#endif
		}
		if (type == SQL_C_CHAR) {
			memcpy(cbuffer, sqlbuffer, min((SQLLEN)sqllen, cbuflen));
			if ((SQLLEN)sqllen >= cbuflen) {
				if (cbuflen > 0) *((CHAR *) cbuffer + cbuflen - 1) = '\0';
				error_record(&stmt->errors, "01004", TEXT_01004, NULL, 0);
				rc = SQL_SUCCESS_WITH_INFO;
			}
			else *((CHAR *) cbuffer + sqllen) = '\0';
		}
		else memcpy(cbuffer, &ctype, sqllen);
	}
	if (crownum) {
		if (!crowsize) crowsize = sizeof(SQLINTEGER);
		coffset += crownum * crowsize;
	}
	if (cnullind != NULL && cnullind != cdatalen) {
		if (coffset) {
			cnullind = (SQLLEN *) (CHAR *) cnullind + coffset;
#if OS_WIN32
			if (IsBadWritePtr(cnullind, sizeof(SQLINTEGER))) {
				error_record(&stmt->errors, "HY000", TEXT_HY000, "invalid indicator offset", 0);
				return SQL_ERROR;
			}
#else
			/* TODO CODE HERE TO PREVENT UNIX CORE DUMP */
#endif
		}
		*cnullind = 0;
	}
	if (cdatalen != NULL) {
		if (coffset) {
			cdatalen = (SQLLEN *) (CHAR *) cdatalen + coffset;
#if OS_WIN32
			if (IsBadWritePtr(cdatalen, sizeof(SQLINTEGER))) {
				error_record(&stmt->errors, "HY000", TEXT_HY000, "invalid length offset", 0);
				return SQL_ERROR;
			}
#else
			/* TODO CODE HERE TO PREVENT UNIX CORE DUMP */
#endif
		}
		*cdatalen = sqllen;
	}
	return rc;
}

/**
 * This uses the global field workbuffer and must be called inside a thread-safe region.
 */
static SQLRETURN fixupexec(STATEMENT *stmt, ERRORS *errors, UCHAR **textp, INT *textlen, INT dynflag)
{
#define ESCAPE_IGNORE		1
#define ESCAPE_OJ			2
#define ESCAPE_DATE			3
#define ESCAPE_ESCAPE		4
#define ESCAPE_TIME			5
#define ESCAPE_TIMESTAMP	6
	INT i1, i2, esccnt, len, param, scanflag, sqltype, type;
	SQLLEN parmlen, worklen, pos, mark;
	CHAR work[64], *ptr;
	UCHAR *text;
	UCHAR errmsg[MAX_ERROR_MSG];
	SQL_DATE_STRUCT *sqldate;
	SQL_TIME_STRUCT *sqltime;
	SQL_TIMESTAMP_STRUCT *sqltimestamp;
	APP_DESC_RECORD *apd_rec;
	IMP_PARAM_DESC_RECORD *ipd_rec;
	struct {
		int type;
		SQLLEN offset;  /* currently not being utilized usefully */
	} escapes[32];
	esccnt = 0;
	scanflag = (stmt->sql_attr_noscan == SQL_NOSCAN_OFF);
	text = *textp;
	len = *textlen;
	for (pos = mark = worklen = 0, param = 0; pos < len; ) {
		if (text[pos] == '\'') {
			pos++;
			if (pos < len && text[pos] != '\'') {  /* not null string */
				do {  /* find matching quote */
					if (text[pos] == '\'') {
						if (pos + 1 == len || text[pos + 1] != '\'') break;
						pos++;
					}
					else if (text[pos] == '\\') {
						if (++pos == len) break;
					}
				} while (++pos < len);
			}
			if (pos++ == len) {  /* end of statement - error */
				error_record(errors, "42000", "Syntax error: no closing apostrophe", NULL, 0);
				return SQL_ERROR;
			}
			continue;
		}
		if (text[pos] == '?') {
			if (!dynflag) {
				pos++;
				continue;
			}
			if (++param > stmt->sql_attr_app_param_desc->sql_desc_count) {  /* too few dynamic parameters have been bound */
				sprintf((char*)errmsg, "%s, too few bound parameters", TEXT_07002);
				error_record(errors, "07002", (char*)errmsg, NULL, 0);
				return SQL_ERROR;
			}
			memcpy(&workbuffer[worklen], &text[mark], pos - mark);
			worklen += pos - mark;
			if (param == 1) {
				apd_rec = stmt->sql_attr_app_param_desc->firstrecord.appdrec;
				ipd_rec = stmt->sql_attr_imp_param_desc->firstrecord.ipdrec;
			}
			else {
				apd_rec = apd_rec->nextrecord;
				ipd_rec = ipd_rec->nextrecord;
			}
			type = apd_rec->sql_desc_concise_type;
			if (type == SQL_C_DEFAULT) {
				if (ipd_rec->sql_desc_concise_type == SQL_TYPE_DATE) type = SQL_C_TYPE_DATE;
				else if (ipd_rec->sql_desc_concise_type == SQL_TYPE_TIME) type = SQL_C_TYPE_TIME;
				else if (ipd_rec->sql_desc_concise_type == SQL_TYPE_TIMESTAMP) type = SQL_C_TYPE_TIMESTAMP;
				else type = SQL_C_CHAR;
			}
			if (type == SQL_C_CHAR) {
				sqltype = SQL_CHAR;
				ptr = (CHAR *) apd_rec->sql_desc_data_ptr;
				if (apd_rec->sql_desc_octet_length_ptr == NULL || *apd_rec->sql_desc_octet_length_ptr == SQL_NTS)
					parmlen = (INT)strlen((CHAR *) apd_rec->sql_desc_data_ptr);
				else if (*apd_rec->sql_desc_octet_length_ptr == SQL_NULL_DATA) parmlen = 0;
				else if (*apd_rec->sql_desc_octet_length_ptr >= 0) parmlen = *apd_rec->sql_desc_octet_length_ptr;
				else {
					ptr = (CHAR*)apd_rec->dataatexecptr;
					parmlen = apd_rec->dataatexeclength;
				}
			}
			else {
				if (apd_rec->sql_desc_octet_length_ptr != NULL && (*apd_rec->sql_desc_octet_length_ptr == SQL_NULL_DATA ||
					*apd_rec->sql_desc_octet_length_ptr == SQL_DATA_AT_EXEC || *apd_rec->sql_desc_octet_length_ptr <= SQL_LEN_DATA_AT_EXEC_OFFSET)) {
					if (*apd_rec->sql_desc_octet_length_ptr != SQL_NULL_DATA) {
						ptr = (CHAR*)apd_rec->dataatexecptr;
						parmlen = apd_rec->dataatexeclength;
					}
					else parmlen = 0;
				}
				else {
					ptr = (CHAR *) apd_rec->sql_desc_data_ptr;
					parmlen = 1;  /* set to any non-zero value */
				}
				sqltype = SQL_NUMERIC;
				if (parmlen) {
					if (type == SQL_C_LONG || type == SQL_C_SLONG)
						_ltoa(*(SQLINTEGER *) ptr, work, 10);
					else if (type == SQL_C_ULONG)
						_ltoa(*(SQLUINTEGER *) ptr, work, 10);
					else if (type == SQL_C_SHORT || type == SQL_C_SSHORT)
						_itoa(*(SQLSMALLINT *) ptr, work, 10);
					else if (type == SQL_C_USHORT)
						_itoa(*(SQLUSMALLINT *) ptr, work, 10);
					else if (type == SQL_C_TINYINT || type == SQL_C_STINYINT)
						_itoa(*(SQLSCHAR *) ptr, work, 10);
					else if (type == SQL_C_UTINYINT)
						_itoa(*(SQLCHAR *) ptr, work, 10);
					else if (type == SQL_C_DOUBLE)
						sprintf(work, "%.*f", ipd_rec->sql_desc_scale, *(SQLDOUBLE *) ptr);
					else if (type == SQL_C_FLOAT)
						sprintf(work, "%.*f", ipd_rec->sql_desc_scale, (double)(*((SQLREAL *) ptr)));
					else if (type == SQL_C_NUMERIC)
						cnumtoasc((SQL_NUMERIC_STRUCT *) ptr, apd_rec->sql_desc_precision, apd_rec->sql_desc_scale, work);
					else if (type == SQL_C_TYPE_DATE) {
						sqltype = SQL_TYPE_DATE;
						sqldate = (SQL_DATE_STRUCT *) ptr;
						i1 = sqldate->year;
						for (i1 = 4, i2 = sqldate->year; i1 > 0; i2 /= 10) work[--i1] = (char)(i2 % 10 + '0');
						for (i1 = 7, i2 = sqldate->month; i1 > 5; i2 /= 10) work[--i1] = (char)(i2 % 10 + '0');
						for (i1 = 10, i2 = sqldate->day; i1 > 8; i2 /= 10) work[--i1] = (char)(i2 % 10 + '0');
						work[4] = work[7] = '-';
						work[10] = '\0';
					}
					else if (type == SQL_C_TYPE_TIME) {
						sqltype = SQL_TYPE_TIME;
						sqltime = (SQL_TIME_STRUCT *) ptr;
						for (i1 = 2, i2 = sqltime->hour; i1 > 0; i2 /= 10) work[--i1] = (char)(i2 % 10 + '0');
						for (i1 = 5, i2 = sqltime->minute; i1 > 3; i2 /= 10) work[--i1] = (char)(i2 % 10 + '0');
						for (i1 = 8, i2 = sqltime->second; i1 > 6; i2 /= 10) work[--i1] = (char)(i2 % 10 + '0');
						work[2] = work[5] = ':';
						work[8] = '\0';
					}
					else if (type == SQL_C_TYPE_TIMESTAMP) {
						sqltype = SQL_TYPE_TIMESTAMP;
						sqltimestamp = (SQL_TIMESTAMP_STRUCT *) ptr;
						for (i1 = 4, i2 = sqltimestamp->year; i1 > 0; i2 /= 10) work[--i1] = (char)(i2 % 10 + '0');
						for (i1 = 7, i2 = sqltimestamp->month; i1 > 5; i2 /= 10) work[--i1] = (char)(i2 % 10 + '0');
						for (i1 = 10, i2 = sqltimestamp->day; i1 > 8; i2 /= 10) work[--i1] = (char)(i2 % 10 + '0');
						for (i1 = 13, i2 = sqltimestamp->hour; i1 > 11; i2 /= 10) work[--i1] = (char)(i2 % 10 + '0');
						for (i1 = 16, i2 = sqltimestamp->minute; i1 > 14; i2 /= 10) work[--i1] = (char)(i2 % 10 + '0');
						for (i1 = 19, i2 = sqltimestamp->second; i1 > 17; i2 /= 10) work[--i1] = (char)(i2 % 10 + '0');
						/*** MS docs say that FRACTION is REPRESENTED by Billionths ***/
						for (i1 = TIMESTAMP_LENGTH, i2 = sqltimestamp->fraction/1000000; i1 > 20; i2 /= 10)
							work[--i1] = (char)(i2 % 10 + '0');
						work[4] = work[7] = '-';
						work[10] = ' ';
						work[13] = work[16] = ':';
						work[19] = '.';
						work[TIMESTAMP_LENGTH] = '\0';
					}
					else {
						sprintf((char*)errmsg, "%s: Binding parameters: Unexpected C data type: %d", TEXT_HY003, (int) apd_rec->sql_desc_concise_type);
						error_record(errors, "HY003", (char*)errmsg, NULL, 0);
						return SQL_ERROR;
					}
					ptr = work;
					parmlen = (INT)strlen(work);
				}
				else if (type == SQL_C_TYPE_DATE) sqltype = SQL_TYPE_DATE;
				else if (type == SQL_C_TYPE_TIME) sqltype = SQL_TYPE_TIME;
				else if (type == SQL_C_TYPE_TIMESTAMP) sqltype = SQL_TYPE_TIMESTAMP;
			}

			if (ipd_rec->sql_desc_concise_type == SQL_NUMERIC || ipd_rec->sql_desc_concise_type == SQL_DECIMAL) {
				if (sqltype != SQL_NUMERIC) {
					if (sqltype == SQL_CHAR) {
/*** CODE: VERIFY STRING TO BE VALID NUMERIC ***/
						sqltype = SQL_NUMERIC;
					}
					if (sqltype != SQL_NUMERIC) {
						sprintf((char*)errmsg, "%s: Binding parameters: Unable to convert C type %d to SQL type %d", TEXT_HYC00, type, (int) ipd_rec->sql_desc_concise_type);
						error_record(errors, "HYC00", (char*)errmsg, NULL, 0);
						return SQL_ERROR;
					}
				}
				if (parmlen) {
					memcpy(&workbuffer[worklen], ptr, parmlen);
					worklen += parmlen;
				}
				else {
					memcpy(&workbuffer[worklen], "NULL", 4 * sizeof(CHAR));
					worklen += 4 * sizeof(CHAR);
				}
			}
			else {
				if (ipd_rec->sql_desc_concise_type == SQL_TYPE_DATE) {
					if (sqltype != SQL_TYPE_DATE) {
						if (sqltype == SQL_CHAR) {
/*** CODE: VERIFY STRING TO BE A VALID DATE / TIMESTAMP ***/
							sqltype = SQL_TYPE_DATE;
						}
						if (sqltype == SQL_TYPE_TIMESTAMP) {
							if (parmlen) parmlen = 10;  /* truncate timestamp */
							sqltype = SQL_TYPE_DATE;
						}
						if (sqltype != SQL_TYPE_DATE) {
							sprintf((char*)errmsg, "%s: Binding parameters: Unable to convert C type %d to SQL type %d", TEXT_HYC00, type, (int) ipd_rec->sql_desc_concise_type);
							error_record(errors, "HYC00", (char*)errmsg, NULL, 0);
							return SQL_ERROR;
						}
					}
				}
				else if (ipd_rec->sql_desc_concise_type == SQL_TYPE_TIME) {
					if (sqltype != SQL_TYPE_TIME) {
						if (sqltype == SQL_CHAR) {
/*** CODE: VERIFY STRING TO BE A VALID TIME / TIMESTAMP ***/
							sqltype = SQL_TYPE_TIME;
						}
						if (sqltype == SQL_TYPE_TIMESTAMP) {
							if (parmlen) {  /* move time portion */
								memmove(work, work + 11, TIMESTAMP_LENGTH - 11);
								parmlen = TIMESTAMP_LENGTH - 11;
							}
							sqltype = SQL_TYPE_TIME;
						}
						if (sqltype != SQL_TYPE_TIME) {
							sprintf((char*)errmsg, "%s: Binding parameters: Unable to convert C type %d to SQL type %d", TEXT_HYC00, type, (int) ipd_rec->sql_desc_concise_type);
							error_record(errors, "HYC00", (char*)errmsg, NULL, 0);
							return SQL_ERROR;
						}
					}
				}
				else if (ipd_rec->sql_desc_concise_type == SQL_TYPE_TIMESTAMP) {
					if (sqltype != SQL_TYPE_TIMESTAMP) {
						if (sqltype == SQL_CHAR) {
/*** CODE: VERIFY STRING TO BE A VALID DATE / TIME / TIMESTAMP ***/
							sqltype = SQL_TYPE_TIMESTAMP;
						}
						if (sqltype == SQL_TYPE_DATE) {
							if (parmlen) {  /* append zero time */
								memcpy(work + 10, " 00:00:00", 9);
								parmlen = 19;
							}
							sqltype = SQL_TYPE_TIMESTAMP;
						}
						else if (sqltype == SQL_TYPE_TIME) {
							if (parmlen) {  /* prefix zero date */
								memmove(work + 11, work, 8);
								memcpy(work, "0000-00-00 ", 11);
								parmlen = 19;
							}
							sqltype = SQL_TYPE_TIMESTAMP;
						}
						if (sqltype != SQL_TYPE_TIMESTAMP) {
							sprintf((char*)errmsg, "%s: Binding parameters: Unable to convert C type %d to SQL type %d", TEXT_HYC00, type, (int) ipd_rec->sql_desc_concise_type);
							error_record(errors, "HYC00", (char*)errmsg, NULL, 0);
							return SQL_ERROR;
						}
					}
				}
				else if (ipd_rec->sql_desc_concise_type != SQL_CHAR && ipd_rec->sql_desc_concise_type != SQL_VARCHAR && ipd_rec->sql_desc_concise_type != SQL_LONGVARCHAR) {
					sprintf((char*)errmsg, "%s: Binding parameters: Unsupported SQL data type: %d", TEXT_HY003, (int) ipd_rec->sql_desc_concise_type);
					error_record(errors, "HY003", (char*)errmsg, NULL, 0);
					return SQL_ERROR;
				}
				if (sqltype == SQL_TYPE_TIMESTAMP || sqltype == SQL_TYPE_TIME || sqltype == SQL_TYPE_DATE) {
					/* check for escape clause in parameter */
					if (scanflag && parmlen) {
						if (*ptr == '{' && *(ptr + (parmlen - 1)) == '}') {
							/* i1 is number of characters to skip in front of data that is copied */
							if (parmlen >= 2 && *(ptr + 1) == 't' && *(ptr + 2) == 's') {
								i1 = 4;
								escapes[esccnt++].type = ESCAPE_TIMESTAMP;
							}
							else {
								i1 = 3;
								if (parmlen >= 1 && *(ptr + 1) == 't') escapes[esccnt++].type = ESCAPE_TIME;
								else if (parmlen >= 1 && *(ptr + 1) == 'd') escapes[esccnt++].type = ESCAPE_DATE;
								else escapes[esccnt++].type = ESCAPE_IGNORE;
							}
							escapes[esccnt].offset = pos;
							if (escapes[--esccnt].type != ESCAPE_IGNORE) {
								memcpy(&workbuffer[worklen], (CHAR *) (ptr + i1), (parmlen - i1) - 1);
								worklen += (parmlen - i1) - 1;
/*** CODE: IF DATE/TIME/TIMESTAMP ESCAPE, USE escapes[esccnt].offset TO GET VALUE TO CONVERT ***/
								mark = ++pos;
								continue;
							}
						}
					}
				}
				workbuffer[worklen++] = '\'';
				while (parmlen--) {
					if (*ptr == '\'' || *ptr == '\\') workbuffer[worklen++] = '\\';
					workbuffer[worklen++] = *ptr++;
				}
				workbuffer[worklen++] = '\'';
			}
			mark = ++pos;
			continue;
		}
		if (scanflag) {
			if (text[pos] == '{') {
				i1 = (INT)pos++;
				while ((pos < len && isspace(text[pos])) || text[pos] == '\n') pos++;
				for (i2 = (INT)pos; pos < len && isalpha(text[pos]); pos++);
				if (pos - i2 == 1) {
					if (tolower(text[i2]) == 'd') escapes[esccnt].type = ESCAPE_DATE;
					else if (tolower(text[i2]) == 't') escapes[esccnt].type = ESCAPE_TIME;
					else {
/*** CODE: ADD OTHER 1 BYTE ESCAPES HERE ***/
						escapes[esccnt++].type = ESCAPE_IGNORE;
						continue;
					}
				}
				else if (pos - i2 == 2) {
					if (!_memicmp("oj", (const void *)&text[i2], 2)) escapes[esccnt].type = ESCAPE_OJ;
					else if (!_memicmp("ts", (const void *)&text[i2], 2)) escapes[esccnt].type = ESCAPE_TIMESTAMP;
					else {
/*** CODE: ADD OTHER 2 BYTE ESCAPES HERE ***/
						escapes[esccnt++].type = ESCAPE_IGNORE;
						continue;
					}
				}
				else if (pos - i2 == 6) {
					if (!_memicmp("escape", (const void *)&text[i2], 6)) escapes[esccnt].type = ESCAPE_ESCAPE;
					else {
/*** CODE: ADD OTHER 6 BYTE ESCAPES HERE ***/
						escapes[esccnt++].type = ESCAPE_IGNORE;
						continue;
					}
				}
				else {
/*** CODE: ADD OTHER ESCAPES HERE ***/
					escapes[esccnt++].type = ESCAPE_IGNORE;
					continue;
				}
				memcpy(&workbuffer[worklen], &text[mark], i1 - mark);
				worklen += i1 - mark;
				escapes[esccnt++].offset = worklen;
				if (escapes[esccnt - 1].type == ESCAPE_ESCAPE) mark = pos - 6; /* keep 'escape' keyword */
				else mark = pos;
				continue;
			}
			if (text[pos] == '}') {
				if (esccnt) {
					if (escapes[--esccnt].type != ESCAPE_IGNORE) {
						memcpy(&workbuffer[worklen], &text[mark], pos - mark);
						worklen += pos - mark;
/*** CODE: IF DATE/TIME/TIMESTAMP ESCAPE, USE escapes[esccnt].offset TO GET VALUE TO CONVERT ***/
						mark = ++pos;
						continue;
					}
				}
			}
		}
		pos++;
	}
	if (worklen) {
		memcpy(&workbuffer[worklen], &text[mark], pos - mark);
		worklen += pos - mark;
		*textp = (UCHAR*)workbuffer;
		*textlen = (INT)worklen;
	}
	return SQL_SUCCESS;
#undef ESCAPE_IGNORE
#undef ESCAPE_OJ
#undef ESCAPE_DATE
#undef ESCAPE_TIME
#undef ESCAPE_TIMESTAMP
}

static int nextword(char *string, int length, int *offset, int *wordlen)
{
	int i1, i2, quoteflag;

	for (i1 = 0; i1 < length && (string[i1] == ' ' || string[i1] == '\t' || string[i1] == '\r' || string[i1] == '\n'); i1++);
	for (i2 = i1, quoteflag = FALSE; i2 < length; i2++) {
		if (string[i2] == '\\') {
			if (++i2 == length) break;
		}
		else if (string[i2] == '\'') {
			quoteflag = !quoteflag;
			continue;
		}
		if (!quoteflag && (string[i2] == ' ' || string[i2] == '\t' || string[i2] == '\r' || string[i2] == '\n')) break;
	}

	*offset = i1;
	*wordlen = i2 - i1;
	return i1 != i2;
}

static int findword(char *string1, int length1, char *string2, int length2, int *offset)
{
	int i1, chr, quoteflag;

	*offset = length2;
	length2 -= length1;
	chr = toupper(*string1);
	for (i1 = 0, quoteflag = FALSE; i1 <= length2; i1++) {
		if (string2[i1] == '\\') {
			if (++i1 > length2) break;
		}
		else if (string2[i1] == '\'') {
			quoteflag = !quoteflag;
			continue;
		}
		if (quoteflag || chr != toupper(string2[i1])) continue;
		if (!_memicmp(string1, &string2[i1], length1) && (i1 == length2 ||
		    string2[i1 + length1] == ' ' || string2[i1 + length1] == '\t' ||
		    string2[i1 + length1] == '\r' || string2[i1 + length1] == '\n')) {
			*offset = i1;
			return TRUE;
		}
	}
	return FALSE;
}

static void asctocnum(char *str, int precision, int scale, SQL_NUMERIC_STRUCT *num)
{
	int i1, i2, carry, decimal, left, right;
	unsigned short xwork[sizeof(num->val) / sizeof(unsigned short) + 1];

	while (*str == ' ') str++;
	if (*str == '-') num->sign = 0;
	else num->sign = 1;

	left = right = 0;
	decimal = FALSE;
	for (i1 = 0; str[i1]; i1++) {
		if (!isdigit(str[i1])) {
			if (str[i1] == '.') decimal = TRUE;
		}
		else if (decimal) right++;
		else left++;
	}
	if (precision < 0) precision = left + right;
	if (scale < 0) scale = right;
	if (precision < scale) precision = scale;
	for ( ; left > precision - scale; str++) {  /* data is being truncated */
		if (!isdigit(*str)) continue;
		left--;
	}

	right = 0;
	decimal = FALSE;
	memset(xwork, 0, sizeof(xwork));
	for (i1 = 0; *str; str++) {
		if (!isdigit(*str)) {
			if (*str == '.') decimal = TRUE;
			continue;
		}
		if (decimal) {
			if (right == scale) break;
			right++;
		}
		carry = *str - '0';
		for (i2 = 0; i2 < i1; i2++) {
			carry += (int) xwork[i2] * 10;
			xwork[i2] = (unsigned short) carry;
			carry >>= sizeof(*xwork) * 8;
		}
		if (carry && (size_t)i1 < sizeof(xwork) / sizeof(*xwork)) xwork[i1++] = (unsigned short) carry;
	}
	while (right++ < scale) {
		carry = 0;
		for (i2 = 0; i2 < i1; i2++) {
			carry += (int) xwork[i2] * 10;
			xwork[i2] = (unsigned short) carry;
			carry >>= sizeof(*xwork) * 8;
		}
		if (carry && (size_t)i1 < sizeof(xwork) / sizeof(*xwork)) xwork[i1++] = (unsigned short) carry;
	}
	memcpy(num->val, xwork, sizeof(num->val));
	num->precision = (unsigned char) precision;
	num->scale = (char) scale;
}

static void cnumtoasc(SQL_NUMERIC_STRUCT *num, int precision, int scale, char *str)
{
	int i1, i2, carry, pos, sign;
	unsigned short xwork[sizeof(num->val) / sizeof(unsigned short) + 1];
	char work[64];

	pos = sizeof(work);
	work[--pos] = 0;
	memset(xwork, 0, sizeof(xwork));
	memcpy(xwork, num->val, sizeof(num->val));
	carry = 0;
	for (i1 = sizeof(xwork) / sizeof(*xwork) - 1; i1 >= 0; ) {
		if (!xwork[i1]) {
			i1--;
			continue;
		}
		carry = 0;
		for (i2 = i1; i2 >= 0; i2--) {
			carry = (carry << (sizeof(*xwork) * 8)) + xwork[i2];
			xwork[i2] = (unsigned short)(carry / 10);
			carry %= 10;
		}
		work[--pos] = (char)(carry + '0');
		if ((unsigned) scale == sizeof(work) - 1 - pos) work[--pos] = '.';
	}
	sign = num->sign;
	if (!work[pos]) sign = 1;
	if ((unsigned) scale > sizeof(work) - 1 - pos) {
		while ((unsigned) scale > sizeof(work) - 1 - pos) work[--pos] = '0';
		work[--pos] = '.';
	}
	else if (!work[pos]) work[--pos] = '0';
	if (sign != 1) work[--pos] = '-';
#if 0
/*** CODE: LET SERVER MAKE ADJUSTMENTS ***/
	if (precision) {
		if (scale > precision) precision = scale;
		if (scale) precision++;
		if (sizeof(work) - 1 - pos > precision) pos = sizeof(work) - 1 - precision;
		/*** CODE: SHOULD WE PAD OUT FOR PRECISION WITH BLANKS ***/
	}
#endif
	strcpy(str, work + pos);
}

static INT char_to_int32(CHAR *str, INT len)
{
	INT i1, result, negflag;

	for (i1 = 0; i1 < len && str[i1] == ' '; i1++);
	if (i1 == len) return(0);

	if (str[i1] == '-')  {
		i1++;
		negflag = TRUE;
	}
	else negflag = FALSE;

	for (result = 0; i1 < len; i1++)
		if (isdigit(str[i1])) result = result * 10 + str[i1] - '0';

	if (negflag) result = -result;
	return(result);
}

static INT cvttodatetime(CHAR *src, INT srclen, INT desttype, CHAR *dest)
{
	INT i1, i2, i3, len;
	CHAR work[32];


	memcpy(work, "0000-00-00 00:00:00.000000", 26);
	while (srclen && src[srclen - 1] == ' ') srclen--;
	if (!srclen) {
		if (desttype == SQL_C_TYPE_DATE) len = 10;
		else if (desttype == SQL_C_TYPE_TIME) len = 12;
		else len = TIMESTAMP_LENGTH;
		memset(dest, ' ', len);
		return len;
	}
	for (i1 = i2 = 0; i1 < srclen && isdigit(src[i1]); i1++);
	/* get date information */
	if (i1 == 4) {
		if (i1 < srclen && src[i1] != '-') return -1;
		memcpy(work, src, 4);
		for (i2 = ++i1; i1 < srclen && isdigit(src[i1]); i1++);
		if (i1 != i2) {
			if (i1 - i2 != 2 || (i1 < srclen && src[i1] != '-')) return -1;
			memcpy(work + 5, src + i2, 2);
			for (i2 = ++i1; i1 < srclen && isdigit(src[i1]); i1++);
			if (i1 != i2) {
				if (i1 - i2 != 2 || (i1 < srclen && src[i1] != ' ')) return -1;
				memcpy(work + 8, src + i2, 2);
				for (i2 = ++i1; i1 < srclen && isdigit(src[i1]); i1++);
			}
		}
	}
	/* get time information */
	if (i1 != i2) {
		if (i1 - i2 != 2 || (i1 < srclen && src[i1] != ':')) return -1;
		memcpy(work + 11, src + i2, 2);
		for (i2 = ++i1; i1 < srclen && isdigit(src[i1]); i1++);
		if (i1 != i2) {
			if (i1 - i2 != 2 || (i1 < srclen && src[i1] != ':')) return -1;
			memcpy(work + 14, src + i2, 2);
			for (i2 = ++i1; i1 < srclen && isdigit(src[i1]); i1++);
			if (i1 != i2) {
				if (i1 - i2 != 2 || (i1 < srclen && src[i1] != '.')) return -1;
				memcpy(work + 17, src + i2, 2);
				for (i2 = ++i1; i1 < srclen && isdigit(src[i1]); i1++);
				if (i1 != i2) {
					i3 = i1 - i2;
					if (i3 > 3) i3 = 3;
					memcpy(work + 20, src + i2, i3);
				}
			}
		}
	}
	if (i1 < srclen) return -1;
	if (desttype == SQL_C_TYPE_DATE) {
		memcpy(dest, work, 10);
		len = 10;
	}
	else if (desttype == SQL_C_TYPE_TIME) {
		memcpy(dest, work + 11, 12);
		len = 12;
	}
	else {
		memcpy(dest, work, TIMESTAMP_LENGTH);
		len = TIMESTAMP_LENGTH;
	}
	return len;
}
