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

#define INC_CTYPE
#define INC_STDLIB
#define INC_STDIO
#define INC_STRING
#define INC_SQL
#define INC_SQLEXT
#include "includes.h"
#ifdef __MACOSX
typedef char CHAR;
#ifndef __SQLTYPES_H
typedef wchar_t WCHAR;
#endif
typedef unsigned int UINT;
typedef CHAR TCHAR;
#endif

#include "base.h"

#define INC_UTIL
#define INC_TARGET
#include "dbcsql.h"

static CHAR cmdbuf[CMDSIZE];
static CHAR cnctstr[CMDSIZE];
static CHAR cursorname[20];
static CHAR uid[50];
static CHAR passwd[50];
static CHAR server[50];
static CHAR string[250];
static CHAR errorstr[64];
static INT localerr;
static INT foundcursor, connectionFound;

static INT findConnection(CHAR* connectionname, INT insertnew);


/*
 * DBCSQLX
 * adr1 is the SQL command
 * adr2 is any 'from' parameters
 * adr3 is any 'into' parameters
 */
INT dbcsqlx(UCHAR *adr1, UCHAR **adr2, UCHAR **adr3)
{
	INT i1, retcode;
	INT cursornum, connectionnum = DEFAULTCONN;
	INT cmdlen;
	INT fcmdmatch, ckcount;
	CHAR *ptr1, *ptr2, *ptr3, *destptr, connectname[CONNAMELENGTH + 1];
	SQLLEN index;
	INT fetchtype;
	INT scroll;
	INT lock;
	INT optstart;
	INT optend;

	localerr = FALSE;
	errorstr[0] = '\0';

	/* convert command line to ascii string */
	if (dbctostr(adr1, cmdbuf, CMDSIZE) == RC_ERROR) {
		strcpy(errorstr, "[DB/C]: Could not build command string: 1");
		return RC_ERROR;
	}
	strncpy(cnctstr, cmdbuf, 50);
	ptr1 = strtok(cnctstr, " \t");
	if (ptr1 == NULL) {
		localerr = TRUE;
		strcpy(errorstr, "[DB/C]: Could not build command string: 2");
		return RC_ERROR;
	}
	for (cmdlen = 0; ptr1[cmdlen]; cmdlen++) ptr1[cmdlen] = (CHAR)toupper(ptr1[cmdlen]);

	/* replace parameters */
	if (buildsql(cmdbuf, (UCHAR **)adr2) < 0) {
		localerr = TRUE;
		strcpy(errorstr, "[DB/C]: Could not build command string: 3");
		return RC_ERROR;
	}

	/*
	 * Pull cursor name out of cmdbuf, maybe
	 */
	if (!strcmp(ptr1, "CREATE")) {
		foundcursor = 0;
		strcpy(cursorname, TABLE_CURSOR_NAME);
	}
	else foundcursor = getcursorname(cmdbuf, cursorname, sizeof(cursorname));
	cursornum = getcursornum(cursorname);
	if (cursornum == RC_ERROR) {
		localerr = TRUE;
		strcpy(errorstr, "[DB/C]: Exceeded maximum cursors");
		return RC_NO_MEM;
	}

	/* Branch on Command */
	switch (ptr1[0]) {
		case 'A':
			if (!strcmp(ptr1, "ALTER")) {
				if (getconnectionname(cmdbuf, connectname)) {
					connectionnum = findConnection(connectname, FALSE);
					if (connectionnum == RC_ERROR) {
						localerr = TRUE;
						strcpy(errorstr, "[DB/C]: Connection name never seen before");
						return RC_ERROR;
					}
				}
				else connectionnum = DEFAULTCONN;
				if (!connections[connectionnum].connected) {
					localerr = TRUE;
					strcpy(errorstr, "[DB/C]: Attempt to alter table when not connected");
					return RC_ERROR;
				}
				retcode = dbcexecdir(cmdbuf, -1, connectionnum);
				return(retcode);
			}
			else fcmdmatch = 0;
			break;
		case 'C':
			if (!strcmp(ptr1, "CONNECT")) {
				ckcount = 0;
				uid[0] = passwd[0] = server[0] = string[0] = connectname[0] = '\0';
				strcpy(cnctstr, cmdbuf);
				if (strstr(cnctstr, "=") == NULL) {     /* replace */
					for (index = 0; adr2[index] != 0; index++);
					if (index == 3) {
						retcode = dbctostr((UCHAR *)adr2[0], server, 50);
						if (!retcode) retcode = dbctostr((UCHAR *)adr2[1], uid, 50);
						if (!retcode) dbctostr((UCHAR *)adr2[2], passwd, 50);
					}
					else if (index == 2) {
						retcode = dbctostr((UCHAR *)adr2[0], uid, 50);
						if (!retcode) dbctostr((UCHAR *)adr2[1], passwd, 50);
					}
					else if (index == 1) dbctostr((UCHAR *)adr2[0], uid, 50);
				}
				else {  /* keywords */
					ptr1 += strcspn(ptr1, " \t");
					if (*ptr1) ptr1 += strspn(ptr1, " \t");  /* name, server, ... */
					while (*ptr1) {
						ptr2 = ptr1 + strcspn(ptr1, "=");
						if (*ptr2 == '\0') break;
						*ptr2 = '\0';
						ptr2 = ptr1;
						do *ptr2 = (CHAR) toupper(*ptr2);
						while (*ptr2++);  /* allowing toupper on \0 to save extra ptr2++ */
						i1 = 49;
						if (!strcmp(ptr1, "NAME")) destptr = uid;
						else if (!strcmp(ptr1, "PASSWORD")) destptr = passwd;
						else if (!strcmp(ptr1, "SERVER")) destptr = server;
						else if (!strcmp(ptr1, "CONNECTION")) {
							destptr = connectname;
							i1 = 20;
							ckcount++;
						}
						else if (!strcmp(ptr1, "STRING")) {
							destptr = string;
							i1 = 249;
						}
						else break;
						ptr1 = ptr2 + strcspn(ptr2, ", \t");   /* value */
						if (*ptr1 != '\0') *ptr1++ = '\0';
						strncpy(destptr, ptr2, i1);
						destptr[i1] = '\0';
						if (*ptr1 == '\0') break;
						ptr1 += strspn(ptr1, ", \t");
					}
				}
				if (ckcount > 1) {
					localerr = TRUE;
					strcpy(errorstr, "[DB/C] CONNECTION keyword appeared more than once");
					return  RC_ERROR;
				}
				ptr1 = cmdbuf + cmdlen;
				while (isspace(*ptr1)) ptr1++;
				if ((connectionnum = findConnection(connectname, TRUE)) == -1) {
					localerr = TRUE;
					sprintf(errorstr, "[DB/C] Exceeded maximum connections [%i]", MAXCONNS);
					return RC_ERROR;
				}
				retcode = dbcconnect(ptr1, uid, passwd, server, string, connectionnum);
				connections[connectionnum].connected = (retcode < 0) ? FALSE : TRUE;
				return(retcode);
			}
			else if (!strcmp(ptr1, "CLOSE")) {
				strcpy(cnctstr, cmdbuf);
				ptr1 = strtok(cnctstr, " \t");
				ptr2 = strtok(NULL, " \t");
				if (ptr1 != NULL && ptr2 != NULL) {
					cursornum = getcursornum(ptr2);
					if (cursornum < 0) return(0);
				}
				retcode = dbcclose(cursornum);
				return(retcode);
			}
			else if (!strcmp(ptr1, "COMMIT") || !strcmp(ptr1, "CREATE")) {
				if (getconnectionname(cmdbuf, connectname)) {
					connectionnum = findConnection(connectname, FALSE);
					if (connectionnum == -1) {
						localerr = TRUE;
						strcpy(errorstr, "[DB/C]: Connection name never seen before");
						return RC_ERROR;
					}
				}
				else connectionnum = DEFAULTCONN;
				if (!connections[connectionnum].connected) {
					localerr = TRUE;
					sprintf(errorstr, "[DB/C]: Attempt to %s when not connected", ptr1);
					return RC_ERROR;
				}
				retcode = dbcexecdir(cmdbuf, -1, connectionnum);
				return(retcode);
			}
			else fcmdmatch = 0;
			break;
		case 'D':
			if (!strcmp(ptr1, "DISCONNECT")) {
				//fcmdmatch = 1; Not used before the return
				if (getconnectionname(cmdbuf, connectname)) {
					connectionnum = findConnection(connectname, FALSE);
					if (connectionnum == -1) {
						localerr = TRUE;
						strcpy(errorstr, "[DB/C]: Connection name never seen before");
						return RC_ERROR;
					}
				}
				else connectionnum = DEFAULTCONN;
				if (!connections[connectionnum].connected) {
					localerr = TRUE;
					strcpy(errorstr, "[DB/C]: Attempt to disconnect when not connected");
					return RC_ERROR;
				}
				retcode = dbcdisconnect(connectionnum);
				connections[connectionnum].connected = FALSE;
				return(retcode);
			}
			else if (!strcmp(ptr1, "DELETE")) {
				//fcmdmatch = 1; Not used before the return
				connectionFound = getconnectionname(cmdbuf, connectname);
				if (connectionFound && foundcursor) {
					localerr = TRUE;
					strcpy(errorstr, "[DB/C]: You may not have both CONNECTION and USING clauses");
					return RC_ERROR;
				}
				if (foundcursor)
					connectionnum = cursor2connection[cursornum];
				else if (connectionFound) {
					connectionnum = findConnection(connectname, FALSE);
					if (connectionnum == -1) {
						localerr = TRUE;
						strcpy(errorstr, "[DB/C]: Connection name never seen before");
						return RC_ERROR;
					}
				}
				else connectionnum = DEFAULTCONN;

				if (!connections[connectionnum].connected) {
					localerr = TRUE;
					strcpy(errorstr, "[DB/C]: Attempt to delete when not connected");
					return RC_ERROR;
				}
				if (foundcursor) {
					strcat(cmdbuf, "OF ");
					strcat(cmdbuf, cursorname);
				}
				retcode = dbcupddel(cmdbuf, connectionnum);
				return(retcode);
			}
			else if (!strcmp(ptr1, "DROP")) {
				//fcmdmatch = 1; Not used before the return
				if (getconnectionname(cmdbuf, connectname)) {
					connectionnum = findConnection(connectname, FALSE);
					if (connectionnum == -1) {
						localerr = TRUE;
						strcpy(errorstr, "[DB/C]: Connection name never seen before");
						return RC_ERROR;
					}
				}
				else connectionnum = DEFAULTCONN;
				if (!connections[connectionnum].connected) {
					localerr = TRUE;
					strcpy(errorstr, "[DB/C]: Attempt to alter table when not connected");
					return RC_ERROR;
				}
				retcode = dbcexecdir(cmdbuf, -1, connectionnum);
				return(retcode);
			}
			else fcmdmatch = 0;
			break;
		case 'F':
			if (!strcmp(ptr1, "FETCH")) {
				//fcmdmatch = 1; Not used before the return
				connectionnum = cursor2connection[cursornum];
				if (!connections[connectionnum].connected) {
					localerr = TRUE;
					sprintf(errorstr, "[DB/C]: Attempt to fetch when not connected. (%d:%d)", connectionnum, cursornum);
					return RC_ERROR;
				}
				strcpy(cnctstr, cmdbuf);
				ptr1 = strtok(cnctstr, " \t");
				ptr2 = strtok(NULL, " \t");
				if (ptr1 == NULL || ptr2 == NULL) {
					index = 0;
					fetchtype = DBC_FET_NEXT;
				}
				else {
					ptr1 = ptr2;
					while (*ptr2) {
						*ptr2 = (CHAR)toupper(*ptr2);
						ptr2++;
					}
					if (!strcmp(ptr1, "FIRST")) {
						index = 0;
						fetchtype = DBC_FET_FIRST;
					}
					else if (!strcmp(ptr1, "NEXT")) {
						index = 0;
						fetchtype = DBC_FET_NEXT;
					}
					else if (!strcmp(ptr1, "LAST")) {
						index = 0;
						fetchtype = DBC_FET_LAST;
					}
					else if (!strcmp(ptr1, "RELATIVE")) {
						fetchtype = DBC_FET_REL;
						ptr1 = strtok(NULL, " \t");
						if (ptr1 == NULL) {
							localerr = TRUE;
							strcpy(errorstr, "[DB/C]: Unable to parse fetch command : 1");
							return RC_ERROR;
						}
						if (*ptr1 == '-') {
							ptr1++;
							index = -atol(ptr1);
						}
						else index = atoi(ptr1);
						if (index == 0 && *ptr1 != '0') {
							localerr = TRUE;
							strcpy(errorstr, "[DB/C]: Unable to parse fetch command : 2");
							return RC_ERROR;
						}
					}
					else if (!strcmp(ptr1, "ABSOLUTE") || !strcmp(ptr1, "RANDOM")) {
						fetchtype = DBC_FET_ABS;
						ptr1 = strtok(NULL, " \t");
						if (ptr1 == NULL) {
							localerr = TRUE;
							strcpy(errorstr, "[DB/C]: Unable to parse fetch command : 3");
							return RC_ERROR;
						}
						if (*ptr1 == '-') {
							ptr1++;
							index = -atol(ptr1);
						}
						else index = atol(ptr1);
						if (index == 0 && *ptr1 != '0') {
							localerr = TRUE;
							strcpy(errorstr, "[DB/C]: Unable to parse fetch command : 4");
							return RC_ERROR;
						}
					}
					else if (!strcmp(ptr1, "PRIOR") || !strcmp(ptr1, "PREV") || !strcmp(ptr1, "PREVIOUS")) {
						index = 0;
						fetchtype = DBC_FET_PREV;
					}
					else {
						localerr = TRUE;
						strcpy(errorstr, "[DB/C]: Unable to parse fetch command : 5");
						return RC_ERROR;
					}
				}
				retcode = dbcfetch(cursornum, fetchtype, index, adr3, connectionnum);
				return(retcode);
			}
			else fcmdmatch = 0;
			break;
		case 'R':
			if (!strcmp(ptr1, "ROLLBACK")) {
				//fcmdmatch = 1; Not used before the return
				if (getconnectionname(cmdbuf, connectname)) {
					connectionnum = findConnection(connectname, FALSE);
					if (connectionnum == -1) {
						localerr = TRUE;
						strcpy(errorstr, "[DB/C]: Connection name never seen before");
						return RC_ERROR;
					}
				}
				else connectionnum = DEFAULTCONN;
				if (!connections[connectionnum].connected) {
					localerr = TRUE;
					strcpy(errorstr, "[DB/C]: Attempt to rollback when not connected");
					return RC_ERROR;
				}
				retcode = dbcexecdir(cmdbuf, -1, connectionnum);
				return(retcode);
			}
			else fcmdmatch = 0;
			break;
		case 'S':
			if (!strcmp(ptr1, "SELECT")) {
				//fcmdmatch = 1; Not used before the return
				if (getconnectionname(cmdbuf, connectname)) {
					connectionnum = findConnection(connectname, FALSE);
					if (connectionnum == -1) {
						localerr = TRUE;
						strcpy(errorstr, "[DB/C]: Connection name never seen before");
						return RC_ERROR;
					}
				}
				else connectionnum = DEFAULTCONN;
				if (!connections[connectionnum].connected) {
					localerr = TRUE;
					strcpy(errorstr, "[DB/C]: Attempt to select when not connected");
					return RC_ERROR;
				}
				/* all scroll and lock options are nonzero */
				scroll = 0;
				lock = 0;
				/* First word after SELECT in cnctstr*/
				strcpy(cnctstr, cmdbuf);
				//ptr1 = strtok(cnctstr, " \t");
				ptr1 = strtok(NULL, " (\t");
				if (ptr1 == NULL) {
					localerr = TRUE;
					strcpy(errorstr, "[DB/C]: Unable to parse select statement : 1");
					return RC_ERROR;
				}
				ptr2 = ptr1;
				while (*ptr2) {
					*ptr2 = (CHAR)toupper(*ptr2);
					ptr2++;
				}
				if (!strcmp(ptr1, "OPTIONS")) {
					ptr3 = strstr(cmdbuf, ")");
					if (ptr3 == NULL) {
						localerr = TRUE;
						strcpy(errorstr, "[DB/C]: Unable to parse select statement : 2");
						return RC_ERROR;
					}
					optstart = (INT) (ptrdiff_t) (ptr1 - cnctstr);
					optend = (INT) (ptrdiff_t) (ptr3 - cmdbuf);
					ptr1 = strtok(NULL, " ,()=\t"); /* LOCK, SCROLL, ... */
					while (TRUE) {
						if (ptr1 == NULL) {
							localerr = TRUE;
							strcpy(errorstr, "[DB/C]: Unable to parse select statement : 3");
							return RC_ERROR;
						}
						ptr2 = ptr1;
						while (*ptr2) {
							*ptr2 = (CHAR)toupper(*ptr2);
							ptr2++;
						}
						if (!strcmp(ptr1, "LOCK")) {
							ptr1 = strtok(NULL, " =,)\t");   /* value */
							if (ptr1 == NULL) {
								localerr = TRUE;
								strcpy(errorstr, "[DB/C]: Unable to parse select statement : 4");
								return RC_ERROR;
							}
							ptr2 = ptr1;
							while (*ptr2) {
								*ptr2 = (CHAR)toupper(*ptr2);
								ptr2++;
							}
							if (!strcmp(ptr1, "READONLY")) lock = DBC_CON_RDONLY;
							else if (!strcmp(ptr1, "OPTBYTIME")) lock = DBC_CON_OPTTIME;
							else if (!strcmp(ptr1, "OPTBYVAL")) lock = DBC_CON_OPTVAL;
							else if (!strcmp(ptr1, "FETCH")) lock = DBC_CON_LOCK;
							else {
								localerr = TRUE;
								strcpy(errorstr, "[DB/C]: Illegal LOCK Option");
								return RC_ERROR;
							}
						}
						else if (!strcmp(ptr1, "SCROLL")) {
							ptr1 = strtok(NULL, " =,)\t");
							if (ptr1 == NULL) {
								localerr = TRUE;
								strcpy(errorstr, "[DB/C]: Unable to parse select statement : 5");
								return RC_ERROR;
							}
							ptr2 = ptr1;
							while (*ptr2) {
								*ptr2 = (CHAR)toupper(*ptr2);
								ptr2++;
							}
							if (!strcmp(ptr1, "FORWARD")) scroll = DBC_CUR_FOR;
							else if (!strcmp(ptr1, "DYNAMIC")) scroll = DBC_CUR_DYN;
							else if (!strcmp(ptr1, "KEYSET")) scroll = DBC_CUR_KEY;
							else {
								localerr = TRUE;
								strcpy(errorstr, "[DB/C]: Illegal SCROLL Option");
								return RC_ERROR;
							}
						}
						else break;

						if (lock && scroll) break;
						ptr1 = strtok(NULL, " ,=)\t");
					}
					strncpy(cnctstr, cmdbuf, optstart);
					cnctstr[optstart] = '\0';
					strcat(cnctstr, cmdbuf + optend + 1);
					retcode = dbcselect(cnctstr, cursornum, scroll, lock, connectionnum);
				}
				else retcode = dbcselect(cmdbuf, cursornum, 0, 0, connectionnum);
				cursor2connection[cursornum] = connectionnum;
				return(retcode);
			}
			else fcmdmatch = 0;
			break;
		case 'U':
			if (!strcmp(ptr1, "UPDATE")) {
				//fcmdmatch = 1; Not used before thr return
				connectionFound = getconnectionname(cmdbuf, connectname);
				if (connectionFound && foundcursor) {
					localerr = TRUE;
					strcpy(errorstr, "[DB/C]: You may not have both CONNECTION and USING clauses");
					return RC_ERROR;
				}
				if (foundcursor) {
					connectionnum = cursor2connection[cursornum];
					strcat(cmdbuf, "OF ");
					strcat(cmdbuf, cursorname);
				}
				else if (connectionFound) {
					connectionnum = findConnection(connectname, FALSE);
					if (connectionnum == -1) {
						localerr = TRUE;
						strcpy(errorstr, "[DB/C]: Connection name never seen before");
						return RC_ERROR;
					}
				}
				else connectionnum = DEFAULTCONN;

				if (!connections[connectionnum].connected) {
					localerr = TRUE;
					strcpy(errorstr, "[DB/C]: Attempt to update when not connected");
					return RC_ERROR;
				}
				retcode = dbcupddel(cmdbuf, connectionnum);
				return(retcode);
			}
			else fcmdmatch = 0;
			break;
		default:
			fcmdmatch = 0;
			break;
	}
	if (!fcmdmatch) {
		if (getconnectionname(cmdbuf, connectname)) {
			connectionnum = findConnection(connectname, FALSE);
			if (connectionnum == -1) {
				localerr = TRUE;
				strcpy(errorstr, "[DB/C]: Connection name never seen before");
				return RC_ERROR;
			}
		}
		else connectionnum = DEFAULTCONN;
		if (!connections[connectionnum].connected) {
			localerr = TRUE;
			strcpy(errorstr, "[DB/C]: Attempt to execute an SQL statement when not connected");
			return RC_ERROR;
		}
		if (!foundcursor) cursornum = -1;
		retcode = dbcexecdir(cmdbuf, cursornum, connectionnum);
		if (cursornum >= 0) cursor2connection[cursornum] = connectionnum; /* JPR 2/16/2001 */
	}
	return(retcode);
}

