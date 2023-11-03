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
#define INC_STDINT
#define INC_LIMITS
#include "includes.h"
#include "base.h"
#include "dbc.h"
#include "fio.h"

static void movearray(INT, UCHAR *, INT, INT, UCHAR *, INT, INT, INT, INT *);

void vmove(INT listflg2)
{
	INT arrayflg, flags, listflg1, mod2, off1, off2, type;
	INT mod1 = INT_MIN;
	UCHAR c1, work6[6], *adr1, *adr2;

	arrayflg = listflg1 = 0;
	adr1 = getint(work6);
	if (vartype & (TYPE_LIST | TYPE_ARRAY)) {
		if (vartype & TYPE_LIST) {
			adr1 = getlist(LIST_READ | LIST_IGNORE | LIST_LIST | LIST_NUM1);
			listflg1 = LIST_READ | LIST_IGNORE | LIST_LIST | LIST_NUM1;
			
			/*
			 * If the first item in the source list is an array,
			 * we must set the arrayflg, mod1, and off1 fields.
			 * Otherwise the for(;;) loop below will not operate correctly.
			 */
			if (vartype & TYPE_ARRAY) {
				getlastvar(&mod1, &off1);
				arrayflg = 0x01;
			}
			vartype = 0;
		}
		else {
			getlastvar(&mod1, &off1);
			arrayflg |= 0x01;
			c1 = adr1[((*adr1 - 0xA6) << 1) + 5];
			if (c1 < 0x80 || (c1 >= 0xA0 && c1 != 0xB1 && c1 != 0xE0 && (c1 < 0xF4 || c1 > 0xFC))) vartype = 0;  /* non-numeric */
		}
	}
	type = vartype;
	if (!(listflg2 & LIST_PROG)) {
		adr2 = getvar(VAR_WRITE);
		if (adr1 == NULL) return;  /* src list is empty */
		if ((vartype & (TYPE_NUMERIC | TYPE_CHARACTER)) && !arrayflg) {  /* simple move */
			movevar(adr1, adr2);
			if (listflg1) getlist(LIST_END | LIST_NUM1);
			return;
		}
		if (vartype & TYPE_LIST) {
			adr2 = getlist(LIST_WRITE | LIST_IGNORE | LIST_LIST | LIST_NUM2);
			if (adr2 == NULL) {  /* dest list is empty */
				if (listflg1) getlist(LIST_END | LIST_NUM1);
				return;
			}

			/*
			 * If the first item in the destination list is an array,
			 * we must set the arrayflg, mod2, and off2 fields.
			 * Otherwise the for(;;) loop below will not operate correctly.
			 */
			if (vartype & TYPE_ARRAY) {
				getlastvar(&mod2, &off2);
				arrayflg |= 0x02;
			}

			listflg2 = LIST_WRITE | LIST_IGNORE | LIST_LIST | LIST_NUM2;
			type = 0;
		}
		else if (vartype & TYPE_ARRAY) {
			getlastvar(&mod2, &off2);
			arrayflg |= 0x02;
			c1 = adr2[((*adr2 - 0xA6) << 1) + 5];
			if (c1 < 0x80 || (c1 >= 0xA0 && c1 != 0xB1 && c1 != 0xE0 && (c1 < 0xF4 || c1 > 0xFC))) type = 0;  /* non-numeric */
		}
	}
	else {
		adr2 = getlist(LIST_WRITE | LIST_PROG | LIST_IGNORE | LIST_LIST | LIST_NUM2);
		if (adr1 == NULL) {  /* src list is empty */
			getlist(LIST_END | LIST_PROG | LIST_NUM2);
			return;
		}
		if (adr2 == NULL) {  /* dest list is empty */
			if (listflg1) getlist(LIST_END | LIST_NUM1);
			return;
		}
		if (vartype & TYPE_ARRAY) {
			getlastvar(&mod2, &off2);
			arrayflg |= 0x02;
		}
		listflg2 = LIST_WRITE | LIST_PROG | LIST_IGNORE | LIST_LIST | LIST_NUM2;
		type = 0;
	}

	if (type & TYPE_NUMERIC) flags = (dbcflags & DBCFLAG_EOS) | DBCFLAG_EQUAL;
	else flags = dbcflags & (DBCFLAG_EQUAL | DBCFLAG_LESS | DBCFLAG_OVER);

	for ( ; ; ) {
		if (!arrayflg) {
			movevar(adr1, adr2);
			if (type & TYPE_NUMERIC) {
				flags &= dbcflags & DBCFLAG_EQUAL;
				flags |= dbcflags & (DBCFLAG_LESS | DBCFLAG_OVER);
			}
			else flags |= dbcflags & DBCFLAG_EOS;
		}
		else {
			if (mod1 == INT_MIN && arrayflg & 0x01) dbcerrinfo(1798, (UCHAR*)"In vmove, mod1 never assigned", -1);
			movearray(arrayflg, adr1, mod1, off1, adr2, mod2, off2, type, &flags);
			if (!listflg2) break;
		}
		if (listflg1) {
			if ((adr1 = getlist(listflg1)) == NULL) {
				if (listflg2) getlist(LIST_END | LIST_NUM2);
				break;
			}
			if (vartype & TYPE_ARRAY) {
				getlastvar(&mod1, &off1);
				arrayflg |= 0x01;
			}
			else arrayflg &= ~0x01;
		}
		if ((adr2 = getlist(listflg2)) == NULL) {
			if (listflg1) getlist(LIST_END | LIST_NUM1);
			break;
		}
		if (vartype & TYPE_ARRAY) {
			getlastvar(&mod2, &off2);
			arrayflg |= 0x02;
		}
		else arrayflg &= ~0x02;
	}
	dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | flags;
}

