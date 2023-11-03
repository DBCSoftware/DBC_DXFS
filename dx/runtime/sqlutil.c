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

/*  This module is provided for use with DB/C SQL modules.  The		*/
/*  routines can be used to convert DB/C variables to text strings	*/
/*  text strings to DB/C variables, and routines to expand an SQL	*/
/*  command with DB/C variables */

#define INC_CTYPE
#define INC_STRING
#define INC_STDLIB
#define INC_STDIO
#define INC_MATH
#define INC_SQL
#define INC_SQLEXT
#include "includes.h"

#include "base.h"
#include "dbc.h"

#define INC_UTIL
#include "dbcsql.h"

#define CONKEYWORD "CONNECTION "

static CHAR work[CMDSIZE];  /* work variable for build */
static CHAR swork[12];
static INT right;
static INT left;
static INT sign;

static INT sqlstrstr(CHAR *source, CHAR *findit);


/**
 * Convert a DB/C variable to ascii string
 */
INT dbctostr(UCHAR *dbcvar, CHAR *str, INT maxlen)
{
	INT i1, i2;
	INT32 lwork;
	UCHAR formvar[32];
	
	if (dbcvar == NULL) {
		str[0] = '\0';
		return(0);
	}
	if (dbcvar[0] < 0x80) {  /* small DIM */
		i1 = (INT) dbcvar[0];
		if (i1) {
			i2 = (INT)dbcvar[1] - i1 + 1;
			if (i2 > maxlen) return RC_ERROR;
			memcpy(str, &dbcvar[i1 + 2], i2);
		}
		else i2 = 0;
		str[i2] = '\0';
	}
	else if (dbcvar[0] == 0xf0) {  /* large DIM */
		i1 = dbcvar[1] + (dbcvar[2] << 8);
		if (i1) {
			i2 = dbcvar[3] + (dbcvar[4] << 8);
			if (i2 > maxlen) return RC_ERROR;
			memcpy(str, &dbcvar[i1 + 6], i2);
		}
		else i2 = 0;
		str[i2] = '\0';
	}
	else if ((dbcvar[0] & 0xe0) == 0x80) {  /* FORM */
		i2 = dbcvar[0] & 0x1f;
		if (i2 > maxlen) return RC_ERROR;
		memcpy(str, &dbcvar[1], i2);
		str[i2] = '\0';
	}
	else if (dbcvar[0] == 0xfc) {  /* INT */
		memcpy((CHAR *) &lwork, &dbcvar[2], sizeof(INT32));
		if (lwork < 0) {
			sign = 1;
			lwork = -lwork;
		}
		else sign = 0;
		swork[11] = '\0';
		i1 = 11;
		do swork[--i1] = (CHAR)(lwork % 10 + '0');
		while (lwork /= 10);
		if (sign) swork[--i1] = '-';
		strcpy(str, &swork[i1]);
	}
	else if ((dbcvar[0] & 0xfc) == 0xf8) {  /* FLOAT */
		left = (((dbcvar[0] & 0x3) << 3) + (dbcvar[1] >> 5));
		right = dbcvar[1] & 0x1f;
		if (right) i2 = left + right + 1;
		else i2 = left;
		if (i2 > maxlen) return RC_ERROR;
		nvmkform(dbcvar, formvar);
		i2 = formvar[0] & 0x1f;
		if (i2 > maxlen) return RC_ERROR;
		memcpy(str, &formvar[1], i2);
		str[i2] = '\0';
		while (str[0] == ' ') memmove(str, str + 1, strlen(str));
	}
	else if (dbcvar[0] == 0xe0 || dbcvar[0] == 0xe1) {  /* literal */
		i2 = dbcvar[1];
		if (i2 > maxlen) return RC_ERROR;
		memcpy(str, &dbcvar[2], i2);
		str[i2] = '\0';
	}
	else str[0] = '\0';
	return(0);
}

