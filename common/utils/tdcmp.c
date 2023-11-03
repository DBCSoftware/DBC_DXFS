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

#define INC_STDLIB
#define INC_STDIO
#define INC_STRING
#define INC_CTYPE
#define INC_SIGNAL
#define INC_TIME
#include "includes.h"
#include "release.h"
#include "arg.h"
#include "base.h"
#include "dbccfg.h"
#include "fio.h"
#include "vid.h"

#define DSPFLAGS_VERBOSE	0x01
#define DSPFLAGS_DSPXTRA	0x02
#define DEATH_INTERRUPT		0
#define DEATH_INVPARM		1
#define DEATH_INIT			2
#define DEATH_NOMEM			3
#define DEATH_OPEN			4
#define DEATH_CREATE		5
#define DEATH_READ			6
#define DEATH_INTERNAL		7

static char *errormsg[] = {
	"HALTED - user interrupt",
	"Invalid parameter ->",
	"Unable to initialize",
	"Unable to allocate memory for buffers",
	"Unable to open",
	"Unable to create",
	"Unable to read from file",
	"Internal error"
};

/* local declarations */
static INT dspflags;
static UCHAR workarea[4000];				/* general work area */
static UCHAR inrec[1004];				/* input record area */
#define NUMSECTIONS 19
static UCHAR **sectionpptr[NUMSECTIONS];	/* pointer to moveable memory */
static INT sectionsize[NUMSECTIONS];		/* current size of a section */
static INT sectionalloc[NUMSECTIONS];		/* currently allocated size of a section */
static INT sectionid[NUMSECTIONS] = {		/* section ids */
	1, 2, 3, 4,
	101, 102, 103, 104, 105, 106, 120,
	301
};
static CHAR *strdefaultstr[] = {
	"BELL", "ENTER_KEY", "BACKSPACE_KEY", "ESCAPE_KEY", "TAB_KEY", NULL
};
static UCHAR strdefaultval[] = { 0x07, 0x0d, 0x08, 0x1b, 0x09 };
static UCHAR defaultflg = FALSE;
static INT kwsection;					/* result of kwlookup */
static INT kwnumber;					/* result of kwlookup */
static INT kwtype;						/* result of kwlookup */
static INT errorcount;					/* error count */

static INT kwlookup(void);
static void setvalue(INT);
static void setstring(INT);
static void error(INT);
static void usage(void);
static void death(INT, INT, CHAR *);
static void quitsig(INT);


