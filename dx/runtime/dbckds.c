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
#define INC_LIMITS
#include "includes.h"
#include "base.h"
#include "dbc.h"
#include "dbcclnt.h"
#include "evt.h"
#include "fio.h"
#include "gui.h"
#include "tim.h"
#include "vid.h"

/* right most 9 bits are saved between vkyds() */
#define KYDS_COMPAT		0x00000001
#define KYDS_UPPER		0x00000002
#define KYDS_EDITON		0x00000004
#define KYDS_FSHIFT		0x00000008
#define KYDS_PIPE		0x00000010
#define KYDS_OLDHOFF	0x00000020
#define KYDS_RESETSW	0x00000040
#define KYDS_FPITEST	0x00000080
#define KYDS_DCON		0x00000100
/* below cleared on entry to vkyds */
#define KYDS_CLIENTKEY	0x00000200
#define KYDS_CLIENTGET	0x00000400
#define KYDS_CLIENTWAIT	0x00000800
#define KYDS_FUNCKEY	0x00001000
#define KYDS_TIMEOUT	0x00002000
#define KYDS_TRAP		0x00004000
#define KYDS_PRIOR		0x00008000
#define KYDS_BACKGND	0x00010000
#define KYDS_DOWN		0x00020000
#define KYDS_EDIT		0x00040000
#define KYDS_FORMAT		0x00080000
#define KYDS_LITERAL	0x00100000
#define KYDS_KCON		0x00200000
#define KYDS_DV			0x00400000
#define KYDS_KL			0x00800000
#define KYDS_LL			0x01000000
#define KYDS_RV			0x02000000
#define KYDS_SL			0x04000000
#define KYDS_ZF			0x08000000
#define KYDS_ZS			0x10000000
#define KYDS_DE			0x20000000
#define KYDS_JR			0x40000000
#define KYDS_JL			0x80000000

#define KYDS_SAVEMASK	0x000001FF
#define MAXCMD 100

/* local variables */
static UINT deffgcolor, defbgcolor;
static INT endkey, fncendkey;
static INT keyineventid;
static UINT kydsflags;
static INT keyinerror = 0;

static INT getvalue(void);
static INT getcolor(CHAR *, UINT *);
static INT getnextkey(INT *);


