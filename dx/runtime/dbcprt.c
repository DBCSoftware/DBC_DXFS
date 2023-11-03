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

#include <assert.h>
#define INC_STDIO
#define INC_STRING
#define INC_CTYPE
#define INC_STDLIB
#define INC_SIGNAL
#define INC_ERRNO
#define INC_MATH
#include "includes.h"
#include "release.h"
#include "base.h"
#include "dbc.h"
#include "dbcclnt.h"
#include "evt.h"
#include "gui.h"
#include "imgs.h"
#include "prt.h"

/* routines in dbcctl.c */
extern PIXHANDLE refnumtopixhandle(INT);
extern CHAR * refnumtoimagename(INT);

typedef struct {
	INT type;
	INT prtnum;	/* One-Based index into the printtabptr */
	INT module;
	INT offset;
	INT noeject;
	INT useclnt; /*/ Printing at Smart Client? */
	INT flags;
	/*
	 * This gets set in getsplrefnum from initinfo
	 */
	enum PRINT_24BITCOLOR_FORMAT colorFormat;
} SPLINFO;

#define SPL_INTERNAL	0x01
#define SPL_PFILE		0x02
#define SPL_COMMON		0x04

#define DBCPRTFLAG_DSPERR		0x01
#define DBCPRTFLAG_AUTOFLUSH	0x02

/* defines for control */
#define PRT_SL 0x01
#define PRT_LL 0x02
#define PRT_ZF 0x04
#define PRT_ZS 0x08
#define PRT_FM 0x10

/* local variables */
static INT dbcprtflags = 0;
static SPLINFO **spltabptr;
static INT splhi, splmax;
static INT splrefnum[10];
static INT splnum;			/* current spool number (non-pfile) */
static CHAR printerrstr[256];

/* local function prototypes */
static INT printgetvalue(void);
static void printdefname(CHAR *, INT);
static INT printmemicmp(void *src, void *dest, INT len);
static INT getsplrefnum(void);
static INT printvalue(CHAR *error);
static INT printerror(INT);

static INT testSplopnOption(UCHAR *keyword, UCHAR *optlist, INT index) {
	INT kwlen = (INT)strlen((CHAR*)keyword);
	INT i1;
	if (lp - index < kwlen) return FALSE;
	for (i1 = 0; i1 < kwlen; i1++) {
		if (toupper(optlist[i1]) != keyword[i1]) return FALSE;
	}
	return TRUE;
}


static void getDoubleSplopnOption(UCHAR *errmsg, INT index, UCHAR *optlist, DOUBLE *dest) {
	UCHAR work[128];
	INT i1 = 0;
	sprintf((CHAR*)work, "Bad %s option", errmsg);
	if (index != lp && optlist[i1++] == '=') {
		*dest = strtod((const char *)&optlist[i1], NULL);
		if (*dest == HUGE_VAL || *dest < 0.0
#ifdef ERANGE
				|| errno == ERANGE
#endif
			) dbcerrinfo(411, work, -1);
	}
	else dbcerrinfo(411, work, -1);
}