INT main(INT argc, CHAR **argv)
{
	INT i1, i2, i3, i4, inhndl, outhndl;
	OFFSET pos;
	CHAR cfgname[MAX_NAMESIZE], inname[MAX_NAMESIZE], outname[MAX_NAMESIZE];
	CHAR work[32], *ptr, *ptr2;
	UCHAR c1;
	FIOPARMS parms;

	arginit(argc, argv, &i1);
	if (!i1) dspsilent();
	signal(SIGINT, quitsig);

	/* initialize */
	if (meminit(512 << 10, 0, 64) == -1) death(DEATH_INIT, ERR_NOMEM, NULL);
	cfgname[0] = '\0';
	while (!argget(ARG_NEXT | ARG_IGNOREOPT, (char *) workarea, sizeof(workarea))) {
		if (workarea[0] == '-') {
			if (workarea[1] == '?') usage();
			if (toupper(workarea[1]) == 'C' && toupper(workarea[2]) == 'F' &&
			    toupper(workarea[3]) == 'G' && workarea[4] == '=') strcpy(cfgname, (char *) &workarea[5]);
		}
	}
	if (cfginit(cfgname, FALSE)) death(DEATH_INIT, 0, cfggeterror());
	if (prpinit(cfggetxml(), CFG_PREFIX "cfg")) ptr = fioinit(NULL, FALSE);
	else ptr = fioinit(&parms, FALSE);
	if (ptr != NULL) death(DEATH_INIT, 0, ptr);

	ptr = (CHAR *) workarea;
	outname[0] = '\0';

	/* scan input and output file names */
	i1 = argget(ARG_FIRST, inname, sizeof(inname));
	if (i1 == 1) usage();
	if (i1 < 0) death(DEATH_INIT, i1, NULL);

	/* get other parameters */
	for ( ; ; ) {
		i1 = argget(ARG_NEXT, ptr, sizeof(workarea));
		if (i1) {
			if (i1 < 0) death(DEATH_INIT, i1, NULL);
			break;
		}
		if (ptr[0] == '-') {
			switch (toupper(ptr[1])) {
				case '!':
					dspflags |= DSPFLAGS_VERBOSE | DSPFLAGS_DSPXTRA;
					break;
				case 'C':
					if (toupper(ptr[2]) != 'F' || toupper(ptr[3]) != 'G' || ptr[4] != '=' || !ptr[5]) death(DEATH_INVPARM, 0, ptr);
					break;
				case 'V':
					dspflags |= DSPFLAGS_VERBOSE;
					break;
				default:
					death(DEATH_INVPARM, 0, ptr);
			}
		}
		else if (!outname[0]) strcpy(outname, ptr);
		else death(DEATH_INVPARM, 0, ptr);
	}
	if (!outname[0]) {
		strcpy(outname, inname);
		miofixname(outname, ".tdb", FIXNAME_EXT_ADD | FIXNAME_EXT_REPLACE);
	}
	else miofixname(outname, ".tdb", FIXNAME_EXT_ADD);

	miofixname(inname, ".tdf", FIXNAME_EXT_ADD);
	inhndl = rioopen(inname, RIO_M_ERO | RIO_P_TDF | RIO_T_ANY, 3, 1000);
	if (inhndl < 0) death(DEATH_OPEN, inhndl, inname);

	outhndl = fioopen(outname, FIO_M_PRP | FIO_P_TDB);
	if (outhndl < 0) death(DEATH_CREATE, outhndl, outname);

	/* header section */
	sectionpptr[0] = memalloc(48, 0);
	ptr = (CHAR *) *(sectionpptr[0]);
	ptr[0] = 1;  /* major version */
	ptr[1] = 0;  /* minor version */
	memset(&ptr[2], ' ', 16);
	memcpy(&ptr[2], inname, strlen(inname));
	msctimestamp((UCHAR *) &ptr[18]);
	sectionsize[0] = 34;

	while (TRUE) {
		i4 = rioget(inhndl, inrec, 1000);
		if (i4 < 0) {
			if (i4 == -1) break;
			if (i4 == -2) continue;
			death(DEATH_READ, i4, NULL);
		}
		inrec[i4] = '\0';
		// Skip leading spaces
		for (i1 = 0; isspace(inrec[i1]); i1++);
		// Skip records that are zero length or that start with a ;
		if (!inrec[i1] || inrec[i1] == ';') continue;
		// Scan keyword. Must be letter, number, or underscore
		for (i4 = 0; i4 < 40 && (isalnum(inrec[i1]) || inrec[i1] == '_'); i1++)
			workarea[i4++] = (UCHAR) toupper(inrec[i1]);
		workarea[i4] = '\0';
		
		/* Special case, ignore obsolete keywords */
		if (strcmp((CHAR*)workarea, "SCROLL_NOERASE") == 0) continue;
		if (strcmp((CHAR*)workarea, "ALL_OFF") == 0) continue;
		
		if (i4 == 40 || kwlookup()) {  /* look up keyword */
			error(2);
			continue;
		}
		if (kwtype == 0) continue;  /* internal flags */
		if (kwtype == 1) {  /* flag means set value to 1 */
			setvalue(1);
			continue;
		}
		while (isspace(inrec[i1])) i1++;
		if (inrec[i1++] != '=') {  /* parse equal sign */
			error(3);
			continue;
		}
		while (isspace(inrec[i1])) i1++;
		if (kwtype == 2) {  /* its a value type keyword */
			i4 = i1;
			if (inrec[i1] == '#') i1++;
			for (i3 = 0; isdigit(inrec[i1]); i1++) i3 = i3 * 10 + inrec[i1] - '0';
			if (inrec[i1] == '#') i1++;
			if (i1 == i4 || (inrec[i1] && !isspace(inrec[i1]))) {
				error(1);
				continue;
			}
			setvalue(i3);
			continue;
		}
		/* kwtype > 2 means character sequence keyword, store it in workarea */
		i4 = 0;
		while ((c1 = inrec[i1++]) && c1 != ';') {
			if (isspace(c1)) continue;
			if (c1 == '\\' || c1 == '^') {  /* forcing and CTRL character */
				if (!inrec[i1]) {
					error(5);
					break;
				}
				if (c1 == '^') {
					// Compress ^ syntax to a single control byte
					// e.g. ^[ becomes 0x1B aka ESC
					workarea[i4++] = inrec[i1] & 0x1f;
				}
				else workarea[i4++] = inrec[i1];
				i1++;
			}
			else if (c1 == '#') {  /* decimal value of character*/
				for (i3 = 0; i3 < 256 && isdigit(inrec[i1]); i1++) i3 = i3 * 10 + inrec[i1] - '0';
				if (!(c1 = inrec[i1++])) {
					error(5);
					break;
				}
				if (c1 != '#' || i3 > 255) {
					error(1);
					break;
				}
				if (!i3 && kwtype >= 4) workarea[i4++] = '\0';
				workarea[i4++] = (UCHAR) i3;
			}
			else if (c1 == '%') {  /* replacement character */
				/* 2 bytes written for replacement character */
				/* first byte = 0, second byte != 0 */
				/* second byte format uses bits: */
				/*     10000111 for parameter type */
				/*     00001000 for add character */
				/*     01110000 for format type */
				if (kwtype < 4) {
					error(6);
					break;
				}
				ptr2 = "";
				if (kwtype == 4) ptr = "VH";
				else if (kwtype == 5) ptr = "H";
				else if (kwtype == 6) ptr = "V";
				else if (kwtype == 7) {
					ptr = "TB";
					ptr2 = "-B\0B-T\0";
				}
				else if (kwtype == 8) {
					ptr = "LR";
					ptr2 = "-R\0R-L\0";
				}
				else if (kwtype == 9) {
					ptr = "TBLRNA";
					ptr2 = "-B\0B-T\0-R\0R-L\0";
				}
				else if (kwtype == 10) ptr="A";
				workarea[i4++] = '\0';
				if (!(c1 = inrec[i1++])) {
					error(5);
					break;
				}
				if (c1 != '(') {
					c1 = (UCHAR) toupper(c1);
					for (i3 = 0; ptr[i3] && ptr[i3] != c1; i3++);
					if (!ptr[i3]) {
						error(6);
						break;
					}
					workarea[i4] = (UCHAR)(i3 + 1);
				}
				else {
					for (i2 = 0; inrec[i1] && inrec[i1] != ')'; i1++) work[i2++] = (CHAR) toupper(inrec[i1]);
					if (!inrec[i1++]) {
						error(5);
						break;
					}
					work[i2] = 0;
					for (i3 = 0, i2 = 0; ptr2[i2] && strcmp(work, &ptr2[i2]); i3++, i2 += (INT)strlen(&ptr2[i2]) + 1);
					if (!ptr2[i2]) {
						error(6);
						break;
					}
					workarea[i4] = (UCHAR)(i3 + 0x80);
				}
				c1 = inrec[i1++];
				if (c1 == '+') {
					workarea[i4] |= 0x08;
					if (!(c1 = inrec[i1++])) {
						error(5);
						break;
					}
					if (c1 == '\\' || c1 == '^') {
						if (!inrec[i1]) {
							error(5);
							break;
						}
						if (c1 == '^') workarea[i4 + 1] = toupper(inrec[i1]) - 'A' + 1;
						else workarea[i4 + 1] = inrec[i1];
						i1++;
					}
					else if (c1 == '#') {
						if (!inrec[i1]) {
							error(5);
							break;
						}
						for (i3 = 0; i3 < 256 && isdigit(inrec[i1]); i1++) i3 = i3 * 10 + inrec[i1] - '0';
						if (!inrec[i1]) {
							error(5);
							break;
						}
						if (inrec[i1++] != '#' || i3 > 255) {
							error(1);
							break;
						}
						workarea[i4 + 1] = (UCHAR) i3;
					}
					else if (isdigit(c1)) workarea[i4 + 1] = c1 - '0';
					else workarea[i4 + 1] = c1;
					c1 = inrec[i1++];
				}
				c1 = toupper(c1);
				if (c1 == 'C'); // @suppress("Suspicious semicolon")
				else if (c1 == 'D') workarea[i4] |= 0x10;
				else if (c1 == '2') workarea[i4] |= 0x20;
				else if (c1 == 'Z') workarea[i4] |= 0x30;
				else if (c1 == 'T') workarea[i4] |= 0x40;
				else if (c1 == 'N') workarea[i4] |= 0x50;
				else {
					error(1);
					break;
				}
				if (workarea[i4] & 0x08) i4++;
				i4++;
			}
			else workarea[i4++] = c1;  /* not a special character, just store it */
		}
		setstring(i4);
	}

	defaultflg = TRUE;
	strcpy((CHAR *) inrec, "DEFAULT: ");
	for (i1 = 0; strdefaultstr[i1] != NULL; i1++) {
		if (!strdefaultval[i1]) continue;
		strcpy((CHAR *) workarea, strdefaultstr[i1]);
		strcpy((CHAR *) &inrec[9], (CHAR *) workarea);
		i2 = (INT)strlen((CHAR *) inrec);
		inrec[i2++] = '=';
		inrec[i2++] = '^';
		inrec[i2++] = (UCHAR)(strdefaultval[i1] + '@');
		inrec[i2] = 0;
		
		if (kwlookup()) {
			error(2);
			continue;
		}
		workarea[0] = strdefaultval[i1];
		setstring(1);
	}
	
	if (errorcount) {  /* if errors, display error message */
		if (errorcount > 9) dspchar((char)(errorcount / 10 + '0'));
		dspchar((char)(errorcount % 10 + '0'));
		dspstring(" errors\n");
		cfgexit();
		exit(1);
	}

	pos = 0;
	for (i1 = 0; i1 < NUMSECTIONS; i1++) {  /* write out the file */
		if (sectionsize[i1]) {
			workarea[0] = (UCHAR)(sectionsize[i1] + 4);
			workarea[1] = (UCHAR)((sectionsize[i1] + 4) >> 8);
			workarea[2] = (UCHAR) sectionid[i1];
			workarea[3] = (UCHAR)(sectionid[i1] >> 8);
			fiowrite(outhndl, pos, workarea, 4);
			pos += 4;
			fiowrite(outhndl, pos, *(sectionpptr[i1]), sectionsize[i1]);
			pos += sectionsize[i1];
		}
	}
	workarea[0] = 4;
	memset(&workarea[1], 0, 3);
	fiowrite(outhndl, pos, workarea, 4L);
	rioclose(inhndl);
	fioclose(outhndl);
	if (dspflags & DSPFLAGS_VERBOSE) {
		dspstring(outname);
		dspstring(" Created\n");
	}
	cfgexit();
	exit(0);
	return(0);
}