static void movearray(INT arrayflg, UCHAR *adr1, INT mod1, INT off1, UCHAR *adr2, INT mod2, INT off2, INT type, INT *flags)
{
	if ((arrayflg & 0x03) == 0x03 && memcmp(adr1, adr2, ((*adr1 - 0xA6) << 1) + 3)) {
		*flags |= DBCFLAG_OVER;
		return;
	}
	if (arrayflg & 0x01) {
		setlastvar(mod1, off1);
		adr1 = getlist(LIST_READ | LIST_ARRAY1 | LIST_NUM3);
	}
	if (arrayflg & 0x02) {
		setlastvar(mod2, off2);
		adr2 = getlist(LIST_WRITE | LIST_ARRAY1 | LIST_NUM4);
	}
	for ( ; ; ) {
		movevar(adr1, adr2);
		if (type & TYPE_NUMERIC) {
			*flags &= dbcflags & DBCFLAG_EQUAL;
			*flags |= dbcflags & (DBCFLAG_LESS | DBCFLAG_OVER);
		}
		else *flags |= dbcflags & DBCFLAG_EOS;
		if (arrayflg & 0x01) {
			if ((adr1 = getlist(LIST_READ | LIST_NUM3)) == NULL) {
				if (arrayflg & 0x02) getlist(LIST_END | LIST_NUM4);
				break;
			}
		}
		if (!(arrayflg & 0x02)) {
			getlist(LIST_END | LIST_NUM3);
			break;
		}
		if ((adr2 = getlist(LIST_WRITE | LIST_NUM4)) == NULL) {
			if (arrayflg & 0x01) getlist(LIST_END | LIST_NUM3);
			break;
		}
	}
}

