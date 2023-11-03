
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
#define PRT_C_

#define INC_STDIO
#define INC_STRING
#define INC_CTYPE
#define INC_STDLIB
#define INC_LIMITS
#define INC_MATH
#include "includes.h"
#include "base.h"
#include "dbcmd5.h"
#include "fio.h"
#include "fsfileio.h"
#include "gui.h"
#include "guix.h"
#include "prt.h"
#include "prtx.h"
#include "release.h"

#define PRT_DEFAULT_LINESIZE	1024
#define PRT_DEFAULT_BUFSIZE		2048
#define PRT_FCC_MAXSIZE			PRT_DEFAULT_LINESIZE

/* PDF defines */
/*
 * default DPI for pdf files
 */
#define PDFDPI 300

static INT printmax;
static INT printhi;
static INT (*getserver)(CHAR *, INT);
static UCHAR **translatetable;
static UINT pdfOldTabbing;	/* Use version 15 tabbing for PDFs */
static UINT TextRotateOld;	/* If TRUE, use version 16 text rotation semantics */

#if OS_UNIX
static INT prtcupswrite(INT prtnum);
#endif
static void normalizeUserPrintMargins(PRTMARGINS* in_mgns, PRTMARGINS* out_mgns);
static INT prtpclinit(INT prtnum);
static INT prtpclend(INT prtnum);
static INT prtpcllinenew(INT prtnum, INT xpos, INT ypos);
static INT prtpcllineold(INT prtnum, INT xpos, INT ypos);
static INT prtpclposnew(INT prtnum, INT xpos, INT ypos);
static INT prtpclposold(PRINTINFO *print, INT prtnum, INT xpos, INT ypos);
static INT prtpslimage(INT prtnum, void *pixhandle);
static INT prtpslinit(INT prtnum);
static INT prtpsl_DSC_Page(INT prtnum);
static INT prtpslend(INT prtnum);
static INT prtpslstartpage(INT prtnum);
static INT prtfilopen(INT prtnum, INT alloctype);
static INT prtfilclose(INT prtnum);
static INT prtfilwrite(INT prtnum);
static INT prtrecopen(INT prtnum, INT alloctype);
static INT prtrecclose(INT prtnum);
static INT prtrecwrite(INT prtnum);

/*
 * The Mac has fcvt but does not supply a prototype for it!
 */
#ifdef __MACOSX
CHAR *fcvt(DOUBLE, INT, INT *, INT *);
#endif

INT prtinit(PRTINITINFO *initinfo)
{
	INT i1, i2, i3;
	USHORT mapoff, *maptab;
	CHAR *ptr;
	UCHAR **mapptr;

	*prterrorstring = '\0';
	getserver = initinfo->getserver;
	if (initinfo->translate != NULL && *initinfo->translate) {
		ptr = initinfo->translate;
		mapptr = memalloc((INT)(256 * sizeof(USHORT) + strlen(ptr)), MEMFLAGS_ZEROFILL);
		if (mapptr == NULL) return PRTERR_NOMEM;
		maptab = (USHORT *) *mapptr;
		mapoff = 256 * sizeof(USHORT);
		prtputerror(ptr);
		for ( ; ; ) {
			while (isspace((int)*ptr)) ptr++;
			if (*ptr == ';') {
				ptr++;
				continue;
			}
			if (!*ptr) break;
			if (!isdigit((int)*ptr)) {
				memfree(mapptr);
				return PRTERR_BADTRANS;
			}
			for (i1 = 0; isdigit((int)*ptr); ptr++) i1 = i1 * 10 + *ptr - '0';
			while (isspace((int)*ptr)) ptr++;
			if (*ptr == '-') {
				ptr++;
				while (isspace((int)*ptr)) ptr++;
				if (!isdigit((int)*ptr)) {
					memfree(mapptr);
					return PRTERR_BADTRANS;
				}
				for (i2 = 0; isdigit((int)*ptr); ptr++) i2 = i2 * 10 + *ptr - '0';
				while (isspace((int)*ptr)) ptr++;
			}
			else i2 = i1;
			if (i1 > UCHAR_MAX || i2 > UCHAR_MAX || *ptr != ':') {
				memfree(mapptr);
				return PRTERR_BADTRANS;
			}
			ptr++;
			while (isspace((int)*ptr)) ptr++;
			if (*ptr == '(') {
				ptr++;
				while (i1 <= i2) maptab[i1++] = mapoff;
			}
			for ( ; ; ) {
				while (isspace((int)*ptr)) ptr++;
				if (!isdigit((int)*ptr)) {
					memfree(mapptr);
					return PRTERR_BADTRANS;
				}
				for (i3 = 0; isdigit((int)*ptr); ptr++) i3 = i3 * 10 + *ptr - '0';
				if (i1 <= i2) {
					if (i3 + (i2 - i1) > UCHAR_MAX) {
						memfree(mapptr);
						return PRTERR_BADTRANS;
					}
					while (i1 <= i2) {
						maptab[i1++] = mapoff;
						(*mapptr)[mapoff++] = (unsigned char) i3++;
						(*mapptr)[mapoff++] = '\0';
					}
					break;
				}
				if (i3 > UCHAR_MAX) {
					memfree(mapptr);
					return PRTERR_BADTRANS;
				}
				(*mapptr)[mapoff++] = (unsigned char) i3;
				while (isspace((int)*ptr)) ptr++;
				if (*ptr == ',') {
					ptr++;
					continue;
				}
				if (*ptr != ')') {
					memfree(mapptr);
					return PRTERR_BADTRANS;
				}
				(*mapptr)[mapoff++] = '\0';
			}
		}
		prtputerror("");
		memchange(mapptr, mapoff, 0);
		translatetable = mapptr;
	}
	if (initinfo->program != NULL) strcpy(program, initinfo->program);
	if (initinfo->version != NULL) strcpy(version, initinfo->version);
	pdfOldTabbing = initinfo->pdfOldTabbing;
	TextRotateOld = initinfo->TextRotateOld;
	return prtainit(initinfo);
}

INT prtexit()
{
	INT i1, prtnum, rc;

	*prterrorstring = '\0';
	rc = 0;
	for (prtnum = 0; prtnum < printhi && (*printtabptr + prtnum)->flags; ) {
		i1 = prtclose(++prtnum, NULL);
		if (!rc) rc = i1;
	}
	getserver = NULL;
	memfree(translatetable);
	translatetable = NULL;
	if (rle_buffer != NULL) {
		free(rle_buffer);
		rle_buffer = NULL;
		rle_buffer_size = 0;
	}
	i1 = prtaexit();
	if (!rc) rc = i1;
	return i1;
}

INT prtpagesetup()
{
#if OS_WIN32GUI
	INT i1;
	*prterrorstring = '\0';
	i1 = prtadrvpagesetup();
	return i1;
#else
	return 0;
#endif
}

/**
 * prtnum The address of an INT to receive the One-Based index into the printtabptr
 */
INT prtopen(CHAR *prtname, PRTOPTIONS *options, INT *prtnum)
{
	INT i1, i2, i3, flags, num, openflags;
	USHORT mapoff, *maptab;
	CHAR *ptr, prtwork[MAX_NAMESIZE];
	UCHAR **binptr, **docptr, **nameptr, **mapptr, **paperptr;
	PRINTINFO *print;

	*prterrorstring = '\0';
	/* find empty spool file table entry */
	for (num = 0; num < printhi && (*printtabptr + num)->flags; num++);
	if (num == printmax) {
		if (!printmax) {
			i1 = 4;
			i2 = ((printtabptr = (PRINTINFO **) memalloc(4 * sizeof(PRINTINFO), 0)) == NULL);
		}
		else {
			i1 = printmax << 1;
			i2 = memchange((UCHAR **) printtabptr, i1 * sizeof(PRINTINFO), 0);
		}
		if (i2) return PRTERR_NOMEM;
		printmax = i1;
	}

	if (options->dpi == 0) options->dpi = PDFDPI;
	openflags = options->flags;

	/* get printer type */
	for (i1 = (INT)strlen(prtname); i1 && prtname[i1 - 1] == ' '; i1--);
	memcpy(prtwork, prtname, i1);
	prtwork[i1] = '\0';
#if OS_UNIX
	if (openflags & PRT_PIPE) flags = PRTFLAG_TYPE_PIPE;
	else
#endif
	/*
	 * if Unix, prtanametype could return PRTFLAG_TYPE_DEVICE, PRTFLAG_TYPE_PIPE, PRTFLAG_TYPE_FILE,
	 * or PRTFLAG_TYPE_CUPS.
	 * if Windows, the return could be PRTFLAG_TYPE_PRINTER, PRTFLAG_TYPE_DRIVER, PRTFLAG_TYPE_DEVICE,
	 * or PRTFLAG_TYPE_FILE.
	 */
	flags = prtanametype(prtwork);

	if (!flags) return PRTERR_BADNAME;

	/* set printer format */
#if OS_WIN32
	if (flags == PRTFLAG_TYPE_DRIVER ||
		(flags == PRTFLAG_TYPE_PRINTER
				&& (openflags & PRT_WDRV)
				&& !(openflags & (PRT_LPCL | PRT_LPSL | PRT_LPDF | PRT_LCTL)))
	)
	{
		if (openflags & PRT_WRAW) return PRTERR_BADOPT;
		flags = PRTFLAG_TYPE_DRIVER | PRTFLAG_FORMAT_GUI;
	}
	else
#endif
	if (openflags & PRT_LPCL) flags |= PRTFLAG_FORMAT_PCL;
	else if (openflags & PRT_LPSL) flags |= PRTFLAG_FORMAT_PSL;
	else if (openflags & PRT_LPDF) flags |= PRTFLAG_FORMAT_PDF;
	else if ((openflags & PRT_LCTL) || flags != PRTFLAG_TYPE_FILE) flags |= PRTFLAG_FORMAT_DCC;
	else flags = PRTFLAG_TYPE_RECORD | PRTFLAG_FORMAT_FCC;

	if (!prpget("print", "translate", NULL, NULL, &ptr, 0) && *ptr) {
		mapptr = memalloc((INT)(256 * sizeof(USHORT) + strlen(ptr)), MEMFLAGS_ZEROFILL);
		if (mapptr == NULL) return PRTERR_NOMEM;
		maptab = (USHORT *) *mapptr;
		mapoff = 256 * sizeof(USHORT);
		prtputerror(ptr);
		for ( ; ; ) {
			while (isspace((int)*ptr)) ptr++;
			if (*ptr == ';') {
				ptr++;
				continue;
			}
			if (!*ptr) break;
			if (!isdigit((int)*ptr)) {
				memfree(mapptr);
				return PRTERR_BADTRANS;
			}
			for (i1 = 0; isdigit((int)*ptr); ptr++) i1 = i1 * 10 + *ptr - '0';
			while (isspace((int)*ptr)) ptr++;
			if (*ptr == '-') {
				ptr++;
				while (isspace((int)*ptr)) ptr++;
				if (!isdigit((int)*ptr)) {
					memfree(mapptr);
					return PRTERR_BADTRANS;
				}
				for (i2 = 0; isdigit((int)*ptr); ptr++) i2 = i2 * 10 + *ptr - '0';
				while (isspace((int)*ptr)) ptr++;
			}
			else i2 = i1;
			if (i1 > UCHAR_MAX || i2 > UCHAR_MAX || *ptr != ':') {
				memfree(mapptr);
				return PRTERR_BADTRANS;
			}
			ptr++;
			while (isspace((int)*ptr)) ptr++;
			if (*ptr == '(') {
				ptr++;
				while (i1 <= i2) maptab[i1++] = mapoff;
			}
			for ( ; ; ) {
				while (isspace((int)*ptr)) ptr++;
				if (!isdigit((int)*ptr)) {
					memfree(mapptr);
					return PRTERR_BADTRANS;
				}
				for (i3 = 0; isdigit((int)*ptr); ptr++) i3 = i3 * 10 + *ptr - '0';
				if (i1 <= i2) {
					if (i3 + (i2 - i1) > UCHAR_MAX) {
						memfree(mapptr);
						return PRTERR_BADTRANS;
					}
					while (i1 <= i2) {
						maptab[i1++] = mapoff;
						(*mapptr)[mapoff++] = (unsigned char) i3++;
						(*mapptr)[mapoff++] = '\0';
					}
					break;
				}
				if (i3 > UCHAR_MAX) {
					memfree(mapptr);
					return PRTERR_BADTRANS;
				}
				(*mapptr)[mapoff++] = (unsigned char) i3;
				while (isspace((int)*ptr)) ptr++;
				if (*ptr == ',') {
					ptr++;
					continue;
				}
				if (*ptr != ')') {
					memfree(mapptr);
					return PRTERR_BADTRANS;
				}
				(*mapptr)[mapoff++] = '\0';
			}
		}
		prtputerror("");
		memchange(mapptr, mapoff, 0);
	}
	else mapptr = NULL;

	if (options->papersize[0]) {
		paperptr = memalloc((INT)strlen(options->papersize) + 1, 0);
		if (paperptr == NULL) {
			memfree(mapptr);
			return PRTERR_NOMEM;
		}
		strcpy((CHAR *) *paperptr, options->papersize);
	}
	else paperptr = NULL;

	if (options->paperbin[0]) {
		binptr = memalloc((INT)strlen(options->paperbin) + 1, 0);
		if (binptr == NULL) {
			memfree(paperptr);
			memfree(mapptr);
			return PRTERR_NOMEM;
		}
		strcpy((CHAR *) *binptr, options->paperbin);
	}
	else binptr = NULL;

	if (options->docname[0]) {
		docptr = memalloc((INT)strlen(options->docname) + 1, 0);
		if (docptr == NULL) {
			memfree(binptr);
			memfree(paperptr);
			memfree(mapptr);
			return PRTERR_NOMEM;
		}
		strcpy((CHAR *) *docptr, options->docname);
	}
	else docptr = NULL;

	if (flags & (PRTFLAG_TYPE_FILE | PRTFLAG_TYPE_RECORD)) {
		if (openflags & PRT_NOEX) ptr = "";
		else ptr = ".prt";
		miofixname(prtwork, ptr, FIXNAME_EXT_ADD);
	}
	i1 = (INT)strlen(prtwork);
	nameptr = memalloc(i1 + 1, 0);
	if (nameptr == NULL) {
		memfree(docptr);
		memfree(binptr);
		memfree(paperptr);
		memfree(mapptr);
		return PRTERR_NOMEM;
	}
	strcpy((CHAR *) *nameptr, prtwork);

	print = *printtabptr + num;
	memset((UCHAR *) print, 0, sizeof(PRINTINFO));
	print->flags = flags;
	print->openflags = openflags;
	print->copies = options->copies;
	print->papersize = (CHAR **) paperptr;
	print->paperbin = (CHAR **) binptr;
	print->docname = (CHAR **) docptr;
	print->dpi = options->dpi;
	print->prtname = (CHAR **) nameptr;
	print->map = mapptr;
	print->margins = options->margins;
	/* Note that num is incremented below */
	if (++num > printhi) printhi = num;
	if ((openflags & (PRT_TEST | PRT_JDLG)) || (flags & (PRTFLAG_TYPE_FILE | PRTFLAG_TYPE_RECORD))) {
		if (!(openflags & PRT_TEST)) openflags |= PRT_WAIT;
		i1 = prtalloc(num, openflags & (PRT_TEST | PRT_WAIT));
		if (i1) {
			print = *printtabptr + num - 1;
			print->flags = 0;
			if (num == printhi) printhi = num - 1;
			memfree(nameptr);
			memfree(docptr);
			memfree(binptr);
			memfree(paperptr);
			memfree(mapptr);
			return i1;
		}
	}

	*prtnum = num; /* num has been incremented, so the prtnum value is ONE-based */
	return 0;
}

/*
 * prtnum is ONE-based here
 */