/* DBCSQLM */
CHAR *dbcsqlm(void)
{
	CHAR *retstr;

	if (localerr) {
		localerr = FALSE;
		return(errorstr);
	}
	retstr = dbcgeterr();
	strcpy(cmdbuf, retstr);
	return(cmdbuf);
}

/*
 * DBCSQLQ
 * Call dbcdisconnect for all open connections
 *
 */
void dbcsqlq(void)
{
	INT idx;
	for (idx = 0; idx < (INT)(sizeof(connections)/sizeof(CONNECTION)); idx++) {
		if (connections[idx].connected == TRUE) {
			dbcdisconnect(idx);
			connections[idx].connected = FALSE;
		}
	}
}

/*
 * FINDCONNECTION
 *
 * Takes a connection name and returns an index.
 * If the connection name is a zero length string, return the default connection number.
 * If the name is not in the table, return RC_ERROR unless insertnew is true.
 * If insertnew is true and the table is full, return RC_ERROR.
 */
static INT findConnection(CHAR* connectionname, INT insertnew) {
	INT idx;

	if (strlen(connectionname) == 0) return DEFAULTCONN;

	for (idx = 0; idx < MAXCONNS; idx++) {
		if (!strcmp(connections[idx].name, connectionname)) break;
	}
	if (idx == MAXCONNS) {
		/* Connection name not in table */
		if (insertnew) {
			for (idx = 0; idx < MAXCONNS; idx++) {
				if (!connections[idx].name[0]) break;
			}
			if (idx == MAXCONNS) return RC_ERROR;
			strcpy(connections[idx].name, connectionname);
		}
		else idx = RC_ERROR;
	}
	return(idx);
}
