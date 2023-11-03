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
#define INC_STRING
#define INC_STDLIB
#define INC_CTYPE
#include "includes.h"
#include "base.h"
#include "dbc.h"
#include "fio.h"
#include "fsfileio.h"

#define FLAG_SERVER		0x01
#define FLAG_PRIMARY	0x02
#define FLAG_PREFIX		0x04

typedef struct {
	char fsname[128];
	int fshandle;
	char error[256];
} FILESERVERSTRUCT;

typedef struct {
	char fsname[128];
	char server[256];
	int serverport;
	int localport;
	int encryptionflag;
	char authfile[256];
	char database[MAX_NAMESIZE];
	char user[64];
	char password[64];
} SERVERINFOSTRUCT;

static int fileserverflag;
static int fileservercnt;
static FILESERVERSTRUCT **fileserver;
static char srverrstr[256];


int srvinit()
{
	INT i1, i2, i3;
	CHAR *ptr, *name;
	SERVERINFOSTRUCT **serverinfo;

	if (!fileservercnt) {  /* perform initializations */
		if (!prpget("file", "server", NULL, NULL, &ptr, PRP_GETCHILD) && !prpname(&name)) {
			do {
				for (i1 = 0; i1 < fileservercnt; i1++) {
					if (!strcmp((*serverinfo)[i1].fsname, name)) break;
				}
				if (i1 == fileservercnt) {
					/* no match, not defined yet */
					if (!fileservercnt++)
						i3 = ((serverinfo = (SERVERINFOSTRUCT **) memalloc(sizeof(SERVERINFOSTRUCT), MEMFLAGS_ZEROFILL)) != NULL) ? 0 : -1;
					else i3 = memchange((unsigned char **) serverinfo, fileservercnt * sizeof(SERVERINFOSTRUCT), MEMFLAGS_ZEROFILL);
					if (i3 == -1) dbcerror(1630 - ERR_NOMEM);
					strcpy((*serverinfo)[i1].fsname, name);
				}
				if (!prpget("file", "server", name, "user", &ptr, 0)) strcpy((*serverinfo)[i1].user, ptr);
				if (!prpget("file", "server", name, "password", &ptr, 0)) strcpy((*serverinfo)[i1].password, ptr);
				if (!prpget("file", "server", name, "serverhost", &ptr, 0)) strcpy((*serverinfo)[i1].server, ptr);
				if (!prpget("file", "server", name, "database", &ptr, 0)) strcpy((*serverinfo)[i1].database, ptr);			
				if (!prpget("file", "server", name, "localport", &ptr, 0)) {
					for (i2 = i3 = 0; isdigit(ptr[i2]); i2++) i3 = i3 * 10 + ptr[i2] - '0';
					if (i3 >= 0) (*serverinfo)[i1].localport = (i3 == 0) ? -1 : i3;
				}
				if (!prpget("file", "server", name, "serverport", &ptr, 0)) {
					for (i2 = i3 = 0; isdigit(ptr[i2]); i2++) i3 = i3 * 10 + ptr[i2] - '0';
					(*serverinfo)[i1].serverport = i3;
				}
				if (!prpget("file", "server", name, "encryption", &ptr, PRP_LOWER)) {
					(*serverinfo)[i1].encryptionflag = (strcmp("on", ptr) == 0);
				}
				if (prpget("file", "server", name, NULL, &ptr, 0)) break; /* reset position */
				if (prpget("file", "server", name, NULL, &ptr, PRP_NEXT)) break;
				if (prpname(&name)) continue;
			} while (TRUE);
		}
		if (fileservercnt) {
			fileserverflag |= FLAG_SERVER;
			fileserver = (FILESERVERSTRUCT **) memalloc(fileservercnt * sizeof(FILESERVERSTRUCT), 0);
			if (fileserver == NULL) dbcerror(1630 - ERR_NOMEM);
			for (i1 = 0; i1 < fileservercnt; i1++) {
				strcpy((*fileserver)[i1].fsname, (*serverinfo)[i1].fsname);
				if (*(*serverinfo)[i1].server && *(*serverinfo)[i1].database) {
					i2 = fsconnect((*serverinfo)[i1].server, (*serverinfo)[i1].serverport, (*serverinfo)[i1].localport,
						(*serverinfo)[i1].encryptionflag, (*serverinfo)[i1].authfile, (*serverinfo)[i1].database,
						(*serverinfo)[i1].user, (*serverinfo)[i1].password);
					if (i2 < 0) {
						i2 = -1;
						fsgeterror((*fileserver)[i1].error, sizeof((*fileserver)[i1].error));
					}
				}
				else {
					i2 = -1;
					strcpy((*fileserver)[i1].error, "missing server and/or database name");
				}
				(*fileserver)[i1].fshandle = i2;
/*** NOTE: COULD USE GREETINGS TO GET VERSION NUMBER ***/
			}
			memfree((unsigned char **) serverinfo);
			if (!prpget("file", "primary", "server", NULL, &ptr, 0) && *ptr) fileserverflag |= FLAG_PRIMARY;
			if (!prpget("file", "prefix", NULL, NULL, &ptr, PRP_GETCHILD)) fileserverflag |= FLAG_PREFIX;
		}
	}
	return 0;
}

