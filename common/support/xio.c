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

#define XIO_FLUSH 0x0400

#define SCAN_VBLOCK	0x01
#define SCAN_UBLOCK	0x02
#define SCAN_EXACT	0x04

#define MAXBUF 100
#define MAXLEVEL	32
#define MAXKEYS	64
#define MODFLG 0x0001
#define LCKFLG 0x0002

/* local declarations */
static struct xtab *x;			/* working pointer to xtab */
static INT filenum;				/* current file number */
static INT blksize;				/* blksize */
static INT version;				/* version number */
/**
 * The actual key length
 */
static INT size;
/**
 * Number of bytes used to hold the file offset.
 * This is 6 for version 9 and newer
 */
static INT size0;
/*
 * This is always size plus size0
 */
static INT size1;
/*
 * This is always size plus 2 * size0
 */
static INT size2;
static UCHAR eobbyte;			/* end of block byte */
static UCHAR thekey[XIO_MAX_KEYSIZE + 30];  /* lookup key */
static OFFSET path[MAXLEVEL];	/* tree - path thru the index u-blocks */
static INT off[MAXLEVEL];		/* tree - block offset thru the index u-blocks */
static INT pathix;				/* count of tree levels + 1 including x->curblk & up */
static UCHAR keystack[MAXKEYS][XIO_MAX_KEYSIZE + 12];  /* stack of keys that overflowed */
static UCHAR keyrepeat[MAXKEYS];  /* stack of keys that overflowed */
static INT lastkeypos;			/* position of last key */
static OFFSET vblock;			/* xioxins uses this when insert after last key in v block */
static INT keyoff;				/* key offset in block */
static INT dupflg;				/* flag if key matches even if filepos does not */
static INT fixcnt;				/* at least one index has had fix set */
static UCHAR priority[256];		/* collating priority */
static UCHAR collateflg;		/* collating flag */
static UCHAR keytruncflg;		/* key truncation flag */
static INT nbuf[4];				/* block buffer numbers */
static UCHAR *blk0, *blk1, *blk2, *blk3;  /* block buffer pointers */
static UCHAR **blkptr[] = { &blk0, &blk1, &blk2, &blk3 };
static struct bufdef {
	UCHAR next;
	UCHAR prev;
	UCHAR flgs;
	UCHAR size;
	UINT lrucnt;
	INT fnum;
	OFFSET pos;
	UCHAR **bptr;
} buf[MAXBUF];
static INT maxbufs = MAXBUF;
static INT freebuf = 0;
static INT bufhi = 0;
static UCHAR hashbuf[256];
static UINT lrucnt;

/* local routine declarations */
static INT xioxprv(void);
static INT xioxins(void);
static INT xioxinsv(INT, INT);
static INT xioxfind(INT);
static INT xioxnxt(void);
static void xioxdelk(UCHAR *, INT);
static INT xioxeor(UCHAR *);
static INT xioxscan(INT);
static INT xioxgo(INT);
static INT xioxgetb(INT, OFFSET, INT);
static INT xioxnewb(INT, OFFSET *);
static void xioxdelb(INT, OFFSET);
static INT xioxend(INT);
static void xioxpurge(INT);
static INT xioxfree(INT);

/*
 * XIOOPEN
 * open index file
 * return positive integer file number if successful, else return -1
 *
 * The name of the TXT file associated with the isi is returned at txtname
 */
INT xioopen(CHAR *name, INT opts, INT keylen, INT reclen, OFFSET *txtpos, CHAR *txtname)
{
	static UCHAR firstflg = TRUE;
	INT i1, i2, fnum;
	CHAR filename[MAX_NAMESIZE + 1], *ptr;
	UCHAR c1, *blk, **blkptr, **xptr;

	if (firstflg) {
/* NOTE: maxbufs is MAXBUF */
		for (i1 = 0; i1 < maxbufs; i1++) buf[i1].next = (UCHAR)(i1 + 1);
		buf[maxbufs - 1].next = 0xFF;
		memset(hashbuf, 0xFF, sizeof(hashbuf));

		collateflg = FALSE;
		xptr = fiogetopt(FIO_OPT_COLLATEMAP);
		if (xptr != NULL) {
			memcpy(priority, *xptr, 256);
			collateflg = TRUE;
		}

		keytruncflg = FALSE;
		if (fiogetflags() & FIO_FLAG_KEYTRUNCATE) keytruncflg = TRUE;

		fixcnt = 0;
		firstflg = FALSE;
	}

	/* open the file with isi extension */
	strncpy(filename, name, sizeof(filename) - 1);
	filename[sizeof(filename) - 1] = 0;
	miofixname(filename, ".isi", FIXNAME_EXT_ADD);
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
	if ((opts & XIO_M_MASK) >= XIO_M_PRP) {
		if (!keylen || keylen > XIO_MAX_KEYSIZE) {
			memfree(blkptr);
			fioclose(fnum);
			return(ERR_BADKL);
		}
		blk[0] = 'I';
		memset(&blk[1], 0, 24);
		memset(&blk[25], ' ', 75);
		memcpy(&blk[42], "1024", 4);
		blk[54] = 'L';
		if (opts & XIO_DUP) blk[56] = 'D';
		if (keylen > 99) blk[58] = (UCHAR)((keylen - 100) / 10 + 'A');
		else if (keylen > 9) blk[58] = (UCHAR)(keylen / 10 + '0');
		blk[59] = (UCHAR)(keylen % 10 + '0');
		blk[98] = '1';
		blk[99] = '0';
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
		memset(&blk[i1], DBCDEL, 1024 - i1);
		i1 = fiowrite(fnum, 0, blk, 1024);
		if (i1) {
			memfree(blkptr);
			fioclose(fnum);
			return(i1);
		}
	}

	/* read header */
	i1 = fioread(fnum, 0, blk, 1024);
	if (i1 < 512) {
		memfree(blkptr);
		fioclose(fnum);
		if (i1 < 0) return(i1);
		return(ERR_BADIX);
	}

	/* test validity of header block and test for fixed length */
	if (blk[99] != ' ') {
		version = blk[99] - '0';
		if (blk[98] != ' ') version += (blk[98] - '0') * 10;
	}
	else version = 0;
	c1 = blk[57];
	if (version > 10) c1 = DBCDEL;
	else if (version >= 9) {
		if (c1 != ' ' && c1 != 'S') c1 = DBCDEL;
	}
	else if (version >= 7) {
		if (c1 != 'V' && c1 != 'S') c1 = DBCDEL;
	}
	else if (version == 6) {
		if (c1 != 'V' && c1 != 'F') c1 = DBCDEL;
	}
	else if (c1 != 'D') c1 = DBCDEL;
	if (blk[0] != 'I' || blk[100] != DBCEOR || c1 == DBCDEL) {
		memfree(blkptr);
		fioclose(fnum);
		return(ERR_BADIX);
	}
	if (c1 == 'F' || c1 == 'S') {
		mscntoi(&blk[64], &i1, 5);
		if ((opts & (XIO_FIX | XIO_UNC)) != (XIO_FIX | XIO_UNC) || reclen != i1) {
			memfree(blkptr);
			fioclose(fnum);
			return(ERR_BADRL);
		}
		fixcnt++;
	}
	else opts &= ~(XIO_FIX | XIO_UNC);
	if (blk[56] == 'D') {
		if (opts & XIO_NOD) {
			memfree(blkptr);
			fioclose(fnum);
			return(ERR_IXDUP);
		}
		opts |= XIO_DUP;
	}
/*** COULD SAY != XIO_M_SHR, IF WANT TO SUPPORT BUFFERING FOR SRO ***/
	if ((opts & XIO_M_MASK) < XIO_M_ERO) opts |= XIO_FLUSH << 16;

	/* finish getting info out of header block and free buffer */
	mscntoi(&blk[41], &blksize, 5);
	if (version >= 9) {
		for (i1 = 101; i1 < blksize && blk[i1] != DBCEOR; i1++);
		if (i1 == blksize) {
			memfree(blkptr);
			fioclose(fnum);
			return(ERR_BADIX);
		}
	}
	else {
		for (i1 = 165; i1 > 100 && blk[i1 - 1] == ' '; i1--);
		if (i1 == 100) {
			memfree(blkptr);
			fioclose(fnum);
			return(ERR_BADIX);
		}
	}
	blk[i1] = 0;
	if (blk[55] == 'T' && fioaslash((CHAR *) &blk[101]) < 0) {
		ptr = fioname(fnum);
		i2 = fioaslash(ptr);
		if (++i2) memcpy(txtname, ptr, i2);
	}
	else i2 = 0;
	strcpy(&txtname[i2], (CHAR *) &blk[101]);

	i1 = blk[59] - '0';
	if (blk[58] != ' ') {
		if (blk[58] >= 'A' && blk[58] <= 'Z') i1 += (blk[58] - 'A') * 10 + 100;
		else i1 += (blk[58] - '0') * 10;
	}
	memfree(blkptr);

	/* test key length if specified */
	if (!keylen) keylen = i1;
	else if (keylen != i1) {
		fioclose(fnum);
		return(ERR_BADKL);
	}

	/* allocate xtab structure */
	i1 = sizeof(struct xtab) - (109 - (keylen + 10)) * sizeof(UCHAR);
	xptr = memalloc(i1, MEMFLAGS_ZEROFILL);
	if (xptr == NULL) {
		fioclose(fnum);
		return(ERR_NOMEM);
	}

	x = (struct xtab *) *xptr;
	x->type = 'X';
	x->version = version;
	x->opts = (UINT)(opts >> 16);
	x->ptxtpos = txtpos;
	x->keylen = (SHORT) keylen;
	x->keyflg = 2;
	x->blksize = blksize;

	i1 = fiosetwptr(fnum, xptr);
	if (i1) {
		memfree(xptr);
		fioclose(fnum);
		return(i1);
	}
	return(fnum);
}

