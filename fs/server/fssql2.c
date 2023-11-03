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
#define INC_WINDOWS
#include "includes.h"
#include "base.h"
#include "fsrun.h"
#include "fssql.h"
#include "fssqlx.h"
#include "fio.h"

#define READ_INDEX_NONE		0
#define READ_INDEX_AIMDEX	1
#define READ_INDEX_RANGE		2
#define READ_INDEX_EXACT_DUP	3
#define READ_INDEX_EXACT		4

#define KEYTYPE_EQ 0x01
#define KEYTYPE_GE 0x02
#define KEYTYPE_GT 0x04
#define KEYTYPE_LE 0x08
#define KEYTYPE_LT 0x10

#define TEMPLATE_EXACT		1
#define TEMPLATE_PATTERN	2

extern INT fsflags;
extern CONNECTION connection;
extern OFFSET filepos;
extern int sqlstatisticsflag;

static USHORT lrucount = 1;
static CHAR lasttable[MAX_NAME_LENGTH + 1];
static CHAR lastalias[MAX_NAME_LENGTH + 1];

static CHAR *getcrfinfo(COLREF *crf1, HCORRTABLE corrtable);
static INT matchname(CHAR *name, CHAR *pattern, INT templateflag);
static void replacename(CHAR *srcpattern, CHAR *srcname, CHAR *destpattern, CHAR *destname);

/* return a result set with column information */
/* return 4 if statement succeeded with empty result set */
/* return 6 if statement succeeded with result set, w/ rsid = result id */
/* return negative if error */
INT getcolumninfo(CHAR *table, CHAR *column, INT *rsid)
{
	static struct {
		INT length;
		INT type;
	} fielddefs[] = {
		0,	TYPE_CHAR,		/* TABLE_NAME */
		0,	TYPE_CHAR,		/* COLUMN_NAME */
		1,	TYPE_CHAR,		/* DATA_TYPE */
/*** CODE: COLUMN_SIZE COULD PROBABLY BE 5 ***/
		6,	TYPE_NUM,		/* COLUMN_SIZE */
		2,	TYPE_NUM,		/* DECIMAL_DIGITS */
		0,	TYPE_CHAR,		/* REMARKS */
/*** CODE: ORDINAL_POSITION COULD PROBABLY BE 4-5 ***/
		6,	TYPE_NUM,		/* ORDINAL_POSITION */
	};
	INT i1, columnnum, count, filenamelen;
	INT offset, tablenum, rowbytecount, worksetnum;
	CHAR datatype, filename[MAX_NAMESIZE + 1];
	CHAR tablename[MAX_NAME_LENGTH + 1], tablework[MAX_NAME_LENGTH + 1];
	CHAR workname1[MAX_NAME_LENGTH + 1], workname2[MAX_NAME_LENGTH + 1], *found;
	UCHAR *currentrow, **hrows;
	TABLE *tab1;
	COLUMN *col1;
	COLREF *crf1;
	HCOLREF hcrf1;
	WORKSET *wks1;

	/* NOTE: this function corresponds to SQLColumns (search patterns allowedin both names) */

	/* fill in length of variable length fields */
	fielddefs[0].length = connection.maxtablename;
	fielddefs[1].length = connection.maxcolumnname;
	fielddefs[5].length = connection.maxcolumnremarks;

	/* calculate length of rows */
	for (i1 = 0, rowbytecount = 0; (size_t) i1 < sizeof(fielddefs) / sizeof(*fielddefs); i1++)
		rowbytecount += fielddefs[i1].length;
	/* build column result fields */
	hcrf1 = (HCOLREF) memalloc((sizeof(fielddefs) / sizeof(*fielddefs)) * sizeof(COLREF), 0);
	if (hcrf1 == NULL) return sqlerrnum(SQLERR_NOMEM);
	crf1 = *hcrf1;
	for (i1 = offset = 0; (size_t) i1 < sizeof(fielddefs) / sizeof(*fielddefs); i1++) {
		crf1[i1].tabcolnum = 0x00010001 + i1;
		crf1[i1].offset = offset;
		crf1[i1].type = fielddefs[i1].type;
		crf1[i1].length = fielddefs[i1].length;
		offset += fielddefs[i1].length;
		crf1[i1].scale = 0;
		crf1[i1].tablenum = 0;
		crf1[i1].columnnum = 0;
	}

	for (i1 = 0; table[i1]; i1++) workname1[i1] = (CHAR) toupper(table[i1]);
	if (!i1) workname1[i1++] = '%';
	workname1[i1] = '\0';
	//
	//CODE Note that we are converting the incoming column name to upper case here
	//
	for (i1 = 0; column[i1]; i1++) workname2[i1] = (CHAR) toupper(column[i1]);
	if (!i1) workname2[i1++] = '%';
	workname2[i1] = '\0';
	for (tablenum = count = 0; ++tablenum <= connection.numtables; ) {
		tab1 = tableptr(tablenum);
		strcpy(tablename, nameptr(tab1->name));
		if (!matchname(tablename, workname1, (tab1->flags & TABLE_FLAGS_TEMPLATE) ? (tab1->flags & TABLE_FLAGS_TEMPLATEDIR) ? TEMPLATE_EXACT : TEMPLATE_PATTERN : 0)) continue;
		if ((tab1->flags & (TABLE_FLAGS_TEMPLATE | TABLE_FLAGS_TEMPLATEDIR)) == TABLE_FLAGS_TEMPLATE) {
			replacename(nameptr(tab1->name), tablename, nameptr(tab1->textfilename), filename);
			miofixname(filename, ".txt", FIXNAME_EXT_ADD);
			if (fiofindfirst(filename, FIO_P_TXT, &found)) continue;
			tab1 = tableptr(tablenum);
			miofixname(filename, NULL, FIXNAME_PAR_DBCVOL);
			filenamelen = (INT)strlen(filename);
			strcpy(tablework, tablename);
		}
		do {
			if ((tab1->flags & (TABLE_FLAGS_TEMPLATE | TABLE_FLAGS_TEMPLATEDIR)) == TABLE_FLAGS_TEMPLATE)
				replacename(filename, found + strlen(found) - filenamelen, tablework, tablename);
			for (columnnum = 0; ++columnnum <= tab1->numcolumns; ) {
				col1 = columnptr(tab1, columnnum);
				if (!matchname(nameptr(col1->name), workname2, 0)) continue;
				if (!count++) {
					hrows = memalloc(rowbytecount, 0);
					if (hrows == NULL) {
						memfree((UCHAR **) hcrf1);
						return sqlerrnum(SQLERR_NOMEM);
					}
				}
				else if (memchange(hrows, count * rowbytecount, 0) < 0) {
					memfree(hrows);
					memfree((UCHAR **) hcrf1);
					return sqlerrnum(SQLERR_NOMEM);
				}
				tab1 = tableptr(tablenum);
				col1 = columnptr(tab1, columnnum);
				currentrow = *hrows + (count - 1) * rowbytecount;
				memset(currentrow, ' ', rowbytecount);
				memcpy(currentrow + crf1[0].offset, tablename, strlen(tablename));
				memcpy(currentrow + crf1[1].offset, nameptr(col1->name), strlen(nameptr(col1->name)));
				switch (col1->type) {
					case TYPE_NUM:
					case TYPE_POSNUM:
						datatype = 'N';
						break;
					case TYPE_DATE:
						datatype = 'D';
						break;
					case TYPE_TIME:
						datatype = 'T';
						break;
					case TYPE_TIMESTAMP:
						datatype = 'S';
						break;
					case TYPE_CHAR:
					default:
						datatype = 'C';
						break;
				}
				currentrow[crf1[2].offset] = datatype;
				i1 = col1->length;
				if ((col1->type == TYPE_NUM || col1->type == TYPE_POSNUM) && col1->scale) i1--;
				msciton(i1, currentrow + crf1[3].offset, crf1[3].length);
				if (col1->type == TYPE_NUM || col1->type == TYPE_POSNUM || col1->type == TYPE_TIME || col1->type == TYPE_TIMESTAMP)
					msciton(col1->scale, currentrow + crf1[4].offset, crf1[4].length);
				if (col1->description)
					memcpy(currentrow + crf1[5].offset, nameptr(col1->description), strlen(nameptr(col1->description)));
				msciton(columnnum, currentrow + crf1[6].offset, crf1[6].length);
			}
		} while ((tab1->flags & (TABLE_FLAGS_TEMPLATE | TABLE_FLAGS_TEMPLATEDIR)) == TABLE_FLAGS_TEMPLATE && !fiofindnext(&found));
	}
	if (count) {
		worksetnum = allocworkset(1);
		if (worksetnum < 0) {
			memfree(hrows);
			memfree((UCHAR **) hcrf1);
			return worksetnum;
		}
		wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset;
		wks1->updatable = 'R';
		wks1->numcolrefs = wks1->rsnumcolumns = sizeof(fielddefs) / sizeof(*fielddefs);
		wks1->colrefarray = (HCOLREF) hcrf1;
		wks1->rsrowbytes = wks1->rowbytes = rowbytecount;
		wks1->rows = hrows;
		wks1->rowcount = wks1->memrowalloc = count;
		*rsid = worksetnum;
		return 6;
	}
	memfree((UCHAR **) hcrf1);
	return 4;  /* let driver build result set column information */
}

