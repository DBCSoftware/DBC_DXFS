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

#ifndef _DBCGUI_INCLUDED
#define _DBCGUI_INCLUDED

#ifdef GUI_C
INT formatFor24BitColorIsRGB = FALSE;
#else
extern INT formatFor24BitColorIsRGB;
#endif

#if OS_WIN32GUI
typedef PBYTE ZHANDLE;
typedef POINT ZPOINT;
typedef RECT ZRECT;
#else
typedef UCHAR *ZHANDLE;
typedef struct {
	int x;
	int y;
} ZPOINT;
typedef struct {
	int top;
	int bottom;
	int left;
	int right;
} ZRECT;
#endif
#define ZPOINTH(p) ((p).x)
#define ZPOINTV(p) ((p).y)
#define ZRECTTOP(p) ((p).top)
#define ZRECTBOTTOM(p) ((p).bottom)
#define ZRECTLEFT(p) ((p).left)
#define ZRECTRIGHT(p) ((p).right)

typedef struct windowstruct ** WINHANDLE;
typedef struct resourcestruct ** RESHANDLE;
typedef struct pixmapstruct ** PIXHANDLE;
#define GUIHANDLESDEFINED

typedef void (*WINCALLBACK)(WINHANDLE, UCHAR *, INT);
typedef void (*RESCALLBACK)(RESHANDLE, UCHAR *, INT);

/* create window style definitions */
#define WINDOW_STYLE_SIZEABLE			0x0001
#define WINDOW_STYLE_MAXIMIZE			0x0002
#define WINDOW_STYLE_NOCLOSEBUTTON		0x0004
#define WINDOW_STYLE_AUTOSCROLL			0x0008
#define WINDOW_STYLE_DEBUG				0x0010
#define WINDOW_STYLE_CENTERED			0x0020
#define WINDOW_STYLE_PANDLGSCALE		0x0040
/*#define WINDOW_STYLE_FULLSCREEN			0x0080*/
#define WINDOW_STYLE_FLOATWINDOW		0x0080
#define WINDOW_STYLE_NOTASKBARBUTTON	0x0100
#define WINDOW_STYLE_NOFOCUS			0x0200
#define WINDOW_STYLE_STATUSBAR			0x0400

 /* define some limits */
 /* The maximum length of a resource name in characters (not necessarily in bytes) */
 /* and not including the terminating null. */
#define MAXRESNAMELEN			8
#define MAXBOXTABS				50

/* DRAW line type parameter definitions */

#define DRAW_LINE_SOLID			1
#define DRAW_LINE_DOT			2
#define DRAW_LINE_DASH			3
#define DRAW_LINE_DASHDOT		4
#define DRAW_LINE_INVERT			5
#define DRAW_LINE_REVDOT			6

/* DRAW text alignment definitions */

#define DRAW_TEXT_LEFT			1
#define DRAW_TEXT_CENTER		2
#define DRAW_TEXT_RIGHT		3

/* RESOURCE creation definitions */

#define PANEL_END				1
#define PANEL_ICON				2
#define PANEL_BOX				3
#define PANEL_BOXTITLE			4
#define PANEL_TEXT				5
#define PANEL_EDIT				6
#define PANEL_CHECKBOX			7
#define PANEL_BUTTON			8
#define PANEL_RADIOBUTTON		9
#define PANEL_LASTRADIOBUTTON	10
#define PANEL_PUSHBUTTON		11
#define PANEL_DEFPUSHBUTTON		12
#define PANEL_ESCPUSHBUTTON		13
#define PANEL_LOCKBUTTON		14
#define PANEL_ICONLOCKBUTTON	15
#define PANEL_ICONPUSHBUTTON	16
#define PANEL_ICONDEFPUSHBUTTON	17
#define PANEL_ICONESCPUSHBUTTON	18
#define PANEL_LISTBOX			19
#define PANEL_DROPBOX			20
#define PANEL_HSCROLLBAR		21
#define PANEL_VSCROLLBAR		22
#define PANEL_PROGRESSBAR		23
#define PANEL_RTEXT				24
#define PANEL_VTEXT				25
#define PANEL_RVTEXT			26
#define PANEL_CTEXT				27
#define PANEL_BOXTABS			28
#define PANEL_FEDIT				29
#define PANEL_MEDIT				30
#define PANEL_PEDIT				31
#define PANEL_TAB				32
#define PANEL_LEDIT				33
#define PANEL_MLEDIT			34
#define PANEL_MLISTBOX			35
#define PANEL_PLEDIT			36
#define PANEL_TREE				37
#define PANEL_LISTBOXHS			38
#define PANEL_MLISTBOXHS		39
#define PANEL_MLEDITHS			40
#define PANEL_MLEDITVS			41
#define PANEL_MLEDITS			42
#define PANEL_MEDITHS			43
#define PANEL_MEDITVS			44
#define PANEL_MEDITS			45
#define PANEL_HELPTEXT			46
#define PANEL_CVTEXT			47
#define PANEL_VICON				48
#define PANEL_LTCHECKBOX		49
#define PANEL_POPBOX            50
#define PANEL_CDROPBOX          51  /* 'Common' dropbox, used only in tables */
#define PANEL_TABLE             52
#define TOOLBAR_END				PANEL_END
#define TOOLBAR_PUSHBUTTON		PANEL_PUSHBUTTON
#define TOOLBAR_LOCKBUTTON		PANEL_LOCKBUTTON
#define TOOLBAR_DROPBOX			PANEL_DROPBOX
#define TOOLBAR_SPACE			65
#define TOOLBAR_GRAY			66

/* define resource types */

#define RES_TYPE_NONE			0
#define RES_TYPE_MENU			1
#define RES_TYPE_PANEL			2
#define RES_TYPE_DIALOG			3
#define RES_TYPE_TOOLBAR		4
#define RES_TYPE_ICON			5
#define RES_TYPE_POPUPMENU		6
#define RES_TYPE_OPENFILEDLG	7
#define RES_TYPE_SAVEASFILEDLG	8
#define RES_TYPE_FONTDLG		9
#define RES_TYPE_COLORDLG		10
#define RES_TYPE_OPENDIRDLG     11
#define RES_TYPE_ALERTDLG       12

#define GUIRESCODE_MENU				RES_TYPE_MENU
#define GUIRESCODE_PANEL			RES_TYPE_PANEL
#define GUIRESCODE_DIALOG			RES_TYPE_DIALOG
#define GUIRESCODE_TOOLBAR			RES_TYPE_TOOLBAR
#define GUIRESCODE_ICON				RES_TYPE_ICON
#define GUIRESCODE_POPUPMENU		RES_TYPE_POPUPMENU
#define GUIRESCODE_OPENFILEDLG		RES_TYPE_OPENFILEDLG
#define GUIRESCODE_SAVEASFILEDLG	RES_TYPE_SAVEASFILEDLG
#define GUIRESCODE_FONTDLG			RES_TYPE_FONTDLG
#define GUIRESCODE_COLORDLG			RES_TYPE_COLORDLG
#define GUIRESCODE_OPENDIRDLG		RES_TYPE_OPENDIRDLG
#define GUIRESCODE_ALERTDLG			RES_TYPE_ALERTDLG

#define GUIMENUCODE_END				1
#define GUIMENUCODE_MAIN			2
#define GUIMENUCODE_ITEM			3
#define GUIMENUCODE_SUBMENU			4
#define GUIMENUCODE_ENDSUBMENU		5
#define GUIMENUCODE_KEY				6
#define GUIMENUCODE_LINE			7
#define GUIMENUCODE_GRAY			8
#define GUIMENUCODE_ICONITEM        9
#define GUIMENUCODE_ICONSUBMENU     10

