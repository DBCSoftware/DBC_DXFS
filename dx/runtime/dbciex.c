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
#define INC_STDLIB
#define INC_CTYPE
#define INC_MATH
#define INC_SETJMP
#define INC_ERRNO
#include "includes.h"
#include "release.h"
#include "base.h"
#include "dbc.h"
#include "dbcclnt.h"
#include "evt.h"
#include "fio.h"
#include "fsfileio.h"
#include "gui.h"
#include "prt.h"
#include "que.h"
#include "util.h"
#include "vid.h"
#include <assert.h>

typedef struct {
	int retpgm;	/* return module */
	int retpcnt;	/* return pcount */
	int parmdata;	/* call with parms data area offset */
	int parmnext;	/* call with parms working parm addresses */
	int parmfirst;	/* call with parms fixed parm addresses */
	int makepgm;	/* make's call program module */
	int makedata;	/* make's call data module */
	int makepcnt;	/* make's call pcount */
	int objdata;	/* make's object module */
	int objoff;	/* make's object offset */
} RETURNSTACK;

static jmp_buf errjmp;		/* stack address to return to if untrapable error */
static INT action;				/* xprefix, filepi count, error or debug action required */

#define ACTION_DEBUG	0x01
#define ACTION_ERROR	0x02
#define ACTION_FILEPI	0x04
#define ACTION_XPREFIX	0x08
#define ACTION_EVENT	0x10
#define ACTION_POLL		0x20
#define ACTION_SHUTDOWN	0x40

extern INT datachn;
extern INT pgmchn;

CHAR utilerror[256];

#define CALLMAX 200

static INT retptr;
static INT callmax;
static INT fpiretptr;
static INT makeretptr;
static RETURNSTACK *rstack;
static UCHAR work45[45], work6a[6], work6b[6];
static UCHAR *adr1, *adr2, *adr3;
static INT fp1, lp1, hl1, fp2, lp2, hl2;
static UCHAR c1, c2;
static INT calldmod, callpmod, callpcnt;
static INT makemod;
static UCHAR bitopflg;		/* use 7 bit operations */
static UCHAR olddisableflg;	/* do not set over */
static UCHAR oldcxcompatflg;	/* do not create E 566 */
static INT xprfxflags;
static INT xprfxcode;
static UCHAR x8130[2] = { 0x81, 0x30 };
static UCHAR x8131[2] = { 0x81, 0x31 };
static UCHAR x00000000[4] = { 0x00, 0x00, 0x00, 0x00 };
static INT clearUnusedRoutineParameters;

/* local function prototypes */
static void v58verbs(void);
/*
 * The Mac has fcvt but does not supply a prototype for it!
 */
#ifdef __MACOSX
CHAR *fcvt(DOUBLE, INT, INT *, INT *);
#endif

