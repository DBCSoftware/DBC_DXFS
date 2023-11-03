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
#include "includes.h"
#include "base.h"
#include "fio.h"

#define KEYSINIT 16
#define ANDSIZE 4096

struct keydef {
	INT start;
	INT len;
	INT xflg;
};

/* local declarations */
static struct atab *a;		/* working pointer to atab */
static UCHAR casemap[256];

/* local routine declarations */
static void aioxhash(INT, INT, INT, UCHAR *);
static void aioxpurge(INT);


/* AIOOPEN */
/* open aim file */
/* return positive integer file number if successful, else return error */
INT aioopen(CHAR *name, INT opts, INT reclen, OFFSET *txtpos, CHAR *txtname, INT *fixflg, CHAR *keyinfo,
	INT dstnctflg, INT matchc)
{
	static UCHAR firstflg = TRUE;
	INT i1, i2, i3, fnum, from, len, nkeys, version, zvalue;
	OFFSET offset;
	CHAR filename[MAX_NAMESIZE + 1], *ptr;
	UCHAR c1, *blk, **blkptr, **aptr;
	struct keydef *keys, **keysptr;

	if (firstflg) {
		aptr = fiogetopt(FIO_OPT_CASEMAP);
		if (aptr == NULL) {
			for (i1 = 0; i1 < 256; i1++) casemap[i1] = (UCHAR) toupper(i1);
		}
		else memcpy(casemap, *aptr, 256);
		firstflg = FALSE;
	}

	/* open the file with aim extension */
	strncpy(filename, name, sizeof(filename) - 1);
	filename[sizeof(filename) - 1] = '\0';
	miofixname(filename, ".aim", FIXNAME_EXT_ADD);
	fnum = fioopen(filename, opts);
	if (fnum < 0) return(fnum);

	/* allocate buffer */
	blkptr = memalloc(1024, 0);
	if (blkptr == NULL) {
		fioclose(fnum);
		return(ERR_NOMEM);
	}
	blk = *blkptr;

	/* create index file */
	if ((opts & AIO_M_MASK) == AIO_M_PRP) {
		blk[0] = 'A';
		memset(&blk[1], 0, 12);
		mscoffto6x(512, &blk[13]);  /* current extension record count */
		memset(&blk[19], ' ', 81);
		msciton(199, &blk[32], 5);  /* number of slots (z value) */
		if ((opts & (AIO_FIX | AIO_UNC)) == (AIO_FIX | AIO_UNC)) i1 = reclen;
		else i1 = 256;
		msciton(i1, &blk[41], 5);  /* record length */
		if (dstnctflg) blk[57] = 'Y';
		else blk[57] = 'N';
		blk[58] = (UCHAR) matchc;
		if ((opts & (AIO_FIX | AIO_UNC)) == (AIO_FIX | AIO_UNC)) blk[59] = 'F';
		else blk[59] = 'V';
		mscoffto6x(0, &blk[60]);  /* cmd line secondary rec count */
		blk[99] = '9';
		blk[100] = DBCEOR;
		if (txtname[0]) ptr = txtname;
		else {
			strncpy(filename, name, sizeof(filename) - 1);
			filename[sizeof(filename) - 1] = 0;
			miofixname(filename, ".txt", FIXNAME_EXT_REPLACE);
			ptr = filename;
		}
		i1 = (INT)strlen(ptr);
		memcpy(&blk[101], ptr, i1);
		i1 += 101;
		blk[i1++] = DBCEOR;

		/* parse the key information */
		for (i2 = 0, i3 = i1; keyinfo[i2]; ) {
			if (keyinfo[i2] == ',' || keyinfo[i2] == ' ') {
				i2++;
				continue;
			}
			from = 0;
			while (isdigit(keyinfo[i2])) {
				from = from * 10 + keyinfo[i2] - '0';
				blk[i1++] = keyinfo[i2++];
			}
			if (keyinfo[i2] == '-') {
				blk[i1++] = '-';
				i2++;
				len = 0;
				while (isdigit(keyinfo[i2])) {
					len = len * 10 + keyinfo[i2] - '0';
					blk[i1++] = keyinfo[i2++];
				}
			}
			else len = from;
			c1 = (UCHAR) toupper(keyinfo[i2]);
			if (c1 == 'X') {
				blk[i1++] = 'X';
				c1 = keyinfo[++i2];
			}
			if ((c1 && c1 != ' ' && c1 != ',') || !from || len < from || len > RIO_MAX_RECSIZE) {
				i1 = i3;
				break;
			}
			blk[i1++] = DBCEOR;
		}
		if (i1 == i3) {
			memfree(blkptr);
			fioclose(fnum);
			return(ERR_BADKL);
		}
		memset(&blk[i1], DBCDEL, 1024 - i1);

		/* write out header and aim file */
		i2 = 1024;
		for (offset = 0; offset < 13760; offset += i2) {
			if (offset == 13312) i2 = 448;
			i1 = fiowrite(fnum, offset, blk, i2);
			if (i1) {
				memfree(blkptr);
				fioclose(fnum);
				return(i1);
			}
			if (!offset) memset(blk, 0, 1024);
 		}
	}

	/* read header */
	i1 = fioread(fnum, 0, blk, 1024);
	if (i1 != 1024) {
		memfree(blkptr);
		fioclose(fnum);
		if (i1 < 0) return(i1);
		return(ERR_BADIX);
	}

	if (blk[99] != ' ') {
		version = blk[99] - '0';
		if (blk[98] != ' ') version += (blk[98] - '0') * 10;
	}
	else version = 0;
	c1 = blk[59];
	if (version > 10) c1 = DBCDEL;
	else if (version >= 7) {
		if (c1 != 'V' && c1 != 'F' && c1 != 'S') c1 = DBCDEL;
	}
	else {
		if (c1 != 'V' && c1 != 'F') c1 = DBCDEL;
		else if (version == 6 && c1 == 'F') c1 = 'S';
	}
	if (blk[0] != 'A' || blk[100] != DBCEOR || (blk[57] != 'Y' && blk[57] != 'N') || c1 == DBCDEL) {
		memfree(blkptr);
		fioclose(fnum);
		return(ERR_BADIX);
	}
	if (c1 == 'F' || c1 == 'S') {
		mscntoi(&blk[41], &i1, 5);
		if ((opts & (AIO_FIX | AIO_UNC)) != (AIO_FIX | AIO_UNC) || reclen != i1) {
			memfree(blkptr);
			fioclose(fnum);
			return(ERR_BADRL);
		}
		*fixflg = TRUE;
	}
	else {  /* processing will not be fixed recs */
		//opts &= ~(AIO_FIX | AIO_UNC); Not used past this point
		*fixflg = FALSE;
	}

	mscntoi(&blk[32], &zvalue, 5);
	i1 = zvalue;
	if (i1 & 0x07) i1 += 0x08;
	i1 >>= 3;

	/* allocate atab structure and buffer */
	i2 = sizeof(struct atab) - (250 - i1) * sizeof(UCHAR);
	aptr = memalloc(i2, MEMFLAGS_ZEROFILL);
	if (aptr == NULL) {
		memfree(blkptr);
		fioclose(fnum);
		return(ERR_NOMEM);
	}
	blk = *blkptr;

	a = (struct atab *) *aptr;
	if (blk[57] == 'Y') a->dstnct = TRUE;
	else a->dstnct = FALSE;
	if (c1 == 'S') a->delslot = 1;
	else a->delslot = 0;
	a->match = blk[58];
	/* first extent number of records */
	if (version >= 9) msc6xtooff(&blk[13], &a->fxnrec);
	else msc9tooff(&blk[19], &a->fxnrec);
	/* second extent number of records */
	if (version >= 9) msc6xtooff(&blk[60], &a->sxnrec);
	else msc9tooff(&blk[69], &a->sxnrec);
	if (!a->sxnrec) {
		a->sxnrec = a->fxnrec >> 2;
		if (a->sxnrec < 256) a->sxnrec = 256;
	}
	a->version = (UCHAR) version;
	if (version >= 6) a->hdrsize = 1024;
	else a->hdrsize = 512;
	a->slots = (SHORT) zvalue;
	a->hptsiz = (SHORT) i1;
	a->status = 0;
	a->precnum = txtpos;

	if (version >= 9) {
		i2 = a->hdrsize;
		for (i1 = 101; i1 < i2 && blk[i1] != DBCEOR; i1++);
		if (i1 == i2) {
			memfree(aptr);
			memfree(blkptr);
			fioclose(fnum);
			return(ERR_BADIX);
		}
		i3 = i1 + 1;
	}
	else {
		for (i1 = 165; i1 > 100 && blk[i1 - 1] == ' '; i1--);
		if (i1 == 100) {
			memfree(aptr);
			memfree(blkptr);
			fioclose(fnum);
			return(ERR_BADIX);
		}
		i3 = 166;
	}
	blk[i1] = 0;
	if (blk[55] == 'T' && fioaslash((CHAR *) &blk[101]) < 0) {
		ptr = fioname(fnum);
		i2 = fioaslash(ptr);
		if (++i2) memcpy(txtname, ptr, i2);
	}
	else i2 = 0;
	strcpy(&txtname[i2], (CHAR *) &blk[101]);

	/* build the key definition table */
	keysptr = (struct keydef **) memalloc(KEYSINIT * sizeof(struct keydef), 0);
	if (keysptr == NULL) {
		memfree(aptr);
		memfree(blkptr);
		fioclose(fnum);
		return(ERR_NOMEM);
	}
	keys = *keysptr;
	blk = *blkptr;
	nkeys = 0;
	for ( ; ; ) {
		while (blk[i3] == ' ' || blk[i3] == DBCEOR) i3++;
		if (blk[i3] == DBCDEL) break;
		if (!isdigit(blk[i3])) {
			while (blk[i3] != DBCEOR) i3++;
			continue;
		}
		for (i1 = 0; isdigit(blk[i3]); ) i1 = i1 * 10 + blk[i3++] - '0';
		if (blk[i3] == '-') for (i2 = 0, i3++; isdigit(blk[i3]); ) i2 = i2 * 10 + blk[i3++] - '0';
		else i2 = i1;
		if (!i1 || i1 > i2 || i2 > RIO_MAX_RECSIZE) {
			memfree((UCHAR **) keysptr);
			memfree(aptr);
			memfree(blkptr);
			fioclose(fnum);
			return(ERR_BADIX);
		}
		if (nkeys && !(nkeys % KEYSINIT)) {
			if (memchange((UCHAR **) keysptr, (nkeys + KEYSINIT) * sizeof(struct keydef), 0)) {
				memfree((UCHAR **) keysptr);
				memfree(aptr);
				memfree(blkptr);
				fioclose(fnum);
				return(ERR_NOMEM);
			}
			keys = *keysptr;
			blk = *blkptr;
		}
		keys[nkeys].start = --i1;
		keys[nkeys].len = i2 - i1;
		keys[nkeys].xflg = FALSE;
		if (toupper(blk[i3]) == 'X') {
			keys[nkeys].xflg = TRUE;
			i3++;
		}
		nkeys++;
		while (blk[i3] != ' ' && blk[i3] != DBCEOR) i3++;
	}
	memfree(blkptr);

	if (!nkeys) i1 = ERR_BADIX;
	else if (memchange((UCHAR **) keysptr, nkeys * sizeof(struct keydef), 0)) i1 = ERR_NOMEM;
	else i1 = fiosetwptr(fnum, aptr);
	if (i1) {
		memfree((UCHAR **) keysptr);
		memfree(aptr);
		fioclose(fnum);
		return(i1);
	}
	a = (struct atab *) *aptr;
	a->keys = (SHORT) nkeys;
	a->defptr = (UCHAR **) keysptr;
	a->type = 'A';

	return(fnum);
}