#define GUIPANDLGCODE_LOWNOCONTROL	900
#define GUIPANDLGCODE_END			900
#define GUIPANDLGCODE_TITLE			901
#define GUIPANDLGCODE_SIZE			902
#define GUIPANDLGCODE_NOCLOSEBUTTON 903
#define GUIPANDLGCODE_BUTTONGROUP	904
#define GUIPANDLGCODE_ENDBUTTONGROUP 905
#define GUIPANDLGCODE_INSERTORDER	906
#define GUIPANDLGCODE_GRAY			907
#define GUIPANDLGCODE_SHOWONLY		908
#define GUIPANDLGCODE_TABGROUP		909
#define GUIPANDLGCODE_TABGROUPEND	910
#define GUIPANDLGCODE_FONT			911
#define GUIPANDLGCODE_TEXTCOLOR		912
#define GUIPANDLGCODE_READONLY		913
#define GUIPANDLGCODE_BOXTABS		914
#define GUIPANDLGCODE_NOFOCUS		915
#define GUIPANDLGCODE_TBL_COLTITLE  916		/* Defining a table column title */
#define GUIPANDLGCODE_TBL_COLUMN    917		/* Defining other table column attributes */
#define GUIPANDLGCODE_TBL_INSERTORDER 918
#define GUIPANDLGCODE_TBL_JUSTIFY  919
#define GUIPANDLGCODE_JUSTIFY      920
#define GUIPANDLGCODE_TBL_NOHEADER 921

#define GUIPANDLGCODE_ICON			PANEL_ICON
#define GUIPANDLGCODE_VICON			PANEL_VICON
#define GUIPANDLGCODE_BOX			PANEL_BOX
#define GUIPANDLGCODE_BOXTITLE		PANEL_BOXTITLE
#define GUIPANDLGCODE_TEXT			PANEL_TEXT
#define GUIPANDLGCODE_EDIT			PANEL_EDIT
#define GUIPANDLGCODE_LTCHECKBOX	PANEL_LTCHECKBOX
#define GUIPANDLGCODE_CHECKBOX		PANEL_CHECKBOX
#define GUIPANDLGCODE_PUSHBUTTON	PANEL_PUSHBUTTON
#define GUIPANDLGCODE_DEFPUSHBUTTON PANEL_DEFPUSHBUTTON
#define GUIPANDLGCODE_ESCPUSHBUTTON PANEL_ESCPUSHBUTTON
#define GUIPANDLGCODE_LOCKBUTTON	PANEL_LOCKBUTTON
#define GUIPANDLGCODE_ICONLOCKBUTTON PANEL_ICONLOCKBUTTON
#define GUIPANDLGCODE_ICONPUSHBUTTON PANEL_ICONPUSHBUTTON
#define GUIPANDLGCODE_ICONDEFPUSHBUTTON PANEL_ICONDEFPUSHBUTTON
#define GUIPANDLGCODE_ICONESCPUSHBUTTON PANEL_ICONESCPUSHBUTTON
#define GUIPANDLGCODE_BUTTON		PANEL_BUTTON
#define GUIPANDLGCODE_LISTBOX		PANEL_LISTBOX
#define GUIPANDLGCODE_MLISTBOX		PANEL_MLISTBOX
#define GUIPANDLGCODE_DROPBOX		PANEL_DROPBOX
#define GUIPANDLGCODE_HSCROLLBAR	PANEL_HSCROLLBAR
#define GUIPANDLGCODE_VSCROLLBAR	PANEL_VSCROLLBAR
#define GUIPANDLGCODE_PROGRESSBAR	PANEL_PROGRESSBAR
#define GUIPANDLGCODE_CTEXT			PANEL_CTEXT
#define GUIPANDLGCODE_RTEXT			PANEL_RTEXT
#define GUIPANDLGCODE_VTEXT			PANEL_VTEXT
#define GUIPANDLGCODE_RVTEXT		PANEL_RVTEXT
#define GUIPANDLGCODE_MEDIT			PANEL_MEDIT
#define GUIPANDLGCODE_LOCKBUTTON	PANEL_LOCKBUTTON
#define GUIPANDLGCODE_FEDIT			PANEL_FEDIT
#define GUIPANDLGCODE_PROGRESSBAR	PANEL_PROGRESSBAR
#define GUIPANDLGCODE_PEDIT			PANEL_PEDIT
#define GUIPANDLGCODE_TAB			PANEL_TAB
#define GUIPANDLGCODE_LEDIT			PANEL_LEDIT
#define GUIPANDLGCODE_MLEDIT		PANEL_MLEDIT
#define GUIPANDLGCODE_PLEDIT		PANEL_PLEDIT
#define GUIPANDLGCODE_TREE			PANEL_TREE
#define GUIPANDLGCODE_LISTBOXHS		PANEL_LISTBOXHS
#define GUIPANDLGCODE_MLISTBOXHS	PANEL_MLISTBOXHS
#define GUIPANDLGCODE_MLEDITHS		PANEL_MLEDITHS
#define GUIPANDLGCODE_MLEDITVS		PANEL_MLEDITVS
#define GUIPANDLGCODE_MLEDITS		PANEL_MLEDITS
#define GUIPANDLGCODE_MEDITHS		PANEL_MEDITHS
#define GUIPANDLGCODE_MEDITVS		PANEL_MEDITVS
#define GUIPANDLGCODE_MEDITS		PANEL_MEDITS
#define GUIPANDLGCODE_HELPTEXT		PANEL_HELPTEXT
#define GUIPANDLGCODE_CVTEXT		PANEL_CVTEXT
#define GUIPANDLGCODE_POPBOX        PANEL_POPBOX
#define GUIPANDLGCODE_TABLE         PANEL_TABLE

#define GUITOOLBARCODE_END			TOOLBAR_END
#define GUITOOLBARCODE_PUSHBUTTON	TOOLBAR_PUSHBUTTON
#define GUITOOLBARCODE_LOCKBUTTON	TOOLBAR_LOCKBUTTON
#define GUITOOLBARCODE_DROPBOX		TOOLBAR_DROPBOX
#define GUITOOLBARCODE_SPACE		TOOLBAR_SPACE
#define GUITOOLBARCODE_GRAY			TOOLBAR_GRAY

/* defines for key values */
/* NOTE: the defines in vid.h conflict with defines in Windows standard include */
#define KEYENTER       256  /* enter */
#define KEYESCAPE      257  /* escape */
#define KEYBKSPC       258  /* backspace */
#define KEYTAB         259  /* tab */
#define KEYBKTAB       260  /* back tab */

#define KEYUP          261  /* up arrow */
#define KEYDOWN        262  /* down arrow */
#define KEYLEFT        263  /* left arrow */
#define KEYRIGHT       264  /* right arrow */
#define KEYINSERT      265  /* insert */
#define KEYDELETE      266  /* delete */
#define KEYHOME        267  /* home */
#define KEYEND         268  /* end */
#define KEYPGUP        269  /* page up */
#define KEYPGDN        270  /* page down */

#define KEYSHIFTUP     271  /* shift-up arrow */
#define KEYSHIFTDOWN   272  /* shift-down arrow */
#define KEYSHIFTLEFT   273  /* shift-left arrow */
#define KEYSHIFTRIGHT  274  /* shift-right arrow */
#define KEYSHIFTINSERT 275  /* shift-insert */
#define KEYSHIFTDELETE 276  /* shift-delete */
#define KEYSHIFTHOME   277  /* shift-home */
#define KEYSHIFTEND    278  /* shift-end */
#define KEYSHIFTPGUP   279  /* shift-page up */
#define KEYSHIFTPGDN   280  /* shift-page down */