void movevar(UCHAR *src, UCHAR *dest)  /* move one variable to another variable */
{
	static UCHAR roundflg = 0;
	INT i1, i2, i3, i4;
	INT stype, dtype;
	INT srcfp, srclp, srcsp, destpl, destsp;
	INT numflg, flags, rd, rs;
	INT32 x1;
	UCHAR c1, *ptr, work45[45], wrk1[67], wrk2[67];
	DOUBLE64 y1;

	if (src == dest) {  /* this should only happen from vmove */
		if (vartype & TYPE_NUMVAR) {  /* it is a numeric variable */
			if (vartype & TYPE_NULL) setnullvar(dest, FALSE);
			memcpy(work45, dest, hl + pl);
			dest = work45;
		}
		else if ((vartype & TYPE_CHAR) && fp > 1) {
			memmove(&dest[hl], &dest[hl + fp - 1], lp - fp + 1);
			lp -= fp - 1;
			fp = 1;
			setfplp(dest);
			return;
		}
		else return;
	}

	if (!roundflg) {
		roundflg = 2;
		if (!prpget("rounding", NULL, NULL, NULL, (CHAR **) &ptr, PRP_LOWER) && !strcmp((CHAR *) ptr, "old")) roundflg = 1;
	}
	if (!(src[0] & 0x80)) {  /* small dim */
		stype = 1;
		srcfp = src[0];
		srcsp = srcfp + 2;
		srclp = src[1];
	}
	else if (src[0] < 0xA0) stype = 5;  /* form */
	else if (src[0] == 0xFC) stype = 6;  /* integer */
	else if (src[0] == 0xE0 || src[0] == 0xE1) {  /* literal */
		if (src[0] == 0xE0) stype = 4;
		else stype = 3;
		srcfp = 1;
		srcsp = 2;
		srclp = src[1];
		if (!srclp) srcfp = 0;
	}
	else if (src[0] == 0xF0) {  /* big dim */
		stype = 2;
		srcfp = (src[2] << 8) + src[1];
		srcsp = srcfp + 6;
		srclp = (src[4] << 8) + src[3];
	}
	else if (src[0] >= 0xF4 && src[0] <= 0xFB) stype = 7;  /* float */
	else return;

	if (!(dest[0] & 0x80)) {  /* small dim */
		dtype = 1;
		destpl = dest[2];
		destsp = 3;
		if (dest[0] == 0x00 && dest[1] == 0xFF) dest[1] = 0x00;
	}
	else if (dest[0] < 0xA0) {
		dtype = 3;  /* form */
		if (dest[0] == 0x80) setnullvar(dest, FALSE);
	}
	else if (dest[0] == 0xF0) {  /* big dim */
		dtype = 2;
		destpl = (dest[6] << 8) + dest[5];
		destsp = 7;
		if (dest[1] == 0x00 && dest[2] == 0x00 && dest[3] == 0xFF && dest[4] == 0xFF) dest[3] = dest[4] = 0x00;
	}
	else if (dest[0] == 0xFC) {
		dtype = 4;  /* integer */
		dest[1] &= ~0x80;
	}
	else if (dest[0] >= 0xF4 && dest[0] <= 0xFB) {
		dtype = 5;  /* float */
		if (dest[0] <= 0xF7) dest[0] += 0x04;
	}
	else return;

	if (dtype < 3) {  /* destination is char var */
		dbcflags &= ~DBCFLAG_EOS;
		if (stype <= 3) {
			if (!srcfp) {
				if (dtype == 1) dest[0] = 0;
				else dest[1] = dest[2] = 0;
				return;
			}
			i3 = srclp - srcfp + 1;
		}
		else {  /* number variable */
			if (stype >= 5) {
				if (stype >= 6 || src[0] == 0x80) {  /* int, float or null form */
					nvmkform(src, wrk2);
					src = wrk2;
				}
				i3 = src[0] & 0x1F;
				srcsp = 1;
			}
			else i3 = srclp;
		}
		if (destpl < i3) {
			dbcflags |= DBCFLAG_EOS;
			i3 = destpl;
		}
		if (dtype == 1) {  /* small dim var */
			dest[0] = 1;
			dest[1] = (UCHAR) i3;
		}
		else {  /* large dim var */
			dest[1] = 1;
			dest[2] = 0;
			dest[3] = (UCHAR) i3;
			dest[4] = (UCHAR) (i3 >> 8);
		}
		memcpy(&dest[destsp], &src[srcsp], i3);
		return;
	}

	/* destination is some sort of numeric variable */
	if (stype <= 3) {  /* source is char var */
		if (!typ(src)) {
			dbcflags |= DBCFLAG_EOS;
			return;
		}
		dbcflags &= ~DBCFLAG_EOS;
		numflg = FALSE;
		i3 = srclp - srcfp + 1;
		if (i3 > 31) i3 = 31;
	}

	if (dtype == 3) {  /* destination is a form variable */
		if (stype >= 4) {  /* source is number var */
			if (stype >= 5) {
				if (stype >= 6 || src[0] == 0x80) {  /* int, float or null form */
					nvmkform(src, wrk2);
					src = wrk2;
				}
				i3 = src[0] & 0x1F;
				srcsp = 1;
			}
			else i3 = srclp;
			numflg = TRUE;
		}
		memcpy(&wrk1[2], &src[srcsp], i3);
		if (wrk1[++i3] == '.') i3--;  /* i3=length of src & truncate trailing . */
		wrk1[1] = ' ';  /* wrk1[0] is not used, [1]=extra space for rounding */
		for (i1 = 0; i1 < i3 && wrk1[++i1] != '.'; );  /* see if rounding will take place */
		rs = i3 - i1;  /* rs is number of right dec digits in src */
		i4 = dest[0] & 0x1F;  /* i4 is total length of dest */
		for (i1 = 0; i1 < i4 && dest[++i1] != '.'; );
		rd = i4 - i1;  /* rd is number of right dec digits in dest */
		if (rd < rs) {  /* must round the source */
			c1 = wrk1[i3 - rs + rd + 1];  /* c1 is rounding digit */
			if (c1 == '5') {
				c1 = '6';
				if (roundflg == 1) {
					i2 = rs - rd - 1;
					i1 = i3 - rs + rd + 2;
					while (i2-- && wrk1[i1++] == '0');
					if (i2 < 0) {  /* digits to right were zero */
						i1 = 0;
						while (wrk1[++i1] == ' ');
						if (wrk1[i1] == '-') c1 = '5';
					}
				}
			}
			if (c1 > '5') {  /* round source */
				for (i1 = i3 - rs + rd; i1 > 0; i1--) {
					if ((c1 = wrk1[i1]) == '.') continue;
					else if (c1 >= '0' && c1 < '9') {
						wrk1[i1] = (UCHAR)(c1 + 1);
						break;
					}
					else if (c1 == '9') wrk1[i1] = '0';
					else if (c1 == ' ') {
						wrk1[i1] = '1';
						break;
					}
					else if (c1 == '-') {
						wrk1[i1--] = '1';
						if (i1 != 0) wrk1[i1] = '-';
						break;
					}
				}
			}
			/* make the source shorter by however many digits we rounded */
			if (rd == 0) i3 = i3 - rs - 1;  /* drop dec point too */
			else i3 = i3 - rs + rd;
			rs = rd;
		}
		/* suppress leading zero in source */
		for (i1 = 1; i1 <= i3; i1++) {
			if (wrk1[i1] == ' ') continue;
			else if (wrk1[i1] == '-') {
				if (wrk1[i1+1] == ' ' || wrk1[i1+1] == '0') {
					wrk1[i1+1] = '-';
				}
				else break;
			}
			else if (wrk1[i1] != '0') break;
			wrk1[i1] = ' ';
		}
		/* move characters from right to left */
		i1 = i3;
		i2 = i3 = i4;  /* save length of dest in i3 */
		flags = DBCFLAG_EQUAL;
		while (i4--) {
			if (rd > rs) {
				dest[i2--] = '0';
				if (!(--rd)) {  /* skip over dec pt */
					i4--;
					i2--;
				}
			}
			else if (i1 > 0) {
				c1 = dest[i2--] = wrk1[i1--];
				if (c1 == '-') {
					if (flags & DBCFLAG_EQUAL) dest[i2+1] = ' ';  /* rounded to 0 */
					else flags |= DBCFLAG_LESS;
				}
				if ((flags & DBCFLAG_EQUAL) && c1 > '0' && c1 <= '9') flags &= ~DBCFLAG_EQUAL;
			}
			else dest[i2--] = ' ';
		}
		if (dest[i3] == ' ') dest[i3] = '0';
		if (numflg) dbcflags = (dbcflags & ~(DBCFLAG_EQUAL | DBCFLAG_LESS | DBCFLAG_OVER)) | flags;
		if (i1 > 0 && wrk1[i1] != ' ') {
			if (numflg) {
				dbcflags |= DBCFLAG_OVER;
				while (i1 > 0) if (wrk1[i1--] == '-') {
					dbcflags |= DBCFLAG_LESS;
					break;
				}
			}
			else dbcflags |= DBCFLAG_EOS;
		}
		/* remove all leading zeros */
		for (i1 = 1; i1 < i3 && (dest[i1] == '0' || dest[i1] == ' ' || dest[i1] == '-'); i1++) {
			if (dest[i1] == '-') {
				if (dest[i1 + 1] == '0' || dest[i1 + 1] == ' ') dest[i1 + 1] = '-';
				else break;
			}
			dest[i1] = ' ';
		}
		return;
	}
	if (dtype == 4) {  /* destination is an integer variable */
		if (stype <= 5) {  /* source is dim or form variable */
			if (stype == 5) {  /* source is a form variable */
				if (src[0] == 0x80) {  /* null form */
					nvmkform(src, wrk2);
					src = wrk2;
				}
				srcsp = 1;
				i3 = src[0] & 0x1F;
			}
			else if (stype == 4) {
				i3 = srclp;
			}
			ptr = &src[srcsp + i3];
			c1 = *ptr;
			*ptr = '\0';
			x1 = atoi_internal((CHAR *) &src[srcsp]);
			*ptr = c1;
		}
		else if (stype == 6) {
			memcpy((UCHAR *) &x1, &src[2], sizeof(INT32));
		}
		else if (stype == 7) {
			memcpy((UCHAR *) &y1, &src[2], sizeof(DOUBLE64));
			x1 = (INT32) y1;
		}
		memcpy(&dest[2], (UCHAR *) &x1, sizeof(INT32));
		dbcflags &= ~(DBCFLAG_EQUAL | DBCFLAG_LESS | DBCFLAG_OVER);
		if (!x1) dbcflags |= DBCFLAG_EQUAL;
		else if (x1 < 0) dbcflags |= DBCFLAG_LESS;
		return;
	}
	if (dtype == 5) {  /* destination is a float variable */
		if (stype <= 5) {  /* source is dim or form variable */
			if (stype == 5) {  /* source is a form variable */
				if (src[0] == 0x80) {  /* null form */
					nvmkform(src, wrk2);
					src = wrk2;
				}
				srcsp = 1;
				i3 = src[0] & 0x1F;
			}
			else if (stype == 4) i3 = srclp;
			ptr = &src[srcsp + i3];
			c1 = *ptr;
			*ptr = 0;
			y1 = atof((CHAR *) &src[srcsp]);
			*ptr = c1;
		}
		else if (stype == 6) {
			memcpy((UCHAR *) &x1, &src[2], sizeof(INT32));
			y1 = (DOUBLE64) x1;
		}
		else if (stype == 7) memcpy((UCHAR *) &y1, &src[2], sizeof(DOUBLE64));
		memcpy(&dest[2], (UCHAR *) &y1, sizeof(DOUBLE64));
		dbcflags &= ~(DBCFLAG_EQUAL | DBCFLAG_LESS | DBCFLAG_OVER);
		if (y1 == (DOUBLE64) 0) dbcflags |= DBCFLAG_EQUAL;
		else if (y1 < (DOUBLE64) 0) dbcflags |= DBCFLAG_LESS;
		return;
	}
}

