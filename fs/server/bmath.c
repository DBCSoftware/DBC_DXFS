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

#include <string.h>
#include <ctype.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#include "bmath.h"

static void baddx(char *src, char *dest);		/* add routine used by badd and bsub */

int bisnum(char *src)  /* return true if valid number */
{
	int i;
	for (i = 0; src[i] == ' '; i++);
	if (src[i] == '-' || src[i] == '+') i++;
	if (!src[i]) return(FALSE);
	while (isdigit(src[i])) i++;
	if (!src[i]) return(TRUE);
	if (src[i] == '.' && src[i + 1]) i++;
	else return(FALSE);
	while (isdigit(src[i])) i++;
	if (src[i]) return(FALSE);
	return(TRUE);
}

void bnorm(char *src)  /* pads src with digits */
{
	int i, j;
	for (i = 0; src[i] == ' ' || src[i] == '-' || src[i] == '+'; i++)
		src[i] = '0';
	for (j = i; src[j] != '.' && src[j]; j++);
	if (src[j] == '.') {
		while (i != j) {
			src[j] = src[j - 1];
			j--;
		}
		src[j] = '0';
	}
}

void bsetzero(char *dest)  /* zero dest */
{
	int i;
	if (!bisnum(dest)) {
		for (i = 0; dest[i]; i++) dest[i] = ' ';
		dest[--i] = '0';
	}
	else {
		for (i = 0; dest[i] && dest[i] != '.'; i++) dest[i] = ' ';
		if (!dest[i]) dest[--i] = '0';
		else while (dest[++i]) dest[i] = '0';
	}
}

void bfmt(char *src, int rgt, int neg)  /* converts digital string back to numeric */
{
	int i, j;
	for (j = 0; src[j] == '0' && src[j + rgt]; j++) src[j] = ' ';
	if (!rgt && !src[j] && j) src[--j] = '0';
	if (neg && j) src[--j] = '-';
	if (rgt) {
		if (j) j--;
		i = (int)strlen(src) - rgt - 1;
		while (i > j++) src[j - 1] = src[j];
		src[i] = '.';
	}
}

int biszero(char *src)  /* returns true if number is equal to zero */
{
	int i;
	char work[132];
	for (i = (int)strlen(src); i >= 0; i--) work[i] = src[i];
	bnorm(work);
	for (i = 0; work[i] == '0'; i++);
	if (!work[i]) return(1);
	return(0);
}

void binfo(char *src, int *lft, int *rgt, int *neg)  /* gives size of left and right, and if negative */
{
	int i;
	*neg = FALSE;
	for (i = 0; src[i] && src[i] != '.'; i++) {
		if (src[i] == '-') *neg = TRUE;
		else if (src[i] == '+') src[i] = ' ';
	}
	*lft = i;
	if (!src[i++]) *rgt = 0;
	else *rgt = (int)strlen(src) - i;
}

int bmove(char *src, char *dest)  /* move number from src to dest */
{
	int i, j, good, rnd, lft, lfts, lftd, rgt, rgts, rgtd, neg;
	if (!*src) {
		bsetzero(dest);
		return(TRUE);
	}
	if (!bisnum(src) || !bisnum(dest)) return(FALSE);
	good = TRUE;
	binfo(dest, &lftd, &rgtd, &neg);
	lft = lftd;
	rgt = rgtd;
	binfo(src, &lfts, &rgts, &neg);
	for (j = 0; lfts < lftd; j++, lftd--) dest[j] = ' ';
	for (i = 0; lftd < lfts; i++, lfts--)
		if (src[i] != ' ' && src[i] != '0') good = FALSE;
	while (lfts--) dest[j++] = src[i++];
	if (rgts) i++;
	if (rgtd) j++;
	while (rgts && rgtd) {
		dest[j++] = src[i++];
		rgts--;
		rgtd--;
	}
	if (src[i] >= '5') {
		rnd = 1;
		if (src[i] == '5') {
			if (rnd && neg && !src[i + 1]) rnd = 0;
			while (rnd && neg && src[i + rnd] == '0')
				if (!src[i + 1 + rnd++]) rnd = 0;
		}
		if (rnd) {
			j--;
			while (dest[j] == '9' && j) dest[j--] = '0';
			if (dest[j] == ' ') dest[j] = '1';
			else if (dest[j] == '9') {
				dest[j] = '0';
				good = FALSE;
			}
			else if (dest[j] == '-') {
				dest[j] = '1';
				if (j) dest[--j] = '-';
				else good = FALSE;
			}
			else if (dest[j] != '.') dest[j]++;
			else {
				while (j && dest[j - 1] == '9')
					dest[--j] = '0';
				if (!j) good = FALSE;
				else if (dest[--j] == ' ') dest[j] = '1';
				else if (dest[j] == '-') {
					dest[j] = '1';
					if (!j) good = FALSE;
					else dest[--j] = '-';
				}
				else dest[j]++;
			}
		}
	}
	while (rgtd--) dest[j++] = '0';

	/* following is to clean up dest for unusual cases */

	if (biszero(dest) || !good) neg = FALSE;
	bnorm(dest);
	bfmt(dest, rgt, neg);
	return(good);
}

