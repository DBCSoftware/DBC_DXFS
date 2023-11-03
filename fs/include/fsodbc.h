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

#define INC_SQL
#define INC_SQLEXT
#include "includes.h"
#include "release.h"

#if !OS_WIN32
#include "tcp.h"
typedef void* LPVOID;
#endif

#define FIELD_LENGTH 8

#define DRIVER_NAME "fs101odbc.dll"
#define DRIVER_VERSION FS_ODBC_VERSION

#define MAX_NAME_LENGTH 32		/* maximum length of data source, table, column, etc. names */
#define MAX_ERRORS 3			/* maximum number of errors for each handle type for SQLError */
#define MAX_ERROR_MSG 160		/* maximum length of text description of error */
#define MAX_TABLES_IN_SELECT 15	/* maximum number of tables in SELECT FROM table list */
#define MAX_CURSOR_NAME_LENGTH 18  /* value is minumum recommended by ODBC */
#define TIMESTAMP_LENGTH 23

/* error structure */
#define MAX_DYNFUNC_LEN 21

/* descriptor types */
#define TYPE_APP_DESC		1
#define TYPE_IMP_PARAM_DESC	2
#define TYPE_IMP_ROW_DESC	3

typedef struct ERRORINFO {
	SQLCHAR sqlstate[SQL_SQLSTATE_SIZE + 1];
	SQLINTEGER native;
	SQLCHAR text[MAX_ERROR_MSG + 1];
	SQLINTEGER columnNumber;
	SQLINTEGER rowNumber;
} ERRORINFO;

/*** CODE: THIS IS TOTALLY FUCKED UP.  ANYTHING THAT USES 'ERRORS', ***/
/***       WHICH IS BASICALLY EVERYTHING IS USING OVER 3K OF MEMORY. ***/
/***       KNOCKED IT DOWN TO 1K OF MEMORY, BUT THIS MAY NEED TO BECOME ***/
/***       MORE DYNAMIC ***/
typedef struct ERRORS {
	SQLINTEGER crsrows;						/* cursor row count */
	SQLCHAR dynfunc[MAX_DYNFUNC_LEN + 1];	/* dynamic function name */
	SQLINTEGER dfcode;						/* dynamic function code */
	SQLINTEGER numerrors;					/* number of errors */
	SQLINTEGER rowcount;					/* row count affected rows */
	ERRORINFO errorinfo[MAX_ERRORS];		/* description of each error */
} ERRORS;

typedef struct environment_struct {
	SQLINTEGER sql_attr_odbc_version;		/* 3.0 or 2.0 application behavior */
	ERRORS errors;
} ENVIRONMENT;

typedef struct client_connection_struct CONNECTION;

typedef struct descriptor_struct DESCRIPTOR;

typedef struct statement_struct STATEMENT;

/**
 * Represents a Connection from the client's point of view
 */
struct client_connection_struct {
	ENVIRONMENT *env;
	/* note: access to firststmt must be protected with entersync/exitsync */
	struct statement_struct *firststmt; /* linked list of statements associated with connection */
	/* note: access to firstexplicitdesc must be protected with entersync/exitsync */
	DESCRIPTOR *firstexplicitdesc; /* explicitly allocated application descriptors associated with connection */
	INT connected; /* connected to server = 1, fake connection for 'DRIVER' type = 2 */
	INT server_majorver;
	INT server_minorver;
	INT server_subrelease;
	SOCKET socket;
	INT tcpflags;
	LPVOID threadinfo;
	CHAR cnid[FIELD_LENGTH];
	CHAR *data_source;
	CHAR *user;
	ERRORS errors;
	/* connect attributes */
	SQLUINTEGER sql_attr_access_mode;
	SQLUINTEGER sql_attr_async_enable;
	SQLUINTEGER sql_attr_auto_ipd;
	SQLUINTEGER sql_attr_autocommit;
	SQLUINTEGER sql_attr_connection_dead;
	SQLUINTEGER sql_attr_connection_timeout;
	SQLCHAR *sql_attr_current_catalog;
	SQLUINTEGER sql_attr_login_timeout;
	SQLUINTEGER sql_attr_metadata_id;
	SQLUINTEGER sql_attr_packet_size;
	SQLHANDLE sql_attr_quiet_mode;
	SQLCHAR *sql_attr_translate_lib;
	DWORD sql_attr_translate_option;
	DWORD sql_attr_txn_isolation;
};

