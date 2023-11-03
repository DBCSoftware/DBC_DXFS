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

/* operating system dependent includes, typedefs and defines */

#if OS_WIN32GUI
#include <commctrl.h>
#include <shlobj.h>
#endif


/* define control style definitions */

#define CTRLSTYLE_DISABLED 			0x001
#define CTRLSTYLE_SHOWONLY			0x002
#define CTRLSTYLE_MARKED 			0x004
#define CTRLSTYLE_INSERTORDER		0x008
#define CTRLSTYLE_APPEND 			0x010
#define CTRLSTYLE_READONLY			0x020
#define CTRLSTYLE_NOFOCUS			0x040
#define CTRLSTYLE_LEFTJUSTIFIED		0x080 /* for edit fields */
#define CTRLSTYLE_RIGHTJUSTIFIED	0x100 /* for edit fields */
#define CTRLSTYLE_CENTERJUSTIFIED	0x200 /* for edit fields */

#define LISTSTYLE_UNSPECIFIED		0x000 /* must be 0 */
#define LISTSTYLE_PLAIN				0x001
#define LISTSTYLE_BOLD				0x002
#define LISTSTYLE_ITALIC			0x004 /* also cannot have styles > 0x0FF */
#define LISTSTYLE_FONTMASK			0x007 /* lower three bits of byte used for font styles */
#define LISTSTYLE_SELECTED          0x080 /* Used only to preserve selected state of MLISTBOX */
										  /* items when it is on a tab */

/* define control style definitions */

#define MENUSTYLE_DISABLED 	0x01
#define MENUSTYLE_MARKED 	0x02
#define MENUSTYLE_LINE 		0x04
#define MENUSTYLE_HIDDEN	0x08
#define MENUSTYLE_SUBMENU	0x10

#define TREE_POS_INSERT 0
#define TREE_POS_ROOT 0

/* define keywords */
#define GUI_ITEMON				1
#define GUI_ITEMOFF				2
#define GUI_MARK				3
#define GUI_UNMARK				4
#define GUI_AVAILABLE			5
#define GUI_GRAY				6
#define GUI_SHOWONLY			7
#define GUI_TEXT				8
#define GUI_FOCUS				9
#define GUI_ERASE				10
#define GUI_DELETE				11
#define GUI_INSERT				12
#define GUI_LOCATE				13
#define GUI_RANGE				14
#define GUI_POSITION			15
#define GUI_STATUS				16
#define GUI_AVAILABLEALL		17
#define GUI_GRAYALL				18
#define GUI_JUSTIFYLEFT			19
#define GUI_JUSTIFYCENTER		20
#define GUI_JUSTIFYRIGHT		21
#define GUI_POINTER				22
#define GUI_INSERTAFTER			23
#define GUI_INSERTBEFORE		24
#define GUI_ICON				25
#define GUI_ADDPUSHBUTTON		26
#define GUI_ADDLOCKBUTTON		27
#define GUI_ADDSPACE 			28
#define GUI_DELETELINE			29
#define GUI_LOCATELINE			30
#define GUI_INSERTLINEAFTER		31
#define GUI_INSERTLINEBEFORE	32
#define GUI_FOCUSON				33
#define GUI_FOCUSOFF			34
#define GUI_HIDE				35
#define GUI_SHOW				36
#define GUI_LINEON				37
#define GUI_LINEOFF				38
#define GUI_FILENAME			39
#define GUI_FONTNAME			40
#define GUI_COLOR				41
#define GUI_DEFPUSHBUTTON		42
#define GUI_ESCPUSHBUTTON		43
#define GUI_PUSHBUTTON			44
#define GUI_SETMAXCHARS			45
#define GUI_TEXTCOLOR			46
#define GUI_READONLY			47
#define GUI_PASTE				48
#define GUI_TITLE				49
#define GUI_NOFOCUS				50
#define GUI_DESELECTLINE		51
#define GUI_SELECTLINE			52
#define GUI_DESELECT			53
#define GUI_SELECT				54
#define GUI_SELECTALL			55
#define GUI_DESELECTALL			56
#define GUI_RIGHTCLICKON		57
#define GUI_RIGHTCLICKOFF		58
#define GUI_TEXTCOLORLINE		59
#define GUI_TEXTCOLORDATA		60
#define GUI_ROOTLEVEL			61
#define GUI_UPLEVEL				62
#define GUI_DOWNLEVEL			63
#define GUI_SETLINE				64
#define GUI_TEXTSTYLELINE		65
#define GUI_TEXTSTYLEDATA		66
#define GUI_EXPAND				67
#define GUI_COLLAPSE			68
#define GUI_MINSERT				69
#define GUI_FEDITMASK			70
#define GUI_HELPTEXT			71
#define GUI_ADDDROPBOX			72
#define GUI_REMOVECONTROL		73
#define GUI_REMOVESPACEBEFORE	74
#define GUI_REMOVESPACEAFTER	75
#define GUI_LOCATETAB			76
#define GUI_NAMEFILTER			77
#define GUI_ADDROW              78
#define GUI_DELETEROW           79
#define GUI_INSERTROWBEFORE     80
#define GUI_GETROWCOUNT         81
#define GUI_DELETEALLROWS       82
#define GUI_DIRNAME             83
#define GUI_SETSELECTEDLINE		84
#define GUI_TEXTFONT            85
#define GUI_TEXTBGCOLORLINE		86
#define GUI_TEXTBGCOLORDATA		87
#define GUI_TEXTBGCOLOR			88

/* other definitions */

#define GUI_DEFAULT_FONT_SIZE 9
#define ITEM_MSG_START 		128

#if OS_WIN32

#define ISLISTBOX(pctl) ((pctl)->type == PANEL_LISTBOX || \
	(pctl)->type == PANEL_MLISTBOX || \
	(pctl)->type == PANEL_MLISTBOXHS || \
	(pctl)->type == PANEL_LISTBOXHS || \
	(pctl)->type == PANEL_DROPBOX)
#define ISCOLUMNFOCUSABLE(t) ((t) == PANEL_CHECKBOX || (t) == PANEL_POPBOX || (t) == PANEL_EDIT \
	|| (t) == PANEL_DROPBOX || (t) == PANEL_CDROPBOX || (t) == PANEL_LEDIT || (t) == PANEL_FEDIT)
#define ISTABLECOLUMNTEXT(t) ((t) == PANEL_VTEXT || (t) == PANEL_RVTEXT || (t) == PANEL_EDIT \
	|| (t) == PANEL_POPBOX || (t) == PANEL_LEDIT || (t) == PANEL_FEDIT || (t) == PANEL_CVTEXT)
#define ISTABLECOLUMNDROPBOX(t) ((t) == PANEL_DROPBOX || (t) == PANEL_CDROPBOX)
#define ISCHECKBOX(pctl) ((pctl)->type == PANEL_CHECKBOX || \
	(pctl)->type == PANEL_LTCHECKBOX)