#define KEYCTLUP       281  /* control-up arrow */
#define KEYCTLDOWN     282  /* control-down arrow */
#define KEYCTLLEFT     283  /* control-left arrow */
#define KEYCTLRIGHT    284  /* control-right arrow */
#define KEYCTLINSERT   285  /* control-insert */
#define KEYCTLDELETE   286  /* control-insert */
#define KEYCTLHOME     287  /* control-home */
#define KEYCTLEND      288  /* control-end */
#define KEYCTLPGUP     289  /* control-page up */
#define KEYCTLPGDN     290  /* control-page down */

#define KEYALTUP       291  /* alt-up arrow */
#define KEYALTDOWN     292  /* alt-down arrow */
#define KEYALTLEFT     293  /* alt-left arrow */
#define KEYALTRIGHT    294  /* alt-right arrow */
#define KEYALTINSERT   295  /* alt-insert */
#define KEYALTDELETE   296  /* alt-insert */
#define KEYALTHOME     297  /* alt-home */
#define KEYALTEND      298  /* alt-end */
#define KEYALTPGUP     299  /* alt-page up */
#define KEYALTPGDN     300  /* alt-page down */

#define KEYF1          301  /* F1 */
#define KEYF2          302  /* F2 */
#define KEYF3          303  /* F3 */
#define KEYF4          304  /* F4 */
#define KEYF5          305  /* F5 */
#define KEYF6          306  /* F6 */
#define KEYF7          307  /* F7 */
#define KEYF8          308  /* F8 */
#define KEYF9          309  /* F9 */
#define KEYF10         310  /* F10 */
#define KEYF11         311  /* F11 */
#define KEYF12         312  /* F12 */
#define KEYF13         313  /* F13 */
#define KEYF14         314  /* F14 */
#define KEYF15         315  /* F15 */
#define KEYF16         316  /* F16 */
#define KEYF17         317  /* F17 */
#define KEYF18		   318  /* F18 */
#define KEYF19		   319  /* F19 */
#define KEYF20         320  /* F20 */

#define KEYSHIFTF1     321  /* shift-F1 */
#define KEYSHIFTF2     322  /* shift-F2 */
#define KEYSHIFTF3     323  /* shift-F3 */
#define KEYSHIFTF4     324  /* shift-F4 */
#define KEYSHIFTF5     325  /* shift-F5 */
#define KEYSHIFTF6     326  /* shift-F6 */
#define KEYSHIFTF7     327  /* shift-F7 */
#define KEYSHIFTF8     328  /* shift-F8 */
#define KEYSHIFTF9     329  /* shift-F9 */
#define KEYSHIFTF10    330  /* shift-F10 */
#define KEYSHIFTF11    331  /* shift-F11 */
#define KEYSHIFTF12    332  /* shift-F12 */
#define KEYSHIFTF13    333  /* shift-F13 */
#define KEYSHIFTF14    334  /* shift-F14 */
#define KEYSHIFTF15    335  /* shift-F15 */
#define KEYSHIFTF16    336  /* shift-F16 */
#define KEYSHIFTF17    337  /* shift-F17 */
#define KEYSHIFTF18    338  /* shift-F18 */
#define KEYSHIFTF19    339  /* shift-F19 */
#define KEYSHIFTF20    340  /* shift-F20 */

#define KEYCTLF1       341  /* control-F1 */
#define KEYCTLF2       342  /* control-F2 */
#define KEYCTLF3       343  /* control-F3 */
#define KEYCTLF4       344  /* control-F4 */
#define KEYCTLF5       345  /* control-F5 */
#define KEYCTLF6       346  /* control-F6 */
#define KEYCTLF7       347  /* control-F7 */
#define KEYCTLF8       348  /* control-F8 */
#define KEYCTLF9       349  /* control-F9 */
#define KEYCTLF10      350  /* control-F10 */
#define KEYCTLF11      351  /* control-F11 */
#define KEYCTLF12      352  /* control-F12 */
#define KEYCTLF13      353  /* control-F13 */
#define KEYCTLF14      354  /* control-F14 */
#define KEYCTLF15      355  /* control-F15 */
#define KEYCTLF16      356  /* control-F16 */
#define KEYCTLF17      357  /* control-F17 */
#define KEYCTLF18      358  /* control-F18 */
#define KEYCTLF19      359  /* control-F19 */
#define KEYCTLF20      360  /* control-F20 */

#define KEYALTF1       361  /* alt-F1 */
#define KEYALTF2       362  /* alt-F1 */
#define KEYALTF3       363  /* alt-F1 */
#define KEYALTF4       364  /* alt-F1 */
#define KEYALTF5       365  /* alt-F1 */
#define KEYALTF6       366  /* alt-F1 */
#define KEYALTF7       367  /* alt-F1 */
#define KEYALTF8       368  /* alt-F1 */
#define KEYALTF9       369  /* alt-F1 */
#define KEYALTF10      370  /* alt-F1 */
#define KEYALTF11      371  /* alt-F1 */
#define KEYALTF12      372  /* alt-F1 */
#define KEYALTF13      373  /* alt-F1 */
#define KEYALTF14      374  /* alt-F1 */
#define KEYALTF15      375  /* alt-F1 */
#define KEYALTF16      376  /* alt-F1 */
#define KEYALTF17      377  /* alt-F1 */
#define KEYALTF18      378  /* alt-F1 */
#define KEYALTF19      379  /* alt-F1 */
#define KEYALTF20      380  /* alt-F20 */

#define KEYALTA        381  /* alt-A */
#define KEYALTB        382  /* alt-B */
#define KEYALTC        383  /* alt-C */
#define KEYALTD        384  /* alt-D */
#define KEYALTE        385  /* alt-E */
#define KEYALTF        386  /* alt-F */
#define KEYALTG        387  /* alt-G */
#define KEYALTH        388  /* alt-H */
#define KEYALTI        389  /* alt-I */
#define KEYALTJ        390  /* alt-J */
#define KEYALTK        391  /* alt-K */
#define KEYALTL        392  /* alt-L */
#define KEYALTM        393  /* alt-M */
#define KEYALTN        394  /* alt-N */
#define KEYALTO        395  /* alt-O */
#define KEYALTP        396  /* alt-P */
#define KEYALTQ        397  /* alt-Q */
#define KEYALTR        398  /* alt-R */
#define KEYALTS        399  /* alt-S */
#define KEYALTT        400  /* alt-T */
#define KEYALTU        401  /* alt-U */
#define KEYALTV        402  /* alt-V */
#define KEYALTW        403  /* alt-W */
#define KEYALTX        404  /* alt-X */
#define KEYALTY        405  /* alt-Y */
#define KEYALTZ        406  /* alt-Z */

#define KEYCTLSHFTF1   520  /* control-shift-F1 */
#define KEYCTLSHFTF2   521  /* control-shift-F2 */
#define KEYCTLSHFTF3   522  /* control-shift-F3 */
#define KEYCTLSHFTF4   523  /* control-shift-F4 */
#define KEYCTLSHFTF5   524  /* control-shift-F5 */
#define KEYCTLSHFTF6   525  /* control-shift-F6 */
#define KEYCTLSHFTF7   526  /* control-shift-F7 */
#define KEYCTLSHFTF8   527  /* control-shift-F8 */
#define KEYCTLSHFTF9   528  /* control-shift-F9 */
#define KEYCTLSHFTF10  529  /* control-shift-F10 */
#define KEYCTLSHFTF11  530  /* control-shift-F11 */
#define KEYCTLSHFTF12  531  /* control-shift-F12 */
#define KEYCTLSHFTF13  532  /* control-shift-F13 */
#define KEYCTLSHFTF14  533  /* control-shift-F14 */
#define KEYCTLSHFTF15  534  /* control-shift-F15 */
#define KEYCTLSHFTF16  535  /* control-shift-F16 */
#define KEYCTLSHFTF17  536  /* control-shift-F17 */
#define KEYCTLSHFTF18  537  /* control-shift-F18 */
#define KEYCTLSHFTF19  538  /* control-shift-F19 */
#define KEYCTLSHFTF20  539  /* control-shift-F20 */