/* VMOVEEXP */
void vmoveexp(INT vx)
{
	int32_t x1;
	UCHAR *adr1;

	adr1 = getvar(VAR_WRITE);
	switch (vx) {
		case 0x47: x1 = dbcflags & DBCFLAG_EQUAL; break;
		case 0x48: x1 = dbcflags & DBCFLAG_LESS; break;
		case 0x49: x1 = dbcflags & DBCFLAG_OVER; break;
		case 0x4A: x1 = dbcflags & DBCFLAG_EOS; break;
		case 0x4B: x1 = !(dbcflags & DBCFLAG_EQUAL); break;
		case 0x4C: x1 = !(dbcflags & DBCFLAG_LESS); break;
		case 0x4D: x1 = !(dbcflags & DBCFLAG_OVER); break;
		case 0x4E: x1 = !(dbcflags & DBCFLAG_EOS); break;
		case 0x4F: x1 = !(dbcflags & (DBCFLAG_EQUAL | DBCFLAG_LESS)); break;
		case 0x50: x1 = dbcflags & (DBCFLAG_EQUAL | DBCFLAG_LESS); break;
		default: dbcerrinfo(1798, (UCHAR*)"Bad internal opcode at vmoveexp", -1);
	}
	if (x1) x1 = 1;
	adr1[0] = 0xFC;
	adr1[1] = 0x01;
	memcpy(&adr1[2], (CHAR *) &x1, sizeof(int32_t));
	if (getmver(pgmmodule) <= 7) {
		if (!x1) dbcflags |= DBCFLAG_EQUAL;
		else dbcflags &= ~DBCFLAG_EQUAL;
	}
}