INT prtclose(INT prtnum, PRTOPTIONS *options)
{
	INT i1, cups = 0;
	CHAR prtfile[MAX_NAMESIZE], *ptr;
	PRINTINFO *print;

	print = *printtabptr + --prtnum;

	*prterrorstring = '\0';
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;

	if (options != NULL) {
		if (options->docname[0] == '\0') strcpy(options->docname, *print->docname);
		if (print->paperbin != NULL) strcpy(options->paperbin, *print->paperbin);
		if (print->papersize != NULL) strcpy(options->papersize, *print->papersize);
		if (print->openflags & PRT_LAND) options->flags |= PRT_LAND;
		if (options->copies == 1 && print->copies != 1) options->copies = print->copies;
		if (print->openflags & PRT_BANR) options->flags |= PRT_BANR;
		if ((options->flags & PRT_SBMT) || (print->flags & PRTFLAG_TYPE_CUPS)) {
			if ((print->flags & (PRTFLAG_TYPE_FILE | PRTFLAG_TYPE_RECORD)) && !(print->flags & PRTFLAG_SERVER)) {
				ptr = NULL;
				if (print->flags & PRTFLAG_ALLOC) ptr = fioname(print->prthandle.handle);
				if (ptr != NULL) strcpy(prtfile, ptr);
				else strcpy(prtfile, *print->prtname);
				if (print->flags & PRTFLAG_FORMAT_FCC) options->flags &= ~PRT_LCTL;
				else options->flags |= PRT_LCTL;
			}
			else if (print->flags & PRTFLAG_TYPE_CUPS) {
				ptr = fioname(print->prthandle.handle);
				strcpy(prtfile, ptr);
				options->flags |= PRT_SBMT | PRT_REMV;
				strcpy(options->submit.prtname, *print->prtname);
				cups = 1;
			}
			else options->flags &= ~PRT_SBMT;
		}
	}
	i1 = prtfree(prtnum + 1);
	print = *printtabptr + prtnum;
	memfree((UCHAR **) print->prtname);
	memfree((UCHAR **) print->docname);
	memfree((UCHAR **) print->paperbin);
	memfree((UCHAR **) print->papersize);
	memfree(print->map);
	print->flags = 0;
	if (prtnum + 1 == printhi) printhi = prtnum;

	if (i1) {
		return i1;
	}
	if (options != NULL && (options->flags & PRT_SBMT)) {
#if OS_UNIX && defined(CUPS)
		if (cups) i1 = prtacupssubmit(prtfile, options);
		else i1 = prtasubmit(prtfile, options);
#else
		i1 = prtasubmit(prtfile, options);
#endif
		return i1;
	}
	return 0;
}

INT prtalloc(INT prtnum, INT type)
{
	INT i1, bufsize;
	UCHAR **buffer, **line;
	PRINTINFO *print;

	*prterrorstring = '\0';
	print = *printtabptr + --prtnum;
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	if (!type) return (print->flags & PRTFLAG_ALLOC) ? 1 : 0;
	if (print->flags & PRTFLAG_ALLOC) return 0;

	if (!(print->flags & PRTFLAG_FORMAT_GUI)) {
		if (print->flags & PRTFLAG_FORMAT_FCC) bufsize = PRT_FCC_MAXSIZE + 1;
		else bufsize = PRT_DEFAULT_BUFSIZE;
		buffer = memalloc(bufsize, 0);
		if (buffer == NULL) return PRTERR_NOMEM;
	}
	else {
		buffer = NULL;
		bufsize = 0;
	}
	line = memalloc(PRT_DEFAULT_LINESIZE, 0);
	if (line == NULL) {
		memfree(line);
		memfree(buffer);
		return PRTERR_NOMEM;
	}

	i1 = PRTERR_INTERNAL; // In case none of the following IFs are true
	if (print->flags & PRTFLAG_TYPE_FILE) {
		i1 = prtfilopen(prtnum, type);
	}
	else if (print->flags & PRTFLAG_TYPE_RECORD) {
		i1 = prtrecopen(prtnum, type);
	}
	else if (print->flags & PRTFLAG_TYPE_DEVICE) {
		i1 = prtadevopen(prtnum, type);
	}
#if OS_WIN32
	else if (print->flags & PRTFLAG_TYPE_PRINTER) {
		i1 = prtaprtopen(prtnum);
	}
	else if (print->flags & PRTFLAG_TYPE_DRIVER) {
		i1 = prtadrvopen(prtnum);
	}
#endif
#if OS_UNIX
	else if (print->flags & PRTFLAG_TYPE_PIPE) {
		i1 = prtapipopen(prtnum, type);
	}
#ifdef CUPS
	else if (print->flags & PRTFLAG_TYPE_CUPS) {
		i1 = prtacupsopen(prtnum, type);
	}
#endif
#endif
	if (i1) {
		memfree(line);
		memfree(buffer);
		return i1;
	}

	print = *printtabptr + prtnum;
	print->flags |= PRTFLAG_ALLOC;
	print->linebuf = line;
	print->linesize = PRT_DEFAULT_LINESIZE;
	print->buffer = buffer;
	print->bufsize = bufsize;

	if (print->flags & PRTFLAG_FORMAT_PDF) {
/*** NOTE: initialize here as prtpdfinit calls memalloc and this can not be ***/
/***       done from prttext ***/
		i1 = prtpdfinit(prtnum);
		print = *printtabptr + prtnum;
		if (i1) {
			if (print->flags & PRTFLAG_TYPE_FILE) prtfilclose(prtnum);
			else if (print->flags & PRTFLAG_TYPE_RECORD) prtrecclose(prtnum);
			else if (print->flags & PRTFLAG_TYPE_DEVICE) prtadevclose(prtnum);
#if OS_WIN32
			else if (print->flags & PRTFLAG_TYPE_PRINTER) prtaprtclose(prtnum);
			else if (print->flags & PRTFLAG_TYPE_DRIVER) prtadrvclose(prtnum);
#endif
#if OS_UNIX
			else if (print->flags & PRTFLAG_TYPE_PIPE) prtapipclose(prtnum);
#endif
			print->flags &= PRTFLAG_TYPE_MASK | PRTFLAG_FORMAT_MASK;
			memfree(line);
			memfree(buffer);
			/* clear everything below allocinfostart */
			memset(&print->allocinfostart, 0, sizeof(PRINTINFO) - (INT)(&print->allocinfostart - (CHAR *) print));
			return i1;
		}
	}
	return 0;
}

INT prtfree(INT prtnum)
{
	INT i1, rc = 0;
	PRINTINFO *print;
	*prterrorstring = '\0';
	print = *printtabptr + --prtnum;
	if (prtnum < 0 || prtnum >= printhi || !print->flags) {
		return PRTERR_NOTOPEN;
	}
	if (!(print->flags & PRTFLAG_ALLOC)) {
		return 0;
	}

	rc = prtflushline(prtnum, TRUE);

	print = *printtabptr + prtnum;
	if ((print->flags & (PRTFLAG_FORMAT_PCL | PRTFLAG_FORMAT_PSL | PRTFLAG_FORMAT_PDF)) && (print->flags & PRTFLAG_INIT)) {
		if (print->flags & PRTFLAG_FORMAT_PCL) i1 = prtpclend(prtnum);
		else if (print->flags & PRTFLAG_FORMAT_PSL) i1 = prtpslend(prtnum);
		else i1 = prtpdfend(prtnum);
		if (!rc) rc = i1;
		print = *printtabptr + prtnum;
	}

	i1 = prtflushbuf(prtnum);
	if (!rc) rc = i1;
	if (print->flags & PRTFLAG_TYPE_FILE) {
		i1 = prtfilclose(prtnum);
	}
	else if (print->flags & PRTFLAG_TYPE_RECORD) {
		i1 = prtrecclose(prtnum);
	}
	else if (print->flags & PRTFLAG_TYPE_DEVICE) {
		i1 = prtadevclose(prtnum);
	}
#if OS_WIN32
	else if (print->flags & PRTFLAG_TYPE_PRINTER) {
		i1 = prtaprtclose(prtnum);
	}
	else if (print->flags & PRTFLAG_TYPE_DRIVER) {
		i1 = prtadrvclose(prtnum);
	}
#endif
#if OS_UNIX
	else if (print->flags & PRTFLAG_TYPE_PIPE) {
		i1 = prtapipclose(prtnum);
	}
#ifdef CUPS
	else if (print->flags & PRTFLAG_TYPE_CUPS) {
		i1 = prtacupsclose(prtnum);
	}
#endif
#endif
	if (!rc) rc = i1;

	print->flags &= PRTFLAG_TYPE_MASK | PRTFLAG_FORMAT_MASK;
	memfree(print->linebuf);
	memfree(print->buffer);
	/* clear everything below allocinfostart */
	memset(&print->allocinfostart, 0, sizeof(PRINTINFO) - (INT)(&print->allocinfostart - (CHAR *) print));
	return rc;
}

/**
 * Print a rectangle
 * The current position is one corner of the rectangle,
 * the given coordinates are the other corner
 */
INT prtrect(INT prtnum, INT x, INT y) {
	PRINTINFO *print;

	print = *printtabptr + --prtnum;
	if (print->flags & PRTFLAG_FORMAT_PDF) return(prtpdfrect(prtnum, x, y, 1));
#if OS_WIN32
	else if (print->flags & PRTFLAG_FORMAT_GUI) return(prtadrvrect(prtnum, x, y, 1));
#endif
	return 0;
}

INT prtbox(INT prtnum, INT x, INT y) {
	PRINTINFO *print;
	print = *printtabptr + --prtnum;
	if (print->flags & PRTFLAG_FORMAT_PDF) return(prtpdfrect(prtnum, x, y, 0));
#if OS_WIN32
	else if (print->flags & PRTFLAG_FORMAT_GUI) return(prtadrvrect(prtnum, x, y, 0));
#endif
	return 0;
}

INT prtcircle(INT prtnum, INT r) {
	PRINTINFO *print;
	print = *printtabptr + --prtnum;
	if (print->flags & PRTFLAG_FORMAT_PDF) return(prtpdfcircle(prtnum, r, 0));
#if OS_WIN32
	else if (print->flags & PRTFLAG_FORMAT_GUI) return(prtadrvcircle(prtnum, r, 0));
#endif
	return 0;
}

INT prtbigdot(INT prtnum, INT r) {
	PRINTINFO *print;
	print = *printtabptr + --prtnum;
	if (print->flags & PRTFLAG_FORMAT_PDF) return(prtpdfcircle(prtnum, r, 1));
#if OS_WIN32
	else if (print->flags & PRTFLAG_FORMAT_GUI) return(prtadrvcircle(prtnum, r, 1));
#endif
	return 0;
}

INT prttriangle(INT prtnum, INT x1, INT y1, INT x2, INT y2) {
	PRINTINFO *print;
	print = *printtabptr + --prtnum;
	if (print->flags & PRTFLAG_FORMAT_PDF) return(prtpdftriangle(prtnum, x1, y1, x2, y2));
#if OS_WIN32
	else if (print->flags & PRTFLAG_FORMAT_GUI) return(prtadrvtriangle(prtnum, x1, y1, x2, y2));
#endif
	return 0;
}

INT prttext(INT prtnum, UCHAR *string, UCHAR chr, INT len, INT blanks, INT type, INT value, INT angle)
{
	INT i1, i2, i3, i4, linebgn, linepos, linepossave, worklen;
	UCHAR *buf, *map;
	PRINTINFO *print;

	*prterrorstring = '\0';
	if (len < 0) len = 0;
	if (blanks < 0) blanks = 0;
	if (!len && !blanks) return 0;
	if ((angle %= 360) < 0) angle += 360;

	print = *printtabptr + --prtnum;
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	if (print->linebuf == NULL) return PRTERR_NOTOPEN; /* Printer has been closed */

	/* If this is for PDF on OSX, transfer control to other module */
#ifdef __MACOSX
	if (print->flags & PRTFLAG_FORMAT_PDF) {
		return prtpdfosxtext(prtnum, string, chr, len, blanks, type, value, angle);
	}
#endif
	if ((print->flags & (PRTFLAG_FORMAT_PCL | PRTFLAG_FORMAT_PSL)) && !(print->flags & PRTFLAG_INIT)) {
		if (print->flags & PRTFLAG_FORMAT_PCL) i1 = prtpclinit(prtnum);
		else i1 = prtpslinit(prtnum);
		if (i1) return i1;
		print = *printtabptr + prtnum;
	}

	linepos = print->linepos - print->linebgn;
	if (linepos < 0) {  /* should not happen for non-DCC */
		linepos = print->linepos;
		i1 = prtflushline(prtnum, TRUE);
		if (i1) return i1;
		print = *printtabptr + prtnum;
		if (print->flags & PRTFLAG_FORMAT_DCC) {
			i1 = prtbufcopy(prtnum, "\015", 1);		/* do not use "\x0d" as some cruddy old compilers no capice */
			if (i1) return i1;
		}
		print->linepos = linepos;
	}
	if (print->flags & (PRTFLAG_FORMAT_FCC | PRTFLAG_FORMAT_DCC)) {
		if (linepos < print->linecnt) {
			i1 = print->linecnt - linepos;
			if (i1 > len) i1 = len;
			for (buf = *print->linebuf + linepos; i1 && *buf == ' '; i1--, buf++);
			if (i1) {
				linepos = print->linepos;
				i1 = prtflushline(prtnum, TRUE);
				if (i1) return i1;
				print = *printtabptr + prtnum;
				if (print->flags & PRTFLAG_FORMAT_DCC) {
					i1 = prtbufcopy(prtnum, "\015", 1);
					if (i1) return i1;
				}
			}
		}
	}
	else if (print->flags & (PRTFLAG_FORMAT_PCL | PRTFLAG_FORMAT_PSL | PRTFLAG_FORMAT_PDF | PRTFLAG_FORMAT_GUI)) {
		if ((print->flags & PRTFLAG_FORMAT_PDF) && print->fmt.pdf.fontselected < 4 /* Courier */
				&& !type && !print->linecnt && print->linebgn) {
			print->linebgn = 0;
			linepossave = linepos = print->linepos;
			print->fmt.pdf.posx = (DOUBLE)0;
		}
		else linepossave = 0;
		if (type != print->linetype || ((type & PRT_TEXT_ANGLE) && angle != print->lineangle)) {
			i1 = prtflushline(prtnum, FALSE);
			if (i1) return i1;
			print = *printtabptr + prtnum;
			linepos = linepossave;
		}
	}

	print->linetype = type;
	print->linevalue = value;
	print->lineangle = angle;

	/* calculate length of text */
	if (print->map != NULL) {
		map = *print->map;
		if (string != NULL) {
			for (i2 = worklen = 0; i2 < len; i2++) {
				i1 = ((USHORT *) map)[string[i2]];
				if (i1) worklen += (INT)strlen((CHAR *) map + i1);
				else worklen++;
			}
		}
		else {
			i1 = ((USHORT *) map)[chr];
			if (i1) worklen = len * (INT)strlen((CHAR *) map + i1);
			else worklen = len;
		}
	}
	else worklen = len;


	/* test for non-blank overflow */
	if ((print->flags & PRTFLAG_FORMAT_FCC) && worklen + blanks > print->linesize - linepos) {
		if (worklen > print->linesize - linepos) {
			if (print->map != NULL) map = *print->map;
			else {
				map = NULL;
				i1 = 0;
			}
			if (string != NULL) {
				do {
					if (string[--len] != ' ') return PRTERR_TOOLONG;
					if (map != NULL) i1 = ((USHORT *) map)[string[len]];
					if (i1) worklen -= (INT)strlen((CHAR *) map + i1);
					else worklen--;
				} while (worklen > print->linesize - linepos);
			}
			else {
				if (chr != ' ') return PRTERR_TOOLONG;
				if (map != NULL) i1 = ((USHORT *) map)[chr];
				if (i1) {
					i3 = (INT)strlen((CHAR *) map + i1);
					len = (print->linesize - linepos) / i3;
					worklen = len * i3;
				}
				else worklen = len = print->linesize - linepos;
			}
			blanks = 0;
		}
		else if (blanks > print->linesize - linepos - worklen) blanks = print->linesize - linepos - worklen;
	}

	/* copy text to line buffer */
	linebgn = print->linebgn;
	if (worklen) {
		if (linepos > print->linecnt) {
			if (!(print->flags & PRTFLAG_FORMAT_FCC) && linepos > print->linesize) {
				print->linebgn = print->linepos = 0;  /* prevent prtflushline duplicating blank fill */
				do {
					memset(*print->linebuf + print->linecnt, ' ', print->linesize - print->linecnt);
					print->linecnt = print->linesize;
					i1 = prtflushline(prtnum, FALSE);
					if (i1) return i1;
					linebgn += print->linesize;
					linepos -= print->linesize;
				} while (linepos > print->linesize);
			}
			memset(*print->linebuf + print->linecnt, ' ', linepos - print->linecnt);
		}
		if (print->map != NULL) {
			map = *print->map;
			if (string != NULL) {
				for (i2 = 0; i2 < len; i2++) {
					i1 = ((USHORT *) map)[string[i2]];
					if (i1) i3 = (INT)strlen((CHAR *) map + i1);
					else i3 = 1;
					if (!(print->flags & PRTFLAG_FORMAT_FCC) && linepos + i3 > print->linesize) {
						print->linecnt = linepos;
						i4 = prtflushline(prtnum, FALSE);
						if (i4) return i4;
						linebgn += linepos;
						linepos = 0;
					}
					if (i1) {
						if (i3 == 1) (*print->linebuf)[linepos] = map[i1];
						else memcpy(*print->linebuf + linepos, map + i1, i3);
					}
					else (*print->linebuf)[linepos] = string[i2];
					linepos += i3;
				}
			}
			else {
				i1 = ((USHORT *) map)[chr];
				if (i1) {
					i3 = (INT)strlen((CHAR *) map + i1);
					if (i3 == 1) chr = map[i1];
				}
				else i3 = 1;
				if (i3 == 1) {
					if (!(print->flags & PRTFLAG_FORMAT_FCC) && linepos + worklen > print->linesize) {
						do {
							memset(*print->linebuf + linepos, chr, print->linesize - linepos);
							print->linecnt = print->linesize;
							i1 = prtflushline(prtnum, FALSE);
							if (i1) return i1;
							linebgn += print->linesize;
							worklen -= print->linesize - linepos;
							linepos = 0;
						} while (worklen > print->linesize);
					}
					memset(*print->linebuf + linepos, chr, worklen);
					linepos += worklen;
				}
				else {
					buf = map + i1;
					for (i2 = 0; i2 < len; i2++) {
						if (!(print->flags & PRTFLAG_FORMAT_FCC) && linepos + i3 > print->linesize) {
							print->linecnt = linepos;
							i4 = prtflushline(prtnum, FALSE);
							if (i4) return i4;
							linebgn += linepos;
							linepos = 0;
						}
						memcpy(*print->linebuf + linepos, buf, i3);
						linepos += i3;
					}
				}
			}
		}
		else {
			if (!(print->flags & PRTFLAG_FORMAT_FCC) && linepos + worklen > print->linesize) {
				do {
					if (string != NULL) {
						memcpy(*print->linebuf + linepos, string, print->linesize - linepos);
						string += print->linesize - linepos;
					}
					else {
						/*memset(*print->linebuf + linepos, chr, worklen);*/
						memset(*print->linebuf + linepos, chr, print->linesize - linepos);
					}
					print->linecnt = print->linesize;
					i1 = prtflushline(prtnum, FALSE);
					if (i1) return i1;
					linebgn += print->linesize;
					worklen -= print->linesize - linepos;
					linepos = 0;
				} while (worklen > print->linesize);
			}
			if (string != NULL) {
				memcpy(*print->linebuf + linepos, string, worklen);
			}
			else memset(*print->linebuf + linepos, chr, worklen);
			linepos += worklen;
		}
		if (linepos > print->linecnt) print->linecnt = linepos;
	}
	/* append blanks */
	linepos += blanks;
	if (linepos > print->linecnt) {
		if (!(print->flags & PRTFLAG_FORMAT_FCC) && linepos > print->linesize) {
			do {
				memset(*print->linebuf + print->linecnt, ' ', print->linesize - print->linecnt);
				print->linecnt = print->linesize;
				i1 = prtflushline(prtnum, FALSE);
				if (i1) return i1;
				linebgn += print->linesize;
				linepos -= print->linesize;
			} while (linepos > print->linesize);
		}
		memset(*print->linebuf + print->linecnt, ' ', linepos - print->linecnt);
		print->linecnt = linepos;
	}
	print->linebgn = linebgn;
	print->linepos = linebgn + linepos;
	return 0;
}

