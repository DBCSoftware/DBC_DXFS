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
#define INC_IPV6
#include "includes.h"
#include "base.h"
#include "fsodbc.h"
#include "fsomem.h"
#include "tcp.h"

#if OS_UNIX
#define _strnicmp(a, b, c) strncasecmp(a, b, c)
#define _memicmp(a, b, c) strncasecmp(a, b, c)
#define _itoa(a, b, c) mscitoa(a, b)
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#endif

#define MAX_DATA_LENGTH 4		/* maximum number of digits including '-' of a SQL_ data type */
#define VENDOR_ID "[DB/C Software]"
#define ODBC_ID "[FS101 ODBC Driver]"
#if OS_WIN32
extern INT ConfigFileDSN(SQLHWND hwnd, CHAR *lpszAttributes);
#endif

/* note: access to environments must be protected with entersync/exitsync */
static SQLINTEGER environments = 0;
/*** CODE: WORKBUFFER MAY NEED TO BE DYNAMIC, BUT ASSUME IT IS BIG ENOUGH FOR NOW ***/
/* note: access to workbuffer must be protected with entersync/exitsync */
CHAR workbuffer[65536];

static SQLRETURN setDescType(SQLSMALLINT Value, INT descType, LPVOID drec, ERRORS *er);
static SQLRETURN SQL_API fsFetchScroll(SQLHSTMT StatementHandle, SQLSMALLINT FetchOrientation,
		SQLLEN FetchOffset);
static SQLRETURN SQL_API fsAllocStmt(SQLHANDLE, SQLHANDLE *);
static BOOL consistency_check(DESCRIPTOR *, LPVOID);
static INT hide_escapes(CHAR *output,  CHAR *input, INT Length);

/**
 * This odd looking thing is to avoid compiler warnings on some 64-bit builds when
 * a SQLPOINTER need to be cast to a SQLUINTEGER.
 */
#if defined(__PPC__) || __SIZEOF_LONG__ == 8 || defined(_WIN64) || defined(_LP64)
#define SQLPTR_TO_SQLUINT (SQLUINTEGER) (unsigned long long)
#define SQLPTR_TO_SQLINT (SQLINTEGER) (unsigned long long)
#else
#define SQLPTR_TO_SQLUINT (SQLUINTEGER)
#define SQLPTR_TO_SQLINT (SQLINTEGER)
#endif

/**
 *  Allocates an environment, connection, statement, or descriptor handle.
 */
SQLRETURN SQL_API SQLAllocHandle(
	SQLSMALLINT HandleType,
	SQLHANDLE InputHandle,
	SQLHANDLE *OutputHandlePtr)
{
#if OS_WIN32
	WSADATA versioninfo;
#endif
	ENVIRONMENT *env;
	struct client_connection_struct *cnct;
	DESCRIPTOR *desc;

	switch (HandleType) {
	case SQL_HANDLE_ENV: /* 1 */
		entersync();
		env = (ENVIRONMENT *) allocmem(sizeof(ENVIRONMENT), 1);
		if (env == NULL) {
			exitsync();
			return SQL_ERROR;
		}
#if OS_WIN32
		__try {
#endif
		if (environments == 0) {
#if OS_WIN32
			/* setup winsock */
			if (WSAStartup(MAKEWORD(2,2), &versioninfo)) {
				freemem(env);
				exitsync();
				return SQL_ERROR;
			}
#endif
		}
		environments++;
#if OS_WIN32
		}
		__except( EXCEPTION_EXECUTE_HANDLER ) {
			exitsync();
			return SQL_ERROR;
		}
#endif
		env->sql_attr_odbc_version = 0;
		*OutputHandlePtr = (SQLHANDLE) env;
		exitsync();
		break;
	case SQL_HANDLE_DBC: /* 2 */
		env = (ENVIRONMENT *) InputHandle;
		error_init(&env->errors);
		if (env->sql_attr_odbc_version == 0) {
			error_record(&env->errors, "HY010", TEXT_HY010,
					"application must first call SQLSetEnvAttr with SQL_ATTR_ODBC_VERSION", 0);
			*OutputHandlePtr = SQL_NULL_HDBC;
			return SQL_ERROR;
		}
		/**
		 * The below is a struct client_connection_struct
		 * defined in fsodbc.h
		 */
		entersync();
		cnct = (CONNECTION *) allocmem(sizeof(CONNECTION), 1);
		if (cnct == NULL) {
			error_record(&env->errors, "HY001", TEXT_HY001_CNCT, NULL, 0);
			*OutputHandlePtr = SQL_NULL_HDBC;
			exitsync();
			return SQL_ERROR;
		}
		cnct->env = env;
		memcpy(cnct->cnid, EMPTY, FIELD_LENGTH);
		/* connection attributes */
		cnct->sql_attr_access_mode = SQL_MODE_READ_WRITE;
		cnct->sql_attr_async_enable = SQL_ASYNC_ENABLE_OFF;
/*** CODE: SUPPORT AUTO_IPD=TRUE ***/
		cnct->sql_attr_auto_ipd = SQL_FALSE;
		cnct->sql_attr_autocommit = SQL_AUTOCOMMIT_ON;
/*** CODE: MAYBE THIS SHOULD BE TRUE UNTIL THE CONNECTION IS SUCCESSFUL ***/
		cnct->sql_attr_connection_dead = SQL_CD_FALSE;
		/* cnct->sql_attr_connection_timeout = 0; */
		/* cnct->sql_attr_current_catalog = NULL; */
		cnct->sql_attr_login_timeout= SQL_LOGIN_TIMEOUT_DEFAULT;
		cnct->sql_attr_metadata_id = SQL_FALSE;
		/* cnct->sql_attr_packet_size = 0; */
		/* cnct->sql_attr_quiet_mode = NULL; */
		/* cnct->sql_attr_translate_lib = NULL; */
		/* cnct->sql_attr_translate_option = 0; */
		/* cnct->sql_attr_txn_isolation = 0; */
		*OutputHandlePtr = (SQLHANDLE) cnct;
		exitsync();
		break;
	case SQL_HANDLE_STMT: /* 3 */
		return fsAllocStmt(InputHandle, OutputHandlePtr);
	case SQL_HANDLE_DESC:
		cnct = (CONNECTION *) InputHandle;
		error_init(&cnct->errors);
		entersync();
		desc = (DESCRIPTOR *) allocmem(sizeof(DESCRIPTOR), 1);
		if (desc == NULL) {
			error_record(&cnct->errors, "HY001", TEXT_HY001_DESC, NULL, 0);
			*OutputHandlePtr = SQL_NULL_HDESC;
			exitsync();
			return SQL_ERROR;
		}
		desc->cnct = cnct;
		desc->type = TYPE_APP_DESC;
		/* application descriptor attibutes */
		desc->sql_desc_alloc_type = SQL_DESC_ALLOC_USER;
		desc->sql_desc_array_size = 1;
		/* desc->sql_desc_array_status_ptr = NULL; */
		/* desc->sql_desc_bind_offset_ptr = NULL; */
		desc->sql_desc_bind_type = SQL_BIND_BY_COLUMN;
		/* desc->sql_desc_count = 0; */

#if OS_WIN32
		__try {
			desc->nextexplicitdesc = cnct->firstexplicitdesc;
			cnct->firstexplicitdesc = desc;
		}
		__except( EXCEPTION_EXECUTE_HANDLER )
		{
			int i1 = GetExceptionCode();
			exitsync();
			error_record(&cnct->errors, "HY000", TEXT_HY000, "In SQLAllocHandle, SQL_HANDLE_DESC", i1);
			return SQL_ERROR;
		}
#else
		desc->nextexplicitdesc = cnct->firstexplicitdesc;
		cnct->firstexplicitdesc = desc;
#endif
		exitsync();
		*OutputHandlePtr = (SQLHANDLE) desc;
		break;
	}

	return SQL_SUCCESS;
}

#if OS_UNIX
/* unixODBC is calling this deprecated function for some reason */
SQLRETURN SQL_API SQLAllocStmt(
	SQLHANDLE InputHandle,
	SQLHANDLE *OutputHandlePtr)
{
/*** NOTE: call separate fsAllocStmt function to not break unixODBC driver ***/
	return fsAllocStmt(InputHandle, OutputHandlePtr);
}
#endif

static SQLRETURN SQL_API fsAllocStmt(
	SQLHANDLE InputHandle,
	SQLHANDLE *OutputHandlePtr)
{
	struct client_connection_struct *cnct;
	STATEMENT *stmt;
	DESCRIPTOR *apd, *ard, *ipd, *ird;

	entersync();
	cnct = (CONNECTION *) InputHandle;
	error_init(&cnct->errors);
	stmt = (STATEMENT *) allocmem(sizeof(STATEMENT), 1);
	apd = (DESCRIPTOR *) allocmem(sizeof(DESCRIPTOR), 1);
	ard = (DESCRIPTOR *) allocmem(sizeof(DESCRIPTOR), 1);
	ipd = (DESCRIPTOR *) allocmem(sizeof(DESCRIPTOR), 1);
	ird = (DESCRIPTOR *) allocmem(sizeof(DESCRIPTOR), 1);
	if (stmt == NULL || apd == NULL || ard == NULL || ipd == NULL || ird == NULL) {
		freemem(ird);
		freemem(ipd);
		freemem(ard);
		freemem(apd);
		freemem(stmt);
		error_record(&cnct->errors, "HY001", TEXT_HY001_STMT, NULL, 0);
		*OutputHandlePtr = SQL_NULL_HSTMT;
		exitsync();
		return SQL_ERROR;
	}
	stmt->cnct = cnct;
	stmt->row_count = -1;
	/* stmt->row_number = 0; */
	memcpy(stmt->rsid, EMPTY, FIELD_LENGTH);
	stmt->apdsave = apd;
	stmt->ardsave = ard;
	/* statement attributes */
	stmt->sql_attr_app_param_desc = apd;
	stmt->sql_attr_app_row_desc = ard;
	stmt->sql_attr_async_enable = SQL_ASYNC_ENABLE_OFF;
	stmt->sql_attr_concurrency = SQL_CONCUR_READ_ONLY;
	stmt->sql_attr_cursor_scrollable = SQL_NONSCROLLABLE;
	stmt->sql_attr_cursor_sensitivity = SQL_UNSPECIFIED;
	stmt->sql_attr_cursor_type = SQL_CURSOR_FORWARD_ONLY;
/*** NOTE: CONFLICT IN DOCUMENTATION, POSSIBLY SHOULD EQUAL cnct->sql_attr_auto_ipd ***/
	stmt->sql_attr_enable_auto_ipd = SQL_FALSE;
	/* stmt->sql_attr_fetch_bookmark_ptr = NULL; */
	stmt->sql_attr_imp_param_desc = ipd;
	stmt->sql_attr_imp_row_desc = ird;
	/* stmt->sql_attr_keyset_size = 0; */
	/* stmt->sql_attr_max_length = 0; */
	/* stmt->sql_attr_max_rows = 0; */
	stmt->sql_attr_metadata_id = SQL_FALSE;
	stmt->sql_attr_noscan = SQL_NOSCAN_OFF;
	/* stmt->sql_attr_paramset_size = 0; */
	/* stmt->sql_attr_query_timeout = 0; */
	stmt->sql_attr_retrieve_data = SQL_RD_ON;
	/* stmt->sql_attr_row_number = 0; */
/*** CODE: THIS MAY CHANGE IF POSITION UPDATES/DELETE CAN BE EMULATED CORRECTLY ***/
	stmt->sql_attr_simulate_cursor = SQL_SC_NON_UNIQUE;
	stmt->sql_attr_use_bookmarks = SQL_UB_OFF;

	apd->type = TYPE_APP_DESC;
	/* application parameter descriptor attibutes */
	apd->sql_desc_alloc_type = SQL_DESC_ALLOC_AUTO;
	apd->sql_desc_array_size = 1;
	/* apd->sql_desc_array_status_ptr = NULL; */
	/* apd->sql_desc_bind_offset_ptr = NULL; */
	apd->sql_desc_bind_type = SQL_BIND_BY_COLUMN;
	/* apd->sql_desc_count = 0; */

	ard->type = TYPE_APP_DESC;
	/* application row descriptor attibutes */
	ard->sql_desc_alloc_type = SQL_DESC_ALLOC_AUTO;
	ard->sql_desc_array_size = 1;
	/* ard->sql_desc_array_status_ptr = NULL; */
	/* ard->sql_desc_bind_offset_ptr = NULL; */
	ard->sql_desc_bind_type = SQL_BIND_BY_COLUMN;
	/* ard->sql_desc_count = 0; */

	ipd->type = TYPE_IMP_PARAM_DESC;
	/* implementation parameter descriptor attibutes */
	ipd->sql_desc_alloc_type = SQL_DESC_ALLOC_AUTO;
	/* ipd->sql_desc_array_status_ptr = NULL; */
	/* ipd->sql_desc_count = 0; */
	/* ipd->sql_desc_rows_processed_ptr = NULL; */

	ird->stmt = stmt;
	ird->type = TYPE_IMP_ROW_DESC;
	/* implementation row descriptor attibutes */
	ird->sql_desc_alloc_type = SQL_DESC_ALLOC_AUTO;
	/* ird->sql_desc_array_status_ptr = NULL; */
	/* ird->sql_desc_count = 0; */
	/* ird->sql_desc_rows_processed_ptr = NULL; */

#if OS_WIN32
	__try {
		stmt->nextstmt = cnct->firststmt;
		cnct->firststmt = stmt;
	}
	__except ( EXCEPTION_EXECUTE_HANDLER ) {
		int i1 = GetExceptionCode();
		exitsync();
		error_record(&cnct->errors, "HY000", TEXT_HY000, "In fsAllocStmt", i1);
		return SQL_ERROR;
	}
#else
	stmt->nextstmt = cnct->firststmt;
	cnct->firststmt = stmt;
#endif
	*OutputHandlePtr = (SQLHANDLE) stmt;
	exitsync();
	return SQL_SUCCESS;
}

/**
 * Binds application data buffers to columns in the result set.
 */
SQLRETURN SQL_API SQLBindCol(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT ColumnNumber,
	SQLSMALLINT TargetType,
	SQLPOINTER TargetValuePtr,
	SQLLEN BufferLength,
	SQLLEN *StrLen_or_Ind)
{
	CHAR errmsg[64];
	STATEMENT *stmt;
	DESCRIPTOR *ard;
	APP_DESC_RECORD *ard_rec;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	if (ColumnNumber == 0) {
		if (stmt->sql_attr_use_bookmarks == SQL_UB_OFF) {
			error_record(&stmt->errors, "HYC00", TEXT_HYC00, "bookmarks not supported", 0);
			return SQL_ERROR;
		}
/*** FUTURE CODE GOES HERE TO SUPPORT BOOKMARKS */
		error_record(&stmt->errors, "HYC00", TEXT_HYC00, "bookmarks not supported", 0);
		return SQL_ERROR;
	}

	entersync();
	ard = stmt->sql_attr_app_row_desc;
	if (TargetValuePtr == NULL && StrLen_or_Ind == NULL) {  /* unbind the column */
		if (desc_unbind(ard, ColumnNumber) == SQL_ERROR) {  /* programming error if this happens */
			error_record(&stmt->errors, "HY000", TEXT_HY000, "desc_unbind failed", 0);
			exitsync();
			return SQL_ERROR;
		}
		exitsync();
		return SQL_SUCCESS;
	}

	if (TargetType != SQL_C_CHAR && TargetType != SQL_C_DEFAULT &&
	    TargetType != SQL_C_LONG && TargetType != SQL_C_SLONG && TargetType != SQL_C_ULONG &&
	    TargetType != SQL_C_SHORT && TargetType != SQL_C_SSHORT && TargetType != SQL_C_USHORT &&
	    TargetType != SQL_C_TINYINT && TargetType != SQL_C_STINYINT && TargetType != SQL_C_UTINYINT &&
	    TargetType != SQL_C_DOUBLE && TargetType != SQL_C_FLOAT && TargetType != SQL_C_NUMERIC &&
	    TargetType != SQL_C_TYPE_DATE && TargetType != SQL_C_TYPE_TIME && TargetType != SQL_C_TYPE_TIMESTAMP) {
		sprintf(errmsg, "unsupported C data type: %d", (INT) TargetType);
		error_record(&stmt->errors, "HYC00", TEXT_HYC00, errmsg, 0);
		exitsync();
		return SQL_ERROR;
	}
	ard_rec = (APP_DESC_RECORD *) desc_get(ard, ColumnNumber, TRUE);
	if (ard_rec == NULL) {
		error_record(&stmt->errors, "HY001", TEXT_HY001, NULL, 0);
		exitsync();
		return SQL_ERROR;
	}
	if (ColumnNumber > ard->sql_desc_count) ard->sql_desc_count = ColumnNumber;

	if (TargetType == SQL_C_TYPE_DATE) {
		ard_rec->sql_desc_type = SQL_DATETIME;
		ard_rec->sql_desc_concise_type = SQL_TYPE_DATE;
		ard_rec->sql_desc_datetime_interval_code = SQL_CODE_DATE;
		ard_rec->sql_desc_length = 10;
		ard_rec->sql_desc_datetime_interval_precision = 0;
	}
	else if (TargetType == SQL_C_TYPE_TIME) {
		ard_rec->sql_desc_type = SQL_DATETIME;
		ard_rec->sql_desc_concise_type = SQL_TYPE_TIME;
		ard_rec->sql_desc_datetime_interval_code = SQL_CODE_TIME;
		ard_rec->sql_desc_length = 8;
		ard_rec->sql_desc_precision = 0;
		ard_rec->sql_desc_datetime_interval_precision = 0;
	}
	else if (TargetType == SQL_C_TYPE_TIMESTAMP) {
		ard_rec->sql_desc_type = SQL_DATETIME;
		ard_rec->sql_desc_concise_type = SQL_TYPE_TIMESTAMP;
		ard_rec->sql_desc_datetime_interval_code = SQL_CODE_TIMESTAMP;
		ard_rec->sql_desc_length = 22;
		ard_rec->sql_desc_precision = 2;
/*** CODE: SHOULD THIS BE 2 ? ***/
		ard_rec->sql_desc_datetime_interval_precision = 0;
	}
	else {
		ard_rec->sql_desc_concise_type = ard_rec->sql_desc_type = TargetType;
		ard_rec->sql_desc_datetime_interval_precision = 0;
		if (TargetType == SQL_C_CHAR) ard_rec->sql_desc_length = (SQLUINTEGER)BufferLength;
		else if (TargetType == SQL_C_NUMERIC) {
/*** CODE: SQL_DESC_PRECISION field is set to a driver-defined default precision (page 1208) ***/
			ard_rec->sql_desc_precision = 31;
			ard_rec->sql_desc_scale = 0;
		}
		else {
			ard_rec->sql_desc_precision = (SQLSMALLINT) BufferLength;
			ard_rec->sql_desc_scale = (SQLSMALLINT) BufferLength;
		}
	}
	ard_rec->sql_desc_octet_length = (SQLINTEGER)BufferLength;
	ard_rec->sql_desc_data_ptr = TargetValuePtr;
	ard_rec->sql_desc_octet_length_ptr = ard_rec->sql_desc_indicator_ptr = StrLen_or_Ind;
	if (ard_rec->dataatexecptr != NULL) {
		freemem(ard_rec->dataatexecptr);
		ard_rec->dataatexecptr = NULL;
	}
	exitsync();
	return SQL_SUCCESS;
}

/**
 * Binds a buffer to a parameter marker in an SQL statement.
 * SQLBindParameter supports binding to a Unicode C data type, even if the underlying driver does not support Unicode data.
 */
SQLRETURN SQL_API SQLBindParameter(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT ParameterNumber,
	SQLSMALLINT InputOutputType, /* <SQL_PARAM_INPUT> */
	SQLSMALLINT ValueType,		 /* <SQL_C_CHAR> */
	SQLSMALLINT ParameterType,	 /* <SQL_NUMERIC> */
	SQLULEN ColumnSize,			 /* 31 */
	SQLSMALLINT DecimalDigits,	 /* 30 */
	SQLPOINTER ParameterValuePtr,
	SQLLEN BufferLength,
	SQLLEN *StrLen_or_IndPtr)
{
	UCHAR errmsg[MAX_ERROR_MSG];
	STATEMENT *stmt;
	DESCRIPTOR *apd, *ipd;
	APP_DESC_RECORD *apd_rec;
	IMP_PARAM_DESC_RECORD *ipd_rec;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	/* if called from 1.0 application (SQLSetParam); assume only input */
	if (InputOutputType == SQL_PARAM_INPUT_OUTPUT && BufferLength == SQL_SETPARAM_VALUE_MAX)
		InputOutputType = SQL_PARAM_INPUT;

	/* We support only input type parameters */
	if (InputOutputType != SQL_PARAM_INPUT) {
		sprintf((char*)errmsg, "Binding parameters: Unsupported Parameter type: %d", (int) ParameterNumber);
		error_record(&stmt->errors, "HY105", TEXT_HY105, (char*)errmsg, 0);
		return SQL_ERROR;
	}

	if (ParameterNumber == 0) {
		error_record(&stmt->errors, "07009", TEXT_07009, NULL, 0);
		return SQL_ERROR;
	}

	if (ValueType != SQL_C_CHAR && ValueType != SQL_C_DEFAULT &&
	    ValueType != SQL_C_LONG && ValueType != SQL_C_SLONG && ValueType != SQL_C_ULONG &&
	    ValueType != SQL_C_SHORT && ValueType != SQL_C_SSHORT && ValueType != SQL_C_USHORT &&
	    ValueType != SQL_C_TINYINT && ValueType != SQL_C_STINYINT && ValueType != SQL_C_UTINYINT &&
	    ValueType != SQL_C_DOUBLE && ValueType != SQL_C_FLOAT && ValueType != SQL_C_NUMERIC &&
	    ValueType != SQL_C_TYPE_DATE && ValueType != SQL_C_TYPE_TIME &&
		ValueType != SQL_C_TYPE_TIMESTAMP) {
		sprintf((char*)errmsg, "Binding parameters: Unsupported C data type: %d", (int) ValueType);
		error_record(&stmt->errors, "HY003", TEXT_HY003, (char*)errmsg, 0);
		return SQL_ERROR;
	}

	/* We support only CHAR, VARCHAR, LONGVARCHAR, NUMERIC, and DECIMAL sql types */
	if (ParameterType != SQL_CHAR && ParameterType != SQL_VARCHAR &&
	    ParameterType != SQL_LONGVARCHAR &&
	    ParameterType != SQL_NUMERIC && ParameterType != SQL_DECIMAL &&
	    ParameterType != SQL_TYPE_DATE && ParameterType != SQL_TYPE_TIME &&
	    ParameterType != SQL_TYPE_TIMESTAMP) {
		sprintf((char*)errmsg, "Binding parameters: Unsupported SQL data type: %d", (int) ParameterType);
		error_record(&stmt->errors, "HY004", TEXT_HY004, (char*)errmsg, 0);
		return SQL_ERROR;
	}

 	if (ParameterValuePtr == NULL && *StrLen_or_IndPtr != SQL_NULL_DATA &&
		*StrLen_or_IndPtr != SQL_DATA_AT_EXEC) {
		error_record(&stmt->errors, "HY009", TEXT_HY009, "Invalid value of StrLen_or_Ind", 0);
		return SQL_ERROR;
	}

 	entersync();
	apd = stmt->sql_attr_app_param_desc;
	apd_rec = (APP_DESC_RECORD *) desc_get(apd, ParameterNumber, TRUE);
	if (apd_rec == NULL) {
		error_record(&stmt->errors, "HY001", TEXT_HY001, NULL, 0);
		exitsync();
		return SQL_ERROR;
	}
	if (ParameterNumber > apd->sql_desc_count) apd->sql_desc_count = ParameterNumber;

	switch (ValueType) {
	case SQL_C_TYPE_DATE:
		apd_rec->sql_desc_type = SQL_DATETIME;
		apd_rec->sql_desc_datetime_interval_code = SQL_CODE_DATE;
		break;
	case SQL_C_TYPE_TIME:
		apd_rec->sql_desc_type = SQL_DATETIME;
		apd_rec->sql_desc_datetime_interval_code = SQL_CODE_TIME;
		break;
	case SQL_C_TYPE_TIMESTAMP:
		apd_rec->sql_desc_type = SQL_DATETIME;
		apd_rec->sql_desc_datetime_interval_code = SQL_CODE_TIMESTAMP;
		break;
/*** CODE: NOT SUPPORTING INTERVAL TYPE (PAGE 926) ***/
	case SQL_C_NUMERIC:
/*** CODE: SQL_DESC_PRECISION field is set to a driver-defined default precision (page 1208) ***/
		apd_rec->sql_desc_precision = 31;
		apd_rec->sql_desc_scale = 0;
		//no break here
	default:
		apd_rec->sql_desc_type = ValueType;
		apd_rec->sql_desc_datetime_interval_code = 0;
		break;
	}
	apd_rec->sql_desc_concise_type = ValueType;
	apd_rec->sql_desc_data_ptr = ParameterValuePtr;
	apd_rec->sql_desc_octet_length = (SQLINTEGER)BufferLength;
	apd_rec->sql_desc_octet_length_ptr = apd_rec->sql_desc_indicator_ptr = StrLen_or_IndPtr;
	if (apd_rec->dataatexecptr != NULL) {
		freemem(apd_rec->dataatexecptr);
		apd_rec->dataatexecptr = NULL;
	}

	ipd = stmt->sql_attr_imp_param_desc;
	ipd_rec = (IMP_PARAM_DESC_RECORD *) desc_get(ipd, ParameterNumber, TRUE);
	if (ipd_rec == NULL) {
		error_record(&stmt->errors, "HY001", TEXT_HY001, NULL, 0);
		exitsync();
		return SQL_ERROR;
	}
	if (ParameterNumber > ipd->sql_desc_count) ipd->sql_desc_count = ParameterNumber;

	switch (ParameterType) {
	case SQL_TYPE_DATE:
		ipd_rec->sql_desc_type = SQL_DATETIME;
		ipd_rec->sql_desc_datetime_interval_code = SQL_CODE_DATE;
		break;
	case SQL_TYPE_TIME:
		ipd_rec->sql_desc_type = SQL_DATETIME;
		ipd_rec->sql_desc_datetime_interval_code = SQL_CODE_TIME;
		break;
	case SQL_TYPE_TIMESTAMP:
		ipd_rec->sql_desc_type = SQL_DATETIME;
		ipd_rec->sql_desc_datetime_interval_code = SQL_CODE_TIMESTAMP;
		break;
/*** CODE: NOT SUPPORTING INTERVAL TYPE (PAGE 926) ***/
	default:
		ipd_rec->sql_desc_type = ParameterType;
		ipd_rec->sql_desc_datetime_interval_code = 0;
		break;
	}
	ipd_rec->sql_desc_concise_type = ParameterType;
	/* store columnsize */
	if (ParameterType == SQL_CHAR || ParameterType == SQL_VARCHAR ||
	    ParameterType == SQL_LONGVARCHAR || ParameterType == SQL_BINARY ||
	    ParameterType == SQL_VARBINARY || ParameterType == SQL_LONGVARBINARY)
		ipd_rec->sql_desc_length = (SQLUINTEGER)ColumnSize;
	else if (ParameterType == SQL_DECIMAL || ParameterType == SQL_NUMERIC ||
	    ParameterType == SQL_FLOAT || ParameterType == SQL_REAL ||
	    ParameterType == SQL_DOUBLE) ipd_rec->sql_desc_precision = (short) ColumnSize;
	/* store decimaldigits */
	if (ParameterType == SQL_DECIMAL || ParameterType == SQL_NUMERIC)
		ipd_rec->sql_desc_scale = DecimalDigits;
	else if (ParameterType == SQL_TYPE_TIME || ParameterType == SQL_TYPE_TIMESTAMP ||
	    ParameterType == SQL_INTERVAL_SECOND || ParameterType == SQL_INTERVAL_DAY_TO_SECOND ||
	    ParameterType == SQL_INTERVAL_HOUR_TO_SECOND || ParameterType == SQL_INTERVAL_MINUTE_TO_SECOND)
		ipd_rec->sql_desc_precision = DecimalDigits;
	exitsync();
	return SQL_SUCCESS;
}

#if OS_UNIX
/*** CODE: DO NOT SUPPORT INITIALLY ***/
SQLRETURN SQL_API SQLBrowseConnect(
	SQLHDBC ConnectionHandle,
	SQLCHAR *InConnectionString,
	SQLSMALLINT StringLength1,
	SQLCHAR *OutConnectionString,
	SQLSMALLINT BufferLength,
	SQLSMALLINT *StringLength2Ptr)
{
	return SQL_SUCCESS;
}
#endif

#if 0
/*** CODE: DO NOT SUPPORT INITIALLY ***/
SQLRETURN SQL_API SQLBulkOperations(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT Operation)
{
	return SQL_SUCCESS;
}
#endif

SQLRETURN SQL_API SQLCancel(
	SQLHSTMT StatementHandle)
{
	STATEMENT *stmt;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

/*** CODE: MAYBE IF 2.X APP, PERFORM A DROP OR CLOSE CURSOR ***/
	stmt->executeflags &= ~EXECUTE_NEEDDATA;
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLCloseCursor(
	SQLHSTMT StatementHandle)
{
	SQLRETURN rc;
	STATEMENT *stmt;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	rc = SQL_SUCCESS;
	if (!(stmt->executeflags & EXECUTE_RESULTSET)) {  /* no cursor opened, this is a ODBC 3 driver to give error */
		error_record(&stmt->errors, "24000", TEXT_24000, NULL, 0);
		rc = SQL_ERROR;
	}
	if (stmt->executeflags & EXECUTE_RSID) {
		entersync();
		if (server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD,
			NULL, 0, &stmt->errors, NULL, NULL) != SERVER_OK) {
			exitsync();
			rc = SQL_ERROR;
		}
		exitsync();
	}
	stmt->executeflags &= EXECUTE_PREPARED;
	stmt->row_count = -1;
/*** CODE: SHOULD ROW_BUFFER BE FREED HERE OR WAIT TO FREEHANDLE ? ***/
/*	freemem(stmt->row_buffer); */
/*	stmt->row_buffer = NULL; */
	return rc;
}

SQLRETURN SQL_API SQLColAttribute(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT ColumnNumber,
	SQLUSMALLINT FieldIdentifier,
	SQLPOINTER CharacterAttributePtr,
	SQLSMALLINT BufferLength,
	SQLSMALLINT *StringLengthPtr,
#ifdef _WIN64
	SQLLEN *NumericAttributePtr)
#else
	SQLPOINTER NumericAttributePtr)