void vsplopen()
{
	INT i1, i2, i3, mod, off, newnum, pfileflg, prtnum = 0, refnum;
	CHAR language[100], prtname[200], work[100], clntprt[500], *ptr;
	UCHAR c1, *adr;
	DAVB *davb;
	PRTOPTIONS options;
	ELEMENT *e1;

	if (fpicnt) filepi0();  /* cancel any filepi */
	adr = getvar(VAR_READ);
	if (*adr == DAVB_PFILE) {
		getlastvar(&mod, &off);
		davb = aligndavb(adr, &i1);
		off += i1;
		adr = getvar(VAR_READ);
		pfileflg = TRUE;
	}
	else pfileflg = FALSE;

	/* At this point, adr points to the print destination */
	cvtoname(adr);
	strncpy(prtname, name, sizeof(prtname) - 1);
	prtname[sizeof(prtname) - 1] = '\0';
	language[0] = '\0';
	newnum = 0;
	InitializePrintOptions(&options);
	adr = NULL;
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		if (!prpget("print", "destination", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "client")) {
			options.flags |= PRT_CLNT;
		}
		if (vbcode == 0x47) { /*Did the programmer supply an option operand? */
			adr = getvar(VAR_READ);
			if (fp) { /* and does it actually have any content? */
				/* Scan for smart client printing option */
				for (i1 = hl + fp - 1, i2 = lp + hl; i1 < i2; ) {
					if (toupper(adr[i1++]) == 'D') {
						if ((i1 + 4) >= i2) continue;
						if (toupper(adr[i1++]) != 'E' || toupper(adr[i1++]) != 'S' || toupper(adr[i1++]) != 'T') continue;
						if (adr[i1] != '=') dbcerrinfo(411, (UCHAR*)"Bad Dest option", -1);
						for (i1++, i2 = 0; i2 < (INT) (sizeof(work) - 1) && i1 < lp + hl && adr[i1] != ' ' && adr[i1] != ','; ) {
							work[i2++] = toupper(adr[i1++]);
						}
						work[i2] = '\0';
						if (!i2) dbcerrinfo(411, (UCHAR*)"Bad Dest option", -1);
						if (!strcmp(work, "CLIENT")) {
							options.flags |= PRT_CLNT;
							break;
						}
						else if (!strcmp(work, "SERVER")) {
							options.flags &= ~PRT_CLNT;
							break;
						}
						else dbcerrinfo(411, (UCHAR*)"Bad Dest option", -1);
					}
				}
			}
		}
	}

	if (vbcode == 0x47) {
		if (adr == NULL) adr = getvar(VAR_READ);
		if (fp) {
			for (i1 = hl + fp - 1, lp += hl; i1 < lp; ) {
				c1 = adr[i1++];
				switch (toupper(c1)) {
					case 'A':  /* allocate now */
						options.flags |= PRT_TEST;
						break;

					/*
					 * printer bin source or BANNER or BOTTOMMARGIN
					 */
					case 'B':
						/* It is an error for a B to appear as the last character of the option string */
						if (i1 == lp) dbcerrinfo(411, (UCHAR*)"Invalid 'B' option", -1);
						/* If the next character is '=' then assume this is the paper bin option */
						if (adr[i1] == '=') {
							for (i1++, i2 = 0; i2 < (INT) (sizeof(work) - 1) && i1 < lp; i1++) {
								if (adr[i1] == ',') {
									if (i1 == 0 || work[i2 - 1] != '\\') break;
									work[i2 - 1] = ',';
								}
								else work[i2++] = adr[i1];
							}
							while (i2 && work[i2 - 1] == ' ') i2--;
							work[i2] = '\0';
							if (!i2) dbcerrinfo(411, (UCHAR*)"Bad Paper Bin option", -1);
							strcpy(options.paperbin, work);
						}
						else if (testSplopnOption((UCHAR*)"BANNER", &adr[i1 - 1], i1 - 1))
						{
							options.flags |= PRT_BANR;
							i1 += 5;
						}
						else if (testSplopnOption((UCHAR*)"BOTTOMMARGIN", &adr[i1 - 1], i1 - 1))
						{
							i1 += 11;
							getDoubleSplopnOption((UCHAR*)"Bottommargin", i1, &adr[i1], &options.margins.bottom_margin);
						}
						else dbcerrinfo(411, (UCHAR*)"Unknown 'B' option", -1);
						break;
					case 'C':  /* compressed file */
					case 'H':
						if (options.flags & (PRT_LCTL | PRT_WDRV)) dbcerrinfo(411, (UCHAR*)"Invalid 'C' or 'H' option", -1);
						options.flags |= PRT_COMP;
						break;
					case 'D':  /* device ctl chars or printer destination */
						if (testSplopnOption((UCHAR*)"DEST=", &adr[i1 - 1], i1 - 1)) {
							i1 += 4;
							while (i1 < lp && adr[i1] != ' ' && adr[i1] != ',') { i1++; }
							break;
						}
						/* If not Dest then assume it is 'device control characters' option. */
						if (options.flags & (PRT_COMP | PRT_WDRV)) dbcerrinfo(411, (UCHAR*)"Invalid 'D' option", -1);
						options.flags |= PRT_LCTL | PRT_WRAW;
						break;

					case 'I':
						options.flags |= PRT_NOEJ;
						break;
					case 'J':  /* job dialog at spool open */
						if (options.flags & (PRT_COMP | PRT_LCTL)) goto vsplopenerr;
						options.flags |= PRT_JDLG | PRT_WDRV;
						break;

					/*
					 * printer language or left margin
					 */
					case 'L':
						if (testSplopnOption((UCHAR*)"LEFTMARGIN", &adr[i1 - 1], i1 - 1))
						{
							i1 += 9;
							getDoubleSplopnOption((UCHAR*)"Leftmargin", i1, &adr[i1], &options.margins.left_margin);
						}
						else {
							if (language[0] || i1 == lp || adr[i1] != '=') dbcerrinfo(411, (UCHAR*)"Bad Language option", -1);
							for (i1++, i2 = 0; i2 < (INT) (sizeof(language) - 1) && i1 < lp && adr[i1] != ' ' && adr[i1] != ',' && adr[i1] != '('; i1++)
								language[i2++] = toupper(adr[i1]);
							if (i1 < lp && adr[i1] == '(') {
								language[i2++] = '(';
								for (i1++; i2 < (INT) (sizeof(language) - 1) && i1 < lp && adr[i1] != ')'; i1++) if (adr[i1] != ' ')
									language[i2++] = toupper(adr[i1]);
								if (i1 == lp || adr[i1] != ')') dbcerrinfo(411, (UCHAR*)"Bad Language option", -1);
								language[i2++] = ')';
								i1++;
							}
							language[i2] = '\0';
						}
						break;

					/*
					 * NOTE: map file NO LONGER SUPPORTED, MUST USE dbcdx.print.translate
					 * So Margin is only possible option
					 */
					case 'M':
						if (testSplopnOption((UCHAR*)"MARGIN", &adr[i1 - 1], i1 - 1))
						{
							i1 += 5;
							getDoubleSplopnOption((UCHAR*)"Margin", i1, &adr[i1], &options.margins.margin);
						}
						else dbcerrinfo(411, (UCHAR*)"Unknown 'M' option", -1);
						break;

					case 'N':  /* copies */
						if (i1 == lp || adr[i1] != '=') dbcerrinfo(411, (UCHAR*)"Bad 'N' option", -1);
						for (i1++, i2 = 0; i1 < lp && isdigit(adr[i1]); ) i2 = i2 * 10 + adr[i1++] - '0';
						options.copies = i2;
						break;
					case 'O':  /* orientation */
						if (i1 == lp || adr[i1] != '=') dbcerrinfo(411, (UCHAR*)"Bad 'O' option", -1);
						if (options.flags & (PRT_LAND | PRT_PORT)) dbcerrinfo(411, (UCHAR*)"Duplicate 'O' option", -1);
						for (i1++, i2 = 0; i2 < (INT) (sizeof(work) - 1) && i1 < lp && adr[i1] != ' ' && adr[i1] != ','; )
							work[i2++] = toupper(adr[i1++]);
						work[i2] = '\0';
						if (!i2) dbcerrinfo(411, (UCHAR*)"Bad 'O' option", -1);
						if (!strcmp(work, "LANDSCAPE")) options.flags |= PRT_LAND;
						else if (!strcmp(work, "PORTRAIT")) options.flags |= PRT_PORT;
						else dbcerrinfo(411, (UCHAR*)"Unknown 'O' option", -1);
						break;

					case 'P':  /* pipe to queue or spooler */
						options.flags |= PRT_PIPE;
						break;

					case 'Q':  /* append to file */
						options.flags |= PRT_APPD;
						break;

					case 'R':
						if (testSplopnOption((UCHAR*)"RIGHTMARGIN", &adr[i1 - 1], i1 - 1))
						{
							i1 += 10;
							getDoubleSplopnOption((UCHAR*)"Rightmargin", i1, &adr[i1], &options.margins.right_margin);
							break;
						}
						/* If not RightMargin then assume it is 'device control characters' option. */
						if (options.flags & (PRT_COMP | PRT_WDRV)) dbcerrinfo(411, (UCHAR*)"Invalid 'R' option", -1);
						options.flags |= PRT_LCTL | PRT_WRAW;
						break;

					case 'S':  /* page size */
						if (i1 == lp || adr[i1] != '=') dbcerrinfo(411, (UCHAR*)"Bad Paper Size option", -1);
						for (i1++, i2 = 0; i2 < (INT) (sizeof(work) - 1) && i1 < lp && adr[i1] != ','; ) work[i2++] = adr[i1++];
						while (i2 && work[i2 - 1] == ' ') i2--;
						work[i2] = '\0';
						if (!i2) dbcerrinfo(411, (UCHAR*)"Bad Paper Size option", -1);
						strcpy(options.papersize, work);
						break;

					case 'T':
						if (testSplopnOption((UCHAR*)"TOPMARGIN", &adr[i1 - 1], i1 - 1))
						{
							i1 += 8;
							getDoubleSplopnOption((UCHAR*)"Topmargin", i1, &adr[i1], &options.margins.top_margin);
						}
						else dbcerrinfo(411, (UCHAR*)"Unknown 'T' option", -1);
						break;

					case 'W':
						options.flags |= PRT_DPLX;
						break;
					case 'X':  /* no extension */
						options.flags |= PRT_NOEX;
						break;
					case 'Y':  /* document name */
						/* Check for equal sign and at least one character after it */
						if (i1 + 1 < lp && adr[i1] == '=') {
							for (i1++, i2 = 0; i2 < (INT) (sizeof(work) - 1) && i1 < lp && adr[i1] != ','; ) work[i2++] = adr[i1++];
							while (i2 && work[i2 - 1] == ' ') i2--;
							work[i2] = '\0';
						}
						else strcpy(work, getmname(pgmmodule, datamodule, TRUE));
						strcpy(options.docname, work);
						break;
					case 'Z':  /* dpi */
						if (i1 == lp || adr[i1] != '=') dbcerrinfo(411, (UCHAR*)"Bad 'Z' option", -1);
						for (i1++, i2 = 0; i1 < lp && isdigit(adr[i1]); ) i2 = i2 * 10 + adr[i1++] - '0';
						options.dpi = i2;
						break;
					default:
						if (c1 >= '1' && c1 <= '9') newnum = c1 - '0';
				}
			}
		}
	}

	if (options.flags & PRT_CLNT) {
		clntprt[0] = '\0';
		strcat(clntprt, "<prtopen s=\"");
		strcat(clntprt, prtname);
		strcat(clntprt, "\">");
		if (language[0]) {
			strcat(clntprt, "<lang s=\"");
			for (i3 = (INT)strlen(language), i2 = 0; i2 < i3; i2++) language[i2] = tolower(language[i2]);
			strcat(clntprt, language);
			strcat(clntprt, "\"/>");
		}
		if (options.dpi) {
			strcat(clntprt, "<dpi n=\"");
			mscitoa(options.dpi, work);
			strcat(clntprt, work);
			strcat(clntprt, "\"/>");
		}
		if (options.docname[0] != '\0') {
			strcat(clntprt, "<doc s=\"");
			strcat(clntprt, options.docname);
			strcat(clntprt, "\"/>");
		}
		if (options.flags & PRT_NOEX) strcat(clntprt, "<noext/>");
		if (options.flags & PRT_DPLX) strcat(clntprt, "<duplex/>");
		if (options.papersize[0] != '\0' && strlen(options.papersize)) {
			strcat(clntprt, "<paper s=\"");
			strcat(clntprt, options.papersize);
			strcat(clntprt, "\"/>");
		}
		if (options.paperbin[0] != '\0' && strlen(options.paperbin)) {
			strcat(clntprt, "<bin s=\"");
			strcat(clntprt, options.paperbin);
			strcat(clntprt, "\"/>");
		}
		if (options.flags & PRT_APPD) strcat(clntprt, "<append/>");
		if (options.flags & PRT_PIPE) strcat(clntprt, "<pipe/>");
		if (options.flags & PRT_LAND) strcat(clntprt, "<orient s=\"land\"/>");
		else if (options.flags & PRT_PORT) strcat(clntprt, "<orient s=\"port\"/>");
		if (options.copies) {
			strcat(clntprt, "<copy n=\"");
			mscitoa(options.copies, work);
			strcat(clntprt, work);
			strcat(clntprt, "\"/>");
		}
		if (options.flags & PRT_JDLG) strcat(clntprt, "<jdlg/>");
		if (options.flags & PRT_NOEJ) strcat(clntprt, "<noej/>");
		if (options.flags & PRT_LCTL) strcat(clntprt, "<ctrl/>");
		if (options.flags & PRT_COMP) strcat(clntprt, "<comp/>");
		if (anyUserSetPrintMargins(options.margins)) {
			strcat(clntprt, "<margins");
			if (options.margins.margin != INVALID_PRINT_MARGIN) {
				sprintf(work, " m=\"%g\"", options.margins.margin);
				strcat(clntprt, work);
			}
			if (options.margins.top_margin != INVALID_PRINT_MARGIN) {
				sprintf(work, " t=\"%g\"", options.margins.top_margin);
				strcat(clntprt, work);
			}
			if (options.margins.bottom_margin != INVALID_PRINT_MARGIN) {
				sprintf(work, " b=\"%g\"", options.margins.bottom_margin);
				strcat(clntprt, work);
			}
			if (options.margins.left_margin != INVALID_PRINT_MARGIN) {
				sprintf(work, " l=\"%g\"", options.margins.left_margin);
				strcat(clntprt, work);
			}
			if (options.margins.right_margin != INVALID_PRINT_MARGIN) {
				sprintf(work, " r=\"%g\"", options.margins.right_margin);
				strcat(clntprt, work);
			}
			strcat(clntprt, "/>");
		}
	}

	else if (!(options.flags & (PRT_LCTL | PRT_COMP | PRT_WDRV))) {
		if (!language[0] && !prpget("print", "language", NULL, NULL, &ptr, PRP_UPPER)) {
			for (i1 = i2 = 0; ptr[i1]; i1++) if (ptr[i1] != ' ') language[i2++] = ptr[i1];
			language[i2] = '\0';
		}
		if (language[0]) {
			if (!strcmp(language, "PS")) options.flags |= PRT_LPSL;
			else if (!strcmp(language, "PCL")) options.flags |= PRT_LPCL;
			else if (!strcmp(language, "PCL(NORESET)")) options.flags |= PRT_LPCL | PRT_NORE;
			else if (!strcmp(language, "PDF")) options.flags |= PRT_LPDF;
			else if (!strcmp(language, "NATIVE")) options.flags |= PRT_WDRV;
			else if (!strcmp(language, "NONE"))	options.flags |= PRT_WRAW;
			else dbcerrinfo(411, (UCHAR*)"Bad Language option", -1);
			if ((options.flags & PRT_APPD) && (options.flags & (PRT_LPCL | PRT_LPSL | PRT_LPDF)))
				dbcerrinfo(411, (UCHAR*)"Invalid option set, cannot append", -1);
		}
		else if (!(dbcflags & DBCFLAG_CONSOLE)) options.flags |= PRT_WDRV;
	}
	else if (language[0] && ((options.flags & PRT_COMP) ||
		((options.flags & PRT_LCTL) && strcmp(language, "NONE")) ||
		((options.flags & PRT_WDRV) && strcmp(language, "NATIVE")))) goto vsplopenerr;

	if (pfileflg) refnum = davb->refnum;
	else refnum = splrefnum[newnum];
	if (refnum) {
		if ((*spltabptr + refnum - 1)->useclnt) {
			clientput("<prtclose h=", 12);
			mscitoa((*spltabptr + refnum - 1)->prtnum, work);
			clientput(work, -1);
			clientput("/>", 2);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) {
				i1 = PRTERR_CLIENT;
			}
			else {
				e1 = (ELEMENT *) clientelement(); /* assume <r> element */
				if (e1 == NULL) i1 = PRTERR_CLIENT;
				else if (e1->firstattribute != NULL) { /* assume e attribute */
					i1 = printvalue(e1->firstattribute->value);
					if (e1->firstsubelement != NULL && e1->firstsubelement->cdataflag) {
						strcpy(printerrstr, e1->firstsubelement->tag);
					}
					else printerrstr[0] = '\0';
				}
				else i1 = 0;
			}
			clientrelease();
		}
		else {
			i1 = prtclose((*spltabptr + refnum - 1)->prtnum, NULL);
			/* reset memory */
			setpgmdata();
		}
		if (pfileflg) ((DAVB *) findvar(mod, off))->refnum = 0;
		else splrefnum[newnum] = 0;
		(*spltabptr + refnum - 1)->type = 0;
		if (i1) {
			i1 = printerror(i1);
			dbcerrinfo(i1, (UCHAR *) printerrstr, (INT)strlen(printerrstr));
		}
	}
	refnum = getsplrefnum();
	if (options.flags & PRT_CLNT) {
		clientput(clntprt, -1);
		clientput("</prtopen>", 10);
		if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) {
			i1 = PRTERR_CLIENT;
		}
		else {
			e1 = (ELEMENT *) clientelement(); /* assume <r> element */
			if (e1 == NULL) i1 = PRTERR_CLIENT;
			else if (e1->firstattribute != NULL) { /* assume e attribute */
				i1 = printvalue(e1->firstattribute->value);
				if (e1->firstsubelement != NULL && e1->firstsubelement->cdataflag) {
					strcpy(printerrstr, e1->firstsubelement->tag);
				}
				else printerrstr[0] = '\0';
			}
			else {
				if (e1->firstsubelement != NULL && e1->firstsubelement->cdataflag) {
					prtnum = atoi(e1->firstsubelement->tag);
					i1 = 0;
				}
				else i1 = PRTERR_CLIENT;
			}
		}
		clientrelease();
	}
	else {
		i1 = prtopen(prtname, &options, &prtnum);
		/* reset memory */
		setpgmdata();
	}
	if (i1) {
		if ((options.flags & PRT_TEST) && (i1 == PRTERR_INUSE || i1 == PRTERR_EXIST || i1 == PRTERR_OFFLINE || i1 == PRTERR_CANCEL)) {
			dbcflags |= DBCFLAG_OVER;
			return;
		}
		i1 = printerror(i1);
		dbcerrinfo(i1, (UCHAR *) printerrstr, (INT)strlen(printerrstr));
	}
	else if (options.flags & PRT_TEST) dbcflags &= ~DBCFLAG_OVER;
	if (pfileflg) {
		((DAVB *) findvar(mod, off))->refnum = refnum + 1;
		(*spltabptr + refnum)->type = SPL_PFILE;
		(*spltabptr + refnum)->module = mod;
		(*spltabptr + refnum)->offset = off;
	}
	else {
		splnum = newnum;
		splrefnum[splnum] = refnum + 1;
		(*spltabptr + refnum)->type = SPL_INTERNAL;
	}
	(*spltabptr + refnum)->prtnum = prtnum;
	(*spltabptr + refnum)->useclnt = (options.flags & PRT_CLNT) ? TRUE : FALSE;
	return;

