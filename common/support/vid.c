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
#define INC_LIMITS
#define INC_SIGNAL
#define INC_ERRNO
#include "includes.h"
#include "base.h"
#include "evt.h"
#include "fio.h"
#include "tim.h"
#define VIDMAIN
#include "vid.h"
#include "vidx.h"

#define KBDBUFMAX		256
#define DSPATTRSTATESAVEOFFSET 20

/* cursor mode */
#define CURSMODE_NORMAL	(UCHAR) 0
#define CURSMODE_OFF	(UCHAR) 1
#define CURSMODE_ON		(UCHAR) 2

static INT vidh, vidv;			/* absolute current screen position */
static INT maxlines, maxcolumns;	/* maximum allowable lines and columns */
static INT interchartime;		/* tenths of a second to wait after the 1st char of a multi char sequence, unix only */

static UCHAR vidcurmode = CURSMODE_NORMAL;
static UCHAR vidcurtype = CURSOR_BLOCK;

static UCHAR viddblflag;		/* horizontal = 0x01, vertical = 0x02, lines are double in graphics characters */

static INT cancelkey1, cancelkey2;	/* Cancel key and alternate cancel key */
static INT intrchr;				/* Interrupt key */
static UCHAR validmap[(VID_MAXKEYVAL >> 3) + 1];  /* map of valid characters */
static UCHAR finishmap[(VID_MAXKEYVAL >> 3) + 1];  /* map of finish characters */
static UCHAR trapmap[(VID_MAXKEYVAL >> 3) + 1];
static UCHAR xkeymap[UCHAR_MAX + 1];	/* key translate map for chars 0-255 */
static UCHAR lkeymap[UCHAR_MAX + 1];	/* lower case translation map */
static UCHAR ukeymap[UCHAR_MAX + 1];	/* upper case translation map */
static INT trapstate;			/* 0 = no traps active, 1 = active, 2 = trap char found */
static INT trapeventid;			/* eventid to set when trap char is found */
static INT trapchar;			/* value of trapped character */
static INT traphead;			/* position to set kbdhead when trap char is retrieved */
static INT kbdbuffer[KBDBUFMAX];
static INT kbdtail, kbdhead;
static INT keyinstate;			/* 0 = keyin not active, 1 = active, 2 = complete, 3 = canceled */
static INT keyineventid;			/* eventid to set when keyin is complete */
static UCHAR keyinfld256[256];	/* initial keyin field */
static UCHAR *keyinfield = keyinfld256;  /* keyin field */
static INT keyinfldsize = sizeof(keyinfld256);  /* initial size of keyin field */
static INT keyindsplen;			/* screen display width for keyin */
static INT keyinfldlen;			/* field width for keyin */
static INT keyinflags;			/* keyin entry flag */
static INT keyinfinishchar;		/* actual finish character, 0 = autoenter, -1 = timeout */
static INT keyintot;			/* total characters present in keyin field */
static INT keyinpos;			/* current cursor position in keyin field */
static INT keyinoff;			/* first visible character pos in scrolling keyin field */
static INT keyinhome, keyinend;
static INT keyinleft, keyinright;	/* left and right lens of keyin numeric var */
static INT keyindotpos;			/* position of '.' in an ne keyin */
static INT keyinechochar;		/* echo secret character */
static INT keyintimeout;			/* keyin timeout */
static INT keyintimerhandle;		/* handle of timeout timer */

static INT kynattr;				/* keyin attributes */

#define KYNATTR_EDIT			0x00000001
#define KYNATTR_EDITON			0x00000002
#define KYNATTR_EDITMODE		0x00000004
#define KYNATTR_EDITOVERSTRIKE	0x00000008
#define KYNATTR_EDITNUM			0x00000010
#define KYNATTR_EDITFIRSTNUM	0x00000020
#define KYNATTR_EDITSPECIAL		0x00000040
#define KYNATTR_DIGITENTRY		0x00000080
#define KYNATTR_JUSTRIGHT		0x00000100
#define KYNATTR_JUSTLEFT		0x00000200
#define KYNATTR_ZEROFILL 		0x00000400
#define KYNATTR_AUTOENTER		0x00000800
#define KYNATTR_RIGHTTOLEFT		0x00001000
#define KYNATTR_CLICK			0x00002000
#define KYNATTR_NOECHO			0x00004000
#define KYNATTR_ECHOSECRET		0x00008000
#define KYNATTR_KEYCASENORMAL	0x01000000
#define KYNATTR_KEYCASEUPPER	0x02000000
#define KYNATTR_KEYCASELOWER	0x04000000
#define KYNATTR_KEYCASEINVERT	0x08000000
#define KYNATTR_KEYCASEMASK		0x0F000000

static CHAR viderrorstring[256];
static void vidtrapcheck(void);
static INT vidcharcallback(INT);
static void videchochar(INT);
static void videchostring(UCHAR *, INT);
static void vidkeyinprocess(void);