/* AIOCLOSE */
/* close aim file */
INT aioclose(INT fnum)
{
	UCHAR **aptr;

	/* initialize structure pointers */
	aptr = fiogetwptr(fnum);
	if (aptr == NULL) return(ERR_NOTOP);
	a = (struct atab *) *aptr;
	if (a->type != 'A') return(ERR_NOTOP);

	memfree(a->andptr);
	memfree(a->lupptr);
	memfree(a->defptr);
	memfree(aptr);
	return(fioclose(fnum));
}

/* AIOKILL */
/* close and delete the aim file */
INT aiokill(INT fnum)
{
	UCHAR **aptr;

	/* initialize structure pointers */
	aptr = fiogetwptr(fnum);
	if (aptr == NULL) return(ERR_NOTOP);
	a = (struct atab *) *aptr;
	if (a->type != 'A') return(ERR_NOTOP);

	memfree(a->andptr);
	memfree(a->lupptr);
	memfree(a->defptr);
	memfree(aptr);
	return(fiokill(fnum));
}

/* AIONEW */
/* start new lookup key specifiation */
INT aionew(INT fnum)
{
	UCHAR **aptr;

	/* initialize structure pointers */
	aptr = fiogetwptr(fnum);
	if (aptr == NULL) return(ERR_NOTOP);
	a = (struct atab *) *aptr;
	if (a->type != 'A') return(ERR_NOTOP);

	a->status = 1;
	return(0);
}