/* XIOCLOSE */
/* close index file */
INT xioclose(INT fnum)
{
	INT i1, i2;
	UCHAR **xptr;
	struct xtab *x;

	i1 = xioxgo(fnum);
	if (!i1) i1 = xioxend(1);

	xptr = fiogetwptr(fnum);
	if (xptr == NULL) return(ERR_NOTOP);
	x = (struct xtab *) *xptr;
	if (x->type != 'X') return(ERR_NOTOP);

	if ((x->opts & (XIO_FIX >> 16)) && fixcnt) fixcnt--;
	memfree(xptr);
	i2 = fioclose(fnum);
	if (!i1) i1 = i2;
	return(i1);
}

/* XIOKILL */
/* close and delete index file */
INT xiokill(INT fnum)
{
	INT i1, i2;
	UCHAR **xptr;
	struct xtab *x;

	i1 = xioxgo(fnum);
	if (!i1) xioxend(-1);

	xptr = fiogetwptr(fnum);
	if (xptr == NULL) return(ERR_NOTOP);
	x = (struct xtab *) *xptr;
	if (x->type != 'X') return(ERR_NOTOP);

	if ((x->opts & (XIO_FIX >> 16)) && fixcnt) fixcnt--;
	memfree(xptr);
	i2 = fiokill(fnum);
	if (!i1) i1 = i2;
	return(i1);
}

/**
 * XIOFIND
 * find key in index file
 */
INT xiofind(INT fnum, UCHAR *key, INT len)
{
	INT i1;

	i1 = xioxgo(fnum);
	if (i1) return(i1);

	if (!len) return(x->keyflg);
	if (len < size) memset(&thekey[len], ' ', size - len);
	else if (len > size) {
		if (!keytruncflg) {
			do if (key[--len] != ' ') return(ERR_BADKY);
			while (len > size);
		}
		else len = size;
	}
	memcpy(thekey, key, len);

	i1 = xioxgetb(0, 0, TRUE);
	if (i1) goto xiofind1;

	i1 = xioxfind(FALSE);
	if (i1) goto xiofind1;

	if (x->keyflg == 1) {
		if (blk1[0] == 'V' && version >= 9 && keyoff != 1) keyoff += size - blk1[keyoff] + 1;
		else keyoff += size;
		memcpy(&thekey[size], &blk1[keyoff], size0);
		if (version >= 9) msc6xtooff(&thekey[size], x->ptxtpos);
		else msc9tooff(&thekey[size], x->ptxtpos);
		memcpy(x->curkey, thekey, size1);
	}
	else {
		x->keyflg = 3;
		if (version >= 9) {
			thekey[size] = 0x7F;
			memset(&thekey[size + 1], 0xFF, 5);
		}
		else memset(&thekey[size], '9', 9);
		memcpy(x->curkey, thekey, size1);
	}
	i1 = xioxend(x->opts & XIO_FLUSH);
	if (i1) goto xiofind1;
	return(x->keyflg);

xiofind1:
	xioxend(-1);
	return(i1);
}

/* XIOFINDLAST */
/* find key in index file */
INT xiofindlast(INT fnum, UCHAR *key, INT len)
{
	INT i1, i2, i3, pos;
	UCHAR c1, c2;

	i1 = xioxgo(fnum);
	if (i1) return(i1);

	if (collateflg) {  /* find highest char value */
		for (i2 = 255, i1 = 0; i1 <= UCHAR_MAX; i1++)
			if (priority[i1] > priority[i2]) i2 = i1;
		c1 = (UCHAR) i2;
	}
	else c1 = 0xFF;
	if (len < size) memset(thekey + len, c1, size - len);
	else if (len > size) {
		if (!keytruncflg) {
			do if (key[--len] != ' ') return(ERR_BADKY);
			while (len > size);
		}
		else len = size;
	}
	memcpy(thekey, key, len);
	pos = len - 1;
	if (collateflg) while (pos >= 0 && priority[thekey[pos]] == priority[c1]) pos--;
	else while (pos >= 0 && thekey[pos] == 0xFF) pos--;
	if (pos >= 0) {
		if (collateflg) {  /* find next highest value and lowest value */
			for (i2 = c1, i3 = priority[thekey[pos]], i1 = 0; i1 <= UCHAR_MAX; i1++)
				if (priority[i1] > i3 && priority[i1] < priority[i2]) i2 = i1;
			thekey[pos] = (UCHAR) i2;
			for (i1 = i2 = 0; i1 <= UCHAR_MAX; i1++)
				if (priority[i1] < priority[i2]) i2 = i1;
			c2 = (UCHAR) i2;
		}
		else {
			thekey[pos]++;
			c2 = 0x00;
		}
		if (++pos < size) memset(thekey + pos, c2, size - pos);
	}

	i1 = xioxgetb(0, 0, TRUE);
	if (i1) goto xiofindlast1;

	i1 = xioxfind(FALSE);
	if (i1) goto xiofindlast1;

	if (x->keyflg != 2) {
		if (x->keyflg != 1 || pos >= 0) {
			i1 = xioxprv();  /* and go back to previous record */
			if (i1) goto xiofindlast1;
		}
		if (x->keyflg == 1) {
/*** CODE: MAYBE MAKE A ROUTINE TO DO THIS WITH BELOW IN XIONEXT AND ELSEWHERE ***/
			if (blk1[0] == 'V' && version >= 9 && keyoff != 1) {
				for (i1 = 1, i2 = 0; ; ) {
					memcpy(&thekey[i2], &blk1[i1], size - i2);
					i1 += size1 - i2;
					if (i1 > keyoff) {
						memcpy(&thekey[size], &blk1[i1 - 6], 6);
						break;
					}
					i2 = blk1[i1++];
				}
			}
			else memcpy(thekey, &blk1[keyoff], size1);
			if (collateflg) for (i1 = len; priority[thekey[i1 - 1]] == priority[key[i1 - 1]]; i1--);
			else i1 = memcmp(thekey, key, len);
			if (!i1) {  /* the keys match */
				memcpy(x->curkey, thekey, size1);
				if (version >= 9) msc6xtooff(&thekey[size], x->ptxtpos);
				else msc9tooff(&thekey[size], x->ptxtpos);
			}
			else x->keyflg = 3;
		}
	}
	if (x->keyflg != 1) {
		x->keyflg = 3;
		memcpy(thekey, key, len);
		if (len < size) memset(&thekey[len], c1, size - len);
		if (version >= 9) {
			thekey[size] = 0x7F;
			memset(&thekey[size + 1], 0xFF, 5);
		}
		else memset(&thekey[size], '9', 9);
		memcpy(x->curkey, thekey, size1);
	}

	i1 = xioxend(x->opts & XIO_FLUSH);
	if (i1) goto xiofindlast1;
	return(x->keyflg);

xiofindlast1:
	xioxend(-1);
	return(i1);
}

/**
 *  XIONEXT
 *
 * get next key in index file
 */
INT xionext(INT fnum)
{
	INT i1, findflg;
	OFFSET offset;

	i1 = xioxgo(fnum);
	if (i1) return(i1);

	if (x->keyflg == 1 || x->keyflg == 3) {  /* on or between keys */
		memcpy(thekey, x->curkey, size1);
		findflg = TRUE;
		if (x->keyflg == 1 && x->curblk) {  /* last read exact hit */
			i1 = xioxgetb(1, x->curblk, TRUE);
			if (i1) goto xionext1;

			if (blk1[0] == 'V') {
				i1 = xioxscan(SCAN_VBLOCK | SCAN_EXACT);
				if (i1) goto xionext1;
				if (x->keyflg == 1) {
					if (version >= 9 && keyoff != 1) keyoff += size1 - blk1[keyoff] + 1;
					else keyoff += size1;
					if (blk1[keyoff] != eobbyte) findflg = FALSE;
				}
			}
			else if (blk1[0] == 'U') {
				i1 = xioxscan(SCAN_UBLOCK | SCAN_EXACT);
				if (i1) goto xionext1;
				if (x->keyflg == 1) {
					if (version >= 9) msc6xtooff(&blk1[keyoff + size1], &x->curblk);
					else msc9tooff(&blk1[keyoff + size1], &x->curblk);
					i1 = xioxgetb(1, x->curblk, TRUE);
					if (i1) goto xionext1;

					while (blk1[0] == 'U') {
						if (version >= 9) msc6xtooff(&blk1[1], &offset);
						else msc9tooff(&blk1[1], &offset);
						i1 = xioxgetb(1, offset, TRUE);
						if (i1) goto xionext1;
					}
					if (blk1[0] != 'V') {
						i1 = ERR_BADIX;
						goto xionext1;
					}
					keyoff = 1;
					findflg = FALSE;
				}
			}
		}

		if (findflg) {
			i1 = xioxgetb(0, 0, TRUE);
			if (i1) goto xionext1;

			/* find that record */
			i1 = xioxfind(TRUE);
			if (i1) goto xionext1;

			if (x->keyflg == 1) {
				/* get next key */
				i1 = xioxnxt();
				if (i1) goto xionext1;
			}
			else if (x->keyflg == 3 || (x->keyflg == 2 && x->curblk)) x->keyflg = 1;
		}
	}
	else if (x->keyflg == 2) {  /* before first key */
		i1 = xioxgetb(0, 0, TRUE);
		if (i1) goto xionext1;

		if (version >= 9) msc6xtooff(&blk0[13], &x->curblk);
		else msc9tooff(&blk0[19], &x->curblk);
		if (!x->curblk) {
			x->keyflg = 4;
			goto xionext0;
		}
		while (TRUE) {
			i1 = xioxgetb(1, x->curblk, TRUE);
			if (i1) goto xionext1;

			if (blk1[0] != 'U') break;
			if (version >= 9) msc6xtooff(&blk1[1], &x->curblk);
			else msc9tooff(&blk1[1], &x->curblk);
		}

		if (blk1[0] != 'V') {
			i1 = ERR_BADIX;
			goto xionext1;
		}
		keyoff = x->keyflg = 1;
	}

	if (x->keyflg == 1) {
		if (blk1[0] == 'V' && version >= 9 && keyoff != 1)
			memcpy(&thekey[blk1[keyoff]], &blk1[keyoff + 1], size1 - blk1[keyoff]);
		else memcpy(thekey, &blk1[keyoff], size1);
		memcpy(x->curkey, thekey, size1);
		if (version >= 9) msc6xtooff(&thekey[size], x->ptxtpos);
		else msc9tooff(&thekey[size], x->ptxtpos);
	}

xionext0:
	i1 = xioxend(x->opts & XIO_FLUSH);
	if (i1) goto xionext1;
	return(x->keyflg);

xionext1:
	xioxend(-1);
	return(i1);
}