#define KEYCTLSHFTA    540  /* Ctrl-shift-A */
#define KEYCTLSHFTB    541  /* Ctrl-shift-B */
#define KEYCTLSHFTC    542  /* Ctrl-shift-C */
#define KEYCTLSHFTD    543  /* Ctrl-shift-D */
#define KEYCTLSHFTE    544  /* Ctrl-shift-E */
#define KEYCTLSHFTF    545  /* Ctrl-shift-F */
#define KEYCTLSHFTG    546  /* Ctrl-shift-G */
#define KEYCTLSHFTH    547  /* Ctrl-shift-H */
#define KEYCTLSHFTI    548  /* Ctrl-shift-I */
#define KEYCTLSHFTJ    549  /* Ctrl-shift-J */
#define KEYCTLSHFTK    550  /* Ctrl-shift-K */
#define KEYCTLSHFTL    551  /* Ctrl-shift-L */
#define KEYCTLSHFTM    552  /* Ctrl-shift-M */
#define KEYCTLSHFTN    553  /* Ctrl-shift-N */
#define KEYCTLSHFTO    554  /* Ctrl-shift-O */
#define KEYCTLSHFTP    555  /* Ctrl-shift-P */
#define KEYCTLSHFTQ    556  /* Ctrl-shift-Q */
#define KEYCTLSHFTR    557  /* Ctrl-shift-R */
#define KEYCTLSHFTS    558  /* Ctrl-shift-S */
#define KEYCTLSHFTT    559  /* Ctrl-shift-T */
#define KEYCTLSHFTU    560  /* Ctrl-shift-U */
#define KEYCTLSHFTV    561  /* Ctrl-shift-V */
#define KEYCTLSHFTW    562  /* Ctrl-shift-W */
#define KEYCTLSHFTX    563  /* Ctrl-shift-X */
#define KEYCTLSHFTY    564  /* Ctrl-shift-Y */
#define KEYCTLSHFTZ    565  /* Ctrl-shift-Z */

#define KEYCTRLA       601  /* Ctrl-A */
#define KEYCTRLB       602  /* Ctrl-B */
#define KEYCTRLC       603  /* Ctrl-C */
#define KEYCTRLD       604  /* Ctrl-D */
#define KEYCTRLE       605  /* Ctrl-E */
#define KEYCTRLF       606  /* Ctrl-F */
#define KEYCTRLG       607  /* Ctrl-G */
#define KEYCTRLH       608  /* Ctrl-H */
#define KEYCTRLI       609  /* Ctrl-I */
#define KEYCTRLJ       610  /* Ctrl-J */
#define KEYCTRLK       611  /* Ctrl-K */
#define KEYCTRLL       612  /* Ctrl-L */
#define KEYCTRLM       613  /* Ctrl-M */
#define KEYCTRLN       614  /* Ctrl-N */
#define KEYCTRLO       615  /* Ctrl-O */
#define KEYCTRLP       616  /* Ctrl-P */
#define KEYCTRLQ       617  /* Ctrl-Q */
#define KEYCTRLR       618  /* Ctrl-R */
#define KEYCTRLS       619  /* Ctrl-S */
#define KEYCTRLT       620  /* Ctrl-T */
#define KEYCTRLU       621  /* Ctrl-U */
#define KEYCTRLV       622  /* Ctrl-V */
#define KEYCTRLW       623  /* Ctrl-W */
#define KEYCTRLX       624  /* Ctrl-X */
#define KEYCTRLY       625  /* Ctrl-Y */
#define KEYCTRLZ 	   626  /* Ctrl-Z */

#define KEYCOMMA		630  /* Ctrl-, */
#define KEYPERIOD		631  /* Ctrl-. */
#define KEYFSLASH		632  /* Ctrl-/ */
#define KEYSEMICOLON	633  /* Ctrl-; */
#define KEYLBRACKET		634  /* Ctrl-[ */
#define KEYRBRACKET		635  /* Ctrl-] */
#define KEYBSLASH		636  /* Ctrl-\ */
#define KEYMINUS		637  /* Ctrl-- */
#define KEYEQUAL		638  /* Ctrl-= */
#define KEYQUOTE		639  /* Ctrl-' */

#define MAXKEYVALUE	   640
#if OS2
#define KEYINTERRUPT   505
#endif

/* DB/C message specifications */
#define DBC_MSGOK				"OK  "
#define DBC_MSGCANCEL			"CANC"
#define DBC_MSGCLOSE			"CLOS"
#define DBC_MSGITEM				"ITEM"
#define DBC_MSGPICK				"PICK"
#define DBC_MSGPUSH				"PUSH"
#define DBC_MSGMENU 			"MENU"
#define DBC_MSGPOSITION			"POSN"
#define DBC_MSGLEFTMBTNDOWN		"LBDN"
#define DBC_MSGLEFTMBTNUP		"LBUP"
#define DBC_MSGLEFTMBTNDBLCLK	"LBDC"
#define DBC_MSGMIDMBTNDOWN		"MBDN"
#define DBC_MSGMIDMBTNUP		"MBUP"
#define DBC_MSGMIDMBTNDBLCLK	"MBDC"
#define DBC_MSGRGHTMBTNDOWN		"RBDN"
#define DBC_MSGRGHTMBTNUP		"RBUP"
#define DBC_MSGRGHTMBTNDBLCLK	"RBDC"
#define DBC_MSGSCROLLUPLEFT		"SBA-"
#define DBC_MSGSCROLLDNRGHT		"SBA+"
#define DBC_MSGSCROLLPAGEUP		"SBP-"
#define DBC_MSGSCROLLPAGEDN		"SBP+"
#define DBC_MSGSCROLLTRKMVE		"SBTM"
#define DBC_MSGSCROLLTRKFNL   	"SBTF"
#define DBC_MSGHWINSCROLLLEFT	"HSA-"
#define DBC_MSGHWINSCROLLRGHT	"HSA+"
#define DBC_MSGHWINSCROLLPAGEUP	"HSP-"
#define DBC_MSGHWINSCROLLPAGEDN	"HSP+"
#define DBC_MSGHWINSCROLLTRKMVE	"HSTM"
#define DBC_MSGHWINSCROLLTRKFNL	"HSTF"
#define DBC_MSGVWINSCROLLUP		"VSA-"
#define DBC_MSGVWINSCROLLDN		"VSA+"
#define DBC_MSGVWINSCROLLPAGEUP	"VSP-"
#define DBC_MSGVWINSCROLLPAGEDN	"VSP+"
#define DBC_MSGVWINSCROLLTRKMVE	"VSTM"
#define DBC_MSGVWINSCROLLTRKFNL	"VSTF"
#define DBC_MSGWINCHAR			"CHAR"
#define DBC_MSGWINNKEY			"NKEY"
#define DBC_MSGWINACTIVATE		"WACT"
#define DBC_MSGWINMINIMIZE		"WMIN"
#define DBC_MSGWINSIZE			"WSIZ"
#define DBC_MSGWINPOS			"WPOS"
#define DBC_MSGPOSTOP			"POST"
#define DBC_MSGPOSBTM			"POSB"
#define DBC_MSGPOSLEFT			"POSL"
#define DBC_MSGPOSRIGHT			"POSR"
#define DBC_MSGFOCUS			"FOCS"
#define DBC_MSGLOSEFOCUS		"FOCL"
#define DBC_MSGEDITSELECT		"ESEL"
#define DBC_MSGOPEN             "OPEN"
#define DBC_MSGCLOSE			"CLOS"

#define MSG_NAMESIZE 8
#define MSG_FUNCSIZE 4
#define MSG_ITEMSIZE 5
#define MSG_HORZSIZE 5
#define MSG_VERTSIZE 5