vsplopenerr:
	dbcerror(411);
}

void vsplclose()
{
	INT i1, i2, mod, newnum, off, refnum;
	INT pfileflg;
	CHAR c1, clntprt[500], work[200];
	UCHAR *adr;
	DAVB *davb;
	PRTOPTIONS options;
	ELEMENT *e1;

	if (fpicnt) filepi0();  /* cancel any filepi */

	newnum = splnum;
	if (vbcode != 0x3D) adr = getvar(VAR_READ);
	else adr = NULL;
	if (adr != NULL && *adr == DAVB_PFILE) {
		getlastvar(&mod, &off);
		davb = aligndavb(adr, &i1);
		off += i1;
		adr = getvar(VAR_READ);
		pfileflg = TRUE;
	}
	else {
		pfileflg = FALSE;
	}

	clntprt[0] = '\0';
	options.flags = 0;
	options.copies = 1;
	options.submit.prtname[0] = '\0';
	options.docname[0] = '\0';
	options.paperbin[0] = '\0';
	options.papersize[0] = '\0';
	if (adr != NULL && fp) {
		for (i1 = hl + fp - 1, lp += hl; i1 < lp; ) {
			c1 = adr[i1++];
			switch(toupper(c1)) {
				case 'D':
					options.flags |= PRT_REMV;
					if (dbcflags & DBCFLAG_CLIENTINIT) strcat(clntprt, "<del/>");
					break;
				case 'S':
					options.flags |= PRT_SBMT;
					if (dbcflags & DBCFLAG_CLIENTINIT) strcat(clntprt, "<sub/>");
					if (lp - i1 >= 5 && toupper(adr[i1]) == 'U' && toupper(adr[i1 + 1]) == 'B'
						&& toupper(adr[i1 + 2]) == 'M' && toupper(adr[i1 + 3]) == 'I'
						&& toupper(adr[i1 + 4]) == 'T')
					{
						i1 += 5;
						if (i1 != lp && adr[i1++] == '=') {
							if (i1 == lp) goto vsplcloseerr;
							for (i2 = 0; i2 < (INT) (sizeof(options.submit.prtname) - 1) && i1 < lp && adr[i1] != ','; )
								options.submit.prtname[i2++] = adr[i1++];
							while (i2 && options.submit.prtname[i2 - 1] == ' ') i2--;
							if (i2 == 0) goto vsplcloseerr;
							options.submit.prtname[i2] = 0;
							if (dbcflags & DBCFLAG_CLIENTINIT) {
								strcat(clntprt, "<printer s=\">");
								strcat(clntprt, options.submit.prtname);
								strcat(clntprt, "\"/>");
							}
						}
					}
					break;
				default:
					if (c1 > '0' && c1 <= '9') newnum = c1 - '0';
			}
		}
	}

	if (pfileflg) refnum = davb->refnum;
	else refnum = splrefnum[newnum];
	if (newnum == splnum) splnum = 0;
	if (refnum) {
		if ((*spltabptr + refnum - 1)->useclnt) {
			clientput("<prtclose h=", 12);
			mscitoa((*spltabptr + refnum - 1)->prtnum, work);
			clientput(work, -1);
			if (clntprt[0] != '\0') {
				clientput(">", 1);
				clientput(clntprt, -1);
				clientput("</prtclose>", 11);
			}
			else clientput("/>", 2);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) {
				i1 = PRTERR_CLIENT;
			}
			else {
				e1 = (ELEMENT *) clientelement(); /* assume <r> element */
				if (e1 == NULL) i1 = PRTERR_CLIENT;
				else if (e1->firstattribute != NULL) { /* assume e attribute */
					i1 = printvalue(e1->firstattribute->value);
					if (e1->firstsubelement != NULL && e1->firstsubelement->cdataflag) {
						strcpy(printerrstr, e1->firstsubelement->tag);
					}
					else printerrstr[0] = '\0';
				}
				else i1 = 0;
			}
			clientrelease();
		}
		else {
			i1 = prtclose((*spltabptr + refnum - 1)->prtnum, &options);
			if (i1 == PRTERR_BADOPT) i1 = PRTERR_SPLCLOSE;
			/* reset memory */
			setpgmdata();
		}
		if (pfileflg) ((DAVB *) findvar(mod, off))->refnum = 0;
		else splrefnum[newnum] = 0;
		(*spltabptr + refnum - 1)->type = 0;
		if (i1) {
			i1 = printerror(i1);
			dbcerrinfo(i1, (UCHAR *) printerrstr, (INT)strlen(printerrstr));
		}
	}
	return;

vsplcloseerr:
	dbcerror(452);
}

/**
 * Note: For SC use, coordinates are ALWAYS sent ONE-BASED over the wire
 */