void bform(char *dest, int lft, int rgt)  /* forms lft.rgt */
{
	int i;
	for (i = 0; lft-- > 0; i++) dest[i] = ' ';
	if (rgt > 0) dest[i++] = '.';
	else dest[i - 1] = '0';
	while (rgt-- > 0) dest[i++] = '0';
	dest[i] = 0;
}

#if 0
binit(dest, n)  /* initialize dest with blanks */
int n;
char *dest;
{
	dest[n] = 0;
	while (--n >= 0) dest[n] = ' ';
}
#endif

static void baddx(char *src, char *dest)  /* add routine used by badd and bsub */
{
	int carry, i, j;
	j = (int)strlen(src);
	carry = 0;
	while (j--) {
		if ((i = (src[j] + dest[j] - 2 * '0' + carry)) < 10) {
			dest[j] = i + '0';
			carry = 0;
		}
		else {
			dest[j] = i - 10 + '0';
			carry = 1;
		}
	}
}

void bsubx(char *src, char *dest)  /* subtract routine used by badd and bsub */
{
	int borrow, i, j;
	j = (int)strlen(src);
	borrow = 0;
	while (j--) {
		i = dest[j];
		if ((i = (i - src[j] - borrow)) >= 0) {
			dest[j] = i + '0';
			borrow = 0;
		}
		else {
			dest[j] = i + 10 + '0';
			borrow = 1;
		}
	}
}

int bcomp(char *src, char *dest)  /* subtract src from dest for comparison only */
{
	int lfts, lftd, rgts, rgtd, negs, negd, zeros, zerod;
	char work1[132], work2[132];

	if (!bisnum(src) || !bisnum(dest)) return(0);
	zeros = biszero(src);
	zerod = biszero(dest);
	if (zeros && zerod) return(0);
	binfo(src, &lfts, &rgts, &negs);
	binfo(dest, &lftd, &rgtd, &negd);
	if (zeros != zerod) {
		if ((zeros && !negd) || (!zeros && negs)) return(1);
		return(-1);
	}
	if (negs != negd) return(1 - 2 * negd);

	/* both are non-zero and have the same sign */
	if (lfts < lftd) lfts = lftd;
	if (rgts < rgtd) rgts = rgtd;
	bform(work1, lfts, rgts);
	bform(work2, lfts, rgts);
	bmove(src, work1);
	bmove(dest, work2);
	bnorm(work1);
	bnorm(work2);
	return(strcmp(work2, work1) * (1 - 2 * negd));
}

int badd(char *src, char *dest)  /* adds src to dest */
{
	int cmp, lfts, lftd, rgts, rgtd, negs, negd;
	char work1[132], work2[132];
	if (!bisnum(src) || !bisnum(dest)) return(FALSE);
	binfo(src, &lfts, &rgts, &negs);
	binfo(dest, &lftd, &rgtd, &negd);
	if (lfts++ < lftd) lfts = lftd + 1;
	if (rgts < rgtd) rgts = rgtd;
	bform(work1, lfts, rgts);
	bform(work2, lfts, rgts);
	bmove(src, work1);
	bmove(dest, work2);
	bnorm(work1);
	bnorm(work2);
	if (negs == negd) {
		baddx(work1, work2);
		bfmt(work2, rgts, negs);
		bmove(work2, dest);
		if (biszero(dest)) return(0);
		binfo(dest, &lftd, &rgtd, &negd);
		return(1 - 2 * negd);
	}
	if ((cmp = strcmp(work1, work2)) < 0) {
		bsubx(work1, work2);
		bfmt(work2, rgts, negd);
		bmove(work2, dest);
		if (biszero(dest)) return(0);
		binfo(dest, &lftd, &rgtd, &negd);
		return(1 - 2 * negd);
	}
	if (cmp) {
		bsubx(work2, work1);
		bfmt(work1, rgts, negs);
		bmove(work1, dest);
		if (biszero(dest)) return(0);
		binfo(dest, &lftd, &rgtd, &negd);
		return(1 - 2 * negd);
	}
	bsetzero(dest);
	return(0);
}