/* return a result set with index information */
/* return 4 if statement succeeded with empty result set */
/* return 6 if statement succeeded with result set, w/ rsid = result id */
/* return negative if error */
INT getprimaryinfo(CHAR *table, INT *rsid)
{
	static struct tagFIELDDEFS {
		INT length;
		INT type;
	} fielddefs[] = {
		0,	TYPE_CHAR,		/* TABLE_NAME */
		0,	TYPE_CHAR,		/* COLUMN_NAME */
		4,	TYPE_NUM		/* KEY_SEQ */
	};
	INT i1, i2, colref, count, indexnum, keylen, offset;
	INT rowbytecount, size, tablenum, worksetnum;
	CHAR tablename[MAX_NAME_LENGTH + 1], workname[MAX_NAME_LENGTH + 1];
	UCHAR *currentrow, **hrows;
	TABLE *tab1;
	COLUMN *col1;
	COLREF *crf1;
	HCOLREF hcrf1;
	WORKSET *wks1;
	INDEX *idx1;
	COLKEY *colkeys;

	/* NOTE: this function corresponds to SQLPrimaryKeys (no search patterns) */

	/* fill in length of variable length fields */
	fielddefs[0].length = connection.maxtablename;
	fielddefs[1].length = connection.maxcolumnname;

	/* calculate length of rows */
	for (i1 = 0, rowbytecount = 0; (size_t) i1 < sizeof(fielddefs) / sizeof(*fielddefs); i1++)
		rowbytecount += fielddefs[i1].length;
	/* build column result fields */
	hcrf1 = (HCOLREF) memalloc((sizeof(fielddefs) / sizeof(*fielddefs)) * sizeof(COLREF), 0);
	if (hcrf1 == NULL) return sqlerrnum(SQLERR_NOMEM);
	crf1 = *hcrf1;
	for (i1 = offset = 0; (size_t) i1 < sizeof(fielddefs) / sizeof(*fielddefs); i1++) {
		crf1[i1].tabcolnum = 0x00010001 + i1;
		crf1[i1].offset = offset;
		crf1[i1].type = fielddefs[i1].type;
		crf1[i1].length = fielddefs[i1].length;
		offset += fielddefs[i1].length;
		crf1[i1].scale = 0;
		crf1[i1].tablenum = 0;
		crf1[i1].columnnum = 0;
	}

	/* escape search patterns */
	for (i1 = i2 = 0; table[i1]; i1++) {
		if (table[i1] == '\\' || table[i1] == '_' || table[i1] == '%') workname[i2++] = '\\';
		workname[i2++] = (CHAR) toupper(table[i1]);
	}
	if (!i2) {
		memfree((UCHAR **) hcrf1);
		return 4;
	}
	workname[i2] = '\0';
	for (tablenum = 0; ++tablenum <= connection.numtables; ) {
		tab1 = tableptr(tablenum);
		strcpy(tablename, nameptr(tab1->name));
		if (matchname(tablename, workname, (tab1->flags & TABLE_FLAGS_TEMPLATE) ? TEMPLATE_EXACT : 0)) break;
	}
	if (tablenum > connection.numtables) {
		memfree((UCHAR **) hcrf1);
		return 4;
	}
	if (tab1->flags & TABLE_FLAGS_TEMPLATE) {
		if (maketemplate(tablename, &tablenum)) {
			memfree((UCHAR **) hcrf1);
			return -1;
		}
		tab1 = tableptr(tablenum);
	}
	if (!(tab1->flags & TABLE_FLAGS_COMPLETE)) {
		i1 = maketablecomplete(tablenum);
		if (i1) return i1;
		tab1 = tableptr(tablenum);
	}

	for (indexnum = count = size = keylen = 0; ++indexnum <= tab1->numindexes; ) {
		idx1 = indexptr(tab1, indexnum);
		if (idx1->type != INDEX_TYPE_ISAM) continue;
		if (idx1->flags & INDEX_DUPS) continue;  /* dups */
		/* assume index with shortest key is primary key */
		if (keylen && idx1->keylength >= keylen) continue;
		if (idx1->hcolkeys == NULL) continue;
		colkeys = *idx1->hcolkeys;
		for (colref = i1 = 0; colref != -1; colref = colkeys[colref].next) i1 += colkeys[colref].len;
		if (i1 != idx1->keylength) continue;  /* missing keys */
		keylen = i1;
		for (colref = count = 0; colref != -1; colref = colkeys[colref].next) {
			if (count++ == size) {
				if (!size++) {
					hrows = memalloc(rowbytecount, 0);
					if (hrows == NULL) {
						memfree((UCHAR **) hcrf1);
						return sqlerrnum(SQLERR_NOMEM);
					}
				}
				else if (memchange(hrows, size * rowbytecount, 0) < 0) {
					memfree(hrows);
					memfree((UCHAR **) hcrf1);
					return sqlerrnum(SQLERR_NOMEM);
				}
				tab1 = tableptr(tablenum);
				idx1 = indexptr(tab1, indexnum);
			}
			col1 = columnptr(tab1, colkeys[colref].colnum);
			currentrow = *hrows + (count - 1) * rowbytecount;
			memset(currentrow, ' ', rowbytecount);
			memcpy(currentrow + crf1[0].offset, tablename, strlen(tablename));
			memcpy(currentrow + crf1[1].offset, nameptr(col1->name), strlen(nameptr(col1->name)));
			msciton(count, currentrow + crf1[2].offset, crf1[2].length);
		}
	}
	if (count) {
		if (size > count) memchange(hrows, count * rowbytecount, 0);
		worksetnum = allocworkset(1);
		if (worksetnum < 0) {
			memfree(hrows);
			memfree((UCHAR **) hcrf1);
			return worksetnum;
		}
		wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset;
		wks1->updatable = 'R';
		wks1->numcolrefs = wks1->rsnumcolumns = sizeof(fielddefs) / sizeof(*fielddefs);
		wks1->colrefarray = (HCOLREF) hcrf1;
		wks1->rsrowbytes = wks1->rowbytes = rowbytecount;
		wks1->rows = hrows;
		wks1->rowcount = wks1->memrowalloc = count;
		*rsid = worksetnum;
		return 6;
	}
	memfree((UCHAR **) hcrf1);
	return 4;  /* let driver build result set column information */
}

/* Return a result set with an optimal set of columns that uniquely identifies a row*/
/* return 4 if statement succeeded with empty result set */
/* return 6 if statement succeeded with result set, w/ rsid = result id */
/* return negative if error */
INT getbestrowid(CHAR *table, INT *rsid)
{
	static struct {
		INT length;
		INT type;
	} fielddefs[] = {
		0,	TYPE_CHAR,		/* COLUMN_NAME */
		1,	TYPE_CHAR,		/* DATA_TYPE */
/*** CODE: COLUMN_SIZE COULD PROBABLY BE 5 ***/
		6,	TYPE_NUM,		/* COLUMN_SIZE */
		2,	TYPE_NUM,		/* DECIMAL_DIGITS */
	};
	INT i1, i2, colref, count, indexnum, keylen, offset, rowbytecount, size, tablenum, worksetnum;
	CHAR datatype, tablename[MAX_NAME_LENGTH + 1], workname[MAX_NAME_LENGTH + 1];
	UCHAR *currentrow, **hrows;
	TABLE *tab1;
	COLUMN *col1;
	COLREF *crf1;
	HCOLREF hcrf1;
	WORKSET *wks1;
	INDEX *idx1;
	COLKEY *colkeys;

	/* NOTE: this function corresponds to SQLSpecialColumns(SQL_BEST_ROWID) (no search patterns) */

	/* fill in length of variable length fields */
	fielddefs[0].length = connection.maxcolumnname;

	/* calculate length of rows */
	for (i1 = 0, rowbytecount = 0; (size_t) i1 < sizeof(fielddefs) / sizeof(*fielddefs); i1++)
		rowbytecount += fielddefs[i1].length;
	/* build column result fields */
	hcrf1 = (HCOLREF) memalloc((sizeof(fielddefs) / sizeof(*fielddefs)) * sizeof(COLREF), 0);
	if (hcrf1 == NULL) return sqlerrnum(SQLERR_NOMEM);
	crf1 = *hcrf1;
	for (i1 = offset = 0; (size_t) i1 < sizeof(fielddefs) / sizeof(*fielddefs); i1++) {
		crf1[i1].tabcolnum = 0x00010001 + i1;
		crf1[i1].offset = offset;
		crf1[i1].type = fielddefs[i1].type;
		crf1[i1].length = fielddefs[i1].length;
		offset += fielddefs[i1].length;
		crf1[i1].scale = 0;
		crf1[i1].tablenum = 0;
		crf1[i1].columnnum = 0;
	}

	/* escape search patterns */
	for (i1 = i2 = 0; table[i1]; i1++) {
		if (table[i1] == '\\' || table[i1] == '_' || table[i1] == '%') workname[i2++] = '\\';
		workname[i2++] = (CHAR) toupper(table[i1]);
	}
	if (!i2) {
		memfree((UCHAR **) hcrf1);
		return 4;
	}
	workname[i2] = '\0';
	for (tablenum = 0; ++tablenum <= connection.numtables; ) {
		tab1 = tableptr(tablenum);
		strcpy(tablename, nameptr(tab1->name));
		if (matchname(tablename, workname, (tab1->flags & TABLE_FLAGS_TEMPLATE) ? TEMPLATE_EXACT : 0)) break;
	}
	if (tablenum > connection.numtables || tab1->numindexes < 1) {
		memfree((UCHAR **) hcrf1);
		return 4;
	}
	if (tab1->flags & TABLE_FLAGS_TEMPLATE) {
		if (maketemplate(tablename, &tablenum)) {
			memfree((UCHAR **) hcrf1);
			return -1;
		}
		tab1 = tableptr(tablenum);
	}
	if (!(tab1->flags & TABLE_FLAGS_COMPLETE)) {
		i1 = maketablecomplete(tablenum);
		if (i1) return i1;
		tab1 = tableptr(tablenum);
	}

	for (indexnum = count = size = keylen = 0; ++indexnum <= tab1->numindexes; ) {
		idx1 = indexptr(tab1, indexnum);
		if (idx1->type != INDEX_TYPE_ISAM) continue;
		if (idx1->flags & INDEX_DUPS) continue;  /* dups */
		/* assume index with shortest key is best index */
		if (keylen && idx1->keylength >= keylen) continue;
		if (idx1->hcolkeys == NULL) continue;
		colkeys = *idx1->hcolkeys;
		for (colref = i1 = 0; colref != -1; colref = colkeys[colref].next) i1 += colkeys[colref].len;
		if (i1 != idx1->keylength) continue;  /* missing keys */
		keylen = i1;
		for (colref = count = 0; colref != -1; colref = colkeys[colref].next) {
			if (count++ == size) {
				if (!size++) {
					hrows = memalloc(rowbytecount, 0);
					if (hrows == NULL) {
						memfree((UCHAR **) hcrf1);
						return sqlerrnum(SQLERR_NOMEM);
					}
				}
				else if (memchange(hrows, size * rowbytecount, 0) < 0) {
					memfree(hrows);
					memfree((UCHAR **) hcrf1);
					return sqlerrnum(SQLERR_NOMEM);
				}
				tab1 = tableptr(tablenum);
				idx1 = indexptr(tab1, indexnum);
			}
			col1 = columnptr(tab1, colkeys[colref].colnum);
			currentrow = *hrows + (count - 1) * rowbytecount;
			memset(currentrow, ' ', rowbytecount);
			memcpy(currentrow + crf1[0].offset, nameptr(col1->name), strlen(nameptr(col1->name)));
			switch (col1->type) {
				case TYPE_NUM:
				case TYPE_POSNUM:
					datatype = 'N';
					break;
				case TYPE_DATE:
					datatype = 'D';
					break;
				case TYPE_TIME:
					datatype = 'T';
					break;
				case TYPE_TIMESTAMP:
					datatype = 'S';
					break;
				case TYPE_CHAR:
				default:
					datatype = 'C';
					break;
			}
			currentrow[crf1[1].offset] = datatype;
			i1 = col1->length;
			if ((col1->type == TYPE_NUM || col1->type == TYPE_POSNUM) && col1->scale) i1--;
			msciton(i1, currentrow + crf1[2].offset, crf1[2].length);
			if (col1->type == TYPE_NUM || col1->type == TYPE_POSNUM || col1->type == TYPE_TIME || col1->type == TYPE_TIMESTAMP)
				msciton(col1->scale, currentrow + crf1[3].offset, crf1[3].length);
		}
	}
	if (count) {
		if (size > count) memchange(hrows, count * rowbytecount, 0);
		worksetnum = allocworkset(1);
		if (worksetnum < 0) {
			memfree(hrows);
			memfree((UCHAR **) hcrf1);
			return worksetnum;
		}
		wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset;
		wks1->updatable = 'R';
		wks1->numcolrefs = wks1->rsnumcolumns = sizeof(fielddefs) / sizeof(*fielddefs);
		wks1->colrefarray = (HCOLREF) hcrf1;
		wks1->rsrowbytes = wks1->rowbytes = rowbytecount;
		wks1->rows = hrows;
		wks1->rowcount = wks1->memrowalloc = count;
		*rsid = worksetnum;
		return 6;
	}
	memfree((UCHAR **) hcrf1);
	return 4;  /* let driver build result set column information */
}