/* AIOKEY */
/* specify next part of lookup key */
INT aiokey(INT fnum, UCHAR *key, INT len)
{
	INT i1, i2, keysize, keylen, keynum;
	UCHAR c1, c2, c3, mchr, typ, *hptable, *keyptr, *ptr, **pptr, **aptr;
	struct keydef *keys;

	/* initialize structure pointers */
	aptr = fiogetwptr(fnum);
	if (aptr == NULL) return(ERR_NOTOP);
	a = (struct atab *) *aptr;
	if (a->type != 'A') return(ERR_NOTOP);

	if (!a->status || a->status > 3) {  /* aionew not called first */
		a->status = 0;
		return(ERR_BADKY);
	}

	if (a->status == 1) {  /* start new lookup key table */
		a->lupsiz = 0;
		memset(a->hptable, 0, a->hptsiz);
		a->status = 2;
	}

	/* build the lookup key table */	
	/* each entry in the table of current key fields */
	/* format is:  typ, keynum, <key>, 0 */

	keysize = len - 3;
	if (keysize < 1) {
		a->status = 0;
		return(ERR_BADKY);
	}

	keynum = key[1] - '0';
	if (key[0] != ' ') keynum += (key[0] - '0') * 10;
	typ = (UCHAR)toupper(key[2]);
	if ((key[0] != ' ' && !isdigit(key[0])) || !isdigit(key[1]) || !keynum || keynum > (INT) a->keys
        || (typ != 'L' && typ != 'R' && typ != 'X' && typ != 'F')) {
		a->status = 0;
		return(ERR_BADKY);
	}

	keys = (struct keydef *) *a->defptr;
	keylen = keys[--keynum].len;
	key += 3;  /* point to characters that make up key */
	if (keysize > 0xFFFF - 4) {  /* max keyinfo length */
		if (typ == 'R') key += keysize - (0xFFFF - 4);  /* truncate on left */
		keysize = 0xFFFF - 4;
	}
	if (keysize >= keylen) {  /* truncate and turn into X type key */
		if (typ == 'R') key += keylen - keysize;  /* truncate on left */
		keysize = keylen;  /* new field length */
		typ = 'X';
	}
	if (typ == 'X') i1 = keylen - keysize;
	else i1 = 0;

	/* allocate more memory for look-up table if needed */
	if (a->lupsiz + keysize + i1 + 5 > (INT) a->lupmax) {
		i2 = a->lupmax;
		if (!i2) i2 = 32;
		while (a->lupsiz + keysize + i1 + 5 > i2) i2 <<= 1;
		if (!a->lupmax) pptr = memalloc(i2, 0);
		else {
			pptr = a->lupptr;
			if (memchange(pptr, i2, 0)) pptr = NULL;
		}
		a = (struct atab *) *aptr;
		if (pptr == NULL) {
			a->status = 0;
			return(ERR_NOMEM);
		}
		a->lupptr = pptr;
		a->lupmax = (SHORT) i2;
	}

	/* add entry to look-up table */
	ptr = *a->lupptr + a->lupsiz;
	a->lupsiz += (SHORT)(keysize + i1 + 4);
	*ptr++ = typ;
	*ptr++ = (UCHAR) keynum;
	*ptr++ = (UCHAR)((keysize + i1) >> 8);
	*ptr++ = (UCHAR)(keysize + i1);
	keyptr = ptr;
	if (!(a->dstnct)) {  /* upper/lower case are indistinct */
		for (i2 = 0; i2 < keysize; ) *ptr++ = casemap[(UCHAR) key[i2++]];
	}
	else {
		memcpy(ptr, key, keysize);
		ptr += keysize;
	}
	if (i1) {
		memset(ptr, ' ', i1);
		ptr += i1;
	}
	*ptr = 0;

	/* calculate the slots that this key creates if not excluded */
	if (!keys[keynum].xflg) {
		a->status = 3;
		mchr = a->match;
		hptable = a->hptable;
		/* check for L1 hash */
		if ((typ == 'X' || typ == 'L') && (c1 = keyptr[0]) != ' ' && c1 != mchr) {
			aioxhash(keynum, 31, c1, hptable);
			/* check for L2 hash */
			c2 = keyptr[1];
			if (keylen > 2 && keysize > 1 && (c2 = keyptr[1]) != ' ' && c2 != mchr) aioxhash(keynum, c1, c2, hptable);
		}
		/* check for R1 hash */
		if (((typ == 'X' && keysize == keylen) || typ == 'R') && keylen > 1 && (c1 = keyptr[keysize - 1]) != ' ' && c1 != mchr) {
			a->status = 3;
			aioxhash(keynum, 30, c1, hptable);
			/* check for R2 hash */
			if (keylen > 2 && keysize > 1 && (c2 = keyptr[keysize - 2]) != ' ' && c2 != mchr) aioxhash(keynum, c2, c1, hptable);
		}
		if (keylen > 3) {  /* floating keys must be greater than 3 */
			i1 = 0;
			keysize -= 2;
			while (i1 < keysize) {  /* search for all 3 non-match chars */
				if ((c1 = keyptr[i1++]) != ' ' && c1 != mchr) {
					if ((c2 = keyptr[i1]) != ' ' && c2 != mchr) {
						if ((c3 = keyptr[i1 + 1]) != ' ' && c3 != mchr) {
							aioxhash(c1, c2, c3, hptable);
						}
						else i1 += 2;
					}
					else i1++;
				}
			}
		}
	}
	return(0);
}