static void setvalue(INT num)  /* store a value for the current keyword */
{
	INT i1;
	UCHAR *ptr;

	for (i1 = 0; i1 < NUMSECTIONS && sectionid[i1] != kwsection; i1++);
	if (i1 == NUMSECTIONS || kwsection != 2) death(DEATH_INTERNAL, 1, NULL);
	if (!sectionalloc[i1]) {
		sectionalloc[i1] = sectionsize[i1] = 16;
		sectionpptr[i1] = memalloc(sectionalloc[i1], 0);
		if (sectionpptr[i1] == NULL) death(DEATH_NOMEM, 0, NULL);
		ptr = *(sectionpptr[i1]);
		memset(ptr, 0, sectionalloc[i1]);
	}
	else ptr = *(sectionpptr[i1]);
	if (ptr[kwnumber] || ptr[kwnumber + 1]) {
		error(7);
		return;
	}
	ptr[kwnumber] = (UCHAR) num;
	ptr[kwnumber + 1] = (UCHAR)(num >> 8);
}

static void setstring(INT len)
{
	INT sectionindex, i2, i3, i4;
	UCHAR *ptr;
	CHAR dspwork[64];

	for (sectionindex = 0; sectionindex < NUMSECTIONS && sectionid[sectionindex] != kwsection; sectionindex++);
	if (sectionindex == NUMSECTIONS) death(DEATH_INTERNAL, 2, NULL);

	if (kwsection == 301) i2 = len + 3;  /* keyin section */
	else if (kwsection == 120) i2 = len + 2;  /* SPECIAL_ display section */
	else i2 = kwnumber + len + 1;  /* regular display sections */
	if (sectionsize[sectionindex] + i2 > sectionalloc[sectionindex]) {
		if (!sectionalloc[sectionindex]) {
			sectionpptr[sectionindex] = memalloc(300, 0);
			if (sectionpptr[sectionindex] == NULL) death(DEATH_NOMEM, 0, NULL);
		}
		else if (memchange(sectionpptr[sectionindex], sectionalloc[sectionindex] + 300, 0)) death(DEATH_NOMEM, 0, NULL);
		ptr = *(sectionpptr[sectionindex]);
		memset(&ptr[sectionalloc[sectionindex]], 0, 300);
		sectionalloc[sectionindex] += 300;
		if (dspflags & DSPFLAGS_DSPXTRA && (sectionid[sectionindex] == 104 || sectionid[sectionindex] == 105)) { // @suppress("Suggested parenthesis around expression")
#ifndef HPUX11
			sprintf(dspwork, "In %s, secalloc expanded to %d", __FUNCTION__, sectionalloc[sectionindex]);
#else
			sprintf(dspwork, "In setstring, secalloc expanded to %d", sectionalloc[sectionindex]);
#endif
			dspstring(dspwork);
			dspstring("\n");
		}
	}
	else ptr = *(sectionpptr[sectionindex]);

	if (kwsection == 301) {  /* keyin section */
		for (i2 = 0; i2 < sectionsize[sectionindex]; i2 += ptr[i2] + 3) {
			i3 = ptr[i2];
			if (i3 > len) i3 = len;
			i4 = memcmp(workarea, &ptr[i2 + 1], i3);
			if (i4 < 0 || (i4 == 0 && len > i3)) {
				memmove(&ptr[i2 + len + 3], &ptr[i2], sectionsize[sectionindex] - i2);
				break;
			}
			if (i4 == 0 && len == ptr[i2]) {
				if (!defaultflg) error(8);
				return;
			}
		}
		ptr[i2++] = (UCHAR) len;
		memcpy(&ptr[i2], workarea, len);
		i2 += len;
		ptr[i2] = (UCHAR) kwnumber;
		ptr[i2 + 1] = (UCHAR)(kwnumber >> 8);
		sectionsize[sectionindex] += len + 3;
	}
	else if (kwsection == 120) {  /* SPECIAL_ display sections */
		for (i2 = 0; i2 < sectionsize[sectionindex] && kwnumber > (INT) ptr[i2]; i2 += ptr[i2 + 1] + 2);
		if (i2 < sectionsize[sectionindex]) {
			if (kwnumber == (INT) ptr[i2]) {
				error(7);
				return;
			}
			memmove(&ptr[i2 + len + 2], &ptr[i2], sectionsize[sectionindex] - i2);
		}
		ptr[i2] = (UCHAR) kwnumber;
		ptr[i2 + 1] = (UCHAR) len;
		memcpy(&ptr[i2 + 2], workarea, len);
		sectionsize[sectionindex] += len + 2;
	}
	else {  /* regular display sections */
		for (i2 = i3 = 0; i3 < kwnumber; i2 += ptr[i2] + 1, i3++);
		if (ptr[i2]) {
			error(7);
			if (dspflags & DSPFLAGS_DSPXTRA) {
#ifndef HPUX11
				sprintf(dspwork, "In %s, sec=%d, num=%d, wa='%.*s'", __FUNCTION__, kwsection, kwnumber, len, workarea);
#else
				sprintf(dspwork, "In setstring, sec=%d, num=%d, wa='%.*s'", kwsection, kwnumber, len, workarea);
#endif
				dspstring(dspwork);
				dspstring("\n");
			}
			return;
		}
		if (i2 >= sectionsize[sectionindex]) sectionsize[sectionindex] = i2;
		else {
			memmove(&ptr[i2 + 1 + len], &ptr[i2 + 1], sectionsize[sectionindex] - (i2 + 1));
			sectionsize[sectionindex]--;  /* length byte already counted for */
		}
		ptr[i2] = (UCHAR) len;
		memcpy(&ptr[i2 + 1], workarea, len);
		sectionsize[sectionindex] += len + 1;
	}
}