INT kdsinit()  /* initialize keyin-display routines */
{
 	INT i1, cmdcnt, intrchar, savecnt;
	INT32 vidcmd[20];
	CHAR *ptr;
	UCHAR kbdfmap[(VID_MAXKEYVAL >> 3) + 1];
	UCHAR kbdvmap[(VID_MAXKEYVAL >> 3) + 1];

	if (dbcflags & DBCFLAG_KDSINIT) return(0);

	kydsflags = 0;
	if ((keyineventid = evtcreate()) == -1) return(RC_ERROR);
	if (!prpget("keyin", "error", NULL, NULL, &ptr, 0)) keyinerror = atoi(ptr);
	if (!prpget("display", "hoff", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) kydsflags |= KYDS_OLDHOFF;
	if (!prpget("display", "resetsw", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) kydsflags |= KYDS_RESETSW;
	if (!prpget("file", "filepi", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "test")) kydsflags |= KYDS_FPITEST;
	if (!prpget("kydspipe", NULL, NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) kydsflags |= KYDS_PIPE;

	cmdcnt = 0;
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		if (clientvidinit() < 0) dbcerror(798);
		if (!prpget("keyin", "fkeyshift", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old"))
			kydsflags |= KYDS_FSHIFT;
	}
	else {
		if ( (i1 = vidinit(NULL, dbcgetbrkevt())) ) return i1;
		dbcflags |= DBCFLAG_VIDINIT;

		/* set up valid keys */
		memset(kbdvmap, 0x00, (VID_MAXKEYVAL >> 3) + 1);
		for (i1 = 32; i1 < 127; i1++) kbdvmap[i1 >> 3] |= 1 << (i1 & 0x07);
		if (!prpget("keyin", "ikeys", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) {
			kbdvmap[21 >> 3] |= 1 << (21 & 0x07);
			for (i1 = 128; i1 < 256; i1++) kbdvmap[i1 >> 3] |= 1 << (i1 & 0x07);
		}
		vidkeyinvalidmap(kbdvmap);

		/* set up ending keys */
		memset(kbdfmap, 0x00, (VID_MAXKEYVAL >> 3) + 1);
		kbdfmap[VID_ENTER >> 3] |= 1 << (VID_ENTER & 0x07);
		for (i1 = VID_F1; i1 <= VID_F20; i1++) kbdfmap[i1 >> 3] |= 1 << (i1 & 0x07);
#if OS_WIN32
		if (!prpget("keyin", "fkeyshift", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) {
			kydsflags |= KYDS_FSHIFT;
			for (i1 = VID_SHIFTF1; i1 <= VID_SHIFTF10; i1++) kbdfmap[i1 >> 3] |= 1 << (i1 & 0x07);
		}
#endif
		for (i1 = VID_UP; i1 <= VID_RIGHT; i1++) kbdfmap[i1 >> 3] |= 1 << (i1 & 0x07);
		if (!prpget("keyin", "endkey", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "xkeys")) {
			for (i1 = VID_INSERT; i1 <= VID_PGDN; i1++) kbdfmap[i1 >> 3] |= 1 << (i1 & 0x07);
			kbdfmap[VID_ESCAPE >> 3] |= 1 << (VID_ESCAPE & 0x07);
			kbdfmap[VID_TAB >> 3] |= 1 << (VID_TAB & 0x07);
			kbdfmap[VID_BKTAB >> 3] |= 1 << (VID_BKTAB & 0x07);
		}
		vidkeyinfinishmap(kbdfmap);

		/* set interrupt key */
		intrchar = 26;  /* default is ^Z */
		if (!prpget("keyin", "interrupt", NULL, NULL, &ptr, PRP_LOWER)) {
			if (!strcmp(ptr, "esc")) intrchar = VID_ESCAPE;
			else if (ptr[0] == '^' && (i1 = toupper(ptr[1])) >= 'A' && i1 <= '_') {
				intrchar = i1 - 'A' + 1;
				if (intrchar == 0x1B) intrchar = VID_ESCAPE;
				else if (intrchar == 0x09) intrchar = VID_TAB;
			}
			else if ((ptr[0] == 'f') &&
				((i1 = atoi(&ptr[1])) >= 1 && i1 <= 20)) intrchar = VID_F1 + i1 - 1;
			else if (!strcmp(ptr, "none")) intrchar = 0xFFFF;
		}
		vidcmd[cmdcnt++] = VID_KBD_F_INTR | intrchar;

		/* set cancel key */
		savecnt = cmdcnt;
		if (!prpget("keyin", "cancelkey", NULL, NULL, &ptr, PRP_LOWER)) {
			if (!strcmp(ptr, "tab")) {
				vidcmd[cmdcnt++] = VID_KBD_F_CNCL1 | VID_TAB;
				vidcmd[cmdcnt++] = VID_KBD_F_CNCL2 | VID_BKTAB;
			}
			else if (!strcmp(ptr, "esc")) vidcmd[cmdcnt++] = VID_KBD_F_CNCL1 | VID_ESCAPE;
			else if (ptr[0] == '^') {
				i1 = toupper(ptr[1]);
				if (i1 >= 'A' && i1 <= '_') {
					i1 -= 'A' - 1;
					if (i1 == 0x1B) vidcmd[cmdcnt++] = VID_KBD_F_CNCL1 | VID_ESCAPE;
					else if (i1 == 0x09) vidcmd[cmdcnt++] = VID_KBD_F_CNCL1 | VID_TAB;
					else vidcmd[cmdcnt++] = VID_KBD_F_CNCL1 | i1;
				}
			}
			else if (ptr[0] == 'f') {
				i1 = atoi(&ptr[1]);
				if (i1 >= 1 && i1 <= 20) vidcmd[cmdcnt++] = VID_KBD_F_CNCL1 | (VID_F1 + i1 - 1);
			}
		}
		/* default = none */
		if (cmdcnt == savecnt) vidcmd[cmdcnt++] = VID_KBD_F_CNCL1 | 0xFFFF;
		if (cmdcnt == savecnt + 1) vidcmd[cmdcnt++] = VID_KBD_F_CNCL2 | 0xFFFF;
	}

	/* set up keycase */
	
	if (!(dbcflags & DBCFLAG_CLIENTINIT)) {
		if (!prpget("keyin", "case", NULL, NULL, &ptr, PRP_LOWER)) {
			if (!strcmp("reverse", ptr)) {
				kydsflags |= KYDS_COMPAT;
				vidcmd[cmdcnt++] = VID_KBD_IC;
			}
			else if (!strcmp("upper", ptr)) {
				kydsflags |= KYDS_UPPER;
				vidcmd[cmdcnt++] = VID_KBD_UC;
			}
		}
	}

	/* set up default colors */
	deffgcolor = VID_FOREGROUND | VID_WHITE;
	defbgcolor = VID_BACKGROUND | VID_BLACK;
	if (!prpget("display", "color", NULL, NULL, &ptr, PRP_LOWER)) {
		if (isdigit(ptr[0])) deffgcolor = VID_FOREGROUND | (USHORT)(strtol(ptr, NULL, 10) & 0xFFl);
		else if (!getcolor(ptr, &deffgcolor)) deffgcolor |= VID_FOREGROUND;
	}
	if (!prpget("display", "bgcolor", NULL, NULL, &ptr, PRP_LOWER)) {
		if (isdigit(ptr[0])) defbgcolor = VID_BACKGROUND | (USHORT)(strtol(ptr, NULL, 10) & 0xFFl);
		else if (!getcolor(ptr, &defbgcolor)) defbgcolor |= VID_BACKGROUND;
	}
	vidcmd[cmdcnt++] = deffgcolor;
	vidcmd[cmdcnt++] = defbgcolor;

	if (!prpget("display", "autoroll", NULL, NULL, &ptr, PRP_LOWER) && !strncmp(ptr, "off", 3))
		vidcmd[cmdcnt++] = VID_NL_ROLL_OFF;

	if ((dbcflags & DBCFLAG_CLIENTINIT) || !(kydsflags & KYDS_PIPE)) {
/*** CODE: NOT SURE IF IT'S A GOOD IDEA TO NOT DO THE *ES ***/
		vidcmd[cmdcnt++] = VID_HORZ | 0;
		vidcmd[cmdcnt++] = VID_VERT | 0;
	}
	vidcmd[cmdcnt] = VID_END_FLUSH;
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		clientput("<k>", 3);
		clientvidput(vidcmd, CLIENTVIDPUT_SAVE | CLIENTVIDPUT_SEND);
		clientput("</k>", 4);
		if (clientsend(0, 0) < 0) dbcerror(798);
	}
	else vidput(vidcmd);

	dbcflags |= DBCFLAG_KDSINIT;
	return(0);
}

void kdsexit()
{
	if (dbcflags & DBCFLAG_VIDINIT) videxit();
}

void kdschain()  /* when program chaining, certain flags are reset, etc. */
{
	INT32 vidcmd[4];

	if (dbcflags & DBCFLAG_SERVICE) return;
	if (dbcflags & DBCFLAG_DEBUG) vdebug(DEBUG_RESTORE);
	if (!(dbcflags & DBCFLAG_KDSINIT)) return;

	/* reset keycase & subwindow */
	if (kydsflags & KYDS_COMPAT) vidcmd[0] = VID_KBD_IC;
	else if (kydsflags & KYDS_UPPER) vidcmd[0] = VID_KBD_UC;
	else vidcmd[0] = VID_KBD_IN;
	vidcmd[1] = VID_WIN_RESET;
	vidcmd[2] = VID_FINISH;
	if (dbcflags & DBCFLAG_VIDINIT) {
		vidput(vidcmd);
		vidreset();
	}
	else if (dbcflags & DBCFLAG_CLIENTINIT) {
		clientput("<k>", 3);
		clientvidput(vidcmd, CLIENTVIDPUT_SAVE | CLIENTVIDPUT_SEND);
		clientvidreset();
		clientput("</k><s/>", 8);
		if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL | CLIENTSEND_RELEASE, 0) < 0) dbcerror(798);
	}
	kydsflags &= ~KYDS_DCON;
}

void vkyds()
{
	INT i1, i2, i3, i4, i5, cmdcnt, fieldlen, kllen, listflg;
	INT origlen, psave, rc, savekydsflags, savepsave, vidputtype = 0;
	INT32 vidcmd[MAXCMD];
	UCHAR c1, *adr, *fmtadr, *ptr, savestate[sizeof(STATESAVE)], work[263];
	INT colors[] = { VID_BLACK, VID_RED, VID_GREEN, VID_YELLOW,
		VID_BLUE, VID_MAGENTA, VID_CYAN, VID_WHITE };

	if (!(dbcflags & DBCFLAG_SERVICE)) {
		if (!(dbcflags & DBCFLAG_KDSINIT)) kdsinit();
		if (dbcflags & DBCFLAG_DEBUG) vdebug(DEBUG_RESTORE);
	}
	if (fpicnt && !(kydsflags & KYDS_FPITEST)) filepi0();  /* cancel any filepi */

	kydsflags &= KYDS_SAVEMASK;
	listflg = LIST_READ | LIST_PROG | LIST_NUM1;
	if (vbcode == 0x20) /* keyin */ {
		psave = pcount;  /* save in case of trap */
		fncendkey = endkey = 0;
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("<k>", 3);
			clientstatesave(savestate);
			vidputtype = CLIENTVIDPUT_SEND;
			savepsave = psave;
			savekydsflags = kydsflags;
		}
		else {
			if (dbcflags & DBCFLAG_DEBUG && dbgflags & DBGFLAGS_DDT) {
				guiappchange((UCHAR *)"consolefocus", 12, NULL, 0);
			}
		}
	}
	else if (dbcflags & DBCFLAG_CLIENTINIT) {
		clientput("<d>", 3);
		vidputtype = CLIENTVIDPUT_SAVE | CLIENTVIDPUT_SEND;
	}
	cmdcnt = 0;

next:
	if (cmdcnt > MAXCMD - 10) {
		vidcmd[cmdcnt] = VID_END_NOFLUSH;
		cmdcnt = 0;
		if (dbcflags & DBCFLAG_KDSINIT) {
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (clientvidput(vidcmd, vidputtype) < 0)\
						dbcerrinfo(505, (UCHAR*)"Unknown VID code", -1);
			}
			else if (vidput(vidcmd) < 0) dbcerrinfo(505, (UCHAR*)"Unknown VID code", -1);
		}
	}
	if ((adr = getlist(listflg)) != NULL) {  /* it's a variable */
		if (vartype & (TYPE_LIST | TYPE_ARRAY)) {  /* list or array variable */
			listflg = (listflg & ~LIST_PROG) | LIST_LIST | LIST_ARRAY;
			goto next;
		}
		if (!(vartype & (TYPE_CHARACTER | TYPE_NUMERIC))) goto next;  /* unsupported variable type */
		if (kydsflags & KYDS_FORMAT) {
			kydsflags &= ~KYDS_FORMAT;
			work[0] = work[1] = '\0';
			work[2] = 0x7F;
			i1 = dbcflags & DBCFLAG_FLAGS;
			vformat(adr, fmtadr, work);
			dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1;
			work[2] = work[1];
			scanvar(work);
			vartype = TYPE_CHARLIT;
			adr = work;
		}
		if (kydsflags & KYDS_KL) {
			kydsflags &= ~KYDS_KL;
			i1 = fp;
			i2 = lp;
			i3 = pl;
			i4 = hl;
			i5 = vartype;
			kllen = getvalue();
			fp = i1;
			lp = i2;
			pl = i3;
			hl = i4;
			vartype = i5;
		}
		else kllen = pl;
		if (vartype & TYPE_LITERAL) kydsflags |= KYDS_LITERAL;
		if (vbcode == 0x14 || (kydsflags & (KYDS_LITERAL | KYDS_DV | KYDS_DOWN))) {
			if (!(kydsflags & KYDS_LITERAL)) {
				if (vartype & TYPE_NUMVAR) {
					if ((vartype & (TYPE_INT | TYPE_FLOAT)) || (kydsflags & (KYDS_ZF | KYDS_ZS | KYDS_DCON))) {
						nvmkform(adr, work);
						adr = work;
					}
					if (kydsflags & KYDS_ZF) {
						for (i1 = 1; i1 <= lp; i1++) {
							if (adr[i1] == ' ') adr[i1] = '0';
							else if (adr[i1] == '-' && i1 > 1) {
								adr[1] = '-';
								adr[i1] = '0';
							}
						}
					}
					if (kydsflags & KYDS_ZS) {
						for (i1 = 1; i1 <= lp && (adr[i1] == ' ' || adr[i1] == '0' || adr[i1] == '.'); i1++);
						if (i1 > lp) memset(&adr[1], ' ', lp);
					}
					if (kydsflags & KYDS_DCON) {
						for (i1 = 1; i1 <= lp && adr[i1] != '.'; i1++);
						if (i1 <= lp) adr[i1] = ',';
					}
				}
				else if (fp == 0) lp = 0;
			}
			i1 = hl;
			i2 = lp;
			i3 = 0;
			if (!(kydsflags & KYDS_LITERAL) && !(vartype & TYPE_NUMVAR)) {
				if ((kydsflags & KYDS_LL) && fp) {
					i1 += fp - 1;
					i2 -= fp - 1;
				}
				if (!(kydsflags & (KYDS_LL | KYDS_SL))) i3 = pl - lp;
			}
			vidcmd[cmdcnt++] = VID_DISPLAY | i2;
			if (sizeof(UCHAR *) > sizeof(INT32)) {
				ptr = &adr[i1];
				memcpy((UCHAR *) &vidcmd[cmdcnt], (UCHAR *) &ptr, sizeof(UCHAR *));
				cmdcnt += (sizeof(UCHAR *) + sizeof(INT32) - 1) / sizeof(INT32);
			}
			else *(UCHAR **)(&vidcmd[cmdcnt++]) = &adr[i1];
			if (i3) vidcmd[cmdcnt++] = VID_DSP_BLANKS | i3;
			if (adr == work) {
				vidcmd[cmdcnt] = VID_END_NOFLUSH;  /* continue because multiple displays of work would not work */
				cmdcnt = 0;
				if (dbcflags & DBCFLAG_KDSINIT) {
					if (dbcflags & DBCFLAG_CLIENTINIT) {
						if (clientvidput(vidcmd, vidputtype) < 0)
							dbcerrinfo(505, (UCHAR*)"Unknown VID code", -1);
					}
					else if (vidput(vidcmd) < 0) dbcerrinfo(505, (UCHAR*)"Unknown VID code", -1);
				}
			}
			if (kydsflags & KYDS_LITERAL) {
				kydsflags &= ~KYDS_LITERAL;
				if (!(vartype & TYPE_ADDRESS)) goto next;
			}
			goto next0;
		}
		if (vartype & TYPE_NULL) setnullvar(adr, FALSE);
		if ((dbcflags & DBCFLAG_KDSINIT) && !(kydsflags & (KYDS_TRAP | KYDS_TIMEOUT | KYDS_FUNCKEY))) {  /* keyin */
			if (keyinerror) dbcerror(keyinerror);
			if ((kydsflags & KYDS_KCON) && !(kydsflags & (KYDS_EDIT | KYDS_EDITON))) vidcmd[cmdcnt++] = VID_KBD_KCON;
			else vidcmd[cmdcnt++] = VID_KBD_KCOFF;
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				vidcmd[cmdcnt] = VID_END_NOFLUSH;
				cmdcnt = 0;
				if (clientvidput(vidcmd, vidputtype) < 0) dbcerror(505);
				if (!(kydsflags & KYDS_CLIENTGET)) {
					if (vartype & TYPE_NUMVAR) {
						clientput("<cn w=", 6);
						clientputnum(pl);
						if (vartype & (TYPE_FLOAT | TYPE_NUM)) {
							if (vartype & TYPE_FLOAT) i1 = adr[1] & 0x1F;
							else {
								for (i1 = 1; i1 <= pl && adr[i1] != '.'; i1++);
								if (i1 <= pl) i1 = pl - i1;
								else i1 = 0;
							}
							if (i1) {
								clientput(" r=", 3);
								clientputnum(i1);
							}
						}
						if (kydsflags & KYDS_DCON) clientput(" dc=y", 5);
					}
					else {
						clientput("<cf w=", 6);
						clientputnum(pl);
						if (kllen < pl) {
							clientput(" kl=", 4);
							clientputnum(kllen);
						}
						if (kydsflags & KYDS_DE) clientput(" de=y", 5);
					}
					if (kydsflags & (KYDS_EDIT | KYDS_EDITON)) {
						if (kydsflags & KYDS_EDIT) clientput(" edit=y", 7);
						clientput(">", 1);
						if (vartype & TYPE_NUMVAR) {
							if (vartype & (TYPE_FLOAT | TYPE_INT)) {
								nvmkform(adr, work);
								adr = work;
							}
						}
						else if ((kydsflags & (KYDS_EDIT | KYDS_EDITON)) && !fp) lp = 0;
						clientputdata((CHAR *)(adr + hl), lp);
						if (vartype & TYPE_NUMVAR) clientput("</cn>", 5);
						else clientput("</cf>", 5);
					}
					else clientput("/>", 2);
				}
				else {
					rc = dbcwait(&keyineventid, 1);
					if (rc < 0) {  /* some type of trap occured */
						if (rc == -1) kydsflags |= KYDS_PRIOR;
						kydsflags |= KYDS_TRAP;
						clientput("<cancel/>", 9);
						if (clientsend(CLIENTSEND_CONTWAIT, 0) < 0) dbcerror(798);
					}
					if (vartype & TYPE_NUMVAR) {
						ptr = work + 1;
						i2 = CLIENTKEYIN_NUM;
						if ((kydsflags & KYDS_JL) && (vartype & (TYPE_FLOAT | TYPE_NUM))) {
							if (vartype & TYPE_FLOAT) i1 = adr[1] & 0x1F;
							else {
								for (i1 = 1; i1 <= pl && adr[i1] != '.'; i1++);
								if (i1 <= pl) i1 = pl - i1;
								else i1 = 0;
							}
							if (i1) i2 |= CLIENTKEYIN_NUMRGT;
						}
					}
					else {
						ptr = adr + hl;
						if (kydsflags & KYDS_RV) i2 = CLIENTKEYIN_RV;
						else i2 = 0;
					}
					fieldlen = clientkeyin(ptr, pl, &endkey, kllen, i2);
					if (fieldlen < 0) dbcerror(798);
					if (fieldlen > 0 || !(kydsflags & KYDS_RV)) {
						if (vartype & TYPE_NUMVAR) {
							if (fieldlen > 0) work[0] = (UCHAR)(0x80 | fieldlen);
							else {
								work[0] = 0x81;
								work[1] = '0';
							}
							i2 = dbcflags & DBCFLAG_FLAGS;
							movevar(work, adr);
							dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i2;
						}
						else {
							if (fieldlen > 0) {
								fp = 1;
								lp = fieldlen;
							}
							else fp = lp = 0;
							setfplp(adr);
						}
					}
					if (endkey == -1) {
						fncendkey = endkey = 0;
						kydsflags |= KYDS_TIMEOUT;
					}
					else if (!endkey && !rc) fncendkey = VID_ENTER;
					else {
						fncendkey = endkey;
						if (fncendkey && fncendkey != VID_ENTER) kydsflags |= KYDS_FUNCKEY;
					}

					if (kydsflags & KYDS_RV) {
						dbcflags &= ~(DBCFLAG_EOS | DBCFLAG_LESS | DBCFLAG_OVER);
						if (fieldlen <= 0) dbcflags |= DBCFLAG_EOS;
						if (kydsflags & KYDS_TIMEOUT) dbcflags |= DBCFLAG_LESS;
						if (kydsflags & (KYDS_TRAP | KYDS_FUNCKEY)) dbcflags |= DBCFLAG_OVER;
					}
				}
				kydsflags |= KYDS_CLIENTKEY;
			}
			else {
				vidcmd[cmdcnt] = VID_END_FLUSH;
				cmdcnt = 0;
				if (vidput(vidcmd) < 0) dbcerror(505);
				evtclear(keyineventid);
				if (vartype & TYPE_NUMVAR) {
					nvmkform(adr, work);
					if (vidkeyinstart(&work[1], pl, lp, pl, (kydsflags & KYDS_DCON) ? VID_KEYIN_NUMERIC | VID_KEYIN_COMMA : VID_KEYIN_NUMERIC, keyineventid))
						dbcdeath(65);
					rc = dbcwait(&keyineventid, 1);
					vidkeyinend(&work[1], &fieldlen, &endkey);
					if (fieldlen > 0 || !(kydsflags & KYDS_RV)) {
						if (fieldlen > 0) work[0] = (UCHAR)(0x80 | fieldlen);
						else {
							work[0] = 0x81;
							work[1] = '0';
						}
						i2 = dbcflags & DBCFLAG_FLAGS;
						movevar(work, adr);
						dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i2;
					}
				}
				else {
					if ((kydsflags & (KYDS_EDIT | KYDS_EDITON)) && !fp) lp = 0;
					if (kydsflags & KYDS_RV) {  /* supports up to dim256 */
						origlen = (hl + pl);
						if (origlen > (INT) sizeof(work)) origlen = sizeof(work);
						memcpy(work, adr, origlen);
					}
					if (vidkeyinstart(adr + hl, kllen, lp, pl, 0, keyineventid)) dbcdeath(65);
					rc = dbcwait(&keyineventid, 1);
					vidkeyinend(adr + hl, &fieldlen, &endkey);
					if (fieldlen > 0) {
						fp = 1;
						lp = fieldlen;
					}
					else if (kydsflags & KYDS_RV) memcpy(adr, work, origlen);
					else fp = lp = 0;
					setfplp(adr);
				}
				if (rc < 0) {  /* some type of trap occured */
					if (rc == -1) kydsflags |= KYDS_PRIOR;
					kydsflags |= KYDS_TRAP;
				}
				if (endkey == -1) {
					fncendkey = endkey = 0;
					kydsflags |= KYDS_TIMEOUT;
				}
				else if (!endkey && !rc) fncendkey = VID_ENTER;
				else {
					fncendkey = endkey;
					if (fncendkey && fncendkey != VID_ENTER) kydsflags |= KYDS_FUNCKEY;
				}

				if (kydsflags & KYDS_RV) {
					dbcflags &= ~(DBCFLAG_EOS | DBCFLAG_LESS | DBCFLAG_OVER);
					if (fieldlen <= 0) dbcflags |= DBCFLAG_EOS;
					if (kydsflags & KYDS_TIMEOUT) dbcflags |= DBCFLAG_LESS;
					if (kydsflags & (KYDS_TRAP | KYDS_FUNCKEY)) dbcflags |= DBCFLAG_OVER;
				}
			}
		}
		else if ((dbcflags & DBCFLAG_KDSINIT) && (kydsflags & (KYDS_EDIT | KYDS_EDITON))) {  /* display *edit character variables anyway */
			if (!(dbcflags & DBCFLAG_CLIENTINIT) && (vartype & (TYPE_CHAR | TYPE_NUMVAR))) {
				if (vartype & TYPE_NUMVAR) {
					if ((vartype & (TYPE_INT | TYPE_FLOAT)) || (kydsflags & KYDS_DCON)) {
						nvmkform(adr, work);
						if (kydsflags & KYDS_DCON) {
							for (i1 = 1; i1 <= lp && work[i1] != '.'; i1++);
							if (i1 <= lp) work[i1] = ',';
						}
						adr = work;
					}
				}
				else if (!fp) lp = 0;
				i1 = lp;
				if (i1 > kllen) i1 = kllen;
				vidcmd[cmdcnt++] = VID_DISPLAY | i1;

				if (sizeof(UCHAR *) > sizeof(INT32)) {
					ptr = &adr[hl];
					memcpy((UCHAR *) &vidcmd[cmdcnt], (UCHAR *) &ptr, sizeof(UCHAR *));
					cmdcnt += (sizeof(UCHAR *) + sizeof(INT32) - 1) / sizeof(INT32);
				}
				else *(UCHAR **)(&vidcmd[cmdcnt++]) = &adr[hl];
				vidcmd[cmdcnt++] = VID_DSP_BLANKS | (USHORT)(kllen - i1);
			}
		}
		else if (!(kydsflags & KYDS_RV)) {  /* clear variable */
			if (vartype & TYPE_NUMVAR) {
				work[0] = 0x81;
				work[1] = '0';
				i1 = dbcflags & DBCFLAG_FLAGS;
				movevar(work, adr);
				dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1;
			}
			else {
				fp = lp = 0;
				setfplp(adr);
			}
		}
next0:
		if (!(listflg & LIST_PROG)) goto next;
next1:
		kydsflags &= ~(KYDS_EDIT | KYDS_DV | KYDS_RV | KYDS_ZF | KYDS_ZS | KYDS_DE | KYDS_JR | KYDS_JL);
		vidcmd[cmdcnt++] = VID_KBD_RESET;
		goto next;
	}
	if (!(listflg & LIST_PROG)) {
		listflg = (listflg & ~(LIST_LIST | LIST_ARRAY)) | LIST_PROG;
		goto next1;
	}

	i1 = pgm[pcount - 1];
	switch (i1) {  /* it is a control code */
		case 0xC4:  /* *DV */
			if (kydsflags & KYDS_CLIENTKEY) {  /* process keyin before displaying variable */
				vidcmd[cmdcnt] = VID_END_NOFLUSH;
				cmdcnt = 0;
				if (clientvidput(vidcmd, vidputtype) < 0) dbcerror(505);
				if (!(kydsflags & KYDS_CLIENTGET)) {
					clientput("</k>", 4);
/*** CODE: FIGURE OUT ERROR VALUE ***/
					if (clientsend(CLIENTSEND_SERIAL | CLIENTSEND_EVENT, keyineventid) < 0) dbcerror(798);
					clientstaterestore(savestate);
					vidputtype = CLIENTVIDPUT_SAVE;
					kydsflags = savekydsflags | KYDS_CLIENTGET;
					pcount = savepsave;
					goto next;
				}
				clientrelease();
				clientput("<k>", 3);
				clientstatesave(savestate);
				vidputtype = CLIENTVIDPUT_SEND;
				kydsflags &= ~(KYDS_CLIENTKEY | KYDS_CLIENTGET | KYDS_CLIENTWAIT);
				savekydsflags = kydsflags;
				savepsave = pcount - 1;
			}
			kydsflags |= KYDS_DV;
			break;
		case 0xC5:  /* *EDIT */
			kydsflags |= KYDS_EDIT;
			vidcmd[cmdcnt++] = VID_KBD_EDIT;
			break;
		case 0xC6:  /* *Pn:n */
			vidcmd[cmdcnt++] = VID_HORZ | (USHORT)(pgm[pcount++] - 1);
			vidcmd[cmdcnt++] = VID_VERT | (USHORT)(pgm[pcount++] - 1);
			break;
		case 0xC7:  /* *Pnvar:nvar */
			vidcmd[cmdcnt++] = VID_HORZ | (USHORT)(getvalue() - 1);
			vidcmd[cmdcnt++] = VID_VERT | (USHORT)(getvalue() - 1);
			break;
		case 0xC8:  /* *Pn:nvar */
			vidcmd[cmdcnt++] = VID_HORZ | (USHORT)(getbyte() - 1);
			vidcmd[cmdcnt++] = VID_VERT | (USHORT)(getvalue() - 1);
			break;
		case 0xC9:  /* *Pnvar:n */
			vidcmd[cmdcnt++] = VID_HORZ | (USHORT)(getvalue() - 1);
			vidcmd[cmdcnt++] = VID_VERT | (USHORT)(getbyte() - 1);
			break;
		case 0xCB:  /* *ES */
			vidcmd[cmdcnt++] = VID_ES;
			break;
		case 0xCC:  /* *EF */
			vidcmd[cmdcnt++] = VID_EF;
			break;
		case 0xCD:  /* *EL */
			vidcmd[cmdcnt++] = VID_EL;
			break;
		case 0xCE:  /* *R */
			vidcmd[cmdcnt++] = VID_RU;
			break;
		case 0xCF:  /* *RD */
			vidcmd[cmdcnt++] = VID_RD;
			break;
		case 0xD0:  /* *IT */
			if (kydsflags & KYDS_COMPAT) vidcmd[cmdcnt++] = VID_KBD_IN;
			else if (kydsflags & KYDS_UPPER) vidcmd[cmdcnt++] = VID_KBD_LC;
			else vidcmd[cmdcnt++] = VID_KBD_IC;
			break;
		case 0xD1:  /* *IN */
			if (kydsflags & KYDS_COMPAT) vidcmd[cmdcnt++] = VID_KBD_IC;
			else if (kydsflags & KYDS_UPPER) vidcmd[cmdcnt++] = VID_KBD_UC;
			else vidcmd[cmdcnt++] = VID_KBD_IN;
			break;
		case 0xD2:  /* *ZF */
			kydsflags |= KYDS_ZF;
			vidcmd[cmdcnt++] = VID_KBD_ZF;
			break;
		case 0xD3:  /* *- (*PL or *KCOFF) */
			kydsflags &= ~(KYDS_KCON | KYDS_LL | KYDS_SL);
			break;
		case 0xD4:  /* *RV */
			kydsflags |= KYDS_RV;
			break;
		case 0xD5:  /* *DE */
			kydsflags |= KYDS_DE;
			vidcmd[cmdcnt++] = VID_KBD_DE;
			break;
		case 0xD6:  /* *ULON */
			vidcmd[cmdcnt++] = VID_UL_ON;
			break;
		case 0xD7:  /* *HON and *REVON */
			vidcmd[cmdcnt++] = VID_REV_ON;
			break;
		case 0xD8:  /* *HOFF and *ALLOFF */
			vidcmd[cmdcnt++] = VID_REV_OFF;
			vidcmd[cmdcnt++] = VID_BOLD_OFF;
			vidcmd[cmdcnt++] = VID_UL_OFF;
			vidcmd[cmdcnt++] = VID_BLINK_OFF;
			vidcmd[cmdcnt++] = VID_DIM_OFF;
			if (kydsflags & KYDS_OLDHOFF) {
				vidcmd[cmdcnt++] = deffgcolor;
				vidcmd[cmdcnt++] = defbgcolor;
			}
			break;
		case 0xD9:  /* *B */
			vidcmd[cmdcnt++] = VID_BEEP | 5;
			kydsflags |= KYDS_CLIENTWAIT;
			break;
		case 0xDA:  /* Wn */
			i2 = getbyte();
			if (!i2 || !(dbcflags & DBCFLAG_KDSINIT)) break;
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (!(kydsflags & KYDS_CLIENTGET)) {
					vidcmd[cmdcnt] = VID_END_NOFLUSH;
					cmdcnt = 0;
					if (clientvidput(vidcmd, vidputtype) < 0) dbcerror(505);
					clientput("<wait n=", 8);
					clientputnum(i2);
					clientput("/>", 2);
				}
				kydsflags |= KYDS_CLIENTWAIT;
			}
			else {
				vidcmd[cmdcnt] = VID_END_FLUSH;
				cmdcnt = 0;
				if (vidput(vidcmd) < 0) dbcerror(505);
					if (!(kydsflags & (KYDS_TRAP | KYDS_TIMEOUT | KYDS_FUNCKEY))) {
					i2 = kdspause(i2);
					if (i2 < 0) {
						if (i2 == -1) kydsflags |= KYDS_PRIOR;
						kydsflags |= KYDS_TRAP;
					}
				}
			}
			break;
		case 0xDB:  /* *EOFF */
			vidcmd[cmdcnt++] = VID_KBD_EOFF;
			break;
		case 0xDC:  /* *EON */
			vidcmd[cmdcnt++] = VID_KBD_EON;
			break;
		case 0xDD:  /* *JL */
			kydsflags &= ~KYDS_JR;
			kydsflags |= KYDS_JL;
			vidcmd[cmdcnt++] = VID_KBD_JL;
			break;
		case 0xDE:  /* *JR */
			kydsflags &= ~KYDS_JL;
			kydsflags |= KYDS_JR;
			vidcmd[cmdcnt++] = VID_KBD_JR;
			break;
		case 0xDF:  /* *BLINKON */
			vidcmd[cmdcnt++] = VID_BLINK_ON;
			break;
		case 0xE0:
		case 0xE1:
		case 0xE2:
		case 0xE3:
		case 0xE4:
		case 0xE5:
		case 0xE6:
		case 0xE7:  /* *color */
			if (kydsflags & KYDS_BACKGND) {  /* background */
				kydsflags &= ~KYDS_BACKGND;
				vidcmd[cmdcnt] = VID_BACKGROUND | colors[i1 - 0xE0];
			}
			else vidcmd[cmdcnt] = VID_FOREGROUND | colors[i1 - 0xE0];
			cmdcnt++;
			break;
		case 0xE8:  /* *+ (*SL or *KCON) */
			kydsflags |= KYDS_KCON | KYDS_SL;
			kydsflags &= ~KYDS_LL;
			break;
		case 0xE9:  /* *N */
			vidcmd[cmdcnt++] = VID_NL;
			break;
		case 0xEA:  /* *Tn */
			i1 = getbyte();
			if (i1 == 0xFF) i1 = 0xFFFF;
			vidcmd[cmdcnt++] = VID_KBD_TIMEOUT | i1;
			break;
		case 0xEB:  /* *L */
			vidcmd[cmdcnt++] = VID_LF;
			break;
		case 0xEC:  /* *C */
			vidcmd[cmdcnt++] = VID_CR;
			break;
		case 0xED:  /* *V2LON and *DION */
			vidcmd[cmdcnt++] = VID_BOLD_ON;
			break;
		case 0xEE:  /* *F */
			dbcerror(505);
			break;
		case 0xEF:  /* *DIM */
			vidcmd[cmdcnt++] = VID_DIM_ON;
			break;
		case 0xF0:  /* *CL */
			if (!(dbcflags & DBCFLAG_KDSINIT)) break;
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (!(kydsflags & KYDS_CLIENTGET)) {
					vidcmd[cmdcnt] = VID_END_NOFLUSH;
					cmdcnt = 0;
					if (clientvidput(vidcmd, vidputtype) < 0) dbcerror(505);
					clientput("<cl/>", 5);
				}
			}
			else vidkeyinclear();
			break;
		case 0xF1: /* *LL */
			kydsflags |= KYDS_LL;
			kydsflags &= ~KYDS_SL;
			break;
		case 0xF2: /* *PL */
			kydsflags &= ~(KYDS_LL | KYDS_SL);
			break;