INT prtnl(INT prtnum, INT cnt)
{
	INT i1;
	PRINTINFO *print;

	*prterrorstring = '\0';
	if (cnt < 1) return 0;

	print = *printtabptr + --prtnum;
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	if ((print->flags & (PRTFLAG_FORMAT_PCL | PRTFLAG_FORMAT_PSL)) && !(print->flags & PRTFLAG_INIT)) {
		if (print->flags & PRTFLAG_FORMAT_PCL) i1 = prtpclinit(prtnum);
		else i1 = prtpslinit(prtnum);
		if (i1) return i1;
	}

	i1 = prtflushline(prtnum, TRUE);
	if (i1) return i1;

	print = *printtabptr + prtnum;
	if (print->flags & (PRTFLAG_FORMAT_DCC | PRTFLAG_FORMAT_PCL)) {
		while (cnt--) {
			i1 = prtbufcopy(prtnum, "\015\012", 2);
			if (i1) return i1;
		}
	}
	else if (print->flags & PRTFLAG_FORMAT_FCC) {
		print->fmt.fcc.prefix = ' ';
		while (--cnt) {
			i1 = prtflushline(prtnum, FALSE);
			if (i1) return i1;
			print->fmt.fcc.prefix = ' ';
		}
	}
	else if (print->flags & PRTFLAG_FORMAT_PSL) {
		while (cnt--) {
			i1 = prtbufcopy(prtnum, "CR LF\n", 6);
			if (i1) return i1;
		}
	}
	else if (print->flags & PRTFLAG_FORMAT_PDF) {
		print->fmt.pdf.posx = (DOUBLE) 0;
#ifdef __MACOSX
		return prtpdfosxlf(prtnum, cnt);
#else
		if (print->fmt.pdf.fontselected >= 8 && print->fmt.pdf.fontselected <= 11) { /* Helvetica */
			print->fmt.pdf.posy += (DOUBLE) cnt * ((DOUBLE) print->fontheight + 2.00 + ((DOUBLE) print->fontheight / 30.00));
		}
		else print->fmt.pdf.posy += (DOUBLE) cnt * ((DOUBLE) print->fontheight + 1.00);
#endif
	}
#if OS_WIN32
	else if (print->flags & PRTFLAG_FORMAT_GUI) {
		print->fmt.gui.pos.x = 0;
		print->fmt.gui.pos.y += cnt * print->fmt.gui.fontvsize;
		if (print->fmt.gui.pos.y >= print->fmt.gui.vsize) print->fmt.gui.pos.y = print->fmt.gui.vsize - 1;
	}
#endif
	return 0;
}

INT prtffbin(INT prtnum, CHAR *binname)
{
	PRINTINFO *print;
	INT i1;
#if OS_WIN32
	INT paperbin;
#endif
	CHAR work[32], c1;

	*prterrorstring = '\0';
	print = *printtabptr + --prtnum;
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;

	i1 = prtflushline(prtnum, TRUE);
	if (i1) return i1;
	print = *printtabptr + prtnum;

	if (print->flags & PRTFLAG_FORMAT_PCL) {
		if (binname != NULL && binname[0]) {
			c1 = binname[0];
			if (isdigit((int)c1)) {
				memcpy(work, "\033&l", 3);
				work[3] = c1;
				work[4] = 'H';
				prtbufcopy(prtnum, work, 5);
				print->pagecount++;
				memcpy(work, "\033&a", 3);
				mscitoa((print->fontheight * 30) / 4, work + 3);
				i1 = (INT)strlen(work);
				work[i1] = 'V';
				i1 = prtbufcopy(prtnum, work, i1 + 1);
				if (i1) return i1;
			}
		}
	}
#if OS_WIN32
	else if (print->flags & PRTFLAG_FORMAT_GUI) {
		if ((i1 = prtadrvpage(prtnum))) return i1;
		print->pagecount++;
		paperbin = prtadrvgetpaperbinnumber(print->fmt.gui.device, print->fmt.gui.port, binname);
		if (paperbin != 0 && paperbin != print->fmt.gui.binnumber) {
			if (prtadrvchangepaperbin(prtnum, paperbin)) return PRTERR_NATIVE;
		}
	}
#endif
	else return prtff(prtnum + 1, 1);

	return 0;
}

INT prtff(INT prtnum, INT cnt)
{
	INT i1;
	CHAR work[32];
	PRINTINFO *print;

	*prterrorstring = '\0';
	if (cnt < 1) return 0;

	print = *printtabptr + --prtnum;
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	if ((print->flags & (PRTFLAG_FORMAT_PCL | PRTFLAG_FORMAT_PSL)) && !(print->flags & PRTFLAG_INIT)) {
		if (print->flags & PRTFLAG_FORMAT_PCL) i1 = prtpclinit(prtnum);
		else i1 = prtpslinit(prtnum);
		if (i1) return i1;
	}

	i1 = prtflushline(prtnum, TRUE);
	if (i1) return i1;

	print = *printtabptr + prtnum;
	if (print->flags & (PRTFLAG_FORMAT_DCC | PRTFLAG_FORMAT_PCL)) {
		print->pagecount += cnt;
		while (cnt--) {
			i1 = prtbufcopy(prtnum, "\r\f", 2);
			if (i1) return i1;
		}
		if (print->flags & PRTFLAG_FORMAT_PCL) {
			memcpy(work, "\033&a", 3);
			mscitoa((print->fontheight * 30) / 4, work + 3);
			i1 = (INT)strlen(work);
			work[i1] = 'V';
			i1 = prtbufcopy(prtnum, work, i1 + 1);
			if (i1) return i1;
		}
	}
	else if (print->flags & PRTFLAG_FORMAT_FCC) {
		print->fmt.fcc.prefix = '1';
		while (--cnt) {
			i1 = prtflushline(prtnum, FALSE);
			if (i1) return i1;
			print->fmt.fcc.prefix = '1';
		}
	}
	else if (print->flags & PRTFLAG_FORMAT_PSL) {
		i1 = prtbufcopy(prtnum, "pagelevel restore\n", -1);
		if (!i1) i1 = prtbufcopy(prtnum, "showpage\n", -1);
		if (i1) return i1;
		cnt--;
		while (cnt--) {
			if ( (i1 = prtpslstartpage(prtnum)) ) return i1;
			i1 = prtbufcopy(prtnum, "pagelevel restore\n", -1);
			if (!i1) i1 = prtbufcopy(prtnum, "showpage\n", -1);
			if (i1) return i1;
		}
		if ( (i1 = prtpslstartpage(prtnum)) ) return i1;
	}
	else if (print->flags & PRTFLAG_FORMAT_PDF) {
		while (cnt--) {
			i1 = prtpdfpageend(prtnum);
			if (i1) return i1;
			i1 = prtpdfpagestart(prtnum);
			if (i1) return i1;
			print->pagecount++;
		}
	}
#if OS_WIN32
	else if (print->flags & PRTFLAG_FORMAT_GUI) {
		while (cnt--) {
			i1 = prtadrvpage(prtnum);
			if (i1) return i1;
			print->pagecount++;
		}
	}
#endif
	return 0;
}

static INT prtpslstartpage(INT prtnum) {
	INT i1;
	if ( (i1 = prtpsl_DSC_Page(prtnum)) ) return i1;
	if ( (i1 = prtbufcopy(prtnum, "%%BeginPageSetup\n", -1)) ) return i1;
	if ( (i1 = prtbufcopy(prtnum, "/pagelevel save def\n", -1)) ) return i1;
	if ( (i1 = prtbufcopy(prtnum, "initmatrix\n", -1)) ) return i1;
	if ( (i1 = prtbufcopy(prtnum, "SX SY scale\n", -1)) ) return i1;
	if ( (i1 = prtlinewidth(prtnum + 1, 1)) ) return i1;
	if ( (i1 = prtbufcopy(prtnum, "%%EndPageSetup\n", -1)) ) return i1;
	if ( (i1 = prtbufcopy(prtnum, "ULX ULY moveto\n", -1)) ) return i1;
	return 0;
}

/**
 * Print the PostScript Document Structuring Conventions %%Page comment
 * The page name and ordinal are the same
 */
static INT prtpsl_DSC_Page(INT prtnum) {
	INT i1;
	PRINTINFO *print;
	CHAR work[32];
	print = *printtabptr + prtnum;
	i1 = prtbufcopy(prtnum, "%%Page: ", -1);
	if (i1) return i1;
	i1 = mscitoa(++print->pagecount, work);
	work[i1] = ' ';
	memcpy(work + i1 + 1, work, i1);
	i1 <<= 1;
	work[++i1] = '\n';
	i1 = prtbufcopy(prtnum, work, i1 + 1);
	if (i1) return i1;
	return 0;
}

INT prtcr(INT prtnum, INT cnt)
{
	INT i1;
	PRINTINFO *print;

	*prterrorstring = '\0';
	if (cnt < 1) return 0;

	print = *printtabptr + --prtnum;
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	if ((print->flags & (PRTFLAG_FORMAT_PCL | PRTFLAG_FORMAT_PSL)) && !(print->flags & PRTFLAG_INIT)) {
		if (print->flags & PRTFLAG_FORMAT_PCL) i1 = prtpclinit(prtnum);
		else i1 = prtpslinit(prtnum);
		if (i1) return i1;
	}

	i1 = prtflushline(prtnum, TRUE);
	if (i1) return i1;

	print = *printtabptr + prtnum;
	if (print->flags & (PRTFLAG_FORMAT_DCC | PRTFLAG_FORMAT_PCL)) {
		i1 = prtbufcopy(prtnum, "\r", 1);
		if (i1) return i1;
	}
	else if (print->flags & PRTFLAG_FORMAT_PSL) {
		print->fmt.psl.posx = 0;
		i1 = prtbufcopy(prtnum, "CR\n", 3);
		if (i1) return i1;
	}
	else if (print->flags & PRTFLAG_FORMAT_PDF) {
		print->fmt.pdf.posx = (DOUBLE) 0;
	}
#if OS_WIN32
	else if (print->flags & PRTFLAG_FORMAT_GUI) {
		print->fmt.gui.pos.x = 0;
	}
#endif
	return 0;
}

INT prtlf(INT prtnum, INT cnt)
{
	INT i1, linepos;
	PRINTINFO *print;

	*prterrorstring = '\0';
	if (cnt < 1) return 0;

	print = *printtabptr + --prtnum;
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	if ((print->flags & (PRTFLAG_FORMAT_PCL | PRTFLAG_FORMAT_PSL)) && !(print->flags & PRTFLAG_INIT)) {
		if (print->flags & PRTFLAG_FORMAT_PCL) i1 = prtpclinit(prtnum);
		else i1 = prtpslinit(prtnum);
		if (i1) return i1;
		print = *printtabptr + prtnum;
	}

	linepos = print->linepos;
	i1 = prtflushline(prtnum, FALSE);
	if (i1) return i1;
	print = *printtabptr + prtnum;

	if (print->flags & (PRTFLAG_FORMAT_DCC | PRTFLAG_FORMAT_PCL)) {
		while (cnt--) {
			i1 = prtbufcopy(prtnum, "\n", 1);
			if (i1) return i1;
		}
		if (print->flags & PRTFLAG_FORMAT_DCC) print->linebgn = print->linepos = linepos;
	}
	else if (print->flags & PRTFLAG_FORMAT_FCC) {
		print->fmt.fcc.prefix = ' ';
		while (--cnt) {
			i1 = prtflushline(prtnum, FALSE);
			if (i1) return i1;
			print->fmt.fcc.prefix = ' ';
		}
		print->linepos = linepos;
	}
	else if (print->flags & PRTFLAG_FORMAT_PSL) {
		while (cnt--) {
			i1 = prtbufcopy(prtnum, "LF\n", 3);
			if (i1) return i1;
		}
	}
	else if (print->flags & PRTFLAG_FORMAT_PDF) {
#ifdef __MACOSX
		return prtpdfosxlf(prtnum, cnt);
#else

		if (print->fmt.pdf.fontselected >= 8 && print->fmt.pdf.fontselected <= 11) { /* Helvetica */
			print->fmt.pdf.posy += (DOUBLE) cnt * ((DOUBLE) print->fontheight + 2.00 + ((DOUBLE) print->fontheight / 30.00));
		}
		else print->fmt.pdf.posy += (DOUBLE) cnt * ((DOUBLE) print->fontheight + 1.00);
#endif
	}
#if OS_WIN32
	else if (print->flags & PRTFLAG_FORMAT_GUI) {
		print->fmt.gui.pos.y += cnt * print->fmt.gui.fontvsize;
		if (print->fmt.gui.pos.y >= print->fmt.gui.vsize) print->fmt.gui.pos.y = print->fmt.gui.vsize - 1;
	}
#endif
	return 0;
}

