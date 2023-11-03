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
#define INC_TIME
#define INC_SIGNAL
#define INC_ERRNO
#include "includes.h"
#include "arg.h"
#include "chain.h"
#include "base.h"
#include "evt.h"
#include "fio.h"
#include "tim.h"

#if UNIX
#include <sys/types.h>
#endif


#define LOGIC_OR		0x0001
#define LOGIC_AND		0x0002
#define LOGIC_NOT		0x0003
#define LOGIC_EQ		0x0004
#define LOGIC_LT		0x0005
#define LOGIC_GT		0x0006
#define LOGIC_NEQ		0x0007
#define LOGIC_LTE		0x0008
#define LOGIC_GTE		0x0009

#define CHAR_CONCATE	0x0010
#define CHAR_SUBSTR		0x0020
#define CHAR_SUBSTRX	0x0030
#define CHAR_LENGTH		0x0040
#define CHAR_PMATCH		0x0050
#define CHAR_SCAN		0x0060
#define CHAR_NOTSCAN	0x0070
#define CHAR_REPLACE	0x0080

#define MATH_ADD		0x0100
#define MATH_SUB		0x0200
#define MATH_MLT		0x0300
#define MATH_DIV		0x0400
#define MATH_MOD		0x0500
#define MATH_NEG		0x0600

#define FUNC_FFILE		0x0800
#define FUNC_GETPOS		0x1000
#define FUNC_GETLASTPOS	0x1800
#define FUNC_CLOCK		0x2000

#define OPER_LEFTPRN	0x4000
#define OPER_RGTPRN		0x8000
#define OPER_SYMBOL		0xc000

#define LOGIC_MASK		0x000f
#define CHAR_MASK		0x00f0
#define MATH_MASK		0x0700
#define FUNC_MASK		0x3800
#define OPER_MASK		0xc000

#define TYPE_CHAR	0x01
#define TYPE_DEC	0x02
#define TYPE_FILE	0x04

#define SYMINIT 256
#define SYMINC 128
#define LABINIT 128
#define LABINC 256
#define EXPMAX 512
#define LOOPMAX 32
#define INCMAX 16
#define STRMAX 80
#define LINEMAX 256
#define RIOBUFS 1

struct symdef {
	UCHAR typeflg;
	INT boolflg;
	CHAR name[SYMNAMSIZ + 1];
	CHAR string[STRMAX + 1];
};

struct labdef {
	CHAR name[SYMNAMSIZ + 1];
	UCHAR foundflg;
	OFFSET pos;
};

static UCHAR **symptr;
static struct symdef *sym;
static INT symcnt, symcnt2;
static INT symconst;
static INT symmax;
static UCHAR **labptr;
static struct labdef *lab;
static INT labcnt;
static INT labmax;
static UINT explist[EXPMAX];
static INT expcnt;
static OFFSET looppos[LOOPMAX];
static INT looplist[INCMAX];
static INT loopcnt;
static INT inclist[INCMAX];
static INT inccnt;
static INT chnhndl;
static UINT linelist[INCMAX];
static UINT linecnt;
static CHAR record[RECMAX + 4];
static UCHAR **bufptr = NULL;
static INT openflg;
static INT wrkhndl;
static OFFSET nextpos, eofpos;
static INT displayflg;
static INT debugflg;
static INT caseflg;
static INT discardflg;
static CHAR *directives[] = {
	"BORT",
	"BORTOFF",
	"BORTON",
	"BTIF",
	"SSIGN",
	"EEP",
	"LICK",
	"LOSE",
	"EBUG",
	"ISCARD",
	"ISPLAY",
	"O",
	"LSE",
	"LSEIF",
	"ND",
	"RRABORT",
	"RRGOTO",
	"OTO",
	"F",
	"NCLUDE",
	"EYBOARD",
	"EYIN",
	"ABEL",
	"OG",
	"OGOFF",
	"OABORT",
	"PEN",
	"PENREAD",
	"OSITION",
	"EAD",
	"EWRITE",
	"ET",
	"OUND",
	"OUNDR",
	"TAMP",
	"TOP",
	"YSTEMOFF",
	"YSTEMON",
	"ERMINATE",
	"NTIL",
	"AIT",
	"HILE",
	"RITE",
	"IF"
};
static CHAR directmap[] = {
	0, 5, 6, 8, 12, 17, 17, 18, 18, 20,
	20, 22, 25, 25, 26, 28, 29, 29, 31, 38,
	39, 40, 40, 43, 44, 44, 44
};

static void chninit(CHAR *);
static INT replace(INT);
static INT control(CHAR *);
static void expinit(CHAR *);
static void expeval(INT);
static void expevalx(INT);
static INT expvalue(CHAR *);
static void getsym(INT, INT *, INT *, OFFSET *, CHAR *);
static INT getstr(CHAR *, CHAR **);
static INT getnum(CHAR *);
static INT getfile(CHAR *);
static void chnofftoo(OFFSET, CHAR *);
static void chn10tou(CHAR *, UINT *);
static void xstatement(CHAR *, INT);
static CHAR *skiplines(INT);
static void vexec(INT);
static void vabort(void);
static void vassign(CHAR *);
static void vbeep(void);
static void vclose(CHAR *);
static void vdebug(CHAR *);
static void vdiscard(CHAR *);
static void vdisplay(CHAR *);
static void vdo(void);
static void velse(void);
static void vend(void);
static void vif(CHAR *);
static void vinclude(CHAR *);
static void vkeyboard(CHAR *);
static void vkeyin(CHAR *);
static void vlabel(CHAR *, INT);
static void vlog(CHAR *);
static void vopen(CHAR *, INT);
static void vposition(CHAR *);
static void vread(CHAR *);
static void vrewrite(CHAR *);
static void vset(CHAR *);
static void vsound(CHAR *);
static void vsoundr(CHAR *);
static void vstop(CHAR *);
static void vuntil(CHAR *);
static void vwait(CHAR *);
static void vwhile(CHAR *);
static void vwrite(CHAR *);
static void moresyms(void);
static void morelabs(void);
static void deatha(INT);


INT compile(CHAR *workname)
{
	INT i1, i2;

	/* initialize variables, open chain file and create/open work file */
	chninit(workname);

	sym = (struct symdef *) *symptr;
	lab = (struct labdef *) *labptr;
	while (TRUE) {
		i1 = rioget(chnhndl, (UCHAR *) record, RECMAX);
		if (i1 < 0) {
			if (i1 == -1) {
				if (loopcnt && (!inccnt || loopcnt != looplist[inccnt - 1])) death(DEATH_NOLOOP, 0, fioname(chnhndl), 1);
				rioclose(chnhndl);
				if (!inccnt--) break;
				chnhndl = inclist[inccnt];
				loopcnt = looplist[inccnt];
				linecnt = linelist[inccnt];
				continue;
			}
			if (i1 == -2) continue;
			death(DEATH_READ, i1, fioname(chnhndl), 1);
		}
		linecnt++;
		i1 = replace(i1);
		if (!i1) continue;
		if (displayflg) {
			dspstring(record);
			dspchar('\n');
		}
		if (i1 >= 2 && record[0] == '/') {
			if (record[1] == '/' || record[1] == '@') {
				if (control(&record[2])) {
					while (TRUE) {
						rioclose(chnhndl);
						if (!inccnt--) break;
						chnhndl = inclist[inccnt];
					}
					break;
				}
				continue;
			}
			if (record[1] == '.') {
				dspstring(&record[2]);
				dspchar('\n');
				continue;
			}
			if (record[1] == '&') continue;
			if (record[1] == ':' || record[1] == '*') {
				xstatement(&record[1], i1 - 1);
				continue;
			}
		}
		memmove(&record[1], record, i1);
		record[0] = COMMAND;
		xstatement(record, i1 + 1);
	}

	/* close open files */
	for (i1 = symconst; i1 < symcnt; i1++) {
		if (sym[i1].typeflg == TYPE_FILE) {
			i2 = rioclose(sym[i1].boolflg);
			if (i2) death(DEATH_CLOSE, i2, NULL, 1);
		}
	}

	/* test for undefined labels */
	for (i1 = i2 = 0; i1 < labcnt; i1++) {
		if (!lab[i1].foundflg) {
			dspstring("Undefined label: ");
			dspstring(lab[i1].name);
			dspchar('\n');
			i2 = 1;
		}
	}
	if (i2) {
		dspstring("CHAIN compilation aborted\n");
		exit(1);
	}

	record[0] = END_OF_EXTENT;
	mscoffto9(nextpos, (UCHAR *) &record[1]);
	mscoffto9(eofpos, (UCHAR *) &record[10]);
	xstatement(record, EOFSIZ);

	nextpos = eofpos;
	rionextpos(wrkhndl, &eofpos);
	riosetpos(wrkhndl, 0);
	i1 = rioget(wrkhndl, (UCHAR *) record, RECMAX);
	if (i1 != HDRSIZ) death(DEATH_INVWORK, 0, fioname(wrkhndl), 1);
	if (record[0] != EXECUTING) record[0] = COMPILED;
	mscoffto9(nextpos, (UCHAR *) &record[1]);
	mscoffto9(eofpos, (UCHAR *) &record[10]);
	riosetpos(wrkhndl, 0);
	xstatement(record, HDRSIZ);
	i1 = rioclose(wrkhndl);
	if (i1) death(DEATH_CLOSE, i1, NULL, 1);
	memfree(bufptr);
	memfree(labptr);
	memfree(symptr);
	if (record[0] == EXECUTING) return(COMPILE);
	return(0);
}