void vprint()
{
	INT i1, i2, i3, i4;
	INT listflg, mod, off, prterrnum, prtnum, refnum, psave, rc;
	INT control, ctlangle, ctltype, ctlvalue, fmtmod, fmtoff;
	CHAR language[100], work[200], *ptr;
	UCHAR c1, pfileflg, *adr, *fmtadr;
	DAVB *davb;
	PRTOPTIONS options;
	PIXHANDLE pixhandle;
	ELEMENT *e1;
	ATTRIBUTE *a1;
	static struct COLORS {
		UINT n;
		USHORT r, g, b;
	} Colors[] = {
			{0, 0, 0, 0},				// Black
			{16711680, 0xFF, 0, 0},		// Red
			{65280,    0, 0xFF, 0},		// Green
			{16776960, 0xFF, 0xFF, 0},	// Yellow
			{255,      0, 0, 0xFF},		// Blue
			{16711935, 0xFF, 0, 0xFF},	// Magenta
			{65535,    0, 0xFF, 0xFF},	// Cyan
			{16777215, 0xFF, 0xFF, 0xFF}// White
	};

	if (fpicnt) filepi0();  /* cancel any filepi */

	prterrnum = 0;
	printerrstr[0] = '\0';
	pfileflg = FALSE;
	if (pgm[pcount] <= 0xC1) {
		psave = pcount;
		adr = getvar(VAR_READ);
		if (*adr == DAVB_PFILE) {
			getlastvar(&mod, &off);
			davb = aligndavb(adr, &i1);
			off += i1;
			refnum = davb->refnum;
			pfileflg = TRUE;
		}
		else pcount = psave;
	}
	if (!pfileflg) refnum = splrefnum[splnum];

	if (!refnum) {
		refnum = getsplrefnum();
		printdefname(work, sizeof(work));
		options.flags = PRT_WAIT;
		options.copies = 0;
		options.dpi = 0;
		options.papersize[0] = '\0';
		options.paperbin[0] = '\0';
		strcpy(options.docname, "DB/C");
		language[0] = '\0';
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			if (!prpget("print", "destination", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "client")) {
				options.flags |= PRT_CLNT;
				clientput("<prtopen s=\"", -1);
				clientput(work, -1);
				clientput("\">", -1);
			}
		}
		if (!prpget("print", "language", NULL, NULL, &ptr, PRP_UPPER)) {
			for (i1 = i2 = 0; ptr[i1]; i1++) if (ptr[i1] != ' ') language[i2++] = ptr[i1];
			language[i2] = 0;
			if (options.flags & PRT_CLNT) {
				clientput("<lang s=\"", -1);
				clientput(language, -1);
				clientput("\"/>", -1);
			}
			else {
				if (!strcmp(language, "PS")) options.flags |= PRT_LPSL;
				else if (!strcmp(language, "PCL")) options.flags |= PRT_LPCL;
				else if (!strcmp(language, "PCL(NORESET)")) options.flags |= PRT_LPCL | PRT_NORE;
				else if (!strcmp(language, "PDF")) options.flags |= PRT_LPDF;
				else if (!strcmp(language, "NATIVE")) options.flags |= PRT_WDRV;
				else if (!strcmp(language, "NONE")) options.flags |= PRT_WRAW;
				else dbcerror(411);
			}
		}
		else if (!(dbcflags & DBCFLAG_CONSOLE)) options.flags |= PRT_WDRV;
		if (options.flags & PRT_CLNT) {
			clientput("</prtopen>", 10);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) {
				i1 = PRTERR_CLIENT;
			}
			else {
				e1 = (ELEMENT *) clientelement(); /* assume <r> element */
				if (e1 == NULL) i1 = PRTERR_CLIENT;
				else if (e1->firstattribute != NULL) { /* assume e attribute */
					i1 = printvalue(e1->firstattribute->value);
					if (e1->firstsubelement != NULL && e1->firstsubelement->cdataflag) {
						strcpy(printerrstr, e1->firstsubelement->tag);
					}
					else printerrstr[0] = '\0';
				}
				else {
					if (e1->firstsubelement != NULL && e1->firstsubelement->cdataflag) {
						prtnum = atoi(e1->firstsubelement->tag);
						i1 = 0;
					}
					else i1 = PRTERR_CLIENT;
				}
			}
			clientrelease();
		}
		else {
			i1 = prtopen(work, &options, &prtnum);
			/* reset memory */
			setpgmdata();
		}
		if (i1) {
			prterrnum = printerror(i1);
			goto printerr1;
		}
		if (pfileflg) {
			((DAVB *) findvar(mod, off))->refnum = refnum + 1;
			(*spltabptr + refnum)->type = SPL_PFILE;
			(*spltabptr + refnum)->module = mod;
			(*spltabptr + refnum)->offset = off;
		}
		else {
			splrefnum[splnum] = refnum + 1;
			(*spltabptr + refnum)->type = SPL_INTERNAL;
		}
		(*spltabptr + refnum)->prtnum = prtnum;
		(*spltabptr + refnum)->useclnt = (options.flags & PRT_CLNT) ? TRUE : FALSE;
		refnum++;
	}

	prtnum = (*spltabptr + refnum - 1)->prtnum;
	if ((*spltabptr + refnum - 1)->useclnt) {
		if (!((*spltabptr + refnum - 1)->flags & PRTFLAG_ALLOC)) {
			sprintf(work, "<prtalloc h=\"%d\"/>", prtnum);
			clientput(work, -1);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) {
				i1 = PRTERR_CLIENT;
			}
			else {
				e1 = (ELEMENT *) clientelement(); /* assume <r> element */
				if (e1 == NULL) i1 = PRTERR_CLIENT;
				else if (e1->firstattribute != NULL) { /* assume e attribute */
					if (e1->firstsubelement != NULL && e1->firstsubelement->cdataflag) {
						strcpy(printerrstr, e1->firstsubelement->tag);
					}
					else printerrstr[0] = '\0';
					i1 = printvalue(e1->firstattribute->value);
					clientrelease();
					dbcerrinfo(printerror(i1), (UCHAR *) printerrstr, (INT)strlen(printerrstr));
				}
				else i1 = 0;
			}
			clientrelease();
			(*spltabptr + refnum - 1)->flags |= PRTFLAG_ALLOC;
		}
		else i1 = 0;
	}
	else {
		i1 = prtalloc(prtnum, PRT_WAIT);
		/* reset memory */
		setpgmdata();
	}
	if (i1) prterrnum = printerror(i1);

	if ((*spltabptr + refnum - 1)->useclnt) {
		sprintf(work, "<prttext h=\"%d\">", prtnum);
		clientput(work, -1);
	}