/* AIONEXT */
/* return record number of next potential record that matches key */
INT aionext(INT fnum, INT flag)
{
	INT i1, i2, i3, inc, andi, andsize, hashsize, nonzero, size;
	OFFSET offwork = 0, slot0pos = 0, slotsize = 0;
	UCHAR firstflg, buf28[28], *hashbits, *where, *andbuf, *workbuf = NULL, **workptr;
	UCHAR **aptr;

	/* initialize structure pointers */
	aptr = fiogetwptr(fnum);
	if (aptr == NULL) return(ERR_NOTOP);
	a = (struct atab *) *aptr;
	if (a->type != 'A') return(ERR_NOTOP);

	if (a->status < 3) return(3);
	if (a->status == 3) {
		if (flag == AIONEXT_PREV) return(2);
		hashbits = a->hptable;
		hashsize = a->hptsiz;
		for (i1 = 0; i1 < hashsize && !hashbits[i1]; i1++);
		if (i1 == hashsize) return(3);
	}
	if (flag == AIONEXT_NEXT) {
		if (a->status == 5) return(2);
		if (a->status == 6) a->status = 4;
		inc = 1;
	}
	else if (flag == AIONEXT_PREV) {
		if (a->recnum == 0) {
			a->recnum = -1;
			a->status = 6;
		}
		if (a->status == 6) return(2);
		if (a->status == 5) a->status = 4;
		inc = -1;
	}
	else {
		if (flag == AIONEXT_FIRST) {
			flag = AIONEXT_NEXT;
			inc = 1;
		}
		else inc = -1;
		a->status = 3;
	}

	i1 = FALSE;
	if (a->andptr == NULL) {
		workptr = memalloc(ANDSIZE, 0);
		if (workptr == NULL) return(ERR_NOMEM);
		a = (struct atab *) *aptr;
		a->andptr = workptr;
		i1 = TRUE;
	}
	else membufend(a->andptr);

	if (a->status == 3) {  /* first entry, must create new AND buffer */
		a->status = 4;
		a->exhdfp = 0;
		a->exfrec = 0;
		a->exnrec = a->fxnrec;
		a->exslfp = a->hdrsize;
		a->recnum = 0;
		i1 = TRUE;
	}
	else a->recnum += inc;

	if (i1) {  /* AND buffer needs to be recreated */
		a->anfrec = a->recnum & ~0x07;
		if (flag == AIONEXT_PREV) a->anfrec += 8;
		a->anlrec = a->anfrec;
	}

	/* loop thru AND buffer searching for possible records */
	workptr = NULL;
	andbuf = *a->andptr;
	andi = (INT)((a->recnum - a->anfrec) / 8);  /* result can be negative */
	andsize = (INT)((a->anlrec - a->anfrec) >> 3);
	for ( ; ; ) {
		while (andi >= 0 && andi < andsize) {  /* check for any bits on */
			if (andbuf[andi]) {
				i3 = andbuf[andi];
				offwork = a->anfrec + (OFFSET)(andi << 3);
				if (flag == AIONEXT_NEXT && offwork >= a->recnum) i1 = 0;
				else if (flag == AIONEXT_PREV && offwork + 7 <= a->recnum) i1 = 7;
				else i1 = (INT)(a->recnum - offwork);
				while (i1 >= 0 && i1 < 8) {
					if (i3 & (1 << i1)) {
						*a->precnum = a->recnum = offwork + i1;
						goto aionext3;
					}
					i1 += inc;
				}
			}
			andi += inc;
		}

		/* need to get a new AND buffer */
		if (flag != AIONEXT_PREV) {
			if (flag == AIONEXT_LAST || a->anlrec >= a->exfrec + a->exnrec) {  /* skip to new extent */
				/* get secondary extent start */
				i1 = fioread(fnum, a->exhdfp, buf28, 28);
				if (i1 < 0) goto aionext2;
				if (i1 != 28 || buf28[0] != 'A') goto aionext1;
				if (a->version >= 9) msc6xtooff(&buf28[1], &offwork);
				else msc9tooff(&buf28[1], &offwork);
				if (!offwork) {  /* end of extents */
					if (flag != AIONEXT_LAST) {
						a->status = 5;
						goto aionext3;
					}
					flag = AIONEXT_PREV;
					a->recnum = a->exfrec + a->exnrec - 1;
					if ((OFFSET)(ANDSIZE << 3) >= a->exnrec) a->anfrec = a->exfrec;
					else a->anfrec = a->exfrec + a->exnrec - (OFFSET)(ANDSIZE << 3);
				}
				else {
					i1 = fioread(fnum, offwork, buf28, 28);
					if (i1 < 0) goto aionext2;
					if (i1 != 28 || buf28[0] != 'A') goto aionext1;
					a->exhdfp = offwork;
					a->exslfp = offwork + 28;
					a->exfrec += a->exnrec;
					if (a->version >= 9) msc6xtooff(&buf28[13], &a->exnrec);
					else msc9tooff(&buf28[19], &a->exnrec);
					if (flag == AIONEXT_LAST) continue;
					a->anfrec = a->exfrec;
				}
			}
			else a->anfrec = a->anlrec;
			a->anlrec = a->anfrec + (OFFSET)(ANDSIZE << 3);
			if (a->anlrec > a->exfrec + a->exnrec) a->anlrec = a->exfrec + a->exnrec;
		}
		else {
			if (!a->anfrec) {  /* at beginning of file */
				a->status = 6;
				goto aionext3;
			}
			if (a->anfrec <= a->exfrec) { 
				a->exhdfp = 0;
				a->exslfp = a->hdrsize;
				a->exnrec = a->fxnrec;
				a->exfrec = 0;
				while (a->anfrec > a->exfrec + a->exnrec) {
					if (!a->exhdfp) {
						i1 = fioread(fnum, 0, buf28, 28);
						if (i1 < 0) goto aionext2;
						if (i1 != 28 || buf28[0] != 'A') goto aionext1;
						if (a->version >= 9) msc6xtooff(&buf28[1], &a->exhdfp);
						else msc9tooff(&buf28[1], &a->exhdfp);
					}
					else a->exhdfp = offwork;
					if (!a->exhdfp) goto aionext1;  /* end of extents */
					i1 = fioread(fnum, a->exhdfp, buf28, 28);
					if (i1 < 0) goto aionext2;
					if (i1 != 28 || buf28[0] != 'A') goto aionext1;
					a->exslfp = a->exhdfp + 28;
					a->exfrec += a->exnrec;
					if (a->version >= 9) {
						msc6xtooff(&buf28[13], &a->exnrec);
						msc6xtooff(&buf28[1], &offwork);
					}
					else {
						msc9tooff(&buf28[19], &a->exnrec);
						msc9tooff(&buf28[1], &offwork);
					}
				}
				a->anlrec = a->exfrec + a->exnrec;
			}
			else a->anlrec = a->anfrec;
			a->anfrec = a->anlrec - (OFFSET)(ANDSIZE << 3);
			if (a->anfrec < a->exfrec) a->anfrec = a->exfrec;
		}

		if (workptr == NULL) {
			workptr = memalloc(ANDSIZE, 0);
			a = (struct atab *) *aptr;
			if (workptr == NULL) {
				memfree(a->andptr);
				a->andptr = NULL;
				a->status = 0;
				return(ERR_NOMEM);
			}
			andbuf = *a->andptr;
			workbuf = *workptr;
		}

		firstflg = TRUE;
		size = andsize = (INT)((a->anlrec - a->anfrec) >> 3);  /* number of bytes to read out of each slot */
		nonzero = 0;
		slot0pos = a->exslfp + ((a->anfrec - a->exfrec) >> 3);  /* file position of first slot */
		slotsize = a->exnrec >> 3;
		hashbits = a->hptable;
		hashsize = a->hptsiz;
		where = andbuf;
		for (i2 = 0; i2 < hashsize; i2++) {  /* build the AND buffer */
			if (!hashbits[i2]) continue;
			i3 = hashbits[i2];
			offwork = slot0pos + slotsize * (OFFSET)(i2 << 3);
			do {
				if (i3 & 0x01) {  /* found a slot that is on */
					i1 = fioread(fnum, offwork + nonzero, &where[nonzero], size - nonzero);
					if (i1 < size - nonzero) {
						if (i1 < 0) goto aionext2;
						goto aionext1;
					}
					if (firstflg) {
						firstflg = FALSE;
						where = workbuf;
					}
					else for (i1 = nonzero; i1 < size; i1++) andbuf[i1] &= workbuf[i1];
					while (nonzero < size && !andbuf[nonzero]) nonzero++;
					if (nonzero == size) break;
					while (size > nonzero && !andbuf[size - 1]) size--;
				}
				offwork += slotsize;
			} while (i3 >>= 1);
			if (i3) break;
		}
		if (i2 < hashsize) andi = andsize;  /* force bypass of first loop */
		else if (flag == AIONEXT_NEXT) andi = nonzero;
		else andi = size - 1;
	}

aionext1:
	i1 = ERR_BADIX;

aionext2:
	memfree(workptr);
	memfree(a->andptr);
	a->andptr = NULL;
	a->status = 0;
	return(i1);

aionext3:
	memfree(workptr);
	i1 = membuffer(a->andptr, aioxpurge, fnum);
	a = (struct atab *) *aptr;
	if (i1) {
		memfree(a->andptr);
		a->andptr = NULL;
	}
	if (a->status == 4) i1 = 1;
	else if (a->status == 5 || a->status == 6) i1 = 2;
	else i1 = 0;
	return(i1);
}