static void chninit(CHAR *workname)
{
	INT i1, i2, systemflg;
	UINT lwork1, lwork2;
	CHAR c1, chnname[100], *ptr;

	symptr = memalloc(SYMINIT * sizeof(struct symdef), 0);
	labptr = memalloc(LABINIT * sizeof(struct labdef), 0);
	if (symptr == NULL || labptr == NULL) death(DEATH_INIT, ERR_NOMEM, NULL, 1);
	symmax = SYMINIT;
	labmax = LABINIT;

	sym = (struct symdef *) *symptr;
	strcpy(sym[0].name, "FALSE");
	strcpy(sym[1].name, "NO");
	strcpy(sym[2].name, "OFF");
	strcpy(sym[3].name, "TRUE");
	strcpy(sym[4].name, "YES");
	strcpy(sym[5].name, "ON");
	strcpy(sym[6].name, "NULL");
	for (i1 = 0; i1 < 7; i1++) {
		sym[i1].typeflg = TYPE_CHAR;
		if (i1 < 3) sym[i1].boolflg = FALSE;
		else sym[i1].boolflg = TRUE;
		sym[i1].string[0] = 0;
	}
	symcnt = symconst = 7;

	ptr = record;
	caseflg = debugflg = displayflg = systemflg = FALSE;
	openflg = RIO_M_ERO | RIO_P_TXT | RIO_T_ANY;
	chnname[0] = 0;

	/* scan chain name */
	i1 = argget(ARG_FIRST, chnname, sizeof(chnname));
	if (i1 < 0) death(DEATH_INIT, i1, NULL, 1);
	if (i1 == 1) usage();

	/* get other parameters */
	for ( ; ; ) {
		i1 = argget(ARG_NEXT, ptr, sizeof(record));
		if (i1) {
			if (i1 < 0) death(DEATH_INIT, i1, NULL, 1);
			break;
		}
		if (ptr[0] == '-') {
			switch(toupper(ptr[1])) {
				case '!':
					break;
				case 'C':
					break;
				case 'D':
					displayflg = TRUE;
					break;
				case 'I':
					caseflg = TRUE;
					break;
				case 'J':
					if (toupper(ptr[2]) == 'R') openflg = RIO_M_SRO | RIO_P_TXT | RIO_T_ANY;
					else openflg = RIO_M_SHR | RIO_P_TXT | RIO_T_ANY;
					break;
				case 'N':
					break;
				case 'P':
					break;
				case 'S':
					systemflg = TRUE;
					break;
				case 'T':
					debugflg = TRUE;
					break;
				case 'V':
					break;
				case 'W':
					break;
				default:
					death(DEATH_INVPARM, 0, ptr, 1);
			}
		}
		else if (isalpha(ptr[0])) {
			for (i1 = 0; ptr[i1] && ptr[i1] != '='; i1++);
			c1 = ptr[i1];
			if (i1 > SYMNAMSIZ) {
				ptr[SYMNAMSIZ - 1] = ptr[i1 - 1];
				ptr[SYMNAMSIZ] = 0;
			}
			else ptr[i1] = 0;

			if (symcnt == symmax) moresyms();
			sym[symcnt].typeflg = TYPE_CHAR;
			sym[symcnt].boolflg = TRUE;
			strcpy(sym[symcnt].name, ptr);
			if (c1 == '=') {
				i2 = (INT)strlen(&ptr[++i1]);
				if (i2 > STRMAX) i2 = STRMAX;
				memcpy(sym[symcnt].string, &ptr[i1], i2);
			}
			else i2 = 0;
			sym[symcnt].string[i2] = 0;
			symcnt++;
		}
		else death(DEATH_INVPARM, 0, ptr, 1);
	}

	if (!caseflg) {  /* make lower case equavalents of reserved names */
		if (symcnt + symconst > symmax) moresyms();
		memmove((CHAR *) &sym[symconst], (CHAR *) sym, symcnt * sizeof(*sym));
		symcnt += symconst;
		for (i1 = symconst << 1; symconst < i1; symconst++) {
			for (ptr = sym[symconst].name; *ptr; ptr++) *ptr = (CHAR) tolower(*ptr);
		}
	}
	else {  /* convert symbols to upper case */
		for (i1 = symconst; i1 < symcnt; i1++) {
			for (ptr = sym[i1].name; *ptr; ptr++) *ptr = (CHAR) toupper(*ptr);
		}
	}

	discardflg = FALSE;
	loopcnt = inccnt = 0;
	linecnt = 0;

	miofixname(chnname, ".chn", FIXNAME_EXT_ADD);
	chnhndl = rioopen(chnname, openflg, RIOBUFS + 1, RECMAX);
	if (chnhndl < 0) death(DEATH_OPEN, chnhndl, chnname, 1);

	wrkhndl = rioopen(workname, RIO_M_EXC | RIO_P_WRK | RIO_T_STD | RIO_UNC, RIOBUFS + 1, RECMAX);
	if (wrkhndl < 0 && wrkhndl != ERR_FNOTF) death(DEATH_OPEN, wrkhndl, workname, 1);
	if (wrkhndl != ERR_FNOTF) {
		i1 = rioget(wrkhndl, (UCHAR *) record, RECMAX);
		lwork1 = (UINT) time((time_t *) NULL);
		chn10tou(&record[28], &lwork2);
		if (i1 == HDRSIZ && record[0] == EXECUTING && lwork1 < lwork2 + 15) {
			/* nested chain */
			msc9tooff((UCHAR *) &record[1], &nextpos);
			msc9tooff((UCHAR *) &record[10], &eofpos);
		}
		else {
			rioclose(wrkhndl);
			wrkhndl = ERR_FNOTF;
		}
	}
	if (wrkhndl == ERR_FNOTF) {
		wrkhndl = rioopen(workname, RIO_M_PRP | RIO_P_WRK | RIO_T_STD | RIO_UNC, RIOBUFS + 1, RECMAX);
		if (wrkhndl < 0) death(DEATH_CREATE, wrkhndl, workname, 1);
		nextpos = 999999999;
		eofpos = HDRSIZ + 1;
		record[0] = COMPILING;
		mscoffto9(nextpos, (UCHAR *) &record[1]);
		mscoffto9(eofpos, (UCHAR *) &record[10]);
		memset(&record[19], '9', 9);
		memset(&record[28], '0', 10);
		xstatement(record, HDRSIZ);
	}
	else {
		riosetpos(wrkhndl, eofpos);
		vexec(ERRABORT);
		if (systemflg) vexec(SYSTEMON);
	}
}

