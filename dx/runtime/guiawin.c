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

#define GUIAWIN1
#define INC_STRING
#define INC_CTYPE
#define INC_LIMITS
#define INC_STDLIB
#define INC_STDIO
#define INC_MATH
#include "includes.h"
#include "base.h"
#include "evt.h"
#include "fio.h"
#include "gui.h"
#include "guix.h"
#include "svc.h"
#include "dbc.h"
#include "fontcache.h"
#include "release.h"

#include <dde.h>
#include <process.h>
#include <conio.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <shtypes.h>
#include <strsafe.h>
#include <uxtheme.h>
#if _MSC_VER >= 1800
#include <VersionHelpers.h>
#endif
#pragma warning(disable : 4995)

/*
 * Define some stuff strictly for debugging. Not used here.
 */
#define WM_UAHDESTROYWINDOW 0x0090
#define WM_UAHDRAWMENU 0x0091
#define WM_UAHDRAWMENUITEM 0x0092
#define WM_UAHINITMENU 0x0093
#define WM_UAHMEASUREMENUITEM 0x0094
#define WM_UAHNCPAINTMENUPOPUP 0x0095

/* define some general macros */
extern CHAR pgmname[20];
#define GUIADEBUG

#ifndef SPI_SETFOREGROUNDLOCKTIMEOUT
#define SPI_SETFOREGROUNDLOCKTIMEOUT 0x2001
#endif

#ifndef IDC_HAND
#define IDC_HAND            MAKEINTRESOURCE(32649)
#endif

/* define MENU related macros */
#define MAXMENUDEPTH 		10

/**
 * Maximum number of bytes in the return from a system call for an open file dialog
 */
#define MAXCbMSGFILE		16000
//#define MAXRESFONTS			30

#define ITEM_MSG_STATUSBAR	63
#define ITEM_MSG_TOOLBAR	64

/* define some scroll bar values */
#define SBPIXINCR 12				/* window scroll bar pixel increment value */

/* define control query macros */
#define ISLASTRADIO(pctl) ((pctl)->type == PANEL_LASTRADIOBUTTON)
#define ISCHECKTYPE(pctl) ((pctl)->type == PANEL_CHECKBOX || \
	(pctl)->type == PANEL_LTCHECKBOX || \
	(pctl)->type == PANEL_RADIOBUTTON || \
	(pctl)->type == PANEL_LASTRADIOBUTTON || \
	(pctl)->type == PANEL_BUTTON)
#define ISPUSHBUTTON(pctl) ((pctl)->type == PANEL_PUSHBUTTON || \
	(pctl)->type == PANEL_ICONPUSHBUTTON || \
	(pctl)->type == PANEL_ICONDEFPUSHBUTTON || \
	(pctl)->type == PANEL_DEFPUSHBUTTON)
#define ISDEFPUSHBUTTON(pctl) ((pctl)->type == PANEL_DEFPUSHBUTTON || \
	(pctl)->type == PANEL_ICONDEFPUSHBUTTON)
#define ISLOCKBUTTON(pctl) ((pctl)->type == PANEL_LOCKBUTTON || \
	(pctl)->type == PANEL_ICONLOCKBUTTON)
#define ISICONBUTTON(pctl) ((pctl)->type == PANEL_ICONPUSHBUTTON || \
	(pctl)->type == PANEL_ICONDEFPUSHBUTTON || \
	(pctl)->type == PANEL_ICONLOCKBUTTON)
#define ISTEXT(pctl) ((pctl)->type == PANEL_TEXT || \
	(pctl)->type == PANEL_VTEXT || \
	(pctl)->type == PANEL_RTEXT || \
	(pctl)->type == PANEL_RVTEXT || \
	(pctl)->type == PANEL_CVTEXT || \
	(pctl)->type == PANEL_CTEXT || \
	(pctl)->type == PANEL_POPBOX)
#define ISMEDITTYPE(pctl) ((pctl)->type == PANEL_MEDIT || \
	(pctl)->type == PANEL_MEDITS || \
	(pctl)->type == PANEL_MEDITHS || \
	(pctl)->type == PANEL_MEDITVS)
#define ISMLEDITTYPE(pctl) ((pctl)->type == PANEL_MLEDIT || \
	(pctl)->type == PANEL_MLEDITS || \
	(pctl)->type == PANEL_MLEDITHS || \
	(pctl)->type == PANEL_MLEDITVS)
#define ISMEDIT(pctl) (ISMEDITTYPE(pctl) || ISMLEDITTYPE(pctl))
#define ISSINGLELINEEDIT(pctl) ((pctl)->type == PANEL_EDIT || \
	(pctl)->type == PANEL_PEDIT || \
	(pctl)->type == PANEL_FEDIT || \
	(pctl)->type == PANEL_LEDIT || \
	(pctl)->type == PANEL_PLEDIT)
#define ISINTERACTIVE(pctl) ((pctl)->type == PANEL_BUTTON || \
	(pctl)->type == PANEL_CHECKBOX || \
	(pctl)->type == PANEL_DEFPUSHBUTTON || \
	(pctl)->type == PANEL_DROPBOX || \
	(pctl)->type == PANEL_EDIT || \
	(pctl)->type == PANEL_FEDIT || \
	(pctl)->type == PANEL_HSCROLLBAR || \
	(pctl)->type == PANEL_ICONPUSHBUTTON || \
	(pctl)->type == PANEL_ICONLOCKBUTTON || \
	(pctl)->type == PANEL_ICONDEFPUSHBUTTON || \
	(pctl)->type == PANEL_LASTRADIOBUTTON || \
	(pctl)->type == PANEL_LEDIT || \
	(pctl)->type == PANEL_LISTBOX || \
	(pctl)->type == PANEL_LISTBOXHS || \
	(pctl)->type == PANEL_LOCKBUTTON || \
	(pctl)->type == PANEL_LTCHECKBOX || \
	(pctl)->type == PANEL_MEDIT || \
	(pctl)->type == PANEL_MEDITS || \
	(pctl)->type == PANEL_MEDITHS || \
	(pctl)->type == PANEL_MEDITVS || \
	(pctl)->type == PANEL_MLEDIT || \
	(pctl)->type == PANEL_MLEDITS || \
	(pctl)->type == PANEL_MLEDITHS || \
	(pctl)->type == PANEL_MLEDITVS || \
	(pctl)->type == PANEL_MLISTBOX || \
	(pctl)->type == PANEL_MLISTBOXHS || \
	(pctl)->type == PANEL_PEDIT || \
	(pctl)->type == PANEL_PLEDIT || \
	(pctl)->type == PANEL_PUSHBUTTON || \
	(pctl)->type == PANEL_RADIOBUTTON || \
	(pctl)->type == PANEL_TABLE || \
	(pctl)->type == PANEL_VSCROLLBAR)

#define DX_FATAL_A "Fatal Error - unable to sub-class frame window"
#define DX_FATAL_B "Fatal Error - unable to sub-class dialog window"
#define DX_FATAL_C "Fatal Error - unable to create main window"
#define DX_FATAL_D "Fatal Error - unable to start secondary thread"
#define DX_FATAL_15 "buffer overflow while generating an ESEL message"

/*
 * ID number that we use for the icon in the tray
 */
#define TRAY_ICON_ID 1

/*
 * Set up a typedef for the system function SetMenuInfo in user32
 * We need to do this because the function did not exist on Win95
 * and using it directly would prevent loading
 * As of 02 JULY 2019, no longer used. We will not support less than Vista at all.
 */
//typedef BOOL (*guiaPSetMenuInfo) (HMENU, LPCMENUINFO);

/* define structures used in this module */
typedef struct guia_win_create {		/* structure passed to window creation in msg loop thread */
	WINHANDLE wh;
	INT style;
	INT hpos;	/* one-based */
	INT vpos;	/* one-based */
	INT clienthsize;
	INT clientvsize;
} GUIAWINCREATESTRUCT;

typedef struct guia_dlg_create { 		/* structure passed to dialog creation in msg loop thread */
	RESOURCE *res;
	INT hpos;
	INT vpos;
} GUIADLGCREATESTRUCT;

typedef struct guia_show_controls {	/* structure passed to control group creation in msg loop thread */
	HWND hwnd;
	RESOURCE *res;
	INT hpos;
	INT vpos;
	DWORD tabmask;
} GUIASHOWCONTROLS;

typedef struct guia_hide_controls {	/* structure passed to control group hide in msg loop thread */
	RESOURCE *res;
} GUIAHIDECONTROLS;

typedef struct guia_ext_window_create {		/* structure passed to extended window create in msg loop thread */
	HWND hwndParent;
	DWORD dwExStyle;
	LPCSTR lpszClassName;
	LPCSTR lpszWindowName;
	DWORD dwStyle;
	INT x;
	INT y;
	INT width;
	INT height;
	HWND hwnd;
	HMENU hmenu;
	LPVOID lpvParam;
} GUIAEXWINCREATESTRUCT;

typedef struct guia_get_win_text {
	HWND hwnd;
	CHAR *title;
	INT textlen;
} GUIAGETWINTEXT;

typedef struct guia_toolbar_dropbox_create {
	RESOURCE *res;
	CONTROL *control;
	INT_PTR idCommand;
} GUIATOOLBARDROPBOX;

typedef struct startdocstruct {
	HANDLE handle;
	DOCINFO *docinfo;
	UINT errval;
} STARTDOCINFO;

/**
 * Used only by File Open and File Save dialogs
 */
typedef struct filedlgstruct {
	/**
	 * Holds return value from call to GetOpenFileName or GetSaveFileName
	 */
	int retcode;
	/**
	 * Holds return value from call to CommDlgExtendedError
	 */
	int extcode;
	OPENFILENAME ofnstruct;
} FILEDLGINFO;

/* defines for reading .ICO files */
typedef struct
{
	BYTE	bWidth;               /* Width of the image */
	BYTE	bHeight;              /* Height of the image (times 2) */
	BYTE	bColorCount;          /* Number of colors in image (0 if >=8bpp) */
	BYTE	bReserved;            /* Reserved */
	WORD	wPlanes;              /* Color Planes */
	WORD	wBitCount;            /* Bits per pixel */
	DWORD	dwBytesInRes;         /* how many bytes in this resource? */
	DWORD	dwImageOffset;        /* where in the file is this image */
} ICONDIRENTRY, *LPICONDIRENTRY;

typedef struct
{
	WORD			idReserved;   /* Reserved */
	WORD			idType;       /* resource type (1 for icons) */
	WORD			idCount;      /* how many images? */
	ICONDIRENTRY	idEntries[1]; /* the entries for each image */
} ICONDIR, *LPICONDIR;

typedef struct
{
	UINT			Width, Height, Colors; /* Width, Height and bpp */
	LPBYTE			lpBits;                /* ptr to DIB bits */
	DWORD			dwNumBytes;            /* how many bytes? */
	LPBITMAPINFO	lpbi;                  /* ptr to header */
} ICONIMAGE, *LPICONIMAGE;

typedef struct
{
	BOOL		bHasChanged;                     /* Has image changed? */
	UINT		nNumImages;                      /* How many images? */
	ICONIMAGE	IconImages[1];                   /* Image entries */
} ICONRESOURCE, *LPICONRESOURCE;

typedef HRESULT (CALLBACK* DLLGETVERSIONPROC2)(DLLVERSIONINFO2 *);

/**
 * Handle to this program instance, set in WinMain upon startup
 */
static HINSTANCE hinstancemain;
static INT screenheight;			/* height of screen in pixels */
static INT screenwidth;				/* width of screen in pixels */
static INT breakeventid;			/* EVT handle for the Break event */
static ZHANDLE aboutline1;			/* Info line 1 for About dialog */
static ZHANDLE aboutline2;			/* Info line 2 for About dialog */
static CHAR winclassname[] = "DBCwinwc";	/* window class name for all windows */
static CHAR dlgclassname[] = "DBCdlgwc";	/* window class name for all dialogs */
static INT breakflag;				/* set to true when a break event has occured */
static HWND hwndmainwin;			/* handle of the main DB/C window */
static HWND hwndfocuswin;			/* handle of DB/C window currently in focus */
static HACCEL haccelfocus;			/* handle of DB/C accelerator table currently in focus */
/**
 * linked list of shown DIALOG resources - ACCESSED IN MSG LOOP THREAD ONLY
 * top most dialog is first
 */
static RESHANDLE dlgreshandlelisthdr;
static LOGBRUSH logbrushbackground;	/* background brush used to paint client area of DB/C Window */
static INT bpplimit;				/* bits per pixel resolution limit for current video mode */
static HPALETTE hpaloldpal;			/* palette established before a DRAW was started */
static HFONT oldhfont;				/* font established before a DRAW was stated */
static HBITMAP oldhbitmap;			/* bitmap established before a DRAW was stated */
static WINHANDLE drawwinhandle;		/* handle of window that DRAW operation is currently taking place in */
static PIXHANDLE drawpixhandle;		/* pix map the the current DRAW operation is currently taking place on */
static HANDLE hpennull, hbrushnull; /* static objects */
static INT shutdownflag;			/* set to TRUE when main interpreter thread has shut itself down */
static INT exitvalue = 0;			/* the value to exit with */
static INT levelitemcnt[MAXMENUDEPTH];			/* item count at each menu level (used in the guiaWinProc routine) */
static HMENU hmenucurrentmenu[MAXMENUDEPTH];	/* current menu handle at each menu level (used in the guiaWinProc routine) */
static TBBUTTON tbbutton;			/* tool bar button structure (used in the guiaWinProc routine) */
static TBADDBITMAP tbbmaddbitmap;	/* tool bar add bitmap structure (used in guiaWinProc routine) */
static TOOLINFO toolinfo;			/* contains info about a tool (used for bar tool tips) */
static BYTE initcomctlflag;		/* flag set to TRUE when common controls DLL has been loaded */
static INT nexticonid = 32001;		/* next available ID of an icon for window notification messages */

static HFONT hfontcontrolfont;		/* handle of font to be used for all controls */
static LOGFONT logfontcontrolfont;	/* logical font structure of font to be used for all controls */
//static BOOL isWinXP;				/* Set if this is Windows XP */
static DWORD shell32_Major;			/* Major version of the shell32 library */
static DWORD shell32_Minor;			/* Minor version of the shell32 library */
static DWORD shell32_Build;			/* Build version of the shell32 library */

static BOOL useTransparentBlt;		/* Do we have dbcdx.gui.menu.icon=useTransparentBlt */
static BOOL useEditBoxClientEdge;	/* For edit boxes of all types, should we use WS_EX_CLIENTEDGE */
static BOOL useListBoxClientEdge;	/* For List boxes of all types, should we use WS_EX_CLIENTEDGE */
static BOOL useTerminateProcess;	/* Do we have dbcdx.gui.quit=useTerminateProcess */
static INT endsessionflag;			/* set to TRUE if a WM_QUERYENDSESSION message has been received */
static INT popupmenuh;				/* horz position of popup menu */
static INT popupmenuv;				/* vert position of popup menu */
static INT msgfilterlow;			/* suspend/resume control of message dispatching */
static INT msgfilterhigh;			/* suspend/resume control of message dispatching */
static INT commonDlgFlag = 0;

/**
 * Image rotation for draw function
 * This has different meanings depending on imageRotateOld
 *
 * Note. When irotation is set via the draw verb, it persists forever.
 * A chain does not reset it.
 */
static INT NoRotation = 0; /* NEW setting, old method value is 90 */
static INT irotation = 0;

static INT tableclassregistered;	/* boolean for lazy registration of the table window class */
static HANDLE showControlsEvent;		/* Used to coordinate the guiaShowControls and showcontrols functions */
static LRESULT gShowControlsReturnCode; /* Same as above */
static HANDLE eraseDropListControlEvent;	/* Used to coordinate the guiaChangeControl and erasedroplistbox functions */

/* flags set from dbcdx.cfg */
static INT winbackflag = FALSE;

/**
 * if TRUE then use guiaSetSingleLineEditCaretPos
 */
static INT editCaretOld = FALSE;

/**
 * if TRUE, then when an fedit box gets focus, all text will be selected
 */
static INT feditTextSelectionOnFocusOld = FALSE;

enum EnterKeyBehavior {
	enterKeyBehavior_normal = 0,
	enterKeyBehavior_old = 1,
	enterKeyBehavior_tab = 2
};

/* make ENTER tab to next control instead of give NKEY message */
static enum EnterKeyBehavior enterkeybehavior = enterKeyBehavior_normal;

static INT focuswinold = FALSE;		/* make change focus for control on background window not bring window to front */
static INT insertbeforeold = FALSE; /* if true, increment insert position after each insert */
static INT beepold = FALSE;
static UINT clipboardtextformat = CF_TEXT;
static INT bitbltErrorIgnore = FALSE;	/* In WM_PAINT processing, ignore any errors from BitBlt */
/**
 * This was added 24 Feb 2014 for Sandro at automotivesystems.de
 * It is a three way switch.
 * If DEFAULT this means special config keyword was not found. In this case, if running under
 * terminal server, erase early. If not running under TS, erase late.
 * If OLD, always erase late, regardless of TS.
 * If RDC, always erase early, regardless of TS.
 */
static erase_background_processing_style_t erasebkgndSpecial = EBPS_DEFAULT;
static INT trapeventid;			/* eventid to set when tray stop is requested */
static int imageDrawStretchMode = -1;
static BOOL coinitializeex = FALSE;
static BOOL imageRotateOld = FALSE;

static BYTE translatehexdigit[256];		/* used to translate ASCII Hex values ('0' - 'F') to palette indices quickly */

/* define table to translate Windows VK_* codes to DB/C key codes */
static USHORT translatekeycode[VK_F20 + 1] = {
	0x00,	0x00,	0x00,	0x00,	0x00,	/* 0 - 4 */
	0x00,	0x00,	0x00,	KEYBKSPC,	KEYTAB,	/* 5 - 9 */
	0x00,	0x00,	0x00,	KEYENTER,	0x00,	/* 10 - 14 */
	0x00,	0x00,	0x00,	0x00,	0x00,	/* 15 - 19 */
	0x00,	0x00,	0x00,	0x00,	0x00,	/* 20 - 24 */
	0x00,	0x00,	KEYESCAPE,0x00,	0x00,	/* 25 - 29 */
	0x00,	0x00,	0x00,	KEYPGUP,	KEYPGDN,	/* 30 - 34 */
	KEYEND,	KEYHOME,	KEYLEFT,	KEYUP,	KEYRIGHT,	/* 35 - 39 */
	KEYDOWN,	0x00,	0x00,	0x00,	0x00,	/* 40 - 44 */
	KEYINSERT,KEYDELETE,0x00,	0x00,	0x00,	/* 45 - 49 */
	0x00,	0x00,	0x00,	0x00,	0x00,	/* 50 - 54 */
	0x00,	0x00,	0x00,	0x00,	0x00,	/* 55 - 59 */
	0x00,	0x00,	0x00,	0x00,	0x00,	/* 60 - 64 */
	0x00,	0x00,	0x00,	0x00,	0x00,	/* 65 - 69 */
	0x00,	0x00,	0x00,	0x00,	0x00,	/* 70 - 74 */
	0x00,	0x00,	0x00,	0x00,	0x00,	/* 75 - 79 */
	0x00,	0x00,	0x00,	0x00,	0x00,	/* 80 - 84 */
	0x00,	0x00,	0x00,	0x00,	0x00,	/* 85 - 89 */
	0x00,	0x00,	0x00,	0x00,	0x00,	/* 90 - 94 */
	0x00,	0x00,	0x00,	0x00,	0x00,	/* 95 - 99 */
	0x00,	0x00,	0x00,	0x00,	0x00,	/* 100 - 104 */
	0x00,	0x00,	0x00,	0x00,	0x00,	/* 105 - 109 */
	0x00,	0x00,	KEYF1,	KEYF2,	KEYF3,	/* 110 - 114 */
	KEYF4,	KEYF5,	KEYF6,	KEYF7,	KEYF8,	/* 115 - 119 */
	KEYF9,	KEYF10,	KEYF11,	KEYF12,	KEYF13,	/* 120 - 124 */
	KEYF14,	KEYF15,	KEYF16,	KEYF17,	KEYF18,	/* 125 - 129 */
	KEYF19,	KEYF20 };							/* 130 - 131 */

#define move4llhh(src, dest) (* (BYTE *) (dest + 3) = ((BYTE) ((src) >> 24)), \
	* (BYTE *) (dest + 2) = ((BYTE) ((src) >> 16)), \
	* (BYTE *) (dest + 1) = ((BYTE) ((src) >> 8)), \
	* (BYTE *) (dest) = (BYTE) (src))
#define load4llhh(src) ((((UINT) *(src + 3)) << 24) + \
	(((UINT) *(src + 2)) << 16) + \
	(((UINT) *(src + 1)) << 8) + ((UINT) *(src)))

LRESULT CALLBACK guiaControlProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK guiaControlInactiveProc(HWND, UINT, WPARAM, LPARAM);
int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData);

static DWORD WINAPI aboutPopup(LPVOID unused);
static DWORD WINAPI trayShutdown(LPVOID unused);
static BOOL ActivateMenuItem(HWND, WORD);
static void acquireEditBoxText (CONTROL *control, INT *textlen);
static LONG addmenuitem(MENUENTRY *, UINT);
static void checkFloatWindowVisibility(HWND hwnd, WPARAM wParam);
static void checkForIconItem(PMENUENTRY pMenuEntry, LPMENUITEMINFO mii, HMENU hmenu);
static void checkHorizontalScrollBar(WINHANDLE wh);
static LONG checkScrollBars(WINHANDLE wh);
static void checkselchange(CONTROL *);
static INT checkSingleLineEditFocusNotification(WPARAM wParam, LPARAM lParam);
static INT checkVerticalScrollBar(WINHANDLE wh);
static void checkwinsize(WINHANDLE);
static void clearPBDefStyle(CONTROL*);
static HICON createiconfromres(RESOURCE *, INT, DWORD *error);
static HWND createwindowex(GUIAEXWINCREATESTRUCT *);
static int CustomTranslateAccelerator(HWND, HACCEL, LPMSG);
static LONG deletemenuitem(HMENU, UINT);
static LONG dlgcreate(GUIADLGCREATESTRUCT *);
static LONG dlgdestroy(HWND);
static BOOL doesWindowHaveAnyFocusableControls(WINHANDLE winhandle);
static LONG erasedroplistbox(CONTROL *);
static void eraseWindowBackground(HWND hwnd, PAINTSTRUCT *ps);
static void eraseWindowBackground_EX(HDC hPaintDC, PAINTSTRUCT *ps);
static CONTROL* findNextFocusableControl(CONTROL *startCtl);
static LONG getselectedtreeitemtext(GUIAGETWINTEXT *);
static LONG getwindowtext(GUIAGETWINTEXT *);
static INT guiaAddTooltip(CONTROL *);
static BOOL guiaadjusticonimage(LPICONIMAGE lpImage);
static INT guiaAltKeySetFocus(RESOURCE *, INT);
static INT32 guiaBitmapGetPixel(BITMAP, INT, INT, INT);
static BOOL guiaCenterWindow(HWND);
static void guiaChangeTableInsert(CONTROL* control, CHAR* string);
static INT guiaChangeTree(RESOURCE *res, CONTROL *control, INT iCmd);
static void guiaCheckScrollBars(WINHANDLE);
static UINT_PTR CALLBACK guiaCommonDlgProcOne(HWND, UINT, WPARAM, LPARAM);
static UINT_PTR CALLBACK guiaCommonDlgProcTwo(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK guiaDlgProc(HWND, UINT, WPARAM, LPARAM);
static INT guiaGetCtlTextBoxSize(CONTROL *, CHAR *, INT *, INT *);
static INT guiaGetSelectedTreeitemText(HWND, CHAR *, INT);
//static void guiaGetTextSize(PIXMAP *, UCHAR *, INT, INT *, INT *);
static INT guiaGrayIcon(RESOURCE *, HDC);
static BOOL guiaIsTableFocusable(CONTROL *control);
static void guiaMsgLoop(void);
static INT guiaParseFont(CHAR *, INT, LOGFONT *, INT, HDC);
static void guiaPixBitBlt(PIXMAP *, INT, INT, INT, INT, PIXMAP *, INT, INT, INT, INT);
static void guiaProcessAutoScrollBars(WINDOW *, UINT, WPARAM);
static void guiaProcessWinScrollBars(WINDOW *, UINT, WPARAM);
static INT guiaResCallBack(RESOURCE *, CONTROL *, WORD);
static void guiaScrollCallBack(RESOURCE *, CONTROL *, WPARAM);
static void guiaSetSingleLineEditCaretPos(CONTROL*);
static DWORD WINAPI startMainDXProcess(LPVOID);
static INT guiaThreadStopTest(void);
LRESULT CALLBACK guiaWinProc(HWND, UINT, WPARAM, LPARAM);
static void handleCheckboxStateChange(CONTROL *control, HWND hwnd);
static LRESULT handleCtlColorStatic(WPARAM wParam, LPARAM lParam);
static void handleGuiAvailable(CONTROL *control);
static void handleGuiGray(RESOURCE *res, CONTROL *control);
static void handleGuiNofocus(RESOURCE *res, CONTROL *control);
static void handleGuiReadonly(CONTROL *control);
static void handleGuiShowonly(RESOURCE *res, CONTROL *control);
static LRESULT handleTree_CustomDraw(CONTROL* control, LPNMTVCUSTOMDRAW lParam);
static LONG hidecontrols(GUIAHIDECONTROLS *);
static INT hidePixmap(PIXHANDLE ph, RECT *rect);
static INT isnummask(CHAR *, INT *, INT *);
//static INT makecontrol(CONTROL *, INT, INT, HWND, INT);
static LONG menucreate(RESHANDLE);
static LONG menudestroy(RESOURCE *);
static LONG popupmenushow(RESHANDLE);
static void saveMListboxSelectionState(CONTROL *);
//static LONG setctrlfocus(CONTROL *);
static INT setCtlTextBoxSize(RESOURCE *res, CONTROL *ctl);
static void setLastErrorMessage (CHAR *msg);
static void setMListboxSelectionState(CONTROL *);
static LONG settoolbartext(CONTROL *);
static LONG showcontrols(GUIASHOWCONTROLS *);
static INT showPixmap(PIXHANDLE ph, RECT *rect);
static LONG startdraw(PIXHANDLE);
static LONG statusbarcreate(WINDOW *);
static LONG statusbartext(WINDOW *);
static LONG statusbardestroy(WINDOW *);
static LONG toolbarcreate(RESOURCE *);
static LONG toolbarcreatedbox(GUIATOOLBARDROPBOX *);
static LONG toolbardestroy(RESOURCE *);
static void guiaSetTabs(CONTROL *);
static void guiaFEditInsertChar(CHAR *, CHAR *, CHAR *, INT, INT *);
static HBITMAP guiaGetRotatedBitmap(PIXMAP *, PIXMAP *, INT, INT *, INT *);
static LPICONRESOURCE guiareadiconfromICOfile(HANDLE hFile);
static UINT guiareadICOheader(HANDLE hFile);
static HICON guiamakeiconfromresource(LPICONIMAGE lpIcon);
static void setPBDefStyle(CONTROL*, INT);
static INT sendESELMessage(CONTROL *control, INT iInsertPos, INT iEndPos);
//static WNDPROC setControlWndProc(CONTROL *control);
static TREESTRUCT * treesearch(TREESTRUCT *, HTREEITEM);
static void treeinsert(HWND, TREESTRUCT *);
static void treedelete(HWND, INT, TREESTRUCT *);
static void treeposition(CHAR *, TREESTRUCT *, TREESTRUCT *);
static void wincb(WINDOW *, CHAR *, INT datalen);
static INT wincreate(GUIAWINCREATESTRUCT *);
static LONG windestroy(HWND);
static LONG winshowcaret(WINDOW *);
static LONG winhidecaret(WINDOW *);
static LONG winmovecaret(WINDOW *);

/**
 * Set to COLOR_BTNFACE
 */
//static HBRUSH hbrButtonFace;
static HBRUSH hbrWindow;
static DWORD mainDXThreadId;
static DWORD mainWindowsThread;
static NOTIFYICONDATA notifyIconData;
static HICON hIcon, hSmIcon;
/*
 * Used to keep track of the drawn-to rectangle during draw verb processing.
 */
static RECT guiaDrawInvalidRect;
/*
 * An event object.
 * In guiaEndDraw this is used to wait for the completion of the UpdateWindow call.
 * In the message thread, WM_ERASEBKGND message processing draws images and sets this event
 * to the signaled state when it is done. We do not want to return from the draw verb processing
 * in the main thread until all images have completed painting.
 */
static HANDLE hPaintingEvent;

static CHAR lastErrorMessage[128];

/**
 * August 2013 jpr
 * We don't like the fact that Windows will set a radio button to marked
 * when we request the focus by calling SetFocus.
 * So we set a flag and intercept the BM_SETCHECK message to suppress this
 * behavior.
 */
static BOOL radioButtonSuppressAutoCheck;

//#pragma comment(linker,"\"/manifestdependency:type='win32' \
//name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
//processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

/*
 * Windows entry point
 */
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	INT color;

	WNDCLASSEX wcClass;
	INT i, gotsmall, gotbig;
	LPICONRESOURCE	lpIR;
	HANDLE hFile;
	HMODULE shell32lib;		/* Pointer to the shell32 library */
	DLLVERSIONINFO2 dvi;
	DLLGETVERSIONPROC2 DllGetVersion;

	mainWindowsThread = GetCurrentThreadId();
	hinstancemain = hinst;
	//nextusermsgvalue = 0;
	/*
	 * Create a background brush for the OS to use on dialogs and our code
	 * to use on windows
	 */
	logbrushbackground.lbStyle = BS_SOLID;
	logbrushbackground.lbColor = GetSysColor(COLOR_BTNFACE);
	hbrButtonFace = CreateBrushIndirect(&logbrushbackground);


	if( (hFile = CreateFile("DBCICONS.ICO", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL )) == INVALID_HANDLE_VALUE )
	{
		hIcon = LoadImage(hinstancemain, DBCLARGEICONNAME, IMAGE_ICON, 0, 0, 0);
		hSmIcon = LoadImage(hinstancemain, DBCSMALLICONNAME, IMAGE_ICON, 0, 0, 0);
	}
	else {
		HICON hLocalIcon;
		gotbig = gotsmall = FALSE;
		lpIR = guiareadiconfromICOfile(hFile);
		for (i = 0; i < (INT) lpIR->nNumImages; i++) {
			if (gotbig && gotsmall) break;
			hLocalIcon = guiamakeiconfromresource(&(lpIR->IconImages[i]));
			if (lpIR->IconImages[i].Width == 16 && lpIR->IconImages[i].Height == 16)
			{
				gotsmall = TRUE;
				hSmIcon = hLocalIcon;
			}
			else if (lpIR->IconImages[i].Width == 32 && lpIR->IconImages[i].Height == 32)
			{
				gotbig = TRUE;
				hIcon = hLocalIcon;
			}
		}
		if (!gotsmall)
			hSmIcon = LoadImage(hinstancemain, DBCSMALLICONNAME, IMAGE_ICON, 0, 0, 0);
		if (!gotbig)
			hIcon = LoadImage(hinstancemain, DBCLARGEICONNAME, IMAGE_ICON, 0, 0, 0);
	}

	screenheight = GetSystemMetrics(SM_CYSCREEN);
	screenwidth = GetSystemMetrics(SM_CXSCREEN);

/* sub-class DB/C windows (and DB/C dialog windows) */
	wcClass.cbSize = sizeof(WNDCLASSEX);
	wcClass.style = CS_DBLCLKS | CS_OWNDC | CS_BYTEALIGNCLIENT;
	wcClass.lpfnWndProc = (LPVOID) guiaWinProc;
	wcClass.cbClsExtra = 0;
	wcClass.cbWndExtra = 0;
	wcClass.hInstance = hinstancemain;
	wcClass.hIcon = hIcon;
	wcClass.hIconSm = hSmIcon;
	wcClass.hCursor = NULL;  /* cursor is set at Window creation time */
	/*
	 * We leave the background brush null so that Windows will ask us to erase
	 * the background of our windows.
	 * We want that so that images can always be painted underneath panels
	 */
	wcClass.hbrBackground = NULL;
	wcClass.lpszMenuName = NULL;
	wcClass.lpszClassName = winclassname;
	if (!RegisterClassEx(&wcClass)) {
		DWORD err = GetLastError();
		svclogerror(DX_FATAL_A, err);
		guiaErrorMsg(DX_FATAL_A, err);
		return(1);
	}

	/*
	 * However for dialogs, we want a cursor defined in the winclass
	 */
	wcClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	/*
	 * AND we want a background brush
	 */
	wcClass.hbrBackground = hbrButtonFace;
	wcClass.lpfnWndProc = (LPVOID) guiaDlgProc;
	wcClass.hIcon = NULL;
	wcClass.hIconSm = NULL;
	wcClass.lpszClassName = dlgclassname;
	if (!(aDialogClass = RegisterClassEx(&wcClass))) {
		DWORD err = GetLastError();
		svclogerror(DX_FATAL_B, err);
		guiaErrorMsg(DX_FATAL_B, err);
		return(1);
	}

	/* Register the Popbox window class */
	if (!guiaRegisterPopbox(hinstancemain)) return (1);

	hpennull = GetStockObject(NULL_PEN);
	hbrushnull = GetStockObject(NULL_BRUSH);
	hbrWindow = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
	for (color = 0; color < 16; color++) {
		if (color >= 10) {
			translatehexdigit[color - 10 + 'A'] = color;
			translatehexdigit[color - 10 + 'a'] = color;
		}
		else translatehexdigit[color + '0'] = color;
	}

	GdiSetBatchLimit(255);

	/*
	 * Create main DB/C window as a 'message only' window
	 */
	hwndmainwin = CreateWindow(winclassname, pgmname, 0,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			HWND_MESSAGE, NULL, hinstancemain, NULL);
	if (hwndmainwin == NULL) {
		DWORD err = GetLastError();
		svclogerror(DX_FATAL_C, err);
		guiaErrorMsg(DX_FATAL_C, err);
		return(1);
	}

	/*
	 * This starts the main DX thread.
	 * Must start it after hwndmainwin is initialized
	 */
	if (CreateThread(NULL, 0, startMainDXProcess, NULL, 0, &mainDXThreadId) == NULL) {
		DWORD err = GetLastError();
		svclogerror(DX_FATAL_D, err);
		guiaErrorMsg(DX_FATAL_D, err);
		return(1);
	}

	/*
	 * If we are running on WinXP or newer, allow icons on menus.
	 * (Table from OSVERSIONINFOEX)
	 *
	 *		Operating system Version number
	 *		Windows 10                    10.0
	 *		Windows Server 2016           10.0
	 *		Windows Server 2012 R2         6.3
	 *		Windows 8.1                    6.3
	 *		Windows Server 2012            6.2
	 *		Windows 8                      6.2
	 *		Windows 7                      6.1
	 *		Windows Server 2008 R2         6.1
	 *		Windows Server 2008            6.0
	 *		Windows Vista		     	   6.0
	 *		Windows Server 2003 R2		   5.2
	 *		Windows Server 2003			   5.2
	 *		Windows XP					   5.1
	 *		Windows 2000				   5.0
	 *
	 *		Nothing earlier than this supported at all
	 */

	/*
	 * This is for the filtering option on a opendirdlg resource. Only works on Vista or newer.
	 */
	if (IsWindowsVistaOrGreater() /*osinfo.dwMajorVersion >= 6*/) {
		shell32lib = LoadLibrary((LPCSTR)"shell32");
		if (shell32lib != NULL)
			guiaSHCreateItemWithParent = (guiaPSHCreateItemWithParent) GetProcAddress(shell32lib, "SHCreateItemWithParent");
	}

	shell32_Major = dvi.info1.dwMajorVersion = 4;
	shell32_Minor = dvi.info1.dwMinorVersion = 0;
	shell32lib = LoadLibrary("Shell32.dll");
	if (shell32lib != NULL) {
		DllGetVersion = (DLLGETVERSIONPROC2)GetProcAddress(shell32lib, "DllGetVersion");
		if (DllGetVersion != NULL) {
			(DllGetVersion)(&dvi);
			shell32_Major = dvi.info1.dwMajorVersion;
			shell32_Minor = dvi.info1.dwMinorVersion;
			shell32_Build = dvi.info1.dwBuildNumber;
		}
		FreeLibrary(shell32lib);
	}

	/*
	 * This used when painting an icon on a menu item.
	 * And in the processing of messages from the tray icon
	 */
//	isWinXP = !IsWindowsVistaOrGreater()
			/*osinfo.dwMajorVersion == 5 && osinfo.dwMinorVersion == 1*/;

	guiaMsgLoop();
	if (notifyIconData.cbSize) Shell_NotifyIcon(NIM_DELETE, &notifyIconData);
	CloseWindow(hwndmainwin);
	DestroyWindow(hwndmainwin);
	DeleteObject(hbrButtonFace);
	DeleteObject(hbrWindow);
	if (coinitializeex) CoUninitialize();
	if (showControlsEvent != NULL) CloseHandle(showControlsEvent);
	if (eraseDropListControlEvent != NULL) CloseHandle(eraseDropListControlEvent);
	if (useTerminateProcess) TerminateProcess(GetCurrentProcess(), exitvalue);
	return(exitvalue);
}

/*
 * secondary thread which calls mainentry to start processing
 */
static DWORD WINAPI startMainDXProcess(LPVOID unused)
{
	GdiSetBatchLimit(255);
	mainentry(__argc, __argv, _environ);
	shutdownflag = TRUE;
	// Sending GUIAM_SHUTDOWN causes a call to PostQuitMessage
	SendMessage(hwndmainwin, GUIAM_SHUTDOWN, (WPARAM) 0, (LPARAM) 0);
	ExitThread(0);
	return 0;
}

void guiaInsertbeforeOld(void) {
	insertbeforeold = TRUE;
}

void guiaUseTransparentBlt(INT value) {
	useTransparentBlt = value;
}

void guiaUseTerminateProcess() {
	useTerminateProcess = TRUE;
}

void guiaEditCaretOld()
{
	editCaretOld = TRUE;
}

void guiaFeditTextSelectionOnFocusOld()
{
	feditTextSelectionOnFocusOld = TRUE;
}

void guiaSetImageDrawStretchMode(CHAR *ptr)
{
	if (strcmp(ptr, "1") == 0) imageDrawStretchMode = 1;
	else if (strcmp(ptr, "2") == 0) imageDrawStretchMode = 2;
	else if (strcmp(ptr, "3") == 0) imageDrawStretchMode = 3;
	else if (strcmp(ptr, "4") == 0) imageDrawStretchMode = 4;
}

void guiaEnterkey(CHAR *ptr)
{
	if (strcmp(ptr, "old") == 0) enterkeybehavior = enterKeyBehavior_old;
	else if (strcmp(ptr, "tab") == 0) enterkeybehavior = enterKeyBehavior_tab;
}

void guiaFocusOld()
{
	focuswinold = TRUE;
}

void guiaBeepOld()
{
	beepold = TRUE;
}

void guiaBitbltErrorIgnore()
{
	bitbltErrorIgnore = TRUE;
}

void guiaUseEditBoxClientEdge(INT state)
{
	useEditBoxClientEdge = state;
}

void guiaUseListBoxClientEdge(INT state)
{
	useListBoxClientEdge = state;
}

void guiaErasebkgndSpecial(erase_background_processing_style_t v1)
{
	erasebkgndSpecial = v1;
}

#if DX_MAJOR_VERSION >= 17
void guiaSetImageRotateOld(void)
{
	imageRotateOld = TRUE;
	irotation = NoRotation = 90; // 'EAST' is normal, no-rotation, in the old interpretation
}
#else
void guiaSetImageRotateNew(void)
{
	imageRotateOld = FALSE;
	irotation = NoRotation = 0;
}
#endif

void guiaClipboardCodepage(CHAR * cp)
{
	if (!strcmp(cp, "850") || !strcmp(cp, "oem")) clipboardtextformat = CF_OEMTEXT;
}

/**
 * GUI initialization
 * called from guiinit in gui.c
 *
 * Always returns zero.
 */
INT guiaInit(INT iEventID, CHAR *p1)
{
	HDC hdc;
	CHAR *ptr;

	if (!initcomctlflag) {
		INITCOMMONCONTROLSEX iccex;
		//InitCommonControls();
		iccex.dwICC = ICC_WIN95_CLASSES;
		iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
		InitCommonControlsEx(&iccex);
		initcomctlflag = TRUE;
	}
	breakeventid = iEventID;
	breakflag = FALSE;
	dlgreshandlelisthdr = NULL;
	hfontcontrolfont = NULL;
	if (p1 == NULL || strlen(p1) == 0) {
		CHAR work[32];
		sprintf(work, "SYSTEM(%d)", GUI_DEFAULT_FONT_SIZE);
		p1 = work;
	}
	hdc = GetDC(hwndmainwin);
	if (!guiaParseFont(p1, (INT)strlen(p1), &logfontcontrolfont, TRUE, hdc)) {
		hfontcontrolfont = getFont(&logfontcontrolfont);
		SelectObject(hdc, hfontcontrolfont);
	}
	ReleaseDC(hwndmainwin, hdc);

	/*
	 * Check for the temporary kludge to activate WM_WINDOWPOSCHANGING and WM_WINDOWPOSCHANGED
	 * messages to deal with the 'cannot restore minimized window' problem
	 *
	 * Dead as of 16.1
	 */
	//if (!prpget("gui", "windowrestore", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "kludge")) restoreKludge = TRUE;

	/*
	 * This is used for painting icons on menus
	 * It is only effective on WinXP.
	 * Note that testing the value is case sensitive.
	 */
	if (!prpget("gui", "menu", "icon", NULL, &ptr, 0) && !strcmp(ptr, "useTransparentBlt")) {
		useTransparentBlt = TRUE;
	}

	//
	// Detection of this option is now over in dbcctl.c
	// 21 Mar 13 jpr
	//
//	if (!prpget("gui", "editbox", "style", NULL, &ptr, 0) && !strcmp(ptr, "useClientEdge")) {
//		useEditBoxClientEdge = TRUE;
//	}

	hPaintingEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	return 0;
}

void guiaInitTrayIcon(INT trapid) {
	trapeventid = trapid;
	if (shell32_Major < 5) notifyIconData.cbSize = sizeof(NOTIFYICONDATA_V1_SIZE); // NT 4.0 and earlier
	else if (shell32_Major == 5) notifyIconData.cbSize = sizeof(NOTIFYICONDATA_V2_SIZE); // Win 2000
	else {
		if (shell32_Major > 6) notifyIconData.cbSize = sizeof(NOTIFYICONDATA); // future windows?
		else if (shell32_Minor < 1 ) { // Major.Minor must be 6.0; XP or Vista
			if (shell32_Build <= 2900) notifyIconData.cbSize = sizeof(NOTIFYICONDATA_V3_SIZE); // Win XP
			else notifyIconData.cbSize = sizeof(NOTIFYICONDATA); // Vista
		}
		else notifyIconData.cbSize = sizeof(NOTIFYICONDATA); // Win7
	}
	notifyIconData.uCallbackMessage = GUIAM_TRAYICON;
	notifyIconData.hWnd = hwndmainwin;
	notifyIconData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
	notifyIconData.hIcon = hIcon;
	notifyIconData.uID = TRAY_ICON_ID;
	sprintf(notifyIconData.szTip, "DX Runtime version %s", RELEASE);
	Shell_NotifyIcon(NIM_ADD, &notifyIconData);
	if (shell32_Major >= 5) {
		notifyIconData.uVersion = NOTIFYICON_VERSION_4;
		Shell_NotifyIcon(NIM_SETVERSION, &notifyIconData);
	}
}

/* GUI termination */
INT guiaExit(INT exitval)
{
	INT i1;

	if (notifyIconData.cbSize) Shell_NotifyIcon(NIM_DELETE, &notifyIconData);
	if (ddeinitflg) {
		for (i1 = 0; i1 < ddemax && !(ddehead[i1].type & DDE_TYPE_WINDOW); i1++);
		if (i1 < ddemax) {
			Sleep(2000);  /* give the dde windows a chance to be destroyed */
			for (i1 = 0; i1 < ddemax; i1++) {
				if (ddehead[i1].type & DDE_TYPE_WINDOW)
					SendMessage(hwndmainwin, GUIAM_DDEDESTROY, (WPARAM) 0, (LPARAM) &ddehead[i1]);
			}
		}
		ddeinitflg = FALSE;
	}
	aboutline1 = aboutline2 = NULL;
	exitvalue = exitval;
	shutdownflag = TRUE;
	guiaResume(); /* removes all message filters and synchronize message thread */
	/*
	 * The below SendMessage commented out on 03/18/2011, it is useless to
	 * send this message to our 'message-only' main window
	 */
 	//SendMessage(hwndmainwin, WM_SYSCOMMAND, (WPARAM) SC_CLOSE, (LPARAM) 0);
	return 0;
}

static void handleCheckboxStateChange(CONTROL *control, HWND hwnd)
{
	if (ISMARKED(control) /* if marked before the click */) {
		control->style &= ~CTRLSTYLE_MARKED;
		PostMessage(hwnd, BM_SETCHECK, BST_UNCHECKED, 0);
	}
	else {
		control->style |= CTRLSTYLE_MARKED;
		PostMessage(hwnd, BM_SETCHECK, BST_CHECKED, 0);
	}
}

/**
 * Creates a popup menu when the user right clicks the tray icon
 */
static BOOL doTrayPopup(INT x, INT y) {
	BOOL ret;
	BOOL allowStop = TRUE;	// By default, we allow it
	CHAR work[256], *ptr;
	HMENU traymenu;
	INT i1;

	traymenu = CreatePopupMenu();
	if (traymenu == NULL) {
		sprintf_s(work, ARRAYSIZE(work), "In %s, Failed call to CreatePopupMenu - %d",
				__FUNCTION__, (int)GetLastError());
		MessageBox(hwndmainwin, work, RELEASEPROGRAM RELEASE, MB_ICONERROR | MB_SETFOREGROUND);
		return 0;
	}

	i1 = prpget("gui", "traymenu", "stop", NULL, &ptr, PRP_LOWER);
	if (!i1) {
		if (!strcmp(ptr, "no") || !strcmp(ptr, "false") || !strcmp(ptr, "off")) allowStop = FALSE;
	}
	if (allowStop) {
		AppendMenu(traymenu, MF_STRING, 900, "Stop");
		AppendMenu(traymenu, MF_SEPARATOR, 0, NULL);
	}
	AppendMenu(traymenu, MF_STRING, 100, "About...");
	SetForegroundWindow(hwndmainwin);
	ret = TrackPopupMenuEx(traymenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_NONOTIFY | TPM_RETURNCMD
			| TPM_NOANIMATION | TPM_RIGHTBUTTON, x, y, hwndmainwin, NULL);
	DestroyMenu(traymenu);
	SendMessage(hwndmainwin, WM_NULL, 0, 0);
	return ret;
}

/*
 * Called when the user clicks the 'about' item in the tray icon
 * popup menu.
 */
static DWORD WINAPI aboutPopup(LPVOID unused) // @suppress("No return")
{
	MessageBox(hwndmainwin, DXAboutInfoString, "About DB/C", MB_OK | MB_SETFOREGROUND);
	ExitThread(0);
}

/*
 * Called when the user clicks the 'shutdown' item in the tray icon
 * popup menu.
 */
static DWORD WINAPI trayShutdown(LPVOID unused) // @suppress("No return")
{
	dbcflags |= DBCFLAG_TRAYSTOP;
	evtset(trapeventid);
	ExitThread(0);
}

/* display a message box */
void guiaMsgBox(ZHANDLE title, ZHANDLE msg)
{
	int i1;
	if (IsOS(OS_TERMINALCLIENT)) Beep(400, 250);
	else MessageBeep(MB_OK);
	i1 = MessageBox(NULL, (CHAR *) msg, (CHAR *) title, MB_OK | MB_SETFOREGROUND | MB_APPLMODAL);
}

/**
 * message "hook" function for open and save file common dialogs
 */
UINT_PTR CALLBACK guiaCommonDlgProcOne(HWND hwnd, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	LPNMHDR nmhdr;
/* NOTE: CDN_INITDONE is received after commdlg is processed. */
	if (WM_NOTIFY == nMessage) {
		nmhdr = (LPNMHDR)lParam;
		if (nmhdr->code == CDN_INITDONE) {
			guiaCenterWindow(GetParent(hwnd));
			return(TRUE);
		}
	}
	return(FALSE);
}

/**
 * message "hook" function for color and font picker common dialogs
 */
UINT_PTR CALLBACK guiaCommonDlgProcTwo(HWND hwnd, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
/* NOTE: WM_INITDIALOG is received after commdlg is processed. */
	if (WM_INITDIALOG == nMessage) {
		guiaCenterWindow(hwnd);
		return(TRUE);
	}
	return(FALSE);
}

BOOL guiaCenterWindow(HWND hwnd)
{
	RECT rcWin;
	INT cxWin, cyWin, cxScreen, cyScreen, xNew, yNew;
	GetWindowRect(hwnd, &rcWin);
	cxWin = rcWin.right - rcWin.left;
	cyWin = rcWin.bottom - rcWin.top;
	cxScreen = GetSystemMetrics (SM_CXSCREEN);
	cyScreen = GetSystemMetrics (SM_CYSCREEN);
	xNew = (cxScreen - cxWin) / 2;
	yNew = (cyScreen - cyWin) / 2;
	return SetWindowPos(hwnd, NULL, xNew, yNew, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/* beep */
void guiaBeep(INT pitch, INT tenths, INT beepflag)
{
	if (beepflag && !beepold) {
		MessageBeep(MB_OK);
	}
	else {
//		if (osinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
			/* The operating system is Windows Me, Windows 98, or Windows 95. */
//			_outp(0x42, (BYTE) pitch);  /* low order pitch value */
//			_outp(0x42, (BYTE)(pitch >> 8));  /* high order pitch value */
//			_outp(0x61, _inp(0x61) | 0x03);
//			Sleep(tenths * 100);
//			_outp(0x61, _inp(0x61) & ~0x03);
//		}
		/*else*/ Beep(pitch, tenths * 100);
	}
}

/* execute a task */
//INT guiaExecTask(ZHANDLE zhCmd, INT bkgdflag, void (*fpDspFnc)(CHAR *))
//{
//	STARTUPINFO suiInfo;
//	PROCESS_INFORMATION piInfo;
//	DWORD dwExitCode;
//
//	if (NULL == zhCmd || !zhCmd[0]) {
//		(*fpDspFnc)("Null command line\n");
//		return RC_ERROR;
//	}
//	memset(&suiInfo, 0, sizeof(STARTUPINFO));
//	suiInfo.cb = sizeof(STARTUPINFO);
//	if (bkgdflag) {
//		suiInfo.dwFlags = STARTF_USESHOWWINDOW;
//		suiInfo.wShowWindow = SW_SHOWMINNOACTIVE;
//	}
//	if (!CreateProcess(NULL, (CHAR *) zhCmd, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &suiInfo, &piInfo)) {
//		(*fpDspFnc)("Unable to create process\n");
//		return RC_ERROR;
//	}
//	if (WAIT_FAILED == WaitForSingleObject(piInfo.hProcess, INFINITE)) {
//		(*fpDspFnc)("Unable to wait for process to terminate\n");
//		return RC_ERROR;
//	}
//	if (!GetExitCodeProcess(piInfo.hProcess, &dwExitCode)) {
//		(*fpDspFnc)("Unable to get process exit code\n");
//		return RC_ERROR;
//	}
//	return(dwExitCode);
//}

/* suspend processing of any messages other than GUIAM_ messages */
void guiaSuspend(void)
{
	if (shutdownflag) return;
	msgfilterlow = GUIAM_LOWMESSAGE;
	msgfilterhigh = GUIAM_HIGHMESSAGE;
	PostMessage(hwndmainwin, GUIAM_SYNC, (WPARAM) 0, (LPARAM) 0);
}

/* resume normal message processing */
void guiaResume(void)
{
	msgfilterlow = msgfilterhigh = 0;
	PostMessage(hwndmainwin, GUIAM_SYNC, (WPARAM) 0, (LPARAM) 0);
}

static BOOL ActivateMenuItem(HWND hWnd, WORD wID) {
	MENUITEMINFO mii;

	mii.cbSize = sizeof(MENUITEMINFO);
	mii.fMask = MIIM_STATE;
	GetMenuItemInfo(GetMenu(hWnd), wID, FALSE, &mii);
	if (mii.fState & MFS_GRAYED) {
		return TRUE; /* change this to FALSE to generate NKEY message */
	}
	HiliteMenuItem(hWnd, GetMenu(hWnd), wID, MF_BYCOMMAND | MF_HILITE);
	SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(wID, 1), 0L);
	HiliteMenuItem(hWnd, GetMenu(hWnd), wID, MF_BYCOMMAND | MF_UNHILITE);
	return TRUE;  /* key combination found and processed */
}


static int CustomTranslateAccelerator(HWND hWnd, HACCEL hAccelSrc, LPMSG lpMsg) {
	int accelSize, i, j;
	RESOURCE **menu;
	LPACCEL aaclAccelTable = NULL;
	if ((lpMsg->message == WM_KEYDOWN) || (lpMsg->message == WM_SYSKEYDOWN)) {
		menu = (*((WINHANDLE) GetWindowLongPtr(hWnd, GWLP_USERDATA)))->showmenu;
		if (menu == NULL) return FALSE;
		accelSize = (*menu)->speedkeys;
		aaclAccelTable = (ACCEL *) guiAllocMem(sizeof(ACCEL) * accelSize);
		if (aaclAccelTable == NULL) {
			guiaErrorMsg("Out of memory error in CustomTranslateAccelerator", sizeof(ACCEL) * accelSize);
			return FALSE;
		}
		j = CopyAcceleratorTable(hAccelSrc, aaclAccelTable, accelSize);
		for (i = 0; i < accelSize; i++) {
			if (lpMsg->wParam == aaclAccelTable[i].key)
				switch (aaclAccelTable[i].fVirt) {
					case FVIRTKEY:
						if ((!(GetKeyState(VK_CONTROL) & 0x1000)) && (!(GetKeyState(VK_SHIFT) & 0x1000)) && (!(GetKeyState(VK_MENU) & 0x1000)))
							return ActivateMenuItem(hWnd, aaclAccelTable[i].cmd);
						break;
					case FVIRTKEY | FCONTROL:
						if ((GetKeyState(VK_CONTROL) & 0x1000) && (!(GetKeyState(VK_SHIFT) & 0x1000)) && (!(GetKeyState(VK_MENU) & 0x1000)))
							return ActivateMenuItem(hWnd, aaclAccelTable[i].cmd);
						break;
					case FVIRTKEY | FSHIFT:
						if ((GetKeyState(VK_SHIFT) & 0x1000) && (!(GetKeyState(VK_CONTROL) & 0x1000)) &&(!(GetKeyState(VK_MENU) & 0x1000)))
							return ActivateMenuItem(hWnd, aaclAccelTable[i].cmd);
						break;
					case FVIRTKEY | FALT:
						if ((GetKeyState(VK_MENU) & 0x1000) && (!(GetKeyState(VK_SHIFT) & 0x1000)) &&(!(GetKeyState(VK_CONTROL) & 0x1000)))
							return ActivateMenuItem(hWnd, aaclAccelTable[i].cmd);
						break;
					case FVIRTKEY | FCONTROL | FSHIFT:
						if ((GetKeyState(VK_SHIFT) & 0x1000) && (GetKeyState(VK_CONTROL) & 0x1000) && (!(GetKeyState(VK_MENU) & 0x1000)))
							return ActivateMenuItem(hWnd, aaclAccelTable[i].cmd);
						break;
				}
		}
		guiFreeMem((ZHANDLE) aaclAccelTable);
	}
	return FALSE;
}

/* thread that manages message queues, creating/destroying windows */
static void guiaMsgLoop(void)
{
	MSG msg;
	BOOL bRet;

	while ((bRet = GetMessage(&msg, NULL, msgfilterlow, msgfilterhigh)) != 0) {
		if (bRet == -1) {
			guiaErrorMsg("Fatal Error - from GetMessage in guiaMsgLoop", (exitvalue = GetLastError()));
			PostMessage(hwndmainwin, GUIAM_SHUTDOWN, (WPARAM) 0, (LPARAM) 0);
		}
		if (haccelfocus != NULL && hwndfocuswin != NULL) {
			if (!CustomTranslateAccelerator(hwndfocuswin, haccelfocus, &msg)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
 	if (!breakflag) {
		breakflag = TRUE;
		evtset(breakeventid);
	}
	while (!guiaThreadStopTest());
}

/* test to see if secondary thread is done yet */
static INT guiaThreadStopTest(void)
{
	return(shutdownflag);
}

/**
 * Called in response to WM_CTLCOLORLISTBOX or WM_CTLCOLOREDIT
 * messages to the window proc.
 * TREE widgets also call this function upon creation.
 * Always returns a handle to a brush
 */
static LRESULT handleCtlColorEdit(HWND hwnd, WPARAM wParam, LPARAM lParam) {

	CONTROL *control = (CONTROL *) GetWindowLongPtr((HWND) lParam, GWLP_USERDATA);
	if (control != NULL) {
		SetBkColor((HDC) wParam,  GetSysColor(COLOR_WINDOW));
		guiaSetControlTextColor((HDC) wParam, control, RGB(0, 0, 0));
		if (ISLISTBOX(control)) {
			return (LRESULT) ((ISSHOWONLY(control)) ? hbrButtonFace : hbrWindow);
		}
	}
	return (LRESULT) hbrWindow;
}

/**
 * wParam is the HDC that we alter
 * lParam is the HWND of the control making this call to us.
 */
static LRESULT handleCtlColorStatic(WPARAM wParam, LPARAM lParam) {

	CONTROL *control = (CONTROL *) GetWindowLongPtr((HWND) lParam, GWLP_USERDATA);

	/**
	 * Sometimes, usually when running under terminal services, this gets called too soon.
	 * It gets called by Windows during the processing of CreateWindowEx over in makecontrol,
	 * before we can call SetWindowLongPtr to save the *control in the HWND.
	 * Seems to only happen to gray edit boxes.
	 * So, if NULL and it is an edit box, do something reasonable.
	 */
	if (control == NULL) {
		CHAR buff[64];
		if (GetClassName((HWND)lParam, buff, 64)) {
			if (strcmp(buff, WC_EDIT) == 0) {
				SetBkColor((HDC) wParam,  GetSysColor(COLOR_3DFACE));
				return (LRESULT) hbrButtonFace;
			}
		}
	}

	if (ISEDIT(control)) {
		// Takes an HDC and a COLORREF
		SetBkColor((HDC) wParam,  GetSysColor(COLOR_3DFACE));
		guiaSetControlTextColor((HDC) wParam, control, RGB(0, 0, 0));
		return (LRESULT) hbrButtonFace;
	}

	// Special section for popbox as we allow new change command here, 'textbgcolor'
	if (control->type == PANEL_POPBOX) {
		SetBkColor((HDC) wParam, (control->userBGColor) ?  control->textbgcolor : GetSysColor(COLOR_BTNFACE));
		guiaSetControlTextColor((HDC) wParam, control, RGB(0, 0, 0));
		return (LRESULT)NULL;
	}

	if (ISTEXT(control) || control->type == PANEL_BOX || control->type == PANEL_BOXTITLE || ISRADIO(control)) {
		SetBkColor((HDC) wParam,  GetSysColor(COLOR_BTNFACE));
		if (control->type != PANEL_BOX) guiaSetControlTextColor((HDC) wParam, control, RGB(0, 0, 0));
		return (LRESULT) hbrButtonFace;
	}
	return 0;
}


static void setMListboxSelectionState(CONTROL *control) {
	INT i2;
	for (i2 = 0; i2 < control->value1; i2++) {
		if (control->itemattributes2[i2].attr1 & LISTSTYLE_SELECTED << 24) {
			SendMessage(control->ctlhandle, LB_SETSEL, TRUE, i2);
		}
		else {
			SendMessage(control->ctlhandle, LB_SETSEL, FALSE, i2);
		}
	}
}

static void saveMListboxSelectionState(CONTROL *control) {
	INT i1;
	for (i1 = 0; i1 < control->value1; i1++) {
		if (SendMessage(control->ctlhandle, LB_GETSEL, i1, 0L)) {
			control->itemattributes2[i1].attr1 |= LISTSTYLE_SELECTED << 24;
		}
		else {
			control->itemattributes2[i1].attr1 &= ~(LISTSTYLE_SELECTED << 24);
		}
	}
}

static void checkselchange(CONTROL *control) {
	INT iInsertPos, iEndPos;

	if (!ISEDIT(control))
		return;
	SendMessage(control->ctlhandle, EM_GETSEL, (WPARAM) &iInsertPos, (LPARAM) &iEndPos);
	if ((iInsertPos != control->editinsertpos || iEndPos != control->editendpos)
			&& !((iEndPos - iInsertPos == 0) && (control->editendpos - control->editinsertpos == 0))) {
		sendESELMessage(control, iInsertPos, iEndPos);
	}
	return;
}

static INT sendESELMessage(CONTROL *control, INT iInsertPos, INT iEndPos)
{
	if (iEndPos + 1 > (INT)((sizeof(cbmsg) - 17) / sizeof(CHAR))) {
		guiaErrorMsg(DX_FATAL_15, 0);
		return RC_ERROR;
	}
	SendMessage(control->ctlhandle, WM_GETTEXT, iEndPos + 1, (LPARAM) cbmsgdata);
	if (iInsertPos) memmove(cbmsgdata, cbmsgdata + iInsertPos, iEndPos - iInsertPos);
	control->editinsertpos = iInsertPos;
	control->editendpos = iEndPos;
	pvistart();
	rescb(*control->res, DBC_MSGEDITSELECT, control->useritem, iEndPos - iInsertPos);
	pviend();
	return 0;
}

/*
 * The position fields in pwcsCreateStruct are one-based
 */
static INT wincreate(GUIAWINCREATESTRUCT *pwcsCreateStruct)
{
	WINDOW *win;
	DWORD dwStyle, dwExStyle = 0;
	INT iWinWidth, iWinHeight, vsize;
	RECT rect, clientrect;
	HMENU hmenuSystem;
	HWND hWndInsertAfter;

	win = *pwcsCreateStruct->wh;
	win->hcursor = LoadCursor(NULL, IDC_ARROW);
	win->debugflg = pwcsCreateStruct->style & WINDOW_STYLE_DEBUG; // I don't think this ever get sets anymore??
	win->pandlgscale = pwcsCreateStruct->style & WINDOW_STYLE_PANDLGSCALE;
	win->nofocus = pwcsCreateStruct->style & WINDOW_STYLE_NOFOCUS;
	win->taskbarbutton = !(pwcsCreateStruct->style & WINDOW_STYLE_NOTASKBARBUTTON);
	win->floatwindow = pwcsCreateStruct->style & WINDOW_STYLE_FLOATWINDOW;

	GetWindowRect(GetDesktopWindow(), &rect);
	if (pwcsCreateStruct->style & WINDOW_STYLE_SIZEABLE || pwcsCreateStruct->style & WINDOW_STYLE_MAXIMIZE) {
		dwStyle = WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL;
		if (pwcsCreateStruct->style & WINDOW_STYLE_MAXIMIZE) {
			dwStyle |= WS_MAXIMIZE;
			SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
		}
		win->sbflg = TRUE;
		if (pwcsCreateStruct->style & WINDOW_STYLE_AUTOSCROLL) win->autoscroll = TRUE;
		else win->autoscroll = FALSE;
	}
	else {  /* window is not sizeable */
		if (win->floatwindow) {
			/* TODO prevent menu, and toolbar from being added. */
			/* The NOCLOSE attribute is implemented here by omitting WS_SYSMENU */
			dwStyle = WS_OVERLAPPED | WS_BORDER;
			if (!(pwcsCreateStruct->style & WINDOW_STYLE_NOCLOSEBUTTON)) dwStyle |= WS_SYSMENU;
			if (pwcsCreateStruct->style & WINDOW_STYLE_AUTOSCROLL) {
				dwStyle |= WS_HSCROLL | WS_VSCROLL;
				win->autoscroll = TRUE;
			}
			else win->autoscroll = FALSE;
			win->sbflg = TRUE;
		}
		else {
			dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_HSCROLL | WS_VSCROLL | WS_BORDER;
			if (pwcsCreateStruct->style & WINDOW_STYLE_MAXIMIZE) dwStyle |= WS_MAXIMIZE;
			win->sbflg = FALSE;
			win->autoscroll = FALSE;
		}
	}

	if (win->floatwindow) {
		win->hwnd = CreateWindowEx(WS_EX_TOOLWINDOW, winclassname, (CHAR *) guiMemToPtr(win->title), dwStyle,
				rect.left, rect.top, rect.right, rect.bottom,
				win->ownerhwnd, /* hWndParent */
				NULL, hinstancemain, NULL);
	}
	else {
		if (!win->taskbarbutton) dwExStyle = WS_EX_NOACTIVATE;
		win->hwnd = CreateWindowEx(dwExStyle, winclassname, (CHAR *) guiMemToPtr(win->title), dwStyle,
			rect.left, rect.top, rect.right, rect.bottom,
			NULL/*hwndmainwin*/, NULL, hinstancemain, NULL);
	}

	if (win->hwnd == NULL) {
		guiaErrorMsg("Create window error", GetLastError());
		return RC_ERROR;
	}

	/* Create tooltip window for win->hwnd */
	win->hwndtt = CreateWindow(TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        win->hwnd, //NULL,
        NULL, hinstancemain, NULL
    );

	if (win->hwndtt == NULL) {
		guiaErrorMsg("Create TT window error", GetLastError());
		return RC_ERROR;
	}
	SetWindowLongPtr(win->hwnd, GWLP_USERDATA, (LONG_PTR) win->thiswinhandle);

	if (pwcsCreateStruct->style & WINDOW_STYLE_STATUSBAR) {
		win->hwndstatusbar = CreateWindow(STATUSCLASSNAME,
			(LPCSTR) NULL, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
			win->hwnd, (HMENU) ITEM_MSG_STATUSBAR, hinstancemain, NULL);
		if (win->hwndstatusbar == NULL) {
			guiaErrorMsg("Create statusbar error", GetLastError());
			return RC_ERROR;
		}
		SendMessage(win->hwndstatusbar, SB_SIMPLE, (WPARAM) TRUE, (LPARAM) 0);
		SendMessage(win->hwndstatusbar, WM_SETFONT, (WPARAM) hfontcontrolfont, MAKELONG((DWORD) TRUE, 0));
		win->statusbarshown = TRUE;
	}

	if (pwcsCreateStruct->style & WINDOW_STYLE_SIZEABLE) {
		if (!(pwcsCreateStruct->style & WINDOW_STYLE_AUTOSCROLL)) {
			win->vsbflg = win->hsbflg = TRUE;
		}
	}
	ShowScrollBar(win->hwnd, SB_BOTH, FALSE);
	if (pwcsCreateStruct->style & WINDOW_STYLE_NOCLOSEBUTTON && !win->floatwindow) {
		hmenuSystem = GetSystemMenu(win->hwnd, 0);
		DeleteMenu(hmenuSystem, SC_CLOSE, MF_BYCOMMAND);
	}

	if (pwcsCreateStruct->style & WINDOW_STYLE_MAXIMIZE) {
		SetWindowPos(win->hwnd, HWND_TOP, 0, 0, 0, 0,
			(win->nofocus ? SWP_NOACTIVATE : 0) | SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE
		);
	}
	else {
		GetWindowRect(win->hwnd, &rect);
		GetClientRect(win->hwnd, &clientrect);
		iWinWidth = rect.right - clientrect.right + pwcsCreateStruct->clienthsize;
		iWinHeight = rect.bottom - clientrect.bottom + pwcsCreateStruct->clientvsize;
		if (win->statusbarshown) {
			GetWindowRect(win->hwndstatusbar, &rect);
			vsize = rect.bottom - rect.top;
			win->borderbtm = vsize;
			iWinHeight += vsize;
		}
		if (pwcsCreateStruct->style & WINDOW_STYLE_CENTERED) {
			pwcsCreateStruct->hpos = (screenwidth - iWinWidth) >> 1;
			pwcsCreateStruct->vpos = (screenheight - iWinHeight) >> 1;
		}
		else {
			pwcsCreateStruct->hpos -= 1;
			pwcsCreateStruct->vpos -= 1;
		}
		if (win->floatwindow && win->ownerhwnd == NULL) hWndInsertAfter = HWND_TOPMOST;
		else hWndInsertAfter = HWND_TOP;

		SetWindowPos(win->hwnd, hWndInsertAfter, pwcsCreateStruct->hpos, pwcsCreateStruct->vpos,
			iWinWidth, iWinHeight, (win->nofocus ? SWP_NOACTIVATE : 0) | SWP_SHOWWINDOW
		);
	}
	GetClientRect(win->hwnd, &clientrect);
	win->clientx = clientrect.right;
	win->clienty = clientrect.bottom;
	win->hsicon = NULL;
	win->hbicon = NULL;
	if (dlgreshandlelisthdr != NULL && !win->nofocus) {
		if (GetActiveWindow() != (*dlgreshandlelisthdr)->hwnd) {
			SetActiveWindow((*dlgreshandlelisthdr)->hwnd);
		}
	}
    SetWindowPos(win->hwndtt, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    /* Set a default maximum width of 40 percent of the screen width */
    guiaChangeWinTTWidth(win, (INT) (0.40 * screenwidth));
	return 0;
}

/**
 * Change the given Windows's Tooltip window width
 *
 * @return always return zero, this message cannot produce an error.
 *
 */
INT guiaChangeWinTTWidth(WINDOW *win, INT width) {
	SendMessage(win->hwndtt, TTM_SETMAXTIPWIDTH, (WPARAM) 0, (LPARAM) width);
	return 0;
}

static LONG windestroy(HWND hwnd)
{
	if (hwnd == hwndfocuswin) {
		hwndfocuswin = NULL;
		haccelfocus = NULL;
	}
	if (!DestroyWindow(hwnd)) { /* this will also destroy child windows, like tooltip window */
		return RC_ERROR;
	}
	return 0;
}

/**
 * Always runs on the Windows message thread.
 * Called by our message GUIAM_DLGCREATE
 * Will return 0 or RC_ERROR
 */
static LONG dlgcreate(GUIADLGCREATESTRUCT *pdcsCreateStruct)
{
	DWORD dwStyle;
	RECT rect;
	RESOURCE *res;
	HMENU sysmenu;

	ReleaseCapture();
	res = pdcsCreateStruct->res;
	if (pdcsCreateStruct->hpos == INT_MIN || pdcsCreateStruct->vpos == INT_MIN) {  /* center dialog box */
		pdcsCreateStruct->hpos = (screenwidth - res->rect.right) >> 1;
		pdcsCreateStruct->vpos = (screenheight - res->rect.bottom) >> 1;
	}
	rect.left = rect.top = 0;
	rect.right = res->rect.right;
	rect.bottom = res->rect.bottom;
	dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_POPUP;
	if (!res->noclosebuttonflag)  dwStyle |= WS_SYSMENU;
	AdjustWindowRectEx(&rect, dwStyle, FALSE, WS_EX_DLGMODALFRAME);
	rect.right -= rect.left;
	rect.bottom -= rect.top;

	/**
	 * 17 SEP 2012 jpr
	 * Change here to make dialogs work better with the taskbar and the Alt-Tab thing.
	 * The 'Owner' window was the main (invisible) DX main window.
	 * Change it to the db/c created window that the dialog is being shown on.
	 */
	res->hwnd = CreateWindowEx(WS_EX_DLGMODALFRAME, dlgclassname, (CHAR *) res->title,
		dwStyle, pdcsCreateStruct->hpos, pdcsCreateStruct->vpos,
		rect.right, rect.bottom,

		/* Parent or owner window */
		(*(pdcsCreateStruct->res->winshow))->hwnd,
		/*hwndmainwin, This argument was the 'main' DX window, which always exists, is invisible,
		               and never has a taskbar button */

		NULL,			/* Menu */
		hinstancemain, NULL
	);

	if (res->hwnd == NULL) {
		CHAR work[128];
		INT i1 = GetLastError();
		sprintf_s(work, ARRAYSIZE(work), "In %s CreateWindowEx fail code = %d", __FUNCTION__, i1);
		setLastErrorMessage(work);
		return RC_ERROR;
	}
	SetWindowLongPtr(res->hwnd, GWLP_USERDATA, (LONG_PTR) res->thisreshandle);
	res->nextshowndlg = dlgreshandlelisthdr;	/* add dialog to front of linked list of shown dialogs */
	dlgreshandlelisthdr = res->thisreshandle;
	sysmenu = GetSystemMenu(res->hwnd, FALSE);
	if (sysmenu != NULL) {
		DeleteMenu(sysmenu, SC_MINIMIZE, MF_BYCOMMAND);
		DeleteMenu(sysmenu, SC_MAXIMIZE, MF_BYCOMMAND);
		DeleteMenu(sysmenu, SC_SIZE, MF_BYCOMMAND);
		DeleteMenu(sysmenu, SC_RESTORE, MF_BYCOMMAND);
	}
	return 0;
}

/*
 * This function is run on the Windows message thread.
 * Returns zero or RC_ERROR
 */
static LONG dlgdestroy(HWND hwnd)
{
	RESHANDLE currentreshandle, *prhLastHandle;

#if 0
	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG) NULL);
#endif
	currentreshandle = dlgreshandlelisthdr;
	prhLastHandle = &dlgreshandlelisthdr;
	while (currentreshandle != NULL) {
		if ((*currentreshandle)->hwnd == hwnd) {
			*prhLastHandle = (*currentreshandle)->nextshowndlg;
			break;
		}
		prhLastHandle = &(*currentreshandle)->nextshowndlg;
		currentreshandle = (*currentreshandle)->nextshowndlg;
	}
	if (IsWindow(hwnd)) {
		if (!DestroyWindow(hwnd)) {
			CHAR work[128];
			INT i1 = GetLastError();
			sprintf_s(work, ARRAYSIZE(work), "In %s DestroyWindow fail %d", __FUNCTION__, i1);
			setLastErrorMessage(work);
			return RC_ERROR;
		}
	}
	return 0;
}

/*
 * This function is run on the Windows message thread.
 * Always returns zero
 */
static LONG showcontrols(GUIASHOWCONTROLS *pscsControlStruct)
{
	INT i1, width, height, textheight, tabgroupxoffset, tabgroupyoffset;
	INT iCurCtrl, iXPos, iYPos;
	RESOURCE *res;
	WINDOW *win;
	CONTROL *control;
	RECT rect;
	HWND tabgroupctlhandle, foregroundwin = 0x0000;

	foregroundwin = GetForegroundWindow();
	res = pscsControlStruct->res;
	win = *res->winshow;
	control = (CONTROL *) res->content;
	res->ctldefault = NULL;
	textheight = -1;
	winbackflag = FALSE;
	res->showoffset.x = pscsControlStruct->hpos;
	if (res->showoffset.x == INT_MIN) res->showoffset.x = 0;
	res->showoffset.y = pscsControlStruct->vpos;
	if (res->showoffset.y == INT_MIN) res->showoffset.y = 0;

	if (foregroundwin != pscsControlStruct->hwnd) winbackflag = TRUE;
	for (iCurCtrl = 0; iCurCtrl < res->entrycount; iCurCtrl++, control++) {
		control->ctloffset = iCurCtrl;

		if (res->restype != RES_TYPE_DIALOG) {
			iXPos = 0 - win->scrollx;
			iYPos = win->bordertop - win->scrolly;
			if (res->restype == RES_TYPE_PANEL) {
				iXPos += res->showoffset.x;
				iYPos += res->showoffset.y;
			}
		}
		else iXPos = iYPos = 0;
		control->tabgroupxoffset = control->tabgroupyoffset = 0;
		if (control->tabgroup) {
			if (control->type != PANEL_TAB || control->tabsubgroup != 1) {
				control->tabgroupxoffset = tabgroupxoffset;
				control->tabgroupyoffset = tabgroupyoffset;
				control->tabgroupctlhandle = tabgroupctlhandle;
				iXPos = 0 - win->scrollx;
				if ((res->restype != RES_TYPE_DIALOG)) iYPos = win->bordertop - win->scrolly;
			}
			if (control->type != PANEL_TAB) {
				if (control->tabsubgroup != (control->selectedtab + 1)) {
					/* do not create controls within a tabfolder that is not selected */
					continue;
				}
			}
		}
		if (ISPUSHBUTTON(control) && control->escflag) res->ctlescape = iCurCtrl + 1;
		makecontrol(control, iXPos, iYPos, pscsControlStruct->hwnd, (res->restype == RES_TYPE_DIALOG));
		if (res->restype == RES_TYPE_PANEL) {
			/* makecontrol() modifies control->rect.bottom for certain controls, like   */
			/* TEXT and CHECKBOX, where height is unknown before the control has been   */
			/* created.  Therefore, this code must go after call to makecontrol() - SSN */
			rect = control->rect;
			width = rect.right - rect.left + 1;
			height = rect.bottom - rect.top + 1;
			if (rect.left + res->showoffset.x + width > win->xsize) {
				win->xsize = rect.left + res->showoffset.x + width;
			}
			if (rect.top + res->showoffset.y + height > win->ysize) {
				win->ysize = rect.top + res->showoffset.y + height;
			}
		}
		if (control->type == PANEL_TAB && control->tabsubgroup == 1) {
			tabgroupxoffset = control->tabgroupxoffset;
			tabgroupyoffset = control->tabgroupyoffset;
			if (res->restype == RES_TYPE_PANEL) {
				tabgroupxoffset += res->showoffset.x;
				tabgroupyoffset += res->showoffset.y;
			}
			tabgroupctlhandle = control->tabgroupctlhandle;
		}
	}
	if (RES_TYPE_PANEL == res->restype && win->autoscroll) {
		// Calling checkScrollBars instead of guiaCheckScrollBars because
		// we are always on the message loop thread in this function
		// and it is therefore not necessary to go through guiaWinProc again.
		checkScrollBars(res->winshow);
	}

	for (control = (CONTROL *) res->content, i1 = res->entrycount; i1; i1--, control++) {
		if (control->type == PANEL_TAB) {
			if (control->tabsubgroup == (control->selectedtab + 1)) {
				SendMessage(control->tabgroupctlhandle, TCM_SETCURSEL, control->selectedtab, 0);
			}
		}
	}
	/**
	 * SW_SHOWNA
	 * Displays the window in its current size and position.
	 * This value is similar to SW_SHOW, except the window is not activated.
	 *
	 * Commented out on 05/04/2012 for 16.1+
	 * Seems to be causing Win7 64-bit to behave incorrectly.
	 * And it also seems redundant altogether.
	 *
	 * Will put in a config switch to turn it back on if any reports come in from the
	 * field that it is needed.
	 *
	 * Update 05/15/2012
	 * Report of dialogs not working at all. So suppress only for panels
	 */
	if (res->restype != RES_TYPE_PANEL) ShowWindow(pscsControlStruct->hwnd, SW_SHOWNA);

	if (win->nofocus) {
		return 0; /* don't do any of the following focusing */
	}

	/* if this is first panel on window then set focus to first field,
	 * except if the first control is part of a button group, find the
   	 * one that is marked and set the focus on that one.
   	 * Tables may or may not be focusable.
   	 */
	if (res == *win->showreshdr || RES_TYPE_DIALOG == res->restype) {
		for (control = (CONTROL *) res->content, i1 = res->entrycount; i1; i1--, control++) {
			if (ISGRAY(control) || ISSHOWONLY(control) || !ISINTERACTIVE(control) || ISNOFOCUS(control)) {
				continue;
			}
			if (ISRADIO(control) && !(ISMARKED(control))) continue;
			if (control->type == PANEL_TABLE && !guiaIsTableFocusable(control)) continue;
			SetFocus(control->ctlhandle);
			break;
		}
	}
	/* This next call is needed for Windows 2000 to bring the window to the top */
	SetForegroundWindow(pscsControlStruct->hwnd);
	if (winbackflag) {
		BringWindowToTop(pscsControlStruct->hwnd);
		winbackflag = FALSE;
	}
	/* Windows 98 workaround for repaint issues */
//	if (osinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
//		if (pscsControlStruct->hwnd != NULL) {
//			UpdateWindow(pscsControlStruct->hwnd);
//		}
//		if (foregroundwin != NULL) UpdateWindow(foregroundwin);
//	}
	foregroundwin = pscsControlStruct->hwnd;
	return 0;
}

/*
 * This function is run on the Windows message thread
 * It is only called in response to a GUIAM_HIDECONTROLS message
 * sent to the parent window.
 */
static LONG hidecontrols(GUIAHIDECONTROLS *phcsHideCtrlStruct)
{
	RESOURCE *res;
	CONTROL *control;
	INT rc, iCurCtrl, textlen;
	LONG_PTR lrc;
	WINDOW *win;
	CHAR work[128];

	res = phcsHideCtrlStruct->res;
	checkwinsize(res->winshow);
	win = *res->winshow;
	control = (CONTROL *) res->content;
	for (iCurCtrl = 0; iCurCtrl < res->entrycount; iCurCtrl++, control++) {
		if (control->tabgroup) {
			/* store selected tab in case we show this control again */
			control->selectedtab = TabCtrl_GetCurSel(control->tabgroupctlhandle);
		}
	}
	control = (CONTROL *) res->content;
	for (iCurCtrl = 0; iCurCtrl < res->entrycount; iCurCtrl++, control++) {
		if (control->type == PANEL_TAB && control->tabsubgroup != (control->selectedtab + 1)) {
			control->ctlhandle = NULL;
			control->shownflag = FALSE;
			continue;
		}
		if (control->ctlhandle == NULL) continue;

		if (ISEDIT(control) && control->type != PANEL_FEDIT) {
			acquireEditBoxText(control, &textlen);
		}
		else if (ISLISTBOX(control)) {
			control->value4 = 1 + (INT)SendMessage(control->ctlhandle, LB_GETTOPINDEX, 0, 0L);
			if (control->type == PANEL_MLISTBOX || control->type == PANEL_MLISTBOXHS)
				saveMListboxSelectionState(control);
		}

		if (control->type == PANEL_TREE) {
			ImageList_Destroy(TreeView_GetImageList(control->ctlhandle, TVSIL_STATE));
		}
		else if (control->type == PANEL_TABLE) {
			hidetablecontrols(control);
		}

		if (control->escflag && iCurCtrl + 1 == res->ctlescape) {
			res->ctlescape = 0;
		}

		if (control->oldproc != NULL) {
			SetLastError(0);
			lrc = SetWindowLongPtr(control->ctlhandle, GWLP_WNDPROC, (LONG_PTR) control->oldproc);
			if (lrc == 0 && GetLastError() != 0) {
				// Ignore for now
			}
		}

		control->shownflag = FALSE;
		if (IsWindow(control->ctlhandle)) {
			rc = DestroyWindow(control->ctlhandle);
			/**
			 * When the owner window of an owned floatwindow is destroyed, all of the control windows
			 * in the float window become invalid. The call to DestroyWindow will fail in that case and
			 * we can ignore it.
			 */
			if (!rc && /* Other than an owned float window */ !(win->floatwindow && win->owner != NULL)) {
				sprintf_s(work, ARRAYSIZE(work), "DestroyWindow fail, e=%d, control->type=%hu",
						(INT)GetLastError(), control->type);
				setLastErrorMessage(work);
				control->ctlhandle = NULL;
				return RC_ERROR;
			}
		}
		control->ctlhandle = NULL;
	}
	return 0;
}

static void acquireEditBoxText (CONTROL *control, INT *textlen) {
	CHAR work[64];
	*textlen = GetWindowTextLength(control->ctlhandle) + 1;
	if (control->textval != NULL) {
		control->textval = guiReallocMem(control->textval, *textlen);
		if (control->textval == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "Error in %s, guiReallocMem failed", __FUNCTION__);
			guiaErrorMsg(work, *textlen);
			return;
		}
	}
	else {
		control->textval = guiAllocMem(*textlen);
		if (control->textval == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "Error in %s, guiAllocMem failed", __FUNCTION__);
			guiaErrorMsg(work, *textlen);
			return;
		}
	}
	*textlen = GetWindowText(control->ctlhandle, (CHAR *) control->textval, *textlen);
	control->textval[*textlen] = '\0';
}

/**
 * Find the rectangle that is the union of all controls and all images.
 * Set win->xsize and win->ysize to the size of this rectangle
 */
static void checkwinsize(WINHANDLE wh) {
	WINDOW *win;
	RESOURCE *rs1;
	RESHANDLE rh;
	PIXMAP *pm1;
	PIXHANDLE ph;
	CONTROL *control;
	INT iCurCtrl;
	RECT rect;
	POINT p2;

	pvistart();
	win = *wh;
	win->xsize = win->ysize = 0;
	for (rh = win->showreshdr; rh != NULL; rh = rs1->showresnext) {
		rs1 = *rh;
		if (RES_TYPE_PANEL != rs1->restype) continue;
		control = (CONTROL *) rs1->content;
		for (iCurCtrl = 0; iCurCtrl < rs1->entrycount; iCurCtrl++, control++) {
			GetWindowRect(control->ctlhandle, &rect);
			p2.x = rect.right;
			p2.y = rect.bottom - win->bordertop;
			ScreenToClient(win->hwnd, &p2);
			if (p2.x > win->xsize) win->xsize = p2.x;
			if (p2.y > win->ysize) win->ysize = p2.y;
		}
	}
	for (ph = win->showpixhdr; ph != NULL; ph = pm1->showpixnext) {
		pm1 = *ph;
		if (pm1->winshow) {
			if ((pm1->hshow + pm1->hsize) > win->xsize) win->xsize = (pm1->hshow + pm1->hsize);
			if ((pm1->vshow + pm1->vsize) > win->ysize) win->ysize = (pm1->vshow + pm1->vsize);
		}
	}
	pviend();
}

static HWND createwindowex(GUIAEXWINCREATESTRUCT *pecsExtWinCreate)
{
	return CreateWindowEx(pecsExtWinCreate->dwExStyle,
		pecsExtWinCreate->lpszClassName,
		pecsExtWinCreate->lpszWindowName,
		pecsExtWinCreate->dwStyle,
		pecsExtWinCreate->x, pecsExtWinCreate->y,
		pecsExtWinCreate->width, pecsExtWinCreate->height,
		pecsExtWinCreate->hwndParent, pecsExtWinCreate->hmenu,
		hinstancemain, pecsExtWinCreate->lpvParam);
}

static LONG winshowcaret(WINDOW *win)
{
	if (GetFocus() == win->hwnd) {
		CreateCaret(win->hwnd, NULL, 2, win->caretvsize);
		SetCaretPos(win->caretpos.x, win->bordertop + win->caretpos.y);
		ShowCaret(win->hwnd);
		win->caretcreatedflg = TRUE;
	}
	return 0;
}

static LONG winhidecaret(WINDOW *win)
{
	if (win->caretcreatedflg) {
		DestroyCaret();
		win->caretcreatedflg = FALSE;
	}
	return 0;
}

static LONG winmovecaret(WINDOW *win)
{
	if (win->caretcreatedflg) SetCaretPos(win->caretpos.x, win->bordertop + win->caretpos.y);
	return 0;
}

/**
 * Will return 0 or RC_ERROR
 */
static INT speedKeyText(MENUENTRY *pMenuItem, CHAR *MenuBuffer, INT menutxtlen) {

	static CHAR letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	if (pMenuItem->speedkey >= KEYF1 && pMenuItem->speedkey <= KEYF12) {
		sprintf(&MenuBuffer[menutxtlen + 1], "F%i", pMenuItem->speedkey - KEYF1 + 1);
	}
	else if (pMenuItem->speedkey >= KEYSHIFTF1 && pMenuItem->speedkey <= KEYSHIFTF12) {
		sprintf(&MenuBuffer[menutxtlen + 1], "Shift+F%i", pMenuItem->speedkey - KEYSHIFTF1 + 1);
	}
	else if (pMenuItem->speedkey >= KEYALTF1 && pMenuItem->speedkey <= KEYALTF12) {
		sprintf(&MenuBuffer[menutxtlen + 1], "Alt+F%i", pMenuItem->speedkey - KEYALTF1 + 1);
	}
	else if (pMenuItem->speedkey >= KEYCTLSHFTF1 && pMenuItem->speedkey <= KEYCTLSHFTF12) {
		sprintf(&MenuBuffer[menutxtlen + 1], "Ctrl+Shift+F%i", pMenuItem->speedkey - KEYCTLSHFTF1 + 1);
	}
	/* The parser does not seem to support CTRL+F keys, but I put it in here for completeness */
	else if (pMenuItem->speedkey >= KEYCTLF1 && pMenuItem->speedkey <= KEYCTLF12) {
		sprintf(&MenuBuffer[menutxtlen + 1], "Ctrl+F%i", pMenuItem->speedkey - KEYCTLF1 + 1);
	}
	else if (pMenuItem->speedkey >= KEYCTRLA && pMenuItem->speedkey <= KEYCTRLZ) {
		strcpy(&MenuBuffer[menutxtlen + 1], "Ctrl+");
		MenuBuffer[menutxtlen + 6] = letters[pMenuItem->speedkey - KEYCTRLA];
		MenuBuffer[menutxtlen + 7] = '\0';
	}
	else if (pMenuItem->speedkey >= KEYCTLSHFTA && pMenuItem->speedkey <= KEYCTLSHFTZ) {
		strcpy(&MenuBuffer[menutxtlen + 1], "Ctrl+Shift+");
		MenuBuffer[menutxtlen + 12] = letters[pMenuItem->speedkey - KEYCTLSHFTA];
		MenuBuffer[menutxtlen + 13] = '\0';
	}
	else if (pMenuItem->speedkey >= KEYCOMMA && pMenuItem->speedkey <= KEYQUOTE) {
		strcpy(&MenuBuffer[menutxtlen + 1], "Ctrl+");
		switch (pMenuItem->speedkey) {
			case KEYCOMMA:		MenuBuffer[menutxtlen + 6] = ','; break;
			case KEYPERIOD:		MenuBuffer[menutxtlen + 6] = '.'; break;
			case KEYFSLASH:		MenuBuffer[menutxtlen + 6] = '/'; break;
			case KEYSEMICOLON:	MenuBuffer[menutxtlen + 6] = ';'; break;
			case KEYLBRACKET:	MenuBuffer[menutxtlen + 6] = '['; break;
			case KEYRBRACKET:	MenuBuffer[menutxtlen + 6] = ']'; break;
			case KEYBSLASH:		MenuBuffer[menutxtlen + 6] = '\\';  break;
			case KEYMINUS:		MenuBuffer[menutxtlen + 6] = '-'; break;
			case KEYEQUAL:		MenuBuffer[menutxtlen + 6] = '='; break;
			case KEYQUOTE:		MenuBuffer[menutxtlen + 6] = '\'';  break;
		}
		MenuBuffer[menutxtlen + 7] = '\0';
	}
	else if (pMenuItem->speedkey == KEYINSERT) {
		strcpy(&MenuBuffer[menutxtlen + 1], "Ins");
	}
	else if (pMenuItem->speedkey == KEYDELETE) {
		strcpy(&MenuBuffer[menutxtlen + 1], "Del");
	}
	else if (pMenuItem->speedkey == KEYHOME) {
		strcpy(&MenuBuffer[menutxtlen + 1], "Home");
	}
	else if (pMenuItem->speedkey == KEYEND) {
		strcpy(&MenuBuffer[menutxtlen + 1], "End");
	}
	else if (pMenuItem->speedkey == KEYPGUP) {
		strcpy(&MenuBuffer[menutxtlen + 1], "Pgup");
	}
	else if (pMenuItem->speedkey == KEYPGDN) {
		strcpy(&MenuBuffer[menutxtlen + 1], "Pgdn");
	}
	else return RC_ERROR;
	return 0;
}

/**
 * Will return 0 or RC_ERROR
 */
static LONG menucreate(RESHANDLE reshandle)
{
	WINDOW *win;
	MENUENTRY *pMenuItem;
	CHAR *MenuText, *MenuBuffer;
	RECT rect;
	INT curentry, numspeedkey, menutxtlen, maxmenutxt, curlvl, nextlvl;
	INT hsize, vsize, curspeedkey, i1;
	USHORT fAutoScroll;
	ACCEL * aaclAccelTable = NULL;
	MENUITEMINFO mii;
	CHAR work[64];
	ZHANDLE zhMB = NULL, zhAccel = NULL;
	RESOURCE *res;
	INT notMaximized;

	res = *reshandle;
	win = *res->winshow;
	pMenuItem = (MENUENTRY *) res->content;

	/*
	 * Do not attempt to change the window size if it is maximized
	 */
	notMaximized = !(GetWindowLongPtr(win->hwnd, GWL_STYLE) & WS_MAXIMIZE);

	/* count each accelerator key, if any */
	numspeedkey = 0;
	maxmenutxt = 0;
	for (curentry = 0; curentry < res->entrycount; curentry++) {
		if (pMenuItem[curentry].speedkey) {
			numspeedkey++;
			menutxtlen = (INT)strlen((CHAR *) pMenuItem[curentry].text);
			if (maxmenutxt < menutxtlen) maxmenutxt = menutxtlen;
		}
	}
	res->speedkeys = numspeedkey;
	if (numspeedkey) {
		zhAccel = guiAllocMem(sizeof(ACCEL) * numspeedkey );
		maxmenutxt += 15 * sizeof(CHAR); /* 15 is added here for accelerator key text like CTRL-A .. CTRL-Z, F1 .. F10, PgUp, Insert, etc. */
		zhMB = guiAllocMem(maxmenutxt);
		if (zhAccel == NULL || zhMB == NULL) {
			sprintf(work, "guiAllocMem returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, 0);
			goto menucreateerror;
		}
		MenuBuffer = (CHAR *) guiLockMem(zhMB);
		aaclAccelTable = (ACCEL *) guiLockMem(zhAccel);
	}

	/* loop through each menu item, adding speed keys to accelerator table, adding menu items */
	res->hmenu = hmenucurrentmenu[0] = CreateMenu();
	if (res->hmenu == NULL) {
		sprintf(work, "Error creating menu in %s", __FUNCTION__);
		guiaErrorMsg(work, GetLastError());
		goto menucreateerror;
	}
	memset(levelitemcnt, 0, sizeof(levelitemcnt));
	curspeedkey = 0;
	mii.cbSize = sizeof(MENUITEMINFO);
	for (curentry = 0; curentry < res->entrycount; curentry++, pMenuItem++) {
		if (curentry + 1 >= res->entrycount) nextlvl = 0;
		else nextlvl = (pMenuItem + 1)->level;
		curlvl = pMenuItem->level;
		pMenuItem->count = levelitemcnt[curlvl]++;
		pMenuItem->menuhandle = hmenucurrentmenu[curlvl];
		if (nextlvl <= curlvl && pMenuItem->style & MENUSTYLE_HIDDEN) continue;
		if (pMenuItem->speedkey) {
			/* add speed keys to accelerator table and to menu item text */
			menutxtlen = (INT)strlen((CHAR *) pMenuItem->text);
			strcpy(MenuBuffer, pMenuItem->text);
			MenuBuffer[menutxtlen] = '\t';
			i1 = speedKeyText(pMenuItem, MenuBuffer, menutxtlen);
			if (i1) {
				guiUnlockMem(zhMB);
				guiFreeMem(zhMB);
				guiUnlockMem(zhAccel);
				guiFreeMem(zhAccel);
				return i1;
			}
			if (pMenuItem->speedkey >= KEYF1 && pMenuItem->speedkey <= KEYF12) {
				aaclAccelTable[curspeedkey].fVirt = FVIRTKEY;
				aaclAccelTable[curspeedkey].key = pMenuItem->speedkey - KEYF1 + VK_F1;
			}
			else if (pMenuItem->speedkey >= KEYSHIFTF1 && pMenuItem->speedkey <= KEYSHIFTF12) {
				aaclAccelTable[curspeedkey].fVirt = FSHIFT | FVIRTKEY;
				aaclAccelTable[curspeedkey].key = pMenuItem->speedkey - KEYSHIFTF1 + VK_F1;
			}
			else if (pMenuItem->speedkey >= KEYALTF1 && pMenuItem->speedkey <= KEYALTF12) {
				aaclAccelTable[curspeedkey].fVirt = FALT | FVIRTKEY;
				aaclAccelTable[curspeedkey].key = pMenuItem->speedkey - KEYALTF1 + VK_F1;
			}
			else if (pMenuItem->speedkey >= KEYCTLF1 && pMenuItem->speedkey <= KEYCTLF12) {
				aaclAccelTable[curspeedkey].fVirt = FCONTROL | FVIRTKEY;
				aaclAccelTable[curspeedkey].key = pMenuItem->speedkey - KEYCTLF1 + VK_F1;
			}
			else if (pMenuItem->speedkey >= KEYCTLSHFTF1 && pMenuItem->speedkey <= KEYCTLSHFTF12) {
				aaclAccelTable[curspeedkey].fVirt = FCONTROL | FSHIFT | FVIRTKEY;
				aaclAccelTable[curspeedkey].key = pMenuItem->speedkey - KEYCTLSHFTF1 + VK_F1;
			}
			else if (pMenuItem->speedkey >= KEYCTRLA && pMenuItem->speedkey <= KEYCTRLZ) {
				aaclAccelTable[curspeedkey].fVirt = FCONTROL | FVIRTKEY;
				aaclAccelTable[curspeedkey].key = pMenuItem->speedkey - KEYCTRLA + 'A';
			}
			else if (pMenuItem->speedkey >= KEYCTLSHFTA && pMenuItem->speedkey <= KEYCTLSHFTZ) {
				aaclAccelTable[curspeedkey].fVirt = FCONTROL | FSHIFT | FVIRTKEY;
				aaclAccelTable[curspeedkey].key = pMenuItem->speedkey - KEYCTLSHFTA + 'A';
			}
			else if (pMenuItem->speedkey >= KEYCOMMA && pMenuItem->speedkey <= KEYQUOTE) {
				aaclAccelTable[curspeedkey].fVirt = FCONTROL | FVIRTKEY;
				switch (pMenuItem->speedkey) {
					case KEYCOMMA:		aaclAccelTable[curspeedkey].key = VK_OEM_COMMA; break;
					case KEYPERIOD:		aaclAccelTable[curspeedkey].key = VK_OEM_PERIOD; break;
					case KEYFSLASH:		aaclAccelTable[curspeedkey].key = VK_OEM_2; break;
					case KEYSEMICOLON:	aaclAccelTable[curspeedkey].key = VK_OEM_1; break;
					case KEYLBRACKET:	aaclAccelTable[curspeedkey].key = VK_OEM_4; break;
					case KEYRBRACKET:	aaclAccelTable[curspeedkey].key = VK_OEM_6; break;
					case KEYBSLASH:		aaclAccelTable[curspeedkey].key = VK_OEM_5; break;
					case KEYMINUS:		aaclAccelTable[curspeedkey].key = VK_OEM_MINUS; break;
					case KEYEQUAL:		aaclAccelTable[curspeedkey].key = VK_OEM_PLUS; break;
					case KEYQUOTE:		aaclAccelTable[curspeedkey].key = VK_OEM_7; break;
				}
			}
			else if (pMenuItem->speedkey == KEYINSERT) {
				aaclAccelTable[curspeedkey].fVirt = FVIRTKEY;
				aaclAccelTable[curspeedkey].key = VK_INSERT;
			}
			else if (pMenuItem->speedkey == KEYDELETE) {
				aaclAccelTable[curspeedkey].fVirt = FVIRTKEY;
				aaclAccelTable[curspeedkey].key = VK_DELETE;
			}
			else if (pMenuItem->speedkey == KEYHOME) {
				aaclAccelTable[curspeedkey].fVirt = FVIRTKEY;
				aaclAccelTable[curspeedkey].key = VK_HOME;
			}
			else if (pMenuItem->speedkey == KEYEND) {
				aaclAccelTable[curspeedkey].fVirt = FVIRTKEY;
				aaclAccelTable[curspeedkey].key = VK_END;
			}
			else if (pMenuItem->speedkey == KEYPGUP) {
				aaclAccelTable[curspeedkey].fVirt = FVIRTKEY;
				aaclAccelTable[curspeedkey].key = VK_PRIOR;
			}
			else if (pMenuItem->speedkey == KEYPGDN) {
				aaclAccelTable[curspeedkey].fVirt = FVIRTKEY;
				aaclAccelTable[curspeedkey].key = VK_NEXT;
			}
			else goto menucreateerror;
			aaclAccelTable[curspeedkey++].cmd = (USHORT) pMenuItem->useritem;
			MenuText = MenuBuffer;
		}
		else MenuText = (CHAR *) pMenuItem->text;

		/* add menu item to current menu/pop-up menu */
		mii.fMask = MIIM_TYPE;
		mii.hSubMenu = (HMENU)NULL;
		mii.hbmpItem = (HBITMAP)NULL;
		if (pMenuItem->style & MENUSTYLE_LINE) mii.fType = MFT_SEPARATOR;
		else {
			mii.fType = MFT_STRING;
			mii.fState = 0;
			mii.fMask |= MIIM_STATE | MIIM_ID;
			mii.dwTypeData = MenuText;
			mii.wID = pMenuItem->useritem;
			if (pMenuItem->style & MENUSTYLE_DISABLED) mii.fState |= MFS_GRAYED;
			if (nextlvl > curlvl) {	/* create new pop-up menu for subsequent items */
				hmenucurrentmenu[nextlvl] = CreateMenu();
				if (hmenucurrentmenu[nextlvl] == NULL) {
					sprintf(work, "Error creating menu in %s", __FUNCTION__);
					guiaErrorMsg(work, GetLastError());
					goto menucreateerror;
				}
				mii.fMask |= MIIM_SUBMENU;
				mii.hSubMenu = hmenucurrentmenu[nextlvl];
				levelitemcnt[nextlvl] = 0;
				pMenuItem->style |= MENUSTYLE_SUBMENU;
			}
			else if (pMenuItem->style & MENUSTYLE_MARKED) mii.fState |= MFS_CHECKED;
		}

		checkForIconItem(pMenuItem, &mii, hmenucurrentmenu[curlvl]);

		/*
		 * Store a pointer to our MENUENTRY structure in the widget.
		 * This makes processing the WM_DRAWITEM and WM_MEASUREITEM messages much easier.
		 * This is safe because res->content is malloc memory, not memalloc.
		 */
		mii.fMask |= MIIM_DATA;
		mii.dwItemData = (ULONG_PTR) pMenuItem;

		i1 = GetMenuItemCount(hmenucurrentmenu[curlvl]);
		if (i1 == -1) {
			sprintf(work, "Error calling GetMenuItemCount in %s, curlvl=%i", __FUNCTION__, curlvl);
			guiaErrorMsg(work, GetLastError());
			goto menucreateerror;
		}
		if (!InsertMenuItem(hmenucurrentmenu[curlvl], i1, TRUE, &mii)) {
			i1 = GetLastError();
			sprintf(work, "Error appending item to menu in %s", __FUNCTION__);
			guiaErrorMsg(work, i1);
			goto menucreateerror;
		}
	}

	if (numspeedkey) {
		guiUnlockMem(zhMB);
		guiFreeMem(zhMB);
		res->hacAccelHandle = CreateAcceleratorTable(aaclAccelTable, numspeedkey);
		guiUnlockMem(zhAccel);
		guiFreeMem(zhAccel);
		if (res->hacAccelHandle == NULL) {
			sprintf(work, "Error creating accelerator table in %s", __FUNCTION__);
			guiaErrorMsg(work, GetLastError());
			return RC_ERROR;
		}
		if (win->hwnd == GetActiveWindow()) haccelfocus = res->hacAccelHandle;
	}
	else res->hacAccelHandle = NULL;

	if (notMaximized) {
		GetWindowRect(win->hwnd, &rect);
		hsize = rect.right - rect.left;
		vsize = rect.bottom - rect.top;
		GetClientRect(win->hwnd, &rect);
		vsize += rect.bottom;
	}

	fAutoScroll = win->autoscroll;
	win->autoscroll = FALSE;
	SetMenu(win->hwnd, res->hmenu);
	win->autoscroll = fAutoScroll;

	if (notMaximized) {
		GetClientRect(win->hwnd, &rect);
		vsize -= rect.bottom;
		SetWindowPos(win->hwnd, NULL, 0, 0, hsize, vsize, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	}

	if (win->autoscroll) guiaCheckScrollBars(res->winshow);
	return 0;

menucreateerror:
	if (zhMB != NULL) {
		guiUnlockMem(zhMB);
		guiFreeMem(zhMB);
	}
	if (zhAccel != NULL) {
		guiUnlockMem(zhAccel);
		guiFreeMem(zhAccel);
	}
	return RC_ERROR;
}

/*
 * We cannot call SetMenuInfo directly as doing so will prevent DX from loading on
 * 95 and NT.
 *
 * HBMMENU_CALLBACK = A bitmap that is drawn by the window that owns the menu.
 * The application must process the WM_MEASUREITEM and WM_DRAWITEM messages.
 *
 */
static void checkForIconItem(PMENUENTRY pMenuEntry, LPMENUITEMINFO mii, HMENU hmenu) {
	MENUINFO mi;
	INT i1;
	CHAR work[64];
	if (pMenuEntry->iconres != NULL) {
		mii->fType &= ~(MFT_STRING);
		mii->fMask &= ~(MIIM_TYPE);
		mii->fMask |= MIIM_FTYPE | MIIM_BITMAP | MIIM_STRING;
		mii->hbmpItem = HBMMENU_CALLBACK;
		mi.cbSize = sizeof(MENUINFO);
		mi.fMask = MIM_STYLE;
		mi.dwStyle = MNS_CHECKORBMP;
		if (!SetMenuInfo(hmenu, &mi)) {
			i1 = GetLastError();
			sprintf(work, "Error calling SetMenuInfo in %s", __FUNCTION__);
			guiaErrorMsg(work, i1);
		}
	}
}

/**
 * Always called on the Windows message thread
 */
static LONG menudestroy(RESOURCE *res)
{
	WINDOW *win;
	RECT rect;
	MENUENTRY *pMenuItem;
	INT hsize, vsize, curentry, iNextTopLevel;
	INT notMaximized;

	win = *res->winshow;

	/*
	 * Do not attempt to change the window size if it is maximized
	 */
	notMaximized = !(GetWindowLongPtr(win->hwnd, GWL_STYLE) & WS_MAXIMIZE);

	/*
	 * delete accelerator table if one was used
	 */
	if (res->hacAccelHandle) {
		if (res->hacAccelHandle == haccelfocus) haccelfocus = NULL;
		DestroyAcceleratorTable(res->hacAccelHandle);
		res->hacAccelHandle = NULL;
	}

	if (notMaximized) {
		GetWindowRect(win->hwnd, &rect); 	/* Returns screen coordinates */
		hsize = rect.right - rect.left;		/* Overall width */
		vsize = rect.bottom - rect.top;		/* Overall height */
		GetClientRect(win->hwnd, &rect);	/* Relative to window, UL corner will always be 0,0 */
		vsize += rect.bottom;
	}

	/**
	 * The below thing in the loop about ERROR_INVALID_MENU_HANDLE was added
	 * on 07 Feb 2014
	 *
	 *  Peter J�schke at automotivesystems.de was getting this on *some* machines
	 *  *sometimes*.
	 */
	pMenuItem = (PMENUENTRY) res->content;
	iNextTopLevel = 0;
	for (curentry = 0; curentry < res->entrycount; curentry++, pMenuItem++) {
		if (!pMenuItem->level && !(pMenuItem->style & MENUSTYLE_HIDDEN)) {
			if (!DeleteMenu(pMenuItem->menuhandle, 0, MF_BYPOSITION)) {
				DWORD dw1 = GetLastError();
				if (dw1 == ERROR_INVALID_MENU_HANDLE) continue;
				guiaErrorMsg("DeleteMenu() failure in guiaHideMenu", dw1);
			}
		}
	}
	DrawMenuBar(win->hwnd);
	SetMenu(win->hwnd, NULL);

	if (notMaximized) {
		GetClientRect(win->hwnd, &rect);	/* This will be larger now that the menu has gone away */
		vsize -= rect.bottom;	/* now smaller than before by exactly the height of the vanished menu */
		SetWindowPos(win->hwnd, NULL, 0, 0, hsize, vsize, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	}

	if (win->autoscroll) guiaCheckScrollBars(res->winshow);
	res->winshow = NULL;
	return 0;
}

/**
 * Will return 0 or RC_ERROR
 */
static LONG popupmenushow(RESHANDLE rh)
{
	RESOURCE *res;
	WINDOW *win;
	INT curentry, curlvl, nextlvl;
	MENUENTRY *pMenuItem;
	RECT rect;
	POINT pt;
	MENUITEMINFO mii;
	CHAR work[80];

	pvistart();
	res = *rh;
	win = *res->winshow;
	pt.x = popupmenuh;
	pt.y = popupmenuv;
	ClientToScreen(win->hwnd, &pt);
	pt.x = pt.x - win->scrollx;
	pt.y = pt.y - (win->scrolly - win->bordertop);
	pMenuItem = (MENUENTRY *) res->content;
	res->hmenu = hmenucurrentmenu[0] = CreatePopupMenu();
	if (res->hmenu == NULL) {
		pviend();
		sprintf(work, "Error creating menu in %s", __FUNCTION__);
		guiaErrorMsg(work, GetLastError());
		return RC_ERROR;
	}
	memset(levelitemcnt, 0, sizeof(levelitemcnt));
	mii.cbSize = sizeof(MENUITEMINFO);
	for (curentry = 0; curentry < res->entrycount; curentry++, pMenuItem++) {
		if (curentry + 1 >= res->entrycount) nextlvl = 0;
		else nextlvl = (pMenuItem + 1)->level;
		curlvl = pMenuItem->level;
		pMenuItem->count = levelitemcnt[curlvl]++;
		pMenuItem->menuhandle = hmenucurrentmenu[curlvl];
		if (nextlvl <= curlvl && pMenuItem->style & MENUSTYLE_HIDDEN) continue;

		/* add menu item to current pop-up menu */
		mii.fMask = MIIM_TYPE;
		if (pMenuItem->style & MENUSTYLE_LINE) mii.fType = MFT_SEPARATOR;
		else {
			mii.fType = MFT_STRING;
			mii.fMask |= MIIM_STATE | MIIM_ID;
			mii.fState = 0;
			mii.dwTypeData = (LPSTR) pMenuItem->text;
			mii.wID = pMenuItem->useritem;
			if (pMenuItem->style & MENUSTYLE_DISABLED) mii.fState |= MFS_GRAYED;
		}

		if (nextlvl > curlvl) {	/* create new pop-up menu for subsequent items */
			levelitemcnt[nextlvl] = 0;
			hmenucurrentmenu[nextlvl] = CreatePopupMenu();
			if (hmenucurrentmenu[nextlvl] == NULL) {
				pviend();
				sprintf(work, "Error creating menu in %s", __FUNCTION__);
				guiaErrorMsg(work, GetLastError());
				return RC_ERROR;
			}
			mii.fMask |= MIIM_SUBMENU;
			mii.hSubMenu = hmenucurrentmenu[nextlvl];
			pMenuItem->style |= MENUSTYLE_SUBMENU;
		}
		else if (pMenuItem->style & MENUSTYLE_MARKED) mii.fState |= MFS_CHECKED;

		checkForIconItem(pMenuItem, &mii, hmenucurrentmenu[curlvl]);

		/*
		 * Store a pointer to our MENUENTRY structure in the widget.
		 * This makes processing the WM_DRAWITEM and WM_MEASUREITEM messages much easier.
		 */
		mii.fMask |= MIIM_DATA;
		mii.dwItemData = (ULONG_PTR) pMenuItem;

		if (!InsertMenuItem(hmenucurrentmenu[curlvl], GetMenuItemCount(hmenucurrentmenu[curlvl]),
				TRUE, &mii)) {
			pviend();
			sprintf(work, "Error appending item to menu in %s", __FUNCTION__);
			guiaErrorMsg(work, GetLastError());
			return RC_ERROR;
		}
	}
	rect.left = rect.top = 0;
	rect.right = rect.bottom = 2048;
	win->showpopupmenu = rh;
	TrackPopupMenu(res->hmenu, TPM_LEFTALIGN | TPM_LEFTBUTTON, pt.x, pt.y, 0, win->hwnd, &rect);
	pviend();
	return 0;
}

static LONG getwindowtext(GUIAGETWINTEXT *pgwtGetWinTextStruct)
{
	pgwtGetWinTextStruct->textlen = GetWindowText(pgwtGetWinTextStruct->hwnd,
		pgwtGetWinTextStruct->title, pgwtGetWinTextStruct->textlen);
	return 0;
}

static LONG getselectedtreeitemtext(GUIAGETWINTEXT *pgwtGetWinTextStruct) {
	TVITEM tvitem;
	HTREEITEM selectedItem = TreeView_GetSelection(pgwtGetWinTextStruct->hwnd);
	if (selectedItem != NULL) {
		tvitem.hItem = selectedItem;
		tvitem.mask = TVIF_TEXT;
		tvitem.pszText = pgwtGetWinTextStruct->title;
		tvitem.cchTextMax = pgwtGetWinTextStruct->textlen;
		TreeView_GetItem(pgwtGetWinTextStruct->hwnd, &tvitem);
	}
	else pgwtGetWinTextStruct->title[0] = '\0';
	return 0;
}

/**
 * This runs only on the message loop thread
 *
 * It is called only via SendMessage from the DX main thread. So the DX main thread is blocked while this runs.
 * Perhaps we do not need to call the pvi functions in here?
 * ALWAYS returns zero.
 */
static LONG startdraw(PIXHANDLE ph)
{
	WINDOW *win;
	HDC hdc;
	PIXMAP *pix;

	pix = *ph;
	drawpixhandle = pix->thispixhandle;
	if (pix->winshow != NULL) {
		drawwinhandle = (*pix->winshow)->thiswinhandle;
		win = *pix->winshow;
	}
	else {
		drawwinhandle = NULL;
		win = NULL;
	}

	hdc = GetDC(hwndmainwin);
	pix->hdc = CreateCompatibleDC(hdc);
	ReleaseDC(hwndmainwin, hdc);
	oldhbitmap = SelectObject(pix->hdc, pix->hbitmap);
	if (pix->hpal != NULL) hpaloldpal = SelectPalette(pix->hdc, pix->hpal, TRUE);
	oldhfont = SelectObject(pix->hdc, pix->hfont);
//	if (win != NULL) {
//		win->hdc = GetDC(win->hwnd);
//		if (pix->hpal != NULL) {
//			if (win->hwnd == GetActiveWindow()) hpaldrawoldpal = SelectPalette(win->hdc, pix->hpal, FALSE);
//			else hpaldrawoldpal = SelectPalette(win->hdc, pix->hpal, TRUE);
//			RealizePalette(win->hdc);
//		}
//	}
//	else hpaldrawoldpal = NULL;
	return 0;
}

/*
 * Returns TRUE if successful, or FALSE otherwise.
 */
static INT guiaAddTooltip(CONTROL *control) {
	INT i1;
	RECT rect;
	TOOLINFO toolinfo;
	CHAR buffer[80];
	HWND window;

	if (control->style & (CTRLSTYLE_DISABLED | CTRLSTYLE_SHOWONLY | CTRLSTYLE_READONLY)) {
		/* do not set tooltip yet if disabled */
		return TRUE;
	}
	window = (*(*control->res)->winshow)->hwndtt;
	/* initialize members of the toolinfo structure */
	ZeroMemory(&toolinfo, sizeof(TOOLINFO));
	toolinfo.cbSize = sizeof(TOOLINFO);
	toolinfo.lpszText = (LPSTR) buffer;
	i1 = (INT)SendMessage(window, TTM_GETTOOLCOUNT, 0, (LPARAM) 0);
	for (i1 -= 1; i1 >= 0; i1--) {
		if (SendMessage(window, TTM_ENUMTOOLS, (UINT) i1, (LPARAM) (LPTOOLINFO) &toolinfo)) {
			if (toolinfo.uId == control->useritem && toolinfo.hwnd == control->ctlhandle)  {
				// We already have a tooltip on this widget.
				// Change it if necessary
				//guiaDelTooltip(control);
				if (strcmp((LPSTR)control->tooltip, buffer) != 0)
				{
					toolinfo.lpszText = (LPSTR) control->tooltip;
					SendMessage(window, TTM_UPDATETIPTEXT , 0, (LPARAM) (LPTOOLINFO) &toolinfo);
				}
				return TRUE;
			}
		}
	}

	/*
	 * We were using TTF_SUBCLASS but that was causing some (rare) problems
	 * with our own subclassing.
	 * So we now relay mouse events in guiaControlProc to the TT window using TTM_RELAYEVENT
	 */
	GetClientRect(control->ctlhandle, &rect); /* Get coordinates of control's client area */
	toolinfo.hwnd = control->ctlhandle;
	toolinfo.lpszText = (LPSTR) control->tooltip;
	toolinfo.uFlags = (UINT) 0;
	toolinfo.uId =  control->useritem;
	toolinfo.rect = rect;
	i1 = (INT)SendMessage(window, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &toolinfo);
	return i1;
}

/**
 * Returns 0 or RC_ERROR
 */
static LONG toolbarcreate(RESOURCE *res)
{
	WINDOW *win;
	RECT rect;
	INT hsize, vsize, iWinWidth, iWinHeight, iCurCtrl;
	RESOURCE *resicon;
	CONTROL *control;
	GUIATOOLBARDROPBOX tbdropbox;
	HWND hwndCtrl;
	INT notMaximized;

	win = *res->winshow;

	/*
	 * Do not attempt to change the window size if it is maximized
	 */
	notMaximized = !(GetWindowLongPtr(win->hwnd, GWL_STYLE) & WS_MAXIMIZE);

//	if (!initcomctlflag) {
//		InitCommonControls();
//		initcomctlflag = TRUE;
//	}

	if (notMaximized) {
		GetWindowRect(win->hwnd, &rect);
		iWinWidth = rect.right - rect.left;
		iWinHeight = rect.bottom - rect.top;
	}

	res->hwndtoolbar = CreateWindowEx(0, TOOLBARCLASSNAME, "", WS_CHILD | TBSTYLE_TOOLTIPS,
		 0, 0, 0, 0, win->hwnd, (HMENU) ITEM_MSG_TOOLBAR, hinstancemain, NULL
	);
	if (res->hwndtoolbar == NULL) {
		guiaErrorMsg("Create toolbar error code", GetLastError());
		return RC_ERROR;
	}
	hsize = 16;
	vsize = 16;
	control = (CONTROL *) res->content;
	for (iCurCtrl = 0; iCurCtrl < res->entrycount; iCurCtrl++, control++) {
		/* get maximum dimensions of PUSHBUTTON or LOCKBUTTON */
		if (TOOLBAR_SPACE == control->type) continue;
		else if (control->iconres != NULL) {
			resicon = *control->iconres;
			if (resicon->hsize > hsize) hsize = resicon->hsize;
			if (resicon->vsize > vsize) vsize = resicon->vsize;
		}
	}
	SendMessage(res->hwndtoolbar, TB_SETBITMAPSIZE, 0, (LPARAM) MAKELONG(hsize, vsize));
	hsize += 7; /* add horizontal padding for button size */
	vsize += 6; /* add vertical padding for button size */
	SendMessage(res->hwndtoolbar, TB_SETBUTTONSIZE, 0, (LPARAM) MAKELONG(hsize, vsize));
	SendMessage(res->hwndtoolbar, TB_AUTOSIZE, 0, 0);
	vsize += 8; /* add vertical spacing */
	win->bordertop = vsize;
	rect.top = 0;
	rect.left = 0;
	rect.right = win->clientx;
	rect.bottom = win->clienty + win->borderbtm;
	ScrollWindow(win->hwnd, 0, vsize, NULL, &rect);

	if (notMaximized)
		SetWindowPos(win->hwnd, NULL, 0, 0, iWinWidth, iWinHeight + vsize, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

	SetWindowPos(res->hwndtoolbar, NULL, 0, 0, 0, vsize, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
	SendMessage(res->hwndtoolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
	control = (CONTROL *) res->content;
	tbbutton.dwData = 0;
	tbbutton.iString = 0;
	toolinfo.cbSize = sizeof(TOOLINFO);
	toolinfo.uFlags = TTF_CENTERTIP;
	toolinfo.hwnd = win->hwnd;
	toolinfo.hinst = NULL;
	toolinfo.lpszText = LPSTR_TEXTCALLBACK;
/* get handle of tooltip control */
	hwndCtrl = (HWND) SendMessage(res->hwndtoolbar, TB_GETTOOLTIPS, 0, 0);
	for (iCurCtrl = 0; iCurCtrl < res->entrycount; iCurCtrl++, control++) {
		control->ctloffset = iCurCtrl;
		control->ctlhandle = NULL;
		control->changeflag = FALSE;
		if (TOOLBAR_SPACE == control->type) {
			tbbutton.fsState = 0;
			tbbutton.fsStyle = TBSTYLE_SEP;
			tbbutton.iBitmap = 8;	/* width of seperator (undocumented feature) */
  				if (!SendMessage(res->hwndtoolbar, TB_ADDBUTTONS, 1, (LPARAM) &tbbutton)) { // @suppress("Symbol is not resolved")
				guiaErrorMsg("Unable to add separator to toolbar", GetLastError());
				return RC_ERROR;
			}
		}
		else if (TOOLBAR_DROPBOX == control->type) {
			/* place separator in toolbar */
			tbbutton.fsState = 0;
			tbbutton.idCommand = control->useritem + ITEM_MSG_START;
			tbbutton.fsStyle = TBSTYLE_SEP;
 			if (!SendMessage(res->hwndtoolbar, TB_ADDBUTTONS, 1, (LPARAM) &tbbutton)) { // @suppress("Symbol is not resolved")
				guiaErrorMsg("Unable to add dropbox to toolbar", GetLastError());
				return RC_ERROR;
			}
			tbdropbox.res = res;
			tbdropbox.control = control;
			tbdropbox.idCommand = tbbutton.idCommand;
			if (toolbarcreatedbox(&tbdropbox) != 0) return RC_ERROR;
		}
		else if (control->iconres != NULL) {
			/* TOOLBAR_PUSHBUTTON or TOOLBAR_LOCKBUTTON */
			resicon = *control->iconres;
			tbbmaddbitmap.hInst = NULL;
			tbbmaddbitmap.nID = (UINT_PTR) resicon->hbmicon;
			tbbutton.iBitmap = LOWORD(SendMessage(res->hwndtoolbar, TB_ADDBITMAP, 1, (LPARAM) &tbbmaddbitmap));
			if (tbbutton.iBitmap < 0) {
				guiaErrorMsg("Unable to add bitmap to tool bar button", GetLastError());
				return RC_ERROR;
			}
			tbbutton.fsStyle = TBSTYLE_BUTTON;
			if (control->textval != NULL) {
				tbbutton.fsStyle |= TBSTYLE_TOOLTIPS;
			}
			if (TOOLBAR_LOCKBUTTON == control->type) tbbutton.fsStyle |= TBSTYLE_CHECK;
			toolinfo.uId = tbbutton.idCommand = control->useritem + ITEM_MSG_START;
			tbbutton.fsState = TBSTATE_ENABLED;
			if (!SendMessage(res->hwndtoolbar, TB_ADDBUTTONS, 1, (LPARAM) &tbbutton)) { // @suppress("Symbol is not resolved")
				guiaErrorMsg("Unable to add button to toolbar", GetLastError());
				return RC_ERROR;
			}
			if (control->textval != NULL) {
				if (!SendMessage(hwndCtrl, TTM_ADDTOOL, 0, (LPARAM) &toolinfo)) {
					guiaErrorMsg("Unable to add tool tip to toolbar", GetLastError());
					return RC_ERROR;
				}
			}
			if (ISMARKED(control)) {
				control->changeflag = TRUE;
				if (!SendMessage(res->hwndtoolbar, TB_CHECKBUTTON, tbbutton.idCommand, (LPARAM) MAKELONG(TRUE, 0))) {
					guiaErrorMsg("Unable to mark button in toolbar", GetLastError());
					return RC_ERROR;
				}
				control->changeflag = FALSE;
			}
			if (ISGRAY(control)) {
				control->changeflag = TRUE;
				if (!SendMessage(res->hwndtoolbar, TB_ENABLEBUTTON, control->useritem + ITEM_MSG_START, (LPARAM) MAKELONG(FALSE, 0))) {
					guiaErrorMsg("Unable to gray button in toolbar", GetLastError());
					return RC_ERROR;
				}
				control->changeflag = FALSE;
			}
		}
	}
	return 0;
}

/**
 * Will return 0 or RC_ERROR
 */
static LONG toolbardestroy(RESOURCE *res)
{
	WINDOW *win;
	RECT rect;
	INT vsize, iWinWidth, iWinHeight;
	INT notMaximized;

	win = *res->winshow;
	/*
	 * Do not attempt to change the window size if it is maximized
	 */
	notMaximized = !(GetWindowLongPtr(win->hwnd, GWL_STYLE) & WS_MAXIMIZE);

	if (notMaximized) {
		GetWindowRect(win->hwnd, &rect);
		iWinWidth = rect.right - rect.left;
		iWinHeight = rect.bottom - rect.top;
	}

	GetWindowRect(res->hwndtoolbar, &rect);
	vsize = rect.bottom - rect.top + 2;
	if (!DestroyWindow(res->hwndtoolbar)) return RC_ERROR;
	rect.top = 0;
	rect.left = 0;
	rect.right = win->clientx;
	rect.bottom = win->clienty;
	ScrollWindow(win->hwnd, 0, -vsize, NULL, &rect);
	if (notMaximized)
		SetWindowPos(win->hwnd, NULL, 0, 0, iWinWidth, iWinHeight - vsize, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

	win->bordertop = 0;
	res->winshow = NULL;
	return 0;
}

/**
 * Runs on the Windows message thread.
 * Always returns zero.
 */
static LONG erasedroplistbox(CONTROL *control)
{
	UINT nMessage;
	if (PANEL_LISTBOX == control->type || PANEL_MLISTBOX == control->type ||
			PANEL_LISTBOXHS == control->type || PANEL_MLISTBOXHS == control->type) {
		nMessage = LB_RESETCONTENT;
		if (control->listboxtabs == NULL && (control->type == PANEL_MLISTBOXHS || control->type == PANEL_LISTBOXHS)) {
			/* remove horizontal scroll bar */
			SendMessage(control->ctlhandle, LB_SETHORIZONTALEXTENT, (WPARAM) 0, (LPARAM) 0);
			control->maxlinewidth = 0;
		}
	}
	else nMessage = CB_RESETCONTENT;
	SendMessage(control->ctlhandle, nMessage, 0, 0L);
	return 0;
}

static LONG settoolbartext(CONTROL *control)
{
	WINDOW *win;
	RESOURCE *res;
	HWND hwndCtrl;
	RECT rect;
	if (control->res == NULL) return 0;
	res = *control->res;
	if (res->winshow == NULL) return 0;
	win = *res->winshow;
	hwndCtrl = (HWND) SendMessage(res->hwndtoolbar, TB_GETTOOLTIPS, 0, 0);
	toolinfo.cbSize = sizeof(TOOLINFO);
	if (control->type == PANEL_DROPBOX) {
		GetClientRect(control->ctlhandle, &rect); /* Get coordinates of control's client area */
		/* initialize members of the toolinfo structure */
		toolinfo.uFlags = TTF_SUBCLASS;
		toolinfo.hwnd = control->ctlhandle;
		toolinfo.hinst = hinstancemain;
		toolinfo.uId = 0;
		toolinfo.lpszText = (CHAR *) control->tooltip;
		toolinfo.rect.left = rect.left;
		toolinfo.rect.top = rect.top;
		toolinfo.rect.right = rect.right;
		toolinfo.rect.bottom = rect.bottom;
	    if (!SendMessage(hwndCtrl, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &toolinfo)) {
			guiaErrorMsg("Unable to add tool tip to toolbar dropbox", GetLastError());
	    	return RC_ERROR;
		}
	}
	else {
		toolinfo.uFlags = TTF_CENTERTIP;
		toolinfo.hwnd = win->hwnd;
		toolinfo.hinst = NULL;
		toolinfo.uId = control->useritem + ITEM_MSG_START;
		toolinfo.lpszText = LPSTR_TEXTCALLBACK;
		if (!SendMessage(hwndCtrl, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &toolinfo)) {
			guiaErrorMsg("Unable to add tool tip to toolbar button", GetLastError());
			return RC_ERROR;
		}
	}
	return 0;
}

static LONG statusbarcreate(WINDOW *win)
{
	RECT rect;
	INT vsize, iWinWidth, iWinHeight;

//	if (!initcomctlflag) {
//		InitCommonControls();
//		initcomctlflag = TRUE;
//	}
	GetWindowRect(win->hwnd, &rect);
	iWinWidth = rect.right - rect.left;
	iWinHeight = rect.bottom - rect.top;
	win->hwndstatusbar = CreateWindowEx(0L, STATUSCLASSNAME,
		(CHAR *) win->statusbartext, WS_CHILD , 0, 0, 0, 0,
		win->hwnd, (HMENU) ITEM_MSG_STATUSBAR, hinstancemain, NULL);
	if (win->hwndstatusbar == NULL) {
		guiaErrorMsg("Create statusbar error", GetLastError());
		return RC_ERROR;
	}
	SendMessage(win->hwndstatusbar, SB_SIMPLE, (WPARAM) TRUE, (LPARAM) 0);
	SendMessage(win->hwndstatusbar, WM_SETFONT, (WPARAM) hfontcontrolfont, MAKELONG((DWORD) TRUE, 0));
	GetWindowRect(win->hwndstatusbar, &rect);
	vsize = rect.bottom - rect.top;
	win->borderbtm = vsize;
	SetWindowPos(win->hwnd, NULL, 0, 0, iWinWidth, iWinHeight + vsize, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	SetWindowPos(win->hwndstatusbar, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_SHOWWINDOW);
	if (!SendMessage(win->hwndstatusbar, SB_SETTEXT, (WPARAM) 255, (LPARAM) win->statusbartext)) {
		return RC_ERROR;
	}
	return 0;
}

static LONG statusbardestroy(WINDOW *win)
{
	RECT rect;
	INT vsize, iWinWidth, iWinHeight;

	vsize = 0;
	GetWindowRect(win->hwnd, &rect);
	iWinWidth = rect.right - rect.left;
	iWinHeight = rect.bottom - rect.top;
	if (IsZoomed(win->hwnd) == 0) vsize = win->borderbtm;
	if (!DestroyWindow(win->hwndstatusbar)) return RC_ERROR;
	SetWindowPos(win->hwnd, NULL, 0, 0, iWinWidth, iWinHeight - vsize, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	win->borderbtm = 0;
	win->hwndstatusbar = NULL;
	return 0;
}

static LONG statusbartext(WINDOW *win)
{
	if (!SendMessage(win->hwndstatusbar, SB_SETTEXT, (WPARAM) 255, (LPARAM) win->statusbartext)) return RC_ERROR;
	return 0;
}

/**
 * This always runs in the Windows thread
 */
LONG setctrlfocus(CONTROL *control)
{
	if (control == NULL) SetFocus(NULL);
	else if (!ISSHOWONLY(control) /* && !ISNOFOCUS(control) */) {
		if (!focuswinold) {
			// Problem: If the control is a dropbox in a toolbar,
			// how do we tell that it is or is NOT in a background window?
			//(*control->res)->hwndtoolbar has the HWND of the toolbar
			// Solution: Get the HWND of the toolbar and see if it is a child of the foreground window.
			HWND foreWin = GetForegroundWindow();
			HWND controlParentWindow = (*control->res)->hwnd;
			if ((*control->res)->restype == RES_TYPE_TOOLBAR)
			{
				HWND toolbarHwnd = (*control->res)->hwndtoolbar;
				if (!IsChild(foreWin, toolbarHwnd)) {
					(*control->res)->ctlfocus = control;
					return 0;
				}
			}
			else if (foreWin != controlParentWindow) {
				/* changing focus to a control in a background window */
				(*control->res)->ctlfocus = control;
				return 0;
			}
		}
		if (ISRADIO(control)) radioButtonSuppressAutoCheck = TRUE;
		SetFocus(control->ctlhandle);
		if (control->type == PANEL_TAB) {
			drawTabHeaderFocusRectangle(control, TRUE);
		}
		if (ISRADIO(control)) radioButtonSuppressAutoCheck = FALSE;
		else if (ISEDIT(control)) {
			if (control->editinsertpos < 0)
				control->editinsertpos = 0;
			if ((control->editinsertpos = 0) && (control->editendpos <= 0))
				control->editendpos = -1;
			SendMessage(control->ctlhandle, EM_SETSEL, control->editinsertpos, control->editendpos);
		}
	}
	return 0;
}

static LONG addmenuitem(MENUENTRY *pMenuItem, UINT itemnumber)
{
	ZHANDLE zhMB = NULL;
	CHAR *MenuBuffer;
	INT retval, menutxtlen;
	MENUITEMINFO mii;
	CHAR work[64];

	mii.cbSize = sizeof(MENUITEMINFO);
	mii.fMask = MIIM_TYPE;
	if (pMenuItem->style & MENUSTYLE_LINE) mii.fType = MFT_SEPARATOR;
	else {
		mii.fType = MFT_STRING;
		mii.fState = 0;
		mii.fMask |= MIIM_STATE | MIIM_ID;
		mii.wID = pMenuItem->useritem;
		if (pMenuItem->speedkey) {
			/* add speed keys to menu item text */
			menutxtlen = (INT)strlen((CHAR *) pMenuItem->text);
			zhMB = guiAllocMem(menutxtlen + 15 * sizeof(CHAR));
			if (zhMB == NULL) {
				sprintf(work, "guiAllocMem failure in %s", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
			MenuBuffer = (CHAR *) guiLockMem(zhMB);
			strcpy(MenuBuffer, pMenuItem->text);
			MenuBuffer[menutxtlen] = '\t';
			retval = speedKeyText(pMenuItem, MenuBuffer, menutxtlen);
			if (retval) {
				guiUnlockMem(zhMB);
				guiFreeMem(zhMB);
				return retval;
			}
			mii.dwTypeData = MenuBuffer;
		}
		else mii.dwTypeData = (CHAR *) pMenuItem->text;
		if (pMenuItem->style & MENUSTYLE_DISABLED) mii.fState |= MFS_GRAYED;
		if (pMenuItem->style & MENUSTYLE_SUBMENU) {
			mii.fMask |= MIIM_SUBMENU;
			mii.hSubMenu = pMenuItem->submenu;
		}
		else if (pMenuItem->style & MENUSTYLE_MARKED) mii.fState |= MFS_CHECKED;
	}

	checkForIconItem(pMenuItem, &mii, pMenuItem->menuhandle);

	/*
	 * Store a pointer to our MENUENTRY structure in the widget.
	 * This makes processing the WM_DRAWITEM and WM_MEASUREITEM messages much easier.
	 */
	mii.fMask |= MIIM_DATA;
	mii.dwItemData = (ULONG_PTR) pMenuItem;

	retval = InsertMenuItem(pMenuItem->menuhandle, itemnumber, TRUE, &mii);
	if (zhMB != NULL) {
		guiUnlockMem(zhMB);
		guiFreeMem(zhMB);
	}
	if (!retval) {
		sprintf(work, "Error appending item to menu in %s", __FUNCTION__);
		guiaErrorMsg(work, GetLastError());
		return RC_ERROR;
	}
	return 0;
}

static LONG deletemenuitem(HMENU hMenu, UINT itemnumber)
{
	return(DeleteMenu(hMenu, itemnumber, MF_BYPOSITION));
}


static void clearPBDefStyle(CONTROL* control) {
	WINHANDLE winhandle;
	WINDOW *win;
	RESOURCE *res;
	DRAWITEMSTRUCT drawItem;
	CHAR message[256];

	if (control == NULL) {
		sprintf(message, "intercepted fatal error in %s\n", __FUNCTION__);
		MessageBox(NULL, message, "DB/C DEBUG",	MB_OK | MB_ICONSTOP | MB_SETFOREGROUND);
		return;
	}

	if (!ISDEFPUSHBUTTON(control)) return;
	if (control->type == PANEL_DEFPUSHBUTTON) control->type = PANEL_PUSHBUTTON;
	else if (control->type == PANEL_ICONDEFPUSHBUTTON) control->type = PANEL_ICONPUSHBUTTON;
	res = *control->res;
	if (res == NULL) {
		sprintf(message, "intercepted fatal error in clearPBDefStyle(): ctype=%hu, citem=%hu, cshown=%d\n",
			control->type, control->useritem, ((control->shownflag) ? 1 : 0));
		MessageBox(NULL, message, "DB/C DEBUG",	MB_OK | MB_ICONSTOP | MB_SETFOREGROUND);
		return;
	}
	if (res->ctldefault == control) res->ctldefault = NULL;
	SendMessage(res->hwnd, DM_SETDEFID, 0L, 0L);
	if (!control->shownflag) return;
	winhandle = res->winshow;
	if (winhandle == NULL) return;
	win = *winhandle;
	if (win != NULL && win->ctldefault == control) win->ctldefault = NULL;
	/* For Icon buttons, Win32 doesn't always send us the expected WM_DRAWITEM
	   message. We must send one to ourselves to make sure that the old deficonbutton
	   gets re-drawn with a normal border. BM_SETSTYLE occasionaly failed to do this*/
	if (control->type == PANEL_ICONPUSHBUTTON) {
		memset(&drawItem, 0, sizeof(DRAWITEMSTRUCT));
		drawItem.CtlType = ODT_BUTTON;
		drawItem.CtlID   = control->useritem + ITEM_MSG_START;
		drawItem.itemAction = ODA_DRAWENTIRE;
		drawItem.hwndItem = control->ctlhandle;
		drawItem.hDC = GetDC(drawItem.hwndItem);
		drawItem.rcItem = control->rect;
		drawItem.rcItem.right = ++drawItem.rcItem.right - drawItem.rcItem.left;
		drawItem.rcItem.bottom = ++drawItem.rcItem.bottom - drawItem.rcItem.top;
		drawItem.rcItem.top = drawItem.rcItem.left = 0;
		SendMessage(win->hwnd, WM_DRAWITEM, drawItem.CtlID, (LPARAM)&drawItem);
		ReleaseDC(drawItem.hwndItem, drawItem.hDC);
	}
	else {
		SendMessage(control->ctlhandle, BM_SETSTYLE, BS_PUSHBUTTON, (LPARAM) TRUE);
	}
}


static void setPBDefStyle(CONTROL* control, INT location) {
	WINDOW *win;
	RESOURCE *res;
	CHAR message[256];

	/*
	 * Fixup on 8/23/10
	 * If the user is so foolish as to use the enter key as a tab,
	 * disable setting a default pushbutton
	 */
	if (enterkeybehavior == enterKeyBehavior_tab) return;

	if (control == NULL) {
		sprintf_s(message, ARRAYSIZE(message), "intercepted fatal error in setPBDefStyle(): from location=%d\n", location);
		MessageBox(NULL, message, "DB/C DEBUG",	MB_OK | MB_ICONSTOP | MB_SETFOREGROUND);
		return;
	}
	res = *control->res;
	if (res == NULL) {
		sprintf_s(message, ARRAYSIZE(message), "intercepted fatal error in setPBDefStyle(): from location=%d, ctype=%hu, citem=%hu, cshown=%d\n",
			location, control->type, control->useritem, ((control->shownflag) ? 1 : 0));
		MessageBox(NULL, message, "DB/C DEBUG",	MB_OK | MB_ICONSTOP | MB_SETFOREGROUND);
		return;
	}
	win = *res->winshow;
	if (control->type == PANEL_PUSHBUTTON) control->type = PANEL_DEFPUSHBUTTON;
	else if (control->type == PANEL_ICONPUSHBUTTON)	control->type = PANEL_ICONDEFPUSHBUTTON;
		res->ctldefault = control;
		SendMessage(res->hwnd, DM_SETDEFID, control->useritem, 0L);
	if (control->shownflag) {
		SendMessage(control->ctlhandle, BM_SETSTYLE, BS_DEFPUSHBUTTON, (LPARAM) TRUE);
		/*
		 * If this control's resource is shown on a window, set the ctldefault field of the window
		 */
		if (win != NULL) win->ctldefault = control;
	}
}

/**
 *  window procedure for DB/C dialog windows
 */
LRESULT CALLBACK guiaDlgProc(HWND hwnd, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	CHAR *msgid;
	CHAR xtext[TREEVIEW_ITEM_MAX_SIZE];
	INT iCtlNum, iCtlsave, textlen, iCurCtrl, color;
	LRESULT rc;
	//WINDOW *win;
	RESOURCE *res;
	RESHANDLE currentreshandle;
	CONTROL *control, *tcontrol, *pctlBase, *currentcontrol;
	HWND hwndDlg, hwndCtrl;
	NMHDR *nmhdr;
	TREESTRUCT *tpos;
	TVITEM tvitem;
	static BOOL focwasontab = FALSE;

	switch (nMessage) {

	case WM_PAINT:
		/* NOTE: */
	 	/* After using GetUpdateRect() here, I discovered that the update region is not correct */
	 	/* for certain dialogs with some combination of tabfolders/dropboxes (was reported by APS). */
	 	/* Until the cause of this behavior is found, invalidating entire window region will work. - SSN */
		InvalidateRect(hwnd, NULL, FALSE);
		break;

	case WM_CTLCOLORSTATIC:
		rc = handleCtlColorStatic(wParam, lParam);
		if (rc == -1) break;
		return rc;

	case WM_CTLCOLORLISTBOX:
	case WM_CTLCOLOREDIT:
		rc = handleCtlColorEdit(hwnd, wParam, lParam);
		if (rc == -1) break;
		return (LRESULT) rc;

	case WM_CTLCOLORBTN:
		control = (CONTROL *) GetWindowLongPtr((HWND) lParam, GWLP_USERDATA);
		if (control != NULL) {
			if (control->textcolor != TEXTCOLOR_DEFAULT) color = control->textcolor;
			else color = TEXTCOLOR_BLACK;
			SetBkColor((HDC) wParam, GetSysColor(COLOR_BTNFACE));
			guiaSetControlTextColor((HDC) wParam, control, GetSysColor(COLOR_WINDOWTEXT));
			return (LRESULT) hbrButtonFace;
		}
		break;

//	case WM_CTLCOLORSCROLLBAR:
//		if (ctl3dflag) return((LONG) hbrushbackground);
//		return 0;

	case WM_MOUSEACTIVATE:
		if (dlgreshandlelisthdr != NULL) {
			pvistart();
			res = *dlgreshandlelisthdr;
			hwndDlg = res->hwnd;
			if (hwnd != hwndDlg) {
				if (res->ctlfocus != NULL) hwndCtrl = res->ctlfocus->ctlhandle;
				else hwndCtrl = NULL;
				pviend();
				SetActiveWindow(hwndDlg);
				if (hwndCtrl != NULL) SetFocus(hwndCtrl);
				return(MA_NOACTIVATEANDEAT);
			}
			pviend();
		}
		break;
	case WM_ACTIVATEAPP:
		if (dlgreshandlelisthdr != NULL && wParam) {	/* is there a modal dialog active? */
			pvistart();
			res = *dlgreshandlelisthdr;
			hwndDlg = res->hwnd;
			if (res->ctlfocus != NULL) hwndCtrl = res->ctlfocus->ctlhandle;
			else hwndCtrl = NULL;
			pviend();

			SetActiveWindow(hwndDlg);
			if (hwndCtrl != NULL) SetFocus(hwndCtrl);
			return 0;
		}
		break;
	case WM_SETFOCUS:
		if (dlgreshandlelisthdr == NULL) break;
		hwndCtrl = NULL;
		pvistart();
		res = *dlgreshandlelisthdr;
		hwndDlg = res->hwnd;
		if (res->ctlfocus != NULL) hwndCtrl = res->ctlfocus->ctlhandle;
		pviend();

		SetActiveWindow(hwndDlg);
		if (hwndCtrl != NULL){
			SetFocus(hwndCtrl);
		}
		break;

	HANDLE_MSG(hwnd, WM_MEASUREITEM, WinProc_OnMeasureItem); /* This function is in guiawin3 */

	case WM_DRAWITEM:
		WinProc_OnDrawItem(hwnd, (const DRAWITEMSTRUCT *)lParam); /* This function is in guiawin3 */
		return 1;

	case WM_SYSCOMMAND:
		if (wParam == SC_CLOSE) {
			currentreshandle = (RESHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			if (currentreshandle == NULL) break;
			pvistart();
			res = *currentreshandle;
			rescb(res, DBC_MSGCLOSE, 0, 0);
			pviend();
			return 0;
		}
		break;
	case WM_COMMAND:
		if (checkSingleLineEditFocusNotification(wParam, lParam))
			return(DefWindowProc(hwnd, nMessage, wParam, lParam));
		currentreshandle = (RESHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (currentreshandle == NULL) break;
		pvistart();
		res = *currentreshandle;
		while (res != NULL) {
			control = (CONTROL *) res->content;
			iCurCtrl = res->entrycount;
			while(iCurCtrl--) {
				if (control->useritem == LOWORD(wParam) - ITEM_MSG_START &&
					control->ctlhandle == (HWND) lParam) {
					if (res->winshow == NULL || control->changeflag) {
						pviend();
						return 0;
					}
					if (ISLOCKBUTTON(control)) {
						control->style ^= CTRLSTYLE_MARKED;
					}
					if (ISCHECKTYPE(control) && HIWORD(wParam) == BN_CLICKED) {
						res->ctlfocus = control;
						if (ISCHECKBOX(control)) {
							handleCheckboxStateChange(control, (HWND)lParam);
						}
						else if (!radioButtonSuppressAutoCheck) {
							control->style |= CTRLSTYLE_MARKED;
						}
						if (ISRADIO(control) && !radioButtonSuppressAutoCheck){
							pctlBase = (CONTROL *) res->content;
							iCtlsave = iCtlNum = control->ctloffset;
							while(iCtlNum < res->entrycount && pctlBase[iCtlNum].type == PANEL_RADIOBUTTON){
								iCtlNum++;
							}
							do {
								if (iCtlNum != iCtlsave) {
									pctlBase[iCtlNum].style &= ~CTRLSTYLE_MARKED;
									SendMessage(pctlBase[iCtlNum].ctlhandle, BM_SETCHECK, BST_UNCHECKED, 0);
								}
								iCtlNum--;
							}
							while (iCtlNum > -1 && pctlBase[iCtlNum].type == PANEL_RADIOBUTTON);
							/* Make sure that all buttons that should be checked are indeed checked - SSN */
							iCurCtrl = res->entrycount;
							tcontrol = (CONTROL *) res->content;
							while(iCurCtrl--) {
								if (ISRADIO(tcontrol) && ISMARKED(tcontrol)) {
									SendMessage(tcontrol->ctlhandle, BM_SETCHECK, BST_CHECKED, 0);
								}
								tcontrol++;
							}
						}
					}
					rc = guiaResCallBack(res, control, HIWORD(wParam));
					pviend();

					return(DefWindowProc(hwnd, nMessage, wParam, lParam));
				}
				control++;
			}
			if (res->showresnext == NULL) break;
			res = *res->showresnext;
		}
		pviend();
		break;
	case WM_VSCROLL:
	case WM_HSCROLL:
		currentreshandle = (RESHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (currentreshandle == NULL) break;
		pvistart();
		res = *currentreshandle;
		while (res != NULL) {
			control = (CONTROL *) res->content;
			iCurCtrl = res->entrycount;
			while(iCurCtrl--) {
				if (control->ctlhandle == (HWND) lParam)  {
//					if (res->winshow == NULL || control->changeflag) {}
//					else guiaScrollCallBack(res, control, wParam);
					if (res->winshow != NULL && !control->changeflag) guiaScrollCallBack(res, control, wParam);
					pviend();
					return 0;
				}
				control++;
			}
			if (res->showresnext == NULL) break;
			res = *res->showresnext;
		}
		pviend();
		break;
	case WM_NOTIFY:
		nmhdr = (LPNMHDR)lParam;
		if (nmhdr->code == TCN_SELCHANGING || nmhdr->code == TCN_SELCHANGE /* || nmhdr->code == GUIAM_TCN_SELCHANGE_NOFOCUS */)
		{
			handleTabChangeChanging(hwnd, lParam, &focwasontab);
//			currentcontrol = (CONTROL *) GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
//			if (currentcontrol == NULL || !currentcontrol->tabgroup) break;
//			pvistart();
//			HideTabControls(currentcontrol);
//			res = *currentcontrol->res;
//			win = *res->winshow;
//			i1 = TabCtrl_GetCurSel(nmhdr->hwndFrom) + 1;
//			control = (CONTROL *) res->content;
//			for (iCurCtrl = 0; iCurCtrl < res->entrycount; iCurCtrl++, control++) {
//				if (control->tabgroup != currentcontrol->tabgroup) continue;
//					control->selectedtab = i1 - 1;
//				if (control->tabsubgroup != i1) continue;
//				if (control->type == PANEL_TAB) {
//					if (res->itemmsgs && (textlen = (INT)strlen((LPSTR) control->textval))) {
//						memcpy(cbmsgdata, control->textval, min(textlen, MAXCBMSGSIZE - 17));
//						rescb(res, DBC_MSGITEM, control->useritem, textlen);
//					}
//				}
//				else {
//					control->changeflag = TRUE;
//					if (control->ctlhandle == NULL) makecontrol(control, 0 - win->scrollx, win->bordertop - win->scrolly, hwnd, TRUE);
//					else {
//						ShowWindow(control->ctlhandle, SW_SHOW);
//						if (control->type != PANEL_TABLE) setControlWndProc(control);
//						if (ISEDIT(control) && control->type != PANEL_FEDIT) {
//							SetWindowText(control->ctlhandle, (LPCSTR) control->textval);
//						}
//					}
//					control->changeflag = FALSE;
//				}
//				control->shownflag = TRUE;
//			}
//			pviend();
		}
		else if (nmhdr->code == TVN_SELCHANGED || nmhdr->code == TVN_ITEMEXPANDED || nmhdr->code == TVN_KEYDOWN) {
			currentcontrol = (CONTROL *) GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
			if (currentcontrol == NULL) break;
			pvistart();
			tpos = NULL;
			res = *currentcontrol->res;
			control = (CONTROL *) res->content;
			for (iCurCtrl = res->entrycount; iCurCtrl; iCurCtrl--, control++) {
				if (control->ctlhandle == nmhdr->hwndFrom && control->type == PANEL_TREE)  {
					if (nmhdr->code == TVN_SELCHANGED) {
						/* copy text from currently selected item into control structure */
						tvitem = ((LPNMTREEVIEW) lParam)->itemNew;
						tvitem.mask = TVIF_TEXT;
						tvitem.pszText = &control->tree.text[0];
						tvitem.cchTextMax = TREEVIEW_ITEM_MAX_SIZE;
						TreeView_GetItem(control->ctlhandle, &tvitem);
						control->tree.hitem = tvitem.hItem;
						control->tree.spos = tpos = treesearch(control->tree.root, ((LPNMTREEVIEW) lParam)->itemNew.hItem);
						/* now do message stuff if necessary */
						if (!res->itemmsgs) break;
						else {
							msgid = DBC_MSGITEM;
						}
					}
					else if (nmhdr->code == TVN_KEYDOWN) {
						if (((TV_KEYDOWN*)lParam)->wVKey == 0) { /* fabricated enter press message */
							msgid = DBC_MSGPICK;
						}
						else break;
					}
					else if (nmhdr->code == TVN_ITEMEXPANDED) {
						if (((LPNMTREEVIEW) lParam)->action & TVE_COLLAPSE) {
							msgid = DBC_MSGCLOSE;
							tpos = treesearch(control->tree.root, ((LPNMTREEVIEW) lParam)->itemNew.hItem);
							tpos->expanded = FALSE;
						}
						else {
							msgid = DBC_MSGOPEN;
							tpos = treesearch(control->tree.root, ((LPNMTREEVIEW) lParam)->itemNew.hItem);
							tpos->expanded = TRUE;
						}
					}
					if (res->linemsgs) {
						cbmsgdata[0] = '\0';
						if (nmhdr->code == TVN_KEYDOWN) {
							tpos = treesearch(control->tree.root, control->tree.hitem);
						}
						else {
							if (tpos == NULL) { /* don't search again after TVN_ITEMEXPANDED or TVN_SELCHANGED code */
								tpos = treesearch(control->tree.root, ((LPNMTREEVIEW) lParam)->itemNew.hItem);
							}
						}
						if (tpos == NULL) break; /* should never happen */
						treeposition((CHAR *) cbmsgdata, control->tree.root, tpos);
						textlen = (INT)strlen((CHAR *) cbmsgdata);
						/* remove trailing comma */
						if (textlen > 0 && cbmsgdata[textlen - 1] == ',') cbmsgdata[--textlen] = '\0';
						rescb(res, msgid, control->useritem, textlen);
					}
					else {
						if (nmhdr->code == TVN_ITEMEXPANDED) {
							/* use text from currently selected item */
							tvitem = ((LPNMTREEVIEW) lParam)->itemNew;
							tvitem.mask = TVIF_TEXT;
							tvitem.pszText = &xtext[0];
							tvitem.cchTextMax = TREEVIEW_ITEM_MAX_SIZE;
							TreeView_GetItem(control->ctlhandle, &tvitem);
							StringCbCopy((LPSTR)cbmsgdata, MAXCBMSGSIZE - 17, xtext);
							//strcpy((CHAR *) cbmsgdata, xtext);
							rescb(res, msgid, control->useritem, (INT)strlen(xtext));
						}
						else {
							StringCbCopy((LPSTR)cbmsgdata, MAXCBMSGSIZE - 17, control->tree.text);
							//strcpy((CHAR *) cbmsgdata, control->tree.text);
							rescb(res, msgid, control->useritem, (INT)strlen(control->tree.text));
						}
					}
					break;
				}
			}
			pviend();
		}
		else if (nmhdr->code == (UINT) NM_CUSTOMDRAW) {
			control = (CONTROL *) GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
			if (control == NULL || control->type != PANEL_TREE) break;
			return handleTree_CustomDraw(control, (LPNMTVCUSTOMDRAW)lParam);
			}
		break;
	}
	return(DefWindowProc(hwnd, nMessage, wParam, lParam));
}

static BOOL isTablePart(HWND hwnd) {
	HWND parent1;
	CHAR NewText[150];
	parent1 = GetParent(hwnd);
	if (parent1 == NULL) return FALSE;
	if (!GetClassName(parent1, NewText, 150)) return FALSE;
	if (strcmp(szTableCellsClassName, NewText) == 0) return TRUE;
	return FALSE;
}

static HWND getParentTableWindow(HWND cellWindow) {
	return GetParent(GetParent(cellWindow));
}

static void relayTooltipEvent(CONTROL *control, HWND hwnd, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	MSG msg;
	if (control->tooltip != NULL) {
		msg.hwnd = hwnd;
		msg.message = nMessage;
		msg.wParam = wParam;
		msg.lParam = lParam;
		msg.time = (DWORD) 0;
		msg.pt.x = msg.pt.y = 0;
		SendMessage((*(*control->res)->winshow)->hwndtt, TTM_RELAYEVENT, 0, (LPARAM) &msg);
	}
}

/**
 *  sub-classed control window procedure
 */
LRESULT CALLBACK guiaControlProc(HWND hwnd, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	INT color, iCtlNum, iCtlsave, iInsertPos, iEndPos, iLineIndex, datasize;
	UINT n1;
	INT i1, textlen;
	CHAR NewText[150], CBText[150], work[128], *ptr;
	USHORT usItem;
	LRESULT retcode;
	HWND hwndParent, hwndControl, hwndGetFocus;
	RESOURCE *res, *testGetFocus;
	RESHANDLE currentreshandle;
	CONTROL *control, *savepcontrol, *pctlBase, *pctlOldFocus, *pctlOldDefault;
	WINDOW *win;
	TCITEM tc;
	NMHDR nmhdr;
	TREESTRUCT *tpos;
	TV_KEYDOWN tvkd;
	USHORT ctrlkeydown;

	control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

	if (control == NULL || control->ctlhandle != hwnd) return 0;

	/*
	 * We must short-circuit the big Switch for some messages, they would otherwise
	 * cause an infinite loop. It is not known why this is.
	 */
	if (nMessage == TCM_GETCURSEL || nMessage == TCM_GETITEM || nMessage == WM_NCDESTROY) {
		if (control->oldproc != NULL) {
			return CallWindowProc(control->oldproc, hwnd, nMessage, wParam, lParam);
		}
	}

	/* Each tab of a tabgroup is a separate control structure to us.  But all of the tabs in a tab group
	   are in one window as far as Windows is concerned.
	   We store in the 'Item' for each tab the handle to our corresponding control structure.
	   We retrieve that information now.
	*/
	if (control->type == PANEL_TAB) {
		n1 = TabCtrl_GetCurSel(hwnd);			/* TCM_GETCURSEL */
		if (n1 != -1 && control->tabsubgroup != n1 + 1){
			tc.mask = TCIF_PARAM;
			TabCtrl_GetItem(hwnd, n1, &tc);		/* TCM_GETITEM */
			control = (CONTROL *) tc.lParam;
		}
	}

	switch (nMessage) {

	case WM_ERASEBKGND:
		if ((control->type == PANEL_BOXTITLE || control->type == PANEL_BOX)
				&& control->tabgroup != 0 /* Only if in a tab */) {
			RECT rc, boxRect, testRect;
			GetClientRect(hwnd, &rc);
			FillRect((HDC) wParam, &rc, hbrButtonFace);
			// March 1, 2024 jpr
			// Cause painting of all controls that are both on this tab and in this box.
			// Fixes an ugly visual glitch of bleed through inside the box of any background.
			//
			RESOURCE* res = *control->res;
			CONTROL* ctrls = (CONTROL *) res->content;
			boxRect = rc;
			for (int i2 = 0; i2 < res->entrycount; i2++) {
				CONTROL* otherControl = &ctrls[i2];
				if (otherControl == control) continue;
				if (control->tabgroup != otherControl->tabgroup) continue;
				if (control->tabsubgroup != otherControl->tabsubgroup) continue;
				GetClientRect(otherControl->ctlhandle, &rc);
				if (IntersectRect(&testRect, &boxRect, &rc))
					InvalidateRect(otherControl->ctlhandle, &rc, FALSE);
			}
			return 1;
		}
		break;
	case WM_LBUTTONDBLCLK:
		if (control->type == PANEL_TREE) {
			pvistart();
			res = *control->res;
			if (res->linemsgs) {
				cbmsgdata[0] = '\0';
				tpos = treesearch(control->tree.root, control->tree.hitem);
				if (tpos == NULL) {   /* should never happen */
					pviend();
					break;
				}
				treeposition((CHAR *) cbmsgdata, control->tree.root, tpos);
				textlen = (INT)strlen((CHAR *) cbmsgdata);
				/* remove trailing comma */
				if (textlen > 0 && cbmsgdata[textlen - 1] == ',') cbmsgdata[--textlen] = '\0';
				rescb(res, DBC_MSGPICK, control->useritem, textlen);
			}
			else {
				StringCbCopy((LPSTR)cbmsgdata, MAXCBMSGSIZE - 17, control->tree.text);
				//strcpy((CHAR *) cbmsgdata, control->tree.text);
				rescb(res, DBC_MSGPICK, control->useritem, (INT)strlen(control->tree.text));
			}
			pviend();
		}
		break;
	case WM_LBUTTONUP:
		relayTooltipEvent(control, hwnd, nMessage, wParam, lParam);
		if (ISEDIT(control)) {
			if (control->oldproc != NULL) retcode = CallWindowProc(control->oldproc, hwnd, nMessage, wParam, lParam);
			SendMessage(control->ctlhandle, EM_GETSEL, (WPARAM) &iInsertPos, (LPARAM) &iEndPos);
			if (iInsertPos != control->editinsertpos || iEndPos != control->editendpos) {
				i1 = sendESELMessage(control, iInsertPos, iEndPos);
				if (i1 != 0) retcode = i1;
			}
			return(retcode);
		}
		break;

	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
		relayTooltipEvent(control, hwnd, nMessage, wParam, lParam); // @suppress("No break at end of case")
		/* fall through */
	case WM_RBUTTONDBLCLK:
		if (!sendRightMouseMessageFromPanel(hwnd, nMessage, control, lParam, (UINT) wParam)) return 0;
		break;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEMOVE:
		relayTooltipEvent(control, hwnd, nMessage, wParam, lParam);
		break;

	case WM_CTLCOLORLISTBOX:
		/* color the list box from a drop box */
		if (control != NULL) {
			res = *control->res;
			if (control->textcolor != TEXTCOLOR_DEFAULT) color = control->textcolor;
			else if (res->textcolor != TEXTCOLOR_DEFAULT) color = control->textcolor;
			else color = TEXTCOLOR_DEFAULT;
			SetBkColor((HDC) wParam, GetSysColor(COLOR_WINDOW));
			guiaSetControlTextColor((HDC) wParam, control, GetSysColor(COLOR_WINDOWTEXT));
			return (LRESULT) hbrWindow;
		}
		break;
	case BM_SETSTYLE:
		if (ISICONBUTTON(control)) return 0;
		break;
	case WM_NCHITTEST:
		if (ISGRAY(control)) return 0;
		break;

	case WM_KILLFOCUS:
		if (ISMEDIT(control)) {
			SendMessage(control->ctlhandle, EM_GETSEL, (WPARAM)&(control->editinsertpos), (LPARAM)&(control->editendpos));
		}
		/**
		 * wParam
		 * A handle to the window that receives the keyboard focus. This parameter can be NULL.
		 */
		if (wParam == (WPARAM)NULL) break;
		if (control->type == PANEL_FEDIT) {
			control->changeflag = TRUE;
			SetWindowText(control->ctlhandle, (CHAR *) control->textval);
			control->changeflag = FALSE;
		}
		//if (ISSINGLELINEEDIT(control)) guiaSetSingleLineEditCaretPos(control);
		pvistart();
		res = *control->res;
		win = *res->winshow;

		hwndGetFocus = (HWND) wParam;
/**
 * This is not right, but it works, so it stays in for now.
 * control->insthandle refers to the control losing focus,
 * it should refer to the control gaining focus.  Doing a
 * GetWindowLong with GWL_HINSTANCE causes problems on Win95/98
 * for some reason.
 * IST 6-14-99
 */
		testGetFocus = NULL;
		GetClassName(hwndGetFocus, NewText, ARRAYSIZE(NewText));
		if (strcmp(dlgclassname, NewText) && strcmp(winclassname, NewText)){
			CONTROL *pctlGetFocus = NULL;
			if (ISMEDIT(control))
				control->editinsertpos = control->editendpos;
			else {
				control->editinsertpos = control->editendpos = 0;
			}
			if (isTablePart(hwndGetFocus))
				pctlGetFocus = (CONTROL *)GetWindowLongPtr(getParentTableWindow(hwndGetFocus), GWLP_USERDATA);
			else pctlGetFocus = (CONTROL *)GetWindowLongPtr(hwndGetFocus, GWLP_USERDATA);
			if (pctlGetFocus != NULL) {
				RESOURCE *presGetFocus = *pctlGetFocus->res;
				testGetFocus = presGetFocus;
				if (*presGetFocus->winshow == win && presGetFocus->restype == RES_TYPE_PANEL)
					res->ctlfocus = NULL;
			}
		}

		if (hwndGetFocus != win->hwnd && testGetFocus && *testGetFocus->winshow == win) {
			if (res->focusmsgs) rescb(res, DBC_MSGLOSEFOCUS, control->useritem, 0);
			if (control->type == PANEL_TAB) drawTabHeaderFocusRectangle(control, FALSE);
		}
		pviend();
		break;

	case WM_SETFOCUS:
		pvistart();
		res = *control->res;

		/*
		 * Sometimes the windows thread gets here while the resource is in the process of being destroyed
		 * over on the DX main thread. Should not happen but it does.
		 */
		if (res->isBeingDestroyed) {
			pviend();
			break;
		}
		win = *res->winshow;
		pctlOldFocus = pctlOldDefault = NULL;
		for (currentreshandle = win->showreshdr; currentreshandle != NULL; currentreshandle = res->showresnext) {
			res = *currentreshandle;
			if (res->ctlfocus == NULL) continue;
			pctlOldFocus = res->ctlfocus;
			break;
		}
		for (currentreshandle = win->showreshdr; currentreshandle != NULL; currentreshandle = res->showresnext) {
			res = *currentreshandle;
			if (res->ctldefault == NULL) continue;
			pctlOldDefault = res->ctldefault;
			break;
		}
		res = *control->res;
		if (res->focusmsgs && (pctlOldFocus == NULL || hwnd != pctlOldFocus->ctlhandle)){
			if (control->type != PANEL_TABLE) {
				rescb(res, DBC_MSGFOCUS, control->useritem, 0);
			}
		}

		res->ctlfocus = control;
		/**
		 * Next IF block added 01 MAR 2016 for Dave Enlow of Apex data systems
		 * He noticed that DX version 16.1.4 would select all text in an fedit box when clicked.
		 * And they really liked that behavior.
		 * We had dropped this with 16.1.5+   I don't know why or where.
		 * So this is a bit of a kludge to restore it for him.
		 */
		if (feditTextSelectionOnFocusOld && control->type == PANEL_FEDIT) {
			PostMessage(control->ctlhandle, EM_SETSEL, 0, -1);
		}

		if (ISMEDIT(control)) {
			if (control->editinsertpos < 0) control->editinsertpos = 0;
			if ((control->editinsertpos == 0) && (control->editendpos <= 0))
				control->editendpos = -1;
			if(control->editendpos < 0) control->editendpos = 0;
			PostMessage(control->ctlhandle, EM_SETSEL, control->editinsertpos, control->editendpos);
		}

		if (ISPUSHBUTTON(control) && control != win->ctldefault) {  /* new focus control is a push button */
			if (res->ctldefault != NULL) {
				clearPBDefStyle(res->ctldefault);
			}
			else if (win->ctldefault != NULL) {
				clearPBDefStyle(win->ctldefault);
			}
			setPBDefStyle(control, 1);
		}

		/* Focus is going from a Pushbutton to a Non-pushbutton */
		else if (pctlOldDefault != NULL && ISPUSHBUTTON(pctlOldDefault)) {
			if (win->ctlorgdflt != NULL) {
				if (win->ctlorgdflt->useritem != pctlOldDefault->useritem) {
					clearPBDefStyle(pctlOldDefault);	/* Clear the Default Style of the old Pushbutton */
					setPBDefStyle(win->ctlorgdflt, 2);
				}
			}
			else {
				clearPBDefStyle(pctlOldDefault);	/* Clear the Default Style of the old Pushbutton */
			}
		}
		pviend();
		break;

	case WM_SYSCHAR:
	case WM_CHAR:
		ctrlkeydown = (GetKeyState(VK_CONTROL) & 0x8000);
		if (wParam == VK_RETURN ||  wParam == VK_ESCAPE || wParam == VK_TAB) return 0;
		n1 = toupper((BYTE) wParam);
		if (lParam & 0x20000000 && n1 >= 'A' && n1 <= 'Z') {
			pvistart();
			res = *control->res;
			n1 = guiaAltKeySetFocus(res, n1);
			pviend();
			if (!n1) return 0;
			break;
		}
		if (control->type == PANEL_FEDIT && !(control->style & CTRLSTYLE_READONLY)) {
			if (wParam < 256) {
				pvistart();
				if (control->textval == NULL) {  /* possible if erase happened */
					control->textval = guiAllocMem((INT)strlen((LPSTR) control->mask) + 1);
					if (control->textval == NULL) {
						pviend();
						sprintf(work, "Error in %s, guiAllocMem fail at WM_[SYS]CHAR", __FUNCTION__);
						guiaErrorMsg(work, (DWORD)strlen((LPSTR) control->mask) + 1);
						return 0;
					}
					control->textval[0] = '\0';
				}
				iLineIndex = (INT)SendMessage(control->ctlhandle, EM_LINEINDEX, 0, 0L);
				datasize = (INT)SendMessage(control->ctlhandle, EM_LINELENGTH, iLineIndex, 0L);
				control->changetext = guiAllocMem(datasize + 256);	/* add 256 bytes for buffer length */
				if (control->changetext == NULL) {
					pviend();
					sprintf(work, "Error in %s, guiAllocMem fail at WM_[SYS]CHAR", __FUNCTION__);
					guiaErrorMsg(work, datasize + 256);
					return 0;
				}
				* (WORD *) control->changetext = (WORD) datasize + 1;
				SendMessage(control->ctlhandle, EM_GETLINE, 0, (LPARAM) control->changetext);
				control->changetext[datasize] = '\0';
				SendMessage(control->ctlhandle, EM_GETSEL, (WPARAM) &iInsertPos, (LPARAM) &iEndPos);
				if (VK_BACK == wParam) {  /* backspace */
					if (iInsertPos == iEndPos) {
						if (!iInsertPos) {
							guiFreeMem(control->changetext);
							control->changetext = NULL;
							pviend();
							return 0;
						}
						iInsertPos--;
					}
					memmove(&control->changetext[iInsertPos], &control->changetext[iEndPos], datasize - iEndPos + 1);
					ptr = guiMemToPtr(control->changetext);
					StringCbCopy(NewText, ARRAYSIZE(NewText), ptr);
					//strcpy(NewText, (CHAR *) control->changetext);
					n1 = 0;
				}
				else if (wParam == ('A' - '@') && ctrlkeydown) {
					/* control-a / select all */
					if (IsWindows7OrGreater()) {
						SendMessage(hwnd, EM_SETSEL, 0, -1);
					}
					else {
						if (control->oldproc != NULL) retcode = CallWindowProc(control->oldproc, hwnd, nMessage, wParam, lParam);
					}
					checkselchange(control);
					n1 = 1;
				}
				else if (wParam == ('C' - '@') && ctrlkeydown) {
					/* control-c / copy */
					guiaPutCBText(&control->changetext[iInsertPos], iEndPos - iInsertPos);
					ptr = guiMemToPtr(control->changetext);
					StringCbCopy(NewText, ARRAYSIZE(NewText), ptr);
					//strcpy(NewText, (CHAR *) control->changetext);
					n1 = 1;
				}
				else if (wParam == ('X' - '@') && ctrlkeydown) {
					/* control-x / cut */
					guiaPutCBText(&control->changetext[iInsertPos], iEndPos - iInsertPos);
					if (iInsertPos != iEndPos) {
						/* remove selected section */
						memmove(&control->changetext[iInsertPos], &control->changetext[iEndPos], datasize - iEndPos + 1);
					}
					ptr = guiMemToPtr(control->changetext);
					StringCbCopy(NewText, ARRAYSIZE(NewText), ptr);
					//strcpy(NewText, (CHAR *) control->changetext);
					n1 = 0;
				}
				else if (wParam == ('V' - '@') && ctrlkeydown) {
					/* control-v / paste */
					size_t n22;
					guiaGetCBTextLen(&n22);
					guiaGetCBText((BYTE *) CBText, &n22);
					if (iInsertPos != iEndPos) {
						/* remove selected section */
						memmove(&control->changetext[iInsertPos], &control->changetext[iEndPos], datasize - iEndPos + 1);
					}
					for (n1 = 0; n1 < n22; n1++) {
						guiaFEditInsertChar(guiMemToPtr(control->changetext),
								guiMemToPtr(control->mask), NewText, CBText[n1], &iInsertPos);
						ptr = guiMemToPtr(control->changetext);
						StringCbCopy(ptr, datasize + 256, NewText);
						//strcpy((CHAR *) control->changetext, NewText);
					}
					n1 = 0;
				}
				else if (wParam >= ' ') {
					if (iInsertPos != iEndPos) {
						/* remove selected section */
						memmove(&control->changetext[iInsertPos], &control->changetext[iEndPos], datasize - iEndPos + 1);
					}
					guiaFEditInsertChar(guiMemToPtr(control->changetext),
							guiMemToPtr(control->mask), NewText, (INT)wParam, &iInsertPos);
					n1 = 0;
				}
				if (!n1) { /* don't do the following if we are just copying the text */
					guiaFEditMaskText(NewText, (CHAR *) control->mask, (CHAR *) control->textval);
					if (strlen(NewText) <= strlen((CHAR *) control->mask)) {
						control->changeflag = TRUE;
						SetWindowText(control->ctlhandle, NewText);
						control->changeflag = FALSE;
						SendMessage(hwnd, EM_SETSEL, iInsertPos, iInsertPos);
					}
				}
				res = *control->res;
				guiaResCallBack(res, control, EN_CHANGE);
				guiFreeMem(control->changetext);
				control->changetext = NULL;
				pviend();
				return 0;
			}
			else return 0;
		}
		else if (ISEDIT(control)) {
			if (ctrlkeydown && wParam == ('A' - '@') &&
					(IsWindows7OrGreater() || ISMLEDITTYPE(control)))
			{
				SendMessage(hwnd, EM_SETSEL, 0, -1);
				checkselchange(control);
				return 0;
			}
			else if (control->oldproc != NULL)
				retcode = CallWindowProc(control->oldproc, hwnd, nMessage, wParam, lParam);
			checkselchange(control);
			return(retcode);
		}
		break;
	case WM_PASTE:
		if (control->type == PANEL_FEDIT && !(control->style & CTRLSTYLE_READONLY)) {
			size_t n22;
			if (wParam < 256) {
				if (control->textval == NULL) {  /* possible if erase happened */
					control->textval = guiAllocMem((INT)strlen((CHAR *) control->mask) + 1);
					if (control->textval == NULL) {
						sprintf(work, "Error in %s, guiAllocMem fail at WM_PASTE", __FUNCTION__);
						guiaErrorMsg(work, (DWORD)strlen((CHAR *) control->mask) + 1);
						return 0;
					}
					control->textval[0] = '\0';
				}
				iLineIndex = (INT)SendMessage(control->ctlhandle, EM_LINEINDEX, 0, 0L);
				datasize = (INT)SendMessage(control->ctlhandle, EM_LINELENGTH, iLineIndex, 0L);
				control->changetext = guiAllocMem(datasize + 256);	/* add 256 bytes for buffer length */
				if (control->changetext == NULL) {
					sprintf(work, "Error in %s, guiAllocMem fail at WM_PASTE", __FUNCTION__);
					guiaErrorMsg(work, datasize + 256);
					return 0;
				}
				*(WORD*) control->changetext = (WORD) datasize + 1;
				SendMessage(control->ctlhandle, EM_GETLINE, 0, (LPARAM) control->changetext);
				control->changetext[datasize] = '\0';
				SendMessage(hwnd, EM_GETSEL, (WPARAM) &iInsertPos, (LPARAM) &iEndPos);
			}
			guiaGetCBTextLen(&n22);
			guiaGetCBText((BYTE *) CBText, &n22);
			if (iInsertPos != iEndPos) {
				/* remove selected section */
				memmove(&control->changetext[iInsertPos], &control->changetext[iEndPos], datasize - iEndPos + 1);
			}
			for (n1 = 0; n1 < n22; n1++) {
				guiaFEditInsertChar((CHAR *) control->changetext, (CHAR *) control->mask,
						NewText, CBText[n1], &iInsertPos);
				ptr = guiMemToPtr(control->changetext);
				StringCbCopy(ptr, datasize + 256, NewText);
				//strcpy((CHAR *) control->changetext, NewText);
			}

			guiaFEditMaskText(NewText, (CHAR *) control->mask, (CHAR *) control->textval);
			if (strlen(NewText) <= strlen((CHAR *) control->mask)) {
				control->changeflag = TRUE;
				SetWindowText(control->ctlhandle, NewText);
				control->changeflag = FALSE;
				SendMessage(hwnd, EM_SETSEL, iInsertPos, iInsertPos);
			}
			pvistart();
			res = *control->res;
			guiaResCallBack(res, control, EN_CHANGE);
			pviend();
			guiFreeMem(control->changetext);
			control->changetext = NULL;
			return 0;
		}
		break;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		switch (wParam) {
		case VK_RETURN:	/* Enter - Default push button */
			if (GetKeyState(VK_CONTROL) & 0x8000) break;
			if (ISMEDIT(control)) {
				PostMessage(control->ctlhandle, WM_CHAR, 0x0a, 0L);
				return 0;
			}
			if (control->type == PANEL_TREE) {
				nmhdr.code = TVN_KEYDOWN;
				nmhdr.hwndFrom = control->ctlhandle;
				nmhdr.idFrom = control->useritem;
				tvkd.wVKey = 0;
				tvkd.flags = 0;
				tvkd.hdr = nmhdr;
				SendMessage((*control->res)->hwnd, WM_NOTIFY, control->useritem, (LPARAM) &tvkd);
				return 0;
			}
			pvistart();
			res = *control->res;
			if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
				win = *res->winshow;
				/*
				 * On 11/6/07 added '&& win->ctldefault->shownflag' to the below IF.
				 * It is something of a bug and it is confusing to activate
				 * a default pushbutton that is not visible.
				 * This was causing problems for Larry Houben at bellsouth.
				 * They use enterkeyold.
				 */
				if (win->ctldefault != NULL && !ISGRAY(win->ctldefault) && win->ctldefault->shownflag) {
					usItem = win->ctldefault->useritem;
					hwndControl = win->ctldefault->ctlhandle;
					if (hwndControl == NULL) {
						/* CODE - find out how win->ctldefault->ctlhandle becomes NULL */
						/* UPDATE - January 27th, 2003 fix should prevent this from happening */
						savepcontrol = (CONTROL *) res->content;
						if (savepcontrol != NULL) {
							for (i1 = res->entrycount; i1 > 0; savepcontrol++) {
								if (savepcontrol->type == PANEL_DEFPUSHBUTTON) {
									hwndControl = win->ctldefault->ctlhandle = savepcontrol->ctlhandle;
									break;
								}
								if (--i1 == 0) break;
							}

						}
					}
					hwndParent = res->hwnd;
					pviend();
					if (hwndControl == NULL) return 0;
					PostMessage(hwndParent, WM_COMMAND,
						MAKEWPARAM(usItem + ITEM_MSG_START, BN_CLICKED),
						(LPARAM)hwndControl
					);
					return 0;
				}
			}
			if (enterkeybehavior == enterKeyBehavior_old) {
				guiaTabToControl(res, control, !(GetKeyState(VK_SHIFT) & 0x8000));
			}
			else {
				itonum5(KEYENTER, cbmsgdata);
				rescb(res, DBC_MSGWINNKEY, control->useritem, 5);
				/**
				 * The next two lines are a small bug fix added 09 April 2012
				 * (Bug pointed out by Ryan at PPro)
				 * We were consuming the enter keystroke by doing a return here.
				 * Let it go through and the dropbox will simply close in response.
				 * This makes it consistent with Java JComboBox and with
				 * other dropboxes seen in other Windows apps.
				 */
				pviend();
				break;
			}
			pviend();
			return 0;

		case VK_ESCAPE:	/* Escape - Escape push button */
			if ((GetKeyState(VK_CONTROL) | GetKeyState(VK_SHIFT)) & 0x8000) break;
			currentreshandle = control->res;
			savepcontrol = control;
			pvistart();
			if ((*currentreshandle)->restype == RES_TYPE_DIALOG) {
				/* escape key pressed in dialog */
				if ((*currentreshandle)->ctlescape != 0) {
					control = (CONTROL *) (*currentreshandle)->content + (*currentreshandle)->ctlescape - 1;
					if (!ISGRAY(control)) {
						usItem = control->useritem;
						hwndControl = control->ctlhandle;
						hwndParent = (*currentreshandle)->hwnd;
						pviend();
						SendMessage(hwndParent, WM_COMMAND, MAKEWPARAM(usItem + ITEM_MSG_START, BN_CLICKED), (LPARAM) hwndControl);
						return 0;
					}
				}
			}
			else {
				/* escape key pressed in window, search all panels for escpushbutton */
				win = *(*currentreshandle)->winshow;
				for (currentreshandle = win->showreshdr; currentreshandle != NULL; currentreshandle = res->showresnext) {
					res = *currentreshandle;
					if (res->restype == RES_TYPE_PANEL && res->ctlescape != 0) {
						control = (CONTROL *) res->content + res->ctlescape - 1;
						if (!ISGRAY(control)) {
							usItem = control->useritem;
							hwndControl = control->ctlhandle;
							hwndParent = res->hwnd;
							pviend();
							SendMessage(hwndParent, WM_COMMAND, MAKEWPARAM(usItem + ITEM_MSG_START, BN_CLICKED), (LPARAM) hwndControl);
							return 0;
						}
					}
				}
			}
			pviend();
			control = savepcontrol;
			break;
		case VK_TAB:
			if (GetKeyState(VK_CONTROL) & 0x8000) break; // We do not process Ctrl+Tab
			pvistart();
			res = *control->res;
			guiaTabToControl(res, control, !(GetKeyState(VK_SHIFT) & 0x8000));
			pviend();
			return 0;
		case VK_LEFT:
		case VK_UP:
			if (ISRADIO(control)) {
				pvistart();
				res = *control->res;
				pctlBase = (CONTROL *) res->content;
				iCtlNum = control->ctloffset;
				do {
					iCtlNum--;
					if (iCtlNum < 0 || !ISRADIO(&pctlBase[iCtlNum])) {
						do {
							iCtlNum++;
						} while (!ISLASTRADIO(&pctlBase[iCtlNum]));
					}
				} while (ISGRAY(&pctlBase[iCtlNum]));
				SetFocus(pctlBase[iCtlNum].ctlhandle);
				pctlBase[iCtlNum].style |= CTRLSTYLE_MARKED;
				SendMessage(pctlBase[iCtlNum].ctlhandle, BM_SETCHECK, BST_CHECKED, 0L);
				iCtlsave = iCtlNum;
				while(iCtlNum < res->entrycount && pctlBase[iCtlNum].type == PANEL_RADIOBUTTON) {
					iCtlNum++;
				}
				do {
					if (iCtlNum != iCtlsave) {
						SendMessage(pctlBase[iCtlNum].ctlhandle, BM_SETCHECK, BST_UNCHECKED, 0L);
						pctlBase[iCtlNum].style &= ~CTRLSTYLE_MARKED;
					}
					iCtlNum--;
				} while (iCtlNum > -1 && pctlBase[iCtlNum].type == PANEL_RADIOBUTTON);
				pviend();
				return 0;
			}
			else if (ISEDIT(control)) {
				if (control->oldproc != NULL) retcode = CallWindowProc(control->oldproc, hwnd, nMessage, wParam, lParam);
				checkselchange(control);
				return(retcode);
			}
			break;
		case VK_RIGHT:
		case VK_DOWN:
			if (ISRADIO(control)) {
				pvistart();
				res = *control->res;
				pctlBase = (CONTROL *) res->content;
				iCtlNum = control->ctloffset;
				do {
					if (ISLASTRADIO(&pctlBase[iCtlNum])) {
						do {
							iCtlNum--;
						} while (iCtlNum >= 0 && pctlBase[iCtlNum].type == PANEL_RADIOBUTTON);
						iCtlNum++;
					}
					else iCtlNum++;
				} while(ISGRAY(&pctlBase[iCtlNum]));
				SetFocus(pctlBase[iCtlNum].ctlhandle);
				pctlBase[iCtlNum].style |= CTRLSTYLE_MARKED;
				SendMessage(pctlBase[iCtlNum].ctlhandle, BM_SETCHECK, BST_CHECKED, 0L);
				iCtlsave = iCtlNum;
				while(iCtlNum < res->entrycount && pctlBase[iCtlNum].type == PANEL_RADIOBUTTON) {
					iCtlNum++;
				}
				do {
					if (iCtlNum != iCtlsave) {
						SendMessage(pctlBase[iCtlNum].ctlhandle, BM_SETCHECK, BST_UNCHECKED, 0L);
						pctlBase[iCtlNum].style &= ~CTRLSTYLE_MARKED;
					}
					iCtlNum--;
				} while (iCtlNum > -1 && pctlBase[iCtlNum].type == PANEL_RADIOBUTTON);
				pviend();
				return 0;
			}
			else if (ISEDIT(control)) {
				if (control->oldproc != NULL) retcode = CallWindowProc(control->oldproc, hwnd, nMessage, wParam, lParam);
				checkselchange(control);
				return(retcode);
			}
			break;
		case VK_DELETE:
			if (control->type == PANEL_FEDIT && !(control->style & CTRLSTYLE_READONLY)) {
				if (control->textval == NULL) {  /* possible if erase happened */
					control->textval = guiAllocMem((INT)strlen((CHAR *) control->mask) + 1);
					if (control->textval == NULL) {
						sprintf(work, "Error in %s, guiAllocMem fail at VK_DELETE", __FUNCTION__);
						guiaErrorMsg(work, (DWORD)strlen((CHAR *) control->mask) + 1);
						return 0;
					}
					control->textval[0] = '\0';
				}
				iLineIndex = (INT)SendMessage(control->ctlhandle, EM_LINEINDEX, 0, 0L);
				datasize = (INT)SendMessage(control->ctlhandle, EM_LINELENGTH, iLineIndex, 0L);
				control->changetext = guiAllocMem(datasize + 2);	/* add two bytes for buffer length */
				if (control->changetext == NULL) {
					sprintf(work, "Error in %s, guiAllocMem fail at VK_DELETE", __FUNCTION__);
					guiaErrorMsg(work, datasize + 2);
					return 0;
				}
				*((WORD *) control->changetext) = (WORD) datasize + 1;
				SendMessage(control->ctlhandle, EM_GETLINE, 0, (LPARAM) control->changetext);
				control->changetext[datasize] = '\0';
				SendMessage(hwnd, EM_GETSEL, (WPARAM) &iInsertPos, (LPARAM) &iEndPos);
				if (iInsertPos == iEndPos) {
					if (iEndPos == datasize) {
						guiFreeMem(control->changetext);
						control->changetext = NULL;
						return 0;
					}
					iEndPos++;
				}
				memmove(&control->changetext[iInsertPos], &control->changetext[iEndPos], datasize - iEndPos + 1);
				StringCbCopy(NewText, ARRAYSIZE(NewText), guiMemToPtr(control->changetext));
				//strcpy(NewText, (CHAR *) control->changetext);
				guiaFEditMaskText(NewText, (CHAR *) control->mask, (CHAR *) control->textval);
				if (strlen(NewText) <= strlen((CHAR *) control->mask)) {
					control->changeflag = TRUE;
					SetWindowText(control->ctlhandle, NewText);
					control->changeflag = FALSE;
					SendMessage(hwnd, EM_SETSEL, iInsertPos, iInsertPos);
				}
				pvistart();
				res = *control->res;
				guiaResCallBack(res, control, EN_CHANGE);
				pviend();
				guiFreeMem(control->changetext);
				control->changetext = NULL;
				return 0;
			}
			if (ISEDIT(control)) {
				if (control->oldproc != NULL) retcode = CallWindowProc(control->oldproc, hwnd, nMessage, wParam, lParam);
				checkselchange(control);
				return(retcode);
			}
			break;
		case VK_END:
		case VK_HOME:
			if (ISEDIT(control)) {
				if (control->oldproc != NULL) retcode = CallWindowProc(control->oldproc, hwnd, nMessage, wParam, lParam);
				checkselchange(control);
				return(retcode);
			}
			break;
		}
		if (wParam >= VK_F1 && wParam <= VK_F12) {
			pvistart();
			res = *control->res;
			itonum5((INT)wParam - VK_F1 + KEYF1, cbmsgdata);
			rescb(res, DBC_MSGWINNKEY, control->useritem, 5);
			pviend();
		}
		else if (wParam == VK_ESCAPE) {
			pvistart();
			res = *control->res;
			if (res->ctlescape == 0) {
				itonum5(KEYESCAPE, cbmsgdata);
				rescb(res, DBC_MSGWINNKEY, 0, 5);
			}
			pviend();
		}
		break;
	}
	if (control->oldproc != NULL) retcode = CallWindowProc(control->oldproc, hwnd, nMessage, wParam, lParam);
	return retcode;
}

/**
 * X and Y coordinates (in lParam) are assumed to be relative to the control.
 * They are converted to be relative to the DB/C client area
 * of the window.
 *
 * Returns 0 if the message should be considered processed.
 * and returns 1 if it should be passed to DefWindowProc.
 */
INT sendRightMouseMessageFromPanel(HWND hwnd, UINT nMessage, CONTROL *control, LPARAM lParam, UINT wParam)
{
	RESOURCE *res;
	WINDOW *win;
	POINT pt;
	INT iXPos, iYPos, i1;

	pvistart();
	res = *control->res;
	if (!res->rclickmsgs) {
		pviend();
		return 1;
	}
	if (res->winshow == NULL) {
		pviend();
		return 0;
	}
	win = *(res->winshow);
	pt.x = GET_X_LPARAM(lParam);
	pt.y = GET_Y_LPARAM(lParam);
	ClientToScreen(hwnd, &pt);
	ScreenToClient(win->hwnd, &pt);
	iXPos = pt.x + win->scrollx;
	iYPos = pt.y + win->scrolly - win->bordertop;
	itonum5(iXPos, cbmsgdata);
	itonum5(iYPos, cbmsgdata + 5);
	if (wParam & MK_CONTROL && wParam & MK_SHIFT) {
		memcpy(cbmsgdata + 10, "CTRLSHIFT", 9);
		i1 = 19;
	}
	else if (wParam & MK_CONTROL) {
		memcpy(cbmsgdata + 10, "CTRL", 4);
		i1 = 14;
	}
	else if (wParam & MK_SHIFT) {
		memcpy(cbmsgdata + 10, "SHIFT", 5);
		i1 = 15;
	}
	else i1 = 10;
	if (nMessage == WM_RBUTTONDOWN) rescb(res, DBC_MSGRGHTMBTNDOWN, control->useritem, i1);
	else if (nMessage == WM_RBUTTONUP) rescb(res, DBC_MSGRGHTMBTNUP, control->useritem, i1);
	else if (nMessage == WM_RBUTTONDBLCLK) rescb(res, DBC_MSGRGHTMBTNDBLCLK, control->useritem, i1);
	pviend();
	if (ISEDIT(control)) {
		return 0; /* prevent edit control pop-up menu */
	}
	return 1;
}

void guiaSetControlTextColor(HDC dc, CONTROL *control, DWORD defaultColor) {
	if (control->textcolor == TEXTCOLOR_CUSTOM) {
		SetTextColor(dc, control->textfgcolor);
	}
	else {
		switch(control->textcolor) {
		case TEXTCOLOR_RED:
			SetTextColor(dc, RGB(255, 0, 0));
			break;
		case TEXTCOLOR_BLUE:
			SetTextColor(dc, RGB(0, 0, 255));
			break;
		case TEXTCOLOR_CYAN:
			SetTextColor(dc, RGB(0, 255, 255));
			break;
		case TEXTCOLOR_WHITE:
			SetTextColor(dc, RGB(255, 255, 255));
			break;
		case TEXTCOLOR_BLACK:
			SetTextColor(dc, RGB(0, 0, 0));
			break;
		case TEXTCOLOR_GREEN:
			SetTextColor(dc, RGB(0, 255, 0));
			break;
		case TEXTCOLOR_YELLOW:
			SetTextColor(dc, RGB(255, 255, 0));
			break;
		case TEXTCOLOR_MAGENTA:
			SetTextColor(dc, RGB(255, 0, 255));
			break;
		default:
			SetTextColor(dc, defaultColor);
			break;
		}
	}
}

/**
 * sub-classed inactive control window procedure
 */
LRESULT CALLBACK guiaControlInactiveProc(HWND hwnd, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	INT iXPos, iYPos, i1, iInsertPos, iEndPos;
	WINHANDLE currentwinhandle;
	WINDOW *win;
	RESOURCE *res;
	CONTROL *control;
	POINT pt;
	LRESULT retcode;

	control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

	if (control == NULL || control->ctlhandle != hwnd) return 0;

	if (WM_GETDLGCODE == nMessage) return(DLGC_WANTALLKEYS);
	switch (nMessage) {
	case WM_SETFOCUS:
		if (ISSHOWONLY(control)) {
			pvistart();
			res = *control->res;
			win = *res->winshow;
			SetFocus(win->hwnd);
			pviend();
			return 0;
		}
		break;
	case WM_SYSCHAR:
	case WM_CHAR:
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		if (wParam == VK_TAB && (control->style & CTRLSTYLE_READONLY)) {
			if (GetKeyState(VK_CONTROL) & 0x8000) break;
			pvistart();
			res = *control->res;
			guiaTabToControl(res, control, !(GetKeyState(VK_SHIFT) & 0x8000));
			pviend();
			return 0;
		}
		if (wParam == VK_END && (control->style & CTRLSTYLE_READONLY)) {
			if (GetKeyState(VK_SHIFT)) {
				if (ISEDIT(control)) {
					if (control->oldproc != NULL) retcode = CallWindowProc(control->oldproc, hwnd, nMessage, wParam, lParam);
					SendMessage(control->ctlhandle, EM_GETSEL, (WPARAM) &iInsertPos, (LPARAM) &iEndPos);
					if (iInsertPos != control->editinsertpos || iEndPos != control->editendpos) {
						i1 = sendESELMessage(control, iInsertPos, iEndPos);
						if (i1 != 0) retcode = i1;
					}
					return(retcode);
				}
			}
		}
		if (wParam >= VK_F1 && wParam <= VK_F12) {
			pvistart();
			res = *control->res;
			itonum5((INT)wParam - VK_F1 + KEYF1, cbmsgdata);
			rescb(res, DBC_MSGWINNKEY, control->useritem, 5);
			pviend();
		}
		else if (wParam == VK_ESCAPE) {
			pvistart();
			res = *control->res;
			if (res->ctlescape == 0) {
				itonum5(KEYESCAPE, cbmsgdata);
				rescb(res, DBC_MSGWINNKEY, 0, 5);
			}
			pviend();
		}
		return 0;
	case WM_MOUSEACTIVATE:
		return(MA_ACTIVATE);

	case WM_MOUSEMOVE:
		if (control->style & CTRLSTYLE_READONLY) break;
		pvistart();
		res = *control->res;
		currentwinhandle = res->winshow;
		if (currentwinhandle == NULL || dlgreshandlelisthdr != NULL) {
			if (!(wParam == MK_LBUTTON && ISNOFOCUS(control))) {
				pviend();
				return 0;
			}
		}
		win = *currentwinhandle;
		if (win->mousemsgs && win->cbfnc != NULL) {	/* are mouse messages turned on? */
			pt.x = (signed short) LOWORD(lParam);
			pt.y = (signed short) HIWORD(lParam);
			ClientToScreen(hwnd, &pt);
			ScreenToClient(win->hwnd, &pt);
			iXPos = pt.x + win->scrollx;
			iYPos = pt.y + win->scrolly - win->bordertop;
			itonum5(iXPos, cbmsgdata);
			itonum5(iYPos, cbmsgdata + 5);
			wincb(win, DBC_MSGPOSITION, 10);
		}
		if (wParam == MK_LBUTTON && ISNOFOCUS(control)) {
			pt.x = (signed short) LOWORD(lParam);
			pt.y = (signed short) HIWORD(lParam);
			iXPos = (control->rect.right - control->rect.left) - 2;
			iYPos = (control->rect.bottom - control->rect.top) - 2;
			if (pt.x <= 1 || pt.y <= 1 || pt.x >= iXPos || pt.y >= iYPos) {
				/* mouse is being dragged near edge, so undo nofocus pushbutton push indication */
				SendMessage(control->ctlhandle, BM_SETSTATE, FALSE, 0L);
				rescb(res, DBC_MSGPUSH, control->useritem, 0);
			}
		}
		pviend();
		return 0;
	case WM_LBUTTONUP:
		if (ISEDIT(control) && (control->style & CTRLSTYLE_READONLY)) {
			if (control->oldproc != NULL) retcode = CallWindowProc(control->oldproc, hwnd, nMessage, wParam, lParam);
			SendMessage(control->ctlhandle, EM_GETSEL, (WPARAM) &iInsertPos, (LPARAM) &iEndPos);
			if (iInsertPos != control->editinsertpos || iEndPos != control->editendpos) {
				i1 = sendESELMessage(control, iInsertPos, iEndPos);
				if (i1 != 0) retcode = i1;
			}
			return(retcode);
		}
		if (!wParam) wParam = MK_LBUTTON; /* Don't want wParam = 0 for following code - SSN */ // @suppress("No break at end of case")
		/* note fall through */
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
		if (control->style & CTRLSTYLE_READONLY) {
			if (!(*control->res)->rclickmsgs || !(nMessage == WM_RBUTTONDBLCLK || nMessage == WM_RBUTTONDOWN || nMessage == WM_RBUTTONUP)) {
				break;
			}
		}
		pvistart();
		res = *control->res;
		currentwinhandle = res->winshow;
		if (currentwinhandle == NULL || dlgreshandlelisthdr != NULL) {
			if (!(wParam == MK_LBUTTON && ISNOFOCUS(control))) {
				pviend();
				return 0;
			}
		}
		if (ISNOFOCUS(control)) {
			if (nMessage == WM_LBUTTONDOWN) {
				SendMessage(control->ctlhandle, BM_SETSTATE, TRUE, 0L);
			}
			else if (nMessage == WM_LBUTTONUP) {
				SendMessage(control->ctlhandle, BM_SETSTATE, FALSE, 0L);
				rescb(res, DBC_MSGPUSH, control->useritem, 0);
			}
			else if (nMessage == WM_LBUTTONDBLCLK) {
				SendMessage(control->ctlhandle, BM_SETSTATE, TRUE, 0L);
				Sleep(80);
				SendMessage(control->ctlhandle, BM_SETSTATE, FALSE, 0L);
			}
			if (!res->rclickmsgs || !(nMessage == WM_RBUTTONDBLCLK || nMessage == WM_RBUTTONDOWN || nMessage == WM_RBUTTONUP)) {
				pviend();
				return 0;
			}
		}
		win = *currentwinhandle;
		pt.x = (signed short) LOWORD(lParam);
		pt.y = (signed short) HIWORD(lParam);
		ClientToScreen(hwnd, &pt);
		ScreenToClient(win->hwnd, &pt);
		iXPos = pt.x + win->scrollx;
		iYPos = pt.y + win->scrolly - win->bordertop;
		if (nMessage == WM_LBUTTONUP || nMessage == WM_LBUTTONDOWN || nMessage == WM_LBUTTONDBLCLK) cbmsgfunc[0] = 'L';
		else if (nMessage == WM_MBUTTONUP || nMessage == WM_MBUTTONDOWN || nMessage == WM_MBUTTONDBLCLK) cbmsgfunc[0] = 'M';
		else cbmsgfunc[0] = 'R';
		cbmsgfunc[1] = 'B';
		cbmsgfunc[2] = 'D';
		if (nMessage == WM_LBUTTONDOWN || nMessage == WM_MBUTTONDOWN || nMessage == WM_RBUTTONDOWN) cbmsgfunc[3] = 'N';
		else if (nMessage == WM_LBUTTONDBLCLK || nMessage == WM_MBUTTONDBLCLK || nMessage == WM_RBUTTONDBLCLK) cbmsgfunc[3] = 'C';
		else {
			cbmsgfunc[2] = 'U';
			cbmsgfunc[3] = 'P';
		}
		itonum5(iXPos, cbmsgdata);
		itonum5(iYPos, cbmsgdata + 5);
		if (wParam & MK_CONTROL) {
			memcpy(cbmsgdata + 10, "CTRL", 4);
			i1 = 14;
		}
		else if (wParam & MK_SHIFT) {
			memcpy(cbmsgdata + 10, "SHIFT", 5);
			i1 = 15;
		}
		else i1 = 10;
		wincb(win, NULL, i1);
		pviend();
		if (control->style & CTRLSTYLE_READONLY) break;
		return 0;
	}
	if (control->oldproc != NULL) retcode = CallWindowProc(control->oldproc, hwnd, nMessage, wParam, lParam);
	return retcode;
}

/**
 * this function handles a WM_DRAWITEM message for a LISTBOX/DROPBOX type controls
 */
INT guiaOwnerDrawListbox(CONTROL *control, const DRAWITEMSTRUCT *pdrawStruct)
{
	INT i1, i2, i3, tabstops[MAXBOXTABS], *tabsptr;
	CHAR *justptr;
	INT nBkMode, nStyle;
	UINT nFormat;
	ZHANDLE sText;
	COLORREF crText, crNorm, crSet;
	HBRUSH brush;
	RECT rect;
	HFONT hfont, oldfont;
	LOGFONT lf;
	HDC pDC;
	CHAR work[128];
	DWORD ecode;

	/**
	 * attr1 is filled from control->itemattributes2.attr1 and has font/style bits in the
	 * upper 8 and foreground color in the lower 24.
	 */
	UINT attr1;

	/**
	 * bgcolor is filled from control->itemattributes2.bgcolor, if it has been specified.
	 */
	COLORREF bgcolor;

	if (pdrawStruct->itemID != (UINT) -1) {
		attr1 = control->itemattributes2[pdrawStruct->itemID].attr1;
		if (control->itemattributes2[pdrawStruct->itemID].userBGColor)
			bgcolor = control->itemattributes2[pdrawStruct->itemID].bgcolor;
		else
			bgcolor = GetSysColor((ISSHOWONLY(control)) ? COLOR_BTNFACE : COLOR_WINDOW);
	}
	else {
		attr1 = (UINT)0;
		bgcolor = GetSysColor((ISSHOWONLY(control)) ? COLOR_BTNFACE : COLOR_WINDOW);
	}
	pDC = pdrawStruct->hDC;
	if (pDC == NULL) return RC_ERROR;
	crSet = GetTextColor(pDC);
	/* Color info is in lower 24 bits of item attribute 1 data */
	crNorm = (COLORREF) (0x00FFFFFF & attr1);

	nBkMode = SetBkMode(pDC, TRANSPARENT);

	if (pdrawStruct->itemState & ODS_SELECTED) {
		brush = CreateSolidBrush(GetSysColor(COLOR_HIGHLIGHT));
		FillRect(pDC, &pdrawStruct->rcItem, brush);
		DeleteObject(brush);
	}
	else {
		if (control->type == PANEL_DROPBOX && pdrawStruct->itemState & ODS_DISABLED) {
			FillRect(pDC, &pdrawStruct->rcItem, hbrButtonFace);
		}
		else {
			brush = CreateSolidBrush(bgcolor);
			FillRect(pDC, &pdrawStruct->rcItem, brush);
			DeleteObject(brush);
		}
	}

	if (pdrawStruct->itemID == (UINT) -1) {
		/* no items */
		if (pdrawStruct->itemState & ODS_FOCUS) {
			SetTextColor(pDC, COLOR_HIGHLIGHT);
			DrawFocusRect(pDC, &pdrawStruct->rcItem);
		}
		SetBkMode(pDC, nBkMode);
		return 0;
	}

	/* If the item's color information is set, use the highlight color
	   gray text color, or normal color for the text.
	*/
	if (pdrawStruct->itemState & ODS_SELECTED) {
		crText = SetTextColor(pDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
	}
	else if (pdrawStruct->itemState & ODS_DISABLED) {
		crText = SetTextColor(pDC, GetSysColor(COLOR_GRAYTEXT));
	}
	else {
		if (crNorm == GetSysColor(COLOR_WINDOWTEXT)) {
			/* use control's overall color instead of individual line color */
			switch(control->textcolor) {
			case TEXTCOLOR_RED:
				crNorm = RGB(255, 0, 0);
				break;
			case TEXTCOLOR_BLUE:
				crNorm = RGB(0, 0, 255);
				break;
			case TEXTCOLOR_CYAN:
				crNorm = RGB(0, 255, 255);
				break;
			case TEXTCOLOR_WHITE:
				crNorm = RGB(255, 255, 255);
				break;
			case TEXTCOLOR_BLACK:
				crNorm = RGB(0, 0, 0);
				break;
			case TEXTCOLOR_GREEN:
				crNorm = RGB(0, 255, 0);
				break;
			case TEXTCOLOR_YELLOW:
				crNorm = RGB(255, 255, 0);
				break;
			case TEXTCOLOR_MAGENTA:
				crNorm = RGB(255, 0, 255);
				break;
			default:
				crNorm = crSet;
				break;
			}
		}
		crText = SetTextColor(pDC, crNorm);
	}

	/*
	 * OS memory must be used for sText because this always runs on the Windows thread
	 */
	sText = NULL;
	if (control->type == PANEL_DROPBOX) {
		/* Get and display item text */
		i1 = (INT)SendMessage(control->ctlhandle, CB_GETLBTEXTLEN, (WPARAM) pdrawStruct->itemID, (LPARAM) 0);
		if (i1 == CB_ERR) {
			ecode = GetLastError();
			if (ecode == 0) {
				// Wait 1/50 of a second and try again. If it fails again, give up.
				Sleep(20);
				i1 = (INT)SendMessage(control->ctlhandle, CB_GETLBTEXTLEN, (WPARAM) pdrawStruct->itemID, (LPARAM) 0);
				if (i1 == CB_ERR) return 0;

			}
			goto CB_GETLBTEXTLEN_ERROR;
		}
		sText = guiAllocMem(i1 + 1);
		if (sText == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "Error in guiaOwnerDrawListbox(A), guiAllocMem fail");
			guiaErrorMsg(work, i1 + 1);
			return 0;
		}
		else {
			SendMessage(control->ctlhandle, CB_GETLBTEXT, (WPARAM) pdrawStruct->itemID, (LPARAM) (LPCSTR) sText);
			sText[i1] = 0;
		}
	}
	else {
		i1 = (INT)SendMessage(control->ctlhandle, LB_GETTEXTLEN, (WPARAM) pdrawStruct->itemID, (LPARAM) 0);
		if (i1 == LB_ERR) {
			sprintf_s(work, ARRAYSIZE(work),
					"Error in guiaOwnerDrawListbox(B), LB_GETTEXTLEN fail (%d)", pdrawStruct->itemID);
			guiaErrorMsg(work, GetLastError());
			return 0;
		}
		sText = guiAllocMem(i1 + 1);
		if (sText == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "Error in guiaOwnerDrawListbox(B), guiAllocMem fail");
			guiaErrorMsg(work, i1 + 1);
			return 0;
		}
		else {
			SendMessage(control->ctlhandle, LB_GETTEXT, (WPARAM) pdrawStruct->itemID, (LPARAM) (LPCSTR) sText);
			sText[i1] = 0;
		}
	}

	GetObject(control->font, sizeof(LOGFONT), (LPVOID) &lf);
	/* Style is in high 8 bits */
	nStyle = (attr1 >> 24) & LISTSTYLE_FONTMASK;
	if (nStyle != LISTSTYLE_UNSPECIFIED) {
		if (nStyle == LISTSTYLE_PLAIN) {
			lf.lfWeight = FW_NORMAL;
			lf.lfItalic = FALSE;
		}
		else {
			if (nStyle & LISTSTYLE_BOLD) lf.lfWeight = FW_BOLD;
			if (nStyle & LISTSTYLE_ITALIC) lf.lfItalic = TRUE;
		}
	}
	hfont = getFont(&lf);
	oldfont = SelectObject(pDC, hfont);

	rect.left = pdrawStruct->rcItem.left + 2;
	rect.top = pdrawStruct->rcItem.top;
	rect.bottom = pdrawStruct->rcItem.bottom;

	/* Setup the text format. Dropboxes need clipping or it can display badly */
	nFormat = DT_EXPANDTABS | DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER;
	if (control->type != PANEL_DROPBOX) nFormat |= DT_NOCLIP;

	if (control->listboxtabs != NULL) {
		nFormat |= DT_END_ELLIPSIS;
		tabsptr = (INT *) control->listboxtabs;
		justptr = (CHAR *) control->listboxjust;
		for (i1 = 0, i2 = (INT)strlen((CHAR *)sText), i3 = 0; i1 < i2; i1++) {
			if (sText[i1] == '\t') {
				tabstops[i3++] = i1;
				sText[i1] = '\0';
			}
		}
		for (i1 = i2 = 0; tabsptr[i1]; i1++) {
			if (i1 <= i3) {
				nFormat &= ~(DT_LEFT | DT_RIGHT | DT_CENTER);
				if (justptr[i1] == 'c') nFormat |= DT_CENTER;
				else if (justptr[i1] == 'r') nFormat |= DT_RIGHT;
				else nFormat |= DT_LEFT;
				rect.right = rect.left + tabsptr[i1] - 1;
				DrawText(pDC, (CHAR *) sText + ((!i1) ? 0 : tabstops[i1 - 1] + 1), -1, &rect, nFormat);
				rect.left += tabsptr[i1];
			}
		}
	}
	else {
		rect.right = pdrawStruct->rcItem.right;
		nFormat |= DT_LEFT;
		DrawText(pDC, (CHAR *) sText, -1, &rect, nFormat);
	}

	if (pdrawStruct->itemState & ODS_FOCUS) {
		SetTextColor(pDC, COLOR_HIGHLIGHT);
		DrawFocusRect(pDC, &pdrawStruct->rcItem);
	}

	SetTextColor(pDC, crText);
	SetBkMode(pDC, nBkMode);
	SelectObject(pDC, oldfont);
	guiFreeMem(sText);
	return 0;

CB_GETLBTEXTLEN_ERROR:
	sprintf_s(work, ARRAYSIZE(work),
			"Error in guiaOwnerDrawListbox(A), CB_GETLBTEXTLEN fail (%d)", pdrawStruct->itemID);
	guiaErrorMsg(work, ecode);
	return 0;

}

/**
 * this function handles a WM_DRAWITEM message for an ICON, VICON,
 * ICON_PUSHBUTTON, or ICON_DEFPUSHBUTTON.
 */
INT guiaOwnerDrawIcon(CONTROL *control, const DRAWITEMSTRUCT *pdrawStruct)
{
	RESOURCE *res;
	HDC hDCIcon;
	HBITMAP hbmOld;
	COLORREF workcolor;
	INT rc;
	RECT rect;
	HBRUSH hbrOld, hbrBrush;
	HPEN hpenOld, hpen;
	INT hsize, vsize, iCurWidth, iCurHeight, iXPos, iYPos, iIconOffset;
	PBITMAPINFO bmiUsed;

	rc = 0;
	rect.left = pdrawStruct->rcItem.left;
	rect.right = pdrawStruct->rcItem.right;
	rect.top = pdrawStruct->rcItem.top;
	rect.bottom = pdrawStruct->rcItem.bottom;
	if (((pdrawStruct->itemAction & ODA_DRAWENTIRE) || (pdrawStruct->itemAction & ODA_SELECT))
		&& control->iconres != NULL)
	{
		pvistart();
		if (!isValidHandle((UCHAR**)control->iconres)) {
			pviend();
			return 0;
		}
		res = *control->iconres;
		hDCIcon = CreateCompatibleDC(pdrawStruct->hDC);
		if (hDCIcon == NULL) {
			pviend();
			return RC_ERROR;
		}

		hbmOld = SelectObject(hDCIcon, res->hbmicon);
		if (hbmOld == NULL) {
			pviend();
			DeleteDC(hDCIcon);
			return RC_ERROR;
		}

		if (PANEL_ICON == control->type || PANEL_VICON == control->type) {
			pviend();
			BitBlt(pdrawStruct->hDC, rect.left, rect.top, rect.right - rect.left,
				rect.bottom - rect.top, hDCIcon, 0, 0, SRCCOPY);
			SelectObject(hDCIcon, hbmOld);
			DeleteDC(hDCIcon);
			return 0;
		}

		if (PANEL_ICONDEFPUSHBUTTON == control->type) {
			FrameRect(pdrawStruct->hDC, &rect, GetStockObject(BLACK_BRUSH));
			rect.left++;
			rect.top++;
			rect.right--;
			rect.bottom--;
			iCurWidth = rect.right - rect.left - 2;
			iCurHeight = rect.bottom - rect.top - 2;
			iIconOffset = 1;
		}
		else {
			iCurWidth = rect.right - rect.left - 3;
			iCurHeight = rect.bottom - rect.top - 3;
			iIconOffset = 2;
		}

		if (res->hsize > iCurWidth - 1) hsize = iCurWidth - 1;
		else hsize = res->hsize;

		if (res->vsize > iCurHeight - 1) vsize = iCurHeight - 1;
		else vsize = res->vsize;

		pviend();
		if (hsize < iCurWidth || vsize < iCurHeight) {
			workcolor = GetSysColor(COLOR_BTNFACE);
			hpen = CreatePen(PS_SOLID, 1, workcolor);
			hpenOld = SelectObject(pdrawStruct->hDC, hpen);
			hbrBrush = CreateSolidBrush(workcolor);
			hbrOld = SelectObject(pdrawStruct->hDC, hbrBrush);
			Rectangle(pdrawStruct->hDC, rect.left + 2, rect.top + 2, iCurWidth + 1, iCurHeight + 1);
			SelectObject(pdrawStruct->hDC, hbrOld);
			DeleteObject(hbrBrush);
			DeleteObject(SelectObject(pdrawStruct->hDC, hpenOld));
			iXPos = rect.left + ((iCurWidth - hsize) >> 1) + iIconOffset;
			iYPos = rect.top + ((iCurHeight - vsize) >> 1) + iIconOffset;
		}
		else {
			iXPos = rect.left + 2;
			iYPos = rect.top + 2;
		}

		/* Draw frame of button, which includes: */
		/* the top and bottom edges have a 1 pixel wide white border */
		/* the left and bottom edges have a 1 pixel wide dark gray border */
		/* the left and bottom edges additionally have another black line of border */
#if 0
		FrameRect(pdrawStruct->hDC, &rect, GetStockObject(BLACK_BRUSH));
#endif
		DrawEdge(pdrawStruct->hDC, &rect, EDGE_RAISED, BF_RECT);

		workcolor = GetSysColor(COLOR_BTNSHADOW);
		hpen = CreatePen(PS_SOLID, 1, workcolor);
		hpenOld = SelectObject(pdrawStruct->hDC, hpen);
		if ((pdrawStruct->itemAction & ODA_SELECT) && (pdrawStruct->itemState & ODS_SELECTED)) {
			MoveToEx(pdrawStruct->hDC, rect.left + 1, rect.bottom - 2, NULL);
			LineTo(pdrawStruct->hDC, rect.left + 1, rect.top + 1);
			LineTo(pdrawStruct->hDC, rect.right - 1, rect.top + 1);
			workcolor = GetSysColor(COLOR_BTNFACE);
			hpen = CreatePen(PS_SOLID, 1, workcolor);
			DeleteObject(SelectObject(pdrawStruct->hDC, hpen));
			MoveToEx(pdrawStruct->hDC, rect.right - 2, rect.top + 2, NULL);
			LineTo(pdrawStruct->hDC, rect.right - 2, rect.bottom - 2);
			LineTo(pdrawStruct->hDC, rect.left + 1, rect.bottom - 2);
		}
		else {
			MoveToEx(pdrawStruct->hDC, rect.right - 2, rect.top + 2, NULL);
			LineTo(pdrawStruct->hDC, rect.right - 2, rect.bottom - 2);
			LineTo(pdrawStruct->hDC, rect.left + 1, rect.bottom - 2);
			workcolor = GetSysColor(COLOR_BTNHIGHLIGHT);
			hpen = CreatePen(PS_SOLID, 1, workcolor);
			DeleteObject(SelectObject(pdrawStruct->hDC, hpen));
			LineTo(pdrawStruct->hDC, rect.left + 1, rect.top + 1);
			LineTo(pdrawStruct->hDC, rect.right - 1, rect.top + 1);
		}
		DeleteObject(SelectObject(pdrawStruct->hDC, hpenOld));

		pvistart();
		if (!isValidHandle((UCHAR**)control->iconres)) {
			pviend();
			SelectObject(hDCIcon, hbmOld);
			DeleteDC(hDCIcon);
			return 0;
		}
		res = *control->iconres;
		bmiUsed = res->bmiColor;
		if (bmiUsed != NULL) {
			if ((bmiUsed->bmiHeader).biBitCount == 24) {
				if (ISGRAY(control)) {
					if (res->hbmgray == NULL) guiaGrayIcon(res, hDCIcon);
					SelectObject(hDCIcon, res->hbmgray);
				}
				if (!BitBlt(pdrawStruct->hDC, iXPos, iYPos, hsize, vsize, hDCIcon, 0, 0, SRCCOPY)) {
					rc = RC_ERROR;
				}
			}
			else {
				if (ISGRAY(control)) {
					if (res->bmiGray == NULL) guiaGrayIcon(res, hDCIcon);
					bmiUsed = res->bmiGray;
				}
				else bmiUsed = res->bmiColor;
				StretchDIBits(pdrawStruct->hDC, iXPos, iYPos, hsize, vsize, 0, 0, hsize, vsize, res->bmiData, bmiUsed, DIB_RGB_COLORS, SRCCOPY);
			}
		}
		SelectObject(hDCIcon, hbmOld);
		DeleteDC(hDCIcon);
		pviend();
	}
	else {
		if (PANEL_ICON == control->type || PANEL_VICON == control->type) return 0;
		iCurWidth = rect.right - rect.left - 3;
		iCurHeight = rect.bottom - rect.top - 3;
	}

	if (pdrawStruct->itemAction & ODA_FOCUS) {
		rect.left += 3;
		rect.right = rect.left + iCurWidth - 3;
		rect.top += 3;
		rect.bottom = rect.top + iCurHeight - 3;
		DrawFocusRect(pdrawStruct->hDC, &rect);
	}
	return(rc);
}

/**
 * create list of selected items in mlistbox
 * The result string is in control->mlbsellist
 */
void guiaMListboxCreateList(RESOURCE *res, CONTROL *control)
{
	INT i1, i2, i3, i4;
	LRESULT selcount = 0;
	CHAR work[32];
	INT *selbuffer;

	/* If the list is empty */
	if (control->value1 < 1) {
		if (control->mlbsellist == NULL) {
			control->mlbsellist = guiAllocMem(4);
			if (control->mlbsellist == NULL) {
				sprintf(work, "guiAllocMem failed in %s", __FUNCTION__);
				guiaErrorMsg(work, 4);
				return;
			}
		}
		control->mlbsellist[0] = '\0';
		return;
	}

	/* allocate a memory area that is maxitems * sizeof(INT) */
	selbuffer = (INT *) guiAllocMem((INT)(control->value1 * sizeof(INT)));
	if (selbuffer == NULL) {
		sprintf(work, "guiAllocMem failed in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}

	if (res->winshow) {
		/* use LB_GETSELITEMS to get an array of selected item numbers,
		 * it returns the selected count.
		 */
		selcount = SendMessage(control->ctlhandle, LB_GETSELITEMS, (WPARAM)control->value1, (LPARAM) selbuffer);
	}
	else {
		for (i2 = 0; i2 < control->value1; i2++) {
			if (control->itemattributes2[i2].attr1 & (LISTSTYLE_SELECTED << 24)) {
				selbuffer[selcount] = i2;
				selcount++;
			}
		}
	}

	if (res->linemsgs) {
		for (i1 = i3 = 0; i1 < selcount; i1++) {
			mscitoa(selbuffer[i1] + 1, work);
			i3 += (INT)strlen(work);
		}
	}
	else {
		for (i1 = i3 = 0; i1 < selcount; i1++) {
			i3 += (INT)strlen((CHAR *) ((BYTE **)control->itemarray)[selbuffer[i1]]);
		}
	}
	i3 += (INT)selcount;		/* for the commas and the trailing null */

	if (i3 == 0) {	/* if no lines are selected */
		if (control->mlbsellist == NULL) {
			control->mlbsellist = guiAllocMem(1);
			if (control->mlbsellist == NULL) {
				sprintf(work, "guiAllocMem failed in %s", __FUNCTION__);
				guiaErrorMsg(work, 1);
				return;
			}
		}
		control->mlbsellist[0] = '\0';
	}
	else {
		if (control->mlbsellist != NULL) {
			control->mlbsellist = guiReallocMem(control->mlbsellist, i3);
			if (control->mlbsellist == NULL) {
				sprintf(work, "guiReallocMem failed in %s", __FUNCTION__);
				guiaErrorMsg(work, i3);
				return;
			}
		}
		else {
			control->mlbsellist = guiAllocMem(i3);
			if (control->mlbsellist == NULL) {
				sprintf(work, "guiAllocMem failed in %s", __FUNCTION__);
				guiaErrorMsg(work, i3);
				return;
			}
		}
		for (i1 = i2 = i3 = i4 = 0; i1 < selcount; i1++) {
			if (i4) control->mlbsellist[i2++] = ',';
			else i4 = TRUE;
			if (res->linemsgs) {
				mscitoa(selbuffer[i1] + 1, work);
				i3 = (INT)strlen(work);
				memcpy(&control->mlbsellist[i2], work, i3);
			}
			else {
				i3 = (INT)strlen((CHAR *) ((BYTE **)control->itemarray)[selbuffer[i1]]);
				memcpy(&control->mlbsellist[i2], ((BYTE **) control->itemarray)[selbuffer[i1]], i3);
			}
			i2 += i3;
		}
		control->mlbsellist[i2] = '\0';
	}
	guiFreeMem((ZHANDLE)selbuffer);
}

/**
 * resource message handler call back
 *
 * When we get around to sending ITEM messages from Tables, this will be the guy to call
 *
 */
static INT guiaResCallBack(RESOURCE *res, CONTROL *control, WORD wCmd)
{
	INT datasize, selected;
	CHAR *function, work[64];

	datasize = 0;
	if (res->cbfnc == NULL) return RC_ERROR;
	switch (control->type) {
	case PANEL_FEDIT:
		if (!res->itemmsgs || wCmd != EN_CHANGE) return RC_ERROR;
		function = DBC_MSGITEM;
		if (control->textval == NULL) datasize = 0;
		else {
			datasize = (INT)strlen((CHAR *) control->textval);
			if (datasize) memcpy(cbmsgdata, control->textval, datasize);
		}
		break;
	case PANEL_EDIT:
	case PANEL_PEDIT:
	case PANEL_MEDIT:
	case PANEL_MEDITHS:
	case PANEL_MEDITVS:
	case PANEL_MEDITS:
	case PANEL_LEDIT:
	case PANEL_MLEDIT:
	case PANEL_MLEDITHS:
	case PANEL_MLEDITVS:
	case PANEL_MLEDITS:
	case PANEL_PLEDIT:
		if (!res->itemmsgs || wCmd != EN_CHANGE) return RC_ERROR;
		function = DBC_MSGITEM;
		datasize = GetWindowText(control->ctlhandle, (CHAR *) cbmsgdata, MAXCBMSGSIZE - 17);
		break;

	case PANEL_MLISTBOXHS:
	case PANEL_MLISTBOX:
		if (wCmd != LBN_DBLCLK && wCmd != LBN_SELCHANGE) return 0;
		if (wCmd == LBN_DBLCLK) {
			function = DBC_MSGPICK;
			selected = (INT)SendMessage(control->ctlhandle, LB_GETCARETINDEX, 0, 0L);
			if (LB_ERR == selected) return RC_ERROR;
			control->value = (USHORT) selected + 1;
			if (res->linemsgs) {
				itonum5(selected + 1, cbmsgdata);
				datasize = 5;
			}
			else {
				datasize = (INT)strlen((CHAR *) ((BYTE **) control->itemarray)[selected]);
				if (control->textval != NULL) guiFreeMem(control->textval);
				if ((control->textval = guiAllocMem(datasize + 1)) == NULL) {
					sprintf(work, "guiAllocMem failed in %s at PANEL_MLISTBOXx", __FUNCTION__);
					guiaErrorMsg(work, datasize + 1);
					return RC_NO_MEM;
				}
				if (datasize) {
					memcpy(control->textval, ((BYTE **) control->itemarray)[selected], datasize);
				}
				control->textval[datasize] = '\0';
				memcpy(cbmsgdata, control->textval, datasize);
			}
		}
		else {
			if (!res->itemmsgs) return RC_ERROR;
			function = DBC_MSGITEM;
			guiaMListboxCreateList(res, control);
			datasize = min((INT)strlen((CHAR *) control->mlbsellist), MAXCBMSGSIZE - 17);
			memcpy(cbmsgdata, control->mlbsellist, datasize);
		}
		break;

	case PANEL_LISTBOXHS:
	case PANEL_LISTBOX:
		if (wCmd != LBN_DBLCLK && wCmd != LBN_SELCHANGE) return 0;
		selected = (INT)SendMessage(control->ctlhandle, LB_GETCARETINDEX, 0, 0L);
		if (LB_ERR == selected) return RC_ERROR;
		{
			/**
			 * Fix a Windows oddity, 20 MAY 2014
			 * If A1 and A2 below differ, then the click was in empty space
			 * in the list box below the last line.
			 * We need to force Windows in this case to 'select' the last line,
			 * otherwise the visuals and the ITEM message make no sense.
			 */
			LRESULT A1 = selected;  // LB_GETCARETINDEX
			LRESULT A2 = SendMessage(control->ctlhandle, LB_GETCURSEL, 0, 0L);
			if (A1 != A2) {
				SendMessage(control->ctlhandle, LB_SETCURSEL, (WPARAM)A1, (LPARAM)0);
			}
		}
		control->value = (USHORT) selected + 1;
		control->textval = ((BYTE **) control->itemarray)[selected];
		if (wCmd == LBN_DBLCLK) function = DBC_MSGPICK;
		else {
			if (!res->itemmsgs) return RC_ERROR;
			function = DBC_MSGITEM;
		}
		if (res->linemsgs) {
			itonum5(selected + 1, cbmsgdata);
			datasize = 5;
		}
		else {
			datasize = min((INT)strlen((CHAR *) control->textval), MAXCBMSGSIZE - 17);
			memcpy(cbmsgdata, control->textval, datasize);
		}
		break;

	case PANEL_DROPBOX:
		if (CBN_SELCHANGE == wCmd) {
			selected = (INT)SendMessage(control->ctlhandle, CB_GETCURSEL, 0, 0L);
			if (selected == CB_ERR)	return (RC_ERROR);
			control->value = (USHORT) selected + 1;
			control->textval = ((BYTE **) control->itemarray)[selected];
			if (!res->itemmsgs) return RC_ERROR;
			function = DBC_MSGITEM;
			if (res->linemsgs) {
				itonum5(selected + 1, cbmsgdata);
				datasize = 5;
			}
			else {
				datasize = (INT)SendMessage(control->ctlhandle, CB_GETLBTEXTLEN, (WPARAM) selected, 0L);
				if (CB_ERR == datasize) return RC_ERROR; /* jpr 10/15/97 */
				SendMessage(control->ctlhandle, CB_GETLBTEXT, (WPARAM) selected, (LPARAM) cbmsgdata);
			}
		}
		else return RC_ERROR;
		break;
	case PANEL_LOCKBUTTON:
	case PANEL_ICONLOCKBUTTON:
		if (control->ctlhandle != NULL) {
/* lock button on panel or dialog must be redrawn by us */
			InvalidateRect(control->ctlhandle, NULL, TRUE);
		} // @suppress("No break at end of case")
		/* fall through */
	case PANEL_LTCHECKBOX:
	case PANEL_CHECKBOX:
	case PANEL_RADIOBUTTON:
	case PANEL_LASTRADIOBUTTON:
	case PANEL_BUTTON:
		if (wCmd != BN_CLICKED) return RC_ERROR;
		function = DBC_MSGITEM;
		datasize = 1;
		cbmsgdata[0] = (ISMARKED(control)) ? 'Y' : 'N';
		if (!res->itemmsgs) return RC_ERROR;
		break;
	case PANEL_PUSHBUTTON:
	case PANEL_DEFPUSHBUTTON:
	case PANEL_ICONPUSHBUTTON:
	case PANEL_ICONDEFPUSHBUTTON:
	case PANEL_POPBOX:
		function = DBC_MSGPUSH;
		break;
	default:
		return RC_ERROR;
	}
	rescb(res, function, control->useritem, datasize);
	return 0;
}

/**
 * return 0 if success, RC_? if failure
 */
static INT setCtlTextBoxSize(RESOURCE *res, CONTROL *control) {
	LPCSTR string;
	INT iCtrlWidth, iCtrlHeight, iXPos, iYPos, i1;
	WINDOW *win;
	RECT rect;

	string = guiLockMem(control->textval);

	if (string[0] == '\0') iCtrlWidth = iCtrlHeight = 0;
	else if ((i1 = guiaGetCtlTextBoxSize(control, (LPSTR) string, &iCtrlWidth, &iCtrlHeight))) {
		control->changeflag = FALSE;
		guiUnlockMem(control->textval);
		return i1;
	}
	if (control->type == PANEL_VTEXT) {
		SetWindowPos(control->ctlhandle, (HWND) NULL, 0, 0, iCtrlWidth, iCtrlHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	}
	else if (control->type == PANEL_CVTEXT) {
		i1 = SetWindowPos(control->ctlhandle, (HWND) NULL, 0, 0, control->rect.right - control->rect.left,
				iCtrlHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		if (!i1) guiaErrorMsg("SetWindowPos failed (CVTEXT)", GetLastError());
	}
	else if (control->type == PANEL_RVTEXT) {
		win = *res->winshow;
		rect = control->rect;
		iXPos = (rect.right + (control->tabgroupxoffset ? control->tabgroupxoffset : res->showoffset.x)) - iCtrlWidth;
		if (RES_TYPE_DIALOG == res->restype) iYPos = rect.top;
		else iYPos = (rect.top + (control->tabgroupyoffset ? control->tabgroupyoffset : res->showoffset.y)) - win->scrolly + win->bordertop;
		i1 = SetWindowPos(control->ctlhandle, (HWND) NULL, iXPos, iYPos, iCtrlWidth, iCtrlHeight, SWP_NOZORDER | SWP_NOACTIVATE);
		if (!i1) guiaErrorMsg("SetWindowPos failed (RVTEXT)", GetLastError());
	}
	guiUnlockMem(control->textval);
	return 0;
}

/**
 * Given control->font and a text string, determines the dimensions of the rectangular
 * area needed to hold the text, returns them in width and height.
 * Does not change anything in the control structure.
 *
 * returns RC_INVALID_VALUE if text, width, or height are null
 *         RC_ERROR if the call to GetTextExtentPoint32 fails
 *         0 if success or if a dc cannot be obtained from the control.
 */
static INT guiaGetCtlTextBoxSize(CONTROL *ctl, CHAR *text, INT *width, INT *height)
{
	int i1, n1;
	SIZE size;
	HWND hwnd;
	HDC hdc;
	HFONT hfont;
	CHAR worktext[256];

	hwnd = ctl->ctlhandle;
	if (text == NULL || width == NULL || height == NULL) return RC_INVALID_VALUE;
	n1 = (int)strlen(text);
	if (n1 < 256) {
		for (i1 = n1 = 0; text[i1]; ) {
			if (text[i1] == '&' && text[i1 + 1]) i1++;
			worktext[n1++] = text[i1++];
		}
		text = &worktext[0];
	}
	hdc = GetDC(hwnd);
	if (hdc == NULL) return 0; /* control was destroyed in another thread */
	hfont = NULL;
	if (ctl->font != NULL) hfont = SelectObject(hdc, ctl->font);
	else if (hfontcontrolfont != NULL) hfont = SelectObject(hdc, hfontcontrolfont);
	if (!GetTextExtentPoint32(hdc, text, n1, &size)) {
		guiaErrorMsg("GetTextExtentPoint32() failure", GetLastError());
		return RC_ERROR;
	}
	if (hfont != NULL) SelectObject(hdc, hfont);
	ReleaseDC(hwnd, hdc);
	*width = (INT) size.cx + 2;
	*height = (INT) size.cy + 4;
	return 0;
}

/**
 * create and display a window
 * hpos and vpos are one-based
 */
INT guiaWinCreate(WINHANDLE wh, INT style, INT hpos, INT vpos, INT clienthsize, INT clientvsize)
{
	GUIAWINCREATESTRUCT wcsCreateStruct;

	if (clienthsize < 0 || clientvsize < 0 || wh == NULL || *wh == NULL) {
		guiaErrorMsg("Invalid parameters passed to guiaWinCreate()", GetLastError());
		return(RC_INVALID_VALUE);
	}
	wcsCreateStruct.wh = wh;
	wcsCreateStruct.style = style;
	wcsCreateStruct.hpos = hpos;
	wcsCreateStruct.vpos = vpos;
	wcsCreateStruct.clienthsize = clienthsize;
	wcsCreateStruct.clientvsize = clientvsize;
	return (INT)SendMessage(hwndmainwin, GUIAM_WINCREATE, (WPARAM) 0, (LPARAM) &wcsCreateStruct);
}

/**
 * destroy an Application device
 * All we really need to do here is destroy
 * any icons that were assigned by a
 * desktopicon change.
 */
INT guiaAppDestroy()
{
//	HICON hsicon, hbicon;
//	hbicon = (HICON)SendMessage(hwndmainwin, WM_SETICON, (WPARAM) ICON_BIG, (LPARAM)NULL);
//	hsicon = (HICON)SendMessage(hwndmainwin, WM_SETICON, (WPARAM) ICON_SMALL, (LPARAM)NULL);
//	if (hsicon != NULL) DestroyIcon(hsicon);
//	if (hbicon != NULL) DestroyIcon(hbicon);
	return 0;
}


/**
 * destroy a window
 */
INT guiaWinDestroy(WINHANDLE wh)
{
	INT lr1;
	WINDOW *win;
	HWND /*hwndtaskbarbutton,*/ hWindow;
	HICON hsicon, hbicon;

	pvistart();
	win = *wh;
	if (win == NULL || win->hwnd == NULL) {
		pviend();
		guiaErrorMsg("Invalid parameters passed to guiaWinDestroy()", 0);
		return RC_INVALID_VALUE;
	}
	//hwndtaskbarbutton = win->hwndtaskbarbutton;
	hsicon = win->hsicon;
	hbicon = win->hbicon;
	//win->hwndtaskbarbutton = NULL;
	hWindow = win->hwnd;
	win->hwnd = NULL;
	win->hwndtt = NULL;
	pviend();
//	if (hwndtaskbarbutton != NULL) {
//		SendMessage(hwndmainwin, GUIAM_WINDESTROY, (WPARAM) 0, (LPARAM) hwndtaskbarbutton);
//	}
	if (hsicon != NULL) DestroyIcon(hsicon);
	if (hbicon != NULL) DestroyIcon(hbicon);

	lr1 = (INT)SendMessage(hwndmainwin, GUIAM_WINDESTROY, (WPARAM) 0, (LPARAM) hWindow);
	return lr1;
}

/**
 * change the title of a given window
 */
INT guiaChangeWinTitle(WINDOW *win)
{
	if (win == NULL || win->hwnd == NULL) {
		guiaErrorMsg("Invalid parameters passed to guiaChangeWinTitle()", GetLastError());
		return(RC_INVALID_VALUE);
	}
	if (!SetWindowText(win->hwnd, (CHAR *) win->title)) {
		guiaErrorMsg("Change window text error", GetLastError());
		return RC_ERROR;
	}
	return 0;
}

/**
 * change the given Window's location on the screen
 */
INT guiaChangeWinPosition(WINDOW *win, INT hpos, INT vpos)
{
	if (!SetWindowPos(win->hwnd, NULL, hpos, vpos, 0, 0,
		SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)) return RC_ERROR;
	return 0;
}

INT guiaChangeWinMaximize(WINDOW *win) {
	return ShowWindow(win->hwnd, SW_MAXIMIZE);
}

void guiaChangeWinMinimize(WINDOW *win) {
	RESHANDLE currentreshandle;
	currentreshandle = dlgreshandlelisthdr;
	while (currentreshandle != NULL) {
		ShowWindow((*currentreshandle)->hwnd, SW_MINIMIZE);
		currentreshandle = (*currentreshandle)->nextshowndlg;
	}
	//return ShowWindow(win->hwnd, SW_MINIMIZE);
	win->changeflag = TRUE;
	SendMessage(win->hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
	win->changeflag = FALSE;
	return;
}

void guiaChangeWinRestore(WINDOW *win) {
	RESHANDLE currentreshandle = dlgreshandlelisthdr;
	while (currentreshandle != NULL) {
		ShowWindow((*currentreshandle)->hwnd, SW_RESTORE);
		currentreshandle = (*currentreshandle)->nextshowndlg;
	}
	//ShowWindow(win->hwnd, SW_RESTORE);
	win->changeflag = TRUE;
	SendMessage(win->hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
	win->changeflag = FALSE;
	return;
}

/**
 * change the given Window's dimensions
 * Called only from guiwinchange in gui.c
 * Always runs on the DX main thread.
 */
INT guiaChangeWinSize(WINDOW *win, INT hsize, INT vsize)
{
	RECT rectClient, rectWin;
	INT iExtraWidth, iExtraHeight;
	USHORT scrollflg;
	LONG_PTR winStyle;
	UINT swpFlags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE;

	/* Scrollbars screw up window size calculation, so remove them */
	/* NOTE: scroll bars will get added back with WM_SIZE message from SetWindowPos() */
	scrollflg = win->autoscroll;
	win->autoscroll = FALSE;
	ShowScrollBar(win->hwnd, SB_BOTH, FALSE);
	win->hsbflg = win->vsbflg = FALSE;
	win->autoscroll = scrollflg;

	GetClientRect(win->hwnd, &rectClient);
	GetWindowRect(win->hwnd, &rectWin);
	/*
	 * iExtraWidth is the horizontal width in pixels of the vertical borders on the window
	 */
	iExtraWidth = rectWin.right - rectWin.left - rectClient.right;
	iExtraHeight = (rectWin.bottom - rectWin.top - rectClient.bottom) + win->bordertop + win->borderbtm;

	/*
	 * If it was set, we must clear the maximize style bit.
	 */
	winStyle = GetWindowLongPtr(win->hwnd, GWL_STYLE);
	if (winStyle & WS_MAXIMIZE) {
		winStyle &= ~WS_MAXIMIZE;
		SetWindowLongPtr(win->hwnd, GWL_STYLE, winStyle);
		swpFlags |= SWP_FRAMECHANGED;
	}

	/*
	 * The SetWindowPos function is setting the overall size of the window, including borders, title bar,
	 * menu, toolbar, statusbar, et al.
	 * So we take the numbers from the user, which are the desired client dimensions, and add in all those other things.
	 */
	if (!SetWindowPos(win->hwnd, NULL, 0, 0, hsize + iExtraWidth, vsize + iExtraHeight, swpFlags)) {
		return RC_ERROR;
	}

	/*
	 * Windows has a bit of efficiency that can mess us up. If the size requested is identical to the current size,
	 * then Windows does not send a WM_SIZE message and we never put back the scroll bars.
	 * So we check for that condition and fake our own message.
	 */
	if (hsize + iExtraWidth == rectWin.right - rectWin.left && vsize + iExtraHeight == rectWin.bottom - rectWin.top) {
		PostMessage(win->hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(hsize + iExtraWidth, vsize + iExtraHeight));
	}
	return 0;
}

/*
 * Create a handle to a win32 icon with transparency (HICON), given a DB/C icon resource
 */
static HICON createiconfromres(RESOURCE *res, INT large, DWORD *error) {
	ZHANDLE pImageStorage = NULL;
	INT i1, i2, i3, i4, i5;
	HICON hsicon;
	ICONINFO iconinfo;
	HBITMAP maskbm, iconbm, hbmOldSource, hbmOldDest;
	HDC sourceDC, destDC, tmpDC;
	CHAR work[64];

	if (res->restype != RES_TYPE_ICON) {
		*error = RC_INVALID_PARM;
		return NULL; /* sanity check */
	}
	pImageStorage = guiAllocMem(!large ? 32 : 128);
	if (pImageStorage == NULL) {
		sprintf_s(work, ARRAYSIZE(work), "guiAllocMem failed in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		*error = RC_NO_MEM;
		return NULL;
	}
	guiLockMem(pImageStorage);

	/* Create two ICON's
	 * 1 Is a black and white mask with value 1 for every transparent pixel
	 * 2 Is a copy of the image where transparent pixels are set to black for the mask
	 */
	sourceDC = CreateCompatibleDC(NULL);
	if (sourceDC == NULL) goto errorexit;
	destDC = CreateCompatibleDC(NULL);
	if (destDC == NULL) goto errorexit;
	tmpDC = GetDC(NULL);
	if (tmpDC == NULL) goto errorexit;
	iconbm = CreateCompatibleBitmap(tmpDC, res->hsize, res->vsize);
	if (iconbm == NULL) goto errorexit;
	hbmOldSource = (HBITMAP) SelectObject(sourceDC, res->hbmicon);
	hbmOldDest = (HBITMAP) SelectObject(destDC, iconbm);
	for (i2 = i4 = i5 = 0, i3 = 7; i2 < res->vsize; i2++) {
		for (i1 = 0; i1 < res->hsize; i1++, i5++) {
			if (i3 == 7) pImageStorage[i4] = 0;
			if (res->content != NULL && res->content[i5] == 'T') {
				pImageStorage[i4] |= 1 << i3;
				SetPixel(destDC, i1, i2, 0);
			}
			else {
				SetPixel(destDC, i1, i2, GetPixel(sourceDC, i1, i2));
			}
			if (--i3 < 0 && (i3 = 7) /* assignment is intentional */ ) {
				i4++;
			}
			if (res->bpp > 4) i5 += 5;
		}
	}
	maskbm = CreateBitmap((!large ? 16 : 32), (!large ? 16 : 32), 1, 1, pImageStorage);
	if (maskbm == NULL) goto errorexit;
	iconinfo.fIcon = TRUE;
	iconinfo.hbmMask = maskbm;
	iconinfo.hbmColor = iconbm;

	/**
	 * MS suggests that the bitmaps for hbmMask and hbmColor NOT be
	 * selected into any other DC when calling CreateIconIndirect. So free up iconbm.
	 */
	SelectObject(destDC, hbmOldDest);
	hsicon = CreateIconIndirect(&iconinfo);
	if (hsicon == NULL) goto errorexit;

	/* Clean up */
	guiUnlockMem(pImageStorage);
	guiFreeMem(pImageStorage);
	SelectObject(sourceDC, hbmOldSource);
	DeleteObject(iconbm);
	DeleteObject(maskbm);
	DeleteObject(hbmOldSource);
	DeleteObject(hbmOldDest);
	DeleteDC(destDC);
	DeleteDC(sourceDC);
	ReleaseDC(NULL, tmpDC);
	return hsicon;

errorexit:
	*error = GetLastError();
	if (pImageStorage != NULL) guiFreeMem(pImageStorage);
	if (sourceDC != NULL) {
		if (hbmOldSource != NULL) {
			SelectObject(sourceDC, hbmOldSource);
			DeleteObject(hbmOldSource);
		}
		DeleteDC(sourceDC);
	}
	if (destDC != NULL) {
		if (hbmOldDest != NULL) {
			SelectObject(destDC, hbmOldDest);
			DeleteObject(hbmOldDest);
		}
		DeleteDC(destDC);
	}
	if (iconbm != NULL) DeleteObject(iconbm);
	if (maskbm != NULL) DeleteObject(maskbm);
	if (tmpDC != NULL) ReleaseDC(NULL, tmpDC);
	return NULL;
}

/*
 * change the icon of a window
 */
INT guiaChangeWinIcon(WINDOW *win, RESOURCE *res)
{
	HICON hsicon;
	INT large;
	DWORD error;

	if (res->hsize == 16 && res->vsize == 16) large = FALSE;
	else if (res->hsize == 32 && res->vsize == 32) large = TRUE;
	else return(RC_INVALID_VALUE);

	hsicon = createiconfromres(res, large, &error);
	if (hsicon == NULL) {
		guiaErrorMsg("Change window icon error occured", error);
		return RC_ERROR;
	}
	if (large) {
		if (win->hbicon != NULL) DestroyIcon(win->hbicon);
		SendMessage(win->hwnd, WM_SETICON, (WPARAM) ICON_BIG, (LPARAM) hsicon);
		win->hbicon = hsicon;
	}
	else {
		if (win->hsicon != NULL) DestroyIcon(win->hsicon);
		SendMessage(win->hwnd, WM_SETICON, (WPARAM) ICON_SMALL, (LPARAM) hsicon);
		win->hsicon = hsicon;
	}
	return 0;
}

/*
 * change the icon of windows or of dialogs
 *
 * This is done by a desktopicon/dialogicon change operation on the 'application' device.
 */
INT guiaChangeAppIcon(RESOURCE *res, CHAR *cmd)
{
	INT large;
	HICON hicon, ohicon;
	DWORD error;
	HWND dummyDialog;

	if (res->hsize == 16 && res->vsize == 16) large = FALSE;
	else if (res->hsize == 32 && res->vsize == 32) large = TRUE;
	else return(RC_INVALID_VALUE);
	hicon = createiconfromres(res, large, &error);
	if (hicon == NULL) {
		guiaErrorMsg("Change window icon error occured", error);
		return RC_ERROR;
	}

	if (!strcmp(cmd, "desktopicon")) {
		if (large) {
			ohicon = (HICON)SendMessage(hwndmainwin, WM_SETICON, (WPARAM) ICON_BIG, (LPARAM) hicon);
			if (ohicon != NULL) DestroyIcon(ohicon);
			SetClassLongPtr(hwndmainwin, GCLP_HICON, (LONG_PTR)hicon);
		}
		else {
			ohicon = (HICON)SendMessage(hwndmainwin, WM_SETICON, (WPARAM) ICON_SMALL, (LPARAM) hicon);
			if (ohicon != NULL) DestroyIcon(ohicon);
			SetClassLongPtr(hwndmainwin, GCLP_HICONSM, (LONG_PTR)hicon);
		}
	}

	else if (!strcmp(cmd, "dialogicon")) {
		/*
		 * Doing the same for dialogs is a bit trickier.
		 * The problem is getting a hold of the dialog class so we can use SetClassLongPtr.
		 * Win32 does not have a function for this, so we create a temporary invisible dialog.
		 */
		dummyDialog = CreateWindow(MAKEINTATOM(aDialogClass), NULL, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				CW_USEDEFAULT, NULL, NULL, hinstancemain, NULL);
		if (large) SetClassLongPtr(dummyDialog, GCLP_HICON, (LONG_PTR)hicon);
		else SetClassLongPtr(dummyDialog, GCLP_HICONSM, (LONG_PTR)hicon);
		DestroyWindow(dummyDialog);
	}
	return 0;
}

/*
 * change the title of the main application window
 */
INT guiaChangeAppWinTitle(CHAR *title)
{
	if (!SetWindowText(hwndmainwin, title)) {
		guiaErrorMsg("Change application window text error", GetLastError());
		return RC_ERROR;
	}
	return 0;
}

/*
 * change the main window so that a taskbar button no longer appears
 *
 * This is no longer applicable as the db/c main window no longer shows up on the taskbar at all
 * as of version 16.1
 */
//INT guiaChangeAppWinRemoveTaskbarButton()
//{
//	ShowWindow(hwndmainwin, SW_HIDE);
//	return 0;
//}

/*
 * Try to locate the console window and activate it
 */
INT guiaChangeConsoleWinFocus()
{
	HWND hwndFound;
	char pszNewWindowTitle[1024]; /* Contains fabricated WindowTitle. */
	char pszOldWindowTitle[1024]; /* Contains original WindowTitle. */
	GetConsoleTitle(pszOldWindowTitle, 1024);
	wsprintf(pszNewWindowTitle, "%d/%d", GetTickCount(), GetCurrentProcessId()); /* Format a "unique" NewWindowTitle. */
	SetConsoleTitle(pszNewWindowTitle);
	Sleep(40); /* Ensure window title has been updated. */
	hwndFound = FindWindow(NULL, pszNewWindowTitle); /* Look for NewWindowTitle.*/
	SetConsoleTitle(pszOldWindowTitle); /* Restore original window title. */
	if (hwndFound == NULL) return RC_ERROR;
	SetForegroundWindow(hwndFound);
	return 0;
}

/*
 * cause the window to be repainted
 */
#if 0
INT guiaWinInvalidate(WINDOW *win)
{
	if (win == NULL || win->hwnd == NULL) {
		guiaErrorMsg("Invalid parameters passed to guiaWinInvalidate()", GetLastError());
		return(RC_INVALID_VALUE);
	}
	InvalidateRect(win->hwnd, NULL, FALSE);
	return 0;
}
#endif

INT guiaGetWinText(HWND hwnd, CHAR *title, INT textlen)
{
	GUIAGETWINTEXT gwtGetWinTextStruct;

	gwtGetWinTextStruct.hwnd = hwnd;
	gwtGetWinTextStruct.title = title;
	gwtGetWinTextStruct.textlen = textlen;

	SendMessage(hwndmainwin, GUIAM_GETWINDOWTEXT, (WPARAM) 0, (LPARAM) &gwtGetWinTextStruct);
	return(gwtGetWinTextStruct.textlen);
}

/*
 * give this window the focus
 */
INT guiaSetActiveWin(WINDOW *win)
{
	if (win == NULL || win->hwnd == NULL) {
		guiaErrorMsg("Invalid parameters passed to guiaSetActiveWin()", GetLastError());
		return(RC_INVALID_VALUE);
	}
	return((INT)SendMessage(hwndmainwin, GUIAM_WINACTIVATE, (WPARAM) 0, (LPARAM) win));
}

/*
 *  show status bar
 */
INT guiaShowStatusbar(WINDOW *win)
{
	if (win == NULL) {
		guiaErrorMsg("Invalid window device passed to guiaShowStatusbar", GetLastError());
		return(RC_INVALID_VALUE);
	}
	return((INT)SendMessage(hwndmainwin, GUIAM_STATUSBARCREATE, (WPARAM) 0, (LPARAM) win));
}

/*
 * hide status bar
 */
INT guiaHideStatusbar(WINDOW *win)
{
	if (win == NULL || !win->statusbarshown || win->hwndstatusbar == NULL) {
		guiaErrorMsg("Invalid window device passed to guiaHideStatusbar", GetLastError());
		return(RC_INVALID_VALUE);
	}
	return((INT)SendMessage(hwndmainwin, GUIAM_STATUSBARDESTROY, (WPARAM) 0, (LPARAM) win));
}

/*
 * show a menu
 *
 * Will return 0 or RC_ERROR or RC_INVALID_VALUE
 */
INT guiaShowMenu(RESHANDLE reshandle)
{
	if (reshandle == NULL || ((RESOURCE*)*reshandle)->winshow == NULL || ((RESOURCE*)*reshandle)->entrycount < 0) {
		guiaErrorMsg("Invalid menu resource passed to guiaShowMenu", GetLastError());
		return(RC_INVALID_VALUE);
	}
	return((INT)SendMessage(hwndmainwin, GUIAM_MENUCREATE, (WPARAM) 0, (LPARAM) reshandle));
}

/*
 * Show and hide a popup menu
 *
 * Will return 0 or RC_ERROR or RC_INVALID_VALUE
 */
INT guiaShowPopUpMenu(RESHANDLE rh, INT h, INT v)
{
	RESOURCE *res;
	res = *rh;
	if (res == NULL || res->winshow == NULL || res->entrycount < 0) {
		guiaErrorMsg("Invalid menu resource passed to guiaShowMenu", GetLastError());
		return(RC_INVALID_VALUE);
	}
	popupmenuh = h;
	popupmenuv = v;
	return((INT)SendMessage(hwndmainwin, GUIAM_POPUPMENUSHOW, (WPARAM) 0, (LPARAM) rh));
}

/*
 * hide the menu
 */
INT guiaHideMenu(RESOURCE *res)
{
	if (res == NULL || res->winshow == NULL || res->entrycount < 0) {
		guiaErrorMsg("Invalid menu resource passed to guiaHideMenu", GetLastError());
		return(RC_INVALID_VALUE);
	}
	return((INT)SendMessage(hwndmainwin, GUIAM_MENUDESTROY, (WPARAM) 0, (LPARAM) res));
}

/*
 * change a menu item
 */
INT guiaChangeMenu(RESOURCE *res, MENUENTRY *pMenuItem, INT iCmd)
{
	INT menutxtlen, iCurMenuPos;
	WINDOW *win;
	CHAR *MenuBuffer;
	MENUENTRY *pCurMenuItem;
	MENUENTRY *pMenuItemIdx;
	INT curentry, begin, i1;
	MENUITEMINFO mii;
	CHAR work[64];
	ZHANDLE zhMB = NULL;

	if (res == NULL || res->winshow == NULL) {
		sprintf_s(work, ARRAYSIZE(work), "Invalid menu resource passed to %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return RC_INVALID_VALUE;
	}

/* adjust for menu items that have been hidden */
	iCurMenuPos = pMenuItem->count;
	pCurMenuItem = pMenuItem;
	while (iCurMenuPos) {
		pCurMenuItem--;
		if (pCurMenuItem->level != pMenuItem->level) continue;
		if (pCurMenuItem->style & MENUSTYLE_HIDDEN) iCurMenuPos--;
		if (!pCurMenuItem->count) break;
	}

	mii.cbSize = sizeof(MENUITEMINFO);
	switch (iCmd) {
	case GUI_AVAILABLE:
		EnableMenuItem(pMenuItem->menuhandle, iCurMenuPos, MF_BYPOSITION | MF_ENABLED);
		break;
	case GUI_GRAY:
		EnableMenuItem(pMenuItem->menuhandle, iCurMenuPos, MF_BYPOSITION | MF_GRAYED);
		break;
	case GUI_MARK:
		mii.fMask = MIIM_STATE;
		mii.fState = MFS_CHECKED;
		SetMenuItemInfo(pMenuItem->menuhandle, iCurMenuPos, TRUE, &mii);
		break;
	case GUI_UNMARK:
		mii.fMask = MIIM_STATE;
		mii.fState = MFS_UNCHECKED;
		SetMenuItemInfo(pMenuItem->menuhandle, iCurMenuPos, TRUE, &mii);
		break;
	case GUI_TEXT:
		mii.fMask = MIIM_TYPE;
		mii.fType = MFT_STRING;
		if (pMenuItem->speedkey) {
			/* add speed keys to menu item text */
			menutxtlen = (INT)strlen((CHAR *) pMenuItem->text);
			/* 15 is added here for accelerator key text like CTRL-A .. CTRL-Z, F1 .. F10, PgUp, Insert, etc. */
			zhMB = guiAllocMem(menutxtlen + 15 * sizeof(CHAR));
			if (zhMB == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocMem failed in %s at GUI_TEXT", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
			MenuBuffer = (CHAR *) guiLockMem(zhMB);
			strcpy(MenuBuffer, pMenuItem->text);
			MenuBuffer[menutxtlen] = '\t';
			i1 = speedKeyText(pMenuItem, MenuBuffer, menutxtlen);
			if (i1) {
				guiUnlockMem(zhMB);
				guiFreeMem(zhMB);
				return i1;
			}
			mii.dwTypeData = MenuBuffer;
		}
		else {
			mii.dwTypeData = (CHAR*) pMenuItem->text;
		}
		SetMenuItemInfo(pMenuItem->menuhandle, iCurMenuPos, TRUE, &mii);
		if (zhMB != NULL) {
			guiUnlockMem(zhMB);
			guiFreeMem(zhMB);
		}
		break;

	case GUI_HIDE:
		SendMessage(hwndmainwin, GUIAM_DELETEMENUITEM, (WPARAM) iCurMenuPos, (LPARAM) pMenuItem->menuhandle);
		break;

	case GUI_SHOW:
		pMenuItemIdx = pMenuItem;
		pMenuItemIdx++;
		if (pMenuItemIdx->level > pMenuItem->level) {
			pMenuItem->submenu = CreateMenu();
			SendMessage(hwndmainwin, GUIAM_ADDMENUITEM, (WPARAM) iCurMenuPos, (LPARAM) pMenuItem);
		}
		else {
			SendMessage(hwndmainwin, GUIAM_ADDMENUITEM, (WPARAM) iCurMenuPos, (LPARAM) pMenuItem);
		}
		pMenuItemIdx = (MENUENTRY *) res->content;
		begin = FALSE;
		for (curentry = 0; curentry < res->entrycount; curentry++) {
			if (begin) {
				if (pMenuItemIdx[curentry].level == pMenuItem->level + 1) {
					pMenuItemIdx[curentry].menuhandle = pMenuItem->submenu;
					guiaChangeMenu(res, &pMenuItemIdx[curentry], GUI_SHOW);
				}
				else if (pMenuItemIdx[curentry].level <= pMenuItem->level) {
				 	begin = FALSE;
					break;
				}
			}
			if (pMenuItemIdx[curentry].useritem == pMenuItem->useritem) begin = TRUE;
		}
		break;
	default:
		return RC_INVALID_VALUE;
	}
	if (!pMenuItem->level) {
		win = *res->winshow;
		DrawMenuBar(win->hwnd);
	}
	return 0;
}

/*
 * show and hide the open directory dialog
 *
 * Notes:
 * res->content is used for the 'start directory'. This is the 'default' keyword in the prep string.
 * And it is lost in the process of showing this dialog. It is overidden by the return value from the dialog.
 *
 * res->deviceFilters, if in use, specifies a CSV of strings for the dialog to use to filter the display.
 * It too is lost by this process.
 *
 * So while it may be possible to prep one of these and show it repeatedly, the default and devicefilter info will be lost
 * after the 1st use.
 *
 * Returns 0 or RC_NO_MEM
 */
INT guiaShowOpenDirDlg(RESOURCE *res)
{
	DIRDLGINFO dirinfo;
	WINDOW *win;
	CHAR data[MAX_PATH], work[64];
	INT i1;

	memset(data, '\0', sizeof(CHAR) * MAX_PATH);
	memset(&dirinfo, 0, sizeof(dirinfo));
	win = *res->winshow;
	dirinfo.dirstruct.hwndOwner = win->hwnd;
	dirinfo.dirstruct.pidlRoot = NULL;
	dirinfo.dirstruct.pszDisplayName = data;
	if (res->title != NULL) dirinfo.dirstruct.lpszTitle = (LPCSTR) guiLockMem(res->title);
	else dirinfo.dirstruct.lpszTitle = (LPCSTR) NULL;
	dirinfo.dirstruct.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;
	if (res->content != NULL) dirinfo.pszStartDir = (LPSTR) guiLockMem(res->content);
	else dirinfo.pszStartDir = (LPSTR) NULL;
	if (res->deviceFilters != NULL) dirinfo.deviceFilters = (LPCSTR) guiLockMem(res->deviceFilters);
	else dirinfo.deviceFilters = (LPCSTR) NULL;
	commonDlgFlag = TRUE;
	SendMessage(hwndmainwin, GUIAM_DIRDLG, 0, (LPARAM) &dirinfo);
	pvistart(); /* hack - because SendMessage could return too soon - SSN */
	pviend();
	commonDlgFlag = FALSE;
	if (res->deviceFilters != NULL) {
		guiUnlockMem(res->deviceFilters);
		guiFreeMem(res->deviceFilters);
		res->deviceFilters = NULL;
	}
	if (res->content != NULL) {
		guiUnlockMem(res->content);
	}
	if (res->title != NULL) {
		guiUnlockMem(res->title);
	}
	if (dirinfo.retcode) {
		i1 = (INT)strlen(data);
		strcpy((CHAR *) cbmsgdata, data);
		if (res->content == NULL) {
			res->content = guiAllocMem(i1 + 1);
			if (res->content == NULL) {
				sprintf(work, "guiAllocMem returned Null in %s", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
		}
		else {
			res->content = guiReallocMem(res->content, i1 + 1);
			if (res->content == NULL) {
				sprintf(work, "guiReallocMem returned Null in %s", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
		}
		strcpy((CHAR *) guiMemToPtr(res->content), data);
		rescb(res, DBC_MSGOK, 0, i1);
	}
	else {
		if (res->content != NULL) {
			guiUnlockMem(res->content);
			guiFreeMem(res->content);
			res->content = NULL;
		}
		rescb(res, DBC_MSGCANCEL, 0, 0);
	}
	return 0;
}

/*
 * show and hide the open file dialog
 */
INT guiaShowOpenFileDlg(RESOURCE *res)
{
	FILEDLGINFO fileinfo;
	WINDOW *win;
	INT i1;
	CHAR *p1, *p2;
	CHAR *data, work[64];

	data = (CHAR *) guiAllocMem(MAXCbMSGFILE);
	if (data == NULL) {
		sprintf(work, "guiAllocMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return RC_NO_MEM;
	}
	memset(data, '\0', MAXCbMSGFILE);
	win = *res->winshow;
	memset(&fileinfo.ofnstruct, 0, sizeof(fileinfo.ofnstruct));
	fileinfo.ofnstruct.hwndOwner = win->hwnd;
	/* The extra zero at the end of the filter string is necessary */
	if (res->namefilter == NULL)
		fileinfo.ofnstruct.lpstrFilter = (LPCSTR) "All Files\0*.*\0";
	else {
		fileinfo.ofnstruct.lpstrFilter = (LPCSTR) guiLockMem(res->namefilter);
	}
	fileinfo.ofnstruct.lpstrCustomFilter = NULL;
	fileinfo.ofnstruct.lpstrFile = data;
	fileinfo.ofnstruct.nMaxFile = MAXCbMSGFILE;
	//fileinfo.ofnstruct.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_ENABLEHOOK | OFN_EXPLORER;
	//
	// Microsoft doc says that the OFN_NOCHANGEDIR flag has no effect on a call to GetOpenFileName.
	// But that does not agree with what Ryan is finding.
	// So keep it.
	// And why should we not allow resizing? (did the same for savefiledialog below)
	//
	// jpr 19 FEB 2013
	//
	fileinfo.ofnstruct.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_ENABLEHOOK | OFN_EXPLORER
			| OFN_ENABLESIZING;
#if DX_MAJOR_VERSION >= 17
	if (res->openFileDlgMultiSelect) fileinfo.ofnstruct.Flags |= OFN_ALLOWMULTISELECT;
#endif
	fileinfo.ofnstruct.lpstrDefExt = (LPCSTR) "txt";
	fileinfo.ofnstruct.lpfnHook = guiaCommonDlgProcOne;
	fileinfo.ofnstruct.lStructSize = sizeof(OPENFILENAME);
	if (res->content != NULL) {
		p2 = (CHAR *) res->content + strlen((CHAR *) res->content);
		while (p2 != (CHAR *) res->content && *p2 != '\\' && *p2 != '/' && *p2 != ':') p2--;
		if (p2 != (CHAR *) res->content) p2++;
		p1 = (CHAR *) data;
		StringCbCopy(p1, MAXCbMSGFILE, p2);
		//strcpy(p1, p2);
		if (p2 != (CHAR *) res->content) {
			ptrdiff_t pdiff;
			p1 += strlen(p1) + 1;
			pdiff = p2 - (CHAR *) res->content;
			memcpy((CHAR *) p1, res->content, pdiff);
			p1[pdiff] = '\0';
			fileinfo.ofnstruct.lpstrInitialDir = p1;
		}
	}
	else data[0] = '\0';
	commonDlgFlag = TRUE;
	SendMessage(hwndmainwin, GUIAM_OPENDLG, 0, (LPARAM) &fileinfo);
	pvistart(); /* hack - because SendMessage could return too soon - SSN */
	pviend();
	commonDlgFlag = FALSE;
	if (res->namefilter != NULL) {
		guiUnlockMem(res->namefilter);
		guiFreeMem(res->namefilter);
		res->namefilter = NULL;
	}
	if (fileinfo.retcode) {

		//TODO For Dx 17, Deal with NULL separated list from a multi-select
		/**
		 * If the OFN_ALLOWMULTISELECT flag is set and the user selects multiple files,
		 * the buffer contains the current directory followed by the file names of
		 * the selected files. For Explorer-style dialog boxes, the directory and file
		 * name strings are NULL separated, with an extra NULL character after the last file name
		 */

		// User specified a file name and clicked the OK button
		i1 = (INT)strlen(data);
		StringCbCopy((LPSTR)cbmsgdata, MAXCBMSGSIZE - 17, data);
		//strcpy((CHAR *) cbmsgdata, data);
		if (res->content == NULL) {
			res->content = guiAllocMem(i1 + 1);
			if (res->content == NULL) {
				sprintf(work, "guiAllocMem returned Null in %s", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
		}
		else {
			res->content = guiReallocMem(res->content, i1 + 1);
			if (res->content == NULL) {
				sprintf(work, "guiReallocMem returned Null in %s", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
		}
		StringCbCopy(guiMemToPtr(res->content), i1 + 1, data);
		//strcpy((CHAR *) res->content, data);
		rescb(res, DBC_MSGOK, 0, i1);
	}
	else {
		if (res->content != NULL) {
			guiFreeMem(res->content);
			res->content = NULL;
		}
		rescb(res, DBC_MSGCANCEL, 0, 0);
		guiFreeMem((ZHANDLE) data);
		return(fileinfo.extcode);
	}
	if (res->namefilter != NULL) {
		guiUnlockMem(res->namefilter);
		guiFreeMem(res->namefilter);
	}
	guiFreeMem((ZHANDLE) data);
	return 0;
}

/*
 * show and hide the file save as file dialog
 */
INT guiaShowSaveAsFileDlg(RESOURCE *res)
{
	FILEDLGINFO fileinfo;
	WINDOW *win;
	INT i1, i2;
	CHAR *data, *p1, *ptr, work[64];

	data = (CHAR *) guiAllocMem(MAXCbMSGFILE);
	if (data == NULL) {
		sprintf(work, "guiAllocMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return RC_NO_MEM;
	}
	memset(data, '\0', MAXCbMSGFILE);
	win = *res->winshow;
	memset(&fileinfo.ofnstruct, 0, sizeof(fileinfo.ofnstruct));
	fileinfo.ofnstruct.hwndOwner = win->hwnd;
	/* The extra zero at the end of the filter string is necessary */
	if (res->namefilter == NULL)
		fileinfo.ofnstruct.lpstrFilter = (LPCSTR) "All Files\0*.*\0";
	else {
		fileinfo.ofnstruct.lpstrFilter = (LPCSTR) guiLockMem(res->namefilter);
	}
	fileinfo.ofnstruct.lpstrFile = data;
	fileinfo.ofnstruct.nMaxFile = MAXCbMSGFILE;
	fileinfo.ofnstruct.Flags = OFN_NOCHANGEDIR | OFN_ENABLEHOOK | OFN_EXPLORER | OFN_HIDEREADONLY
			| OFN_ENABLESIZING;
	fileinfo.ofnstruct.lpstrDefExt = (LPCSTR) "txt";
	fileinfo.ofnstruct.lpfnHook = guiaCommonDlgProcOne;
	fileinfo.ofnstruct.lStructSize = sizeof(OPENFILENAME);
	if (res->content != NULL) {
		ptr = (CHAR *)(res->content + strlen((CHAR *) res->content));
		while (ptr != (CHAR *) res->content && *ptr != '\\' && *ptr != '/' && *ptr != ':') ptr--;
		if (ptr != (CHAR *) res->content) ptr++;
		p1 = data;
		strcpy(p1, ptr);
		if (ptr != (CHAR *) res->content) {
			ptrdiff_t pdiff;
			p1 += strlen(p1) + 1;
			pdiff = ptr - (CHAR *) res->content;
			memcpy(p1, res->content, pdiff);
			p1[pdiff] = '\0';
			fileinfo.ofnstruct.lpstrInitialDir = p1;
		}
	}
	else data[0] = '\0';
	commonDlgFlag = TRUE;
	SendMessage(hwndmainwin, GUIAM_SAVEDLG, 0, (LPARAM) &fileinfo);
	pvistart(); /* hack - because SendMessage could return too soon - SSN */
	pviend();
	commonDlgFlag = FALSE;
	if (res->namefilter != NULL) {
		guiUnlockMem(res->namefilter);
		guiFreeMem(res->namefilter);
		res->namefilter = NULL;
	}
	if (fileinfo.retcode) {
		i1 = (INT)strlen(data);
/*** CODE: THIS FIXES BUG UNDER WIN95 OF ALWAYS ADDING DEF EXTENSION TO NAME ***/
		ptr = data;
		strcpy((CHAR *) &cbmsg[17], data);
		if (i1 > 4 && !strcmp(&ptr[i1 - 4], ".txt")) {
			for (i2 = i1 - 5; i2 >= 0 && ptr[i2] != '.' && ptr[i2] != '\\' && ptr[i2] != '/' && ptr[i2] != ':'; i2--);
			if (i2 >= 0 && ptr[i2] == '.') {
				i1 -= 4;
				ptr[i1] = '\0';
			}
		}
		if (res->content == NULL) {
			res->content = guiAllocMem(i1 + 1);
			if (res->content == NULL) {
				sprintf(work, "guiAllocMem returned Null in %s", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
		}
		else {
			res->content = guiReallocMem(res->content, i1 + 1);
			if (res->content == NULL) {
				sprintf(work, "guiReallocMem returned Null in %s", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
		}
		rescb(res, DBC_MSGOK, 0, i1);
	}
	else {
		if (res->content != NULL) {
			guiFreeMem(res->content);
			res->content = NULL;
		}
		rescb(res, DBC_MSGCANCEL, 0, 0);
		guiFreeMem((ZHANDLE) data);
		return(fileinfo.extcode);
	}
	guiFreeMem((ZHANDLE) data);
	return 0;
}


/*
 * show and hide the font picker dialog
 */
INT guiaShowFontDlg(RESOURCE *res)
{
	WINDOW *win;
	INT i1;
	CHAR *p1;
	CHOOSEFONT choosefont;
	LOGFONT logfont;
	HDC hDC;
	INT rc;

	win = *res->winshow;
	choosefont.lStructSize = sizeof(CHOOSEFONT);
	choosefont.hwndOwner = win->hwnd;
	choosefont.hDC = NULL;
	choosefont.lpLogFont = &logfont;
	choosefont.iPointSize = 0;
	choosefont.Flags = CF_SCREENFONTS | CF_ENABLEHOOK;
	choosefont.lpfnHook = guiaCommonDlgProcTwo;

	if (res->content != NULL) {
		memset(&logfont, 0, sizeof(logfont));
		p1 = (CHAR *) res->content;
		while (*p1 == ' ') p1++;
		StringCbCopy((LPSTR)cbmsgdata, MAXCBMSGSIZE - 17, p1);
		//strcpy((CHAR *) cbmsgdata, p1);
		if (cbmsgdata[0]) {
			for (p1 = (CHAR *) cbmsgdata; *p1 && *p1 != '('; p1++);
			if (*p1 != '(') {
				StringCbCopy(logfont.lfFaceName, ARRAYSIZE(logfont.lfFaceName), (LPCSTR)cbmsgdata);
				//strcpy(logfont.lfFaceName, (CHAR *) cbmsgdata);
			}
			else {
				*p1++ = '\0';
				StringCbCopy(logfont.lfFaceName, ARRAYSIZE(logfont.lfFaceName), (LPCSTR)cbmsgdata);
				//strcpy(logfont.lfFaceName, (CHAR *) cbmsgdata);
				while (*p1 != ')') {
					while (*p1 == ',' || *p1 == ' ') p1++;
					if (!guiamemicmp((BYTE *) p1, (BYTE *) "BOLD", 4)) {
						logfont.lfWeight = FW_BOLD;
						p1 += 4;
					}
					else if (!guiamemicmp((BYTE *) p1, (BYTE *) "ITALIC", 6)) {
						logfont.lfItalic = TRUE;
						p1 += 6;
					}
					else if (!guiamemicmp((BYTE *) p1, (BYTE *) "UNDERLINE", 9)) {
						logfont.lfUnderline = TRUE;
						p1 += 9;
					}
					else if (isdigit(*p1)) {
						i1 = *p1++ - '0';
						if (isdigit(*p1)) i1 = i1 * 10 + *p1++ - '0';
						hDC = GetDC(choosefont.hwndOwner);
						logfont.lfHeight = -MulDiv(i1, GetDeviceCaps(hDC, LOGPIXELSY), 72);
						ReleaseDC(choosefont.hwndOwner, hDC);
					}
					if (*p1 != ',' && *p1 != ')' && *p1 != ' ') goto nofontinit;
				}
			}
			choosefont.Flags += CF_INITTOLOGFONTSTRUCT;
		}
	}
nofontinit:
	commonDlgFlag = TRUE;
	if (SendMessage(hwndmainwin, GUIAM_CHOOSEFONT, 0, (LPARAM) &choosefont)) {
		commonDlgFlag = FALSE;
		rc = 0;
		p1 = (CHAR *) cbmsgdata;
		strcpy(p1, logfont.lfFaceName);
		p1 += strlen(p1);
		*p1++ = '(';
		if (choosefont.iPointSize > 99) *p1++ = (choosefont.iPointSize / 100) + '0';
		*p1++ = (choosefont.iPointSize % 100) / 10 + '0';
		if (logfont.lfWeight >= FW_BOLD) {
			strcpy(p1, ", BOLD");
			p1 += 6;
		}
		if (logfont.lfItalic) {
			strcpy(p1, ", ITALIC");
			p1 += 8;
		}
		if (logfont.lfUnderline) {
			strcpy(p1, ", UNDERLINE");
			p1 += 11;
		}
		*p1++ = ')';
		*p1 = 0;
		rescb(res, DBC_MSGOK, 0, (INT)strlen((CHAR *) cbmsgdata));
	}
	else {
		commonDlgFlag = FALSE;
		rc = CommDlgExtendedError();
		rescb(res, DBC_MSGCANCEL, 0, 0);
	}
	return(rc);
}

INT guiaShowAlertDlg(RESOURCE *res)
{
	WINDOW *win;
	INT rc;
	win = *res->winshow;

	if (!MessageBox(win->hwnd, res->content, " ", MB_OK)) {
		rc = GetLastError();
	}
	else {
		rc = 0;
		rescb(res, DBC_MSGOK, 0, 15);
	}
	return rc;
}

/*
 * show and hide the color picker dialog
 */
INT guiaShowColorDlg(RESOURCE *res)
{
	static COLORREF customcolors[16];
	CHOOSECOLOR choosecolor;
	WINDOW *win;
	INT rc;

	win = *res->winshow;
	choosecolor.hwndOwner = win->hwnd;
	choosecolor.hInstance = 0;
	choosecolor.rgbResult = res->color;
	choosecolor.lpCustColors = customcolors;
	choosecolor.Flags = CC_RGBINIT | CC_ENABLEHOOK;
	choosecolor.lpfnHook = guiaCommonDlgProcTwo;
	choosecolor.lStructSize = sizeof(CHOOSECOLOR);
#if 0
	if (ChooseColor(&choosecolor)) {
#endif
	commonDlgFlag = TRUE;
	if (SendMessage(hwndmainwin, GUIAM_CHOOSECOLOR, 0, (LPARAM) &choosecolor)) {
		commonDlgFlag = FALSE;
		rc = 0;
		res->color = choosecolor.rgbResult;
		itonum5(GetRValue(choosecolor.rgbResult), cbmsgdata);
		itonum5(GetGValue(choosecolor.rgbResult), cbmsgdata + 5);
		itonum5(GetBValue(choosecolor.rgbResult), cbmsgdata + 10);
		rescb(res, DBC_MSGOK, 0, 15);
	}
	else {
		commonDlgFlag = FALSE;
		rc = CommDlgExtendedError();
		rescb(res, DBC_MSGCANCEL, 0, 15);
	}
	return(rc);
}

/*
 * change text in a status bar
 */
INT guiaChangeStatusbar(WINDOW *win)
{
	if (win == NULL || !win->statusbarshown || win->hwndstatusbar == NULL) {
		guiaErrorMsg("Invalid window device passed to guiaChangeStatusbar", GetLastError());
		return(RC_INVALID_VALUE);
	}
	return((INT)SendMessage(hwndmainwin, GUIAM_STATUSBARTEXT, (WPARAM) 0, (LPARAM) win));
}

/*
 * show tool bar
 *
 * Returns 0 or RC_ERROR or RC_INVALID_VALUE
 */
INT guiaShowToolbar(RESOURCE *res)
{
	if (res == NULL || res->winshow == NULL || res->entrycount < 0) {
		guiaErrorMsg("Invalid tool bar resource passed to guiaShowToolbar", GetLastError());
		return(RC_INVALID_VALUE);
	}

	return((INT)SendMessage(hwndmainwin, GUIAM_TOOLBARCREATE, (WPARAM) 0, (LPARAM) res));
}

/*
 * hide tool bar
 *
 * It is safe for the function over in gui.c to pass us a dereferenced memalloc pointer.
 * A memory compaction can only happen on the main thread and we are on it here.
 * The SendMessage will block this thread until the Windows thread carries out the 'hide' process.
 *
 * Returns 0 or RC_ERROR or RC_INVALID_VALUE
 */
INT guiaHideToolbar(RESOURCE *res)
{
	if (res == NULL || res->winshow == NULL || res->hwndtoolbar == NULL) {
		guiaErrorMsg("Invalid tool bar resource passed to guiaHideToolbar", GetLastError());
		return(RC_INVALID_VALUE);
	}

	return((INT)SendMessage(hwndmainwin, GUIAM_TOOLBARDESTROY, (WPARAM) 0, (LPARAM) res));
}

/*
 * delete a button or combo box from a tool bar
 */
INT guiaDeleteToolbarControl(RESOURCE *res, CONTROL *control, INT deletepos)
{
	INT i1;
	CONTROL *workcontrol;
	RECT rect;
	if (res->winshow != NULL){
		if (control->ctlhandle != NULL) {
			/* item is combo box, remove it */
			SendMessage(control->ctlhandle, WM_CLOSE, 0, 0);
		}
		if (!SendMessage(res->hwndtoolbar, TB_DELETEBUTTON, deletepos, (LPARAM) 0)) {
			guiaErrorMsg("Unable to delete control from toolbar", GetLastError());
			return RC_ERROR;
		}
		workcontrol = (CONTROL *) res->content;
		for (i1 = 0; i1 < res->entrycount; i1++, workcontrol++) {
			if (i1 <= deletepos) continue;
			if (workcontrol->ctlhandle != NULL) {
				/* find new position of separator so we can move combo to the correct place */
				if (!SendMessage(res->hwndtoolbar, TB_GETRECT, workcontrol->useritem + ITEM_MSG_START, (LPARAM) &rect)) {
					guiaErrorMsg("Unable to delete dropbox from toolbar", GetLastError());
					return RC_ERROR;
				}
				/* move combo box to position of separator */
				if (!SetWindowPos(workcontrol->ctlhandle, (HWND) NULL, rect.left, rect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)) {
					guiaErrorMsg("Unable to delete dropbox from toolbar", GetLastError());
					return RC_ERROR;
				}
			}
		}
	}
	return 0;
}

/*
 * Insert a dropbox item into a toolbar
 *
 * control->rect.right and control->rect.bottom (width and height of dropbox)
 * are setup prior to this call in module gui.c.
 */
static LONG toolbarcreatedbox(GUIATOOLBARDROPBOX *info) {
	TBBUTTONINFO tbbuttoninfo; // @suppress("Symbol is not resolved")
	CONTROL *control;
	RECT rect;
	INT i1;

	control = info->control;

	/* change width of separator to width of combo box */
	tbbuttoninfo.cbSize = sizeof(TBBUTTONINFO);
	tbbuttoninfo.dwMask = TBIF_SIZE; // @suppress("Symbol is not resolved")
	tbbuttoninfo.cx = (WORD) control->rect.right;
	if (!SendMessage(info->res->hwndtoolbar, TB_SETBUTTONINFO, info->idCommand, (LPARAM) &tbbuttoninfo)) { // @suppress("Symbol is not resolved")
		guiaErrorMsg("Unable to add dropbox to toolbar", GetLastError());
		return RC_ERROR;
	}
	/* find position of separator so we can place combo in same place */
	if (!SendMessage(info->res->hwndtoolbar, TB_GETRECT, info->idCommand, (LPARAM) &rect)) {
		guiaErrorMsg("Unable to add dropbox to toolbar", GetLastError());
		return RC_ERROR;
	}
	/* calculate center, 16 = dropbox height, 6 = padding * 2 */
	i1 = (HIWORD(SendMessage(info->res->hwndtoolbar, TB_GETBUTTONSIZE, 0, 0)) - (16 + 6)) >> 1;
	control->ctlhandle = CreateWindowEx(0, WC_COMBOBOX, NULL,
		WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_HASSTRINGS | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED,
		rect.left, rect.top + i1, control->rect.right, control->rect.bottom, info->res->hwndtoolbar,
		(HMENU) info->idCommand, hinstancemain, NULL
	);
	/* set height so that control lines up with icon height */
	SendMessage(control->ctlhandle, CB_SETITEMHEIGHT, (WPARAM) -1, (LPARAM) 16);
	/* initialize import things */
	//control->insthandle = hinstancemain;
	control->shownflag = TRUE;
	guiaSetCtlFont(control);
	if (control->font != NULL) {
		SendMessage(control->ctlhandle, WM_SETFONT, (WPARAM) control->font, MAKELONG((DWORD) TRUE, 0));
	}
	//-----
	// Bug fix. Caching the CONTROL in the widget is no good for toolbar items.
	// A realloc may happen if a control is added or removed and the location of the
	// CONTROL may move, making this pointer invalid.
	//SetWindowLongPtr(control->ctlhandle, GWLP_USERDATA, (LONG) control);
	//-----

	for (i1 = 0; i1 < control->value1; i1++) {
		if (SendMessage(control->ctlhandle, CB_INSERTSTRING, (WPARAM) i1, (LPARAM) ((BYTE **) control->itemarray)[i1]) == CB_ERR) {
			return RC_ERROR;
		}
	}
	if (control->value) SendMessage(control->ctlhandle, CB_SETCURSEL, (WPARAM) (control->value - 1), 0L);
	if (ISGRAY(control)) {
		EnableWindow(control->ctlhandle, FALSE);
		InvalidateRect(control->ctlhandle, NULL, TRUE);
	}
	return 0;
}

/*
 * change a tool bar resource
 */
INT guiaChangeToolbar(RESOURCE *res, CONTROL *control, INT iCmd)
{
	CONTROL *pctrlInsertPos;
	RESOURCE *resicon;
	GUIATOOLBARDROPBOX tbdropbox;
	RECT rect;
	INT iCurPos;

	if (res == NULL) {
		guiaErrorMsg("Invalid resource passed to guiaChangeToolbar", GetLastError());
		return(RC_INVALID_VALUE);
	}
	control->changeflag = FALSE;
	switch(iCmd) {
	case GUI_MARK:
		if (res->winshow == NULL) return 0;
		control->changeflag = TRUE;
		SendMessage(res->hwndtoolbar, TB_CHECKBUTTON, (WPARAM) (control->useritem + ITEM_MSG_START), MAKELPARAM(TRUE, 0));
		control->changeflag = FALSE;
		break;
	case GUI_UNMARK:
		if (res->winshow == NULL) return 0;
		control->changeflag = TRUE;
		SendMessage(res->hwndtoolbar, TB_CHECKBUTTON, (WPARAM) (control->useritem + ITEM_MSG_START), MAKELPARAM(FALSE, 0));
		control->changeflag = FALSE;
		break;
	case GUI_AVAILABLE:
		if (res->winshow == NULL) return 0;
		control->changeflag = TRUE;
		if (control->type == TOOLBAR_DROPBOX) {
			EnableWindow(control->ctlhandle, TRUE);
			InvalidateRect(control->ctlhandle, NULL, TRUE);
		}
		else {
			SendMessage(res->hwndtoolbar, TB_ENABLEBUTTON, control->useritem + ITEM_MSG_START, (LPARAM) MAKELONG(TRUE, 0));
		}
		control->changeflag = FALSE;
		break;
	case GUI_GRAY:
		if (res->winshow == NULL) return 0;
		control->changeflag = TRUE;
		if (control->type == TOOLBAR_DROPBOX) {
			EnableWindow(control->ctlhandle, FALSE);
			InvalidateRect(control->ctlhandle, NULL, TRUE);
		}
		else {
			SendMessage(res->hwndtoolbar, TB_ENABLEBUTTON, control->useritem + ITEM_MSG_START, (LPARAM) MAKELONG(FALSE, 0));
		}
		control->changeflag = FALSE;
		break;
	case GUI_HELPTEXT:
	case GUI_TEXT:
		if (res->winshow == NULL) return 0;
		control->changeflag = TRUE;
		settoolbartext((CONTROL *) control);
		control->changeflag = FALSE;
		break;
	case GUI_ICON:
		if (res->winshow == NULL) return 0;
		resicon = *control->iconres;
		tbbmaddbitmap.hInst = NULL;
		tbbmaddbitmap.nID = (UINT_PTR) resicon->hbmicon;
		tbbutton.iBitmap = LOWORD(SendMessage(res->hwndtoolbar, TB_ADDBITMAP, 1, (LPARAM) &tbbmaddbitmap));
		if (tbbutton.iBitmap < 0) {
			guiaErrorMsg("Unable to add bitmap to tool bar button", GetLastError());
			return RC_ERROR;
		}
		if (SendMessage(res->hwndtoolbar, TB_CHANGEBITMAP,
					 (WPARAM) (control->useritem + ITEM_MSG_START),
					 MAKELPARAM(tbbutton.iBitmap, 0)) < 0) {
			guiaErrorMsg("Unable to change bitmap for tool bar button", GetLastError());
			return RC_ERROR;
		}
		break;
	case GUI_ADDPUSHBUTTON:
	case GUI_ADDLOCKBUTTON:
		if (res->winshow == NULL) return 0;
		resicon = *control->iconres;
		tbbmaddbitmap.hInst = NULL;
		tbbmaddbitmap.nID = (UINT_PTR) resicon->hbmicon;
		tbbutton.iBitmap = LOWORD(SendMessage(res->hwndtoolbar, TB_ADDBITMAP, 1, (LPARAM) &tbbmaddbitmap));
		if (tbbutton.iBitmap < 0) {
			guiaErrorMsg("Unable to add bitmap to tool bar button", GetLastError());
			return RC_ERROR;
		} // @suppress("No break at end of case")
/* note drop through */
	case GUI_ADDDROPBOX:
	case GUI_ADDSPACE:
		if (res->winshow == NULL) return 0;
		tbbutton.dwData = 0;
		tbbutton.iString = 0;
		if (GUI_ADDSPACE == iCmd || GUI_ADDDROPBOX == iCmd) {
			tbbutton.fsStyle = TBSTYLE_SEP;
			tbbutton.fsState = 0;
			tbbutton.iBitmap = 0;
			if (GUI_ADDSPACE == iCmd) {
				tbbutton.idCommand = 0;
			}
			else {
				tbdropbox.res = res;
				tbdropbox.control = control;
				tbdropbox.idCommand = tbbutton.idCommand = control->useritem + ITEM_MSG_START;
			}
		}
		else {
			tbbutton.fsStyle = TBSTYLE_BUTTON;
			tbbutton.fsState = TBSTATE_ENABLED;
			tbbutton.idCommand = control->useritem + ITEM_MSG_START;
		}
		if (GUI_ADDLOCKBUTTON == iCmd) tbbutton.fsStyle |= TBSTYLE_CHECK;
		if (!res->entrycount || (!res->insertpos && res->appendflag)) {
			/* first time adding a button, or appending to end */
			if (!SendMessage(res->hwndtoolbar, TB_ADDBUTTONS, 1, (LPARAM) &tbbutton)) { // @suppress("Symbol is not resolved")
				guiaErrorMsg("Unable to add button to toolbar", GetLastError());
				return RC_ERROR;
			}
			if (GUI_ADDDROPBOX == iCmd) {
				SendMessage(hwndmainwin, GUIAM_TOOLBARCREATEDBOX, 0, (LPARAM) &tbdropbox);
			}
			return 0;
		}
		pctrlInsertPos = (CONTROL *) res->content;
		for (iCurPos = 0; pctrlInsertPos != control; pctrlInsertPos++) {
			iCurPos++;
		}
		if (res->appendflag && res->entrycount < iCurPos) {
			if (!SendMessage(res->hwndtoolbar, TB_ADDBUTTONS, 1, (LPARAM) &tbbutton)) { // @suppress("Symbol is not resolved")
				guiaErrorMsg("Unable to add button to toolbar", GetLastError());
				return RC_ERROR;
			}
			if (GUI_ADDDROPBOX == iCmd) {
				SendMessage(hwndmainwin, GUIAM_TOOLBARCREATEDBOX, 0, (LPARAM) &tbdropbox);
			}
			return 0;
		}
		if (!SendMessage(res->hwndtoolbar, TB_INSERTBUTTON, iCurPos, (LPARAM) &tbbutton)) { // @suppress("Symbol is not resolved")
			guiaErrorMsg("Unable to insert button into toolbar", GetLastError());
			return RC_ERROR;
		}
		if (GUI_ADDDROPBOX == iCmd) {
			SendMessage(hwndmainwin, GUIAM_TOOLBARCREATEDBOX, 0, (LPARAM) &tbdropbox);
		}
		/* adjust position of combo boxes positioned after added entry */
		for (pctrlInsertPos++; iCurPos < res->entrycount; iCurPos++, pctrlInsertPos++) {
			if (pctrlInsertPos->ctlhandle != NULL) {
				/* find new position of separator so we can move combo to the correct place */
				if (!SendMessage(res->hwndtoolbar, TB_GETRECT, pctrlInsertPos->useritem + ITEM_MSG_START, (LPARAM) &rect)) {
					guiaErrorMsg("Unable to delete dropbox from toolbar", GetLastError());
					return RC_ERROR;
				}
				/* move combo box to position of separator */
				if (!SetWindowPos(pctrlInsertPos->ctlhandle, (HWND) NULL, rect.left, rect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)) {
					guiaErrorMsg("Unable to delete dropbox from toolbar", GetLastError());
					return RC_ERROR;
				}
			}
		}
		InvalidateRect(res->hwndtoolbar, NULL, TRUE);
		break;

	/**
	 * The following are standard DROPBOX operations
	 */

	case GUI_INSERT:
	case GUI_SELECT:
	case GUI_LOCATE:
	case GUI_DELETE:
	case GUI_MINSERT:
	case GUI_ERASE:
	case GUI_FOCUS:
	case GUI_INSERTLINEBEFORE:
	case GUI_INSERTLINEAFTER:
	case GUI_SELECTLINE:
	case GUI_DELETELINE:
	case GUI_LOCATELINE:
	case GUI_TEXTCOLOR:

	/*
	 * For by-line foreground color changes,
	 * the new COLORREF is in control->changecolor.
	 * If the color asked for was NONE, this will be black.
	 * For LINE, the one-based line number is in control->value3.
	 * For DATA, the text to match is in control->changetext.
	 */
	case GUI_TEXTCOLORDATA:
	case GUI_TEXTCOLORLINE:

	/*
	 * For by-line background color changes,
	 * the new TEXTCOLOR_??? (defined in gui.h) is in control->changecolor.
	 * For LINE, the one-based line number is in control->value3.
	 * For DATA, the text to match is in control->changetext.
	 */
	case GUI_TEXTBGCOLORDATA:
	case GUI_TEXTBGCOLORLINE:

	case GUI_TEXTSTYLEDATA:
	case GUI_TEXTSTYLELINE:
		if (control->type != TOOLBAR_DROPBOX) return RC_ERROR;
		return guiaChangeControl(res, control, iCmd);
	}
	return 0;
}

/*
 * show a panel or dialog
 */
LRESULT guiaShowControls(RESOURCE *res, INT hpos, INT vpos)
{
	GUIADLGCREATESTRUCT dcsCreateStruct;
	GUIASHOWCONTROLS scsControlStruct;
	WINDOW *win;

	if (res->entrycount == 0) return 0;
	if (res == NULL || res->winshow == NULL) {
		guiaErrorMsg("Invalid panel/dialog resource passed to guiaShowControls", 0);
		return(RC_INVALID_VALUE);
	}
	scsControlStruct.hpos = hpos;
	scsControlStruct.vpos = vpos;
	if (RES_TYPE_DIALOG == res->restype) {
		dcsCreateStruct.hpos = hpos;
		dcsCreateStruct.vpos = vpos;
		dcsCreateStruct.res = res;
		//rcRetCode = (INT)SendMessage(hwndmainwin, GUIAM_DLGCREATE, (WPARAM) 0, (LPARAM) &dcsCreateStruct);
		if (SendMessage(hwndmainwin, GUIAM_DLGCREATE, (WPARAM) 0, (LPARAM) &dcsCreateStruct)) return RC_ERROR;
		scsControlStruct.hwnd = res->hwnd;
		scsControlStruct.tabmask = WS_TABSTOP | WS_GROUP;
	}
	else {
		win = *res->winshow;
		scsControlStruct.hwnd = win->hwnd;
		res->hwnd = win->hwnd;
		scsControlStruct.tabmask = 0;
	}
	scsControlStruct.res = res;

	/*
	 * Not quite sure of the configuration that trips this up.
	 * Seems to be only some 64-bit Win7 boxes.
	 *
	 * We were using SendMessage, but sometimes, it was returning BEFORE
	 * the showcontrols function on the other thread was completing.
	 *
	 * So now we use an Event and PostMessage.
	 * 03 JUL 2012 JPR
	 */
	if (showControlsEvent == NULL) {
		showControlsEvent = CreateEvent(
				NULL,	/* No need for any security stuff */
				FALSE,	/* Auto-reset */
				FALSE,	/* Initially NOT signaled */
				NULL	/* Don't need a name */
				);
	}
	PostMessage(hwndmainwin, GUIAM_SHOWCONTROLS, (WPARAM) 0, (LPARAM) &scsControlStruct);
	WaitForSingleObject(showControlsEvent, INFINITE);
	//rcRetCode = gShowControlsReturnCode;
	return(gShowControlsReturnCode);
}

/*
 * hide a panel or dialog
 */
INT guiaHideControls(RESOURCE *res)
{
	GUIAHIDECONTROLS hcsHideCtrlStruct;
	INT rcRetCode;
	WINDOW *win;

	if (res == NULL || res->winshow == NULL) {
		guiaErrorMsg("Invalid panel/dialog resource passed to guiaHideControls", GetLastError());
		return RC_INVALID_VALUE;
	}
	if (res->entrycount == 0) {
		res->winshow = NULL;
		return 0;
	}

	hcsHideCtrlStruct.res = res;
	clearLastErrorMessage();
	rcRetCode = (INT)SendMessage(hwndmainwin, GUIAM_HIDECONTROLS, (WPARAM) 0, (LPARAM) &hcsHideCtrlStruct);
	if (RC_ERROR == rcRetCode) return RC_ERROR;
	win = *(res->winshow);

	/**
	 * Added 07 DEC 2012 jpr
	 * Dave Enlow at Apex Data Systems was the only one seeing a problem but it was chronic for
	 * him and one of his customers.
	 * A panel, when hidden, often did not disappear altogether.
	 * The below call to UpdateWindow seems to clear it up.
	 */
	UpdateWindow(win->hwnd);

	rcRetCode = 0;
	if (RES_TYPE_DIALOG == res->restype) {
		/* Hide main dialog window, runs dlgdestroy on Win thread */
		rcRetCode = (INT)SendMessage(hwndmainwin, GUIAM_DLGDESTROY, (WPARAM) 0, (LPARAM) res->hwnd);
	}
	else if (win->autoscroll) guiaCheckScrollBars(res->winshow);
	if (dlgreshandlelisthdr != NULL) {
		SetForegroundWindow((*dlgreshandlelisthdr)->hwnd);
		SetActiveWindow((*dlgreshandlelisthdr)->hwnd);
	}
	else SetActiveWindow(win->hwnd);
	res->winshow = NULL;
	return(rcRetCode);
}

/*
 * change a dialog title - only works for regular dialogs, not special dialogs
 */
INT guiaChangeDlgTitle(RESOURCE *res)
{
	if (res == NULL || res->hwnd == NULL) {
		guiaErrorMsg("Invalid parameter passed to guiaChangeDlgTitle()", 0);
		return(RC_INVALID_VALUE);
	}
	if (!SetWindowText(res->hwnd, (CHAR *) res->title)) {
		guiaErrorMsg("Change dialog text error", GetLastError());
		return RC_ERROR;
	}
	return 0;
}

/*
 * this function is called for each control being destroyed.
 * Called only from guiresdestroy in gui.c
 * Does special things for listboxes and trees. All other types ignored.
 */
INT guiaDestroyControl(CONTROL *control)
{
	INT i1;
	TREEICONSTRUCT *ptreeicon;

	if (control == NULL) return RC_INVALID_VALUE;
	if (ISLISTBOX(control)) {
		if (!control->value2) return 0;
		if (control->type != PANEL_MLISTBOX && control->type != PANEL_MLISTBOXHS) {
			/* set NULL because it might point to control->itemarray element */
			/* and we don't want to attempt to free the memory twice */
			control->textval = NULL;
		}
		for (i1 = control->value1 - 1; i1 >= 0; i1--) {
			guiFreeMem(((BYTE **) control->itemarray)[i1]);
			if (control->itemcutarray != NULL) {
				guiFreeMem(((BYTE **) control->itemcutarray)[i1]);
			}
		}
		control->value = control->value1 = 0;
		if (control->itemcutarray != NULL) guiFreeMem(control->itemcutarray);
		guiFreeMem(control->itemarray);
//		guiFreeMem((BYTE *) control->itemattributes);
		guiFreeMem((ZHANDLE) control->itemattributes2);
		control->itemarray = NULL;
		control->itemcutarray = NULL;
		control->itemarray = NULL;
	}
	else if (control->type == PANEL_TREE) {
		if (control->tree.iconlist != NULL) {
			do {
				ptreeicon = control->tree.iconlist->next;
				guiFreeMem((BYTE *)control->tree.iconlist);
				control->tree.iconlist = ptreeicon;
			}
			while (ptreeicon != NULL);
		}
		if (control->tree.root != NULL) {
			treedelete(control->ctlhandle, FALSE, control->tree.root);
			if (control->shownflag) TreeView_DeleteAllItems(control->ctlhandle);
		}
		ImageList_Destroy(TreeView_GetImageList(control->ctlhandle, TVSIL_STATE));
	}
	return 0;
}

/*
 * set the tab stops for a listbox control
 */
static void guiaSetTabs(CONTROL *control)
{
	INT *tabsptr;
	INT tabstops[MAXBOXTABS];
	INT i1, i2, avewidth;
	HDC fhdc;
	CHAR * meas = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	SIZE size;

	tabsptr = (INT *) control->listboxtabs;
	if (tabsptr == NULL) return;
	fhdc = GetDC(control->ctlhandle);
	SelectObject(fhdc, control->font);
	GetTextExtentPoint32(fhdc, meas, 52, &size);
	avewidth = size.cx / 52;
	if (((size.cx % 52) << 1) >= 52) avewidth++;
	for (i1 = i2 = 0; tabsptr[i1]; i1++) {
		i2 += tabsptr[i1];
		tabstops[i1] = (tabsptr[i1] * 4) / avewidth;
		if (i1) tabstops[i1] += tabstops[i1 - 1];
	}
	if (i1) {
		SendMessage(control->ctlhandle, LB_SETTABSTOPS, i1, (LPARAM) tabstops);
		/* make control scroll horizontal if it is not as wide as the sum of all tabstops */
		/* 8 gives us a little cushion so that the scrollbar doesn't appear unnecessarily */
		if (i2 > 8) SendMessage(control->ctlhandle, LB_SETHORIZONTALEXTENT, (WPARAM) i2 - 8, (LPARAM) 0);
	}
	ReleaseDC(control->ctlhandle, fhdc);
}

/**
 * Handle the GUI_MINSERT change function to a dropbox or cdropbox column
 * in a table.
 */
void guiaChangeTableMinsert(RESOURCE *res, CONTROL* control, UCHAR** stringArray, INT count) {
	INT n1;
	if (res->winshow == NULL || (!control->tabgroup && !control->shownflag)) return;
	for (n1 = 0; n1 < count; n1++) guiaChangeTableInsert(control, stringArray[n1]);
}

/**
 * Handle the GUI_INSERT change function to a dropbox or cdropbox column
 * in a table.
 */
static void guiaChangeTableInsert(CONTROL* control, CHAR* string) {
	PTABLECOLUMN tblColumns;
	ZHANDLE zh;
	PTABLECELL cell;
	PTABLEROW trptr;
	PTABLEDROPBOX dropbox;
	INT insstyle;
	UINT column;

	column = control->table.changeposition.x - 1;
	control->changeflag = TRUE;
 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);

	/**
	 * Dropbox insert order is set on a column basis, all DROPBOXes in a column
	 * have the same attribute here.
	 * But for DROPBOX, not CDROPBOX, the insert style/position can
	 * vary cell by cell.
	 */
	if (tblColumns[column].type == PANEL_DROPBOX) {
		zh = guiTableLocateRow(control, control->table.changeposition.y);
		trptr = guiLockMem(zh);
		cell = &trptr->cells[column];
		dropbox = &cell->dbcell.db;
		insstyle = dropbox->insstyle;
		guiUnlockMem(zh);
	}
	else if (tblColumns[column].type == PANEL_CDROPBOX) {
		insstyle = tblColumns[column].db.insstyle;
	}
	else insstyle = LIST_INSERTNORMAL;		// defensive coding, should never happen

	if (tblColumns[column].insertorder) {
		switch (insstyle) {
		case LIST_INSERTNORMAL:
			SendMessage(control->ctlhandle, CB_ADDSTRING, 0L, (LPARAM)(LPCSTR)string);
 			//ComboBox_AddString(control->ctlhandle, string);
			break;
		case LIST_INSERTBEFORE:
			SendMessage(control->ctlhandle, CB_INSERTSTRING, (WPARAM)(int)control->value3 - 1, (LPARAM)(LPCSTR)string);
 			//ComboBox_InsertString(control->ctlhandle, control->value3 - 1, string);
			break;
		case LIST_INSERTAFTER:
			SendMessage(control->ctlhandle, CB_INSERTSTRING, (WPARAM)(int)control->value3, (LPARAM)(LPCSTR)string);
 			//ComboBox_InsertString(control->ctlhandle, control->value3, string);
			break;
		}
	}
	else {
		SendMessage(control->ctlhandle, CB_ADDSTRING, 0L, (LPARAM)(LPCSTR)string);
		//ComboBox_AddString(control->ctlhandle, string);
	}
 	guiUnlockMem(control->table.columns);
	control->changeflag = FALSE;
}

/**
 * change a table control in a panel or dialog
 */
static INT guiaChangeTable(RESOURCE *res, CONTROL *control, INT iCmd) {

	PTABLECOLUMN tblColumns;
	LVITEM lvitem;
	UINT row, column;

	if (res->winshow == NULL || (!control->tabgroup && !control->shownflag)) return 0;

	row = (control->table.changeposition.y > 0) ? control->table.changeposition.y - 1 : -1;
	column = control->table.changeposition.x - 1;

	switch(iCmd) {

	case GUI_FOCUS:
		control->changeflag = TRUE;
		SendMessage(control->ctlhandle, DXTBL_FOCUSCELL, row, column);
		control->changeflag = FALSE;
		break;

	//CODE This is temporary!
	// We should send a message to the table for this, and support by-cell and visual feedback
	case GUI_GRAY:
		EnableWindow(control->ctlhandle, FALSE);
		InvalidateRect(control->ctlhandle, NULL, TRUE);
		break;
	case GUI_AVAILABLE:
		SendMessage(control->ctlhandle, DXTBL_READONLY, 0, 0); //Causes table to reevaluate its internal flag
		EnableWindow(control->ctlhandle, TRUE);
		InvalidateRect(control->ctlhandle, NULL, TRUE);
		break;

	case GUI_READONLY:
		SendMessage(control->ctlhandle, DXTBL_READONLY, 0, 0); //Causes table to reevaluate its internal flag
		break;

	case GUI_MARK:
		control->changeflag = TRUE;
		lvitem.iSubItem = control->table.changeposition.x - 1;
		lvitem.mask = LVIF_STATE;
		lvitem.stateMask = LVIS_SELECTED;
		lvitem.state = LVIS_SELECTED;
		SendMessage(control->ctlhandle, LVM_SETITEMSTATE, (WPARAM) row, (LPARAM) &lvitem);
		control->changeflag = FALSE;
		break;

	case GUI_UNMARK:
		control->changeflag = TRUE;
		lvitem.iSubItem = control->table.changeposition.x - 1;
		lvitem.mask = LVIF_STATE;
		lvitem.stateMask = LVIS_SELECTED;
		lvitem.state = 0;
		SendMessage(control->ctlhandle, LVM_SETITEMSTATE, (WPARAM) row, (LPARAM) &lvitem);
		control->changeflag = FALSE;
		break;

	case GUI_ADDROW:
		SendMessage(control->ctlhandle, DXTBL_ADDROW, 0, 0);
		break;

	case GUI_DELETEROW:
		SendMessage(control->ctlhandle, LVM_DELETEITEM, row, 0);
		break;

	case GUI_DELETEALLROWS:
		SendMessage(control->ctlhandle, LVM_DELETEALLITEMS, 0, 0);
		break;

	case GUI_INSERTROWBEFORE:
		SendMessage(control->ctlhandle, DXTBL_INSERTROW, 0, row);
		break;

	case GUI_ERASE:
	 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
	 	if (ISTABLECOLUMNDROPBOX(tblColumns[column].type)) {
	 		SendMessage(control->ctlhandle, CB_RESETCONTENT, 0L, 0L);
	 		//ComboBox_ResetContent(control->ctlhandle);
		 	guiUnlockMem(control->table.columns);
			break;
	 	}
	 	else if (!ISTABLECOLUMNTEXT(tblColumns[column].type)) break; // @suppress("No break at end of case")
 		/* fall through, control->table.changetext has been set to a zero length string already */

	case GUI_TEXT:
		lvitem.iSubItem = column;
		lvitem.pszText = guiLockMem(control->table.changetext);
		control->changeflag = TRUE;
		SendMessage(control->ctlhandle, LVM_SETITEMTEXT, (WPARAM) row, (LPARAM) &lvitem);
		control->changeflag = FALSE;
		guiUnlockMem(control->table.changetext);
		break;

	case GUI_DELETELINE:
	case GUI_DELETE:
	 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
	 	if (ISTABLECOLUMNDROPBOX(tblColumns[column].type)) {
	 		SendMessage(control->ctlhandle, CB_DELETESTRING, (WPARAM)(int)control->value3 - 1, 0L);
	 		//ComboBox_DeleteString(control->ctlhandle, control->value3 - 1);
	 	}
	 	guiUnlockMem(control->table.columns);
		break;

	case GUI_MINSERT:
		// The code will flow through here but we can ignore it.
		// This function is taken care of already.
		break;

	case GUI_INSERT:
		guiaChangeTableInsert(control, guiLockMem(control->table.changetext));
		guiUnlockMem(control->table.changetext);
		break;

	case GUI_LOCATE:
	case GUI_LOCATELINE:
		control->changeflag = TRUE;
		SendMessage(control->ctlhandle, CB_SETCURSEL, (WPARAM)(int)control->value3 - 1, 0L);
		//ComboBox_SetCurSel(control->ctlhandle, control->value3 - 1);
		control->changeflag = FALSE;
		break;

	case GUI_TEXTCOLOR:
		if (control->table.changescope == tableChangeScope_row) SendMessage(control->ctlhandle, DXTBL_COLORROW, row, 0);
		else if (control->table.changescope == tableChangeScope_column) SendMessage(control->ctlhandle, DXTBL_COLORCOLUMN, 0, column);
		else if (control->table.changescope == tableChangeScope_cell) SendMessage(control->ctlhandle, DXTBL_COLORCELL, row, column);
		else if (control->table.changescope == tableChangeScope_table) SendMessage(control->ctlhandle, DXTBL_COLORTABLE, 0, 0);
		break;
	}
	return 0;
}

/**
 * change a tree control in a panel or dialog
 */
static INT guiaChangeTree(RESOURCE *res, CONTROL *control, INT iCmd) {
	INT i1;
	DWORD error;
	RESOURCE* resicon;
	TREESTRUCT *ptree, *ctree, *ntree;
	TVINSERTSTRUCT tvins;
	TVITEMEXA tvitem;
	CHAR work[128];
	HICON hicon;
	RECT rect;
	USHORT usItemPos;
	TREEICONSTRUCT *ptreeicon, *ntreeicon;
	HIMAGELIST imagelist;
	BOOL mightBeVisible = (res->winshow != NULL && (control->tabgroup || control->shownflag));

	switch(iCmd) {
	case GUI_COLLAPSE:
	case GUI_EXPAND:
		ptree = control->tree.xpos;
		ptree->expanded = (iCmd == GUI_EXPAND);
		break;
	case GUI_DELETE:
		if (control->tree.cpos == NULL) {
			return RC_ERROR;
		}
		if (control->tree.line == TREE_POS_INSERT && control->tree.level >= 1) {
			control->tree.level--;
		}
		else control->tree.line = TREE_POS_INSERT; /* invalidate line */

		/* delete all children */
		treedelete(control->ctlhandle, control->shownflag, control->tree.cpos->firstchild);

		/* delete node from linked list */
		if (control->tree.cpos->parent == NULL) ptree = control->tree.root;
		else ptree = control->tree.cpos->parent->firstchild;
		ctree = NULL;
		while (ptree != NULL) {
			if (ptree == control->tree.cpos) break;
			ctree = ptree; /* ctree is previous */
			ptree = ptree->nextchild;
		}
		if (ptree == NULL) return 0;
		if (ctree == NULL) {
			/* first child */
			if (control->tree.cpos->parent == NULL) {
				/* level 0 */
				control->tree.root = ptree->nextchild;
				control->tree.cpos = control->tree.root;
				if (control->tree.root != NULL) {
					control->tree.line = 1;
				}
			}
			else {
				/* > level 0 */
				control->tree.cpos->parent->firstchild = ptree->nextchild;
				if (ptree->nextchild == NULL) {
					/* was single leaf, move down 1 level */
					control->tree.cpos = control->tree.cpos->parent;
					control->tree.level--;
					control->tree.line = 1;
					control->tree.parent = control->tree.cpos->parent;
				}
				else control->tree.cpos = ptree->nextchild;
			}
		}
		else {
			ctree->nextchild = ptree->nextchild;
			control->tree.cpos = ctree;
			control->tree.line = 1;
		}

		/*
		 * If tree is not visible, free the TREESTRUCT memory now,
		 * else it will be freed later in this function.
		 */
		if (control->ctlhandle == NULL) guiFreeMem((BYTE *)ptree);
		break;
	case GUI_ERASE:
		treedelete(control->ctlhandle, FALSE, control->tree.root);
		control->tree.root = NULL;
		control->tree.cpos = NULL;
		control->tree.spos = NULL;
		control->tree.parent = NULL;
		control->tree.line = TREE_POS_INSERT;
		control->tree.level = TREE_POS_ROOT;
		if (mightBeVisible && control->ctlhandle != NULL) TreeView_DeleteAllItems(control->ctlhandle);
		break;

	case GUI_ICON:
		ptreeicon = control->tree.iconlist;
		control->tree.cpos->imageindex = 1;
		while (ptreeicon != NULL) {
			if (control->iconres == ptreeicon->iconres) {
				break;
			}
			control->tree.cpos->imageindex++;
			ptreeicon = ptreeicon->next;
		}
		if (ptreeicon == NULL) {
			/* image not found */
			ntreeicon = (TREEICONSTRUCT *) guiAllocMem(sizeof(TREEICONSTRUCT));
			if (ntreeicon == NULL ) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned Null in %s at GUI_ICON", __FUNCTION__);
				guiaErrorMsg(work, sizeof(TREEICONSTRUCT));
				return RC_NO_MEM;
			}
			ZeroMemory(ntreeicon, sizeof(TREEICONSTRUCT));
			ntreeicon->iconres = control->iconres;
			if (control->tree.iconlist == NULL) {
				control->tree.iconlist = ntreeicon;
			}
			else {
				ptreeicon = control->tree.iconlist;
				while (ptreeicon->next != NULL) ptreeicon = ptreeicon->next;
				ptreeicon->next = ntreeicon;
			}
			ptreeicon = NULL; /* set to NULL to indicate image list insertion in following switch */
		}
		break;

	case GUI_INSERT:
		ptree = (TREESTRUCT *) guiAllocMem(sizeof(TREESTRUCT));
		if (ptree == NULL) {
			sprintf_s(work, ARRAYSIZE(work),
					"guiAllocMem returned Null in %s at GUI_INSERT (ptree)", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return RC_NO_MEM;
		}
		ZeroMemory(ptree, sizeof(TREESTRUCT));
		ZeroMemory(&tvitem, sizeof(TVITEMEXA));
		ZeroMemory(&tvins, sizeof(TVINSERTSTRUCT));
		tvitem.mask = TVIF_CHILDREN | TVIF_TEXT | TVIF_STATE;
		tvitem.cChildren = 0;
		tvitem.pszText = (CHAR *) control->changetext;
		tvitem.stateMask = TVIS_STATEIMAGEMASK;
		tvitem.state = INDEXTOSTATEIMAGEMASK(0); /* no image */
		tvins.itemex = tvitem;
		tvins.hInsertAfter = (control->tree.cpos == NULL) ? TVI_LAST : control->tree.cpos->htreeitem;
		if (control->tree.level == TREE_POS_ROOT) {
			tvins.hParent = TVI_ROOT;
		}
		else {
			tvins.hParent = control->tree.parent->htreeitem;
			/* create parent child relationship */
			ptree->parent = control->tree.parent;
			if (control->tree.line == TREE_POS_INSERT) {
				control->tree.parent->firstchild = ptree;
			}
		}
		if (control->tree.line == TREE_POS_INSERT && control->tree.level == TREE_POS_ROOT
			&& control->tree.root == NULL && control->tree.cpos == NULL) {
			control->tree.root = ptree;
		}
		else if (control->tree.cpos->nextchild != NULL && control->tree.line != TREE_POS_INSERT) {
			/* inserting between two existing children */
			ptree->nextchild = control->tree.cpos->nextchild;
		}
		if (++control->tree.line > 1) control->tree.cpos->nextchild = ptree;
		strncpy(ptree->text, (CHAR *) control->changetext, TREEVIEW_ITEM_MAX_SIZE - 2);
		ptree->text[TREEVIEW_ITEM_MAX_SIZE - 1] = '\0';
		control->tree.cpos = ptree;
		break;

	case GUI_INSERTAFTER:
		if (control->changetext == NULL) break;
		if (!strlen((CHAR *) control->changetext)) break;
		ptree = (TREESTRUCT *) guiAllocMem(sizeof(TREESTRUCT));
		if (ptree == NULL) {
			sprintf_s(work, ARRAYSIZE(work),
					"guiAllocMem returned Null in %s at GUI_INSERTAFTER (ptree)", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return RC_NO_MEM;
		}
		ZeroMemory(ptree, sizeof(TREESTRUCT));
		tvitem.mask = TVIF_CHILDREN | TVIF_TEXT | TVIF_STATE;
		tvitem.cChildren = 0;
		tvitem.pszText = (CHAR *) control->changetext;
		tvitem.stateMask = TVIS_STATEIMAGEMASK;
		tvitem.state = INDEXTOSTATEIMAGEMASK(0); /* no image */
		tvins.itemex = tvitem;
		tvins.hInsertAfter = TVI_LAST;
		if (control->tree.level == TREE_POS_ROOT) {
			tvins.hParent = TVI_ROOT;
		}
		else {
			tvins.hParent = control->tree.parent->htreeitem;
			/* create parent child relationship */
			ptree->parent = control->tree.parent;
			if (control->tree.line == TREE_POS_INSERT) {
				control->tree.parent->firstchild = ptree;
			}
		}
		if (control->tree.line == TREE_POS_INSERT && control->tree.level == TREE_POS_ROOT
			&& control->tree.root == NULL && control->tree.cpos == NULL) {
			control->tree.root = ptree;
		}
		else if (control->tree.cpos != NULL && control->tree.line != TREE_POS_INSERT) {
			/* prepare to insert last in linked list */
			while (control->tree.cpos->nextchild != NULL) {
				control->tree.cpos = control->tree.cpos->nextchild;
				control->tree.line++;
			}
		}
		if (++control->tree.line > 1) control->tree.cpos->nextchild = ptree;
		strncpy(ptree->text, (CHAR *) control->changetext, TREEVIEW_ITEM_MAX_SIZE - 2);
		ptree->text[TREEVIEW_ITEM_MAX_SIZE - 1] = '\0';
		control->tree.cpos = ptree;
		break;

	case GUI_INSERTBEFORE:
		if (control->changetext == NULL) break;
		if (!strlen((CHAR *) control->changetext)) break;
		ptree = (TREESTRUCT *) guiAllocMem(sizeof(TREESTRUCT));
		if (ptree == NULL) {
			sprintf_s(work, ARRAYSIZE(work),
					"guiAllocMem returned Null in %s at GUI_INSERTBEFORE (ptree)", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return RC_NO_MEM;
		}
		ZeroMemory(ptree, sizeof(TREESTRUCT));
		tvitem.mask = TVIF_CHILDREN | TVIF_TEXT | TVIF_STATE;
		tvitem.cChildren = 0;
		tvitem.stateMask = TVIS_STATEIMAGEMASK;
		tvitem.state = INDEXTOSTATEIMAGEMASK(0); /* no image */
		tvitem.pszText = (CHAR *) control->changetext;
		tvins.itemex = tvitem;
		if (control->tree.cpos == NULL) {
			tvins.hInsertAfter = TVI_FIRST;
		}
		else if (control->tree.line == TREE_POS_INSERT) {
			tvins.hInsertAfter = TVI_FIRST;
			ptree->nextchild = control->tree.cpos->firstchild;
			control->tree.cpos->firstchild = ptree;
		}
		else {
			if (control->tree.cpos->parent == NULL) ntree = control->tree.root;
			else ntree = control->tree.cpos->parent->firstchild;
			tvins.hInsertAfter = TVI_FIRST;
			ptree->nextchild = ntree;
		}

		if (control->tree.level == TREE_POS_ROOT) {
			tvins.hParent = TVI_ROOT;
		}
		else {
			tvins.hParent = control->tree.parent->htreeitem;
			/* create parent child relationship */
			ptree->parent = control->tree.parent;
			control->tree.parent->firstchild = ptree;
		}

		if (control->tree.level == TREE_POS_ROOT && tvins.hInsertAfter == TVI_FIRST) {
			control->tree.root = ptree;
		}
		if (control->tree.line == TREE_POS_INSERT && control->tree.level > TREE_POS_ROOT) {
			control->tree.cpos->firstchild = ptree;
		}
		control->tree.line = 1;
		strncpy(ptree->text, (CHAR *) control->changetext, TREEVIEW_ITEM_MAX_SIZE - 2);
		ptree->text[TREEVIEW_ITEM_MAX_SIZE - 1] = '\0';
		control->tree.cpos = ptree;
		break;

	case GUI_TEXTSTYLEDATA:
	case GUI_TEXTCOLORDATA:
		if (control->changetext == NULL) break;
		if (control->tree.level == 0) ptree = control->tree.root;
		else ptree = control->tree.parent->firstchild;
		while (ptree != NULL) {
			if (!strcmp((CHAR *)control->changetext, ptree->text)) {
				// Set ptree for use lower down in this function
				if (iCmd == GUI_TEXTCOLORDATA) {
					ptree->color = control->changecolor;
				}
				else if (iCmd == GUI_TEXTSTYLEDATA) {
					ptree->attr1 = control->changestyle;
				}
				break;
			}
			ptree = ptree->nextchild;
		}
		break;

	case GUI_TEXTSTYLELINE:
	case GUI_TEXTCOLORLINE:
		usItemPos = control->value3;
		if (usItemPos <= 0) return 0;
		if (control->tree.level == 0) ptree = control->tree.root;
		else ptree = control->tree.parent->firstchild;
		i1 = 1;
		while (ptree != NULL) {
			if (i1 == usItemPos) break;
			ptree = ptree->nextchild;
			i1++;
		}
		if (ptree == NULL || usItemPos < i1) return RC_ERROR;
		// Set ptree for use lower down in this function
		if (iCmd == GUI_TEXTCOLORLINE) {
			ptree->color = control->changecolor;
		}
		else if (iCmd == GUI_TEXTSTYLELINE) {
			ptree->attr1 = control->changestyle;
		}
		break;
	}


	if (mightBeVisible) {
		switch(iCmd) {
		case GUI_DESELECT:
			if (control->ctlhandle == NULL) break;
			SendMessage(control->ctlhandle, TVM_SELECTITEM, TVGN_CARET, (LPARAM) NULL);
			break;
		case GUI_SELECT:
			if (control->ctlhandle == NULL) break;
			SendMessage(control->ctlhandle, TVM_ENSUREVISIBLE, 0, (LPARAM) (HTREEITEM) control->tree.cpos->htreeitem);
			SendMessage(control->ctlhandle, TVM_SELECTITEM, TVGN_CARET, (LPARAM) (HTREEITEM) control->tree.cpos->htreeitem);
			break;
		case GUI_AVAILABLE:
			handleGuiAvailable(control);
			break;
		case GUI_GRAY:
			handleGuiGray(res, control);
			break;
		case GUI_READONLY:
			handleGuiReadonly(control);
			break;
		case GUI_EXPAND:
			if (control->ctlhandle != NULL) {
				TreeView_Expand(control->ctlhandle, ptree->htreeitem, TVE_EXPAND);
			}
			break;
		case GUI_COLLAPSE:
			if (control->ctlhandle != NULL) {
				TreeView_Expand(control->ctlhandle, ptree->htreeitem, TVE_COLLAPSE);
			}
			break;
		case GUI_DELETE:
			if (control->ctlhandle == NULL) break;
			SendMessage(control->ctlhandle, TVM_DELETEITEM, 0, (LPARAM) (HTREEITEM) ptree->htreeitem);
			if (ptree->parent != NULL && TreeView_GetChild(control->ctlhandle, ptree->parent->htreeitem) == NULL) {
				/* parent has no more children */
				memset(&tvitem, 0, sizeof(TVITEM));
				tvitem.mask = TVIF_CHILDREN;
				tvitem.hItem = ptree->parent->htreeitem;
				tvitem.cChildren = 0;
				/* visually remove the +/- sign next to the tree node */
				SendMessage(control->ctlhandle, TVM_SETITEM, 0, (LPARAM) (LPTVITEM) &tvitem);
			}
			guiFreeMem((BYTE *)ptree);
			break;

		case GUI_TEXTCOLORDATA:
		case GUI_TEXTCOLORLINE:
		case GUI_TEXTSTYLEDATA:
		case GUI_TEXTSTYLELINE:
			if (!control->shownflag || ptree == NULL || control->ctlhandle == NULL) break;
			control->changeflag = TRUE;
			/* only repaint line if it is visible */
			*(HTREEITEM *) &rect = ptree->htreeitem;
			if (SendMessage(control->ctlhandle, TVM_GETITEMRECT, FALSE, (LPARAM) &rect)) {
				InvalidateRect(control->ctlhandle, &rect, FALSE);
			}
			control->changeflag = FALSE;
			break;

		case GUI_ICON:
			if (control->ctlhandle == NULL) break;
			resicon = *control->iconres;
			if (ptreeicon == NULL) {
				/* insert image at end of image list (position imageindex) */
				imagelist = TreeView_GetImageList(control->ctlhandle, TVSIL_STATE);
				if (imagelist == NULL) return RC_ERROR;
				if (resicon->hsize != 16 || resicon->vsize != 16) return(RC_INVALID_VALUE);
				hicon = createiconfromres(resicon, FALSE, &error);
				if (hicon == NULL) {
					guiaErrorMsg("Change tree icon error occured", error);
					return RC_ERROR;
				}
				if (ImageList_AddIcon(imagelist, hicon) < 0) return RC_ERROR;
			}
			/* set tree item image to image at imageindex */
			/* Do not use the macro, mingw has a faulty version */
			tvitem.mask = TVIF_STATE;
			tvitem.hItem = control->tree.cpos->htreeitem;
			tvitem.state = INDEXTOSTATEIMAGEMASK(control->tree.cpos->imageindex);
			tvitem.stateMask = TVIS_STATEIMAGEMASK;
			SendMessage(control->ctlhandle, TVM_SETITEM, 0, (LPARAM)(const LPTVITEM)&tvitem);
			break;

		case GUI_INSERT:
		case GUI_INSERTAFTER:
		case GUI_INSERTBEFORE:
			if (control->ctlhandle == NULL || ptree == NULL) break;
			ptree->htreeitem = (HTREEITEM) SendMessage(control->ctlhandle,
					TVM_INSERTITEM, (WPARAM) 0, (LPARAM) (LPTVINSERTSTRUCT) &tvins);
			if (ptree->htreeitem == NULL) return RC_ERROR;
			if (ptree->parent != NULL) {
				tvitem.mask = TVIF_CHILDREN;
				tvitem.hItem = ptree->parent->htreeitem;
				tvitem.cChildren = TRUE;
				if (!SendMessage(control->ctlhandle, TVM_SETITEM, 0, (LPARAM) (LPTVITEM) &tvitem)) return RC_ERROR;
			}
			break;

		case GUI_HELPTEXT:
		    if (!guiaAddTooltip(control)) return RC_ERROR;
			break;

		case GUI_FOCUS:
			SendMessage(hwndmainwin, GUIAM_SETCTRLFOCUS, 0, (LPARAM) control);
			break;
		}
	}
	return 0;
}

/*
 * change a control in a panel or dialog
 */
INT guiaChangeControl(RESOURCE *res, CONTROL *control, INT iCmd)
{
	GUIAEXWINCREATESTRUCT ecsExtWinCreate;
	WINDOW *win;
	CHAR *origString, *thistabString, *cutString, work[128];
	BYTE *text;
	INT iItemPos;
	INT i1, i2, iMoveCnt, iResult, iLen, ptmp, thistabPos, cutPos, tabcount, changelen;
	RECT rect;
	UINT nMessage;
	POINT pt;
	DWORD dwStyle, dwMask, styleint;
	HWND hControlNew;
	ZHANDLE NewItemArray, NewItemCutArray;
	ITEMATTRIBUTES *newAttributesArray2;
	CONTROL *workcontrol;
	NMHDR nmhdr;
	RESOURCE* resicon;
	INT *tabsptr;
	SIZE tabsize, size;
	HDC fhdc;
	TCITEM tcitem;
	LPCSTR string;

	if (res == NULL || control == NULL) return(RC_INVALID_VALUE);

	/**
	 * Make early check for allowed changes to a TAB
	 */
	if (control->type == PANEL_TAB && iCmd != GUI_FOCUS && iCmd != GUI_HELPTEXT && iCmd != GUI_LOCATETAB
		&& iCmd != GUI_TEXT && iCmd != GUI_TEXTCOLOR)
	{
		return RC_ERROR;
	}

	if (control->type == PANEL_TABLE) {
		if (control->ctlhandle == NULL) return 0;
		return guiaChangeTable(res, control, iCmd);
	}
	else if (control->type == PANEL_TREE) {
		return guiaChangeTree(res, control, iCmd);
	}

	switch(iCmd) {
	case GUI_FEDITMASK:
		if (control->type == PANEL_FEDIT) {
			pvistart(); /* control->changetext could be modified at same time in guiaControlProc */
			/* control->changetext holds the new value of the mask, so copy it to control->mask */
			control->mask = guiReallocMem(control->mask, (INT)(strlen((CHAR *)control->changetext) + 1) * sizeof(ZHANDLE));
			if (control->mask == NULL) {
				pviend();
				sprintf_s(work, ARRAYSIZE(work), "guiReallocMem returned a null in %s at GUI_FEDITMASK", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
			strcpy((CHAR *)control->mask, (CHAR *)control->changetext);
			/* control->changetext must now hold text of the FEDIT for guiFEditMaskText(), which is in control->textval */
			control->changetext = guiReallocMem(control->changetext,
					(INT)(strlen((CHAR *)control->textval) + 1) * sizeof(ZHANDLE));
			if (control->changetext == NULL) {
				pviend();
				sprintf_s(work, ARRAYSIZE(work), "guiReallocMem returned a null in %s at GUI_FEDITMASK", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
			strcpy((CHAR *)control->changetext, (CHAR *)control->textval);
			/* control->textval must be empty string for guiaFEditMaskText() call */
			control->textval[0] = '\0';
			guiaFEditMaskText((CHAR *) control->changetext, (CHAR *) control->mask, (CHAR *) control->textval);
			pviend();
		}
		break;
	case GUI_TEXT:
		if (control->type == PANEL_FEDIT) {
			pvistart(); /* control->changetext could be modified at same time in guiaControlProc */
			guiaFEditMaskText((CHAR *) control->changetext, (CHAR *) control->mask, (CHAR *) control->textval);
			pviend();
		}
		break;
	case GUI_ERASE:
		if (control->type != PANEL_MLISTBOX && control->type != PANEL_MLISTBOXHS) {
			control->textval = NULL;
		}
		if (ISLISTBOX(control)) {
			for (i1 = control->value1 - 1; i1 >= 0; i1--) {
				guiFreeMem(((BYTE **) control->itemarray)[i1]);
				guiFreeMem(((BYTE **) control->itemcutarray)[i1]);
				memset(&control->itemattributes2[i1], 0, sizeof(ITEMATTRIBUTES));
			}
			control->value = control->value1 = 0;
			control->inspos = 1;  /* insert position in itemarray (one-based) */
			control->insstyle = LIST_INSERTNORMAL;
		}
		break;

	case GUI_DELETE:
		/* find control->changetext in control->textval */
		/* find item in list box/drop box */
		if (control->changetext == NULL) return RC_ERROR;
		if (!(control->style & CTRLSTYLE_INSERTORDER)) {	/* list box is alphabetic */
			iResult = guiListBoxAlphaFind(control->itemarray, control->value1, control->changetext, &iItemPos);
		}
		else iResult = guiListBoxAppendFind(control->itemarray, control->value1, control->changetext, &iItemPos);
		/**
		 * Change on 08 JUN 2012 for 16.1.1
		 * The documentation does not define what happens when a delete to a list/drop box cannot find
		 * the text. We have been issuing an I 794. The Java SC ignores this case.
		 * We have decided to ignore it in the C code too.
		 */
		if (iResult == RC_ERROR) return 0; // @suppress("No break at end of case")
		/* fall through if iResult is zero, that is, the text was found*/
	case GUI_DELETELINE:
		if (GUI_DELETELINE == iCmd) {
			iItemPos = control->value3;
			if (iItemPos <= 0 || iItemPos > control->value1) {
				return RC_ERROR;
			}
		}
		/* if currently selected then deselect */
		if (control->value == iItemPos) {
			control->value = 0;
			if (control->type != PANEL_MLISTBOX && control->type != PANEL_MLISTBOXHS) control->textval = NULL;
		}
		else if (control->value > iItemPos) control->value--;
		if (control->inspos > iItemPos) control->inspos--;
		/* remove index'th value from control->textval */
		guiFreeMem(((BYTE **) control->itemarray)[iItemPos - 1]);
		guiFreeMem(((BYTE **) control->itemcutarray)[iItemPos - 1]);
//		control->itemattributes[usItemPos - 1] = 0;
		memset(&control->itemattributes2[iItemPos - 1], 0, sizeof(ITEMATTRIBUTES));
		iMoveCnt = control->value1 - iItemPos;
		memmove(&((BYTE **) control->itemarray)[iItemPos - 1], &((BYTE **) control->itemarray)[iItemPos], iMoveCnt * sizeof(ZHANDLE));
		memmove(&((BYTE **) control->itemcutarray)[iItemPos - 1], &((BYTE **) control->itemcutarray)[iItemPos], iMoveCnt * sizeof(ZHANDLE));
//		memmove(&((DWORD *) control->itemattributes)[usItemPos - 1], &((DWORD *) control->itemattributes)[usItemPos], iMoveCnt * sizeof(DWORD));
		memmove(&((ITEMATTRIBUTES *) control->itemattributes2)[iItemPos - 1],
				&((ITEMATTRIBUTES *) control->itemattributes2)[iItemPos], iMoveCnt * sizeof(ITEMATTRIBUTES));
		control->value1--;
		break;
	case GUI_INSERTBEFORE:
		/* fall through */
	case GUI_INSERTAFTER:
		if (!control->value2) return 0;  /* no items have been inserted yet */
		if (control->changetext == NULL) return 0;
		if (!(control->style & CTRLSTYLE_INSERTORDER)) {	/* list box is alphabetic */
			iResult = guiListBoxAlphaFind(control->itemarray, control->value1, control->changetext, &iItemPos);
		}
		else {
			iResult = guiListBoxAppendFind(control->itemarray, control->value1, control->changetext, &iItemPos);
		}
		if (!iResult) control->inspos = iItemPos;
		break;

	case GUI_INSERT:

		/* locate insert position */
		tabsptr = (INT *) control->listboxtabs;
		if (control->changetext == NULL) return 0;
		changelen = iLen = (INT)strlen((CHAR *) control->changetext);
		if (!iLen) return (0);

		origString = (CHAR *) guiAllocString((BYTE *) control->changetext, iLen);
		if (origString == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at GUI_INSERT (origString)", __FUNCTION__);
			guiaErrorMsg(work, iLen);
			return RC_NO_MEM;
		}
		ptmp = 0;
		cutPos=0;
		tabcount=0;
		if (tabsptr != NULL) {
			thistabString = (CHAR *) guiAllocMem((INT)strlen((CHAR *) control->changetext) + 1);
			if (thistabString == NULL) {
				guiFreeMem((ZHANDLE) origString);
				sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned Null in %s at GUI_INSERT (thistabString)", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
			cutString = (CHAR *) guiAllocMem((INT)strlen((CHAR *) control->changetext) + 1);
			if (cutString == NULL) {
				guiFreeMem((ZHANDLE) origString);
				guiFreeMem((ZHANDLE) thistabString);
				sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned Null in %s at GUI_INSERT (cutString)", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
			fhdc = GetDC(NULL);
			SelectObject(fhdc, control->font);
			for (i1 = 0; i1 <= changelen; i1++) {
				if ((control->changetext[i1] == 0x9) || (i1 == changelen)) { /* moved one tab string from origString to thistabString */
					for (thistabPos=0; thistabPos < ptmp; cutPos++, thistabPos++) { /* cut thistabString and append it to cutString */
						cutString[cutPos] = thistabString[thistabPos];
						if (!GetTextExtentPoint32(fhdc, thistabString, (thistabPos + 1), &tabsize)) {
							i1 = GetLastError();
						}
						if (tabsize.cx >= (tabsptr[tabcount]/* - 10*/)) {  /* thistabString is too long for tabs set.  Cut it, replace last three letters with '.' */
							cutString[cutPos] = 0; /* immediately cut last letter.  It is equal to or over the limit. */
							ptmp = cutPos - 1;
							thistabPos--;
							for (i2 = 3; i2 > 0; i2--, ptmp--, thistabPos--) { /* move backwords in cutString, replacing last three letters with '.'  If not three letters available, replace as many as there are. */
								if(thistabPos < 0) break;
								cutString[ptmp] = '.';
							}
							break; /* we're done with this tab string.  break out and start on next one. */
						}
					}
					tabcount++; /* move on to next tab string */
					if (i1 == changelen) {
						/* if there are more tab strings, append tab char to cutString, else terminate it. */
						cutString[cutPos] = 0x0;
					}
					else cutString[cutPos] = 0x9;
					cutPos++;
					ptmp = 0;
					continue;
				}
				thistabString[ptmp] = control->changetext[i1]; /* move tab string from origString into thistabString */
				ptmp++;
			}
			guiFreeMem((ZHANDLE) thistabString);
			ReleaseDC(NULL, fhdc);
		}
		else {
			cutString = (CHAR *) guiAllocString((BYTE *) control->changetext, iLen);
			if (cutString == NULL) {
				guiFreeMem((ZHANDLE) origString);
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at GUI_INSERT", __FUNCTION__);
				guiaErrorMsg(work, iLen);
				return RC_NO_MEM;
			}
		}
		if (!control->value2) {  /* first item being inserted */
			control->itemarray = guiAllocMem(LISTITEM_GROW_SIZE * sizeof(ZHANDLE));
			control->itemcutarray = guiAllocMem(LISTITEM_GROW_SIZE * sizeof(ZHANDLE));
			//control->itemattributes = (DWORD *) guiAllocMem(LISTITEM_GROW_SIZE * sizeof(DWORD));
			control->itemattributes2 = (ITEMATTRIBUTES *) guiAllocMem(LISTITEM_GROW_SIZE * sizeof(ITEMATTRIBUTES));
			if (control->itemarray == NULL || control->itemcutarray == NULL /*|| control->itemattributes == NULL*/
					|| control->itemattributes2 == NULL) {
				if (control->itemarray != NULL) guiFreeMem(control->itemarray);
				if (control->itemcutarray != NULL) guiFreeMem(control->itemcutarray);
				//if (control->itemattributes != NULL) guiFreeMem((BYTE *) control->itemattributes);
				if (control->itemattributes2 != NULL) guiFreeMem((BYTE *) control->itemattributes2);
				guiFreeMem((ZHANDLE) origString);
				guiFreeMem((ZHANDLE) cutString);
				sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned Null in %s at GUI_INSERT", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
			//control->itemattributes[0] = 0;
			memset(&control->itemattributes2[0], 0, sizeof(ITEMATTRIBUTES));
			control->value = 0;  		/* currently selected item, (one-based, zero is none) */
			control->value1 = 0;				/* number of items in itemarray */
			control->value2 = LISTITEM_GROW_SIZE;	/* size of itemarray */
			control->inspos = 1;			/* insert position in itemarray (one-based) */
		}
		else {
			if (control->value1 == control->value2) {	/* item array is full, reallocate */
				NewItemArray = guiReallocMem(control->itemarray, (control->value2 + LISTITEM_GROW_SIZE) * sizeof(ZHANDLE));
				NewItemCutArray = guiReallocMem(control->itemcutarray, (control->value2 + LISTITEM_GROW_SIZE) * sizeof(ZHANDLE));
				newAttributesArray2 = (ITEMATTRIBUTES *) guiReallocMem((ZHANDLE) control->itemattributes2
						, (control->value2 + LISTITEM_GROW_SIZE) * sizeof(ITEMATTRIBUTES));
				if (NewItemArray == NULL || NewItemCutArray == NULL /*|| newAttributesArray == NULL*/
						|| newAttributesArray2 == NULL) {
					if (NewItemArray != NULL) guiFreeMem(NewItemArray);
					if (NewItemCutArray != NULL) guiFreeMem(NewItemCutArray);
					//if (newAttributesArray != NULL) guiFreeMem((BYTE *) newAttributesArray);
					if (newAttributesArray2 != NULL) guiFreeMem((BYTE *) newAttributesArray2);
					guiFreeMem((ZHANDLE) origString);
					guiFreeMem((ZHANDLE) cutString);
					sprintf_s(work, ARRAYSIZE(work), "guiReallocMem returned a null in %s", __FUNCTION__);
					guiaErrorMsg(work, 0);
					return RC_NO_MEM;
				}
				control->itemarray = NewItemArray;
				control->itemcutarray = NewItemCutArray;
				//control->itemattributes = newAttributesArray;
				control->itemattributes2 = newAttributesArray2;
				control->value2 += LISTITEM_GROW_SIZE;
			}
			if (!(control->style & CTRLSTYLE_INSERTORDER)) { /* alphabetical order, find insert position */
				INT temp = (INT)control->inspos;
				guiListBoxAlphaFind(control->itemarray, control->value1, control->changetext, &temp);
				control->inspos = (USHORT)temp;
			}
		}
		switch(control->insstyle) {
		case LIST_INSERTNORMAL:
		case LIST_INSERTBEFORE:
			iMoveCnt = control->value1 - control->inspos + 1;
			if (iMoveCnt > 0) {
				memmove(&((BYTE **) control->itemarray)[control->inspos], &((BYTE **) control->itemarray)[control->inspos - 1], iMoveCnt * sizeof(ZHANDLE));
				memmove(&((BYTE **) control->itemcutarray)[control->inspos], &((BYTE **) control->itemcutarray)[control->inspos - 1], iMoveCnt * sizeof(ZHANDLE));
				//memmove(&((DWORD *) control->itemattributes)[control->inspos], &((DWORD *) control->itemattributes)[control->inspos - 1], iMoveCnt * sizeof(DWORD));
				memmove(&((ITEMATTRIBUTES *) control->itemattributes2)[control->inspos],
						&((ITEMATTRIBUTES *) control->itemattributes2)[control->inspos - 1], iMoveCnt * sizeof(ITEMATTRIBUTES));
			}
			iItemPos = control->inspos - 1;
			if (control->insstyle == LIST_INSERTNORMAL || (control->insstyle == LIST_INSERTBEFORE && insertbeforeold))
				control->inspos++;
			break;
		case LIST_INSERTAFTER:
			iMoveCnt = control->value1 - control->inspos;
			iItemPos = control->inspos;
			if (iMoveCnt > 0) {
				memmove(&((BYTE **) control->itemarray)[control->inspos + 1], &((BYTE **) control->itemarray)[control->inspos], iMoveCnt * sizeof(ZHANDLE));
				memmove(&((BYTE **) control->itemcutarray)[control->inspos + 1], &((BYTE **) control->itemcutarray)[control->inspos], iMoveCnt * sizeof(ZHANDLE));
				//memmove(&((DWORD *) control->itemattributes)[control->inspos + 1], &((DWORD *) control->itemattributes)[control->inspos], iMoveCnt * sizeof(DWORD));
				memmove(&((ITEMATTRIBUTES *) control->itemattributes2)[control->inspos + 1],
						&((ITEMATTRIBUTES *) control->itemattributes2)[control->inspos], iMoveCnt * sizeof(ITEMATTRIBUTES));
			}
			else if (iMoveCnt < 0) iItemPos = control->value1;
			break;
		}
		if (iItemPos < control->value) control->value++;  /* move selected item down one */
		((CHAR **) control->itemarray)[iItemPos] = origString;
		((CHAR **) control->itemcutarray)[iItemPos] = cutString;
		//control->itemattributes[usItemPos] = 0;
		memset(&control->itemattributes2[iItemPos], 0, sizeof(ITEMATTRIBUTES));
		control->value1++;
		break;
	case GUI_DESELECT:
	case GUI_SELECT:
	case GUI_LOCATE:
		if (!control->value2) return 0;  /* no items have been inserted yet */
		if (control->changetext == NULL) return 0;
		if (!(control->style & CTRLSTYLE_INSERTORDER)) {	/* list box is alphabetic */
			iResult = guiListBoxAlphaFind(control->itemarray, control->value1, control->changetext, &iItemPos);
		}
		else {
			iResult = guiListBoxAppendFind(control->itemarray, control->value1, control->changetext, &iItemPos);
		}
		if (iResult < 0) {
			if (iCmd != GUI_LOCATE) control->value = 0;
		}
		else {
			if (control->type != PANEL_MLISTBOX && control->type != PANEL_MLISTBOXHS) {
				control->textval = ((BYTE **) control->itemarray)[iItemPos - 1];
			}
			else if (control->ctlhandle == NULL){
				if (iCmd == GUI_LOCATE)
					for (i1 = 0; i1 < control->value1; i1++) control->itemattributes2[i1].attr1 &= ~(LISTSTYLE_SELECTED << 24);
				if (iCmd == GUI_SELECT || iCmd == GUI_LOCATE)
					control->itemattributes2[iItemPos - 1].attr1 |= LISTSTYLE_SELECTED << 24;
				else if (iCmd == GUI_DESELECT)
					control->itemattributes2[iItemPos - 1].attr1 &= ~(LISTSTYLE_SELECTED << 24);
			}
			control->value = iItemPos;
		}
		break;

	case GUI_TEXTSTYLEDATA:
	case GUI_TEXTCOLORDATA:
		if (control->changetext == NULL) return 0;
		if (!control->value2) return 0;  /* no items have been inserted yet */
		if (!(control->style & CTRLSTYLE_INSERTORDER)) {	/* list box is alphabetic */
			iResult = guiListBoxAlphaFind(control->itemarray, control->value1, control->changetext, &iItemPos);
		}
		else {
			iResult = guiListBoxAppendFind(control->itemarray, control->value1, control->changetext, &iItemPos);
		}
		if (iResult < 0) return 0;
		if (iCmd == GUI_TEXTCOLORDATA) {
			control->itemattributes2[iItemPos - 1].attr1 =
					(control->itemattributes2[iItemPos - 1].attr1 & 0xFF000000) | (control->changecolor);
		}
		else if (iCmd == GUI_TEXTSTYLEDATA) {
			styleint = control->itemattributes2[iItemPos - 1].attr1;
			styleint &= ~(LISTSTYLE_FONTMASK << 24);	/* turn off all old font style bits */
			styleint |= control->changestyle << 24;
			control->itemattributes2[iItemPos - 1].attr1 = styleint;
		}
		break;

	case GUI_TEXTSTYLELINE:
	case GUI_TEXTCOLORLINE:
		iItemPos = control->value3;
		if (iItemPos <= 0 || iItemPos > control->value1) return 0;
		if (iCmd == GUI_TEXTCOLORLINE) {
			/*
			 * the following line is a hack to make the correct color render in guiaOwnerDrawListbox() - SSN
			 * JPR - Removed 05/10/11, no longer necessary. And it was making this behave differently
			 * than the TEXTxxxxxDATA commands above.
			 */
			//if ((DWORD) control->changecolor == GetSysColor(COLOR_WINDOWTEXT)) control->changecolor++;
			control->itemattributes2[iItemPos - 1].attr1 =
				(control->itemattributes2[iItemPos - 1].attr1 & 0xFF000000) | (control->changecolor);
		}
		else if (iCmd == GUI_TEXTSTYLELINE) {
			styleint = control->itemattributes2[iItemPos - 1].attr1;
			styleint &= ~(LISTSTYLE_FONTMASK << 24);	/* turn off all old font style bits */
			styleint |= control->changestyle << 24;
			control->itemattributes2[iItemPos - 1].attr1 = styleint;
		}
		break;

	case GUI_TEXTBGCOLORLINE:
	case GUI_TEXTBGCOLORDATA:
		if (iCmd == GUI_TEXTBGCOLORLINE) {
			iItemPos = control->value3;
			if (iItemPos <= 0 || iItemPos > control->value1) return 0;
		}
		else {
			if (control->changetext == NULL) return 0;
			if (!control->value2) return 0;  /* no items have been inserted yet */
			if (!(control->style & CTRLSTYLE_INSERTORDER)) {	/* list box is alphabetic */
				iResult = guiListBoxAlphaFind(control->itemarray, control->value1, control->changetext, &iItemPos);
			}
			else {
				iResult = guiListBoxAppendFind(control->itemarray, control->value1, control->changetext, &iItemPos);
			}
			if (iResult < 0) return 0;
		}
		if (!control->userBGColor) {
			control->itemattributes2[iItemPos - 1].userBGColor = FALSE;
		}
		else {
			control->itemattributes2[iItemPos - 1].userBGColor = TRUE;
			control->itemattributes2[iItemPos - 1].bgcolor = control->changecolor;
		}
		break;

	case GUI_DESELECTLINE:
	case GUI_SELECTLINE:
	case GUI_LOCATELINE:
		iItemPos = control->value3;
		if (iItemPos > control->value1) return 0;
		control->value = iItemPos;
		if (iItemPos) {
			if (control->type != PANEL_MLISTBOX && control->type != PANEL_MLISTBOXHS) {
				if (iCmd == GUI_DESELECTLINE) control->textval = NULL;
				else control->textval = ((BYTE **) control->itemarray)[iItemPos - 1];
			}
			else if (control->ctlhandle == NULL){
				if (iCmd == GUI_SELECTLINE)
					control->itemattributes2[iItemPos - 1].attr1 |= LISTSTYLE_SELECTED << 24;
				else if (iCmd == GUI_DESELECTLINE)
					control->itemattributes2[iItemPos - 1].attr1 &= ~(LISTSTYLE_SELECTED << 24);
			}
		}
		else {
			if (control->type != PANEL_MLISTBOX && control->type != PANEL_MLISTBOXHS) {
				control->textval = NULL;
			}
		}
		break;
	case GUI_INSERTLINEBEFORE:
		control->inspos = control->value3;
		control->insstyle = LIST_INSERTBEFORE;
		if (!control->value3) control->inspos = 1;
		break;
	case GUI_INSERTLINEAFTER:
		control->inspos = control->value3;
		control->insstyle = LIST_INSERTAFTER;
		if (!control->value3 || control->value3 > control->value1) {
			control->inspos = control->value1 + 1;
			control->insstyle = LIST_INSERTNORMAL;
		}
		break;
	case GUI_LOCATETAB:
		workcontrol = (CONTROL *) res->content;
		for (i1 = 0; i1 < res->entrycount; i1++, workcontrol++) {
			if (workcontrol->tabgroup && (workcontrol->tabgroup == control->tabgroup)) {
				/* store selected tab in case we show this control again */
				workcontrol->selectedtab = control->tabsubgroup - 1;
			}
		}
		break;

		/**
		 * The next two change functions apply only to multi-select listboxes.
		 * We only need to keep our own selection state info if the Win32 widget does not exist.
		 */
	case GUI_DESELECTALL:
	case GUI_SELECTALL:
		if (control->ctlhandle != NULL || (control->type != PANEL_MLISTBOX && control->type != PANEL_MLISTBOXHS)) break;
		if (iCmd == GUI_SELECTALL)
			for (i1 = 0; i1 < control->value1; i1++) control->itemattributes2[i1].attr1 |= LISTSTYLE_SELECTED << 24;
		else
			for (i1 = 0; i1 < control->value1; i1++) control->itemattributes2[i1].attr1 &= ~(LISTSTYLE_SELECTED << 24);
		break;
	}

	/*
	 * The next switch handles the special case of listboxes on a tab that is not currently visible
	 */
//	if (control->ctlhandle != NULL) {
//		switch(iCmd) {
//		case GUI_TEXTCOLORDATA:
//		case GUI_TEXTCOLORLINE:
//		case GUI_TEXTSTYLEDATA:
//		case GUI_TEXTSTYLELINE:
//			if (ISLISTBOX(control)) SendMessage(control->ctlhandle, LB_SETITEMDATA, usItemPos - 1, control->itemattributes[usItemPos - 1]);
//			break;
//		}
//	}

	if (res->winshow == NULL || (!control->tabgroup && !control->shownflag)) {
		return 0;
	}

	/*** The following switch statement does extra things for controls that are shown ***/

	switch(iCmd) {
	case GUI_HELPTEXT:
	    if (!guiaAddTooltip(control)) return RC_ERROR;
		break;
	case GUI_PASTE:
		if (!control->shownflag) break;
		if (control->type != PANEL_PEDIT && control->type != PANEL_PLEDIT) {
			SendMessage(control->ctlhandle, EM_REPLACESEL, FALSE, (LPARAM) control->editbuffer);
		}
		else SendMessage(control->ctlhandle, EM_REPLACESEL, FALSE, (LPARAM) "");
		guiFreeMem(control->editbuffer);
		control->editbuffer = NULL;
		iLen = GetWindowTextLength(control->ctlhandle) + 1;
		if (control->type == PANEL_FEDIT) {
			/* guiaFEditMaskText assumes memory allocated for control->textval is at */
			/* least as large as the length of the mask... memory over-write problems */
			/* occur otherwise - SSN */
			i1 = (INT)strlen((CHAR *)guiMemToPtr(control->mask)) + 1;
			if (iLen < i1) iLen = i1;
		}
		if (control->textval != NULL) {
			control->textval = guiReallocMem(control->textval, iLen);
			if (control->textval == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiReallocMem returned Null in %s at GUI_PASTE", __FUNCTION__);
				guiaErrorMsg(work, iLen);
				return RC_NO_MEM;
			}
		}
		else {
			control->textval = guiAllocMem(iLen);
			if (control->textval == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned Null in %s at GUI_PASTE", __FUNCTION__);
				guiaErrorMsg(work, iLen);
				return RC_NO_MEM;
			}
		}
		iLen = GetWindowText(control->ctlhandle, (LPSTR) control->textval, iLen);
		control->textval[iLen] = '\0';
		break;
	case GUI_SETMAXCHARS:
		if (!control->shownflag) break;
		SendMessage(control->ctlhandle, EM_LIMITTEXT, control->maxtextchars, 0L);
		iLen = GetWindowTextLength(control->ctlhandle);
		if (iLen > control->maxtextchars) {
			iLen++;
			text = guiAllocMem(iLen);
			if (text == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned Null in %s at GUI_SETMAXCHARS", __FUNCTION__);
				guiaErrorMsg(work, iLen);
				return RC_NO_MEM;
			}
			GetWindowText(control->ctlhandle, (CHAR *) text, iLen);
			text[control->maxtextchars] = '\0';
			SetWindowText(control->ctlhandle, (CHAR *) text);
			guiFreeMem(text);
		}
		break;
	case GUI_TEXTCOLOR:
		if (!control->shownflag) break;
		InvalidateRect(control->ctlhandle, NULL, FALSE);
		break;
	case GUI_TEXTFONT:
		if (!control->shownflag) break;
		if (control->type == PANEL_VTEXT || control->type == PANEL_RVTEXT || control->type == PANEL_CVTEXT) {
			i1 = setCtlTextBoxSize(res, control);
			if (i1) return i1;
		}
		PostMessage(control->ctlhandle, WM_SETFONT, (WPARAM) control->font, MAKELPARAM(TRUE, 0));
		break;
	case GUI_TEXTCOLORDATA:
	case GUI_TEXTCOLORLINE:
	case GUI_TEXTSTYLEDATA:
	case GUI_TEXTSTYLELINE:
		if (!control->shownflag) break;
		control->changeflag = TRUE;
		if (control->type == PANEL_DROPBOX) {
			//SendMessage(control->ctlhandle, CB_SETITEMDATA, usItemPos - 1, control->itemattributes[usItemPos - 1]);
			InvalidateRect(control->ctlhandle, NULL, FALSE);
		}
		else if (ISLISTBOX(control)) {
			/* only repaint line with color if it is visible */
			SendMessage(control->ctlhandle, LB_GETITEMRECT, iItemPos - 1, (LPARAM) &rect);
			InvalidateRect(control->ctlhandle, &rect, FALSE);
		}
		control->changeflag = FALSE;
		break;

	/*
	 * The next two change functions apply only to listboxes and dropboxes.
	 * so far.
	 */
	case GUI_TEXTBGCOLORDATA:
	case GUI_TEXTBGCOLORLINE:
		if (!control->shownflag) break;
		control->changeflag = TRUE;
		if (control->type == PANEL_DROPBOX) {
			InvalidateRect(control->ctlhandle, NULL, FALSE);
		}
		else if (ISLISTBOX(control)) {
			SendMessage(control->ctlhandle, LB_GETITEMRECT, iItemPos - 1, (LPARAM) &rect);
			InvalidateRect(control->ctlhandle, &rect, FALSE);
		}
		control->changeflag = FALSE;
		break;

	// This so far applies only to popboxes
	case GUI_TEXTBGCOLOR:
		if (control->type == PANEL_POPBOX) {
			if (control->ctlhandle == NULL) break;
			InvalidateRect(control->ctlhandle, NULL, FALSE);
		}
		else return RC_ERROR;
		break;

	case GUI_INSERTAFTER:
	case GUI_INSERTBEFORE:
	case GUI_INSERTLINEBEFORE:
	case GUI_INSERTLINEAFTER:
		/* Needs to be here for listboxes, to avoid the error at the bottom of this switch */
		break;

	case GUI_MARK:
		if (control->ctlhandle == NULL) break;
		control->changeflag = TRUE;
		if (control->type == PANEL_LOCKBUTTON || control->type == PANEL_ICONLOCKBUTTON) {
			SendMessage(control->ctlhandle, BM_SETSTATE, TRUE, 0L);
		}
		else {
			SendMessage(control->ctlhandle, BM_SETCHECK, BST_CHECKED, 0L);
			if (ISCHECKBOX(control)) InvalidateRect(control->ctlhandle, NULL, FALSE);
		}
		control->changeflag = FALSE;
		break;
	case GUI_UNMARK:
		if (control->ctlhandle == NULL) break;
		control->changeflag = TRUE;
		if (control->type == PANEL_LOCKBUTTON || control->type == PANEL_ICONLOCKBUTTON) {
			SendMessage(control->ctlhandle, BM_SETSTATE, BST_UNCHECKED, 0L);
		}
		else {
			SendMessage(control->ctlhandle, BM_SETCHECK, FALSE, 0L);
			if (ISCHECKBOX(control)) InvalidateRect(control->ctlhandle, NULL, FALSE);
		}
		control->changeflag = FALSE;
		break;
	case GUI_AVAILABLE:
		handleGuiAvailable(control);
		break;
	case GUI_GRAY:
		handleGuiGray(res, control);
		break;
	case GUI_SHOWONLY:
		handleGuiShowonly(res, control);
		break;
	case GUI_READONLY:
		handleGuiReadonly(control);
		break;
	case GUI_NOFOCUS:
		handleGuiNofocus(res, control);
		break;
	case GUI_ERASE:
		if (control->ctlhandle == NULL) break;
		if (ISEDIT(control)) {
			control->changeflag = TRUE;
			SetWindowText(control->ctlhandle, NULL);
			control->changeflag = FALSE;
		}
		else if (ISTEXT(control)) {
			iCmd = GUI_TEXT;
		}
		else {
//			if (PANEL_LISTBOX == control->type || PANEL_MLISTBOX == control->type ||
//					PANEL_LISTBOXHS == control->type || PANEL_MLISTBOXHS == control->type) {
//				nMessage = LB_RESETCONTENT;
//				if (control->listboxtabs == NULL && (control->type == PANEL_MLISTBOXHS || control->type == PANEL_LISTBOXHS)) {
//					/* remove horizontal scroll bar */
//					SendMessage(control->ctlhandle, LB_SETHORIZONTALEXTENT, (WPARAM) 0, (LPARAM) 0);
//					control->maxlinewidth = 0;
//				}
//			}
//			else nMessage = CB_RESETCONTENT;
			// Use SendMessage to send a special message to ourselves.
			// This needs to run on the message thread and we need to wait for it.
			/**
			 * I think it is better to Post this and wait for the other thread to complete
			 * the operation.
			 * Using SendMessage puts it ahead of all other messages that might be in the queue
			 */
			if (eraseDropListControlEvent == NULL) {
				eraseDropListControlEvent = CreateEvent(
						NULL,	/* No need for any security stuff */
						FALSE,	/* Auto-reset */
						FALSE,	/* Initially NOT signaled */
						NULL	/* Don't need a name */
						);
			}
			PostMessage(hwndmainwin, GUIAM_ERASEDROPLISTBOX, 0, (LPARAM)control);
			WaitForSingleObject(eraseDropListControlEvent, INFINITE);
			break;
			//SendMessage(control->ctlhandle, nMessage, 0, 0L);
		}
		if (!ISTEXT(control)) break;
		/* fall through if control type is text */
		// @suppress("No break at end of case")
	case GUI_FEDITMASK:
	case GUI_TEXT:
		if (control->ctlhandle == NULL) break;
		if (control->type == PANEL_POPBOX) {
			control->changeflag = TRUE;
			if (control->textval == NULL) SetWindowText(control->ctlhandle, '\0');
			else {
				string = guiLockMem(control->textval);
				SetWindowText(control->ctlhandle, string);
				guiUnlockMem(control->textval);
			}
			control->changeflag = FALSE;
			InvalidateRect(control->ctlhandle, NULL, TRUE);
		}
		else if (control->type == PANEL_VTEXT || control->type == PANEL_CVTEXT || control->type == PANEL_RVTEXT) {
			control->changeflag = TRUE;
			if (control->textval == NULL) SetWindowText(control->ctlhandle, '\0');
			else {
				i1 = setCtlTextBoxSize(res, control);
				if (i1) return i1;
				string = guiLockMem(control->textval);
				Static_SetText(control->ctlhandle, string);
				//SetWindowText(control->ctlhandle, string);
				guiUnlockMem(control->textval);
			}
			control->changeflag = FALSE;
			InvalidateRect(control->ctlhandle, NULL, TRUE);
		}
		else if (control->type == PANEL_TAB) {
			control->changeflag = TRUE;
			tcitem.mask = TCIF_TEXT;
			string = (control->textval == NULL) ? '\0' : guiLockMem(control->textval);
			tcitem.pszText = (LPSTR) string;
			TabCtrl_SetItem(control->tabgroupctlhandle, control->tabsubgroup - 1, &tcitem);
			if (control->textval != NULL) guiUnlockMem(control->textval);
			control->changeflag = FALSE;

			// Tell Windows to repaint the whole dialog. Otherwise bad things happen
			// Because telling it to repaint just the tab group does not work
			if (res->restype == RES_TYPE_DIALOG) {
				InvalidateRect(res->hwnd, NULL, TRUE);
			}
		}
		else {
			control->changeflag = TRUE;
			string = (control->textval == NULL) ? '\0' : guiLockMem(control->textval);
			if (ISCHECKTYPE(control)) {
				if (guiaGetCtlTextBoxSize(control, (LPSTR) string, &i1, &i2)) return RC_ERROR;
				i1 += i2;  /* add height of control to width (for radio button circle or checkbox box) */
				SetWindowPos(control->ctlhandle, NULL, 0, 0, i1, i2, SWP_NOMOVE | SWP_NOZORDER);
			}
			i1 = SetWindowText(control->ctlhandle, string);
			if (control->textval != NULL) guiUnlockMem(control->textval);
			control->changeflag = FALSE;
			if (!i1) return RC_ERROR;
		}
		if (ISEDIT(control)) {
			control->editinsertpos = 0;
			if (ISMEDIT(control)) control->editendpos = 0;
			else control->editendpos = -1;
			if (ISSINGLELINEEDIT(control) && editCaretOld) {
				guiaSetSingleLineEditCaretPos(control);
			}
			else {
				PostMessage(control->ctlhandle, EM_SETSEL, control->editinsertpos, control->editendpos);
			}
		}
		break;
	case GUI_FOCUS:
		if (control->type == PANEL_TAB) {
			/**
			 * Take the simple approach and just clear focus rectangles from all tabs
			 */
			clearAllTabControlFocus(res->hwnd);
			/*
			 * Select the tab that is getting focus on its header
			 */
//			TabCtrl_SetCurSel(control->tabgroupctlhandle, control->tabsubgroup - 1);
//			workcontrol = (CONTROL *) res->content;
//			for (i1 = 0; i1 < res->entrycount; i1++, workcontrol++) {
//				if (workcontrol->tabgroup == control->tabgroup
//						&& workcontrol->tabsubgroup == control->tabsubgroup)
//				{
//					nmhdr.code = GUIAM_TCN_SELCHANGE_NOFOCUS;
//					nmhdr.hwndFrom = workcontrol->tabgroupctlhandle;
//					//nmhdr.idFrom is not used in our processing of a SELCHANGE
//					// This forces us to create the peers for the controls on the tab
//					SendMessage(res->hwnd, WM_NOTIFY, control->useritem, (LPARAM) &nmhdr);
//					break;
//				}
//			}
			SendMessage(hwndmainwin, GUIAM_SETCTRLFOCUS, 0, (LPARAM) control);
		}
		else if (control->tabsubgroup) {
			/**
			 * Take the simple approach and just clear focus rectangles from all tabs
			 */
			clearAllTabControlFocus(res->hwnd);
			i1 = control->tabsubgroup - 1;
			if (control->tabgroup && (iResult = TabCtrl_GetCurSel(control->tabgroupctlhandle)) != i1) {
				TabCtrl_SetCurSel(control->tabgroupctlhandle, i1);
				nmhdr.code = TCN_SELCHANGE;
				nmhdr.hwndFrom = control->tabgroupctlhandle;
				nmhdr.idFrom = control->useritem;
				SendMessage(res->hwnd, WM_NOTIFY, control->useritem, (LPARAM) &nmhdr);
			}
			SendMessage(hwndmainwin, GUIAM_SETCTRLFOCUS, 0, (LPARAM) control);
		}
		else{
			SendMessage(hwndmainwin, GUIAM_SETCTRLFOCUS, 0, (LPARAM) control);
		}
		break;
	case GUI_DELETE:
		/* fall through */
	case GUI_DELETELINE:
		if (control->ctlhandle == NULL) break;
		if (control->type == PANEL_DROPBOX) nMessage = CB_DELETESTRING;
		else nMessage = LB_DELETESTRING;
		SendMessage(control->ctlhandle, nMessage, iItemPos - 1, (LPARAM) 0);
		if (PANEL_LISTBOXHS == control->type || PANEL_MLISTBOXHS == control->type) {
			if (control->listboxtabs == NULL && SendMessage(control->ctlhandle, LB_GETCOUNT, 0, 0) == 0) {
				/* remove horizontal scroll bar */
				SendMessage(control->ctlhandle, LB_SETHORIZONTALEXTENT, (WPARAM) 0, (LPARAM) 0);
				control->maxlinewidth = 0;
			}
		}
		if (control->value) {
			if (control->type == PANEL_DROPBOX)	nMessage = CB_SETCURSEL;
			else nMessage = LB_SETCURSEL;
			SendMessage(control->ctlhandle, nMessage, control->value - 1, 0L);
		}
		break;

	case GUI_INSERT:
		if (control->ctlhandle == NULL) break;
		if (control->type == PANEL_DROPBOX) {
			i1 = (INT)SendMessage(control->ctlhandle, CB_INSERTSTRING, iItemPos, (LPARAM) origString);
		}
		else {
			i1 = (INT)SendMessage(control->ctlhandle, LB_INSERTSTRING, iItemPos, (LPARAM) cutString);
		}
		if (i1 == CB_ERR || i1 == LB_ERR) return RC_ERROR;
		control->changeflag = TRUE;
		if (control->type == PANEL_LISTBOX || control->type == PANEL_LISTBOXHS) {
			SendMessage(control->ctlhandle, LB_SETCURSEL, control->value - 1, 0L);
		}
		else if (control->type == PANEL_MLISTBOX || control->type == PANEL_MLISTBOXHS) {
			/* DO NOTHING */
		}
		else {
			SendMessage(control->ctlhandle, CB_SETCURSEL, control->value - 1, 0L);
		}
		if (control->type != PANEL_DROPBOX) {
			if (control->listboxtabs == NULL && (control->type == PANEL_MLISTBOXHS || control->type == PANEL_LISTBOXHS)) {
				/* control when horizontal scrollbar appears based on width of inserted text */
				fhdc = GetDC(NULL);
				SelectObject(fhdc, control->font);
				GetTextExtentPoint32(fhdc, cutString, (int)strlen(cutString), &size);
				ReleaseDC(NULL, fhdc);
				if (size.cx > control->maxlinewidth) {
					control->maxlinewidth = size.cx;
					SendMessage(control->ctlhandle, LB_SETHORIZONTALEXTENT, (WPARAM) control->maxlinewidth + 2, (LPARAM) 0);
				}
			}
		}
		control->changeflag = FALSE;
		break;
	case GUI_DESELECTALL:
	case GUI_SELECTALL:
		if ((control->type == PANEL_MLISTBOX || control->type == PANEL_MLISTBOXHS) && control->ctlhandle != NULL) {
			control->changeflag = TRUE;
			if (SendMessage(control->ctlhandle, LB_SETSEL, iCmd != GUI_DESELECTALL, -1) == LB_ERR) return RC_ERROR;
			control->changeflag = FALSE;
		}
		break;
	case GUI_DESELECTLINE:
	case GUI_DESELECT:
	case GUI_SELECT:
	case GUI_SELECTLINE:
	case GUI_LOCATE:
	case GUI_LOCATELINE:
		if (control->ctlhandle == NULL) break;
		control->changeflag = TRUE;
		if (control->type == PANEL_LISTBOX || control->type == PANEL_LISTBOXHS) {
			if (iCmd == GUI_DESELECTLINE) {
				SendMessage(control->ctlhandle, LB_SETCURSEL, -1, 0L);
				control->value = 0;
			}
			else SendMessage(control->ctlhandle, LB_SETCURSEL, control->value - 1, 0L);
		}
		else if (control->type == PANEL_MLISTBOX || control->type == PANEL_MLISTBOXHS) {
			/* must make sure line number is valid, otherwise LB_SETSEL message */
			/* will select or deselect all lines */
			if (control->value - 1 <= control->value1 && control->value - 1 >= 0) {
				if (iCmd == GUI_DESELECTLINE || iCmd == GUI_DESELECT) {
					SendMessage(control->ctlhandle, LB_SETSEL, FALSE, control->value - 1);
				}
				else {
					if (iCmd == GUI_LOCATE || iCmd == GUI_LOCATELINE) {
						SendMessage(control->ctlhandle, LB_SETSEL, FALSE, -1);
					}
					SendMessage(control->ctlhandle, LB_SETSEL, TRUE, control->value - 1);
				}
			}
		}
		else SendMessage(control->ctlhandle, CB_SETCURSEL, control->value - 1, 0L);
		control->changeflag = FALSE;
		break;
	case GUI_RANGE:
		if (control->ctlhandle == NULL) break;
		if (PANEL_PROGRESSBAR == control->type)
			SendMessage(control->ctlhandle, PBM_SETRANGE32,
					(WPARAM)(int)control->value1, (LPARAM)(int)control->value2);
		else SetScrollRange(control->ctlhandle, SB_CTL, control->value1, control->value2, TRUE);
		break;
	case GUI_POSITION:
		if (control->ctlhandle == NULL) break;
		if (PANEL_PROGRESSBAR == control->type)
			SendMessage(control->ctlhandle, PBM_SETPOS, (WPARAM)(int) control->value, 0);
		else SetScrollPos(control->ctlhandle, SB_CTL, control->value, TRUE);
		break;
	case GUI_JUSTIFYLEFT:
	case GUI_JUSTIFYCENTER:
	case GUI_JUSTIFYRIGHT:
		if (control->ctlhandle == NULL) break;
		dwMask = ES_LEFT | ES_RIGHT | ES_CENTER;
		dwStyle = (DWORD)GetWindowLongPtr(control->ctlhandle, GWL_STYLE);
		if (!useEditBoxClientEdge) dwStyle |= WS_BORDER;
		dwStyle &= ~WS_VISIBLE;
		switch(iCmd) {
		case GUI_JUSTIFYLEFT:
			if ((dwStyle & dwMask) == ES_LEFT) return 0;
			dwStyle = (dwStyle & ~dwMask) | ES_LEFT;
			break;
		case GUI_JUSTIFYRIGHT:
			if ((dwStyle & dwMask) == ES_RIGHT) return 0;
			dwStyle = (dwStyle & ~dwMask) | ES_RIGHT;
			break;
		case GUI_JUSTIFYCENTER:
			if ((dwStyle & dwMask) == ES_CENTER) return 0;
			dwStyle = (dwStyle & ~dwMask) | ES_CENTER;
			break;
		}
		control->changeflag = TRUE;
		control->shownflag = FALSE;
		GetWindowRect(control->ctlhandle, &rect);
		i1 = GetWindowTextLength(control->ctlhandle) + 1;
		text = guiAllocMem(i1 * sizeof(CHAR));
		if (text == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned Null in %s at GUI_JUSTIFYx", __FUNCTION__);
			guiaErrorMsg(work, i1 * sizeof(CHAR));
			return RC_NO_MEM;
		}
		GetWindowText(control->ctlhandle, text, i1);
		pt.x = rect.left;
		pt.y = rect.top;
		ScreenToClient((*control->res)->hwnd, &pt);
		ecsExtWinCreate.hwndParent = (*control->res)->hwnd;
		ecsExtWinCreate.dwExStyle = WS_EX_NOPARENTNOTIFY;
		if (useEditBoxClientEdge) ecsExtWinCreate.dwExStyle |= WS_EX_CLIENTEDGE;
		ecsExtWinCreate.lpszClassName = WC_EDIT;
		ecsExtWinCreate.lpszWindowName = text;
		ecsExtWinCreate.dwStyle = dwStyle;
		ecsExtWinCreate.x = pt.x;
		ecsExtWinCreate.y = pt.y;
		ecsExtWinCreate.width = rect.right - rect.left;
		ecsExtWinCreate.height = rect.bottom - rect.top;
		ecsExtWinCreate.hmenu = (HMENU) (INT_PTR) (control->useritem + ITEM_MSG_START);
		ecsExtWinCreate.lpvParam = NULL;
		hControlNew = (HWND) SendMessage(hwndmainwin, GUIAM_CREATEWINDOWEX, (WPARAM) 0, (LPARAM) &ecsExtWinCreate);
		if (hControlNew == NULL) {
			guiaErrorMsg("Failed to create EDIT control", GetLastError());
			guiFreeMem(text);
			return RC_ERROR;
		}
		guiFreeMem(text);

		ShowWindow(control->ctlhandle, SW_HIDE);
		if (control->oldproc != NULL) SetWindowLongPtr(control->ctlhandle, GWLP_WNDPROC, (LONG_PTR) control->oldproc);
		DestroyWindow(control->ctlhandle);

		SetWindowLongPtr(hControlNew, GWLP_USERDATA, (LONG_PTR) control);
		control->ctlhandle = hControlNew;
		control->oldproc = setControlWndProc(control);
		control->shownflag = TRUE;
		control->changeflag = FALSE;
		if (control->font != NULL) {
			SendMessage(control->ctlhandle, WM_SETFONT, (WPARAM) control->font, MAKELPARAM(FALSE, 0));
		}
		ShowWindow(control->ctlhandle, SW_SHOWNORMAL);
		if (control->tooltip != NULL) guiaAddTooltip(control);
		break;
	case GUI_DEFPUSHBUTTON:
		if (!ISPUSHBUTTON(control)) return RC_ERROR;
		if (ISDEFPUSHBUTTON(control)) break;
		win = *res->winshow;
		/* If there is currently showing on this window a default pushbutton, clear it */
		if (win->ctldefault != NULL) clearPBDefStyle(win->ctldefault);
		/* Set the user defined def pushbutton pointer in the window structure */
		win->ctlorgdflt = control;
		setPBDefStyle(control, 3);
		break;
	case GUI_ESCPUSHBUTTON:
		break;
	case GUI_PUSHBUTTON:
		if (!ISPUSHBUTTON(control)) return RC_ERROR;
		if (ISDEFPUSHBUTTON(control)) {
			clearPBDefStyle(control);
			/*
			 * The line below is not needed. As of 4/28/08 this exact same line of code is now
			 * executed in gui.c whether or not the button is visible
			 *
			 * if (res->ctlorgdflt == control - (CONTROL *) res->content + 1) res->ctlorgdflt = 0;
			 */
			win = *res->winshow;
			if (control->shownflag && win->ctlorgdflt == control) win->ctlorgdflt = NULL;
		}
		break;
	case GUI_ICON:
		if (control->ctlhandle == NULL) break;
		resicon = *control->iconres;
		if (control->type == PANEL_VICON) {
			InvalidateRect(control->ctlhandle, NULL, FALSE);
		}
		else {
			SendMessage(control->ctlhandle, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM) resicon->hbmicon);
			InvalidateRect(control->ctlhandle, NULL, FALSE);
		}
		break;

	case GUI_LOCATETAB:
		/*
		 * Setting the focus this way causes the tab to be selected, which
		 * also causes Notify messages for SELCHANGE and SELCHANGING.
		 * We use those to hide/show the correct controls.
		 */
		TabCtrl_SetCurFocus(control->tabgroupctlhandle, control->selectedtab);
//		SendMessage(control->tabgroupctlhandle, TCM_SETCURSEL, control->selectedtab, 0);
//		nmhdr.code = GUIAM_TCN_SELCHANGE_NOFOCUS;
//		nmhdr.hwndFrom = control->tabgroupctlhandle;
//		nmhdr.idFrom = control->useritem;
//		SendMessage(res->hwnd, WM_NOTIFY, control->useritem, (LPARAM) &nmhdr);
		break;

	default:
		return RC_ERROR;
	}
	return 0;
}

/**
 * Handle a change/available command for all panel/dialog widgets except tables
 */
static void handleGuiAvailable(CONTROL *control) {
	if (control->ctlhandle == NULL) return;
	SetWindowLongPtr(control->ctlhandle, GWLP_WNDPROC, (LONG_PTR) guiaControlProc);
	if (ISEDIT(control) && GetWindowLongPtr(control->ctlhandle, GWL_STYLE) & ES_READONLY)
		SendMessage(control->ctlhandle, EM_SETREADONLY, FALSE, 0L);
	EnableWindow(control->ctlhandle, TRUE);
	InvalidateRect(control->ctlhandle, NULL, TRUE);
	/* We do the following in case the tooltip text was changed while we were gray */
	if (control->tooltip != NULL && control->res != NULL) guiaAddTooltip(control);
}

/**
 * Handle a change/gray command for all panel/dialog widgets except tables
 */
static void handleGuiGray(RESOURCE *res, CONTROL *control) {
	if (control->ctlhandle == NULL) return;
	if (ISEDIT(control) && !(GetWindowLongPtr(control->ctlhandle, GWL_STYLE) & ES_READONLY))
		SendMessage(control->ctlhandle, EM_SETREADONLY, TRUE, 0L);
	EnableWindow(control->ctlhandle, FALSE);
	if (res->ctlfocus == control) guiaTabToControl(res, control, TRUE);
	InvalidateRect(control->ctlhandle, NULL, TRUE);
}

/**
 * Handle a change/readonly command for all panel/dialog widgets except tables
 */
static void handleGuiReadonly(CONTROL *control) {
	if (control->ctlhandle == NULL) return;
	if (ISEDIT(control)) SetWindowLongPtr(control->ctlhandle, GWLP_WNDPROC, (LONG_PTR) guiaControlProc);
	else SetWindowLongPtr(control->ctlhandle, GWLP_WNDPROC, (LONG_PTR) guiaControlInactiveProc);
	EnableWindow(control->ctlhandle, TRUE);
	if (ISEDIT(control)) SendMessage(control->ctlhandle, EM_SETREADONLY, TRUE, 0L);
	InvalidateRect(control->ctlhandle, NULL, TRUE);
}

/**
 * Handle a change/showonly command for all panel/dialog widgets except tables
 */
static void handleGuiShowonly(RESOURCE *res, CONTROL *control) {
	if (control->ctlhandle == NULL) return;
	SetWindowLongPtr(control->ctlhandle, GWLP_WNDPROC, (LONG_PTR) guiaControlInactiveProc);
	if (ISEDIT(control)) SendMessage(control->ctlhandle, EM_SETREADONLY, TRUE, 0L);
	EnableWindow(control->ctlhandle, TRUE);
	if (res->ctlfocus == control) guiaTabToControl(res, control, TRUE);
	InvalidateRect(control->ctlhandle, NULL, TRUE);
}

/**
 * Handle a change/nofocus command for all panel/dialog widgets except tables
 */
static void handleGuiNofocus(RESOURCE *res, CONTROL *control) {
	if (control->ctlhandle == NULL) return;
	SetWindowLongPtr(control->ctlhandle, GWLP_WNDPROC, (LONG_PTR) guiaControlInactiveProc);
	EnableWindow(control->ctlhandle, TRUE);
	if (res->ctlfocus == control) guiaTabToControl(res, control, TRUE);
	InvalidateRect(control->ctlhandle, NULL, TRUE);
}

/**
 * Removed 2/28/14 for 16.1.5+
 * jpr
 * I think this was called when a single line box lost focus to scroll
 * text that was too big.
 * But how?
 *
 * If gui.edit.caret=old was used it was called when text was changed from DX code
 *
 * Put back for 16.2.+   04 Mar 2015
 *
 * One customer was relying on this! Dammit!
 */
static void guiaSetSingleLineEditCaretPos(CONTROL *control) {
	if (control->style & CTRLSTYLE_RIGHTJUSTIFIED) {
		SendMessage(control->ctlhandle, EM_SETSEL, 0, -1); // select all
	}
	else if (control->style & CTRLSTYLE_CENTERJUSTIFIED) {
		SendMessage(control->ctlhandle, EM_SETSEL, -1, 0); // deselect all
	}
	else SendMessage(control->ctlhandle, EM_SETSEL, 0, 0);
	SendMessage(control->ctlhandle, EM_SCROLLCARET, 0, 0);
}

static INT guiaGetSelectedTreeitemText(HWND hwnd, CHAR *data, INT datalen) {
	GUIAGETWINTEXT gwtGetWinTextStruct;

	gwtGetWinTextStruct.hwnd = hwnd;
	gwtGetWinTextStruct.title = data;
	gwtGetWinTextStruct.textlen = datalen;

	SendMessage(hwndmainwin, GUIAM_GETSELECTEDTREEITEMTEXT, (WPARAM) 0, (LPARAM) &gwtGetWinTextStruct);
	return (INT) strlen(data);
}

/*
 * return window text of a given control window
 */
INT guiaGetControlText(CONTROL *control, CHAR *text, INT maxlen)
{
	INT len, pos;
	CHAR *p1;

	switch (control->type) {
	case PANEL_EDIT:
	case PANEL_PEDIT:
	case PANEL_MEDIT:
	case PANEL_MEDITHS:
	case PANEL_MEDITVS:
	case PANEL_MEDITS:
	case PANEL_MLEDIT:
	case PANEL_MLEDITHS:
	case PANEL_MLEDITVS:
	case PANEL_MLEDITS:
	case PANEL_PLEDIT:
	case PANEL_LEDIT:
		if (control->ctlhandle != NULL) {
			len = guiaGetWinText(control->ctlhandle, text, maxlen);
			break;
		}
		/* NOTE DROP THROUGH in case widget does not exist */
		// @suppress("No break at end of case")
	case PANEL_BOX:
	case PANEL_BOXTITLE:
	case PANEL_TEXT:
	case PANEL_RTEXT:
	case PANEL_VTEXT:
	case PANEL_RVTEXT:
	case PANEL_CVTEXT:
	case PANEL_CTEXT:
	case PANEL_FEDIT:
		if (control->textval == NULL) len = 0;
		else {
			len = (INT)strlen((CHAR *) control->textval);
			if (len > maxlen) len = maxlen;
			memcpy(text, control->textval, len * sizeof(CHAR));
		}
		break;
	case PANEL_TREE:
		len = guiaGetSelectedTreeitemText(control->ctlhandle, text, maxlen);
		break;
	case PANEL_MLISTBOX:
	case PANEL_MLISTBOXHS:
	case PANEL_LISTBOX:
	case PANEL_LISTBOXHS:
	case PANEL_DROPBOX:
		/* copy string number control->value from control->textval */
		p1 = (CHAR *) control->textval;
		if (p1 == NULL || control->value >= control->value1) len = 0;
		else {
			for (len = 0, pos = 0; len < control->value; len++) {
				while(p1[pos++]);
			}
			len = (INT)strlen(&p1[pos]);
			memcpy(text, &p1[pos], len);
		}
		break;
	default:
		len = 0;
	}
	return(len);
}

/*
 * return the control's width and height in pixels
 */
INT guiaGetControlSize(CONTROL *control, INT *width, INT *height)
{
	RECT rect;

	GetWindowRect(control->ctlhandle, &rect);
	*width = rect.right - rect.left;
	*height = rect.bottom - rect.top;
	return 0;
}

/*
 * call window callback routine
 */
static void wincb(WINDOW *win, CHAR *function, INT datasize)
{
	if (win->cbfnc != NULL) {
		memcpy(cbmsgname, win->name, 8);
		if (function != NULL) memcpy(cbmsgfunc, function, 4);
		memset(cbmsgid, ' ', 5);
		(*win->cbfnc)(win->thiswinhandle, cbmsg, datasize + 17);
	}
}

/*
 * call resource callback routine
 */
void rescb(RESOURCE *res, CHAR *function, INT id, INT datasize)
{
	if (res->cbfnc != NULL) {
		memcpy(cbmsgname, res->name, 8);
		if (function != NULL) memcpy(cbmsgfunc, function, 4);
		itonum5(id, cbmsgid);
		(*res->cbfnc)(res->thisreshandle, cbmsg, datasize + 17);
	}
}

/*
 * tab to control by ALT key + alpha char
 */
static int guiaAltKeySetFocus(RESOURCE *res, INT key)
{
	INT i1, i2, iEndPos, iInsertPos;
	BYTE *p1;
	CONTROL *ctl, *tcontrol;
	TCITEM tcitem;

	if (res == NULL || res->winshow == NULL) return RC_ERROR;
	if (res->restype == RES_TYPE_PANEL) res = *(*res->winshow)->showreshdr;
	else if (res->restype != RES_TYPE_DIALOG) return RC_ERROR;

nextres:
/* process this panel or dialog resource */
	ctl = (CONTROL *) res->content;
	for (i1 = 0; i1 < res->entrycount; i1++, ctl++) {
		if (!ctl->shownflag) continue;
		if (ISGRAY(ctl) || ISSHOWONLY(ctl) || ISNOFOCUS(ctl) || ctl->textval == NULL) continue;
		if (ctl->type == PANEL_TAB || ISTEXT(ctl) || ISCHECKTYPE(ctl) || ISPUSHBUTTON(ctl)) {
			i2 = TabCtrl_GetItemCount(ctl->ctlhandle);
			if (ctl->type == PANEL_TAB) {
				tcitem.mask = TCIF_PARAM;
				while(i2--) {
					TabCtrl_GetItem(ctl->ctlhandle, i2, &tcitem);
					tcontrol = (CONTROL *) tcitem.lParam;
					p1 = tcontrol->textval;
					while (*p1) {
						if (*p1++ == '&' && *p1) {
							if (*p1 == '&') p1++;
							else if (key == toupper(*p1)) goto found;
							else break;
						}
					}
				}
			}
			else {
				p1 = ctl->textval;
				while (*p1) {
					if (*p1++ == '&' && *p1) {
						if (*p1 == '&') p1++;
						else if (key == toupper(*p1)) goto found;
						else break;
					}
				}
			}
		}
	}
/* process multiple panels shown on a window */
	if (res->restype == RES_TYPE_PANEL) {
		while (res->showresnext != NULL) {
			res = *res->showresnext;
			if (res->restype == RES_TYPE_PANEL) goto nextres;
		}
	}
	return RC_ERROR;

/* character was found */
found:
	if (ISTEXT(ctl)) {
		if (++i1 < res->entrycount) ctl++;
		else if (res->restype != RES_TYPE_PANEL) return RC_ERROR;
		else {
			while (TRUE) {
				if (res->showresnext == NULL) return RC_ERROR;
				res = *res->showresnext;
				if (res->restype == RES_TYPE_PANEL) break;
			}
			i1 = 0;
			ctl = (CONTROL *) res->content;
		}
		if (!ISINTERACTIVE(ctl) || ISGRAY(ctl) || ISSHOWONLY(ctl) || ISNOFOCUS(ctl)) return RC_ERROR;
	}
	if (ctl->tabgroup) {
		if ((i1 = TabCtrl_GetCurSel(ctl->ctlhandle)) != i2) {
			/* user is using key to switch to a different tab */
			guiaChangeControl(res, tcontrol, GUI_FOCUS);
			return 0;
		}
		else guiaChangeControl(res, ctl, GUI_FOCUS);
	}
	else {
		SetFocus(ctl->ctlhandle);
	}

	if (ISPUSHBUTTON(ctl)){
		PostMessage(ctl->ctlhandle, BM_CLICK, 0, 0);
	}
	else if (ISEDIT(ctl)) {
		SendMessage(ctl->ctlhandle, EM_GETSEL, (WPARAM) &iInsertPos, (LPARAM) &iEndPos);
		if (iInsertPos != ctl->editinsertpos || iEndPos != ctl->editendpos) {
			i1 = sendESELMessage(ctl, iInsertPos, iEndPos);
			if (i1 != 0) return i1;
		}
	}
	else if (ISCHECKBOX(ctl)) {
		if (ctl->style & CTRLSTYLE_MARKED) {
			ctl->style &= ~CTRLSTYLE_MARKED;
			PostMessage(ctl->ctlhandle, BM_SETCHECK, BST_UNCHECKED, 0);
			cbmsgdata[0] = 'N';
		}
		else {
			ctl->style |= CTRLSTYLE_MARKED;
			PostMessage(ctl->ctlhandle, BM_SETCHECK, BST_CHECKED, 0);
			cbmsgdata[0] = 'Y';
		}
		/*
		 * We changed checkboxes to OwnerDraw for 16.1.2
		 * The above messages to change the checked state of the control SHOULD
		 * cause Windows to send us a WM_DRAWITEM but it does not.
		 * So we force a redraw here.
		 */
		InvalidateRect(ctl->ctlhandle, NULL, FALSE);
		if (res->itemmsgs){
			rescb(res, DBC_MSGITEM, ctl->useritem, 1);
		}
	}
	return 0;
}

/**
 * tab to next (or previous) control
 */
void guiaTabToControl(RESOURCE *res, CONTROL *pctl, INT fTabForward)
{
/* define states used with two Finite state machines below */
#define TABSTATE_START 0      /* start state */
#define TABSTATE_RBS 1        /* initially on radio button (set) */
#define TABSTATE_RBC 2        /* initially on radio button (cleared) */
#define TABSTATE_OTHER 3      /* initially on anything other than RBS or RBC */
#define TABSTATE_RBS_LRBC 4   /* initially on radio button (set), followed by a last radio button control (cleared)     */
#define TABSTATE_OTHER_RBC 5  /* initially on anything other than RBS or RBC, followed by RBC */
#define TABSTATE_FINISH 6     /* in a final accept state */

/* define token types (control types) used to direct machines */
#define TYPE_RBS 0            /* radio button (selected) */
#define TYPE_RBC 1            /* read button (cleared) */
#define TYPE_LRBS 2           /* last radio button (selected) */
#define TYPE_LRBC 3           /* last radio button (cleared) */
#define TYPE_STATIC 4         /* static control (not a control that can contain a caret) */
#define TYPE_OTHER 5

	RESOURCE *pResource, *pSavResource, *pTmpRes;
	HWND frontwin;
	INT iState, iInsertPos, iEndPos;
	INT iType, iSavType, iCtlNum, iSavCtlNum, iCtlRef;
	CONTROL *pControl, *pSavControl, *pctlBase;
	INT iCtlsave, inrightradiogroup, n1;

	pctlBase = pControl = (CONTROL *) res->content;
	if (pControl == NULL) return;
	pResource = res;
	/* Tab to the next focusable control after pctl */
	iCtlRef = iCtlNum = pctl->ctloffset;
	iState = TABSTATE_START;

	do {
/* determine control type, store in iType */
		switch (pControl[iCtlNum].type) {
		case PANEL_BOX:
		case PANEL_BOXTITLE:
		case PANEL_TEXT:
		case PANEL_VTEXT:
		case PANEL_CVTEXT:
		case PANEL_CTEXT:
		case PANEL_RTEXT:
		case PANEL_RVTEXT:
			iType = TYPE_STATIC;
			break;
		case PANEL_RADIOBUTTON:
			if (ISSHOWONLY(&pControl[iCtlNum]) || ISNOFOCUS(&pControl[iCtlNum])) {
				iType = TYPE_STATIC;
			}
			else if (ISMARKED(&pControl[iCtlNum])) iType = TYPE_RBS;
			else iType = TYPE_RBC;
			break;
		case PANEL_LASTRADIOBUTTON:
			if (ISSHOWONLY(&pControl[iCtlNum]) || ISNOFOCUS(&pControl[iCtlNum])) {
				iType = TYPE_STATIC;
			}
			else if (ISMARKED(&pControl[iCtlNum])) iType = TYPE_LRBS;
			else iType = TYPE_LRBC;
			break;
		case PANEL_TABLE:
			iType = guiaIsTableFocusable(&pControl[iCtlNum]) ? TYPE_OTHER : TYPE_STATIC;
			break;
		default:
			if (ISSHOWONLY(&pControl[iCtlNum]) || ISNOFOCUS(&pControl[iCtlNum]) || !pControl[iCtlNum].shownflag) {
				iType = TYPE_STATIC;
			}
			else iType = TYPE_OTHER;
			break;
		}

/* finite state machines, state stored in iState, iType contains next token */
		if (fTabForward) {  /* TAB to next control */
			switch (iState) {
			case TABSTATE_START:
				switch (iType) {
				case TYPE_RBS:
					iState = TABSTATE_RBS;
					break;
				case TYPE_RBC:
					iState = TABSTATE_RBC;
					break;
				default:
					iState = TABSTATE_OTHER;
				}
				break;
			case TABSTATE_RBS:
				switch (iType) {
				case TYPE_RBC:
					break;
				case TYPE_LRBC:
					iState = TABSTATE_RBS_LRBC;
					break;
				default:
					iState = TABSTATE_FINISH;
				}
				break;
			case TABSTATE_RBC:
				iState = TABSTATE_FINISH;
				break;
			case TABSTATE_OTHER:
				if (TYPE_RBC == iType) {
					iState = TABSTATE_OTHER_RBC;
					pSavResource = pResource;     /* SAVE */
					pSavControl = pControl;
					iSavCtlNum = iCtlNum;
					iSavType = iType;
				}
				else iState = TABSTATE_FINISH;
				break;
			case TABSTATE_RBS_LRBC:
				if (TYPE_RBC == iType) {
					iState = TABSTATE_OTHER_RBC;
					pSavResource = pResource;     /* SAVE */
					pSavControl = pControl;
					iSavCtlNum = iCtlNum;
					iSavType = iType;
				}
				else iState = TABSTATE_FINISH;
				break;
			case TABSTATE_OTHER_RBC:
				switch (iType) {
				case TYPE_RBC:
					break;
				case TYPE_RBS:
				case TYPE_LRBS:
					iState = TABSTATE_FINISH;
					break;
				default:
					pResource = pSavResource;     /* RESTORE */
					pControl = pSavControl;
					iCtlNum = iSavCtlNum;
					iState = TABSTATE_FINISH;
					iType = iSavType;
				}
				break;
			}
		}
		else {  /* SHIFT-TAB to previous control */
			switch (iState) {
			case TABSTATE_START:
				switch (iType) {
					case TYPE_RBS:
					case TYPE_LRBS:
						iState = TABSTATE_RBS;
						break;
					case TYPE_RBC:
					case TYPE_LRBC:
						iState = TABSTATE_RBC;
						break;
					default:
						iState = TABSTATE_OTHER;
				}
				break;
			case TABSTATE_RBS:
				switch (iType) {
					case TYPE_RBC:
						break;
					case TYPE_LRBC:
						iState = TABSTATE_OTHER_RBC;
						pSavResource = pResource;     /* SAVE */
						pSavControl = pControl;
						iSavCtlNum = iCtlNum;
						iSavType = iType;
						break;
					default:
						iState = TABSTATE_FINISH;
				}
				break;
			case TABSTATE_RBC:
				iState = TABSTATE_FINISH;
				break;
			case TABSTATE_OTHER:
				if (TYPE_RBC == iType || TYPE_LRBC == iType) {
					iState = TABSTATE_OTHER_RBC;
					pSavResource = pResource;     /* SAVE */
					pSavControl = pControl;
					iSavCtlNum = iCtlNum;
					iSavType = iType;
				}
				else iState = TABSTATE_FINISH;
				break;
			case TABSTATE_OTHER_RBC:
				switch (iType) {
					case TYPE_RBC:
						break;
					case TYPE_RBS:
						iState = TABSTATE_FINISH;
						break;
					default:
						pResource = pSavResource;     /* RESTORE */
						pControl = pSavControl;
						iCtlNum = iSavCtlNum;
						iType = iSavType;
						iState = TABSTATE_FINISH;
				}
				break;
			}
		}

/* check to see if we are done */
		if (TABSTATE_FINISH == iState) {
			if (TYPE_STATIC == iType || ISGRAY(&pControl[iCtlNum])) {
				iState = TABSTATE_START;
				if (iCtlNum != iCtlRef || pControl != pctlBase) continue;
			}
			break;
		}

		if (fTabForward) {  /* TAB to next control */
			iCtlNum++;
			if (iCtlNum >= pResource->entrycount) {
				pResource->ctlfocus = NULL; /* reset ctlfocus since we are moving on and no matches were found - SSN */
				if (RES_TYPE_PANEL == res->restype) {
					if (pResource->showresnext == NULL) {
						pResource = *(*pResource->winshow)->showreshdr;
					}
					else pResource = *pResource->showresnext;
					pControl = (CONTROL *) pResource->content;
					iCtlNum = 0;
				}
				else iCtlNum = 0;
			}
		}
		else {
			iCtlNum--;
			if (iCtlNum < 0) {
				pResource->ctlfocus = NULL; /* reset ctlfocus since we are moving on and no matches were found - SSN */
				if (RES_TYPE_PANEL == res->restype) {
					pTmpRes = *(*pResource->winshow)->showreshdr;
					if (pTmpRes == pResource) {
						while (pTmpRes->showresnext != NULL) {
							pTmpRes = *pTmpRes->showresnext;
						}
					}
					else {
						while (*pTmpRes->showresnext != pResource) {
							pTmpRes = *pTmpRes->showresnext;
						}
					}
					pResource = pTmpRes;
					pControl = (CONTROL *) pResource->content;
					iCtlNum = pResource->entrycount - 1;
				}
				else iCtlNum = pResource->entrycount - 1;
			}
		}
	} while ((iCtlNum != iCtlRef || pControl != pctlBase) && pControl != NULL);
	if (pControl == NULL) return;

	/**
	 * If were are tabbing off of the last focusable control on a tab item,
	 * move focus to the tab heading
	 */
	if (pctl->tabgroup && pctl->type != PANEL_TAB) /* We are coming fm a control on a tab */ {
		CONTROL *testCtrl = findNextFocusableControl(pctl);
		INT selectedTabItemIndex = TabCtrl_GetCurSel(pctl->tabgroupctlhandle) + 1;
		if (testCtrl == NULL || testCtrl->tabgroup != pctl->tabgroup || testCtrl->tabsubgroup != selectedTabItemIndex) {
			 /* We ARE trying to tab off the last focusable control on this tabItem */
			res = *pctl->res;
			pctlBase = (CONTROL *) res->content;
			for (INT localControlIndex = 0; localControlIndex < res->entrycount; localControlIndex++, pctlBase++) {
				if (pctlBase->type == PANEL_TAB && pctlBase->tabgroup == pctl->tabgroup
						&& pctlBase->tabsubgroup == selectedTabItemIndex) {
					drawTabHeaderFocusRectangle(pctlBase, TRUE);
					iCtlNum = pctlBase->ctloffset;
					pControl = (CONTROL *) res->content;
					goto dothefocusthing;
				}
			}
			// If we get here, something has gone seriously wrong
			sprintf(lastErrorMessage, "In %s, failed to find tab header CONTROL while in a tab item", __FUNCTION__);
			guiaErrorMsg(lastErrorMessage, 1798);
			goto exit;
		}
	}

	/*
	 * If tabbing into a TABGROUP from outside it,
	 * special consideration is required.
	 */
	if (pctl->tabgroup == 0 && pControl[iCtlNum].tabgroup != 0) {
		INT selectedTabItemIndex = -1; // This will be zero based from Windows
		INT tabgroup = pControl[iCtlNum].tabgroup;
		INT localControlIndex = 0;
		CONTROL *tabHeaderControl, *testCtrl;
		selectedTabItemIndex = TabCtrl_GetCurSel(pControl[iCtlNum].tabgroupctlhandle);
		// First, let's find the Control that is the tab header thing for the selected tab
		res = *pControl[iCtlNum].res;
		pctlBase = (CONTROL *) res->content;
		selectedTabItemIndex++;		// Change to one-based for our use.
		for (; localControlIndex < res->entrycount; localControlIndex++, pctlBase++) {
			if (pctlBase->type == PANEL_TAB && pctlBase->tabgroup == tabgroup
					&& pctlBase->tabsubgroup == selectedTabItemIndex)
				break;
		}
		if (localControlIndex == res->entrycount) {
			// If we get here, something has gone seriously wrong
			sprintf(lastErrorMessage, "In %s, failed to find tab header CONTROL while entering tabgroup", __FUNCTION__);
			guiaErrorMsg(lastErrorMessage, 1798);
			goto exit;
		}
		tabHeaderControl = pctlBase++;
		localControlIndex++;
		// Now find the 1st focusable control in here
		// If none, then we put focus on the header
		testCtrl = findNextFocusableControl(tabHeaderControl);
		if (testCtrl == NULL || testCtrl->tabgroup != tabgroup || testCtrl->tabsubgroup != selectedTabItemIndex) {
			drawTabHeaderFocusRectangle(tabHeaderControl, TRUE);
			iCtlNum = tabHeaderControl->ctloffset;
		}
		else iCtlNum = testCtrl->ctloffset;
		goto dothefocusthing;
	}

	/**
	 * Focus was on a tab header and the next control is not in a tab group
	 */
	if (pctl->tabgroup && pctl->type == PANEL_TAB && pControl[iCtlNum].tabgroup == 0) {
		drawTabHeaderFocusRectangle(pctl, FALSE);
	}

dothefocusthing:
	frontwin = GetForegroundWindow();
	if (GetCurrentThreadId() == GetWindowThreadProcessId(pControl[iCtlNum].ctlhandle, NULL)) {
		if (ISGRAY(&pControl[iCtlNum])) {
			SetFocus(NULL);
		}
		else {
			SetFocus(pControl[iCtlNum].ctlhandle);
			if (pControl[iCtlNum].type == PANEL_TAB) {
				drawTabHeaderFocusRectangle(&pControl[iCtlNum], TRUE);
			}
		}
	}
	else {
		if (ISGRAY(&pControl[iCtlNum])) SendMessage(hwndmainwin, GUIAM_SETCTRLFOCUS, (WPARAM) 0, (LPARAM) NULL);
		else {
			SendMessage(hwndmainwin, GUIAM_SETCTRLFOCUS, (WPARAM) 0, (LPARAM) &pControl[iCtlNum]);
		}
	}
	if (frontwin != GetForegroundWindow()) SetForegroundWindow(frontwin);

	if (ISEDIT(&pControl[iCtlNum])) {
		SendMessage((&pControl[iCtlNum])->ctlhandle, EM_GETSEL, (WPARAM) &iInsertPos, (LPARAM) &iEndPos);
		if (iInsertPos != (&pControl[iCtlNum])->editinsertpos || iEndPos != (&pControl[iCtlNum])->editendpos) {
			n1 = sendESELMessage(&pControl[iCtlNum], iInsertPos, iEndPos);
			if (n1 != 0) return;
		}
	}

	goto exit;

	/* set to next control outside tabgroup if last control in tabgroup */
	if (pctl->tabgroup && pctl->type != PANEL_TAB && pctlBase[iCtlNum].tabsubgroup != pctlBase[iCtlRef].tabsubgroup) {
		res = *pctl->res;
		pctlBase = (CONTROL *) res->content;
		iCtlNum = 0;
		for (iCtlsave = 0; iCtlsave < res->entrycount; iCtlsave++, pctlBase++) {
			if (pctlBase->type != PANEL_TAB
					&& pctlBase->tabgroup == pctl->tabgroup
					&& pctlBase->tabsubgroup == pctl->tabsubgroup && pctlBase->ctloffset > iCtlNum)
			{
				iCtlNum = pctlBase->ctloffset;
			}
		}
		pctlBase = (CONTROL *) res->content;
		inrightradiogroup = FALSE;
		if (pctlBase[iCtlNum].type == PANEL_LASTRADIOBUTTON) {
			for (iInsertPos = pctlBase[iCtlNum].value1; iInsertPos > 0; iInsertPos--) {
				if (iCtlNum - iInsertPos == pctl->ctloffset) {
					inrightradiogroup = TRUE;
					break;
				}
			}
		}
		if (iCtlNum == pctl->ctloffset || inrightradiogroup) {
			n1 = -1;
			for (iCtlsave = 0; iCtlsave < res->entrycount; iCtlsave++, pctlBase++) {
				/* find tabgroup in resource linked list */
				if (n1 < 0 && pctlBase->type == PANEL_TAB && pctlBase->tabgroup == pctl->tabgroup) {
					/* found at position n1 */
					n1 = pctlBase->tabgroup;
				}
				else if (n1 >= 0 && (pctlBase->tabgroup != n1)) {
					if (ISGRAY(pctlBase) || ISSHOWONLY(pctlBase) || !ISINTERACTIVE(pctlBase) || ISNOFOCUS(pctlBase)) {
						continue;
					}
					res->ctlfocus = pctlBase;
					if (res->focusmsgs) {
						rescb(res, DBC_MSGLOSEFOCUS, pctl->useritem, 0);
						rescb(res, DBC_MSGFOCUS, pctlBase->useritem, 0);
					}
					SetFocus(pctlBase->ctlhandle);
					break;
				}
			}
		}
	}

exit:
	return;
}

static CONTROL* findNextFocusableControl(CONTROL *startCtl) {
	RESOURCE *res = *startCtl->res;
	CONTROL *pctlBase = (CONTROL *) res->content;
	INT i1 = startCtl->ctloffset;
	i1++;  // start looking at the next control on the resource
	pctlBase += i1;
	for (; i1 < res->entrycount; i1++, pctlBase++) {
		if (pctlBase->type == PANEL_TAB) return pctlBase;
		if (ISGRAY(pctlBase)) continue;
		if (ISSHOWONLY(pctlBase) || ISNOFOCUS(pctlBase) || !pctlBase->shownflag) continue;
		if (!ISINTERACTIVE(pctlBase)) continue;
		return pctlBase;
	}
	return NULL;
}

/**
 * Decides if a table can accept focus.
 * It must have at least one focusable column and at least one row.
 * Oh, and it must be visible.
 */
static BOOL guiaIsTableFocusable(CONTROL *control) {
	BOOL retval = FALSE;
	UINT i1;
	PTABLECOLUMN tblColumns;

	if (!control->shownflag) return FALSE;
	if (control->table.numrows >= 1) {
		tblColumns = guiLockMem(control->table.columns);
		for (i1 = 0; i1 < control->table.numcolumns; i1++) {
			if (ISCOLUMNFOCUSABLE(tblColumns[i1].type)) {
				retval = TRUE;
				break;
			}
		}
		guiUnlockMem(control->table.columns);
	}
	return retval;
}

/*
 * format the source text through the format mask storing the result in Dest.
 */
void guiaFEditMaskText(CHAR *source, CHAR *mask, CHAR *dest)
{
	INT i1, i2, i3, lft, negate, rgt, zeroflg;

	if (isnummask(mask, &lft, &rgt)) {  /* numeric format */
		while (*mask != 'Z' && *mask != '9' && *mask != '.') {
			if (*mask == '\\') mask++;
			if (*source == *mask) source++;
			*(dest++) = *(mask++);
		}

		while (*source == ' ') source++;
		negate = 0;
		if (*source == '-') {
			source++;
			if (*mask == 'Z') negate = 1;  /* must have a 'Z' format to support negatives */
		}
		while (*source == '0') source++;
		zeroflg = TRUE;
		for (i1 = i2 = 0; isdigit((INT)(BYTE)source[i1]); i1++)
			if (source[i1] != '0') zeroflg = FALSE;
		if (source[i1] == '.') {
			for (i3 = i1 + 1; i2 < rgt && isdigit((INT)(BYTE)source[i3]); i2++, i3++)
				if (source[i3] != '0') zeroflg = FALSE;
		}
		if (zeroflg) negate = 0;

		if (i1 + negate < lft) {
			do {
				if (*(mask++) == 'Z') {
					if (negate && *mask != 'Z') {
						negate = 0;
						*dest = '-';
					}
					else *dest = ' ';
				}
				else *dest = '0';
				dest++;
			} while (i1 + negate < --lft);
		}
		else if (i1 + negate > lft) {
			negate = 0;
			source += i1 - lft;
			i1 = lft;
		}
		if (negate) *(dest++) = '-';
		if (i1) {
			memcpy(dest, source, i1);
			dest += i1;
			source += i1;
		}
		if (rgt) {
			*(dest++) = '.';
			if (i2) {
				source++;
				memcpy(dest, source, i2);
				dest += i2;
				rgt -= i2;
			}
			if (rgt) {
				memset(dest, '0', rgt);
				dest += rgt;
			}
		}
	}
	else {  /* non-numeric format */
		while (*mask) {
			if (*mask == '9' || *mask == 'Z') {  /* start of numeric field */
				for (i1 = 1; mask[i1] == '9' || mask[i1] == 'Z'; i1++);
				for (i2 = 0; isdigit((INT)(BYTE)source[i2]) || source[i2] == ' '; i2++);
				if (i1 > i2) {
					for (i3 = i1 - i2; i3--; ) {
						if (*(mask++) == '9') *dest = '0';
						else *dest = ' ';
						dest++;
					}
				}
				else if (i1 < i2) i2 = i1;
				while (i2--) {
					if (*(mask++) == '9' && *source == ' ') *dest = '0';
					else *dest = *source;
					dest++;
					source++;
				}
			}
			else if (*mask == 'A' || *mask == 'U' || *mask == 'L') {  /* any char */
				if (*source) {
					if (*mask == 'U') *dest = (CHAR) toupper(*source);
					else if (*mask == 'L') *dest = (CHAR) tolower(*source);
					else *dest = *source;
					dest++;
					source++;
				}
				mask++;
			}
			else {
				if (*mask == '\\') {
					if (!*(++mask)) break;
				}
				if (*source == *mask) source++;
				*(dest++) = *(mask++);
			}
		}
	}
	*dest = 0;
}

/*
 * format the source text through the format mask (storing the result in Dest)
 * while inserting a new character and updating the insert position.
 * NOTE: this routine assumes that the storage for dest is at least as long
 * as the length of the string plus one.
 */
static void guiaFEditInsertChar(CHAR *source, CHAR *mask, CHAR *dest, INT newchar, INT *insertpos)
{
	INT i1, i2, lft, rgt, pos;
	INT adjust, destpos, extrazeros, state;
	INT spaces, negate, zeros, left, period, right;
	INT savestate, savezeros, saveleft, saveright;
	INT digitcnt, anycnt, speccnt;
	CHAR c1, work[256], *savesource;

	savesource = source;
	pos = *insertpos;
	if (isnummask(mask, &lft, &rgt)) {  /* number mask */
		adjust = 1;
		savestate = 7;
		spaces = negate = zeros = left = period = right = 0;
		state = destpos = extrazeros = 0;
		for (i1 = 0; ; ) {
			if (i1 < pos) c1 = source[i1];
			else if (i1 == pos) {
				savestate = state;
				savezeros = zeros;
				saveleft = left;
				saveright = right;
				c1 = (CHAR) newchar;
			}
			else c1 = source[i1 - 1];
			if (!c1) break;

			switch (state) {
			case 0:
				if (*mask == 'Z' || *mask == '9' || *mask == '.') {
					state = 1;
					continue;
				}
				if (*mask == '\\') mask++;
				if (c1 != *mask) {
					if (i1 <= pos) {
						dest[destpos++] = *mask;
						adjust++;
					}
					mask++;
					continue;
				}
				dest[destpos++] = *(mask++);
				break;
			case 1:
				if (c1 != ' ') {
					state = 2;
					continue;
				}
				spaces++;
				break;
			case 2:
				state = 3;
				if (c1 != '-') continue;
				if (*mask != 'Z') goto guiaFEditInsertCharX;
				negate++;
				break;
			case 3:
				if (c1 != '0') {
					state = 4;
					continue;
				}
				zeros++;
				break;
			case 4:
				if (!isdigit((INT)(BYTE)c1)) {
					state = 5;
					continue;
				}
				work[left++] = c1;
				break;
			case 5:
				state = 6;
				if (c1 != '.') goto guiaFEditInsertCharX;
				period++;
				break;
			case 6:
				if (!isdigit((INT)(BYTE)c1)) goto guiaFEditInsertCharX;
				if (c1 == '0') extrazeros++;
				else extrazeros = 0;
				work[left + right++] = c1;
				break;
			}
			i1++;
		}

		switch (savestate) {
		case 1:  /* inserting a leading space */
		case 2:  /* inserting a negative sign */
		case 3:  /* inserting a leading zero */
		case 4:  /* inserting a left digit */
			if ((spaces + negate + zeros + left) > lft) {
				i1 = (spaces + negate + zeros + left) - lft;
				i2 = left;
				if (savestate <= 3) {
					if (savestate == 3) i2 -= savezeros + 1;
					i2 += zeros;
				}
				else i2 -= saveleft + 1;
				if (i2 && rgt && !period) {
					if (i2 > rgt) i2 = rgt;
					if (i2 > i1) i1 = 0;
					else i1 -= i2;
				}
				if (savestate == 1) {
					if (i1) goto guiaFEditInsertCharX;
				}
				else {
					if (i1 > spaces) {
						if (savestate == 3) goto guiaFEditInsertCharX;
						i1 -= spaces;
						adjust -= spaces;
						spaces = 0;
						if (i1 > zeros) goto guiaFEditInsertCharX;
						if (savestate == 4) adjust -= i1;
						zeros -= i1;
					}
					else {
						spaces -= i1;
						adjust -= i1;
					}
				}
			}
			break;
		case 5:  /* inserting a period */
			if (!rgt) goto guiaFEditInsertCharX;
			if (right > rgt) right = rgt;
			break;
		case 6:  /* inserting a right digit */
			if (saveright >= rgt) goto guiaFEditInsertCharX;
			if (right > rgt) {
				if (right - rgt > extrazeros) goto guiaFEditInsertCharX;
				right = rgt;
			}
			break;
		case 7:  /* insert never happened */
			goto guiaFEditInsertCharX;
		}

		if (spaces) {
			memset(&dest[destpos], ' ', spaces);
			destpos += spaces;
		}
		if (negate) dest[destpos++] = '-';
		if (zeros) {
			memset(&dest[destpos], '0', zeros);
			destpos += zeros;
		}
		if (left) {
			memcpy(&dest[destpos], work, left);
			destpos += left;
		}
		if (period) {
			dest[destpos++] = '.';
			if (right) {
				memcpy(&dest[destpos], &work[left], right);
				destpos += right;
			}
		}
		*insertpos += adjust;
		dest[destpos] = 0;
	}
	else {  /* non-numeric mask */
		digitcnt = anycnt = speccnt = 0;
		for (i1 = 0; mask[i1]; i1++) {
			if (mask[i1] == 'Z') digitcnt++;
			else if (mask[i1] == '9') digitcnt++;
			else if (mask[i1] == 'A' || mask[i1] == 'U' || mask[i1] == 'L') anycnt++;
			else {
				if (mask[i1] == '\\') {
					if (!mask[++i1]) break;
				}
				speccnt++;
			}
		}

		destpos = 0;
		while (destpos < pos) {
			if (!*mask) {
				dest[destpos] = 0;
				*insertpos = destpos;
				return;
			}
			if (*mask == 'Z' || *mask == '9') {
				if (isdigit((INT)(BYTE)*source) || *source == ' ') dest[destpos++] = *(source++);
				mask++;
				digitcnt--;
			}
			else if (*mask == 'A' || *mask == 'U' || *mask == 'L') {
				dest[destpos++] = *(source++);
				mask++;
				anycnt--;
			}
			else {
				if (*mask == '\\') {
					if (!*(++mask)) {
						dest[destpos] = 0;
						*insertpos = destpos;
						return;
					}
				}
				if (*source == *mask) source++;
				else pos++;
				dest[destpos++] = *(mask++);
				speccnt--;
			}
		}

		i1 = 1;
		while (i1) {
			if (!*mask) goto guiaFEditInsertCharX;
			if (*mask == 'Z' || *mask == '9') {
				if (isdigit((INT)(BYTE)newchar) || newchar == ' ') {
					dest[destpos++] = newchar;
					i1 = 0;
				}
				mask++;
				digitcnt--;
			}
			else if (*mask == 'A' || *mask == 'U' || *mask == 'L') {
				if (*mask == 'U') dest[destpos] = (CHAR) toupper(newchar);
				else if (*mask == 'L') dest[destpos] = (CHAR) tolower(newchar);
				else dest[destpos] = (BYTE) newchar;
				destpos++;
				i1 = 0;
				mask++;
				anycnt--;
			}
			else {
				if (*mask == '\\') {
					if (!*(++mask)) goto guiaFEditInsertCharX;
				}
				if (newchar == *mask) i1 = 0;
				dest[destpos++] = *(mask++);
				speccnt--;
			}
		}
		*insertpos = destpos;

/*** NOT COMPLETE YET, MUST SUPPORT SOME FORM OF TRUNCATION ***/
		strcpy(&dest[destpos], source);
#if 0
		while (*mask) {
			if (*mask == 'Z' || *mask == '9') {
				if (isdigit((INT)(BYTE)*source) || *source == ' ') dest[destpos++] = *(source++);
				*mask++;
				digitcnt--;
			}
			else if (*mask == 'A' || *mask == 'U' || *mask == 'L') {
				dest[destpos++] = *(source++);
				*mask++;
				anycnt--;
			}
			else {
				if (*mask == '\\') {
					if (!*(++mask)) break;
				}
				if (*source == *mask) dest[destpos++] = *(source++);
				*mask++;
				speccnt--;
			}
		}
		dest[destpos] = 0;
#endif
	}
	return;

guiaFEditInsertCharX:
	strcpy(dest, savesource);
}

static INT isnummask(CHAR *mask, INT *left, INT *right)
{
	INT numflg, lft, rgt;

	numflg = lft = rgt = 0;
	while (*mask) {
		if (*mask == 'Z') {
			if (numflg >= 2) return(FALSE);
			numflg = 1;
			lft++;
		}
		else if (*mask == '9') {
			if (numflg <= 2) {
				numflg = 2;
				lft++;
			}
			else if (numflg == 3) rgt++;
		}
		else if (*mask == 'A' || *mask == 'U' || *mask == 'L') return(FALSE);
		else {
			if (*mask == '.') {
				if (numflg == 3 || mask[1] != '9') return(FALSE);
				numflg = 3;
			}
			else {
				if (numflg) return(FALSE);
				if (*mask == '\\') {
					if (!*(++mask)) break;
				}
			}
		}
		mask++;
	}
	if (!numflg) return(FALSE);

	*left = lft;
	*right = rgt;
	return(TRUE);
}

/*
 * scroll bar message handler call back
 */
static void guiaScrollCallBack(RESOURCE *res, CONTROL *control, WPARAM wParam)
{
	CHAR *function;

	if (res->cbfnc == NULL) return;
	switch(LOWORD(wParam)) {
	case SB_PAGEDOWN:
		function = DBC_MSGSCROLLPAGEDN;
		control->value += control->value3;
		if (control->value > control->value2) {
			control->value = control->value2;
		}
		break;
	case SB_LINEDOWN:
		function = DBC_MSGSCROLLDNRGHT;
		control->value = min(control->value2, control->value + 1);
		break;
	case SB_PAGEUP:
		function = DBC_MSGSCROLLPAGEUP;
		if (control->value3 > control->value) {
			control->value = control->value1;
		}
		else {
			control->value -= control->value3;
			if (control->value < control->value1) {
				control->value = control->value1;
			}
		}
		break;
	case SB_LINEUP:
		function = DBC_MSGSCROLLUPLEFT;
		control->value = max(control->value1, control->value - 1);
		break;
	case SB_TOP:
		function = DBC_MSGSCROLLTRKFNL;
		control->value = control->value1;
		break;
	case SB_BOTTOM:
		function = DBC_MSGSCROLLTRKFNL;
		control->value = control->value2;
		break;
	case SB_THUMBPOSITION:
		function = DBC_MSGSCROLLTRKFNL;
		control->value = HIWORD(wParam);
		break;
	case SB_THUMBTRACK:
		function = DBC_MSGSCROLLTRKMVE;
		control->value = HIWORD(wParam);
		break;
	default:
		return;
	}
	SetScrollPos(control->ctlhandle, SB_CTL, control->value, TRUE);
	itonum5(control->value, cbmsgdata);
	rescb(res, function, control->useritem, 5);
}

/*
 * process scroll bar messages from scroll bars attached to DB/C window
 */
static void guiaProcessAutoScrollBars(WINDOW *win, UINT nMessage, WPARAM wParam)
{
	INT iScrollUnits, iNewPos, statusBarHeight;
	RECT rectClip;
	RESOURCE *res;
	WINDOWINFO wi;
	CHAR *function;

	if (win->hwndstatusbar != NULL && win->statusbarshown) {
		wi.cbSize = sizeof(WINDOWINFO);
		GetWindowInfo(win->hwndstatusbar, &wi);
		statusBarHeight = wi.rcWindow.bottom - wi.rcWindow.top;
	}
	else statusBarHeight = 0;

	if (nMessage == WM_VSCROLL) {
		switch(LOWORD(wParam)) {
		case SB_TOP:
			function = DBC_MSGVWINSCROLLTRKFNL;
			iScrollUnits = -win->ysize;
			break;
		case SB_BOTTOM:
			function = DBC_MSGVWINSCROLLTRKFNL;
			iScrollUnits = win->ysize;
			break;
		case SB_LINEUP:
			function = DBC_MSGVWINSCROLLUP;
			iScrollUnits = -SBPIXINCR;
			break;
		case SB_LINEDOWN:
			function = DBC_MSGVWINSCROLLDN;
			iScrollUnits = SBPIXINCR;
			break;
		case SB_PAGEUP:
			function = DBC_MSGVWINSCROLLPAGEUP;
			iScrollUnits = - (win->clienty - SBPIXINCR);
			break;
		case SB_PAGEDOWN:
			function = DBC_MSGVWINSCROLLPAGEDN;
			iScrollUnits = win->clienty - SBPIXINCR;
			break;
		case SB_THUMBTRACK:
			function = DBC_MSGVWINSCROLLTRKMVE;
			iScrollUnits = HIWORD(wParam) - win->scrolly;
			break;
		case SB_THUMBPOSITION:
			function = DBC_MSGVWINSCROLLTRKFNL;
			iScrollUnits = HIWORD(wParam) - win->scrolly;
			break;
		default:
			iScrollUnits = 0;
		}

		win->scrolly += iScrollUnits;
		iNewPos = max(0, min(win->scrolly, (win->ysize - win->clienty - statusBarHeight)));
		if (iNewPos != win->scrolly) {
			iScrollUnits += (iNewPos - win->scrolly);
			win->scrolly = iNewPos;
		}
		if (iScrollUnits) {
			if (win->hwndstatusbar != NULL && win->statusbarshown) {
				SetWindowPos(win->hwndstatusbar, NULL, -100, -100, 10, 10, SWP_HIDEWINDOW);
			}
			rectClip.left = 0;
			rectClip.right = win->clientx + 1;
			rectClip.top = win->bordertop;
			rectClip.bottom = win->bordertop + win->clienty + 1;
			rectClip.bottom += statusBarHeight;
			ScrollWindow(win->hwnd, 0, -iScrollUnits, NULL, &rectClip);
			SetScrollPos(win->hwnd, SB_VERT, win->scrolly, TRUE);
			if (win->showtoolbar != NULL && win->bordertop) {
				res = *win->showtoolbar;
				SetWindowPos(res->hwndtoolbar, NULL, 0, 0, 0, 0, SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
			}
			if (win->hwndstatusbar != NULL && win->statusbarshown) {
				SetWindowPos(win->hwndstatusbar, NULL, -100, -100, 10, 10, SWP_SHOWWINDOW | SWP_NOREDRAW | SWP_NOZORDER | SWP_NOACTIVATE);
				InvalidateRect(win->hwndstatusbar, NULL, FALSE);
			}
			itonum5(win->scrolly, cbmsgdata);
			wincb(win, function, 5);
		}
	}
	else {
		switch(LOWORD(wParam)) {
		case SB_TOP:
			function = DBC_MSGHWINSCROLLTRKFNL;
			iScrollUnits = -win->xsize;
			break;
		case SB_BOTTOM:
			function = DBC_MSGHWINSCROLLTRKFNL;
			iScrollUnits = win->xsize;
			break;
		case SB_LINEUP:
			function = DBC_MSGHWINSCROLLLEFT;
			iScrollUnits = -SBPIXINCR;
			break;
		case SB_LINEDOWN:
			function = DBC_MSGHWINSCROLLRGHT;
			iScrollUnits = SBPIXINCR;
			break;
		case SB_PAGEUP:
			function = DBC_MSGHWINSCROLLPAGEUP;
			iScrollUnits = -(win->clientx - SBPIXINCR);
			break;
		case SB_PAGEDOWN:
			function = DBC_MSGHWINSCROLLPAGEDN;
			iScrollUnits = win->clientx - SBPIXINCR;
			break;
		case SB_THUMBTRACK:
			function = DBC_MSGHWINSCROLLTRKMVE;
			iScrollUnits = HIWORD(wParam) - win->scrollx;
			break;
		case SB_THUMBPOSITION:
			function = DBC_MSGHWINSCROLLTRKFNL;
			iScrollUnits = HIWORD(wParam) - win->scrollx;
			break;
		default:
			iScrollUnits = 0;
		}

		win->scrollx += iScrollUnits;
		iNewPos = max(0, min(win->scrollx, (win->xsize - win->clientx)));
		if (iNewPos != win->scrollx) {
			iScrollUnits += (iNewPos - win->scrollx);
			win->scrollx = iNewPos;
		}
		if (iScrollUnits) {
			rectClip.left = 0;
			rectClip.right = win->clientx + 1;
			rectClip.top = win->bordertop;
			rectClip.bottom = win->bordertop + win->clienty + 1;
			rectClip.bottom += statusBarHeight;
			ScrollWindow(win->hwnd, -iScrollUnits, 0, NULL, &rectClip);
			SetScrollPos(win->hwnd, SB_HORZ, win->scrollx, TRUE);
			if (win->showtoolbar != NULL && win->bordertop) {
				res = *win->showtoolbar;
				SetWindowPos(res->hwndtoolbar, NULL, 0, 0, 0, 0, SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
			}
			if (win->hwndstatusbar != NULL && win->statusbarshown) {
				SetWindowPos(win->hwndstatusbar, NULL, -100, -100, 10, 10, SWP_NOREDRAW | SWP_NOZORDER | SWP_NOACTIVATE);
			}
			itonum5(win->scrollx, cbmsgdata);
			wincb(win, function, 5);
		}
	}
	if (iScrollUnits) UpdateWindow(win->hwnd);
}

/**
 * Always called on the Windows message thread.
 * Always returns zero.
 *
 * Behavior changed on 10/30/09, version 15.0.5
 * If bars not needed and are visible and are not at zero position, scroll to top/left first, then make them invisible.
 */
static LONG checkScrollBars(WINHANDLE wh) {
	int i1;

	checkHorizontalScrollBar(wh);
	i1 = checkVerticalScrollBar(wh);
	if (i1) checkHorizontalScrollBar(wh);
	return 0;
}

static void checkHorizontalScrollBar(WINHANDLE wh) {
	INT iRange, wScrollX, xsize, clientx;
	HWND hWindow;
	WINDOW *win;
	USHORT hsbflg;
	RECT clientrect;

	pvistart();
	win = *wh;
	hWindow = win->hwnd;
	wScrollX = win->scrollx;
	xsize = win->xsize;
	clientx = win->clientx;
	hsbflg = win->hsbflg;
	pviend();

	/*
	 * iRange will be positive if the horizontal extent of the union of
	 * rectangles of 'shown' objects exceeds
	 * the user defined window horizontal client space on the screen
	 */
	iRange = xsize - clientx;
	if (hsbflg) {
		if (iRange <= 0) { /* We do not need the horizontal scroll bar. */
			/*if (wScrollX) return 0; Don't know why we cared if the scroll bar was not at zero */
			/*
			 * Send a message to ourselves to move the scroll to the leftmost position
			 */
			if (wScrollX) SendMessage(hWindow, WM_HSCROLL, MAKEWPARAM(SB_LEFT, 0), (LPARAM)0);
			ShowScrollBar(hWindow, SB_HORZ, FALSE);
			(*wh)->hsbflg = FALSE;
			GetClientRect(hWindow, &clientrect);
			(*wh)->clienty = clientrect.bottom - (*wh)->bordertop;
		}
		else {
			SetScrollRange(hWindow, SB_HORZ, 0, iRange, FALSE);
			SetScrollPos(hWindow, SB_HORZ, wScrollX, TRUE);
		}
	}
	else {
		if (iRange > 0) {
			SetScrollRange(hWindow, SB_HORZ, 0, iRange, FALSE);
			SetScrollPos(hWindow, SB_HORZ, wScrollX, FALSE);
			ShowScrollBar(hWindow, SB_HORZ, TRUE);
			(*wh)->hsbflg = TRUE;
			GetClientRect(hWindow, &clientrect);
			(*wh)->clienty = clientrect.bottom - (*wh)->bordertop;
		}
	}
}

/**
 * Returns TRUE if the status of the vertical scroll bar changed, FALSE otherwise
 */
static INT checkVerticalScrollBar(WINHANDLE wh) {
	INT iRange, wScrollY, ysize, clienty;
	HWND hWindow;
	WINDOW *win;
	USHORT vsbflg;
	RECT clientrect;

	pvistart();
	win = *wh;
	hWindow = win->hwnd;
	wScrollY = win->scrolly;
	ysize = win->ysize;
	clienty = win->clienty;
	vsbflg = win->vsbflg;
	pviend();

	/*
	 * iRange will be positive if the vertical extent of the union of
	 * rectangles of 'shown' objects exceeds
	 * the user defined window vertical client space on the screen
	 */
	iRange = ysize - clienty;
	if (vsbflg) { /* Is the vert scroll bar visible? */
		if (iRange <= 0) { /* We do not need the vertical scroll bar. */
			/*if (wScrollY) return 0; Don't know why we cared if the scroll bar was not at zero */
			/*
			 * Send a message to ourselves to move the scroll to the top position
			 */
			if (wScrollY) SendMessage(hWindow, WM_VSCROLL, MAKEWPARAM(SB_TOP, 0), (LPARAM)0);
			ShowScrollBar(hWindow, SB_VERT, FALSE);
			(*wh)->vsbflg = FALSE;
			GetClientRect(hWindow, &clientrect);
			(*wh)->clientx = clientrect.right;
			return TRUE;
		}
		else {
			SetScrollRange(hWindow, SB_VERT, 0, iRange, FALSE);
			SetScrollPos(hWindow, SB_VERT, wScrollY, TRUE);
		}
	}
	else {
		if (iRange > 0) {
			/*
			 * If the stuff in the window is too tall vertically, and the vert
			 * scroll bar is currently not visible, make it so.
			 */
			SetScrollRange(hWindow, SB_VERT, 0, iRange, FALSE);
			SetScrollPos(hWindow, SB_VERT, wScrollY, FALSE);
			ShowScrollBar(hWindow, SB_VERT, TRUE);
			(*wh)->vsbflg = TRUE;
			GetClientRect(hWindow, &clientrect);
			(*wh)->clientx = clientrect.right;
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Could be called from either the main or Windows' message thread
 */
static void guiaCheckScrollBars(WINHANDLE wh)
{
	SendMessage(hwndmainwin, GUIAM_CHECKSCROLLBARS, (WPARAM) 0, (LPARAM) wh);
}

/*
 * check scroll bar range and position and change accordingly
 */
//static void guiaCheckScrollBars(WINHANDLE wh)
//{
//	INT iRange, wScrollX, wScrollY, xsize, ysize, clientx, clienty;
//	HWND hWindow;
//	WINDOW *win;
//	USHORT hsbflg, vsbflg;
//
//	pvistart();
//	win = *wh;
//	hWindow = win->hwnd;
//	wScrollX = win->scrollx;
//	wScrollY = win->scrolly;
//	xsize = win->xsize;
//	ysize = win->ysize;
//	clientx = win->clientx;
//	clienty = win->clienty;
//	hsbflg = win->hsbflg;
//	vsbflg = win->vsbflg;
//	pviend();
//
//	iRange = xsize - clientx;
//	if (hsbflg) {
//		if (iRange < 0) {
//			if (wScrollX) return;
//			ShowScrollBar(hWindow, SB_HORZ, FALSE);
//			(*wh)->hsbflg = FALSE;
//		}
//		else {
//			SetScrollRange(hWindow, SB_HORZ, 0, iRange, FALSE);
//			SetScrollPos(hWindow, SB_HORZ, wScrollX, TRUE);
//		}
//	}
//	else {
//		if (iRange > 0) {
//			SetScrollRange(hWindow, SB_HORZ, 0, iRange, FALSE);
//			SetScrollPos(hWindow, SB_HORZ, wScrollX, FALSE);
//			ShowScrollBar(hWindow, SB_HORZ, TRUE);
//			(*wh)->hsbflg = TRUE;
//		}
//	}
//
//	iRange = ysize - clienty;
//	if (vsbflg) {
//		if (iRange < 0) {
//			if (wScrollY) return;
//			ShowScrollBar(hWindow, SB_VERT, FALSE);
//			(*wh)->vsbflg = FALSE;
//		}
//		else {
//			SetScrollRange(hWindow, SB_VERT, 0, iRange, FALSE);
//			SetScrollPos(hWindow, SB_VERT, wScrollY, TRUE);
//		}
//	}
//	else {
//		if (iRange > 0) {
//			SetScrollRange(hWindow, SB_VERT, 0, iRange, FALSE);
//			SetScrollPos(hWindow, SB_VERT, wScrollY, FALSE);
//			ShowScrollBar(hWindow, SB_VERT, TRUE);
//			(*wh)->vsbflg = TRUE;
//		}
//	}
//
//	return;
//}

/*
 * set non-auto window scroll bar position
 */
INT guiaWinScrollPos(WINDOW *win, INT iScrollBar, INT iPosition)
{
 	if (win->autoscroll || !win->sbflg) return RC_ERROR;

	if ('H' == iScrollBar) {
		if (iPosition < 1) {
			if (win->hsbflg) {
				ShowScrollBar(win->hwnd, SB_HORZ, FALSE);
				win->hsbflg = FALSE;
			}
		}
		else {
			if (!win->hsbflg) {
				SetScrollPos(win->hwnd, SB_HORZ, iPosition, FALSE);
				ShowScrollBar(win->hwnd, SB_HORZ, TRUE);
				win->hsbflg = TRUE;
			}
			else SetScrollPos(win->hwnd, SB_HORZ, iPosition, TRUE);
		}
	}
	else if ('V' == iScrollBar) {
		if (iPosition < 1) {
			if (win->vsbflg) {
				ShowScrollBar(win->hwnd, SB_VERT, FALSE);
				win->vsbflg = FALSE;
			}
		}
		else {
			if (!win->vsbflg) {
				SetScrollPos(win->hwnd, SB_VERT, iPosition, FALSE);
				ShowScrollBar(win->hwnd, SB_VERT, TRUE);
				win->vsbflg = TRUE;
			}
			else SetScrollPos(win->hwnd, SB_VERT, iPosition, TRUE);
		}
	}
	else return RC_ERROR;
	return 0;
}

/*
 * Implement new scroll bar keyboard interface
 */
INT guiaWinScrollKeys(WINDOW *win, INT o, char * key)
{
	INT rc = 0;

	if (!win->autoscroll || !win->sbflg) return(rc);
	if (o == 'H') {
		if (!strcmp(key, "HOME"))
			PostMessage(win->hwnd, WM_HSCROLL, SB_TOP, 0L);
		else if (!strcmp(key, "END"))
			PostMessage(win->hwnd, WM_HSCROLL, SB_BOTTOM, 0L);
		else if (!strcmp(key, "LEFT"))
			PostMessage(win->hwnd, WM_HSCROLL, SB_LINEUP, 0L);
		else if (!strcmp(key, "RIGHT"))
			PostMessage(win->hwnd, WM_HSCROLL, SB_LINEDOWN, 0L);
		else if (!strcmp(key, "PGLEFT"))
			PostMessage(win->hwnd, WM_HSCROLL, SB_PAGEUP, 0L);
		else if (!strcmp(key, "PGRIGHT"))
			PostMessage(win->hwnd, WM_HSCROLL, SB_PAGEDOWN, 0L);
		else rc = RC_INVALID_VALUE;
	}
	else if (o == 'V') {
		if (!strcmp(key, "HOME"))
			PostMessage(win->hwnd, WM_VSCROLL, SB_TOP, 0L);
		else if (!strcmp(key, "END"))
			PostMessage(win->hwnd, WM_VSCROLL, SB_BOTTOM, 0L);
		else if (!strcmp(key, "UP"))
			PostMessage(win->hwnd, WM_VSCROLL, SB_LINEUP, 0L);
		else if (!strcmp(key, "DOWN"))
			PostMessage(win->hwnd, WM_VSCROLL, SB_LINEDOWN, 0L);
		else if (!strcmp(key, "PGUP"))
			PostMessage(win->hwnd, WM_VSCROLL, SB_PAGEUP, 0L);
		else if (!strcmp(key, "PGDOWN"))
			PostMessage(win->hwnd, WM_VSCROLL, SB_PAGEDOWN, 0L);
		else rc = RC_INVALID_VALUE;
	}
	else rc = RC_INVALID_VALUE;
	return(rc);
}

/*
 * set non-auto window scroll range and page size
 */
INT guiaWinScrollRange(WINDOW *win, INT iScrollBar, INT iMin, INT iMax, INT iPage)
{
	SCROLLINFO sInfo;

 	if (win->autoscroll || !win->sbflg) return RC_ERROR;

 	sInfo.cbSize = sizeof(SCROLLINFO);
 	sInfo.fMask = SIF_RANGE;
	sInfo.nMin = iMin;
	sInfo.nMax = iMax;
	if ('H' == iScrollBar) {
		win->hsbmin = iMin;
		win->hsbmax = iMax;
		win->hsbpage = iPage;
		if (!win->hsbflg) {
			if (! /*SetScrollRange(win->hwnd, SB_HORZ, iMin, iMax, FALSE)*/
				SetScrollInfo(win->hwnd, SB_HORZ, &sInfo, FALSE))
			{
				guiaErrorMsg("Error in setting HorzScrollBarRange", GetLastError());
			}
			ShowScrollBar(win->hwnd, SB_HORZ, TRUE);
			win->hsbflg = TRUE;
		}
		else {
			if (! /*SetScrollRange(win->hwnd, SB_HORZ, iMin, iMax, TRUE)*/
				SetScrollInfo(win->hwnd, SB_HORZ, &sInfo, TRUE))
			{
				guiaErrorMsg("Error in setting HorzScrollBarRange", GetLastError());
			}
		}
	}
	else if ('V' == iScrollBar) {
		win->vsbmin = iMin;
		win->vsbmax = iMax;
		win->vsbpage = iPage;
		if (!win->vsbflg) {
			if (! /*SetScrollRange(win->hwnd, SB_VERT, iMin, iMax, FALSE)*/
				SetScrollInfo(win->hwnd, SB_VERT, &sInfo, FALSE))
			{
				guiaErrorMsg("Error in setting VertScrollBarRange", GetLastError());
			}
			ShowScrollBar(win->hwnd, SB_VERT, TRUE);
			win->vsbflg = TRUE;
		}
		else {
			if (! /*SetScrollRange(win->hwnd, SB_VERT, iMin, iMax, TRUE)*/
				SetScrollInfo(win->hwnd, SB_VERT, &sInfo, TRUE))
			{
				guiaErrorMsg("Error in setting VertScrollBarRange", GetLastError());
			}
		}
	}
	else return RC_ERROR;
	return 0;
}

/*
 * process scroll bar messages from non-auto scroll bars attached to DB/C window
 */
static void guiaProcessWinScrollBars(WINDOW *win, UINT nMessage, WPARAM wParam)
{
	CHAR *function;
	INT scrollpos;
	SCROLLINFO sInfo;

	if (win->cbfnc == NULL) return;
 	sInfo.cbSize = sizeof(SCROLLINFO);
 	sInfo.fMask = SIF_POS;
	if (nMessage == WM_VSCROLL) {
		switch(LOWORD(wParam)) {
		case SB_LINEUP:
			function = DBC_MSGVWINSCROLLUP;
			scrollpos = GetScrollPos(win->hwnd, SB_VERT) - 1;
			break;
		case SB_LINEDOWN:
			function = DBC_MSGVWINSCROLLDN;
			scrollpos = GetScrollPos(win->hwnd, SB_VERT) + 1;
			break;
		case SB_PAGEUP:
			function = DBC_MSGVWINSCROLLPAGEUP;
			scrollpos = GetScrollPos(win->hwnd, SB_VERT) - win->vsbpage;
			break;
		case SB_PAGEDOWN:
			function = DBC_MSGVWINSCROLLPAGEDN;
			scrollpos = GetScrollPos(win->hwnd, SB_VERT) + win->vsbpage;
			break;
		case SB_TOP:
			function = DBC_MSGVWINSCROLLTRKFNL;
			scrollpos = win->vsbmin;
			break;
		case SB_BOTTOM:
			function = DBC_MSGVWINSCROLLTRKFNL;
			scrollpos = win->vsbmax;
			break;
		case SB_THUMBPOSITION:
			function = DBC_MSGVWINSCROLLTRKFNL;
			scrollpos = HIWORD(wParam);
			break;
		case SB_THUMBTRACK:
			function = DBC_MSGVWINSCROLLTRKMVE;
			scrollpos = HIWORD(wParam);
			break;
		default:
			return;
		}
		if (scrollpos < win->vsbmin) scrollpos = win->vsbmin;
		else if (scrollpos > win->vsbmax) scrollpos = win->vsbmax;
		/*SetScrollPos(win->hwnd, SB_VERT, scrollpos, TRUE);*/
		sInfo.nPos = scrollpos;
		SetScrollInfo(win->hwnd, SB_VERT, &sInfo, TRUE);
	}
	else {
		switch(LOWORD(wParam)) {
		case SB_LINEUP:
			function = DBC_MSGHWINSCROLLLEFT;
			scrollpos = GetScrollPos(win->hwnd, SB_HORZ) - 1;
			break;
		case SB_LINEDOWN:
			function = DBC_MSGHWINSCROLLRGHT;
			scrollpos = GetScrollPos(win->hwnd, SB_HORZ) + 1;
			break;
		case SB_PAGEUP:
			function = DBC_MSGHWINSCROLLPAGEUP;
			scrollpos = GetScrollPos(win->hwnd, SB_HORZ) - win->hsbpage;
			break;
		case SB_PAGEDOWN:
			function = DBC_MSGHWINSCROLLPAGEDN;
			scrollpos = GetScrollPos(win->hwnd, SB_HORZ) + win->hsbpage;
			break;
		case SB_TOP:
			function = DBC_MSGHWINSCROLLTRKFNL;
			scrollpos = win->hsbmin;
			break;
		case SB_BOTTOM:
			function = DBC_MSGHWINSCROLLTRKFNL;
			scrollpos = win->hsbmax;
			break;
		case SB_THUMBPOSITION:
			function = DBC_MSGHWINSCROLLTRKFNL;
			scrollpos = HIWORD(wParam);
			break;
		case SB_THUMBTRACK:
			function = DBC_MSGHWINSCROLLTRKMVE;
			scrollpos = HIWORD(wParam);
			break;
		default:
			return;
		}
		if (scrollpos < win->hsbmin) scrollpos = win->hsbmin;
		else if (scrollpos > win->hsbmax) scrollpos = win->hsbmax;
		/*SetScrollPos(win->hwnd, SB_HORZ, scrollpos, TRUE);*/
		sInfo.nPos = scrollpos;
		SetScrollInfo(win->hwnd, SB_HORZ, &sInfo, TRUE);
	}
	itonum5(scrollpos, cbmsgdata);
	wincb(win, function, 5);
}

/*
 * create a pixel map
 */
INT guiaCreatePixmap(PIXHANDLE ph)
{
	BITMAPINFO *pbmi;
	INT iClrRef, iClrUsed;
	ZRECT rect;
	HDC hdc;
	PLOGPALETTE pLogPal;
	TEXTMETRIC tm;
	PIXMAP *pix;

	pix = *ph;

	/*
	 * Create the Mutex to control access to the pixmap and painting it.
	 */
	pix->hPaintingMutex = CreateMutex(NULL, FALSE, NULL);
	if (pix->hPaintingMutex == NULL) {
		guiaErrorMsg("Fatal Error - unable to create pix mutex", GetLastError());
		return RC_ERROR;
	}

	rect.top = rect.left = 0;
	rect.right = pix->hsize;
	rect.bottom = pix->vsize;

	hdc = GetDC(hwndmainwin);
	/* if colorbits was not specified then use max bpp for screen device */
	if (!pix->bpp) {
		if (bpplimit) pix->bpp = (BYTE) bpplimit;
		else {
			if (hdc == NULL) pix->bpp = 24;	/* Unless there is no screen device, should not happen! */
			else {
				/* query system about current video mode resolution */
				bpplimit = GetDeviceCaps(hdc, BITSPIXEL) * GetDeviceCaps(hdc, PLANES);
				pix->bpp = (BYTE) bpplimit;
			}
		}
	}

	pix->logpen.lopnStyle = pix->pentype = 0;
	pix->logpen.lopnColor = pix->pencolor = 0;
	pix->logpen.lopnWidth.x = pix->penwidth = 1;
	if (guiaParseFont("SYSTEM(" INITIAL_DRAW_TEXT_SIZE ",PLAIN)", 16, &pix->lf, TRUE, hdc) < 0) return RC_ERROR;

	switch(pix->bpp) {
	case 1: iClrUsed = 2; break;
	case 4: iClrUsed = 16; break;
	case 8: iClrUsed = 256; break;
	case 16:
	case 32:
		pix->bpp = 24;
		// @suppress("No break at end of case")
	case 24: iClrUsed = 0; break;
	default: return RC_ERROR;
	}

/* allocate enough space for BITMAPINFOHEADER, */
/* color palette and device-independent pixmap */
	if (iClrUsed > 2) {
		pLogPal = (LOGPALETTE *) guiAllocMem(sizeof(LOGPALETTE) + iClrUsed * sizeof(PALETTEENTRY));
		if (pLogPal == NULL) {
			guiaErrorMsg("Out of guiAllocMem memory in guiaCreatePixmap", sizeof(LOGPALETTE) + iClrUsed * sizeof(PALETTEENTRY));
			return RC_NO_MEM;
		}
	}

	pbmi = (BITMAPINFO *) guiAllocMem(sizeof(BITMAPINFOHEADER) + iClrUsed * sizeof(RGBQUAD));
	if (pbmi == NULL) {
		guiaErrorMsg("Out of guiAllocMem memory in guiaCreatePixmap", sizeof(BITMAPINFOHEADER) + iClrUsed * sizeof(RGBQUAD));
		return RC_NO_MEM;
	}

/* initialize BITMAPINFOHEADER in BITMAPINFO structure */
	pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pbmi->bmiHeader.biWidth = pix->hsize;
	pbmi->bmiHeader.biHeight = pix->vsize;
	pbmi->bmiHeader.biPlanes = 1;
	pbmi->bmiHeader.biBitCount = pix->bpp;
	pbmi->bmiHeader.biCompression = BI_RGB;
	pbmi->bmiHeader.biSizeImage = ((((pix->hsize * pix->bpp) + 31) & ~31) >> 3) * pix->vsize;
	pbmi->bmiHeader.biXPelsPerMeter = 1000;
	pbmi->bmiHeader.biYPelsPerMeter = 1000;
	pbmi->bmiHeader.biClrUsed = iClrUsed;
	pbmi->bmiHeader.biClrImportant = 0;

/* initialize RGB structures */
	if (iClrUsed > 2) {
		guiaSetDefaultPalette(iClrUsed, pbmi->bmiColors);
		pLogPal->palVersion = 0x300;
		pLogPal->palNumEntries = iClrUsed;
		for (iClrRef = 0; iClrRef < iClrUsed; iClrRef++) {
			pLogPal->palPalEntry[iClrRef].peRed = pbmi->bmiColors[iClrRef].rgbRed;
			pLogPal->palPalEntry[iClrRef].peGreen = pbmi->bmiColors[iClrRef].rgbGreen;
			pLogPal->palPalEntry[iClrRef].peBlue = pbmi->bmiColors[iClrRef].rgbBlue;
			pLogPal->palPalEntry[iClrRef].peFlags = 0;
		}
		pix->hpal = CreatePalette(pLogPal);
		guiFreeMem((ZHANDLE) pLogPal);
		if (pix->hpal == NULL) {
			guiFreeMem((ZHANDLE) pbmi);
			return RC_ERROR;
		}
	}
	else if (iClrUsed == 2) {
		pbmi->bmiColors[0].rgbRed = 0x00;
		pbmi->bmiColors[0].rgbGreen = 0x00;
		pbmi->bmiColors[0].rgbBlue = 0x00;
		pbmi->bmiColors[1].rgbRed = 0xFF;
		pbmi->bmiColors[1].rgbGreen = 0xFF;
		pbmi->bmiColors[1].rgbBlue = 0xFF;
	}
	else pix->hpal = NULL;

	pix->pbmi = pbmi;
	pix->hbitmap =
		CreateDIBSection(hdc,	/* This is ignored for DIB_RGB_COLORS */
			pix->pbmi, DIB_RGB_COLORS,
			(VOID *) &pix->buffer, /* [out] Pointer to a variable that receives a pointer to the location of the DIB bit values */
			NULL,			/* NULL asks the system to allocate memory for the DIB */
			0				/* This is ignored if the above is NULL */
		);
	if (pix->hbitmap == NULL) {
		guiFreeMem((ZHANDLE) pbmi);
		pix->pbmi = NULL;
		return RC_ERROR;
	}
	pix->hfont = getFont(&pix->lf);
	if (pix->hfont == NULL) {
		ReleaseDC(hwndmainwin, hdc);
		return RC_ERROR;
	}
	oldhfont = SelectObject(hdc, pix->hfont);
	GetTextMetrics(hdc, &tm);
	pix->fontheight = (USHORT) (tm.tmHeight + tm.tmExternalLeading);
	pix->fontwidth = (USHORT) tm.tmMaxCharWidth;
	pix->status = 1;
	SelectObject(hdc, oldhfont);
	ReleaseDC(hwndmainwin, hdc);
	return 0;
}

/*
 * creates a rainbow color wash palette to be used for default coloring
 */
void guiaSetDefaultPalette(INT iNumClrs, RGBQUAD *rgbClrTable)
{
	INT iCurClr, iCurRed, iCurGreen, iCurBlue;
	BYTE ucGrayShade;

	/* fill in colors used for 16 and 256 color palette modes */
	for (iCurClr = 0; iCurClr < 16; iCurClr++) {
		rgbClrTable[iCurClr].rgbBlue = DBC_BLUE(dwDefaultPal[iCurClr]);
		rgbClrTable[iCurClr].rgbGreen = DBC_GREEN(dwDefaultPal[iCurClr]);
		rgbClrTable[iCurClr].rgbRed = DBC_RED(dwDefaultPal[iCurClr]);
		rgbClrTable[iCurClr].rgbReserved = 0;
	}
	if (iNumClrs < 256) return;

	/* fill in colors used only in 256 color palette modes */
	for (iCurRed = 0; iCurRed < 256; iCurRed += 51) {
		for (iCurGreen = 0; iCurGreen < 256; iCurGreen += 51) {
			for (iCurBlue = 0; iCurBlue < 256; iCurBlue += 51) {
				rgbClrTable[iCurClr].rgbBlue = (BYTE) iCurBlue;
				rgbClrTable[iCurClr].rgbGreen = (BYTE) iCurGreen;
				rgbClrTable[iCurClr].rgbRed = (BYTE) iCurRed;
				rgbClrTable[iCurClr++].rgbReserved = 0;
			}
		}
	}
	/* finish off with a spectrum of gray shades */
	/* White and black are already in the palette */
	ucGrayShade = 10;
	for (;iCurClr < 256; iCurClr++) {
		rgbClrTable[iCurClr].rgbBlue = ucGrayShade;
		rgbClrTable[iCurClr].rgbGreen = ucGrayShade;
		rgbClrTable[iCurClr].rgbRed = ucGrayShade;
		rgbClrTable[iCurClr].rgbReserved = 0;
		ucGrayShade += 10;
	}
}

/*
 * destroy a pixel map
 */
INT guiaDestroyPixmap(PIXHANDLE ph)
{
	PIXMAP *pix;
	HFONT hfont;
	BITMAPINFO *pbmi;
	HBITMAP hbitmap;
	HPALETTE hpal;

	pvistart();
	pix = *ph;
	hfont = pix->hfont;
	pix->hfont = NULL;
	pbmi = pix->pbmi;
	pix->pbmi = NULL;
	hbitmap = pix->hbitmap;
	pix->hbitmap = NULL;
	hpal = pix->hpal;
	pix->hpal = NULL;
	pix->status = 0;
	pviend();

	if (pbmi != NULL) guiFreeMem((ZHANDLE) pbmi);
	if (hbitmap != NULL) DeleteObject(hbitmap);
	if (hpal != NULL) DeleteObject(hpal);
	return 0;
}

/*
 * show a pixel map in a window
 * This is called only from guipixshow in gui.c
 *
 * This is always run on the DB/C main thread and does not need to
 * protect itself with calls to pvistart/pviend
 */
INT guiaShowPixmap(PIXHANDLE ph)
{
	ZRECT rectDest;
	INT iSize;
	WINHANDLE wh;
	WINDOW *win;
	PIXMAP *pix;
	HWND hWindow;
	USHORT autoscroll;

	pix = *ph;
	wh = pix->winshow;
	win = *wh;
	hWindow = win->hwnd;
	autoscroll = win->autoscroll;

	rectDest.top = pix->vshow - win->scrolly + win->bordertop;
	rectDest.left = pix->hshow - win->scrollx;
	rectDest.bottom = rectDest.top + pix->vsize;
	rectDest.right = rectDest.left + pix->hsize;

	iSize = pix->hshow + pix->hsize;
	if (win->xsize < iSize) win->xsize = iSize;
	iSize = pix->vshow + pix->vsize;
	if (win->ysize < iSize) win->ysize = iSize;

	if (autoscroll) guiaCheckScrollBars(wh);
	SendMessage(hWindow, GUIAM_SHOWPIX, (WPARAM) &rectDest, (LPARAM) ph);
	return 0;
}

/*
 * hide a pixel map
 * This is called only from guipixhide in gui.c on the DX main thread
 *
 * At this point, (*ph)->winshow != NULL yet.
 * But it has been removed from win->showpixhdr linked list
 */
INT guiaHidePixmap(PIXHANDLE ph)
{
	RECT rect;
	WINDOW *win;
	WINHANDLE wh;
	PIXMAP *pix;
	USHORT autoscroll;
	HWND hWindow;

	wh = (*ph)->winshow;
	if (wh == NULL) return 0;
	checkwinsize(wh);

	pix = *ph;
	wh = pix->winshow;
	win = *wh;
	rect.top = pix->vshow - win->scrolly + win->bordertop;
	rect.left = pix->hshow - win->scrollx;
	rect.bottom = rect.top + pix->vsize;
	rect.right = rect.left + pix->hsize;
	autoscroll = win->autoscroll;
	hWindow = win->hwnd;

	if (autoscroll) guiaCheckScrollBars(wh);
	SendMessage(hWindow, GUIAM_HIDEPIX, (WPARAM) &rect, (LPARAM) ph);
	return 0;
}

/*
 * start a drawing command sequence
 * This always runs on the DX thread
 */
void guiaDrawStart(PIXHANDLE ppixDraw)
{
	DWORD dwWaitResult;

	dwWaitResult = WaitForSingleObject((*ppixDraw)->hPaintingMutex, INFINITE);
	if (dwWaitResult == WAIT_FAILED) {
		guiaErrorMsg("Failure waiting for pix mutex", GetLastError());
		return;
	}
	SetRectEmpty(&guiaDrawInvalidRect);
	SendMessage(hwndmainwin, GUIAM_STARTDRAW, (WPARAM) 0, (LPARAM) ppixDraw);
}

/*
 * end a drawing command sequence
 */
void guiaDrawEnd()
{
	PIXMAP *pix;
	WINDOW *win;

	pix = *drawpixhandle;

	DeleteObject(SelectObject(pix->hdc, hpennull));
	DeleteObject(SelectObject(pix->hdc, hbrushnull));
	SelectObject(pix->hdc, oldhfont);
	SelectObject(pix->hdc, oldhbitmap);
	if (pix->hpal != NULL) SelectPalette(pix->hdc, hpaloldpal, TRUE);
	DeleteDC(pix->hdc); /* removed compatible DC for pix */
	pix->hdc = NULL;
	drawpixhandle = NULL;
	GdiFlush();
	ReleaseMutex(pix->hPaintingMutex);
	if (drawwinhandle != NULL && !IsRectEmpty(&guiaDrawInvalidRect)) {
		RECT clientRect;
		win = *drawwinhandle;
		GetClientRect(win->hwnd, &clientRect);
		IntersectRect(&guiaDrawInvalidRect, &guiaDrawInvalidRect, &clientRect);
		if (!IsRectEmpty(&guiaDrawInvalidRect)) {
			ResetEvent(hPaintingEvent); // Set to non-signaled state
			InvalidateRect(win->hwnd, &guiaDrawInvalidRect, TRUE);
			UpdateWindow(win->hwnd);
			/**
			 * UpdateWindow will cause a WM_PAINT message to be sent immediately.
			 * It runs on the other thread. But we must not proceed here
			 * on our main thread until all the drawing is complete
			 * So, wait for all the WM_PAINT processing to finish
			 */
			WaitForSingleObject(hPaintingEvent, INFINITE);
			if (win->statusbarshown) {
				/* make sure status bar gets placed back on top of image */
				PostMessage(win->hwndstatusbar, SB_SETTEXT, (WPARAM) 255, (LPARAM) win->statusbartext);
			}
		}
	}
	drawwinhandle = NULL;
}

/*
 * set the pen style and color
 *
 * This is called at the beginning of a draw verb, and when color, linewidth, or linetype is set.
 */
void guiaSetPen()
{
	HPEN hPenOld, hPenNew;
	HBRUSH hBrushOld, hBrushNew;
	PIXMAP *ppixDraw;

	ppixDraw = *drawpixhandle;
	switch(ppixDraw->pentype) {
	case DRAW_LINE_SOLID:
	case DRAW_LINE_INVERT:
		ppixDraw->logpen.lopnStyle = PS_SOLID;
		break;
	case DRAW_LINE_DASH:
		ppixDraw->logpen.lopnStyle = PS_DASH;
		break;
	case DRAW_LINE_DOT:
			ppixDraw->logpen.lopnStyle = PS_DOT;
			break;
	case DRAW_LINE_DASHDOT:
		ppixDraw->logpen.lopnStyle = PS_DASHDOT;
		break;
	case DRAW_LINE_REVDOT:
		ppixDraw->logpen.lopnStyle = PS_DOT;
		break;
	}
	ppixDraw->pencolor = GetNearestColor(ppixDraw->hdc, ppixDraw->pencolor);
	ppixDraw->logpen.lopnColor = ppixDraw->pencolor;
	ppixDraw->logpen.lopnWidth.x = ppixDraw->penwidth;
	SetTextColor(ppixDraw->hdc, ppixDraw->pencolor);
	hBrushNew = CreateSolidBrush(ppixDraw->pencolor);
	hBrushOld = SelectObject(ppixDraw->hdc, hBrushNew);
	hPenNew = CreatePenIndirect(&ppixDraw->logpen);
	hPenOld = SelectObject(ppixDraw->hdc, hPenNew);
	SetBkMode(ppixDraw->hdc, TRANSPARENT);
	if (ppixDraw->pentype == DRAW_LINE_REVDOT) SetROP2(ppixDraw->hdc, R2_NOTXORPEN);
	else SetROP2(ppixDraw->hdc, R2_COPYPEN);

	if (hBrushOld != hbrushnull) DeleteObject(hBrushOld);
	if (hPenOld != hpennull) DeleteObject(hPenOld);
}

/*
 * set the draw font
 */
void guiaSetFont(ZHANDLE text)
{
	HFONT hfont;
	TEXTMETRIC tm;
	PIXMAP *ppixDraw;
	CHAR *tptr = (CHAR *) guiMemToPtr(text);

	if (text == NULL) return;
	ppixDraw = *drawpixhandle;
	if (guiaParseFont(tptr,
			(INT)strlen(tptr), &ppixDraw->lf, TRUE, ppixDraw->hdc) == 0) {
		hfont = getFont(&ppixDraw->lf);
		SelectObject(ppixDraw->hdc, hfont);
		ppixDraw->hfont = hfont;
		GetTextMetrics(ppixDraw->hdc, &tm);
		ppixDraw->fontheight = (USHORT) (tm.tmHeight + tm.tmExternalLeading);
		ppixDraw->fontwidth = (USHORT) tm.tmMaxCharWidth;
	}
}

void guiaDrawReplace(INT32 color1, INT32 color2)
{
	INT i1, i2;
	PIXMAP *ppixDraw;
	WINDOW *pwinDrawWin;
	RECT rect;
	BOOL first = TRUE;

	ppixDraw = *drawpixhandle;

	for (i2 = 0; i2 < ppixDraw->vsize; i2++) {
		for (i1 = 0; i1 < ppixDraw->hsize; i1++) {
			if ((COLORREF) color1 == (0x00FFFFFF & GetPixel(ppixDraw->hdc, i1, i2))) {
				SetPixel(ppixDraw->hdc, i1, i2, color2);
				if (drawwinhandle != NULL) {
					if (first) {
						rect.left = rect.right = i1;
						rect.top = rect.bottom = i2;
						first = FALSE;
					}
					if (i1 < rect.left) rect.left = i1;
					if (i1 > rect.right) rect.right = i1;
					if (i2 < rect.top) rect.top = i2;
					if (i2 > rect.bottom) rect.bottom = i2;
				}
			}
		}
	}
	if (drawwinhandle != NULL) {
		pwinDrawWin = *drawwinhandle;
		OffsetRect(&rect, ppixDraw->hshow - pwinDrawWin->scrollx,
				ppixDraw->vshow - pwinDrawWin->scrolly + pwinDrawWin->bordertop);
		UnionRect(&guiaDrawInvalidRect, &guiaDrawInvalidRect, &rect);
	}
}

static void setPenWidthTemporarilyToOne (PIXMAP *ppixDraw) {
	ppixDraw->tempWidth = ppixDraw->penwidth;
	ppixDraw->logpen.lopnWidth.x = 1;
	ppixDraw->hTempPen = SelectObject(ppixDraw->hdc, CreatePenIndirect(&ppixDraw->logpen));
}

static void restorePenWidthFromTemporaryValue(PIXMAP *ppixDraw) {
	ppixDraw->logpen.lopnWidth.x = ppixDraw->tempWidth;
	DeleteObject(SelectObject(ppixDraw->hdc, ppixDraw->hTempPen));
}

/*
 * draw a rectangle
 */
void guiaDrawRect()
{
	ZRECT rect;
	PIXMAP *ppixDraw;
	WINDOW *pwinDrawWin;
	LONG temp;

	ppixDraw = *drawpixhandle;

	/**
	 * The next two if blocks fix a very specific visual problem.
	 * jpr 29 MAR 2013
	 *
	 * Although the Rectangle function is ok with an upside-down rect, the UnionRect function
	 * is not. We must flip the coords if the rect is inverted up-down or left-right.
	 * Otherwise, we might not tell the OS to paint it as UnionRect seems to see it as empty.
	 */
	if (ppixDraw->pos1.y > ppixDraw->pos2.y) {
		temp = ppixDraw->pos2.y;
		ppixDraw->pos2.y = ppixDraw->pos1.y;
		ppixDraw->pos1.y = temp;
	}
	if (ppixDraw->pos1.x > ppixDraw->pos2.x) {
		temp = ppixDraw->pos2.x;
		ppixDraw->pos2.x = ppixDraw->pos1.x;
		ppixDraw->pos1.x = temp;
	}

	rect.top = ppixDraw->pos1.y;
	rect.left = ppixDraw->pos1.x;
	/* rectangle does not include x2,y2 so add one */
	rect.bottom = ppixDraw->pos2.y + 1;
	rect.right = ppixDraw->pos2.x + 1;

	/*
	 * In the unlikely case that the current pen width is not one,
	 * we need to create a one pixel wide pen. The Win32 function Rectangle
	 * will draw a border around the specified rectangle using the current line width.
	 * We don't want that. It is not documented and users don't expect it.
	 */
	if (ppixDraw->penwidth != 1) setPenWidthTemporarilyToOne(ppixDraw);
	Rectangle(ppixDraw->hdc, rect.left, rect.top, rect.right, rect.bottom);
	if (ppixDraw->penwidth != 1) restorePenWidthFromTemporaryValue(ppixDraw);

	if (drawwinhandle != NULL) {
		pwinDrawWin = *drawwinhandle;
		OffsetRect(&rect, ppixDraw->hshow - pwinDrawWin->scrollx,
				ppixDraw->vshow - pwinDrawWin->scrolly + pwinDrawWin->bordertop);
		UnionRect(&guiaDrawInvalidRect, &guiaDrawInvalidRect, &rect);
	}
}

/*
 * draw a Triangle
 */
void guiaDrawTriangle()
{
	POINT pts[3];
	INT yoff, xoff;
	PIXMAP *ppixDraw;
	WINDOW *pwinDrawWin;

	ppixDraw = *drawpixhandle;
	pts[0].x = ppixDraw->pos1.x;
	pts[1].x = ppixDraw->pos2.x;
	pts[2].x = ppixDraw->pos3.x;
	pts[0].y = ppixDraw->pos1.y;
	pts[1].y = ppixDraw->pos2.y;
	pts[2].y = ppixDraw->pos3.y;
	/*
	 * In the unlikely case that the current pen width is not one,
	 * we need to create a one pixel wide pen. The Win32 function Polygon
	 * will draw a border around the specified shape using the current pen width.
	 * We don't want that. It is not documented and users don't expect it.
	 */
	if (ppixDraw->penwidth != 1) setPenWidthTemporarilyToOne(ppixDraw);
	Polygon(ppixDraw->hdc, pts, 3);
	if (ppixDraw->penwidth != 1) restorePenWidthFromTemporaryValue(ppixDraw);

	if (drawwinhandle != NULL) {
		RECT rect;
		INT i1;
		rect.top = rect.left = LONG_MAX;
		rect.bottom = rect.right = LONG_MIN;
		pwinDrawWin = *drawwinhandle;
		xoff = ppixDraw->hshow - pwinDrawWin->scrollx;
		pts[0].x += xoff; pts[1].x += xoff; pts[2].x += xoff;

		yoff = ppixDraw->vshow - pwinDrawWin->scrolly + pwinDrawWin->bordertop;
		pts[0].y += yoff; pts[1].y += yoff; pts[2].y += yoff;
		for (i1 = 0; i1 < 3; i1++) {
			if (pts[i1].y < rect.top) rect.top = pts[i1].y;
			if (pts[i1].y > rect.bottom) rect.bottom = pts[i1].y;
			if (pts[i1].x < rect.left) rect.left = pts[i1].x;
			if (pts[i1].x > rect.right) rect.right = pts[i1].x;
		}
		UnionRect(&guiaDrawInvalidRect, &guiaDrawInvalidRect, &rect);
	}
}

/*
 * draw a circle
 */
void guiaDrawCircle()
{
	ZRECT rect;
	HWND hTemp;
	INT radius;
	PIXMAP *ppixDraw;
	WINDOW *pwinDrawWin;

	ppixDraw = *drawpixhandle;
	if (ppixDraw->pentype == DRAW_LINE_REVDOT) {
		SetROP2(ppixDraw->hdc, R2_XORPEN);
	}
	radius = ppixDraw->pos2.x;
	rect.top = ppixDraw->pos1.y - radius;
	rect.bottom = ppixDraw->pos1.y + radius;
	rect.left = ppixDraw->pos1.x - radius;
	rect.right = ppixDraw->pos1.x + radius;
	hTemp = SelectObject(ppixDraw->hdc, hbrushnull);

	/**
	 * If the pen width is more than one, the radius of the overall circle will
	 * be the db/c programmer's requested amount plus one half the pen width
	 */
	Ellipse(ppixDraw->hdc, rect.left, rect.top, rect.right, rect.bottom);
	SelectObject(ppixDraw->hdc, hTemp);
	if (ppixDraw->pentype == DRAW_LINE_REVDOT) {
		SetROP2(ppixDraw->hdc, R2_COPYPEN);
	}
	if (drawwinhandle != NULL) {
		pwinDrawWin = *drawwinhandle;
		OffsetRect(&rect, ppixDraw->hshow - pwinDrawWin->scrollx,
				ppixDraw->vshow - pwinDrawWin->scrolly + pwinDrawWin->bordertop);
		if (ppixDraw->penwidth != 1) {
			LONG x = ppixDraw->penwidth / 2;
			InflateRect(&rect, x, x);
		}
		UnionRect(&guiaDrawInvalidRect, &guiaDrawInvalidRect, &rect);
	}
}

/*
 * draw a big dot
 */
void guiaDrawBigDot()
{
	ZRECT rect;
	INT radius;
	PIXMAP *ppixDraw;
	WINDOW *pwinDrawWin;

	ppixDraw = *drawpixhandle;
	radius = ppixDraw->pos2.x;
	rect.top = ppixDraw->pos1.y - radius;
	rect.bottom = ppixDraw->pos1.y + radius;
	rect.left = ppixDraw->pos1.x - radius;
	rect.right = ppixDraw->pos1.x + radius;
	/**
	 * In the unlikely case that the current pen width is not one,
	 * we need to create a one pixel wide pen. The Win32 function Ellipse
	 * will draw a border around the specified shape in the current line width.
	 * We don't want that. It is not documented and users don't expect it.
	 */
	if (ppixDraw->penwidth != 1) setPenWidthTemporarilyToOne(ppixDraw);
	Ellipse(ppixDraw->hdc, rect.left, rect.top, rect.right, rect.bottom);
	if (ppixDraw->penwidth != 1) restorePenWidthFromTemporaryValue(ppixDraw);

	if (drawwinhandle != NULL) {
		pwinDrawWin = *drawwinhandle;
		OffsetRect(&rect, ppixDraw->hshow - pwinDrawWin->scrollx,
				ppixDraw->vshow - pwinDrawWin->scrolly + pwinDrawWin->bordertop);
		UnionRect(&guiaDrawInvalidRect, &guiaDrawInvalidRect, &rect);
	}
}

/*
 * draw one pixel
 */
void guiaSetPixel()
{
	PIXMAP *ppixDraw;
	WINDOW *pwinDrawWin;

	ppixDraw = *drawpixhandle;
	SetPixel(ppixDraw->hdc, ppixDraw->pos1.x, ppixDraw->pos1.y, ppixDraw->logpen.lopnColor);
	if (drawwinhandle != NULL) {
		RECT rect;
		pwinDrawWin = *drawwinhandle;
		rect.top = rect.bottom = ppixDraw->pos1.y + ppixDraw->vshow - pwinDrawWin->scrolly + pwinDrawWin->bordertop;
		rect.left = rect.right = ppixDraw->pos1.x + ppixDraw->hshow - pwinDrawWin->scrollx;
		UnionRect(&guiaDrawInvalidRect, &guiaDrawInvalidRect, &rect);
	}
}

/*
 * return the color value of one pixel
 */
void guiaGetPixel(PIXMAP *pix, INT32 *iPix, INT h, INT v)
{
	HDC hdc, hDCDraw;
	HBITMAP oldhbitmapsave;

	hdc = GetDC(hwndmainwin);
	hDCDraw = CreateCompatibleDC(hdc);
	ReleaseDC(hwndmainwin, hdc);
	oldhbitmapsave = SelectObject(hDCDraw, pix->hbitmap);
	*iPix = (INT32) GetPixel(hDCDraw, h, v);
	SelectObject(hDCDraw, oldhbitmapsave);
	DeleteDC(hDCDraw);
}

/*
 * draw atext
 */
void guiaDrawAngledText(ZHANDLE text, INT angle)
{
	PIXMAP *ppixDraw;
	HFONT hfnt, hfntprev;
	SIZE size;
	INT len;

	if (text == NULL) return;
	len = (INT)strlen((CHAR *) text);
	/*
	 * The angle used by fonts (escapement) has zero at 'east' or normal text. It goes counter clockwise
	 * and is in tenths of a degree
	 */
	if (angle < 0 || angle >= 360) angle = 0;
	/* convert clockwise value to counter-clockwise */
	if (angle <= 90) angle = 90 - angle;
	else angle = 450 - angle;

	ppixDraw = *drawpixhandle;
	ppixDraw->lf.lfOrientation = ppixDraw->lf.lfEscapement = angle * 10;
	hfnt = getFont(&ppixDraw->lf);
	hfntprev = SelectObject(ppixDraw->hdc, hfnt);
	ppixDraw->lf.lfOrientation = ppixDraw->lf.lfEscapement = 0;
	TextOut(ppixDraw->hdc, ppixDraw->pos1.x, ppixDraw->pos1.y, (LPCSTR) text, len);
	SelectObject(ppixDraw->hdc, hfntprev);
	if (drawwinhandle != NULL) {
		WINDOW *pwinDrawWin = *drawwinhandle;
		RECT rect;
		INT x = ppixDraw->pos1.x + ppixDraw->hshow - pwinDrawWin->scrollx;
		INT y = ppixDraw->pos1.y + ppixDraw->vshow - pwinDrawWin->scrolly + pwinDrawWin->bordertop;
		GetTextExtentPoint32(ppixDraw->hdc, (LPCSTR) text, len, &size);
		/*
		 * Too much trouble to do the trig to find the optimal
		 * 'minimum rectangle', and probably not worth it.
		 * So we find one a little bigger.
		 */
		rect.left = max(0, x - size.cx);
		rect.right = x + size.cx;
		rect.top = max (0, y - size.cx);
		rect.bottom = y + size.cx;
		UnionRect(&guiaDrawInvalidRect, &guiaDrawInvalidRect, &rect);
	}
}


/*
 * draw text
 */
void guiaDrawText(ZHANDLE text, UINT align)
{
	PIXMAP *ppixDraw;
	WINDOW *pwinDrawWin;
	SIZE size;
	UINT mask;
	INT x, y, len;
	RECT rect;

	if (text == NULL) return;
	len = (INT)strlen((CHAR *) text);
	switch (align) {
		case DRAW_TEXT_LEFT: align = TA_LEFT; break;
		case DRAW_TEXT_CENTER: align = TA_CENTER; break;
		case DRAW_TEXT_RIGHT: align = TA_RIGHT; break;
	}
	mask = TA_TOP | TA_NOUPDATECP | align;
	ppixDraw = *drawpixhandle;
	SetTextAlign(ppixDraw->hdc, mask);
	TextOut(ppixDraw->hdc, ppixDraw->pos1.x, ppixDraw->pos1.y, (LPCSTR) text, len);
	GetTextExtentPoint32(ppixDraw->hdc, (LPCSTR) text, len, &size);
	if (drawwinhandle != NULL) {
		pwinDrawWin = *drawwinhandle;
		x = ppixDraw->pos1.x + ppixDraw->hshow - pwinDrawWin->scrollx;
		y = ppixDraw->pos1.y + ppixDraw->vshow - pwinDrawWin->scrolly + pwinDrawWin->bordertop;
		rect.top = y;
		rect.bottom = y + size.cy;
		switch (align) {
		case TA_LEFT:
			rect.left = x;
			rect.right = x + size.cx;
			break;
		case TA_CENTER:
			rect.left = x - size.cx / 2;
			rect.right = x + size.cx / 2;
			break;
		case TA_RIGHT:
			rect.left = x - size.cx;
			rect.right = x;
			break;
		}
		UnionRect(&guiaDrawInvalidRect, &guiaDrawInvalidRect, &rect);
	}
	/**
	 * Bug fix 16 APR 2013 jpr
	 * CTEXT and RTEXT are not supposed to change the current draw position.
	 * The below code did not have the 'if'. It was advancing the position
	 * for right and center alignement.
	 */
	if (align == TA_LEFT) ppixDraw->pos1.x += size.cx;
}

/*
 * draw a line
 */
void guiaDrawLine()
{
	PIXMAP *ppixDraw;
	WINDOW *pwinDrawWin;

	ppixDraw = *drawpixhandle;
	if (ppixDraw->pentype == DRAW_LINE_REVDOT) {
		SetROP2(ppixDraw->hdc, R2_XORPEN);
	}
	MoveToEx(ppixDraw->hdc, ppixDraw->pos1.x, ppixDraw->pos1.y, NULL);
	LineTo(ppixDraw->hdc, ppixDraw->pos2.x, ppixDraw->pos2.y);
	/* LineTo does not include endpoint */
	SetPixel(ppixDraw->hdc, ppixDraw->pos2.x, ppixDraw->pos2.y, ppixDraw->logpen.lopnColor);
	if (ppixDraw->pentype == DRAW_LINE_REVDOT) {
		SetROP2(ppixDraw->hdc, R2_COPYPEN);
	}

	if (drawwinhandle != NULL) {
		ZRECT rect;
		INT p1x, p1y, p2x, p2y;
		pwinDrawWin = *drawwinhandle;
		p1x = ppixDraw->pos1.x + ppixDraw->hshow - pwinDrawWin->scrollx;
		p2x = ppixDraw->pos2.x + ppixDraw->hshow - pwinDrawWin->scrollx;
		p1y = ppixDraw->pos1.y + ppixDraw->vshow - pwinDrawWin->scrolly + pwinDrawWin->bordertop;
		p2y = ppixDraw->pos2.y + ppixDraw->vshow - pwinDrawWin->scrolly + pwinDrawWin->bordertop;
		rect.top = min(p1y, p2y);
		rect.bottom = max(p1y, p2y) + 1;
		rect.left = min(p1x, p2x);
		rect.right = max(p1x, p2x) + 1;
		if (ppixDraw->penwidth != 1) {
			LONG x = ppixDraw->penwidth;
			InflateRect(&rect, x, x);
		}
		UnionRect(&guiaDrawInvalidRect, &guiaDrawInvalidRect, &rect);
	}
}

/*
 * draw a box
 */
void guiaDrawBox()
{
	PIXMAP *ppixDraw;
	WINDOW *pwinDrawWin;

	ppixDraw = *drawpixhandle;
	if (DRAW_LINE_REVDOT == ppixDraw->pentype) {
		SetROP2(ppixDraw->hdc, R2_XORPEN);
	}
	MoveToEx(ppixDraw->hdc, ppixDraw->pos1.x, ppixDraw->pos1.y, NULL);
	LineTo(ppixDraw->hdc, ppixDraw->pos1.x, ppixDraw->pos2.y);
	LineTo(ppixDraw->hdc, ppixDraw->pos2.x, ppixDraw->pos2.y);
	LineTo(ppixDraw->hdc, ppixDraw->pos2.x, ppixDraw->pos1.y);
	LineTo(ppixDraw->hdc, ppixDraw->pos1.x, ppixDraw->pos1.y);
	if (DRAW_LINE_REVDOT == ppixDraw->pentype) {
		SetROP2(ppixDraw->hdc, R2_COPYPEN);
	}

	if (drawwinhandle != NULL) {
		ZRECT rect;
		INT p1x, p1y, p2x, p2y;
		pwinDrawWin = *drawwinhandle;
		p1y = ppixDraw->pos1.y + ppixDraw->vshow - pwinDrawWin->scrolly + pwinDrawWin->bordertop;
		p2y = ppixDraw->pos2.y + ppixDraw->vshow - pwinDrawWin->scrolly + pwinDrawWin->bordertop;
		p1x = ppixDraw->pos1.x + ppixDraw->hshow - pwinDrawWin->scrollx;
		p2x = ppixDraw->pos2.x + ppixDraw->hshow - pwinDrawWin->scrollx;
		rect.top = min(p1y, p2y);
		rect.bottom = max(p1y, p2y) + 1;
		rect.left = min(p1x, p2x);
		rect.right = max(p1x, p2x) + 1;
		if (ppixDraw->penwidth != 1) {
			LONG x = ppixDraw->penwidth / 2;
			InflateRect(&rect, x, x);
		}
		UnionRect(&guiaDrawInvalidRect, &guiaDrawInvalidRect, &rect);
	}
}

void guiaDrawImageRotationAngle(INT angle)
{
	irotation = angle;
}

/*
 * guiaGetRotatedBitmap - Create a new bitmap with rotated image on top of background
 */
static HBITMAP guiaGetRotatedBitmap(PIXMAP *pixsrc, PIXMAP *pixback, INT angle, INT *p1x, INT *p1y)
{
	INT x1, x2, x3, y1, y2, y3, minx, miny, maxx, maxy;
	INT width, height, sourcex, sourcey, destx, desty, linesize;
	INT iBitCount, iClrUsed, iBitmapRes;
	INT32 color;
	DOUBLE cosine, sine, radian;
	BYTE *pBits, *qBits, *ptr1;
	HDC sourceDC, destDC, tmpDC;
	HBITMAP hbmResult, hbmOldSource, hbmOldDest;
	HPALETTE oldhPalette;
	BITMAPINFO *pbmi;
	BITMAP bm;
	PIXMAP psrc;

	radian = (M_PI * angle) / 180.0f;
	cosine = cos(radian);
	sine = sin(radian);
	oldhPalette = NULL;
	sourceDC = CreateCompatibleDC(NULL);
	destDC = CreateCompatibleDC(NULL);
	if (pixsrc->hpal != NULL) {
		oldhPalette = SelectPalette(sourceDC, pixsrc->hpal, TRUE);
		RealizePalette(sourceDC);
	}
	GetObject(pixsrc->hbitmap, sizeof(BITMAP), (LPVOID) &bm);

	/****** New code for GetPixel replacement follows ******/

	iBitmapRes = bm.bmPlanes * bm.bmBitsPixel;
	if (iBitmapRes == 1) {
		iClrUsed = 2;
		iBitCount = 1;
	}
	else if (iBitmapRes <= 4) {
		iClrUsed = 16;
		iBitCount = 4;
	}
	else if (iBitmapRes <= 8) {
		iClrUsed = 256;
		iBitCount = 8;
	}
	else {
		iClrUsed = 0;
		iBitCount = 24;
	}
	pbmi = (BITMAPINFO *) guiAllocMem(sizeof(BITMAPINFOHEADER) + iClrUsed * sizeof(RGBQUAD));
	if (pbmi == NULL) {
		guiaErrorMsg("Out of guiAllocMem memory in guiaGetRotatedBitmap()", 0);
		if (oldhPalette != NULL) {
			SelectPalette(sourceDC, oldhPalette, TRUE);
			RealizePalette(sourceDC);
		}
		DeleteDC(destDC);
		DeleteDC(sourceDC);
		return pixsrc->hbitmap;
	}

	/* initialize BITMAPINFOHEADER in BITMAPINFO structure */
	pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pbmi->bmiHeader.biWidth = bm.bmWidth;
	pbmi->bmiHeader.biHeight = bm.bmHeight;
	pbmi->bmiHeader.biPlanes = 1;
	pbmi->bmiHeader.biBitCount = iBitCount;
	pbmi->bmiHeader.biCompression = BI_RGB;
	pbmi->bmiHeader.biSizeImage = 0;
	pbmi->bmiHeader.biXPelsPerMeter = 0;
	pbmi->bmiHeader.biYPelsPerMeter = 0;
	pbmi->bmiHeader.biClrUsed = 0;
	pbmi->bmiHeader.biClrImportant = 0;
	pbmi->bmiHeader.biSizeImage = ((((bm.bmWidth * iBitCount) + 31) & ~31) >> 3) * bm.bmHeight;

	pBits = (BYTE *) guiAllocMem(pbmi->bmiHeader.biSizeImage);
	if (pBits == NULL) {
		guiaErrorMsg("Out of guiAllocMem memory in guiaGetRotatedBitmap()", 0);
		guiFreeMem((ZHANDLE) pbmi);
		if (oldhPalette != NULL) {
			SelectPalette(sourceDC, oldhPalette, TRUE);
			RealizePalette(sourceDC);
		}
		DeleteDC(destDC);
		DeleteDC(sourceDC);
		return pixsrc->hbitmap;
	}

	tmpDC = GetDC(NULL);
	if (!GetDIBits(GetDC(NULL), pixsrc->hbitmap, 0, bm.bmHeight, pBits, pbmi, DIB_RGB_COLORS)) {
		guiaErrorMsg("GetDIBits() failed in guiaGetRotatedBitmap()", GetLastError());
		guiFreeMem(pBits);
		guiFreeMem((ZHANDLE) pbmi);
		if (oldhPalette != NULL) {
			SelectPalette(sourceDC, oldhPalette, TRUE);
			RealizePalette(sourceDC);
		}
		DeleteDC(destDC);
		DeleteDC(sourceDC);
		ReleaseDC(NULL, tmpDC);
		return pixsrc->hbitmap;
	}
	ReleaseDC(NULL, tmpDC);
	if (oldhPalette != NULL) {
		SelectPalette(sourceDC, oldhPalette, TRUE);
		RealizePalette(sourceDC);
	}

	bm.bmBits = pBits;
	bm.bmBitsPixel = iBitCount;
	bm.bmWidthBytes = ((bm.bmWidth * bm.bmBitsPixel + 0x1F) & ~0x1F) >> 3;

	/****** ******/

	/* Get coordinates for the 3 corners other than origin for the result bitmap */
	x1 = (INT) (-bm.bmHeight * sine);
	y1 = (INT) (bm.bmHeight * cosine);
	x2 = (INT) (bm.bmWidth * cosine - bm.bmHeight * sine);
	y2 = (INT) (bm.bmHeight * cosine + bm.bmWidth * sine);
	x3 = (INT) (bm.bmWidth * cosine);
	y3 = (INT) (bm.bmWidth * sine);

	minx = min(0, min(x1, min(x2, x3)));
	miny = min(0, min(y1, min(y2, y3)));
	maxx = max(0, max(x1, max(x2, x3)));
	maxy = max(0, max(y1, max(y2, y3)));

	width = (maxx - minx) + 1;
	height = (maxy - miny) + 1;

	/****** New code for SetPixel replacement follows ******/

	linesize = (((width * 24) + 31) & ~31) >> 3;
	pbmi->bmiHeader.biBitCount = 24;
	pbmi->bmiHeader.biSizeImage = linesize * height;
	pbmi->bmiHeader.biWidth = width;
	pbmi->bmiHeader.biHeight = height;
	hbmResult = CreateDIBSection(tmpDC = GetDC(NULL), pbmi, DIB_RGB_COLORS, (PVOID*) &qBits, NULL, 0);
	if (hbmResult == NULL) {
		guiaErrorMsg("CreateDIBSection() failed in guiaGetRotatedBitmap()", GetLastError());
		guiFreeMem(pBits);
		guiFreeMem((ZHANDLE) pbmi);
		DeleteDC(destDC);
		DeleteDC(sourceDC);
		return pixsrc->hbitmap;
	}

	/****** ******/

	hbmOldSource = (HBITMAP) SelectObject(sourceDC, pixsrc->hbitmap);
	hbmOldDest = (HBITMAP) SelectObject(destDC, hbmResult);
	/* copy background onto bitmap before we draw rotated image */
	if (angle <= 90) {
		radian = (M_PI * angle) / 180.0f;
		*p1y = *p1y - (INT) (bm.bmWidth * sin(radian));
		*p1x = *p1x - 1;
	}
	else if (angle <= 180) {
		radian = (M_PI * (180 - angle)) / 180.0f;
		*p1y = *p1y - height + 1;
		*p1x = *p1x - width + 2 + (INT) (bm.bmHeight * sin(radian));
	}
	else if (angle <= 270) {
		radian = (M_PI * (270 - angle)) / 180.0f;
		*p1y = *p1y - (INT) (bm.bmHeight * sin(radian));
		*p1x = *p1x - width + 2;
	}
	else {
		radian = (M_PI * (360 - angle)) / 180.0f;
		*p1x = *p1x - (INT) (bm.bmHeight * sin(radian));
	}
	psrc.hdc = destDC;
	guiaPixBitBlt(&psrc, 0, 0, width, height, pixback, *p1x, *p1y, width, height);

	/* Set mapping mode so that +ve y axis is upwards */
	SetMapMode(sourceDC, MM_ISOTROPIC);
	SetWindowExtEx(sourceDC, 1 , 1, NULL);
	SetViewportExtEx(sourceDC, 1 , -1, NULL);
	SetViewportOrgEx(sourceDC, 0, bm.bmHeight - 1, NULL);
	SetMapMode(destDC, MM_ISOTROPIC);
	SetWindowExtEx(destDC, 1, 1, NULL);
	SetViewportExtEx(destDC, 1, -1, NULL);
	SetWindowOrgEx(destDC, minx, maxy, NULL);

	GdiFlush(); /* Must make sure subsystem has completed any drawing to the image */

	/**
	 * Now do the actual rotating one pixel at a time. Computing the
	 * destination point for each source point will leave a few pixels
	 * that do not get covered so we use a reverse transform.
	 *
	 * For performance, cosine & sine for horizontal/vertical flip (180) is not used.
	 *
	**/
	if (angle == 180) sourcey = bm.bmHeight - 1;
	for (y1 = miny, desty = 0; y1 <= maxy; y1++, desty++) {
		if (angle == 180) sourcex = bm.bmWidth - 1;
		for (x1 = minx, destx = 0; x1 <= maxx; x1++, destx++) {
			if (angle != 180) {
				/* extra .5 is to help with cast from double to int */
				sourcex = (int) ((x1 * cosine + y1 * sine) + .5);
				sourcey = (int) ((y1 * cosine - x1 * sine) + .5);
			}
			if (destx < 0 || destx >= width || desty < 0 || desty >= height) {
				continue; /* safety - should never happen */
			}
			if (sourcex < 0 || sourcex >= bm.bmWidth || sourcey < 0 || sourcey >= bm.bmHeight) {
				continue; /* safety - should never happen */
			}
			color = guiaBitmapGetPixel(bm, sourcex, sourcey, FALSE);
			ptr1 = qBits + ((linesize * desty) + (destx * 3));
			*ptr1 = (BYTE) (color >> 16);
			ptr1++;
			*ptr1 = (BYTE) ((color >> 8) & 0xFF);
			ptr1++;
			*ptr1 = (BYTE) (color & 0xFF);
			if (angle == 180) sourcex--;
		}
		if (angle == 180) sourcey--;
	}

	/* Restore DCs */
	SelectObject(sourceDC, hbmOldSource);
	SelectObject(destDC, hbmOldDest);

	/* Clean up */
	DeleteObject(hbmOldSource);
	DeleteObject(hbmOldDest);
	DeleteDC(destDC);
	DeleteDC(sourceDC);
	ReleaseDC(NULL, tmpDC);
	guiFreeMem(pBits);
	guiFreeMem((ZHANDLE) pbmi);
	/* caller must issue Deleteobject() on hbmResult when it is done with it */
	return hbmResult;
}

/* This is a much faster replacement for the Win32 API GetPixel() call - SSN */
static INT32 guiaBitmapGetPixel(BITMAP bm, INT h1, INT v1, INT reverse)
{
	static RGBQUAD quadclrs[256];
	static INT initquad = FALSE;
	INT iCurClr, iCurRed, iCurGreen, iCurBlue;
	INT32 color;
	BYTE *ptr1, *ptrend, ucGrayShade;

	if (reverse) {
		/* bitmap scanlines are stored in reverse order, so start from the end */
		ptrend = (BYTE *) bm.bmBits + bm.bmWidthBytes * (bm.bmHeight - 1);
		ptr1 = (BYTE *) ptrend - bm.bmWidthBytes * v1;
	}
	else {
		ptr1 = (BYTE *) bm.bmBits + bm.bmWidthBytes * v1;
	}
	if (bm.bmBitsPixel == 1) {
		ptr1 = ptr1 + (h1 >> 3); /* h1 divide 8 */
		if ((128 >> (h1 % 8)) & *ptr1) color = 0x000000;
		else color = 0xFFFFFF;
	}
	else if (bm.bmBitsPixel == 4) {
		ptr1 = ptr1 + (h1 >> 1); /* h1 divide 2 */
		switch ((h1 & 0x1) ? (*ptr1 & 0x0F) : (*ptr1 & 0xF0) >> 4) {
			case 0x0: color = 0x000000; break; /* dark black */
			case 0x1: color = 0x800000; break; /* dark blue */
			case 0x2: color = 0x008000; break; /* dark green */
			case 0x3: color = 0x808000; break; /* dark cyan */
			case 0x4: color = 0x000080; break; /* dark red */
			case 0x5: color = 0x800080; break; /* dark magenta */
			case 0x6: color = 0x008080; break; /* dark yellow */
			case 0x7: color = 0xC0C0C0; break; /* light gray */
			case 0x8: color = 0x808080; break; /* medium gray */
			case 0x9: color = 0xFF0000; break; /* blue */
			case 0xA: color = 0x00FF00; break; /* green */
			case 0xB: color = 0xFFFF00; break; /* cyan */
			case 0xC: color = 0x0000FF; break; /* red */
			case 0xD: color = 0xFF00FF; break; /* magenta */
			case 0xE: color = 0x00FFFF; break; /* yellow */
			case 0xF: color = 0xFFFFFF; break; /* white */
		}
	}
	else if (bm.bmBitsPixel == 8) {
		if (!initquad) {
		/* fill in colors used for 16 and 256 color palette modes */
			for (iCurClr = 0; iCurClr < 16; iCurClr++) {
				quadclrs[iCurClr].rgbBlue = DBC_BLUE(dwDefaultPal[iCurClr]);
				quadclrs[iCurClr].rgbGreen = DBC_GREEN(dwDefaultPal[iCurClr]);
				quadclrs[iCurClr].rgbRed = DBC_RED(dwDefaultPal[iCurClr]);
			}

		/* fill in colors used only in 256 color palette modes */
			for (iCurRed = 0; iCurRed < 256; iCurRed += 51) {
				for (iCurGreen = 0; iCurGreen < 256; iCurGreen += 51) {
					for (iCurBlue = 0; iCurBlue < 256; iCurBlue += 51) {
						quadclrs[iCurClr].rgbBlue = (BYTE) iCurBlue;
						quadclrs[iCurClr].rgbGreen = (BYTE) iCurGreen;
						quadclrs[iCurClr].rgbRed = (BYTE) iCurRed;
						iCurClr++;
					}
				}
			}
		/* finish off with a spectrum of gray shades */
			ucGrayShade = 8;
			for (; iCurClr < 256; iCurClr++) {
				quadclrs[iCurClr].rgbBlue = ucGrayShade;
				quadclrs[iCurClr].rgbGreen = ucGrayShade;
				quadclrs[iCurClr].rgbRed = ucGrayShade;
				ucGrayShade += 8;
			}
			initquad = TRUE;
		}
		ptr1 += h1;
		color = (quadclrs[*ptr1].rgbBlue << 16) | (quadclrs[*ptr1].rgbGreen << 8) | quadclrs[*ptr1].rgbRed;
	}
	else if (bm.bmBitsPixel == 24) {
		ptr1 = ptr1 + (h1 * 3);
		color = *ptr1 << 16;
		ptr1++;
		color |= *ptr1 << 8;
		ptr1++;
		color |= *ptr1;
	}
	return color;
}

static int getStretchBltMode(void) {
	switch (imageDrawStretchMode) {
	case 1:
		return STRETCH_DELETESCANS;
	case 2:
		return STRETCH_HALFTONE;
	case 3:
		return STRETCH_ANDSCANS;
	case 4:
		return STRETCH_ORSCANS;
	default:
		return STRETCH_DELETESCANS;
	}
}

/*
 * copy a pixel map with stretching
 */
void guiaStretchCopyPixmap(PIXMAP *pix, INT stretch)
{
	HDC hdcSrc, hdcSrcMem;
	INT iBitmapRes, hsize, vsize, p1x, p1y, p2x, p2y, angle;
	BITMAP bm;
	HBITMAP hbmOldDisplay, oldbitmap, sbitmap;
	RECT destrect;
	PIXMAP *ppixDraw, psrc;
	WINDOW *pwinDrawWin;
	int stretchBltMode = getStretchBltMode();

	ppixDraw = *drawpixhandle;

	if (irotation != NoRotation) {
		angle = irotation;
		//TODO Implement new image rotate.
		//     But not for some time. Maybe 17.1
		//if (imageRotateOld) {
			if (angle < 0 || angle >= 360) angle = 0;
			/* convert clockwise value to counter-clockwise */
			if (angle <= 90) angle = 90 - angle;
			else angle = 450 - angle;
		//}
		oldbitmap = pix->hbitmap;
		p1x = ppixDraw->pos1.x;
		p1y = ppixDraw->pos1.y;
		if (stretch) {
			p2x = ppixDraw->pos2.x; /* stretched width */
			p2y = ppixDraw->pos2.y; /* stretched height */
			hdcSrc = GetDC(NULL);
			hdcSrcMem = CreateCompatibleDC(hdcSrc);
			sbitmap = CreateCompatibleBitmap(hdcSrc, p2x, p2y);
			hbmOldDisplay = SelectObject(hdcSrcMem, sbitmap);
			psrc.hdc = hdcSrcMem;
			psrc.hbitmap = sbitmap;
			SetStretchBltMode(psrc.hdc, stretchBltMode);
			if (stretchBltMode == STRETCH_HALFTONE) SetBrushOrgEx(psrc.hdc, 0, 0, NULL);
			/* place the stretched image into a new image */
			guiaPixBitBlt(&psrc, 0, 0, p2x, p2y, pix, 0, 0, pix->hsize, pix->vsize);
			SelectObject(hdcSrcMem, hbmOldDisplay);
			DeleteDC(hdcSrcMem);
			ReleaseDC(NULL, hdcSrc);
			/* rotate it */
			pix->hbitmap = guiaGetRotatedBitmap(&psrc, ppixDraw, angle, &p1x, &p1y);
		}
		else {
			pix->hbitmap = guiaGetRotatedBitmap(pix, ppixDraw, angle, &p1x, &p1y);
		}
		GetObject(pix->hbitmap, sizeof(BITMAP), (LPVOID) &bm);
		hsize = p2x = bm.bmWidth;
		vsize = p2y = bm.bmHeight;
	}
	else {
		hsize = pix->hsize;
		vsize = pix->vsize;
		p1x = ppixDraw->pos1.x;
		p1y = ppixDraw->pos1.y;
		p2x = ppixDraw->pos2.x;
		p2y = ppixDraw->pos2.y;
		GetObject(pix->hbitmap, sizeof(BITMAP), (LPVOID) &bm);
	}

	iBitmapRes = bm.bmPlanes * bm.bmBitsPixel;

	SetStretchBltMode(ppixDraw->hdc, stretchBltMode);
	if (stretchBltMode == STRETCH_HALFTONE) SetBrushOrgEx(psrc.hdc, 0, 0, NULL);
	if (pix == ppixDraw) {
		StretchBlt(ppixDraw->hdc, p1x, p1y, p2x, p2y, ppixDraw->hdc, 0, 0, hsize, vsize, SRCCOPY);
	}
	else if (pix->bpp == ppixDraw->bpp && iBitmapRes != 8) {
		hdcSrc = GetDC(NULL);
		hdcSrcMem = CreateCompatibleDC(hdcSrc);
		hbmOldDisplay = SelectObject(hdcSrcMem, pix->hbitmap);
		StretchBlt(ppixDraw->hdc, p1x, p1y, p2x, p2y, hdcSrcMem, 0, 0, hsize, vsize, SRCCOPY);
		SelectObject(hdcSrcMem, hbmOldDisplay);
		DeleteDC(hdcSrcMem);
		ReleaseDC(NULL, hdcSrc);
	}
	else {
		guiaPixBitBlt(ppixDraw, p1x, p1y, p2x, p2y, pix, 0, 0, hsize, vsize);
	}

	if (drawwinhandle != NULL) {
		pwinDrawWin = *drawwinhandle;
		destrect.left = ppixDraw->pos1.x;
		destrect.right = destrect.left + ppixDraw->pos2.x;
		destrect.top = ppixDraw->pos1.y;
		destrect.bottom = destrect.top + ppixDraw->pos2.y;
		destrect.top += ppixDraw->vshow - pwinDrawWin->scrolly + pwinDrawWin->bordertop;
		destrect.bottom += ppixDraw->vshow - pwinDrawWin->scrolly + pwinDrawWin->bordertop;
		destrect.left += ppixDraw->hshow - pwinDrawWin->scrollx;
		destrect.right += ppixDraw->hshow - pwinDrawWin->scrollx;
		UnionRect(&guiaDrawInvalidRect, &guiaDrawInvalidRect, &destrect);
	}

	if (irotation != NoRotation) {
		if (stretch) DeleteObject(sbitmap); /* temporary stretched image for rotation */
		DeleteObject(pix->hbitmap); /* created from guiaGetRotatedBitmap call */
		pix->hbitmap = oldbitmap; /* restore pointer to original bitmap */
	}
}

/*
 * copy a pixel map with clipping
 */
void guiaClipCopyPixmap(PIXMAP *pix)
{
	INT hsize, vsize, n, iBitmapRes;
	INT p1x, p1y, p2x, p2y, angle;
	BITMAP bm;
	HDC hdcSrc, hdcSrcMem;
	HBITMAP hbmOldDisplay, sbitmap, oldbitmap;
	RECT srcrect, destrect;
	PIXMAP *ppixDraw, psrc;
	WINDOW *pwinDrawWin;

	ppixDraw = *drawpixhandle;

	srcrect.left = ppixDraw->pos2.x;
	srcrect.top = ppixDraw->pos2.y;
	srcrect.right = ppixDraw->pos3.x;
	srcrect.bottom = ppixDraw->pos3.y;
	if (srcrect.right < srcrect.left) {
		n = srcrect.right;
		srcrect.right = srcrect.left;
		srcrect.left = n;
	}
	if (srcrect.bottom < srcrect.top) {
		n = srcrect.bottom;
		srcrect.bottom = srcrect.top;
		srcrect.top = n;
	}
	destrect.left = ppixDraw->pos1.x;
	destrect.right = destrect.left + srcrect.right - srcrect.left;
	destrect.top = ppixDraw->pos1.y;
	destrect.bottom = destrect.top + srcrect.bottom - srcrect.top;
	if (destrect.right > ppixDraw->hsize - 1) {
		n = destrect.right - ppixDraw->hsize + 1;
		destrect.right -= n;
	}
	if (destrect.bottom > ppixDraw->vsize - 1) {
		n = destrect.bottom - ppixDraw->vsize + 1;
		destrect.bottom -= n;
	}
/* srcrect.right and srcrect.bottom are no longer valid */

	hsize = destrect.right - destrect.left + 1;
	vsize = destrect.bottom - destrect.top + 1;
	if (irotation != NoRotation) {
		angle = irotation;
		if (angle < 0 || angle >= 360) angle = 0;
		/* convert clockwise value to counter-clockwise */
		if (angle <= 90) angle = 90 - angle;
		else angle = 450 - angle;
		oldbitmap = pix->hbitmap;
		p1x = ppixDraw->pos1.x;
		p1y = ppixDraw->pos1.y;
		hdcSrc = GetDC(NULL);
		hdcSrcMem = CreateCompatibleDC(hdcSrc);
		sbitmap = CreateCompatibleBitmap(hdcSrc, hsize, vsize);
		hbmOldDisplay = SelectObject(hdcSrcMem, sbitmap);
		psrc.hdc = hdcSrcMem;
		psrc.hbitmap = sbitmap;
		SetStretchBltMode(psrc.hdc, STRETCH_DELETESCANS);
		/* draw clipped image onto new bitmap */
		guiaPixBitBlt(&psrc, 0, 0, hsize, vsize, pix, srcrect.left, srcrect.top, hsize, vsize);
		SelectObject(hdcSrcMem, hbmOldDisplay);
		DeleteDC(hdcSrcMem);
		ReleaseDC(NULL, hdcSrc);
		/* rotate the new bitmap */
		pix->hbitmap = guiaGetRotatedBitmap(&psrc, ppixDraw, angle, &p1x, &p1y);
		GetObject(pix->hbitmap, sizeof(BITMAP), (LPVOID) &bm);
		hsize = p2x = bm.bmWidth;
		vsize = p2y = bm.bmHeight;
	}
	else {
		p1x = destrect.left;
		p1y = destrect.top;
		p2x = srcrect.left;
		p2y = srcrect.top;
		GetObject(pix->hbitmap, sizeof(BITMAP), (LPVOID) &bm);
	}

	iBitmapRes = bm.bmPlanes * bm.bmBitsPixel;

	SetStretchBltMode(ppixDraw->hdc, STRETCH_DELETESCANS);
	if (pix == ppixDraw) {
		if (irotation != NoRotation) {
			/* draw rotated image onto destination image */
			StretchBlt(ppixDraw->hdc, p1x, p1y, p2x, p2y, ppixDraw->hdc, 0, 0, hsize, vsize, SRCCOPY);
		}
		else {
			/* draw clipped image onto destination image */
			StretchBlt(ppixDraw->hdc, p1x, p1y, hsize, vsize, ppixDraw->hdc, p2x, p2y, hsize, vsize, SRCCOPY);
		}
	}
	else if (pix->bpp == ppixDraw->bpp && iBitmapRes != 8) {
		hdcSrc = GetDC(NULL);
		hdcSrcMem = CreateCompatibleDC(hdcSrc);
		hbmOldDisplay = SelectObject(hdcSrcMem, pix->hbitmap);
		if (irotation != NoRotation) {
			/* draw rotated image onto destination image */
			StretchBlt(ppixDraw->hdc, p1x, p1y, p2x, p2y, hdcSrcMem, 0, 0, hsize, vsize, SRCCOPY);
		}
		else {
			/* draw clipped image onto destination image */
			StretchBlt(ppixDraw->hdc, p1x, p1y, hsize, vsize, hdcSrcMem, p2x, p2y, hsize, vsize, SRCCOPY);
		}
		SelectObject(hdcSrcMem, hbmOldDisplay);
		DeleteDC(hdcSrcMem);
		ReleaseDC(NULL, hdcSrc);
	}
	else {
		if (irotation != NoRotation) {
			/* draw rotated image onto destination image */
			guiaPixBitBlt(ppixDraw, p1x, p1y, p2x, p2y, pix, 0, 0, hsize, vsize);
		}
		else {
			/* draw clipped image onto destination image */
			guiaPixBitBlt(ppixDraw, p1x, p1y, hsize, vsize, pix, p2x, p2y, hsize, vsize);
		}
	}

	if (drawwinhandle != NULL) {
		pwinDrawWin = *drawwinhandle;
		destrect.top += ppixDraw->vshow - pwinDrawWin->scrolly + pwinDrawWin->bordertop;
		destrect.bottom += ppixDraw->vshow - pwinDrawWin->scrolly + pwinDrawWin->bordertop + 1;
		destrect.left += ppixDraw->hshow - pwinDrawWin->scrollx;
		destrect.right += (ppixDraw->hshow + 1) - pwinDrawWin->scrollx;
		UnionRect(&guiaDrawInvalidRect, &guiaDrawInvalidRect, &destrect);
	}

	if (irotation != NoRotation) {
		DeleteObject(sbitmap); /* temporary stretched image for rotation */
		DeleteObject(pix->hbitmap); /* created from guiaGetRotatedBitmap call */
		pix->hbitmap = oldbitmap; /* restore pointer to original bitmap */
	}
}

/*
 * pixmap bitblt for mismatched pixmaps
 */
static void guiaPixBitBlt(PIXMAP *destpix, INT destx, INT desty, INT destwidth, INT destheight,
	PIXMAP *srcpix, INT srcx, INT srcy, INT srcwidth, INT srcheight)
{
	BITMAPINFO *pbmi;
	HDC hdcSrc;
	INT iClrUsed, iBitmapRes, iBitCount, i1;
	BYTE *pBits;
	HPALETTE oldhPalette;
	BITMAP bm;

	hdcSrc = GetDC(NULL);
	oldhPalette = NULL;
	if (srcpix->hpal != NULL) {
		oldhPalette = SelectPalette(hdcSrc, srcpix->hpal, TRUE);
		RealizePalette(hdcSrc);
	}
	GetObject(srcpix->hbitmap, sizeof(BITMAP), (LPVOID) &bm);
	iBitmapRes = bm.bmPlanes * bm.bmBitsPixel;
	if (iBitmapRes == 1) {
		iClrUsed = 2;
		iBitCount = 1;
	}
	else if (iBitmapRes <= 4) {
		iClrUsed = 16;
		iBitCount = 4;
	}
	else if (iBitmapRes <= 8) {
		iClrUsed = 256;
		iBitCount = 8;
	}
	else {
		iClrUsed = 0;
		iBitCount = 24;
	}

	pbmi = (BITMAPINFO *) guiAllocMem(sizeof(BITMAPINFOHEADER) + iClrUsed * sizeof(RGBQUAD));
	if (pbmi == NULL) {
		guiaErrorMsg("Out of guiAllocMem memory in guiaPixBitBlt", sizeof(BITMAPINFOHEADER) + iClrUsed * sizeof(RGBQUAD));
		if (oldhPalette != NULL) {
			SelectPalette(hdcSrc, oldhPalette, TRUE);
			RealizePalette(hdcSrc);
		}
		ReleaseDC(NULL, hdcSrc);
		return;
	}

/* initialize BITMAPINFOHEADER in BITMAPINFO structure */
	pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pbmi->bmiHeader.biWidth = bm.bmWidth;
	pbmi->bmiHeader.biHeight = bm.bmHeight;
	pbmi->bmiHeader.biPlanes = 1;
	pbmi->bmiHeader.biBitCount = iBitCount;
	pbmi->bmiHeader.biCompression = BI_RGB;
	pbmi->bmiHeader.biSizeImage = 0;
	pbmi->bmiHeader.biXPelsPerMeter = 0;
	pbmi->bmiHeader.biYPelsPerMeter = 0;
	pbmi->bmiHeader.biClrUsed = 0;
	pbmi->bmiHeader.biClrImportant = 0;
	pbmi->bmiHeader.biSizeImage = ((((bm.bmWidth * iBitCount) + 31) & ~31) >> 3) * bm.bmHeight;

	pBits = (BYTE *) guiAllocMem(pbmi->bmiHeader.biSizeImage);
	if (pBits == NULL) {
		guiaErrorMsg("Out of guiAllocMem memory in guiaPixBitBlt", pbmi->bmiHeader.biSizeImage);
		guiFreeMem((ZHANDLE) pbmi);
		if (oldhPalette != NULL) {
			SelectPalette(hdcSrc, oldhPalette, TRUE);
			RealizePalette(hdcSrc);
		}
		ReleaseDC(NULL, hdcSrc);
		return;
	}

/* do NOT select srcpix-hbitmap into the hdcSrc DC before this call */
	if (!GetDIBits(hdcSrc, srcpix->hbitmap, 0, bm.bmHeight, pBits, pbmi, DIB_RGB_COLORS)) {
		guiaErrorMsg("GetDIBits() failed", GetLastError());
		guiFreeMem(pBits);
		guiFreeMem((ZHANDLE) pbmi);
		if (oldhPalette != NULL) {
			SelectPalette(hdcSrc, oldhPalette, TRUE);
			RealizePalette(hdcSrc);
		}
		ReleaseDC(NULL, hdcSrc);
		return;
	}
	pbmi->bmiHeader.biClrUsed = iClrUsed;
	pbmi->bmiHeader.biClrImportant = iClrUsed;

	if (oldhPalette != NULL) {
		SelectPalette(hdcSrc, oldhPalette, TRUE);
		RealizePalette(hdcSrc);
	}
	i1 = StretchDIBits(destpix->hdc, destx, desty, destwidth, destheight, srcx,
			bm.bmHeight - srcy - srcheight, srcwidth, srcheight, pBits, pbmi, DIB_RGB_COLORS, SRCCOPY);

	if (i1 == (int) GDI_ERROR) {
		guiaErrorMsg("StretchDIBits() failed", GetLastError());
	}
	guiFreeMem(pBits);
	guiFreeMem((ZHANDLE) pbmi);
	ReleaseDC(NULL, hdcSrc);
}

/*
 * parse font string
 */
static INT guiaParseFont(CHAR *font, INT fontlen, LOGFONT *lf, INT reset, HDC hdc)
{
	INT i1, size;
	LONG saveHeight;

	if (reset) memset(lf, 0, sizeof(LOGFONT));
	if (!fontlen) return 0;
	if (reset) {
		lf->lfWeight = FW_NORMAL;
		lf->lfWidth = 0;
	}
	/* override these 3, even if reset = false, so font can be matched properly in guiaSetResFont() */
	lf->lfOutPrecision = OUT_DEFAULT_PRECIS;
	lf->lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lf->lfQuality = DEFAULT_QUALITY;
	saveHeight = lf->lfHeight;
	i1 = parseFontString(font, lf, hdc);	/* This is in gui.c */
	if (i1) return i1;
	if (saveHeight != lf->lfHeight) {
		int lsize = GetDeviceCaps(hdc, LOGPIXELSY);
		/* Hard coded size -- SSN */
		/* size = GetDeviceCaps(hdc, LOGPIXELSY); */
		size = 96;
		lf->lfHeight = -((lf->lfHeight * size) / 72);
	}
	return 0;
}

/*
 * return the length of text data on clipboard
 */
INT guiaGetCBTextLen(size_t *cbTextLen)
{
	HGLOBAL hgMemory;

	if (!OpenClipboard(hwndmainwin)) return RC_ERROR;

	if (!IsClipboardFormatAvailable(clipboardtextformat)) {
		CloseClipboard();
		return RC_ERROR;
	}

	hgMemory = GetClipboardData(clipboardtextformat);
	if (hgMemory == NULL) {
		CloseClipboard();
		guiaErrorMsg("GetClipboardData() failure", GetLastError());
		return RC_ERROR;
	}

	*cbTextLen = GlobalSize(hgMemory);  // This OS function always returns bytes

	CloseClipboard();
	return 0;
}

/*
 * return the text clipboard data
 */
INT guiaGetCBText(BYTE *pucText, size_t *cbText)
{
	HGLOBAL hgMemory;
	size_t iLen;
	LPSTR lpString;

	if (!OpenClipboard(hwndmainwin)) return RC_ERROR;

	if (!IsClipboardFormatAvailable(clipboardtextformat)) {
		CloseClipboard();
		return RC_ERROR;
	}
	hgMemory = GetClipboardData(clipboardtextformat);
	if (hgMemory == NULL) {
		CloseClipboard();
		guiaErrorMsg("GetClipboardData() failure", GetLastError());
		return RC_ERROR;
	}
	lpString = GlobalLock(hgMemory);

	iLen = strlen(lpString);
	if (iLen <= *cbText) {
		StringCbCopy((LPSTR)pucText, *cbText, lpString);
		//memcpy(pucText, lpString, iLen);
		*cbText = iLen;
	}
	else StringCbCopy(pucText, *cbText, lpString);//memcpy(pucText, lpString, *cbText);
	GlobalUnlock(hgMemory);
	CloseClipboard();
	return 0;
}

/*
 * store text into the clipboard
 */
INT guiaPutCBText(BYTE *pucText, size_t cbText)
{
	HGLOBAL hgMemory;
	LPSTR lpString;
	DWORD error;

	hgMemory = GlobalAlloc(GHND, cbText + sizeof(CHAR));
	if (hgMemory == NULL) return RC_ERROR;

	lpString = GlobalLock(hgMemory);
	if (lpString == NULL) return RC_ERROR;

	memcpy(lpString, pucText, cbText);
	//lpString[cbText] = '\0'; Not needed, GlobalAlloc is zeroing memory

	GlobalUnlock(hgMemory);
	/*
	 * The OpenClipboard function opens the clipboard for examination
	 * and prevents other applications from modifying the clipboard content.
	 */
	if (!OpenClipboard(hwndmainwin)) {
		guiaErrorMsg("OpenClipboard() failure", GetLastError());
		return RC_ERROR;
	}
	/*
	 * The EmptyClipboard function empties the clipboard and frees handles to data in the clipboard.
	 * It then assigns ownership of the clipboard to us.
	 */
	if (!EmptyClipboard()) {
		guiaErrorMsg("EmptyClipboard() failure", GetLastError());
		return RC_ERROR;
	}

	if (SetClipboardData(clipboardtextformat, hgMemory) == NULL) {
		error = GetLastError();
		CloseClipboard();
		guiaErrorMsg("SetClipboardData() failure", error);
		return RC_ERROR;
	}
	CloseClipboard();
	return 0;
}

/**
 * get the image resolution of the clipboard
 */
INT guiaClipboardRes(INT *resx, INT *resy)
{
	HGLOBAL hgBmi;
	BITMAPINFO *pbmi;

	*resx = 100;
	*resy = 100;

	if (!OpenClipboard(NULL)) return RC_ERROR;
	hgBmi = GetClipboardData(CF_DIB);
	if (hgBmi == NULL) { /* clipboard most likely contained text */
		CloseClipboard();
		*resx = *resy = 0;
		return 0;
	}
	pbmi = (BITMAPINFO *) GlobalLock(hgBmi);
	if ((INT) pbmi->bmiHeader.biXPelsPerMeter != 0) {
		*resx = (INT)(pbmi->bmiHeader.biXPelsPerMeter / .0254); /* convert x rex to inches */
	}
	if ((INT) pbmi->bmiHeader.biYPelsPerMeter != 0) {
		*resy = (INT)(pbmi->bmiHeader.biYPelsPerMeter / .0254); /* convert y res to inches */
	}
	GlobalUnlock(hgBmi);
	CloseClipboard();

	return 0;
}

/**
 * get the colorbits of the clipboard
 */
INT guiaClipboardBPP(INT *bpp)
{
	HGLOBAL hgBmi;
	BITMAPINFO *pbmi;

	*bpp = 0;

	if (!OpenClipboard(NULL)) return RC_ERROR;
	hgBmi = GetClipboardData(CF_DIB);
	if (hgBmi == NULL) { /* clipboard most likely contained text */
		CloseClipboard();
		return 0;
	}
	pbmi = (BITMAPINFO *) GlobalLock(hgBmi);
	*bpp = (INT) pbmi->bmiHeader.biBitCount;
	if (*bpp == 0 || *bpp > 24) *bpp = 24; /* jpeg bpp = 0 */
	GlobalUnlock(hgBmi);
	CloseClipboard();

	return 0;
}

/**
 * get the horizontal and vertical size of the clipboard
 * Return zero for success, RC_ERROR if the clipboard cannot
 * be opened.
 */
INT guiaClipboardSize(INT *hsize, INT *vsize)
{
	HGLOBAL hgBmi;
	BITMAPINFO *pbmi;

	*hsize = 0;
	*vsize = 0;

	if (!OpenClipboard(NULL)) return RC_ERROR;
	hgBmi = GetClipboardData(CF_DIB);
	if (hgBmi == NULL) { /* clipboard most likely contained text */
		CloseClipboard();
		return 0;
	}
	pbmi = (BITMAPINFO *) GlobalLock(hgBmi);
	*hsize = (INT) pbmi->bmiHeader.biWidth;
	*vsize = (INT) pbmi->bmiHeader.biHeight;
	GlobalUnlock(hgBmi);
	CloseClipboard();

	return 0;
}

/**
 * return the pixel map clipboard data
 */
INT guiaGetCBPixmap(PIXMAP *pix)
{
	INT iClrUsed, iHeaderSize, iBitCount;
	BYTE *pBits;
	BITMAPINFO *pbmi;
	BITMAPCOREHEADER *pbmc;
	HGLOBAL hgBmi;
	HDC hdc, hdcMem;
	HBITMAP hbitmap, hbmOld1, hbmOld2;
	RECT rect;
	HPALETTE oldhPalette;
	WINDOW *win;

	if (!OpenClipboard(hwndmainwin)) return RC_ERROR;
	hgBmi = GetClipboardData(CF_DIB);
	if (hgBmi != NULL) {
		hdc = GetDC(hwndmainwin);
		if (hdc == NULL) return RC_ERROR;

		pix->hdc = CreateCompatibleDC(hdc);
		if (pix->hdc == NULL) {
			ReleaseDC(hwndmainwin, hdc);
			return RC_ERROR;
		}
		ReleaseDC(hwndmainwin, hdc);

		hbmOld1 = SelectObject(pix->hdc, pix->hbitmap);
		if (pix->hpal != NULL) {
			oldhPalette = SelectPalette(pix->hdc, pix->hpal, TRUE);
			RealizePalette(pix->hdc);
		}

		pbmi = (BITMAPINFO *) GlobalLock(hgBmi);

		if (pbmi->bmiHeader.biSize == sizeof(BITMAPCOREHEADER)) {
			pbmc = (BITMAPCOREHEADER *) pbmi;
			iBitCount = pbmc->bcBitCount;
			iClrUsed = 0;

		}
		else {
			iBitCount = pbmi->bmiHeader.biBitCount;
			iClrUsed = pbmi->bmiHeader.biClrUsed;
		}
		if (!iClrUsed) {
			if (1 == iBitCount) {
				iClrUsed = 2;
			}
			else if (iBitCount <= 4) {
				iClrUsed = 16;
			}
			else if (iBitCount <= 8) {
				iClrUsed = 256;
			}
		}
		iHeaderSize = pbmi->bmiHeader.biSize;
		if (pbmi->bmiHeader.biCompression == BI_BITFIELDS && pbmi->bmiHeader.biBitCount == 16)
			iHeaderSize += 3*sizeof(DWORD);
		else
			iHeaderSize += iClrUsed*sizeof(RGBQUAD);
		pBits = (BYTE *) pbmi + iHeaderSize;

		StretchDIBits(pix->hdc, 0, 0, pbmi->bmiHeader.biWidth, pbmi->bmiHeader.biHeight,
			0, 0, pbmi->bmiHeader.biWidth, pbmi->bmiHeader.biHeight, pBits, pbmi, DIB_RGB_COLORS, SRCCOPY);
		GlobalUnlock(hgBmi);
		if (pix->hpal != NULL) {
			SelectPalette(pix->hdc, oldhPalette, TRUE);
			RealizePalette(pix->hdc);
		}
		SelectObject(pix->hdc, hbmOld1);
		DeleteDC(pix->hdc);
		pix->hdc = NULL;
		if (pix->winshow != NULL) {
			win = *pix->winshow;
			rect.top = pix->vshow - win->scrolly + win->bordertop;
			rect.left = pix->hshow;
			rect.bottom = rect.top + pix->vsize;
			rect.right = rect.left + pix->hsize;
			InvalidateRect(win->hwnd, &rect, TRUE);
		}
	}
	else {
/* try device dependant format (for Windows NT only since 95 does automatic conversion) */
		hdc = GetDC(hwndmainwin);
		if (hdc == NULL) return RC_ERROR;

		pix->hdc = CreateCompatibleDC(hdc);
		if (pix->hdc == NULL) {
			ReleaseDC(hwndmainwin, hdc);
			return RC_ERROR;
		}

		hdcMem = CreateCompatibleDC(hdc);
		ReleaseDC(hwndmainwin, hdc);

		if (hdcMem == NULL) {
			DeleteDC(pix->hdc);
			return RC_ERROR;
		}

		hbmOld1 = SelectObject(pix->hdc, pix->hbitmap);

		hbitmap = GetClipboardData(CF_BITMAP);
		if (hbitmap) {
			hbmOld2 = SelectObject(hdcMem, hbitmap);
			if (pix->hpal != NULL) {
				oldhPalette = SelectPalette(pix->hdc, pix->hpal, TRUE);
				RealizePalette(pix->hdc);
			}
			BitBlt(pix->hdc, 0, 0, pix->hsize, pix->vsize, hdcMem, 0, 0, SRCCOPY);
			if (pix->hpal != NULL) {
				SelectPalette(pix->hdc, oldhPalette, TRUE);
				RealizePalette(pix->hdc);
			}
			SelectObject(hdcMem, hbmOld2);
			if (pix->winshow != NULL) {
				win = *pix->winshow;
				rect.top = pix->vshow - win->scrolly + win->bordertop;
				rect.left = pix->hshow;
				rect.bottom = rect.top + pix->vsize;
				rect.right = rect.left + pix->hsize;
				InvalidateRect(win->hwnd, &rect, TRUE);
			}
		}
		else {
			SelectObject(pix->hdc, hbmOld1);
			DeleteDC(hdcMem);
			DeleteDC(pix->hdc);
			pix->hdc = NULL;
			return RC_ERROR;
		}
		SelectObject(pix->hdc, hbmOld1);
		DeleteDC(hdcMem);
		DeleteDC(pix->hdc);
		pix->hdc = NULL;
	}
	CloseClipboard();
	return 0;
}

/**
 * store the pixel map into the clipboard
 */
INT guiaPutCBPixmap(PIXMAP *pix)
{
	HDC hdc;
	BITMAP bm;
	HPALETTE oldhPalette;
	INT iBitCount, iClrUsed, iBitmapRes, iImageSize, iHeaderSize;
	BYTE *pBits;
	BITMAPINFO *pbmi;
	HGLOBAL hgBmi;

	hdc = GetDC(hwndmainwin);
	if (hdc == NULL) return RC_ERROR;

	if (pix->hpal != NULL) {
		oldhPalette = SelectPalette(hdc, pix->hpal, TRUE);
		RealizePalette(hdc);
	}

	GetObject(pix->hbitmap, sizeof(BITMAP), (LPVOID) &bm);

	iBitmapRes = bm.bmPlanes*bm.bmBitsPixel;
	if (1 == iBitmapRes) {
		iClrUsed = 2;
		iBitCount = 1;
	}
	else if (iBitmapRes <= 4) {
		iClrUsed = 16;
		iBitCount = 4;
	}
	else if (iBitmapRes <= 8) {
		iClrUsed = 256;
		iBitCount = 8;
	}
	else {
		iClrUsed = 0;
		iBitCount = 24;
	}

	iHeaderSize = sizeof(BITMAPINFOHEADER) + iClrUsed*sizeof(RGBQUAD);
	iImageSize = ((((bm.bmWidth*iBitCount) + 31) & ~31) >> 3) * bm.bmHeight;
	hgBmi = GlobalAlloc(GMEM_DDESHARE, iHeaderSize + iImageSize);
	if (hgBmi == NULL) {
		guiaErrorMsg("Out of memory", 0);
		if (oldhPalette != NULL) {
			SelectPalette(hdc, oldhPalette, TRUE);
			RealizePalette(hdc);
		}
		ReleaseDC(hwndmainwin, hdc);
		return RC_ERROR;
	}

	pbmi = (BITMAPINFO *) GlobalLock(hgBmi);
	pBits = (BYTE *) pbmi + iHeaderSize;

/* Initialize BITMAPINFOHEADER in BITMAPINFO structure */
	pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pbmi->bmiHeader.biWidth = bm.bmWidth;
	pbmi->bmiHeader.biHeight = bm.bmHeight;
	pbmi->bmiHeader.biPlanes = 1;
	pbmi->bmiHeader.biBitCount = iBitCount;
	pbmi->bmiHeader.biCompression = BI_RGB;
	pbmi->bmiHeader.biSizeImage = 0;
	pbmi->bmiHeader.biXPelsPerMeter = 0;
	pbmi->bmiHeader.biYPelsPerMeter = 0;
	pbmi->bmiHeader.biClrUsed = 0;
	pbmi->bmiHeader.biClrImportant = 0;
	pbmi->bmiHeader.biSizeImage = ((((bm.bmWidth*iBitCount) + 31) & ~31) >> 3) * bm.bmHeight;

/* do NOT select pix->hbitmap into a DC before this call */
	if (!GetDIBits(hdc, pix->hbitmap, 0, bm.bmHeight, pBits, pbmi, DIB_RGB_COLORS)) {
		guiaErrorMsg("GetDIBits() failed", 0);
		GlobalUnlock(hgBmi);
		GlobalFree(hgBmi);
		if (oldhPalette != NULL) {
			SelectPalette(hdc, oldhPalette, TRUE);
			RealizePalette(hdc);
		}
		ReleaseDC(hwndmainwin, hdc);
		return RC_ERROR;
	}

	GlobalUnlock(hgBmi);
//	OpenClipboard(hwndmainwin);
	if (!OpenClipboard(hwndmainwin)) {
		guiaErrorMsg("OpenClipboard() failure", GetLastError());
		return RC_ERROR;
	}
	EmptyClipboard();
	if (SetClipboardData(CF_DIB, hgBmi) == NULL) {
		guiaErrorMsg("SetClipboardData() failure", GetLastError());
	}
	CloseClipboard();

	if (oldhPalette != NULL) {
		SelectPalette(hdc, oldhPalette, TRUE);
		RealizePalette(hdc);
	}
	ReleaseDC(hwndmainwin, hdc);
	return 0;
}

/**
 * access to the instance handle
 */
HINSTANCE guia_hinst()
{
	return(hinstancemain);
}

/**
 * access to the main window handle
 */
HWND guia_hwnd()
{
	return(hwndmainwin);
}

/**
 * show a caret for a given window
 */
INT guiaShowCaret(WINDOW *win)
{
	return((INT)SendMessage(win->hwnd, GUIAM_WINSHOWCARET, (WPARAM) 0, (LPARAM) win));
}

/**
 * hide a caret for a given window
 */
INT guiaHideCaret(WINDOW *win)
{
	return((INT)SendMessage(win->hwnd, GUIAM_WINHIDECARET, (WPARAM) 0, (LPARAM) win));
}

/**
 * set the position of the caret in a given window
 */
INT guiaSetCaretPos(WINDOW *win, int x, int y)
{
	win->caretpos.x = x;
	win->caretpos.y = y;
	if (!win->caretcreatedflg) return 0;
	return((INT)SendMessage(win->hwnd, GUIAM_WINMOVECARET, (WPARAM) 0, (LPARAM) win));
}

/**
 * set the vertical height of the caret in the given window
 */
INT guiaSetCaretVSize(WINDOW *win, int vsize)
{
	win->caretvsize = vsize;
	if (!win->caretcreatedflg) return 0;
	SendMessage(win->hwnd, GUIAM_WINHIDECARET, (WPARAM) 0, (LPARAM) win);
	return((INT)SendMessage(win->hwnd, GUIAM_WINSHOWCARET, (WPARAM) 0, (LPARAM) win));
}

/**
 * create an icon resource
 */
INT guiaCreateIcon(RESOURCE *res)
{
	HDC hDCMain, hdc;
	INT iImageSize, iCurScan, iCurPos, iDataSize, destpos, iDestOffset, vsize, iScanWidth;
	INT iHByteSize, color, r, g, b;
	BYTE *pImageStorage, *pData;
	PBITMAPINFO pDibInfo;
	BOOL transparencyUsed = FALSE;

	if (res == NULL || res->restype != RES_TYPE_ICON) {
		guiaErrorMsg("Invalid icon resource passed to guiaCreateIcon", GetLastError());
		return(RC_INVALID_VALUE);
	}

	hDCMain = GetDC(hwndmainwin);
	hdc = CreateCompatibleDC(hDCMain);

	if (res->content == NULL) {
		res->hbmicon = CreateBitmap(res->hsize, res->vsize, 1, res->bpp, NULL);
	}
	else {

		/*
		 * The r, g, b values are used for the 'Transparent' pixels
		 */
		color = GetSysColor(COLOR_BTNFACE);
		r = color >> 16;
		g = (color & 0x00FF00) >> 8;
		b = color & 0x0000FF;
//		if (osinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
//			/* CODE - this is a hack for windows 98 */
//			r--;
//			g--;
//			b--;
//		}

		/*
		 * The below is used only on WinXP because of a glitch
		 * in it's rendering of an icon on a menu item.
		 * We use TransparentBlt instead of BitBlt.
		 * see WinProc_OnDrawItem in guiawin3.c
		 */
		res->transColor = (r<<16) + (g<<8) + b;

		iScanWidth = ((((res->hsize * 24) + 31) & ~31) >> 3);
		iImageSize = iScanWidth * res->vsize;
		pImageStorage = guiAllocMem(iImageSize);
		if (pImageStorage == NULL) {
			guiaErrorMsg("Out of guiAllocMem memory in guiaCreateIcon", iImageSize);
			return RC_NO_MEM;
		}
		pData = res->content;
		iDataSize = res->entrycount;
		iHByteSize = res->hsize * 6; // Only used if colorbits=24
		if (res->bpp == 1 || res->bpp == 4) {
			if (res->vsize * res->hsize > iDataSize) {
				vsize = iDataSize / res->hsize;
			}
			else vsize = res->vsize;
		}
		else if (res->vsize * iHByteSize > iDataSize) {
			vsize = iDataSize / iHByteSize;
		}
		else vsize = res->vsize;
		pDibInfo = (PBITMAPINFO) guiAllocMem(sizeof(BITMAPINFOHEADER));
		if (pDibInfo == NULL) {
			res->hbmicon = NULL;
			ReleaseDC(hwndmainwin, hDCMain);
			DeleteDC(hdc);
			guiaErrorMsg("Out of guiAllocMem memory in guiaCreateIcon", sizeof(BITMAPINFOHEADER));
			return RC_NO_MEM;
		}
		pDibInfo->bmiHeader.biSize = (LONG) sizeof(BITMAPINFOHEADER);
		pDibInfo->bmiHeader.biWidth = res->hsize;
		pDibInfo->bmiHeader.biHeight = res->vsize;
		pDibInfo->bmiHeader.biPlanes = 1;
		pDibInfo->bmiHeader.biBitCount = 24;
		pDibInfo->bmiHeader.biCompression = BI_RGB;
		pDibInfo->bmiHeader.biSizeImage =  iImageSize;
		pDibInfo->bmiHeader.biXPelsPerMeter = 1000;
		pDibInfo->bmiHeader.biYPelsPerMeter = 1000;
		pDibInfo->bmiHeader.biClrUsed = 0;
		pDibInfo->bmiHeader.biClrImportant = 0;
		destpos = iImageSize - iScanWidth;

		switch (res->bpp) {
		case 1:
			for (iCurScan = 0; iCurScan < vsize; iCurScan++) {
				iCurPos = iDestOffset = 0;
				while (iCurPos < res->hsize) {
					if (pData[iCurPos] == 'T') {
						pImageStorage[destpos + iDestOffset] = r;
						pImageStorage[destpos + iDestOffset + 1] = g;
						pImageStorage[destpos + iDestOffset + 2] = b;
						transparencyUsed = TRUE;
					}
					else if (pData[iCurPos] == '0') {
						pImageStorage[destpos + iDestOffset] = 0x00;
						pImageStorage[destpos + iDestOffset + 1] = 0x00;
						pImageStorage[destpos + iDestOffset + 2] = 0x00;
					}
					else {
						pImageStorage[destpos + iDestOffset] = 0xFF;
						pImageStorage[destpos + iDestOffset + 1] = 0xFF;
						pImageStorage[destpos + iDestOffset + 2] = 0xFF;
					}
					++iCurPos;
					iDestOffset += 3;
				}
				destpos -= iScanWidth;
				pData += res->hsize;
			}
			break;
		case 4:
			for (iCurScan = 0; iCurScan < vsize; iCurScan++) {
				iCurPos = iDestOffset = 0;
				for (; iCurPos < res->hsize; iCurPos++) {
					pImageStorage[destpos + iDestOffset] = 0x00;
					pImageStorage[destpos + iDestOffset + 1] = 0x00;
					pImageStorage[destpos + iDestOffset + 2] = 0x00;
					switch (pData[iCurPos]) {
						case '0':
							break;
						case '1':
							pImageStorage[destpos + iDestOffset] = 0x80;
							break;
						case '2':
							pImageStorage[destpos + iDestOffset + 1] = 0x80;
							break;
						case '3':
							pImageStorage[destpos + iDestOffset] = 0x80;
							pImageStorage[destpos + iDestOffset + 1] = 0x80;
							break;
						case '4':
							pImageStorage[destpos + iDestOffset + 2] = 0x80;
							break;
						case '5':
							pImageStorage[destpos + iDestOffset] = 0x80;
							pImageStorage[destpos + iDestOffset + 2] = 0x80;
							break;
						case '6':
							pImageStorage[destpos + iDestOffset + 1] = 0x80;
							pImageStorage[destpos + iDestOffset + 2] = 0x80;
							break;
						case '7':
							pImageStorage[destpos + iDestOffset] = 0xC0;
							pImageStorage[destpos + iDestOffset + 1] = 0xC0;
							pImageStorage[destpos + iDestOffset + 2] = 0xC0;
							break;
						case '8':
							pImageStorage[destpos + iDestOffset] = 0x80;
							pImageStorage[destpos + iDestOffset + 1] = 0x80;
							pImageStorage[destpos + iDestOffset + 2] = 0x80;
							break;
						case '9':
							pImageStorage[destpos + iDestOffset] = 0xFF;
							break;
						case 'A':
							pImageStorage[destpos + iDestOffset + 1] = 0xFF;
							break;
						case 'B':
							pImageStorage[destpos + iDestOffset] = 0xFF;
							pImageStorage[destpos + iDestOffset + 1] = 0xFF;
							break;
						case 'C':
							pImageStorage[destpos + iDestOffset + 2] = 0xFF;
							break;
						case 'D':
							pImageStorage[destpos + iDestOffset] = 0xFF;
							pImageStorage[destpos + iDestOffset + 2] = 0xFF;
							break;
						case 'E':
							pImageStorage[destpos + iDestOffset + 1] = 0xFF;
							pImageStorage[destpos + iDestOffset + 2] = 0xFF;
							break;
						case 'F':
							pImageStorage[destpos + iDestOffset] = 0xFF;
							pImageStorage[destpos + iDestOffset + 1] = 0xFF;
							pImageStorage[destpos + iDestOffset + 2] = 0xFF;
							break;
						case 'T':
							pImageStorage[destpos + iDestOffset] = r;
							pImageStorage[destpos + iDestOffset + 1] = g;
							pImageStorage[destpos + iDestOffset + 2] = b;
							transparencyUsed = TRUE;
							break;
					}
					iDestOffset += 3;
				}
				destpos -= iScanWidth;
				pData += res->hsize;
			}
			break;
		case 24:
			for (iCurScan = 0;  iCurScan < vsize; iCurScan++) {
				iCurPos = iDestOffset = 0;
				while (iCurPos < iHByteSize - 5) {
					if (pData[iCurPos] == 'T') {
						pImageStorage[destpos + iDestOffset] = r;
						pImageStorage[destpos + iDestOffset + 1] = g;
						pImageStorage[destpos + iDestOffset + 2] = b;
						transparencyUsed = TRUE;
					}
					else {
						pImageStorage[destpos + iDestOffset] = translatehexdigit[pData[iCurPos]] << 4;
						pImageStorage[destpos + iDestOffset] += translatehexdigit[pData[iCurPos + 1]]; 	/* blue value */
						pImageStorage[destpos + iDestOffset + 1] = translatehexdigit[pData[iCurPos + 2]] << 4;
						pImageStorage[destpos + iDestOffset + 1] += translatehexdigit[pData[iCurPos + 3]]; 	/* green value */
						pImageStorage[destpos + iDestOffset + 2] = translatehexdigit[pData[iCurPos + 4]] << 4;
						pImageStorage[destpos + iDestOffset + 2] += translatehexdigit[pData[iCurPos + 5]]; 	/* red value */
					}
					iDestOffset += 3;
					iCurPos += 6;
				}
				destpos -= iScanWidth;
				pData += iHByteSize;
			}
			break;
		default:
			res->hbmicon = NULL;
			ReleaseDC(hwndmainwin, hDCMain);
			DeleteDC(hdc);
			guiaErrorMsg("Invalid bpp value in icon resource passed to guiaCreateIcon", GetLastError());
			return RC_ERROR;
		}
		res->hbmicon = CreateDIBitmap(hDCMain, &pDibInfo->bmiHeader, CBM_INIT, pImageStorage, pDibInfo, DIB_RGB_COLORS);
		res->hbmgray = NULL;
		res->bmiColor = pDibInfo;
		res->bmiGray = NULL;
		res->bmiData = pImageStorage;
		if (IsWindows7OrGreater() && transparencyUsed && useTransparentBlt) res->useTransBlt4Icon = TRUE;
	}
	ReleaseDC(hwndmainwin, hDCMain);
	DeleteDC(hdc);
	if (res->hbmicon == NULL) return RC_ERROR;
	return 0;
}


static INT guiaGrayIcon(RESOURCE *res, HDC hDCIcon)
{
	PBITMAPINFO bmiUsed, pDibInfo;
	HBITMAP bmOld;
	INT i, j, iScanWidth, iDataSize, bpp, grayScale, color;
	COLORREF pixelVal, pixelGray;

	if (res == NULL || res->restype != RES_TYPE_ICON) {
		guiaErrorMsg("Invalid icon resource passed to guiaGrayIcon", GetLastError());
		return(RC_INVALID_VALUE);
	}
	bmiUsed = res->bmiColor;
	bpp = ((*bmiUsed).bmiHeader).biBitCount;
	if (bpp == 24) {
		if (res->hbmgray != NULL) return (RC_ERROR);
		else {
			res->hbmgray = CreateDIBitmap(hDCIcon, &bmiUsed->bmiHeader, CBM_INIT, res->bmiData, bmiUsed, DIB_RGB_COLORS);
			guiFreeMem(res->bmiData);
			res->bmiData = NULL;
			bmOld = SelectObject(hDCIcon, res->hbmgray);
			for (i = 0; i < res->vsize; i++) {
				for (j = 0; j < res->hsize; j++) {
					pixelVal = GetPixel(hDCIcon, j, i);
					pixelGray = ((pixelVal & 0x0000ff) + ((pixelVal & 0x00ff00) >> 8) + ((pixelVal & 0xff0000) >> 16)) / 3;
					pixelVal = (pixelGray << 16) | (pixelGray << 8) | (pixelGray);
					SetPixel(hDCIcon, j, i, pixelVal);
				}
			}
			SelectObject(hDCIcon, bmOld);
		}
	}
	else if ((bpp == 1) || (bpp == 4)) {
		if (res->bmiGray != NULL) return (RC_ERROR);
		iScanWidth = ((((res->hsize * res->bpp) + 31) & ~31) >> 3);
		iDataSize = res->entrycount;
		if (bpp == 1) pDibInfo = (PBITMAPINFO) guiAllocMem(sizeof(BITMAPINFOHEADER) + 2*sizeof(RGBQUAD));
		else pDibInfo = (PBITMAPINFO) guiAllocMem(sizeof(BITMAPINFOHEADER) + 16*sizeof(RGBQUAD));
		if (pDibInfo == NULL) {
			guiaErrorMsg("Out of memory in guiaGrayIcon", 0);
			return RC_NO_MEM;
		}
		pDibInfo->bmiHeader.biSize = (LONG) sizeof(BITMAPINFOHEADER);
		pDibInfo->bmiHeader.biWidth = res->hsize;
		pDibInfo->bmiHeader.biHeight = res->vsize;
		pDibInfo->bmiHeader.biPlanes = 1;
		pDibInfo->bmiHeader.biCompression = BI_RGB;
		pDibInfo->bmiHeader.biSizeImage =  iScanWidth*res->vsize;
		pDibInfo->bmiHeader.biXPelsPerMeter = 1000;
		pDibInfo->bmiHeader.biYPelsPerMeter = 1000;
		pDibInfo->bmiHeader.biClrImportant = 0;
		if (bpp == 1) {
			pDibInfo->bmiHeader.biBitCount = 1;
			pDibInfo->bmiHeader.biClrUsed = 2;
			pDibInfo->bmiHeader.biClrImportant = 0;
			pDibInfo->bmiColors[0].rgbRed = 128;
			pDibInfo->bmiColors[0].rgbGreen = 128;
			pDibInfo->bmiColors[0].rgbBlue = 128;
			pDibInfo->bmiColors[1].rgbRed = 128;
			pDibInfo->bmiColors[1].rgbGreen = 128;
			pDibInfo->bmiColors[1].rgbBlue = 128;
		}
		else {
			pDibInfo->bmiHeader.biBitCount = 4;
			pDibInfo->bmiHeader.biClrUsed = 16;
			grayScale = 15;
			for (color = 0; color < 16; color++) {
				pDibInfo->bmiColors[color].rgbRed = grayScale;
				pDibInfo->bmiColors[color].rgbGreen = grayScale;
				pDibInfo->bmiColors[color].rgbBlue = grayScale;
				grayScale += 16;
			}
		}
		res->bmiGray = pDibInfo;
	}
	return (0);
}


/**
 * free an icon resource
 */
INT guiaFreeIcon(RESOURCE *res)
{
	if (res->hbmicon != NULL) {
		DeleteObject(res->hbmicon);
		res->hbmicon = NULL;
	}
	if (res->hbmgray != NULL) {
		DeleteObject(res->hbmgray);
		res->hbmgray = NULL;
	}
	if (res->bmiColor != NULL) {
		guiFreeMem((ZHANDLE) res->bmiColor);
		res->bmiColor = NULL;
	}
	if (res->bmiGray != NULL) {
		guiFreeMem((ZHANDLE) res->bmiGray);
		res->bmiGray = NULL;
	}
	if (res->bmiData != NULL) {
		guiFreeMem(res->bmiData);
		res->bmiData = NULL;
	}
	return 0;
}

/**
 * set the cursor shape
 */
INT guiaSetCursorShape(WINDOW *win, INT cursortype)
{
	POINT cpos;
	BOOL worked;

	switch (cursortype) {
	case CURSORTYPE_ARROW:
		win->hcursor = LoadCursor(NULL, IDC_ARROW);
		break;
	case CURSORTYPE_WAIT:
		win->hcursor = LoadCursor(NULL, IDC_WAIT);
		break;
	case CURSORTYPE_CROSS:
		win->hcursor = LoadCursor(NULL, IDC_CROSS);
		break;
	case CURSORTYPE_IBEAM:
		win->hcursor = LoadCursor(NULL, IDC_IBEAM);
		break;
	case CURSORTYPE_SIZENS:
		win->hcursor = LoadCursor(NULL, IDC_SIZENS);
		break;
	case CURSORTYPE_SIZEWE:
		win->hcursor = LoadCursor(NULL, IDC_SIZEWE);
		break;
	case CURSORTYPE_APPSTARTING:
		win->hcursor = LoadCursor(NULL, IDC_APPSTARTING);
		break;
	case CURSORTYPE_HELP:
		win->hcursor = LoadCursor(NULL, IDC_HELP);
		break;
	case CURSORTYPE_NO:
		win->hcursor = LoadCursor(NULL, IDC_NO);
		break;
	case CURSORTYPE_SIZEALL:
		win->hcursor = LoadCursor(NULL, IDC_SIZEALL);
		break;
	case CURSORTYPE_SIZENESW:
		win->hcursor = LoadCursor(NULL, IDC_SIZENESW);
		break;
	case CURSORTYPE_SIZENWSE:
		win->hcursor = LoadCursor(NULL, IDC_SIZENWSE);
		break;
	case CURSORTYPE_UPARROW:
		win->hcursor = LoadCursor(NULL, IDC_UPARROW);
		break;
	case CURSORTYPE_HAND:
		win->hcursor = LoadCursor(NULL, IDC_HAND);
		break;
	default:
		return RC_ERROR;
	}
	SetCursor(win->hcursor);
	worked = GetCursorPos(&cpos);
	worked = SetCursorPos(cpos.x, cpos.y);
	return 0;
}

void guiaErrorMsg(CHAR *text, DWORD num)
{
	CHAR worktext[90], worknum[6];

	strcpy(worktext, text);
	itonum5(num, (BYTE *) worknum);
	worknum[5] = '\0';
	strcat(worktext, worknum);
	MessageBeep(MB_ICONSTOP);
	if (MessageBox(NULL, worktext, "DB/C INTERNAL ERROR",
		MB_OK | MB_ICONSTOP | MB_APPLMODAL | MB_SETFOREGROUND) == 0) {
		CHAR bwork[1024];
		sprintf_s(bwork, 1024, "Call to MessageBox Failed, message was '%s'   ", worktext);
		svclogerror(bwork, GetLastError());
	}
}

INT makecontrol(CONTROL *control, INT xoffset, INT yoffset, HWND hwndshow, INT dialogflag)
{
	INT i1, x1, y1, height, width, rtextwidth;
	DWORD style, exstyle, error;
	LPSTR classptr, wintitle;
	CHAR *cptr1;
	RECT rect;
	HDC fhdc, hdc;
	SIZE size;
	TCITEM tcitem;
	HFONT oldhfont;
	TEXTMETRIC tm;
	HIMAGELIST hImageList;
	HBITMAP membmp;
	TREEICONSTRUCT *ptreeicon;
	static WNDPROC oldtcproc;
	HICON hicon;
	LPSTR deferTextForMEDIT;

	rect = control->rect;
	x1 = rect.left + control->tabgroupxoffset;
	y1 = rect.top + control->tabgroupyoffset;
	width = rect.right - rect.left + 1;
	rtextwidth = 0;
	height = rect.bottom - rect.top + 1;
	/*
	 * Everything created here is a child window
	 */
	style = WS_CHILD;
	if (ISGRAY(control)) style |= WS_DISABLED;
	exstyle = WS_EX_NOPARENTNOTIFY;
	control->changeflag = TRUE;
	control->shownflag = FALSE;
	wintitle = deferTextForMEDIT = NULL;

	switch (control->type) {
	case PANEL_BOX:
		classptr = WC_STATIC;
		style |= SS_BLACKFRAME;
		break;
	case PANEL_BOXTITLE:
		if (control->textval != NULL) wintitle = (LPSTR) guiLockMem(control->textval);
		classptr = WC_BUTTON;
		style |= BS_GROUPBOX;
		break;
	case PANEL_TEXT:
	case PANEL_VTEXT:
	case PANEL_RVTEXT:
	case PANEL_RTEXT:
		if (control->textval != NULL) wintitle = (LPSTR) guiLockMem(control->textval);
		style &= ~WS_VISIBLE;
		classptr = WC_STATIC;
		style |= SS_LEFT;
		break;
	case PANEL_CVTEXT:
	case PANEL_CTEXT:
		if (control->textval != NULL) wintitle = (LPSTR) guiLockMem(control->textval);
		classptr = WC_STATIC;
		style |= SS_CENTER;
		break;
	case PANEL_PLEDIT:
	case PANEL_PEDIT:
//		if (control->textval != NULL) wintitle = (LPSTR) guiLockMem(control->textval);
//		classptr = WC_EDIT;
//		if (useEditBoxClientEdge) exstyle |= WS_EX_CLIENTEDGE;
//		if (control->style & CTRLSTYLE_RIGHTJUSTIFIED) style |= ES_RIGHT;
//		else if (control->style & CTRLSTYLE_CENTERJUSTIFIED) style |= ES_CENTER;
//		else style |= ES_LEFT;
//		style |= ES_AUTOHSCROLL | WS_BORDER | ES_PASSWORD;
//		if (dialogflag) style |= WS_TABSTOP | WS_GROUP;
//		if (control->style & (CTRLSTYLE_SHOWONLY | CTRLSTYLE_READONLY)) style |= ES_READONLY;
//		break;
	case PANEL_LEDIT:
	case PANEL_FEDIT:
	case PANEL_EDIT:
		if (control->textval != NULL) wintitle = (LPSTR) guiLockMem(control->textval);
		classptr = WC_EDIT;

		/**
		 * Change for 16.1.5+ on Feb 24 2014
		 * When client edge is requested, do NOT use WS_BORDER.
		 * The two together were producing a kind of annoying double edge effect.
		 * This combo, when client edge is on, gives a nice compromise
		 * between the 3-D XP border and the flat Win7 border.
		 */
		if (useEditBoxClientEdge) exstyle |= WS_EX_CLIENTEDGE;
		else style |= WS_BORDER;

		if (control->style & CTRLSTYLE_RIGHTJUSTIFIED) style |= ES_RIGHT;
		else if (control->style & CTRLSTYLE_CENTERJUSTIFIED) style |= ES_CENTER;
		else style |= ES_LEFT;
		style |= ES_AUTOHSCROLL; /*| WS_BORDER;  | ES_MULTILINE; <-- Should not be here, is messing things up! */
		if (dialogflag) style |= WS_TABSTOP | WS_GROUP;
		if (control->style & (CTRLSTYLE_SHOWONLY | CTRLSTYLE_READONLY)) style |= ES_READONLY;
		if (control->type == PANEL_PEDIT || control->type == PANEL_PLEDIT) style |= ES_PASSWORD;
		break;
	case PANEL_LTCHECKBOX:
	case PANEL_CHECKBOX:
		if (control->textval != NULL) wintitle = (LPSTR) guiLockMem(control->textval);
		classptr = WC_BUTTON;
		style &= ~WS_VISIBLE;
		style |= BS_OWNERDRAW;
		if (dialogflag) style |= WS_TABSTOP | WS_GROUP;
		break;
	case PANEL_BUTTON:
		if (control->textval != NULL) wintitle = (LPSTR) guiLockMem(control->textval);
		classptr = WC_BUTTON;
		style &= ~WS_VISIBLE;
		style |= BS_AUTORADIOBUTTON | WS_TABSTOP | WS_GROUP;
		break;
	case PANEL_LASTRADIOBUTTON:
		if (control->textval != NULL) wintitle = (LPSTR) guiLockMem(control->textval);
		classptr = WC_BUTTON;
		style &= ~WS_VISIBLE;
		style |= BS_AUTORADIOBUTTON | WS_TABSTOP | WS_GROUP;
		break;
	case PANEL_RADIOBUTTON:
		if (control->textval != NULL) wintitle = (LPSTR) guiLockMem(control->textval);
		classptr = WC_BUTTON;
		style &= ~WS_VISIBLE;
		style |= BS_AUTORADIOBUTTON | WS_TABSTOP;
		break;
	case PANEL_TREE:
		classptr = WC_TREEVIEW;
		exstyle |= WS_EX_CLIENTEDGE;
		style |= WS_TABSTOP | WS_GROUP | TVS_HASBUTTONS | TVS_HASLINES
				| TVS_LINESATROOT | TVS_DISABLEDRAGDROP | TVS_SHOWSELALWAYS;
		break;
	case PANEL_POPBOX:
		if (control->textval != NULL) wintitle = (LPSTR) guiLockMem(control->textval);
		classptr = szPopboxClassName;
		exstyle |= WS_EX_CLIENTEDGE;
		height = CW_USEDEFAULT;
		if (dialogflag) style |= WS_TABSTOP | WS_GROUP;
		break;
	case PANEL_TABLE:
		if (!tableclassregistered) {
			guiaRegisterTable(hinstancemain);
			tableclassregistered = TRUE;
		}
		classptr = szTableClassName;
		style |= WS_HSCROLL | WS_VSCROLL;
		if (dialogflag) style |= WS_TABSTOP | WS_GROUP;
		exstyle |= WS_EX_CLIENTEDGE;
		break;
	case PANEL_LISTBOXHS:
	case PANEL_MLISTBOXHS:
		style |= WS_HSCROLL; /* DX12 */
		// @suppress("No break at end of case")
	case PANEL_MLISTBOX:
		if (control->type != PANEL_LISTBOXHS) style |= LBS_EXTENDEDSEL;
		// @suppress("No break at end of case")
	case PANEL_LISTBOX:
		classptr = WC_LISTBOX;

		/**
		 * Change for 16.1.5+ on May 19 2014
		 * When client edge is requested, do NOT use WS_BORDER.
		 * The two together were producing a kind of annoying double edge effect.
		 * This combo, when client edge is on, gives a nice compromise
		 * between the 3-D XP border and the flat Win7 border.
		 */
		if (useListBoxClientEdge) exstyle |= WS_EX_CLIENTEDGE;
		else style |= WS_BORDER;

		//exstyle |= WS_EX_CLIENTEDGE;
		style |= LBS_NOTIFY /*| WS_BORDER*/ | WS_VSCROLL;
		style |= LBS_OWNERDRAWFIXED | LBS_HASSTRINGS; /* DX12 */
		if (control->listboxtabs != NULL) style |= LBS_USETABSTOPS;
		if (dialogflag) style |= WS_TABSTOP | WS_GROUP;
		if (!(control->style & CTRLSTYLE_INSERTORDER)) style |= LBS_SORT;
		break;
	case PANEL_DROPBOX:
		classptr = WC_COMBOBOX;
		exstyle |= WS_EX_CLIENTEDGE;
		style |= WS_VSCROLL | CBS_DROPDOWNLIST;
		style |= CBS_OWNERDRAWFIXED | CBS_HASSTRINGS; /* DX12 */
		if (dialogflag) style |= WS_TABSTOP | WS_GROUP;
		if (control->listboxtabs != NULL) style |= LBS_USETABSTOPS; /* DX12 */
		if (!(control->style & CTRLSTYLE_INSERTORDER)) style |= CBS_SORT;
		/**
		 * Added on 07/16/12 jpr in 16.1.1+
		 *
		 * The new ComCtl32.dll opens the drop list differently than before.
		 * Windows uses the programmer specified height as a hint and does what it feels like.
		 * Adding this style bit seems to put things back. It limits the drop box
		 * height to the requested value.
		 */
		style |= CBS_NOINTEGRALHEIGHT;
		break;
	case PANEL_HSCROLLBAR:
		classptr = WC_SCROLLBAR;
		style |= SBS_HORZ;
		if (dialogflag) style |= WS_TABSTOP | WS_GROUP;
		break;
	case PANEL_VSCROLLBAR:
		classptr = WC_SCROLLBAR;
		style |= SBS_VERT;
		if (dialogflag) style |= WS_TABSTOP | WS_GROUP;
		break;
	case PANEL_PUSHBUTTON:
		if (control->textval != NULL) wintitle = (LPSTR) guiLockMem(control->textval);
		classptr = WC_BUTTON;
		style |= BS_PUSHBUTTON;
		if (dialogflag) style |= WS_TABSTOP | WS_GROUP;
		break;
	case PANEL_DEFPUSHBUTTON:
		if (control->textval != NULL) wintitle = (LPSTR) guiLockMem(control->textval);
		classptr = WC_BUTTON;
		if (control->res != NULL && !(*control->res)->ctldefault) {
			(*control->res)->ctldefault = control;
			style |= BS_DEFPUSHBUTTON;
		}
		else style |= BS_PUSHBUTTON;
		if (dialogflag) style |= WS_TABSTOP | WS_GROUP;
		break;
	case PANEL_LOCKBUTTON:
	case PANEL_ICONPUSHBUTTON:
	case PANEL_ICONLOCKBUTTON:
		if (control->textval != NULL) wintitle = (LPSTR) guiLockMem(control->textval);
		classptr = WC_BUTTON;
		style |= BS_OWNERDRAW;
		break;
	case PANEL_VICON:
		classptr = WC_STATIC;
		style |= SS_OWNERDRAW;
		break;
	case PANEL_ICON:
		classptr = WC_STATIC;
		style |= SS_OWNERDRAW;
		control->useritem = nexticonid++;
		break;
	case PANEL_ICONDEFPUSHBUTTON:
		if (control->textval != NULL) wintitle = (LPSTR) guiLockMem(control->textval);
		classptr = WC_BUTTON;
		if (control->res != NULL && !(*control->res)->ctldefault) {
			(*control->res)->ctldefault = control;
			style |= BS_DEFPUSHBUTTON | BS_OWNERDRAW;
		}
		else style |= BS_PUSHBUTTON | BS_OWNERDRAW;
		if (dialogflag) style |= WS_TABSTOP | WS_GROUP;
		break;
	case PANEL_MLEDIT:
	case PANEL_MLEDITHS:
	case PANEL_MLEDITVS:
	case PANEL_MLEDITS:
	case PANEL_MEDIT:
	case PANEL_MEDITHS:
	case PANEL_MEDITVS:
	case PANEL_MEDITS:
		if (control->textval != NULL) {
			if (!IsWindows7OrGreater()) wintitle = (LPSTR) guiLockMem(control->textval);
			else deferTextForMEDIT = (LPSTR) guiLockMem(control->textval);
		}
		classptr = WC_EDIT;
		/**
		 * Change for 16.1.5+ on May 19 2014
		 * When client edge is requested, do NOT use WS_BORDER.
		 * The two together were producing a kind of annoying double edge effect.
		 * This combo, when client edge is on, gives a nice compromise
		 * between the 3-D XP border and the flat Win7 border.
		 */
		if (useEditBoxClientEdge) exstyle |= WS_EX_CLIENTEDGE;
		else style |= WS_BORDER;
		style |= ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN;
		if (dialogflag) style |= WS_TABSTOP | WS_GROUP;
		if (control->style & (CTRLSTYLE_SHOWONLY | CTRLSTYLE_READONLY)) style |= ES_READONLY;
		if (control->type == PANEL_MLEDITHS || control->type == PANEL_MEDITHS
				|| control->type == PANEL_MEDITS || control->type == PANEL_MLEDITS) {
			style |= WS_HSCROLL;
		}
		if (control->type == PANEL_MLEDITVS || control->type == PANEL_MEDITVS
				|| control->type == PANEL_MEDITS || control->type == PANEL_MLEDITS) {
			style |= WS_VSCROLL;
		}
		break;
	case PANEL_PROGRESSBAR:
		classptr = PROGRESS_CLASS;
		style |= WS_BORDER;
		break;
	case PANEL_TAB:
		if (control->tabsubgroup > 1) {
			tcitem.mask = TCIF_TEXT | TCIF_PARAM;
			tcitem.pszText = (LPSTR) control->textval;
			tcitem.lParam = (LPARAM) control;
			TabCtrl_InsertItem(control->tabgroupctlhandle, control->tabsubgroup - 1, &tcitem);
			control->changeflag = FALSE;
			control->ctlhandle  = control->tabgroupctlhandle;	/* jpr 10/24/97 */
			control->oldproc = oldtcproc;		/* jpr 10/24/97 */
			//return here because at the API level, the tab group is all one window and it has
			// been created. We just added a 'tab' to it.
			return 0;
		}
		exstyle |= WS_EX_WINDOWEDGE;
		classptr = WC_TABCONTROL;
		style |= WS_CLIPSIBLINGS;
		break;
	}
	control->ctlhandle = CreateWindowEx(exstyle, (LPCSTR) classptr, (LPCSTR) wintitle,
		style, x1 + xoffset, y1 + yoffset, width, height, hwndshow,
		(HMENU) (INT_PTR) (control->useritem + ITEM_MSG_START),
		hinstancemain,
		control		/* This is passed in to the control's WinProc with the WM_CREATE message. Table uses this */
		);
	if (control->ctlhandle != NULL) SetWindowLongPtr(control->ctlhandle, GWLP_USERDATA, (LONG_PTR) control);
	//control->insthandle = hinstancemain;
	if (control->ctlhandle == NULL) {
		control->changeflag = FALSE;
		guiaErrorMsg("Create control error", GetLastError());
		return RC_ERROR;
	}

	/**
	 * Version 6 of ComCtl32.dll uses themes and it is messing up the
	 * background color. The simple answer that seems to work well
	 * is to disable themes for tabs
	 *
	 * 07/11/12 JPR
	 */
	if (control->type == PANEL_TAB) {
		SetWindowTheme(control->ctlhandle, L" ", L" ");
	}

	/**
	 * Ditto for BOXTITLEs
	 * The box lines were way too light in color. This helps.
	 *
	 * 04/08/13 JPR
	 */
	else if (control->type == PANEL_BOXTITLE) {
		SetWindowTheme(control->ctlhandle, L" ", L" ");
	}
	control->shownflag = TRUE;
	if (control->font != NULL) {
		SendMessage(control->ctlhandle, WM_SETFONT, (WPARAM) control->font, MAKELPARAM(TRUE, 0));
	}

	/**
	 * On or about 27 APR 2021 Giuseppe Sonanini found a 'PAINTING' bug with
	 * tab groups in a dialog. They were invisible.
	 * The part after the OR in the below IF fixes this.
	 */
	if (winbackflag == FALSE || (control->type == PANEL_TAB && dialogflag)) {
		BringWindowToTop(control->ctlhandle);
	}

	/* Do not subclass tables */
	if (control->type != PANEL_TABLE) control->oldproc = setControlWndProc(control);
	else control->oldproc = (WNDPROC) GetWindowLongPtr(control->ctlhandle, GWLP_WNDPROC);

	switch (control->type) {
	case PANEL_TEXT:
	case PANEL_VTEXT:
		guiaGetCtlTextBoxSize(control, wintitle, &width, &height);
		control->rect.bottom = control->rect.top + (height - 1); /* store height */
		break;
	case PANEL_RVTEXT:
	case PANEL_RTEXT:
		guiaGetCtlTextBoxSize(control, wintitle, &width, &height);
		control->rect.bottom = control->rect.top + (height - 1); /* store height */
		rtextwidth = width;
		break;
	case PANEL_CVTEXT:
	case PANEL_CTEXT:
		if (width <= 0 && height <= 0) guiaGetCtlTextBoxSize(control, wintitle, &width, &height);
		else if (height <= 0) guiaGetCtlTextBoxSize(control, wintitle, &i1, &height);
		control->rect.bottom = control->rect.top + (height - 1); /* store height */
		break;
	case PANEL_PEDIT:
	case PANEL_EDIT:
		guiaGetCtlTextBoxSize(control, "M", &i1, &height);
		SendMessage(control->ctlhandle, EM_LIMITTEXT, 0, 0L);
		height += (height >> 2);
		control->rect.bottom = control->rect.top + (height - 1); /* store height */
		break;
	case PANEL_FEDIT:
		guiaGetCtlTextBoxSize(control, "M", &i1, &height);
		cptr1 = guiLockMem(control->mask);
		SendMessage(control->ctlhandle, EM_LIMITTEXT, strlen((LPSTR)cptr1), 0L);
		guiUnlockMem(control->mask);
		height += (height >> 2);
		control->rect.bottom = control->rect.top + (height - 1); /* store height */
		break;

		/**
		 * Version 6 of ComCtl32.dll seems to have a bug only on Win XP
		 * The runtime will crash badly if a multi-line edit box
		 * is created with text already in it (wintitle not NULL)
		 * So we defer putting the text in it until now.
		 * That seems to get around the bug.
		 *
		 * 07/13/12 JPR
		 */
	case PANEL_MEDIT:
	case PANEL_MEDITHS:
	case PANEL_MEDITVS:
	case PANEL_MEDITS:
	case PANEL_MLEDITHS:
	case PANEL_MLEDITVS:
	case PANEL_MLEDITS:
	case PANEL_MLEDIT:
		if (ISMLEDITTYPE(control)) SendMessage(control->ctlhandle, EM_LIMITTEXT, control->maxtextchars, 0L);
		if (deferTextForMEDIT != NULL && IsWindows7OrGreater()) {
			SetWindowText(control->ctlhandle, deferTextForMEDIT);
		}
		break;

	case PANEL_PLEDIT:
	case PANEL_LEDIT:
		guiaGetCtlTextBoxSize(control, "M", &i1, &height);
		SendMessage(control->ctlhandle, EM_LIMITTEXT, control->maxtextchars, 0L);
		height += (height >> 2);
		control->rect.bottom = control->rect.top + height - 1; /* store height */
		break;
	case PANEL_CHECKBOX:
	case PANEL_LTCHECKBOX:
	case PANEL_RADIOBUTTON:
	case PANEL_LASTRADIOBUTTON:
	case PANEL_BUTTON:
		if (ISMARKED(control)){
			SendMessage(control->ctlhandle, BM_SETCHECK, BST_CHECKED, 0L);
		}
		if (wintitle == NULL || wintitle[0] == '\0') guiaGetCtlTextBoxSize(control, " ", &width, &height);
		else guiaGetCtlTextBoxSize(control, wintitle, &width, &height);
		width += height;  /* add height of box to width */
		control->rect.bottom = control->rect.top + (height - 1); /* store height */
		break;
	case PANEL_TREE:
		hImageList = ImageList_Create(16, 16, ILC_COLORDDB | ILC_MASK, 1, 1);
		SendMessage(control->ctlhandle, TVM_SETIMAGELIST,  TVSIL_STATE, (LPARAM)(HIMAGELIST)hImageList);
		//TreeView_SetImageList(control->ctlhandle, hImageList, TVSIL_STATE);
	    fhdc = GetDC(NULL);
	    membmp = CreateCompatibleBitmap(fhdc, 16, 16); /* first image is blank, or none */
		ImageList_Add(hImageList, membmp, (HBITMAP) NULL);
		ReleaseDC(NULL, fhdc);
		ptreeicon = control->tree.iconlist;
		while (ptreeicon != NULL) {
			hicon = createiconfromres(*ptreeicon->iconres, FALSE, &error);
			if (hicon == NULL) {
				guiaErrorMsg("Create tree icon error", error);
				return RC_ERROR;
			}
			ImageList_AddIcon(hImageList, hicon);
			ptreeicon = ptreeicon->next;
		}
		if (control->tree.root != NULL) {
			/* items were added to tree before it was shown, so make win32 calls to insert them now */
			treeinsert(control->ctlhandle, control->tree.root);
		}
		if (control->tree.spos != NULL) {
			/*
			 * select the previously selected item
			 *
			 * The TVM_ENSUREVISIBLE message was commented out on 31 Mar 2006.
			 * It seems to be overkill. The SELECTITEM causes vertical scrolling as necessary.
			 * The ENSURE.. message was also causing hosizontal scrolling and this was not desired by
			 * Philippe. Let's see if anyone else complains.
			 *
			 */
#if 0
			SendMessage(control->ctlhandle, TVM_ENSUREVISIBLE, 0, (LPARAM) (HTREEITEM) control->tree.spos->htreeitem);
#endif
			SendMessage(control->ctlhandle, TVM_SELECTITEM, TVGN_CARET, (LPARAM) (HTREEITEM) control->tree.spos->htreeitem);
		}
		break;
	case PANEL_MLISTBOXHS:
	case PANEL_MLISTBOX:
	case PANEL_LISTBOXHS:
	case PANEL_LISTBOX:
		guiaSetTabs(control);
		control->maxlinewidth = 0;
		size.cx = size.cy = 0;
		for (i1 = 0; i1 < control->value1; i1++) {
			if (SendMessage(control->ctlhandle, LB_INSERTSTRING, i1, (LPARAM) ((BYTE **) control->itemcutarray)[i1]) == CB_ERR)
				return RC_ERROR;
			if (control->type == PANEL_MLISTBOXHS || control->type == PANEL_LISTBOXHS) {
				/* control when horizontal scrollbar appears based on width of inserted text */
				/* if boxtabs are used, all of the following will be for nothing because */
				/* LB_SETHORIZONTALEXTENT will be called again in guiaSetTabs */
				fhdc = GetDC(NULL);
				SelectObject(fhdc, control->font);
				GetTextExtentPoint32(fhdc, (CHAR *) ((BYTE **) control->itemcutarray)[i1],
						(int)strlen((CHAR *) ((BYTE **) control->itemcutarray)[i1]), &size);
				ReleaseDC(NULL, fhdc);
				if (size.cx > control->maxlinewidth) control->maxlinewidth = size.cx;
			}
			//SendMessage(control->ctlhandle, LB_SETITEMDATA, i1, control->itemattributes[i1]);
		}
		if (control->type == PANEL_MLISTBOXHS || control->type == PANEL_LISTBOXHS) {
			if (!control->listboxtabs) {
				SendMessage(control->ctlhandle, LB_SETHORIZONTALEXTENT, (WPARAM) control->maxlinewidth + 2, (LPARAM) 0);
			}
		}
		if (control->type == PANEL_MLISTBOX || control->type == PANEL_MLISTBOXHS) setMListboxSelectionState(control);
		else if (control->value) SendMessage(control->ctlhandle, LB_SETCURSEL, control->value - 1, 0L);
		if (control->value4) {
			SendMessage(control->ctlhandle, LB_SETTOPINDEX, control->value4 - 1, 0L);
		}
		hdc = GetDC(hwndmainwin);
		oldhfont = SelectObject(hdc, control->font);
		GetTextMetrics(hdc, &tm);
		SelectObject(hdc, oldhfont);
		ReleaseDC(hwndmainwin, hdc);
		SendMessage(control->ctlhandle, LB_SETITEMHEIGHT, 0, tm.tmHeight + tm.tmExternalLeading);
		break;
	case PANEL_DROPBOX:
		for (i1 = 0; i1 < control->value1; i1++) {
			if (SendMessage(control->ctlhandle, CB_INSERTSTRING, i1, (LPARAM) ((BYTE **) control->itemarray)[i1]) == CB_ERR)
				return RC_ERROR;
			//SendMessage(control->ctlhandle, CB_SETITEMDATA, i1, control->itemattributes[i1]);
		}
		if (control->value) {
			SendMessage(control->ctlhandle, CB_SETCURSEL, (WPARAM)(control->value - 1), 0L);
		}
		hdc = GetDC(hwndmainwin);
		oldhfont = SelectObject(hdc, control->font);
		GetTextMetrics(hdc, &tm);
		SelectObject(hdc, oldhfont);
		ReleaseDC(hwndmainwin, hdc);
		SendMessage(control->ctlhandle, CB_SETITEMHEIGHT, 0, tm.tmHeight + tm.tmExternalLeading);
		SendMessage(control->ctlhandle, CB_SETITEMHEIGHT, (WPARAM)-1, tm.tmHeight + tm.tmExternalLeading);
		break;
	case PANEL_HSCROLLBAR:
	case PANEL_VSCROLLBAR:
		SetScrollRange(control->ctlhandle, SB_CTL, control->value1, control->value2, TRUE);
		SetScrollPos(control->ctlhandle, SB_CTL, control->value, TRUE);
		break;
	case PANEL_PROGRESSBAR:
		SendMessage(control->ctlhandle, PBM_SETRANGE32, (WPARAM)(int)control->value1, (LPARAM)(int)control->value2);
		SendMessage(control->ctlhandle, PBM_SETPOS, (WPARAM) control->value, 0);
		break;
	}
	if (ISGRAY(control)) EnableWindow(control->ctlhandle, FALSE);
	SetWindowPos(control->ctlhandle, NULL, x1 + xoffset - rtextwidth, y1 + yoffset, width, height, SWP_NOZORDER | SWP_SHOWWINDOW);
	if (control->type == PANEL_TAB) {
		// We get here on the *first* tab of a group only
		control->tabgroupctlhandle = control->ctlhandle;
		tcitem.mask = TCIF_TEXT | TCIF_PARAM;
		tcitem.pszText = (LPSTR) control->textval;
		tcitem.lParam  = (LPARAM) control;
		TabCtrl_InsertItem(control->tabgroupctlhandle, 0, &tcitem);
		GetClientRect(control->tabgroupctlhandle, &rect);
		TabCtrl_AdjustRect(control->tabgroupctlhandle, FALSE, &rect); // @suppress("Symbol is not resolved")
		control->tabgroupxoffset = x1 + rect.left;
		control->tabgroupyoffset = y1 + rect.top;

		/**
		 * When a tab is created, save the WinProc in a local, static variable.
		 */
		oldtcproc = control->oldproc;
	}

	if (control->tooltip != NULL && control->res != NULL) {
		/* must make this call after control is shown */
		guiaAddTooltip(control);
	}
	if (control->textval != NULL) guiUnlockMem(control->textval);
	control->changeflag = FALSE;
	return 0;
}

WNDPROC setControlWndProc(CONTROL *control) {
	if (ISSHOWONLY(control) /* || ISNOFOCUS(control) */ || (!ISEDIT(control) && (control->style & CTRLSTYLE_READONLY))) {
		return (WNDPROC)SetWindowLongPtr(control->ctlhandle, GWLP_WNDPROC, (LONG_PTR) guiaControlInactiveProc);
	}
	else {
		return (WNDPROC)SetWindowLongPtr(control->ctlhandle, GWLP_WNDPROC, (LONG_PTR) guiaControlProc);
	}
}

static void treeposition(CHAR *buf, TREESTRUCT *root, TREESTRUCT *node)
{
	INT i1 = 1;
	CHAR work[8];
	TREESTRUCT *ptree;
	if (node->parent == NULL) ptree = root;
	else ptree = node->parent->firstchild;
	while (ptree != node) {
		ptree = ptree->nextchild;
		i1++;
	}
	if (node->parent != NULL) treeposition(buf, root, node->parent);
	mscitoa(i1, work);
	strcat(buf, work);
	strcat(buf, ",");
	return;
}

static TREESTRUCT * treesearch(TREESTRUCT *node, HTREEITEM hitem)
{
	TREESTRUCT *t1, *t2;
	if (node != NULL) {
		if (node->htreeitem == hitem) return node;
		else {
			t1 = treesearch(node->firstchild, hitem);
			t2 = treesearch(node->nextchild, hitem);
			return (t1 == NULL ? t2 : t1);
		}
	}
	return NULL;
}

/**
 * Recursively delete items from tree structure and from view.
 * Makes Win32 API calls
 */
static void treedelete(HWND ctlhandle, INT shown, TREESTRUCT *node)
{
	TVITEM tvitem;
	ZHANDLE ptr;
	while (node != NULL) {
		if (node->firstchild != NULL) treedelete(ctlhandle, shown, node->firstchild);
		if (shown) {
			SendMessage(ctlhandle, TVM_DELETEITEM, 0, (LPARAM) (HTREEITEM) node->htreeitem);
			if (node->parent != NULL && TreeView_GetChild(ctlhandle, node->parent->htreeitem) == NULL) {
				/* parent has no more children */
				tvitem.mask = TVIF_CHILDREN;
				tvitem.hItem = node->parent->htreeitem;
				tvitem.cChildren = 0;
				/* visually remove the +/- sign next to the tree node */
				SendMessage(ctlhandle, TVM_SETITEM, 0, (LPARAM) (LPTVITEM) &tvitem);
			}
		}
		ptr = (ZHANDLE) node;
		node = node->nextchild;
		guiFreeMem(ptr);
	}
	return;
}

/**
 * recursively show tree structure in tree view control
 */
static void treeinsert(HWND ctlhandle, TREESTRUCT *node)
{
	TVINSERTSTRUCT tvins;
	TVITEM tvitem;
	if (node == NULL) return;
	tvitem.mask = TVIF_CHILDREN | TVIF_TEXT | TVIF_STATE;
	tvitem.cChildren = 0;
	tvitem.pszText = (CHAR *) node->text;
	tvitem.stateMask = TVIS_STATEIMAGEMASK;
	tvitem.state = INDEXTOSTATEIMAGEMASK(node->imageindex); /* 0 = no image */
	tvins.item = tvitem;
	tvins.hInsertAfter = TVI_LAST;
	if (node->parent == NULL) tvins.hParent = TVI_ROOT;
	else tvins.hParent = node->parent->htreeitem;
	node->htreeitem = (HTREEITEM) SendMessage(ctlhandle, TVM_INSERTITEM, 0, (LPARAM) (LPTVINSERTSTRUCT) &tvins);
	if (node->parent != NULL) {
		tvitem.mask = TVIF_CHILDREN;
		tvitem.hItem = node->parent->htreeitem;
		tvitem.cChildren = TRUE;
		SendMessage(ctlhandle, TVM_SETITEM, 0, (LPARAM) (LPTVITEM) &tvitem);
	}
	treeinsert(ctlhandle, node->firstchild);
	treeinsert(ctlhandle, node->nextchild);
	if (node->expanded) {
		SendMessage(ctlhandle, TVM_EXPAND, (WPARAM) TVE_EXPAND, (LPARAM) node->htreeitem);
	}
}

INT guiamemicmp(BYTE * src, BYTE * dest, INT len)
{
	while(len--) if (_totupper(((CHAR *) src)[len]) != _totupper(((CHAR *)dest)[len])) return(1);
	return 0;
}

INT guiaSetResFont(RESOURCE *res, BYTE *text, INT textlen, INT firstflag)
{
	INT i1, i2;
	BYTE work[8];
	LOGFONT lf;
	HDC hdc;
	INT retval = 0;

	for (i1 = i2 = 0; i1 < textlen && i1 < 8 && isalnum(text[i1]); i1++) {
		work[i2++] = toupper(text[i1]);
	}
	if (i2 == 7 && !strncmp((CHAR *) work, "DEFAULT", 7)) {
		res->font = NULL;
		text += i1;
		textlen -= i1;
	}
	hdc = GetDC(res->hwnd);
	if (firstflag || res->font == NULL) {
		/* use default font as a basis for user specified font changes */
		/* res->font == NULL will occur if DEFAULT was used last */
		GetObject(hfontcontrolfont, sizeof(LOGFONT), &lf);
	}
	else {
		/* use last font as a basis for user specified font changes */
		/*memcpy(&lf, &lastfont, sizeof(LOGFONT));*/
		GetObject(res->font, sizeof(LOGFONT), &lf);
	}
	retval = guiaParseFont((CHAR *) text, textlen, &lf, FALSE, hdc);
	res->font = getFont(&lf);
	ReleaseDC(res->hwnd, hdc);
	return retval;
}

/**
 * Find the font for the resource that the control is in.
 * If it is null, set the control's font to the DX default font for controls.
 * If it is not null, set the control's font to the resource's font.
 *
 * The DX default font for controls will be either SYSTEM(9) or as
 * user specified by dbcdx.gui.ctlfont
 */
void guiaSetCtlFont(CONTROL *ctl)
{
	HFONT hfont;

	hfont = (*ctl->res)->font;
	if (hfont == NULL) {
		ctl->font = hfontcontrolfont;
	}
	else {
		ctl->font = hfont;
	}
}

static LPICONRESOURCE guiareadiconfromICOfile(HANDLE hFile)
{
	LPICONRESOURCE    	lpIR = NULL, lpNew = NULL;
	UINT                i;
	DWORD            	dwBytesRead;
	LPICONDIRENTRY    	lpIDE = NULL;


	/* Allocate memory for the resource structure */
	if((lpIR = (ICONRESOURCE *) guiAllocMem(sizeof(ICONRESOURCE))) == NULL)
	{
		CloseHandle(hFile);
		return NULL;
	}
	/* Read in the header */
	if((lpIR->nNumImages = guiareadICOheader(hFile)) == (UINT)-1)
	{
		CloseHandle(hFile);
		guiFreeMem((ZHANDLE) lpIR);
		return NULL;
	}
	/* Adjust the size of the struct to account for the images */
	if( (lpNew = (ICONRESOURCE *) guiReallocMem( (ZHANDLE) lpIR, sizeof(ICONRESOURCE) + ((lpIR->nNumImages-1) * sizeof(ICONIMAGE)))) == NULL)
	{
		CloseHandle(hFile);
		guiFreeMem((ZHANDLE) lpIR);
		return NULL;
	}
	lpIR = lpNew;
	/* Allocate enough memory for the icon directory entries */
	if( (lpIDE = (ICONDIRENTRY *) guiAllocMem(lpIR->nNumImages * sizeof(ICONDIRENTRY))) == NULL)
	{
		CloseHandle(hFile);
		guiFreeMem((ZHANDLE) lpIR);
		return NULL;
	}
	/* Read in the icon directory entries */
	if(!ReadFile(hFile, lpIDE, lpIR->nNumImages * sizeof(ICONDIRENTRY), &dwBytesRead, NULL))
	{
		CloseHandle(hFile);
		guiFreeMem((ZHANDLE) lpIR);
		return NULL;
	}
	if(dwBytesRead != lpIR->nNumImages * sizeof(ICONDIRENTRY))
	{
		CloseHandle(hFile);
		guiFreeMem((ZHANDLE) lpIR);
		return NULL;
	}
	/* Loop through and read in each image */
	for(i = 0;i < lpIR->nNumImages;i++ )
	{
		/* Allocate memory for the resource */
		if((lpIR->IconImages[i].lpBits = guiAllocMem(lpIDE[i].dwBytesInRes)) == NULL)
		{
			CloseHandle(hFile);
			guiFreeMem((ZHANDLE) lpIR);
			guiFreeMem((ZHANDLE) lpIDE);
			return NULL;
		}
		lpIR->IconImages[i].dwNumBytes = lpIDE[i].dwBytesInRes;
		/* Seek to beginning of this image */
		if( SetFilePointer(hFile, lpIDE[i].dwImageOffset, NULL, FILE_BEGIN) == 0xFFFFFFFF )
		{
			CloseHandle(hFile);
			guiFreeMem((ZHANDLE) lpIR);
			guiFreeMem((ZHANDLE) lpIDE);
			return NULL;
		}
		/* Read it in */
		if(!ReadFile(hFile, lpIR->IconImages[i].lpBits, lpIDE[i].dwBytesInRes, &dwBytesRead, NULL))
		{
			CloseHandle(hFile);
			guiFreeMem((ZHANDLE) lpIR);
			guiFreeMem((ZHANDLE) lpIDE);
			return NULL;
		}
		if(dwBytesRead != lpIDE[i].dwBytesInRes)
		{
			CloseHandle(hFile);
			guiFreeMem((ZHANDLE) lpIDE);
			guiFreeMem((ZHANDLE) lpIR);
			return NULL;
		}
		/* Set the internal pointers appropriately */
		if(!guiaadjusticonimage(&(lpIR->IconImages[i])))
		{
			CloseHandle(hFile);
			guiFreeMem((ZHANDLE) lpIDE);
			guiFreeMem((ZHANDLE) lpIR);
			return NULL;
		}
	}
	/* Clean up	*/
	guiFreeMem((ZHANDLE) lpIDE);
	CloseHandle(hFile);
	return lpIR;
}

static UINT guiareadICOheader( HANDLE hFile )
{
	WORD    Input;
	DWORD	dwBytesRead;

	/* Read the 'reserved' WORD */
	if(!ReadFile(hFile, &Input, sizeof(WORD), &dwBytesRead, NULL))
		return (UINT)-1;
	/* Did we get a WORD? */
	if(dwBytesRead != sizeof(WORD))
		return (UINT)-1;
	/* Was it 'reserved' ?   (ie 0) */
	if(Input != 0)
		return (UINT)-1;
	/* Read the type WORD */
	if(!ReadFile(hFile, &Input, sizeof(WORD), &dwBytesRead, NULL))
		return (UINT)-1;
	/* Did we get a WORD? */
	if(dwBytesRead != sizeof(WORD))
		return (UINT)-1;
	/* Was it type 1? */
	if(Input != 1)
		return (UINT)-1;
	/* Get the count of images */
	if(!ReadFile(hFile, &Input, sizeof(WORD), &dwBytesRead, NULL))
		return (UINT)-1;
	/* Did we get a WORD? */
	if(dwBytesRead != sizeof(WORD))
		return (UINT)-1;
	/* Return the count */
	return Input;
}

static BOOL guiaadjusticonimage( LPICONIMAGE lpImage )
{
	/* Sanity check */
	if( lpImage==NULL )
		return FALSE;
	/* BITMAPINFO is at beginning of bits */
	lpImage->lpbi = (LPBITMAPINFO)lpImage->lpBits;
	/* Width - simple enough */
	lpImage->Width = lpImage->lpbi->bmiHeader.biWidth;
	/* Icons are stored in funky format where height is doubled - account for it */
	lpImage->Height = (lpImage->lpbi->bmiHeader.biHeight)/2;
	/* How many colors? */
	lpImage->Colors = lpImage->lpbi->bmiHeader.biPlanes * lpImage->lpbi->bmiHeader.biBitCount;
	return TRUE;
}

static HICON guiamakeiconfromresource(LPICONIMAGE lpIcon)
{
	HICON hIcon = NULL;

	/* Sanity Check */
	if( lpIcon == NULL )
		return NULL;
	if( lpIcon->lpBits == NULL )
		return NULL;
	/* Let the OS do the real work :) */
	hIcon = CreateIconFromResourceEx( lpIcon->lpBits, lpIcon->dwNumBytes, TRUE, 0x00030000,
			(*(LPBITMAPINFOHEADER)(lpIcon->lpBits)).biWidth, (*(LPBITMAPINFOHEADER)(lpIcon->lpBits)).biHeight/2, 0 );

	/* It failed, odds are good we're on NT so try the non-Ex way */
	if( hIcon == NULL ) {
		/* We would break on NT if we try with a 16bpp image */
		if(lpIcon->lpbi->bmiHeader.biBitCount != 16) {
			hIcon = CreateIconFromResource( lpIcon->lpBits, lpIcon->dwNumBytes, TRUE, 0x00030000 );
		}
	}
	return hIcon;
}

CONTROL* guiaGetFocusControl(RESOURCE * res)
{
	return res->ctlfocus;
}

void guiaMBDebug(CHAR * UserMessage, CHAR * UserTitle)
{
	HANDLE hStdOut;
	DWORD count;
	CHAR buffer[100];

	buffer[0] = '\0';
	lstrcat((LPSTR)buffer, (LPCSTR)UserMessage);
	lstrcat((LPSTR)buffer, (LPCSTR)" -- ");
	lstrcat((LPSTR)buffer, (LPCSTR)UserTitle);
	lstrcat((LPSTR)buffer, (LPCSTR)"\n");
	buffer[strlen((LPCSTR)buffer)] = 0;
	AllocConsole();
	hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	WriteConsole(hStdOut, buffer, (DWORD)strlen((LPCSTR)buffer), &count, NULL);
	return;
}

static LRESULT handleTree_CustomDraw(CONTROL* control, LPNMTVCUSTOMDRAW lpNMCustomDraw) {
	TREESTRUCT *tpos;
	COLORREF workcolor;
	LRESULT retval = CDRF_DODEFAULT;

	if (lpNMCustomDraw->nmcd.dwDrawStage == CDDS_PREPAINT) {
		return CDRF_NOTIFYITEMDRAW;
	}
	else if (lpNMCustomDraw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
		if (lpNMCustomDraw->nmcd.uItemState & CDIS_SELECTED) {
			return retval; /* do not apply color/style if selected */
		}
		tpos = treesearch(control->tree.root, (HTREEITEM) lpNMCustomDraw->nmcd.dwItemSpec);
		if (tpos == NULL) return retval;
		if (tpos->color == 0 && control->textcolor != 0) {
			if (control->textcolor == TEXTCOLOR_CUSTOM)  workcolor = control->textfgcolor;
			else {
				workcolor = GetColorefFromColorNumber(control->textcolor);
			}
		}
		else {
			workcolor = tpos->color;
		}
		lpNMCustomDraw->clrText = workcolor; //GetColorefFromColorNumber(workcolor);
		if (tpos->attr1 != LISTSTYLE_UNSPECIFIED) {
			LOGFONT lf;
			HFONT hfont;
			GetObject(control->font, sizeof(LOGFONT), (LPVOID) &lf);
			if (tpos->attr1 == LISTSTYLE_PLAIN) {
				lf.lfWeight = FW_NORMAL;
				lf.lfItalic = FALSE;
			}
			else {
				if (tpos->attr1 & LISTSTYLE_BOLD) lf.lfWeight = FW_BOLD;
				if (tpos->attr1 & LISTSTYLE_ITALIC) lf.lfItalic = TRUE;
			}
			hfont = getFont(&lf);
			SelectObject(lpNMCustomDraw->nmcd.hdc, hfont);
			retval |= CDRF_NEWFONT;
		}
	}
	return retval;
}

/**
 * Called when a DB/C Window or Dialog process receives a WM_COMMAND
 * If this is a focus notification from a single line edit box, we mess with the
 * selection and return 1;
 * Otherwise return 0
 *
 *
 * EM_SETSEL Remarks from MSDN
 *
 * The start value can be greater than the end value.
 * The lower of the two values specifies the character position of the first character in the selection.
 * The higher value specifies the position of the first character beyond the selection.
 * The start value is the anchor point of the selection, and the end value is the active end.
 * If the user uses the SHIFT key to adjust the size of the selection,
 * the active end can move but the anchor point remains the same.
 * If the start is 0 and the end is -1, all the text in the edit control is selected.
 * If the start is -1, any current selection is deselected.
 *
 */
static INT checkSingleLineEditFocusNotification(WPARAM wParam, LPARAM lParam) {
	WORD nc = HIWORD(wParam);
	if (lParam != (LPARAM) NULL && (nc == EN_SETFOCUS || nc == EN_KILLFOCUS)) {
		CONTROL *ectrl = (CONTROL*) GetWindowLongPtr((HWND) lParam, GWLP_USERDATA);
		if (ectrl != NULL && ISSINGLELINEEDIT(ectrl)) {
			if (nc == EN_SETFOCUS) {
				SendMessage(ectrl->ctlhandle, EM_SETSEL, 0, -1);
				SendMessage(ectrl->ctlhandle, EM_GETSEL,
						(WPARAM)&(ectrl->editinsertpos), (LPARAM)&(ectrl->editendpos));
			}
			else {
				SendMessage(ectrl->ctlhandle, EM_GETSEL,
						(WPARAM)&(ectrl->editinsertpos), (LPARAM)&(ectrl->editendpos));
				SendMessage(ectrl->ctlhandle, EM_SCROLLCARET, 0, 0);
			}
			return 1;
		}
	}
	return 0;
}

/**
 * ALWAYS returns zero.
 */
static INT hidePixmap(PIXHANDLE ph, RECT *rect) {
	PIXMAP *pix = *ph;
	WINHANDLE wh = pix->winshow;
	WINDOW *win = *wh;
	HWND hWnd = win->hwnd;
	PAINTSTRUCT ps;
	HDC hPaintDC;
	RECT rectDest = *rect;

	InvalidateRect(hWnd, &rectDest, FALSE);

	hPaintDC = BeginPaint(hWnd, &ps);
	if (hPaintDC != NULL) {
		HPALETTE hOldBitPal, hOldPal;
		PIXHANDLE nextpixhandle;
		RECT oneImageRect, dummyRect;
		DWORD dwWaitResult;
		HDC hdc = CreateCompatibleDC(hPaintDC);

		eraseWindowBackground_EX(hPaintDC, &ps);
		hOldBitPal = hOldPal = NULL;
		for (nextpixhandle = win->showpixhdr; nextpixhandle != NULL; nextpixhandle = pix->showpixnext) {
			pix = *nextpixhandle;
			oneImageRect.top = pix->vshow - win->scrolly + win->bordertop;
			oneImageRect.bottom = oneImageRect.top + pix->vsize;
			oneImageRect.left = pix->hshow - win->scrollx;
			oneImageRect.right = oneImageRect.left + pix->hsize;
			if (!IntersectRect(&dummyRect, &oneImageRect, &rectDest)) continue;

			dwWaitResult = WaitForSingleObject(pix->hPaintingMutex, 50);
			if (dwWaitResult != WAIT_OBJECT_0) {
				continue;
			}
			SelectObject(hdc, pix->hbitmap);
			if (pix->hpal != NULL) {
				if (hOldBitPal == NULL) hOldBitPal = SelectPalette(hdc, pix->hpal, TRUE);
				else SelectPalette(hdc, pix->hpal, TRUE);
				if (hOldPal == NULL) hOldPal = SelectPalette(hPaintDC, pix->hpal, TRUE);
				else SelectPalette(hPaintDC, pix->hpal, TRUE);
				RealizePalette(hPaintDC);
			}
			if (BitBlt(hPaintDC, pix->hshow - win->scrollx,
				pix->vshow - win->scrolly + win->bordertop,
				pix->hsize, pix->vsize, hdc, 0, 0, SRCCOPY) == 0
				&& !bitbltErrorIgnore)
			{
				guiaErrorMsg("BitBlt failure", GetLastError());
			}
			ReleaseMutex(pix->hPaintingMutex);
		}
		if (hOldBitPal != NULL) SelectPalette(hdc, hOldBitPal, TRUE);
		if (hOldPal != NULL) {
			SelectPalette(hPaintDC, hOldPal, TRUE);
			RealizePalette(hPaintDC);
		}
		DeleteDC(hdc);
		EndPaint(hWnd, &ps);
	}
	else {
		guiaErrorMsg("BeginPaint in showPixmap", GetLastError());
	}
	return 0;
}

/**
 * ALWAYS returns zero.
 */
static INT showPixmap(PIXHANDLE ph, RECT *rect) {
	PIXMAP *pix = *ph;
	WINHANDLE wh = pix->winshow;
	WINDOW *win = *wh;
	HWND hWnd = win->hwnd;
	PAINTSTRUCT ps;
	HDC hPaintDC;
	RECT rectDest = *rect;

	InvalidateRect(hWnd, &rectDest, FALSE);
	hPaintDC = BeginPaint(hWnd, &ps);
	if (hPaintDC != NULL) {
		HPALETTE hOldPal = NULL, hOldBitPal = NULL;
		HDC hdc = CreateCompatibleDC(hPaintDC);

		eraseWindowBackground_EX(hPaintDC, &ps);

		SelectObject(hdc, pix->hbitmap);
		if (pix->hpal != NULL) {
			hOldBitPal = SelectPalette(hdc, pix->hpal, TRUE);
			hOldPal = SelectPalette(hPaintDC, pix->hpal, TRUE);
			RealizePalette(hPaintDC);
		}
		if (BitBlt(hPaintDC, pix->hshow - win->scrollx,
			pix->vshow - win->scrolly + win->bordertop,
			pix->hsize, pix->vsize, hdc, 0, 0, SRCCOPY) == 0
			&& !bitbltErrorIgnore)
		{
			guiaErrorMsg("BitBlt failure", GetLastError());
		}
		if (hOldBitPal != NULL) SelectPalette(hdc, hOldBitPal, TRUE);
		if (hOldPal != NULL) {
			SelectPalette(hPaintDC, hOldPal, TRUE);
			RealizePalette(hPaintDC);
		}
		DeleteDC(hdc);
		EndPaint(hWnd, &ps);
	}
	else {
		guiaErrorMsg("BeginPaint in showPixmap", GetLastError());
	}
	return 0;
}

/**
 * window procedure for DB/C windows
 */
LRESULT CALLBACK guiaWinProc(HWND hwnd, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	static HWND nowactmsghwnd = 0;
	INT i1, iCtlsave, iCtlNum, iWinWidth, iWinHeight, iCurCtrl;
	INT textlen;
	BOOL rc;
	CHAR *msgid;
	CHAR xtext[TREEVIEW_ITEM_MAX_SIZE];
	static BOOL outmsgsent = TRUE;
	static BOOL focwasontab = FALSE;
	static BOOL bIsPopupMenuActive = FALSE;
	static HMENU hPopupMenu = NULL;
	//BOOL focset;
	POINTS mousepos;
	POINT cpos;
	RECT rect, clientrect;
	WINDOW *win;
	WINHANDLE currentwinhandle;
	RESOURCE *res;
	RESHANDLE currentreshandle;
	CONTROL *control, *pctlBase, *currentcontrol;
	HWND hwndDlg, hwndCtrl;
	USHORT usCallbackChar;
	SHORT sRepeatCnt, sNextRepeat;
	BYTE ucDataChar;
	HDC hPaintDC;
	PAINTSTRUCT ps;
	TVITEM tvitem;
	LPTOOLTIPTEXT lpToolTipText;
	LPNMHDR nmhdr;
	COLORREF workcolor;
	HANDLE hpen, hpenold;
	DBCDDE *ddeptr;
	DDEPEEK *ddepeek;
	TREESTRUCT *tpos;
	PIDLIST_ABSOLUTE pidla;
	FILEDLGINFO *finfo;
	DIRDLGINFO *dinfo;
	LPBROWSEINFO binfo;
	WORD event;
	LRESULT lresult;

	pvistart();
	currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (currentwinhandle != NULL) {
		GetCursorPos(&cpos);
		GetWindowRect(hwnd, &clientrect);
		/* Only change the cursor when it is more than 4 pixels inside of  */
		/* the window so windows can have a chance to allow mouse resizing  */
		if (cpos.x > clientrect.left + 4 && cpos.x < clientrect.right - 4) {
			if (cpos.y > clientrect.top + 4 && cpos.y < clientrect.bottom - 4) {
				SetCursor((*currentwinhandle)->hcursor);
			}
		}
	}
	pviend();
	/* process messages from guia functions SendMessage() calls */
	switch (nMessage) {
	case GUIAM_SYNC:
		return 0;
	case GUIAM_WINCREATE:
		return(wincreate((GUIAWINCREATESTRUCT *) lParam));
	case GUIAM_WINDESTROY:
		return(windestroy((HWND) lParam));
	case GUIAM_DLGCREATE:
		return(dlgcreate((GUIADLGCREATESTRUCT *) lParam));
	case GUIAM_DLGDESTROY:
		return(dlgdestroy((HWND) lParam));

	case GUIAM_SHOWCONTROLS:
		gShowControlsReturnCode = showcontrols((GUIASHOWCONTROLS *) lParam);
		SetEvent(showControlsEvent);
		return gShowControlsReturnCode;

	case GUIAM_HIDECONTROLS:
		return(hidecontrols((GUIAHIDECONTROLS *) lParam));
	case GUIAM_CREATEWINDOWEX:
		return (LRESULT) createwindowex((GUIAEXWINCREATESTRUCT *) lParam);
	case GUIAM_WINSHOWCARET:
		return(winshowcaret((WINDOW *) lParam));
	case GUIAM_WINHIDECARET:
		return(winhidecaret((WINDOW *) lParam));
	case GUIAM_WINMOVECARET:
		return(winmovecaret((WINDOW *) lParam));
	case GUIAM_MENUCREATE:
		return(menucreate((RESHANDLE) lParam));
	case GUIAM_MENUDESTROY:
		return(menudestroy((RESOURCE *) lParam));
	case GUIAM_POPUPMENUSHOW:
		return(popupmenushow((RESHANDLE) lParam));
	case GUIAM_ERASEDROPLISTBOX:
		lresult = erasedroplistbox((CONTROL *) lParam);
		SetEvent(eraseDropListControlEvent);
		return lresult;
	case GUIAM_GETWINDOWTEXT:
		return(getwindowtext((GUIAGETWINTEXT *) lParam));
	case GUIAM_GETSELECTEDTREEITEMTEXT:
		return(getselectedtreeitemtext((GUIAGETWINTEXT *) lParam));
	case GUIAM_STARTDRAW:
		return(startdraw((PIXHANDLE) lParam));
	case GUIAM_TOOLBARCREATE:
		return(toolbarcreate((RESOURCE *) lParam));
	case GUIAM_TOOLBARDESTROY:
		return(toolbardestroy((RESOURCE *) lParam));
	case GUIAM_SETTOOLBARTEXT:
		return(settoolbartext((CONTROL *) lParam));
	case GUIAM_STATUSBARCREATE:
		return(statusbarcreate((WINDOW *) lParam));
	case GUIAM_STATUSBARDESTROY:
		return(statusbardestroy((WINDOW *) lParam));
	case GUIAM_STATUSBARTEXT:
		return(statusbartext((WINDOW *) lParam));
	case GUIAM_SETCTRLFOCUS:
		return(setctrlfocus((CONTROL *) lParam));
	case GUIAM_ADDMENUITEM:
		return(addmenuitem((MENUENTRY *) lParam, (UINT) wParam));
	case GUIAM_DELETEMENUITEM:
		return(deletemenuitem((HMENU) lParam, (UINT) wParam));
	case GUIAM_CHECKSCROLLBARS:
		return(checkScrollBars((WINHANDLE) lParam));
	case GUIAM_STARTDOC:
		i1 = StartDoc(((STARTDOCINFO *) lParam)->handle, ((STARTDOCINFO *) lParam)->docinfo);
		if (i1 <= 0) ((STARTDOCINFO *) lParam)->errval = GetLastError();
		return(i1);
	case GUIAM_SHOWPIX:
		return showPixmap((PIXHANDLE)lParam, (RECT *) wParam);
	case GUIAM_HIDEPIX:
		return hidePixmap((PIXHANDLE)lParam, (RECT *) wParam);

	/*
	 * Don't believe the pvis are necessary below.
	 * This is called from main thread using SendMessage, so it is blocked.
	 * No chance of memory moving.
	 */
	case GUIAM_WINACTIVATE:
		win = (WINDOW *) lParam;
		nowactmsghwnd = win->hwnd;
		if (IsZoomed(win->hwnd) == 0) {
			/* window is not maximized so restore it */
			//pvistart();
			ShowWindow(win->hwnd, SW_SHOW);
//			if (win->hwndtaskbarbutton != NULL) {
//				DestroyWindow(win->hwndtaskbarbutton);
//				win->hwndtaskbarbutton = NULL;
//			}
			//pviend();
		}
		SetWindowPos(win->hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
		SetForegroundWindow(win->hwnd);
		return 0;
	case GUIAM_CHOOSEFONT:
		return(ChooseFont((LPCHOOSEFONT) lParam));

	/**
	 * OPENDIRDLG
	 *
	 * The BROWSEINFO lParam is passed into the callback function as lpData
	 * We use it there, if present, to set the selection of the dialog.
	 */
	case GUIAM_DIRDLG:
		pvistart();
		if (!coinitializeex) {
			CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
			coinitializeex = TRUE;
		}
		dinfo = (DIRDLGINFO *) lParam;
		binfo = &dinfo->dirstruct;
		if (dinfo->pszStartDir != NULL || dinfo->deviceFilters != 0) {
			binfo->lpfn = BrowseCallbackProc;
			binfo->lParam = lParam; //(LPARAM) dinfo->pszStartDir;
		}
		pidla = SHBrowseForFolder(binfo);
		if (pidla == NULL) {
			dinfo->retcode = FALSE;
		}
		else {
			dinfo->retcode = SHGetPathFromIDList(pidla, binfo->pszDisplayName);
			//ILFree(pidla);
			CoTaskMemFree(pidla);
		}
		pviend();
		return 0;

	case GUIAM_OPENDLG:
		pvistart();
		/**
		 * The below IF block fixes a really obscure problem that only seems to happen on XP.
		 * An access violation crash happened sometimes when the user hovered the mouse
		 * over a PDF file!
		 *
		 * jpr 18 FEB 2013
		 */
		if (!coinitializeex) {
			CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
			coinitializeex = TRUE;
		}
		finfo = (FILEDLGINFO *) lParam;
		// If the user specifies a file name and clicks the OK button, the return value is nonzero.
		__try { // @suppress("Statement has no effect") // @suppress("Symbol is not resolved")
			rc = GetOpenFileName((LPOPENFILENAME) &finfo->ofnstruct);
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			finfo->retcode = 0;
			finfo->extcode = GetExceptionCode();
			pviend();
			return 0;
		}
		finfo->retcode = rc;
		if (!rc) {
			// user canceled or closed or an error happened.
			finfo->extcode = CommDlgExtendedError();
		}
		pviend();
		return 0;
	case GUIAM_SAVEDLG:
		pvistart();
		finfo = (FILEDLGINFO *) lParam;
		// If the user specifies a file name and clicks the OK button and the function is successful,
		// the return value is nonzero.
		rc = GetSaveFileName((LPOPENFILENAME) &finfo->ofnstruct);
		finfo->retcode = rc;
		if (!rc) {
			// user canceled or closed or an error happened.
			finfo->extcode = CommDlgExtendedError();
		}
		pviend();
		return 0;
	case GUIAM_CHOOSECOLOR:
		return(ChooseColor((LPCHOOSECOLOR) lParam));
	case GUIAM_SHUTDOWN:
		PostQuitMessage(0);
		return 0;
	case GUIAM_TOOLBARCREATEDBOX:
		return(toolbarcreatedbox((GUIATOOLBARDROPBOX *) lParam));
	case GUIAM_DDESERVER:
		ddeptr = (DBCDDE *) lParam;
		ddeptr->hwndOwner = CreateWindow(serverclass,
			(CHAR *) ddeptr->hApplication, WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			CW_USEDEFAULT, NULL, NULL, hinstancemain, NULL);
		return 0;
	case GUIAM_DDECLIENT:
		ddeptr = (DBCDDE *) lParam;
		ddeptr->hwndOwner = CreateWindow(clientclass,
			(CHAR *) ddeptr->hApplication, WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			CW_USEDEFAULT, NULL, NULL, hinstancemain, NULL);
		return 0;
	case GUIAM_DDEPEEK:
		ddepeek = (DDEPEEK *) lParam;
		return(PeekMessage(ddepeek->msg, ddepeek->hwnd, ddepeek->msgmin, ddepeek->msgmax, ddepeek->removeflags));
	case GUIAM_DDEDESTROY:
		ddeptr = (DBCDDE *) lParam;
		DestroyWindow(ddeptr->hwndOwner);
		return 0;

	case GUIAM_TRAYICON:
		if (shell32_Major < 5) return 0;
		event = LOWORD(lParam);
		if (event != WM_RBUTTONUP && event != WM_CONTEXTMENU) return 0;
		if ((IsWindows7OrGreater() ? wParam : HIWORD(lParam)) == TRAY_ICON_ID) {
			DWORD threadId;
			DWORD ret = 0;
			if (event == WM_RBUTTONUP) {
				if (IsWindows7OrGreater()) {
					DWORD loca = GetMessagePos();
					ret = doTrayPopup(GET_X_LPARAM(loca), GET_Y_LPARAM(loca));
				}
			}
			else if (event == WM_CONTEXTMENU) {
				if (!IsWindows7OrGreater()) {
					ret = doTrayPopup(GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
				}
			}
			if (ret == 100) {
				CreateThread(NULL, 0, aboutPopup, NULL, 0, &threadId);
			}
			else if (ret == 900)
				CreateThread(NULL, 0, trayShutdown, NULL, 0, &threadId);
		}
		return 0;
	}

/* process normal window messages */
	switch (nMessage) {

	/*
	 * This message is sent by us to ourselves.
	 * Sent for a change/application/desktopicon
	 */
	case WM_SETICON:
		if (hwnd != hwndmainwin || wParam != ICON_BIG) break;
		if (!notifyIconData.cbSize) break;
		notifyIconData.hIcon = (HICON) lParam;
		notifyIconData.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP;
		Shell_NotifyIcon(NIM_MODIFY, &notifyIconData);
		if (shell32_Major >= 5) {
			notifyIconData.uVersion = NOTIFYICON_VERSION_4;
			Shell_NotifyIcon(NIM_SETVERSION, &notifyIconData);
		}
		break;

	/*
	 * This message is sent by us to ourselves.
	 * Sent for a change/application/taskbartext
	 */
	case WM_SETTEXT:
		if (hwnd != hwndmainwin) break;
		if (!notifyIconData.cbSize) break;
		StringCbCopy(notifyIconData.szTip, ARRAYSIZE(notifyIconData.szTip), (CHAR*) lParam);
		notifyIconData.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP;
		Shell_NotifyIcon(NIM_MODIFY, &notifyIconData);
		if (shell32_Major >= 5) {
			notifyIconData.uVersion = NOTIFYICON_VERSION_4;
			Shell_NotifyIcon(NIM_SETVERSION, &notifyIconData);
		}
		break;

	case WM_CTLCOLORSTATIC:
		lresult = handleCtlColorStatic(wParam, lParam);
		if (lresult == -1) break;
		return lresult;

	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
		lresult = handleCtlColorEdit(hwnd, wParam, lParam);
		if (lresult == -1) break;
		return lresult;

	case WM_CTLCOLORBTN:
		control = (CONTROL *) GetWindowLongPtr((HWND) lParam, GWLP_USERDATA);
		workcolor = GetSysColor(COLOR_BTNFACE);
		if (control != NULL) {
			SetBkColor((HDC) wParam, workcolor);
			if (control->textcolor != TEXTCOLOR_DEFAULT)
				SetTextColor((HDC) wParam, GetColorefFromColorNumber(control->textcolor));
			return (LRESULT) hbrButtonFace;
		}
		break;

#if 0
		currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (currentwinhandle != NULL) {
			pvistart();
			win = *currentwinhandle;
			if (win->showreshdr != NULL) {
				res = *win->showreshdr;
				while (res != NULL) {
					control = (CONTROL *) res->content;
					if (control != NULL) {
						for (iCurCtrl = res->entrycount; iCurCtrl > 0; control++) {
							if (control->ctlhandle == (HWND) lParam) break;
							if (--iCurCtrl == 0) break;
						}
						if (iCurCtrl) SetBkColor((HDC) wParam, workcolor);
						if (!iCurCtrl || control->textcolor == TEXTCOLOR_DEFAULT) {
							if (res->showresnext == NULL) break;
							res = *res->showresnext;
							continue;
						}
						color = control->textcolor;
						pviend();
						SetTextColor((HDC) wParam, GetColorefFromColorNumber(color));
						return (LONG) hbrBrush;
					}
					if (res->showresnext == NULL) break;
					res = *res->showresnext;
				}
			}
			pviend();
		}
		break;
#endif

//	case WM_CTLCOLORSCROLLBAR:
//		if (ctl3dflag) return((LONG) hbrushbackground);
//		return 0;

	case WM_DESTROY:
		if (hwnd == hwndmainwin) {
			if (!breakflag) {
				breakflag = TRUE;
				evtset(breakeventid);
				PostQuitMessage(0);
			}
		}
		return(DefWindowProc(hwnd, nMessage, wParam, lParam));

	case WM_NOTIFY:
		nmhdr = (LPNMHDR)lParam;
		if (nmhdr->code == TCN_SELCHANGING || nmhdr->code == TCN_SELCHANGE /* || nmhdr->code == GUIAM_TCN_SELCHANGE_NOFOCUS */)
		{
			handleTabChangeChanging(hwnd, lParam, &focwasontab);
//			currentcontrol = (CONTROL *) GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
//			if (currentcontrol == NULL || !currentcontrol->tabgroup) break;
//			pvistart();
//			res = *currentcontrol->res;
//			control = (CONTROL *) res->content;
//			if (nmhdr->code == TCN_SELCHANGING) {
//				focwasontab = FALSE;
//				if ((hwndCtrl = GetFocus()) != NULL) {
//					i1 = TabCtrl_GetCurSel(nmhdr->hwndFrom) + 1;
//					for (iCurCtrl = 0; iCurCtrl < res->entrycount; iCurCtrl++, control++) {
//						if (hwndCtrl == control->ctlhandle && control->tabgroup == currentcontrol->tabgroup
//							&& control->tabsubgroup == i1) {
//							if (res->focusmsgs) {
//								rescb(res, DBC_MSGLOSEFOCUS, control->useritem, 0);
//							}
//							if ((focwasontab = control->type == PANEL_TAB))
//								drawTabHeaderFocusRectangle(control, FALSE);
//							break;
//						}
//					}
//				}
//			}
//			else if (nmhdr->code == TCN_SELCHANGE || GUIAM_TCN_SELCHANGE_NOFOCUS) {
//				win = *res->winshow;
//				HideTabControls(currentcontrol);
//
//				i1 = TabCtrl_GetCurSel(nmhdr->hwndFrom) + 1;
//				control = (CONTROL *) res->content;
//				for (focset = FALSE, iCurCtrl = res->entrycount; iCurCtrl ; iCurCtrl--, control++) {
//					if (currentcontrol->tabgroup != control->tabgroup) continue;
//					control->selectedtab = i1 - 1;
//					if (control->tabsubgroup != i1) continue;
//					if (control->type == PANEL_TAB) {
//						if (res->itemmsgs && (textlen = (INT)strlen((LPSTR) control->textval))){
//							memcpy(cbmsgdata, control->textval, min(textlen, MAXCBMSGSIZE - 17));
//							rescb(res, DBC_MSGITEM, control->useritem, textlen);
//						}
//					}
//					else {
//						control->changeflag = TRUE;
//						if (control->ctlhandle == NULL) {
//							makecontrol(control, 0 - win->scrollx, win->bordertop - win->scrolly, hwnd, TRUE);
//						}
//						else {
//							ShowWindow(control->ctlhandle, SW_SHOW);
//							// Set the control's window procedure
//							// But not if it is a Table!
//							if (control->type != PANEL_TABLE) setControlWndProc(control);
//							if (ISEDIT(control) && control->type != PANEL_FEDIT) {
//								SetWindowText(control->ctlhandle, (LPCSTR) control->textval);
//							}
//						}
//						control->changeflag = FALSE;
//					}
//					control->shownflag = TRUE;
//					if (nmhdr->code == GUIAM_TCN_SELCHANGE_NOFOCUS) continue;
//					if (!focset) {
//						if (control->type == PANEL_TAB) {
//							if (focwasontab) {
//								focset = TRUE;
//								res->ctlfocus = control;
//								if (res->focusmsgs)	rescb(res, DBC_MSGFOCUS, control->useritem, 0);
//							}
//							drawTabHeaderFocusRectangle(control, TRUE);
//						}
//						else if (res->ctlfocus != control && !focwasontab
//								&& control->useritem && !ISGRAY(control) && !ISSHOWONLY(control) && !ISNOFOCUS(control)
//								&& control->type != PANEL_VTEXT && control->type != PANEL_RVTEXT && control->type != PANEL_CVTEXT) {
//							if (ISRADIO(control)) {
//								if (ISMARKED(control)) setctrlfocus(control);
//							}
//							else setctrlfocus(control);
//							focset = TRUE;
//							res->ctlfocus = control;
//						}
//					}
//				}
//			}
//			pviend();
		}
		else if (nmhdr->code == TVN_SELCHANGED || nmhdr->code == TVN_ITEMEXPANDED || nmhdr->code == TVN_KEYDOWN) {
			currentcontrol = (CONTROL *) GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
			if (currentcontrol == NULL || currentcontrol->type != PANEL_TREE) break;
			pvistart();
			tpos = NULL;
			res = *currentcontrol->res;
			control = (CONTROL *) res->content;
			for (iCurCtrl = res->entrycount; iCurCtrl; iCurCtrl--, control++) {
				if (control->ctlhandle == nmhdr->hwndFrom && control->type == PANEL_TREE)  {
					if (nmhdr->code == TVN_SELCHANGED) {
						/* copy text from currently selected item into control structure */
						tvitem = ((LPNMTREEVIEW) lParam)->itemNew;
						tvitem.mask = TVIF_TEXT;
						tvitem.pszText = &control->tree.text[0];
						tvitem.cchTextMax = TREEVIEW_ITEM_MAX_SIZE;
						TreeView_GetItem(control->ctlhandle, &tvitem);
						control->tree.hitem = tvitem.hItem;
						control->tree.spos = tpos = treesearch(control->tree.root, ((LPNMTREEVIEW) lParam)->itemNew.hItem);
						/* now do message stuff if necessary */
						if (!res->itemmsgs) break;
						else {
							msgid = DBC_MSGITEM;
						}
					}
					else if (nmhdr->code == TVN_KEYDOWN) {
						if (((TV_KEYDOWN*)lParam)->wVKey == 0) {
							msgid = DBC_MSGPICK;
						}
						else break;
					}
					else if (nmhdr->code == TVN_ITEMEXPANDED) {
						if (((LPNMTREEVIEW) lParam)->action & TVE_COLLAPSE) {
							msgid = DBC_MSGCLOSE;
							tpos = treesearch(control->tree.root, ((LPNMTREEVIEW) lParam)->itemNew.hItem);
							tpos->expanded = FALSE;
						}
						else {
							msgid = DBC_MSGOPEN;
							tpos = treesearch(control->tree.root, ((LPNMTREEVIEW) lParam)->itemNew.hItem);
							tpos->expanded = TRUE;
						}
					}
					if (res->linemsgs) {
						cbmsgdata[0] = '\0';
						if (nmhdr->code == TVN_KEYDOWN) {
							tpos = treesearch(control->tree.root, control->tree.hitem);
						}
						else {
							if (tpos == NULL) { /* don't search again after TVN_ITEMEXPANDED or TVN_SELCHANGED code */
								tpos = treesearch(control->tree.root, ((LPNMTREEVIEW) lParam)->itemNew.hItem);
							}
						}
						if (tpos == NULL) break; /* should never happen */
						treeposition((CHAR *) cbmsgdata, control->tree.root, tpos);
						textlen = (INT)strlen((CHAR *) cbmsgdata);
						/* remove trailing comma */
						if (textlen > 0 && cbmsgdata[textlen - 1] == ',') cbmsgdata[--textlen] = 0;
						rescb(res, msgid, control->useritem, textlen);
					}
					else {
						if (nmhdr->code == TVN_ITEMEXPANDED) {
							/* use text from currently selected item */
							tvitem = ((LPNMTREEVIEW) lParam)->itemNew;
							tvitem.mask = TVIF_TEXT;
							tvitem.pszText = &xtext[0];
							tvitem.cchTextMax = TREEVIEW_ITEM_MAX_SIZE;
							TreeView_GetItem(control->ctlhandle, &tvitem);
							strcpy((CHAR *) cbmsgdata, xtext);
							rescb(res, msgid, control->useritem, (INT)strlen(xtext));
						}
						else {
							strcpy((CHAR *) cbmsgdata, control->tree.text);
							rescb(res, msgid, control->useritem, (INT)strlen(control->tree.text));
						}
					}
					break;
				}
			}
			pviend();
		}
		else if (nmhdr->code == (UINT)NM_CUSTOMDRAW) {
			control = (CONTROL *) GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
			if (control == NULL || control->type != PANEL_TREE) break;
			return handleTree_CustomDraw(control, (LPNMTVCUSTOMDRAW)lParam);
			}
		else if (nmhdr->code == TTN_NEEDTEXT) {
			currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			if (currentwinhandle == NULL) break;
			lpToolTipText = (LPTOOLTIPTEXT) lParam;
			pvistart();
			win = *currentwinhandle;
			if (win->showtoolbar != NULL) {
				res = *win->showtoolbar;
				control = (CONTROL *) res->content;
				for (iCurCtrl = res->entrycount; iCurCtrl; iCurCtrl--, control++) {
					if (control->useritem == LOWORD(lpToolTipText->hdr.idFrom) - ITEM_MSG_START) {
						lpToolTipText->lpszText = (LPSTR) control->tooltip;
						break;
					}
				}
			}
			pviend();
		}
		break;

	HANDLE_MSG(hwnd, WM_DRAWITEM, WinProc_OnDrawItem); /* This function is in guiawin3 */
	HANDLE_MSG(hwnd, WM_MEASUREITEM, WinProc_OnMeasureItem); /* This function is in guiawin3 */

	case WM_MOUSEACTIVATE:  /* a Windows check to see if a mouse click can activate a window */
		if (commonDlgFlag) {
			MessageBeep(MB_OK);
			return (MA_NOACTIVATEANDEAT);
		}
		if (dlgreshandlelisthdr != NULL) {
			currentwinhandle =  (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			pvistart();
			res = *dlgreshandlelisthdr;
			if (currentwinhandle != NULL) {
				win = *currentwinhandle;
				if (win->debugflg && res->winshow != NULL) {
					win = *res->winshow;
					if (!win->debugflg) {
						/* active dialog was not shown in debugger window */
						pviend();
						break;
					}
				}
			}
			hwndDlg = res->hwnd;
			if (res->ctlfocus != NULL) hwndCtrl = res->ctlfocus->ctlhandle;
			else hwndCtrl = NULL;
			pviend();
			SetActiveWindow(hwndDlg);
			if (hwndCtrl != NULL) SetFocus(hwndCtrl);
			return(MA_NOACTIVATEANDEAT);
		}
		break;
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE) {
			hwndfocuswin = NULL;
			haccelfocus = NULL;
			break;
		}
		if (dlgreshandlelisthdr != NULL) {
			currentwinhandle =  (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			pvistart();
			res = *dlgreshandlelisthdr;
			if (currentwinhandle != NULL) {
				win = *currentwinhandle;
				if (win->debugflg && res->winshow != NULL) {
					win = *res->winshow;
					if (!win->debugflg) {
						/* active dialog was not shown in debugger window */
						pviend();
						break;
					}
				}
			}
			hwndDlg = res->hwnd;
			if (res->ctlfocus != NULL) hwndCtrl = res->ctlfocus->ctlhandle;
			else hwndCtrl = NULL;
			pviend();

			SetActiveWindow(hwndDlg);
			if (hwndCtrl != NULL) SetFocus(hwndCtrl);
			return 0;
		}
		currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (currentwinhandle == NULL) break;
		haccelfocus = NULL;
		pvistart();
		win = *currentwinhandle;
		currentreshandle = win->showmenu;
		if (currentreshandle != NULL) haccelfocus = (*currentreshandle)->hacAccelHandle;
		if (nowactmsghwnd != hwnd && !win->changeflag) {
			wincb(win, DBC_MSGWINACTIVATE, 0);
		}
		nowactmsghwnd = NULL;
		pviend();
		break;

	case WM_ACTIVATEAPP:
		if (dlgreshandlelisthdr != NULL && wParam) {  /* is there a modal dialog active? */
			currentwinhandle =  (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			pvistart();
			res = *dlgreshandlelisthdr;
			if (currentwinhandle != NULL) {
				win = *currentwinhandle;
				if (win->debugflg && res->winshow != NULL) {
					win = *res->winshow;
					if (!win->debugflg) {
						/* active dialog was not shown in debugger window */
						pviend();
						break;
					}
				}
			}
			hwndDlg = res->hwnd;
			if (res->ctlfocus != NULL) hwndCtrl = res->ctlfocus->ctlhandle;
			else hwndCtrl = NULL;
			pviend();

			SetActiveWindow(hwndDlg);
			if (hwndCtrl != NULL) SetFocus(hwndCtrl);
			checkFloatWindowVisibility(hwnd, wParam);
			return 0;   // <---- !

		}
		else {
			hwndfocuswin = NULL;
			haccelfocus = NULL;
		}
		checkFloatWindowVisibility(hwnd, wParam);
		break;

	case WM_NCMOUSEMOVE:
		currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (currentwinhandle == NULL) break;
		pvistart();
		win = *currentwinhandle;
		if (hwnd != GetActiveWindow()) {
			pviend();
			return 0;
		}
		if (win->mousemsgs && win->cbfnc != NULL) {
			/* POSB, POST, POSL, and POSR messages */
			if (!outmsgsent) {
				outmsgsent = TRUE;
				if (wParam == HTBORDER && !(GetWindowLongPtr(hwnd, GWL_STYLE) & WS_MAXIMIZEBOX)) {
					/* FIXSIZE windows only generate HTCAPTION and HTBORDER messages */
					mousepos = MAKEPOINTS(lParam); /* x, y */
					GetWindowRect(hwnd, &rect);
					if (mousepos.x <= rect.left + 1) wincb(win, DBC_MSGPOSLEFT, 0);
					else if (mousepos.x >= rect.right - 1) wincb(win, DBC_MSGPOSRIGHT, 0);
					else wincb(win, DBC_MSGPOSBTM, 0);
				}
				else if (wParam == HTBOTTOM || wParam == HTBOTTOMLEFT || wParam == HTBOTTOMRIGHT) {
					wincb(win, DBC_MSGPOSBTM, 0);
				}
				else if (wParam == HTRIGHT) {
					wincb(win, DBC_MSGPOSRIGHT, 0);
				}
				else if (wParam == HTLEFT) {
					wincb(win, DBC_MSGPOSLEFT, 0);
				}
				else if (wParam == HTCAPTION || wParam == HTTOP || wParam == HTTOPLEFT || wParam == HTTOPRIGHT) {
					wincb(win, DBC_MSGPOSTOP, 0);
				}
				else outmsgsent = FALSE;
			}
		}
		pviend();
		break;
	case WM_MOUSEMOVE:
		currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (currentwinhandle == NULL) break;
		pvistart();
		win = *currentwinhandle;
		if (hwnd != GetActiveWindow()) {
			pviend();
			return 0;
		}
		if (win->mousemsgs && win->cbfnc != NULL) {
			mousepos = MAKEPOINTS(lParam);
			mousepos.y -= win->bordertop;
			itonum5(mousepos.x += win->scrollx, cbmsgdata);
			itonum5(mousepos.y += win->scrolly, cbmsgdata + 5);
			wincb(win, DBC_MSGPOSITION, 10);
			outmsgsent = FALSE;
		}
		pviend();
		break;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
		currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (currentwinhandle == NULL || dlgreshandlelisthdr != NULL) break;
		pvistart();
		win = *currentwinhandle;
		if (win->cbfnc == NULL) {
			ReleaseCapture();
			pviend();
			break;
		}
		mousepos = MAKEPOINTS(lParam);
		mousepos.y -= win->bordertop;

		/* If the position reported is out of the client area */
		/* 'normalize' it to the client area. */
		if (mousepos.x < 0) mousepos.x = 0;
		else if (mousepos.x > win->clientx) mousepos.x = win->clientx;
		if (mousepos.y < 0) mousepos.y = 0;
		else if (mousepos.y > win->clienty) mousepos.y = win->clienty;

		if (nMessage == WM_LBUTTONUP || nMessage == WM_LBUTTONDOWN || nMessage == WM_LBUTTONDBLCLK) cbmsgfunc[0] = 'L';
		else if (nMessage == WM_MBUTTONUP || nMessage == WM_MBUTTONDOWN || nMessage == WM_MBUTTONDBLCLK) cbmsgfunc[0] = 'M';
		else cbmsgfunc[0] = 'R';
		cbmsgfunc[1] = 'B';
		cbmsgfunc[2] = 'D';
		if (nMessage == WM_LBUTTONDOWN || nMessage == WM_MBUTTONDOWN || nMessage == WM_RBUTTONDOWN) cbmsgfunc[3] = 'N';
		else if (nMessage == WM_LBUTTONDBLCLK || nMessage == WM_MBUTTONDBLCLK || nMessage == WM_RBUTTONDBLCLK) cbmsgfunc[3] = 'C';
		else {
			cbmsgfunc[2] = 'U';
			cbmsgfunc[3] = 'P';
		}
		mousepos.x += win->scrollx;
		mousepos.y += win->scrolly;
		itonum5(mousepos.x, cbmsgdata);
		itonum5(mousepos.y, cbmsgdata + 5);
		if (wParam & MK_CONTROL) {
			memcpy(cbmsgdata + 10, "CTRL", 4);
			i1 = 14;
		}
		else if (wParam & MK_SHIFT) {
			memcpy(cbmsgdata + 10, "SHIFT", 5);
			i1 = 15;
		}
		else i1 = 10;
		wincb(win, NULL, i1);
		if (nMessage == WM_LBUTTONDOWN || nMessage == WM_MBUTTONDOWN || nMessage == WM_RBUTTONDOWN) {
			SetCapture(win->hwnd);
		}
		else {
			ReleaseCapture();
		}
		pviend();
		break;
	case WM_ENTERMENULOOP:
		bIsPopupMenuActive = (BOOL)wParam;
		hPopupMenu = NULL;
		break;
	case WM_EXITMENULOOP:
		if (bIsPopupMenuActive) {
			DestroyMenu(hPopupMenu);
#if 0
			hPopupMenu = NULL;
#endif
		}
		bIsPopupMenuActive = FALSE;
		break;

	case WM_INITMENU:
		if (bIsPopupMenuActive) hPopupMenu = (HMENU)wParam;
		break;

	case WM_COMMAND:
		if (checkSingleLineEditFocusNotification(wParam, lParam))
			return(DefWindowProc(hwnd, nMessage, wParam, lParam));
		if (HIWORD(wParam))	hPopupMenu = NULL;
		currentwinhandle =  (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (currentwinhandle == NULL) break;
		pvistart();
		win = *currentwinhandle;
		if (lParam == (LPARAM) NULL) {	/* MENU or ACCELERATOR command */
			if (hPopupMenu != NULL && win->showpopupmenu != NULL) {
				res = *win->showpopupmenu;
				if (res->hmenu != hPopupMenu) res = *win->showmenu;
			}
			else res = *win->showmenu;
			if (res != NULL && res->winshow != NULL && res->cbfnc != NULL)
				rescb(res, DBC_MSGMENU, LOWORD(wParam), 0);
			pviend();
			return 0;
		}

		/* control notification message */

		/* is it possibly a toolbar message? */
		if (win->showtoolbar != NULL && (*win->showtoolbar)->hwndtoolbar == (HWND)lParam) {
			res = *win->showtoolbar;
			control = (CONTROL *) res->content;
			iCurCtrl = res->entrycount;
			while (iCurCtrl--) {
				if (control->useritem == LOWORD(wParam) - ITEM_MSG_START) {
					if (res->winshow == NULL || control->changeflag) {
						pviend();
						return 0;
					}
					if (ISLOCKBUTTON(control)) control->style ^= CTRLSTYLE_MARKED;
					rc = guiaResCallBack(res, control, HIWORD(wParam));
					pviend();
					if (!rc) return 0;
					return(DefWindowProc(hwnd, nMessage, wParam, lParam));
				}
				control++;
			}
		}

		if (win->showreshdr == NULL && win->showtoolbar == NULL) {
			pviend();
			return(DefWindowProc(hwnd, nMessage, wParam, lParam));
		}

		/* see if it is a panel resource */
		if (win->showreshdr != NULL) {
			res = *win->showreshdr;
			while (res != NULL) {
				control = (CONTROL *) res->content;
				for (iCurCtrl = res->entrycount; iCurCtrl; iCurCtrl--, control++) {
					if (control->useritem == LOWORD(wParam) - ITEM_MSG_START && control->ctlhandle == (HWND) lParam)  {
						if (res->winshow == NULL || control->changeflag) {
							pviend();
							return 0;
						}
						if (ISLOCKBUTTON(control)) control->style ^= CTRLSTYLE_MARKED;
						else if (ISCHECKTYPE(control) && HIWORD(wParam) == BN_CLICKED) {
							res->ctlfocus = control;
							if (ISCHECKBOX(control)) {
								handleCheckboxStateChange(control, (HWND)lParam);
							}
							else if (!radioButtonSuppressAutoCheck) {
								control->style |= CTRLSTYLE_MARKED;
							}
							if (ISRADIO(control) && !radioButtonSuppressAutoCheck){
								SendMessage(control->ctlhandle, BM_SETCHECK, BST_CHECKED, 0);
								pctlBase = (CONTROL *) res->content;
								iCtlsave = iCtlNum = control->ctloffset;
								while(pctlBase[iCtlNum].type != PANEL_LASTRADIOBUTTON) {
									iCtlNum++;
								}
								for(;;) {
									if (iCtlNum != iCtlsave) {
										pctlBase[iCtlNum].style &= ~CTRLSTYLE_MARKED;
										SendMessage(pctlBase[iCtlNum].ctlhandle, BM_SETCHECK, BST_UNCHECKED, 0);
									}
									if (pctlBase[iCtlNum].value1 == 1) break;
									iCtlNum--;
								}
							}
						}
						rc = guiaResCallBack(res, control, HIWORD(wParam));
						pviend();
						return(DefWindowProc(hwnd, nMessage, wParam, lParam));
					}
				}
				if (res->showresnext == NULL) break;
				res = *res->showresnext;
			}
		}

		/* see if it is a combo inside a toolbar */
		if (win->showtoolbar != NULL) {
			res = *win->showtoolbar;
			while (res != NULL) {
				control = (CONTROL *) res->content;
				iCurCtrl = res->entrycount;
				while(iCurCtrl--) {
					if (control->ctlhandle == (HWND) lParam) {
						if (ISLISTBOX(control)) {
							rc = guiaResCallBack(res, control, HIWORD(wParam));
							pviend();
							return(DefWindowProc(hwnd, nMessage, wParam, lParam));
						}
					}
					control++;
				}
				if (res->showresnext == NULL) break;
				res = *res->showresnext;
			}
		}

		pviend();
		break;
	case WM_VSCROLL:
	case WM_HSCROLL:
		if ((HWND) lParam != NULL) {
			currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			if (currentwinhandle == NULL) break;
			pvistart();
			win = *currentwinhandle;
			res = *win->showreshdr;
			while (res != NULL) {
				control = (CONTROL *) res->content;
				iCurCtrl = res->entrycount;
				while(iCurCtrl--) {
					if (control->ctlhandle == (HWND) lParam)  {
						if (res->winshow == NULL || control->changeflag) {
						}
						else guiaScrollCallBack(res, control, wParam);
						pviend();
						return 0;
					}
					control++;
				}
				if (res->showresnext == NULL) break;
				res = *res->showresnext;
			}
			pviend();
		}
		else {  /* window scroll message */
			currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			if (currentwinhandle == NULL) break;
			pvistart();
			win = *currentwinhandle;
			if (win->autoscroll) {
				guiaProcessAutoScrollBars(win, nMessage, wParam);
			}
			else {
				guiaProcessWinScrollBars(win, nMessage, wParam);
			}
			pviend();
		}
		return 0;

	/*
	 * The following two cases are here only to deal with Windows flakeyness that
	 * happens on some XP and Vista machines, some of the time.
	 * We are only interested in these two messages when they are sent to
	 * one of our special taskbar windows.
	 * Windows fails to send the expected WM_SYSCOMMAND/SC_RESTORE to the window
	 * when the user clicks on it to request a restore. Instead (or in addition??) it
	 * sends these. We use them to prevent the taskbar window from being restored,
	 * and to trick ourselves into the 'normal' code.
	 */
//	case WM_WINDOWPOSCHANGING:
//		if (restoreKludge) {
//			currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
//			if (currentwinhandle == NULL) break;
//			pvistart();
//			win = *currentwinhandle;
//			if (hwnd == win->hwndtaskbarbutton) {
//				pviend();
//				((WINDOWPOS *) lParam)->flags |= SWP_NOMOVE;
//				return 0;
//			}
//			pviend();
//		}
//		break;

//	case WM_WINDOWPOSCHANGED:
//		if (restoreKludge) {
//			currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
//			if (currentwinhandle == NULL) break;
//			pvistart();
//			win = *currentwinhandle;
//			if (hwnd == win->hwndtaskbarbutton) {
//				pviend();
//				PostMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
//				return 0;
//			}
//			pviend();
//		}
//		break;

	case WM_MOVE:
		currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (currentwinhandle == NULL) break;
		pvistart();
		win = *currentwinhandle;
		if (win != NULL && win->cbfnc != NULL && !win->changeflag) {
			GetWindowRect(hwnd, &rect);
			itonum5((signed short) rect.left, cbmsgdata);
			itonum5((signed short) rect.top, cbmsgdata + 5);
			wincb(win, DBC_MSGWINPOS, 10);
		}
		pviend();
		break;
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED) {
			pvistart();
			pviend();
		}
		else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED) {
			currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			if (currentwinhandle == NULL) break;
			GetClientRect(hwnd, &clientrect);
			pvistart();
			win = *currentwinhandle;
			win->clientx = clientrect.right;
			win->clienty = clientrect.bottom - win->bordertop;
			if (win->autoscroll) {
				if (mainWindowsThread == GetCurrentThreadId()) checkScrollBars(currentwinhandle);
				else guiaCheckScrollBars(currentwinhandle);
			}
			/* Could get scrollbar size with SM_CYHSCROLL, SM_CXVSCROLL and compensate*/
			if (win->showtoolbar != NULL) {
				res = *win->showtoolbar;
				GetWindowRect(res->hwndtoolbar, &rect);
				iWinWidth = rect.right - rect.left;
				if (iWinWidth != clientrect.right) {
					iWinHeight = rect.bottom - rect.top;
					SetWindowPos(res->hwndtoolbar, NULL, 0, 0, clientrect.right, iWinHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
				}
			}
			if (win->statusbarshown) {
/*				SetWindowPos(win->hwndstatusbar, NULL, -100, -100, 10, 10, SWP_NOZORDER | SWP_NOACTIVATE); */
				PostMessage(win->hwndstatusbar, WM_SIZE, wParam, lParam);
				/* subtract height of statusbar from client area height */
				GetClientRect(win->hwndstatusbar, &clientrect);
				win->clienty -= clientrect.bottom - clientrect.top;
				if (win->clienty < 0) win->clienty = 0;
			}
			if (!win->changeflag) {
				itonum5(win->clientx, cbmsgdata);
				itonum5(win->clienty, cbmsgdata + 5);
				wincb(win, DBC_MSGWINSIZE, 10);
			}
			pviend();
		}
		break;

	case WM_ERASEBKGND:
		switch (erasebkgndSpecial) {
			case EBPS_DEFAULT:
				if (IsOS(OS_TERMINALCLIENT)) {
					GetClientRect(hwnd, &clientrect);
					FillRect((HDC)wParam, &clientrect, hbrButtonFace);
				}
				break;
			case EBPS_EARLY:
				GetClientRect(hwnd, &clientrect);
				FillRect((HDC)wParam, &clientrect, hbrButtonFace);
				break;
			case EBPS_LATE:
				/* do nothing here */
				break;
		}
		return 0;

	case WM_PAINT:
		if (!GetUpdateRect(hwnd, NULL, FALSE)) return 0;
		currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (currentwinhandle == NULL || !isValidHandle((UCHAR**)currentwinhandle)) {
			break;
		}
		hPaintDC = BeginPaint(hwnd, &ps);
		if (hPaintDC == NULL) {
			break;
		}
		if (ps.fErase) {
			eraseWindowBackground(hwnd, &ps);
		}
		win = *currentwinhandle;
		if (win->bordertop) {  /* toolbar is shown */
			hpen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_3DSHADOW));
			hpenold = SelectObject(hPaintDC, hpen);
			MoveToEx(hPaintDC, 0, win->bordertop - 2, NULL);
			LineTo(hPaintDC, 2400, win->bordertop - 2);
			hpen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_3DHILIGHT));
			DeleteObject(SelectObject(hPaintDC, hpen));
			MoveToEx(hPaintDC, 0, win->bordertop - 1, NULL);
			LineTo(hPaintDC, 2400, win->bordertop - 1);
			DeleteObject(SelectObject(hPaintDC, hpenold));
		}
		EndPaint(hwnd, &ps);
		break;

	case WM_KILLFOCUS:
		currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (currentwinhandle == NULL) break;
		pvistart();
		win = *currentwinhandle;
		if (win->caretcreatedflg) {
			DestroyCaret();
			win->caretcreatedflg = FALSE;
		}
		pviend();
		break;

	case WM_SETFOCUS:
		hwndfocuswin = hwnd;
		currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (currentwinhandle == NULL) break;
		hwndCtrl = NULL;
		pvistart();
		win = *currentwinhandle;
		currentreshandle = win->showreshdr;
		while (currentreshandle != NULL) {
			res = *currentreshandle;
			if (res->ctlfocus != NULL) {
				hwndCtrl = res->ctlfocus->ctlhandle;
//				if (ISEDIT(res->ctlfocus)) {
//					if (res->ctlfocus->editinsertpos < 0)
//						res->ctlfocus->editinsertpos = 0;
//					if ((res->ctlfocus->editinsertpos == 0) && (res->ctlfocus->editendpos <= 0))
//						res->ctlfocus->editendpos = -1;
//					SendMessage(hwndCtrl, EM_SETSEL, res->ctlfocus->editinsertpos, res->ctlfocus->editendpos);
//				}
				break;
			}
			currentreshandle = res->showresnext;
		}
		if (win->caretonflg && !win->caretcreatedflg) {
			CreateCaret(win->hwnd, NULL, 2, win->caretvsize);
			SetCaretPos(win->caretpos.x, win->bordertop + win->caretpos.y);
			ShowCaret(win->hwnd);
			win->caretcreatedflg = TRUE;
		}
		pviend();
		if (hwndCtrl != NULL) {
			SetFocus(hwndCtrl);
		}
		break;
	case WM_SYSCOMMAND:
		//if (wParam == SC_RESTORE) break;
		if (wParam == SC_CLOSE || wParam == SC_MINIMIZE) {
			currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			if (currentwinhandle == NULL) {
				break;
			}
			pvistart();
			win = *currentwinhandle;
			if (wParam == SC_CLOSE) {
				if (win && win->cbfnc) {
					wincb(win, DBC_MSGCLOSE, 0);
					pviend();
					return 0;
				}
			}
			/*
			 * Seems to me that we should send the WMIN message even if the user
			 * has set notaskbarbutton
			 */
			else if (wParam == SC_MINIMIZE /*&& win->taskbarbutton*/) {
				/* WMIN must go into queue before hide (for situation where there are multiple windows) */
				if (!win->changeflag) wincb(win, DBC_MSGWINMINIMIZE, 0);
			}
			pviend();
		}
		break;
	case WM_CLOSE:
		if (hwnd == hwndmainwin) {
			if (!breakflag) {
				breakflag = TRUE;
				evtset(breakeventid);
				PostQuitMessage(0);
			}
			return 0;
		}
		currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (currentwinhandle == NULL) break;
		pvistart();
		win = *currentwinhandle;
		if (win != NULL && win->cbfnc != NULL) wincb(win, DBC_MSGCLOSE, 0);
		pviend();
		//return(DefWindowProc(hwnd, nMessage, wParam, lParam));
		break;

	case WM_SYSCHAR:
	case WM_CHAR:
		if (VK_BACK == wParam || VK_TAB == wParam || VK_RETURN == wParam || VK_ESCAPE == wParam) return 0;
		currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (currentwinhandle == NULL) break;
		pvistart();
		win = *currentwinhandle;
		if (win && win->cbfnc && win->showreshdr == NULL) {
			sRepeatCnt = (SHORT) lParam;
			ucDataChar = toupper((BYTE) wParam);
			if (lParam & 0x20000000 && ucDataChar >= 'A' && ucDataChar <= 'Z') { 	/* ALT key is pressed */
				usCallbackChar = ucDataChar - 'A' + KEYALTA;
				itonum5((INT) usCallbackChar, cbmsgdata);
				for (sNextRepeat = 0; sNextRepeat < sRepeatCnt; sNextRepeat++) wincb(win, DBC_MSGWINNKEY, 5);
			}
			else {
				cbmsgdata[0] = (BYTE) wParam;
				for (sNextRepeat = 0; sNextRepeat < sRepeatCnt; sNextRepeat++) wincb(win, DBC_MSGWINCHAR, 1);
			}
			if (win->showmenu == NULL) {  /* no default processing in this case */
				pviend();
				return 0;
			}
		}
		pviend();
		break;

	case WM_KEYDOWN:
		currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (currentwinhandle == NULL) break;
		if (wParam < VK_F20 + 1) {
			if ((usCallbackChar = translatekeycode[wParam]) > 0) {
				if (usCallbackChar >= KEYF1 && usCallbackChar <= KEYF20) {
					if (GetKeyState(VK_CONTROL) & 0x8000) {		/* CONTROL is pressed */
						usCallbackChar += 40;
					}
					else if (GetKeyState(VK_SHIFT) & 0x8000) {	/* SHIFT is pressed */
						usCallbackChar += 20;
					}
					else if (GetKeyState(VK_MENU) & 0x8000) {	/* ALT is pressed */
						usCallbackChar += 60;
					}
				}
				else if (usCallbackChar >= KEYUP && usCallbackChar <= KEYPGDN) {
					if (GetKeyState(VK_CONTROL) & 0x8000) {		/* CONTROL is pressed */
						usCallbackChar += 20;
					}
					else if (GetKeyState(VK_SHIFT) & 0x8000) {	/* SHIFT is pressed */
						usCallbackChar += 10;
					}
					else if (GetKeyState(VK_MENU) & 0x8000) {	/* ALT is pressed */
						usCallbackChar += 30;
					}
				}
				else if (usCallbackChar == KEYTAB) {
					if (GetKeyState(VK_SHIFT) & 0x8000) usCallbackChar++; /* SHIFT is pressed */
				}
				pvistart();
				win = *currentwinhandle;
				if (win != NULL && win->cbfnc != NULL && ((win->showreshdr == NULL)
						|| (win->showreshdr != NULL && !doesWindowHaveAnyFocusableControls(currentwinhandle))))
				{
					itonum5((INT) usCallbackChar, cbmsgdata);
					if (wParam != VK_UP && wParam != VK_DOWN) {	/* kludge for FDE EDIT */
						sRepeatCnt = (SHORT) lParam;
						for (sNextRepeat = 0; sNextRepeat < sRepeatCnt; sNextRepeat++) wincb(win, DBC_MSGWINNKEY, 5);
					}
					else wincb(win, DBC_MSGWINNKEY, 5);
				}
				pviend();
			}
		}
		break;
	case WM_QUERYENDSESSION:
		if (!endsessionflag) {
			MessageBeep(MB_ICONWARNING);
			iCtlNum = MessageBox(NULL,
				(LPCSTR)"Windows is shutting down while DB/C is\n still running, continue exit of Windows?",
				(LPCSTR)pgmname, MB_ICONWARNING | MB_SYSTEMMODAL | MB_YESNO);
			if (IDNO == iCtlNum) return 0;
			endsessionflag = TRUE;
		}
		return 1;
	case WM_ENDSESSION:
		if (wParam) {
			breakflag = TRUE;
			evtset(breakeventid);
			while (!guiaThreadStopTest());
		}
		else endsessionflag = FALSE;
		return 0;

	}

	return(DefWindowProc(hwnd, nMessage, wParam, lParam));
}

static BOOL doesWindowHaveAnyFocusableControls(WINHANDLE winhandle) {
	WINDOW *win = *winhandle;
	RESHANDLE rh;
	CONTROL *control;
	INT i1;

	for (rh = win->showreshdr; rh != NULL; rh = (*rh)->showresnext) {
		for (control = (CONTROL *) (*rh)->content, i1 = (*rh)->entrycount; i1; i1--, control++) {
			if (ISGRAY(control) || ISSHOWONLY(control) || !ISINTERACTIVE(control) || ISNOFOCUS(control)) {
				continue;
			}
			if (ISRADIO(control) && !(ISMARKED(control))) continue;
			if (control->type == PANEL_TABLE && !guiaIsTableFocusable(control)) continue;
			return TRUE;
		}
	}
	return FALSE;
}


/**
 * If I am a floatwindow and not owned, and if we are being deactivated, hide me, otherwise
 * show me.
 */
static void checkFloatWindowVisibility(HWND hwnd, WPARAM wParam) {
	UINT uFlags;
	WINDOW *win;
	WINHANDLE currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (currentwinhandle == NULL) return;
	pvistart();
	win = *currentwinhandle;
	if (win->floatwindow && win->ownerhwnd == NULL) {
		if ((IsWindowVisible(win->hwnd) && !wParam) || (!IsWindowVisible(win->hwnd) && wParam)) {
			uFlags = SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
			uFlags |= wParam ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;
			SetWindowPos(win->hwnd, NULL, 0, 0, 0, 0, uFlags);
		}
	}
	pviend();
}

/**
 * Called during WM_PAINT processing.
 * Called only from eraseWindowBackground(HWND hwnd, PAINTSTRUCT *ps)
 */
static void eraseWindowBackground_EX(HDC hPaintDC, PAINTSTRUCT *ps) {
	switch (erasebkgndSpecial) {
		case EBPS_DEFAULT:
			if (!IsOS(OS_TERMINALCLIENT)) {
				FillRect(hPaintDC, &ps->rcPaint, hbrButtonFace);
			}
			break;
		case EBPS_EARLY:
			/* do nothing here */
			break;
		case EBPS_LATE:
			FillRect(hPaintDC, &ps->rcPaint, hbrButtonFace);
			break;
	}
}

/**
 * Called during WM_PAINT processing
 */
static void eraseWindowBackground(HWND hwnd, PAINTSTRUCT *ps) {
	WINDOW *win;
	WINHANDLE currentwinhandle = (WINHANDLE) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	HDC hdc;
	HPALETTE hOldBitPal, hOldPal;
	PIXHANDLE nextpixhandle;
	PIXMAP *pix;
	INT imgCount = 0;
	HDC hPaintDC = ps->hdc;
	RECT oneImageRect;

	if (currentwinhandle == NULL || !isValidHandle((UCHAR**)currentwinhandle)) {
		SetEvent(hPaintingEvent); // Set to signaled state
		return;
	}
	pvistart();
	win = *currentwinhandle;
	if (win->showpixhdr == NULL || !isValidHandle((UCHAR**)win->showpixhdr)) {
		pviend();
		/*
		 * Zero images are on the screen, erase the background, maybe
		 */
		eraseWindowBackground_EX(hPaintDC, ps);
		SetEvent(hPaintingEvent); // Set to signaled state
		return;
	}
	hdc = CreateCompatibleDC(hPaintDC);
	if (hdc != NULL) {
		DWORD dwWaitResult;
		RECT dummyRect;
		/*
		 * Preliminary loop to count the number of images currently being shown
		 * that have at least part of them in the invalid rect
		 */
		for (nextpixhandle = win->showpixhdr; nextpixhandle != NULL; nextpixhandle = pix->showpixnext) {
			pix = *nextpixhandle;
			oneImageRect.top = pix->vshow - win->scrolly + win->bordertop;
			oneImageRect.bottom = oneImageRect.top + pix->vsize;
			oneImageRect.left = pix->hshow - win->scrollx;
			oneImageRect.right = oneImageRect.left + pix->hsize;
			if (IntersectRect(&dummyRect, &oneImageRect, &ps->rcPaint)) imgCount++;
		}

		if (imgCount == 1) {
			/*
			 * Special deal here for just one image, this is for Jon E.
			 * If the one image to be painted and the invalid rect are the same, do nothing.
			 */
			IntersectRect(&dummyRect, &oneImageRect, &ps->rcPaint);
			if (!EqualRect(&dummyRect, &ps->rcPaint)) {
				eraseWindowBackground_EX(hPaintDC, ps);
			}
		}
		else {
			/**
			 * If more than one image is shown, don't try to be fancy, just start by erasing
			 * the invalid rect.
			 */
			eraseWindowBackground_EX(hPaintDC, ps);
		}

		hOldBitPal = hOldPal = NULL;
		for (nextpixhandle = win->showpixhdr; nextpixhandle != NULL; nextpixhandle = pix->showpixnext) {
			pix = *nextpixhandle;
			oneImageRect.top = pix->vshow - win->scrolly + win->bordertop;
			oneImageRect.bottom = oneImageRect.top + pix->vsize;
			oneImageRect.left = pix->hshow - win->scrollx;
			oneImageRect.right = oneImageRect.left + pix->hsize;
			if (!IntersectRect(&dummyRect, &oneImageRect, &ps->rcPaint)) continue;

			dwWaitResult = WaitForSingleObject(pix->hPaintingMutex, 5);
			if (dwWaitResult != WAIT_OBJECT_0) {
				continue;
			}
			SelectObject(hdc, pix->hbitmap);
			if (pix->hpal != NULL) {
				if (hOldBitPal == NULL) hOldBitPal = SelectPalette(hdc, pix->hpal, TRUE);
				else SelectPalette(hdc, pix->hpal, TRUE);
				if (hOldPal == NULL) hOldPal = SelectPalette(hPaintDC, pix->hpal, TRUE);
				else SelectPalette(hPaintDC, pix->hpal, TRUE);
				RealizePalette(hPaintDC);
			}
			if (BitBlt(hPaintDC, pix->hshow - win->scrollx,
				pix->vshow - win->scrolly + win->bordertop,
				pix->hsize, pix->vsize, hdc, 0, 0, SRCCOPY) == 0
				&& !bitbltErrorIgnore)
			{
				guiaErrorMsg("BitBlt failure", GetLastError());
			}
			ReleaseMutex(pix->hPaintingMutex);
		}
		if (hOldBitPal != NULL) SelectPalette(hdc, hOldBitPal, TRUE);
		if (hOldPal != NULL) {
			SelectPalette(hPaintDC, hOldPal, TRUE);
			RealizePalette(hPaintDC);
		}
		DeleteDC(hdc);
	}
	pviend();
	SetEvent(hPaintingEvent); // Set to signaled state
	return;
}

static void clearLastErrorMessage() {
	lastErrorMessage[0] = '\0';
}

static void setLastErrorMessage (CHAR *msg) {
	strcpy(lastErrorMessage, msg);
}

CHAR* guiaGetLastErrorMessage () {
	return lastErrorMessage;
}
