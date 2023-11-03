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
#define INC_LIMITS
#define INC_TIME
#define INC_ERRNO
#include "includes.h"
#include "release.h"
#include "fsrun.h"
#include "fssqlx.h"
#include "fio.h"
#include "base.h"
#include "xml.h"

#define DBCEOR	0xFA
#define DBCEOF	0xFB

#define DBDSTATE_BEFORETABLE	0x0001
#define DBDSTATE_ONTABLE		0x0002
#define DBDSTATE_AFTERTABLE		0x0004
#define DBDSTATE_ONCOLUMN		0x0008
#define DBDSTATE_AFTERCOLUMN	0x0010
#define DBDSTATE_FINISHTABLE	0x0020
#define DBDSTATE_FINISHCOLUMN	0x0040
#define DBDSTATE_FIRSTCOLUMN	0x0080
#define DBDSTATE_INVALIDKW		0x0100

#define SYNTAX_CFG	1
#define SYNTAX_DBD	2

#define MAX_VOLUMESIZE	8
typedef struct {
	CHAR volume[MAX_VOLUMESIZE + 1];
	CHAR **path;
} VOLUME;

typedef struct cfgvolume_struct {
	INT next;
	CHAR volume[MAX_VOLUMESIZE + 1];
	CHAR path[1];
} CFGVOLUME;

extern INT fsflags;
INT sqlstatisticsflag = 0;

CHAR dbdaccess[64];							/* dbd access */
CHAR workdir[256];							/* dbd work directory */
CHAR filepath[4096];						/* dbd file path */
INT dbdvolcount;							/* dbd count of volumes defined */
static INT volcount = 0;					/* dbd number of VOLUME entries */
static INT volalloc = 0;					/* dbd volume memory allocation flag */
static VOLUME **volumes;					/* dbd array structure of volumes */

static CFGVOLUME *cfgvolumes = NULL;		/* buffer of linked list of volumes as defined in the cfg file */
static INT lastcfgvolume = -1;		/* linked list of volumes as defined in the cfg file */
static CHAR *lognames = NULL;
static INT logallfiles = FALSE;
static INT logfilenames = FALSE;
static INT logopenclose = TRUE;
static INT logtimestamp = FALSE;
static INT logusername = FALSE;
static CHAR cfgerrorstring[256];

static INT parsecfgfile(CHAR *cfgfilename, CHAR *useraccess, CHAR *password, INT *memallocsize, FIOPARMS *fioparms, CONNECTION *cnct);
static INT readdbdfile(CHAR *dbdfile, CHAR *useraccess, CONNECTION *cnct);
static void writebuiltin(FILE *dbd, CHAR *data);
static ELEMENT *getxmldata(CHAR *filebuffer, LONG filesize);
static INT getnextkwval(CHAR *record, CHAR **kw, CHAR **val, CHAR **error);
static int fixupvolmap(void);
static int volmapfnc(char *, char ***);
static INT translate(CHAR *ptr, UCHAR *map);
static void syntaxerror(INT type, INT line, CHAR *msg1, CHAR *msg2);

/*
 * Read in and parse the config and dbd files
 * Note that this function calls fioinit
 * @param logfile is input, if it has something in it, it is passed to riologstart
 */
INT cfginit(CHAR *cfgfile, CHAR *dbdfile, CHAR *user, CHAR *password, CHAR *logfile, CONNECTION *cnct)
{
	INT i1, cid, memallocsize, flags;
	CHAR *ptr, useraccess[512];
	FIOPARMS fioparms;
	clock_t t1;

	cfgerrorstring[0] = '\0';
	memset(&fioparms, 0, sizeof(FIOPARMS));
	memallocsize = 0;
	workdir[0] = filepath[0] = dbdaccess[0] = '\0';
	for (i1 = 0; user[i1]; i1++) useraccess[i1] = (CHAR) toupper(user[i1]);
	useraccess[i1] = '\0';
	for (i1 = 0; password[i1]; i1++) password[i1] = (CHAR) toupper(password[i1]);
	if (cnct != NULL) {
		memset(cnct, 0, sizeof(*cnct));
		cnct->updatelock = LOCK_RECORD;
		cnct->deletelock = LOCK_RECORD;
		cnct->maxtablename = 1;
		cnct->maxtableindex = 1;
		cnct->maxtableremarks = 1;
		cnct->maxcolumnname = 1;
		cnct->maxcolumnremarks = 1;
	}
	if (parsecfgfile(cfgfile, useraccess, password, &memallocsize, &fioparms, cnct) < 0) return RC_ERROR;
	if (memallocsize < 256) memallocsize = 256;
	if (meminit(memallocsize << 10, 0, 1024) == -1) {
		strcpy(cfgerrorstring, "meminit failed");
		cfgexit();
		return RC_ERROR;
	}
	fioparms.cvtvolfnc = volmapfnc;
	ptr = fioinit(&fioparms, TRUE);
	if (ptr != NULL) {
		strcpy(cfgerrorstring, ptr);
		cfgexit();
		return RC_ERROR;
	}

	if (readdbdfile(dbdfile, useraccess, cnct) < 0) {
		if (cnct != NULL) memset(cnct, 0, sizeof(*cnct));
		cfgexit();
		return RC_ERROR;
	}
	dbdvolcount = volcount;
	if (cnct != NULL && !cnct->memresultsize) cnct->memresultsize = 128;
	if (fixupvolmap() < 0) {
		if (cnct != NULL) memset(cnct, 0, sizeof(*cnct));
		cfgexit();
		return RC_ERROR;
	}
	if (logfile != NULL && logfile[0]) {
		flags = 0;
		if (logfilenames) flags |= RIO_L_NAM;
		if (logtimestamp) flags |= RIO_L_TIM;
		if (logopenclose) flags |= RIO_L_OPN | RIO_L_CLS;
		if (logusername) flags |= RIO_L_USR;
		i1 = riologstart(logfile, user, dbdfile, flags);
		if (i1) {
			strcpy(cfgerrorstring, "Logging error: ");
			strcat(cfgerrorstring, fioerrstr(i1));
			cfgexit();
			return RC_ERROR;
		}
		fsflags |= FSFLAGS_LOGFILE;
	}
	t1 = clock();
	cid = (t1 % 9999) + 10000;
	if (cnct != NULL) cid += 10000;

	return cid;
}

void cfgexit()
{
	if (cfgvolumes != NULL) {
		free(cfgvolumes);
		cfgvolumes = NULL;
	}
	if (lognames != NULL) {
		free(lognames);
		lognames = NULL;
	}
	riologend();
	fioexit();
#ifdef _DEBUG
	memcompact();
#endif
	meminit(0, 0, 0);
}

/**
 * Checks the argument to see if it occurs in 'lognames'
 */
INT cfglogname(CHAR *filename)
{
	INT i1, i2;
	CHAR *pattern;

	if (logallfiles) return TRUE;
	if (lognames == NULL) return FALSE;
	for (pattern = lognames; *pattern; pattern += strlen(pattern) + 1) {
		for (i1 = i2 = 0; pattern[i1]; ) {
			if (pattern[i1] == '*') {
/*** NOTE: CODE CONVERTS "*?" to "*". FIX ONLY IF IT BECOMES A PROBLEM ***/
				for (i1++; pattern[i1] == '*' || pattern[i1] == '?'; i1++);
				if (!pattern[i1]) {
					while (filename[i2]) i2++;
					break;
				}
				while (filename[i2] && filename[i2] != pattern[i1]) i2++;
				if (!filename[i2]) break;
				continue;
			}
			if (pattern[i1] == '?') {
				if (!filename[i2]) break;
			}
			else if (pattern[i1] != filename[i2]) break;
			i2++;
			i1++;
		}
		if (!pattern[i1] && !filename[i2]) return TRUE;
	}
	return FALSE;
}

/**
 * Returns -1 if error, the length of the value if success
 */
INT cfggetentry(CHAR *cfgfilename, CHAR *entry, CHAR *value, INT size)
{
	INT i1, length;
	LONG filesize;
	INT nextoffset, rc, xmlflag;
	CHAR *filebuffer, *error, *kw, *val;
	FILE *cfgfile; // @suppress("Statement has no effect")
	ELEMENT *element, *nextelement, *xmlbuffer;

	if (size <= 0) return RC_ERROR;
	value[0] = '\0';

	cfgfile = fopen(cfgfilename, "rb");
	if (cfgfile == NULL) {
		strcpy(cfgerrorstring, "CFG file open failed");
		return RC_ERROR;
	}
	fseek(cfgfile, 0, 2);
	filesize = (INT) ftell(cfgfile);
	fseek(cfgfile, 0, 0);
	filebuffer = (CHAR *) malloc(filesize + sizeof(CHAR));
	if (filebuffer == NULL) {
		fclose(cfgfile);
		strcpy(cfgerrorstring, "insufficient memory (malloc)");
		return RC_ERROR;
	}
	i1 = (INT)fread(filebuffer, 1, filesize, cfgfile);
	fclose(cfgfile);
	if (i1 != filesize) {
		free(filebuffer);
		strcpy(cfgerrorstring, "error reading cfg file");
		return RC_ERROR;
	}
	for (i1 = 0; i1 < filesize && (isspace(filebuffer[i1]) || filebuffer[i1] == '\n' || filebuffer[i1] == '\r'); i1++);
	if (i1 < filesize && filebuffer[i1] == '<') {
		xmlbuffer = getxmldata(filebuffer + i1, filesize - i1);
		free(filebuffer);
		if (xmlbuffer == NULL) return RC_ERROR;
		for (element = xmlbuffer; element != NULL && (element->cdataflag || strcmp(element->tag, "dbcfscfg")); element = element->nextelement);
		if (element == NULL) {
			free(xmlbuffer);
			strcpy(cfgerrorstring, "CFG file does not contain 'dbcfscfg' element");
			return RC_ERROR;
		}
		nextelement = element->firstsubelement;
		xmlflag = TRUE;
	}
	else {
		nextoffset = length = 0;
		xmlflag = FALSE;
	}

	rc = -1;  /* assume error */
	for ( ; ; ) {
		if (xmlflag) {
			if (nextelement == NULL) break;
			element = nextelement;
			nextelement = element->nextelement;
			if (element->cdataflag) continue;
			kw = element->tag;
			if (element->firstsubelement != NULL && element->firstsubelement->cdataflag) val = element->firstsubelement->tag;
			else val = "";
		}
		else {
			if (nextoffset >= length) {
				if (nextoffset >= filesize) break;
				for (length = nextoffset; length < filesize && filebuffer[length] != '\n' && filebuffer[length] != '\r'; length++);
				if (filebuffer[length] == '\r' && filebuffer[length + 1] == '\n') filebuffer[length + 1] = ' ';
				filebuffer[length] = '\0';
			}
			i1 = getnextkwval(filebuffer + nextoffset, &kw, &val, &error);
			if (i1 < 0) break;
			nextoffset += i1;
		}
		if (!strcmp(kw, entry)) {
			strncpy(value, val, size);
			value[size - 1] = '\0';
			break;
		}
	}
	if (xmlflag) free(xmlbuffer);
	else free(filebuffer);
	return (INT)strlen(value);
}

INT cfgaddname(CHAR *name, CHAR ***nametable, INT *nametablesize, INT *nametablealloc)
{
	INT i1, i2;

	if (*nametable == NULL) {
		*nametable = (CHAR **) memalloc(4096, 0);
		if (*nametable == NULL) {
			strcpy(cfgerrorstring, "insufficient memory");
			return RC_ERROR;
		}
		*nametablealloc = 4096;
		*nametablesize = 0;
	}

	i2 = (INT)strlen(name);
	i1 = *nametablesize;

	while (i1 + i2 >= *nametablealloc) {
		if (memchange((UCHAR **) *nametable, *nametablealloc + 4096, 0) == -1) {
			strcpy(cfgerrorstring, "insufficient memory");
			return RC_ERROR;
		}
		*nametablealloc += 4096;
	}
	strcpy(**nametable + i1, name);
	*nametablesize += i2 + 1;
	return i1;
}

CHAR *cfggeterror()
{
	return cfgerrorstring;
}