/* AIOINSERT */
/* insert key information into the .aim file */
INT aioinsert(INT fnum, UCHAR *rec)
{
	INT i1, i2, i3;
	OFFSET offwork, offwork1, offwork2, bytenum, slotsize = 0, slot0pos = 0, recnum, reccnt;
	UCHAR c1, c2, c3, bitnum, dstnctflg, buf28[28], hptable[250];
	UCHAR *workbuf, **workptr, **aptr;
	struct keydef *keys;

	/* initialize structure pointers */
	aptr = fiogetwptr(fnum);
	if (aptr == NULL) return(ERR_NOTOP);
	a = (struct atab *) *aptr;
	if (a->type != 'A') return(ERR_NOTOP);

	/* hash the record */
	dstnctflg = a->dstnct;
	memset(hptable, 0, (INT) a->hptsiz);
	keys = (struct keydef *) *a->defptr;
	for (i1 = 0; i1 < (INT) a->keys; i1++) {
		if (keys[i1].xflg) continue;  /* excluded field */
		i2 = keys[i1].start;
		i3 = keys[i1].len - 1;  /* key length minus one */
		c1 = rec[i2];
		if (!dstnctflg) c1 = casemap[c1];
		if (c1 != ' ') {  /* leftmost character non-blank */
			aioxhash(i1, 31, c1, hptable);
			if (i3 > 1) {
				c2 = rec[i2 + 1];
				if (!dstnctflg) c2 = casemap[c2];
				if (c2 != ' ') aioxhash(i1, c1, c2, hptable);
			}
		}
		if (i3 > 0) {
			c1 = rec[i2 + i3];
			if (!dstnctflg) c1 = casemap[c1];
			if (c1 != ' ') {
				aioxhash(i1, 30, c1, hptable);
				if (i3 > 1) {
					c2 = rec[i2 + i3 - 1];
					if (!dstnctflg) c2 = casemap[c2];
					if (c2 != ' ') aioxhash(i1, c2, c1, hptable);
				}
			}
		}
		if (i3 > 2) {
			i3 += i2 - 1;
			while (i2 < i3) {
				c1 = rec[i2++];
				if (!dstnctflg) c1 = casemap[c1];
				if (c1 == ' ') continue;
				c2 = rec[i2];
				if (!dstnctflg) c2 = casemap[c2];
				if (c2 != ' ') {
					c3 = rec[i2 + 1];
					if (!dstnctflg) c3 = casemap[c3];
					if (c3 != ' ') aioxhash(c1, c2, c3, hptable);
					else i2 += 2;
				}
				else i2++;
			}
		}
	}

	recnum = *a->precnum;
	bytenum = recnum >> 3;  /* recnum is record number starting at 0 */
	bitnum = (UCHAR)(1 << (INT)(recnum & 0x07));
	if (recnum < a->fxnrec) {
		slot0pos = a->hdrsize;
		slotsize = a->fxnrec / 8;
	}
	else {  /* its not in first extent */
		reccnt = offwork = 0;
		do {
			offwork2 = offwork;
			if (fioread(fnum, offwork2, buf28, 28) != 28 || buf28[0] != 'A') {
				i1 = ERR_BADIX;
				goto aioinsert1;
			}
			if (a->version >= 9) msc6xtooff(&buf28[13], &offwork1);
			else msc9tooff(&buf28[19], &offwork1);
			if (recnum < reccnt + offwork1) {
				slot0pos = offwork + 28;
				slotsize = offwork1 / 8;
				break;
			}
			reccnt += offwork1;
			if (a->version >= 9) msc6xtooff(&buf28[1], &offwork);
			else msc9tooff(&buf28[1], &offwork);
		} while (offwork);
		bytenum -= (reccnt / 8);

		/* must extend file, buf28 is last hdr & offwork2 is fpos */
		if (!offwork) {
			/* offwork1 is number of records to extend by */
			offwork1 = a->sxnrec;
			if (offwork1 + reccnt < recnum) offwork1 = recnum - reccnt;
			if (offwork1 & 0x1f) offwork1 = (offwork1 & ~0x1f) + 0x20;  /* round up to 32 */
			if (a->version >= 9) msc6xtooff(&buf28[13], &offwork);
			else msc9tooff(&buf28[19], &offwork);
			offwork = offwork2 + (a->slots + a->delslot) * (offwork >> 3);
			if (!offwork2) offwork += a->hdrsize;
			else offwork += 28;
			if (a->version >= 9) mscoffto6x(offwork, &buf28[1]);
			else mscoffto9(offwork, &buf28[1]);

			/* allocate memory for workbuf */
			workptr = memalloc(1024, 0);
			if (workptr == NULL) {
				i1 = ERR_NOMEM;
				goto aioinsert1;
			}
			a = (struct atab *) *aptr;
			workbuf = *workptr;

			/* write out extent header and extent */
			workbuf[0] = 'A';
			if (a->version >= 9) {
				memset(&workbuf[1], 0, 12);
				mscoffto6x(offwork1, &workbuf[13]);
			}
			else {
				memset(&workbuf[1], ' ', 18);
				workbuf[9] = workbuf[18] = '0';
				mscoffto9(offwork1, &workbuf[19]);
			}
			i1 = fiowrite(fnum, offwork, workbuf, 28);
			if (i1) {
				memfree(workptr);
				goto aioinsert1;
			}
			offwork += 28;
			slot0pos = offwork;
			slotsize = offwork1 >> 3;
			memset(workbuf, 0, 1024);
			for (i2 = 1024, offwork1 = slotsize * (a->slots + a->delslot); offwork1; offwork1 -= i2) {
				if (offwork1 < 1024) i2 = (INT) offwork1;
				i1 = fiowrite(fnum, offwork, workbuf, i2);
				if (i1) {
					memfree(workptr);
					goto aioinsert1;
				}
				offwork += i2;
			}
			memfree(workptr);

			/* reflect changes in previous extent */
			i1 = fiowrite(fnum, offwork2, buf28, 28);
			if (i1) goto aioinsert1;
		}
	}
	/* insert bits into file */
	slot0pos += bytenum;
	for (i2 = 0; i2 < (INT) a->hptsiz; i2++) {
		if (!hptable[i2]) continue;
		i3 = hptable[i2];
		offwork = slot0pos + slotsize * (OFFSET)(i2 << 3);
		do {
			if (i3 & 0x01) {
				i1 = fioread(fnum, offwork, buf28, 1);
				if (i1 < 1) {
					if (!i1) i1 = ERR_BADIX;
					goto aioinsert1;
				}
				if (!(buf28[0] & bitnum)) {
					buf28[0] |= bitnum;
					i1 = fiowrite(fnum, offwork, buf28, 1);
					if (i1) goto aioinsert1;
				}
			}
			offwork += slotsize;
		} while (i3 >>= 1);
	}
	if (a->status > 3 && a->andptr != NULL && recnum >= a->anfrec && recnum < a->anlrec) {
		for (i2 = 0; i2 < (INT) a->hptsiz; i2++) {
			if (!a->hptable[i2]) continue;
			if ((a->hptable[i2] & hptable[i2]) != a->hptable[i2]) break;
		}
		if (i2 == a->hptsiz) {  /* turn on the record in the and buffer */
			i2 = (INT)(recnum - a->anfrec) >> 3;
			c1 = (UCHAR)(1 << ((UINT) recnum & 0x07));
			(*a->andptr)[i2] |= c1;
		}
	}
	i1 = 0;

aioinsert1:
	/* restore the hptable */
	return(i1);
}