static INT replace(INT reclen)
{
	INT i1, i2, i3, i4, bool, len, type, chgflg;
	OFFSET num;
	CHAR c1, c2, work[LINEMAX + 1], name[SYMNAMSIZ + 1], str[STRMAX + 1], *ptr;

	while (reclen && isspace(record[reclen - 1])) reclen--;
	record[reclen] = 0;
	if (reclen >= 2 && record[0] == '/') {
		if (record[1] == '&' || record[1] == '$' || record[1] == '@') return(reclen);
	}
		
	while (TRUE) {
		chgflg = FALSE;
		for (i1 = i2 = 0; record[i1]; i1++) {
			c1 = record[i1];
			if (c1 == '#' || c1 == '$' || c1 == '%') {
				for (i3 = i1 + 1; record[i3] && c1 != record[i3]; i3++);
				if (record[i3] && i3 > i1 + 1) {
					len = i3 - i1 - 1;
					if (len > SYMNAMSIZ) {
						memcpy(name, &record[i1 + 1], SYMNAMSIZ - 1);
						name[SYMNAMSIZ - 1] = record[i3 - 1];
						name[SYMNAMSIZ] = 0;
					}
					else {
						memcpy(name, &record[i1 + 1], len);
						name[len] = 0;
					}
					c2 = name[0];
					for (i4 = 0; i4 < symcnt && (!sym[i4].typeflg || c2 != sym[i4].name[0] || strcmp(name, sym[i4].name)); i4++);
					if (i4 < symcnt) {
						if (sym[i4].boolflg) {
							getsym(i4, &type, &bool, &num, str);
							if (c1 == '$') mscofftoa(num, str);
							else if (c1 == '%') chnofftoo(num, str);
							ptr = str;
							len = (INT)strlen(ptr);
							if (len == i3 - i1 - 1 && !memcmp(&record[i1], ptr, len)) deatha(DEATH_CIRCULAR);
							chgflg = TRUE;
						}
						else {
							ptr = &record[i1];
							len += 2;
						}
						if (len > LINEMAX - i2) len = LINEMAX - i2;
						memcpy(&work[i2], ptr, len);
						i2 += len;
						i1 = i3;
						continue;
					}
				}
			}
			if (i2 < LINEMAX) work[i2++] = c1;
		}
		if (!chgflg) break;
		memcpy(record, work, i2);
		record[i2] = 0;
		reclen = i2;
	}

	return(reclen);
}

static INT control(CHAR *rec)
{
	INT i1, i2, i3;
	CHAR work[33], *ptr;

	for (i1 = 0; isspace(rec[i1]); i1++);
	for (i2 = i1, i3 = 0; rec[i2] && !isspace(rec[i2]); i2++) work[i3++] = (CHAR) toupper(rec[i2]);
	work[i3] = 0;
	while (isspace(rec[i2])) i2++;
	ptr = &rec[i2];
	i2 = work[0];
	if (i2 < 'A' || i2 > 'Z') deatha(DEATH_INVDIRECT);
	i2 -= 'A';
	i1 = directmap[i2];
	i2 = directmap[i2 + 1];
	while (i1 < i2 && strcmp(&work[1], directives[i1])) i1++;
	if (i1 == i2) deatha(DEATH_INVDIRECT);
	switch (i1) {
		case 0:  /* ABORT  compile-time directive */
			vabort();
			break;
		case 1:  /* ABORTOFF  execution-time directive */
			vexec(ABORTOFF);
			break;
		case 2:  /* ABORTON  execution-time directive */
			vexec(ABORTON);
			break;
		case 3:  /* ABTIF  execution-time directive */
			vexec(ABTIF);
			break;
		case 4:  /* ASSIGN  compile-time directive */
			vassign(ptr);
			break;
		case 5:  /* BEEP  compile-time directive */
		case 6:  /* CLICK  compile-time directive */
			vbeep();
			break;
		case 7:  /* CLOSE  compile-time directive */
			vclose(ptr);
			break;
		case 8:  /* DEBUG  compile-time directive */
			vdebug(ptr);
			break;
		case 9:  /* DISCARD  compile-time directive */
			vdiscard(ptr);
			break;
		case 10:  /* DISPLAY  compile-time directive */
			vdisplay(ptr);
			break;
		case 11:  /* DO  compile-time directive */
			vdo();
			break;
		case 12:  /* ELSE  compile-time directive */
		case 13:  /* ELSEIF  compile-time directive */
			velse();
			break;
		case 14:  /* END  compile-time directive */
			vend();
			break;
		case 15:  /* ERRABORT  execution-time directive */
			vexec(ERRABORT);
			break;
		case 16:  /* ERRGOTO  execution-time directive */
			vlabel(ptr, ERRGOTO);
			break;
		case 17:  /* GOTO  execution-time directive */
			vlabel(ptr, GOTO);
			break;
		case 18:  /* IF  compile-time directive */
			vif(ptr);
			break;
		case 19:  /* INCLUDE  compile-time directive */
			vinclude(ptr);
			break;
		case 20:  /* KEYBOARD  execution-time directive */
			vkeyboard(ptr);
			break;
		case 21:  /* KEYIN  compile-time directive */
			vkeyin(ptr);
			break;
		case 22:  /* LABEL  execution-time directive */
			vlabel(ptr, LABEL);
			break;
		case 23:  /* LOG   execution-time directive */
			vlog(ptr);
			break;
		case 24:  /* LOGOFF   execution-time directive */
			vexec(LOGOFF);
			break;
		case 25:  /* NOABORT  execution-time directive */
			vexec(NOABORT);
			break;
		case 26:  /* OPEN  compile-time directive */
			vopen(ptr, RIO_M_EXC | RIO_P_TXT | RIO_T_ANY);
			break;
		case 27:  /* OPENREAD  compile-time directive */
			vopen(ptr, RIO_M_SRO | RIO_P_TXT | RIO_T_ANY);
			break;
		case 28:  /* POSITION  compile-time directive */
			vposition(ptr);
			break;
		case 29:  /* READ  compile-time directive */
			vread(ptr);
			break;
		case 30:  /* REWRITE  compile-time directive */
			vrewrite(ptr);
			break;
		case 31:  /* SET  compile-time directive */
			vset(ptr);
			break;
		case 32:  /* SOUND  compile-time directive */
			vsound(ptr);
			break;
		case 33:  /* SOUNDR  execution-time directive */
			vsoundr(ptr);
			break;
		case 34:  /* STAMP  execution-time directive */
			vexec(STAMP);
			break;
		case 35:  /* STOP  compile-time directive */
			vstop(ptr);
			return(1);
		case 36:  /* SYSTEMOFF  execution-time directive */
			vexec(SYSTEMOFF);
			break;
		case 37:  /* SYSTEMON  execution-time directive */
			vexec(SYSTEMON);
			break;
		case 38:  /* TERMINATE  execution-time directive */
			vexec(TERMINATE);
			break;
		case 39:  /* UNTIL  compile-time directive */
			vuntil(ptr);
			break;
		case 40:  /* WAIT  compile-time directive */
			vwait(ptr);
			break;
		case 41:  /* WHILE  compile-time directive */
			vwhile(ptr);
			break;
		case 42:  /* WRITE  compile-time directive */
			vwrite(ptr);
			break;
		case 43:  /* XIF  compile-time directive */
			break;
	}
	return(0);
}