/* return a result set with index information */
/* return 4 if statement succeeded with empty result set */
/* return 6 if statement succeeded with result set, w/ rsid = result id */
/* return negative if error */
INT getindexinfo(CHAR *table, INT allindexes, INT *rsid)
{
	static struct tagFIELDDEFS {
		INT length;
		INT type;
	} fielddefs[] = {
		0,	TYPE_CHAR,		/* TABLE_NAME */
		1,	TYPE_NUM,		/* NON_UNIQUE */
		0,	TYPE_CHAR,		/* INDEX_NAME */
		1,	TYPE_CHAR,		/* TYPE */
		4,	TYPE_NUM,		/* ORDINAL_POSITION */
		0,	TYPE_CHAR,		/* COLUMN_NAME */
		1,	TYPE_CHAR		/* ASC_OR_DESC */
#if 0
/*** CODE: WOULD WE WANT THIS TO RETURN SIZE OF FILE ***/
		10,	TYPE_NUM,		/* PAGES */
/*** CODE: SHOULD THIS RETURN THE -P OPTIONS ??? ***/
		0,	TYPE_CHAR		/* FILTER_CONDITION */
#endif
	};
	INT i1, i2, i3, cnt, colref, count, indexnum, keylen, nextref, offset, sortedflag;
	INT rowbytecount, tablenum, worksetnum, trace[MAX_IDXKEYS];
	CHAR filename[MAX_NAMESIZE], tablename[MAX_NAME_LENGTH + 1];
	CHAR workname[MAX_NAME_LENGTH + 1], *ptr1, *ptr2;
	UCHAR *currentrow, *nextrow, **hrows;
	TABLE *tab1;
	COLUMN *col1;
	COLREF *crf1;
	HCOLREF hcrf1;
	WORKSET *wks1;
	INDEX *idx1;
	COLKEY *colkeys;
#if 0
	COLKEY colkeywork;
#endif

	/* NOTE: this function corresponds to SQLStatistics (no search patterns) */

	/* fill in length of variable length fields */
	fielddefs[0].length = connection.maxtablename;
	fielddefs[2].length = connection.maxtableindex;
	fielddefs[5].length = connection.maxcolumnname;

	/* calculate length of rows */
	for (i1 = 0, rowbytecount = 0; (size_t) i1 < sizeof(fielddefs) / sizeof(*fielddefs); i1++)
		rowbytecount += fielddefs[i1].length;
	/* build column result fields */
	hcrf1 = (HCOLREF) memalloc((sizeof(fielddefs) / sizeof(*fielddefs)) * sizeof(COLREF), 0);
	if (hcrf1 == NULL) return sqlerrnum(SQLERR_NOMEM);
	crf1 = *hcrf1;
	for (i1 = offset = 0; (size_t) i1 < sizeof(fielddefs) / sizeof(*fielddefs); i1++) {
		crf1[i1].tabcolnum = 0x00010001 + i1;
		crf1[i1].offset = offset;
		crf1[i1].type = fielddefs[i1].type;
		crf1[i1].length = fielddefs[i1].length;
		offset += fielddefs[i1].length;
		crf1[i1].scale = 0;
		crf1[i1].tablenum = 0;
		crf1[i1].columnnum = 0;
	}

	/* escape search patterns */
	for (i1 = i2 = 0; table[i1]; i1++) {
		if (table[i1] == '\\' || table[i1] == '_' || table[i1] == '%') workname[i2++] = '\\';
		workname[i2++] = (CHAR) toupper(table[i1]);
	}
	if (!i2) {
		memfree((UCHAR **) hcrf1);
		return 4;
	}
	workname[i2] = '\0';
	for (tablenum = 0; ++tablenum <= connection.numtables; ) {
		tab1 = tableptr(tablenum);
		strcpy(tablename, nameptr(tab1->name));
		if (matchname(tablename, workname, (tab1->flags & TABLE_FLAGS_TEMPLATE) ? TEMPLATE_EXACT : 0)) break;
	}
	/* NOTE: fs2 returned empty set if tab1->numindexes < 1 */
	if (tablenum > connection.numtables) {
		memfree((UCHAR **) hcrf1);
		return 4;
	}
	if (tab1->flags & TABLE_FLAGS_TEMPLATE) {
		if (maketemplate(tablename, &tablenum)) {
			memfree((UCHAR **) hcrf1);
			return -1;
		}
		tab1 = tableptr(tablenum);
	}
	if (!(tab1->flags & TABLE_FLAGS_COMPLETE)) {
		i1 = maketablecomplete(tablenum);
		if (i1) return i1;
		tab1 = tableptr(tablenum);
	}

	/* create stat row to be used as sort work area and filled in at return */
	hrows = memalloc(rowbytecount, 0);
	if (hrows == NULL) {
		memfree((UCHAR **) hcrf1);
		return sqlerrnum(SQLERR_NOMEM);
	}
	count = 1;
	for (indexnum = 0; ++indexnum <= tab1->numindexes; ) {
		idx1 = indexptr(tab1, indexnum);
		if (!allindexes && (idx1->type != INDEX_TYPE_ISAM || (idx1->flags & INDEX_DUPS))) continue;  /* not isi or dups */
		if (idx1->type == INDEX_TYPE_AIM && !(sqlstatisticsflag & 0x04)) continue;
		if (idx1->hcolkeys != NULL) colkeys = *idx1->hcolkeys;
		else {
#if 0
/*** CODE: DON'T USE FOR NOW 12-26-00 ***/
			colkeywork.len = 0;
			colkeywork.next = colkeywork.alt = -1;
			colkeys = &colkeywork;
#else
			continue;
#endif
		}
#if 0
/*** NOTE: RETURNING ALTERNATE KEYS CAUSES ACCESS.EXE TO FAIL, BUT LEFT CODE HERE ***/
/***       FOR FUTURE POSSIBLE USE ***/
		for (nextref = keylen = cnt = 0; nextref != -1; nextref = colkeys[nextref].alt) {
#else
			nextref = keylen = cnt = 0;
#endif
			do {
				trace[cnt++] = nextref;
				keylen += colkeys[nextref].len;
				nextref = colkeys[nextref].next;
			} while (nextref != -1);
			if (allindexes || keylen == idx1->keylength) {
				for (colref = 0; colref < cnt; colref++) {
					if (memchange(hrows, ++count * rowbytecount, 0) < 0) {
						memfree(hrows);
						memfree((UCHAR **) hcrf1);
						return sqlerrnum(SQLERR_NOMEM);
					}
					tab1 = tableptr(tablenum);
					idx1 = indexptr(tab1, indexnum);
					currentrow = *hrows + (count - 1) * rowbytecount;
					memset(currentrow, ' ', rowbytecount);
					memcpy(currentrow + crf1[0].offset, tablename, strlen(tablename));
					if (idx1->type == INDEX_TYPE_ISAM) i1 = (idx1->flags & INDEX_DUPS) ? 1 : 0;
					else i1 = 1;
					msciton(i1, currentrow + crf1[1].offset, crf1[1].length);
					/* MS Access needs the extension removed, also remove any directory specification */
					strcpy(filename, nameptr(idx1->indexfilename));
					if (!(sqlstatisticsflag & 0x03))
						miofixname(filename, "", FIXNAME_EXT_REPLACE | FIXNAME_PAR_DBCVOL | FIXNAME_PAR_MEMBER);
					else if (sqlstatisticsflag & 0x02) {
						for (i1 = (INT)strlen(filename) - 1; i1 > 0 && filename[i1] != '.' && filename[i1] != '/' && filename[i1] != '\\' && filename[i1] != ':'; i1--);
						if (filename[i1] == '.') filename[i1] = '_';
					}
					miogetname(&ptr1, &ptr2);
					if (*ptr2) strcpy(filename, ptr2);
					i3 = fioaslash(filename) + 1;
					memcpy(currentrow + crf1[2].offset, filename + i3, strlen(filename + i3));
					if (idx1->type == INDEX_TYPE_ISAM) currentrow[crf1[3].offset] = 'I';
					else currentrow[crf1[3].offset] = 'A';
					if (idx1->hcolkeys != NULL) {
						msciton(colref + 1, currentrow + crf1[4].offset, crf1[4].length);
						col1 = columnptr(tab1, colkeys[trace[colref]].colnum);
						memcpy(currentrow + crf1[5].offset, nameptr(col1->name), strlen(nameptr(col1->name)));
					}
					currentrow[crf1[6].offset] = 'A';  /* always asending */
				}
			}
#if 0
			/* back up to next alternate */
			do keylen -= colkeys[trace[--cnt]].len;
			while (cnt && colkeys[trace[cnt]].alt == -1);
			nextref = trace[cnt];
		}
#endif
	}
	/* NOTE: fs2 returned empty set if count == 1 */
	/* sort by NON_UNIQUE, TYPE, INDEX_NAME, and ORDINAL_POSITION */
	/* not sure of the name of the sort, but the concept is to take the highest */
	/* record and place it at the end. next pass takes the next highest and so on. */
	/* use the stat row for swapping */
	for (i2 = count; --i2 > 1; ) {
		sortedflag = TRUE;
		nextrow = currentrow = *hrows + rowbytecount;
		for (i1 = 0; ++i1 < i2; currentrow = nextrow) {
			nextrow += rowbytecount;
			i3 = memcmp(currentrow + crf1[1].offset, nextrow + crf1[1].offset, crf1[1].length);
			if (!i3) {
				i3 = memcmp(currentrow + crf1[3].offset, nextrow + crf1[3].offset, crf1[3].length);
				if (!i3) {
					i3 = memcmp(currentrow + crf1[2].offset, nextrow + crf1[2].offset, crf1[2].length);
#if 0
/*** CODE: DON'T SORT ON ORDINAL POSITION AS THIS WILL INTERMIX MULTIPLE KEYS (SHOULD BE IN ORDER ANYWAY) ***/
					if (!i3) i3 = memcmp(currentrow + crf1[4].offset, nextrow + crf1[4].offset, crf1[4].length);
#endif
				}
			}
			if (i3 > 0) {  /* swap rows */
				memcpy(*hrows, nextrow, rowbytecount);
				memcpy(nextrow, currentrow, rowbytecount);
				memcpy(currentrow, *hrows, rowbytecount);
				sortedflag = FALSE;
			}
		}
		if (sortedflag) break;  /* completely sorted */
	}
	/* finish up the stat row */
	currentrow = *hrows;
	memset(currentrow, ' ', rowbytecount);
	memcpy(currentrow + crf1[0].offset, tablename, strlen(tablename));

	worksetnum = allocworkset(1);
	if (worksetnum < 0) {
		memfree(hrows);
		memfree((UCHAR **) hcrf1);
		return worksetnum;
	}
	wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset;
	wks1->updatable = 'R';
	wks1->numcolrefs = wks1->rsnumcolumns = sizeof(fielddefs) / sizeof(*fielddefs);
	wks1->colrefarray = (HCOLREF) hcrf1;
	wks1->rsrowbytes = wks1->rowbytes = rowbytecount;
	wks1->rows = hrows;
	wks1->rowcount = wks1->memrowalloc = count;
	*rsid = worksetnum;
	return 6;
}