/* AIOHASH */
/* hash the three parameters into the hash pattern table */
static void aioxhash(INT h1, INT h2, INT h3, UCHAR *hptable)
{
	INT i1;

	i1 = ((h1 & 0x1f) << 10) | ((h2 & 0x1f) << 5) | (h3 & 0x1f);
	i1 %= a->slots;  /* i1 is slot number */
	hptable[i1 >> 3] |= 1 << (i1 & 0x07);
}

/* AIOCHKREC */
/* check record for matching key criteria */
INT aiochkrec(INT fnum, UCHAR *record, INT recsize)
{
	INT i1, i2, i3, i4, i5, nkey;
	UCHAR mchr, typ, *lupbuf, **aptr;
	struct keydef *keys;

	/* initialize structure pointers */
	aptr = fiogetwptr(fnum);
	if (aptr == NULL) return(ERR_NOTOP);
	a = (struct atab *) *aptr;
	if (a->type != 'A') return(ERR_NOTOP);

	mchr = a->match;
	keys = (struct keydef *) *a->defptr;
	for (lupbuf = *a->lupptr; *lupbuf; lupbuf += i5) {  /* loop through lookup key table */
		typ = *lupbuf++;
		nkey = *lupbuf++;
		i5 = ((INT) *lupbuf++) << 8;
		i5 += *lupbuf++;
		i1 = keys[nkey].start;
		i2 = keys[nkey].len;
		if (typ == 'R') {
			i1 = i1 + i2 - i5;
			typ = 'X';
		}
		else if (typ == 'L') typ = 'X';
		if (typ == 'X') {
			if (!(a->dstnct)) {  /* upper/lower case are indistinct */
				for (i3 = 0; i3 < i5 && (lupbuf[i3] == ((i1 < recsize) ? casemap[record[i1]] : ' ') || lupbuf[i3] == mchr); i1++, i3++);
			}
			else {
				for (i3 = 0; i3 < i5 && (lupbuf[i3] == ((i1 < recsize) ? record[i1] : ' ') || lupbuf[i3] == mchr); i1++, i3++);
			}
			if (i3 != i5) return(1);  /* did not match key */
		}
		else {  /* must be floating */
			i2 += i1 - i5;
			if (!(a->dstnct)) {  /* upper/lower case are indistinct */
				for ( ; i1 <= i2; i1++) {
					if (*lupbuf != ((i1 < recsize) ? casemap[record[i1]] : ' ') && *lupbuf != mchr) continue;
					for (i3 = 1, i4 = i1 + 1; i3 < i5 && (lupbuf[i3] == ((i4 < recsize) ? casemap[record[i4]] : ' ') || lupbuf[i3] == mchr); i4++, i3++);
					if (i3 == i5) break;  /* match found */
				}
			}
			else {
				for ( ; i1 <= i2; i1++) {
					if (*lupbuf != ((i1 < recsize) ? record[i1] : ' ') && *lupbuf != mchr) continue;
					for (i3 = 1, i4 = i1 + 1; i3 < i5 && (lupbuf[i3] == ((i4 < recsize) ? record[i4] : ' ') || lupbuf[i3] == mchr); i4++, i3++);
					if (i3 == i5) break;  /* match found */
				}
			}
			if (i1 > i2) return(1);  /* no floating match */
		}
	}
	return(0);
}

