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
#include "includes.h"
#include "base.h"
#include "dbc.h"
#include "fio.h"

/* local variables */
static UCHAR x8130[2] = { 0x81, 0x30 };
static UCHAR x00000000[4] = { 0x00, 0x00, 0x00, 0x00 };
static UCHAR precisionflg = 0;


void vmathexp(INT vx)  /* _math operations */
{
	INT i1, i2, lft1, lft2, lft3, rgt1, rgt2, rgt3, typ1, typ2;
	CHAR *ptr;
	UCHAR *adr1, *adr2, *adr3, work6a[6], work6b[6];

	if (!precisionflg) {
		precisionflg = 1;
		if (!prpget("precision", NULL, NULL, NULL, (CHAR **) &ptr, PRP_LOWER)) {
			if (!strcmp((CHAR *) ptr, "old")) precisionflg = 2;
			else if (!strcmp((CHAR *) ptr, "old8")) precisionflg = 3;
		}
	}

	adr1 = getint(work6a);
	vgetsize(adr1, &lft1, &rgt1);
	typ1 = vartype;
	
	adr2 = getint(work6b);
	vgetsize(adr2, &lft2, &rgt2);
	typ2 = vartype;

	if (vx == 0x01 || vx == 0x02) {  /* add or sub */
		lft3 = (lft1 > lft2) ? lft1 : lft2;
		lft3 += 2;
		rgt3 = (rgt1 > rgt2) ? rgt1 : rgt2;
	}
	else if (vx == 0x03) {  /* mult */
		lft3 = lft1 + lft2;
		rgt3 = rgt1 + rgt2;
	}
	else if (vx == 0x04 || precisionflg >= 2) {  /* div or mod w/ old precision */
		lft3 = lft2 + rgt1;
		if (precisionflg == 1) rgt3 = (rgt1 > rgt2) ? rgt1 : rgt2;
		else if (precisionflg == 2) rgt3 = lft1 + rgt2;
		else rgt3 = rgt1 + rgt2;
	}
	else {  /* mod */
		lft3 = (lft1 > lft2) ? lft1 : lft2;
		rgt3 = 0;
	}

	if (precisionflg == 1) vx |= (lft3 << 16) | (rgt3 << 8);

	adr3 = getvar(VAR_WRITE);
	if ((typ1 & TYPE_INT) && (typ2 & TYPE_INT)) {  /* result is int */
		if (lft3 > 31) lft3 = 31;
		adr3[0] = 0xFC;
		adr3[1] = (UCHAR) lft3;
	}
	else {
		i1 = (rgt3) ? 30 : 31;
		if (lft3 + rgt3 > i1) {  /* truncate on right */
			i2 = lft3 + rgt3 - i1;
			if (rgt1 < rgt2) rgt1 = rgt2;
			if (rgt3 > rgt1) {
				if (rgt3 - rgt1 > i2) rgt3 -= i2;
				else rgt3 = rgt1;
			}
			if (lft3 > i1 - rgt3) lft3 = i1 - rgt3;  /* truncate on left */
		}
		if ((typ1 & TYPE_FLOAT) || (typ2 & TYPE_FLOAT)) {  /* result is float */
			adr3[0] = (UCHAR)(0xF8 | (lft3 >> 3));
			adr3[1] = (UCHAR)((lft3 << 5) | rgt3);
		}
		else {  /* result is form */
			memset(&adr3[1], ' ', lft3);
			i1 = lft3;
			if (rgt3) {
				adr3[lft3 + 1] = '.';
				memset(&adr3[lft3 + 2], '0', rgt3);
				i1 += rgt3 + 1;
			}
			else adr3[lft3] = '0';
			adr3[0] = (UCHAR)(0x80 | i1);
		}
	}

	i1 = dbcflags & DBCFLAG_FLAGS;
	mathop(vx, adr1, adr2, adr3);
	if (!(dbcflags & DBCFLAG_PRE_V7)) dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1;
}

/**
 * math operation - src1 to src2 giving dest
 *
 * 0x00 = compare,
 * 0x01 = add,
 * 0x02 = sub,
 * 0x03 = mult,
 * 0x04 = div,
 * 0x05 = mod
 */