typedef struct {
	CHAR *name;
	INT value;
} GUISYMTAB;

static GUISYMTAB symtable[]
#ifdef __GNUC__
	__attribute__ ((unused))
#endif
	= {
	"ADDDROPBOX",			GUI_ADDDROPBOX,
	"ADDLOCKBUTTON",		GUI_ADDLOCKBUTTON,
	"ADDPUSHBUTTON",		GUI_ADDPUSHBUTTON,
	"ADDROW",				GUI_ADDROW,
	"ADDSPACE",				GUI_ADDSPACE,
	"AVAILABLE",			GUI_AVAILABLE,
	"AVAILABLEALL",			GUI_AVAILABLEALL,
	"COLLAPSE",				GUI_COLLAPSE,
	"COLOR",				GUI_COLOR,
	"DEFPUSHBUTTON",		GUI_DEFPUSHBUTTON,
	"DELETE",				GUI_DELETE,
	"DELETEALLROWS",		GUI_DELETEALLROWS,
	"DELETELINE",			GUI_DELETELINE,
	"DELETEROW",			GUI_DELETEROW,
	"DESELECT",				GUI_DESELECT,
	"DESELECTALL",			GUI_DESELECTALL,
	"DESELECTLINE",			GUI_DESELECTLINE,
	"DIRNAME",              GUI_DIRNAME,
	"DOWNLEVEL",			GUI_DOWNLEVEL,
	"ERASE",				GUI_ERASE,
	"ESCPUSHBUTTON",		GUI_ESCPUSHBUTTON,
	"EXPAND",				GUI_EXPAND,
	"FEDITMASK",			GUI_FEDITMASK,
	"FILENAME",				GUI_FILENAME,
	"FOCUS",				GUI_FOCUS,
	"FOCUSOFF",				GUI_FOCUSOFF,
	"FOCUSON",				GUI_FOCUSON,
	"FONTNAME",				GUI_FONTNAME,
	"GETROWCOUNT",          GUI_GETROWCOUNT,
	"GRAY",					GUI_GRAY,
	"GRAYALL",				GUI_GRAYALL,
	"HELPTEXT",				GUI_HELPTEXT,
	"HIDE",					GUI_HIDE,
	"ICON",					GUI_ICON,
	"INSERT",				GUI_INSERT,
	"INSERTAFTER",			GUI_INSERTAFTER,
	"INSERTBEFORE",			GUI_INSERTBEFORE,
	"INSERTLINEAFTER",		GUI_INSERTLINEAFTER,
	"INSERTLINEBEFORE",		GUI_INSERTLINEBEFORE,
	"INSERTROWBEFORE",		GUI_INSERTROWBEFORE,
	"ITEMON",				GUI_ITEMON,
	"ITEMOFF",				GUI_ITEMOFF,
	"JUSTIFYCENTER",		GUI_JUSTIFYCENTER,
	"JUSTIFYLEFT",			GUI_JUSTIFYLEFT,
	"JUSTIFYRIGHT",			GUI_JUSTIFYRIGHT,
	"LINEOFF",				GUI_LINEOFF,
	"LINEON",				GUI_LINEON,
	"LOCATE",				GUI_LOCATE,
	"LOCATELINE",			GUI_LOCATELINE,
	"LOCATETAB",			GUI_LOCATETAB,
	"MARK",					GUI_MARK,
	"MINSERT",				GUI_MINSERT,
	"NAMEFILTER",			GUI_NAMEFILTER,
	"NOFOCUS",				GUI_NOFOCUS,
	"PASTE",				GUI_PASTE,
	"POINTER",				GUI_POINTER,
	"POSITION",				GUI_POSITION,
	"PUSHBUTTON",			GUI_PUSHBUTTON,
	"RANGE",				GUI_RANGE,
	"READONLY",				GUI_READONLY,
	"REMOVECONTROL",		GUI_REMOVECONTROL,
	"REMOVESPACEAFTER",		GUI_REMOVESPACEAFTER,
	"REMOVESPACEBEFORE",	GUI_REMOVESPACEBEFORE,
	"RIGHTCLICKOFF", 		GUI_RIGHTCLICKOFF,
	"RIGHTCLICKON",			GUI_RIGHTCLICKON,
	"ROOTLEVEL",			GUI_ROOTLEVEL,
	"SELECT",				GUI_SELECT,
	"SELECTALL",			GUI_SELECTALL,
	"SELECTLINE",			GUI_SELECTLINE,
	"SETLINE",				GUI_SETLINE,
	"SETMAXCHARS",			GUI_SETMAXCHARS,
	"SETSELECTEDLINE",		GUI_SETSELECTEDLINE,
	"SHOW",					GUI_SHOW,
	"SHOWONLY",				GUI_SHOWONLY,
	"STATUS",				GUI_STATUS,
	"TEXT",					GUI_TEXT,
	"TEXTBGCOLOR",			GUI_TEXTBGCOLOR,
	"TEXTCOLOR",			GUI_TEXTCOLOR,
	"TEXTCOLORDATA",		GUI_TEXTCOLORDATA,
	"TEXTCOLORLINE",		GUI_TEXTCOLORLINE,
	"TEXTSTYLEDATA",		GUI_TEXTSTYLEDATA,
	"TEXTSTYLELINE",		GUI_TEXTSTYLELINE,
	"TEXTFONT",				GUI_TEXTFONT,
	"TITLE",				GUI_TITLE,
	"UNMARK",				GUI_UNMARK,
	"UPLEVEL",				GUI_UPLEVEL,
	"TEXTBGCOLORDATA",		GUI_TEXTBGCOLORDATA,
	"TEXTBGCOLORLINE",		GUI_TEXTBGCOLORLINE,
	NULL, 0
};
#endif

#define LIST_INSERTNORMAL		0
#define LIST_INSERTBEFORE		1
#define LIST_INSERTAFTER		2

#define MENU_BASECMD_MASK	0x003F  /* menu mask, internal use only */

#define ICONDEFAULT_HSIZE		16
#define ICONDEFAULT_VSIZE		16
#define ICONDEFAULT_COLORBITS	1

#define CURSORTYPE_ARROW		1
#define CURSORTYPE_WAIT			2
#define CURSORTYPE_IBEAM		3
#define CURSORTYPE_CROSS		4
#define CURSORTYPE_SIZENS		5
#define CURSORTYPE_SIZEWE		6
#define CURSORTYPE_APPSTARTING  7
#define CURSORTYPE_UPARROW      8
#define CURSORTYPE_HAND         9
#define CURSORTYPE_HELP        10
#define CURSORTYPE_NO          11
#define CURSORTYPE_SIZEALL     12
#define CURSORTYPE_SIZENESW    13
#define CURSORTYPE_SIZENWSE    14

#define LISTITEM_GROW_SIZE		64
#define TABLEROW_GROW_SIZE     32