#if 0
		case 0xF3: /* *MP */
			break;
#endif
		case 0xF4:  /* *OVER  (means next item in keyin list is literal) */
			kydsflags |= KYDS_LITERAL;
			break;
		case 0xF5:  /* *Hn */
			vidcmd[cmdcnt++] = VID_HORZ | (USHORT)(getvalue() - 1);
			break;
		case 0xF6:  /* *HAn */
			vidcmd[cmdcnt++] = VID_HORZ_ADJ | (USHORT) getvalue();
			break;
		case 0xF7:  /* *Vn */
			vidcmd[cmdcnt++] = VID_VERT | (USHORT)(getvalue() - 1);
			break;
		case 0xF8:  /* *VAn */
			vidcmd[cmdcnt++] = VID_VERT_ADJ | (USHORT) getvalue();
			break;
		case 0xF9:  /* *BACKGRND */
			kydsflags |= KYDS_BACKGND;
			break;
		case 0xFA:
			goto switch2;
		case 0xFB:  /* display character pass through */
			vidcmd[cmdcnt++] = VID_DISP_CHAR | getbyte();
			break;
		case 0xFF:  /* end of list or *SCREND if down flag */
			if (kydsflags & KYDS_DOWN) {
				kydsflags &= ~KYDS_DOWN;
				vidcmd[cmdcnt++] = VID_DSPDOWN_OFF;
				break;
			}
			if (cmdcnt && vidcmd[cmdcnt - 1] == VID_KBD_RESET) cmdcnt--;
			vidcmd[cmdcnt++] = VID_NL;
			/* fall through */
		case 0xFE:  /* semi-colon end of list */
			if (!(dbcflags & DBCFLAG_KDSINIT)) return;
			if (cmdcnt && vidcmd[cmdcnt - 1] == VID_KBD_RESET) cmdcnt--;
			vidcmd[cmdcnt] = VID_FINISH;
			cmdcnt = 0;
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (clientvidput(vidcmd, vidputtype) < 0) dbcerror(505);
				if (!(kydsflags & KYDS_CLIENTGET)) {
					if (vbcode == 0x20) clientput("</k>", 4);
					else clientput("</d>", 4);
					if (!(kydsflags & KYDS_CLIENTKEY) && (kydsflags & KYDS_CLIENTWAIT)) clientput("<s/>", 4);
					if (kydsflags & (KYDS_CLIENTWAIT | KYDS_CLIENTKEY)) i1 = CLIENTSEND_SERIAL | CLIENTSEND_EVENT;
					else i1 = 0;
/*** CODE: FIGURE OUT ERROR VALUE ***/
					if (clientsend(i1, keyineventid) < 0) dbcerror(798);
					if (vbcode == 0x20) {
						clientstaterestore(savestate);
						vidputtype = CLIENTVIDPUT_SAVE;
						kydsflags = savekydsflags | KYDS_CLIENTGET;
						pcount = savepsave;
						goto next;
					}
				}
				if (kydsflags & KYDS_CLIENTKEY) clientrelease();
				else if (kydsflags & KYDS_CLIENTWAIT) {
					i1 = dbcwait(&keyineventid, 1);
					if (i1 < 0) {  /* some type of trap occured */
						if (i1 == -1) kydsflags |= KYDS_PRIOR;
						kydsflags |= KYDS_TRAP;
					}
					clientrelease();
				}
			}
			else if (vidput(vidcmd) < 0) dbcerror(505);

			/* trap occurred during keyin, reset pcount to start of keyin */
			if ((kydsflags & KYDS_PRIOR) && vbcode == 0x20) pcount = psave - 1;
			return;
	}
	goto next;