/* Convert ASCII string to a DB/C variable */
void strtodbc(CHAR *str, ULONG len, UCHAR *dbcvar)
{
	INT i1, i2, lft1, lft2, rgt1, rgt2, numflg;
	INT32 lwork;
	DOUBLE x1;

	i1 = (INT)len;
	if (dbcvar[0] < 0x80) {  /* small DIM */
		if (i1) {
			if ((ULONG)dbcvar[2] < len) i1 = (INT)dbcvar[2];
			memcpy(&dbcvar[3], str, i1);
			dbcvar[0] = 1;
			dbcvar[1] = (UCHAR) i1;
		}
		else
			dbcvar[0] = dbcvar[1] = 0;
	}
	else if (dbcvar[0] == 0xf0) {  /* large DIM */
		if (i1) {
			i2 = dbcvar[5] + (dbcvar[6] << 8);
			if (i2 < i1) i1 = i2;
			memcpy(&dbcvar[7], str, i1);
			dbcvar[1] = 1;
			dbcvar[2] = 0;
			dbcvar[3] = (UCHAR) i1;
			dbcvar[4] = (UCHAR)(i1 >> 8);
		}
		else dbcvar[1] = dbcvar[2] = dbcvar[3] = dbcvar[4] = 0;
	}
	else if ((dbcvar[0] & 0xe0) == 0x80) {  /* FORM */
		/* test for valid numeric field */
		for (lft1 = 0; str[lft1] == ' '; lft1++);
		if (str[lft1] == '-') lft1++;
		while (isdigit(str[lft1])) lft1++;
		if (str[lft1] == '.') {
			for (rgt1 = lft1 + 1; isdigit(str[rgt1]); rgt1++);
			rgt1 -= lft1;
		}
		else rgt1 = 0;
		if (i1 && lft1 + rgt1 == i1 && isdigit(str[i1 - 1])) numflg = 1;
		else numflg = 0;

		/* get size of form variable */
		for (i2 = dbcvar[0] & 0x1f, lft2 = 0; lft2 < i2 && dbcvar[lft2 + 1] != '.'; lft2++);
		if (lft2 < i2) rgt2 = i2 - lft2;
		else rgt2 = 0;

		/* zero form variable */
		memset(&dbcvar[1], ' ', lft2);
		if (rgt2) {
			dbcvar[lft2 + 1] = '.';
			memset(&dbcvar[lft2 + 2], '0', rgt2 - 1);
		}
		else dbcvar[lft2] = '0';

		/* move string to form variable */
		if (numflg) {
			i1 = i2 = 0;
			if (lft1 > lft2) i1 = lft1 - lft2;
			if (lft1 < lft2) i2 = lft2 - lft1;
			memcpy(&dbcvar[i2 + 1], &str[i1], lft2 - i2);
			if (rgt1 > rgt2) rgt1 = rgt2;
			if (rgt1) memcpy(&dbcvar[lft2 + 1], &str[lft1], rgt1);
		}
	}
	else if (dbcvar[0] == 0xfc) {  /* INT */
		lwork = atol(str);
		memcpy(&dbcvar[2], (CHAR *) &lwork, sizeof(INT32));
	}
	else if ((dbcvar[0] & 0xfc) == 0xf8) {  /* FLOAT */
		x1 = atof(str);
		memcpy(&dbcvar[2], (CHAR *) &x1, sizeof(DOUBLE));
	}
}

/*
 * Place a null value in a DB/C variable
 */