struct statement_struct {
	struct client_connection_struct *cnct;
	struct statement_struct *nextstmt;					/* linked list of statements associated with connection */
	INT executeflags;
	INT textlength;										/* execution text length */
	INT textsize;										/* execution text buffer size */
	UCHAR *text;										/* execution text buffer */
	INT row_count;										/* number of rows in result set or affected rows, -1 means unknown/not available */
	INT row_number;										/* best guess at current row number */
/*** CODE, SET ROW_LENGTH TO -1 IN APPROPRIATE PLACES ***/
	INT row_length;										/* length of row from last fetch, -1 means no previous fetch */
	INT row_buffer_size;
	UCHAR *row_buffer;
	SQLUSMALLINT lastcolumn;							/* last column retrieved by sqlgetdata, -1 = none */
	INT coloffset;										/* offset in column to support multiple sqlgetdata's */
	CHAR rsid[FIELD_LENGTH];
	CHAR cursor_name[MAX_CURSOR_NAME_LENGTH];
	DESCRIPTOR *apdsave;
	DESCRIPTOR *ardsave;
	ERRORS errors;
	/*
	 * Used only in SQLPrepare and SQLNumParams
	 */
	SQLUSMALLINT parameter_marker_count;
	/* statement attributes */
	DESCRIPTOR *sql_attr_app_param_desc;
	DESCRIPTOR *sql_attr_app_row_desc;
	SQLUINTEGER sql_attr_async_enable;
	SQLUINTEGER sql_attr_concurrency;
	SQLUINTEGER sql_attr_cursor_scrollable;
	SQLUINTEGER sql_attr_cursor_sensitivity;
	SQLUINTEGER sql_attr_cursor_type;
	SQLUINTEGER sql_attr_enable_auto_ipd;
	SQLPOINTER sql_attr_fetch_bookmark_ptr;
	DESCRIPTOR *sql_attr_imp_param_desc;
	DESCRIPTOR *sql_attr_imp_row_desc;
	SQLUINTEGER sql_attr_keyset_size;
	SQLUINTEGER sql_attr_max_length;
	SQLUINTEGER sql_attr_max_rows;
	SQLUINTEGER sql_attr_metadata_id;
	SQLUINTEGER sql_attr_noscan;
	SQLUINTEGER sql_attr_query_timeout;
	SQLUINTEGER sql_attr_retrieve_data;
	SQLUINTEGER	sql_attr_row_number;
	SQLUINTEGER sql_attr_simulate_cursor;
	SQLUINTEGER sql_attr_use_bookmarks;
};

struct descriptor_struct {
	union {
		struct app_desc_record_struct *appdrec;
		struct imp_param_desc_record_struct *ipdrec;
		struct imp_row_desc_record_struct *irdrec;
	} firstrecord;
	CONNECTION *cnct;								/* used for explicitly allocated descriptors */
	STATEMENT *stmt;								/* used by implementation row descriptor */
	DESCRIPTOR *nextexplicitdesc;		/* linked list of explicitly allocated descriptors */
	INT type;
	INT dataatexecrecnum;
	ERRORS errors;
	/* application descriptor header attributes */
	SQLSMALLINT sql_desc_alloc_type;
	SQLUINTEGER sql_desc_array_size;
	SQLUSMALLINT *sql_desc_array_status_ptr;
	SQLUINTEGER *sql_desc_bind_offset_ptr;
	SQLINTEGER sql_desc_bind_type;
	SQLUSMALLINT sql_desc_count;
	SQLUINTEGER *sql_desc_rows_processed_ptr;
};

typedef struct app_desc_record_struct {
	struct app_desc_record_struct *nextrecord;
	UCHAR initialized;
	UCHAR *dataatexecptr;
	INT dataatexectype;
	INT dataatexeclength;
	INT dataatexecsize;
	/* application descriptor record attributes */
	SQLSMALLINT sql_desc_concise_type;
	SQLPOINTER sql_desc_data_ptr;
	SQLSMALLINT sql_desc_datetime_interval_code;
	SQLINTEGER sql_desc_datetime_interval_precision;
	SQLLEN *sql_desc_indicator_ptr;
	SQLUINTEGER sql_desc_length;
	SQLINTEGER sql_desc_num_prec_radix;
	SQLLEN sql_desc_octet_length;
	SQLLEN *sql_desc_octet_length_ptr;
	SQLSMALLINT sql_desc_precision;
	SQLSMALLINT sql_desc_scale;
	SQLSMALLINT sql_desc_type;
} APP_DESC_RECORD;