/**
 * Takes a one-based prtnum
 */
INT prttab(INT prtnum, INT pos)
{
	INT i1;
	CHAR work[32];
	PRINTINFO *print;

	*prterrorstring = '\0';
	if (pos < 0) return 0;

	print = *printtabptr + --prtnum;
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	if (print->flags & PRTFLAG_FORMAT_PDF) return(prtpdftab(prtnum, pos, pdfOldTabbing));

	if (print->flags & (PRTFLAG_FORMAT_DCC | PRTFLAG_FORMAT_FCC)) {
		print->linepos = pos;
		return 0;
	}
	if ((print->flags & (PRTFLAG_FORMAT_PCL | PRTFLAG_FORMAT_PSL)) && !(print->flags & PRTFLAG_INIT)) {
		if (print->flags & PRTFLAG_FORMAT_PCL) i1 = prtpclinit(prtnum);
		else i1 = prtpslinit(prtnum);
		if (i1) return i1;
	}

	i1 = prtflushline(prtnum, FALSE);
	if (i1) return i1;

	print = *printtabptr + prtnum;
	if (print->flags & PRTFLAG_FORMAT_PCL) {
		memcpy(work, "\033&a", 3);
		i1 = mscitoa(pos, work + 3) + 3;
		work[i1] = 'C';
		i1 = prtbufcopy(prtnum, work, i1 + 1);
		if (i1) return i1;
	}
	else if (print->flags & PRTFLAG_FORMAT_PSL) {
		i1 = mscitoa(pos, work);
		memcpy(work + i1, " HT\n", 4);
		i1 = prtbufcopy(prtnum, work, i1 + 4);
		if (i1) return i1;
	}
#if OS_WIN32
	else if (print->flags & PRTFLAG_FORMAT_GUI) {
		pos *= print->fmt.gui.fonthsize;
		if (pos < print->fmt.gui.hsize) print->fmt.gui.pos.x = pos;
	}
#endif
	return 0;
}

#if 0
#if DX_MAJOR_VERSION >= 18
INT prthorz(INT prtnum, INT xpos)
{
	PRINTINFO *print;
	print = *printtabptr + --prtnum;
	*prterrorstring = '\0';
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	if (print->flags & (PRTFLAG_FORMAT_DCC | PRTFLAG_FORMAT_FCC)) return 0;
	if (print->flags & (PRTFLAG_FORMAT_PDF)) return prtpdfposx(prtnum, xpos);
#if OS_WIN32
	if (print->flags & PRTFLAG_FORMAT_GUI) {
		xpos = (xpos * print->fmt.gui.scalex) / 3600;
		if (xpos < print->fmt.gui.hsize) print->fmt.gui.pos.x = xpos;
	}
#endif
	return 0;
}

INT prtvert(INT prtnum, INT ypos)
{
	PRINTINFO *print;
	print = *printtabptr + --prtnum;
	*prterrorstring = '\0';
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	if (print->flags & (PRTFLAG_FORMAT_DCC | PRTFLAG_FORMAT_FCC)) return 0;
	if (print->flags & (PRTFLAG_FORMAT_PDF)) return prtpdfposy(prtnum, ypos);
#if OS_WIN32
	if (print->flags & PRTFLAG_FORMAT_GUI) {
		ypos = (ypos * print->fmt.gui.scaley) / 3600;
		if (ypos < print->fmt.gui.vsize) print->fmt.gui.pos.y = ypos;
	}
#endif
	return 0;
}

INT prthorzadj(INT prtnum, INT xadj)
{
	PRINTINFO *print;
	print = *printtabptr + --prtnum;
	*prterrorstring = '\0';
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	if (print->flags & (PRTFLAG_FORMAT_DCC | PRTFLAG_FORMAT_FCC)) return 0;
	if (print->flags & (PRTFLAG_FORMAT_PDF)) return prtpdfposxadj(prtnum, xadj);
	return 0;
}

INT prtvertadj(INT prtnum, INT yadj)
{
	PRINTINFO *print;
	print = *printtabptr + --prtnum;
	*prterrorstring = '\0';
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	if (print->flags & (PRTFLAG_FORMAT_DCC | PRTFLAG_FORMAT_FCC)) return 0;
	if (print->flags & (PRTFLAG_FORMAT_PDF)) return prtpdfposyadj(prtnum, yadj);
	return 0;
}

#endif
#endif

/**
 * prtnum The One-Based index into printtabptr
 */