static INT parsecfgfile(CHAR *cfgfilename, CHAR *useraccess, CHAR *password, INT *memallocsize, FIOPARMS *fioparms, CONNECTION *cnct)
{
	INT i1, i2, exclusiveflag, length, linecnt, logpos, logsize;
	LONG filesize;
	INT nextlevel, nextoffset, prepflag, pwdstate, rc, volpos, volsize, xmlflag;
	OFFSET off;
	CHAR pwdfile[256], *filebuffer, *error, *kw, *ptr, *val;
	FILE *cfgfile, *f1;
	ELEMENT *element, *element2, *nextelement[2], *xmlbuffer;
	CFGVOLUME *cfgvol;

	cfgfile = fopen(cfgfilename, "rb");
	if (cfgfile == NULL) {
		strcpy(cfgerrorstring, "CFG file open failed");
		return RC_ERROR;
	}
	fseek(cfgfile, 0, 2);
	filesize = (INT) ftell(cfgfile);
	fseek(cfgfile, 0, 0);
	filebuffer = (CHAR *) malloc(filesize + sizeof(CHAR));
	if (filebuffer == NULL) {
		fclose(cfgfile);
		strcpy(cfgerrorstring, "insufficient memory (malloc)");
		return RC_ERROR;
	}
	i1 = (INT)fread(filebuffer, 1, filesize, cfgfile);
	fclose(cfgfile);
	if (i1 != filesize) {
		free(filebuffer);
		strcpy(cfgerrorstring, "error reading cfg file");
		return RC_ERROR;
	}
	for (i1 = 0; i1 < filesize && (isspace(filebuffer[i1]) || filebuffer[i1] == '\n' || filebuffer[i1] == '\r'); i1++);
	if (i1 < filesize && filebuffer[i1] == '<') {
		xmlbuffer = getxmldata(filebuffer + i1, filesize - i1);
		free(filebuffer);
		if (xmlbuffer == NULL) return RC_ERROR;
		for (element = xmlbuffer; element != NULL && (element->cdataflag || strcmp(element->tag, "dbcfscfg")); element = element->nextelement);
		if (element == NULL) {
			free(xmlbuffer);
			strcpy(cfgerrorstring, "CFG file does not contain 'dbcfscfg' element");
			return RC_ERROR;
		}
		nextelement[0] = element->firstsubelement;
		nextlevel = 0;
		xmlflag = TRUE;
	}
	else {
		nextoffset = length = 0;
		xmlflag = FALSE;
	}

	rc = RC_ERROR;  /* assume error */
	sqlstatisticsflag = 0;
	exclusiveflag = TRUE;
	prepflag = FALSE;
	pwdfile[0] = '\0';
	logpos = logsize = volpos = volsize = 0;
	for (linecnt = 0; ; ) {
		if (xmlflag) {
			if (nextelement[nextlevel] == NULL) {
				if (--nextlevel >= 0) continue;
				rc = 0;  /* set success */
				break;
			}
			element = nextelement[nextlevel];
			nextelement[nextlevel] = element->nextelement;
			if (element->cdataflag) continue;
			kw = element->tag;
			if (element->firstsubelement != NULL && element->firstsubelement->cdataflag) val = element->firstsubelement->tag;
			else val = "";
		}
		else {
			if (nextoffset >= length) {
				if (nextoffset >= filesize) {
					rc = 0;  /* set success */
					break;
				}
				for (length = nextoffset; length < filesize && filebuffer[length] != '\n' && filebuffer[length] != '\r'; length++);
				if (filebuffer[length] == '\r' && filebuffer[length + 1] == '\n') filebuffer[length + 1] = ' ';
				filebuffer[length] = '\0';
				linecnt++;
			}
			i1 = getnextkwval(filebuffer + nextoffset, &kw, &val, &error);
			if (i1 < 0) {
				syntaxerror(SYNTAX_CFG, linecnt, error, kw);
				break;
			}
			nextoffset += i1;
		}
		if (!*kw) continue;
		if (*kw == 'a') {
			if (!strcmp(kw, "adminpassword")) continue;
			if (!strcmp(kw, "authfile")) continue;
		}
		if (*kw == 'c') {
			if (!strcmp(kw, "casemap")) {
				if (!val[0]) {
					syntaxerror(SYNTAX_CFG, linecnt, "invalid casemap specification", NULL);
					break;
				}
				for (i1 = 0; i1 <= UCHAR_MAX; i1++) fioparms->casemap[i1] = (UCHAR) toupper(i1);
				if (translate(val, fioparms->casemap)) {
					f1 = fopen(val, "rb");
					if (f1 == NULL) {
						strcpy(cfgerrorstring, "CFG error: casemap open failure");
						break;
					}
					i1 = (INT)fread(fioparms->casemap, sizeof(CHAR), 256, f1);
					fclose(f1);
					if (i1 != 256) {
						strcpy(cfgerrorstring, "CFG error: casemap read failure");
						break;
					}
				}
				continue;
			}
			if (!strcmp(kw, "checkretry")) continue;
			if (!strcmp(kw, "certificatefilename")) continue;
			if (!strcmp(kw, "checktime")) continue;
			if (!strcmp(kw, "collatemap")) {
				if (!val[0]) {
					syntaxerror(SYNTAX_CFG, linecnt, "invalid collatemap specification", NULL);
					break;
				}
				for (i1 = 0; i1 <= UCHAR_MAX; i1++) fioparms->collatemap[i1] = (UCHAR) i1;
				if (translate(val, fioparms->collatemap)) {
					f1 = fopen(val, "rb");
					if (f1 == NULL) {
						strcpy(cfgerrorstring, "CFG error: collatemap open failure");
						break;
					}
					i1 = (INT)fread(fioparms->collatemap, sizeof(CHAR), 256, f1);
					fclose(f1);
					if (i1 != 256) {
						strcpy(cfgerrorstring, "CFG error: collatemap read failure");
						break;
					}
				}
				continue;
			}
			if (!strcmp(kw, "compat")) {
				if (cnct == NULL) {
					for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
					if (!strcmp(val, "dos")) fioparms->flags |= FIO_FLAG_COMPATDOS;
					else if (!strcmp(val, "rms")) fioparms->flags |= FIO_FLAG_COMPATRMS;
					else if (!strcmp(val, "rmsx")) fioparms->flags |= FIO_FLAG_COMPATRMSX;
					else if (!strcmp(val, "rmsy")) fioparms->flags |= FIO_FLAG_COMPATRMSY;
					else {
						syntaxerror(SYNTAX_CFG, linecnt, "invalid compat specification", NULL);
						break;
					}
				}
				continue;
			}
		}
		if (*kw == 'd') {
			if (!strcmp(kw, "dbdpath")) {
				if (xmlflag) {
					for (i2 = 0, element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement) {
						if (element2->cdataflag) continue;
						if (!strcmp(element2->tag, "dir")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								if (i2) fioparms->dbdpath[i2++] = ';';
								strcpy(fioparms->dbdpath + i2, element2->firstsubelement->tag);
								i2 += (INT)strlen(element2->firstsubelement->tag);
							}
						}
					}
				}
				else strcpy(fioparms->dbdpath, val);
				continue;
			}
			else if (!strcmp(kw, "deletelock")) {
				if (cnct != NULL) {
					for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
					if (!strcmp(val, "file")) cnct->deletelock = LOCK_FILE;
					else if (!strcmp(val, "off")) cnct->deletelock = LOCK_OFF;
					else if (!strcmp(val, "record")) cnct->deletelock = LOCK_RECORD;
					else {
						syntaxerror(SYNTAX_CFG, linecnt, "invalid deletelock specification", NULL);
						break;
					}
				}
				continue;
			}
		}
		if (*kw == 'e') {
			if (!strcmp(kw, "encryption")) continue;
			if (!strcmp(kw, "excloffset")) {
				mscatooff(val, &off);
				fioparms->excloffset = off;
				fioparms->parmflags |= FIO_PARM_EXCLOFFSET;
				continue;
			}
			if (!strcmp(kw, "eport")) continue;
			if (!strcmp(kw, "extcase")) {
				for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
				if (!strcmp(val, "upper")) fioparms->flags |= FIO_FLAG_EXTCASEUPPER;
				else if (!strcmp(val, "lower")) { /* do nothing */ }
				else {
					syntaxerror(SYNTAX_CFG, linecnt, "invalid extcase specification", NULL);
					break;
				}
				continue;
			}
		}
		if (*kw == 'f') {
			if (!strcmp(kw, "filepath")) {
				if (xmlflag) {
					for (i2 = 0, element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement) {
						if (element2->cdataflag) continue;
						if (!strcmp(element2->tag, "dir")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								if (i2) fioparms->openpath[i2++] = ';';
								else if (!prepflag) strcpy(fioparms->preppath, element2->firstsubelement->tag);
								strcpy(fioparms->openpath + i2, element2->firstsubelement->tag);
								i2 += (INT)strlen(element2->firstsubelement->tag);
							}
						}
					}
				}
				else {
					strcpy(fioparms->openpath, val);
					if (!prepflag) {
						for (i1 = 0; val[i1] && val[i1] != ';'; i1++);
						val[i1] = 0;
						strcpy(fioparms->preppath, val);
					}
				}
				continue;
			}
			if (!strcmp(kw, "fileoffset")) {
				mscatooff(val, &off);
				fioparms->fileoffset = off;
				fioparms->parmflags |= FIO_PARM_FILEOFFSET;
				continue;
			}
		}
		if (*kw == 'l') {
			if (!strcmp(kw, "licensekey")) continue;
			if (xmlflag) {
				if (!strcmp(kw, "logchanges")) {
					for (element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement) {
						if (element2->cdataflag) continue;
						if (!strcmp(element2->tag, "file")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								i1 = (INT)strlen(element2->firstsubelement->tag);
								if (logpos + i1 + 2 > logsize) {
									for (logsize = 512; logpos + i1 + 2 > logsize; logsize <<= 1);
									if (lognames == NULL) ptr = (CHAR *) malloc(logsize);
									else ptr = realloc(lognames, logsize);
									if (ptr == NULL) {
										strcpy(cfgerrorstring, "insufficient memory (realloc)");
										break;
									}
									lognames = ptr;
								}
								strcpy(lognames + logpos, element2->firstsubelement->tag);
								logpos += (INT)strlen(element2->firstsubelement->tag) + 1;
							}
						}
						else if (!strcmp(element2->tag, "allfiles")) logallfiles = TRUE;
						else if (!strcmp(element2->tag, "lognames")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								val = element2->firstsubelement->tag;
								for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
								if (!strcmp(val, "on")) logfilenames = TRUE;
								else if (strcmp(val, "off")) {
									syntaxerror(SYNTAX_CFG, linecnt, "invalid lognames specification", NULL);
									break;
								}
							}
						}
						else if (!strcmp(element2->tag, "logopenclose")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								val = element2->firstsubelement->tag;
								for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
								if (!strcmp(val, "off")) logopenclose = FALSE;
								else if (strcmp(val, "on")) {
									syntaxerror(SYNTAX_CFG, linecnt, "invalid logopenclose specification", NULL);
									break;
								}
							}
						}
						else if (!strcmp(element2->tag, "logtimestamp") || !strcmp(element2->tag, "timestamp")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								val = element2->firstsubelement->tag;
								for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
								if (!strcmp(val, "on")) logtimestamp = TRUE;
								else if (strcmp(val, "off")) {
									syntaxerror(SYNTAX_CFG, linecnt, "invalid logtimestamp specification", NULL);
									break;
								}
							}
						}
						else if (!strcmp(element2->tag, "logusername")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								val = element2->firstsubelement->tag;
								for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
								if (!strcmp(val, "on")) logusername = TRUE;
								else if (strcmp(val, "off")) {
									syntaxerror(SYNTAX_CFG, linecnt, "invalid logusername specification", NULL);
									break;
								}
							}
						}
					}
					continue;
				}
			}
			else if (!strcmp(kw, "logfile")) {  /* non-xml parameter */
				logallfiles = TRUE;
				continue;
			}
			else if (!strcmp(kw, "lognames")) {  /* non-xml parameter */
				for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
				if (!strcmp(val, "on")) logfilenames = TRUE;
				else if (strcmp(val, "off")) {
					syntaxerror(SYNTAX_CFG, linecnt, "invalid lognames specification", NULL);
					break;
				}
				continue;
			}
			else if (!strcmp(kw, "logopenclose")) {  /* non-xml parameter */
				for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
				if (!strcmp(val, "off")) logopenclose = FALSE;
				else if (strcmp(val, "on")) {
					syntaxerror(SYNTAX_CFG, linecnt, "invalid logopenclose specification", NULL);
					break;
				}
				continue;
			}
			else if (!strcmp(kw, "logtimestamp")) {  /* non-xml parameter */
				for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
				if (!strcmp(val, "on")) logtimestamp = TRUE;
				else if (strcmp(val, "off")) {
					syntaxerror(SYNTAX_CFG, linecnt, "invalid logtimestamp specification", NULL);
					break;
				}
				continue;
			}
			else if (!strcmp(kw, "logusername")) {  /* non-xml parameter */
				for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
				if (!strcmp(val, "on")) logusername = TRUE;
				else if (strcmp(val, "off")) {
					syntaxerror(SYNTAX_CFG, linecnt, "invalid logusername specification", NULL);
					break;
				}
				continue;
			}
		}
		if (*kw == 'm') {
			if (!strcmp(kw, "memalloc")) {
				*memallocsize = atoi(val);
				continue;
			}
			if (!strcmp(kw, "memresult")) {
				if (cnct != NULL) cnct->memresultsize = atoi(val);
				continue;
			}
		}
		if (*kw == 'n') {
			if (!strcmp(kw, "namecase")) {
				for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
				if (!strcmp(val, "upper")) fioparms->flags |= FIO_FLAG_NAMECASEUPPER;
				else if (!strcmp(val, "lower")) fioparms->flags |= FIO_FLAG_NAMECASELOWER;
				else {
					syntaxerror(SYNTAX_CFG, linecnt, "invalid namecase specification", NULL);
					break;
				}
				continue;
			}
			if (!strcmp(kw, "nodigitcompression")) {
				fioparms->flags |= FIO_FLAG_NOCOMPRESS;
				continue;
			}
			if (!strcmp(kw, "noexclusivesupport")) {
				exclusiveflag = FALSE;
				continue;
			}
			if (!strcmp(kw, "nport")) continue;
			if (!strcmp(kw, "newconnectiondelay")) continue;
		}
		if (*kw == 'o') {
/*** CODE: WHY DO WE HAVE THIS ??? ***/
			if (!strcmp(kw, "openexclusive")) {
				fioparms->flags |= FIO_FLAG_SINGLEUSER;
				continue;
			}
		}
		if (*kw == 'p') {
			if (!strcmp(kw, "passwordfile")) {
				strcpy(pwdfile, val);
				continue;
			}
			if (!strcmp(kw, "pathcase")) {
				for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
				if (!strcmp(val, "upper")) fioparms->flags |= FIO_FLAG_PATHCASEUPPER;
				else if (!strcmp(val, "lower")) fioparms->flags |= FIO_FLAG_PATHCASELOWER;
				else {
					syntaxerror(SYNTAX_CFG, linecnt, "invalid pathcase specification", NULL);
					break;
				}
				continue;
			}
			if (!strcmp(kw, "preppath")) {
				if (xmlflag) {
					for (element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement) {
						if (element2->cdataflag) continue;
						if (!strcmp(element2->tag, "dir")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								strcpy(fioparms->preppath, element2->firstsubelement->tag);
								break;
							}
						}
					}
				}
				else strcpy(fioparms->preppath, val);
				prepflag = TRUE;
				continue;
			}
		}
		if (*kw == 'r') {
			if (!strcmp(kw, "recoffset")) {
				mscatooff(val, &off);
				fioparms->recoffset = off;
				fioparms->parmflags |= FIO_PARM_RECOFFSET;
				continue;
			}
		}
		if (*kw == 's') {
			if (!strcmp(kw, "showpassword")) continue;
			if (!strcmp(kw, "sport")) continue;
			if (!strcmp(kw, "sqlstatistics")) {
				if (cnct != NULL) {
					for (i1 = 0; val[i1]; i1++) val[i1] = (char) tolower(val[i1]);
					if (!strcmp(val, "includeext")) sqlstatisticsflag |= 0x01;
					else if (!strcmp(val, "replacedot")) sqlstatisticsflag |= 0x02;
					else if (!strcmp(val, "includeaim")) sqlstatisticsflag |= 0x04;
					else {
						syntaxerror(SYNTAX_CFG, linecnt, "invalid sqlstatistics option", NULL);
						break;
					}
				}
				continue;
			}
		}
		if (*kw == 'u') {
			if (!strcmp(kw, "updatelock")) {
				if (cnct != NULL) {
					for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) tolower(val[i1]);
					if (!strcmp(val, "file")) cnct->updatelock = LOCK_FILE;
					else if (!strcmp(val, "off")) cnct->updatelock = LOCK_OFF;
					else if (!strcmp(val, "record")) cnct->updatelock = LOCK_RECORD;
					else {
						syntaxerror(SYNTAX_CFG, linecnt, "invalid updatelock specification", NULL);
						break;
					}
				}
				continue;
			}
			if (!strcmp(kw, "usermax")) continue;
		}
		if (*kw == 'v') {
			if (!strcmp(kw, "volume")) {
				if (xmlflag) {
					for (i2 = 0, element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement) {
						if (element2->cdataflag) continue;
						if (!strcmp(element2->tag, "dir")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								if (i2) i2++;
								i2 += (INT)strlen(element2->firstsubelement->tag);
							}
						}
						else if (!strcmp(element2->tag, "name")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) val = element2->firstsubelement->tag;
							else val = "";
						}
					}
				}
				else {
					for (i1 = 0; val[i1] && !isspace(val[i1]); i1++);
					if (val[i1]) val[i1++] = '\0';
					for ( ; isspace(val[i1]); i1++);
					i2 = (INT)strlen(val + i1);
				}
				if (!val[0]) {
					syntaxerror(SYNTAX_CFG, linecnt, "invalid volume specification", NULL);
					break;
				}
				if (strlen(val) > MAX_VOLUMESIZE) {
					syntaxerror(SYNTAX_CFG, linecnt, "volume name too long", val);
					break;
				}
				if (!i2) {
					syntaxerror(SYNTAX_CFG, linecnt, "volume path missing", val);
					break;
				}
				if ((INT)((volpos + 1) * sizeof(CFGVOLUME) + i2) > volsize) {
					for (volsize = 512; (INT)((volpos + 1) * sizeof(CFGVOLUME) + i2) > volsize; volsize <<= 1);
					if (cfgvolumes == NULL) ptr = (CHAR *) malloc(volsize);
					else ptr = (CHAR *) realloc(cfgvolumes, volsize);
					if (ptr == NULL) {
						strcpy(cfgerrorstring, "insufficient memory (realloc)");
						break;
					}
					cfgvolumes = (CFGVOLUME *) ptr;
				}
				cfgvol = cfgvolumes + volpos;
				cfgvol->next = lastcfgvolume;
				lastcfgvolume = volpos;
				strcpy(cfgvol->volume, val);
				if (xmlflag) {
					for (i2 = 0, element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement) {
						if (element2->cdataflag) continue;
						if (!strcmp(element2->tag, "dir")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								if (i2) cfgvol->path[i2++] = ';';
								strcpy(cfgvol->path + i2, element2->firstsubelement->tag);
								i2 += (INT)strlen(element2->firstsubelement->tag);
							}
						}
					}
				}
				else strcpy(cfgvol->path, val + i1);
				volpos += 1 + (i2 / sizeof(CFGVOLUME));
				if (i2 % sizeof(CFGVOLUME)) volpos++;
				continue;
			}
		}
		if (*kw == 'w') {
			if (!strcmp(kw, "workdir")) {
				strcpy(workdir, val);
				continue;
			}
		}
		syntaxerror(SYNTAX_CFG, linecnt, "unsupported keyword", kw);
		break;
	}
	if (lognames != NULL) lognames[logpos] = '\0';
	if (xmlflag) free(xmlbuffer);
	else free(filebuffer);

	if (!rc) {
#if OS_UNIX
		if (exclusiveflag || (fioparms->flags & FIO_FLAG_SINGLEUSER)) fioparms->flags |= FIO_FLAG_EXCLOPENLOCK;
#endif
		/* get access code from password file and store back into useraccess */
		if (!pwdfile[0]) strcpy(pwdfile, "dbcfspwd.cfg");
		cfgfile = fopen(pwdfile, "rb");
		if (cfgfile == NULL) {
			strcpy(cfgerrorstring, "open failure on password file");
			return RC_ERROR;
		}
		fseek(cfgfile, 0, 2);
		filesize = (INT) ftell(cfgfile);
		fseek(cfgfile, 0, 0);
		filebuffer = (CHAR *) malloc(filesize + sizeof(CHAR));
		if (filebuffer == NULL) {
			fclose(cfgfile);
			strcpy(cfgerrorstring, "insufficient memory (malloc)");
			return RC_ERROR;
		}
		i1 = (INT)fread(filebuffer, 1, filesize, cfgfile);
		fclose(cfgfile);
		if (i1 != filesize) {
			free(filebuffer);
			strcpy(cfgerrorstring, "error reading password file");
			return RC_ERROR;
		}
		for (i1 = 0; i1 < filesize && (isspace(filebuffer[i1]) || filebuffer[i1] == '\n' || filebuffer[i1] == '\r'); i1++);
		if (i1 < filesize && filebuffer[i1] == '<') {
			xmlbuffer = getxmldata(filebuffer + i1, filesize - i1);
			free(filebuffer);
			if (xmlbuffer == NULL) return RC_ERROR;
			for (element = xmlbuffer; element != NULL && (element->cdataflag || strcmp(element->tag, "dbcfspwd")); element = element->nextelement);
			if (element == NULL) {
				free(xmlbuffer);
				strcpy(cfgerrorstring, "CFG file does not contain 'dbcfspwd' element");
				return RC_ERROR;
			}
			nextelement[0] = element->firstsubelement;
			nextlevel = 0;
			xmlflag = TRUE;
		}
		else {
			nextoffset = length = 0;
			xmlflag = FALSE;
		}

		rc = RC_ERROR;  /* assume error */
		pwdstate = 0;
		for (linecnt = 0; ; ) {
			if (xmlflag) {
				if (nextelement[nextlevel] == NULL) {
					if (--nextlevel < 0) break;
					if (pwdstate == 0x07) {
						useraccess[i2] = '\0';
						rc = 0;  /* set success */
						break;
					}
					continue;
				}
				element = nextelement[nextlevel];
				nextelement[nextlevel] = element->nextelement;
				if (element->cdataflag) continue;
				kw = element->tag;
				if (element->firstsubelement != NULL && element->firstsubelement->cdataflag) val = element->firstsubelement->tag;
				else val = "";
			}
			else {
				if (nextoffset >= length) {
					if (nextoffset >= filesize) break;
					for (length = nextoffset; length < filesize && filebuffer[length] != '\n' && filebuffer[length] != '\r'; length++);
					if (filebuffer[length] == '\r' && filebuffer[length + 1] == '\n') filebuffer[length + 1] = ' ';
					filebuffer[length] = '\0';
					linecnt++;
				}
				i1 = getnextkwval(filebuffer + nextoffset, &kw, &val, &error);
				if (i1 < 0) {
					nextoffset = length;
					continue;
				}
				nextoffset += i1;
				i2 = 0;
			}
			if (!*kw) continue;
			if (xmlflag) {
				if (!strcmp(kw, "user")) {
					if (nextlevel) {  /* invalid format */
						nextlevel = 0;
						continue;
					}
					nextelement[++nextlevel] = element->firstsubelement;
					i2 = 0;
					continue;
				}
				if (!nextlevel) continue;  /* invalid format */
			}
			if (!*val) continue;
			if (!strcmp(kw, "name")) {
				for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) toupper(val[i1]);
				if (!strcmp(val, useraccess)) pwdstate |= 0x01;
				else {
					pwdstate = 0;
					if (xmlflag) nextlevel--;
				}
			}
			else if (!strcmp(kw, "password")) {
				for (i1 = 0; val[i1]; i1++) val[i1] = (CHAR) toupper(val[i1]);
				if (!strcmp(val, password)) pwdstate |= 0x02;
				else {
					pwdstate = 0;
					if (xmlflag) nextlevel--;
				}
			}
			else if (!strcmp(kw, "access")) {
				if (xmlflag || pwdstate == 0x03) {
					/* fixup to be used by readdbdfile */
					for (i1 = 0; val[i1]; i1++) {
						if (isspace(val[i1])) continue;
						if (val[i1] == ';') useraccess[i2++] = '\0';
						else useraccess[i2++] = (CHAR) toupper(val[i1]);
					}
					useraccess[i2++] = '\0';
					if (!xmlflag) {
						useraccess[i2] = '\0';
						rc = 0;  /* set success */
						break;
					}
					pwdstate |= 0x04;
				}
				else pwdstate = 0;
			}
			else {
				pwdstate = 0;
				if (xmlflag) nextlevel--;
			}
		}
		if (xmlflag) free(xmlbuffer);
		else free(filebuffer);
		if (rc == -1) strcpy(cfgerrorstring, "user access denied");
	}
	return rc;
}

