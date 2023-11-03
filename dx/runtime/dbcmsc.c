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
#define INC_MATH
#define INC_SETJMP
#include "includes.h"
#include "base.h"
#include "dbc.h"
#include "fio.h"

#if OS_UNIX && DBC_SQL
#include <dlfcn.h>
#endif

/* miscellaneous verbs and routines */

/* local variables */
#if OS_UNIX
#if DBC_SQL
static void loadsql(void);
static void freesql(void);
#endif
typedef INT (_DBCSQLX)(UCHAR *, UCHAR **, UCHAR **);
typedef CHAR *(_DBCSQLM)(void);
static _DBCSQLX *dbcsqlx;
static _DBCSQLM *dbcsqlm;

#if DBC_SQL
typedef void (_DBCSQLQ) (void);
static _DBCSQLQ *dbcsqlq;
static UCHAR sqlinitflag = 0;
static void *sqlhandle;
#endif

#elif OS_WIN32
extern INT dbcsqlx(UCHAR *, UCHAR **, UCHAR **);
extern CHAR *dbcsqlm(void);
extern void dbcsqlq(void);
#endif

/*
 * The Mac has fcvt but does not supply a prototype for it!
 */
#ifdef __MACOSX
CHAR *fcvt(DOUBLE, INT, INT *, INT *);
#endif

void vnformat()
{
	INT i1, i2, i3, lft, rgt, listflg;
	UCHAR c1, *adr;

	adr = getvar(VAR_WRITE);
	if (vartype & TYPE_ARRAY) {
		adr = getlist(LIST_WRITE | LIST_ARRAY | LIST_NUM1);
		listflg = TRUE;
	}
	else listflg = FALSE;
	if (pgm[pcount] >= 0xFE) {
		if (pgm[pcount++] == 0xFE) lft = 0 - (INT) pgm[pcount];
		else lft = pgm[pcount];
		pcount++;
	}
	else lft = nvtoi(getvar(VAR_READ));
	if (pgm[pcount] >= 0xFE) {
		if (pgm[pcount++] == 0xFE) rgt = 0 - (INT) pgm[pcount];
		else rgt = pgm[pcount];
		pcount++;
	}
	else rgt = nvtoi(getvar(VAR_READ));
	i2 = lft + rgt;
	if (rgt) i2++;
	if (lft < 0 || rgt < 0 || i2 < 1 || i2 > 31) {
		/**
		 * The next line was added on 18 Jan 2012 to fix an extremely
		 * subtle bug found by the boys at PPro.
		 * If getlist is called, and never called with LIST_END, it is left
		 * in a bad state that screws things up later.
		 */
		if (listflg) getlist(LIST_END | LIST_NUM1);
		dbcflags |= DBCFLAG_OVER;
		return;
	}

	dbcflags &= ~DBCFLAG_OVER;
	do {
		c1 = adr[0];
		if (c1 == 0xFC) adr[1] = (UCHAR) lft;  /* integer */
		else if ((c1 & 0xFC) == 0xF8) {  /* float */
			adr[0] = (UCHAR)(0xF8 | (lft >> 3));
			adr[1] = (UCHAR)((lft << 5) | rgt);
		}
		else {
			i1 = c1 & 0x1F;
			if (i1 < i2) {  /* check for nformat extender characters (0xF3) */
				i3 = i2 - i1;
				while (adr[++i1] == 0xF3 && --i3);
				if (i3) {
					dbcflags |= DBCFLAG_OVER;
					continue;
				}
			}
			else i3 = i1 - i2;
			adr[0] = (UCHAR)(0x80 | i2);
			memset(&adr[1], ' ', lft);
			if (rgt) {
				adr[lft + 1] = '.';
				memset(&adr[lft + 2], '0', rgt);
			}
			else adr[lft] = '0';
			memset(&adr[i2 + 1], 0xF3, i3);
		}
	} while (listflg && ((adr = getlist(LIST_WRITE | LIST_NUM1)) != NULL));
}

void vsformat()
{
	INT i1, i2, hl1, pl1, pl2, listflg;
	UCHAR *adr;

	adr = getvar(VAR_WRITE);
	if (vartype & TYPE_ARRAY) {
		adr = getlist(LIST_WRITE | LIST_ARRAY | LIST_NUM1);
		listflg = TRUE;
	}
	else listflg = FALSE;
	pl1 = pl;
	hl1 = hl;
	if (pgm[pcount] >= 0xFE) {
		if (pgm[pcount++] == 0xFE) pl2 = 0 - (INT) pgm[pcount];
		else pl2 = pgm[pcount];
		pcount++;
	}
	else pl2 = nvtoi(getvar(VAR_READ));
	if (pl2 < 1) {
		/**
		 * The next line was added on 17 Jan 2012 to fix an extremely
		 * subtle bug found by the boys at PPro.
		 * If getlist is called, and never called with LIST_END, it is left
		 * in a bad state that screws things up later.
		 */
		if (listflg) getlist(LIST_END | LIST_NUM1);
		dbcflags |= DBCFLAG_OVER;
		return;
	}

	pl = pl1;
	hl = hl1;
	dbcflags &= ~DBCFLAG_OVER;
	do {
		if (pl < pl2) {  /* check for sformat extender characters (0xF3) */
			i1 = hl + pl;
			i2 = pl2 - pl;
			while (adr[i1++] == 0xF3 && --i2);
			if (i2) {
				dbcflags |= DBCFLAG_OVER;
				continue;
			}
		}
		else i2 = pl - pl2;
		fp = lp = 0;
		setfplp(adr);
		if (adr[0] < 128) adr[2] = (UCHAR) pl2;  /* small dim */
		else {  /* big dim */
			adr[5] = (UCHAR) pl2;
			adr[6] = (UCHAR) (pl2 >> 8);
		}
		memset(&adr[hl], ' ', pl2);
		memset(&adr[hl + pl2], 0xF3, i2);
	} while (listflg && ((adr = getlist(LIST_WRITE | LIST_NUM1)) != NULL));
}