static void expinit(CHAR *rec)
{
	INT i1, i2, i3, expval, prncnt;
	CHAR c1, name[SYMNAMSIZ + 1], work[SYMNAMSIZ + 1];

	expcnt = 0;
	symcnt2 = symcnt;

	prncnt = 0;
	for (i1 = 0; rec[i1]; ) {
		c1 = rec[i1++];
		if (isalpha(c1)) {  /* symbol or function */
			for (i2 = i1--; isalnum(rec[i2]); i2++);
			i3 = i2 - i1;
			if (i3 > SYMNAMSIZ) {
				memcpy(name, &rec[i1], SYMNAMSIZ - 1);
				name[SYMNAMSIZ - 1] = rec[i2 - 1];
				name[SYMNAMSIZ] = 0;
			}
			else {
				memcpy(name, &rec[i1], i3);
				name[i3] = 0;
			}
			for (i3 = 0; name[i3]; i3++) work[i3] = (CHAR) toupper(name[i3]);
			work[i3] = 0;
			if (caseflg) strcpy(name, work);
			if (!strcmp(work, "FFILE")) expval = FUNC_FFILE;
			else if (!strcmp(work, "GETPOS")) expval = FUNC_GETPOS;
			else if (!strcmp(work, "GETLASTPOS")) expval = FUNC_GETLASTPOS;
			else if (!strcmp(work, "CLOCK")) expval = FUNC_CLOCK;
			else {
				c1 = name[0];
				for (i3 = 0; i3 < symcnt && (!sym[i3].typeflg || c1 != sym[i3].name[0] || strcmp(name, sym[i3].name)); i3++);
				if (i3 < symcnt) expval = OPER_SYMBOL | i3;  /* found symbol */
				else {  /* create temporary null character symbol */
					if (symcnt2 == symmax) moresyms();
					sym[symcnt2].typeflg = TYPE_CHAR;
					sym[symcnt2].boolflg = FALSE;
					sym[symcnt2].string[0] = 0;
					strcpy(sym[symcnt2].name, name);
					expval = OPER_SYMBOL | symcnt2++;
				}
			}
			i1 = i2;
		}
		else if (isdigit(c1)) {  /* numeric string */
			for (i2 = i1--; isdigit(rec[i2]); i2++);
			if (isalpha(rec[i2])) deatha(DEATH_INVNUM);
			i3 = i2 - i1;
			if (symcnt2 == symmax) moresyms();
			sym[symcnt2].typeflg = TYPE_CHAR;
			sym[symcnt2].boolflg = TRUE;
			sym[symcnt2].name[0] = 0;
			memcpy(sym[symcnt2].string, &rec[i1], i3);
			sym[symcnt2].string[i3] = 0;
			expval = OPER_SYMBOL | symcnt2++;
			i1 = i2;
		}
		else if (isspace(c1) || c1 == ',') continue;
		else if (c1 == ';') break;  /* start of comment */
		else {  /* operator */
			switch (c1) {
				case '"':
					if (symcnt2 == symmax) moresyms();
					for (i2 = i1, i3 = 0; rec[i2] && rec[i2] != '"'; i2++) {
						if (rec[i2] == '#' && (rec[i2 + 1] == '"' || rec[i2 + 1] == '#')) i2++;
						if (i3 < STRMAX) sym[symcnt2].string[i3++] = rec[i2];
					}
					if (!rec[i2]) deatha(DEATH_NOQUOTE);
					sym[symcnt2].typeflg = TYPE_CHAR;
					sym[symcnt2].boolflg = TRUE;
					sym[symcnt2].name[0] = 0;
					sym[symcnt2].string[i3] = 0;
					i1 = i2 + 1;
					expval = OPER_SYMBOL | symcnt2++;
					break;
				case '&':
					expval = LOGIC_AND;
					break;
				case '(':
					prncnt++;
					expval = OPER_LEFTPRN;
					break;
				case ')':
					if (!prncnt) deatha(DEATH_NOPAREN);
					prncnt--;
					if (expcnt && explist[expcnt - 1] == OPER_LEFTPRN) {
						expcnt--;
						continue;
					}
					expval = OPER_RGTPRN;
					break;
				case '*':
					expval = MATH_MLT;
					break;
				case '+':
					expval = MATH_ADD;
					break;
				case '-':
					expval = MATH_SUB;
					break;
				case '.':
					expval = CHAR_LENGTH;
					break;
				case '/':
					if (rec[i1] == '/') {
						i1++;
						expval = MATH_MOD;
					}
					else expval = MATH_DIV;
					break;
				case ':':
					expval = CHAR_SUBSTRX;
					break;
				case '<':
					if (rec[i1] == '=') {
						i1++;
						expval = LOGIC_LTE;
					}
					else expval = LOGIC_LT;
					break;
				case '=':
					expval = LOGIC_EQ;
					break;
				case '>':
					if (rec[i1] == '=') {
						i1++;
						expval = LOGIC_GTE;
					}
					else expval = LOGIC_GT;
					break;
				case '[':
					if (rec[i1] == '[') {
						i1++;
						expval = CHAR_SCAN;
					}
					else expval = CHAR_PMATCH;
					break;
				case '\\':
					if (rec[i1] == '\\') {
						i1++;
						expval = CHAR_CONCATE;
					}
					else expval = CHAR_REPLACE;
					break;
				case '^':
					expval = CHAR_SUBSTR;
					break;
				case '|':
					expval = LOGIC_OR;
					break;
				case '~':
				case '!':
					if (rec[i1] == '=') {
						i1++;
						expval = LOGIC_NEQ;
					}
					else if (rec[i1] == '[' && rec[i1 + 1] == '[') {
						i1 += 2;
						expval = CHAR_NOTSCAN;
					}
					else expval = LOGIC_NOT;
					break;
				default:
					deatha(DEATH_INVCHAR);
			}
		}
		if (expcnt == EXPMAX) deatha(DEATH_TOOMANYEXP);
		explist[expcnt++] = expval;
	}
	if (prncnt) deatha(DEATH_NOPAREN);
}

static void expeval(INT start)
{
	INT i1, i2, expval;

	for (i1 = start; i1 < expcnt; ) {
		expval = explist[i1];
		if (expval & OPER_MASK) {
			if (expval == OPER_LEFTPRN) {
				if (i1 < --expcnt) memmove((CHAR *) &explist[i1], (CHAR *) &explist[i1 + 1], (expcnt - i1) * sizeof(*explist));
				expeval(i1);
			}
			else if (expval == OPER_RGTPRN) {
				if (i1 < --expcnt) memmove((CHAR *) &explist[i1], (CHAR *) &explist[i1 + 1], (expcnt - i1) * sizeof(*explist));
				return;
			}
			else {
				if (i1 > start) deatha(DEATH_INVEXP);
				i1++;
			}
			continue;
		}

		if (expval == MATH_SUB && i1 == start) expval = explist[i1] = MATH_NEG;
		i2 = start + 2;
		if (expval == MATH_NEG || expval == LOGIC_NOT || expval == CHAR_LENGTH || (expval & FUNC_MASK)) i2 = start + 1;
		if (expval == FUNC_CLOCK) i2 = start;
		else i1++;
		if (i1 != i2 || i1 == expcnt || explist[i1] == OPER_RGTPRN ||
             (!(explist[i1] & (OPER_MASK | FUNC_MASK)) && explist[i1] != MATH_SUB && explist[i1] != LOGIC_NOT)) deatha(DEATH_INVEXP);
		if (explist[i1] == OPER_LEFTPRN || explist[i1] == MATH_SUB || explist[i1] == LOGIC_NOT || ((explist[i1] & FUNC_MASK) && explist[i1] != FUNC_CLOCK)) {
			if (explist[i1] == OPER_LEFTPRN && i1 < --expcnt) memmove((CHAR *) &explist[i1], (CHAR *) &explist[i1 + 1], (expcnt - i1) * sizeof(*explist));
			expeval(i1);
			if (i1 == expcnt) deatha(DEATH_INVEXP);
		}
		if (expval == CHAR_SUBSTRX) {
			i1++;
			continue;
		}
		if (expval == CHAR_SUBSTR && i1 + 1 < expcnt && explist[i1 + 1] == CHAR_SUBSTRX) {
			if (expval == CHAR_SUBSTR) i1 += 2;
			else i1++;
			if (i1 == expcnt || !(explist[i1] & OPER_MASK) || explist[i1] == OPER_RGTPRN) deatha(DEATH_INVEXP);
			if (explist[i1] == OPER_LEFTPRN) {
				if (i1 < --expcnt) memmove((CHAR *) &explist[i1], (CHAR *) &explist[i1 + 1], (expcnt - i1) * sizeof(*explist));
				expeval(i1);
				if (i1 == expcnt) deatha(DEATH_INVEXP);
			}
		}
		expevalx(start);
		i1 = start;
		continue;
	}
}

