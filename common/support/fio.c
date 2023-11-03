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
#define INC_CTYPE
#define INC_STDLIB
#define INC_LIMITS
#define INC_ERRNO
#define INC_TIME
#define INC_ASSERT
#include "includes.h"
#include "base.h"
#include "fio.h"

#define MEMBERSIZ 8
#define VOLUMESIZ 8

/* global declarations */
extern INT fioaoperr;		/* variable to hold open errors */
extern INT fioalkerr;		/* variable to hold lock errors */
extern INT fioaclerr;		/* variable to hold close errors */
extern INT fioarderr;		/* variable to hold read errors */
extern INT fioawrerr;		/* variable to hold write errors */
extern INT fioaskerr;		/* variable to hold seek errors */
extern INT fioadlerr;		/* variable to hold delete errors */
#if OS_UNIX
extern INT fiosemerr;		/* variable to hold semaphore errors */
#endif

/* local declarations */
static UCHAR **ftable;
static INT ftabmax;			/* number of file table entries */
static INT ftabhi;			/* index of highest used file table entry */
static INT opencnt;			/* count of actually open files */
static UINT lastuse;		/* last use counter */

static INT flags;			/* flags from fioparms structure */
static INT openlimit;		/* maximum number of operating system files to be open */
static INT filetimeout;		/* filepi timeout in seconds */
static INT rectimeout;		/* record lock timeout in seconds */
static OFFSET maxoffset = 0x7FFFFFFF;  /* decare here so optimizer won't produce overflow warning */
static CHAR **openpath = NULL;
static CHAR **preppath = NULL;
static CHAR **srcpath = NULL;
static CHAR **dbcpath = NULL;
static CHAR **editcfgpath = NULL;
static CHAR **dbdpath = NULL;
static CHAR **prtpath = NULL;
static CHAR **tdfpath = NULL;
static CHAR **tdbpath = NULL;
static CHAR **imgpath = NULL;
static CHAR **cftpath = NULL; /* Smart Client File Transfer */
static UCHAR **casemap = NULL;
static UCHAR **collatemap = NULL;
static INT (*cvtvolfnc)(CHAR *, CHAR ***);	/* function to convert :VOLUME to dirctory */
static CHAR **findpath = NULL;
static CHAR **findfile = NULL;

/* local routine declarations */
static INT fioxop(INT, CHAR *, CHAR *, INT, FHANDLE *, INT *);
static INT fiolibsrch(FHANDLE, CHAR *, OFFSET *, OFFSET *);
static INT fiolibmod(FHANDLE, CHAR *, OFFSET, OFFSET, INT);
static CHAR *fioinitprops(FIOPARMS *);
static INT fioinitcvtvol(CHAR *, CHAR ***);

/* FIOINITCFG */
/* initialize using a prefix and from a .cfg file */
CHAR *fioinit(FIOPARMS *parms, INT initialized)
{
	INT i1, i2, parmflags;
	OFFSET offset;
	FIOAINITSTRUCT fioaparms;
	CHAR *errmsg;

/* initialize ftab structures */
	if (ftabmax) return("attempt to call fioinit twice");
	ftable = memalloc(32 * sizeof(struct ftab), 0);
	if (ftable == NULL) return("fioinit: no memory(a)");
	ftabmax = 32;
	ftabhi = 0;

	openlimit = 0;
	memset(&fioaparms, 0, sizeof(fioaparms));
#if OS_WIN32
	filetimeout = 360;
#else
	filetimeout = -1;
#endif
	rectimeout = -1;
	/* calculate largest offset because we can not guarantee a correct define */
	offset = maxoffset;
	i1 = sizeof(OFFSET);
	if (i1 == 8) {  /* do this to prevent compiler warnings on 32 bit numbers */
#if OS_WIN32
/*** CODE: NUMBERS GREATER THAN 32 BITS DOES NOT WORK FOR WIN32 & WIN95 AND/OR ***/
/***       NOVELL WHEN PASSING A LOCK PACKET ***/
		offset <<= 1;
#else
/*** CODE: HP AND MAYBE OTHERS ONLY SUPPORT 40 BIT VALUES ***/
		offset <<= 9;
#endif
		offset |= 0x0001FF;
	}
 	fioaparms.fileoffset = offset - 0x0F;
 	fioaparms.recoffset = (offset >> 1) + 0x01;
#if OS_UNIX
	fioaparms.excloffset = offset - 0x07;
#endif

	if (parms != NULL) {
		if (!initialized) {
			errmsg = fioinitprops(parms);
			if (errmsg != NULL) return errmsg;
		}
		flags = parms->flags;
		parmflags = parms->parmflags;
		openlimit = parms->openlimit;
		if (parmflags & FIO_PARM_FILETIMEOUT) filetimeout = parms->filetimeout;
		if (parmflags & FIO_PARM_RECTIMEOUT) rectimeout = parms->rectimeout;
#if OS_WIN32
		if (parmflags & FIO_PARM_LOCKRETRYTIME) fioaparms.lockretrytime = parms->lockretrytime;
#endif
		if (parmflags & FIO_PARM_FILEOFFSET) fioaparms.fileoffset = parms->fileoffset;
		if (parmflags & FIO_PARM_RECOFFSET) fioaparms.recoffset = parms->recoffset;
#if OS_UNIX
		if (parmflags & FIO_PARM_EXCLOFFSET) fioaparms.excloffset = parms->excloffset;
#endif
		if (parms->openpath[0]) {
			i2 = (INT)strlen(parms->openpath);
			openpath = (CHAR **) memalloc(i2 + 1, 0);
			strcpy(*openpath, parms->openpath);
		}
		if (parms->preppath[0]) {
			i2 = (INT)strlen(parms->preppath);
			preppath = (CHAR **) memalloc(i2 + 1, 0);
			strcpy(*preppath, parms->preppath);
		}
		if (parms->srcpath[0]) {
			i2 = (INT)strlen(parms->srcpath);
			srcpath = (CHAR **) memalloc(i2 + 1, 0);
			strcpy(*srcpath, parms->srcpath);
		}
		if (parms->dbcpath[0]) {
			i2 = (INT)strlen(parms->dbcpath);
			dbcpath = (CHAR **) memalloc(i2 + 1, 0);
			strcpy(*dbcpath, parms->dbcpath);
		}
		if (parms->editcfgpath[0]) {
			i2 = (INT)strlen(parms->editcfgpath);
			editcfgpath = (CHAR **) memalloc(i2 + 1, 0);
			strcpy(*editcfgpath, parms->editcfgpath);
		}
		if (parms->dbdpath[0]) {
			i2 = (INT)strlen(parms->dbdpath);
			dbdpath = (CHAR **) memalloc(i2 + 1, 0);
			strcpy(*dbdpath, parms->dbdpath);
		}
		if (parms->prtpath[0]) {
			i2 = (INT)strlen(parms->prtpath);
			prtpath = (CHAR **) memalloc(i2 + 1, 0);
			strcpy(*prtpath, parms->prtpath);
		}
		if (parms->tdfpath[0]) {
			i2 = (INT)strlen(parms->tdfpath);
			tdfpath = (CHAR **) memalloc(i2 + 1, 0);
			strcpy(*tdfpath, parms->tdfpath);
		}
		if (parms->tdbpath[0]) {
			i2 = (INT)strlen(parms->tdbpath);
			tdbpath = (CHAR **) memalloc(i2 + 1, 0);
			strcpy(*tdbpath, parms->tdbpath);
		}
		if (parms->imgpath[0]) {
			i2 = (INT)strlen(parms->imgpath);
			imgpath = (CHAR **) memalloc(i2 + 1, 0);
			strcpy(*imgpath, parms->imgpath);
		}
		if (parms->cftpath[0]) {
			i2 = (INT)strlen(parms->cftpath);
			cftpath = (CHAR **) memalloc(i2 + 1, 0);
			strcpy(*cftpath, parms->cftpath);
		}
		for (i1 = 0; i1 <= UCHAR_MAX && !parms->casemap[i1]; i1++);
		if (i1 <= UCHAR_MAX) {
			casemap = memalloc(UCHAR_MAX + 1, 0);
			memcpy(*casemap, parms->casemap, UCHAR_MAX + 1);
		}
		for (i1 = 0; i1 <= UCHAR_MAX && !parms->collatemap[i1]; i1++);
		if (i1 <= UCHAR_MAX) {
			collatemap = memalloc(UCHAR_MAX + 1, 0);
			memcpy(*collatemap, parms->collatemap, UCHAR_MAX + 1);
		}
		cvtvolfnc = parms->cvtvolfnc;
	}
	else flags = 0;

	if (!openlimit) openlimit = 9999;
	i1 = fioainit(&flags, &fioaparms);
	if (i1 == 0) return NULL;
	return fioerrstr(i1);
}