/* VSEARCH */
void vsearch()
{
	static UCHAR searchflg = 0;
	INT i1, i2, i3, i4, flags, fp1, lp1, hl1, off1, len1, listflag, off2, vartype1;
	INT32 x1;
	UCHAR work45[45], formvar1[32], formvar2[32], *adr1, *adr2, *adr3, *ptr2;

	if (!searchflg) {
		searchflg = 1;
		if (!prpget("search", NULL, NULL, NULL, (CHAR **) &ptr2, PRP_LOWER) && !strcmp((CHAR *) ptr2, "old")) searchflg = 2;
	}

	flags = dbcflags & DBCFLAG_FLAGS;
	adr1 = getvar(VAR_READ);
	fp1 = fp;
	lp1 = lp;
	hl1 = hl;
	vartype1 = vartype;
	adr2 = getvar(VAR_READ);
	if (vartype & (TYPE_LIST | TYPE_ARRAY)) {
		adr2 = getlist(LIST_READ | LIST_IGNORE | LIST_LIST | LIST_ARRAY | LIST_NUM1);
		listflag = TRUE;
	}
	else listflag = FALSE;
	i3 = nvtoi(getvar(VAR_READ));
	adr3 = getvar(VAR_WRITE);

	if (!fp1) i3 = 0;
	else if (vartype1 & TYPE_NUMERIC) {  /* numeric variable */
		if (vartype1 & (TYPE_INT | TYPE_FLOAT | TYPE_NULL)) {
			nvmkform(adr1, formvar1);
			adr1 = formvar1;
			fp1 = fp;
			lp1 = lp;
			hl1 = hl;
		}
		off1 = hl1;
		len1 = lp1;
		if (searchflg == 2) vartype1 = TYPE_CHAR;
	}
	else {  /* character variable */
		off1 = fp1 + hl1 - 1;
		len1 = lp1 - fp1 + 1;
	}

	x1 = 0;
	for (i4 = 1; i4 <= i3; i4++) {
		if (!listflag) {
			if (i4 > 1) adr2 += skipvar(adr2, TRUE);  /* skip to next variable */
			scanvar(adr2);
		}
		else {
			if (i4 > 1) adr2 = getlist(LIST_READ | LIST_IGNORE | LIST_LIST | LIST_ARRAY | LIST_NUM1);
			if (adr2 == NULL) {
				i4 = i3 + 1;
				break;
			}
		}
		if ((vartype & TYPE_NUMVAR) && (vartype1 & TYPE_NUMVAR)) {
			mathop(0x00, adr1, adr2, NULL);
			if (dbcflags & DBCFLAG_EQUAL) {
				x1 = i4;
				break;
			}
		}
		else {
			ptr2 = adr2;
			if (vartype & TYPE_NUMVAR) {  /* numeric variable */
				if (vartype & (TYPE_INT | TYPE_FLOAT | TYPE_NULL)) {
					nvmkform(adr2, formvar2);
					ptr2 = formvar2;
				}
				if (len1 > lp) continue;
				off2 = 1;
			}
			else if (vartype & TYPE_CHAR) {  /* character variable */
				if (!fp || len1 > (lp - fp + 1)) continue;
				off2 = fp + hl - 1;
			}
			else if (*adr2 == 0xF2) break;  /* end of data */
			else {
				i4--;
				continue;
			}
			for (i1 = off1, i2 = 0; i2 < len1 && adr1[i1++] == ptr2[off2++]; i2++);
			if (i2 == len1) {
				x1 = i4;
				break;
			}
		}
	}
	if (listflag && i4 <= i3) getlist(LIST_END | LIST_NUM1);

	work45[0] = 0xFC;
	work45[1] = 0x05;
	memcpy(&work45[2], (UCHAR *) &x1, sizeof(INT32));
	movevar(work45, adr3);
	dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | flags;
	if (x1) {
		dbcflags |= DBCFLAG_EQUAL;
		dbcflags &= ~DBCFLAG_OVER;
	}
	else {
		dbcflags |= DBCFLAG_OVER;
		dbcflags &= ~DBCFLAG_EQUAL;
	}
}

void vcount()
{
	INT i1, i2, count;
	INT32 x1;
	UCHAR work45[45], *adr1, *adr2;

	count = 0;
	adr1 = getvar(VAR_WRITE);
	while ((adr2 = getlist(LIST_READ | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1)) != NULL) {
		if (vartype & TYPE_NUMERIC) {  /* numeric variable */
			if (vartype & (TYPE_INT | TYPE_FLOAT | TYPE_NULL)) {
				nvmkform(adr2, work45);
				adr2 = work45;
			}
			i1 = pl;
			count += i1;
			for (i2 = 1; i2 <= i1; i2++, count--) {
				if (adr2[i2] != ' ' && adr2[i2] != '0') break;
			}
			while (i2 <= i1) {
				if (adr2[i2++] == '.') {
					for ( ; adr2[i1] == '0'; i1--, count--);
					if (adr2[i1] == '.') count--;
					break;
				}
			}
		}
		else {  /* character variable */
			if (fp) {
				count += lp - fp + 1;
				for (i1 = hl + lp - 1, i2 = hl + fp - 1; i1 >= i2 && adr2[i1] == ' '; i1--, count--);
			}
		}
	}
	x1 = count;
	work45[0] = 0xFC;
	work45[1] = 0x05;
	memcpy(&work45[2], (UCHAR *) &x1, sizeof(INT32));
	i1 = dbcflags & DBCFLAG_LESS;
	movevar(work45, adr1);
	dbcflags = (dbcflags & ~DBCFLAG_LESS) | i1;
}