void dbciex()  /* instruction execution module */
{
	INT i1, i2, i3, i4, i5, i6, lastmod = 0, lastoff = 0;
	INT32 x1, x2;
	UCHAR *ptr;
	DAVB *davb;

	if (!prpget("retstack", NULL, NULL, NULL, (CHAR **) &ptr, 0)) callmax = atoi((CHAR *) ptr);
	if (callmax <= 0) callmax = CALLMAX;
	davb = NULL;
	/* Note that the return stack is not from memalloc, it is from the heap */
	rstack = (RETURNSTACK *) malloc(callmax * sizeof(RETURNSTACK));
	if (rstack == NULL) badanswer(ERR_NOMEM);
	fpiretptr = makeretptr = callmax;
	bitopflg = olddisableflg = FALSE;
	if (!prpget("bitop", NULL, NULL, NULL, (CHAR **) &ptr, PRP_LOWER) && !strcmp((CHAR *) ptr, "old"))
			bitopflg = TRUE;
	if (!prpget("disable", NULL, NULL, NULL, (CHAR **) &ptr, PRP_LOWER) && !strcmp((CHAR *) ptr, "old"))
			olddisableflg = TRUE;
	if (!prpget("cxcompat", NULL, NULL, NULL, (CHAR **) &ptr, PRP_LOWER) && !strcmp((CHAR *) ptr, "old"))
			oldcxcompatflg = TRUE;
	if (!prpget("routine", "clearparmlist", NULL, NULL, (CHAR **) &ptr, PRP_LOWER)) {
		/* 'yes' is preferred */
		if (!strcmp((CHAR *) ptr, "yes") || !strcmp((CHAR *) ptr, "true") || !strcmp((CHAR *) ptr, "on")) {
			clearUnusedRoutineParameters = TRUE;
		}
	}
	utilerror[0] = '\0';
	i1 = newmod(MTYPE_CHAIN, name, NULL, NULL, -1);  /* start answer program */
	if (i1) {
		badanswer(i1);
	}
	if (dbcflags & DBCFLAG_DEBUG) action = ACTION_DEBUG;
	else action = 0;
	evtactionflag(&action, ACTION_POLL);
	setjmp(errjmp);

next:
	if (action) {
		if (action & (ACTION_XPREFIX | ACTION_FILEPI)) {
			if (action & ACTION_XPREFIX) {
				action &= ~ACTION_XPREFIX;
				dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | xprfxflags;
				vbcode = xprfxcode;
				goto next1;
			}
			if (action & ACTION_FILEPI) {
				if (fpicnt > 1) fpicnt--;
				else {
					if (fpicnt == 1) filepi0();
					if (!fpicnt) action &= ~ACTION_FILEPI;
				}
			}
		}
next1:
		if (action & (ACTION_POLL | ACTION_ERROR | ACTION_EVENT | ACTION_DEBUG | ACTION_SHUTDOWN)) {
			if (action & ACTION_POLL) {
				evtpoll();
				if (!(action & ~ACTION_POLL)) goto next3;
			}
			if (action & ACTION_ERROR) {
				action &= ~ACTION_ERROR;
				i1 = retptr;
				errorexec();
				if ((action & ACTION_FILEPI) && i1 < retptr) {  /* turn off filepi decrement */
					fpiretptr = i1;
					action &= ~ACTION_FILEPI;
				}
			}
			if (action & ACTION_SHUTDOWN) {
				dbcexit(0);
			}
			if ((action & ACTION_EVENT) && !(action & ACTION_XPREFIX)) {
				i1 = retptr;
				actionexec();
				if ((action & ACTION_FILEPI) && i1 < retptr) {  /* turn off filepi decrement */
					fpiretptr = i1;
					action &= ~ACTION_FILEPI;
				}
				if (action & ACTION_ERROR) goto next1;
			}
next2:
			if ((action & ACTION_DEBUG) && !(action & ACTION_XPREFIX)) {
				action &= ~ACTION_DEBUG;
				vdebug(DEBUG_CHECK);
			}
		}
	}
next3:
	lsvbcode = vbcode;
#if OS_UNIX && defined(Linux)
	setpgmdata();
#endif
	vbcode = getbyte();
	if (dbgflags & DBGFLAGS_DDT && counterBrkhi) {
		for (i1 = 0; i1 < counterBrkhi; i1++) {
			if (dbgCounterbrk[i1].mod == -1) continue;
			if (dbgCounterbrk[i1].pcnt == pcount - 1 && getmpgm(dbgCounterbrk[i1].mod) == pgm) {
				if (--dbgCounterbrk[i1].count == 0) {
					CHAR line[12];
					INT ptidx = dbgCounterbrk[i1].ptidx;
					dbctcxputtag(1, "CounterBreakToZero");
					sprintf(line, "%d", dbgCounterbrk[i1].line);
					dbctcxputattr(1, "line", line);
					dbctcxputattr(1, "module", dbgCounterbrk[i1].module);
					convertCounterBPToRealBP(1, i1);
					dbctcxput(1);
					vbcode = pgm[pgmtable[ptidx].pcnt] = VERB_DEBUG;
					break;
				}
			}
		}
	}
	switch(vbcode) {
		case 0x00: goto vzeroop;
		case 0x01: vclos(); goto next;
		case 0x02: i1 = 0x01; goto vmath;
		case 0x03: goto vappend;
		case 0x04: vbeep(); goto next;
		case 0x05: vbrpf(); goto next;
		case 0x06:
		case 0x07:
		case 0x08: goto vbump;
		case VERB_CHAIN: goto vchain;
		case 0x0A: goto vclear;
		case 0x0B: vclock(); goto next;
		case 0x0C:
		case 0x0D: goto vcmatch;
		case 0x0E:
		case 0x0F: goto vcmove;
		case 0x10: goto vcompare;
		case VERB_DEBUG: {
			vdebug(DEBUG_DEBUG); goto next;
		}
		case VERB_DELETE:	/* delete */
		case 0x13:	/* deletek */
			vdiotwo(); goto next;
		case 0x14:  /* display */
			vkyds(); goto next;
		case 0x15: i1 = 0x04; goto vmath;
		case 0x16: vccall(); goto next;
		case 0x17: vedit(0); goto next;
		case 0x18: goto vendset;
		case 0x19: vexecute(); goto next;
		case 0x1A: goto vextend;
		case 0x1B: vcount(); goto next;
		case 0x1C:
			vfilepi();
			if (fpicnt) action |= ACTION_FILEPI;
			goto next;
		case 0x1D:	/* fposit */
			vdiotwo(); goto next;
		case 0x1E: goto vgiving;
		case VERB_INSERT:	/* insert */
			vdiotwo(); goto next;
		case 0x20: /* keyin */
			enable();
			vkyds();
			goto next;
		case 0x21: goto vlenset;
		case 0x22: goto vldst; /* load (34) */
		case VERB_READKG:	/* readkg  (35) */
			vread(0); goto next;

		/* Might use 0x24 (36) for CLOCKCLIENT */
		case VERB_CLOCKCLIENT:	/* not used as of 16.2 */
			if (fpicnt) filepi0();  /* cancel any filepi */
			vclockClient(); goto next;

		case 0x25: goto vmatch;
		case 0x26: vmove(0); goto next;
		case 0x27: goto vmovefp;
		case 0x28: goto vmovelp;
		case 0x29: i1 = 0x03; goto vmath;
		case 0x2A: goto vmoveadr;
		case 0x2B: goto vnoret;
		case 0x2C /* 44 */:
		case 0x2D /* 45 */:
			vopen(); goto next;
		case 0x2E: /* ROLLOUT */
			vrollout(); goto next;
		case VERB_PACK: vpack(FALSE); goto next;
		case 0x30: vpause(); goto next;
		case 0x31: goto vtest;
		case 0x32: vprep(); goto next;
		case 0x33: vprint(); goto next;
		case 0x34:	/* read readtab */
			vread(0); goto next;
		case 0x35: goto vbadop; /* (53) not used */
		case 0x36:	/* readkp readkptb */
			vread(0); goto next;
		case 0x37: goto vbadop;	/* not used */
		case 0x38:	/* readks readkstb */
			vread(0); goto next;
		case 0x39: goto vbadop;	/* not used */
		case 0x3A: vrelease(); goto next;
		case 0x3B: vrep(); goto next;
		case 0x3C:	/* reposit */
			vdiotwo(); goto next;
		case 0x3D:
			vsplclose(); goto next;
		case 0x3E:
		case 0x3F:
		case 0x40:
			goto vreset;
		case 0x41: vscan(); goto next;
		case 0x42: vsearch(); goto next;
		case 0x43:
		case 0x44:
		case 0x45: goto vsetlptr;
		case 0x46:							/* splopen (70) */
		case 0x47: vsplopen(); goto next;	/* splopen with pfile (71) */
		case 0x48: goto vldst;				/* store (72) */
		case 0x49: vtrap(); goto next;
		case 0x4A: i1 = 0x02; goto vmath;
		case 0x4B: vtrap(); goto next;
		case 0x4C: vtrapclr(); goto next;
		case 0x4D: goto vtype;
		case 0x4E: {
			dbcexit(0);
		} // @suppress("No break at end of case")
		case 0x4F: vunpack(); goto next;
		case 0x50:
		case 0x51: vwrite(); goto next;
		case 0x52: vdiotwo(); goto next;
		case 0x53:
		case 0x54: vwrite(); goto next;
		case 0x55: vcom(); goto next;
		case 0x56: vnformat(); goto next;
		case 0x57: vsformat(); goto next;
		case 0x58: v58verbs(); goto next;
		case 0x59: goto vset;
		case 0x5A: vreturn(); goto next;
		case 0x5B:
		case 0x5C:
		case 0x5D:
		case 0x5E:
		case 0x5F:
		case 0x60:
		case 0x61:
		case 0x62: goto vreturnc;
		case 0x63: goto vreturnx;
		case 0x64: vstop(NULL, 0); goto next;
		case 0x65:
		case 0x66:
		case 0x67:
		case 0x68:
		case 0x69:
		case 0x6A:
		case 0x6B:
		case 0x6C: goto vstopc;
		case 0x6D: goto vstopx;
		case 0x6E: goto vroutine;
		case 0x6F: goto vcallp;  /* CALL with parameters */
		case 0x70:
		case 0x71:
		case 0x72:
		case 0x73: goto vcall;
		case 0x74: // goto (116)
		case 0x75: // (117)
		case 0x76: // (118)
		case 0x77: // (119)
			goto vgoto;
		case 0x78:
		case 0x79:
		case 0x7A:
		case 0x7B: goto vcallx;
		case 0x7C:
		case 0x7D:
		case 0x7E:
		case 0x7F: goto vgotox;
		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85:
		case 0x86:
		case 0x87:
		case 0x88:
		case 0x89:
		case 0x8A:
		case 0x8B: /* switch? */
		case 0x8C: /* case? */
		case 0x8D: /* default */
		case 0x8E: /* endswitch? */
		case 0x8F:
		case 0x90:
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x96:
		case 0x97:
		case 0x98:
		case 0x99:
		case 0x9A:
		case 0x9B:
		case 0x9C:
		case 0x9D:
		case 0x9E:
		case 0x9F: goto vcallc;
		case 0xA0: /* (160) */
		case 0xA1:
		case 0xA2:
		case 0xA3:
		case 0xA4:
		case 0xA5:
		case 0xA6:
		case 0xA7:
		case 0xA8:
		case 0xA9:
		case 0xAA:
		case 0xAB:
		case 0xAC:
		case 0xAD:
		case 0xAE:
		case 0xAF:
		case 0xB0:
		case 0xB1:
		case 0xB2:
		case 0xB3:
		case 0xB4:
		case 0xB5:
		case 0xB6:
		case 0xB7:
		case 0xB8:
		case 0xB9:
		case 0xBA:
		case 0xBB:
		case 0xBC:
		case 0xBD:
		case 0xBE:
		case 0xBF: goto vgotoc;
		case 0xC0: // _XPREFIX (192)
			action |= ACTION_XPREFIX;
			xprfxflags = dbcflags & DBCFLAG_FLAGS;
			xprfxcode = lsvbcode;
			goto next3;
		case 0xC1:
		case 0xC2: goto _gotosetclr;
		case 0xC3:
		case 0xC4:
		case 0xC5:
		case 0xC6:
		case 0xC7:
		case 0xC8: goto _gotoifx;
		case 0xC9: goto _goto;
		case 0xCA: goto _gotox;
		case 0xCB: goto _call;
		case 0xCC: goto _callx;
		case 0xCD: goto vcallp;
		case 0xCE: goto _gotofor;
		case 0xCF: goto vcallp;
	}
	if (vbcode == 0xFF) {
		if (action & ACTION_FILEPI) fpicnt++;
		goto next;
	}
vbadop:
	dbcerrinfo(505, (UCHAR*)"Bad opcode encountered", -1);
	goto next;

vzeroop:
	dbcerrinfo(505, (UCHAR*)"Zero opcode encountered", -1);
	goto next;

vldst:
	adr1 = getvar(VAR_READ);
	i2 = nvtoi(getvar(VAR_READ));
	i3 = (vbcode == 0x22) ? LIST_READ : LIST_WRITE;
	i3 |= LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1;
	for (i1 = 1; (adr2 = getlist(i3)) != NULL; i1++) {
		if (i1 == i2) {
			if (vbcode == 0x22) {/* load */
				movevar(adr2, adr1);
			}
			else movevar(adr1, adr2);
			getlist(LIST_PROG | LIST_END | LIST_NUM1);
			break;
		}
	}
	goto next;

vmoveadr:
	i1 = datamodule;
	getvarx(pgm, &pcount, &i1, &i4);
	adr1 = getvar(VAR_ADR);
	adr1[1] = (UCHAR) i1;
	adr1[2] = (UCHAR)(i1 >> 8);
	adr1[3] = (UCHAR) i4;
	adr1[4] = (UCHAR)((UINT) i4 >> 8);
	adr1[5] = (UCHAR)(i4 >> 16);
	chkavar(adr1, TRUE);
	goto next;

vmath:
	adr1 = getint(work6a);
	i2 = vartype;
	adr2 = getvar(VAR_WRITE);
	if ((i2 | vartype) & TYPE_ARRAY) arraymath(i1, adr1, adr2, adr2);
	else mathop(i1, adr1, adr2, adr2);
	goto next;

vcompare:
	adr1 = getint(work6a);
	i1 = vartype;
	adr2 = getint(work6b);
	if ((i1 | vartype) & TYPE_ARRAY) arraymath(0x00, adr1, adr2, NULL);
	else mathop(0x00, adr1, adr2, NULL);
	goto next;

vgiving:
	i1 = getbyte();
	if (action & ACTION_XPREFIX) vmathexp(i1);
	else {
		adr1 = getint(work6a);
		i2 = vartype;
		adr2 = getint(work6b);
		i3 = vartype;
		adr3 = getvar(VAR_WRITE);
		if ((i2 | i3) & TYPE_ARRAY) arraymath(i1, adr1, adr2, adr3);
		else mathop(i1, adr1, adr2, adr3);
	}
	goto next;

vtest:
	adr1 = getint(work6a);
	if (vartype & TYPE_NUMERIC) {
		if (vartype & TYPE_INT) { /* int */
			memcpy((UCHAR *) &x1, &adr1[2], sizeof(INT32));
			dbcflags &= ~(DBCFLAG_EQUAL | DBCFLAG_LESS);
			if (x1 == 0) dbcflags |= DBCFLAG_EQUAL;
			else if (x1 < 0) dbcflags |= DBCFLAG_LESS;
		}
		else mathop(0x00, x8130, adr1, NULL);
	}
	else if (vartype & TYPE_CHARACTER) {
		if (fp) dbcflags &= ~DBCFLAG_EOS;
		else dbcflags |= DBCFLAG_EOS;
	}
	else {  /* assume davb type */
		if ((aligndavb(adr1, NULL))->refnum) dbcflags &= ~DBCFLAG_OVER;
		else dbcflags |= DBCFLAG_OVER;
	}
	goto next;

vappend:
	adr1 = getvar(VAR_READ);
	if (vartype & (TYPE_INT | TYPE_FLOAT | TYPE_NULL)) { /* INT, FLOAT or NULL */
		if (vartype & TYPE_NUMVAR) {
			nvmkform(adr1, work45);
			adr1 = work45;
		}
	}
	fp1 = fp;
	lp1 = lp;
	hl1 = hl;
	adr2 = getvar(VAR_WRITE);
	dbcflags &= ~DBCFLAG_EOS;
	if (!fp1) goto next;
	i1 = fp1 + hl1 - 1;
	i3 = lp1 - fp1 + 1;
	i2 = pl - fp;
	if (i2 < i3) {
		dbcflags |= DBCFLAG_EOS;
		i3 = i2;
	}
	i2 = fp + hl;
	lp = (fp += i3);
	setfplp(adr2);
	if (adr1 == adr2) while(i3--) adr2[i2++] = adr1[i1++];
	else memcpy(&adr2[i2], &adr1[i1], i3);
	goto next;

vclear:
	adr1 = getvar(VAR_WRITE);
	if (adr1[0] < 128) {
		adr1[0] = adr1[1] = 0;  /* do non-list small dim fast */
		goto next;
	}
	if (vartype & (TYPE_LIST | TYPE_ARRAY)) {
		adr1 = getlist(LIST_WRITE | LIST_LIST | LIST_ARRAY | LIST_NUM1);
		if (adr1 == NULL) goto next;
		i1 = TRUE;
	}
	else i1 = FALSE;
	do {
		if (vartype & TYPE_NUMVAR) {  /* numeric */
			i2 = dbcflags & DBCFLAG_FLAGS;
			movevar(x8130, adr1);
			dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i2;
		}
		else if (vartype & TYPE_CHAR) {
			fp = lp = 0;
			setfplp(adr1);
		}
	} while (i1 && ((adr1 = getlist(LIST_WRITE | LIST_LIST | LIST_ARRAY | LIST_NUM1)) != NULL));
	goto next;

vset:
	adr1 = getvar(VAR_WRITE);
	if (vartype & (TYPE_LIST | TYPE_ARRAY)) {
		adr1 = getlist(LIST_WRITE | LIST_LIST | LIST_ARRAY | LIST_NUM1);
		if (adr1 == NULL) goto next;
		i1 = TRUE;
	}
	else i1 = FALSE;
	i2 = dbcflags & DBCFLAG_FLAGS;
	do movevar(x8131, adr1);
	while (i1 && ((adr1 = getlist(LIST_WRITE | LIST_LIST | LIST_ARRAY | LIST_NUM1)) != NULL));
	dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i2;
	goto next;

vmatch:
	dbcflags &= ~(DBCFLAG_EOS | DBCFLAG_EQUAL);
	adr1 = getvar(VAR_READ);
	fp1 = fp;
	lp1 = lp;
	hl1 = hl;
	adr2 = getvar(VAR_READ);
	fp2 = fp;
	lp2 = lp;
	hl2 = hl;
	if (!fp1 || !fp2) {
		dbcflags |= DBCFLAG_EOS;
		goto next;
	}
	i1 = lp1 - fp1 + 1;
	i2 = lp2 - fp2 + 1;
	if (i1 > i2) {
		i1 = i2;
		dbcflags |= DBCFLAG_LESS;
	}
	else dbcflags &= ~DBCFLAG_LESS;
	adr1 += fp1 + hl1 - 1;
	adr2 += fp2 + hl2 - 1;
	while (i1 && *adr1++ == *adr2++) i1--;
	if (!i1) dbcflags |= DBCFLAG_EQUAL;
	else {
		if (*(adr1 - 1) > *(adr2 - 1)) dbcflags |= DBCFLAG_LESS;
		else dbcflags &= ~DBCFLAG_LESS;
	}
	goto next;

vlenset:
	adr1 = getvar(VAR_WRITE);
	if (adr1[0] < 128) adr1[1] = adr1[0];
	else {
		adr1[3] = adr1[1];
		adr1[4] = adr1[2];
	}
	goto next;

vendset:
	adr1 = getvar(VAR_WRITE);
	if (adr1[0] < 128) adr1[0] = adr1[1];
	else {
		adr1[1] = adr1[3];
		adr1[2] = adr1[4];
	}
	goto next;

vextend:
	adr1 = getvar(VAR_WRITE);
	if (fp == pl) dbcflags |= DBCFLAG_EOS;
	else {
		adr1[hl + fp] = ' ';
		lp = ++fp;
		setfplp(adr1);
		dbcflags &= ~DBCFLAG_EOS;
	}
	goto next;

vtype:
	adr1 = getvar(VAR_READ);
	if (!fp) {
		dbcflags &= ~DBCFLAG_EQUAL;
		dbcflags |= DBCFLAG_EOS;
	}
	else {
		dbcflags &= ~(DBCFLAG_EQUAL | DBCFLAG_EOS);
		if (typ(adr1)) dbcflags |= DBCFLAG_EQUAL;
	}
	goto next;

vmovefp:
	adr1 = getvar(VAR_READ);
	x1 = fp;
	work45[0] = 0xFC;
	work45[1] = 0x05;
	memcpy(&work45[2], (UCHAR *) &x1, sizeof(INT32));
	adr1 = getvar(VAR_WRITE);
	if (action & ACTION_XPREFIX) memcpy(adr1, work45, 6);
	else {
		i1 = dbcflags & DBCFLAG_LESS;
		movevar(work45, adr1);
		dbcflags = (dbcflags & ~DBCFLAG_LESS) | i1;
	}
	goto next;

vmovelp:
	adr1 = getvar(VAR_READ);
	x1 = lp;
	work45[0] = 0xFC;
	work45[1] = 0x05;
	memcpy(&work45[2], (UCHAR *) &x1, sizeof(INT32));
	adr1 = getvar(VAR_WRITE);
	if (action & ACTION_XPREFIX) memcpy(adr1, work45, 6);
	else {
		i1 = dbcflags & DBCFLAG_LESS;
		movevar(work45, adr1);
		dbcflags = (dbcflags & ~DBCFLAG_LESS) | i1;
	}
	goto next;

vcmatch:
	dbcflags |= DBCFLAG_EOS;
	dbcflags &= ~(DBCFLAG_LESS | DBCFLAG_EQUAL);
	if (vbcode == 0x0D) c1 = getbyte();
	else {
		adr1 = getvar(VAR_READ);
		if (!fp) {
			getvar(VAR_READ);
			goto next;
		}
		c1 = adr1[fp + hl - 1];
	}
	adr2 = getvar(VAR_READ);
	if (!fp) goto next;
	c2 = adr2[fp + hl - 1];
	dbcflags &= ~DBCFLAG_EOS;
	if (c1 == c2) dbcflags |= DBCFLAG_EQUAL;
	else {
		i1 = c2;
		if ((UCHAR) i1 < c1) dbcflags |= DBCFLAG_LESS;
	}
	goto next;

vcmove:
	dbcflags |= DBCFLAG_EOS;
	if (vbcode == 0x0F) c1 = getbyte();
	else {
		adr1 = getvar(VAR_READ);
		if (!fp) {
			getvar(VAR_READ);
			goto next;
		}
		c1 = adr1[fp + hl - 1];
	}
	adr2 = getvar(VAR_WRITE);
	if (!fp) goto next;
	adr2[fp + hl - 1] = c1;
	dbcflags &= ~DBCFLAG_EOS;
	goto next;

vbump:
	adr1 = getvar(VAR_WRITE);
	fp1 = fp;
	lp1 = lp;
	if (vbcode == 0x06) i1 = 1;
	else if (vbcode == 0x07) {
		i1 = getbyte();
		if (i1 >= 0x80) i1 -= 256;
	}
	else i1 = nvtoi(getvar(VAR_READ));
	fp = fp1 + i1;
	if (fp < 1 || fp > lp1) dbcflags |= DBCFLAG_EOS;
	else {
		setfp(adr1);
		dbcflags &= ~DBCFLAG_EOS;
	}
	goto next;

vreset:
	adr1 = getvar(VAR_WRITE);
	lp1 = lp;
	i2 = pl;
	if (vbcode == 0x3E) i1 = 1;
	else if (vbcode == 0x3F) {
		i1 = getbyte();
		if (i1 >= 0x80) i1 -= 256;
	}
	else {
		adr2 = getvar(VAR_READ);
		if (vartype & TYPE_NUMERIC) i1 = nvtoi(adr2);
		else {
			if (fp) i1 = (INT) (adr2[fp + hl - 1]) - 31;
			else i1 = 0;
		}
	}
	dbcflags |= DBCFLAG_EOS;
	if (i1 >= 0 && i1 <= i2) {
		fp = i1;
		lp = lp1;
		if (i1 > lp) lp = i1;
		else dbcflags &= ~DBCFLAG_EOS;
		setfplp(adr1);
	}
	else if (i1 > i2) {
		fp = lp = i2;
		setfplp(adr1);
	}
	goto next;

vsetlptr:
	adr1 = getvar(VAR_WRITE);
	fp1 = fp;
	i2 = pl;
	if (vbcode == 0x43) i1 = i2;
	else if (vbcode == 0x44) {
		i1 = getbyte();
		if (i1 >= 0x80) i1 -= 256;
	}
	else {
		adr2 = getvar(VAR_READ);
		if (vartype & TYPE_NUMERIC) i1 = nvtoi(adr2);
		else {
			if (fp) i1 = (INT) (adr2[fp + hl - 1]) - 31;
			else i1 = 0;
		}
	}
	dbcflags &= ~(DBCFLAG_EOS | DBCFLAG_OVER);
	if (i1 < 0 || i1 > i2) dbcflags |= DBCFLAG_OVER;
	else {
		lp = i1;
		fp = fp1;
		if (i1 < fp) {
			fp = i1;
			dbcflags |= DBCFLAG_EOS;
		}
		setfplp(adr1);
	}
	goto next;

/**
 * The <label> construct is a two byte field that represents the execution
 * label table entry number in HHLL format.  The <8 bit label> construct is
 * a one byte field that, in conjunction with the two low order bits of the operation code
 * in the preceeding byte, is used to form a ten bit value.
 * The two bits from the operation code are used as the high order bits in the ten bit value.
 * This ten bit value allows for specification of execution label table entry numbers
 * with values from 0 to 1022.
 * If the value is 1023, then this specifies that the following two bytes are
 * the standard HHLL format label specification.
 */
vgoto: // 0x74 -> -0x77
	i1 = getbyte() + ((vbcode & 0x03) << 8);
	if (i1 == 0x03FF) i1 = gethhll();
	chgpcnt(i1);
	goto next1;

vgotoc: // 0xA0 -> 0xBF
	if (chkcond(vbcode >> 2 & 0x07)) goto vgoto;
	if (getbyte() == 0xFF && (vbcode & 0x03) == 0x03) pcount += 2;
	goto next1;

vgotox:
	i1 = getbyte() + ((vbcode & 0x03) << 8);
	if (i1 == 0x03FF) i1 = gethhll();
	if (chkcond(getbyte())) chgpcnt(i1);
	goto next1;

vcall:
	i1 = getbyte() + ((vbcode & 0x03) << 8);
	if (i1 == 0x03FF /* 1,023 */) i1 = gethhll();
	if (pushreturnstack(-1)) {
		dbcerror(501);
		goto next;
	}
	chgpcnt(i1);
	goto next1;

vcallc:
	if (chkcond(vbcode >> 2 & 0x07)) {
		i1 = getbyte() + ((vbcode & 0x03) << 8);
		if (i1 == 0x03FF) i1 = gethhll();
		if (pushreturnstack(-1)) {
			dbcerror(501);
			goto next;
		}
		chgpcnt(i1);
	}
	else if (getbyte() == 0xFF && (vbcode & 0x03) == 0x03) pcount += 2;
	goto next1;

vcallx:
	i1 = getbyte() + ((vbcode & 0x03) << 8);
	if (i1 == 0x03FF) i1 = gethhll();
	if (chkcond(getbyte())) {
		if (pushreturnstack(-1)) {
			dbcerror(501);
			goto next;
		}
		chgpcnt(i1);
	}
	goto next1;

vcallp:
	if (vbcode == 0xCD /* 205 */) {
		i4 = llmmhh(&pgm[pcount]);
		pcount += 3;
	}
	else {
		// The <label> construct is a two byte field that represents the
		// execution label table entry number in HHLL format.
		i4 = gethhll();
		if (vbcode == 0xCF) {
			davb = getdavb();
			if (!davb->refnum) {
				dbcerror(563);
				goto next;
			}
			getlastvar(&lastmod, &lastoff);
		}
	}
	calldmod = datamodule;
	callpmod = pgmmodule;
	callpcnt = pcount;
	i1 = FALSE;
	i3 = getmver(pgmmodule); /* module DX version */
	while ((c1 = pgm[pcount]) < 0xF8) {
		if (i3 >= 8) {
			if (c1 == 0xC1 && pgm[pcount + 1] == 0x80) pcount += 4;
			else getvar(VAR_READ);
		}
		else {  /* pre-version 8 */
			if (c1 >= 0xD0 && c1 <= 0xEF) pcount += 2;
			else getvar(VAR_READ);
		}
		i1 = TRUE;
	}
	if (pgm[pcount] != 0xFF) {
		i5 = pcount;
		do {
			if ((i2 = getbyte() - 0xF7) == 7) i2 = 1;
			while (i2--) {
				if ((c1 = pgm[pcount]) == 0xFF) pcount += 2;  /* skip decimal contants */
				else if (i3 >= 8) {
					if (c1 == 0xC1 && pgm[pcount + 1] == 0x80) pcount += 4;
					else getvar(VAR_READ);
				}
				else {
					if (c1 >= 0xD0 && c1 <= 0xEF) pcount += 2;
					else getvar(VAR_READ);
				}
			}
		} while (pgm[pcount] != 0xFF);
	}
	else i5 = -1;
	if (!i1) callpcnt = pcount;
	pcount++;
	if (pushreturnstack(i5)) {
		dbcerror(501);
		goto next;
	}
	if (vbcode == 0x6F /* 111  call parms... */) chgpcnt(i4);
	else if (vbcode == 0xCF /* 207  call method...*/) {
		rstack[retptr - 1].objdata = lastmod;
		rstack[retptr - 1].objoff = lastoff;
		chgmethod(i4, davb->refnum - 1);
	}
	else pcount = i4;
	goto next2;

_goto:
	pcount = llmmhh(&pgm[pcount]);
	goto next1;

_gotox:
	i1 = getbyte();
	if (i1 < 10) {
		switch (i1) {
			case 0: if (dbcflags & DBCFLAG_EQUAL) goto _goto; break;
			case 1: if (dbcflags & DBCFLAG_LESS) goto _goto; break;
			case 2: if (dbcflags & DBCFLAG_OVER) goto _goto; break;
			case 3: if (dbcflags & DBCFLAG_EOS) goto _goto; break;
			case 4: if (!(dbcflags & DBCFLAG_EQUAL)) goto _goto; break;
			case 5: if (!(dbcflags & DBCFLAG_LESS)) goto _goto; break;
			case 6: if (!(dbcflags & DBCFLAG_OVER)) goto _goto; break;
			case 7: if (!(dbcflags & DBCFLAG_EOS)) goto _goto; break;
			case 8: if (!(dbcflags & (DBCFLAG_LESS | DBCFLAG_EQUAL))) goto _goto; break;
			case 9: if (dbcflags & (DBCFLAG_LESS | DBCFLAG_EQUAL)) goto _goto; break;
		}
	}
	else if (chkcond(i1)) goto _goto;
	pcount += 3;
	goto next2;

_gotosetclr:
	adr1 = getint(work6a);
	if (vartype & TYPE_NUMERIC) { /* numeric */
		if (vartype & TYPE_INT) {
			if (!memcmp(&adr1[2], x00000000, 4)) fp = 0;
			else fp = 1;
		}
		else {
			i1 = dbcflags & DBCFLAG_FLAGS;
			mathop(0x00, x8130, adr1, NULL);
			if (dbcflags & DBCFLAG_EQUAL) fp = 0;
			else fp = 1;
			dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1;
		}
	}
	if (vbcode == 0xC1) {
		if (fp) goto _goto;
	}
	else if (!fp) goto _goto;
	pcount += 3;
	goto next2;

_gotoifx:
	adr1 = getint(work6a);
	if (vartype & TYPE_NUMERIC) {  /* numeric */
		i1 = vartype;
		adr2 = getint(work6b);
		if ((i1 & TYPE_INT) && (vartype & TYPE_INT)) {
			memcpy((UCHAR *) &x1, &adr1[2], sizeof(INT32));
			memcpy((UCHAR *) &x2, &adr2[2], sizeof(INT32));
			if (x1 == x2) i1 = 0;
			else if (x1 < x2) i1 = -1;
			else i1 = 1;
		}
		else {
			i2 = dbcflags & DBCFLAG_FLAGS;
			mathop(0x00, adr2, adr1, NULL);
			if (dbcflags & DBCFLAG_EQUAL) i1 = 0;
			else if (dbcflags & DBCFLAG_LESS) i1 = -1;
			else i1 = 1;
			dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i2;
		}
	}
	else {
		if (!fp) i1 = -1;
		else {
			i1 = 0;
			i2 = lp - fp + 1;
			adr1 += hl + fp - 1;
		}
		adr2 = getvar(VAR_READ);
		if (!fp) i1++;
		else if (!i1) {
			i3 = lp - fp + 1;
			adr2 += hl + fp - 1;
			i6 = (i2 <= i3) ? i2 : i3;
			i1 = memcmp(adr1, adr2, i6);
			if (!i1) i1 = i2 - i3;
		}
	}
	switch (vbcode) {
		case 195: if (i1 == 0) goto _goto; break;
		case 196: if (i1 != 0) goto _goto; break;
		case 197: if (i1 < 0) goto _goto; break;
		case 198: if (i1 > 0) goto _goto; break;
		case 199: if (i1 <= 0) goto _goto; break;
		case 200: if (i1 >= 0) goto _goto; break;
	}
	pcount += 3;
	goto next2;

_gotofor:
	adr1 = getint(work6a);
	vbcode = 0xC6;
	if (vartype & TYPE_INT) {
		memcpy((UCHAR *) &x1, &adr1[2], sizeof(INT32));
		if (x1 < 0) vbcode = 0xC5 /* _gotoiflt */;
	}
	else {
		i1 = dbcflags & DBCFLAG_FLAGS;
		mathop(0x00, x8130, adr1, NULL);
		if (dbcflags & DBCFLAG_LESS) vbcode = 0xC5;
		dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1;
	}
	goto _gotoifx;

_call:
	i4 = llmmhh(&pgm[pcount]);
	pcount += 3;
	if (pushreturnstack(-1)) {
		dbcerror(501);
		goto next;
	}
	pcount = i4;
	goto next2;

_callx:
	i1 = getbyte();
	if (i1 < 10) {
		switch (i1) {
			case 0: if (dbcflags & DBCFLAG_EQUAL) goto _call; break;
			case 1: if (dbcflags & DBCFLAG_LESS) goto _call; break;
			case 2: if (dbcflags & DBCFLAG_OVER) goto _call; break;
			case 3: if (dbcflags & DBCFLAG_EOS) goto _call; break;
			case 4: if (!(dbcflags & DBCFLAG_EQUAL)) goto _call; break;
			case 5: if (!(dbcflags & DBCFLAG_LESS)) goto _call; break;
			case 6: if (!(dbcflags & DBCFLAG_OVER)) goto _call; break;
			case 7: if (!(dbcflags & DBCFLAG_EOS)) goto _call; break;
			case 8: if (!(dbcflags & (DBCFLAG_LESS | DBCFLAG_EQUAL))) goto _call; break;
			case 9: if (dbcflags & (DBCFLAG_LESS | DBCFLAG_EQUAL)) goto _call; break;
		}
	}
	else if (chkcond(i1)) goto _call;
	pcount += 3;
	goto next2;

vreturnc:
	if (chkcond(vbcode - 0x5B)) vreturn();
	goto next1;

vreturnx:
	if (chkcond(getbyte())) vreturn();
	goto next1;

vnoret:
	/* pop stack if not part of make or destroy call path */
	if (retptr <= makeretptr || rstack[retptr - 1].makepgm == -1) {
		i1 = pgmmodule;
		i2 = pcount;
		if (popreturnstack()) dbcflags |= DBCFLAG_OVER;
		else dbcflags &= ~DBCFLAG_OVER;
		pgmmodule = i1;
		pcount = i2;
	}
	else dbcflags |= DBCFLAG_OVER;
	goto next1;

	/*
	 * 0x6F = 111 = CALL parms
	 * 0xCD = 205 = _CALL parms
	 * 0xCF = 207 = CALL mthd,obj,parm
	 */
vroutine:
	if (lsvbcode == 0x6F || lsvbcode == 0xCD || lsvbcode == 0xCF) {
		if ((ptr = getmpgm(callpmod)) == NULL) {
			dbcerrinfo(505, (UCHAR*)"Failure calling a routine", -1);
			goto next1;
		}
		i1 = TRUE;  // Are there parameters because we got here from a CALL?
	}
	else i1 = FALSE;

	i3 = getmver(pgmmodule);
	/*
	 * A routine or lroutine  is 110 (decimal) followed by an <adr list>
	 * 0xFF signals the end of the <adr list> construct
	 */
	while ((c1 = pgm[pcount]) != 0xFF) {
		i6 = 0xFFFF;
		if (i3 >= 8) {
			if (c1 == 0xC1 && pgm[pcount + 1] == 0x80) {
				pcount += 2;
				i6 = gethhll();
			}
			else adr1 = getvar(VAR_ADR);
		}
		else {
			if (c1 >= 0xD0 && c1 <= 0xEF) i6 = gethhll() - 0xD000;
			else adr1 = getvar(VAR_ADR);
		}
		/* check for invalid code, this should not happen */
		if (i6 == 0xFFFF && (adr1[0] < 0xB0 || adr1[0] > 0xC7)) {
			dbcerrinfo(505, (UCHAR*)"Failure parsing param list on a call", -1);
			goto next;
		}
		if (i1) {
			if ((c1 = ptr[callpcnt]) < 0xF8) {
				i2 = calldmod;
				if (i3 >= 8) {
					if (c1 == 0xC1 && ptr[callpcnt + 1] == 0x80) {
						callpcnt += 2;
						i4 = 0x00800000 + (ptr[callpcnt++] << 8);
						i4 += ptr[callpcnt++];
					}
					else getvarx(ptr, &callpcnt, &i2, &i4);
				}
				else {
					if (c1 >= 0xD0 && c1 <= 0xEF) {
						callpcnt++;
						i4 = 0x00800000 + ((c1 - 0xD0) << 8);
						i4 += ptr[callpcnt++];
					}
					else getvarx(ptr, &callpcnt, &i2, &i4);
				}
				if (i6 == 0xFFFF) {  /* not a label type */
					adr1[1] = (UCHAR) i2;
					adr1[2] = (UCHAR)(i2 >> 8);
					adr1[3] = (UCHAR) i4;
					adr1[4] = (UCHAR)((UINT) i4 >> 8);
					adr1[5] = (UCHAR)(i4 >> 16);
					chkavar(adr1, TRUE);
				}
				else if ((UCHAR)(i4 >> 16) == 0x80) {  /* label */
					i5 = (USHORT) i4;
					movelabel(i2, i5, i6);
				}
			}
			else {
				i1 = FALSE;
			}
		}
		/*
		 * If the routine parameter just looked at does not have a matching argument
		 * and is not a label var, clear it, maybe.
		 */
//		if (!i1 && i6 == 0xFFFF) {
//			if (clearUnusedRoutineParameters) memset(&adr1[1], 0xFF, 5);
//		}
		if (!i1 && clearUnusedRoutineParameters) {
			if (i6 == 0xFFFF) memset(&adr1[1], 0xFF, 5);
			else clearlabel(i6);
		}
	}
	pcount++;
	goto next1;

vchain:
	if (fpicnt) filepi0();  /* cancel any filepi */

	i2 = pcount;
	i3 = retptr;
	cvtoname(getvar(VAR_READ));
	if (fp) {
		i1 = newmod(MTYPE_CHAIN, name, NULL, NULL, i2 - 1);
	}
	else i1 = 101;

	if (i1) dbcerror(i1);
	else if (i3 >= retptr) {
		kdschain();
		trapclearall();
		enable();
		/*vsql(0);*/
	}
	goto next;

vstopc:
	if (!chkcond(vbcode - 0x65)) goto next1;
	vstop(NULL, 0);
	goto next;

vstopx:
	if (!chkcond(getbyte())) goto next1;
	vstop(NULL, 0);
	goto next;
}