#endif
{
	SQLSMALLINT length;
	SQLINTEGER integer;
	SQLRETURN rc;
	SQLCHAR *sqlchar;
	STATEMENT *stmt;
	DESCRIPTOR *ird;
	IMP_ROW_DESC_RECORD *ird_rec;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);
	ird = stmt->sql_attr_imp_row_desc;

	rc = SQL_SUCCESS;
	sqlchar = NULL;
	entersync();
	if (FieldIdentifier == SQL_DESC_COUNT) integer = ird->sql_desc_count;
	else {
		if (ColumnNumber == 0) {
			error_record(&stmt->errors, "07009", TEXT_07009, "Bookmarks are not supported", 0);
			exitsync();
			return SQL_ERROR;
		}

		ird_rec = (IMP_ROW_DESC_RECORD *) desc_get(ird, ColumnNumber, FALSE);
		switch (FieldIdentifier) {
		case SQL_DESC_AUTO_UNIQUE_VALUE:
			integer = ird_rec->sql_desc_auto_unique_value;
			break;
		case SQL_DESC_BASE_COLUMN_NAME:
			sqlchar = ird_rec->sql_desc_base_column_name;
			break;
		case SQL_DESC_BASE_TABLE_NAME:
			sqlchar = ird_rec->sql_desc_base_table_name;
			break;
		case SQL_DESC_CASE_SENSITIVE:
			integer = ird_rec->sql_desc_case_sensitive;
			break;
		case SQL_DESC_CATALOG_NAME:
			sqlchar = ird_rec->sql_desc_catalog_name;
			break;
		case SQL_DESC_CONCISE_TYPE:
			integer = ird_rec->sql_desc_concise_type;
			break;
		case SQL_DESC_DISPLAY_SIZE:
			integer = ird_rec->sql_desc_display_size;
			break;
		case SQL_DESC_FIXED_PREC_SCALE:
			integer = ird_rec->sql_desc_fixed_prec_scale;
			break;
		case SQL_DESC_LABEL:
			sqlchar = ird_rec->sql_desc_label;
			break;
		case SQL_DESC_LENGTH:
			integer = ird_rec->sql_desc_length;
			break;
		case SQL_COLUMN_LENGTH:  /* ODBC 2.0 compatability */
			if (ird_rec->sql_desc_concise_type == SQL_NUMERIC) integer = ird_rec->sql_desc_precision + 2;
			else integer = ird_rec->sql_desc_octet_length;
			break;
		case SQL_DESC_LITERAL_PREFIX:
			sqlchar = ird_rec->sql_desc_literal_prefix;
			break;
		case SQL_DESC_LITERAL_SUFFIX:
			sqlchar = ird_rec->sql_desc_literal_suffix;
			break;
		case SQL_DESC_LOCAL_TYPE_NAME:
			sqlchar = ird_rec->sql_desc_local_type_name;
			break;
		case SQL_DESC_NAME:
			sqlchar = ird_rec->sql_desc_name;
			break;
		case SQL_DESC_NULLABLE:
			integer = ird_rec->sql_desc_nullable;
			break;
		case SQL_DESC_NUM_PREC_RADIX:
			integer = ird_rec->sql_desc_num_prec_radix;
			break;
		case SQL_DESC_OCTET_LENGTH:
			integer = ird_rec->sql_desc_octet_length;
			break;
		case SQL_DESC_PRECISION:
			integer = ird_rec->sql_desc_precision;
			break;
		case SQL_COLUMN_PRECISION:  /* ODBC 2.0 compatability */
			if (ird_rec->sql_desc_concise_type == SQL_NUMERIC) integer = ird_rec->sql_desc_precision;
			else integer = ird_rec->sql_desc_length;
			break;
		case SQL_DESC_SCALE:
			integer = ird_rec->sql_desc_scale;
			break;
		case SQL_COLUMN_SCALE:  /* ODBC 2.0 compatability */
			if (ird_rec->sql_desc_concise_type == SQL_NUMERIC) integer = ird_rec->sql_desc_scale;
			else integer = ird_rec->sql_desc_precision;
			break;
		case SQL_DESC_SCHEMA_NAME:
			sqlchar = ird_rec->sql_desc_schema_name;
			break;
		case SQL_DESC_SEARCHABLE:
			integer = ird_rec->sql_desc_searchable;
			break;
		case SQL_DESC_TABLE_NAME:
			sqlchar = ird_rec->sql_desc_table_name;
			break;
		case SQL_DESC_TYPE:
			integer = ird_rec->sql_desc_type;
			break;
		case SQL_DESC_TYPE_NAME:
			sqlchar = ird_rec->sql_desc_type_name;
			break;
		case SQL_DESC_UNNAMED:
			integer = ird_rec->sql_desc_unnamed;
			break;
		case SQL_DESC_UNSIGNED:
			integer = ird_rec->sql_desc_unsigned;
			break;
		case SQL_DESC_UPDATABLE:
			integer = ird_rec->sql_desc_updatable;
			break;
		default:
			error_record(&stmt->errors, "HY091", HY091TEXT, NULL, 0);
			exitsync();
			return SQL_ERROR;
		}
	}

	if (sqlchar != NULL) {
		length = (SQLSMALLINT)strlen((const char *)sqlchar);
		if (CharacterAttributePtr != NULL) {
			memcpy(CharacterAttributePtr, sqlchar, min(length + 1, BufferLength));
			if (length >= BufferLength) {
				if (BufferLength > 0) ((SQLCHAR *) CharacterAttributePtr)[BufferLength - 1] = '\0';
				error_record(&stmt->errors, "01004", TEXT_01004, NULL, 0);
				rc = SQL_SUCCESS_WITH_INFO;
			}
		}
		if (StringLengthPtr != NULL) *StringLengthPtr = length;
	}
	else if (NumericAttributePtr != NULL) *((SQLINTEGER *) NumericAttributePtr) = integer;
	exitsync();
	return rc;
}

#if 0
/*** CODE: DO NOT SUPPORT INITIALLY ***/
SQLRETURN SQL_API SQLColumnPrivileges(
	SQLHSTMT StatementHandle,
	SQLCHAR *CatalogName,
	SQLSMALLINT NameLength1,
	SQLCHAR *SchemaName,
	SQLSMALLINT NameLength2,
	SQLCHAR *TableName,
	SQLSMALLINT NameLength3,
	SQLCHAR *ColumnName,
	SQLSMALLINT NameLength4)
{
	return SQL_SUCCESS;
}
#endif

SQLRETURN SQL_API SQLColumns(
	SQLHSTMT StatementHandle,
	SQLCHAR *CatalogName,
	SQLSMALLINT NameLength1,
	SQLCHAR *SchemaName,
	SQLSMALLINT NameLength2,
	SQLCHAR *TableName,
	SQLSMALLINT NameLength3,
	SQLCHAR *ColumnName,
	SQLSMALLINT NameLength4)
{
	INT i1, i2, replylen;
	SQLRETURN rc, rc2;
	CHAR buffer[512];  /* must fit 4 names, 8 quotes, 3 spaces, and message function string of 'COLUMNS ' */
	CHAR buffer2[2 * MAX_NAME_LENGTH];
	CHAR *ptr;
	STATEMENT *stmt;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	if (TableName != NULL) {
		if (NameLength3 == SQL_NTS) NameLength3 = (SQLSMALLINT)strlen((const char *)TableName);
	}
	else NameLength3 = 0;
	if (ColumnName != NULL) {
		if (NameLength4 == SQL_NTS) NameLength4 = (SQLSMALLINT)strlen((const char *)ColumnName);
	}
	else NameLength4 = 0;

	/* copy identifier names into the buffer */
	memcpy(buffer, "COLUMNS ", 8);
	i1 = 8;
	if (stmt->cnct->server_majorver == 2) {
		memcpy(buffer + i1, "NULL NULL ", 10);
		i1 += 10;
		if (NameLength3) {
			memcpy(buffer + i1, TableName, NameLength3);
			i1 += NameLength3;
		}
		else {
			memcpy(buffer + i1, "NULL", 4);
			i1 += 4;
		}
		buffer[i1++] = ' ';
		if (NameLength4) {
			memcpy(buffer + i1, ColumnName, NameLength4);
			i1 += NameLength4;
		}
		else {
			memcpy(buffer + i1, "NULL", 4);
			i1 += 4;
		}
		ptr = GETATTR;
	}
	else {
		if (stmt->sql_attr_metadata_id == SQL_TRUE) {
			if (TableName == NULL) {
				error_record(&stmt->errors, "HY009", TEXT_HY009, "TableName cannot be NULL", 0);
				return SQL_ERROR;
			}
			if (ColumnName == NULL) {
				error_record(&stmt->errors, "HY009", TEXT_HY009, "ColumnName cannot be NULL", 0);
				return SQL_ERROR;
			}
			i2 = hide_escapes(buffer2, (CHAR*)TableName, NameLength3);
			i1 += tcpquotedcopy((unsigned char *)(buffer + i1), (unsigned char *)buffer2, i2);
			buffer[i1++] = ' ';
			i2 = hide_escapes(buffer2, (CHAR*)ColumnName, NameLength4);
			i1 += tcpquotedcopy((unsigned char *)(buffer + i1), (unsigned char *)buffer2, i2);
		}
		else {
			i1 += tcpquotedcopy((unsigned char *)(buffer + i1), (unsigned char *)TableName, NameLength3);
			buffer[i1++] = ' ';
			i1 += tcpquotedcopy((unsigned char *)(buffer + i1), (unsigned char *)ColumnName, NameLength4);
		}
		ptr = CATALOG;
	}

	/* check for invalid cursor state */
	if (stmt->executeflags & EXECUTE_RESULTSET) {
		error_record(&stmt->errors, "24000", TEXT_24000, NULL, 0);
		return SQL_ERROR;
	}
	entersync();
	if (stmt->executeflags & EXECUTE_RSID) {
		stmt->executeflags &= ~EXECUTE_RSID;
		if (server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD, NULL, 0, NULL, NULL, NULL) != SERVER_OK) {
			exitsync();
			return SQL_ERROR;
		}
	}
	stmt->executeflags = 0;
	stmt->row_count = -1;

	replylen = sizeof(buffer);
	rc = server_communicate(stmt->cnct, (UCHAR*)EMPTY, (UCHAR*)ptr,
			(UCHAR*)buffer, i1, &stmt->errors, (UCHAR*)buffer, &replylen);
/*** CODE: SHOULD ERROR BE RETURNED IF SERVER_OK (NOT SERVER_FAIL OR COMM ERROR) ***/
	if (rc != SERVER_SET && rc != SERVER_NONE) {
		exitsync();
		return SQL_ERROR;
	}
	rc2 = exec_resultset(stmt, &stmt->errors, buffer, replylen, EXECUTE_SQLCOLUMNS);
	if ((rc2 == SQL_ERROR || rc == SERVER_NONE) && (stmt->executeflags & EXECUTE_RSID)) {
		stmt->executeflags &= ~EXECUTE_RSID;
		server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD, NULL, 0, NULL, NULL, NULL);
	}
	if (rc2 != SQL_ERROR) stmt->executeflags |= EXECUTE_EXECUTED;
	else stmt->executeflags = 0;
	exitsync();
	return rc2;
}

SQLRETURN SQL_API SQLConnect(
	SQLHDBC ConnectionHandle,
	SQLCHAR *ServerName,
	SQLSMALLINT NameLength1,
	SQLCHAR *UserName,
	SQLSMALLINT NameLength2,
	SQLCHAR *Authentication,
	SQLSMALLINT NameLength3)
{
	SQLRETURN rc;
	struct client_connection_struct *cnct;
	CONNECTINFO connectinfo;
	cnct = (CONNECTION *) ConnectionHandle;
	error_init(&cnct->errors);

	if (ServerName == NULL) NameLength1 = 0;
	else if (NameLength1 == SQL_NTS) NameLength1 = (SQLSMALLINT)strlen((const char *)ServerName);
	if (UserName == NULL) NameLength2 = 0;
	else if (NameLength2 == SQL_NTS) NameLength2 = (SQLSMALLINT)strlen((const char *)UserName);
	if (Authentication == NULL) NameLength3 = 0;
	else if (NameLength3 == SQL_NTS) NameLength3 = (SQLSMALLINT)strlen((const char *)Authentication);

	memset(&connectinfo, 0, sizeof(connectinfo));
	connectinfo.dsn = ServerName;
	connectinfo.dsnlen = NameLength1;
	connectinfo.uid = UserName;
	connectinfo.uidlen = NameLength2;
	connectinfo.pwd = Authentication;
	connectinfo.pwdlen = NameLength3;
	entersync();
	rc = server_connect(cnct, &connectinfo);
	if (rc == SQL_ERROR) {
		exitsync();
		return SQL_ERROR;
	}
	cnct->connected = 1;  /* set connected flag */

	/* save data source name and user name */
	cnct->data_source = allocmem(NameLength1 + 1, 0);
	if (cnct->data_source == NULL) {
		SQLDisconnect(ConnectionHandle);
		error_init(&cnct->errors);
		error_record(&cnct->errors, "HY001", TEXT_HY001, NULL, 0);
		exitsync();
		return SQL_ERROR;
	}
	memcpy(cnct->data_source, ServerName, NameLength1);
	cnct->data_source[NameLength1] = '\0';

	cnct->sql_attr_current_catalog = allocmem(NameLength1 + 1, 0);
	if (cnct->sql_attr_current_catalog == NULL) {
		SQLDisconnect(ConnectionHandle);
		error_init(&cnct->errors);
		error_record(&cnct->errors, "HY001", TEXT_HY001, NULL, 0);
		exitsync();
		return SQL_ERROR;
	}
	memcpy(cnct->sql_attr_current_catalog, ServerName, NameLength1);
	cnct->sql_attr_current_catalog[NameLength1] = '\0';

	if (UserName != NULL && NameLength2) {
		cnct->user = allocmem(NameLength2 + 1, 0);
		if (cnct->user == NULL) {
			SQLDisconnect(ConnectionHandle);
			error_init(&cnct->errors);
			error_record(&cnct->errors, "HY001", TEXT_HY001, NULL, 0);
			exitsync();
			return SQL_ERROR;
		}
		memcpy(cnct->user, UserName, NameLength2);
		cnct->user[NameLength2] = '\0';
	}

	/* if any connection attributes are non-default, tell the server */
#if 0
/*** NOTE: SERVER DOES NOT CURRENTLY SUPPORT THIS ***/
	if (cnct->sql_attr_access_mode == SQL_MODE_READ_ONLY) {
		if (server_setattr(cnct, "READONLY Y", &cnct->errors) == SQL_ERROR) {
/*** CODE: NEED TO DISCONNECT, BUT DON"T WANT TO LOSE cnct->errors */
/***       HAVE server_disconnect AND PASS IN NULL FOR ERRORS ***/
			return SQL_ERROR;
		}
	}
#endif
#if 0
/*** NOTE: SHOULD NOT HAVE TO SUPPORT ASYNC IN MULTI-THREADED ENVIRONMENT ***/
/***       AS THE APPLICATION SHOULD USE THREADS, BUT WE MAY NEED THIS IF ***/
/***       DBCFSRUN BECOMES MULTI-THREADED */
	if (cnct->async == SQL_ASYNC_ENABLE_ON);
#endif
	exitsync();
	return rc;
}