printerr1:
	listflg = LIST_READ | LIST_PROG | LIST_NUM1;
	control = ctltype = ctlvalue = ctlangle = 0;
	for ( ; ; ) {
		if ((adr = getlist(listflg)) != NULL) {  /* it's a variable */
			if (vartype & (TYPE_LIST | TYPE_ARRAY)) {  /* list or array variable */
				listflg = (listflg & ~LIST_PROG) | LIST_LIST | LIST_ARRAY;
				continue;
			}
			if (prterrnum) continue;
			if (*adr == 0xAB) {  /* image */
				adr = (UCHAR *) aligndavb(adr, NULL);
				if (((DAVB *) adr)->refnum) {
					if ((*spltabptr + refnum - 1)->useclnt) {
						/* If printing at the client, assume the image there is current, no need to xmit it */
						clientput("<dimage image=\"", -1);
						strcpy(work, refnumtoimagename(((DAVB *) adr)->refnum));
						clientput(work, -1);
						clientput("\"/>", -1);
					}
					else {
						pixhandle = refnumtopixhandle(((DAVB *) adr)->refnum);
						if (dbcflags & DBCFLAG_CLIENTINIT) {
							if (!prpget("client", "imageuse", NULL, NULL, (CHAR**)&ptr, PRP_LOWER)
									&& !strcmp((CHAR*)ptr, "serveronly")) {
								goto skipimagefetch;
							}
							/* retrieve image from client since it may have been modified */
							clientput("<getimagebits image=", -1);
							strcpy(work, refnumtoimagename(((DAVB *) adr)->refnum));
							clientput(work, -1);
							clientput("/>", 2);
							if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
							e1 = (ELEMENT *) clientelement();
							if (e1 != NULL && !strcmp(e1->tag, "image")) {
								a1 = e1->firstattribute;
								while (a1 != NULL) {
									if (!memcmp(a1->tag, "bits", 4)) {
										i1 = decodeimage(pixhandle, (UCHAR *) a1->value, (INT)strlen(a1->value));
										if (i1 == RC_NO_MEM) {
											prterrnum = printerror(PRTERR_NOMEM);
										}
									}
									a1 = a1->nextattribute;
								}
							}
							clientrelease();
						}
						skipimagefetch:
						i1 = prtimage(prtnum, (void *) pixhandle);
						/* reset memory */
						setpgmdata();
						if (i1) {
							prterrnum = printerror(i1);
						}
					}
				}
			}
			else {  /* variable */
				if (control & PRT_FM) {
					control &= ~PRT_FM;
					fmtadr = findvar(fmtmod, fmtoff);
					work[0] = work[1] = '\0';
					work[2] = 0x7F;
					i1 = dbcflags & DBCFLAG_FLAGS;
					vformat(adr, fmtadr, (UCHAR *) work);
					dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1;
					work[2] = work[1];
					scanvar((UCHAR *) work);
					vartype = TYPE_CHARLIT;
					adr = (UCHAR *) work;
				}
				if (vartype & TYPE_NUMVAR) {  /* numeric variable */
					if ((vartype & (TYPE_INT | TYPE_FLOAT | TYPE_NULL)) || (control & (PRT_ZF | PRT_ZS))) {
						nvmkform(adr, (UCHAR *) work);
						adr = (UCHAR *) work;
					}
					if (control & PRT_ZF) {
						for (i1 = 2; i1 < lp + 1; i1++) {
							if (adr[i1] == '-') {
								if (adr[1] == ' ') {
									adr[1] = '-';
									adr[i1] = '0';
								}
							}
						}
						for (i1 = 1; i1 < lp + 1; i1++) if (adr[i1] == ' ') adr[i1] = '0';
					}
					if (control & PRT_ZS) {
						for (i1 = 1; (i1 < lp + 1) && (adr[i1] == ' ' || adr[i1] == '.' || adr[i1] == '0'); i1++);
						if (i1 == lp + 1) memset(adr + 1, ' ', lp);
					}
				}
				else if (fp == 0) lp = 0;
				if (lp) {
					if (!(control & PRT_LL)) {
						adr += hl;
						i1 = lp;
					}
					else {
						adr += hl + fp - 1;
						i1 = lp - fp + 1;
					}
				}
				else i1 = 0;
				if ((vartype & TYPE_CHAR) && !(control & (PRT_SL | PRT_LL)) && pl > lp) i2 = pl - lp;
				else i2 = 0;
				if ((*spltabptr + refnum - 1)->useclnt) {
					clientput("<d", 2);
					if (ctltype & PRT_TEXT_RIGHT) clientput(" rj=\"y\"", -1);
					if (ctltype & PRT_TEXT_CENTER) {
						sprintf(work, " cj=\"%d\"", ctlvalue);
						clientput(work, -1);
					}
					if (ctltype & PRT_TEXT_ANGLE) {
						sprintf(work, " ang=\"%d\"", ctlangle);
						clientput(work, -1);
					}
					if (i2) {
						clientput(" b=", 3);
						mscitoa(i2, work);
						clientput(work, -1);
					}
					if (i1) {
						clientput(">", 1);
						clientputdata((CHAR *) adr, i1);
						clientput("</d>", 4);
					}
					else clientput("/>", 2);
				}
				else {
					rc = prttext(prtnum, adr, 0, i1, i2, ctltype, ctlvalue, ctlangle);
					if (rc) prterrnum = printerror(rc);
				}
			}
			if (listflg & LIST_PROG) {
				if (!(vartype & TYPE_LITERAL)) control &= ~(PRT_ZF | PRT_ZS);
				ctltype = 0;
			}
			continue;
		}
		if (!(listflg & LIST_PROG)) {
			listflg = (listflg & ~(LIST_LIST | LIST_ARRAY)) | LIST_PROG;
			control &= ~(PRT_ZF | PRT_ZS);
			ctltype = 0;
			continue;
		}

		rc = 0;
		c1 = pgm[pcount - 1];
		switch (c1) {
		case 0xD2:  /* *ZF */
			control |= PRT_ZF;
			break;
		case 0xD3:  /* *- & *PL */
			control &= ~(PRT_SL | PRT_LL);
			break;

		case 0xE0: /* black */
		case 0xE1: /* red */
		case 0xE2: /* green */
		case 0xE3: /* yellow */
		case 0xE4: /* blue */
		case 0xE5: /* magenta */
		case 0xE6: /* cyan */
		case 0xE7: /* white */
			if ((*spltabptr + refnum - 1)->useclnt) {
				struct COLORS *clr = &Colors[c1 - 0xE0];
				sprintf(work, "<color n=\"%u\" r=\"%hu\" g=\"%hu\" b=\"%hu\"/>", clr->n,
						clr->r, clr->g, clr->b);
				clientput(work, -1);
			}
			else {
				if (!prterrnum) rc = prtcolor(prtnum, (INT)Colors[c1 - 0xE0].n);
			}
			break;

		case 0xE8:  /* *+ & *SL */
			control = (UCHAR)((control & ~PRT_LL) | PRT_SL);
			break;
		case 0xEB:  /* *L */
			if ((*spltabptr + refnum - 1)->useclnt) {
				clientput("<l/>", 4);
			}
			else {
				if (!prterrnum) rc = prtlf(prtnum, 1);
			}
			break;
		case 0xEC:  /* *C */
			if ((*spltabptr + refnum - 1)->useclnt) {
				clientput("<c/>", 4);
			}
			else {
				if (!prterrnum) rc = prtcr(prtnum, 1);
			}
			break;
		case 0xE9:  /* *N */
		case 0xFF:  /* regular end of list */
			if ((*spltabptr + refnum - 1)->useclnt) {
				clientput("<n/>", 4);
			}
			else {
				if (!prterrnum) rc = prtnl(prtnum, 1);
			}
			break;
		case 0xEE:  /* *F */
			if ((*spltabptr + refnum - 1)->useclnt) {
				clientput("<f/>", 4);
			}
			else {
				if (!prterrnum) rc = prtff(prtnum, 1);
			}
			break;
		case 0xF1:  /* *LL */
			control = (UCHAR)((control & ~PRT_SL) | PRT_LL);
			break;
#if 0
#if DX_MAJOR_VERSION >= 18
		case 0xF5:	/* *h */
			i1 = printgetvalue();
			if ((*spltabptr + refnum - 1)->useclnt) {
				sprintf(work, "<h n=\"%d\"/>", i1);
				clientput(work, -1);
			}
			else {
				if (!prterrnum) rc = prthorz(prtnum, i1 - 1);
			}
			break;
		case 0xF6:	/* *ha */
			i1 = printgetvalue();
			if ((*spltabptr + refnum - 1)->useclnt) {
				sprintf(work, "<ha n=\"%d\"/>", i1);
				clientput(work, -1);
			}
			else {
				if (!prterrnum) rc = prthorzadj(prtnum, i1);
			}
			break;
		case 0xF7:	/* *v */
			i1 = printgetvalue();
			if ((*spltabptr + refnum - 1)->useclnt) {
				sprintf(work, "<v n=\"%d\"/>", i1);
				clientput(work, -1);
			}
			else {
				if (!prterrnum) rc = prtvert(prtnum, i1 - 1);
			}
			break;
		case 0xF8:	/* *va */
			i1 = printgetvalue();
			if ((*spltabptr + refnum - 1)->useclnt) {
				sprintf(work, "<va n=\"%d\"/>", i1);
				clientput(work, -1);
			}
			else {
				if (!prterrnum) rc = prtvertadj(prtnum, i1);
			}
			break;
#endif
#endif
		case 0xF9:  /* *FLUSH */
			if ((*spltabptr + refnum - 1)->useclnt) {
				clientput("<flush/>", 8);
			}
			else {
				if (!prterrnum) rc = prtflush(prtnum);
			}
			break;
		case 0xFA:  /* 2 byte opcode (250) */
			c1 = getbyte();
			switch (c1) {
			case 0x09:  /* *RPTCHAR=<n> */
				adr = getvar(VAR_READ);
				c1 = adr[fp + hl - 1];
				i1 = printgetvalue();
				if ((*spltabptr + refnum - 1)->useclnt) {
					clientput("<rpt n=\"", -1);
					mscitoa(i1, work);
					clientput(work, -1);
					clientput("\"", 1);
					if (ctltype & PRT_TEXT_RIGHT) clientput(" rj=\"y\"", -1);
					if (ctltype & PRT_TEXT_CENTER) {
						sprintf(work, " cj=\"%d\"", ctlvalue);
						clientput(work, -1);
					}
					if (ctltype & PRT_TEXT_ANGLE) {
						sprintf(work, " ang=\"%d\"", ctlangle);
						clientput(work, -1);
					}
					clientput(">", 1);
					mscitoa((INT) c1, work);
					clientput(work, -1);
					clientput("</rpt>", 6);
				}
				else {
					if (!prterrnum) rc = prttext(prtnum, NULL, c1, i1, 0, ctltype, ctlvalue, ctlangle);
				}
				ctltype = 0;
				break;
			case 0x2E:  /* *N=<n> */
				i1 = printgetvalue();
				if ((*spltabptr + refnum - 1)->useclnt) {
					clientput("<n n=", 5);
					mscitoa(i1, work);
					clientput(work, -1);
					clientput("/>", 2);
				}
				else {
					if (!prterrnum) rc = prtnl(prtnum, i1);
				}
				break;
			case 0x31:  /* *FORMAT=<var> */
				getvar(VAR_READ);
				getlastvar(&fmtmod, &fmtoff);  /* memory may move before using */
				control |= PRT_FM;
				break;

				/**
				 * Assumes that the color number, according to our doc, is in BGR format
				 */
			case 0x34: /* *COLOR=<n>  (decimal 52)*/
				i1 = printgetvalue();
				if ((*spltabptr + refnum - 1)->useclnt) {
					clientput("<color n=\"", -1);
					mscitoa(i1, work);
					clientput(work, -1);
					/**
					 * Added 26 MAY 13 for use by newer SCs to
					 * GET IT RIGHT
					 *
					 * Honor config option dbcdx.print.color=RGB
					 */
					if ((*spltabptr + refnum - 1)->colorFormat == PRINT_24BITCOLOR_FORMAT_RGB) {
						sprintf(work, "\" b=\"%d\" g=\"%d\" r=\"%d\"",
								i1 & 0x0000FF, (i1 & 0x00FF00) >> 8, (UINT)(i1 & 0xFF0000) >> 16);
					}
					else if ((*spltabptr + refnum - 1)->colorFormat == PRINT_24BITCOLOR_FORMAT_BGR) {
						sprintf(work, "\" r=\"%d\" g=\"%d\" b=\"%d\"",
								i1 & 0x0000FF, (i1 & 0x00FF00) >> 8, (UINT)(i1 & 0xFF0000) >> 16);
					}
					else {
						assert(0);
					}
					clientput(work, -1);
					clientput("/>", 2);
				}
				else {
					if (!prterrnum) {
						INT colorRGB;
						/**
						 * Honor config option dbcdx.print.color=RGB
						 */
						if ((*spltabptr + refnum - 1)->colorFormat == PRINT_24BITCOLOR_FORMAT_RGB) {
							colorRGB = i1;
						}
						else if ((*spltabptr + refnum - 1)->colorFormat == PRINT_24BITCOLOR_FORMAT_BGR) {
							colorRGB = ((i1 & 0x0000FF) << 16) | (i1 & 0x00FF00) | (i1 >> 16);
						}
						else {
							assert(0);
						}
						rc = prtcolor(prtnum, colorRGB);
					}
				}
				break;
			case 0x38:  /* *ZS */
				control |= PRT_ZS;
				break;
			case 0x3E:  /* *F=<n> */
				i1 = printgetvalue();
				if ((*spltabptr + refnum - 1)->useclnt) {
					sprintf(work, "<f n=\"%d\"/>", i1);
					clientput(work, -1);
				}
				else {
					if (!prterrnum) rc = prtff(prtnum, i1);
				}
				break;
			case 0x3F:  /* *C=<n> */
				i1 = printgetvalue();
				if ((*spltabptr + refnum - 1)->useclnt) {
					sprintf(work, "<c n=\"%d\"/>", i1);
					clientput(work, -1);
				}
				else {
					if (!prterrnum) rc = prtcr(prtnum, i1);
				}
				break;
			case 0x40:  /* *L=<n> */
				i1 = printgetvalue();
				if ((*spltabptr + refnum - 1)->useclnt) {
					sprintf(work, "<l n=\"%d\"/>", i1);
					clientput(work, -1);
				}
				else {
					if (!prterrnum) rc = prtlf(prtnum, i1);
				}
				break;
			case 0x43:  /* *FONT */
				cvtoname(getvar(VAR_READ));
				if ((*spltabptr + refnum - 1)->useclnt) {
					clientput("<font s=\"", 9);
					clientputdata(name, -1);
					clientput("\"/>", 3);
				}
				else {
					if (!prterrnum) rc = prtfont(prtnum, name);
				}
				break;
			case 0x44:  /* *LINE */
				i1 = printgetvalue();
				i2 = printgetvalue();
				if ((*spltabptr + refnum - 1)->useclnt) {
					sprintf(work, "<line h=\"%d\" v=\"%d\"/>", i1, i2);
					clientput(work, -1);
				}
				else {
					if (!prterrnum) rc = prtline(prtnum, i1 - 1, i2 - 1);
				}
				break;
			case 0x45:  /* *LINEWIDTH */
				i1 = printgetvalue();
				if ((*spltabptr + refnum - 1)->useclnt) {
					sprintf(work, "<lw n=\"%d\"/>", i1);
					clientput(work, -1);
				}
				else {
					if (!prterrnum) rc = prtlinewidth(prtnum, i1);
				}
				break;
			case 0x46:  /* *RJ */
				ctltype &= ~PRT_TEXT_CENTER;
				ctltype |= PRT_TEXT_RIGHT;
				break;
			case 0x47:  /* *CJ */
				ctltype &= ~PRT_TEXT_RIGHT;
				ctltype |= PRT_TEXT_CENTER;
				ctlvalue = printgetvalue();
				break;
			case 0x48:  /* *TEXTANGLE (72) */
				ctltype |= PRT_TEXT_ANGLE;
				ctlangle = printgetvalue();
				break;
			case 0x7F:  /* tab */
				i1 = (INT) gethhll();
				if (i1 >= 1) {
					if ((*spltabptr + refnum - 1)->useclnt) {
						sprintf(work, "<t n=\"%d\"/>", i1 - 1);
						clientput(work, -1);
					}
					else {
						if (!prterrnum) rc = prttab(prtnum, i1 - 1);
					}
				}
				break;
			case 151:	/* RECTANGLE */
				i1 = printgetvalue();
				i2 = printgetvalue();
				if ((*spltabptr + refnum - 1)->useclnt) {
					sprintf(work, "<rect x=\"%d\" y=\"%d\"/>", i1, i2);
					clientput(work, -1);
				}
				else {
					if (!prterrnum) rc = prtrect(prtnum, i1 - 1, i2 - 1);
				}
				break;
			case 152:	/* BOX */
				i1 = printgetvalue();
				i2 = printgetvalue();
				if ((*spltabptr + refnum - 1)->useclnt) {
					sprintf(work, "<box x=\"%d\" y=\"%d\"/>", i1, i2);
					clientput(work, -1);
				}
				else {
					if (!prterrnum) rc = prtbox(prtnum, i1 - 1, i2 - 1);
				}
				break;
			case 153:	/* CIRCLE */
				i1 = printgetvalue();
				if ((*spltabptr + refnum - 1)->useclnt) {
					sprintf(work, "<circ r=\"%d\"/>", i1);
					clientput(work, -1);
				}
				else {
					if (!prterrnum) rc = prtcircle(prtnum, i1);
				}
				break;
			case 154:	/* BIGDOT */
				i1 = printgetvalue();
				if ((*spltabptr + refnum - 1)->useclnt) {
					sprintf(work, "<bigd r=\"%d\"/>", i1);
					clientput(work, -1);
				}
				else {
					if (!prterrnum) rc = prtbigdot(prtnum, i1);
				}
				break;
			case 155:	/* TRIANGLE */
				i1 = printgetvalue();
				i2 = printgetvalue();
				i3 = printgetvalue();
				i4 = printgetvalue();
				if ((*spltabptr + refnum - 1)->useclnt) {
					sprintf(work, "<tria x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\"/>", i1, i2, i3, i4);
					clientput(work, -1);
				}
				else {
					if (!prterrnum) rc = prttriangle(prtnum, i1, i2, i3, i4);
				}
				break;
			case 156:  /* *FB */
				cvtoname(getvar(VAR_READ));
				if ((*spltabptr + refnum - 1)->useclnt) {
					clientput("<fb s=\"", -1);
					clientputdata(name, -1);
					clientput("\"/>", 3);
				}
				else {
					if (!prterrnum) rc = prtffbin(prtnum, name);
				}
				break;
			}
			break;
		case 0xFB:  /* octal character (also used for hex and decimal chars) */
			if ((*spltabptr + refnum - 1)->useclnt) {
				clientput("<rpt n=\"1\"", -1);
				if (ctltype & PRT_TEXT_RIGHT) clientput(" rj=\"y\"", -1);
				if (ctltype & PRT_TEXT_CENTER) {
					sprintf(work, " cj=\"%d\"", ctlvalue);
					clientput(work, -1);
				}
				if (ctltype & PRT_TEXT_ANGLE) {
					sprintf(work, " ang=\"%d\"", ctlangle);
					clientput(work, -1);
				}
				clientput(">", 1);
				mscitoa((INT) getbyte(), work);
				clientput(work, -1);
				clientput("</rpt>", 6);
			}
			else {
				if (!prterrnum) rc = prttext(prtnum, NULL, getbyte(), 1, 0, ctltype, ctlvalue, ctlangle);
			}
			ctltype = 0;
			break;
		case 0xFC:  /* tab */
			i1 = getbyte();
			if (i1 >= 1) {
				if ((*spltabptr + refnum - 1)->useclnt) {
					sprintf(work, "<t n=\"%d\"/>", i1 - 1);
					clientput(work, -1);
				}
				else {
					if (!prterrnum) rc = prttab(prtnum, i1 - 1);
				}
			}
			break;
		case 0xFD:  /* tab */
			i1 = nvtoi(getvar(VAR_READ));
			if (i1 >= 1) {
				if ((*spltabptr + refnum - 1)->useclnt) {
					sprintf(work, "<t n=\"%d\"/>", i1 - 1);
					clientput(work, -1);
				}
				else {
					if (!prterrnum) rc = prttab(prtnum, i1 - 1);
				}
			}
			break;
		default:
			if (c1 >= 0xC6 && c1 <= 0xC9) {  /* *P */
				if (c1 == 0xC6 || c1 == 0xC8) i1 = getbyte();
				else i1 = printgetvalue();
				if (c1 == 0xC6 || c1 == 0xC9) i2 = getbyte();
				else i2 = printgetvalue();
				if ((*spltabptr + refnum - 1)->useclnt) {
					sprintf(work, "<p h=\"%d\" v=\"%d\"/>", i1, i2);
					clientput(work, -1);
				}
				else {
					if (!prterrnum) rc = prtpos(prtnum, i1 - 1, i2 - 1);
				}
			}
			break;
		}
		if (!(*spltabptr + refnum - 1)->useclnt) {
/*** CODE: DETERMINE WHICH ROUTINES CHANGE MEMORY ***/
			/* reset memory */
			setpgmdata();
			if (rc) prterrnum = printerror(rc);
		}
		if (c1 >= 0xFE) break;  /* end of print statement (254 = ;  255 = regular) */
	}

	if ((*spltabptr + refnum - 1)->useclnt) {
		clientput("</prttext>", 10);
		if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) {
			i1 = PRTERR_CLIENT;
		}
		else {
			e1 = (ELEMENT *) clientelement(); /* assume <r> element */
			if (e1 == NULL) i1 = PRTERR_CLIENT;
			else if (e1->firstattribute != NULL) { /* assume e attribute */
				i1 = printvalue(e1->firstattribute->value);
				if (e1->firstsubelement != NULL && e1->firstsubelement->cdataflag) {
					strcpy(printerrstr, e1->firstsubelement->tag);
				}
				else printerrstr[0] = '\0';
			}
			else i1 = 0;
		}
		clientrelease();
		if (i1) prterrnum = printerror(i1);
	}

	if (!(*spltabptr + refnum - 1)->useclnt) {
		if ((dbcprtflags & DBCPRTFLAG_AUTOFLUSH) && prtisdevice(prtnum) && !prterrnum) {
			rc = prtflush(prtnum);
			if (rc) prterrnum = printerror(rc);
		}
	}
	if (prterrnum) {
		dbcerrinfo(prterrnum, (UCHAR *) printerrstr, (INT)strlen(printerrstr));
	}
}