switch2:
	i1 = getbyte();
	switch(i1) {
		case 0x01:  /* HD */
			vidcmd[cmdcnt++] = VID_HD;
			break;
		case 0x02:  /* *HU */
			vidcmd[cmdcnt++] = VID_HU;
			break;
		case 0x03:  /* *INSCHR n:n */
			vidcmd[cmdcnt++] = VID_INSCHAR;
			i2 = getvalue() - 1;
			vidcmd[cmdcnt++] = (((UINT) i2) << 16) | (USHORT)(getvalue() - 1);
			break;
		case 0x04:  /* *DELCHR n:n */
			vidcmd[cmdcnt++] = VID_DELCHAR;
			i2 = getvalue() - 1;
			vidcmd[cmdcnt++] = (((UINT) i2) << 16) | (USHORT)(getvalue() - 1);
			break;
		case 0x05:  /* *INSLIN */
			vidcmd[cmdcnt++] = VID_IL;
			break;
		case 0x06:  /* *DELLIN */
			vidcmd[cmdcnt++] = VID_DL;
			break;
		case 0x07:  /* *OPNLIN */
			vidcmd[cmdcnt++] = VID_OL;
			break;
		case 0x08:  /* *CLSLIN */
			vidcmd[cmdcnt++] = VID_CL;
			break;
		case 0x09:  /* *RPTCHAR var:n */
		case 0x22:  /* *RPTDOWN var:n */
			if (i1 == 0x22) vidcmd[cmdcnt++] = VID_DSPDOWN_ON;
			if (pgm[pcount] == 0xFA) {  /* graphics */
				pcount++;
				c1 = (UCHAR) (getbyte() - (UCHAR) 128);
				vidcmd[cmdcnt + 1] = VID_DISP_SYM | c1;
			}
			else {  /* normal cvar */
				adr = getvar(VAR_READ);
				if (fp) c1 = adr[fp + hl - 1];
				else c1 = ' ';
				vidcmd[cmdcnt + 1] = VID_DISP_CHAR | c1;
			}
			i2 = getvalue();
			if (i2 > 0) {
				vidcmd[cmdcnt] = VID_REPEAT | i2;
				cmdcnt += 2;
			}
			if (i1 == 0x22) vidcmd[cmdcnt++] = VID_DSPDOWN_OFF;
			break;
		case 0x0A:  /* *SCRLEFT list*/
			kydsflags |= KYDS_DOWN;
			vidcmd[cmdcnt++] = VID_RL;
			vidcmd[cmdcnt++] = VID_EU;
			vidcmd[cmdcnt++] = VID_DSPDOWN_ON;
			break;
		case 0x0B:  /* *SCRRIGHT list */
			kydsflags |= KYDS_DOWN;
			vidcmd[cmdcnt++] = VID_RR;
			vidcmd[cmdcnt++] = VID_HU;
			vidcmd[cmdcnt++] = VID_DSPDOWN_ON;
			break;
		case 0x0C:  /* *SETSWTB n:n */
			vidcmd[cmdcnt++] = VID_WIN_TOP | (USHORT)(getvalue() - 1);
			vidcmd[cmdcnt++] = VID_WIN_BOTTOM | (USHORT)(getvalue() - 1);
			vidcmd[cmdcnt++] = VID_HU;
			break;
		case 0x0D:  /* *SETSWLR n:n */
			vidcmd[cmdcnt++] = VID_WIN_LEFT | (USHORT)(getvalue() - 1);
			vidcmd[cmdcnt++] = VID_WIN_RIGHT | (USHORT)(getvalue() - 1);
			vidcmd[cmdcnt++] = VID_HU;
			break;
		case 0x0E:  /* *RESETSW */
			vidcmd[cmdcnt++] = VID_WIN_RESET;
			if (!(kydsflags & KYDS_RESETSW)) vidcmd[cmdcnt++] = VID_HU;
			break;
		case 0x0F:  /* ESON */
			vidcmd[cmdcnt++] = VID_KBD_ESON;
			break;
		case 0x10:  /* ESOFF */
			vidcmd[cmdcnt++] = VID_KBD_ESOFF;
			break;
		case 0x11:  /* ESCHAR var */
			adr = getvar(VAR_READ);
			if (fp) vidcmd[cmdcnt++] = VID_KBD_ECHOCHR | adr[hl + fp - 1];
			break;
		case 0x12:  /* CLICK */
			vidcmd[cmdcnt++] = VID_SOUND;
			vidcmd[cmdcnt++] = 0x70000001;
			break;
		case 0x13:  /* CURSON */
			vidcmd[cmdcnt++] = VID_CURSOR_NORM;
			break;
		case 0x14:  /* CURSOFF */
			vidcmd[cmdcnt++] = VID_CURSOR_OFF;
			break;
		case 0x15:  /* CLICKON */
			vidcmd[cmdcnt++] = VID_KBD_CK_ON;
			break;
		case 0x16:  /* CLICKOFF */
			vidcmd[cmdcnt++] = VID_KBD_CK_OFF;
			break;
		case 0x17:  /* *RAW */
			vidcmd[cmdcnt++] = VID_RAW_ON;
			break;
		case 0x18:  /* RAWOFF */
			vidcmd[cmdcnt++] = VID_RAW_OFF;
			break;
		case 0x19:  /* PON */
			vidcmd[cmdcnt++] = VID_PRT_ON;
			break;
		case 0x1A:  /* POFF */
			vidcmd[cmdcnt++] = VID_PRT_OFF;
			break;
		case 0x1B:  /* *UC */
			vidcmd[cmdcnt++] = VID_KBD_UC;
			break;
		case 0x1C:  /* *LC */
			vidcmd[cmdcnt++] = VID_KBD_LC;
			break;
		case 0x1D:  /* DIOFF */
			vidcmd[cmdcnt++] = VID_BOLD_OFF;
			break;
		case 0x1E:  /* ULOFF */
			vidcmd[cmdcnt++] = VID_UL_OFF;
			break;
		case 0x1F:  /* BLINKOFF */
			vidcmd[cmdcnt++] = VID_BLINK_OFF;
			break;
		case 0x20:  /* *COLOROFF */
			vidcmd[cmdcnt++] = deffgcolor;
			vidcmd[cmdcnt++] = defbgcolor;
			break;
		case 0x21:  /* *REVOFF */
			vidcmd[cmdcnt++] = VID_REV_OFF;
			break;
		case 0x23:  /* *RTLEFT */
			vidcmd[cmdcnt++] = VID_RL_ON;
			break;
		case 0x24:  /* *LTORIGHT */
			vidcmd[cmdcnt++] = VID_RL_OFF;
			break;
		case 0x25:  /* *EDITON */
			kydsflags |= KYDS_EDITON;
			vidcmd[cmdcnt++] = VID_KBD_ED_ON;
			break;
		case 0x26:  /* *EDITOFF */
			kydsflags &= ~(KYDS_EDIT | KYDS_EDITON);
			vidcmd[cmdcnt++] = VID_KBD_ED_OFF;
			break;
		case 0x28:  /* *EU */
			vidcmd[cmdcnt++] = VID_EU;
			break;
		case 0x29:  /* *ED */
			vidcmd[cmdcnt++] = VID_ED;
			break;
		case 0x2A:  /* FKEYON */
#if 0
/*** STILL NOT EXACTLY LIKE V7 SO MAKE FUNCTION OBSOLETE ***/
	UCHAR kbdfmap[(VID_MAXKEYVAL >> 3) + 1];
			vidkeyingetfinishmap(kbdfmap);
			for (i2 = VID_F1; i2 <= VID_F20; i2++) kbdfmap[(i2 >> 3)] |= (1 << (i2 & 0x07));
			vidkeyinfinishmap(kbdfmap);
#else
			dbcerror(565);
#endif
			break;
		case 0x2B:  /* FKEYOFF */
#if 0
/*** STILL NOT EXACTLY LIKE V7 SO MAKE FUNCTION OBSOLETE ***/
			vidkeyingetfinishmap(kbdfmap);
			for (i2 = VID_F1; i2 <= VID_F20; i2++) kbdfmap[(i2 >> 3)] &= ~(1 << (i2 & 0x07));
			vidkeyinfinishmap(kbdfmap);
#else
			dbcerror(565);
#endif
			break;
		case 0x2C:  /* XFKEYON */
#if 0
/*** STILL NOT EXACTLY LIKE V7 SO MAKE FUNCTION OBSOLETE ***/
			vidkeyingetfinishmap(kbdfmap);
			kbdfmap[(VID_ESCAPE >> 3)] |= (1 << (VID_ESCAPE & 0x07));
			for (i2 = VID_TAB; i2 <= VID_PGDN; i2++) kbdfmap[(i2 >> 3)] |= (1 << (i2 & 0x07));
			vidkeyinfinishmap(kbdfmap);
#else
			dbcerror(565);
#endif
			break;
		case 0x2D:  /* XFKEYOFF */
#if 0
/*** STILL NOT EXACTLY LIKE V7 SO MAKE FUNCTION OBSOLETE ***/
			vidkeyingetfinishmap(kbdfmap);
			kbdfmap[(VID_ESCAPE >> 3)] &= ~(1 << (VID_ESCAPE & 0x07));
			for (i2 = VID_TAB; i2 <= VID_PGDN; i2++) kbdfmap[(i2 >> 3)] &= ~(1 << (i2 & 0x07));
			vidkeyinfinishmap(kbdfmap);
#else
			dbcerror(565);
#endif
			break;
		case 0x2E:  /* *N <var or con> */
			i2 = getvalue();
			if (i2 > 0) {
				vidcmd[cmdcnt++] = VID_REPEAT | i2;
				vidcmd[cmdcnt++] = VID_NL;
			}
			break;
		case 0x2F:  /* *W <var or con> */
			i2 = getvalue();
			if (i2 <= 0 || !(dbcflags & DBCFLAG_KDSINIT)) break;
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (!(kydsflags & KYDS_CLIENTGET)) {
					vidcmd[cmdcnt] = VID_END_NOFLUSH;
					cmdcnt = 0;
					if (clientvidput(vidcmd, vidputtype) < 0) dbcerror(505);
					clientput("<wait n=", 8);
					clientputnum(i2);
					clientput("/>", 2);
				}
				kydsflags |= KYDS_CLIENTWAIT;
			}
			else {
				vidcmd[cmdcnt] = VID_END_FLUSH;
				cmdcnt = 0;
				if (vidput(vidcmd) < 0) dbcerror(505);
				if (i2 > 0 && !(kydsflags & (KYDS_TRAP | KYDS_TIMEOUT | KYDS_FUNCKEY))) {
					i2 = kdspause(i2);
					if (i2 < 0) {
						if (i2 == -1) kydsflags |= KYDS_PRIOR;
						kydsflags |= KYDS_TRAP;
					}
				}
			}
			break;
		case 0x30:  /* *T <var or con> */
			i1 = getvalue();
			if ((UINT) i1 > 0x7FFF) i1 = 0xFFFF;
			vidcmd[cmdcnt++] = VID_KBD_TIMEOUT | i1;
			break;
		case 0x31:  /* *FORMAT <var> */
			kydsflags |= KYDS_FORMAT;
			fmtadr = getlist(listflg);
			break;
		case 0x33:  /* *KL=<cvar>:<nvar> */
			kydsflags |= KYDS_KL | KYDS_EDIT;
			vidcmd[cmdcnt++] = VID_KBD_EDIT;
			break;
		case 0x34:  /* *COLOR <var or con> */
			i2 = getvalue();
			vidcmd[cmdcnt++] = VID_FOREGROUND | (i2 & 0xFF);
			break;
		case 0x35:  /* *BGCOLOR <var or con> */
			i2 = getvalue();
			vidcmd[cmdcnt++] = VID_BACKGROUND | (i2 & 0xFF);
			break;
		case 0x36:  /* *INVISIBLE */