/* VSETOP */
void vsetop(INT vx)
{
	INT i1, i2, i3, len1, len2, fp1, relflg;
	INT32 x1;
	UCHAR *adr1, *adr2, work6a[6], work6b[6];

	adr1 = getint(work6a);
	if (vartype & TYPE_NUMERIC) {  /* numeric */
		adr2 = getint(work6b);
		i1 = dbcflags & DBCFLAG_FLAGS;
		mathop(0x00, adr1, adr2, NULL);
		relflg = 0;
		switch(vx) {
			case 0x56:  /* matcheq */
				relflg = dbcflags & DBCFLAG_EQUAL; break;
			case 0x57:  /* matchneq */
				relflg = !(dbcflags & DBCFLAG_EQUAL); break;
			case 0x58:  /* matchlt */
				relflg = dbcflags & DBCFLAG_LESS; break;
			case 0x59:  /* matchgt */
				relflg = !(dbcflags & (DBCFLAG_LESS | DBCFLAG_EQUAL)); break;
			case 0x5A:  /* matchleq */
				relflg = dbcflags & (DBCFLAG_LESS | DBCFLAG_EQUAL); break;
			case 0x5B:  /* matchgeq */
				relflg = !(dbcflags & DBCFLAG_LESS); break;
		}
		if (relflg) relflg = 1;
		dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1;
	}
	else {
		i1 = hl + fp - 1;
		len1 = lp - fp + 1;
		fp1 = fp;
		adr2 = getvar(VAR_READ);
		if (fp1 && fp) {
			len2 = lp - fp + 1;
			i3 = (len1 <= len2) ? len1 : len2;
			//if (len1 <= len2) i3 = len1;
			//else i3 = len2;
			if (!(i2 = memcmp(&adr1[i1], &adr2[hl + fp - 1], i3))) {
				if (len1 < len2) i2 = -1;
				else if (len1 > len2) i2 = 1;
			}
		}
		else if (!fp1 && fp) i2 = -1;
		else if (fp1 && !fp) i2 = 1;
		else i2 = 0;
		
		if ((!i2 && (vx == 0x56 || vx == 0x5A || vx == 0x5B)) ||
		   (i2 < 0 && (vx == 0x57 || vx == 0x58 || vx == 0x5A)) ||
		   (i2 > 0 && (vx == 0x57 || vx == 0x59 || vx == 0x5B))) relflg = 1;
		else relflg = 0;
	}
	x1 = relflg;
	adr1 = getvar(VAR_WRITE);
	adr1[0] = 0xFC;
	adr1[1] = 0x01;
	memcpy(&adr1[2], (CHAR *) &x1, sizeof(INT32));
	if (dbcflags & DBCFLAG_PRE_V7) {
		if (!relflg) dbcflags |= DBCFLAG_EQUAL;
		else dbcflags &= ~DBCFLAG_EQUAL;
	}
	if (relflg) dbcflags |= DBCFLAG_EXPRESSION;
	else dbcflags &= ~DBCFLAG_EXPRESSION;
}