/**
 * Other verbs, prefixed by 88 (0x58)
 */
static void v58verbs()
{
	INT i1, i2, i3, i4, i5, i6, i7;
	INT32 x1;
	CHAR work[512], *chrptr, *workptr;
	UCHAR flags[4], *ptr;
	DOUBLE64 y1, y2;
	DAVB *davb;
	ELEMENT *e1;

	vx = getbyte();

#if 0 && OS_UNIX && defined(Linux)
	if (keepCircularEventLog) logVxcode(vx);
#endif
	switch (vx) {
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:  /* check10, check11 */
			adr1 = getvar(VAR_READ);
			adr2 = getvar(VAR_READ);
			if (vx == 0x02 || vx == 0x04) adr3 = getvar(VAR_WRITE);
			else {
				work45[0] = work45[1] = work45[2] = 1;
				work45[3] = ' ';
				adr3 = work45;
			}
			if (vx < 0x03) i1 = 10;
			else i1 = 11;
			chk1011(i1, adr1, adr2, adr3);
			return;
		case 0x05:  /* or */
		case 0x06:  /* and */
		case 0x07:  /* xor */
		case 0x08:  /* not */
			fp1 = 1;
			if (pgm[pcount] == 0xFF) {
				i1 = pgm[pcount + 1];
				pcount += 2;
			}
			else {
				adr1 = getvar(VAR_READ);
				if (vartype & TYPE_NUMERIC) i1 = nvtoi(adr1);
				else {
					fp1 = fp;
					i1 = adr1[fp + hl - 1];
				}
			}
			adr2 = getvar(VAR_WRITE);
			if (vartype & TYPE_NUMVAR) {
				i2 = nvtoi(adr2);
				if (vx == 0x05) i1 = i1 | i2;  /* or */
				else if (vx == 0x06) i1 = i1 & i2;  /* and */
				else if (vx == 0x07) i1 = i1 ^ i2;  /* xor */
				else if (vx == 0x08) i1 = ~i1;  /* not */
				work45[0] = 0xFC;
				work45[1] = 0x0C;
				memcpy(&work45[2], (UCHAR *) &i1, sizeof(INT32));
				movevar(work45, adr2);
			}
			else {
				i2 = adr2[fp + hl - 1];
				if (!fp1 || !fp) {
					dbcflags |= DBCFLAG_EOS;
					return;
				}
				dbcflags &= ~DBCFLAG_EOS;
				if (vx == 0x05) i1 = i1 | i2;  /* or */
				else if (vx == 0x06) i1 = i1 & i2;  /* and */
				else if (vx == 0x07) i1 = i1 ^ i2;  /* xor */
				else if (vx == 0x08) i1 = ~i1;  /* not */
				if (!bitopflg) i1 &= 0xFF;
				else i1 &= 0x7F;
				adr2[fp + hl - 1] = (UCHAR) i1;
			}
			if (i1) dbcflags &= ~DBCFLAG_EQUAL;
			else dbcflags |= DBCFLAG_EQUAL;
			return;
		case 0x09:  /* loadmod */
			adr1 = getvar(VAR_READ);
			if (fp) {
				cvtoname(adr1);
				i1 = newmod(MTYPE_LOADMOD, name, NULL, NULL, -1);
			}
			else i1 = 101;
			if (i1) dbcerror(i1);
			return;
		case 0x0A:  /* close-delete */
			vclos();
			return;
		case 0x0B:  /* unload */
			vunload(FALSE, pcount - 2);
			return;
		case 0x0C:  /* sound */
			i1 = nvtoi(getvar(VAR_READ));
			i2 = nvtoi(getvar(VAR_READ));
			if (i1 > 0 && i2 > 0) {
				if (i2 > 1000) i2 = 1000;
				kdssound(i1 , i2);
			}
			return;
		case 0x0D:  /* mod */
			adr1 = getint(work6a);
			adr2 = getvar(VAR_WRITE);
			mathop(0x05, adr1, adr2, adr2);
			return;
		case 0x0E:  /* splopt */
			vsplopt();
			return;
		case 0x0F:  /* splclose with variable */
			vsplclose();
			return;
		case 0x10:  /* winsave */
			vscrnsave(0x01);
			return;
		case 0x11:  /* winrestore */
			vscrnrest(0x01);
			return;
		case 0x12:  /* flush */
			vflush();
			return;
		case 0x13:  /* flagsave */
			adr1 = getvar(VAR_WRITE);
			adr1[hl] = (dbcflags & DBCFLAG_EOS) ? '1' : '0';
			if (pl > 1)	adr1[hl + 1] = (dbcflags & DBCFLAG_EQUAL) ? '1' : '0';
			if (pl > 2) adr1[hl + 2] = (dbcflags & DBCFLAG_LESS) ? '1' : '0';
			if (pl > 3) adr1[hl + 3] = (dbcflags & DBCFLAG_OVER) ? '1' : '0';
			fp = 1;
			if (pl >= 4) lp = 4;
			else lp = pl;
			setfplp(adr1);
			return;
		case 0x14:  /* flagrestore variable */
		case 0x15:  /* flagrestore constant */
			if (vx == 0x14) {
				adr1 = getvar(VAR_READ);
				if (!fp) return;
				ptr = &adr1[fp + hl - 1];
			}
			else {
				ptr = &flags[0];
				flags[0] = getbyte();
				flags[1] = getbyte();
				flags[2] = getbyte();
				flags[3] = getbyte();
			}
			if (*ptr == '0') dbcflags &= ~DBCFLAG_EOS;
			else if (*ptr == '1') dbcflags |= DBCFLAG_EOS;
			ptr++;
			if (*ptr == '0') dbcflags &= ~DBCFLAG_EQUAL;
			else if (*ptr == '1') dbcflags |= DBCFLAG_EQUAL;
			ptr++;
			if (*ptr == '0') dbcflags &= ~DBCFLAG_LESS;
			else if (*ptr == '1') dbcflags |= DBCFLAG_LESS;
			ptr++;
			if (*ptr == '0') dbcflags &= ~DBCFLAG_OVER;
			else if (*ptr == '1') dbcflags |= DBCFLAG_OVER;
			ptr++;
			return;
		case 0x16:  /* console */
			vconsole();
			return;
		case 0x17:  /* retcount */
			vretcnt();
			return;
		case 0x18:  /* perform */
			vbrpf();
			return;
		case 0x19:  /* loadadr */
		case 0x1A:  /* storeadr */
/*** NOTE: loadadr/storeadr have been modified to support loading cleared ***/
/***       addresses unlike moveadr.  this was done because clearadr/testadr ***/
/***       can not be perfomed on a list element without changing the compiler. ***/
			if (vx == 0x19) adr1 = getvar(VAR_ADR);
			else {
				getvar(VAR_ADRX);
				getlastvar(&i1, &i5);
			}
			i2 = nvtoi(getvar(VAR_READ));
			/* determine if single list element */
			i6 = pcount;
			adr2 = getvar(VAR_ADRX);
			if (pgm[pcount] == 0xFF && (vartype & (TYPE_LIST | TYPE_ARRAY))) {
				i4 = LIST_LIST1 | LIST_ARRAY1 | LIST_NUM1;
				if (vx == 0x19) i4 |= LIST_ADRX;
				else i4 |= LIST_ADR;
				for (i3 = 1; ; i3++) {
					if (vx == 0x19) {
						if (!getlistx(i4, &i1, &i5)) break;
					}
					else if ((adr1 = getlist(i4)) == NULL) break;
					if (i3 == i2) {
						adr1[1] = (UCHAR) i1;
						adr1[2] = (UCHAR)(i1 >> 8);
						adr1[3] = (UCHAR) i5;
						adr1[4] = (UCHAR)((UINT) i5 >> 8);
						adr1[5] = (UCHAR)(i5 >> 16);
						if (i1 != 0xFFFF) chkavar(adr1, TRUE);
						break;
					}
					i4 &= ~(LIST_LIST1 | LIST_ARRAY1);
				}
				getlist(LIST_END | LIST_NUM1);
			}
			else {
				pcount = i6;
				for (i3 = 1; pgm[pcount] != 0xFF; i3++) {
					if (i3 == i2) {
						if (vx == 0x19) {
							getvar(VAR_ADRX);
							getlastvar(&i1, &i5);
						}
						else adr1 = getvar(VAR_ADR);
						adr1[1] = (UCHAR) i1;
						adr1[2] = (UCHAR)(i1 >> 8);
						adr1[3] = (UCHAR) i5;
						adr1[4] = (UCHAR)((UINT) i5 >> 8);
						adr1[5] = (UCHAR)(i5 >> 16);
						if (i1 != 0xFFFF) chkavar(adr1, TRUE);
						continue;
					}
					skipadr();
				}
				pcount++;
			}
			return;
		case 0x1B:  /* trapsave */
			vtrapsave();
			return;
		case 0x1C:  /* traprestore */
			vtraprestore();
			return;
		case 0x1D:  /* noeject */
			vnoeject();
			return;
		case 0x1E:  /* complex prepare */
			vbcode = 0x1E;
			vprep();
			return;
		case 0x1F:  /* complex aim prepare */
			vbcode = 0x1F;
			vprep();
			return;
		case 0x20:  /* unload (single) */
			i1 = pcount;
			cvtoname(getvar(VAR_READ));
			vunload(TRUE, i1 - 2);
			return;
		case 0x21:  /* complex aim prepare */
			vbcode = 0x21;
			vprep();
			return;

		case 0x22:  /* getpapernames 34 */
			vgetprintinfo(GETPRINTINFO_PAPERNAMES);
			return;

		case 0x23:  /* extend constant */
		case 0x24:  /* extend variable */
			adr1 = getvar(VAR_WRITE);
			fp1 = fp;
			i1 = pl;
			i2 = hl;
			if (vx == 0x24) i3 = nvtoi(getvar(VAR_READ));
			else i3 = getbyte();
			dbcflags &= ~DBCFLAG_EOS;
			if (i3 > i1 - fp1) {
				i3 = i1 - fp1;
				dbcflags |= DBCFLAG_EOS;
			}
			if (i3 > 0) {
				memset(&adr1[i2 + fp1], ' ', i3);
				lp = fp = fp1 + i3;
				setfplp(adr1);
			}
			return;
		case 0x25:  /* (37) fill (variable) */
		case 0x26:  /* (38) fill (byte) */
			if (vx == 0x25) {
				adr1 = getvar(VAR_READ);
				i2 = adr1[hl + fp - 1];
				i1 = fp;
			}
			else {
				i2 = getbyte();
				i1 = 1;
			}
			while ((adr1 = getlist(LIST_WRITE | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1)) != NULL) {
				if (i1 && (vartype & TYPE_CHAR)) {
					memset(&adr1[hl], i2, pl);
					fp = 1;
					lp = pl;
					setfplp(adr1);
				}
			}
			return;
		case 0x27:  /* movesize */
		case 0x28:  /* movelength */
			adr1 = getvar(VAR_READ);
			if (vx == 0x27 && (vartype & TYPE_NUMERIC)) {
				vgetsize(adr1, &i1, &i2);
				i3 = 0;
				if (i1 >= 10) work45[++i3] = (UCHAR)(i1 / 10 + '0');
				work45[++i3] = (UCHAR)(i1 % 10 + '0');
				work45[++i3] = '.';
				if (i2 >= 10) work45[++i3] = (UCHAR)(i2 / 10 + '0');
				work45[++i3] = (UCHAR)(i2 % 10 + '0');
				work45[0] = (UCHAR)(0x80 | i3);
				i3++;
				i1 += i2;
			}
			else {
				if (vartype & TYPE_NUMERIC) {
					if (vartype & (TYPE_NUM | TYPE_NUMLIT)) i1 = pl;  /* form, literal */
					else if (vartype & TYPE_INT) i1 = adr1[1];  /* int */
					else {  /* float */
						i1 = ((adr1[0] << 3) & 0x18) | (adr1[1] >> 5);
						if (adr1[1] & 0x1F) i1 += (adr1[1] & 0x1F) + 1;
					}
				}
				else if (vx == 0x27) {
					if (fp) i1 = lp - fp + 1;
					else i1 = 0;
				}
				else i1 = pl;
				x1 = i1;
				work45[0] = 0xFC;
				work45[1] = 0x05;
				memcpy(&work45[2], (UCHAR *) &x1, sizeof(INT32));
				i3 = 6;
			}
			adr3 = getvar(VAR_WRITE);
			if (action & ACTION_XPREFIX) memcpy(adr3, work45, i3);
			else {
				i1 = dbcflags & DBCFLAG_LESS;
				movevar(work45, adr3);
				dbcflags = (dbcflags & ~DBCFLAG_LESS) | i1;
			}
			return;
		case 0x29:  /* sin */
		case 0x2A:  /* cos */
		case 0x2B:  /* tan */
		case 0x2C:  /* arcsin */
		case 0x2D:  /* arccos */
		case 0x2E:  /* arctan */
		case 0x2F:  /* exp */
		case 0x30:  /* log */
		case 0x31:  /* log10 */
		case 0x32:  /* sqrt */
			adr1 = getint(work6a);
			adr2 = getvar(VAR_WRITE);
			y1 = nvtof(adr1);
			y2 = (DOUBLE64) 0;
			switch(vx) {
				case 0x29:  /* sin */
					y2 = (DOUBLE64) sin((double) y1);
					break;
				case 0x2A:  /* cos */
					y2 = (DOUBLE64) cos((double) y1);
					break;
				case 0x2B:  /* tan */
					y2 = (DOUBLE64) tan((double) y1);
					break;
				case 0x2C:  /* arcsin */
					if (y1 >= -1 && y1 <= 1) y2 = (DOUBLE64) asin((double) y1);
					break;
				case 0x2D:  /* arccos */
					if (y1 >= -1 && y1 <= 1) y2 = (DOUBLE64) acos((double) y1);
					break;
				case 0x2E:  /* arctan */
					y2 = (DOUBLE64) atan((double) y1);
					break;
				case 0x2F:  /* exp */
					y2 = (DOUBLE64) exp((double) y1);
					break;
				case 0x30:  /* log */
					if (y1 > 0) y2 = (DOUBLE64) log((double) y1);
					break;
				case 0x31:  /* log10 */
					if (y1 > 0) y2 = (DOUBLE64) log10((double) y1);
					break;
				case 0x32:  /* sqrt */
					if (y1 >= 0) y2 = (DOUBLE64) sqrt((double) y1);
					break;
			}
			if (action & ACTION_XPREFIX) {
				vgetsize(adr1, &i1, &i2);
				adr2[0] = (UCHAR)(0xF8 | (i1 >> 3));
				adr2[1] = (UCHAR)((i1 << 5) | i2);
				memcpy(&adr2[2], (UCHAR *) &y2, sizeof(DOUBLE64));
			}
			else {
				vgetsize(adr2, &i1, &i2);
				work45[0] = (UCHAR)(0xF8 | (i1 >> 3));
				work45[1] = (UCHAR)((i1 << 5) | i2);
				memcpy(&work45[2], (UCHAR *) &y2, sizeof(DOUBLE64));
				movevar(work45, adr2);
			}
			return;
		case 0x33:  /* abs */
			adr1 = getint(work6a);
			if (vartype & TYPE_NULL) {  /* null */
				assert((UINT)(hl + pl) <= sizeof(work45));
				memcpy(work45, adr1, hl + pl);
				setnullvar(work45, FALSE);
				adr1 = work45;
				i1 = hl + pl;
			}
			else if (vartype & TYPE_INT) {  /* int */
				memcpy((UCHAR *) &x1, &adr1[2], sizeof(INT32));
				if (x1 < 0) {
					x1 = -x1;
					work45[0] = adr1[0];
					work45[1] = adr1[1];
					memcpy(&work45[2], (UCHAR *) &x1, sizeof(INT32));
					adr1 = work45;
				}
				i1 = 6;
			}
			else if (vartype & TYPE_FLOAT) {  /* float */
				memcpy((UCHAR *) &y1, &adr1[2], sizeof(DOUBLE64));
				if (y1 < 0) {
					y1 = -y1;
					work45[0] = adr1[0];
					work45[1] = adr1[1];
					memcpy(&work45[2], (UCHAR *) &y1, sizeof(DOUBLE64));
					adr1 = work45;
				}
				i1 = 10;
			}
			else {  /* num */
				for (i1 = hl; i1 < hl + lp && adr1[i1] == ' '; i1++);
				if (i1 < hl + lp && adr1[i1] == '-') {
					memcpy(work45, adr1, hl + lp);
					work45[i1] = ' ';
					adr1 = work45;
				}
				i1 = hl + lp;
			}
			adr2 = getvar(VAR_WRITE);
			if (action & ACTION_XPREFIX) memcpy(adr2, adr1, i1);
			else movevar(adr1, adr2);
			return;
		case 0x34:  /* power */
			adr1 = getint(work6a);
			adr2 = getint(work6b);
			y1 = (DOUBLE64) pow((double) nvtof(adr2), (double) nvtof(adr1));
			adr3 = getvar(VAR_WRITE);
			if (action & ACTION_XPREFIX) {
				vgetsize(adr1, &i1, &i2);
				vgetsize(adr2, &i3, &i4);
				i1 += i3;
				i2 += i4;
				if (i2) {
					if (++i2 > 31) i2 = 31;
				}
				if (i1 > 31 - i2) i1 = 31 - i2;
				if (i2) i2--;
				else if (y1 != (DOUBLE64) 0.0) {
					/* force rounding so that 2 ** 6 is not 63, etc */
					workptr = (CHAR *) fcvt((double) y1, 0, &i3, &i4);
					y1 = (DOUBLE64) atof(workptr);
					if (i4) y1 = -y1;
				}
				adr3[0] = (UCHAR)(0xF8 | (i1 >> 3));
				adr3[1] = (UCHAR)((i1 << 5) | i2);
				memcpy(&adr3[2], (UCHAR *) &y1, sizeof(DOUBLE64));
			}
			else {
				vgetsize(adr3, &i1, &i2);
				work45[0] = (UCHAR)(0xF8 | (i1 >> 3));
				work45[1] = (UCHAR)((i1 << 5) | i2);
				memcpy(&work45[2], (UCHAR *) &y1, sizeof(DOUBLE64));
				movevar(work45, adr3);
			}
			return;
		case 0x35:  /* _add */
		case 0x36:  /* _sub */
		case 0x37:  /* _mult */
		case 0x38:  /* _div */
		case 0x39:  /* _mod */  /* v7 - _power, v8 - bad op */
			vmathexp(vx - 0x34);
			if (action & ACTION_FILEPI) fpicnt++;
			vbcode = lsvbcode;
			return;
		case 0x3A:  /* lcmove */
			adr1 = getvar(VAR_READ);
			if (lp) i1 = lp + hl - 1;
			else i1 = 0;
			adr2 = getvar(VAR_WRITE);
			if (i1) {
				adr2[hl] = adr1[i1];
				dbcflags &= ~DBCFLAG_EOS;
			}
			else dbcflags |= DBCFLAG_EOS;
			return;
		case 0x3B:  /* squeeze */
			adr1 = getvar(VAR_READ);
			i1 = fp + hl - 1;
			if (fp) i2 = lp + hl;
			else i2 = 0;
			adr2 = getvar(VAR_WRITE);
			if (action & ACTION_XPREFIX) pl = TEMPTYPE_CHAR_LENGTH;
			dbcflags &= ~DBCFLAG_EOS;
			lp = hl;
			for (i3 = hl + pl; i1 < i2 && lp < i3; i1++) if (adr1[i1] != ' ') adr2[lp++] = adr1[i1];
			while (i1 < i2) {
				if (adr1[i1++] != ' ') {
					if ((action & ACTION_XPREFIX) && !oldcxcompatflg) dbcerrinfo(566, (UCHAR*)"Squeeze", -1);
					dbcflags |= DBCFLAG_EOS;
					break;
				}
			}
			lp -= hl;
			if (lp) fp = 1;
			else fp = 0;
			setfplp(adr2);
			if (action & ACTION_XPREFIX) {
				// Set the PL to the LL.
				adr2[5] = adr2[3];
				adr2[6] = adr2[4];
			}
			return;
		case 0x3C:  /* chop */
			adr1 = getvar(VAR_READ);
			i1 = fp + hl - 1;
			if (fp) {
				i2 = lp + hl - 1;
				while (i2 >= i1 && adr1[i2] == ' ') i2--;
				i2 -= i1 - 1;
			}
			else i2 = 0;
			adr2 = getvar(VAR_WRITE);
			if (action & ACTION_XPREFIX) pl = TEMPTYPE_CHAR_LENGTH;
			lp = i2;
			if (lp > pl) {
				if ((action & ACTION_XPREFIX) && !oldcxcompatflg) dbcerrinfo(566, (UCHAR*)"Chop", -1);
				lp = pl;
				dbcflags |= DBCFLAG_EOS;
			}
			else dbcflags &= ~DBCFLAG_EOS;
			if (lp) {
				memmove(&adr2[hl], &adr1[i1], lp);
				fp = 1;
			}
			else fp = 0;
			setfplp(adr2);
			if (action & ACTION_XPREFIX) {
				// Set the PL to the LL.
				adr2[5] = adr2[3];
				adr2[6] = adr2[4];
			}
			return;
		case 0xF1:  /* trim */
			adr1 = getvar(VAR_READ);
			i1 = fp + hl - 1;
			if (fp) {
				i2 = lp + hl - 1;
				while (i1 <= i2 && adr1[i1] == ' ') i1++;
				while (i2 >= i1 && adr1[i2] == ' ') i2--;
				i2 -= i1 - 1;
			}
			else i2 = 0;
			adr2 = getvar(VAR_WRITE);
			if (action & ACTION_XPREFIX) pl = TEMPTYPE_CHAR_LENGTH;
			lp = i2;
			if (lp > pl) {
				if ((action & ACTION_XPREFIX) && !oldcxcompatflg) dbcerrinfo(566, (UCHAR*)"Trim", -1);
				lp = pl;
				dbcflags |= DBCFLAG_EOS;
			}
			else dbcflags &= ~DBCFLAG_EOS;
			if (lp) {
				memmove(&adr2[hl], &adr1[i1], lp);
				fp = 1;
			}
			else fp = 0;
			setfplp(adr2);
			if (action & ACTION_XPREFIX) {
				// Set the PL to the LL.
				adr2[5] = adr2[3];
				adr2[6] = adr2[4];
			}
			return;
		case 61 :  /* (0x3D) _resize */
			adr1 = getint(work6a);
			adr2 = getint(work6b);
			i1 = i2 = 0;
			if (!(vartype & TYPE_NULL)) {
				if (vartype & TYPE_INT) {  /* int */
					memcpy((UCHAR *) &x1, &adr2[2], sizeof(INT32));
					i1 = (INT) x1;
				}
				else if (vartype & TYPE_FLOAT) {  /* float */
					memcpy((UCHAR *) &y1, &adr2[2], sizeof(DOUBLE64));
					i1 = (INT) y1;
				}
				else {  /* numeric variable or literal */
					adr2 += hl;
					while (lp-- && *adr2 != '.') {
						if (isdigit(*adr2)) i1 = i1 * 10 + *adr2 - '0';
						adr2++;
					}
					if (lp > 0) {
						while (lp--) {
							adr2++;
							if (isdigit(*adr2)) i2 = i2 * 10 + *adr2 - '0';
						}
					}
				}
			}
			adr2 = getvar(VAR_WRITE); /* point adr2 at the destination, temporary, field */
			if (vartype & TYPE_NUMERIC) {
				i3 = i1;
				if (i2) i3 += i2 + 1;
				if (i1 < 0 || i2 < 0 || i3 > 31 || i3 < 1) {
					/* the requested resize value is invalid */
					/* Set i1, i2, i3 to match the size of adr1 */
					vgetsize(adr1, &i1, &i2);
					i3 = i1;
					if (i2) i3 += i2 + 1;
				}
				adr2[0] = (UCHAR)(0x80 | i3);
				memset(&adr2[1], ' ', i1);
				if (i2) {
					adr2[i1 + 1] = '.';
					memset(&adr2[i1 + 2], '0', i2);
				}
			}
			else /* compiler generated temp var */ {
				if (i1 < 0 || i1 > TEMPTYPE_CHAR_LENGTH) {
					if (oldcxcompatflg) return;
					dbcerrinfo(566, (UCHAR*)"_resize", -1);
				}
				memset(&adr2[hl], ' ', i1);
				fp = lp = 0;
				setfplp(adr2);
				// Set the PL to the LL.
				adr2[5] = (UCHAR) i1;
				adr2[6] = (UCHAR)(i1 >> 8);
			}
			i2 = dbcflags & DBCFLAG_FLAGS;
			movevar(adr1, adr2);
			dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i2;
			if (!(vartype & TYPE_NUMERIC)) {
				fp = 1;
				lp = i1;
				setfplp(adr2);
			}
			if (action & ACTION_FILEPI) fpicnt++;
			vbcode = lsvbcode;
			return;
		case VERB__CONCAT :  /* (0x3E) _concat */
			adr1 = getvar(VAR_READ);
			fp1 = fp;
			lp1 = lp;
			hl1 = hl;
			adr2 = getvar(VAR_READ);
			fp2 = fp;
			lp2 = lp;
			hl2 = hl;
			adr3 = getvar(VAR_WRITE);
			if (fp1) {
				i1 = lp1 - fp1 + 1;
				if (i1 > TEMPTYPE_CHAR_LENGTH) {
					if (!oldcxcompatflg) dbcerrinfo(566, (UCHAR*)"_concat", -1);
					i1 = TEMPTYPE_CHAR_LENGTH;
				}
				memcpy(&adr3[7], &adr1[hl1 + fp1 - 1], i1);
			}
			else i1 = 0;
			if (fp2) {
				i2 = lp2 - fp2 + 1;
				if (i2 > TEMPTYPE_CHAR_LENGTH - i1) {
					if (!oldcxcompatflg) dbcerrinfo(566, (UCHAR*)"_concat", -1);
					i2 = TEMPTYPE_CHAR_LENGTH - i1;
				}
				memcpy(&adr3[i1 + 7], &adr2[hl2 + fp2 - 1], i2);
			}
			else i2 = 0;
			i1 += i2;
			if (i1) fp = 1;
			else fp = 0;
			lp = i1;
			setfplp(adr3);
			adr3[5] = (UCHAR) i1;
			adr3[6] = (UCHAR)(i1 >> 8);
			if (action & ACTION_FILEPI) fpicnt++;
			vbcode = lsvbcode;
			return;
		case 63 :  /* (0x3F) _numnot */
			vnumop(vx);
			if (action & ACTION_FILEPI) fpicnt++;
			vbcode = lsvbcode;
			return;
		case 64 :  /* (0x40) _negate */
			adr1 = getint(work6a);
/*** CODE CHANGE: SAVE vartype, hl, ETC TO BUILD STRAIGHT INTO VARIABLE ***/
			if (vartype & TYPE_NULL) {
				assert((UINT)(hl + pl) <= sizeof(work45));
				memcpy(work45, adr1, hl + pl);
				setnullvar(work45, FALSE);
			}
			else if (vartype & TYPE_INT) {  /* int */
				memcpy((UCHAR *) &x1, &adr1[2], sizeof(INT32));
				x1 = -x1;
				memcpy(work45, adr1, 2);
				memcpy(&work45[2], (UCHAR *) &x1, sizeof(INT32));
				i1 = 6;
			}
			else if (vartype & TYPE_FLOAT) {  /* float */
				memcpy((UCHAR *) &y1, &adr1[2], sizeof(DOUBLE64));
				y1 = -y1;
				memcpy(work45, adr1, 2);
				memcpy(&work45[2], (UCHAR *) &y1, sizeof(DOUBLE64));
				i1 = 10;
			}
			else {  /* numeric variable or literal */
				memcpy(&work45[1], &adr1[hl], lp);
				for (i1 = 1, i2 = lp + 1; i1 < i2 && work45[i1] == ' '; i1++);
				if (work45[i1] == '-') work45[i1] = ' ';
				else if (i1 > 1) work45[i1 - 1] = '-';
				else if (lp < 31) {
					memmove(&work45[2], &work45[1], lp++);
					work45[1] = '-';
				}
				work45[0] = (UCHAR)(0x80 | lp);
				i1 = lp + 1;
			}
			memcpy(getvar(VAR_WRITE), work45, i1);
			if (action & ACTION_FILEPI) fpicnt++;
			vbcode = lsvbcode;
			return;

		case VERB_CLIENTGETFILE :  /* 65 (0x41) clientgetfile */
		case VERB_CLIENTPUTFILE :  /* 66 (0x42) clientputfile */
			if (fpicnt) filepi0();  /* cancel any filepi */
			adr1 = getvar(VAR_READ);
			fp1 = fp;
			adr2 = getvar(VAR_READ);
			fp2 = fp;
			if ((dbcflags & DBCFLAG_CLIENTINIT) && fp1 && fp2) {
				clientFileTransfer(vx, adr1, adr2);
			}
			return;

		case 0x47:  /* _moveequal1 */
		case 0x48:  /* _moveless1 */
		case 0x49:  /* _moveover1 */
		case 0x4A:  /* _moveeos1 */
		case 0x4B:  /* _moveequal0 */
		case 0x4C:  /* _moveless0 */
		case 0x4D:  /* _moveover0 */
		case 0x4E:  /* _moveeos0 */
		case 0x4F:  /* _movegreater1 */
		case 0x50:  /* _movegreater0 */
			vmoveexp(vx);
			if (action & ACTION_FILEPI) fpicnt++;
			vbcode = lsvbcode;
			return;
		case 0x51:  /* _nummove (v7) */
			adr1 = getint(work6a);
			adr2 = getvar(VAR_WRITE);
			i1 = dbcflags & DBCFLAG_FLAGS;
			movevar(adr1, adr2);
			if (!(dbcflags & DBCFLAG_PRE_V7)) dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1;
			if (action & ACTION_FILEPI) fpicnt++;
			vbcode = lsvbcode;
			return;
		case 0x52:  /* numand */
		case 0x53:  /* numor */
			vnumop(vx);
			if (action & ACTION_FILEPI) fpicnt++;
			vbcode = lsvbcode;
			return;
		case 0x54:  /* scrnsave */
			vscrnsave(0x03);
			return;
		case 0x55:  /* scrnrestore */
			vscrnrest(0x03);
			return;
		case 0x56:  /* _setifeq */
		case 0x57:  /* _setifneq */
		case 0x58:  /* _setiflt */
		case 0x59:  /* _setifgt */
		case 0x5A:  /* _setiflteq */
		case 0x5B:  /* _setifgteq */
			vsetop(vx);
			if (action & ACTION_FILEPI) fpicnt++;
			vbcode = lsvbcode;
			return;
		case 0x5C:  /* aimdex */
			chrptr = "aimdex";
			i5 = UTIL_AIMDEX;
			goto v58ver1;
		case 0x5D:  /* build */
			chrptr = "build";
			i5 = UTIL_BUILD;
			goto v58ver1;
		case 0x5E:  /* copy */
			chrptr = "copy";
			i5 = UTIL_COPY;
			goto v58ver1;
		case 0x5F:  /* create */
			chrptr = "create";
			i5 = UTIL_CREATE;
			goto v58ver1;
		case 0x60:  /* dbcmp */
			chrptr = "dbcmp";
			i5 = 0;
			goto v58ver1;
		case 0x61:  /* encode */
			chrptr = "encode";
			i5 = UTIL_ENCODE;
			goto v58ver1;
		case 0x62:  /* erase */
			chrptr = "erase";
			i5 = UTIL_ERASE;
			goto v58ver1;
		case 0x63:  /* filechk */
			chrptr = "filechk";
			i5 = 0;
			goto v58ver1;
		case 0x64:  /* index */
			chrptr = "index";
			i5 = UTIL_INDEX;
			goto v58ver1;
		case 0x65:  /* list */
			chrptr = "list";
			i5 = 0;
			goto v58ver1;
		case 0x66:  /* reformat */
			chrptr = "reformat";
			i5 = UTIL_REFORMAT;
			goto v58ver1;
		case 0x67:  /* rename */
			chrptr = "rename";
			i5 = UTIL_RENAME;
			goto v58ver1;
		case 0x68:  /* sort */
			chrptr = "sort";
			i5 = UTIL_SORT;
			goto v58ver1;
		case 0x69:  /* exist */
			chrptr = "exist";
			i5 = UTIL_EXIST;
v58ver1:
#if OS_WIN32
			__try { // @suppress("Statement has no effect")
#endif
			i3 = UTIL_FIXUP_CHOP;
			i4 = 0;
			strcpy((CHAR *) work45, chrptr);
			if (!prpget((CHAR *) work45, NULL, NULL, NULL, &workptr, 0) && *workptr) {
				strcpy(work, workptr);
				i5 = 0;
			}
			else if (i5) {
				i4 = srvgetprimary();
				if (i4) {
#if OS_WIN32
					if (i4 != -1 && !fsversion(i4, &i1, &i2)) {  /* check for FS 3.0 */
						if (i1 == 3 && i2 == 0) i3 = UTIL_FIXUP_CHOP | UTIL_FIXUP_OLDWIN;
					}
#endif
					i1 = 0;
					do work[i1] = (CHAR) toupper(chrptr[i1]);
					while (chrptr[i1++]);
				}
				else {
					if (prpget("utility", "internal", NULL, NULL, &workptr, PRP_LOWER) || strcmp(workptr, "off")) {  /* support internal */
#if OS_UNIX
						if (!prpget("utility", "shell", NULL, NULL, &workptr, PRP_LOWER) && !strcmp(workptr, "unix"))
							i3 = UTIL_FIXUP_CHOP | UTIL_FIXUP_UNIX;  /* take out any unix dependent escapes */
#endif
						work[0] = '\0';
					}
					else {
						i5 = 0;
						strcpy(work, chrptr);
					}
				}
			}
			else strcpy(work, chrptr);

			dbcflags &= ~DBCFLAG_EOS;
			i1 = (INT)strlen(work);
			if (i1) work[i1++] = ' ';
			for ( ; ; ) {
				if ((adr1 = getlist(LIST_READ | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1)) == NULL) break;
				if (!(vartype & TYPE_CHARACTER) || !fp--) continue;
				i2 = utilfixupargs(i3, (CHAR *) adr1 + hl + fp, lp - fp, work + i1, sizeof(work) - 4 - i1);
				if (i2 == -1) {  /* work is too small */
					dbcflags |= DBCFLAG_EOS;
					continue;
				}
				if (i2) {
					i1 += i2;
					work[i1++] = ' ';
				}
			}
			if (i1) i1--;
			utilerror[0] = '\0';
			if (i4) {
				if (i4 != -1) {
					work[i1] = '\0';
					i1 = fsexecute(i4, work);
					if (i1) fsgeterror(utilerror, sizeof(utilerror));
				}
				else {
					i1 = -1;
					srvgeterror(utilerror, sizeof(utilerror));
				}
			}
			else if (i5) {
				i1 = utility(i5, work, i1);
				/* reset memory pointers */
				setpgmdata();
				if (i1) {
					utilgeterrornum(i1, utilerror, sizeof(utilerror));
					i2 = (INT)strlen(utilerror);
					if (i2 + 2 < (INT) sizeof(utilerror)) {
						utilgeterror(utilerror + i2 + 2, sizeof(utilerror) - (i2 + 2));
						if (utilerror[i2 + 2]) {
							utilerror[i2] = ',';
							utilerror[i2 + 1] = ' ';
						}
					}
				}
			}
			else {
				strcpy(&work[i1], " --"); // Causes the command line utilities to run in 'silent' mode.
				i1 = utilexec(work);
			}
			if (i1) dbcflags |= DBCFLAG_OVER;
			else dbcflags &= ~DBCFLAG_OVER;
			return;
#if OS_WIN32
			}
			__except( EXCEPTION_EXECUTE_HANDLER ) {
				int i1 = GetExceptionCode();
				sprintf(work, "Windows exception code %d (%#.8x) during Utility processing", i1, (UINT) i1);
				dbcerrinfo(1699, work, -1);
			}
#endif
		case 0x6B:  /* erase with i/o errors */
			//int i1 = EXCEPTION_DATATYPE_MISALIGNMENT;
			verase();
			return;
		case 0x6C:  /* rename with i/o errors */
			vrename();
			return;
		case 0x6E:  /* getcolor */
		case 0x6F:  /* getpos */
			dbcctl(vx);
			return;
		case 0x78:  /* setflag equal */
			dbcflags |= DBCFLAG_EQUAL;
			return;
		case 0x79:  /* setflag less */
			dbcflags |= DBCFLAG_LESS;
			return;
		case 0x7A:  /* setflag over */
			dbcflags |= DBCFLAG_OVER;
			return;
		case 0x7B:  /* setflag eos */
			dbcflags |= DBCFLAG_EOS;
			return;
		case 0x7C:  /* setflag not equal */
			dbcflags &= ~DBCFLAG_EQUAL;
			return;
		case 0x7D:  /* setflag not less */
			dbcflags &= ~DBCFLAG_LESS;
			return;
		case 0x7E:  /* setflag not over */
			dbcflags &= ~DBCFLAG_OVER;
			return;
		case 0x7F:  /* setflag not eos */
			dbcflags &= ~DBCFLAG_EOS;
			return;
		case 0x80:  /* sqlexec */
		case 0x81:  /* sqlcode */
		case 0x82:  /* sqlmsg */
			vsql(vx);
			return;
		case 0x83:  /* winsize */
			if (!(dbcflags & DBCFLAG_SERVICE)) {
				if (!(dbcflags & DBCFLAG_KDSINIT)) {
					if (kdsinit() == RC_NO_MEM) {
						dbcdeath(64);
					}
				}
				else if (dbcflags & DBCFLAG_DEBUG) vdebug(DEBUG_RESTORE);
			}
			if (dbcflags & DBCFLAG_KDSINIT) {
				if (!(dbcflags & DBCFLAG_CLIENTINIT)) i1 = vidwinsize();
				else i1 = clientvidsize(CLIENT_VID_WIN);
			}
			else i1 = 0;
			work6a[0] = 0xFC; work6a[1] = 0x09;
			itonv(i1, work6a);
			movevar(work6a, getvar(VAR_WRITE));
			return;
		case 0x84:  /* scrnsize */
			if (!(dbcflags & DBCFLAG_SERVICE) && !(dbcflags & DBCFLAG_KDSINIT)) {
				if (kdsinit() == RC_NO_MEM) {
					dbcdeath(64);
				}
			}
			if (dbcflags & DBCFLAG_KDSINIT) {
				if (!(dbcflags & DBCFLAG_CLIENTINIT)) i1 = vidscreensize();
				else i1 = clientvidsize(CLIENT_VID_SCRN);
			}
			else i1 = 0;
			work6a[0] = 0xFC; work6a[1] = 0x09;
			itonv(i1, work6a);
			movevar(work6a, getvar(VAR_WRITE));
			return;
		case 0x85:  /* trapsize */
			work6a[0] = 0xFC; work6a[1] = 0x09;
			itonv(vtrapsize(), work6a);
			movevar(work6a, getvar(VAR_WRITE));
			return;
		case 0x86:  /* statesize */
			work6a[0] = 0xFC; work6a[1] = 0x09;
			if (!(dbcflags & DBCFLAG_SERVICE) && !(dbcflags & DBCFLAG_KDSINIT)) {
				if (kdsinit() == RC_NO_MEM) {
					dbcdeath(64);
				}
			}
			if (dbcflags & DBCFLAG_KDSINIT) {
				if (!(dbcflags & DBCFLAG_CLIENTINIT)) i1 = vidstatesize();
				else i1 = clientvidsize(CLIENT_VID_STATE);
			}
			else i1 = 0;
			itonv(i1, work6a);
			movevar(work6a, getvar(VAR_WRITE));
			return;
		case 0x87:  /* statesave */
			vscrnsave(0x02);
			return;
		case 0x88:  /* staterestore */
			vscrnrest(0x02);
			return;
		case 0x89:  /* charsave */
			vscrnsave(0x00);
			return;
		case 0x8A:  /* charrestore */
			vscrnrest(0x00);
			return;
		case 0x8B:  /* flag reverse */
			adr1 = getvar(VAR_READ);
			i1 = 0;
			if (!(vartype & TYPE_NULL)) {
				if (vartype & TYPE_NUM) {  /* form variable */
					for (i2 = adr1[0] & 0x1F, i3 = 1; i3 <= i2; i3++) {
						if (adr1[i3] == '-') {
							i1 = 1;
							break;
						}
					}
				}
				else if (vartype & TYPE_INT) {  /* integer variable */
					if (nvtoi(adr1) < 0) i1 = 1;
				}
				else {  /* float variable */
					if (nvtof(adr1) < (DOUBLE64) 0.0) i1 = 1;
				}
			}
			if (i1) {
				if (!(dbcflags & (DBCFLAG_LESS | DBCFLAG_EQUAL))) dbcflags |= DBCFLAG_LESS;
				else dbcflags &= ~DBCFLAG_LESS;
			}
			return;
		case 0x8C:  /* type */
			i4 = gettype(&i1, &i2, &i3);
			itonv(i4, getvar(VAR_WRITE));
			if (pgm[pcount] != 0xFF) {
				itonv(i1, getvar(VAR_WRITE));
				if (pgm[pcount] != 0xFF) {
					itonv(i2, getvar(VAR_WRITE));
					if (pgm[pcount] != 0xFF) itonv(i3, getvar(VAR_WRITE));
				}
			}
			pcount++;
			return;
		case 0x8D:  /* (141) getparm */
			dbcflags &= ~DBCFLAG_OVER;
			i2 = returnstackcount() - 1;
			i3 = returnstackpgm(i2);
			if (i2 < 0 || (ptr = getmpgm(i3)) == NULL) {
				dbcerror(555);
				longjmp(errjmp, TRUE);
			}
			i4 = getmver(i3);
			i7 = returnstackparmadr(i2);
			if (i7 == -1 || (i3 = ptr[i7] - 0xF8) > 6) {  /* end of list */
				while (pgm[pcount] != 0xFF) getvar(VAR_ADR);
				pcount++;
				dbcflags |= DBCFLAG_OVER;
				return;
			}
			if (i3 == 6) i3 = vx = 0;
			i7++;
			while (pgm[pcount] != 0xFF && i3-- >= 0) {
				adr1 = getvar(VAR_ADR);
				i1 = returnstackdata(i2);
				if (i4 >= 8) {
					if (ptr[i7] == 0xC1 && ptr[i7 + 1] == 0x80) {
						i7 += 2;
						i5 = 0x00800000 + (ptr[i7++] << 8);
						i5 += ptr[i7++];
					}
					else getvarx(ptr, &i7, &i1, &i5);
				}
				else {
					if (ptr[i7] >= 0xD0 && ptr[i7] <= 0xEF) {
						i5 = 0x00800000 + ((ptr[i7++] - 0xD0) << 8);
						i5 += ptr[i7++];
					}
					else getvarx(ptr, &i7, &i1, &i5);
				}
				if (!vx) {  /* =type */
					memset(&adr1[1], 0, 5);  /* set to global varible __gdim1 */
					adr1 = findvar(0, 0);  /* get redirected address */
					fp = lp = 0;
					setfplp(adr1);
					if (pgm[pcount] == 0xFF) break;
					adr1 = getvar(VAR_ADR);
				}
				adr1[1] = (UCHAR) i1;
				adr1[2] = (UCHAR)(i1 >> 8);
				adr1[3] = (UCHAR) i5;
				adr1[4] = (UCHAR)((UINT) i5 >> 8);
				adr1[5] = (UCHAR)(i5 >> 16);
				chkavar(adr1, TRUE);
				if (!vx) break;
			}
			while (i3-- >= 0) {
				i1 = returnstackdata(i2);
				if (i4 >= 8) {
					if (ptr[i7] == 0xC1 && ptr[i7 + 1] == 0x80) i7 += 4;
					else getvarx(ptr, &i7, &i1, &i5);
				}
				else {
					if (ptr[i7] >= 0xD0 && ptr[i7] <= 0xEF) i7 += 2;
					else getvarx(ptr, &i7, &i1, &i5);
				}
			}
			setreturnstackparmadr(i2, i7);
			while (pgm[pcount] != 0xFF) getvar(VAR_ADR);
			pcount++;
			return;
		case 0x8E:  /* loadparm */
			dbcflags &= ~DBCFLAG_OVER;
			i6 = nvtoi(getvar(VAR_READ));
			i2 = returnstackcount() - 1;
			i3 = returnstackpgm(i2);
			if (i2 < 0 || (ptr = getmpgm(i3)) == NULL) {
				dbcerror(555);
				longjmp(errjmp, TRUE);
			}
			i4 = getmver(i3);
			i7 = returnstackfirstparmadr(i2);
			if (i6 < 1 || i7 == -1) {  /* invalid index or null list */
				while (pgm[pcount] != 0xFF) getvar(VAR_ADR);
				pcount++;
				dbcflags |= DBCFLAG_OVER;
				return;
			}
			for ( ; ; ) {
				if ((i3 = ptr[i7] - 0xF8) > 6) {  /* end of list */
					while (pgm[pcount] != 0xFF) getvar(VAR_ADR);
					pcount++;
					dbcflags |= DBCFLAG_OVER;
					return;
				}
				vx = 1;
				if (i3 == 6) i3 = vx = 0;
				i7++;
				if (!--i6) break;
				while (i3-- >= 0) {
					i1 = returnstackdata(i2);
					if (i4 >= 8) {
						if (ptr[i7] == 0xC1 && ptr[i7 + 1] == 0x80) i7 += 4;
						else getvarx(ptr, &i7, &i1, &i5);
					}
					else {
						if (ptr[i7] >= 0xD0 && ptr[i7] <= 0xEF) i7 += 2;
						else getvarx(ptr, &i7, &i1, &i5);
					}
				}
			}
			while (pgm[pcount] != 0xFF && i3-- >= 0) {
				adr1 = getvar(VAR_ADR);
				i1 = returnstackdata(i2);
				if (i4 >= 8) {
					if (ptr[i7] == 0xC1 && ptr[i7 + 1] == 0x80) {
						i7 += 2;
						i5 = 0x00800000 + (ptr[i7++] << 8);
						i5 += ptr[i7++];
					}
					else getvarx(ptr, &i7, &i1, &i5);
				}
				else {
					if (ptr[i7] >= 0xD0 && ptr[i7] <= 0xEF) {
						i5 = 0x00800000 + ((ptr[i7++] - 0xD0) << 8);
						i5 += ptr[i7++];
					}
					else getvarx(ptr, &i7, &i1, &i5);
				}
				if (!vx) {  /* =type */
					memset(&adr1[1], 0, 5);  /* set to global varible __gdim1 */
					adr1 = findvar(0, 0);  /* get redirected address */
					fp = lp = 0;
					setfplp(adr1);
					if (pgm[pcount] == 0xFF) break;
					adr1 = getvar(VAR_ADR);
				}
				adr1[1] = (UCHAR) i1;
				adr1[2] = (UCHAR)(i1 >> 8);
				adr1[3] = (UCHAR) i5;
				adr1[4] = (UCHAR)((UINT) i5 >> 8);
				adr1[5] = (UCHAR)(i5 >> 16);
				chkavar(adr1, TRUE);
				if (!vx) break;
			}
			while (pgm[pcount] != 0xFF) getvar(VAR_ADR);
			pcount++;
			return;
		case 0x8F:  /* cverb */
			vcverb();
			return;
		case 0x90:  /* movelv */
			i1 = gethhll();  /* label table number */
			adr1 = getvar(VAR_ADR) + 1;
			*adr1++ = (UCHAR) datamodule;
			*adr1++ = (UCHAR)(datamodule >> 8);
			*adr1++ = (UCHAR) i1;
			*adr1++ = (UCHAR)(i1 >> 8);
			*adr1++ = 0x80;
			return;
		case 0x91:  /* movevl */
			adr1 = getvar(VAR_ADR);
			if (adr1[5] != 0x80) {  /* its not a label */
				dbcerror(552);
				longjmp(errjmp, TRUE);
			}
			i1 = llhh(&adr1[1]);  /* datamodule */
			i2 = llhh(&adr1[3]);  /* source label */
			i3 = gethhll();  /* dest label */
			if (getmver(pgmmodule) < 8) i3 -= 0xD000;
			movelabel(i1, i2, i3);
			return;
		case 0x92:  /* movelabel */
			i1 = gethhll();  /* src label table number */
			i2 = gethhll();  /* dest label table number */
			if (getmver(pgmmodule) < 8) {
				i1 -= 0xD000;
				i2 -= 0xD000;
			}
			movelabel(datamodule, i1, i2);
			return;
		case 0x93:  /* loadlabel */
		case 0x94:  /* storelabel */
			i3 = gethhll();  /* src or dest label table number */
			i1 = nvtoi(getvar(VAR_READ));
			for (i2 = 1; TRUE; ) {
				i4 = gethhll();
				if (i4 >= 0xFF00) break;
				if (i2++ == i1) {
					if (getmver(pgmmodule) < 8) {
						i3 -= 0xD000;
						i4 -= 0xD000;
					}
					if (vx == 0x93) movelabel(datamodule, i4, i3);
					else movelabel(datamodule, i3, i4);
				}
			}
			pcount--;
			return;
		case 0x95:  /* resetparm */
			i1 = returnstackcount() - 1;
			if (i1 >= 0) resetreturnstackparmadr(i1);
			return;
		case 0x96:  /* unpacklist <list-var> to <list of var@ and var[]@> */
			vunpacklist();
			return;
		case 0x97:  /* clearadr */
			while ((adr1 = getlist(LIST_ADR | LIST_PROG | LIST_ARRAY | LIST_NUM1)) != NULL)
				memset(&adr1[1], 0xFF, 5);
			return;
		case 0x98:  /* compareadr */
			getvar(VAR_ADRX);
			getlastvar(&i1, &i3);
			getvar(VAR_ADRX);
			getlastvar(&i2, &i4);
			if (i1 != 0xFFFF && i2 != 0xFFFF && i1 == i2 && i3 == i4) dbcflags |= DBCFLAG_EQUAL;
			else dbcflags &= ~DBCFLAG_EQUAL;
			return;
		case 0x99:  /* testadr */
			adr1 = getvar(VAR_ADR);
			chkavar(adr1, FALSE);
			if (adr1[1] == 0xFF && adr1[2] == 0xFF) dbcflags |= DBCFLAG_OVER;
			else dbcflags &= ~DBCFLAG_OVER;
			return;
		case 0xA0:  /* getcursor */
			if (!(dbcflags & DBCFLAG_SERVICE)) {
				if (!(dbcflags & DBCFLAG_KDSINIT)) {
					if (kdsinit() == RC_NO_MEM) {
						dbcdeath(64);
					}
				}
				else if (dbcflags & DBCFLAG_DEBUG) vdebug(DEBUG_RESTORE);
			}
			if (dbcflags & DBCFLAG_KDSINIT) {
				if (!(dbcflags & DBCFLAG_CLIENTINIT)) vidgetpos(&i1, &i2);
				else clientvidgetpos(&i1, &i2);
			}
			else i1 = i2 = 0;
			dbcflags &= ~DBCFLAG_OVER;
			work6a[0] = 0xFC; work6a[1] = 0x09;
			itonv(i1 + 1, work6a);
			movevar(work6a, getvar(VAR_WRITE));
			i1 = dbcflags & DBCFLAG_OVER;
			itonv(i2 + 1, work6a);
			movevar(work6a, getvar(VAR_WRITE));
			dbcflags |= i1;
			return;
		case 0xA1:  /* move src to dest-list */
			vmove(LIST_PROG);
			return;
		case 0xA2:  /* fchar */
		case 0xA3:  /* lchar */
			adr1 = getvar(VAR_READ);
			if (fp) {
				if (vx == 0xA2) i1 = hl + fp - 1;
				else i1 = hl + lp - 1;
			}
			else i1 = 0;
			adr2 = getvar(VAR_WRITE);
			if (i1) {
				adr2[hl] = adr1[i1];
				fp = lp = 1;
			}
			else fp = lp = 0;
			setfplp(adr2);
			if (action & ACTION_XPREFIX) {
				adr2[5] = adr2[3];
				adr2[6] = adr2[4];
			}
			if (action & ACTION_FILEPI) fpicnt++;
			vbcode = lsvbcode;
			return;
		case 0xA4:  /* edit giving */
			vedit(1);
			return;
		case 0xA5:  /* enable */
			enable();
			return;
		case 0xA6:  /* disable */
			if (!olddisableflg) {
				if (getdisable()) dbcflags |= DBCFLAG_OVER;
				else dbcflags &= ~DBCFLAG_OVER;
			}
			disable();
			return;
		case 0xA8:  /* show image on h,v */
		case 0xA9:  /* prep dev/res */
		case 0xAA:  /* open res/dev */
		case 0xAB:  /* close res/dev */
		case 0xAC:  /* show res/dev */
		case 0xAE:  /* load 174   load an image from a device*/
		case 0xAF:  /* store */
		case 0xB0:  /* change */
		case 0xB1:  /* query */
		case 0xB3:  /* get */
		case 0xB4:  /* put */
		case 0xB5:  /* wait (all) */
		case 0xB6:  /* wait(complex) */
		case 0xB7:  /* draw */
		case 0xB8:  /* link */
		case 0xBA:  /* empty (all) */
		case 0xBB:  /* empty (complex) */
			dbcctl(vx);
			return;
		case 0xC2:  /* readlk */
			vbcode = 0x34;
			goto v58ver2;
		case 0xC3:  /* readkslk */
			vbcode = 0x38;
			goto v58ver2;
		case 0xC4:  /* readkplk */
			vbcode = 0x36;
			goto v58ver2;
		case 0xC5:  /* readkglk */
			vbcode = 0x23;
			goto v58ver2;
		case 0xC6:  /* readgplk */
		case 0xC7:  /* readkgp readgptb */
			vbcode = 0x22;
v58ver2:
			if (vx < 0xC7) i1 = 0x01;  /* record lock */
			else i1 = 0x02;
			vread(i1);
			return;
		case 0xC8:  /* unlock */
		case 0xE8:  /* unlock record */
			vunlock(vx);
			return;
		case 0xC9:  /* makevar */
		case 0xCA:  /* makeglobal */
		case 0xCB:  /* getglobal */
			if (vx == 0xC9) i1 = MAKEVAR_VARIABLE | MAKEVAR_DATA;
			else if (vx == 0xCA) i1 = MAKEVAR_VARIABLE | MAKEVAR_NAME | MAKEVAR_GLOBAL | MAKEVAR_DATA;
			else i1 = MAKEVAR_NAME | MAKEVAR_GLOBAL | MAKEVAR_DATA;
			dbcflags &= ~(DBCFLAG_LESS | DBCFLAG_OVER);
			adr1 = getvar(VAR_READ);
			cvtoname(adr1);
			adr1 = getvar(VAR_ADR);
			i1 = makevar(name, adr1, i1);
			if (i1) {
				if (i1 == 1) dbcflags |= DBCFLAG_LESS;
				else if (i1 == 2) dbcflags |= DBCFLAG_OVER;
				else {
					switch (vx) {
					case 0xC9:
						dbcerrinfo(i1, (UCHAR*)"Fail in makevar", -1);
						break;
					case 0xCA:
						dbcerrinfo(i1, (UCHAR*)"Fail in makeglobal", -1);
						break;
					case 0xCB:
						dbcerrinfo(i1, (UCHAR*)"Fail in getglobal", -1);
						break;
					}
				}
			}
			return;
		case 0xCC:  /* getlabel */
			adr1 = getvar(VAR_READ);
			cvtoname(adr1);
			if (vgetlabel(gethhll())) dbcflags |= DBCFLAG_OVER;
			else dbcflags &= ~DBCFLAG_OVER;
			return;
		case 0xCD:  /* testlabel */
			if (vtestlabel(gethhll())) dbcflags |= DBCFLAG_OVER;
			else dbcflags &= ~DBCFLAG_OVER;
			return;
		case 0xCE:  /* ploadmod */
			adr1 = getvar(VAR_READ);
			if (fp) {
				cvtoname(adr1);
				i1 = newmod(MTYPE_PRELOAD, name, NULL, NULL, -1);
			}
			else i1 = 101;
			if (i1) dbcerror(i1);
			return;
		case 0xCF:  /* putfirst */
			dbcctl(vx);
			return;
		case 0xD0:  /* closeall */
			dioclosemod(0);  /* close files */
			prtclosemod(0);  /* close pfiles */
			ctlclosemod(0);  /* close all devices */
			comclosemod(0);  /* close comfiles */
			/* reset memory pointers */
			setpgmdata();
			return;
		case 0xD1:  /* getmodules */
		case 0xEE:  /* getreturnmodules */
			dbcflags &= ~(DBCFLAG_LESS | DBCFLAG_EOS);
			adr1 = getvar(VAR_WRITE);
			if (!(vartype & TYPE_ARRAY)) {
				dbcflags |= DBCFLAG_LESS | DBCFLAG_EOS;
				return;
			}
			adr1 += 3 + ((*adr1 - 0xA6) << 1);
			i2 = llhh(adr1);
			adr1 += 2;

			for (i1 = 0; ; ) {
				if (vx == 0xD1) {
					chrptr = getmodules(i1);
					if (chrptr == NULL) break;
					i1 = 1;
				}
				else {
					if (i1 == retptr) break;
					i3 = rstack[retptr - ++i1].retpgm;
					chrptr = getmname(i3, getdatamod(i3), FALSE);
				}
				scanvar(adr1);
				if (!(vartype & TYPE_CHAR)) {
					dbcflags |= DBCFLAG_LESS;
					return;
				}
				i3 = (INT)strlen(chrptr);
				if (i3 > pl) {
					i3 = pl;
					dbcflags |= DBCFLAG_EOS;
				}
				memcpy(&adr1[hl], chrptr, i3);
				fp = 1;
				lp = i3;
				setfplp(adr1);
				adr1 += i2;
			}

			do {  /* clear the rest of the array */
				scanvar(adr1);
				if (!(vartype & TYPE_CHAR)) break;
				fp = lp = 0;
				setfplp(adr1);
				adr1 += i2;
			} while (TRUE);
			return;
		case 0xD2:  /* format */
			adr1 = getvar(VAR_READ);
			adr2 = getvar(VAR_READ);
			adr3 = getvar(VAR_WRITE);
			vformat(adr1, adr2, adr3);
			return;
		case 0xD3:  /* setendkey */
		case 0xD4:  /* clearendkey */
		case 0xD5:  /* getendkey */
			dbckds(vx);
			return;
		case 0xD6:  /* getwindow */
			if (!(dbcflags & DBCFLAG_SERVICE)) {
				if (!(dbcflags & DBCFLAG_KDSINIT)) {
					if (kdsinit() == RC_NO_MEM) {
						dbcdeath(64);
					}
				}
				else if (dbcflags & DBCFLAG_DEBUG) vdebug(DEBUG_RESTORE);
			}
			if (dbcflags & DBCFLAG_KDSINIT) {
				if (!(dbcflags & DBCFLAG_CLIENTINIT)) vidgetwin(&i1, &i2, &i3, &i4);
				else clientvidgetwin(&i1, &i2, &i3, &i4);
			}
			else i1 = i2 = i3 = i4 = 0;
			dbcflags &= ~DBCFLAG_OVER;
			work6a[0] = 0xFC; work6a[1] = 0x09;  /* integer w/ display size 9 */
			x1 = i1 + 1;
			memcpy(&work6a[2], (UCHAR *) &x1, sizeof(INT32));
			movevar(work6a, getvar(VAR_WRITE));
			i5 = dbcflags & DBCFLAG_OVER;
			x1 = i2 + 1;
			memcpy(&work6a[2], (UCHAR *) &x1, sizeof(INT32));
			movevar(work6a, getvar(VAR_WRITE));
			i5 |= dbcflags & DBCFLAG_OVER;
			x1 = i3 + 1;
			memcpy(&work6a[2], (UCHAR *) &x1, sizeof(INT32));
			movevar(work6a, getvar(VAR_WRITE));
			i5 |= dbcflags & DBCFLAG_OVER;
			x1 = i4 + 1;
			memcpy(&work6a[2], (UCHAR *) &x1, sizeof(INT32));
			movevar(work6a, getvar(VAR_WRITE));
			i5 |= dbcflags & DBCFLAG_OVER;
			dbcflags |= i5;
			return;
		case 0xD7:  /* _cvtochar */
		case 0xD8:  /* _cvtoform */
		case 0xD9:  /* _cvtoint */
		case 0xDA:  /* _cvtofloat */
			vnummove(vx);
			if (action & ACTION_FILEPI) fpicnt++;
			vbcode = lsvbcode;
			return;
		case 0xDB:  /* hide */
		case 0xDC:  /* unlink */
			dbcctl(vx);
			return;

		case VERB_MAKE:  /* make */
			i1 = pcount;
			davb = getdavb();
			getlastvar(&i4, &i5);
			if (davb->refnum) {
				i2 = retptr;
				i1 = unloadclass(davb->refnum - 1, i1 - 2);
				if (i1) {
					unloadclass(davb->refnum - 1, -1);
					davb->refnum = 0;
					dbcerror(i1);
					return;
				}
				if (i2 < retptr) {  /* perform destroy methods */
					if (i2 == makeretptr) makemod = davb->refnum - 1;
					return;
				}
				davb->refnum = 0;
			}
			cvtoname(getvar(VAR_READ));
			strcpy(work, name);
			cvtoname(getvar(VAR_READ));

			calldmod = datamodule;
			callpmod = pgmmodule;
			callpcnt = pcount;
			i1 = FALSE;
			while ((c1 = pgm[pcount]) < 0xF8) {
				if (c1 == 0xC1 && pgm[pcount + 1] == 0x80) pcount += 4;
				else getvar(VAR_READ);
				i1 = TRUE;
			}
			if (pgm[pcount] != 0xFF) {  /* this should not happen */
				do {
					if ((i2 = getbyte() - 0xF7) == 7) i2 = 1;
					while (i2--) {
						if ((c1 = pgm[pcount]) == 0xFF) pcount += 2;  /* skip decimal contants */
						else {
							if (c1 == 0xC1 && pgm[pcount + 1] == 0x80) pcount += 4;
							else getvar(VAR_READ);
						}
					}
				} while (pgm[pcount] != 0xFF);
			}
			if (!i1) callpcnt = pcount;
			pcount++;

			i3 = retptr;
			i2 = i4;
			i1 = newmod(MTYPE_CLASS, name, work, &i2, -1);
			if (i1) dbcerror(i1);
			else {
				davb = aligndavb(findvar(i4, i5), NULL);
				davb->refnum = i2 + 1;
				if (i3 < retptr) {
					rstack[i3].objdata = i4;
					rstack[i3].objoff = i5;
					if (i3 == makeretptr) makemod = i2;
					vbcode = 0xCF;
				}
			}
			return;
		case 0xE3:  /* destroy */
			i1 = pcount;
			davb = getdavb();
			if (davb->refnum) {
				getlastvar(&i3, &i4);
				i2 = retptr;
				i1 = unloadclass(davb->refnum - 1, i1 - 2);
				if (i1) unloadclass(davb->refnum - 1, -1);
				else if (i2 < retptr) {  /* perform destroy methods */
					if (i2 == makeretptr) makemod = davb->refnum - 1;
					return;
				}
				for (i2 = 0; i2 < retptr; i2++) {
					if (rstack[i2].objdata == i3 && rstack[i2].objoff == i4) {
						while (retptr - 1 > i2) popreturnstack();
						vreturn();
						break;
					}
				}
				davb->refnum = 0;
				if (i1) dbcerror(i1);
			}
			return;
		case 0xE4:  /* clearlabel */
			clearlabel(gethhll());  /* clear label table number */
			return;
		case 0xE5:  /* pushreturn */
			if (getpcnt(gethhll(), &i1, &i2)) {
				dbcerror(551);
				return;
			}
			i3 = pgmmodule;
			pgmmodule = i1;
			i4 = pcount;
			pcount = i2;
			i1 = pushreturnstack(-1);
			pgmmodule = i3;
			pcount = i4;
			if (i1 == -1) {
				dbcerror(501);
				return;
			}
			return;
		case 0xE6:  /* popreturn */
			i1 = gethhll();  /* label table number */
			if (retptr > makeretptr && rstack[retptr - 1].makepgm != -1) return;  /* make or destroy active */
			i3 = pgmmodule;
			i4 = pcount;
			if (!popreturnstack()) {
				putpcnt(i1, pgmmodule, pcount);
				dbcflags &= ~DBCFLAG_OVER;
			}
			else {
				clearlabel(i1);
				dbcflags |= DBCFLAG_OVER;
			}
			pgmmodule = i3;
			pcount = i4;
			return;
		case 0xE7:  /* getname */
/*** CODE: VERIFY THIS CODE WORKS WITH COMPILER ***/
			adr1 = getvar(VAR_READ);
/*** CODE: PREOBLEM IF DAVB ADDRESSES ARE ROUNDED UP, THEN WILL NEED CODE ***/
/***       TO WORK BACKWARDS WHICH REQUIRES TEST OF VARIABLE IF IT IS A DAVB ***/
/***       MAYBE HAVE NEW VARTYPE TO DESCRIBE IT'S A DAVB ***/
			getlastvar(&i1, &i2);
			adr2 = getvar(VAR_WRITE);
/*** CODE: PROBLEM IF A CHAR VAR IS BEFORE ADR1 AND CHAR VAR CONTAINS 0xFE ***/
			if (i2-- && *(--adr1) == 0xFE) {
				for (i1 = i2; i2-- && *(--adr1) != 0xFE; );
				i1 -= ++i2;
				if (i2 && i1 && i1 < 32) {
					work45[0] = 0xE1;
					work45[1] = (UCHAR) i1;
					memcpy(&work45[2], adr1 + 1, i1);
					movevar(work45, adr2);
					dbcflags &= ~DBCFLAG_OVER;
					return;
				}
			}
			fp = lp = 0;
			setfplp(adr2);
			dbcflags |= DBCFLAG_OVER;
			return;
		case 0xE9:  /* setnull */
			adr1 = getvar(VAR_WRITE);
			if (vartype & (TYPE_LIST | TYPE_ARRAY)) {
				adr1 = getlist(LIST_WRITE | LIST_LIST | LIST_ARRAY | LIST_NUM1);
				if (adr1 == NULL) return;
				i1 = TRUE;
			}
			else i1 = FALSE;
			do setnullvar(adr1, TRUE);
			while (i1 && ((adr1 = getlist(LIST_WRITE | LIST_LIST | LIST_ARRAY | LIST_NUM1)) != NULL));
			return;
		case 0xEA:  /* _isnull */
			getvar(VAR_READ);
			if (vartype & TYPE_NULL) x1 = 1;
			else x1 = 0;
			adr1 = getvar(VAR_WRITE);
			adr1[0] = 0xFC;
			adr1[1] = 0x01;
			memcpy(&adr1[2], (CHAR *) &x1, sizeof(INT32));
			if (action & ACTION_FILEPI) fpicnt++;
			vbcode = lsvbcode;
			return;
		case 0xEB:  /* getobject */
			adr1 = getvar(VAR_ADR);
			for (i1 = retptr; i1--; ) if (rstack[i1].objdata != -1) break;
			if (i1 >= 0) {
				adr1[1] = (UCHAR) rstack[i1].objdata;
				adr1[2] = (UCHAR)(rstack[i1].objdata >> 8);
				adr1[3] = (UCHAR) rstack[i1].objoff;
				adr1[4] = (UCHAR)(rstack[i1].objoff >> 8);
				adr1[5] = (UCHAR)(rstack[i1].objoff >> 16);
				chkavar(adr1, TRUE);
			}
			else memset(&adr1[1], 0xFF, 5);
			return;
		case VERB_PACKLEN:  /* packlen */
			vpack(TRUE);
			return;
		case 0xED:  /* getprinters 237 */
			vgetprintinfo(GETPRINTINFO_PRINTERS);
			return;
		case 0xEF:  /* getpaperbins 239 */
			vgetprintinfo(GETPRINTINFO_PAPERBINS);
			return;
		case 0xF0:  /* clientrollout */
			if (fpicnt) filepi0();  /* cancel any filepi */
			// Turn ON the Over flag, assume failure
			dbcflags |= DBCFLAG_OVER;
			adr1 = getvar(VAR_READ);
			if ((dbcflags & DBCFLAG_CLIENTINIT) && fp) {
				setClientrollout(TRUE); // extend 'keepalive' time
				cvtoname(adr1);
				clientput("<rollout>", 9);
				clientputdata(name, -1);
				clientput("</rollout>", 10);
				if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) i1 = -1;
				else {
					e1 = (ELEMENT *) clientelement(); /* assume <r> element */
					if (e1 == NULL) i1 = -1;
					else if (e1->firstattribute != NULL) i1 = (INT)strtol(e1->firstattribute->value, NULL, 0);
					else i1 = 0; // Success
				}
				clientrelease();
				if (!i1) {
					// Turn OFF the Over flag, SUCCESS
					dbcflags &= ~DBCFLAG_OVER;
				}
				setClientrollout(FALSE);
			}
			return;
		case 0xF2:  /* rotate */
			if (pgm[pcount] == 0xFF) {
				i1 = pgm[pcount + 1];
				pcount += 2;
			}
			else if (pgm[pcount] == 0xFE) {
				i1 = (0 - pgm[pcount + 1]);
				pcount += 2;
			}
			else {
				adr1 = getvar(VAR_READ);
				i1 = nvtoi(adr1);
			}
			adr2 = getvar(VAR_WRITE);
			i2 = nvtoi(adr2);
			if (i1 < 0) i1 = (i1 | 0xFFFFFFE0) + 32;
			else i1 = i1 & 0x1F;
			if (i1 == 0) return;
			i3 = i2 << i1;
			i4 = ((UINT)i2) >> (32 - i1);
			i3 |= (i4 & (~(((unsigned int)-1) << i1)));
			work45[0] = 0xFC;
			work45[1] = 0x0C;
			memcpy(&work45[2], (UCHAR *) &i3, sizeof(INT32));
			movevar(work45, adr2);
			return;
	}
	sprintf((CHAR*)work45, "Bad type '88' opcode seen (%u)", (UINT)vx);
	dbcerrinfo(505, work45, -1);
}