void vbrpf()  /* branch and perform */
{
	INT i, j, n;

	n = nvtoi(getvar(VAR_READ));
	i = 1;
	while ((j = getbyte()) != 0xFF) {
		if (i++ == n) {
			j = (j << 8) + getbyte();
			if (vbcode != 0x05) {  /* not branch (perform) */
				while (getbyte() != 0xFF) pcount++;
				if (pushreturnstack(-1)) dbcerror(501);
			}
			chgpcnt(j);
			return;
		}
		pcount++;
	}
}

INT typ(UCHAR *s)  /* s is char variable, return TRUE or FALSE if valid number */
/* no flags affected */
{
	INT i1, i2, state;
	UCHAR c1 = '\0';

	scanvar(s);
	if (!fp) return(FALSE);
	i1 = fp + hl - 1;
	i2 = lp - fp + 1;
	if (i2 > 31) return(FALSE);
	if (i2 == 1 && s[i1] == '.') return(FALSE);  /* "." */
	if (i2 == 2 && s[i1] == '-' && s[i1 + 1] == '.') return(FALSE);  /* "-." */
	for (state = 0; i2--; i1++) {
		c1 = s[i1];
		if (!state) {
			if (c1 == ' ') continue;
			else if (c1 == '-' || (c1 >= '0' && c1 <= '9')) state = 1;
			else if (c1 == '.') state = 2;
			else return(FALSE);
		}
		else if (state == 1) {
			if (c1 >= '0' && c1 <= '9') continue;
			else if (c1 == '.') state = 2;
			else return(FALSE);
		}
		else if (state == 2) {
			if (c1 >= '0' && c1 <= '9') continue;
			else return(FALSE);
		}
	}
	if (c1 == '-' || state == 0) return(FALSE);
	return(TRUE);
}

void chk1011(INT chk, UCHAR *s1, UCHAR *s2, UCHAR *s3)  /* check10, check11 */
{
	INT i1, i2, fp1, fp2, lp1, lp2, hl1, hl2, hl3, sum;

	dbcflags &= ~DBCFLAG_EQUAL;
	dbcflags |= DBCFLAG_OVER;
	scanvar(s1);
	fp1 = fp;
	lp1 = lp;
	hl1 = hl - 1;
	scanvar(s2);
	fp2 = fp;
	lp2 = lp;
	hl2 = hl - 1;
	scanvar(s3);
	hl3 = hl;
	fp = lp = 1;
	setfplp(s3);
	s3[hl3] = ' ';
	if (!fp1 || !fp2) return;  /* zero formpointer check */
	lp1 = lp1 - fp1 + 1;  /* lp1 is now logical length */
	lp2 = lp2 - fp2 + 1;  /* ditto lp2 */
	if (lp1 - 1 != lp2) return;
	fp1 += hl1;  /* fp1 now points to first byte of logical string */
	fp2 += hl2;  /* same for fp2 */
	sum = i2 = 0;
	for (i1 = 0; i1 < lp2; i1++) {
		i2 = (s1[fp1 + i1] - '0') * (s2[fp2 + i1] - '0');
		if (chk == 10 && i2 > 9) i2 = i2 / 10 + i2 % 10;  /* check10 does lateral addition */
		sum += i2;
	}
	i2 = chk - sum % chk;
	if (chk == 10) {
		if (i2 == 10) i2 = 0;
		i2 += '0';
	}
	if (chk == 11) {
		if (i2 == 10) i2 = 'A';
		else if (i2 == 11) i2 = 'B';
		else i2 += '0';
	}
	s3[hl3] = (UCHAR) i2;
	if (i2 != 'A' && i2 != 'B' && i2 == (INT) s1[fp1 + lp2]) {
		dbcflags |= DBCFLAG_EQUAL;
		dbcflags &= ~DBCFLAG_OVER;
	}
}

void vscan()
{
	INT i1, i2, i3, i4, fp1, lp1, hl1, fp2, lp2, hl2;
	UCHAR *adr1, *adr2;

	dbcflags &= ~(DBCFLAG_EOS | DBCFLAG_EQUAL);
	adr1 = getvar(VAR_READ);
	fp1 = fp;
	lp1 = lp;
	adr1 += fp + hl - 1;
	adr2 = getvar(VAR_WRITE);
	fp2 = fp;
	lp2 = lp;
	hl2 = hl;
	if (!fp1 || !fp2) {
		dbcflags |= DBCFLAG_EOS;
		return;
	}
	hl1 = lp1 - fp1 + 1;
	i4 = lp2 - fp2 + 2 - hl1;
	for (i3 = 0; i3 < i4; i3++) {
		for (i1 = 0, i2 = fp2 + hl2 + i3 - 1; i1 < hl1; i1++, i2++) if (adr1[i1] != adr2[i2]) break;
		if (i1 == hl1) {
			fp = fp2 + i3;
			setfp(adr2);
			dbcflags |= DBCFLAG_EQUAL;
			break;
		}
	}
}

void vpack(INT lenflag)
{
	INT i1, i2, i3, fp1, lp1, hl1;
	UCHAR *adr1, *adr2, work45[45];

	adr1 = getvar(VAR_WRITE);
	fp1 = 0;
	lp1 = pl;
	hl1 = hl;
	dbcflags &= ~DBCFLAG_EOS;
	while ((adr2 = getlist(LIST_READ | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1)) != NULL) {
		if ((vartype & TYPE_NUMVAR) && (vartype & (TYPE_INT | TYPE_FLOAT | TYPE_NULL))) {
			nvmkform(adr2, work45);
			adr2 = work45;
		}
		if (dbcflags & DBCFLAG_EOS) continue;
		if (!fp) {
			if (!lenflag) continue;
			i2 = 0;
			i3 = pl;
		}
		else if (!lenflag) {
			i2 = lp - fp + 1;
			i3 = 0;
		}
		else {
			fp = 1;
			i2 = lp;
			i3 = pl - lp;
		}
		i1 = lp1 - fp1;
		if (i1 < i2 + i3) {
			dbcflags |= DBCFLAG_EOS;
			if (i1 < i2) {
				i2 = i1;
				i3 = 0;
			}
			else i3 = i1 - i2;
		}
		if (i2) memcpy(&adr1[fp1 + hl1], &adr2[hl + fp - 1], i2);
		if (i3) memset(&adr1[fp1 + hl1 + i2], ' ', i3);
		fp1 += i2 + i3;
	}
	if (!fp1) fp = lp = 0;
	else {
		fp = 1;
		lp = fp1;
	}
	setfplp(adr1);
}