/* VNUMOP */
void vnumop(INT vx)
{
	static UCHAR x00000000[4] = { 0x00, 0x00, 0x00, 0x00 };
	static UCHAR x8130[2] = { 0x81, 0x30 };
	INT i1, fp1, fp2, relflg;
	INT32 x1;
	UCHAR *adr1, work6a[6];

	i1 = dbcflags & DBCFLAG_FLAGS;
	adr1 = getint(work6a);
	if (vartype & TYPE_NUMERIC) {  /* numeric */
		fp1 = 1;
		if (vartype & TYPE_INT) {
			if (!memcmp(&adr1[2], x00000000, 4)) fp1 = 0;
		}
		else {
			mathop(0x00, x8130, adr1, NULL);
			if (dbcflags & DBCFLAG_EQUAL) fp1 = 0;
		}
	}
	else fp1 = fp;
	if (vx != 63) {
		adr1 = getint(work6a);
		fp2 = 1;
		if (vartype & TYPE_INT) {
			if (!memcmp(&adr1[2], x00000000, 4)) fp2 = 0;
		}
		else {
			mathop(0x00, x8130, adr1, NULL);
			if (dbcflags & DBCFLAG_EQUAL) fp2 = 0;
		}
	}
	dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1;
	adr1 = getvar(VAR_WRITE);
	if ((vx == 63 && !fp1) || (vx == 82 && fp1 && fp2) || (vx == 83 && (fp1 || fp2))) relflg = 1;
	else relflg = 0;
	x1 = relflg;
	adr1[0] = 0xFC;
	adr1[1] = 0x01;
	memcpy(&adr1[2], (CHAR *) &x1, sizeof(INT32));
	if (dbcflags & DBCFLAG_PRE_V7) {
		if (!relflg) dbcflags |= DBCFLAG_EQUAL;
		else dbcflags &= ~DBCFLAG_EQUAL;
	}
	if (relflg) dbcflags |= DBCFLAG_EXPRESSION;
	else dbcflags &= ~DBCFLAG_EXPRESSION;
}