SQLRETURN SQL_API SQLCopyDesc(
	SQLHDESC SourceDescHandle,
	SQLHDESC TargetDescHandle)
{
	SQLSMALLINT source_count, recNum, smallint, smallint2, smallint3;
	DESCRIPTOR *source, *target;
	LPVOID sdrec, tdrec;
	SQLLEN integer, integer2;
	SQLUINTEGER uinteger;

	source = (DESCRIPTOR *) SourceDescHandle,
	target = (DESCRIPTOR *) TargetDescHandle;
	error_init(&target->errors);

	/* cannot modify IRD */
	if (target->type == TYPE_IMP_ROW_DESC) {
		error_record(&target->errors, "HY016", HY016TEXT, NULL, 0);
		return SQL_ERROR;
	}

	/* copy header fields, all but count */
	target->sql_desc_array_size = source->sql_desc_array_size;
	target->sql_desc_array_status_ptr = source->sql_desc_array_status_ptr;
	target->sql_desc_bind_offset_ptr = source->sql_desc_bind_offset_ptr;
	target->sql_desc_bind_type = source->sql_desc_bind_type;
	target->sql_desc_rows_processed_ptr = source->sql_desc_rows_processed_ptr;

	source_count = source->sql_desc_count;
	entersync();
	if (source_count > target->sql_desc_count) {
		/* target desc has fewer records than source desc, so allocate needed
		   records, but don't initialize, since all fields will be overwritten anyway */
		if (desc_get(target, source_count, FALSE) == NULL) {
			error_record(&target->errors, "HY001", TEXT_HY001, NULL, 0);
			exitsync();
			return SQL_ERROR;
		}
	}
	target->sql_desc_count = source_count;

	for (recNum = 1; recNum <= source_count; recNum++) {
		sdrec = desc_get(source, recNum, FALSE);
		tdrec = desc_get(target, recNum, FALSE);

		/* Transfer SQL_DESC_CASE_SENSITIVE */
		if (source->type == TYPE_IMP_ROW_DESC && target->type == TYPE_IMP_PARAM_DESC) {
			((IMP_PARAM_DESC_RECORD *) tdrec)->sql_desc_case_sensitive
				= ((IMP_ROW_DESC_RECORD *) sdrec)->sql_desc_case_sensitive;
		}

		/* Transfer SQL_DESC_CONCISE_TYPE */
		switch (source->type) {
		case TYPE_APP_DESC:
			smallint = ((APP_DESC_RECORD *) sdrec)->sql_desc_concise_type;
			break;
		case TYPE_IMP_ROW_DESC:
			smallint = ((IMP_ROW_DESC_RECORD *) sdrec)->sql_desc_concise_type;
			break;
		case TYPE_IMP_PARAM_DESC:
			smallint = ((IMP_PARAM_DESC_RECORD *) sdrec)->sql_desc_concise_type;
			break;
		}
		switch (target->type) {
		case TYPE_APP_DESC:
			((APP_DESC_RECORD *) tdrec)->sql_desc_concise_type = smallint;
			break;
		case TYPE_IMP_PARAM_DESC:
			((IMP_PARAM_DESC_RECORD *) tdrec)->sql_desc_concise_type = smallint;
			break;
		}

		/* Transfer SQL_DESC_DATA_PTR */
		if (source->type == TYPE_APP_DESC && target->type == TYPE_APP_DESC)
			((APP_DESC_RECORD *) tdrec)->sql_desc_data_ptr
				= ((APP_DESC_RECORD *) sdrec)->sql_desc_data_ptr;

		/* Transfer SQL_DESC_DATETIME_INTERVAL_CODE */
		switch (source->type) {
		case TYPE_APP_DESC:
			smallint = ((APP_DESC_RECORD *) sdrec)->sql_desc_datetime_interval_code;
			break;
		case TYPE_IMP_ROW_DESC:
			smallint = ((IMP_ROW_DESC_RECORD *) sdrec)->sql_desc_datetime_interval_code;
			break;
		case TYPE_IMP_PARAM_DESC:
			smallint = ((IMP_PARAM_DESC_RECORD *) sdrec)->sql_desc_datetime_interval_code;
			break;
		}
		switch (target->type) {
		case TYPE_APP_DESC:
			((APP_DESC_RECORD *) tdrec)->sql_desc_datetime_interval_code = smallint;
			break;
		case TYPE_IMP_PARAM_DESC:
			((IMP_PARAM_DESC_RECORD *) tdrec)->sql_desc_datetime_interval_code = smallint;
			break;
		}

		/* Transfer SQL_DESC_DATETIME_INTERVAL_PRECISION */
		switch (source->type) {
		case TYPE_APP_DESC:
			integer = ((APP_DESC_RECORD *) sdrec)->sql_desc_datetime_interval_precision;
			break;
		case TYPE_IMP_ROW_DESC:
			integer = ((IMP_ROW_DESC_RECORD *) sdrec)->sql_desc_datetime_interval_precision;
			break;
		case TYPE_IMP_PARAM_DESC:
			integer = ((IMP_PARAM_DESC_RECORD *) sdrec)->sql_desc_datetime_interval_precision;
			break;
		}
		switch (target->type) {
		case TYPE_APP_DESC:
			((APP_DESC_RECORD *) tdrec)->sql_desc_datetime_interval_precision = (SQLINTEGER)integer;
			break;
		case TYPE_IMP_PARAM_DESC:
			((IMP_PARAM_DESC_RECORD *) tdrec)->sql_desc_datetime_interval_precision = (SQLINTEGER)integer;
			break;
		}

		/* Transfer SQL_DESC_FIXED_PREC_SCALE */
		if (source->type == TYPE_IMP_ROW_DESC && target->type == TYPE_IMP_PARAM_DESC)
			((IMP_PARAM_DESC_RECORD *) tdrec)->sql_desc_fixed_prec_scale
				= ((IMP_ROW_DESC_RECORD *) sdrec)->sql_desc_fixed_prec_scale;

		/* Transfer SQL_DESC_INDICATOR_PTR */
		if (source->type == TYPE_APP_DESC && target->type == TYPE_APP_DESC)
			((APP_DESC_RECORD *) tdrec)->sql_desc_indicator_ptr
				= ((APP_DESC_RECORD *) sdrec)->sql_desc_indicator_ptr;

		/* Transfer SQL_DESC_LENGTH */
		switch (source->type) {
		case TYPE_APP_DESC:
			uinteger = ((APP_DESC_RECORD *) sdrec)->sql_desc_length;
			break;
		case TYPE_IMP_ROW_DESC:
			uinteger = ((IMP_ROW_DESC_RECORD *) sdrec)->sql_desc_length;
			break;
		case TYPE_IMP_PARAM_DESC:
			uinteger = ((IMP_PARAM_DESC_RECORD *) sdrec)->sql_desc_length;
			break;
		}
		switch (target->type) {
		case TYPE_APP_DESC:
			((APP_DESC_RECORD *) tdrec)->sql_desc_length = uinteger;
			break;
		case TYPE_IMP_PARAM_DESC:
			((IMP_PARAM_DESC_RECORD *) tdrec)->sql_desc_length = uinteger;
			break;
		}

		/* Transfer SQL_DESC_LOCAL_TYPE_NAME */
		/* Transfer SQL_DESC_NAME */
		/* Transfer SQL_DESC_NULLABLE */
		if (source->type == TYPE_IMP_ROW_DESC && target->type == TYPE_IMP_PARAM_DESC) {
			strcpy((char *)((IMP_PARAM_DESC_RECORD *) tdrec)->sql_desc_local_type_name,
				(const char *)((IMP_ROW_DESC_RECORD *) sdrec)->sql_desc_local_type_name);
			strcpy((char *)((IMP_PARAM_DESC_RECORD *) tdrec)->sql_desc_name,
				(const char *)((IMP_ROW_DESC_RECORD *) sdrec)->sql_desc_name);
			((IMP_PARAM_DESC_RECORD *) tdrec)->sql_desc_nullable
				= ((IMP_ROW_DESC_RECORD *) sdrec)->sql_desc_nullable;
		}

		/* Transfer SQL_DESC_NUM_PREC_RADIX */
		/* Transfer SQL_DESC_OCTET_LENGTH */
		switch (source->type) {
		case TYPE_APP_DESC:
			integer = ((APP_DESC_RECORD *) sdrec)->sql_desc_num_prec_radix;
			integer2 = ((APP_DESC_RECORD *) sdrec)->sql_desc_octet_length;
			break;
		case TYPE_IMP_ROW_DESC:
			integer = ((IMP_ROW_DESC_RECORD *) sdrec)->sql_desc_num_prec_radix;
			integer2 = ((IMP_ROW_DESC_RECORD *) sdrec)->sql_desc_octet_length;
			break;
		case TYPE_IMP_PARAM_DESC:
			integer = ((IMP_PARAM_DESC_RECORD *) sdrec)->sql_desc_num_prec_radix;
			integer2 = ((IMP_PARAM_DESC_RECORD *) sdrec)->sql_desc_octet_length;
			break;
		}
		switch (target->type) {
		case TYPE_APP_DESC:
			((APP_DESC_RECORD *) tdrec)->sql_desc_num_prec_radix = (SQLINTEGER)integer;
			((APP_DESC_RECORD *) tdrec)->sql_desc_octet_length = integer2;
			break;
		case TYPE_IMP_PARAM_DESC:
			((IMP_PARAM_DESC_RECORD *) tdrec)->sql_desc_num_prec_radix = (SQLINTEGER)integer;
			((IMP_PARAM_DESC_RECORD *) tdrec)->sql_desc_octet_length = integer2;
			break;
		}

		/* Transfer SQL_DESC_OCTET_LENGTH_PTR */
		if (source->type == TYPE_APP_DESC && target->type == TYPE_APP_DESC)
			((APP_DESC_RECORD *) tdrec)->sql_desc_octet_length_ptr
				= ((APP_DESC_RECORD *) sdrec)->sql_desc_octet_length_ptr;

		/* Transfer SQL_DESC_PRECISION */
		/* Transfer SQL_DESC_SCALE */
		/* Transfer SQL_DESC_TYPE */
		switch (source->type) {
		case TYPE_APP_DESC:
			smallint = ((APP_DESC_RECORD *) sdrec)->sql_desc_precision;
			smallint2 = ((APP_DESC_RECORD *) sdrec)->sql_desc_scale;
			smallint3 = ((APP_DESC_RECORD *) sdrec)->sql_desc_type;
			break;
		case TYPE_IMP_ROW_DESC:
			smallint = ((IMP_ROW_DESC_RECORD *) sdrec)->sql_desc_precision;
			smallint2 = ((IMP_ROW_DESC_RECORD *) sdrec)->sql_desc_scale;
			smallint3 = ((IMP_ROW_DESC_RECORD *) sdrec)->sql_desc_type;
			break;
		case TYPE_IMP_PARAM_DESC:
			smallint = ((IMP_PARAM_DESC_RECORD *) sdrec)->sql_desc_precision;
			smallint2 = ((IMP_PARAM_DESC_RECORD *) sdrec)->sql_desc_scale;
			smallint3 = ((IMP_PARAM_DESC_RECORD *) sdrec)->sql_desc_type;
			break;
		}
		switch (target->type) {
		case TYPE_APP_DESC:
			((APP_DESC_RECORD *) tdrec)->sql_desc_precision = smallint;
			((APP_DESC_RECORD *) tdrec)->sql_desc_scale = smallint2;
			((APP_DESC_RECORD *) tdrec)->sql_desc_type = smallint3;
			break;
		case TYPE_IMP_PARAM_DESC:
			((IMP_PARAM_DESC_RECORD *) tdrec)->sql_desc_precision = smallint;
			((IMP_PARAM_DESC_RECORD *) tdrec)->sql_desc_scale = smallint2;
			((IMP_PARAM_DESC_RECORD *) tdrec)->sql_desc_type = smallint3;
			break;
		}

		/* Transfer SQL_DESC_TYPE_NAME */
		if (source->type == TYPE_IMP_ROW_DESC && target->type == TYPE_IMP_PARAM_DESC)
			strcpy((char *)((IMP_PARAM_DESC_RECORD *) tdrec)->sql_desc_type_name,
				(const char *)((IMP_ROW_DESC_RECORD *) sdrec)->sql_desc_type_name);

		/* Transfer SQL_DESC_UNNAMED */
		/* Transfer SQL_DESC_UNSIGNED */
		if (source->type == TYPE_IMP_ROW_DESC && target->type == TYPE_IMP_PARAM_DESC) {
			((IMP_PARAM_DESC_RECORD *) tdrec)->sql_desc_unnamed
				= ((IMP_ROW_DESC_RECORD *) sdrec)->sql_desc_unnamed;
			((IMP_PARAM_DESC_RECORD *) tdrec)->sql_desc_unsigned
				= ((IMP_ROW_DESC_RECORD *) sdrec)->sql_desc_unsigned;
		}

		/* If SQL_DESC_DATA_PTR was copied, perform check */
		if (target->type == TYPE_APP_DESC && ((APP_DESC_RECORD *) tdrec)->sql_desc_data_ptr != NULL
			&& !consistency_check(target, tdrec)) {
			exitsync();
			return SQL_ERROR;
		}
	}
	exitsync();
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLDescribeCol(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT ColumnNumber,
	SQLCHAR *ColumnName,
	SQLSMALLINT BufferLength,
	SQLSMALLINT *NameLengthPtr,
	SQLSMALLINT *DataTypePtr,
	SQLULEN *ColumnSizePtr,
	SQLSMALLINT *DecimalDigitsPtr,
	SQLSMALLINT *NullablePtr)
{
	SQLSMALLINT length;
	SQLRETURN rc;
	STATEMENT *stmt;
	DESCRIPTOR *ird;
	IMP_ROW_DESC_RECORD *ird_rec;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	if (ColumnNumber == 0) {
		error_record(&stmt->errors, "07009", TEXT_07009, "Bookmarks are not supported", 0);
		return SQL_ERROR;
	}

	if (!(stmt->executeflags & EXECUTE_RESULTSET)) {
		if (stmt->executeflags & EXECUTE_EXECUTED) {
			error_record(&stmt->errors, "07005", TEXT_07005, "No result set", 0);
			return SQL_ERROR;
		}
		if (!(stmt->executeflags & EXECUTE_PREPARED)) {
			error_record(&stmt->errors, "HY010", TEXT_HY010, "Statement has not been executed or prepared", 0);
			return SQL_ERROR;
		}
		if (exec_columninfo(stmt, &stmt->errors) == SQL_ERROR) return SQL_ERROR;
	}
	entersync();

	ird = stmt->sql_attr_imp_row_desc;
	if (ColumnNumber > ird->sql_desc_count) {
		exitsync();
		return SQL_NO_DATA;
	}
	ird_rec = (IMP_ROW_DESC_RECORD *) desc_get(ird, ColumnNumber, FALSE);

	rc = SQL_SUCCESS;
	length = (SQLSMALLINT)strlen((const char *)ird_rec->sql_desc_name);
	if (ColumnName != NULL) {
		memcpy(ColumnName, ird_rec->sql_desc_name, min(length + 1, BufferLength));
		if (length >= BufferLength) {
			if (BufferLength > 0) ColumnName[BufferLength - 1] = '\0';
			error_record(&stmt->errors, "01004", TEXT_01004, NULL, 0);
			rc = SQL_SUCCESS_WITH_INFO;
		}
	}
	if (NameLengthPtr != NULL) *NameLengthPtr = length;
	if (DataTypePtr != NULL) *DataTypePtr = ird_rec->sql_desc_concise_type;
	if (ColumnSizePtr != NULL) {
		if (ird_rec->sql_desc_concise_type == SQL_NUMERIC) *ColumnSizePtr = ird_rec->sql_desc_precision;
		else *ColumnSizePtr = ird_rec->sql_desc_length;
	}
	if (DecimalDigitsPtr != NULL) {
		if (ird_rec->sql_desc_concise_type == SQL_NUMERIC) *DecimalDigitsPtr = ird_rec->sql_desc_scale;
		else *DecimalDigitsPtr = 0;
	}
	if (NullablePtr != NULL) *NullablePtr = ird_rec->sql_desc_nullable;
	exitsync();
	return rc;
}

#if 0
/*** CODE: DO NOT SUPPORT INITIALLY ??? ***/
SQLRETURN SQL_API SQLDescribeParam(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT ParameterNumber,
	SQLSMALLINT *DataTypePtr,
	SQLUINTEGER *ParameterSizePtr,
	SQLSMALLINT *DecimalDigitsPtr,
	SQLSMALLINT *NullablePtr)
{
	return SQL_SUCCESS;
}
#endif

SQLRETURN SQL_API SQLDisconnect(
	SQLHDBC ConnectionHandle)
{
	SQLRETURN rc;
	struct client_connection_struct *cnct;
	STATEMENT *stmt, *stmt2;
	DESCRIPTOR *desc, *desc2;

	cnct = (CONNECTION *) ConnectionHandle;
	error_init(&cnct->errors);

	entersync();
	freemem(cnct->sql_attr_translate_lib);
	cnct->sql_attr_translate_lib = NULL;
	freemem(cnct->user);
	cnct->user = NULL;
	freemem(cnct->data_source);
	cnct->data_source = NULL;

	if (cnct->connected == 1) { /* disconnect */
		/* note: go ahead and disconnects even though there may be pending result sets */
		rc = server_disconnect(cnct);
	}
	else rc = SQL_SUCCESS;
	cnct->connected = 0;

#if OS_WIN32
	__try {
#endif
	/* free statement handles */
	for (stmt = cnct->firststmt; stmt != NULL; stmt = stmt2) {
		/* free descriptor records and descriptors */
		desc_unbind(stmt->apdsave, -1);
		freemem(stmt->apdsave);
		desc_unbind(stmt->ardsave, -1);
		freemem(stmt->ardsave);
		desc_unbind(stmt->sql_attr_imp_param_desc, -1);
		freemem(stmt->sql_attr_imp_param_desc);
		desc_unbind(stmt->sql_attr_imp_row_desc, -1);
		freemem(stmt->sql_attr_imp_row_desc);
		stmt2 = stmt->nextstmt;
		freemem(stmt);
	}
	cnct->firststmt = NULL;

	/* free explicitly allocated descriptor handles */
	for (desc = cnct->firstexplicitdesc; desc != NULL; desc = desc2) {
		desc_unbind(desc, -1);
		desc2 = desc->nextexplicitdesc;
		freemem(desc);
	}
	cnct->firstexplicitdesc = NULL;
#if OS_WIN32
	}
	__except( EXCEPTION_EXECUTE_HANDLER )
	{
		int i1 = GetExceptionCode();
		exitsync();
		error_record(&cnct->errors, "HY000", TEXT_HY000, "In SQLDisconnect", i1);
		return SQL_ERROR;
	}
#endif
	exitsync();
	return rc;
}

SQLRETURN SQL_API SQLDriverConnect(
	SQLHDBC ConnectionHandle,
	SQLHWND WindowHandle,
	SQLCHAR *InConnectionString,
	SQLSMALLINT StringLength1,
	SQLCHAR *OutConnectionString,
	SQLSMALLINT BufferLength,
	SQLSMALLINT *StringLength2Ptr,
	SQLUSMALLINT DriverCompletion)
{
	INT i1, i2, length;
	INT driverlen, filedsnlen, savefilelen;
	SQLRETURN result;
	CHAR *workstring;
	SQLCHAR *driver, *filedsn, *savefile;
	struct client_connection_struct *cnct;
	CONNECTINFO connectinfo;
	cnct = (CONNECTION *) ConnectionHandle;
	error_init(&cnct->errors);

/*** CODE: MAYBE ADD SUPPORT FOR DRIVER COMPLETION ***/
	driverlen = filedsnlen = savefilelen = 0;
	memset(&connectinfo, 0, sizeof(connectinfo));
	if (StringLength1 == SQL_NTS) StringLength1 = (SQLSMALLINT)strlen((const char *)InConnectionString);
	for (i1 = 0; i1 < StringLength1; i1++) {
		if (isspace(InConnectionString[i1])) continue;
		for (i2 = 0; i1 + i2 < StringLength1 && InConnectionString[i1 + i2] != ';'; i2++) {}
		switch (toupper(InConnectionString[i1])) {
		case 'D':
			if (i2 >= 10 && !_strnicmp((const char *)(InConnectionString + i1), "DATABASE=", 9)) {
				connectinfo.database = InConnectionString + i1 + 9;
				connectinfo.databaselen = i2 - 9;
			}
			else if (i2 >= 8 && !_strnicmp((const char *)(InConnectionString + i1), "DRIVER=", 7)) {
				driver = InConnectionString + i1 + 7;
				driverlen = i2 - 7;
			}
			else if (i2 >= 5 && !_strnicmp((const char *)(InConnectionString + i1), "DSN=", 4)) {
				connectinfo.dsn = InConnectionString + i1 + 4;
				connectinfo.dsnlen = i2 - 4;
			}
			break;
		case 'E':
			if (i2 >= 12 && !_strnicmp((const char *)(InConnectionString + i1), "ENCRYPTION=", 11)) {
				connectinfo.encryption = InConnectionString + i1 + 11;
				connectinfo.encryptionlen = i2 - 11;
			}
			break;
		case 'F':
			if (i2 >= 9 && !_strnicmp((const char *)(InConnectionString + i1), "FILEDSN=", 8)) {
				filedsn = InConnectionString + i1 + 8;
				filedsnlen = i2 - 8;
			}
			break;
		case 'L':
			if (i2 >= 11 && !_strnicmp((const char *)(InConnectionString + i1), "LOCALPORT=", 10)) {
				connectinfo.localport = InConnectionString + i1 + 10;
				connectinfo.localportlen = i2 - 10;
			}
			break;
		case 'P':
			if (i2 >= 5 && !_strnicmp((const char *)(InConnectionString + i1), "PWD=", 4)) {
				connectinfo.pwd = InConnectionString + i1 + 4;
				connectinfo.pwdlen = i2 - 4;
			}
			break;
		case 'S':
			if (i2 >= 10 && !_strnicmp((const char *)(InConnectionString + i1), "SAVEFILE=", 9)) {
				savefile = InConnectionString + i1 + 9;
				savefilelen = i2 - 9;
			}
			else if (i2 >= 8 && !_strnicmp((const char *)(InConnectionString + i1), "SERVER=", 7)) {
				connectinfo.server = InConnectionString + i1 + 7;
				connectinfo.serverlen = i2 - 7;
			}
			else if (i2 >= 12 && !_strnicmp((const char *)(InConnectionString + i1), "SERVERPORT=", 11)) {
				connectinfo.serverport = InConnectionString + i1 + 11;
				connectinfo.serverportlen = i2 - 11;
			}
			break;
		case 'U':
			if (i2 >= 5 && !_strnicmp((const char *)(InConnectionString + i1), "UID=", 4)) {
				connectinfo.uid = InConnectionString + i1 + 4;
				connectinfo.uidlen = i2 - 4;
			}
			break;
		}
		i1 += i2;
	}
#if OS_WIN32
	if (savefilelen) {  /* being called by the ODBC administrator to create or modify a file DSN */
		workstring = (CHAR *) allocmem(4096, 0);
		if (workstring == NULL) {
			error_record(&cnct->errors, "HY001", TEXT_HY001, NULL, 0);
			return SQL_ERROR;
		}
		length = 0;
		if (driverlen) {
			strncpy(workstring + length, driver - 7, driverlen + 7);
			length += driverlen + 7;
			workstring[length++] = ';';
		}
		i1 = length;
		if (connectinfo.uidlen) {
			strncpy(workstring + i1, connectinfo.uid - 4, connectinfo.uidlen + 4);
			i1 += connectinfo.uidlen + 4;
			workstring[i1++] = '\0';
		}
		if (connectinfo.pwdlen) {
			strncpy(workstring + i1, connectinfo.pwd - 4, connectinfo.pwdlen + 4);
			i1 += connectinfo.pwdlen + 4;
			workstring[i1++] = '\0';
		}
		if (connectinfo.databaselen) {
			strncpy(workstring + i1, connectinfo.database - 9, connectinfo.databaselen + 9);
			i1 += connectinfo.databaselen + 9;
			workstring[i1++] = '\0';
		}
		if (connectinfo.serverlen) {
			strncpy(workstring + i1, connectinfo.server - 7, connectinfo.serverlen + 7);
			i1 += connectinfo.serverlen + 7;
			workstring[i1++] = '\0';
		}
		if (connectinfo.serverportlen) {
			strncpy(workstring + i1, connectinfo.serverport - 11, connectinfo.serverportlen + 11);
			i1 += connectinfo.serverportlen + 11;
			workstring[i1++] = '\0';
		}
		if (connectinfo.localportlen) {
			strncpy(workstring + i1, connectinfo.localport - 10, connectinfo.localportlen + 10);
			i1 += connectinfo.localportlen + 10;
			workstring[i1++] = '\0';
		}
		if (connectinfo.encryptionlen) {
			strncpy(workstring + i1, connectinfo.encryption - 11, connectinfo.encryptionlen + 11);
			i1 += connectinfo.encryptionlen + 11;
			workstring[i1++] = '\0';
		}
		if (filedsnlen) {
			strncpy(workstring + i1, filedsn - 8, filedsnlen + 8);
			i1 += filedsnlen + 8;
			workstring[i1++] = '\0';
		}
		if (savefilelen) {
			strncpy(workstring + i1, savefile - 9, savefilelen + 9);
			i1 += savefilelen + 9;
			workstring[i1++] = '\0';
		}
		workstring[i1] = '\0';
		i1 = ConfigFileDSN(WindowHandle, workstring + length);
		if (i1) {
			freemem(workstring);
			if (i1 == 1) return SQL_NO_DATA;
			error_record(&cnct->errors, "IM0008", TEXT_IM008, NULL, i1);
			return SQL_ERROR;
		}
		length += (INT)strlen(workstring + length);
		if (filedsnlen) {
			strncpy(workstring + length, filedsn - 8, filedsnlen + 8);
			length += filedsnlen + 8;
			workstring[length++] = ';';
		}
		if (savefilelen) {
			strncpy(workstring + length, savefile - 9, savefilelen + 9);
			length += savefilelen + 9;
			workstring[length++] = ';';
		}
		if (OutConnectionString != NULL) {
			if (length < BufferLength) {
				BufferLength = (SQLSMALLINT) length;
				OutConnectionString[BufferLength] = '\0';
			}
			else {
				if (BufferLength > 0) OutConnectionString[--BufferLength] = '\0';
				error_record(&(cnct->errors), "01004", TEXT_01004, NULL, 0);
				result = SQL_SUCCESS_WITH_INFO;
			}
			if (BufferLength > 0) strncpy(OutConnectionString, workstring, BufferLength);
		}
		freemem(workstring);
		if (StringLength2Ptr != NULL) *StringLength2Ptr = (SQLSMALLINT) length;
		cnct->connected = 2;
		return SQL_SUCCESS;
	}
#endif
	entersync();
	if (!driverlen || connectinfo.dsnlen || connectinfo.databaselen) {
		result = server_connect(cnct, &connectinfo);
		if (result == SQL_ERROR) {
			exitsync();
			return SQL_ERROR;
		}
		cnct->connected = 1;
	}
	else {
		connectinfo.dsn = (SQLCHAR*)"DSN=DEFAULT";
		connectinfo.dsn += 4;
		connectinfo.dsnlen = 7;
		result = SQL_SUCCESS;
		cnct->connected = 2;
	}

	/* save data source name and user name */
	cnct->data_source = allocmem(connectinfo.dsnlen + 1, 0);
	if (cnct->data_source == NULL) {
		SQLDisconnect(ConnectionHandle);
		error_init(&cnct->errors);
		error_record(&cnct->errors, "HY001", TEXT_HY001, NULL, 0);
		exitsync();
		return SQL_ERROR;
	}
	memcpy(cnct->data_source, connectinfo.dsn, connectinfo.dsnlen);
	cnct->data_source[connectinfo.dsnlen] = '\0';
	if (connectinfo.uidlen) {
		cnct->user = allocmem(connectinfo.uidlen + 1, 0);
		if (cnct->user == NULL) {
			SQLDisconnect(ConnectionHandle);
			error_init(&cnct->errors);
			error_record(&cnct->errors, "HY001", TEXT_HY001, NULL, 0);
			exitsync();
			return SQL_ERROR;
		}
		memcpy(cnct->user, connectinfo.uid, connectinfo.uidlen);
		cnct->user[connectinfo.uidlen] = '\0';
	}

	/* if any connection attributes are non-default, tell the server */
	if (cnct->connected == 1) {
#if 0
/*** NOTE: SERVER DOES NOT CURRENTLY SUPPORT THIS ***/
		if (cnct->sql_attr_access_mode == SQL_MODE_READ_ONLY) {
			if (server_setattr(cnct, "READONLY Y", &cnct->errors) == SQL_ERROR) {
				ERRORS errors;
				errors = cnct->errors;
				SQLDisconnect(ConnectionHandle);
				cnct->errors = errors;
				return SQL_ERROR;
			}
			if (rc == SQL_SUCCESS_WITH_INFO) result = rc;
		}
#endif
#if 0
/*** NOTE: SHOULD NOT HAVE TO SUPPORT ASYNC IN MULTI-THREADED ENVIRONMENT ***/
/***       AS THE APPLICATION SHOULD USE THREADS, BUT WE MAY NEED THIS IF ***/
/***       DBCFSRUN BECOMES MULTI-THREADED */
		if (cnct->async == SQL_ASYNC_ENABLE_ON);
#endif
	}

	/* Build the completed connection string */
	workstring = (CHAR *) allocmem(4096, 0);
	if (workstring == NULL) {
		error_init(&cnct->errors);
		SQLDisconnect(ConnectionHandle);
		error_record(&cnct->errors, "HY001", TEXT_HY001, NULL, 0);
		return SQL_ERROR;
	}
	length = 0;
	if (driverlen) {
		strncpy(workstring + length, (const char *)(driver - 7), driverlen + 7);
		length += driverlen + 7;
		workstring[length++] = ';';
	}
	if (connectinfo.dsnlen) {
		strncpy(workstring + length, (const char *)(connectinfo.dsn - 4), connectinfo.dsnlen + 4);
		length += connectinfo.dsnlen + 4;
		workstring[length++] = ';';
	}
	if (connectinfo.uidlen) {
		strncpy(workstring + length, (const char *)(connectinfo.uid - 4), connectinfo.uidlen + 4);
		length += connectinfo.uidlen + 4;
		workstring[length++] = ';';
	}
	if (connectinfo.pwdlen) {
		strncpy(workstring + length, (const char *)(connectinfo.pwd - 4), connectinfo.pwdlen + 4);
		length += connectinfo.pwdlen + 4;
		workstring[length++] = ';';
	}
	if (connectinfo.databaselen) {
		strncpy(workstring + length, (const char *)(connectinfo.database - 9), connectinfo.databaselen + 9);
		length += connectinfo.databaselen + 9;
		workstring[length++] = ';';
	}
	if (connectinfo.serverlen) {
		strncpy(workstring + length, (const char *)(connectinfo.server - 7), connectinfo.serverlen + 7);
		length += connectinfo.serverlen + 7;
		workstring[length++] = ';';
	}
	if (connectinfo.serverportlen) {
		strncpy(workstring + length, (const char *)(connectinfo.serverport - 11), connectinfo.serverportlen + 11);
		length += connectinfo.serverportlen + 11;
		workstring[length++] = ';';
	}
	if (connectinfo.localportlen) {
		strncpy(workstring + length, (const char *)(connectinfo.localport - 10), connectinfo.localportlen + 10);
		length += connectinfo.localportlen + 10;
		workstring[length++] = ';';
	}
	if (connectinfo.encryptionlen) {
		strncpy(workstring + length, (const char *)(connectinfo.encryption - 11), connectinfo.encryptionlen + 11);
		length += connectinfo.encryptionlen + 11;
		workstring[length++] = ';';
	}
	if (connectinfo.authenticationlen) {
		strncpy(workstring + length, (const char *)(connectinfo.authentication - 15), connectinfo.authenticationlen + 15);
		length += connectinfo.authenticationlen + 15;
		workstring[length++] = ';';
	}
	if (filedsnlen) {
		strncpy(workstring + length, (const char *)(filedsn - 8), filedsnlen + 8);
		length += filedsnlen + 8;
		workstring[length++] = ';';
	}

	if (OutConnectionString != NULL) {
		if (length < BufferLength) {
			BufferLength = (SQLSMALLINT) length;
			OutConnectionString[BufferLength] = '\0';
		}
		else {
			if (BufferLength > 0) OutConnectionString[--BufferLength] = '\0';
			error_record(&(cnct->errors), "01004", TEXT_01004, NULL, 0);
			result = SQL_SUCCESS_WITH_INFO;
		}
		if (BufferLength > 0) strncpy((char*)OutConnectionString, workstring, BufferLength);
	}
	freemem(workstring);
	if (StringLength2Ptr != NULL) *StringLength2Ptr = (SQLSMALLINT) length;
	exitsync();
	return result;
}


SQLRETURN SQL_API SQLEndTran(
	SQLSMALLINT HandleType,
	SQLHANDLE Handle,
	SQLSMALLINT CompletionType)
{
	ERRORS *errors;

	if (HandleType == SQL_HANDLE_ENV) errors = &((ENVIRONMENT *) Handle)->errors;
	else errors = &((struct client_connection_struct *) Handle)->errors;
	error_init(errors);

	if (CompletionType == SQL_ROLLBACK) {
		error_record(errors, "HYC00", TEXT_HYC00, NULL, 0);
		return SQL_ERROR;
	}
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLExecDirect(
	SQLHSTMT StatementHandle,
	SQLCHAR *StatementText,
	SQLINTEGER TextLength)
{
	STATEMENT *stmt;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	entersync();
	if (stmt->executeflags & EXECUTE_RSID) {
		stmt->executeflags &= ~EXECUTE_RSID;
		if (server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD, NULL, 0, NULL, NULL, NULL) != SERVER_OK) {
			exitsync();
			return SQL_ERROR;
		}
	}
	stmt->executeflags = 0;
	stmt->row_count = -1;

	if (TextLength == SQL_NTS) TextLength = (SQLINTEGER)strlen((const char *)StatementText);
	if (exec_prepare(stmt, StatementText, TextLength) == SQL_ERROR) return SQL_ERROR;
	exitsync(); // 12/15/19 17:12

	return exec_execute((STATEMENT *) StatementHandle);
}

SQLRETURN SQL_API SQLExecute(
	SQLHSTMT StatementHandle)
{
	STATEMENT *stmt;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	entersync();
	if (stmt->executeflags & EXECUTE_RSID) {
		stmt->executeflags &= ~EXECUTE_RSID;
		if (server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD, NULL, 0, NULL, NULL, NULL) != SERVER_OK) {
			exitsync();
			return SQL_ERROR;
		}
	}
	exitsync();
	if (!(stmt->executeflags &= EXECUTE_PREPARED)) {  /* clear and test */
		error_record(&stmt->errors, "24000", TEXT_24000, "statement not prepared", 0);
		return SQL_ERROR;
	}
	stmt->row_count = -1;
	return exec_execute(stmt);
}

#if 0
/*** CODE: SUPPORT IF WANT TO WORK WITH 2.X APPLICATIONS ***/
SQLRETURN SQL_API SQLExtendedFetch(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT FetchOrientation,
	SQLINTEGER FetchOffset,
	SQLUINTEGER *RowCountPtr,
	SQLUSMALLINT *RowStatusArray)
{
/*** CODE: SEE SQLExtendedFetch DOC FOR DIFFERENCES TO SQLFetchScroll ***/
	return SQL_SUCCESS;
}
#endif

SQLRETURN SQL_API SQLFetch(
	SQLHSTMT StatementHandle)
{
/*** NOTE: call separate fsFetchScroll function to not break unixODBC driver ***/
	return fsFetchScroll(StatementHandle, SQL_FETCH_NEXT, (SQLLEN)0);
}

SQLRETURN SQL_API SQLFetchScroll(
	SQLHSTMT StatementHandle,
	SQLSMALLINT FetchOrientation,
	SQLLEN FetchOffset)
{
/*** NOTE: call separate fsFetchScroll function to not break unixODBC driver ***/
	return fsFetchScroll(StatementHandle, FetchOrientation, FetchOffset);
}

static SQLRETURN SQL_API fsFetchScroll(
	SQLHSTMT StatementHandle,
	SQLSMALLINT FetchOrientation,
	SQLLEN FetchOffset)
{
	INT i1, columnlen, precision, rowlength, scale;
	SQLLEN rownumber;
	SQLSMALLINT ard_count, count, ird_count;
	SQLRETURN rc, rc2;
	CHAR work[65], *getrow;
	UCHAR *bufptr;
	STATEMENT *stmt;
	struct client_connection_struct *cnct;
	APP_DESC_RECORD *ard_rec;
	IMP_ROW_DESC_RECORD *ird_rec;
	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);
/*** CODE: CONVERT/VERIFY FS 2 CODE ***/
	if (!(stmt->executeflags & EXECUTE_RESULTSET)) {
		if (stmt->executeflags & EXECUTE_EXECUTED)
			error_record(&stmt->errors, "24000", TEXT_24000, "No result set", 0);
		else error_record(&stmt->errors, "HY010", TEXT_HY010, "Statement has not been executed", 0);
		return SQL_ERROR;
	}
	if (stmt->row_count == 0) return SQL_NO_DATA;
	stmt->executeflags &= ~EXECUTE_FETCH;

	work[0] = 0;
	rownumber = stmt->row_number;
	switch (FetchOrientation) {
	case SQL_FETCH_NEXT:
		getrow = GETROWN;
		rownumber++;
		break;
	case SQL_FETCH_PRIOR:
		getrow = GETROWP;
		rownumber--;
		break;
	case SQL_FETCH_FIRST:
		getrow = GETROWF;
		rownumber = 1;
		break;
	case SQL_FETCH_LAST:
		getrow = GETROWL;
		if (stmt->row_count != -1) rownumber = stmt->row_count;
		else rownumber = 1;
		break;
	case SQL_FETCH_ABSOLUTE:
		getrow = GETROWA;
#ifdef _WIN64
		_i64toa(FetchOffset, work, 10);
#else
		_itoa(FetchOffset, work, 10);
#endif
		rownumber = FetchOffset;
		break;
	case SQL_FETCH_RELATIVE:
		getrow = GETROWR;
#ifdef _WIN64
		_i64toa(FetchOffset, work, 10);
#else
		_itoa(FetchOffset, work, 10);
#endif
		rownumber += FetchOffset;
		break;
	case SQL_FETCH_BOOKMARK:
	default:
		error_record(&stmt->errors, "07009", TEXT_HYC00, NULL, 0);
		return SQL_ERROR;
	}

	if (stmt->sql_attr_imp_row_desc->sql_desc_array_status_ptr != NULL) {
		for (i1 = 0; i1 < (int) stmt->sql_attr_app_row_desc->sql_desc_array_size; i1++)
			stmt->sql_attr_imp_row_desc->sql_desc_array_status_ptr[i1] = SQL_ROW_NOROW;
	}
	if (stmt->executeflags & EXECUTE_SQLGETTYPEINFO) {
		if (rownumber < 1 || rownumber > stmt->row_count) {
			if (rownumber < 1) stmt->row_number = 0;
			else stmt->row_number = stmt->row_count + 1;
			stmt->sql_attr_row_number = 0;
			if (stmt->sql_attr_imp_row_desc->sql_desc_rows_processed_ptr != NULL)
				*stmt->sql_attr_imp_row_desc->sql_desc_rows_processed_ptr = 0;
			return SQL_NO_DATA;
		}
		memcpy(stmt->row_buffer, stmt->row_buffer + rownumber * MAX_DATA_LENGTH, MAX_DATA_LENGTH);
		stmt->row_length = rowlength = MAX_DATA_LENGTH;
	}
	else {
		entersync();  /* protect workbuffer */
#if OS_WIN32
		__try {
#endif
 		cnct = stmt->cnct;
		rowlength = sizeof(workbuffer);
		rc = server_communicate(cnct, (UCHAR*)stmt->rsid, (UCHAR*)getrow,
				(UCHAR*)work, (INT)strlen(work), &stmt->errors, (UCHAR*)workbuffer, &rowlength);
		if (rc != SERVER_OK) {
			exitsync();
			if (rc == SERVER_NODATA) {
				if (rownumber < 1) stmt->row_number = 0;
				else stmt->row_number = stmt->row_count + 1;
				stmt->sql_attr_row_number = 0;
				if (stmt->sql_attr_imp_row_desc->sql_desc_rows_processed_ptr != NULL)
					*stmt->sql_attr_imp_row_desc->sql_desc_rows_processed_ptr = 0;
				return SQL_NO_DATA;
			}
			return SQL_ERROR;
		}
		/* fixup column packet returned from the FS 2 server */
		if (cnct->server_majorver == 2 && (stmt->executeflags & EXECUTE_SQLCOLUMNS) && rowlength == 253) {
			memcpy(workbuffer + 250, workbuffer + 244, 3);
			memset(workbuffer + 244, ' ', 3);
		}

/*** CODE: FREEING THIS BUFFER DURING CLOSE CURSOR AND FREESTATEMENT(SQL_CLOSE), MAY WANT TO ***/
/***       WAIT TILL STATMENT HANDLE IS FREED ***/
		if (stmt->row_buffer == NULL) {
			bufptr = allocmem(rowlength, 0);
			if (bufptr == NULL) {
				exitsync();
				error_record(&stmt->errors, "HY001", TEXT_HY001, NULL, 0);
				return SQL_ERROR;
			}
			stmt->row_buffer = bufptr;
			stmt->row_buffer_size = rowlength;
		}
		else if (rowlength > stmt->row_buffer_size) {
			bufptr = reallocmem(stmt->row_buffer, rowlength, 0);
			if (bufptr == NULL) {
				exitsync();
				error_record(&stmt->errors, "HY001", TEXT_HY001, NULL, 0);
				return SQL_ERROR;
			}
			stmt->row_buffer = bufptr;
			stmt->row_buffer_size = rowlength;
		}
		memcpy(stmt->row_buffer, workbuffer, rowlength);
		stmt->row_length = rowlength;
#if OS_WIN32
		}
		__except( EXCEPTION_EXECUTE_HANDLER )
		{
			int i1 = GetExceptionCode();
			exitsync();
			error_record(&stmt->errors, "HY000", TEXT_HY000, "In fsFetchScroll", i1);
			return SQL_ERROR;
		}
#endif
		exitsync();  /* done with workbuffer */
	}

	rc = SQL_SUCCESS;
	ard_count = stmt->sql_attr_app_row_desc->sql_desc_count;
	if (ard_count) {  /* fill the bound columns */
		ird_count = stmt->sql_attr_imp_row_desc->sql_desc_count;
		ard_rec = stmt->sql_attr_app_row_desc->firstrecord.appdrec;
		ird_rec = stmt->sql_attr_imp_row_desc->firstrecord.irdrec;
		for (count = 0; ++count <= ard_count; ) {
			if (count <= ird_count) {
				if (ard_rec->sql_desc_data_ptr != NULL) {  /* bound */
					if (stmt->cnct->env->sql_attr_odbc_version == SQL_OV_ODBC2) precision = scale = -1;
					else {
						precision = ard_rec->sql_desc_precision;
						scale = ard_rec->sql_desc_scale;
					}
					columnlen = ird_rec->fieldlength;
					if (ird_rec->rowbufoffset >= rowlength) columnlen = 0;
					else if (columnlen > rowlength - ird_rec->rowbufoffset) columnlen = rowlength - ird_rec->rowbufoffset;
					bufptr = stmt->row_buffer + ird_rec->rowbufoffset;
					rc2 = exec_sqltoc(stmt, ird_rec, count, (CHAR*)bufptr, columnlen,
						ard_rec->sql_desc_concise_type, precision, scale,
						ard_rec->sql_desc_data_ptr, ard_rec->sql_desc_octet_length,
						ard_rec->sql_desc_octet_length_ptr, ard_rec->sql_desc_indicator_ptr,
						(stmt->sql_attr_app_row_desc->sql_desc_bind_offset_ptr != NULL) ? *stmt->sql_attr_app_row_desc->sql_desc_bind_offset_ptr : 0,
						0, stmt->sql_attr_app_row_desc->sql_desc_bind_type);
					if (rc2 == SQL_ERROR) return SQL_ERROR;
					if (rc2 != SQL_SUCCESS) rc = rc2;
				}
				ird_rec = ird_rec->nextrecord;
			}
			else if (ard_rec->sql_desc_data_ptr != NULL) {
				error_record(&stmt->errors, "07009", TEXT_07009, "Bound column exceeds number of columns in result set", 0);
				return SQL_ERROR;
			}
			ard_rec = ard_rec->nextrecord;
		}
	}
	if (rc != SQL_ERROR) {
		entersync(); // added 22MAY2020
		if (FetchOrientation != SQL_FETCH_LAST) {
			if (rownumber < 1) rownumber = 1;
			if (rownumber > stmt->row_count) {
				if (!(stmt->executeflags & EXECUTE_DYNAMIC)) rownumber = stmt->row_count;
				else stmt->row_count = (INT)rownumber;
			}
			stmt->sql_attr_row_number = stmt->row_number = (INT)rownumber;
		}
		else if (!(stmt->executeflags & EXECUTE_DYNAMIC)) stmt->sql_attr_row_number = stmt->row_number = stmt->row_count;
		else {
			i1 = sizeof(work);
			rc2 = server_communicate(cnct, (UCHAR*)stmt->rsid, (UCHAR*)GETROWCT,
				NULL, 0, NULL, (UCHAR *) work, &i1);
			if (rc2 == SERVER_OK && i1) {
				tcpntoi((UCHAR *) work, i1, &i1);
				stmt->row_count = i1;
			}
			else {
				i1 = 0;
				stmt->row_count = -1;
			}
			stmt->sql_attr_row_number = stmt->row_number = i1;
		}
		if (stmt->sql_attr_imp_row_desc->sql_desc_rows_processed_ptr != NULL) {
			/**
			 * After success fetch update row count - SSN
			 *
			 * jpr 30 NOV 2015
			 * Change, the ODBC spec says that this should be the size of the 'row set'
			 * We always fetch ONE row, so this should always return 1
			 *
			 * Note that stmt->sql_attr_app_row_desc->sql_desc_array_size, used below and in other places
			 * is always ONE.
			 */
			*stmt->sql_attr_imp_row_desc->sql_desc_rows_processed_ptr = 1; //stmt->row_count;
		}
		if (stmt->sql_attr_imp_row_desc->sql_desc_array_status_ptr != NULL) {
			/** After success fetch update row status - SSN **/
			for (i1 = 0; i1 < (int) stmt->sql_attr_app_row_desc->sql_desc_array_size; i1++) {
				stmt->sql_attr_imp_row_desc->sql_desc_array_status_ptr[i1] = SQL_SUCCESS;
			}
		}
		stmt->executeflags |= EXECUTE_FETCH;
		stmt->lastcolumn = 0;
		exitsync(); // added 22MAY2020
	}
	return rc;
}