/*** NO LONGER SUPPORTED ***/
/***			vidcmd[cmdcnt++] = VID_DROPSW; ***/
			break;
		case 0x37:  /* *SL */
			kydsflags |= KYDS_SL;
			kydsflags &= ~KYDS_LL;
			break;
		case 0x38:  /* *ZS */
			kydsflags |= KYDS_ZS;
			break;
		case 0x39:  /* *SETSWALL=<nvar>:<nvar>:<nvar>:<nvar> */
			vidcmd[cmdcnt++] = VID_WIN_TOP | (USHORT)(getvalue() - 1);
			vidcmd[cmdcnt++] = VID_WIN_BOTTOM | (USHORT)(getvalue() - 1);
			vidcmd[cmdcnt++] = VID_WIN_LEFT | (USHORT)(getvalue() - 1);
			vidcmd[cmdcnt++] = VID_WIN_RIGHT | (USHORT)(getvalue() - 1);
			vidcmd[cmdcnt++] = VID_HU;
			break;
		case 0x3A:  /* *CURSOR=*ON */
			vidcmd[cmdcnt++] = VID_CURSOR_ON;
			break;
		case 0x3B:  /* *CURSOR=*BLOCK */
			vidcmd[cmdcnt++] = VID_CURSOR_BLOCK;
			break;
		case 0x3C:  /* *CURSOR=*HALF */
			vidcmd[cmdcnt++] = VID_CURSOR_HALF;
			break;
		case 0x3D:  /* *CURSOR=*ULINE */
			vidcmd[cmdcnt++] = VID_CURSOR_ULINE;
			break;
		case 0x3E:  /* *F=<nvar> */
			dbcerror(505);
			break;
		case 0x3F:  /* *C=<nvar> */
			i2 = getvalue();
			if (i2 > 0) {
				vidcmd[cmdcnt++] = VID_REPEAT | i2;
				vidcmd[cmdcnt++] = VID_CR;
			}
			break;
		case 0x40:  /* *L=<nvar> */
			i2 = getvalue();
			if (i2 > 0) {
				vidcmd[cmdcnt++] = VID_REPEAT | i2;
				vidcmd[cmdcnt++] = VID_LF;
			}
			break;
		case 0x41:  /* *R=<nvar> */
			i2 = getvalue();
			if (i2 > 0) {
				vidcmd[cmdcnt++] = VID_REPEAT | i2;
				vidcmd[cmdcnt++] = VID_RU;
			}
			break;
		case 0x42:  /* *RD=<nvar> */
			i2 = getvalue();
			if (i2 > 0) {
				vidcmd[cmdcnt++] = VID_REPEAT | i2;
				vidcmd[cmdcnt++] = VID_RD;
			}
			break;
		case 0x80:  /* *HLN */
		case 0x81:  /* *VLN */
		case 0x82:  /* *CRS */
		case 0x83:  /* *ULC */
		case 0x84:  /* *URC */
		case 0x85:  /* *LLC */
		case 0x86:  /* *LRC */
		case 0x87:  /* *RTK */
		case 0x88:  /* *DTK */
		case 0x89:  /* *LTK */
		case 0x8A:  /* *UTK */
			vidcmd[cmdcnt++] = VID_DISP_SYM | (USHORT)(i1 - 128);
			break;
		case 0x8B:  /* *DBLON */
			vidcmd[cmdcnt++] = VID_HDBL_ON;
			vidcmd[cmdcnt++] = VID_VDBL_ON;
			break;
		case 0x8C:  /* *DBLOFF */
			vidcmd[cmdcnt++] = VID_HDBL_OFF;
			vidcmd[cmdcnt++] = VID_VDBL_OFF;
			break;
		case 0x8D:  /* *HDBLON */
			vidcmd[cmdcnt++] = VID_HDBL_ON;
			break;
		case 0x8E:  /* *HDBLOFF */
			vidcmd[cmdcnt++] = VID_HDBL_OFF;
			break;
		case 0x8F:  /* *VDBLON */
			vidcmd[cmdcnt++] = VID_VDBL_ON;
			break;
		case 0x90:  /* *VDBLOFF */
			vidcmd[cmdcnt++] = VID_VDBL_OFF;
			break;
		case 0x91:  /* *INSMODE */
			vidcmd[cmdcnt++] = VID_KBD_OVS_OFF;
			break;
		case 0x92:  /* *OVSMODE */
			vidcmd[cmdcnt++] = VID_KBD_OVS_ON;
			break;
		case 0x93:  /* *UPA */
		case 0x94:  /* *RTA */
		case 0x95:  /* *DNA */
		case 0x96:  /* *LFA */
			vidcmd[cmdcnt++] = VID_DISP_SYM | (USHORT)(VID_SYM_UPA + i1 - 147);
			break;
		case 0x97:  /* *DCON */
			kydsflags |= KYDS_DCON;
			break;
		case 0x98:  /* *DCOFF */
			kydsflags &= ~KYDS_DCON;
			break;
	}
	goto next;
}