void vunpack()
{
	INT i1, i2;
	UCHAR *adr1, *adr2, work45[45];

	dbcflags &= ~(DBCFLAG_EOS | DBCFLAG_OVER);
	adr1 = getvar(VAR_READ);
	if (fp) i1 = lp - fp + 1;
	else {
		dbcflags |= DBCFLAG_EOS;
		i1 = 0;
	}
	adr1 += fp + hl - 1;
	while ((adr2 = getlist(LIST_WRITE | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1)) != NULL) {
		if (vartype & TYPE_NUMVAR) {  /* its numeric */
			if (!i1) {
				work45[0] = 0x81;
				work45[1] = '0';
				pl = 0;
			}
			else {
				if (vartype & (TYPE_INT | TYPE_FLOAT)) nvmkform(adr2, work45);  /* just to get pl */
				work45[0] = 1;
				work45[1] = work45[2] = (UCHAR) pl;
				memcpy(&work45[3], adr1, pl);
			}
			i2 = dbcflags & DBCFLAG_FLAGS;
			movevar(work45, adr2);
			if (dbcflags & DBCFLAG_EOS) i2 |= DBCFLAG_OVER;
			dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i2;
			i1 -= pl;
			if (i1 < 0) i1 = 0;
			adr1 += pl;
		}
		else if (vartype & TYPE_CHAR){  /* character */
			if (!i1 || !pl) fp = lp = 0;
			else {
				lp = pl;
				if (lp > i1) lp = i1;
				memcpy(&adr2[hl], adr1, lp);
				adr1 += lp;
				fp = 1;
				pl -= lp;
				i1 -= lp;
			}
			if (pl) memset(&adr2[hl + lp], ' ', pl);
			setfplp(adr2);
		}
	}
}

void vrep()
{
	INT i1, i2, fp1, lp1;
	UCHAR *adr1, *adr2, *adr3;

	adr1 = getvar(VAR_READ);
	fp1 = fp;
	lp1 = lp;
	adr1 += fp + hl - 1;
	adr2 = getvar(VAR_WRITE);
	adr2 += fp + hl - 1;
	dbcflags &= ~DBCFLAG_EOS;
	if (!fp1 || !fp || ((i1 = (lp1 - fp1 + 1)) & 0x01)) {  /* null operand or logical len is odd */
		dbcflags |= DBCFLAG_EOS;
		return;
	}
	i2 = lp - fp + 1;
	while (i2--) {
		adr3 = adr1 + i1;
		while (adr3 != adr1) {
			adr3 -= 2;
			if (*adr3 == *adr2) {
				*adr2 = *(adr3 + 1);
				break;
			}
		}
		adr2++;
	}
}