static void writebuiltin(FILE *dbd, CHAR *data) {
	INT i1, len;
	for (i1 = 0, len = (INT)strlen(data); i1 < len; i1++) {
		if (data[i1] == '>') fprintf(dbd, "&gt;");
		else if (data[i1] == '<') fprintf(dbd, "&lt;");
		else if (data[i1] == '&') fprintf(dbd, "&amp;");
		else if (data[i1] == '\'') fprintf(dbd, "&apos;");
		else if (data[i1] == '"') fprintf(dbd, "&quot;");
		else if ((UCHAR) data[i1] < 0x20 || data[i1] >= 0x7F) fprintf(dbd, "&#%d;", (INT) data[i1]);
		else fprintf(dbd, "%c", data[i1]);
	}
}

INT writedbdfile(CONNECTION *cnct) {
	INT i1, i2, i3, i4, type;
	CHAR work[MAX_NAMESIZE], work2[256], *found, c1;
	TABLE table;
	INDEX index;
	DBDPROP dbdprop;
	COLUMN column;
	FILE *dbd; // @suppress("Statement has no effect")

	strcpy(work, *cnct->dbdfilename);
	miofixname(work, ".dbd", FIXNAME_EXT_ADD);
	found = NULL;
	fiofindfirst(work, FIO_P_DBD, &found);
	if (found == NULL) {
		strcpy(cfgerrorstring, "DBD file not found");
		return RC_ERROR;
	}
	strcpy(work, found);
	i1 = (INT)strlen(work);
	work[i1++] = '~';
	work[i1] = '\0';
	dbd = fopen(work, "w");
	if (dbd == NULL) {
		strcpy(cfgerrorstring, "creation of temporary DBD file failed");
		return RC_ERROR;
	}
	fprintf(dbd, "<dbcfsdbd>\n");
	fprintf(dbd, "\t<access>%s</access>\n", dbdaccess);
	if (filepath[0]) {
		fprintf(dbd, "\t<filepath>\n");
		for (i1 = i2 = ((INT)strlen(filepath) - 1); i1 >= 0; i1--) {
			if (filepath[i1] == ';' || i1 == 0) {
				if (i1 == 0) {
					memcpy(work2, filepath + i1, (i2 - i1) + 1);
					work2[(i2 - i1) + 1] = '\0';
				}
				else {
					memcpy(work2, filepath + i1 + 1, i2 - i1);
					work2[i2 - i1] = '\0';
					i2 = i1 - 1;
				}
				fprintf(dbd, "\t\t<dir>%s</dir>\n", work2);
			}
		}
		fprintf(dbd, "\t</filepath>\n");
	}
	fflush(dbd);
	/* fixupvolmap() merges volumes from cfg and dbd, so only loop through volumes defined in dbd */
	for (i3 = 0; i3 < dbdvolcount; i3++) {
		fprintf(dbd, "\t<volume>\n\t\t<name>%s</name>\n", (*volumes)[i3].volume);
		for (i1 = i2 = ((INT)strlen(*((*volumes)[i3].path)) - 1); i1 >= 0; i1--) {
			if ((*((*volumes)[i3].path))[i1] == ';' || i1 == 0) {
				if (i1 == 0) {
					memcpy(work2, (*((*volumes)[i3].path)) + i1, (i2 - i1) + 1);
					work2[(i2 - i1) + 1] = 0;
				}
				else {
					memcpy(work2, (*((*volumes)[i3].path)) + i1 + 1, i2 - i1);
					work2[i2 - i1] = 0;
					i2 = i1 - 1;
				}
				fprintf(dbd, "\t\t<dir>%s</dir>\n", work2);
			}
		}
		fprintf(dbd, "\t</volume>\n");
	}
	fflush(dbd);
	for (i1 = 0; i1 < cnct->numtables; i1++) {
		table = (*cnct->htablearray)[i1];
		fprintf(dbd, "\t<table>\n");
		fprintf(dbd, "\t\t<name>%s</name>\n", *cnct->nametable + table.name);
		if (table.numdbdprops) fprintf(dbd, "\t\t<properties>\n");
		for (i2 = 0; i2 < table.numdbdprops; i2++) {
			dbdprop = (*(table.hdbdproparray))[i2];
			if (strlen(*cnct->nametable + dbdprop.value)) {
				fprintf(dbd, "\t\t\t<%s>", *cnct->nametable + dbdprop.key);
				writebuiltin(dbd, *cnct->nametable + dbdprop.value);
				fprintf(dbd, "</%s>\n", *cnct->nametable + dbdprop.key);
			}
			else {
				fprintf(dbd, "\t\t\t<%s/>\n", *cnct->nametable + dbdprop.key);
			}

		}
		if (table.numdbdprops) fprintf(dbd, "\t\t</properties>\n");
		if (table.description) {
			fprintf(dbd, "\t\t<description>");
			writebuiltin(dbd, *cnct->nametable + table.description);
			fprintf(dbd, "</description>\n");
		}
		fprintf(dbd, "\t\t<textfile>%s</textfile>\n", *cnct->nametable + table.textfilename);
		type = table.rioopenflags & RIO_T_MASK;
		if (type == RIO_T_STD) fprintf(dbd, "\t\t<textfiletype>standard</textfiletype>\n");
		else if (type == RIO_T_TXT) fprintf(dbd, "\t\t<textfiletype>text</textfiletype>\n");
		else if (type == RIO_T_DAT) fprintf(dbd, "\t\t<textfiletype>data</textfiletype>\n");
		else if (type == RIO_T_DOS) fprintf(dbd, "\t\t<textfiletype>crlf</textfiletype>\n");
		else if (type == RIO_T_MAC) fprintf(dbd, "\t\t<textfiletype>mac</textfiletype>\n");
		else if (type == RIO_T_BIN) fprintf(dbd, "\t\t<textfiletype>binary</textfiletype>\n");
		for (i2 = 0; i2 < table.numindexes; i2++) {
			index = (*table.hindexarray)[i2];
			fprintf(dbd, "\t\t<indexfile>%s</indexfile>\n", *cnct->nametable + index.indexfilename);
		}
		if (table.filtervalue && table.filtercolumnnum) {
			fprintf(dbd, "\t\t<filter>\n\t\t\t<column>%s</column>\n", *cnct->nametable + (*(table.hcolumnarray))[table.filtercolumnnum - 1].name);
			fprintf(dbd, "\t\t\t<value>");
			writebuiltin(dbd, *cnct->nametable + table.filtervalue);
			fprintf(dbd, "</value>\n\t\t</filter>\n");
		}
		if (table.rioopenflags & RIO_FIX) {
			if (table.reclength > 0) {
				fprintf(dbd, "\t\t<fixedlength>%d</fixedlength>\n", table.reclength);
			}
			else fprintf(dbd, "\t\t<fixedlength/>\n"); /* FS will calculate length */
		}
		else {
			fprintf(dbd, "\t\t<variablelength>%d</variablelength>\n", table.reclength);
		}
		if (table.flags & TABLE_FLAGS_NOUPDATE) fprintf(dbd, "\t\t<compressed/>\n");
		if (table.flags & TABLE_FLAGS_READONLY) fprintf(dbd, "\t\t<readonly/>\n");
		fprintf(dbd, "\t\t<columns>\n");
		for (i2 = 0; i2 < table.numcolumns; i2++) {
			column = (*(table.hcolumnarray))[i2];
			fprintf(dbd, "\t\t\t<c>\n");
			fprintf(dbd, "\t\t\t\t<n>%s</n>\n", *cnct->nametable + column.name);
			if (column.numdbdprops) fprintf(dbd, "\t\t\t\t<properties>\n");
			for (i3 = 0; i3 < column.numdbdprops; i3++) {
				dbdprop = (*(column.hdbdproparray))[i3];
				if (strlen(*cnct->nametable + dbdprop.value)) {
					fprintf(dbd, "\t\t\t\t\t<%s>", *cnct->nametable + dbdprop.key);
					writebuiltin(dbd, *cnct->nametable + dbdprop.value);
					fprintf(dbd, "</%s>\n", *cnct->nametable + dbdprop.key);
				}
				else {
					fprintf(dbd, "\t\t\t\t\t<%s/>\n", *cnct->nametable + dbdprop.key);
				}
			}
			if (column.numdbdprops) fprintf(dbd, "\t\t\t\t</properties>\n");
			fprintf(dbd, "\t\t\t\t<t>");
			if (column.type == TYPE_CHAR) {
				fprintf(dbd, "CHAR(%d)", column.fieldlen);
			}
			else if (column.type == TYPE_NUM || column.type == TYPE_POSNUM) {
				if (column.scale) fprintf(dbd, "NUM(%d,%d)", column.fieldlen - 1, column.scale);
				else fprintf(dbd, "NUM(%d)", column.fieldlen);
			}
			else if (column.type == TYPE_DATE || column.type == TYPE_TIME || column.type == TYPE_TIMESTAMP) {
				if (column.type == TYPE_DATE) fprintf(dbd, "DATE(");
				else if (column.type == TYPE_TIME) fprintf(dbd, "TIME(");
				else if (column.type == TYPE_TIMESTAMP) fprintf(dbd, "TIMESTAMP(");
				for (i3 = 0, i4 = column.fieldlen; i3 < i4; i3++) {
					c1 = (*cnct->nametable + column.format)[i3];
					if (c1 == FORMAT_DATE_YEAR) {
						fprintf(dbd, "%s", "YYYY");
						i4 -= 3;
					}
					else if (c1 == FORMAT_DATE_YEAR2) {
						fprintf(dbd, "%s", "YY");
						i4--;
					}
					else if (c1 == FORMAT_DATE_MONTH || c1 == FORMAT_TIME_MINUTES) {
						fprintf(dbd, "%s", "MM");
						i4--;
					}
					else if (c1 == FORMAT_DATE_DAY) {
						fprintf(dbd, "%s", "DD");
						i4--;
					}
					else if (c1 == FORMAT_TIME_HOUR) {
						fprintf(dbd, "%s", "HH");
						i4--;
					}
					else if (c1 == FORMAT_TIME_SECONDS) {
						fprintf(dbd, "%s", "SS");
						i4--;
					}
					else if (c1 == FORMAT_TIME_FRACTION1) {
						fprintf(dbd, "%c", 'F');
					}
					else if (c1 == FORMAT_TIME_FRACTION2) {
						fprintf(dbd, "%s", "FF");
						i4--;
					}
					else if (c1 == FORMAT_TIME_FRACTION3) {
						fprintf(dbd, "%s", "FFF");
						i4 -= 2;
					}
					else if (c1 == FORMAT_TIME_FRACTION4) {
						fprintf(dbd, "%s", "FFFF");
						i4 -= 3;
					}
					else fprintf(dbd, "%c", c1);
				}
				fprintf(dbd, ")");
			}
			fprintf(dbd, "</t>\n");
			fprintf(dbd, "\t\t\t\t<p>%d</p>\n", column.offset + 1); /* always store offset */
			if (column.description) {
				fprintf(dbd, "\t\t\t\t<d>");
				writebuiltin(dbd, *cnct->nametable + column.description);
				fprintf(dbd, "</d>\n");
			}
			if (column.label) fprintf(dbd, "\t\t\t\t<l>%s</l>\n", *cnct->nametable + column.label);
			if (column.type == TYPE_POSNUM) fprintf(dbd, "\t\t\t\t<nonnegative/>\n");
			fprintf(dbd, "\t\t\t</c>\n");
		}
		fprintf(dbd, "\t\t</columns>\n");
		fprintf(dbd, "\t</table>\n");
		fflush(dbd);
	}

	fprintf(dbd, "</dbcfsdbd>\n");
	fflush(dbd);
	fclose(dbd);
#if OS_WIN32
	/* copy temporary dbd over original, then delete the temporary dbd file */
	if (CopyFile(work, found, FALSE) == 0) {
		sprintf(cfgerrorstring, "failed to replace old DBD file with new DBD file, error = %d", (int) GetLastError());
		return RC_ERROR;
	}
	if (DeleteFile(work) == 0) {
		sprintf(cfgerrorstring, "failure deleting temp DBD file, error = %d", (int) GetLastError());
		return RC_ERROR;
	}
#else
	/* remove original dbd, then rename temp dbd to the original name */
	if (remove(found) == -1) {
		sprintf(cfgerrorstring, "failure removing DBD file, errno = %d", errno);
		return RC_ERROR;
	}
	if (rename(work, found) == -1) {
		sprintf(cfgerrorstring, "failure renaming temp DBD file, errno = %d", errno);
		return RC_ERROR;
	}
#endif
	return 0;
}