#define TREEVIEW_ITEM_MAX_SIZE 256
#define GUIAM_LOWMESSAGE			WM_APP

#if OS_WIN32
#define DBC_RGB(r, g, b) ((DWORD) (((BYTE) (r) | ((WORD) (g) << 8)) | (((DWORD) (BYTE) (b)) << 16)))
#define DBC_RED(dw) 	((BYTE) (dw))
#define DBC_GREEN(dw)	((BYTE) (dw >> 8))
#define DBC_BLUE(dw) 	((BYTE) (dw >> 16))
static DWORD dwDefaultPal[16]
#ifdef __GNUC__
	__attribute__ ((unused))
#endif
	= {
	DBC_RGB(0x00, 0x00, 0x00),		/* 0 black */
	DBC_RGB(0x00, 0x00, 0x80),		/* 1 dark blue */
	DBC_RGB(0x00, 0x80, 0x00), 		/* 2 dark green */
	DBC_RGB(0x00, 0x80, 0x80),		/* 3 dark cyan */
	DBC_RGB(0x80, 0x00, 0x00), 		/* 4 dark red */
	DBC_RGB(0x80, 0x00, 0x80),		/* 5 dark magenta */
	DBC_RGB(0x80, 0x80, 0x00),		/* 6 dark yellow */
	DBC_RGB(0xC0, 0xC0, 0xC0),		/* 7 light gray */
	DBC_RGB(0x80, 0x80, 0x80),		/* 8 medium gray */
	DBC_RGB(0x00, 0x00, 0xFF),		/* 9 blue */
	DBC_RGB(0x00, 0xFF, 0x00),		/* 10 green */
	DBC_RGB(0x00, 0xFF, 0xFF),		/* 11 cyan */
	DBC_RGB(0xFF, 0x00, 0x00),		/* 12 red */
	DBC_RGB(0xFF, 0x00, 0xFF),		/* 13 magenta */
	DBC_RGB(0xFF, 0xFF, 0x00),		/* 14 yellow */
	DBC_RGB(0xFF, 0xFF, 0xFF)};		/* 15 white */

/* define maximum return message size */
#define MAXCBMSGSIZE		65500
#define cbmsgname (cbmsg)			/* name */
#define cbmsgfunc (cbmsg + 8)		/* function */
#define cbmsgid (cbmsg + 12)		/* control id */
#define cbmsgdata (cbmsg + 17)		/* data area */

typedef struct ddestruct {
	BYTE type;  				/* see DDE_TYPE defines */
	HWND  hwndOwner;			/* handle for DDE communication */
	HWND  hwndRemote;			/* handle of DDE remote connection */
	UINT  acktype;				/* acknowledge type */
	ZHANDLE hApplication;		/* Application name */
	ATOM  aApplication;			/* Application atom */
	ZHANDLE hTopic;				/* Topic name */
	ATOM  aTopic;				/* Topic atom */
	HGLOBAL hDdeData;			/* global handle to DDE data to be released */
	void  (*callback)(INT, BYTE *, INT);  /* DDE callback function */
	UINT iCount;
} DBCDDE;

typedef struct ddepeekstruct {
	LPMSG msg;
	HWND hwnd;
	UINT msgmin;
	UINT msgmax;
	UINT removeflags;
} DDEPEEK;

/* defines for dde */
#define DDE_TYPE_OPEN		0x01
#define DDE_TYPE_WINDOW		0x02
#define DDE_TYPE_TERMINATE	0x04
#define DDE_TYPE_SERVER		0x10
#define DDE_TYPE_CLIENT		0x20

#define GUIAM_DDESERVER				(GUIAM_LOWMESSAGE + 31)
#define GUIAM_DDECLIENT				(GUIAM_LOWMESSAGE + 32)
#define GUIAM_DDEPEEK				(GUIAM_LOWMESSAGE + 33)
#define GUIAM_DDEDESTROY			(GUIAM_LOWMESSAGE + 34)

#ifdef GUIADDE
CHAR clientclass[] = "DBCDDECLIENT";	/* window class name for all DDE clients */
CHAR serverclass[] = "DBCDDESERVER";	/* window class name for all DDE server */
INT ddeinitflg = FALSE;		/* dde initialized flag */
DBCDDE *ddehead;				/* Pointer to head of DDE array */
INT ddemax;					/* number of allocated dde structures */
#else
extern CHAR clientclass[];	/* window class name for all DDE clients */
extern CHAR serverclass[];	/* window class name for all DDE server */
extern INT ddeinitflg;		/* dde initialized flag */
extern DBCDDE *ddehead;				/* Pointer to head of DDE array */
extern INT ddemax;					/* number of allocated dde structures */
#endif

#ifdef GUIAWIN2
CHAR szPopboxClassName[] = "DXPopbox";
CHAR szTableClassName[] = "DXTable";
CHAR szTableCellsClassName[] = "DXTableCells";
BYTE cbmsg[MAXCBMSGSIZE];	/* message area for all call backs */
HBRUSH hbrButtonFace;		/* brush used to paint client area background */
ATOM TableClassAtom;
#else
extern CHAR szPopboxClassName[];
extern CHAR szTableClassName[];
extern CHAR szTableCellsClassName[];
extern BYTE cbmsg[MAXCBMSGSIZE];	/* message area for all call backs */
extern HBRUSH hbrButtonFace;		/* brush used to paint client area background */
extern ATOM TableClassAtom;
#endif

#ifdef GUIAWIN3
ATOM aDialogClass;
CHAR szDXDropboxClassName[] = "DXDropbox";
CHAR szDXCheckboxClassName[] = "DXCheckbox";
INT dxdropboxclassregistered;	/* boolean for lazy registration of the popbox window class */
INT dxcheckboxclassregistered;	/* boolean for lazy registration of the checkbox window class */
#else
extern ATOM aDialogClass;
extern CHAR szDXDropboxClassName[];
extern INT dxdropboxclassregistered;	/* boolean for lazy registration of the popbox window class */
extern CHAR szDXCheckboxClassName[];
extern INT dxcheckboxclassregistered;	/* boolean for lazy registration of the checkbox window class */
#endif

#if OS_WIN32GUI
typedef struct dirdlgstruct {
	/**
	 * This is the DEFAULT keyword value from the prep string for an opendirdlg
	 */
	CHAR *pszStartDir;
	LPCSTR deviceFilters;
	int retcode;
	BROWSEINFO dirstruct;
} DIRDLGINFO;

/*
 * Set up a typedef for the system function SHCreateItemWithParent in shell32
 * We need to do this because the function did not exist prior to Vista
 * and using it directly would prevent loading
 */
typedef HRESULT (STDAPICALLTYPE *guiaPSHCreateItemWithParent) (LPCITEMIDLIST, IShellFolder*, LPCITEMIDLIST, REFIID, void **);