void dbcsetnull(UCHAR *dbcvar)
{
	INT i1, i2;
	INT decflag = 0;

	if (dbcvar[0] < 0x80) {  /* small char */
		memset(&dbcvar[3], ' ', dbcvar[2]);
		dbcvar[0] = 0;
		dbcvar[1] = 0xFF;
	}
	else if (dbcvar[0] == 0xF0) {  /* large char */
		i2 = dbcvar[5] + (dbcvar[6] << 8);
		memset(&dbcvar[7], ' ', i2);
		dbcvar[1] = dbcvar[2] = 0;
		dbcvar[3] = dbcvar[4] = 0xFF;
	}
	else if ((dbcvar[0] >= 0x80 && dbcvar[0] < 0xA0) || (dbcvar[0] >= 0xF8 && dbcvar[0] <= 0xFC)) {  /* num, float, int */
		if (dbcvar[0] == 0xFC) { /* int */
			memset(&dbcvar[2], 0, 4);
		}
		else if (dbcvar[0] >= 0xF8) { /* float */
			memset(&dbcvar[2], 0, 8);
		}
		else {  /* form */
			i2 = dbcvar[0] & 0x1F;
			decflag = 0;
			for (i1 = 1; i1 <= i2; i1++) {
				if (dbcvar[i1] == '.') {
					decflag = i1;
					continue;
				}
				dbcvar[i1] = '0';
			}
			if (decflag) {
				for (i1 = 1; i1 < (decflag - 1); i1++) dbcvar[i1] = ' ';
			}
			else {
				for (i1 = 1; i1 < i2; i1++) dbcvar[i1] = ' ';
			}

		}
		if (dbcvar[0] < 0xA0) {
			if (dbcvar[1] != '.') dbcvar[1] = dbcvar[0] & 0x1F;
			else dbcvar[1] = dbcvar[0];
			dbcvar[0] = 0x80;
		}
		else if (dbcvar[0] == 0xFC) dbcvar[1] |= 0x80;
		else dbcvar[0] -= 0x04;
	}
	return;
}

/*
 * check if a DB/C variable is null
 */
INT dbcisnull(UCHAR *dbcvar)
{
	if (dbcvar[0] < 0x80) {  /* small char */
		if (dbcvar[0] == 0x00 && dbcvar[1] == 0xFF) return 1;
	}
	else if (dbcvar[0] < 0xA0) {  /* num */
		if (dbcvar[0] == 0x80) return 1;
	}
	else if (dbcvar[0] == 0xF0) {  /* large char */
		if (dbcvar[1] == 0x00 && dbcvar[2] == 0x00 && dbcvar[3] == 0xFF && dbcvar[4] == 0xFF) return 1;
	}
	else if (dbcvar[0] == 0xFC) {  /* int */
		if (dbcvar[1] & 0x80) return 1;
	}
	else if (dbcvar[0] >= 0xF4 && dbcvar[0] <= 0xF7) return 1;  /* null float */
	return 0;
}

 /*
  * Remove USING <cursorname> from cmdstr
  * copy <cursorname> to cname
  * Return 1 if USING <cursorname> found, else Return 0
  *
  * Note to self: Don wants 'OF' to stay in here.
  *   The Delete statement might have 'WHERE CURRENT [OF|USING] cursor_name'
  *   'OF cursor_name' is put back into the DELETE command in dbcsqlx.
  *   Standard SQL recognizes a 'WHERE CURRENT OF cursor_name' on a Delete.
  */
INT getcursorname(CHAR *cmdstr, CHAR *cname, INT cbCnameLength)
{
	INT i1, i2, len, upos, length;

	length = 6;
	upos = sqlstrstr(cmdstr, "USING ");
	if (upos == 0) {
		length = 3;
		upos = sqlstrstr(cmdstr, "OF ");
	}

	if (upos == 0 || !isspace(cmdstr[upos - 1])) {
		/* USING <cursorname> not found so use table_cursor */
		strncpy(cname, TABLE_CURSOR_NAME, cbCnameLength - 1);
		return 0;
	}

	i1 = upos + length;
	while (cmdstr[i1] == ' ') i1++;
	i2 = 0;
	while (cmdstr[i1] && cmdstr[i1] != ' ') {
		if (i2 < cbCnameLength - 1) cname[i2++] = cmdstr[i1];
		i1++;
	}
	cname[i2] = '\0';
	len = (INT)strlen(&cmdstr[i1]);
	if (len > 0) {
		memmove(&cmdstr[upos], &cmdstr[i1], len);
		cmdstr[upos + len] = '\0';
	}
	else cmdstr[upos] = '\0';
	return 1;
}

 /* Remove CONNECTION <connectionname> from cmdstr */
 /* copy <connectionname> to conname */
 /* Return 1 if found, else Return 0 */