static int readdbdfile(CHAR *dbdfilename, CHAR *useraccess, CONNECTION *cnct)
{
	INT i1, i2, i3, accessflag, coloffset, tablealloc, columnalloc;
	INT filecompression, filenum, filereadonly, filetype, filevar, flag1;
	INT filtercolumnnamelength, length, linecnt, linefilter, linetable;
	INT nextlevel, nextoffset, rc, templatecnt, xmlflag, dbdstate;
	OFFSET filesize;
	CHAR filter[MAX_NAME_LENGTH + 2], format[32], name[MAX_NAME_LENGTH + 2];
	CHAR path[4096], work[MAX_NAMESIZE];
	CHAR *error, *filebuffer, *kw, *ptr, *val, **hmem;
	ELEMENT *element, *element2, *nextelement[4], *xmlbuffer;
	TABLE tab1;
	COLUMN col1;
	CFGVOLUME *cfgvol;
	NAMETABLE tempntable;


	strcpy(work, dbdfilename);
	miofixname(work, ".dbd", FIXNAME_EXT_ADD);
	if (cnct != NULL) {
		cnct->dbdfilename = (CHAR **) memalloc((INT)strlen(work) + 1, sizeof(CHAR *));
		if (cnct->dbdfilename == NULL) {
			strcpy(cfgerrorstring, "insufficient memory");
			return RC_ERROR;
		}
		strcpy(*cnct->dbdfilename, work);
	}
	filenum = fioopen(work, FIO_M_SRO | FIO_P_DBD);
	if (filenum < 0) {
		strcpy(cfgerrorstring, "DBD file open failed: ");
		strcat(cfgerrorstring, work);
		strcat(cfgerrorstring, ": ");
		strcat(cfgerrorstring, fioerrstr(filenum));
		return RC_ERROR;
	}
	fiogetsize(filenum, &filesize);
	filebuffer = (CHAR *) malloc((INT) filesize + sizeof(CHAR));
	if (filebuffer == NULL) {
		fioclose(filenum);
		strcpy(cfgerrorstring, "insufficient memory (malloc)");
		return RC_ERROR;
	}
	i1 = fioread(filenum, 0, (UCHAR *) filebuffer, (INT) filesize);
	fioclose(filenum);
	if (i1 != (INT) filesize) {
		free(filebuffer);
		strcpy(cfgerrorstring, "DBD file read error");
		return RC_ERROR;
	}
	for (i1 = 0; i1 < (INT) filesize && (isspace(filebuffer[i1]) || filebuffer[i1] == '\n' || filebuffer[i1] == '\r' || (UCHAR) filebuffer[i1] == DBCEOR); i1++);
	if (i1 < (INT) filesize && filebuffer[i1] == '<') {
		xmlbuffer = getxmldata(filebuffer + i1, (LONG) filesize - i1);
		free(filebuffer);
		if (xmlbuffer == NULL) return RC_ERROR;
		for (element = xmlbuffer; element != NULL && (element->cdataflag || strcmp(element->tag, "dbcfsdbd")); element = element->nextelement);
		if (element == NULL) {
			free(xmlbuffer);
			strcpy(cfgerrorstring, "DBD file does not contain 'dbcfsdbd' element");
			return RC_ERROR;
		}
		nextelement[0] = element->firstsubelement;
		nextlevel = 0;
		xmlflag = TRUE;
	}
	else {
		nextoffset = length = 0;
		xmlflag = FALSE;
	}

	dbdstate = DBDSTATE_BEFORETABLE;
	tablealloc = 0; /* applies to CONNECTION->numtables */
	tempntable.table = NULL;
	tempntable.alloc = 0;
	tempntable.size = 0;
	accessflag = FALSE;
	rc = RC_ERROR;  /* assume error */
	for (linecnt = 0; ; ) {
		if (!(dbdstate & (DBDSTATE_ONTABLE | DBDSTATE_FIRSTCOLUMN | DBDSTATE_FINISHTABLE | DBDSTATE_FINISHCOLUMN))) {
			if (xmlflag) {
				while (nextelement[nextlevel] == NULL) {
					if (--nextlevel < 0) break;
					if (nextlevel == 2) dbdstate = DBDSTATE_ONCOLUMN | DBDSTATE_FINISHCOLUMN;
					else if (nextlevel == 1) dbdstate = DBDSTATE_AFTERTABLE | (dbdstate & DBDSTATE_FINISHCOLUMN);
					else dbdstate = DBDSTATE_BEFORETABLE | DBDSTATE_FINISHTABLE | (dbdstate & DBDSTATE_FINISHCOLUMN);
				}
				if (nextlevel < 0) {
					if (dbdstate &= (DBDSTATE_FINISHTABLE | DBDSTATE_FINISHCOLUMN)) continue;
					rc = 0;  /* set success */
					break;
				}
				element = nextelement[nextlevel];
				nextelement[nextlevel] = element->nextelement;
				if (element->cdataflag) continue;
				kw = element->tag;
				if (element->firstsubelement != NULL && element->firstsubelement->cdataflag) val = element->firstsubelement->tag;
				else val = "";
			}
			else {
				if (nextoffset >= length) {
					if (nextoffset >= (INT) filesize) {
						if (dbdstate & (DBDSTATE_AFTERTABLE | DBDSTATE_AFTERCOLUMN)) {
							if (dbdstate == DBDSTATE_AFTERTABLE) dbdstate = DBDSTATE_FINISHTABLE;
							else dbdstate = DBDSTATE_FINISHTABLE | DBDSTATE_FINISHCOLUMN;
							continue;
						}
						rc = 0;  /* set success */
						break;
					}
					for (length = nextoffset; length < (INT) filesize && filebuffer[length] != '\n' && filebuffer[length] != '\r' && (UCHAR) filebuffer[length] != DBCEOR && (UCHAR) filebuffer[length] != DBCEOF; length++);
					if ((filebuffer[length] == '\r' && filebuffer[length + 1] == '\n') || ((UCHAR) filebuffer[length] == DBCEOR && (UCHAR) filebuffer[length + 1] == DBCEOF)) filebuffer[length + 1] = ' ';
					filebuffer[length] = '\0';
					linecnt++;
				}
				i1 = getnextkwval(filebuffer + nextoffset, &kw, &val, &error);
				if (i1 < 0) {
					syntaxerror(SYNTAX_CFG, linecnt, error, kw);
					break;
				}
				nextoffset += i1;
				if (!*kw) continue;
			}
		}
		dbdstate &= ~DBDSTATE_FIRSTCOLUMN;
		if (dbdstate & (DBDSTATE_FINISHTABLE | DBDSTATE_FINISHCOLUMN)) {
			if (dbdstate & DBDSTATE_FINISHCOLUMN) {  /* finish with column definition */
				if (col1.offset == 0xFFFF) col1.offset = (USHORT) coloffset;
				coloffset = col1.offset + col1.fieldlen;
				if (!columnalloc) {
					tab1.hcolumnarray = (HCOLUMN) memalloc((columnalloc += 24) * sizeof(COLUMN), 0);
					if (tab1.hcolumnarray == NULL) {
						strcpy(cfgerrorstring, "insufficient memory");
						break;
					}
				}
				else if (tab1.numcolumns == columnalloc) {
					if (memchange((UCHAR **) tab1.hcolumnarray, (columnalloc += 24) * sizeof(COLUMN), 0) < 0) {
						strcpy(cfgerrorstring, "insufficient memory");
						break;
					}
				}
				*(*tab1.hcolumnarray + tab1.numcolumns++) = col1;
			}
			if (dbdstate & DBDSTATE_FINISHTABLE) {  /* finished with the table */
				if (filtercolumnnamelength) {
					syntaxerror(SYNTAX_DBD, linefilter, "filter column not defined for table", NULL);
					break;
				}
				/*
				if (filereadonly) filetype |= RIO_M_SRA;
				else filetype |= RIO_M_SHR;
				*/
				filetype |= RIO_M_SHR;

				if (!filevar) filetype |= RIO_FIX;
				if (!filecompression) filetype |= RIO_UNC;
				if (!tab1.textfilename) {
					syntaxerror(SYNTAX_DBD, linetable, "textfile not specified for table", NULL);
					break;
				}
				tab1.rioopenflags = filetype | RIO_P_TXT;
				if (!tab1.reclength) {
					if (!filevar) tab1.reclength = -coloffset;
					else tab1.reclength = coloffset;
				}
				if (tab1.hcolumnarray != NULL) {
					memchange((UCHAR **) tab1.hcolumnarray, tab1.numcolumns * sizeof(COLUMN), 0);
				}
				if (cnct->numtables == tablealloc) {
					if (!tablealloc) {
						tablealloc = 48;
						cnct->htablearray = (HTABLE) memalloc(tablealloc * sizeof(TABLE), 0);
						if (cnct->htablearray == NULL) {
							strcpy(cfgerrorstring, "insufficient memory");
							break;
						}
					}
					else if (memchange((UCHAR **) cnct->htablearray, (tablealloc += 48) * sizeof(TABLE), 0) < 0) {
						strcpy(cfgerrorstring, "insufficient memory");
						break;
					}
				}
				*(*cnct->htablearray + cnct->numtables++) = tab1;
			}
			dbdstate &= ~(DBDSTATE_FINISHCOLUMN | DBDSTATE_FINISHTABLE);
			if (!dbdstate) {
				/* free unused tables */
				memchange((UCHAR **) cnct->htablearray, cnct->numtables * sizeof(TABLE), 0);
				cnct->numalltables = cnct->numtables;
				rc = 0;  /* set success */
				break;
			}
		}
		if (dbdstate == DBDSTATE_BEFORETABLE) {
			if (!strcmp(kw, "access")) {
				for (i1 = i2 = 0; val[i1]; i1++) if (!isspace(val[i1])) val[i2++] = (CHAR) toupper(val[i1]);
				val[i2] = '\0';
				if (!*val) {
					syntaxerror(SYNTAX_DBD, linecnt, "invalid access specification", NULL);
					break;
				}
				strcpy(dbdaccess, val);
				for (i1 = 0; useraccess[i1] && strcmp(useraccess + i1, val); i1 += (INT)strlen(useraccess + i1) + 1);
				if (useraccess[i1]) accessflag = TRUE;
			}
			else if (!strcmp(kw, "filepath")) {
				if (xmlflag) {
					for (i2 = 0, element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement) {
						if (element2->cdataflag) continue;
						if (!strcmp(element2->tag, "dir")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								if (i2) path[i2++] = ';';
								else fiosetopt(FIO_OPT_PREPPATH, (UCHAR *) element2->firstsubelement->tag);
								strcpy(path + i2, element2->firstsubelement->tag);
								i2 += (INT)strlen(element2->firstsubelement->tag);
							}
						}
					}
					if (i2 == 0) {
						syntaxerror(SYNTAX_DBD, linecnt, "filepath element needs at least one dir sub-element", NULL);
						break;
					}
					strcpy(filepath, path); /* was strcpy(filepath, val) */
					fiosetopt(FIO_OPT_OPENPATH, (UCHAR *) path);
				}
				else {
					strcpy(filepath, val);
					fiosetopt(FIO_OPT_OPENPATH, (UCHAR *) val);
					for (i1 = 0; val[i1] && val[i1] != ';'; i1++);
					val[i1] = 0;
					fiosetopt(FIO_OPT_PREPPATH, (UCHAR *) val);
				}
			}
			else if (!strcmp(kw, "volume")) {
				if (xmlflag) {
					for (i2 = 0, element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement) {
						if (element2->cdataflag) continue;
						if (!strcmp(element2->tag, "dir")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								if (i2) i2++;
								i2 += (INT)strlen(element2->firstsubelement->tag);
							}
						}
						else if (!strcmp(element2->tag, "name")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) val = element2->firstsubelement->tag;
							else val = "";
						}
					}
				}
				else {
					for (i1 = 0; val[i1] && !isspace(val[i1]); i1++);
					if (val[i1]) val[i1++] = '\0';
					for ( ; isspace(val[i1]); i1++);
					i2 = (INT)strlen(val + i1);
				}
				if (!val[0]) {
					syntaxerror(SYNTAX_DBD, linecnt, "invalid volume specification", NULL);
					break;
				}
				if (strlen(val) > MAX_VOLUMESIZE) {
					syntaxerror(SYNTAX_DBD, linecnt, "volume name too long", val);
					break;
				}
				if (!i2) {
					syntaxerror(SYNTAX_CFG, linecnt, "volume path missing", val);
					break;
				}
				for (i3 = 0; i3 < volcount && strcmp((*volumes)[i3].volume, val); i3++);
				if (i3 < volcount) {
					if (memchange((UCHAR **)(*volumes)[i3].path, i2 + 1, 0) == -1) {
						strcpy(cfgerrorstring, "insufficient memory");
						break;
					}
					hmem = (*volumes)[i3].path;
				}
				else {
					if (volcount == volalloc) {
						if (!volalloc) {
							volumes = (VOLUME **) memalloc(8 * sizeof(VOLUME), 0);
							if (volumes == NULL) {
								strcpy(cfgerrorstring, "insufficient memory");
								break;
							}
						}
						else if (memchange((UCHAR **) volumes, (volalloc + 8) * sizeof(VOLUME), 0) == -1) {
							strcpy(cfgerrorstring, "insufficient memory");
							break;
						}
						volalloc += 8;
					}
					strcpy((*volumes)[volcount].volume, val);
					hmem = (CHAR **) memalloc(i2 + 1, 0);
					if (hmem == NULL) {
						rioclose(filenum);
						strcpy(cfgerrorstring, "insufficient memory");
						break;
					}
					(*volumes)[volcount++].path = hmem;
				}
				if (xmlflag) {
					for (i2 = 0, element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement) {
						if (element2->cdataflag) continue;
						if (!strcmp(element2->tag, "dir")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								if (i2) (*hmem)[i2++] = ';';
								strcpy(*hmem + i2, element2->firstsubelement->tag);
								i2 += (INT)strlen(element2->firstsubelement->tag);
							}
						}
					}
				}
				else strcpy(*hmem, val + i1);
			}