#ifdef GUIAWIN1
/*
 * pointer to the SHCreateItemWithParent function in the shell32 library
 */
guiaPSHCreateItemWithParent guiaSHCreateItemWithParent;
#else
extern guiaPSHCreateItemWithParent guiaSHCreateItemWithParent;
#endif


#endif /* OS_WIN32GUI */

#define DXTBL_ADDROW            WM_APP + 100
#define DXTBL_INSERTROW         WM_APP + 101
#define DXTBL_FOCUSCELL         WM_APP + 102
#define DXTBL_COLORROW          WM_APP + 103
#define DXTBL_COLORCOLUMN       WM_APP + 104
#define DXTBL_COLORCELL         WM_APP + 105
#define DXTBL_COLORTABLE        WM_APP + 106
#define DXTBL_READONLY          WM_APP + 107

#define TBLSTATESTRUCT_OFFSET  0

typedef struct tagTableState {
	HFONT font;
	INT headerCellHeight, cellHeight;
	BOOL hscroll, vscroll, readonly;
	SIZE table, visible;
	HWND hwndHeader;
	HWND hwndCells;
	UINT numcolumns;
	struct columnsTag {
		UINT columnWidth;
		UINT type;			/* Uses PANEL_* codes */
	} columns[1];
} TABLESTATE, *PTABLESTATE;

#endif	/* OS_WIN32 */

#ifdef GUIB
/**
 * Default and minimum strip size
 */
#define UNCMPSIZE 38000
/*
 * In bytes, this is set to zero if not applicable, by guiawin
 */
UINT userTifMaxstripsize;
#else
extern UINT userTifMaxstripsize;
#endif

/* typedefs and structure definitions */

#ifndef GUIHANDLESDEFINED
typedef struct windowstruct ** WINHANDLE;
typedef struct resourcestruct ** RESHANDLE;
typedef struct pixmapstruct ** PIXHANDLE;
#endif

typedef struct windowstruct WINDOW;
typedef struct pixmapstruct PIXMAP;

struct xpoint {
	INT h;
	INT v;
};

struct xrect {
	INT top, bottom, left, right;
};

#if OS_WIN32
struct windowstruct {
	UCHAR name[MAXRESNAMELEN + 1];		/* 8 character blank filled window name, and a terminating null */
	USHORT autoscroll;		/* if TRUE window is autoscroll */
	USHORT maxsizeflg;		/* TRUE if window is maximized, else mid sized */
	USHORT nosizeflg;		/* TRUE if window should disallow sizing */
	USHORT debugflg;		/* TRUE if window is a debugger window, which can be activated when a dialog is up */
	USHORT sbflg;			/* scroll bars can exist flag */
	USHORT hsbflg;			/* horz scroll bar active flag */
	USHORT vsbflg;			/* vert scroll bar active flag */
	USHORT mousemsgs;		/* report mouse messages flag */
	USHORT activeflg;		/* TRUE if window is active window */
	USHORT caretonflg;		/* TRUE/FALSE equal to status of CARETON/CARET change function calls */
	USHORT pandlgscale;		/* TRUE if window is scaled */
	USHORT nofocus;			/* TRUE if window should not get focus when created */
	USHORT taskbarbutton;	/* TRUE if taskbar button should appear when window is minimized */
	USHORT statusbarshown;	/* set to TRUE if window is showing a status bar */
	USHORT caretcreatedflg;	/* TRUE if caret is currently created */
	USHORT floatwindow;		/* TRUE if this is a FloatWindow */
	INT xsize;				/* window contained object maximum horz size (use with autoscroll) */
	INT ysize;				/* window contained object maximum vert size (use with autoscroll) */
	INT scrollx;			/* scrolling horz offset (use with autoscroll) */
	INT scrolly;			/* scrolling vert offset (use with autoscroll) */
	INT hsbmin;				/* horz scroll bar minimum value (use without autoscroll) */
	INT hsbmax;				/* horz scroll bar maximum value (use without autoscroll) */
	INT hsbpage;			/* horz scroll bar page change value (use without autoscroll) */
	INT vsbmin;				/* vert scroll bar minimum value (use without autoscroll) */
	INT vsbmax;				/* vert scroll bar maximum value (use without autoscroll) */
	INT vsbpage;			/* vert scroll bar page change value (use without autoscroll) */
	INT clientx;			/* currently displayed part of work area horz size */
	INT clienty;			/* currently displayed part of work area vert size (does not include status or tool bar) */
	INT caretvsize;			/* vertical height of caret */
	INT bordertop;			/* height of top border of client area (region used for tool bars) */
	INT borderbtm;			/* height of bottom border of client area (region used for status bars) */
	INT changeflag;			/* set to TRUE when a CHANGE operation is taking place on this window */
	ZPOINT caretpos;		/* current caret position */
	ZHANDLE title;			/* title or NULL */
	ZHANDLE owner;			/* optional owner window of a floatwindow */
	ZHANDLE statusbartext;	/* status bar text or NULL */
	WINCALLBACK cbfnc;		/* callback function or NULL */
	WINHANDLE thiswinhandle;	/* handle of this window structure */
	WINHANDLE nextwinhandle;	/* handle of next window */
	RESHANDLE showmenu;		/* handle of currently shown menu */
	RESHANDLE showtoolbar;	/* handle of currently shown toolbar */
	RESHANDLE showreshdr;	/* handle of first shown resource in linked list of shown resources */
	RESHANDLE showpopupmenu;/* handle of pop up menu shown, if any */
	PIXHANDLE showpixhdr;	/* handle of first shown pixmap in linked list of shown pixmaps */
	UINT lrufocus;			/* lru focus count */
	CONTROL *ctlorgdflt;	/* Is there a user defined Default Pushbutton on this window? NULL if not*/
	CONTROL *ctldefault;	/* Currently visible default pushbutton on this window */
#if OS_WIN32GUI
	HWND hwnd;				/* HWND to window */
	HWND hwndstatusbar;		/* HWND of status bar in window */
	//HWND hwndtaskbarbutton;	/* if minimized, HWND of window in task bar */
	HWND hwndtt;			/* HWND for tooltip window */
	HWND ownerhwnd;			/* HWND for the owner window of a floater*/
	//HDC  hdc;				/* HDC of window, new draw code elimnated the use of this 01/2011 */
	HCURSOR hcursor;		/* HCURSOR for mouse when in client area of this window */
	HICON hbicon;			/* Big icon for window */
	HICON hsicon;			/* Small icon for window */
#endif
};
#endif

/*
 * RESOURCE structures are allocated in DX memory (memalloc)
 */