void fioexit()
{
	INT i1;

	if (!ftabmax) return;
	for (i1 = 1; i1 <= ftabhi; i1++) fioclose(i1);
	memfree((UCHAR **) openpath);
	openpath = NULL;
	memfree((UCHAR **) preppath);
	preppath = NULL;
	memfree((UCHAR **) srcpath);
	srcpath = NULL;
	memfree((UCHAR **) dbcpath);
	dbcpath = NULL;
	memfree((UCHAR **) editcfgpath);
	editcfgpath = NULL;
	memfree((UCHAR **) dbdpath);
	dbdpath = NULL;
	memfree((UCHAR **) prtpath);
	prtpath = NULL;
	memfree((UCHAR **) tdfpath);
	tdfpath = NULL;
	memfree((UCHAR **) tdbpath);
	tdbpath = NULL;
	memfree((UCHAR **) imgpath);
	imgpath = NULL;
	memfree((UCHAR **) cftpath);
	cftpath = NULL;
	memfree(casemap);
	casemap = NULL;
	memfree(collatemap);
	collatemap = NULL;
	memfree(ftable);
	ftable = NULL;
	ftabmax = ftabhi = 0;
}

/* FIOSETFLAGS */
void fiosetflags(INT newflags)
{
	flags = newflags;
}

/* FIOGETFLAGS */
INT fiogetflags()
{
	return(flags);
}

/*
 * FIOSETOPT
 *
 * Called only by utilities and fs, and as far as I can tell,
 * only for OPT_PREPPATH and OPT_OPENPATH
 */
INT fiosetopt(INT opt, UCHAR *value)
{
	INT i1;
	CHAR ***ppptr;

	if (opt >= FIO_OPT_OPENPATH && opt <= FIO_OPT_IMGPATH) {
		switch (opt) {
		case FIO_OPT_OPENPATH:
			ppptr = &openpath;
			break;
		case FIO_OPT_PREPPATH:
			ppptr = &preppath;
			break;
		case FIO_OPT_SRCPATH:
			ppptr = &srcpath;
			break;
		case FIO_OPT_DBCPATH:
			ppptr = &dbcpath;
			break;
		case FIO_OPT_EDITCFGPATH:
			ppptr = &editcfgpath;
			break;
		case FIO_OPT_DBDPATH:
			ppptr = &dbdpath;
			break;
		case FIO_OPT_PRTPATH:
			ppptr = &prtpath;
			break;
		case FIO_OPT_TDFPATH:
			ppptr = &tdfpath;
			break;
		case FIO_OPT_TDBPATH:
			ppptr = &tdbpath;
			break;
		case FIO_OPT_IMGPATH:
			ppptr = &imgpath;
			break;
		default:
			return (ERR_OTHER);
		}
		memfree((UCHAR **) *ppptr);
		if (value != NULL) {
			*ppptr = (CHAR **) memalloc((INT)strlen((CHAR *) value) + 1, 0);
			if (*ppptr == NULL) return(ERR_NOMEM);
			strcpy(**ppptr, (CHAR *) value);
		}
		else *ppptr = NULL;
		return(0);
	}
	if (opt == FIO_OPT_CASEMAP) {
		if (value != NULL) {
			for (i1 = 0; i1 <= UCHAR_MAX && !value[i1]; i1++);
			if (i1 > UCHAR_MAX) value = NULL;
		}
		if (value != NULL) {
			if (casemap == NULL) {
				casemap = memalloc(UCHAR_MAX + 1, 0);
				if (casemap == NULL) return(ERR_NOMEM);
			}
			memcpy(*casemap, value, UCHAR_MAX + 1);
		}
		else {
			memfree(casemap);
			casemap = NULL;
		}
		return(0);
	}
	if (opt == FIO_OPT_COLLATEMAP) {
		if (value != NULL) {
			for (i1 = 0; i1 <= UCHAR_MAX && !value[i1]; i1++);
			if (i1 > UCHAR_MAX) value = NULL;
		}
		if (value != NULL) {
			if (collatemap == NULL) {
				collatemap = memalloc(UCHAR_MAX + 1, 0);
				if (collatemap == NULL) return(ERR_NOMEM);
			}
			memcpy(*collatemap, value, UCHAR_MAX + 1);
		}
		else {
			memfree(collatemap);
			collatemap = NULL;
		}
		return(0);
	}
	return(ERR_INVAR);
}

/* FIOGETOPT */
UCHAR **fiogetopt(INT opt)
{
	switch (opt) {
	case FIO_OPT_CASEMAP:
		return(casemap);
	case FIO_OPT_COLLATEMAP:
		return(collatemap);
	case FIO_OPT_OPENPATH:
		return((UCHAR **) openpath);
	case FIO_OPT_PREPPATH:
		return((UCHAR **) preppath);
	case FIO_OPT_SRCPATH:
		return((UCHAR **) srcpath);
	case FIO_OPT_DBCPATH:
		return((UCHAR **) dbcpath);
	case FIO_OPT_EDITCFGPATH:
		return((UCHAR **) editcfgpath);
	case FIO_OPT_DBDPATH:
		return((UCHAR **) dbdpath);
	case FIO_OPT_PRTPATH:
		return((UCHAR **) prtpath);
	case FIO_OPT_TDFPATH:
		return((UCHAR **) tdfpath);
	case FIO_OPT_TDBPATH:
		return((UCHAR **) tdbpath);
	case FIO_OPT_IMGPATH:
		return((UCHAR **) imgpath);
	}
	return(NULL);
}

/* FIOCVTVOL */
INT fiocvtvol(CHAR *volume, CHAR ***directory)
{
	if (cvtvolfnc == NULL) return RC_ERROR;
	return(cvtvolfnc(volume, directory));
}

/**
 * FIOOPEN
 * Returns a negative number for error, positive file handle for success
 * Might move memory
 */