/* return 4 if statement succeeded with empty result set */
/* return 6 if statement succeeded with result set, w/ rsid = result id */
/* return negative if error */
INT gettableinfo(CHAR *table, INT *rsid)
{
	static struct {
		INT length;
		INT type;
	} fielddefs[] = {
		0,	TYPE_CHAR,		/* TABLE_NAME */
		0,	TYPE_CHAR		/* REMARKS */
	};
	INT i1, count, filenamelen, offset, rowbytecount, tablenum, worksetnum;
	CHAR filename[MAX_NAMESIZE + 1];
	CHAR tablename[MAX_NAME_LENGTH + 1], tablework[MAX_NAME_LENGTH + 1];
	CHAR workname[MAX_NAME_LENGTH + 1], *found;
	UCHAR *currentrow, **hrows;
	TABLE *tab1;
	WORKSET *wks1;
	COLREF *crf1;
	HCOLREF hcrf1;

	/* NOTE: this function corresponds to SQLTables (search patterns allowed) */

	/* fill in length of variable length fields */
	fielddefs[0].length = connection.maxtablename;
	fielddefs[1].length = connection.maxtableremarks;

	/* calculate length of rows */
	for (i1 = 0, rowbytecount = 0; (size_t) i1 < sizeof(fielddefs) / sizeof(*fielddefs); i1++)
		rowbytecount += fielddefs[i1].length;
	/* build column result fields */
	hcrf1 = (HCOLREF) memalloc((sizeof(fielddefs) / sizeof(*fielddefs)) * sizeof(COLREF), 0);
	if (hcrf1 == NULL) return sqlerrnum(SQLERR_NOMEM);
	crf1 = *hcrf1;
	for (i1 = offset = 0; (size_t) i1 < sizeof(fielddefs) / sizeof(*fielddefs); i1++) {
		crf1[i1].tabcolnum = 0x00010001 + i1;
		crf1[i1].offset = offset;
		crf1[i1].type = fielddefs[i1].type;
		crf1[i1].length = fielddefs[i1].length;
		offset += fielddefs[i1].length;
		crf1[i1].scale = 0;
		crf1[i1].tablenum = 0;
		crf1[i1].columnnum = 0;
	}

	for (i1 = 0; table[i1]; i1++) workname[i1] = (CHAR) toupper(table[i1]);
	if (!i1) workname[i1++] = '%';
	workname[i1] = '\0';
	for (tablenum = count = 0; ++tablenum <= connection.numtables; ) {
		tab1 = tableptr(tablenum);
		strcpy(tablename, nameptr(tab1->name));
		if (!matchname(tablename, workname, (tab1->flags & TABLE_FLAGS_TEMPLATE) ? (tab1->flags & TABLE_FLAGS_TEMPLATEDIR) ? TEMPLATE_EXACT : TEMPLATE_PATTERN : 0)) continue;
		if ((tab1->flags & (TABLE_FLAGS_TEMPLATE | TABLE_FLAGS_TEMPLATEDIR)) == TABLE_FLAGS_TEMPLATE) {
			replacename(nameptr(tab1->name), tablename, nameptr(tab1->textfilename), filename);
			miofixname(filename, ".txt", FIXNAME_EXT_ADD);
			if (fiofindfirst(filename, FIO_P_TXT, &found)) continue;
			tab1 = tableptr(tablenum);
			miofixname(filename, NULL, FIXNAME_PAR_DBCVOL);
			filenamelen = (INT)strlen(filename);
			strcpy(tablework, tablename);
		}
		do {
			if ((tab1->flags & (TABLE_FLAGS_TEMPLATE | TABLE_FLAGS_TEMPLATEDIR)) == TABLE_FLAGS_TEMPLATE)
				replacename(filename, found + strlen(found) - filenamelen, tablework, tablename);
			if (!count++) {
				hrows = memalloc(rowbytecount, 0);
				if (hrows == NULL) {
					memfree((UCHAR **) hcrf1);
					return sqlerrnum(SQLERR_NOMEM);
				}
			}
			else if (memchange(hrows, count * rowbytecount, 0) < 0) {
				memfree(hrows);
				memfree((UCHAR **) hcrf1);
				return sqlerrnum(SQLERR_NOMEM);
			}
			tab1 = tableptr(tablenum);
			currentrow = *hrows + (count - 1) * rowbytecount;
			memset(currentrow, ' ', rowbytecount);
			memcpy(currentrow + crf1[0].offset, tablename, strlen(tablename));
			if (tab1->description)
				memcpy(currentrow + crf1[1].offset, nameptr(tab1->description), strlen(nameptr(tab1->description)));
		} while ((tab1->flags & (TABLE_FLAGS_TEMPLATE | TABLE_FLAGS_TEMPLATEDIR)) == TABLE_FLAGS_TEMPLATE && !fiofindnext(&found));
	}
	if (count) {
		worksetnum = allocworkset(1);
		if (worksetnum < 0) {
			memfree(hrows);
			memfree((UCHAR **) hcrf1);
			return worksetnum;
		}
		wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset;
		wks1->updatable = 'R';
		wks1->numcolrefs = wks1->rsnumcolumns = sizeof(fielddefs) / sizeof(*fielddefs);
		wks1->colrefarray = (HCOLREF) hcrf1;
		wks1->rsrowbytes = wks1->rowbytes = rowbytecount;
		wks1->rows = hrows;
		wks1->rowcount = wks1->memrowalloc = count;
		*rsid = worksetnum;
		return 6;
	}
	memfree((UCHAR **) hcrf1);
	return 4;  /* let driver build result set column information */
}

/**
 * execute a statement
 * return 0 if statement succeeded
 * return 1 if statement succeeded, w/ rsid = count
 * return 2 if statement succeeded, but data truncated
 * return 3 if statement succeeded, but data truncated, w/ rsid = count
 * return 4 if statement succeeded with empty result set
 * return 5 if statement succeeded with empty result set, w/ rsid = result id
 * return 6 if statement succeeded with result set, w/ rsid = result id
 * return 7 if statement succeeded with result set (plan required), w/ rsid = result id
 * return negative if error
 * return negative if error
 *
 * Called only from sqlexecute in fssql1
 */
INT execstmt(UCHAR *stmtstring, INT stmtsize, INT *rsid)
{
	INT rc;
	LONG sqlexeccode, sqlinfo;
	S_GENERIC **stmt;
	WORKSETINFO *wsinfo;
	HPLAN hplan;

	if (parsesqlstmt(stmtstring, stmtsize, &stmt)) {
		return RC_ERROR;
	}
	if ((*stmt)->stype == STYPE_SELECT) rc = buildselectplan((S_SELECT **) stmt, &hplan, FALSE);
	else if ((*stmt)->stype == STYPE_INSERT) rc = buildinsertplan((S_INSERT **) stmt, &hplan);
	else if ((*stmt)->stype == STYPE_UPDATE) rc = buildupdateplan((S_UPDATE **) stmt, &hplan);
	else if ((*stmt)->stype == STYPE_DELETE) rc = builddeleteplan((S_DELETE **) stmt, &hplan);
	else if ((*stmt)->stype == STYPE_LOCK) rc = buildlockplan((S_LOCK **) stmt, &hplan);
	else if ((*stmt)->stype == STYPE_UNLOCK) rc = buildunlockplan((S_LOCK **) stmt, &hplan);
	else if ((*stmt)->stype == STYPE_CREATE || (*stmt)->stype == STYPE_DROP || (*stmt)->stype == STYPE_ALTER) {
		freestmt(stmt);
		return 0;
	}
	else {
		rc = RC_ERROR;
	}
	freestmt(stmt);
	if (rc) return rc;

	sqlexeccode = 0;
	rc = execplan(hplan, &sqlexeccode, &sqlinfo, NULL);
	if (!rc) {
		rc = sqlexeccode;
		if (rc == 1 || rc == 3) *rsid = sqlinfo;
		else if (rc >= 5) {
			/* transfer information from plan to workset */
			*rsid = (*hplan)->worksetnum;
			wsinfo = *(*(*connection.hworksetarray + *rsid - 1));
			wsinfo->corrtable = (*hplan)->corrtable;
			(*hplan)->corrtable = NULL;
			if (rc == 7) {  /* except plan is needed for getting rows */
				/* transfer plan to workset */
				wsinfo->hplan = hplan;
				hplan = NULL;
			}
			else (*hplan)->worksetnum = 0;  /* prevent freeplan from discarding workset */
		}
	}
	freeplan(hplan);
	return rc;
}

/**
 * return negative if error; otherwise return 0 and data copied in length
 * Called only by sqlexecnone in fssql1
 */
INT execnonestmt(UCHAR *stmtstring, INT stmtsize, UCHAR *result, INT *length)
{
	INT i1, i2, i3, cnt, rc;
	CHAR *ptr;
	S_GENERIC **stmt;
	PLAN *plan;
	HPLAN hplan;

	if (*length < 16) {
		*length = 0;
		return 0;
	}

	if (scanselectstmt(stmtstring, stmtsize, &stmt)) {
		return -1;
	}
	rc = buildselectplan((S_SELECT **) stmt, &hplan, TRUE);
	freestmt(stmt);
	if (rc) {
		return rc;
	}

/*** NOTE: build result set here instead of fssql1.c to prevent allocating ***/
/***       and populating workset structure */
	/* parse out the information from the plan */
	lasttable[0] = lastalias[0] = 0;
	memcpy(result, "NONE R 0 ", 9);
	plan = *hplan;
	result[5] = (*hplan)->updatable;
	i1 = (*plan->worksetcolrefinfo)[0];
	i2 = (*plan->worksetcolrefinfo)[1];
	cnt = mscitoa(i2 - i1, (CHAR *)(result + 9)) + 9;
	for ( ; i1 < i2; i1++) {
		ptr = getcrfinfo(*plan->colrefarray + i1, plan->corrtable);
		i3 = (INT)strlen(ptr);
		if (cnt + i3 + 1 > *length) {
			*length = cnt;
			freeplan(hplan);
			return 0;
		}
		result[cnt++] = ' ';
		memcpy(result + cnt, ptr, i3);
		cnt += i3;
	}
	*length = cnt;
	freeplan(hplan);
	return 4;
}

/* get a row */
/* type: 1 = next, 2 = prev, 3 = first, 4 = last, 5 = absolute, 6 = relative */
/* return 0 if success */
/* return 1 if no more rows */
/* return negative if error */
INT getrow(INT rsid, INT type, LONG recnum, UCHAR *row, INT *rowsize)
{
	INT i1, rc;
	LONG sqlexeccode;
	WORKSET *ws1;
	WORKSETINFO *wsinfo;

	if (rsid <= 0 || rsid > connection.numworksets || (*connection.hworksetarray)[rsid - 1] == NULL) return sqlerrnum(SQLERR_BADRSID);
	wsinfo = *(*(*connection.hworksetarray + rsid - 1));
	if (wsinfo->hplan != NULL) {
		sqlexeccode = type;
		rc = execplan(wsinfo->hplan, &sqlexeccode, &recnum, NULL);
		if (!rc) {
			rc = sqlexeccode;
			if (rc >= 6) rc = 0;
			else rc = 1;  /* no more row available */
		}
		ws1 = (*(*(*connection.hworksetarray + rsid - 1)))->workset;
	}
	else {
		rc = 0;
		ws1 = (*(*(*connection.hworksetarray + rsid - 1)))->workset;
		switch (type) {
		case 1:  /* get next row */
			if (ws1->rowid <= ws1->rowcount) ws1->rowid++;
			break;
		case 2:  /* get previous row */
			if (ws1->rowid) ws1->rowid--;
			break;
		case 3:  /* get first row */
			ws1->rowid = 1;
			break;
		case 4:  /* get last row */
			ws1->rowid = ws1->rowcount;
			break;
		case 5:  /* get absolute row */
			if (recnum >= 0) ws1->rowid = (int) recnum;
			else if (recnum < 0) ws1->rowid = ws1->rowcount + 1 + (int) recnum;
			break;
		case 6:  /* get relative row */
			ws1->rowid += (int) recnum;
			break;
		default:
			rc = 1;
			break;
		}
		if (ws1->rowid < 0) ws1->rowid = 0;
		else if (ws1->rowid > ws1->rowcount) ws1->rowid = ws1->rowcount + 1;
	}
	if (!rc && ws1->rowid > 0 && ws1->rowid <= ws1->rowcount) {
		i1 = ws1->rsrowbytes;
		if (i1 > *rowsize) i1 = *rowsize;
		else *rowsize = i1;
		if (ws1->rowid <= ws1->memrowoffset || ws1->rowid > ws1->memrowoffset + ws1->memrowalloc) {
/*** CODE: SHOULD ANYTHING HAPPEN IF 0 IS RETURNED ***/
			if (readwks(rsid, 1) < 0) return sqlerrnum(SQLERR_EXEC_BADWORKFILE);
		}
		memcpy(row, *ws1->rows + (ws1->rowid - ws1->memrowoffset - 1) * ws1->rowbytes, i1);
	}
	else if (rc >= 0) {
		*rowsize = 0;
		rc = 1;
	}
	return rc;
}

