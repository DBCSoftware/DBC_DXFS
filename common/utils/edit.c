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
#define INC_SIGNAL
#include "includes.h"
#include "release.h"
#include "arg.h"
#include "base.h"
#include "dbccfg.h"
#include "evt.h"
#include "fio.h"
#include "vid.h"

#if OS_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define CBKFUNC void (*)(INT, INT, INT)
#define BRKFUNC void (*)(INT)

#define MAXUNDO 100
#define MAXWINDOW 2
#define MAXBUFFER 9
#define MAXPLAYBACK 500
#define MAXDSPINFO 40
#define MAXRECSIZE 8192

struct linestruct {                /* edit line structure */
	struct linestruct **next;     /* ptr to ptr to next line */
	struct linestruct **prev;     /* ptr to ptr to previous line */
	INT size;                     /* num of bytes used in linestruct.data */
	UCHAR data[1];                /* line data */
};

typedef struct linestruct * LINEPTR;
typedef struct linestruct ** LINEHANDLE;

struct bufferstruct {
	LINEHANDLE firstline;         /* handle of first line in buffer */
	LINEHANDLE lastline;          /* handle of last line in buffer */
	LINEHANDLE curline;           /* handle of current line in buffer */
	UINT linecount;               /* number of lines in buffer */
	UINT linenum;                 /* current line number (1 is first) */
	UINT cursorhorz;              /* cursor horz display offset in current line */
	UINT lineoffset;              /* cursor line offset (not always valid) */
	UINT windowtopline;           /* window top line number (1 is top) */
	UINT windowleftoffset;        /* window left side offset */
	UINT windownumber;            /* window number (0 if not in window) */
	UINT beginmark;               /* first marked line (0 if first or no mark) */
	UINT endmark;                 /* last marked line (0 if no mark) */
	INT filetype;                 /* file type of buffer */
	UCHAR changeflg;              /* buffer has been changed flag */
	UCHAR memcapflg;              /* memory capacity exceeded */
	UCHAR readonlyflag;			/* set to TRUE if file opened was flagged read-only */
	CHAR filename[128];           /* filename of buffer */
};

typedef struct bufferstruct *BUFFERPTR;

static struct bufferstruct buffers[MAXBUFFER];  /* buffer table */
static BUFFERPTR buf;              /* current buffer */

struct optionstruct {
	CHAR *text;                   /* text of option */
	USHORT type;                  /* option type (CMD_ or OPT_) */
	USHORT value;                 /* option value */
} ;

static UINT meminitsize;                /* alternate amount to a 1 meg meminit (-A parm) */
static USHORT cmdtable[512];            /* keyin char to cmd translation */
static USHORT keyend;                   /* response line: keyin end */
static USHORT keyabort;                 /* response line: keyin abort */
static USHORT keybkspc;                 /* response line: keyin backspace */
static USHORT keyrecall;                /* response line: keyin recall */
static UCHAR quitflg;                   /* TRUE if exit is occurring */
static UCHAR cursorflg;                 /* TRUE if cursor is currently on */
static UCHAR insmodeflg;                /* TRUE if INSERT mode on */
static UCHAR recmodeflg;                /* TRUE if RECORDER on */
static UCHAR playbackflg;               /* TRUE if PLAYBACK is on */
static UCHAR fileplaybackflg;           /* TRUE if file PLAYBACK is on */
static UCHAR msgflg;                    /* TRUE if message displayed on top line */
static UCHAR keyflg;                    /* TRUE if residual keyin displayed on top line */
static UCHAR linenumchgflg;             /* TRUE if line number display should change */
static UCHAR updownflg;                 /* TRUE if last cursor command was up or down */
static UCHAR tabdisplayflg;             /* if TRUE, display tab characters */
static UCHAR filecompflg;               /* DB/C standard file compression flag */
static UCHAR dsplineinfoflg;            /* display upper right corner line info */
static UCHAR searchcaseflg;             /* TRUE if search is case sensitive */
static CHAR forcechar;                  /* search force character */
static CHAR matchanychar;               /* search wildcard character */
static CHAR leftanchorchar;             /* search line left anchor character */
static CHAR rightanchorchar;            /* search line right anchor character */
static CHAR matchinsertchar;            /* translate insert match string character */
static UCHAR brightflg;                 /* if TRUE foreground is bold */
static USHORT fgcolor;                  /* foreground color */
static USHORT bgcolor;                  /* background color */
static UINT slines, scolumns;           /* screen number of lines, columns */
static UINT wlines;                     /* current window number of lines */
static UCHAR windowstatus;              /* 0 = full screen, 1 = window 1, 2 = window 2 */
static USHORT windowsplitline;               /* window split line number */
static INT playbackindex, playbackcount;  /* index and count in playback buffer */
static USHORT playbackbuffer[MAXPLAYBACK];  /* playback buffer */
static INT fileplaybackindex, fileplaybackcount;  /* index and count in file playback buffer */
static USHORT fileplaybackbuffer[MAXPLAYBACK];  /* file playback buffer */
static INT currentbufindex;             /* current buffer index */
static INT markbufindex;                /* marked buffer index */
static INT window1bufindex;             /* window 1 buffer index */
static INT window2bufindex;             /* window 2 buffer index */
static INT markstatus;                  /* mark status 0 = none, 1 = first, 2 = marked */
static LINEHANDLE undostack[MAXUNDO];   /* undo stack */
static INT undostacktop;                /* undo stack top index */
static INT undostacktail;               /* undo stack tail index */
static INT keyinvalue;                  /* value returned from keyin */
static CHAR keyinstring[128];           /* string returned from keyin */
static UCHAR tablookup[512];            /* tab expansion, each entry is num of chars/tab */
static CHAR backext[4] = "_";           /* backup file extension */
static CHAR tabset[32];                 /* tab setting */
static UCHAR tabdisplay;                /* tab display character */
static UINT lastwindowtopline;          /* last displayed window top line */
static UINT lastwindowleftoffset;       /* last displayed window left offset */
static CHAR dspline[1200];              /* display line */
static CHAR dottext[5] = ".txt";        /* default extension */
static UCHAR record[1024];              /* used by cmdread() */
static CHAR readfilename[128];          /* used by cmdread() */
static UCHAR filedata[1024];            /* used by rdwtcfg() */
static INT createtype;

/* vidput cmd sequences */
static INT32 setwindow[] = { VID_WIN_TOP, VID_WIN_BOTTOM, VID_HU, VID_FINISH };  /* is current window settings */
static INT32 cursorpos[] = { VID_HORZ, VID_VERT, VID_FINISH };  /* is current cursor position */
static INT32 cursoron[] = { VID_CURSOR_ON, VID_FINISH };
static INT32 cursoroff[] = { VID_CURSOR_OFF, VID_FINISH };
static INT32 scrollup[] = { VID_RU, VID_END_NOFLUSH };
#if !OS_WIN32
static INT32 scrolldown[] = { VID_RD, VID_END_NOFLUSH };
#endif
static INT32 insline[] = { VID_IL, VID_FINISH };
static INT32 delline[] = { VID_DL, VID_FINISH };
static INT32 erasewindow[] = { VID_ES, VID_FINISH };
static INT32 eraseline[] = { VID_EL, VID_END_NOFLUSH };
static INT32 reveraseline[] = { VID_REV_ON, VID_EL, VID_REV_OFF, VID_END_NOFLUSH };
static INT32 beep[] = { VID_BEEP, VID_FINISH };
static INT32 revon[] = { VID_BOLD_OFF, VID_REV_ON, VID_END_NOFLUSH };
static INT32 revoff[] = { VID_REV_OFF, VID_END_NOFLUSH };
static INT32 revoffboldon[] = { VID_REV_OFF, VID_BOLD_ON, VID_END_NOFLUSH };

#define setcursorhorz(n) (cursorpos[0] = VID_HORZ + (n))
#define setcursorvert(n) (cursorpos[1] = VID_VERT + (n))

/* keyin type */
#define KEYIN_KEYSTROKE 1
#define KEYIN_NUMBER 2
#define KEYIN_YESNO 3
#define KEYIN_STRING 4
#define KEYIN_TABSTRING 5
#define KEYIN_STRING3 6

/* values for options.type field */
#define OPT_TABSET 1
#define OPT_TABDSP 2
#define OPT_TABDSPCHAR 3
#define OPT_WINSPLIT 4
#define OPT_BACKEXT 5
#define OPT_FILECOMP 6
#define OPT_LINENUM 7
#define OPT_CASESENSE 8
#define OPT_FGCOLOR 9
#define OPT_BGCOLOR 10
#define OPT_BRIGHT 11
#define OPT_SRCHFORCE 12
#define OPT_SRCHMATCH 13
#define OPT_SRCHLEFTANCHOR 14
#define OPT_SRCHRIGHTANCHOR 15
#define OPT_TRANINSERT 16
#define OPT_KEYEND 17
#define OPT_KEYABORT 18
#define OPT_KEYBKSPC 19
#define OPT_KEYRECALL 20
#define OPT_BUFFER1 21
#define OPT_BUFFER2 22
#define OPT_BUFFER3 23
#define OPT_BUFFER4 24
#define OPT_BUFFER5 25
#define OPT_BUFFER6 26
#define OPT_BUFFER7 27
#define OPT_BUFFER8 28
#define OPT_BUFFER9 29
#define CMD_TAB 601
#define CMD_CLF 602
#define CMD_CRT 603
#define CMD_CUP 604
#define CMD_CDN 605
#define CMD_WLF 606
#define CMD_WRT 607
#define CMD_LLF 608
#define CMD_LRT 609
#define CMD_HOM 610
#define CMD_END 611
#define CMD_WHM 612
#define CMD_WEN 613
#define CMD_SUP 614
#define CMD_SDN 615
#define CMD_PUP 616
#define CMD_PDN 617
#define CMD_INS 618
#define CMD_GTO 619
#define CMD_KRC 620
#define CMD_KPB 621
#define CMD_FRC 622
#define CMD_FPB 623
#define CMD_FPA 624
#define CMD_SET 625
#define CMD_HLP 626
#define CMD_XIT 627
#define CMD_NXW 628
#define CMD_CLW 629
#define CMD_NXB 630
#define CMD_PVB 631
#define CMD_NLB 632
#define CMD_NLA 633
#define CMD_RET 634
#define CMD_BSP 635
#define CMD_DEL 636
#define CMD_DLN 637
#define CMD_DLE 638
#define CMD_DBK 639
#define CMD_CPY 640
#define CMD_MOV 641
#define CMD_MRK 642
#define CMD_UND 643
#define CMD_SRH 644
#define CMD_SRA 645
#define CMD_XLT 646
#define CMD_FRD 647
#define CMD_FWT 648
#define CMD_FNM 649
#define CMD_FWM 650
#define CMD_FMG 651

/* The first character of each word of text is used to construct the lookup key */
static struct optionstruct options[] = {
	/* buffer display options must remain first - elements 0 to MAXBUFFER */
	"  Buffer 1 display window...............", OPT_BUFFER1, 1,
	"  Buffer 2 display window...............", OPT_BUFFER2, 1,
	"  Buffer 3 display window...............", OPT_BUFFER3, 1,
	"  Buffer 4 display window...............", OPT_BUFFER4, 2,
	"  Buffer 5 display window...............", OPT_BUFFER5, 2,
	"  Buffer 6 display window...............", OPT_BUFFER6, 2,
	"  Buffer 7 display window...............", OPT_BUFFER7, 0,
	"  Buffer 8 display window...............", OPT_BUFFER8, 0,
	"  Buffer 9 display window...............", OPT_BUFFER9, 0,
	"  Window split line.....................", OPT_WINSPLIT, 14,
	"  TAB expansion setting.................", OPT_TABSET, 0,
	"  TAB keystroke.........................", CMD_TAB, VID_TAB,         /* TAB */
	"  Display TAB characters................", OPT_TABDSP, 0,
	"  TAB character to display..............", OPT_TABDSPCHAR, '>',
	"  Move cursor left......................", CMD_CLF, VID_LEFT,        /* LEFT */
	"  Move cursor right.....................", CMD_CRT, VID_RIGHT,       /* RIGHT */
	"  Move cursor up........................", CMD_CUP, VID_UP,          /* UP */
	"  Move cursor down......................", CMD_CDN, VID_DOWN,        /* DOWN */
	"  Move cursor word left.................", CMD_WLF, VID_CTLLEFT,     /* CTL LEFT */
	"  Move cursor word right................", CMD_WRT, VID_CTLRIGHT,    /* CTL RIGHT */
	"  Move cursor line left.................", CMD_LLF, VID_F5,          /* F5 */
	"  Move cursor line right................", CMD_LRT, VID_F6,          /* F6 */
	"  Move cursor to first line of buffer...", CMD_HOM, VID_HOME,        /* HOME */
	"  Move cursor to last line of buffer....", CMD_END, VID_END,         /* END */
	"  Move cursor to first line of window...", CMD_WHM, VID_CTLHOME,     /* CTL HOME */
	"  Move cursor to last line of window....", CMD_WEN, VID_CTLEND,      /* CTL END */
	"  Scroll window up one line.............", CMD_SUP, VID_CTLUP,       /* CTL UP */
	"  Scroll window down one line...........", CMD_SDN, VID_CTLDOWN,     /* CTL DOWN */
	"  Page up...............................", CMD_PUP, VID_PGUP,        /* PGUP */
	"  Page down.............................", CMD_PDN, VID_PGDN,        /* PGDN */
	"  Goto line by number...................", CMD_GTO, 7,               /* CTL G */
	"  Change INSERT/OVERSTRIKE mode.........", CMD_INS, VID_INSERT,      /* INSERT */
	"  Start/Stop keystroke recorder.........", CMD_KRC, 18,              /* CTL R */
	"  Playback recorded keystrokes..........", CMD_KPB, 16,              /* CTL P */
	"  Save keystroke recorder in file.......", CMD_FRC, VID_F7,          /* F7 */
	"  Playback recorded keystrokes in file..", CMD_FPB, VID_F8,          /* F8 */
	"  Playback file keystrokes again........", CMD_FPA, 17,              /* CTL Q */
	"  Set options...........................", CMD_SET, 15,              /* CTL O */
	"  Help..................................", CMD_HLP, VID_F10,         /* F10 */
	"  Exit..................................", CMD_XIT, 24,              /* CTL X */
	"  Change window.........................", CMD_NXW, VID_F1,          /* F1 */
	"  Close lower window....................", CMD_CLW, VID_F2,          /* F2 */
	"  Change to next buffer.................", CMD_NXB, VID_F3,          /* F3 */
	"  Change to previous buffer.............", CMD_PVB, VID_F4,          /* F4 */
	"  New line before.......................", CMD_NLB, 2,               /* CTL B */
	"  New line after........................", CMD_NLA, 1,               /* CTL A */
	"  Split current line at cursor..........", CMD_RET, VID_ENTER,       /* ENTER */
	"  Backspace.............................", CMD_BSP, VID_BKSPC,       /* BACKSPACE */
	"  Delete character......................", CMD_DEL, VID_DELETE,      /* DELETE */
	"  Delete line...........................", CMD_DLN, 4,               /* CTL D */
	"  Delete from cursor to end of line.....", CMD_DLE, 12,              /* CTL L */
	"  Delete marked block...................", CMD_DBK, 11,              /* CTL K */
	"  Copy marked block.....................", CMD_CPY, 3,               /* CTL C */
	"  Move marked block.....................", CMD_MOV, 14,              /* CTL N */
	"  Mark block............................", CMD_MRK, VID_ESCAPE,      /* ESC */
	"  Undo deleted line.....................", CMD_UND, 21,              /* CTL U */
	"  Search................................", CMD_SRH, 19,              /* CTL S */
	"  Search again..........................", CMD_SRA, 26,              /* CTL Z */
	"  Translate.............................", CMD_XLT, 20,              /* CTL T */
	"  Read file.............................", CMD_FRD, 5,               /* CTL E */
	"  Write file............................", CMD_FWT, 23,              /* CTL W */
	"  Write marked block....................", CMD_FWM, 22,              /* CTL V */
	"  Change file name......................", CMD_FNM, 6,               /* CTL F */
	"  Merge file............................", CMD_FMG, 25,              /* CTL Y */
	"  Response line: ending character.......", OPT_KEYEND, VID_ENTER,    /* ENTER */
	"  Response line: abort character........", OPT_KEYABORT, VID_ESCAPE, /* ESC */
	"  Response line: backspace character....", OPT_KEYBKSPC, VID_BKSPC,  /* BKSPC */
	"  Response line: recall character.......", OPT_KEYRECALL, VID_UP,    /* UP */
	"  Search: forcing character.............", OPT_SRCHFORCE, '#',
	"  Search: match all character...........", OPT_SRCHMATCH, '?',
	"  Search: line start anchor.............", OPT_SRCHLEFTANCHOR, '$',
	"  Search: line end anchor...............", OPT_SRCHRIGHTANCHOR, '!',
	"  Translate: insert matched string......", OPT_TRANINSERT, '^',
	"  Backup file extension.................", OPT_BACKEXT, 0,
	"  Compress DB/C standard file...........", OPT_FILECOMP, 0,
	"  Display line numbers..................", OPT_LINENUM, 1,
	"  Case sensitive search.................", OPT_CASESENSE, 1,
	"  Foreground color......................", OPT_FGCOLOR, 7,
	"  Background color......................", OPT_BGCOLOR, 0,
	"  High intensity foreground.............", OPT_BRIGHT, 1,
	NULL, 0, 0
} ;

