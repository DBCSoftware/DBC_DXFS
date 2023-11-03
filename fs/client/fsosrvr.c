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
#define INC_IPV6
#define INC_SQLEXT
#include "includes.h"
#if OS_WIN32
#else
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#if defined(PTHREADS)
#include <pthread.h>
#include <sys/time.h>
#endif
#endif
#include <odbcinst.h>
#include "fsodbc.h"
#include "fsodbcx.h"
#include "tcp.h"
#include "base.h"

#if OS_UNIX
#define _itoa(a, b, c) mscitoa(a, b)
#define _strnicmp(a, b, c) strncasecmp(a, b, c)
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#define closesocket(a) close(a)
#endif

#define SERVERPORT 9584

typedef struct threadinfo_struct {
	struct threadinfo_struct *next;
	INT keepalivetime;
	INT slepttime;
	SOCKET sockethandle;
	INT tcpflags;
#if defined(PTHREADS)
	pthread_mutex_t critical;
#endif
} THREADINFO;

const char EMPTYSTR  []= "";

/* ODBC.INI keywords */
const char ODBC[]                = "ODBC";         /* ODBC application name */
const char ODBC_INI[]            = "ODBC.INI";     /* ODBC initialization file */
const char INI_KDATABASE[]       = "Database";     /* Name of database (.dbd file) */
const char INI_KDESCRIPTION[]    = "Description";  /* Data source description */
const char INI_KUID[]	         = "UID";          /* User Default ID */
const char INI_KPASSWORD[]       = "Password";     /* User Default Password */
const char INI_KSERVER[]         = "Server";       /* Server IP address */
const char INI_KSERVERPORT[]     = "ServerPort";   /* Server port number */
const char INI_KLOCALPORT[]      = "LocalPort";    /* Local port number */
const char INI_KENCRYPTION[]     = "Encryption";   /* Encryption */

/**
 * Head of a singly-linked list of threadinfo structures
 */
static THREADINFO *firstthreadinfo = NULL;
#if OS_WIN32
static HANDLE threadevent;
/**
 * Holds the HANDLE of the one-per-process keep alive thread
 */
static HANDLE threadhandle;
/**
 * Holds the ID of the one-per-process keep alive thread
 */
static DWORD threadid;
static LPTHREAD_START_ROUTINE keepaliveproc(LPVOID parm);
#else
#if defined(PTHREADS)
static INT threadeventcreated = FALSE;
static pthread_mutex_t globalmut = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t threadevent;
static pthread_t threadid = 0;
static void* keepaliveproc(void* parm);
#endif
#endif
static UCHAR threadshutdownflag; // @suppress("Unused variable declaration in file scope")

/**
 * This is called only by SQLFreeHandle and only when the type is SQL_HANDLE_ENV
 * and the environment count has gone to zero
 */
void server_cleanup(void)
{
	THREADINFO *threadinfo, *nextthreadinfo;
#if OS_WIN32
	// We are entered with entersync on.
	threadshutdownflag = TRUE;
	for (threadinfo = firstthreadinfo; threadinfo != NULL; threadinfo = threadinfo->next) {
		threadinfo->keepalivetime = 0;
	}
	if (threadevent != NULL) {
		SetEvent(threadevent);
	}
	if (threadhandle != NULL) {
		exitsync(); // deadlock possible if we don't do this
		WaitForSingleObject(threadhandle, INFINITE);
		entersync();
		threadhandle = NULL;
	}
	if (threadevent != NULL) {
		CloseHandle(threadevent);
		threadevent = NULL;
	}
	threadinfo = firstthreadinfo;
	while (threadinfo != NULL) {
		//DeleteCriticalSection(&threadinfo->critical);
		nextthreadinfo = threadinfo->next;
		freemem(threadinfo);
		threadinfo = nextthreadinfo;
	}
#else
#if defined(PTHREADS)
	pthread_mutex_lock(&globalmut);
	threadshutdownflag = TRUE;
	for (threadinfo = firstthreadinfo; threadinfo != NULL; threadinfo = threadinfo->next) {
		threadinfo->keepalivetime = 0;
	}
	if (threadeventcreated) {
		pthread_cond_signal(&threadevent);
	}
	pthread_mutex_unlock(&globalmut);
	if (threadid != 0) {
		pthread_join(threadid, NULL);
	}
	if (threadeventcreated) {
		pthread_cond_destroy(&threadevent);
		threadeventcreated = FALSE;
	}
	threadinfo = firstthreadinfo;
	while (threadinfo != NULL) {
		pthread_mutex_destroy(&threadinfo->critical);
		nextthreadinfo = threadinfo->next;
		freemem(threadinfo);
		threadinfo = nextthreadinfo;
	}
#endif	
#endif
	firstthreadinfo = NULL;
}