INT vidinit(VIDPARMS *vidparms, INT breakeventid)
{
#if OS_UNIX
	INT flag = VID_FLAG_USEMOUSE;
#else
	INT flag = 0;
#endif
	INT i1;
	CHAR termdeffilename[MAX_NAMESIZE];
	CHAR indevice[MAX_NAMESIZE], outdevice[MAX_NAMESIZE];
	CHAR fontname[100], *ptr;
 
	*viderrorstring = '\0';
	termdeffilename[0] = '\0';
	indevice[0] = '\0';
	outdevice[0] = '\0';
	fontname[0] = '\0';
	for (i1 = 0; i1 <= UCHAR_MAX; i1++) xkeymap[i1] = (UCHAR) i1;
	memcpy(ukeymap, xkeymap, 256);
	memcpy(lkeymap, xkeymap, 256);
	for (i1 = 'A'; i1 <= 'Z'; i1++) {
		ukeymap[tolower(i1)] = (UCHAR) i1;
		lkeymap[i1] = (UCHAR) tolower(i1);
	}
	if (vidparms == NULL) {
		if (!prpget("kydspipe", NULL, NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) flag |= VID_FLAG_KYDSPIPE;
		if (!prpget("keyin", "editmode", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) flag |= VID_FLAG_EDITMODE;
		if (!prpget("keyin", "edit_special", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) flag |= VID_FLAG_EDITSPECIAL;
#if OS_WIN32
		if (!prpget("beep", NULL, NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) flag |= VID_FLAG_OLDBEEP;
		if (!prpget("keyin", "ikeys", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) flag |= VID_FLAG_IKEYS;
		if (!prpget("keyin", "handshake", NULL, NULL, &ptr, PRP_LOWER)){
			if (!strcmp(ptr, "on")) flag |= VID_FLAG_SHAKEON;
			else if (!strcmp(ptr, "cts")) flag |= VID_FLAG_CTSRTS;
		}
		if (!prpget("display", "newcon", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "off")) flag |= VID_FLAG_NOCON;
		if (!prpget("display", "console", "closebutton", NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "off")) flag |= VID_FLAG_NOCLOSE;
#endif
#if OS_UNIX
		if (!prpget("keyin", "xon", NULL, NULL, &ptr, PRP_LOWER)) {
			if (!strcmp(ptr, "off")) flag |= VID_FLAG_XOFF;
			if (!strcmp(ptr, "on")) flag |= VID_FLAG_XON;
		}
#endif
		kdsConfigColorMode(&flag);
		if (!prpget("display", "columns", NULL, NULL, &ptr, 0)) maxcolumns = atoi(ptr);
		if (!prpget("display", "lines", NULL, NULL, &ptr, 0)) maxlines = atoi(ptr);
		if (!prpget("display", "termdef", NULL, NULL, &ptr, 0)) strcpy(termdeffilename, ptr);
		if (!prpget("display", "device", NULL, NULL, &ptr, 0)) strcpy(outdevice, ptr);
		if (!prpget("display", "font", NULL, NULL, &ptr, 0)) strcpy(fontname, ptr);
		if (!prpget("keyin", "ict", NULL, NULL, &ptr, 0)) interchartime = atoi(ptr);
		if (!prpget("keyin", "device", NULL, NULL, &ptr, 0)) strcpy(indevice, ptr);
		if (!prpget("keyin", "translate", NULL, NULL, &ptr, 0)) {
			if (prptranslate(ptr, xkeymap)) {
				vidputerror("invalid keyin.translate value", VIDPUTERROR_NEW);
				return -1;
			}
		}
		if (!prpget("keyin", "uppertranslate", NULL, NULL, &ptr, 0)) {
			if (prptranslate(ptr, ukeymap)) {
				vidputerror("invalid keyin.uppertranslate value", VIDPUTERROR_NEW);
				return -1;
			}
		}
		if (!prpget("keyin", "lowertranslate", NULL, NULL, &ptr, 0)) {
			if (prptranslate(ptr, lkeymap)) {
				vidputerror("invalid keyin.lowertranslate value", VIDPUTERROR_NEW);
				return -1;
			}
		}
	}
	else {
		flag = vidparms->flag;
		maxcolumns = vidparms->numcolumns;
		maxlines = vidparms->numlines;
		interchartime = vidparms->interchartime;
		if (vidparms->termdef != NULL) strcpy(termdeffilename, vidparms->termdef);
		if (vidparms->indevice != NULL) strcpy(indevice, vidparms->indevice);
		if (vidparms->outdevice != NULL) strcpy(outdevice, vidparms->outdevice);
		if (vidparms->fontname != NULL) strcpy(fontname, vidparms->fontname);
/*** CODE: NEED ->DSPMAP ***/
	 	if (vidparms->keymap != NULL) memcpy(xkeymap, vidparms->keymap, sizeof(xkeymap));
 		if (vidparms->ukeymap != NULL) memcpy(ukeymap, vidparms->ukeymap, sizeof(ukeymap));
 		if (vidparms->lkeymap != NULL) memcpy(lkeymap, vidparms->lkeymap, sizeof(lkeymap));
	}

	dspattr = DSPATTR_ROLL + (DSPATTR_WHITE << DSPATTR_FGCOLORSHIFT);
	kynattr = KYNATTR_KEYCASENORMAL;

	keyintimeout = -1;
	keyinechochar = (INT) '*';

	if (flag & VID_FLAG_KYDSPIPE) dspattr |= DSPATTR_KYDSPIPE;
	if (flag & VID_FLAG_EDITMODE) kynattr |= KYNATTR_EDITMODE;
	if (flag & VID_FLAG_EDITSPECIAL) kynattr |= KYNATTR_EDITSPECIAL;

	if ( (i1 = vidastart(flag, breakeventid, termdeffilename, indevice,
			outdevice, fontname, &maxcolumns, &maxlines,
		vidcharcallback, interchartime)) ) return i1;

	wscbot = maxlines - 1;
	wscrgt = maxcolumns - 1;
	wsclft = wsctop = 0;

	if (flag & VID_FLAG_COLORMODE_ANSI256) {
		VID_BLACK	= 0x0000;
		VID_RED		= 0x0001;
		VID_GREEN	= 0x0002;
		VID_YELLOW	= 0x0003;
		VID_BLUE	= 0x0004;
		VID_MAGENTA	= 0x0005;
		VID_CYAN	= 0x0006;
		VID_WHITE	= 0x0007;
	}
	else {
		VID_BLACK	= 0x0000;
		VID_BLUE	= 0x0001;
		VID_GREEN	= 0x0002;
		VID_CYAN	= 0x0003;
		VID_RED		= 0x0004;
		VID_MAGENTA	= 0x0005;
		VID_YELLOW	= 0x0006;
		VID_WHITE	= 0x0007;
	}

	vidacursor(CURSOR_NONE, dspattr);
	vidreset();
	return 0;
}

void videxit()
{
	INT32 vidcmd[10];
	INT cmdcnt;

	vidreset();
	vidcmd[0] = VID_WIN_RESET;
	vidcmd[1] = VID_FOREGROUND | VID_WHITE;
	vidcmd[2] = VID_BACKGROUND | VID_BLACK;
	vidcmd[3] = VID_HD;
	vidcmd[4] = VID_EL;
	cmdcnt = 5;
#if OS_WIN32
	vidcmd[cmdcnt++] = VID_VERT_ADJ | (USHORT) -1;
#endif
	vidcmd[cmdcnt++] = VID_CURSOR_ULINE;
	vidcmd[cmdcnt++] = VID_CURSOR_ON;
	vidcmd[cmdcnt] = VID_FINISH;
	vidput(vidcmd);
	vidaexit();
}

void vidsuspend(INT cursorflg)
{
	if (cursorflg) vidacursor(CURSOR_ULINE, dspattr);
	vidasuspend(dspattr);
}

void vidresume()
{
	vidaresume();
	vidaposition(vidh, vidv, dspattr);
	if (vidcurmode == CURSMODE_ON) vidacursor(vidcurtype, dspattr);
	else vidacursor(CURSOR_NONE, dspattr);
}

INT vidput(INT32 *cmd)
{
	INT i1, i2, i3, h, v, repeatcnt;
	INT32 *repeatcmd;
	UCHAR *ptr;

	repeatcnt = 0;

	for ( ; ; ) {
		switch(((*cmd) >> 16)) {
		case (VID_FINISH >> 16):
			kynattr &= ~(KYNATTR_DIGITENTRY | KYNATTR_JUSTRIGHT | KYNATTR_JUSTLEFT |
				KYNATTR_ZEROFILL | KYNATTR_EDIT | KYNATTR_RIGHTTOLEFT |
				KYNATTR_NOECHO | KYNATTR_ECHOSECRET | KYNATTR_AUTOENTER);
			keyintimeout = -1;
			if (keyintimerhandle > 0) timstop(keyintimerhandle);
			keyintimerhandle = 0;
			/* fall through */
		case (VID_END_FLUSH >> 16):
			vidaflush();
			/* fall through */
		case (VID_END_NOFLUSH >> 16):
			return 0;

		case (VID_DISPLAY >> 16):
			i1 = *cmd++ & 0x0000FFFF;
			if (sizeof(CHAR *) > sizeof(INT32)) {
				memcpy((UCHAR *) &ptr, (UCHAR *) cmd, sizeof(UCHAR *));
				cmd += ((sizeof(UCHAR *) + sizeof(INT32) - 1) / sizeof(INT32)) - 1;
			}
			else ptr = (UCHAR *) (ULONG_PTR) *cmd;
			if (i1) {
				if (!(dspattr & (DSPATTR_RAWMODE | DSPATTR_AUXPORT | DSPATTR_KYDSPIPE))) {
					if (!(dspattr & DSPATTR_DISPDOWN)) {
						if (!(dspattr & DSPATTR_DISPLEFT)) i2 = wscrgt - vidh + 1;
						else i2 = vidh - wsclft + 1;
					}
					else if (!(dspattr & DSPATTR_DISPLEFT)) i2 = wscbot - vidv + 1;
					else i2 = vidv - wsctop + 1;
					if (i1 > i2) i1 = i2;
				}
				vidaputstring(ptr, i1, dspattr);
				if (!(dspattr & (DSPATTR_RAWMODE | DSPATTR_AUXPORT | DSPATTR_KYDSPIPE))) {
					if ( (i2 = (i1 == i2)) ) i1--;
					if (!(dspattr & DSPATTR_DISPDOWN)) {
						if (!(dspattr & DSPATTR_DISPLEFT)) vidh += i1;
						else vidh -= i1;
					}
					else if (!(dspattr & DSPATTR_DISPLEFT)) vidv += i1;
					else vidv -= i1;
					if (i2) vidaposition(vidh, vidv, dspattr);
				}
			}
			break;

		case (VID_REPEAT >> 16):
			repeatcnt = *cmd++ & 0xFFFF;
			if (repeatcnt) repeatcmd = cmd;
			break;
		case (VID_DISP_CHAR >> 16):
			i1 = repeatcnt + 1;
			repeatcnt = 0;
			if (!(dspattr & (DSPATTR_RAWMODE | DSPATTR_AUXPORT | DSPATTR_KYDSPIPE))) {
				if (!(dspattr & DSPATTR_DISPDOWN)) {
					if (!(dspattr & DSPATTR_DISPLEFT)) i2 = wscrgt - vidh + 1;
					else i2 = vidh - wsclft + 1;
				}
				else if (!(dspattr & DSPATTR_DISPLEFT)) i2 = wscbot - vidv + 1;
				else i2 = vidv - wsctop + 1;
				if (i1 > i2) i1 = i2;
			}
			vidaputstring(NULL, i1, dspattr + (UCHAR) *cmd);
			if (!(dspattr & (DSPATTR_RAWMODE | DSPATTR_AUXPORT | DSPATTR_KYDSPIPE))) {
				if ( (i2 = (i1 == i2)) ) i1--;
				if (!(dspattr & DSPATTR_DISPDOWN)) {
					if (!(dspattr & DSPATTR_DISPLEFT)) vidh += i1;
					else vidh -= i1;
				}
				else if (!(dspattr & DSPATTR_DISPLEFT)) vidv += i1;
				else vidv -= i1;
				if (i2) vidaposition(vidh, vidv, dspattr);
			}
			break;

		case (VID_DISP_SYM >> 16):
			i1 = repeatcnt + 1;
			repeatcnt = 0;
			if (!(dspattr & (DSPATTR_RAWMODE | DSPATTR_AUXPORT | DSPATTR_KYDSPIPE))) {
				if (!(dspattr & DSPATTR_DISPDOWN)) {
					if (!(dspattr & DSPATTR_DISPLEFT)) i2 = wscrgt - vidh + 1;
					else i2 = vidh - wsclft + 1;
				}
				else if (!(dspattr & DSPATTR_DISPLEFT)) i2 = wscbot - vidv + 1;
				else i2 = vidv - wsctop + 1;
				if (i1 > i2) i1 = i2;
			}
			i3 = (UCHAR) *cmd | (viddblflag << 6);
			vidaputstring(NULL, i1, i3 + dspattr + DSPATTR_GRAPHIC);
			if (!(dspattr & (DSPATTR_RAWMODE | DSPATTR_AUXPORT | DSPATTR_KYDSPIPE))) {
				if ( (i2 = (i1 == i2)) ) i1--;
				if (!(dspattr & DSPATTR_DISPDOWN)) {
					if (!(dspattr & DSPATTR_DISPLEFT)) vidh += i1;
					else vidh -= i1;
				}
				else if (!(dspattr & DSPATTR_DISPLEFT)) vidv += i1;
				else vidv -= i1;
				if (i2) vidaposition(vidh, vidv, dspattr);
			}
			break;
		case (VID_DSP_BLANKS >> 16):
			i1 = *cmd & 0xFFFF;
			if (i1) {
				if (!(dspattr & (DSPATTR_RAWMODE | DSPATTR_AUXPORT | DSPATTR_KYDSPIPE))) {
					if (!(dspattr & DSPATTR_DISPDOWN)) {
						if (!(dspattr & DSPATTR_DISPLEFT)) i2 = wscrgt - vidh + 1;
						else i2 = vidh - wsclft + 1;
					}
					else if (!(dspattr & DSPATTR_DISPLEFT)) i2 = wscbot - vidv + 1;
					else i2 = vidv - wsctop + 1;
					if (i1 > i2) i1 = i2;
				}
				vidaputstring(NULL, i1, dspattr + ' ');
				if (!(dspattr & (DSPATTR_RAWMODE | DSPATTR_AUXPORT | DSPATTR_KYDSPIPE))) {
					if ( (i2 = (i1 == i2)) ) i1--;
					if (!(dspattr & DSPATTR_DISPDOWN)) {
						if (!(dspattr & DSPATTR_DISPLEFT)) vidh += i1;
						else vidh -= i1;
					}
					else if (!(dspattr & DSPATTR_DISPLEFT)) vidv += i1;
					else vidv -= i1;
					if (i2) vidaposition(vidh, vidv, dspattr);
				}
			}
			break;
		case (VID_HORZ >> 16):
			i1 = wsclft + (*cmd & 0xFFFF);
			if (i1 <= wscrgt) {
				vidh = i1;
				if ((*(cmd + 1) >> 16) == (VID_VERT >> 16)) {
					i1 = wsctop + (*++cmd & 0xFFFF);
					if (i1 <= wscbot) vidv = i1;
				}
				vidaposition(vidh, vidv, dspattr);
			}
			break;
		case (VID_VERT >> 16):
			i1 = wsctop + (*cmd & 0xFFFF);
			if (i1 <= wscbot) {
				vidv = i1;
				vidaposition(vidh, vidv, dspattr);
			}
			break;
		case (VID_HORZ_ADJ >> 16):
			i1 = vidh + (SHORT)(*cmd & 0xFFFF);
			if (i1 >= wsclft && i1 <= wscrgt) {
				vidh = i1;
				vidaposition(vidh, vidv, dspattr);
			}
			break;
		case (VID_VERT_ADJ >> 16):
			i1 = vidv + (SHORT)(*cmd & 0xFFFF);
			if (i1 >= wsctop && i1 <= wscbot) {
				vidv = i1;
				vidaposition(vidh, vidv, dspattr);
			}
			break;
		case (VID_WIN_TOP >> 16):
			i1 = (*cmd & 0xFFFF);
			if (i1 >= 0 && i1 < maxlines) {
				wsctop = i1;
				if (wsctop > wscbot) wscbot = wsctop;
			}
			break;
		case (VID_WIN_BOTTOM >> 16):
			i1 = (*cmd & 0xFFFF);
			if (i1 >= 0 && i1 < maxlines) {
				wscbot = i1;
				if (wscbot < wsctop) wsctop = wscbot;
			}
			break;
		case (VID_WIN_LEFT >> 16):
			i1 = (*cmd & 0xFFFF);
			if (i1 >= 0 && i1 < maxcolumns) {
				wsclft = i1;
				if (wsclft > wscrgt) wscrgt = wsclft;
			}
			break;
		case (VID_WIN_RIGHT >> 16):
			i1 = (*cmd & 0xFFFF);
			if (i1 >= 0 && i1 < maxcolumns) {
				wscrgt = i1;
				if (wscrgt < wsclft) wsclft = wscrgt;
			}
			break;
		case (VID_WIN_RESET >> 16):
			wsclft = wsctop = 0;
			wscbot = maxlines - 1;
			wscrgt = maxcolumns - 1;
			break;
		case (VID_REV_ON >> 16):
			dspattr |= DSPATTR_REV;
			break;
		case (VID_REV_OFF >> 16):
			dspattr &= ~DSPATTR_REV;
			break;
		case (VID_UL_ON >> 16):
			dspattr |= DSPATTR_UNDERLINE;
			break;
		case (VID_UL_OFF >> 16):
			dspattr &= ~DSPATTR_UNDERLINE;
			break;
		case (VID_BLINK_ON >> 16):
			dspattr |= DSPATTR_BLINK;
			break;
		case (VID_BLINK_OFF >> 16):
			dspattr &= ~DSPATTR_BLINK;
			break;
		case (VID_BOLD_ON >> 16):
			dspattr |= DSPATTR_BOLD;
			break;
		case (VID_BOLD_OFF >> 16):
			dspattr &= ~DSPATTR_BOLD;
			break;
		case (VID_DIM_ON >> 16):
			dspattr |= DSPATTR_DIM;
			break;
		case (VID_DIM_OFF >> 16):
			dspattr &= ~DSPATTR_DIM;
			break;
		case (VID_RL_ON >> 16):
			dspattr |= DSPATTR_DISPLEFT;
			break;
		case (VID_RL_OFF >> 16):
			dspattr &= ~DSPATTR_DISPLEFT;
			break;
		case (VID_DSPDOWN_ON >> 16):
			dspattr |= DSPATTR_DISPDOWN;
			break;
		case (VID_DSPDOWN_OFF >> 16):
			dspattr &= ~DSPATTR_DISPDOWN;
			break;
		case (VID_NL_ROLL_ON >> 16):
			dspattr |= DSPATTR_ROLL;
			break;
		case (VID_NL_ROLL_OFF >> 16):
			dspattr &= ~DSPATTR_ROLL;
			break;
		case (VID_RAW_ON >> 16):
			dspattr |= DSPATTR_RAWMODE;
			break;
		case (VID_RAW_OFF >> 16):
			dspattr &= ~DSPATTR_RAWMODE;
			break;
		case (VID_PRT_ON >> 16):
			dspattr |= DSPATTR_AUXPORT;
			break;
		case (VID_PRT_OFF >> 16):
			dspattr &= ~DSPATTR_AUXPORT;
			break;
		case (VID_CURSOR_ON >> 16):
			vidacursor(vidcurtype, dspattr);
			vidcurmode = CURSMODE_ON;
			break;
		case (VID_CURSOR_OFF >> 16):
			vidacursor(CURSOR_NONE, dspattr);
			vidcurmode = CURSMODE_OFF;
			break;
		case (VID_CURSOR_NORM >> 16):
			if (vidcurmode == CURSMODE_ON && !keyinstate) vidacursor(CURSOR_NONE, dspattr);
			vidcurmode = CURSMODE_NORMAL;
			break;
		case (VID_CURSOR_ULINE >> 16):
			vidcurtype = CURSOR_ULINE;
			if (vidcurmode == CURSMODE_ON || keyinstate) vidacursor(vidcurtype, dspattr);
			break;
		case (VID_CURSOR_HALF >> 16):
			vidcurtype = CURSOR_HALF;
			if (vidcurmode == CURSMODE_ON || keyinstate) vidacursor(vidcurtype, dspattr);
			break;
		case (VID_CURSOR_BLOCK >> 16):
			vidcurtype = CURSOR_BLOCK;
			if (vidcurmode == CURSMODE_ON || keyinstate) vidacursor(vidcurtype, dspattr);
			break;
		case (VID_HDBL_ON >> 16):
			viddblflag |= 0x01;
			break;
		case (VID_HDBL_OFF >> 16):
			viddblflag &= ~0x01;
			break;
		case (VID_VDBL_ON >> 16):
			viddblflag |= 0x02;
			break;
		case (VID_VDBL_OFF >> 16):
			viddblflag &= ~0x02;
			break;
		case (VID_KBD_RESET >> 16):
			kynattr &= ~(KYNATTR_DIGITENTRY | KYNATTR_JUSTRIGHT | KYNATTR_JUSTLEFT |
				KYNATTR_ZEROFILL | KYNATTR_EDIT);
			break;
		case (VID_KBD_DE >> 16):
			kynattr |= KYNATTR_DIGITENTRY;
			break;
		case (VID_KBD_JR >> 16):
			kynattr &= ~KYNATTR_JUSTLEFT;
			kynattr |= KYNATTR_JUSTRIGHT;
			break;
		case (VID_KBD_JL >> 16):
			kynattr &= ~KYNATTR_JUSTRIGHT;
			kynattr |= KYNATTR_JUSTLEFT;
			break;
		case (VID_KBD_ZF >> 16):
			kynattr |= KYNATTR_ZEROFILL;
			break;
		case (VID_KBD_EDIT >> 16):
			kynattr |= KYNATTR_EDIT;
			break;
		case (VID_KBD_UC >> 16):
			if (!(kynattr & KYNATTR_KEYCASEUPPER)) {
				kynattr &= ~KYNATTR_KEYCASEMASK;
				kynattr |= KYNATTR_KEYCASEUPPER;
#if OS_WIN32
				pvistart();
#endif
				vidtrapcheck();
#if OS_WIN32
				pviend();
#endif
			}
			break;
		case (VID_KBD_LC >> 16):
			if (!(kynattr & KYNATTR_KEYCASELOWER)) {
			kynattr &= ~KYNATTR_KEYCASEMASK;
				kynattr |= KYNATTR_KEYCASELOWER;
#if OS_WIN32
				pvistart();
#endif
				vidtrapcheck();
#if OS_WIN32
				pviend();
#endif
			}
			break;
		case (VID_KBD_IC >> 16):
			if (!(kynattr & KYNATTR_KEYCASEINVERT)) {
				kynattr &= ~KYNATTR_KEYCASEMASK;
				kynattr |= KYNATTR_KEYCASEINVERT;
#if OS_WIN32
				pvistart();
#endif
				vidtrapcheck();
#if OS_WIN32
				pviend();
#endif
			}
			break;
		case (VID_KBD_IN >> 16):
			if (!(kynattr & KYNATTR_KEYCASENORMAL)) {
				kynattr &= ~KYNATTR_KEYCASEMASK;
				kynattr |= KYNATTR_KEYCASENORMAL;
#if OS_WIN32
				pvistart();
#endif
				vidtrapcheck();
#if OS_WIN32
				pviend();
#endif
			}
			break;
		case (VID_KBD_ESON >> 16):
			kynattr |= KYNATTR_ECHOSECRET;
			break;
		case (VID_KBD_ESOFF >> 16):
			kynattr &= ~KYNATTR_ECHOSECRET;
			break;
		case (VID_KBD_EON >> 16):
			kynattr &= ~KYNATTR_NOECHO;
			break;
		case (VID_KBD_EOFF >> 16):
			kynattr |= KYNATTR_NOECHO;
			break;
		case (VID_KBD_KCON >> 16):
			kynattr |= KYNATTR_AUTOENTER;
			break;
		case (VID_KBD_KCOFF >> 16):
			kynattr &= ~KYNATTR_AUTOENTER;
			break;
		case (VID_KBD_ED_ON >> 16):
			kynattr |= KYNATTR_EDITON;
			break;
		case (VID_KBD_ED_OFF >> 16):
			kynattr &= ~KYNATTR_EDITON;
			break;
		case (VID_KBD_CK_ON >> 16):
			kynattr |= KYNATTR_CLICK;
			break;
		case (VID_KBD_CK_OFF >> 16):
			kynattr &= ~KYNATTR_CLICK;
			break;
		case (VID_KBD_OVS_ON >> 16):
			kynattr |= KYNATTR_EDITOVERSTRIKE;
			break;
		case (VID_KBD_OVS_OFF >> 16):
			kynattr &= ~KYNATTR_EDITOVERSTRIKE;
			break;
		case (VID_KBD_ECHOCHR >> 16):
			keyinechochar = *cmd & 0xFFFF;
			break;
		case (VID_KBD_TIMEOUT >> 16):
			keyintimeout = *cmd & 0xFFFF;
			if (keyintimeout == 0xFFFF) keyintimeout = -1;
			break;
		case (VID_KBD_F_CNCL1 >> 16):
			cancelkey1 = *cmd & 0xFFFF;
			break;
		case (VID_KBD_F_CNCL2 >> 16):
			cancelkey2 = *cmd & 0xFFFF;
			break;
		case (VID_KBD_F_INTR >> 16):
			intrchr = *cmd & 0xFFFF;
			break;
		case (VID_ES >> 16):
			vidv = wsctop;
			vidh = wsclft;
			vidaeraserect(wsctop, wscbot, wsclft, wscrgt, dspattr);
			vidaposition(vidh, vidv, dspattr);
			break;
		case (VID_EF >> 16):
			if (vidh == wsclft) vidaeraserect(vidv, wscbot, wsclft, wscrgt, dspattr);
			else {
				vidaeraserect(vidv, vidv, vidh, wscrgt, dspattr);
				if (vidv < wscbot) vidaeraserect(vidv + 1, wscbot, wsclft, wscrgt, dspattr);
			}
			break;
		case (VID_EL >> 16):
			vidaeraserect(vidv, vidv, vidh, wscrgt, dspattr);
			break;
		case (VID_RU >> 16):
			vidamove(ROLL_UP, wsctop, wscbot, wsclft, wscrgt, dspattr);
			break;
		case (VID_RD >> 16):
			vidamove(ROLL_DOWN, wsctop, wscbot, wsclft, wscrgt, dspattr);
			break;
		case (VID_RL >> 16):
			vidamove(ROLL_LEFT, wsctop, wscbot, wsclft, wscrgt, dspattr);
			break;
		case (VID_RR >> 16):
			vidamove(ROLL_RIGHT, wsctop, wscbot, wsclft, wscrgt, dspattr);
			break;
		case (VID_CR >> 16):
			if (!(dspattr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE))) {
				vidh = wsclft;
				vidaposition(vidh, vidv, dspattr);
			}
			else {
				vidaputstring(NULL, 1, dspattr + '\r');
				vidaflush();
			}
			break;
		case (VID_NL >> 16):
			if (!(dspattr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE))) {
				vidh = wsclft;
				if (vidv != wscbot) vidv++;
				else if (dspattr & DSPATTR_ROLL) vidamove(ROLL_UP, wsctop, wscbot, wsclft, wscrgt, dspattr);
				vidaposition(vidh, vidv, dspattr);
			}
			else {
				vidaputstring((UCHAR *) "\r\n", 2, dspattr);
				vidaflush();
			}
			break;
		case (VID_LF >> 16):
			if (!(dspattr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE))) {
				if (vidv != wscbot) vidv++;
				else if (dspattr & DSPATTR_ROLL) vidamove(ROLL_UP, wsctop, wscbot, wsclft, wscrgt, dspattr);
				vidaposition(vidh, vidv, dspattr);
			}
			else {
				vidaputstring(NULL, 1, dspattr + '\n');
				vidaflush();
			}
			break;
		case (VID_HU >> 16): /* home up */
			vidv = wsctop;
			vidh = wsclft;
			vidaposition(vidh, vidv, dspattr);
			break;
		case (VID_HD >> 16): /* home down */
			vidv = wscbot;
			vidh = wsclft;
			vidaposition(vidh, vidv, dspattr);
			break;
		case (VID_EU >> 16): /* end up */
			vidv = wsctop;
			vidh = wscrgt;
			vidaposition(vidh, vidv, dspattr);
			break;
		case (VID_ED >> 16): /* end down */
			vidv = wscbot;
			vidh = wscrgt;
			vidaposition(vidh, vidv, dspattr);
			break;
		case (VID_OL >> 16):
			vidamove(ROLL_DOWN, vidv + 1, wscbot, wsclft, wscrgt, dspattr);
			vidamove(ROLL_DOWN, vidv, vidv + 1, vidh, wscrgt, dspattr);
			break;
		case (VID_CL >> 16):
			vidamove(ROLL_UP, vidv, vidv + 1, vidh, wscrgt, dspattr);
			vidamove(ROLL_UP, vidv + 1, wscbot, wsclft, wscrgt, dspattr);
			break;
		case (VID_IL >> 16):
			vidamove(ROLL_DOWN, vidv, wscbot, wsclft, wscrgt, dspattr);
			break;
		case (VID_DL >> 16):
			vidamove(ROLL_UP, vidv, wscbot, wsclft, wscrgt, dspattr);
			break;
		case (VID_INSCHAR >> 16):
			h = wsclft + ((*++cmd >> 16) & 0xFFFF);
			v = wsctop + (*cmd & 0xFFFF);
			if (v >= vidv && (v > vidv || h > vidh) && v <= wscbot && h <= wscrgt) {
				vidainsertchar(h, v, wsctop, wscbot, wsclft, wscrgt, dspattr + ' ');
			}
			break;
		case (VID_DELCHAR >> 16):
			h = wsclft + ((*++cmd >> 16) & 0xFFFF);
			v = wsctop + (*cmd & 0xFFFF);
			if (v >= vidv && (v > vidv || h > vidh) && v <= wscbot && h <= wscrgt) {
				vidadeletechar(h, v, wsctop, wscbot, wsclft, wscrgt, dspattr + ' ');
			}
			break;
		case (VID_BEEP >> 16):
			vidasound(900, 5, dspattr, TRUE);
			break;
		case (VID_SOUND >> 16):
			cmd++;
			vidasound((*cmd >> 16) & 0xFFFF, *cmd & 0xFFFF, dspattr, FALSE);
			break;

		case (VID_FOREGROUND >> 16):
			dspattr &= ~DSPATTR_FGCOLORMASK;
#if OS_UNIX
			dspattr |= (((UINT64)*cmd) << DSPATTR_FGCOLORSHIFT) & DSPATTR_FGCOLORMASK;
#else
			dspattr |= (*cmd << DSPATTR_FGCOLORSHIFT) & DSPATTR_FGCOLORMASK;
#endif
			break;
		case (VID_BACKGROUND >> 16):
			dspattr &= ~DSPATTR_BGCOLORMASK;
#if OS_UNIX
			dspattr |= (((UINT64)*cmd) << DSPATTR_BGCOLORSHIFT) & DSPATTR_BGCOLORMASK;
#else
			dspattr |= (*cmd << DSPATTR_BGCOLORSHIFT) & DSPATTR_BGCOLORMASK;
#endif
			break;

		default:
			return RC_ERROR;
		}
		if (repeatcnt) {
			repeatcnt--;
			cmd = repeatcmd;
		}
		else cmd++;
	}
}

void vidreset()
{
	static INT32 vidcmd[] = {
		VID_REV_OFF,
		VID_UL_OFF,
		VID_BLINK_OFF,
		VID_BOLD_OFF,
		VID_DIM_OFF,
		VID_RAW_OFF,
		VID_PRT_OFF,
		VID_CURSOR_NORM,
		VID_CURSOR_BLOCK,
		VID_HDBL_OFF,
		VID_VDBL_OFF,
		VID_KBD_CK_OFF,
		VID_KBD_OVS_OFF,
		VID_KBD_ECHOCHR | '*',
		VID_FINISH
	};
	vidput(vidcmd);
}

/* get the physical screen size */
void vidgetsize(INT *h, INT *v)
{
	*h = maxcolumns;
	*v = maxlines;
}

/* get the current cursor position ( offset in window) */
void vidgetpos(INT *h, INT *v)
{
	*h = vidh - wsclft;
	*v = vidv - wsctop;
}

/* get the current cursor position ( offset in window) */
void vidgetwin(INT *top, INT *bottom, INT *left, INT *right)
{
	*top = wsctop;
	*bottom = wscbot;
	*left = wsclft;
	*right = wscrgt;
}

INT vidscreensize()
{
	return (maxcolumns * maxlines * CELLWIDTH) + sizeof(STATESAVE);
}

INT vidwinsize()
{
	return (wscrgt - wsclft + 1) * (wscbot - wsctop + 1) * CELLWIDTH;
}

INT vidstatesize()
{
	return sizeof(STATESAVE);
}

/**
 * memory Points to the 1st physical byte of a CHAR (DIM)
 * n is the length pointer (but is set to PL before we are called)
 */
INT vidsavechar(UCHAR *memory, INT *n)
{
	INT i1, i2;

	i2 = (wscrgt - wsclft + 1);
	i1 = i2 * (wscbot - wsctop + 1);
	if (*n >= i1) {
		*n = i1;
		vidagetrect(memory, wsctop, wscbot, wsclft, wscrgt, TRUE);
		return 0;
	}
	if (*n > 0) {
		i1 = *n / i2;
		i2 = *n - i1 * i2;
		if (i1) vidagetrect(memory, wsctop, wsctop + i1 - 1, wsclft, wscrgt, TRUE);
		if (i2) vidagetrect(memory, wsctop + i1, wsctop + i1, wsclft, wsclft + i2 - 1, TRUE); 
	}
	else *n = 0;
	return -1;
}

void vidrestorechar(UCHAR *memory, INT n)
{
	INT i1, i2;
	
	i2 = (wscrgt - wsclft + 1);
	i1 = i2 * (wscbot - wsctop + 1);
	if (n >= i1) vidaputrect(memory, wsctop, wscbot, wsclft, wscrgt, TRUE, dspattr);
	else if (n > 0) {
		i1 = n / i2;
		i2 = n - i1 * i2;
		if (i1) vidaputrect(memory, wsctop, wsctop + i1 - 1, wsclft, wscrgt, TRUE, dspattr);
		if (i2) vidaputrect(memory, wsctop + i1, wsctop + i1, wsclft, wsclft + i2 - 1, TRUE, dspattr); 
	}
}

INT vidsavewin(UCHAR *memory, INT *n)
{
	INT i1;

	i1 = (wscrgt - wsclft + 1) * (wscbot - wsctop + 1) * CELLWIDTH;
	if (*n < i1) {
		*n = 0;
		return -1;
	}
	*n = i1;
	vidagetrect(memory, wsctop, wscbot, wsclft, wscrgt, FALSE);
	return 0;
}

INT vidrestorewin(UCHAR *memory, INT n)
{
	if (n < (wscrgt - wsclft + 1) * (wscbot - wsctop + 1) * CELLWIDTH) return -1;
	vidaputrect(memory, wsctop, wscbot, wsclft, wscrgt, FALSE, 0);
	vidaflush();
	return 0;
}

INT vidsavestate(UCHAR *memory, INT *n)
{
	STATESAVE savearea;
	if ((UINT)(*n) < sizeof(STATESAVE)) {
		*n = 0;
		return RC_ERROR;
	}
	// Step 1  Move components to the structure
	savearea.wsclft = (USHORT)wsclft;
	savearea.wsctop = (USHORT)wsctop;
	savearea.wscrgt = (USHORT)wscrgt;
	savearea.wscbot = (USHORT)wscbot;
	savearea.vidh = (USHORT)vidh;
	savearea.vidv = (USHORT)vidv;
	savearea.dspattr = dspattr;
	savearea.viddblflag = (UCHAR)viddblflag;
	savearea.vidcurmode = vidcurmode;
	savearea.vidcurtype = vidcurtype;
	savearea.keyinechochar = keyinechochar;
	savearea.kynattr = kynattr;
	memcpy(savearea.validmap, validmap, sizeof(savearea.validmap));
	memcpy(savearea.finishmap, finishmap, sizeof(savearea.finishmap));
	// Step 2  Move the structure to the caller's memory
	memcpy((void*)memory, &savearea, sizeof(STATESAVE));
	return 0;
}

INT vidrestorestate(UCHAR *memory, INT n)
{
	STATESAVE savearea;
	if ((UINT)n < sizeof(STATESAVE)) return RC_ERROR;
	// Step 1  Move from caller's memory to the structure
	memcpy((void*)&savearea, (void*)memory, sizeof(STATESAVE));
	// Step 2  Move the structure components to our fields
	wsclft = savearea.wsclft;
	wsctop = savearea.wsctop;
	wscrgt = savearea.wscrgt;
	wscbot = savearea.wscbot;
	vidh = savearea.vidh;
	vidv = savearea.vidv;
	dspattr = savearea.dspattr;
	viddblflag = savearea.viddblflag;
	vidcurmode = savearea.vidcurmode;
	vidcurtype = savearea.vidcurtype;
	keyinechochar = savearea.keyinechochar;
	kynattr = savearea.kynattr;
	memcpy(validmap, savearea.validmap, sizeof(validmap));
	memcpy(finishmap, savearea.finishmap, sizeof(finishmap));

	vidaposition(vidh, vidv, dspattr);
	if (vidcurmode == CURSMODE_ON) vidacursor(vidcurtype, dspattr);
	else vidacursor(CURSOR_NONE, dspattr);
	return 0;
}

INT vidsavescreen(UCHAR *memory, INT *n)
{
	INT i1;

	i1 = maxlines * maxcolumns * CELLWIDTH + sizeof(STATESAVE);
	if (*n < i1) {
		*n = 0;
		return RC_ERROR;
	}
	vidsavestate(memory, n);
	*n = i1;
	vidagetrect(memory + sizeof(STATESAVE), 0, maxlines - 1, 0, maxcolumns - 1, FALSE);
	return 0;
}

INT vidrestorescreen(UCHAR *memory, INT n)
{
	if ((UINT)n < maxlines * maxcolumns * CELLWIDTH + sizeof(STATESAVE)) return RC_ERROR;
	vidrestorestate(memory, n);
	vidaputrect(memory + sizeof(STATESAVE), 0, maxlines - 1, 0, maxcolumns - 1, FALSE, 0);
	return 0;
}

static void videchochar(INT c)
{
	if (kynattr & KYNATTR_ECHOSECRET) vidaputstring(NULL, 1, dspattr + keyinechochar);
	else vidaputstring(NULL, 1, dspattr + c);
}

static void videchostring(UCHAR *string, INT length)
{
	if (kynattr & KYNATTR_ECHOSECRET) vidaputstring(NULL, length, dspattr + keyinechochar);
	else vidaputstring(string, length, dspattr);
}

void vidtrapstart(UCHAR *map, INT eventid)
{
	/* map is 64 bytes = 512 bits */
#if OS_WIN32
	pvistart();
#endif
	if (map == NULL) {
		trapstate = 0;
		memset(trapmap, 0, (VID_MAXKEYVAL >> 3) + 1);
	}
	else {
		memcpy(trapmap, map, (VID_MAXKEYVAL >> 3) + 1);
		trapeventid = eventid;
		trapstate = 1;
		vidtrapcheck();
	}
#if OS_WIN32
	pviend();
#endif
}

INT vidtrapend(INT *endchar)
{
	INT rc;

#if OS_WIN32
	pvistart();
#endif
	if (trapstate == 2) {
		*endchar = trapchar;
		kbdhead = traphead;
		rc = 0;
	}
	else rc = RC_ERROR;
	trapstate = 0;
#if OS_WIN32
	pviend();
#endif
	return rc;
}

INT vidtrapget(INT *endchar)
{
	INT rc;
	
	rc = 0;
#if OS_WIN32
	pvistart();
#endif
	if (trapstate == 2) *endchar = trapchar;
	else rc = -1;
#if OS_WIN32
	pviend();
#endif
	return rc;
}

static void vidtrapcheck()
{
	INT chr, pos;

	/* assumes caller started critical section */
	if (trapstate == 1) {
		for (pos = kbdhead; pos != kbdtail;) {	/* check keyahead buffer for traps */
			chr = kbdbuffer[pos];
			if (++pos == KBDBUFMAX) pos = 0;
			if (!(kynattr & KYNATTR_KEYCASENORMAL) && chr < 256 && !(dspattr & DSPATTR_KYDSPIPE)) {
				if (kynattr & KYNATTR_KEYCASEUPPER) chr = ukeymap[chr];
				else if (kynattr & KYNATTR_KEYCASELOWER) chr = lkeymap[chr];
				else if (kynattr & KYNATTR_KEYCASEINVERT) {
					if (ukeymap[chr] != lkeymap[chr]) {
						if ((UCHAR) chr == ukeymap[chr]) chr = lkeymap[chr];
						else if ((UCHAR) chr == lkeymap[chr]) chr = ukeymap[chr];
					}
				}
			}
			if (chr <= VID_MAXKEYVAL) {
				if (trapmap[chr >> 3] && (trapmap[chr >> 3] & (1 << (chr & 0x07)))) {
					if (keyinstate == 1) {
						keyinfinishchar = 0;
						keyinstate = 2;
					}
					trapstate = 2;
					trapchar = chr;
					traphead = pos;
					evtset(trapeventid);
					break;
				}
			}
		}
	}
}

void vidkeyingetfinishmap(UCHAR *map)
{
	/* 64 bytes = 512 bits */
	memcpy(map, finishmap, (VID_MAXKEYVAL >> 3) + 1);
}

void vidkeyinfinishmap(UCHAR *map)
{
	/* 64 bytes = 512 bits */
	memcpy(finishmap, map, (VID_MAXKEYVAL >> 3) + 1);
}

void vidkeyinvalidmap(UCHAR *map)
{
	/* 64 bytes = 512 bits */
	memcpy(validmap, map, (VID_MAXKEYVAL >> 3) + 1);
}

INT vidkeyinstart(UCHAR *field, INT length1, INT length2, INT length3, INT flags, INT eventid)
{
	INT i1, i2;
	UCHAR timestamp[16], *ptr;

	if (length1 < 0 || length2 < 0 || length3 < 0) return -1;
	if (length1 > keyinfldsize || length2 > keyinfldsize || length3 > keyinfldsize) {
		i1 = length1;
		if (length2 > i1) i1 = length2;
		if (length3 > i1) i1 = length3;
		i1 = 0x0400 + (i1 & ~0xFF);
		if (keyinfield == keyinfld256) ptr = (UCHAR *) malloc(i1);
		else ptr = (UCHAR *) realloc(keyinfield, i1);
		if (ptr == NULL) return RC_NO_MEM;
		keyinfield = ptr;
		keyinfldsize = i1;
	}
#if OS_WIN32
	pvistart();
#endif
	if (keyintimerhandle > 0) {
		timstop(keyintimerhandle);
		keyintimerhandle = 0;
	}
	kynattr &= ~(KYNATTR_EDITNUM | KYNATTR_EDITFIRSTNUM);

	keyinfldlen = length3;
	if (length1 <= wscrgt - vidh + 1) keyindsplen = length1;
	else keyindsplen = wscrgt - vidh + 1;
	keyinflags = flags;
	if (flags & VID_KEYIN_NUMERIC) {
		keyinleft = 0;
		while (keyinleft < keyinfldlen && field[keyinleft] != '.') keyinleft++;
		keyinright = keyinfldlen - keyinleft - 1;
		if (keyinright < 0) keyinright = 0;
		keyindotpos = -1;
	}
	if (kynattr & (KYNATTR_EDIT | KYNATTR_EDITON)) {  /* *edit */
		if (kynattr & KYNATTR_EDITSPECIAL) {
/*** SPECIAL CODE FOR CBA, NORWAY TO BE ABLE TO INITIALIZE KEYINS FROM THE ***/
/*** CURRENT SCREEN OUTPUT ***/
			keyintot = keyinfldlen;
			if (keyintot > keyindsplen) keyintot = keyindsplen;
			vidagetrect(keyinfield, vidv, vidv, vidh, vidh + keyintot - 1, TRUE);
			if (flags & VID_KEYIN_NUMERIC) {
				for (i1 = 0; i1 < keyintot && field[i1] == ' '; i1++);
				if (i1 < keyintot && field[i1] == '-') i1++;
				for ( ; i1 < keyintot && isdigit(field[i1]); i1++);
				if (i1 != keyinleft) keyintot = 0;
				else if (i1 < keyintot) {
					if ((!(flags & VID_KEYIN_COMMA) && field[i1] != '.') || ((flags & VID_KEYIN_COMMA) && field[i1] != ',') || i1 + 1 == keyintot) keyintot = 0;
					else {
						keyindotpos = i1;
						for (i1++; i1 < keyintot && isdigit(field[i1]); i1++);
						if (i1 < keyintot) {
							keyintot = 0;
							keyindotpos = -1;
						}
					}
				}
/*** RESET keyintot AND keyindotpos FOR NOW UNTIL CBA FIGURES OUT WHAT THEY ***/
/*** WANT TO DO WITH NUMERIC KEYINS ***/
				keyintot = 0;
				keyindotpos = -1;
			}
		}
		else {
			if (length2 > keyinfldlen) keyintot = keyinfldlen;
			else keyintot = length2;
			memcpy(keyinfield, field, keyintot);
			if ((flags & VID_KEYIN_COMMA) && keyinright) {
				for (i1 = 0; i1 < keyintot && keyinfield[i1] != '.'; i1++);
				if (i1 < keyintot) keyinfield[i1] = ',';
			}
			if (keyintot > keyindsplen) i1 = keyindsplen;
			else i1 = keyintot;
			i2 = vidh;
			videchostring(keyinfield, i1);
			if (flags & VID_KEYIN_NUMERIC) kynattr |= KYNATTR_EDITNUM | KYNATTR_EDITFIRSTNUM;
			else if (keyindsplen - i1 > 0) vidaputstring(NULL, keyindsplen - i1, dspattr + ' ');
			vidh = i2;
			vidaposition(vidh, vidv, dspattr);
		}
	}
	else keyintot = 0;

	keyinpos = 0;
	keyinoff = 0;
	keyinhome = vidh;
	keyinend = vidh + keyindsplen;
	if (keyinend > wscrgt) keyinend = wscrgt;
	else if (keyindsplen < keyinfldlen) keyinend--;

	if (vidcurmode == CURSMODE_NORMAL) vidacursor(vidcurtype, dspattr);
	keyineventid = eventid;
	keyinfinishchar = 0;
	keyinstate = 1;
	vidkeyinprocess();
	vidaflush();
	if (keyinstate == 1) {
		if (keyintimeout == 0) {
			keyinfinishchar = -1;
			keyinstate = 2;
			evtset(keyineventid);
		}
		else if (keyintimeout > 0) {
			keyinfinishchar = -1;
			msctimestamp(timestamp);
			timadd(timestamp, keyintimeout * 100);
			keyintimerhandle = timset(timestamp, keyineventid);
		}
	}
#if OS_WIN32
	pviend();
#endif
	return 0;
}

INT vidkeyinend(UCHAR *field, INT *length, INT *finishchar)
{
	INT i1, fieldoffset, fillchr, filloffset, filllength, keyinlength;

#if OS_WIN32
	pvistart();
#endif
	if (!keyinstate) {
#if OS_WIN32
		pviend();
#endif
		return -1;
	}

	if (vidcurmode == CURSMODE_NORMAL) vidacursor(CURSOR_NONE, dspattr);
	if (keyintimerhandle > 0) {
		timstop(keyintimerhandle);
		keyintimerhandle = 0;
	}
	keyinstate = 0;

	if (kynattr & KYNATTR_EDITSPECIAL) keyintot = keyinpos;
	keyinlength = keyintot;
	fillchr = '0';
	fieldoffset = filllength = filloffset = 0;
	if (keyinflags & VID_KEYIN_NUMERIC) {
		if (!keyinright) {
			if (keyintot && (kynattr & KYNATTR_JUSTLEFT)) {
				keyinlength = keyinfldlen;
				filllength = keyinfldlen - keyintot;
				filloffset = keyintot;
			}
		}
		else if (keyinflags & VID_KEYIN_COMMA) {
			for (i1 = 0; i1 < keyintot && keyinfield[i1] != ','; i1++);
			if (i1 < keyintot) keyinfield[i1] = '.';
		}
	}
	else {
		if (!(kynattr & KYNATTR_ZEROFILL)) fillchr = ' ';
		filllength = keyinfldlen - keyintot;
		if (keyintot) {
			if (kynattr & (KYNATTR_JUSTRIGHT | KYNATTR_JUSTLEFT | KYNATTR_ZEROFILL)) keyinlength = keyinfldlen;
			if (kynattr & KYNATTR_JUSTRIGHT) fieldoffset = filllength;
			else filloffset = keyintot;
		}
	}
	if (keyintot) memcpy(&field[fieldoffset], keyinfield, keyintot);
	if (filllength) memset(&field[filloffset], fillchr, filllength);
	*length = keyinlength;
	*finishchar = keyinfinishchar;
#if OS_WIN32
	pviend();
#endif
	return 0;
}

void vidkeyinabort()
{
#if OS_WIN32
	pvistart();
#endif
	if (keyinstate) {
		if (vidcurmode == CURSMODE_NORMAL) vidacursor(CURSOR_NONE, dspattr);
		if (keyintimerhandle > 0) {
			timstop(keyintimerhandle);
			keyintimerhandle = 0;
		}
		keyinstate = 0;
	}
#if OS_WIN32
	pviend();
#endif
}

void vidkeyinclear()
{
#if OS_WIN32
	pvistart();
#endif
	traphead = kbdhead = kbdtail = 0;
#if OS_WIN32
	pviend();
#endif
}

INT vidgeterror(CHAR *errorstr, INT size)
{
	INT length;

	length = (INT)strlen(viderrorstring);
	if (--size >= 0) {
		if (size > length) size = length;
		if (size) memcpy(errorstr, viderrorstring, size);
		errorstr[size] = 0;
		viderrorstring[0] = 0;
	}
	return length;
}

INT vidputerror(CHAR *errorstr, INT type)
{
	INT length1, length2, offset;

	length1 = offset = 0;
	length2 = (INT)strlen(errorstr);
	if (length2 > (INT) sizeof(viderrorstring) - 1) length2 = sizeof(viderrorstring) - 1;
	if (type & (VIDPUTERROR_APPEND | VIDPUTERROR_INSERT)) {
		length1 = (INT)strlen(viderrorstring);
		if (type & VIDPUTERROR_APPEND) {
			if (length2 > (INT)(sizeof(viderrorstring) - 1 - length1)) length2 = sizeof(viderrorstring) - 1 - length1;
			offset = length1;
		}
		else {
			if (length1 > (INT)(sizeof(viderrorstring) - 1 - length2)) length1 = sizeof(viderrorstring) - 1 - length2;
			if (length1) memmove(viderrorstring + length2, viderrorstring, length1);
		}
	}
	if (length2) memcpy(viderrorstring + offset, errorstr, length2);
	viderrorstring[length1 + length2] = 0;
	return length1 + length2;
}

static INT vidcharcallback(INT chr)
{
	INT newtail;
	UCHAR timestamp[16];

#if OS_WIN32
	pvistart();
#endif
	if (chr < 256) chr = xkeymap[chr];
	if (!(dspattr & DSPATTR_KYDSPIPE)) {
		if (chr == cancelkey1 || chr == cancelkey2) chr = VID_CANCEL;
		else if (chr == intrchr) chr = VID_INTERRUPT;
	}
	newtail = kbdtail;
	if (++newtail == KBDBUFMAX) newtail = 0;
	if (newtail == kbdhead) {  /* overflow of buffer */
#if OS_WIN32
		pviend();
#endif
		return RC_ERROR;
	}
	kbdbuffer[kbdtail] = chr;
	kbdtail = newtail;

	if (trapstate == 1) {
		if (!(kynattr & KYNATTR_KEYCASENORMAL) && chr < 256 && !(dspattr & DSPATTR_KYDSPIPE)) {
			if (kynattr & KYNATTR_KEYCASEUPPER) chr = ukeymap[chr];
			else if (kynattr & KYNATTR_KEYCASELOWER) chr = lkeymap[chr];
			else if (kynattr & KYNATTR_KEYCASEINVERT) {
				if (ukeymap[chr] != lkeymap[chr]) {
					if ((UCHAR) chr == ukeymap[chr]) chr = lkeymap[chr];
					else if ((UCHAR) chr == lkeymap[chr]) chr = ukeymap[chr];
				}
			}
		}
		if (chr <= VID_MAXKEYVAL) {
			if (trapmap[chr >> 3] && (trapmap[chr >> 3] & (1 << (chr & 0x07)))) {
				if (keyinstate == 1) {
					keyinfinishchar = 0;
					keyinstate = 2;
				}
				trapstate = 2;
				trapchar = chr;
				traphead = kbdtail;
				evtset(trapeventid);
				goto vidcharcallbackend;
			}
		}
	}

	if (keyinstate == 1) {
		if (keyintimerhandle > 0) {
			timstop(keyintimerhandle);
			keyintimerhandle = 0;
			if (evttest(keyineventid)) {
				keyinstate = 2;
				goto vidcharcallbackend;
			}
		}
		vidkeyinprocess();
		vidaflush();
		if (keyinstate == 1 && keyintimeout > 0) {
			msctimestamp(timestamp);
			timadd(timestamp, keyintimeout * 100);
			keyintimerhandle = timset(timestamp, keyineventid);
		}
	}

vidcharcallbackend:
#if OS_WIN32
	pviend();
#endif
	return 0;
}

static void vidkeyinprocess()
{
	INT i1, chr, len;
	UCHAR c1;

	while (kbdhead != kbdtail) {
		chr = kbdbuffer[kbdhead];
		if (++kbdhead == KBDBUFMAX) kbdhead = 0;
		if (!(kynattr & KYNATTR_KEYCASENORMAL) && chr < 256 && !(dspattr & DSPATTR_KYDSPIPE)) {
			if (kynattr & KYNATTR_KEYCASEUPPER) chr = ukeymap[chr];
			else if (kynattr & KYNATTR_KEYCASELOWER) chr = lkeymap[chr];
			else if (kynattr & KYNATTR_KEYCASEINVERT) {
				if (ukeymap[chr] != lkeymap[chr]) {
					if ((UCHAR) chr == ukeymap[chr]) chr = lkeymap[chr];
					else if ((UCHAR) chr == lkeymap[chr]) chr = ukeymap[chr];
				}
			}
		}

		if (kynattr & KYNATTR_CLICK) vidasound(0x7000, 1, dspattr, FALSE);

		if ((kynattr & (KYNATTR_EDIT | KYNATTR_EDITON)) && !(kynattr & KYNATTR_EDITNUM)) {
			if (chr == VID_DELETE) {
				if (keyinpos < keyintot) {
					len = keyintot-- - keyinpos;
					if (len > 1) memmove(&keyinfield[keyinpos], &keyinfield[keyinpos + 1], len - 1);
					keyinfield[keyintot] = ' ';
					i1 = 0;
					if (len > keyindsplen - (keyinpos - keyinoff)) len = keyindsplen - (keyinpos - keyinoff);
					else if (kynattr & KYNATTR_ECHOSECRET) i1 = 1;
					if (vidcurmode != CURSMODE_OFF) vidacursor(CURSOR_NONE, dspattr);

					videchostring(&keyinfield[keyinpos], len - i1);
					if (i1) videchochar(' ');
					vidaposition(vidh, vidv, dspattr);

					if (vidcurmode != CURSMODE_OFF) vidacursor(vidcurtype, dspattr);
				}
				continue;
			}
			if (chr == VID_LEFT) {
				if (keyinpos != 0) {
					if (--keyinpos < keyinoff) {
						keyinoff = keyinpos;
						len = keyintot - keyinpos;
						if (len > keyindsplen) len = keyindsplen;
						if (vidcurmode != CURSMODE_OFF) vidacursor(CURSOR_NONE, dspattr);

						videchostring(&keyinfield[keyinpos], len);
						vidh = keyinhome;
						vidaposition(vidh, vidv, dspattr);

						if (vidcurmode != CURSMODE_OFF) vidacursor(vidcurtype, dspattr);
					}
					else vidaposition(--vidh, vidv, dspattr);
				}
				continue;
			}
			if (chr == VID_RIGHT) {
				if (keyinpos < keyintot) {
					keyinpos++;
					if (vidh >= keyinend) {
						keyinoff++;
						len = keyindsplen;
						if (len > keyintot - keyinoff) len = keyintot - keyinoff;
						if (vidcurmode != CURSMODE_OFF) vidacursor(CURSOR_NONE, dspattr);

						vidaposition(keyinhome, vidv, dspattr);
						videchostring(&keyinfield[keyinoff], len);
						if (len < keyindsplen) vidaputstring(NULL, 1, dspattr + ' ');
						vidh = keyinend;
						vidaposition(vidh, vidv, dspattr);

						if (vidcurmode != CURSMODE_OFF) vidacursor(vidcurtype, dspattr);
					}
					else vidaposition(++vidh, vidv, dspattr);
				}
				continue;
			}
			if (chr == VID_HOME) {
				if (keyinpos != 0) {
					keyinpos = 0;
					vidh = keyinhome;
					if (keyinoff) {
						keyinoff = 0;
						if (vidcurmode != CURSMODE_OFF) vidacursor(CURSOR_NONE, dspattr);

						vidaposition(vidh, vidv, dspattr);
						videchostring(keyinfield, keyindsplen);
						vidaposition(vidh, vidv, dspattr);

						if (vidcurmode != CURSMODE_OFF) vidacursor(vidcurtype, dspattr);
					}
					else vidaposition(vidh, vidv, dspattr);
				}
				continue;
			}
			if (chr == VID_END) {
				if (keyinpos != keyintot) {
					keyinpos = keyintot;
					if (keyinhome + keyintot > keyinend) {
						keyinoff = keyintot - (keyindsplen - 1);
						if (vidcurmode != CURSMODE_OFF) vidacursor(CURSOR_NONE, dspattr);

						vidaposition(keyinhome, vidv, dspattr);
						videchostring(&keyinfield[keyinoff], keyindsplen - 1);
						vidaputstring(NULL, 1, dspattr + ' ');
						vidh = keyinend;
						vidaposition(vidh, vidv, dspattr);

						if (vidcurmode != CURSMODE_OFF) vidacursor(vidcurtype, dspattr);
					}
					else {
						vidh = keyinhome + keyintot;
						vidaposition(vidh, vidv, dspattr);
					}
				}
				continue;
			}
			if (chr == VID_INSERT && (kynattr & KYNATTR_EDITMODE)) {
				kynattr ^= KYNATTR_EDITOVERSTRIKE;
				continue;
			}
		}
		if (chr >= 10000 || finishmap[chr >> 3] & (1 << (chr & 0x07))) {
			keyinfinishchar = chr;
			keyinstate = 2;
			evtset(keyineventid);
			return;
		}
		if (chr == VID_CANCEL) {
			if (keyintot) {
				if ((kynattr & (KYNATTR_EDIT | KYNATTR_EDITON)) || !(kynattr & KYNATTR_NOECHO)) {
					len = keyintot - keyinoff;
					if (len > keyindsplen) len = keyindsplen;
					if (vidcurmode != CURSMODE_OFF) vidacursor(CURSOR_NONE, dspattr);

					vidh = keyinhome;
					vidaposition(vidh, vidv, dspattr);
					vidaputstring(NULL, len, dspattr + ' ');
					vidaposition(vidh, vidv, dspattr);

					if (vidcurmode != CURSMODE_OFF) vidacursor(vidcurtype, dspattr);
				}

				memset(keyinfield, ' ', keyinfldlen);
				keyinpos = keyinoff = keyintot = 0;
				kynattr &= ~KYNATTR_EDITFIRSTNUM;
			}
			if (keyinflags & VID_KEYIN_NUMERIC) keyindotpos = -1;
		}
		else if (chr == VID_BKSPC) {
			if (keyinpos != 0) {
				keyinpos--;
				keyintot--;
				if ((keyinflags & VID_KEYIN_NUMERIC) && (keyinfield[keyinpos] == '.' || keyinfield[keyinpos] == ',')) keyindotpos = -1;
				memmove(&keyinfield[keyinpos], &keyinfield[keyinpos + 1], (keyintot - keyinpos));
				keyinfield[keyintot] = ' ';
				if ((kynattr & (KYNATTR_EDIT | KYNATTR_EDITON)) && !(kynattr & KYNATTR_EDITNUM)) {
					if (keyinpos >= keyinoff) {
						i1 = 0;
						len = keyintot - keyinpos + 1;
						if (len > keyindsplen - (keyinpos - keyinoff)) len = keyindsplen - (keyinpos - keyinoff);
						else if (kynattr & KYNATTR_ECHOSECRET) i1 = 1;
						if (vidcurmode != CURSMODE_OFF) vidacursor(CURSOR_NONE, dspattr);

						vidaposition(--vidh, vidv, dspattr);
						videchostring(&keyinfield[keyinpos], len - i1);
						if (i1) vidaputstring(NULL, 1, dspattr + ' ');
						vidaposition(vidh, vidv, dspattr);

						if (vidcurmode != CURSMODE_OFF) vidacursor(vidcurtype, dspattr);
					}
					else keyinoff = keyinpos;
				}
				else if (!(kynattr & KYNATTR_NOECHO)) {
					c1 = ' ';
					if (keyinhome + keyinpos < keyinend) {
						if (!(dspattr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE))) vidaposition(--vidh, vidv, dspattr);
						else vidaputstring(NULL, 1, dspattr + '\b');
					}
					else {
						if (keyinhome + keyinpos > keyinend) {
							if (!(kynattr & KYNATTR_ECHOSECRET)) c1 = keyinfield[keyinpos - 1];
							else c1 = keyinechochar;
						}
					}
					vidaputstring(NULL, 1, dspattr + c1);
					if (!(dspattr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE))) vidaposition(vidh, vidv, dspattr);
					else vidaputstring(NULL, 1, dspattr + '\b');
				}
			}
		}
		else if ((dspattr & DSPATTR_KYDSPIPE) || (validmap[chr >> 3] & (1 << (chr & 0x07)))) {
			if (keyinpos == keyinfldlen ||
			   ((kynattr & KYNATTR_DIGITENTRY) && (chr < '0' || chr > '9')) ||
			   ((keyinflags & VID_KEYIN_NUMERIC) && (((chr < '0' || chr > '9') &&
			      ((!(keyinflags & VID_KEYIN_COMMA) && chr != '.') || ((keyinflags & VID_KEYIN_COMMA) && chr != ',') ||
			      !keyinright || keyindotpos >= 0) && (chr != '-' || keyinpos != 0)) ||
			         (keyinpos == keyinleft && keyindotpos < 0 && chr != '.' && chr != ',') ||
			         (keyindotpos >= 0 && (keyinpos - keyindotpos) > keyinright)))) {
					if (!(kynattr & KYNATTR_NOECHO)) vidasound(900, 2, dspattr, FALSE);
			}
			else {
				if (kynattr & KYNATTR_EDITFIRSTNUM) keyintot = 0;
				if ((keyinflags & VID_KEYIN_NUMERIC) && (chr == '.' || chr == ',')) keyindotpos = keyinpos;
				if (keyinpos < keyintot && !(kynattr & KYNATTR_EDITOVERSTRIKE)) {
					len = keyintot - keyinpos;
					if (keyintot == keyinfldlen) len--;
					else keyintot++;
					memmove(&keyinfield[keyinpos + 1], &keyinfield[keyinpos], len);
				}
				else if (keyinpos == keyintot) keyintot++;
				keyinfield[keyinpos++] = (UCHAR) chr;

				if ((kynattr & (KYNATTR_EDIT | KYNATTR_EDITON)) || !(kynattr & KYNATTR_NOECHO)) {
					if (vidh >= keyinend) {
						if ((kynattr & (KYNATTR_EDIT | KYNATTR_EDITON)) && !(kynattr & KYNATTR_EDITNUM)) {
							keyinoff++;
							if (vidcurmode != CURSMODE_OFF) vidacursor(CURSOR_NONE, dspattr);

							vidh = keyinhome;
							vidaposition(vidh, vidv, dspattr);
							videchostring(&keyinfield[keyinoff], keyinpos - keyinoff);
							if (keyinpos == keyinfldlen) vidaputstring(NULL, 1, dspattr + ' ');
							else if (kynattr & KYNATTR_EDITOVERSTRIKE) videchochar(keyinfield[keyinpos]);
							vidh = keyinend;
							vidaposition(vidh, vidv, dspattr);

							if (vidcurmode != CURSMODE_OFF) vidacursor(vidcurtype, dspattr);
						}
						else {
							videchochar(chr);
							vidh = keyinend;
							vidaposition(vidh, vidv, dspattr);
						}
					}
					else if (keyinpos < keyintot && !(kynattr & KYNATTR_EDITOVERSTRIKE)) {
						len = keyintot - (keyinpos - 1);
						if (len > keyindsplen - (keyinpos - 1 - keyinoff)) len = keyindsplen - (keyinpos - 1 - keyinoff);
						if (vidcurmode != CURSMODE_OFF) vidacursor(CURSOR_NONE, dspattr);

						videchostring(&keyinfield[keyinpos - 1], len);
						vidaposition(++vidh, vidv, dspattr);

						if (vidcurmode != CURSMODE_OFF) vidacursor(vidcurtype, dspattr);
					}
					else {
						videchochar(chr);
						vidaposition(++vidh, vidv, dspattr);
						if (kynattr & KYNATTR_EDITFIRSTNUM) {
							kynattr &= ~KYNATTR_EDITFIRSTNUM;
							if (vidcurmode != CURSMODE_OFF) vidacursor(CURSOR_NONE, dspattr);

							vidaputstring(NULL, keyindsplen - 1, dspattr + ' ');
							vidaposition(vidh, vidv, dspattr);

							if (vidcurmode != CURSMODE_OFF) vidacursor(vidcurtype, dspattr);
						}
					}
				}

				if ((kynattr & KYNATTR_AUTOENTER) && (keyintot == keyinfldlen ||
				   ((keyinflags & VID_KEYIN_NUMERIC) && keyindotpos >= 0 && keyinpos - keyindotpos > keyinright))) {
					keyinfinishchar = 0;
					keyinstate = 2;
					if (trapstate == 1 && (trapmap[VID_ENTER >> 3] & (1 << (VID_ENTER & 0x07)))) {
						trapstate = 2;
						trapchar = VID_ENTER;
						traphead = kbdhead;
						evtset(trapeventid);
					}
					else evtset(keyineventid);
					return;
				}
			}
		}
	}
}