/* XIOPREV */
/* get previous key in index file */
INT xioprev(INT fnum)
{
	INT i1, i2;

	i1 = xioxgo(fnum);
	if (i1) return(i1);

	i1 = xioxgetb(0, 0, TRUE);
	if (i1) goto xioprev1;

	if (x->keyflg == 1 || x->keyflg == 3) {  /* on or between keys */
		memcpy(thekey, x->curkey, size1);
		i1 = xioxfind(TRUE);  /* find last exact hit */
		if (i1) goto xioprev1;

		if (x->keyflg != 2) {
			i1 = xioxprv();  /* and go back to previous record */
			if (i1) goto xioprev1;
		}
	}
	else if (x->keyflg == 4) {  /* after last key */
		if (version >= 9) msc6xtooff(&blk0[13], &x->curblk);
		else msc9tooff(&blk0[19], &x->curblk);
		if (!x->curblk) {
			x->keyflg = 2;
			goto xioprev0;
		}
		while (TRUE) {
			i1 = xioxgetb(1, x->curblk, TRUE);
			if (i1) goto xioprev1;

			if (blk1[0] != 'U') break;
			i1 = xioxeor(blk1);
			if (i1 < 0) goto xioprev1;

			if (version >= 9) msc6xtooff(&blk1[i1 - 6], &x->curblk);
			else msc9tooff(&blk1[i1 - 9], &x->curblk);
		}

		if (blk1[0] != 'V') {
			i1 = ERR_BADIX;
			goto xioprev1;
		}

		i1 = xioxeor(blk1);
		if (i1 < 0) goto xioprev1;
		if (version >= 9) {
/*** CODE: MAYBE MAKE A ROUTINE TO DO THIS ***/
			keyoff = size1 + 1;
			i2 = 1;
			while (keyoff < i1) {
				i2 = keyoff;
				keyoff += size1 - blk1[keyoff] + 1;
			}
			keyoff = i2;
		}
		else keyoff = i1 - size1;
		x->keyflg = 1;
	}
	if (x->keyflg == 1) {
/*** CODE: MAYBE MAKE A ROUTINE TO DO THIS WITH ABOVE IN XIONEXT AND ELSEWHERE ***/
		if (blk1[0] == 'V' && version >= 9 && keyoff != 1) {
			for (i1 = 1, i2 = 0; ; ) {
				memcpy(&thekey[i2], &blk1[i1], size - i2);
				i1 += size1 - i2;
				if (i1 > keyoff) {
					memcpy(&thekey[size], &blk1[i1 - 6], 6);
					break;
				}
				i2 = blk1[i1++];
			}
		}
		else memcpy(thekey, &blk1[keyoff], size1);
		memcpy(x->curkey, thekey, size1);
		if (version >= 9) msc6xtooff(&thekey[size], x->ptxtpos);
		else msc9tooff(&thekey[size], x->ptxtpos);
	}

xioprev0:
	i1 = xioxend(x->opts & XIO_FLUSH);
	if (i1) goto xioprev1;
	return(x->keyflg);

xioprev1:
	xioxend(-1);
	return(i1);
}

/* XIOXPRV */
/* get previous record in index */
/* on entry assume current block is in blk1, x->curblk and position is keyoff */
/* on return, set x->curblk, keyoff and x->keyflg appropriately */
static INT xioxprv()
{
	INT i1, i2, lvl;

	if (blk1[0] == 'U') {  /* not at bottom level */
		while (blk1[0] == 'U') {
			if (version >= 9) msc6xtooff(&blk1[keyoff - 6], &x->curblk);
			else msc9tooff(&blk1[keyoff - 9], &x->curblk);
			i1 = xioxgetb(1, x->curblk, TRUE);
			if (i1) return(i1);

			i1 = xioxeor(blk1);
			if (i1 < 0) return(i1);
			keyoff = i1;
		}
	}

	/* bottom level */
	if (blk1[0] != 'V') return(ERR_BADIX);
	if (keyoff != 1) {
		if (version >= 9 && keyoff != size1 + 1) {
			i1 = size1 + 1;
			i2 = 1;
			while (i1 < keyoff) {
				i2 = i1;
				i1 += size1 - blk1[i1] + 1;
			}
			keyoff = i2;
		}
		else keyoff -= size1;
		x->keyflg = 1;
		return(0);
	}

	/* at first entry of current bottom level block, find predescesor */
	lvl = pathix - 1;
	while (lvl >= 0 && off[lvl] == size0 + 1) lvl--;
	if (lvl >= 0) {
		keyoff = off[lvl] - size2;
		x->curblk = path[lvl];
		i1 = xioxgetb(1, x->curblk, TRUE);
		if (i1) return(i1);
		x->keyflg = 1;
		return(0);
	}
	x->keyflg = 2;
	return(0);
}

/**
 * XIOINSERT
 * insert key into index file
 */
INT xioinsert(INT fnum, UCHAR *key, INT len)
{
	INT i1;

	i1 = xioxgo(fnum);
	if (i1) return(i1);

	if (len < size) memset(&thekey[len], ' ', size - len);
	else if (len > size) {
		if (!keytruncflg) {
			do {
				if (key[--len] != ' ') return(ERR_BADKY);
			}
			while (len > size);
		}
		else len = size;
	}
	memcpy(thekey, key, len);
	if (version >= 9) mscoffto6x(*x->ptxtpos, &thekey[size]);
	else mscoffto9(*x->ptxtpos, &thekey[size]);

	i1 = xioxgetb(0, 0, TRUE);
	if (i1) goto xioins1;

	dupflg = FALSE;
	i1 = xioxfind(TRUE);
	if (i1) goto xioins1;

	if (x->keyflg == 1 || (!(x->opts & (XIO_DUP >> 16)) && dupflg)) {  /* duplicate found */
		i1 = xioxend(x->opts & XIO_FLUSH);
		if (i1) goto xioins1;
		if (x->keyflg == 1) {
			return(2); /* Attempt to insert two identical keys for the same record (719) */
		}
		return(1); /* Attempt to write or insert a dup key (709) */
	}
	i1 = xioxins();
	if (i1) goto xioins1;

	x->keyflg = 1;
	memcpy(x->curkey, thekey, size1);

	i1 = xioxend(x->opts & XIO_FLUSH);
	if (i1) goto xioins1;
	return(0);

xioins1:
	xioxend(-1);
	return(i1);
}

/**
 * XIOXINS
 * insert a key into index
 * on entry assume current block is in blk1, x->curblk and position is keyoff
 * on return, set x->curblk, keyoff and x->keyflg appropriately
 * vblock was set by xioxfind()
 */