/*** NOTE: THIS OVERWRITES STATIC WORKDIR, WHICH IS NOT A PROBLEM UNLESS SOME ***/
/***       DAY, WE SUPPORT MULTIPLE CONNECTIONS ***/
			else if (!strcmp(kw, "workdir")) {
				if (xmlflag) {
					for (element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement) {
						if (element2->cdataflag) continue;
						if (!strcmp(element2->tag, "dir")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag)
								strcpy(workdir, element2->firstsubelement->tag);
						}
					}
				}
				else strcpy(workdir, val);
			}
			else if (!strcmp(kw, "table")) {
				if (cnct == NULL) {
					if (xmlflag) continue;
					rc = 0;  /* set success */
					break;
				}
				  /* first entry needs to be "" */
				i1 = cfgaddname("", &tempntable.table, &tempntable.size, &tempntable.alloc);
				if (i1 == RC_ERROR) break;
				if (xmlflag) {
					if (nextlevel) {
						syntaxerror(SYNTAX_DBD, 0, "<table> defined at invalid level", NULL);
						break;
					}
					nextelement[++nextlevel] = element->firstsubelement;
				}
				dbdstate = DBDSTATE_ONTABLE;
			}
			else dbdstate = DBDSTATE_INVALIDKW;
		}
		else if (dbdstate == DBDSTATE_ONTABLE) {
			memset(&tab1, 0, sizeof(TABLE));
			flag1 = FALSE;
			filetype = RIO_T_ANY;
			filecompression = filevar = filereadonly = FALSE;
			filter[0] = '\0';
			filtercolumnnamelength = 0;
			linetable = linefilter = linecnt;
			coloffset = 0;
			columnalloc = 0;
			dbdstate = DBDSTATE_AFTERTABLE;
			if (!xmlflag) {
				if (!val[0]) {
					syntaxerror(SYNTAX_DBD, linecnt, "table name not specified", NULL);
					break;
				}
				for (i1 = templatecnt = 0; val[i1] && !isspace(val[i1]) && i1 <= MAX_NAME_LENGTH; i1++) {
					name[i1] = (CHAR) toupper(val[i1]);
					if (val[i1] == '?') templatecnt++;
				}
				if (i1 > MAX_NAME_LENGTH) {
					syntaxerror(SYNTAX_DBD, linecnt, "table name too long", val);
					break;
				}
				name[i1] = '\0';
				if (templatecnt) tab1.flags |= TABLE_FLAGS_TEMPLATE;
				if (i1 > cnct->maxtablename) cnct->maxtablename = i1;
				for (i1 = 0; i1 < cnct->numtables; i1++) {
					ptr = *tempntable.table + (*cnct->htablearray + i1)->name;
					for (i2 = 0; ptr[i2] && (ptr[i2] == name[i2] || ptr[i2] == '?' || (name[i2] && name[i2] == '?')); i2++);
					if (ptr[i2] == name[i2]) {
						syntaxerror(SYNTAX_DBD, linecnt, "table name already specified", ptr);
						break;
					}
				}
				if (i1 < cnct->numtables) break;
				tab1.name = cfgaddname(name, &tempntable.table, &tempntable.size, &tempntable.alloc);
				if (tab1.name == RC_ERROR) break;
			}
			else templatecnt = -1;
		}
		else if (dbdstate == DBDSTATE_AFTERTABLE) {
			if (xmlflag && !strcmp(kw, "name")) {
				if (!val[0]) {
					syntaxerror(SYNTAX_DBD, linecnt, "table name not specified", NULL);
					break;
				}
				for (i1 = i2 = 0; val[i1] && i1 <= MAX_NAME_LENGTH; i1++) {
					name[i1] = (CHAR) toupper(val[i1]);
					if (val[i1] == '?') i2++;
				}
				if (i1 > MAX_NAME_LENGTH) {
					syntaxerror(SYNTAX_DBD, linecnt, "table name too long", val);
					break;
				}
				name[i1] = '\0';
				if (templatecnt == -1) templatecnt = i2;
				if (i2 || (tab1.flags & TABLE_FLAGS_TEMPLATE)) {
					if (i2 != templatecnt) {
						syntaxerror(SYNTAX_DBD, linecnt, "table/file template do not match", NULL);
						break;
					}
					tab1.flags |= TABLE_FLAGS_TEMPLATE;
				}
				if (i1 > cnct->maxtablename) cnct->maxtablename = i1;
				for (i1 = 0; i1 < cnct->numtables; i1++) {
					ptr = *tempntable.table + (*cnct->htablearray + i1)->name;
					for (i2 = 0; ptr[i2] && (ptr[i2] == name[i2] || ptr[i2] == '?' || (name[i2] && name[i2] == '?')); i2++);
					if (ptr[i2] == name[i2]) {
						syntaxerror(SYNTAX_DBD, linecnt, "table name already specified", ptr);
						break;
					}
				}
				if (i1 < cnct->numtables) break;
				tab1.name = cfgaddname(name, &tempntable.table, &tempntable.size, &tempntable.alloc);
				if (tab1.name == RC_ERROR) break;
			}
			else if (!strcmp(kw, "textfile")) {
				if (tab1.textfilename) {
					syntaxerror(SYNTAX_DBD, linetable, "textfile keyword already specified for table",
							*tempntable.table + tab1.name);
					break;
				}
				if (!val[0]) {
					syntaxerror(SYNTAX_DBD, linecnt, "textfile name not specified", NULL);
					break;
				}
				for (i1 = i2 = 0; val[i1]; i1++) {
					if (val[i1] == '?') i2++;
					else if (i2) {
						if (val[i1] == '/' || val[i1] == '\\') tab1.flags |= TABLE_FLAGS_TEMPLATEDIR;
						else if (val[i1] == ':' && volmapfnc(val + i1 + 1, NULL)) {
							for (i3 = lastcfgvolume; i3 != -1; i3 = cfgvol->next) {
								cfgvol = cfgvolumes + i3;
								if (!strcmp(val + i1 + 1, cfgvol->volume)) break;
							}
							if (i3 == -1) tab1.flags |= TABLE_FLAGS_TEMPLATEDIR;
						}
					}
				}
				if (templatecnt == -1) templatecnt = i2;
				if (i2 || (tab1.flags & TABLE_FLAGS_TEMPLATE)) {
					if (i2 != templatecnt) {
						syntaxerror(SYNTAX_DBD, linecnt, "table/file(T) template do not match", NULL);
						break;
					}
					tab1.flags |= TABLE_FLAGS_TEMPLATE;
				}
				tab1.textfilename = cfgaddname(val, &tempntable.table, &tempntable.size, &tempntable.alloc);
				if (tab1.textfilename == RC_ERROR) break;
			}
			else if (!strcmp(kw, "indexfile")) {
				if (!val[0]) {
					syntaxerror(SYNTAX_DBD, linecnt, "indexfile name not specified", NULL);
					break;
				}
				for (i1 = i2 = 0; val[i1]; i1++) if (val[i1] == '?') i2++;
				if (templatecnt == -1) templatecnt = i2;
				if (i2 || (tab1.flags & TABLE_FLAGS_TEMPLATE)) {
					if (i2 != templatecnt) {
						syntaxerror(SYNTAX_DBD, linecnt, "table/file(I) template do not match", NULL);
						break;
					}
					tab1.flags |= TABLE_FLAGS_TEMPLATE;
				}
				if (i1 > cnct->maxtableindex) cnct->maxtableindex = i1;
				if (tab1.numindexes == 0) {
					tab1.hindexarray = (HINDEX) memalloc(sizeof(INDEX), MEMFLAGS_ZEROFILL);
					if (tab1.hindexarray == NULL) {
						strcpy(cfgerrorstring, "insufficient memory");
						break;
					}
				}
				else if (memchange((UCHAR **) tab1.hindexarray, (tab1.numindexes + 1) * sizeof(INDEX), MEMFLAGS_ZEROFILL) < 0) {
					strcpy(cfgerrorstring, "insufficient memory");
					break;
				}
				i1 = cfgaddname(val, &tempntable.table, &tempntable.size, &tempntable.alloc);
				if (i1 == RC_ERROR) break;
				(*tab1.hindexarray + tab1.numindexes++)->indexfilename = i1;
			}
			else if (!strcmp(kw, "textfiletype")) {
				if (!val[0]) {
					syntaxerror(SYNTAX_DBD, linecnt, "textfiletype value not specified", NULL);
					break;
				}
				if (filetype != RIO_T_ANY) {
					syntaxerror(SYNTAX_DBD, linecnt, "multiple textfiletype specification", NULL);
					break;
				}
				for (i1 = 0; val[i1]; i1++) val[i1] = (char) tolower(val[i1]);
				if (!strcmp(val, "standard") || !strcmp(val, "std")) filetype = RIO_T_STD;
				else if (!strcmp(val, "text")) filetype = RIO_T_TXT;
				else if (!strcmp(val, "data")) filetype = RIO_T_DAT;
				else if (!strcmp(val, "crlf")) filetype = RIO_T_DOS;
				else if (!strcmp(val, "mac")) filetype = RIO_T_MAC;
				else if (!strcmp(val, "binary") || !strcmp(val, "bin")) filetype = RIO_T_BIN;
				else {
					syntaxerror(SYNTAX_DBD, linecnt, "invalid textfiletype specification", NULL);
					break;
				}
			}
			else if (!strcmp(kw, "description")) {
				i1 = (INT)strlen(val);
				if (i1 > cnct->maxtableremarks) cnct->maxtableremarks = i1;
				tab1.description = cfgaddname(val, &tempntable.table, &tempntable.size, &tempntable.alloc);
				if (tab1.description == RC_ERROR) break;
			}
			else if (!strcmp(kw, "filter")) {
				if (tab1.filtervalue) {
					syntaxerror(SYNTAX_DBD, linecnt, "multiple filter specification", NULL);
					break;
				}
				if (xmlflag) {
					val = "";
					for (i2 = 0, element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement) {
						if (element2->cdataflag) continue;
						if (!strcmp(element2->tag, "column")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
								strcpy(filter, element2->firstsubelement->tag);
								filtercolumnnamelength = (INT)strlen(filter);
								for (i1 = 0; i1 < filtercolumnnamelength; i1++) filter[i1] = toupper(filter[i1]);
							}
						}
						else if (!strcmp(element2->tag, "value")) {
							if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) val = element2->firstsubelement->tag;
						}
					}
				}
				else {
					for (i1 = 0; val[i1] && !isspace(val[i1]) && i1 <= MAX_NAME_LENGTH; i1++) filter[i1] = (CHAR) toupper(val[i1]);
					filter[i1] = '\0';
					filtercolumnnamelength = i1;
					while (isspace(val[i1])) i1++;
					for (i2 = 0; val[i1]; ) {
						if (val[i1] == '"') {
							if (val[++i1] != '"') {
								for ( ; ; ) {
									if (!val[i1]) break;  /* getnextkwval should not happen */
									if (val[i1] == '"') {
										if (val[++i1] != '"') break;
									}
									val[i2++] = val[i1++];
								}
								continue;
							}
						}
						val[i2++] = val[i1++];
					}
					val[i2] = '\0';
				}
				if (!filtercolumnnamelength) {
					syntaxerror(SYNTAX_DBD, linecnt, "filter column not specified", NULL);
					break;
				}
				if (filtercolumnnamelength > MAX_NAME_LENGTH) {
					syntaxerror(SYNTAX_DBD, linecnt, "filter column name too long", filter);
					break;
				}
				if (!val[0]) {
					syntaxerror(SYNTAX_DBD, linecnt, "filter value not specified", NULL);
					break;
				}
				tab1.filtervalue = cfgaddname(val, &tempntable.table, &tempntable.size, &tempntable.alloc);
				if (tab1.filtervalue == RC_ERROR) break;
				tab1.flags |= TABLE_FLAGS_READONLY;
				linefilter = linecnt;
			}
			else if (!strcmp(kw, "compressed")) {
				if (!filevar && flag1) {
					syntaxerror(SYNTAX_DBD, linecnt, "conflicting record types", NULL);
					break;
				}
				filecompression = filevar = TRUE;
				tab1.flags |= TABLE_FLAGS_NOUPDATE;
			}
			else if (!strcmp(kw, "readonly")) {
				filereadonly = TRUE;
				tab1.flags |= TABLE_FLAGS_READONLY;
			}
			else if (!strcmp(kw, "fixedlength")) {
				if (flag1 || filecompression) {
					syntaxerror(SYNTAX_DBD, linecnt, "conflicting record types", NULL);
					break;
				}
				flag1 = TRUE;
				if (val[0]) tab1.reclength = atoi(val);
			}
			else if (!strcmp(kw, "variablelength")) {
				if (!val[0]) {
					syntaxerror(SYNTAX_DBD, linecnt, "variablelength value not specified", NULL);
					break;
				}
				if (flag1) {
					syntaxerror(SYNTAX_DBD, linecnt, "conflicting record types", NULL);
					break;
				}
				flag1 = TRUE;
				tab1.reclength = atoi(val);
				filevar = TRUE;
			}
			else if (xmlflag) {
				if (!strcmp(kw, "columns")) {
					if (nextlevel != 1) {
						syntaxerror(SYNTAX_DBD, 0, "<columns> defined at invalid level", NULL);
						break;
					}
					nextelement[++nextlevel] = element->firstsubelement;
					dbdstate = DBDSTATE_ONCOLUMN;
				}
				else if (!strcmp(kw, "properties")) { /* table properties */
					/* store property count in tab1.numdbdprops */
					for (tab1.numdbdprops = 0, element2 = element->firstsubelement; element2 != NULL; tab1.numdbdprops++) {
						element2 = element2->nextelement;
					}
					/* allocate memory */
					tab1.hdbdproparray = (HDBDPROP) memalloc(tab1.numdbdprops * sizeof(DBDPROP), 0);
					if (tab1.hdbdproparray == NULL) {
						strcpy(cfgerrorstring, "insufficient memory");
						break;
					}
					/* assign key value pairs */
					for (i2 = 0, element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement, i2++) {
						if (element2->cdataflag) continue;
						(*tab1.hdbdproparray + i2)->key =
							cfgaddname(element2->tag, &tempntable.table, &tempntable.size, &tempntable.alloc);
						if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
							(*tab1.hdbdproparray + i2)->value =
								cfgaddname(element2->firstsubelement->tag, &tempntable.table, &tempntable.size, &tempntable.alloc);
						}
						else (*tab1.hdbdproparray + i2)->value =
							cfgaddname("", &tempntable.table, &tempntable.size, &tempntable.alloc);
					}
				}
				else dbdstate = DBDSTATE_INVALIDKW;
			}
			else if (!strcmp(kw, "column")) dbdstate = DBDSTATE_ONCOLUMN | DBDSTATE_FIRSTCOLUMN;
			else if (!strcmp(kw, "table")) dbdstate = DBDSTATE_ONTABLE | DBDSTATE_FINISHTABLE;  /* columnless table */
			else dbdstate = DBDSTATE_INVALIDKW;
		}
		else if (dbdstate == DBDSTATE_ONCOLUMN) {
			if (xmlflag) {
				if (kw[0] != 'c' || kw[1]) {
					syntaxerror(SYNTAX_DBD, 0, "expected <c> element", NULL);
					break;
				}
				if (nextlevel != 2) {
					syntaxerror(SYNTAX_DBD, 0, "<c> defined at invalid level", NULL);
					break;
				}
				nextelement[++nextlevel] = element->firstsubelement;
			}
			memset(&col1, 0, sizeof(COLUMN));
			col1.offset = 0xFFFF;
			if (!xmlflag) {
				if (!val[0]) {
					syntaxerror(SYNTAX_DBD, linecnt, "missing column name", NULL);
					break;
				}
				for (i1 = 0; val[i1] && !isspace(val[i1]) && i1 <= MAX_NAME_LENGTH; i1++) name[i1] = (CHAR) toupper(val[i1]);
				if (i1 > MAX_NAME_LENGTH) {
					syntaxerror(SYNTAX_DBD, linecnt, "column name too long", val);
					break;
				}
				name[i1] = '\0';
				if (i1 > cnct->maxcolumnname) cnct->maxcolumnname = i1;
				if (i1 == filtercolumnnamelength && !memcmp(name, filter, i1)) {
					tab1.filtercolumnnum = tab1.numcolumns + 1;
					filtercolumnnamelength = 0;
				}
				col1.name = cfgaddname(name, &tempntable.table, &tempntable.size, &tempntable.alloc);
				if (col1.name == RC_ERROR) break;
				if (coltype(val + i1, linecnt, &col1, format) == -1) break;
				if (format[0]) {
					col1.format = cfgaddname(format, &tempntable.table, &tempntable.size, &tempntable.alloc);
					if (col1.format == RC_ERROR) break;
				}
			}
			dbdstate = DBDSTATE_AFTERCOLUMN;
		}
		else if (dbdstate == DBDSTATE_AFTERCOLUMN) {
			if ((xmlflag && kw[0] == 'p' && !kw[1]) || (!xmlflag && !strcmp(kw, "position"))) {
				if (!val[0]) {
					syntaxerror(SYNTAX_DBD, linecnt, "missing position value", NULL);
					break;
				}
				for (i1 = i3 = 0; isdigit(val[i1]); i1++)
					i3 = i3 * 10 + val[i1] - '0';
				if (!i3 || val[i1]) {
					syntaxerror(SYNTAX_DBD, linecnt, "invalid position specification", NULL);
					break;
				}
				col1.offset = (USHORT)(i3 - 1);
			}
			else if ((xmlflag && kw[0] == 'd' && !kw[1]) || (!xmlflag && !strcmp(kw, "description"))) {
				i1 = (INT)strlen(val);
				if (i1 > cnct->maxcolumnremarks) cnct->maxcolumnremarks = i1;
				col1.description = cfgaddname(val, &tempntable.table, &tempntable.size, &tempntable.alloc);
				if (col1.description == RC_ERROR) break;
			}
			else if ((xmlflag && kw[0] == 'l' && !kw[1]) || (!xmlflag && !strcmp(kw, "label"))) {
				for (i1 = 0; val[i1] && i1 <= MAX_NAME_LENGTH; i1++);
				if (i1 > MAX_NAME_LENGTH) {
					syntaxerror(SYNTAX_DBD, linecnt, "column label too long", val);
					break;
				}
				col1.label = cfgaddname(val, &tempntable.table, &tempntable.size, &tempntable.alloc);
				if (col1.label == RC_ERROR) break;
			}
			else if (!strcmp(kw, "nonnegative")) {
				if (col1.type != TYPE_NUM) {
					syntaxerror(SYNTAX_DBD, linecnt, "nonnegative only valid with numeric types", NULL);
					break;
				}
				col1.type = TYPE_POSNUM;
			}
			else if (xmlflag) {
				if (kw[0] == 'n' && !kw[1]) {
					if (!val[0]) {
						syntaxerror(SYNTAX_DBD, linecnt, "missing column name", NULL);
						break;
					}
					for (i1 = 0; val[i1] && !isspace(val[i1]) && i1 <= MAX_NAME_LENGTH; i1++) name[i1] = (CHAR) toupper(val[i1]);
					if (i1 > MAX_NAME_LENGTH) {
						syntaxerror(SYNTAX_DBD, linecnt, "column name too long", val);
						break;
					}
					name[i1] = '\0';
					if (i1 > cnct->maxcolumnname) cnct->maxcolumnname = i1;
					if (i1 == filtercolumnnamelength && !memcmp(name, filter, i1)) {
						tab1.filtercolumnnum = tab1.numcolumns + 1;
						filtercolumnnamelength = 0;
					}
					col1.name = cfgaddname(name, &tempntable.table, &tempntable.size, &tempntable.alloc);
					if (col1.name == RC_ERROR) break;
				}
				else if (kw[0] == 't' && !kw[1]) {
					if (coltype(val, 0, &col1, format) == -1) break;
					if (format[0]) {
						col1.format = cfgaddname(format, &tempntable.table, &tempntable.size, &tempntable.alloc);
						if (col1.format == RC_ERROR) break;
					}
				}
				else if (!strcmp(kw, "properties")) { /* column properties */
					/* store property count in tab1.numdbdprops */
					for (col1.numdbdprops = 0, element2 = element->firstsubelement; element2 != NULL; col1.numdbdprops++) {
						element2 = element2->nextelement;
					}
					/* allocate memory */
					col1.hdbdproparray = (HDBDPROP) memalloc(col1.numdbdprops * sizeof(DBDPROP), 0);
					if (col1.hdbdproparray == NULL) {
						strcpy(cfgerrorstring, "insufficient memory");
						break;
					}
					/* assign key value pairs */
					for (i2 = 0, element2 = element->firstsubelement; element2 != NULL; element2 = element2->nextelement, i2++) {
						if (element2->cdataflag) continue;
						(*col1.hdbdproparray + i2)->key =
							cfgaddname(element2->tag, &tempntable.table, &tempntable.size, &tempntable.alloc);
						if (element2->firstsubelement != NULL && element2->firstsubelement->cdataflag) {
							(*col1.hdbdproparray + i2)->value =
								cfgaddname(element2->firstsubelement->tag, &tempntable.table, &tempntable.size, &tempntable.alloc);
						}
						else (*col1.hdbdproparray + i2)->value =
							cfgaddname("", &tempntable.table, &tempntable.size, &tempntable.alloc);
					}
				}
				else dbdstate = DBDSTATE_INVALIDKW;
			}
			else if (!strcmp(kw, "column")) dbdstate = DBDSTATE_ONCOLUMN | DBDSTATE_FINISHCOLUMN;
			else if (!strcmp(kw, "table")) dbdstate = DBDSTATE_ONTABLE | DBDSTATE_FINISHCOLUMN | DBDSTATE_FINISHTABLE;
			else dbdstate = DBDSTATE_INVALIDKW;
		}
		if (dbdstate == DBDSTATE_INVALIDKW) {
			syntaxerror(SYNTAX_DBD, linecnt, "invalid keyword", kw);
			break;
		}
	}
	if (xmlflag) free(xmlbuffer);
	else free(filebuffer);
	if (!accessflag) {
		strcpy(cfgerrorstring, "user access denied");
		rc = RC_ERROR;
	}
	if (rc == RC_ERROR) {
		if (tempntable.table != NULL) memfree((UCHAR **) tempntable.table);
		if (cnct != NULL) {
/*** CODE: CLEAN UP CONNECTION ***/
		}
	}
	else if (cnct != NULL) {
		cnct->nametable = tempntable.table;
		cnct->nametablealloc = tempntable.alloc;
		cnct->nametablesize = tempntable.size;
	}
	return rc;
}