#define MSG_NAMESTART 0
#define MSG_FUNCSTART 8
#define MSG_ITEMSTART 12
#define MSG_HORZSTART 17
#define MSG_VERTSTART 22
#define MSG_OTHERSTART 27

/* text color values */
#define TEXTCOLOR_DEFAULT 0
#define TEXTCOLOR_DARK_BLUE 1
#define TEXTCOLOR_DARK_GREEN 2
#define TEXTCOLOR_DARK_CYAN 3
#define TEXTCOLOR_DARK_RED 4
#define TEXTCOLOR_DARK_MAGENTA 5
#define TEXTCOLOR_DARK_YELLOW 6
#define TEXTCOLOR_LIGHT_GRAY 7
#define TEXTCOLOR_MEDIUM_GRAY 8
#define TEXTCOLOR_BLUE 9
#define TEXTCOLOR_GREEN 10
#define TEXTCOLOR_CYAN 11
#define TEXTCOLOR_RED 12
#define TEXTCOLOR_MAGENTA 13
#define TEXTCOLOR_YELLOW 14
#define TEXTCOLOR_WHITE 15
#define TEXTCOLOR_BLACK 16
#define TEXTCOLOR_CUSTOM 17

#define guimenustart(a, b, c) guiresstart(GUIRESCODE_MENU, a, (UCHAR *) b, c)
#define guipanelstart(a, b, c) guiresstart(GUIRESCODE_PANEL, a, (UCHAR *) b, c)
#define guialertdlgstart(a, b, c) guiresstart(GUIRESCODE_ALERTDLG, a, (UCHAR *) b, c)
#define guidialogstart(a, b, c) guiresstart(GUIRESCODE_DIALOG, a, (UCHAR *) b, c)
#define guitoolbarstart(a, b, c) guiresstart(GUIRESCODE_TOOLBAR, a, (UCHAR *) b, c)
#define guiiconstart(a, b, c) guiresstart(GUIRESCODE_ICON, a, (UCHAR *) b, c)
#define guipopupmenustart(a, b, c) guiresstart(GUIRESCODE_POPUPMENU, a, (UCHAR *) b, c)
#define guiopenfiledlgstart(a, b, c) guiresstart(GUIRESCODE_OPENFILEDLG, a, (UCHAR *) b, c)
#define guisaveasfiledlgstart(a, b, c) guiresstart(GUIRESCODE_SAVEASFILEDLG, a, (UCHAR *) b, c)
#define guifontdlgstart(a, b, c) guiresstart(GUIRESCODE_FONTDLG, a, (UCHAR *) b, c)
#define guicolordlgstart(a, b, c) guiresstart(GUIRESCODE_COLORDLG, a, (UCHAR *) b, c)
#define guiopendirdlgstart(a, b, c) guiresstart(GUIRESCODE_OPENDIRDLG, a, (UCHAR *) b, c)

#define guimenuend() guimenuprocess(GUIMENUCODE_END, 0, NULL, 0, NULL, 0)
#define guimenumain(a, b, c) guimenuprocess(GUIMENUCODE_MAIN, a, (UCHAR *) b, c, NULL, 0)
#define guimenuitem(a, b, c) guimenuprocess(GUIMENUCODE_ITEM, a, (UCHAR *) b, c, NULL, 0)
#define guimenuiconitem(a, b, c, d, e) guimenuprocess(GUIMENUCODE_ICONITEM, a, (UCHAR *) b, c, d, (UINT) e)
#define guimenusubmenu(a, b, c, d, e) guimenuprocess(GUIMENUCODE_SUBMENU, a, (UCHAR *) b, c, d, (UINT) e)
/*#define guimenuiconsubmenu(a, b, c, d, e) guimenuprocess(GUIMENUCODE_ICONSUBMENU, a, (UCHAR *) b, c, d, (UINT) e)*/
#define guimenuendsubmenu() guimenuprocess(GUIMENUCODE_ENDSUBMENU, 0, NULL, 0, NULL, 0)
#define guimenukey(a) guimenuprocess(GUIMENUCODE_KEY, a, NULL, 0, NULL, 0)
#define guimenuline() guimenuprocess(GUIMENUCODE_LINE, 0, NULL, 0, NULL, 0)
#define guimenugray() guimenuprocess(GUIMENUCODE_GRAY, 0, NULL, 0, NULL, 0)

#define guitoolbarend() guitoolbarprocess(GUITOOLBARCODE_END, 0, 0, 0, NULL, 0, NULL, 0)
#define guitoolbarpushbutton(item, icon, iconlen, text, textlen) guitoolbarprocess(GUITOOLBARCODE_PUSHBUTTON, item, 0, 0, icon, iconlen, text, textlen)
#define guitoolbarlockbutton(item, icon, iconlen, text, textlen) guitoolbarprocess(GUITOOLBARCODE_LOCKBUTTON, item, 0, 0, icon, iconlen, text, textlen)
#define guitoolbardropbox(item, hsize, vsize) guitoolbarprocess(GUITOOLBARCODE_DROPBOX, item, hsize, vsize, NULL, 0, NULL, 0)
#define guitoolbarspace() guitoolbarprocess(GUITOOLBARCODE_SPACE, 0, 0, 0, NULL, 0, NULL, 0)
#define guitoolbargray() guitoolbarprocess(GUITOOLBARCODE_GRAY, 0, 0, 0, NULL, 0, NULL, 0)