static INT kwlookup()  /* keyword lookup, keyword in workarea */
/* return 0 if found, else 1 */
{
	INT i1, i2;
	CHAR dspwork[4096];
	static struct kwstruct {
		INT section;
		INT number;
		UCHAR type;
		CHAR *name;
	} keywords[] = {
		2, 0, 2, "LINES",
		2, 2, 2, "COLUMNS",
		2, 4, 1, "XTERM_MOUSE",  /* turn on and recognize left mouse button */
		2, 6, 1, "SCROLL_REPOS",
		2, 8, 1, "COLOR_ERASE",
		2, 10, 1, "NO_ROLL",
		2, 12, 1, "SCROLL_ERASE",
		2, 14, 1, "ASCII_CONTROL",  /* note length 16 is hardwired in setvalue() */
		3, 0, 3, "INITIALIZE",
		4, 0, 3, "TERMINATE",
		101, 0, 4, "POSITION_CURSOR",
		101, 1, 5, "POSITION_CURSOR_HORZ",
		101, 2, 6, "POSITION_CURSOR_VERT",
		101, 3, 3, "CLEAR_SCREEN",
		101, 4, 3, "CLEAR_TO_SCREEN_END",
		101, 5, 3, "CLEAR_TO_LINE_END",
		101, 6, 3, "CURSOR_ON",
		101, 7, 3, "CURSOR_OFF",
		101, 8, 3, "CURSOR_BLOCK",
		101, 9, 3, "CURSOR_UNDERLINE",
		101, 10, 3, "REVERSE_ON",
		101, 11, 3, "REVERSE_OFF",
		101, 12, 3, "UNDERLINE_ON",
		101, 13, 3, "UNDERLINE_OFF",
		101, 14, 3, "BLINK_ON",
		101, 15, 3, "BLINK_OFF",
		101, 16, 3, "BOLD_ON",
		101, 17, 3, "BOLD_OFF",
		101, 18, 3, "DIM_ON",
		101, 19, 3, "DIM_OFF",
		101, 20, 3, "RIGHT_TO_LEFT_DISPLAY_ON",
		101, 21, 3, "RIGHT_TO_LEFT_DISPLAY_OFF",
		101, 22, 3, "DISPLAY_DOWN_ON",
		101, 23, 3, "DISPLAY_DOWN_OFF",
		101, 24, 3, "ALL_OFF",
		101, 25, 3, "BELL",
		101, 26, 3, "AUXPORT_ON",
		101, 27, 3, "AUXPORT_OFF",
		101, 28, 3, "CURSOR_HALF",
		101, 29, 10, "FG_COLOR_ANSI256",
		101, 30, 10, "BG_COLOR_ANSI256",
		102, 0, 3, "INSERT_CHAR",
		102, 1, 3, "DELETE_CHAR",
		102, 2, 3, "INSERT_LINE",
		102, 3, 3, "DELETE_LINE",
		102, 4, 3, "OPEN_LINE",
		102, 5, 3, "CLOSE_LINE",
		103, 0, 7, "SCROLL_REGION_TB",
		103, 1, 8, "SCROLL_REGION_LR",
		103, 2, 3, "SCROLL_UP",
		103, 3, 3, "SCROLL_DOWN",
		103, 4, 3, "SCROLL_LEFT",
		103, 5, 3, "SCROLL_RIGHT",
		103, 6, 9, "SCROLL_WIN_UP",
		103, 7, 9, "SCROLL_WIN_DOWN",
		103, 8, 9, "SCROLL_WIN_LEFT",
		103, 9, 9, "SCROLL_WIN_RIGHT",
		103, 10, 9, "ERASE_WIN",
		103, 11, 3, "ENABLE_ROLL",
		103, 12, 3, "DISABLE_ROLL",
		104, 0, 3, "FG_BLACK",
		104, 1, 3, "FG_BLUE",
		104, 2, 3, "FG_GREEN",
		104, 3, 3, "FG_CYAN",
		104, 4, 3, "FG_RED",
		104, 5, 3, "FG_MAGENTA",
		104, 6, 3, "FG_YELLOW",
		104, 7, 3, "FG_WHITE",
		105, 0, 3, "BG_BLACK",
		105, 1, 3, "BG_BLUE",
		105, 2, 3, "BG_GREEN",
		105, 3, 3, "BG_CYAN",
		105, 4, 3, "BG_RED",
		105, 5, 3, "BG_MAGENTA",
		105, 6, 3, "BG_YELLOW",
		105, 7, 3, "BG_WHITE",
		106, 0, 3, "GRAPHIC_ON",
		106, 1, 3, "GRAPHIC_OFF",
		106, 2, 3, "GRAPHIC_UP_ARROW",
		106, 3, 3, "GRAPHIC_RIGHT_ARROW",
		106, 4, 3, "GRAPHIC_DOWN_ARROW",
		106, 5, 3, "GRAPHIC_LEFT_ARROW",
		106, 6, 3, "GRAPHIC_HORZ_LINE",
		106, 6, 3, "GRAPHIC_0",
		106, 7, 3, "GRAPHIC_VERT_LINE",
		106, 7, 3, "GRAPHIC_1",
		106, 8, 3, "GRAPHIC_CROSS",
		106, 8, 3, "GRAPHIC_2",
		106, 9, 3, "GRAPHIC_UPPER_LEFT_CORNER",
		106, 9, 3, "GRAPHIC_3",
		106, 10, 3, "GRAPHIC_UPPER_RIGHT_CORNER",
		106, 10, 3, "GRAPHIC_4",
		106, 11, 3, "GRAPHIC_LOWER_LEFT_CORNER",
		106, 11, 3, "GRAPHIC_5",
		106, 12, 3, "GRAPHIC_LOWER_RIGHT_CORNER",
		106, 12, 3, "GRAPHIC_6",
		106, 13, 3, "GRAPHIC_RIGHT_TICK",
		106, 13, 3, "GRAPHIC_7",
		106, 14, 3, "GRAPHIC_DOWN_TICK",
		106, 14, 3, "GRAPHIC_8",
		106, 15, 3, "GRAPHIC_LEFT_TICK",
		106, 15, 3, "GRAPHIC_9",
		106, 16, 3, "GRAPHIC_UP_TICK",
		106, 16, 3, "GRAPHIC_10",
		106, 17, 3, "GRAPHIC_11",
		106, 18, 3, "GRAPHIC_12",
		106, 19, 3, "GRAPHIC_13",
		106, 20, 3, "GRAPHIC_14",
		106, 21, 3, "GRAPHIC_15",
		106, 22, 3, "GRAPHIC_16",
		106, 23, 3, "GRAPHIC_17",
		106, 24, 3, "GRAPHIC_18",
		106, 25, 3, "GRAPHIC_19",
		106, 26, 3, "GRAPHIC_20",
		106, 27, 3, "GRAPHIC_21",
		106, 28, 3, "GRAPHIC_22",
		106, 29, 3, "GRAPHIC_23",
		106, 30, 3, "GRAPHIC_24",
		106, 31, 3, "GRAPHIC_25",
		106, 32, 3, "GRAPHIC_26",
		106, 33, 3, "GRAPHIC_27",
		106, 34, 3, "GRAPHIC_28",
		106, 35, 3, "GRAPHIC_29",
		106, 36, 3, "GRAPHIC_30",
		106, 37, 3, "GRAPHIC_31",
		106, 38, 3, "GRAPHIC_32",
		106, 39, 3, "GRAPHIC_33",
		106, 40, 3, "GRAPHIC_34",
		106, 41, 3, "GRAPHIC_35",
		106, 42, 3, "GRAPHIC_36",
		106, 43, 3, "GRAPHIC_37",
		106, 44, 3, "GRAPHIC_38",
		106, 45, 3, "GRAPHIC_39",
		106, 46, 3, "GRAPHIC_40",
		106, 47, 3, "GRAPHIC_41",
		106, 48, 3, "GRAPHIC_42",
		106, 49, 3, "GRAPHIC_43",
		301, VID_ENTER, 3, "ENTER_KEY",
		301, VID_ESCAPE, 3, "ESCAPE_KEY",
		301, VID_BKSPC, 3, "BACKSPACE_KEY",
		301, VID_TAB, 3, "TAB_KEY",
		301, VID_BKTAB, 3, "BACKTAB_KEY",
		301, VID_UP, 3, "UP_KEY",
		301, VID_DOWN, 3, "DOWN_KEY",
		301, VID_LEFT, 3, "LEFT_KEY",
		301, VID_RIGHT, 3, "RIGHT_KEY",
		301, VID_INSERT, 3, "INSERT_KEY",
		301, VID_DELETE, 3, "DELETE_KEY",
		301, VID_HOME, 3, "HOME_KEY",
		301, VID_END, 3, "END_KEY",
		301, VID_PGUP, 3, "PAGE_UP_KEY",
		301, VID_PGDN, 3, "PAGE_DOWN_KEY",
		301, VID_SHIFTUP, 3, "SHIFT_UP_KEY",
		301, VID_SHIFTDOWN, 3, "SHIFT_DOWN_KEY",
		301, VID_SHIFTLEFT, 3, "SHIFT_LEFT_KEY",
		301, VID_SHIFTRIGHT, 3, "SHIFT_RIGHT_KEY",
		301, VID_SHIFTINSERT, 3, "SHIFT_INSERT_KEY",
		301, VID_SHIFTDELETE, 3, "SHIFT_DELETE_KEY",
		301, VID_SHIFTHOME, 3, "SHIFT_HOME_KEY",
		301, VID_SHIFTEND, 3, "SHIFT_END_KEY",
		301, VID_SHIFTPGUP, 3, "SHIFT_PAGE_UP_KEY",
		301, VID_SHIFTPGDN, 3, "SHIFT_PAGE_DOWN_KEY",
		301, VID_CTLUP, 3, "CONTROL_UP_KEY",
		301, VID_CTLDOWN, 3, "CONTROL_DOWN_KEY",
		301, VID_CTLLEFT, 3, "CONTROL_LEFT_KEY",
		301, VID_CTLRIGHT, 3, "CONTROL_RIGHT_KEY",
		301, VID_CTLINSERT, 3, "CONTROL_INSERT_KEY",
		301, VID_CTLDELETE, 3, "CONTROL_DELETE_KEY",
		301, VID_CTLHOME, 3, "CONTROL_HOME_KEY",
		301, VID_CTLEND, 3, "CONTROL_END_KEY",
		301, VID_CTLPGUP, 3, "CONTROL_PAGE_UP_KEY",
		301, VID_CTLPGDN, 3, "CONTROL_PAGE_DOWN_KEY",
		301, VID_ALTUP, 3, "ALT_UP_KEY",
		301, VID_ALTDOWN, 3, "ALT_DOWN_KEY",
		301, VID_ALTLEFT, 3, "ALT_LEFT_KEY",
		301, VID_ALTRIGHT, 3, "ALT_RIGHT_KEY",
		301, VID_ALTINSERT, 3, "ALT_INSERT_KEY",
		301, VID_ALTDELETE, 3, "ALT_DELETE_KEY",
		301, VID_ALTHOME, 3, "ALT_HOME_KEY",
		301, VID_ALTEND, 3, "ALT_END_KEY",
		301, VID_ALTPGUP, 3, "ALT_PAGE_UP_KEY",
		301, VID_ALTPGDN, 3, "ALT_PAGE_DOWN_KEY",
		301, VID_F1, 3, "F1_KEY",
		301, VID_F2, 3, "F2_KEY",
		301, VID_F3, 3, "F3_KEY",
		301, VID_F4, 3, "F4_KEY",
		301, VID_F5, 3, "F5_KEY",
		301, VID_F6, 3, "F6_KEY",
		301, VID_F7, 3, "F7_KEY",
		301, VID_F8, 3, "F8_KEY",
		301, VID_F9, 3, "F9_KEY",
		301, VID_F10, 3, "F10_KEY",
		301, VID_F11, 3, "F11_KEY",
		301, VID_F12, 3, "F12_KEY",
		301, VID_F13, 3, "F13_KEY",
		301, VID_F14, 3, "F14_KEY",
		301, VID_F15, 3, "F15_KEY",
		301, VID_F16, 3, "F16_KEY",
		301, VID_F17, 3, "F17_KEY",
		301, VID_F18, 3, "F18_KEY",
		301, VID_F19, 3, "F19_KEY",
		301, VID_F20, 3, "F20_KEY",
		301, VID_SHIFTF1, 3, "SHIFT_F1_KEY",
		301, VID_SHIFTF2, 3, "SHIFT_F2_KEY",
		301, VID_SHIFTF3, 3, "SHIFT_F3_KEY",
		301, VID_SHIFTF4, 3, "SHIFT_F4_KEY",
		301, VID_SHIFTF5, 3, "SHIFT_F5_KEY",
		301, VID_SHIFTF6, 3, "SHIFT_F6_KEY",
		301, VID_SHIFTF7, 3, "SHIFT_F7_KEY",
		301, VID_SHIFTF8, 3, "SHIFT_F8_KEY",
		301, VID_SHIFTF9, 3, "SHIFT_F9_KEY",
		301, VID_SHIFTF10, 3, "SHIFT_F10_KEY",
		301, VID_SHIFTF11, 3, "SHIFT_F11_KEY",
		301, VID_SHIFTF12, 3, "SHIFT_F12_KEY",
		301, VID_SHIFTF13, 3, "SHIFT_F13_KEY",
		301, VID_SHIFTF14, 3, "SHIFT_F14_KEY",
		301, VID_SHIFTF15, 3, "SHIFT_F15_KEY",
		301, VID_SHIFTF16, 3, "SHIFT_F16_KEY",
		301, VID_SHIFTF17, 3, "SHIFT_F17_KEY",
		301, VID_SHIFTF18, 3, "SHIFT_F18_KEY",
		301, VID_SHIFTF19, 3, "SHIFT_F19_KEY",
		301, VID_SHIFTF20, 3, "SHIFT_F20_KEY",
		301, VID_CTLF1, 3, "CONTROL_F1_KEY",
		301, VID_CTLF2, 3, "CONTROL_F2_KEY",
		301, VID_CTLF3, 3, "CONTROL_F3_KEY",
		301, VID_CTLF4, 3, "CONTROL_F4_KEY",
		301, VID_CTLF5, 3, "CONTROL_F5_KEY",
		301, VID_CTLF6, 3, "CONTROL_F6_KEY",
		301, VID_CTLF7, 3, "CONTROL_F7_KEY",
		301, VID_CTLF8, 3, "CONTROL_F8_KEY",
		301, VID_CTLF9, 3, "CONTROL_F9_KEY",
		301, VID_CTLF10, 3, "CONTROL_F10_KEY",
		301, VID_CTLF11, 3, "CONTROL_F11_KEY",
		301, VID_CTLF12, 3, "CONTROL_F12_KEY",
		301, VID_CTLF13, 3, "CONTROL_F13_KEY",
		301, VID_CTLF14, 3, "CONTROL_F14_KEY",
		301, VID_CTLF15, 3, "CONTROL_F15_KEY",
		301, VID_CTLF16, 3, "CONTROL_F16_KEY",
		301, VID_CTLF17, 3, "CONTROL_F17_KEY",
		301, VID_CTLF18, 3, "CONTROL_F18_KEY",
		301, VID_CTLF19, 3, "CONTROL_F19_KEY",
		301, VID_CTLF20, 3, "CONTROL_F20_KEY",
		301, VID_ALTF1, 3, "ALT_F1_KEY",
		301, VID_ALTF2, 3, "ALT_F2_KEY",
		301, VID_ALTF3, 3, "ALT_F3_KEY",
		301, VID_ALTF4, 3, "ALT_F4_KEY",
		301, VID_ALTF5, 3, "ALT_F5_KEY",
		301, VID_ALTF6, 3, "ALT_F6_KEY",
		301, VID_ALTF7, 3, "ALT_F7_KEY",
		301, VID_ALTF8, 3, "ALT_F8_KEY",
		301, VID_ALTF9, 3, "ALT_F9_KEY",
		301, VID_ALTF10, 3, "ALT_F10_KEY",
		301, VID_ALTF11, 3, "ALT_F11_KEY",
		301, VID_ALTF12, 3, "ALT_F12_KEY",
		301, VID_ALTF13, 3, "ALT_F13_KEY",
		301, VID_ALTF14, 3, "ALT_F14_KEY",
		301, VID_ALTF15, 3, "ALT_F15_KEY",
		301, VID_ALTF16, 3, "ALT_F16_KEY",
		301, VID_ALTF17, 3, "ALT_F17_KEY",
		301, VID_ALTF18, 3, "ALT_F18_KEY",
		301, VID_ALTF19, 3, "ALT_F19_KEY",
		301, VID_ALTF20, 3, "ALT_F20_KEY",
		301, VID_ALTA, 3, "ALT_A_KEY",
		301, VID_ALTB, 3, "ALT_B_KEY",
		301, VID_ALTC, 3, "ALT_C_KEY",
		301, VID_ALTD, 3, "ALT_D_KEY",
		301, VID_ALTE, 3, "ALT_E_KEY",
		301, VID_ALTF, 3, "ALT_F_KEY",
		301, VID_ALTG, 3, "ALT_G_KEY",
		301, VID_ALTH, 3, "ALT_H_KEY",
		301, VID_ALTI, 3, "ALT_I_KEY",
		301, VID_ALTJ, 3, "ALT_J_KEY",
		301, VID_ALTK, 3, "ALT_K_KEY",
		301, VID_ALTL, 3, "ALT_L_KEY",
		301, VID_ALTM, 3, "ALT_M_KEY",
		301, VID_ALTN, 3, "ALT_N_KEY",
		301, VID_ALTO, 3, "ALT_O_KEY",
		301, VID_ALTP, 3, "ALT_P_KEY",
		301, VID_ALTQ, 3, "ALT_Q_KEY",
		301, VID_ALTR, 3, "ALT_R_KEY",
		301, VID_ALTS, 3, "ALT_S_KEY",
		301, VID_ALTT, 3, "ALT_T_KEY",
		301, VID_ALTU, 3, "ALT_U_KEY",
		301, VID_ALTV, 3, "ALT_V_KEY",
		301, VID_ALTW, 3, "ALT_W_KEY",
		301, VID_ALTX, 3, "ALT_X_KEY",
		301, VID_ALTY, 3, "ALT_Y_KEY",
		301, VID_ALTZ, 3, "ALT_Z_KEY",
		  0,   0, 0, "~"
	};

	for (i1 = 0; keywords[i1].section && strcmp(keywords[i1].name, (CHAR *) workarea); i1++);
	if (!(kwsection = keywords[i1].section)) {
		if (!memcmp("G_COLOR_", &workarea[1], 8)) {
			if (!isdigit(workarea[9])) return(1);
			for (i1 = 9, kwnumber = 0; isdigit(workarea[i1]); i1++) kwnumber = kwnumber * 10 + workarea[i1] - '0';
			if (kwnumber > 15) return(1);
			kwtype = 3;
			if (workarea[0] == 'F') {
				kwsection = 104;
				return(0);
			}
			if (workarea[0] == 'B') {
				kwsection = 105;
				return(0);
			}
		}
		else if (!memcmp("SPECIAL_", workarea, 8)) {
			if (!isdigit(workarea[8])) return(1);
			for (i1 = 8, kwnumber = 0; isdigit(workarea[i1]); i1++) kwnumber = kwnumber * 10 + workarea[i1] - '0';
			if (!kwnumber) return(1);
			kwtype = 3;
			if (!workarea[i1]) {
				if (kwnumber < 256) {
					kwsection = 120;
					return(0);
				}
			}
			else if (!memcmp(&workarea[i1], "_KEY", 4)) {
				if (kwnumber <= VID_MAXKEYVAL) {
					kwsection = 301;
					return(0);
				}
			}
		}
		else if (!strcmp("NO_DEFAULTS", (CHAR *) workarea)) {
			for (i1 = 0; strdefaultstr[i1] != NULL; i1++) strdefaultval[i1] = 0x00;
			kwtype = 0;
			return(0);
		}
		return(1);
	}
	if (!defaultflg) {
		for (i2 = 0; strdefaultstr[i2] != NULL; i2++) {
			if (strcmp(strdefaultstr[i2], (CHAR *) workarea)) continue;
			strdefaultval[i2] = 0x00;
			break;
		}
	}
	kwnumber = keywords[i1].number;
	kwtype = keywords[i1].type;
	if (dspflags & DSPFLAGS_DSPXTRA) {
		sprintf(dspwork, "Keyword %s found", workarea);
		dspstring(dspwork);
		dspstring("\n");
	}
	return(0);
}