#if 0
/*** CODE: DO NOT SUPPORT INITIALLY ***/
SQLRETURN SQL_API SQLForeignKeys(
	SQLHSTMT StatementHandle,
	SQLCHAR *PKCatalogName,
	SQLSMALLINT NameLength1,
	SQLCHAR *PKSchemaName,
	SQLSMALLINT NameLength2,
	SQLCHAR *PKTableName,
	SQLSMALLINT NameLength3,
	SQLCHAR *FKCatalogName,
	SQLSMALLINT NameLength4,
	SQLCHAR *FKSchemaName,
	SQLSMALLINT NameLength5,
	SQLCHAR *FKTableName,
	SQLSMALLINT NameLength6)
{
	return SQL_SUCCESS;
}
#endif

SQLRETURN SQL_API SQLFreeHandle(
	SQLSMALLINT HandleType,
	SQLHANDLE Handle)
{
	struct client_connection_struct *cnct;
	STATEMENT *stmt, **stmtp;
	DESCRIPTOR *desc, **descp;


	switch (HandleType) {
	case SQL_HANDLE_ENV:
		entersync();
		freemem(Handle);
#if OS_WIN32
		__try {
#endif
		if (environments) {
			if (--environments == 0) {
				server_cleanup();
#if OS_WIN32
				WSACleanup();  /* last env handle, clean up winsock */
#endif
			}
		}
#if OS_WIN32
		}
		__except ( EXCEPTION_EXECUTE_HANDLER ) {
			exitsync();
			return SQL_ERROR;
		}
#endif
		exitsync();
		break;
	case SQL_HANDLE_DBC:
		entersync();
		cnct = (CONNECTION *) Handle;
		freemem(cnct->sql_attr_current_catalog);
		freemem(cnct);
		exitsync();
		break;
	case SQL_HANDLE_STMT: /* 3 */
		entersync();
		stmt = (STATEMENT *) Handle;
		cnct = stmt->cnct;
		/* discard results */
		if (stmt->executeflags & EXECUTE_RSID) {
			stmt->executeflags &= ~EXECUTE_RSID;
			error_init(&stmt->errors);
			if (server_communicate(cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD,
				NULL, 0, &stmt->errors, NULL, NULL) != SERVER_OK) {
				exitsync();
				return SQL_ERROR;
			}
		}
		/* remove from connections linked list */
#if OS_WIN32
		/**
		 * What happens here if cnct is NULL or pointing to free'd memory?
		 * We get an exception and the critical section is orphaned!
		 */
		__try {
			for (stmtp = &cnct->firststmt; *stmtp != stmt; stmtp = &(*stmtp)->nextstmt) {}
			*stmtp = stmt->nextstmt;
		}
		__except( EXCEPTION_EXECUTE_HANDLER )
		{
			int i1 = GetExceptionCode();
			exitsync();
			error_record(&stmt->errors, "HY000", TEXT_HY000, "In SQLFreeHandle, SQL_HANDLE_STMT", i1);
			return SQL_ERROR;
		}
#else
		for (stmtp = &cnct->firststmt; *stmtp != stmt; stmtp = &(*stmtp)->nextstmt) {}
		*stmtp = stmt->nextstmt;
#endif
		/* free descriptor records and descriptors */
		desc_unbind(stmt->apdsave, -1);
		freemem(stmt->apdsave);
		desc_unbind(stmt->ardsave, -1);
		freemem(stmt->ardsave);
		desc_unbind(stmt->sql_attr_imp_param_desc, -1);
		freemem(stmt->sql_attr_imp_param_desc);
		desc_unbind(stmt->sql_attr_imp_row_desc, -1);
		freemem(stmt->sql_attr_imp_row_desc);
		freemem(stmt->text);
		freemem(stmt->row_buffer);
		freemem(stmt);
		exitsync();
		break;
	case SQL_HANDLE_DESC:
		entersync();
		desc = (DESCRIPTOR *) Handle;
		/* DM only allows apd->sql_desc_alloc_type = SQL_DESC_ALLOC_USER types */
		cnct = desc->cnct;
#if OS_WIN32
		__try {
			/* find if any statements on connection are using the descriptor */
			for (stmt = cnct->firststmt; stmt != NULL; stmt = stmt->nextstmt) {
				if (desc == stmt->sql_attr_app_param_desc) {
					stmt->sql_attr_app_param_desc = stmt->apdsave;
				}
				if (desc == stmt->sql_attr_app_row_desc) {
					stmt->sql_attr_app_row_desc = stmt->ardsave;
				}
			}
			/* remove from connections linked list */
			for (descp = &cnct->firstexplicitdesc; *descp != desc; descp = &(*descp)->nextexplicitdesc) {}
			*descp = desc->nextexplicitdesc;
		}
		__except( EXCEPTION_EXECUTE_HANDLER )
		{
			int i1 = GetExceptionCode();
			exitsync();
			error_record(&cnct->errors, "HY000", TEXT_HY000, "In SQLFreeHandle, SQL_HANDLE_DESC", i1);
			return SQL_ERROR;
		}
#else
		/* find if any statements on connection are using the descriptor */
		for (stmt = cnct->firststmt; stmt != NULL; stmt = stmt->nextstmt) {
			if (desc == stmt->sql_attr_app_param_desc) {
				stmt->sql_attr_app_param_desc = stmt->apdsave;
			}
			if (desc == stmt->sql_attr_app_row_desc) {
				stmt->sql_attr_app_row_desc = stmt->ardsave;
			}
		}
		/* remove from connections linked list */
		for (descp = &cnct->firstexplicitdesc; *descp != desc; descp = &(*descp)->nextexplicitdesc) {}
		*descp = desc->nextexplicitdesc;
#endif
		/* free descriptor records and descriptor */
		desc_unbind(desc, -1);
		freemem(desc);
		exitsync();
		break;
	}
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLFreeStmt(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT Option)
{
	SQLRETURN rc;
	STATEMENT *stmt;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	rc = SQL_SUCCESS;
	entersync();
	switch (Option) {
	case SQL_CLOSE:  /* close cursor and discard pending results */
		if (stmt->executeflags & EXECUTE_RSID) {
			if (server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD,
				NULL, 0, &stmt->errors, NULL, NULL) != SERVER_OK) {
				exitsync();
				rc = SQL_ERROR;
			}
		}
		stmt->executeflags &= EXECUTE_PREPARED;
		stmt->row_count = -1;
/*** CODE: SHOULD ROW_BUFFER BE FREED HERE OR WAIT TO FREEHANDLE ? ***/
/*		freemem(stmt->row_buffer); */
/*		stmt->row_buffer = NULL; */
		break;
	case SQL_UNBIND:
		desc_unbind(stmt->sql_attr_app_row_desc, 0);
/*** CODE: VERIFY THAT SQL_UNBIND APPLIES TO IRD ***/
/* I believe it does not. JR */
/*		stmt->sql_attr_imp_row_desc->sql_desc_count = 0; */
		break;
	case SQL_RESET_PARAMS:
		desc_unbind(stmt->sql_attr_app_param_desc, 0);
/*** CODE: VERIFY THAT SQL_RESET_PARAMS APPLIES TO IPD ***/
		stmt->sql_attr_imp_param_desc->sql_desc_count = 0;
		break;
	}
	exitsync();
	return rc;
}

SQLRETURN SQL_API SQLGetConnectAttr(
	SQLHDBC ConnectionHandle,
	SQLINTEGER Attribute,
	SQLPOINTER ValuePtr,
	SQLINTEGER BufferLength,
	SQLINTEGER *StringLengthPtr)
{
	INT copyflag;
	SQLINTEGER length;
	SQLRETURN rc;
	struct client_connection_struct *cnct;
	union {
		SQLUINTEGER sqluinteger;
		DWORD dword;
		SQLHANDLE handle;
	} ptr;

	cnct = (CONNECTION *) ConnectionHandle;
	error_init(&cnct->errors);

	rc = SQL_SUCCESS;
	copyflag = TRUE;
	length = sizeof(ptr.sqluinteger);
	switch (Attribute) {
	case SQL_ATTR_ACCESS_MODE:
		ptr.sqluinteger = cnct->sql_attr_access_mode;
		break;
	case SQL_ATTR_ASYNC_ENABLE:
		ptr.sqluinteger = cnct->sql_attr_async_enable;
		break;
	case SQL_ATTR_AUTO_IPD:  /* read-only */
		ptr.sqluinteger = cnct->sql_attr_auto_ipd;
		break;
	case SQL_ATTR_AUTOCOMMIT:
		ptr.sqluinteger = cnct->sql_attr_autocommit;
		break;
	case SQL_ATTR_CONNECTION_DEAD:  /* read-only */
		ptr.sqluinteger = cnct->sql_attr_auto_ipd;
		break;
	case SQL_ATTR_CONNECTION_TIMEOUT:
		ptr.sqluinteger = cnct->sql_attr_connection_timeout;
		break;
	case SQL_ATTR_CURRENT_CATALOG:
		length = (cnct->sql_attr_current_catalog != NULL) ?
				(SQLINTEGER)strlen((const char *)cnct->sql_attr_current_catalog) : 0;
		if (ValuePtr != NULL) {
			if (length < BufferLength) {
				BufferLength = length;
				((SQLCHAR *) ValuePtr)[BufferLength] = '\0';
			}
			else {
				if (BufferLength > 0) ((SQLCHAR *) ValuePtr)[--BufferLength] = '\0';
				error_record(&(cnct->errors), "01004", TEXT_01004, NULL, 0);
				rc = SQL_SUCCESS_WITH_INFO;
			}
			if (BufferLength > 0) memcpy(ValuePtr, cnct->sql_attr_current_catalog, BufferLength);
		}
		copyflag = FALSE;
		break;
	case SQL_ATTR_LOGIN_TIMEOUT:
		ptr.sqluinteger = cnct->sql_attr_login_timeout;
		break;
	case SQL_ATTR_METADATA_ID:
		ptr.sqluinteger = cnct->sql_attr_metadata_id;
		break;
	case SQL_ATTR_ODBC_CURSORS:  /* DM */
		length = 0;
		break;
	case SQL_ATTR_PACKET_SIZE:
		ptr.sqluinteger = cnct->sql_attr_packet_size;
		break;
	case SQL_ATTR_QUIET_MODE:
		ptr.handle = cnct->sql_attr_quiet_mode;
		length = sizeof(ptr.handle);
		break;
	case SQL_ATTR_TRACE:  /* DM */
		length = 0;
		break;
	case SQL_ATTR_TRACEFILE:  /* DM */
		length = 0;
		break;
	case SQL_ATTR_TRANSLATE_LIB:
		length = (cnct->sql_attr_translate_lib != NULL) ?
				(SQLINTEGER)strlen((const char *)cnct->sql_attr_translate_lib) : 0;
		if (ValuePtr != NULL) {
			if (length < BufferLength) {
				BufferLength = length;
				((SQLCHAR *) ValuePtr)[BufferLength] = '\0';
			}
			else {
				if (BufferLength > 0) ((SQLCHAR *) ValuePtr)[--BufferLength] = '\0';
				error_record(&(cnct->errors), "01004", TEXT_01004, NULL, 0);
				rc = SQL_SUCCESS_WITH_INFO;
			}
			if (BufferLength > 0) memcpy(ValuePtr, cnct->sql_attr_translate_lib, BufferLength);
		}
		copyflag = FALSE;
		break;
	case SQL_ATTR_TRANSLATE_OPTION:
		ptr.dword = cnct->sql_attr_translate_option;
		length = sizeof(ptr.dword);
		break;
	case SQL_ATTR_TXN_ISOLATION:
		ptr.dword = cnct->sql_attr_txn_isolation;
		length = sizeof(ptr.dword);
		break;
	default:  /* invalid attribute */
		error_record(&cnct->errors, "HYC00", TEXT_HYC00, NULL, 0);
		return SQL_ERROR;
	}
	if (copyflag && ValuePtr != NULL) memcpy(ValuePtr, &ptr, length);
	if (StringLengthPtr != NULL) *StringLengthPtr = length;
	return rc;
}

SQLRETURN SQL_API SQLGetCursorName(
	SQLHSTMT StatementHandle,
	SQLCHAR *CursorName,
	SQLSMALLINT BufferLength,
	SQLSMALLINT *NameLengthPtr)
{
	INT i1;
	SQLSMALLINT length;
	BYTE c1, *ptr;
	STATEMENT *stmt;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	if (!stmt->cursor_name[0]) {  /* create cursor name */
		strcpy(stmt->cursor_name, "SQL_CUR_");
		ptr = (BYTE *) &StatementHandle;
		for (i1 = 0; (size_t)i1++ < sizeof(StatementHandle); ) {
			c1 = (ptr[sizeof(StatementHandle) - i1] >> 4) & 0x0F;
			if (c1 < 10) c1 += '0';
			else c1 += 'A' - 10;
			stmt->cursor_name[6 + (i1 << 1)] = (CHAR) c1;
			c1 = ptr[sizeof(StatementHandle) - i1] & 0x0F;
			if (c1 < 10) c1 += '0';
			else c1 += 'A' - 10;
			stmt->cursor_name[7 + (i1 << 1)] = (CHAR) c1;
		}
		stmt->cursor_name[6 + (i1 << 1)] = '\0';
	}

	length = (SQLSMALLINT) strlen(stmt->cursor_name);
	if (CursorName != NULL) {
		if (BufferLength > 0) memcpy(CursorName, stmt->cursor_name, min(length + 1, BufferLength));
		if (length >= BufferLength) {
			if (BufferLength > 0) CursorName[BufferLength - 1] = '\0';
			error_record(&stmt->errors, "01004", TEXT_01004, NULL, 0);
			return SQL_SUCCESS_WITH_INFO;
		}
	}
	if (NameLengthPtr != NULL) *NameLengthPtr = length;
	return SQL_SUCCESS;
}

/**
 * StatementHandle		[Input] Statement handle
 * ColumnNumber			[Input] For retrieving column data, it is the number of the column
 * 						for which to return data. Result set columns are numbered in increasing
 * 						column order starting at 1. The bookmark column is column number 0;
 * 						this can be specified only if bookmarks are enabled.
 * 						For retrieving parameter data, it is the ordinal of the parameter, which starts at 1.
 * TargetType			[Input] The type identifier of the C data type of the *TargetValuePtr buffer.
 * TargetValuePtr		[Output] Pointer to the buffer in which to return the data.
 * BufferLength			[Input] Length of the *TargetValuePtr buffer in bytes.
 */
SQLRETURN SQL_API SQLGetData(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT ColumnNumber,
	SQLSMALLINT TargetType,
	SQLPOINTER TargetValuePtr,
	SQLLEN BufferLength,
	SQLLEN *StrLen_or_IndPtr)
{
	INT columnlen, precision, scale;
	SQLRETURN rc;
	UCHAR *bufptr;
	STATEMENT *stmt;
	APP_DESC_RECORD *ard_rec;
	IMP_ROW_DESC_RECORD *ird_rec;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	if (!(stmt->executeflags & EXECUTE_RESULTSET)) {
		if (stmt->executeflags & EXECUTE_EXECUTED)
			error_record(&stmt->errors, "24000", TEXT_24000, "No result set", 0);
		else error_record(&stmt->errors, "HY010", TEXT_HY010, "Statement has not been executed", 0);
		return SQL_ERROR;
	}
	if (!(stmt->executeflags & EXECUTE_FETCH)) {
		error_record(&stmt->errors, "24000", TEXT_24000, "Statement has not been fetched", 0);
		return SQL_ERROR;
	}

	if (ColumnNumber == 0) {
		error_record(&stmt->errors, "07009", TEXT_07009, "Bookmarks are not supported", 0);
		return SQL_ERROR;
	}
	if (!stmt->row_count) return SQL_NO_DATA;

	entersync();
	if (TargetType == SQL_ARD_TYPE) {
		/*
		 * The SQL_ARD_TYPE type identifier is used to indicate that the data in a buffer will be of the
		 * type specified in the SQL_DESC_CONCISE_TYPE field of the ARD
		 */
		if (ColumnNumber > stmt->sql_attr_app_row_desc->sql_desc_count) {
			error_record(&stmt->errors, "07009", TEXT_07009, "Column number greater than ARD count", 0);
			exitsync();
			return SQL_ERROR;
		}
		ard_rec = (APP_DESC_RECORD *) desc_get(stmt->sql_attr_app_row_desc, stmt->sql_attr_app_row_desc->sql_desc_count, FALSE);
		TargetType = ard_rec->sql_desc_concise_type;
		if (TargetType == SQL_C_NUMERIC) {
			precision = ard_rec->sql_desc_precision;
			scale = ard_rec->sql_desc_scale;
		}
	}
	else if (TargetType == SQL_C_NUMERIC) {
		if (stmt->cnct->env->sql_attr_odbc_version == SQL_OV_ODBC2) precision = scale = -1;
		else {
			precision = 31;
			scale = 0;
		}
	}

	if (ColumnNumber > stmt->sql_attr_imp_row_desc->sql_desc_count) {
		error_record(&stmt->errors, "07009", TEXT_07009, "Column number greater than IRD count", 0);
		exitsync();
		return SQL_ERROR;
	}
	ird_rec = (IMP_ROW_DESC_RECORD *) desc_get(stmt->sql_attr_imp_row_desc, ColumnNumber, FALSE);
	columnlen = ird_rec->fieldlength;
	if (ird_rec->rowbufoffset >= stmt->row_length) columnlen = 0;
	else if (columnlen > stmt->row_length - ird_rec->rowbufoffset) columnlen = stmt->row_length - ird_rec->rowbufoffset;
	bufptr = stmt->row_buffer + ird_rec->rowbufoffset;
	if (ColumnNumber == stmt->lastcolumn) {
		if (stmt->coloffset >= columnlen) {
			exitsync();
			return SQL_NO_DATA;
		}
		columnlen -= stmt->coloffset;
		bufptr += stmt->coloffset;
	}
	rc = exec_sqltoc(stmt, ird_rec, ColumnNumber, (CHAR*)bufptr, columnlen, TargetType, precision, scale,
		TargetValuePtr, BufferLength, StrLen_or_IndPtr, StrLen_or_IndPtr, 0, 0, 0);
	if (rc != SQL_ERROR) {
		if (ColumnNumber == stmt->lastcolumn) {
			stmt->coloffset += (INT)min(BufferLength, columnlen);
			if (BufferLength <= columnlen) stmt->coloffset--;
		}
		else {
			stmt->lastcolumn = ColumnNumber;
			if (BufferLength > 0) {
				stmt->coloffset = (INT)min(BufferLength, columnlen);
				if (BufferLength <= columnlen) stmt->coloffset--;
			}
			else stmt->coloffset = 0;
		}
	}
	exitsync();
	return rc;
}

SQLRETURN SQL_API SQLGetDescField(
	SQLHDESC DescriptorHandle,
	SQLSMALLINT RecNumber,
	SQLSMALLINT FieldIdentifier,
	SQLPOINTER ValuePtr,
	SQLINTEGER BufferLength,
	SQLINTEGER *StringLengthPtr)
{
	SQLINTEGER length;
	SQLRETURN rc = SQL_SUCCESS;
	DESCRIPTOR *desc;
	LPVOID drec;
	SQLCHAR *sqlchar = NULL;
	union {
		SQLSMALLINT smallint;
		SQLUSMALLINT usmallint;
		SQLINTEGER integer;
		SQLUINTEGER uinteger;
		SQLPOINTER pointer;
	} ptr;

	desc = (DESCRIPTOR *) DescriptorHandle;
	error_init(&desc->errors);
	entersync();
	switch (FieldIdentifier) {
	case SQL_DESC_ALLOC_TYPE:
		ptr.smallint = desc->sql_desc_alloc_type;
		length = sizeof(SQLSMALLINT);
		break;
	case SQL_DESC_ARRAY_SIZE:
		ptr.uinteger = desc->sql_desc_array_size;
		length = sizeof(SQLUINTEGER);
		break;
	case SQL_DESC_ARRAY_STATUS_PTR:
		ptr.pointer = desc->sql_desc_array_status_ptr;
		length = sizeof(SQLPOINTER);
		break;
	case SQL_DESC_BIND_OFFSET_PTR:
		ptr.pointer = desc->sql_desc_bind_offset_ptr;
		length = sizeof(SQLPOINTER);
		break;
	case SQL_DESC_BIND_TYPE:
		ptr.uinteger = desc->sql_desc_bind_type;
		length = sizeof(SQLUINTEGER);
		break;
	case SQL_DESC_COUNT:
		ptr.smallint = desc->sql_desc_count;
		length = sizeof(SQLSMALLINT);
		break;
	case SQL_DESC_ROWS_PROCESSED_PTR:
		ptr.pointer = desc->sql_desc_rows_processed_ptr;
		length = sizeof(SQLPOINTER);
		break;
	default:
		if (RecNumber < 0) {
			error_record(&desc->errors, "07009", TEXT_07009, NULL, 0);
			exitsync();
			return SQL_ERROR;
		}
		if (RecNumber == 0) {
			error_record(&desc->errors, "HY000", "Record number is zero; Driver does not support bookmark records", NULL, 0);
			exitsync();
			return SQL_ERROR;
		}
		if (RecNumber > desc->sql_desc_count) {
			exitsync();
			return SQL_NO_DATA;
		}
		drec = desc_get(desc, RecNumber, FALSE);
		switch (FieldIdentifier) {
		case SQL_DESC_AUTO_UNIQUE_VALUE:			/*  11 */
			if (desc->type == TYPE_IMP_ROW_DESC)
				ptr.integer = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_auto_unique_value;
			length = sizeof(SQLINTEGER);
			break;
		case SQL_DESC_BASE_COLUMN_NAME:				/*  22 */
			if (desc->type == TYPE_IMP_ROW_DESC)
				sqlchar = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_base_column_name;
			else sqlchar = (SQLCHAR*)"";
			break;
		case SQL_DESC_BASE_TABLE_NAME:				/*  23 */
			if (desc->type == TYPE_IMP_ROW_DESC)
				sqlchar = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_base_table_name;
			else sqlchar = (SQLCHAR*)"";
			break;
		case SQL_DESC_CASE_SENSITIVE:				/*  12 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				ptr.integer = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_case_sensitive;
				break;
			case TYPE_IMP_ROW_DESC:
				ptr.integer = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_case_sensitive;
				break;
			}
			length = sizeof(SQLINTEGER);
			break;
		case SQL_DESC_CATALOG_NAME:					/*  17 */
			if (desc->type == TYPE_IMP_ROW_DESC)
				sqlchar = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_catalog_name;
			else sqlchar = (SQLCHAR*)"";
			break;
		case SQL_DESC_CONCISE_TYPE:					/*   2 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				ptr.smallint = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_concise_type;
				break;
			case TYPE_IMP_ROW_DESC:
				ptr.smallint = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_concise_type;
				break;
			case TYPE_APP_DESC:
				ptr.smallint = ((APP_DESC_RECORD *) drec)->sql_desc_concise_type;
				break;
			}
			length = sizeof(SQLSMALLINT);
			break;
		case SQL_DESC_DATA_PTR:						/* 1010 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				ptr.pointer = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_data_ptr;
				break;
			case TYPE_APP_DESC:
				ptr.pointer = ((APP_DESC_RECORD *) drec)->sql_desc_data_ptr;
				break;
			}
			length = sizeof(SQLPOINTER);
			break;
		case SQL_DESC_DATETIME_INTERVAL_CODE:		/* 1007 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				ptr.smallint = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_datetime_interval_code;
				break;
			case TYPE_IMP_ROW_DESC:
				ptr.smallint = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_datetime_interval_code;
				break;
			case TYPE_APP_DESC:
				ptr.smallint = ((APP_DESC_RECORD *) drec)->sql_desc_datetime_interval_code;
				break;
			}
			length = sizeof(SQLSMALLINT);
			break;
		case SQL_DESC_DATETIME_INTERVAL_PRECISION:	/*  26 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				ptr.integer = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_datetime_interval_precision;
				break;
			case TYPE_IMP_ROW_DESC:
				ptr.integer = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_datetime_interval_precision;
				break;
			case TYPE_APP_DESC:
				ptr.integer = ((APP_DESC_RECORD *) drec)->sql_desc_datetime_interval_precision;
				break;
			}
			length = sizeof(SQLINTEGER);
			break;
		case SQL_DESC_DISPLAY_SIZE:					/*   6 */
			if (desc->type == TYPE_IMP_ROW_DESC)
				ptr.integer = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_display_size;
			length = sizeof(SQLINTEGER);
			break;
		case SQL_DESC_FIXED_PREC_SCALE:				/*   9 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				ptr.smallint = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_fixed_prec_scale;
				break;
			case TYPE_IMP_ROW_DESC:
				ptr.smallint = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_fixed_prec_scale;
				break;
			}
			length = sizeof(SQLSMALLINT);
			break;
		case SQL_DESC_INDICATOR_PTR:				/* 1009 */
			if (desc->type == TYPE_APP_DESC)
				ptr.pointer = ((APP_DESC_RECORD *) drec)->sql_desc_indicator_ptr;
			length = sizeof(SQLPOINTER);
			break;
		case SQL_DESC_LABEL:						/*   18 */
			if (desc->type == TYPE_IMP_ROW_DESC)
				sqlchar = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_label;
			else sqlchar = (SQLCHAR*)"";
			break;
		case SQL_DESC_LENGTH:						/* 1003 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				ptr.uinteger = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_length;
				break;
			case TYPE_IMP_ROW_DESC:
				ptr.uinteger = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_length;
				break;
			case TYPE_APP_DESC:
				ptr.uinteger = ((APP_DESC_RECORD *) drec)->sql_desc_length;
				break;
			}
			length = sizeof(SQLUINTEGER);
			break;
		case SQL_DESC_LITERAL_PREFIX:				/*   27 */
			if (desc->type == TYPE_IMP_ROW_DESC)
				sqlchar = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_literal_prefix;
			else sqlchar = (SQLCHAR*)"";
			break;
		case SQL_DESC_LITERAL_SUFFIX:				/*   28 */
			if (desc->type == TYPE_IMP_ROW_DESC)
				sqlchar = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_literal_suffix;
			else sqlchar = (SQLCHAR*)"";
			break;
		case SQL_DESC_LOCAL_TYPE_NAME:				/*   29 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				sqlchar = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_local_type_name;
				break;
			case TYPE_IMP_ROW_DESC:
				sqlchar = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_local_type_name;
				break;
			default:
				sqlchar = (SQLCHAR*)"";
			}
			break;
		case SQL_DESC_NAME:							/* 1011 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				sqlchar = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_name;
				break;
			case TYPE_IMP_ROW_DESC:
				sqlchar = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_name;
				break;
			default:
				sqlchar = (SQLCHAR*)"";
			}
			break;
		case SQL_DESC_NULLABLE:						/* 1008 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				ptr.smallint = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_nullable;
				break;
			case TYPE_IMP_ROW_DESC:
				ptr.smallint = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_nullable;
				break;
			}
			length = sizeof(SQLSMALLINT);
			break;
		case SQL_DESC_NUM_PREC_RADIX:				/*   32 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				ptr.uinteger = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_num_prec_radix;
				break;
			case TYPE_IMP_ROW_DESC:
				ptr.uinteger = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_num_prec_radix;
				break;
			case TYPE_APP_DESC:
				ptr.uinteger = ((APP_DESC_RECORD *) drec)->sql_desc_num_prec_radix;
				break;
			}
			length = sizeof(SQLUINTEGER);
			break;
		case SQL_DESC_OCTET_LENGTH:					/* 1013 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				ptr.uinteger = (SQLUINTEGER)((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_octet_length;
				break;
			case TYPE_IMP_ROW_DESC:
				ptr.uinteger = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_octet_length;
				break;
			case TYPE_APP_DESC:
				ptr.uinteger = (SQLUINTEGER)((APP_DESC_RECORD *) drec)->sql_desc_octet_length;
				break;
			}
			length = sizeof(SQLUINTEGER);
			break;
		case SQL_DESC_OCTET_LENGTH_PTR:				/* 1004 */
			if (desc->type == TYPE_APP_DESC)
				ptr.pointer = ((APP_DESC_RECORD *) drec)->sql_desc_octet_length_ptr;
			length = sizeof(SQLPOINTER);
			break;
		case SQL_DESC_PARAMETER_TYPE:				/*   33 */
			if (desc->type == TYPE_IMP_PARAM_DESC)
				ptr.smallint = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_parameter_type;
			length = sizeof(SQLSMALLINT);
			break;
		case SQL_DESC_PRECISION:					/* 1005 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				ptr.smallint = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_precision;
				break;
			case TYPE_IMP_ROW_DESC:
				ptr.smallint = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_precision;
				break;
			case TYPE_APP_DESC:
				ptr.smallint = ((APP_DESC_RECORD *) drec)->sql_desc_precision;
				break;
			}
			length = sizeof(SQLSMALLINT);
			break;
		case SQL_DESC_ROWVER:						/*   35 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				ptr.smallint = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_rowver;
				break;
			case TYPE_IMP_ROW_DESC:
				ptr.smallint = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_rowver;
				break;
			}
			length = sizeof(SQLSMALLINT);
			break;
		case SQL_DESC_SCALE:						/* 1006 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				ptr.smallint = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_scale;
				break;
			case TYPE_IMP_ROW_DESC:
				ptr.smallint = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_scale;
				break;
			case TYPE_APP_DESC:
				ptr.smallint = ((APP_DESC_RECORD *) drec)->sql_desc_scale;
				break;
			}
			length = sizeof(SQLSMALLINT);
			break;
		case SQL_DESC_SCHEMA_NAME:					/*   16 */
			if (desc->type == TYPE_IMP_ROW_DESC)
				sqlchar = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_schema_name;
			else sqlchar = (SQLCHAR*)"";
			break;
		case SQL_DESC_SEARCHABLE:					/*   13 */
			if (desc->type == TYPE_IMP_ROW_DESC)
				ptr.smallint = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_searchable;
			length = sizeof(SQLSMALLINT);
			break;
		case SQL_DESC_TABLE_NAME:					/*   15 */
			if (desc->type == TYPE_IMP_ROW_DESC)
				sqlchar = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_table_name;
			else sqlchar = (SQLCHAR*)"";
			break;
		case SQL_DESC_TYPE:							/* 1002 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				ptr.smallint = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_type;
				break;
			case TYPE_IMP_ROW_DESC:
				ptr.smallint = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_type;
				break;
			case TYPE_APP_DESC:
				ptr.smallint = ((APP_DESC_RECORD *) drec)->sql_desc_type;
				break;
			}
			length = sizeof(SQLSMALLINT);
			break;
		case SQL_DESC_TYPE_NAME:					/*   14 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				sqlchar = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_type_name;
				break;
			case TYPE_IMP_ROW_DESC:
				sqlchar = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_type_name;
				break;
			default:
				sqlchar = (SQLCHAR*)"";
			}
			break;
		case SQL_DESC_UNNAMED:						/* 1012 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				ptr.smallint = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_unnamed;
				break;
			case TYPE_IMP_ROW_DESC:
				ptr.smallint = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_unnamed;
				break;
			}
			length = sizeof(SQLSMALLINT);
			break;
		case SQL_DESC_UNSIGNED:						/*    8 */
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				ptr.smallint = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_unsigned;
				break;
			case TYPE_IMP_ROW_DESC:
				ptr.smallint = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_unsigned;
				break;
			}
			length = sizeof(SQLSMALLINT);
			break;
		case SQL_DESC_UPDATABLE:					/*   10 */
			if (desc->type == TYPE_IMP_ROW_DESC)
				ptr.smallint = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_updatable;
			length = sizeof(SQLSMALLINT);
			break;
		default:
			error_record(&desc->errors, "HY091", HY091TEXT, NULL, 0);
			exitsync();
			return SQL_ERROR;
		}
	}

	if (sqlchar != NULL) {
		length = (SQLINTEGER)strlen((const char *)sqlchar);
		if (ValuePtr != NULL) {
			memcpy(ValuePtr, sqlchar, min(length + 1, BufferLength));
			if (length >= BufferLength) {
				if (BufferLength > 0) ((SQLCHAR *) ValuePtr)[BufferLength - 1] = '\0';
				error_record(&desc->errors, "01004", TEXT_01004, NULL, 0);
				rc = SQL_SUCCESS_WITH_INFO;
			}
		}
	}
	else if (ValuePtr != NULL) memcpy(ValuePtr, &ptr, length);
	if (StringLengthPtr != NULL) *StringLengthPtr = length;
	exitsync();
	return rc;
}