#define guipandlgend() guipandlgprocess(GUIPANDLGCODE_END, 0, NULL, 0, 0, 0)
#define guipandlgtitle(b, c) guipandlgprocess(GUIPANDLGCODE_TITLE, 0, (UCHAR *) b, c, 0, 0)
#define guipandlgsize(d, e) guipandlgprocess(GUIPANDLGCODE_SIZE, 0, 0, 0, d, e)
#define guipandlgnoclosebutton() guipandlgprocess(GUIPANDLGCODE_NOCLOSEBUTTON, 0, 0, 0, 0, 0)
#define guipandlgfont(b, c) guipandlgprocess(GUIPANDLGCODE_FONT, 0, (UCHAR *) b, c, 0, 0)
#define guipandlgtextcolor(b, c) guipandlgprocess(GUIPANDLGCODE_TEXTCOLOR, 0, (UCHAR *) b, c, 0, 0)
#define guipandlgicon(a, b, c) guipandlgprocess(a == 0 ? GUIPANDLGCODE_ICON : GUIPANDLGCODE_VICON, a, (UCHAR *) b, c, 0, 0)
#define guipandlgbox(d, e) guipandlgprocess(GUIPANDLGCODE_BOX, 0, NULL, 0, d, e)
#define guipandlgboxtitle(b, c, d, e) guipandlgprocess(GUIPANDLGCODE_BOXTITLE, 0, (UCHAR *) b, c, d, e)
#define guipandlgtext(b, c) guipandlgprocess(GUIPANDLGCODE_TEXT, 0, (UCHAR *) b, c, 0, 0)
#define guipandlgedit(a, b, c, d) guipandlgprocess(GUIPANDLGCODE_EDIT, a, (UCHAR *) b, c, d, 0)
#define guipandlgcheckbox(a, b, c) guipandlgprocess(GUIPANDLGCODE_CHECKBOX, a, (UCHAR *) b, c, 0, 0)
#define guipandlgltcheckbox(a, b, c) guipandlgprocess(GUIPANDLGCODE_LTCHECKBOX, a, (UCHAR *) b, c, 0, 0)
#define guipandlgpushbutton(a, b, c, d, e) guipandlgprocess(GUIPANDLGCODE_PUSHBUTTON, a, (UCHAR *) b, c, d, e)
#define guipandlgdefpushbutton(a, b, c, d, e) guipandlgprocess(GUIPANDLGCODE_DEFPUSHBUTTON, a, (UCHAR *) b, c, d, e)
#define guipandlgescpushbutton(a, b, c, d, e) guipandlgprocess(GUIPANDLGCODE_ESCPUSHBUTTON, a, (UCHAR *) b, c, d, e)
#define guipandlgiconlockbutton(a, b, c, d, e) guipandlgprocess(GUIPANDLGCODE_ICONLOCKBUTTON, a, (UCHAR *) b, c, d, e)
#define guipandlgiconpushbutton(a, b, c, d, e) guipandlgprocess(GUIPANDLGCODE_ICONPUSHBUTTON, a, (UCHAR *) b, c, d, e)
#define guipandlgicondefpushbutton(a, b, c, d, e) guipandlgprocess(GUIPANDLGCODE_ICONDEFPUSHBUTTON, a, (UCHAR *) b, c, d, e)
#define guipandlgiconescpushbutton(a, b, c, d, e) guipandlgprocess(GUIPANDLGCODE_ICONESCPUSHBUTTON, a, (UCHAR *) b, c, d, e)
#define guipandlgbutton(a, b, c) guipandlgprocess(GUIPANDLGCODE_BUTTON, a, (UCHAR *) b, c, 0, 0)
#define guipandlgbuttongroup() guipandlgprocess(GUIPANDLGCODE_BUTTONGROUP, 0, NULL, 0, 0, 0)
#define guipandlgendbuttongroup() guipandlgprocess(GUIPANDLGCODE_ENDBUTTONGROUP, 0, NULL, 0, 0, 0)
#define guipandlgtree(a, d, e) guipandlgprocess(GUIPANDLGCODE_TREE, a, NULL, 0, d, e)
#define guipandlglistbox(a, d, e) guipandlgprocess(GUIPANDLGCODE_LISTBOX, a, NULL, 0, d, e)
#define guipandlgmlistbox(a, d, e) guipandlgprocess(GUIPANDLGCODE_MLISTBOX, a, NULL, 0, d, e)
#define guipandlglistboxhs(a, d, e) guipandlgprocess(GUIPANDLGCODE_LISTBOXHS, a, NULL, 0, d, e)
#define guipandlgmlistboxhs(a, d, e) guipandlgprocess(GUIPANDLGCODE_MLISTBOXHS, a, NULL, 0, d, e)
#define guipandlginsertorder() guipandlgprocess(GUIPANDLGCODE_INSERTORDER, 0, NULL, 0, 0, 0)
#define guipandlgdropbox(a, d, e) guipandlgprocess(GUIPANDLGCODE_DROPBOX, a, NULL, 0, d, e)
#define guipandlghscrollbar(a, d, e) guipandlgprocess(GUIPANDLGCODE_HSCROLLBAR, a, NULL, 0, d, e)
#define guipandlgvscrollbar(a, d, e) guipandlgprocess(GUIPANDLGCODE_VSCROLLBAR, a, NULL, 0, d, e)
#define guipandlgprogressbar(a, d, e) guipandlgprocess(GUIPANDLGCODE_PROGRESSBAR, a, NULL, 0, d, e)
#define guipandlgctext(b, c, d) guipandlgprocess(GUIPANDLGCODE_CTEXT, 0, (UCHAR *) b, c, d, 0)
#define guipandlgrtext(b, c) guipandlgprocess(GUIPANDLGCODE_RTEXT, 0, (UCHAR *) b, c, 0, 0)
#define guipandlgvtext(a, b, c) guipandlgprocess(GUIPANDLGCODE_VTEXT, a, (UCHAR *) b, c, 0, 0)
#define guipandlgcvtext(a, b, c, d) guipandlgprocess(GUIPANDLGCODE_CVTEXT, a, (UCHAR *) b, c, d, 0)
#define guipandlgrvtext(a, b, c) guipandlgprocess(GUIPANDLGCODE_RVTEXT, a, (UCHAR *) b, c, 0, 0)
#define guipandlgmedit(a, b, c, d, e) guipandlgprocess(GUIPANDLGCODE_MEDIT, a, (UCHAR *) b, c, d, e)
#define guipandlgmediths(a, b, c, d, e) guipandlgprocess(GUIPANDLGCODE_MEDITHS, a, (UCHAR *) b, c, d, e)
#define guipandlgmeditvs(a, b, c, d, e) guipandlgprocess(GUIPANDLGCODE_MEDITVS, a, (UCHAR *) b, c, d, e)
#define guipandlgmedits(a, b, c, d, e) guipandlgprocess(GUIPANDLGCODE_MEDITS, a, (UCHAR *) b, c, d, e)
#define guipandlgpedit(a, b, c, d) guipandlgprocess(GUIPANDLGCODE_PEDIT, a, (UCHAR *) b, c, d, 0)
#define guipandlgledit(a, b, c, d, e) guipandlgmledit(a, (UCHAR *) b, c, d, 0, e, 0, 0)
#define guipandlggray() guipandlgprocess(GUIPANDLGCODE_GRAY, 0, NULL, 0, 0, 0)
#define guipandlgshowonly() guipandlgprocess(GUIPANDLGCODE_SHOWONLY, 0, NULL, 0, 0, 0)
#define guipandlgnofocus() guipandlgprocess(GUIPANDLGCODE_NOFOCUS, 0, NULL, 0, 0, 0)
#define guipandlgpaste() guipandlgprocess(GUIPANDLGCODE_PASTE, 0, NULL, 0, 0, 0)
#define guipandlgreadonly() guipandlgprocess(GUIPANDLGCODE_READONLY, 0, NULL, 0, 0, 0)
#define guipandlgtabgroup(d, e) guipandlgprocess(GUIPANDLGCODE_TABGROUP, 0, NULL, 0, d, e)
#define guipandlgtab(a, b, c) guipandlgprocess(GUIPANDLGCODE_TAB, a, (UCHAR *) b, c, 0, 0)
#define guipandlgtabgroupend() guipandlgprocess(GUIPANDLGCODE_TABGROUPEND, 0, NULL, 0, 0, 0)
#define guipandlghelptext(b, c) guipandlgprocess(GUIPANDLGCODE_HELPTEXT, 0, (UCHAR *) b, c, 0, 0)

#define IMAGEROTATE "imagerotate"
#define INITIAL_DRAW_TEXT_SIZE "12"