typedef struct resourcestruct {
	UCHAR name[MAXRESNAMELEN];/* 8 character blank filled resource name */
	UCHAR restype;			/* RES_TYPE_ value */
	UCHAR screenrelflag;	/* TRUE if boundary of PANEL is screen relative */
	UCHAR noclosebuttonflag;/* TRUE if no close button in DIALOG title bar */
	ZHANDLE title;			/* pascal string of title */
	/**
	 * content of resource
	 *
	 * For a Panel/Dialog, this is an array of CONTROL structures from heap memory.
	 * 	entrycount is the number of items in the array.
	 */
	ZHANDLE content;
	INT entrycount;			/* number array entries in content of resource, or size (in bytes) of initial image of icon resource */
	INT insertpos;			/* toolbar button where new buttons/spaces are inserted/appended */
	INT32 color;			/* color */
	ZRECT rect;				/* bounding rectangle of this resource */
	void (*cbfnc)(RESHANDLE, UCHAR *, INT);  /* callback function or NULL */
	RESHANDLE thisreshandle;/* handle of this resource structure */
	RESHANDLE nextreshandle;/* handle of next resource */
	WINHANDLE winshow;		/* window shown in, NULL if not shown */
	RESHANDLE showresnext;	/* handle to next resource in linked list of shown resources */
	RESHANDLE nextshowndlg;	/* next shown dialog in linked list of shown dialogs */
	INT hsize;				/* horz size of icon resource */
	INT vsize;				/* vert size of icon resource */
	UCHAR itemmsgs;			/* report item messages flag */
	UCHAR focusmsgs;		/* report focus messages flag */
	UCHAR linemsgs;			/* report list box line numbers instead of text in messages */
	UCHAR rclickmsgs;		/* report right click messages flag */
	UCHAR bpp;				/* number of bits per pixel for icon resource */
	UCHAR appendflag;		/* set to TRUE if toolbar buttons are appended to insert pos */
	INT32 textcolor;		/* current fg text color code, one of the TEXTCOLOR_??? codes */
	INT speedkeys;			/* used for menu resource */
	ZPOINT showoffset;		/* horizontal and vertical offset shown (for panel) */
	CONTROL *ctlfocus;		/* Current control focus */
	CONTROL *ctldefault;	/* default pushbutton controlstruct */
	INT ctlorgdflt;			/* index + 1 for original default pushbutton defined by PREP statement */
	INT ctlescape;			/* index + 1 for content or currently visible escape pushbutton on this panel/dialog, if any */
	UCHAR *bmiData;         /* Allocated from the heap */
#if OS_WIN32GUI
	COLORREF textfgcolor;	/* current foreground text color, used only if above is TEXTCOLOR_CUSTOM */
	HACCEL hacAccelHandle;	/* handle to an accelarator table */
	HWND hwnd;				/* Window handle to dialog window (or if panel, window res is shown in) */
	HWND hwndtoolbar;		/* Window handle of toolbar resource */
	HMENU hmenu;			/* Menu handle of menu resource */
	HBITMAP hbmicon;		/* handle of icon bitmap */
	HBITMAP hbmgray;
	PBITMAPINFO bmiColor;   /* Allocated from the heap */
	PBITMAPINFO bmiGray;
	BOOL useTransBlt4Icon;  /* When icon used in menu, for painting, use TransparentBlt */
	UINT transColor;		/* Color representing transparent. Stored as Reserved;Red;Green;Blue */
	HFONT font;				/* working control font or NULL if default */
	ZHANDLE namefilter;		/* Used for Open and SaveAs dialogs */
	ZHANDLE deviceFilters;	/* Use for 'deviceFilter' prep option of an opendirdlg */
	BOOL isBeingDestroyed;  /* Set to TRUE at the top of guiresdestroy in gui.c */
	/*
	 * Presence of "MULTIPLE" keyword in prep string of an Open File Dialog sets this to TRUE
	 */
	BOOL openFileDlgMultiSelect;
#endif
} RESOURCE;

/**
 * Memory for these structures comes from memalloc and is zeroed.
 */
struct pixmapstruct {
	PIXHANDLE thispixhandle;/* handle of the pixel map structure */
	PIXHANDLE nextpixhandle;/* linked list pointer for next pixel map */
	WINHANDLE winshow;		/* window shown in, NULL if not shown */
	PIXHANDLE showpixnext;	/* handle to next pixmap in linked list of shown pixmaps for this window */
	ZHANDLE buffer;			/* DIB buffer for application storage/manipulation of image */
	LONG pencolor;			/* current color, stored as 00BBGGRR*/
	LONG bgpencolor;		/* current text background color, -1 means transparent */
	/*
	 * Used only if a Smart Client is in use. If TRUE, image has changed
	 * and has yet to be sent to the client.
	 */
	INT dirty;
	ZPOINT pos1;			/* current position */
	ZPOINT pos2;			/* draw parameter */
	ZPOINT pos3;			/* draw parameter */
	USHORT hsize;			/* horz size of pixel map */
	USHORT vsize;			/* vert size of pixel map */
	SHORT hshow;			/* shown window client area horz offset */
	SHORT vshow;			/* shown window client area vert offset */
	USHORT fontheight;		/* height of current font (including leading) */
	USHORT fontwidth;		/* width of capital M in current font */
	USHORT tifnum;			/* number of image in multiple image TIF file */
	UCHAR status;			/* 1 = open, 0 = closed */
	UCHAR bpp;				/* bits per pixel */
	UCHAR penwidth;			/* draw line width */
	UCHAR pentype;			/* draw line type */
#if OS_WIN32
	HBITMAP hbitmap;		/* handle to a BITMAP created by CreateDIBSection */
	LOGPEN logpen;			/* logical pen selected for pixel map */
	HPALETTE hpal;			/* handle to logical palette associated with this pix map */
	BOOL isGrayScaleJpeg;   /* Used only by JPEG and only for transmission to Smart Clients */
#endif
#if OS_WIN32GUI
	/*
	 * A pix mutex object.
	 * All sections of code that either alter the pix or
	 * paint it need to get ownership of this mutex first.
	 */
	HANDLE hPaintingMutex;
	HDC hdc;				/* DC for pixel map */
	COLORREF clrref;		/* color reference */
	LOGFONT lf;				/* logical font structure */
	HFONT hfont;
	BITMAPINFO *pbmi;		/* Bitmap info for 24bit images */
	HPEN hTempPen;
	UCHAR tempWidth;
#else
	UCHAR **pixbuf;			/* pixel buffer */
	UINT bufsize;			/* size in bytes of the buffer */
#endif
};

typedef struct menuitemstruct {
	ZHANDLE text;			/* text */
	RESHANDLE iconres;		/* handle of icon resource */
#if OS_WIN32GUI
	HMENU menuhandle;		/* menu handle that this item is contained in */
	HMENU submenu;			/* menu handle of submenu that this item controls */
#endif
	USHORT speedkey;		/* speed key character, 0 if none */
	USHORT useritem;		/* user item number */
 	USHORT count;			/* menu item byposition position */
	UCHAR level;			/* menu level, 0 = main */
	UCHAR style;			/* 0 = normal */
							/* 1 = gray */
							/* 2 = checked */
							/* 3 = gray and checked */
							/* 4 = line */
							/* 5 = not displayed and takes up no space */
} MENUENTRY, *PMENUENTRY;