INT fioopen(CHAR *name, INT opts)  /* open a file */
{
	INT i1, fnum, opnflg, savemode = 0, search;
	SHORT mode;
	FHANDLE handle;
	OFFSET pos, length;
	CHAR filename[MAX_NAMESIZE + 1], memname[MEMBERSIZ + 1], dbcvol[VOLUMESIZ + 8];
	CHAR *ptr, *ptr1, *ptr2, **pptr, **pptr1, **pptr2;
	UCHAR **hptr;
	UCHAR **libptr;
	struct ftab *f;
	struct htab *h;
	struct ltab *lib;

	if (!ftabmax) {
		if (fioinit(NULL, FALSE) != NULL) return ERR_OTHER;
	}

	/* find an empty ftable entry */
	f = (struct ftab *) *ftable;
	for (fnum = 0; fnum < ftabhi && f[fnum].hptr != NULL; fnum++);
	if (fnum == ftabmax) {
		UINT newSize = (ftabmax << 1) * sizeof(struct ftab);
		i1 = memchange(ftable, newSize, 0);
		if (i1) return(ERR_NOMEM);
		ftabmax <<= 1;
	}

	strncpy(filename, name, sizeof(filename) - 1);
	filename[sizeof(filename) - 1] = '\0';

	/* miofixname always returns zero */
	miofixname(filename, NULL, FIXNAME_PAR_DBCVOL | FIXNAME_PAR_MEMBER);
	miogetname(&ptr1, &ptr2);
	strcpy(dbcvol, ptr1);
	strcpy(memname, ptr2);

	/* mode and search options */
	mode = opts & FIO_M_MASK;
	if (mode < FIO_M_SRO || mode > FIO_M_EFC) return(ERR_INVAR);
	search = opts & FIO_P_MASK;

	/* parse member name and library name */
	if (memname[0]) {
		i1 = (INT)strlen(memname);
		if (MEMBERSIZ - i1 > 0) memset(&memname[i1], ' ', MEMBERSIZ - i1);
		while (i1--) memname[i1] = (CHAR) toupper(memname[i1]);
		miofixname(filename, ".lib", FIXNAME_EXT_ADD);
		savemode = mode;
		if (mode >= FIO_M_PRP) {
			if (mode == FIO_M_MTC) mode = FIO_M_MXC;
			else mode = FIO_M_EXC;
		}
	}

	i1 = (INT)strlen(filename);
	/* remove trailing period from name */
	if (i1 && filename[i1 - 1] == '.') filename[--i1] = '\0';
	/* test for zero length name */
	if (!i1) return(ERR_BADNM);
	if (dbcvol[0]) {
		if (fiocvtvol(dbcvol, &pptr1)) pptr1 = NULL;
		pptr2 = pptr1;
	}
	else {
		/* check for directory or drive specification */
		if (fioaslash(filename) >= 0) search = FIO_P_WRK;
		if (search == FIO_P_TXT) {
			pptr1 = (CHAR **) openpath;
			pptr2 = (CHAR **) preppath;
		}
		else if (search == FIO_P_SRC) pptr1 = pptr2 = (CHAR **) srcpath;
		else if (search == FIO_P_DBC) pptr1 = pptr2 = (CHAR **) dbcpath;
		else if (search == FIO_P_CFG) pptr1 = pptr2 = (CHAR **) editcfgpath;
		else if (search == FIO_P_DBD) pptr1 = pptr2 = (CHAR **) dbdpath;
		else if (search == FIO_P_PRT) pptr1 = pptr2 = (CHAR **) prtpath;
		else if (search == FIO_P_TDF) pptr1 = pptr2 = (CHAR **) tdfpath;
		else if (search == FIO_P_TDB) pptr1 = pptr2 = (CHAR **) tdbpath;
		else if (search == FIO_P_IMG) pptr1 = pptr2 = (CHAR **) imgpath;
		else if (search == FIO_P_CFT) pptr1 = pptr2 = (CHAR **) cftpath;
		else pptr1 = pptr2 = NULL;
	}

	if (pptr1 == NULL) {
		pptr1 = &ptr;
		ptr = "";
	}
	i1 = (INT)strlen(*pptr1) + 1;
	pptr = (CHAR **) memalloc(i1, 0);
	if (pptr == NULL) return(ERR_NOMEM);
	memcpy(*pptr, *pptr1, i1);
	/* attempt to open name in search paths specified */
	i1 = fioxop(0, filename, *pptr, mode, &handle, &opnflg);
	memfree((UCHAR **) pptr);

	/* open has been attempted, if unsuccessful attempt a create */
	if (i1 == ERR_FNOTF && mode > FIO_M_MXC) {
		if (pptr2 == NULL) {
			pptr2 = &ptr;
			ptr = "";
		}
		i1 = (INT)strlen(*pptr2) + 1;
		pptr = (CHAR **) memalloc(i1, 0);
		if (pptr == NULL) return(ERR_NOMEM);
		memcpy(*pptr, *pptr2, i1);
		i1 = fioxop(1, filename, *pptr, mode, &handle, &opnflg);
		memfree((UCHAR **) pptr);
	}
	/* error during open */
	if (i1) {
		return(i1);
	}

	/* reset f */
	f = (struct ftab *) *ftable;

	/* already open through another handle */
	if (opnflg != -1) {
		hptr = f[opnflg].hptr;
		h = (struct htab *) *hptr;
		h->opct++;
	}
	else {  /* new filename, alloc htab */
		i1 = (INT)strlen(filename);
		hptr = memalloc(sizeof(struct htab) - (MAX_NAMESIZE - i1) * sizeof(CHAR), 0);
		if (hptr == NULL) {
			fioaclose(handle);
			opencnt--;
			return(ERR_NOMEM);
		}

		/* reset f */
		f = (struct ftab *) *ftable;
		h = (struct htab *) *hptr;
		h->opct = 1;
		h->hndl = handle;
		h->mode = mode;
		h->lckflg = 0;
		h->fsiz = -1;
		h->npos = 0L;
		h->lpos = 0L;
		h->luse = ++lastuse;
		h->pptr = NULL;
		memcpy(h->fnam, filename, i1 + 1);
	}
	f[fnum].hptr = hptr;
	f[fnum].lptr = NULL;
	f[fnum++].wptr = NULL;
	if (fnum > ftabhi) ftabhi = fnum;

	if (!memname[0]) {
		return(fnum);
	}

	/* if member, check if member exits */
	h->npos = -1;  /* fiolibsrch modifies current file position */
	i1 = fiolibsrch(handle, memname, &pos, &length);
	if (i1 < 0) {
		fioclose(fnum);
		return(i1);
	}
	if (i1 == 1 && savemode < FIO_M_PRP) {
		fioclose(fnum);
		return(ERR_FNOTF);
	}
	if (i1 == 0 && savemode == FIO_M_EFC) {
		fioclose(fnum);
		return(ERR_EXIST);
	}

	libptr = memalloc(sizeof(struct ltab), 0);
	if (libptr == NULL) {
		fioclose(fnum);
		return(ERR_NOMEM);
	}
	lib = (struct ltab *) *libptr;
	memcpy(lib->member, memname, MEMBERSIZ);
	if (savemode >= FIO_M_PRP) {
		lib->createflg = TRUE;
		fioalseek(handle, 0L, 2, &lib->filepos);
		lib->length = 0L;
	}
	else {
		lib->createflg = FALSE;
		lib->filepos = pos;
		lib->length = length;
	}

	f[fnum - 1].lptr = libptr;
	return(fnum);
}

/**
 * FIOXOP
 * attempt open only with path(s)
 * if create is non-zero, then only attempt create on first path
 * return 0 for success, negative error value for failure
 */
static INT fioxop(INT create, CHAR *name, CHAR *path, INT mode, FHANDLE *phandle, INT *opnflg)
{
	INT i1;
	FHANDLE handle;
	CHAR work[MAX_NAMESIZE + 1];
	struct ftab *f;
	struct htab *h = NULL;


	*opnflg = FALSE;
	while (opencnt >= openlimit && !fioclru(0));

	f = (struct ftab *) *ftable;
	miostrscan(path, work);
	do {
		fioaslashx(work);
		strcat(work, name);

		/* check for already open through another handle */
		for (i1 = 0; i1 < ftabhi; i1++) {
			if (f[i1].hptr == NULL) continue;
			h = (struct htab *) *f[i1].hptr;
#if OS_UNIX
			if (!strcmp(work, h->fnam)) break;
#else
			if (!_stricmp(work, h->fnam)) break;
#endif
		}
		if (i1 < ftabhi) {  /* found this filename already open */
			if ((mode == h->mode &&
					(mode <= FIO_M_SHR || mode == FIO_M_MXC || mode == FIO_M_MTC)) || (h->mode == FIO_M_MTC && mode == FIO_M_MXC))
			{
				*phandle = h->hndl;
				*opnflg = i1;
				return(0);
			}
			return(ERR_NOACC);
		}
		do i1 = fioaopen(work, mode, create, &handle);
		while (i1 == ERR_EXCMF && !fioclru(0));
	} while (i1 == ERR_FNOTF && !create && miostrscan(path, work) == 0);
	if (i1) {
		return(i1);
	}
	strcpy(name, work);
	opencnt++;
	*phandle = handle;
	*opnflg = -1;
	return(0);
}

/* FIOLIBSRCH
 * search library for member and
 * return 0 if found, 1 if not found, and error if error
 */
static INT fiolibsrch(FHANDLE handle, CHAR *member, OFFSET *posptr, OFFSET *lenptr)
{
	INT i1, i2;
	OFFSET pos;
	UCHAR *buffer, **bufptr;

	bufptr = memalloc(3072, 0);
	if (bufptr == NULL) return(ERR_NOMEM);
	buffer = *bufptr;
	pos = 0L;
	do {
		i1 = fioaread(handle, buffer, 3072, pos, &i2);
		if (i1 || i2 != 3072 || memcmp(buffer, "$LIBRARY", 8)) {
			memfree(bufptr);
			return(ERR_BADLB);
		}
		for (i1 = 48; i1 < 3072; i1 += 48) {
			if (buffer[i1] != '+' && buffer[i1] != '-' && buffer[i1] != ' ') {
				memfree(bufptr);
				return(ERR_BADLB);
			}
			if (!memcmp("+000", &buffer[i1], 4) && !memcmp(member, &buffer[i1 + 4], MEMBERSIZ)) break;
		}
		if (i1 != 3072) {  /* found member */
			msc9tooff(&buffer[i1 + 16], posptr);
			msc9tooff(&buffer[i1 + 25], lenptr);
			break;
		}
		msc9tooff(&buffer[16], &pos);
	} while (pos);
	memfree(bufptr);
	if (i1 != 3072) return(0);
	return(1);
}

/* FIOCLOSE */
INT fioclose(INT fnum)  /* close fnum */
{
	INT i1;
	struct ftab *f;
	struct htab *h;
	struct ltab *lib;


	f = (struct ftab *) *ftable;
	fnum--;
	if (fnum < 0 || fnum >= ftabhi || f[fnum].hptr == NULL) return(ERR_NOTOP);
	h = (struct htab *) *f[fnum].hptr;
	i1 = 0;

	/* library close and add or replace entry */
	if (f[fnum].lptr != NULL) {  /* library member */
		lib = (struct ltab *) *f[fnum].lptr;
		if (lib->createflg) {  /* add entry */
			h->npos = -1;  /* fiolibmod modifies current file position */
			i1 = fiotouch(fnum + 1);
			if (!i1) i1 = fiolibmod(h->hndl, lib->member, lib->filepos, lib->length, FALSE);
		}
	}

	if (h->pptr != NULL) fioulkpos(fnum + 1, -1);  /* remove position locks */
	h->opct--;
	if (!h->opct) {
		if (h->hndl != (FHANDLE)-1) {
			/* unlock locked files */
			if (h->lckflg) fioalock(h->hndl, FIOA_FLLCK | FIOA_UNLCK, 0, 0);
			i1 = fioaclose(h->hndl);
			opencnt--;
		}
		memfree(f[fnum].hptr);
	}
	f[fnum].hptr = NULL;
	if (f[fnum].lptr != NULL) memfree(f[fnum].lptr);
	if (fnum + 1 == ftabhi) ftabhi = fnum;
	return(i1);
}

