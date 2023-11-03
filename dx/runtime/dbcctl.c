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
#define INC_ERRNO
#define INC_LIMITS
#define INCLUDE_DDE_H
#define INC_ASSERT
#include "includes.h"
#include "release.h"
#include "arg.h"
#include "base.h"
#include "cert.h"
#include "dbc.h"
#include "dbccfg.h"
#include "dbcclnt.h"
#include "evt.h"
#include "fio.h"
#include "fsfileio.h"
#include "gui.h"
#include "guix.h"
#define LCS_MACHINE_WINI	0x02
#define LCS_MACHINE_LINUXI	0x08
#define LCS_MACHINE_MAC		0x13
#define LCS_MACHINE_FREEBSD 0x09
#include "que.h"
#include "tcp.h"
#include "tim.h"
#include "vid.h"
#include "xml.h"
#include "imgs.h"
#if OS_WIN32
#include "svc.h"
#endif

/**
 * Some *nixes don't define this for some reason
 */
#ifndef ERANGE
#define ERANGE 34
#endif

CHAR pgmname[20] = "DB/C DX";

extern INT pgmchn;
extern INT fioaoperr;
extern INT fioaclerr;
extern INT fioarderr;
extern INT fioawrerr;
extern INT fioaskerr;
extern INT fioalkerr;

/* for use by dbcprt.c */
extern PIXHANDLE refnumtopixhandle(INT);
extern CHAR * refnumtoimagename(INT);

#define DEV_DSR				1
#define DEV_WINDOW			2
#define DEV_CLIPBOARD		3
#define DEV_TIFFILE			4
#define DEV_GIFFILE			5
#define DEV_JPGFILE			6
#define DEV_DDE				7
#define DEV_TIMER			8
#define DEV_APPLICATION		9
#define DEV_PNGFILE        10

#define KEYWORD_MENU		 1
#define KEYWORD_PANEL		 2
#define KEYWORD_DIALOG		 3
#define KEYWORD_TOOLBAR		 4
#define KEYWORD_ICON		 5
#define KEYWORD_TITLE		 6
#define KEYWORD_NOCLOSE		 7
#define KEYWORD_POS			 8
#define KEYWORD_ABSPOS		 9
#define KEYWORD_SIZE		 10
#define KEYWORD_MAIN		 11
#define KEYWORD_ITEM		 12
#define KEYWORD_SUBMENU		 13
#define KEYWORD_ENDSUBMENU	 14
#define KEYWORD_KEY			 15
#define KEYWORD_LINE		 16
#define KEYWORD_H			 17
#define KEYWORD_V			 18
#define KEYWORD_HA			 19
#define KEYWORD_VA			 20
#define KEYWORD_BOX			 21
#define KEYWORD_BOXTITLE	 22
#define KEYWORD_TEXT		 23
#define KEYWORD_BOXTEXT		 24
#define KEYWORD_EDIT		 25
#define KEYWORD_CHECKBOX	 26
#define KEYWORD_PUSHBUTTON	 27
#define KEYWORD_BUTTON		 28
#define KEYWORD_BUTTONGROUP	 29
#define KEYWORD_ENDBUTTONGROUP 30
#define KEYWORD_LISTBOX		 31
#define KEYWORD_DROPBOX		 32
#define KEYWORD_HSCROLLBAR	 33
#define KEYWORD_VSCROLLBAR	 34
#define KEYWORD_PROGRESSBAR	 35
#define KEYWORD_DEFPUSHBUTTON 36
#define KEYWORD_FIXSIZE		 37
#define KEYWORD_ESCPUSHBUTTON 38
#define KEYWORD_VTEXT		 39
#define KEYWORD_CTEXT		 40
#define KEYWORD_INSERTORDER	 41
#define KEYWORD_BOXTABS		 42
#define KEYWORD_MEDIT		 43
#define KEYWORD_LOCKBUTTON	 44
#define KEYWORD_ICONLOCKBUTTON 45
#define KEYWORD_ICONPUSHBUTTON 46
#define KEYWORD_ICONDEFPUSHBUTTON 47
#define KEYWORD_ICONESCPUSHBUTTON 48
#define KEYWORD_FEDIT		 49
#define KEYWORD_COLORBITS	 50
#define KEYWORD_PIXELS		 51
#define KEYWORD_SPACE		 52
#define KEYWORD_PEDIT		 53
#define KEYWORD_RATEXT		 54
#define KEYWORD_RTEXT		 55
#define KEYWORD_RVTEXT		 56
#define KEYWORD_OPENFILEDLG	 57
#define KEYWORD_SAVEASFILEDLG 58
#define KEYWORD_FONTDLG		 59
#define KEYWORD_COLORDLG	 60
#define KEYWORD_GRAY		 61
#define KEYWORD_SHOWONLY	 62
#define KEYWORD_TIMER		 63
#define KEYWORD_WINDOW		 64
#define KEYWORD_TABGROUP	 65
#define KEYWORD_TAB			 66
#define KEYWORD_TABGROUPEND	 67
#define KEYWORD_POPUPMENU	 68
#define KEYWORD_DEFAULT		 69
#define KEYWORD_DDE			 70
#define KEYWORD_LEDIT		 71
#define KEYWORD_MLEDIT		 72
#define KEYWORD_READONLY	 73
#define KEYWORD_FONT		 74
#define KEYWORD_TEXTCOLOR	 75
#define KEYWORD_PANDLGSCALE	 76
#define KEYWORD_MAXIMIZE	 77
#define KEYWORD_NOSCROLLBARS 78
#define KEYWORD_NOFOCUS		 79
#define KEYWORD_MLISTBOX	 80
#define KEYWORD_PLEDIT		 81
#define KEYWORD_TREE        82
#define KEYWORD_LISTBOXHS   83
#define KEYWORD_MEDITHS	     84
#define KEYWORD_MEDITVS		 85
#define KEYWORD_MEDITS		 86
#define KEYWORD_MLEDITHS	 87
#define KEYWORD_MLEDITVS	 88
#define KEYWORD_MLEDITS		 89
#define KEYWORD_MLISTBOXHS	 90
#define KEYWORD_NOTASKBARBUTTON 91
#define KEYWORD_HELPTEXT     92
#define KEYWORD_CVTEXT       93
#define KEYWORD_VICON        94
#define KEYWORD_STATUSBAR    95
#define KEYWORD_LTCHECKBOX   96
#define KEYWORD_POPBOX       97
#define KEYWORD_TABLE        98
#define KEYWORD_TABLEEND     99
#define KEYWORD_CDROPBOX     100		/* 'common' dropbox, used only in tables */
#define KEYWORD_ICONITEM     101       /* menu item with an icon */
#define KEYWORD_ICONSUBMENU  102       /* submenu item with an icon */
#define KEYWORD_CHECKITEM    103       /* menu item that can be checked, only affects Java SC, otherwise identical to ITEM */
#define KEYWORD_JUSTIFYCENTER 104
#define KEYWORD_JUSTIFYLEFT  105
#define KEYWORD_JUSTIFYRIGHT 106
#define KEYWORD_OPENDIRDLG   107
#define KEYWORD_ATEXT		 108
#define KEYWORD_CATEXT		 109
#define KEYWORD_ALERTDLG	 110
#define KEYWORD_FLOATWINDOW  111
#define KEYWORD_NOHEADER     112
#define KEYWORD_DEVICEFILTER 113
#define KEYWORD_OWNER        114
#define KEYWORD_TEXTPUSHBUTTON 115
#define KEYWORD_MULTIPLE    116

/* definitions and declarations for device timers */
#define INVALIDTIMERHANDLE -1
#define DEVTIMERALLOCAMT		32
#define MAXTIMERNAMELEN	8

typedef LONG DEVTIMERHANDLE;

typedef struct devtimerstruct {
	INT refnum;			/* OPENINFO reference number */
	UCHAR activeflg;		/* set to TRUE when this timer device is active */
	UCHAR name[MAXTIMERNAMELEN];	/* timer name */
	UCHAR expiretime[16];	/* time after which this timer has expired */
	UINT timestep;			/* number of centi-seconds between timer ticks for recurring timers */
	DEVTIMERHANDLE next;	/* next device timer in devtimerlist or devtimerfreelist */
} DEVTIMER;

static DEVTIMER **devtimerstorage;			/* array of device timer structure */
static DEVTIMERHANDLE devtimerlist = INVALIDTIMERHANDLE; 		/* linked list of all currently active device timers */
static DEVTIMERHANDLE devtimerfreelist = INVALIDTIMERHANDLE;	/* linked list of freed device timer structures */
static INT devtimeralloc;				/* number of device timer structures allocated in devtimerstorage[] array */
static INT devtimernext;					/* next unused device structure in the devtimerstorage[] array */
static INT devtimerhandle = -1;			/* handle to MSC timer */
static UCHAR devexpiretime[] = TIMMAXTIME;	/* time associated with devtimerhandle */
#if OS_UNIX
static UCHAR ka_timstamp[16];	/* Keep Alive TimeStamp */
static INT katimerhandle;
#endif

#define ISVALIDTIMERHANDLE(x) (x >= 0)
#define DEVTIMERHANDLETOPTR(x) (&(*devtimerstorage)[x])

typedef struct openinfostruct {
	UCHAR davbtype;		/* DAVB_IMAGE, DAVB_DEVICE, DAVB_RESOURCE or DAVB_QUEUE */
	UCHAR devtype;			/* DEV_ values */
	INT datamod;			/* data module of image, device or resource */
	INT dataoff;			/* data offset of image, device or resource */
	union {
		INT inthandle;		/* general INT handle */
		PIXHANDLE pixhandle;/* IMAGE var pixhandle */
		WINHANDLE winhandle;/* window device winhandle */
		RESHANDLE reshandle;/* RESOURCE var resource handle */
		DEVTIMERHANDLE timerhandle;	/* timer device handle */
		QUEHANDLE quehandle;/* QUEUE var queue handle */
	} u;
	union {
		INT srvhandle;		/* server handle if image file */
		QUEHANDLE linkquehandle; /* handle of queue linked to RESOURCE or DEVICE var */
		INT qvareventid;	/* handle of eventid associated with QUEUE var */
	} v;
	INT timestep;			/* number of seconds between each timer TICK message for device timers */
	INT trapindex;			/* trap index for QUEUE var */

	INT tifnum;				/* the number of the image to load in a multiple image TIF */
	/*
	 * device or resource name, used by smart server
	 * For images, this is the one-based index into the openinfo array
	 */
	CHAR name[9];
	CHAR ident[9];			/* somewhat unique identifier for each resource */
	UCHAR restype;			/* RES_TYPE_ for resources, used by smart server */
} OPENINFO;

static INT openinfohi;		/* array index of open dadb array */
static INT openinfomax;		/* size of open dadb array */
static OPENINFO **openinfopptr; /* memory pointer to open res/dev table */

static CHAR scanstring[256];	/* zero delimited result of scanname, scantext */
static UINT scanstringlen;	/* length of scanstring */
static UCHAR *scanstart;		/* scan functions, current position in string */
static UCHAR *scanend;		/* scan functions, ptr to last position in string */
static UCHAR *scanlastname;	/* start of last name scanned in string */

static INT maineventids[EVT_MAX_EVENTS];
static INT maineventidcount;
static INT actionbit, *actionvar;

/* key event traps must occur, before non-key traps to support disable */
#define EVENT_BREAK			0
#define EVENT_TRAPCHAR		1  /* dbcwait() assumes this is last key event */
#define EVENT_SHUTDOWN		2
#define EVENT_DEVTIMER		3
#define EVENT_TRAPTIMER		4
#if OS_UNIX
#define EVENT_KATIMER       5
#define EVENT_MAXIMUM       6
#else
#define EVENT_MAXIMUM       5
#endif

#define TRAPHDR 5

/**
 * Was 13 for 16.1.x, is 17 now
 * When 'program module number' was expanded from two bytes to four,
 * that added 4 bytes to this, there are two modules numbers stored.
 */
#define TRAPSIZE (INT)(9 + (2 * sizeof(INT)))
#define DEBUGKEY 0x04

/* don't change the order or values of the following without checking the code */
#define TRAPPARITY		0
#define TRAPRANGE		1
#define TRAPFORMAT		2
#define TRAPCFAIL		3
#define TRAPIO			4
#define TRAPSPOOL		5
#define TRAPTIMER		6
#define TRAPQUEUE		7
#define TRAPALLKEYS		8
#define TRAPALLCHARS	9
#define TRAPALLFKEYS	10
#define TRAPMISC		11
#define TRAPMAX		128  /* must support at least 96 chars to be ansi conforming */

/* trap value mask (values stored can not exceed this define) */
#define TRAPVALUE_MASK	0x000FFF

/* trap miscellaneous values (consecutive numbering is supported) */
#define TRAPTYPE_QUEUE	0x001000
#define TRAPTYPE_CHAR	0x002000
#define TRAPTYPE_MASK	0x00F000

/* trap.value flag values, note values correspond with .dbc file coding */
#define TRAPSET		0x010000
#define TRAPNORESET		0x020000
#define TRAPPRIOR		0x040000
#define TRAPNOCASE		0x080000
#define TRAPDEFER		0x100000

#define TRAPALLKEYS_STARTEND { start1 = 1; end1 = VID_PGDN; start2 = VID_F1; end2 = VID_F20; }
#define TRAPALLCHARS_STARTEND { start1 = 32; end1 = 254; start2 = 255; end2 = 255; }
#define TRAPALLFKEYS_STARTEND { start1 = VID_ENTER; end1 = VID_PGDN; start2 = VID_F1; end2 = VID_F20; }

struct trapstruct {
	INT type;			/* llmmhh, low 2 bytes are character value or'd with flag values */
	INT module;			/* INT, module numbers */
	INT pcount;			/* llmmhh, pcount in the module */
	INT givmod;			/* INT, giving module */
	INT givoff;			/* llmmhh, giving variable offset */
} trap[TRAPMAX];

static INT guilock = FALSE;
static INT timerhandle = -1;	/* time routines handle of trap timer */
static UCHAR timertimestamp[16]; /* timestamp of trap timer */
static INT errornum;		/* error number for errorexec */
static UCHAR *errorinfoptr;	/* pointer to extra info for errorexec */
static UINT errorinfolen;	/* length of error info pointed to by errorinfoptr */
static CHAR errormsg[160];	/* error message for geterrmsg */
static CHAR oldtrapflg;		/* traps behave like v6, (case insensitive and prior) */
static UCHAR trapcharmap[64];	/* character trap map */
static UCHAR trapnullmap[64];  /* null character trap map (used during disable) */
static UCHAR *trapmap;		/* pointer to active trap map */
static UCHAR trapcharindex[512];  /* index into trap array */
static INT trapmaxindex = -1;  /* high water mark of trap array usage */
static INT lasttrap = -1;
static INT breakeventid;
static UCHAR disableflag;
static UCHAR stopflag;
static UCHAR fkeyshiftflag;	/* true if dbc_fkeyshift=old */
static UINT imgstorebufsize;

#ifndef DX_MACHINE
#if OS_WIN32
#define DX_MACHINE LCS_MACHINE_WINI
#elif defined(__MACOSX)
#define DX_MACHINE LCS_MACHINE_MAC
#elif defined(FREEBSD)
#define DX_MACHINE LCS_MACHINE_FREEBSD
#else
#define DX_MACHINE LCS_MACHINE_LINUXI
#endif
#endif

static FILE *clientlogfile;
static UCHAR dbcinitflag = FALSE;

#if OS_WIN32
typedef void (_DSRINIT)(void (**)(void));
typedef INT (_DSROPEN)(CHAR *);
typedef INT (_DSRCLOSE)(INT);
typedef INT (_DSRQUERY)(INT, CHAR *, UCHAR *, INT *);
typedef INT (_DSRCHANGE)(INT, CHAR *, UCHAR *, INT);
typedef INT (_DSRLOAD)(INT, struct pixmapstruct **);
typedef INT (_DSRSTORE)(INT, struct pixmapstruct **);
typedef INT (_DSRLINK)(INT, struct queuestruct **);
typedef INT (_DSRUNLINK)(INT, struct queuestruct **);
static _DSRINIT *dsrinit;
static _DSROPEN *dsropen;
static _DSRCLOSE *dsrclose;
static _DSRQUERY *dsrquery;
static _DSRCHANGE *dsrchange;
static _DSRLOAD *dsrload;
static _DSRSTORE *dsrstore;
static _DSRLINK *dsrlink;
static _DSRUNLINK *dsrunlink;
static void (*quefunc[])(void) = {
	(void (*)(void)) queget,
	(void (*)(void)) queput,
	(void (*)(void)) quewait
};
static UCHAR dsrinitflag = 0;
static HINSTANCE dsrhandle;
#endif

#if OS_UNIX
extern INT dsropen(CHAR *);
extern INT dsrclose(INT);
extern INT dsrquery(INT, CHAR *, UCHAR *, INT *);
extern INT dsrchange(INT, CHAR *, UCHAR *, INT);
extern INT dsrload(INT, void *);
extern INT dsrstore(INT, void *);
extern INT dsrlink(INT, INT);
extern INT dsrunlink(INT, INT);
#endif

#if OS_UNIX
#if defined(SA_RESTART) | defined(SA_INTERRUPT) | defined(SA_RESETHAND)
static struct sigaction oldterm, oldhup, oldquit;
#else
static void (*oldterm)(INT);
static void (*oldhup)(INT);
static void (*oldquit)(INT);
#endif
static void sigevent(INT);
#endif

extern INT clientsendgreeting(void);

static INT actiontrap(INT);
static void closerefnum(INT, INT);
static void cltputihv(CHAR *tag, INT item, INT h1, INT v1);
static void ctlappchange(UCHAR *, INT, UCHAR *, INT);
static void ctlchange(void);
static void ctlclose(void);
static void ctldraw(void);
static void ctlempty(void);
static void ctlget(void);
static void ctlgetcolor(void);
static void ctlgetpos(void);
static void ctlhide(void);
static void ctllink(void);
static void ctlload(void);
static INT ctlmemicmp(void *, void *, INT);
static void ctlopen(void);
static void ctlprep(void);
static void ctlput(INT);
static void ctlquery(void);
static void ctlshow(void);
static void ctlshowpos(void);
static void ctlstore(void);
static void ctltimerchange(OPENINFO *, UCHAR *, INT, UCHAR *, INT);
static void ctlunlink(void);
static void ctlwaitall(void);
static void ctlwait(void);
static void devicecbfnc(WINHANDLE, UCHAR *, INT);
static INT devtimercreate(UCHAR *, DEVTIMERHANDLE *, INT);
static INT devtimerset(DEVTIMERHANDLE, UCHAR *, UINT);
static void devtimerclear(DEVTIMERHANDLE);
static void devtimerevent(void);
static INT devtimerdestroy(DEVTIMERHANDLE);
static UINT getdrawvalue(void);
static INT getnewrefnum(INT *);
static CHAR* interpretIconName(CHAR* text);
static INT keywordToPanelType(INT kw);
static void makeres(RESHANDLE, UCHAR *, INT, OPENINFO *);
static void makeresmenu(RESHANDLE reshandle, UCHAR *resname, INT keywordval, OPENINFO *openinfo);
static void makerespnldlg(RESHANDLE reshandle, UCHAR *resname, INT keywordval, OPENINFO *openinfo);
static INT nametokeywordvalue(void);
static INT processTablePrepString(void);
static void processTablePrepStringForSC(void);
static void resourcecbfnc(RESHANDLE, UCHAR *, INT);
static void scancolon(void);
static INT scancomma(void);
static void scanequal(void);
static void scanerror(INT);
static void scanfont(void);
static void scaninit(UCHAR *, INT);
static void scanname(void);
static INT scannumber(void);
static void scantext(void);
static void showResourceLocal(RESHANDLE reshandle, WINHANDLE winhandle, INT hpos, INT vpos);
static INT trapcode(INT);
static void trapenable(void);
static void trapdisable(void);

/* the following are gui server functions: */
static INT guiservergenfont(CHAR *, INT fromDraw);
static INT guiserverwinchange(CHAR *, UCHAR *, INT, UCHAR *, INT);
static void guiserverreschange(CHAR *, UCHAR, UCHAR *, INT, UCHAR *, INT, INT);
static void getuniqueid(CHAR *);
static INT num5toi(UCHAR *, INT *);
static void numtoi(UCHAR *, INT, INT *);
static void clientcbfnc(ELEMENT *);
static CHAR * windowNameToIdent(CHAR *name);
static void ctldspstring(char *);

#if OS_WIN32
static void ddecbfnc(INT, UCHAR *, INT);
static void loaddsr(void);
static void freedsr(void);
#endif

/*
 * keep alive messages time interval, in seconds
 * This is the default, it can be changed with dbcdx.smartserver.keepalivetime=n
 */
LONG keepalivetime = 61;


void dbcinit(INT argc, CHAR **argv, CHAR **envp)
{
	INT i1, i2, encryptflag;
	INT cfginitSucceeded = 0, prpinitSucceeded = 0;
	LONG l1;
	INT serverport = INT_MIN;
	INT usernum;
	INT clientport = 0;
	CHAR work[MAX_NAMESIZE];
	CHAR cfgname[MAX_NAMESIZE], dbcname[MAX_NAMESIZE];
	CHAR brkfile[MAX_NAMESIZE], glblfile[MAX_NAMESIZE], preffile[MAX_NAMESIZE];
	CHAR clientaddress[128], *ptr;
	ELEMENT *prpptr;
	FIOPARMS parms;
#if OS_WIN32
	INT i3;
#endif

#if OS_UNIX & (defined(SA_RESTART) | defined(SA_INTERRUPT) | defined(SA_RESETHAND))
	struct sigaction act;
#endif

#if OS_WIN32
	if (dbcflags & DBCFLAG_SERVICE) svcstatus(SVC_STARTING, 1000 + 1);
#endif

	encryptflag = 0;
	cfgname[0] = clientaddress[0] = '\0';
	smartClientSubType[0] = '\0';
	smartClientUTCOffset[0] = '\0';
	arginit(argc, argv, &i1);
	while (!argget(ARG_NEXT | ARG_IGNOREOPT, name, sizeof(name))) {
		if (name[0] == '-') {
			if (name[1] == '?') {
				if (dbcflags & DBCFLAG_CONSOLE) ctldspstring(DXAboutInfoString);
#if OS_WIN32
				else {
					MessageBox(NULL, DXAboutInfoString, "About DB/C", MB_OK);
				}
#endif
				exit(0);
			}
			if (toupper(name[1]) == 'C' && toupper(name[2]) == 'F' && toupper(name[3]) == 'G' && name[4] == '=') {
				strcpy(cfgname, &name[5]);  /* -CFG */
			}
			if (toupper(name[1]) == 'S') {
				for (i1 = 0; name[i1 + 1] && name[i1 + 1] != '=' && i1 < 16; i1++) work[i1] = (CHAR) toupper(name[i1 + 1]);
				work[i1] = '\0';
				if (!strcmp(work, "SCHOST")) {
					if (name[7] == '=') strcpy(clientaddress, name + 8);
				}
				else if (!strcmp(work, "SCPORT")) {
					if (name[7] == '=') clientport = atoi(name + 8);
					dbcflags |= DBCFLAG_SERVER;
				}
				else if (!strcmp(work, "SCLOG")) {
					if (name[6] == '=' && name[7]) clientlogfile = fopen(name + 7, "w");
				}
				else if (!strcmp(work, "SCSSL")) encryptflag = TRUE;
				else if (!strcmp(work, "SCGUI")) dbcflags |= DBCFLAG_CLIENTGUI;
				else if (!strcmp(work, "SCSERVER")) {
					if (name[9] == '=') {
						for (i1 = 10; name[i1] && name[i1] != ':'; i1++);
						if (name[i1] == ':') {
							name[i1] = '\0';
							usernum = atoi(name + i1 + 1);
						}
						serverport = atoi(name + 10);
					}
					dbcflags |= DBCFLAG_SERVER;
				}
				else if (!strcmp(work, "SCTYPE")) {
					if (name[7] == '=') {
						ptr = name + 8;
						for (i1 = MAX_SCSUBTYPE_CHARS; i1 && isalpha((int)ptr[0]); i1--)
							strncat(smartClientSubType, ptr++, 1);
					}
				}
			}
		}
	}
	cfginitSucceeded = cfginit(cfgname, /*TRUE*/ FALSE /* Not required for this version */) == 0;
	if (cfginitSucceeded) {
		prpinitSucceeded = prpinit(cfggetxml(), CFG_PREFIX "cfg") == 0;
	}

	// Initialize our memory pool
	if (!prpget("memalloc", NULL, NULL, NULL, &ptr, 0)) {
		i1 = atoi(ptr);
		if (i1 <= 0) i1 = 2048;
	}
	else i1 = 2048;

	if (!prpget("minalloc", NULL, NULL, NULL, &ptr, 0)) {
		i2 = atoi(ptr);
		if (i2 < 0) i2 = 0;
	}
	else i2 = 0;
	i1 = meminit(i1 << 10, i2 << 10, 512);
	if (i1 < 0) dbcdeath(61);

	ptr = fioinit(&parms, FALSE);
	if (ptr != NULL) dbcdeath(62);

	if (dbcflags & DBCFLAG_SERVER) {
		/* get started as soon as possible so error messages can be transmitted */
		if (serverport == INT_MIN) dbcdeath(0);
		if (prpget("smartserver", "certificatefilename", NULL, NULL, &ptr, 0)) ptr = NULL;
		if (clientstart(clientaddress, clientport, encryptflag, serverport, usernum, clientlogfile, ptr)){
			dbcdeath(64);
		}
		dbcflags |= DBCFLAG_CLIENTINIT;
	}
#if OS_WIN32
	else if ((dbcflags & DBCFLAG_SERVICE) & !*cfgname) {
		svcstatus(SVC_STARTING, 1000 + 2);
/*** NOTE:  IT APPEARS THAT ONLY A PARTIAL ENVIRONMENT BLOCK IS ATTACHED TO THE ***/
/***        CREATED PROCESS SO THIS MAY BE DONE IN VAIN ***/
		i1 = GetEnvironmentVariable("DBC_CFG", cfgname, sizeof(cfgname));
		if (!i1) {
			if (!GetModuleFileName(NULL, cfgname, sizeof(cfgname))) strcpy(cfgname, argv[0]);
			for (i1 = (INT)strlen(cfgname) - 1;
					i1 >= 0 && cfgname[i1] != '\\' && cfgname[i1] != '/' && cfgname[i1] != ':';
					i1--);
			strcpy(&cfgname[i1 + 1], "dbcdx.cfg");
		}
		else if (i1 > (INT) sizeof(cfgname)) dbcdeath(51);
		svcstatus(SVC_STARTING, 2000 + 3);
	}
#endif

	if (!cfginitSucceeded) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			if (clientsendgreeting() == RC_ERROR) dbcerror(798);
		}
		dbcdeath(51);
	}
	if (!prpinitSucceeded) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			if (clientsendgreeting() == RC_ERROR) dbcerror(798);
		}
		dbcdeath(52);
	}

	if (dbcflags & DBCFLAG_CLIENTINIT) {
		if (clientsendgreeting() == RC_ERROR) dbcerror(798);
	}
	/**
	 * Set up the keep alive time
	 */
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		if (!prpget("smartserver", "keepalivetime", NULL, NULL, &ptr, 0)) {
			l1 = strtol(ptr, NULL, 10);
			if (errno != ERANGE) keepalivetime = l1;
		}
	}

	dbcname[0] = brkfile[0] = glblfile[0] = preffile[0] = '\0';
/*** CODE: THESE ARE NOT BEING USE BUT MAY BE FOR GUICLNT/CHRCLNT ***/
	for (i1 = ARG_FIRST; ; i1 = ARG_NEXT) {
		i1 = argget(i1, name, sizeof(name));
		if (i1) {
			if (i1 < 0) dbcdeath(53);
			break;
		}
		if (name[0] == '-') {
			switch(toupper(name[1])) {
			case 'G':
				if (name[2] == '=') strcpy(glblfile, name + 3);
				break;
			case 'Z':
				dbcflags |= DBCFLAG_DEBUG;
				if (toupper(name[2]) == 'Z') vdebug(DEBUG_EXTRAINFO);
				else if (name[2] == '=' && strlen(name) > 3) {
					dbcflags |= DBCFLAG_DDTDEBUG;
					dbginit(DBGINIT_DDT, name + 3);
				}
				else if (toupper(name[2]) == 'S' && name[3] == '=' && strlen(name) > 4) {
					dbcflags |= DBCFLAG_DDTDEBUG;
					dbginit(DBGINIT_DDT_SERVER, name + 4);
				}
				break;
#if OS_WIN32
			case '!':
				SetErrorMode(SEM_NOALIGNMENTFAULTEXCEPT);
				break;
#endif
			}
		}
		else if (!dbcname[0]) strcpy(dbcname, name);
	}
	if (dbcflags & DBCFLAG_DEBUG) {
		if (*brkfile || prpget("debug", "break", NULL, NULL, &ptr, 0)) ptr = brkfile;
		if (*ptr) dbginit(DBGINIT_BREAK, ptr);
		if (*glblfile || prpget("debug", "global", NULL, NULL, &ptr, 0)) ptr = glblfile;
		if (*ptr) dbginit(DBGINIT_GLOBAL, ptr);
		if (*preffile || prpget("debug", "preference", NULL, NULL, &ptr, 0)) ptr = preffile;
		if (*ptr) dbginit(DBGINIT_PREFERENCE, ptr);
	}

	if (!(!prpget("file", "sharing", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "off"))) {
#if OS_WIN32
		if (dbcflags & DBCFLAG_SERVICE) svcstatus(SVC_STARTING, 30000 + 4);
#endif
	}

#if OS_WIN32
	else if (dbcflags & DBCFLAG_SERVICE) svcstatus(SVC_STARTING, 1000 + 5);
#endif

	if (!prpget("file", "error", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "on")) dsperrflg = TRUE;
	if (!prpget("trap", NULL, NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) oldtrapflg = TRUE;
	if (!prpget("keyin", "fkeyshift", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) fkeyshiftflag = TRUE;
	if (!prpget("file", "untrappederrors", NULL, NULL, &ptr, 0)  && *ptr) {
		writeUntrappedErrorsFlag = TRUE;
		strcpy(writeUntrappedErrorsFile, ptr);
	}
	if (!prpget("eventlog", NULL, NULL, NULL, &ptr, PRP_LOWER)  && *ptr) {
		int size = INT_MIN;
		if (mscis_integer(ptr, (INT) strlen(ptr))) {
			size = atoi(ptr);
		}
		else if (!strcmp(ptr, "on") || !strcmp(ptr, "yes")) {
			size = 50;
		}
	}

#if OS_WIN32
	if (dbcflags & DBCFLAG_SERVICE) svcstatus(SVC_STARTING, 2000 + 6);
#endif

	openinfopptr = (OPENINFO **) memalloc(8 * sizeof(OPENINFO), MEMFLAGS_ZEROFILL);
	if (openinfopptr == NULL) dbcdeath(61);
	openinfomax = 8;
	openinfohi = 0;

	maineventids[EVENT_BREAK] = evtcreate();
	maineventids[EVENT_TRAPCHAR] = evtcreate();
	maineventids[EVENT_SHUTDOWN] = evtcreate();
	maineventids[EVENT_TRAPTIMER] = evtcreate();
	maineventids[EVENT_DEVTIMER] = evtcreate();
#if OS_UNIX
	if (dbcflags & DBCFLAG_CLIENTINIT && keepalivetime > 0) {
		maineventids[EVENT_KATIMER] = evtcreate();
	}
	else maineventids[EVENT_KATIMER] = EVTBASE; /* This will be ignored by evtwait */
#endif
	if (maineventids[EVENT_BREAK] == -1 ||
	    maineventids[EVENT_TRAPCHAR] == -1 ||
	    maineventids[EVENT_SHUTDOWN] == -1 ||
	    maineventids[EVENT_TRAPTIMER] == -1 ||
#if OS_UNIX
		(dbcflags & DBCFLAG_CLIENTINIT && keepalivetime > 0 && maineventids[EVENT_KATIMER] == -1) ||
#endif
	    maineventids[EVENT_DEVTIMER] == -1) dbcdeath(63);

	breakeventid = maineventids[EVENT_BREAK];
	stopflag = FALSE;
	if (!prpget("keyin", "break", NULL, NULL, &ptr, PRP_LOWER)) {
		if (!strcmp(ptr, "off")) breakeventid = 0;
		else if (!strcmp(ptr, "stop")) stopflag = TRUE;
	}
#if OS_WIN32
	i1 = i2 = 1;
	if (!prpget("gui", "pandlgscale", NULL, NULL, &ptr, 0)) {
		strcpy(work, ptr);
		for (i3 = 0; i3 < (INT) strlen(work) && work[i3] != '/'; i3++);
		if (i3) {
			if (i3 == (INT) strlen(ptr)) i3 = 0;
			else work[i3] = '\0';
			i1 = (INT)strtol(work, NULL, 0);
			if (i3) i2 = atoi(work + i3 + 1);
		}
	}
#endif
	/**
	 * We need to do this here and not below because whether stand-alone or Smart Client, image decompression
	 * is done here in the runtime.
	 */
	if (!prpget("gui", "tiff", "maxstripsize", NULL, &ptr, PRP_LOWER) && *ptr) {
		guisetTiffMaxStripSize(ptr);
	}
	if (!prpget("gui", "color", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "rgb")) guisetRGBformatFor24BitColor();

	/**
	 * We look for this here because it is communicated to a SC even if we are dbcc
	 */
	if (prpget("gui", "ctlfont", NULL, NULL, &ptr, 0) || !*ptr) ptr = NULL;
	if (ptr != NULL) {
		GuiCtlfont = malloc(strlen(ptr) + 1);
		strcpy(GuiCtlfont, ptr);
	}

	if (dbcflags & DBCFLAG_SERVER) {
		if (clientevents(maineventids[EVENT_SHUTDOWN], breakeventid, clientcbfnc)) dbcdeath(64);
	}
#if OS_WIN32
	else if (dbcflags & DBCFLAG_SERVICE) svcstatus(SVC_STARTING, 1000 + 7);
	else if (
				!(dbcflags & DBCFLAG_CONSOLE) /* Must be dbc.exe */
				&& !(dbcflags & DBCFLAG_SERVER) /* and not running under dbcd */
			)
	{
		guisetscalexy(i1, i2);
		if (!prpget("beep", NULL, NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) guisetbeepold();
		if (!prpget("gui", "enterkey", NULL, NULL, &ptr, PRP_LOWER)) {
			guisetenterkey(ptr);
		}
		if (!prpget("gui", "draw", "stretchmode", NULL, &ptr, 0)) {
			guiSetImageDrawStretchMode(ptr);
		}
		if (!prpget("gui", "focus", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) guisetfocusold();
		if (!prpget("gui", "fedit", "selectionbehavior", NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old"))
			guisetfeditboxtextselectiononfocusold();
		if (!prpget("gui", "clipboard", "codepage", NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "850"))
			guisetcbcodepage(ptr);
		if (!prpget("gui", "listbox", "insertbefore", NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) guisetlistinsertbeforeold();

		if (!prpget("gui", "edit", "caret", NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) guisetEditCaretOld();

		if (!prpget("gui", "minsert", "separator", NULL, &ptr, PRP_LOWER)) guisetMinsertSeparator(ptr);
		if (!prpget("gui", "bitblt", "error", NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "ignore")) guisetBitbltErrorIgnore();
		/* last check before guiinit */
		if (!prpget("gui", "editbox", "style", NULL, &ptr, 0) && !strcmp(ptr, "useClientEdge")) {
			guisetUseEditBoxClientEdge(TRUE);
		}
		if (!prpget("gui", "listbox", "style", NULL, &ptr, 0) && !strcmp(ptr, "useClientEdge")) {
			guisetUseListBoxClientEdge(TRUE);
		}
		if (!prpget("gui", "bkimg", NULL, NULL, &ptr, PRP_LOWER)) {
			 if (!strcmp(ptr, "old")) guisetErasebkgndSpecial(EBPS_LATE);
			 else if (!strcmp(ptr, "rdc")) guisetErasebkgndSpecial(EBPS_EARLY);
		}

#if DX_MAJOR_VERSION >= 17
		if (!prpget("gui", IMAGEROTATE, NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old"))
			guisetImageRotateOld();
#else
		if (!prpget("gui", IMAGEROTATE, NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "new"))
			guisetImageRotateNew();
#endif

		if (!guiinit(breakeventid, GuiCtlfont)) {
			BOOL allowTrayIcon = TRUE;	/* By default, we allow it */
			/**
			 * Put an icon in the tray ('notification area')
			 * But not if we are running under dbcd.
			 */
			INT i4 = prpget("gui", "trayicon", NULL, NULL, &ptr, PRP_LOWER);
			if (!i4) {
				if (!strcmp(ptr, "no") || !strcmp(ptr, "false") || !strcmp(ptr, "off")) allowTrayIcon = FALSE;
			}
			if (!(dbcflags & DBCFLAG_SERVER) && allowTrayIcon) {
				guiInitTrayIcon(maineventids[EVENT_TRAPCHAR]);
			}
			dbcflags |= DBCFLAG_GUIINIT;
		}
		if ((dbcflags & DBCFLAG_DEBUG) && !(dbcflags & DBCFLAG_DDTDEBUG)) {
			if (kdsinit()) dbcdeath(64);
		}

#if OS_WIN32GUI
		/*
		 * If we get here we are a Windows gui build and guiinit has been called.
		 * And we are also NOT running as a dbcd server, so we need to do this...
		 */
		/**
		 * Fix mostly for Jon E.
		 * 19 OCT 2012
		 * Since XP, MS has turned off the default visibility of mnemonics (access keys), those
		 * underscores that show for characters preceeded by an ampersand.
		 * They are supposed to show up when the ALT key is pressed. This is not working
		 * for us. Not known why,
		 * So the below call turns on the visibility all the time (just for us, not permanently).
		 *
		 * Moved here from much earlier in the code on 20 SEP 2013.
		 * In the hope that it will help prevent failure on some Server 2003 R2 machines
		 * when run from dbcd as a service.
		 */
		if (!SystemParametersInfo(SPI_SETKEYBOARDCUES, 0, &true, 0)) {
			DWORD e1 = GetLastError();
			svclogerror("Call to SystemParametersInfo failed(SPI_SETKEYBOARDCUES)", e1);
		}
		if (!SystemParametersInfo(SPI_SETKEYBOARDPREF, 0, &true, 0)) {
			DWORD e1 = GetLastError();
			svclogerror("Call to SystemParametersInfo failed(SPI_SETKEYBOARDPREF)", e1);
		}
#endif

	}
#endif
	else if (kdsinit()) dbcdeath(64);

	compat = 0;
	if (!prpget("file", "compat", NULL, NULL, &ptr, PRP_LOWER)) {
		if (!strncmp(ptr, "rms", 3)) compat = 1;
		else if (!strcmp(ptr, "dos")) compat = 2;
		else if (!strcmp(ptr, "dosx")) compat = 3;
		else if (!strcmp(ptr, "ansmas")) compat = 4;
	}

#if OS_WIN32
	if (dbcflags & DBCFLAG_SERVICE) svcstatus(SVC_STARTING, 10000 + 8);
#endif

	srvinit();
	sysinit(argc, argv, envp);

#if OS_WIN32
	if (dbcflags & DBCFLAG_SERVICE) svcstatus(SVC_STARTING, 3000 + 9);
#endif
	for (i1 = 0; ; i1 = PRP_NEXT) {
		if (prpget("preload", "prog", NULL, NULL, &ptr, i1)) break;
		strcpy(name, ptr);
		prpptr = prpgetprppos();
		i2 = newmod(1, name, NULL, NULL, -1);
		if (i2) badanswer(i2);
		/* restore saved lookup position in xml - it was probably wiped out in newmod() */
		prpsetprppos(prpptr);
	}
	if (dbcname[0]) strcpy(name, dbcname);
	else if (!prpget("start", "prog", NULL, NULL, &ptr, 0)) strcpy(name, ptr);
	else strcpy(name, "ANSWER");

	/**
	 * When images are output to a file ('store') we need to allocate from memalloc
	 * a chunk to hold the entire uncompressed file image.
	 * The default is 24MB
	 */
	if (!prpget("imgstorebuf", NULL, NULL, NULL, &ptr, PRP_LOWER))
	{
		imgstorebufsize = atoi(ptr);
		if (imgstorebufsize <= 0) imgstorebufsize = 12 << 20;
		else imgstorebufsize *= 1024;
	}
	else imgstorebufsize = 24 << 20;

	maineventidcount = EVENT_MAXIMUM;
	getaction(&actionvar, &actionbit);
	evtmultiplex(maineventids, maineventidcount, actionvar, actionbit);

	trapcharmap[VID_INTERRUPT >> 3] |= (1 << (VID_INTERRUPT & 0x07));
	if (dbcflags & DBCFLAG_DEBUG) trapcharmap[DEBUGKEY >> 3] |= (1 << (DEBUGKEY & 0x07));
	memcpy(trapnullmap, trapcharmap, sizeof(trapnullmap));
	trapmap = trapcharmap;
	if (!(dbcflags & DBCFLAG_CLIENTINIT)) vidtrapstart(trapmap, maineventids[EVENT_TRAPCHAR]);
/*** CODE: CHOOSE CORRECT ERROR NUMBER ***/
	else if (clienttrapstart(trapmap, maineventids[EVENT_TRAPCHAR]) < 0) dbcdeath(65);

#if OS_UNIX
#if defined(SA_RESTART) | defined(SA_INTERRUPT) | defined(SA_RESETHAND)
	act.sa_handler = sigevent;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
#ifdef SA_INTERRUPT
	act.sa_flags |= SA_INTERRUPT;
#endif
	sigaction(SIGTERM, &act, &oldterm);
	sigaction(SIGQUIT, &act, &oldquit);
	sigaction(SIGHUP, NULL, &oldhup);
	if (oldhup.sa_handler != SIG_IGN) sigaction(SIGHUP, &act, NULL);
#else
	oldterm = sigset(SIGTERM, sigevent);
	oldquit = sigset(SIGQUIT, sigevent);
	oldhup = sigset(SIGHUP, NULL);
	if (oldhup != SIG_IGN) sigset(SIGHUP, sigevent);
#endif
#endif

#if OS_WIN32
	if (dbcflags & DBCFLAG_SERVICE) svcstatus(SVC_STARTING, 1000 + 10);
#endif

	dbcinitflag = TRUE;
#if OS_WIN32
	if (dbcflags & DBCFLAG_SERVICE) svcstatus(SVC_RUNNING, 0);
#endif
#if OS_UNIX
	if (dbcflags & DBCFLAG_CLIENTINIT && keepalivetime > 0)
	{
		msctimestamp(ka_timstamp);
		timadd(ka_timstamp, keepalivetime * 100);
		katimerhandle = timset((UCHAR *)ka_timstamp, maineventids[EVENT_KATIMER]);
		if (katimerhandle == -1) dbcdeath(96);
	}
#endif
	return;
}

#if OS_UNIX
void resetKaTimer()
{
	if (dbcflags & DBCFLAG_CLIENTINIT && keepalivetime > 0)
	{
		timstop(katimerhandle);
		msctimestamp(ka_timstamp);
		timadd(ka_timstamp, keepalivetime * 100);
		katimerhandle = timset((UCHAR *)ka_timstamp, maineventids[EVENT_KATIMER]);
		if (katimerhandle == -1) dbcdeath(96);
	}
}
#endif

INT dbcgetbrkevt()
{
	return(breakeventid);
}

/**
 * Does not return (longjmp)
 */
void dbcerror(INT num)
{
	INT i1;

	for (i1 = LIST_NUM1; i1 <= LIST_NUMMASK; i1++) getlist(LIST_END | i1);
	dbcerrinfo(num, NULL, 0);
}

/**
 * Does not return (longjmp)
 * infolen can be -1 if infoptr is a null terminated string
 */
void dbcerrinfo(INT num, UCHAR *infoptr, INT infolen)
{
	if ( (errornum = num) ) {  /* yes it's assignment and test */
		if (dbcflags & DBCFLAG_CLIENTINIT) clientcancelputs();
		errorinfoptr = infoptr;
		errorinfolen = (infolen != -1) ? infolen : (INT)strlen((CHAR*)infoptr);
	}
	else {
		errorinfoptr = NULL;
		errorinfolen = 0;
	}
	dbcerrjmp();
}

/**
 * End of DB/C execution
 * Does not return (calls exit())
 * -1 is internal error, 0 is normal, 1 is break, 2 is bad answer
 */
void dbcexit(INT n)
{
	INT eventid;
	UCHAR work[(VID_MAXKEYVAL >> 3) + 1];

#if OS_WIN32
	static INT32 vidcmd[] = { VID_KBD_RESET, VID_KBD_ED_OFF, VID_KBD_EOFF, VID_CURSOR_OFF, VID_END_NOFLUSH };
	CHAR *ptr;
#endif

#if OS_WIN32
	if (dbcflags & DBCFLAG_SERVICE) {
		if (!dbcinitflag) svcstatus(SVC_RUNNING, 0);
		svcstatus(SVC_STOPPING, 2000 + 11);
	}
#endif

	dbgexit();
	dioclosemod(-1);
	prtclosemod(-1);
	ctlclosemod(-1);
	comclosemod(-1);
#if OS_WIN32
	if (dbcflags & DBCFLAG_SERVICE) svcstatus(SVC_STOPPING, 2000 + 12);
#endif
	vsql(0);
	sysexit();
	srvexit();
	if (dbcflags & DBCFLAG_SERVER) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			if (n) {
				if (maineventids[EVENT_BREAK] > 0) {
					eventid = maineventids[EVENT_BREAK];
					evtclear(eventid);
				}
				else eventid = evtcreate();
				if (eventid > 0) {
					clientput("<k><timeout n=\"4\"/><kcon/><eoff/><cf w=\"1\"/></k>", -1);
					if (!clientsend(CLIENTSEND_SERIAL | CLIENTSEND_EVENT, eventid)) {
						msctimestamp(work);
						timadd(work, 500);
						timset(work, eventid);
						evtwait(&eventid, 1);
						clientrelease();
					}
				}
			}
			clientend("");
			FreeCertBio();
		}
		if (clientlogfile != NULL) fclose(clientlogfile);
	}

#if OS_WIN32
	else if (dbcflags & DBCFLAG_SERVICE) svcstatus(SVC_STOPPING, 2000 + 13);
	else
#endif
	if (dbcflags & DBCFLAG_VIDINIT) {
#if OS_WIN32
		if (n && (prpget("display", "newcon", NULL, NULL, &ptr, PRP_LOWER) || strcmp(ptr, "off"))) {
			if (maineventids[EVENT_BREAK] > 0) {
				eventid = maineventids[EVENT_BREAK];
				evtclear(eventid);
			}
			else eventid = evtcreate();
			if (eventid > 0) {
				msctimestamp(work);
				timadd(work, 400);
				timset(work, eventid);
				memset(work, 0xFF, sizeof(work));
				vidkeyinfinishmap(work);
				vidput(vidcmd);
				vidtrapstart(NULL, 0);
				vidkeyinstart(work, 1, 1, 1, FALSE, eventid); /* Ignore return, don't care if it fails */
				evtwait(&eventid, 1);
			}
		}
#endif
		kdsexit();
	}

#if OS_UNIX
#if defined(SA_RESTART) | defined(SA_INTERRUPT) | defined(SA_RESETHAND)
	sigaction(SIGTERM, &oldterm, NULL);
	sigaction(SIGQUIT, &oldquit, NULL);
	sigaction(SIGHUP, &oldhup, NULL);
#else
	sigset(SIGTERM, oldterm);
	sigset(SIGQUIT, oldquit);
	sigset(SIGHUP, oldhup);
#endif
#endif

#if OS_WIN32
	if (dbcflags & DBCFLAG_SERVICE) svcstatus(SVC_STOPPED, 0);
	else
#endif
	if (dbcflags & DBCFLAG_GUIINIT) guiexit(n);
	cfgexit();
	exit(n);
}

void dbcdeath(INT n)  /* internal error, end DB/C execution */
{
	INT i1;
	CHAR work[256], work2[256], *ptr1, *ptr2;

	if (n == 92) ptr1 = "Licensing error: ";
	else if (dbcinitflag) ptr1 = "DB/C DX internal error: ";
	else ptr1 = "DB/C DX initialization error: ";
	ptr2 = NULL;
	switch (n) {
	case 10: ptr2 = "null ccall"; break;
	case 20: ptr2 = "null cverb"; break;
	case 51: ptr2 = cfggeterror(); break;
	case 52: ptr2 = "configuration file empty or missing dbcdxcfg element"; break;
	case 53: ptr2 = "unable to get arguments"; break;
	case 61: ptr2 = "insufficient memory"; break;
	case 62: ptr2 = "file access subsystem error"; break;
	case 63:
		strcpy(work2, "event handler subsystem error");
		i1 = (INT)strlen(work2);
		if (evtgeterror(work2 + 2 + i1, (INT) (sizeof(work2) - 2 - i1))) {
			work2[i1] = ':';
			work2[i1 + 1] = ' ';
		}
		ptr2 = work2;
		break;
	case 64:
		if (dbcflags & DBCFLAG_SERVER) {
			clientgeterror(work2, sizeof(work2));
			ptr2 = work2;
		}
		else
		{
			strcpy(work2, "video subsystem error");
			i1 = (INT)strlen(work2);
			if (vidgeterror(work2 + 2 + i1, (INT) (sizeof(work2) - 2 - i1))) {
				work2[i1] = ':';
				work2[i1 + 1] = ' ';
			}
			ptr2 = work2;
		}
		break;
	case 65:
		if (dbcflags & DBCFLAG_SERVER) {
			clientgeterror(work2, sizeof(work2));
			ptr2 = work2;
		}
		break;
	case 81: ptr2 = tcpgeterror(); break;
	case 95: ptr2 = "unable to connect with DDT server"; break;
	case 96: ptr2 = "could not init KA timer"; break;
	}
	if (ptr2 != NULL) strcpy(work, ptr2);
	else mscitoa(n, work);
	if (dbcinitflag) {
		strcat(work, ", pgm address = ");
		mscitoa(pcount, &work[strlen(work)]);
	}

	if (dbcflags & DBCFLAG_SERVER) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			strcpy(work2, ptr1);
			strcat(work2, work);
			clientend(work2);
		}
		if (clientlogfile != NULL) {
			fputs(ptr1, clientlogfile);
			fputs(work, clientlogfile);
			fputs("\n", clientlogfile);
		}
	}
#if OS_WIN32
	else if (dbcflags & DBCFLAG_SERVICE) {
		strcpy(work2, ptr1);
		strcat(work2, work);
		svclogerror(work2, 0);
	}
#endif
	else
		if (!(dbcflags & DBCFLAG_CONSOLE)) {
		guimsgbox(ptr1, work);
	}
	else if ((dbcflags & DBCFLAG_CLIENTINIT) && (dbcflags & DBCFLAG_CLIENTGUI)) {
		clientput("<msgbox title=\"", -1);
		clientput(ptr1, -1);
		clientput("\" text=\"", -1);
		clientput(work, -1);
		clientput("\"/>", -1);
		if (clientsend(0, 0) < 0) dbcerrinfo(798, NULL, 0);
	}
	else if (dbcflags & DBCFLAG_VIDINIT) {
	 	kdscmd(VID_BEEP | 2L);
		kdscmd(VID_WIN_RESET);
		kdscmd(VID_HORZ | 0);
		kdscmd(VID_VERT | 22L);
		kdsdsp(ptr1);
		kdsdsp(work);
	}
	else {
		ctldspstring(ptr1);
		ctldspstring(work);
		ctldspstring("\n");
	}
	dbcexit(255);
}

void badanswer(INT err)
{
	CHAR line1[100], line2[120], work[32];

	if (err == 101) strcpy(line1, "Unable to find program");
	else if (err == 102) strcpy(line1, "Bad compile");
	else if (err == 103) strcpy(line1, "Program is too large");
	else if (err == 155) strcpy(line1, "Global variables do not match");
	else {
		strcpy(line1, "Error (");
		mscitoa(err, &line1[7]);
		if (dsperrflg) {
			if (errno) {
				strcpy(work, ", e1 = ");
				mscitoa(errno, &work[7]);
				strcat(line1, work);
			}
			if (fioaoperr) {
				strcpy(work, ", OP = ");
				mscitoa(fioaoperr, &work[7]);
				strcat(line1, work);
			}
			if (fioarderr) {
				strcpy(work, ", RD = ");
				mscitoa(fioarderr, &work[7]);
				strcat(line1, work);
			}
			if (fioaskerr) {
				strcpy(work, ", SK = ");
				mscitoa(fioaskerr, &work[7]);
				strcat(line1, work);
			}
		}
		strcat(line1, ") while loading ");
	}
	strcpy(line2, "Program = ");
	strncat(line2, name, 100);
#if OS_WIN32
	if (dbcflags & DBCFLAG_SERVICE) {
		char work2[512];
		strcpy(work2, line1);
		strcat(work2, ": ");
		strcat(work2, line2);
		svclogerror(work2, 0);
	}
	else
#endif
	if (dbcflags & DBCFLAG_GUIINIT) {
		guimsgbox(line1, line2);
	}
	else if ((dbcflags & DBCFLAG_CLIENTINIT) && (dbcflags & DBCFLAG_CLIENTGUI)) {
		clientput("<msgbox title=\"", -1);
		clientput(line1, -1);
		clientput("\" text=\"", -1);
		clientput(line2, -1);
		clientput("\"/>", -1);
		if (clientsend(0, 0) < 0) dbcerrinfo(798, NULL, 0);
	}
	else {  /* can assume dbcflag_vidinit */
		kdscmd(VID_HORZ | 0);
		kdscmd(VID_VERT | 21L);
		kdsdsp(line1);
		kdscmd(VID_NL);
		kdsdsp(line2);
	}
	dbcexit(1);
}

void dbcsetevent()
{
	evtset(maineventids[EVENT_SHUTDOWN]);
}

INT dbcwait(INT evtarray[], INT count)  /* wait for trap events plus evtarray events */
/* return is 0 if one of the evtarray eventids is set */
/* return is -1 if a trap event occurred with TRAP PRIOR in effect */
/* return is -2 if a trap event occurred without TRAP PRIOR */
{
	INT i1, i2;

	guiresume();
	if (disableflag) i2 = EVENT_TRAPCHAR + 1;  /* prevent any key events, assume EVENT_TRAPCHAR is last key event */
	else i2 = 0;
	for (i1 = 0; i1 < count; ) maineventids[maineventidcount++] = evtarray[i1++];
	for ( ; ; ) {
		guilock = TRUE;
		i1 = evtwait(&maineventids[i2], maineventidcount - i2);
		guilock = FALSE;
		if (i1 + i2 != EVENT_DEVTIMER) break;
		devtimerevent();	/* process device timer events */
	}
	maineventidcount -= count;
	i1 += i2;
	if (i1 < maineventidcount) {  /* its a trap or break */
		lasttrap = i1;
		if (i1 == EVENT_SHUTDOWN || i1 == EVENT_BREAK) return RC_ERROR;
		if (i1 == EVENT_TRAPCHAR) {
			if (!(dbcflags & DBCFLAG_CLIENTINIT)) {
#if OS_WIN32
				if (dbcflags & DBCFLAG_TRAYSTOP) return RC_ERROR;
#endif
				i2 = vidtrapget(&i1);
			}
			else i2 = clienttrappeek(&i1);
			if (!i2) {
				i2 = trapcharindex[i1];
				if (i2) {
					if (trap[i2].type & TRAPPRIOR) return RC_ERROR;
				}
				else if (i1 == VID_INTERRUPT || i1 == DEBUGKEY) {
					return RC_ERROR;
				}
			}
		}
		else if (i1 == EVENT_TRAPTIMER) {
			if (trap[TRAPTIMER].type & TRAPPRIOR) return RC_ERROR;
		}
		else if (trap[TRAPQUEUE].type & TRAPPRIOR) return RC_ERROR;
#if OS_UNIX
		else if (dbcflags & DBCFLAG_CLIENTINIT && keepalivetime > 0 && i1 == EVENT_KATIMER) dbcsetevent();
#endif
		return RC_INVALID_HANDLE;
	}
	return(0);
}

void vpause()
{
	static UCHAR xFC03[2 + sizeof(INT32)] = { 0xFC, 0x03 };
	INT i1, eventid, timeid, savepcount;
	UCHAR work[17];

	guiresume();
	if (fpicnt) filepi0();  /* cancel any filepi */

	savepcount = pcount - 1;
	i1 = 100;
	memcpy(xFC03 + 2, &i1, sizeof(INT32));
	work[0] = 0xFC;
	work[1] = 0x0B;
	i1 = dbcflags & DBCFLAG_FLAGS;
	/*
	 * Multiply the input value times 100 and put result in work
	 */
	mathop(3, getvar(VAR_READ), xFC03, work);
	dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1;
	i1 = nvtoi(work);
	msctimestamp(work);
	timadd(work, i1);
	if ((eventid = evtcreate()) != -1) {
		timeid = timset(work, eventid);
		if (dbcwait(&eventid, 1) == -1) pcount = savepcount;
		timstop(timeid);
		evtdestroy(eventid);
	}
}

/* set specific trap or traps */
void vtrap()
{
	static UCHAR xFC03[2 + sizeof(INT32)] = { 0xFC, 0x03 };
	INT i1, i2 = 0, i3, start1, end1, start2, end2, type, mod, off, givemod, giveoff;
	UCHAR work[16], *adr1;
	DAVB *queuevar;
	OPENINFO *openinfo;

	givemod = giveoff = 0xFFFF;
	if (getpcnt(gethhll(), &mod, &off) == -1) {
		if (vbcode == 0x49) {
			pcount++;
			skipadr();
			dbcerror(551);
		}
	}
	type = TRAPSET;
	if (vbcode == 0x49) {  /* noreset, prior, nocase, defer */
		type |= (INT)(getbyte()) << 17;
		givemod = datamodule;
		getvarx(pgm, &pcount, &givemod, &giveoff);
	}
	if (oldtrapflg) type |= TRAPPRIOR | TRAPNOCASE;
	i1 = getbyte();
	if (i1 == 0x1F || i1 == 0x80) {
		adr1 = getvar(VAR_READ);
		scanvar(adr1);
		if (vartype & TYPE_NUMERIC) i1 = nvtoi(adr1);
		else if (fp) i1 = adr1[fp + hl - 1];
		else i1 = 0;
		if (i1 <= 0 || i1 > VID_MAXKEYVAL) return;
		type |= i1;
	}
	else if (i1 == 0xFC) type |= getbyte() + 256;
	else if (i1 == 0xFE || i1 == 0xFF) {
		type |= getbyte();
		if (i1 == 0xFF) type |= (getbyte() << 8);
	}
	else type |= trapcode(i1);

	if (!(type & TRAPVALUE_MASK)) {  /* it is not a single key trap */
		if (i1 >= 0x0B && i1 <= 0x10) i2 = i1 - 11;  /* PARITY - SPOOL */
		else if (i1 == 0x85) i2 = TRAPALLCHARS;
		else if (i1 == 0x86) i2 = TRAPALLKEYS;
		else if (i1 == 0x87) {		/* TRAP ANYQUEUE */
			type &= ~TRAPNORESET;
			i2 = TRAPQUEUE;
		}
		else if (i1 == 0x88) i2 = TRAPALLFKEYS;
		else if (i1 == 0x89 || i1 == 0x8A) {  /* TIMEOUT and TIMESTAMP */
			type &= ~TRAPNORESET;
			adr1 = getvar(VAR_READ);
			if (i1 == 0x89) {
				i1 = 100;
				memcpy(xFC03 + 2, &i1, sizeof(INT32));
				work[0] = 0xFC;
				work[1] = 0x0B;
				mathop(3, adr1, xFC03, work);
				i1 = nvtoi(work);
				msctimestamp(timertimestamp);
				timadd(timertimestamp, i1);
			}
			else {
				cvtoname(adr1);
				memcpy(timertimestamp, name, 16);
				i1 = (INT)strlen(name);
				if (i1 < 16) memset(&timertimestamp[i1], '0', (UINT) (16 - i1));
			}
			i2 = TRAPTIMER;
			if (trap[TRAPTIMER].type) timstop(timerhandle);
			timerhandle = timset(timertimestamp, maineventids[EVENT_TRAPTIMER]);
		}
		else if (i1 == 0x8C) {	/* Trap a particular queue */
			type &= ~TRAPNORESET;
			queuevar = getdavb();
			type |= TRAPTYPE_QUEUE | (queuevar->refnum - 1);
		}
		else dbcerrinfo(505, (UCHAR *)"Unknown trap code", 17);
	}
	else {
		type |= TRAPTYPE_CHAR;  /* single key trap */
		i2 = 0; /* Just to keep the compiler static analyzer happy */
	}

	if (type & TRAPTYPE_MASK) {
		/* find an open slot in the trap table for single key/queue trap */
		i1 = type & (TRAPTYPE_MASK | TRAPVALUE_MASK);
		for (i2 = TRAPMAX, i3 = TRAPMISC; i3 <= trapmaxindex; i3++) {
			if (i1 == (trap[i3].type & (TRAPTYPE_MASK | TRAPVALUE_MASK))) {
				i2 = i3;
				break;
			}
			if (!trap[i3].type && i3 < i2) i2 = i3;
		}
		if (i2 == TRAPMAX) {
			if (i3 == TRAPMAX) dbcerror(562);
			i2 = i3;
		}
	}
	else type |= i2;

	trap[i2].type = type;
	trap[i2].module = mod;
	trap[i2].pcount = off;
	trap[i2].givmod = givemod;
	trap[i2].givoff = giveoff;
	if (i2 > trapmaxindex) trapmaxindex = i2;

	if (i2 < TRAPQUEUE) return;

	/* check if trap is for queue(s) */
	if (i2 == TRAPQUEUE || (type & TRAPTYPE_MASK) == TRAPTYPE_QUEUE) {
		if (i2 == TRAPQUEUE) start1 = 0, end1 = openinfohi - 1;
		else start1 = end1 = type & TRAPVALUE_MASK;
		openinfo = *openinfopptr + start1;
		for ( ; start1 <= end1; start1++, openinfo++) {
			if (openinfo->davbtype != DAVB_QUEUE) continue;
			if (openinfo->trapindex >= TRAPMISC && openinfo->trapindex != i2) trap[openinfo->trapindex].type = 0;
			if (!openinfo->trapindex) maineventids[maineventidcount++] = openinfo->v.qvareventid;
			openinfo->trapindex = i2;
		}
		evtmultiplex(maineventids, maineventidcount, actionvar, actionbit);
		return;
	}

	/* trap is for key(s) */
	if (i2 == TRAPALLKEYS) {
		trap[TRAPALLCHARS].type = 0;
		trap[TRAPALLFKEYS].type = 0;
		TRAPALLKEYS_STARTEND
	}
	else if (i2 == TRAPALLCHARS) TRAPALLCHARS_STARTEND
	else if (i2 == TRAPALLFKEYS) TRAPALLFKEYS_STARTEND
	else {
		i1 = type & TRAPVALUE_MASK;
		start1 = end1 = i1;
		if (i1 < 255 && isalpha(i1)) {
			if (isupper(i1)) i1 = tolower(i1);
			else i1 = toupper(i1);
			if (type & TRAPNOCASE) start2 = end2 = i1;
			else {
				if (trapcharindex[i1] == i2) {
					trapcharindex[i1] = 0;
					trapcharmap[i1 >> 3] &= ~(1 << (i1 & 0x07));
				}
/*** CODE: COULD FORCE PREVIOUS OPPOSITE CASE TRAP TO BE CASE SENSITIVE. ***/
/***       THIS FIXES POSSIBLE PROBLEM OF TRAPRESTORING THE CASE SENSITIVE TRAP. ***/
/***       IE. RESTORE CASE SENSITIVE REPLACED BY RESTORE OF CASE INSENSITIVE. ***/
/***				else if (trapcharindex[i1] > TRAPMISC) trap[trapcharindex[i1]].type &= ~TRAPNOCASE; ***/
				start2 = 1, end2 = 0;
			}
		}
		else start2 = 1, end2 = 0;
	}
	for (i1 = start1; i1 <= end1; i1++) {
		if (trapcharindex[i1] >= TRAPMISC && trapcharindex[i1] != i2) {
			if (i2 < TRAPMISC || (type & TRAPPRIOR)) trap[trapcharindex[i1]].type = 0;
		}
		trapcharindex[i1] = i2;
		trapcharmap[i1 >> 3] |= (1 << (i1 & 0x07));
		if (i1 == VID_INTERRUPT || i1 == DEBUGKEY) trapnullmap[i1 >> 3] &= ~(1 << (i1 & 0x07));
		if (fkeyshiftflag && i1 >= VID_F11 && i1 <= VID_F20) {
#if OS_UNIX
			if (dbcflags & DBCFLAG_CLIENTINIT) {
#endif
				trapcharmap[(VID_SHIFTF1 + i1 - VID_F11) >> 3] |= (1 << ((VID_SHIFTF1 + i1 - VID_F11) & 0x07));
#if OS_UNIX
			}
#endif
		}
	}
	for (i1 = start2; i1 <= end2; i1++) {
		if (trapcharindex[i1] >= TRAPMISC && trapcharindex[i1] != i2) trap[trapcharindex[i1]].type = 0;
		trapcharindex[i1] = i2;
		trapcharmap[i1 >> 3] |= (1 << (i1 & 0x07));
		if (fkeyshiftflag && i1 >= VID_F11 && i1 <= VID_F20) {
#if OS_UNIX
			if (dbcflags & DBCFLAG_CLIENTINIT) {
#endif
				trapcharmap[(VID_SHIFTF1 + i1 - VID_F11) >> 3] |= (1 << ((VID_SHIFTF1 + i1 - VID_F11) & 0x07));
#if OS_UNIX
			}
#endif
		}
	}
	if (!(dbcflags & DBCFLAG_CLIENTINIT)) vidtrapstart(trapmap, maineventids[EVENT_TRAPCHAR]);
/*** CODE: CHOOSE CORRECT ERROR NUMBER ***/
	else if (clienttrapstart(trapmap, maineventids[EVENT_TRAPCHAR]) < 0) dbcerror(798);
}

void vtrapclr()
{
	INT i1, i2, start1, end1, start2, end2, type;
	UCHAR *adr1;
	DAVB *queuevar;
	OPENINFO *openinfo;

	i1 = getbyte();
	if (!i1) {
		trapclearall();
		return;
	}
	if (i1 == 0x1F || i1 == 0x80) {
		adr1 = getvar(VAR_READ);
		scanvar(adr1);
		if (vartype & TYPE_NUMERIC) i1 = nvtoi(adr1);
		else if (fp) i1 = adr1[fp + hl - 1];
		else i1 = 0;
		if (i1 <= 0 || i1 > VID_MAXKEYVAL) return;
		type = i1;
	}
	else if (i1 == 0xFC) type = getbyte() + 256;
	else if (i1 == 0xFE || i1 == 0xFF) {
		type = getbyte();
		if (i1 == 0xFF) type |= (getbyte() << 8);
	}
	else type = trapcode(i1);
	if (!(type & TRAPVALUE_MASK)) {  /* it is not a single key trap */
		if (i1 >= 0x0B && i1 <= 0x10) i2 = i1 - 11;  /* PARITY - SPOOL */
		else if (i1 == 0x85) i2 = TRAPALLCHARS;
		else if (i1 == 0x86) i2 = TRAPALLKEYS;
		else if (i1 == 0x87) i2 = TRAPQUEUE;
		else if (i1 == 0x88) i2 = TRAPALLFKEYS;
		else if (i1 == 0x89 || i1 == 0x8A) {  /* TIMEOUT, TIMESTAMP only support one timer */
/*** CODE: WOULD EXPECT A VARIABLE TO FOLLOW, CORDINATE WITH COMPILER ***/
			i2 = TRAPTIMER;
			if (trap[TRAPTIMER].type) {
				timstop(timerhandle);
				timerhandle = -1;
			}
		}
		else if (i1 == 0x8C) {
			queuevar = getdavb();
			openinfo = *openinfopptr + queuevar->refnum - 1;
			if (openinfo->trapindex < TRAPMISC) return;  /* not currently trapped */
			i2 = openinfo->trapindex;
		}
		else dbcerrinfo(505, (UCHAR *)"Unknown trapclr code", 20);
	}
	else {  /* check for existing single key trap */
		if (trapcharindex[type] < TRAPMISC) return;  /* not currently trapped */
		i2 = trapcharindex[type];
	}
	type = trap[i2].type;
	trap[i2].type = 0;
	if (i2 == trapmaxindex) while (--trapmaxindex >= 0) if (trap[trapmaxindex].type) break;

	if (i2 < TRAPQUEUE) return;

	/* check if trap is for queue(s) */
	if (i2 == TRAPQUEUE || (type & TRAPTYPE_MASK) == TRAPTYPE_QUEUE) {
		if (i2 == TRAPQUEUE) start1 = 0, end1 = openinfohi - 1;
		else start1 = end1 = type & TRAPVALUE_MASK;
		openinfo = *openinfopptr + start1;
		for ( ; start1 <= end1; start1++, openinfo++) {
			if (openinfo->davbtype != DAVB_QUEUE) continue;
			if (openinfo->trapindex == i2) {
				openinfo->trapindex = 0;
				for (start2 = EVENT_MAXIMUM; start2 < maineventidcount; start2++) {
					if (maineventids[start2] == openinfo->v.qvareventid) {
						maineventids[start2] = 0;
						break;
					}
				}
			}
		}
		for (start2 = end2 = EVENT_MAXIMUM; start2 < maineventidcount; start2++) {
			if (maineventids[start2]) maineventids[end2++] = maineventids[start2];
		}
		maineventidcount = end2;
		evtmultiplex(maineventids, maineventidcount, actionvar, actionbit);
		return;
	}

	/* trap is for key(s) */
	if (i2 == TRAPALLKEYS) TRAPALLKEYS_STARTEND
	else if (i2 == TRAPALLCHARS) TRAPALLCHARS_STARTEND
	else if (i2 == TRAPALLFKEYS) TRAPALLFKEYS_STARTEND
	else {
		i1 = type & TRAPVALUE_MASK;
		start1 = end1 = i1;
		if ((type & TRAPNOCASE) && i1 < 255 && isalpha(i1)) {
			if (isupper(i1)) i1 = tolower(i1);
			else i1 = toupper(i1);
			start2 = end2 = i1;
		}
		else start2 = 1, end2 = 0;
	}
	for (i1 = start1; i1 <= end1; i1++) {
		if (trapcharindex[i1] == i2 || (trapcharindex[i1] && i2 < TRAPMISC)) {
			trapcharindex[i1] = 0;
			if (i1 == VID_INTERRUPT || (i1 == DEBUGKEY && (dbcflags & DBCFLAG_DEBUG))) trapnullmap[i1 >> 3] |= (1 << (i1 & 0x07));
			else trapcharmap[i1 >> 3] &= ~(1 << (i1 & 0x07));
			if (fkeyshiftflag && i1 >= VID_F11 && i1 <= VID_F20) {
#if OS_UNIX
				if (dbcflags & DBCFLAG_CLIENTINIT) {
#endif
					trapcharmap[(VID_SHIFTF1 + i1 - VID_F11) >> 3] &= ~(1 << ((VID_SHIFTF1 + i1 - VID_F11) & 0x07));
#if OS_UNIX
				}
#endif
			}
		}
	}
	for (i1 = start2; i1 <= end2; i1++) {
		if (trapcharindex[i1] == i2 || (trapcharindex[i1] && i2 < TRAPMISC)) {
			trapcharindex[i1] = 0;
			trapcharmap[i1 >> 3] &= ~(1 << (i1 & 0x07));
			if (fkeyshiftflag && i1 >= VID_F11 && i1 <= VID_F20) {
#if OS_UNIX
				if (dbcflags & DBCFLAG_CLIENTINIT) {
#endif
					trapcharmap[(VID_SHIFTF1 + i1 - VID_F11) >> 3] &= ~(1 << ((VID_SHIFTF1 + i1 - VID_F11) & 0x07));
#if OS_UNIX
				}
#endif
			}
		}
	}
	if (!(dbcflags & DBCFLAG_CLIENTINIT)) vidtrapstart(trapmap, maineventids[EVENT_TRAPCHAR]);
/*** CODE: CHOOSE CORRECT ERROR NUMBER ***/
	else if (clienttrapstart(trapmap, maineventids[EVENT_TRAPCHAR]) < 0) dbcerror(798);
}

static INT trapcode(INT code)  /* translate .dbc trap code to vid key code */
{
	INT n;

	if (code <= 0x0A) n = VID_F1 + code - 1;  /* F1 - F10 */
	else if (code == 0x11) n = VID_INTERRUPT;
	else if (code >= 0x12 && code <= 0x1B) n = VID_UP + code - 0x12;
	else if (code == 0x1D) n = VID_ESCAPE;
	else if (code == 0x1E) n = VID_ENTER;
	else if (code >= 0x20 && code <= 0x7F) n = code;
	else if (code == 0x81) n = VID_TAB;
	else if (code == 0x82) n = VID_BKTAB;
	else if (code == 0x83) n = VID_BKSPC;
	else if (code == 0x84) n = VID_CANCEL;
	else if (code >= 0xD3 && code <= 0xDC) n = VID_F11 + code - 0xD3;
	else n = 0;
	return(n);
}

INT vtrapsize()  /* trapsize verb */
{
	INT i1, i2;

	for (i1 = 0, i2 = TRAPHDR; i1 <= trapmaxindex; i1++)
		if (trap[i1].type) {
			i2 += TRAPSIZE;
			if (i1 == TRAPTIMER) i2 += 16;
			else if ((trap[i1].type & TRAPTYPE_MASK) == TRAPTYPE_QUEUE) i2 += 2;
		}
	return(i2);
}

void vtrapsave()  /* trapsave verb */
{
	UCHAR *adr1, *adr2;
	INT i1, i2, i3;

	adr1 = adr2 = getvar(VAR_WRITE);
	adr1 += hl;
	dbcflags &= ~DBCFLAG_EOS;
	if (pl >= TRAPHDR) {
		fp = 1;
		msciton(pgmchn, adr1, TRAPHDR);
		adr1 += TRAPHDR;
		lp = TRAPHDR;
		for (i1 = 0; i1 <= trapmaxindex; i1++) {
			if (!trap[i1].type) continue;
			if ((lp += TRAPSIZE) > pl) {
				dbcflags |= DBCFLAG_EOS;
				break;
			}
			i2 = trap[i1].type;
			i3 = 0;
			adr1[i3++] = (UCHAR) i2;
			adr1[i3++] = (UCHAR)(i2 >> 8);
			adr1[i3++] = (UCHAR)(trap[i1].type >> 16);
//			i2 = trap[i1].module;
			memcpy(&adr1[i3], &trap[i1].module, sizeof(INT));
			i3 += sizeof(INT);
//			adr1[i3++] = (UCHAR)i2;
//			adr1[i3++] = (UCHAR)(i2 >> 8);
			i2 = trap[i1].pcount;
			adr1[i3++] = (UCHAR) i2;
			adr1[i3++] = (UCHAR)(i2 >> 8);
			adr1[i3++] = (UCHAR)(i2 >> 16);
//			i2 = trap[i1].givmod;
			memcpy(&adr1[i3], &trap[i1].givmod, sizeof(INT));
			i3 += sizeof(INT);
//			adr1[i3++] = (UCHAR) i2;
//			adr1[i3++] = (UCHAR)(i2 >> 8);
			i2 = trap[i1].givoff;
			adr1[i3++] = (UCHAR) i2;
			adr1[i3++] = (UCHAR)(i2 >> 8);
			adr1[i3++] = (UCHAR)(i2 >> 16);
			adr1 += TRAPSIZE;
			if (i1 == TRAPTIMER) {
				if ((lp += 16) > pl) {
					dbcflags |= DBCFLAG_EOS;
					break;
				}
				memcpy(adr1, timertimestamp, 16);
				adr1 += 16;
			}
			else if ((trap[i1].type & TRAPTYPE_MASK) == TRAPTYPE_QUEUE) {
				if ((lp += 2) > pl) {
					dbcflags |= DBCFLAG_EOS;
					break;
				}
				i2 = (*openinfopptr + (trap[i1].type & TRAPVALUE_MASK))->datamod;
				adr1[0] = (UCHAR) i2;
				adr1[1] = (UCHAR)(i2 >> 8);
				adr1 += 2;
			}
		}
	}
	else dbcflags |= DBCFLAG_EOS;
	if (dbcflags & DBCFLAG_EOS) fp = lp = 0;
	setfplp(adr2);
}

void vtraprestore()  /* traprestore verb */
{
	INT i1, i2, i3, i4, mod, start1, end1, start2, end2;
	INT resetevtflg, resettrpflg, type;
	UCHAR *adr1, work[TRAPHDR];
	OPENINFO *openinfo;

	adr1 = getvar(VAR_READ);
	adr1 += hl;
	msciton(pgmchn, work, TRAPHDR);
	if (!fp || lp < TRAPHDR || memcmp(work, adr1, TRAPHDR)) dbcerror(506);
	trapclearall();
	resetevtflg = resettrpflg = FALSE;
	adr1 += TRAPHDR;
	lp -= TRAPHDR;
	for (i3 = TRAPMISC; lp >= TRAPSIZE; ) {
		type = llmmhh(adr1);
		if (type & TRAPTYPE_MASK) {
			if (i3 == TRAPMAX) break;
			i2 = i3++;
		}
		else i2 = type & TRAPVALUE_MASK;
		trap[i2].type = type;
//		trap[i2].module = llhh(&adr1[3]);
		i4 = 3;
		memcpy(&(trap[i2].module), &adr1[i4], sizeof(INT));
		i4 += sizeof(INT);
		trap[i2].pcount = llmmhh(&adr1[i4]);
		i4 += 3;
//		trap[i2].givmod = llhh(&adr1[8]);
		memcpy(&(trap[i2].givmod), &adr1[i4], sizeof(INT));
		i4 += sizeof(INT);
		trap[i2].givoff = llmmhh(&adr1[i4]);
		if (i2 > trapmaxindex) trapmaxindex = i2;
		adr1 += TRAPSIZE;
		lp -= TRAPSIZE;

		if (i2 < TRAPTIMER) continue;

		/* restore timer */
		if (i2 == TRAPTIMER) {
			if (lp < 16) {
				lp = 1;  /* force trap clear before error */
				break;
			}
			memcpy(timertimestamp, adr1, 16);
			adr1 += 16;
			lp -= 16;
			timerhandle = timset(timertimestamp, maineventids[EVENT_TRAPTIMER]);
			continue;
		}

		/* restore queue(s) */
		if (i2 == TRAPQUEUE || (type & TRAPTYPE_MASK) == TRAPTYPE_QUEUE) {
			if (i2 == TRAPQUEUE) {
				mod = -1;
				start1 = 0;
				end1 = openinfohi - 1;
			}
			else {
				if (lp < 2) {
					lp = 1;  /* force trap clear before error */
					break;
				}
				mod = llhh(adr1);
				adr1 += 2;
				lp -= 2;
				start1 = end1 = type & TRAPVALUE_MASK;
			}
			openinfo = *openinfopptr + start1;
			for ( ; start1 <= end1; start1++, openinfo++) {
				if (openinfo->davbtype != DAVB_QUEUE || (mod != -1 && openinfo->datamod != mod)) {
					if (mod != -1) {
						trap[i2].type = 0;
						i3--;
					}
					continue;
				}
				if (!openinfo->trapindex) maineventids[maineventidcount++] = openinfo->v.qvareventid;
				openinfo->trapindex = i2;
			}
			resetevtflg = TRUE;
			continue;
		}

		/* restore key(s) */
		if (i2 == TRAPALLKEYS) TRAPALLKEYS_STARTEND
		else if (i2 == TRAPALLCHARS) TRAPALLCHARS_STARTEND
		else if (i2 == TRAPALLFKEYS) TRAPALLFKEYS_STARTEND
		else {
			i1 = type & TRAPVALUE_MASK;
			start1 = end1 = i1;
			if (i1 < 255 && isalpha(i1)) {
				if (isupper(i1)) i1 = tolower(i1);
				else i1 = toupper(i1);
				if (type & TRAPNOCASE) start2 = end2 = i1;
				else {
					if (trapcharindex[i1] == i2) {
						trapcharindex[i1] = 0;
						trapcharmap[i1 >> 3] &= ~(1 << (i1 & 0x07));
					}
					start2 = 1, end2 = 0;
				}
			}
			else start2 = 1, end2 = 0;
		}
		for (i1 = start1; i1 <= end1; i1++) {
			if (trapcharindex[i1] >= TRAPMISC && trapcharindex[i1] != i2) {
				if (type & TRAPPRIOR) trap[trapcharindex[i1]].type = 0;
			}
			trapcharindex[i1] = i2;
			trapcharmap[i1 >> 3] |= (1 << (i1 & 0x07));
			if (i1 == VID_INTERRUPT || i1 == DEBUGKEY) trapnullmap[i1 >> 3] &= ~(1 << (i1 & 0x07));
			if (fkeyshiftflag && i1 >= VID_F11 && i1 <= VID_F20) {
#if OS_UNIX
				if (dbcflags & DBCFLAG_CLIENTINIT) {
#endif
					trapcharmap[(VID_SHIFTF1 + i1 - VID_F11) >> 3] |= (1 << ((VID_SHIFTF1 + i1 - VID_F11) & 0x07));
#if OS_UNIX
				}
#endif
			}
		}
		for (i1 = start2; i1 <= end2; i1++) {
			if (trapcharindex[i1] >= TRAPMISC && trapcharindex[i1] != i2) trap[trapcharindex[i1]].type = 0;
			trapcharindex[i1] = i2;
			trapcharmap[i1 >> 3] |= (1 << (i1 & 0x07));
			if (fkeyshiftflag && i1 >= VID_F11 && i1 <= VID_F20) {
#if OS_UNIX
				if (dbcflags & DBCFLAG_CLIENTINIT) {
#endif
					trapcharmap[(VID_SHIFTF1 + i1 - VID_F11) >> 3] |= (1 << ((VID_SHIFTF1 + i1 - VID_F11) & 0x07));
#if OS_UNIX
				}
#endif
			}
		}
		resettrpflg = TRUE;
	}
	if (lp) {
		trapclearall();
		dbcerror(506);
	}
	else {
		if (resetevtflg) evtmultiplex(maineventids, maineventidcount, actionvar, actionbit);
		if (resettrpflg) {
			if (!(dbcflags & DBCFLAG_CLIENTINIT)) vidtrapstart(trapmap, maineventids[EVENT_TRAPCHAR]);
/*** CODE: CHOOSE CORRECT ERROR NUMBER ***/
			else if (clienttrapstart(trapmap, maineventids[EVENT_TRAPCHAR]) < 0) dbcerror(798);
		}
	}
}

void trapclearall()
{
	INT i1;
	OPENINFO *openinfo;

	/* clear timer */
	if (trap[TRAPTIMER].type) {
		timstop(timerhandle);
		timerhandle = -1;
	}

	/* clear queues */
	openinfo = *openinfopptr;
	for (i1 = 0; i1 < openinfohi; i1++, openinfo++) {
		if (openinfo->davbtype != DAVB_QUEUE) continue;
		openinfo->trapindex = 0;
	}
	maineventidcount = EVENT_MAXIMUM;
	evtmultiplex(maineventids, maineventidcount, actionvar, actionbit);

	/* clear keys */
	memset(trapcharindex, 0, sizeof(trapcharindex));
	trapnullmap[VID_INTERRUPT >> 3] |= (1 << (VID_INTERRUPT & 0x07));
	if (dbcflags & DBCFLAG_DEBUG) trapnullmap[DEBUGKEY >> 3] |= (1 << (DEBUGKEY & 0x07));
	memcpy(trapcharmap, trapnullmap, sizeof(trapcharmap));
	if (!(dbcflags & DBCFLAG_CLIENTINIT)) vidtrapstart(trapmap, maineventids[EVENT_TRAPCHAR]);
/*** CODE: CHOOSE CORRECT ERROR NUMBER ***/
	else if (clienttrapstart(trapmap, maineventids[EVENT_TRAPCHAR]) < 0) dbcerror(798);

	while (trapmaxindex >= 0) trap[trapmaxindex--].type = 0;
}

void trapclearmod(INT module)
{
	INT i1, i2, start1, end1, start2, end2, resetevtflg, resettrpflg;
	OPENINFO *openinfo;

	resetevtflg = resettrpflg = FALSE;
	for (i2 = 0; i2 <= trapmaxindex; i2++) {
		if (!trap[i2].type || trap[i2].module != module) continue;
		if (i2 == TRAPTIMER) {
			timstop(timerhandle);
			timerhandle = -1;
		}
		else if (i2 == TRAPQUEUE || (trap[i2].type & TRAPTYPE_MASK) == TRAPTYPE_QUEUE) {
			if (i2 == TRAPQUEUE) start1 = 0, end1 = openinfohi - 1;
			else start1 = end1 = trap[i2].type & TRAPVALUE_MASK;
			openinfo = *openinfopptr + start1;
			for ( ; start1 <= end1; start1++, openinfo++) {
				if (openinfo->davbtype != DAVB_QUEUE) continue;
				if (openinfo->trapindex == i2) {
					openinfo->trapindex = 0;
					for (start2 = EVENT_MAXIMUM; start2 < maineventidcount; start2++) {
						if (maineventids[start2] == openinfo->v.qvareventid) {
							maineventids[start2] = 0;
							break;
						}
					}
				}
			}
			resetevtflg = TRUE;
		}
		else if (i2 >= TRAPALLKEYS) {
			if (i2 == TRAPALLKEYS) TRAPALLKEYS_STARTEND
			else if (i2 == TRAPALLCHARS) TRAPALLCHARS_STARTEND
			else if (i2 == TRAPALLFKEYS) TRAPALLFKEYS_STARTEND
			else {
				start1 = end1 = trap[i2].type & TRAPVALUE_MASK;
				if ((trap[i2].type & TRAPNOCASE) && start1 < 255 && isalpha(start1)) {
					if (isupper(start1)) start2 = tolower(start1);
					else start2 = toupper(start1);
					end2 = start2;
				}
				else start2 = 1, end2 = 0;
			}
			for (i1 = start1; i1 <= end1; i1++) {
				if (trapcharindex[i1] == i2) {
					trapcharindex[i1] = 0;
					if (i1 == VID_INTERRUPT || (i1 == DEBUGKEY && (dbcflags & DBCFLAG_DEBUG))) trapnullmap[i1 >> 3] |= (1 << (i1 & 0x07));
					else trapcharmap[i1 >> 3] &= ~(1 << (i1 & 0x07));
					if (fkeyshiftflag && i1 >= VID_F11 && i1 <= VID_F20) {
#if OS_UNIX
						if (dbcflags & DBCFLAG_CLIENTINIT) {
#endif
								trapcharmap[(VID_SHIFTF1 + i1 - VID_F11) >> 3] &= ~(1 << ((VID_SHIFTF1 + i1 - VID_F11) & 0x07));
#if OS_UNIX
						}
#endif
					}
				}
			}
			for (i1 = start2; i1 <= end2; i1++) {
				if (trapcharindex[i1] == i2) {
					trapcharindex[i1] = 0;
					trapcharmap[i1 >> 3] &= ~(1 << (i1 & 0x07));

					if (fkeyshiftflag && i1 >= VID_F11 && i1 <= VID_F20) {
#if OS_UNIX
						if (dbcflags & DBCFLAG_CLIENTINIT) {
#endif
							trapcharmap[(VID_SHIFTF1 + i1 - VID_F11) >> 3] &= ~(1 << ((VID_SHIFTF1 + i1 - VID_F11) & 0x07));
#if OS_UNIX
						}
#endif
					}
				}
			}
			resettrpflg = TRUE;
		}
		trap[i2].type = 0;
	}
	if (resetevtflg) {
		for (start2 = end2 = EVENT_MAXIMUM; start2 < maineventidcount; start2++) {
			if (maineventids[start2]) maineventids[end2++] = maineventids[start2];
		}
		maineventidcount = end2;
		evtmultiplex(maineventids, maineventidcount, actionvar, actionbit);
	}
	if (resettrpflg) {
		if (!(dbcflags & DBCFLAG_CLIENTINIT)) vidtrapstart(trapmap, maineventids[EVENT_TRAPCHAR]);
/*** CODE: CHOOSE CORRECT ERROR NUMBER ***/
		else if (clienttrapstart(trapmap, maineventids[EVENT_TRAPCHAR]) < 0) dbcerror(798);
	}
	while (trapmaxindex >= 0 && !trap[trapmaxindex].type) trapmaxindex--;
}

void disable()
{
	guisuspend();
	trapdisable();
}

static void trapdisable()
{
	if (!disableflag) {
		trapmap = trapnullmap;
		if (!(dbcflags & DBCFLAG_CLIENTINIT)) vidtrapstart(trapmap, maineventids[EVENT_TRAPCHAR]);
/*** CODE: CHOOSE CORRECT ERROR NUMBER ***/
		else if (clienttrapstart(trapmap, maineventids[EVENT_TRAPCHAR]) < 0) dbcerror(798);
		disableflag = TRUE;
	}
}

void enable()
{
	guiresume();
	trapenable();
}

static void trapenable()
{
	if (disableflag) {
		disableflag = FALSE;
		trapmap = trapcharmap;
		if (!(dbcflags & DBCFLAG_CLIENTINIT)) vidtrapstart(trapmap, maineventids[EVENT_TRAPCHAR]);
/*** CODE: CHOOSE CORRECT ERROR NUMBER ***/
		else if (clienttrapstart(trapmap, maineventids[EVENT_TRAPCHAR]) < 0) dbcerror(798);
	}
}

INT getdisable()
{
	return(disableflag);
}

INT geterrmsg(UCHAR *errmsg, INT *length, INT resetflg)
{
	INT i1, i2;

	i1 = (INT)strlen(errormsg);
	if (*length < i1) {
		i1 = *length;
		i2 = 1;
	}
	else {
		*length = i1;
		i2 = 0;
	}
	memcpy(errmsg, (UCHAR *) errormsg, (UINT) i1);
	if (resetflg) errormsg[0] = '\0';
	return(i2);
}

/**
 * error execution
 */
void errorexec()
{
	static INT destroyflg = 0;
	INT i1, err;
	INT32 x1;
	CHAR *ptr, work2[32], work3[32];
	UCHAR c1, work[512], *adr;

	if (!errornum) {
		destroyflg = 0;
		return;
	}
	memset(work, '\0', 512);
	work[0] = work[1] = ' ';
	err = (errornum % 1000) / 100;
	if (err == 1) {  /* CFAIL */
		err = TRAPCFAIL;
		c1 = 'C';
	}
	else if (err == 2) {  /* FORMAT */
		err = TRAPFORMAT;
		c1 = 'F';
	}
	else if (err == 3) {  /* RANGE */
		err = TRAPRANGE;
		c1 = 'R';
	}
	else if (err == 4) {  /* SPOOL */
		err = TRAPSPOOL;
		c1 = 'P';
	}
	else if (err == 6 || err == 7) {  /* IO */
		err = TRAPIO;
		c1 = 'I';
	}
	else {  /* ERROR */
		err = TRAPMAX;
		c1 = 'E';
	}

	work[2] = c1;
	msciton(errornum, &work[3], 4);
	work[7] = ' ';
	mscitoa(pcount, (CHAR *) &work[8]);
	ptr = getmname(pgmmodule, datamodule, FALSE);
	if (ptr != NULL) {
		strcat((CHAR *) &work[8], " ");
		strcat((CHAR *) &work[9], ptr);
	}
	if (errorinfolen) {
		strcat((CHAR *) &work[8], ", ");
		strncat((CHAR *) &work[10], (CHAR *) errorinfoptr, errorinfolen);
	}
	if (dsperrflg) {
		if (errno) {
			strcpy(work2, ", e1 = ");
			mscitoa(errno, &work2[7]);
			strcat((CHAR *) &work[8], work2);
			errno = 0;
		}
		if (fioaoperr) {
			strcpy(work2, ", OP = ");
			mscitoa(fioaoperr, &work2[7]);
			strcat((CHAR *) &work[8], work2);
			fioaoperr = 0;
		}
		if (fioaclerr) {
			strcpy(work2, ", CL = ");
			mscitoa(fioaclerr, &work2[7]);
			strcat((CHAR *) &work[8], work2);
			fioaclerr = 0;
		}
		if (fioarderr) {
			strcpy(work2, ", RD = ");
			mscitoa(fioarderr, &work2[7]);
			strcat((CHAR *) &work[8], work2);
			fioarderr = 0;
		}
		if (fioawrerr) {
			strcpy(work2, ", WR = ");
			mscitoa(fioawrerr, &work2[7]);
			strcat((CHAR *) &work[8], work2);
			fioawrerr = 0;
		}
		if (fioalkerr) {
			strcpy(work2, ", LK = ");
			mscitoa(fioalkerr, &work2[7]);
			strcat((CHAR *) &work[8], work2);
			fioalkerr = 0;
		}
		if (fioaskerr) {
			strcpy(work2, ", SK = ");
			mscitoa(fioaskerr, &work2[7]);
			strcat((CHAR *) &work[8], work2);
			fioaskerr = 0;
		}

		if (err == TRAPMAX && (errornum == 505 || errornum == 552)) {
			sprintf(work3, ", lsvb=%#.2x, vb=%#.2x, vx=%#.2x", (UINT)lsvbcode, (UINT)vbcode, (UINT)vx);
			strcat((CHAR *) &work[8], work3);
		}
#if 0
/*** ADD CODE TO MAKE AVAILABLE NIOERROR ***/
		if (nioerror) {
			strcpy(work2, ", NIO = ");
			mscitoa(nioerror, &work2[7]);
			strcat((CHAR *) &work[8], work2);
			nioerror = 0;
		}
#endif
	}

	work[0] = 0xE1;
	work[1] = (UCHAR) strlen((CHAR *) &work[2]);
	if (err != TRAPMAX && trap[err].type && data != NULL && pgm != NULL) {  /* trapped error */
		if (trap[err].type & TRAPDEFER) trapdisable();
		if (trap[err].givmod != -1) {
			if ((adr = findvar(trap[err].givmod, trap[err].givoff)) != NULL) {
				scanvar(adr);
				if (vartype & TYPE_NUMERIC) {
					work[0] = 0xFC;
					work[1] = 0x04;
					x1 = errornum;
					memcpy(&work[2], (UCHAR *) &x1, sizeof(INT32));
				}
				i1 = dbcflags & DBCFLAG_FLAGS;
				movevar(work, adr);
				dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1;
			}
		}
		if (!(trap[err].type & TRAPNORESET)) trap[err].type = 0;
		if (pushreturnstack(-1)) dbcerror(502);
		setpcnt(trap[err].module, trap[err].pcount);
	}
	else {  /* untrapped error */
		/**
		 * dbcdx.file.untrappederrors=<somefilepath>
		 */
		if (writeUntrappedErrorsFlag) {
			logUntrappedError((CHAR *) &work[2], errornum);
		}

		if (!destroyflg) {
			strcpy(errormsg, (CHAR *) &work[2]);
			if (dbcflags & DBCFLAG_DEBUG) vdebug(DEBUG_RESTORE);
			if (prpget("errordisplay", NULL, NULL, NULL, &ptr, PRP_LOWER) || strcmp(ptr, "off")) {
#if OS_WIN32
				if (dbcflags & DBCFLAG_SERVICE) svclogerror(errormsg, 0);
				else
#endif
				if (dbcflags & DBCFLAG_GUIINIT) {
					guimsgbox("Untrapped error", errormsg);
				}
				else if ((dbcflags & DBCFLAG_CLIENTINIT) && (dbcflags & DBCFLAG_CLIENTGUI)) {
					clientput("<msgbox title=\"Untrapped error\" text=\"", -1);
					clientputdata(errormsg, -1);
					clientput("\"/>", -1);
					clientsend(0, 0);
				}
				else {
					kdscmd(VID_WIN_RESET);
					kdscmd(VID_HORZ | 0);
					kdscmd(VID_VERT | 22);
					kdsdsp(errormsg);
					kdscmd(VID_NL);
				}
			}
			if (dbcflags & DBCFLAG_DEBUG) vdebug(DEBUG_ERROR);
			ptr = (CHAR *) work;
		}
		else ptr = NULL;
		destroyflg = vstop((UCHAR *) ptr, errornum);
	}
}

void actionexec()
{
	INT i1, i2;

	if (evttest(maineventids[EVENT_SHUTDOWN])) dbcexit(0);

	if (lasttrap == -1) i1 = 0;
	else i1 = lasttrap;
	do {
		i2 = 1;
		if (evttest(maineventids[i1])) {
			if (i1 == EVENT_BREAK) {
				if (!disableflag) {
					if (!stopflag) dbcexit(0);
					evtclear(maineventids[EVENT_BREAK]);
					vstop(NULL, 0);
					return;
				}
			}
			else if (i1 == EVENT_DEVTIMER) {
				devtimerevent();
				i2 = 0;
			}
			else if (!disableflag || i1 != EVENT_TRAPCHAR) {
				if ((i2 = actiontrap(i1)) == -1) return;
			}
		}
		if (lasttrap != -1) {
			lasttrap = -1;
			i1 = 0;
		}
		else i1 += i2;
	} while (i1 < maineventidcount);
}

static INT actiontrap(INT event)
{
	static CHAR *mfnckeys[] = {"ENTER", "ESC", "BKSPC", "TAB", "BKTAB"};
	static CHAR *xfnckeys[] = {"UP", "DOWN", "LEFT", "RIGHT", "INSERT",
		"DELETE", "HOME", "END", "PGUP", "PGDN"};
	INT i1, i2, eventid, start1, end1, start2, end2, trapkeyvalue, type;
	INT32 x1;
	UCHAR *adr, work[32];
	OPENINFO *openinfo;

	if (event == EVENT_TRAPCHAR || event == EVENT_TRAPTIMER) evtclear(maineventids[event]);

	trapkeyvalue = 0;
	if (event == EVENT_TRAPCHAR) {
		if (!(dbcflags & DBCFLAG_CLIENTINIT)) {
#if OS_WIN32
			if (dbcflags & DBCFLAG_TRAYSTOP) {
				dbcflags &= ~DBCFLAG_TRAYSTOP;
				vstop(NULL, 0);
				return RC_ERROR;
			}
#endif
			i2 = vidtrapend(&trapkeyvalue);
		}
		else i2 = clienttrapget(&trapkeyvalue);
		if (i2 || !trapkeyvalue) {
			if (!(dbcflags & DBCFLAG_CLIENTINIT)) vidtrapstart(trapmap, maineventids[EVENT_TRAPCHAR]);
/*** CODE: CHOOSE CORRECT ERROR NUMBER ***/
			else if (clienttrapstart(trapmap, maineventids[EVENT_TRAPCHAR]) < 0) dbcerror(798);
			if (i2) i2 = 1;
			return(i2);
		}
		if (fkeyshiftflag && trapkeyvalue >= VID_SHIFTF1 && trapkeyvalue <= VID_SHIFTF10) {
#if OS_UNIX
			if (dbcflags & DBCFLAG_CLIENTINIT) {
#endif
				trapkeyvalue += VID_F11 - VID_SHIFTF1;
#if OS_UNIX
			}
#endif
		}
		i2 = trapkeyvalue;
		if (i2 == VID_INTERRUPT && !trapcharindex[VID_INTERRUPT]) {
			vstop(NULL, 0);
			return RC_ERROR;
		}
		if (i2 == DEBUGKEY && (dbcflags & DBCFLAG_DEBUG) && !trapcharindex[DEBUGKEY]) {
			vdebug(DEBUG_INTERRUPT);
			if (!(dbcflags & DBCFLAG_CLIENTINIT)) vidtrapstart(trapmap, maineventids[EVENT_TRAPCHAR]);
/*** CODE: CHOOSE CORRECT ERROR NUMBER ***/
			else if (clienttrapstart(trapmap, maineventids[EVENT_TRAPCHAR]) < 0) dbcerror(798);
			return RC_ERROR;
		}
		if (i2 < 32) {
			work[2] = '^';
			work[3] = (CHAR)(i2 - 1 + 'A');
			work[4] = 0;
		}
		else if (i2 < 256) {
			work[2] = (CHAR) i2;
			work[3] = 0;
		}
		else {
			if (i2 >= VID_ENTER && i2 <= VID_BKTAB) strcpy((CHAR *) &work[2], mfnckeys[i2 - VID_ENTER]);
			else if (i2 >= VID_UP && i2 <= VID_PGDN) strcpy((CHAR *) &work[2], xfnckeys[i2 - VID_UP]);
			else if (i2 >= VID_F1 && i2 <= VID_F20) {
				work[2] = 'F';
				mscitoa(i2 - VID_F1 + 1, (CHAR *) &work[3]);
			}
			else if (i2 == VID_INTERRUPT) strcpy((CHAR *) &work[2], "INT");
			else if (i2 == VID_CANCEL) strcpy((CHAR *) &work[2], "CANCEL");
			else work[2] = 0;
		}
		i2 = trapcharindex[i2];
	}
	else if (event == EVENT_TRAPTIMER) {
		strcpy((CHAR *) &work[2], "TIME");
		i2 = TRAPTIMER;
	}
	else {  /* queue */
		eventid = maineventids[event];
		openinfo = *openinfopptr;
		for (i1 = 0; i1 < openinfohi && (openinfo->davbtype != DAVB_QUEUE || openinfo->v.qvareventid != eventid); i1++, openinfo++);
		if (i1 == openinfohi) {
			evtclear(maineventids[event]);
/*** CODE: MAYBE GIVE DEATH ERROR ***/
			return(1);
		}

		strcpy((CHAR *) &work[2], "QUEUE");
		i2 = openinfo->trapindex;
	}

/*** CODE: TRY TO CREATE SITUATION THAT THIS CAN HAPPEN ***/
/*** CODE: MAY BE POSSIBLE IF STARTED WITH -Z AND NOT ENOUGH MEMORY FOR ***/
/***       DEBUGGER. IF DBGFLAG GETS SET TO 0, THEN SHOULD CLEAR THIS TRAP ***/
/*** CODE: MAYBE GIVE DEATH ERROR ***/
	if (!i2 || !(type = trap[i2].type)) return(1);

	if (type & TRAPDEFER) trapdisable();
	if (trap[i2].givmod != -1) {
		if ((adr = findvar(trap[i2].givmod, trap[i2].givoff)) != NULL) {
			scanvar(adr);
			if (vartype & TYPE_NUMERIC) {
				work[0] = 0xFC;
				work[1] = 0x03;
				x1 = trapkeyvalue;
				memcpy(&work[2], (UCHAR *) &x1, sizeof(INT32));
			}
			else {
				work[0] = 0xE1;
				work[1] = (UCHAR) strlen((CHAR *) &work[2]);
			}
			i1 = dbcflags & DBCFLAG_FLAGS;
			movevar(work, adr);
			dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1;
		}
	}
	if (!(type & TRAPNORESET)) trap[i2].type = 0;

	if (i2 == TRAPTIMER) {
		timstop(timerhandle);
		timerhandle = -1;
	}
	else if (i2 == TRAPQUEUE || (type & TRAPTYPE_MASK) == TRAPTYPE_QUEUE) {
		if (i2 == TRAPQUEUE) start1 = 0, end1 = openinfohi - 1;
		else start1 = end1 = type & TRAPVALUE_MASK;
		openinfo = *openinfopptr + start1;
		for ( ; start1 <= end1; start1++, openinfo++) {
			if (openinfo->davbtype != DAVB_QUEUE || openinfo->trapindex != i2) continue;
			openinfo->trapindex = 0;
			for (start2 = EVENT_MAXIMUM; start2 < maineventidcount; start2++) {
				if (maineventids[start2] == openinfo->v.qvareventid) {
					maineventids[start2] = 0;
					break;
				}
			}
		}
		for (start2 = end2 = EVENT_MAXIMUM; start2 < maineventidcount; start2++) {
			if (maineventids[start2]) maineventids[end2++] = maineventids[start2];
		}
		maineventidcount = end2;
		evtmultiplex(maineventids, maineventidcount, actionvar, actionbit);
	}
	else if (i2 >= TRAPALLKEYS) {
		if (!trap[i2].type) {  /* clear traps */
			if (i2 == TRAPALLKEYS) TRAPALLKEYS_STARTEND
			else if (i2 == TRAPALLCHARS) TRAPALLCHARS_STARTEND
			else if (i2 == TRAPALLFKEYS) TRAPALLFKEYS_STARTEND
			else {
				i1 = type & TRAPVALUE_MASK;
				start1 = end1 = i1;
				if ((type & TRAPNOCASE) && i1 < 255 && isalpha(i1)) {
					if (isupper(i1)) i1 = tolower(i1);
					else i1 = toupper(i1);
					start2 = end2 = i1;
				}
				else start2 = 1, end2 = 0;
			}
			for (i1 = start1; i1 <= end1; i1++) {
				if (trapcharindex[i1] == i2) {
					trapcharindex[i1] = 0;
					if (i1 == VID_INTERRUPT || (i1 == DEBUGKEY && (dbcflags & DBCFLAG_DEBUG))) trapnullmap[i1 >> 3] |= (1 << (i1 & 0x07));
					else trapcharmap[i1 >> 3] &= ~(1 << (i1 & 0x07));
					if (fkeyshiftflag && i1 >= VID_F11 && i1 <= VID_F20) {
#if OS_UNIX
						if (dbcflags & DBCFLAG_CLIENTINIT) {
#endif
		 					trapcharmap[(VID_SHIFTF1 + i1 - VID_F11) >> 3] &= ~(1 << ((VID_SHIFTF1 + i1 - VID_F11) & 0x07));
#if OS_UNIX
						}
#endif
					}
				}
			}
			for (i1 = start2; i1 <= end2; i1++) {
				if (trapcharindex[i1] == i2) {
					trapcharindex[i1] = 0;
					trapcharmap[i1 >> 3] &= ~(1 << (i1 & 0x07));
					if (fkeyshiftflag && i1 >= VID_F11 && i1 <= VID_F20) {
#if OS_UNIX
						if (dbcflags & DBCFLAG_CLIENTINIT) {
#endif
							trapcharmap[(VID_SHIFTF1 + i1 - VID_F11) >> 3] &= ~(1 << ((VID_SHIFTF1 + i1 - VID_F11) & 0x07));
#if OS_UNIX
						}
#endif
					}
				}
			}
		}
		if (!(dbcflags & DBCFLAG_CLIENTINIT)) vidtrapstart(trapmap, maineventids[EVENT_TRAPCHAR]);
/*** CODE: CHOOSE CORRECT ERROR NUMBER ***/
		else if (clienttrapstart(trapmap, maineventids[EVENT_TRAPCHAR]) < 0) dbcerror(798);
	}

	if (pushreturnstack(-1)) dbcerror(502);
	setpcnt(trap[i2].module, trap[i2].pcount);
	return(0);
}

void resettrapmap()
{
	if (!(dbcflags & DBCFLAG_CLIENTINIT)) vidtrapstart(trapmap, maineventids[EVENT_TRAPCHAR]);
/*** CODE: CHOOSE CORRECT ERROR NUMBER ***/
	else if (clienttrapstart(trapmap, maineventids[EVENT_TRAPCHAR]) < 0) dbcerror(798);
}

/**
 * Return zero if success.
 * Return RC_NO_MEM, RC_INVALID_VALUE, or RC_ERROR
 */
INT ctlimageopen(INT bpp, INT h, INT v, INT datamod, INT dataoff, INT *refnum)
{
	CHAR work[8], work2[128];
	ELEMENT *e1;
	INT imgrefnum, i2;
	PIXHANDLE pixhandle;
	OPENINFO *openinfo;

	if (getnewrefnum(&imgrefnum)) goto error;

	if ( (i2 = guipixcreate(bpp, h, v, &pixhandle)) ) {
		*refnum = 0;
		return i2;
	}

	openinfo = *openinfopptr + imgrefnum - 1;
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		//clientput("<prep image=", -1);
		mscitoa(imgrefnum, work);
		strcpy(openinfo->name, work); /* name of image will be unique number */
		sprintf(work2, "<prep image=\"%s\" h=\"%d\" v=\"%d\" colorbits=\"%d\"/>", work, h, v, (INT)(*pixhandle)->bpp);
		clientput(work2, -1);
		if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
		e1 = (ELEMENT *) clientelement(); /* assume <r> element */
		if (e1 == NULL || e1->firstattribute != NULL) {
			clientrelease();
			goto error;
		}
		clientrelease();
	}

	openinfo->davbtype = DAVB_IMAGE;
	openinfo->u.pixhandle = pixhandle;
	openinfo->datamod = datamod;
	openinfo->dataoff = dataoff;
	*refnum = imgrefnum;
	return 0;
error:
	*refnum = 0;
	return RC_ERROR;
}

INT ctlqueueopen(INT entries, INT size, INT datamod, INT dataoff, INT *refnum)
{
	INT i1, eventid;
	QUEHANDLE quehandle;
	OPENINFO *openinfo;
	eventid = evtcreate();
	if (eventid == RC_ERROR) {
#if 0
		/* Enhance the error reporting? */
		evtgeterror();
#endif
		*refnum = 0;
		return RC_NO_MEM;
	}
	if (getnewrefnum(&i1)) goto error;
	if (quecreate(entries, size, eventid, &quehandle)) goto error;
	openinfo = *openinfopptr + i1 - 1;
	openinfo->davbtype = DAVB_QUEUE;
	openinfo->u.quehandle = quehandle;
	openinfo->v.qvareventid = eventid;
	openinfo->datamod = datamod;
	openinfo->dataoff = dataoff;
	openinfo->trapindex = 0;
	*refnum = i1;
	return(0);
error:
	*refnum = 0;
	return RC_ERROR;
}

/**
 * close all open image, queue, device and resource variables in datamod
 */
void ctlclosemod(INT datamod)
{
	INT i1, newtablehi;
	OPENINFO *openinfo;

	newtablehi = 0;
	for (i1 = 0; i1 < openinfohi; i1++) {
		openinfo = *openinfopptr + i1;
		if (!openinfo->davbtype) continue;
		if (datamod == 0) (aligndavb(findvar(openinfo->datamod, openinfo->dataoff), NULL))->refnum = 0;
		else if (datamod > 0 && datamod != openinfo->datamod) {
			newtablehi = i1 + 1;
			continue;
		}
		closerefnum(i1 + 1, datamod);
		setpgmdata();
	}
	openinfohi = newtablehi;
}

PIXHANDLE refnumtopixhandle(INT refnum)
{
	OPENINFO *openinfo;

	if (refnum < 1 || refnum > openinfohi) return(NULL);
	openinfo = *openinfopptr + refnum - 1;
	if (openinfo->davbtype != DAVB_IMAGE) return(NULL);
	return openinfo->u.pixhandle;
}

CHAR * refnumtoimagename(INT refnum)
{
	OPENINFO *openinfo;

	if (refnum < 1 || refnum > openinfohi) return(NULL);
	openinfo = *openinfopptr + refnum - 1;
	if (openinfo->davbtype != DAVB_IMAGE) return(NULL);
	return openinfo->name;
}

/*
 * Takes a window name, in the form that db/c programmers use in the code,
 * and returns the 'ident' field from the openinfo structure.
 *
 * This is only needed in a Smart Client situation, when a floatwindow
 * is being prepped and it has an owner.
 */
static CHAR * windowNameToIdent(CHAR *name_p)
{
	INT i1, n1;
	OPENINFO *openinfo;

	n1 = (INT)strlen(name_p) + 1;
	openinfo = *openinfopptr;
	for (i1 = 0; i1 < openinfohi; i1++, openinfo++) {
		if (openinfo->davbtype != DAVB_DEVICE || openinfo->devtype != DEV_WINDOW) continue;
		if (memcmp(openinfo->name, name_p, n1) == 0) return openinfo->ident;
	}
	return NULL;
}

void dbcctl(INT vx_p)
{
	CHAR work[64];
	if (dbcflags & DBCFLAG_DEBUG) vdebug(DEBUG_RESTORE);

	switch(vx_p) {
	case 0xB7:
		ctldraw(); break;
	case 0x6E:
		ctlgetcolor(); break;
	case 0x6F:
		ctlgetpos(); break;
	case 0xAC:
		ctlshow(); break;
	case 0xA8:
		ctlshowpos(); break;
	case 0xDB:
		ctlhide(); break;
	case 0xAE:
		ctlload(); break;
	case 0xAF:
		ctlstore(); break;
	case 0xB3:
		ctlget(); break;
	case 0xB4:
		ctlput(FALSE); break;
	case 0xB5:
		ctlwaitall(); break;
	case 0xB6:
		ctlwait(); break;
	case 0xBB:
		ctlempty(); break;
	case 0xCF:
		ctlput(TRUE); break;
	case 0xA9:
		ctlprep(); break;
	case 0xAA:
		ctlopen(); break;
	case 0xAB:
		ctlclose(); break;
	case 0xB0:
		ctlchange(); break;
	case 0xB1:
		ctlquery(); break;
	case 0xB8:
		ctllink(); break;
	case 0xDC:
		ctlunlink(); break;
	default:
		sprintf(work, "Bad opcode in function dbcctl (%d)", vx_p);
		dbcerrinfo(505, (UCHAR*)work, -1);
	}
}

static void ctlget()
{
	INT i1, errnum, bufpos, bufsize;
	INT rc;
	DAVB *queuevar;
	OPENINFO *openinfo;
	UCHAR *adr;

	queuevar = getdavb();
	openinfo = *openinfopptr + queuevar->refnum - 1;
	errnum = 0;
	bufsize = sizeof(dbcbuf);
	rc = queget(openinfo->u.quehandle, dbcbuf, &bufsize);
	if (rc) {
		if (rc < 0) errnum = 755;
		dbcflags |= DBCFLAG_OVER;
		bufsize = 0;
	}
	else dbcflags &= ~DBCFLAG_OVER;
	bufpos = 0;
	while ((adr = getlist(LIST_WRITE | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1)) != NULL)
		if ((i1 = recgetv(adr, 0, dbcbuf, &bufpos, bufsize)) && !errnum) errnum = i1;
	if (errnum) dbcerror(errnum);
	if (!disableflag) guiresume();
}

static void ctlput(INT firstflag)
{
	INT i1, listflg, flags;
	UINT bufpos, bufsize;
	UCHAR c1, *adr, *fmtadr = NULL;
	DAVB *queuevar;
	OPENINFO *openinfo;

	queuevar = getdavb();
	openinfo = *openinfopptr + queuevar->refnum - 1;
	bufpos = bufsize = flags = 0;
	listflg = LIST_READ | LIST_PROG | LIST_NUM1;
	for ( ; ; ) {
		if ((adr = getlist(listflg)) != NULL) {  /* it's a variable */
			if (vartype & (TYPE_LIST | TYPE_ARRAY)) {  /* list or array variable */
				listflg = (listflg & ~LIST_PROG) | LIST_LIST | LIST_ARRAY;
				continue;
			}
			recputv(adr, flags, dbcbuf, &bufpos, &bufsize, sizeof(dbcbuf), fmtadr);
			if ((listflg & LIST_PROG) && !(vartype & TYPE_LITERAL)) flags &= ~(RECFLAG_ZERO_FILL | RECFLAG_ZERO_SUPPRESS | RECFLAG_MINUS_OVERPUNCH | RECFLAG_FORMAT);
			continue;
		}
		if (!(listflg & LIST_PROG)) {
			listflg = (listflg & ~(LIST_LIST | LIST_ARRAY)) | LIST_PROG;
			flags &= ~(RECFLAG_ZERO_FILL | RECFLAG_ZERO_SUPPRESS | RECFLAG_MINUS_OVERPUNCH | RECFLAG_FORMAT);
			continue;
		}

		c1 = pgm[pcount - 1];
		if (c1 == 0xD2) flags |= RECFLAG_ZERO_FILL;
		else if (c1 == 0xF1) {
			flags |= RECFLAG_LOGICAL_LENGTH;
			flags &= ~RECFLAG_BLANK_SUPPRESS;
		}
		else if (c1 == 0xF2) flags &= ~(RECFLAG_LOGICAL_LENGTH | RECFLAG_BLANK_SUPPRESS);
		else if (c1 == 0xF3) flags |= RECFLAG_MINUS_OVERPUNCH;
		else if (c1 == 0xFA) {
			c1 = getbyte();
			if (c1 == 0x31) {
				fmtadr = getlist(listflg);
				flags |= RECFLAG_FORMAT;
			}
			else if (c1 == 0x37) {
				flags |= RECFLAG_BLANK_SUPPRESS;
				flags &= ~RECFLAG_LOGICAL_LENGTH;
			}
			else if (c1 == 0x38) flags |= RECFLAG_ZERO_SUPPRESS;
		}
		else if (c1 == 0xFB) {
			c1 = getbyte();
			if (bufpos < sizeof(dbcbuf)) {
				dbcbuf[bufpos++] = c1;
				if (bufpos > bufsize) bufsize = bufpos;
			}
		}
		else break;
	}
	if (firstflag) i1 = queputfirst(openinfo->u.quehandle, dbcbuf, bufsize);
	else i1 = queput(openinfo->u.quehandle, dbcbuf, bufsize);
	if (!i1) dbcflags &= ~DBCFLAG_OVER;
	else dbcflags |= DBCFLAG_OVER;
}

static void ctlwaitall()
{
	INT i1, i2, eventidlist[100];
	OPENINFO *openinfo;

	if (fpicnt) filepi0();  /* cancel any filepi 4/13/04 jpr*/
	for (i1 = i2 = 0; i1 < 100 && i2 < openinfohi; i2++) {
		openinfo = *openinfopptr + i2;
		if (openinfo->davbtype == DAVB_QUEUE && !openinfo->trapindex) eventidlist[i1++] = openinfo->v.qvareventid;
	}
	i2 = 100 - i1;
	comevents(0, &eventidlist[i1], &i2);
	i1 += i2;
	dbcwait(eventidlist, i1);
}

static void ctlwait()
{
	INT i1, i2, eventidlist[100];
	DAVB *queuevar;
	OPENINFO *openinfo;

	if (fpicnt) filepi0();  /* cancel any filepi 4/13/04 jpr*/
	for (i1 = 0; i1 < 100 && pgm[pcount] != 0xFF; ) {
		queuevar = getdavb();
		if (!queuevar->refnum) continue;
		if (queuevar->type == DAVB_QUEUE) {
			openinfo = *openinfopptr + queuevar->refnum - 1;
			if (!openinfo->trapindex) {
				eventidlist[i1++] = openinfo->v.qvareventid;
			}
		}
		else if (queuevar->type == DAVB_COMFILE) {
			i2 = 100 - i1;
			comevents(queuevar->refnum, &eventidlist[i1], &i2);
			i1 += i2;
		}

	}
	pcount++;
	dbcwait(eventidlist, i1);
}

static void ctlempty()
{
	DAVB *queuevar;
	OPENINFO *openinfo;

	while (pgm[pcount] != 0xFF) {
		queuevar = getdavb();
		openinfo = *openinfopptr + queuevar->refnum - 1;
		queempty(openinfo->u.quehandle);
	}
	pcount++;
}

static void ctlprep()
{
	ELEMENT *e1;
	CHAR *ident;
	CHAR work[32];
	INT i1, i2;
	INT refnum; /* One-based index into array of OPENINFO structures */
	INT defstringlen;
	INT mod, offset;
	INT hpos, vpos, horz, vert, style;
	CHAR winname[9], title[256], owner[MAXRESNAMELEN + 1];
	UCHAR type, timername[MAXTIMERNAMELEN], *defstring;
	DAVB *resdevvar; /* The DAVB variable that is the 1st parameter of the PREP */
	OPENINFO *openinfo;
	DEVTIMERHANDLE timehandle;
	RESHANDLE reshandle; /* Handle to the RESOURCE structure that will be allocated in this function */
	WINHANDLE winhandle = NULL;

	setpgmdata();
	resdevvar = getdavb();

	type = resdevvar->type;
	if (type != DAVB_DEVICE && type != DAVB_RESOURCE) {
		dbcerrinfo(505, (UCHAR*)"Attempt to prep a non-device/non-resource", -1);
	}

	refnum = resdevvar->refnum;
	resdevvar->refnum = 0;
	if (refnum) {
		closerefnum(refnum, FALSE); /* If this DAVB was open, close it */
		setpgmdata();
	}
	else if (getnewrefnum(&refnum) /* assign an OPENINFO structure */) {
		dbcerror(type == DAVB_DEVICE ? 765 : 767);
		return;
	}
	getlastvar(&mod, &offset); /* Save the mod and offset of the DAVB structure */

	/* prep resource */
	if (type == DAVB_RESOURCE) {
		if (dbcflags & DBCFLAG_CLIENTINIT) i1 = 0;
		else i1 = guirescreate(&reshandle) /* Allocate a RESOURCE struct */;
		setpgmdata(); /* because guirescreate does a memalloc */
		defstring = getvar(VAR_READ);
		defstring += hl + fp - 1;
		defstringlen = lp - fp + 1;
		if (i1) {
			dbcerror(767);
			return;
		}
		openinfo = *openinfopptr + refnum - 1;
		makeres(reshandle, defstring, defstringlen, openinfo);
		if (!(dbcflags & DBCFLAG_CLIENTINIT)) guiresmsgfnc(reshandle, resourcecbfnc);
		openinfo->davbtype = DAVB_RESOURCE;
		openinfo->u.reshandle = reshandle;
		goto ctlprepok;
	}

	/* prep device */
	if (type == DAVB_DEVICE) {
		defstring = getvar(VAR_READ);
		defstring += hl + fp - 1;
		defstringlen = lp - fp + 1;
		while (defstringlen && *defstring == ' ') defstring++, defstringlen--;
		for (i1 = defstringlen; i1 && defstring[i1 - 1] == ' '; i1--);
		for (i2 = i1 - 4; i2 > 0 && defstring[i2] != '.'; i2--);
		if (i2 > 0 && !ctlmemicmp(&defstring[i2], ".TIF", 4) && (i2 + 4 == i1 || defstring[i2 + 4] == ':')) {
			memcpy(name, defstring, (UINT) i1);
			name[i1] = '\0';
			i2 = srvgetserver(name, FIO_OPT_IMGPATH);
			if (i2) {
				if (i2 == -1) {
					dbcerrinfo(780, (UCHAR *) name, -1);
				}
				miofixname(name, "", 0);  /* need compat conversions */
				i1 = fsprepraw(i2, name, 0);
				if (i1 < 0) {
					/* convert back to fio type error (required for pre-fs 2.1.3) */
					if (i1 == -601) i1 = ERR_FNOTF;
					else if (i1 == -602 || i1 == -607) i1 = ERR_NOACC;
					else if (i1 == -603) i1 = ERR_BADNM;
					else if (i1 == -614) i1 = ERR_NOMEM;
					else if (i1 < -100) i1 = ERR_OPERR;
					else if (i1 == -1) {
						if (dsperrflg) {
							fsgeterror(name, sizeof(name));
							dbcerrinfo(781, (UCHAR *) name, -1);
						}
						else dbcerror(781);
					}
				}
			}
			else i1 = fioopen(name, FIO_M_PRP | FIO_P_IMG);
			setpgmdata();
			if (i1 < 0) {
				if (dsperrflg) {
					fsgeterror(name, sizeof(name));
					dbcerrinfo(761, (UCHAR *) name, -1);
				}
				dbcerror(761);
			}
			openinfo = *openinfopptr + refnum - 1;
			openinfo->davbtype = DAVB_DEVICE;
			openinfo->devtype = DEV_TIFFILE;
			openinfo->u.inthandle = i1;
			openinfo->v.srvhandle = i2;
			goto ctlprepok;
		}
		scaninit(defstring, defstringlen);
		scanname();
		i1 = nametokeywordvalue();
		if (i1 == KEYWORD_TIMER) {
			scanequal();
			scanname();
			if (scanstringlen && scanstringlen <= MAXTIMERNAMELEN) {
				memset(timername, ' ', MAXTIMERNAMELEN);
				memcpy(timername, scanstring, scanstringlen);
			}
			else dbcerrinfo(761, (UCHAR *) defstring, defstringlen);
			if (scancomma()) dbcerrinfo(761, (UCHAR *) defstring, defstringlen);
			openinfo = *openinfopptr + refnum - 1;
			i1 = devtimercreate(timername, &timehandle, refnum);
			setpgmdata();
			if (i1 < 0) dbcerror(761);
			openinfo->davbtype = DAVB_DEVICE;
			openinfo->devtype = DEV_TIMER;
			openinfo->u.timerhandle = timehandle;
			goto ctlprepok;
		}
		if (i1 == -1) {  /* check for WINDOWnn */
			if (scanstringlen == 8 && !ctlmemicmp(scanstring, "WINDOW", 6)) i1 = KEYWORD_WINDOW;
		}
		else if (i1 == KEYWORD_WINDOW || i1 == KEYWORD_FLOATWINDOW) {
			scanequal();
			scanname();
			if (scanstringlen > 8) scanstringlen = 8;
		}
		if (i1 == KEYWORD_WINDOW || i1 == KEYWORD_FLOATWINDOW) {
			memcpy((UCHAR *) winname, scanstring, scanstringlen);
			winname[scanstringlen] = title[0] = owner[0] = '\0';
			hpos = vpos = INT_MIN;
			horz = 200;
			vert = 100;

			style = WINDOW_STYLE_AUTOSCROLL | WINDOW_STYLE_CENTERED;
			if (i1 == KEYWORD_FLOATWINDOW) style |= WINDOW_STYLE_FLOATWINDOW;
			else style |= WINDOW_STYLE_SIZEABLE;

			openinfo = *openinfopptr + refnum - 1;
			while (scancomma()) {
				scanname();
				switch(nametokeywordvalue()) {
				case KEYWORD_POS:
					/* The DX doc says that the position coords are one-based. */
					scanequal();
					hpos = scannumber();
					scancolon();
					vpos = scannumber();
					if (vpos < 1) vpos = 1;
					style &= ~WINDOW_STYLE_CENTERED;
					break;
				case KEYWORD_SIZE:
					scanequal();
					horz = scannumber();
					scancolon();
					vert = scannumber();
					break;
				case KEYWORD_TITLE:
					scanequal();
					scantext();
					strcpy(title, scanstring);
					break;
				case KEYWORD_NOCLOSE:
					style |= WINDOW_STYLE_NOCLOSEBUTTON;
					break;
				case KEYWORD_NOFOCUS:
					if (!(style & WINDOW_STYLE_FLOATWINDOW)) style |= WINDOW_STYLE_NOFOCUS;
					break;
				case KEYWORD_FIXSIZE:
					style &= ~WINDOW_STYLE_SIZEABLE;
					break;
				case KEYWORD_MAXIMIZE:
					if (!(style & WINDOW_STYLE_FLOATWINDOW)) style |= WINDOW_STYLE_MAXIMIZE;
					break;
				case KEYWORD_PANDLGSCALE:
					/**
					 * Odd thing. This was wrong from the get-go.
					 * Positions and sizes of controls inside a floatwindow *are* scaled.
					 * So the size should be too.
					 * Fixed this on 27 APR 2021
					 */
					/*if (!(style & WINDOW_STYLE_FLOATWINDOW))*/ style |= WINDOW_STYLE_PANDLGSCALE;
					break;
				case KEYWORD_NOSCROLLBARS:
					style &= ~WINDOW_STYLE_AUTOSCROLL;
					break;
				case KEYWORD_NOTASKBARBUTTON:
					/* This does not apply to float windows, they never have one anyway */
					if (!(style & WINDOW_STYLE_FLOATWINDOW)) style |= WINDOW_STYLE_NOTASKBARBUTTON;
					break;
				case KEYWORD_STATUSBAR:
					style |= WINDOW_STYLE_STATUSBAR;
					break;
				case KEYWORD_OWNER:
					scanequal();
					scantext();
					strcpy(owner, scanstring);
					break;
				default:
					dbcerrinfo(771, (UCHAR *) scanstring, scanstringlen);
				}
			}
			if (!title[0]) strcpy(title, winname);

			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (style & WINDOW_STYLE_FLOATWINDOW) clientput("<prep floatwindow=\"", -1);
				else clientput("<prep window=\"", -1);
				strcpy(openinfo->ident, winname);
				getuniqueid(openinfo->ident);
				clientput(openinfo->ident, -1);
				clientput("\"", 1);
				if (!(style & WINDOW_STYLE_CENTERED)) {
					clientput(" posx=\"", -1);
					mscitoa(hpos, work);
					clientput(work, -1);
					clientput("\" posy=\"", -1);
					mscitoa(vpos, work);
					clientput(work, -1);
					clientput("\"", 1);
				}
				clientput(" hsize=\"", -1);
				mscitoa(horz, work);
				clientput(work, -1);
				clientput("\" vsize=\"", -1);
				mscitoa(vert, work);
				clientput(work, -1);
				clientput("\" title=\"", -1);
				clientputdata(title, -1);
				clientput("\"", 1);
				if (owner[0] != '\0' && (style & WINDOW_STYLE_FLOATWINDOW)) {
					/*
					 * Search openinfo for owner == openinfo->name, then send
					 * openinfo->ident to the SC.
					 */
					ident = windowNameToIdent(owner);
					if (ident != NULL) {
						clientput(" owner=\"", -1);
						clientputdata(ident, -1);
						clientput("\"", 1);
					}
				}
				if (style & WINDOW_STYLE_NOCLOSEBUTTON) clientput(" noclose=\"yes\"", -1);
				if (style & WINDOW_STYLE_NOFOCUS) clientput(" nofocus=\"yes\"", -1);
				if (!(style & WINDOW_STYLE_AUTOSCROLL)) clientput(" scrollbars=\"no\"", -1);
				if (!(style & WINDOW_STYLE_SIZEABLE)) clientput(" fixsize=\"yes\"", -1);
				if (style & WINDOW_STYLE_MAXIMIZE) clientput(" maximize=\"yes\"", -1);
				if (style & WINDOW_STYLE_PANDLGSCALE) clientput(" pandlgscale=\"yes\"", -1);
				if (style & WINDOW_STYLE_NOTASKBARBUTTON) clientput(" notaskbarbutton=\"yes\"", -1);
				if (style & WINDOW_STYLE_STATUSBAR) clientput(" statusbar=\"yes\"", -1);
				clientput("/>", -1);
				if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
				e1 = (ELEMENT *) clientelement(); /* assume <r> element */
				if (e1 == NULL || e1->firstattribute != NULL) {
					clientrelease();
					i1 = -1;
				}
				clientrelease();
			}
			else {
				if (owner[0] != '\0' && !(style & WINDOW_STYLE_FLOATWINDOW)) owner[0] = '\0';
				i1 = guiwincreate(winname, title, style, hpos, vpos, horz, vert, &winhandle, owner);
			}
			setpgmdata();
			if (i1 < 0) dbcerror(765);
			if (!(dbcflags & DBCFLAG_CLIENTINIT)) guiwinmsgfnc(winhandle, devicecbfnc);
			openinfo->davbtype = DAVB_DEVICE;
			openinfo->devtype = DEV_WINDOW;
			openinfo->u.winhandle = winhandle;
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				memset(openinfo->name, 0, 8);
				strcpy(openinfo->name, winname);
			}
			goto ctlprepok;
		}
#if OS_WIN32
		if (i1 == KEYWORD_DDE) {
			memcpy(name, defstring, defstringlen);
			name[defstringlen] = 0;
			i1 = guiddestart((UCHAR*) name, ddecbfnc);
			if (i1 < 0) dbcerror(765);
			setpgmdata();
			openinfo = *openinfopptr + refnum - 1;
			openinfo->davbtype = DAVB_DEVICE;
			openinfo->devtype = DEV_DDE;
			openinfo->u.inthandle = i1;
			goto ctlprepok;
		}
#endif
#if OS_WIN32
		if (!dsrinitflag) loaddsr();
#endif
		memcpy((UCHAR *) name, defstring, defstringlen);
		name[defstringlen] = '\0';
		i1 = dsropen(name);
		setpgmdata();
		if (i1 < 0) {
			dbcerrinfo(761, (UCHAR *) name, defstringlen);
			return;
		}
		openinfo = *openinfopptr + refnum - 1;
		openinfo->davbtype = DAVB_DEVICE;
		openinfo->devtype = DEV_DSR;
		openinfo->u.inthandle = i1;
		goto ctlprepok;
	}
	dbcerrinfo(505, (UCHAR*)"Unknown internal fail in ctlprep", -1);

ctlprepok:
	openinfo->datamod = mod;
	openinfo->dataoff = offset;
	(aligndavb(findvar(mod, offset), NULL))->refnum = refnum;
}

static void ctlopen()
{
	INT i1, i2, i3, i4, i5, num, mult, refnum, mod, offset;
	UCHAR devtype, type;
	DAVB *resdevvar;
	OPENINFO *openinfo;

	resdevvar = getdavb();
	getlastvar(&mod, &offset);
	type = resdevvar->type;
	refnum = resdevvar->refnum;
	resdevvar->refnum = 0;
	if (refnum) {
		closerefnum(refnum, FALSE);
		setpgmdata();
	}
	else if (getnewrefnum(&refnum)) {
		if (type == DAVB_DEVICE) dbcerror(765);
		else dbcerror(767);
		return;
	}

	/* open resource */
	if (type == DAVB_RESOURCE) {
		getvar(VAR_READ);
		dbcerror(764);
		return;
	}

	/* open device */
	if (type == DAVB_DEVICE) {
		cvtoname(getvar(VAR_READ));
		for (i2 = 0; name[i2] == ' '; i2++);
		i1 = (INT)strlen(&name[i2]);
		if (i2) memmove(name, &name[i2], i1 + 1);
		num = 0;
		for (i2 = i1, i3 = 0; i2 > 0 && name[i2] != '.'; i2--) {
			if (name[i2] == '(') i3 = i2;
		}
		/* i3 is position of '(', which is > 0 when alternate extension is used */
		if (i2 > 0 && (i2 + 4 == i1 || name[i2 + 4] == ':' || i3)) {
			if (!ctlmemicmp(&name[i2], ".TIF", 4)) {
				devtype = DEV_TIFFILE;
			}
			else if (!ctlmemicmp(&name[i2], ".GIF", 4)) {
				devtype = DEV_GIFFILE;
			}
			else if (!ctlmemicmp(&name[i2], ".JPG", 4)) {
				devtype = DEV_JPGFILE;
			}
			else if (!ctlmemicmp(&name[i2], ".PNG", 4)) {
				devtype = DEV_PNGFILE;
			}
			else devtype = 0;
			if (devtype || i3) {
				if (i3) {
					/* Parse image load settings: image.ext(number, type)  */
					/*    number - image number in multiple image tif      */
					/*    type   - type of image, gif, tif, or jpg         */
					i2 = i3;
					/* i4 is counter of characters to the right of the ')' */
					for (i4 = 0, i3 = i1 - 1; i3 > i2;) {
						if (name[i3--] == ')') break;
						else i4++;
					}
					for (i5 = i3; i5 > i2; i5--) {
						if (name[i5] == ' ' || name[i5] == ',') continue;
						if (isdigit(name[i5])) {
							/* parse image number */
							for (mult = 1; i5 > i2 && name[i5] != ',' && name[i5] != ' '; i5--, mult *= 10) {
								num += (name[i5] - '0') * mult;
							}
						}
						else if (i5 - 2 > i2) {
							if (!ctlmemicmp(&name[i5 - 2], "JPG", 3)) devtype = DEV_JPGFILE;
							else if (!ctlmemicmp(&name[i5 - 2], "GIF", 3)) devtype = DEV_GIFFILE;
							else if (!ctlmemicmp(&name[i5 - 2], "TIF", 3)) devtype = DEV_TIFFILE;
							else if (!ctlmemicmp(&name[i5 - 2], "PNG", 3)) devtype = DEV_PNGFILE;
							else continue;
							i5 -= 2; /* decrement if match */
						}
					}
					/* remove parentheses and keep volume information */
					i3 = 0;
					while (i4 > 0) {
						name[i2 + i3] = name[(i1 - 1) - (--i4)];
						i3++;
					}
					name[i2 + i3] = '\0';
				}
				i2 = srvgetserver(name, FIO_OPT_IMGPATH);
				if (i2) {
					if (i2 == -1) {
						dbcerrinfo(780, (UCHAR *) name, -1);
					}
					miofixname(name, "", 0);  /* need compat conversions */
					i1 = fsopenraw(i2, name, FS_READONLY);
					if (i1 < 0) {
						/* convert back to fio type error (required for pre-fs 2.1.3) */
						if (i1 == -601) i1 = ERR_FNOTF;
						else if (i1 == -602 || i1 == -607) i1 = ERR_NOACC;
						else if (i1 == -603) i1 = ERR_BADNM;
						else if (i1 == -614) i1 = ERR_NOMEM;
						else if (i1 < -100) i1 = ERR_OPERR;
						else if (i1 == -1) {
							if (dsperrflg) {
								fsgeterror(name, sizeof(name));
								dbcerrinfo(781, (UCHAR *) name, -1);
							}
							else dbcerror(781);
						}
					}
				}
				else i1 = fioopen(name, FIO_M_SRO | FIO_P_IMG);
				setpgmdata();
				if (i1 < 0) {
					if (dsperrflg) {
						fsgeterror(name, sizeof(name));
						dbcerrinfo(761, (UCHAR *) name, -1);
					}
					else dbcerror(761);
				}
				/*if (i1 < 0) dbcerror(761);*/
				openinfo = *openinfopptr + refnum - 1;
				openinfo->davbtype = DAVB_DEVICE;
				openinfo->devtype = devtype;
				openinfo->u.inthandle = i1;
				openinfo->v.srvhandle = i2;
				openinfo->tifnum = num;
				goto ctlopenok;
			}
		}
		if (i1 == 9 && !ctlmemicmp(name, "CLIPBOARD", 9)) {
			openinfo = *openinfopptr + refnum - 1;
			openinfo->davbtype = DAVB_DEVICE;
			openinfo->devtype = DEV_CLIPBOARD;
			openinfo->u.inthandle = 1; /*** CODE - what is 1 for? ***/
			goto ctlopenok;
		}
		if (i1 == 11 && !ctlmemicmp(name, "APPLICATION", 11)) {
			openinfo = *openinfopptr + refnum - 1;
			openinfo->davbtype = DAVB_DEVICE;
			openinfo->devtype = DEV_APPLICATION;
			goto ctlopenok;
		}

#if OS_WIN32
		if (!dsrinitflag) loaddsr();
#endif
		i1 = dsropen(name);
		if (i1 < 0) {
			dbcerror(761);
			return;
		}
		openinfo = *openinfopptr + refnum - 1;
		openinfo->davbtype = DAVB_DEVICE;
		openinfo->devtype = DEV_DSR;
		openinfo->u.inthandle = i1;
		goto ctlopenok;
	}
	dbcerrinfo(505, (UCHAR*)"Attempt to open a non-device/non-resource", -1);
	return;

ctlopenok:
	openinfo->datamod = mod;
	openinfo->dataoff = offset;
	(aligndavb(findvar(mod, offset), NULL))->refnum = refnum;
}

static void ctlclose()
{
	INT refnum;
	DAVB *resdevvar;

	resdevvar = getdavb();
	if (!(refnum = resdevvar->refnum)) return;
	resdevvar->refnum = 0;

	closerefnum(refnum, FALSE);
	setpgmdata();
}

static void ctllink()
{
	DAVB *resdevvar;
	DAVB *queuevar;
	OPENINFO *openinfo, *openinfo2;

	resdevvar = getdavb();
	queuevar = getdavb();
	if (!resdevvar->refnum) dbcerror(791);
	openinfo = *openinfopptr + resdevvar->refnum - 1;
	openinfo2 = *openinfopptr + queuevar->refnum - 1;
	openinfo->v.linkquehandle = openinfo2->u.quehandle;
}

static void ctlunlink()
{
	DAVB *resdevvar;
	OPENINFO *openinfo;

	resdevvar = getdavb();
	if (resdevvar->refnum) {
		openinfo = *openinfopptr + resdevvar->refnum - 1;
		openinfo->v.linkquehandle = NULL;
	}
}

static void ctlchange()
{
	INT hl1, lp1, listflg, flags, i1;
	UINT bufpos, bufsize;
	UCHAR c1, *adr, *fmtadr = NULL, *var1;
	DAVB *resdevvar;
	OPENINFO *openinfo;

	resdevvar = getdavb();
	var1 = getvar(VAR_READ);
	hl1 = hl;
	lp1 = lp;

	bufpos = bufsize = flags = 0;
	listflg = LIST_READ | LIST_PROG | LIST_NUM1;
	for (i1 = 0 ; ; i1++) {
		if ((adr = getlist(listflg)) != NULL) {  /* it's a variable */
			if (vartype & (TYPE_LIST | TYPE_ARRAY)) {  /* list or array variable */
				listflg = (listflg & ~LIST_PROG) | LIST_LIST | LIST_ARRAY;
				continue;
			}
			recputv(adr, flags, dbcbuf, &bufpos, &bufsize, sizeof(dbcbuf), fmtadr);
			if ((listflg & LIST_PROG) && !(vartype & TYPE_LITERAL)) flags &= ~(RECFLAG_ZERO_FILL | RECFLAG_ZERO_SUPPRESS | RECFLAG_MINUS_OVERPUNCH);
			continue;
		}
		if (!(listflg & LIST_PROG)) {
			listflg = (listflg & ~(LIST_LIST | LIST_ARRAY)) | LIST_PROG;
			flags &= ~(RECFLAG_ZERO_FILL | RECFLAG_ZERO_SUPPRESS | RECFLAG_MINUS_OVERPUNCH);
			continue;
		}

		c1 = pgm[pcount - 1];
		if (c1 == 0xD2) flags |= RECFLAG_ZERO_FILL;
		else if (c1 == 0xF1) {
			flags |= RECFLAG_LOGICAL_LENGTH;
			flags &= ~RECFLAG_BLANK_SUPPRESS;
		}
		else if (c1 == 0xF2) flags &= ~(RECFLAG_LOGICAL_LENGTH | RECFLAG_BLANK_SUPPRESS);
		else if (c1 == 0xF3) flags |= RECFLAG_MINUS_OVERPUNCH;
		else if (c1 == 0xFA) {
			c1 = getbyte();
			if (c1 == 0x31) {
				fmtadr = getlist(listflg);
				flags |= RECFLAG_FORMAT;
			}
			else if (c1 == 0x37) {
				flags |= RECFLAG_BLANK_SUPPRESS;
				flags &= ~RECFLAG_LOGICAL_LENGTH;
			}
			else if (c1 == 0x38) flags |= RECFLAG_ZERO_SUPPRESS;
		}
		else if (c1 == 0xFB) {
			c1 = getbyte();
			if (bufpos < sizeof(dbcbuf)) {
				dbcbuf[bufpos++] = c1;
				if (bufpos > bufsize) bufsize = bufpos;
			}
		}
		else break;
	}

	if (!resdevvar->refnum) dbcerror(791);

	openinfo = *openinfopptr + resdevvar->refnum - 1;
	if (openinfo->davbtype == DAVB_RESOURCE) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			guiserverreschange(openinfo->ident, openinfo->restype, &var1[hl1], lp1, dbcbuf, bufsize, i1);
		}
		else {
			if (guimemicmp(&var1[hl1], "multiple", lp1) == 0) {
				i1 = guiMultiChange(openinfo->u.reshandle, dbcbuf, bufsize);
			}
			else {
				i1 = guireschange(openinfo->u.reshandle, &var1[hl1], lp1, dbcbuf, bufsize);
			}
			if (i1) {
				if (i1 == RC_NO_MEM) dbcerror(795);
				else dbcerror(794);
			}
		}
		return;
	}
	if (openinfo->davbtype == DAVB_DEVICE) {
		switch(openinfo->devtype) {
		case DEV_DSR:
			cvtoname(var1);
			dsrchange(openinfo->u.inthandle, name, dbcbuf, (INT) bufsize);
			break;
		case DEV_WINDOW:
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				i1 = guiserverwinchange(openinfo->ident, &var1[hl1], lp1, dbcbuf, (INT) bufsize);
				if (i1) {
					if (i1 == 1) dbcerrinfo(798, NULL, 0);
					else if (i1 == 2) dbcerrinfo(794, (UCHAR*)errormsg, -1);
					else dbcerror(794);
				}
			}
			else if (guiwinchange(openinfo->u.winhandle, &var1[hl1], lp1, dbcbuf, (INT) bufsize)) dbcerror(794);
			break;
		case DEV_TIMER:
			ctltimerchange(openinfo, &var1[hl1], lp1, dbcbuf, (INT) bufsize);
			break;
		case DEV_APPLICATION:
			ctlappchange(&var1[hl1], lp1, dbcbuf, (INT) bufsize);
			break;
#if OS_WIN32
		case DEV_DDE:
			if (guiddechange(openinfo->u.inthandle, &var1[hl1], lp1, dbcbuf, (INT) bufsize)) dbcerror(794);
			break;
#endif
		default:
			dbcerror(792);
		}
		return;
	}
	dbcerrinfo(505, (UCHAR*)"Attempt to change a non-device/non-resource", -1);
}

static void ctlqueryClipboard(UCHAR *var1, INT hl1) {
	INT i1 = -1, count, bufpos, bufsize = 0, errnum, hsize, vsize;
	ELEMENT *e1;
	ATTRIBUTE *a1;
	UCHAR *adr;

	if (ctlmemicmp(&var1[hl1], "IMAGESIZE", 9) && ctlmemicmp(&var1[hl1], "BITSPERPIXEL", 12)
		&& ctlmemicmp(&var1[hl1], "RESOLUTION", 10) && ctlmemicmp(&var1[hl1], "IMAGECOUNT", 10))
	{
		dbcerrinfo(793, (UCHAR*)"Invalid function for clipboard query", -1);
	}
	if (!ctlmemicmp(&var1[hl1], "BITSPERPIXEL", 12)) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("<query clipboard=\"0\" f=\"bitsperpixel\"/>", -1);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
			e1 = (ELEMENT *) clientelement();
			if (e1 == NULL || (strcmp(e1->tag, "status") && strcmp(e1->tag, "r"))) {
				clientrelease();
				dbcerror(798);
			}
			if (!strcmp(e1->tag, "r")) { /* error occurred */
				clientrelease();
			}
			else {
				a1 = e1->firstattribute;
				if (memcmp(a1->tag, "clipboard", 9)) {
					clientrelease();
					dbcerrinfo(798, NULL, 0);
				}
				a1 = a1->nextattribute;
				if (a1 == NULL) {
					clientrelease();
					dbcerrinfo(798, NULL, 0);
				}
				count = atoi(a1->value);
				clientrelease();
				i1 = 0;
			}
		}
		else i1 = guicbdbpp(&count);
		if (i1 < 0) dbcerror(792);
		itonum5(count, dbcbuf);
		bufsize = 5;
	}
	else if (!ctlmemicmp(&var1[hl1], "RESOLUTION", 10)) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("<query clipboard=\"0\" f=\"resolution\"/>", -1);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
			e1 = (ELEMENT *) clientelement();
			if (e1 == NULL || (strcmp(e1->tag, "status") && strcmp(e1->tag, "r"))) {
				clientrelease();
				dbcerror(798);
			}
			if (!strcmp(e1->tag, "r")) { /* error occurred */
				clientrelease();
			}
			else {
				a1 = e1->firstattribute;
				if (memcmp(a1->tag, "clipboard", 9)) {
					clientrelease();
					dbcerrinfo(798, NULL, 0);
				}
				a1 = a1->nextattribute;
				if (a1 == NULL) {
					clientrelease();
					dbcerrinfo(798, NULL, 0);
				}
				hsize = atoi(a1->value);
				a1 = a1->nextattribute;
				if (a1 == NULL) {
					clientrelease();
					dbcerrinfo(798, NULL, 0);
				}
				vsize = atoi(a1->value);
				clientrelease();
				i1 = 0;
			}
		}
		else i1 = guicbdres(&hsize, &vsize);
		if (i1 < 0) dbcerror(792);
		itonum5(hsize, dbcbuf);
		itonum5(vsize, &dbcbuf[5]);
		bufsize = 10;
	}
	else if (!ctlmemicmp(&var1[hl1], "IMAGESIZE", 9)) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("<query clipboard=\"0\" f=\"imagesize\"/>", -1);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
			e1 = (ELEMENT *) clientelement();
			if (e1 == NULL || (strcmp(e1->tag, "status") && strcmp(e1->tag, "r"))) {
				clientrelease();
				dbcerror(798);
			}
			if (!strcmp(e1->tag, "r")) { /* error occurred */
				clientrelease();
			}
			else {
				a1 = e1->firstattribute;
				if (memcmp(a1->tag, "clipboard", 9)) {
					clientrelease();
					dbcerrinfo(798, NULL, 0);
				}
				a1 = a1->nextattribute;
				if (a1 == NULL) {
					clientrelease();
					dbcerrinfo(798, NULL, 0);
				}
				hsize = atoi(a1->value);
				a1 = a1->nextattribute;
				if (a1 == NULL) {
					clientrelease();
					dbcerrinfo(798, NULL, 0);
				}
				vsize = atoi(a1->value);
				clientrelease();
				i1 = 0;
			}
		}
		else i1 = guicbdsize(&hsize, &vsize);
		if (i1 < 0) dbcerror(792);
		itonum5(hsize, dbcbuf);
		itonum5(vsize, &dbcbuf[5]);
		bufsize = 10;
	}
	else if (!ctlmemicmp(&var1[hl1], "IMAGECOUNT", 10)) {
		count = 1;
		itonum5(count, dbcbuf);
		bufsize = 5;
	}

	errnum = 0;
	bufpos = 0;
	while ((adr = getlist(LIST_WRITE | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1)) != NULL)
		if ((i1 = recgetv(adr, 0, dbcbuf, &bufpos, bufsize)) && !errnum) errnum = i1;
	if (errnum) dbcerror(errnum);
}

/**
 * QUERY
 */
static void ctlquery()
{
	INT i1, pos, errnum, hl1, lp1, bufpos, bufsize, func;
	INT len, filehandle, srvhandle, hsize = 0, vsize = 0, count = 0;
	UCHAR **pptr = NULL;
	CHAR cmd[20],work[8];
	DAVB *resdevvar;
	UCHAR *var1, *adr;
	OFFSET eofpos;
	OPENINFO *openinfo;
	ELEMENT *e1;
	ATTRIBUTE *a1;
	CHAR c1;
#if OS_WIN32
	WINDOW *win;
#endif

	resdevvar = getdavb();
	if (!resdevvar->refnum) dbcerror(791);
	openinfo = *openinfopptr + resdevvar->refnum - 1;
	var1 = getvar(VAR_READ);
	hl1 = hl;
	lp1 = lp;
	bufsize = sizeof(dbcbuf);
	if (openinfo->davbtype == DAVB_RESOURCE) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("<query ", -1);
			if (openinfo->restype == RES_TYPE_PANEL) clientput("panel=\"", -1);
			else if (openinfo->restype == RES_TYPE_DIALOG) clientput("dialog=\"", -1);
			else if (openinfo->restype == RES_TYPE_MENU) clientput("menu=\"", -1);
			else if (openinfo->restype == RES_TYPE_TOOLBAR) clientput("toolbar=\"", -1);
			else if (openinfo->restype == RES_TYPE_POPUPMENU) clientput("popupmenu=\"", -1);
			else dbcerrinfo(798, NULL, 0);
			clientput(openinfo->ident, -1);
			clientput("\" n=\"", -1);
			for (pos = 0; pos < lp1 && var1[hl1 + pos] != ' ' && !isdigit(var1[hl1 + pos]); pos++) {
				cmd[pos] = toupper(var1[hl1 + pos]);
			}
			cmd[pos] = '\0';
			if (memcmp(cmd, "STATUS", 6) == 0) func = 1;
			else if (memcmp(cmd, "GETROWCOUNT", 11) == 0) func = 2;
			else dbcerrinfo(798, NULL, 0);
			while (pos < lp1 && var1[hl1 + pos] == ' ') pos++;  /* skip leading blanks */

			for (; pos < lp1; pos++) {
				c1 = var1[hl1 + pos];
				if (c1 == ' ') continue;
				if (isdigit(c1) || c1 == '[' || c1 == ']') clientput(&c1, 1);
				else break;
			}
			clientput("\" f=\"", -1);
			i1 = mscitoa(func, work);
			clientput(work, i1);
			clientput("\"/>", -1);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
			e1 = (ELEMENT *) clientelement();

			if (e1 == NULL) {
				clientrelease();
				dbcerror(798);
			}
			else if (!strcmp(e1->tag, "r")) {
				clientrelease();
				bufsize = 0;
				dbcerror(793);
			}

			if (strcmp(e1->tag, "status") && strcmp(e1->tag, "getrowcount")) {
				clientrelease();
				dbcerror(798);
			}
			else {
				a1 = e1->firstattribute;
				if (!(!memcmp(a1->tag, "menu", 4) || !memcmp(a1->tag, "popupmenu", 9) ||
					!memcmp(a1->tag, "toolbar", 7) ||
					!memcmp(a1->tag, "dialog", 6) || !memcmp(a1->tag, "panel", 5))) {
					clientrelease();
					dbcerrinfo(798, NULL, 0);
				}
				a1 = a1->nextattribute;
				if (a1 == NULL) {
					clientrelease();
					dbcerrinfo(798, NULL, 0);
				}
				if (a1->tag[0] == 'i') a1 = a1->nextattribute; /* this attribute is not needed */
				strcpy((CHAR *) dbcbuf, a1->value); /* assume 't' tag */
				bufsize = (INT)strlen((CHAR *) dbcbuf);
				clientrelease();
			}
		}
/*** CODE: DBCBUF SHOULD PROBABLY NOT BE USED FOR HERE AND ABOVE ***/
		else if (guiresquery(openinfo->u.reshandle, &var1[hl1], lp1, dbcbuf, &bufsize)) {
			bufsize = 0;
			dbcerror(793);
		}
	}
	else if (openinfo->davbtype == DAVB_DEVICE) {
		if (openinfo->devtype == DEV_CLIPBOARD) {
			ctlqueryClipboard(var1, hl1);
			return;
		}
		switch (openinfo->devtype) {
		case DEV_GIFFILE:
		case DEV_TIFFILE:
		case DEV_JPGFILE:
		case DEV_PNGFILE:
			if (!ctlmemicmp(&var1[hl1], "IMAGESIZE", 9) || !ctlmemicmp(&var1[hl1], "BITSPERPIXEL", 12)
				|| !ctlmemicmp(&var1[hl1], "RESOLUTION", 10) || !ctlmemicmp(&var1[hl1], "IMAGECOUNT", 10))
			{
				filehandle = openinfo->u.inthandle;
				srvhandle = openinfo->v.srvhandle;
				if (srvhandle > 0) {
					i1 = fsfilesize(filehandle, &eofpos);
					if (i1) {
						dbcerror(792);
						return;
					}
					len = (INT) eofpos;
					if (len < 100) {
						dbcerror(792);
					}
					pptr = memalloc(len, 0);
					setpgmdata();
					if (pptr == NULL) {
						dbcerror(1667 /*792*/);
					}
					i1 = fsreadraw(filehandle, 0, (CHAR *) *pptr, len);
				}
				else {
					fiogetsize(filehandle, &eofpos);
					len = (INT) eofpos;
					if (len < 100) {
						dbcerror(792);
					}
					pptr = memalloc(len, 0);
					setpgmdata();
					if (pptr == NULL) {
						dbcerror(1667 /*792*/);
					}
					i1 = fioread(filehandle, 0, *pptr, len);
				}
				if (i1 == len) {
					i1 = -1;
					if (!ctlmemicmp(&var1[hl1], "BITSPERPIXEL", 12)) {
						if (openinfo->devtype == DEV_TIFFILE) i1 = guitifbpp(openinfo->tifnum, *pptr, &count);
						else if (openinfo->devtype == DEV_GIFFILE) i1 = guigifbpp(*pptr, &count);
						else if (openinfo->devtype == DEV_JPGFILE) i1 = guijpgbpp(*pptr, &count);
						else if (openinfo->devtype == DEV_PNGFILE) i1 = guipngbpp(*pptr, &count);
					}
					else if (!ctlmemicmp(&var1[hl1], "IMAGECOUNT", 10)) {
						if (openinfo->devtype == DEV_TIFFILE) i1 = guitifcount(*pptr, &count);
						else count = i1 = 1;
					}
					else if (!ctlmemicmp(&var1[hl1], "RESOLUTION", 10)) {
						if (openinfo->devtype == DEV_TIFFILE) i1 = guitifres(openinfo->tifnum, *pptr, &hsize, &vsize);
						else if (openinfo->devtype == DEV_GIFFILE) i1 = hsize = vsize = 0;
						else if (openinfo->devtype == DEV_JPGFILE) i1 = guijpgres(*pptr, &hsize, &vsize);
						else if (openinfo->devtype == DEV_PNGFILE) i1 = guipngres(*pptr, &hsize, &vsize);
					}
					else if (!ctlmemicmp(&var1[hl1], "IMAGESIZE", 9)) {
						if (openinfo->devtype == DEV_TIFFILE) i1 = guitifsize(openinfo->tifnum, *pptr, &hsize, &vsize);
						else if (openinfo->devtype == DEV_GIFFILE) i1 = guigifsize(*pptr, &hsize, &vsize);
						else if (openinfo->devtype == DEV_JPGFILE) i1 = guijpgsize(*pptr, &hsize, &vsize);
						else if (openinfo->devtype == DEV_PNGFILE) i1 = guipngsize(*pptr, &hsize, &vsize);
					}
				}
				else i1 = -1;
				if (pptr != NULL) memfree(pptr);
				if (i1 < 0) dbcerror(792);
				if (!ctlmemicmp(&var1[hl1], "BITSPERPIXEL", 12) || !ctlmemicmp(&var1[hl1], "IMAGECOUNT", 10)) {
					/* image bpp & image count are 5 characters, xxxxx */
					itonum5(count, dbcbuf);
					bufsize = 5;
				}
				else {
					/* image size or resolution is 10 characters, xxxxxyyyyy */
					itonum5(hsize, dbcbuf);
					itonum5(vsize, &dbcbuf[5]);
					bufsize = 10;
				}
			}
			else {
				bufsize = 0;
				dbcerror(793);
			}
			break;
		case DEV_DSR:
			cvtoname(var1);
			dsrquery(openinfo->u.inthandle, name, dbcbuf, &bufsize);
			break;
#if OS_WIN32
		case DEV_DDE:
			bufsize = 0;
			break;
#endif

		case DEV_WINDOW:
			if (!ctlmemicmp(&var1[hl1], "STATUSBARHEIGHT", 15)) {
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					if (dbcflags & DBCFLAG_CLIENTGUI) {
						if (smartClientSubType[0] != '\0'
							&& strlen(smartClientSubType) == 7
							&& !strcmp(smartClientSubType, "Servlet")) vsize = 0;
						else {
							clientput("<query window=\"", -1);
							clientput(openinfo->ident, -1);
							clientput("\" f=\"3\"/>", -1);
							if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
							e1 = (ELEMENT *) clientelement();
							if (e1 == NULL) {
								clientrelease();
								dbcerror(798);
							}
							else if (!strcmp(e1->tag, "r") || strcmp(e1->tag, "statusbarheight")) {
								clientrelease();
								dbcerror(793);
							}
							for (a1 = e1->firstattribute; a1 != NULL && strcmp(a1->tag, "t") != 0; a1 = a1->nextattribute);
							if (a1 == NULL) dbcerror(798);
							mscntoi((UCHAR*)a1->value, &vsize, (INT)strlen((CHAR *) a1->value));
						}
						itonum5(vsize, dbcbuf);
						bufsize = 5;
						clientrelease();
					}
					else {
						vsize = 0;
						itonum5(vsize, dbcbuf);
						bufsize = 5;
						clientrelease();
					}
				}
#if OS_WIN32
				else {
					win = *openinfo->u.winhandle;
					if (win->statusbarshown) vsize = win->borderbtm;
					else vsize = 0;
					itonum5(vsize, dbcbuf);
					bufsize = 5;
					clientrelease();
				}
#endif
			}
			else {
				clientrelease();
				dbcerror(793);
			}
			break;
		default:
			clientrelease();
			dbcerror(792);
			break;
		}
	}
	else {
		bufsize = 0;
		dbcerrinfo(505, (UCHAR*)"Attempt to query unknown type", -1);
	}
	errnum = 0;
	bufpos = 0;
	while ((adr = getlist(LIST_WRITE | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1)) != NULL)
		if ((i1 = recgetv(adr, 0, dbcbuf, &bufpos, bufsize)) && !errnum) errnum = i1;
	if (errnum) dbcerror(errnum);
}

static void ctlgetcolor()
{
	INT32 n, h, v;
	DAVB *imagevar;
	OPENINFO *openinfo;
	ELEMENT *e1;
	ATTRIBUTE *a1;

	imagevar = getdavb();
	openinfo = *openinfopptr + imagevar->refnum - 1;
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		clientput("<getcolor image=\"", -1);
		clientput(openinfo->name, -1);
		clientput("\"/>", -1);
		if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
		e1 = (ELEMENT *) clientelement();
		if (e1 == NULL || strcmp(e1->tag, "sendcolor")) {
			clientrelease();
			dbcerror(798);
		}
		a1 = e1->firstattribute;
		if (a1 == NULL) {
			clientrelease();
			dbcerrinfo(798, NULL, 0);
		}
		n = atoi(a1->value); /* assume 't' tag */
		clientrelease();
	}
	else {
		guipixgetpos(openinfo->u.pixhandle, &h, &v);
		guipixgetcolor(openinfo->u.pixhandle, &n, h, v);
	}
	itonv((INT) n, getvar(VAR_WRITE));
}

static void ctlgetpos()
{
	INT hpos = 0, vpos = 0, i1, i2;
	INT32 x1;
	DAVB *imagevar;
	OPENINFO *openinfo;
	ELEMENT *e1;
	ATTRIBUTE *a1;
	UCHAR work[6];

	imagevar = getdavb();
	openinfo = *openinfopptr + imagevar->refnum - 1;
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		clientput("<getpos image=\"", -1);
		clientput(openinfo->name, -1);
		clientput("\"/>", -1);
		if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
		e1 = (ELEMENT *) clientelement();
		if (e1 == NULL || strcmp(e1->tag, "sendpos")) {
			clientrelease();
			dbcerror(798);
		}
		a1 = e1->firstattribute;
		if (a1 == NULL) {
			clientrelease();
			dbcerrinfo(798, NULL, 0);
		}
		while (a1 != NULL) {
			if (a1->tag[0] == 'h') hpos = atoi(a1->value);
			else if (a1->tag[0] == 'v') vpos = atoi(a1->value);
			else {
				clientrelease();
				dbcerrinfo(798, NULL, 0);
			}
			a1 = a1->nextattribute;
		}
		clientrelease();
	}
	else guipixgetpos(openinfo->u.pixhandle, &hpos, &vpos);
	/* i1 is original state of eos, equal, and less before movevar */
	i1 = (dbcflags & DBCFLAG_FLAGS) & ~DBCFLAG_OVER;
	work[0] = 0xFC;
	work[1] = 0x1F;
	x1 = hpos + 1;
	memcpy(&work[2], (UCHAR *) &x1, sizeof(INT32));
	movevar(work, getvar(VAR_WRITE));
	i2 = dbcflags & DBCFLAG_OVER;
	x1 = vpos + 1;
	memcpy(&work[2], (UCHAR *) &x1, sizeof(INT32));
	movevar(work, getvar(VAR_WRITE));
	i2 |= dbcflags & DBCFLAG_OVER;
	dbcflags = (dbcflags & ~DBCFLAG_FLAGS) | i1 | i2;
#if 0
	itonv(hpos + 1, getvar(VAR_WRITE));
	itonv(vpos + 1, getvar(VAR_WRITE));
#endif
}

static void ctlshow()
{
	DAVB *resimgvar;
	DAVB *devicevar;
	OPENINFO *openinfo, *openinfo2;
	ELEMENT *e1;

	resimgvar = getdavb();
	devicevar = getdavb();
	if (!resimgvar->refnum) {
		if ((!(dbcflags & DBCFLAG_GUIINIT) && !(dbcflags & DBCFLAG_CLIENTINIT)) ||
			((dbcflags & DBCFLAG_CLIENTINIT) &&	!(dbcflags & DBCFLAG_CLIENTGUI))) {
			dbcerror(564);
		}
		dbcerror(791);
	}
	else if (!devicevar->refnum) dbcerror(791);
	openinfo = *openinfopptr + resimgvar->refnum - 1;
	openinfo2 = *openinfopptr + devicevar->refnum - 1;
	if (openinfo->davbtype == DAVB_RESOURCE) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			if (openinfo->restype == RES_TYPE_PANEL) clientput("<show panel=\"", -1);
			else if (openinfo->restype == RES_TYPE_DIALOG) clientput("<show dialog=\"", -1);
			else if (openinfo->restype == RES_TYPE_MENU) clientput("<show menu=\"", -1);
			else if (openinfo->restype == RES_TYPE_TOOLBAR) clientput("<show toolbar=\"", -1);
			else if (openinfo->restype == RES_TYPE_POPUPMENU) clientput("<show popupmenu=\"", -1);
			else if (openinfo->restype == RES_TYPE_OPENFILEDLG) clientput("<show openfdlg=\"", -1);
			else if (openinfo->restype == RES_TYPE_OPENDIRDLG) clientput("<show openddlg=\"", -1);
			else if (openinfo->restype == RES_TYPE_SAVEASFILEDLG) clientput("<show saveasfdlg=\"", -1);
			else if (openinfo->restype == RES_TYPE_FONTDLG) clientput("<show fontdlg=\"", -1);
			else if (openinfo->restype == RES_TYPE_COLORDLG) clientput("<show colordlg=\"", -1);
			else if (openinfo->restype == RES_TYPE_ALERTDLG) clientput("<show alertdlg=\"", -1);
			else dbcerrinfo(798, NULL, 0);
			clientput(openinfo->ident, -1);
			clientput("\" window=\"", -1);
			clientput(openinfo2->ident, -1);
			clientput("\"/>", -1);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
			e1 = (ELEMENT *) clientelement(); /* assume <r> element */
			if (e1 == NULL || e1->firstattribute != NULL) {
				clientrelease();
				dbcerror(799);
			}
			clientrelease();
		}
		else {
			showResourceLocal(openinfo->u.reshandle, openinfo2->u.winhandle, INT_MIN, INT_MIN);
		}
	}
	else if (openinfo->davbtype == DAVB_IMAGE) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("<show image=\"", -1);
			clientput(openinfo->name, -1);
			clientput("\" window=\"", -1);
			clientput(openinfo2->ident, -1);
			clientput("\"/>", -1);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
			//e1 = (ELEMENT *) clientelement(); /* assume <r> element  NOT USED*/
			clientrelease();
		}
		else guipixshow(openinfo->u.pixhandle, openinfo2->u.winhandle, 0, 0);
	}
	else dbcerrinfo(505, (UCHAR*)"Attempt to show unknown type", -1);
}

/**
 * Added 13 Feb 2014
 * ctlshow() and ctlshowpos() had duplicate code for this.
 * Consolidated it for better and consistent error reporting.
 */
static void showResourceLocal(RESHANDLE reshandle, WINHANDLE winhandle, INT hpos, INT vpos) {
	CHAR *errmsg;
	CHAR work[64];
	int i1 = (INT)guiresshow(reshandle, winhandle, hpos, vpos);
	if (i1) {
		if (i1 == RC_ERROR) {
			if (strlen(errmsg = guiGetLastErrorMessage()) != 0) {
				dbcerrinfo(1698, (UCHAR*)errmsg, -1);
			}
			else sprintf(work, "(%d)", RC_ERROR);
		}
		else sprintf(work, "(%d)", i1);
		dbcerrinfo(799, (UCHAR*)work, -1);
	}
}

static void ctlshowpos()
{
	INT hpos, vpos;
	CHAR work[128], work2[256];
	DAVB *resimgvar;
	DAVB *devicevar;
	OPENINFO *openinfo, *openinfo2;
	ELEMENT *e1;

	resimgvar = getdavb();
	devicevar = getdavb();
	hpos = nvtoi(getvar(VAR_READ)) - 1;
	vpos = nvtoi(getvar(VAR_READ)) - 1;
	if (!resimgvar->refnum) {
		if ((!(dbcflags & DBCFLAG_GUIINIT) && !(dbcflags & DBCFLAG_CLIENTINIT)) ||
			((dbcflags & DBCFLAG_CLIENTINIT) &&	!(dbcflags & DBCFLAG_CLIENTGUI))) {
			dbcerror(564);
		}
		dbcerror(791);
	}
	openinfo = *openinfopptr + resimgvar->refnum - 1;
	openinfo2 = *openinfopptr + devicevar->refnum - 1;
	if (openinfo->davbtype == DAVB_RESOURCE) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			strcpy(work2, "<show ");
			if (openinfo->restype == RES_TYPE_PANEL) strcat(work2, "panel=\"");
			else if (openinfo->restype == RES_TYPE_DIALOG) strcat(work2, "dialog=\"");
			else if (openinfo->restype == RES_TYPE_MENU) strcat(work2, "menu=\"");
			else if (openinfo->restype == RES_TYPE_TOOLBAR) strcat(work2, "toolbar=\"");
			else if (openinfo->restype == RES_TYPE_POPUPMENU) strcat(work2, "popupmenu=\"");
			else dbcerror(799);
			strcat(work2, "%s\" window=\"%s\" h=\"%d\"  v=\"%d\"/>");
			sprintf(work, work2, openinfo->ident, openinfo2->ident, hpos, vpos);
			clientput(work, -1);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
			e1 = (ELEMENT *) clientelement(); /* assume <r> element */
			if (e1 == NULL || e1->firstattribute != NULL) {
				clientrelease();
				dbcerror(799);
			}
			clientrelease();
		}
		else {
			showResourceLocal(openinfo->u.reshandle, openinfo2->u.winhandle, hpos, vpos);
		}
	}
	else if (openinfo->davbtype == DAVB_IMAGE) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			sprintf(work, "<show image=\"%s\" window=\"%s\" h=\"%d\"  v=\"%d\"/>", openinfo->name,
					openinfo2->ident, hpos, vpos);
			clientput(work, -1);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
			//e1 = (ELEMENT *) clientelement(); /* assume <r> element, NOT USED */
			clientrelease();
		}
		else guipixshow(openinfo->u.pixhandle, openinfo2->u.winhandle, hpos, vpos);
	}
	else dbcerrinfo(505, (UCHAR*)"Attempt to show unknown type", -1);
}

static void ctlhide()
{
	DAVB *resimgvar;
	OPENINFO *openinfo;
	ELEMENT *e1;

	resimgvar = getdavb();
	if (!resimgvar->refnum) {
		if ((!(dbcflags & DBCFLAG_GUIINIT) && !(dbcflags & DBCFLAG_CLIENTINIT)) ||
			((dbcflags & DBCFLAG_CLIENTINIT) &&	!(dbcflags & DBCFLAG_CLIENTGUI))) {
			dbcerror(564);
		}
	}
	else {
		openinfo = *openinfopptr + resimgvar->refnum - 1;
		if (openinfo->davbtype == DAVB_RESOURCE) {
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (openinfo->restype == RES_TYPE_PANEL) clientput("<hide panel=\"", -1);
				else if (openinfo->restype == RES_TYPE_DIALOG) clientput("<hide dialog=\"", -1);
				else if (openinfo->restype == RES_TYPE_MENU) clientput("<hide menu=\"", -1);
				else if (openinfo->restype == RES_TYPE_TOOLBAR) clientput("<hide toolbar=\"", -1);
				else if (openinfo->restype == RES_TYPE_POPUPMENU) clientput("<hide popupmenu=\"", -1);
				else return;
				clientput(openinfo->ident, -1);
				clientput("\"/>", -1);
				if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
				e1 = (ELEMENT *) clientelement();
				if (e1 == NULL || strcmp(e1->tag, "r")) {
					clientrelease();
					dbcerror(798);
				}
				clientrelease();
			}
			else guireshide(openinfo->u.reshandle);
		}
		else if (openinfo->davbtype == DAVB_IMAGE) {
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<hide image=\"", -1);
				clientput(openinfo->name, -1);
				clientput("\"/>", -1);
				if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
				clientelement(); /* assume <r> element */
				clientrelease();
			}
			else guipixhide(openinfo->u.pixhandle);
		}
		else dbcerrinfo(505, (UCHAR*)"Attempt to hide unknown type", -1);
	}
}

/**
 * For the Clipboard, we can load either a CHARVAR or an IMAGE
 * For other device types, can only load an IMAGE
 */
static void ctlload()
{
	INT i1, len, type, filehandle, srvhandle;
	CHAR work[8], *bits;
	OFFSET eofpos;
	UCHAR *ptr, **pptr;
	DAVB *imagevar, *devicevar;

	/*
	 * pointer to the OPENINFO structure for the 1st operand, may be an image variable
	 */
	OPENINFO *openinfo = NULL;

	/*
	 * If this is a load of an image, save the one-based index here as memory may move and imagevar may become invalid
	 */
	INT imagerefnum;

	/*
	 * pointer to the OPENINFO structure for the 2nd operand
	 */
	OPENINFO *openinfo2;
	INT devrefnum;
	PIXHANDLE pixhandle;
	ELEMENT *e1;

	i1 = pcount;
	ptr = getvar(VAR_WRITE);
	if (*ptr == 0xAB) {  /* image variable */
		pcount = i1;
		imagevar = getdavb();
		if (!imagevar->refnum) dbcerror(564);
		imagerefnum = imagevar->refnum;
		openinfo = *openinfopptr + imagerefnum - 1;
		if (openinfo->davbtype != DAVB_IMAGE) {
			dbcerrinfo(505, (UCHAR*)"Attempt to load to non-image", -1);
			return;
		}
		imagevar = NULL;
		ptr = NULL;
	}
	else { /* 'loading' text into a char var from device, which can only be clipboard */
		i1 = hl;
		len = pl;
	}

	devicevar = getdavb();
	if (!devicevar->refnum) {
		dbcerror(791);
		return;
	}
	devrefnum = devicevar->refnum;
	openinfo2 = *openinfopptr + devrefnum - 1;
	if (openinfo2->davbtype != DAVB_DEVICE) {
		dbcerrinfo(505, (UCHAR*)"Attempt to load fm non-device", -1);
		return;
	}
	switch(openinfo2->devtype) {
	case DEV_DSR:
		if (openinfo == NULL) dbcerrinfo(792, (UCHAR*)"Attempt to load a CHARVAR from a DSR", -1);
		dsrload(openinfo2->u.inthandle, (void *) openinfo->u.pixhandle);
		break;
	case DEV_CLIPBOARD:
		if (ptr != NULL) {/* 'loading' text into a char var from clipboard */
			fp = 1;
			lp = len;
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<getclipboard text=\"0\"/>", -1);
				if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
				e1 = (ELEMENT *) clientelement();
				if (e1 == NULL || strcmp(e1->tag, "r")) {
					clientrelease();
					dbcerror(798);
				}
				if (e1->firstsubelement != NULL) {
					len = (INT)strlen(e1->firstsubelement->tag);
					if (len < lp) lp = len;
					memcpy(ptr + i1, e1->firstsubelement->tag, (UINT) lp);
				}
				clientrelease();
			}
			else guicbdgettext((CHAR*)(ptr + i1), (size_t*)&lp);
			setfplp(ptr);
		}
		else {/* 'loading' an image from clipboard into an image var */
			assert(openinfo != NULL);
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<getclipboard image=\"", -1);
				clientput(openinfo->name, -1);
				clientput("\"/>", -1);
				if (clientsend(0, 0) < 0) dbcerrinfo(798, NULL, 0);
			}
			else guicbdgetpixmap(openinfo->u.pixhandle);
		}
		break;
	case DEV_TIFFILE:
	case DEV_GIFFILE:
	case DEV_JPGFILE:
	case DEV_PNGFILE:
		if (openinfo == NULL) dbcerrinfo(792, (UCHAR*)"Attempt to load a CHARVAR from an image file", -1);
		filehandle = openinfo2->u.inthandle;
		srvhandle = openinfo2->v.srvhandle;
		pixhandle = openinfo->u.pixhandle;
		type = openinfo2->devtype;
		if (srvhandle > 0) {
			i1 = fsfilesize(filehandle, &eofpos);
			if (i1) goto loaderr;
			len = (INT) eofpos;
			if (len < 100) goto loaderr;
			pptr = memalloc(len, 0);
			setpgmdata();
			if (pptr == NULL) dbcerror(1667);
			openinfo = *openinfopptr + imagerefnum - 1;
			openinfo2 = *openinfopptr + devrefnum - 1;
			i1 = fsreadraw(filehandle, 0, (CHAR *) *pptr, len);
		}
		else {
			fiogetsize(filehandle, &eofpos);
			len = (INT) eofpos;
			if (len < 49) goto loaderr;
			/* The next call MAY MOVE MEMORY! */
			pptr = memalloc(len, 0);
			setpgmdata();
			if (pptr == NULL) dbcerror(1667);
			/* Do these two things in case the memalloc moved memory */
			openinfo = *openinfopptr + imagerefnum - 1;
			openinfo2 = *openinfopptr + devrefnum - 1;
			i1 = fioread(filehandle, 0, *pptr, len);
		}

		if (i1 == len) {
			if (type == DEV_TIFFILE) {
				(*pixhandle)->tifnum = openinfo2->tifnum;
				i1 = guitiftopix(pixhandle, pptr);
			}
			else if (type == DEV_GIFFILE) i1 = guigiftopix(pixhandle, pptr);
			else if (type == DEV_PNGFILE) {
				i1 = guipngtopix(pixhandle, pptr);
			}
			else {
				i1 = guijpgtopix(pixhandle, pptr);
			}
			/* Do these things in case the memalloc (in the image decoding routines) moved memory */
			setpgmdata();
			if (!i1) {
				openinfo = *openinfopptr + imagerefnum - 1;
				//openinfo2 = *openinfopptr + devrefnum - 1;  NOT USED
			}
		}
		else i1 = RC_ERROR;
		memfree(pptr);
		if (i1 < 0) goto loaderr;

		if (dbcflags & DBCFLAG_CLIENTINIT) {
			if (!prpget("client", "imageuse", NULL, NULL, (CHAR**)&ptr, PRP_LOWER)
					&& !strcmp((CHAR*)ptr, "serveronly")) {
				break;
			}

			bits = encodeimage(pixhandle, &i1);
			if (bits == NULL) dbcerrinfo(1667, (UCHAR*)"encodeimage", 11); /* not enough memory */
			else {
				clientput("<storeimagebits image=\"", -1);
				clientput(openinfo->name, -1);
				clientput("\" bpp=\"", -1);
				mscitoa((INT) (*pixhandle)->bpp, work);
				clientput(work, -1);
				clientput("\" h=\"", -1);
				mscitoa((*pixhandle)->hsize, work);
				clientput(work, -1);
				clientput("\" v=\"", -1);
				mscitoa((*pixhandle)->vsize, work);
				clientput(work, -1);
				clientput("\"", 1);
#if OS_WIN32
				if ((*pixhandle)->isGrayScaleJpeg) clientput(" usegrayscalepalette=\"yes\"", -1);
#endif
				clientput(" bits=\"", -1);
				clientput(bits, i1);
				clientput("\"", -1);
				clientput("/>", -1);
				if (clientsend(0, 0) < 0 || i1 == 0) {
					if (i1) free(bits);
					dbcerrinfo(798, NULL, 0);
				}
				else free(bits);
			}
		}
		break;
	default:
loaderr:
		if (i1 == RC_NO_MEM) dbcerror(1667);
		else dbcerror(792);
	}
}

static void ctlstore()
{
	INT i1, i2 = 0, filehandle, len, srvhandle;
	UCHAR *ptr, **pptr;
	DAVB *imagevar, *devicevar;
	OPENINFO *openinfo = NULL, *openinfo2;
	PIXHANDLE pixhandle;
	ELEMENT *e1;
	ATTRIBUTE *a1;

	i1 = pcount;
	ptr = getvar(VAR_READ);
	if (*ptr == 0xAB) {  /* image variable */
		pcount = i1;
		imagevar = getdavb();
		if (!imagevar->refnum) dbcerror(564);
		openinfo = *openinfopptr + imagevar->refnum - 1;
		ptr = NULL;
	}
	else {
		ptr += hl;
		len = lp;
	}
	devicevar = getdavb();
	openinfo2 = *openinfopptr + devicevar->refnum - 1;
	if (!devicevar->refnum) {
		dbcerror(791);
		return;
	}
	if (openinfo2->davbtype != DAVB_DEVICE) {
		dbcerrinfo(505, (UCHAR*)"Attempt to store to non-device", -1);
		return;
	}
	switch(openinfo2->devtype) {
	case DEV_DSR:
		if (openinfo == NULL) dbcerrinfo(792, (UCHAR*)"Attempt to store a CHARVAR to a DSR", -1);
		dsrstore(openinfo2->u.inthandle, (void *) openinfo->u.pixhandle);
		break;
	case DEV_TIFFILE:
		assert(openinfo != NULL);
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("<getimagebits image=\"", -1);
			clientput(openinfo->name, -1);
			clientput("\"/>", -1);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
			e1 = (ELEMENT *) clientelement();
			if (e1 == NULL || strcmp(e1->tag, "image")) {
				clientrelease();
				dbcerror(798);
			}
			a1 = e1->firstattribute;
			/* this XML element must have the order: image, bpp, then bits */
			if (a1 == NULL) {
				clientrelease();
				dbcerrinfo(798, NULL, 0);
			}
			while (a1 != NULL) {
				if (strcmp(a1->tag, "image") == 0) {
					if (strcmp(openinfo->name, a1->value)) {
						clientrelease();
						dbcerrinfo(798, NULL, 0);
					}
				}
				else if (strcmp(a1->tag, "bpp") == 0) {
					//i1 = atoi(a1->value);  Not Used !!
				}
				else if (strcmp(a1->tag, "bits") == 0) {
					if (decodeimage(openinfo->u.pixhandle, (UCHAR *) a1->value,
							(INT) strlen(a1->value)) == RC_NO_MEM)
					{
						dbcerrinfo(1667, (UCHAR*)"decodeimage", 11);
					}
				}
				a1 = a1->nextattribute;
			}
			clientrelease();
		}
		pixhandle = openinfo->u.pixhandle;
		filehandle = openinfo2->u.inthandle;
		srvhandle = openinfo2->v.srvhandle;

		len = imgstorebufsize; /* default is 12MB */
		do {
			pptr = memalloc(len, 0);
			if (pptr != NULL) break;
			len >>= 1;
			if (len < 2048) goto storeerr; /* out of memory */
		}
		while (TRUE);

		setpgmdata();
		if (pptr == NULL) goto storeerr;
		i1 = guipixtotif(pixhandle, *pptr, &len);
		if (i1) {
			/*CODE Detect and report an out-of-memory error here!*/
			memfree(pptr);
			if (i1 == RC_ERROR || i1 == RC_INVALID_HANDLE) goto storeerr;
			if (i1 == RC_NO_MEM) {
				dbcerrinfo(1667, (UCHAR*)"guipixtotif", 11);
				return;
			}
		}
		else {
			if (srvhandle > 0) {
				i2 = fswriteraw(filehandle, 0, (CHAR *) *pptr, len);
			}
			else i2 = fiowrite(filehandle, 0, *pptr, len);
		}
		memfree(pptr);
		if (i2 < 0) goto storeerr;
		break;
	case DEV_CLIPBOARD:
		if (ptr != NULL) {
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<setclipboard text=\"", -1);
				clientputdata((CHAR *) ptr, len);
				clientput("\"/>", -1);
				if (clientsend(0, 0) < 0) dbcerrinfo(798, NULL, 0);
			}
			else guicbdputtext(ptr, len);
		}
		else {
			assert(openinfo != NULL);
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<setclipboard image=\"", -1);
				clientput(openinfo->name, -1);
				clientput("\"/>", -1);
				if (clientsend(0, 0) < 0) dbcerrinfo(798, NULL, 0);
			}
			else guicbdputpixmap(openinfo->u.pixhandle);
		}
		break;
	default:
		goto storeerr;
	}
	return;

storeerr:
	dbcerror(792);
	return;
}

/**
 * Note: For SC use, coordinates are ALWAYS sent ONE-BASED over the wire
 * Sizes, of course, are sent as-is
 */
static void ctldraw()
{
	INT h1, v1, h2, v2, lp1;
	INT32 i1, i2;
	UCHAR *ptr;
	CHAR work[128];
	DAVB *imagevar;
	OPENINFO *openinfo;
	PIXHANDLE pixhandle;

	imagevar = getdavb();
	if (!imagevar->refnum) dbcerror(564);
	openinfo = *openinfopptr + imagevar->refnum - 1;
	pixhandle = openinfo->u.pixhandle;
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		clientput("<draw image=\"", -1);
		clientput(openinfo->name, -1);
		clientput("\">", -1);
	}
	else guipixdrawstart(pixhandle);
	while(pgm[pcount] != 0xFF) {
		switch(pgm[pcount++]) {
		case 2:  /* NEWLINE */
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<newline/>", -1);
			else {
				guipixgetpos(pixhandle, &h1, &v1);
				guipixgetfontsize(pixhandle, &h1, &v2);
				guipixdrawpos(0, v1 + h1);
			}
			break;
		case 3:  /* LINEFEED */
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<linefeed/>", -1);
			else {
				guipixgetpos(pixhandle, &h1, &v1);
				guipixgetfontsize(pixhandle, &h1, &v2);
				guipixdrawvpos(v1 + h1);
			}
			break;
		case 4:  /* ERASE */
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<erase/>", -1);
			else guipixdrawerase();
			break;
		case 5:  /* DOT */
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<dot/>", -1);
			else guipixdrawdot(0);
			break;
		case 8:  /* LINETYPE=*SOLID */
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<brush t=\"solid\"/>", -1);
			else guipixdrawlinetype(DRAW_LINE_SOLID);
			break;
		case 9:  /* LINETYPE=*DOT */
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				/* not supported in gui client */
			}
			else guipixdrawlinetype(DRAW_LINE_DOT);
			break;
		case 10:  /* LINETYPE=*DASH */
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				/* not supported in gui client */
			}
			else guipixdrawlinetype(DRAW_LINE_DASH);
			break;
		case 11:  /* LINETYPE=*DASHDOT */
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				/* not supported in gui client */
			}
			else guipixdrawlinetype(DRAW_LINE_DASHDOT);
			break;
		case 12:  /* LINETYPE=*INVERT */
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				/* not supported in gui client */
			}
			else guipixdrawlinetype(DRAW_LINE_INVERT);
			break;
		case 13:  /* LINETYPE=*REVDOT */
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<brush t=\"revdot\"/>", -1);
			else guipixdrawlinetype(DRAW_LINE_REVDOT);
			break;
		case 16:  /* COLOR=*BLACK */
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<color r=\"0\" g=\"0\" b=\"0\"/>", -1);
			else guipixdrawcolor(0, 0, 0);
			break;
		case 17:  /* COLOR=*RED */
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<color r=\"255\" g=\"0\" b=\"0\"/>", -1);
			else guipixdrawcolor(0xFF, 0, 0);
			break;
		case 18:  /* COLOR=*GREEN */
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<color r=\"0\" g=\"255\" b=\"0\"/>", -1);
			else guipixdrawcolor(0, 0xFF, 0);
			break;
		case 19:  /* COLOR=*YELLOW */
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<color r=\"255\" g=\"255\" b=\"0\"/>", -1);
			else guipixdrawcolor(0xFF, 0xFF, 0);
			break;
		case 20:  /* COLOR=*BLUE */
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<color r=\"0\" g=\"0\" b=\"255\"/>", -1);
			else guipixdrawcolor(0, 0, 0xFF);
			break;
		case 21:  /* COLOR=*MAGENTA */
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<color r=\"255\" g=\"0\" b=\"255\"/>", -1);
			else guipixdrawcolor(0xFF, 0, 0xFF);
			break;
		case 22:  /* COLOR=*CYAN */
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<color r=\"0\" g=\"255\" b=\"255\"/>", -1);
			else guipixdrawcolor(0, 0xFF, 0xFF);
			break;
		case 23:  /* COLOR=*WHITE */
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<color r=\"255\" g=\"255\" b=\"255\"/>", -1);
			else guipixdrawcolor(0xFF, 0xFF, 0xFF);
			break;
		case 24:  /* COLOR=<nvar> */
			i1 = getdrawvalue();
			INT iRed, iBlue;
			INT iGreen = (i1 & 0x00FF00) >> 8;
			if (formatFor24BitColorIsRGB) {
				iRed = (i1 & 0xFF0000) >> 16;
				iBlue = i1 & 0x0000FF;
			}
			else {
				iBlue = (i1 & 0xFF0000) >> 16;
				iRed = i1 & 0x0000FF;
			}
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<color r=\"%d\" g=\"%d\" b=\"%d\"/>", iRed, iGreen, iBlue);
				clientput(work, -1);
			}
			else guipixdrawcolor(iRed, iGreen, iBlue);
			break;
		case 25:  /* LINEWIDTH=<nvar> */
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<brush w=\"%d\"/>", (INT) getdrawvalue());
				clientput(work, -1);
			}
			else guipixdrawlinewidth(getdrawvalue());
			break;
		case 26:  /* CIRCLE=<nvar> */
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<circle r=\"%d\"/>", (INT) getdrawvalue());
				clientput(work, -1);
			}
			else guipixdrawcircle((INT) getdrawvalue());
			break;
		case 27:  /* BIGDOT=<nvar> */
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<bigdot s=\"%d\"/>", (INT) getdrawvalue());
				clientput(work, -1);
			}
			else guipixdrawdot((INT) getdrawvalue());
			break;
		case 28:  /* H=<nvar> */
			h1 = getdrawvalue();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<p h=\"%d\"/>", h1);
				clientput(work, -1);
			}
			else guipixdrawhpos(h1 - 1);
			break;
		case 29:  /* V=<nvar> */
			v1 = getdrawvalue();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<p v=\"%d\"/>", v1);
				clientput(work, -1);
			}
			else guipixdrawvpos(v1 - 1);
			break;
		case 30:  /* HA=<nvar> */
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<p ha=\"%d\"/>", (INT) getdrawvalue());
				clientput(work, -1);
			}
			else guipixdrawhadj((INT) getdrawvalue());
			break;
		case 31:  /* VA=<nvar> */
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<p va=\"%d\"/>", (INT) getdrawvalue());
				clientput(work, -1);
			}
			else guipixdrawvadj((INT) getdrawvalue());
			break;
		case 32:  /* WIPE=<cvarlit> */
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				/* no support in gui client */
			}
			getvar(VAR_READ);
			break;
		case 33:  /* FONT=<cvarlit> */
			ptr = getvar(VAR_READ);
			if (!fp) lp = 0;
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (lp >= (INT) sizeof(work)) lp = sizeof(work) - 1;
				memcpy(work, ptr + hl, lp);
				work[lp] = '\0';
				if (guiservergenfont(work, TRUE) <= 0) dbcerror(799);
			}
			else guipixdrawfont(ptr + hl, lp);
			break;
		case 34:  /* TEXT=<cvarlit> */
			ptr = getvar(VAR_READ);
			if (!fp) lp = 0;
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<text t=\"", -1);
				clientputdata((CHAR *) (ptr + hl), lp);
				clientput("\"/>", -1);
			}
			else guipixdrawtext(ptr + hl, lp, DRAW_TEXT_LEFT);
			break;
		case 35:  /* IMAGE=<imagevar> */
			imagevar = getdavb();
			openinfo = *openinfopptr + imagevar->refnum - 1;
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<dimage image=\"%s\"/>", openinfo->name);
				clientput(work, -1);
			}
			else guipixdrawcopyimage(openinfo->u.pixhandle);
			break;
		case 36:  /* P=<nvar>:<nvar> */
			h1 = getdrawvalue();
			v1 = getdrawvalue();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<p h=\"%d\" v=\"%d\"/>", h1, v1);
				clientput(work, -1);
			}
			else guipixdrawpos(h1 - 1, v1 - 1);
			break;
		case 37:  /* LINE=<nvar>:<nvar> */
			h1 = getdrawvalue();
			v1 = getdrawvalue();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<line h=\"%d\" v=\"%d\"/>", h1, v1);
				clientput(work, -1);
			}
			else guipixdrawline(h1 - 1, v1 - 1);
			break;
		case 38:  /* BOX=<nvar>:<nvar> */
			h1 = getdrawvalue();
			v1 = getdrawvalue();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<dbox h=\"%d\" v=\"%d\"/>", h1, v1);
				clientput(work, -1);
			}
			else guipixdrawbox(h1 - 1, v1 - 1);
			break;
		case 39:  /* RECTANGLE=<nvar>:<nvar> */
			h1 = getdrawvalue();
			v1 = getdrawvalue();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<rectangle h=\"%d\" v=\"%d\"/>", h1, v1);
				clientput(work, -1);
			}
			else guipixdrawrectangle(h1 - 1, v1 - 1);
			break;
		case 40:  /* TRIANGLE=<nvar>:<nvar>:<nvar>:<nvar> */
			h1 = getdrawvalue();
			v1 = getdrawvalue();
			h2 = getdrawvalue();
			v2 = getdrawvalue();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<triangle h1=\"%d\" v1=\"%d\" h2=\"%d\" v2=\"%d\"/>", h1, v1, h2, v2);
				clientput(work, -1);
			}
			else guipixdrawtriangle(h1 - 1, v1 - 1, h2 - 1, v2 - 1);
			break;
  		case 41:  /* CLIPIMAGE=<imagevar>:<nvar>:<nvar>:<nvar>:<nvar> */
			imagevar = getdavb();
			openinfo = *openinfopptr + imagevar->refnum - 1;
			// Values are coordinates and so are subject to the one-based, zero-based thing
			h1 = getdrawvalue();
			v1 = getdrawvalue();
			h2 = getdrawvalue();
			v2 = getdrawvalue();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<clipimage image=\"%s\" h1=\"%d\" v1=\"%d\" h2=\"%d\" v2=\"%d\"/>",
						openinfo->name, h1, v1, h2, v2);
				clientput(work, -1);
			}
			else guipixdrawclipimage(openinfo->u.pixhandle, h1 - 1, v1 - 1, h2 - 1, v2 - 1);
			break;
		case 42:  /* STRETCHIMAGE=<imagevar>:<nvar>:<nvar> */
			imagevar = getdavb();
			openinfo = *openinfopptr + imagevar->refnum - 1;
			// Values are a size and so are NOT subject to the one-based, zero-based thing
			h1 = getdrawvalue();
			v1 = getdrawvalue();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<stretchimage image=\"%s\" h1=\"%d\" v1=\"%d\"/>",
						openinfo->name, h1, v1);
				clientput(work, -1);
			}
			else guipixdrawstretchimage(openinfo->u.pixhandle, h1, v1);
			break;
		case 43:  /* CTEXT=<cvarlit>:<nvarlit> */
			ptr = getvar(VAR_READ);
			ptr += hl;
			lp1 = lp;
			if (!fp) lp1 = lp = 0;
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<ctext t=\"", -1);
				clientputdata((CHAR *) ptr, lp1);
				clientput("\" hsize=\"", -1);
				mscitoa((INT) getdrawvalue(), work);
				clientput(work, -1);
				clientput("\"/>", -1);
			}
			else {
				guipixgetpos(pixhandle, &h1, &v1);
				h2 = getdrawvalue();
				h2 /= 2;
				guipixdrawhadj(h2);
				guipixdrawtext(ptr, lp1, DRAW_TEXT_CENTER);
				guipixdrawpos(h1, v1);
			}
			break;
		case 44:  /* RTEXT=<cvarlit> */
			ptr = getvar(VAR_READ);
			ptr += hl;
			lp1 = lp;
			if (!fp) lp1 = lp = 0;
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<rtext t=\"", -1);
				clientputdata((CHAR *) ptr, lp1);
				clientput("\"/>", -1);
			}
			else {
				guipixdrawtext(ptr, lp1, DRAW_TEXT_RIGHT);
			}
			break;

			/**
			 * 2nd parm is the angle, in degrees, at which the text is
			 * drawn.
			 *
			 * Version 16 and earlier interpretation:
			 * The angle is the same as for a compass, that is north (up) is zero degrees, east (normal) is 90
			 * degrees, south (down) is 180 degrees, and west (upside down) is 270 degrees.
			 *
			 * Version 17 and newer interpretation:
			 * Zero is normal. A negative number rotates counter-clockwise. Positive rotates clockwise.
			 */
		case 45:  /* ATEXT=<cvarlit>:<nvarlit> */
			ptr = getvar(VAR_READ);
			ptr += hl;
			lp1 = lp;
			if (!fp) lp1 = lp = 0;
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<atext t=\"", -1);
				clientputdata((CHAR *) ptr, lp1);
				clientput("\" n=\"", -1);
				mscitoa((INT) getdrawvalue(), work);
				clientput(work, -1);
				clientput("\"/>", -1);
			}
			else {
				guipixdrawatext(ptr, lp1, (INT) getdrawvalue());
			}
			break;
		case 46:  /* IMAGEROTATE=<nvarlit> */
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<irotate n=\"", -1);
				mscitoa((INT) getdrawvalue(), work);
				clientput(work, -1);
				clientput("\"/>", -1);
			}
			else {
				guipixdrawirotate((INT) getdrawvalue());
			}
			break;
		case 47: /* REPLACE=<*color>:<*color> */
			switch(pgm[pcount++]) {
				case 0: i1 = 0x000000; break; /* black   */
				case 1: i1 = 0x0000FF; break; /* red     */
				case 2: i1 = 0x00FF00; break; /* green   */
				case 3: i1 = 0x00FFFF; break; /* yellow  */
				case 4: i1 = 0xFF0000; break; /* blue    */
				case 5: i1 = 0xFF00FF; break; /* magenta */
				case 6: i1 = 0xFFFF00; break; /* cyan    */
				case 7: i1 = 0xFFFFFF; break; /* white   */
			}
			switch(pgm[pcount++]) {
				case 0: i2 = 0x000000; break; /* black   */
				case 1: i2 = 0x0000FF; break; /* red     */
				case 2: i2 = 0x00FF00; break; /* green   */
				case 3: i2 = 0x00FFFF; break; /* yellow  */
				case 4: i2 = 0xFF0000; break; /* blue    */
				case 5: i2 = 0xFF00FF; break; /* magenta */
				case 6: i2 = 0xFFFF00; break; /* cyan    */
				case 7: i2 = 0xFFFFFF; break; /* white   */
			}
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<replace r=\"%d\" g=\"%d\" b=\"%d\" r=\"%d\" g=\"%d\" b=\"%d\"/>",
						i1 & 0x0000FF, (i1 & 0x00FF00) >> 8, (UINT)(i1 & 0xFF0000) >> 16,
						i2 & 0x0000FF, (i2 & 0x00FF00) >> 8, (UINT)(i2 & 0xFF0000) >> 16);
				clientput(work, -1);
			}
			else {
				guipixdrawreplace(i1, i2);
			}
			break;
		case 48: /* REPLACE <*color>:<nvar> */
			switch(pgm[pcount++]) {
				case 0: i1 = 0x000000; break; /* black   */
				case 1: i1 = 0x0000FF; break; /* red     */
				case 2: i1 = 0x00FF00; break; /* green   */
				case 3: i1 = 0x00FFFF; break; /* yellow  */
				case 4: i1 = 0xFF0000; break; /* blue    */
				case 5: i1 = 0xFF00FF; break; /* magenta */
				case 6: i1 = 0xFFFF00; break; /* cyan    */
				case 7: i1 = 0xFFFFFF; break; /* white   */
			}
			i2 = getdrawvalue();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<replace r=\"%d\" g=\"%d\" b=\"%d\" r=\"%d\" g=\"%d\" b=\"%d\"/>",
						i1 & 0x0000FF, (i1 & 0x00FF00) >> 8, (UINT)(i1 & 0xFF0000) >> 16,
						i2 & 0x0000FF, (i2 & 0x00FF00) >> 8, (UINT)(i2 & 0xFF0000) >> 16);
				clientput(work, -1);
			}
			else {
				guipixdrawreplace(i1, i2);
			}
			break;
		case 49: /* REPLACE <nvar>:<*color> */
			i1 = getdrawvalue();
			switch(pgm[pcount++]) {
				case 0: i2 = 0x000000; break; /* black   */
				case 1: i2 = 0x0000FF; break; /* red     */
				case 2: i2 = 0x00FF00; break; /* green   */
				case 3: i2 = 0x00FFFF; break; /* yellow  */
				case 4: i2 = 0xFF0000; break; /* blue    */
				case 5: i2 = 0xFF00FF; break; /* magenta */
				case 6: i2 = 0xFFFF00; break; /* cyan    */
				case 7: i2 = 0xFFFFFF; break; /* white   */
			}
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<replace r=\"%d\" g=\"%d\" b=\"%d\" r=\"%d\" g=\"%d\" b=\"%d\"/>",
						i1 & 0x0000FF, (i1 & 0x00FF00) >> 8, (UINT)(i1 & 0xFF0000) >> 16,
						i2 & 0x0000FF, (i2 & 0x00FF00) >> 8, (UINT)(i2 & 0xFF0000) >> 16);
				clientput(work, -1);
			}
			else {
				guipixdrawreplace(i1, i2);
			}
			break;
		case 50: /* REPLACE <nvar>:<nvar> */
			i1 = getdrawvalue();
			i2 = getdrawvalue();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<replace r=\"%d\" g=\"%d\" b=\"%d\" r=\"%d\" g=\"%d\" b=\"%d\"/>",
						i1 & 0x0000FF, (i1 & 0x00FF00) >> 8, (UINT)(i1 & 0xFF0000) >> 16,
						i2 & 0x0000FF, (i2 & 0x00FF00) >> 8, (UINT)(i2 & 0xFF0000) >> 16);
				clientput(work, -1);
			}
			else {
				guipixdrawreplace(i1, i2);
			}
			break;
		default:
			dbcerrinfo(505, (UCHAR*)"Bad opcode seen in draw", -1);  /* invalid control code */
		}
	}
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		clientput("</draw>", -1);
		if (clientsend(0, 0) < 0) dbcerrinfo(798, NULL, 0);
	}
	else guipixdrawend();
}

static void devicecbfnc(WINHANDLE winhandle, UCHAR *data, INT datalen)
{
	INT i1;
	OPENINFO *openinfo;

	openinfo = *openinfopptr;
	for (i1 = 0; i1 < openinfohi; i1++, openinfo++) {
		if (openinfo->davbtype == DAVB_DEVICE && openinfo->devtype == DEV_WINDOW && openinfo->u.winhandle == winhandle) {
			if (openinfo->v.linkquehandle != NULL) {
				if (!guilock) guisuspend();
				queput(openinfo->v.linkquehandle, data, datalen);
				if (dbcflags & DBCFLAG_DEBUG) guiresume();
			}
			break;
		}
	}
}

static void resourcecbfnc(RESHANDLE reshandle, UCHAR *data, INT datalen)
{
	INT i1;
	OPENINFO *openinfo;

	openinfo = *openinfopptr;
	for (i1 = 0; i1 < openinfohi; i1++, openinfo++) {
		if (openinfo->davbtype == DAVB_RESOURCE && openinfo->u.reshandle == reshandle) {
			if (openinfo->v.linkquehandle != NULL) {
				if (!guilock) guisuspend();
				queput(openinfo->v.linkquehandle, data, datalen);
				if (dbcflags & DBCFLAG_DEBUG) guiresume();
			}
			break;
		}
	}
}

#if OS_WIN32
static void ddecbfnc(INT handle, UCHAR *data, INT datalen)
{
	INT i1;
	OPENINFO *openinfo;

	openinfo = *openinfopptr;
	for (i1 = 0; i1 < openinfohi; i1++, openinfo++) {
		if (openinfo->davbtype == DAVB_DEVICE && openinfo->devtype == DEV_DDE && openinfo->u.inthandle == handle) {
			if (openinfo->v.linkquehandle != NULL) queput(openinfo->v.linkquehandle, data, datalen);
			break;
		}
	}
}
#endif


/**
 * Parameters:
 * reshandle - a double pointer to a RESOURCE structure
 * str       - a pointer to a string defining the resource
 * len       - the length of str
 * openinfo  - a single pointer to an OPENINFO struct. This is in local memory
 *             in this module so no need to worry about it moving.
 */
static void makeres(RESHANDLE reshandle, UCHAR *str, INT len, OPENINFO *openinfo)
{
	INT keywordval, resnamelen;
	INT i1, i2, h1, v1;
	UINT trueiconsize;
	UCHAR resname[8], textmask[130], *pixelptr;
	CHAR work[256];
	ELEMENT *e1;

	scaninit(str, len);
	scanname();
	keywordval = nametokeywordvalue();
	scanequal();
	scanname();
	resnamelen = scanstringlen;
	if (resnamelen > MAXRESNAMELEN) resnamelen = MAXRESNAMELEN;
	memset(resname, ' ', MAXRESNAMELEN);
	memcpy(resname, scanstring, resnamelen);

	/**
	 * If we are using SC, we mangle the names of the resources. Users are dumb and they use the
	 * same name for multiple resources. This can be Ok in a db/c program if handled properly.
	 * But with SC we lookup things by their name, not their handle.
	 * So we generate a unique name for internal use.
	 *
	 * Except! We do not do this for icons.
	 */
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		memcpy(openinfo->name, resname, MAXRESNAMELEN);
		i1 = MAXRESNAMELEN;
		do openinfo->name[i1--] = '\0';
		while (i1 > 0 && openinfo->name[i1] == ' ');
		strcpy(openinfo->ident, openinfo->name);
		if (keywordval != KEYWORD_ICON) getuniqueid(openinfo->ident);
	}
	if (keywordval == KEYWORD_MENU || keywordval == KEYWORD_POPUPMENU) {
		makeresmenu(reshandle, resname, keywordval, openinfo);
		return;
	}

	if (keywordval == KEYWORD_ICON) {
		h1 = v1 = i1 = i2 = -1;
		pixelptr = NULL;
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("<prep icon=\"", -1);
			clientput(openinfo->ident, -1);
			clientput("\"", 1);
		}
		else if (guiiconstart(reshandle, resname, MAXRESNAMELEN)) /* Sets up default values in RESOURCE struct */ {
			goto makereserror1;
		}
		while (scancomma()) {
			scanname();
			keywordval = nametokeywordvalue();
			scanequal();
			switch(keywordval) {
			case KEYWORD_H:  /* <num> */
				if (h1 != -1) goto makereserror2;
				h1 = scannumber();
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					sprintf(work, " h=\"%d\"", h1);
					clientput(work, -1);
				}
				else if (guiiconhsize(h1)) goto makereserror2;
				break;
			case KEYWORD_V:  /* <num> */
				if (v1 != -1) goto makereserror2;
				v1 = scannumber();
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					sprintf(work, " v=\"%d\"", v1);
					clientput(work, -1);
				}
				else if (guiiconvsize(v1)) goto makereserror2;
				break;
			case KEYWORD_COLORBITS:  /* <num> */
				if (i1 != -1) goto makereserror2;
				i1 = scannumber();
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					sprintf(work, " colorbits=\"%d\"", i1);
					clientput(work, -1);
				}
				else if (guiiconcolorbits(i1)) goto makereserror2;
				break;
			case KEYWORD_PIXELS:  /* <text> */
				if (pixelptr != NULL) goto makereserror2;
				pixelptr = (UCHAR *) scanstart;
				scantext();
				/* see if pixel is right size */
				if (h1 == -1) h1 = 16;
				if (v1 == -1) v1 = 16;
				if (i1 == -1) i1 = 1;
				if (h1 && v1 && i1) {
					trueiconsize = h1 * v1;
					if (i1 == 24) trueiconsize *= 6;
					else if (i1 != 4 && i1 != 1) goto makereserror1;
					if (scanstringlen != trueiconsize)  goto makereserror1;
				}
				else goto makereserror1;
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					clientput(" pixels=\"", -1);
					i1 = (INT) pixelptr[scanstringlen];
					clientput((CHAR *) pixelptr, scanstringlen);
					clientput("\"", 1);
					pixelptr[scanstringlen] = (CHAR) i1; /* must restore character */
				}
				else if (guiiconpixels(pixelptr, scanstringlen)) goto makereserror2;
				break;
			default:
				goto makereserror2;
			}
		}
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("/>", -1);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
			e1 = (ELEMENT *) clientelement(); /* assume <r> element */
			if (e1 == NULL || e1->firstattribute != NULL) {
				clientrelease();
				goto makereserror1;
			}
			openinfo->restype = RES_TYPE_ICON;
			clientrelease();
		}
		else if (guiiconend()) goto makereserror1;
		return;
	}

	if (keywordval == KEYWORD_OPENFILEDLG || keywordval == KEYWORD_SAVEASFILEDLG || keywordval == KEYWORD_OPENDIRDLG) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			switch (keywordval) {
			case KEYWORD_OPENFILEDLG:
				clientput("<prep openfdlg=\"", -1);
				openinfo->restype = RES_TYPE_OPENFILEDLG;
				break;
			case KEYWORD_SAVEASFILEDLG:
				clientput("<prep saveasfdlg=\"", -1);
				openinfo->restype = RES_TYPE_SAVEASFILEDLG;
				break;
			case KEYWORD_OPENDIRDLG:
				clientput("<prep openddlg=\"", -1);
				openinfo->restype = RES_TYPE_OPENDIRDLG;
				break;
			}
			clientput(openinfo->ident, -1);
			clientput("\"", 1);
		}
		else {
			switch (keywordval) {
			case KEYWORD_OPENFILEDLG:
				if (guiopenfiledlgstart(reshandle, resname, MAXRESNAMELEN)) goto makereserror1;
				break;
			case KEYWORD_SAVEASFILEDLG:
				if (guisaveasfiledlgstart(reshandle, resname, MAXRESNAMELEN)) goto makereserror1;
				break;
			case KEYWORD_OPENDIRDLG:
				if (guiopendirdlgstart(reshandle, resname, MAXRESNAMELEN)) goto makereserror1;
				break;
			}
		}

		while (scancomma()) {
			scanname();
			keywordval = nametokeywordvalue();
			switch(keywordval) {

			/*
			 * We accept and silently ignore the multiple keyword if this is not an openFileDlg
			 */
			case KEYWORD_MULTIPLE: /* For openFileDlg only, multiple file select */
				if (openinfo->restype == RES_TYPE_OPENFILEDLG || (*reshandle)->restype == RES_TYPE_OPENFILEDLG) {
					if (dbcflags & DBCFLAG_CLIENTINIT) {
						clientput(" multiple=\"Y\"", -1);
					}
					else {
						if (guifiledlgMulti()) goto makereserror2;
					}
				}
				break;

			case KEYWORD_DEFAULT: /* The file name or directory to start the dialog on */
				scanequal();
				scantext();
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					clientput(" default=\"", -1);
					clientputdata(scanstring, scanstringlen);
					clientput("\"", 1);
				}
				else {
					if (guifiledlgdefault((UCHAR *) scanstring, scanstringlen)) goto makereserror2;
				}
				break;

			/*
			 * We accept and silently ignore the devicefilter keyword if this is not an openDirDlg
			 */
			case KEYWORD_DEVICEFILTER:
				scanequal();
				scantext();
				if (openinfo->restype == RES_TYPE_OPENDIRDLG || (*reshandle)->restype == RES_TYPE_OPENDIRDLG) {
					if (dbcflags & DBCFLAG_CLIENTINIT) {
						clientput(" devicefilter=\"", -1);
						clientputdata(scanstring, scanstringlen);
						clientput("\"", 1);
					}
					else {
						if (guifiledlgDeviceFilter((UCHAR *) scanstring, scanstringlen)) goto makereserror2;
					}
				}
				break;

			/*
			 * We accept and silently ignore the title keyword if this is not an openDirDlg
			 */
			case KEYWORD_TITLE:
				scanequal();
				scantext();
				if (openinfo->restype == RES_TYPE_OPENDIRDLG || (*reshandle)->restype == RES_TYPE_OPENDIRDLG) {
					if (dbcflags & DBCFLAG_CLIENTINIT) {
						clientput(" title=\"", -1);
						clientputdata(scanstring, scanstringlen);
						clientput("\"", 1);
					}
					else {
						if (guifiledlgTitle((UCHAR *) scanstring, scanstringlen)) goto makereserror2;
					}
				}
				break;
			}
		}

		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("/>", -1);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
			e1 = (ELEMENT *) clientelement(); /* assume <r> element */
			clientrelease();
			if (e1 == NULL || e1->firstattribute != NULL) {
				if (e1 == NULL) dbcerror(771);
				dbcerrinfo(771, (UCHAR *) e1->firstattribute->value, -1);
			}
		}
		else {
			if (guifiledlgend()) goto makereserror1;
		}
		return; /* Special dialogs */
	}

	if (keywordval == KEYWORD_ALERTDLG) {
		if (scancomma()) {
			scanname();
			if (nametokeywordvalue() != KEYWORD_TEXT) goto makereserror2;
			scanequal();
			scantext();
		}
		else scanstringlen = 0;
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("<prep alertdlg=", -1);
			openinfo->restype = RES_TYPE_ALERTDLG;
			clientput(openinfo->ident, -1);
			if (scanstringlen != 0) {
				clientput(" text=\"", -1);
				clientputdata(scanstring, scanstringlen);
				clientput("\"", 1);
			}
			clientput("/>", -1);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
			e1 = (ELEMENT *) clientelement(); /* assume <r> element */
			clientrelease();
			if (e1 == NULL || e1->firstattribute != NULL) goto makereserror1;
		}
		else {
			if (guialertdlgstart(reshandle, resname, MAXRESNAMELEN)) goto makereserror1;
			if (scanstringlen != 0) {
				if (guialertdlgtext((UCHAR *) scanstring, scanstringlen)) goto makereserror2;
			}
			if (guialertdlgend()) goto makereserror1;
		}
		return;
	}

	if (keywordval == KEYWORD_FONTDLG) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("<prep fontdlg=", -1);
			clientput(openinfo->ident, -1);
			clientput("/>", -1);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
			e1 = (ELEMENT *) clientelement(); /* assume <r> element */
			if (e1 == NULL || e1->firstattribute != NULL) {
				clientrelease();
				goto makereserror1;
			}
			openinfo->restype = RES_TYPE_FONTDLG;
			clientrelease();
		}
		else if (guifontdlgstart(reshandle, resname, MAXRESNAMELEN)) goto makereserror1;
		if (scancomma()) {
			scanname();
			keywordval = nametokeywordvalue();
			if (keywordval != KEYWORD_DEFAULT) goto makereserror2;
			scanequal();
			scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				/* not supported in the gui client */
			}
			else if (guifontdlgdefault((UCHAR *) scanstring, scanstringlen)) goto makereserror2;
		}
		if (!(dbcflags & DBCFLAG_CLIENTINIT)) {
			if (guifontdlgend()) goto makereserror1;
		}
		return;
	}

	if (keywordval == KEYWORD_COLORDLG) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("<prep colordlg=", -1);
			clientput(openinfo->ident, -1);
			clientput("/>", -1);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
			e1 = (ELEMENT *) clientelement(); /* assume <r> element */
			if (e1 == NULL || e1->firstattribute != NULL) {
				clientrelease();
				goto makereserror1;
			}
			openinfo->restype = RES_TYPE_COLORDLG;
			clientrelease();
		}
		else if (guicolordlgstart(reshandle, resname, 8)) goto makereserror1;
		if (scancomma()) {
			scanname();
			keywordval = nametokeywordvalue();
			if (keywordval != KEYWORD_DEFAULT) goto makereserror2;
			scanequal();
			scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				/* not supported in the gui client */
			}
			else if (guicolordlgdefault((UCHAR *) scanstring, scanstringlen)) goto makereserror2;
		}
		if (!(dbcflags & DBCFLAG_CLIENTINIT)) {
			if (guicolordlgend()) goto makereserror1;
		}
		return;
	}

	if (keywordval == KEYWORD_TOOLBAR) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("<prep toolbar=", -1);
			clientput(openinfo->ident, -1);
			clientput(">", -1);
		}
		else guitoolbarstart(reshandle, resname, MAXRESNAMELEN);
		while (scancomma()) {
			scanname();
			keywordval = nametokeywordvalue();
			switch(keywordval) {
			case KEYWORD_PUSHBUTTON:  /* <item>:<icon>:<text> */
				scanequal();
				i1 = scannumber();
				scancolon();
				scanname();
				h1 = scanstringlen;
				memcpy(textmask, scanstring, h1);  /* icon name */
				scancolon();
				if (scanstart > scanend) scanstringlen = 0;
				else scantext();
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					clientput("<pushbutton i=", -1);
					mscitoa(i1, work);
					clientput(work, -1);
					if (scanstringlen > 0) {
						clientput(" t=\"", -1);
						clientputdata((CHAR *) scanstring, scanstringlen);
						clientput("\"", 1);
					}
					clientput(" icon=", -1);
					textmask[h1] = '\0';
					clientput(interpretIconName((CHAR *)textmask), -1);
					clientput("/>", -1);
				}
				else if (guitoolbarpushbutton(i1, textmask, h1, (UCHAR *) scanstring, scanstringlen) < 0) goto makereserror4;
				break;
			case KEYWORD_DROPBOX:	/* <item>:<hsize>:<vsize> */
				scanequal();
				i1 = scannumber();
				scancolon();
				h1 = scannumber();
				scancolon();
				v1 = scannumber();
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					sprintf(work, "<dropbox i=\"%d\" h1=\"%d\" v1=\"%d\"/>", i1, h1, v1);
					clientput(work, -1);
				}
				else if (guitoolbardropbox(i1, h1, v1) < 0) goto makereserror4;
				break;
			case KEYWORD_LOCKBUTTON:  /* <item>:<icon>:<text> */
				scanequal();
				i1 = scannumber();
				scancolon();
				scanname();
				h1 = scanstringlen;
				memcpy(textmask, scanstring, h1);  /* icon name */
				scancolon();
				if (scanstart > scanend) scanstringlen = 0;
				else scantext();
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					clientput("<lockbutton i=", -1);
					mscitoa(i1, work);
					clientput(work, -1);
					if (scanstringlen > 0) {
						clientput(" t=\"", -1);
						clientputdata((CHAR *) scanstring, scanstringlen);
						clientput("\"", 1);
					}
					clientput(" icon=", -1);
					textmask[h1] = '\0';
					clientput(interpretIconName((CHAR *)textmask), -1);
					clientput("/>", -1);
				}
				else if (guitoolbarlockbutton(i1, textmask, h1, (UCHAR *) scanstring, scanstringlen) < 0) goto makereserror4;
				break;
			case KEYWORD_TEXTPUSHBUTTON:	/* <item>:<text> */
				scanequal();
				i1 = scannumber();
				scancolon();
				scantext();
				if ((dbcflags & DBCFLAG_CLIENTINIT) && SUPPORTSTOOLBARTEXTPB)
				{
					sprintf(work, "<textpushbutton i=\"%u\"", (UINT)i1);
					clientput(work, -1);
					if (scanstringlen > 0) {
						clientput(" t=\"", -1);
						clientputdata((CHAR *) scanstring, scanstringlen);
						clientput("\"", 1);
					}
					clientput("/>", -1);
				}
				else {
					/* Implement this for 17.0  */
				}
				break;
			case KEYWORD_SPACE:
				if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<space/>", -1);
				else if (guitoolbarspace() < 0) goto makereserror2;
				break;
			case KEYWORD_GRAY:
				if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<gray/>", -1);
				else if (guitoolbargray()) goto makereserror2;
				break;
			default:
				goto makereserror2;
			}
		}
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("</prep>", -1);
			if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
			e1 = (ELEMENT *) clientelement(); /* assume <r> element */
			if (e1 == NULL || e1->firstattribute != NULL) {
				clientrelease();
				goto makereserror1;
			}
			openinfo->restype = RES_TYPE_TOOLBAR;
			clientrelease();
		}
		else if (guitoolbarend() < 0) dbcerror(767);
		return;
	}

	if (keywordval != KEYWORD_PANEL && keywordval != KEYWORD_DIALOG) {
		dbcerrinfo(763, (UCHAR *) scanstring, scanstringlen);
		return;
	}
	makerespnldlg(reshandle, resname, keywordval, openinfo);
	return;

makereserror1:
	dbcerror(771);
	return;
makereserror2:
	dbcerrinfo(771, (UCHAR *) scanstring, scanstringlen);
	return;
makereserror4:
	dbcerrinfo(779, (UCHAR *) scanstring, scanstringlen);
	return;
}

static void makerespnldlg(RESHANDLE reshandle, UCHAR *resname, INT keywordval, OPENINFO *openinfo) {

	INT firstflag, radioflg, panelflag, emptyflag, tabgroupflag;
	INT rc, opentabflag, boxtabs[MAXBOXTABS];
	INT boxtabscnt, i1, i2, h1, v1;
	ELEMENT *e1 = NULL;
	CHAR c1, work[128], textmask[130], boxjust[MAXBOXTABS];

	if (keywordval == KEYWORD_PANEL) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("<prep panel=\"", -1);
			clientput(openinfo->ident, -1);
			clientput("\"", 1);
			openinfo->restype = RES_TYPE_PANEL;
		}
		else if (guipanelstart(reshandle, resname, MAXRESNAMELEN)) goto makerespnldlgerror1;
		panelflag = firstflag = TRUE;
	}
	else {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("<prep dialog=\"", -1);
			clientput(openinfo->ident, -1);
			clientput("\"", 1);
			openinfo->restype = RES_TYPE_DIALOG;
		}
		else if (guidialogstart(reshandle, resname, MAXRESNAMELEN)) goto makerespnldlgerror1;
		panelflag = FALSE;
		firstflag = TRUE;
	}
	emptyflag = TRUE;
	radioflg = tabgroupflag = opentabflag = FALSE;

	while(scancomma()) {
		emptyflag = FALSE;
		scanname();
		keywordval = nametokeywordvalue();
		if (firstflag) {
			switch(keywordval) {
			case KEYWORD_TITLE:  /* <text> */
				if (panelflag) goto makerespnldlgerror2;
				scanequal();
				scantext();
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					clientput(" title=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				else if (guipandlgtitle(scanstring, scanstringlen)) goto makerespnldlgerror2;
				continue;
			case KEYWORD_NOCLOSE:
				if (panelflag) goto makerespnldlgerror2;
				if (dbcflags & DBCFLAG_CLIENTINIT) clientput(" noclose=yes", -1);
				else guipandlgnoclosebutton();
				continue;
			case KEYWORD_SIZE:  /* <num>:<num> */
				scanequal();
				h1 = scannumber();
				scancolon();
				v1 = scannumber();
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					clientput(" hsize=", -1);
					mscitoa(h1, work);
					clientput(work, -1);
					clientput(" vsize=", -1);
					mscitoa(v1, work);
					clientput(work, -1);
				}
				else if (guipandlgsize(h1, v1)) goto makerespnldlgerror2;
				continue;
			default:
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					clientput(">", -1);
					if (GuiCtlfont != NULL) {
						if (guiservergenfont(GuiCtlfont, FALSE) <= 0) {
							dbcerrinfo(778, (UCHAR*)GuiCtlfont, -1);
						}
					}
				}
				firstflag = FALSE;
			}
		}

		/*
		 * This switch is designed to filter what kinds of
		 * controls are acceptable inside a buttongroup
		 */
		switch(keywordval) {
		case KEYWORD_H: /* <n> */
			scanequal();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<p h=", -1);
				mscitoa(scannumber(), work);
				clientput(work, -1);
				clientput("/>", -1);
			}
			else if (guipandlgposh(scannumber())) goto makerespnldlgerror2;
			continue;
		case KEYWORD_V: /* <n> */
			scanequal();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<p v=", -1);
				mscitoa(scannumber(), work);
				clientput(work, -1);
				clientput("/>", -1);
			}
			else if (guipandlgposv(scannumber())) goto makerespnldlgerror2;
			continue;
		case KEYWORD_HA:  /* <n> and can be negative */
		case KEYWORD_VA:
			scanequal();
			i1 = 1;
			if (*scanstart == '-') {
				scanstart++;
				i1 = -1;
			}
			i1 *= scannumber();
			if (keywordval == KEYWORD_HA) {
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					clientput("<p ha=\"", -1);
					mscitoa(i1, work);
					clientput(work, -1);
					clientput("\"/>", -1);
				}
				else if (guipandlgposha(i1)) goto makerespnldlgerror2;
			}
			else {
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					clientput("<p va=\"", -1);
					mscitoa(i1, work);
					clientput(work, -1);
					clientput("\"/>", -1);
				}
				else if (guipandlgposva(i1)) goto makerespnldlgerror2;
			}
			continue;
		case KEYWORD_BUTTON:  /* <item>:<text> */
			if (!radioflg) dbcerrinfo(771, (UCHAR *) scanstring, scanstringlen);
			scanequal();
			i1 = scannumber();
			scancolon();
			if (scanstart > scanend) scanstringlen = 0;
			else scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<button i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				clientput(" t=\"", -1);
				clientputdata((CHAR *) scanstring, scanstringlen);
				clientput("\"/>", -1);
			}
			else if (guipandlgbutton(i1, scanstring, scanstringlen)) goto makerespnldlgerror2;
			continue;
		case KEYWORD_HELPTEXT: /* <text> */
			scanequal();
			scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<helptext t=\"", -1);
				clientputdata((CHAR *) scanstring, scanstringlen);
				clientput("\"/>", -1);
			}
			else if (guipandlghelptext(scanstring, scanstringlen)) goto makerespnldlgerror2;
			continue;
		case KEYWORD_BUTTONGROUP:
			if (radioflg) dbcerrinfo(771, (UCHAR *) scanstring, scanstringlen);
			radioflg = TRUE;
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<buttongroup>", -1);
			else if (guipandlgbuttongroup()) goto makerespnldlgerror2;
			continue;
		case KEYWORD_ENDBUTTONGROUP:
			if (!radioflg) dbcerrinfo(771, (UCHAR *) scanstring, scanstringlen);
			radioflg = FALSE;
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("</buttongroup>", -1);
			else if (guipandlgendbuttongroup()) goto makerespnldlgerror2;
			continue;
		/*
		 * Moved the FONT section of code here from the below switch. 6/3/08
		 * This is to avoid erroring when it is seen in a buttongroup.
		 * But to be consistent with the Smart Clients, it will be ignored if in
		 * a button group.
		 */
		case KEYWORD_FONT:
			scanequal();
			scanfont();
			if (!radioflg) {
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					scanstring[scanstringlen] = '\0';
					if (guiservergenfont(scanstring, FALSE) <= 0) {
						dbcerrinfo(778, (UCHAR*)scanstring, -1);//dbcerror(799);
					}
				}
				else if (guipandlgfont(scanstring, scanstringlen)) goto makerespnldlgerror3;
			}
			continue;
		default:
			if (radioflg) goto makerespnldlgerror2;
		}

		switch(keywordval) {
		case KEYWORD_GRAY:
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<gray/>", -1);
			else if (guipandlggray()) goto makerespnldlgerror2;
			continue;
		case KEYWORD_SHOWONLY:
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<showonly/>", -1);
			else if (guipandlgshowonly()) goto makerespnldlgerror2;
			continue;
		case KEYWORD_NOFOCUS:
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<nofocus/>", -1);
			// What the next call does is "lastcontrol->style |= CTRLSTYLE_NOFOCUS"
			else if (guipandlgnofocus()) goto makerespnldlgerror2;
			continue;
		case KEYWORD_TEXTCOLOR:
			scanequal();
			scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<textcolor color=", -1);
				clientputdata((CHAR *) scanstring, scanstringlen);
				clientput("/>", -1);
			}
			else if (guipandlgtextcolor(scanstring, scanstringlen)) goto makerespnldlgerror2;
			continue;
		case KEYWORD_READONLY:
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<readonly/>", -1);
			else if (guipandlgreadonly()) goto makerespnldlgerror2;
			continue;
		case KEYWORD_INSERTORDER:
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<insertorder/>", -1);
			else if (guipandlginsertorder()) goto makerespnldlgerror2;
			continue;

		case KEYWORD_JUSTIFYLEFT:
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<justifyleft/>", -1);
			else if (guipandlgjustify(CTRLSTYLE_LEFTJUSTIFIED)) goto makerespnldlgerror2;
			continue;
		case KEYWORD_JUSTIFYRIGHT:
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<justifyright/>", -1);
			else if (guipandlgjustify(CTRLSTYLE_RIGHTJUSTIFIED)) goto makerespnldlgerror2;
			continue;
		case KEYWORD_JUSTIFYCENTER:
			if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<justifycenter/>", -1);
			else if (guipandlgjustify(CTRLSTYLE_CENTERJUSTIFIED)) goto makerespnldlgerror2;
			continue;

		case KEYWORD_TABGROUPEND:
			if (!tabgroupflag) goto makerespnldlgerror2;
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (opentabflag == TRUE) {
					clientput("</tab>", -1);
					opentabflag = FALSE;
				}
				clientput("</tabgroup>", -1);
			}
			else if (guipandlgtabgroupend()) goto makerespnldlgerror2;
			tabgroupflag = FALSE;
			continue;
		}
		scanequal();
		switch(keywordval) {
		case KEYWORD_TABLE:
			if (dbcflags & DBCFLAG_CLIENTINIT) processTablePrepStringForSC();
			else {
				i1 = processTablePrepString();
				if (i1) {
					if (i1 == RC_NO_MEM) dbcerror(796);
					else goto makerespnldlgerror1;
				}
			}
			break;
		case KEYWORD_VICON: /* <item>:<iconname> */
		case KEYWORD_ICON: /* <iconname> */
			if (keywordval == KEYWORD_VICON) {
				i1 = scannumber();
				scancolon();
			}
			else {
				i1 = 0;
			}
			scanname();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<icon ", -1);
				if (keywordval == KEYWORD_VICON) {
					clientput("i=", -1);
					mscitoa(i1, work);
					clientput(work, -1);
					clientput(" ", 1);
				}
				clientput("image=", -1);
				scanstring[scanstringlen] = 0;
				clientput(interpretIconName(scanstring), -1);
				clientput("/>", -1);
			}
			else if (guipandlgicon(i1, scanstring, scanstringlen)) goto makerespnldlgerror4;
			break;
		case KEYWORD_BOX:  /* <h>:<v> */
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<box hsize=", -1);
				mscitoa(h1, work);
				clientput(work, -1);
				clientput(" vsize=", -1);
				mscitoa(v1, work);
				clientput(work, -1);
				clientput("/>", -1);
			}
			else if (guipandlgbox(h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_BOXTITLE: /* <text>:<h>:<v> */
			scantext();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<box hsize=", -1);
				mscitoa(h1, work);
				clientput(work, -1);
				clientput(" vsize=", -1);
				mscitoa(v1, work);
				clientput(work, -1);
				clientput(" t=\"", -1);
				clientputdata((CHAR *) scanstring, scanstringlen);
				clientput("\"/>", -1);
			}
			else if (guipandlgboxtitle(scanstring, scanstringlen, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_TEXT: /* <text> */
			scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<text t=\"", -1);
				clientputdata((CHAR *) scanstring, scanstringlen);
				clientput("\"/>", -1);
			}
			else if (guipandlgtext(scanstring, scanstringlen)) goto makerespnldlgerror2;
			break;

		/**
		 * Send the ATEXT keyword to Smart Clients. Here we convert it to a VTEXT
		 */
		case KEYWORD_ATEXT: /* <item>:<text> */
			i1 = scannumber();
			scancolon();
			if (scanstart > scanend) scanstringlen = 0;
			else scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<atext t=\"", -1);
				clientputdata((CHAR *) scanstring, scanstringlen);
				clientput("\" i=\"", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				clientput("\"/>", -1);
			}
			else if (guipandlgvtext(i1, scanstring, scanstringlen)) goto makerespnldlgerror2;
			break;

		case KEYWORD_LTCHECKBOX:
		case KEYWORD_CHECKBOX:  /* <item>:<text> */
			i1 = scannumber();
			scancolon();
			if (scanstart > scanend) scanstringlen = 0;
			else scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (keywordval == KEYWORD_CHECKBOX) clientput("<checkbox i=", -1);
				else clientput("<ltcheckbox i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				clientput(" t=\"", -1);
				clientputdata((CHAR *) scanstring, scanstringlen);
				clientput("\"/>", -1);
			}
			else {
				if (keywordval == KEYWORD_CHECKBOX) {
					if (guipandlgcheckbox(i1, scanstring, scanstringlen)) goto makerespnldlgerror2;
				}
				else {
					if (guipandlgltcheckbox(i1, scanstring, scanstringlen)) goto makerespnldlgerror2;
				}
			}
			break;
		case KEYWORD_POPBOX: /* <item>:<hsize> */
			i1 = scannumber();
			scancolon();
			h1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<popbox i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				clientput(" hsize=", -1);
				mscitoa(h1, work);
				clientput(work, -1);
				clientput("/>", -1);
			}
			else if (guipandlgpopbox(i1, h1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_VTEXT:  /* <item>:<text> */
			i1 = scannumber();
			scancolon();
			if (scanstart > scanend) scanstringlen = 0;
			else scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<vtext", -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				clientput(" i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				clientput("/>", -1);
			}
			else if (guipandlgvtext(i1, scanstring, scanstringlen)) goto makerespnldlgerror2;
			break;
		case KEYWORD_EDIT:  /* <item>:<text>:<width> */
			i1 = scannumber();
			scancolon();
			scantext();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<edit i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				clientput(" hsize=", -1);
				mscitoa(v1, work);
				clientput(work, -1);
				clientput("/>", -1);
			}
			else if (guipandlgedit(i1, scanstring, scanstringlen, v1)) goto makerespnldlgerror2;
			break;

		/**
		 * Send the CATEXT keyword to Smart Clients. Here we convert it to a CVTEXT
		 */
		case KEYWORD_CATEXT:  /* <item>:<text>:<width> */
			i1 = scannumber();
			scancolon();
			scantext();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<catext i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				clientput(" hsize=", -1);
				mscitoa(v1, work);
				clientput(work, -1);
				clientput("/>", -1);
			}
			else if (guipandlgcvtext(i1, scanstring, scanstringlen, v1)) goto makerespnldlgerror2;
			break;

		case KEYWORD_CVTEXT:  /* <item>:<text>:<width> */
			i1 = scannumber();
			scancolon();
			scantext();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<cvtext i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				clientput(" hsize=", -1);
				mscitoa(v1, work);
				clientput(work, -1);
				clientput("/>", -1);
			}
			else if (guipandlgcvtext(i1, scanstring, scanstringlen, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_CTEXT:  /* <text>:<width> */
			scantext();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<ctext t=\"", -1);
				clientputdata((CHAR *) scanstring, scanstringlen);
				clientput("\" hsize=", -1);
				mscitoa(v1, work);
				clientput(work, -1);
				clientput("/>", -1);
			}
			else {
				i1 = guipandlgctext(scanstring, scanstringlen, v1);
				if (i1) {
					if (i1 == RC_NO_MEM) {
						dbcerrinfo(796, (UCHAR *) scanstring, scanstringlen);
					}
					goto makerespnldlgerror2;
				}
			}
			break;
		case KEYWORD_PUSHBUTTON:  /* <item>:<text>:<h>:<v> */
			i1 = scannumber();
			scancolon();
			scantext();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<pushbutton i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				clientput(" hsize=", -1);
				mscitoa(h1, work);
				clientput(work, -1);
				clientput(" vsize=", -1);
				mscitoa(v1, work);
				clientput(work, -1);
				clientput("/>", -1);
			}
			else if (guipandlgpushbutton(i1, scanstring, scanstringlen, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_DEFPUSHBUTTON:  /* <item>:<text>:<h>:<v> */
			i1 = scannumber();
			scancolon();
			scantext();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<defpushbutton i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				clientput(" hsize=", -1);
				mscitoa(h1, work);
				clientput(work, -1);
				clientput(" vsize=", -1);
				mscitoa(v1, work);
				clientput(work, -1);
				clientput("/>", -1);
			}
			else if (guipandlgdefpushbutton(i1, scanstring, scanstringlen, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_ESCPUSHBUTTON:  /* <item>:<text>:<h>:<v> */
			i1 = scannumber();
			scancolon();
			scantext();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<escpushbutton i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				clientput(" hsize=", -1);
				mscitoa(h1, work);
				clientput(work, -1);
				clientput(" vsize=", -1);
				mscitoa(v1, work);
				clientput(work, -1);
				clientput("/>", -1);
			}
			else if (guipandlgescpushbutton(i1, scanstring, scanstringlen, h1, v1)) goto makerespnldlgerror2;
			break;
//		case KEYWORD_LOCKBUTTON:  /* <item>:<text>:<h>:<v> */
//			i1 = scannumber();
//			scancolon();
//			scantext();
//			scancolon();
//			h1 = scannumber();
//			scancolon();
//			v1 = scannumber();
//			if (dbcflags & DBCFLAG_CLIENTINIT) {
//				/* not support in gui client */
//			}
//			else if (guipandlglockbutton(i1, scanstring, scanstringlen, h1, v1)) goto makerespnldlgerror2;;
//			break;
		case KEYWORD_ICONLOCKBUTTON:  /* <item>:<iconname>:<h>:<v> */
		case KEYWORD_ICONPUSHBUTTON:
		case KEYWORD_ICONDEFPUSHBUTTON:
		case KEYWORD_ICONESCPUSHBUTTON:
			i1 = scannumber();
			scancolon();
			scanname();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (keywordval == KEYWORD_ICONLOCKBUTTON) {
				if (dbcflags & DBCFLAG_CLIENTINIT) {
					/* not supported in gui client */
				}
				else rc = guipandlgiconlockbutton(i1, scanstring, scanstringlen, h1, v1);
			}
			else if (keywordval == KEYWORD_ICONPUSHBUTTON) {
				if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<iconpushbutton i=\"", -1);
				else rc = guipandlgiconpushbutton(i1, scanstring, scanstringlen, h1, v1);
			}
			else if (keywordval == KEYWORD_ICONDEFPUSHBUTTON) {
				if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<icondefpushbutton i=\"", -1);
				else rc = guipandlgicondefpushbutton(i1, scanstring, scanstringlen, h1, v1);
			}
			else {
				if (dbcflags & DBCFLAG_CLIENTINIT) clientput("<iconescpushbutton i=\"", -1);
				else rc = guipandlgiconescpushbutton(i1, scanstring, scanstringlen, h1, v1);
			}
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				mscitoa(i1, work);
				clientput(work, -1);
				scanstring[scanstringlen] = '\0';
				sprintf(work, "\" image=\"%s\" hsize=\"%d\" vsize=\"%d\"/>", interpretIconName(scanstring),
						h1, v1);
				clientput(work, -1);
				rc = 0;
			}
			if (rc) goto makerespnldlgerror4;
			break;
		case KEYWORD_MEDIT:  /* <item>:<text>:<h>:<v> */
			i1 = scannumber();
			scancolon();
			scantext();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<medit i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				sprintf(work, " hsize=\"%d\" vsize=\"%d\"/>", h1, v1);
				clientput(work, -1);
			}
			else if (guipandlgmedit(i1, scanstring, scanstringlen, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_MEDITHS:  /* <item>:<text>:<h>:<v> */
			i1 = scannumber();
			scancolon();
			scantext();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<mediths i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				sprintf(work, " hsize=\"%d\" vsize=\"%d\"/>", h1, v1);
				clientput(work, -1);
			}
			else if (guipandlgmediths(i1, scanstring, scanstringlen, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_MEDITVS:  /* <item>:<text>:<h>:<v> */
			i1 = scannumber();
			scancolon();
			scantext();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<meditvs i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				sprintf(work, " hsize=\"%d\" vsize=\"%d\"/>", h1, v1);
				clientput(work, -1);
			}
			else if (guipandlgmeditvs(i1, scanstring, scanstringlen, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_MEDITS:  /* <item>:<text>:<h>:<v> */
			i1 = scannumber();
			scancolon();
			scantext();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<medits i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				sprintf(work, " hsize=\"%d\" vsize=\"%d\"/>", h1, v1);
				clientput(work, -1);
			}
			else if (guipandlgmedits(i1, scanstring, scanstringlen, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_FEDIT:  /* <item>:<mask>:<init>:<h> */
			i1 = scannumber();
			scancolon();
			scantext();
			v1 = (scanstringlen < sizeof(textmask)) ? scanstringlen : sizeof(textmask);
			memcpy(textmask, scanstring, v1);
			scancolon();
			scantext();
			scancolon();
			h1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<fedit i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				clientput(" mask=\"", -1);
				textmask[v1] = '\0';
				clientputdata((CHAR *) textmask, -1);
				clientput("\" hsize=", -1);
				mscitoa(h1, work);
				clientput(work, -1);
				clientput("/>", -1);
			}
			else if (guipandlgfedit(i1, (UCHAR *) scanstring, scanstringlen, (UCHAR *)textmask, v1, h1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_MLEDIT: /* <item>:<text>:<h>:<v>:<max-chars> */
			i1 = scannumber();
			scancolon();
			scantext();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			scancolon();
			i2 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<mledit i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				sprintf(work, " hsize=\"%d\" vsize=\"%d\" maxchars=\"%d\"/>", h1, v1, i2);
				clientput(work, -1);
			}
			else if (guipandlgmledit(i1, (UCHAR *) scanstring, scanstringlen, h1, v1, i2, FALSE, FALSE)) goto makerespnldlgerror2;
			break;
		case KEYWORD_MLEDITHS: /* <item>:<text>:<h>:<v>:<max-chars> */
			i1 = scannumber();
			scancolon();
			scantext();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			scancolon();
			i2 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<mlediths i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				sprintf(work, " hsize=\"%d\" vsize=\"%d\" maxchars=\"%d\"/>", h1, v1, i2);
				clientput(work, -1);
			}
			else if (guipandlgmledit(i1, (UCHAR *) scanstring, scanstringlen, h1, v1, i2, TRUE, FALSE)) goto makerespnldlgerror2;
			break;
		case KEYWORD_MLEDITVS: /* <item>:<text>:<h>:<v>:<max-chars> */
			i1 = scannumber();
			scancolon();
			scantext();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			scancolon();
			i2 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<mleditvs i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				sprintf(work, " hsize=\"%d\" vsize=\"%d\" maxchars=\"%d\"/>", h1, v1, i2);
				clientput(work, -1);
			}
			else if (guipandlgmledit(i1, (UCHAR *) scanstring, scanstringlen, h1, v1, i2, FALSE, TRUE)) goto makerespnldlgerror2;
			break;
		case KEYWORD_MLEDITS: /* <item>:<text>:<h>:<v>:<max-chars> */
			i1 = scannumber();
			scancolon();
			scantext();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			scancolon();
			i2 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<mledits i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				sprintf(work, " hsize=\"%d\" vsize=\"%d\" maxchars=\"%d\"/>", h1, v1, i2);
				clientput(work, -1);
			}
			else if (guipandlgmledit(i1, (UCHAR *) scanstring, scanstringlen, h1, v1, i2, TRUE, TRUE)) goto makerespnldlgerror2;
			break;
		case KEYWORD_LEDIT: /* <item>:<text>:<h>:<max-chars> */
			i1 = scannumber();
			scancolon();
			scantext();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<ledit i=\"%d\"", i1);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				sprintf(work, " hsize=\"%d\" maxchars=\"%d\"/>", h1, v1);
				clientput(work, -1);
			}
			else if (guipandlgledit(i1, (UCHAR *) scanstring, scanstringlen, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_TREE:  /* <item>:<h>:<v> */
			i1 = scannumber();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) cltputihv("tree", i1, h1, v1);
			else if (guipandlgtree(i1, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_LISTBOX:  /* <item>:<h>:<v> */
			i1 = scannumber();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) cltputihv("listbox", i1, h1, v1);
			else if (guipandlglistbox(i1, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_LISTBOXHS:  /* <item>:<h>:<v> */
			i1 = scannumber();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) cltputihv("listboxhs", i1, h1, v1);
			else if (guipandlglistboxhs(i1, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_MLISTBOX:  /* <item>:<h>:<v> */
			i1 = scannumber();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) cltputihv("mlistbox", i1, h1, v1);
			else if (guipandlgmlistbox(i1, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_MLISTBOXHS:  /* <item>:<h>:<v> */
			i1 = scannumber();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) cltputihv("mlistboxhs", i1, h1, v1);
			else if (guipandlgmlistboxhs(i1, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_DROPBOX:  /* <item>:<h>:<v> */
			i1 = scannumber();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) cltputihv("dropbox", i1, h1, v1);
			else if (guipandlgdropbox(i1, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_HSCROLLBAR:  /* <item>:<h>:<v> */
			i1 = scannumber();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) cltputihv("hscrollbar", i1, h1, v1);
			else if (guipandlghscrollbar(i1, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_VSCROLLBAR:  /* <item>:<h>:<v> */
			i1 = scannumber();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) cltputihv("vscrollbar", i1, h1, v1);
			else if (guipandlgvscrollbar(i1, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_PROGRESSBAR:  /* <item>:<h>:<v> */
			i1 = scannumber();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) cltputihv("progressbar", i1, h1, v1);
			else if (guipandlgprogressbar(i1, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_BOXTABS:  /* <n>:<n>:<n>: .... :<n> */
			boxtabscnt = 0;
			while (scanstart <= scanend && boxtabscnt < MAXBOXTABS) {
				scantext();
				if (scanstring[0] == '\0') scanerror(771);
				i1 = i2 = 0;
				while ((UINT) i1 < scanstringlen && scanstring[i1] == ' ') i1++;
				if (!isdigit(scanstring[i1])) scanerror(776);
				while ((UINT) i1 < scanstringlen && isdigit(scanstring[i1])) i2 = i2 * 10 + scanstring[i1++] - '0';
				c1 = scanstring[scanstringlen - 1];
				if (isdigit(c1)) boxjust[boxtabscnt] = 'l';
				else {
					if (c1 == 'L' || c1 == 'l') boxjust[boxtabscnt] = 'l';
					else if (c1 == 'R' || c1 == 'r') boxjust[boxtabscnt] = 'r';
					else if (c1 == 'C' || c1 == 'c') boxjust[boxtabscnt] = 'c';
					else scanerror(771);
				}
				boxtabs[boxtabscnt++] = i2;
				if (*scanstart != ':') break;
				scanstart++;
			}
			if (*scanstart == ':' && boxtabscnt == MAXBOXTABS) scanerror(776);
			if ((dbcflags & DBCFLAG_CLIENTINIT) && boxtabscnt > 0) {
				clientput("<boxtabs tabs=\"", -1);
				mscitoa(boxtabs[0], work);
				clientput(work, -1);
				for (i1 = 1; i1 < boxtabscnt; i1++) {
					clientput(":", -1);
					mscitoa(boxtabs[i1], work);
					clientput(work, -1);
				}
				clientput("\" j=\"", -1);
				clientput(boxjust, boxtabscnt);
				clientput("\"/>", -1);
			}
			else if (guipandlgboxtabs(boxtabs, boxjust, boxtabscnt)) goto makerespnldlgerror2;
			break;
		case KEYWORD_PEDIT: /* <item>:<text>:<hsize> */
			i1 = scannumber();
			scancolon();
			scantext();
			scancolon();
			h1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<pedit i=\"%d\"", i1);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				sprintf(work, " hsize=\"%d\"/>", h1);
				clientput(work, -1);
			}
			else if (guipandlgpedit(i1, scanstring, scanstringlen, h1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_PLEDIT: /* <item>:<text>:<hsize>:<maxchars> */
			i1 = scannumber();
			scancolon();
			scantext();
			scancolon();
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				sprintf(work, "<pledit i=\"%d\"", i1);
				clientput(work, -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata((CHAR *) scanstring, scanstringlen);
					clientput("\"", -1);
				}
				sprintf(work, " hsize=\"%d\" maxchars=\"%d\"/>", h1, v1);
				clientput(work, -1);
			}
			else if (guipandlgpledit(i1, (UCHAR *) scanstring, scanstringlen, h1, v1)) goto makerespnldlgerror2;
			break;
		case KEYWORD_RTEXT: /* <text> */
			scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<rtext t=\"", -1);
				clientputdata(scanstring, scanstringlen);
				clientput("\"/>", -1);
			}
			else if (guipandlgrtext(scanstring, scanstringlen)) goto makerespnldlgerror2;
			break;

		/**
		 * Send the RATEXT keyword to Smart Clients. Here we convert it to a RVTEXT
		 */
		case KEYWORD_RATEXT:  /* <item>:<text> */
			i1 = scannumber();
			scancolon();
			if (scanstart > scanend) scanstringlen = 0;
			else scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<ratext", -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata(scanstring, scanstringlen);
					clientput("\"", -1);
				}
				clientput(" i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				clientput("/>", -1);
			}
			else if (guipandlgrvtext(i1, scanstring, scanstringlen)) goto makerespnldlgerror2;
			break;

		case KEYWORD_RVTEXT:  /* <item>:<text> */
			i1 = scannumber();
			scancolon();
			if (scanstart > scanend) scanstringlen = 0;
			else scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<rvtext", -1);
				if (scanstringlen > 0) {
					clientput(" t=\"", -1);
					clientputdata(scanstring, scanstringlen);
					clientput("\"", -1);
				}
				clientput(" i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				clientput("/>", -1);
			}
			else if (guipandlgrvtext(i1, scanstring, scanstringlen)) goto makerespnldlgerror2;
			break;
		case KEYWORD_TABGROUP:  /* <h>:<v> */
			if (tabgroupflag) goto makerespnldlgerror2;
			h1 = scannumber();
			scancolon();
			v1 = scannumber();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<tabgroup hsize=", -1);
				mscitoa(h1, work);
				clientput(work, -1);
				clientput(" vsize=", -1);
				mscitoa(v1, work);
				clientput(work, -1);
				clientput(">", -1);
			}
			else if (guipandlgtabgroup(h1, v1)) goto makerespnldlgerror2;
			if (!scancomma()) goto makerespnldlgerror2;
			scanname();
			keywordval = nametokeywordvalue();
			if (keywordval != KEYWORD_TAB) goto makerespnldlgerror2;
			tabgroupflag = TRUE;
			scanequal(); // @suppress("No break at end of case")
			// fall through
		case KEYWORD_TAB:  /* <item>:<text> */
			if (!tabgroupflag) goto makerespnldlgerror2;
			i1 = scannumber();
			scancolon();
			scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (opentabflag == TRUE) clientput("</tab>", -1);
				else opentabflag = TRUE;
				clientput("<tab i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				clientput(" t=\"", -1);
				clientputdata((CHAR *) scanstring, scanstringlen);
				clientput("\">", 2);
			}
			else if (guipandlgtab(i1, scanstring, scanstringlen)) goto makerespnldlgerror2;
			break;
		default:
			goto makerespnldlgerror2;
		}
	}
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		if (emptyflag || firstflag) clientput(">", 1);
		clientput("</prep>", -1);
		if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) >= 0) {
			e1 = (ELEMENT *) clientelement(); /* assume <r> element */
			if (e1 == NULL || e1->firstattribute != NULL) {
				clientrelease();
				goto makerespnldlgerror1;
			}
			clientrelease();
			return;
		}
	}
	else if (!guipandlgend()) {
		return;
	}

makerespnldlgerror1:
	if (e1 == NULL || e1->firstattribute == NULL) dbcerror(771);
	if (strcmp(e1->tag, "r") == 0) {
		ATTRIBUTE *aErr = e1->firstattribute;
		if (strcmp(aErr->tag, "e") == 0) {
			dbcerrinfo(771, (UCHAR*)aErr->value, -1);
		}
	}
	dbcerror(771);

makerespnldlgerror2:
	dbcerrinfo(771, (UCHAR *) scanstring, scanstringlen);

makerespnldlgerror3:
	dbcerrinfo(778, (UCHAR *) scanstring, scanstringlen);

makerespnldlgerror4:
	dbcerrinfo(779, (UCHAR *) scanstring, scanstringlen);
}

static CHAR* interpretIconName(CHAR* text) {
	INT i1;
	OPENINFO *oinfoptr;
	for (i1 = 0, oinfoptr = *openinfopptr; i1 < openinfohi; i1++, oinfoptr++) {
		if ((oinfoptr->davbtype == DAVB_RESOURCE) && !strcmp(oinfoptr->name, text)) {
			strcpy(text, oinfoptr->ident);
			break;
		}
	}
	return text;
}

static void makeresmenu(RESHANDLE reshandle, UCHAR *resname, INT keywordval, OPENINFO *openinfo) {
	INT i1, i2, h1;
	INT openmitemtag, openmaintag;
	CHAR *elementTag = NULL, work[128];
	UCHAR textmask[130];
	ELEMENT *e1;
	ATTRIBUTE *a1 = NULL;
	UINT submenudepth = 0;

	if ((i2 = keywordval) == KEYWORD_MENU) {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("<prep menu=", -1);
			clientput(openinfo->ident, -1);
			clientput(">", -1);
		}
		else if (guimenustart(reshandle, resname, 8)) goto makeresmenuerror1;
	}
	else {
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("<prep popupmenu=", -1);
			clientput(openinfo->ident, -1);
			clientput(">", -1);
		}
		else if (guipopupmenustart(reshandle, resname, 8)) goto makeresmenuerror1;
	}
	openmitemtag = openmaintag = FALSE;
	while (scancomma()) {
		scanname();
		keywordval = nametokeywordvalue();
		switch(keywordval) {
		case KEYWORD_MAIN:  /* <item>:<text> */
			if (i2 == KEYWORD_POPUPMENU) goto makeresmenuerror2;
			scanequal();
			i1 = scannumber();
			scancolon();
			if (scanstart > scanend) scanstringlen = 0;
			else scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (openmitemtag == TRUE) {
					clientput("/>", -1);
					openmitemtag = FALSE;
				}
				if (openmaintag == TRUE) clientput("</main>", -1);
				else openmaintag = TRUE;
				clientput("<main i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				clientput(" t=\"", -1);
				clientputdata((CHAR *) scanstring, -1);
				clientput("\">", -1);
			}
			else if (guimenumain(i1, scanstring, scanstringlen)) goto makeresmenuerror2;
			break;
		case KEYWORD_ITEM:		/* <item>:<text> */
		case KEYWORD_CHECKITEM:	/* <item>:<text> */
			scanequal();
			i1 = scannumber();
			scancolon();
			if (scanstart > scanend) scanstringlen = 0;
			else scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (openmitemtag == TRUE) clientput("/>", -1);
				clientput("<mitem i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				clientput(" t=\"", -1);
				clientputdata((CHAR *) scanstring, -1);
				clientput("\"", -1);
				if (keywordval == KEYWORD_CHECKITEM) clientput(" ck=1", -1);
				openmitemtag = TRUE;
				continue;
			}
			else if (guimenuitem(i1, scanstring, scanstringlen)) goto makeresmenuerror2;
			break;
		case KEYWORD_ICONITEM:  /* <item>:<icon>:<text> */
			scanequal();
			i1 = scannumber();
			scancolon();
			scanname();
			h1 = scanstringlen;
			memcpy(textmask, scanstring, h1);  /* icon name */
			textmask[h1] = 0;
			scancolon();
			if (scanstart > scanend) scanstringlen = 0;
			else scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (openmitemtag == TRUE) clientput("/>", -1);
				clientput("<iconitem i=", -1);
				mscitoa(i1, work);
				clientput(work, -1);
				clientput(" t=\"", -1);
				clientputdata((CHAR *) scanstring, scanstringlen);
				clientput("\" icon=\"", -1);
				clientput(interpretIconName((CHAR *)textmask), -1);
				clientput("\"", -1);
				openmitemtag = TRUE;
				continue;
			}
			else {
				i1 = guimenuiconitem(i1, scanstring, scanstringlen, (CHAR*) textmask, h1);
				if (i1) goto makeresmenuerror2;
			}
			break;
		case KEYWORD_SUBMENU:  /* <item>:<text> */
			scanequal();
			i1 = scannumber();
			scancolon();
			if (scanstart > scanend) scanstringlen = 0;
			else scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (openmitemtag == TRUE) {
					clientput("/>", -1);
					openmitemtag = FALSE;
				}
				elementTag = "submenu";
				sprintf(work, "<%s i=%d t=\"", elementTag, i1);
				clientput(work, -1);
				clientputdata((CHAR *) scanstring, -1);
				clientput("\">", -1);
			}
			else if (guimenusubmenu(i1, scanstring, scanstringlen, NULL, 0)) goto makeresmenuerror2;
			submenudepth++;
			break;
		case KEYWORD_ICONSUBMENU:  /* <item>:<icon>:<text> */
			scanequal();
			i1 = scannumber();
			scancolon();
			scanname();
			h1 = scanstringlen;
			memcpy(textmask, scanstring, h1);  /* icon name */
			textmask[h1] = 0;
			scancolon();
			if (scanstart > scanend) scanstringlen = 0;
			else scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (openmitemtag == TRUE) {
					clientput("/>", -1);
					openmitemtag = FALSE;
				}
				elementTag = "submenu";
				sprintf(work, "<%s i=%d t=\"", elementTag, i1);
				clientput(work, -1);
				clientputdata((CHAR *) scanstring, scanstringlen);
				clientput("\" icon=\"", -1);
				clientput(interpretIconName((CHAR *)textmask), -1);
				clientput("\">", -1);
			}
/*			else if (guimenuiconsubmenu(i1, scanstring, scanstringlen, (CHAR*) textmask, h1)) goto makeresmenuerror2;*/
			else if (guimenusubmenu(i1, scanstring, scanstringlen, (CHAR*) textmask, h1)) goto makeresmenuerror2;
			submenudepth++;
			break;
		case KEYWORD_ENDSUBMENU:
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (openmitemtag == TRUE) {
					clientput("/>", -1);
					openmitemtag = FALSE;
				}
				sprintf(work, "</%s>", elementTag);
				clientput(work, -1);
			}
			else if (guimenuendsubmenu()) goto makeresmenuerror2;
			submenudepth--;
			break;
		case KEYWORD_KEY:
			if (i2 == KEYWORD_POPUPMENU) goto makeresmenuerror2;
			scanequal();
			scantext();
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput(" k=\"", -1);
				clientput((CHAR *) scanstring, -1);
				clientput("\"", -1);
				break;
			}
			switch(toupper(scanstring[0])) {
				case '^':  /* Control key A-Z */
					i1 = toupper(scanstring[1]);
					if (i1 < 'A' || i1 > 'Z' || scanstring[2]) i1 = 0;
					else i1 = i1 - 'A' + 601;
					break;
				case 'A': /* Alt key F1-F12 */
					if (!ctlmemicmp(scanstring, "ALT", 3)) {
						i1 = toupper(scanstring[3]);
						if (i1 == 'F' && scanstring[4]) {
							i1 = atoi((CHAR *) &scanstring[4]);
							if (i1 > 12) i1 = 0;
							else (i1 += 360);
						}
						else i1 = 0;
					}
					else i1 = 0;
					break;
				case 'C': /* CTRL-SHIFT F1-F12, A-Z; CTRL A-Z */
					if (!ctlmemicmp(scanstring, "CTRLSHIFT", 9)) {
						i1 = toupper(scanstring[9]);
						if (i1 == 'F' && scanstring[10]) {
							i1 = atoi((CHAR *) &scanstring[10]);
							if (i1 > 12) i1 = 0;
							else (i1 += 519);
						}
						else {
							i1 = toupper(scanstring[9]);
							if (i1 < 'A' || i1 > 'Z' || scanstring[10]) i1 = 0;
							else i1 = i1 - 'A' + 540;
						}
					}
					else if (!ctlmemicmp(scanstring, "CTRL", 4)) {
						i1 = toupper(scanstring[4]);
						if (scanstring[5] && (scanstring[5] != ' ')) {
							switch (i1) {
							case 'C':
								if (!ctlmemicmp(scanstring, "CTRLCOMMA", 9)) i1 = KEYCOMMA;
								else i1 = 0;
								break;
							case 'P':
								if (!ctlmemicmp(scanstring, "CTRLPERIOD", 10)) i1 = KEYPERIOD;
								else i1 = 0;
								break;
							case 'F':
								if (!ctlmemicmp(scanstring, "CTRLFSLASH", 10)) i1 = KEYFSLASH;
								else if (isdigit(scanstring[5])) {
									/* code here for CTRLF1 thru CTRLF12 */
									i1 = atoi((CHAR *) &scanstring[5]);
									if (i1 > 12) i1 = 0;
									else (i1 += 340);
								}
								else i1 = 0;
								break;
							case 'S':
								if (!ctlmemicmp(scanstring, "CTRLSEMICOLON", 13))
									i1 = KEYSEMICOLON;
								else
									i1 = 0;
								break;
							case 'L':
								if (!ctlmemicmp(scanstring, "CTRLLBRACKET", 12))
									i1 = KEYLBRACKET;
								else
									i1 = 0;
								break;
							case 'R':
								if (!ctlmemicmp(scanstring, "CTRLRBRACKET", 12))
									i1 = KEYRBRACKET;
								else
									i1 = 0;
								break;
							case 'B':
								if (!ctlmemicmp(scanstring, "CTRLBSLASH", 10))
									i1 = KEYBSLASH;
								else
									i1 = 0;
								break;
							case 'M':
								if (!ctlmemicmp(scanstring, "CTRLMINUS", 9))
									i1 = KEYMINUS;
								else
									i1 = 0;
								break;
							case 'E':
								if (!ctlmemicmp(scanstring, "CTRLEQUAL", 9))
									i1 = KEYEQUAL;
								else
									i1 = 0;
								break;
							case 'Q':
								if (!ctlmemicmp(scanstring, "CTRLQUOTE", 9))
									i1 = KEYQUOTE;
								else
									i1 = 0;
								break;
							default: i1 = 0;
							}
						}
						else if (i1 < 'A' || i1 > 'Z' || (scanstring[5] && (scanstring[5] != ' '))) {
							i1 = 0;
						}
						else i1 = i1 - 'A' + 601;
					}
					else i1 = 0;
					break;
				case 'D': /* Delete */
					if (!ctlmemicmp(scanstring, "DELETE", 6))
						i1 = KEYDELETE;
					else i1 = 0;
					break;
				case 'E': /* End */
					if (!ctlmemicmp(scanstring, "END", 3))
						i1 = KEYEND;
					else i1 = 0;
					break;
				case 'F':  /* Function key */
					i1 = atoi((CHAR *) &scanstring[1]);
					if (i1 > 12) i1 = 0;
					else (i1 += 300);
					break;
				case 'H': /* Home */
					if (!ctlmemicmp(scanstring, "HOME", 4))
						i1 = KEYHOME;
					else i1 = 0;
					break;
				case 'I': /* Insert */
					if (!ctlmemicmp(scanstring, "INSERT", 6))
						i1 = KEYINSERT;
					else i1 = 0;
					break;
				case 'P': /* Page Up/Down*/
					if (!ctlmemicmp(scanstring, "PGUP", 4))
						i1 = KEYPGUP;
					else if (!ctlmemicmp(scanstring, "PGDN", 4))
						i1 = KEYPGDN;
					else i1 = 0;
					break;
				case 'S': /* Shift key F1-F12 */
					if (!ctlmemicmp(scanstring, "SHIFT", 5)) {
						i1 = toupper(scanstring[5]);
						if (i1 == 'F' && scanstring[6]) {
							i1 = atoi((CHAR *) &scanstring[6]);
							if (i1 > 12) i1 = 0;
							else (i1 += 320);
						}
						else i1 = 0;
					}
					else i1 = 0;
					break;
				default:
					i1 = 0;
					break;
			}
			if (!i1) goto makeresmenuerror2;
			if (guimenukey(i1)) goto makeresmenuerror1;
			break;
		case KEYWORD_LINE:
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (openmitemtag == TRUE) {
					clientput("/>", -1);
					openmitemtag = FALSE;
				}
				clientput("<line/>", -1);
			}
			else if (guimenuline()) goto makeresmenuerror2;
			break;
		case KEYWORD_GRAY:
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				if (openmitemtag == TRUE) {
					clientput("/>", -1);
					openmitemtag = FALSE;
				}
				clientput("<gray/>", -1);
			}
			else if (guimenugray()) goto makeresmenuerror2;
			break;
		default:
			goto makeresmenuerror2;
		}
		if ((dbcflags & DBCFLAG_CLIENTINIT) && openmitemtag == TRUE) {
			openmitemtag = FALSE;
			clientput("/>", -1);
		}
	}
	if (submenudepth) dbcerrinfo(771, (UCHAR*)"Open submenus", 0);
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		if (openmitemtag == TRUE) clientput("/>", -1);
		if (openmaintag == TRUE) clientput("</main>", -1);
		clientput("</prep>", -1);
		if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
		e1 = (ELEMENT *) clientelement(); /* assume <r> element */
		if (e1 == NULL || ( a1 = e1->firstattribute ) != NULL) {
			if (a1 != NULL) {
				strcpy(work, a1->value);
				clientrelease();
				dbcerrinfo(771, (UCHAR*)work, -1);
			}
			else {
				clientrelease();
				dbcerror(771);
			}
			return;
		}
		if (keywordval == KEYWORD_POPUPMENU) openinfo->restype = RES_TYPE_POPUPMENU;
		else openinfo->restype = RES_TYPE_MENU;
		clientrelease();
	}
	else if (guimenuend()) dbcerror(771);
	return;

makeresmenuerror1:
	dbcerror(771);
	return;

makeresmenuerror2:
	dbcerrinfo(771, (UCHAR *) scanstring, scanstringlen);
	return;
}



/**
 * Output to Smart Client for those controls that have an item, and a size only
 */
static void cltputihv(CHAR *tag, INT item, INT h1, INT v1) {
	CHAR work[48];
	sprintf(work, "<%s i=%u hsize=%u vsize=%u/>", tag, item, h1, v1);
	clientput(work, -1);
}


/**
 * The TABLE prep string item looks like:
 *
 * 		table=<item>:<hsize>:<vsize>{:<coltitle>}
 *
 * At least one column title must appear.
 *
 * Upon entry to this routine, the equal sign has been scanned.
 */
static INT processTablePrepString() {
	INT i1, h1, v1, i3 = 0, colcounta, colcountb, keywordval;
	INT dropboxseen = FALSE, editseen = FALSE;

	i1 = scannumber();
	if (!i1) scanerror(776);
	scancolon();
	h1 = scannumber();
	if (!h1) scanerror(776);
	scancolon();
	v1 = scannumber();
	if (!v1) scanerror(776);
	scancolon();
	if ( (i1 = guipandlgtable(i1, h1, v1)) ) return i1;
	scantext();
	if ( (i1 = guipandlgtablecolumntitle(scanstring, scanstringlen)) ) return i1;
	colcounta = 1;
	while(scanstart <= scanend && *scanstart == ':') {
		scancolon();
		scantext();
		if ( (i1 = guipandlgtablecolumntitle(scanstring, scanstringlen)) ) return i1;
		colcounta++;
	}
	if (!scancomma()) scanerror(772);
	colcountb = 0;
	do {
		scanname();
		if ((keywordval = nametokeywordvalue()) == KEYWORD_TABLEEND) break;

		if (keywordval == KEYWORD_INSERTORDER) {
			if (!dropboxseen) scanerror(771);
			if ( (i1 = guipandlgtableinsertorder()) ) return i1;
			continue;
		}
		if (keywordval == KEYWORD_JUSTIFYLEFT) {
			if (!editseen) scanerror(771);
			if ( (i1 = guipandlgtablejustify(CTRLSTYLE_LEFTJUSTIFIED)) ) return i1;
			continue;
		}
		if (keywordval == KEYWORD_JUSTIFYRIGHT) {
			if (!editseen) scanerror(771);
			if ( (i1 = guipandlgtablejustify(CTRLSTYLE_RIGHTJUSTIFIED)) ) return i1;
			continue;
		}
		if (keywordval == KEYWORD_JUSTIFYCENTER) {
			if (!editseen) scanerror(771);
			if ( (i1 = guipandlgtablejustify(CTRLSTYLE_CENTERJUSTIFIED)) ) return i1;
			continue;
		}
		if (keywordval == KEYWORD_NOHEADER) {
			if ( (i1 = guipandlgtablenoheader()) ) return i1;
			continue;
		}

		if (colcountb == colcounta) scanerror(771);
		scanequal();
		i1 = scannumber();		/* item number */
		if (!i1) scanerror(776);
		scancolon();
		h1 = scannumber();		/* horz size */
		if (!h1) scanerror(776);
		switch (keywordval) {
		case KEYWORD_EDIT:
			editseen = TRUE;
			break;
		case KEYWORD_VTEXT: /* <item>:<hsize> */
		case KEYWORD_RVTEXT: /* <item>:<hsize> */
		case KEYWORD_CVTEXT: /* <item>:<hsize> */
		case KEYWORD_CHECKBOX:
		case KEYWORD_POPBOX:
			break;
		case KEYWORD_LEDIT: /* <item>:<hsize>:<max-chars> */
			editseen = TRUE; // @suppress("No break at end of case")
			// fall through
		case KEYWORD_DROPBOX:  /* <item>:<hsize>:<expanded-height> */
		case KEYWORD_CDROPBOX:  /* <item>:<hsize>:<expanded-height> */
			scancolon();
			i3 = scannumber();		/* extra data */
			if (!i3) scanerror(776);
			if (keywordval != KEYWORD_LEDIT) dropboxseen = TRUE;
			break;
		case KEYWORD_FEDIT: /* <item>:<hsize>:<mask> */
			scancolon();
			scantext();
			break;
		default:
			scanerror(771);
		}
		if ( (i1 = guipandlgtablecolumn(colcountb, i1, h1,
				keywordToPanelType(keywordval), i3, scanstring, scanstringlen)) )
			return i1;
		colcountb++;
	} while(scancomma());
	if (colcounta != colcountb || keywordval != KEYWORD_TABLEEND) scanerror(771);
	return 0;
}

/* Start with a default size of twenty columns */
#define TABLE_STRUCTURE_COLUMN_ARRAY_START_SIZE 20
#define TABLE_STRUCTURE_COLUMN_ARRAY_INCREMENT_SIZE 5

typedef struct tableColumnStruct {
	UCHAR ** columnTitle; /* Array of memalloc pointers to strings that are the column names */
	UCHAR **mask; /* We keep this but it is not really used */
	INT type;   /* One of the PANEL_xxxx type codes */
	INT item;   /* The DX item number */
	INT width;  /* In pixels */
	INT p3data; /* For PANEL_DROPBOX and PANEL_CDROPBOX it is height, for LEDIT it is maxlength */
	INT insertOrder; /* FALSE by default */
	INT justify; /* zero means what ever the default is */
} TABLE_COLUMN;

typedef struct tableHeaderStruct {
	INT sizeOfArray;        /* Actual size of the array, both titles and columns */
	INT numberOfColumns;	/* Slots in the array and the columns array that are in use, when finished building */
	INT itemNumber;
	INT hsize;
	INT vsize;
	INT noHeader;
	TABLE_COLUMN columns[TABLE_STRUCTURE_COLUMN_ARRAY_START_SIZE];
} TABLE, **PPTABLE;

static void addTableStructureColumnTitle(PPTABLE table) {
	TABLE_COLUMN *col;
	UCHAR **ptr;
	if ((*table)->numberOfColumns == (*table)->sizeOfArray) {
		(*table)->sizeOfArray += TABLE_STRUCTURE_COLUMN_ARRAY_INCREMENT_SIZE;
		if (memchange((UCHAR**)table,
				sizeof(struct tableHeaderStruct)
				+ ((*table)->sizeOfArray - TABLE_STRUCTURE_COLUMN_ARRAY_START_SIZE) * sizeof(TABLE_COLUMN),
				0)) dbcerror(ERR_NOMEM);
	}
	col = &(*table)->columns[(*table)->numberOfColumns];
	ptr = memalloc(scanstringlen + 1, 0);
	if (ptr == NULL) dbcerror(ERR_NOMEM);
	memcpy(*ptr, scanstring, scanstringlen);
	(*ptr)[scanstringlen] = '\0';
	col->columnTitle = ptr;
	(*table)->numberOfColumns++;
}

/**
 * table	 A double pointer to the table structure
 * item		 The DX item number of the column
 * width 	 The column width in pixels
 * panelType The PANEL_xxx value
 * p3data    Applies to drop boxes, and is the max length of ledit cells
 * mask      Applies only to fedit, not actually used by the system, treated like an ledit
 * masklen   The length of the mask string
 * index     The zero-based column index
 */
static void setTableStructureColumnData(PPTABLE table, INT item, INT width, INT panelType, INT p3data, CHAR *mask, INT masklen,
		INT index) {
	TABLE_COLUMN *col = &(*table)->columns[index];
	col->item = item;
	col->type = panelType;
	col->width = width;
	col->p3data = p3data;
	if (panelType == PANEL_FEDIT) {
		col->mask = memalloc(masklen + 1, 0);
		if (col->mask == NULL) dbcerror(ERR_NOMEM);
		memcpy(col->mask, mask, masklen);
		(*col->mask)[masklen] = '\0';
	}
}

static PPTABLE getNewTableStructure(void) {
	PPTABLE table = (PPTABLE)memalloc(sizeof(TABLE), MEMFLAGS_ZEROFILL);
	if (table == NULL) dbcerror(ERR_NOMEM);
	(*table)->sizeOfArray = TABLE_STRUCTURE_COLUMN_ARRAY_START_SIZE;
	return table;
}

static void freeTableStructure(PPTABLE table) {
	INT i1;
	for (i1 = 0; i1 < (*table)->numberOfColumns; i1++) {
		TABLE_COLUMN *col = &(*table)->columns[i1];
		memfree(col->columnTitle);
		if (col->type == PANEL_FEDIT) memfree(col->mask);
	}
	memfree((UCHAR**)table);
}

static void setTableStructureItemNumber(PPTABLE table, INT i1) {
	(*table)->itemNumber = i1;
}

static INT getTableStructureColumnCount(PPTABLE table) {
	return (*table)->numberOfColumns;
}

static void setTableStructureSize(PPTABLE table, INT h1, INT v1) {
	(*table)->hsize = h1;
	(*table)->vsize = v1;
}

static void getTableStructureTableElement(PPTABLE table, CHAR *work) {
	sprintf(work, "<table i=\"%d\" hsize=\"%d\" vsize=\"%d\" nh=\"%s\">",
			(*table)->itemNumber, (*table)->hsize, (*table)->vsize, ((*table)->noHeader) ? "true" : "false");
}

/**
 * Outputs directly to the 'sendbuffer' using
 * the clientput___ functions in dbcclnt.c
 */
static void getTableStructureColumnElement(PPTABLE table, INT colnum) {
	CHAR localwork[128];
	TABLE_COLUMN *col = &(*table)->columns[colnum];

	clientput("<c i=\"", 6);
	clientputnum(col->item);
	clientput("\" h=\"", 5);
	clientputnum(col->width);
	clientput("\" c=\"", 5);
	clientputnum(col->type);
	clientput("\" t=\"", 5);
	clientputdata((CHAR*)*col->columnTitle, -1);
	clientput("\"", 1);
#if 0
	sprintf(work, "<c i=\"%d\" h=\"%d\" c=\"%d\"", col->item, col->width, col->type);
	strcat(work, " t=\"");
	strcat(work, (CHAR*)*col->columnTitle);
	strcat(work, "\"");
#endif
	if (col->type == PANEL_DROPBOX || col->type == PANEL_CDROPBOX) {
		sprintf(localwork, " p3=\"%d\"", col->p3data);
		clientput(localwork, -1);
		sprintf(localwork, " order=\"%s\"", col->insertOrder ? "insert" : "alpha");
		clientput(localwork, -1);
	}
	if (col->type == PANEL_LEDIT) {
		sprintf(localwork, " p3=\"%d\"", col->p3data);
		clientput(localwork, -1);
	}
	if (col->type == PANEL_FEDIT) {
		sprintf(localwork, " p3=\"%s\"", *col->mask);
		clientput(localwork, -1);
	}
	if (col->type == PANEL_EDIT || col->type == PANEL_LEDIT) {
		if (col->justify != 0) {
			switch (col->justify) {
			case KEYWORD_JUSTIFYLEFT:
				clientput(" justify=\"left\"", -1);
				break;
			case KEYWORD_JUSTIFYRIGHT:
				clientput(" justify=\"right\"", -1);
				break;
			case KEYWORD_JUSTIFYCENTER:
				clientput(" justify=\"center\"", -1);
				break;
			}
		}
	}
	clientput("/>", 2);
}

static void applyTableStructureSpecialCode(PPTABLE table, INT keyword, INT colnum) {
	TABLE_COLUMN *col = &(*table)->columns[colnum];
	switch (keyword) {
		case KEYWORD_INSERTORDER:
			col->insertOrder = TRUE;
			break;
		case KEYWORD_JUSTIFYLEFT:
		case KEYWORD_JUSTIFYRIGHT:
		case KEYWORD_JUSTIFYCENTER:
			col->justify = keyword;
			break;
		case KEYWORD_NOHEADER:
			(*table)->noHeader = TRUE;
			break;
	}
}

/**
 * The TABLE prep string item looks like:
 *
 * 		table=<item>:<hsize>:<vsize>{:<coltitle>}
 *
 * At least one column title must appear.
 *
 * Upon entry to this routine, the equal sign has been scanned.
 *
 * The SC XML looks like:
 *
 * 	<table i=item hsize=hsize vsize=vsize nh=[ "true" | "false" ]>
 * 		<c i=item t=title h=width c=control type code>
 *  </table>
 *
 * The control type codes use the PANEL_* codes from gui.h
 *
 */
static void processTablePrepStringForSC() {
	INT i1, h1, v1, i3 = 0, colcounta, colcountb, keywordval;
	CHAR work[256];
	INT dropboxseen, editseen;
	PPTABLE table;

	table = getNewTableStructure();
	i1 = scannumber();
	if (!i1) scanerror(776);
	setTableStructureItemNumber(table, i1);
	scancolon();
	h1 = scannumber();
	if (!h1) scanerror(776);
	scancolon();
	v1 = scannumber();
	if (!v1) scanerror(776);
	setTableStructureSize(table, h1, v1);
	scancolon();
	scantext();
	addTableStructureColumnTitle(table);
	while (scanstart <= scanend && *scanstart == ':') {
		scancolon();
		scantext();
		addTableStructureColumnTitle(table);
	}
	if (!scancomma()) scanerror(772);

	colcounta = getTableStructureColumnCount(table);
	colcountb = 0;
	dropboxseen = -1;
	editseen = -1;
	do {
		scanname();
		if ((keywordval = nametokeywordvalue()) == KEYWORD_TABLEEND) break;

		switch (keywordval) {
		case KEYWORD_INSERTORDER:
			if (dropboxseen < 0) scanerror(771);
			applyTableStructureSpecialCode(table, KEYWORD_INSERTORDER, dropboxseen);
			continue;
		case KEYWORD_JUSTIFYLEFT:
			if (editseen < 0) scanerror(771);
			applyTableStructureSpecialCode(table, KEYWORD_JUSTIFYLEFT, editseen);
			continue;
		case KEYWORD_JUSTIFYRIGHT:
			if (editseen < 0) scanerror(771);
			applyTableStructureSpecialCode(table, KEYWORD_JUSTIFYRIGHT, editseen);
			continue;
		case KEYWORD_JUSTIFYCENTER:
			if (editseen < 0) scanerror(771);
			applyTableStructureSpecialCode(table, KEYWORD_JUSTIFYCENTER, editseen);
			continue;
		case KEYWORD_NOHEADER:
			applyTableStructureSpecialCode(table, KEYWORD_NOHEADER, -1);
			continue;
		}

		if (colcountb == colcounta) scanerror(771);
		scanequal();
		i1 = scannumber();
		if (!i1) scanerror(776);
		scancolon();
		h1 = scannumber();
		if (!h1) scanerror(776);

		switch (keywordval) {
		case KEYWORD_EDIT:
			editseen = colcountb;
			break;
		case KEYWORD_VTEXT:  /* <item>:<hsize> */
			// fall through
		case KEYWORD_RVTEXT: /* <item>:<hsize> */
			// fall through
		case KEYWORD_CVTEXT: /* <item>:<hsize> */
			// fall through
		case KEYWORD_CHECKBOX:
			// fall through
		case KEYWORD_POPBOX:
			break;
		case KEYWORD_LEDIT: /* <item>:<hsize>:<max-chars> */
			editseen = colcountb; // @suppress("No break at end of case")
			// fall through
		case KEYWORD_DROPBOX:   /* <item>:<hsize>:<expanded-height> */
			// fall through
		case KEYWORD_CDROPBOX:  /* <item>:<hsize>:<expanded-height> */
			scancolon();
			i3 = scannumber();
			if (!i3) scanerror(776);
			if (keywordval != KEYWORD_LEDIT) dropboxseen = colcountb;
			break;
		case KEYWORD_FEDIT: /* <item>:<hsize>:<mask> */
			scancolon();
			scantext();
			break;
		default:
			scanerror(771);
		}
		setTableStructureColumnData(table, i1, h1, keywordToPanelType(keywordval), i3, scanstring, scanstringlen, colcountb++);
	} while(scancomma());
	if (colcounta != colcountb || keywordval != KEYWORD_TABLEEND) scanerror(771);
	getTableStructureTableElement(table, work);
	clientput(work, -1);
	for (i1 = 0; i1 < colcountb; i1++ ) {
		getTableStructureColumnElement(table, i1);
	}
	clientput("</table>", 8);
	freeTableStructure(table);
	return;
}

static INT keywordToPanelType(INT kw) {
	switch (kw) {
	case KEYWORD_VTEXT: return PANEL_VTEXT;
	case KEYWORD_RVTEXT: return PANEL_RVTEXT;
	case KEYWORD_CVTEXT: return PANEL_CVTEXT;
	case KEYWORD_EDIT: return PANEL_EDIT;
	case KEYWORD_LEDIT: return PANEL_LEDIT;
	case KEYWORD_FEDIT: return PANEL_FEDIT;
	case KEYWORD_CHECKBOX: return PANEL_CHECKBOX;
	case KEYWORD_POPBOX: return PANEL_POPBOX;
	case KEYWORD_DROPBOX: return PANEL_DROPBOX;
	case KEYWORD_CDROPBOX: return PANEL_CDROPBOX;
	default: return RC_ERROR;
	}
}

static UINT getdrawvalue()
{
	UINT value;

	switch(pgm[pcount]) {
	case 0xFE:  /* 0 - 255 */
		pcount += 2;
		return((UINT) pgm[pcount-1]);
	case 0xFD:  /* -255 - 1 */
		pcount += 2;
		return(0 - (UINT) pgm[pcount-1]);
	case 0xFC:  /* > 255 < 64k */
		pcount += 3;
		value = (UINT) llhh(&pgm[pcount-2]);
		return(value);
	case 0xFB:  /* < 0 > -64k */
		pcount += 3;
		value = (UINT) llhh(&pgm[pcount-2]);
		return(0 - value);
	case 0xFA:  /* -1G to 1G - 1 */
		pcount += 5;
		value = (UINT) pgm[pcount - 4] + ((UINT) pgm[pcount - 3] << 8) + ((UINT) pgm[pcount - 2] << 16) + ((UINT) pgm[pcount - 1] << 24);
		return(value);
	default:
		return((UINT) nvtoi(getvar(VAR_READ)));
	}
}

/* start of scanning routines */
static void scaninit(UCHAR *str, INT len)
{
	scanstart = scanlastname = str;
	scanend = str + len - 1;
	if (!len) scanerror(771);
}

/* scan name up to non-alphanumeric character, strip leading and trailing blanks */
/* first character must be alpha, length must be > 0 and < 32 */
static void scanname()
{
	while (scanstart <= scanend && *scanstart == ' ') scanstart++;
	if (scanstart > scanend) {
		scanerror(771);
		return;
	}
	scanlastname = scanstart;
	if (!isalpha(*scanstart)) {
		scanerror(771);
		return;
	}
	scanstart++;
	while (scanstart <= scanend && (isalnum(*scanstart) || *scanstart == '_')) scanstart++;
	scanstringlen = (UINT)(ptrdiff_t)(scanstart - scanlastname);
	if (scanstringlen) {
		if (scanstringlen > 31) scanstringlen = 31;
		memcpy(scanstring, scanlastname, scanstringlen);
		scanstring[scanstringlen] = 0;
		while (scanstart <= scanend && *scanstart == ' ') scanstart++;
	}
	else scanerror(771);
}

/**
 * scan text up to comma, colon or end, including blanks, \ is forcing character
 * if text is > 255 characters long, scanstringlen is correct but only the
 * first 255 characters of the string are stored in scanstring
 */
static void scantext()
{
	INT i1;

	i1 = 0;
	if (scanstart > scanend) scanerror(771);
	while (scanstart <= scanend) {
		if (*scanstart == '\\') {
			if (scanstart++ == scanend) scanerror(777);
			if (i1 < 256) scanstring[i1] = *scanstart++;
			else scanstart++;
			i1++;
			continue;
		}
		if (*scanstart == ',' || *scanstart == ':') break;
		if (i1 < 256) scanstring[i1] = *scanstart++;
		else scanstart++;
		i1++;
	}
	scanstringlen = i1;
	if (i1 > 255) i1 = 255;
	scanstring[i1] = '\0';
}

/* scan font name, strip leading and trailing blanks */
static void scanfont()
{
	UINT i1;
	UCHAR *p1;

	while (scanstart <= scanend && *scanstart == ' ') scanstart++;
	if (scanstart > scanend || (!isalpha(*scanstart) && *scanstart != '(')) {
		scanerror(778);
		return;
	}
	p1 = scanstart;
	while (scanstart <= scanend && (isalnum(*scanstart) || *scanstart == ' ')) scanstart++;
	if (*scanstart == '(') {
		scanstart++;
		while (scanstart <= scanend && (isalnum(*scanstart) || *scanstart == ' ' || *scanstart == ',')) scanstart++;
		if (*scanstart != ')') {
			scanerror(778);
			return;
		}
		scanstart++;
	}
	i1 = (UINT)(ptrdiff_t)(scanstart - p1);
	if (i1 > 255) i1 = 255;
	memcpy(scanstring, p1, i1);
	scanstring[i1] = '\0';
	scanstringlen = i1;
	while (scanstart <= scanend && *scanstart == ' ') scanstart++;
}

/**
 * scan number, strip leading and trailing blanks
 * @return the number, or zero if no number is found
 */
static INT scannumber()
{
	INT i1;

	i1 = 0;
	while (scanstart <= scanend && *scanstart == ' ') scanstart++;
	if (scanstart > scanend || !isdigit(*scanstart)) scanerror(776);
	while (scanstart <= scanend && isdigit(*scanstart)) i1 = i1 * 10 + *scanstart++ - '0';
	while (scanstart <= scanend && *scanstart == ' ') scanstart++;
	return(i1);
}

/* scan comma or end-of-string, return TRUE if comma, FALSE if end-of-string */
static INT scancomma()
{
	if (scanstart <= scanend) {
		if (*scanstart++ == ',') return(TRUE);
		scanerror(772);
	}
	return(FALSE);
}

/* scan colon */
static void scancolon()
{
	if (scanstart > scanend) scanerror(774);
	else if (*scanstart++ != ':') scanerror(774);
}

/* scan equal sign */
static void scanequal()
{
	if (scanstart > scanend) scanerror(773);
	else if (*scanstart++ != '=') scanerror(773);
}

/* scan routines error */
static void scanerror(INT errnum)
{
	UINT i1;

	i1 = (UINT)(ptrdiff_t)(scanstart - scanlastname) + 1;
	if (i1 < 8) {
		i1 = (UINT)(ptrdiff_t)(scanend - scanlastname) + 1;
		if (i1 > 8) i1 = 8;
	}
	dbcerrinfo(errnum, scanlastname, (INT)i1);
	scanstart = scanend + 1;
}

/* Return token index where str is token. Return -1 if not found */
static INT nametokeywordvalue()
{
	if (!scanstringlen) return RC_ERROR;
	switch (toupper(scanstring[0])) {
	case 'A':
		if (scanstringlen == 5 && !ctlmemicmp(scanstring, "ATEXT", 5)) return(KEYWORD_ATEXT);
		if (scanstringlen == 8 && !ctlmemicmp(scanstring, "ALERTDLG", 8)) return(KEYWORD_ALERTDLG);
		break;
	case 'B':
		if (scanstringlen == 3 && !ctlmemicmp(scanstring, "BOX", 3)) return(KEYWORD_BOX);
		if (scanstringlen == 6 && !ctlmemicmp(scanstring, "BUTTON", 6)) return(KEYWORD_BUTTON);
		if (scanstringlen == 7 && !ctlmemicmp(scanstring, "BOXTABS", 7)) return(KEYWORD_BOXTABS);
		if (scanstringlen == 8 && !ctlmemicmp(scanstring, "BOXTITLE", 8)) return(KEYWORD_BOXTITLE);
		if (scanstringlen == 11 && !ctlmemicmp(scanstring, "BUTTONGROUP", 11)) return(KEYWORD_BUTTONGROUP);
		break;
	case 'C':
		if (scanstringlen == 5 && !ctlmemicmp(scanstring, "CTEXT", 5)) return(KEYWORD_CTEXT);
		if (scanstringlen == 6 && !ctlmemicmp(scanstring, "CATEXT", 6)) return(KEYWORD_CATEXT);
		if (scanstringlen == 6 && !ctlmemicmp(scanstring, "CVTEXT", 6)) return(KEYWORD_CVTEXT);
		if (scanstringlen == 8) {
			if (!ctlmemicmp(scanstring, "CHECKBOX", 8)) return(KEYWORD_CHECKBOX);
			if (!ctlmemicmp(scanstring, "COLORDLG", 8)) return(KEYWORD_COLORDLG);
			if (!ctlmemicmp(scanstring, "CDROPBOX", 8)) return(KEYWORD_CDROPBOX);
			/*
			 * We may try this again in 14.1
			if (!ctlmemicmp(scanstring, "COMBOBOX", 8)) return(KEYWORD_COMBOBOX);
			*/
		}
		if (scanstringlen == 9) {
			 if (!ctlmemicmp(scanstring, "COLORBITS", 9)) return(KEYWORD_COLORBITS);
			 if (!ctlmemicmp(scanstring, "CHECKITEM", 9)) return(KEYWORD_CHECKITEM);
		}
		break;
	case 'D':
		if (scanstringlen == 6 && !ctlmemicmp(scanstring, "DIALOG", 6)) return(KEYWORD_DIALOG);
		if (scanstringlen == 7) {
			if (!ctlmemicmp(scanstring, "DROPBOX", 7)) return(KEYWORD_DROPBOX);
			if (!ctlmemicmp(scanstring, "DEFAULT", 7)) return(KEYWORD_DEFAULT);
		}
		if (scanstringlen == 9) {
			if (!ctlmemicmp(scanstring, "DDECLIENT", 9)) return(KEYWORD_DDE);
			if (!ctlmemicmp(scanstring, "DDESERVER", 9)) return(KEYWORD_DDE);
		}
		if (scanstringlen == 12 && !ctlmemicmp(scanstring, "DEVICEFILTER", 12)) return(KEYWORD_DEVICEFILTER);
		if (scanstringlen == 13 && !ctlmemicmp(scanstring, "DEFPUSHBUTTON", 13)) return(KEYWORD_DEFPUSHBUTTON);
		break;
	case 'E':
		if (scanstringlen == 4 && !ctlmemicmp(scanstring, "EDIT", 4)) return(KEYWORD_EDIT);
		if (scanstringlen == 10 && !ctlmemicmp(scanstring, "ENDSUBMENU", 10)) return(KEYWORD_ENDSUBMENU);
		if (scanstringlen == 13 && !ctlmemicmp(scanstring, "ESCPUSHBUTTON", 13)) return(KEYWORD_ESCPUSHBUTTON);
		if (scanstringlen == 14 && !ctlmemicmp(scanstring, "ENDBUTTONGROUP", 14)) return(KEYWORD_ENDBUTTONGROUP);
		break;
	case 'F':
		if (scanstringlen == 4 && !ctlmemicmp(scanstring, "FONT", 4)) return(KEYWORD_FONT);
		if (scanstringlen == 5 && !ctlmemicmp(scanstring, "FEDIT", 5)) return(KEYWORD_FEDIT);
		if (scanstringlen == 7) {
			if (!ctlmemicmp(scanstring, "FIXSIZE", 7)) return(KEYWORD_FIXSIZE);
			if (!ctlmemicmp(scanstring, "FONTDLG", 7)) return(KEYWORD_FONTDLG);
		}
		if (scanstringlen == 11 && !ctlmemicmp(scanstring, "FLOATWINDOW", 11)) return(KEYWORD_FLOATWINDOW);
		break;
	case 'G':
		if (scanstringlen == 4 && !ctlmemicmp(scanstring, "GRAY", 4)) return(KEYWORD_GRAY);
		break;
	case 'H':
		if (scanstringlen == 1) return(KEYWORD_H);
		if (scanstringlen == 2 && toupper(scanstring[1]) == 'A') return(KEYWORD_HA);
		if (scanstringlen == 8 && !ctlmemicmp(scanstring, "HELPTEXT", 8)) return(KEYWORD_HELPTEXT);
		if (scanstringlen == 10 && !ctlmemicmp(scanstring, "HSCROLLBAR", 10)) return(KEYWORD_HSCROLLBAR);
		break;
	case 'I':
		if (scanstringlen == 4) {
			if (!ctlmemicmp(scanstring, "ITEM", 4)) return(KEYWORD_ITEM);
			if (!ctlmemicmp(scanstring, "ICON", 4)) return(KEYWORD_ICON);
		}
		if (scanstringlen == 8 && !ctlmemicmp(scanstring, "ICONITEM", 8)) return(KEYWORD_ICONITEM);
		if (scanstringlen == 11) {
			 if (!ctlmemicmp(scanstring, "INSERTORDER", 11)) return(KEYWORD_INSERTORDER);
			 if (!ctlmemicmp(scanstring, "ICONSUBMENU", 11)) return(KEYWORD_ICONSUBMENU);
		}
		if (scanstringlen == 14) {
			if (!ctlmemicmp(scanstring, "ICONPUSHBUTTON", 14)) return(KEYWORD_ICONPUSHBUTTON);
			if (!ctlmemicmp(scanstring, "ICONLOCKBUTTON", 14)) return(KEYWORD_ICONLOCKBUTTON);
		}
		if (scanstringlen == 17) {
			if (!ctlmemicmp(scanstring, "ICONDEFPUSHBUTTON", 17)) return(KEYWORD_ICONDEFPUSHBUTTON);
			if (!ctlmemicmp(scanstring, "ICONESCPUSHBUTTON", 17)) return(KEYWORD_ICONESCPUSHBUTTON);
		}
		break;
	case 'J':
		if (scanstringlen == 11) {
			 if (!ctlmemicmp(scanstring, "JUSTIFYLEFT", scanstringlen)) return(KEYWORD_JUSTIFYLEFT);
		}
		if (scanstringlen == 12) {
			 if (!ctlmemicmp(scanstring, "JUSTIFYRIGHT", scanstringlen)) return(KEYWORD_JUSTIFYRIGHT);
		}
		if (scanstringlen == 13) {
			 if (!ctlmemicmp(scanstring, "JUSTIFYCENTER", scanstringlen)) return(KEYWORD_JUSTIFYCENTER);
		}
		break;
	case 'K':
		if (scanstringlen == 3 && !ctlmemicmp(scanstring, "KEY", 3)) return(KEYWORD_KEY);
		break;
	case 'L':
		if (scanstringlen == 4 && !ctlmemicmp(scanstring, "LINE", 4)) return(KEYWORD_LINE);
		if (scanstringlen == 5 && !ctlmemicmp(scanstring, "LEDIT", 5)) return(KEYWORD_LEDIT);
		if (scanstringlen == 7 && !ctlmemicmp(scanstring, "LISTBOX", 7)) return(KEYWORD_LISTBOX);
		if (scanstringlen == 9 && !ctlmemicmp(scanstring, "LISTBOXHS", 9)) return(KEYWORD_LISTBOXHS);
		if (scanstringlen == 10 && !ctlmemicmp(scanstring, "LOCKBUTTON", 10)) return(KEYWORD_LOCKBUTTON);
		if (scanstringlen == 10 && !ctlmemicmp(scanstring, "LTCHECKBOX", 10)) return(KEYWORD_LTCHECKBOX);
		break;
	case 'M':
		if (scanstringlen == 4) {
			if (!ctlmemicmp(scanstring, "MAIN", 4)) return(KEYWORD_MAIN);
			if (!ctlmemicmp(scanstring, "MENU", 4)) return(KEYWORD_MENU);
		}
		if (scanstringlen == 5 && !ctlmemicmp(scanstring, "MEDIT", 5)) return(KEYWORD_MEDIT);
		if (scanstringlen == 7 && !ctlmemicmp(scanstring, "MEDITHS", 7)) return(KEYWORD_MEDITHS);
		if (scanstringlen == 7 && !ctlmemicmp(scanstring, "MEDITVS", 7)) return(KEYWORD_MEDITVS);
		if (scanstringlen == 6 && !ctlmemicmp(scanstring, "MEDITS", 6)) return(KEYWORD_MEDITS);
		if (scanstringlen == 6 && !ctlmemicmp(scanstring, "MLEDIT", 6)) return(KEYWORD_MLEDIT);
		if (scanstringlen == 8 && !ctlmemicmp(scanstring, "MLEDITHS", 8)) return(KEYWORD_MLEDITHS);
		if (scanstringlen == 8 && !ctlmemicmp(scanstring, "MLEDITVS", 8)) return(KEYWORD_MLEDITVS);
		if (scanstringlen == 7 && !ctlmemicmp(scanstring, "MLEDITS", 7)) return(KEYWORD_MLEDITS);
		if (scanstringlen == 8 && !ctlmemicmp(scanstring, "MAXIMIZE", 8)) return(KEYWORD_MAXIMIZE);
		if (scanstringlen == 8 && !ctlmemicmp(scanstring, "MLISTBOX", 8)) return(KEYWORD_MLISTBOX);
		if (scanstringlen == 8 && !ctlmemicmp(scanstring, "MULTIPLE", 8)) return(KEYWORD_MULTIPLE);
		if (scanstringlen == 10 && !ctlmemicmp(scanstring, "MLISTBOXHS", 10)) return(KEYWORD_MLISTBOXHS);
		break;
	case 'N':
		if (scanstringlen == 7 && !ctlmemicmp(scanstring, "NOCLOSE", 7)) return(KEYWORD_NOCLOSE);
		if (scanstringlen == 7 && !ctlmemicmp(scanstring, "NOFOCUS", 7)) return(KEYWORD_NOFOCUS);
		if (scanstringlen == 8 && !ctlmemicmp(scanstring, "NOHEADER", 8)) return(KEYWORD_NOHEADER);
		if (scanstringlen == 12 && !ctlmemicmp(scanstring, "NOSCROLLBARS", 12)) return(KEYWORD_NOSCROLLBARS);
		if (scanstringlen == 15 && !ctlmemicmp(scanstring, "NOTASKBARBUTTON", 15)) return(KEYWORD_NOTASKBARBUTTON);
		break;
	case 'O':
		if (scanstringlen == 11 && !ctlmemicmp(scanstring, "OPENFILEDLG", 11)) return(KEYWORD_OPENFILEDLG);
		if (scanstringlen == 10 && !ctlmemicmp(scanstring, "OPENDIRDLG", 10)) return(KEYWORD_OPENDIRDLG);
		if (scanstringlen == 5 && !ctlmemicmp(scanstring, "OWNER", 5)) return(KEYWORD_OWNER);
		break;
	case 'P':
		if (scanstringlen == 3 && !ctlmemicmp(scanstring, "POS", 3)) return(KEYWORD_POS);
		if (scanstringlen == 5) {
			if (!ctlmemicmp(scanstring, "PANEL", 5)) return(KEYWORD_PANEL);
			if (!ctlmemicmp(scanstring, "PEDIT", 5)) return(KEYWORD_PEDIT);
		}
		if (scanstringlen == 6) {
			if (!ctlmemicmp(scanstring, "PIXELS", 6)) return(KEYWORD_PIXELS);
			if (!ctlmemicmp(scanstring, "PLEDIT", 6)) return(KEYWORD_PLEDIT);
			if (!ctlmemicmp(scanstring, "POPBOX", 6)) return(KEYWORD_POPBOX);
		}
		if (scanstringlen == 9 && !ctlmemicmp(scanstring, "POPUPMENU", 9)) return(KEYWORD_POPUPMENU);
		if (scanstringlen == 10 && !ctlmemicmp(scanstring, "PUSHBUTTON", 10)) return(KEYWORD_PUSHBUTTON);
		if (scanstringlen == 11) {
			if (!ctlmemicmp(scanstring, "PROGRESSBAR", 11)) return(KEYWORD_PROGRESSBAR);
			if (!ctlmemicmp(scanstring, "PANDLGSCALE", 11)) return(KEYWORD_PANDLGSCALE);
		}
		break;
	case 'R':
		if (scanstringlen == 5 && !ctlmemicmp(scanstring, "RTEXT", 5)) return(KEYWORD_RTEXT);
		if (scanstringlen == 6 && !ctlmemicmp(scanstring, "RVTEXT", 6)) return(KEYWORD_RVTEXT);
		if (scanstringlen == 6 && !ctlmemicmp(scanstring, "RATEXT", 6)) return(KEYWORD_RATEXT);
		if (scanstringlen == 8 && !ctlmemicmp(scanstring, "READONLY", 6)) return(KEYWORD_READONLY);
		break;
	case 'S':
		if (scanstringlen == 4 && !ctlmemicmp(scanstring, "SIZE", 4)) return(KEYWORD_SIZE);
		if (scanstringlen == 5 && !ctlmemicmp(scanstring, "SPACE", 5)) return(KEYWORD_SPACE);
		if (scanstringlen == 7 && !ctlmemicmp(scanstring, "SUBMENU", 7)) return(KEYWORD_SUBMENU);
		if (scanstringlen == 8 && !ctlmemicmp(scanstring, "SHOWONLY", 8)) return(KEYWORD_SHOWONLY);
		if (scanstringlen == 13 && !ctlmemicmp(scanstring, "SAVEASFILEDLG", 13)) return(KEYWORD_SAVEASFILEDLG);
		if (scanstringlen == 9 && !ctlmemicmp(scanstring, "STATUSBAR", 9)) return(KEYWORD_STATUSBAR);
		break;
	case 'T':
		if (scanstringlen == 3 && !ctlmemicmp(scanstring, "TAB", 3)) return(KEYWORD_TAB);
		if (scanstringlen == 4) {
			if (!ctlmemicmp(scanstring, "TEXT", 4)) return(KEYWORD_TEXT);
			if (!ctlmemicmp(scanstring, "TREE", 4)) return(KEYWORD_TREE);
		}
		if (scanstringlen == 5) {
			if (!ctlmemicmp(scanstring, "TABLE", 5)) return(KEYWORD_TABLE);
			if (!ctlmemicmp(scanstring, "TITLE", 5)) return(KEYWORD_TITLE);
			if (!ctlmemicmp(scanstring, "TIMER", 5)) return(KEYWORD_TIMER);
		}
		if (scanstringlen == 7 && !ctlmemicmp(scanstring, "TOOLBAR", 7)) return(KEYWORD_TOOLBAR);
		if (scanstringlen == 8) {
			 if (!ctlmemicmp(scanstring, "TABGROUP", 8)) return(KEYWORD_TABGROUP);
			 if (!ctlmemicmp(scanstring, "TABLEEND", 8)) return(KEYWORD_TABLEEND);
		}
		if (scanstringlen == 9 && !ctlmemicmp(scanstring, "TEXTCOLOR", 9)) return(KEYWORD_TEXTCOLOR);
		if (scanstringlen == 11 && !ctlmemicmp(scanstring, "TABGROUPEND", 11)) return(KEYWORD_TABGROUPEND);
		if (scanstringlen == 14 && !ctlmemicmp(scanstring, "TEXTPUSHBUTTON", 14)) return(KEYWORD_TEXTPUSHBUTTON);
		break;
	case 'V':
		if (scanstringlen == 1) return(KEYWORD_V);
		if (scanstringlen == 2 && toupper(scanstring[1]) == 'A') return(KEYWORD_VA);
		if (scanstringlen == 5) {
			if (!ctlmemicmp(scanstring, "VICON", 5)) return(KEYWORD_VICON);
			if (!ctlmemicmp(scanstring, "VTEXT", 5)) return(KEYWORD_VTEXT);
		}
		if (scanstringlen == 10 && !ctlmemicmp(scanstring, "VSCROLLBAR", 10)) return(KEYWORD_VSCROLLBAR);
		break;
	case 'W':
		if (scanstringlen == 6 && !ctlmemicmp(scanstring, "WINDOW", 6)) return(KEYWORD_WINDOW);
		break;
	}
	return RC_ERROR;
}

/**
 * Assign an OPENINFO structure.
 * Return an index to it in refnum.
 * N.B. The returned value of refnum is a ONE-BASED index
 * into the list headed by openinfopptr.
 *
 * Start by searching all existing structures for an unused one. davbtype will be zero if unused.
 * If an unused one is not found then allocate a new one.
 * For efficiency we allocate them 8 at a time.
 */
static INT getnewrefnum(INT *refnum)
{
	INT i1, i2;
	OPENINFO *openinfo;

	openinfo = *openinfopptr;
	for (i1 = 0; i1 < openinfohi && openinfo->davbtype; i1++, openinfo++);
	if (i1 == openinfohi) {
		if (openinfohi == openinfomax) {
			i2 = memchange((UCHAR **) openinfopptr, (openinfomax + 8) * sizeof(OPENINFO), MEMFLAGS_ZEROFILL);
			setpgmdata();
			if (i2) return RC_ERROR;
			openinfomax += 8;
		}
		openinfohi++;
	}
	*refnum = i1 + 1;
	return(0);
}

/**
 * refnum is the ONE-BASED index into the openinfo array (openinfopptr)
 */
static void closerefnum(INT refnum, INT closeall)
{
	INT i1;
	OPENINFO *openinfo;
	CHAR *errmsg;

	/* caller is responsible for resetting memory */
	if (!refnum) return;
	openinfo = *openinfopptr + refnum - 1;
	switch(openinfo->davbtype) {
	case DAVB_QUEUE:
		if (!closeall) return;
		i1 = openinfo->v.qvareventid;
		quedestroy(openinfo->u.quehandle);
		evtdestroy(i1);
		break;
	case DAVB_RESOURCE:
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			if (openinfo->restype == RES_TYPE_MENU) clientput("<close menu=", -1);
			else if (openinfo->restype == RES_TYPE_TOOLBAR) clientput("<close toolbar=", -1);
			else if (openinfo->restype == RES_TYPE_DIALOG) clientput("<close dialog=", -1);
			else if (openinfo->restype == RES_TYPE_PANEL) clientput("<close panel=", -1);
			else if (openinfo->restype == RES_TYPE_POPUPMENU) clientput("<close popupmenu=", -1);
			else if (openinfo->restype == RES_TYPE_ICON) clientput("<close icon=", -1);
			else if (openinfo->restype == RES_TYPE_ALERTDLG) clientput("<close alertdlg=", -1);
			else break;
			clientput(openinfo->ident, -1);
			clientput("/>", -1);
			if (clientsend(0, 0) < 0) dbcerrinfo(798, NULL, 0);
		}
		else {
			i1 = guiresdestroy(openinfo->u.reshandle);
			if (i1 == RC_ERROR) {
				errmsg = guiGetLastErrorMessage();
				if (strlen(errmsg) != 0) {
					dbcerrinfo(1698, (UCHAR*)errmsg, -1);
				}
			}
		}
		break;
	case DAVB_IMAGE:
		if (!closeall) return;
		if (dbcflags & DBCFLAG_CLIENTINIT) {
			clientput("<close image=", -1);
			clientput(openinfo->name, -1);
			clientput("/>", -1);
			if (clientsend(0, 0) < 0) dbcerrinfo(798, NULL, 0);
			/* find image structure in linked list and remove it */
		}
		guipixdestroy(openinfo->u.pixhandle);
		break;
	case DAVB_DEVICE:
		switch(openinfo->devtype) {
		case DEV_DSR:
			dsrclose(openinfo->u.inthandle);
			break;
		case DEV_WINDOW:
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<close window=\"", -1);
				clientput(openinfo->ident, -1);
				clientput("\"/>", -1);
				if (clientsend(0, 0) < 0) dbcerrinfo(798, NULL, 0);
			}
			else {
				i1 = guiwindestroy(openinfo->u.winhandle);
				if (i1 == RC_ERROR) {
					errmsg = guiGetLastErrorMessage();
					if (strlen(errmsg) != 0) {
						openinfo = *openinfopptr + refnum - 1;
						openinfo->davbtype = 0;
						dbcerrinfo(1698, (UCHAR*)errmsg, -1);
					}
				}
			}
			break;
		case DEV_TIFFILE:
		case DEV_GIFFILE:
		case DEV_JPGFILE:
		case DEV_PNGFILE:
			if (openinfo->v.srvhandle > 0) {
				fsclose(openinfo->u.inthandle);
			}
			else fioclose(openinfo->u.inthandle);
			break;
		case DEV_TIMER:
			devtimerdestroy(openinfo->u.timerhandle);
			break;
		case DEV_APPLICATION:
			if (dbcflags & DBCFLAG_CLIENTINIT) {
				clientput("<close application=0/>", -1);
				if (clientsend(0, 0) < 0) dbcerrinfo(798, NULL, 0);
			}
			else guiAppDestroy();
			break;
#if OS_WIN32
		case DEV_DDE:
			guiddestop(openinfo->u.inthandle);
			break;
#endif
		}
	}
	openinfo = *openinfopptr + refnum - 1;
	openinfo->davbtype = 0;
}

/**
 * generic application change
 */
static void ctlappchange(UCHAR *func, INT funclen, UCHAR *data, INT datalen)
{
	CHAR cmd[20], work[128];
	INT i1, pos;
	ELEMENT *e1;
	ATTRIBUTE *a1;

	/* convert command to lower case */
	for (pos = 0; pos < funclen && func[pos] != ' ' && !isdigit(func[pos]); pos++) {
		cmd[pos] = tolower(func[pos]);
	}
	cmd[pos] = '\0';
	if (dbcflags & DBCFLAG_CLIENTINIT) {
		clientput("<change application=0", -1);
		clientput(" f=\"", -1);
		clientput(cmd, -1);
		clientput("\"", -1);
		if (datalen != 0) {
			clientput(" l=\"", -1);
			data[datalen] = '\0';
			if (!memcmp(cmd, "consolewindow", 13)) {
				for (i1 = 0; i1 < datalen; i1++) data[i1] = tolower(data[i1]);
				if (datalen == 3 && data[0] == 'o' && data[1] == 'f' && data[2] == 'f') {
					kdsexit();
					dbcflags &= ~(DBCFLAG_VIDINIT | DBCFLAG_KDSINIT);
				}
			}
			if (!strcmp(cmd, "desktopicon") || !strcmp(cmd, "dialogicon")) {
				interpretIconName((CHAR *)data);
			}
			clientputdata((CHAR*) data, -1);
			clientput("\"", -1);
		}
		clientput("/>", -1);
		if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
		e1 = (ELEMENT *) clientelement(); /* assume <r> element */
		if (e1 == NULL) {
			clientrelease();
			dbcerrinfo(798, NULL, 0);
		}
		else if ( (a1 = e1->firstattribute) != NULL) {
			strcpy(work, a1->value);
			clientrelease();
			dbcerrinfo(794, (UCHAR*)work, -1);
		}
		clientrelease();
	}
	else {
		if (!strcmp(cmd, "consolewindow")) {
			if (datalen == 3 && tolower(data[0]) == 'o' && tolower(data[1]) == 'f' && tolower(data[2]) == 'f') {
				/* If the character mode debugger is running, this command must be ignored */
				if (!(dbcflags & DBCFLAG_DEBUG) || (dbcflags & DBCFLAG_DDTDEBUG)) {
					kdsexit();
					dbcflags &= ~(DBCFLAG_VIDINIT | DBCFLAG_KDSINIT);
				}
			}
		}
		else if (guiappchange(func, funclen, data, datalen) != 0) {
			dbcerror(794);
		}
	}
}

static void ctltimerchange(OPENINFO *openinfo, UCHAR *func, INT funclen, UCHAR *data, INT datalen)	/* timer device change */
{
#define MAXSECSIZE	5
	INT curpos;
	DOUBLE seconds;
	UINT centiseconds;
	CHAR secstr[MAXSECSIZE + 1];
	UCHAR timestamp[16];

	if (!ISVALIDTIMERHANDLE(openinfo->u.timerhandle)) dbcerror(791);	/* device timer must be preped first */
	if (3 == funclen &&
	    toupper(func[0]) == 'S' && toupper(func[1]) == 'E' && toupper(func[2]) == 'T') {
		if (datalen > 16 || datalen < 12) dbcerror(794);
		memcpy(timestamp, data, datalen);
		if (datalen < 16) memset(&timestamp[datalen], '0', 16 - datalen);
		openinfo->timestep = 0;
		if (devtimerset(openinfo->u.timerhandle, timestamp, 0) < 0) dbcerror(794);
		return;
	}
	if (funclen >= 5 && toupper(func[0]) == 'M' && toupper(func[1]) == 'S'
	    && toupper(func[2]) == 'E' && toupper(func[3]) == 'T' && (isdigit(func[4]) || func[4] == '.'))
	{
		if (datalen > 16 || (datalen && datalen < 12)) dbcerror(794);
		for (curpos = 0; curpos < MAXSECSIZE && curpos < funclen - 4; curpos++) secstr[curpos] = func[4 + curpos];
		secstr[curpos] = '\0';
		seconds = strtod(secstr, NULL);
		centiseconds = (UINT)(seconds * 100);
		if (!datalen) {
			msctimestamp(timestamp);
			timadd(timestamp, centiseconds);
		}
		else {
			memcpy(timestamp, data, datalen);
			if (datalen < 16) memset(&timestamp[datalen], '0', 16 - datalen);
		}
		if (devtimerset(openinfo->u.timerhandle, timestamp, centiseconds) < 0) dbcerror(794);
		return;
	}
	if (5 == funclen && !ctlmemicmp(func, "CLEAR", 5)) {
		devtimerclear(openinfo->u.timerhandle);
		return;
	}
	dbcerror(794);
}

static INT devtimercreate(UCHAR *name, DEVTIMERHANDLE *thptr, INT refnum)
{
	DEVTIMERHANDLE th;
	DEVTIMER *devtimer;

	*thptr = INVALIDTIMERHANDLE;

	/*If the free list has anything on it, use the first one*/
	if (ISVALIDTIMERHANDLE(devtimerfreelist)) {
	 	th = devtimerfreelist;
		devtimer = DEVTIMERHANDLETOPTR(th);
		devtimerfreelist = devtimer->next;
	}
	else if (devtimernext == devtimeralloc) {
		if (NULL == devtimerstorage) {
			devtimerstorage = (DEVTIMER **) memalloc(sizeof(DEVTIMER) * DEVTIMERALLOCAMT, 0);
			if (NULL == devtimerstorage) return(RC_NO_MEM);
			devtimeralloc = DEVTIMERALLOCAMT;
			devtimernext = 0;
		}
		else {
			if (memchange((UCHAR **) devtimerstorage, sizeof(DEVTIMER) * (devtimeralloc + DEVTIMERALLOCAMT), 0) < 0) return(RC_NO_MEM);
			devtimeralloc += DEVTIMERALLOCAMT;
		}
		th = devtimernext++;
		devtimer = DEVTIMERHANDLETOPTR(th);
	}
	else {
		th = devtimernext++;
		devtimer = DEVTIMERHANDLETOPTR(th);
	}

	devtimer->next = devtimerlist;
	devtimerlist = th;
	memcpy(devtimer->name, name, MAXTIMERNAMELEN);
	devtimer->refnum = refnum;
	devtimer->activeflg = FALSE;
	*thptr = th;
	return(0);
}

/**
 * Change in DX 15, timestep is now in centiseconds
 */
static INT devtimerset(DEVTIMERHANDLE th, UCHAR *timestamp, UINT timestep)
{
	DEVTIMER *devtimer;

	if (!ISVALIDTIMERHANDLE(th)) {
		return RC_ERROR;
	}
	devtimer = DEVTIMERHANDLETOPTR(th);
	devtimer->activeflg = FALSE;

	if (memcmp(timestamp, devexpiretime, 16) < 0) {
		if (devtimerhandle > 0) timstop(devtimerhandle);
		devtimerhandle = timset(timestamp, maineventids[EVENT_DEVTIMER]);
		if (devtimerhandle < 0) {
			devtimerhandle = timset(devexpiretime, maineventids[EVENT_DEVTIMER]);
			if (devtimerhandle < 0) memcpy(devexpiretime, TIMMAXTIME, 16);
			return RC_ERROR;
		}
		memcpy(devexpiretime, timestamp, 16);
	}

	memcpy(devtimer->expiretime, timestamp, 16);
	devtimer->timestep = timestep;
	devtimer->activeflg = TRUE;
	return(0);
}

static void devtimerclear(DEVTIMERHANDLE th)
{
	DEVTIMER *devtimer;

	if (!ISVALIDTIMERHANDLE(th)) return;
	devtimer = DEVTIMERHANDLETOPTR(th);
	devtimer->activeflg = FALSE;
}

static void devtimerevent()
{
	DEVTIMERHANDLE th;
	DEVTIMER *devtimer;
	UCHAR msgdata[33], newexpiretime[16];
	OPENINFO *openinfo;

	if (devtimerhandle > 0) {
		timstop(devtimerhandle);
		devtimerhandle = -1;
	}
	evtclear(maineventids[EVENT_DEVTIMER]);
	memcpy(newexpiretime, TIMMAXTIME, 16);

	th = devtimerlist;
	while (ISVALIDTIMERHANDLE(th)) {
		devtimer = DEVTIMERHANDLETOPTR(th);
		if (devtimer->activeflg) {
			if (memcmp(devtimer->expiretime, devexpiretime, 16) <= 0) {
				openinfo = *openinfopptr + devtimer->refnum - 1;
				if (openinfo->v.linkquehandle != NULL) {
					memcpy(msgdata, devtimer->name, MSG_NAMESIZE);
					memcpy(&msgdata[MSG_FUNCSTART], "TICK     ", MSG_FUNCSIZE + MSG_ITEMSIZE);
					memcpy(&msgdata[MSG_HORZSTART], devtimer->expiretime, 16);
					queput(openinfo->v.linkquehandle, msgdata, 33);
				}
				if (devtimer->timestep) timadd(devtimer->expiretime, devtimer->timestep);
				else devtimer->activeflg = FALSE;
			}
			if (devtimer->activeflg && memcmp(devtimer->expiretime, newexpiretime, 16) < 0) memcpy(newexpiretime, devtimer->expiretime, 16);
		}
		th = devtimer->next;
	}

	memcpy(devexpiretime, newexpiretime, 16);
	if (memcmp(newexpiretime, TIMMAXTIME, 16)) {
		devtimerhandle = timset(devexpiretime, maineventids[EVENT_DEVTIMER]);
		if (devtimerhandle < 0) memcpy(devexpiretime, TIMMAXTIME, 16);
	}
}

static INT devtimerdestroy(DEVTIMERHANDLE targetth)
{
	DEVTIMERHANDLE th, *prevth;
	DEVTIMER *devtimer;

	prevth = &devtimerlist;
	th = devtimerlist;
	while (ISVALIDTIMERHANDLE(th)) {
		devtimer = DEVTIMERHANDLETOPTR(th);
		if (th == targetth) {	/* found it */
			*prevth = devtimer->next;
			devtimer->next = devtimerfreelist;
			devtimerfreelist = th;
			devtimer->activeflg = FALSE;
			return(0);
		}
		prevth = &devtimer->next;
		th = devtimer->next;
	}
	return RC_INVALID_PARM;
}

static INT ctlmemicmp(void *src, void *dest, INT len)
{
	while (len--) if (toupper(((UCHAR *) src)[len]) != toupper(((UCHAR *) dest)[len])) return(1);
	return(0);
}

#if 0
static int getlicensenum()
{
	int i1, i2, i3;

	for (i1 = 10, i2 = i3 = 0; i1 < 190; i1++) {
		if (companyinfo[i1] & 0x15) i2 += companyinfo[i1];
		if (i1 & 0x06) i3 += (UCHAR)(i1 ^ companyinfo[i1]);
	}
	if (companyinfo[109] || companyinfo[169] || companyinfo[189]) return RC_ERROR;
	if ((i2 % 241) != (INT) companyinfo[190] || (i3 % 199) != (INT) companyinfo[191]) return RC_ERROR;
	return atoi((char *) &companyinfo[100]);
}
#endif

/**
 * return -1 if the end of the string is found
 *         0 if the token is an item number, which may include square brackets.
 *         1 if the token is a command.
 */
static INT nextChangeStringToken(UCHAR *str, INT length, INT *strpos, CHAR *buffer) {
	CHAR cmd[20], c1;
	INT pos, rc;

	if (*strpos >= length) return RC_ERROR;
	while ((isspace(str[*strpos]) || str[*strpos] == ',') && *strpos < length) (*strpos)++;
	if (*strpos >= length) return RC_ERROR;
	pos = 0;
	if (isdigit(str[*strpos])) {
		do {
			c1 = str[(*strpos)++];
			if (c1 != ' ') cmd[pos++] = c1;
		}
		while (*strpos < length && (isdigit(str[*strpos]) || str[*strpos] != ','));

		rc = 0;
	}
	else {
		do cmd[pos++] = tolower(str[(*strpos)++]);
		while (*strpos < length && isalpha(str[*strpos]));
		rc = 1;
	}
	cmd[pos] = '\0';
	strcpy(buffer, cmd);
	return rc;
}

/*
 * gui server routine to make changes to a resource
 *
 * The datacount parameter is used to distinguish between no data at all and zero length data
 */
static void guiserverreschange(CHAR *resname, UCHAR restype, UCHAR *fnc, INT fnclen,
		UCHAR *data, INT datalen, INT datacount)
{
	INT i1, pos, state;
	CHAR cmd[20], work[128], work2[256];
	ELEMENT *e1;
	ATTRIBUTE *a1;
#if 0
	OPENINFO *oinfoptr;
#endif

	if (restype == RES_TYPE_MENU) clientput("<change menu=", -1);
	else if (restype == RES_TYPE_PANEL) clientput("<change panel=", -1);
	else if (restype == RES_TYPE_DIALOG) clientput("<change dialog=", -1);
	else if (restype == RES_TYPE_TOOLBAR) clientput("<change toolbar=", -1);
	else if (restype == RES_TYPE_POPUPMENU) clientput("<change popupmenu=", -1);
	else if (restype == RES_TYPE_OPENFILEDLG) clientput("<change openfdlg=", -1);
	else if (restype == RES_TYPE_OPENDIRDLG) clientput("<change openddlg=", -1);
	else if (restype == RES_TYPE_SAVEASFILEDLG) clientput("<change saveasfdlg=", -1);
	else if (restype == RES_TYPE_FONTDLG) clientput("<change fontdlg=", -1);
	else if (restype == RES_TYPE_COLORDLG) clientput("<change colordlg=", -1);
	clientput(resname, -1);
	if (datalen != 0) {
		clientput(" l=\"", 4);
		clientputdata((CHAR *) data, datalen);
		clientput("\">", 2);
	}
	else if (datacount) clientput(" l=\"\">", -1);
	else clientput(">", 1);

	for (state = pos = 0;;) {
		i1 = nextChangeStringToken(fnc, fnclen, &pos, work);
		if (i1 == -1) break;
		switch(state) {
		case 0:
			if (i1 == 0) {		/* the command string did not start with a command */
				clientrelease();
				dbcerrinfo(794, (UCHAR*)"Bad change command string", -1);
			}
			strcpy(cmd, work);
			state = 1;
			break;
		case 1:
			if (!i1) {		/* we just scanned an item number */
				sprintf(work2, "<%s n=\"%s\"/>", cmd, work);
				clientput(work2, -1);
				state = 2;
			}
			else {
				sprintf(work2, "<%s/>", cmd);
				clientput(work2, -1);
				strcpy(cmd, work);
			}
			break;
		case 2:
			if (!i1) {		/* we just scanned an item number */
				sprintf(work2, "<%s n=\"%s\"/>", cmd, work);
				clientput(work2, -1);
			}
			else {
				strcpy(cmd, work);
				state = 1;
			}
			break;
		}
	}
	if (state == 1) {
		clientput("<", 1);
		clientput(cmd, -1);
		clientput("/>", 2);
	}

	clientput("</change>", -1);
	if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) dbcerrinfo(798, NULL, 0);
	e1 = (ELEMENT *) clientelement(); /* assume <r> element */
	if (e1 == NULL) {
		clientrelease();
		dbcerrinfo(798, NULL, 0);
	}
	else if ( (a1 = e1->firstattribute) != NULL) {
		if (strlen(a1->value) == 1 && (a1->value)[0] == '0') {
			clientrelease();
			dbcerrinfo(794, NULL, 0);
		}
		else {
			strcpy(work, a1->value);
			clientrelease();
			dbcerrinfo(794, (UCHAR*)work, -1);
		}
	}
	clientrelease();
	return;
}

/**
 * gui server routine to make changes to a window
 * returns RC_INVALID_PARM, RC_INVALID_VALUE,
 * 	1 if there is an error in communications with the SC,
 * 	2 if the SC returns an error message, in that case, the message is copied to errormsg,
 *  and finally, zero if AOK.
 */
static INT guiserverwinchange(CHAR *winname, UCHAR *fnc, INT fnclen, UCHAR *data, INT datalen)
{
	INT i1, i2;
	CHAR cmd[20], cmdl[20];
	CHAR work[128];
	ELEMENT *pe1;
	OPENINFO *oinfoptr;

	if (fnc == NULL || fnclen < 1 || fnclen >= 20) return(RC_INVALID_PARM);
	for (i1 = 0; i1 < fnclen; i1++) cmd[i1] = toupper(fnc[i1]);
	cmd[fnclen] = '\0';
	if ((cmd[0] == 'H' || cmd[0] == 'V') && !strncmp(&cmd[1], "SCROLLBAR", 9)){
		if (!strcmp(&cmd[10], "POS")) {
			if (data == NULL || datalen < 1) return(RC_INVALID_PARM);
			numtoi(data, datalen, &i1);
			if (i1 < 0) return(RC_INVALID_VALUE);
			clientput("<change window=\"", -1);
			clientput(winname, -1);
			clientput("\"", 1);
			if (cmd[0] == 'H') clientput(" hsbpos=", -1);
			else clientput(" vsbpos=", -1);
			mscitoa(i1, work);
			clientput(work, -1);
			clientput("/>", -1);
		}
		else if (!strcmp(&cmd[10], "OFF")) {
			clientput("<change window=\"", -1);
			clientput(winname, -1);
			clientput("\"", 1);
			if (cmd[0] == 'H') clientput(" hsboff=y/>", -1);
			else clientput(" vsboff=y/>", -1);
		}
		else if (!strcmp(&cmd[10], "RANGE")) {
			if (data == NULL || datalen < 1) return(RC_INVALID_PARM);
			clientput("<change window=\"", -1);
			clientput(winname, -1);
			clientput("\"", 1);
			if (cmd[0] == 'H') clientput(" hsbrange=", -1);
			else clientput(" vsbrange=", -1);
			data[datalen] = '\0';
			clientput("\"", -1);
			clientput((CHAR *) data, -1);
			clientput("\"/>", -1);
		}

		/**
		 * As of 1/12/09, send the undocumented window scrollbar commands
		 * to Smart Clients.
		 * We will implement this in the CSC and the .Net SC
		 * but not in the Java SC.
		 * It will remain undocumented.
		 */
		else {
			for (i1 = 0; i1 < fnclen; i1++) cmdl[i1] = tolower(fnc[i1]);
			clientput("<change window=\"", -1);
			clientput(winname, -1);
			clientput("\"><", -1);
			clientput(cmdl, fnclen);
			clientput("/></change>", -1);
		}
	}
	else if (!strcmp(cmd, "MOUSEON")) {
		clientput("<change window=\"", -1);
		clientput(winname, -1);
		clientput("\" mouse=y/>", -1);
	}
	else if (!strcmp(cmd, "MOUSEOFF")) {
		clientput("<change window=\"", -1);
		clientput(winname, -1);
		clientput("\" mouse=n/>", -1);
	}
	else if (!strcmp(cmd, "FOCUS")) {
		clientput("<change window=\"", -1);
		clientput(winname, -1);
		clientput("\" focus=y/>", -1);
	}
	else if (!strcmp(cmd, "UNFOCUS")) {
		clientput("<change window=\"", -1);
		clientput(winname, -1);
		clientput("\" focus=n/>", -1);
	}
	else if (!strcmp(cmd, "DESKTOPICON")) {
		clientput("<change window=\"", -1);
		clientput(winname, -1);
		clientput("\" f=desktopicon l=\"", -1);
		data[datalen] = '\0';
		for (i1 = 0, oinfoptr = *openinfopptr; i1 < openinfohi; i1++, oinfoptr++) {
			if ((oinfoptr->davbtype == DAVB_RESOURCE) && !strcmp(oinfoptr->name, (CHAR *) data)) {
				strcpy((CHAR *) data, oinfoptr->ident);
				break;
			}
		}
		clientputdata((CHAR *) data, -1);
		clientput("\"/>", -1);
	}
	else if (!strcmp(cmd, "TITLE") || !strcmp(cmd, "TASKBARTEXT")) {
		clientput("<change window=\"", -1);
		clientput(winname, -1);
		clientput("\" title=\"", -1);
		data[datalen] = '\0';
		clientputdata((CHAR *) data, -1);
		clientput("\"/>", -1);
	}
	else if (!strcmp(cmd, "POINTER")) {
		if (data == NULL || datalen < 1) return(RC_INVALID_PARM);
		if (datalen == 8) {
			if (!guimemicmp((CHAR *) data, "SIZENESW", 8)) {
				clientput("<change window=\"", -1);
				clientput(winname, -1);
				clientput("\" pointer=sizenesw/>", -1);
			}
			else if (!guimemicmp((CHAR *) data, "SIZENWSE", 8)) {
				clientput("<change window=\"", -1);
				clientput(winname, -1);
				clientput("\" pointer=sizenwse/>", -1);
			}
			else return(RC_INVALID_VALUE);
		}
		else if (datalen == 7) {
			if (!guimemicmp((CHAR *) data, "UPARROW", 7)) {
				clientput("<change window=\"", -1);
				clientput(winname, -1);
				clientput("\" pointer=uparrow/>", -1);
			}
			else if (!guimemicmp((CHAR *) data, "SIZEALL", 7)) {
				clientput("<change window=\"", -1);
				clientput(winname, -1);
				clientput("\" pointer=sizeall/>", -1);
			}
			else return(RC_INVALID_VALUE);
		}
		else if (datalen == 6) {
			if (!guimemicmp((CHAR *) data, "SIZENS", 6)) {
				clientput("<change window=\"", -1);
				clientput(winname, -1);
				clientput("\" pointer=sizens/>", -1);
			}
			else if (!guimemicmp((CHAR *) data, "SIZEWE", 6)) {
				clientput("<change window=\"", -1);
				clientput(winname, -1);
				clientput("\" pointer=sizewe/>", -1);
			}
			else return(RC_INVALID_VALUE);
		}
		else if (datalen == 5) {
			if (!guimemicmp((CHAR *) data, "ARROW", 5)) {
				clientput("<change window=\"", -1);
				clientput(winname, -1);
				clientput("\" pointer=arrow/>", -1);
			}
			else if (!guimemicmp((CHAR *) data, "IBEAM", 5)) {
				clientput("<change window=\"", -1);
				clientput(winname, -1);
				clientput("\" pointer=ibeam/>", -1);
			}
			else if (!guimemicmp((CHAR *) data, "CROSS", 5)) {
				clientput("<change window=\"", -1);
				clientput(winname, -1);
				clientput("\" pointer=cross/>", -1);
			}
			else return(RC_INVALID_VALUE);
		}
		else if (datalen == 4) {
			if (!guimemicmp((CHAR *) data, "HELP", 4)) {
				clientput("<change window=\"", -1);
				clientput(winname, -1);
				clientput("\" pointer=help/>", -1);
			}
			else if (!guimemicmp((CHAR *) data, "WAIT", 4)) {
				clientput("<change window=\"", -1);
				clientput(winname, -1);
				clientput("\" pointer=wait/>", -1);
			}
			else return(RC_INVALID_VALUE);
		}
		else {
			if (!guimemicmp((CHAR *) data, "APPSTARTING", 11)) {
				clientput("<change window=\"", -1);
				clientput(winname, -1);
				clientput("\" pointer=appstarting/>", -1);
			}
			else if (!guimemicmp((CHAR *) data, "HANDPOINT", 9)) {
				clientput("<change window=\"", -1);
				clientput(winname, -1);
				clientput("\" pointer=handpoint/>", -1);
			}
			else if (!guimemicmp((CHAR *) data, "NO", 2)) {
				clientput("<change window=\"", -1);
				clientput(winname, -1);
				clientput("\" pointer=no/>", -1);
			}
			else return(RC_INVALID_VALUE);
		}
	}
	else if (!strcmp(cmd, "CARETPOS")) {
		if (data == NULL || datalen < 1) return(RC_INVALID_PARM);
		num5toi(&data[0], &i1);
		num5toi(&data[5], &i2);
		if (i1 < 0 || i2 < 0) return(RC_INVALID_VALUE);
		clientput("<change window=\"", -1);
		clientput(winname, -1);
		clientput("\" caretposh=", -1);
		mscitoa(i1, work);
		clientput(work, -1);
		clientput(" caretposv=", -1);
		mscitoa(i2, work);
		clientput(work, -1);
		clientput("/>", -1);
	}
	else if (!strcmp(cmd, "CARETON")) {
		clientput("<change window=\"", -1);
		clientput(winname, -1);
		clientput("\" caret=y/>", -1);
	}
	else if (!strcmp(cmd, "CARETOFF")) {
		clientput("<change window=\"", -1);
		clientput(winname, -1);
		clientput("\" caret=n/>", -1);
	}
	else if (!strcmp(cmd, "CARETSIZE")) {
		if (data == NULL || datalen < 1) return(RC_INVALID_PARM);
		numtoi(data, datalen, &i1);
		if (i1 < 1) return(RC_INVALID_VALUE);
		clientput("<change window=\"", -1);
		clientput(winname, -1);
		clientput("\" caretsize=", -1);
		mscitoa(i1, work);
		clientput(work, -1);
		clientput("/>", -1);
	}
	else if (!strcmp(cmd, "SIZE")) {
		if (data == NULL || datalen < 1) return(RC_INVALID_PARM);
		num5toi(&data[0], &i1);
		num5toi(&data[5], &i2);
		if (i1 < 0 || i2 < 0) return(RC_INVALID_VALUE);
		sprintf(work, "<change window=\"%s\" hsize=\"%d\" vsize=\"%d\"/>", winname, i1, i2);
		clientput(work, -1);
	}
	else if (!strcmp(cmd, "POSITION")) {
		if (data == NULL || datalen < 1) return(RC_INVALID_PARM);
		num5toi(&data[0], &i1);
		num5toi(&data[5], &i2);
		if (i1 < 1 || i2 < 1) return(RC_INVALID_VALUE);
		// NOTE: Coordinates are still one-based over the wire
		sprintf(work, "<change window=\"%s\" h=\"%d\" v=\"%d\"/>", winname, i1, i2);
		clientput(work, -1);
	}
	else if (!strcmp(cmd, "STATUSBAR")) {
		if (data == NULL) return(RC_INVALID_PARM);
		clientput("<change window=\"", -1);
		clientput(winname, -1);
		clientput("\" statusbartext=\"", -1);
		data[datalen] = '\0';
		clientputdata((CHAR *) data, -1);
		clientput("\"/>", -1);
	}
	else if (!strcmp(cmd, "NOSTATUSBAR")) {
		clientput("<change window=\"", -1);
		clientput(winname, -1);
		clientput("\" statusbar=n/>", -1);
	}
	else if (!strcmp(cmd, "MAXIMIZE")) {
		clientput("<change window=\"", -1);
		clientput(winname, -1);
		clientput("\" f=maximize/>", -1);
	}
	else if (!strcmp(cmd, "MINIMIZE")) {
		clientput("<change window=\"", -1);
		clientput(winname, -1);
		clientput("\" f=minimize/>", -1);
	}
	else if (!strcmp(cmd, "RESTORE")) {
		clientput("<change window=\"", -1);
		clientput(winname, -1);
		clientput("\" f=restore/>", -1);
	}
	else if (!strcmp(cmd, "HELPTEXTWIDTH")) {
		if (data == NULL || datalen < 1) return(RC_INVALID_PARM);
		numtoi(data, datalen, &i1);
		if (i1 < 1) return(RC_INVALID_VALUE);
		clientput("<change window=\"", -1);
		clientput(winname, -1);
		clientput("\" ttwidth=", -1);
		mscitoa(i1, work);
		clientput(work, -1);
		clientput("/>", -1);
	}
	else return(RC_INVALID_VALUE);
	if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) return 1;
	pe1 = (ELEMENT *) clientelement(); /* assume <r> element */
	if (pe1 == NULL || pe1->firstattribute != NULL) {
		clientrelease();
		if (pe1 != NULL && pe1->firstattribute != NULL) strcpy(errormsg, pe1->firstattribute->value);
		else errormsg[0] = '\0';
		return 2;
	}
	clientrelease();
	return 0;
}

/**
 * Returns 1 for success
 * Returns a zero or negative value if error
 *
 * @param font A standard DB/C font description
 * @param fromDraw A boolean, TRUE if called from the Draw verb processing
 */
static INT guiservergenfont(CHAR *font, INT fromDraw)
{
	CHAR *ptr, work[128];
	INT i1, pos, worklen, fontlen;
	CHAR size[32];

	assert(font != NULL);
	fontlen = (INT)strlen(font);
	if (!fontlen) return(0);
	clientput("<font", -1);
	for (worklen = 0, ptr = font; worklen < fontlen; ptr++) work[worklen++] = toupper(*ptr);
	work[worklen] = '\0';
	if (work[0] == '(') pos = 0;
	else {
		for (pos = 0; work[pos] && work[pos] != '('; pos++);  /* find end of string or '(' */
		for (i1 = pos; work[i1 - 1] == ' '; i1--);	/* cutoff trailing blanks on font */
		work[i1] = '\0';
	}
	if (pos != worklen) {  /* '(' was found in original font name */
		ptr = &work[pos + 1];
		if (*ptr == ' ') {
			while(*ptr == ' ') ptr++;
			if (!*ptr) return(0);  /* blanks after the ( */
			if (*ptr != '(') return(-2);
		}
		size[0] = '\0';
		while (*ptr != ')') {
			if (!*ptr) return(-2);
			else if (isdigit(*ptr) || *ptr == '.') {
				i1 = 0;
				if (*ptr == '.') size[i1++] = '0';
				size[i1++] = *ptr++;
				while (isdigit(*ptr) || *ptr == '.') size[i1++] = *ptr++;
				if (!*ptr) return(-3);
				size[i1] = '\0';
			}
			else if (!strncmp(ptr, "PLAIN", 5)) {
				clientput(" bold=\"no\"", -1);
				clientput(" italic=\"no\"", -1);
				clientput(" underline=\"no\"", -1);
				ptr += 5;
			}
			else if (!strncmp(ptr, "BOLD", 4)) {
				clientput(" bold=\"yes\"", -1);
				ptr += 4;
			}
			else if (!strncmp(ptr, "NOBOLD", 6)) {
				clientput(" bold=\"no\"", -1);
				ptr += 6;
			}
			else if (!strncmp(ptr, "ITALIC", 6)) {
				clientput(" italic=\"yes\"", -1);
				ptr += 6;
			}
			else if (!strncmp(ptr, "NOITALIC", 8)) {
				clientput(" italic=\"no\"", -1);
				ptr += 8;
			}
			else if (!strncmp(ptr, "UNDERLINE", 9)) {
				clientput(" underline=\"yes\"", -1);
				ptr += 9;
			}
			else if (!strncmp(ptr, "NOUNDERLINE", 11)) {
				clientput(" underline=\"no\"", -1);
				ptr += 11;
			}
			else return(-3);

			while(*ptr == ' ') ptr++;
			if (*ptr == ',') ptr++;
			else if (*ptr != ')' && *ptr != ' ') return(-3);
			while(*ptr == ' ') ptr++;
		}
		if (size[0]) {
			clientput(" s=\"", -1);
			clientput(size, -1);
			clientput("\"", -1);
		}
	}
	if (work[0] != '(') { /* Name */
		clientput(" n=\"", -1);
		if (!fromDraw && dbcflags & DBCFLAG_CLIENTINIT && GuiCtlfont != NULL) {
			if (ctlmemicmp(work, "default", 8 /* YES, I mean EIGHT */ ) == 0) {
				strcpy(work, GuiCtlfont);
			}
		}
		clientput(work, -1);
		clientput("\"", -1);
	}
	clientput("/>", -1);
	return 1;
}

/**
 * Smart Client call back function
 * Interprets asynchronous messages from client like 'push' or 'esel'
 */
static void clientcbfnc(ELEMENT *element)
{
	static CHAR lastwin[9], lastposx[6], lastposy[6];
	INT i1, i2, pflag1, pflag2, pflag3;
	CHAR workname[9], work[32768];
	OPENINFO *openinfo;
	ELEMENT *e1;
	ATTRIBUTE *a1 = NULL;

	e1 = element;
	a1 = e1->firstattribute;
	/*
	 * There must be at least on attribute, or something went very badly wrong
	 */
	if (a1 == NULL) {
		dbcerrinfo(798, (UCHAR*)"1st Attribute missing in clientcbfnc", -1);
	}
	memset(work, ' ', 36);
	work[8] = toupper(e1->tag[0]);
	work[9] = toupper(e1->tag[1]);
	if (!(e1->tag[0] == 'o' && e1->tag[1] == 'k')) {
		work[10] = toupper(e1->tag[2]);
		work[11] = toupper(e1->tag[3]);
	}
	if (!strcmp(e1->tag, "posn")) {
		pflag1 = pflag2 = pflag3 = FALSE;
		while (a1 != NULL) {
			i1 = (INT)strlen(a1->value);
			if (a1->tag[0] == 'n') {
				memcpy(work, a1->value, i1);
				strcpy(lastwin, a1->value);
				pflag1 = TRUE;
			}
			else if (a1->tag[0] == 'h') {
				memcpy(work + (22 - i1), a1->value, i1);
				strcpy(lastposx, a1->value);
				pflag2 = TRUE;
			}
			else if (a1->tag[0] == 'v') {
				memcpy(work + (27 - i1), a1->value, i1);
				strcpy(lastposy, a1->value);
				pflag3 = TRUE;
			}
			a1 = a1->nextattribute;
		}
		if (pflag1 == FALSE) {
			i1 = (INT)strlen(lastwin);
			memcpy(work, lastwin, i1);
		}
		if (pflag2 == FALSE) {
			i1 = (INT)strlen(lastposx);
			memcpy(work + (22 - i1), lastposx, i1);
		}
		if (pflag3 == FALSE) {
			i1 = (INT)strlen(lastposy);
			memcpy(work + (27 - i1), lastposy, i1);
		}
		work[27] = '\0';
	}
	else {
		/* assume window name is first attribute */
		memcpy(work, a1->value, strlen(a1->value));
	}

	if (!strcmp(e1->tag, "posn")) { /* do nothing */ }
	else if (!memcmp(e1->tag, "char", 4)) {
		assert(a1 != NULL);
		a1 = a1->nextattribute; /* assume 't' tag */
		i1 = (INT)strlen(a1->value);
		memcpy(work + (22 - i1), a1->value, i1);
		work[27] = '\0';
	}
	else if (!memcmp(e1->tag, "nkey", 4)) {
		work[27] = '\0';
		assert(a1 != NULL);
		a1 = a1->nextattribute;
		while (a1 != NULL) {
			i1 = (INT)strlen(a1->value);
			if (a1->tag[0] == 't') memcpy(work + (22 - i1), a1->value, i1);
			else if (a1->tag[0] == 'i') {
				memcpy(work + (17 - i1), a1->value, i1);
				work[22] = '\0';
			}
			a1 = a1->nextattribute;
		}
	}
	else if (!memcmp(e1->tag, "wact", 4) || !memcmp(e1->tag, "wmin", 4)) work[27] = '\0';

	/*
	 * Code required here to translate some of the hs and vs messages.
	 * They are sent over the wire in a form that differs from the
	 * message function code that we ultimately want to send to the DB/C program.
	 * That is because it is invalid to use + and - in an element name in XML.
	 */
	else if (!memcmp(e1->tag, "hsam", 4) || !memcmp(e1->tag, "hsap", 4) ||
		!memcmp(e1->tag, "hspm", 4) || !memcmp(e1->tag, "hspp", 4) ||
		!memcmp(e1->tag, "vsam", 4) || !memcmp(e1->tag, "vsap", 4) ||
		!memcmp(e1->tag, "vspm", 4) || !memcmp(e1->tag, "vspp", 4) ||
		!memcmp(e1->tag, "vstm", 4) || !memcmp(e1->tag, "vstf", 4) ||
		!memcmp(e1->tag, "hstm", 4) || !memcmp(e1->tag, "hstf", 4)
		)
	{
		if (e1->tag[2] == 'a' || e1->tag[2] == 'p') {
			if (e1->tag[3] == 'm') work[11] = '-';
			else work[11] = '+';
		}
		assert(a1 != NULL);
		a1 = a1->nextattribute;
		while (a1 != NULL) {
			i1 = (INT)strlen(a1->value);
			if (a1->tag[0] == 'h') memcpy(work + (22 - i1), a1->value, i1);
			a1 = a1->nextattribute;
		}
		work[22] = '\0';
	}

	else if (!memcmp(e1->tag, "lbdn", 4) || !memcmp(e1->tag, "wsiz", 4) ||
		!memcmp(e1->tag, "rbup", 4) ||
		!memcmp(e1->tag, "lbup", 4) || !memcmp(e1->tag, "rbdn", 4) ||
		!memcmp(e1->tag, "lbdc", 4) || !memcmp(e1->tag, "mbdn", 4) ||
		!memcmp(e1->tag, "mbup", 4) || !memcmp(e1->tag, "mbdc", 4) ||
		!memcmp(e1->tag, "wpos", 4) || !memcmp(e1->tag, "post", 4) ||
		!memcmp(e1->tag, "posb", 4) || !memcmp(e1->tag, "posl", 4) ||
		!memcmp(e1->tag, "posr", 4) || !memcmp(e1->tag, "rbdc", 4))
	{
		assert(a1 != NULL);
		a1 = a1->nextattribute;
		while (a1 != NULL) {
			i1 = (INT)strlen(a1->value);
			if (a1->tag[0] == 'h') memcpy(work + (22 - i1), a1->value, i1);
			else if (a1->tag[0] == 'v') memcpy(work + (27 - i1), a1->value, i1);
			else if (a1->tag[0] == 'i') memcpy(work + (17 - i1), a1->value, i1);
			else if (strcmp(a1->tag, "mkeys") == 0) {
				if (a1->value[0] == '1') memcpy(work + 27, "CTRL", 4);
				else if (a1->value[0] == '2') memcpy(work + 27, "SHIFT", 5);
				else if (a1->value[0] == '3') memcpy(work + 27, "CTRLSHIFT", 9);
			}
			a1 = a1->nextattribute;
		}
		work[36] = '\0';
	}
	else if (!memcmp(e1->tag, "tick", 4) || !memcmp(e1->tag, "ok", 2)) {
		assert(a1 != NULL);
		a1 = a1->nextattribute; /* assume 'time' or 't' tag */
		/* 't' attribute may be missing */
		if (a1 != NULL) {
			i1 = (INT)strlen(a1->value);
			memcpy(work + 17, a1->value, i1);
			work[17 + i1] = '\0';
		}
	}
	else if (!memcmp(e1->tag, "canc", 4)) {
		/* do nothing */
	}
	else if (!memcmp(e1->tag, "push", 4)) {
		i2 = 17;
		assert(a1 != NULL);
		a1 = a1->nextattribute;
		while (a1 != NULL) {
			if (a1->tag[0] == 'i') {
				i1 = (INT)strlen(a1->value);
				memcpy(work + (17 - i1), a1->value, i1);
			}
			else if (a1->tag[0] == 't') {
				i1 = (INT)strlen(a1->value);
				memcpy(work + 17, a1->value, i1);
				i2 = 17 + i1;
			}
			a1 = a1->nextattribute;
		}
		work[i2] = '\0';
	}
	else if (!strcmp(e1->tag, "menu") || !strcmp(e1->tag, "focs") ||
		!strcmp(e1->tag, "focl"))
	{
		CHAR* item = NULL;
		CHAR* data = NULL;
		a1 = e1->firstattribute;
		while (a1 != NULL)
		{
			if (!strcmp(a1->tag, "i")) item = a1->value;
			else if (!strcmp(a1->tag, "t")) data = a1->value;
			a1 = a1->nextattribute;
		}
		work[12] = '\0';
		if (item != NULL)
		{
			work[12] = ' ';
			i1 = (INT)strlen(item);
			memcpy(work + (17 - i1), item, i1);
			work[17] = '\0';
		}
		if (data != NULL)
		{
			work[17] = ' ';
			i1 = (INT)strlen(data);
			memcpy(work + (22 - i1), data, i1);
			work[22] = '\0';
		}
	}
	else if (!memcmp(e1->tag, "item", 4) || !memcmp(e1->tag, "pick", 4) ||
		!memcmp(e1->tag, "dsel", 4) || !memcmp(e1->tag, "esel", 4) ||
		!memcmp(e1->tag, "open", 4) || !memcmp(e1->tag, "clos", 4)) {
		assert(a1 != NULL);
		a1 = a1->nextattribute;
		work[17] = '\0'; /* in case message is window CLOS, not tree CLOS */
		while (a1 != NULL) {
			i1 = (INT)strlen(a1->value);
			if (a1->tag[0] == 't') {
				if (17 + i1 > 32768) {
/*** CODE: PROVIDE AN ERROR OR PERHAPS ALLOCATE MORE MEMORY HERE ***/
					return;
				}
				if (i1 > 0) memcpy(work + 17, a1->value, i1);
				work[17 + i1] = '\0';
			}
			else if (a1->tag[0] == 'i') {
				memcpy(work + (17 - i1), a1->value, i1);
			}
			a1 = a1->nextattribute;
		}
	}

	/*
	 * Code required here to translate the sb messages.
	 * They are sent over the wire in a form that differs from the
	 * message function code that we ultimately want to send to the DB/C program.
	 * That is because it is invalid to use + and - in an element name in XML.
	 */
	else if (!memcmp(e1->tag, "sbam", 4) || !memcmp(e1->tag, "sbap", 4) ||
		!memcmp(e1->tag, "sbpm", 4) || !memcmp(e1->tag, "sbpp", 4) ||
		!memcmp(e1->tag, "sbtm", 4) || !memcmp(e1->tag, "sbtf", 4)) {
		if (e1->tag[2] == 'a' || e1->tag[2] == 'p') {
			if (e1->tag[3] == 'm') work[11] = '-';
			else work[11] = '+';
		}
		assert(a1 != NULL);
		a1 = a1->nextattribute;
		while (a1 != NULL) {
			i1 = (INT)strlen(a1->value);
			if (a1->tag[0] == 'i') memcpy(work + (17 - i1), a1->value, i1);
			else if (!memcmp(a1->tag, "pos", 3)) memcpy(work + (22 - i1), a1->value, i1);
			a1 = a1->nextattribute;
		}
		work[22] = '\0';
	}
/*** CODE: ELSE CAN NOT CALL DBCERROR() AS THIS IS MAY BE A CALLBACK FROM THREAD ***/
	else return;

	memcpy(workname, work, 8);
	for (i1 = 0; i1 < 8; i1++) work[i1] = ' '; /* erase ident */
	if (work[0] == 'D') {
		workname[8] = '\0';
	}
	i1 = 8;
	do workname[i1--] = '\0';
	while (i1 > 0 && workname[i1] == ' ');
	openinfo = *openinfopptr;
	for (i1 = 0; i1 < openinfohi; i1++, openinfo++) {
		if (((openinfo->davbtype == DAVB_DEVICE && openinfo->devtype == DEV_WINDOW) ||
			openinfo->davbtype == DAVB_RESOURCE) && !memcmp(openinfo->ident, workname, 8)) {
			if (openinfo->v.linkquehandle != NULL) {
				for (i1 = (INT)strlen(openinfo->name) - 1; i1 >= 0; i1--) {
					work[i1] = openinfo->name[i1]; /* replace with true name */
				}
				if (!guilock) guisuspend();
				queput(openinfo->v.linkquehandle, (UCHAR *) work, (INT)strlen(work));
				if (dbcflags & DBCFLAG_DEBUG) guiresume();
			}
			break;
		}
	}
}

static void ctldspstring(char *str)
{
	fputs(str, stdout);
}

#if OS_UNIX
static void sigevent(INT sig)
{
	dbcexit(0);
}
#endif

#if OS_WIN32
static void loaddsr()
{
	dsrhandle = LoadLibrary("DBCDSR");
	if (dsrhandle == NULL) dbcerrinfo(653, (UCHAR *) "DBCDSR.DLL", 10);

	dsrinit = (_DSRINIT *) GetProcAddress(dsrhandle, "dsrinit");
	dsropen = (_DSROPEN *) GetProcAddress(dsrhandle, "dsropen");
	dsrclose = (_DSRCLOSE *) GetProcAddress(dsrhandle, "dsrclose");
	dsrquery = (_DSRQUERY *) GetProcAddress(dsrhandle, "dsrquery");
	dsrchange = (_DSRCHANGE *) GetProcAddress(dsrhandle, "dsrchange");
	dsrload = (_DSRLOAD *) GetProcAddress(dsrhandle, "dsrload");
	dsrstore = (_DSRSTORE *) GetProcAddress(dsrhandle, "dsrstore");
	dsrlink = (_DSRLINK *) GetProcAddress(dsrhandle, "dsrlink");
	dsrunlink = (_DSRUNLINK *) GetProcAddress(dsrhandle, "dsrunlink");
	if (dsrinit == NULL || dsropen == NULL || dsrclose == NULL ||
	    dsrquery == NULL || dsrchange == NULL || dsrload == NULL ||
	    dsrstore == NULL || dsrlink == NULL || dsrunlink == NULL) {
		FreeLibrary(dsrhandle);
		dbcerrinfo(655, (UCHAR *) "DBCDSR.DLL", 10);
	}

	dsrinit(quefunc);
	dsrinitflag = 1;
	atexit(freedsr);
}

static void freedsr()
{
	FreeLibrary(dsrhandle);
}
#endif

/* convert n character name to 4 characters + 4 digits */
static void getuniqueid(CHAR *name)
{
	static INT n1 = 0;
	INT i1, i2;

	i2 = n1;
	if (i2 > 9999) i2 = 0;
	for (i1 = 0; i1 < 5; i1++) if (name[i1] == 0 || name[i1] == ' ') name[i1] = '0';
	for (i1 = 3; i1 >= 0; ) {
		name[4 + i1--] = i2 % 10 + '0';
		if (!(i2 /= 10)) break;
	}
	while (i1 >= 0) name[4 + i1--] = '0';
	n1++;
	name[8] = '\0';
}

static INT num5toi(UCHAR *p1, INT *dest)  /* convert 5 characters to integer */
/* return 0 if 5 characters are valid, else return -1 */
{
	INT i1, n1, negative;

	negative = FALSE;
	*dest = n1 = 0;
	for (i1 = 0; i1 < 5 && p1[i1] == ' '; i1++);
	if (i1 == 5) return RC_ERROR;
	if (p1[i1] == '-') {
		negative = TRUE;
		if (++i1 == 5) return RC_ERROR;
	}
	while (i1 < 5 && isdigit(p1[i1])) n1 = n1 * 10 + p1[i1++] - '0';
	if (i1 < 5) return RC_ERROR;
	if (negative) n1 = (-n1);
	*dest = n1;
	return(0);

}

static void numtoi(UCHAR *data, INT datalen, INT *destnum)
{
	INT datapos, num;

	num = 0;
	for (datapos = 0; datapos < datalen; datapos++) {
		if (isdigit(data[datapos])) num = num*10 + data[datapos] - '0';
	}
	*destnum = num;
}

#if 0
INT guimemicmp(void *src, void *dest, INT len)
{
	while(len--) if (toupper(((CHAR *) src)[len]) != toupper(((CHAR *)dest)[len])) return(1);
	return(0);
}
#endif

void buildDXAboutInfoString() {
	strcat(DXAboutInfoString, "Release: " RELEASE);
	strcat(DXAboutInfoString, "\nMachine Type: ");
#if DX_MACHINE == LCS_MACHINE_WINI
	strcat(DXAboutInfoString, "WINI");
#elif DX_MACHINE == LCS_MACHINE_LINUXI
	strcat(DXAboutInfoString, "LINUXI");
#elif DX_MACHINE == LCS_MACHINE_MAC
	strcat(DXAboutInfoString, "MACOS");
#endif
	strcat(DXAboutInfoString, "\n");
}