typedef struct treestruct TREESTRUCT;
struct treestruct {
	CHAR text[TREEVIEW_ITEM_MAX_SIZE];
	TREESTRUCT *nextchild;
	TREESTRUCT *firstchild;
	TREESTRUCT *parent;
	INT imageindex;
	INT expanded;
#if OS_WIN32GUI
	COLORREF color;
	HTREEITEM htreeitem;
	USHORT attr1;			/* line style */
#endif
};

typedef struct treeiconstruct TREEICONSTRUCT;
struct treeiconstruct {
	RESHANDLE iconres;
	TREEICONSTRUCT *next;
};

enum TableChangeScope {
	tableChangeScope_table,
	tableChangeScope_column,
	tableChangeScope_row,
	tableChangeScope_cell
};

#if OS_WIN32GUI
typedef struct tag_itemattributes {
	UINT attr1;			/* array for color, line style, etc... for each listbox/drop box line */
	BOOL userBGColor;	/* If FALSE use COLOR_WINDOW instead of the next field */
	COLORREF bgcolor;	/* background color */
} ITEMATTRIBUTES;
#endif

/*
 * CONTROL structures are allocated by guiAllocMem so
 * they come from the Windows heap (malloc) and can be
 * expected to NOT move.
 *
 * Exception to the above. For a toolbar, controls can be dynamically added and removed.
 * So the array of controls will change. For toolbars, cannot cache pointer to CONTROL
 * struct in the peer. It might move.
 */
#if OS_WIN32

typedef struct color_tag {
	INT32 color;
	INT code; /* 1 = COLORREF, 0 = Standard DX color number */
} TABLECOLOR;

struct controlstruct {
	ZHANDLE tooltip;		/* text for tooltips */
	ZHANDLE textval;		/* handle to text value */
	ZHANDLE mask;			/* handle to mask value for fedit or array of ZHANDLES of column headers text for mlistbox */
	ZHANDLE changetext;		/* handle to text value for change */
	ZHANDLE listboxtabs;	/* handle to tab settings for listbox */
	ZHANDLE listboxjust;	/* handle to tabs justification control string */
	ZHANDLE mlbsellist;		/* handle to multi select list box query data */
	RESHANDLE iconres;		/* handle of icon resource */
	RESHANDLE res;			/* pointer to resource control is contained in */
	union {
		struct tree_struct {
			TREESTRUCT *root;					/* insert position : pointer to oldest ancestor */
			TREESTRUCT *cpos;					/* insert position : pointer to current leaf */
			TREESTRUCT *parent;  				/* insert position : pointer to parent */
			TREESTRUCT *spos;					/* current selection position */
			TREESTRUCT *xpos;					/* temp only for expand/collapse change command */
			TREEICONSTRUCT *iconlist;			/* linked list of icon resources used in tree */
#if OS_WIN32GUI
			HTREEITEM hitem;					/* current selected tree item */
#endif
			INT line;							/* insert position : current line number */
			INT level;							/* insert position : current level */
			CHAR text[TREEVIEW_ITEM_MAX_SIZE];	/* current selected text */
		} tree;
		struct table_struct {
			ZHANDLE columns;			/* Points to a contiguous set of column structs */
			ZHANDLE rows;				/* An array of ZHANDLEs, each one points to a row struct, is NULL if there are no rows */
			ZHANDLE changetext;
			UINT numcolumns;
			UINT numrows;
			UINT numrowsalloc;			/* size of rows array */
			ZPOINT changeposition;		/* These should always be one-based row and column numbers */
			enum TableChangeScope changescope;
			TABLECOLOR textcolor;			    /* current text color */
			INT noheader;				/* TRUE if the NOHEADER option was used for this table */
		} table;
		struct tab_struct {
			BOOL hasFocus;	/* The Tab Header itself has the focus */
		} tab;
	};
	UCHAR *editbuffer;		/* edit buffer for PASTE command */
	USHORT type;			/* control type - same as PANEL_ definitions */
	USHORT useritem;		/* user item number */
	USHORT value;			/* control value, current item in list box (one based) */

	/*
	 * min val for progress/scroll bar, radio button count, or number items in list box
	 * If listbox/dropbox, is number of items in arrays itemarray, itemattributes
	 */
	USHORT value1;
	/*
	 * maximum value for progress/scroll bar or ZHANDLE count in a listbox itemarray
	 */
	USHORT value2;
	/**
	 * page up/down increment for scroll bar, change value for list box,
	 * 'collapse/expand/setline' line number (one-based) for a tree
	 */
	USHORT value3;
	INT value4;				/* listbox top item (1 based) (hide/show persistance) */
	USHORT insstyle;		/* see defines for control->insstyle */
	USHORT inspos;			/* insert pos for style={before|after} (one based) */
	USHORT style;			/* control style */
	USHORT selectedtab;		/* selected tab (0 based) (hide/show persistance) */
	USHORT changestyle;		/* temporarily holds font style for change function */
	ZRECT rect;				/* bounding rectangle */
	INT maxtextchars;		/* handle to maximum characters allowed in LEDIT box */

	/*
	 * temporarily holds color for change function
	 * depending on how it is used, this could be either a TEXTCOLOR_??? (defined in gui.h)
	 * value or an actual COLORREF
	 */
	INT32 changecolor;

	INT editinsertpos;		/* for cut, copy, paste of edit controls */
	INT editendpos;			/* ditto */
	INT maxlinewidth;		/* for LISTBOX and MLISTBOX horizontal scrollbar */
	INT tabgroupxoffset;	/* horz offset of this control's tab group */
	INT tabgroupyoffset;	/* vert offset of this control's tab group */
	INT ctloffset;			/* offset in array of controlstructs of this control */
	INT changeflag;			/* set to TRUE when a CHANGE operation is taking place on this control */
	UCHAR shownflag;		/* set to TRUE if control is shown (other tabsubgroups are not shown) */
	UCHAR escflag;			/* TRUE if user designated this control as ESCPUSHBUTTON */
	UCHAR tabgroup;			/* tab group number, zero if not part of tabgroup */
	UCHAR tabsubgroup;		/* tab subgroup number one-based */
	UCHAR filler1;			/* unused */
#if OS_WIN32GUI
	INT32 textcolor;		/* current fg text color code, one of the TEXTCOLOR_??? codes */
	COLORREF textfgcolor;	/* current foreground text color, used only if above is TEXTCOLOR_CUSTOM */
	HFONT font;				/* control font or NULL if default */
	ZHANDLE itemarray;		/* array of ZHANDLES, one ZHANDLE per item in listbox/drop box control */
	ZHANDLE itemcutarray;	/* same as item array, except with cut strings for list box control */
	/*
	 * Pointer to an array of ITEMATTRIBUTES structures
	 */
	ITEMATTRIBUTES *itemattributes2;
	HWND ctlhandle;			/* control handle */
	HWND tabgroupctlhandle;	/* this control's tab group control handle */
	//HINSTANCE insthandle;	/* instance handle */
	WNDPROC oldproc;
	COLORREF textbgcolor;	/* current background text color */
	BOOL userBGColor;		/* If FALSE use DX default for the control instead of the above field */
#endif
};