static INT xioxins()
{
	INT i1, i2, i3, i4, i5, eor1, eorsize, keycnt, lvl;
	OFFSET pos1, pos2, pos3;
	UCHAR save1[XIO_MAX_KEYSIZE + 30], save2[XIO_MAX_KEYSIZE + 30];

	if (!x->curblk) {  /* empty */
		i1 = xioxnewb(1, &x->curblk);
		if (i1) return(i1);

		/* block 0 & 1 already marked for write */
		if (version >= 9) mscoffto6x(x->curblk, &blk0[13]);
		else mscoffto9(x->curblk, &blk0[19]);
		blk1[0] = 'V';
		if (version < 9) blk1[1] = DBCEOR;
		eor1 = keyoff = 1;
	}
	else {
		if (vblock) {  /* current block is not v block */
			x->curblk = vblock;
			i1 = xioxgetb(1, x->curblk, TRUE);
			if (i1) return(i1);
		}

		if (blk1[0] != 'V') return(ERR_BADIX);
		eor1 = xioxeor(blk1);
		if (eor1 < 0) return(eor1);
		if (vblock) keyoff = eor1;
	}

	/* try to insert key into v block */
	keycnt = xioxinsv(keyoff, eor1);
	if (!keycnt) {
		return(0);  /* no overflow */
	}
	if (keycnt < 0) {
		return keycnt;
	}

	/* lowest level block overflowed. keystack contains last key in v block */
	/* and keys that overflowed. attempt to move keystack[2] - */
	/* keystack[keycnt] and key from parent into right brother. if unable to */
	/* move into right brother, then split the original v block moving the */
	/* last key from that v block to the parent and keystack[1] - */
	/* keystack[keycnt] into new the v block */

	/* in the following code, blk1 is left brother, blk2 is parent, blk3 is */
	/* right brother, pos1, pos2, pos3 correspond */
	/* attempt to overflow through parent into right brother */
	pos1 = x->curblk;
	if (pathix) {
		pos2 = path[pathix - 1];  /* parent */
		i1 = xioxgetb(2, pos2, TRUE);
		if (i1) return(i1);

		i1 = off[pathix - 1];
		if (blk2[i1] != eobbyte) {  /* right brother exists */
			if (version >= 9) msc6xtooff(&blk2[i1 + size1], &pos3);  /* pos3 is brother */
			else msc9tooff(&blk2[i1 + size1], &pos3);  /* pos3 is brother */
			i2 = xioxgetb(3, pos3, TRUE);
			if (i2) return(i2);

			i2 = xioxeor(blk3);
			if (i2 < 0) return(i2);
			if (version >= 9) {
				memcpy(keystack[keycnt + 1], &blk2[i1], size1);
				if (collateflg) for (i3 = 0; i3 < size && priority[keystack[keycnt][i3]] == priority[keystack[keycnt + 1][i3]]; i3++);
				else for (i3 = 0; i3 < size && keystack[keycnt][i3] == keystack[keycnt + 1][i3]; i3++);
				if (i3 > 255) i3 = 255;
				keyrepeat[keycnt + 1] = (UCHAR) i3;
				for (i3 = size1, i4 = 2; i4 <= keycnt; ) i3 += size1 - keyrepeat[++i4] + 1;
				if (collateflg) for (i5 = 0; i5 < size && priority[keystack[i4][i5]] == priority[blk3[i5 + 1]]; i5++);
				else for (i5 = 0; i5 < size && keystack[i4][i5] == blk3[i5 + 1]; i5++);
				if (i5 > 255) i5 = 255;
				i3 -= i5 - 1;
				if (i2 + i3 <= blksize) {
					memmove(&blk3[i3 + i5 + 1], &blk3[i5 + 1], i2 - (i5 + 1));
					blk3[i3 + i5] = (UCHAR) i5;
					memcpy(&blk3[1], keystack[2], size1);
					for (i3 = size1 + 1, i4 = 2; i4 <= keycnt; ) {
						i5 = keyrepeat[++i4];
						blk3[i3++] = (UCHAR) i5;
						memcpy(&blk3[i3], &keystack[i4][i5], size1 - i5);
						i3 += size1 - i5;
					}
					memcpy(&blk2[i1], keystack[1], size1);
					buf[nbuf[2]].flgs |= MODFLG;
					buf[nbuf[3]].flgs |= MODFLG;
					return(xioxfind(TRUE));  /* reset position */
				}
			}
			else if (i2 + size1 < blksize) {
				memmove(&blk3[size1 + 1], &blk3[1], i2);
				memcpy(&blk3[1], &blk2[i1], size1);
				memcpy(&blk2[i1], keystack[1], size1);
				buf[nbuf[2]].flgs |= MODFLG;
				buf[nbuf[3]].flgs |= MODFLG;
				return(xioxfind(TRUE));  /* reset position */
			}
		}
	}

	/* overflow to right brother did not work, split the block */
	i1 = xioxnewb(3, &pos3);
	if (i1) return(i1);
	blk3[0] = 'V';
	if (version >= 9) {
		memcpy(&blk3[1], keystack[1], size1);
		for (i3 = size1 + 1, i4 = 1; i4 < keycnt; ) {
			i5 = keyrepeat[++i4];
			blk3[i3++] = (UCHAR) i5;
			memcpy(&blk3[i3], &keystack[i4][i5], size1 - i5);
			i3 += size1 - i5;
		}
	}
	else {
		memcpy(&blk3[1], keystack[1], size1);
		blk3[size1 + 1] = DBCEOR;
	}
	memset(&blk1[lastkeypos], DBCDEL, blksize - lastkeypos);
	if (version < 9) blk1[lastkeypos] = DBCEOR;
	buf[nbuf[1]].flgs |= MODFLG;

	memcpy(save1, keystack[0], size1);
	if (version >= 9) {
		mscoffto6x(pos3, &save1[size1]);
		eorsize = 0;
	}
	else {
		mscoffto9(pos3, &save1[size1]);
		eorsize = 1;
	}
	lvl = pathix;
	while (--lvl >= 0) {
		pos1 = path[lvl];
		i1 = xioxgetb(1, pos1, TRUE);
		if (i1) return(i1);

		keyoff = off[lvl];
		i1 = xioxeor(blk1);
		if (i1 < 0) return(i1);
		if (i1 + eorsize + size2 <= blksize) {  /* no upper block overflow */
			memmove(&blk1[keyoff + size2], &blk1[keyoff], i1 + eorsize - keyoff);
			memcpy(&blk1[keyoff], save1, size2);
			buf[nbuf[1]].flgs |= MODFLG;
			return(xioxfind(TRUE));  /* reset position */
		}

		/* 'U' block must be split */
		if (keyoff == i1) memcpy(&save2[size0], save1, size2);
		else {
			memcpy(&save2[size0], &blk1[i1 - size2], size2);
			memmove(&blk1[keyoff + size2], &blk1[keyoff], i1 - (keyoff + size2));
			memcpy(&blk1[keyoff], save1, size2);
		}

		memcpy(save1, &blk1[i1 - size2], size1);
		memcpy(save2, &blk1[i1 - size0], size0);
		memset(&blk1[i1 - size2], DBCDEL, size2 + eorsize);
		if (eorsize) blk1[i1 - size2] = DBCEOR;
		buf[nbuf[1]].flgs |= MODFLG;

		i1 = xioxnewb(3, &pos3);
		if (i1) return(i1);
		if (version >= 9) mscoffto6x(pos3, &save1[size1]);
		else mscoffto9(pos3, &save1[size1]);
		blk3[0] = 'U';
		memcpy(&blk3[1], save2, size2 + size0);
		if (version < 9) blk3[size2 + size0 + 1] = DBCEOR;
	}

	/* the block that was split causes creation of new top block */
	i1 = xioxnewb(2, &pos2);
	if (i1) return(i1);
	blk2[0] = 'U';
	memcpy(&blk2[size0 + 1], save1, size2);
	if (version >= 9) {
		mscoffto6x(pos1, &blk2[1]);
		mscoffto6x(pos2, &blk0[13]);
	}
	else {
		mscoffto9(pos1, &blk2[1]);
		blk2[size2 + size0 + 1] = DBCEOR;
		mscoffto9(pos2, &blk0[19]);
	}
	return(xioxfind(TRUE));  /* reset position */
}