static INT printgetvalue()  /* convert numeric variable or constant value to int */
{
	if (pgm[pcount] < 0xFC) return(nvtoi(getvar(VAR_READ)));
	if (pgm[pcount] == 0xFF) {
		pcount++;
		return(pgm[pcount++]);
	}
	if (pgm[pcount] == 0xFE) {
		pcount++;
		return(0 - pgm[pcount++]);
	}
	pcount++;
	return(gethhll());
}

void vrelease()
{
	INT i1, i2, prtnum, refnum;
	CHAR language[100], prtname[200], work[200], *ptr;
	PRTOPTIONS options;
	ELEMENT *e1;

	if (fpicnt) filepi0();  /* cancel any filepi */

	refnum = splrefnum[splnum];
	if (!refnum) {
		printdefname(prtname, sizeof(prtname));
		i1 = prtnameisdevice(prtname);
		/* reset memory */
		setpgmdata();
		if (!i1) return;
		options.flags = PRT_TEST;
		options.copies = 0;
		options.dpi = 0;
		options.papersize[0] = '\0';
		options.paperbin[0] = '\0';
		strcpy(options.docname, "DB/C");
		language[0] = '\0';
/*** CODE: LANGUAGE CODE PROBABLY NOT NEEDED, BUT SINCE THIS IS GOING TO CHANGE ***/
/***       FOR DX 12, I PUT IT IN TO BE CONSISTENT ***/
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			if (!prpget("print", "destination", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "client")) {
				options.flags |= PRT_CLNT;
				clientput("<prtopen s=\"", -1);
				clientput(prtname, -1);
				clientput("\">", -1);
			}
		}
		if (!prpget("print", "language", NULL, NULL, &ptr, PRP_UPPER)) {
			for (i1 = i2 = 0; ptr[i1]; i1++) if (ptr[i1] != ' ') language[i2++] = ptr[i1];
			language[i2] = '\0';
		}
		if (language[0]) {
			if (options.flags & PRT_CLNT) {
				clientput("<lang s=", -1);
				clientput(language, -1);
				clientput("/>", -1);
			}
			else {
				if (!strcmp(language, "PS")) options.flags |= PRT_LPSL;
				else if (!strcmp(language, "PCL")) options.flags |= PRT_LPCL;
				else if (!strcmp(language, "PCL(NORESET)")) options.flags |= PRT_LPCL | PRT_NORE;
				else if (!strcmp(language, "PDF")) options.flags |= PRT_LPDF;
				else if (!strcmp(language, "NATIVE")) options.flags |= PRT_WDRV;
				else if (!strcmp(language, "NONE")) options.flags |= PRT_WRAW;
				else dbcerror(411);
			}
		}
		else if (!(dbcflags & DBCFLAG_CONSOLE)) options.flags |= PRT_WDRV;
		if (options.flags & PRT_CLNT) {
			clientput("</prtopen>", 10);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) {
				i1 = PRTERR_CLIENT;
			}
			else {
				e1 = (ELEMENT *) clientelement(); /* assume <r> element */
				if (e1 == NULL) i1 = PRTERR_CLIENT;
				else if (e1->firstattribute != NULL) { /* assume e attribute */
					i1 = printvalue(e1->firstattribute->value);
					if (e1->firstsubelement != NULL && e1->firstsubelement->cdataflag) {
						strcpy(printerrstr, e1->firstsubelement->tag);
					}
					else printerrstr[0] = '\0';
				}
				else {
					if (e1->firstsubelement != NULL && e1->firstsubelement->cdataflag) {
						prtnum = atoi(e1->firstsubelement->tag);
						i1 = 0;
					}
					else i1 = PRTERR_CLIENT;
				}
			}
			clientrelease();
		}
		else {
			i1 = prtopen(prtname, &options, &prtnum);
			/* reset memory */
			setpgmdata();
		}
		if (i1) {
			if (i1 == PRTERR_INUSE || i1 == PRTERR_EXIST || i1 == PRTERR_OFFLINE || i1 == PRTERR_CANCEL) {
				dbcflags |= DBCFLAG_OVER;
				return;
			}
			i1 = printerror(i1);
			dbcerrinfo(i1, (UCHAR *) printerrstr, (INT)strlen(printerrstr));
		}
		if (options.flags & PRT_CLNT) {
			clientput("<prtclose h=", 12);
			mscitoa(prtnum, work);
			clientput(work, -1);
			clientput("/>", 2);
			clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0);
			clientrelease();
		}
		else {
			prtclose(prtnum, NULL);
			/* reset memory */
			setpgmdata();
		}
		dbcflags &= ~DBCFLAG_OVER;
		return;
	}

	prtnum = (*spltabptr + refnum - 1)->prtnum;
	if ((*spltabptr + refnum - 1)->useclnt) {
		clientput("<prtrel h=", -1);
		mscitoa(prtnum, work);
		clientput(work, -1);
		clientput("></prtrel>", -1);
/*** CODE: MIGHT NEED TO SET OVER FLAG HERE ***/
		if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) {
			i1 = PRTERR_CLIENT;
		}
		else {
			e1 = (ELEMENT *) clientelement(); /* assume <r> element */
			if (e1 == NULL) i1 = PRTERR_CLIENT;
			else if (e1->firstattribute != NULL) { /* assume e attribute */
				i1 = printvalue(e1->firstattribute->value);
				if (e1->firstsubelement != NULL && e1->firstsubelement->cdataflag) {
					strcpy(printerrstr, e1->firstsubelement->tag);
				}
				else printerrstr[0] = '\0';
			}
			else i1 = 0;
		}
		clientrelease();
	}
	else {
		if (!prtisdevice(prtnum)) return;
		i1 = prtalloc(prtnum, 0);  /* get allocation status */
		if (!i1) {
			i1 = prtalloc(prtnum, PRT_TEST);
			/* reset memory */
			setpgmdata();
			if (i1) {
				if (i1 == PRTERR_INUSE || i1 == PRTERR_EXIST || i1 == PRTERR_OFFLINE || i1 == PRTERR_CANCEL) {
					dbcflags |= DBCFLAG_OVER;
					return;
				}
				i1 = printerror(i1);
				dbcerrinfo(i1, (UCHAR *) printerrstr, (INT)strlen(printerrstr));
			}
			dbcflags &= ~DBCFLAG_OVER;
		}
		i1 = prtfree(prtnum);
		/* reset memory */
		setpgmdata();
	}
	if (i1) {
		i1 = printerror(i1);
		dbcerrinfo(i1, (UCHAR *) printerrstr, (INT)strlen(printerrstr));
	}
}