static CHAR *colors[] = {
	"BLACK", "BLUE", "GREEN", "CYAN",
	"RED", "MAGENTA", "YELLOW", "WHITE" 
};
static INT eventid;

static void initialize(void);
static void cmdleft(void);
static void cmdright(void);
static void cmdup(void);
static void cmddown(void);
static void cmdbkspc(void);
static void cmddelchar(void);
static void cmdwordleft(void);
static void cmdwordright(void);
static void cmdlineleft(void);
static void cmdlineright(void);
static void cmdhome(void);
static void cmdend(void);
static void cmdwinhome(void);
static void cmdwinend(void);
static void cmdscrollup(void);
static void cmdscrolldown(void);
static void cmdpageup(void);
static void cmdpagedown(void);
static void cmdinsmode(void);
static void cmdplayback(void);
static void cmdrecorder(void);
static void cmdfileplayback(INT);
static void cmdfilerecorder(void);
static void cmdgoto(void);
static void cmdoptions(void);
static void displayoptionline(INT, INT);
static void cmdhelp(void);
static void cmdexit(void);
static void cmdnextwindow(void);
static void cmdclosewindow(void);
static void cmdnextbuffer(INT);
static void cmdbefore(void);
static void cmdsplit(INT);
static void cmddelete(void);
static void cmddelend(void);
static void cmddelmarked(void);
static void cmdcopy(void);
static void cmdmove(void);
static void cmdmark(void);
static void cmdundo(void);
static void cmdsearch(INT);
static void cmdtranslate(void);
static void cmdread(CHAR *);
static void cmdwrite(void);
static void cmdfilename(void);
static void cmdwritemarked(void);
static void cmdmerge(void);
static INT search(UINT, UINT, CHAR *);
static void displayspecial(INT, INT);
static INT writefile(CHAR *, INT, INT, INT);
static INT setcurrentbufwin(INT);
static void clearbuffer(void);
static void gotoline(BUFFERPTR, UINT);
static LINEHANDLE linealloc(INT) ;
static INT chglinesize(LINEHANDLE, INT);
static void pushundostack(LINEHANDLE);
static INT keyin(CHAR *, INT);
static void display(INT);
static void displayline(void);
static void displaylineinfo(void);
static void doframe(void);
static INT horztooffset(INT);
static INT offsettohorz(INT);
static INT horztohorz(INT);
static void insertline(BUFFERPTR, LINEHANDLE);
static void deleteline(BUFFERPTR);
static INT edittab(CHAR *);
static void message(CHAR *);
static void rdwtcfg(INT);
static CHAR *keytext(INT);
static INT charget(void);
static void usage(void);
static void quitsig(INT);

INT main(INT argc, CHAR *argv[])
{
	INT i1, c, m, n;

	arginit(argc, argv, &i1);
	signal(SIGINT, quitsig);

	initialize();
	while (!quitflg) {  /* main loop */
#if !OS_WIN32
		lastwindowtopline = buf->windowtopline;
		lastwindowleftoffset = buf->windowleftoffset;
#endif
		if (linenumchgflg || msgflg || keyflg) displaylineinfo();
		if (fileplaybackflg) {  /* keystrokes/commands come from file playback array */
			if (fileplaybackindex == fileplaybackcount) {
				fileplaybackflg = FALSE;
				if (!playbackflg && !recmodeflg) displayspecial(0, 1);
				continue;
			}
			c = fileplaybackbuffer[fileplaybackindex++];
		}
		else if (playbackflg) {  /* keystrokes/commands come from playback array */
			if (playbackindex == playbackcount) {
				playbackflg = FALSE;
				if (!recmodeflg) displayspecial(0, 1);
				continue;
			}
			c = playbackbuffer[playbackindex++];
		}
		else {  /* keystrokes/commands come from keyboard */
			if (!cursorflg) {
				vidput(cursoron);
				cursorflg = TRUE;
			}
			vidput(cursorpos);
			do c = charget();
			while (c > VID_MAXKEYVAL || !(c = cmdtable[c]));
			if (recmodeflg && playbackcount < MAXPLAYBACK) {
				if (c != CMD_KPB) playbackbuffer[playbackcount++] = c;
				else message("Only file playbacks can be recorded");
			}
			vidput(cursoroff);
			cursorflg = FALSE;
		}
		if (msgflg) {
			msgflg = FALSE;
			keyflg = TRUE;  /* force erasure at top of loop */
		}
		if (c == CMD_TAB) c = 9;
		if (c < 256) {  /* it's a character to insert or overwrite with */
			n = horztooffset(buf->cursorhorz);
			m = (*buf->curline)->size - n;
			if (insmodeflg || !m) {
				if (chglinesize(buf->curline, (*buf->curline)->size + 1)) continue;
				if (m) memmove(&(*buf->curline)->data[n + 1], &(*buf->curline)->data[n], m);
			}
			(*buf->curline)->data[n++] = c;
			buf->cursorhorz = offsettohorz(n);
			if (buf->cursorhorz >= scolumns) {
				doframe();
				display(TRUE);
			}
			else displayline();
			setcursorhorz(buf->cursorhorz - buf->windowleftoffset);
			buf->changeflg = TRUE;
			continue;
		}
		switch (c) {  /* its a command, do it */
			case CMD_CLF: cmdleft(); continue;
			case CMD_CRT: cmdright(); continue;
			case CMD_CUP: cmdup(); continue;
			case CMD_CDN: cmddown(); continue;
			case CMD_BSP: cmdbkspc();     continue;
			case CMD_DEL: cmddelchar(); continue;
			case CMD_WLF: cmdwordleft(); continue;
			case CMD_WRT: cmdwordright(); continue;
			case CMD_LLF: cmdlineleft(); continue;
			case CMD_LRT: cmdlineright(); continue;
			case CMD_HOM: cmdhome(); continue;
			case CMD_END: cmdend(); continue;
			case CMD_WHM: cmdwinhome(); continue;
			case CMD_WEN: cmdwinend(); continue;
			case CMD_SUP: cmdscrollup(); continue;
			case CMD_SDN: cmdscrolldown(); continue;
			case CMD_PUP: cmdpageup(); continue;
			case CMD_PDN: cmdpagedown(); continue;
		}
		if (cursorflg) {
			vidput(cursoroff);
			cursorflg = FALSE;
		}
		switch (c) {
			case CMD_INS: cmdinsmode(); break;
			case CMD_KRC: cmdrecorder(); break;
			case CMD_KPB: cmdplayback(); break;
			case CMD_FRC: cmdfilerecorder(); break;
			case CMD_FPB: cmdfileplayback(FALSE); break;
			case CMD_FPA: cmdfileplayback(TRUE); break;
			case CMD_GTO: cmdgoto(); break;
			case CMD_SET: cmdoptions(); break;
			case CMD_HLP: cmdhelp(); break;
			case CMD_XIT: cmdexit(); break;
			case CMD_NXW: cmdnextwindow(); break;
			case CMD_CLW: cmdclosewindow(); break;
			case CMD_NXB: cmdnextbuffer(TRUE); break;
			case CMD_PVB: cmdnextbuffer(FALSE); break;
			case CMD_NLB: cmdbefore(); break;
			case CMD_NLA: cmdsplit(FALSE); break;
			case CMD_RET: cmdsplit(TRUE); break;
			case CMD_DLN: cmddelete(); break;
			case CMD_DLE: cmddelend(); break;
			case CMD_DBK: cmddelmarked(); break;
			case CMD_CPY: cmdcopy(); break;
			case CMD_MOV: cmdmove(); break;
			case CMD_MRK: cmdmark(); break;
			case CMD_UND: cmdundo(); break;
			case CMD_SRH: cmdsearch(0); break;
			case CMD_SRA: cmdsearch(1); break;
			case CMD_XLT: cmdtranslate(); break;
			case CMD_FRD: cmdread(NULL); break;
			case CMD_FWT: cmdwrite(); break;
			case CMD_FNM: cmdfilename(); break;
			case CMD_FWM: cmdwritemarked(); break;
			case CMD_FMG: cmdmerge(); break;
		}
	}
	setwindow[0] = VID_WIN_RESET;
	setwindow[1] = VID_BACKGROUND | VID_BLACK;
	setwindow[2] = VID_ES;
	vidput(setwindow);
	videxit();
	exit(0);
	return(0);
}