/*
 * Check condition flags, return TRUE if condition is set
 */
INT chkcond(INT n)
{
	INT fnckey, eqlflg;

	if (n < 10) {
		switch (n) {
			case 0: return(dbcflags & DBCFLAG_EQUAL);
			case 1: return(dbcflags & DBCFLAG_LESS);
			case 2: return(dbcflags & DBCFLAG_OVER);
			case 3: return(dbcflags & DBCFLAG_EOS);
			case 4: return(!(dbcflags & DBCFLAG_EQUAL));
			case 5: return(!(dbcflags & DBCFLAG_LESS));
			case 6: return(!(dbcflags & DBCFLAG_OVER));
			case 7: return(!(dbcflags & DBCFLAG_EOS));
			case 8: return(!(dbcflags & (DBCFLAG_LESS | DBCFLAG_EQUAL)));
			case 9: return(dbcflags & (DBCFLAG_LESS | DBCFLAG_EQUAL));
		}
	}

	/* pre-version 8 conditions */
	if (n == 59) return(!(dbcflags & (DBCFLAG_LESS | DBCFLAG_EQUAL)));
	if (n == 60) return(dbcflags & DBCFLAG_EXPRESSION);
	if (n == 79) return(dbcflags & (DBCFLAG_LESS | DBCFLAG_EQUAL));
	if (n == 80) return(!(dbcflags & DBCFLAG_EXPRESSION));

	fnckey = 0;
	eqlflg = TRUE;
	if (n >= 21 && n <= 30) fnckey = n - 21 + VID_F1;  /* F1 - F10 */
	else if (n >= 41 && n <= 50) fnckey = n - 41 + VID_UP;  /* UP - PGDN */
	else if (n == 52) fnckey = VID_ESCAPE;
	else if (n == 53) fnckey = VID_ENTER;
	else if (n == 54) fnckey = VID_TAB;
	else if (n == 55) fnckey = VID_BKTAB;
	else if (n >= 81 && n <= 90) fnckey = n - 81 + VID_F11;  /* F11 - F20 */
	else {  /* NOT */
		eqlflg = FALSE;
		if (n >= 31 && n <= 40) fnckey = n - 31 + VID_F1;  /* F1 - F10 */
		else if (n >= 61 && n <= 70) fnckey = n - 61 + VID_UP;  /* UP - PGDN */
		else if (n == 72) fnckey = VID_ESCAPE;
		else if (n == 73) fnckey = VID_ENTER;
		else if (n == 74) fnckey = VID_TAB;
		else if (n == 75) fnckey = VID_BKTAB;
		else if (n >= 91 && n <= 100) fnckey = n - 91 + VID_F11;  /* F11 - F20 */
	}
	if (fnckey == fncendkey) {
		fncendkey = 0;
		return(eqlflg);
	}
#if OS_WIN32
	if ((kydsflags & KYDS_FSHIFT) && fnckey >= VID_F11 && fnckey <= VID_F20 && fnckey == fncendkey + VID_F11 - VID_SHIFTF1) {
		fncendkey = 0;
		return(eqlflg);
	}
#endif
	return(!eqlflg);
}