/* AIOGETREC */
/* return record position where new record may be written */
INT aiogetrec(INT fnum)
{
	INT i1, i2, i3, i4;
	OFFSET offwork, offwork1, offwork2, offwork3, offwork4;
	OFFSET slot0pos, slotsize, recnum, reccnt;
	UCHAR c, hdr28[28], buf28[28], *workbuf, **workptr, **aptr;

	/* initialize structure pointers */
	aptr = fiogetwptr(fnum);
	if (aptr == NULL) return(ERR_NOTOP);
	a = (struct atab *) *aptr;
	if (a->type != 'A') return(ERR_NOTOP);

	if (!a->delslot) return(1);

	/* read header */
	i1 = fioread(fnum, 0, hdr28, 28);
	if (i1 < 0) return(i1);
	if (i1 != 28 || hdr28[0] != 'A') return(ERR_BADIX);

	/* recnum is lowest deleted record number starting at 0 */
	if (a->version >= 9) {
		msc6xtooff(&hdr28[7], &recnum);
		if (!recnum--) return(1);
	}
	else {
		msc9tooff(&hdr28[10], &recnum);
		if (recnum == 999999999) return(1);
	}

	/* allocate memory for work buffer */
	workptr = memalloc(1024, 0);
	if (workptr == NULL) return(ERR_NOMEM);
	a = (struct atab *) *aptr;
	workbuf = *workptr;

	/* find deleted bit turned on */
	/* offwork1 = number of records in current extent */
	/* offwork2 = file position of start of next extent */
	reccnt = 0;
	slot0pos = 1024;
	offwork1 = a->fxnrec;
	slotsize = offwork1 >> 3;
	if (a->version >= 9) msc6xtooff(&hdr28[1], &offwork2);
	else msc9tooff(&hdr28[1], &offwork2);
	for ( ; ; ) {
		if (recnum < reccnt + offwork1) {
			/* search for deleted record bits */
			/* offwork4 = offset within current extent to start */
			if (recnum > reccnt) offwork4 = (recnum - reccnt) >> 3;
			else offwork4 = 0;
			/* offwork = file position to start reading from */
			offwork = slot0pos + a->slots * slotsize + offwork4;
			for (offwork3 = slotsize - offwork4; offwork3; offwork3 -= i2) {
				/* read deleted record slot */
				if (offwork3 < 1024) i2 = (INT) offwork3;
				else i2 = 1024;
				i1 = fioread(fnum, offwork, workbuf, i2);
				if (i1 < 0) goto aioget2;
				if (i1 != i2) goto aioget1;

				/* look for byte with bit(s) turned on */
				for (i3 = 0; i3 < i2 && !workbuf[i3]; i3++);

				/* found deleted bit on */
				if (i3 != i2) {
					c = workbuf[i3];
					for (i2 = 1, i4 = 0; !(c & (UCHAR) i2); i2 <<= 1, i4++);
					c &= ~i2;
					i1 = fiowrite(fnum, offwork + i3, &c, 1);
					if (i1) goto aioget2;

					/* calculate record number */
					offwork = reccnt + ((slotsize - offwork3 + i3) << 3) + i4;
					if ((offwork >> 3) != (recnum >> 3)) {
						if (a->version >= 9) mscoffto6x(offwork + 1, &hdr28[7]);
						else mscoffto9(offwork, &hdr28[10]);
						i1 = fiowrite(fnum, 0, hdr28, 28);
						if (i1) goto aioget2;
					}
					*a->precnum = offwork;
					i1 = 0;
					goto aioget2;
				}
				offwork += i2;
			}
		}

		/* test for no more extents */
		if (!offwork2) {
			if (recnum >= reccnt + offwork1) goto aioget1;
			if (a->version >= 9) mscoffto6x(0, &hdr28[7]);
			else mscoffto9(999999999, &hdr28[10]);
			i1 = fiowrite(fnum, 0, hdr28, 28);
			if (i1) goto aioget2;
			i1 = 1;
			goto aioget2;
		}

		/* read next extent */
		i1 = fioread(fnum, offwork2, buf28, 28);
		if (i1 < 0) goto aioget2;
		if (i1 != 28 || buf28[0] != 'A') goto aioget1;
		reccnt += offwork1;
		slot0pos = offwork2 + 28;
		if (a->version >= 9) msc6xtooff(&buf28[13], &offwork1);
		else msc9tooff(&buf28[19], &offwork1);
		slotsize = offwork1 >> 8;
		if (a->version >= 9) msc6xtooff(&buf28[1], &offwork2);
		else msc9tooff(&buf28[1], &offwork2);
	}

aioget1:
	i1 = ERR_BADIX;

aioget2:
	memfree(workptr);
	return(i1);
}