/* XIODELETE */
/* delete key from index file */
INT xiodelete(INT fnum, UCHAR *key, INT len, INT posflg)
{
	INT i1, i2, i3, i4, i5, eorsize, keypos = 0, lvl;
	OFFSET pos1, pos2, pos3;
	UCHAR work[XIO_MAX_KEYSIZE + 12], *ptr1, *ptr2, *ptr3;

	i1 = xioxgo(fnum);
	if (i1) return(i1);

	if (len) {  /* get this key */
		if (len < size) memset(&thekey[len], ' ', size - len);
		else if (len > size) {
			if (!keytruncflg) {
				do if (key[--len] != ' ') return(ERR_BADKY);
				while (len > size);
			}
			else len = size;
		}
		memcpy(thekey, key, len);
		if (posflg) {
			if (version >= 9) mscoffto6x(*x->ptxtpos, &thekey[size]);
			else mscoffto9(*x->ptxtpos, &thekey[size]);
		}
	}
	else {  /* null key specified */
		if (x->keyflg != 1) return(2);
		memcpy(thekey, x->curkey, size1);  /* get last key */
		posflg = TRUE;
	}

	i1 = xioxgetb(0, 0, TRUE);
	if (i1) goto xiodel1;

	i1 = xioxfind(posflg);
	if (i1) goto xiodel1;
	if (x->keyflg != 1) {
		x->keyflg = 3;
		if (!posflg) {
			if (version >= 9) {
				thekey[size] = 0x7F;
				memset(&thekey[size + 1], 0xFF, 5);
			}
			else memset(&thekey[size], '9', 9);
		}
		memcpy(x->curkey, thekey, size1);
		i1 = xioxend(x->opts & XIO_FLUSH);
		if (i1) goto xiodel1;
		return(1);
	}

	/* save key and set file position */
	x->keyflg = 3;
	if (!posflg) {
		if (blk1[0] == 'V' && version >= 9 && keyoff != 1) i1 = keyoff + size - blk1[keyoff] + 1;
		else i1 = keyoff + size;
		memcpy(&thekey[size], &blk1[i1], size0);
	}
	if (version >= 9) msc6xtooff(&thekey[size], x->ptxtpos);
	else msc9tooff(&thekey[size], x->ptxtpos);
	memcpy(x->curkey, thekey, size1);

	/* blk0 is buffer pointer to block 0 (header block) */
	/* blk1 is buffer pointer and pos1 is its .isi file position */
	/* same for blk2, pos2 and blk3, pos3 */
	/* care must be taken never to erase a buffer pointer */
	/* because xioxend frees them up */
	pos1 = x->curblk;
	if (blk1[0] == 'U') {  /* delete is from an upper level */
		/* swap blocks 1 & 2 */
		i1 = nbuf[1];
		nbuf[1] = nbuf[2];
		nbuf[2] = i1;
		ptr1 = blk1;
		blk1 = blk2;
		blk2 = ptr1;

		/* find successor on bottom level and move it to here */
		off[pathix - 1] += size2;
		if (version >= 9) msc6xtooff(&blk2[keyoff + size1], &pos1);
		else msc9tooff(&blk2[keyoff + size1], &pos1);
		while (TRUE) {
			i1 = xioxgetb(1, pos1, TRUE);
			if (i1) goto xiodel1;
			if (blk1[0] != 'U') break;
			if (pathix == MAXLEVEL) {
				i1 = ERR_PROGX;
				goto xiodel1;
			}
			path[pathix] = pos1;
			off[pathix++] = size0 + 1;
			if (version >= 9) msc6xtooff(&blk1[1], &pos1);
			else msc9tooff(&blk1[1], &pos1);
		}

		if (blk1[0] != 'V') {
			i1 = ERR_BADIX;
			goto xiodel1;
		}
		memcpy(&blk2[keyoff], &blk1[1], size1);
		xioxdelk(blk1, 1);
		buf[nbuf[1]].flgs |= MODFLG;
		buf[nbuf[2]].flgs |= MODFLG;
	}
	else {  /* delete is from bottom level contained in blk1 */
		if (blk1[0] != 'V') {
			i1 = ERR_BADIX;
			goto xiodel1;
		}
		if (!pathix && blk1[size1 + 1] == eobbyte) {
			/* deleting only key in entire index */
			if (version >= 9) mscoffto6x(0, &blk0[13]);
			else mscoffto9(0, &blk0[19]);
			if (!(x->opts & (XIO_FIX >> 16))) {  /* make empty index */
				if (version >= 9) {
					memcpy(&blk0[1], &blk0[13], 6);
					memcpy(&blk0[19], &blk0[13], 6);
				}
				else {
					memcpy(&blk0[1], &blk0[19], 9);
					memcpy(&blk0[28], &blk0[19], 9);
				}
			}
			else xioxdelb(1, pos1);  /* retain deleted record positions */
			buf[nbuf[0]].flgs |= MODFLG;
			x->curblk = 0;
			goto xiodel0;
		}

		xioxdelk(blk1, keyoff);
		buf[nbuf[1]].flgs |= MODFLG;
	}

	/* blk1 is 'V' block that delete took place from */
	/* attempt to collapse it with brother and entry from parent */
	if (version >= 9) eorsize = 0;
	else eorsize = 1;
	lvl = pathix;
	while (--lvl >= 0) {  /* loop up thru tree each time a collapse happened */
		pos2 = path[lvl];
		i1 = xioxgetb(2, pos2, TRUE);  /* block 2 is parent */
		if (i1) goto xiodel1;

		i2 = off[lvl];
		if (blk2[i2] != eobbyte) {  /* right brother exists */
			if (version >= 9) msc6xtooff(&blk2[i2 + size1], &pos3);
			else msc9tooff(&blk2[i2 + size1], &pos3);
			i1 = xioxgetb(3, pos3, TRUE);  /* get right brother */
			if (i1) goto xiodel1;
		}
		else {  /* left brother must exist */
			/* swap blocks 1 & 3 */
			i1 = nbuf[1];
			nbuf[1] = nbuf[3];
			nbuf[3] = i1;
			ptr1 = blk1;
			blk1 = blk3;
			blk3 = ptr1;
			pos3 = pos1;
			i2 -= size2;
			if (version >= 9) msc6xtooff(&blk2[i2 - 6], &pos1);
			else msc9tooff(&blk2[i2 - 9], &pos1);
			i1 = xioxgetb(1, pos1, TRUE);
			if (i1) goto xiodel1;
		}

		i1 = xioxeor(blk3);
		if (i1 < 0) goto xiodel1;
		i3 = i1;
		i1 = xioxeor(blk1);
		if (i1 < 0) goto xiodel1;

		/* blk1 is left child, blk2 is parent, blk3 is right child */
		/* i1 is EOR in blk1, i2 is current entry in blk2, i3 is EOR in blk3 */
		if (version >= 9 && blk1[0] == 'V') {
			if (i1 != 1) {
				/* build last key in blk1 */
				/* set keypos used below */
				for (i4 = keypos = 1, i5 = 0; ; ) {
					memcpy(&work[i5], &blk1[i4], size - i5);
					i4 += size1 - i5;
					if (i4 >= i1) {
						memcpy(&work[size], &blk1[i4 - 6], 6);
						break;
					}
					keypos = i4;
					i5 = blk1[i4++];
				}
				if (collateflg) for (i4 = 0; i4 < size && priority[blk2[i2 + i4]] == priority[work[i4]]; i4++);
				else for (i4 = 0; i4 < size && blk2[i2 + i4] == work[i4]; i4++);
				if (i4 > 255) i4 = 255;
			}
			else i4 = 0;
			if (i3 != 1) {
				if (collateflg) for (i5 = 0; i5 < size && priority[blk3[i5 + 1]] == priority[blk2[i2 + i5]]; i5++);
				else for (i5 = 0; i5 < size && blk3[i5 + 1] == blk2[i2 + i5]; i5++);
				if (i5 > 255) i5 = 255;
			}
			else i5 = 1;
			if (i1 + size1 - i4 + 1 + i3 - i5 > blksize) goto cannotcollapse;
			if (i1 != 1) blk1[i1++] = (UCHAR) i4;
			memcpy(&blk1[i1], &blk2[i2 + i4], size1 - i4);
			i1 += size1 - i4;
			xioxdelk(blk2, i2);
			if (i3 != 1) {
				blk1[i1++] = (UCHAR) i5;
				memcpy(&blk1[i1], &blk3[i5 + 1], i3 - i5 - 1);
			}
		}
		else {
			if (i1 + i3 + size1 + eorsize - 1 > blksize) goto cannotcollapse;

			/* can collapse, always collapse to left child */
			memcpy(&blk1[i1], &blk2[i2], size1);
			xioxdelk(blk2, i2);
			memcpy(&blk1[i1 + size1], &blk3[1], i3 + eorsize - 1);
		}

		/* mark left and parent blocks for writing out */
		buf[nbuf[1]].flgs |= MODFLG;
		buf[nbuf[2]].flgs |= MODFLG;

		/* delete right child */
		xioxdelb(3, pos3);

		/* swap blocks 1 & 2 */
		i1 = nbuf[1];
		nbuf[1] = nbuf[2];
		nbuf[2] = i1;
		ptr1 = blk1;
		blk1 = blk2;
		blk2 = ptr1;
		pos3 = pos1;  /* save for below */
		pos1 = pos2;
	}

	/* at top of index */
	i1 = xioxeor(blk1);
	if (i1 < 0) goto xiodel1;
	if (i1 == size0 + 1) {  /* top block is now empty */
		xioxdelb(1, pos1);
		if (version >= 9) mscoffto6x(pos3, &blk0[13]);
		else mscoffto9(pos3, &blk0[19]);
	}
	goto xiodel0;

cannotcollapse:
	/* blk1 is left child, blk2 is parent, blk3 is right child */
	/* i1 is EOR in blk1, i2 is current entry in blk2, i3 is EOR in blk3 */
	if (blk1[0] == 'V') i5 = 0;
	else i5 = size0;
	if (i1 == i5 + 1) {  /* left child is empty */
		ptr1 = blk3;
		i4 = i5 + 1;
		ptr2 = &blk1[i4];
		ptr3 = &blk3[i4];
	}
	else if (i3 == i5 + 1) {  /* right child is empty */
		ptr1 = blk1;
		ptr2 = &blk3[i5 + 1];
		if (version >= 9 && !i5) {
			i4 = keypos;
			ptr3 = work;
		}
		else {
			i4 = i1 - size1 - i5;
			ptr3 = &blk1[i4];
		}
	}
	else goto xiodel0;  /* neither child is empty, all done */

	/* move key from parent to empty child, move last key of */
	/* left child or first key of right child to parent */
	memcpy(ptr2, &blk2[i2], size1);
	memcpy(&blk2[i2], ptr3, size1);
	if (i5) {  /* for 'U' blocks move lower pointer to brother */
		if (i1 == size0 + 1) {  /* left child was empty */
			i4 -= size0;
			memcpy(&blk1[size2 + 1], &ptr1[i4], size0);
		}
		else {  /* right child was empty */
			memcpy(&blk3[size2 + 1], &blk3[1], size0);
			memcpy(&blk3[1], &blk1[i1 - size0], size0);
		}
	}

	/* clean up old empty block */
	if (version < 9) ptr2[size1 + i5] = DBCEOR;
	xioxdelk(ptr1, i4);

	/* mark all three blocks for writing out */
	buf[nbuf[1]].flgs |= MODFLG;
	buf[nbuf[2]].flgs |= MODFLG;
	buf[nbuf[3]].flgs |= MODFLG;

xiodel0:
	i1 = xioxend(x->opts & XIO_FLUSH);
	if (i1) goto xiodel1;
	return(0);

xiodel1:
	xioxend(-1);
	return(i1);
}

/**
 * XIOGETREC
 * return record position where new record may be written
 */
INT xiogetrec(INT fnum)
{
	INT i1;
	OFFSET pos;

	if (!fixcnt) {
		return(1);
	}
	i1 = xioxgo(fnum);
	if (i1) {
		return(i1);
	}

	if (!(x->opts & (XIO_FIX >> 16))) {
		i1 = xioxend(x->opts & XIO_FLUSH);
		if (i1) goto xiogetrec1;
		return(1);
	}

	i1 = xioxgetb(0, 0, TRUE);
	if (i1) goto xiogetrec1;

	if (version >= 9) msc6xtooff(&blk0[7], &pos);
	else msc9tooff(&blk0[10], &pos);
	if (!pos) {
		i1 = xioxend(x->opts & XIO_FLUSH);
		if (i1) goto xiogetrec1;
		return(1);
	}

	i1 = xioxgetb(1, pos, TRUE);
	if (i1) goto xiogetrec1;
	if (blk1[0] != 'F') {
		i1 = ERR_BADIX;
		goto xiogetrec1;
	}

	if (version >= 9) msc6xtooff(&blk1[7], x->ptxtpos);
	else msc9tooff(&blk1[10], x->ptxtpos);
	xioxdelk(blk1, size0 + 1);
	buf[nbuf[1]].flgs |= MODFLG;

	if (blk1[size0 + 1] == eobbyte) {
		memcpy(&blk0[size0 + 1], &blk1[1], size0);
		xioxdelb(1, pos);
	}

	i1 = xioxend(x->opts & XIO_FLUSH);
	if (i1) goto xiogetrec1;
	return(0);

xiogetrec1:
	xioxend(-1);
	return(i1);
}