INT prtpos(INT prtnum, INT xpos, INT ypos)
{
	INT i1;
	CHAR work[64], *ptr;
	PRINTINFO *print;

	print = *printtabptr + --prtnum;

	*prterrorstring = '\0';
#if 0
	if (xpos < 0 || ypos < 0) return 0;
#endif

	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	if (print->flags & (PRTFLAG_FORMAT_DCC | PRTFLAG_FORMAT_FCC)) return 0;
	if (print->flags & (PRTFLAG_FORMAT_PDF)) return prtpdfpos(prtnum, xpos, ypos);

	if ((print->flags & (PRTFLAG_FORMAT_PCL | PRTFLAG_FORMAT_PSL)) && !(print->flags & PRTFLAG_INIT)) {
		if (print->flags & PRTFLAG_FORMAT_PCL) i1 = prtpclinit(prtnum);
		else i1 = prtpslinit(prtnum);
		if (i1) return i1;
	}

	i1 = prtflushline(prtnum, FALSE);
	if (i1) return i1;

	print = *printtabptr + prtnum;

	if (print->flags & PRTFLAG_FORMAT_PCL) {
		if (!prpget("print", "pcl", "uom", NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old"))
			i1 = prtpclposold(print, prtnum, xpos, ypos);
		else
			i1 = prtpclposnew(prtnum, xpos, ypos);
		if (i1) return i1;
	}
	else if (print->flags & PRTFLAG_FORMAT_PSL) {
		print->fmt.psl.posx = xpos;
		print->fmt.psl.posy = ypos;
		i1 = mscitoa(xpos, work);
		memcpy(work + i1, " ULX add ", 9);
		i1 += 9;
		i1 += mscitoa(ypos, work + i1);
		memcpy(work + i1, " ULY exch sub moveto\n", 21);
		i1 = prtbufcopy(prtnum, work, i1 + 21);
		if (i1) return i1;
	}
#if OS_WIN32
	else if (print->flags & PRTFLAG_FORMAT_GUI) {
		xpos = (xpos * print->fmt.gui.scalex) / 3600;
		ypos = (ypos * print->fmt.gui.scaley) / 3600;
		if (xpos < print->fmt.gui.hsize) print->fmt.gui.pos.x = xpos;
		if (ypos < print->fmt.gui.vsize) print->fmt.gui.pos.y = ypos;
	}
#endif
	return 0;
}

/**
 * Use the old method, position is by 1/72s of an inch
 */
static INT prtpclposold(PRINTINFO *print, INT prtnum, INT xpos, INT ypos) {
	CHAR work[32];
	INT i1;
	memcpy(work, "\033&a", 3);
	i1 = mscitoa(xpos * 10, work + 3) + 3;
	work[i1++] = 'h';
	i1 += mscitoa(ypos * 10 + (print->fontheight * 30) / 4, work + i1);
	work[i1] = 'V';
	return prtbufcopy(prtnum, work, i1 + 1);
}

/**
 * Use the new method, position is by PCL Units
 */
static INT prtpclposnew(INT prtnum, INT xpos, INT ypos) {
	CHAR work[32];
	sprintf(work, "\033*p%dx%dY", xpos, ypos);
	return prtbufcopy(prtnum, work, -1);
}

INT prtline(INT prtnum, INT xpos, INT ypos)
{
	INT i1;
	CHAR work[64], *ptr;
	PRINTINFO *print;

	*prterrorstring = '\0';
	if (xpos < 0 || ypos < 0) return 0;

	print = *printtabptr + --prtnum;
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	if (print->flags & (PRTFLAG_FORMAT_DCC | PRTFLAG_FORMAT_FCC)) return 0;
	if ((print->flags & (PRTFLAG_FORMAT_PCL | PRTFLAG_FORMAT_PSL)) && !(print->flags & PRTFLAG_INIT)) {
		if (print->flags & PRTFLAG_FORMAT_PCL) i1 = prtpclinit(prtnum);
		else i1 = prtpslinit(prtnum);
		if (i1) return i1;
	}

	i1 = prtflushline(prtnum, FALSE);
	if (i1) return i1;

	print = *printtabptr + prtnum;
	if (print->flags & PRTFLAG_FORMAT_PCL) {
		if (!prpget("print", "pcl", "uom", NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old"))
			i1 = prtpcllineold(prtnum, xpos, ypos);
		else
			i1 = prtpcllinenew(prtnum, xpos, ypos);
		if (i1) return i1;
	}
	else if (print->flags & PRTFLAG_FORMAT_PSL) {
		i1 = mscitoa(xpos, work);
		memcpy(work + i1, " ULX add ", 9);
		i1 += 9;
		i1 += mscitoa(ypos, work + i1);
		memcpy(work + i1, " ULY exch sub LN\n", 17);
		i1 = prtbufcopy(prtnum, work, i1 + 17);
		if (i1) return i1;
		print->fmt.psl.posx = xpos;
		print->fmt.psl.posy = ypos;
	}
	else if (print->flags & PRTFLAG_FORMAT_PDF) {
		return prtpdfline(prtnum, xpos, ypos);
	}
#if OS_WIN32
	else if (print->flags & PRTFLAG_FORMAT_GUI) {
		i1 = prtadrvline(prtnum, xpos, ypos);
		if (i1) return i1;
	}
#endif
	return 0;
}


/**
 * HPGL Plotter Units are .025mm
 * So there are 1016 of them in an inch
 */
static INT prtpcllinenew(INT prtnum, INT xpos, INT ypos) {
	CHAR work[1024];
	PRINTINFO *print;
	DOUBLE d1;

	print = *printtabptr + prtnum;
	if (print->dpi) {		/* defensive coding, print->dpi should never be zero */
		d1 = 1016.0 / (double) print->dpi;
		sprintf(work, "\033%%1BSC0,%f,0,%f,2;SP1;PD%d,%d;PU;\033%%1A", d1, d1, ypos, xpos);
	}
	else sprintf(work, "\033%%1BSP1;PD%d,%d;PU;\033%%1A", ypos, xpos);
	return prtbufcopy(prtnum, work, -1);
}

static INT prtpcllineold(INT prtnum, INT xpos, INT ypos) {
	INT i1;
	CHAR work[64];
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	i1 = ((print->fontheight * 30) / 4);
	ypos = (ypos * 1411) / 100;
	xpos = (xpos * 1411) / 100;
	sprintf(work, "\033&a-%dV\033%%1BSP1;PD%d,%d;\033%%1A\033&a+%dV", i1, ypos, xpos, i1);
	i1 = prtbufcopy(prtnum, work, -1);
	return i1;
}


INT prtlinewidth(INT prtnum, INT width)
{
	INT i1;
	CHAR work[64];
	PRINTINFO *print;

	*prterrorstring = '\0';
	print = *printtabptr + --prtnum;
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	if (print->flags & (PRTFLAG_FORMAT_DCC | PRTFLAG_FORMAT_FCC)) return 0;
	if ((print->flags & (PRTFLAG_FORMAT_PCL | PRTFLAG_FORMAT_PSL)) && !(print->flags & PRTFLAG_INIT)) {
		if (print->flags & PRTFLAG_FORMAT_PCL) i1 = prtpclinit(prtnum);
		else i1 = prtpslinit(prtnum);
		if (i1) return i1;
	}

	i1 = prtflushline(prtnum, FALSE);
	if (i1) return i1;

	if (print->flags & PRTFLAG_FORMAT_PCL) {
		memcpy(work, "\033%1BPW", 6);
		i1 = mscitoa((width * 35) / 100, work + 6) + 6;
		work[i1++] = '.';
		i1 += mscitoa(((width * 3500) / 100) % 100, work + i1);
		memcpy(work + i1, ";\033%1A", 5);
		i1 = prtbufcopy(prtnum, work, i1 + 5);
		if (i1) return i1;
	}
	else if (print->flags & PRTFLAG_FORMAT_PSL) {
		i1 = mscitoa(width, work);
		memcpy(work + i1, " setlinewidth\n", 14);
		i1 = prtbufcopy(prtnum, work, i1 + 14);
		if (i1) return i1;
	}
	else if (print->flags & PRTFLAG_FORMAT_PDF) {
		i1 = prtpdflinewidth(prtnum, width);
		if (i1) return i1;
	}
	print->linewidth = width;
	return 0;
}

/**
 * prtnum must be ONE-based
 * font is in the DX normal font specification format
 *
 * This function will always be called at least once by DX,
 * for PS, PCL, or PDF print output, with the
 * default value. ("Courier(12, plain)")
 */
INT prtfont(INT prtnum, CHAR *font)
{
	static INT bold = FALSE, italic = FALSE, ul = FALSE;
	static CHAR lastSeenFontName[100];
	INT i1, i2, i3, fontchangeflag, pointchangeflag, pointsize;
	CHAR name[100], work[128], work20[20];
	PRINTINFO *print;

	*prterrorstring = '\0';
	print = *printtabptr + --prtnum;

	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	if (print->flags & (PRTFLAG_FORMAT_DCC | PRTFLAG_FORMAT_FCC)) return 0;
	if ((print->flags & (PRTFLAG_FORMAT_PCL | PRTFLAG_FORMAT_PSL)) && !(print->flags & PRTFLAG_INIT)) {
		if (print->flags & PRTFLAG_FORMAT_PCL) i1 = prtpclinit(prtnum);
		else i1 = prtpslinit(prtnum);
		if (i1) return i1;
	}

	i1 = prtflushline(prtnum, FALSE);
	if (i1) return i1;

	print = *printtabptr + prtnum;
	if (print->flags & (PRTFLAG_FORMAT_PCL | PRTFLAG_FORMAT_PSL | PRTFLAG_FORMAT_PDF)) {
		fontchangeflag = pointchangeflag = FALSE;
		pointsize = print->fontheight;
		/*
		 * Squeeze out blanks and set all alpha to caps,
		 * except on Mac-PDF keep the blanks!
		 */
#ifdef __MACOSX
		if (print->flags & PRTFLAG_FORMAT_PDF) {
			for (i1 = i2 = 0; i1 < (INT)strlen(font); i1++) {
				name[i2++] = toupper(font[i1]);
			}
		}
		else {
			for (i1 = i2 = 0; i1 < (INT)strlen(font); i1++) {
				if (font[i1] != ' ') name[i2++] = toupper(font[i1]);
			}
		}
#else
		for (i1 = i2 = 0; i1 < (INT)strlen(font); i1++) {
			if (font[i1] != ' ') name[i2++] = toupper(font[i1]);
		}
#endif
		if (!i2) return 0;
		name[i2] = '\0';
		for (i1 = 0; name[i1] && name[i1] != '('; i1++);
		if (name[i1] == '(') {
			if (i1) fontchangeflag = TRUE;
			name[i1++] = '\0';
			while (name[i1] && name[i1] != ')') {
				while (name[i1] == ',') i1++;
				for (i2 = 0; i2 < 20 && name[i1] && name[i1] != ',' && name[i1] != ')'; ) work20[i2++] = name[i1++];
				work20[i2] = '\0';
				if (isdigit((int)work20[0])) {
					if (i2 > 1) pointsize = (work20[0] - '0') * 10 + work20[1] - '0';
					else pointsize = work20[0] - '0';
					pointchangeflag = TRUE;
				}
				else if (!strcmp(work20, "PLAIN")) bold = italic = ul = FALSE;
				else if (!strcmp(work20, "BOLD")) bold = TRUE;
				else if (!strcmp(work20, "NOBOLD")) bold = FALSE;
				else if (!strcmp(work20, "ITALIC")) italic = TRUE;
				else if (!strcmp(work20, "NOITALIC")) italic = FALSE;
				else if (!strcmp(work20, "UNDERLINE")) ul = TRUE;
				else if (!strcmp(work20, "NOUNDERLINE")) ul = FALSE;
			}
		}
		else fontchangeflag = TRUE;

		/*
		 * The below will happen if the font spec contains only size and/or style.
		 * That is, it looks something like this, "(14,italic)"
		 */
		if (!name[0]) strcpy(name, lastSeenFontName);

		if (print->flags & PRTFLAG_FORMAT_PCL) {
			i2 = ((print->fontheight * 30) / 4);
			if (i2) {
				memcpy(work, "\033&a-", 4);
				i1 = mscitoa(i2, work + 4) + 4;
				work[i1++] = 'V';
			}
			else i1 = 0;
			if (fontchangeflag) {
				if (!strcmp(name, "COURIER")) i2 = 0, i3 = 3;
				else if (!strcmp(name, "HELVETICA")) i2 = 1, i3 = 4148;
				else if (!strcmp(name, "SYSTEM")) i2 = 1, i3 = 4148;
				else if (!strcmp(name, "TIMES")) i2 = 1, i3 = 4101;
				memcpy(work + i1, "\033(s", 3);
				i1 += 3;
				i1 += mscitoa(i2, work + i1);
				memcpy(work + i1, "P\033(s", 4);
				i1 += 4;
				i1 += mscitoa(i3, work + i1);
				work[i1++] = 'T';
			}
			if (pointchangeflag) {
				memcpy(work + i1, "\033(s", 3);
				i1 += 3;
				i1 += mscitoa(pointsize, work + i1);
				memcpy(work + i1, "V\033(s", 4);
				i1 += 4;
				i2 = 480 / pointsize;
				if (((480 % pointsize) << 1) >= pointsize) i2++;
				i1 += mscitoa(i2 >> 2, work + i1);
				if (i2 & 0x03) {
					work[i1++] = '.';
					i1 += mscitoa((i2 & 3) * 25, work + i1);
				}
				memcpy(work + i1, "H\033&l", 4);
				i1 += 4;
				i2 = (pointsize * 48) / 72;
				i1 += mscitoa(i2, work + i1);
				i2 = (pointsize * 4800) / 72 - i2 * 100;
				if (i2) {
					work[i1++] = '.';
					i1 += mscitoa(i2 % 100, work + i1);
				}
				work[i1++] = 'C';
			}
			memcpy(work + i1, "\033(s", 3);
			i1 += 3;
			i1 += mscitoa(bold * 3, work + i1);
			work[i1++] = 'B';
			memcpy(work + i1, "\033(s", 3);
			i1 += 3;
			i1 += mscitoa(italic, work + i1);
			work[i1++] = 'S';
			memcpy(work + i1, "\033&d", 3);
			i1 += 3;
			if (ul) {
				work[i1++] = '0';
				work[i1++] = 'D';
			}
			else work[i1++] = '@';
			memcpy(work + i1, "\033&a+", 4);
			i1 += 4;
			i1 += mscitoa((pointsize * 30) / 4, work + i1);
			work[i1] = 'V';
			i1 = prtbufcopy(prtnum, work, i1 + 1);
			if (i1) return i1;
		}
		else if (print->flags & PRTFLAG_FORMAT_PSL) {
			i2 = 0;  /* default to courier, plain */
			if (italic) i2 += 0x01;
			if (bold) i2 += 0x02;
			if (!strcmp(name, "TIMES")) i2 += 0x04;
			else if (!strcmp(name, "HELVETICA")) i2 += 0x08;
			work[0] = '/';
			strcpy(work + 1, fontnames[i2]);
			i1 = (INT)strlen(work);
			memcpy(work + i1, " findfont [", 11);
			i1 += 11;
			i1 += mscitoa(pointsize, work + i1);
			memcpy(work + i1, " SX div 0 0 ", 12);
			i1 += 12;
			i1 += mscitoa(pointsize, work + i1);
			memcpy(work + i1, " SY div 0 ", 10);
			i1 += 10;
			i1 += mscitoa((pointsize * 3) / 4, work + i1);
			memcpy(work + i1, " SY div neg] selectfont\n/FH ", 28);
			i1 += 28;
			i1 += mscitoa(pointsize, work + i1);
			memcpy(work + i1, " SY div def\n", 12);
			i1 += 12;
			i1 = prtbufcopy(prtnum, work, i1);
			if (i1) return i1;
		}
		else if (print->flags & PRTFLAG_FORMAT_PDF) {
			i1 = prtpdffont(prtnum, name, bold, italic, pointsize);
			if (i1) return i1;
		}
		strcpy(lastSeenFontName, name);
		print->fontheight = pointsize;
	}
#if OS_WIN32
	else if (print->flags & PRTFLAG_FORMAT_GUI) {
		i1 = prtadrvfont(prtnum, font);
		if (i1) return i1;
	}
#endif
	return 0;
}

/**
 * Color is assumed to be RGB
 */
INT prtcolor(INT prtnum, INT color)
{
	INT i1, i2, i3;
	CHAR work[1024];
	PRINTINFO *print;

	*prterrorstring = '\0';
	print = *printtabptr + --prtnum;
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	if (print->flags & (PRTFLAG_FORMAT_DCC | PRTFLAG_FORMAT_FCC)) return 0;
	if ((print->flags & (PRTFLAG_FORMAT_PCL | PRTFLAG_FORMAT_PSL)) && !(print->flags & PRTFLAG_INIT)) {
		if (print->flags & PRTFLAG_FORMAT_PCL) i1 = prtpclinit(prtnum);
		else i1 = prtpslinit(prtnum);
		if (i1) return i1;
	}

	i1 = prtflushline(prtnum, FALSE);
	if (i1) return i1;

	print = *printtabptr + prtnum;
	if (print->flags & PRTFLAG_FORMAT_PDF) return prtpdfcolor(prtnum, color);
	else if (print->flags & PRTFLAG_FORMAT_PCL) {
		i1 = color >> 16;
		i2 = (color & 0x00FF00) >> 8;
		i3 = color & 0x0000FF;
		/**
		 * Esc*v1S is Foreground Color Command
		 * found in the PCL5 Color Tech Ref Man, Page C-18
		 */
		sprintf(work, "\033%%0BPC1,%d,%d,%d;\033%%0A\033*v1S", i1, i2, i3);
		i1 = prtbufcopy(prtnum, work, -1);
		if (i1) return i1;
	}
	else if (print->flags & PRTFLAG_FORMAT_PSL) {
		i2 = 0;
		i1 = color >> 16;
		if (i1 == 0xFF) work[i2++] = '1';
		else i2 += prtdbltoa((DOUBLE) i1 / 255.00, &work[i2], 2);
		work[i2++] = ' ';
		i1 = (color & 0x00FF00) >> 8;
		if (i1 == 0xFF) work[i2++] = '1';
		else i2 += prtdbltoa((DOUBLE) i1 / 255.00, &work[i2], 2);
		work[i2++] = ' ';
		i1 = color & 0x0000FF;
		if (i1 == 0xFF) work[i2++] = '1';
		else i2 += prtdbltoa((DOUBLE) i1 / 255.00, &work[i2], 2);
		work[i2++] = ' ';
		work[i2++] = '\0';
		strcat(work, "setrgbcolor\n");
		i1 = prtbufcopy(prtnum, work, -1);
		if (i1) return i1;
	}
#if OS_WIN32
	else if (print->flags & PRTFLAG_FORMAT_GUI) {
		/* fmt.gui.color is stored as a Win32 COLORREF */
		print->fmt.gui.color = (color >> 16) | (color & 0x00FF00) | ((color & 0x0000FF) << 16);
	}
#endif
	return 0;
}

INT prtimage(INT prtnum, void *pixhandle)
{
	INT i1, i2, i3, i4, hsize, vsize, rowbytecount, revbits;
	INT32 color;
	CHAR work[64];
	UCHAR *ptr, **prtimg;
	CHAR *cfgopt;
	PRINTINFO *print;
#if OS_WIN32GUI
	BITMAP bm;
	UCHAR *ptrend;
#endif

	*prterrorstring = '\0';
	print = *printtabptr + --prtnum;
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	if (print->flags & (PRTFLAG_FORMAT_DCC | PRTFLAG_FORMAT_FCC)) return 0;
	if ((print->flags & (PRTFLAG_FORMAT_PCL | PRTFLAG_FORMAT_PSL)) && !(print->flags & PRTFLAG_INIT)) {
		if (print->flags & PRTFLAG_FORMAT_PCL) i1 = prtpclinit(prtnum);
		else i1 = prtpslinit(prtnum);
		if (i1) return i1;
	}

	i1 = prtflushline(prtnum, FALSE);
	if (i1) return i1;
	print = *printtabptr + prtnum;
	if (print->flags & PRTFLAG_FORMAT_PCL) {
		hsize = (*(PIXHANDLE) pixhandle)->hsize;
		vsize = (*(PIXHANDLE) pixhandle)->vsize;
		if ((*((PIXHANDLE) pixhandle))->bpp == 1) {

			/* When this was first coded, a mistake was made. We assumed that
			 * the programmer expected to position the cursor at the lower left corner
			 * of the image. It should be the upper left.
			 * The old code can be activated by using
			 * dbcdx.print.pcl.imageposition=old
			 */
			if (!prpget("print", "pcl", "imageposition", NULL, &cfgopt, PRP_LOWER) && !strcmp(cfgopt, "old")) {
				if (!i1) i1 = prtbufcopy(prtnum, "\033*p-", -1);
				if (!i1) mscitoa(vsize, work);
				if (!i1) i1 = prtbufcopy(prtnum, work, -1);
				if (!i1) i1 = prtbufcopy(prtnum, "Y", -1);
			}

			/*
			 * Set raster graphics presentation mode
			 */
			if (!i1) i1 = prtbufcopy(prtnum, "\033*r0F", -1);

			/*
			 * Set raster width and height.
			 * If we don't do this, any partial bytes at the end of a row might
			 * print random black dots on the page.
			 */
			if (!i1) {
				sprintf(work, "\033*r%it%iS", vsize, hsize);
				i1 = prtbufcopy(prtnum, work, -1);
			}

			/* start raster graphics */
			if (!i1) i1 = prtbufcopy(prtnum, "\033*r1A", -1);
			if (i1) return i1;
			prtimg = (UCHAR **) memalloc(hsize / 8 + ((hsize % 8) ? 1 : 0), 0);
			if (prtimg == NULL) return PRTERR_NOMEM;
			ptr = *prtimg;

			if (!prpget("print", "pcl", "bwimage", NULL, &cfgopt, PRP_LOWER) && !strcmp(cfgopt, "reverse")) revbits = 1;
			else revbits = 0;

#if OS_WIN32GUI
			GetObject((*((PIXHANDLE)pixhandle))->hbitmap, sizeof(BITMAP), &bm);
			bm.bmWidthBytes = ((hsize * bm.bmBitsPixel + 0x1F) & ~0x1F) >> 3;
			/* ptrend points at the first byte of the highest row in memory,
			 * which in Windows is scan line 0
			 */
			ptrend = (UCHAR *) bm.bmBits + bm.bmWidthBytes * (bm.bmHeight - 1);
#endif

			/* PCL raster graphics use a zero for white */
			for (i1 = i4 = 0; i1 < vsize; i1++) {
				for (i2 = 0, i3 = 7; i2 < hsize; i2++) {
					/* color = 0xBBGGRR */
#if OS_WIN32GUI
					/*
					 * The prtgetcolor routine will return 0x000000 if the pixel is one (white).
					 * and 0xffffff if the pixel is zero (black).
					 * This is the wrong sense for PCL printing so the normal mode is for us to reverse it.
					 */
					color = prtgetcolor(bm, ptrend, i2, i1);
					if (!revbits) color = ~color;
#else
					/*
					 * In our internal 1bpp map, zero is black, one is white.
					 * This is the correct sense for PCL so we do not normally reverse it.
					 */
					guipixgetcolor((PIXHANDLE) pixhandle, &color, i2, i1);
					if (revbits) color = ~color;
#endif
					if (i3 == 7) *ptr = 0;
					if (color == 0) *ptr |= 1 << i3;
					if (--i3 < 0) {
						i3 = 7;
						ptr++;
					}
				}
				/* print 1 row */
				rowbytecount = hsize / 8 + ((hsize % 8) ? 1 : 0);
				if (!i4) sprintf(work, "\033*b%dW", rowbytecount);
				if (!i4) i4 = prtbufcopy(prtnum, work, -1);
				if (!i4) i4 = prtbufcopy(prtnum, (CHAR *) *prtimg, rowbytecount);
				ptr = *prtimg;
			}
			/* end raster graphics */
			if (!i4) i4 = prtbufcopy(prtnum, "\033*rC", -1);
			if (i4) return i4;
		}
		else {
/*** CURRENTLY DO NOT SUPPORT > 1 COLORBIT IMAGES FOR PCL ***/
		}
	}
	else if (print->flags & PRTFLAG_FORMAT_PSL) {
		i1 = prtpslimage(prtnum, pixhandle);
		if (i1) return i1;
	}
	else if (print->flags & PRTFLAG_FORMAT_PDF) {
		i1 = prtpdfimage(prtnum, pixhandle);
		if (i1) return i1;
	}
#if OS_WIN32
	else if (print->flags & PRTFLAG_FORMAT_GUI) {
		i1 = prtadrvpixmap(prtnum, (PIXHANDLE) pixhandle);
		if (i1) return i1;
	}
#endif
	return 0;
}


static INT prtpslimage(INT prtnum, void *pixhandle) {
	INT i1, i2, hsize, vsize;
	UINT bcount;
	UCHAR bpp, *ptr, **prtimg, *scanline, *ptrend;
	PRINTINFO *print;
	CHAR work[64];
	INT32 color;
	PIXMAP *pix;
#if OS_WIN32GUI
	BITMAP bm;
#else
	UINT scanlinewidth;
#endif

	pix = *((PIXHANDLE)pixhandle);
	hsize = pix->hsize;
	vsize = pix->vsize;
	bpp = pix->bpp;
	if (bpp == 1) bcount = vsize * ((hsize + 0x07 & ~0x07) >> 3);
	else bcount = vsize * hsize * 3;
	prtimg = (UCHAR **) memalloc(bcount, 0);
	if (prtimg == NULL) return PRTERR_NOMEM;
	print = *printtabptr + prtnum;		/* memalloc might have moved printtabptr */
	i1 = prtbufcopy(prtnum, "gsave\n", -1);
	if (!i1) mscitoa(print->fmt.psl.posx, work);
	if (!i1) i1 = prtbufcopy(prtnum, work, -1);
	if (!i1) i1 = prtbufcopy(prtnum, " ", -1);
	if (!i1) mscitoa((print->fmt.psl.height - vsize) - print->fmt.psl.posy, work);
	if (!i1) i1 = prtbufcopy(prtnum, work, -1);
	if (!i1) i1 = prtbufcopy(prtnum, " translate\n1 dict begin\n", -1);
	if (!i1) mscitoa(hsize, work);
	if (!i1) i1 = prtbufcopy(prtnum, work, -1);
	if (!i1) i1 = prtbufcopy(prtnum, " ", 1);
	if (!i1) mscitoa(vsize, work);
	if (!i1) i1 = prtbufcopy(prtnum, work, -1);
	if (!i1) i1 = prtbufcopy(prtnum, " scale\n", -1);
	if (!i1) {
		if (bpp == 1) i1 = prtbufcopy(prtnum, "/DeviceGray", -1);
		else i1 = prtbufcopy(prtnum, "/DeviceRGB", -1);
	}
	if (!i1) i1 = prtbufcopy(prtnum, " setcolorspace\n{\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "<<\n/ImageType 1\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "/Width ", -1);
	if (!i1) mscitoa(hsize, work);
	if (!i1) i1 = prtbufcopy(prtnum, work, -1);
	if (!i1) i1 = prtbufcopy(prtnum," /Height ", -1);
	if (!i1) mscitoa(vsize, work);
	if (!i1) i1 = prtbufcopy(prtnum, work, -1);
	if (!i1) i1 = prtbufcopy(prtnum, "\n/ImageMatrix [", -1);
	if (!i1) mscitoa(hsize, work);
	if (!i1) i1 = prtbufcopy(prtnum, work, -1);
	if (!i1) i1 = prtbufcopy(prtnum, " 0 0 -", -1);
	if (!i1) mscitoa(vsize, work);
	if (!i1) i1 = prtbufcopy(prtnum, work, -1);
	if (!i1) i1 = prtbufcopy(prtnum, " 0 ", -1);
	if (!i1) mscitoa(vsize, work);
	if (!i1) i1 = prtbufcopy(prtnum, work, -1);
	if (!i1) i1 = prtbufcopy(prtnum, "]\n/BitsPerComponent ", -1);
	if (!i1) {
		if (bpp == 1) i1 = prtbufcopy(prtnum, "1\n", 2);
		else i1 = prtbufcopy(prtnum, "8\n", 2);
	}
	if (!i1) {
		if (bpp == 1) i1 = prtbufcopy(prtnum, "/Decode [0 1]\n", -1);
		else i1 = prtbufcopy(prtnum, "/Decode [0 1 0 1 0 1]\n", -1);
	}
	if (!i1) i1 = prtbufcopy(prtnum, "/DataSource currentfile\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, ">> image\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "}\n%%BeginData: ", -1);
	if (!i1) mscitoa(bcount + 5, work);
	if (!i1) i1 = prtbufcopy(prtnum, work, -1);
	if (!i1) i1 = prtbufcopy(prtnum, " Binary Bytes\nexec\n", -1);
	if (i1) return i1;
	ptr = *prtimg;
	pix = *((PIXHANDLE)pixhandle);
#if OS_WIN32GUI
	GetObject(pix->hbitmap, sizeof(BITMAP), &bm);
	bm.bmWidthBytes = ((hsize * bm.bmBitsPixel + 0x1F) & ~0x1F) >> 3;
	ptrend = (UCHAR *) bm.bmBits + bm.bmWidthBytes * (bm.bmHeight - 1);
#else
	scanlinewidth = (hsize * bpp + 0x1F & ~0x1F) >> 3;
	ptrend = (UCHAR *) *pix->pixbuf + scanlinewidth * (vsize - 1);
#endif
	if (bpp == 1) {
		i2 = (hsize + 0x07 & ~0x07) >> 3;
#if OS_WIN32GUI
		for (scanline = ptrend; scanline >= (UCHAR *) bm.bmBits; scanline -= bm.bmWidthBytes) {
			for (i1 = 0; i1 < i2; i1++) *ptr++ = *(scanline + i1);
		}
#else
		for (scanline = *pix->pixbuf; scanline <= ptrend; scanline += scanlinewidth) {
			memcpy(ptr, scanline, i2);
			ptr += i2;
		}
#endif
	}
	else {
		for (i1 = 0; i1 < vsize; i1++) {
			for (i2 = 0; i2 < hsize; i2++) {
				/* color = 0xBBGGRR */
#if OS_WIN32GUI
				color = prtgetcolor(bm, ptrend, i2, i1);
#else
				guipixgetcolor((PIXHANDLE) pixhandle, &color, i2, i1);
#endif
				*ptr++ = (UCHAR) color;
				*ptr++ = (UCHAR) (color >> 8);
				*ptr++ = (UCHAR) (color >> 16);
			}
		}
	}
	i1 = prtbufcopy(prtnum, (CHAR *) *prtimg, bcount);
	memfree(prtimg);
	if (!i1) i1 = prtbufcopy(prtnum, "\n%%EndData\nend\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "grestore\n", -1);
	return i1;
}


#if OS_WIN32 && OS_WIN32GUI
/*
 * This is a much faster replacement for the Win32 API GetPixel() call - SSN
 *
 * ptrend points at the first byte of scan line 0, which is the highest in memory.
 */
INT32 prtgetcolor(BITMAP bm, UCHAR *ptrend, INT h1, INT v1)
{
	static RGBQUAD quadclrs[256];
	static INT initquad = FALSE;
	INT iCurClr, iCurRed, iCurGreen, iCurBlue;
	INT32 color;
	UCHAR *ptr1, ucGrayShade;

	/* bitmap scanlines are stored in reverse order, so start from the end */
	ptr1 = ptrend - bm.bmWidthBytes * v1;
	if (bm.bmBitsPixel == 1) {
		ptr1 = ptr1 + (h1 >> 3); /* h1 divide 8 */
		if ((0x80 >> (h1 % 8)) & *ptr1) color = 0x000000;
		else color = 0xFFFFFF;
	}
	else if (bm.bmBitsPixel == 4) {
		ptr1 = ptr1 + (h1 >> 1); /* h1 divide 2 */
		switch ((h1 & 0x1) ? (*ptr1 & 0x0F) : (*ptr1 & 0xF0) >> 4) {
			case 0x0: color = 0x000000; break; /* dark black */
			case 0x1: color = 0x800000; break; /* dark blue */
			case 0x2: color = 0x008000; break; /* dark green */
			case 0x3: color = 0x808000; break; /* dark cyan */
			case 0x4: color = 0x000080; break; /* dark red */
			case 0x5: color = 0x800080; break; /* dark magenta */
			case 0x6: color = 0x008080; break; /* dark yellow */
			case 0x7: color = 0xC0C0C0; break; /* light gray */
			case 0x8: color = 0x808080; break; /* medium gray */
			case 0x9: color = 0xFF0000; break; /* blue */
			case 0xA: color = 0x00FF00; break; /* green */
			case 0xB: color = 0xFFFF00; break; /* cyan */
			case 0xC: color = 0x0000FF; break; /* red */
			case 0xD: color = 0xFF00FF; break; /* magenta */
			case 0xE: color = 0x00FFFF; break; /* yellow */
			case 0xF: color = 0xFFFFFF; break; /* white */
		}
	}
	else if (bm.bmBitsPixel == 8) {
		if (!initquad) {
		/* fill in colors used for 16 and 256 color palette modes */
			for (iCurClr = 0; iCurClr < 16; iCurClr++) {
				quadclrs[iCurClr].rgbBlue = DBC_BLUE(dwDefaultPal[iCurClr]);
				quadclrs[iCurClr].rgbGreen = DBC_GREEN(dwDefaultPal[iCurClr]);
				quadclrs[iCurClr].rgbRed = DBC_RED(dwDefaultPal[iCurClr]);
			}

		/* fill in colors used only in 256 color palette modes */
			for (iCurRed = 0; iCurRed < 256; iCurRed += 51) {
				for (iCurGreen = 0; iCurGreen < 256; iCurGreen += 51) {
					for (iCurBlue = 0; iCurBlue < 256; iCurBlue += 51) {
						quadclrs[iCurClr].rgbBlue = (UCHAR) iCurBlue;
						quadclrs[iCurClr].rgbGreen = (UCHAR) iCurGreen;
						quadclrs[iCurClr].rgbRed = (UCHAR) iCurRed;
						iCurClr++;
					}
				}
			}
		/* finish off with a spectrum of gray shades */
			ucGrayShade = 8;
			for (; iCurClr < 256; iCurClr++) {
				quadclrs[iCurClr].rgbBlue = ucGrayShade;
				quadclrs[iCurClr].rgbGreen = ucGrayShade;
				quadclrs[iCurClr].rgbRed = ucGrayShade;
				ucGrayShade += 8;
			}
			initquad = TRUE;
		}
		ptr1 += h1;
		color = (quadclrs[*ptr1].rgbBlue << 16) | (quadclrs[*ptr1].rgbGreen << 8) | quadclrs[*ptr1].rgbRed;
	}
	else if (bm.bmBitsPixel == 24) {
		ptr1 = ptr1 + (h1 * 3);
		color = *ptr1 << 16;
		ptr1++;
		color |= *ptr1 << 8;
		ptr1++;
		color |= *ptr1;
	}
	return color;
}
#endif

INT prtflush(INT prtnum)
{
	INT i1, linepos;
	PRINTINFO *print;

	*prterrorstring = '\0';
	print = *printtabptr + --prtnum;
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return PRTERR_NOTOPEN;
	linepos = print->linepos;
	i1 = prtflushline(prtnum, FALSE);
	if (i1) return i1;
	print = *printtabptr + prtnum;
	print->linepos = linepos;
	if (!(print->flags & PRTFLAG_FORMAT_FCC)) print->linebgn = linepos;

	i1 = prtflushbuf(prtnum);
	return i1;
}

INT prtnameisdevice(CHAR *prtname)
{
	INT i1;
	CHAR prtwork[MAX_NAMESIZE];

	*prterrorstring = '\0';
	for (i1 = (INT)strlen(prtname); i1 && prtname[i1 - 1] == ' '; i1--);
	memcpy(prtwork, prtname, i1);
	prtwork[i1] = '\0';
	return prtanametype(prtname) == PRTFLAG_TYPE_DEVICE;
}

INT prtisdevice(INT prtnum)
{
	PRINTINFO *print;

	*prterrorstring = '\0';
/*** CODE: TESTING FOR PRTFLAG_TYPE_PRINTER IS JUST A QUICK FIX UNTIL DBCPRT.C IS ***/
/***       ALSO CHANGED SO THAT V11.1 RELEASE WILL ADDITIONALLY CAUSE NATIVE/PIPE ***/
/***       TO BE CLOSED ***/
	print = *printtabptr + --prtnum;
	if (prtnum < 0 || prtnum >= printhi || !print->flags) return FALSE;
	return (print->flags & (PRTFLAG_TYPE_DEVICE | PRTFLAG_TYPE_PRINTER)) ? TRUE : FALSE;
}

INT prtdefname(_Out_ CHAR *prtname, INT size)
{
	*prterrorstring = '\0';
	return prtadefname(prtname, size);
}

INT prtgetprinters(CHAR **buffer, UINT size)
{
	*prterrorstring = '\0';
#if OS_WIN32
	return prtagetprinters(PRTGET_PRINTERS_DEFAULT | PRTGET_PRINTERS_ALL, buffer, size);
#else
	return prtagetprinters(buffer, size);
#endif
}

INT prtgetpaperbins(CHAR *prtname, CHAR **buffer, INT size)
{
	INT i1;
	CHAR prtwork[MAX_NAMESIZE];

	*prterrorstring = '\0';
	for (i1 = (INT)strlen(prtname); i1 && prtname[i1 - 1] == ' '; i1--);
	memcpy(prtwork, prtname, i1);
	prtwork[i1] = '\0';
#if OS_UNIX
	return prtagetpaperinfo(prtwork, buffer, size, GETPRINTINFO_PAPERBINS);
#else
	return prtagetpaperbins(prtwork, buffer, size);
#endif
}

INT prtgetpapernames(CHAR *prtname, CHAR **buffer, INT size)
{
	INT i1;
	CHAR prtwork[MAX_NAMESIZE];

	*prterrorstring = '\0';
	for (i1 = (INT)strlen(prtname); i1 && prtname[i1 - 1] == ' '; i1--);
	memcpy(prtwork, prtname, i1);
	prtwork[i1] = '\0';
#if OS_UNIX
	return prtagetpaperinfo(prtwork, buffer, size, GETPRINTINFO_PAPERNAMES);
#else
	return prtagetpapernames(prtwork, buffer, size);
#endif
}

INT prtgeterror(CHAR *errorstr, INT size)
{
	INT length;

	length = (INT)strlen(prterrorstring);
	if (--size >= 0) {
		if (size > length) size = length;
		if (size) memcpy(errorstr, prterrorstring, size);
		errorstr[size] = '\0';
		prterrorstring[0] = '\0';
	}
	return length;
}

INT prtputerror(CHAR *errorstr)
{
	INT length;

	length = (INT)strlen(errorstr);
	if (length > (INT) (sizeof(prterrorstring) - 1)) length = sizeof(prterrorstring) - 1;
	if (length) memcpy(prterrorstring, errorstr, length);
	prterrorstring[length] = '\0';
	return length;
}

INT prtflushline(INT prtnum, INT truncateflag)
{
	INT i1, i2, i3, cnt, linecnt, linepos;
	UCHAR c1, *buf, *line;
	PRINTINFO *print;
	INT move_position;

	move_position = 1;		/* true */
	print = *printtabptr + prtnum;
	linecnt = print->linecnt;
	if ((print->flags & PRTFLAG_FORMAT_DCC) && !truncateflag) {
		linepos = print->linepos - print->linebgn;
		if (linepos > linecnt) {
			if (linepos > print->linesize) {
				do {
					memset(*print->linebuf + linecnt, ' ', print->linesize - linecnt);
					i1 = prtbufcopy(prtnum, (CHAR *) *print->linebuf, print->linesize);
					if (i1) return i1;
					linecnt = 0;
					linepos -= print->linesize;
				} while (linepos > print->linesize);
			}
			memset(*print->linebuf + linecnt, ' ', linepos - linecnt);
			linecnt = linepos;
		}
	}
	print->linebgn = print->linepos = print->linecnt = 0;
	if (truncateflag & !(print->linetype & (PRT_TEXT_RIGHT | PRT_TEXT_CENTER))) {
		for (buf = *print->linebuf; linecnt && buf[linecnt - 1] == ' '; linecnt--);
	}
	if (!linecnt && (!(print->flags & PRTFLAG_FORMAT_FCC) || print->fmt.fcc.prefix == '+')) {
		return 0;
	}

	if (print->flags & (PRTFLAG_FORMAT_DCC | PRTFLAG_FORMAT_PCL)) {
		i1 = prtbufcopy(prtnum, (CHAR *) *print->linebuf, linecnt);
		if (i1) return i1;
	}
	else if (print->flags & PRTFLAG_FORMAT_FCC) {
		if (print->fmt.fcc.prefix == '+') {
			buf = *print->linebuf;
			for (buf = *print->linebuf; linecnt && buf[linecnt - 1] == ' '; linecnt--);
			if (!linecnt) return 0;
		}
		**print->buffer = print->fmt.fcc.prefix;
		memcpy(*print->buffer + 1, *print->linebuf, linecnt);
		print->bufcnt = linecnt + 1;
		print->fmt.fcc.prefix = '+';
		i1 = prtflushbuf(prtnum);
		if (i1) return i1;
	}
	else if (print->flags & PRTFLAG_FORMAT_PSL) {
#define MAX_PSL_SHOW 60
		line = *print->linebuf;
		buf = *print->buffer;
		for (i2 = 0; i2 < linecnt; ) {
			if (print->bufcnt + 8 + MAX_PSL_SHOW * 4 > print->bufsize) {
				i1 = prtflushbuf(prtnum);
				if (i1) return i1;
			}
			cnt = print->bufcnt;
			buf[cnt++] = '(';
			for (i3 = 0; i2 < linecnt && i3 < MAX_PSL_SHOW; i3++) {
				c1 = line[i2++];
				if (c1 == '(' || c1 == ')' || c1 == '\\') {  /* convert to octal */
					buf[cnt++] = '\\';
					buf[cnt++] = (UCHAR)((c1 >> 6) + '0');
					buf[cnt++] = (UCHAR)(((c1 >> 3) & 0x07) + '0');
					buf[cnt++] = (UCHAR)((c1 & 0x07) + '0');
				}
				else buf[cnt++] = c1;
			}
			memcpy(buf + cnt, ") show\n", 7);
			print->bufcnt = cnt + 7;
		}
	}
	else if (print->flags & PRTFLAG_FORMAT_PDF) {
		i1 = prtflushlinepdf(prtnum, linecnt);
	}
#if OS_WIN32
	else if (print->flags & PRTFLAG_FORMAT_GUI) {
		i1 = prtadrvtext(prtnum, *print->linebuf, linecnt);
		if (i1) return i1;
	}
#endif
	return 0;
}

INT prtflushbuf(INT prtnum)
{
	INT i1 = 0	;
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	if (!print->bufcnt) return 0;

	if (print->flags & PRTFLAG_TYPE_FILE) {
		i1 = prtfilwrite(prtnum);
	}
	else if (print->flags & PRTFLAG_TYPE_RECORD) {
		i1 = prtrecwrite(prtnum);
	}
	else if (print->flags & PRTFLAG_TYPE_DEVICE) {
		i1 = prtadevwrite(prtnum);
	}
#if OS_WIN32
	else if (print->flags & PRTFLAG_TYPE_PRINTER) {
		i1 = prtaprtwrite(prtnum);
	}
#endif
#if OS_UNIX
	else if (print->flags & PRTFLAG_TYPE_PIPE) {
		i1 = prtapipwrite(prtnum);
	}
	else if (print->flags & PRTFLAG_TYPE_CUPS) {
		i1 = prtcupswrite(prtnum);
	}
#endif
	else {
		prtputerror("In prtflushbuf, unknown PRTFLAG_TYPE");
		return PRTERR_INTERNAL;
	}
	print->filepos += print->bufcnt;
	print->bufcnt = 0;
	return i1;
}

INT prtbufcopy(INT prtnum, const CHAR *string, INT len)
{
	INT i1, i2, cnt;
	PRINTINFO *print;

	if (len == -1) len = (INT)strlen(string);
	print = *printtabptr + prtnum;

	for (cnt = 0; cnt < len; cnt += i2) {
		i2 = len - cnt;
		if (i2 > print->bufsize - print->bufcnt) i2 = print->bufsize - print->bufcnt;
		memcpy(*print->buffer + print->bufcnt, string + cnt, i2);
		print->bufcnt += i2;
		if (print->bufcnt == print->bufsize) {
			i1 = prtflushbuf(prtnum);
			if (i1) return i1;
		}
	}
	return 0;
}

static INT prtpclinit(INT prtnum)
{
	/*	\033E		PCL printer reset. Sets color mode to black-and-white.
					Creates 2-pen palette, 0=white, 1=black.
		\033%1B		Enter HP-GL/2 Mode, pen at current PCL cursor position
		RO270		Rotate the coordinate system 270 degrees ccw
		IP			Set scaling points P1 and P2 to default positions
		IW			Set input window to the PCL Picture Frame
		\033%1A		Enter PCL Mode, cursor at current HP-GL/2 pen position
		\033&a0V	Move the cursor to vertical position zero
		\033*v6W\000\001\010\010\010\010" 	Configure Image Data (CID) Enables PCL Imaging Mode
												Byte 0 - 000 Color Space 'Device Dependent RGB'
												Byte 1 - 001 Pixel Encoding Mode 'Indexed by Pixel'
												Byte 2 - 010 Bits per Index
												Bytes 3, 4, and 5 - 010, 010, 010 - number of bits per component
		\033%0B		Enter HP-GL/2 Mode, pen at previous HP-GL/2 pen position
		PC1,0,0,0;	PC is Pen Color. Set Pen 1 color to black
		\033%0A		Enter PCL Mode, cursor at previous PCL cursor position
		\033*v1S	Set Foreground Color (black)
	*/

#if 0
	static CHAR pclinit[] = "\033E\033%1BRO270IPIW\033%1A\033&a0V\033*v6W018888\033%0BPC1,0,0,0;\033%0A\033*v1S";
	static CHAR pclinit[] = "\033E\033%1BRO270IPIW\033%1A\033&a0V\033*v6W\000\001\010\010\010\010\033*v1S";
	/*
	 * Removed the CID command for now, until we figure out how to output color raster data to PCL
	 * 04/07/04 jpr
	 */
#endif
	static CHAR pclinit[] = "\033E\033%1BRO270;IP;IW;\033%1A\033&a0V\033*v1S";

	INT i1;
	CHAR work[32], *ptr, c1;
	PRINTINFO *print;

	print = *printtabptr + prtnum;

	if (!(print->openflags & PRT_NORE)) {
		i1 = prtbufcopy(prtnum, pclinit, sizeof(pclinit) - 1);
		if(!i1 && print->openflags & PRT_LAND) {
			i1 = prtbufcopy(prtnum, "\033&l1O", 5);
		}
		if (!i1 && print->dpi) {
			/* Set the Unit of Measure for PCL Unit cursor movements */
			/* This defaults to 300 if not specified */
			sprintf(work, "\033&u%dD", print->dpi);
			i1 = prtbufcopy(prtnum, work, -1);
		}
		if (i1) return i1;
	}

	/* Set the input bin */
	if (print->paperbin != NULL) {
		c1 = (*(print->paperbin))[0];
		if (isdigit((int)c1)) {
			memcpy(work, "\033&l", 3);
			work[3] = c1;
			work[4] = 'H';
			prtbufcopy(prtnum, work, 5);
		}
	}

	/* Set the Raster Graphics Resolution */
	/* We default this to 300 if the Z option is absent. (PCL would default this to 75!) */
	if (!print->dpi) print->dpi = 300;
	sprintf(work, "\033*t%dR", print->dpi);
	i1 = prtbufcopy(prtnum, work, -1);
	if (i1) return i1;

	if (!prpget("print", "pcl", "lor", NULL, &ptr, 0) && *ptr) {
		strcpy(work, "\033&l");
		strcat(work, ptr);
		strcat(work, "U");
		i1 = prtbufcopy(prtnum, work, -1);
		if (i1) return i1;
	}
	print->flags |= PRTFLAG_INIT;
	print->pagecount = 1;
	prtfont(prtnum + 1, "COURIER(12,PLAIN)");
	prtlinewidth(prtnum + 1, 1);
	return 0;
}

static INT prtpclend(INT prtnum)
{
	return prtbufcopy(prtnum, "\033E", 2);
}

static void setWidthHeightFromPaper(DOUBLE *width, DOUBLE *height, INT prtnum) {
	PRINTINFO *print;
	CHAR *ptr;

	print = *printtabptr + prtnum;
	*width = WIDTH_LETTER;
	*height = HEIGHT_LETTER;
	if (print->papersize != NULL) {
		ptr = *print->papersize;
		if (!guimemicmp(ptr, "LEGAL", 5)) {
			*width = WIDTH_LEGAL;
			*height = HEIGHT_LEGAL;
		}
		else if (!strcmp(ptr, "COMPUTER") || !strcmp(ptr, "computer")) {
			*width = WIDTH_COMPUTER;
			*height = HEIGHT_COMPUTER;
		}
		else if (!strcmp(ptr, "A3") || !strcmp(ptr, "a3")) {
			*width = WIDTH_A3;
			*height = HEIGHT_A3;
		}
		else if (!strcmp(ptr, "A4") || !strcmp(ptr, "a4")) {
			*width = WIDTH_A4;
			*height = HEIGHT_A4;
		}
		else if (!strcmp(ptr, "B4") || !strcmp(ptr, "b4")) {
			*width = WIDTH_B4;
			*height = HEIGHT_B4;
		}
		else if (!strcmp(ptr, "B5") || !strcmp(ptr, "b5")) {
			*width = WIDTH_B5;
			*height = HEIGHT_B5;
		}
	}
}

/**
 * Postscript
 */
static INT prtpslinit(INT prtnum)
{
	INT i1;
	DOUBLE width, height;
	CHAR work[16];
	PRINTINFO *print;
	PRTMARGINS margins;

	setWidthHeightFromPaper(&width, &height, prtnum);
	print = *printtabptr + prtnum;

#ifdef USEPJL
	i1 = prtbufcopy(prtnum, "\033%-12345X@PJL\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "@PJL SET LPARM : POSTSCRIPT PRTPSERRS = ON\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "@PJL ENTER LANGUAGE = POSTSCRIPT\n", -1);
#endif

	i1 = prtbufcopy(prtnum, "%!PS-Adobe-3.0\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "%%LanguageLevel: 2\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "%%Orientation: ", -1);
	if (print->openflags & PRT_LAND) i1 = prtbufcopy(prtnum, "Landscape", 9);
	else i1 = prtbufcopy(prtnum, "Portrait", 8);
	if (!i1) i1 = prtbufcopy(prtnum, "\n%%DocumentMedia: Plain ", -1);
	if (!i1) prtdbltoa(width * 72, work, 4);
	if (!i1) i1 = prtbufcopy(prtnum, work, -1);
	if (!i1) i1 = prtbufcopy(prtnum, " ", 1);
	if (!i1) prtdbltoa(height * 72, work, 4);
	if (!i1) i1 = prtbufcopy(prtnum, work, -1);
	if (!i1) i1 = prtbufcopy(prtnum, " 75 white ()\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "%%Creator: ", -1);
	if (!i1) i1 = prtbufcopy(prtnum, program, -1);
	if (!i1) i1 = prtbufcopy(prtnum, "\n%%EndComments\n%%BeginProlog\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "/CR {ULX currentpoint exch pop moveto} bind def\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "/LF {0 FH neg rmoveto} bind def\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "/HT {CR ( ) stringwidth pop mul 0 rmoveto} bind def\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "/LN {lineto currentpoint stroke moveto} bind def\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "%%EndProlog\n%%BeginSetup\n", -1);
	if (i1) return i1;

	i1 = 0;
	/* In PS default user space, vertical and horizontal units are 1/72 of an inch */
	if (!i1) i1 = prtbufcopy(prtnum, "/SX ", -1);
	if (anyUserSetPrintMargins(print->margins)) {
		normalizeUserPrintMargins(&print->margins, &margins);
		if (!i1) prtdbltoa((width * 72) /* number of horizontal units in default user space */
				/ ((width - margins.left_margin - margins.right_margin) * ((DOUBLE) print->dpi)), work, 4);
	}
	else {
		if (!i1) prtdbltoa((width * 72) /* number of horizontal units in default user space */
				/ ((width - MARGINX) * ((DOUBLE) print->dpi)), work, 4);
	}
	if (!i1) i1 = prtbufcopy(prtnum, work, -1);
	if (!i1) i1 = prtbufcopy(prtnum, " def\n/SY ", -1);
	if (anyUserSetPrintMargins(print->margins)) {
		if (!i1) prtdbltoa((height * 72)
				/ ((height - margins.top_margin - margins.bottom_margin) * ((DOUBLE) print->dpi)), work, 4);
	}
	else {
		if (!i1) prtdbltoa((height * 72) / ((height - MARGINY) * ((DOUBLE) print->dpi)), work, 4);
	}
	if (!i1) i1 = prtbufcopy(prtnum, work, -1);
	if (!i1) i1 = prtbufcopy(prtnum, " def\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "SX SY scale\n", -1);
	if (!i1) i1 =  prtbufcopy(prtnum, "clippath pathbbox /ULY exch def pop pop /ULX exch def newpath\n", -1);
	print->flags |= PRTFLAG_INIT;
	prtfont(prtnum + 1, "COURIER(12,PLAIN)");
	if (!i1) i1 =  prtbufcopy(prtnum, "%%EndSetup\n", -1);
	if ( (i1 = prtpslstartpage(prtnum)) ) return i1;
	if (anyUserSetPrintMargins(print->margins)) {
		print->fmt.psl.width = (INT)(print->dpi * (width - margins.left_margin - margins.right_margin));
		print->fmt.psl.height = (INT)(print->dpi * (height - margins.top_margin - margins.bottom_margin));
	}
	else {
		print->fmt.psl.width = (INT)(print->dpi * (width - MARGINX));
		print->fmt.psl.height = (INT)(print->dpi * (height - MARGINY));
	}
	print->fmt.psl.posx = 0;
	print->fmt.psl.posy = 0;
	return 0;
}

static INT prtpslend(INT prtnum)
{
	INT i1;
	PRINTINFO *print;
	CHAR work[32];

	i1 = prtbufcopy(prtnum, "pagelevel restore\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "showpage\n", 9);
	if (i1) return i1;
	if ( (i1 = prtbufcopy(prtnum, "%%Trailer\n", 10)) ) return i1;
	if ( (i1 = prtbufcopy(prtnum, "%%Pages: ", 9)) ) return i1;
	print = *printtabptr + prtnum;
	if ( (i1 = prtbufcopy(prtnum, work, mscitoa(print->pagecount, work))) ) return i1;
	return prtbufcopy(prtnum, "\n%%EOF\n", 7);
}

INT compress_rle(UCHAR *src, UCHAR *dst, INT src_size) {

	INT src_scan;		/* index into src */
	INT have_run;		/* bool */
	INT dst_size;		/* num of characters in compressed data */
	INT run_end;
	INT length = 0;		/* size of a run */
	UCHAR *header;
	UCHAR cur_byte;

	src_scan = 0;
	dst_size = 0;
	header = NULL;
	while (src_scan < src_size)
	{
		cur_byte = src[src_scan++];
		have_run = FALSE;
		if (cur_byte == src[src_scan] && cur_byte == src[src_scan + 1] && src_scan + 1 < src_size) {
			src[src_size] = !cur_byte;		/* sentinel byte, make sure run ends */
			run_end = src_scan;
			while(src[run_end] == cur_byte) run_end++;
			have_run = TRUE;
			if (header != NULL) {				/* do we have a previous unpacked string? */
				*header = (UCHAR) (length - 1); /* close it */
				header = NULL;
			}
			length = run_end - src_scan + 1;
			src_scan = run_end;
		}
		if (have_run) {
			for (;;) {
				if (length < 129) {
					dst[dst_size++] = (UCHAR) (257 - length);
					dst[dst_size++] = cur_byte;
					break;
				}
				else {
					dst[dst_size++] = (UCHAR) (257 - 128);
					dst[dst_size++] = cur_byte;
					length -= 128;
				}
			}
		}
		else {
			if (header == NULL) {				/* if necessary, start a new unpacked string */
				header = &dst[dst_size++];
				length = 0;
			}
			dst[dst_size++] = cur_byte;
			if (++length == 128) {
				*header = (UCHAR) (length - 1);		/* close it */
				header = NULL;
			}
		}
	}
	if (header != NULL) {					/* do we have a previous unpacked string? */
		*header = (UCHAR) (length - 1);		/* close it */
	}
	dst[dst_size++] = 0x80;					/* PDF EOD */
	return dst_size;
}

/**
 * Returns number of characters put in str not counting the null
 */
INT prtdbltoa(DOUBLE d1, CHAR *str, INT precision)
{
	INT i1, cnt, dec, sign;
	CHAR *ptr;

#if OS_WIN32
	ptr = _fcvt(d1, precision, &dec, &sign);
#else
	ptr = fcvt(d1, precision, &dec, &sign);
#endif
	while (*ptr && *ptr == '0') {
		ptr++;
		dec--;
	}
	if (!*ptr) {  /* value is all zeros */
		str[0] = '0';
		str[1] = '\0';
		return 1;
	}

	cnt = 0;
	if (sign) str[cnt++] = '-';
	if (dec < 0) {
		str[cnt++] = '.';
		for (i1 = 0; i1 > dec; i1--) str[cnt++] = '0';
	}
	for (i1 = 0; ptr[i1]; i1++) {
		if (i1 == dec) str[cnt++] = '.';  /* insert decimal point where it belongs */
		str[cnt++] = ptr[i1];
	}
	str[cnt] = '\0';
	if (i1 > dec) {  /* trim off trailing zeros */
		while (str[--cnt] == '0');
		if (str[cnt] != '.') cnt++;  /* else remove decimal */
		str[cnt] = '\0';
	}
	return cnt;
}

static INT prtfilopen(INT prtnum, INT alloctype)
{
#ifndef DX_SINGLEUSER
	INT srvhandle = 0;
#endif
	INT handle, opts;
	CHAR prtname[MAX_NAMESIZE];
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	strcpy(prtname, *print->prtname);

	handle = ERR_FNOTF;
#ifndef DX_SINGLEUSER
	if (getserver != NULL) srvhandle = getserver(prtname, FIO_OPT_PRTPATH);
	if (srvhandle) {
		CHAR work[256];
		if (srvhandle == -1) {
			prtputerror("server not available");
			return PRTERR_SERVER;
		}
		opts = 0;
		if (!(alloctype & PRT_WAIT)) opts = FS_CREATEONLY;
		else if (print->openflags & PRT_APPD) {
			handle = fsopenraw(srvhandle, prtname, FS_EXCLUSIVE);
			/* convert back to fio type error (required for pre-fs 2.1.3) */
			if (handle == -601) handle = ERR_FNOTF;
			else if (handle < -1650 && handle >= -1699) handle += 1630;
			else if (handle < -1750 && handle >= -1799) handle += 1730;
		}
		if (handle == ERR_FNOTF) handle = fsprepraw(srvhandle, prtname, opts);
		if (handle < 0) {
			/* convert back to fio type error (required for pre-fs 2.1.3) */
			if (handle < -1650 && handle >= -1699) handle += 1630;
			else if (handle < -1750 && handle >= -1799) handle += 1730;
			if (!(alloctype & PRT_WAIT) && (handle == ERR_NOACC || handle == -602 || handle == -607 || handle == ERR_EXIST || handle == -605)) return PRTERR_EXIST;
			/* -600 errors are returned by pre-fs 2.1.3) */
			if (handle == ERR_FNOTF || handle == -601 || handle == ERR_BADNM || handle == -603) handle = PRTERR_BADNAME;
			else if (handle == ERR_NOACC || handle == -602 || handle == -607) handle = PRTERR_ACCESS;
			else if (handle == ERR_EXIST || handle == -605) handle = PRTERR_EXIST;
			else if (handle == ERR_NOMEM || handle == -614) handle = PRTERR_NOMEM;
			else if (handle == -1) {
				fsgeterror(work, sizeof(work));
				prtputerror(work);
				handle = PRTERR_SERVER;
			}
			else {  /* let -600 errors get displayed as unknown error */
				prtputerror(fioerrstr(handle));
				handle = PRTERR_CREATE;
			}
			return handle;
		}
		print = *printtabptr + prtnum;
		if (print->openflags & PRT_APPD) fsfilesize(handle, &print->filepos);
		print->flags |= PRTFLAG_SERVER;
	}
	else
#endif
	{
		opts = FIO_M_PRP | FIO_P_PRT;
		if (!(alloctype & PRT_WAIT)) opts = FIO_M_EFC | FIO_P_PRT;
		else if (print->openflags & PRT_APPD) handle = fioopen(prtname, FIO_M_EXC | FIO_P_PRT);
		if (handle == ERR_FNOTF) handle = fioopen(prtname, opts);
		if (handle < 0) {
			if (!(alloctype & PRT_WAIT) && (handle == ERR_NOACC || handle == ERR_EXIST)) return PRTERR_EXIST;
/*** CODE: DOES PRT_TEST AND ERR_NOACC BECOME ERR_EXIST? (WITH/WITHOUT CREATEFLAG?) ***/
			if (handle == ERR_FNOTF || handle == ERR_BADNM) handle = PRTERR_BADNAME;
			else if (handle == ERR_NOACC) handle = PRTERR_ACCESS;
			else if (handle == ERR_EXIST) handle = PRTERR_EXIST;
			else if (handle == ERR_NOMEM) handle = PRTERR_NOMEM;
			else {
				prtputerror(fioerrstr(handle));
				handle = PRTERR_CREATE;
			}
			return handle;
		}
		print = *printtabptr + prtnum;
		if (print->openflags & PRT_APPD) fiogetsize(handle, &print->filepos);
	}
	print->prthandle.handle = handle;
	return 0;

}

/**
 * Always returns zero
 */
static INT prtfilclose(INT prtnum)
{
	PRINTINFO *print;

	print = *printtabptr + prtnum;
#ifndef DX_SINGLEUSER
	if (print->flags & PRTFLAG_SERVER) fsclose(print->prthandle.handle);
	else
#endif
		fioclose(print->prthandle.handle);
	return 0;
}

#if OS_UNIX
static INT prtcupswrite(INT prtnum) {
	INT i1;
	PRINTINFO *print;
	print = *printtabptr + prtnum;
	i1 = fiowrite(print->prthandle.handle, print->filepos, *print->buffer, print->bufcnt);
	if (i1) {
		prtputerror(fioerrstr(i1));
		if (i1 == ERR_WRERR) i1 = PRTERR_NOSPACE;
		else i1 = PRTERR_WRITE;
		return i1;
	}
	return 0;
}
#endif

static INT prtfilwrite(INT prtnum)
{
	INT i1;
	PRINTINFO *print;

	print = *printtabptr + prtnum;
#if !defined(DX_SINGLEUSER)
	if (print->flags & PRTFLAG_SERVER) {
		CHAR work[256];
		i1 = fswriteraw(print->prthandle.handle, print->filepos, (CHAR *) *print->buffer, print->bufcnt);
		if (i1) {
			/* convert back to fio type error (required for pre-fs 2.1.3) */
			if (i1 < -1750 && i1 >= -1799) i1 += 1730;
			/* -700 errors are returned by pre-fs 2.1.3) */
			if (i1 == ERR_WRERR || i1 == -706) i1 = PRTERR_NOSPACE;
			else if (i1 == -1) {
				fsgeterror(work, sizeof(work));
				prtputerror(work);
				i1 = PRTERR_SERVER;
			}
			else {  /* let -700 errors get displayed as unknown error */
				prtputerror(fioerrstr(i1));
				i1 = PRTERR_WRITE;
			}
			return i1;
		}
	}
	else
#endif
	{
		i1 = fiowrite(print->prthandle.handle, print->filepos, *print->buffer, print->bufcnt);
		if (i1) {
			prtputerror(fioerrstr(i1));
			if (i1 == ERR_WRERR) i1 = PRTERR_NOSPACE;
			else i1 = PRTERR_WRITE;
			return i1;
		}
	}
	return 0;
}

static INT prtrecopen(INT prtnum, INT alloctype)
{
#ifndef DX_SINGLEUSER
	INT srvhandle = 0;
#endif
	INT handle, opts;
	OFFSET pos;
	CHAR prtname[MAX_NAMESIZE];
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	strcpy(prtname, *print->prtname);
	handle = ERR_FNOTF;
#ifndef DX_SINGLEUSER
	if (getserver != NULL) srvhandle = getserver(prtname, FIO_OPT_PRTPATH);
	if (srvhandle) {
		CHAR work[256];
		if (srvhandle == -1) {
			prtputerror("server not available");
			return PRTERR_SERVER;
		}
		if (!(print->openflags & PRT_COMP)) opts = FS_TEXT | FS_NOEXTENSION;
		else opts = FS_NOEXTENSION;
		if (!(alloctype & PRT_WAIT)) opts |= FS_CREATEONLY;
		else if (print->openflags & PRT_APPD) {
			handle = fsopen(srvhandle, prtname, FS_EXCLUSIVE | FS_NOEXTENSION, PRT_FCC_MAXSIZE + 1);
			/* convert back to fio type error (required for pre-fs 2.1.3) */
			if (handle == -601) handle = ERR_FNOTF;
			else if (handle < -1650 && handle >= -1699) handle += 1630;
			else if (handle < -1750 && handle >= -1799) handle += 1730;
		}
		if (handle == ERR_FNOTF) handle = fsprep(srvhandle, prtname, opts, PRT_FCC_MAXSIZE + 1);
		if (handle < 0) {
			/* convert back to fio type error (required for pre-fs 2.1.3) */
			if (handle < -1650 && handle >= -1699) handle += 1630;
			else if (handle < -1750 && handle >= -1799) handle += 1730;
			if (!(alloctype & PRT_WAIT) && (handle == ERR_NOACC || handle == -602 || handle == -607 || handle == ERR_EXIST || handle == -605)) return PRTERR_EXIST;
			/* -600 errors are returned by pre-fs 2.1.3) */
			if (handle == ERR_FNOTF || handle == -601 || handle == ERR_BADNM || handle == -603) handle = PRTERR_BADNAME;
			else if (handle == ERR_NOACC || handle == -602 || handle == -607) handle = PRTERR_ACCESS;
			else if (handle == ERR_EXIST || handle == -605) handle = PRTERR_EXIST;
			else if (handle == ERR_NOMEM || handle == -614) handle = PRTERR_NOMEM;
			else if (handle == -1) {
				fsgeterror(work, sizeof(work));
				prtputerror(work);
				handle = PRTERR_SERVER;
			}
			else {  /* let -600 errors get displayed as unknown error */
				prtputerror(fioerrstr(handle));
				handle = PRTERR_CREATE;
			}
			return handle;
		}
		print = *printtabptr + prtnum;
		if (print->openflags & PRT_APPD) fssetposateof(handle);
		print->flags |= PRTFLAG_SERVER;
	}
	else
#endif
	{
		if (!(print->openflags & PRT_COMP)) opts = RIO_M_PRP | RIO_P_PRT | RIO_T_TXT | RIO_NOX;
		else opts = RIO_M_PRP | RIO_P_PRT | RIO_T_STD | RIO_NOX;
		if (!(alloctype & PRT_WAIT)) opts = (opts & ~RIO_M_PRP) | RIO_M_EFC;
		else if (print->openflags & PRT_APPD) handle = rioopen(prtname, RIO_M_EXC | RIO_P_PRT | RIO_T_ANY | RIO_NOX, 2, PRT_FCC_MAXSIZE + 1);
		if (handle == ERR_FNOTF) handle = rioopen(prtname, opts, 2, PRT_FCC_MAXSIZE + 1);
		if (handle < 0) {
			if (!(alloctype & PRT_WAIT) && (handle == ERR_NOACC || handle == ERR_EXIST)) return PRTERR_EXIST;
/*** CODE: DOES PRT_TEST AND ERR_NOACC BECOME ERR_EXIST? (WITH/WITHOUT CREATEFLAG?) ***/
			if (handle == ERR_FNOTF || handle == ERR_BADNM) handle = PRTERR_BADNAME;
			else if (handle == ERR_NOACC) handle = PRTERR_ACCESS;
			else if (handle == ERR_EXIST) handle = PRTERR_EXIST;
			else if (handle == ERR_NOMEM) handle = PRTERR_NOMEM;
			else {
				prtputerror(fioerrstr(handle));
				handle = PRTERR_CREATE;
			}
			return handle;
		}
		print = *printtabptr + prtnum;
		if (print->openflags & PRT_APPD) {
			rioeofpos(handle, &pos);
			riosetpos(handle, pos);
		}
	}
	print->prthandle.handle = handle;
	print->fmt.fcc.prefix = '+';
	return 0;
}

static INT prtrecclose(INT prtnum)
{
	PRINTINFO *print;

	print = *printtabptr + prtnum;
#ifndef DX_SINGLEUSER
	if (print->flags & PRTFLAG_SERVER) fsclose(print->prthandle.handle);
	else
#endif
		rioclose(print->prthandle.handle);
	return 0;
}

static INT prtrecwrite(INT prtnum)
{
	INT i1;
	PRINTINFO *print;

	print = *printtabptr + prtnum;
#ifndef DX_SINGLEUSER
	if (print->flags & PRTFLAG_SERVER) {
		i1 = fswritenext(print->prthandle.handle, (CHAR *) *print->buffer, print->bufcnt);
		if (i1) {
			CHAR work[256];
			/* convert back to fio type error (required for pre-fs 2.1.3) */
			if (i1 < -1750 && i1 >= -1799) i1 += 1730;
			/* -700 errors are returned by pre-fs 2.1.3) */
			if (i1 == ERR_WRERR || i1 == -706) i1 = PRTERR_NOSPACE;
			else if (i1 == -1) {
				fsgeterror(work, sizeof(work));
				prtputerror(work);
				i1 = PRTERR_SERVER;
			}
			else {  /* let -700 errors get displayed as unknown error */
				prtputerror(fioerrstr(i1));
				i1 = PRTERR_WRITE;
			}
			return i1;
		}
	}
	else
#endif
	{
		i1 = rioput(print->prthandle.handle, *print->buffer, print->bufcnt);
		if (i1) {
			prtputerror(fioerrstr(i1));
			if (i1 == ERR_WRERR) i1 = PRTERR_NOSPACE;
			else i1 = PRTERR_WRITE;
			return i1;
		}
	}
	return 0;
}

/**
 * Set up the print->fmt.pdf.imgobjs array
 * Will return either zero or PRTERR_NOMEM
 * prtnum is zero-based
 *
 * Might move memory
 */
INT prtSetPdfImageCacheSize(INT prtnum) {
	PRINTINFO *print;
	UCHAR **prtimg;
	INT i1;

	print = *printtabptr + prtnum;

	if (print->fmt.pdf.imgobjcount + print->fmt.pdf.imagecount == print->fmt.pdf.imgobjsize) {
		if (!print->fmt.pdf.imgobjsize) {
			print->fmt.pdf.imgobjsize = 16;
			prtimg = memalloc(sizeof(PDFDISKIMAGE) * print->fmt.pdf.imgobjsize, MEMFLAGS_ZEROFILL);
			if (prtimg == NULL) return PRTERR_NOMEM;
			print = *printtabptr + prtnum;
			print->fmt.pdf.imgobjs = (PDFDISKIMAGE **) prtimg;
		}
		else {
			print->fmt.pdf.imgobjsize <<= 1;
			i1 = memchange((UCHAR **) print->fmt.pdf.imgobjs, sizeof(PDFDISKIMAGE) * print->fmt.pdf.imgobjsize,
					0);
			if (i1) return PRTERR_NOMEM;
		}
	}
	return 0;
}

void InitializePrintOptions(PRTOPTIONS *options) {
	options->flags = 0;
	options->copies = 0;
	options->dpi = 0;
	options->papersize[0] = '\0';
	options->paperbin[0] = '\0';
	options->margins.margin = INVALID_PRINT_MARGIN;
	options->margins.top_margin = INVALID_PRINT_MARGIN;
	options->margins.bottom_margin = INVALID_PRINT_MARGIN;
	options->margins.left_margin = INVALID_PRINT_MARGIN;
	options->margins.right_margin = INVALID_PRINT_MARGIN;
	strcpy(options->docname, "DB/C");
}

static void normalizeUserPrintMargins(PRTMARGINS* in_mgns, PRTMARGINS* out_mgns) {
	if (in_mgns->margin != INVALID_PRINT_MARGIN) {
		out_mgns->top_margin = in_mgns->margin;
		out_mgns->bottom_margin = in_mgns->margin;
		out_mgns->left_margin = in_mgns->margin;
		out_mgns->right_margin = in_mgns->margin;
	}
	else {
		out_mgns->top_margin = MARGIN_TOP;
		out_mgns->bottom_margin = MARGIN_BOTTOM;
		out_mgns->left_margin = MARGIN_LEFT;
		out_mgns->right_margin = MARGIN_RIGHT;
	}
	if (in_mgns->top_margin != INVALID_PRINT_MARGIN) out_mgns->top_margin = in_mgns->top_margin;
	if (in_mgns->bottom_margin != INVALID_PRINT_MARGIN) out_mgns->bottom_margin = in_mgns->bottom_margin;
	if (in_mgns->left_margin != INVALID_PRINT_MARGIN) out_mgns->left_margin = in_mgns->left_margin;
	if (in_mgns->right_margin != INVALID_PRINT_MARGIN) out_mgns->right_margin = in_mgns->right_margin;
	return;
}

/**
 * Set the PRINTINFO fields;
 * 	fmt.pdf.{width height gwidth gheight}
 */
void prtSetPdfSize(INT prtnum) {
	PRINTINFO *print;
	/* width and height are in inches */
	DOUBLE height, width;
	PRTMARGINS margins;

	setWidthHeightFromPaper(&width, &height, prtnum);
	print = *printtabptr + prtnum;
	if (print->openflags & PRT_LAND) {
		DOUBLE x1 = width;
		width = height;
		height = x1;
	}
	/*
	 * For 16.1 we added to the splopen verb. The user can modify the margins from
	 * the default of 1/4 inch on each side
	 */
	if (!anyUserSetPrintMargins(print->margins)) {
		/* define the width and height of the printable area in PDF default unit size (1/72 inch) */
		print->fmt.pdf.width = (INT) ((PDFUSERSPACEUNITS * width) - (PDFUSERSPACEUNITS * MARGINX));
		print->fmt.pdf.height = (INT) ((PDFUSERSPACEUNITS * height) - (PDFUSERSPACEUNITS * MARGINY));
		/*
		 * define the width and height of the printable area in user units, which default to 300dpi
		 * and can be set using the Z option in a SPLOPEN verb
		 */
		print->fmt.pdf.gwidth = (INT) ((print->dpi * width) - (print->dpi * MARGINX));
		print->fmt.pdf.gheight = (INT) ((print->dpi * height) - (print->dpi * MARGINY));
	}
	else {
		normalizeUserPrintMargins(&print->margins, &margins);
		print->fmt.pdf.width = (INT) ((PDFUSERSPACEUNITS * width)
				- (PDFUSERSPACEUNITS * margins.left_margin) - (PDFUSERSPACEUNITS * margins.right_margin));
		print->fmt.pdf.height = (INT) ((PDFUSERSPACEUNITS * height)
				- (PDFUSERSPACEUNITS * margins.top_margin) - (PDFUSERSPACEUNITS * margins.bottom_margin));
		print->fmt.pdf.gwidth = (INT) ((print->dpi * width)
				- (print->dpi * margins.left_margin) - (print->dpi * margins.right_margin));
		print->fmt.pdf.gheight = (INT) ((print->dpi * height)
				- (print->dpi * margins.top_margin) - (print->dpi * margins.bottom_margin));
	}
}