INT coltype(CHAR *val, INT linecnt, COLUMN *col1, CHAR *format)
{
	INT i1, i2, i3, i4, monthflag, rmsflag;
	CHAR work[128];

	format[0] = '\0';
	for (i1 = i2 = 0; val[i1]; i1++) if (!isspace(val[i1])) work[i2++] = (CHAR) toupper(val[i1]);
	work[i2] = '\0';

	/* The string 'work' now has a 'squeezed' and capitalized
	 * version of the input spec
	 */
	if (!i2) {
		syntaxerror(SYNTAX_DBD, linecnt, "column type not specified", NULL);
		return RC_ERROR;
	}

	/*
	 * Make sure that the spec contains at least a left parens
	 */
	for (i1 = 0; work[i1] && work[i1] != '('; i1++);
	if (work[i1] != '(') {
		syntaxerror(SYNTAX_DBD, linecnt, "column length not specified", NULL);
		return RC_ERROR;
	}

	/*
	 * Replace the left parens with a NULL
	 */
	work[i1++] = '\0';

	if (!strcmp(work, "CHAR") || !strcmp(work, "CHARACTER")) {
		for (i2 = 0; isdigit(work[i1]); i1++) i2 = i2 * 10 + work[i1] - '0';
		if (!i2 || work[i1] != ')') {
			syntaxerror(SYNTAX_DBD, linecnt, "invalid character length specification", NULL);
			return RC_ERROR;
		}
		col1->fieldlen = (USHORT) i2;
		col1->length = (USHORT) i2;
		col1->type = TYPE_CHAR;
	}
	else if (!strcmp(work, "NUM") || !strcmp(work, "NUMERIC") || !strcmp(work, "DEC") || !strcmp(work, "DECIMAL")) {
		/* i1 is pointing to the 1st character after the left parens */
		for (i2 = 0; isdigit(work[i1]); i1++) i2 = i2 * 10 + work[i1] - '0';

		/* The character that terminates the first string of digits (which might be empty!)
		 * must be a ',' or '.' or ')'
		 */
		if (work[i1] != ')' && work[i1] != ',' && work[i1] != '.') {
			syntaxerror(SYNTAX_DBD, linecnt, "invalid numeric length specification", NULL);
			return RC_ERROR;
		}

		/* i2 can be zero only if the terminating character was a '.' */
		if (i2 == 0 && work[i1] != '.') {
			syntaxerror(SYNTAX_DBD, linecnt, "invalid numeric length specification", NULL);
			return RC_ERROR;
		}

		col1->fieldlen = (USHORT) i2;
		col1->length = (USHORT) i2;
		i3 = 0;
		if (work[i1] == ',' || work[i1] == '.') {
			if (work[i1] == '.') rmsflag = TRUE;
			else rmsflag = FALSE;
			for (i1++, i3 = 0; isdigit(work[i1]); i1++) i3 = i3 * 10 + work[i1] - '0';
			if ((!rmsflag && i3 > i2) || work[i1] != ')') {
				syntaxerror(SYNTAX_DBD, linecnt, "invalid numeric scale specification", NULL);
				return RC_ERROR;
			}
			col1->scale = (char) i3;
			if (rmsflag) {
				/* size was specified as DB/C x.y so convert to FS x,y size */
				col1->fieldlen += i3;
				col1->length += i3;
			}
		}
		if (col1->scale) {
			col1->fieldlen++;
			col1->length++;
		}
		col1->type = TYPE_NUM;
	}
	else if (!strcmp(work, "DATE")) {
		for (i2 = i3 = 0; work[i1] && work[i1] != ')'; i1++) {
			if (i3 == 5) break;
			if (work[i1] == 'Y') {
				for (i4 = 1; work[i1 + i4] == 'Y'; i4++);
				if (i4 == 4) format[i3++] = FORMAT_DATE_YEAR;
				else if (i4 == 2) format[i3++] = FORMAT_DATE_YEAR2;
				else break;
				i2 += i4;
				i1 += i4 - 1;
			}
			else if (work[i1] == 'M') {
				for (i4 = 1; work[i1 + i4] == 'M'; i4++);
				if (i4 == 2) format[i3++] = FORMAT_DATE_MONTH;
				else break;
				i2 += 2;
				i1++;
			}
			else if (work[i1] == 'D') {
				for (i4 = 1; work[i1 + i4] == 'D'; i4++);
				if (i4 == 2) format[i3++] = FORMAT_DATE_DAY;
				else break;
				i2 += 2;
				i1++;
			}
			else if (work[i1] == '/' || work[i1] == '-' || work[i1] == '.' || work[i1] == ':') {
				format[i3++] = work[i1];
				i2++;
			}
			else break;
		}
		format[i3] = '\0';
		if (!i2 || work[i1] != ')') {
			syntaxerror(SYNTAX_DBD, linecnt, "invalid date specification", NULL);
			return RC_ERROR;
		}
		col1->fieldlen = (USHORT) i2;
		col1->length = 10;
		col1->type = TYPE_DATE;
	}
	else if (!strcmp(work, "TIME")) {
		for (i2 = i3 = 0; work[i1] && work[i1] != ')'; i1++) {
			if (i3 == 7) break;
			if (work[i1] == 'H') {
				for (i4 = 1; work[i1 + i4] == 'H'; i4++);
				if (i4 == 2) format[i3++] = FORMAT_TIME_HOUR;
				else break;
				i2 += i4;
				i1 += i4 - 1;
			}
			else if (work[i1] == 'M') {
				for (i4 = 1; work[i1 + i4] == 'M'; i4++);
				if (i4 == 2) format[i3++] = FORMAT_TIME_MINUTES;
				else break;
				i2 += 2;
				i1++;
			}
			else if (work[i1] == 'S') {
				for (i4 = 1; work[i1 + i4] == 'S'; i4++);
				if (i4 == 2) format[i3++] = FORMAT_TIME_SECONDS;
				else break;
				i2 += 2;
				i1++;
			}
			else if (work[i1] == 'F') {
				for (i4 = 1; work[i1 + i4] == 'F'; i4++);
				if (i4 >= 1 && i4 <= 4) format[i3++] = FORMAT_TIME_FRACTION1 + i4 - 1;
				else break;
				i2 += i4;
				i1++;
			}
			else if (work[i1] == '/' || work[i1] == '-' || work[i1] == '.' || work[i1] == ':') {
				format[i3++] = work[i1];
				i2++;
			}
			else break;
		}
		format[i3] = '\0';
		if (!i2 || work[i1] != ')') {
			syntaxerror(SYNTAX_DBD, linecnt, "invalid time specification", NULL);
			return RC_ERROR;
		}
		col1->fieldlen = (USHORT) i2;
		col1->length = 12;
		col1->scale = 3;
		col1->type = TYPE_TIME;
	}
	else if (!strcmp(work, "TIMESTAMP")) {
		monthflag = TRUE;
		for (i2 = i3 = 0; work[i1] && work[i1] != ')'; i1++) {
			if (i3 == 13) break;
			if (work[i1] == 'Y') {
				for (i4 = 1; work[i1 + i4] == 'Y'; i4++);
				if (i4 == 4) format[i3++] = FORMAT_DATE_YEAR;
				else if (i4 == 2) format[i3++] = FORMAT_DATE_YEAR2;
				else break;
				i2 += i4;
				i1 += i4 - 1;
			}
			else if (work[i1] == 'M') {
				for (i4 = 1; work[i1 + i4] == 'M'; i4++);
				if (i4 == 2) {
					if (monthflag) format[i3++] = FORMAT_DATE_MONTH;
					else {
						format[i3++] = FORMAT_TIME_MINUTES;
						monthflag = TRUE;
					}
				}
				else break;
				i2 += 2;
				i1++;
			}
			else if (work[i1] == 'D') {
				for (i4 = 1; work[i1 + i4] == 'D'; i4++);
				if (i4 == 2) format[i3++] = FORMAT_DATE_DAY;
				else break;
				i2 += 2;
				i1++;
			}
			else if (work[i1] == 'H') {
				for (i4 = 1; work[i1 + i4] == 'H'; i4++);
				if (i4 == 2) format[i3++] = FORMAT_TIME_HOUR;
				else break;
				i2 += i4;
				i1 += i4 - 1;
				monthflag = FALSE;
			}
			else if (work[i1] == 'S') {
				for (i4 = 1; work[i1 + i4] == 'S'; i4++);
				if (i4 == 2) format[i3++] = FORMAT_TIME_SECONDS;
				else break;
				i2 += 2;
				i1++;
			}
			else if (work[i1] == 'F') {
				for (i4 = 1; work[i1 + i4] == 'F'; i4++);
				if (i4 >= 1 && i4 <= 4) format[i3++] = FORMAT_TIME_FRACTION1 + i4 - 1;
				else break;
				i2 += i4;
				i1 += i4 - 1;
			}
			else if (work[i1] == '/' || work[i1] == '-' || work[i1] == '.' || work[i1] == ':') {
				format[i3++] = work[i1];
				i2++;
			}
			else break;
		}
		format[i3] = '\0';
		if (!i2 || work[i1] != ')') {
			syntaxerror(SYNTAX_DBD, linecnt, "invalid timestamp specification", NULL);
			return RC_ERROR;
		}
		col1->fieldlen = (USHORT) i2;
		col1->length = 23;
		col1->scale = 3;
		col1->type = TYPE_TIMESTAMP;
	}
	else {
		syntaxerror(SYNTAX_DBD, linecnt, "invalid column type specification", work);
		return RC_ERROR;
	}
	return 0;
}