/**
 * non-terminal error
 */
static void error(INT n)
{
	if (++errorcount == 100) {
		dspstring("Too many errors, compilation terminated\n");
		cfgexit();
		exit(1);
	}
	dspstring((CHAR *) inrec);
	switch (n) {
		case 1:
			dspstring("\n* Syntax error *\n");
			break;
		case 2:
			dspstring("\n* Invalid keyword *\n");
			break;
		case 3:
			dspstring("\n* Equal sign (=) expected *\n");
			break;
		case 4:
			dspstring("\n* One or more characters expected *\n");
			break;
		case 5:
			dspstring("\n* Unexpected end of record *\n");
			break;
		case 6:
			dspstring("\n* Invalid parameterization *\n");
			break;
		case 7:
			dspstring("\n* Keyword is duplicated *\n");
			break;
		case 8:
			dspstring("\n* Key sequence is duplicated *\n");
			break;
		default:
			dspstring("\n Unexpected error\n");
			break;
	}
}

/* USAGE */
static void usage()
{
	dspstring("TDCMP command  " RELEASEPROGRAM RELEASE COPYRIGHT);
	dspchar('\n');
	dspstring("Usage:  tdcmp file1 [file2] [-CFG=cfgfile] [-V]\n");
	exit(1);
}

/* DEATH */
static void death(INT n, INT e, CHAR *s)
{
	CHAR work[17];

	if (n < (INT) (sizeof(errormsg) / sizeof(*errormsg))) dspstring(errormsg[n]);
	else {
		mscitoa(n, work);
		dspstring("*** UNKNOWN ERROR ");
		dspstring(work);
		dspstring("***");
	}
	if (e) {
		dspstring(": ");
		dspstring(fioerrstr(e));
	}
	if (s != NULL) {
		dspstring(": ");
		dspstring(s);
	}
	dspchar('\n');
	cfgexit();
	exit(1);
}

/* QUITSIG */
static void quitsig(INT sig)
{
	signal(SIGINT, SIG_IGN);
	dspchar('\n');
	dspstring("HALTED - user interrupt");
	dspchar('\n');
	cfgexit();
	exit(1);
}