static void expevalx(INT start)
{
	INT i1, i2, i3, explen, expval, bool[3], len[3], symbol[3], type[3];
	OFFSET num[3];
	CHAR c1, name[100], str[3][STRMAX + 1];

	/* 26 OCT 15
	 * This strange looking assignment is to stop the clang static analyzer
	 * from complaining. It does not understand that expval can NEVER
	 * start off as CHAR_CONCATE
	 */
	len[1] = 0xCACA;

	expval = explist[start];
	explen = 3;
	if (expval & OPER_MASK) {
		symbol[0] = expval & ~OPER_MASK;
		expval = explist[start + 1];
		symbol[1] = explist[start + 2] & ~OPER_MASK;
		if (expval == CHAR_SUBSTR && start + 3 < expcnt && explist[start + 3] == CHAR_SUBSTRX) {
			symbol[2] = explist[start + 4] & ~OPER_MASK;
			explen = 5;
		}
	}
	else {  /* must be a function or negate */
		if (expval != FUNC_CLOCK) {
			symbol[0] = explist[start + 1] & ~OPER_MASK;
			explen = 2;
		}
		else explen = 1;
	}
	/* get symbol values */
	i2 = explen - 1;
	if (i2 == 4) i2 = 3;
	for (i1 = 0; i1 < i2; i1++) {
		getsym(symbol[i1], &type[i1], &bool[i1], &num[i1], str[i1]);
		len[i1] = (INT)strlen(str[i1]);
		if (symbol[i1] >= symcnt) sym[symbol[i1]].typeflg = 0;
	}
	if (expval & LOGIC_MASK) {  /* logical operator */
		if (expval == LOGIC_OR) bool[0] |= bool[1];
		else if (expval == LOGIC_AND) bool[0] &= bool[1];
		else if (expval == LOGIC_NOT) bool[0] = !bool[0];
		else {
			if (type[0] == TYPE_CHAR || type[1] == TYPE_CHAR) {
				if (len[0] && len[1]) {
					if (len[0] < len[1]) {
						memset(&str[0][len[0]], ' ', len[1] - len[0]);
						str[0][len[1]] = 0;
					}
					else if (len[0] > len[1]) {
						memset(&str[1][len[1]], ' ', len[0] - len[1]);
						str[1][len[0]] = 0;
					}
				}
				num[0] = strcmp(str[0], str[1]);
			}
			else num[0] -= num[1];
			if (expval == LOGIC_EQ) bool[0] = (num[0] == 0);
			else if (expval == LOGIC_LT) bool[0] = (num[0] < 0);
			else if (expval == LOGIC_GT) bool[0] = (num[0] > 0);
			else if (expval == LOGIC_LTE) bool[0] = (num[0] <= 0);
			else if (expval == LOGIC_GTE) bool[0] = (num[0] >= 0);
			else if (expval == LOGIC_NEQ) bool[0] = (num[0] != 0);
		}
		type[0] = TYPE_CHAR;
		len[0] = 0;
	}
	else if (expval & CHAR_MASK) {  /* character operator */
		if (expval == CHAR_CONCATE) {
			type[0] = TYPE_CHAR;
			bool[0] = TRUE;
			i1 = len[0];
			len[0] += len[1];
			if (len[0] > STRMAX) len[0] = STRMAX;
			memcpy(&str[0][i1], str[1], len[0] - i1);
		}
		else if (expval == CHAR_SUBSTR) {
			type[0] = TYPE_CHAR;
			bool[0] = TRUE;
			i2 = 1;
			i1 = (INT) num[1];
			if (i1 > 0 && i1 <= len[0]) i1--;
			else if (i1 < 0 && -i1 <= len[0]) i1 = len[0] + i1;
			else i2 = 0;
			if (i2) {
				i3 = 1;
				if (explen == 5) {
					i2 = (INT) num[2];
					if (i2 < 0) {
						i2 = -i2;
						i3 = -1;
					}
				}
			}
			len[1] = 0;
			while (i2-- && len[1] < STRMAX) {
				str[1][len[1]++] = str[0][i1];
				i1 += i3;
				if (i1 == -1) i1 = len[0] - 1;
				else if (i1 == len[0]) i1 = 0;
			}
			len[0] = len[1];
			memcpy(str[0], str[1], len[0]);
		}
		else if (expval == CHAR_LENGTH) {
			type[0] = TYPE_DEC;
			bool[0] = TRUE;
			num[0] = len[0];
		}
		else if (expval == CHAR_PMATCH) {
			type[0] = TYPE_DEC;
			bool[0] = FALSE;
			len[0] -= len[1];
			for (i1 = 0; i1 <= len[0]; i1++) {
				for (i2 = 0; i2 < len[1] && (str[1][i2] == '?' || str[1][i2] == str[0][i1 + i2]); i2++);
				if (i2 == len[1]) {
					bool[0] = TRUE;
					num[0] = i1 + 1;
					break;
				}
			}
		}
		else if (expval == CHAR_SCAN) {
			type[0] = TYPE_DEC;
			bool[0] = FALSE;
			for (i1 = 0; i1 < len[0]; i1++) {
				for (i2 = 0; i2 < len[1] && str[1][i2] != str[0][i1]; i2++);
				if (i2 < len[1]) {
					bool[0] = TRUE;
					num[0] = i1 + 1;
					break;
				}
			}
		}
		else if (expval == CHAR_NOTSCAN) {
			type[0] = TYPE_DEC;
			bool[0] = FALSE;
			for (i1 = 0; i1 < len[0]; i1++) {
				for (i2 = 0; i2 < len[1] && str[1][i2] != str[0][i1]; i2++);
				if (i2 == len[1]) {
					bool[0] = TRUE;
					num[0] = i1 + 1;
					break;
				}
			}
		}
		else if (expval == CHAR_REPLACE) {
			type[0] = TYPE_CHAR;
			if (!(len[1] & 0x01)) {
				for (i1 = 0; i1 < len[0]; i1++) {
					for (i2 = 0; i2 < len[1]; i2 += 2) {
						if (str[1][i2] == str[0][i1]) {
							str[0][i1] = str[1][i2 + 1];
							break;
						}
					}
				}
			}
		}
	}
	else if (expval & MATH_MASK) {  /* math operator */
		if (expval == MATH_ADD) num[0] += num[1];
		else if (expval == MATH_SUB) num[0] -= num[1];
		else if (expval == MATH_MLT) num[0] *= num[1];
		else if (expval == MATH_DIV || expval == MATH_MOD) {
			if (!num[1]) deatha(DEATH_ZERODIV);
			if (expval == MATH_DIV) num[0] /= num[1];
			else num[0] %= num[1];
		}
		else if (expval == MATH_NEG) num[0] = -num[0];
		type[0] = TYPE_DEC;
		bool[0] = TRUE;
	}
	else {  /* function */
		if (expval == FUNC_FFILE) {
			strcpy(name, str[0]);
			if (!name[0]) deatha(DEATH_INVOPER);
			miofixname(name, ".txt", FIXNAME_EXT_ADD);
			i1 = fioopen(name, FIO_M_EXC | FIO_P_TXT);
			type[0] = TYPE_CHAR;
			bool[0] = TRUE;
			if (i1 == ERR_FNOTF) bool[0] = FALSE;
			else if (i1 < 0) {
				strcpy(str[0], fioerrstr(i1));
				len[0] = (INT)strlen(str[0]);
			}
			else {
				fioclose(i1);
				len[0] = 0;
			}
		}
		else if (expval == FUNC_GETPOS) {
			if (type[0] == TYPE_FILE) {
				bool[0] = TRUE;
				rionextpos(sym[symbol[0]].boolflg, &num[0]);
			}
			else bool[0] = FALSE;
			type[0] = TYPE_DEC;
		}
		else if (expval == FUNC_GETLASTPOS) {
			if (type[0] == TYPE_FILE) {
				bool[0] = TRUE;
				riolastpos(sym[symbol[0]].boolflg, &num[0]);
			}
			else bool[0] = FALSE;
			type[0] = TYPE_DEC;
		}
		else if (expval == FUNC_CLOCK) {
			type[0] = TYPE_CHAR;
			bool[0] = TRUE;
			len[0] = 19;
			msctimestamp((UCHAR *) str[0]);
			for (i1 = 13, i2 = 18; i2 > 3; ) {
				if (i2 == 4 || i2 == 7) c1 = '/';
				else if (i2 == 10) c1 = ' ';
				else if (i2 == 13 || i2 == 16) c1 = ':';
				else c1 = str[0][i1--];
				str[0][i2--] = c1;
			}
			str[0][19] = 0;
		}
		else deatha(DEATH_PROG1);
	}

	for (symbol[0] = symcnt; symbol[0] < symcnt2 && sym[symbol[0]].typeflg; symbol[0]++);
	if (symbol[0] == symcnt2) {
		if (symbol[0] == symmax) moresyms();
		symcnt2++;
	}
	sym[symbol[0]].typeflg = TYPE_CHAR;
	sym[symbol[0]].name[0] = 0;
	sym[symbol[0]].boolflg = bool[0];
	if (bool[0]) {
		if (type[0] == TYPE_DEC) {
			mscofftoa(num[0], str[0]);
			len[0] = (INT)strlen(str[0]);
		}
		memcpy(sym[symbol[0]].string, str[0], len[0]);
	}
	else len[0] = 0;
	sym[symbol[0]].string[len[0]] = 0;
	if (start + explen < expcnt) memmove((CHAR *) &explist[start + 1], (CHAR *) &explist[start + explen], (expcnt - (start + explen)) * sizeof(*explist));
	expcnt -= explen - 1;
	explist[start] = OPER_SYMBOL | symbol[0];
}

static INT expvalue(CHAR *rec)
{
	INT symbol;

	expinit(rec);
	expeval(0);
	if (!expcnt) deatha(DEATH_NOOP);
	symbol = explist[0] & ~OPER_MASK;
	return(sym[symbol].boolflg);
}