void vsplopt()
{
	INT i1;
	UCHAR *adr;
	ELEMENT *e1;

	adr = getvar(VAR_READ);
	if (!fp) return;

	for (fp += hl - 1, lp += hl; fp < lp; fp++) {
		if (isdigit(adr[fp])) {
			INT newnum = adr[fp] - '0';
			if (splrefnum[newnum]) /* Non-null entry means this spl number is open */
			{
				splnum = newnum;	/* Accept this as the current spool number */
				dbcflags &= ~DBCFLAG_OVER; /* Turn OVER off */
			}
			else {
				dbcflags |= DBCFLAG_OVER; /* Turn OVER on */
			}
			return; /* Stop looking. This option should be used alone */
		}
		else {
			i1 = toupper(adr[fp]);
			if (i1 == 'S') {
				if (dbcflags & DBCFLAG_CLIENTINIT) { /* always show setup on Smart Client side */
					clientput("<prtsetup/>", 11);
					if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) {
						i1 = PRTERR_CLIENT;
					}
					else {
						e1 = (ELEMENT *) clientelement(); /* assume <r> element */
						if (e1 == NULL) i1 = PRTERR_CLIENT;
						else if (e1->firstattribute != NULL) { /* assume e attribute */
							i1 = printvalue(e1->firstattribute->value);
							if (e1->firstsubelement != NULL && e1->firstsubelement->cdataflag) {
								strcpy(printerrstr, e1->firstsubelement->tag);
							}
							else printerrstr[0] = '\0';
						}
						else i1 = 0;
					}
					clientrelease();
				}
				else {
					i1 = prtpagesetup();
					/* reset memory */
					setpgmdata();
				}
				if (i1) {
					if (i1 == PRTERR_CANCEL) {
						dbcflags |= DBCFLAG_OVER;
						return;
					}
					i1 = printerror(i1);
					dbcerrinfo(i1, (UCHAR *) printerrstr, (INT)strlen(printerrstr));
				}
				dbcflags &= ~DBCFLAG_OVER; /* Turn OVER off */
			}
		}
	}
}

void vnoeject()
{
	INT refnum;
/*** CODE: DON'T KNOW IF THIS SHOULD BE JUST IGNORED OR SENT TO PRT.C ***/
	refnum = splrefnum[splnum];
	if (refnum) (*spltabptr + refnum - 1)->noeject = TRUE;
}

void vgetprintinfo(INT flag)
{
	INT i1, i2, mod, off;
	UINT size;
	CHAR *ptr;
	UCHAR *adr1, **pptr;
	ELEMENT *e1;

	dbcflags &= ~(DBCFLAG_LESS | DBCFLAG_EOS);
	if (flag == GETPRINTINFO_PAPERBINS || flag == GETPRINTINFO_PAPERNAMES) {
		dbcflags &= ~DBCFLAG_OVER;
		cvtoname(getvar(VAR_READ));
	}
	adr1 = getvar(VAR_WRITE);
	if (!(vartype & TYPE_ARRAY)) {
		dbcflags |= DBCFLAG_LESS | DBCFLAG_EOS;
		return;
	}
	getlastvar(&mod, &off);  /* save module and offset */

	pptr = memalloc(GETPRINTINFO_MALLOC, MEMFLAGS_ZEROFILL);
	if (pptr == NULL) {
		/* reset memory */
		setpgmdata();
		dbcerror(408);
	}
	size = GETPRINTINFO_MALLOC;
	if ((dbcflags & DBCFLAG_CLIENTINIT) && !prpget("print", "destination", NULL, NULL, &ptr, PRP_LOWER)
			&& !strcmp(ptr, "client")) {
		if (flag == GETPRINTINFO_PRINTERS) clientput("<prtget/>", -1);
		else if (flag == GETPRINTINFO_PAPERBINS) {
			if (name[0]) {
				clientput("<prtbin>", -1);
				clientput(name, -1);
				clientput("</prtbin>", -1);
			}
			else clientput("<prtbin/>", -1);
		}
		else {
			if (name[0]) {
				clientput("<prtpnames>", -1);
				clientput(name, -1);
				clientput("</prtpnames>", -1);
			}
			else clientput("<prtpnames/>", -1);
		}
		i1 = 0;
		if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) {
			i1 = PRTERR_CLIENT;
		}
		else {
			e1 = (ELEMENT *) clientelement();  /* assume <r> element */
			if (e1 == NULL) i1 = PRTERR_CLIENT;
			else if (e1->firstattribute != NULL) {  /* assume e attribute */
				i1 = printvalue(e1->firstattribute->value);
				if (e1->firstsubelement != NULL && e1->firstsubelement->cdataflag) {
					strcpy(printerrstr, e1->firstsubelement->tag);
				}
				else printerrstr[0] = '\0';
			}
			else {
				i2 = 0;
				ptr = (CHAR *) *pptr;
				e1 = e1->firstsubelement;
				while (e1 != NULL) {
					if (e1->firstsubelement == NULL || !e1->firstsubelement->cdataflag) {
						i1 = PRTERR_CLIENT;
						break;
					}
					if ((UINT)(i2 + strlen(e1->firstsubelement->tag) + 1) > size) {
						if (memchange(pptr, size <<= 1, MEMFLAGS_ZEROFILL) < 0) {
							clientrelease();
							memfree(pptr);
							/* reset memory */
							setpgmdata();
							dbcerror(408);
						}
						/* Reset ptr in case the memchange moved stuff around */
						ptr = (CHAR *) *pptr + i2;
					}
					i2 += (INT)(strlen(e1->firstsubelement->tag) + 1);
					memcpy(ptr, e1->firstsubelement->tag, strlen(e1->firstsubelement->tag) + 1);
					ptr = ((CHAR *) *pptr) + i2;

					e1 = e1->nextelement;
				}
			}
		}
		clientrelease();
		/* reset memory */
		setpgmdata();
	}
	else {
		i1 = 0;
		for ( ; ; ) {
			switch (flag) {
			case GETPRINTINFO_PRINTERS:
				i1 = prtgetprinters((CHAR **) pptr, size);
				break;
			case GETPRINTINFO_PAPERBINS:
				i1 = prtgetpaperbins(name, (CHAR **) pptr, size);
				break;
			case GETPRINTINFO_PAPERNAMES:
				i1 = prtgetpapernames(name, (CHAR **) pptr, size);
				break;
			default:
				sprintf(printerrstr, "Internal fail in vgetprintinfo, flag=%#.8x", flag);
				dbcerrinfo(1499, (UCHAR *) printerrstr, (INT)strlen(printerrstr));
			}
			if (i1 != RC_ERROR) break;
			if (memchange(pptr, size <<= 1, 0) < 0) {
				memfree(pptr);
				/* reset memory */
				setpgmdata();
				dbcerror(408);
			}
		}
		/* reset memory */
		setpgmdata();
		if (i1) memfree(pptr);
	}
	if (i1) {
		if (i1 == PRTERR_BADOPT && flag == GETPRINTINFO_PAPERBINS) {
			dbcflags |= DBCFLAG_OVER;
			return;
		}
		i1 = printerror(i1);
		dbcerrinfo(i1, (UCHAR *) printerrstr, (INT)strlen(printerrstr));
	}
	adr1 = findvar(mod, off);
	i1 = 3 + ((*adr1 - 0xA6) << 1);
	size = llhh(adr1 + i1);
	adr1 += i1 + 2;
	for (ptr = (CHAR *) *pptr; *ptr; ptr += i1 + 1) {
		scanvar(adr1);
		if (!(vartype & TYPE_CHAR)) {
			dbcflags |= DBCFLAG_LESS;
			break;
		}
		i1 = (INT)strlen(ptr);
		if (i1 <= pl) pl = i1;
		else dbcflags |= DBCFLAG_EOS;
		memcpy(&adr1[hl], ptr, pl);
		fp = 1;
		lp = pl;
		setfplp(adr1);
		adr1 += size;
	}
	memfree(pptr);

	for ( ; ; ) {  /* clear the rest of the array */
		scanvar(adr1);
		if (!(vartype & TYPE_CHAR)) break;
		fp = lp = 0;
		setfplp(adr1);
		adr1 += size;
	}
}