/*
 * Returns a pointer to the topmost ELEMENT if successful, NULL if otherwise
 */
static ELEMENT *getxmldata(CHAR *filebuffer, LONG filesize)
{
	INT i1, i2, i3;
	CHAR *ptr;
	ELEMENT *xmlbuffer;

	for (i1 = i2 = i3 = 0; i1 < filesize; ) {
		for ( ; i1 < filesize && filebuffer[i1] != '\n'
				&& filebuffer[i1] != '\r' && (UCHAR) filebuffer[i1] != DBCEOR
				&& (UCHAR) filebuffer[i1] != DBCEOF; i1++)
		{
			if (filebuffer[i1] == '<' && i1 + 1 < filesize && (filebuffer[i1 + 1] == '!' || filebuffer[i1 + 1] == '?'))
			{
				/* comment or meta data */
				if (filebuffer[++i1] == '!') {  /* <!-- anything --> */
					for (i1++; i1 + 2 < filesize && (filebuffer[i1] != '-' || filebuffer[i1 + 1] != '-' || filebuffer[i1 + 2] != '>'); i1++);
					if ((i1 += 2) >= filesize) {  /* invalid comment */
						strcpy(cfgerrorstring, "invalid XML comment");
						return NULL;
					}
				}
				else {  /* <?tagname [attribute="value" ...] ?> */
					for (i1++; i1 + 1 < filesize && (filebuffer[i1] != '?' || filebuffer[i1 + 1] != '>'); i1++);
					if (++i1 >= filesize) {  /* invalid meta data */
						strcpy(cfgerrorstring, "invalid XML meta data");
						return NULL;
					}
				}
				continue;
			}
			filebuffer[i2++] = filebuffer[i1];
			if (!isspace(filebuffer[i1])) i3 = i2;
		}
		for (i2 = i3; i1 < filesize && (isspace(filebuffer[i1]) || filebuffer[i1] == '\n'
				|| filebuffer[i1] == '\r' || (UCHAR) filebuffer[i1] == DBCEOR
				|| (UCHAR) filebuffer[i1] == DBCEOF); i1++);
	}
	for (i3 = 512; i3 < i2; i3 <<= 1);
	xmlbuffer = (ELEMENT *) malloc(i3);
	if (xmlbuffer == NULL) {
		strcpy(cfgerrorstring, "insufficient memory (malloc)");
		return NULL;
	}
	while ((i1 = xmlparse(filebuffer, i2, xmlbuffer, i3)) == -1) {
		ptr = (CHAR *) realloc(xmlbuffer, i3 << 1);
		if (ptr == NULL) {
			free(xmlbuffer);
			strcpy(cfgerrorstring, "insufficient memory (realloc)");
			return NULL;
		}
		xmlbuffer = (ELEMENT *) ptr;
		i3 <<= 1;
	}
	if (i1 < 0) {
		free(xmlbuffer);
		strcpy(cfgerrorstring, "CFG/DBD file contains invalid XML format: ");
		strcat(cfgerrorstring, xmlgeterror());
		return NULL;
	}
	return xmlbuffer;
}