/* XIODELREC */
/* add record to deleted record list */
INT xiodelrec(INT fnum)
{
	INT i1, eorsize;
	OFFSET pos;

	if (!fixcnt) return(0);
	i1 = xioxgo(fnum);
	if (i1) return(i1);

	if (!(x->opts & (XIO_FIX >> 16))) {
		i1 = xioxend(x->opts & XIO_FLUSH);
		if (i1) goto xiodelrec1;
		return(0);
	}

	i1 = xioxgetb(0, 0, TRUE);
	if (i1) goto xiodelrec1;

	if (version >= 9) {
		msc6xtooff(&blk0[7], &pos);
		eorsize = 0;
	}
	else {
		msc9tooff(&blk0[10], &pos);
		eorsize = 1;
	}
	if (pos) {
		i1 = xioxgetb(1, pos, TRUE);
		if (blk1[0] != 'F') {
			i1 = ERR_BADIX;
			goto xiodelrec1;
		}
		i1 = xioxeor(blk1);
		if (i1 < 0) goto xiodelrec1;
	}
	else i1 = blksize;  /* force creation of first block */

	if (i1 + size0 + eorsize > blksize) {  /* free block will overflow */
		i1 = xioxnewb(1, &pos);
		if (i1) goto xiodelrec1;

		blk1[0] = 'F';
		memcpy(&blk1[1], &blk0[size0 + 1], size0);
		if (version >= 9) mscoffto6x(pos, &blk0[7]);
		else mscoffto9(pos, &blk0[10]);
		i1 = size0 + 1;
	}
	else buf[nbuf[1]].flgs |= MODFLG;

	if (version >= 9) mscoffto6x(*x->ptxtpos, &blk1[i1]);
	else {
		mscoffto9(*x->ptxtpos, &blk1[i1]);
		blk1[i1 + size0] = DBCEOR;
	}

	i1 = xioxend(x->opts & XIO_FLUSH);
	if (i1) goto xiodelrec1;
	return(0);

xiodelrec1:
	xioxend(-1);
	return(i1);
}

/* XIOFLUSH */
/* flush index buffers */
INT xioflush(INT fnum)
{
	INT i1;

	i1 = xioxgo(fnum);
	if (!i1) i1 = xioxend(1);
	return(i1);
}

/**
 * XIOXFIND
 * get the key block and offset that matches thekey
 * return values:  blk1, keyoff, x->curblk and thekey
 * exact match flag (including text file position)
 */
static INT xioxfind(INT exact)
{
	INT i1, fstflg, lstflg, dupix;  /* first, last and duplicate flags */

	fstflg = lstflg = TRUE;
	dupix = pathix = 0;
	vblock = 0;

	if (version >= 9) msc6xtooff(&blk0[13], &x->curblk);
	else msc9tooff(&blk0[19], &x->curblk);
	if (!x->curblk) {
		x->keyflg = 2;
		return(0);
	}

	/* checking for key in upper branch blocks */
	if (exact) exact = SCAN_EXACT;
	while (TRUE) {
		i1 = xioxgetb(1, x->curblk, TRUE);
		if (i1) {
			return(i1);
		}
		if (blk1[0] != 'U') break;
		i1 = xioxscan(SCAN_UBLOCK | exact);
		if (i1) {
			return(i1);
		}
		if (pathix == MAXLEVEL) {
			return ERR_PROGX;
		}
		path[pathix] = x->curblk;
		off[pathix++] = keyoff;
		if (x->keyflg == 1) {
			/* return if key matched or duplicates not supported */
			if (exact || !(x->opts & (XIO_DUP >> 16))) {
				return(0);
			}
			dupix = pathix;
		}
		if (x->keyflg != 2) fstflg = FALSE;
		if (x->keyflg != 4) lstflg = FALSE;
		if (version >= 9) msc6xtooff(&blk1[keyoff - 6], &x->curblk);
		else msc9tooff(&blk1[keyoff - 9], &x->curblk);
	}

	/* check for key in lower branch block */
	if (blk1[0] != 'V') {
		return(ERR_BADIX);
	}
	i1 = xioxscan(SCAN_VBLOCK | exact);
	if (i1) {
		return(i1);
	}
	if (x->keyflg != 1 && dupix) {  /* found in upper block */
		pathix = dupix;
		x->curblk = path[pathix - 1];
		i1 = xioxgetb(1, x->curblk, TRUE);
		if (i1) {
			return(i1);
		}
		keyoff = off[pathix - 1];
		x->keyflg = 1;
		return(0);
	}
	if (x->keyflg == 1 || x->keyflg == 3) {
		return(0);
	}
	if (x->keyflg == 2) {
		if (!fstflg) x->keyflg = 3;
		return(0);
	}
	if (x->keyflg == 4 && lstflg) {
		return(0);
	}
	vblock = x->curblk;

	/* get successor to the last key of this block */
	i1 = xioxnxt();
	if (i1) {
		return(i1);
	}
	x->keyflg = 3;
	return(0);
}

/*
 * XIOXNXT
 * get next record in index
 * on entry assume current block is in blk1, x->curblk and position is keyoff
 * on return, set x->curblk, keyoff and x->keyflg appropriately
 */
static INT xioxnxt()
{
	INT i1, lvl;

	if (blk1[0] == 'U') {  /* not at bottom level */
		i1 = keyoff + size1;
		while (blk1[0] == 'U') {
			if (version >= 9) msc6xtooff(&blk1[i1], &x->curblk);
			else msc9tooff(&blk1[i1], &x->curblk);
			i1 = xioxgetb(1, x->curblk, TRUE);
			if (i1) return(i1);
			i1 = 1;
		}
		if (blk1[0] != 'V') return(ERR_BADIX);
		keyoff = x->keyflg = 1;
		return(0);
	}

	/* should be a V block */
	if (blk1[0] != 'V') return(ERR_BADIX);
	if (blk1[keyoff] != eobbyte) {
		if (version >= 9 && keyoff != 1) keyoff += size1 - blk1[keyoff] + 1;
		else keyoff += size1;
		if (blk1[keyoff] != eobbyte) {
			x->keyflg = 1;
			return(0);
		}
	}

	/* at last entry of current bottom level block, find successor */
	lvl = pathix;
	while (--lvl >= 0) {
		x->curblk = path[lvl];
		i1 = xioxgetb(1, x->curblk, TRUE);
		if (i1) return(i1);
		keyoff = off[lvl];
		if (blk1[keyoff] != eobbyte) {
			x->keyflg = 1;
			return(0);
		}
	}
	x->keyflg = 4;  /* no successor, after last key */
	return(0);
}

/**
 * XIOXINSV
 * insert key into blk1 at position pos, eor is the end of block position,
 * thekey is the key to insert. if the insertion causes the blk to overflow,
 * move the keys that overflowed into keystack[1] and up. move the last
 * key remaining in the block to keystack[0] and set last key position
 * (this is in case insertion will cause the v block to be split).
 */
static INT xioxinsv(INT pos, INT eor)
{
	INT i1, i2, i3, adjust, keycnt;
	INT lenadd, lenadd2, lensub, lenomit, lenomit2 = 0;
	UCHAR keywork[XIO_MAX_KEYSIZE + 6];

	if (version < 9) {
		if (eor + size1 < blksize) {  /* will fit */
			memmove(&blk1[pos + size1], &blk1[pos], eor + 1 - pos);
			memcpy(&blk1[pos], thekey, size1);
			buf[nbuf[1]].flgs |= MODFLG;
			return(0);
		}
		if (pos == eor) memcpy(keystack[1], thekey, size1);  /* new key overflows */
		else {
			memcpy(keystack[1], &blk1[eor - size1], size1);
			memmove(&blk1[pos + size1], &blk1[pos], eor - (pos + size1));
			memcpy(&blk1[pos], thekey, size1);
			buf[nbuf[1]].flgs |= MODFLG;
		}

		lastkeypos = eor - size1;
		memcpy(keystack[0], &blk1[lastkeypos], size1);
		return(1);
	}

	/* version 9 and higher */
	if (pos == 1) {  /* first key */
		if (eor == 1) {  /* no other keys */
			memcpy(&blk1[1], thekey, size1);
			buf[nbuf[1]].flgs |= MODFLG;
			return(0);
		}
		lenadd = size1;
		lenomit = 0;

		/* try to compress next key */
		if (collateflg) for (i1 = 0; i1 < size && priority[thekey[i1]] == priority[blk1[pos + i1]]; i1++);
		else {
			for (i1 = 0; i1 < size && thekey[i1] == blk1[pos + i1]; i1++);
		}
		if (i1 > 255) i1 = 255;
		lensub = lenomit2 = i1;
		lenadd2 = 1;
	}
	else {
		for (i1 = 1, i2 = 0; ; ) {  /* build previous key */
			memcpy(&keywork[i2], &blk1[i1], size - i2);
			i1 += size1 - i2;
			if (i1 >= pos) {
				memcpy(&keywork[size], &blk1[i1 - 6], 6);
				break;
			}
			lastkeypos = i1;
			i2 = blk1[i1++];
		}

		/* try to compress new key */
		if (collateflg) for (i1 = 0; i1 < size && priority[thekey[i1]] == priority[keywork[i1]]; i1++);
		else for (i1 = 0; i1 < size && thekey[i1] == keywork[i1]; i1++);
		if (i1 > 255) i1 = 255;
		lenadd = size1 - i1 + 1;
		lenomit = i1;

		if (pos != eor) {  /* try to compress next key */
			i1 = blk1[pos];
			i2 = pos + 1;
			if (collateflg) while (i1 < size && priority[thekey[i1]] == priority[blk1[i2]]) { i1++; i2++; }
			else while (i1 < size && thekey[i1] == blk1[i2]) { i1++; i2++; }
			if (i1 > 255) i1 = 255;
			lensub = i1 - blk1[pos];
			lenomit2 = i1;
		}
		else lensub = 0;
		lenadd2 = 0;
	}

	adjust = lenadd + lenadd2 - lensub;
	if (eor + adjust <= blksize) {  /* will fit in block */
		if (pos != eor) {  /* move remaining keys */
			memmove(&blk1[pos + lenadd + lenadd2], &blk1[pos + lensub], eor - (pos + lensub));
			blk1[pos + lenadd] = (UCHAR) lenomit2;
		}

		/* insert new key */
		if (pos != 1) blk1[pos++] = (UCHAR) lenomit;
		memcpy(&blk1[pos], &thekey[lenomit], size1 - lenomit);
		buf[nbuf[1]].flgs |= MODFLG;
		return(0);
	}

	/* overflow */
	if (pos == eor) {  /* trying to append to block */
		/* lastkeypos set above (pos != 1) */
		memcpy(keystack[0], keywork, size1);
		memcpy(keystack[1], thekey, size1);
		return(1);
	}

	/* parse remaining keys for overflow */
	keycnt = 0;
	i1 = lastkeypos = pos;
	if (i1 != 1) {
		memcpy(keywork, thekey, size1);
		i2 = blk1[i1++];
	}
	else {
		i2 = 0;  /* assume key will not overflow first time */
	}
	for ( ; ; ) {
		if (i1 + adjust + size1 - i2 > blksize) {
			if (keycnt == MAXKEYS) {
				return ERR_PROGX;
			}
			memcpy(keystack[keycnt++], keywork, size1);
		}
		else if (lastkeypos == pos) lastkeypos += lenadd;
		else lastkeypos = i1 + adjust - 1;
		memcpy(&keywork[i2], &blk1[i1], size1 - i2);
		i1 += size1 - i2;
		if (i1 >= eor) {
			memcpy(keystack[keycnt], keywork, size1);
			break;
		}
		i2 = blk1[i1++];
	}

	/* move remaining keys */
	memmove(&blk1[pos + lenadd + lenadd2], &blk1[pos + lensub], blksize - (pos + lenadd + lenadd2));
	if (pos != lastkeypos) blk1[pos + lenadd] = (UCHAR) lenomit2;

	/* insert new key */
	if (pos != 1) blk1[pos++] = (UCHAR) lenomit;
	memcpy(&blk1[pos], &thekey[lenomit], size1 - lenomit);

	/* calculate new eor */
	eor = lastkeypos + size1 - blk1[lastkeypos] + 1;
	memset(&blk1[eor], DBCDEL, blksize - eor);

	/* count repeat chars for keystack[2] and up */
	for (i2 = 1; i2 < keycnt; ) {
		i1 = i2++;
		if (collateflg) for (i3 = 0; i3 < size && priority[keystack[i1][i3]] == priority[keystack[i2][i3]]; i3++);
		else for (i3 = 0; i3 < size && keystack[i1][i3] == keystack[i2][i3]; i3++);
		if (i3 > 255) i3 = 255;
		keyrepeat[i2] = (UCHAR) i3;
	}

	buf[nbuf[1]].flgs |= MODFLG;
	return(keycnt);
}