INT getconnectionname(CHAR *cmdstr, CHAR *conname)
{
	INT i1, len, upos;
	CHAR* pconname;
	
	pconname = conname;

	upos = sqlstrstr(cmdstr, CONKEYWORD);
	if (upos == 0) {
		/* CONNECTION <connectionname> not found so use default_no_name_conn */
		strcpy(conname, "default_no_name_conn");
		return 0;
	}
	
	i1 = upos + (INT)strlen(CONKEYWORD);
	while(cmdstr[i1] == ' ') i1++;
	if (cmdstr[i1] == '=') {
		i1++;
		while(cmdstr[i1] == ' ') i1++;
	}
	while (cmdstr[i1] && cmdstr[i1] != ' ') {
		*conname = cmdstr[i1];
		conname++;
		i1++;
	}
	*conname = '\0';
	len = (INT)strlen(&cmdstr[i1]);
	if (len > 0) {
		memmove(&cmdstr[upos], &cmdstr[i1], len);
		cmdstr[upos + len] = '\0';
	}
	else cmdstr[upos] = '\0';
	
	return 1;
}

/* Variable substitution */
INT buildsql(CHAR *cmdline, UCHAR *arglist[])
{
	INT cmdpos, wrkpos, argcnt, num;

	for (argcnt = 0; arglist[argcnt] != NULL; argcnt++);
	cmdpos = wrkpos = 0;
	while (cmdline[cmdpos]) {
		/* A backslash followed by a colon is important, remove the backslash and leave the colon and do not
		 * interpret the colon followed by a number as a replacement field
		 */
		if (cmdline[cmdpos] == '\\' && cmdline[cmdpos + 1] == ':') {
			cmdpos++; // cmdpos now pointing at the colon
			work[wrkpos++] = cmdline[cmdpos++]; // move colon to dest and move past it
		}
		/*if (cmdline[cmdpos] == '\\') {
			oe = 0;
			while (cmdline[cmdpos] == '\\') {
				cmdpos++;
				if (oe++ % 2) {
					work[wrkpos++] = '\\';
				}
			}
			if (cmdline[cmdpos] && (oe % 2)) {
				work[wrkpos++] = cmdline[cmdpos++];
			}
		}*/
		else if (cmdline[cmdpos] == ':' && isdigit(cmdline[cmdpos + 1]) && argcnt) {
			cmdpos++;
			for (num = 0; isdigit(cmdline[cmdpos]); ) {
				num = num * 10 + cmdline[cmdpos++] - '0';
			}
			num--;
			if (num >= 0 && num < argcnt) {
				if (dbcisnull(arglist[num])) {
					if (wrkpos && work[wrkpos - 1] == '\'' && cmdline[cmdpos] == '\'') {
						wrkpos--;
						cmdpos++;
					}
					strcpy(&work[wrkpos], "NULL");
				}
				else
				if (dbctostr(arglist[num], &work[wrkpos], CMDSIZE - wrkpos) == -1) {
					return RC_ERROR;
				}
				wrkpos += (INT)strlen(&work[wrkpos]);
			}
		}
		else work[wrkpos++] = cmdline[cmdpos++];
		if (cmdpos >= CMDSIZE || wrkpos >= CMDSIZE) return RC_ERROR;
	}
	work[wrkpos] = '\0';
	strcpy(cmdline, work);
	return(wrkpos);
}

/* Find a substring in a larger string
 *
 * Skips quoted strings
 */