INT posupdate(INT rsid, UCHAR *stmtstring, INT stmtsize)
{
	INT oldpgmcount, rc;
	LONG sqlexeccode, sqlinfo;
	S_GENERIC **genstmt;
	S_UPDATE **stmt;
	HPLAN hplan;
	WORKSET *ws1;
	WORKSETINFO **wsinfo;

	if (rsid <= 0 || rsid > connection.numworksets || (*connection.hworksetarray)[rsid - 1] == NULL) return sqlerrnum(SQLERR_BADRSID);
	wsinfo = *(*connection.hworksetarray + rsid - 1);
	if ((*wsinfo)->hplan != NULL) {
		hplan = (*wsinfo)->hplan;
		if (!((*hplan)->flags & PLAN_FLAG_FORUPDATE)) return sqlerrnum(SQLERR_PARSE_NOFORUPDATE);
		ws1 = (*wsinfo)->workset;
		if (ws1->rowid < 1) return sqlerrnum(SQLERR_BADROWID);
		if (parsesqlstmt(stmtstring, stmtsize, &genstmt)) return -1;
		if ((*genstmt)->stype != STYPE_UPDATE) {
			freestmt(genstmt);
			return sqlerrnum(SQLERR_BADCMD);
		}
		stmt = (S_UPDATE **) genstmt;
		if ((*stmt)->where != NULL) {
			freestmt(genstmt);
			return sqlerrnum(SQLERR_BADCMD);
		}
		oldpgmcount = (*hplan)->pgmcount;
		rc = appendupdateplan((S_UPDATE **) genstmt, hplan);
		if (rc) return rc;
		freestmt(genstmt);
		sqlexeccode = 8;  /* cause execution of update code */
		rc = execplan(hplan, &sqlexeccode, &sqlinfo, NULL);
		memchange((UCHAR **) (*hplan)->pgm, oldpgmcount * sizeof(PCODE), 0);
		(*(*hplan)->pgm)[oldpgmcount - 1].code = OP_RETURN;
		(*hplan)->pgmcount = oldpgmcount;
		if (!rc) {
#if 0
			if (sqlexeccode == 1 && sqlinfo == 1);
/*** CODE: THEN TRUE SUCCESS ***/
			else ;
/*** CODE: THEN NOT VALID PRIOR FETCH OR LAST OP WAS A UPDATE/DELETE ***/
#endif
			return sqlsuccess();
		}
		return sqlerrnum(rc);
	}
	else return sqlerrnum(SQLERR_PARSE_NOFORUPDATE);
}

/* delete positioned */
INT posdelete(INT rsid)
{
	INT rc;
	LONG sqlexeccode, sqlinfo;
	HPLAN hplan;
	WORKSET *ws1;
	WORKSETINFO **wsinfo;

	if (rsid <= 0 || rsid > connection.numworksets || (*connection.hworksetarray)[rsid - 1] == NULL) return sqlerrnum(SQLERR_BADRSID);
	wsinfo = *(*connection.hworksetarray + rsid - 1);
	if ((*wsinfo)->hplan != NULL) {
		hplan = (*wsinfo)->hplan;
		if (!((*hplan)->flags & PLAN_FLAG_FORUPDATE)) return sqlerrnum(SQLERR_PARSE_NOFORUPDATE);
		ws1 = (*wsinfo)->workset;
		if (ws1->rowid < 1) return sqlerrnum(SQLERR_BADROWID);
		sqlexeccode = 9;  /* cause execution of delete code */
		rc = execplan(hplan, &sqlexeccode, &sqlinfo, NULL);
		if (!rc) {
#if 0
			if (sqlexeccode == 1 && sqlinfo == 1);
/*** CODE: THEN TRUE SUCCESS ***/
			else ;
/*** CODE: THEN NOT VALID PRIOR FETCH OR LAST OP WAS A UPDATE/DELETE ***/
#endif
			return sqlsuccess();
		}
		return sqlerrnum(rc);
	}
	else return sqlerrnum(SQLERR_PARSE_NOFORUPDATE);
}

/* return the number of columns in a result set */
/* return negative if error */
INT getrsupdatable(INT rsid, CHAR *updatable)
{
	WORKSET *ws1;

	if (rsid <= 0 || rsid > connection.numworksets || (*connection.hworksetarray)[rsid - 1] == NULL) return sqlerrnum(SQLERR_BADRSID);
	ws1 = (*(*(*connection.hworksetarray + rsid - 1)))->workset;
	*updatable = ws1->updatable;
	return 0;
}

/* return the number of rows in a result set */
/* return negative if error */
/*** CODE: SHOULD THIS BE OFFSET. IF SO DO WE NEED ARGS TO EXECPLAN TO BE OFFSET ***/
INT getrsrowcount(INT rsid, LONG *rowcount)
{
	INT rc;
	LONG sqlexeccode;
	WORKSET *ws1;
	WORKSETINFO *wsinfo;

	if (rsid <= 0 || rsid > connection.numworksets || (*connection.hworksetarray)[rsid - 1] == NULL) return sqlerrnum(SQLERR_BADRSID);
	wsinfo = *(*(*connection.hworksetarray + rsid - 1));
	if (wsinfo->hplan != NULL) {
		sqlexeccode = 7;
		rc = execplan(wsinfo->hplan, &sqlexeccode, rowcount, NULL);
		if (rc) return rc;
		if (sqlexeccode == -1) return sqlerrnum(SQLERR_EXEC_BADPGM);
	}
	else {
		ws1 = (*(*(*connection.hworksetarray + rsid - 1)))->workset;
		*rowcount = ws1->rowcount;
	}
	return 0;
}

/* return the number of columns in a result set */
/* return negative if error */
INT getrscolcount(INT rsid)
{
	WORKSET *ws1;

	if (rsid <= 0 || rsid > connection.numworksets || (*connection.hworksetarray)[rsid - 1] == NULL) return sqlerrnum(SQLERR_BADRSID);
	ws1 = (*(*(*connection.hworksetarray + rsid - 1)))->workset;
	return ws1->rsnumcolumns;
}

CHAR *getrscolinfo(INT rsid, INT column)
{
	WORKSET *ws1;
	WORKSETINFO *wsinfo;

	if (rsid <= 0 || rsid > connection.numworksets || (*connection.hworksetarray)[rsid - 1] == NULL) {
		sqlerrnum(SQLERR_BADRSID);
		return "";
	}
	wsinfo = *(*(*connection.hworksetarray + rsid - 1));
	ws1 = wsinfo->workset;
	if (column < 1 || column > ws1->rsnumcolumns) {
		sqlerrnummsg(SQLERR_EXEC_BADPARM, "invalid column number", NULL);
		return "";
	}
	if (column == 1) lasttable[0] = lastalias[0] = 0;
	return getcrfinfo(*ws1->colrefarray + column - 1, wsinfo->corrtable);
}

INT discard(INT rsid)
{
	if (rsid <= 0 || rsid > connection.numworksets || (*connection.hworksetarray)[rsid - 1] == NULL) return sqlerrnum(SQLERR_BADRSID);
	freeworkset(rsid);
	return 0;
}

INT allocworkset(INT count)
{
	INT i1;
	WORKSETINFO **wsinfo;

	for (i1 = 0; i1 < connection.numworksets && (*connection.hworksetarray)[i1] != NULL; i1++);
	if (i1 == connection.numworksets) {
		if (!connection.numworksets) {
			connection.hworksetarray = (HWORKSETINFO **) memalloc(20 * sizeof(HWORKSETINFO), MEMFLAGS_ZEROFILL);
			if (connection.hworksetarray == NULL) return sqlerrnum(SQLERR_NOMEM);
			connection.numworksets = 20;
		}
		else {
			if (memchange((UCHAR **) connection.hworksetarray, (connection.numworksets + 20) * sizeof(HWORKSETINFO), MEMFLAGS_ZEROFILL))
				return sqlerrnum(SQLERR_NOMEM);
			connection.numworksets += 20;
		}
	}
	wsinfo = (HWORKSETINFO) memalloc(sizeof(WORKSETINFO) + (count - 1) * sizeof(WORKSET), MEMFLAGS_ZEROFILL);
	if (wsinfo == NULL) return sqlerrnum(SQLERR_NOMEM);
	(*connection.hworksetarray)[i1] = wsinfo;
	(*wsinfo)->count = count;
	return i1 + 1;
}

void freeworkset(INT worksetnum)
{
	INT i1;
	WORKSET *ws1;
	WORKSETINFO **wsinfo;

	if (worksetnum <= 0 || worksetnum > connection.numworksets || (*connection.hworksetarray)[worksetnum - 1] == NULL) return;
	wsinfo = *(*connection.hworksetarray + worksetnum - 1);
	for (i1 = 0; i1 < (*wsinfo)->count; i1++) {
		ws1 = (*wsinfo)->workset + i1;
		if (ws1->colrefarray != NULL) memfree((UCHAR **) ws1->colrefarray);
		if (ws1->rows != NULL) memfree(ws1->rows);
		if (ws1->fileposarray != NULL) memfree((UCHAR **) ws1->fileposarray);
		if (ws1->fiofilenum) fiokill(ws1->fiofilenum);
	}
	if ((*wsinfo)->hplan != NULL) {
		(*(*wsinfo)->hplan)->worksetnum = 0;  /* prevent callback to freeworkset */
		freeplan((*wsinfo)->hplan);
	}
	memfree((UCHAR **)(*wsinfo)->corrtable);
	memfree((UCHAR **) wsinfo);
	*((*connection.hworksetarray) + worksetnum - 1) = NULL;
}