/* XIOXDELK */
/* delete key at position pos */
static void xioxdelk(UCHAR *blk, INT pos)
{
	INT i1, i2, i3;

	if (blk[0] == 'V') {
		if (version >= 9) {
			if (pos == 1) {
				i2 = 0;
				i1 = size1 + 1;
			}
			else {
				i2 = blk[pos];
				i1 = pos + size1 - i2 + 1;
			}
			if (blk[i1] != eobbyte) {
				i3 = blk[i1];
				if (i2 < i3) {
					if (pos == 1) pos += i3;
					else pos += i3 - i2 + 1;
					i1++;
				}
				else if (pos == 1) i1++;
			}
		}
		else i1 = pos + size1;
	}
	else if (blk[0] == 'U') i1 = pos + size2;
	else if (blk[0] == 'F') i1 = pos + size0;
	else return;

	memmove(&blk[pos], &blk[i1], blksize - i1);
	memset(&blk[blksize - (i1 - pos)], DBCDEL, i1 - pos);
}

/* XIOXEOR */
/* return the position of LEOR in the block in buffer */
static INT xioxeor(UCHAR *blk)
{
	INT i1, i2;

	if (blk[0] == 'V') {
		if (version >= 9) {
			if (blk[1] == eobbyte) {
				return(1);
			}
			for (i1 = size1; blk[++i1] != eobbyte; i1 += size1 - blk[i1]);
			if (i1 > blksize) {
				return(ERR_BADIX);
			}
			return(i1);
		}
		i1 = 1;
		i2 = size1;
	}
	else if (blk[0] == 'U') {
		i1 = size0 + 1;
		i2 = size2;
	}
	else if (blk[0] == 'F') {
		i1 = size0 + 1;
		i2 = size0;
	}
	else {
		return(ERR_BADIX);
	}

	while (blk[i1] != eobbyte) i1 += i2;
	if (i1 > blksize) {
		return(ERR_BADIX);
	}
	return(i1);
}

/**
 *
 * XIOXSCAN
 * scan an index block in blk1
 * return x->keyflg == 1 if exact match of key
 * return x->keyflg == 2 if key is before first key in block
 * return x->keyflg == 3 if no match and but not before first or after last key
 * return x->keyflg == 4 if key is after last key in block
 * keyoff points to key if match or next key otherwise
 *
 */
static INT xioxscan(INT typeflg)
{
	INT i1, i2, i3, increment, keylen, start;
	UCHAR *ptr;

	keylen = size;
	if ((typeflg & SCAN_VBLOCK) && version >= 9 && blk1[1] != eobbyte) {
		start = 1;
		if (typeflg & SCAN_EXACT) keylen = size1;
		for (i1 = 1, i2 = i3 = 0; ; ) {
			ptr = &blk1[i1 - i2];
			thekey[keylen] = ~ptr[keylen];
xioxscan1:
			while (thekey[i2] == ptr[i2]) i2++;
			if (i2 >= size || !collateflg) {
				if (i2 >= size) {
					dupflg = TRUE;
					if (i2 == keylen) {  /* found a match */
						if (i1 != 1) i1--;
						if (i1 > blksize) return(ERR_BADIX);
						keyoff = i1;
						x->keyflg = 1;
						return(0);
					}
				}
				if (thekey[i2] < ptr[i2]) {
					if (i1 != 1) i1--;
					break;
				}
				if (i2 > size) i2 = size;
			}
			else if (priority[thekey[i2]] <= priority[ptr[i2]]) {
				if (priority[thekey[i2]] == priority[ptr[i2]]) {
					i2++;
					goto xioxscan1;
				}
				if (i1 != 1) i1--;
				break;
			}
xioxscan2:
			i1 += size1 - i3;
			if (blk1[i1] == eobbyte || (i3 = blk1[i1]) < i2) break;
			i1++;
			if (i3 != i2) goto xioxscan2;
		}
	}
	else {
		start = 1;
		if (typeflg & SCAN_UBLOCK) {
			start += size0;
			increment = size2;
		}
		else increment = size1;
		if (!collateflg) {
			for (i1 = start; blk1[i1] != eobbyte; i1 += increment) {
				if ((i2 = memcmp(thekey, &blk1[i1], keylen)) > 0) continue;
				if (!i2) {
					dupflg = TRUE;
					if (typeflg & SCAN_EXACT) {
						typeflg &= ~SCAN_EXACT;
						keylen = size1;
						if ((i2 = memcmp(&thekey[size], &blk1[i1 + size], size0)) > 0) continue;
					}
				}
				if (i2 < 0) break;
				/* found a match */
				if (i1 > blksize) return(ERR_BADIX);
				keyoff = i1;
				x->keyflg = 1;
				return(0);
			}
		}
		else {
			if (typeflg & SCAN_EXACT) keylen = size1;
			for (i1 = start; blk1[i1] != eobbyte; i1 += increment) {
				ptr = &blk1[i1];
				thekey[keylen] = ~ptr[keylen];
				i2 = 0;
xioxscan3:
				while (thekey[i2] == ptr[i2]) i2++;
				if (i2 >= size) {
					dupflg = TRUE;
					if (i2 == keylen) {  /* found a match */
						if (i1 > blksize) return(ERR_BADIX);
						keyoff = i1;
						x->keyflg = 1;
						return(0);
					}
					if (thekey[i2] < ptr[i2]) break;
				}
				else if (priority[thekey[i2]] <= priority[ptr[i2]]) {
					if (priority[thekey[i2]] == priority[ptr[i2]]) {
						i2++;
						goto xioxscan3;
					}
					break;
				}
			}
		}
	}
	if (i1 > blksize) return(ERR_BADIX);
	keyoff = i1;
	if (blk1[i1] == eobbyte) x->keyflg = 4;
	else if (i1 == start) x->keyflg = 2;
	else x->keyflg = 3;
	return(0);
}

/**
 *  XIOXGO
 *
 *  start an xio function
 *  sets global fields filenum, blksize, size, version, sizeN, blkN, nbufN
 */
static INT xioxgo(INT fnum)
{
	INT i1;
	UCHAR **xptr;

	/* initialize structure pointers */
	xptr = fiogetwptr(fnum);
	if (xptr == NULL) return(ERR_NOTOP);
	x = (struct xtab *) *xptr;
	if (x->type != 'X') return(ERR_NOTOP);

	if (x->errflg) {
		i1 = x->errflg;
		x->errflg = 0;
		return(i1);
	}

	filenum = fnum;
	blksize = x->blksize;
	size = x->keylen;
	version = x->version;
	if (version >= 9) {
		size0 = 6;
		eobbyte = 0xFF;
	}
	else {
		size0 = 9;
		eobbyte = 0xFA;
	}
	size1 = size + size0;
	size2 = size1 + size0;
	blk0 = blk1 = blk2 = blk3 = NULL;
	nbuf[0] = nbuf[1] = nbuf[2] = nbuf[3] = -1;
	return(0);
}

/*
 * XIOXGETB - get a block
 *
 * Return 0 if ok, else ERR_NOMEM, ERR_PROGX, ERR_BADIX
 */