/* FIOKILL */
INT fiokill(INT fnum)  /* close fnum and delete file */
{
	INT i1;
	CHAR *ptr;
	struct ftab *f;
	struct htab *h;
	struct ltab *lib;

	f = (struct ftab *) *ftable;
	fnum--;
	if (fnum < 0 || fnum >= ftabhi || f[fnum].hptr == NULL) return(ERR_NOTOP);
	h = (struct htab *) *f[fnum].hptr;
	i1 = 0;

	if (f[fnum].lptr != NULL) {  /* library delete */
		lib = (struct ltab *) *f[fnum].lptr;
		if (lib->createflg) {  /* delete entry */
			h->npos = -1;  /* fiolibmod modifies current file position */
			i1 = fiotouch(fnum + 1);
			if (!i1) i1 = fiolibmod(h->hndl, lib->member, lib->filepos, lib->length, TRUE);
		}
	}

	if (h->pptr != NULL) fioulkpos(fnum + 1, -1);  /* remove position locks */
	h->opct--;
	if (!h->opct) {
		ptr = h->fnam;
		if (h->hndl != (FHANDLE)-1) {  /* unlock locked files */
			if (h->lckflg) fioalock(h->hndl, FIOA_FLLCK | FIOA_UNLCK, 0, 0);
			i1 = fioaclose(h->hndl);
			opencnt--;
		}
		if (f[fnum].lptr == NULL) i1 = fioadelete(ptr);  /* delete if not a library */
		memfree(f[fnum].hptr);
	}
	f[fnum].hptr = NULL;
	if (f[fnum].lptr != NULL) memfree(f[fnum].lptr);
	if (fnum + 1 == ftabhi) ftabhi = fnum;
	return(i1);
}

/* FIOLIBMOD */
/* if delflg == FALSE, add or replace entry, if TRUE then delete entrys */
/* return 0 if successful, -1 if error */
static INT fiolibmod(FHANDLE handle, CHAR *member, OFFSET filepos, OFFSET length, INT delflg)  /* modify library directory */
{
	INT i1, i2, addflg, chgflg;
	OFFSET pos, lastpos;
	UCHAR timestamp[17], *buffer, **bufptr;

	bufptr = memalloc(3072, 0);
	if (bufptr == NULL) return(ERR_NOMEM);
	buffer = *bufptr;
	pos = 0L;
	addflg = delflg;
	do {
		i1 = fioaread(handle, buffer, 3072, pos, &i2);
		if (i1 || i2 != 3072 || memcmp(buffer, "$LIBRARY", 8)) {
			memfree(bufptr);
			return(ERR_BADLB);
		}
		chgflg = FALSE;
		for (i1 = 48; i1 < 3072; i1 += 48) {
			if (buffer[i1] != '+' && buffer[i1] != '-' && buffer[i1] != ' ') {
				memfree(bufptr);
				return(ERR_BADLB);
			}
			if (!addflg && (buffer[i1] == '-' || !memcmp("    ", &buffer[i1], 4))) {
				memset(&buffer[i1], ' ', 48);
				memcpy(&buffer[i1], "+000", 4);
				memcpy(&buffer[i1 + 4], member, MEMBERSIZ);
				mscoffto9(filepos, &buffer[i1 + 16]);
				mscoffto9(length, &buffer[i1 + 25]);
				msctimestamp(timestamp);
				memcpy(&buffer[i1 + 34], &timestamp[8], 4);
				memcpy(&buffer[i1 + 38], &timestamp[4], 4);
				buffer[42] = timestamp[2];
				buffer[43] = timestamp[3];
				addflg = TRUE;
				chgflg = TRUE;
			}
			else if (buffer[i1] == '+' && !memcmp(member, &buffer[i1 + 4], MEMBERSIZ)) {
				if (!delflg && !memcmp(&buffer[i1 + 1], "000", 3)) memcpy(&buffer[i1 + 1], "001", 3);
				else memcpy(&buffer[i1], "-000", 4);
				chgflg = TRUE;
			}
		}
		if (chgflg) {
			if ( (i2 = fioawrite(handle, buffer, 3072, pos, NULL)) != 0 ) {
				memfree(bufptr);
				return(i2);
			}
		}
		lastpos = pos;
		msc9tooff(&buffer[16], &pos);
	} while (pos);

	/* must create new directory block */
	if (!addflg) {
		memset(buffer, ' ', 3072);
		memcpy(buffer, "$LIBRARY R8.0   ", 16);
		mscoffto9(0L, &buffer[16]);
		memcpy(&buffer[48], "+000", 4);
		memcpy(&buffer[52], member, MEMBERSIZ);
		mscoffto9(filepos, &buffer[64]);
		mscoffto9(length, &buffer[73]);
		msctimestamp(timestamp);
		memcpy(&buffer[i1 + 82], &timestamp[8], 4);
		memcpy(&buffer[i1 + 86], &timestamp[4], 4);
		buffer[90] = timestamp[2];
		buffer[91] = timestamp[3];
		pos = filepos + length;
		if ( (i2 = fioawrite(handle, buffer, 3072, pos, NULL)) != 0 ) {
			memfree(bufptr);
			return(i2);
		}
		mscoffto9(pos, buffer);
		if ( (i2 = fioawrite(handle, buffer, 9, lastpos + 16, NULL)) != 0 ) {
			memfree(bufptr);
			return(i2);
		}
	}
	memfree(bufptr);
	return(0);
}

FHANDLE fiogetOSHandle(INT fnum)
{
	struct ftab *f;
	struct htab *h;
	fnum--;
	f = (struct ftab *) *ftable;
	h = (struct htab *) *f[fnum].hptr;
	return h->hndl;
}

/**
 * FIOREAD
 */
INT fioread(INT fnum, OFFSET fpos, UCHAR *buffer, INT count)
{
	INT i1, i2;
	FHANDLE handle;
	OFFSET pos;
	struct ftab *f;
	struct htab *h;
	struct ltab *lib;

	if (!count) return(0);
	i1 = fiotouch(fnum);
	if (i1) return(i1);
	fnum--;
	f = (struct ftab *) *ftable;
	h = (struct htab *) *f[fnum].hptr;

	handle = h->hndl;
	if (f[fnum].lptr != NULL) {  /* library member */
		lib = (struct ltab *) *f[fnum].lptr;
		if (fpos >= lib->length) return(0);
		if (fpos + count > lib->length) count = (INT)(lib->length - fpos);
		fpos += lib->filepos;
	}

	pos = fpos;
	if (pos == h->npos) pos = -1;

	i1 = fioaread(handle, buffer, count, pos, &i2);
	if (i1) {
		return(i1);
	}
	if (i2 > 0) {
		h->npos = fpos + i2;
		if (h->npos > h->fsiz && h->fsiz != -1) h->fsiz = h->npos;
	}
	else h->npos = -1;
	return i2;
}

/* FIOWRITE */
INT fiowrite(INT fnum, OFFSET fpos, UCHAR *buffer, size_t count)
{
	INT i1;
	size_t i2;
	FHANDLE handle;
	OFFSET pos;
	struct ftab *f;
	struct htab *h;
	struct ltab *lib = NULL;

	if (!count) return(0);
	i1 = fiotouch(fnum);
	if (i1) return(i1);
	fnum--;
	f = (struct ftab *) *ftable;
	h = (struct htab *) *f[fnum].hptr;

	if (h->mode == FIO_M_SRO || h->mode == FIO_M_SRA || h->mode == FIO_M_ERO) return(ERR_RONLY);
	handle = h->hndl;
	if (f[fnum].lptr != NULL) {  /* library member */
		lib = (struct ltab *) *f[fnum].lptr;
		if (!lib->createflg) return(ERR_RONLY);
		fpos += lib->filepos;
	}

	pos = fpos;
	if (pos == h->npos) pos = -1;

	i1 = fioawrite(handle, buffer, count, pos, &i2);
	if (i1) return(i1);

	h->npos = fpos + i2;
	if (h->npos > h->fsiz && h->fsiz != (OFFSET) -1) h->fsiz = h->npos;
	if (f[fnum].lptr != NULL) {  /* library member */
		fpos = h->npos - lib->filepos;
		if (fpos > lib->length) lib->length = fpos;
	}
	return(0);
}

/**
 * FIOGETSIZE
 * Returns zero for success, negative for fail
 */
INT fiogetsize(INT fnum, OFFSET *size)
{
	INT i1;
	struct ftab *f;
	struct htab *h;
	struct ltab *lib;

	f = (struct ftab *) *ftable;
	fnum--;
	if (fnum < 0 || fnum >= ftabhi || f[fnum].hptr == NULL) return(ERR_NOTOP);
	if (f[fnum].lptr != NULL) {  /* library member */
		lib = (struct ltab *) *f[fnum].lptr;
		*size = lib->length;
		return(0);
	}
	h = (struct htab *) *f[fnum].hptr;

	/*
	 * The below test for FIO_M_SHR can cause problems
	 * when DX is adding records to this file, and FS
	 * opens it READONLY. We cache the file size and
	 * never get it again. If it grows via DX, the check for
	 * file type in rioopen will fail.
	 */
	if (h->mode == FIO_M_SHR || h->fsiz == (OFFSET) -1) {
		i1 = fiotouch(fnum + 1);
		if (i1) return(i1);
		i1= fioalseek(h->hndl, 0L, 2, &h->npos);
		if (i1) return(i1);
		h->fsiz = h->npos;
	}
	*size = h->fsiz;
	return(0);
}