static void getsym(INT symbol, INT *type, INT *bool, OFFSET *num, CHAR *str)
{
	INT i1, negflg;
	OFFSET l1, l2;

	*type = TYPE_CHAR;
	*num = 0;
	*bool = sym[symbol].boolflg;
	if (!*bool) str[0] = 0;
	else {
		strcpy(str, sym[symbol].string);
		if (sym[symbol].typeflg == TYPE_CHAR) {  /* test for valid numeric field */
			for (i1 = 0; str[i1] == ' '; i1++);
			if (str[i1] == '-') {
				i1++;
				negflg = TRUE;
			}
			else negflg = FALSE;
			if (str[i1]) {
				for (l1 = 0; isdigit(str[i1]); i1++) {
					l2 = l1;
					l1 = l1 * 10 + str[i1] - '0';
					if (l1 < l2) break;
				}
				if (!str[i1]) {
					*type = TYPE_DEC;
					if (negflg) l1 = -l1;
					*num = l1;
				}
			}
		}
		else *type = TYPE_FILE;
	}
}

static INT getstr(CHAR *rec, CHAR **str)
{
	INT i1, i2;
	CHAR c1, name[SYMNAMSIZ + 1];

	if (rec[0] == '"') {  /* literal */
		i2 = (INT)strlen(rec);
		for (i1 = i2 - 1; i1 && rec[i1] != '"'; i1--);
		if (!i1) deatha(DEATH_NOQUOTE);
		*str = &rec[1];
		return(i1 - 1);
	}
	if (isalpha(rec[0])) {  /* check if symbol */
		for (i1 = 1; isalnum(rec[i1]); i1++);
		if (!rec[i1] || rec[i1] == ' ' || rec[i1] == ',') {
			if (i1 > SYMNAMSIZ) {
				memcpy(name, rec, SYMNAMSIZ - 1);
				name[SYMNAMSIZ - 1] = rec[i1 - 1];
				name[SYMNAMSIZ] = '\0';
			}
			else {
				memcpy(name, rec, i1);
				name[i1] = '\0';
			}
			if (caseflg) for (i1 = 0; name[i1]; i1++) name[i1] = (CHAR) toupper(name[i1]);
			c1 = name[0];
			for (i1 = 0; i1 < symcnt && (!sym[i1].typeflg || c1 != sym[i1].name[0] || strcmp(name, sym[i1].name)); i1++);
			if (i1 < symcnt) {  /* found symbol */
				if (sym[i1].typeflg == TYPE_CHAR && sym[i1].boolflg) {
					*str = sym[i1].string;
					return (INT)(strlen(*str));
				}
				*str = NULL;
				return(0);
			}
		}
	}
	/* unquoted literal */
	for (i1 = 0; rec[i1] && rec[i1] != ' ' && rec[i1] != ','; i1++);
	*str = rec;
	return(i1);
}

static INT getnum(CHAR *rec)
{
	INT i1;
	CHAR *ptr;

	i1 = getstr(rec, &ptr);
	if (i1) mscntoi((UCHAR *) ptr, &i1, i1);
	return(i1);
}

static INT getfile(CHAR *rec)
{
	INT handle, symbol;

	expinit(rec);
	symbol = explist[0];
	handle = -1;
 	if (expcnt && (symbol & OPER_MASK) == OPER_SYMBOL) {
		symbol &= ~OPER_MASK;
		if (sym[symbol].typeflg == TYPE_FILE) handle = sym[symbol].boolflg;
	}
	if (handle < 0) deatha(DEATH_NOFILE);
	return(handle);
}

static void chnofftoo(OFFSET src, CHAR *dest)
{
	INT i1, negflg;
	CHAR work[32];

	if (src < 0) {
		src = -src;
		negflg = TRUE;
	}
	else negflg = FALSE;

	i1 = sizeof(work);
	work[--i1] = 0;
	do work[--i1] = (CHAR)((src & 0x07) + '0');
	while (src >>= 3);
	work[--i1] = '0';
	if (negflg) work[--i1] = '-';
	strcpy(dest, &work[i1]);
}

static void chn10tou(CHAR *src, UINT *dest)
{
	INT i, j;
	UINT lwork;

	for (i = 0, lwork = 0L; i < 10; ) {
		j = src[i++];
		if (isdigit(j)) lwork = lwork * 10 + j - '0';
	}
	*dest = lwork;
}

static void xstatement(CHAR *rec, INT reclen)
{
	INT i1;

	i1 = rioput(wrkhndl, (UCHAR *) rec, reclen);
	if (i1) death(DEATH_WRITE, i1, fioname(wrkhndl), 1);
}

static CHAR *skiplines(INT typeflg) // @suppress("No return")
{
	INT i1, i2, deathtyp, nestcnt;
	CHAR *ptr1, *ptr2;

	if (typeflg == 3) {
		ptr1 = "WHILE";
		ptr2 = "END";
		deathtyp = DEATH_WHILEEND;
	}
	else {
		ptr1 = "IF";
		ptr2 = "XIF";
		if (typeflg == 1) deathtyp = DEATH_IFXIF;
		else deathtyp = DEATH_IFELSEXIF;
	}
	nestcnt = 0;
	while (TRUE) {
		i1 = rioget(chnhndl, (UCHAR *) record, RECMAX);
		if (i1 < 0) {
			if (i1 == -2) continue;
			ptr1 = fioname(chnhndl);
			if (i1 == -1) death(deathtyp, 0, ptr1, 1);
			death(DEATH_READ, i1, ptr1, 1);
		}
		linecnt++;
		record[i1] = 0;
		if (record[0] != '/' || (record[1] != '/' && record[1] != '@')) continue;
		for (i1 = 2; isspace(record[i1]); i1++);
		for (i2 = i1; record[i2] && !isspace(record[i2]); i2++) record[i2] = (CHAR) toupper(record[i2]);
		if (record[i2]) record[i2++] = 0;
		if (!strcmp(&record[i1], ptr2)) {
			if (!nestcnt--) return(NULL);
			continue;
		}
		if (!strcmp(&record[i1], ptr1)) {
			nestcnt++;
			continue;
		}
		if (typeflg == 1) {
			if (nestcnt) continue;
			if (!strcmp(&record[i1], "ELSE")) return(NULL);
			if (!strcmp(&record[i1], "ELSEIF")) {
				while (isspace(record[i2])) i2++;
				return(&record[i2]);
			}
		}
	}
}

static void vexec(INT exectype)
{
	record[0] = (CHAR) exectype;
	xstatement(record, 1);
}

static void vabort()
{
	dspstring("CHAIN compilation aborted\n");
	exit(1);
}

static void vassign(CHAR *rec)
{
	INT i1, symbol;
	CHAR name[SYMNAMSIZ + 1];

	expinit(rec);
	symbol = explist[0];
	if (expcnt < 3 || (symbol & OPER_MASK) != OPER_SYMBOL || explist[1] != LOGIC_EQ) deatha(DEATH_INVOPER);
	symbol &= ~OPER_MASK;
	if (symbol < symconst || sym[symbol].typeflg == TYPE_FILE || !sym[symbol].name[0]) deatha(DEATH_INVOPER);
	strcpy(name, sym[symbol].name);
	if (symbol == symcnt) {
		if (discardflg) for (symbol = symconst; symbol < symcnt && sym[symbol].typeflg; symbol++);
		if (symbol == symcnt) {
			symcnt++;
			discardflg = FALSE;
		}
	}
	expeval(2);
	i1 = explist[2] & ~OPER_MASK;
	memcpy((CHAR *) &sym[symbol], (CHAR *) &sym[i1], sizeof(struct symdef));
	sym[symbol].boolflg = TRUE;
	strcpy(sym[symbol].name, name);
}

static void vbeep()
{
	dspchar(0x07);
	dspflush();
}

static void vclose(CHAR *rec)
{
	INT i1, handle, symbol;

	handle = getfile(rec);
	i1 = rioclose(handle);
	if (i1) death(DEATH_CLOSE, i1, NULL, 1);
	symbol = explist[0] & ~OPER_MASK;
	sym[symbol].typeflg = TYPE_CHAR;
	sym[symbol].boolflg = FALSE;
}

static void vdebug(CHAR *rec)
{
	INT i1, symbol;

	if (!debugflg) return;
	expinit(rec);
	for (i1 = 0; i1 < expcnt; i1++) {
		symbol = explist[i1];
 		if ((symbol & OPER_MASK) != OPER_SYMBOL) continue;
		symbol &= ~OPER_MASK;
		if (symbol >= symcnt) continue;
		if (sym[symbol].boolflg) {
			dspstring(sym[symbol].string);
			dspchar(' ');
		}
	}
	dspchar('\n');
}