/**
 * Called only from SQLConnect and SQLDriverConnect in fsosql.
 * It is protected by entersync from there.
 */
SQLRETURN server_connect(struct client_connection_struct *cnct, CONNECTINFO *connectinfo)
{
	INT i1, encryptionflag, keepalive, localportnum, reslen, serverportnum;
	CHAR buf[2048], database[256], encryption[16], localport[32], result[2048], server[256],
		serverport[32], uid[64], password[64], *ptr;
	SOCKET mainsocket = INVALID_SOCKET, tempsocket = INVALID_SOCKET;
	CONNECTINFO connectwork;
	THREADINFO *threadinfo;

	if (!connectinfo->dsnlen && !connectinfo->databaselen) {
		connectinfo->dsn = (SQLCHAR*)"DSN=DEFAULT";
		connectinfo->dsn += 4;
		connectinfo->dsnlen = 7;
	}
	connectwork = *connectinfo;
	if (connectwork.dsnlen) {  /* get the registry values for the DSN */
		strncpy(buf, (const char *)connectwork.dsn, connectwork.dsnlen);
		buf[connectwork.dsnlen] = '\0';
		if (!connectwork.databaselen) {
			i1 = SQLGetPrivateProfileString(buf, INI_KDATABASE,
				EMPTYSTR, database, sizeof(database), ODBC_INI);
			if (i1 > 0) {
				connectwork.database = (SQLCHAR*)database;
				connectwork.databaselen = i1;
			}
		}
		if (!connectwork.databaselen) {
			connectwork.database = connectwork.dsn;
			connectwork.databaselen = connectwork.dsnlen;
		}
		if (!connectwork.uidlen) {
			i1 = SQLGetPrivateProfileString(buf, INI_KUID, EMPTYSTR, uid, sizeof(uid), ODBC_INI);
			if (i1 > 0) {
				connectwork.uid = (SQLCHAR*)uid;
				connectwork.uidlen = i1;
			}
		}
		if (!connectwork.pwdlen) {
			i1 = SQLGetPrivateProfileString(buf, INI_KPASSWORD, EMPTYSTR, password, sizeof(password), ODBC_INI);
			if (i1 > 0) {
				connectwork.pwd = (SQLCHAR*)password;
				connectwork.pwdlen = i1;
			}
		}
		if (!connectwork.serverlen) {
			i1 = SQLGetPrivateProfileString(buf, INI_KSERVER, EMPTYSTR, server, sizeof(server), ODBC_INI);
			while (i1 > 0 && server[i1 - 1] == ' ') i1--;
			if (i1 > 0) {
				server[i1] = '\0';
				connectwork.server = (SQLCHAR*)server;
				connectwork.serverlen = i1;
			}
		}
		if (!connectwork.serverportlen) {
			i1 = SQLGetPrivateProfileString(buf, INI_KSERVERPORT, EMPTYSTR, serverport, sizeof(serverport) - 1, ODBC_INI);
			if (i1 > 0) {
				connectwork.serverport = (SQLCHAR*)serverport;
				connectwork.serverportlen = i1;
			}
		}
		if (!connectwork.localportlen) {
			i1 = SQLGetPrivateProfileString(buf, INI_KLOCALPORT, EMPTYSTR, localport, sizeof(localport) - 1, ODBC_INI);
			if (i1 > 0) {
				connectwork.localport = (SQLCHAR*)localport;
				connectwork.localportlen = i1;
			}
		}
		if (!connectwork.encryptionlen) {
			i1 = SQLGetPrivateProfileString(buf, INI_KENCRYPTION, EMPTYSTR, encryption, sizeof(encryption) - 1, ODBC_INI);
			if (i1 > 0) {
				connectwork.encryption = (SQLCHAR*)encryption;
				connectwork.encryptionlen = i1;
			}
		}
	}
	if (!connectwork.uidlen) {
		connectinfo->uid = (SQLCHAR*)"UID=DEFAULTUSER";
		connectinfo->uid += 4;
		connectinfo->uidlen = 11;
		connectwork.uid = connectinfo->uid;
		connectwork.uidlen = connectinfo->uidlen;
	}
	if (!connectwork.pwdlen) {
		connectwork.pwd = (SQLCHAR*)"PASSWORD";
		connectwork.pwdlen = 8;
	}

	/* check that everything will fit into the data field buffer */
	/* 31 = keywords + quotes + blanks */
	if ((connectinfo->databaselen + connectinfo->uidlen + connectinfo->pwdlen + 31) > (INT)sizeof(buf)) {
		error_record(&cnct->errors, "08001", TEXT_08001_MAXLEN, NULL, 0);
		return SQL_ERROR;
	}
	if (tcpissslsupported() && connectwork.encryptionlen &&
		!_strnicmp((const char *)connectwork.encryption, "Yes", connectwork.encryptionlen)) encryptionflag = TRUE;
	else encryptionflag = FALSE;
	if (!connectwork.serverlen) {
		error_record(&cnct->errors, "08001", TEXT_08001, "server name not specified", 0);
		return SQL_ERROR;
	}
	if ((CHAR *) connectwork.server != server) {
		strncpy(server, (const char *)connectwork.server, connectwork.serverlen);
		server[connectwork.serverlen] = '\0';
	}
	if (connectwork.serverportlen) {
		i1 = tcpntoi((UCHAR *) connectwork.serverport, connectwork.serverportlen, &serverportnum);
		if (i1 == -1 || serverportnum <= 0) {
			error_record(&cnct->errors, "08001", TEXT_08001, "invalid server port number specified", 0);
			return SQL_ERROR;
		}
	}
	else if (encryptionflag) serverportnum = SERVERPORT + 1;
	else serverportnum = SERVERPORT;
	if (connectwork.localportlen) {
		i1 = tcpntoi((UCHAR *) connectwork.localport, connectwork.localportlen, &localportnum);
#ifndef _DEBUG
		if (i1 == -1 || localportnum < 0) {
#else
		if (i1 == -1) {
#endif
			error_record(&cnct->errors, "08001", TEXT_08001, "invalid local port number specified", 0);
			return SQL_ERROR;
		}
		if (localportnum == 0) localportnum = -1;  /* server supplies server port number */
	}
	else localportnum = 0;
	if (encryptionflag) {
		cnct->tcpflags = TCP_UTF8 | TCP_SSL;
	}
	else cnct->tcpflags = TCP_UTF8;
	cnct->threadinfo = NULL;
	memcpy(cnct->cnid, EMPTY, FIELD_LENGTH);
	if (localportnum >= -1) {  /* not manual startup of v3 dbcfsrun */
		/* establish socket connection to the server to get FS version number */
		mainsocket = tcpconnect(server, serverportnum, cnct->tcpflags | TCP_CLNT, NULL,
				cnct->sql_attr_login_timeout ? (int)cnct->sql_attr_login_timeout : -1);
		if (mainsocket == INVALID_SOCKET) {
			error_record(&cnct->errors, "08001", TEXT_08001, tcpgeterror(), 0);
			return SQL_ERROR;
		}
		reslen = sizeof(result);
		cnct->socket = mainsocket;
		i1 = server_communicate(cnct, (UCHAR*)EMPTY, (UCHAR*)HELLO,
			NULL, 0, &cnct->errors, (UCHAR*)result, &reslen);
		/* server closes socket after hello */ 
		if (cnct->tcpflags & TCP_SSL) tcpsslcomplete(mainsocket);
		closesocket(mainsocket);
		cnct->socket = mainsocket = INVALID_SOCKET;
		if (i1 != SERVER_OK) return SQL_ERROR;
		if (reslen < 9 || memcmp(result, "DB/C FS ", 8) || !isdigit(result[8])) {
			error_record(&cnct->errors, "08001", TEXT_08001, "invalid hello result", 0);
			return SQL_ERROR;
		}
		for (cnct->server_majorver = 0, i1 = 8; i1 < reslen && isdigit(result[i1]); i1++)
			cnct->server_majorver = cnct->server_majorver * 10 + result[i1] - '0';
		if (cnct->server_majorver < 3 || cnct->server_majorver > FS_MAJOR_VERSION) {
			sprintf(buf, "DB/C FS server must be version >=3 and <=%d", FS_MAJOR_VERSION);
			error_record(&cnct->errors, "08001", TEXT_08001, buf, 0);
			return SQL_ERROR;
		}
		for (cnct->server_minorver = 0, ++i1; i1 < reslen && isdigit(result[i1]); i1++)
			cnct->server_minorver = cnct->server_minorver * 10 + result[i1] - '0';
		for (cnct->server_subrelease = 0, ++i1; i1 < reslen && isdigit(result[i1]); i1++)
			cnct->server_subrelease = cnct->server_subrelease * 10 + result[i1] - '0';
	}

	if (cnct->server_majorver != 2 && localportnum != -1) {  /* set up listening socket */
		/* begin listening for connection from server */
		tempsocket = tcplisten((localportnum >= 0) ? localportnum : -localportnum, &localportnum);
		if (tempsocket == INVALID_SOCKET) {
			error_record(&cnct->errors, "08001", TEXT_08001, tcpgeterror(), 0);
			return SQL_ERROR;
		}
	}
	else if (cnct->server_majorver == 2) cnct->tcpflags = 0;
	else localportnum = 0;  /* change from flag to server value */
	if (localportnum >= 0) {  /* not manual startup of v3 dbcfsrun */
		/* re-connect and send connection request */
		mainsocket = tcpconnect(server, serverportnum, cnct->tcpflags | TCP_CLNT, NULL,
				cnct->sql_attr_login_timeout ? (int)cnct->sql_attr_login_timeout : -1);
		if (mainsocket == INVALID_SOCKET) {
			error_record(&cnct->errors, "08001", TEXT_08001, tcpgeterror(), 0);
			return SQL_ERROR;
		}

		if (cnct->server_majorver == 2) {
		/* pack connection data buffer */
			memcpy(buf, "NAME=", 5);
			i1 = 5;
			memcpy(buf + i1, connectwork.uid, connectwork.uidlen);
			i1 += connectwork.uidlen;

			memcpy(buf + i1, " PASSWORD=", 10);
			i1 += 10;
			memcpy(buf + i1, connectwork.pwd, connectwork.pwdlen);
			i1 += connectwork.pwdlen;

			memcpy(buf + i1, " DATABASE=", 10);
			i1 += 10;
			memcpy(buf + i1, connectwork.database, connectwork.databaselen);
			i1 += connectwork.databaselen;
			ptr = CONNECT;
		}
		else {  /* start connection */
			_itoa(localportnum, buf, 10);
			i1 = (INT)strlen(buf);
			buf[i1++] = ' ';
			i1 += tcpquotedcopy((unsigned char *)(buf + i1), connectwork.uid, connectwork.uidlen);
			ptr = START;
		}

		/* request connection or start */
		reslen = sizeof(result);
		cnct->socket = mainsocket;
		i1 = server_communicate(cnct, (UCHAR*)EMPTY, (UCHAR*)ptr,
				(UCHAR*)buf, i1, &cnct->errors, (UCHAR*)result, &reslen);
		if (cnct->tcpflags & TCP_SSL) tcpsslcomplete(mainsocket);
		closesocket(mainsocket);
		cnct->socket = mainsocket = INVALID_SOCKET;
		if (i1 != SERVER_OK) {
			if (cnct->server_majorver != 2 && localportnum != 0) {
				closesocket(tempsocket);
			}
			return SQL_ERROR;
		}
	}
	if (cnct->server_majorver == 2) {
		memcpy(cnct->cnid, result, FIELD_LENGTH);
		/* get new port number to reconnect to */
		for (i1 = FIELD_LENGTH; i1 < reslen && result[i1] != ':'; i1++);
		for (serverportnum = 0; i1 < reslen; i1++)
			if (isdigit(result[i1])) serverportnum = serverportnum * 10 + result[i1] - '0';
		if (!serverportnum) {
			error_record(&cnct->errors, "08S01", TEXT_08S01_INVALIDLEN, NULL, 0);
			return(SQL_ERROR);
		}

		/* re-establish socket connection to the returned port number */
		mainsocket = tcpconnect(server, serverportnum, cnct->tcpflags | TCP_CLNT, NULL,
				cnct->sql_attr_login_timeout ? (int)cnct->sql_attr_login_timeout : -1);
		if (mainsocket == INVALID_SOCKET) {
			error_record(&cnct->errors, "08001", TEXT_08001, tcpgeterror(), 0);
			return SQL_ERROR;
		}
		cnct->socket = mainsocket;
	}
	else {  /* version 3 server, wait for connection back from server */
		if (localportnum != 0) {  /* normal connection */
			mainsocket = tcpaccept(tempsocket, cnct->tcpflags | TCP_CLNT, NULL,
					cnct->sql_attr_login_timeout ? (int)cnct->sql_attr_login_timeout : -1);
			if (mainsocket == INVALID_SOCKET) {
				error_record(&cnct->errors, "08001", TEXT_08001, tcpgeterror(), 0);
				return SQL_ERROR;
			}
		}
		else {  /* s-port connection */
			for (serverportnum = 0, i1 = 0; i1 < reslen; i1++)
				if (isdigit(result[i1])) serverportnum = serverportnum * 10 + result[i1] - '0';
			if (!serverportnum) {
				error_record(&cnct->errors, "08S01", TEXT_08S01_INVALIDLEN, NULL, 0);
				return(SQL_ERROR);
			}

			/* re-establish socket connection to the returned port number */
			mainsocket = tcpconnect(server, serverportnum, cnct->tcpflags | TCP_CLNT, NULL,
					cnct->sql_attr_login_timeout ? (int)cnct->sql_attr_login_timeout : -1);
			if (mainsocket == INVALID_SOCKET) {
				error_record(&cnct->errors, "08001", TEXT_08001, tcpgeterror(), 0);
				return SQL_ERROR;
			}
		}
	
		/* pack connection data buffer */
		i1 = tcpquotedcopy((unsigned char *)buf, (unsigned char *)connectwork.uid, connectwork.uidlen);

		buf[i1++] = ' ';
		i1 += tcpquotedcopy((unsigned char *)(buf + i1), (unsigned char *)connectwork.pwd, connectwork.pwdlen);

		buf[i1++] = ' ';
		memcpy(buf + i1, "DATABASE", 8);
		i1 += 8;

		buf[i1++] = ' ';
		i1 += tcpquotedcopy((unsigned char *)(buf + i1), (unsigned char *)connectwork.database, connectwork.databaselen);

		buf[i1++] = ' ';
		memcpy(buf + i1, "CLIENT", 6);
		i1 += 6;

		/* request connection */
/*** CODE: NEED TO PUT TIMEOUT ON SERVER_COMMUNICATE ***/
		reslen = sizeof(result);
		cnct->socket = mainsocket;
		i1 = server_communicate(cnct, (UCHAR*)EMPTY, (UCHAR*)CONNECT, (UCHAR*)buf, i1, &cnct->errors, (UCHAR*)result, &reslen);
		if (i1 != SERVER_OK) {
			if (cnct->tcpflags & TCP_SSL) tcpsslcomplete(mainsocket);
			closesocket(mainsocket);
			cnct->socket = mainsocket = INVALID_SOCKET;
			return SQL_ERROR;
		}
		memcpy(cnct->cnid, result, FIELD_LENGTH);
		if (reslen >= FIELD_LENGTH + FIELD_LENGTH) {  /* check if keepalive is supported */
			tcpntoi((unsigned char *)(result + FIELD_LENGTH), FIELD_LENGTH, &keepalive);
			if (keepalive) {  /* start up thread to send keepalive acknowledgements */
#if OS_WIN32
				if (threadevent == NULL) {
					threadevent = CreateEvent(NULL, TRUE, FALSE, NULL);
					if (threadevent == NULL) {
						error_record(&cnct->errors, "08001", TEXT_08001, "CreateEvent failed", GetLastError());
						server_communicate(cnct, EMPTY, DISCNCT, NULL, 0, NULL, NULL, NULL);
						if (cnct->tcpflags & TCP_SSL) tcpsslcomplete(mainsocket);
						closesocket(mainsocket);
						cnct->socket = mainsocket = INVALID_SOCKET;
						return SQL_ERROR;
					}
				}
#else
#if defined(PTHREADS)
				if (!threadeventcreated) {
					threadeventcreated = TRUE;	
					if (pthread_cond_init(&threadevent, NULL) != 0) {
						threadeventcreated = FALSE;
						error_record(&cnct->errors, "08001", TEXT_08001, "pthread_cond_init failed", errno);
						server_communicate(cnct, (UCHAR*)EMPTY, (UCHAR*)DISCNCT, NULL, 0, NULL, NULL, NULL);
						if (cnct->tcpflags & TCP_SSL) tcpsslcomplete(mainsocket);
						closesocket(mainsocket);
						cnct->socket = mainsocket = INVALID_SOCKET;
						return SQL_ERROR;
					}
				}
#endif
#endif
#if OS_WIN32
				if (threadhandle == NULL) {
					threadshutdownflag = FALSE;
					threadhandle = CreateThread(NULL, // no thread attributes
							0, // default stack size
							keepaliveproc, NULL,
							0, // start thread running immediately
							&threadid);
					if (threadhandle == NULL) {
						error_record(&cnct->errors, "08001", TEXT_08001, "CreateThread failed", GetLastError());					
						server_communicate(cnct, EMPTY, DISCNCT, NULL, 0, NULL, NULL, NULL);
						if (cnct->tcpflags & TCP_SSL) tcpsslcomplete(mainsocket);
						closesocket(mainsocket);
						cnct->socket = mainsocket = INVALID_SOCKET;
						return SQL_ERROR;
					}
				}
#endif
				for (threadinfo = firstthreadinfo;
						threadinfo != NULL && threadinfo->keepalivetime; threadinfo = threadinfo->next);
				if (threadinfo == NULL) {
					threadinfo = (THREADINFO *) allocmem(sizeof(THREADINFO), 1);
					if (threadinfo == NULL) {
						error_record(&cnct->errors, "HY001", TEXT_HY001, NULL, 0);
						server_communicate(cnct, (UCHAR*)EMPTY, (UCHAR*)DISCNCT, NULL, 0, NULL, NULL, NULL);
						if (cnct->tcpflags & TCP_SSL) tcpsslcomplete(mainsocket);
						closesocket(mainsocket);
						cnct->socket = mainsocket = INVALID_SOCKET;
						return SQL_ERROR;
					}
#if OS_WIN32
					/**
					 * This needs to be initialized here because the moment
					 * this structure is inserted into the list, the keepalivethread
					 * may try and use it. Even if the keepalivetime is still zero.
					 */
					//InitializeCriticalSection(&threadinfo->critical);
#else
#if defined(PTHREADS)
					pthread_mutex_init(&threadinfo->critical, NULL);
#endif
#endif
					/* since keepalivetime is 0 from allocmem, go ahead and insert at the head of the list */
					threadinfo->next = firstthreadinfo;
					firstthreadinfo = threadinfo;
				}
				threadinfo->sockethandle = mainsocket;
				threadinfo->tcpflags = cnct->tcpflags;
				threadinfo->slepttime = 0;
				threadinfo->keepalivetime = keepalive;  /* do last, otherwise use critical section */
#if OS_WIN32
				SetEvent(threadevent);  /* wake up thread */
#else
#if defined(PTHREADS)
				if (threadid == 0) {
					threadshutdownflag = FALSE;
					if (pthread_create(&threadid, NULL, keepaliveproc, NULL) != 0) {
						error_record(&cnct->errors, "08001", TEXT_08001, "pthread_create failed", errno);
						server_communicate(cnct, (UCHAR*)EMPTY, (UCHAR*)DISCNCT, NULL, 0, NULL, NULL, NULL);
						if (cnct->tcpflags & TCP_SSL) tcpsslcomplete(mainsocket);
						closesocket(mainsocket);
						cnct->socket = mainsocket = INVALID_SOCKET;
						return SQL_ERROR;
					}
				}
#endif
#endif
				cnct->threadinfo = threadinfo;
			}
		}
	}
/*** CODE: SET KEEPALIVE? (PROBABLY NOT NEEDED AS CLIENT INITIATES COMMUNICATIONS WITH A SEND) ***/
	return SQL_SUCCESS;
}

/**
 * This is called only from SQLDisconnect in fsosql. It is protected from there by entersync
 */
SQLRETURN server_disconnect(struct client_connection_struct *cnct)
{
	INT rc;
	THREADINFO *threadinfo;
	if (cnct->threadinfo != NULL) {  /* disable keepalive */
		threadinfo = (THREADINFO *) cnct->threadinfo;
//#if OS_WIN32
//		//EnterCriticalSection(&threadinfo->critical);
//		entersync();
//#else
//#if defined(PTHREADS)
//		pthread_mutex_lock(&threadinfo->critical);
//#endif
//#endif
		threadinfo->keepalivetime = 0;  /* exclusive access means keepalives are immediately stopped */
//#if OS_WIN32
//		//LeaveCriticalSection(&threadinfo->critical);
//		exitsync();
//#else
//#if defined(PTHREADS)
//		pthread_mutex_unlock(&threadinfo->critical);
//#endif
//#endif
	}
		
/*** CODE: MAKE SURE THAT IF SERVER IS NOT GOING TO EXIT, THAT IT CLEANS UP THE RESULT SETS ***/
	if (server_communicate(cnct, (UCHAR*)EMPTY, (UCHAR*)DISCNCT,
		NULL, 0, &cnct->errors, NULL, NULL) == SERVER_OK) rc = SQL_SUCCESS;
	else rc = SQL_ERROR;
	if (cnct->tcpflags & TCP_SSL) tcpsslcomplete(cnct->socket);
	shutdown(cnct->socket,
#if OS_WIN32
			SD_BOTH
#elif defined(Linux) || defined(FREEBSD)
			SHUT_RDWR
#else
			2
#endif
	);
	closesocket(cnct->socket);
	cnct->socket = INVALID_SOCKET;
	cnct->threadinfo = NULL;
	return rc;
}

/**
 * Should be thread protected by caller
 * Calls from exec_execute are protected
 * Calls from exec_columninfo are protected
 * As of 22MAY2020 all calls from SQL... functions are protected
 */
SQLRETURN server_communicate(struct client_connection_struct *cnct, UCHAR *rsid, UCHAR *func,
	UCHAR *data, INT datalen, ERRORS *error, UCHAR *result, INT *resultlen)
{
	static INT serialid = 0;
	INT i1, flags, len, rc, recvlen, recvpos, reslen;
	CHAR state[SQL_SQLSTATE_SIZE + 1];
	UCHAR buffer[REQUEST_HEADER + 512 + 1], *ptr;  
	SOCKET sockethandle;

	sockethandle = cnct->socket;
	if (!sockethandle) {
		error_record(error, "HY000", TEXT_HY000_DRIVER, NULL, 0);
		return SQL_ERROR;
	}
	flags = cnct->tcpflags;

	/* pack message to server */
	if (data == NULL || datalen < 0) datalen = 0;
	if (++serialid >= 100000000) serialid = 1;
	tcpiton(serialid, buffer + REQ_MSID, FIELD_LENGTH);
	memcpy(buffer + REQ_CNID, cnct->cnid, FIELD_LENGTH);
	memcpy(buffer + REQ_DTID, rsid, FIELD_LENGTH);
	memcpy(buffer + REQ_FNID, func, FIELD_LENGTH);
	tcpiton(datalen, buffer + REQ_MSGLEN, FIELD_LENGTH);
	if (datalen <= (INT)(sizeof(buffer) - REQUEST_HEADER)) {
		if (datalen) memcpy(buffer + REQUEST_HEADER, data, datalen);
		len = REQUEST_HEADER + datalen;
		datalen = 0;
	}
	else len = REQUEST_HEADER;

	/* send */
	if (tcpsend(sockethandle, buffer, len, flags, 30) != len) {
		if (error != NULL) error_record(error, "08S01", TEXT_08S01_WRITE, tcpgeterror(), 0);
		return COMM_ERROR;
	}
	if (datalen && tcpsend(sockethandle, data, datalen, flags, 30) != datalen) {
		if (error != NULL) error_record(error, "08S01", TEXT_08S01_WRITE, tcpgeterror(), 1);
		return COMM_ERROR;
	}

	for (rc = 0, recvpos = 0, recvlen = sizeof(buffer) - FIELD_LENGTH - 1, ptr = buffer + FIELD_LENGTH; ; ) {
/*** CODE: MAYBE SET TIMEOUT AFTER FIRST CHARACTER RECIEVED ***/
		i1 = tcprecv(sockethandle, ptr + recvpos, recvlen - recvpos, flags, -1);
		if (i1 <= 0) {  /* assume disconnect or timeout */
/*** CODE: if (i1 == 0), THEN TIMEOUT ***/
			if (error != NULL) error_record(error, "08S01", TEXT_08S01_DISCONNECT, tcpgeterror(), 0);
			return COMM_ERROR;
		}
		recvpos += i1;
		if (!rc) {  /* receiving the header */
			if (recvpos < RETURN_HEADER) continue;  /* not complete */
			/* message synchronization id */
			if (memcmp(buffer, ptr, FIELD_LENGTH)) {
				rc = COMM_ERROR;
				if (error != NULL) error_record(error, "08S01", TEXT_08S01_SYNC, NULL, 0);
				break;
			}
			tcpntoi(ptr + RTN_MSGLEN, FIELD_LENGTH, &len);
			ptr += RTN_RTCD;
			if (!memcmp(ptr, OK, FIELD_LENGTH)) rc = SERVER_OK;
/*** CODE: ONLY CHANGED FSSTMT.C TO LOOK FOR OKTRUNC, BUT THIS SHOULD BE THE ONLY PLACE IT CAN HAPPEN ***/
			else if (!memcmp(ptr, OKTRUNC, FIELD_LENGTH)) rc = SERVER_OKTRUNC;
			else if (!memcmp(ptr, NONE, FIELD_LENGTH)) rc = SERVER_NONE;
			else if (!memcmp(ptr, SET, FIELD_LENGTH)) rc = SERVER_SET;
			else if (!memcmp(ptr, NODATA, FIELD_LENGTH)) rc = SERVER_NODATA;
			else if (!memcmp(ptr, FAIL, 3)) rc = SERVER_FAIL;
			else {
				rc = COMM_ERROR;
				if (error != NULL) error_record(error, "08S01", TEXT_08S01_RC, NULL, 0);
				break;
			}
			recvpos -= RETURN_HEADER;
			if (resultlen != NULL) {
				reslen = *resultlen;
				*resultlen = len;
			}
			else reslen = len;
			if (result != NULL && (rc == SERVER_OK || rc == SERVER_OKTRUNC || rc == SERVER_NONE || rc == SERVER_SET)) {
				recvlen = min(reslen, len);
				if (recvpos) memcpy(result, ptr + RETURN_HEADER - RTN_RTCD, min(recvpos, recvlen));
				ptr = result;
			}
			else if (rc == SERVER_FAIL) {
				memcpy(state, ptr + 3, 5);
				state[5] = '\0';
				if (recvpos) memmove(buffer, ptr + RETURN_HEADER - RTN_RTCD, recvpos);
				recvlen = sizeof(buffer) - 1;
				ptr = buffer;
			}
			else {
				len -= recvpos;
				recvpos = 0;
				recvlen = sizeof(buffer);
				ptr = buffer;
			}
		}
		/* receiving the data */
		if (recvpos < recvlen && recvpos < len) continue;  /* not complete */
		if (rc == SERVER_FAIL && state[0]) {
			buffer[recvpos] = '\0';
			if (error != NULL) error_record(error, state, (char*)buffer, NULL, 0);
			state[0] = '\0';
		}
		len -= recvpos;
		if (len <= 0) break;
		recvpos = 0;
		recvlen = sizeof(buffer);
		ptr = buffer;
	}
	if (rc == COMM_ERROR) {  /* invalid format, empty the socket buffer */
		do i1 = tcprecv(sockethandle, buffer, sizeof(buffer), flags & ~TCP_UTF8, 3);
		while (i1 > 0);
	}
	return(rc);
}

/**
 * There is only one thread running keepaliveproc
 */
#if OS_WIN32
static LPTHREAD_START_ROUTINE keepaliveproc(LPVOID parm)
{
	INT sleeptime, waittime;
	THREADINFO *threadinfo;
	sleeptime = 0;
	while (!threadshutdownflag) {
		waittime = -1;
		entersync(); // traversing the threadinfo linked list needs protection
		for (threadinfo = firstthreadinfo; threadinfo != NULL; threadinfo = threadinfo->next) {
			if (threadinfo->keepalivetime) {
				threadinfo->slepttime += sleeptime;
				if (threadinfo->slepttime >= threadinfo->keepalivetime) {
					threadinfo->slepttime = 0;
					tcpsend(threadinfo->sockethandle, (UCHAR *) "ALIVEACK", 8, threadinfo->tcpflags, 20);
				}
				if (waittime == -1 || threadinfo->keepalivetime - threadinfo->slepttime < waittime)
					waittime = threadinfo->keepalivetime - threadinfo->slepttime;
			}
		}
		if (waittime == -1) sleeptime = 0;
		else sleeptime = waittime;
		exitsync();
		WaitForSingleObject(threadevent, (waittime == -1) ? (DWORD)INFINITE : (DWORD)(waittime * 1000));
		ResetEvent(threadevent);
	}
	return 0;
}
#endif

#if OS_UNIX && defined(PTHREADS)
static void* keepaliveproc(void* parm)
{
	INT sleeptime, waittime;
	THREADINFO *threadinfo;
	struct timeval now;
	struct timespec timeout;
	sleeptime = 0;
	while (!threadshutdownflag) {
		waittime = -1;
		for (threadinfo = firstthreadinfo; threadinfo != NULL; threadinfo = threadinfo->next) {
			pthread_mutex_lock(&threadinfo->critical);
			if (threadinfo->keepalivetime) {
				threadinfo->slepttime += sleeptime;
				if (threadinfo->slepttime >= threadinfo->keepalivetime) {
					threadinfo->slepttime = 0;
					tcpsend(threadinfo->sockethandle, (UCHAR *) "ALIVEACK", 8, threadinfo->tcpflags, 20);
				}
				if (waittime == -1 || threadinfo->keepalivetime - threadinfo->slepttime < waittime) waittime = threadinfo->keepalivetime - threadinfo->slepttime;
			}
			pthread_mutex_unlock(&threadinfo->critical);
		}
		if (waittime == -1) sleeptime = 0;
		else sleeptime = waittime;
		if (waittime == -1) {
			pthread_mutex_lock(&globalmut);
			pthread_cond_wait(&threadevent, &threadinfo->critical); 
			pthread_mutex_unlock(&globalmut);
		}
		else {
			gettimeofday(&now, NULL);
			timeout.tv_sec = now.tv_sec + waittime;
			timeout.tv_nsec = now.tv_usec * 1000;
			pthread_mutex_lock(&globalmut);
			pthread_cond_timedwait(&threadevent, &globalmut, &timeout);
			pthread_mutex_unlock(&globalmut);
		}
	}
	return 0;
}
#endif