void mathop(INT op, UCHAR *src1, UCHAR *src2, UCHAR *dest)
{
	static UCHAR compareflg = 0;
	INT i, j, k, n, m, typeflag, expflg;
	INT len, lft, rgt;
	INT len1, sgn1, lft1, rgt1, typ1, zero1;
	INT len2, sgn2, lft2, rgt2, typ2, zero2;
	INT32 x1, x2, x3;
	UCHAR c, d, carry, *ptr1, *ptr2, *result;
	UCHAR formvar1[32], formvar2[32], wrk1[67], wrk2[67], wrk3[67];
	DOUBLE64 y1, y2, y3;

	if (!compareflg) {
		compareflg = 1;
		if (!prpget("compare", NULL, NULL, NULL, (CHAR **) &ptr1, PRP_LOWER) && !strcmp((CHAR *) ptr1, "old")) compareflg = 2;
	}
	if (!precisionflg) {
		precisionflg = 1;
		if (!prpget("precision", NULL, NULL, NULL, (CHAR **) &ptr1, PRP_LOWER)) {
			if (!strcmp((CHAR *) ptr1, "old")) precisionflg = 2;
			else if (!strcmp((CHAR *) ptr1, "old8")) precisionflg = 3;
		}
	}

	expflg = op & ~0x0F;
	op &= 0x0F;
	if (op == 0x00 && compareflg == 2) {
		op = 0x02;
		memcpy(wrk3, src2, 32);
		dest = wrk3;
	}

	if (src1[0] == 0xFC) {  /* first source is integer */
		memcpy((UCHAR *) &x1, &src1[2], sizeof(INT32));
		typ1 = 1;
	}
	else if (src1[0] < 0xF4) {  /* first source is form or literal */
		typ1 = 2;
	}
	else {  /* first source is float */
		memcpy((UCHAR *) &y1, &src1[2], sizeof(DOUBLE64));
		typ1 = 3;
	}
	
	if (src2[0] == 0xFC) {  /* second source is integer */
		memcpy((UCHAR *) &x2, &src2[2], sizeof(INT32));
		typ2 = 1;
	}
	else if (src2[0] < 0xF4) {  /* second source is form or literal */
		typ2 = 2;
	}
	else {  /* second source is float */
		memcpy((UCHAR *) &y2, &src2[2], sizeof(DOUBLE64));
		typ2 = 3;
	}

	ptr2 = src2;  /* save for divide */
	dbcflags &= ~DBCFLAG_OVER;
	switch(typ1 * 3 + typ2) {
		case 4:  /* INT : INT */
			typeflag = 1;
			break;
		case 5:  /* INT : form */
			mscitoa(x1, (CHAR *) &formvar1[1]);
			formvar1[0] = (UCHAR)(0x80 | strlen((CHAR *) &formvar1[1]));
			src1 = formvar1;
			typeflag = 2;
			break;
		case 6:  /* INT : float */
			y1 = (DOUBLE64) x1;
			typeflag = 3;
			break;
		case 7:  /* form : INT */
			mscitoa(x2, (CHAR *) &formvar2[1]);
			formvar2[0] = (UCHAR)(0x80 | strlen((CHAR *) &formvar2[1]));
			src2 = formvar2;
			// fall through
		case 8:  /* form : form */
			typeflag = 2;
			break;
		case 9:  /* form : float */
			y1 = nvtof(src1);
			typeflag = 3;
			break;
		case 10:  /* float : INT */
			y2 = (DOUBLE64) x2;
			typeflag = 3;
			break;
		case 11:  /* float : form */
			y2 = nvtof(src2);
			// fall through
		case 12:  /* float : float */
			typeflag = 3;
			break;
	}

	if (typeflag == 2) {  /* form : form  OR INT : form */
		if (src1[0] < 0xA0) {  /* not a literal */
			if (src1[0] == 0x80) {  /* null */
				memcpy(formvar1, src1, 32);
				setnullvar(formvar1, FALSE);
				src1 = formvar1;
			}
			i = len1 = src1[0] & 0x1F;
			j = 0;
		}
		else {
			i = len1 = src1[1] + 1;
			j = 1;
		}
		rgt1 = 0;
		sgn1 = FALSE;
		zero1 = TRUE;
mathop1:  /* look for non-digits */
		while (i > j && isdigit(src1[i])) {
			if (src1[i] != '0') zero1 = FALSE;
			i--;
		}
		if (i > j) {
			if (src1[i] == '.') {
				rgt1 = len1 - i;
				i--;
				goto mathop1;
			}
			if (src1[i] == '-') sgn1 = !zero1;
		}
		src1 += i + 1;
		len1 -= i;
		lft1 = (rgt1) ? len1 - rgt1 - 1 : len1;
		if (op == 4) lft = lft1 + i;

		if (src2[0] < 0xA0) {  /* not a literal */
			if (src2[0] == 0x80) {  /* null */
				memcpy(formvar2, src2, 32);
				setnullvar(formvar2, FALSE);
				src2 = formvar2;
			}
			i = len2 = src2[0] & 0x1F;
			j = 0;
		}
		else {
			i = len2 = src2[1] + 1;
			j = 1;
		}
		rgt2 = 0;
		sgn2 = FALSE;
		zero2 = TRUE;
mathop2:  /* look for non-digits */
		while (i > j && isdigit(src2[i])) {
			if (src2[i] != '0') zero2 = FALSE;
			i--;
		}
		if (i > j) {
			if (src2[i] == '.') {
				rgt2 = len2 - i;
				i--;
				goto mathop2;
			}
			if (src2[i] == '-') sgn2 = !zero2;
		}
		src2 += i + 1;
		len2 -= i;
		lft2 = (rgt2) ? len2 - rgt2 - 1 : len2;

		if (!op) {  /* compare */
			dbcflags &= ~(DBCFLAG_EQUAL | DBCFLAG_LESS);
			if (sgn1 != sgn2) {
				if (sgn2) dbcflags |= DBCFLAG_LESS;
				return;
			}
			if (zero1 != zero2) {
				if (zero2) dbcflags |= DBCFLAG_LESS;
				return;
			}
			if (zero1 && zero2) {
				dbcflags |= DBCFLAG_EQUAL;
				return;
			}
		}
		if (op <= 0x02) {  /* compare, add and subtract */
			rgt = (rgt1 > rgt2) ? rgt1 : rgt2;
			lft = (lft1 > lft2) ? lft1 : lft2;
			len = (rgt) ? lft + rgt + 3 : lft + 2;
			if (!zero1) {
				memset(&wrk1[1], '0', len);
				if (rgt) wrk1[lft + 3] = '.';
				memcpy(&wrk1[lft + 3 - lft1], src1, len1);
			}
			if (!zero2) {
				memset(&wrk2[1], '0', len);
				if (rgt) wrk2[lft + 3] = '.';
				memcpy(&wrk2[lft + 3 - lft2], src2, len2);
			}
			result = wrk2;
			if (zero1 || zero2) {  /* at least one element is zero */
				if (!zero1) {  /* adding to or subtracting from 0 */
					if ((op == 0x01 && sgn1) || (op == 0x02 && !sgn1)) wrk1[1] = '-';
					result = wrk1;
				}
				else if (!zero2) {  /* adding or subtracting 0 */
					if (sgn2) wrk2[1] = '-';
				}
				else {  /* both are zero */
					wrk2[1] = '0';
					len = 1;
				}
			}
			else {
				if ((op == 0x01 && sgn1 == sgn2) || (op == 0x02 && sgn1 != sgn2)) {  /* add */
					c = '0';
					for (i = len; i > 1; i--) {
						if (wrk2[i] == '.') continue;
						c = (UCHAR)(wrk2[i] + (wrk1[i] - c));
						if (c > '9') {
							wrk2[i] = (UCHAR)(c - 10);
							c = '0' - 1;
						}
						else {
							wrk2[i] = c;
							c = '0';
						}
					}
					if (sgn2) wrk2[1] = '-';
				}
				else {  /* subtract */
					for (j = 3; j <= len && wrk2[j] == wrk1[j]; j++);
					if (j > len) {
						if (!op) {
							dbcflags |= DBCFLAG_EQUAL;
							return;
						}
						wrk2[1] = '0';
						len = 1;
					}
					else {
						if (wrk2[j] > wrk1[j]) {
							if (!op) {
								if (sgn2) dbcflags |= DBCFLAG_LESS;
								return;
							}
							ptr1 = wrk1;
							ptr2 = wrk2;
							if (sgn2) wrk2[1] = '-';
						}
						else {
							if (!op) {
								if (!sgn2) dbcflags |= DBCFLAG_LESS;
								return;
							}
							ptr1 = wrk2;
							ptr2 = wrk1;
							result = wrk1;
							if (!sgn2) wrk1[1] = '-';
						}
						c = '0';
						for (i = len; i >= j; i--) {
							if (ptr2[i] == '.') continue;
							c += (UCHAR)(ptr2[i] - ptr1[i]);
							if (c < '0') {
								ptr2[i] = (UCHAR)(c + 10);
								c = '0' - 1;
							}
							else {
								ptr2[i] = c;
								c = '0';
							}
						}
						while (i >= 3) {
							if (ptr2[i] != '.') ptr2[i] = '0';
							i--;
						}
					}
				}
			}
			if (len <= 31) result[0] = (UCHAR)(0x80 | len);
			else {
				memmove(&result[2], &result[1], len);
				result[0] = 0xE0;
				result[1] = (UCHAR) len;
			}
		}
		else if (op == 0x03) {  /* multiply */
			if (zero1 || zero2) {
				result = x8130;
				goto mathop3;
			}
/*** CODE: IS IT POSSIBLE TO USE DWORDS TO PERFORM MULTIPLY WITHOUT ***/
/***       TOO MUCH OVERHEAD ???  LOOK INTO WHEN HAVE FREE TIME. THIS ***/
/***       WOULD ALLOW MULTIPLYING 4 DIGITS AT A TIME, BUT I BELIEVE ***/
/***       THE OVERHEAD WILL MAKE THIS WORTHLESS ***/
			for (i = j = 0; i < len1; i++) if (src1[i] != '.') wrk1[j++] = (UCHAR)(src1[i] & 0x0F);
			len1 = j;
			memset(wrk3, 0, 66);
			for ((k = (rgt1 || rgt2) ? 64 : 65), j = len2; j--; k--) {
				if ((d = src2[j]) == '.') {
					k++;
					continue;
				}
				if (!(d &= 0x0F)) continue;
				for (i = len1, m = k; i--; ) {
					c = wrk1[i] * d + wrk3[m];
					wrk3[m--] = (UCHAR)(c % 10);
					wrk3[m] += c / 10;  /* this will never be greater than 9 */
				}
			}
			n = k = len1 + len2 + 2;  /* one for possible sign and one for possible decimal point */
			j = 66 - n;  /* should never be less than 2 */
			for (ptr1 = &wrk3[j]; n--; ) *ptr1++ |= 0x30;
			i = rgt1 + rgt2;
			if (i) {
				memmove(&wrk3[66 - i], &wrk3[65 - i], i);
				wrk3[65 - i] = '.';
			}
			if (sgn1 != sgn2) wrk3[j] = '-';
			if (k <= 31) wrk3[--j] = (UCHAR)(0x80 | k);
			else {
				wrk3[--j] = (UCHAR) k;
				wrk3[--j] = 0xE0;
			}
			result = &wrk3[j];
		}
		else if (op == 0x04) {  /* divide */
			if (zero1) goto zerodiv;
			if (zero2) {
				result = x8130;
				goto mathop3;
			}

			/* calculate precision of right side */
			if (expflg & 0xFFFF00) rgt = (UCHAR)(expflg >> 8);  /* match precision calculated for expressions */
			else if (precisionflg >= 2 || ptr2 == dest) rgt = rgt1 + rgt2;  /* old precision or normal divide (ptr2 is original src2) */
			else {  /* divide giving */
				vgetsize(dest, &i, &rgt);
				rgt++;
			}

			/* calculate required precision of left side */
			if (rgt1) {
				while (rgt1 && src1[len1 - 1] == '0') { rgt1--; len1--; }
				if (!rgt1) len1--;
			}
			if (!rgt1) while (src1[len1 - 1] == '0') { rgt1--; len1--; }
			for (i = 0; i < len2 && src2[i] == '0'; i++);
			if (i < len2 && src2[i] == '.') {
				for (i++; i < len2 && src2[i] == '0'; i++);
				lft2++;  /* add one for the decimal */
			}
			if (i) {
				lft2 -= i;
				len2 -= i;
				src2 += i;
			}
			lft = lft2 + rgt1;  /* may be negative */

			/* calculate if enough right precision */
			for (i = 0; i < len1 && (src1[i] == '0' || src1[i] == '.'); i++);
			if (i) {
				len1 -= i;
				src1 += i;
			}
			i = len1;
			if (rgt1 > 0 && len1 > rgt1) i--;
			if (i > lft + rgt) {  /* insufficient right precision */
				result = x8130;
				goto mathop3;
			}

			/* prepare divisor */
			for (i = j = 0; j < len1; j++) if (src1[j] != '.') wrk1[i++] = src1[j] & 0x0F;
			len1 = i;

			/* prepare quotient */
			lft -= len1 - 1;
			if (lft > 0) j = lft;
			else j = 0;
			memset(&wrk3[3], '0', j + rgt + 1);
			i = 0;
			if (rgt) {
				wrk3[3 + j] = '.';
				if (lft < 0) {
					i = -lft + 1;
					lft = 0;
				}
				lft += rgt + 1;
			}

			/* prepare dividend */
			for (j = k = 0; j < len2; j++) if (src2[j] != '.') wrk2[k++] = src2[j] & 0x0F;
			if (lft - i + len1 - 1 > k) memset(&wrk2[k], 0, (lft - i + len1 - 1) - k);

			/* make compatable to version 8 */
			if (precisionflg >= 2 && !lft1) {
				lft--;
				if (rgt1 > len1) lft -= rgt1 - len1;
			}

			/* perform divide using subtraction */
			for (ptr2 = wrk2, lft += 3, i += 3; i < lft; ptr2++, i++) {
				if (wrk3[i] == '.') i++;
				if (wrk1[0] <= ptr2[0]) {
					if (len1 > 1) {
						for ( ; ; ) {  /* subtract divisor from dividend loop */
							for (j = 0; j < len1 && wrk1[j] == ptr2[j]; j++);
							if (j == len1 || wrk1[j] < ptr2[j]) {  /* divisor is same or smaller */
								wrk3[i]++;  /* add 1 to quotient digit */
								carry = 0;
								for (j = len1; j--; ) {
									k = (ptr2[j] - carry) - wrk1[j];
									if (k < 0) {
										k += 10;
										carry = 1;
									}
									else carry = 0;
									ptr2[j] = (UCHAR) k;
								}
							}
							else break;
						}
					}
					else if (wrk1[0] > 1) {
						wrk3[i] += ptr2[0] / wrk1[0];
						ptr2[0] %= wrk1[0];
					}
					else {
						wrk3[i] += ptr2[0];
						continue;
					}
				}
				ptr2[1] += ptr2[0] * 10;
			}
			j = 3;
			if (sgn1 != sgn2) wrk3[--j] = '-';
			k = lft - j;
			if (k <= 31) wrk3[--j] = (UCHAR)(0x80 | k);
			else {
				wrk3[--j] = (UCHAR) k;
				wrk3[--j] = 0xE0;
			}
			result = &wrk3[j];
		}
		else {  /* mod */
			/* mod ignores right sides */
			if (rgt1) len1 -= (rgt1 + 1);
			if (rgt2) len2 -= (rgt2 + 1);

			/* calculate required precision of left side */
			rgt1 = 0;
			while (len1 && src1[len1 - 1] == '0') { rgt1--; len1--; }
			if (!len1) goto zerodiv;
			for (i = 0; i < len2 && src2[i] == '0'; i++);
			if (i == len2) {
				result = x8130;
				goto mathop3;
			}
			if (i) {
				len2 -= i;
				src2 += i;
			}
			lft = len2 + rgt1;  /* may be negative */

			/* calculate if enough right precision */
			for (i = 0; i < len1 && src1[i] == '0'; i++);
			if (i) {
				lft1 -= i;
				len1 -= i;
				src1 += i;
			}
			if (len1 > lft) {  /* divisor bigger than dividend */
				memcpy(&wrk2[1], src2, len2);
				wrk2[0] = (UCHAR)(0x80 | len2);
				result = wrk2;
				goto mathop3;
			}

			/* prepare divisor */
			for (j = 0; j < len1; j++) wrk1[j] = src1[j] & 0x0F;

			/* prepare dividend */
			for (j = 0, i = 1; j < len2; j++) wrk2[i++] = src2[j] & 0x0F;

			/* perform divide using subtraction */
			for (ptr2 = &wrk2[1], lft -= len1 - 1, i = 0; ; ptr2++) {
				if (wrk1[0] <= ptr2[0]) {
					if (len1 > 1) {
						for ( ; ; ) {  /* subtract divisor from dividend loop */
							for (j = 0; j < len1 && wrk1[j] == ptr2[j]; j++);
							if (j == len1 || wrk1[j] < ptr2[j]) {  /* divisor is same or smaller */
								carry = 0;
								for (j = len1; j--; ) {
									k = (ptr2[j] - carry) - wrk1[j];
									if (k < 0) {
										k += 10;
										carry = 1;
									}
									else carry = 0;
									ptr2[j] = (UCHAR) k;
								}
							}
							else break;
						}
					}
					else if (wrk1[0] > 1) ptr2[0] %= wrk1[0];
					else {
						ptr2[0] = 0;
						if (++i == lft) break;
						continue;
					}
				}
				if (++i == lft) break;
				ptr2[1] += ptr2[0] * 10;
			}
			for (i = 0; i < lft1; ) ptr2[i++] |= 0x30;
			ptr2--;
			ptr2[0] = (UCHAR)(0x80 | lft1);
			result = ptr2;
		}
	}
	else {
		if (typeflag == 1) {  /* INT : INT */
			if (!op) {
				dbcflags &= ~(DBCFLAG_EQUAL | DBCFLAG_LESS);
				if (x2 == x1) dbcflags |= DBCFLAG_EQUAL;
				else if (x2 < x1) dbcflags |= DBCFLAG_LESS;
				return;
			}
			if (op == 0x01) x3 = x2 + x1;
			else if (op == 0x02) x3 = x2 - x1;
			else if (op == 0x03) x3 = x2 * x1;
			else {
				if (!x1) goto zerodiv;
				if (op == 0x04) x3 = x2 / x1;
				else x3 = x2 % x1;
			}
			if (dest[0] == 0xFC) { /* If dest is an int */
				dest[1] &= ~0x80;  /* unnull */
				memcpy(&dest[2], (UCHAR *) &x3, sizeof(INT32));
				dbcflags &= ~(DBCFLAG_EQUAL | DBCFLAG_LESS);
				if (x3 == 0) dbcflags |= DBCFLAG_EQUAL;
				else if (x3 < 0) dbcflags |= DBCFLAG_LESS;
				return;
			}
		}
		else {  /* float : float */
			if (!op) {
				dbcflags &= ~(DBCFLAG_EQUAL | DBCFLAG_LESS);
				if (y2 == y1) dbcflags |= DBCFLAG_EQUAL;
				else if (y2 < y1) dbcflags |= DBCFLAG_LESS;
				return;
			}
			if (op == 0x01) y3 = y2 + y1;
			else if (op == 0x02) y3 = y2 - y1;
			else if (op == 0x03) y3 = y2 * y1;
			else {
				if (!y1) goto zerodiv;
				else if (op == 0x04) y3 = y2 / y1;
				else y3 = (DOUBLE64)((LONG) y2 % (LONG) y1);
			}
			if (dest[0] >= 0xF4 && dest[0] <= 0xFB) {
				if (dest[0] <= 0xF7) dest[0] += 0x04;  /* unnull */
				memcpy(&dest[2], (UCHAR *) &y3, sizeof(DOUBLE64));
				dbcflags &= ~(DBCFLAG_EQUAL | DBCFLAG_LESS);
				if (y3 == 0) dbcflags |= DBCFLAG_EQUAL;
				else if (y3 < 0) dbcflags |= DBCFLAG_LESS;
				return;
			}
		}

		if (expflg & 0xFFFF00) {  /* match precision calculated for expressions */
			lft = (UCHAR)(expflg >> 16);
			rgt = (UCHAR)(expflg >> 8);
		}
		else {  /* normal math operation or old precision */
			vgetsize(src1, &lft1, &rgt1);
			vgetsize(src2, &lft2, &rgt2);
			if (op == 0x01 || op == 0x02) {  /* add or sub */
				lft = (lft1 > lft2) ? lft1 : lft2;
				lft += 2;
				rgt = (rgt1 > rgt2) ? rgt1 : rgt2;
			}
			else if (op == 0x03) {  /* mult */
				lft = lft1 + lft2;
				rgt = rgt1 + rgt2;
			}
			else if (op == 0x04 || precisionflg >= 2) {  /* div or mod w/ old precision*/
				lft = lft2 + rgt1;
				if (typeflag != 1) {  /* non integer */
					if (precisionflg >= 2 || src2 == dest) rgt = rgt1 + rgt2;  /* old precision or normal divide */
					else {  /* divide giving */
						vgetsize(dest, &i, &rgt);
						rgt++;
					}
				}
			}
			else {  /* mod */
				lft = (lft1 > lft2) ? lft1 : lft2;
				rgt = 0;
			}
		}

		if (typeflag == 1) {
			if (lft > 31) lft = 31;
			wrk1[0] = 0xFC;
			wrk1[1] = (UCHAR) lft;
			memcpy(&wrk1[2], (UCHAR *) &x3, sizeof(INT32));
		}
		else {
			i = (rgt) ? 30 : 31;
			if (lft + rgt > i) {  /* truncate on right */
				j = lft + rgt - i;
				if (rgt1 < rgt2) rgt1 = rgt2;
				if (rgt > rgt1) {
					if (rgt - rgt1 > j) rgt -= j;
					else rgt = rgt1;
				}
				if (lft > i - rgt) lft = i - rgt;  /* truncate on left */
			}
			wrk1[0] = (UCHAR)(0xF8 | (lft >> 3));
			wrk1[1] = (UCHAR)((lft << 5) | rgt);
			memcpy(&wrk1[2], (UCHAR *) &y3, sizeof(DOUBLE64));
		}
		result = wrk1;
	}

mathop3:
	movevar(result, dest);
	return;

zerodiv:
	dbcflags |= DBCFLAG_OVER;
	if (dest[0] < 0xF4) {  /* form */
		j = dest[0] & 0x1F;
		for (i = 1; i <= j && dest[i] != '.'; i++);
		if (i > j) {  /* no decimal point, move 9s into destination */
			for (i = 1; i <= j; i++) dest[i] = '9';
			return;
		}
	}
	itonv(0, dest);
}