INT maketemplate(CHAR *tablename, INT *tablenum)
{
	INT i1, i2, numindexes;
	CHAR filename[MAX_NAMESIZE + 1];
	TABLE *tab1, *tab2;
	HINDEX hindex1, hindex2;

	/* search for matching template name */
	for (i1 = connection.numtables; ++i1 <= connection.numalltables; ) {
		tab1 = tableptr(i1);
		if (!strcmp(tablename, nameptr(tab1->name))) {
			*tablenum = i1;
			return 0;
		}
	}

	/* create new table and fixup names */
	if (memchange((UCHAR **) connection.htablearray, (connection.numalltables + 1) * sizeof(TABLE), 0) < 0)
		return sqlerrnum(SQLERR_NOMEM);
	tab1 = tableptr(*tablenum);
	tab2 = tableptr(connection.numalltables + 1);
	*tab2 = *tab1;
	i1 = cfgaddname(tablename, &connection.nametable, &connection.nametablesize, &connection.nametablealloc);
	if (i1 == -1) return sqlerrnum(SQLERR_NOMEM);
	tab2 = tableptr(connection.numalltables + 1);
	tab2->name = i1;
	tab1 = tableptr(*tablenum);
	replacename(nameptr(tab1->name), tablename, nameptr(tab2->textfilename), filename);
	i1 = cfgaddname(filename, &connection.nametable, &connection.nametablesize, &connection.nametablealloc);
	if (i1 == -1) return sqlerrnum(SQLERR_NOMEM);
	tab2 = tableptr(connection.numalltables + 1);
	tab2->textfilename = i1;
	numindexes = tab2->numindexes;
	hindex1 = tab2->hindexarray;
	if (numindexes) {
		hindex2 = (HINDEX) memalloc(numindexes * sizeof(INDEX), MEMFLAGS_ZEROFILL);
		if (hindex2 == NULL) return sqlerrnum(SQLERR_NOMEM);
		for (i1 = 0; i1 < numindexes; i1++) {
			tab1 = tableptr(*tablenum);
			replacename(nameptr(tab1->name), tablename, nameptr((*hindex1)[i1].indexfilename), filename);
			i2 = cfgaddname(filename, &connection.nametable, &connection.nametablesize, &connection.nametablealloc);
			if (i2 == -1) {
				memfree((UCHAR **) hindex2);
				return sqlerrnum(SQLERR_NOMEM);
			}
			(*hindex2)[i1].indexfilename = i2;
		}
	}
	else hindex2 = NULL;
	connection.numalltables++;
	tab2 = tableptr(connection.numalltables);
	tab2->hindexarray = hindex2;
	*tablenum = connection.numalltables;
	return 0;
}

/* fill in rest of TABLE, COLUMN, etc. information  */
/* return 0 if successful, otherwise return ERR_ number */
INT maketablecomplete(INT tablenum)
{
	INT i1, i2, alternateflag, asciisortflag, cnt, colnum, filenum, indexnum;
	INT keycnt, keylen, keyoff, keypos, numkeys, version, trace[MAX_IDXKEYS];
	CHAR filename[MAX_NAMESIZE + 1], *ptr1;
	UCHAR c1, blk[1025], **pptr;
	TABLE *tab1;
	COLUMN *col1;
	INDEX *idx1;
	IDXKEY keys[MAX_IDXKEYS];
	HIDXKEY hkeys;
	COLKEY colkeywork, colkeys[MAX_COLKEYS];
	HCOLKEY hcolkeys;

	tab1 = tableptr(tablenum);
	for (indexnum = 0; ++indexnum <= tab1->numindexes; ) {
		idx1 = indexptr(tab1, indexnum);
		strcpy(filename, nameptr(idx1->indexfilename));
		filenum = fioopen(filename, (tab1->rioopenflags & FIO_M_MASK) | FIO_P_TXT);
		if (filenum < 1) return sqlerrnummsg(filenum, "error in opening index file", filename);
		i1 = fioread(filenum, 0, blk, 1024);
		fioclose(filenum);
		if (i1 < 512) {
			if (i1 >= 0) return sqlerrnummsg(SQLERR_BADINDEX, "index was smaller than minimum index size of 512 bytes", filename);
			else return sqlerrnummsg(i1, "error in reading index file", filename);
		}
		tab1 = tableptr(tablenum);
		idx1 = indexptr(tab1, indexnum);
		if (blk[0] == 'I') {
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
			if (blk[100] == DBCEOR && c1 != DBCDEL) {
				idx1->type = INDEX_TYPE_ISAM;
				if (c1 == 'F' || c1 == 'S') {
					if ((tab1->rioopenflags & (RIO_FIX | RIO_UNC)) != (RIO_FIX | RIO_UNC))
						return sqlerrnummsg(SQLERR_BADINDEX, "index has space reclaimation, but text file is compressed or variable", filename);
					if (!tab1->reclaimindex) tab1->reclaimindex = indexnum;
				}
				idx1->keylength = blk[59] - '0';
				if (blk[58] != ' ') {
					if (isdigit(blk[58])) idx1->keylength += (blk[58] - '0') * 10;
					else idx1->keylength += (blk[58] - 'A' + 10) * 10;
				}
				if (blk[56] == 'D') idx1->flags |= INDEX_DUPS;
				pptr = fiogetopt(FIO_OPT_COLLATEMAP);
				if (pptr == NULL || (*pptr)['A'] != (*pptr)['a']) idx1->flags |= INDEX_DISTINCT;
			}
		}
		else if (blk[0] == 'A') {
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
			if (blk[100] == DBCEOR && (blk[57] == 'Y' || blk[57] == 'N') && c1 != DBCDEL) {
				idx1->type = INDEX_TYPE_AIM;
				if (c1 == 'F' || c1 == 'S') {
					if ((tab1->rioopenflags & (RIO_FIX | RIO_UNC)) != (RIO_FIX | RIO_UNC))
						return sqlerrnummsg(SQLERR_BADINDEX, "aimdex specifies fixed records, but text file is compressed or variable", filename);
					if (c1 == 'S' && !tab1->reclaimindex) tab1->reclaimindex = indexnum;
					tab1->flags |= TABLE_FLAGS_EORSIZE;
					idx1->flags |= INDEX_FIXED;
				}
				if (blk[57] == 'Y') idx1->flags |= INDEX_DISTINCT;
			}
		}
		if (!idx1->type) return sqlerrnummsg(SQLERR_BADINDEX, "index not valid index or aimdex file", filename);
#ifdef __MACOSX
		for (ptr1 = (CHAR*)(blk + 101); ptr1 != NULL; ptr1++) if ((UCHAR)*ptr1 == DBCEOR) break;
#else
		ptr1 = strchr((CHAR *)(blk + 101), DBCEOR);
#endif
		ptr1++;
		blk[i1] = DBCDEL;
		for (numkeys = 0; ((UCHAR) *ptr1) != DBCDEL; ptr1++) {
			if (isdigit(*ptr1)) {
				if (numkeys == MAX_IDXKEYS) return sqlerrnummsg(SQLERR_INTERNAL, "too many index keys", NULL);
				keys[numkeys].pos = (int) strtol(ptr1, &ptr1, 10) - 1;
				if (*ptr1 == '-') {
					ptr1++;
					i1 = (int) strtol(ptr1, &ptr1, 10);
					keys[numkeys].len = i1 - keys[numkeys].pos;
				}
				else keys[numkeys].len = 1;
				if (idx1->type == INDEX_TYPE_ISAM) {
					if (numkeys && keys[numkeys].pos == keys[numkeys - 1].pos + keys[numkeys - 1].len) {
						keys[numkeys - 1].len += keys[numkeys].len;
						continue;
					}
				}
				else if (idx1->type == INDEX_TYPE_AIM) {
					if (toupper(*ptr1) == 'X') {
/*** CODE: IGNORE FOR NOW, COULD JUST PREVENT IT FROM BEING USED IN A LOOKUP ALONE ***/
						ptr1++;
						continue;
					}
				}
				numkeys++;
			}
			else {
#ifdef __MACOSX
				for (; ptr1 != NULL; ptr1++) if ((UCHAR)*ptr1 == DBCEOR) break;
#else
				ptr1 = strchr(ptr1, DBCEOR);
#endif
			}
		}
		/* build the column key table */
		cnt = 0;
		if (numkeys) {
			hkeys = (HIDXKEY) memalloc(numkeys * sizeof(IDXKEY), 0);
			if (hkeys == NULL) return sqlerrnum(SQLERR_NOMEM);
			tab1 = tableptr(tablenum);
			idx1 = indexptr(tab1, indexnum);
			idx1->hkeys = hkeys;
			memcpy(*hkeys, keys, numkeys * sizeof(IDXKEY));
			keycnt = 0;
			keypos = keys[0].pos;
			keylen = keys[0].len;
			keyoff = 0;
			alternateflag = FALSE;
			for (i1 = 0, colnum = 1; ; colnum++) {
				if (colnum > tab1->numcolumns) {
					/* backing out along trace route */
					while (i1 && colkeys[trace[--i1]].alt != -1) {
						if (idx1->type == INDEX_TYPE_ISAM) {
							/* compare key against alternate key and place longest (better) first */
							i2 = colkeys[trace[i1]].alt;
							if (colkeys[trace[i1]].keylen < colkeys[i2].keylen) {
								/* swap as alternate key is longer */
								colkeys[trace[i1]].alt = colkeys[i2].alt;
								colkeys[i2].alt = trace[i1];
								if (i1) colkeys[trace[i1 - 1]].alt = i2;
								else {  /* swap the structures */
									colkeywork = colkeys[trace[i1]];
									colkeys[trace[i1]] = colkeys[i2];
									colkeys[i2] = colkeywork;
								}
							}
						}
					}
					if (!keyoff) {
						if (!keycnt) break;
						keypos = keys[--keycnt].pos;
						keyoff = keylen = keys[keycnt].len;
					}
					keyoff -= colkeys[trace[i1]].len;
					if (colkeys[trace[i1]].next != -1) colkeys[trace[i1]].keylen += colkeys[colkeys[trace[i1]].next].keylen;
					colnum = colkeys[trace[i1]].colnum;
					alternateflag = TRUE;
					continue;
				}
				col1 = columnptr(tab1, colnum);
				if ((INT) col1->offset != keypos + keyoff) {
#if 0
/*** CODE: IF DATE/TIME/TIMESTAMP AND KEYPOS + KEYOFF > COL1->OFFSET && ***/
/***       KEYPOS + KEYOFF < COL1->OFFSET + COL1->FIELDLEN, WE COULD TRY ***/
/***       TO SEE IF OUT OF ORDER DATE/TIME/TIMESTAMP MATCHES KEY. PROBLEM ***/
/***       IS THAT MULTIPLE KEYS COULD BE ASSOCIATED WITH ONE COLUMN AND ***/
/***       INTERNALLY, THE CODE DOES NOT SUPPORT THAT ***/
#endif
					continue;
				}
				asciisortflag = FALSE;
				if (col1->type == TYPE_CHAR || col1->type == TYPE_POSNUM) asciisortflag = TRUE;
				else if (col1->type == TYPE_DATE || col1->type == TYPE_TIME || col1->type == TYPE_TIMESTAMP) {
					ptr1 = nameptr(col1->format);
					for (c1 = 0, i2 = 0; ptr1[i2]; i2++) {
						if (ptr1[i2] < ' ') {
							if (ptr1[i2] <= c1 || ptr1[i2] == FORMAT_DATE_YEAR2) break;
							c1 = ptr1[i2];
						}
					}
					if (!ptr1[i2]) asciisortflag = TRUE;
				}
				if (alternateflag) colkeys[trace[i1++]].alt = cnt;
				else if (i1 > 0) colkeys[trace[i1 - 1]].next = cnt;
				if (i1 == MAX_IDXKEYS) return sqlerrnummsg(SQLERR_INTERNAL, "too many index key columns", NULL);
				if (cnt == MAX_COLKEYS) return sqlerrnummsg(SQLERR_INTERNAL, "too many index key combinations", NULL);
				trace[i1++] = cnt;
				colkeys[cnt].next = colkeys[cnt].alt = -1;
				colkeys[cnt].colnum = colnum;
				i2 = col1->fieldlen;
				if (i2 > keylen - keyoff) i2 = keylen - keyoff;
				colkeys[cnt].keylen = colkeys[cnt].len = i2;
				colkeys[cnt++].asciisortflag = asciisortflag;
				if ((keyoff += i2) == keylen) {
					keyoff = 0;
					if (++keycnt == numkeys) {
						colnum = tab1->numcolumns;
						continue;
					}
					keypos = keys[keycnt].pos;
					keylen = keys[keycnt].len;
				}
				colnum = 0;
				alternateflag = FALSE;
			}
			if (cnt) {
				hcolkeys = (HCOLKEY) memalloc(cnt * sizeof(COLKEY), 0);
				if (hcolkeys == NULL) return sqlerrnum(SQLERR_NOMEM);
				tab1 = tableptr(tablenum);
				idx1 = indexptr(tab1, indexnum);
				idx1->hcolkeys = hcolkeys;
				memcpy(*hcolkeys, colkeys, cnt * sizeof(COLKEY));
			}
		}
		else tab1->flags |= TABLE_FLAGS_READONLY;
		idx1->numkeys = numkeys;
		idx1->numcolkeys = cnt;
	}
	tab1->flags |= TABLE_FLAGS_COMPLETE;
	return 0;
}