typedef struct imp_param_desc_record_struct {
	struct imp_param_desc_record_struct *nextrecord;
	UCHAR initialized;
	/* implementation parameter descriptor record attributes */
	SQLINTEGER sql_desc_case_sensitive;
	SQLSMALLINT sql_desc_concise_type;
	SQLPOINTER sql_desc_data_ptr;
	SQLSMALLINT sql_desc_datetime_interval_code;
	SQLINTEGER sql_desc_datetime_interval_precision;
	SQLSMALLINT sql_desc_fixed_prec_scale;
	SQLUINTEGER sql_desc_length;
	SQLCHAR *sql_desc_local_type_name;
	SQLCHAR sql_desc_name[MAX_NAME_LENGTH + 1];
	SQLSMALLINT sql_desc_nullable;
	SQLINTEGER sql_desc_num_prec_radix;
	SQLLEN sql_desc_octet_length;
	SQLSMALLINT sql_desc_parameter_type;
	SQLSMALLINT sql_desc_precision;
	SQLSMALLINT sql_desc_rowver;
	SQLSMALLINT sql_desc_scale;
	SQLSMALLINT sql_desc_type;
	SQLCHAR *sql_desc_type_name;
	SQLSMALLINT sql_desc_unnamed;
	SQLSMALLINT sql_desc_unsigned;
} IMP_PARAM_DESC_RECORD;

typedef struct imp_row_desc_record_struct {
	struct imp_row_desc_record_struct *nextrecord;
	SQLSMALLINT default_type;								/* default type for catalog functions */
	INT rowbufoffset;										/* offset in row buffer for data, = -1 with undefined catalog column */ 
	INT fieldlength;										/* length of field in row buffer */ 
	/* implementation row descriptor record attributes */
	SQLINTEGER sql_desc_auto_unique_value;
	SQLCHAR sql_desc_base_column_name[MAX_NAME_LENGTH + 1];
	SQLCHAR sql_desc_base_table_name[MAX_NAME_LENGTH + 1];
	SQLINTEGER sql_desc_case_sensitive;
	SQLCHAR sql_desc_catalog_name[1];
	SQLSMALLINT sql_desc_concise_type;
	SQLSMALLINT sql_desc_datetime_interval_code;
	SQLINTEGER sql_desc_datetime_interval_precision;
	SQLINTEGER sql_desc_display_size;
	SQLSMALLINT sql_desc_fixed_prec_scale;
	SQLCHAR sql_desc_label[MAX_NAME_LENGTH + 1];
	SQLUINTEGER sql_desc_length;
	SQLCHAR sql_desc_literal_prefix[1 + 1];
	SQLCHAR sql_desc_literal_suffix[1 + 1];
	SQLCHAR *sql_desc_local_type_name;
	SQLCHAR sql_desc_name[MAX_NAME_LENGTH + 1];
	SQLSMALLINT sql_desc_nullable;
	SQLINTEGER sql_desc_num_prec_radix;
	SQLINTEGER sql_desc_octet_length;
	SQLSMALLINT sql_desc_precision;
	SQLSMALLINT sql_desc_rowver;
	SQLSMALLINT sql_desc_scale;
	SQLCHAR sql_desc_schema_name[1];
	SQLSMALLINT sql_desc_searchable;
	SQLCHAR sql_desc_table_name[MAX_NAME_LENGTH + 1];
	SQLSMALLINT sql_desc_type;
	SQLCHAR *sql_desc_type_name;
	SQLSMALLINT sql_desc_unnamed;
	SQLSMALLINT sql_desc_unsigned;
	SQLSMALLINT sql_desc_updatable;
} IMP_ROW_DESC_RECORD;

typedef struct connectinfo_struct {
	SQLCHAR *dsn;
	SQLCHAR *pwd;
	SQLCHAR *uid;
	SQLCHAR *database;
	SQLCHAR *server;
	SQLCHAR *serverport;
	SQLCHAR *localport;
	SQLCHAR *encryption;
	SQLCHAR *authentication;
	INT dsnlen;
	INT pwdlen;
	INT uidlen;
	INT databaselen;
	INT serverlen;
	INT serverportlen;
	INT localportlen;
	INT encryptionlen;
	INT authenticationlen;
} CONNECTINFO;

/* executed flags */
#define EXECUTE_PREPARED			0x0001
#define EXECUTE_NEEDDATA			0x0002
#define EXECUTE_FETCH				0x0004
#define EXECUTE_RSID				0x0010
#define EXECUTE_DYNAMIC				0x0020
#define EXECUTE_RESULTSET			0x0040
#define EXECUTE_EXECUTED			0x0080
#define EXECUTE_SQLCOLUMNS			0x0100
#define EXECUTE_SQLGETTYPEINFO		0x0200
#define EXECUTE_SQLPRIMARYKEYS		0x0400
#define EXECUTE_SQLSPECIALCOLUMNS	0x0800
#define EXECUTE_SQLSTATISTICS		0x1000
#define EXECUTE_SQLTABLES			0x2000