/* VFORMAT */
void vformat(UCHAR *adr1, UCHAR *adr2, UCHAR *adr3)
{
	INT i1, fp1, fp2, lp1, lp2, lft1, lft2, vartype1, fillcnt;
	INT firstflg, negflg1, negflg2, periodflg, zeroflg;
	UINT floatcnt;
	UCHAR c1, floatchr[16], fillchr, work[32];

	dbcflags |= DBCFLAG_EOS;
	scanvar(adr1);
	if ((vartype & TYPE_NUMVAR) && (vartype & (TYPE_INT | TYPE_FLOAT | TYPE_NULL))) {
		nvmkform(adr1, work);
		adr1 = work;
	}
	if (!fp) {
		lp = 0;
		setfplp(adr3);
		return;
	}
	fp1 = fp + hl - 1;
	lp1 = lp + hl;
	vartype1 = vartype;
	scanvar(adr2);
	if (!fp) {
		lp = 0;
		setfplp(adr3);
		return;
	}
	fp2 = fp + hl - 1;
	lp2 = lp + hl;
	scanvar(adr3);
	lp = hl;
	pl += hl;

	dbcflags &= ~(DBCFLAG_EOS | DBCFLAG_OVER | DBCFLAG_LESS);
	if (vartype1 & TYPE_NUMERIC) {  /* source is a numeric variable */
		negflg2 = FALSE;
		lft2 = 0;
		i1 = fp2;
		while (i1 < lp2) {
			c1 = adr2[i1++];
			if (c1 == 'Z' || c1 == '9' || c1 == '~') lft2++;
			else if (c1 == '.' || c1 == '^') break;
			else if (c1 == '<' || c1 == '>' || c1 == '\\' || c1 == '&') i1++;
			else if (c1 == '-' || c1 == '(' || c1 == ')' || c1 == '+') negflg2 = TRUE;
		}
		while (i1 < lp2 && !negflg2) {
			c1 = adr2[i1++];
			if (c1 == '<' || c1 == '>' || c1 == '\\') i1++;
			else if (c1 == '-' || c1 == '(' || c1 == ')' || c1 == '+') negflg2 = TRUE;
		}

		negflg1 = 0;
		while (fp1 < lp1 && adr1[fp1] == ' ') fp1++;
		if (adr1[fp1] == '-') {
			fp1++;
			if (!negflg2) negflg1 = 1;
		}
		else negflg2 = FALSE;
		zeroflg = TRUE;
		i1 = fp1;
		while (i1 < lp1 && adr1[i1] != '.') if (adr1[i1++] != '0') zeroflg = FALSE;
		lft1 = i1 - fp1;
		if (zeroflg && i1++ < lp1) {
			while (i1 < lp1 && adr1[i1] == '0') i1++;
			if (i1 < lp1) zeroflg = FALSE;
		}
		if (lft1 > lft2) {
			fp1 += lft1 - lft2;
			lft1 = lft2;
		}

		fillcnt = 0;
		fillchr = ' ';
		floatcnt = 0;
		periodflg = FALSE;
		firstflg = TRUE;
		while (fp2 < lp2) {
			c1 = adr2[fp2++];
			if (c1 == 'Z' || c1 == '9') {
				if (!periodflg) {
					if (--lft2 < lft1) c1 = adr1[fp1++];
					else if (c1 == 'Z' || (firstflg && fillcnt < (INT) floatcnt + negflg1)) c1 = fillchr;
					else c1 = '0';
				}
				else if (fp1 < lp1) c1 = adr1[fp1++];
				else c1 = '0';
			}
			else if (c1 == '~') {
				if (!periodflg) {
					if (--lft2 < lft1) fp1++;
				}
				else if (fp1 < lp1) fp1++;
				continue;
			}
			else if (c1 == ',') {
				if (firstflg) c1 = fillchr;
			}
			else if (c1 == '-' || c1 == '(' || c1 == ')') {
				if (!negflg2) c1 = fillchr;
			}
			else if (c1 == '+') {
				if (negflg2) c1 = '-';
				else if (zeroflg) c1 = fillchr;
			}
			else if (c1 == '.') {
				periodflg = TRUE;
				if (fp1 < lp1) fp1++;
			}
			else if (c1 == '^') {
				periodflg = TRUE;
				if (fp2 == lp2) {
					dbcflags |= DBCFLAG_OVER;
					break;
				}
				c1 = adr2[fp2++];
				if (fp1 < lp1) fp1++;
			}
			else if (c1 == '<') {
				if (fp2 == lp2) {
					dbcflags |= DBCFLAG_OVER;
					break;
				}
				fillchr = adr2[fp2++];
				continue;
			}
			else if (c1 == '$') {
				if (floatcnt < sizeof(floatchr)) floatchr[floatcnt++] = '$';
				continue;
			}
			else if (c1 == '>') {
				if (fp2 == lp2) {
					dbcflags |= DBCFLAG_OVER;
					break;
				}
				if (floatcnt < sizeof(floatchr)) floatchr[floatcnt++] = adr2[fp2++];
				continue;
			}
			else if (c1 == '&') {
				if (fp2 == lp2) {
					dbcflags |= DBCFLAG_OVER;
					break;
				}
				c1 = adr2[fp2++];
				if (firstflg) c1 = fillchr;
			}
			else if (c1 == '\\') {
				if (fp2 == lp2) {
					dbcflags |= DBCFLAG_OVER;
					break;
				}
				c1 = adr2[fp2++];
			}
			if (firstflg) {
				if (isdigit(c1) || periodflg) {
					firstflg = FALSE;
					i1 = lp;
					if (negflg1 && --fillcnt >= 0) adr3[--i1] = '-';
					while (floatcnt && --fillcnt >= 0) adr3[--i1] = floatchr[--floatcnt];
					if (fillcnt < 0) dbcflags |= DBCFLAG_LESS;
				}
				else if (c1 == fillchr) fillcnt++;
				else fillcnt = 0;
			}
			if (lp == pl) {
				dbcflags |= DBCFLAG_EOS;
				break;
			}
			adr3[lp++] = c1;
		}
	}
	else {  /* source is a character variable */
		while (fp2 < lp2) {
			c1 = adr2[fp2++];
			if (c1 == 'A') {
				if (fp1 < lp1) c1 = adr1[fp1++];
				else c1 = ' ';
			}
			else if (c1 == '~') {
				if (fp1 < lp1) fp1++;
				continue;
			}
			else if (c1 == '\\') {
				if (fp2 == lp2) {
					dbcflags |= DBCFLAG_OVER;
					break;
				}
				c1 = adr2[fp2++];
			}
			if (lp == pl) {
				dbcflags |= DBCFLAG_EOS;
				break;
			}
			adr3[lp++] = c1;
		}
	}
	lp -= hl;
	if (lp) fp = 1;
	else fp = 0;
	setfplp(adr3);
}

void vunpacklist()
{
	INT listflg, mod, off;
	UCHAR *adr;

	getvar(LIST_READ);
	listflg = LIST_ADRX | LIST_LIST1 | LIST_NUM1;
	for ( ; ; ) {
		if (listflg) {
			if (!getlistx(listflg, &mod, &off)) listflg = 0;
			else listflg &= ~LIST_LIST1;
		}
		if ((adr = getlist(LIST_ADR | LIST_PROG | LIST_ARRAY | LIST_NUM2)) == NULL) break;
		if (listflg) {
			adr[1] = (UCHAR) mod;
			adr[2] = (UCHAR)(mod >> 8);
			adr[3] = (UCHAR) off;
			adr[4] = (UCHAR)(off >> 8);
			adr[5] = (UCHAR)(off >> 16);
			chkavar(adr, FALSE);
		}
		else memset(&adr[1], 0xFF, 5);
	}
	if (listflg) getlist(LIST_END | LIST_NUM1);
}