/**
 * This structure is used in table DROPBOX and CDROPBOX column and cell structures
 */
typedef struct tabledropboxstruct {
	ZHANDLE itemarray;		/* Array of ZHANDLES, one ZHANDLE per item in cdropbox control */
	USHORT itemalloc;		/* Number of slots allocated in itemarray */
	USHORT itemcount;		/* Number of slots in use in itemarray */
	USHORT insstyle;		/* Before, after, or normal */
	USHORT inspos;			/* Insert position for style={before|after} (one based) */
} TABLEDROPBOX, *PTABLEDROPBOX;

typedef struct tablecolumnstruct {
	ZHANDLE name;
	ZHANDLE mask;
	TABLECOLOR textcolor;			/* current text color */
	UINT namelength;		/* size of allocated space, may be larger than actual null terminated string, but never smaller! */
	UINT type;				/* Uses PANEL_* codes */
	UINT width;
	USHORT useritem;		/* column user item number */
	USHORT dbopenht;		/* For DROPBOX and CDROPBOX columns, expanded height in pixels*/
	USHORT justify;         /* for Edit columns, uses CTRLSTYLE_ codes */
	UCHAR insertorder;		/* For DROPBOX and CDROPBOX columns, TRUE means do not use alpha sorting */
	UCHAR maxchars;			/* For LEDIT columns, max number of characters in the box */
	TABLEDROPBOX db;
} TABLECOLUMN, *PTABLECOLUMN;

typedef struct tablecellstruct {
	union {
		ZHANDLE text;           /* For a VTEXT, RVTEXT, POPBOX, EDIT, LEDIT, FEDIT */
		struct {
			USHORT itemslctd;	/* For DROPBOX and CDROPBOX columns, the currently selected item, one based, zero means no selection */
			TABLEDROPBOX db;
		} dbcell;
	};
	USHORT style;		/* used for edit cells for justify flags (CTRLSTYLE_???) and checkbox cells for marked */
	TABLECOLOR textcolor;			/* current text color */
} TABLECELL, *PTABLECELL;

typedef struct tablerowstruct {
	TABLECOLOR textcolor;			/* current text color */
	TABLECELL cells[1];
} TABLEROW, *PTABLEROW;
#endif

/* function prototypes */
INT guiaAbout(ZHANDLE, ZHANDLE);
//INT guiaActivateMsg(void (*)(INT, UCHAR *, INT), INT);
INT guiaAppDestroy(void);
void guiaBeep(INT, INT, INT);
void guiaBeepOld(void);
void guiaBitbltErrorIgnore(void);
INT guiaChangeAppIcon(RESOURCE *, CHAR *);
INT guiaChangeAppWinTitle(CHAR *);
INT guiaChangeConsoleWinFocus(void);
INT guiaChangeControl(RESOURCE *, CONTROL *, INT);
INT guiaChangeDlgTitle(RESOURCE *);
INT guiaChangeIcon(RESOURCE *, UCHAR *, INT);
INT guiaChangeMenu(RESOURCE *, MENUENTRY *, INT);
INT guiaChangeStatusbar(WINDOW *);
void guiaChangeTableMinsert(RESOURCE* res, CONTROL* control, UCHAR** stringArray, INT count);
INT guiaChangeToolbar(RESOURCE *, CONTROL *, INT);
INT guiaChangeWinIcon(WINDOW *, RESOURCE *);
INT guiaChangeWinMaximize(WINDOW *);
void guiaChangeWinMinimize(WINDOW *);
INT guiaChangeWinPosition(WINDOW *, INT, INT);
void guiaChangeWinRestore(WINDOW *);
INT guiaChangeWinSize(WINDOW *, INT, INT);
INT guiaChangeWinTitle(WINDOW *);
INT guiaChangeWinTTWidth(WINDOW *, INT);
INT guiaClipboardBPP(INT *);
void guiaClipboardCodepage(CHAR *);
INT guiaClipboardRes(INT *, INT *);
INT guiaClipboardSize(INT *, INT *);
void guiaClipCopyPixmap(PIXMAP *);
void guiaCopyPixmap(PIXMAP *);
INT guiaCreateIcon(RESOURCE *);
INT guiaCreatePixmap(PIXHANDLE);
INT guiaDeleteToolbarControl(RESOURCE *, CONTROL *, INT);
INT guiaDestroyControl(CONTROL *);
INT guiaDestroyPixmap(PIXHANDLE);
void guiaDisplayText(WINDOW*, INT, INT, INT, INT);
void guiaDrawAngledText(ZHANDLE, INT);
void guiaDrawBigDot(void);
void guiaDrawBox(void);
void guiaDrawCircle(void);
void guiaDrawImageRotationAngle(INT);
void guiaDrawLine(void);
void guiaDrawRect(void);
void guiaDrawReplace(INT32, INT32);
void guiaDrawText(ZHANDLE, UINT);
void guiaDrawTriangle(void);
void guiaEditCaretOld(void);
void guiaFeditTextSelectionOnFocusOld(void);
void guiaDrawEnd(void);
void guiaEnterkey(CHAR *ptr);
INT guiaExecTask(ZHANDLE, INT, void (*)(CHAR *));
INT guiaExit(INT);
void guiaFEditMaskText(CHAR *, CHAR *, CHAR *);
void guiaFocusOld(void);
INT guiaFreeIcon(RESOURCE *);
INT guiaGetCBPixmap(PIXMAP *);
INT guiaGetCBText(UCHAR *text, size_t *cbText);
INT guiaGetCBTextLen(size_t *cbTextLen);
INT guiaGetControlSize(CONTROL *, INT *, INT *);
INT guiaGetControlText(CONTROL *, CHAR *, INT);
CONTROL* guiaGetFocusControl(RESOURCE *);
INT guiaGetNextUserMsg(INT *);
void guiaGetPixel(PIXMAP *, INT32 *, INT, INT);
INT guiaGetTableCellText(CONTROL *, CHAR *data, INT datalen);
INT guiaGetTableCellSelection(CONTROL *);
INT guiaGifBPP(UCHAR *, INT *);
INT guiaGifSize(UCHAR *, INT *, INT *);
INT guiaGifToPixmap(PIXHANDLE pixhandle, UCHAR **bufhandle);
INT guiaHideCaret(WINDOW *);
INT guiaHideControls(RESOURCE *);
INT guiaHideMenu(RESOURCE *);
INT guiaHidePixmap(PIXHANDLE);
INT guiaHideStatusbar(WINDOW *);
INT guiaHideToolbar(RESOURCE *);
INT guiaInit(INT, CHAR *);
void guiaInitTrayIcon(INT trapid);
void guiaInsertbeforeOld(void);
void guiaUseTerminateProcess(void);
void guiaUseTransparentBlt(INT value);
INT guiaJpgBPP(UCHAR *, INT *);
INT guiaJpgRes(UCHAR *, INT *, INT *);
INT guiaJpgSize(UCHAR *, INT *, INT *);
INT guiaJpgToPixmap(PIXHANDLE pixhandle, UCHAR **bufhandle);
INT guiaMakeIcon(RESOURCE *, INT, INT, INT, UCHAR *, INT);
INT guiamemicmp(BYTE *, BYTE *, INT);
void guiaMListboxCreateList(RESOURCE *, CONTROL *);
void guiaMsgBox(ZHANDLE, ZHANDLE);
INT guiaPixmapToTif(PIXMAP *, UCHAR *, INT *);
void guiaPrintbandOn(void);
INT guiaPutCBPixmap(PIXMAP *);
INT guiaPutCBText(UCHAR *text, size_t cbText);
void guiaResume(void);
void guiaTableSendFocusMessage(CHAR* message, CONTROL *control, UINT column, UINT row);
INT guiaSetActiveWin(WINDOW *);
INT guiaSetCaretPos(WINDOW *, int, int);
INT guiaSetCaretVSize(WINDOW *, int);
void guiaSetCtlFont(CONTROL *);
INT guiaSetCursorShape(WINDOW *, INT);
void guiaSetFont(ZHANDLE);
void guiaSetImageDrawStretchMode(CHAR *ptr);
#if DX_MAJOR_VERSION >= 17
void guiaSetImageRotateOld(void);
#else
void guiaSetImageRotateNew(void);
#endif
void guiaSetPen(void);
void guiaSetPixel(void);
INT guiaSetResFont(RESOURCE *, UCHAR *, INT, INT);
INT guiaShowAlertDlg(RESOURCE *);
INT guiaShowColorDlg(RESOURCE *);
LRESULT guiaShowControls(RESOURCE *, INT, INT);
INT guiaShowFontDlg(RESOURCE *);
INT guiaShowMenu(RESHANDLE);
INT guiaShowOpenFileDlg(RESOURCE *);
INT guiaShowOpenDirDlg(RESOURCE *);
INT guiaShowPixmap(PIXHANDLE);
INT guiaShowPopUpMenu(RESHANDLE, INT, INT);
INT guiaShowSaveAsFileDlg(RESOURCE *);
void guiaSuspend(void);
void guiaTabToControl(RESOURCE *res, CONTROL *pctl, INT fTabForward);
INT guiaTifBPP(INT, UCHAR *, INT *);
INT guiaTifImageCount(UCHAR *, INT *);
void guiaTifMaxStripSize(CHAR *ptr);
INT guiaTifRes(INT, UCHAR *, INT *, INT *);
INT guiaTifSize(INT, UCHAR *, INT *, INT *);
INT guiaTifToPixmap(PIXHANDLE pixhandle, UCHAR **bufhandle);
void guiaUseEditBoxClientEdge(INT state);
void guiaUseListBoxClientEdge(INT state);
void guiaErasebkgndSpecial(erase_background_processing_style_t);
INT guiaWinCreate(WINHANDLE, INT, INT, INT, INT, INT);
INT guiaWinDestroy(WINHANDLE);
INT guiaWinScrollKeys(WINDOW *, INT, char *);
INT guiaWinScrollPos(WINDOW *, INT, INT);
INT guiaWinScrollRange(WINDOW *, INT, INT, INT, INT);