void vgetsize(UCHAR *adr, INT *lft, INT *rgt)
{
	INT i1, i2;

	if (adr[0] >= 0xF4 && adr[0] <= 0xFC) {  /* integer or float */
		if (adr[0] == 0xFC) {
			i1 = adr[1] & ~0x80;
			i2 = 0;
		}
		else {
			i1 = ((adr[1] >> 5) | ((adr[0] << 3) & 0x18)) & 0x1F;
			i2 = adr[1] & 0x1F;
		}
	}
	else {  /* form or literal */
		if (adr[0] != 0x80) {  /* non-null form or literal */
			if (adr[0] == 0xE0) {  /* literal */
				i2 = adr[1];
				adr += 2;
			}
			else {  /* form */
				i2 = adr[0] & 0x1F;
				adr++;
			}
			for (i1 = 0; i1 < i2 && adr[i1] != '.'; i1++);
		}
		else {  /* null form */
			i2 = adr[1] & 0x1F;
			if (adr[1] & 0x80) i1 = 0;
			else {
				adr += 2;
				for (i1 = 1; i1 < i2 && adr[i1] != '.'; i1++);
			}
		}
		if (i1 < i2) i2 -= i1 + 1;
		else i2 = 0;
	}
	*lft = i1;
	*rgt = i2;
}