/* VNUMMOVE */
void vnummove(INT vx)
{
	INT i1, i2, lft1, rgt1, numflg;
	UCHAR *adr1, *adr2, work6[6], work32[32];

	adr1 = getint(work6);
	numflg = TRUE;
	rgt1 = 0;
	if (vartype & TYPE_INT) lft1 = adr1[1] & ~0x80;
	else if (vartype & TYPE_FLOAT) {  /* float */
		lft1 = ((adr1[1] >> 5) | ((adr1[0] << 3) & 0x18)) & 0x1F;
		rgt1 = adr1[1] & 0x1F;
	}
	else if (vartype & TYPE_NUMERIC) {  /* form or literal */
		if (vartype & TYPE_NULL) {
			memcpy(work32, adr1, hl + pl);
			setnullvar(work32, FALSE);
			adr1 = work32;
		}
		for (i1 = hl, i2 = hl + lp; i1 < i2 && adr1[i1] != '.'; i1++);
		if (i1 < i2) {
			lft1 = i1 - hl;			
			rgt1 = i2 - i1 - 1;
		}
		else lft1 = lp;
	}
	else {  /* character or literal */
		if (vx > 0xD7) {  /* moving to numeric variable */
			i1 = hl + fp - 1;
			lft1 = lp - fp + 1;
			if (fp && typ(adr1)) {
				lp = lft1;
				for (i2 = i1 + lft1; i1 < i2 && adr1[i1] != '.'; i1++);
				if (i1 < i2) {
					rgt1 = i2 - i1;
					lft1 -= rgt1--;
				}
				if (lp > 31) {  /* check if total length > 31 */
					lp -= 31;
					if (lp > lft1) {
						lft1 = 0;
						rgt1 = 30;
					}
					else lft1 -= lp;
				}
			}
			else {
				work6[0] = 0x81;
				work6[1] = 0x30;
				adr1 = work6;
				lft1 = 1;
			}
		}
		else {
			numflg = FALSE;
			if (fp) {
				adr1 += hl + fp - 1;
				lft1 = lp - fp + 1;
				if (lft1 > 256) lft1 = 256;
			}
			else lft1 = 0;
		}
	}

	if (rgt1) i1 = lft1 + rgt1 + 1;
	else i1 = lft1;
	adr2 = getvar(VAR_WRITE);
	if (vx == 0xD7) {  /* _cvtochar */
		if (i1) fp = 1;
		else fp = 0;
		lp = i1;
		setfplp(adr2);
		adr2[5] = (UCHAR) i1;
		adr2[6] = (UCHAR)(i1 >> 8);
	}
	else if (vx == 0xD8) {  /* _cvtoform */
		adr2[0] = (UCHAR)(0x80 | i1);
		memset(&adr2[1], ' ', lft1);
		if (rgt1) {
			adr2[lft1 + 1] = '.';
			memset(&adr2[lft1 + 2], '0', rgt1);
		}
		else adr2[lft1] = '0';
	}
	else if (vx == 0xD9) {  /* _cvtoint */
		adr2[0] = 0xFC;
		adr2[1] = (UCHAR) lft1;
	}
	else if (vx == 0xDA) {  /* _cvtofloat */
		adr2[0] = (UCHAR)(0xF8 | (lft1 >> 3));
		adr2[1] = (UCHAR)((lft1 << 5) | rgt1);
	}
	else {  /* version 7 nummove */
	}

	if (numflg) {
		i1 = dbcflags & DBCFLAG_FLAGS;
		movevar(adr1, adr2);
		dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1;
	}
	else if (i1) memcpy(&adr2[hl], adr1, i1);
}