INT vstop(UCHAR *errvar, INT errnum)
{
	INT i1, i2;
	CHAR work[MAX_NAMESIZE], *ptr1, *ptr2;

	if (fpicnt) filepi0();
	if (retptr > makeretptr) {  /* error during make or destroy */
		retptr = makeretptr + 1;
		vreturn();
		unloadclass(makemod, -1);
	}

	/*
	 * answer-master or stop program feature
	 *
	 * 'prog' is inserted into the xml by us. See buildxmlentry in dbccfg.c
	 */
	if (prpget("stop", "prog", NULL, NULL, &ptr1, 0) || !*ptr1) {
		if (compat == 4) ptr1 = "MASTER";
		else ptr1 = NULL;
	}
	adr1 = (UCHAR *) getpname(pgmchn);
	if (ptr1 != NULL && adr1 != NULL) {
		if (prpget("start", "prog", NULL, NULL, &ptr2, 0) || !*ptr2) {
			if (compat == 4) ptr2 = "ANSWER";
			else ptr2 = NULL;
		}
		strcpy(work, ptr1);
		miofixname(work, ".dbc", FIXNAME_EXT_ADD | FIXNAME_PAR_DBCVOL);
		for (i1 = (INT)strlen(work) - 1; work[i1] != '.' && work[i1] != ')'; i1--);
		for (work[i1--] = '\0';
				i1 >= 0 && work[i1] != '\\' && work[i1] != ':' && work[i1] != '/' && work[i1] != ']' && work[i1] != '(';
				i1--)
		{
#if OS_WIN32
			work[i1] = (CHAR) toupper(work[i1]);
#endif
		}
		if (!strcmp(&work[i1 + 1], (CHAR *) adr1)) ptr1 = ptr2;  /* currently in stop program */
		else if (ptr2 != NULL) {  /* check if in start program */
			strcpy(work, ptr2);
			miofixname(work, ".dbc", FIXNAME_EXT_ADD | FIXNAME_PAR_DBCVOL);
			for (i1 = (INT)strlen(work) - 1; work[i1] != '.' && work[i1] != ')'; i1--);
			for (work[i1--] = '\0';
					i1 >= 0 && work[i1] != '\\' && work[i1] != ':' && work[i1] != '/' && work[i1] != ']' && work[i1] != '('; i1--) {
#if OS_WIN32
				work[i1] = (CHAR) toupper(work[i1]);
#endif
			}
			if (!strcmp(&work[i1 + 1], (CHAR *) adr1)) ptr1 = NULL;  /* currently in start program */
		}
		if (ptr1 != NULL) {
			strcpy(name, ptr1);
			if (errvar != NULL) {  /* error variable */
				adr1 = getmdata(datachn);
				if (adr1 != NULL) movevar(errvar, adr1);
			}
			i1 = getpgmsize(pgmmodule);
			if (i1 >= 0) i1--;
			i2 = retptr;
			i1 = newmod(MTYPE_CHAIN, name, NULL, NULL, i1);
			if (i1) badanswer(i1);
			if (i2 < retptr) {
				return(1);
			}
			kdschain();
			trapclearall();
			enable();
			/*vsql(0);*/
 /* reset destroyflg to 0 through dbcerror() if stop came from dbciex */
			dbcerror(0);
			errorexec();
			return(0);
		}
	}

	/* call any loaded classes destructors */
	i1 = getpgmsize(pgmmodule);
	if (i1 >= 0) i1--;
	i2 = retptr;
	newmod(0, NULL, NULL, NULL, i1);
	if (i2 < retptr) {
		return(1);
	}

	/* stopping DBC */
	if (dbcflags & DBCFLAG_DEBUG) vdebug(DEBUG_RESTORE);  /* to restore user screen */
	dbcexit((errnum / 100) % 10);  /* it is a STOP that exits DBC */
	return(0); // Never happens. Keep compiler happy.
}