SQLRETURN SQL_API SQLGetDescRec(
	SQLHDESC DescriptorHandle,
	SQLSMALLINT RecNumber,
	SQLCHAR *Name,
	SQLSMALLINT BufferLength,
	SQLSMALLINT *StringLengthPtr,
	SQLSMALLINT *TypePtr,
	SQLSMALLINT *SubTypePtr,
	SQLLEN *LengthPtr,
	SQLSMALLINT *PrecisionPtr,
	SQLSMALLINT *ScalePtr,
	SQLSMALLINT *NullablePtr)
{
	SQLSMALLINT length;
	SQLRETURN rc;
	SQLCHAR *sqlchar;
	DESCRIPTOR *desc;
	LPVOID drec;

	desc = (DESCRIPTOR *) DescriptorHandle;
	error_init(&desc->errors);

	if (RecNumber < 0) {
		error_record(&desc->errors, "07009", TEXT_07009, NULL, 0);
		return SQL_ERROR;
	}
	if (RecNumber == 0) {
		error_record(&desc->errors, "07009", TEXT_07009, "Bookmarks are not supported", 0);
		return SQL_ERROR;
	}
	if (RecNumber > desc->sql_desc_count) return SQL_NO_DATA;
	rc = SQL_SUCCESS;
	entersync();
	drec = desc_get(desc, RecNumber, FALSE);

	/* return SQL_DESC_NAME field */
	if (desc->type == TYPE_IMP_ROW_DESC)
		sqlchar = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_name;
	else if (desc->type == TYPE_IMP_PARAM_DESC)
		sqlchar = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_name;
	else sqlchar = (SQLCHAR*)"";
	length = (SQLSMALLINT)strlen((const char *)sqlchar);
	if (Name != NULL) {
		memcpy(Name, sqlchar, min(length + 1, BufferLength));
		if (length >= BufferLength) {
			if (BufferLength > 0) Name[BufferLength - 1] = '\0';
			error_record(&desc->errors, "01004", TEXT_01004, NULL, 0);
			rc = SQL_SUCCESS_WITH_INFO;
		}
	}
	if (StringLengthPtr != NULL) *StringLengthPtr = length;

	/* return SQL_DESC_TYPE */
	if (TypePtr != NULL) {
		switch (desc->type) {
		case TYPE_IMP_PARAM_DESC:
			*TypePtr = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_type;
			break;
		case TYPE_IMP_ROW_DESC:
			*TypePtr = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_type;
			break;
		case TYPE_APP_DESC:
			*TypePtr = ((APP_DESC_RECORD *) drec)->sql_desc_type;
			break;
		}
	}

	/* return SQL_DESC_DATETIME_INTERVAL_CODE */
	if (SubTypePtr != NULL) {
		switch (desc->type) {
		case TYPE_IMP_PARAM_DESC:
			*SubTypePtr = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_datetime_interval_code;
			break;
		case TYPE_IMP_ROW_DESC:
			*SubTypePtr = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_datetime_interval_code;
			break;
		case TYPE_APP_DESC:
			*SubTypePtr = ((APP_DESC_RECORD *) drec)->sql_desc_datetime_interval_code;
			break;
		}
	}

	/* return SQL_DESC_OCTET_LENGTH */
	if (LengthPtr != NULL) {
		switch (desc->type) {
		case TYPE_IMP_PARAM_DESC:
			*LengthPtr = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_octet_length;
			break;
		case TYPE_IMP_ROW_DESC:
			*LengthPtr = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_octet_length;
			break;
		case TYPE_APP_DESC:
			*LengthPtr = ((APP_DESC_RECORD *) drec)->sql_desc_octet_length;
			break;
		}
	}

	/* return SQL_DESC_PRECISION */
	if (PrecisionPtr != NULL) {
		switch (desc->type) {
		case TYPE_IMP_PARAM_DESC:
			*PrecisionPtr = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_precision;
			break;
		case TYPE_IMP_ROW_DESC:
			*PrecisionPtr = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_precision;
			break;
		case TYPE_APP_DESC:
			*PrecisionPtr = ((APP_DESC_RECORD *) drec)->sql_desc_precision;
			break;
		}
	}

	/* return SQL_DESC_SCALE */
	if (ScalePtr != NULL) {
		switch (desc->type) {
		case TYPE_IMP_PARAM_DESC:
			*ScalePtr = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_scale;
			break;
		case TYPE_IMP_ROW_DESC:
			*ScalePtr = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_scale;
			break;
		case TYPE_APP_DESC:
			*ScalePtr = ((APP_DESC_RECORD *) drec)->sql_desc_scale;
			break;
		}
	}

	/* return SQL_DESC_NULLABLE */
	if (NullablePtr != NULL) {
		switch (desc->type) {
		case TYPE_IMP_PARAM_DESC:
			*NullablePtr = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_nullable;
			break;
		case TYPE_IMP_ROW_DESC:
			*NullablePtr = ((IMP_ROW_DESC_RECORD *) drec)->sql_desc_nullable;
			break;
		}
	}
	exitsync();
	return rc;
}

SQLRETURN SQL_API SQLGetDiagField(
	SQLSMALLINT HandleType,
	SQLHANDLE Handle,
	SQLSMALLINT RecNumber,
	SQLSMALLINT DiagIdentifier,
	SQLPOINTER DiagInfoPtr,
	SQLSMALLINT BufferLength,
	SQLSMALLINT *StringLengthPtr)
{
	SQLSMALLINT length, rec, cb;
	SQLRETURN rc;
	STATEMENT *stmtHandle = NULL;
	DESCRIPTOR *descHandle = NULL;
	ERRORS er;
	SQLINTEGER returnInt, i1;
	SQLCHAR *sqlchar;
	CHAR message[sizeof(VENDOR_ID) + sizeof(ODBC_ID) + MAX_ERROR_MSG];
	static CHAR *odbcsubclass[] = {
		"01S00", "01S01", "01S02", "01S06", "01S07",
		"07S01", "08S01", "21S01", "21S02", "25S01",
		"25S02", "25S03", "42S01", "42S02", "42S11",
		"42S12", "42S21", "42S22", "HY095", "HY097",
		"HY098", "HY099", "HY100", "HY101", "HY105",
		"HY107", "HY109", "HY110", "HY111", "HYT00",
		"HYT01", "IM001", "IM002", "IM003", "IM004",
		"IM005", "IM006", "IM007", "IM008", "IM010",
		"IM011", "IM012", NULL
	};

	switch (HandleType) {
	case SQL_HANDLE_ENV:
		er = ((ENVIRONMENT *) Handle)->errors;
		break;
	case SQL_HANDLE_DBC:
		er = ((struct client_connection_struct *) Handle)->errors;
		break;
	case SQL_HANDLE_STMT:
		stmtHandle = (STATEMENT *)Handle;
		er = stmtHandle->errors;
		break;
	case SQL_HANDLE_DESC:
		descHandle = (DESCRIPTOR *)Handle;
		er = descHandle->errors;
		break;
	default:
		return SQL_ERROR;  /* undefined handle type */
	}

	rc = SQL_SUCCESS;
	sqlchar = NULL;
	length = sizeof(SQLINTEGER);
	switch (DiagIdentifier) {
	case SQL_DIAG_CURSOR_ROW_COUNT:
		if (HandleType != SQL_HANDLE_STMT) return SQL_ERROR;
		returnInt = stmtHandle->row_count;
		break;
	case SQL_DIAG_DYNAMIC_FUNCTION:
		if (HandleType != SQL_HANDLE_STMT) return SQL_ERROR;
		sqlchar = er.dynfunc;
		break;
	case SQL_DIAG_DYNAMIC_FUNCTION_CODE:
		if (HandleType != SQL_HANDLE_STMT) return SQL_ERROR;
		returnInt = er.dfcode;
		break;
	case SQL_DIAG_NUMBER:
		returnInt = er.numerrors;
		break;
	/* case SQL_DIAG_RETURNCODE:  Always handled by the DM */
	case SQL_DIAG_ROW_COUNT:
		if (HandleType != SQL_HANDLE_STMT) return SQL_ERROR;
		returnInt = stmtHandle->errors.rowcount;
		break;
	default:
		if (RecNumber < 1) return SQL_ERROR;
		if (RecNumber > er.numerrors) return SQL_NO_DATA;
		rec = RecNumber - 1;
		switch (DiagIdentifier) {
		case SQL_DIAG_CLASS_ORIGIN:
			if (!strncmp((const char *)er.errorinfo[rec].sqlstate, "IM", 2)) sqlchar = (SQLCHAR*)"ODBC 3.0";
			else sqlchar = (SQLCHAR*)"ISO 9075";
			break;
		case SQL_DIAG_COLUMN_NUMBER:
			if (HandleType != SQL_HANDLE_STMT) return SQL_ERROR;
			returnInt = er.errorinfo[rec].columnNumber;
			break;
		case SQL_DIAG_CONNECTION_NAME:
			/*** CODE: MAYBE SUPPORT BETTER ***/
			sqlchar = (SQLCHAR*)"";
			break;
		case SQL_DIAG_MESSAGE_TEXT:
			sprintf(message, "%s%s%s", VENDOR_ID, ODBC_ID, er.errorinfo[rec].text);
			sqlchar = (SQLCHAR*)message;
			break;
		case SQL_DIAG_NATIVE:
			returnInt = er.errorinfo[rec].native;
			break;
		case SQL_DIAG_ROW_NUMBER:
			if (HandleType != SQL_HANDLE_STMT) return SQL_ERROR;
			returnInt = er.errorinfo[rec].rowNumber;
			break;
		case SQL_DIAG_SERVER_NAME:
			switch (HandleType) {
			case SQL_HANDLE_ENV:
				sqlchar = (SQLCHAR*)"";
				break;
			case SQL_HANDLE_DBC:
				SQLGetInfo((SQLHDBC)Handle, SQL_DATA_SOURCE_NAME, message, sizeof(message), &cb);
				sqlchar = (SQLCHAR*)message;
				break;
			case SQL_HANDLE_STMT:
				if (stmtHandle->cnct != NULL) {
					SQLGetInfo((SQLHDBC)stmtHandle->cnct, SQL_DATA_SOURCE_NAME, message, sizeof(message), &cb);
					sqlchar = (SQLCHAR*)message;
				}
				else sqlchar = (SQLCHAR*)"";
				break;
			case SQL_HANDLE_DESC:
				if (descHandle->cnct != NULL) {
					SQLGetInfo((SQLHDBC)descHandle->cnct, SQL_DATA_SOURCE_NAME, message, sizeof(message), &cb);
					sqlchar = (SQLCHAR*)message;
				}
				else sqlchar = (SQLCHAR*)"";
				break;
			}
			break;
		case SQL_DIAG_SQLSTATE:
			sqlchar = er.errorinfo[rec].sqlstate;
			break;
		case SQL_DIAG_SUBCLASS_ORIGIN:
			sqlchar = (SQLCHAR*)"ISO 9075";
			for (i1 = 0; odbcsubclass[i1] != NULL; i1++) {
				if (!strcmp((const char *)er.errorinfo[rec].sqlstate, odbcsubclass[i1])) {
					sqlchar = (SQLCHAR*)"ODBC 3.0";
					break;
				}
			}
			break;
		default:
			return SQL_ERROR;
		}
	}

/*** CODE: VERIFY THE VALUES OF BufferLength ***/
	if (sqlchar != NULL) {
		length = (SQLSMALLINT)strlen((const char *)sqlchar);
		if (DiagInfoPtr != NULL) {
			memcpy(DiagInfoPtr, sqlchar, min(length + 1, BufferLength));
			if (length >= BufferLength) {
				if (BufferLength > 0) ((SQLCHAR *) DiagInfoPtr)[BufferLength - 1] = '\0';
				rc = SQL_SUCCESS_WITH_INFO;
			}
		}
	}
	else if (DiagInfoPtr != NULL) *((SQLINTEGER *) DiagInfoPtr) = returnInt;
	if (StringLengthPtr != NULL) *StringLengthPtr = length;
	return rc;
}

SQLRETURN SQL_API SQLGetDiagRec(
	SQLSMALLINT HandleType,
	SQLHANDLE Handle,
	SQLSMALLINT RecNumber,
	SQLCHAR *Sqlstate,
	SQLINTEGER *NativeErrorPtr,
	SQLCHAR *MessageText,
	SQLSMALLINT BufferLength,
	SQLSMALLINT *TextLengthPtr)
{
	CHAR message[sizeof(VENDOR_ID) + sizeof(ODBC_ID) + MAX_ERROR_MSG];
	ERRORS er;			/* error structure */
	ERRORINFO diag;
	SQLRETURN rc = SQL_SUCCESS;
	SQLSMALLINT errorlen;

	switch (HandleType) {
	case SQL_HANDLE_ENV:
		er = ((ENVIRONMENT *) Handle)->errors;
		break;
	case SQL_HANDLE_DBC:
		er = ((struct client_connection_struct *) Handle)->errors;
		break;
	case SQL_HANDLE_STMT:
		er = ((STATEMENT *) Handle)->errors;
		break;
	case SQL_HANDLE_DESC:
		er = ((DESCRIPTOR *) Handle)->errors;
		break;
	default:
		return SQL_ERROR;
	}

	if ((RecNumber < 1) || (BufferLength < 0)) {
/*** CODE: ERROR CODE GOES HERE ***/
		return SQL_ERROR;
	}

	if (er.numerrors < RecNumber) {  /* no errors or not that many errors */
		return SQL_NO_DATA;
	}

	diag = er.errorinfo[RecNumber - 1];
	/* copy current error message to output */
	if (Sqlstate != NULL) strcpy((char *)Sqlstate, (const char *)diag.sqlstate);
	if (NativeErrorPtr != NULL) *NativeErrorPtr = diag.native;

	sprintf(message, "%s%s%s", VENDOR_ID, ODBC_ID, diag.text);
	errorlen = (SQLSMALLINT)strlen(message);
	if (TextLengthPtr != NULL) *TextLengthPtr = errorlen;
	if (MessageText != NULL) {
		/* copy error message to user buffer but don't overrun buffer length */
		memcpy(MessageText, message, min((errorlen + 1), BufferLength));
		if (errorlen >= BufferLength) {	/* data too long, trunc and null terminate */
			if (BufferLength > 0) *(MessageText + BufferLength - 1) = '\0';
			rc = SQL_SUCCESS_WITH_INFO;
		}
	}
	return rc;
}

SQLRETURN SQL_API SQLGetEnvAttr(
	SQLHENV EnvironmentHandle,
	SQLINTEGER Attribute,
	SQLPOINTER ValuePtr,
	SQLINTEGER BufferLength,
	SQLINTEGER *StringLengthPtr)
{
	ENVIRONMENT *env;

	env = (ENVIRONMENT *) EnvironmentHandle;
	error_init(&env->errors);

	switch (Attribute) {
	case SQL_ATTR_CONNECTION_POOLING:  /* DM */
		break;
	case SQL_ATTR_CP_MATCH:  /* DM */
		break;
	case SQL_ATTR_ODBC_VERSION:
		if (ValuePtr != NULL) {
#if OS_UNIX
			*(SQLINTEGER *) ValuePtr = SQL_OV_ODBC3;
#else
			*(SQLINTEGER *) ValuePtr = env->sql_attr_odbc_version;
#endif
		}
		if (StringLengthPtr != NULL) *StringLengthPtr = sizeof(SQLINTEGER);
		break;
	case SQL_ATTR_OUTPUT_NTS:  /* DM */
		break;
	default:  /* should never get here */
		error_record(&env->errors, "HY092", TEXT_HY092, NULL, 0);
		return SQL_ERROR;
	}
	return SQL_SUCCESS;
}

/**
 * Is thread safe. Does not alloc/free memory. Does not call any other function
 */