/*
 * FIOCLRU
 * handle = -1, close first non-exclusive file (neglect file and record locks)
 * handle = 0, close lru file that is non-exclusive, no file or record locks
 * handle > 0, close given file (neglect exclusive file, file and record locks)
 *
 * Return 1 if no unlock happened, zero if one happened
 */
INT fioclru(INT handle)
{
	INT fnum, fnum1;
	struct ftab *f;
	struct htab *h = NULL, *h1;

	if (!ftabmax) return 1;

	f = (struct ftab *) *ftable;
	fnum = -1;
	for (fnum1 = (handle <= 0) ? 0 : handle - 1; fnum1 < ftabhi; fnum1 = (handle <= 0) ? fnum1 + 1 : ftabhi) {
		if (f[fnum1].hptr == NULL) continue;
		h1 = (struct htab *) *f[fnum1].hptr;
		if (h1->hndl == (FHANDLE)-1) continue;
		if ((handle == -1 && h1->mode <= FIO_M_SHR) || handle > 0) {
			fnum = fnum1;
			h = h1;
			break;
		}
		if (h1->mode <= FIO_M_SHR && !h1->lckflg && h1->pptr == NULL) {
			if (fnum == -1 || h1->luse < h->luse) {
				fnum = fnum1;
				h = h1;
			}
		}
	}
	if (fnum == -1) {
		return(1);
	}

	if (h->pptr != NULL) fioulkpos(fnum + 1, -2);  /* remove all position locks including associated files */
	if (h->lckflg) {
		h->lckflg = 0;
		fioalock(h->hndl, FIOA_FLLCK | FIOA_UNLCK, 0, 0);
	}
	fioaclose(h->hndl);
	h->hndl = (FHANDLE)-1;
	opencnt--;
	return(0);
}

/**
 * FIOTOUCH
 * force a file open, return 0 if ok.
 *
 * Possible error returns are;
 * ERR_EXCMF, ERR_EXIST, ERR_FNOTF, ERR_LKERR, ERR_NOACC, ERR_NOMEM, ERR_NOSEM,
 * 		ERR_NOTOP, ERR_OPERR
 */
INT fiotouch(INT fnum)
{
	INT i1;
	FHANDLE handle;
	struct ftab *f;
	struct htab *h;

	f = (struct ftab *) *ftable;
	fnum--;
	if (fnum < 0 || fnum >= ftabhi || f[fnum].hptr == NULL) return(ERR_NOTOP);
	h = (struct htab *) *f[fnum].hptr;

	h->luse = ++lastuse;
	if (h->hndl != (FHANDLE)-1) {
		return 0;
	}
	while (opencnt >= openlimit && !fioclru(0));

	do i1 = fioaopen(h->fnam, h->mode, 2, &handle);
	while (i1 == ERR_EXCMF && !fioclru(0));

	if (i1) return i1;
	opencnt++;
	h->hndl = handle;
	h->npos = 0L;
	return 0;
}

/* FIOFLUSH */
/* cause file header to be updated */
INT fioflush(INT fnum)
{
	INT i1;
	struct ftab *f;
	struct htab *h;

	i1 = fiotouch(fnum);
	if (i1) return(i1);
	f = (struct ftab *) *ftable;
	h = (struct htab *) *f[fnum - 1].hptr;

	if ( (i1 = fioaflush(h->hndl)) != 0 ) {
		i1 = fioclru(fnum);
		if (i1 > 0) return(0);
		if (!i1) i1 = fiotouch(fnum);
	}
	return(i1);
}

/* FIOTRUNC */
/* truncate file */
INT fiotrunc(INT fnum, OFFSET size)
{
	INT i1;
	struct ftab *f;
	struct htab *h;

	i1 = fiotouch(fnum);
	if (i1) return(i1);
	f = (struct ftab *) *ftable;
	h = (struct htab *) *f[fnum - 1].hptr;

	i1 = fioatrunc(h->hndl, size);
	if (!i1) h->fsiz = h->npos = size;
	else h->npos = -1;
	return(i1);
}

/* FIONAME */
/* return pointer to associated filename */
/* return of NULL is because of ERR_NOTOP */
CHAR *fioname(INT fnum)
{
	struct ftab *f;
	struct htab *h;

	f = (struct ftab *) *ftable;
	fnum--;
	if (fnum < 0 || fnum >= ftabhi || f[fnum].hptr == NULL) return(NULL);
	h = (struct htab *) *f[fnum].hptr;
	return(h->fnam);
}

/* FIOGETLPOS */
INT fiogetlpos(INT fnum, OFFSET *pos)
{
	struct ftab *f;
	struct htab *h;

	f = (struct ftab *) *ftable;
	fnum--;
	if (fnum < 0 || fnum >= ftabhi || f[fnum].hptr == NULL) return(ERR_NOTOP);
	h = (struct htab *) *f[fnum].hptr;
	*pos = h->lpos;
	return(0);
}

/* FIOSETLPOS */
INT fiosetlpos(INT fnum, OFFSET pos)
{
	struct ftab *f;
	struct htab *h;

	f = (struct ftab *) *ftable;
	fnum--;
	if (fnum < 0 || fnum >= ftabhi || f[fnum].hptr == NULL) return(ERR_NOTOP);
	h = (struct htab *) *f[fnum].hptr;
	h->lpos = pos;
	return(0);
}

/* FIOGETWTAB */
UCHAR **fiogetwptr(INT fnum)
{
	struct ftab *f;

	f = (struct ftab *) *ftable;
	fnum--;
	if (fnum < 0 || fnum >= ftabhi || f[fnum].hptr == NULL) return(NULL);
	return(f[fnum].wptr);
}

/* FIOSETWTAB */
INT fiosetwptr(INT fnum, UCHAR **wptr)
{
	struct ftab *f;

	f = (struct ftab *) *ftable;
	fnum--;
	if (fnum < 0 || fnum >= ftabhi || f[fnum].hptr == NULL) return(ERR_NOTOP);
	if (f[fnum].wptr != NULL) return(ERR_PROGX);
	f[fnum].wptr = wptr;
	return(0);
}

/*
 * FIOLOCK
 *
 * DX only calls this in response to a filepi verb.
 * It is not used for implicit file locks
 *
 * Also called from FS in fsfile.c
 */