static INT xioxgetb(INT bufnum, OFFSET pos, INT rdflg)
{
	INT i1, i2, bsize, hash;
	UINT u1;
	UCHAR allocflg, **pptr;

#ifdef XIODEBUG
	INT i3, i4;
	for (i1 = freebuf, i3 = 0; i1 != 0xFF; ) {
		if (buf[i1].fnum) {
			printf("\rfreebuf has fnum set\n");
			exit(1);
		}
		i3++;
		i4 = buf[i1].next;
		if (i4 <= i1) {
			printf("\rfreebuf is out of order\n");
			exit(1);
		}
		i1 = i4;
	}
	for (i1 = 0, i2 = 0, i4 = 0; i1 < maxbufs; i1++) {
		if (buf[i1].fnum) {
			i2++;
			if (i1 >= bufhi) {
				printf("\rbuffer is higher than bufhi\n");
				exit(1);
			}
			if (buf[i1].flgs & LCKFLG) i4++;
		}
	}
	if (i4 > 4) {
		printf("\rthere are %d locks\n", i4);
		exit(1);
	}
	if (i2 + i3 != maxbufs) {
		printf("\rthere are %d freebufs and %d bufs\n", i3, i2);
		exit(1);
	}
	for (i1 = 0, i4 = 0; i1 < 256; i1++) {
		if (hashbuf[i1] == 0xFF) continue;
		for (i3 = hashbuf[i1]; i3 != 0xFF; i3 = buf[i3].next) {
			if (!buf[i3].fnum) {
				printf("\rhashtab does not have fnum set\n");
				exit(1);
			}
			i4++;
		}
	}
	if (i2 != i4) {
		printf("\rthere are %d bufs and %d hashtabs\n", i2, i4);
		exit(1);
	}
#endif

	bsize = blksize;

	/* test for buffer already in use */
	allocflg = FALSE;
	if (nbuf[bufnum] != -1) {
		i2 = nbuf[bufnum];
		if (pos == buf[i2].pos) return(0);
		nbuf[bufnum] = -1;
		for (i1 = 0; i1 < 4 && nbuf[i1] != i2; i1++);
		if (i1 == 4) {
			buf[i2].flgs &= ~LCKFLG;
			i1 = membuffer(buf[i2].bptr, xioxpurge, i2);
			if (i1) return(ERR_NOMEM);
			allocflg = TRUE;
		}
	}

	hash = (UCHAR)(filenum + (pos >> 8));
	for (i2 = hashbuf[hash]; i2 != 0xFF && (filenum != buf[i2].fnum || pos != buf[i2].pos); i2 = buf[i2].next);
	if (i2 == 0xFF) {
		pptr = NULL;
		if (freebuf == 0xFF) {
			i2 = 0xFF;
			u1 = (UINT) 0xFFFFFFFF;
			for (i1 = 0; i1 < maxbufs; i1++) {
				if (buf[i1].flgs & LCKFLG) continue;
				if (buf[i1].lrucnt < u1) {
					u1 = buf[i1].lrucnt;
					i2 = i1;
				}
			}
			if (i2 == 0xFF) return(ERR_PROGX);
			i1 = xioxfree(i2);
			if (i1 || bsize != ((INT) buf[i2].size << 8)) {
				memfree(buf[i2].bptr);
				if (i1) return(i1);
			}
			else pptr = buf[i2].bptr;
		}

		if (pptr == NULL) {
			pptr = memalloc(bsize + 1, 0);
			if (pptr == NULL) return(ERR_NOMEM);
			(*pptr)[bsize] = DBCDEL;  /* terminating block character */
			allocflg = TRUE;
		}
		if (rdflg) {
			i1 = fioread(filenum, pos, *pptr, bsize);
			if (i1 != bsize) {
				memfree(pptr);
				if (i1 < 0) return(i1);
				return(ERR_BADIX);
			}
		}

		i2 = freebuf;
		freebuf = buf[i2].next;
		buf[i2].next = hashbuf[hash];
		if (hashbuf[hash] != 0xFF) buf[hashbuf[hash]].prev = (UCHAR) i2;
		hashbuf[hash] = i2;
		buf[i2].prev = 0xFF;
		buf[i2].flgs = 0;
		buf[i2].size = (UCHAR)(bsize >> 8);
		buf[i2].fnum = filenum;
		buf[i2].pos = pos;
		buf[i2].bptr = pptr;
		if (i2 >= bufhi) bufhi = i2 + 1;
	}

	buf[i2].flgs |= LCKFLG;
	membufend(buf[i2].bptr);
	buf[i2].lrucnt = lrucnt++;
	nbuf[bufnum] = i2;

	if (allocflg) {
		for (i1 = 0; i1 < 4; i1++) if (nbuf[i1] != -1) *blkptr[i1] = *buf[nbuf[i1]].bptr;
		x = (struct xtab *) *fiogetwptr(filenum);
	}
	else *blkptr[bufnum] = *buf[nbuf[bufnum]].bptr;
	return(0);
}

/* XIOXNEWB */
/* add a new a block */
static INT xioxnewb(INT bufnum, OFFSET *pos)
{
	INT i1;
	UCHAR *blk;

	if (version >= 9) msc6xtooff(&blk0[1], pos);
	else msc9tooff(&blk0[1], pos);
	if (*pos) {  /* reuse of previously deleted block */
		i1 = xioxgetb(bufnum, *pos, TRUE);
		if (i1) return(i1);
		blk = *buf[nbuf[bufnum]].bptr;
		if (*blk != 'D') return(ERR_BADIX);
		memcpy(&blk0[1], &blk[1], size0);
	}
	else {
		if (version >= 9) msc6xtooff(&blk0[19], pos);
		else msc9tooff(&blk0[28], pos);
		*pos += blksize;
		i1 = xioxgetb(bufnum, *pos, FALSE);
		if (i1) return(i1);
		blk = *buf[nbuf[bufnum]].bptr;
		if (version >= 9) mscoffto6x(*pos, &blk0[19]);
		else mscoffto9(*pos, &blk0[28]);
	}
	memset(blk, DBCDEL, blksize);
	buf[nbuf[0]].flgs |= MODFLG;
	buf[nbuf[bufnum]].flgs |= MODFLG;
	return(0);
}

/* XIOXDELB */
/* delete block n which is at file position pos */
static void xioxdelb(INT bufnum, OFFSET pos)
{
	UCHAR *blk;

	blk = *buf[nbuf[bufnum]].bptr;
	memset(blk, DBCDEL, blksize);
	blk[0] = 'D';
	memcpy(&blk[1], &blk0[1], size0);
	if (version >= 9) mscoffto6x(pos, &blk0[1]);
	else {
		mscoffto9(pos, &blk0[1]);
		blk[10] = DBCEOR;
	}
	buf[nbuf[0]].flgs |= MODFLG;
	buf[nbuf[bufnum]].flgs |= MODFLG;
}

/**
 * XIOXEND
 * flush the buffers or place onto buffer list
 */
static INT xioxend(INT flushflg)
{
	INT i1, i2, retval;

	if (!flushflg) {
		for (i1 = 0; i1 < 4; i1++) {
			i2 = nbuf[i1];
			if (i2 != -1) {
				if (membuffer(buf[i2].bptr, xioxpurge, i2)) {
					flushflg = 1;
					break;
				}
				buf[i2].flgs &= ~LCKFLG;
			}
		}
	}

	retval = 0;
	if (flushflg) {
		i2 = bufhi;
		while (--i2 >= 0) {
			if (filenum != buf[i2].fnum) continue;
			if (flushflg == -1) buf[i2].flgs &= ~MODFLG;
			i1 = xioxfree(i2);
			if (i1) retval = i1;
			memfree(buf[i2].bptr);
		}
	}

	/* x may now be invalid from above membuffer or from xioxgetb() */
	x = (struct xtab *) *fiogetwptr(filenum);
	if (!retval) retval = x->errflg;
	x->errflg = 0;
	return(retval);
}

/* XIOXPURGE */
/* purge buffer function */
static void xioxpurge(INT bufnum)
{
	INT i1, fnum;
	UCHAR **xptr;
	struct xtab *x;

	if (bufnum < 0 || bufnum >= bufhi) return;
	fnum = buf[bufnum].fnum;
	if (!fnum) return;
	i1 = xioxfree(bufnum);
	if (i1) {
		xptr = fiogetwptr(fnum);
		if (xptr == NULL) return;
		x = (struct xtab *) *xptr;
		if (x->type != 'X') return;
		if (!x->errflg) x->errflg = i1;
	}
}

/*
 * XIOXFREE
 * put buffer onto free list, but do not release memory
 * Returns zero for success. Or an error value from fiowrite
 */
static INT xioxfree(INT bufnum)
{
	INT i1, i2, i3, retval;
	struct bufdef *bufptr;

	retval = 0;
	bufptr = &buf[bufnum];
	if (bufptr->flgs & MODFLG) {
		i1 = fiowrite(bufptr->fnum, bufptr->pos, *bufptr->bptr, (INT) bufptr->size << 8);
		if (i1) retval = i1;
	}
	i1 = bufptr->next;
	i2 = bufptr->prev;
	if (i1 != 0xFF) buf[i1].prev = (UCHAR) i2;
	if (i2 != 0xFF) buf[i2].next = (UCHAR) i1;
	else hashbuf[(UCHAR)(bufptr->fnum + (bufptr->pos >> 8))] = (UCHAR) i1;

	/* put back onto freebuf list in the correct order */
	i1 = i2 = freebuf;
	if (bufnum > freebuf) {  /* not going to be first in list */
		for ( ; ; ) {
			i3 = i2;
			i2 = buf[i2].next;
			if (bufnum < i2) break;
			if (i3 + 1 != i2) i1 = i2;  /* skip over gap */
		}
		if (i3 + 1 != bufnum) i1 = i2;  /* skip over gap */
		buf[i3].next = bufnum;
	}
	else freebuf = bufnum;
	if (bufnum + 1 == bufhi) {
		if (i1 < bufnum) bufhi = i1;
		else bufhi = bufnum;
	}

	bufptr->next = (UCHAR) i2;
	bufptr->fnum = 0;
	return(retval);
}