void dbcerrjmp()
{
	action |= ACTION_ERROR;
	longjmp(errjmp, 0);
}

void getaction(INT **actionvar, INT *actionbit)
{
	*actionvar = &action;
	*actionbit = ACTION_EVENT;
}

void dbgaction(INT flag)
{
	if (flag) action |= ACTION_DEBUG;
	else action &= ~ACTION_DEBUG;
}

void dbcshutdown()
{
	action |= ACTION_SHUTDOWN;
}

/* pop stack and set pgmmodule and pcount */
INT popreturnstack()
{
	if (!retptr) return(RC_ERROR);
	if (--retptr == fpiretptr) {
		fpiretptr = callmax;
		if (fpicnt) action |= ACTION_FILEPI;
	}
	pgmmodule = rstack[retptr].retpgm;
	pcount = rstack[retptr].retpcnt;
	if (retptr >= makeretptr) {
		if (retptr == makeretptr) makeretptr = callmax;  /* done with make */
		else if (rstack[retptr].makepgm >= 0) {  /* returning to make routine */
			callpmod = rstack[retptr].makepgm;
			calldmod = rstack[retptr].makedata;
			callpcnt = rstack[retptr].makepcnt;
			vbcode = 0xCF;  /* set last code to be call with parms */
		}
	}
	return(0);
}