int bsub(char *src, char *dest)  /* subtract src from dest */
{
	int cmp, lfts, lftd, rgts, rgtd, negs, negd;
	char work1[132], work2[132];
	if (!bisnum(src) || !bisnum(dest)) return(FALSE);
	binfo(src, &lfts, &rgts, &negs);
	binfo(dest, &lftd, &rgtd, &negd);
	if (lfts++ < lftd) lfts = lftd + 1;
	if (rgts < rgtd) rgts = rgtd;
	bform(work1, lfts, rgts);
	bform(work2, lfts, rgts);
	bmove(src, work1);
	bmove(dest, work2);
	bnorm(work1);
	bnorm(work2);
	if (negs != negd) {
		baddx(work1, work2);
		bfmt(work2, rgts, negd);
		bmove(work2, dest);
		if (biszero(dest)) return(0);
		binfo(dest, &lftd, &rgtd, &negd);
		return(1 - 2 * negd);
	}
	if ((cmp = strcmp(work1, work2)) < 0) {
		bsubx(work1, work2);
		bfmt(work2, rgts, negs);
		bmove(work2, dest);
		if (biszero(dest)) return(0);
		binfo(dest, &lftd, &rgtd, &negd);
		return(1 - 2 * negd);
	}
	if (cmp) {
		bsubx(work2, work1);
		bfmt(work1, rgts, 1 - negs);
		bmove(work1, dest);
		if (biszero(dest)) return(0);
		binfo(dest, &lftd, &rgtd, &negd);
		return(1 - 2 * negd);
	}
	bsetzero(dest);
	return(0);
}

int bmult(char *src, char *dest)  /* multiply dest by src */
{
	int carry, i, j, k, x, y, lens, lfts, lftd, rgts, rgtd, negs, negd;
	char work1[200];
	if (!bisnum(src) || !bisnum(dest)) return(FALSE);
	binfo(src, &lfts, &rgts, &negs);
	binfo(dest, &lftd, &rgtd, &negd);
	if (negs == negd) negd = FALSE;
	else negd = TRUE;
	lfts += lftd;
	rgts += rgtd;
	bform(work1, lfts, rgts);
	bnorm(work1);
	x = (int)strlen(work1);
	lens = (int)strlen(src) - 1;
	/* for (k = strlen(dest) - 1; k && dest[k] != ' ' && dest[k] != '-'; k--) { */
	for (k = (int)strlen(dest) - 1; k >= 0 && dest[k] != ' ' && dest[k] != '-'; k--) {
		if (dest[k] == '0' || dest[k] == '.') {
			if (dest[k] == '0') x--;
			continue;
		}
		carry = 0;
		y = --x;
		/* bmk -- 04/03/86 j >= 0 used to be j */
		for (j = lens; j >= 0 && src[j] != ' ' && src[j] != '-'; j--) {
			if (src[j] == '.') continue;
			i = (src[j] - '0') * (dest[k] - '0') + work1[y] - '0' + carry;
			carry = i / 10;
			work1[y--] = i % 10 + '0';
		}
		work1[y] += carry;
	}
	bfmt(work1, rgts, negd);
	bmove(work1, dest);
	if (biszero(dest)) return(0);
	binfo(dest, &lftd, &rgtd, &negd);
	return (1 - 2 * negd);
}

int bdiv(char *src, char *dest)  /* divide src into dest */
{
	int i, x, y, z, cmp, sigs, sigd, lft, lftd, rgts, rgtd, rgtw, negs, negd;
	char work1[200], work2[200], work3[200];
	if (!bisnum(src) || !bisnum(dest)) return(FALSE);
	if (biszero(src)) {
		for (i = 0; dest[i]; i++) {
			if (dest[i] == '.') i++;
			dest[i] = '9';
		}
		return(1);
	}
	binfo(src, &lft, &rgts, &negs);
	binfo(dest, &lftd, &rgtd, &negd);
	if (negs == negd) negd = FALSE;
	else negd = TRUE;
	bform(work2, ++lft, rgts);
	rgtw = rgtd + rgts + 1;
	bform(work3, lftd, rgtw);
	lft = lftd + rgts + 1;
	bform(work1, lft + rgtd + 1, 0);
	bmove(src, work2);
	bmove(dest, work3);
	bnorm(work1);
	bnorm(work2);
	bnorm(work3);
	for (x = 0; work2[x] == '0'; x++);
	for (y = 0; work3[y] == '0'; y++);
	sigs = (int)strlen(&work2[x]);
	sigd = (int)strlen(&work3[y]);
	if (sigd < sigs) {
		bsetzero(dest);
		return(0);
	}
	i = lft + sigs + rgtw - sigd - rgts - 1;
	z = y + sigs - 1;
	while (work3[z]) {
		if (work3[y] == '0') {
			y++;
			if (work2[x] == '0') {
				x++;
				sigs--;
			}
			else {
				i++;
				z++;
				continue;
			}
		}
		if ((cmp = strncmp(&work3[y], &work2[x], sigs)) < 0) {
			x--;
			sigs++;
			i++;
			z++;
			continue;
		}
		if (!work3[z + 1] && work1[z] == '4') {
			if (cmp > 0 || !negd) work1[i] = '6';
			break;
		}
		bsubx(&work2[x], &work3[y]);
		work1[i]++;
	}
	bfmt(work1, rgtd + 1, negd);
	bmove(work1, dest);
	if (biszero(dest)) return(0);
	binfo(dest, &lftd, &rgtd, &negd);
	return(1 - 2 * negd);
}