void srvexit()
{
	int i1;

	for (i1 = 0; i1 < fileservercnt; i1++)
		if ((*fileserver)[i1].fshandle != -1) fsdisconnect((*fileserver)[i1].fshandle);
	memfree((unsigned char **) fileserver);
	fileservercnt = 0;
	fileserverflag = 0;
}

/**
 * path should be one of the FIO_OPT_ codes
 */
int srvgetserver(char *filename, int path)
{
	int i1, i2;
	char server[256], work[MAX_NAMESIZE], *ptr1, *ptr2, **pptr, *name;

	if (fileservercnt) {
		strncpy(work, filename, sizeof(work) - 1);
		work[sizeof(work) - 1] = '\0';
		pptr = NULL;
		if (fileserverflag & FLAG_PREFIX) {
			miofixname(work, "", 0);
			for (i2 = 0; ; i2 = PRP_NEXT) {
				if (prpget("file", "prefix", NULL, NULL, &ptr1, i2 | PRP_GETCHILD)) break;
				if (prpname(&name)) continue;
				i1 = (int)strlen(ptr1);
#if OS_WIN32
#pragma warning(suppress:4996)
				if (_memicmp(ptr1, work, i1)) continue;
#else
				if (memcmp(ptr1, work, i1)) continue;
#endif
				strcpy(server, name);
				ptr1 = server;
				pptr = &ptr1;
				break;
			}
		}
		if (pptr == NULL) {
			miofixname(work, "", FIXNAME_PAR_DBCVOL | FIXNAME_PAR_MEMBER);
			miogetname(&ptr1, &ptr2);
			if (*ptr1) {
				if (fiocvtvol(ptr1, &pptr)) return 0;
			}
			else if (fioaslash(work) >= 0) return 0;
			else if (path) pptr = (char **) fiogetopt(path);
		}
		if (pptr == NULL) {
			if (!(fileserverflag & FLAG_PRIMARY) || prpget("file", "primary", "server", NULL, &ptr1, 0)) return 0;
			pptr = &ptr1;
		}
		for (i1 = 0; i1 < fileservercnt; i1++)
			if (!strcmp(*pptr, (*fileserver)[i1].fsname)) {
				if ((*fileserver)[i1].fshandle == -1) strcpy(srverrstr, (*fileserver)[i1].error);
				return (*fileserver)[i1].fshandle;
			}
	}
	return 0;
}

int srvgetprimary()
{
	int i1;
	char *ptr;

	if (fileservercnt && !prpget("file", "primary", "server", NULL, &ptr, 0) && *ptr) {
		for (i1 = 0; i1 < fileservercnt && strcmp(ptr, (*fileserver)[i1].fsname); i1++);
		if (i1 < fileservercnt) {
			if ((*fileserver)[i1].fshandle == -1) strcpy(srverrstr, (*fileserver)[i1].error);
			return (*fileserver)[i1].fshandle;
		}
	}
	return 0;
}

int srvgeterror(char *msg, int msglength)
{
	int i1;

	if (--msglength < 0) return -1;

	i1 = (int)strlen(srverrstr);
	if (i1 > msglength) i1 = msglength;
	memcpy(msg, srverrstr, i1);
	msg[i1] = 0;
	return 0;
}


#if 0
/*** STARTED WRITING THIS CODE, BUT IT REMAINS INCOMPLETE 01-05-2000 ***/
int srvfixerror(int server, int error, int type, int defaulterr, char *string, int len)
{
	int i1, i2;
	char *ptr;

/*** CODE: MAYBE CHANGE ERROR CONDITION DEPENDING ON SERVER VERSION ***/
	server;

	if (error >= 0) return error;

	ptr = fsiogeterror();
	if (*ptr) {
		i1 = strlen(string);
		if (i1 >= len) i1 = len - 1;
		i2 = strlen(ptr);
		if (i1 && i2) {
			if (i1 + 1 < len) string[i1++] = ':';
			if (i1 + 1 < len) string[i1++] = ' ';
		}
		if (i2 >= len - i1) i2 = len - i1 - 1;
		ptr[i2] = 0;
		strcpy(string + i1, ptr);
		*ptr = 0;
	}

	error = -error;
	if (error > 1000) return error + type + 3000;  /* make 4000 errors */
	if (error % 100 > 50) return error % 100 + type + 1000;  /* make 1000 errors */
	if (error >= 100) {
		if (error / 100 == type / 100) return error;  /* exact correspondance */
/*** CODE: SUPPORT THIS BETTER ***/
		switch (type / 100) {
		case 1:
			if (error == 602) return 105;
			return 101;
		case 4:
			if (error == 602) return 405;
			return 401;
		case 6:
		case 7:
			return error;
		}
	}
	return defaulterr;
}
#endif