/* open the text file of a table */
/* return the OPENFILE number if successful */
/* otherwise return ERR_ */
INT opentabletextfile(INT tablenum)
{
	INT i1, emptyfilenum, eorsize, filenum, filetype, geteorsizeflag, getrecsizeflag;
	INT lrufilenum, numindexes, openfilenum, openflags, recsize, **hint;
	USHORT lowestlru;
	OFFSET pos;
	CHAR filename[MAX_NAMESIZE + 1];
	UCHAR **hmem;
	TABLE *tab1;
	OPENFILE *opf1;

	tab1 = tableptr(tablenum);
	if (!(tab1->flags & TABLE_FLAGS_COMPLETE)) {
		i1 = maketablecomplete(tablenum);
		if (i1) return i1;
		tab1 = tableptr(tablenum);
	}
	/* first try the one last used by this table */
	if (tab1->lastopenfilenum >= 1 && tab1->lastopenfilenum <= connection.numopenfiles) {
		openfilenum = tab1->lastopenfilenum;
		opf1 = openfileptr(openfilenum);
		if (!opf1->inuseflag && opf1->tablenum == tablenum) goto textfileisopen;
	}
	/* next look for an already open but not in use entry */
	lowestlru = lrucount;
/*** CODE: CAN PREVENT WASTEFUL LOOPING BY HAVING A HIGHEST USE COUNT ***/
	for (openfilenum = emptyfilenum = lrufilenum = 0; ++openfilenum <= connection.numopenfiles; ) {
		opf1 = openfileptr(openfilenum);
		if (!opf1->inuseflag) {
			if (opf1->tablenum == tablenum) goto textfileisopen;
			if (!opf1->tablenum) {
				if (!emptyfilenum) emptyfilenum = openfilenum;
			}
			else if (opf1->lrucount < lowestlru) {
				lowestlru = opf1->lrucount;
				lrufilenum = openfilenum;
			}
		}
	}
	if (!emptyfilenum) {  /* no empty entries, make more */
		if (!lrufilenum) {
			if (memchange((UCHAR **) connection.hopenfilearray, (connection.numopenfiles + 32) * sizeof(OPENFILE), MEMFLAGS_ZEROFILL)) return sqlerrnum(SQLERR_NOMEM);
			connection.numopenfiles += 32;
		}
		else {
			closetablefiles(lrufilenum);
			openfilenum = lrufilenum;
		}
	}
	else openfilenum = emptyfilenum;
	tab1 = tableptr(tablenum);
	numindexes = tab1->numindexes;
	recsize = tab1->reclength;
	if (recsize < 0) {  /* fixed length size not specified */
		recsize = -recsize;
		getrecsizeflag = TRUE;
	}
	else getrecsizeflag = FALSE;
	geteorsizeflag = tab1->flags & TABLE_FLAGS_EORSIZE;
	strcpy(filename, nameptr(tab1->textfilename));

	openflags = tab1->rioopenflags;
	if ((fsflags & FSFLAGS_LOGFILE) && cfglogname(filename)) openflags |= RIO_LOG;
	filenum = rioopen(filename, openflags, 0, recsize);
	if (filenum < 0) return sqlerrnummsg(filenum, "error in opening text file (A)", filename);
	if (geteorsizeflag) {
		eorsize = rioeorsize(filenum);
		if (eorsize < 0) {
			if (eorsize != ERR_BADTP) {
				rioclose(filenum);
				return sqlerrnummsg(eorsize, filename, NULL);
			}
#if OS_WIN32
			eorsize = 2;  /* default to MSDOS text type */
#else
			eorsize = 1;  /* default to UNIX text type */
#endif
		}
		else {
			tab1 = tableptr(tablenum);
			tab1->flags &= ~TABLE_FLAGS_EORSIZE;
			tab1->eorsize = (UCHAR) eorsize;
			geteorsizeflag = FALSE;
		}
	}
	hmem = memalloc(recsize + 4, 0);
	if (hmem == NULL) return sqlerrnum(SQLERR_NOMEM);
	if (getrecsizeflag || geteorsizeflag || riotype(filenum) == RIO_T_ANY) {
		for (pos = -1; ; ) {
			while ((i1 = rioget(filenum, *hmem, recsize)) == -2);
			if (i1 == -1) break;  /* EOF */
			if (i1 >= 0) {  /* found record */
				if (geteorsizeflag) {
					eorsize = rioeorsize(filenum);
					if (eorsize < 0) {
						memfree(hmem);
						rioclose(filenum);
						return sqlerrnummsg(eorsize, filename, NULL);
					}
				}
				if ((openflags & RIO_T_MASK) == RIO_T_ANY) {
					filetype = riotype(filenum);
					if (filetype >= 0) {
						openflags &= ~RIO_T_MASK;
						openflags |= filetype;
					}
				}
				if (pos != -1) {  /* reopen to get the RIO_FIX set */
					if (getrecsizeflag) {
						recsize = i1;
						if (memchange(hmem, recsize + 4, 0) < 0) {
							memfree(hmem);
							rioclose(filenum);
							return sqlerrnum(SQLERR_NOMEM);
						}
					}
					rioclose(filenum);
					filenum = rioopen(filename, openflags, 0, recsize);
					if (filenum < 0) {
						memfree(hmem);
						return sqlerrnummsg(filenum, "error in opening text file (B)", filename);
					}
				}
				/* else the table matches the record length */
				break;
			}
			/* added test for ERR_SHORT in case file starts with deleted record and table is shorter than record */
			if (!getrecsizeflag || (i1 != ERR_NOEOR && i1 != ERR_SHORT) || recsize == RIO_MAX_RECSIZE) {
				memfree(hmem);
				rioclose(filenum);
#if 0
/*** CODE: NEED ERROR FOR FIXED LENGTH BUT TABLE IS LONGER THAN RECORD ***/
				if (i1 == ERR_SHORT) return sqlerrnummsg(SQLERR_????, filename, NULL);
				else
#endif
				return sqlerrnummsg(i1, "error in reading text file", filename);
			}
			if (getrecsizeflag) {
				recsize += 4096;
				if (recsize > RIO_MAX_RECSIZE) recsize = RIO_MAX_RECSIZE;
				if (memchange(hmem, recsize + 4, 0) < 0) {
					memfree(hmem);
					rioclose(filenum);
					return sqlerrnum(SQLERR_NOMEM);
				}
			}
			riolastpos(filenum, &pos);
			rioclose(filenum);
			filenum = rioopen(filename, openflags & ~(RIO_FIX | RIO_LOG), 0, recsize);
			if (filenum < 0) {
				memfree(hmem);
				return sqlerrnummsg(filenum, "error in opening text file (C)", filename);
			}
			riosetpos(filenum, pos);
		}
		tab1 = tableptr(tablenum);
		if (getrecsizeflag) tab1->reclength = recsize;
		if (geteorsizeflag) {
			tab1->flags &= ~TABLE_FLAGS_EORSIZE;
			tab1->eorsize = (UCHAR) eorsize;
		}
		tab1->rioopenflags = openflags & ~RIO_LOG;  /* in case type was determined */
	}
	else tab1 = tableptr(tablenum);

	hint = (int **) memalloc(numindexes * sizeof(int), MEMFLAGS_ZEROFILL);
	if (hint == NULL) return sqlerrnum(SQLERR_NOMEM);
	tab1 = tableptr(tablenum);
	opf1 = openfileptr(openfilenum);
	opf1->textfilenum = filenum;
	opf1->hrecbuf = hmem;
	opf1->hopenindexarray = hint;
	opf1->tablenum = tablenum;
	opf1->reclength = recsize;
textfileisopen:
	tab1->lastopenfilenum = openfilenum;
	opf1->inuseflag = TRUE;
	opf1->lrucount = lrucount++;
	if (lrucount == 0x8000) {
		lrucount >>= 1;
		for (i1 = 0; ++i1 <= connection.numopenfiles; ) {
			opf1 = openfileptr(openfilenum);
			opf1->lrucount >>= 1;
		}
	}
	return openfilenum;
}

/* open the index file of a table */
/* return 0 if successful */
/* otherwise return ERR_ */
INT opentableindexfile(INT openfilenum, INT indexnum)
{
	INT i1, filenum, fixflag, opts;
	CHAR filename[MAX_NAMESIZE];
	CHAR textname[MAX_NAMESIZE];
	TABLE *tab1;
	INDEX *idx1;
	OPENFILE *opf1;

	if (openfilenum < 1 || openfilenum > connection.numopenfiles) return sqlerrnum(SQLERR_INTERNAL);
	opf1 = openfileptr(openfilenum);
	if ((*opf1->hopenindexarray)[indexnum - 1]) return 0;  /* already open */
	tab1 = tableptr(opf1->tablenum);
	idx1 = indexptr(tab1, indexnum);
	opts = tab1->rioopenflags & FIO_M_MASK;
	strcpy(filename, nameptr(idx1->indexfilename));

	if (idx1->type == INDEX_TYPE_ISAM) {
		if ((tab1->rioopenflags & (RIO_FIX | RIO_UNC)) == (RIO_FIX | RIO_UNC)) opts |= XIO_FIX | XIO_UNC;
		filenum = xioopen(filename, opts | XIO_P_TXT, 0, tab1->reclength, &filepos, textname);
	}
	else if (idx1->type == INDEX_TYPE_AIM) {
		if ((tab1->rioopenflags & (RIO_FIX | RIO_UNC)) == (RIO_FIX | RIO_UNC)) opts |= AIO_FIX | AIO_UNC;
		filenum = aioopen(filename, opts | AIO_P_TXT, tab1->reclength, &filepos, textname, &fixflag, textname, 0, '?');
		tab1 = tableptr(opf1->tablenum);
		idx1 = indexptr(tab1, indexnum);
	}
	else return sqlerrnummsg(SQLERR_INTERNAL, "file type was not index or aim", NULL);
	if (filenum <= 0) return sqlerrnummsg(filenum, "error in opening index file", filename);
	opf1 = openfileptr(openfilenum);
	(*opf1->hopenindexarray)[indexnum - 1] = filenum;
	opf1->lrucount = lrucount++;
	if (lrucount == 0x8000) {
		lrucount >>= 1;
		for (i1 = 0; ++i1 <= connection.numopenfiles; ) {
			opf1 = openfileptr(openfilenum);
			opf1->lrucount >>= 1;
		}
	}
	return 0;
}