INT fiolock(INT *flist)  /* lock files */
{
	INT i1, fnum, locktype = 0;
	struct ftab *f;
	struct htab *h, *h1 = NULL;

	if (flags & FIO_FLAG_SINGLEUSER) return(0);

	f = (struct ftab *) *ftable;
	for (i1 = 0; flist[i1]; i1++) {
		fnum = flist[i1] - 1;
		if (fnum < 0 || fnum >= ftabhi || f[fnum].hptr == NULL) {
			fiounlock(-1);
			return(ERR_NOTOP);
		}
	}
	for (;;) {
		for (i1 = 0, fnum = -1; flist[i1]; i1++) {
			h = (struct htab *) *f[flist[i1] - 1].hptr;
			/* don't lock if already locked, or not shared mode */
			if (h->lckflg & FIOX_HLK) continue;
			if (h->mode == FIO_M_SHR) locktype = FIOA_FLLCK | FIOA_WRLCK;
			else if (h->mode == FIO_M_SRA) locktype = FIOA_FLLCK | FIOA_RDLCK;
#if OS_UNIX
			else if (h->mode == FIO_M_SRO) locktype = FIOA_FLLCK | FIOA_RDLCK;
#endif
			else continue;
			/* already has low level lock */
			if (h->lckflg & FIOX_LLK) {
				h->lckflg = (h->lckflg & ~FIOX_LLK) | FIOX_HLK;
				continue;
			}
#if OS_UNIX
			if (fnum == -1 || strcmp(h->fnam, h1->fnam) < 0) {
#else
			if (fnum == -1 || _stricmp(h->fnam, h1->fnam) < 0) {
#endif
				fnum = flist[i1] - 1;
				h1 = h;
			}
		}
		if (fnum == -1) break;
		i1 = fiotouch(fnum + 1);
		if (!i1) {
			i1 = fioalock(h1->hndl, locktype, 0, filetimeout);
			if (i1 == ERR_NOACC) {
				fioalkerr = 0;
				i1 = ERR_LKERR;
			}
		}
		if (i1) {
			fiounlock(-1);
			return(i1);
		}
		h1->lckflg |= FIOX_HLK;
	}
	return(0);
}

/*
 * FIOUNLOCK
 *
 * If fnum is -1 will clear all 'high level locks' (FIOX_HLK)
 */
void fiounlock(INT fnum)
{
	INT fnumhi;
	struct ftab *f;
	struct htab *h;

	if (flags & FIO_FLAG_SINGLEUSER) return;

	f = (struct ftab *) *ftable;
	if (fnum > 0) fnumhi = fnum--;
	else {
		fnum = 0;
		fnumhi = ftabhi;
	}
	for ( ; fnum < fnumhi; fnum++) {
		if (f[fnum].hptr == NULL) continue;
		h = (struct htab *) *f[fnum].hptr;
		/* don't unlock if not locked */
		if (!(h->lckflg & FIOX_HLK)) continue;
		h->lckflg &= ~FIOX_HLK;
		fioalock(h->hndl, FIOA_FLLCK | FIOA_UNLCK, 0, 0);
	}
}

/**
 *  FIOFLCK
 *
 *  Returns; ERR_LKERR, ERR_NOTOP, or zero for success
 */
INT fioflck(INT fnum)
{
	INT i1, locktype;
	struct ftab *f;
	struct htab *h;

	if (flags & FIO_FLAG_SINGLEUSER) {
		return(0);
	}

	f = (struct ftab *) *ftable;
	fnum--;
	if (fnum < 0 || fnum >= ftabhi || f[fnum].hptr == NULL) return(ERR_NOTOP);
	h = (struct htab *) *f[fnum].hptr;

	/* don't lock if already locked, or not shared mode */
	if (h->lckflg) {
		return(0);
	}
	if (h->mode == FIO_M_SHR) locktype = FIOA_FLLCK | FIOA_WRLCK;
	else if (h->mode == FIO_M_SRA) locktype = FIOA_FLLCK | FIOA_RDLCK;
#if OS_UNIX
	else if (h->mode == FIO_M_SRO) locktype = FIOA_FLLCK | FIOA_RDLCK;
#endif
	else return(0);
	i1 = fiotouch(fnum + 1);
	if (!i1) {
		i1 = fioalock(h->hndl, locktype, 0, filetimeout);
		if (i1 == ERR_NOACC) {
			fioalkerr = 0;
			i1 = ERR_LKERR;
		}
	}
	if (i1) return(i1);
	h->lckflg |= FIOX_LLK;
	return(0);
}

/* FIOFULK */
void fiofulk(INT fnum)
{
	struct ftab *f;
	struct htab *h;

	if (flags & FIO_FLAG_SINGLEUSER) return;

	f = (struct ftab *) *ftable;
	fnum--;
	if (fnum < 0 || fnum >= ftabhi || f[fnum].hptr == NULL) return;
	h = (struct htab *) *f[fnum].hptr;

	/* don't unlock if not locked by fioflck() */
	if (!(h->lckflg & FIOX_LLK)) {
		return;
	}
	h->lckflg &= ~FIOX_LLK;
	fioalock(h->hndl, FIOA_FLLCK | FIOA_UNLCK, 0, 0);
}

/* FIOLCKPOS */
INT fiolckpos(INT fnum, OFFSET pos, INT testlckflg)
{
	INT i1, lckflg, locktype;
	UCHAR **pptr;
	struct ftab *f;
	struct htab *h;
	struct ptab *p;

	if (flags & FIO_FLAG_SINGLEUSER) return(0);

	f = (struct ftab *) *ftable;
	fnum--;
	if (fnum < 0 || fnum >= ftabhi || f[fnum].hptr == NULL) return(ERR_NOTOP);
	h = (struct htab *) *f[fnum].hptr;

	/* don't lock if not shared mode */
	if (h->mode == FIO_M_SHR) locktype = FIOA_RCLCK | FIOA_WRLCK;
	else if (h->mode == FIO_M_SRA) locktype = FIOA_RCLCK | FIOA_RDLCK;
#if OS_UNIX
	else if (h->mode == FIO_M_SRO) locktype = FIOA_RCLCK | FIOA_RDLCK;
#endif
	else return(0);

	if (h->pptr != NULL) {  /* loop through locks to see if already locked */
		p = (struct ptab *) *h->pptr;
		for (;;) {
			if (pos == p->pos) {
				if (fnum == p->filenum) return(0);
				/* trying to lock record already locked by another handle */
				if (testlckflg) return(1);
				fioalkerr = 0;
				return(ERR_LKERR);
			}
			if (p->pptr == NULL) break;
			p = (struct ptab *) *p->pptr;
		}
	}
/*** DO WE ALLOW A POSITION LOCK TO GO THROUGH IF FILE LOCK IS ACTIVE ***/
/*** WE COULD TRY A TEST LOCK INSTEAD ***/

	if (testlckflg) lckflg = 0;
	else lckflg = rectimeout;

	pptr = memalloc(sizeof(struct ptab), 0);
	if (pptr == NULL) return(ERR_NOMEM);
	/* reset h */
	h = (struct htab *) *f[fnum].hptr;
	p = (struct ptab *) *pptr;

	i1 = fiotouch(fnum + 1);
	if (!i1) {
		i1 = fioalock(h->hndl, locktype, pos, lckflg);
		if (i1 == ERR_NOACC) i1 = 1;
	}
	if (i1) {
		memfree(pptr);
		return(i1);
	}
	p->pos = pos;
	p->filenum = fnum;
	p->pptr = h->pptr;
	h->pptr = pptr;
	return(0);
}

/*
 * FIOULCKPOS
 * pos = -1, unlock all position locks associated with fnum
 * pos = -2, unlock all position locks associated with htab
 * otherwise, unlock position lock at pos associated with fnum
 */
void fioulkpos(INT fnum, OFFSET pos)
{
	UCHAR hflg, **pptr;
	struct ftab *f;
	struct htab *h;
	struct ptab *p, *p1 = NULL;

	if (flags & FIO_FLAG_SINGLEUSER) return;

	f = (struct ftab *) *ftable;
	fnum--;
	if (fnum < 0 || fnum >= ftabhi || f[fnum].hptr == NULL) return;
	h = (struct htab *) *f[fnum].hptr;

	/* do not unlock if no position locks */
	if (h->pptr == NULL) return;

	hflg = TRUE;
	p = (struct ptab *) *h->pptr;
	for (;;) {
		pptr = p->pptr;
		if (pos == (OFFSET) -2 || (fnum == p->filenum && (pos == (OFFSET) -1 || pos == p->pos))) {
			fioalock(h->hndl, FIOA_RCLCK | FIOA_UNLCK, p->pos, 0);
			if (hflg) {
				memfree(h->pptr);
				h->pptr = pptr;
			}
			else {
				memfree(p1->pptr);
				p1->pptr = pptr;
			}
		}
		else {
			hflg = FALSE;
			p1 = p;
		}
		if (pptr == NULL) break;
		p = (struct ptab *) *pptr;
	}
}

INT fiorename(INT fnum, CHAR *newname)
{
	INT i1;
	CHAR oldname[MAX_NAMESIZE], work[MAX_NAMESIZE], *ptr1, *ptr2, **pptr;
	struct ftab *f;
	struct htab *h;

	f = (struct ftab *) *ftable;
	if (fnum <= 0 || fnum > ftabhi || f[fnum - 1].hptr == NULL) return ERR_NOTOP;
	h = (struct htab *) *f[fnum - 1].hptr;

	if (f[fnum - 1].lptr != NULL) {  /* library member */
#if 0
		lib = (struct ltab *) *f[fnum - 1].lptr;
		if (lib->createflg) {  /* add entry */
			h->npos = -1;  /* fiolibmod modifies current file position */
			i1 = fiotouch(fnum);
			if (!i1) i1 = fiolibmod(h->hndl, lib->member, lib->filepos, lib->length, FALSE);
		}
#else
		fioclose(fnum);
		return ERR_BADNM;
#endif
	}
	if (h->opct > 1) {
		fioclose(fnum);
		return ERR_NOACC;
	}
	strcpy(oldname, h->fnam);

	/* parse the old and new names */
	miofixname(newname, "", FIXNAME_PAR_DBCVOL | FIXNAME_PAR_MEMBER);
	miogetname(&ptr1, &ptr2);
	if (*ptr2) {
		fioclose(fnum);
		return ERR_BADNM;
	}
	if (*ptr1) {
		if (!fiocvtvol(ptr1, &pptr)) {
			for (ptr1 = *pptr, i1 = 0; ptr1[i1] && ptr1[i1] != ';'; i1++) work[i1] = ptr1[i1];
			work[i1] = '\0';
			fioaslashx(work);
			strcat(work, newname);
		}
		else ptr1 = "";
	}
	if (!*ptr1) {
		strcpy(work, newname);
		if (fioaslash(newname) < 0) {  /* new file name has no directory specified */
			i1 = fioaslash(oldname);
			if (i1++ >= 0) {
				memcpy(work, oldname, i1);
				strcpy(&work[i1], newname);
			}
		}
	}
	i1 = (INT)strlen(work);
	if (work[i1 - 1] == '.') work[i1 - 1] = 0;

	/* rename it */
	fioclose(fnum);
	i1 = fioarename(oldname, work);
	return i1;
}

INT fiofindfirst(CHAR *name, INT search, CHAR **found)
{
	INT i1, i2;
	CHAR filename[MAX_NAMESIZE + 1], memname[MEMBERSIZ + 1], dbcvol[VOLUMESIZ + 8], work[MAX_NAMESIZE + 1];
	CHAR *file, *ptr, *ptr1, *ptr2, **pptr;

	if (findpath != NULL) {
		memfree((UCHAR **) findpath);
		findpath = NULL;
		memfree((UCHAR **) findfile);
	}
	search &= FIO_P_MASK;
	strncpy(filename, name, sizeof(filename) - 1);
	filename[sizeof(filename) - 1] = 0;
	i1 = miofixname(filename, NULL, FIXNAME_PAR_DBCVOL | FIXNAME_PAR_MEMBER);
	if (i1) return(i1);
	miogetname(&ptr1, &ptr2);
	strcpy(dbcvol, ptr1);
	strcpy(memname, ptr2);

	/* parse member name and library name */
	if (memname[0]) {
		return ERR_BADNM;
#if 0
/*** CODE: CURRENTLY NOT SUPPORTED ***/
		i1 = (INT)strlen(memname);
		if (MEMBERSIZ - i1 > 0) memset(&memname[i1], ' ', MEMBERSIZ - i1);
		while (i1--) memname[i1] = (CHAR) toupper(memname[i1]);
		miofixname(filename, ".lib", FIXNAME_EXT_ADD);
		savemode = mode;
		if (mode >= FIO_M_PRP) {
			if (mode == FIO_M_MTC) mode = FIO_M_MXC;
			else mode = FIO_M_EXC;
		}
#endif
	}

	i1 = (INT)strlen(filename);
	/* remove trailing period from name */
	if (i1 && filename[i1 - 1] == '.') filename[--i1] = 0;
	file = filename;
	ptr = "";
	if (dbcvol[0]) {
		if (fiocvtvol(dbcvol, &pptr)) pptr = &ptr;
	}
	else {
		/* check for directory or drive specification */
		i1 = fioaslash(filename);
		if (i1 >= 0) {
			if (i1) {
				ptr = filename;
				filename[i1] = '\0';
			}
#if OS_WIN32
			else ptr = "\\";
#else
			else ptr = "/";
#endif
			file = &filename[i1 + 1];
			search = FIO_P_WRK;
		}
		if (search == FIO_P_TXT) pptr = (CHAR **) openpath;
		else if (search == FIO_P_SRC) pptr = (CHAR **) srcpath;
		else if (search == FIO_P_DBC) pptr = (CHAR **) dbcpath;
		else if (search == FIO_P_CFG) pptr = (CHAR **) editcfgpath;
		else if (search == FIO_P_DBD) pptr = (CHAR **) dbdpath;
		else if (search == FIO_P_PRT) pptr = (CHAR **) prtpath;
		else if (search == FIO_P_TDF) pptr = (CHAR **) tdfpath;
		else if (search == FIO_P_TDB) pptr = (CHAR **) tdbpath;
		else if (search == FIO_P_IMG) pptr = (CHAR **) imgpath;
		else pptr = &ptr;
	}
	if (pptr == NULL) {
		pptr = &ptr;
		ptr = "";
	}
	i1 = (INT)strlen(*pptr) + 1;
	findpath = (CHAR **) memalloc(i1, 0);
	if (findpath == NULL) return ERR_NOMEM;
	memcpy(*findpath, *pptr, i1);

	miostrscan(*findpath, work);
	do {
		// Windows fioafindfirst can return only zero or ERR_FNOTF
		i1 = fioafindfirst(work, file, found);
		if (i1 == ERR_EXCMF && !fioclru(0)) continue;
	} while (i1 == ERR_FNOTF && !miostrscan(*findpath, work));
	if (!i1) {
		i2 = (INT)strlen(file) + 1;
		findfile = (CHAR **) memalloc(i2, 0);
		if (findfile != NULL) memcpy(*findfile, file, i2);
		else i1 = ERR_NOMEM;
	}
	if (i1) {
		memfree((UCHAR **) findpath);
		findpath = NULL;
	}
	return i1;
}

INT fiofindnext(CHAR **found)
{
	INT i1;
	CHAR work[MAX_NAMESIZE + 1];

	if (findpath == NULL) return ERR_NOTOP;

	i1 = fioafindnext(found);
	while (i1 == ERR_FNOTF && !miostrscan(*findpath, work)) {
		do i1 = fioafindfirst(work, *findfile, found);
		while (i1 == ERR_EXCMF && !fioclru(0));
	}
	if (i1) {
		memfree((UCHAR **) findpath);
		findpath = NULL;
		memfree((UCHAR **) findfile);
	}
	return i1;
}

INT fiofindclose()
{
	INT i1;

	if (findpath == NULL) return ERR_NOTOP;

	i1 = fioafindclose();
	memfree((UCHAR **) findpath);
	findpath = NULL;
	memfree((UCHAR **) findfile);
	return i1;
}

CHAR *fioerrstr(INT err)
{
	static CHAR work[64];
	CHAR *ptr;

	switch (err) {
		case ERR_NOTOP: return("file not open");
		case ERR_FNOTF: return("file not found");
		case ERR_NOACC: return("access denied");
		case ERR_EXIST: return("file already exists");
		case ERR_EXCMF: return("exceed maximum files open");
		case ERR_BADNM: return("invalid name");
		case ERR_BADTP: return("invalid file type");
		case ERR_NOEOR: return("no end of record mark or record too long");
		case ERR_SHORT: return("record too short");
		case ERR_BADCH: return("invalid character encountered");
		case ERR_RANGE: return("beyond end of file");
		case ERR_ISDEL: return("record has already been deleted");
		case ERR_BADIX: return("index file is invalid");
		case ERR_BADKL: return("wrong key length");
		case ERR_BADRL: return("wrong record length");
		case ERR_BADKY: return("invalid key");
		case ERR_NOMEM: return("unable to allocate memory");
		case ERR_RDERR:
			ptr = "unable to read";
			err = fioarderr;
			break;
		case ERR_WRERR:
			ptr = "unable to write";
			err = fioawrerr;
			break;
		case ERR_DLERR:
			ptr = "unable to delete";
			err = fioadlerr;
			break;
		case ERR_LKERR:
			ptr = "unable to lock file or record";
			err = fioalkerr;
			break;
		case ERR_BADLN: return("invalid character buffer length");
		case ERR_NOENV: return("unable to open environment file");
		case ERR_RDENV: return("unable to read environment file");
		case ERR_NOOPT: return("unable to open options file");
		case ERR_RDOPT: return("unable to read options file");
		case ERR_NOPRM: return("command line parameters not initialized");
		case ERR_RENAM: return("rename failed");
		case ERR_CLERR:
			ptr = "unable to close";
			err = fioaclerr;
			break;
		case ERR_SKERR:
			ptr = "unable to seek";
			err = fioaskerr;
			break;
		case ERR_BADLB: return("bad library");
		case ERR_FHNDL: return("invalid value for file handle");
		case ERR_RONLY: return("attempted write on read-only file");
		case ERR_OPERR:
			ptr = "unspecified open error";
			err = fioaoperr;
			break;
		case ERR_INVAR: return("invalid argument");
		case ERR_NOSEM:
#if OS_UNIX
			ptr = "no semaphores";
			err = fiosemerr;
			break;
#else
			return ("no semaphores");
#endif
		case ERR_NOEOF: return("no EOF");
		case ERR_COLAT: return("error opening or reading collate file");
		case ERR_CASMP: return("error opening or reading casemap file");
		case ERR_PROGX: return("programming error");
		case ERR_OTHER: return("unspecified error");
		default: return("* UNKNOWN ERROR *");
	}
	/* vague i/o error, try to include more information */
	strcpy(work, ptr);
	if (err) {
		strcat(work, ", error = ");
		mscitoa(err, work + strlen(work));
	}
	return(work);
}

static CHAR *fioinitprops(FIOPARMS *parms)
{
	/*static CHAR errmsg[256];*/
	INT i1, i2;
	OFFSET off;
	CHAR *ptr;
	assert (parms != NULL);
	memset(parms, 0, sizeof(*parms));

	if (!prpget("file", "sharing", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "off")) parms->flags |= FIO_FLAG_SINGLEUSER;
	if (!prpget("file", "ichrs", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) parms->flags |= FIO_FLAG_NOCOMPRESS;
	if (!prpget("file", "keytrunc", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) parms->flags |= FIO_FLAG_KEYTRUNCATE;
#if OS_UNIX
	if (prpget("file", "exclusive", NULL, NULL, &ptr, PRP_LOWER) || strcmp(ptr, "off")) parms->flags |= FIO_FLAG_EXCLOPENLOCK;
	if (!prpget("file", "lock", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "sem")) parms->flags |= FIO_FLAG_SEMLOCK;
#endif
	if (!prpget("file", "compat", NULL, NULL, &ptr, PRP_LOWER)) {
		if (!strncmp(ptr, "dos", 3)) parms->flags |= FIO_FLAG_COMPATDOS;
		else if (!strcmp(ptr, "rms")) parms->flags |= FIO_FLAG_COMPATRMS;
		else if (!strcmp(ptr, "rmsx")) parms->flags |= FIO_FLAG_COMPATRMSX;
		else if (!strcmp(ptr, "rmsy")) parms->flags |= FIO_FLAG_COMPATRMSY;
	}
	if (!prpget("file", "extcase", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "upper")) parms->flags |= FIO_FLAG_EXTCASEUPPER;
	if (!prpget("file", "namecase", NULL, NULL, &ptr, PRP_LOWER)) {
		if (!strcmp(ptr, "upper")) parms->flags |= FIO_FLAG_NAMECASEUPPER;
		else if (!strcmp(ptr, "lower")) parms->flags |= FIO_FLAG_NAMECASELOWER;
	}
	if (!prpget("file", "nameblanks", NULL, NULL, &ptr, PRP_LOWER)) {
		if (!strcmp(ptr, "squeeze")) parms->flags |= FIO_FLAG_NAMEBLANKSSQUEEZE;
		else if (!strcmp(ptr, "nochop")) parms->flags |= FIO_FLAG_NAMEBLANKSNOCHOP;
	}
	if (!prpget("file", "utilprep", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "dos")) parms->flags |= FIO_FLAG_UTILPREPDOS;

	if (!prpget("file", "randomwritelock", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "off"));
	else parms->flags |= FIO_FLAG_RANDWRTLCK;

	if (!prpget("file", "openlimit", NULL, NULL, &ptr, 0)) {
		i1 = atoi(ptr);
		if (i1 > 0) parms->openlimit = i1;
	}
	if (!prpget("file", "filetime", NULL, NULL, &ptr, 0)) {
		i1 = atoi(ptr);
		if (i1 >= -1) {
			parms->filetimeout = i1;
			parms->parmflags |= FIO_PARM_FILETIMEOUT;
		}
	}
	if (!prpget("file", "rectime", NULL, NULL, &ptr, 0)) {
		i1 = atoi(ptr);
		if (i1 >= -1) {
			parms->rectimeout = i1;
			parms->parmflags |= FIO_PARM_RECTIMEOUT;
		}
	}
#if OS_WIN32
	if (!prpget("file", "lockretrytime", NULL, NULL, &ptr, 0)) {
		i1 = atoi(ptr);
		if (i1 >= 0) {
			parms->lockretrytime = i1;
			parms->parmflags |= FIO_PARM_LOCKRETRYTIME;
		}
	}
#endif
	if (!prpget("file", "fileoffset", NULL, NULL, &ptr, 0)) {
		mscatooff(ptr, &off);
		if (off >= 0) {
			parms->fileoffset = off;
			parms->parmflags |= FIO_PARM_FILEOFFSET;
		}
	}
	if (!prpget("file", "recoffset", NULL, NULL, &ptr, 0)) {
		mscatooff(ptr, &off);
		if (off >= 0) {
			parms->recoffset = off;
			parms->parmflags |= FIO_PARM_RECOFFSET;
		}
	}
#if OS_UNIX
	if (!prpget("file", "excloffset", NULL, NULL, &ptr, 0)) {
		mscatooff(ptr, &off);
		if (off >= 0) {
			parms->excloffset = off;
			parms->parmflags |= FIO_PARM_EXCLOFFSET;
		}
	}
#endif
	for (i1 = 0, i2 = sizeof(parms->openpath) - 1; ; i1 = PRP_NEXT) {
		if (prpget("file", "open", "dir", NULL, &ptr, i1)) break;
		i2 -= (INT)strlen(ptr);
		if (i1) {
			if (--i2 > 0) strcat(parms->openpath, ";");
		}
		if (i2 > 0) strcat(parms->openpath, ptr);
	}
	for (i1 = 0, i2 = sizeof(parms->preppath) - 1; ; i1 = PRP_NEXT) {
		if (prpget("file", "prep", "dir", NULL, &ptr, i1)) break;
		i2 -= (INT)strlen(ptr);
		if (i1) {
			if (--i2 > 0) strcat(parms->preppath, ";");
		}
		if (i2 > 0) strcat(parms->preppath, ptr);
	}
	for (i1 = 0, i2 = sizeof(parms->srcpath) - 1; ; i1 = PRP_NEXT) {
		if (prpget("file", "source", "dir", NULL, &ptr, i1)) {
			if (!i1 && i2 > (INT) strlen(parms->openpath)) strcpy(parms->srcpath, parms->openpath);
			break;
		}
		i2 -= (INT)strlen(ptr);
		if (i1) {
			if (i2-- > 0) strcat(parms->srcpath, ";");
		}
		if (i2 > 0) strcat(parms->srcpath, ptr);
	}
	for (i1 = 0, i2 = sizeof(parms->dbcpath) - 1; ; i1 = PRP_NEXT) {
		if (prpget("file", "dbc", "dir", NULL, &ptr, i1)) break;
		i2 -= (INT)strlen(ptr);
		if (i1) {
			if (--i2 > 0) strcat(parms->dbcpath, ";");
		}
		if (i2 > 0) strcat(parms->dbcpath, ptr);
	}
	for (i1 = 0, i2 = sizeof(parms->editcfgpath) - 1; ; i1 = PRP_NEXT) {
		if (prpget("file", "editcfg", "dir", NULL, &ptr, i1)) break;
		i2 -= (INT)strlen(ptr);
		if (i1) {
			if (--i2 > 0) strcat(parms->editcfgpath, ";");
		}
		if (i2 > 0) strcat(parms->editcfgpath, ptr);
	}
	for (i1 = 0, i2 = sizeof(parms->prtpath); ; i1 = PRP_NEXT) {
		if (prpget("file", "prt", "dir", NULL, &ptr, i1)) {
			if (!i1 && i2 > (INT) strlen(parms->preppath)) strcpy(parms->prtpath, parms->preppath);
			break;
		}
		i2 -= (INT)strlen(ptr);
		if (i1) {
			if (--i2 > 0) strcat(parms->prtpath, ";");
		}
		if (i2 > 0) strcat(parms->prtpath, ptr);
	}
	for (i1 = 0, i2 = sizeof(parms->tdfpath) - 1; ; i1 = PRP_NEXT) {
		if (prpget("file", "tdf", "dir", NULL, &ptr, i1)) {
			if (!i1 && i2 > (INT) strlen(parms->srcpath)) strcpy(parms->tdfpath, parms->srcpath);
			break;
		}
		i2 -= (INT)strlen(ptr);
		if (i1) {
			if (--i2 > 0) strcat(parms->tdfpath, ";");
		}
		if (i2 > 0) strcat(parms->tdfpath, ptr);
	}
	for (i1 = 0, i2 = sizeof(parms->tdbpath) - 1; ; i1 = PRP_NEXT) {
		if (prpget("file", "tdb", "dir", NULL, &ptr, i1)) break;
		i2 -= (INT)strlen(ptr);
		if (i1) {
			if (--i2 > 0) strcat(parms->tdbpath, ";");
		}
		if (i2 > 0) strcat(parms->tdbpath, ptr);
	}
	for (i1 = 0, i2 = sizeof(parms->imgpath) - 1; ; i1 = PRP_NEXT) {
		if (prpget("file", "image", "dir", NULL, &ptr, i1)) {
			if (!i1 && i2 > (INT) strlen(parms->openpath)) strcpy(parms->imgpath, parms->openpath);
			break;
		}
		i2 -= (INT)strlen(ptr);
		if (i1) {
			if (--i2 > 0) strcat(parms->imgpath, ";");
		}
		if (i2 > 0) strcat(parms->imgpath, ptr);
	}
	if (!prpget("file", "casemap", NULL, NULL, &ptr, 0)) {
		for (i1 = 0; i1 <= UCHAR_MAX; i1++) parms->casemap[i1] = (unsigned char) toupper(i1);
		if (prptranslate(ptr, parms->casemap)) return "Invalid translate-spec for file casemap";
	}
	if (!prpget("file", "collate", NULL, NULL, &ptr, 0)) {
		for (i1 = 0; i1 <= UCHAR_MAX; i1++) parms->collatemap[i1] = (unsigned char) i1;
		if (prptranslate(ptr, parms->collatemap)) return "Invalid translate-spec for file collate";
	}
	if (!prpget("client", "filetransfer", "serverdir", NULL, &ptr, 0)) {
		strcpy(parms->cftpath, ptr);
	}
	parms->cvtvolfnc = fioinitcvtvol;

	return NULL;
}

/**
 * Returns 0 for success, 1 for not found, RC_NO_MEM if malloc failed
 * Does not move memory
 */
static INT fioinitcvtvol(CHAR *volume, CHAR ***directory)
{
	static CHAR *staticptr = NULL;
	if (staticptr == NULL) {
		staticptr = (CHAR *) malloc(4096 * sizeof(CHAR));
		if (staticptr == NULL) return RC_NO_MEM;
	}
	staticptr[0] = '\0';
	if (prpgetvol(volume, staticptr, 4096) || !*staticptr) return 1;
	if (directory != NULL) *directory = &staticptr;
	return 0;
}