INT guicPngBPP(UCHAR *, INT *);
INT guicPngSize(UCHAR *, INT *, INT *);
INT guicPngToPixmap(PIXHANDLE pixhandle, UCHAR **bufhandle);
void guiaDrawStart(PIXHANDLE);
void guiaStretchCopyPixmap(PIXMAP *, INT);
INT guiaShowCaret(WINDOW *);
INT guiaShowToolbar(RESOURCE *);
INT guiaShowStatusbar(WINDOW *);
void guiaMBDebug(CHAR *, CHAR *);
INT guicPngRes(UCHAR *, INT *, INT *);
void hidetablecontrols(CONTROL *control);
void rescb(RESOURCE *, CHAR *, INT, INT);

#if OS_WIN32GUI
static void clearLastErrorMessage();
void drawTabHeaderFocusRectangle(CONTROL* ctl, BOOL hasFocus);
CONTROL* FindControlInWindow(HWND hwndWindow, HWND hwndControl);
void clearAllTabControlFocus(HWND hwndWindow);
HBRUSH getBackgroundBrush(HWND hwnd);
COLORREF getBackgroundColor(HWND hwnd);
HINSTANCE guia_hinst(void);
HWND guia_hwnd(void);
void guiaErrorMsg(CHAR *, DWORD);
CHAR* guiaGetLastErrorMessage ();
INT guiaGetWinText(HWND, CHAR *, INT);
void guiaHandleTableFocusMessage(HWND hwnd, UINT message, BOOL invalidate);
INT guiaOwnerDrawIcon(CONTROL *, const DRAWITEMSTRUCT *);
INT guiaOwnerDrawListbox(CONTROL *, const DRAWITEMSTRUCT *);
void guiaRegisterDXCheckbox(HINSTANCE);
void guiaRegisterDXDropbox(HINSTANCE);
ATOM guiaRegisterPopbox(HINSTANCE);
void guiaRegisterTable(HINSTANCE);
void guiaSetDefaultPalette(INT, RGBQUAD *);
void getColumnRowFromWindow(HWND hwnd, HWND hwndCells, int *column, int *row) ;
void handleTabChangeChanging(HWND hwnd, LPARAM lParam, BOOL *focwasontab);
int handleTableRightButtonClick(HWND hwnd, UINT message, LPARAM lParam, WPARAM wParam);
BOOL isTableEnabled(HWND hwnd);
int mainentry(int, char *[], char*[]);
INT makecontrol(CONTROL *control, INT xoffset, INT yoffset, HWND hwndshow, INT dialogflag);
INT sendRightMouseMessageFromPanel(HWND hwnd, UINT nMessage, CONTROL *control, LPARAM lParam, UINT wParam);
WNDPROC setControlWndProc(CONTROL *control);
LONG setctrlfocus(CONTROL *control);
void guiaSetControlTextColor(HDC dc, CONTROL *control, DWORD defaultColor);
void OwnerDrawCheckBox(CONTROL *control, const DRAWITEMSTRUCT *pdi);
void WinProc_OnDrawItem(HWND hwnd, const DRAWITEMSTRUCT *pdi);
void WinProc_OnMeasureItem(HWND hwnd, MEASUREITEMSTRUCT *pmi);
#endif