SQLRETURN SQL_API SQLGetInfo(
	SQLHDBC ConnectionHandle,
	SQLUSMALLINT InfoType,
	SQLPOINTER InfoValuePtr,
	SQLSMALLINT BufferLength,
	SQLSMALLINT *StringLengthPtr)
{
	SQLSMALLINT length;			/* byte count of data returned to user */
	CHAR errmsg[512];			/* Formatted error message goes here */
	CHAR temp[64];
	BOOL chardata;				/* True if the return type is a string */
	SQLRETURN rc = SQL_SUCCESS;	/* Let's be optimistic */
	struct client_connection_struct *cnct;
	char* sqlchar;
	union {
		SQLUSMALLINT smallint;
		SQLUINTEGER	uinteger;
	} ptr;

	cnct = (CONNECTION *) ConnectionHandle;
	error_init(&cnct->errors);
	chardata = FALSE;		/* Most of the time the return is not character data. */

	switch (InfoType) {
	case SQL_ACCESSIBLE_PROCEDURES:		/*  20 */
		sqlchar = "N";
		chardata = TRUE;
		break;
	case SQL_ACCESSIBLE_TABLES:			/*  19 */
		sqlchar = "Y";
		chardata = TRUE;
		break;
	case SQL_ACTIVE_ENVIRONMENTS:		/* 116 */
		ptr.smallint = 0;
		length = sizeof(ptr.smallint);
		break;
	case SQL_AGGREGATE_FUNCTIONS:		/* 169 */
		ptr.uinteger = 0;
		if (cnct->server_majorver >= 3) {
			ptr.uinteger |= SQL_AF_ALL;
			ptr.uinteger |= SQL_AF_AVG;
			ptr.uinteger |= SQL_AF_COUNT;
			ptr.uinteger |= SQL_AF_DISTINCT;
			ptr.uinteger |= SQL_AF_MAX;
			ptr.uinteger |= SQL_AF_MIN;
			ptr.uinteger |= SQL_AF_SUM;
		}
		length = sizeof(ptr.uinteger);
		break;
	case SQL_ALTER_DOMAIN:				/* 117 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_ALTER_TABLE:				/*  86 */
		ptr.uinteger = 0;
		if (cnct->server_majorver >= 4) {
			ptr.uinteger |= SQL_AT_ADD_COLUMN_DEFAULT;
			ptr.uinteger |= SQL_AT_DROP_COLUMN_DEFAULT;
		}
		length = sizeof(ptr.uinteger);
		break;
	case SQL_ASYNC_MODE:				/*10021*/
		ptr.uinteger = SQL_AM_STATEMENT;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_BATCH_ROW_COUNT:			/* 120 */
	case SQL_BATCH_SUPPORT:				/* 121 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_BOOKMARK_PERSISTENCE:		/*  82 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_CATALOG_LOCATION:			/* 114 Formerly SQL_QUALIFIER_LOCATION*/
		ptr.smallint = 0;
		length = sizeof(ptr.smallint);
		break;
	case SQL_CATALOG_NAME:				/*10003*/
		sqlchar = "N";
		chardata = TRUE;
		break;
	case SQL_CATALOG_NAME_SEPARATOR:	/*  41 Formerly SQL_QUALIFIER_NAME_SEPARATOR*/
		sqlchar = "";
		chardata = TRUE;
		break;
	case SQL_CATALOG_TERM:				/*  42 Formerly SQL_QUALIFIER_TERM*/
		sqlchar = "";
		chardata = TRUE;
		break;
	case SQL_CATALOG_USAGE:				/*  92 Formerly SQL_QUALIFIER_USAGE*/
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_COLLATION_SEQ:				/*10004*/
		sqlchar = "ASCII";
		chardata = TRUE;
		break;
	case SQL_COLUMN_ALIAS:				/*  87 */
		sqlchar = "Y";
		chardata = TRUE;
		break;
	case SQL_CONCAT_NULL_BEHAVIOR:		/*  22 */
		ptr.smallint = SQL_CB_NULL;
		length = sizeof(ptr.smallint);
		break;
	case SQL_CONVERT_BIGINT:			/*  53 */
	case SQL_CONVERT_BINARY:			/*  54 */
	case SQL_CONVERT_BIT:				/*  55 */
	case SQL_CONVERT_CHAR:				/*  56 */
	case SQL_CONVERT_DATE:				/*  57 */
	case SQL_CONVERT_DECIMAL:			/*  58 */
	case SQL_CONVERT_DOUBLE:			/*  59 */
	case SQL_CONVERT_FLOAT:				/*  60 */
#if OS_WIN32
	case SQL_CONVERT_GUID:				/* 173 */
#else
	case 173:
#endif
	case SQL_CONVERT_INTEGER:			/*  61 */
	case SQL_CONVERT_INTERVAL_YEAR_MONTH:/* 123 */
	case SQL_CONVERT_INTERVAL_DAY_TIME:	/* 124 */
	case SQL_CONVERT_LONGVARBINARY: 	/*  71 */
	case SQL_CONVERT_LONGVARCHAR:		/*  62 */
	case SQL_CONVERT_NUMERIC:			/*  63 */
	case SQL_CONVERT_REAL:				/*  64 */
	case SQL_CONVERT_SMALLINT:			/*  65 */
	case SQL_CONVERT_TIME:				/*  66 */
	case SQL_CONVERT_TIMESTAMP:			/*  67 */
	case SQL_CONVERT_TINYINT:			/*  68 */
	case SQL_CONVERT_VARBINARY:			/*  69 */
	case SQL_CONVERT_VARCHAR:			/*  70 */
	case SQL_CONVERT_WCHAR:				/* 122 */
	case SQL_CONVERT_WLONGVARCHAR: 		/* 125 */
	case SQL_CONVERT_WVARCHAR:			/* 126 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_CONVERT_FUNCTIONS:			/*  48 */
		ptr.uinteger = 0;
		if (cnct->server_majorver >= 4) {
			ptr.uinteger |= SQL_FN_CVT_CAST;
		}
		length = sizeof(ptr.uinteger);
		break;
	case SQL_CORRELATION_NAME:			/*  74 */
		ptr.smallint = SQL_CN_ANY;
		length = sizeof(ptr.smallint);
		break;
	case SQL_CREATE_ASSERTION:			/* 127 */
	case SQL_CREATE_CHARACTER_SET:		/* 128 */
	case SQL_CREATE_COLLATION:			/* 129 */
	case SQL_CREATE_DOMAIN:				/* 130 */
	case SQL_CREATE_SCHEMA:				/* 131 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_CREATE_TABLE:				/* 132 */
		ptr.uinteger = 0;
		if (cnct->server_majorver >= 4) {
			ptr.uinteger |= SQL_CT_CREATE_TABLE;
		}
		length = sizeof(ptr.uinteger);
		break;
	case SQL_CREATE_TRANSLATION:		/* 133 */
	case SQL_CREATE_VIEW:				/* 134 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_CURSOR_COMMIT_BEHAVIOR:	/*  23 */
	case SQL_CURSOR_ROLLBACK_BEHAVIOR:	/*  24 */
		ptr.smallint = SQL_CB_PRESERVE;
		length = sizeof(ptr.smallint);
		break;
	case SQL_CURSOR_SENSITIVITY:		/*10001*/
		ptr.uinteger = SQL_UNSPECIFIED;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_DATA_SOURCE_NAME:			/*   2 */
		if (cnct->data_source != NULL) sqlchar = cnct->data_source;
		else sqlchar = "";
		chardata = TRUE;
		break;
	case SQL_DATA_SOURCE_READ_ONLY:		/*  25 */
		if (cnct->sql_attr_access_mode == SQL_MODE_READ_ONLY) sqlchar = "Y";
		else sqlchar = "N";
		chardata = TRUE;
		break;
	case SQL_DATABASE_NAME:				/*  16 */
		if (cnct->sql_attr_current_catalog != NULL) sqlchar = (char *)cnct->sql_attr_current_catalog;
		else sqlchar = "";
		chardata = TRUE;
		break;
	case SQL_DATETIME_LITERALS:			/* 119 */
		ptr.uinteger = 0;
		if (cnct->server_majorver >= 3) {
			ptr.uinteger |= SQL_DL_SQL92_DATE;
			ptr.uinteger |= SQL_DL_SQL92_TIME;
		}
		length = sizeof(ptr.uinteger);
		break;
	case SQL_DBMS_NAME:					/*  17 */
		sqlchar = "DB/C FS";
		chardata = TRUE;
		break;
	case SQL_DBMS_VER:					/*  18 */
		sprintf(temp, "%02d.%02d.%04d", cnct->server_majorver, cnct->server_minorver,
				cnct->server_subrelease);
		sqlchar = temp;
		chardata = TRUE;
		break;
	case SQL_DDL_INDEX:					/* 170 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_DEFAULT_TXN_ISOLATION:		/*  26 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_DESCRIBE_PARAMETER:		/*10002*/
		sqlchar = "N";
		chardata = TRUE;
		break;
	/* case SQL_DM_VER:			 	handled by driver manager,   171 */
	/* case SQL_DRIVER_HDBC:	 	handled by driver manager,	   3 */
	/* case SQL_DRIVER_HDESC:		handled by driver manager,   135 */
	/* case SQL_DRIVER_HENV:		handled by driver manager,     4 */
	/* case SQL_DRIVER_HLIB:		handled by driver manager,    76 */
	/* case SQL_DRIVER_HSTMT:		handled by driver manager,     5 */
	case SQL_DRIVER_NAME:				/*   6 */
		sqlchar = DRIVER_NAME;
		chardata = TRUE;
		break;
	case SQL_DRIVER_ODBC_VER:			/*  77 */
		sqlchar = SQL_SPEC_STRING;
		chardata = TRUE;
		break;
	case SQL_DRIVER_VER:  				/*   7 */
		sqlchar = DRIVER_VERSION;
		chardata = TRUE;
		break;
	case SQL_DROP_ASSERTION:			/* 136 */
	case SQL_DROP_CHARACTER_SET:		/* 137 */
	case SQL_DROP_COLLATION:			/* 138 */
	case SQL_DROP_DOMAIN:				/* 139 */
	case SQL_DROP_SCHEMA:				/* 140 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_DROP_TABLE:				/* 141 */
		ptr.uinteger = 0;
		if (cnct->server_majorver >= 4) {
			ptr.uinteger |= SQL_DT_DROP_TABLE;
		}
		length = sizeof(ptr.uinteger);
		break;
	case SQL_DROP_TRANSLATION:			/* 142 */
	case SQL_DROP_VIEW:					/* 143 */

	/**
	 * We do not support dynamic cursors, only static.
	 */
	case SQL_DYNAMIC_CURSOR_ATTRIBUTES1:/* 144 */
	case SQL_DYNAMIC_CURSOR_ATTRIBUTES2:/* 145 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;

	case SQL_EXPRESSIONS_IN_ORDERBY:	/*  27 */
		sqlchar = "N";
		chardata = TRUE;
		break;

	/**
	 * This was defined in ODBC 1.0.
	 * It was deprecated in ODBC 3.0.
	 * But a 2.0 application will use it and we are required to still answer it.
	 * (A 3.0 app may use it but is not supposed to.)
	 */
	case SQL_FETCH_DIRECTION:			/*   8 */
		ptr.uinteger = SQL_FD_FETCH_NEXT;
		if (cnct->server_majorver >= 3) {
			ptr.uinteger |= SQL_FD_FETCH_FIRST;
			ptr.uinteger |= SQL_FD_FETCH_LAST;
			ptr.uinteger |= SQL_FD_FETCH_PRIOR;
			ptr.uinteger |= SQL_FD_FETCH_ABSOLUTE;
			ptr.uinteger |= SQL_FD_FETCH_RELATIVE;
		}
		length = sizeof(ptr.uinteger);
		break;

	case SQL_FILE_USAGE:				/*  84 */
		ptr.smallint = SQL_FILE_NOT_SUPPORTED;
		length = sizeof(ptr.smallint);
		break;
	case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1:/* 146 */
	case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2:/* 147 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_GETDATA_EXTENSIONS:		/*  81 */
		ptr.uinteger = SQL_GD_BLOCK;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_GROUP_BY:					/*  88 */
		ptr.smallint = SQL_GB_GROUP_BY_CONTAINS_SELECT;
		length = sizeof(ptr.smallint);
		break;
	case SQL_IDENTIFIER_CASE:			/*  28 */
		ptr.smallint = SQL_IC_UPPER;
		length = sizeof(ptr.smallint);
		break;
	case SQL_IDENTIFIER_QUOTE_CHAR:		/*  29 */
		sqlchar = " ";
		chardata = TRUE;
		break;
	case SQL_INDEX_KEYWORDS:			/* 148 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_INFO_SCHEMA_VIEWS:			/* 149 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_INSERT_STATEMENT:			/* 172 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_INTEGRITY:					/*  73 Formerly SQL_ODBC_SQL_OPT_IEF*/
		sqlchar = "N";
		chardata = TRUE;
		break;
	case SQL_KEYSET_CURSOR_ATTRIBUTES1:	/* 150 */
	case SQL_KEYSET_CURSOR_ATTRIBUTES2:	/* 151 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_KEYWORDS:					/*  89 */
		sqlchar = "";
		chardata = TRUE;
		break;
	case SQL_LIKE_ESCAPE_CLAUSE:		/* 113 */
		sqlchar = "Y";
		chardata = TRUE;
		break;
	case SQL_LOCK_TYPES:				/*  78 */
		ptr.uinteger = SQL_LCK_NO_CHANGE;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_MAX_ASYNC_CONCURRENT_STATEMENTS:	/*10022*/
		if (cnct->server_majorver < 3) ptr.uinteger = 20;
		else ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_MAX_BINARY_LITERAL_LEN:	/* 112 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_MAX_CATALOG_NAME_LEN:		/*  34 Formerly SQL_MAX_QUALIFIER_NAME_LEN*/
		ptr.smallint = 0;
		length = sizeof(ptr.smallint);
		break;
	case SQL_MAX_CHAR_LITERAL_LEN:		/* 108 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_MAX_COLUMN_NAME_LEN:		/*  30 */
		ptr.smallint = MAX_NAME_LENGTH;
		length = sizeof(ptr.smallint);
		break;
	case SQL_MAX_COLUMNS_IN_GROUP_BY:	/*  97 */
		ptr.smallint = 0;
		length = sizeof(ptr.smallint);
		break;
	case SQL_MAX_COLUMNS_IN_INDEX:		/*  98 */
		ptr.smallint = 100;
		length = sizeof(ptr.smallint);
		break;
	case SQL_MAX_COLUMNS_IN_ORDER_BY:	/*  99 */
		ptr.smallint = 100;
		length = sizeof(ptr.smallint);
		break;
	case SQL_MAX_COLUMNS_IN_SELECT:		/* 100 */
		ptr.smallint = 0;
		length = sizeof(ptr.smallint);
		break;
	case SQL_MAX_COLUMNS_IN_TABLE:		/* 101 */
		ptr.smallint = 0;
		length = sizeof(ptr.smallint);
		break;
	case SQL_MAX_CONCURRENT_ACTIVITIES:	/*   1 formerly SQL_ACTIVE_STATEMENTS*/
		if (cnct->server_majorver < 3) ptr.smallint = 20;
		else ptr.smallint = 0;
		length = sizeof(ptr.smallint);
		break;
	case SQL_MAX_CURSOR_NAME_LEN:		/*  31 */
		ptr.smallint = MAX_NAME_LENGTH;
		length = sizeof(ptr.smallint);
		break;
	case SQL_MAX_DRIVER_CONNECTIONS:	/*   0 formerly SQL_ACTIVE_CONNECTIONS*/
		if (cnct->server_majorver < 3) ptr.smallint = 100;
		else ptr.smallint = 0;
		length = sizeof(ptr.smallint);
		break;
	case SQL_MAX_IDENTIFIER_LEN:		/*10005*/
		ptr.smallint = MAX_NAME_LENGTH;
		length = sizeof(ptr.smallint);
		break;
	case SQL_MAX_INDEX_SIZE:			/* 102 */
		ptr.uinteger = 359;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_MAX_PROCEDURE_NAME_LEN:	/*  33 */
		ptr.smallint = 0;
		length = sizeof(ptr.smallint);
		break;
	case SQL_MAX_ROW_SIZE:				/* 104 */
		ptr.uinteger = 32760;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_MAX_ROW_SIZE_INCLUDES_LONG:/* 103 */
		sqlchar = "Y";
		chardata = TRUE;
		break;
	case SQL_MAX_SCHEMA_NAME_LEN:		/*  32 Formerly SQL_MAX_OWNER_NAME_LEN */
		ptr.smallint = 0;
		length = sizeof(ptr.smallint);
		break;
	case SQL_MAX_STATEMENT_LEN:			/* 105 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_MAX_TABLE_NAME_LEN:		/*  35 */
		ptr.smallint = MAX_NAME_LENGTH;
		length = sizeof(ptr.smallint);
		break;
	case SQL_MAX_TABLES_IN_SELECT:		/* 106 */
		ptr.smallint = MAX_TABLES_IN_SELECT;
		length = sizeof(ptr.smallint);
		break;
	case SQL_MAX_USER_NAME_LEN:			/* 107 */
		ptr.smallint = MAX_NAME_LENGTH;
		length = sizeof(ptr.smallint);
		break;
	case SQL_MULT_RESULT_SETS:			/*  36 */
		sqlchar = "N";
		chardata = TRUE;
		break;
	case SQL_MULTIPLE_ACTIVE_TXN:		/*  37 */
		sqlchar = "N";
		chardata = TRUE;
		break;
	case SQL_NEED_LONG_DATA_LEN:		/* 111 */
		sqlchar = "N";
		chardata = TRUE;
		break;
	case SQL_NON_NULLABLE_COLUMNS:		/*  75 */
		ptr.smallint = SQL_NNC_NON_NULL;
		length = sizeof(ptr.smallint);
		break;
	case SQL_NULL_COLLATION:			/*  85 */
		ptr.smallint = SQL_NC_LOW;
		length = sizeof(ptr.smallint);
		break;
	case SQL_NUMERIC_FUNCTIONS:			/*  49 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_ODBC_API_CONFORMANCE:		/*   9 */
		/* REQUIRED BY 2.x APPLICATIONS */
		ptr.smallint = SQL_OAC_LEVEL1;
		length = sizeof(ptr.smallint);
		break;
	case SQL_ODBC_INTERFACE_CONFORMANCE:  /* 152 */
		ptr.uinteger = SQL_OIC_CORE;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_ODBC_SAG_CLI_CONFORMANCE:  /*  12 */
	/*** CODE: NOT IN 3.0 MANUAL, BUT IN 2.0 MANUAL ***/
		ptr.smallint = SQL_OSCC_NOT_COMPLIANT;
		length = sizeof(ptr.smallint);
		break;
	case SQL_ODBC_SQL_CONFORMANCE:		/*  15 */
		/* REQUIRED BY 2.x APPLICATIONS */
		ptr.smallint = SQL_OSC_MINIMUM;
		length = sizeof(ptr.smallint);
		break;
	/* case SQL_ODBC_VER:			handled by driver manager,	  10 */
	case SQL_OJ_CAPABILITIES:			/* 115 */
		ptr.uinteger = 0;
		ptr.uinteger |= SQL_OJ_LEFT;
		ptr.uinteger |= SQL_OJ_NOT_ORDERED;
		ptr.uinteger |= SQL_OJ_INNER;
		ptr.uinteger |= SQL_OJ_ALL_COMPARISON_OPS;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_ORDER_BY_COLUMNS_IN_SELECT:
		if (cnct->server_majorver == 2) sqlchar = "Y";
		else sqlchar = "N";
		chardata = TRUE;
		break;
	case SQL_OUTER_JOINS:				/*  38 */
	/*** CODE: In v2 manual, not in v3?***/
		sqlchar = "Y";
		chardata = TRUE;
		break;
	case SQL_PARAM_ARRAY_ROW_COUNTS:	/* 153 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_PARAM_ARRAY_SELECTS:		/* 154 */
		ptr.uinteger = SQL_PAS_NO_SELECT;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_POS_OPERATIONS:			/*  79 Deprecated! */
		ptr.uinteger = SQL_POS_POSITION;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_POSITIONED_STATEMENTS:		/*  80 Deprecated! */
		ptr.uinteger = 0;
		if (cnct->server_majorver >= 3) {
			ptr.uinteger |= SQL_PS_POSITIONED_DELETE;
			ptr.uinteger |= SQL_PS_POSITIONED_UPDATE;
			ptr.uinteger |= SQL_PS_SELECT_FOR_UPDATE;
		}
		length = sizeof(ptr.uinteger);
		break;
	case SQL_PROCEDURE_TERM:			/*  40 */
		sqlchar = "procedure";
		chardata = TRUE;
		break;
	case SQL_PROCEDURES:				/*  21 */
		sqlchar = "N";
		chardata = TRUE;
		break;
	case SQL_QUOTED_IDENTIFIER_CASE:	/*  93 */
		ptr.uinteger = SQL_IC_SENSITIVE;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_ROW_UPDATES:				/*  11 */
		sqlchar = "N";
		chardata = TRUE;
		break;
	case SQL_SCHEMA_TERM:				/*  39 Formerly SQL_OWNER_TERM */
		sqlchar = "";
		chardata = TRUE;
		break;
	case SQL_SCHEMA_USAGE:				/*  91 Formerly SQL_OWNER_USAGE */
		ptr.uinteger = 0;				/* Schemas are NOT supported  at all, anywhere */
		length = sizeof(ptr.uinteger);
		break;
	case SQL_SCROLL_CONCURRENCY:		/*  43 Deprecated! */
		ptr.uinteger = SQL_SCCO_READ_ONLY | SQL_SCCO_OPT_VALUES;
		length = sizeof(ptr.uinteger);
		break;

	/**
	 * Version 2 of FS supported only forward cursors.
	 * All versions of FS support only static cursors.
	 */
	case SQL_SCROLL_OPTIONS:			/*  44 */
		ptr.uinteger = SQL_SO_STATIC;
		if (cnct->server_majorver == 2) ptr.uinteger |= SQL_SO_FORWARD_ONLY;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_SEARCH_PATTERN_ESCAPE:		/*  14 */
		sqlchar = "\\";
		chardata = TRUE;
		break;
	case SQL_SERVER_NAME:				/*  13 */
		sqlchar = "DBCFS";
		chardata = TRUE;
		break;
	case SQL_SPECIAL_CHARACTERS:		/*  94 */
		sqlchar = "";
		chardata = TRUE;
		break;
	case SQL_SQL_CONFORMANCE:			/* 118 */
		ptr.uinteger = SQL_SC_SQL92_ENTRY;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_SQL92_DATETIME_FUNCTIONS:	/* 155 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_SQL92_FOREIGN_KEY_DELETE_RULE:/* 156 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_SQL92_FOREIGN_KEY_UPDATE_RULE:/* 157 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_SQL92_GRANT:				/* 158 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_SQL92_NUMERIC_VALUE_FUNCTIONS:/* 159 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_SQL92_PREDICATES:			/* 160 */
		ptr.uinteger = 0;
		if (cnct->server_majorver >= 3) {
			ptr.uinteger |= SQL_SP_BETWEEN;
			ptr.uinteger |= SQL_SP_IN;
			ptr.uinteger |= SQL_SP_LIKE;
		}
		length = sizeof(ptr.uinteger);
		break;
	case SQL_SQL92_RELATIONAL_JOIN_OPERATORS:/* 161 */
		ptr.uinteger = SQL_SRJO_INNER_JOIN | SQL_SRJO_LEFT_OUTER_JOIN;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_SQL92_REVOKE:				/* 162 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_SQL92_ROW_VALUE_CONSTRUCTOR:/* 163 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_SQL92_STRING_FUNCTIONS:	/* 164 */
		ptr.uinteger = 0;
		if (cnct->server_majorver >= 4) {
			ptr.uinteger |= SQL_SSF_LOWER;
			ptr.uinteger |= SQL_SSF_UPPER;
			ptr.uinteger |= SQL_SSF_SUBSTRING;
			ptr.uinteger |= SQL_SSF_TRIM_BOTH;
			ptr.uinteger |= SQL_SSF_TRIM_LEADING;
			ptr.uinteger |= SQL_SSF_TRIM_TRAILING;
		}
		length = sizeof(ptr.uinteger);
		break;
	case SQL_SQL92_VALUE_EXPRESSIONS:	/* 165 */
		ptr.uinteger = 0;
		if (cnct->server_majorver >= 4) {
			ptr.uinteger |= SQL_SVE_CAST;
		}
		length = sizeof(ptr.uinteger);
		break;
	case SQL_STANDARD_CLI_CONFORMANCE:	/* 166 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;

	/**
	 * SQL_CA1_NEXT = A FetchOrientation argument of SQL_FETCH_NEXT is supported in a call
	 * to SQLFetchScroll.
	 *
	 * SQL_CA1_ABSOLUTE = FetchOrientation arguments of SQL_FETCH_FIRST, SQL_FETCH_LAST,
	 * and SQL_FETCH_ABSOLUTE are supported in a call to SQLFetchScroll.
	 * (The rowset that will be fetched is independent of the current cursor position.)
	 *
	 * SQL_CA1_RELATIVE = FetchOrientation arguments of SQL_FETCH_PRIOR and SQL_FETCH_RELATIVE
	 * are supported in a call to SQLFetchScroll.
	 * (The rowset that will be fetched depends on the current cursor position.
	 * Note that this is separated from SQL_FETCH_NEXT because in a forward-only cursor, only SQL_FETCH_NEXT is supported.)
	 *
	 * SQL_CA1_POSITIONED_UPDATE = An UPDATE WHERE CURRENT OF SQL statement is supported.
	 * (An SQL-92 Entry level–conformant driver will always return this option as supported.)
	 *
	 * SQL_CA1_POSITIONED_DELETE = A DELETE WHERE CURRENT OF SQL statement is supported.
	 * (An SQL-92 Entry level–conformant driver will always return this option as supported.)
	 */
	case SQL_STATIC_CURSOR_ATTRIBUTES1:	/* 167 */
		ptr.uinteger = SQL_CA1_NEXT;
		if (cnct->server_majorver >= 3) {
			ptr.uinteger |= SQL_CA1_ABSOLUTE;
			ptr.uinteger |= SQL_CA1_RELATIVE;
			ptr.uinteger |= SQL_CA1_POSITIONED_UPDATE;
			ptr.uinteger |= SQL_CA1_POSITIONED_DELETE;
		}
		length = sizeof(ptr.uinteger);
		break;

	case SQL_STATIC_CURSOR_ATTRIBUTES2:	/* 168 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;

	/**
	 * This was added in ODBC 2.0 and deprecated in 3.0
	 *
	 * SQL_SS_UPDATES = Updates to rows are visible to the cursor;
	 * if the cursor scrolls from and returns to an updated row, the data returned by the cursor
	 * is the updated data, not the original data. This option applies only to static cursors
	 * or updates on keyset-driven cursors that do not update the key.
	 * This option does not apply for a dynamic cursor or in the case in which a key is changed in a mixed cursor.
	 */
	case SQL_STATIC_SENSITIVITY:		/*  83 Deprecated! */
		ptr.uinteger = SQL_SS_UPDATES;
		length = sizeof(ptr.uinteger);
		break;

	case SQL_STRING_FUNCTIONS:			/*  50 */
		ptr.uinteger = 0;
		if (cnct->server_majorver >= 4) {
			ptr.uinteger |= SQL_FN_STR_SUBSTRING;
		}
		length = sizeof(ptr.uinteger);
		break;
	case SQL_SUBQUERIES:				/*  95 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_SYSTEM_FUNCTIONS:			/*  51 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_TABLE_TERM:				/*  45 */
		sqlchar = "file";
		chardata = TRUE;
		break;
	case SQL_TIMEDATE_ADD_INTERVALS:	/* 109 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_TIMEDATE_DIFF_INTERVALS:	/* 110 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_TIMEDATE_FUNCTIONS:		/*  52 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_TXN_CAPABLE:				/*  46 */
		ptr.smallint = SQL_TC_NONE;
		length = sizeof(ptr.smallint);
		break;
	case SQL_TXN_ISOLATION_OPTION:		/*  72 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_UNION:						/*  96 */
		ptr.uinteger = 0;
		length = sizeof(ptr.uinteger);
		break;
	case SQL_USER_NAME:					/*  47 */
		if (cnct->user != NULL) sqlchar = cnct->user;
		else sqlchar = "";
		chardata = TRUE;
		break;
	case SQL_XOPEN_CLI_YEAR:			/*10000*/
		sqlchar = "1995";
		chardata = TRUE;
		break;
	default:
		sprintf(errmsg, "%s: InfoType = %d", ERROR_HY096, (int) InfoType);
		error_record(&cnct->errors, "HY096", errmsg, NULL, 0);
		return SQL_ERROR;
	}

	if (chardata) length = (SQLSMALLINT)strlen(sqlchar);

	if (StringLengthPtr != NULL) *StringLengthPtr = length;
	if (InfoValuePtr != NULL) {
		if (chardata) {
			if (length < BufferLength) {
				BufferLength = length;
				((SQLCHAR *) InfoValuePtr)[BufferLength] = '\0';
			}
			else {
				if (BufferLength > 0) ((SQLCHAR *) InfoValuePtr)[--BufferLength] = '\0';
				error_record(&(cnct->errors), "01004", TEXT_01004, NULL, 0);
				rc = SQL_SUCCESS_WITH_INFO;
			}
			if (BufferLength > 0) memcpy(InfoValuePtr, sqlchar, BufferLength);
		}
		else memcpy(InfoValuePtr, &ptr, length);
	}
	return rc;
}

/**
 * Is thread safe. Does not alloc/free memory. Does not call any other function
 */
SQLRETURN SQL_API SQLGetStmtAttr(
	SQLHSTMT StatementHandle,
	SQLINTEGER Attribute,
	SQLPOINTER ValuePtr,
	SQLINTEGER BufferLength,
	SQLINTEGER *StringLengthPtr)
{
	SQLINTEGER length;
	SQLRETURN rc;
	STATEMENT *stmt;
	union {
		SQLUINTEGER sqluinteger;
		SQLUINTEGER *sqluintegerptr;
		SQLUSMALLINT *sqlusmallintptr;
		SQLPOINTER sqlpointer;
		SQLHDESC sqlhdesc;
	} ptr;

	stmt = (STATEMENT *) StatementHandle;

	rc = SQL_SUCCESS;
	length = sizeof(ptr.sqluinteger);
	switch (Attribute) {
	case SQL_ATTR_APP_PARAM_DESC:
		ptr.sqlhdesc = stmt->sql_attr_app_param_desc;
		length = sizeof(ptr.sqlhdesc);
		break;
	case SQL_ATTR_APP_ROW_DESC:
		ptr.sqlhdesc = stmt->sql_attr_app_row_desc;
		length = sizeof(ptr.sqlhdesc);
		break;
	case SQL_ATTR_ASYNC_ENABLE:
		ptr.sqluinteger = stmt->sql_attr_async_enable;
		break;
	case SQL_ATTR_CONCURRENCY:
		ptr.sqluinteger = stmt->sql_attr_concurrency;
		break;
	case SQL_ATTR_CURSOR_SCROLLABLE:
		ptr.sqluinteger = stmt->sql_attr_cursor_scrollable;
		break;
	case SQL_ATTR_CURSOR_SENSITIVITY:
		ptr.sqluinteger = stmt->sql_attr_cursor_sensitivity;
		break;
	case SQL_ATTR_CURSOR_TYPE:
		ptr.sqluinteger = stmt->sql_attr_cursor_type;
		break;
	case SQL_ATTR_ENABLE_AUTO_IPD:
		ptr.sqluinteger = stmt->sql_attr_enable_auto_ipd;
		break;
	case SQL_ATTR_FETCH_BOOKMARK_PTR:
		ptr.sqlpointer = stmt->sql_attr_fetch_bookmark_ptr;
		length = sizeof(ptr.sqlpointer);
		break;
	case SQL_ATTR_IMP_PARAM_DESC:
		ptr.sqlhdesc = stmt->sql_attr_imp_param_desc;
		length = sizeof(ptr.sqlhdesc);
		break;
	case SQL_ATTR_IMP_ROW_DESC:
		ptr.sqlhdesc = stmt->sql_attr_imp_row_desc;
		length = sizeof(ptr.sqlhdesc);
		break;
	case SQL_ATTR_KEYSET_SIZE:
		ptr.sqluinteger = stmt->sql_attr_keyset_size;
		break;
	case SQL_ATTR_MAX_LENGTH:
		ptr.sqluinteger = stmt->sql_attr_max_length;
		break;
	case SQL_ATTR_MAX_ROWS:
		ptr.sqluinteger = stmt->sql_attr_max_rows;
		break;
	case SQL_ATTR_METADATA_ID:
		ptr.sqluinteger = stmt->sql_attr_metadata_id;
		break;
	case SQL_ATTR_NOSCAN:
		ptr.sqluinteger = stmt->sql_attr_noscan;
		break;
	case SQL_ATTR_PARAM_BIND_OFFSET_PTR:
		ptr.sqluintegerptr = stmt->sql_attr_app_param_desc->sql_desc_bind_offset_ptr;
		length = sizeof(ptr.sqluintegerptr);
		break;
	case SQL_ATTR_PARAM_BIND_TYPE:
		ptr.sqluinteger = stmt->sql_attr_app_param_desc->sql_desc_bind_type;
		break;
	case SQL_ATTR_PARAM_OPERATION_PTR:
		ptr.sqlusmallintptr = stmt->sql_attr_app_param_desc->sql_desc_array_status_ptr;
		length = sizeof(ptr.sqlusmallintptr);
		break;
	case SQL_ATTR_PARAM_STATUS_PTR:
		ptr.sqlusmallintptr = stmt->sql_attr_imp_param_desc->sql_desc_array_status_ptr;
		length = sizeof(ptr.sqlusmallintptr);
		break;
	case SQL_ATTR_PARAMS_PROCESSED_PTR:
		ptr.sqluintegerptr = stmt->sql_attr_imp_param_desc->sql_desc_rows_processed_ptr;
		length = sizeof(ptr.sqluintegerptr);
		break;
	case SQL_ATTR_PARAMSET_SIZE:
		ptr.sqluinteger = stmt->sql_attr_app_param_desc->sql_desc_array_size;
		break;
	case SQL_ATTR_QUERY_TIMEOUT:
		ptr.sqluinteger = stmt->sql_attr_query_timeout;
		break;
	case SQL_ATTR_RETRIEVE_DATA:
		ptr.sqluinteger = stmt->sql_attr_retrieve_data;
		break;
	case SQL_ATTR_ROW_ARRAY_SIZE:
	case SQL_ROWSET_SIZE:  /* ODBC 2.0 compatability */
		ptr.sqluinteger = stmt->sql_attr_app_row_desc->sql_desc_array_size;
		break;
	case SQL_ATTR_ROW_BIND_OFFSET_PTR:
		ptr.sqluintegerptr = stmt->sql_attr_app_row_desc->sql_desc_bind_offset_ptr;
		length = sizeof(ptr.sqluintegerptr);
		break;
	case SQL_ATTR_ROW_BIND_TYPE:
		ptr.sqluinteger = stmt->sql_attr_app_row_desc->sql_desc_bind_type;
		break;
	case SQL_ATTR_ROW_NUMBER:
		if (stmt->executeflags & EXECUTE_FETCH) ptr.sqluinteger = stmt->sql_attr_row_number;
		else ptr.sqluinteger = 0;
		break;
	case SQL_ATTR_ROW_OPERATION_PTR:
		ptr.sqlusmallintptr = stmt->sql_attr_app_row_desc->sql_desc_array_status_ptr;
		length = sizeof(ptr.sqlusmallintptr);
		break;
	case SQL_ATTR_ROW_STATUS_PTR:
		ptr.sqlusmallintptr = stmt->sql_attr_imp_row_desc->sql_desc_array_status_ptr;
		length = sizeof(ptr.sqlusmallintptr);
		break;
	case SQL_ATTR_ROWS_FETCHED_PTR:
		ptr.sqluintegerptr = stmt->sql_attr_imp_row_desc->sql_desc_rows_processed_ptr;
		length = sizeof(ptr.sqluintegerptr);
		break;
	case SQL_ATTR_SIMULATE_CURSOR:
		ptr.sqluinteger = stmt->sql_attr_simulate_cursor;
		break;
	case SQL_ATTR_USE_BOOKMARKS:
		ptr.sqluinteger = stmt->sql_attr_use_bookmarks;
		break;
	default:
		error_init(&stmt->errors);
		error_record(&stmt->errors, "HYC00", TEXT_HYC00, NULL, 0);
		return SQL_ERROR;
	}
	if (ValuePtr != NULL) memcpy(ValuePtr, &ptr, length);
	if (StringLengthPtr != NULL) *StringLengthPtr = length;
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetTypeInfo(
	SQLHSTMT StatementHandle,
	SQLSMALLINT DataType)
{
	INT i1, i2, numtypes, version, types[32];
	SQLRETURN rc;
	CHAR result[512], work[32];
	UCHAR *bufptr;
	STATEMENT *stmt;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	entersync();
	version = stmt->cnct->env->sql_attr_odbc_version;
	switch (DataType) {
	case SQL_ALL_TYPES:
		types[0] = SQL_CHAR;
		types[1] = SQL_NUMERIC;
		if (stmt->cnct->server_majorver >= 3) {
			if (version == SQL_OV_ODBC2) types[2] = SQL_DATE;
			else types[2] = SQL_TYPE_DATE;
			if (version == SQL_OV_ODBC2) types[3] = SQL_TIME;
			else types[3] = SQL_TYPE_TIME;
			if (version == SQL_OV_ODBC2) types[4] = SQL_TIMESTAMP;
			else types[4] = SQL_TYPE_TIMESTAMP;
			numtypes = 5;
		}
		else numtypes = 0;
		break;
	case SQL_CHAR:
		types[0] = SQL_CHAR;
		numtypes = 1;
		break;
	case SQL_DECIMAL:
	case SQL_NUMERIC:
		types[0] = SQL_NUMERIC;
		numtypes = 1;
		break;
	case SQL_TYPE_DATE:
		if (stmt->cnct->server_majorver >= 3) {
			if (version == SQL_OV_ODBC2) types[0] = SQL_DATE;
			else types[0] = SQL_TYPE_DATE;
			numtypes = 1;
		}
		else numtypes = 0;
		break;
	case SQL_TYPE_TIME:
		if (stmt->cnct->server_majorver >= 3) {
			if (version == SQL_OV_ODBC2) types[0] = SQL_TIME;
			else types[0] = SQL_TYPE_TIME;
			numtypes = 1;
		}
		else numtypes = 0;
		break;
	case SQL_TYPE_TIMESTAMP:
		if (stmt->cnct->server_majorver >= 3) {
			if (version == SQL_OV_ODBC2) types[0] = SQL_TIMESTAMP;
			else types[0] = SQL_TYPE_TIMESTAMP;
			numtypes = 1;
		}
		else numtypes = 0;
		break;
	default:
		numtypes = 0;
		break;
	}

	if (stmt->row_buffer == NULL) {
		bufptr = (UCHAR *) allocmem((numtypes + 1) * MAX_DATA_LENGTH, 0);
		if (bufptr == NULL) {
			error_record(&stmt->errors, "HY001", TEXT_HY001, NULL, 0);
			exitsync();
			return SQL_ERROR;
		}
		stmt->row_buffer = bufptr;
		stmt->row_buffer_size = (numtypes + 1) * MAX_DATA_LENGTH;
	}
	else if ((numtypes + 1) * MAX_DATA_LENGTH > stmt->row_buffer_size) {
		bufptr = (UCHAR *) reallocmem(stmt->row_buffer, (numtypes + 1) * MAX_DATA_LENGTH, 0);
		if (bufptr == NULL) {
			error_record(&stmt->errors, "HY001", TEXT_HY001, NULL, 0);
			exitsync();
			return SQL_ERROR;
		}
		stmt->row_buffer = bufptr;
		stmt->row_buffer_size = (numtypes + 1) * MAX_DATA_LENGTH;
	}
	else bufptr = stmt->row_buffer;

	for (i1 = 0; i1 < numtypes; i1++) {
		_itoa(types[i1], work, 10);
		i2 = (INT)strlen(work);
		if (i2 > MAX_DATA_LENGTH) {
			error_record(&stmt->errors, "HY000", TEXT_HY001, "type larger than expected", 0);
			exitsync();
			return SQL_ERROR;
		}
		memset(bufptr + (i1 + 1) * MAX_DATA_LENGTH, ' ', MAX_DATA_LENGTH - i2);
		memcpy(bufptr + (i1 + 2) * MAX_DATA_LENGTH - i2, work, i2);
	}
	strcpy(result, "NONE ");
	_itoa(numtypes, work, 10);
	strcat(result, work);
	strcat(result, " 1 (N");
	_itoa(MAX_DATA_LENGTH, work, 10);
	strcat(result, work);
	strcat(result, ")");

	/* check for invalid cursor state */
	if (stmt->executeflags & EXECUTE_RESULTSET) {
		error_record(&stmt->errors, "24000", TEXT_24000, NULL, 0);
		exitsync();
		return SQL_ERROR;
	}
	if (stmt->executeflags & EXECUTE_RSID) {
		stmt->executeflags &= ~EXECUTE_RSID;
		if (server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD, NULL, 0, NULL, NULL, NULL) != SERVER_OK) {
			exitsync();
			return SQL_ERROR;
		}
	}
	stmt->executeflags = 0;
	stmt->row_count = -1;

	rc = exec_resultset(stmt, &stmt->errors, result, (INT)strlen(result), EXECUTE_SQLGETTYPEINFO);
	if (rc != SQL_ERROR) stmt->executeflags |= EXECUTE_EXECUTED;
	else stmt->executeflags = 0;
	exitsync();
	return rc;
}

SQLRETURN SQL_API SQLMoreResults(
	SQLHSTMT StatementHandle)
{
	SQLRETURN rc;
	STATEMENT *stmt;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	rc = SQL_NO_DATA;
	if (stmt->executeflags & EXECUTE_RESULTSET) {
		if (stmt->executeflags & EXECUTE_RSID) {
			entersync();
			if (server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD, NULL, 0, &stmt->errors, NULL, NULL) != SERVER_OK) {
				rc = SQL_ERROR;
			}
			exitsync();
		}
	}
	stmt->executeflags &= EXECUTE_PREPARED;
	stmt->row_count = -1;
/*** CODE: SHOULD ROW_BUFFER BE FREED HERE OR WAIT TO FREEHANDLE ? ***/
/*	freemem(stmt->row_buffer); */
/*	stmt->row_buffer = NULL; */
	return rc;
}

SQLRETURN SQL_API SQLNativeSql(
	SQLHDBC ConnectionHandle,
	SQLCHAR *InStatementText,
	SQLINTEGER TextLength1,
	SQLCHAR *OutStatementText,
	SQLINTEGER BufferLength,
	SQLINTEGER *TextLength2Ptr)
{
	INT rc;

/*** CODE GOES HERE, DO THIS FOR NOW SO IT DOESN'T FAIL ***/
/*** ALSO EXTEND THE CODE FOR SUPPORT FOR SQLExecute, SQLExecDirect, SQLPrepare ***/
	rc = SQL_SUCCESS;
	if (TextLength2Ptr != NULL) *TextLength2Ptr = TextLength1;
	if (OutStatementText != NULL) {
		memcpy(OutStatementText, InStatementText, min(TextLength1, BufferLength));
		if (TextLength1 >= BufferLength) {
			if (BufferLength > 0) OutStatementText[BufferLength - 1] = '\0';
			error_record(&(((CONNECTION *) ConnectionHandle)->errors), "01004", TEXT_01004, NULL, 0);
			rc = SQL_SUCCESS_WITH_INFO;
		}
		else OutStatementText[TextLength1] = '\0';
	}
	return rc;
}

/*
 * Returns the number of parameters in an SQL statement.
 * Does not need to check for HY010 error condition, the DM does that
 */
