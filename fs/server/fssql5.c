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
#define INC_TIME
#define INC_WINDOWS
#include "includes.h"
#include "base.h"
#include "fsrun.h"
#include "fssqlx.h"
#include "fio.h"
#include "bmath.h"
#include "sio.h"

/* following code is for debug purposes only */
#define FS_DEBUGCODE 1

#define MATCHCHAR	0x01

#define MAXVARS 64
#define MAXFILEREFS 32
#define RETSTACKSIZE 32

#define COLREFTOSTR_DIRECT	0x01

/* global variables */
extern int fsflags;
extern CONNECTION connection;
extern OFFSET filepos;
extern CHAR workdir[256];
#if FS_DEBUGCODE
extern void debugcode(char *str, int pcount, PCODE *pgm);
#endif

/* local variables */
static PLAN **thishplan;				/* for use by supporting routines */
static long vars[MAXVARS];				/* variables */
static int filerefmap[MAXFILEREFS];		/* array of open file numbers */
static int retstack[RETSTACKSIZE];		/* call return stack */
static UCHAR **updaterecbuf;				/* alternate record buffer for use by update */
static int updaterecbufsize;			/* size of buffer */
static char tempnumcolumns[32][64];		/* temporary numeric values for OP_COL arithmethic operations */
static char work1[1024], work2[1024], work3[128];

/* prototypes */
static void execerror(void);
static INT writedirtywks(INT worksetnum, INT count);
static int sqlaimrec(int, int, int, int, int, int *);
static void buildkey(INT, INT, UCHAR **, CHAR *);
static int colreftostr(UINT, INT, UCHAR **, INT *, INT *);
static int strtocolref(UCHAR *, int, int, int);
static INT changecolsize(INT, INT, CHAR*);
static COLREF *getcolref(int);
static int getcoltype(int);
static INT cvttodatetime(INT srctype, UCHAR *src, INT srclen, INT desttype, UCHAR *dest);
static INT matchstring(UCHAR *string, INT stringlen, UCHAR *pattern, INT patternlen);