static void vdiscard(CHAR *rec)
{
	INT i1, symbol;

	expinit(rec);
	symbol = explist[0];
 	if (!expcnt || (symbol & OPER_MASK) != OPER_SYMBOL) deatha(DEATH_INVOPER);
	symbol &= ~OPER_MASK;
	if (symbol < symconst) return;
	if (sym[symbol].typeflg == TYPE_FILE) {
		i1 = rioclose(sym[symbol].boolflg);
		if (i1) death(DEATH_CLOSE, i1, NULL, 1);
	}
	sym[symbol].typeflg = 0;
	if (symbol + 1 == symcnt) while (symcnt && !sym[symcnt - 1].typeflg) symcnt--;
	else discardflg = TRUE;
}

static void vdisplay(CHAR *rec)
{
	INT i1, symbol;

	expinit(rec);
	for (i1 = 0; i1 < expcnt; i1++) {
		symbol = explist[i1];
 		if ((symbol & OPER_MASK) != OPER_SYMBOL) continue;
		symbol &= ~OPER_MASK;
		if (sym[symbol].boolflg) dspstring(sym[symbol].string);
	}
	dspchar('\n');
}

static void vdo()
{
	if (loopcnt == LOOPMAX) deatha(DEATH_TOOMANYLOOP);
	riolastpos(chnhndl, &looppos[loopcnt++]);
}

static void velse()
{
	skiplines(2);
}

static void vend()
{
	if (!loopcnt--) deatha(DEATH_WHILEEND);
	riosetpos(chnhndl, looppos[loopcnt]);
}

static void vif(CHAR *rec)
{
	while (rec != NULL) {
		if (expvalue(rec)) return;
		rec = skiplines(1);
	}
}

static void vinclude(CHAR *rec)
{
	INT i1;
	CHAR *ptr, work[100];

	i1 = getstr(rec, &ptr);
	if (!i1) deatha(DEATH_INVOPER);
	memcpy(work, ptr, i1);
	work[i1] = 0;

	if (inccnt == INCMAX) deatha(DEATH_TOOMANYINC);
	inclist[inccnt] = chnhndl;
	looplist[inccnt] = loopcnt;
	linelist[inccnt++] = linecnt;

	i1 = 1;
	if (inccnt > 1) i1 = 0;
	miofixname(work, ".chn", FIXNAME_EXT_ADD);
	chnhndl = rioopen(work, openflg, RIOBUFS + i1, RECMAX);
	if (chnhndl < 0) death(DEATH_OPEN, chnhndl, work, 1);
	sym = (struct symdef *) *symptr;
	lab = (struct labdef *) *labptr;
}

static void vkeyboard(CHAR *rec)
{
	INT i1;
	CHAR *ptr;

	i1 = getstr(rec, &ptr);
	memmove(&record[1], ptr, i1);
	record[0] = KEYBOARD;
	xstatement(record, i1 + 1);
}

static void vkeyin(CHAR *rec)
{
	INT i1, i2, nlflg, symbol;
	CHAR str[256];

	nlflg = TRUE;
	expinit(rec);
	for (i1 = 0; i1 < expcnt; i1++) {
		symbol = explist[i1];
 		if ((symbol & OPER_MASK) != OPER_SYMBOL) deatha(DEATH_INVOPER);
		symbol &= ~OPER_MASK;
		if (sym[symbol].typeflg != TYPE_FILE && sym[symbol].name[0]) {  /* keyin */
			if (fgets(str, sizeof(str) - 1, stdin) != NULL) {
				i2 = (INT)strlen(str);
				if (i2 && str[i2 - 1] == '\n') str[i2 - 1] = 0;
			}
			else str[0] = 0;
			sym[symbol].boolflg = TRUE;
			strcpy(sym[symbol].string, str);
			if (symbol >= symcnt) {
				if (symbol > symcnt) memcpy((CHAR *) &sym[symcnt], (CHAR *) &sym[symbol], sizeof(struct symdef));
				symcnt++;
			}
			nlflg = FALSE;
		}
		else {  /* display */
			if (sym[symbol].boolflg) {
				dspstring(sym[symbol].string);
				dspflush();
				nlflg = TRUE;
			}
		}
	}
	if (nlflg) dspchar('\n');
}

static void vlabel(CHAR *rec, INT labtype)
{
	INT i1, label;
	CHAR name[SYMNAMSIZ + 5];

	for (i1 = 0; isalnum(rec[i1]); i1++);
	if (!i1) deatha(DEATH_INVOPER);
	memcpy(&name[1], rec, SYMNAMSIZ);
	if (i1 > SYMNAMSIZ) {
		name[SYMNAMSIZ] = rec[i1 - 1];
		i1 = SYMNAMSIZ;
	}
	name[i1 + 1] = 0;

	for (label = 0; label < labcnt && strcmp(&name[1], lab[label].name); label++);
	if (label < labcnt) {
		if (lab[label].foundflg) {
			if (labtype == LABEL) deatha(DEATH_DUPLAB);
			name[0] = (CHAR)(labtype + 1);
			mscoffto9(lab[label].pos, (UCHAR *) &name[1]);
			xstatement(name, 10);
			return;
		}
	}
	else {  /* label == labcnt */
		if (label == labmax) morelabs();
		strcpy(lab[label].name, &name[1]);
		lab[label].foundflg = FALSE;
		labcnt++;
	}

	name[0] = (CHAR) labtype;
	xstatement(name, i1 + 1);
	if (labtype == LABEL) {
		lab[label].foundflg = TRUE;
		rionextpos(wrkhndl, &lab[label].pos);
	}
}

static void vlog(CHAR *rec)
{
	INT i1;
	CHAR work1[100], work2[100], *ptr1, *ptr2, **pptr;

	i1 = getstr(rec, &ptr1);
	if (!i1) deatha(DEATH_INVOPER);
	memcpy(work1, ptr1, i1);
	work1[i1] = 0;
	miofixname(work1, ".log", FIXNAME_EXT_ADD | FIXNAME_PAR_DBCVOL);
	miogetname(&ptr1, &ptr2);
	if (*ptr1) {
		if (!fiocvtvol(ptr1, &pptr)) {  /* prefix volume */
			miostrscan(*pptr, work2);
			fioaslashx(work2);
			strcat(work2, work1);
			strcpy(work1, work2);
		}
	}

	record[0] = LOG;
	strcpy(&record[1], work1);
	xstatement(record, (INT)strlen(record));
}

static void vopen(CHAR *rec, INT openflg)
{
	INT i1, handle, symbol;
	CHAR *ptr;

	if (bufptr == NULL) {
		bufptr = memalloc(RIO_MAX_RECSIZE, 0);
		if (bufptr == NULL) death(DEATH_OPEN, ERR_NOMEM, NULL, 1);
		sym = (struct symdef *) *symptr;
		lab = (struct labdef *) *labptr;
	}
	expinit(rec);
	symbol = explist[0];
 	if (!expcnt || (symbol & OPER_MASK) != OPER_SYMBOL) deatha(DEATH_INVOPER);
	symbol &= ~OPER_MASK;
	if (symbol == symcnt) {  /* new file symbol */
		if (!sym[symbol].name[0]) deatha(DEATH_INVOPER);
		symcnt++;
	}
	if (sym[symbol].typeflg == TYPE_FILE) {  /* close file */
		i1 = rioclose(sym[symbol].boolflg);
		if (i1) death(DEATH_CLOSE, i1, NULL, 1);
	}

	for (i1 = 1; i1 < expcnt && (explist[i1] & OPER_MASK) != OPER_SYMBOL; i1++);
	if (i1 == expcnt) deatha(DEATH_INVOPER);
	i1 = explist[i1] & ~OPER_MASK;
	ptr = sym[i1].string;
	if (!ptr[0]) deatha(DEATH_INVOPER);
	handle = rioopen(ptr, openflg, RIOBUFS, RIO_MAX_RECSIZE);
	if (handle == ERR_FNOTF && (openflg & RIO_M_MASK) == RIO_M_EXC) {
		handle = rioopen(ptr, (openflg & ~RIO_M_MASK) | RIO_M_PRP, RIOBUFS, RIO_MAX_RECSIZE);
	}
	if (handle < 0) death(DEATH_OPEN, handle, ptr, 1);
	sym = (struct symdef *) *symptr;
	lab = (struct labdef *) *labptr;
	sym[symbol].typeflg = TYPE_FILE;
	sym[symbol].boolflg = handle;
	strcpy(sym[symbol].string, ptr);
}