static INT sqlstrstr(CHAR *source, CHAR *findit)
{
	enum State {Normal, singleQuote, doubleQuote};
	INT i1, len, upos;
	enum State state = Normal;

	len = (INT)(strlen(source) - strlen(findit));
	for (upos = 1; upos <= len; upos++) {
		if (source[upos] == '\\') {
			upos++;
			continue;
		}
		switch (state) {
		case singleQuote:
			if (source[upos] == '\'') state = Normal;
			break;
		case doubleQuote:
			if (source[upos] == '"') state = Normal;
			break;
		case Normal:
			switch (source[upos]) {
			case '\'':
				state = singleQuote;
				break;
			case '\"':
				state = doubleQuote;
				break;
			default:
				if (toupper(source[upos]) == findit[0]) {
					i1 = 0;
					do {
						if (!findit[++i1]) return upos;
					}
					while (toupper(source[upos + i1]) == findit[i1]);
				}
				break;
			}
			break;
		}
	}
	return 0;
}

/**
 * This is a cut-and-paste of the same routine in dbcmsc.c
 * For unknown reasons we need this on Mac but not on other Unixes??
 * And now on Linux too!?
 *
 * This function defined for Linux on 14 NOV 2007 and was in the 15.0.2 release of 03 APR 2008
 */
#if defined(__MACOSX) || defined(Linux)
void nvmkform(UCHAR *nvar, UCHAR *formvar)  /* make a form variable from the numeric variable at nvar */
/* set fp, lp, pl, hl */
{
	INT i1, i2, i3, lft, rgt, zeroflg;
	/*INT32 x1;*/
	CHAR work[64];
	CHAR *ptr;
	DOUBLE64 y1;

	if (*nvar >= 0xF4 && *nvar <= 0xFB) {  /* float variable */
		zeroflg = TRUE;
		lft = ((nvar[0] << 3) & 0x18) | (nvar[1] >> 5);
		rgt = nvar[1] & 0x1F;
		memcpy((UCHAR *) &y1, &nvar[2], sizeof(DOUBLE64));
		if (y1 == (DOUBLE64) 0.0) { /* XENIX and UNIX386 don't handle fcvt(0.0) properly */
			i2 = i3 = 0;
			ptr = "0000000000000000000000000000000";
		}
		else ptr = (CHAR *) fcvt((double) y1, rgt, &i2, &i3);
		/* this fixes bug with watcom fcvt, but do it also for everyone else */
		i1 = strlen(ptr);
		if (i1 - i2 < rgt) {
			memcpy(work, ptr, i1);
			memset(&work[i1], '0', rgt - (i1 - i2));
			work[rgt + i2] = 0;
			ptr = work;
		}

		i1 = (i2 < 0 ? 0 : i2);
		if (lft) {
			if (i1 < lft) {
				if (i3) i3 = lft - i1;
				memset(&formvar[1], ' ', lft - i1);
				if (i1) {
					memcpy(&formvar[lft - i1 + 1], ptr, i1);
					zeroflg = FALSE;
				}
				else if (!rgt) formvar[lft] = '0';
			}
			else {
				memcpy(&formvar[1], &ptr[i1 - lft], lft);
				i3 = 0;
			}
		}
		ptr += i1;
		if (rgt) {
			formvar[++lft] = '.';
			if (i2 < 0) {
				i2 = -i2;
				if (i2 > rgt) i2 = rgt;
				memset(&formvar[lft + 1], '0', i2);
				lft += i2;
				rgt -= i2;
			}
			while (rgt--) {
				if (*ptr != '0') zeroflg = FALSE;
				formvar[++lft] = *ptr++;
			}
		}
		if (!zeroflg && i3) formvar[i3] = '-';
	}
	else {  /* shouldn't happen */
		lft = 1;
		formvar[1] = '0';
	}

	formvar[0] = (UCHAR) (0x80 | lft);
	return;
}
#endif