static INT getnextkwval(CHAR *record, CHAR **kw, CHAR **val, CHAR **error)
{
	INT i2, i3, cnt, filterflag, parenlevel;

	*kw = *val = *error = "";
	for (cnt = 0; isspace(record[cnt]); cnt++);
	if (!record[cnt]) return cnt + 1;
	if (record[cnt] == '-' && record[cnt + 1] == '-') return cnt + (INT)strlen(record + cnt) + 1;

	for (*kw = record + cnt; record[cnt] && record[cnt] != '=' && !isspace(record[cnt]) && record[cnt] != ','; cnt++)
		record[cnt] = (CHAR) tolower(record[cnt]);
	for (i2 = cnt; isspace(record[cnt]); cnt++);
	if (!record[cnt]) {  /* only keyword */
		record[i2] = '\0';  /* truncate any spaces */
		return cnt + 1;
	}

	if (record[cnt] == '=') {
		record[i2] = '\0';  /* terminate key word */
		for (cnt++; isspace(record[cnt]); cnt++);
		/* assume no keywords follow description */
		if (!strcmp(*kw, "description")) {
			*val = record + cnt;
			cnt += (INT)strlen(record + cnt);
			for (i2 = cnt - 1; isspace(record[i2]); i2--);
			record[i2 + 1] = '\0';  /* truncate spaces */
			return cnt + 1;
		}
/*** SHOULD KEYWORD= WITH NO VALUE BE CONSIDERED AN ERROR ? ***/
		if (!record[cnt] || record[cnt] == ',') return cnt + 1;
		*val = record + cnt;
		if (!strcmp(*kw, "filter")) filterflag = 1;
		else filterflag = 0;
		parenlevel = 0;
		for (i2 = cnt; ; ) {
			if (record[cnt] == '"') {
				if (filterflag == 2) record[i2++] = '"';  /* keep quotes intact for filter */
				if (record[++cnt] != '"') {
					for ( ; ; ) {
						if (!record[cnt]) {
							*error = "missing closing quote";
							return RC_ERROR;
						}
						if (record[cnt] == '"') {
							if (filterflag == 2) record[i2++] = '"';  /* keep quotes intact for filter */
							if (record[++cnt] != '"') break;
						}
						record[i2++] = record[cnt++];
					}
					continue;
				}
			}
			if (!parenlevel) {  /* check if end of value */
				i3 = cnt;
				if (isspace(record[cnt])) {
					while (isspace(record[cnt])) cnt++;
					if (filterflag == 1) filterflag = 2;
				}
				if (!record[cnt] || record[cnt] == ',' ||
				    (record[cnt] == '-' && record[cnt + 1] == '-')) break;
				if (i3 != cnt) cnt--;  /* just keep one space */
			}
			else if (!record[cnt]) {
				*error = "missing closing parentheses";
				return RC_ERROR;
			}
			if (record[cnt] == '(') parenlevel++;
			else if (record[cnt] == ')') {
				if (!parenlevel) {
					*error = "missing opening parentheses";
					return RC_ERROR;
				}
				parenlevel--;
			}
			record[i2++] = record[cnt++];
		}
	}
	if (record[cnt] != ',') cnt += (INT)strlen(record + cnt);  /* ignoring rest of line, even if not comment */
	record[i2] = '\0';  /* terminate key word or value */
	return cnt + 1;
}

/* merge the information from the cfg and dbd files */
static INT fixupvolmap()
{
	INT i1;
	CHAR **hmem;
	CFGVOLUME *cfgvol;

	for (i1 = lastcfgvolume; i1 != -1; i1 = cfgvol->next) {
		cfgvol = cfgvolumes + i1;
		if (volmapfnc(cfgvol->volume, NULL)) {
			if (volcount == volalloc) {
				if (!volalloc) {
					volumes = (VOLUME **) memalloc(8 * sizeof(VOLUME), 0);
					if (volumes == NULL) {
						strcpy(cfgerrorstring, "insufficient memory");
						return RC_ERROR;
					}
				}
				else if (memchange((UCHAR **) volumes, (volalloc + 8) * sizeof(VOLUME), 0) == -1) {
					strcpy(cfgerrorstring, "insufficient memory");
					return RC_ERROR;
				}
				volalloc += 8;
			}
			strcpy((*volumes)[volcount].volume, cfgvol->volume);
			hmem = (char **) memalloc((INT)strlen(cfgvol->path) + 1, 0);
			if (hmem == NULL) {
				strcpy(cfgerrorstring, "insufficient memory");
				return RC_ERROR;
			}
			strcpy(*hmem, cfgvol->path);
			(*volumes)[volcount++].path = hmem;
		}
	}
	return 0;
}

static int volmapfnc(char *vol, char ***path)
{
	int i1;

	for (i1 = 0; i1 < volcount; i1++) {
		if (strcmp(vol, (*volumes)[i1].volume)) continue;
		if (path != NULL) *path = (*volumes)[i1].path;
		return 0;
	}
	return RC_ERROR;
}

static INT translate(CHAR *ptr, UCHAR *map) // @suppress("No return")
{
	int i1, i2, i3;

	for ( ; ; ) {
		while (isspace(*ptr)) ptr++;
		if (*ptr == ';') {
			ptr++;
			continue;
		}
		if (!*ptr) return 0;
		if (!isdigit(*ptr)) return RC_ERROR;
		for (i1 = 0; isdigit(*ptr); ptr++) i1 = i1 * 10 + *ptr - '0';
		while (isspace(*ptr)) ptr++;
		if (*ptr == '-') {
			ptr++;
			while (isspace(*ptr)) ptr++;
			if (!isdigit(*ptr)) return RC_ERROR;
			for (i2 = 0; isdigit(*ptr); ptr++) i2 = i2 * 10 + *ptr - '0';
			while (isspace(*ptr)) ptr++;
		}
		else i2 = i1;
		if (*ptr != ':') return RC_ERROR;
		ptr++;
		while (isspace(*ptr)) ptr++;
		if (!isdigit(*ptr)) return RC_ERROR;
		for (i3 = 0; isdigit(*ptr); ptr++) i3 = i3 * 10 + *ptr - '0';
		if (i1 > UCHAR_MAX || i2 > UCHAR_MAX || i3 + (i2 - i1) > UCHAR_MAX) return RC_ERROR;
		while (i1 <= i2) map[i1++] = (UCHAR) i3++;
	}
}

static void syntaxerror(INT type, INT line, CHAR *msg1, CHAR *msg2)
{
	INT i1, i2;
	CHAR work[32];

	if (type == SYNTAX_CFG) strcpy(cfgerrorstring, "CFG syntax error");
	else if (type == SYNTAX_DBD) strcpy(cfgerrorstring, "DBD syntax error");
	else strcpy(cfgerrorstring, "syntax error");
	if (line) {
		strcat(cfgerrorstring, ": line ");
		mscitoa(line, work);
		strcat(cfgerrorstring, work);
	}
	if (msg1 != NULL && *msg1) {
		i1 = (INT)strlen(cfgerrorstring);
		if (i1 < (INT) sizeof(cfgerrorstring) - 2) {
			cfgerrorstring[i1++] = ':';
			cfgerrorstring[i1++] = ' ';
			i2 = (INT)strlen(msg1);
			if (i2 > (INT) sizeof(cfgerrorstring) - 1 - i1) i2 = sizeof(cfgerrorstring) - 1 - i1;
			memcpy(cfgerrorstring + i1, msg1, i2);
			cfgerrorstring[i1 + i2] = '\0';
		}
	}
	if (msg2 != NULL && *msg2) {
		i1 = (INT)strlen(cfgerrorstring);
		if (i1 < (INT) sizeof(cfgerrorstring) - 2) {
			cfgerrorstring[i1++] = ':';
			cfgerrorstring[i1++] = ' ';
			i2 = (INT)strlen(msg2);
			if (i2 > (INT) sizeof(cfgerrorstring) - 1 - i1) i2 = sizeof(cfgerrorstring) - 1 - i1;
			memcpy(cfgerrorstring + i1, msg2, i2);
			cfgerrorstring[i1 + i2] = '\0';
		}
	}
}