static void vposition(CHAR *rec)
{
	INT handle, bool, symbol, type;
	OFFSET num, pos;
	CHAR str[STRMAX + 1];

	handle = getfile(rec);
	for (symbol = 1; symbol < expcnt && (explist[symbol] & OPER_MASK) != OPER_SYMBOL; symbol++);
	if (symbol == expcnt) deatha(DEATH_INVOPER);
	symbol = explist[symbol] & ~OPER_MASK;
	getsym(symbol, &type, &bool, &num, str);
	if (type != TYPE_DEC) deatha(DEATH_INVOPER);

	rioeofpos(handle, &pos);
	if (num < 0) {
		dspstring("Warning:  attempted to position before beginning of file\n");
		num = 0;
	}
	if (num > pos) {
		dspstring("Warning:  attempted to position beyond EOF\n");
		num = pos;
	}
	riosetpos(handle, num);
}

static void vread(CHAR *rec)
{
	INT i1, i2, i3, i4, handle, symbol;

	handle = getfile(rec);
	while ((i3 = rioget(handle, *bufptr, RIO_MAX_RECSIZE)) == -2);
	if (i3 < -1) death(DEATH_READ, i3, fioname(handle), 1);
	for (i1 = 1, i2 = 0; i1 < expcnt; i1++) {
		symbol = explist[i1];
 		if ((symbol & OPER_MASK) != OPER_SYMBOL) deatha(DEATH_INVOPER);
		symbol &= ~OPER_MASK;
		if (sym[symbol].typeflg != TYPE_CHAR || !sym[symbol].name[0]) deatha(DEATH_INVOPER);
		if (i2 < i3) {
			sym[symbol].boolflg = TRUE;
			i4 = i3 - i2;
			if (i4 > STRMAX) i4 = STRMAX;
			memcpy(sym[symbol].string, &(*bufptr)[i2], i4);
			i2 += i4;
		}
		else {
			if (i1 == 1 && !i3) sym[symbol].boolflg = TRUE;
			else sym[symbol].boolflg = FALSE;
			i4 = 0;
		}
		sym[symbol].string[i4] = 0;
		if (symbol >= symcnt) {
			if (symbol > symcnt) memcpy((CHAR *) &sym[symcnt], (CHAR *) &sym[symbol], sizeof(struct symdef));
			symcnt++;
		}
	}
}

static void vrewrite(CHAR *rec)
{
	INT i1, i3, handle, symbol;
	OFFSET pos;
	size_t i2, len;

	handle = getfile(rec);
	riolastpos(handle, &pos);
	riosetpos(handle, pos);
	i3 = rioget(handle, *bufptr, RIO_MAX_RECSIZE);
	if (i3 < 0) {
		if (i3 == -1) deatha(DEATH_WRITEEOF);
		if (i3 == -2) deatha(DEATH_WRITEDEL);
		death(DEATH_READ, i3, fioname(handle), 1);
	}

	for (i1 = 1, i2 = 0; i1 < expcnt; i1++) {
		symbol = explist[i1];
 		if ((symbol & OPER_MASK) != OPER_SYMBOL) continue;
		symbol &= ~OPER_MASK;
		if (sym[symbol].boolflg) {
			len = strlen(sym[symbol].string);
			memcpy(&(*bufptr)[i2], sym[symbol].string, len);
			i2 += len;
		}
	}

	riosetpos(handle, pos);
	i1 = rioput(handle, *bufptr, i3);
	if (i1) death(DEATH_WRITE, i1, fioname(handle), 1);
}

static void vset(CHAR *rec)
{
	INT i1, symbol;

	expinit(rec);
	symbol = explist[0];
	if (expcnt < 3 || (symbol & OPER_MASK) != OPER_SYMBOL || explist[1] != LOGIC_EQ) deatha(DEATH_INVOPER);
	symbol &= ~OPER_MASK;
	if (symbol < symconst || sym[symbol].typeflg == TYPE_FILE || !sym[symbol].name[0]) deatha(DEATH_INVOPER);
	if (symbol == symcnt) {
		if (discardflg) for (symbol = symconst; symbol < symcnt && sym[symbol].typeflg; symbol++);
		if (symbol == symcnt) {
			symcnt++;
			discardflg = FALSE;
		}
	}
	expeval(2);
	i1 = explist[2] & ~OPER_MASK;
	sym[symbol].boolflg = sym[i1].boolflg;
	sym[symbol].string[0] = 0;
}

static void vsound(CHAR *rec)
{
	INT i1;
	time_t twork1, twork2;

	i1 = getnum(rec);
	if (i1 < 1) i1 = 1;
	time(&twork1);
	twork2 = twork1 + i1;
	while (twork1 < twork2) {
		dspchar(0x07);
		dspflush();
		time(&twork1);
	}
}

static void vsoundr(CHAR *rec)
{
	INT i1;

	i1 = getnum(rec);
	if (i1 < 1) i1 = 1;
	record[0] = SOUNDR;
	mscitoa(i1, &record[1]);
	xstatement(record, (INT)strlen(record));
}

static void vstop(CHAR *rec)
{
	INT i1;
	CHAR *ptr, work[EOFSIZ];

	i1 = getstr(rec, &ptr);
	if (i1) {
		work[0] = END_OF_EXTENT;
		mscoffto9(nextpos, (UCHAR *) &work[1]);
		mscoffto9(eofpos, (UCHAR *) &work[10]);
		xstatement(work, EOFSIZ);
		nextpos = eofpos;
		rionextpos(wrkhndl, &eofpos);
		memmove(&record[1], ptr, i1);
		record[0] = COMMAND;
		xstatement(record, i1 + 1);
	}
}

static void vuntil(CHAR *rec)
{
	if (!loopcnt--) deatha(DEATH_DOUNTIL);
	if (expvalue(rec)) return;
	riosetpos(chnhndl, looppos[loopcnt]);
}

static void vwait(CHAR *rec)
{
	INT i1, eventid;
	UCHAR work[17];

	i1 = getnum(rec);
	if (i1 < 1) i1 = 1;
	msctimestamp(work);
	timadd(work, i1 * 100);
	if ((eventid = evtcreate()) == -1) return;
	timset(work, eventid);
	evtwait(&eventid, 1);
	evtdestroy(eventid);
}

static void vwhile(CHAR *rec)
{
	if (expvalue(rec)) {
		if (loopcnt == LOOPMAX) deatha(DEATH_TOOMANYLOOP);
		riolastpos(chnhndl, &looppos[loopcnt++]);
		return;
	}
	skiplines(3);
}

static void vwrite(CHAR *rec)
{
	INT i1, i2, handle, len, symbol;
	OFFSET pos;

	handle = getfile(rec);
	for (i1 = 1, i2 = 0; i1 < expcnt; i1++) {
		symbol = explist[i1];
 		if ((symbol & OPER_MASK) != OPER_SYMBOL) continue;
		symbol &= ~OPER_MASK;
		if (sym[symbol].boolflg) {
			len = (INT)strlen(sym[symbol].string);
			memcpy(&(*bufptr)[i2], sym[symbol].string, len);
			i2 += len;
		}
	}

	rioeofpos(handle, &pos);
	riosetpos(handle, pos);
	i1 = rioput(handle, *bufptr, i2);
	if (i1) death(DEATH_WRITE, i1, fioname(handle), 1);
}

static void moresyms()
{
	if (memchange(symptr, (symmax + SYMINC) * sizeof(struct symdef), 0)) deatha(DEATH_TOOMANYSYM);
	symmax += SYMINC;
	sym = (struct symdef *) *symptr;
	lab = (struct labdef *) *labptr;
}

static void morelabs()
{
	if (memchange(labptr, (labmax + LABINC) * sizeof(struct labdef), 0)) deatha(DEATH_TOOMANYLAB);
	labmax += LABINC;
	sym = (struct symdef *) *symptr;
	lab = (struct labdef *) *labptr;
}

static void deatha(INT deathtyp)
{
	CHAR work[17];

	mscitoa(linecnt, work);
	if (inccnt) {
		dspstring(fioname(chnhndl));
		dspstring(": ");
	}
	dspstring("LINE ");
	dspstring(work);
	dspstring(": ");
	dspstring(record);
	dspstring(" : ");
	death(deathtyp, 0, NULL, 1);
}