/* Error messages */
#define HY000POINTER "Buffer to small to fit a pointer "
#define HY007TEXT "Associated statement has not been executed"
#define HY014CNT "Limit on number of connections per environment exceeded"
#define HY014STMT "Limit on number of statement handles per connection exceeded"
#define HY014DESC "Limit on number of descriptor handles per connection exceeded"
#define HY016TEXT "Cannot modify an implementation row descriptor"
#define HY042TEXT "Invalid attribute value"
#define HY091TEXT "Invalid descriptor field identifier"
#define S01S02TEXT "Attribute value changed by driver(server)"
#define C01S02TEXT "Attribute value changed by driver(client)"

#define ERROR_22002 "Indicator variable required but not supplied"
#define ERROR_HY096 "Information type out of range"

#define TEXT_01004 "String data, right truncated"
#define TEXT_01S07 "Fractional truncation"
#define TEXT_07002 "COUNT field incorrect"
#define TEXT_07005 "Prepared statement not a cursor-specification"
#define TEXT_07006 "Restricted data type attribute violation"
#define TEXT_07009 "Invalid descriptor index"
#define TEXT_07S01 "Invalid use of default parameter"
#define TEXT_08001 "Client unable to establish connection"
/*** CODE: REMOVE 08001 DUPLICATES ***/
/*#define TEXT_08001_BIND "Client unable to establish connection, bind failed" */
/*#define TEXT_08001_CONNECT "Communication link failure, connect to server failed" */
/*#define TEXT_08001_HOSTNAME "Client unable to establish connection, unrecognized server name" */
/*#define TEXT_08001_INVALIDIP "Client unable to establish connection, invalid server ip address" */
/*#define TEXT_08001_INVALIDPORT "Client unable to establish connection, invalid server port number" */
#define TEXT_08001_MAXLEN "Client unable to establish connection, data too long" 
/*#define TEXT_08001_NOPORT "Client unable to establish connection, no ODBC DSN entry or port number not specified" */
/*#define TEXT_08001_NOSERVER "Client unable to establish connection, no ODBC DSN entry or server name not specified" */
/*#define TEXT_08001_SOCKET "Client unable to establish connection, socket failed" */
#define TEXT_08S01_DISCONNECT "Communication link failure, server disconnected"
#define TEXT_08S01_INVALIDLEN "Communication link failure, invalid data length"
#define TEXT_08S01_RC "Communication link failure, unknown return code from server"
#define TEXT_08S01_READ "Communication link failure, read failed"
#define TEXT_08S01_STARTUP "Communication link failure, WSAStartup failed"
#define TEXT_08S01_SYNC "Communication link failure, message synchronization error"
#define TEXT_08S01_WRITE "Communication link failure, write failed"
#define TEXT_22018 "Invalid character value for cast specification"
#define TEXT_24000 "Invalid cursor state"
#define TEXT_34000 "Invalid cursor name"
#define TEXT_3C000 "Duplicate cursor name"
#define TEXT_3D000 "Invalid catalog name"
#define TEXT_HY000 "General error"
#define TEXT_HY000_DRIVER "Operation not supported for DRIVER type connection"
#define TEXT_HY001 "Unable to allocate memory"
#define TEXT_HY001_CNCT "Unable to allocate memory for connection handle"
#define TEXT_HY001_STMT "Unable to allocate memory for statement handle"
#define TEXT_HY001_DESC "Unable to allocate memory for descriptor handle"
#define TEXT_HY003 "Invalid application buffer type"
#define TEXT_HY004 "Invalid SQL data type"
#define TEXT_HY009 "Invalid use of null pointer"
#define TEXT_HY010 "Function sequence error"
#define TEXT_HY019 "Non-character and non-binary data sent in pieces"
#define TEXT_HY020 "Attempt to concatenate a null value"
#define TEXT_HY021 "Inconsistent descriptor information"
#define TEXT_HY090 "Invalid string or buffer length"
#define TEXT_HY092 "Invalid attribute/option"
#define TEXT_HY105 "Invalid parameter type"
#define TEXT_HYC00 "Optional feature not implemented"
#define TEXT_HYT00 "Timeout expired"
#define TEXT_IM001 "Driver does not support this function"
#define TEXT_IM008 "Dialog failed"

/* definitions for client-server communication */