SQLRETURN SQL_API SQLNumParams(
	SQLHSTMT StatementHandle,
	SQLSMALLINT *ParameterCountPtr)
{

	STATEMENT *stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	if (ParameterCountPtr != NULL) {
		*ParameterCountPtr = stmt->parameter_marker_count;
	}
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLNumResultCols(
	SQLHSTMT StatementHandle,
	SQLSMALLINT *ColumnCountPtr)
{
	STATEMENT *stmt;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	if (!(stmt->executeflags & EXECUTE_RESULTSET)) {
		if (stmt->executeflags & EXECUTE_EXECUTED) {
			if (ColumnCountPtr != NULL) *ColumnCountPtr = 0;
			return SQL_SUCCESS;
		}
		if (!(stmt->executeflags & EXECUTE_PREPARED)) {
			error_record(&stmt->errors, "HY010", TEXT_HY010, "Statement has not been executed or prepared", 0);
			return SQL_ERROR;
		}
		if (exec_columninfo(stmt, &stmt->errors) == SQL_ERROR) return SQL_ERROR;
	}
	if (ColumnCountPtr != NULL) *ColumnCountPtr = stmt->sql_attr_imp_row_desc->sql_desc_count;
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLParamData(
	SQLHSTMT StatementHandle,
	SQLPOINTER *ValuePtrPtr)
{
	SQLRETURN rc;
	STATEMENT *stmt;
	APP_DESC_RECORD *apd_rec;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	if (!(stmt->executeflags & EXECUTE_NEEDDATA)) {
		error_record(&stmt->errors, "HY010", TEXT_HY010, "Statement does not need data", 0);
		return SQL_ERROR;
	}

	rc = exec_execute(stmt);
	if (rc == SQL_NEED_DATA) {
		entersync();
		apd_rec = (APP_DESC_RECORD *) desc_get(stmt->sql_attr_app_param_desc, stmt->sql_attr_app_param_desc->dataatexecrecnum, FALSE);
/*** CODE: IF CALLED FOR SQLBulkOperation OR SQLSetPos, THEN THIS VALUE NEEDS TO BE CALCULATED FOR BIND_OFFSET ***/
		*ValuePtrPtr = apd_rec->sql_desc_data_ptr;
		exitsync();
	}
	return rc;
}

SQLRETURN SQL_API SQLPrepare(
	SQLHSTMT StatementHandle,
	SQLCHAR *StatementText,
	SQLINTEGER TextLength)
{
	SQLRETURN rc;
	STATEMENT *stmt;
	INT i1;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	entersync();
	if (stmt->executeflags & EXECUTE_RSID) {
		stmt->executeflags &= ~EXECUTE_RSID;
		if (server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD, NULL, 0, NULL, NULL, NULL) != SERVER_OK) {
			exitsync();
			return SQL_ERROR;
		}
	}
	stmt->executeflags = 0;
	stmt->row_count = -1;
	stmt->sql_attr_imp_param_desc->sql_desc_count = 0;
	stmt->parameter_marker_count = 0;

	if (TextLength == SQL_NTS) TextLength = (SQLINTEGER)strlen((const char *)StatementText);
	rc = exec_prepare(stmt, StatementText, TextLength);
	if (rc != SQL_ERROR) stmt->executeflags = EXECUTE_PREPARED;

	/**
	 * In accordance with the ODBC spec, we need to count the question marks here.
	 * A call to SQLNumParams should succeed if done immediately after a call to this guy.
	 * Independant of any calls to SQLBindParameter.
	 */
	if (rc != SQL_ERROR) {
		for (i1 = 0; i1 < stmt->textlength; i1++) {
			if (stmt->text[i1] == '?') stmt->parameter_marker_count++;
		}
	}
	exitsync();
	return rc;
}

SQLRETURN SQL_API SQLPrimaryKeys(
	SQLHSTMT StatementHandle,
	SQLCHAR *CatalogName,
	SQLSMALLINT NameLength1,
	SQLCHAR *SchemaName,
	SQLSMALLINT NameLength2,
	SQLCHAR *TableName,
	SQLSMALLINT NameLength3)
{
	INT i1, replylen;
	SQLRETURN rc, rc2;
	CHAR buffer[512];
	STATEMENT *stmt;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	if (TableName != NULL) {
		if (NameLength3 == SQL_NTS) NameLength3 = (SQLSMALLINT)strlen((const char *)TableName);
	}
	else NameLength3 = 0;

	if (stmt->cnct->server_majorver == 2) {
		error_record(&stmt->errors, "IM001", TEXT_IM001, NULL, 0);
		return SQL_ERROR;
/*** CODE: OR COULD NOT MAKE SERVER CALL AND DO replylen = 0; rc = SERVER_SET; ***/
	}

	memcpy(buffer, "PRIMARY ", 8);
	i1 = 8;
	i1 += tcpquotedcopy((unsigned char *)(buffer + i1), TableName, NameLength3);

	/* check for invalid cursor state */
	if (stmt->executeflags & EXECUTE_RESULTSET) {
		error_record(&stmt->errors, "24000", TEXT_24000, NULL, 0);
		return SQL_ERROR;
	}
	entersync();
	if (stmt->executeflags & EXECUTE_RSID) {
		stmt->executeflags &= ~EXECUTE_RSID;
		if (server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD, NULL, 0, NULL, NULL, NULL) != SERVER_OK) {
			exitsync();
			return SQL_ERROR;
		}
	}
	stmt->executeflags = 0;
	stmt->row_count = -1;

	replylen = sizeof(buffer);
	rc = server_communicate(stmt->cnct, (UCHAR*)EMPTY, (UCHAR*)CATALOG,
		(UCHAR*)buffer, i1, &stmt->errors, (UCHAR*)buffer, &replylen);
	/*** CODE: SHOULD ERROR BE RETURNED IF SERVER_OK (NOT SERVER_FAIL OR COMM ERROR) ***/
	if (rc != SERVER_SET && rc != SERVER_NONE) {
		exitsync();
		return SQL_ERROR;
	}

	rc2 = exec_resultset(stmt, &stmt->errors, buffer, replylen, EXECUTE_SQLPRIMARYKEYS);
	if ((rc2 == SQL_ERROR || rc == SERVER_NONE) && (stmt->executeflags & EXECUTE_RSID)) {
		stmt->executeflags &= ~EXECUTE_RSID;
		server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD, NULL, 0, NULL, NULL, NULL);
	}
	if (rc2 != SQL_ERROR) stmt->executeflags |= EXECUTE_EXECUTED;
	else stmt->executeflags = 0;
	exitsync();
	return rc2;
}

#if 0
/*** CODE: DO NOT SUPPORT INITIALLY ***/
SQLRETURN SQL_API SQLProcedureColumns(
	SQLHSTMT StatementHandle,
	SQLCHAR *CatalogName,
	SQLSMALLINT NameLength1,
	SQLCHAR *SchemaName,
	SQLSMALLINT NameLength2,
	SQLCHAR *ProcName,
	SQLSMALLINT NameLength3,
	SQLCHAR *ColumnName,
	SQLSMALLINT NameLength4)
{
	return SQL_SUCCESS;
}
#endif

#if 0
/*** CODE: DO NOT SUPPORT INITIALLY ***/
SQLRETURN SQL_API SQLProcedures(
	SQLHSTMT StatementHandle,
	SQLCHAR *CatalogName,
	SQLSMALLINT NameLength1,
	SQLCHAR *SchemaName,
	SQLSMALLINT NameLength2,
	SQLCHAR *ProcName,
	SQLSMALLINT NameLength3)
{
	return SQL_SUCCESS;
}
#endif