/**
 * setendkey
 * clearendkey
 * getendkey
 */
void dbckds(INT vx)
{
	INT clearflag = FALSE, key;
	UCHAR kbdfmap[(VID_MAXKEYVAL >> 3) + 1];

	if (!(dbcflags & DBCFLAG_SERVICE)) {
		if (!(dbcflags & DBCFLAG_KDSINIT)) kdsinit();
		if (dbcflags & DBCFLAG_DEBUG) vdebug(DEBUG_RESTORE);
	}

	switch(vx) {
		case 0xD3:  /* SETENDKEY */
			if (dbcflags & DBCFLAG_KDSINIT) {
				if (dbcflags & DBCFLAG_CLIENTINIT) memset(kbdfmap, 0, sizeof(kbdfmap));
				else vidkeyingetfinishmap(kbdfmap);
			}
			while (getnextkey(&key)) {
				if (key <= 0 || key > VID_MAXKEYVAL) continue;
				kbdfmap[key >> 3] |= 1 << (key & 0x07);
			}
			if (dbcflags & DBCFLAG_KDSINIT) {
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					if (clientsetendkey(kbdfmap)) dbcerror(798);
				}
				else vidkeyinfinishmap(kbdfmap);
			}
			break;
		case 0xD4:  /* CLEARENDKEY */
			if (dbcflags & DBCFLAG_KDSINIT) {
				clearflag = FALSE;
				if (dbcflags & DBCFLAG_CLIENTINIT) memset(kbdfmap, 0, sizeof(kbdfmap));
				else vidkeyingetfinishmap(kbdfmap);
			}
			while (getnextkey(&key)) {
				if (key < 0 || key > VID_MAXKEYVAL || !(dbcflags & DBCFLAG_KDSINIT)) continue;
				if (!key) {
					memset(kbdfmap, 0, (VID_MAXKEYVAL >> 3) + 1);
					kbdfmap[VID_ENTER >> 3] |= 1 << (VID_ENTER & 0x07);
					if (dbcflags & DBCFLAG_CLIENTINIT) {
						if (clientclearendkey(NULL)) dbcerror(798);
						if (clientsetendkey(kbdfmap)) dbcerror(798);
						kbdfmap[VID_ENTER >> 3] = 0;
						clearflag = FALSE;
					}
				}
				else if (dbcflags & DBCFLAG_CLIENTINIT) {
					kbdfmap[key >> 3] |= 1 << (key & 0x07);
					clearflag = TRUE;
				}
				else kbdfmap[key >> 3] &= ~(1 << (key & 0x07));
			}
			if (dbcflags & DBCFLAG_KDSINIT) {
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					if (clearflag && clientclearendkey(kbdfmap)) dbcerror(798);
				}
				else vidkeyinfinishmap(kbdfmap);
			}
			break;
		case 0xD5:  /* GETENDKEY */
			itonv(endkey, getvar(VAR_WRITE));
			break;
		default:
			dbcerror(505);
			break;
	}
}

static INT getnextkey(INT *key)
{
	INT value;
	UCHAR *adr;

	adr = getlist(LIST_READ | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1);
	if (adr != NULL) value = nvtoi(adr);  /* address of variable */
	else {
		value = pgm[pcount - 1];
		if (value == 0xFC) value = (INT) gethhll();  /* 2 byte constant */
		else if (value == 0xFD) {  /* 1 byte constant from 256 - 511 */
			value = 256 + pgm[pcount++];
		}
		else if (value == 0xFE) {  /* 1 byte constant */
			value = pgm[pcount++];
		}
		else if (value == 0xFF) return(FALSE);  /* end of list */
		else {
			dbcerror(505);
			return(FALSE);
		}
	}
	*key = value;
	return(TRUE);
}