void vsql(INT verbx)  /* sqlexec, sqlcode and sqlmsg verbs and chain */
{
	static INT lastcode;
	INT i1, i2;
	CHAR *ptr;
	UCHAR *adr1, *parmadr[1500];
	const UINT parmsize = sizeof(parmadr) / sizeof(*parmadr);

	if (verbx == 0x80) {  /* sqlexec */
		adr1 = getvar(VAR_READ);
		for (i1 = 0; (parmadr[i1] = getlist(LIST_READ | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1)) != NULL; )
		{
			if (i1 < (INT) (parmsize - 1)) i1++;
		}
		if (pgm[pcount - 1] == 0xFE) {
			if (i1 < (INT) (parmsize - 1)) i1++;
			for (i2 = i1; (parmadr[i2] = getlist(LIST_WRITE | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1)) != NULL; )
				if (i2 < (INT) (parmsize - 1)) i2++;
		}

#if DBC_SQL
		if (!sqlinitflag) loadsql();
#endif
		lastcode = dbcsqlx(adr1, parmadr, &parmadr[i1]);
		dbcflags &= ~(DBCFLAG_EQUAL | DBCFLAG_LESS | DBCFLAG_OVER);
		if (lastcode == 100) dbcflags |= DBCFLAG_OVER;
		else if (lastcode < 0) dbcflags |= DBCFLAG_LESS;
		else if (lastcode == 0) dbcflags |= DBCFLAG_EQUAL;
	}
	else if (verbx == 0x81) itonv(lastcode, getvar(VAR_WRITE));  /* sqlcode */
	else if (verbx == 0x82) {  /* sqlmsg */
		adr1 = getvar(VAR_WRITE);
#if DBC_SQL
		if (!sqlinitflag) loadsql();
#endif
		ptr = dbcsqlm();
		if (ptr == NULL || !(lp = (INT)strlen(ptr))) fp = lp = 0;
		else {
			if (lp > pl) lp = pl;
			memcpy(&adr1[hl], ptr, lp);
			fp = 1;
		}
		setfplp(adr1);
	}

	/**
	 * A zero argument tells us to close all open SQL connections
	 * In Unix, if we are not initialized, there can't be any, so do nothing.
	 */
	else if (verbx == 0) {
#if OS_UNIX
#if DBC_SQL
		if (sqlinitflag) dbcsqlq();
#endif
#else
		dbcsqlq();   // Windows
#endif
	}
}

UCHAR *getint(UCHAR *nvint)
{
	INT num, negflg;
	INT32 x1;
	UCHAR size;

	if (pgm[pcount] >= 0xFE) {
		negflg = FALSE;
		if (pgm[pcount++] == 0xFE) negflg = TRUE;
		num = pgm[pcount++];
		if (num >= 100) size = 3;
		else if (num >= 10) size = 2;
		else size = 1;
		if (negflg && num) {
			num = -num;
			size++;
		}
		nvint[0] = 0xFC;
		nvint[1] = size;
		x1 = num;
		memcpy(&nvint[2], (UCHAR *) &x1, sizeof(INT32));
		fp = lp = -1;
		pl = 4;
		hl = 2;
		vartype = TYPE_INT;
		return(nvint);
	}
	return(getvar(VAR_READ));
}