SQLRETURN SQL_API SQLPutData(
	SQLHSTMT StatementHandle,
	SQLPOINTER DataPtr,
	SQLLEN StrLen_or_Ind)
{
	INT i1;
	CHAR errmsg[512], *ptr;
	STATEMENT *stmt;
	APP_DESC_RECORD *apd_rec;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	if (!(stmt->executeflags & EXECUTE_NEEDDATA)) {
		error_record(&stmt->errors, "HY010", TEXT_HY010, "Statement does not need data", 0);
		return SQL_ERROR;
	}

	if (StrLen_or_Ind == SQL_DEFAULT_PARAM) {
		sprintf(errmsg, "%s, Procedures are not supported with SQLPutData", TEXT_07S01);
		error_record(&stmt->errors, "07S01", errmsg, NULL, 0);
		return SQL_ERROR;
	}
	entersync();
	apd_rec = (APP_DESC_RECORD *) desc_get(stmt->sql_attr_app_param_desc, stmt->sql_attr_app_param_desc->dataatexecrecnum, FALSE);
	if (StrLen_or_Ind == SQL_NULL_DATA) {
		if (apd_rec->dataatexeclength) {
			error_record(&stmt->errors, "HY020", TEXT_HY020, NULL, 0);
			exitsync();
			return SQL_ERROR;
		}
		exitsync();
		return SQL_SUCCESS;
	}
	if (apd_rec->dataatexectype == SQL_C_CHAR) {
		if (StrLen_or_Ind == SQL_NTS) StrLen_or_Ind = strlen((CHAR *) DataPtr) * sizeof(CHAR);
		if (apd_rec->dataatexeclength + StrLen_or_Ind > apd_rec->dataatexecsize) {
			for (i1 = apd_rec->dataatexecsize; apd_rec->dataatexeclength + StrLen_or_Ind > i1; i1 <<= 1) {}
			ptr = (CHAR *) reallocmem(apd_rec->dataatexecptr, i1, 0);
			if (ptr == NULL) {
				error_record(&stmt->errors, "HY001", TEXT_HY001, NULL, 0);
				exitsync();
				return SQL_ERROR;
			}
			apd_rec->dataatexecptr = (UCHAR*)ptr;
			apd_rec->dataatexecsize = i1;
		}
		memcpy(apd_rec->dataatexecptr + apd_rec->dataatexeclength, DataPtr, StrLen_or_Ind);
		apd_rec->dataatexeclength += (INT)StrLen_or_Ind;
	}
	else {
		if (apd_rec->dataatexeclength) {
			error_record(&stmt->errors, "HY019", TEXT_HY019, NULL, 0);
			exitsync();
			return SQL_ERROR;
		}
		memcpy(apd_rec->dataatexecptr, DataPtr, apd_rec->dataatexecsize);
		apd_rec->dataatexeclength = apd_rec->dataatexecsize;
	}
	exitsync();
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLRowCount(
	SQLHSTMT StatementHandle,
	SQLLEN *RowCountPtr)
{
	STATEMENT *stmt;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	if (RowCountPtr != NULL)
		*RowCountPtr = stmt->row_count;
	return SQL_SUCCESS;
}

/**
 * Calls alloc memory. Uses local calls to enter/exit sync
 */
SQLRETURN SQL_API SQLSetConnectAttr(
	SQLHDBC ConnectionHandle,
	SQLINTEGER Attribute,
	SQLPOINTER ValuePtr,
	SQLINTEGER StringLength)
{
	CHAR *ptr;
	struct client_connection_struct *cnct;

	cnct = (CONNECTION *) ConnectionHandle;
	error_init(&cnct->errors);

	switch (Attribute) {
	case SQL_ATTR_ACCESS_MODE:
		cnct->sql_attr_access_mode = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_ASYNC_ENABLE:
		cnct->sql_attr_async_enable = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_AUTO_IPD:  /* read-only */
		break;
	case SQL_ATTR_AUTOCOMMIT:
		cnct->sql_attr_autocommit = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_CONNECTION_DEAD:  /* read-only */
		break;
	case SQL_ATTR_CONNECTION_TIMEOUT:
		cnct->sql_attr_connection_timeout = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_CURRENT_CATALOG:
		error_record(&(cnct->errors), "3D000", TEXT_3D000, "This attribute is read-only.", 0);
		return SQL_SUCCESS_WITH_INFO;
	case SQL_ATTR_LOGIN_TIMEOUT:
		cnct->sql_attr_login_timeout = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_METADATA_ID:
		cnct->sql_attr_metadata_id = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_ODBC_CURSORS:  /* DM */
		break;
	case SQL_ATTR_PACKET_SIZE:
		cnct->sql_attr_packet_size = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_QUIET_MODE:
		cnct->sql_attr_quiet_mode = (SQLHANDLE) ValuePtr;
		break;
	case SQL_ATTR_TRACE:  /* DM */
		break;
	case SQL_ATTR_TRACEFILE:  /* DM */
		break;
	case SQL_ATTR_TRANSLATE_LIB:
		entersync();
		if (StringLength == SQL_NTS) StringLength = (SQLINTEGER)strlen((CHAR *) ValuePtr);
		if (cnct->sql_attr_translate_lib == NULL) {
			cnct->sql_attr_translate_lib = (SQLCHAR *) allocmem(sizeof(SQLCHAR) * (StringLength + 1), 0);
			exitsync();
			if (cnct->sql_attr_translate_lib == NULL) {
				error_record(&cnct->errors, "HY001", TEXT_HY001, NULL, 0);
				exitsync();
				return SQL_ERROR;
			}
		}
		else if (StringLength > (SQLINTEGER) strlen((const char *)cnct->sql_attr_translate_lib)) {
			ptr = (CHAR *) reallocmem(cnct->sql_attr_translate_lib, sizeof(SQLCHAR) * (StringLength + 1), 0);
			if (ptr == NULL) {
				error_record(&cnct->errors, "HY001", TEXT_HY001, NULL, 0);
				exitsync();
				return SQL_ERROR;
			}
			cnct->sql_attr_translate_lib = (UCHAR*)ptr;
		}
		memcpy(cnct->sql_attr_translate_lib, ValuePtr, StringLength);
		cnct->sql_attr_translate_lib[StringLength] = '\0';
		exitsync();
		break;
	case SQL_ATTR_TRANSLATE_OPTION:
		cnct->sql_attr_translate_option = (DWORD) SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_TXN_ISOLATION:
		cnct->sql_attr_txn_isolation = (DWORD) SQLPTR_TO_SQLUINT ValuePtr;
		break;
	default:  /* invalid attribute */
		error_record(&cnct->errors, "HYC00", TEXT_HYC00, NULL, 0);
		return SQL_ERROR;
	}
	return SQL_SUCCESS;
}

/**
 * Uses local enter/exit sync to protect traversal of Statement linked list
 */
SQLRETURN SQL_API SQLSetCursorName(
	SQLHSTMT StatementHandle,
	SQLCHAR *CursorName,
	SQLSMALLINT NameLength)
{
	SQLSMALLINT length;
	CHAR *ptr;
	STATEMENT *stmt, *stmt2;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	if (stmt->executeflags && stmt->executeflags != EXECUTE_PREPARED) {  /* statement is in executed state */
		error_record(&stmt->errors, "24000", TEXT_24000, NULL, 0);
		return SQL_ERROR;
	}

	if (NameLength == SQL_NTS) NameLength = (SQLSMALLINT) strlen((const char *)CursorName);
	if (NameLength == 0) {
/*** NOTE: zero length cursor name is not discussed in docs, but make ***/
/***       assumption to clear cursor name and go back to default name ***/
		stmt->cursor_name[0] = '\0';
		return SQL_SUCCESS;
	}

#pragma warning(disable : 4996)

	if (NameLength > MAX_CURSOR_NAME_LENGTH || (NameLength >= 7 && !_memicmp((const char *)CursorName, "SQL_CUR", 7))) {
		if (NameLength > MAX_CURSOR_NAME_LENGTH) ptr = "user-defined cursor name is too long";
		else ptr = "user-defined cursor name can not begin with 'SQL_CUR'";
		error_record(&stmt->errors, "34000", TEXT_34000, ptr, 0);
	}

	/* check for duplicate cursor names on connection */
	entersync();
#if OS_WIN32
	__try {
#endif
	for (stmt2 = stmt->cnct->firststmt; stmt2 != NULL; stmt2 = stmt2->nextstmt) {
		if (stmt2 == stmt) continue;
		length = (SQLSMALLINT) strlen(stmt2->cursor_name);
		if (length == NameLength && !_memicmp(stmt2->cursor_name, stmt->cursor_name, length)) break;
	}
#if OS_WIN32
	}
	__except( EXCEPTION_EXECUTE_HANDLER ) {
		int i1 = GetExceptionCode();
		exitsync();
		error_record(&stmt->errors, "HY000", TEXT_HY000, "In SQLSetCursorName", i1);
		return SQL_ERROR;
	}
#endif
	exitsync();
	if (stmt2 != NULL) {
		error_record(&stmt->errors, "3C000", TEXT_3C000, NULL, 0);
		return SQL_ERROR;
	}

	memcpy(stmt->cursor_name, CursorName, NameLength);
	stmt->cursor_name[NameLength] = '\0';
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetDescField(
	SQLHDESC DescriptorHandle,
	SQLSMALLINT RecNumber,
	SQLSMALLINT FieldIdentifier,
	SQLPOINTER ValuePtr,
	SQLINTEGER BufferLength)
{
	SQLSMALLINT dataType, oldCount, newCount, cb;
	SQLRETURN rc = SQL_SUCCESS;
	SQLINTEGER i1;
	DESCRIPTOR *desc;
	LPVOID drec;
	CHAR errmsg[64];

	desc = (DESCRIPTOR *) DescriptorHandle;
	error_init(&desc->errors);

	if (desc->type == TYPE_IMP_ROW_DESC &&
		FieldIdentifier != SQL_DESC_ARRAY_STATUS_PTR &&
		FieldIdentifier != SQL_DESC_ROWS_PROCESSED_PTR) {
		error_record(&desc->errors, "HY016", HY016TEXT, NULL, 0);
		return SQL_ERROR;
	}
	entersync();
	switch (FieldIdentifier) {
	case SQL_DESC_ARRAY_SIZE:
		if (desc->type == TYPE_APP_DESC && SQLPTR_TO_SQLUINT ValuePtr != 1) {
			error_record(&desc->errors, "01S02", C01S02TEXT, NULL, 0);
			exitsync();
			return SQL_SUCCESS_WITH_INFO;
		}
		break;
	case SQL_DESC_ARRAY_STATUS_PTR:
		desc->sql_desc_array_status_ptr = (SQLUSMALLINT *) ValuePtr;
		break;
	case SQL_DESC_BIND_OFFSET_PTR:
		if (desc->type == TYPE_APP_DESC) {
			desc->sql_desc_bind_offset_ptr = (SQLUINTEGER *) ValuePtr;
		}
		break;
	case SQL_DESC_BIND_TYPE:
		if (desc->type == TYPE_APP_DESC) {
			desc->sql_desc_bind_type = SQLPTR_TO_SQLUINT ValuePtr;
		}
		break;
	case SQL_DESC_COUNT:
		oldCount = desc->sql_desc_count;
		newCount = (SQLSMALLINT) SQLPTR_TO_SQLUINT ValuePtr;
		if (newCount > oldCount) {
			if (desc_get(desc, newCount, TRUE) == NULL) {
				error_record(&desc->errors, "HY001", TEXT_HY001, NULL, 0);
				exitsync();
				return SQL_ERROR;
			}
		}
		else if (newCount < oldCount) desc->sql_desc_count = newCount;
		break;
	case SQL_DESC_ROWS_PROCESSED_PTR:
		if (desc->type != TYPE_APP_DESC)
			desc->sql_desc_rows_processed_ptr = (SQLUINTEGER *) ValuePtr;
		break;
	default:
		if (RecNumber < 0) {
			error_record(&desc->errors, "07009", TEXT_07009, NULL, 0);
			exitsync();
			return SQL_ERROR;
		}
		if (RecNumber == 0) {
			error_record(&desc->errors, "HY000", "Record number is zero; Driver does not support bookmark records", NULL, 0);
			exitsync();
			return SQL_ERROR;
		}
		if (FieldIdentifier != SQL_DESC_DATA_PTR && FieldIdentifier != SQL_DESC_OCTET_LENGTH_PTR &&
			FieldIdentifier != SQL_DESC_INDICATOR_PTR) desc_unbind(desc, RecNumber);
		if (RecNumber > desc->sql_desc_count) {
			if (desc_get(desc, RecNumber, TRUE) == NULL) {
				error_record(&desc->errors, "HY001", TEXT_HY001, NULL, 0);
				exitsync();
				return SQL_ERROR;
			}
		}
		drec = desc_get(desc, RecNumber, FALSE);
		switch (FieldIdentifier) {
		case SQL_DESC_AUTO_UNIQUE_VALUE:
		case SQL_DESC_BASE_COLUMN_NAME:
		case SQL_DESC_BASE_TABLE_NAME:
		case SQL_DESC_CASE_SENSITIVE:
		case SQL_DESC_CATALOG_NAME:
			break;
		case SQL_DESC_CONCISE_TYPE:
			dataType = (SQLSMALLINT) SQLPTR_TO_SQLUINT ValuePtr;
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				switch (dataType) {
				case SQL_CHAR:
				case SQL_NUMERIC:
					((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_concise_type = dataType;
					((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_type = dataType;
					((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_datetime_interval_code = 0;
					break;
				case SQL_TYPE_DATE:
				case SQL_TYPE_TIME:
				case SQL_TYPE_TIMESTAMP:
					((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_concise_type = dataType;
					((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_type = SQL_DATETIME;
					if (dataType == SQL_TYPE_DATE)
						((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_datetime_interval_code = SQL_CODE_DATE;
					else if (dataType == SQL_TYPE_TIME)
						((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_datetime_interval_code = SQL_CODE_TIME;
					else ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_datetime_interval_code = SQL_CODE_TIMESTAMP;
					break;
				default:
					sprintf(errmsg, "unsupported SQL data type: %d", (INT) dataType);
					error_record(&desc->errors, "HYC00", TEXT_HYC00, errmsg, 0);
					exitsync();
					return SQL_ERROR;
				}
				break;
			case TYPE_APP_DESC:
				switch (dataType) {
				case SQL_C_CHAR:
				case SQL_C_DEFAULT:
				case SQL_C_DOUBLE:
				case SQL_C_FLOAT:
				case SQL_C_LONG:
				case SQL_C_NUMERIC:
				case SQL_C_SHORT:
				case SQL_C_SLONG:
				case SQL_C_SSHORT:
				case SQL_C_STINYINT:
				case SQL_C_TINYINT:
				case SQL_C_ULONG:
				case SQL_C_USHORT:
				case SQL_C_UTINYINT:
					((APP_DESC_RECORD*)drec)->sql_desc_concise_type = dataType;
					((APP_DESC_RECORD*)drec)->sql_desc_type = dataType;
					((APP_DESC_RECORD*)drec)->sql_desc_datetime_interval_code = 0;
					break;
				case SQL_C_TYPE_DATE:
				case SQL_C_TYPE_TIME:
				case SQL_C_TYPE_TIMESTAMP:
					((APP_DESC_RECORD*)drec)->sql_desc_concise_type = dataType;
					((APP_DESC_RECORD*)drec)->sql_desc_type = SQL_DATETIME;
					if (dataType == SQL_C_TYPE_DATE)
						((APP_DESC_RECORD*)drec)->sql_desc_datetime_interval_code = SQL_CODE_DATE;
					else if (dataType == SQL_C_TYPE_TIME)
						((APP_DESC_RECORD*)drec)->sql_desc_datetime_interval_code = SQL_CODE_TIME;
					else if (dataType == SQL_C_TYPE_TIMESTAMP)
						((APP_DESC_RECORD*)drec)->sql_desc_datetime_interval_code = SQL_CODE_TIMESTAMP;
					break;
				default:
					sprintf(errmsg, "unsupported C data type: %d", (INT) dataType);
					error_record(&desc->errors, "HYC00", TEXT_HYC00, errmsg, 0);
					exitsync();
					return SQL_ERROR;
				}
				break;
			}
			break;
		case SQL_DESC_DATA_PTR:
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				break;
			case TYPE_APP_DESC:
				((APP_DESC_RECORD *) drec)->sql_desc_data_ptr = ValuePtr;
				if (ValuePtr == NULL) desc_unbind(desc, RecNumber);
				else {
					if (!consistency_check(desc, drec)) {
						exitsync();
						return SQL_ERROR;
					}
					if (RecNumber > desc->sql_desc_count) desc->sql_desc_count = RecNumber;
				}
				break;
			}
			break;
		case SQL_DESC_DATETIME_INTERVAL_CODE:
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_datetime_interval_code = (SQLSMALLINT) SQLPTR_TO_SQLUINT ValuePtr;
				break;
			case TYPE_APP_DESC:
				((APP_DESC_RECORD *) drec)->sql_desc_datetime_interval_code = (SQLSMALLINT) SQLPTR_TO_SQLUINT ValuePtr;
				break;
			}
			break;
		case SQL_DESC_DATETIME_INTERVAL_PRECISION:
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_datetime_interval_precision = SQLPTR_TO_SQLINT ValuePtr;
				break;
			case TYPE_APP_DESC:
				((APP_DESC_RECORD *) drec)->sql_desc_datetime_interval_precision = SQLPTR_TO_SQLINT ValuePtr;
				break;
			}
			break;
		case SQL_DESC_DISPLAY_SIZE:
		case SQL_DESC_FIXED_PREC_SCALE:
			break;
		case SQL_DESC_INDICATOR_PTR:
			if (desc->type == TYPE_APP_DESC)
				((APP_DESC_RECORD *) drec)->sql_desc_indicator_ptr = ValuePtr;
			break;
		case SQL_DESC_LABEL:
			break;
		case SQL_DESC_LENGTH:
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				((IMP_PARAM_DESC_RECORD *)drec)->sql_desc_length = SQLPTR_TO_SQLUINT ValuePtr;
				break;
			case TYPE_APP_DESC:
				((APP_DESC_RECORD *) drec)->sql_desc_length = SQLPTR_TO_SQLUINT ValuePtr;
				break;
			}
			break;
		case SQL_DESC_LITERAL_PREFIX:
		case SQL_DESC_LITERAL_SUFFIX:
		case SQL_DESC_LOCAL_TYPE_NAME:
			break;
		case SQL_DESC_NAME:
			if (desc->type == TYPE_IMP_PARAM_DESC) {
				if (ValuePtr != NULL) {
					if (BufferLength > MAX_NAME_LENGTH) {
						error_record(&desc->errors, "22001", "String data, right truncated", NULL, 0);
						rc = SQL_SUCCESS_WITH_INFO;
					}
					cb = (SQLSMALLINT) min(BufferLength, MAX_NAME_LENGTH);
					memcpy(((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_name, ValuePtr, cb);
					((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_name[cb] = '\0';
					((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_unnamed = SQL_NAMED;
				}
				else {
					((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_name[0] = '\0';
					((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_unnamed = SQL_UNNAMED;
				}
			}
			break;
		case SQL_DESC_NULLABLE:
			break;
		case SQL_DESC_NUM_PREC_RADIX:
			i1 = SQLPTR_TO_SQLINT ValuePtr;
			if (i1 == 10 || i1 == 0 || i1 == 2) {
				switch (desc->type) {
				case TYPE_IMP_PARAM_DESC:
					((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_num_prec_radix = i1;
					break;
				case TYPE_APP_DESC:
					((APP_DESC_RECORD *) drec)->sql_desc_num_prec_radix = i1;
					break;
				}
			}
			else {
				error_record(&desc->errors, "HY021", TEXT_HY021, "SQL_DESC_NUM_PREC_RADIX must be 0, 2, 10", 0);
				exitsync();
				return SQL_ERROR;
			}
			break;
		case SQL_DESC_OCTET_LENGTH:
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_octet_length = SQLPTR_TO_SQLINT ValuePtr;
				break;
			case TYPE_APP_DESC:
				((APP_DESC_RECORD *) drec)->sql_desc_octet_length = SQLPTR_TO_SQLINT ValuePtr;
				break;
			}
			break;
		case SQL_DESC_OCTET_LENGTH_PTR:
			if (desc->type == TYPE_APP_DESC) {
				((APP_DESC_RECORD *) drec)->sql_desc_octet_length_ptr = ValuePtr;
			}
			break;
		case SQL_DESC_PARAMETER_TYPE:
			if (desc->type == TYPE_IMP_PARAM_DESC) {
				((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_parameter_type = (SQLSMALLINT) SQLPTR_TO_SQLUINT ValuePtr;
			}
			break;
		case SQL_DESC_PRECISION:
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_precision = (SQLSMALLINT) SQLPTR_TO_SQLUINT ValuePtr;
				break;
			case TYPE_APP_DESC:
				((APP_DESC_RECORD *) drec)->sql_desc_precision = (SQLSMALLINT) SQLPTR_TO_SQLUINT ValuePtr;
				break;
			}
			break;
		case SQL_DESC_SCALE:
			switch (desc->type) {
			case TYPE_IMP_PARAM_DESC:
				((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_scale = (SQLSMALLINT) SQLPTR_TO_SQLUINT ValuePtr;
				break;
			case TYPE_APP_DESC:
				((APP_DESC_RECORD *) drec)->sql_desc_scale = (SQLSMALLINT) SQLPTR_TO_SQLUINT ValuePtr;
				break;
			}
			break;
		case SQL_DESC_SCHEMA_NAME:
		case SQL_DESC_SEARCHABLE:
		case SQL_DESC_TABLE_NAME:
			break;
		case SQL_DESC_TYPE:
			rc = setDescType((SQLSMALLINT) SQLPTR_TO_SQLUINT ValuePtr, desc->type, drec, &desc->errors);
			break;
		case SQL_DESC_TYPE_NAME:
			break;
		case SQL_DESC_UNNAMED:
			if (desc->type == TYPE_IMP_PARAM_DESC) {
				if ((SQLSMALLINT) SQLPTR_TO_SQLUINT ValuePtr == SQL_UNNAMED) {
					((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_unnamed = SQL_UNNAMED;
				}
				else{
					error_record(&desc->errors, "HY091", HY091TEXT, NULL, 0);
					exitsync();
					return SQL_ERROR;
				}
			}
			break;
		case SQL_DESC_UNSIGNED:
		case SQL_DESC_UPDATABLE:
			break;
		}
	}
	exitsync();
	return rc;
}

SQLRETURN SQL_API SQLSetDescRec(
	SQLHDESC DescriptorHandle,
	SQLSMALLINT RecNumber,
	SQLSMALLINT Type,
	SQLSMALLINT SubType,
	SQLLEN Length,
	SQLSMALLINT Precision,
	SQLSMALLINT Scale,
	SQLPOINTER DataPtr,
	SQLLEN *StringLengthPtr,
	SQLLEN *IndicatorPtr)
{
	LPVOID drec;
	CHAR errmsg[128];
	DESCRIPTOR *desc;

	desc = (DESCRIPTOR *) DescriptorHandle;
	error_init(&desc->errors);

	if (desc->type == TYPE_IMP_ROW_DESC) {  /* cannot modify IRD */
		error_record(&desc->errors, "HY016", HY016TEXT, NULL, 0);
		return SQL_ERROR;
	}
	if (RecNumber == 0) {  /* we don't support bookmarks */
		sprintf(errmsg, "%s: Bookmarks not supported", TEXT_07009);
		error_record(&desc->errors, "07009", errmsg, NULL, 0);
		return SQL_ERROR;
	}
	entersync();
	drec = desc_get(desc, RecNumber, TRUE);
	if (drec == NULL) {
		error_record(&desc->errors, "HY001", TEXT_HY001, NULL, 0);
		exitsync();
		return SQL_ERROR;
	}
	if (RecNumber > desc->sql_desc_count) desc->sql_desc_count = RecNumber;

	/* set SQL_DESC_TYPE */
	if (setDescType(Type, desc->type, drec, &desc->errors) != SQL_SUCCESS) {
		exitsync();
		return SQL_ERROR;
	}

	/* set SQL_DESC_DATETIME_INTERVAL_CODE */
	if (Type == SQL_DATETIME || Type == SQL_INTERVAL) {
		switch (desc->type) {
		case TYPE_APP_DESC:
			((APP_DESC_RECORD *) drec)->sql_desc_datetime_interval_code = SubType;
			break;
		case TYPE_IMP_PARAM_DESC:
			((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_datetime_interval_code = SubType;
			break;
		}
	}

	/* set SQL_DESC_OCTET_LENGTH */
	switch (desc->type) {
	case TYPE_IMP_PARAM_DESC:
		((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_octet_length = Length;
		break;
	case TYPE_APP_DESC:
		((APP_DESC_RECORD*)drec)->sql_desc_octet_length = Length;
		break;
	}

	/* set SQL_DESC_PRECISION */
	switch (desc->type) {
	case TYPE_IMP_PARAM_DESC:
		((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_precision = Precision;
		break;
	case TYPE_APP_DESC:
		((APP_DESC_RECORD *) drec)->sql_desc_precision = Precision;
		break;
	}

	/* set SQL_DESC_SCALE */
	switch (desc->type) {
	case TYPE_IMP_PARAM_DESC:
		((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_scale = Scale;
		break;
	case TYPE_APP_DESC:
		((APP_DESC_RECORD *) drec)->sql_desc_scale = Scale;
		break;
	}

	/* set SQL_DESC_DATA_PTR */
	if (desc->type == TYPE_APP_DESC) {
		((APP_DESC_RECORD *) drec)->sql_desc_data_ptr = DataPtr;
		if (DataPtr == NULL) desc_unbind(desc, RecNumber);
		else {
			if (!consistency_check(desc, drec)) {
				exitsync();
				return SQL_ERROR;
			}
			if (RecNumber > desc->sql_desc_count) desc->sql_desc_count = RecNumber;
		}
	}

	/* set SQL_DESC_OCTET_LENGTH_PTR */
	if (desc->type == TYPE_APP_DESC) {
		((APP_DESC_RECORD *) drec)->sql_desc_octet_length_ptr = StringLengthPtr;
	}

	/* set SQL_DESC_INDICATOR_PTR */
	if (desc->type == TYPE_APP_DESC)
		((APP_DESC_RECORD *) drec)->sql_desc_indicator_ptr = IndicatorPtr;

	exitsync();
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetEnvAttr(
	SQLHENV EnvironmentHandle,
	SQLINTEGER Attribute,
	SQLPOINTER ValuePtr,
	SQLINTEGER StringLength)
{
	ENVIRONMENT *env;


	env = (ENVIRONMENT *) EnvironmentHandle;
	switch (Attribute) {
	case SQL_ATTR_CONNECTION_POOLING:  /* DM */
		break;
	case SQL_ATTR_CP_MATCH:  /* DM */
		break;
	case SQL_ATTR_ODBC_VERSION:
		env->sql_attr_odbc_version = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_OUTPUT_NTS:  /* DM */
		break;
	default:  /* invalid attribute */
		error_record(&env->errors, "HY092", TEXT_HY092, NULL, 0);
		return SQL_ERROR;
	}
	return SQL_SUCCESS;
}

#if 0
SQLRETURN SQL_API SQLSetPos(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT RowNumber,
	SQLUSMALLINT Operation,
	SQLUSMALLINT LockType)
{
/*** CODE: IMPLEMENT (WAS NOT SUPPORTED IN FS 2) ***/
	return SQL_SUCCESS;
}
#endif

SQLRETURN SQL_API SQLSetStmtAttr(
	SQLHSTMT StatementHandle,
	SQLINTEGER Attribute,
	SQLPOINTER ValuePtr,
	SQLINTEGER StringLength)
{
	STATEMENT *stmt = (STATEMENT *) StatementHandle;

	error_init(&stmt->errors);

	switch (Attribute) {
	case SQL_ATTR_APP_PARAM_DESC:
		if ((DESCRIPTOR *) ValuePtr == SQL_NULL_HDESC)
			stmt->sql_attr_app_param_desc = stmt->apdsave;	/* set implicitly allocated descriptor */
		else stmt->sql_attr_app_param_desc = (DESCRIPTOR *) ValuePtr;
		break;
	case SQL_ATTR_APP_ROW_DESC:
		if ((DESCRIPTOR *) ValuePtr == SQL_NULL_HDESC)
			stmt->sql_attr_app_row_desc = stmt->ardsave; /* set implicitly allocated ard */
		else stmt->sql_attr_app_row_desc = (DESCRIPTOR *) ValuePtr;
		break;
	case SQL_ATTR_ASYNC_ENABLE:
		if (SQLPTR_TO_SQLUINT ValuePtr == SQL_ASYNC_ENABLE_ON) {
			error_record(&stmt->errors, "HYC00", TEXT_HYC00, "driver does not support asynchronous execution", 0);
			return SQL_ERROR;
		}
		stmt->sql_attr_async_enable = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_CONCURRENCY:
		stmt->sql_attr_concurrency = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_CURSOR_SCROLLABLE:
		stmt->sql_attr_cursor_scrollable = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_CURSOR_SENSITIVITY:
		stmt->sql_attr_cursor_sensitivity = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_CURSOR_TYPE:
		stmt->sql_attr_cursor_type = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_ENABLE_AUTO_IPD:
		if (SQLPTR_TO_SQLUINT ValuePtr == SQL_TRUE) {
			error_record(&stmt->errors, "01S02", C01S02TEXT, NULL, 0);
			return SQL_SUCCESS_WITH_INFO;
		}
		break;
	case SQL_ATTR_FETCH_BOOKMARK_PTR:
	case SQL_ATTR_IMP_PARAM_DESC:		/* read-only */
	case SQL_ATTR_IMP_ROW_DESC:		/* read-only */
		break;
	case SQL_ATTR_KEYSET_SIZE:
		stmt->sql_attr_keyset_size = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_MAX_LENGTH:
		stmt->sql_attr_max_length = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_MAX_ROWS:
		stmt->sql_attr_max_rows = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_METADATA_ID:
		stmt->sql_attr_metadata_id = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_NOSCAN:
		stmt->sql_attr_noscan = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_PARAM_BIND_OFFSET_PTR:
		stmt->sql_attr_app_param_desc->sql_desc_bind_offset_ptr = (SQLUINTEGER *) ValuePtr;
		break;
	case SQL_ATTR_PARAM_BIND_TYPE:
		stmt->sql_attr_app_param_desc->sql_desc_bind_type = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_PARAM_OPERATION_PTR:
		stmt->sql_attr_app_param_desc->sql_desc_array_status_ptr = (SQLUSMALLINT *) ValuePtr;
		break;
	case SQL_ATTR_PARAM_STATUS_PTR:
		stmt->sql_attr_imp_param_desc->sql_desc_array_status_ptr = (SQLUSMALLINT *) ValuePtr;
		break;
	case SQL_ATTR_PARAMS_PROCESSED_PTR:
		stmt->sql_attr_imp_param_desc->sql_desc_rows_processed_ptr = (SQLUINTEGER *) ValuePtr;
		break;
	case SQL_ATTR_PARAMSET_SIZE:
		stmt->sql_attr_app_param_desc->sql_desc_array_size = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_QUERY_TIMEOUT:
		stmt->sql_attr_query_timeout = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_RETRIEVE_DATA:
		stmt->sql_attr_retrieve_data = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_ROW_ARRAY_SIZE:
	case SQL_ROWSET_SIZE:  /* ODBC 2.0 compatability */
		if (SQLPTR_TO_SQLUINT ValuePtr != 1) {
			error_record(&stmt->errors, "01S02", C01S02TEXT, NULL, 0);
			return SQL_SUCCESS_WITH_INFO;
		}
		break;
	case SQL_ATTR_ROW_BIND_OFFSET_PTR:
		stmt->sql_attr_app_row_desc->sql_desc_bind_offset_ptr = (SQLUINTEGER *) ValuePtr;
		break;
	case SQL_ATTR_ROW_BIND_TYPE:
		stmt->sql_attr_app_row_desc->sql_desc_bind_type = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_ROW_NUMBER: /* read-only */
		break;
	case SQL_ATTR_ROW_OPERATION_PTR:
		stmt->sql_attr_app_row_desc->sql_desc_array_status_ptr = (SQLUSMALLINT *) ValuePtr;
		break;
	case SQL_ATTR_ROW_STATUS_PTR:
		stmt->sql_attr_imp_row_desc->sql_desc_array_status_ptr = (SQLUSMALLINT *) ValuePtr;
		break;
	case SQL_ATTR_ROWS_FETCHED_PTR:
		stmt->sql_attr_imp_row_desc->sql_desc_rows_processed_ptr = (SQLUINTEGER *) ValuePtr;
		break;
	case SQL_ATTR_SIMULATE_CURSOR:
		stmt->sql_attr_simulate_cursor = SQLPTR_TO_SQLUINT ValuePtr;
		break;
	case SQL_ATTR_USE_BOOKMARKS: /* we don't support bookmarks */
		if (SQLPTR_TO_SQLUINT ValuePtr == SQL_UB_ON) {
			error_record(&stmt->errors, "01S02", C01S02TEXT, NULL, 0);
			return SQL_SUCCESS_WITH_INFO;
		}
		break;
	default:
		error_record(&stmt->errors, "HYC00", TEXT_HYC00 , NULL, 0);
		return SQL_ERROR;
	}
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSpecialColumns(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT IdentifierType,
	SQLCHAR *CatalogName,
	SQLSMALLINT NameLength1,
	SQLCHAR *SchemaName,
	SQLSMALLINT NameLength2,
	SQLCHAR *TableName,
	SQLSMALLINT NameLength3,
	SQLUSMALLINT Scope,
	SQLUSMALLINT Nullable)
{
	INT i1, replylen;
	SQLRETURN rc, rc2;
	CHAR *ptr;
	CHAR buffer[512];  /* must fit 3 names, string '????? ' or '????? ', 6 quotes, 1 digit & 3 spaces */
	STATEMENT *stmt;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	if (TableName != NULL) {
		if (NameLength3 == SQL_NTS) NameLength3 = (SQLSMALLINT)strlen((const char *)TableName);
	}
	else NameLength3 = 0;

	/* copy identifier names into the buffer */
	if (IdentifierType == SQL_BEST_ROWID) {
		memcpy(buffer, "BESTROWID ", 10);
		i1 = 10;
		if (stmt->cnct->server_majorver == 2) {
			memcpy(buffer + i1, "NULL NULL ", 10);
			i1 += 10;
			if (NameLength3) {
				memcpy(buffer + i1, TableName, NameLength3);
				i1 += NameLength3;
			}
			else {
				memcpy(buffer + i1, "NULL", 4);
				i1 += 4;
			}
			memcpy(buffer + i1, " NULL", 5);
			i1 += 4;
			ptr = GETATTR;
		}
		else {
			i1 += tcpquotedcopy((UCHAR*)(buffer + i1), TableName, NameLength3);
			ptr = CATALOG;
		}
	}

	/* check for invalid cursor state */
	if (stmt->executeflags & EXECUTE_RESULTSET) {
		error_record(&stmt->errors, "24000", TEXT_24000, NULL, 0);
		return SQL_ERROR;
	}
	entersync();
	if (stmt->executeflags & EXECUTE_RSID) {
		stmt->executeflags &= ~EXECUTE_RSID;
		if (server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD, NULL, 0, NULL, NULL, NULL) != SERVER_OK) {
			exitsync();
			return SQL_ERROR;
		}
	}
	stmt->executeflags = 0;
	stmt->row_count = -1;

	if (IdentifierType == SQL_BEST_ROWID) {
		replylen = sizeof(buffer);
		rc = server_communicate(stmt->cnct, (UCHAR*)EMPTY, (UCHAR*)ptr,
				(UCHAR*)buffer, i1, &stmt->errors, (UCHAR*)buffer, &replylen);
		/*** CODE: SHOULD ERROR BE RETURNED IF SERVER_OK (NOT SERVER_FAIL OR COMM ERROR) ***/
		if (rc != SERVER_SET && rc != SERVER_NONE) {
			exitsync();
			return SQL_ERROR;
		}
	}
	else {
		replylen = 0;
		rc = SERVER_SET;
	}

	rc2 = exec_resultset(stmt, &stmt->errors, buffer, replylen, EXECUTE_SQLSPECIALCOLUMNS);
	if ((rc2 == SQL_ERROR || rc == SERVER_NONE) && (stmt->executeflags & EXECUTE_RSID)) {
		stmt->executeflags &= ~EXECUTE_RSID;
		server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD, NULL, 0, NULL, NULL, NULL);
	}
	if (rc2 != SQL_ERROR) stmt->executeflags |= EXECUTE_EXECUTED;
	else stmt->executeflags = 0;
	exitsync();
	return rc2;
}

SQLRETURN SQL_API SQLStatistics(
	SQLHSTMT StatementHandle,
	SQLCHAR *CatalogName,
	SQLSMALLINT NameLength1,
	SQLCHAR *SchemaName,
	SQLSMALLINT NameLength2,
	SQLCHAR *TableName,
	SQLSMALLINT NameLength3,
	SQLUSMALLINT Unique,
	SQLUSMALLINT Reserved)
{
	INT i1, replylen;
	SQLRETURN rc, rc2;
	CHAR buffer[512];  /* must fit 4 names, 8 quotes, 3 spaces, and message function string of 'TABLES ' */
	CHAR *ptr;
	STATEMENT *stmt;

	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	if (TableName != NULL) {
		if (NameLength3 == SQL_NTS) NameLength3 = (SQLSMALLINT)strlen((const char *)TableName);
	}
	else NameLength3 = 0;

	/* copy identifier names into the buffer */
	memcpy(buffer, "STATS ", 6);
	i1 = 6;
	if (stmt->cnct->server_majorver == 2) {
		memcpy(buffer + i1, "NULL NULL ", 10);
		i1 += 10;
		if (NameLength3) {
			memcpy(buffer + i1, TableName, NameLength3);
			i1 += NameLength3;
		}
		else {
			memcpy(buffer + i1, "NULL", 4);
			i1 += 4;
		}
		memcpy(buffer + i1, " NULL", 5);
		i1 += 4;
		ptr = GETATTR;
	}
	else {
		i1 += tcpquotedcopy((UCHAR*)(buffer + i1), TableName, NameLength3);
		ptr = CATALOG;
	}

	buffer[i1++] = ' ';
	if (Unique == SQL_INDEX_UNIQUE)
		buffer[i1++] = '0';
	else
		buffer[i1++] = '1';

	/* check for invalid cursor state */
	if (stmt->executeflags & EXECUTE_RESULTSET) {
		error_record(&stmt->errors, "24000", TEXT_24000, NULL, 0);
		return SQL_ERROR;
	}
	entersync();
	if (stmt->executeflags & EXECUTE_RSID) {
		stmt->executeflags &= ~EXECUTE_RSID;
		if (server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD, NULL, 0, NULL, NULL, NULL) != SERVER_OK) {
			exitsync();
			return SQL_ERROR;
		}
	}
	stmt->executeflags = 0;
	stmt->row_count = -1;

	replylen = sizeof(buffer);
	rc = server_communicate(stmt->cnct, (UCHAR*)EMPTY, (UCHAR*)ptr,
		(UCHAR*)buffer, i1, &stmt->errors, (UCHAR*)buffer, &replylen);
/*** CODE: SHOULD ERROR BE RETURNED IF SERVER_OK (NOT SERVER_FAIL OR COMM ERROR) ***/
	if (rc != SERVER_SET && rc != SERVER_NONE) {
		exitsync();
		return SQL_ERROR;
	}
	rc2 = exec_resultset(stmt, &stmt->errors, buffer, replylen, EXECUTE_SQLSTATISTICS);
	if ((rc2 == SQL_ERROR || rc == SERVER_NONE) && (stmt->executeflags & EXECUTE_RSID)) {
		stmt->executeflags &= ~EXECUTE_RSID;
		server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD, NULL, 0, NULL, NULL, NULL);
	}
	if (rc2 != SQL_ERROR) stmt->executeflags |= EXECUTE_EXECUTED;
	else stmt->executeflags = 0;
	exitsync();
	return rc2;
}

#if 0
/*** CODE: DO NOT SUPPORT INITIALLY ***/
SQLRETURN SQL_API SQLTablePrivileges(
	SQLHSTMT StatementHandle,
	SQLCHAR *CatalogName,
	SQLSMALLINT NameLength1,
	SQLCHAR *SchemaName,
	SQLSMALLINT NameLength2,
	SQLCHAR *TableName,
	SQLSMALLINT NameLength3)
{
	return SQL_SUCCESS;
}
#endif

SQLRETURN SQL_API SQLTables(
	SQLHSTMT StatementHandle,
	SQLCHAR *CatalogName,	/* Not used in this function */
	SQLSMALLINT NameLength1,
	SQLCHAR *SchemaName,	/* Not used in this function */
	SQLSMALLINT NameLength2,
	SQLCHAR *TableName,
	SQLSMALLINT NameLength3,
	SQLCHAR *TableType,   /* Not used in this function */
	SQLSMALLINT NameLength4)
{
	INT i1, i2, replylen;
	SQLRETURN rc, rc2;
	CHAR buffer[512];  /* must fit 4 names, 8 quotes, 3 spaces, and message function string of 'TABLES ' */
	CHAR buffer2[2 * MAX_NAME_LENGTH];
	CHAR *ptr;
	STATEMENT *stmt;
	stmt = (STATEMENT *) StatementHandle;
	error_init(&stmt->errors);

	if (TableName != NULL) {
		if (NameLength3 == SQL_NTS) NameLength3 = (SQLSMALLINT)strlen((const char *)TableName);
	}
	else NameLength3 = 0;

	/**
	 * Special trap for Excel.
	 */
	if (CatalogName != NULL) {
		size_t catNameLen, schemaNameLen, tableNameLen;
		if (NameLength1 == SQL_NTS) catNameLen = strlen((const char *)CatalogName);
		else catNameLen = NameLength1;
		if (SchemaName != NULL) {
			if (NameLength2 == SQL_NTS) schemaNameLen = strlen((const char *)SchemaName);
			else schemaNameLen = NameLength2;
		}
		else schemaNameLen = 0;
		if (TableName != NULL) {
			if (NameLength3 == SQL_NTS) tableNameLen = strlen((const char *)TableName);
			else tableNameLen = NameLength3;
		}
		else tableNameLen = 0;
		if (catNameLen == 1 && memcmp(CatalogName, SQL_ALL_CATALOGS, 1) == 0) {
			if (schemaNameLen == 0 && tableNameLen == 0) {
				error_record(&stmt->errors, "IM001", TEXT_IM001, "FS does not have Catalogs", 0);
				return SQL_ERROR;
			}
		}
	}

	/* copy identifier names into the buffer */
	memcpy(buffer, "TABLES ", 7);
	i1 = 7;
	if (stmt->cnct->server_majorver == 2) {
		memcpy(buffer + i1, "NULL NULL ", 10);
		i1 += 10;
		if (NameLength3) {
			memcpy(buffer + i1, TableName, NameLength3);
			i1 += NameLength3;
		}
		else {
			memcpy(buffer + i1, "NULL", 4);
			i1 += 4;
		}
		memcpy(buffer + i1, " NULL", 5);
		i1 += 4;
		ptr = GETATTR;
	}
	else {
		if (stmt->sql_attr_metadata_id == SQL_TRUE) {
			if (TableName == NULL) {
				error_record(&stmt->errors, "HY009", TEXT_HY009, "TableName cannot be NULL", 0);
				return SQL_ERROR;
			}
			i2 = hide_escapes(buffer2, (CHAR*)TableName, NameLength3);
			i1 += tcpquotedcopy((unsigned char *)(buffer + i1), (UCHAR*)buffer2, i2);
		}
		else {
			i1 += tcpquotedcopy((unsigned char *)(buffer + i1), (unsigned char *) TableName, NameLength3);
		}
		ptr = CATALOG;
	}

	/* check for invalid cursor state */
	if (stmt->executeflags & EXECUTE_RESULTSET) {
		error_record(&stmt->errors, "24000", TEXT_24000, NULL, 0);
		return SQL_ERROR;
	}
	entersync();
	if (stmt->executeflags & EXECUTE_RSID) {
		stmt->executeflags &= ~EXECUTE_RSID;
		if (server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD, NULL, 0, NULL, NULL, NULL) != SERVER_OK) {
			exitsync();
			return SQL_ERROR;
		}
	}
	stmt->executeflags = 0;
	stmt->row_count = -1;

	replylen = sizeof(buffer);
	rc = server_communicate(stmt->cnct, (UCHAR*)EMPTY, (UCHAR*)ptr,
		(UCHAR*)buffer, i1, &stmt->errors, (UCHAR*)buffer, &replylen);
/*** CODE: SHOULD ERROR BE RETURNED IF SERVER_OK (NOT SERVER_FAIL OR COMM ERROR) ***/
	if (rc != SERVER_SET && rc != SERVER_NONE) {
		exitsync();
		return SQL_ERROR;
	}
	rc2 = exec_resultset(stmt, &stmt->errors, buffer, replylen, EXECUTE_SQLTABLES);
	if ((rc2 == SQL_ERROR || rc == SERVER_NONE) && (stmt->executeflags & EXECUTE_RSID)) {
		stmt->executeflags &= ~EXECUTE_RSID;
		server_communicate(stmt->cnct, (UCHAR*)stmt->rsid, (UCHAR*)DISCARD, NULL, 0, NULL, NULL, NULL);
	}
	if (rc2 != SQL_ERROR) stmt->executeflags |= EXECUTE_EXECUTED;
	else stmt->executeflags = 0;
	exitsync();
	return rc2;
}

static SQLRETURN setDescType(SQLSMALLINT dataType, INT descType, LPVOID drec, ERRORS* er)
{
	CHAR errmsg[128];

	switch (descType) {
	case TYPE_IMP_PARAM_DESC:
		switch (dataType) {
		case SQL_CHAR:
		case SQL_NUMERIC:
			((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_concise_type = dataType;
			((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_type = dataType;
			((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_datetime_interval_code = 0;
			break;
		case SQL_DATETIME:
			switch (((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_datetime_interval_code) {
			case SQL_CODE_DATE:
				((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_concise_type = SQL_TYPE_DATE;
				break;
			case SQL_CODE_TIME:
				((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_concise_type = SQL_TYPE_TIME;
				break;
			case SQL_CODE_TIMESTAMP:
				((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_concise_type = SQL_TYPE_TIMESTAMP;
				break;
			}
			((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_type = dataType;
			break;
		default:
			sprintf(errmsg, "unsupported SQL data type: %d", (INT) dataType);
			error_record(er, "HY021", TEXT_HY021, errmsg, 0);
			return SQL_ERROR;
		}
		break;
	case TYPE_APP_DESC:
		switch (dataType) {
		case SQL_C_CHAR:
		case SQL_C_DEFAULT:
		case SQL_C_DOUBLE:
		case SQL_C_FLOAT:
		case SQL_C_LONG:
		case SQL_C_NUMERIC:
		case SQL_C_SHORT:
		case SQL_C_SLONG:
		case SQL_C_SSHORT:
		case SQL_C_STINYINT:
		case SQL_C_TINYINT:
		case SQL_C_ULONG:
		case SQL_C_USHORT:
		case SQL_C_UTINYINT:
			((APP_DESC_RECORD *) drec)->sql_desc_concise_type = dataType;
			((APP_DESC_RECORD *) drec)->sql_desc_type = dataType;
			((APP_DESC_RECORD *) drec)->sql_desc_datetime_interval_code = 0;
			break;
		case SQL_DATETIME:
			switch (((APP_DESC_RECORD *) drec)->sql_desc_datetime_interval_code) {
			case SQL_CODE_DATE:
				((APP_DESC_RECORD *) drec)->sql_desc_concise_type = SQL_C_TYPE_DATE;
				break;
			case SQL_CODE_TIME:
				((APP_DESC_RECORD *) drec)->sql_desc_concise_type = SQL_C_TYPE_TIME;
				break;
			case SQL_CODE_TIMESTAMP:
				((APP_DESC_RECORD *) drec)->sql_desc_concise_type = SQL_C_TYPE_TIMESTAMP;
				break;
			}
			break;
		default:
			sprintf(errmsg, "unsupported C data type: %d", (INT) dataType);
			error_record(er, "HY021", TEXT_HY021, errmsg, 0);
			return SQL_ERROR;
		}
		break;
	}
	return SQL_SUCCESS;
}

static BOOL consistency_check(DESCRIPTOR *desc, LPVOID drec)
{
	static SQLINTEGER app_v_types[] = { SQL_C_CHAR, SQL_C_DEFAULT, SQL_C_LONG, SQL_C_SLONG, SQL_C_ULONG,
		SQL_C_SHORT, SQL_C_SSHORT, SQL_C_USHORT, SQL_C_TINYINT, SQL_C_STINYINT, SQL_C_UTINYINT,
		SQL_C_DOUBLE, SQL_C_FLOAT, SQL_C_NUMERIC, SQL_DATETIME };
	static SQLINTEGER imp_v_types[] = { SQL_CHAR, SQL_VARCHAR, SQL_LONGVARCHAR, SQL_NUMERIC, SQL_DECIMAL, SQL_DATETIME };
	static INT app_v_types_length = (sizeof(app_v_types) / sizeof(*app_v_types));
	static INT imp_v_types_length = (sizeof(imp_v_types) / sizeof(*imp_v_types));
	INT i1;
	BOOL rc = TRUE;
	CHAR buffer[128];
	SQLSMALLINT vtype;	/* SQL_DESC_TYPE, aka verbose type */
	SQLSMALLINT ctype;	/* SQL_DESC_CONCISE_TYPE */
	SQLSMALLINT dicode;	/* SQL_DESC_DATETIME_INTERVAL_CODE */

	if (desc->type == TYPE_APP_DESC) {
		vtype = ((APP_DESC_RECORD *) drec)->sql_desc_type;
		ctype = ((APP_DESC_RECORD *) drec)->sql_desc_concise_type;
		dicode = ((APP_DESC_RECORD *) drec)->sql_desc_datetime_interval_code;
		for (i1 = 0; i1 < app_v_types_length; i1++) {
			if (vtype == app_v_types[i1]) break;
		}
		if (i1 == app_v_types_length) {
			sprintf(buffer, "SQL_DESC_TYPE invalid [%d]", vtype);
			error_record(&desc->errors, "HY021", TEXT_HY021, buffer, 0);
			rc = FALSE;
		}
		else {
			if (vtype == SQL_DATETIME) {
				switch (ctype) {
				case SQL_C_TYPE_DATE:
					if (dicode != SQL_CODE_DATE) {
						sprintf(buffer, "SQL_DESC_DATETIME_INTERVAL_CODE invalid [%d]", dicode);
						error_record(&desc->errors, "HY021", TEXT_HY021, buffer, 0);
						rc = FALSE;
					}
					break;
				case SQL_C_TYPE_TIME:
					if (dicode != SQL_CODE_TIME) {
						sprintf(buffer, "SQL_DESC_DATETIME_INTERVAL_CODE invalid [%d]", dicode);
						error_record(&desc->errors, "HY021", TEXT_HY021, buffer, 0);
						rc = FALSE;
					}
					break;
				case SQL_C_TYPE_TIMESTAMP:
					if (dicode != SQL_CODE_TIMESTAMP) {
						sprintf(buffer, "SQL_DESC_DATETIME_INTERVAL_CODE invalid [%d]", dicode);
						error_record(&desc->errors, "HY021", TEXT_HY021, buffer, 0);
						rc = FALSE;
					}
					break;
				default:
					sprintf(buffer, "SQL_DESC_CONCISE_TYPE invalid [%d]", ctype);
					error_record(&desc->errors, "HY021", TEXT_HY021, buffer, 0);
					rc = FALSE;
					break;
				}
			}
			else {
				if (ctype != vtype || dicode != 0) {
					error_record(&desc->errors, "HY021", TEXT_HY021, NULL, 0);
					rc = FALSE;
				}
			}
		}
	}
	else if (desc->type == TYPE_IMP_PARAM_DESC) {
		vtype = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_type;
		ctype = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_concise_type;
		dicode = ((IMP_PARAM_DESC_RECORD *) drec)->sql_desc_datetime_interval_code;
		for (i1 = 0; i1 < imp_v_types_length && vtype != imp_v_types[i1]; i1++) {}
		if (i1 == imp_v_types_length) {
			sprintf(buffer, "SQL_DESC_TYPE invalid [%d]", vtype);
			error_record(&desc->errors, "HY021", TEXT_HY021, buffer, 0);
			rc = FALSE;
		}
		else {
			if (vtype == SQL_DATETIME) {
				switch (ctype) {
				case SQL_TYPE_DATE:
					if (dicode != SQL_CODE_DATE) {
						sprintf(buffer, "SQL_DESC_DATETIME_INTERVAL_CODE invalid [%d]", dicode);
						error_record(&desc->errors, "HY021", TEXT_HY021, buffer, 0);
						rc = FALSE;
					}
					break;
				case SQL_TYPE_TIME:
					if (dicode != SQL_CODE_TIME) {
						sprintf(buffer, "SQL_DESC_DATETIME_INTERVAL_CODE invalid [%d]", dicode);
						error_record(&desc->errors, "HY021", TEXT_HY021, buffer, 0);
						rc = FALSE;
					}
					break;
				case SQL_TYPE_TIMESTAMP:
					if (dicode != SQL_CODE_TIMESTAMP) {
						sprintf(buffer, "SQL_DESC_DATETIME_INTERVAL_CODE invalid [%d]", dicode);
						error_record(&desc->errors, "HY021", TEXT_HY021, buffer, 0);
						rc = FALSE;
					}
					break;
				default:
					sprintf(buffer, "SQL_DESC_CONCISE_TYPE invalid [%d]", ctype);
					error_record(&desc->errors, "HY021", TEXT_HY021, buffer, 0);
					rc = FALSE;
					break;
				}
			}
			else {
				if (ctype != vtype || dicode != 0) {
					error_record(&desc->errors, "HY021", TEXT_HY021, NULL, 0);
					rc = FALSE;
				}
			}
		}
	}
	else {
		sprintf(buffer, "consistency_check, unknown desc type [%d]", desc->type);
		error_record(&desc->errors, "HY000", TEXT_HY000, buffer, 0);
		rc = FALSE;
	}
	return rc;
}

static INT hide_escapes(CHAR* output, CHAR* input, INT Length)
{
	INT i1 = 0, i2 = 0;
	for(;i1 < Length; i1++) {
		if (input[i1] == '\\' || input[i1] == '%' || input[i1] == '_') output[i2++] = '\\';
		output[i2++] = input[i1];
	}
	output[i2] = '\0';
	return i2;
}