/* AIODELREC */
/* add record to deleted record list, but do not add extents to do it */
INT aiodelrec(INT fnum)
{
	INT i1;
	OFFSET offwork, offwork1, offwork2;
	OFFSET slot0pos, slotsize, recnum, reccnt;
	UCHAR hdr28[28], buf28[28], **aptr;

	/* initialize structure pointers */
	aptr = fiogetwptr(fnum);
	if (aptr == NULL) return(ERR_NOTOP);
	a = (struct atab *) *aptr;
	if (a->type != 'A') return(ERR_NOTOP);

	if (!a->delslot) return(0);

	/* read header */
	i1 = fioread(fnum, 0, hdr28, 28);
	if (i1 < 0) return(i1);
	if (i1 != 28 || hdr28[0] != 'A') return(ERR_BADIX);

	recnum = *a->precnum;
	reccnt = 0;
	slot0pos = 1024;
	offwork1 = a->fxnrec;
	slotsize = offwork1 >> 3;
	if (a->version >= 9) msc6xtooff(&hdr28[1], &offwork2);
	else msc9tooff(&hdr28[1], &offwork2);
	for ( ; ; ) {
		if (recnum < reccnt + offwork1) {
			/* write deleted record bit */
			offwork = slot0pos + a->slots * slotsize + ((recnum - reccnt) >> 3);
			i1 = fioread(fnum, offwork, buf28, 1);
			if (i1 < 1) {
				if (i1 < 0) return(i1);
				return(ERR_BADIX);
			}
			buf28[0] |= (UCHAR)(1 << ((INT) recnum & 0x07));
			i1 = fiowrite(fnum, offwork, buf28, 1);
			if (i1) return(i1);

			/* if new deleted record is lower, rewrite header */
			if (a->version >= 9) {
				msc6xtooff(&hdr28[7], &offwork);
				if (!offwork--) offwork = recnum + 8;
			}
			else msc9tooff(&hdr28[10], &offwork);
			if ((recnum >> 3) < (offwork >> 3)) {
				if (a->version >= 9) mscoffto6x(recnum + 1, &hdr28[7]);
				else mscoffto9(recnum, &hdr28[10]);
				i1 = fiowrite(fnum, 0, hdr28, 28);
				if (i1) return(i1);
			}
			return(0);
		}

		/* test for no more extents */
		if (!offwork2) return(1);

		/* read next extent */
		i1 = fioread(fnum, offwork2, buf28, 28);
		if (i1 < 0) return(i1);
		if (i1 != 28 || buf28[0] != 'A') return(ERR_BADIX);
		reccnt += offwork1;
		slot0pos = offwork2 + 28;
		if (a->version >= 9) msc6xtooff(&buf28[13], &offwork1);
		else msc9tooff(&buf28[19], &offwork1);
		slotsize = offwork1 >> 8;
		if (a->version >= 9) msc6xtooff(&buf28[1], &offwork2);
		else msc9tooff(&buf28[1], &offwork2);
	}
}

/* AIOMATCH */
/* set a new match character */
void aiomatch(INT fnum, INT chr)
{
	UCHAR **aptr;

	/* initialize structure pointers */
	aptr = fiogetwptr(fnum);
	if (aptr == NULL) return;
	a = (struct atab *) *aptr;
	if (a->type != 'A') return;

	a->match = (UCHAR) chr;
}

/* AIOXPURGE */
/* callback routine for mem routines */
static void aioxpurge(INT fnum)
{
	UCHAR **aptr;

	/* initialize structure pointers */
	aptr = fiogetwptr(fnum);
	if (aptr == NULL) return;
	a = (struct atab *) *aptr;
	if (a->type != 'A') return;

	a->andptr = NULL;
}