void closefilesinalltables()
{
	INT i1;

	for (i1 = 0; ++i1 <= connection.numopenfiles; ) closetablefiles(i1);
}

INT locktextfile(INT openfilenum)
{
	INT i1, filenum, lockfilenum, handles[2];
	CHAR textname[MAX_NAMESIZE];
	CHAR *filename, *ptr;
	OPENFILE *opf1;

	if (openfilenum < 1 || openfilenum > connection.numopenfiles) return sqlerrnum(SQLERR_INTERNAL);
	opf1 = openfileptr(openfilenum);
	filename = fioname(opf1->textfilenum);
	if (filename == NULL) return sqlerrnum(SQLERR_INTERNAL);
	for (i1 = lockfilenum = connection.numlockfiles; --i1 >= 0; ) {
		if (!(*connection.hlockfilearray)[i1].textfilenum) {
			lockfilenum = i1;
			continue;
		}
		ptr = fioname((*connection.hlockfilearray)[i1].textfilenum);
		if (ptr == NULL) return sqlerrnum(SQLERR_INTERNAL);
		if (!strcmp(ptr, filename)) {
			(*connection.hlockfilearray)[i1].lockcount++;
			return i1 + 1;
		}
	}
	strcpy(textname, filename);
	if (lockfilenum == connection.numlockfiles) {
		if (!lockfilenum) {
			connection.hlockfilearray = (HLOCKFILE) memalloc(4 * sizeof(LOCKFILE), MEMFLAGS_ZEROFILL);
			if (connection.hlockfilearray == NULL) return sqlerrnum(SQLERR_NOMEM);
		}
		else if (memchange((UCHAR **) connection.hlockfilearray, (connection.numlockfiles + 4) * sizeof(LOCKFILE), MEMFLAGS_ZEROFILL)) return sqlerrnum(SQLERR_NOMEM);
		connection.numlockfiles += 4;
	}
	filenum = fioopen(textname, FIO_M_SHR | FIO_P_WRK);
	if (filenum < 0) {
		return sqlerrnummsg(filenum, "error in opening text file (D)", textname);
	}
	handles[0] = filenum;
	handles[1] = 0;
	i1 = fiolock(handles);
	if (i1 < 0) return sqlerrnummsg(i1, "error in locking table", textname);
	(*connection.hlockfilearray)[lockfilenum].textfilenum = filenum;
	(*connection.hlockfilearray)[lockfilenum].lockcount = 1;
	return lockfilenum + 1;
}

INT unlocktextfile(INT lockfilenum)
{
	if (lockfilenum < 1 || lockfilenum > connection.numlockfiles) return sqlerrnum(SQLERR_INTERNAL);
	if ((*connection.hlockfilearray)[--lockfilenum].lockcount) {
		if (!--(*connection.hlockfilearray)[lockfilenum].lockcount) {
			fiounlock((*connection.hlockfilearray)[lockfilenum].textfilenum);
			fioclose((*connection.hlockfilearray)[lockfilenum].textfilenum);
			(*connection.hlockfilearray)[lockfilenum].textfilenum = 0;
		}
	}
	return 0;
}

void closetablelocks()
{
	INT i1;

	for (i1 = 0; i1 < connection.numlockfiles; i1++) {
		if ((*connection.hlockfilearray)[i1].textfilenum) {
			if ((*connection.hlockfilearray)[i1].lockcount) {
				fiounlock((*connection.hlockfilearray)[i1].textfilenum);
				(*connection.hlockfilearray)[i1].lockcount = 0;
			}
			fioclose((*connection.hlockfilearray)[i1].textfilenum);
			(*connection.hlockfilearray)[i1].textfilenum = 0;
		}
	}
}

static CHAR *getcrfinfo(COLREF *crf1, HCORRTABLE corrtable)
{
	static char workname[256];
	INT i1, i2;
	CHAR datatype, *ptr;
	TABLE *tab1;
	COLUMN *col1;

	i1 = 0;
	if (crf1->columnnum) {  /* column name */
		tab1 = tableptr(crf1->tablenum);
		col1 = columnptr(tab1, crf1->columnnum);
		strcpy(workname, nameptr(col1->name));
		i1 = (INT)strlen(workname);
	}
	workname[i1++] = '(';
	/* column type */
	switch (crf1->type) {
		case TYPE_NUM:
		case TYPE_POSNUM:
			datatype = 'N';
			break;
		case TYPE_DATE:
			datatype = 'D';
			break;
		case TYPE_TIME:
			datatype = 'T';
			break;
		case TYPE_TIMESTAMP:
			datatype = 'S';
			break;
		case TYPE_CHAR:
		default:
			datatype = 'C';
			break;
	}
	workname[i1++] = datatype;
	/* column size */
	if (datatype == 'C') i1 += mscitoa(crf1->length, workname + i1);
	else if (datatype == 'N') {
		i2 = crf1->length;
		if (!crf1->scale) i1 += mscitoa(i2, workname + i1);
		else {
			i1 += mscitoa(i2 - 1, workname + i1);
			workname[i1++] = ':';
			i1 += mscitoa(crf1->scale, workname + i1);
		}
	}
	workname[i1++] = ',';
	if (corrtable != NULL) {  /* column alias */
		for (i2 = 0; i2 < (*corrtable)->count; i2++) {
			if (crf1->tabcolnum == (*corrtable)->info[i2].tabcolnum) {
				strcpy(workname + i1, (*corrtable)->info[i2].name);
				i1 += (INT)strlen(workname + i1);
				break;
			}
		}
	}
	workname[i1++] = ',';
	ptr = "";
	if (crf1->columnnum) {  /* column label */
		ptr = nameptr(col1->label);
		for (i2 = 0; ptr[i2]; i2++) {  /* escape labels that have ')' and ',' */
			if (ptr[i2] == ',' || ptr[i2] == ')' || ptr[i2] == '\\') workname[i1++] = '\\';
			workname[i1++] = ptr[i2];
		}
		ptr = nameptr(tab1->name);
	}
	workname[i1++] = ',';
	if (strcmp(lasttable, ptr)) {  /* table name */
		strcpy(lasttable, ptr);
		if (*ptr) {
			strcpy(workname + i1, ptr);
			i1 += (INT)strlen(workname + i1);
		}
		else workname[i1++] = '*';
	}
	workname[i1++] = ',';
	/* table alias */
	ptr = "";
	if (corrtable != NULL && crf1->tablenum) {
		for (i2 = 0; i2 < (*corrtable)->count; i2++) {
			if (crf1->tablenum == (*corrtable)->info[i2].tablenum) {
				ptr = (*corrtable)->info[i2].name;
				break;
			}
		}
	}
	if (strcmp(lastalias, ptr)) {
		strcpy(lastalias, ptr);
		if (*ptr) {
			strcpy(workname + i1, ptr);
			i1 += (INT)strlen(workname + i1);
		}
		else workname[i1++] = '*';
	}
/*** CODE: SHOULD WE RETURN NULLABLE ??, IE EXPRESSIONS  ***/
	/* remove any empty fields */
	while (workname[i1 - 1] == ',') i1--;
	workname[i1++] = ')';
	workname[i1++] = '\0';
	return workname;
}

static INT matchname(CHAR *name, CHAR *pattern, INT templateflag)
{
	INT i1, i2;
	CHAR templatechar, workname[MAX_NAME_LENGTH + 1];

	if (templateflag) {
		strcpy(workname, name);
		templatechar = '?';
	}
	else templatechar = '\0';
	for (i1 = i2 = 0; pattern[i1]; i2++, i1++) {
		if (pattern[i1] == '%') {
			while (pattern[++i1] == '%');
			if (!pattern[i1]) {
				while (name[i2] && (templateflag != TEMPLATE_EXACT || name[i2] != templatechar)) i2++;
				break;
			}
			while (name[i2]) {
				if ((pattern[i1] == '_' || pattern[i1] == name[i2] || name[i2] == templatechar) && matchname(name + i2, pattern + i1, templateflag)) {
					if (templateflag && i2) strncpy(name, workname, i2);
					return TRUE;
				}
				if (templateflag == TEMPLATE_EXACT && name[i2] == templatechar) return FALSE;
				i2++;
			}
			return FALSE;
		}
		if (pattern[i1] == '_') {
			if (!name[i2] || (templateflag == TEMPLATE_EXACT && name[i2] == templatechar)) return FALSE;
		}
		else {
			if (pattern[i1] == '\\' && (pattern[i1 + 1] == '\\' || pattern[i1 + 1] == '_' || pattern[i1 + 1] == '%')) i1++;
			if (templatechar && name[i2] == templatechar) {
				if (templateflag) workname[i2] = pattern[i1];
			}
			else if (pattern[i1] != name[i2]) return FALSE;
		}
	}
	if (!name[i2] && templateflag) strncpy(name, workname, i2);
	return (!name[i2]);
}

static void replacename(CHAR *srcpattern, CHAR *srcname, CHAR *destpattern, CHAR *destname)
{
	INT i1, i2;

	strcpy(destname, destpattern);
	for (i1 = i2 = 0; srcpattern[i1]; i1++, i2++) {
		while (srcpattern[i1] && srcpattern[i1] != '?') i1++;
		if (!srcpattern[i1]) break;
		while (destpattern[i2] != '?') i2++;
		destname[i2] = srcname[i1];
	}
}

void closetablefiles(INT openfilenum)
{
	INT i1, filenum;
	OPENFILE *opf1;
	TABLE *tab1;
	INDEX *idx1;

	if (openfilenum < 1 || openfilenum > connection.numopenfiles) return;

 	opf1 = openfileptr(openfilenum);
	tab1 = tableptr(opf1->tablenum);
	if (opf1->textfilenum) rioclose(opf1->textfilenum);
	if (opf1->hopenindexarray != NULL) {
		for (i1 = 1; i1 <= tab1->numindexes; i1++) {
			filenum = (*opf1->hopenindexarray)[i1 - 1];
			if (!filenum) continue;
			idx1 = indexptr(tab1, i1);
			if (idx1->type == INDEX_TYPE_ISAM) xioclose(filenum);
			else if (idx1->type == INDEX_TYPE_AIM) aioclose(filenum);
		}
		memfree((unsigned char **) opf1->hopenindexarray);
	}
	if (opf1->hrecbuf != NULL) memfree(opf1->hrecbuf);
	memset(opf1, 0, sizeof(OPENFILE));
}

void closetableindex(INT tablenum, INT i1,  INDEX *idx1)
{
	INT openfilenum, filenum;
	OPENFILE *opf1;
	/* close index file if it is open */
	for (openfilenum = 0; ++openfilenum <= connection.numopenfiles; ) {
		opf1 = openfileptr(openfilenum);
		if (opf1->tablenum == tablenum) {
			filenum = (*opf1->hopenindexarray)[i1 - 1];
			if (filenum) {
				if (idx1->type == INDEX_TYPE_ISAM) xioclose(filenum);
				else if (idx1->type == INDEX_TYPE_AIM) aioclose(filenum);
			}
			break;
		}
	}
}