#define EMPTY		"        "

/* request message function ids */
#define CATALOG		"CATALOG "
#define HELLO		"HELLO   "
#define START		"START   "
#define CONNECT		"CONNECT "
#define DISCNCT		"DISCNCT "
#define GETATTR		"GETATTR "
#define SETATTR		"SETATTR "
#define EXECNONE	"EXECNONE"
#define EXECUTE		"EXECUTE "
#define GETRS		"GETRS   "
#define RSINFO		"RSINFO  "
#define GETROWN		"GETROWN "
#define GETROWP		"GETROWP "
#define GETROWF		"GETROWF "
#define GETROWL		"GETROWL "
#define GETROWA		"GETROWA "
#define GETROWR		"GETROWR "
#define GETROWCT	"GETROWCT"
#define PSUPDATE	"PSUPDATE"
#define PSDELETE	"PSDELETE"
#define DISCARD		"DISCARD "

/* return code values */
#define FAIL     "ERR"
#define OK       "OK      "
#define OKTRUNC  "OKTRUNC "
#define NONE     "NONE    "
#define SET      "SET     "
#define NODATA   "SQL20000"

/* communication and server return codes */
#define COMM_ERROR    -1  /* something went wrong in communication */
#define SERVER_FAIL   -2  /* server rc = FAIL  */
#define SERVER_OK      1  /* server rc = OK    */
#define SERVER_OKTRUNC 2  /* server rc = OKTRUNC */
#define SERVER_NONE    3  /* server rc = NONE  */
#define SERVER_SET     4	 /* server rc = SET   */
#define SERVER_NODATA  5  /* server rc = SQL20000 */
/* pointers to things in messages */
#define FIELD_LENGTH 8  /* length of predefinded fields in message */

#define REQ_MSID    0
#define REQ_CNID    FIELD_LENGTH * 1
#define REQ_DTID    FIELD_LENGTH * 2
#define REQ_FNID    FIELD_LENGTH * 3
#define REQ_MSGLEN  FIELD_LENGTH * 4
#define REQ_BUF     FIELD_LENGTH * 5

#define RTN_MSID    0
#define RTN_RTCD FIELD_LENGTH * 1
#define RTN_MSGLEN  FIELD_LENGTH * 2
#define RTN_BUF FIELD_LENGTH * 3

#define REQUEST_HEADER  REQ_BUF  /* # of bytes in the beginning of request */
#define RETURN_HEADER   RTN_BUF  /* # of bytes in the beginning of return */

/* function prototypes */

/* fsodbc.c */
extern void entersync(void);
extern void exitsync(void);

/* fsodesc.c */
extern LPVOID desc_get(DESCRIPTOR *desc, INT number, INT initflag);
extern SQLRETURN desc_unbind(DESCRIPTOR *desc, INT number);

/* fsoexec.c */
extern SQLRETURN exec_prepare(STATEMENT *stmt, SQLCHAR *text, SQLINTEGER length);
extern SQLRETURN exec_execute(STATEMENT *stmt);
extern SQLRETURN exec_columninfo(STATEMENT *stmt, ERRORS *errors);
extern SQLRETURN exec_resultset(STATEMENT *stmt, ERRORS *errors, CHAR *buffer, INT len, INT executetype);
extern SQLRETURN exec_sqltoc(STATEMENT *stmt, IMP_ROW_DESC_RECORD *drec, SQLSMALLINT column,
		CHAR *sqlbuffer, size_t sqllen, INT type, INT sqlprecision, INT sqlscale, SQLPOINTER cbuffer,
		SQLLEN cbuflen, SQLLEN *cdatalen, SQLLEN *cnullind, SQLUINTEGER coffset,
		SQLUINTEGER crownum, SQLUINTEGER crowsize);

/* fsoerr.c */
extern void error_init(ERRORS *errors);
extern void error_record(ERRORS *errors, char *sqlstate, char *text1, char *text2, int nativeerror);

/* fsomem.c */
extern LPVOID allocmem(INT size, INT zeroflag);
extern LPVOID reallocmem(LPVOID memory, INT size, INT zeroflag);
extern void freemem(LPVOID memory);

/* fsosrvr.c */
extern void server_cleanup(void);
extern SQLRETURN server_connect(CONNECTION *cnct, CONNECTINFO *connectinfo);
extern SQLRETURN server_disconnect(CONNECTION *cnct);
extern SQLRETURN server_communicate(CONNECTION *cnct, UCHAR *rsid, UCHAR *func, UCHAR *data, INT datalen, ERRORS *error, UCHAR *result, INT *resultlen);