void nvmkform(UCHAR *nvar, UCHAR *formvar)  /* make a form variable from the numeric variable at nvar */
/* set fp, lp, pl, hl */
{
	INT i1, i2, i3, lft, rgt, zeroflg;
	INT32 x1;
	CHAR work[64];
	CHAR *ptr;
	DOUBLE64 y1;

	if (*nvar == 0xFC) {  /* integer variable */
		lft = nvar[1] & ~0x80;
		memcpy((UCHAR *) &x1, &nvar[2], sizeof(INT32));
		msciton((INT) x1, &formvar[1], lft);
	}
	else if (*nvar >= 0xF4 && *nvar <= 0xFB) {  /* float variable */
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
		i1 = (INT)strlen(ptr);
		if (i1 - i2 < rgt) {
			memcpy(work, ptr, i1);
			memset(&work[i1], '0', rgt - (i1 - i2));
			work[rgt + i2] = '\0';
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
	else if ((*nvar & 0xE0) == 0x80) {  /* form variable */
		if (*nvar == 0x80) {  /* null */
			lft = nvar[1] & 0x1F;
			memcpy(formvar, nvar, lft + 1);
			setnullvar(formvar, FALSE);
		}
		else {
			lft = nvar[0] & 0x1F;
			memcpy(&formvar[1], &nvar[1], lft);
		}
	}
	else if (*nvar == 0xE0) {  /* numeric literal */
		lft = nvar[1];
		memcpy(&formvar[1], &nvar[2], lft);
	}
	else {  /* shouldn't happen */
		lft = 1;
		formvar[1] = '0';
	}

	formvar[0] = (UCHAR) (0x80 | lft);
	fp = hl = 1;
	pl = lp = lft;
	vartype = TYPE_NUM;
	return;
}

INT nvtoi(UCHAR *nvar)  /* return the truncated value of a numeric variable */
{
	INT i1;
	INT32 x1;
	UCHAR c1, work[33], *ptr;
	DOUBLE64 y1;

	c1 = nvar[0];
	if ((c1 & 0xE0) == 0x80) {  /* form variable */
		if (c1 == 0x80) {
			memcpy(work, nvar, 1 + (nvar[1] & 0x1F));
			setnullvar(work, FALSE);
			nvar = work;
		}
		i1 = nvar[0] & 0x1F;
		ptr = &nvar[i1 + 1];
		c1 = *ptr;
		*ptr = '\0';
		i1 = atoi_internal((CHAR *) &nvar[1]);
		*ptr = c1;
	}
	else if (c1 == 0xE0) {  /* numeric literal */
		i1 = nvar[1];
		ptr = &nvar[i1 + 2];
		c1 = *ptr;
		*ptr = '\0';
		i1 = atoi_internal((CHAR *) &nvar[2]);
		*ptr = c1;
	}
	else if (c1 == 0xFC) {
		memcpy((UCHAR *) &x1, &nvar[2], sizeof(INT32));  /* integer variable */
		i1 = (INT) x1;
	}
	else if (c1 >= 0xF4 && c1 <= 0xFB) {  /* float */
		memcpy((UCHAR *) &y1, &nvar[2], sizeof(DOUBLE64));
		i1 = (INT) y1;
	}
	else i1 = 0;
	return(i1);
}

OFFSET nvtooff(UCHAR *nvar)  /* return the truncated value of a numeric variable */
{
	INT i1;
	OFFSET num;
	INT32 x1;
	UCHAR c1, work[33], *ptr;
	DOUBLE64 y1;

	c1 = nvar[0];
	if ((c1 & 0xE0) == 0x80) {  /* form variable */
		if (c1 == 0x80) {
			memcpy(work, nvar, 1 + (nvar[1] & 0x1F));
			setnullvar(work, FALSE);
			nvar = work;
		}
		i1 = nvar[0] & 0x1F;
		ptr = &nvar[i1 + 1];
		c1 = *ptr;
		*ptr = '\0';
		mscatooff((CHAR *) &nvar[1], &num);
		*ptr = c1;
	}
	else if (c1 == 0xE0) {  /* numeric literal */
		i1 = nvar[1];
		ptr = &nvar[i1 + 2];
		c1 = *ptr;
		*ptr = '\0';
		mscatooff((CHAR *) &nvar[2], &num);
		*ptr = c1;
	}
	else if (c1 == 0xFC) {
		memcpy((UCHAR *) &x1, &nvar[2], sizeof(INT32));  /* integer variable */
		num = (OFFSET) x1;
	}
	else if (c1 >= 0xF4 && c1 <= 0xFB) {  /* float */
		memcpy((UCHAR *) &y1, &nvar[2], sizeof(DOUBLE64));
		num = (OFFSET) y1;
	}
	else num = 0;
	return(num);
}

DOUBLE64 nvtof(UCHAR *nvar)  /* return the value of a numeric variable */
{
	INT i1;
	INT32 x1;
	UCHAR c1, work[33], *ptr;
	DOUBLE64 y1;

	c1 = nvar[0];
	if ((c1 & 0xE0) == 0x80) {  /* form variable */
		if (c1 == 0x80) {
			memcpy(work, nvar, 1 + (nvar[1] & 0x1F));
			setnullvar(work, FALSE);
			nvar = work;
		}
		i1 = nvar[0] & 0x1F;
		ptr = &nvar[i1 + 1];
		c1 = *ptr;
		*ptr = '\0';
		y1 = (DOUBLE64) atof((CHAR *) &nvar[1]);
		*ptr = c1;
	}
	else if (c1 == 0xE0) {  /* numeric literal */
		i1 = nvar[1];
		ptr = &nvar[i1 + 2];
		c1 = *ptr;
		*ptr = '\0';
		y1 = (DOUBLE64) atof((CHAR *) &nvar[2]);
		*ptr = c1;
	}
	else if (c1 == 0xFC) {  /* integer variable */
		memcpy((UCHAR *) &x1, &nvar[2], sizeof(INT32));
		y1 = (DOUBLE64) x1;
	}
	else if (c1 >= 0xF4 && c1 <= 0xFB) memcpy((UCHAR *) &y1, &nvar[2], sizeof(DOUBLE64));  /* float */
	else y1 = (DOUBLE64) 0;
	return(y1);
}

void itonv(INT num, UCHAR *nvar)  /* move a integer to numeric variable */
{
	INT i1;
	INT32 x1;
	UCHAR work[6];

	x1 = num;
	work[0] = 0xFC;
	work[1] = 0x1F;
	memcpy(&work[2], (UCHAR *) &x1, sizeof(INT32));
	i1 = dbcflags & DBCFLAG_FLAGS;
	movevar(work, nvar);
	dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1;
}

/**
 * Move dim variable logical string to name.
 * Terminate with a null.
 */
void cvtoname(UCHAR *cvar)
{
	if (cvar == NULL) {
		name[0] = '\0';
		return;
	}
	scanvar(cvar);
	if ((vartype & TYPE_CHARACTER) && fp) {
		cvar += hl + fp - 1;
		lp -= fp - 1;
		if (lp >= (INT) sizeof(name)) lp = sizeof(name) - 1;
		while (lp && cvar[lp - 1] == ' ') lp--;
		memcpy(name, cvar, lp);
	}
	else lp = 0;
	name[lp] = '\0';
}

/**
 * Move dim variable logical string to dbcbuf.
 * Terminate with a null.
 */
void cvtodbcbuf(UCHAR *cvar) {
	if (cvar == NULL) {
		dbcbuf[0] = '\0';
		return;
	}
	scanvar(cvar);
	if ((vartype & TYPE_CHARACTER) && fp) {
		cvar += hl + fp - 1;
		lp -= fp - 1;
		if (lp >= (INT) sizeof(name)) lp = sizeof(name) - 1;
		while (lp && cvar[lp - 1] == ' ') lp--;
		memcpy(dbcbuf, cvar, lp);
	}
	else lp = 0;
	dbcbuf[lp] = '\0';
}

/*
 * set fp, lp, pl and hl
 * and vartype
 */
void scanvar(UCHAR *adr)
{
	UCHAR c1;

	c1 = *adr;
	if (c1 < 0x80) {  /* small dim */
		fp = c1;
		lp = adr[1];
		pl = adr[2];
		hl = 3;
		vartype = TYPE_CHAR;
		if (!fp && lp == 0xFF) {  /* null */
			lp = 0;
			vartype |= TYPE_NULL;
		}
	}
	else if (c1 < 0xA0) {  /* form */
		pl = lp = c1 & 0x1F;
		fp = hl = 1;
		vartype = TYPE_NUM;
		if (!pl) {  /* null */
			pl = lp = adr[1] & 0x1F;
			vartype |= TYPE_NULL;
		}
	}
	else if (c1 == 0xE0 || c1 == 0xE1) {  /* num or char literal */
		if (c1 == 0xE1) vartype = TYPE_CHARLIT;
		else vartype = TYPE_NUMLIT;
		pl = lp = adr[1];
		if (lp) fp = 1;
		else fp = 0;
		hl = 2;
	}
	else if (c1 == 0xFC) {  /* integer */
		fp = lp = -1;
		pl = 4;
		hl = 2;
		vartype = TYPE_INT;
		if (adr[1] & 0x80) vartype |= TYPE_NULL;  /* null */
	}
	else if (c1 == 0xF0) {  /* large dim */
		fp = llhh(&adr[1]);
		lp = llhh(&adr[3]);
		pl = llhh(&adr[5]);
		hl = 7;
		vartype = TYPE_CHAR;
		if (!fp && lp == 0xFFFF) {  /* null */
			lp = 0;
			vartype |= TYPE_NULL;
		}
	}
	else if (c1 >= 0xF4 && c1 <= 0xFB) {  /* float */
		fp = lp = -1;
		pl = 8;
		hl = 2;
		vartype = TYPE_FLOAT;
		if (c1 <= 0xF7) vartype |= TYPE_NULL;
	}
	else {
		if (c1 == 0xA4) vartype = TYPE_LIST;
		else if (c1 >= 0xA6 && c1 <= 0xA8) vartype = TYPE_ARRAY;
		else vartype = 0;
		fp = lp = pl = hl = 0;
	}
}

void setnullvar(UCHAR *adr, INT flag)
{
	static UCHAR x8130[2] = { 0x81, 0x30 };
	INT i1;

	if (flag) {  /* set null */
		if (adr[0] < 0x80) {  /* small char */
			memset(&adr[3], ' ', adr[2]);
			adr[0] = 0;
			adr[1] = 0xFF;
		}
		else if (adr[0] == 0xF0) {  /* large char */
			memset(&adr[7], ' ', llhh(&adr[5]));
			adr[1] = adr[2] = 0;
			adr[3] = adr[4] = 0xFF;
		}
		else if ((adr[0] >= 0x80 && adr[0] < 0xA0) || (adr[0] >= 0xF8 && adr[0] <= 0xFC)) {  /* num, float, int */
			i1 = dbcflags & DBCFLAG_FLAGS;
			movevar(x8130, adr);
			dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1;
			if (adr[0] < 0xA0) {
				if (adr[1] != '.') adr[1] = adr[0] & 0x1F;
				else adr[1] = adr[0];
				adr[0] = 0x80;
			}
			else if (adr[0] == 0xFC) adr[1] |= 0x80;
			else adr[0] -= 0x04;
		}
	}
	else {  /* unset null */
		if (adr[0] == 0x00) {  /* null small char */
			if (adr[1] == 0xFF) adr[1] = 0;
		}
		else if (adr[0] == 0x80) {  /* null num */
			adr[0] |= adr[1];
			if (adr[1] & 0x80) adr[1] = '.';
			else if ((adr[0] & 0x1F) == 1) adr[1] = '0';
			else adr[1] = ' ';
		}
		else if (adr[0] == 0xF0) {  /* large char */
			if (llhh(&adr[1]) == 0 && llhh(&adr[3]) == 0xFFFF) adr[3] = adr[4] = 0;  /* null */
		}
		else if (adr[0] == 0xFC) {  /* int */
			if (adr[1] & 0x80) adr[1] &= ~0x80;  /* null */
		}
		else if (adr[0] >= 0xF4 && adr[0] <= 0xF7) adr[0] += 0x04;  /* null float */
	}
}

/**
 * Convert a string of digits in base 10,
 * maybe with a leading sign, to an integer.
 * Do not check for overflow!
 *
 * We need this since this is the way in which to GCC atoi function works.
 * But the MSVC one does not. We do not want overflow detection.
 * If it overflows, tough! Just keep going.
 *
 * The guts of this were copied from strtol.c version 1.1 from the Free Software Foundation
 *
 */
int atoi_internal(const char *nptr) {

	int negative;
	register int i;
	register const char *s;
	register unsigned char c;

	s = nptr;

	  /* Skip white space.  */
	  while (isspace ((int)*s)) ++s;
	  if (*s == '\0') goto noconv;

	  /* Check for a sign.  */
	  if (*s == '-')
	    {
	      negative = 1;
	      ++s;
	    }
	  else if (*s == '+')
	    {
	      negative = 0;
	      ++s;
	    }
	  else
	    negative = 0;

	i = 0;
	for (c = *s; c != '\0'; c = *++s)
	{
		if (isdigit (c)) c -= '0';
		else break;
		i *= 10;
		i += c;
	}

	/* Return the result of the appropriate sign.  */
	return (negative ? -i : i);

	noconv:
	  /* There was no number to convert.  */
	  return 0;
}

#if OS_UNIX && DBC_SQL
static void freesql()
{
	dlclose(sqlhandle);
}

static void loadsql()
{
	CHAR *error;
#ifdef __MACOSX
	CHAR *work = "dbcsql.dylib";
	sqlhandle = dlopen(work, RTLD_NOW);
	if (sqlhandle == NULL) {
		error = (CHAR*) dlerror();
		dbcerrinfo(653, (UCHAR*) error, strlen(error));
	}
#else
	CHAR *work = "dbcsql.so";
	sqlhandle = dlopen(work, RTLD_NOW);
	if (sqlhandle == NULL) {
		error = (CHAR*) dlerror();
		dbcerrinfo(653, (UCHAR*) error, strlen(error));
	}
#endif

	dbcsqlx = (_DBCSQLX *) dlsym(sqlhandle, "dbcsqlx");
	dbcsqlm = (_DBCSQLM *) dlsym(sqlhandle, "dbcsqlm");
	dbcsqlq = (_DBCSQLQ *) dlsym(sqlhandle, "dbcsqlq");
	if (dbcsqlx == NULL || dbcsqlm == NULL || dbcsqlq == NULL) {
		dlclose(sqlhandle);
		dbcerror(655);
	}
	sqlinitflag = 1;
	atexit(freesql);
}
#endif