/* flag an open pfile as common (don't close) */
void prtflag(INT refnum)
{
	(*spltabptr + refnum - 1)->type |= SPL_COMMON;
}

/* close pfiles for a given module. if mod <= 0, close all pfiles. */
/* if mod == 0, clear refnum in corresponding davb */
void prtclosemod(INT mod)
{
	INT i1, newsplhi, flag;
	CHAR *ptr, work[16];
	SPLINFO *spl;

	if (!splmax) return;

	newsplhi = flag = 0;
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		if (!prpget("print", "destination", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "client")) {
			flag |= PRT_CLNT;
		}
	}
	for (i1 = 0, spl = *spltabptr; i1++ < splhi; spl++) {
		if (!spl->type) continue;
		if (mod <= 0 || (spl->type == SPL_PFILE && spl->module == mod)) {  /* close pfile */
			if (((dbcflags & DBCFLAG_CLIENTINIT) && spl->useclnt) || flag & PRT_CLNT) {
				clientput("<prtclose h=", 12);
				mscitoa(spl->prtnum, work);
				clientput(work, -1);
				clientput("/>", 2);
				clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0);
				clientrelease();
			}
			else {
				prtclose(spl->prtnum, NULL);
				/* reset memory */
				setpgmdata();
			}
			/* clear refnum in davb */
			if (mod == 0 && spl->type == SPL_PFILE) ((DAVB *) findvar(spl->module, spl->offset))->refnum = 0;
			spl->type = 0;
		}
		else {
			spl->type &= ~SPL_COMMON;  /* reset common flag */
			newsplhi = i1;
		}
	}
	splhi = newsplhi;
	if (mod == -1) {
		if (flag & PRT_CLNT) {
			clientput("<prtexit/>", -1);
			clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0);
			clientrelease();
		}
		else prtexit();
	}
	else if (mod == 0) memset(splrefnum, 0, sizeof(splrefnum));
}

/* renmame old module number to new module number */
void prtrenmod(INT oldmod, INT newmod)
{
	INT i1;
	SPLINFO *spl;

	if (!splmax) return;

	for (i1 = 0, spl = *spltabptr; i1++ < splhi; spl++)
		if (spl->type == SPL_PFILE && spl->module == oldmod) spl->module = newmod;
}

static void printdefname(_Out_ CHAR *prtname, INT cbPrtname)
{
	CHAR *ptr;

	/* get the default printer name */
	if (!prpget("print", "device", NULL, NULL, &ptr, 0) && *ptr) {
		strncpy(prtname, ptr, cbPrtname);
		prtname[cbPrtname - 1] = '\0';
	}
	else {
		prtdefname(prtname, cbPrtname);
		/* reset memory */
		setpgmdata();
	}
}

static INT getsplrefnum()
{
	INT i1, refnum;
	CHAR *ptr;
	SPLINFO *spl;
	PRTINITINFO initinfo;

	initinfo.colorFormat = PRINT_24BITCOLOR_FORMAT_BGR;
	if (!splmax) {  /* perform initializations */
		spltabptr = (SPLINFO **) memalloc(4 * sizeof(SPLINFO), 0);
		/* reset memory pointers */
		setpgmdata();
		if (spltabptr == NULL) dbcerror(408);
		splmax = 4;
		if (!prpget("print", "error", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on"))
			dbcprtflags |= DBCPRTFLAG_DSPERR;
		if (!prpget("print", "autoflush", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on"))
			dbcprtflags |= DBCPRTFLAG_AUTOFLUSH;
		memset(&initinfo, 0, sizeof(initinfo));
		initinfo.getserver = srvgetserver;
#if OS_UNIX
		if (!prpget("print", "lock", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "fcntl")) initinfo.locktype = PRTLOCK_FCNTL;
		else initinfo.locktype = PRTLOCK_FILE;
#endif
		/* do this one last incase prpget destroys previous values */
		if (!prpget("print", "translate", NULL, NULL, &ptr, 0) && *ptr) initinfo.translate = ptr;
		if (!prpget("print", "pdf", "tab", NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) initinfo.pdfOldTabbing = 1;
		if (!prpget("print", "color", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "rgb"))
			initinfo.colorFormat = PRINT_24BITCOLOR_FORMAT_RGB;
		/**
		 * New stuff mostly for PDF printing at iOS.
		 * Should fully implement across the board for 17
		 * 17 JUN 2013 jpr
		 *
		 * Also implemented in JavaFX SC for 17 release
		 * 17 DEC 2013 jpr
		 */
#if DX_MAJOR_VERSION >= 17
		if (!prpget("print", TEXTROTATE, NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) initinfo.TextRotateOld = 1;
#else
		initinfo.TextRotateOld = 1; // Assume 'OLD' for less than 17
		if (!prpget("print", TEXTROTATE, NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "new")) initinfo.TextRotateOld = 0;
#endif
		initinfo.program = "DB/C DX";
		initinfo.version = RELEASE;
		prtinit(&initinfo);
		/* reset memory */
		setpgmdata();
	}

	for (refnum = 0, spl = *spltabptr; refnum < splhi && spl->type; refnum++, spl++);
	if (refnum == splmax) {
		i1 = memchange((UCHAR **) spltabptr, (splmax << 1) * sizeof(SPLINFO), 0);
		/* reset memory */
		setpgmdata();
		if (i1) dbcerror(408);
		splmax <<= 1;
		spl = *spltabptr + refnum;
	}
	memset((UCHAR *) spl, 0, sizeof(SPLINFO));
	spl->colorFormat = initinfo.colorFormat;
	if (refnum == splhi) splhi++;
	return(refnum);
}

static INT printvalue(CHAR *error)
{
	if (!strcmp("access", error)) return PRTERR_ACCESS;
	else if (!strcmp("badname", error)) return PRTERR_BADNAME;
	else if (!strcmp("badopt", error)) return PRTERR_BADOPT;
	else if (!strcmp("badtrans", error)) return PRTERR_BADTRANS;
	else if (!strcmp("cancel", error)) return PRTERR_CANCEL;
	else if (!strcmp("create", error)) return PRTERR_CREATE;
	else if (!strcmp("dialog", error)) return PRTERR_DIALOG;
	else if (!strcmp("exist", error)) return PRTERR_EXIST;
	else if (!strcmp("inuse", error)) return PRTERR_INUSE;
	else if (!strcmp("native", error)) return PRTERR_NATIVE;
	else if (!strcmp("nomem", error)) return PRTERR_NOMEM;
	else if (!strcmp("nospace", error)) return PRTERR_NOSPACE;
	else if (!strcmp("notopen", error)) return PRTERR_NOTOPEN;
	else if (!strcmp("offline", error)) return PRTERR_OFFLINE;
	else if (!strcmp("open", error)) return PRTERR_OPEN;
	else if (!strcmp("server", error)) return PRTERR_SERVER;
	else if (!strcmp("splclose", error)) return PRTERR_SPLCLOSE;
	else if (!strcmp("toolong", error)) return PRTERR_TOOLONG;
#if OS_UNIX
	else if (!printmemicmp("unsupported", error, 11)) return PRTERR_UNSUPCLNT;
#else
	else if (!_stricmp("unsupported", error)) return PRTERR_UNSUPCLNT;
#endif
	else if (!strcmp("write", error)) return PRTERR_WRITE;
	return PRTERR_CLIENT;
}

static INT printmemicmp(void *src, void *dest, INT len)
{
	while (len--) if (toupper(((UCHAR *) src)[len]) != toupper(((UCHAR *) dest)[len])) return(1);
	return(0);
}


static INT printerror(INT error)
{
	INT i1;
	CHAR *ptr, work[64];

	if (!splmax && !prpget("print", "error", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on"))
		dbcprtflags |= DBCPRTFLAG_DSPERR;

	if (error == PRTERR_UNSUPCLNT) {
		strcpy(work, printerrstr);
		strcpy(printerrstr, "Unsupported print function at smart client : ");
		strcat(printerrstr, work);
		error = 412;
		goto exit;
	}
	printerrstr[0] = '\0';
	if (error == PRTERR_OPEN || error == PRTERR_CREATE) error = 401;
	else if (error == PRTERR_BADNAME) error = 402;
	else if (error == PRTERR_INUSE || error == PRTERR_EXIST || error == PRTERR_OFFLINE || error == PRTERR_CANCEL) error = 403;
	else if (error == PRTERR_WRITE) error = 404;
	else if (error == PRTERR_ACCESS) error = 405;
	else if (error == PRTERR_NOTOPEN) error = 406;
	else if (error == PRTERR_TOOLONG) error = 407;
	else if (error == PRTERR_NOMEM) error = 408;
	else if (error == PRTERR_NOSPACE) error = 410;
	else if (error == PRTERR_BADOPT) error = 411;
	else if (error == PRTERR_SPLCLOSE) error = 452;
	else if (error == PRTERR_CLIENT) {
		error = 798;
		strcpy(printerrstr, "smart client error");
	}
	else {
		if (error == PRTERR_SERVER) strcpy(printerrstr, "server error");
		else if (error == PRTERR_DIALOG) strcpy(printerrstr, "dialog error");
		else if (error == PRTERR_NATIVE) strcpy(printerrstr, "API error");
		else if (error == PRTERR_BADTRANS) strcpy(printerrstr, "translation error");
		else if (error == PRTERR_INTERNAL) strcpy(printerrstr, "internal error");
		else {
			strcpy(printerrstr, "PRTERR_");
			mscitoa(error, printerrstr + 7);
		}
		error = 451;
	}
	exit:
	if (dbcprtflags & DBCPRTFLAG_DSPERR) {
		i1 = (INT)strlen(printerrstr);
		if (i1) i1 += 2;
		prtgeterror(printerrstr + i1, sizeof(printerrstr) - i1);
		if (i1 && printerrstr[i1]) {
			printerrstr[i1 - 2] = ':';
			printerrstr[i1 - 1] = ' ';
		}
	}
	return error;
}