/* initialize everything */
static void initialize()
{
	INT i1, i, j, novidflag;
	CHAR work[500], cfgname[MAX_NAMESIZE], curfilename[128], *ptr;
	CHAR termdef[MAX_NAMESIZE], indevice[MAX_NAMESIZE], outdevice[MAX_NAMESIZE];
	CHAR keymap[256], ukeymap[256], lkeymap[256];
	FIOPARMS parms;
	VIDPARMS vidparms;

	/* initialize */
	cfgname[0] = '\0';
	meminitsize = 0;
	while (!argget(ARG_NEXT | ARG_IGNOREOPT, work, sizeof(work))) {
		if (work[0] == '-') {
			if (work[1] == '?') usage();
			if (toupper(work[1]) == 'A') {
				if (work[2] == '=') meminitsize = atoi(&work[3]);
			}
			else if (toupper(work[1]) == 'C' && toupper(work[2]) == 'F' &&
			    toupper(work[3]) == 'G' && work[4] == '=') strcpy(cfgname, &work[5]);
		}
	}
	if (meminitsize <= 0) meminitsize = 8192;
	if (meminit(meminitsize << 10, 0, 8192) == -1) {
		dspstring("Not enough memory\n");
		exit(1);
	}
	if (cfginit(cfgname, FALSE)) {
		dspstring(cfggeterror());
		exit(1);
	}
	if (prpinit(cfggetxml(), CFG_PREFIX "cfg")) ptr = fioinit(NULL, FALSE);
	else ptr = fioinit(&parms, FALSE);
	if (ptr != NULL) {
		dspstring("Unable to initialize fio\n");
		cfgexit();
		exit(1);
	}

	ptr = work;
	curfilename[0] = '\0';
	novidflag = 0;
	createtype = RIO_T_TXT;

	for (i = ARG_FIRST; ; i = ARG_NEXT) {
		j = argget(i, work, sizeof(work));
		if (j) {
			break;
		}
		if (i == ARG_FIRST && work[0] != '-') strcpy(curfilename, work);
		else if (work[0] == '-') {
			switch(toupper(work[1])) {
				case '!':
					break;
				case 'A':
					if (work[2] != '=' || atoi(&work[3]) <= 0) {
						dspstring("Parameter syntax is: -A=nnnn\r");
						cfgexit();
						exit(1);
					}
					break;
				case 'C':
					if (toupper(ptr[2]) == 'F' && toupper(ptr[3]) == 'G' && ptr[4] == '=' && ptr[5]); // @suppress("Suspicious semicolon")
					else createtype = RIO_T_STD;
					break;
				case 'D':
					createtype = RIO_T_DAT;
					break;
				case 'S':
					createtype = RIO_T_STD | RIO_UNC;
					break;
				case 'T':
					if (work[2] == '=') {
						strcpy(cfgname, &work[3]);
						for (i1 = 0; cfgname[i1]; i1++) cfgname[i1] = (CHAR) toupper(cfgname[i1]);
						if (!strcmp(cfgname, "BIN")) createtype = RIO_T_BIN;
						else if (!strcmp(cfgname, "CRLF")) createtype = RIO_T_DOS;
						else if (!strcmp(cfgname, "DATA")) createtype = RIO_T_DAT;
						else if (!strcmp(cfgname, "MAC")) createtype = RIO_T_MAC;
						else if (!strcmp(cfgname, "STD")) {
							if (createtype != RIO_T_STD) createtype = RIO_T_STD | RIO_UNC;
						}
						else if (!strcmp(cfgname, "TEXT")) createtype = RIO_T_TXT;
						else {
							dspstring("Invalid parameter ");
							dspstring(work);
							dspchar('\n');
							cfgexit();
							exit(1);
						}
					}
					else createtype = RIO_T_TXT;
					break;
				case 'V':
					novidflag = TRUE;
					break;
				default:
					dspstring("Invalid parameter ");
					dspstring(work);
					dspchar('\n');
					cfgexit();
					exit(1);
			}
		}
	}

	for (i = 0; i < MAXBUFFER; i++) {
		buffers[i].firstline = buffers[i].lastline = buffers[i].curline = linealloc(0);
		if (buffers[i].curline == NULL) {
			cfgexit();
			exit(5);
		}
		buffers[i].linecount = buffers[i].linenum = buffers[i].windowtopline = 1;
		if (i < 3) buffers[i].windownumber = 1;
		else if (i < 6) buffers[i].windownumber = 2;
		else buffers[i].windownumber = 0;
		buffers[i].filetype = createtype;
	}
	buf = &buffers[0];

	for (i1 = 0; i1 <= 255; i1++) keymap[i1] = (CHAR) i1;
	memcpy(ukeymap, keymap, 256);
	memcpy(lkeymap, keymap, 256);
	for (i1 = 'A'; i1 <= 'Z'; i1++) {
		ukeymap[tolower(i1)] = (CHAR) i1;
		lkeymap[i1] = (CHAR) tolower(i1);
	}

	if ((eventid = evtcreate()) == -1) {
		dspstring("Unable to initialize event id");
		cfgexit();
		exit(1);
	}
	memset(&vidparms, 0, sizeof(VIDPARMS));
	kdsConfigColorMode(&vidparms.flag);
#if OS_UNIX
	if (!prpget("edit", "xon", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) vidparms.flag |= VID_FLAG_XON;
	else vidparms.flag |= VID_FLAG_XOFF;
#endif
	if (!prpget("display", "termdef", NULL, NULL, &ptr, 0)) {
		strcpy(termdef, ptr);
		vidparms.termdef = termdef;
	}
	if (!novidflag) {
		if (!prpget("keyin", "device", NULL, NULL, &ptr, 0)) {
			strcpy(indevice, ptr);
			vidparms.indevice = indevice;
		}
		if (!prpget("display", "device", NULL, NULL, &ptr, 0)) {
			strcpy(outdevice, ptr);
			vidparms.outdevice = outdevice;
		}
	}
	if (!prpget("keyin", "translate", NULL, NULL, &ptr, 0)) {
		if (prptranslate(ptr, (UCHAR *) keymap)) {
			dspstring("Unable to read keyin translate\n");
			cfgexit();
			exit(1);
		}
		vidparms.keymap = keymap;
	}
	if (!prpget("keyin", "uppertranslate", NULL, NULL, &ptr, 0)) {
		if (prptranslate(ptr, (UCHAR *) ukeymap)) {
			dspstring("Unable to read keyin upper translate\n");
			cfgexit();
			exit(1);
		}
		vidparms.ukeymap = ukeymap;
	}
	if (!prpget("keyin", "lowertranslate", NULL, NULL, &ptr, 0)) {
		if (prptranslate(ptr, (UCHAR *) lkeymap)) {
			dspstring("Unable to read keyin lower translate\n");
			cfgexit();
			exit(1);
		}
		vidparms.lkeymap = lkeymap;
	}
	if (!prpget("display", "columns", NULL, NULL, &ptr, 0)) vidparms.numcolumns = atoi(ptr);
	if (!prpget("display", "lines", NULL, NULL, &ptr, 0)) vidparms.numlines = atoi(ptr);
	if (!prpget("keyin", "ict", NULL, NULL, &ptr, 0)) vidparms.interchartime = atoi(ptr);
	i = vidinit(&vidparms, 0);
	if (i) {
/*** CODE: MAKE SURE ERROR IS PROPERLY DISPLAYED ***/
		dspstring("Unable to initialize terminal");
		videxit();
		cfgexit();
		exit(1);
	}

	/* set up ending keys */
	memset(work, 0xFF, VID_MAXKEYVAL / 8 + 1);
	vidkeyinfinishmap((UCHAR *) work);

	/* get screen size */
	vidgetsize((INT *) &scolumns, (INT *) &slines);
	wlines = slines - 2;
	setwindow[0] = VID_WIN_TOP + 2;
	setwindow[1] = VID_WIN_BOTTOM + slines - 1;
	rdwtcfg(FALSE);
	insmodeflg = TRUE;
	updownflg = FALSE;
	if (curfilename[0]) {
		cmdread(curfilename);
		linenumchgflg = TRUE;  /* force display of name */
	}
	else {
		display(TRUE);
		linenumchgflg = dsplineinfoflg;
	}
}

/* move cursor left one character */
static void cmdleft()
{
	UINT m, n;
	UCHAR *p, *q;

	updownflg = FALSE;
	if (!buf->cursorhorz) return;
	p = (*buf->curline)->data;
	q = p + (*buf->curline)->size;
	n = m = 0;
	while (p < q) {
		if (m >= buf->cursorhorz) break;
		n = m;
		if (*p++ != 9) m++;
		else m += tablookup[m];
	}
	buf->cursorhorz = n;
	if (n < buf->windowleftoffset) {
		buf->windowleftoffset = n;
		display(FALSE);
	}
	setcursorhorz(n - buf->windowleftoffset);
}

/* move cursor right one character */
static void cmdright()
{
	UINT curhorz,cursize;
	UCHAR *p, *q;

	if (updownflg) {
		buf->cursorhorz = horztooffset(buf->cursorhorz);
		buf->cursorhorz = offsettohorz(buf->cursorhorz);
		updownflg = FALSE;
	}
	cursize = (*buf->curline)->size;
	if (!cursize) return;
	p = (*buf->curline)->data;
	q = p + cursize;
	curhorz = 0;
	while (p < q) {
		if (*p++ != 9) curhorz++;
		else curhorz += tablookup[curhorz];
		if (curhorz > buf->cursorhorz) break;
	}
	buf->cursorhorz = curhorz;
	if (curhorz >= buf->windowleftoffset + scolumns) {
		buf->windowleftoffset = curhorz - scolumns + 1;
		display(FALSE);
	}
	setcursorhorz(curhorz - buf->windowleftoffset);
}

static void cmdup()  /* move cursor up one line */
{
	UINT n;

	if (buf->linenum == 1) return;
	buf->curline = (*buf->curline)->prev;
	if (buf->linenum-- == buf->windowtopline) {
		buf->windowtopline--;
		display(FALSE);
	}
	else setcursorvert(buf->linenum - buf->windowtopline);
	n = horztohorz(buf->cursorhorz);
	if (n >= buf->windowleftoffset + scolumns) {
		buf->windowleftoffset = n - scolumns + 1;
		display(FALSE);
	}
	else if (n < buf->windowleftoffset) {
		buf->windowleftoffset = n;
		display(FALSE);
	}
	setcursorhorz(n - buf->windowleftoffset);
	linenumchgflg = dsplineinfoflg;
	updownflg = TRUE;
}

static void cmddown()  /* move cursor down one line */
{
	UINT n;

	if (buf->linenum == buf->linecount) return;
	buf->curline = (*buf->curline)->next;
	if (buf->linenum++ == buf->windowtopline + wlines - 1) {
		buf->windowtopline++;
		display(FALSE);
	}
	else setcursorvert(buf->linenum - buf->windowtopline);
	n = horztohorz(buf->cursorhorz);
	if (n >= buf->windowleftoffset + scolumns) {
		buf->windowleftoffset = n - scolumns + 1;
		display(FALSE);
	}
	else if (n < buf->windowleftoffset) {
		buf->windowleftoffset = n;
		display(FALSE);
	}
	setcursorhorz(n - buf->windowleftoffset);
	linenumchgflg = dsplineinfoflg;
	updownflg = TRUE;
}

static void cmdbkspc()
{
	updownflg = FALSE;
	if (buf->cursorhorz && (*buf->curline)->size) {
		cmdleft();
		cmddelchar();
	}
}

static void cmddelchar()
{
	INT m, n;
	UINT savelinenum;
	LINEHANDLE line, nextline;

	updownflg = FALSE;
	n = horztooffset(buf->cursorhorz);
	line = buf->curline;
	m = (*line)->size;
	if (n == m) {  /* deleting line terminator */
		if (buf->linenum == buf->linecount) {
			message("Can't delete");
			return;
		}
		if (markstatus && currentbufindex == markbufindex) {
			if (buf->beginmark == buf->linenum+1) buf->beginmark--;
		}
		buf->cursorhorz = offsettohorz(n);
		buf->curline = nextline = (*line)->next;
		savelinenum = buf->linenum++;
		n = (*nextline)->size;
		if (chglinesize(line, m + n + 1)) return;
		(*line)->data[m] = ' ';
		memcpy(&(*line)->data[m + 1], (*nextline)->data, n);
		deleteline(buf);
		memfree((UCHAR **) nextline);
		buf->curline = line;
		buf->linenum = savelinenum;
		if (buf->linenum != buf->windowtopline + wlines - 1) {
			vidput(delline);
			if (buf->linecount >= buf->windowtopline + wlines - 1) {
				gotoline(buf, buf->windowtopline + wlines - 1);
				displayline();
				buf->linenum = savelinenum;
				buf->curline = line;
			}
		}
	}
	else {  /* deleting other than line terminator */
		memmove(&(*line)->data[n], &(*line)->data[n + 1], m - n);
		chglinesize(buf->curline, m - 1);
	}
	buf->changeflg = TRUE;
	displayline();
}

static void cmdwordleft()
{
	INT n, endfileflg, startoffset;
	UINT startline;
	UCHAR c;

	updownflg = FALSE;
	endfileflg = FALSE;  /* TRUE when end of file has been encountered */
	startline = buf->linenum;
	n = startoffset = horztooffset(buf->cursorhorz) - 1;
	while (TRUE) {  /* looking for previous alphanumeric character */
		if (n == -1) {  /* skip to previous line */
			if (buf->linenum == 1) {
				endfileflg = TRUE;
				buf->linenum = buf->linecount;
				buf->curline = buf->lastline;
			}
			else {
				buf->linenum--;
				buf->curline = (*buf->curline)->prev;
			}
			n = (*buf->curline)->size - 1;
		}
		if (endfileflg && (startline > buf->linenum || (startline == buf->linenum && startoffset > n))) break;
		c = (*buf->curline)->data[n];
		if (isalnum(c)) break;
		n--;
	}
	while (n != -1) {  /* looking for previous non-alphanumeric character */
		if (endfileflg && (startline > buf->linenum || (startline == buf->linenum && startoffset > n))) break;
		c = (*buf->curline)->data[n];
		if (!isalnum(c)) break;
		n--;
	}
	buf->cursorhorz = offsettohorz(n + 1);
	setcursorhorz(buf->cursorhorz - buf->windowleftoffset);
	doframe();
	display(FALSE);
	linenumchgflg = dsplineinfoflg;
}

static void cmdwordright()
{
	INT m, n, endfileflg, startoffset;
	UINT startline;
	UCHAR c;

	updownflg = FALSE;
	endfileflg = FALSE;  /* TRUE when end of file has been encountered */
	startline = buf->linenum;
	n = startoffset = horztooffset(buf->cursorhorz);
	m = (*buf->curline)->size;
	if (n < m) {
		while (TRUE) {  /* skip over one word if starting point is in it */
			c = (*buf->curline)->data[n++];
			if (!isalnum(c) || m == n)  break;
		}
	}
	while (TRUE) {  /* looking for next alphanumeric character */
		if (n >= m) {  /* skip to next line */
			if (buf->linenum == buf->linecount) {
				endfileflg = TRUE;
				buf->linenum = 1;
				buf->curline = buf->firstline;
			}
			else {
				buf->linenum++;
				buf->curline = (*buf->curline)->next;
			}
			n = 0;
			m = (*buf->curline)->size;
		}
		if (endfileflg && (startline < buf->linenum || (startline == buf->linenum && startoffset < n))) break;
		c = (*buf->curline)->data[n];
		if (isalnum(c)) break;
		n++;
	}
	buf->cursorhorz = offsettohorz(n);
	setcursorhorz(buf->cursorhorz - buf->windowleftoffset);
	doframe();
	display(FALSE);
	linenumchgflg = dsplineinfoflg;
}

static void cmdlineleft()
{
	buf->cursorhorz = 0;
	if (buf->windowleftoffset) {
		buf->windowleftoffset = 0;
		display(FALSE);
	}
	setcursorhorz(0);
}

static void cmdlineright()
{
	buf->cursorhorz = offsettohorz((*buf->curline)->size);
	doframe();
	display(FALSE);
	setcursorhorz(buf->cursorhorz - buf->windowleftoffset);
}

static void cmdhome()
{
	buf->curline = buf->firstline;
	buf->linenum = 1;
	buf->cursorhorz = 0;
	doframe();
	display(FALSE);
	setcursorhorz(0);
	setcursorvert(0);
	linenumchgflg = dsplineinfoflg;
	updownflg = FALSE;
}

static void cmdend()
{
	buf->curline = buf->lastline;
	buf->linenum = buf->linecount;
	buf->cursorhorz = 0;
	doframe();
	display(FALSE);
	setcursorhorz(0);
	if (buf->linenum >= wlines) setcursorvert(wlines - 1);
	else setcursorvert(buf->linenum - 1);
	linenumchgflg = dsplineinfoflg;
}

static void cmdwinhome()
{
	while (buf->linenum != buf->windowtopline) {
		buf->curline = (*buf->curline)->prev;
		buf->linenum--;
	}
	buf->cursorhorz = 0;
	if (buf->windowleftoffset) buf->windowleftoffset = 0;
	display(FALSE);
	setcursorhorz(0);
	setcursorvert(0);
	linenumchgflg = dsplineinfoflg;
}

static void cmdwinend()
{
	while (buf->linenum != buf->linecount && buf->linenum != buf->windowtopline + wlines - 1) {
		buf->curline = (*buf->curline)->next;
		buf->linenum++;
	}
	buf->cursorhorz = 0;
	if (buf->windowleftoffset) buf->windowleftoffset = 0;
	display(FALSE);
	setcursorhorz(0);
	if (buf->linenum - buf->windowtopline + 1 >= wlines) setcursorvert(wlines - 1);
	else setcursorvert(buf->linenum - buf->windowtopline);
	linenumchgflg = dsplineinfoflg;
}

static void cmdscrollup()
{
	if (buf->windowtopline == 1) return;
	buf->curline = (*buf->curline)->prev;
	buf->linenum--;
	buf->windowtopline--;
	display(FALSE);
	setcursorhorz(horztohorz(buf->cursorhorz) - buf->windowleftoffset);
	updownflg = TRUE;
}

static void cmdscrolldown()
{
	if (buf->windowtopline + wlines > buf->linecount) return;
	buf->curline = (*buf->curline)->next;
	buf->linenum++;
	buf->windowtopline++;
	display(FALSE);
	setcursorhorz(horztohorz(buf->cursorhorz) - buf->windowleftoffset);
	updownflg = TRUE;
}

static void cmdpageup()
{
	INT n;
	UINT curhorz;

	n = wlines;
	while (n-- && buf->windowtopline != 1) {
		buf->curline = (*buf->curline)->prev;
		buf->windowtopline--;
		buf->linenum--;
	}
	display(FALSE);
	curhorz = horztohorz(buf->cursorhorz);
	if (curhorz >= buf->windowleftoffset + scolumns) {
		buf->windowleftoffset = curhorz - scolumns + 1;
		display(FALSE);
	}
	else if (curhorz < buf->windowleftoffset) {
		buf->windowleftoffset = curhorz;
		display(FALSE);
	}
	setcursorhorz(curhorz - buf->windowleftoffset);
	linenumchgflg = dsplineinfoflg;
}

static void cmdpagedown()
{
	INT n;
	UINT curhorz;

	n = wlines;
	while (n-- && buf->windowtopline + wlines <= buf->linecount) {
		buf->curline = (*buf->curline)->next;
		buf->windowtopline++;
		buf->linenum++;
	}
	display(FALSE);
	curhorz = horztohorz(buf->cursorhorz);
	if (curhorz >= buf->windowleftoffset + scolumns) {
		buf->windowleftoffset = curhorz - scolumns + 1;
		display(FALSE);
	}
	else if (curhorz < buf->windowleftoffset) {
		buf->windowleftoffset = curhorz;
		display(FALSE);
	}
	setcursorhorz(curhorz - buf->windowleftoffset);
	linenumchgflg = dsplineinfoflg;
}

static void cmdinsmode()
{
	insmodeflg = !insmodeflg;
	if (!insmodeflg) displayspecial('O', 0);  /* overstrike mode */
	else displayspecial(0, 0);  /* insert mode */
}

static void cmdplayback()  /* playback the main recorder keystrokes */
{
	if (recmodeflg) {
		message("No playback in record mode");
		return;
	}
	playbackflg = TRUE;
	playbackindex = 0;
	displayspecial('P', 1);
}

static void cmdrecorder()  /* turn the main keystroke recorder on or off */
{
	recmodeflg = !recmodeflg;
	if (recmodeflg) {  /* recorder on */
		playbackcount = 0;
		displayspecial('R', 1);
	}
	else {  /* recorder off */
		playbackcount--;
		displayspecial(0, 1);
	}
}

static void cmdfilerecorder()  /* write the main recorder keystrokes to a file */
{
	INT filenum;
	static CHAR rcfilename[128];

	strcpy(keyinstring, rcfilename);
	if (keyin("File name to write recorded keystrokes to: ", KEYIN_STRING) < 1) return;
	strcpy(rcfilename, keyinstring);
	miofixname(rcfilename, ".rec", FIXNAME_EXT_ADD);
	filenum = fioopen(rcfilename, FIO_M_PRP | FIO_P_CFG);
	if (filenum < 1) {
		message("Unable to create keystroke recorder file");
		return;
	}
	if (fiowrite(filenum, 0L, (UCHAR *) playbackbuffer, playbackcount * sizeof(USHORT)) >= 0) message("Written");
	else message("Unable to write keystroke recorder file");
	fioclose(filenum);
}

/* playback keystrokes from a keystroke file */
/* againflg TRUE if file playback again */
static void cmdfileplayback(INT againflg)
{
	INT n, filenum;
	static CHAR pbfilename[128];

	if (fileplaybackflg) return;
	if (!againflg) {
		strcpy(keyinstring, pbfilename);
		if (keyin("Playback file name: ", KEYIN_STRING) < 1) return;
		fileplaybackcount = 0;
		strcpy(pbfilename, keyinstring);
		miofixname(pbfilename, ".rec", FIXNAME_EXT_ADD);
		filenum = fioopen(pbfilename, FIO_M_ERO | FIO_P_CFG);
		if (filenum < 1) {
			message("Unable to open keystroke playback file");
			return;
		}
		n = fioread(filenum, 0L, (UCHAR *) fileplaybackbuffer, MAXPLAYBACK * sizeof(USHORT));
		if (n < 0) message("Unable to read keystroke playback file");
		else fileplaybackcount = n / sizeof(USHORT);
		fioclose(filenum);
	}
	if (!fileplaybackcount) message("Nothing to play back");
	else {
		fileplaybackindex = 0;
		fileplaybackflg = TRUE;
	}
}

static void cmdgoto()
{
	static CHAR lastgoto[128];

	strcpy(keyinstring, lastgoto);
	if (keyin("Line number: ", KEYIN_NUMBER) < 1) return;
	while (TRUE) {
		if (keyinvalue >= 1 && keyinvalue <= (INT) buf->linecount) break;
		if (keyin("Bad line number (0 or too large) - Line number: ", KEYIN_NUMBER) < 1) return;
	}
	strcpy(lastgoto, keyinstring);
	gotoline(buf, keyinvalue);
	buf->cursorhorz = 0;
	doframe();
	display(FALSE);
	linenumchgflg = dsplineinfoflg;
}

static void cmdoptions()
{
#if OS_WIN32
	static INT32 arrow[] = { VID_DISP_SYM + VID_SYM_HLN, VID_DISP_SYM + VID_SYM_RTA, VID_FINISH };
#else
	static INT32 arrow[] = { VID_DISP_CHAR + '-', VID_DISP_CHAR + '>', VID_FINISH };
#endif
	static INT32 blanks[] = { VID_DSP_BLANKS + 2, VID_FINISH };
	static CHAR *chg = " CHG ";
	static CHAR *set = "Set EDIT Configuration";
	INT option;    /* option number */
	INT nextoption;  /* option number for first option after this page */
	INT page;      /* page number */
	INT line;      /* window line number (0 = top) */
	INT lpp;       /* lines per page */
	INT i, n;
	INT32 vidcmds[15];
	CHAR *ptr;

	recmodeflg = FALSE;
	vidcmds[0] = VID_WIN_RESET;
	vidcmds[1] = VID_FOREGROUND | VID_WHITE;
	vidcmds[2] = VID_BACKGROUND | VID_BLACK;
	vidcmds[3] = VID_BOLD_OFF;
	vidcmds[4] = VID_ES;
	vidcmds[5] = VID_VERT + 1;
	vidcmds[6] = VID_REPEAT + 80;
	vidcmds[7] = VID_DISP_SYM + VID_SYM_HLN;
	vidcmds[8] = VID_HORZ + 27;
	vidcmds[9] = VID_VERT + 2;
	vidcmds[10] = VID_DISPLAY + (INT32)strlen(set);
	i = 11;
	if (sizeof(void *) > sizeof(INT32)) {
		memcpy((void *) &vidcmds[i], (void *) &set, sizeof(void *));
		i += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
	}
	else *(UCHAR **)(&vidcmds[i++]) = (UCHAR *) set;
	vidcmds[i] = VID_FINISH;
	vidput(vidcmds);
	setwindow[0] = VID_WIN_TOP + 3;
	setwindow[1] = VID_WIN_BOTTOM + slines - 1;
	vidput(setwindow);
	page = 0;
	lpp = slines - 3;
newpage:
	vidput(erasewindow);
	for (nextoption = lpp * page, line = 0; line < lpp && options[nextoption].text != NULL; nextoption++, line++) {
		displayoptionline(nextoption, line);
		if (!page && line < MAXBUFFER) {  /* its a buffer line */
			if (buffers[line].changeflg) {
				vidcmds[0] = VID_DISPLAY + (INT32)strlen(chg);
				i = 1;
				if (sizeof(void *) > sizeof(INT32)) {
					memcpy((void *) &vidcmds[i], (void *) &chg, sizeof(void *));
					i += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
				}
				else *(UCHAR **)(&vidcmds[i++]) = (UCHAR *) chg;
			}
			else {
				vidcmds[0] = VID_DSP_BLANKS + (INT32)strlen(chg);
				i = 1;
			}
			vidcmds[i++] = VID_DISPLAY + (INT32)strlen(buffers[line].filename);
			if (sizeof(UCHAR *) > sizeof(INT32)) {
				ptr = buffers[line].filename;
				memcpy((void *) &vidcmds[i], (void *) &ptr, sizeof(void *));
				i += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
			}
			else *(UCHAR **)(&vidcmds[i++]) = (UCHAR *) buffers[line].filename;
			vidcmds[i] = VID_FINISH;
			vidput(vidcmds);
		}
	}
	setcursorhorz(0);
	option = lpp * page;
	line = 0;
	while (TRUE) {
		setcursorvert(line);
		vidput(cursorpos);
		vidput(arrow);

		keyin("up(U), down(D), page(P), change(C), exit(X)", KEYIN_KEYSTROKE);
		if (keyinvalue < 32 || keyinvalue > 126) continue;
		keyinvalue = toupper(keyinvalue);
		switch (keyinvalue) {
			case 'U':
				if (line) {
					option--;
					line--;
				}
				break;
			case 'D':
				if (line + 1 < lpp && option + 1 != nextoption) {
					option++;
					line++;
				}
				break;
			case 'P':
				if (options[nextoption].text == NULL) page = 0;
				else page++;
				goto newpage;
			case 'C':
				n = options[option].type;
				if (n > 600) {
					keyin("Change the key for a command - press the new key", KEYIN_KEYSTROKE);
					options[option].value = keyinvalue;
				}
				switch (n) {
					case OPT_TABDSP:
					case OPT_FILECOMP:
					case OPT_CASESENSE:
					case OPT_BRIGHT:
					case OPT_LINENUM:
						while (TRUE) {
							keyin("Change configuration feature - press Y or N", KEYIN_KEYSTROKE);
							keyinvalue = toupper(keyinvalue);
							if (keyinvalue == 'N') options[option].value = 0;
							else if (keyinvalue == 'Y') options[option].value = 1;
							else continue;
							break;
						}
						break;
					case OPT_WINSPLIT:
						do keyin("Change window split line number: ", KEYIN_NUMBER);
						while (keyinvalue < 4 && keyinvalue > (INT) slines - 3);
						options[option].value = keyinvalue;
						break;
					case OPT_TABSET:
						while (TRUE) {
							strcpy(keyinstring, tabset);
							keyin("Change tab setting (nn:nn or nn,nn,...): ", KEYIN_STRING);
							if (!edittab(keyinstring)) break;
							tabset[0] = 0;
						}
						break;
					case OPT_TABDSPCHAR:
						do keyin("Enter the new TAB display character: ", KEYIN_KEYSTROKE);
						while (!keyinvalue || keyinvalue > 255);
						options[option].value = keyinvalue;
						break;
					case OPT_FGCOLOR:
					case OPT_BGCOLOR:
						do {
							strcpy(keyinstring, colors[options[option].value]);
							keyin("Change the new color: ", KEYIN_STRING);
							for (n = 0; keyinstring[n]; n++) keyinstring[n] = (UCHAR) toupper(keyinstring[n]);
							for (n = 0; n < 8 && strcmp(keyinstring, colors[n]); n++);
						} while (n == 8);
						options[option].value = n;
						break;
					case OPT_BACKEXT:
						do {
							strcpy(keyinstring, backext);
							keyin("Change backup file extension - enter 1 to 3 chars: ", KEYIN_STRING3);
						} while (!keyinstring[0]);
						strcpy(backext, keyinstring);
						break;
					case OPT_SRCHFORCE:
					case OPT_SRCHMATCH:
					case OPT_SRCHLEFTANCHOR:
					case OPT_SRCHRIGHTANCHOR:
					case OPT_TRANINSERT:
						do keyin("Enter the new search/translate special character: ", KEYIN_KEYSTROKE);
						while (keyinvalue < 32 || keyinvalue > 255);
						options[option].value = keyinvalue;
						break;
					case OPT_KEYEND:
					case OPT_KEYABORT:
					case OPT_KEYBKSPC:
					case OPT_KEYRECALL:
						keyin("Change response line key - press the new key", KEYIN_KEYSTROKE);
						if (n == OPT_KEYEND) keyend = n;
						else if (n == OPT_KEYABORT) keyabort = n;
						else if (n == OPT_KEYBKSPC) keybkspc = n;
						else keyrecall = n;
						break;
					case OPT_BUFFER1:
					case OPT_BUFFER2:
					case OPT_BUFFER3:
					case OPT_BUFFER4:
					case OPT_BUFFER5:
					case OPT_BUFFER6:
					case OPT_BUFFER7:
					case OPT_BUFFER8:
					case OPT_BUFFER9:
						do keyin("Change display window for a buffer (0, 1 or 2): ", KEYIN_NUMBER);
						while (!keyinstring[0] || keyinvalue > 2);
						options[option].value = keyinvalue;
						break;
				}
				displayoptionline(option, line);
				break;
			case 'X':
				rdwtcfg(TRUE);  /* write the config file */
				buf->cursorhorz = 0;
				windowstatus = 1;
				currentbufindex = MAXBUFFER - 1;
				cmdnextbuffer(TRUE);
				cmdclosewindow();
				doframe();
				display(TRUE);
				linenumchgflg = TRUE;
				return;
		}
		vidput(cursorpos);
		vidput(blanks);
	}
}

/* display the text of an option */
static void displayoptionline(INT option, INT line)
{
	static INT32 dspline[14] = { VID_HORZ, VID_VERT, VID_DISPLAY + 40 };
	INT i, k;
	CHAR work10[10], *ptr;

	dspline[1] = VID_VERT + line;
	i = 3;
	if (sizeof(void *) > sizeof(INT32)) {
		memcpy((void *) &dspline[i], (void *) &options[option].text, sizeof(void *));
		i += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
	}
	else *(UCHAR **)(&dspline[i++]) = (UCHAR *) options[option].text;
	k = options[option].type;
	if (k > 600) ptr = keytext(options[option].value);
	else {
		switch (k) {
			case OPT_TABDSP:
			case OPT_FILECOMP:
			case OPT_CASESENSE:
			case OPT_BRIGHT:
			case OPT_LINENUM:
				if (options[option].value) ptr = "YES";
				else ptr = "NO";
				break;
			case OPT_WINSPLIT:
				mscitoa(options[option].value, work10);
				ptr = work10;
				break;
			case OPT_TABSET:
				ptr = tabset;
				break;
			case OPT_FGCOLOR:
			case OPT_BGCOLOR:
				ptr = colors[options[option].value];
				break;
			case OPT_BACKEXT:
				ptr = backext;
				break;
			case OPT_TABDSPCHAR:
			case OPT_SRCHFORCE:
			case OPT_SRCHMATCH:
			case OPT_SRCHLEFTANCHOR:
			case OPT_SRCHRIGHTANCHOR:
			case OPT_TRANINSERT:
			case OPT_KEYEND:
			case OPT_KEYABORT:
			case OPT_KEYBKSPC:
			case OPT_KEYRECALL:
				ptr = keytext(options[option].value);
				break;
			case OPT_BUFFER1:
			case OPT_BUFFER2:
			case OPT_BUFFER3:
			case OPT_BUFFER4:
			case OPT_BUFFER5:
			case OPT_BUFFER6:
			case OPT_BUFFER7:
			case OPT_BUFFER8:
			case OPT_BUFFER9:
				work10[0] = (CHAR)(options[option].value + '0');
				work10[1] = 0;
				ptr = work10;
				break;
			default:
				ptr = "";
		}
	}
	dspline[i++] = VID_DISPLAY + (INT32)strlen(ptr);
	if (sizeof(void *) > sizeof(INT32)) {
		memcpy((void *) &dspline[i], (void *) &ptr, sizeof(void *));
		i += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
	}
	else *(UCHAR **)(&dspline[i++]) = (UCHAR *) ptr;
	dspline[i++] = VID_EL;
	dspline[i] = VID_FINISH;
	vidput(dspline);
}

static void cmdhelp()
{
	message("No help available");
}

static void cmdexit()
{
	INT i;

	for (i = 0; i < MAXBUFFER; i++) {
		if (buffers[i].changeflg) {
			keyin("OK to lose changes? ", KEYIN_YESNO);
			if (keyinstring[0] == 'Y') break;
			return;
		}
	}
	quitflg = TRUE;
}

static void cmdnextwindow()
{
	INT i;
	USHORT top, bottom;
	INT32 vidcmds[5];

	if (windowstatus == 2) {
		top = 2;
		bottom = windowsplitline - 2;
		currentbufindex = window1bufindex;
	}
	else {
		if (!windowstatus) {
			for (i = 0; i < MAXBUFFER && buffers[i].windownumber != 2; i++);
			if (i == MAXBUFFER) {
				message("There is no buffer associated with window 2");
				return;
			}
			window2bufindex = i;
			vidcmds[0] = VID_HORZ;
			vidcmds[1] = VID_VERT + windowsplitline - 3;
			vidcmds[2] = VID_REPEAT + 80;
			vidcmds[3] = VID_DISP_SYM + VID_SYM_HLN;
			vidcmds[4] = VID_FINISH;
			vidput(vidcmds);
		}
		top = windowsplitline;
		bottom = slines - 1;
		currentbufindex = window2bufindex;
	}
	wlines = bottom - top + 1;
	setwindow[0] = VID_WIN_TOP + top;
	setwindow[1] = VID_WIN_BOTTOM + bottom;
	vidput(setwindow);
	buf = &buffers[currentbufindex];
	doframe();
	display(TRUE);
	if (windowstatus == 2) windowstatus = 1;
	else windowstatus = 2;
	linenumchgflg = TRUE;
}

static void cmdclosewindow()
{
	if (windowstatus) {
		windowstatus = 0;
		wlines = slines - 2;
		setwindow[0] = VID_WIN_TOP + 2;
		setwindow[1] = VID_WIN_BOTTOM + slines - 1;
		vidput(setwindow);
		currentbufindex = window1bufindex;
		buf = &buffers[currentbufindex];
		display(TRUE);
		setcursorhorz(horztohorz(buf->cursorhorz) - buf->windowleftoffset);
		setcursorvert(buf->linenum - buf->windowtopline);
		linenumchgflg = TRUE;
	}
}

/* change to next or previous buffer */
/* TRUE if next buffer, FALSE if previous buffer */
static void cmdnextbuffer(INT nextflg)
{
	INT i;
	UINT n;

	if (windowstatus == 2) n = 2;
	else n = 1;
	i = currentbufindex;
	while (TRUE) {
		if (nextflg) {
			if (++i == MAXBUFFER) i = 0;
		}
		else if (!i--) i = MAXBUFFER - 1;
		if (buffers[i].windownumber == n) {
			if (windowstatus == 2) window2bufindex = i;
			else window1bufindex = i;
			currentbufindex = i;
			buf = &buffers[currentbufindex];
			doframe();
			display(TRUE);
			break;
		}
		if (i == currentbufindex) {
			if (buffers[i].windownumber != n) message("There are no buffers associated with the primary window");
			break;
		}
	}
	linenumchgflg = TRUE;
}

static void cmdbefore()
{
	LINEHANDLE line, newline;

	if ((newline = linealloc(0)) == NULL) return;
	(*newline)->next = line = buf->curline;
	if (buf->linenum == 1) {
		(*newline)->prev = NULL;
		buf->firstline = newline;
		(*line)->prev = newline;
	}
	else {
		(*newline)->prev = (*line)->prev;
		(*(*line)->prev)->next = newline;
		(*line)->prev = newline;
	}
	buf->curline = newline;
	buf->linecount++;
	if (markstatus && markbufindex == currentbufindex) {
		if (buf->linenum <= buf->beginmark) buf->beginmark++;
		if (markstatus == 2 && buf->linenum <= buf->endmark) buf->endmark++;
	}
	buf->cursorhorz = 0;
	setcursorhorz(0);
	if (buf->windowleftoffset) {
		buf->windowleftoffset = 0;
		display(TRUE);
	}
	else {
		vidput(insline);
		if (markstatus == 2 && markbufindex == currentbufindex &&
			buf->linenum >= buf->beginmark && buf->linenum <= buf->endmark)
			displayline();
	}
	buf->changeflg = TRUE;
	linenumchgflg = dsplineinfoflg;
}

static void cmdsplit(INT splitflg)  /* after and split commands */
{
	INT m, n, mrkrevflg;
	LINEHANDLE line, newline;

	mrkrevflg = FALSE;
	if (splitflg) {
		n = horztooffset(buf->cursorhorz);
		m = (*buf->curline)->size - n;
	}
	else m = 0;
	if ((newline = linealloc(m)) == NULL) return;
	(*newline)->prev = line = buf->curline;
	if (buf->linenum == buf->linecount) {
		(*newline)->next = NULL;
		buf->lastline = newline;
		(*line)->next = newline;
	}
	else {
		(*newline)->next = (*line)->next;
		(*(*line)->next)->prev = newline;
		(*line)->next = newline;
	}
	if (m) {
		memcpy((*newline)->data, &(*line)->data[n], m);
		if (chglinesize(buf->curline, n)) return;
	}
	buf->curline = newline;
	buf->linecount++;
	if (markstatus && markbufindex == currentbufindex) {
		if (buf->linenum < buf->beginmark) buf->beginmark++;
		if (markstatus == 2 && buf->linenum < buf->endmark) {
			buf->endmark++;
			if (buf->linenum >= buf->beginmark) mrkrevflg = TRUE;
		}
	}
	if (buf->windowleftoffset) {
		buf->linenum++;
		buf->cursorhorz = 0;
		setcursorhorz(0);
		buf->windowleftoffset = 0;
		if (buf->linenum == buf->windowtopline + wlines) buf->windowtopline++;
		display(TRUE);
	}
	else {
		if (m) {
			if (playbackflg || fileplaybackflg) {
				setcursorhorz(horztohorz(buf->cursorhorz) - buf->windowleftoffset);
				setcursorvert(buf->linenum - buf->windowtopline);
				vidput(cursorpos);
			}
			if (mrkrevflg) vidput(reveraseline);
			else vidput(eraseline);
		}
		buf->linenum++;
		buf->cursorhorz = 0;
		setcursorhorz(0);
		if (buf->linenum == buf->windowtopline + wlines) {
			buf->windowtopline++;
			vidput(scrollup);
		}
		else {
			setcursorvert(buf->linenum - buf->windowtopline);
			vidput(cursorpos);
			vidput(insline);
		}
		displayline();
	}
	setcursorvert(buf->linenum - buf->windowtopline);
	buf->changeflg = TRUE;
	linenumchgflg = dsplineinfoflg;
}

static void cmddelete()
{
	UINT savelinenum;
	LINEHANDLE line;

	if (buf->linecount == 1) {
		if (!((*buf->curline)->size)) {
			message("Can't delete");
			return;
		}
	}
	buf->cursorhorz = 0;
	if (buf->windowleftoffset) {
		buf->windowleftoffset = 0;
		display(FALSE);
	}
	pushundostack(buf->curline);
	deleteline(buf);
	vidput(cursorpos);
	vidput(delline);
	if (buf->linecount >= buf->windowtopline + wlines - 1) {
		line = buf->curline;
		savelinenum = buf->linenum;
		gotoline(buf, buf->windowtopline + wlines - 1);
		displayline();
		buf->curline = line;
		buf->linenum = savelinenum;
	}
	setcursorhorz(0);
	setcursorvert(buf->linenum - buf->windowtopline);
	buf->changeflg = TRUE;
	linenumchgflg = dsplineinfoflg;
}

static void cmddelend()
{
	INT m, n;
	LINEHANDLE line;
	
	n = horztooffset(buf->cursorhorz);
	m = (*buf->curline)->size - n;
	if ((line = linealloc(m)) == NULL) return;
	memcpy((*line)->data, &(*buf->curline)->data[n], m);
	pushundostack(line);
	chglinesize(buf->curline, n);
	displayline();
	buf->changeflg = TRUE;
}

static void cmddelmarked()
{
	INT m, n, displayflg;

	if (markstatus != 2) {
		message("Nothing marked");
		return;
	}
	markstatus = 0;
	m = currentbufindex;
	displayflg = !setcurrentbufwin(markbufindex);
	gotoline(buf, buf->beginmark);
	n  = buf->endmark - buf->beginmark + 1;
	while (n--) deleteline(buf);
	if (buf->windowleftoffset) buf->windowleftoffset = 0;
	buf->cursorhorz = 0;
	buf->changeflg = TRUE;
	if (displayflg) {
		doframe();
		display(TRUE);
		linenumchgflg = dsplineinfoflg;
	}
	if (m != currentbufindex) {
		setcurrentbufwin(m);
		setcursorhorz(horztohorz(buf->cursorhorz) - buf->windowleftoffset);
		setcursorvert(buf->linenum - buf->windowtopline);
	}
}

static void cmdcopy()
{
	INT k, m, n, savelinenum;
	LINEHANDLE line, fromline, saveline;

	if (markstatus != 2) {
		message("Nothing marked");
		return;
	}
	if (markbufindex == currentbufindex && buf->linenum >= buf->beginmark && buf->linenum < buf->endmark) {
		message("Bad target");
		return;
	}
	buf->cursorhorz = 0;
	setcursorhorz(0);
	if (buf->windowleftoffset) {
		buf->windowleftoffset = 0;
		display(FALSE);
	}
	k = buf->linenum + 1;
	saveline = buffers[markbufindex].curline;
	savelinenum = buffers[markbufindex].linenum;
	gotoline(&buffers[markbufindex], buffers[markbufindex].beginmark);
	n = buffers[markbufindex].endmark - buffers[markbufindex].beginmark + 1;
	fromline = buffers[markbufindex].curline;
	buffers[markbufindex].curline = saveline;
	buffers[markbufindex].linenum = savelinenum;
	while (n--) {
		m = ((*fromline)->size);
		if ((line = linealloc(m)) == NULL) return;
		memcpy((*line)->data, (*fromline)->data, m);
		insertline(buf, line);
		if (buf->linenum < buf->windowtopline + wlines) {
			setcursorvert(buf->linenum - buf->windowtopline);
			vidput(cursorpos);
			vidput(insline);
			displayline();
		}
		fromline = (*fromline)->next;
	}
	gotoline(buf, k);
	if (buf->linenum == buf->windowtopline + wlines) {
		buf->windowtopline++;
		display(FALSE);
	}
	setcursorvert(buf->linenum - buf->windowtopline);
	buf->changeflg = TRUE;
	linenumchgflg = dsplineinfoflg;
}

static void cmdmove()
{
	INT m, n, sameflg;
	UINT firstmark, lastmark, tolinenum;
	BUFFERPTR markbuf;
	LINEHANDLE line, toline, endmarkline, bgnmarkline;

	if (markstatus != 2) {
		message("Nothing marked");
		return;
	}
	if (markbufindex == currentbufindex && buf->linenum >= buf->beginmark && buf->linenum <= buf->endmark) {
		message("Bad target");
		return;
	}
	if (markbufindex == currentbufindex) sameflg = TRUE;
	else sameflg = FALSE;
	markbuf = &buffers[markbufindex];
	markstatus = 0;
	toline = buf->curline;
	tolinenum = buf->linenum;
	n = markbuf->endmark - markbuf->beginmark + 1;
	firstmark = tolinenum + 1;
	lastmark = tolinenum + n;
	if (sameflg && tolinenum > buf->endmark) {
		firstmark -= n;
		lastmark -= n;
		tolinenum -= n;
		if ((UINT) n < buf->windowtopline) buf->windowtopline -= n;
		else buf->windowtopline = 1;
	}
	gotoline(markbuf, markbuf->beginmark);
	bgnmarkline = markbuf->curline;
	gotoline(markbuf, markbuf->endmark);
	endmarkline = markbuf->curline;
	if ((*bgnmarkline)->prev == NULL) {
		markbuf->curline = markbuf->firstline = (*endmarkline)->next;
		markbuf->linenum++;
	}
	else {
		(*(*bgnmarkline)->prev)->next = (*endmarkline)->next;
		markbuf->curline = (*bgnmarkline)->prev;
	}
	if ((*endmarkline)->next == NULL) markbuf->curline = markbuf->lastline = (*bgnmarkline)->prev;
	else (*(*endmarkline)->next)->prev = (*bgnmarkline)->prev;
	(*bgnmarkline)->prev = toline;
	(*endmarkline)->next = (*toline)->next;
	if ((*toline)->next == NULL) buf->lastline = endmarkline;
	else (*(*toline)->next)->prev = endmarkline;
	(*toline)->next = bgnmarkline;
	if (!sameflg) {
		buf->linecount += n;
		markbuf->linecount -= n;
		markbuf->linenum -= n;
	}
	if (markbuf->firstline == NULL) {
		markbuf->linenum = markbuf->linecount = markbuf->windowtopline = 1;
		markbuf->windowleftoffset = markbuf->cursorhorz = 0;
		if ((line = linealloc(0)) == NULL) return;
		markbuf->changeflg = TRUE;
		markbuf->curline = line;
		markbuf->firstline = markbuf->lastline = markbuf->curline;
		(*markbuf->curline)->next = (*markbuf->curline)->prev = NULL;
	}
	m = markbufindex;
	markbufindex = currentbufindex;
	markstatus = 2;
	if (!setcurrentbufwin(m) && !sameflg) {
		doframe();
		display(TRUE);
	}
	buf->changeflg = TRUE;
	setcurrentbufwin(markbufindex);
	buf->beginmark = firstmark;
	buf->endmark = lastmark;
	buf->curline = toline;
	buf->linenum = tolinenum;
	gotoline(buf, buf->beginmark);
	buf->cursorhorz = 0;
	setcursorhorz(0);
	if (buf->linenum == buf->windowtopline + wlines) buf->windowtopline++;
	setcursorvert(buf->linenum - buf->windowtopline);
	doframe();
	display(TRUE);
	buf->changeflg = TRUE;
	linenumchgflg = dsplineinfoflg;
}

static void cmdmark()
{
	UINT savelinenum, visibleend;
	INT savecurrentbuf;

	if (markstatus == 2) {
		markstatus = 0;
		savecurrentbuf = currentbufindex;
		if (!setcurrentbufwin(markbufindex)) {
			if (buf->beginmark < buf->windowtopline + wlines && buf->endmark >= buf->windowtopline) {
				savelinenum = buf->linenum;
				if (buf->windowtopline > buf->beginmark) gotoline(buf, buf->windowtopline);
				else gotoline(buf, buf->beginmark);
				while (buf->linenum < buf->windowtopline + wlines && buf->linenum <= buf->endmark) {
					displayline();
					if (buf->linenum == buf->linecount) break;
					buf->linenum++;
					buf->curline = (*buf->curline)->next;
				}
				gotoline(buf, savelinenum);
			}
		}
		setcurrentbufwin(savecurrentbuf);
		message("Block marks cleared");
	}
	else if (markstatus == 0 || (markstatus == 1 && markbufindex != currentbufindex)) {
		markstatus = 1;
		buf->beginmark = buf->linenum;
		markbufindex = currentbufindex;
		message("First mark set");
	}
	else {
		markstatus = 2;
		savelinenum = buf->linenum;
		if (buf->linenum < buf->beginmark) {
			buf->endmark = buf->beginmark;
			buf->beginmark = buf->linenum;
		}
		else buf->endmark = buf->linenum;
		if (buf->windowtopline > buf->beginmark) gotoline(buf, buf->windowtopline);
		else if (buf->beginmark != buf->linenum) gotoline(buf, buf->beginmark);
		visibleend = buf->windowtopline + wlines - 1;
		if (visibleend > buf->endmark) visibleend = buf->endmark;
		while (TRUE) {
			displayline();
			if (buf->linenum == visibleend) break;
			buf->curline = (*buf->curline)->next;
			buf->linenum++;
		}
		if (buf->linenum != savelinenum) gotoline(buf, savelinenum);
		message("Block is marked");
	}
}

static void cmdundo()
{
	LINEHANDLE line;

	if (undostacktop == undostacktail) {
		message("Empty undo stack");
		return;
	}
	line = undostack[undostacktop];
	if (!undostacktop--) undostacktop = MAXUNDO - 1;
	if (buf->linenum == 1) {
		(*line)->prev = NULL;
		buf->firstline = line;
	}
	else {
		(*line)->prev = (*buf->curline)->prev;
		(*(*buf->curline)->prev)->next = line;
	}
	(*buf->curline)->prev = line;
	(*line)->next = buf->curline;
	buf->curline = line;
	buf->linecount++;
	if (markstatus && markbufindex == currentbufindex) {
		if (buf->linenum <= buf->beginmark) buf->beginmark++;
		if (markstatus == 2 && buf->linenum <= buf->endmark) buf->endmark++;
	}
	buf->cursorhorz = 0;
	if (buf->windowleftoffset) {
		buf->windowleftoffset = 0;
		display(TRUE);
	}
	else {
		vidput(insline);
		displayline();
	}
	setcursorhorz(0);
	buf->changeflg = TRUE;
	linenumchgflg = dsplineinfoflg;
}

static void cmdsearch(INT againflg)  /* search and search again */
{
	static CHAR searchstring[128];

	if (!againflg) {
		strcpy(keyinstring, searchstring);
		if (keyin("Search pattern: ", KEYIN_TABSTRING) < 1) return;
		strcpy(searchstring, keyinstring);
	}
	else if (!searchstring[0]) {
		message("No search string");
		return;
	}
	buf->cursorhorz = horztohorz(buf->cursorhorz);
	buf->lineoffset = horztooffset(buf->cursorhorz) + 1;
	if (!search(buf->linenum, buf->lineoffset - 1, searchstring)) {
		message("Found");
		doframe();
		display(FALSE);
		linenumchgflg = dsplineinfoflg;
	}
	else message("Not found");
}

static void cmdtranslate()
{
	UINT startlinenum, startoffset, globalflg, markedflg, fromsize, tosize;
	INT rightanchorflg, leftanchorflg, trancount;
	UINT n;
	CHAR insertflg, tranmsg[32], worktranslate[128];
	static CHAR translatefrom[128], translateto[128];

	strcpy(keyinstring, translatefrom);
	if (keyin("Translate from: ", KEYIN_TABSTRING) < 1) return;
	strcpy(translatefrom, keyinstring);
	strcpy(keyinstring, translateto);
	if (keyin("Translate to: ", KEYIN_TABSTRING) == -1) return;
	strcpy(translateto, keyinstring);
	//n = 0;
	fromsize = 0;
	for (n = 0;;) {
		if (translatefrom[n] == forcechar) n++;
		if (translatefrom[n]) {
			n++;
			fromsize++;
		}
		else break;
	}
	rightanchorflg = leftanchorflg = FALSE;
	if (translatefrom[fromsize - 1] == rightanchorchar) {
		fromsize--;
		rightanchorflg = TRUE;
	}
	if (translatefrom[0] == leftanchorchar) {
		fromsize--;
		leftanchorflg = TRUE;
	}
	tosize = (UINT)strlen(translateto);
	insertflg = FALSE;
	for (n = 0; translateto[n]; n++) {
		if (translateto[n] == matchinsertchar) {
			if (!fromsize) {
				message("Invalid use of insert matched string");
				return;
			}
			tosize += (fromsize - 1);
			insertflg = TRUE;
		}
	}
	trancount = 0;
	startlinenum = buf->linenum;
	buf->cursorhorz = horztohorz(buf->cursorhorz);
	startoffset = horztooffset(buf->cursorhorz);
	buf->lineoffset = startoffset + 1;
	globalflg = FALSE;
	markedflg = (markstatus == 2);
	//p = (*buf->curline)->data;
	//n = (*buf->curline)->size - startoffset;
	while (TRUE) {
		UCHAR *p;
		if (search(startlinenum, startoffset, translatefrom)) break;
		if (!globalflg) {
			doframe();
			display(FALSE);
tranask:       cursorflg = TRUE;
			if (markedflg) keyin("local(L), global(G), marked(M), final(F), skip(space bar), or quit(X)", KEYIN_KEYSTROKE);
			else keyin("local(L), global(G), final(F), skip(space bar), or quit(X)", KEYIN_KEYSTROKE);
			vidput(cursoroff);
			cursorflg = FALSE;
			keyinvalue = toupper(keyinvalue);
			if (keyinvalue == 'X') break;
			if (markedflg && keyinvalue == 'M') {
				globalflg = TRUE;
				gotoline(buf, buf->beginmark);
				buf->cursorhorz = 0;
				buf->lineoffset = 0;
				startlinenum = buf->endmark;
				startoffset = (*buf->curline)->size;
				continue;
			}
			markedflg = FALSE;
			if (keyinvalue == ' ') {
				if (startlinenum == buf->linenum && startoffset == buf->lineoffset) break;
				buf->lineoffset++;
				continue;
			}
			if (keyinvalue == 'G') globalflg = TRUE;
			else if (keyinvalue != 'L' && keyinvalue != 'F') goto tranask;
		}
		if (startlinenum == buf->linenum && buf->lineoffset < startoffset) startoffset += tosize - fromsize;
		p = (*buf->curline)->data + buf->lineoffset;
		if (insertflg) memcpy(worktranslate, p, fromsize);
		n = (*buf->curline)->size - buf->lineoffset - fromsize;
		if (fromsize > tosize) {
			memmove(p + tosize, p + fromsize, n);
			chglinesize(buf->curline, (*buf->curline)->size - fromsize + tosize);
			p = (*buf->curline)->data + buf->lineoffset;  /* mem could move so reset p */
		}
		else if (fromsize < tosize) {
			if (chglinesize(buf->curline, (*buf->curline)->size - fromsize + tosize)) return;
			p = (*buf->curline)->data + buf->lineoffset;  /* mem could move so reset p */
			memmove(p + tosize, p + fromsize, n);
		}
		if (insertflg) {
			for (n = 0; translateto[n]; n++) {
				if (translateto[n] == matchinsertchar) {
					memcpy(p, worktranslate, fromsize);
					p += fromsize;
				}
				else *p++ = translateto[n];
			}
		}
		else memcpy(p, translateto, tosize);
		if (!globalflg) displayline();
		trancount++;
		if (keyinvalue == 'F') break;
		if (!fromsize && leftanchorflg && startlinenum == buf->linenum) break;
		if (startlinenum == buf->linenum && startoffset == buf->lineoffset) break;
		if (rightanchorflg) buf->lineoffset = 1000;
		else buf->lineoffset += tosize;
	}
	if (globalflg) {
		doframe();
		display(TRUE);
	}
	mscitoa(trancount, tranmsg);
	if (trancount == 1) strcat(tranmsg, " translation");
	else strcat(tranmsg, " translations");
	message(tranmsg);
	if (trancount) buf->changeflg = TRUE;
	linenumchgflg = dsplineinfoflg;
}

/* search for string */
/* search starts at buf->curline, buf->lineoffset + 1 */
/* if successful return 0 and buf->curline, buf->lineoffset, buf->cursorhorz */
/* else return non-zero */
static INT search(UINT endlinenum, UINT endoffset, CHAR *searchstring)
{
	CHAR *p, leftanchorflg, rightanchorflg, matchanyflg, search[128];
	INT flag1;  /* two uses: first record and back to first record */
	INT m, n, size, searchlen;
	UINT linenum;
	LINEHANDLE line;

	linenum = buf->linenum;
	if (endlinenum > linenum || (endlinenum == linenum && endoffset >= buf->lineoffset)) flag1 = TRUE;
	else flag1 = FALSE;
	if (!(n = (INT)strlen(searchstring))) return(-1);
	leftanchorflg = rightanchorflg = matchanyflg = FALSE;
	p = searchstring;
	if (searchstring[0] == leftanchorchar) {
		leftanchorflg = TRUE;
		n--;
		p++;
	}
	for (searchlen = 0; n--; p++) {
		if (*p == forcechar) {
			if (!n--) return(-2);
			search[searchlen++] = *++p;
		}
		else {
			if (!n && *p == rightanchorchar) {
				rightanchorflg = TRUE;
				break;
			}
			if (*p == matchanychar) matchanyflg = TRUE;
			search[searchlen++] = *p;
		}
	}
	if (!searchcaseflg) for (m = 0; m < searchlen; m++) search[m] = toupper(search[m]);
	if (leftanchorflg || rightanchorflg) {
		line = buf->curline;
		if (leftanchorflg || buf->lineoffset > 512) flag1 = TRUE;
		else flag1 = FALSE;
		do {
			if (flag1) {
				line = (*line)->next;
				if (line != NULL) linenum++;
				else {
					line = buf->firstline;
					linenum = 1;
				}
				if (linenum == endlinenum && !leftanchorflg) break;
			}
			else flag1 = TRUE;
			size = (*line)->size;
			if (rightanchorflg) n = searchlen;
			else n = size;
			if (!searchlen) {
				if ((leftanchorflg && !rightanchorflg) ||
					(!leftanchorflg && rightanchorflg) || !size) goto found1;
				if (linenum == endlinenum) break;
				continue;
			}
			if (n < searchlen || (n > searchlen && leftanchorflg && rightanchorflg)) continue;
			m = size - n;
			p = (CHAR *)(*line)->data + m;
			if (!matchanyflg) {
				if (searchcaseflg) {
					if (!memcmp(p, search, searchlen)) goto found1;
					continue;
				}
#if OS_WIN32
#pragma warning(suppress:4996)
				if (!_memicmp(p, search, searchlen)) goto found1;
#else
				while (m < searchlen && toupper(*p) == search[m]) p++, m++;
#endif
			}
			else if (searchcaseflg) {
				while (m < searchlen && (search[m] == matchanychar || *p == search[m])) p++, m++;
			}
			else while (m < searchlen && (search[m] == matchanychar || (CHAR) toupper(*p) == search[m])) p++, m++;
			if (m == searchlen) goto found1;
		} while (linenum != endlinenum || rightanchorflg);
		return(1);
	}
	line = buf->curline;
	p = (CHAR *)(*line)->data + buf->lineoffset;
	size = (*line)->size;
	n = size - buf->lineoffset;
	while (TRUE) {
		if (!matchanyflg) {
			if (searchcaseflg) {
				while (n >= searchlen) {
					if (*p == search[0] && !memcmp(p, search, searchlen)) goto found;
					p++;
					n--;
				}
			}
			else {
				while (n >= searchlen) {
#if OS_WIN32
#pragma warning(suppress:4996)
					if (!_memicmp(p, search, searchlen)) goto found;
#else
					if (toupper(*p) == search[0]) {
						for (m = 1; m < searchlen && toupper(*(p + m)) == search[m]; m++);
						if (m == searchlen) goto found;
					}
#endif
					p++;
					n--;
				}
			}
		}
		else if (searchcaseflg) {
			while (n >= searchlen) {
				for (m = 0; m < searchlen && (matchanychar == search[m] || *(p + m) == search[m]); m++);
				if (m == searchlen) goto found;
				p++;
				n--;
			}
		}
		else {
			while (n >= searchlen) {
				for (m = 0; m < searchlen && (*(p + m) == matchanychar || (CHAR) toupper(*(p + m)) == search[m]); m++);
				if (m == searchlen) goto found;
				p++;
				n--;
			}
		}
		if (!flag1 && linenum == buf->linecount) {
			line = buf->firstline;
			linenum = 1;
			flag1 = TRUE;
		}
		else {
			line = (*line)->next;
			linenum++;
		}
		if (flag1 && linenum >= endlinenum) {
			if (linenum > endlinenum) return(1);
			n = endoffset + searchlen;
			if (n > (INT) (*line)->size) n = size = (*line)->size;
			else size = n;
		}
		else n = size = (*line)->size;
		p = (CHAR *)(*line)->data;
	}
found:
	if (flag1 && linenum == endlinenum && ((UINT) size - n) > endoffset) return(1);
found1:
	buf->curline = line;
	buf->linenum = linenum;
	buf->lineoffset = size - n;
	buf->cursorhorz = offsettohorz(buf->lineoffset);
	return(0);
}

static void cmdread(CHAR *filename)
{
	INT firstflg, truncateflg, roflag, n, filenum;
	LINEHANDLE line, nextline;
	CHAR errmsg[100];

	if (buf->changeflg) {
		keyin("OK to lose changes? ", KEYIN_YESNO);
		if (keyinstring[0] != 'Y') return;
	}
	if (filename == NULL) {
		strcpy(keyinstring, readfilename);
		if (keyin("File to edit: ", KEYIN_STRING) < 1) return;
		miofixname(keyinstring, dottext, FIXNAME_EXT_ADD);
		strcpy(buf->filename, keyinstring);
	}
	else {
		miofixname(filename, dottext, FIXNAME_EXT_ADD);
		strcpy(buf->filename, filename);
	}
	strcpy(readfilename, buf->filename);
	clearbuffer();
	roflag = FALSE;
	filenum = rioopen(buf->filename, RIO_M_SHR | RIO_P_SRC | RIO_T_ANY, 6, MAXRECSIZE);
	if (filenum == ERR_NOACC) {
		filenum = rioopen(buf->filename, RIO_M_SRO | RIO_P_SRC | RIO_T_ANY, 6, MAXRECSIZE);
		if (filenum >= 1) {
			message("Opening read-only file");
			roflag = TRUE;
		}
	}
	if (filenum < 1) {
		if (filenum != ERR_FNOTF) {
			strcpy(errmsg, "Error attempting to open file - ");
			strcat(errmsg, fioerrstr(filenum));
			message(errmsg);
			buf->filename[0] = 0;
		}
		else if (filename == NULL) message("File not found - new file assumed");
		buf->readonlyflag = FALSE;
		return;
	}
	strcpy(buf->filename, fioname(filenum));
	firstflg = TRUE;
	truncateflg = FALSE;
	line = buf->curline;
	buf->linenum = 1;
	buf->readonlyflag = roflag;
	buf->memcapflg = FALSE;
	while (TRUE) {
		n = rioget(filenum, record, sizeof(record) - 9 /*503*/);
		if (n == -2) continue;
		if (n == -1) break;
		if (n == ERR_NOEOR) {
			truncateflg = TRUE;
			message("Error reading file - line too long");
			buf->filename[0] = 0;
			break;
		}
		else if (n < 0) {
			message("Error while reading file");
			buf->filename[0] = 0;
			break;
		}
		if (firstflg) {
			if (n && chglinesize(line, n)) return;
			firstflg = FALSE;
		}
		else {
			if ((nextline = linealloc(n)) == NULL) {
				buf->filename[0] = 0;
				buf->memcapflg = TRUE;
				break;
			}
			(*line)->next = nextline;
			(*nextline)->prev = line;
			line = nextline;
			buf->linecount++;
		}
		memcpy((*line)->data, record, n);
	}
	if (buf->linecount > 1) {
		(*line)->next = NULL;
		buf->lastline = line;
	}
//	if (truncateflg) message("One or more long records ( > 503 ) were truncated");
	if (truncateflg) {
		sprintf(errmsg, "One or more long records ( > %d ) were truncated", (int)(sizeof(record) - 9));
		message(errmsg);
	}
	buf->filetype = riotype(filenum);
	buf->linenum = buf->windowtopline = 1;
	buf->windowleftoffset = buf->cursorhorz = 0;
	setcursorhorz(0);
	setcursorvert(0);
	display(TRUE);
	rioclose(filenum);
	buf->changeflg = FALSE;
	linenumchgflg = TRUE;
}

static void cmdwrite()
{
	INT n;
	CHAR workname[128], fullext[5];

#if OS_UNIX
	INT handle;
	CHAR filename[128];
	struct stat statbuf;
#endif

	if (buf->readonlyflag) {
		message("Unable to write read-only file");
		return;
	}
	if (buf->memcapflg) {
		keyin("File may be incomplete - OK to write? ", KEYIN_YESNO);
		if (keyinstring[0] != 'Y') return;
	}
	fullext[0] = '.';
	strcpy(&fullext[1], backext);
	n = RIO_M_PRP | RIO_P_WRK | RIO_NOX;
	if (!buf->filename[0]) {
		keyinstring[0] = 0;
		if (keyin("File name to write: ", KEYIN_STRING) < 1) return;
		miofixname(keyinstring, dottext, FIXNAME_EXT_ADD);
		strcpy(buf->filename, keyinstring);
		buf->filetype = createtype;
		n = RIO_M_PRP | RIO_P_SRC | RIO_NOX;
		strcpy(workname, buf->filename);
		miofixname(workname, fullext, FIXNAME_EXT_ADD | FIXNAME_EXT_REPLACE);
		fiokill(fioopen(workname, FIO_M_EXC | FIO_P_SRC));
	}
	else {
		if (!filecompflg) n |= RIO_UNC;
		strcpy(workname, buf->filename);
		miofixname(workname, fullext, FIXNAME_EXT_ADD | FIXNAME_EXT_REPLACE);
		fiokill(fioopen(workname, FIO_M_EXC | FIO_P_WRK));
	}
#if OS_UNIX
	handle = fioopen(buf->filename, FIO_M_EXC | FIO_P_SRC);
	if (handle > 0) {
		strcpy(filename, fioname(handle));
		fioclose(handle);
		if (stat(filename, &statbuf) == -1) handle = -1;
	}
#endif
	fioarename(buf->filename, workname);
	if (!writefile(buf->filename, n | buf->filetype, 1, buf->linecount)) buf->changeflg = FALSE;
#if OS_UNIX
	if (handle > 0) {
		chmod(filename, statbuf.st_mode);
		chown(filename, statbuf.st_uid, statbuf.st_gid);
	}
#endif
}

static void cmdfilename()
{
	INT filenum;

	strcpy(keyinstring, buf->filename);
	if (keyin("New filename: ", KEYIN_STRING) < 1) return;
	miofixname(keyinstring, dottext, FIXNAME_EXT_ADD);
	strcpy(buf->filename, keyinstring);
	buf->readonlyflag = FALSE;
	filenum = rioopen(buf->filename, RIO_M_SHR | RIO_P_SRC | RIO_T_ANY, 6, MAXRECSIZE);
	if (filenum == ERR_NOACC) {
		filenum = rioopen(buf->filename, RIO_M_SRO | RIO_P_SRC | RIO_T_ANY, 6, MAXRECSIZE);
		if (filenum >= 1) buf->readonlyflag = TRUE;
	}
	if (filenum >= 1) {
		strcpy(buf->filename, fioname(filenum));
		rioclose(filenum);
	}
}

static void cmdwritemarked()
{
	INT filenum;
	CHAR markfile[128];

	if (markstatus != 2) {
		message("Nothing marked");
		return;
	}
	keyinstring[0] = 0;
	if (keyin("File to write: ", KEYIN_STRING) < 1) return;
	strcpy(markfile, keyinstring);
	miofixname(markfile, dottext, FIXNAME_EXT_ADD);
	filenum = fioopen(markfile, FIO_M_EXC | FIO_P_SRC);
	if (filenum > 0) {
		strcpy(markfile, fioname(filenum));
		fioclose(filenum);
		keyin("File already exists - OK to overwrite? ", KEYIN_YESNO);
		if (keyinstring[0] != 'Y') return;
	}
	buf = &buffers[markbufindex];
	writefile(markfile, RIO_M_PRP | RIO_P_SRC | RIO_NOX | createtype, buf->beginmark, buf->endmark);
	buf = &buffers[currentbufindex];
}

static void cmdmerge()
{
	INT k, n, filenum, truncateflg;
	LINEHANDLE line;
	CHAR errmsg[100];
	UCHAR record[512];

	keyinstring[0] = 0;
	if (keyin("File to merge: ", KEYIN_STRING) < 1) return;
	miofixname(keyinstring, dottext, FIXNAME_EXT_ADD);
	filenum = rioopen(keyinstring, RIO_M_SHR | RIO_P_SRC | RIO_T_ANY, 6, MAXRECSIZE);
	if (filenum < 1) {
		if (filenum == ERR_FNOTF) message("File not found");
		else {
			strcpy(errmsg, "Error attempting to open file - ");
			strcat(errmsg, fioerrstr(filenum));
			message(errmsg);
		}
		return;
	}
	k = buf->linenum + 1;
	truncateflg = FALSE;
	while (TRUE) {
		n = rioget(filenum, record, 503);
		if (n == -2) continue;
		if (n == -1) break;
		if (n == ERR_SHORT) truncateflg = TRUE;
		if (n < 0) {
			message("Error while reading file");
			break;
		}
		if ((line = linealloc(n)) == NULL) return;
		memcpy((*line)->data, record, n);
		insertline(buf, line);
	}
	if (truncateflg) message("One or more long records ( > 512 ) were truncated");
	gotoline(buf, k);
	doframe();
	display(TRUE);
	rioclose(filenum);
	buf->changeflg = TRUE;
	linenumchgflg = dsplineinfoflg;
}

/* c is character to display, offset is 0 or 1 */
static void displayspecial(INT c, INT offset)  /* second line special display */
{
	INT n;
	INT32 vidcmds[10];

	vidcmds[0] = VID_WIN_TOP + 1;  /* second line */
	vidcmds[1] = VID_EU;  /* cursor to upper right corner */
	n = 2;
	if (offset) vidcmds[n++] = VID_HORZ_ADJ + 0xFFFF;
	if (c) {
		if (brightflg) vidcmds[n++] = VID_BOLD_OFF;
		vidcmds[n++] = VID_REV_ON;
		vidcmds[n++] = VID_DISP_CHAR + c;
		vidcmds[n++] = VID_REV_OFF;
		if (brightflg) vidcmds[n++] = VID_BOLD_ON;
	}
	else vidcmds[n++] = VID_DISP_SYM + VID_SYM_HLN;
	vidcmds[n++] = VID_FINISH;
	vidput(vidcmds);
	vidput(setwindow);
}

/* return 0 if successfully written */
static INT writefile(CHAR* filename, INT openmode, INT first, INT last)  /* write records to a file */
{
	INT filenum, i, j;
	LINEHANDLE line;
	UCHAR record[516];

	filenum = rioopen(filename, openmode, 6, MAXRECSIZE);
	if (filenum < 1) {
		message("Unable to create file");
		return(-1);
	}
	message("Writing");
	for (line = buf->firstline, i = 1; i != first; line = (*line)->next, i++);
	for ( ; line != NULL && i <= last; line = (*line)->next, i++) {
		j = (*line)->size;
		memcpy(record, (*line)->data, j);
		if (rioput(filenum, record, j)) {
			rioclose(filenum);
			message("Error while writing file");
			return(-2);
		}
	}
	if (rioclose(filenum)) {
		message("Error while writing file");
		return(-2);
	}
	message("Written successfully");
	return(0);
}

/* return 0 if buffer is in a window, else return 1 */
static INT setcurrentbufwin(INT n)  /* set the current buffer */
{
	INT top, bottom;

	currentbufindex = n;
	buf = &buffers[n];
	if (window1bufindex == n) {
		top = 2;
		if (!windowstatus) bottom = slines - 1;
		else bottom = windowsplitline - 2;
	}
	else if (windowstatus && window2bufindex == n) {
		top = windowsplitline;
		bottom = slines - 1;
	}
	else return(1);
	wlines = bottom - top + 1;
	setwindow[0] = VID_WIN_TOP + top;
	setwindow[1] = VID_WIN_BOTTOM + bottom;
	vidput(setwindow);
	return(0);
}

static void clearbuffer()  /* clear the current buffer */
{
	LINEHANDLE line, nextline, newline;

	if ((newline = linealloc(0)) == NULL) return;
	line = buf->firstline;
	while (TRUE) {
		nextline = (*line)->next;
		memfree((UCHAR **) line);  /* free the line */
		if (nextline == NULL) break;
		line = nextline;
	}
	buf->curline = newline;
	buf->firstline = buf->lastline = buf->curline;
	(*buf->curline)->next = (*buf->curline)->prev = NULL;
	buf->linenum = buf->linecount = buf->windowtopline = 1;
	buf->windowleftoffset = buf->cursorhorz = 0;
	buf->changeflg = FALSE;
	buf->filetype = createtype;
	vidput(erasewindow);
	if (markstatus && markbufindex == currentbufindex) markstatus = 0;
	setcursorhorz(0);
	setcursorvert(0);
}

static void gotoline(BUFFERPTR b, UINT n)  /* goto line n - set buf->linenum and buf->curline */
{
	INT i, j;
	LINEHANDLE line;

	if (n < b->linenum) {
		if (n < b->linenum / 2) {
			i = j = 1;
			line = b->firstline;
		}
		else {
			i = b->linenum;
			j = -1;
			line = b->curline;
		}
	}
	else if (n < b->linenum + b->linenum / 2) {
		i = b->linenum;
		j = 1;
		line = b->curline;
	}
	else {
		i = b->linecount;
		j = -1;
		line = b->lastline;
	}
	while ((UINT)i != n) {
		i += j;
		if (j > 0) line = (*line)->next;
		else line = (*line)->prev;
	}
	b->linenum = n;
	b->curline = line;
}

/*** MAY MOVE MOVEABLE MEMORY ***/
static LINEHANDLE linealloc(INT size)  /* allocate a line */
{
	INT allocsiz;
	LINEHANDLE line;

	allocsiz = size + sizeof(struct linestruct);
	if (allocsiz & 0x1f) allocsiz = (allocsiz & ~0x1f) + 0x20;
	line = (LINEHANDLE) memalloc(allocsiz, 0);
	if (line == NULL) message("Memory capacity exceeded");
	else {
		(*line)->next = (*line)->prev = NULL;
		(*line)->size = size;
	}
	return(line);
}

/***
 *** MAY MOVE MOVEABLE MEMORY
 ***
 * change line allocation, return 0 if OK
 */
static INT chglinesize(LINEHANDLE line, INT size)
{
	INT oldalloc, newalloc;

	oldalloc = (*line)->size + sizeof(struct linestruct);
	if (oldalloc & 0x1f) oldalloc = (oldalloc & ~0x1f) + 0x20;
	newalloc = size + sizeof(struct linestruct);
	if (newalloc & 0x1f) newalloc = (newalloc & ~0x1f) + 0x20;
	if (oldalloc != newalloc && memchange((UCHAR **) line, newalloc, 0)) {
		message("Memory capacity exceeded");
		return(-1);
	}
	(*line)->size = size;
	return(0);
}

/* push a line onto the undo stack */
static void pushundostack(LINEHANDLE line)
{
	if (++undostacktop == MAXUNDO) undostacktop = 0;
	if (undostacktop == undostacktail && ++undostacktail == MAXUNDO) undostacktail = 0;
	undostack[undostacktop] = line;
}

/* keyin on top line of screen */
/* return number of characters entered, or -1 if abort key is pressed */
/* value keyed in is in keyinvalue and keyinstring */
static INT keyin(CHAR *message, INT keyintype) // @suppress("No return")
{
	static INT32 msgcmds[10] = { VID_CURSOR_OFF, VID_WIN_RESET, VID_HU, VID_EL, VID_DISPLAY };
	static INT32 backspace[] = { VID_HORZ_ADJ + 0xFFFF, VID_DISP_CHAR + ' ', VID_HORZ_ADJ + 0xFFFF, VID_FINISH };
	static INT32 displaychar[] = { VID_DISP_CHAR, VID_FINISH };
	INT i, m, n, dspmax;
	INT32 vidcmds[10];
	CHAR savestring[128], *ptr;

	keyflg = TRUE;
	n = (INT)strlen(message);
	dspmax = scolumns - n - 1;
	msgcmds[4] = VID_DISPLAY + n;
	i = 5;
	if (sizeof(void *) > sizeof(INT32)) {
		memcpy((void *) &msgcmds[i], (void *) &message, sizeof(void *));
		i += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
	}
	else *(UCHAR **)(&msgcmds[i++]) = (UCHAR *) message;
	msgcmds[i] = VID_FINISH;
	vidput(msgcmds);
	if (keyintype == KEYIN_KEYSTROKE) {
		vidput(setwindow);
		if (cursorflg) {
			vidput(cursorpos);
			vidput(cursoron);
		}
		keyinvalue = charget();
		return(1);
	}
	vidput(cursoron);
	if (keyintype == KEYIN_YESNO) m = 1;
	else if (keyintype == KEYIN_NUMBER) m = 5;
	else if (keyintype == KEYIN_STRING3) m = 3;
	else m = 100;
	n = 0;
	strcpy(savestring, keyinstring);
	while (TRUE) {
		if (fileplaybackflg) {  /* keystrokes/commands come from file playback array */
			if (fileplaybackindex == fileplaybackcount) {
				fileplaybackflg = FALSE;
				continue;
			}
			keyinvalue = fileplaybackbuffer[fileplaybackindex++];
		}
		else if (playbackflg) {  /* keystrokes/commands come from playback array */
			if (playbackindex == playbackcount) {
				playbackflg = FALSE;
				continue;
			}
			keyinvalue = playbackbuffer[playbackindex++];
		}
		else keyinvalue = charget();
		if (recmodeflg && playbackcount < MAXPLAYBACK) playbackbuffer[playbackcount++] = keyinvalue;
		if ((USHORT) keyinvalue == keyabort) {
			keyinvalue = 0;
			keyinstring[0] = 0;
			return(-1);
		}
		if ((USHORT) keyinvalue == keyend) {
			keyinstring[n] = 0;
			if (keyintype == KEYIN_NUMBER) keyinvalue = atoi(keyinstring);
			vidput(cursoroff);
			vidput(setwindow);
			return(n);
		}
		if ((USHORT) keyinvalue == keybkspc) {
			if (!n) goto putbeep;
			if (n-- > dspmax) {
				vidcmds[0] = VID_CURSOR_OFF;
				vidcmds[1] = VID_HORZ + (INT32)strlen(message);
				vidcmds[2] = VID_DISPLAY + dspmax;
				i = 3;
				if (sizeof(void *) > sizeof(INT32)) {
					ptr = keyinstring + n - dspmax;
					memcpy((void *) &vidcmds[i], (void *) &ptr, sizeof(void *));
					i += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
				}
				else *(UCHAR **)(&vidcmds[i++]) = (UCHAR *) keyinstring + n - dspmax;
				vidcmds[i++] = VID_CURSOR_ON;
				vidcmds[i] = VID_FINISH;
				vidput(vidcmds);
			}
			else vidput(backspace);
			continue;
		}
		if ((USHORT) keyinvalue == keyrecall) { /* up */
			strcpy(keyinstring, savestring);
			vidcmds[0] = VID_CURSOR_OFF;
			vidcmds[1] = VID_HORZ + (INT32)strlen(message);
			n = (INT)strlen(keyinstring);
			if (n > dspmax) {
				vidcmds[2] = VID_DISPLAY + dspmax;
				ptr = keyinstring + n - dspmax;
			}
			else {
				vidcmds[2] = VID_DISPLAY + n;
				ptr = keyinstring;
			}
			i = 3;
			if (sizeof(void *) > sizeof(INT32)) {
				memcpy((void *) &vidcmds[i], (void *) &ptr, sizeof(void *));
				i += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
			}
			else *(UCHAR **)(&vidcmds[i++]) = (UCHAR *) ptr;
			vidcmds[i++] = VID_EL;
			vidcmds[i++] = VID_CURSOR_ON;
			vidcmds[i] = VID_FINISH;
			vidput(vidcmds);
			continue;
		}
		if (keyinvalue < 32 || keyinvalue > 255) {
			if (keyintype == KEYIN_TABSTRING && keyinvalue == 258) keyinvalue = 9;
			else continue;
		}
		if (m == n) goto putbeep;
		if (keyintype == KEYIN_NUMBER && !isdigit(keyinvalue)) goto putbeep;
		if (keyintype == KEYIN_YESNO) {
			keyinstring[n] = (UCHAR) toupper(keyinvalue);
			if (keyinstring[n] == 'Y' || keyinstring[n] == 'N') n++;
			else goto putbeep;
		}
		else keyinstring[n++] = (UCHAR) keyinvalue;
		if (n > dspmax) {
			vidcmds[0] = VID_CURSOR_OFF;
			vidcmds[1] = VID_HORZ + (INT32)strlen(message);
			vidcmds[2] = VID_DISPLAY + dspmax;
			i = 3;
			if (sizeof(void *) > sizeof(INT32)) {
				ptr = keyinstring + n - dspmax;
				memcpy((void *) &vidcmds[i], (void *) &ptr, sizeof(void *));
				i += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
			}
			else *(UCHAR **)(&vidcmds[i++]) = (UCHAR *) keyinstring + n - dspmax;
			vidcmds[i++] = VID_CURSOR_ON;
			vidcmds[i] = VID_FINISH;
			vidput(vidcmds);
		}
		else {
			if (keyinvalue == 9) displaychar[0] = VID_DISP_CHAR + tabdisplay;
			else displaychar[0] = VID_DISP_CHAR + keyinvalue;
			vidput(displaychar);
		}
		continue;
putbeep:
		vidput(beep);
	}
}

/* display the current window */
/* display is window defined by buf->windowtopline and buf->windowleftoffset */
/* if TRUE, then unconditionally display entire window */
static void display(INT changeflg)
{

	static INT32 dsplinecmds[8] = { VID_HORZ, VID_VERT, VID_DISPLAY };
	static INT32 dspellinecmds[9] = { VID_HORZ, VID_VERT, VID_DISPLAY };
	static INT32 dsperasefrom[] = { VID_HORZ, VID_VERT,
		VID_EF, VID_END_NOFLUSH };
	static INT32 dsperaseline[] = { VID_HORZ, VID_VERT,
		VID_EL, VID_END_NOFLUSH };
	INT i, n, v, vstart, vend, srclen;
	UINT dsplen, dspmax;
	UINT savelinenum;
	UCHAR markflg, checkmarkflg, *ptr, *srcline;
	LINEHANDLE line, saveline;

	if (!changeflg && lastwindowleftoffset == buf->windowleftoffset) {
		n = buf->windowtopline - lastwindowtopline;
		if (!n) return;
		vidput(cursoroff);
#if !OS_WIN32
/* this code is useful for minimizing character traffic on dumb tubes */
		if (n > 0) {
			if ((UINT) n < (wlines / 2)) {
				vstart = wlines - n;
				vend = wlines - 1;
				while (n--) vidput(scrollup);
				goto display1;
			}
		}
		else {
			n = 0 - n;
			if ((UINT) n < (wlines / 2)) {
				vstart = 0;
				vend = n - 1;
				while (n--) vidput(scrolldown);
				goto display1;
			}
		}
#endif
	}
	else vidput(cursoroff);
	vstart = 0;
	if (buf->windowtopline + wlines - 1 > buf->linecount) {
		vend = buf->linecount - buf->windowtopline;
		dsperasefrom[1] = VID_VERT + vend;
		vidput(dsperasefrom);
	}
	else vend = wlines - 1;
#if !OS_WIN32
display1:
#endif
	if (buf->windowtopline + vstart == buf->linenum) line = buf->curline;
	else {
		savelinenum = buf->linenum;
		saveline = buf->curline;
		gotoline(buf, buf->windowtopline + vstart);
		line = buf->curline;
		buf->curline = saveline;
		buf->linenum = savelinenum;
	}

	i = 3;
	if (sizeof(UCHAR *) > sizeof(INT32)) {
		ptr = (UCHAR *)(dspline + buf->windowleftoffset);
		memcpy((UCHAR *) &dsplinecmds[i], (UCHAR *) &ptr, sizeof(UCHAR *));
		i += (sizeof(UCHAR *) + sizeof(INT32) - 1) / sizeof(INT32);
	}
	else *(UCHAR **)(&dsplinecmds[i++]) = (UCHAR *)(dspline + buf->windowleftoffset);
	dsplinecmds[i] = VID_END_NOFLUSH;
	i = 3;
	if (sizeof(UCHAR *) > sizeof(INT32)) {
		ptr = (UCHAR *)(dspline + buf->windowleftoffset);
		memcpy((UCHAR *) &dspellinecmds[i], (UCHAR *) &ptr, sizeof(UCHAR *));
		i += (sizeof(UCHAR *) + sizeof(INT32) - 1) / sizeof(INT32);
	}
	else *(UCHAR **)(&dspellinecmds[i++]) = (UCHAR *)(dspline + buf->windowleftoffset);
	dspellinecmds[i++] = VID_EL;
	dspellinecmds[i] = VID_END_NOFLUSH;

	checkmarkflg = markflg = FALSE;
	if (markstatus == 2 && currentbufindex == markbufindex) checkmarkflg = TRUE;
	for (v = vstart; v <= vend; v++) {
		if (checkmarkflg) {
			if (!markflg) {
				if (buf->windowtopline + v >= buf->beginmark && buf->windowtopline + v <= buf->endmark) {
					vidput(&revon[!brightflg]);  /* tricky but it works */
					markflg = TRUE;
				}
			}
			else if (buf->windowtopline + v > buf->endmark) {
				if (brightflg) vidput(revoffboldon);
				else vidput(revoff);
				markflg = checkmarkflg = FALSE;
			}
		}
		srcline = (*line)->data;
		srclen = (*line)->size;
		dsplen = 0;
		dspmax = buf->windowleftoffset + scolumns;
		while (srclen-- && dsplen < dspmax) {
			if (*srcline != 9) dspline[dsplen++] = *srcline++;
			else {
				n = tablookup[dsplen];
				if (tabdisplayflg) {
					dspline[dsplen++] = tabdisplay;
					n--;
				}
				while (n--) dspline[dsplen++] = ' ';
				srcline++;
			}
		}
		if (dsplen >= dspmax) {
			dsplinecmds[1] = VID_VERT + v;
			dsplinecmds[2] = VID_DISPLAY + dsplen - buf->windowleftoffset;
			vidput(dsplinecmds);
		}
		else if (dsplen < buf->windowleftoffset) {
			dsperaseline[1] = VID_VERT + v;
			vidput(dsperaseline);
		}
		else {
			dspellinecmds[1] = VID_VERT + v;
			dspellinecmds[2] = VID_DISPLAY + dsplen - buf->windowleftoffset;
			vidput(dspellinecmds);
		}
		line = (*line)->next;
	}
	if (markflg) {
		if (brightflg) vidput(revoffboldon);
		else vidput(revoff);
	}
	lastwindowtopline = buf->windowtopline;
	lastwindowleftoffset = buf->windowleftoffset;
	vidput(cursoron);
}

static void displayline()  /* display the current line */
{
	static INT32 dsplinecmds[8] = { VID_HORZ, VID_VERT, VID_DISPLAY };
	static INT32 dspellinecmds[9] = { VID_HORZ, VID_VERT, VID_DISPLAY };
	INT i, n, srclen;
	UINT dsplen, dspmax;
	UCHAR markflg, *ptr, *srcline;

	srcline = (*buf->curline)->data;
	srclen = (*buf->curline)->size;
	dsplen = 0;
	dspmax = buf->windowleftoffset + scolumns;
	while (srclen-- && dsplen < dspmax) {
		if (*srcline != 9) dspline[dsplen++] = *srcline++;
		else {
			n = tablookup[dsplen];
			if (tabdisplayflg) {
				dspline[dsplen++] = tabdisplay;
				n--;
			}
			while (n--) dspline[dsplen++] = ' ';
			srcline++;
		}
	}
	if (markstatus == 2 && currentbufindex == markbufindex &&
		buf->linenum >= buf->beginmark && buf->linenum <= buf->endmark) {
		vidput(&revon[!brightflg]);  /* tricky but it works */
		markflg = TRUE;
	}
	else markflg = FALSE;
	if (dsplen >= dspmax) {
		dsplinecmds[1] = VID_VERT + buf->linenum - buf->windowtopline;
		dsplinecmds[2] = VID_DISPLAY + dsplen - buf->windowleftoffset;
		i = 3;
		if (sizeof(UCHAR *) > sizeof(INT32)) {
			ptr = (UCHAR *)(dspline + buf->windowleftoffset);
			memcpy((UCHAR *) &dsplinecmds[i], (UCHAR *) &ptr, sizeof(UCHAR *));
			i += (sizeof(UCHAR *) + sizeof(INT32) - 1) / sizeof(INT32);
		}
		else *(UCHAR **)(&dsplinecmds[i++]) = (UCHAR *)(dspline + buf->windowleftoffset);
		dsplinecmds[i] = VID_FINISH;
		vidput(dsplinecmds);
	}
	else {
		dspellinecmds[1] = VID_VERT + buf->linenum - buf->windowtopline;
		dspellinecmds[2] = VID_DISPLAY + dsplen - buf->windowleftoffset;
		i = 3;
		if (sizeof(UCHAR *) > sizeof(INT32)) {
			ptr = (UCHAR *)(dspline + buf->windowleftoffset);
			memcpy((UCHAR *) &dspellinecmds[i], (UCHAR *) &ptr, sizeof(UCHAR *));
			i += (sizeof(UCHAR *) + sizeof(INT32) - 1) / sizeof(INT32);
		}
		else *(UCHAR **)(&dspellinecmds[i++]) = (UCHAR *)(dspline + buf->windowleftoffset);
		dspellinecmds[i++] = VID_EL;
		dspellinecmds[i] = VID_FINISH;
		vidput(dspellinecmds);
	}
	if (markflg) {
		if (brightflg) vidput(revoffboldon);
		else vidput(revoff);
	}
}

static void displaylineinfo()  /* display the upper right corner info */
{
	static INT lastsize;
	INT i, m, n, o;
	INT32 vidcmds[10];
	CHAR fileinfo[MAXDSPINFO], lineinfo[MAXDSPINFO], *ptr;

	o = 0;
	if (dsplineinfoflg) {
		lineinfo[o++] = ' ';
		m = buf->linenum;
		for (i = 0; TRUE; i++) {  /* do twice */
			if (m >= 10000) lineinfo[o++] = (m / 10000) + '0';
			if (m >= 1000) lineinfo[o++] = (m / 1000) % 10 + '0';
			if (m >= 100) lineinfo[o++] = (m / 100) % 10 + '0';
			if (m >= 10) lineinfo[o++] = (m / 10) % 10 + '0';
			lineinfo[o++] = m % 10 + '0';
			if (i) break;
			lineinfo[o++] = '/';
			m = buf->linecount;
		}
	}
	lineinfo[o++] = ' ';
	lineinfo[o++] = 'B';
	if (windowstatus == 2) lineinfo[o++] = window2bufindex + '1';
	else lineinfo[o++] = window1bufindex + '1';
	lineinfo[o] = 0;
	n = (INT)strlen(buf->filename);
	if (n > MAXDSPINFO - 1 - o) {
		m = MAXDSPINFO - 1 - o;
		memcpy(fileinfo + 3, buf->filename + n - (m - 3), m - 3);  /* leave room to prefix "..." */
		fileinfo[m] = 0;
		fileinfo[0] = fileinfo[1] = fileinfo[2] = '.';
	}
	else strcpy(fileinfo, buf->filename);
	strcat(fileinfo, lineinfo);
	n = (INT)strlen(fileinfo);
	if (cursorflg) {
		vidput(cursoroff);
		cursorflg = FALSE;
	}
	m = 0;
	vidcmds[m++] = VID_WIN_TOP;
	if (keyflg) {
		vidcmds[m++] = VID_HORZ | 0;
		vidcmds[m++] = VID_VERT | 0;
		vidcmds[m++] = VID_EL;
		lastsize = 0;
	}
	if (lastsize > n) {
		vidcmds[m++] = VID_HORZ + scolumns - lastsize;
		vidcmds[m++] = VID_VERT;
		vidcmds[m++] = VID_DSP_BLANKS + lastsize - n;
	}
	else {
		vidcmds[m++] = VID_HORZ + scolumns - n;
		vidcmds[m++] = VID_VERT;
	}
	vidcmds[m++] = VID_DISPLAY + n;
	if (sizeof(void *) > sizeof(INT32)) {
		ptr = fileinfo;
		memcpy((void *) &vidcmds[m], (void *) &ptr, sizeof(void *));
		m += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
	}
	else *(UCHAR **)(&vidcmds[m++]) = (UCHAR *) fileinfo;
	vidcmds[m] = VID_FINISH;
	vidput(vidcmds);
	vidput(setwindow);
	lastsize = n;
	keyflg = linenumchgflg = FALSE;
}

static void doframe()  /* set buf->windowtopline, buf->windowleftoffset */
/* so that buf->linenum and buf->cursorhorz are in the current window */
{
	if (buf->linecount <= wlines) buf->windowtopline = 1;
	else if (buf->linenum < buf->windowtopline || buf->linenum >= buf->windowtopline + wlines) {
		if (buf->linenum <= wlines / 2) buf->windowtopline = 1;
		else {
			buf->windowtopline = buf->linenum - wlines / 2;
			if (buf->windowtopline + wlines - 1 > buf->linecount)
				buf->windowtopline = buf->linecount - wlines + 1;
		}
	}
	if (buf->cursorhorz < buf->windowleftoffset) buf->windowleftoffset = buf->cursorhorz;
	else if (buf->cursorhorz && buf->cursorhorz >= buf->windowleftoffset + scolumns)
		buf->windowleftoffset = buf->cursorhorz - scolumns + 1;
	setcursorhorz(buf->cursorhorz - buf->windowleftoffset);
	setcursorvert(buf->linenum - buf->windowtopline);
}

static INT horztooffset(INT horz)  /* convert cursor horz position to line offset */
{
	INT m, n;
	UCHAR *p, *q;

	m = n = 0;
	if (horz) {
		p = (*buf->curline)->data;
		q = p + (*buf->curline)->size;
		while (p < q && m < horz) {
			if (*p++ != 9) m++;
			else m += tablookup[m];
			n++;
		}
	}
	return(n);
}

static INT offsettohorz(INT offset)  /* convert line offset to cursor horz position */
{
	INT n;
	UCHAR *p, *q;

	n = 0;
	if (offset) {
		p = (*buf->curline)->data;
		q = p + (*buf->curline)->size;
		while (p < q && offset--) {
			if (*p++ != 9) n++;
			else n += tablookup[n];
		}
	}
	return(n);
}

static INT horztohorz(INT horz)  /* convert current cursor horz to a dispaly horz value */
{
	INT n;
	UCHAR *p, *q;

	n = 0;
	if (horz) {
		p = (*buf->curline)->data;
		q = p + (*buf->curline)->size;
		while (p < q && n < horz) {
			if (*p++ != 9) n++;
			else n += tablookup[n];
		}
	}
	return(n);
}

static void insertline(BUFFERPTR b, LINEHANDLE line)  /* insert the line after the current line */
/* code depends on and updates b->curline, b->linenum and b->linecount */
{
	(*line)->next = (*b->curline)->next;
	if ((*b->curline)->next == NULL) b->lastline = line;
	else (*(*b->curline)->next)->prev = line;
	(*line)->prev = b->curline;
	(*buf->curline)->next = line;
	b->curline = line;
	if (markstatus && b == &buffers[markbufindex]) {
		if (b->beginmark > b->linenum) b->beginmark++;
		if (b->endmark > b->linenum) b->endmark++;
	}
	b->linenum++;
	b->linecount++;
}

/**
 * delete the current line in buffer b
 * code depends on and updates b->curline, b->linenum and b->linecount
 */
static void deleteline(BUFFERPTR b)
{
	UCHAR checkmarkflg;
	LINEHANDLE line;

	if (markstatus && b == &buffers[markbufindex]) checkmarkflg = TRUE;
	else checkmarkflg = FALSE;
	if (b->linecount == 1) {
		if ((*b->curline)->size) chglinesize(b->curline, 0);
		if (checkmarkflg) markstatus = 0;
	}
	else {
		line = b->curline;
		if (line != NULL && b->linenum == 1) {
			b->curline = b->firstline = (*line)->next;
			if ((*line)->next != NULL) (*(*line)->next)->prev = NULL;
			if (checkmarkflg) {
				if (b->endmark == 1) markstatus = 0;
				else if (b->beginmark > 1) b->beginmark--;
				b->endmark--;
			}
		}
		else if (line != NULL && b->linenum == b->linecount) {
			b->curline = b->lastline = (*line)->prev;
			(*(*line)->prev)->next = NULL;
			if (checkmarkflg) {
				if (b->beginmark == b->linenum) markstatus = 0;
				else if (b->endmark == b->linenum) b->endmark--;
			}
			b->linenum--;
		}
		else if (line != NULL) {
			b->curline = (*(*line)->prev)->next = (*line)->next;
			if ((*line)->next != NULL) (*(*line)->next)->prev = (*line)->prev;
			if (checkmarkflg) {
				if (b->beginmark > b->linenum) b->beginmark--;
				else if (b->beginmark == b->linenum &&
					(markstatus == 1 || b->endmark == b->linenum))
					markstatus = 0;
				if (b->endmark >= b->linenum) b->endmark--;
			}
		}
		b->linecount--;
	}
}

static int edittab(CHAR *s) /* initialize the tab lookup table, return 0 if OK */
{
	INT j, k, num = 0, pnum, rptflg;
	CHAR *p;

	for (j = 0; j < 512; tablookup[j++] = 1);
	for (p = tabset; *s; s++) if (*s != ' ') *p++ = *s;
	*p = 0;
	p = tabset;
	for (j = rptflg = pnum = 0; *p || (rptflg == 2 && j + num < 255); ) {
		if (rptflg != 2) for (num = 0; isdigit(*p); p++) num = num * 10 + (*p - '0');
		if ((*p && (rptflg || (*p != ':' && *p != ','))) || (!rptflg && num <= pnum)) return(-1);
		if (*p == ':' || rptflg == 1) rptflg++;
		if (rptflg) k = num;
		else {
			k = num - pnum;
			pnum = num;
		}
		if (!j && k == 1 && !(*p)) break;
		else for (k -= (!j) ? 1 : 0; k > 0; ) tablookup[j++] = k--;
		if (rptflg != 2 && *p) p++;
	}
	if (j > 0) tablookup[j - 1] = 1;
	return(0);
}

static void message(CHAR *msg)  /* display a message on the top line */
{
	static INT32 dspmessage[10] = { VID_CURSOR_OFF, VID_WIN_RESET, VID_HU,
		VID_DISPLAY };
	INT i;

	dspmessage[3] = VID_DISPLAY + (INT32)strlen(msg);
	i = 4;
	if (sizeof(void *) > sizeof(INT32)) {
		memcpy((void *) &dspmessage[i], (void *) &msg, sizeof(void *));
		i += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
	}
	else *(UCHAR **)(&dspmessage[i++]) = (UCHAR *) msg;
	dspmessage[i++] = VID_EL;
	dspmessage[i] = VID_FINISH;
	vidput(dspmessage);
#if 0
	if (!dsplineinfoflg) displaylineinfo();
	else
#endif
	vidput(setwindow);
	keyflg = FALSE;
	msgflg = TRUE;
}

static void rdwtcfg(INT rdwtflg)  /* read (rdwtflg = TRUE) or write config file */
{
	INT m, n, filenum;
	USHORT i;
	INT32 vidcmds[12];
	static CHAR *editcfg = "edit9.cfg";
	static CHAR *ident = "EDIT9CFG0001";

	if (!rdwtflg) {  /* read */
		filenum = fioopen(editcfg, FIO_M_ERO | FIO_P_CFG);
		if (filenum < 1) {
			strcpy(tabset, "1:1");
			edittab(tabset);
			goto build;
		}
		n = fioread(filenum, 0L, filedata, 512);
		if (n < 50 || memcmp(filedata, ident, 12)) goto build;
		strcpy(tabset, (CHAR *) &filedata[12]);
		if (edittab(tabset)) goto build;
		strcpy(backext, (CHAR *) &filedata[44]);
		for (i = 0; options[i].text != NULL; i++) options[i].value = filedata[i * 2 + 48] + (filedata[i * 2 + 49] << 8);
	}
	else {  /* write */
		filenum = fioopen(editcfg, FIO_M_PRP | FIO_P_CFG);
		if (filenum < 1) {
			message("Unable to create edit configuration file");
			goto build;
		}
		memcpy(filedata, ident, 12);
		memcpy(&filedata[12], tabset, 32);
		memcpy(&filedata[44], backext, 4);
		for (i = 0; options[i].text != NULL; i++) {
			filedata[i * 2 + 48] = (UCHAR) options[i].value;
			filedata[i * 2 + 49] = (UCHAR) (options[i].value >> 8);
		}
		if (fiowrite(filenum, 0L, filedata, i * 2 + 48) < 0) message("Unable to write edit configuration file");
	}
	fioclose(filenum);
build:  /* make options take effect */
	memset(cmdtable, 0, 512 * sizeof (USHORT));
	for (i = 32; i < 256; i++) cmdtable[i] = i;
	for (i = 0; options[i].text != NULL; i++) {
		if (options[i].type > 600) cmdtable[options[i].value] = options[i].type;
		else {
			switch (options[i].type) {
				case OPT_WINSPLIT:
					windowsplitline = options[i].value;
					break;
				case OPT_TABDSPCHAR:
					tabdisplay = (UCHAR) options[i].value;
					break;
				case OPT_TABDSP:
					tabdisplayflg = (UCHAR) options[i].value;
					break;
				case OPT_FILECOMP:
					filecompflg = (UCHAR) options[i].value;
					break;
				case OPT_LINENUM:
					dsplineinfoflg = (UCHAR) options[i].value;
					break;
				case OPT_CASESENSE:
					searchcaseflg = (UCHAR) options[i].value;
					break;
				case OPT_FGCOLOR:
					fgcolor = options[i].value;
					break;
				case OPT_BGCOLOR:
					bgcolor = options[i].value;
					break;
				case OPT_BRIGHT:
					brightflg = (UCHAR) options[i].value;
					break;
				case OPT_SRCHFORCE:
					forcechar = (UCHAR) options[i].value;
					break;
				case OPT_SRCHMATCH:
					matchanychar = (UCHAR) options[i].value;
					break;
				case OPT_SRCHLEFTANCHOR:
					leftanchorchar = (UCHAR) options[i].value;
					break;
				case OPT_SRCHRIGHTANCHOR:
					rightanchorchar = (UCHAR) options[i].value;
					break;
				case OPT_TRANINSERT:
					matchinsertchar = (UCHAR) options[i].value;
					break;
				case OPT_KEYEND:
					keyend = options[i].value;
					break;
				case OPT_KEYABORT:
					keyabort = options[i].value;
					break;
				case OPT_KEYBKSPC:
					keybkspc = options[i].value;
					break;
				case OPT_KEYRECALL:
					keyrecall = options[i].value;
					break;
				case OPT_BUFFER1:
				case OPT_BUFFER2:
				case OPT_BUFFER3:
				case OPT_BUFFER4:
				case OPT_BUFFER5:
				case OPT_BUFFER6:
				case OPT_BUFFER7:
				case OPT_BUFFER8:
				case OPT_BUFFER9:
					m = options[i].type - OPT_BUFFER1;
					buffers[m].windownumber = options[i].value;
					break;
			}
		}
	}
	vidcmds[0] = VID_CURSOR_OFF;
	vidcmds[1] = VID_CURSOR_ULINE;
	vidcmds[2] = VID_WIN_RESET;
	vidcmds[3] = VID_FOREGROUND | (VID_BLACK + fgcolor);
	vidcmds[4] = VID_BACKGROUND | (VID_BLACK + bgcolor);
	if (brightflg) vidcmds[5] = VID_BOLD_ON;
	else vidcmds[5] = VID_BOLD_OFF;
	vidcmds[6] = VID_ES;
	vidcmds[7] = VID_VERT + 1;
	vidcmds[8] = VID_REPEAT + 80;
	vidcmds[9] = VID_DISP_SYM + VID_SYM_HLN;
	vidcmds[10] = VID_FINISH;
	vidput(vidcmds);
	for (m = 0; m < MAXBUFFER && buffers[m].windownumber != 1; m++);
	if (m == MAXBUFFER) m = 0;
	setcurrentbufwin(m);
	vidput(setwindow);
}

/* convert keystroke code to text */
static CHAR *keytext(INT keynum)
{
	static CHAR result[20];

	if (keynum < 256 || (keynum > 380 && keynum < 407)) {
		if (keynum < 27 || keynum > 255) {
			if (keynum < 27) {
				strcpy(result, "CTL  ");
				result[4] = keynum + 'A' - 1;
			}
			else {
				strcpy(result, "ALT  ");
				result[4] = keynum + 'A' - 381;
			}
		}
		else {
			result[0] = keynum;
			result[1] = 0;
		}
	}
	else if (keynum < 261) {
		switch (keynum) {
			case VID_ENTER: return("ENTER");
			case VID_ESCAPE: return("ESC");
			case VID_BKSPC: return("BKSPC");
			case VID_TAB: return("TAB");
			case VID_BKTAB: return("BKTAB");
		}
	}
	else if (keynum < 301) {
		if (keynum > 290) strcpy(result, "ALT ");
		else if (keynum > 280) strcpy(result, "CTL ");
		else if (keynum > 270) strcpy(result, "SHIFT ");
		else result[0] = 0;
		switch (keynum % 10) {
			case 0: strcat(result, "PGDN"); break;
			case 1: strcat(result, "UP"); break;
			case 2: strcat(result, "DOWN"); break;
			case 3: strcat(result, "LEFT"); break;
			case 4: strcat(result, "RIGHT"); break;
			case 5: strcat(result, "INSERT"); break;
			case 6: strcat(result, "DELETE"); break;
			case 7: strcat(result, "HOME"); break;
			case 8: strcat(result, "END"); break;
			case 9: strcat(result, "PGUP"); break;
		}
	}
	else if (keynum < 381) {
		if (keynum > 360) strcpy(result, "ALT F");
		else if (keynum > 340) strcpy(result, "CTL F");
		else if (keynum > 320) strcpy(result, "SHIFT F");
		else if (keynum > 290) strcpy(result, "F");
		mscitoa((keynum - 1) % 20 + 1, &result[strlen(result)]);
	}
	else return("UNKNOWN KEY");
	return(result);
}

static INT charget()
{
	INT i1, i2;
	UCHAR work[8];

	evtclear(eventid);
	if (vidkeyinstart(work, 1, 1, 1, FALSE, eventid)) {
/*** CODE: ADD ERROR CODE ***/
	}
	evtwait(&eventid, 1);
	vidkeyinend(work, &i1, &i2);
	if (i2 < 0) i2 = 0;
	return(i2);
}

/* USAGE */
static void usage()
{
	dspstring("EDIT command  " RELEASEPROGRAM RELEASE COPYRIGHT);
	dspchar('\n');
	dspstring("Usage:  edit [file] [-A=n] [-CFG=cfgfile] [-V]\n");
	dspstring("             [-T=[TEXT|DATA|STD|CRLF]]\n");
	exit(1);
}

/* QUITSIG */
static void quitsig(INT sig)
{
	signal(sig, SIG_IGN);
	setwindow[0] = VID_WIN_RESET;
	setwindow[1] = VID_BACKGROUND | VID_BLACK;
	setwindow[2] = VID_ES;
	vidput(setwindow);
	videxit();
	cfgexit();
	exit(1);
}