/*
 * push the pcount, module and call with parm address into the return stack
 * return RC_ERROR if stack is full, else return 0
 */
INT pushreturnstack(INT parmadr)
{
	if (retptr == callmax) return(RC_ERROR);
	rstack[retptr].retpgm = pgmmodule;
	rstack[retptr].retpcnt = pcount;
	if (parmadr >= -1) {
		rstack[retptr].parmdata = datamodule;
		rstack[retptr].makepgm = -1;
	}
	else {
		if (parmadr == -2) {
			rstack[retptr].makepgm = callpmod;
			rstack[retptr].makedata = calldmod;
			rstack[retptr].makepcnt = callpcnt;
		}
		else rstack[retptr].makepgm = -2;
		if (retptr < makeretptr) makeretptr = retptr;
		parmadr = -1;
	}
	rstack[retptr].parmnext = rstack[retptr].parmfirst = parmadr;
	rstack[retptr].objdata = -1;
	retptr++;
	return(0);
}

/* return stack index */
INT returnstackcount()
{
	return(retptr);
}

/* return program module */
INT returnstackpgm(INT index)
{
	return(rstack[index].retpgm);
}

/* return data module */
INT returnstackdata(INT index)
{
	return(rstack[index].parmdata);
}

/* return pcount */
INT returnstackpcount(INT index)
{
	return(rstack[index].retpcnt);
}

/* return call parm first address */
INT returnstackfirstparmadr(INT index)
{
	return(rstack[index].parmfirst);
}

/* return call parm address */
INT returnstackparmadr(INT index)
{
	return(rstack[index].parmnext);
}

/* set call parm address */
void setreturnstackparmadr(INT index, INT parmadr)
{
	rstack[index].parmnext = parmadr;
}

/* reset call parm address */
void resetreturnstackparmadr(INT index)
{
	rstack[index].parmnext = rstack[index].parmfirst;
}