/* application defined message types */
#define GUIAM_SYNC					GUIAM_LOWMESSAGE
#define GUIAM_WINCREATE				(GUIAM_LOWMESSAGE + 1)
#define GUIAM_WINDESTROY 			(GUIAM_LOWMESSAGE + 2)
#define GUIAM_DLGCREATE				(GUIAM_LOWMESSAGE + 3)
#define GUIAM_DLGDESTROY			(GUIAM_LOWMESSAGE + 4)
#define GUIAM_SHOWCONTROLS			(GUIAM_LOWMESSAGE + 5)
#define GUIAM_HIDECONTROLS			(GUIAM_LOWMESSAGE + 6)
#define GUIAM_CREATEWINDOWEX		(GUIAM_LOWMESSAGE + 7)
#define GUIAM_SHUTDOWN				(GUIAM_LOWMESSAGE + 8)
#define GUIAM_WINSHOWCARET			(GUIAM_LOWMESSAGE + 9)
#define GUIAM_WINHIDECARET			(GUIAM_LOWMESSAGE + 10)
#define GUIAM_WINMOVECARET			(GUIAM_LOWMESSAGE + 11)
#define GUIAM_MENUCREATE			(GUIAM_LOWMESSAGE + 12)
#define GUIAM_MENUDESTROY			(GUIAM_LOWMESSAGE + 13)
#define GUIAM_POPUPMENUSHOW			(GUIAM_LOWMESSAGE + 14)
#if 0
#define GUIAM_SHOWCONTROLSINACTIVE	(GUIAM_LOWMESSAGE + 15)
#endif
#define GUIAM_WINACTIVATE 			(GUIAM_LOWMESSAGE + 16)
#define GUIAM_GETWINDOWTEXT			(GUIAM_LOWMESSAGE + 17)
#define GUIAM_STARTDRAW				(GUIAM_LOWMESSAGE + 18)
//#define GUIAM_GETTABITEMRECT		(GUIAM_LOWMESSAGE + 19) not used
#define GUIAM_TOOLBARCREATE			(GUIAM_LOWMESSAGE + 20)
#define GUIAM_SETTOOLBARTEXT		(GUIAM_LOWMESSAGE + 21)
#define GUIAM_TOOLBARDESTROY		(GUIAM_LOWMESSAGE + 22)
#define GUIAM_STATUSBARCREATE		(GUIAM_LOWMESSAGE + 23)
#define GUIAM_STATUSBARDESTROY		(GUIAM_LOWMESSAGE + 24)
#define GUIAM_STATUSBARTEXT			(GUIAM_LOWMESSAGE + 25)
#define GUIAM_SETCTRLFOCUS			(GUIAM_LOWMESSAGE + 26)
#define GUIAM_ADDMENUITEM			(GUIAM_LOWMESSAGE + 27)
#define GUIAM_DELETEMENUITEM		(GUIAM_LOWMESSAGE + 28)
#define GUIAM_STARTDOC				(GUIAM_LOWMESSAGE + 29)
#define GUIAM_CHOOSEFONT			(GUIAM_LOWMESSAGE + 30)
#define GUIAM_OPENDLG				(GUIAM_LOWMESSAGE + 35)
#define GUIAM_SAVEDLG				(GUIAM_LOWMESSAGE + 36)
#define GUIAM_CHOOSECOLOR			(GUIAM_LOWMESSAGE + 37)
#define GUIAM_TOOLBARCREATEDBOX		(GUIAM_LOWMESSAGE + 38)
#define GUIAM_HIDEPIX             	(GUIAM_LOWMESSAGE + 39)
#define GUIAM_GETSELECTEDTREEITEMTEXT (GUIAM_LOWMESSAGE + 40)
#define GUIAM_CHECKSCROLLBARS		(GUIAM_LOWMESSAGE + 41)
#define GUIAM_DIRDLG                (GUIAM_LOWMESSAGE + 42)
#define GUIAM_TRAYICON				(GUIAM_LOWMESSAGE + 43)
#define GUIAM_ERASEDROPLISTBOX		(GUIAM_LOWMESSAGE + 44)
#define GUIAM_SHOWPIX               (GUIAM_LOWMESSAGE + 45)
#define GUIAM_HIGHMESSAGE			(GUIAM_LOWMESSAGE + 46)



#define ISEDIT(pctl) ((pctl)->type == PANEL_EDIT || \
	(pctl)->type == PANEL_PEDIT || \
	(pctl)->type == PANEL_FEDIT || \
	(pctl)->type == PANEL_MEDIT || \
	(pctl)->type == PANEL_MEDITS || \
	(pctl)->type == PANEL_MEDITHS || \
	(pctl)->type == PANEL_MEDITVS || \
	(pctl)->type == PANEL_LEDIT || \
	(pctl)->type == PANEL_MLEDIT || \
	(pctl)->type == PANEL_MLEDITVS || \
	(pctl)->type == PANEL_MLEDITHS || \
	(pctl)->type == PANEL_MLEDITS || \
	(pctl)->type == PANEL_PLEDIT)
#define ISRADIO(pctl) ((pctl)->type == PANEL_RADIOBUTTON || \
	(pctl)->type == PANEL_LASTRADIOBUTTON)
#define ISGRAY(pctl) ((pctl)->style & CTRLSTYLE_DISABLED)
#define ISSHOWONLY(pctl) ((pctl)->style & CTRLSTYLE_SHOWONLY)
#define ISNOFOCUS(pctl) ((pctl)->style & CTRLSTYLE_NOFOCUS)
#define ISMARKED(pctl) ((pctl)->style & CTRLSTYLE_MARKED)

/*
 * The control types in the below are allowed to have
 * an initially NOFOCUS in the prep
 */
#define CANNOFOCUS(pctl) (ISEDIT(pctl) || \
		(pctl)->type == PANEL_ICONPUSHBUTTON || \
		(pctl)->type == PANEL_ICONDEFPUSHBUTTON || \
		(pctl)->type == PANEL_DEFPUSHBUTTON || \
		(pctl)->type == PANEL_PUSHBUTTON || \
		(pctl)->type == PANEL_DROPBOX || \
		(pctl)->type == PANEL_LISTBOX || \
		(pctl)->type == PANEL_LISTBOXHS || \
		(pctl)->type == PANEL_MLISTBOX || \
		(pctl)->type == PANEL_MLISTBOXHS || \
		(pctl)->type == PANEL_CHECKBOX || \
		(pctl)->type == PANEL_LTCHECKBOX || \
		(pctl)->type == PANEL_HSCROLLBAR || \
		(pctl)->type == PANEL_VSCROLLBAR || \
		(pctl)->type == PANEL_POPBOX || \
		(pctl)->type == PANEL_TREE)

/**
 * CONTROL structures are allocated from heap memory via guiAllocMem
 * They should be freed via guiFreeMem
 * 'struct controlstruct' is defined in guix.h
 */
typedef struct controlstruct CONTROL;

#if OS_WIN32GUI
COLORREF GetColorefFromColorNumber(INT colornum);
#endif

#if OS_WIN32
static struct _guiFontParseControl {
	/*
	 * The 'initial...' fields are set at SC startup, to defaults or to dbcdx.gui.ctlfont
	 */
	CHAR initialFontName[64];
	INT initialBold;
	INT initialItalic;
	INT initialUnderline;
	INT initialSize;
	INT initialDrawSize;

	/*
	 * The 'current...' fields are set from the 'initial...' fields at the start of a prep or draw
	 */
	CHAR currentFontName[64];
	INT currentBold;
	INT currentItalic;
	INT currentUnderline;
	INT currentSize;
} guiFontParseControl;
INT parseFontString(CHAR* font, LOGFONT* lf, HDC hdc);
INT guiCSCinit(INT breakeventid, struct _guiFontParseControl *fontStruct);
#endif

typedef enum {
	EBPS_DEFAULT,
	EBPS_EARLY,
	EBPS_LATE
} erase_background_processing_style_t;