void arraymath(INT op, UCHAR *adr1, UCHAR *adr2, UCHAR *adr3)
{
	INT i1, flags, dim1[3], dim2[3], dim3[3];
	INT size1, size2, size3;
	INT depth1, depth2, depth3;
	UCHAR type1, type2, type3;
	UCHAR work[64], *adr1x, *adr2x, *adr3x, *save;

	dbcflags &= ~DBCFLAG_OVER;
	if (adr1[0] >= 0xA6 && adr1[0] <= 0xA8) {
		type1 = 1;
		depth1 = adr1[0] - 0xA6 + 1;
		dim1[0] = llhh(&adr1[1]);
		if (depth1 > 1) {
			dim1[1] = llhh(&adr1[3]);
			if (depth1 > 2) dim1[2] = llhh(&adr1[5]);
		}
		i1 = (depth1 << 1) + 1;
		size1 = llhh(&adr1[i1]);
		adr1 += i1 + 2;
	}
	else type1 = 0;

	if (adr2[0] >= 0xA6 && adr2[0] <= 0xA8) {
		type2 = 1;
		depth2 = adr2[0] - 0xA6 + 1;
		dim2[0] = llhh(&adr2[1]);
		if (depth2 > 1) {
			dim2[1] = llhh(&adr2[3]);
			if (depth2 > 2) dim2[2] = llhh(&adr2[5]);
		}
		i1 = (depth2 << 1) + 1;
		size2 = llhh(&adr2[i1]);
		adr2 += i1 + 2;
	}
	else type2 = 0;

	if (op && adr3[0] >= 0xA6 && adr3[0] <= 0xA8) {
		type3 = 1;
		depth3 = adr3[0] - 0xA6 + 1;
		dim3[0] = llhh(&adr3[1]);
		if (depth3 > 1) {
			dim3[1] = llhh(&adr3[3]);
			if (depth3 > 2) dim3[2] = llhh(&adr3[5]);
		}
		i1 = (depth3 << 1) + 1;
		size3 = llhh(&adr3[i1]);
		adr3 += i1 + 2;
	}
	else type3 = 0;

	/* check to see if dimension sizes match */
	/* compiler makes sure number of dimensions match */
	if  ((type1 && type2 && memcmp((CHAR *) dim1, (CHAR *) dim2, depth1 * sizeof(INT))) ||
		(type1 && type3 && memcmp((CHAR *) dim1, (CHAR *) dim3, depth1 * sizeof(INT))) ||
		(type2 && type3 && memcmp((CHAR *) dim2, (CHAR *) dim3, depth2 * sizeof(INT)))) {

		dbcflags |= DBCFLAG_OVER;
		return;
	}

	if (!op) flags = DBCFLAG_EQUAL;
	else if (type1 && !type2) { /* destination is non-array */
		scanvar(adr2);
		memcpy(work, adr2, hl + pl);
		if (op == 0x01) {  /* init to 0 for add only */
			if (vartype & TYPE_INT) memcpy(&work[2], x00000000, sizeof(INT32));
			else movevar(x8130, work);
		}
		save = adr3;
		adr2 = adr3 = work;
	}

	while(TRUE) {
		if (*adr1 == 0xB1) {  /* address variable */
			if ((adr1x = findvar(llhh(&adr1[1]), llmmhh(&adr1[3]))) == NULL) dbcerror(552);
		}
		else adr1x = adr1;
		if (*adr2 == 0xB1) {  /* address variable */
			if ((adr2x = findvar(llhh(&adr2[1]), llmmhh(&adr2[3]))) == NULL) dbcerror(552);
		}
		else adr2x = adr2;
		if (op && *adr3 == 0xB1) {  /* address variable */
			if ((adr3x = findvar(llhh(&adr3[1]), llmmhh(&adr3[3]))) == NULL) dbcerror(552);
		}
		else adr3x = adr3;
		mathop(op, adr1x, adr2x, adr3x);
		if (!op) {
			flags &= (dbcflags & DBCFLAG_EQUAL) | DBCFLAG_LESS;
			flags |= dbcflags & DBCFLAG_LESS;
		}
		if (type1) {
			adr1 += size1;
			if (adr1[0] == 0xA5) break;
		}
		if (type2) {
			adr2 += size2;
			if (adr2[0] == 0xA5) break;
		}
		if (type3) {
			adr3 += size3;
			if (adr3[0] == 0xA5) break;
		}
	}
	if (!op) dbcflags = (dbcflags & ~(DBCFLAG_EQUAL | DBCFLAG_LESS)) | flags;
	else if (type1 && !type2) movevar(work, save);
}