void vedit(INT flag)  /* flag = TRUE means three operand form */
{
	INT i1, i2, i3, i4, negflg, zspflg, dptflg, dlrflg;
	INT fp1, lp1, hl1, vartype1;
	UCHAR c1, c2, work45[45];
	UCHAR *adr1, *adr2, *adr3;
	
	adr1 = getvar(VAR_READ);
	fp1 = fp;
	lp1 = lp;
	hl1 = hl;
	vartype1 = vartype;
	if (!flag) adr2 = getvar(VAR_WRITE);
	else {
		adr3 = getvar(VAR_READ);
		adr2 = getvar(VAR_WRITE);
		movevar(adr3, adr2);
		scanvar(adr2);
	}
	dbcflags &= ~(DBCFLAG_EOS | DBCFLAG_LESS);
	if (!fp1 || !fp) {
		dbcflags |= DBCFLAG_EOS;
		return;
	}

/* handle character strings */
	i2 = fp + hl - 1;
	i4 = lp + hl - 1;
	if (vartype1 & TYPE_CHARACTER) {  /* cvar */
		i1 = fp1 + hl1 - 1;
		i3 = lp1 + hl1 - 1;
		dbcflags &= ~DBCFLAG_OVER;
		while (i2 <= i4) {
			c1 = adr2[i2];
			if (c1 == 'A') {
				if (i1 <= i3) c1 = adr1[i1++];
				else c1 = ' ';
				if (!(isalpha(c1) || c1 == ' ')) dbcflags |= DBCFLAG_OVER;
			}
			else if (c1 == '9') {
				if (i1 <= i3) c1 = adr1[i1++];
				else c1 = ' ';
				if (!isdigit(c1)) dbcflags |= DBCFLAG_OVER;
			}
			else if (c1 == 'X') {
				if (i1 <= i3) c1 = adr1[i1++];
				else c1 = ' ';
			}
			else if (c1 == 'B') c1 = ' ';
			adr2[i2++] = c1;
		}
		if (i1 <= i3) dbcflags |= DBCFLAG_EOS;
	}
	else {  /* nvar */
		if (vartype1 & (TYPE_INT | TYPE_FLOAT | TYPE_NULL)) {
			nvmkform(adr1, work45);
			adr1 = work45;
		}
		negflg = FALSE;
		i3 = adr1[0] & 0x1F;
		for (i1 = 1; i1 < i3; i1++) if (adr1[i1] == '-') negflg = TRUE;
		i1 = 1;
		dlrflg = dptflg = FALSE;
		zspflg = TRUE;
		while (i2 <= i4) {
			c1 = adr2[i2];
			if (c1 == '9') {
				zspflg = FALSE;
				dptflg = TRUE;
				if (i1 <= i3) c1 = adr1[i1++];
				else c1 = '0';
				if (c1 == '.') c1 = adr1[i1++];
				else if (c1 == ' ' || c1 == '-') c1 = '0';
			}
			else if (c1 == '-') {
				if (!negflg) c1 = ' ';
			}
			else if (c1 == '+') {
				if (negflg) c1 = '-';
			}
			else if (c1 == ',') {
				if (zspflg) {
					if (adr2[i2 - 1] == '*') c1 = '*';
					else c1 = ' ';
				}
			}
			else if (c1 == '.') {
				zspflg = FALSE;
				dptflg = TRUE;
			}
			else if (c1 == 'B') c1 = ' ';
			else if (c1 == '*' || c1 == 'Z') {
				c2 = c1;
				if (i1 <= i3) c1 = adr1[i1++];
				else c1 = '0';
				if (c1 == '.') c1 = adr1[i1++];
				if (c1 == '-' || c1 == ' ') c1 = '0';
				if (!dptflg) {
					if (c1 == '0') {
						c1 = c2;
						if (c1 == 'Z') c1 = ' ';
					}
					else {
						dptflg = TRUE;
						zspflg = FALSE;
					}
				}
			}
			else if (c1 == '$') {
				if (i1 <= i3) c1 = adr1[i1++];
				else c1 = '0';
				if (c1 == '.') c1 = adr1[i1++];
				if (c1 == '-' || c1 == ' ') c1 = '0';
				if (!dptflg) {
					if (!dlrflg) {  /* first $ encountered */
						dlrflg = TRUE;
						if (c1 >= '1' && c1 <= '9') {
							dbcflags |= DBCFLAG_LESS;
							dptflg = TRUE;
							zspflg = FALSE;
						}
						c1 = '$';
					}
					else {
						if (c1 == '0') c1 = ' ';
						else {
							dptflg = TRUE;
							zspflg = FALSE;
						}
					}
				}
			}
			if (c1 == ' ' && adr2[i2 - 1] == '$' && (UCHAR) i2 != adr2[0]) {
				adr2[i2 - 1] = ' ';
				c1 = '$';
			}
			adr2[i2++] = c1;
		}
		if (i1 <= i3) dbcflags |= DBCFLAG_EOS;
	}
}