INT guiabout(CHAR *, CHAR *);
ZHANDLE guiAllocMem(INT);
ZHANDLE guiAllocString(UCHAR *, INT);
INT guiappchange(UCHAR *, INT, UCHAR *, INT);
INT guiAppDestroy(void);
void guibeep(INT, INT, INT);
INT guicbdbpp(INT *);
INT guicbdgetpixmap(PIXHANDLE);
INT guicbdgettext(CHAR *text, size_t *cbText);
INT guicbdgettextlen(size_t *cbTextLen);
INT guicbdputpixmap(PIXHANDLE);
INT guicbdputtext(UCHAR *, INT);
INT guicbdres(INT *, INT *);
INT guicbdsize(INT *, INT *);
INT guicolordlgdefault(UCHAR *, INT);
INT guicolordlgend(void);
INT guiddechange(INT, UCHAR *, INT, UCHAR *, INT);
INT guiddestart(UCHAR *, void (*)(INT, UCHAR *, INT));
void guiddestop(INT);
void guiexecmain(void (*)(void));
INT guiexectask(UCHAR *, INT, INT, void (*)(CHAR *));
INT guiexit(INT);
INT guialertdlgtext(UCHAR *text, INT textlen);
INT guifiledlgdefault(UCHAR *, INT);
INT guifiledlgDeviceFilter(UCHAR *data, INT cbData);
INT guifiledlgMulti(void);
INT guifiledlgTitle(UCHAR *data, INT cbData);
INT guialertdlgend(void);
INT guifiledlgend(void);
INT guifontdlgdefault(UCHAR *, INT);
INT guifontdlgend(void);
INT guiFreeMem(ZHANDLE);
INT guigetcontrolsize(RESHANDLE, INT, INT *, INT *);
CHAR* guiGetLastErrorMessage ();
INT guiicon(RESHANDLE, UCHAR *, INT, INT, INT, INT, UCHAR *, INT);
INT guiiconhsize(INT);
INT guiiconvsize(INT);
INT guiiconcolorbits(INT);
INT guiiconpixels(UCHAR *, INT);
INT guiiconend(void);
INT guiinit(INT, CHAR *);
void guiInitTrayIcon(INT trapid);
INT guiListBoxAlphaFind(ZHANDLE itemarray, UINT arraysize, ZHANDLE targetstr, INT *poffset);
INT guiListBoxAppendFind(ZHANDLE itemarray, UINT arraysize, ZHANDLE targetstr, INT *poffset);
void *guiLockMem(ZHANDLE);
INT guimemicmp(void *, void *, INT);
void *guiMemToPtr(ZHANDLE);
INT guimenuprocess(INT, INT, UCHAR *, INT, CHAR*, UINT);
void guimsgbox(CHAR *, CHAR *);
INT guiMultiChange(RESHANDLE reshandle, UCHAR *data, INT datalen);
INT guipandlgboxtabs(INT *, CHAR *, INT);
INT guipandlgfedit(INT, UCHAR *, INT, UCHAR *, INT, INT);
INT guipandlgjustify(INT code);
INT guipandlgposh(INT);
INT guipandlgposv(INT);
INT guipandlgposha(INT);
INT guipandlgposva(INT);
INT guipandlgmledit(INT, UCHAR *, INT, INT, INT, INT, INT, INT);
INT guipandlgpledit(INT, UCHAR *, INT, INT, INT);
INT guipandlgpopbox(INT item, INT hsize);
INT guipandlgprocess(INT, INT, void *, INT, INT, INT);
INT guipandlgtable(INT item, INT hsize, INT vsize);
INT guipandlgtablecolumntitle(CHAR *string, INT stringlen);
INT guipandlgtablecolumn(INT colindex, INT item, INT width, INT type, INT extra, CHAR* mask, UINT masklen);
INT guipandlgtableinsertorder(void);
INT guipandlgtablejustify(INT code);
INT guipandlgtablenoheader(void);
ZHANDLE guiReallocMem(ZHANDLE, INT);
void guiresume(void);
INT guireschange(RESHANDLE, UCHAR *, INT, UCHAR *, UINT);
INT guireschange2(RESHANDLE reshandle, INT cmd, INT useritem, INT subuseritem, UCHAR *data, INT datalen);
INT guireschange3(RESHANDLE reshandle, INT cmd, UCHAR *data, INT datalen);
INT guirescreate(RESHANDLE *);
INT guiresdestroy(RESHANDLE);
INT guireshide(RESHANDLE);
INT guiresmsgfnc(RESHANDLE, RESCALLBACK);
INT guiresquery(RESHANDLE, UCHAR *, INT, UCHAR *, INT *);
LRESULT guiresshow(RESHANDLE, WINHANDLE, INT, INT);
INT guiresstart(INT, RESHANDLE, UCHAR * , INT);
INT guipixcreate(INT, INT, INT, PIXHANDLE *);
INT guipixdestroy(PIXHANDLE);
INT guipixshow(PIXHANDLE, WINHANDLE, INT, INT);
INT guipixhide(PIXHANDLE);
INT guijpgres(UCHAR *, INT *, INT *);
INT guipngres(UCHAR *, INT *, INT *);
INT guitifres(INT, UCHAR *, INT *, INT *);
INT guigifbpp(UCHAR *, INT *);
INT guijpgbpp(UCHAR *, INT *);
INT guipngbpp(UCHAR *, INT *);
ZHANDLE guiTableLocateRow(CONTROL *control, INT one_based_row_index);
INT guitifbpp(INT, UCHAR *, INT *);
INT guitifcount(UCHAR *, INT *);
INT guigifsize(UCHAR *, INT *, INT *);
INT guijpgsize(UCHAR *, INT *, INT *);
INT guipngsize(UCHAR *, INT *, INT *);
INT guitifsize(INT, UCHAR *, INT *, INT *);
INT guipixtotif(PIXHANDLE, UCHAR *, INT *);
INT guitiftopix(PIXHANDLE, UCHAR **);
INT guijpgtopix(PIXHANDLE, UCHAR **);
INT guigiftopix(PIXHANDLE, UCHAR **);
INT guipngtopix(PIXHANDLE, UCHAR **);
INT guipixdrawstart(PIXHANDLE);
void guipixdrawend(void);
void guipixdrawpos(INT, INT);
void guipixdrawhpos(INT);
void guipixdrawvpos(INT);
void guipixdrawhadj(INT);
void guipixdrawvadj(INT);
void guipixdrawcolor(INT red, INT green, INT blue);
void guipixdrawreplace(INT32, INT32);
void guipixdrawlinewidth(INT);
void guipixdrawlinetype(INT);
void guipixdrawerase(void);
void guipixdrawdot(INT);
void guipixdrawcircle(INT);
void guipixdrawline(INT, INT);
void guipixdrawrectangle(INT, INT);
void guipixdrawbox(INT, INT);
void guipixdrawtriangle(INT, INT, INT, INT);
void guipixdrawfont(UCHAR *, INT);
void guipixdrawatext(UCHAR *, INT, INT);
void guipixdrawtext(UCHAR *, INT, UINT);
void guipixdrawcopyimage(PIXHANDLE);
void guipixdrawclipimage(PIXHANDLE, INT, INT, INT, INT);
void guipixdrawstretchimage(PIXHANDLE, INT, INT);
void guipixdrawirotate(INT);
INT guipixgetcolor(PIXHANDLE, INT32 *, INT, INT);
INT guipixgetpos(PIXHANDLE, INT *, INT *);
INT guipixgetfontsize(PIXHANDLE, INT *, INT *);
void guisetbeepold(void);
void guisetcbcodepage(CHAR * codepage);
void guisetEditCaretOld(void);
void guisetfeditboxtextselectiononfocusold(void);
void guisetMinsertSeparator(CHAR *ptr);
void guisetUseTransparentBlt(INT value);
void guisetUseTerminateProcess(void);
void guisetTiffMaxStripSize(CHAR *ptr);
void guisetenterkey(CHAR *ptr);
void guisetfocusold(void);
void guiSetImageDrawStretchMode(CHAR *ptr);
void guisetlistinsertbeforeold(void);
void guisetBitbltErrorIgnore(void);
void guisetErasebkgndSpecial(erase_background_processing_style_t);
#if DX_MAJOR_VERSION >= 17
void guisetImageRotateOld(void);
#else
void guisetImageRotateNew(void);
#endif
void guisetRGBformatFor24BitColor();
void guisetscalexy(INT, INT);
void guisetUseEditBoxClientEdge(INT state);
void guisetUseListBoxClientEdge(INT state);
void guisuspend(void);
INT guitoolbarprocess(INT, INT, INT, INT, UCHAR *, INT, UCHAR *, INT);
void guiUnlockMem(ZHANDLE);
INT guiwincreate(CHAR *, CHAR *, INT, INT, INT, INT, INT, WINHANDLE *, CHAR *owner);
INT guiwindestroy(WINHANDLE);
INT guiwinmsgfnc(WINHANDLE, WINCALLBACK);
INT guiwinchange(WINHANDLE, UCHAR *, INT, UCHAR *, INT);

#endif /* _DBCGUI_INCLUDED */