/* execute a program */
INT execplan(HPLAN hplan, LONG *arg1, LONG *arg2, LONG *arg3) // @suppress("No return")
{
	static SIOKEY **hsortkeys;
	INT i1, i2, i3, i4, i5, fieldlen, fieldoff, len, op, op1, op2, op3, rc, recsize;
	INT pcount, pos, maxpcount, maxvar, worksetnum, retstackindex, openfilenum;
	INT keyfilerefnum, keyindexnum, numkeys, numsortkeys, idxhandle, txthandle;
	INT type1, type2, length1, length2, scale1, scale2;
	INT forupdateflag, nowaitflag;
	UCHAR work[16];
	OFFSET offset;
	UCHAR c1, *ptr1, *ptr2, **hmem1;
	PLAN *plan;
	WORKSET *wks1;
	COLREF *crf1;
	TABLE *tab1;
	COLUMN *col1;
	INDEX *idx1;
	OPENFILE *opf1;
	PCODE *pcode;
	SIOKEY workkey[2], *workkeyptr, **sortkeys;
#if OS_UNIX
	CHAR *ptr;
#endif

	thishplan = hplan;
	plan = *hplan;
	if (plan->flags & PLAN_FLAG_ERROR) return sqlerrnummsg(SQLERR_BADRSID, "result set is in unrecoverable state", NULL);
	if (!plan->worksetnum) {  /* first execution of PLAN */
		if ((plan->flags & PLAN_FLAG_FORUPDATE) && plan->numtables > 1) {
			/* NOTE: otherwise probably need to move VALIDREC flag to openfile structure */
			execerror();
			return sqlerrnum(SQLERR_EXEC_BADPGM);
		}
/*** CODE: MAYBE THINK ABOUT GOING STRAIGHT TO PLAN->FILEREFMAP AND BYPASS ***/
/***       FILEREFMAP[MAXFILEREFS] TO PREVENT BELOW LIMITATION ***/  
		if (plan->numtables > MAXFILEREFS || plan->numvars > MAXVARS) {
			execerror();
			return sqlerrnum(SQLERR_EXEC_TOOBIG);
		}
		hmem1 = memalloc(plan->numtables * sizeof(INT), MEMFLAGS_ZEROFILL);
		if (hmem1 == NULL) {
			execerror();
			return sqlerrnum(SQLERR_NOMEM);
		}
		plan = *hplan;
		plan->filerefmap = (INT **) hmem1;
		for (i1 = 0; i1 < plan->numtables; i1++) {  /* open files */
			i2 = plan->tablenums[i1];
			i2 = opentabletextfile(i2);
			if (i2 < 1) {
				execerror();
				if (!i2) return sqlerrnum(SQLERR_INTERNAL);
 				return i2;
			}
			plan = *hplan;
			(*plan->filerefmap)[i1] = filerefmap[i1] = i2;
		}
		worksetnum = allocworkset(plan->numworksets);
		if (worksetnum < 1) {
			execerror();
			return worksetnum;
		}
		plan = *hplan;
		plan->worksetnum = worksetnum;
		for (i1 = 0; i1 < plan->numworksets; i1++) {  /* allocate worksets */
			i3 = (*plan->worksetcolrefinfo)[i1 * 3];
			i4 = (*plan->worksetcolrefinfo)[i1 * 3 + 1];
			i5 = (*plan->worksetcolrefinfo)[i1 * 3 + 2];
			hmem1 = memalloc(i5 * sizeof(COLREF), 0);
			if (hmem1 == NULL) {
				execerror();
				return sqlerrnum(SQLERR_NOMEM);
			}
			plan = *hplan;
			wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset + i1;
			wks1->updatable = plan->updatable;
			wks1->rsnumcolumns = i4;
			wks1->numcolrefs = i5;
			wks1->colrefarray = (HCOLREF) hmem1;
			crf1 = *wks1->colrefarray;
			memcpy(crf1, *plan->colrefarray + i3, (i5 * sizeof(COLREF)));
			for (i2 = i3 = 0; i3 < i4; i3++) i2 += crf1[i3].length;
			wks1->rsrowbytes = i2;
			for ( ; i3 < i5; i3++) i2 += crf1[i3].length;
			if (plan->flags & PLAN_FLAG_SORT) i2 += 6;
			wks1->rowbytes = i2;
		}
		for (i1 = 0; i1 < plan->numvars; ) vars[i1++] = 0;
	}
	else {
		/* move common use variables to stack */
		for (i1 = 0; i1 < plan->numtables; i1++) filerefmap[i1] = (*plan->filerefmap)[i1];
		worksetnum = plan->worksetnum;
		if (plan->numvars) for (i1 = 0; i1 < plan->numvars; i1++) vars[i1] = (*plan->savevars)[i1];
		else vars[0] = vars[1] = vars[2] = 0;
	}
	if (arg1 != NULL) vars[0] = *arg1;
	if (arg2 != NULL) vars[1] = *arg2;
	if (arg3 != NULL) vars[2] = *arg3;
	memfree((UCHAR **) hsortkeys);
	hsortkeys = NULL;
	maxvar = plan->numvars;
	maxpcount = plan->pgmcount - 1;
	forupdateflag = plan->flags & PLAN_FLAG_FORUPDATE;
	nowaitflag = plan->flags & PLAN_FLAG_LOCKNOWAIT;
	pcount = retstackindex = 0;
	pcode = *plan->pgm;

	for ( ; ; ) {  /* main execution loop */
		op = pcode[pcount].code;
		op1 = pcode[pcount].op1;
		op2 = pcode[pcount].op2;
		op3 = pcode[pcount].op3;
/* following code is for debug purposes only */
#if FS_DEBUGCODE
		if (fsflags & FSFLAGS_DEBUG4) debugcode("EXEC", pcount, &pcode[pcount]);
#endif
		switch (op) {
		case OP_CLEAR:
			openfilenum = filerefmap[op1 - 1];
			opf1 = *connection.hopenfilearray + openfilenum - 1;
			memset(*opf1->hrecbuf, ' ', opf1->reclength);
			break;
		case OP_KEYINIT:
			openfilenum = filerefmap[op1 - 1];
			opf1 = *connection.hopenfilearray + openfilenum - 1;
			tab1 = tableptr(opf1->tablenum);
			if (op2 < 1 || op2 > tab1->numindexes || op3 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			idx1 = *tab1->hindexarray + op2 - 1;
			if (op3 >= 1 && vars[op3 - 1] < 0) c1 = 0xFF;
			else c1 = 0x00;
			for (i1 = 0; i1 < idx1->numkeys; i1++)
				memset(*opf1->hrecbuf + (*idx1->hkeys)[i1].pos, c1, (*idx1->hkeys)[i1].len);
			if (idx1->type == INDEX_TYPE_AIM) {
				idxhandle = (*opf1->hopenindexarray)[op2 - 1];
				if (!idxhandle) {
					i1 = opentableindexfile(openfilenum, op2);
					if (i1 < 0) {
						execerror();
						return -1;
					}
					/* memory may have shifted */
					opf1 = *connection.hopenfilearray + openfilenum - 1;
					tab1 = tableptr(opf1->tablenum);
					idx1 = *tab1->hindexarray + op2 - 1;
					idxhandle = (*opf1->hopenindexarray)[op2 - 1];
				}
				aiomatch(idxhandle, MATCHCHAR);
				rc = aionew(idxhandle);
				if (rc < 0) {
					execerror();
					return sqlerrnummsg(rc, "error in preparing AIM index for reading", NULL);
				}
				opf1->aimrec = -1;
			}
			keyfilerefnum = op1;
			keyindexnum = op2;
			break;
		case OP_KEYLIKE:
			opf1 = *connection.hopenfilearray + filerefmap[keyfilerefnum - 1] - 1;
			tab1 = tableptr(opf1->tablenum);
			idx1 = *tab1->hindexarray + keyindexnum - 1;
			if (colreftostr(op2, 0, &ptr1, &i2, &scale1)) {
				execerror();
				return -1;
			}
			if (!i2) break;
			if (colreftostr(op3, 0, &ptr2, &i3, &scale2)) {
				execerror();
				return -1;
			}
			for (i1 = i3 = 0; i1 < i2; i1++) {
				if (ptr1[i1] == '%') break;
				if (ptr1[i1] == '_') {
					if (idx1->type == INDEX_TYPE_ISAM) break;
					if ((size_t) i3 < sizeof(work1)) work1[i3++] = MATCHCHAR;
					continue;
				}
				if (ptr2[0]) { /* ESCAPE clause was used to specify an escape character */
					if (ptr1[i1] == ptr2[0] && i1 + 1 < i2 && (ptr1[i1 + 1] == ptr2[0] || ptr1[i1 + 1] == '%' || ptr1[i1 + 1] == '_')) i1++;
				}
				if ((size_t) i3 < sizeof(work1)) work1[i3++] = ptr1[i1];
			}
			/* fail if colref does not match key fileref */
			if (gettabref(op1) != keyfilerefnum) {
				execerror();
				return sqlerrnummsg(SQLERR_EXEC_BADCOL, "source column reference invalid", NULL);
			}
			col1 = columnptr(tab1, getcolnum(op1));
			fieldoff = col1->offset;
			fieldlen = col1->fieldlen;
			if (idx1->type == INDEX_TYPE_ISAM || i1 == i2) {
				if (i3) {
					if (i3 > fieldlen) i3 = fieldlen;
					memcpy(*opf1->hrecbuf + fieldoff, work1, i3);
				}
			}
			else if (idx1->type == INDEX_TYPE_AIM) {
				/* at least one % (match any) */
				for (i3 = 0; i3 < idx1->numkeys; i3++) {
					pos = (*idx1->hkeys)[i3].pos;
					len = (*idx1->hkeys)[i3].len;
					if (fieldoff < pos || fieldoff + fieldlen > pos + len) continue;
					/* found the key */
					ptr2 = *opf1->hrecbuf + fieldoff;
					/* parse out left justified key */
					for (i1 = i4 = 0; ; i1++) {
						if (ptr1[i1] == '%') break;
						if (ptr1[i1] == '_') {
							if (i4 < fieldlen) ptr2[i4++] = MATCHCHAR;
							continue;
						}
						if (ptr1[i1] == '\\' && (ptr1[i1 + 1] == '\\' || ptr1[i1 + 1] == '%' || ptr1[i1 + 1] == '_')) i1++;
						if (i4 < fieldlen) ptr2[i4++] = ptr1[i1];
					}
					/* parse out right justified key */
					for (i4 = fieldlen, i2--; ; i2--) {
						c1 = ptr1[i2];
						if (c1 == '%') {
							if (!i2 || ptr1[i2 - 1] != '\\') break;
							i2--;
						}
						else if (c1 == '_') {
							if (!i2 || ptr1[i2 - 1] != '\\') {
								if (i4) ptr2[--i4] = MATCHCHAR;
								continue;
							}
							i2--;
						}
						else if (c1 == '\\' && i2 && ptr1[i2 - 1] == '\\') i2--;
						if (i4) ptr2[--i4] = c1;
					}
					if (i1 == i2) continue;
					/* support floating keys */
					idxhandle = (*opf1->hopenindexarray)[keyindexnum - 1];
					work1[0] = (i3 + 1) / 10 + '0';
					work1[1] = (i3 + 1) % 10 + '0';
					work1[2] = 'F';
					for (i4 = 3; ++i1 <= i2; ) {
						if (ptr1[i1] == '%') {
							if (i4 >= 6) {
								rc = aiokey(idxhandle, (UCHAR *) work1, i4);
								if (rc < 0) {
									execerror();
									return sqlerrnummsg(rc, "error in building key specification of AIM index", NULL);
								}
								/* memory may have moved */
								opf1 = *connection.hopenfilearray + filerefmap[keyfilerefnum - 1] - 1;
								tab1 = tableptr(opf1->tablenum);
								idx1 = *tab1->hindexarray + keyindexnum - 1;
							}
							i4 = 3;
							continue;
						}
						if (ptr1[i1] == '_') {
							if ((size_t) i4 < sizeof(work1)) work1[i4++] = MATCHCHAR;
							continue;
						}
						if (ptr1[i1] == '\\' && (ptr1[i1 + 1] == '\\' || ptr1[i1 + 1] == '%' || ptr1[i1 + 1] == '_')) i1++;
						if ((size_t) i4 < sizeof(work1)) work1[i4++] = ptr1[i1];
					}
				}
			}
			break;
		case OP_SETFIRST:
		case OP_SETLAST:
			openfilenum = filerefmap[op1 - 1];
			opf1 = *connection.hopenfilearray + openfilenum - 1;
			tab1 = tableptr(opf1->tablenum);
			txthandle = opf1->textfilenum;
			if (!op2) {
				if (op == OP_SETFIRST) riosetpos(txthandle, 0);
				else {
					rc = fioflck(txthandle);
					if (rc < 0) {
						execerror();
						return sqlerrnummsg(rc, "error in locking file", nameptr(tab1->textfilename));
					}
					rioeofpos(txthandle, &offset);
					fiofulk(txthandle);
					riosetpos(txthandle, offset);
				}
			}
			else {
				if (op2 < 0 || op2 > tab1->numindexes) {
					execerror();
					return sqlerrnum(SQLERR_EXEC_BADPGM);
				}
				idxhandle = (*opf1->hopenindexarray)[op2 - 1];
				if (!idxhandle) {
					if (opentableindexfile(openfilenum, op2)) {
						execerror();
						return -1;
					}
					/* memory may have shifted */
					opf1 = *connection.hopenfilearray + openfilenum - 1;
					tab1 = tableptr(opf1->tablenum);
					idxhandle = (*opf1->hopenindexarray)[op2 - 1];
				}
				idx1 = *tab1->hindexarray + op2 - 1;
				if (idx1->type == INDEX_TYPE_ISAM) {
/*** CODE: CONSIDER USING COLLATE TABLE ***/
					if (op == OP_SETFIRST) c1 = 0x00;
					else c1 = 0xFF;
					rc = fioflck(txthandle);
					if (rc < 0) {
						execerror();
						return sqlerrnummsg(rc, "error in locking file", nameptr(tab1->textfilename));
					}
					rc = xiofind(idxhandle, &c1, 1);
					fiofulk(txthandle);
					if (rc < 0) {
						execerror();
						return sqlerrnummsg(rc, nameptr(idx1->indexfilename), NULL);
					}
				}
				else {  /* shouldn't happen, but support aim by using rio only */
					if (op == OP_SETFIRST) riosetpos(txthandle, 0);
					else {
						rc = fioflck(txthandle);
						if (rc < 0) {
							execerror();
							return sqlerrnummsg(rc, "error in locking file", nameptr(tab1->textfilename));
						}
						rioeofpos(txthandle, &offset);
						fiofulk(txthandle);
						riosetpos(txthandle, offset);
					}
					opf1->aimrec = -2;
				}
			}
			/* memory may have shifted */
			plan = *hplan;
			pcode = *plan->pgm;
			break;
/*** CODE: FOR ALL READS, SHOULD WE MEMSET TO BLANK IF READ LENGTH IS LESS THAN TABLE LENGTH ***/
		case OP_READNEXT:
		case OP_READPREV:
			openfilenum = filerefmap[op1 - 1];
			opf1 = *connection.hopenfilearray + openfilenum - 1;
			tab1 = tableptr(opf1->tablenum);
			txthandle = opf1->textfilenum;
			if (forupdateflag) riounlock(txthandle, -1);
			plan->flags &= ~PLAN_FLAG_VALIDREC;
			if (!op2) {
				if (op == OP_READNEXT) {
					for ( ; ; ) {
						if (forupdateflag) {
							rc = riolock(txthandle, nowaitflag);
							if (rc != 0) {
								execerror();
								return sqlerrnummsg(rc, "error locking data record", nameptr(tab1->textfilename));
							}
						}
						recsize = rioget(txthandle, *opf1->hrecbuf, tab1->reclength);
						if (forupdateflag && recsize < 0) {
							riolastpos(txthandle, &offset);
							riounlock(txthandle, offset);
						}
						if (recsize != -2) break;
					};
				}
				else {
					for ( ; ; ) {
						recsize = rioprev(txthandle, *opf1->hrecbuf, tab1->reclength);
						if (recsize < 0) {
							if (recsize != -2) break;
							continue;
						}
						if (!forupdateflag) break;
						riolastpos(txthandle, &offset);
						riosetpos(txthandle, offset);
						rc = riolock(txthandle, nowaitflag);
						if (rc != 0) {
							execerror();
							return sqlerrnummsg(rc, "error locking data record", nameptr(tab1->textfilename));
						}
						recsize = rioget(txthandle, *opf1->hrecbuf, tab1->reclength);
						if (recsize < 0) {
							riolastpos(txthandle, &offset);
							riounlock(txthandle, offset);
						}
						if (recsize != -2) break;
					}
				}
				if (recsize >= 0) {
					if (recsize < tab1->reclength) memset(*opf1->hrecbuf + recsize, ' ', tab1->reclength - recsize);
					riolastpos(txthandle, &filepos);
					vars[op3 - 1] = TRUE;
				}
				else if (recsize == -1) vars[op3 - 1] = FALSE;
				else {
					execerror();
					return sqlerrnummsg(recsize, "error reading data record", nameptr(tab1->textfilename));
				}
			}
			else {
				if (op2 < 0 || op2 > tab1->numindexes) {
					execerror();
					return sqlerrnum(SQLERR_EXEC_BADPGM);
				}
				idxhandle = (*opf1->hopenindexarray)[op2 - 1];
				if (!idxhandle) {
					if (opentableindexfile(openfilenum, op2)) {
						execerror();
						return -1;
					}
					/* memory may have shifted */
					opf1 =  *connection.hopenfilearray + openfilenum - 1;
					idxhandle = (*opf1->hopenindexarray)[op2 - 1];
				}
				idx1 = *tab1->hindexarray + op2 - 1;
				if (idx1->type == INDEX_TYPE_ISAM) {
					rc = fioflck(txthandle);
					if (rc < 0) {
						execerror();
						return sqlerrnummsg(rc, "error in locking file", nameptr(tab1->textfilename));
					}
					for (offset = ~0; ; ) {  /* set to invalid position value */
						if (op == OP_READNEXT) {
							rc = xionext(idxhandle);
						}
						else rc = xioprev(idxhandle);
						if (rc != 1 || forupdateflag) fiofulk(txthandle);
						/* memory may have shifted */
						opf1 = *connection.hopenfilearray + openfilenum - 1;
						tab1 = tableptr(opf1->tablenum);
						if (rc < 0) {
							execerror();
							return sqlerrnummsg(rc, "error searching index", nameptr(idx1->indexfilename));
						}
						if (rc == 1) {
							riosetpos(txthandle, filepos);
							if (forupdateflag) {
								rc = riolock(txthandle, nowaitflag);
								if (rc != 0) {
									execerror();
									return sqlerrnummsg(rc, "error locking data record", nameptr(tab1->textfilename));
								}
							}
							recsize = rioget(txthandle, *opf1->hrecbuf, tab1->reclength);
							if (!forupdateflag) fiofulk(txthandle);
							if (recsize < 0) {
								if (forupdateflag) {
									riounlock(txthandle, filepos);
									if (recsize == -2 && filepos != offset) {  /* record probably deleted between index and text access */
										offset = filepos;
										/* try to reread index key by backing up */
										rc = fioflck(txthandle);
										if (rc < 0) {
											execerror();
											return sqlerrnummsg(rc, "error in locking file", nameptr(tab1->textfilename));
										}
										if (op == OP_READNEXT) rc = xioprev(idxhandle);
										else rc = xionext(idxhandle);
										/* memory may have shifted, opf1/tab1 will be restored after the continue */
										if (rc < 0) {
											fiofulk(txthandle);
											execerror();
											return sqlerrnummsg(rc, "error searching index", nameptr(idx1->indexfilename));
										}
										continue;
									}
								}
								execerror();
								if (recsize >= -2) return sqlerrnummsg(0, "unexpected deleted record or eof during index read", nameptr(tab1->textfilename));
								return sqlerrnummsg(recsize, "error in reading record from text file", nameptr(tab1->textfilename));
							}
							if (recsize < tab1->reclength) memset(*opf1->hrecbuf + recsize, ' ', tab1->reclength - recsize);
							vars[op3 - 1] = TRUE;
						}
						else vars[op3 - 1] = FALSE;
						break;
					}
				}
				else if (idx1->type == INDEX_TYPE_AIM) {
					recsize = sqlaimrec(openfilenum, forupdateflag, nowaitflag, op2, (op == OP_READNEXT) ? AIONEXT_NEXT : AIONEXT_PREV, &i1);
					if (recsize >= 0) {
						/* memory may have shifted */
						opf1 = *connection.hopenfilearray + openfilenum - 1;
						/* already blank appended by sqlaimrec */
						riolastpos(txthandle, &filepos);
						vars[op3 - 1] = TRUE;
					}
					else if (recsize == -1) vars[op3 - 1] = FALSE;
					else {
						execerror();
						return sqlerrnummsg(recsize, "error reading AIM record", fioname(i1));
					}
				}
				else {
					execerror();
					return sqlerrnum(SQLERR_EXEC_BADPGM);
				}
			}
			/* memory may have shifted */
			plan = *hplan;
			pcode = *plan->pgm;
			if (vars[op3 - 1] == TRUE) {
				plan->filepos = filepos;
				plan->flags |= PLAN_FLAG_VALIDREC;
			}
			break;
		case OP_READBYKEY:
		case OP_READBYKEYREV:
			openfilenum = filerefmap[op1 - 1];
			opf1 = *connection.hopenfilearray + openfilenum - 1;
			tab1 = tableptr(opf1->tablenum);
			txthandle = opf1->textfilenum;
			if (forupdateflag) riounlock(txthandle, -1);
			plan->flags &= ~PLAN_FLAG_VALIDREC;
			if (op2 < 1 || op2 > tab1->numindexes) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			idxhandle = (*opf1->hopenindexarray)[op2 - 1];
			if (!idxhandle) {
				if (opentableindexfile(openfilenum, op2)) {
					execerror();
					return -1;
				}
				/* memory may have shifted */
				opf1 = *connection.hopenfilearray + openfilenum - 1;
				tab1 = tableptr(opf1->tablenum);
				idxhandle = (*opf1->hopenindexarray)[op2 - 1];
			}
			idx1 = *tab1->hindexarray + op2 - 1;
			if (idx1->type == INDEX_TYPE_ISAM) {
				buildkey(opf1->tablenum, op2, opf1->hrecbuf, work1);
				for (offset = ~0; ; ) {  /* set to invalid position value */
					rc = fioflck(txthandle);
					if (rc < 0) {
						execerror();
						return sqlerrnummsg(rc, "error in locking file", nameptr(tab1->textfilename));
					}
					if (op == OP_READBYKEY) rc = xiofind(idxhandle, (UCHAR *) work1, idx1->keylength);
					else rc = xiofindlast(idxhandle, (UCHAR *) work1, idx1->keylength);
					if (rc != 1 || forupdateflag) fiofulk(txthandle);
					/* memory may have shifted */
					opf1 = *connection.hopenfilearray + openfilenum - 1;
					tab1 = tableptr(opf1->tablenum);
					if (rc < 0) {
						execerror();
						return sqlerrnummsg(rc, "error in finding key in index", nameptr(idx1->indexfilename));
					}
					if (rc == 1) {
						riosetpos(txthandle, filepos);
						if (forupdateflag) {
							rc = riolock(txthandle, nowaitflag);
							if (rc != 0) {
								execerror();
								return sqlerrnummsg(rc, "error locking data record", nameptr(tab1->textfilename));
							}
						}
						recsize = rioget(txthandle, *opf1->hrecbuf, tab1->reclength);
						if (!forupdateflag) fiofulk(txthandle);
						if (recsize < 0) {
							if (forupdateflag) {
								riounlock(txthandle, filepos);
								if (recsize == -2 && filepos != offset) {  /* record probably deleted between index and text access */
									offset = filepos;
									continue;
								}
							}
							execerror();
							if (recsize >= -2) return sqlerrnummsg(0, "unexpected deleted record or eof during index read", nameptr(tab1->textfilename));
							return sqlerrnummsg(recsize, "error in reading record from text file", nameptr(tab1->textfilename));
						}
						if (recsize < tab1->reclength) memset(*opf1->hrecbuf + recsize, ' ', tab1->reclength - recsize);
						vars[op3 - 1] = TRUE;
					}
					else vars[op3 - 1] = FALSE;
					break;
				}
			}
			else if (idx1->type == INDEX_TYPE_AIM) {
				for (i3 = 0; i3 < idx1->numkeys; i3++) {
					work1[0] = (i3 + 1) / 10 + '0';
					work1[1] = (i3 + 1) % 10 + '0';
					i2 = (*idx1->hkeys)[i3].len;
					ptr1 = (*opf1->hrecbuf) + (*idx1->hkeys)[i3].pos;
					if (*ptr1) {  /* exact or left */
						for (i4 = -1, i1 = 0; i1 < i2 && ptr1[i1]; i1++) if (ptr1[i1] != ' ' && ptr1[i1] != MATCHCHAR) i4 = i1;
						if (i4 != -1) {
							if ((size_t) ++i4 > sizeof(work1) - 3) i4 = sizeof(work1) - 3;
							if (i4 == i2) work1[2] = 'X';
							else work1[2] = 'L';
							memcpy(work1 + 3, ptr1, i4);
							rc = aiokey(idxhandle, (UCHAR *) work1, i4 + 3);
							if (rc < 0) {
								execerror();
								return sqlerrnummsg(rc, "error in building key specification of AIM index", NULL);
							}
						}
						if (i1 == i2) continue;
						ptr1 += i1;
						i2 -= i1;
					}
					if (ptr1[i2 - 1]) {  /* right */
						/* note: do not need to check for beginning condition (at least one '\0') */
						for (i4 = -1, i1 = i2; ptr1[--i2]; ) if (ptr1[i2] != ' ' && ptr1[i2] != MATCHCHAR) i4 = i2;
						if (i4 != -1) {
							i4 = i1 - i4;
							if ((size_t) i4 > sizeof(work1) - 3) i4 = sizeof(work1) - 3;
							work1[2] = 'R';
							memcpy(work1 + 3, ptr1 + i1 - i4, i4);
							rc = aiokey(idxhandle, (UCHAR *) work1, i4 + 3);
							if (rc < 0) {
								execerror();
								return sqlerrnummsg(rc, "error in building key specification of AIM index", NULL);
							}
						}
						if (!i2) continue;
					}
					/* floating */
					work1[2] = 'F';
					for (i1 = 0; ; ) {
						for ( ; i1 < i2 && (!ptr1[i1] || ptr1[i1] == ' ' || ptr1[i1] == MATCHCHAR); i1++);
						if (i1 == i2) break;
						for (i4 = i5 = i1; i1 < i2 && ptr1[i1]; i1++) if (ptr1[i1] != ' ' && ptr1[i1] != MATCHCHAR) i4 = i1;
						i4 -= i5 - 1;
						if (i4 < 3) continue;
						if ((size_t) i4 > sizeof(work1) - 3) {
							i4 = sizeof(work1) - 3;
							i1 = i5 + i4;
						}
						memcpy(work1 + 3, ptr1 + i5, i4);
						rc = aiokey(idxhandle, (UCHAR *) work1, i4 + 3);
						if (rc < 0) {
							execerror();
							return sqlerrnummsg(rc, "error in building key specification of AIM index", NULL);
						}
					}
				}
				recsize = sqlaimrec(openfilenum, forupdateflag, nowaitflag, op2, (op == OP_READBYKEY) ? AIONEXT_FIRST : AIONEXT_LAST, &i1);
				if (recsize >= 0) {
					/* memory may have shifted */
					opf1 = *connection.hopenfilearray + openfilenum - 1;
					/* already blank appended by sqlaimrec */
					riolastpos(txthandle, &filepos);
					vars[op3 - 1] = TRUE;
				}
				else if (recsize == -1) vars[op3 - 1] = FALSE;
				else {
					execerror();
					return sqlerrnummsg(recsize, "error reading AIM record", fioname(i1));
				}
			}
			else {
				execerror();
				return sqlerrnummsg(SQLERR_EXEC_BADDATAFILE, "invalid index type", NULL);
			}
			/* memory may have shifted */
			plan = *hplan;
			pcode = *plan->pgm;
			if (vars[op3 - 1] == TRUE) {
				plan->filepos = filepos;
				plan->flags |= PLAN_FLAG_VALIDREC;
			}
			break;
		case OP_READPOS:
			openfilenum = filerefmap[op1 - 1];
			opf1 = *connection.hopenfilearray + openfilenum - 1;
			tab1 = tableptr(opf1->tablenum);
			txthandle = opf1->textfilenum;
			/* not necessary, but just in case OP_UNLOCK is not called */
			riounlock(txthandle, -1);
			plan->flags &= ~PLAN_FLAG_VALIDREC;
			if (colreftostr(op2, 0, &ptr1, &i2, &scale1)) {
				execerror();
				return -1;
			}
			if (i2 != 12 || getcoltype(op2) != TYPE_POSNUM) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			mscntooff(ptr1, &offset, 12);
			riosetpos(txthandle, offset);
			rc = riolock(txthandle, FALSE);
			if (rc != 0) {
				execerror();
				return sqlerrnummsg(rc, "error locking data record", nameptr(tab1->textfilename));
			}
			recsize = rioget(txthandle, *opf1->hrecbuf, tab1->reclength);
			if (recsize < 0) {
				riounlock(txthandle, offset);
				if (recsize <= -3) {
					execerror();
					return sqlerrnummsg(recsize, "error reading data file", nameptr(tab1->textfilename));
				}
				vars[op3 - 1] = FALSE;
			}
			else {
				if (recsize < tab1->reclength) memset(*opf1->hrecbuf + recsize, ' ', tab1->reclength - recsize);
				vars[op3 - 1] = TRUE;
				plan->filepos = offset;
				plan->flags |= PLAN_FLAG_VALIDREC;
			}
			break;
		case OP_UNLOCK:
			if (op1 != 1) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			openfilenum = filerefmap[op1 - 1];
			opf1 = *connection.hopenfilearray + openfilenum - 1;
			riounlock(opf1->textfilenum, -1);
			plan->flags &= ~PLAN_FLAG_VALIDREC;
			break;
		case OP_WRITE:
			openfilenum = filerefmap[op1 - 1];
			opf1 = *connection.hopenfilearray + openfilenum - 1;
			tab1 = tableptr(opf1->tablenum);
			txthandle = opf1->textfilenum;

			/* open the indexes */
			for (i1 = 0; ++i1 <= tab1->numindexes; ) {
				if (!(*opf1->hopenindexarray)[i1 - 1]) {  /* open the index */
					if (opentableindexfile(openfilenum, i1)) {
						execerror();
						return -1;
					}
					/* memory may have shifted */
					opf1 = *connection.hopenfilearray + openfilenum - 1;
					tab1 = tableptr(opf1->tablenum);
				}
			}
			rc = fioflck(txthandle);  /* lock the text file */
			if (rc < 0) {
				execerror();
				return sqlerrnummsg(rc, "error in locking file", nameptr(tab1->textfilename));
			}

			/* check for duplicates */
			for (i1 = 0; ++i1 <= tab1->numindexes; ) {
				idxhandle = (*opf1->hopenindexarray)[i1 - 1];
				idx1 = *tab1->hindexarray + i1 - 1;
				if (idx1->type == INDEX_TYPE_ISAM && !(idx1->flags & INDEX_DUPS)) {  /* if isam and no dups, check for dup */
					buildkey(opf1->tablenum, i1, opf1->hrecbuf, work1);
					rc = xiofind(idxhandle, (UCHAR *) work1, idx1->keylength);
					if (rc == 1) {
						fiofulk(txthandle);
						execerror();
						return sqlerrnummsg(SQLERR_EXEC_DUPKEY, "error in writing record, no duplicate keys allowed", nameptr(idx1->indexfilename));
					}
					if (rc < 0) {
						fiofulk(txthandle);
						execerror();
						return sqlerrnummsg(rc, "error in writing record", NULL);
					}
					/* memory may have shifted */
					opf1 = *connection.hopenfilearray + openfilenum - 1;
					tab1 = tableptr(opf1->tablenum);
				}
			}

			/* get new record position */
			rc = 1;
			if (tab1->reclaimindex) {  /* try to use the space reclaimation index */
				idxhandle = (*opf1->hopenindexarray)[tab1->reclaimindex - 1];
				idx1 = indexptr(tab1, tab1->reclaimindex);
				if (idx1->type == INDEX_TYPE_ISAM) {
					rc = xiogetrec(idxhandle);
					if (!rc) offset = filepos;
				}
				else {
					rc = aiogetrec(idxhandle);
					if (!rc) offset = filepos * (tab1->reclength + tab1->eorsize);
				}
				/* memory may have shifted */
				opf1 = *connection.hopenfilearray + openfilenum - 1;
				tab1 = tableptr(opf1->tablenum);
			}
			if (rc) rioeofpos(txthandle, &offset);

			/* insert keys */
			rc = 0;
			for (i1 = 0; ++i1 <= tab1->numindexes; ) {
				idxhandle = (*opf1->hopenindexarray)[i1 - 1];
				idx1 = *tab1->hindexarray + i1 - 1;
				if (idx1->type == INDEX_TYPE_ISAM) {
					filepos = offset;
					buildkey(opf1->tablenum, i1, opf1->hrecbuf, work1);
					rc = xioinsert(idxhandle, (UCHAR *) work1, (INT)strlen(work1));
				}
				else {
					if (idx1->flags & INDEX_FIXED) {
						tab1 = tableptr(opf1->tablenum);
						filepos = offset / (OFFSET)(tab1->reclength + tab1->eorsize);
					}
					else if (offset) filepos = (offset - 1) / 256;
					else filepos = 0;
					rc = aioinsert(idxhandle, *opf1->hrecbuf);
				}
				/* memory may have shifted */
				opf1 = *connection.hopenfilearray + openfilenum - 1;
				tab1 = tableptr(opf1->tablenum);
				if (rc) break;
			}
			if (!rc) {  /* write record */
				riosetpos(txthandle, offset);
				rc = rioput(txthandle, *opf1->hrecbuf, opf1->reclength);
			}
			if (rc) {  /* undo index inserts */
				filepos = offset;
				while (--i1) {
					idxhandle = (*opf1->hopenindexarray)[i1 - 1];
					idx1 = *tab1->hindexarray + i1 - 1;
					if (idx1->type == INDEX_TYPE_ISAM) {
						buildkey(opf1->tablenum, i1, opf1->hrecbuf, work1);
						xiodelete(idxhandle, (UCHAR *) work1, (INT)strlen(work1), TRUE);
						/* memory may have shifted */
						opf1 = *connection.hopenfilearray + openfilenum - 1;
						tab1 = tableptr(opf1->tablenum);
					}
				}
				fiofulk(txthandle);
				if (rc < 0) {
					execerror();
					return sqlerrnummsg(rc, "error in writing record", NULL);
				}
				execerror();
				return sqlerrnummsg(rc, "error in writing record, unexpected duplicate key", NULL);
			}
			fiofulk(txthandle);
			/* memory may have shifted */
			plan = *hplan;
			pcode = *plan->pgm;
			break;
		case OP_UPDATE:
			if (!(plan->flags & PLAN_FLAG_VALIDREC)) {
				execerror();
				return sqlerrmsg("error in updating record, no current record");
			}
			offset = plan->filepos;
			openfilenum = filerefmap[op1 - 1];
			opf1 = *connection.hopenfilearray + openfilenum - 1;
			if (updaterecbuf == NULL) {
				updaterecbufsize = opf1->reclength + 4;
				updaterecbuf = memalloc(updaterecbufsize, 0);
				if (updaterecbuf == NULL) {
					execerror();
					return sqlerrnum(SQLERR_NOMEM);
				}
			}
			else if (updaterecbufsize < opf1->reclength + 4) {
				updaterecbufsize = opf1->reclength + 4;
				if (memchange(updaterecbuf, updaterecbufsize, 0)) {
					execerror();
					return sqlerrnum(SQLERR_NOMEM);
				}
			}
			/* memory may have shifted */
			opf1 = *connection.hopenfilearray + openfilenum - 1;
			tab1 = tableptr(opf1->tablenum);
			txthandle = opf1->textfilenum;

			/* open the indexes */
			for (i1 = 0; ++i1 <= tab1->numindexes; ) {
				if (!(*opf1->hopenindexarray)[i1 - 1]) {  /* open the index */
					if (opentableindexfile(openfilenum, i1)) {
						execerror();
						return -1;
					}
					/* memory may have shifted */
					opf1 = *connection.hopenfilearray + openfilenum - 1;
					tab1 = tableptr(opf1->tablenum);
				}
			}

			/* get the old record to compare against the keys */
			rc = fioflck(txthandle);
			if (rc < 0) {
				execerror();
				return sqlerrnummsg(rc, "error in locking file", nameptr(tab1->textfilename));
			}
			riosetpos(txthandle, offset);
			recsize = rioget(txthandle, *updaterecbuf, opf1->reclength);
			if (recsize < 0) {
				fiofulk(txthandle);
				execerror();
				return sqlerrnummsg(rc, "error in updating record", NULL);
			}
			if (recsize < tab1->reclength) {
				for (i1 = recsize, i2 = tab1->reclength, ptr1 = *opf1->hrecbuf; i1 < i2 && ptr1[i1] == ' '; i1++);
				if (i1 < i2) {
					fiofulk(txthandle);
					execerror();
					return sqlerrmsg("update data is longer than existing record");
				}
			}

			/* compare keys and check for duplicates */
			for (i1 = 0; ++i1 <= tab1->numindexes; ) {
				idxhandle = (*opf1->hopenindexarray)[i1 - 1];
				idx1 = *tab1->hindexarray + i1 - 1;
				work2[i1 - 1] = 0;
				for (i2 = 0; i2 < idx1->numkeys; i2++)
					if (memcmp(*updaterecbuf + (*idx1->hkeys)[i2].pos, *opf1->hrecbuf + (*idx1->hkeys)[i2].pos, (*idx1->hkeys)[i2].len)) break;
				if (i2 < idx1->numkeys) {
					work2[i1 - 1] = 1;
					if (idx1->type == INDEX_TYPE_ISAM && !(idx1->flags & INDEX_DUPS)) {
						/* check to see if key already exists */
						buildkey(opf1->tablenum, i1, opf1->hrecbuf, work1);
						rc = xiofind(idxhandle, (UCHAR *) work1, (INT)strlen(work1));
						if (rc == 1) {
							fiofulk(txthandle);
							execerror();
							return sqlerrnummsg(SQLERR_EXEC_DUPKEY, "error in updating record, no duplicate keys allowed", nameptr(idx1->indexfilename));
						}
						if (rc < 0) {
							fiofulk(txthandle);
							execerror();
							return sqlerrnummsg(rc, "error in updating record finding old keys", NULL);
						}
						/* memory may have shifted */
						opf1 = *connection.hopenfilearray + openfilenum - 1;
						tab1 = tableptr(opf1->tablenum);
					}
				}
			}

			/* insert the modified keys into the indexes */
			rc = 0;
			for (i1 = 0; ++i1 <= tab1->numindexes; ) {
				if (!work2[i1 - 1]) continue;
				idxhandle = (*opf1->hopenindexarray)[i1 - 1];
				idx1 = *tab1->hindexarray + i1 - 1;
				if (idx1->type == INDEX_TYPE_ISAM) {
					filepos = offset;
					buildkey(opf1->tablenum, i1, opf1->hrecbuf, work1);
					rc = xioinsert(idxhandle, (UCHAR *) work1, (INT)strlen(work1));
				}
				else {
					if (idx1->flags & INDEX_FIXED) {
						tab1 = tableptr(opf1->tablenum);
						filepos = offset / (OFFSET)(tab1->reclength + tab1->eorsize);
					}
					else if (offset) filepos = (offset - 1) / 256;
					else filepos = 0;
					rc = aioinsert(idxhandle, *opf1->hrecbuf);
				}
				/* memory may have shifted */
				opf1 = *connection.hopenfilearray + openfilenum - 1;
				tab1 = tableptr(opf1->tablenum);
				if (rc) break;
			}
			if (!rc) {  /* update the record */
				riosetpos(txthandle, offset);
				rc = rioput(txthandle, *opf1->hrecbuf, recsize);
			}
			if (rc) {  /* undo index inserts */
				filepos = offset;
				while (--i1) {
					if (!work2[i1 - 1]) continue;
					idxhandle = (*opf1->hopenindexarray)[i1 - 1];
					idx1 = *tab1->hindexarray + i1 - 1;
					if (idx1->type == INDEX_TYPE_ISAM) {
						buildkey(opf1->tablenum, i1, opf1->hrecbuf, work1);
						xiodelete(idxhandle, (UCHAR *) work1, (INT)strlen(work1), TRUE);
						/* memory may have shifted */
						opf1 = *connection.hopenfilearray + openfilenum - 1;
						tab1 = tableptr(opf1->tablenum);
					}
				}
				fiofulk(txthandle);
				execerror();
				if (rc < 0) return sqlerrnummsg(rc, "error in updating record", NULL);
				return sqlerrnummsg(rc, "error in updating record, unexpected duplicate key", NULL);
			}

			/* delete the old keys */
			filepos = offset;
			for (i1 = 0; ++i1 <= tab1->numindexes; ) {
				if (!work2[i1 - 1]) continue;
				idxhandle = (*opf1->hopenindexarray)[i1 - 1];
				idx1 = *tab1->hindexarray + i1 - 1;
				if (idx1->type == INDEX_TYPE_ISAM) {
					buildkey(opf1->tablenum, i1, updaterecbuf, work1);
					i2 = xiodelete(idxhandle, (UCHAR *) work1, (INT)strlen(work1), TRUE);
					if (!rc || (rc > 0 && i2 < 0)) rc = i2;
					/* memory may have shifted */
					opf1 = *connection.hopenfilearray + openfilenum - 1;
					tab1 = tableptr(opf1->tablenum);
				}
			}
			fiofulk(txthandle);
			if (forupdateflag) riounlock(txthandle, filepos);
			/* memory may have shifted */
			plan = *hplan;
			pcode = *plan->pgm;
			plan->flags &= ~PLAN_FLAG_VALIDREC;  /* don't allow 2 updates in a row */
			if (rc < 0) {
				execerror();
				if (rc < 0) return sqlerrnummsg(rc, "error in updating record deleting old keys", NULL);
#if 0
/*** CODE: NO LONGER FLAGGING THIS AS AN ERROR ***/
				return sqlerrnummsg(rc, "error in updating record, unexpected key deletion error", NULL);
#endif
			}
			break;
		case OP_DELETE:
			if (!(plan->flags & PLAN_FLAG_VALIDREC)) {
				execerror();
				return sqlerrmsg("error in deleteing record, no current record");
			}
			filepos = plan->filepos;
			openfilenum = filerefmap[op1 - 1];
			opf1 = *connection.hopenfilearray + openfilenum - 1;
			tab1 = tableptr(opf1->tablenum);
			txthandle = opf1->textfilenum;

			/* open the indexes */
			for (i1 = 0; ++i1 <= tab1->numindexes; ) {
				if (!(*opf1->hopenindexarray)[i1 - 1]) {  /* open the index */
					if (opentableindexfile(openfilenum, i1)) {
						execerror();
						return -1;
					}
					/* memory may have shifted */
					opf1 = *connection.hopenfilearray + openfilenum - 1;
					tab1 = tableptr(opf1->tablenum);
				}
			}
			rc = fioflck(txthandle);
			if (rc < 0) {
				execerror();
				return sqlerrnummsg(rc, "error in locking file", nameptr(tab1->textfilename));
			}

			/* delete the index keys */
			rc = 0;
			for (i1 = 0; ++i1 <= tab1->numindexes; ) {
				idx1 = indexptr(tab1, i1);
				if (idx1->type == INDEX_TYPE_ISAM) {
					idxhandle = (*opf1->hopenindexarray)[i1 - 1];
					buildkey(opf1->tablenum, i1, opf1->hrecbuf, work1);
					i2 = xiodelete(idxhandle, (UCHAR *) work1, (INT)strlen(work1), TRUE);
					if (!rc || (rc > 0 && i2 < 0)) rc = i2;
					/* memory may have shifted */
					opf1 = *connection.hopenfilearray + openfilenum - 1;
					tab1 = tableptr(opf1->tablenum);
				}
			}
/*** CODE: NOT SURE IF SHOULD DELETE RECORD NO MATTER WHAT OR REINSERT KEYS ***/
/***       FROM INDEXES THAT WERE DELETED ***/
			if (rc >= 0) {  /* delete the data record */
				riosetpos(txthandle, filepos);
				i2 = riodelete(txthandle, opf1->reclength);
				if (!rc || (rc == 1 && i2 < 0)) rc = i2;
				if (!i2 && tab1->reclaimindex) {  /* insert the record into the space reclaimation index */
					idxhandle = (*opf1->hopenindexarray)[tab1->reclaimindex - 1];
					idx1 = indexptr(tab1, tab1->reclaimindex);
					if (idx1->type == INDEX_TYPE_ISAM) {
						xiodelrec(idxhandle);
						/* memory may have shifted */
						opf1 = *connection.hopenfilearray + openfilenum - 1;
						tab1 = tableptr(opf1->tablenum);
					}
					else {
						filepos /= tab1->reclength + tab1->eorsize;
						aiodelrec(idxhandle);
					}
				}
			}
			fiofulk(txthandle);
			if (forupdateflag) riounlock(txthandle, filepos);
			/* memory may have shifted */
			plan = *hplan;
			pcode = *plan->pgm;
			plan->flags &= ~PLAN_FLAG_VALIDREC;
			if (rc < 0) {
				execerror();
				return sqlerrnummsg(rc, "error in deleting record from text file", nameptr(tab1->textfilename));
			}
			break;
		case OP_TABLELOCK:
			if (op1 != 1) {
				/* otherwise, we need to have array of lock counts in plan */
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			openfilenum = filerefmap[op1 - 1];
			opf1 = *connection.hopenfilearray + openfilenum - 1;
			tab1 = tableptr(opf1->tablenum);
			if (!tab1->lockcount) {
				rc = locktextfile(openfilenum);
				if (rc < 0) {
					execerror();
					return rc;
				}
				/* memory may have shifted */
				opf1 = *connection.hopenfilearray + openfilenum - 1;
				tab1 = tableptr(opf1->tablenum);
				tab1->lockhandle = rc;
			}
			tab1->lockcount++;
			/* memory may have shifted */
			plan = *hplan;
			plan->tablelockcnt++;
			break;
		case OP_TABLEUNLOCK:
			if (op1 != 1) {
				/* otherwise, we need to have array of lock counts in plan */
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			openfilenum = filerefmap[op1 - 1];
			opf1 = *connection.hopenfilearray + openfilenum - 1;
			tab1 = tableptr(opf1->tablenum);
			if (tab1->lockcount) {
				if (!--tab1->lockcount) {
					rc = unlocktextfile(tab1->lockhandle);
					if (rc < 0) {
						execerror();
						return rc;
					}
				}
				if (plan->tablelockcnt) plan->tablelockcnt--;
			}
			break;
		case OP_WORKINIT:
			if (op2 < 1 || op2 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			op1 -= (plan->numtables + 1);
			if (op1 < 0 || op1 >= plan->numworksets) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset + op1;
			/* don't expect workinit on workset to be called twice, but just in case */ 
			memfree(wks1->rows);
			wks1->rows = NULL;
			i1 = wks1->rowbytes;
			i2 = connection.memresultsize << 10;
			if (plan->multirowcnt) i2 /= plan->multirowcnt;
/*** NOTE: i2 use to be limited to 200, but I expect distinct and group by will always be larger */
			i2 /= i1;
			if (i2 < 2) i2 = 2;
			if (vars[op2 - 1] && vars[op2 - 1] < i2) i2 = vars[op2 - 1];
			wks1->memrowalloc = i2;
			wks1->rowid = wks1->memrowoffset = wks1->rowcount = 0;
			break;
		case OP_WORKNEWROW:
			op1 -= (plan->numtables + 1);
			if (op1 < 0 || op1 >= plan->numworksets) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset + op1;
			i1 = wks1->rowbytes;
			if (wks1->rows == NULL) {  /* first time */
				i2 = wks1->memrowalloc;
				hmem1 = memalloc(i2 * i1, 0);
				while (hmem1 == NULL) {
					if ((i2 >>= 1) < 1) {
						execerror();
						return sqlerrnum(SQLERR_NOMEM);
					}
					hmem1 = memalloc(i2 * i1, 0);
				}
				wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset + op1;
				wks1->rows = hmem1;
				wks1->memrowalloc = i2;
			}
			i2 = wks1->rowcount;
			i3 = wks1->memrowoffset;
			if (i2 >= wks1->memrowalloc) {
				if (i2 == wks1->memrowalloc) {
					if (workdir[0]) strcpy(work2, workdir);
					else strcpy(work2, ".");
#if OS_WIN32
					if (!GetTempFileName(work2, "fs6", 0, work1)) sqlerrnummsg(SQLERR_EXEC_BADWORKSET, "unable to create workfile name", NULL);
					i4 = fioopen(work1, FIO_M_PRP | FIO_P_WRK);
#endif
#if OS_UNIX
					for (i5 = 0; i5 < 1000; i5++) {
						ptr = tempnam(work2, "fs6");
						if (ptr == NULL) sqlerrnummsg(SQLERR_EXEC_BADWORKSET, "unable to create workfile name", NULL);
						strcpy(work1, ptr);
						free(ptr);
						i4 = fioopen(work1, FIO_M_EFC | FIO_P_WRK);
						if (i4 != ERR_EXIST) break;
					}
#endif
					if (i4 < 0) {
#if OS_WIN32
						DeleteFile(work1);
#endif
						execerror();
						return sqlerrnummsg(i4, "working set error during file creation", NULL);
					}
					wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset + op1;
					wks1->fiofilenum = i4;
					wks1->bufdirtyflag = TRUE;
					if (writedirtywks(worksetnum, op1 + 1) < 0) {
						execerror();
						return -1;
					}
					wks1->memrowoffset = wks1->memrowalloc;
				}
				else if (i2 > i3 && i2 < i3 + wks1->memrowalloc) {
					if (wks1->rowdirtyflag) wks1->bufdirtyflag = TRUE;
				}
				else {
					if (writedirtywks(worksetnum, op1 + 1) < 0) {
						execerror();
						return -1;
					}
					wks1->memrowoffset = i2;
				}
			}
			wks1->rowid = wks1->rowcount = i2 + 1;
			memset(*wks1->rows + (wks1->rowid - wks1->memrowoffset - 1) * i1, ' ', i1);
			wks1->rowdirtyflag = TRUE;
			/* memory may have shifted */
			plan = *hplan;
			pcode = *plan->pgm;
			break;
		case OP_WORKFREE:
/*** NOTE: COULD PROBABLY JUST PUT THIS CODE AT OP_FINISH AND OP_TERMINATE, ***/
/***       BUT THIS GIVES MORE FLEXIBILITY TO POSSIBLE FUTURE SUPPORT FOR COMBINED ***/
/***       DYNAMIC/STATIC WORKSETS ***/
			if (op2 < 1 || op2 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			op1 -= (plan->numtables + 1);
			if (op1 < 0 || op1 >= plan->numworksets) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset + op1;
			if (vars[op2 - 1] < wks1->memrowalloc) {
				wks1->memrowalloc = vars[op2 - 1];
				if (!wks1->memrowalloc) {
					memfree(wks1->rows);
					wks1->rows = NULL;
					if (wks1->fiofilenum) {
						fiokill(wks1->fiofilenum);
						wks1->fiofilenum = 0;
					}
				}
				else memchange(wks1->rows, wks1->memrowalloc * wks1->rowbytes, 0);
			}
			/* memory may have shifted */
			plan = *hplan;
			pcode = *plan->pgm;
			break;
		case OP_WORKGETROWID:
			if (op2 < 1 || op2 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			op1 -= (plan->numtables + 1);
			if (op1 < 0 || op1 >= plan->numworksets) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset + op1;
			vars[op2 - 1] = wks1->rowid;
			break;
		case OP_WORKSETROWID:
			if (op2 < 1 || op2 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			op1 -= (plan->numtables + 1);
			if (op1 < 0 || op1 >= plan->numworksets) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset + op1;
			i1 = vars[op2 - 1];
			if (wks1->rowcount > wks1->memrowalloc) {
				i2 = wks1->rowcount;
				i3 = wks1->memrowoffset;
				if (i1 <= i3 || i1 > i3 + i2) {
					if (writedirtywks(worksetnum, op1 + 1) < 0) {
						execerror();
						return -1;
					}
					wks1->rowid = i1;
					if (readwks(worksetnum, op1 + 1) < 0) {
						execerror();
						return -1;
					}
				}
			}
			wks1->rowid = i1;
			break;
		case OP_WORKGETROWCOUNT:
			if (op2 < 1 || op2 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			op1 -= (plan->numtables + 1);
			if (op1 < 0 || op1 >= plan->numworksets) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset + op1;
			vars[op2 - 1] = wks1->rowcount;
			break;
		case OP_WORKSORT:
		case OP_WORKUNIQUE:
			if (op2 < 1 || op2 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			op1 -= (plan->numtables + 1);
			if (op1 < 0 || op1 >= plan->numworksets) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset + op1;
			if (wks1->rowcount > 1) {
				if (wks1->rowcount > wks1->memrowalloc && writedirtywks(worksetnum, op1 + 1) < 0) {
					memfree((UCHAR **) hsortkeys);
					hsortkeys = NULL;
					execerror();
					return -1;
				}
				numkeys = vars[op2 - 1];
				workkeyptr = workkey;
				sortkeys = hsortkeys;
				if (!numkeys) {
					if (op == OP_WORKSORT) {
						execerror();
						return sqlerrnum(SQLERR_EXEC_BADPGM);
					}
					sortkeys = &workkeyptr;
				}
				else {
					if (hsortkeys == NULL || numkeys + 2 > numsortkeys) {
						execerror();
						return sqlerrnum(SQLERR_EXEC_BADPGM);
					}
					if (op == OP_WORKUNIQUE) {
						for (i1 = 0; i1 < numkeys && (*hsortkeys)[i1].end <= wks1->rsrowbytes; i1++);
						if (i1 < numkeys) {
							sortkeys = &workkeyptr;
							numkeys = 0;
						}
					}
				}
				if (op == OP_WORKUNIQUE) {
					(*sortkeys)[numkeys].start = 0;
					(*sortkeys)[numkeys].end = wks1->rsrowbytes;
					(*sortkeys)[numkeys++].typeflg = SIO_ASCEND;
				}
				(*sortkeys)[numkeys].start = wks1->rowbytes - 6;
				(*sortkeys)[numkeys].end = wks1->rowbytes;
				(*sortkeys)[numkeys++].typeflg = SIO_POSITION | SIO_ASCEND;

				if (sioinit(wks1->rowbytes, numkeys, sortkeys, workdir, NULL, 0, 0, NULL)) {
					memfree((UCHAR **) hsortkeys);
					hsortkeys = NULL;
					execerror();
					return sqlerrnummsg(SQLERR_INTERNAL, "sort initialization failed", siogeterr());
				}
				for (i1 = 0; i1 < wks1->rowcount; i1++) {
					if (i1 < wks1->memrowoffset || i1 >= wks1->memrowoffset + wks1->memrowalloc) {
						wks1->rowid = i1 + 1;
						if (readwks(worksetnum, op1 + 1) < 0) {
							execerror();
							return -1;
						}
					}
					ptr1 = sioputstart();
					memcpy(ptr1, *wks1->rows + (i1 - wks1->memrowoffset) * wks1->rowbytes, wks1->rowbytes);
					mscoffto6x(i1, ptr1 + wks1->rowbytes - 6);
					if (sioputend()) {
						sioexit();
						memfree((UCHAR **) hsortkeys);
						hsortkeys = NULL;
						execerror();
						return sqlerrnummsg(SQLERR_INTERNAL, "sort writing failed", siogeterr());
					}
					/* sioputend may call fioopen */
					wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset + op1;
				}
				wks1->rowcount = wks1->memrowoffset = 0;
				for (i1 = 0; ; ) {
					i2 = sioget(&ptr1);
					if (i2) {
						sioexit();
						if (i2 == 1) break;
						memfree((UCHAR **) hsortkeys);
						hsortkeys = NULL;
						execerror();
						return sqlerrnummsg(SQLERR_INTERNAL, "sort reading failed", siogeterr());
					}
					if (op == OP_WORKUNIQUE && i1 &&
						!memcmp(ptr1, *wks1->rows + (i1 - 1 - wks1->memrowoffset) * wks1->rowbytes, wks1->rsrowbytes)) continue;
					if (i1 >= wks1->memrowoffset + wks1->memrowalloc) {
						if (writedirtywks(worksetnum, op1 + 1) < 0) {
							sioexit();
							memfree((UCHAR **) hsortkeys);
							hsortkeys = NULL;
							execerror();
							return -1;
						}
						wks1->memrowoffset = i1;
					}
					memcpy(*wks1->rows + (i1 - wks1->memrowoffset) * wks1->rowbytes, ptr1, wks1->rowbytes);
					wks1->rowid = wks1->rowcount = ++i1;
					wks1->bufdirtyflag = TRUE;
				}
				sioexit();
				if (vars[op2 - 1] && sortkeys != hsortkeys) {
					/* first sort was to create unique result set, now do the real sort */
					if (wks1->rowcount > wks1->memrowalloc && writedirtywks(worksetnum, op1 + 1) < 0) {
						memfree((UCHAR **) hsortkeys);
						hsortkeys = NULL;
						execerror();
						return -1;
					}
					numkeys = vars[op2 - 1];
					(*hsortkeys)[numkeys].start = wks1->rowbytes - 6;
					(*hsortkeys)[numkeys].end = wks1->rowbytes;
					(*hsortkeys)[numkeys++].typeflg = SIO_POSITION;

					if (sioinit(wks1->rowbytes, numkeys, hsortkeys, workdir, NULL, 0, 0, NULL)) {
						memfree((UCHAR **) hsortkeys);
						hsortkeys = NULL;
						execerror();
						return sqlerrnummsg(SQLERR_INTERNAL, "sort initialization failed", siogeterr());
					}
					for (i1 = 0; i1 < wks1->rowcount; i1++) {
						if (i1 < wks1->memrowoffset || i1 >= wks1->memrowoffset + wks1->memrowalloc) {
							wks1->rowid = i1 + 1;
							if (readwks(worksetnum, op1 + 1) < 0) {
								execerror();
								return -1;
							}
						}
						ptr1 = sioputstart();
						memcpy(ptr1, *wks1->rows + (i1 - wks1->memrowoffset) * wks1->rowbytes, wks1->rowbytes);
						mscoffto6x(i1, ptr1 + wks1->rowbytes - 6);
						if (sioputend()) {
							sioexit();
							memfree((UCHAR **) hsortkeys);
							hsortkeys = NULL;
							execerror();
							return sqlerrnummsg(SQLERR_INTERNAL, "sort writing failed", siogeterr());
						}
						/* sioputend may call fioopen */
						wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset + op1;
					}
					wks1->rowcount = wks1->memrowoffset = 0;
					for (i1 = 0; ; ) {
						i2 = sioget(&ptr1);
						if (i2) {
							sioexit();
							if (i2 == 1) break;
							memfree((UCHAR **) hsortkeys);
							hsortkeys = NULL;
							execerror();
							return sqlerrnummsg(SQLERR_INTERNAL, "sort reading failed", siogeterr());
						}
						if (i1 >= wks1->memrowoffset + wks1->memrowalloc) {
							if (writedirtywks(worksetnum, op1 + 1) < 0) {
								sioexit();
								memfree((UCHAR **) hsortkeys);
								hsortkeys = NULL;
								execerror();
								return -1;
							}
							wks1->memrowoffset = i1;
						}
						memcpy(*wks1->rows + (i1 - wks1->memrowoffset) * wks1->rowbytes, ptr1, wks1->rowbytes);
						wks1->rowid = wks1->rowcount = ++i1;
						wks1->bufdirtyflag = TRUE;
					}
					sioexit();
				}
			}
			memfree((UCHAR **) hsortkeys);
			hsortkeys = NULL;
			plan = *hplan;
			pcode = *plan->pgm;
			break;
		case OP_SORTSPEC:
		case OP_SORTSPECD:
			if (op2 < 1 || op2 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			i1 = vars[op2 - 1] - 1;
			if (i1 < 0) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			if (hsortkeys == NULL) {
				i2 = i1;
				if (i2 < 8) i2 = 8;
				hsortkeys = (SIOKEY **) memalloc(i2 * sizeof(SIOKEY), MEMFLAGS_ZEROFILL);
				if (hsortkeys == NULL) {
					execerror();
					return sqlerrnum(SQLERR_NOMEM);
				}
				numsortkeys = i2;
			}
			else if (i1 + 2 >= numsortkeys) {
				i2 = memchange((unsigned char **) hsortkeys, (i1 + 8) * sizeof(SIOKEY), MEMFLAGS_ZEROFILL);
				if (i2 == -1) {
					execerror();
					return sqlerrnum(SQLERR_NOMEM);
				}
				numsortkeys = i1 + 8;
			}
			crf1 = getcolref(op1);
			if (crf1 == NULL) {
				execerror();
				return sqlerrnummsg(SQLERR_EXEC_BADCOL, "bad column reference number (OP_SORTSPEC)", NULL);
			}
			(*hsortkeys)[i1].start = crf1->offset;
			(*hsortkeys)[i1].end = crf1->offset + crf1->length;
			if (op == OP_SORTSPEC) (*hsortkeys)[i1].typeflg = SIO_ASCEND;
			else (*hsortkeys)[i1].typeflg = SIO_DESCEND;
			if (crf1->type == TYPE_NUM) (*hsortkeys)[i1].typeflg |= SIO_NUMERIC;

			/* memory may have shifted */
			plan = *hplan;
			pcode = *plan->pgm;
			break;
		case OP_SET:
			if (op1 < 1 || op1 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			vars[op1 - 1] = op2;
			break;
		case OP_INCR:
			if (op1 < 1 || op1 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			vars[op1 - 1] += op2;
			break;
		case OP_MOVE:
			if (op1 < 1 || op1 > maxvar || op2 < 1 || op2 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			vars[op2 - 1] = vars[op1 - 1];
			break;
		case OP_ADD:
			if (op1 < 1 || op1 > maxvar || op2 < 1 || op2 > maxvar || op3 < 1 || op3 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			vars[op3 - 1] = vars[op1 - 1] + vars[op2 - 1];
			break;
		case OP_SUB:
			if (op1 < 1 || op1 > maxvar || op2 < 1 || op2 > maxvar || op3 < 1 || op3 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			vars[op3 - 1] = vars[op1 - 1] - vars[op2 - 1];
			break;
		case OP_MULT:
			if (op1 < 1 || op1 > maxvar || op2 < 1 || op2 > maxvar || op3 < 1 || op3 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			vars[op3 - 1] = vars[op1 - 1] * vars[op2 - 1];
			break;
		case OP_DIV:
			if (op1 < 1 || op1 > maxvar || op2 < 1 || op2 > maxvar || op3 < 1 || op3 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			vars[op3 - 1] = vars[op1 - 1] / vars[op2 - 1];
			break;
		case OP_MOVETOCOL:
			if (op1 < 1 || op1 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			i2 = mscitoa(vars[op1 - 1], work1);
			i1 = strtocolref((UCHAR *) work1, i2, TYPE_NUM, op2);
			if (i1) {
				execerror();
				return i1;
			}
			break;						
		case OP_FPOSTOCOL:
			i2 = mscofftoa(plan->filepos, work1);
			i1 = strtocolref((UCHAR *) work1, i2, TYPE_NUM, op1);
			if (i1) {
				execerror();
				return i1;
			}
			break;						
		case OP_COLMOVE:
			if (colreftostr(op1, 0, &ptr1, &i2, &scale1)) {
				execerror();
				return -1;
			}
			if (strtocolref(ptr1, i2, getcoltype(op1), op2)) {
				execerror();
				return -1;
			}
			break;						
		case OP_COLADD:
			i3 = getcoltype(op1);
			if (i3 < 0) {
				execerror();
				return i3;
			}
			i4 = getcoltype(op2);
			if (i4 < 0) {
				execerror();
				return i4;
			}
			if (i3 == TYPE_DATE || i3 == TYPE_TIME || i3 == TYPE_TIMESTAMP || i4 == TYPE_DATE || i4 == TYPE_TIME || i4 == TYPE_TIMESTAMP) {
#if 0
/*** CODE: SUPPORT THIS ***/
				i1 = datetimeadd(op1, op2, op3);
				if (i1) {
					execerror();
					return i1;
				}
#endif
				execerror();
				return sqlerrnummsg(SQLERR_EXEC_BADCOL, "date/time/timestamp type invalid for ADD operation", NULL);
				break;
			}
			if (colreftostr(op1, 0, &ptr1, &i2, &scale1)) {
				execerror();
				return -1;
			}
#if 0
/*** CODE: TEST CODE TO CONVERT NULL TO ZERO ***/
			if (!i2) {
				ptr1 = "0";
				i2 = 1;
			}
#endif
			if (i2) {
				if ((size_t) i2 >= sizeof(work1)) {
					execerror();
					return sqlerrnummsg(SQLERR_EXEC_BADCOL, "column string too large for ADD operation", NULL);
				}
				memcpy(work1, ptr1, i2);
				work1[i2] = 0;
				if (colreftostr(op2, 0, &ptr1, &i2, &scale1)) {
					execerror();
					return -1;
				}
#if 0
/*** CODE: TEST CODE TO CONVERT NULL TO ZERO ***/
				if (!i2) {
					ptr1 = "0";
					i2 = 1;
				}
#endif
				if (i2) {
					if ((size_t) i2 >= sizeof(work2)) {
						execerror();
						return sqlerrnummsg(SQLERR_EXEC_BADCOL, "column string too large for ADD operation", NULL);
					}
					memcpy(work2, ptr1, i2);
					work2[i2] = 0;
					binfo(work1, &i1, &i2, &i5);
					binfo(work2, &i3, &i4, &i5);
					if (i3 > i1) i1 = i3;
					if (i4 > i2) i2 = i4;
					i1 += 2;
					bform(work3, i1, i2);
					if (i2) i1 += i2 + 1;
					bmove(work1, work3);
					badd(work2, work3);
				}
				else i1 = 0;
			}
			else i1 = 0;
			i1 = strtocolref((unsigned char *) work3, i1, TYPE_LITERAL, op3);
			if (i1) {
				execerror();
				return i1;
			}
			break;
		case OP_COLSUB:
			i3 = getcoltype(op1);
			if (i3 < 0) {
				execerror();
				return i3;
			}
			i4 = getcoltype(op2);
			if (i4 < 0) {
				execerror();
				return i4;
			}
			if (i3 == TYPE_DATE || i3 == TYPE_TIME || i3 == TYPE_TIMESTAMP || i4 == TYPE_DATE || i4 == TYPE_TIME || i4 == TYPE_TIMESTAMP) {
#if 0
/*** CODE: SUPPORT THIS ***/
				i1 = datetimesub(op1, op2, op3);
				if (i1) {
					execerror();
					return i1;
				}
#endif
				execerror();
				return sqlerrnummsg(SQLERR_EXEC_BADCOL, "date/time/timestamp type invalid for SUBTRACT operation", NULL);
			}
			if (colreftostr(op1, 0, &ptr1, &i2, &scale1)) {
				execerror();
				return -1;
			}
			if (i2) {
				if ((size_t) i2 >= sizeof(work1)) {
					execerror();
					return sqlerrnummsg(SQLERR_EXEC_BADCOL, "column string too large for SUBTRACT operation", NULL);
				}
				memcpy(work1, ptr1, i2);
				work1[i2] = 0;
				if (colreftostr(op2, 0, &ptr1, &i2, &scale1)) {
					execerror();
					return -1;
				}
				if (i2) {
					if ((size_t) i2 >= sizeof(work2)) {
						execerror();
						return sqlerrnummsg(SQLERR_EXEC_BADCOL, "column string too large for SUBTRACT operation", NULL);
					}
					memcpy(work2, ptr1, i2);
					work2[i2] = 0;
					binfo(work1, &i1, &i2, &i5);
					binfo(work2, &i3, &i4, &i5);
					if (i3 > i1) i1 = i3;
					if (i4 > i2) i2 = i4;
					i1 += 2;
					bform(work3, i1, i2);
					if (i2) i1 += i2 + 1;
					bmove(work1, work3);
					bsub(work2, work3);
				}
				else i1 = 0;
			}
			else i1 = 0;
			i1 = strtocolref((unsigned char *) work3, i1, TYPE_LITERAL, op3);
			if (i1) {
				execerror();
				return i1;
			}
			break;
		case OP_COLMULT:
			i3 = getcoltype(op1);
			if (i3 < 0) {
				execerror();
				return i3;
			}
			i4 = getcoltype(op2);
			if (i4 < 0) {
				execerror();
				return i4;
			}
			if (i3 == TYPE_DATE || i3 == TYPE_TIME || i3 == TYPE_TIMESTAMP || i4 == TYPE_DATE || i4 == TYPE_TIME || i4 == TYPE_TIMESTAMP) {
				execerror();
				return sqlerrnummsg(SQLERR_EXEC_BADCOL, "date/time/timestamp type invalid for MULTIPLY operation", NULL);
			}
			if (colreftostr(op1, 0, &ptr1, &i2, &scale1)) {
				execerror();
				return -1;
			}
			if (i2) {
				if ((size_t) i2 >= sizeof(work1)) {
					execerror();
					return sqlerrnummsg(SQLERR_EXEC_BADCOL, "column string too large for MULTIPLY operation", NULL);
				}
				memcpy(work1, ptr1, i2);
				work1[i2] = 0;
				if (colreftostr(op2, 0, &ptr1, &i2, &scale1)) {
					execerror();
					return -1;
				}
				if (i2) {
					if ((size_t) i2 >= sizeof(work2)) {
						execerror();
						return sqlerrnummsg(SQLERR_EXEC_BADCOL, "column string too large for MULTIPLY operation", NULL);
					}
					memcpy(work2, ptr1, i2);
					work2[i2] = 0;
					binfo(work1, &i1, &i2, &i5);
					binfo(work2, &i3, &i4, &i5);
					i1 += i3;
					i2 += i4;
					bform(work3, i1, i2);
					if (i2) i1 += i2 + 1;
					bmove(work1, work3);
					bmult(work2, work3);
				}
				else i1 = 0;
			}
			else i1 = 0;
			i1 = strtocolref((unsigned char *) work3, i1, TYPE_LITERAL, op3);
			if (i1) {
				execerror();
				return i1;
			}
			break;
		case OP_COLDIV:
			i3 = getcoltype(op1);
			if (i3 < 0) {
				execerror();
				return i3;
			}
			i4 = getcoltype(op2);
			if (i4 < 0) {
				execerror();
				return i4;
			}
			if (i3 == TYPE_DATE || i3 == TYPE_TIME || i3 == TYPE_TIMESTAMP || i4 == TYPE_DATE || i4 == TYPE_TIME || i4 == TYPE_TIMESTAMP) {
				execerror();
				return sqlerrnummsg(SQLERR_EXEC_BADCOL, "date/time/timestamp type invalid for DIVIDE operation", NULL);
			}
			if (colreftostr(op1, 0, &ptr1, &i2, &scale1)) {
				execerror();
				return -1;
			}
			if (i2) {
				if ((size_t) i2 >= sizeof(work1)) {
					execerror();
					return sqlerrnummsg(SQLERR_EXEC_BADCOL, "column string too large for DIVIDE operation", NULL);
				}
				memcpy(work1, ptr1, i2);
				work1[i2] = 0;
				if (colreftostr(op2, 0, &ptr1, &i2, &scale1)) {
					execerror();
					return -1;
				}
				if (i2) {
					if ((size_t) i2 >= sizeof(work2)) {
						execerror();
						return sqlerrnummsg(SQLERR_EXEC_BADCOL, "column string too large for DIVIDE operation", NULL);
					}
					memcpy(work2, ptr1, i2);
					work2[i2] = 0;
					binfo(work1, &i1, &i2, &i5);
					binfo(work2, &i3, &i4, &i5);
					i1 += i4;
					i2 = i3 + i2;
/*** NOTE: OTHER ALTERNATIVES FOR I2, ARE: if (i4 > i2) i2 = i4 OR i2 += i4, BUT BMATH DOES ROUND ***/
					bform(work3, i1, i2);
					if (i2) i1 += i2 + 1;
					bmove(work1, work3);
					bdiv(work2, work3);
				}
				else i1 = 0;
			}
			else i1 = 0;
			i1 = strtocolref((unsigned char *) work3, i1, TYPE_LITERAL, op3);
			if (i1) {
				execerror();
				return i1;
			}
			break;
		case OP_COLNEGATE:
			i3 = getcoltype(op1);
			if (i3 < 0) {
				execerror();
				return i3;
			}
			if (i3 == TYPE_DATE || i3 == TYPE_TIME || i3 == TYPE_TIMESTAMP) {
				execerror();
				return sqlerrnummsg(SQLERR_EXEC_BADCOL, "date/time/timestamp type invalid for NEGATE operation", NULL);
			}
			if (colreftostr(op1, 0, &ptr1, &i2, &scale1)) {
				execerror();
				return -1;
			}
			if (i2) {
				if ((size_t) i2 >= sizeof(work1)) {
					execerror();
					return sqlerrnummsg(SQLERR_EXEC_BADCOL, "column string too large for NEGATE operation", NULL);
				}
				memcpy(work1, ptr1, i2);
				work1[i2] = 0;
				binfo(work1, &i1, &i2, &i5);
				i1 += 2;
				bform(work3, i1, i2);
				if (i2) i1 += i2 + 1;
				bsub(work1, work3);
			}
			else i1 = 0;
			i1 = strtocolref((unsigned char *) work3, i1, TYPE_LITERAL, op2);
			if (i1) {
				execerror();
				return i1;
			}
			break;
		case OP_COLLOWER:
			i1 = getcoltype(op2);
			if (i1 < 0) {
				execerror();
				return i1;
			}
			if (i1 != TYPE_CHAR) {
				execerror();
				return sqlerrnummsg(SQLERR_EXEC_BADCOL, "LOWER operation requires character destination", NULL);
			}
			if (colreftostr(op1, 0, &ptr1, &i1, &scale1)) {
				execerror();
				return -1;
			}
			if (colreftostr(op2, COLREFTOSTR_DIRECT, &ptr2, &i2, &scale1)) {
				execerror();
				return -1;
			}
/*** CODE: NEED TO HAVE 'TOLOWER' TABLE FOR INTERNATIONAL CHARS ***/
			for (i3 = 0; i3 < i1 && i3 < i2; i3++) ptr2[i3] = (UCHAR) tolower(ptr1[i3]);
			i3 = changecolsize(op2, i3, "COLLOWER");
			if (i3 < 0) { 
				execerror();
				return i3;
			}			
			break;
		case OP_COLUPPER:
			i1 = getcoltype(op2);
			if (i1 < 0) {
				execerror();
				return i1;
			}
			if (i1 != TYPE_CHAR) {
				execerror();
				return sqlerrnummsg(SQLERR_EXEC_BADCOL, "UPPER operation requires character destination", NULL);
			}
			if (colreftostr(op1, 0, &ptr1, &i1, &scale1)) {
				execerror();
				return -1;
			}
			if (colreftostr(op2, COLREFTOSTR_DIRECT, &ptr2, &i2, &scale1)) {
				execerror();
				return -1;
			}
/*** CODE: NEED TO HAVE 'TOUPPER' TABLE FOR INTERNATIONAL CHARS ***/
			for (i3 = 0; i3 < i1 && i3 < i2; i3++) ptr2[i3] = (UCHAR) toupper(ptr1[i3]);
			i3 = changecolsize(op2, i3, "COLUPPER");
			if (i3 < 0) { 
				execerror();
				return i3;
			}
			break;
		case OP_COLTRIM_L:
		case OP_COLTRIM_T:
		case OP_COLTRIM_B:
			if (colreftostr(op1, 0, &ptr1, &i1, &scale1)) { /* trim char */
				execerror();
				return -1;
			}
			if (i1 <= 0) {
				execerror();
				return sqlerrnummsg(SQLERR_EXEC_BADCOL, "invalid character for TRIM", NULL);				
			}
			c1 = ptr1[0];
			if (colreftostr(op2, 0, &ptr1, &len, &scale1)) { /* source value */
				execerror();
				return -1;
			}
			if (op == OP_COLTRIM_L || op == OP_COLTRIM_B) {
				/* trim left */
				while (*ptr1) {
					if (*ptr1 == c1) {
						ptr1++;
						len--;
					}
					else break;
				}
			}
			if (op == OP_COLTRIM_T || op == OP_COLTRIM_B) {
				/* trim right */
				for (i1 = len - 1, i2 = 0; i1 >= 0; i1--) {
					if (ptr1[i1] == c1) len--;
					else break; 
				}
			}
			if (colreftostr(op3, 0, &ptr2, &i2, &scale2)) { /* destination */
				execerror();
				return -1;
			}				
			memcpy(ptr2, ptr1, len);
			i3 = changecolsize(op3, len, "COLTRIM");
			if (i3 < 0) { 
				execerror();
				return i3;
			}
			break;
		case OP_COLSUBSTR_POS:
			if (colreftostr(op1, 0, &ptr1, &i1, &scale1)) { /* source */
				execerror();
				return -1;
			}
			if (colreftostr(op2, 0, &ptr2, &i2, &scale2)) { /* offset */
				execerror();
				return -1;
			}
			memcpy(work, ptr2, i2);
			work[i2] = 0;
			i3 = atoi((CHAR *)work);
			if (i3 < 0) {
				/* convert negative position to positive */
				i3 = (i3 + i1) + 1;
			}
			if (i3 <= 0 || i3 >= i1) {
				execerror();
				return sqlerrnummsg(SQLERR_EXEC_BADCOL, "position for SUBSTRING operation was invalid", NULL);
			}
			i3--;
			if (colreftostr(op3, 0, &ptr2, &i2, &scale2)) { /* destination */ 
				execerror();
				return -1;
			}					
			memcpy(ptr2, ptr1 + i3, i1 - i3);
			ptr2[i1 - i3] = 0;
			break;
		case OP_COLSUBSTR_LEN:
			if (colreftostr(op1, 0, &ptr1, &i1, &scale1)) { /* source */
				execerror();
				return -1;
			}
			if (colreftostr(op2, 0, &ptr2, &i2, &scale2)) { /* length */
				execerror();
				return -1;
			}
			memcpy(work, ptr2, i2);
			work[i2] = 0;
			i3 = atoi((CHAR *)work);
			if (i3 == 0) {
				i3 = (INT)strlen((CHAR *)ptr1); /* i3=0 means length was not specified */
			}
			if (i3 < 0) {
				execerror();
				return sqlerrnummsg(SQLERR_EXEC_BADCOL, "length for SUBSTRING operation was invalid", NULL);
			}
			if (colreftostr(op3, 0, &ptr2, &i2, &scale2)) { /* destination */
				execerror();
				return -1;
			}				
			memcpy(ptr2, ptr1, i3);		
			i2 = changecolsize(op3, i3, "COLSUBSTR");
			if (i2 < 0) { 
				execerror();
				return i2;
			}
			break;
		case OP_COLCONCAT:
			type1 = getcoltype(op1);
			if (type1 < 0) {
				execerror();
				return type1;
			}
			if (colreftostr(op1, (type1 == TYPE_LITERAL || type1 == TYPE_DATE || type1 == TYPE_TIME || type1 == TYPE_TIMESTAMP) ? 0 : COLREFTOSTR_DIRECT, &ptr1, &length1, &scale1)) {
				execerror();
				return -1;
			}
			type2 = getcoltype(op2);
			if (type2 < 0) {
				execerror();
				return type2;
			}
			if (colreftostr(op2, (type2 == TYPE_LITERAL || type2 == TYPE_DATE || type2 == TYPE_TIME || type2 == TYPE_TIMESTAMP) ? 0 : COLREFTOSTR_DIRECT, &ptr2, &length2, &scale2)) {
				execerror();
				return -1;
			}	
			memcpy(work3, ptr1, length1);
			memcpy(work3 + length1, ptr2, length2);
			length2 += length1;
			/* Grow column size if necessary before copying results, previous operations like SUBSTRING */
			/* or TRIM might have reduced column size.  Memory has already been allocated in fssql4 for */
			/* the biggest size result possible from CONCAT, so we can grow and shrink column size at will */
			/* and size will always be in range of allocated memory. */		
			i1 = changecolsize(op3, length2, "COLCONCAT");
			if (i1 < 0) { 
				execerror();
				return i1;
			}
			i1 = strtocolref((unsigned char *) work3, length2, TYPE_CHAR, op3);
			if (i1) {
				execerror();
				return i1;
			}
			break;
		case OP_COLCAST:
			type1 = getcoltype(op1);
			if (type1 < 0) {
				execerror();
				return type1;
			}
			if (colreftostr(op1, (type1 == TYPE_LITERAL || type1 == TYPE_DATE || type1 == TYPE_TIME || type1 == TYPE_TIMESTAMP) ? 0 : COLREFTOSTR_DIRECT, &ptr1, &length1, &scale1)) {
				execerror();
				return -1;
			}
			type2 = getcoltype(op2);
			if (type2 < 0) {
				execerror();
				return type2;
			}
			if (colreftostr(op2, COLREFTOSTR_DIRECT, &ptr2, &length2, &scale2)) {
				execerror();
				return -1;
			}
			for (i1 = 0; i1 < length2; i1++) ptr2[i1] = ' '; /* clear destination */
			switch (type1) {
				case TYPE_NUM:
				switch (type2) {
					case TYPE_NUM:
						/* NOTE: No rounding is done, only truncation */
						/* NOTE: This code is duplicated for CAST TYPE_CHAR -> TYPE_NUM, fix bugs in both places */
						if (scale2) {
							if (scale1 >= scale2) { /* i.e. 120.250 ->  120.2 */
								/* copy digits to the right of the decimal */
								for (i3 = 1, i4 = scale1 - scale2; i3 <= scale2; i3++) {
									ptr2[length2 - i3] = ptr1[length1 - (i3 + i4)];
								}
								ptr2[length2 - i3] = '.';
							}
							else { /* i.e. 120.2 ->  120.200 */
								/* use 0's for extra digits in destination */
								i3 = i4 = scale2 - scale1;
								while (i3--) ptr2[length2 - (i3 + 1)] = '0'; 
								/* copy digits to the right of the decimal */
								for (i3 = 1; i3 <= scale1; i3++) {
									ptr2[length2 - (i3 + i4)] = ptr1[length1 - i3];
								}
								ptr2[length2 - (i3 + i4)] = '.';								
							}
						}
						for (i1 = 0; i1 < length1; i1++) if (ptr1[i1] != ' ') break; /* i1 = leading blanks */
						/* i1 = leading blanks */
						i3 = (length1 - scale1) - 1; /* num digits on left of source, - 1 to convert to position */
						i4 = (length2 - scale2) - 1; /* num digits on left of destination, - 1 to convert to position */
						if (scale1) i3--; /* for decimal */
						if (scale2) i4--; /* for decimal */
						if (i3 - i1 > i4) {
							execerror();
							return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast not allowed because numeric data will be lost", NULL);
						}
						/* copy digits on left side excluding leading blanks of source */
						for (; i3 >= i1; i3--, i4--) ptr2[i4] = ptr1[i3];
						break;
					case TYPE_CHAR:
						for (i1 = 0; i1 < length1; i1++) if (ptr1[i1] != ' ') break; /* i1 = leading blanks */
						if (length1 - i1 > length2) {
							execerror();
							return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast not allowed because numeric data will be lost", NULL);
						}
						for (i3 = length1 - 1, i4 = length2 - 1; i3 >= i1; i3--, i4--) ptr2[i4] = ptr1[i3];
						ptr2[length2] = 0;
						break;
					case TYPE_DATE:
						execerror();
						return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast from NUMERIC to DATE is not legal", NULL);						
					case TYPE_TIME:
						execerror();
						return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast from NUMERIC to TIME is not legal", NULL);
					case TYPE_TIMESTAMP:
						execerror();
						return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast from NUMERIC to TIMESTAMP is not legal", NULL);
				}
				break;
				case TYPE_LITERAL:
				case TYPE_CHAR:	
				switch (type2) {
					case TYPE_NUM:
						for (i1 = 0; i1 < length1; i1++) if (ptr1[i1] != ' ') break; /* i1 = leading blanks */
						for (i2 = i1, i3 = 0; i2 < length1; i2++) {
							if (!isdigit(ptr1[i2])) {
								if (ptr1[i2] == '.') {
									if (!i3) i3 = i2; /* i3 = position of decimal */
									else {
										execerror();
										return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast to NUMERIC not allowed because source did not contain a valid number", NULL);								
									}
									continue;
								}
								if (i2 == i1 && ptr1[i2] == '-') continue;
								if (ptr1[i2] == ' ') {
									/* check that only trailing blanks remain */
									for (++i2, i4 = 1; i2 < length1; i2++) {
										if (ptr1[i2] != ' ') {
											execerror();
											return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast to NUMERIC not allowed because source did not contain a valid number", NULL);
										}
										i4++; 
									} 
									length1 -= i4; /* trim off trailing blanks */
									break;
								}
								execerror();
								return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast to NUMERIC not allowed because source did not contain a valid number", NULL);								
							}
						}
						if (i3) scale1 = (length1 - i3) - 1;
						else scale1 = 0;
						/* NOTE: No rounding is done, only truncation */
						/* NOTE: The following code is duplicated for CAST TYPE_NUM -> TYPE_NUM, fix bugs in both places */
						if (scale2) {
							if (scale1 >= scale2) { /* i.e. 120.250 ->  120.2 */
								/* copy digits to the right of the decimal */
								for (i3 = 1, i4 = scale1 - scale2; i3 <= scale2; i3++) {
									ptr2[length2 - i3] = ptr1[length1 - (i3 + i4)];
								}
								ptr2[length2 - i3] = '.';
							}
							else { /* i.e. 120.2 ->  120.200 */
								/* use 0's for extra digits in destination */
								i3 = i4 = scale2 - scale1;
								while (i3--) ptr2[length2 - (i3 + 1)] = '0'; 
								/* copy digits to the right of the decimal */
								for (i3 = 1; i3 <= scale1; i3++) {
									ptr2[length2 - (i3 + i4)] = ptr1[length1 - i3];
								}
								ptr2[length2 - (i3 + i4)] = '.';								
							}
						}
						/* i1 = leading blanks */
						i3 = (length1 - scale1) - 1; /* num digits on left of source, - 1 to convert to position */
						i4 = (length2 - scale2) - 1; /* num digits on left of destination, - 1 to convert to position */
						if (scale1) i3--; /* for decimal */
						if (scale2) i4--; /* for decimal */
						if (i3 - i1 > i4) {
							execerror();
							return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast not allowed because numeric data will be lost", NULL);
						}
						/* copy digits on left side excluding leading blanks of source */
						for (; i3 >= i1; i3--, i4--) ptr2[i4] = ptr1[i3];
						break;
					case TYPE_CHAR:	
						for (i1 = 0, i2 = min(length1, length2); i1 < i2; i1++) ptr2[i1] = ptr1[i1];
						break;
					case TYPE_DATE:
						/* DATE 'YYYY-MM-DD', length2 = 10 */
						if (length1 == 10 || length1 == 8 || length1 == 6) {
							for (i1 = i2 = 0; i1 < length1; i1++, i2++) {
								if (length1 == 6 && i2 == 0) {
									/* Y2K -- year xx29 and below are considered 2000, above 29 is considered 1900 */
									if (ptr1[0] <= '2') {
										ptr2[i2++] = '2';
										ptr2[i2++] = '0';
									}
									else {
										ptr2[i2++] = '1';
										ptr2[i2++] = '9';
									}
								}
								if ((length1 == 6 || length1 == 8) && (i2 == 4 || i2 == 7)) ptr2[i2++] = '-';
								if (!isdigit(ptr1[i1])) {
									if (length1 == 6 || length1 == 8 || (length1 == 10 && !(i1 == 4 && ptr1[i1] == '-' || i1 == 7 && ptr1[i1] == '-'))) {
										execerror();
										return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast to DATE not allowed because source did not contain a valid date", NULL);	
									}
								}
								ptr2[i2] = ptr1[i1];
							}
						}
						else {
							execerror();
							return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast to DATE not allowed because source did not contain a valid date", NULL);	
						}
						break;
					case TYPE_TIME:
						/* TIME 'HH:MM:SS.FFF', length2 = 12 */
						if (length1 == 12 || (length1 >= 6 && length1 <= 10)) {
							if (length1 == 10 && isdigit(ptr1[9])) length1 = 9; /* drop last digit */
							for (i1 = i2 = 0; i1 < length1; i1++, i2++) {
								if ((length1 >= 6 && length1 <= 9) && (i2 == 2 || i2 == 5)) ptr2[i2++] = ':';
								else if ((length1 >= 6 && length1 <= 9) && i2 == 8) ptr2[i2++] = '.';
								if (!isdigit(ptr1[i1])) {
									if ((length1 >= 6 && length1 <= 9) || (length1 == 12 && !((i1 == 2 || i1 == 5) && ptr1[i1] == ':' || i1 == 8 && ptr1[i1] == '.'))) {
										execerror();
										return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast to TIME not allowed because source did not contain a valid time", NULL);	
									}
								}
								ptr2[i2] = ptr1[i1];
							}
							if (length1 == 6) ptr2[i2++] = '.';
							if (length1 == 6 || length1 == 7 || length1 == 8) ptr2[i2++] = '0';
							if (length1 == 6 || length1 == 7) ptr2[i2++] = '0';
							if (length1 == 6) ptr2[i2++] = '0';
						}
						else {
							execerror();
							return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast to TIME not allowed because source did not contain a valid time", NULL);	
						}
						break;
					case TYPE_TIMESTAMP:
						/* TIMESTAMP 'YYYY-MM-DD HH:MM:SS.FFF', length2 = 23 */	
						if (length1 == 23 || (length1 >= 14 && length1 <= 18)) {
							if (length1 == 18 && isdigit(ptr1[17])) length1 = 17; /* drop last digit */
							for (i1 = i2 = 0; i1 < length1; i1++, i2++) {
								if ((length1 >= 14 && length1 <= 17) && (i2 == 4 || i2 == 7)) ptr2[i2++] = '-';
								else if ((length1 >= 14 && length1 <= 17) && i2 == 10) ptr2[i2++] = ' ';
								else if ((length1 >= 14 && length1 <= 17) && (i2 == 13 || i2 == 16)) ptr2[i2++] = ':';
								else if ((length1 >= 14 && length1 <= 17) && i2 == 19) ptr2[i2++] = '.';
								if (!isdigit(ptr1[i1])) {
									if ((length1 >= 14 && length1 <= 17) || (length1 == 23 && !((i1 == 4 || i1 == 7) && ptr1[i1] == '-' || 
											(i1 == 10 && ptr1[i1] == ' ') || (i1 == 13 || i1 == 16) && ptr1[i1] == ':') || i1 == 19 && ptr1[i1] == '.')) {
										execerror();
										return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast to TIME not allowed because source did not contain a valid timestamp", NULL);	
									}
								}
								ptr2[i2] = ptr1[i1];
							}
							
							if (length1 == 14) ptr2[i2++] = '.';
							if (length1 == 14 || length1 == 15 || length1 == 16) ptr2[i2++] = '0';
							if (length1 == 14 || length1 == 15) ptr2[i2++] = '0';
							if (length1 == 14) ptr2[i2++] = '0';
							
						}
						else {
							execerror();
							return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast to TIMESTAMP not allowed because source did not contain a valid timestamp", NULL);	
						}
						break;
				}
				break;
				case TYPE_DATE:
				switch (type2) {
					case TYPE_NUM:
						execerror();
						return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast from DATE to NUMERIC is not legal", NULL);
					case TYPE_TIME:
						execerror();
						return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast from DATE to TIME is not legal", NULL);
					case TYPE_CHAR:
						if (length1 > length2) {
							execerror();
							return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast to CHAR not allowed because date information will be lost", NULL);
						}
						memcpy(ptr2, ptr1, length1);
						break;
					case TYPE_DATE:
						memcpy(ptr2, ptr1, length1);
						break;
					case TYPE_TIMESTAMP:
						memcpy(ptr2, ptr1, length1);
						memcpy(ptr2 + length1, " 00:00:00.000", 13); 
						break;
				}
				break;
				case TYPE_TIME:
				switch (type2) {
					case TYPE_NUM:
						execerror();
						return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast from TIME to NUMERIC is not legal", NULL);						
					case TYPE_DATE:
						execerror();
						return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast from TIME to DATE is not legal", NULL);
					case TYPE_CHAR:
						if (length1 > length2) {
							execerror();
							return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast to CHAR not allowed because time information will be lost", NULL);
						}
						memcpy(ptr2, ptr1, length1);			
						break;
					case TYPE_TIME:
						memcpy(ptr2, ptr1, length1);					
						break;
					case TYPE_TIMESTAMP:
						/* Use current date for the date */	
						msctimestamp(work);
						ptr2[0] = work[0];
						ptr2[1] = work[1];
						ptr2[2] = work[2];
						ptr2[3] = work[3];
						ptr2[4] = '-';
						ptr2[5] = work[4];
						ptr2[6] = work[5];
						ptr2[7] = '-';
						ptr2[8] = work[6];
						ptr2[9] = work[7];
						memcpy(ptr2 + 11, ptr1, 12);						
						break;
				}
				break;
				case TYPE_TIMESTAMP:
				switch (type2) {
					case TYPE_NUM:
						execerror();
						return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast from TIMESTAMP to NUMERIC is not legal", NULL);
						break;
					case TYPE_CHAR:
						if (length1 > length2) {
							execerror();
							return sqlerrnummsg(SQLERR_EXEC_BADCOL, "cast to CHAR not allowed because timestamp information will be lost", NULL);
						}
						for (i1 = 0; i1 < length1; i1++) ptr2[i1] = ptr1[i1];	
						break;
					case TYPE_DATE:
						memcpy(ptr2, ptr1, 10);					
						break;
					case TYPE_TIME:
						memcpy(ptr2, ptr1 + 11, 12);						
						break;
					case TYPE_TIMESTAMP:
						memcpy(ptr2, ptr1, length1);						
						break;
				}
				break;
			}
			break;
		case OP_COLNULL:
			i1 = strtocolref((unsigned char *) work1, 0, TYPE_LITERAL, op1);
			if (i1) {
				execerror();
				return i1;
			}
			break;
		case OP_COLTEST:
			if (op2 < 1 || op2 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			if (colreftostr(op1, 0, &ptr1, &i2, &scale1)) {
				execerror();
				return -1;
			}
			i3 = getcoltype(op1);
			if (i3 < 0) {
				execerror();
				return i3;
			}
			i4 = 0;
			if (i3 == TYPE_CHAR || i3 == TYPE_LITERAL) {
				for (i1 = 0; i1 < i2 && ptr1[i1] == ' '; i1++);
				if (i1 < i2) {
					if (i3 == TYPE_LITERAL) {
						while (i1 < i2 && ptr1[i1] == '0') i1++;
						if (i1 < i2) i4 = 1;
					}
					else i4 = 1;
				}
			}
			else if (i3 == TYPE_DATE || i3 == TYPE_TIME || i3 == TYPE_TIMESTAMP) {
				for (i1 = 0; i1 < i2 && ptr1[i1] < '1' && ptr1[i1] > '9'; i1++);
				if (i1 < i2) i4 = 1;
			}
			else if (i2) {
				if ((size_t) i2 >= sizeof(work1)) {
					execerror();
					return sqlerrnummsg(SQLERR_EXEC_BADCOL, "column string too large for TEST operation", NULL);
				}
				memcpy(work1, ptr1, i2);
				work1[i2] = 0;
				if (!biszero(work1)) i4 = 1;
			}
			vars[op2 - 1] = i4;
			break;
		case OP_COLCOMPARE:
			if (colreftostr(op1, 0, &ptr1, &i1, &scale1) || colreftostr(op2, 0, &ptr2, &i2, &scale2)) {
				execerror();
				return -1;
			}
			if (op3 < 1 || op3 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			i3 = getcoltype(op1);
			if (i3 < 0) {
				execerror();
				return i3;
			}
			i4 = getcoltype(op2);
			if (i4 < 0) {
				execerror();
				return i4;
			}
/*** NOTE: SINCE WE DO NOT SUPPORT "UNKNOWN", THIS IS THE BEST WE CAN DO.  UNKNOWN WOULD ***/
/***       REQUIRE 6 DIFFERENT OP_COLCOMPAREs SO THAT TRUE, FALSE, UNKNOWN CAN BE RETURNED ***/
			if (!i1 && i3 != TYPE_CHAR && i3 != TYPE_LITERAL) {  /* first element is null */
				if (!i2 && i4 != TYPE_CHAR && i4 != TYPE_LITERAL) i4 = 0;  /* both are null */
				else i4 = -1;  /* null is considered less than anything */
			}
			else if (!i2 && i4 != TYPE_CHAR && i4 != TYPE_LITERAL) i4 = 1;  /* second element is null */
			else {
				if (i3 == TYPE_DATE || i3 == TYPE_TIME || i3 == TYPE_TIMESTAMP) {
					if (i3 != i4) {
						if (i4 != TYPE_TIMESTAMP) {
							i2 = cvttodatetime(i4, ptr2, i2, i3, (UCHAR *) work1);
							if (i2 < 0) {
								execerror();
								return -1;
							}
							ptr2 = (UCHAR *) work1;
						}
						else {
							i1 = cvttodatetime(i3, ptr1, i1, TYPE_TIMESTAMP, (UCHAR *) work1);
							if (i1 < 0) {
								execerror();
								return -1;
							}
							ptr1 = (UCHAR *) work1;
						}
					}
					i3 = i4 = TYPE_CHAR;
				}
				else if (i4 == TYPE_DATE || i4 == TYPE_TIME || i4 == TYPE_TIMESTAMP) {
					i1 = cvttodatetime(i3, ptr1, i1, i4, (UCHAR *) work1);
					if (i1 < 0) {
						execerror();
						return -1;
					}
					ptr1 = (UCHAR *) work1;
					i3 = i4 = TYPE_CHAR;
				}
				if ((i3 == TYPE_CHAR || i3 == TYPE_LITERAL) && (i4 == TYPE_CHAR || i4 == TYPE_LITERAL)) {
					i4 = memcmp(ptr1, ptr2, min(i1, i2));
					if (!i4 && i1 != i2) {
						while (i2 < i1 && ptr1[i2] == ' ') i2++;
						while (i1 < i2 && ptr2[i1] == ' ') i1++;
						i4 = i1 - i2;
					}
				}
				else {  /* numeric comparison */
					if ((size_t) i1 >= sizeof(work1) || (size_t) i2 >= sizeof(work2)) {
						execerror();
						return sqlerrnummsg(SQLERR_EXEC_BADCOL, "column string too large for COMPARE operation", NULL);
					}
					memcpy(work1, ptr1, i1);
					work1[i1] = 0;
					memcpy(work2, ptr2, i2);
					work2[i2] = 0;
					i4 = bcomp(work2, work1);
				}
			}
			vars[op3 - 1] = i4;
			break;
		case OP_COLISNULL:
			if (op2 < 1 || op2 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			if (colreftostr(op1, 0, &ptr1, &i2, &scale1)) {
				execerror();
				return -1;
			}
#if 0
/*** NOTE: USE BELOW IF CHARS MAY BE NULL ***/
			for (i1 = 0; i1 < i2 && ptr1[i1] == ' '; i1++);
			vars[op2 - 1] = (i1 == i2) ? 1 : 0;
#else
			vars[op2 - 1] = (!i2) ? 1 : 0;
#endif
			break;
		case OP_COLLIKE:
			if (colreftostr(op1, 0, &ptr1, &i3, &scale1) || colreftostr(op2, 0, &ptr2, &i4, &scale2)) {
				execerror();
				return -1;
			}
			if (op3 < 1 || op3 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			/* return value is similar to colcompare and not true/false as might expect. */
			vars[op3 - 1] = (matchstring(ptr1, i3, ptr2, i4)) ? 0 : 1;
			break;
		case OP_COLBININCR:
			if (colreftostr(op1, COLREFTOSTR_DIRECT, &ptr1, &i2, &scale1)) {
				execerror();
				return -1;
			}
			if (i2) {
/*** CODE: SHOULD USE COLLATE TABLE TO DO THIS. ***/
/***       FOR NOW ASSUME, THE ADDITION / SUBTRACTION DOES NOT ***/
/***       CAUSE UNDER / OVER FLOW ***/
				ptr1[i2 - 1] = (unsigned char)(ptr1[i2 - 1] + op2);
			}
			break;						
		case OP_COLBINSET:
			if (colreftostr(op1, COLREFTOSTR_DIRECT, &ptr1, &i2, &scale1)) {
				execerror();
				return -1;
			}
/*** CODE: SHOULD USE COLLATE TABLE TO DO THIS. ***/
			while (--i2 >= 0) ptr1[i2] = (unsigned char) op2;
			break;						
		case OP_GOTO:
			if (op1 < 0 || op1 > maxpcount) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			pcount = op1;
			continue;
		case OP_GOTOIFNOTZERO:
			if (op1 < 0 || op1 > maxpcount || op2 < 1 || op2 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			if (vars[op2 - 1]) {
				pcount = op1;
				continue;
			}
			break;
		case OP_GOTOIFZERO:
			if (op1 < 0 || op1 > maxpcount || op2 < 1 || op2 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			if (!vars[op2 - 1]) {
				pcount = op1;
				continue;
			}
			break;
		case OP_GOTOIFPOS:
			if (op1 < 0 || op1 > maxpcount || op2 < 1 || op2 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			if (vars[op2 - 1] > 0) {
				pcount = op1;
				continue;
			}
			break;
		case OP_GOTOIFNOTPOS:
			if (op1 < 0 || op1 > maxpcount || op2 < 1 || op2 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			if (vars[op2 - 1] <= 0) {
				pcount = op1;
				continue;
			}
			break;
		case OP_GOTOIFNEG:
			if (op1 < 0 || op1 > maxpcount || op2 < 1 || op2 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			if (vars[op2 - 1] < 0) {
				pcount = op1;
				continue;
			}
			break;
		case OP_GOTOIFNOTNEG:
			if (op1 < 0 || op1 > maxpcount || op2 < 1 || op2 > maxvar) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			if (vars[op2 - 1] >= 0) {
				pcount = op1;
				continue;
			}
			break;

		case OP_CALL:
			if (op1 < 0 || op1 > maxpcount || retstackindex > RETSTACKSIZE - 2) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			retstack[retstackindex++] = pcount + 1;
			pcount = op1;
			continue;

		case OP_RETURN:
			if (retstackindex < 1) {
				execerror();
				return sqlerrnum(SQLERR_EXEC_BADPGM);
			}
			pcount = retstack[--retstackindex];
			continue;

		case OP_FINISH:
			if (plan->numvars) {
				if (plan->savevars == NULL) {
					hmem1 = memalloc(plan->numvars * sizeof(long), 0);
					if (hmem1 == NULL) {
						execerror();
						return sqlerrnum(SQLERR_NOMEM);
					}
					plan = *hplan;
					pcode = (*plan->pgm);
					plan->savevars = (long **) hmem1;
				}
				for (i1 = 0; i1 < plan->numvars; i1++) (*plan->savevars)[i1] = vars[i1];
			}
			if (arg1 != NULL) *arg1 = vars[0];
			if (arg2 != NULL) *arg2 = vars[1];
			if (arg3 != NULL) *arg3 = vars[2];
			return 0;
		case OP_TERMINATE:
#if 0
/*** CODE: MAYBE ADD THIS, BUT DON'T DO IT FOR NOW (LEAVE ALLOCATED) ***/
			if (updaterecbuf != NULL) {
				memfree(updaterecbuf);
				updaterecbuf = NULL;
			}
#endif
			for (i1 = 0; i1 < plan->numtables; i1++) {  /* flag open files as not in use */
			 	opf1 = openfileptr(filerefmap[i1]);
				opf1->inuseflag = FALSE;
				if (forupdateflag) riounlock(opf1->textfilenum, -1);
			}
			memfree((UCHAR **) plan->savevars);
			plan->savevars = NULL;
			memfree((UCHAR **) plan->filerefmap);
			plan->filerefmap = NULL;
			if (arg1 != NULL) *arg1 = vars[0];
			if (arg2 != NULL) *arg2 = vars[1];
			if (arg3 != NULL) *arg3 = vars[2];
			return 0;
		default:
			return sqlerrnum(SQLERR_EXEC_BADPGM);
		}
		pcount++;
	}
}

static void execerror()
{
	INT i1;
	PLAN *plan;
	TABLE *tab1;
	OPENFILE *opf1;

	plan = *thishplan;
	plan->flags |= PLAN_FLAG_ERROR;
	if ((plan->flags & (PLAN_FLAG_FORUPDATE | PLAN_FLAG_VALIDREC)) == (PLAN_FLAG_FORUPDATE | PLAN_FLAG_VALIDREC)) {
		for (i1 = 0; i1 < plan->numtables; i1++) {  /* flag open files as not in use */
			if ((*plan->filerefmap)[i1] < 1 || (*plan->filerefmap)[i1] > connection.numopenfiles) continue;
			opf1 = openfileptr((*plan->filerefmap)[i1]);
			riounlock(opf1->textfilenum, -1);
		}
		plan->flags &= ~PLAN_FLAG_VALIDREC;
	}
	if (plan->tablelockcnt > 0) {
/*** NOTE: THIS MAKES ASSUMPTION THAT TABLE LOCK CAN ONLY HAPPEN ON THE FIRST FILE ***/
		opf1 = *connection.hopenfilearray + filerefmap[0] - 1;
		tab1 = tableptr(opf1->tablenum);
		if (plan->tablelockcnt >= tab1->lockcount) {
			unlocktextfile(tab1->lockhandle);
			tab1->lockcount = 0;
		}
		else tab1->lockcount -= plan->tablelockcnt;
		plan->tablelockcnt = 0;
	}
}

INT readwks(INT worksetnum, INT count)
{
	int i1, i2, i3;
	WORKSET *wks1;

	wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset + count - 1;
	if (wks1->rowid > wks1->rowcount) return -1;
	if (!wks1->rowcount || !wks1->rowid || wks1->rowcount <= wks1->memrowalloc) return 0;
	i1 = wks1->rowid - 1;
	i2 = wks1->memrowoffset;
	if (i1 < i2) {  /* seem to be moving backwards */
		i3 = wks1->rowid - wks1->memrowalloc;
		if (i3 < 0) i3 = 0;
	}
	else if (i1 >= i2 + wks1->memrowalloc) i3 = i1;  /* seem to be moving forwards */
	else return 0;
	/* read the rows */
	if (i3 + wks1->memrowalloc > wks1->rowcount) i3 = wks1->rowcount - wks1->memrowalloc;
	wks1->memrowoffset = i3;
	i1 = wks1->rowbytes;
	i2 = wks1->memrowalloc * i1;
	i3 = fioread(wks1->fiofilenum, (OFFSET) i3 * i1, *wks1->rows, i2);
	if (i2 != i3) return sqlerrnummsg(SQLERR_EXEC_BADWORKSET, "error reading workset", NULL);
	return 0;
}

static INT writedirtywks(INT worksetnum, INT count)
{
	int i1, i2, i3;
	WORKSET *wks1;

	wks1 = (*(*(*connection.hworksetarray + worksetnum - 1)))->workset + count - 1;
	if (!wks1->fiofilenum) return 0;  /* work file not created */
	i1 = wks1->rowbytes;
	if (wks1->bufdirtyflag) {
		i2 = (wks1->rowcount - wks1->memrowoffset) * i1;
		i3 = fiowrite(wks1->fiofilenum, (OFFSET) wks1->memrowoffset * i1, *wks1->rows, i2);
		if (i3) return sqlerrnummsg(i3, "working set error during execution", NULL);
	}
	else if (wks1->rowdirtyflag) {
		i2 = wks1->rowid - wks1->memrowoffset - 1;
		if (i2 < 0 || i2 >= wks1->memrowalloc) return sqlerrnum(SQLERR_EXEC_BADWORKSET);
		i3 = fiowrite(wks1->fiofilenum, (OFFSET)(wks1->rowid - 1) * i1, *wks1->rows + i2, i1);
		if (i3) return sqlerrnummsg(i3, "working set error during execution", NULL);
	}
	wks1->bufdirtyflag = wks1->rowdirtyflag = FALSE;
	return 0;
}

static int sqlaimrec(int filenum, int lockflag, int nowaitflag, int indexnum, int aionextflag, int *errhandle) // @suppress("No return")
{
#define AIMREC_FLAGS_FORWARD	0x01
#define AIMREC_FLAGS_FIXED		0x02
#define AIMREC_FLAGS_CHECKPOS	0x04

	int aimhandle, flags, retcode, txthandle;
	OFFSET eofpos, offset;
	OPENFILE *opf1;
	TABLE *tab1;

	opf1 = *connection.hopenfilearray + filenum - 1;
	tab1 = tableptr(opf1->tablenum);
	*errhandle = txthandle = opf1->textfilenum;
	aimhandle = (*opf1->hopenindexarray)[indexnum - 1];

	flags = 0;
	if (aionextflag == AIONEXT_NEXT || aionextflag == AIONEXT_FIRST) flags |= AIMREC_FLAGS_FORWARD;
	if ((*tab1->hindexarray + indexnum - 1)->flags & INDEX_FIXED) flags |= AIMREC_FLAGS_FIXED;
	if (opf1->aimrec >= 0) {
		if (aionextflag == AIONEXT_FIRST || aionextflag == AIONEXT_LAST) opf1->aimrec = -1;
		else {
			if (aionextflag == AIONEXT_NEXT) riosetpos(txthandle, opf1->aimnextpos);
			else riosetpos(txthandle, opf1->aimlastpos);
			flags |= AIMREC_FLAGS_CHECKPOS;
		}
	}
	for ( ; ; ) {
		if (opf1->aimrec == -2) {
			if (aionextflag == AIONEXT_FIRST) {
				riosetpos(txthandle, 0);
				aionextflag = AIONEXT_NEXT;
			}
			else if (aionextflag == AIONEXT_LAST) {
				retcode = fioflck(txthandle);
				if (retcode < 0) return retcode;
				rioeofpos(txthandle, &offset);
				fiofulk(txthandle);
				riosetpos(txthandle, offset);
				aionextflag = AIONEXT_PREV;
			}
			if (aionextflag == AIONEXT_NEXT) {
				for ( ; ; ) {
					if (lockflag) {
						retcode = riolock(txthandle, nowaitflag);
						if (retcode != 0) break;
					}
					retcode = rioget(txthandle, *opf1->hrecbuf, tab1->reclength);
					if (lockflag && retcode < 0) {
						riolastpos(txthandle, &offset);
						riounlock(txthandle, offset);
					}
					if (retcode != -2) break;
				}
			}
			else {
				for ( ; ; ) {
					retcode = rioprev(txthandle, *opf1->hrecbuf, tab1->reclength);
					if (retcode < 0) {
						if (retcode != -2) break;
						continue;
					}
					if (!lockflag) break;
					riolastpos(txthandle, &offset);
					riosetpos(txthandle, offset);
					retcode = riolock(txthandle, nowaitflag);
					if (retcode != 0) break;
					retcode = rioget(txthandle, *opf1->hrecbuf, tab1->reclength);
					if (retcode < 0) {
						riolastpos(txthandle, &offset);
						riounlock(txthandle, offset);
					}
					if (retcode != -2) break;
				}
			}
			if (retcode >= 0 && retcode < tab1->reclength)
				memset(*opf1->hrecbuf + retcode, ' ', tab1->reclength - retcode);
			return retcode;
		}
		/* read an aim record and see if it matches the criteria */
		if (opf1->aimrec == -1) {
			retcode = aionext(aimhandle, aionextflag);
			opf1 = *connection.hopenfilearray + filenum - 1;
			tab1 = tableptr(opf1->tablenum);
			if (retcode != 1) {
				if (retcode == 3) {  /* not enough key information */
					opf1->aimrec = -2;
					continue;
				}
				if (retcode == 2) retcode = -1;
				*errhandle = aimhandle; 
				return retcode;
			}
			if (aionextflag == AIONEXT_FIRST) aionextflag = AIONEXT_NEXT;
			else if (aionextflag == AIONEXT_LAST) aionextflag = AIONEXT_PREV;
	
			/* a possible record exists */
			if (flags & AIMREC_FLAGS_FIXED) {  /* fixed length records */
				offset = filepos * (tab1->reclength + tab1->eorsize);
				riosetpos(txthandle, offset);
			}
			else {  /* variable length records */
				offset = filepos << 8;
				if (aionextflag == AIONEXT_PREV) {
					offset += 256;
					rioeofpos(txthandle, &eofpos);
					if (offset > eofpos) offset = eofpos;
				}
				riosetpos(txthandle, offset);
				if (offset) {
					rioget(txthandle, *opf1->hrecbuf, tab1->reclength);
					if (aionextflag == AIONEXT_PREV) {
						rionextpos(txthandle, &offset);
						riosetpos(txthandle, offset);
					}
				}
				opf1->aimrec = filepos;
			}
			flags &= ~AIMREC_FLAGS_CHECKPOS;
		}
		do {
			if (flags & (AIMREC_FLAGS_FORWARD | AIMREC_FLAGS_FIXED)) {
				if ((flags & AIMREC_FLAGS_CHECKPOS) && (opf1->aimnextpos - 1) >> 8 != opf1->aimrec) {
					opf1->aimrec = -1;
					break;
				}
				retcode = rioget(txthandle, *opf1->hrecbuf, tab1->reclength);
			}
			else retcode = rioprev(txthandle, *opf1->hrecbuf, tab1->reclength);
			if (retcode < 0 && retcode != -2) {
				opf1->aimrec = -1;
				return retcode;
			}
			if (!(flags & AIMREC_FLAGS_FIXED)) {
				riolastpos(txthandle, &offset);
				opf1->aimlastpos = offset;
				if (offset) offset = (offset - 1) >> 8;
				if (offset != opf1->aimrec) {
					opf1->aimrec = -1;
					break;
				}
				offset = opf1->aimlastpos;
				rionextpos(txthandle, &opf1->aimnextpos);
			}
			if (retcode == -2 || aiochkrec(aimhandle, *opf1->hrecbuf, retcode)) continue;
			if (!lockflag) return retcode;

			/* lock the reocrd */
			riosetpos(txthandle, offset);
			retcode = riolock(txthandle, nowaitflag);
			if (retcode != 0) {
				if (retcode > 0) retcode = -2;
				return retcode;
			}
			retcode = rioget(txthandle, *opf1->hrecbuf, tab1->reclength);
			if (retcode >= 0 && !aiochkrec(aimhandle, *opf1->hrecbuf, retcode)) return retcode;
			riounlock(txthandle, offset);
			if (retcode < 0 && retcode != -2) return retcode;
		} while (!((flags |= AIMREC_FLAGS_CHECKPOS) & AIMREC_FLAGS_FIXED));
	}
#undef AIMREC_FLAGS_FORWARD
#undef AIMREC_FLAGS_FIXED
#undef AIMREC_FLAGS_CHECKPOS
}

static void buildkey(INT tablenum, INT indexnum, UCHAR **buffer, CHAR *key)
{
	int i1, i2;
	TABLE *tab1;
	INDEX *idx1;

	tab1 = tableptr(tablenum);
	idx1 = *tab1->hindexarray + indexnum - 1;
	for (i1 = i2 = 0; i1 < idx1->numkeys; i1++) {
		memcpy(key + i2, *buffer + (*idx1->hkeys)[i1].pos, (*idx1->hkeys)[i1].len);
		i2 += (*idx1->hkeys)[i1].len;
	}
	key[i2] = '\0';
}

/*** NOTE: MEMORY CAN NOT MOVE WHILE CALLER IS USING POINTER ***/
static int colreftostr(UINT colrefnum, INT flags, UCHAR **ptr, INT *len, INT *scale)
{
	static int bufcnt = 0;
	static unsigned char colrefbuf[4][32];
/*** CODE: USE THIS UNTIL Y2KBREAK=YY IS PART OF CONFIG FILE OR DBD FILE ***/
	static int y2kbreak = 20;
	int i1, i2, i3, i4, i5, offset, length, type;
	UCHAR *dest, *format, *src;
	PLAN *plan;
	COLREF *crf1;
	OPENFILE *opf1;
	WORKSET *wks1;
	TABLE *tab1;
	COLUMN *col1;

	plan = *thishplan;
	i1 = gettabref(colrefnum);
	i2 = getcolnum(colrefnum);
	*scale = 0;
	
	if (i1 == TABREF_LITBUF) {  /* its a literal */
		src = *plan->literals + i2;
		if (src[0] == TYPE_LITERAL && (flags & COLREFTOSTR_DIRECT))
			return sqlerrnummsg(SQLERR_EXEC_BADCOL, "source column reference invalid", NULL);
		*len = ((INT) src[1] << 8) + src[2];
		*ptr = src + 4;
		return 0;
	}
	if (i1 == TABREF_WORKVAR) {  /* its a temporary numeric column */
		*ptr = (UCHAR *) tempnumcolumns[i2 - 1];
		*len = (INT)strlen(tempnumcolumns[i2 - 1]);
		for (i1 = 0; i1 < *len; i1++) {
			if (tempnumcolumns[i2 - 1][i1] == '.') {
				*scale = (*len - i1) - 1;
				break;
			}
		}
		return 0;
	}
	if (i1 <= plan->numtables) {  /* its an open file reference */
		opf1 = *connection.hopenfilearray + filerefmap[i1 - 1] - 1;
		src = *opf1->hrecbuf;
		tab1 = tableptr(opf1->tablenum);
		if (i2 < 1 || i2 > tab1->numcolumns) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "source column reference invalid", NULL);
		col1 = columnptr(tab1, i2);
		type = col1->type;
		offset = col1->offset;
		length = col1->fieldlen;
		*scale = col1->scale;
		format = (UCHAR *) nameptr(col1->format);
	}
	else {
		i1 -= plan->numtables;
		if (i1 > plan->numworksets) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "source column reference invalid", NULL);
		wks1 = (*(*(*connection.hworksetarray + plan->worksetnum - 1)))->workset + i1 - 1;
		if (wks1->rowid <= 0 || wks1->rowid > wks1->rowcount) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "row id of source column reference invalid", NULL);
/*** CODE: VERIFY IF POSSIBLE FOR BLOCK TO BE IN FILE ***/
		if (wks1->rowid <= wks1->memrowoffset || wks1->rowid > wks1->memrowoffset + wks1->memrowalloc) {
			if (readwks(plan->worksetnum, i1) < 0) return -1;
		}
		src = *wks1->rows + (wks1->rowid - wks1->memrowoffset - 1) * wks1->rowbytes;
		if (i2 < 1 || i2 > wks1->numcolrefs) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "workset column reference invalid", NULL);
		crf1 = *wks1->colrefarray + i2 - 1;
		type = crf1->type;
		offset = crf1->offset;
		length = crf1->length;
		*scale = crf1->scale;
		format = (UCHAR *) "";
	}
	src += offset;
	*ptr = src;
	i2 = length;
	if (flags & COLREFTOSTR_DIRECT) {
		*len = i2;
		return 0;
	}
	if (type != TYPE_CHAR) {  /* all except CHAR can be NULL */
		for (i1 = 0; i1 < i2 && src[i1] == ' '; i1++);
		if (i1 == i2) i2 = 0;
		else if ((type == TYPE_DATE || type == TYPE_TIME || type == TYPE_TIMESTAMP) && *format) {
			dest = colrefbuf[bufcnt];
			if (++bufcnt == sizeof(colrefbuf) / sizeof(*colrefbuf)) bufcnt = 0;
			memcpy(dest, "0000-00-00 00:00:00.000", 23);
/*** CODE: MAYBE CHECK FORMAT OF FIELD IN RECBUF TO SEE IF VALID ***/
			for (i3 = i4 = 0; format[i3]; i3++) {
				switch (format[i3]) {
				case FORMAT_DATE_YEAR:
					memcpy(dest, src + i4, 4);
					i4 += 4;
					break;
				case FORMAT_DATE_YEAR2:
					i5 = 0;
					if (isdigit(src[i4])) i5 = (src[i4] - '0') * 10;
					if (isdigit(src[i4 + 1])) i5 += src[i4 + 1] - '0';
					if (i5 > y2kbreak) memcpy(dest, "19", 2);
					else memcpy(dest, "20", 2);
					memcpy(dest + 2, src + i4, 2);
					i4 += 2;
					break;
				case FORMAT_DATE_MONTH:
					memcpy(dest + 5, src + i4, 2);
					i4 += 2;
					break;
				case FORMAT_DATE_DAY:
					memcpy(dest + 8, src + i4, 2);
					i4 += 2;
					break;
				case FORMAT_TIME_HOUR:
					memcpy(dest + 11, src + i4, 2);
					i4 += 2;
					break;
				case FORMAT_TIME_MINUTES:
					memcpy(dest + 14, src + i4, 2);
					i4 += 2;
					break;
				case FORMAT_TIME_SECONDS:
					memcpy(dest + 17, src + i4, 2);
					i4 += 2;
					break;
				case FORMAT_TIME_FRACTION1:
				case FORMAT_TIME_FRACTION2:
				case FORMAT_TIME_FRACTION3:
				case FORMAT_TIME_FRACTION4:
					i5 = format[i3] - FORMAT_TIME_FRACTION1 + 1;
					if (i5 > 3) i5 = 3;
					memcpy(dest + 20, src + i4, i5);
					i4 += format[i3] - FORMAT_TIME_FRACTION1 + 1;
					break;
				default:
					i4++;
					break;
				}
			}
			if (type == TYPE_DATE) i2 = 10;
			else if (type == TYPE_TIME) {
				dest += 11;
				i2 = 12;
			}
			else i2 = 23;
			for (i3 = 0; i3 < i2; i3++) if (dest[i3] == ' ' && (type != TYPE_TIMESTAMP || i3 != 10)) dest[i3] = '0';
			*ptr = dest;
		}
	}
	*len = i2;
	return 0;
}

static INT strtocolref(UCHAR *src, INT len, INT srctype, INT colrefnum)
{
	INT i1, i2, i3, i4, offset, length, type, scale;
	UCHAR work[32], *dest, *format;
	PLAN *plan;
	COLREF *crf1;
	OPENFILE *opf1;
	WORKSET *wks1;
	TABLE *tab1;
	COLUMN *col1;

	plan = *thishplan;
	i1 = gettabref(colrefnum);
	i2 = getcolnum(colrefnum);
	if (i1 == TABREF_LITBUF) {
		dest = *plan->literals + i2;
		type = dest[0];
		if (type == TYPE_LITERAL)
			return sqlerrnummsg(SQLERR_EXEC_BADCOL, "destination column reference number invalid", NULL);  /* its a literal */
		length = ((INT) dest[1] << 8) + dest[2];
		scale = dest[3];
		offset = 0;
		format = (UCHAR *) "";
		dest += 4;
	}
	else if (i1 == TABREF_WORKVAR) {  /* its a temporary numeric column */
		memcpy(tempnumcolumns[i2 - 1], src, len);
		tempnumcolumns[i2 - 1][len] = 0;
		return 0;
	}
	else if (i1 <= plan->numtables) {  /* its an open file reference */
		opf1 = *connection.hopenfilearray + filerefmap[i1 - 1] - 1;
		dest = *opf1->hrecbuf;
		tab1 = tableptr(opf1->tablenum);
		if (i2 < 1 || i2 > tab1->numcolumns) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "destination column reference number invalid", NULL);
		col1 = columnptr(tab1, i2);
		type = col1->type;
		offset = col1->offset;
		length = col1->fieldlen;
		scale = col1->scale;
		format = (UCHAR *) nameptr(col1->format);
	}
	else {
		i1 -= plan->numtables;
		if (i1 > plan->numworksets) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "destination column reference number invalid", NULL);
		wks1 = (*(*(*connection.hworksetarray + plan->worksetnum - 1)))->workset + i1 - 1;
		if (wks1->rowdirtyflag) wks1->bufdirtyflag = TRUE;
		else wks1->rowdirtyflag = TRUE;
		if (wks1->rowid <= 0 || wks1->rowid > wks1->rowcount) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "row id of destination column reference invalid", NULL);
		dest = *wks1->rows + (wks1->rowid - wks1->memrowoffset - 1) * wks1->rowbytes;
		if (i2 < 1 || i2 > wks1->numcolrefs) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "workset column reference invalid", NULL);
		crf1 = *wks1->colrefarray + i2 - 1;
		type = crf1->type;
		offset = crf1->offset;
		length = crf1->length;
		scale = crf1->scale;
		format = (UCHAR *) "";
	}
	dest += offset;
	i2 = length;
	if (type == TYPE_CHAR) {
/*** CODE: POSSIBLE TO TRUNCATE STRING HERE, USE SQLSWI()? ***/
		if (len < i2) {
			memcpy(dest, src, len);
			memset(&dest[len], ' ', i2 - len);
		}
		else memcpy(dest, src, i2);
		return 0;
	}
	for (i3 = 0; i3 < len && src[i3] == ' '; i3++);
	if (i3 == len) {  /* NULL source */
		memset(dest, ' ', i2);
		return 0;
	}
	i4 = scale;
	if (type == TYPE_NUM || type == TYPE_POSNUM) {
		for (i3 = 0; i3 < len && src[i3] != '.'; i3++);
		if (i3 == len) i3 = 0;
		else i3 = len - i3 - 1;
		if (len == i2 && i3 == i4) memcpy(dest, src, len);
		else {
			if (i4)	{
				while (i4 > i3) {
					dest[--i2] = '0';
					i4--;
				}
				for ( ; i3 > i4 && src[len - 1] == '0'; len--, i3--);
				if (i3 > i4) {  /* fractional digits truncated */
					len -= i3 - i4;
					i3 = i4;
#if 0
/*** ANY DIVIDE OR MULTIPLY WILL PROBABLY SET THIS ***/
					sqlswi();
#endif
				}
				while (i3 > 0) {
					dest[--i2] = src[--len];
					i3--;
				}
				dest[--i2] = '.';
				if (src[len - 1] == '.') len--;
			}
			else if (i3) {  /* truncate fractional digits */
				len -= i3 + 1;
				if (src[len - 1] == ' ') {
					len--;
					dest[--i2] = '0';
				}
#if 0
/*** ANY DIVIDE OR MULTIPLY WILL PROBABLY SET THIS ***/
				sqlswi();
#endif
			}
			while (len > 0 && i2 > 0) dest[--i2] = src[--len];
			if (len && src[len - 1] != ' ') sqlswi(); /* decimal digits truncated */
			while (i2 > 0) dest[--i2] = ' ';
		}
		if (type == TYPE_POSNUM) {
			for (i2 = 0; i2 < length && dest[i2] == ' '; i2++);
			if (i2 < length && dest[i2] == '-') {
				dest[i2] = ' ';
/*** CODE: VERIFY THIS CALL ***/
				sqlswi(); /* negative sign truncated */
			}
		}
	}
	else {
		if ((type == TYPE_DATE && srctype == TYPE_TIME) || (type == TYPE_TIME && srctype == TYPE_DATE)) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "unable convert between date and time types", NULL);
		memcpy(work, "0000-00-00 00:00:00.000000", 26);
		if (srctype == TYPE_DATE) memcpy(work, src, 10);
		else if (srctype == TYPE_TIME) memcpy(work + 11, src, 12);
		else if (srctype == TYPE_TIMESTAMP) memcpy(work, src, 23);
		else if (srctype == TYPE_CHAR || srctype == TYPE_LITERAL) {
			i3 = cvttodatetime(srctype, src, len, TYPE_TIMESTAMP, work);
			if (i3 < 0) return -1;
		}
		else return sqlerrnummsg(SQLERR_EXEC_BADCOL, "unable convert between numeric and date or time types", NULL);
		if (!*format) {
			if (type == TYPE_DATE) memcpy(dest, work, 10);
			else if (type == TYPE_TIME) memcpy(dest, work + 11, 12);
			else memcpy(dest, work, 23);
		}
		else {
			for (i3 = i4 = 0; format[i3]; i3++) {
				switch (format[i3]) {
				case FORMAT_DATE_YEAR:
					memcpy(dest + i4, work, 4);
					i4 += 4;
					break;
				case FORMAT_DATE_YEAR2:
					memcpy(dest + i4, work + 2, 2);
					i4 += 2;
					break;
				case FORMAT_DATE_MONTH:
					memcpy(dest + i4, work + 5, 2);
					i4 += 2;
					break;
				case FORMAT_DATE_DAY:
					memcpy(dest + i4, work + 8, 2);
					i4 += 2;
					break;
				case FORMAT_TIME_HOUR:
					memcpy(dest + i4, work + 11, 2);
					i4 += 2;
					break;
				case FORMAT_TIME_MINUTES:
					memcpy(dest + i4, work + 14, 2);
					i4 += 2;
					break;
				case FORMAT_TIME_SECONDS:
					memcpy(dest + i4, work + 17, 2);
					i4 += 2;
					break;
				case FORMAT_TIME_FRACTION1:
				case FORMAT_TIME_FRACTION2:
				case FORMAT_TIME_FRACTION3:
				case FORMAT_TIME_FRACTION4:
					memcpy(dest + i4, work + 20, format[i3] - FORMAT_TIME_FRACTION1 + 1);
					i4 += format[i3] - FORMAT_TIME_FRACTION1 + 1;
					break;
				default:
					dest[i4++] = format[i3];
					break;
				}
			}
		}
	}
	return 0;
}

static COLREF *getcolref(INT colrefnum)
{
	int i1, i2;
	PLAN *plan;
	WORKSET *wks1;

	plan = *thishplan;
	i1 = gettabref(colrefnum);
	i2 = getcolnum(colrefnum);
	if (i1 == TABREF_LITBUF || i1 <= plan->numtables) return NULL;  /* its a literal or an open file reference */
	i1 -= plan->numtables;
	if (i1 > plan->numworksets) return NULL;
	wks1 = (*(*(*connection.hworksetarray + plan->worksetnum - 1)))->workset + i1 - 1;
	if (i2 < 1 || i2 > wks1->numcolrefs) return NULL;
	return (*wks1->colrefarray) + i2 - 1;
}

static INT getcoltype(INT colrefnum)
{
	INT i1, i2;
	PLAN *plan;
	COLREF *crf1;
	OPENFILE *opf1;
	WORKSET *wks1;
	TABLE *tab1;
	COLUMN *col1;

	plan = *thishplan;
	i1 = gettabref(colrefnum);
	i2 = getcolnum(colrefnum);
	if (i1 == TABREF_LITBUF) return (*plan->literals)[i2];  /* literal type */
	if (i1 == TABREF_WORKVAR) return TYPE_NUM;
	if (i1 <= plan->numtables) {  /* its an open file reference */
		opf1 = *connection.hopenfilearray + filerefmap[i1 - 1] - 1;
		tab1 = tableptr(opf1->tablenum);
		if (i2 < 1 || i2 > tab1->numcolumns) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "invalid column reference number", NULL);
		col1 = columnptr(tab1, i2);
		return col1->type;
	}
	i1 -= plan->numtables;
	if (i1 > plan->numworksets) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "invalid column reference number", NULL);
	wks1 = (*(*(*connection.hworksetarray + plan->worksetnum - 1)))->workset + i1 - 1;
	if (i2 < 1 || i2 > wks1->numcolrefs) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "workset column reference invalid", NULL);
	crf1 = *wks1->colrefarray + i2 - 1;
	return crf1->type;
}

/**
 * changecolsize() is used by SUBSTRING, TRIM, and CONCAT because result column size
 * is not known when selection plan is built.  Column size must therefore be corrected
 * after operation is complete.
 *
 * colrefnum low-order 20 bits is the column number, high-order 16 bits are the table number.
 * newlen is the new column size.
 * wherefrom is used for error reporting only
 */
static INT changecolsize(INT colrefnum, INT newlen, CHAR* wherefrom)
{
	INT i1, i2, difference;
	UCHAR *ptr;
	PLAN *plan;
	COLREF *crf1;
	WORKSET *wks1;
	CHAR errwrk1[256], errwrk2[128];
	if (newlen == 0) return 0;
	plan = *thishplan;
	i1 = gettabref(colrefnum);
	i2 = getcolnum(colrefnum);
	if (i1 == TABREF_LITBUF) {
		ptr = *(plan->literals) + i2;
		i2 = ((INT) ptr[1] << 8) + ptr[2];
		if (i2 != newlen) {
			if (newlen > i2) {
				sprintf(errwrk1, "internal error, can not increase literal size, source=%s", wherefrom);
				sprintf(errwrk2, "i2=%d, newlen=%d, lit=%.*s", i2, newlen, i2, ptr+4);
				return sqlerrnummsg(SQLERR_EXEC_BADCOL, errwrk1, errwrk2);
			}
			/* resize literal (smaller) */
			ptr[1] = (UCHAR)(newlen >> 8);
			ptr[2] = (UCHAR) newlen;
		}
		return 0;
	}
	if (i1 == TABREF_WORKVAR || i1 <= plan->numtables) {
/*** NOTE: this should never happen ***/
		return 0;
	};
	i1 -= plan->numtables;
	if (i1 > plan->numworksets) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "invalid column reference number", NULL);
	wks1 = (*(*(*connection.hworksetarray + plan->worksetnum - 1)))->workset + i1 - 1;
	if (i2 < 1 || i2 > wks1->numcolrefs) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "workset column reference invalid", NULL);
	crf1 = *wks1->colrefarray + i2 - 1;
	difference = crf1->length - newlen;
	if (difference == 0) return 0; /* nothing to do */
	wks1->rsrowbytes -= difference;
	wks1->rowbytes -= difference;	
	crf1->length = newlen;
	for (i1 = i2 + 1; i1 <= wks1->numcolrefs; i1++) {
		/* adjust (shift) offsets of remaining columns that were selected */ 
		crf1 = *wks1->colrefarray + i1 - 1;
		crf1->offset -= difference;
	}
	return 0;
}

static INT cvttodatetime(INT srctype, UCHAR *src, INT srclen, INT desttype, UCHAR *dest)
{
	INT i1, i2, i3, len;
	CHAR work[32];

	memcpy(work, "0000-00-00 00:00:00.000000", 26);
	if (srctype == TYPE_DATE) memcpy(work, src, 10);
	else if (srctype == TYPE_TIME) memcpy(work + 11, src, 12);
	else if (srctype == TYPE_TIMESTAMP) memcpy(work, src, 23);
	else if (srctype == TYPE_CHAR || srctype == TYPE_LITERAL) {
		while (srclen && src[srclen - 1] == ' ') srclen--;
		if (!srclen) {
			if (desttype == TYPE_DATE) len = 10;
			else if (desttype == TYPE_TIME) len = 12;
			else len = 23;
			memset(dest, ' ', len);
			return len;
		}
		for (i1 = i2 = 0; i1 < srclen && isdigit(src[i1]); i1++);
		/* get date information */
		if (i1 == 4) {
			if (i1 < srclen && src[i1] != '-') return sqlerrnummsg(SQLERR_EXEC_BADCOL, "character or literal not valid date/time/timestamp", NULL);
			memcpy(work, src, 4);
			for (i2 = ++i1; i1 < srclen && isdigit(src[i1]); i1++);
			if (i1 != i2) {
				if (i1 - i2 != 2 || (i1 < srclen && src[i1] != '-')) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "character or literal not valid date/time/timestamp", NULL);
				memcpy(work + 5, src + i2, 2);
				for (i2 = ++i1; i1 < srclen && isdigit(src[i1]); i1++);
				if (i1 != i2) {
					if (i1 - i2 != 2 || (i1 < srclen && src[i1] != ' ')) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "character or literal not valid date/time/timestamp", NULL);
					memcpy(work + 8, src + i2, 2);
					for (i2 = ++i1; i1 < srclen && isdigit(src[i1]); i1++);
				}
			}
		}
		/* get time information */
		if (i1 != i2) {
			if (i1 - i2 != 2 || (i1 < srclen && src[i1] != ':')) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "character or literal not valid date/time/timestamp", NULL);
			memcpy(work + 11, src + i2, 2);
			for (i2 = ++i1; i1 < srclen && isdigit(src[i1]); i1++);
			if (i1 != i2) {
				if (i1 - i2 != 2 || (i1 < srclen && src[i1] != ':')) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "character or literal not valid date/time/timestamp", NULL);
				memcpy(work + 14, src + i2, 2);
				for (i2 = ++i1; i1 < srclen && isdigit(src[i1]); i1++);
				if (i1 != i2) {
					if (i1 - i2 != 2 || (i1 < srclen && src[i1] != '.')) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "character or literal not valid date/time/timestamp", NULL);
					memcpy(work + 17, src + i2, 2);
					for (i2 = ++i1; i1 < srclen && isdigit(src[i1]); i1++);
					if (i1 != i2) {
						i3 = i1 - i2;
						if (i3 > 3) i3 = 3;
						memcpy(work + 20, src + i2, i3);
					}
				}
			}
		}
		if (i1 < srclen) return sqlerrnummsg(SQLERR_EXEC_BADCOL, "character or literal not valid date", NULL);
	}
	else {  /* numeric */
/*** CODE: SUPPORT NUMERIC TO DATE/TIME TYPE, WHICH CAN BE USED FOR ADD/SUB. ***/
/***       USE MATH.C BUT NOT SURE ABOUT WHAT TO DO WITH NEGATIVE NUMBERS ***/
		return sqlerrnummsg(SQLERR_EXEC_BADCOL, "numeric conversion to date/time/timestamp not supported", NULL);
	}
	if (desttype == TYPE_DATE) {
		memcpy(dest, work, 10);
		len = 10;
	}
	else if (desttype == TYPE_TIME) {
		memcpy(dest, work + 11, 10);
		len = 12;
	}
	else {
		memcpy(dest, work, 23);
		len = 23;
	}
	return len;
}

static INT matchstring(UCHAR *string, INT stringlen, UCHAR *pattern, INT patternlen)
{
	INT i1, i2;

	for (i1 = i2 = 0; i1 < patternlen; i2++, i1++) {
		if (pattern[i1] == '%') {
			do if (++i1 == patternlen) return TRUE;
			while (pattern[i1] == '%');
			while (i2 < stringlen) {
				if ((pattern[i1] == '_' || pattern[i1] == string[i2]) && matchstring(string + i2, stringlen - i2, pattern + i1, patternlen - i1)) return TRUE;
				i2++;
			}
			return FALSE;
		}
		if (pattern[i1] == '_') {
			if (i2 == stringlen) return FALSE;
		}
		else {
			if (pattern[i1] == '\\' && i1 + 1 < patternlen && (pattern[i1 + 1] == '\\' || pattern[i1 + 1] == '%' || pattern[i1 + 1] == '_')) i1++;
			if (pattern[i1] != string[i2]) return FALSE;
		}
	}
	while (i2 < stringlen && string[i2] == ' ') i2++;
	return (i2 == stringlen);
}