void vscrnsave(INT type)  /* save screen verbs */
{
	INT n = 0;
	UCHAR *adr1;
 
	if (!(dbcflags & DBCFLAG_SERVICE)) {
		if (dbcflags & DBCFLAG_DEBUG) vdebug(DEBUG_RESTORE);
	}

	adr1 = getvar(VAR_WRITE);
	if (!(dbcflags & DBCFLAG_SERVICE) && !(dbcflags & DBCFLAG_KDSINIT)) {
		return;
	}
	if (dbcflags & DBCFLAG_KDSINIT) {
		lp = pl; // Note! Field being passed as LP is set to physical length
		switch(type) {
		case 0x00:
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				n = clientvidsave(CLIENT_VID_CHAR, adr1 + hl, &lp);
				if (n != 0) dbcerror(798);
			}
			else n = vidsavechar(adr1 + hl, &lp);
			break;
		case 0x01:
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				n = clientvidsave(CLIENT_VID_WIN, adr1 + hl, &lp);
				if (n != 0) dbcerror(798);
			}
			else n = vidsavewin(adr1 + hl, &lp);
			break;
		case 0x02:
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				n = clientvidsave(CLIENT_VID_STATE, adr1 + hl, &lp);
				if (n != 0) dbcerror(798);
			}
			else n = vidsavestate(adr1 + hl, &lp);
			break;
		case 0x03:
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				n = clientvidsave(CLIENT_VID_SCRN, adr1 + hl, &lp);
				if (n != 0) dbcerror(798);
			}
			else n = vidsavescreen(adr1 + hl, &lp);
			break;
		default:
			dbcerrinfo(1798, (UCHAR*)"Invalid 'type' in vscrnsave", -1);
		}
	}
	else {
		lp = 0;
		n = 1;
	}
	if (!n) dbcflags &= ~DBCFLAG_EOS;
	else dbcflags |= DBCFLAG_EOS;
	if (lp) fp = 1;
	else fp = 0;
	setfplp(adr1);
}

void vscrnrest(INT type)  /* restore screen verbs */
{
	INT n = 0;
	UCHAR *adr1;

	if (!(dbcflags & DBCFLAG_SERVICE)) {
		if (!(dbcflags & DBCFLAG_KDSINIT) && type != 0x02) kdsinit();
		if (dbcflags & DBCFLAG_DEBUG) vdebug(DEBUG_RESTORE);
	}
	
	adr1 = getvar(VAR_READ);
	if (!(dbcflags & DBCFLAG_SERVICE) && !(dbcflags & DBCFLAG_KDSINIT) && type == 0x02) {
		return;
	}
	if (fp && (dbcflags & DBCFLAG_KDSINIT)) {
		adr1 += hl;
		switch(type) {
		case 0x00:
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				n = clientvidrestore(CLIENT_VID_CHAR, adr1, lp);
				if (n == -1) dbcerror(798);
			}
			else vidrestorechar(adr1, lp);
			return;
		case 0x01:
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				n = clientvidrestore(CLIENT_VID_WIN, adr1, lp);
				if (n == -1) dbcerror(798);
			}
			else n = vidrestorewin(adr1, lp);
			break;
		case 0x02:
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				n = clientvidrestore(CLIENT_VID_STATE, adr1, lp);
				if (n == -1) dbcerror(798);
			}
			else n = vidrestorestate(adr1, lp);
			break;
		case 0x03:
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				n = clientvidrestore(CLIENT_VID_SCRN, adr1, lp);
				if (n == -1) dbcerror(798);
			}
			else n = vidrestorescreen(adr1, lp);
			break;
		default:
			dbcerrinfo(1798, (UCHAR*)"Invalid 'type' in vscrnrest", -1);
		}
	}
	else n = type;
	if (n) dbcerror(506);
}

/**
 * Return the value from the nvar or constant at pgm[pcount]
 */
static INT getvalue()
{
	INT i1;
	UCHAR c1;

	if (pgm[pcount] >= 0xFD) {
		c1 = pgm[pcount++];
		if (c1 == 0xFD) i1 = (USHORT) gethhll();
		else {
			if (c1 == 0xFE) i1 = (USHORT)(0 - (INT) pgm[pcount]);
			else i1 = pgm[pcount];
			pcount++;
		}
		return(i1);
	}
	return(nvtoi(getvar(VAR_READ)));
}

/* getcolor returns the color set by DBC_COLOR and DBC_BGCOLOR */
static INT getcolor(CHAR *str, UINT *color)
{
	static CHAR *scolor[] = {
		"*black", "*blue", "*green", "*cyan",
		"*red", "*magenta", "*yellow", "*white"
	};
	INT i1;
#ifndef HPUX11
	UINT ncolor[] = {
		VID_BLACK, VID_BLUE, VID_GREEN, VID_CYAN,
		VID_RED, VID_MAGENTA, VID_YELLOW, VID_WHITE
	};
#else
	UINT ncolor[8];
	ncolor[0] = VID_BLACK;
	ncolor[1] = VID_BLUE;
	ncolor[2] = VID_GREEN;
	ncolor[3] = VID_CYAN;
	ncolor[4] = VID_RED;
	ncolor[5] = VID_MAGENTA;
	ncolor[6] = VID_YELLOW;
	ncolor[7] = VID_WHITE;
#endif

	for (i1 = 0; i1 < 8 && strcmp(str, scolor[i1]); i1++);
	if (i1 == 8) return RC_ERROR;
	*color = ncolor[i1];
	return(0);
}

void vbeep()
{
	INT32 vidcmd[2];
	if (fpicnt) filepi0();

	if (dbcflags & DBCFLAG_SERVICE) return;
	if (dbcflags & DBCFLAG_GUIINIT) {
		guibeep(0x0384, 5, TRUE);
		return;
	}
	if (!(dbcflags & DBCFLAG_CLIENTINIT)) {
		if (!(dbcflags & DBCFLAG_KDSINIT) && kdsinit()) return;
	}
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		clientput("<d><b/></d><s/>", 15);
		clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL | CLIENTSEND_RELEASE, 0);
	}
	else if (!(kydsflags & KYDS_PIPE)) {
		vidcmd[0] = VID_BEEP | 5L;
		vidcmd[1] = VID_END_FLUSH;
		if (vidput(vidcmd) < 0) dbcerror(505);
	}
}

void kdscmd(INT32 cmd)
{
	INT32 vidcmd[2];

	if ((dbcflags & DBCFLAG_SERVICE) || (!(dbcflags & DBCFLAG_KDSINIT) && kdsinit())) return;
	vidcmd[0] = cmd;
	vidcmd[1] = VID_END_FLUSH;
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		clientput("<d>", 3);
		clientvidput(vidcmd, CLIENTVIDPUT_SAVE | CLIENTVIDPUT_SEND);
		clientput("</d><s/>", 8);
		clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL | CLIENTSEND_RELEASE, 0);
	}
	else if (!(kydsflags & KYDS_PIPE)) {
		if (vidput(vidcmd) < 0) dbcerror(505);
	}
}

void kdssound(INT p, INT n)
{
	INT32 vidcmd[3];
	CHAR work[8];

	if (fpicnt) filepi0();
	if (dbcflags & DBCFLAG_SERVICE) return;
	if (dbcflags & DBCFLAG_GUIINIT) {
		guibeep(p, n, FALSE);
		return;
	}
	if (!(dbcflags & DBCFLAG_CLIENTINIT)) {
		if (!(dbcflags & DBCFLAG_KDSINIT) && kdsinit()) return;
	}
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		clientput("<sound i=", -1);
		mscitoa(p, work);
		clientput(work, -1);
		clientput(" t=", -1);
		mscitoa(n, work);
		clientput(work, -1);
		clientput("/><s/>", -1);
		clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL | CLIENTSEND_RELEASE, 0);
	}
	else if (!(kydsflags & KYDS_PIPE)) {
		vidcmd[0] = VID_SOUND;
		vidcmd[1] = (((UINT) p << 16) | n);
		vidcmd[2] = VID_END_FLUSH;
		if (vidput(vidcmd) < 0) dbcerror(505);
	}
}

void kdsdsp(CHAR *str)
{
	int cmdcnt;
	INT32 vidcmd[8];

	if ((dbcflags & DBCFLAG_SERVICE) || (!(dbcflags & DBCFLAG_KDSINIT) && kdsinit())) return;
	cmdcnt = 0;
	vidcmd[cmdcnt++] = VID_DISPLAY | (INT32)strlen(str);
	if (sizeof(void *) > sizeof(INT32)) {
		memcpy((void *) &vidcmd[cmdcnt], (void *) &str, sizeof(void *));
		cmdcnt += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
	}
	else *(UCHAR **)(&vidcmd[cmdcnt++]) = (UCHAR *) str;
	vidcmd[cmdcnt] = VID_END_FLUSH;
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		clientput("<d>", 3);
		clientvidput(vidcmd, CLIENTVIDPUT_SAVE | CLIENTVIDPUT_SEND);
		clientput("</d><s/>", 8);
		clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL | CLIENTSEND_RELEASE, 0);
	}
	else if (!(kydsflags & KYDS_PIPE)) {
		if (vidput(vidcmd) < 0) dbcerror(505);
	}
}

INT kdspause(INT secs)
{
	INT i1, eventid, timeid;
	UCHAR work[17];

	msctimestamp(work);
	timadd(work, secs * 100);
	if ((eventid = evtcreate()) == -1) return(0);
	timeid = timset(work, eventid);
	i1 = dbcwait(&eventid, 1);
	timstop(timeid);
	evtdestroy(eventid);
	return(i1);
}
