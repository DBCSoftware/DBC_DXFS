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

#define GUI_C
#define INC_STRING
#define INC_STDIO
#define INC_CTYPE
#define INC_STDLIB
#define INC_LIMITS
#include "includes.h"
#include "release.h"
#include "base.h"
#include "gui.h"
#include "guix.h"
#if OS_WIN32
#include "fontcache.h"
#include <strsafe.h>
#pragma warning(disable : 4995)
#endif

#define ISLASTRADIO(pctl) ((pctl)->type == PANEL_LASTRADIOBUTTON)
#define ISMULTISELLISTBOX(pctl) ((pctl)->type == PANEL_MLISTBOX || (pctl)->type == PANEL_MLISTBOXHS)
#define ISGRAY(pctl) ((pctl)->style & CTRLSTYLE_DISABLED)


#if OS_WIN32GUI
static WINHANDLE winhandlehdr = NULL;
static RESHANDLE reshandlehdr = NULL;
static RESHANDLE menureshandle;
static RESHANDLE pandlgreshandle;
static RESHANDLE toolbarreshandle;
static RESHANDLE iconreshandle;
static RESHANDLE filedlgreshandle;
static RESHANDLE fontdlgreshandle;
static RESHANDLE alertdlgreshandle;
static RESHANDLE colordlgreshandle;
static INT dlgcontrolposh, dlgcontrolposv;
static UCHAR *dlgcontrolfeditmask;
static INT dlgcontrolfeditmasklen;
static INT dlgcontrolleditmaxchars;
static INT *dlgcontrolboxtabs;
static INT dlgcontrolboxtabcount;
static CHAR *dlgcontrolboxjust;
static UINT lrufocus;
static INT firstresfontflag;
static INT tabgroupflag;
static INT scalex = 1, scaley = 1;
static CHAR minsertSeparator = ',';
static CHAR lastErrorMessage[128];

static void FreeMinsertArray(UCHAR** array, INT count);
static INT GetColorNumberFromText(UCHAR* data);
static INT GetColorNumberFromTextExt(UCHAR* data, INT datalen, COLORREF* cref, BOOL *defcolor);
static void clearLastErrorMessage();
static INT guireschangepnldlg(RESHANDLE reshandle, INT cmd, CONTROL *control, UCHAR *data, INT datalen);
static INT guiResChangeTable(RESHANDLE reshandle, INT cmd, CONTROL *control, UCHAR *data, INT datalen);
static void guiResChangeTableCDLocate(UINT row, CONTROL *control, USHORT index);
static INT guiResChangeTableColumnText(CONTROL *control, UCHAR *data, INT datalen);
static INT guiResChangeTableDBSanityChecks(INT cmd, CONTROL *control, UCHAR *data, INT datalen);
static void guiResChangeTableDeleteFinish(PTABLEDROPBOX dropbox, INT iDeletePos, INT ctype, PTABLECELL cell, CONTROL *control);
static INT guiResChangeTableInsert(CONTROL *control, CHAR *data, INT datalen);
static INT guireschangetoolbar(RESHANDLE reshandle, INT cmd, INT useritem, UCHAR *data, INT datalen);
static INT guireschangemenu(RESHANDLE reshandle, INT cmd, MENUENTRY *menuitem, UCHAR *data, INT datalen);
static INT guireschangeother(RESHANDLE reshandle, INT cmd, UCHAR *data, INT datalen);
static INT guireschangeControlTextStyle(INT cmd, CONTROL *control, UCHAR *data, INT datalen);
static void guiresdestroytable(CONTROL *control);
static INT guiresquerytable(RESOURCE *res, INT cmd, UCHAR *data, INT *datalen, CONTROL*);
static void guiresdestroytabledb(PTABLEDROPBOX dropbox);
static RESHANDLE locateIconByName(UCHAR* resname);
static INT num5toi(UCHAR *, INT *);
static void numtoi(UCHAR *, INT, INT *);
static INT parsefnc(UCHAR *fnc, INT fnclen, INT *useritem, INT *subuseritem);
static INT setControlTextColor(CONTROL *control, UCHAR *data, INT datalen);
static INT setControlTextFont(CONTROL *control, UCHAR *data);
static void setLastErrorMessage(CHAR *msg);
static UCHAR** SplitMinsertData(CHAR *data, INT datalen, INT* count);
#endif

PIXHANDLE pixhandlehdr = NULL;
static PIXHANDLE drawpixhandle = NULL;
static PIXMAP *drawpixptr;

/* define memory header/end protection values */
#define MEMBLKHDR 0x55667788
#define MEMBLKEND 0x76543210
static INT memmaxsize;		/* maximum number of bytes ever allocated by guiAllocMem or guiAllocString */

#if OS_WIN32
static int CALLBACK EnumFontFamExProc(ENUMLOGFONTEX *, NEWTEXTMETRICEX *, DWORD, LPARAM);
#endif

#if OS_WIN32 && !OS_WIN32GUI
INT guiddestart(UCHAR *ddename, void (*callback)(INT, UCHAR *, INT))
{
	return RC_ERROR;
}

void guiddestop(INT ddehandle)
{
}

INT guiddechange(INT ddehandle, UCHAR *fnc, INT fnclen, UCHAR *data, INT datalen)
{
	return RC_ERROR;
}
#endif

#if OS_WIN32
void guisetscalexy(INT scalenumerator, INT scaledenominator)
{
#if OS_WIN32GUI
	if (scalenumerator < 1 || scaledenominator < 1 || scalenumerator == scaledenominator) scalex = scaley = 1;
	else {
		scalex = scalenumerator;
		scaley = scaledenominator;
	}
#endif
}

void guisetErasebkgndSpecial(erase_background_processing_style_t v1)
{
#if OS_WIN32GUI
	guiaErasebkgndSpecial(v1);
#endif
}

#endif

void guisetfeditboxtextselectiononfocusold() {
#if OS_WIN32GUI
	guiaFeditTextSelectionOnFocusOld();
#else
	return;
#endif
}

void guisetEditCaretOld() {
#if OS_WIN32GUI
	guiaEditCaretOld();
#else
	return;
#endif
}


void guiSetImageDrawStretchMode(CHAR *ptr)
{
#if OS_WIN32GUI
	guiaSetImageDrawStretchMode(ptr);
#endif
}

void guisetenterkey(CHAR *ptr)
{
#if OS_WIN32GUI
	guiaEnterkey(ptr);
#endif
}

void guisetfocusold()
{
#if OS_WIN32GUI
	guiaFocusOld();
#endif
}

void guisetbeepold()
{
#if OS_WIN32GUI
	guiaBeepOld();
#endif
}

void guisetBitbltErrorIgnore()
{
#if OS_WIN32GUI
	guiaBitbltErrorIgnore();
#endif
}

void guisetUseEditBoxClientEdge(INT state)
{
#if OS_WIN32GUI
	guiaUseEditBoxClientEdge(state);
#endif
}

void guisetUseListBoxClientEdge(INT state)
{
#if OS_WIN32GUI
	guiaUseListBoxClientEdge(state);
#endif
}

/**
 * For DX >= 17 it will be called if dbcdx.gui.IMAGEROTATE=old
 */
#if DX_MAJOR_VERSION >= 17
void guisetImageRotateOld()
{
#if OS_WIN32GUI
	guiaSetImageRotateOld();
#endif
}
#endif

/**
 * For DX < 17 it will be called if dbcdx.gui.IMAGEROTATE=new
 */
#if DX_MAJOR_VERSION < 17
void guisetImageRotateNew()
{
#if OS_WIN32GUI
	guiaSetImageRotateNew();
#endif
}
#endif

/**
 * This call is valid in all build environments
 */
void guisetTiffMaxStripSize(CHAR *ptr)
{
	guiaTifMaxStripSize(ptr);
}

void guisetcbcodepage(CHAR* cp)
{
#if OS_WIN32GUI
	guiaClipboardCodepage(cp);
#endif
}

void guisetlistinsertbeforeold() {
#if OS_WIN32GUI
	guiaInsertbeforeOld();
#endif
}

void guisetMinsertSeparator(CHAR *ptr) {
#if OS_WIN32GUI
	long l1;
	if (isdigit(ptr[0])) {
		l1 = strtol(ptr, NULL, 0);
		minsertSeparator = (char) l1;
	}
	else minsertSeparator = ptr[0];
#endif
}

void guisetRGBformatFor24BitColor() {
	formatFor24BitColorIsRGB = TRUE;
}

void guisetUseTransparentBlt(INT value) {
#if OS_WIN32GUI
	guiaUseTransparentBlt(value);
#endif
}

void guisetUseTerminateProcess(void) {
#if OS_WIN32GUI
	guiaUseTerminateProcess();
#endif
}

/* initialization */
INT guiinit(INT breakeventid, CHAR *ctlfont)
{
#if OS_WIN32GUI
	return(guiaInit(breakeventid, ctlfont));
#else
	return 0;
#endif
}

/*
 * Special initialization just for use by functions in dbcsc.c
 */
#if OS_WIN32
INT guiCSCinit(INT breakeventid, struct _guiFontParseControl *fontStruct)
{
#if OS_WIN32GUI
	CHAR ctlfont[64];
	CHAR work[32];
	StringCbCopy(ctlfont, sizeof(ctlfont), fontStruct->initialFontName);
	StringCbCat(ctlfont, sizeof(ctlfont), "(");
	mscitoa(fontStruct->initialSize, work);
	StringCbCat(ctlfont, sizeof(ctlfont), work);
	StringCbCat(ctlfont, sizeof(ctlfont), ")");
	return(guiaInit(breakeventid, ctlfont));
#else
	return 0;
#endif
}
#endif

/**
 * Do nothing in non-gui build
 */
void guiInitTrayIcon(INT trapid)
{
#if OS_WIN32GUI
	guiaInitTrayIcon(trapid);
#endif
	return;
}

INT guiexit(INT exitvalue)  /* termination */
{
#if OS_WIN32GUI
	return(guiaExit(exitvalue));
#else
	return 0;
#endif
}

void guisuspend(void)  /* suspend gui message processing */
{
#if OS_WIN32GUI
	guiaSuspend();
#endif
}

void guiresume(void)  /* resume gui message processing */
{
#if OS_WIN32GUI
	guiaResume();
#endif
}

void guimsgbox(CHAR *msg1, CHAR *msg2)  /* display a message box with two lines */
{
#if OS_WIN32GUI
	ZHANDLE h1, h2;
	CHAR work[64];

	h1 = guiAllocString((UCHAR *) msg1, (INT) strlen(msg1));
	if (h1 == NULL) {
		sprintf(work, "guiAllocString returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, (INT) strlen(msg1));
		return;
	}
	h2 = guiAllocString((UCHAR *) msg2, (INT) strlen(msg2));
	if (h2 == NULL) {
		guiFreeMem(h1);
		sprintf(work, "guiAllocString returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, (INT) strlen(msg2));
		return;
	}
	guiaMsgBox(h1, h2);
	guiFreeMem(h1);
	guiFreeMem(h2);
#endif
}

void guibeep(INT pitch, INT tenths, INT beepflag)
{
#if OS_WIN32GUI
	guiaBeep(pitch, tenths, beepflag);
#endif
}

/*
 * create window
 * hpos and vpos are one-based
 * owner is optional and only used by a float window
 */
INT guiwincreate(CHAR *name, CHAR *title, INT style, INT hpos, INT vpos, INT hsize, INT vsize,
		WINHANDLE *winhandleptr, CHAR *owner)
{
#if OS_WIN32GUI
	INT rc;
	INT n;
	CHAR workname[MAXRESNAMELEN], work[64];
	WINHANDLE wh, wh1;
	WINDOW *win;

	if ((n = (INT) strlen(name)) > MAXRESNAMELEN) return RC_NAME_TOO_LONG;
	memset(workname, ' ', MAXRESNAMELEN);
	memcpy(workname, name, n);

	wh = (WINHANDLE) memalloc(sizeof(WINDOW), 0);
	if (wh == NULL) {
		return RC_NO_MEM;
	}
	win = *wh;
	memset(win, 0, sizeof(WINDOW));
	win->thiswinhandle = wh;
	memcpy(&win->name, workname, MAXRESNAMELEN);

	if (title == NULL) win->title = (ZHANDLE) NULL;
	else {
		win->title = guiAllocString((UCHAR *) title, (INT) strlen(title));
		if (win->title == NULL) {
			sprintf(work, "guiAllocString returned null for title in %s", __FUNCTION__);
			guiaErrorMsg(work, (INT) strlen(title));
			memfree((void *) wh);
			return RC_NO_MEM;
		}
	}

	if (owner == NULL || owner[0] == '\0' || !(style & WINDOW_STYLE_FLOATWINDOW))
		win->owner = (ZHANDLE) NULL;
	else {
		if ((n = (INT) strlen(owner)) > MAXRESNAMELEN) return RC_NAME_TOO_LONG;
		memset(workname, ' ', MAXRESNAMELEN);
		memcpy(workname, owner, n);
		for (wh1 = winhandlehdr; wh1 != NULL; wh1 = (*wh1)->nextwinhandle) {
			if ((*wh1)->floatwindow) continue;
			if (strncmp((*wh1)->name, workname, MAXRESNAMELEN) == 0) break;
		}
		if (wh1 == NULL) {
			if (win->title != NULL) guiFreeMem(win->title);
			memfree((void *) wh);
			return RC_ERROR;
		}

		win->ownerhwnd = (*wh1)->hwnd;
		win->owner = guiAllocString((UCHAR *) owner, (INT) strlen(owner));
		if (win->owner == NULL) {
			sprintf(work, "guiAllocString returned null for owner in %s", __FUNCTION__);
			guiaErrorMsg(work, (INT) strlen(owner));
			if (win->title != NULL) guiFreeMem(win->title);
			memfree((void *) wh);
			return RC_NO_MEM;
		}
	}

	ZPOINTH(win->caretpos) = ZPOINTV(win->caretpos) = 0;
	win->caretvsize = 20;
	win->caretonflg = FALSE;
	if (style & WINDOW_STYLE_PANDLGSCALE) {
		hsize = (hsize * scalex) / scaley;
		vsize = (vsize * scalex) / scaley;
	}
	if (rc = guiaWinCreate(wh, style, hpos, vpos, hsize, vsize)) {
		if (win->title != NULL) guiFreeMem(win->title);
		if (win->owner != NULL) guiFreeMem(win->owner);
		memfree((void *) wh);
		return(rc);
	}
	if (++lrufocus & 0x80000000) {
		for (wh1 = winhandlehdr; wh1 != NULL; wh1 = (*wh1)->nextwinhandle) {
			if ((*wh1)->lrufocus) {
				if (!((*wh1)->lrufocus >>= 1)) (*wh1)->lrufocus = 1;;
			}
		}
		lrufocus >>= 1;
	}
	if (!win->debugflg) win->lrufocus = lrufocus;
	win->nextwinhandle = winhandlehdr;
	*winhandleptr = winhandlehdr = wh;
	return 0;
#else
	return RC_ERROR;
#endif
}

/**
 * Destroy the 'application' device.
 * Its openinfo->devtype is DEV_APPLICATION
 */
INT guiAppDestroy()
{
#if OS_WIN32GUI
	guiaAppDestroy();
	return 0;
#else
	return RC_ERROR;
#endif
}

/**
 * Calls guireshide and guipixhide for all 'shown' resources
 * Frees memory for the window title. (malloc)
 * Destroys the window at the Win32 level.
 * Frees memory for the WINDOW structure. (memalloc)
 */
INT guiwindestroy(WINHANDLE winhandle)  /* destroy a window */
{
#if OS_WIN32GUI
	WINHANDLE wh, wh1;
	INT rc;

	for (wh = winhandlehdr; wh != NULL && wh != winhandle; wh = (*wh)->nextwinhandle) wh1 = wh;
	if (wh == NULL) return RC_INVALID_HANDLE;
	if (wh == winhandlehdr) winhandlehdr = (*wh)->nextwinhandle;
	else (*wh1)->nextwinhandle = (*wh)->nextwinhandle;
	if ((*wh)->showmenu != NULL) guireshide((*wh)->showmenu);
	if ((*wh)->showtoolbar != NULL) guireshide((*wh)->showtoolbar);
	while((*wh)->showreshdr != NULL) {
		rc = guireshide((*wh)->showreshdr);
		if (rc == RC_ERROR) return rc;
	}
	while ((*wh)->showpixhdr != NULL) guipixhide((*wh)->showpixhdr);
	if ((*wh)->title != NULL) guiFreeMem((*wh)->title);
	guiaWinDestroy(wh);
	memfree((void *) wh);
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guiwinmsgfnc(WINHANDLE winhandle, WINCALLBACK fnc)  /* callback function for window messages */
{
#if OS_WIN32GUI
	WINHANDLE wh;

	for (wh = winhandlehdr; wh != NULL && wh != winhandle; wh = (*wh)->nextwinhandle);
	if (wh == NULL) return RC_INVALID_HANDLE;
	(*wh)->cbfnc = fnc;
	return 0;
#else
	return RC_ERROR;
#endif
}

/**
 * change a window
 */
INT guiwinchange(WINHANDLE winhandle, UCHAR *fnc, INT fnclen, UCHAR *data, INT datalen)
{
#if OS_WIN32GUI
	INT i1, i2, i3;
	UINT i4;
	WINHANDLE wh;
	WINDOW *win;
	UCHAR resname[MAXRESNAMELEN];
	RESHANDLE iconrh;
	CHAR cmd[20], work[64];

	for (wh = winhandlehdr; wh != NULL && wh != winhandle; wh = (*wh)->nextwinhandle);
	if (wh == NULL) return RC_INVALID_HANDLE;
	if (fnc == NULL || fnclen < 1 || fnclen >= 20) return RC_INVALID_PARM;
	for (i1 = 0; i1 < fnclen; i1++) cmd[i1] = toupper(fnc[i1]);
	cmd[fnclen] = '\0';
	win = *wh;
	if ((cmd[0] == 'H' || cmd[0] == 'V')  && !strncmp(&cmd[1], "SCROLLBAR", 9)){
		if (!strcmp(&cmd[10], "POS")) {
			if (data == NULL || datalen < 1) return RC_INVALID_PARM;
			numtoi(data, datalen, &i1);
			if (i1 < 0) return RC_INVALID_VALUE;
			guiaWinScrollPos(win, cmd[0], i1);
			return 0;
		}
		else if (!strcmp(&cmd[10], "OFF")) {
			guiaWinScrollPos(win, cmd[0], -1);
			return 0;
		}
		else if (!strcmp(&cmd[10], "RANGE")) {
			if (data == NULL || datalen < 1) return RC_INVALID_PARM;
			num5toi(&data[0], &i1);
			num5toi(&data[5], &i2);
			if (datalen < 15) {
				i3 = (i2 - i1) / 8;
				if (i3 < 2) i3 = 2;
			}
			else num5toi(&data[10], &i3);
			if (i1 < 1 || i2 < 1 || i3 < 1) return RC_INVALID_VALUE;
			guiaWinScrollRange(win, (INT)cmd[0], i1, i2, i3);
			return 0;
		}
		else
			return guiaWinScrollKeys(win, cmd[0], &cmd[10]);
	}
	if (!strcmp(cmd, "MOUSEON")) win->mousemsgs = TRUE;
	else if (!strcmp(cmd, "MOUSEOFF")) win->mousemsgs = FALSE;
	else if (!strcmp(cmd, "FOCUS")) {
		if (++lrufocus & 0x80000000) {
			for (wh = winhandlehdr; wh != NULL; wh = (*wh)->nextwinhandle) {
				if ((*wh)->lrufocus) {
					if (!((*wh)->lrufocus >>= 1)) (*wh)->lrufocus = 1;;
				}
			}
			lrufocus >>= 1;
		}
		if (!win->debugflg) win->lrufocus = lrufocus;
		guiaSetActiveWin(win);
	}
	else if (!strcmp(cmd, "UNFOCUS")) {
		i4 = 0;
		for (wh = winhandlehdr; wh != NULL; wh = (*wh)->nextwinhandle) {
			if (wh != winhandle && (*wh)->lrufocus > i4) {
				i4 = (*wh)->lrufocus;
				win = *wh;
			}
		}
		if (i4 && win->lrufocus != lrufocus) {
			win->lrufocus = lrufocus;
			guiaSetActiveWin(win);
		}
	}
	else if (!strcmp(cmd, "TITLE") || !strcmp(cmd, "TASKBARTEXT")) {
		if (win->title != NULL) guiFreeMem(win->title);
		if (data == NULL || datalen < 1) return RC_INVALID_PARM;
		win->title = guiAllocString(data, datalen);
		if (win->title == NULL) {
			sprintf(work, "guiAllocString returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, datalen);
			return RC_NO_MEM;
		}
		guiaChangeWinTitle(win);
	}
	else if (!strcmp(cmd, "DESKTOPICON")) {
		if (data == NULL || datalen <= 0 || datalen > MAXRESNAMELEN) {
			return RC_INVALID_PARM;
		}
		memset(resname, ' ', MAXRESNAMELEN);
		memcpy(resname, data, datalen);
		if ((iconrh = locateIconByName(resname)) == NULL) return RC_ERROR;
		guiaChangeWinIcon(win, *iconrh);
	}
	else if (!strcmp(cmd, "POINTER")) {
		if (data == NULL || datalen < 1) return RC_INVALID_PARM;
		if (datalen == 5) {
			if (!guimemicmp((CHAR *) data, "ARROW", 5)) {
				guiaSetCursorShape(win, CURSORTYPE_ARROW);
			}
			else if (!guimemicmp((CHAR *) data, "IBEAM", 5)) {
				guiaSetCursorShape(win, CURSORTYPE_IBEAM);
			}
			else if (!guimemicmp((CHAR *) data, "CROSS", 5)) {
				guiaSetCursorShape(win, CURSORTYPE_CROSS);
			}
			else return RC_INVALID_VALUE;
		}
		else if (datalen == 4) {
			if (!guimemicmp((CHAR *) data, "WAIT", 4)) {
				guiaSetCursorShape(win, CURSORTYPE_WAIT);
			}
			else if (!guimemicmp((CHAR *) data, "HELP", 4)) {
				guiaSetCursorShape(win, CURSORTYPE_HELP);
			}
			else return RC_INVALID_VALUE;
		}
		else if (datalen == 7) {
			if (!guimemicmp((CHAR *) data, "UPARROW", 7)) {
				guiaSetCursorShape(win, CURSORTYPE_UPARROW);
			}
			else if (!guimemicmp((CHAR *) data, "SIZEALL", 7)) {
				guiaSetCursorShape(win, CURSORTYPE_SIZEALL);
			}
			else return RC_INVALID_VALUE;
		}
		else if (datalen == 6 && !guimemicmp((CHAR *) data, "SIZENS", 6)) {
			guiaSetCursorShape(win, CURSORTYPE_SIZENS);
		}
		else if (datalen == 6 && !guimemicmp((CHAR *) data, "SIZEWE", 6)) {
			guiaSetCursorShape(win, CURSORTYPE_SIZEWE);
		}
		else if (datalen == 8 && !guimemicmp((CHAR *) data, "SIZENESW", 8)) {
			guiaSetCursorShape(win, CURSORTYPE_SIZENESW);
		}
		else if (datalen == 8 && !guimemicmp((CHAR *) data, "SIZENWSE", 8)) {
			guiaSetCursorShape(win, CURSORTYPE_SIZENWSE);
		}
		else if (datalen == 9 && !guimemicmp((CHAR *) data, "HANDPOINT", 9)) {
			guiaSetCursorShape(win, CURSORTYPE_HAND);
		}
		else if (datalen == 11 && !guimemicmp((CHAR *) data, "APPSTARTING", 11)) {
			guiaSetCursorShape(win, CURSORTYPE_APPSTARTING);
		}
		else if (datalen == 2 && !guimemicmp((CHAR *) data, "NO", 2)) {
			guiaSetCursorShape(win, CURSORTYPE_NO);
		}
		else return RC_INVALID_VALUE;
	}
	else if (!strcmp(cmd, "CARETPOS")) {
		if (data == NULL || datalen < 1) return RC_INVALID_PARM;
		num5toi(&data[0], &i1);
		num5toi(&data[5], &i2);
		if (i1 < 0 || i2 < 0) return RC_INVALID_VALUE;
		guiaSetCaretPos(win, i1, i2);
	}
	else if (!strcmp(cmd, "CARETON")) {
		if (!win->caretonflg) {
			win->caretonflg = TRUE;
			guiaShowCaret(win);
		}
	}
	else if (!strcmp(cmd, "CARETOFF")) {
		if (win->caretonflg) {
			win->caretonflg = FALSE;
			guiaHideCaret(win);
		}
	}
	else if (!strcmp(cmd, "CARETSIZE")) {
		if (data == NULL || datalen < 1) return RC_INVALID_PARM;
		numtoi(data, datalen, &i1);
		if (i1 < 1) return RC_INVALID_VALUE;
		guiaSetCaretVSize(win, i1);
	}
	else if (!strcmp(cmd, "SIZE")) {
		if (data == NULL || datalen < 1) return RC_INVALID_PARM;
		num5toi(&data[0], &i1);
		num5toi(&data[5], &i2);
		if (i1 < 0 || i2 < 0) return RC_INVALID_VALUE;
		if (win->pandlgscale) {
			i1 = (i1 * scalex) / scaley;
			i2 = (i2 * scalex) / scaley;
		}
		guiaChangeWinSize(win, i1, i2);
	}
	else if (!strcmp(cmd, "POSITION")) {
		if (data == NULL || datalen < 1) return RC_INVALID_PARM;
		num5toi(&data[0], &i1);
		num5toi(&data[5], &i2);
		if (i1 < 1 || i2 < 1) return RC_INVALID_VALUE;
		/**
		 * Need to change one-based DB/C coords to zero-based Windows coords.
		 * JPR 20 Mar 13
		 */
		guiaChangeWinPosition(win, --i1, --i2);
	}
	else if (!strcmp(cmd, "STATUSBAR")) {
		if (data == NULL) return RC_INVALID_PARM;
		if (win->statusbarshown) {
			if (win->statusbartext != NULL) guiFreeMem(win->statusbartext);
			win->statusbartext = guiAllocString(data, datalen);
			if (win->statusbartext == NULL) {
				sprintf(work, "guiAllocString returned null in %s", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
			guiaChangeStatusbar(win);
		}
		else {
			win->statusbartext = guiAllocString(data, datalen);
			if (win->statusbartext == NULL) {
				sprintf(work, "guiAllocString returned null in %s", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
			if (guiaShowStatusbar(win) < 0) return RC_ERROR;
			win->statusbarshown = TRUE;
		}
	}
	else if (!strcmp(cmd, "NOSTATUSBAR")) {
		if (!win->statusbarshown) return 0;
		guiaHideStatusbar(win);
		if (win->statusbartext != NULL) {
			guiFreeMem(win->statusbartext);
			win->statusbartext = NULL;
		}
		win->statusbarshown = FALSE;
	}
	else if (!strcmp(cmd, "MAXIMIZE")) {
		guiaChangeWinMaximize(win);
	}
	else if (!strcmp(cmd, "MINIMIZE")) {
		guiaChangeWinMinimize(win);
	}
	else if (!strcmp(cmd, "RESTORE")) {
		guiaChangeWinRestore(win);
	}
	else if (!strcmp(cmd, "HELPTEXTWIDTH")) {
		if (data == NULL || datalen < 1) return RC_INVALID_PARM;
		numtoi(data, datalen, &i1);
		if (i1 < 1) return RC_INVALID_VALUE;
		guiaChangeWinTTWidth(win, i1);
	}
	else return RC_INVALID_VALUE;
	return 0;
#else
	return RC_ERROR;
#endif
}

/**
 *  Create an empty resource.
 *  Allocate memalloc memory for a RESOURCE structure.
 *  Insert the handle to it in the linked list reshandlehdr.
 *  Return the handle to it.
 */
INT guirescreate(RESHANDLE *reshandleptr)
{
#if OS_WIN32GUI
	RESHANDLE rh;
	RESOURCE *res;

	rh = (RESHANDLE) memalloc(sizeof(RESOURCE), 0);
	if (rh == NULL) return RC_NO_MEM;
	res = *rh;
	memset(res, 0, sizeof(RESOURCE));
	res->restype = RES_TYPE_NONE;

	/* Insert the handle to this new res at the top of the linked list
	 * pointed to by reshandlehdr */
	res->nextreshandle = reshandlehdr;
	reshandlehdr = rh;

	/* Set up the self reference and set up the return value */
	*reshandleptr = res->thisreshandle = rh;
	return 0;
#else
	return RC_ERROR;
#endif
}

/**
 * cbName is the size of name in bytes
 */
INT guiresstart(INT code, RESHANDLE reshandle, UCHAR *name, INT cbName)
{
#if OS_WIN32GUI
	RESHANDLE rh, currh;
	RESOURCE *res;
	UCHAR newname[MAXRESNAMELEN];

	if (cbName > MAXRESNAMELEN) return(RC_NAME_TOO_LONG);
	memset(newname, ' ', MAXRESNAMELEN);
	memcpy(newname, name, cbName);
	rh = NULL;
	for (currh = reshandlehdr; currh != NULL; currh = (*currh)->nextreshandle) {
		if (currh == reshandle) {
			rh = reshandle;
			break;
		}
	}
	if (rh == NULL) return RC_INVALID_HANDLE;
	res = *rh;
	memcpy(&res->name, newname, MAXRESNAMELEN);
	firstresfontflag = TRUE;
	switch (code) {
	case GUIRESCODE_MENU:
		res->restype = RES_TYPE_MENU;
		menureshandle = rh;
		break;
	case GUIRESCODE_POPUPMENU:
		res->restype = RES_TYPE_POPUPMENU;
		menureshandle = rh;
		break;
	case GUIRESCODE_PANEL:
		res->restype = RES_TYPE_PANEL;
		pandlgreshandle = rh;
		dlgcontrolposh = dlgcontrolposv = 0;
		tabgroupflag = FALSE;
		break;
	case GUIRESCODE_DIALOG:
		res->restype = RES_TYPE_DIALOG;
		pandlgreshandle = rh;
		dlgcontrolposh = dlgcontrolposv = 0;
		tabgroupflag = FALSE;
		break;
	case GUIRESCODE_TOOLBAR:
		res->restype = RES_TYPE_TOOLBAR;
		res->itemmsgs = TRUE;
		res->appendflag = TRUE;
		res->insertpos = 0;
		toolbarreshandle = rh;
		break;
	case GUIRESCODE_ICON:
		res->restype = RES_TYPE_ICON;
		res->hsize = ICONDEFAULT_HSIZE;
		res->vsize = ICONDEFAULT_VSIZE;
		res->bpp = ICONDEFAULT_COLORBITS;
		iconreshandle = rh;
		break;
	case GUIRESCODE_OPENFILEDLG:
		res->restype = RES_TYPE_OPENFILEDLG;
		filedlgreshandle = rh;
		break;
	case GUIRESCODE_OPENDIRDLG:
		res->restype = RES_TYPE_OPENDIRDLG;
		filedlgreshandle = rh;
		break;
	case GUIRESCODE_SAVEASFILEDLG:
		res->restype = RES_TYPE_SAVEASFILEDLG;
		filedlgreshandle = rh;
		break;
	case GUIRESCODE_FONTDLG:
		res->restype = RES_TYPE_FONTDLG;
		fontdlgreshandle = rh;
		break;
	case GUIRESCODE_COLORDLG:
		res->restype = RES_TYPE_COLORDLG;
		colordlgreshandle = rh;
		break;
	case GUIRESCODE_ALERTDLG:
		res->restype = RES_TYPE_ALERTDLG;
		alertdlgreshandle = rh;
		break;
	default:
		return RC_ERROR;
	}
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guiiconhsize(INT hsize)
{
#if OS_WIN32GUI
	RESOURCE *res;

	if (iconreshandle == NULL) return RC_INVALID_HANDLE;
	res = *iconreshandle;
	res->hsize = hsize;
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guiiconvsize(INT vsize)
{
#if OS_WIN32GUI
	RESOURCE *res;

	if (iconreshandle == NULL) return RC_INVALID_HANDLE;
	res = *iconreshandle;
	res->vsize = vsize;
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guiiconcolorbits(INT colorbits)
{
#if OS_WIN32GUI
	RESOURCE *res;

	if (iconreshandle == NULL) return RC_INVALID_HANDLE;
	if (colorbits != 1 && colorbits != 4 && colorbits != 24) {
		iconreshandle = NULL;
		return RC_ERROR;
	}
	res = *iconreshandle;
	res->bpp = colorbits;
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guiiconpixels(UCHAR *pixeldata, INT datalen)
{
#if OS_WIN32GUI
	RESOURCE *res;
	UCHAR *resdata;
	CHAR work[64];

	if (iconreshandle == NULL) return RC_INVALID_HANDLE;
	res = *iconreshandle;
	res->content = guiAllocMem(datalen);
	if (res->content == NULL) {
		iconreshandle = NULL;
		sprintf(work, "guiAllocMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, datalen);
		return RC_NO_MEM;
	}
	resdata = guiLockMem(res->content);
	memcpy(resdata, pixeldata, datalen);
	guiUnlockMem(res->content);
	res->entrycount = datalen;
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guiiconend(void)
{
#if OS_WIN32GUI
	INT rc;
	RESOURCE *res;

	if (iconreshandle == NULL) return RC_INVALID_HANDLE;
	res = *iconreshandle;
	if (res->content == NULL) {
		iconreshandle = NULL;
		return RC_NO_MEM;
	}
	rc = guiaCreateIcon(res);
	iconreshandle = NULL;
	return(rc);
#else
	return RC_ERROR;
#endif
}

#if OS_WIN32GUI
static RESHANDLE locateIconByName(UCHAR* resname) {
	RESHANDLE iconrh= NULL;
	RESOURCE *piconres;
	for (iconrh = reshandlehdr; iconrh != NULL; iconrh = piconres->nextreshandle) {
		piconres = *iconrh;
		if (piconres->restype != RES_TYPE_ICON) continue;
		if (!memcmp(piconres->name, resname, MAXRESNAMELEN)) break;
	}
	return iconrh;
}
#endif

INT guitoolbarprocess(INT rescode, INT itemnum, INT hsize, INT vsize, UCHAR *iconname, INT iconnamelen, UCHAR *text, INT textlen)
{
#if OS_WIN32GUI
#define TOOLBARCTRLALLOC 12
	static INT controlunused;
	static CONTROL *control;
	RESHANDLE iconrh;
	RESOURCE *res;
	UCHAR resname[MAXRESNAMELEN];
	ZHANDLE ptr;
	CHAR work[64];

	if (toolbarreshandle == NULL) return RC_INVALID_HANDLE;
	res = *toolbarreshandle;

	if (rescode == GUITOOLBARCODE_END) {
		if (res->entrycount) {
			ptr = guiReallocMem(res->content, sizeof(CONTROL) * res->entrycount);
			if (ptr != NULL) res->content = ptr;
			else {
				sprintf(work, "guiReallocMem returned null in %s", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
		}
		toolbarreshandle = NULL;
		return 0;
	}

	if (iconnamelen > MAXRESNAMELEN || iconnamelen < 0 || textlen < 0) {
		if (res->content != NULL) guiUnlockMem(res->content);
		guiresdestroy(toolbarreshandle);
		toolbarreshandle = NULL;
		return RC_ERROR;
	}

	if (rescode == GUITOOLBARCODE_GRAY) {
		if (!res->entrycount) return RC_ERROR;
		(control - 1)->style |= CTRLSTYLE_DISABLED;
		return 0;
	}

	if (!res->entrycount) controlunused = 0;
	if (!controlunused) {
		if (!res->entrycount) {
			/* initial allocation */
			ptr = guiAllocMem(sizeof(CONTROL) * TOOLBARCTRLALLOC);
		}
		else {
			/* allocate TOOLBARCTRLALLOC more controls */
			guiUnlockMem(res->content);
			ptr = guiReallocMem(res->content, sizeof(CONTROL) * (res->entrycount + TOOLBARCTRLALLOC));
		}
		if (ptr == NULL) {
			guiresdestroy(toolbarreshandle);
			toolbarreshandle = NULL;
			sprintf(work, "guiAllocXXX returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return RC_NO_MEM;
		}
		res->content = ptr;
		control = (CONTROL *) guiLockMem(res->content) + res->entrycount;
		controlunused = TOOLBARCTRLALLOC;
	}
	memset(control, 0, sizeof(CONTROL));
	control->type = rescode;
	control->res = toolbarreshandle;
	if (rescode == GUITOOLBARCODE_DROPBOX) {
		control->style |= CTRLSTYLE_INSERTORDER;
		control->insstyle = LIST_INSERTNORMAL;
		control->rect.top = control->rect.left = 0;
		control->rect.right = hsize;
		control->rect.bottom = vsize;
	}

	if (rescode != GUITOOLBARCODE_SPACE) {
		control->useritem = itemnum;
		if (iconnamelen) {
			memset(resname, ' ', MAXRESNAMELEN);
			memcpy(resname, iconname, iconnamelen);
			if ((iconrh = locateIconByName(resname)) == NULL) goto error;
			control->iconres = iconrh;
		}
		if (textlen) {
			control->tooltip = guiAllocString(text, textlen);
			if (control->tooltip == NULL) {
				guiUnlockMem(res->content);
				guiresdestroy(toolbarreshandle);
				toolbarreshandle = NULL;
				sprintf(work, "guiAllocString returned null in %s", __FUNCTION__);
				guiaErrorMsg(work, textlen);
				return RC_NO_MEM;
			}
		}
	}
	control++;
	res->entrycount++;
	controlunused--;
	return 0;
error:
	guiUnlockMem(res->content);
	guiresdestroy(toolbarreshandle);
	toolbarreshandle = NULL;
	return RC_ERROR;
#else
	return RC_ERROR;
#endif
}

INT guimenuprocess(INT code, INT num, UCHAR *text, INT textlen, CHAR *iconname, UINT iconnamelen)
{
#if OS_WIN32GUI
	static INT entryavail, menulevel;
	static MENUENTRY * menuentry;
	RESHANDLE iconrh;
	UCHAR resname[MAXRESNAMELEN];
	RESOURCE *res;
	ZHANDLE ptr;
	CHAR work[64];

	if (menureshandle == NULL) return RC_INVALID_HANDLE;
	res = *menureshandle;

	if (iconnamelen > MAXRESNAMELEN) {
		guiUnlockMem(res->content);
		guiresdestroy(menureshandle);
		menureshandle = NULL;
		return RC_ERROR;
	}

	if (!res->entrycount) {
		if (res->restype == RES_TYPE_MENU) {
			if (code != GUIMENUCODE_MAIN) {
				goto menuerror;
			}
		}
		entryavail = menulevel = 0;
	}
	if (code == GUIMENUCODE_END) {
		guiUnlockMem(res->content);
		ptr = guiReallocMem(res->content, sizeof(MENUENTRY) * res->entrycount);
		if (ptr != NULL) res->content = ptr;
		else {
			sprintf_s(work, ARRAYSIZE(work), "guiReallocMem failed in %s", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return RC_NO_MEM;
		}
		menureshandle = NULL;
		return 0;
	}
	if (code == GUIMENUCODE_ENDSUBMENU) {
		menulevel--;
		if (res->restype == RES_TYPE_MENU) {
			if (menulevel < 1) goto menuerror;
		}
		else if (menulevel < 0) goto menuerror;
		return 0;
	}
	if (code == GUIMENUCODE_GRAY) {
		if (!res->entrycount) goto menuerror;
		(menuentry - 1)->style |= CTRLSTYLE_DISABLED;
		return 0;
	}
	if (code == GUIMENUCODE_KEY) {
		if (res->restype == RES_TYPE_POPUPMENU || !menulevel || menulevel != (menuentry - 1)->level) goto menuerror;
		(menuentry - 1)->speedkey = num;
		return 0;
	}
	if (!entryavail) {
		entryavail = 24;
		if (!res->entrycount) ptr = guiAllocMem(sizeof(MENUENTRY) * entryavail);
		else {
			guiUnlockMem(res->content);
			ptr = guiReallocMem(res->content, sizeof(MENUENTRY) * (res->entrycount + entryavail));
		}
		if (ptr == NULL) {
			guiresdestroy(menureshandle);
			menureshandle = NULL;
			sprintf(work, "guiAllocXXX returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return RC_NO_MEM;
		}
		res->content = ptr;
		menuentry = (MENUENTRY *) guiLockMem(res->content) + res->entrycount;
	}

	switch (code) {
	case GUIMENUCODE_MAIN:
		if (res->restype == RES_TYPE_POPUPMENU) goto menuerror; // @suppress("No break at end of case")
		/* drop through */
	case GUIMENUCODE_ITEM:
	case GUIMENUCODE_ICONITEM:
	case GUIMENUCODE_SUBMENU:
	case GUIMENUCODE_ICONSUBMENU:
		menuentry->text = guiAllocString(text, textlen);
		if (menuentry->text == NULL) {
			sprintf(work, "guiAllocString returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, textlen);
			return RC_NO_MEM;
		}
		menuentry->useritem = num;
		menuentry->style = 0;
		menuentry->speedkey = 0;
		if (code == GUIMENUCODE_MAIN) {
			menuentry->level = 0;
			menulevel = 1;
		}
		else {
			menuentry->level = menulevel;
			if (code == GUIMENUCODE_SUBMENU || code == GUIMENUCODE_ICONSUBMENU) menulevel++;
		}
//		if (code == GUIMENUCODE_ICONITEM || code == GUIMENUCODE_ICONSUBMENU) {
		if (iconnamelen > 0) {
			memset(resname, ' ', MAXRESNAMELEN);
			memcpy(resname, iconname, iconnamelen);
			if ((iconrh = locateIconByName(resname)) == NULL) goto menuerror;
			menuentry->iconres = iconrh;
		}
		else menuentry->iconres = NULL;
		break;
	case GUIMENUCODE_LINE:
		/* The next line prevents someone from trying to put a separator on the main menu bar */
		if (!menulevel && res->restype == RES_TYPE_MENU) goto menuerror;
		menuentry->text = NULL;
		menuentry->useritem = 0;
		menuentry->style = MENUSTYLE_LINE;
		menuentry->speedkey = 0;
		menuentry->level = menulevel;
		menuentry->iconres = NULL;		// without this Win2000 sometimes goes bonkers
		break;
	default:
		goto menuerror;
	}
	menuentry++;
	res->entrycount++;
	entryavail--;
	return 0;
menuerror:
	if (res->content != NULL) guiUnlockMem(res->content);
	guiresdestroy(menureshandle);
	menureshandle = NULL;
	return RC_ERROR;
#else
	return RC_ERROR;
#endif
}

INT guipandlgposh(INT h)
{
#if OS_WIN32GUI
	dlgcontrolposh = (h * scalex) / scaley;
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guipandlgposv(INT v)
{
#if OS_WIN32GUI
	dlgcontrolposv = (v * scalex) / scaley;
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guipandlgposha(INT ha)
{
#if OS_WIN32GUI
	dlgcontrolposh += (ha * scalex) / scaley;
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guipandlgposva(INT va)
{
#if OS_WIN32GUI
	dlgcontrolposv += (va * scalex) / scaley;
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guipandlgboxtabs(INT *tabs, CHAR *boxjust, INT count)
{
#if OS_WIN32GUI
	dlgcontrolboxtabs = tabs;
	dlgcontrolboxtabcount = count;
	dlgcontrolboxjust = boxjust;
	return(guipandlgprocess(GUIPANDLGCODE_BOXTABS, 0, NULL, 0, 0, 0));
#else
	return RC_ERROR;
#endif
}

INT guipandlgfedit(INT num, UCHAR *text, INT textlen, UCHAR *mask, INT masklen, INT horz)
{
#if OS_WIN32GUI
	dlgcontrolfeditmask = mask;
	dlgcontrolfeditmasklen = masklen;
	return(guipandlgprocess(GUIPANDLGCODE_FEDIT, num, text, textlen, horz, 0));
#else
	return RC_ERROR;
#endif
}

INT guipandlgmledit(INT num, UCHAR *text, INT textlen, INT horz, INT vert, INT maxchars, INT hscroll, INT vscroll)
{
#if OS_WIN32GUI
	dlgcontrolleditmaxchars = maxchars;
	if (vert) {
		if (vscroll && hscroll) return(guipandlgprocess(GUIPANDLGCODE_MLEDITS, num, text, textlen, horz, vert));
		else if (vscroll) return(guipandlgprocess(GUIPANDLGCODE_MLEDITVS, num, text, textlen, horz, vert));
		else if (hscroll) return(guipandlgprocess(GUIPANDLGCODE_MLEDITHS, num, text, textlen, horz, vert));
		else return(guipandlgprocess(GUIPANDLGCODE_MLEDIT, num, text, textlen, horz, vert));
	}
	else {
		return(guipandlgprocess(GUIPANDLGCODE_LEDIT, num, text, textlen, horz, vert));
	}
#else
	return RC_ERROR;
#endif
}

INT guipandlgpledit(INT num, UCHAR *text, INT textlen, INT horz, INT maxchars)
{
#if OS_WIN32GUI
	dlgcontrolleditmaxchars = maxchars;
	return(guipandlgprocess(GUIPANDLGCODE_PLEDIT, num, text, textlen, horz, 0));
#else
	return RC_ERROR;
#endif
}

INT guipandlgpopbox(INT num, INT horz)
{
#if OS_WIN32GUI
	return(guipandlgprocess(GUIPANDLGCODE_POPBOX, num, NULL, 0, horz, 0));
#else
	return RC_ERROR;
#endif
}

INT guipandlgtable(INT num, INT horz, INT vert)
{
#if OS_WIN32GUI
	return(guipandlgprocess(GUIPANDLGCODE_TABLE, num, NULL, 0, horz, vert));
#else
	return RC_ERROR;
#endif
}

INT guipandlgtablecolumntitle(CHAR *string, INT stringlen)
{
#if OS_WIN32GUI
	return(guipandlgprocess(GUIPANDLGCODE_TBL_COLTITLE, 0, (UCHAR *) string, stringlen, 0, 0));
#else
	return RC_ERROR;
#endif
}

#if OS_WIN32GUI
// radiocount initialized because global optimizations in MSVC were apparently failing to do so
static INT controlunused, radiocount = 0;
static CONTROL *control, *lastradio;
static INT tabgroup, tabsubgroup;
static ZRECT tabgrouprect;
static INT lasttabledropboxcolumn = -1;
static INT lasttableeditcolumn = -1;
typedef struct {
	CHAR *mask;
	UINT colindex;
	UINT type;
	UINT extra;
	UINT masklen;
} CREATETABLECOLUMNSTRUCT;
#endif

/**
 * colindex is zero-based
 * extra is the expanded height for a dropbox or cdropbox
 * or the max characters for an LEDIT box.
 */
INT guipandlgtablecolumn(INT colindex, INT item, INT width, INT type, INT extra, CHAR* mask, UINT masklen) {

#if OS_WIN32GUI
	CREATETABLECOLUMNSTRUCT cs;
	cs.colindex = colindex;
	cs.type = type;
	cs.extra = extra;
	cs.mask = mask;
	cs.masklen = masklen;
	return(guipandlgprocess(GUIPANDLGCODE_TBL_COLUMN, item, (void *) &cs, 0, width, 0));
#else
	return -1;
#endif
}

INT guipandlgtablenoheader(void) {
#if OS_WIN32GUI
	return(guipandlgprocess(GUIPANDLGCODE_TBL_NOHEADER, 0, 0, 0, 0, 0));
#else
	return -1;
#endif
}

INT guipandlgtableinsertorder(void) {
#if OS_WIN32GUI
	return(guipandlgprocess(GUIPANDLGCODE_TBL_INSERTORDER, 0, 0, 0, 0, 0));
#else
	return -1;
#endif
}

INT guipandlgtablejustify(INT code) {
#if OS_WIN32GUI
	return(guipandlgprocess(GUIPANDLGCODE_TBL_JUSTIFY, code, 0, 0, 0, 0));
#else
	return RC_ERROR;
#endif
}

INT guipandlgjustify(INT code) {
#if OS_WIN32GUI
	return(guipandlgprocess(GUIPANDLGCODE_JUSTIFY, code, 0, 0, 0, 0));
#else
	return RC_ERROR;
#endif
}

/**
 * 'text' is defined as a pointer to void because for GUIPANDLGCODE_TBL_COLUMN it is a pointer to
 * a structure, for all others it is a pointer to UCHAR
 */
INT guipandlgprocess(INT code, INT num, void *text, INT textlen, INT horz, INT vert)
{
#if OS_WIN32GUI
	CONTROL *lastcontrol;
	RESHANDLE iconrh;
	RESOURCE *res, *piconres;
	INT i1, *ip1, numcolumns, color;
	UCHAR resname[MAXRESNAMELEN];
	ZHANDLE ptr, ptr2;
	CHAR *cp1;
	TABLECOLUMN *tblcolumns;
	CREATETABLECOLUMNSTRUCT *cs;
	CHAR work[128];

	if (pandlgreshandle == NULL) return RC_INVALID_HANDLE;
	res = *pandlgreshandle;

	if (!res->entrycount) controlunused = 0;
	else lastcontrol = control - 1;

	horz = (horz * scalex) / scaley;
	vert = (vert * scalex) / scaley;

	switch (code) {
	case GUIPANDLGCODE_TITLE:
		if (res->restype != RES_TYPE_DIALOG) goto dlgerror;
		res->title = guiAllocString(text, textlen);
		if (res->title == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocString returned null in %s at GUIPANDLGCODE_TITLE", __FUNCTION__);
			guiaErrorMsg(work, textlen);
			return RC_NO_MEM;
		}
		return 0;
	case GUIPANDLGCODE_NOCLOSEBUTTON:
		if (res->restype != RES_TYPE_DIALOG) goto dlgerror;
		res->noclosebuttonflag = TRUE;
		return 0;
	case GUIPANDLGCODE_FONT:
		i1 = guiaSetResFont(res, text, textlen, firstresfontflag);
		firstresfontflag = FALSE;
		return i1;
	case GUIPANDLGCODE_TEXTCOLOR:
		res->textcolor = TEXTCOLOR_DEFAULT;
		i1 = GetColorNumberFromText(text);
		if (i1 != RC_ERROR) {
			res->textcolor = i1;
			return 0;
		}
		i1 = GetColorNumberFromTextExt(text, textlen, &color, NULL);
		if (i1 == RC_ERROR) {
			sprintf_s(work, ARRAYSIZE(work), "TEXTCOLOR value unknown '%.*s'", textlen, (char *)text);
			guiaErrorMsg(work, 0);
			res->textcolor = TEXTCOLOR_BLACK;
		}
		else {
			res->textcolor = TEXTCOLOR_CUSTOM;
			res->textfgcolor = color;
		}
		return 0;
	case GUIPANDLGCODE_SIZE:
		ZRECTRIGHT(res->rect) = horz;
		ZRECTBOTTOM(res->rect) = vert;
		ZRECTTOP(res->rect) = 0;
		ZRECTLEFT(res->rect) = 0;
		return 0;
	case GUIPANDLGCODE_GRAY:
		if (!res->entrycount) goto dlgerror;
		lastcontrol->style |= CTRLSTYLE_DISABLED;
		return 0;
	case GUIPANDLGCODE_SHOWONLY:
		if (!res->entrycount) goto dlgerror;
		lastcontrol->style |= CTRLSTYLE_SHOWONLY;
		return 0;

	case GUIPANDLGCODE_NOFOCUS:
		if (!res->entrycount) goto dlgerror;
		if (!CANNOFOCUS(lastcontrol)) goto dlgerror;
		lastcontrol->style |= CTRLSTYLE_NOFOCUS;
		return 0;

	case GUIPANDLGCODE_READONLY:
		if (!res->entrycount) goto dlgerror;
		lastcontrol->style |= CTRLSTYLE_READONLY;
		return 0;

	case GUIPANDLGCODE_HELPTEXT:
		if (!res->entrycount) goto dlgerror;
		lastcontrol->tooltip = guiAllocString(text, textlen);
		if (lastcontrol->tooltip == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocString returned null in %s at GUIPANDLGCODE_HELPTEXT", __FUNCTION__);
			guiaErrorMsg(work, textlen);
			return RC_NO_MEM;
		}
		return 0;

	case GUIPANDLGCODE_TBL_COLTITLE:
		if (!res->entrycount) goto dlgerror;
		numcolumns = ++lastcontrol->table.numcolumns;
		if (numcolumns == 1) {
			ptr = guiAllocMem(sizeof(TABLECOLUMN));
			lasttabledropboxcolumn = -1;
			lasttableeditcolumn = -1;
		}
		else {
			ptr = guiReallocMem(lastcontrol->table.columns, numcolumns * sizeof(TABLECOLUMN));
		}
		ptr2 = guiAllocMem(textlen + 1);	// Allocate memory for the column heading

		if (ptr == NULL || ptr2 == NULL) {
			guiresdestroy(pandlgreshandle);
			pandlgreshandle = NULL;
			sprintf_s(work, ARRAYSIZE(work), "guiAllocXXX returned null in %s at GUIPANDLGCODE_TBL_COLTITLE", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return RC_NO_MEM;
		}
		lastcontrol->table.columns = ptr;
		tblcolumns = (PTABLECOLUMN) guiLockMem(lastcontrol->table.columns);
		tblcolumns[numcolumns - 1].name = ptr2;
		cp1 = guiLockMem(tblcolumns[numcolumns - 1].name);
		memcpy(cp1, text, textlen);
		cp1[textlen] = '\0';
		guiUnlockMem(tblcolumns[numcolumns - 1].name);

		tblcolumns[numcolumns - 1].mask = NULL;
		tblcolumns[numcolumns - 1].namelength = textlen + 1;
		tblcolumns[numcolumns - 1].db.insstyle = LIST_INSERTNORMAL;
		tblcolumns[numcolumns - 1].db.itemarray = NULL;
		tblcolumns[numcolumns - 1].db.itemalloc = 0;
		tblcolumns[numcolumns - 1].db.itemcount = 0;
		tblcolumns[numcolumns - 1].insertorder = FALSE;
		tblcolumns[numcolumns - 1].textcolor.color = TEXTCOLOR_DEFAULT;
		tblcolumns[numcolumns - 1].textcolor.code = 0;

		guiUnlockMem(lastcontrol->table.columns);
		return 0;

	case GUIPANDLGCODE_TBL_COLUMN:
		if (!res->entrycount) goto dlgerror;
		tblcolumns = (TABLECOLUMN *) guiLockMem(lastcontrol->table.columns);
		cs = (CREATETABLECOLUMNSTRUCT*) text;
		tblcolumns[cs->colindex].type = cs->type;
		tblcolumns[cs->colindex].width = horz;
		tblcolumns[cs->colindex].useritem = num;
		if (ISTABLECOLUMNDROPBOX(cs->type)) {
			tblcolumns[cs->colindex].insertorder = FALSE;
			tblcolumns[cs->colindex].dbopenht = cs->extra;
			lasttabledropboxcolumn = cs->colindex;
		}
		else if (cs->type == PANEL_EDIT) {
			lasttableeditcolumn = cs->colindex;
		}
		else if (cs->type == PANEL_LEDIT) {
			tblcolumns[cs->colindex].maxchars = cs->extra;
			lasttableeditcolumn = cs->colindex;
		}
		else if (cs->type == PANEL_FEDIT) {
			if ((tblcolumns[cs->colindex].mask = guiAllocMem(cs->masklen + 1)) == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s at GUIPANDLGCODE_TBL_COLUMN", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
			cp1 = guiLockMem(tblcolumns[cs->colindex].mask);
			memcpy(cp1, cs->mask, cs->masklen);
			cp1[cs->masklen] = '\0';
			guiUnlockMem(tblcolumns[cs->colindex].mask);
			lasttableeditcolumn = cs->colindex;
		}
		guiUnlockMem(lastcontrol->table.columns);
		return 0;

	case GUIPANDLGCODE_TBL_INSERTORDER:
		if (lasttabledropboxcolumn == -1) goto dlgerror;
		tblcolumns = (TABLECOLUMN *) guiLockMem(lastcontrol->table.columns);
		tblcolumns[lasttabledropboxcolumn].insertorder = TRUE;
		guiUnlockMem(lastcontrol->table.columns);
		return 0;

	case GUIPANDLGCODE_TBL_JUSTIFY:
		if (lasttableeditcolumn == -1) goto dlgerror;
		tblcolumns = (TABLECOLUMN *) guiLockMem(lastcontrol->table.columns);
		tblcolumns[lasttableeditcolumn].justify = num;
		guiUnlockMem(lastcontrol->table.columns);
		return 0;

	case GUIPANDLGCODE_TBL_NOHEADER:
		lastcontrol->table.noheader = TRUE;
		return 0;
	}


	if (code < GUIPANDLGCODE_LOWNOCONTROL) {  /* all of these create a new control */
		if (!controlunused) {
			controlunused = 24;
			if (!res->entrycount) ptr = guiAllocMem(sizeof(CONTROL) * controlunused);
			else {
				if (lastradio != NULL) i1 = (INT) (control - lastradio);
				guiUnlockMem(res->content);
				ptr = guiReallocMem(res->content, sizeof(CONTROL) * (res->entrycount + controlunused));
			}
			if (ptr == NULL) {
				guiresdestroy(pandlgreshandle);
				pandlgreshandle = NULL;
				sprintf_s(work, ARRAYSIZE(work), "guiAllocXXX returned null in %s at GUIPANDLGCODE_LOWNOCONTROL", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
			res->content = ptr;
			control = (CONTROL *) guiLockMem(res->content) + res->entrycount;
			if (res->entrycount) lastcontrol = control - 1;
			if (lastradio != NULL) lastradio = control - i1;
		}
		memset(control, 0, sizeof(CONTROL));
		if (code == GUIPANDLGCODE_ESCPUSHBUTTON) {
			control->type = PANEL_PUSHBUTTON;
			control->escflag = TRUE;
		}
		else if (code == GUIPANDLGCODE_ICONESCPUSHBUTTON) {
			control->type = PANEL_ICONPUSHBUTTON;
			control->escflag = TRUE;
		}
		else control->type = code;
		control->useritem = num;
		control->res = pandlgreshandle;
		control->textcolor = res->textcolor;
		control->textfgcolor = res->textfgcolor;
		ZRECTLEFT(control->rect) = dlgcontrolposh;
		ZRECTTOP(control->rect) = dlgcontrolposv;
		ZRECTRIGHT(control->rect) = dlgcontrolposh + horz - 1;
		ZRECTBOTTOM(control->rect) = dlgcontrolposv + vert - 1;
		if (tabgroupflag) {
			control->tabgroup = tabgroup;
			control->tabsubgroup = tabsubgroup;
			control->selectedtab = 0;
		}

		switch (code) {  /* all of these have text */
		case GUIPANDLGCODE_FEDIT:
			control->mask = guiAllocString(dlgcontrolfeditmask, dlgcontrolfeditmasklen + 1);
			if (control->mask == NULL) {
				guiUnlockMem(res->content);
				guiresdestroy(pandlgreshandle);
				pandlgreshandle = NULL;
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString returned null in %s at GUIPANDLGCODE_FEDIT", __FUNCTION__);
				guiaErrorMsg(work, dlgcontrolfeditmasklen + 1);
				return RC_NO_MEM;
			}
			control->mask[dlgcontrolfeditmasklen] = '\0';
			control->textval = guiAllocMem(dlgcontrolfeditmasklen + 1);
			control->changetext = guiAllocString(text, textlen);
			if (control->changetext == NULL || control->textval == NULL) {
				guiUnlockMem(res->content);
				guiresdestroy(pandlgreshandle);
				pandlgreshandle = NULL;
				sprintf_s(work, ARRAYSIZE(work), "guiAllocXXX returned null in %s at GUIPANDLGCODE_FEDIT", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
			guiaSetCtlFont(control);
			break;
		case GUIPANDLGCODE_MLEDITS:
		case GUIPANDLGCODE_MLEDITVS:
		case GUIPANDLGCODE_MLEDITHS:
		case GUIPANDLGCODE_MLEDIT:
		case GUIPANDLGCODE_LEDIT:
		case GUIPANDLGCODE_PLEDIT:
			control->maxtextchars = dlgcontrolleditmaxchars;
			/* fall into */
		case GUIPANDLGCODE_BOXTITLE:
		case GUIPANDLGCODE_TEXT:
		case GUIPANDLGCODE_EDIT:
		case GUIPANDLGCODE_LTCHECKBOX:
		case GUIPANDLGCODE_CHECKBOX:
		case GUIPANDLGCODE_PUSHBUTTON:
		case GUIPANDLGCODE_DEFPUSHBUTTON:
		case GUIPANDLGCODE_ESCPUSHBUTTON:
		case GUIPANDLGCODE_BUTTON:
		case GUIPANDLGCODE_CTEXT:
		case GUIPANDLGCODE_CVTEXT:
		case GUIPANDLGCODE_RTEXT:
		case GUIPANDLGCODE_VTEXT:
		case GUIPANDLGCODE_RVTEXT:
		case GUIPANDLGCODE_MEDIT:
		case GUIPANDLGCODE_MEDITHS:
		case GUIPANDLGCODE_MEDITVS:
		case GUIPANDLGCODE_MEDITS:
		case GUIPANDLGCODE_PEDIT:
		case GUIPANDLGCODE_LOCKBUTTON:
		case GUIPANDLGCODE_TAB:
			control->textval = guiAllocString(text, textlen);
			if (control->textval == NULL) {
				guiUnlockMem(res->content);
				guiresdestroy(pandlgreshandle);
				pandlgreshandle = NULL;
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at GUIPANDLGCODE_TAB", __FUNCTION__);
				guiaErrorMsg(work, textlen);
				return RC_NO_MEM;
			}
			/* no break */
		case GUIPANDLGCODE_LISTBOX:
		case GUIPANDLGCODE_LISTBOXHS:
		case GUIPANDLGCODE_MLISTBOX:
		case GUIPANDLGCODE_MLISTBOXHS:
		case GUIPANDLGCODE_DROPBOX:
		case GUIPANDLGCODE_TREE:
		case GUIPANDLGCODE_POPBOX:
		case GUIPANDLGCODE_TABLE:
			guiaSetCtlFont(control);
			break;
		}
	}

	/*
	 * This is where we make sure that the 1st radio button in a group is marked.
	 */
 	if (radiocount) {
		if (code == GUIPANDLGCODE_BUTTON) {
			control->type = PANEL_RADIOBUTTON;
			if (radiocount == 1){
				control->style |= CTRLSTYLE_MARKED;
			}
			control->value1 = radiocount++;
			lastradio = control;
			goto dlgend;
		}
		if (code == GUIPANDLGCODE_ENDBUTTONGROUP) {
			if (lastradio != NULL) lastradio->type = PANEL_LASTRADIOBUTTON;
			radiocount = 0;
			return 0;
		}
		goto dlgerror;
	}

	switch (code) {  /* main processing switch */
	case GUIPANDLGCODE_END:
		if (tabgroupflag) goto dlgerror;
		if (res->content != NULL && res->entrycount > 0) {
			guiUnlockMem(res->content);
			ptr = guiReallocMem(res->content, sizeof(CONTROL) * res->entrycount);
			if (ptr != NULL) res->content = ptr;
			else {
				sprintf_s(work, ARRAYSIZE(work), "guiReallocMem returned null in %s at GUIPANDLGCODE_END", __FUNCTION__);
				guiaErrorMsg(work, sizeof(CONTROL) * res->entrycount);
				goto dlgerror;
			}
		}
		pandlgreshandle = NULL;
		return 0;
	case GUIPANDLGCODE_BUTTONGROUP:
		radiocount = 1;
		lastradio = NULL;
		return 0;
	case GUIPANDLGCODE_INSERTORDER:
		if (!res->entrycount ||
				(lastcontrol->type != PANEL_LISTBOX && lastcontrol->type != PANEL_DROPBOX &&
				lastcontrol->type != PANEL_LISTBOXHS && !ISMULTISELLISTBOX(lastcontrol))
			)
		{
			goto dlgerror;
		}
		lastcontrol->style |= CTRLSTYLE_INSERTORDER;
		return 0;
	case GUIPANDLGCODE_JUSTIFY:
		if (!res->entrycount
			|| (lastcontrol->type != PANEL_EDIT && lastcontrol->type != PANEL_LEDIT && lastcontrol->type != PANEL_FEDIT)) goto dlgerror;
		lastcontrol->style |= num;
		return 0;
	case GUIPANDLGCODE_BOXTABS:
		if (!res->entrycount ||
				(lastcontrol->type != PANEL_LISTBOX && lastcontrol->type != PANEL_DROPBOX &&
				lastcontrol->type != PANEL_LISTBOXHS && !ISMULTISELLISTBOX(lastcontrol))
			)
		{
			goto dlgerror;
		}
		if (dlgcontrolboxtabcount >= MAXBOXTABS || lastcontrol->listboxtabs != NULL) goto dlgerror;
		lastcontrol->listboxtabs = guiAllocMem(sizeof(INT) * (dlgcontrolboxtabcount + 1));
		if (lastcontrol->listboxtabs == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s at GUIPANDLGCODE_BOXTABS", __FUNCTION__);
			guiaErrorMsg(work, sizeof(CHAR) * (dlgcontrolboxtabcount + 1));
			goto dlgerror;
		}
		lastcontrol->listboxjust = guiAllocMem(sizeof(CHAR) * (dlgcontrolboxtabcount + 1));
		if (lastcontrol->listboxjust == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s at GUIPANDLGCODE_BOXTABS", __FUNCTION__);
			guiaErrorMsg(work, sizeof(CHAR) * (dlgcontrolboxtabcount + 1));
			goto dlgerror;
		}
		ip1 = (INT *) guiMemToPtr(lastcontrol->listboxtabs);
		if (scalex == 1 && scaley == 1)
			memcpy(ip1, dlgcontrolboxtabs, sizeof(INT) * dlgcontrolboxtabcount);
		else {
			for (i1 = 0; i1 < dlgcontrolboxtabcount; i1++)
				ip1[i1] = (dlgcontrolboxtabs[i1] * scalex) / scaley;
		}
		ip1[dlgcontrolboxtabcount] = 0;
		cp1 = (CHAR *) guiMemToPtr(lastcontrol->listboxjust);
		memcpy(cp1, dlgcontrolboxjust, sizeof(CHAR) * dlgcontrolboxtabcount);
		cp1[dlgcontrolboxtabcount] = '\0';
		return 0;
	case GUIPANDLGCODE_TABGROUP:
		if (tabgroupflag) goto dlgerror;		// Prevent a tabgroup in a tabgroup
		tabgroupflag = TRUE;
		tabgroup++;
		tabsubgroup = 0;
		ZRECTLEFT(tabgrouprect) = dlgcontrolposh;
		ZRECTTOP(tabgrouprect) = dlgcontrolposv;
		ZRECTRIGHT(tabgrouprect) = dlgcontrolposh + horz - 1;
		ZRECTBOTTOM(tabgrouprect) = dlgcontrolposv + vert - 1;
		return 0;
	case GUIPANDLGCODE_TABGROUPEND:
		if (!tabgroupflag) goto dlgerror;
		tabgroupflag = FALSE;
		dlgcontrolposh = ZRECTLEFT(tabgrouprect);
		dlgcontrolposv = ZRECTTOP(tabgrouprect);
		return 0;
	case GUIPANDLGCODE_TAB:
		if (!tabgroupflag) goto dlgerror;
		control->tabsubgroup = ++tabsubgroup;
		control->rect = tabgrouprect;
		dlgcontrolposh = dlgcontrolposv = 0;
		break;
	case GUIPANDLGCODE_LISTBOX:
	case GUIPANDLGCODE_LISTBOXHS:
	case GUIPANDLGCODE_MLISTBOX:
	case GUIPANDLGCODE_MLISTBOXHS:
	case GUIPANDLGCODE_DROPBOX:
		control->insstyle = LIST_INSERTNORMAL;
		break;
	case GUIPANDLGCODE_FEDIT:
		guiaFEditMaskText((CHAR *) control->changetext, (CHAR *) control->mask, (CHAR *) control->textval);
		guiFreeMem(control->changetext);
		control->changetext = NULL;
		break;
	case GUIPANDLGCODE_ICON:
	case GUIPANDLGCODE_VICON:
	case GUIPANDLGCODE_ICONLOCKBUTTON:
	case GUIPANDLGCODE_ICONPUSHBUTTON:
	case GUIPANDLGCODE_ICONDEFPUSHBUTTON:
	case GUIPANDLGCODE_ICONESCPUSHBUTTON:
		if (textlen > MAXRESNAMELEN || textlen <= 0) goto dlgerror;
		memset(resname, ' ', MAXRESNAMELEN);
		memcpy(resname, text, textlen);
		if ((iconrh = locateIconByName(resname)) == NULL) goto dlgerror;
		control->iconres = iconrh;
		piconres = *iconrh;
		if (code == GUIPANDLGCODE_ICON || code == GUIPANDLGCODE_VICON) {
			ZRECTRIGHT(control->rect) = dlgcontrolposh + piconres->hsize - 1;
			ZRECTBOTTOM(control->rect) = dlgcontrolposv + piconres->vsize - 1;
			control->style |= CTRLSTYLE_SHOWONLY;
		}
		if (code == GUIPANDLGCODE_ICONDEFPUSHBUTTON) {
			if (res->ctlorgdflt != 0) goto dlgerror;
			res->ctlorgdflt = res->entrycount + 1;  /* save a pointer to the user defined default pushbutton */
		}
		break;
	case GUIPANDLGCODE_PROGRESSBAR:
		control->value2 = 100;
		break;
	case GUIPANDLGCODE_HSCROLLBAR:
	case GUIPANDLGCODE_VSCROLLBAR:
		control->value2 = 1;
		break;
	case GUIPANDLGCODE_BOX:
	case GUIPANDLGCODE_BOXTITLE:
	case GUIPANDLGCODE_TEXT:
	case GUIPANDLGCODE_LTCHECKBOX:
	case GUIPANDLGCODE_CHECKBOX:
	case GUIPANDLGCODE_PUSHBUTTON:
		break;
	case GUIPANDLGCODE_DEFPUSHBUTTON:
		if (res->ctlorgdflt != 0) goto dlgerror;
		res->ctlorgdflt = res->entrycount + 1;  /* save a pointer to the user defined default pushbutton */
		break;
	case GUIPANDLGCODE_ESCPUSHBUTTON:
	case GUIPANDLGCODE_BUTTON:
	case GUIPANDLGCODE_LOCKBUTTON:
	case GUIPANDLGCODE_CTEXT:
	case GUIPANDLGCODE_RTEXT:
	case GUIPANDLGCODE_VTEXT:
	case GUIPANDLGCODE_RVTEXT:
	case GUIPANDLGCODE_CVTEXT:
	case GUIPANDLGCODE_EDIT:
	case GUIPANDLGCODE_MEDIT:
	case GUIPANDLGCODE_MEDITHS:
	case GUIPANDLGCODE_MEDITVS:
	case GUIPANDLGCODE_MEDITS:
	case GUIPANDLGCODE_PEDIT:
	case GUIPANDLGCODE_LEDIT:
	case GUIPANDLGCODE_MLEDIT:
	case GUIPANDLGCODE_MLEDITS:
	case GUIPANDLGCODE_MLEDITHS:
	case GUIPANDLGCODE_MLEDITVS:
	case GUIPANDLGCODE_PLEDIT:
	case GUIPANDLGCODE_TREE:
	case GUIPANDLGCODE_POPBOX:
		break;
	case GUIPANDLGCODE_TABLE:
		control->table.textcolor.color = TEXTCOLOR_DEFAULT;
		control->table.textcolor.code = 0;
		break;
	default:
		goto dlgerror;
	}
dlgend:
	control++;
	res->entrycount++;
	controlunused--;
	return 0;
dlgerror:
	guiUnlockMem(res->content);
	guiresdestroy(pandlgreshandle);
	pandlgreshandle = NULL;
	return RC_ERROR;
#else
	return RC_ERROR;
#endif
}

INT guialertdlgtext(UCHAR *text, INT textlen)
{
#if OS_WIN32GUI
	RESOURCE *res;
	UCHAR *p1;
	CHAR work[64];

	if (alertdlgreshandle == NULL) return RC_INVALID_HANDLE;
	res = *alertdlgreshandle;
	if (res->content != NULL) guiFreeMem(res->content);
	if (textlen) {
		res->content = guiAllocMem(textlen + 1);
		if (res->content == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, textlen + 1);
			return RC_NO_MEM;
		}
		p1 = guiMemToPtr(res->content);
		memcpy(p1, text, textlen);
		p1[textlen] = '\0';
	}
	return 0;
#else
	return RC_ERROR;
#endif
}

/**
 * Set the 'Default' file or directory for an open file, save file, or directory dialog
 * Uses the 'content' field of the RESOURCE structure to hold the info.
 */
INT guifiledlgdefault(UCHAR *filename, INT filenamelen)
{
#if OS_WIN32GUI
	RESOURCE *res;
	UCHAR *p1;
	CHAR work[64];

	if (filedlgreshandle == NULL) return RC_INVALID_HANDLE;
	res = *filedlgreshandle;
	if (res->content != NULL) guiFreeMem(res->content);
	if (filenamelen) {
		res->content = guiAllocMem(filenamelen + 1);
		if (res->content == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, filenamelen + 1);
			return RC_NO_MEM;
		}
		p1 = guiMemToPtr(res->content);
		memcpy(p1, filename, filenamelen);
		p1[filenamelen] = '\0';
	}
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guifiledlgTitle(UCHAR *data, INT cbData)
{
#if OS_WIN32GUI
	RESOURCE *res;
	CHAR work[64];
	if (filedlgreshandle == NULL) return RC_INVALID_HANDLE;
	res = *filedlgreshandle;
	if (res->title != NULL) guiFreeMem(res->title);
	if (cbData) {
		res->title = guiAllocString(data, cbData);
		if (res->title == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocString returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, cbData);
			return RC_NO_MEM;
		}
		memcpy(guiMemToPtr(res->title), data, cbData);
	}
	return 0;
#else
	return RC_ERROR;
#endif
}

/**
 * Request a multiple select open file dialog
 */
INT guifiledlgMulti()
{
#if OS_WIN32GUI
	RESOURCE *res;
	if (filedlgreshandle == NULL) return RC_INVALID_HANDLE;
	res = *filedlgreshandle;
	res->openFileDlgMultiSelect = TRUE;
	return 0;
#else
	return RC_ERROR;
#endif
}

/**
 * Set the 'DeviceFilter' CSV set for a directory dialog
 * Uses the 'deviceFilters' field of the RESOURCE structure to hold the info.
 */
INT guifiledlgDeviceFilter(UCHAR *data, INT cbData)
{
#if OS_WIN32GUI
	RESOURCE *res;
	CHAR work[64];

	if (filedlgreshandle == NULL) return RC_INVALID_HANDLE;
	res = *filedlgreshandle;
	if (res->deviceFilters != NULL) guiFreeMem(res->deviceFilters);
	if (cbData) {
		res->deviceFilters = guiAllocString(data, cbData);
		if (res->deviceFilters == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, cbData + 1);
			return RC_NO_MEM;
		}
		memcpy(guiMemToPtr(res->deviceFilters), data, cbData);
	}
	else res->deviceFilters = NULL;
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guialertdlgend()
{
#if OS_WIN32GUI
	if (alertdlgreshandle == NULL) return RC_INVALID_HANDLE;
	alertdlgreshandle = NULL;
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guifiledlgend()
{
#if OS_WIN32GUI
	if (filedlgreshandle == NULL) return RC_INVALID_HANDLE;
	filedlgreshandle = NULL;
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guifontdlgdefault(UCHAR *fontname, INT fontnamelen)
{
#if OS_WIN32GUI
	RESOURCE *res;
	UCHAR *p1;
	CHAR work[64];

	if (fontdlgreshandle == NULL) return RC_INVALID_HANDLE;
	res = *fontdlgreshandle;
	if (res->content != NULL) guiFreeMem(res->content);
	if (fontnamelen) {
		res->content = guiAllocMem(fontnamelen + 1);
		if (res->content == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, fontnamelen + 1);
			return RC_NO_MEM;
		}
		p1 = guiMemToPtr(res->content);
		memcpy(p1, fontname, fontnamelen);
		p1[fontnamelen] = '\0';
	}
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guifontdlgend()
{
#if OS_WIN32GUI
	if (fontdlgreshandle == NULL) return RC_INVALID_HANDLE;
	fontdlgreshandle = NULL;
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guicolordlgdefault(UCHAR *colorstring, INT colorstringlen)
{
#if OS_WIN32GUI
	RESOURCE *res;
	UCHAR c1, *p1;

	if (colordlgreshandle == NULL) return RC_INVALID_HANDLE;
	res = *colordlgreshandle;
	res->color = 0;
	for (p1 = colorstring; colorstringlen; colorstringlen--, p1++) {
		if (isdigit(*p1)) res->color = res->color * 16 + *p1 - '0';
		else {
			c1 = toupper(*p1);
			if (c1 < 'A' || c1 > 'F') break;
			res->color = res->color * 16 + *p1 - 'A' + 10;
		}
	}
	if (colorstringlen) return RC_ERROR;
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guicolordlgend()
{
#if OS_WIN32GUI
	if (colordlgreshandle == NULL) return RC_INVALID_HANDLE;
	colordlgreshandle = NULL;
	return 0;
#else
	return RC_ERROR;
#endif
}

/**
 * destroy a resource
 */
INT guiresdestroy(RESHANDLE reshandle)
{
#if OS_WIN32GUI
	INT rc;
	RESHANDLE rh, rh1;
	RESOURCE *res;
	MENUENTRY *menuitem;
	CONTROL *control;

	(*reshandle)->isBeingDestroyed = TRUE;
	if (rc = guireshide(reshandle)) return rc;

	/* Remove this reshandle from the linked list of res handles */
	for (rh = reshandlehdr; rh != NULL && rh != reshandle; rh = (*rh)->nextreshandle) rh1 = rh;
	if (rh == reshandlehdr) reshandlehdr = (*rh)->nextreshandle;
	else (*rh1)->nextreshandle = (*rh)->nextreshandle;

	res = *rh;
	switch (res->restype) {
	case RES_TYPE_MENU:
	case RES_TYPE_POPUPMENU:
		if (res->content != NULL) {
			menuitem = (MENUENTRY *) guiMemToPtr(res->content);
			while (res->entrycount--) {
				if (menuitem->text != NULL) guiFreeMem(menuitem->text);
				menuitem++;
			}
		}
		break;
	case RES_TYPE_PANEL:
	case RES_TYPE_DIALOG:
		if (res->content != NULL) control = (CONTROL *) guiMemToPtr(res->content);
		while (res->entrycount--) {
			if (control->type == PANEL_TABLE) guiresdestroytable(control);
			else guiaDestroyControl(control);
			if (control->mask != NULL) guiFreeMem(control->mask);
			if (control->textval != NULL) guiFreeMem(control->textval);
			if (control->changetext != NULL) guiFreeMem(control->changetext);
			if (control->listboxtabs != NULL) guiFreeMem(control->listboxtabs);
			if (control->listboxjust != NULL) guiFreeMem(control->listboxjust);
			if (control->tooltip != NULL) guiFreeMem(control->tooltip);
			control++;
		}
		break;
	case RES_TYPE_TOOLBAR:
		if (res->content != NULL) {
			control = (CONTROL *) guiMemToPtr(res->content);
			while (res->entrycount--) {
				if (control->tooltip != NULL) guiFreeMem(control->tooltip);
				control++;
			}
		}
		break;
	case RES_TYPE_ICON:
		guiaFreeIcon(res);
		break;
	}

	if (res->content != NULL) guiFreeMem(res->content);
	if (res->namefilter != NULL) guiFreeMem(res->namefilter);
	if (res->title != NULL) guiFreeMem(res->title);
	if (res->deviceFilters != NULL) guiFreeMem(res->deviceFilters);
	memfree((void *) rh);
	return 0;
#else
	return RC_ERROR;
#endif
}


#if OS_WIN32GUI

static void guiresdestroytable(CONTROL *control) {

	PTABLECOLUMN tblColumns;
	PTABLEROW tblRow;
	PTABLECELL cell;
	UINT i1, i2;
	ZHANDLE *pZh;

	tblColumns = guiLockMem(control->table.columns);
	if (control->table.rows != NULL) {
		pZh = guiLockMem(control->table.rows);
		for (i1 = 0; i1 < control->table.numrows; i1++) {
			tblRow = guiLockMem(pZh[i1]);
			for (i2 = 0; i2 < control->table.numcolumns; i2++) {
				cell = &tblRow->cells[i2];
				if (ISTABLECOLUMNTEXT(tblColumns[i2].type)) if (cell->text != NULL) guiFreeMem(cell->text);
				/* If this column was a DROPBOX, this cell might have an array of ZHANDLES to free */
				if (tblColumns[i2].type == PANEL_DROPBOX) guiresdestroytabledb(&cell->dbcell.db);
			}
			guiUnlockMem(pZh[i1]);
			guiFreeMem(pZh[i1]);
		}
		guiUnlockMem(control->table.rows);
		guiFreeMem(control->table.rows);
	}

	for (i1 = 0; i1 < control->table.numcolumns; i1++) {
		if (tblColumns[i1].name != NULL) guiFreeMem(tblColumns[i1].name);
		if (tblColumns[i1].mask != NULL) guiFreeMem(tblColumns[i1].mask);
		/* If this column was a CDROPBOX, it might have an array of ZHANDLES to free */
		if (tblColumns[i1].type == PANEL_CDROPBOX) guiresdestroytabledb(&tblColumns[i1].db);
	}
	guiFreeMem(control->table.columns);
}

static void guiresdestroytabledb(PTABLEDROPBOX dropbox) {
	UINT i1;
	ZHANDLE *zhArray;

	if (dropbox->itemarray != NULL) {
		zhArray = guiLockMem(dropbox->itemarray);
		for (i1 = 0; i1 < dropbox->itemcount; i1++) {
			if (zhArray[i1] != NULL) guiFreeMem(zhArray[i1]);
		}
		guiUnlockMem(dropbox->itemarray);
		guiFreeMem(dropbox->itemarray);
		dropbox->itemarray = NULL;
	}
}


#endif


INT guiresmsgfnc(RESHANDLE reshandle, RESCALLBACK fnc)  /* callback function for resource messages */
{
#if OS_WIN32GUI
	RESHANDLE rh;

	for (rh = reshandlehdr; rh != NULL && rh != reshandle; rh = (*rh)->nextreshandle);
	if (rh == NULL) return RC_INVALID_HANDLE;
	(*rh)->cbfnc = fnc;
	return 0;
#else
	return RC_ERROR;
#endif
}

INT guiresquery(RESHANDLE reshandle, UCHAR *fnc, INT fnclen, UCHAR *data, INT *datalen)  /* query a resource */
{
#if OS_WIN32GUI
	INT i, i2, useritem, subuseritem;
	INT cmd, curpos, destlen;
	RESHANDLE rh;
	RESOURCE *res;
	MENUENTRY *menuitem;
	CONTROL *control;
	UCHAR *title;
	PTABLECOLUMN tblColumns;
	UINT column;

	for (rh = reshandlehdr; rh != NULL && rh != reshandle; rh = (*rh)->nextreshandle);
	if (rh == NULL) return RC_INVALID_HANDLE;
	cmd = parsefnc(fnc, fnclen, &useritem, &subuseritem);
	res = *rh;
	if (res->restype == RES_TYPE_MENU || res->restype == RES_TYPE_POPUPMENU) {
		if (cmd != GUI_STATUS) return RC_INVALID_VALUE;
		menuitem = (MENUENTRY *) guiMemToPtr(res->content);
		for (i = 0; i < res->entrycount; i++, menuitem++) {
			if (menuitem->useritem == (ULONG) useritem) {
				if (*datalen > 0) {
					data[0] = 'N';
					if (menuitem->style & MENUSTYLE_MARKED) data[0] = 'Y';
					if (*datalen > 1) {
						data[1] = 'A';
						if (menuitem->style & MENUSTYLE_DISABLED) data[1] = 'G';
						*datalen = 2;
					}
					else *datalen = 1;
				}
				else *datalen = 0;
				return 0;
			}
		}
		goto queryfail;
	}
	else if (res->restype == RES_TYPE_TOOLBAR) {
		if (cmd != GUI_STATUS) return RC_INVALID_VALUE;
		control = (CONTROL *) guiMemToPtr(res->content);
		for (i = 0; i < res->entrycount && control->useritem != useritem; i++, control++);
		if (i < res->entrycount) {
			if (control->type == TOOLBAR_DROPBOX) {
				if (*datalen > 0) {
					if (res->linemsgs) {
						if (*datalen < 5) {
							*datalen = 0;
							return 0;
						}
						itonum5(control->value, data);
						*datalen = 5;
					}
					else {
						if (control->textval == NULL) *datalen = 0;
						else {
							destlen = *datalen;
							title = (UCHAR *) guiMemToPtr(control->textval);
							for (curpos = 0; title[curpos] && curpos < destlen; curpos++) {
								data[curpos] = title[curpos];
							}
							*datalen = curpos;
						}
					}
				}
			}
			else {
				if (*datalen > 0) {
					data[0] = 'N';
					if (control->style & CTRLSTYLE_MARKED) data[0] = 'Y';
					if (*datalen > 1) {
						data[1] = 'A';
						if (ISGRAY(control)) data[1] = 'G';
						*datalen = 2;
					}
					else *datalen = 1;
				}
				else *datalen = 0;
			}
			return 0;
		}
		goto queryfail;
	}
	else if (res->restype != RES_TYPE_PANEL && res->restype != RES_TYPE_DIALOG) return RC_INVALID_VALUE;

	control = (CONTROL *) guiMemToPtr(res->content);
	for (i = 0; i < res->entrycount; i++, control++) {
		if (control->type == PANEL_TABLE) {
			// Search through table columns to find match to useritem
		 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
		 	for (column = 0; column < control->table.numcolumns && tblColumns->useritem != useritem; column++, tblColumns++);
		 	guiUnlockMem(control->table.columns);
			if (column < control->table.numcolumns) {
				/* This useritem refers to a column in a table, go query it */
				control->table.changeposition.x = column + 1;		/* needs to be one-based yet */
				control->table.changeposition.y = subuseritem;
				return guiresquerytable(res, cmd, data, datalen, control);
			}
		}
		if (control->useritem == useritem) {
			if (control->type == PANEL_TABLE) {
				return guiresquerytable(res, cmd, data, datalen, control);
			}
			if (cmd != GUI_STATUS) return RC_INVALID_VALUE;
			switch (control->type) {
			case PANEL_POPBOX:
				destlen = *datalen;
				if (destlen > 0) {
					if (control->textval == NULL) *datalen = 0;
					else {
						title = (UCHAR *) guiMemToPtr(control->textval);
						i2 = (INT) strlen((CHAR *)title);
						if (destlen < i2) i2 = destlen;
						memcpy(data, title, i2);
						*datalen = i2;
					}
				}
				break;
			case PANEL_TEXT:
			case PANEL_VTEXT:
			case PANEL_CVTEXT:
			case PANEL_CTEXT:
			case PANEL_BOXTITLE:
			case PANEL_EDIT:
			case PANEL_MEDIT:
			case PANEL_MEDITHS:
			case PANEL_MEDITVS:
			case PANEL_MEDITS:
			case PANEL_PEDIT:
			case PANEL_FEDIT:
			case PANEL_TAB:
			case PANEL_LEDIT:
			case PANEL_MLEDIT:
			case PANEL_MLEDITVS:
			case PANEL_MLEDITHS:
			case PANEL_MLEDITS:
			case PANEL_PLEDIT:
				if (*datalen > 0) {
					if (res->winshow == NULL) {
						if (control->textval == NULL) {
							*datalen = 0;
						}
						else {
							destlen = *datalen;
							title = (UCHAR *) guiLockMem(control->textval);
							for (curpos = 0; title[curpos] && curpos < destlen; curpos++) {
								data[curpos] = title[curpos];
							}
							*datalen = curpos;
							guiUnlockMem(control->textval);
						}
					}
					else {
						*datalen = guiaGetControlText(control, (CHAR *) data, *datalen);
					}
				}
				break;
			case PANEL_MLISTBOXHS:
			case PANEL_MLISTBOX:
				if (*datalen > 0) guiaMListboxCreateList(res, control);
				/* drop through */
			case PANEL_LISTBOX:
			case PANEL_LISTBOXHS:
			case PANEL_DROPBOX:
				if (*datalen > 0) {
					destlen = *datalen;
					if (ISMULTISELLISTBOX(control)) {
						title = (UCHAR *) guiMemToPtr(control->mlbsellist);
						for (curpos = 0; title[curpos] && curpos < destlen; curpos++) data[curpos] = title[curpos];
						*datalen = curpos;
					}
					else {
						if (control->textval == NULL) *datalen = 0;
						else if (res->linemsgs) {
							if (*datalen < 5 || !control->value) {
								*datalen = 0;
								return 0;
							}
							itonum5(control->value, data);
							*datalen = 5;
						}
						else {
							title = (UCHAR *) guiMemToPtr(control->textval);
							for (curpos = 0; title[curpos] && curpos < destlen; curpos++) {
								data[curpos] = title[curpos];
							}
							*datalen = curpos;
						}
					}
				}
				break;
			case PANEL_CHECKBOX:
			case PANEL_LTCHECKBOX:
			case PANEL_BUTTON:
			case PANEL_RADIOBUTTON:
			case PANEL_LASTRADIOBUTTON:
			case PANEL_PUSHBUTTON:
			case PANEL_DEFPUSHBUTTON:
			case PANEL_LOCKBUTTON:
			case PANEL_ICONPUSHBUTTON:
			case PANEL_ICONLOCKBUTTON:
			case PANEL_ICONDEFPUSHBUTTON:
				if (*datalen > 0) {
					data[0] = 'N';
					if (control->style & CTRLSTYLE_MARKED) data[0] = 'Y';
					if (*datalen > 1) {
						data[1] = 'A';
						if (ISGRAY(control)) data[1] = 'G';
						*datalen = 2;
					}
					else *datalen = 1;
				}
				else *datalen = 0;
				break;
			case PANEL_PROGRESSBAR:
			case PANEL_HSCROLLBAR:
			case PANEL_VSCROLLBAR:
				itonum5(control->value, data);
				*datalen = 5;
				break;
			case PANEL_TREE:
				if (*datalen > 0) {
					*datalen = guiaGetControlText(control, (CHAR *) data, *datalen);
				}
				else *datalen = 0;
				break;
			default:
				goto queryfail;
			}
			return 0;
		}
	}

queryfail:
	*datalen = 0;
	return RC_ERROR;
#else
	return RC_ERROR;
#endif
}


#if OS_WIN32GUI

static INT guiresquerytable(RESOURCE *res, INT cmd, UCHAR *data, INT *datalen, CONTROL *control) {


	PTABLECOLUMN tblColumns;
	PTABLEROW tblRow;
	ZHANDLE zhRow, *pzh, zh;
	PTABLECELL cell;
	CHAR *text;
	INT i1, column;

	switch (cmd) {

	case GUI_GETROWCOUNT:
		if (*datalen < 5) {
			*datalen = 0;
			return 0;
		}
		itonum5(control->table.numrows, data);
		*datalen = 5;
		break;

	case GUI_STATUS:
		if (*datalen < 1) return 0;
		if (control->table.changeposition.y < 1 || (UINT) control->table.changeposition.y > control->table.numrows)
			return RC_INVALID_VALUE;
		if (control->table.changeposition.x < 1 || (UINT) control->table.changeposition.x > control->table.numcolumns)
			return RC_INVALID_VALUE;

		// Get the TABLECOLUMN so we know the type
		tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
		zhRow = guiTableLocateRow(control, control->table.changeposition.y);
		tblRow = (PTABLEROW) guiLockMem(zhRow);
		column = control->table.changeposition.x - 1;
		cell = &tblRow->cells[column];

		switch (tblColumns[column].type) {

		case PANEL_VTEXT:
		case PANEL_RVTEXT:
		case PANEL_POPBOX:
			if (cell->text != NULL) {
				text = (CHAR*) guiLockMem(cell->text);
				i1 = (INT) strlen(text);
				i1 = min(*datalen, i1);
				memcpy(data, text, i1);
				*datalen = i1;
				guiUnlockMem(cell->text);
			}
			else *datalen = 0;
			break;

		case PANEL_CHECKBOX:
			*datalen = 1;
			data[0] = (cell->style & CTRLSTYLE_MARKED) ? 'Y' : 'N';
			break;

		case PANEL_EDIT:
		case PANEL_LEDIT:
			if (control->ctlhandle == NULL) {
				if (cell->text != NULL) {
					text = (CHAR*) guiLockMem(cell->text);
					i1 = (INT) strlen(text);
					i1 = min(*datalen, i1);
					memcpy(data, text, i1);
					*datalen = i1;
					guiUnlockMem(cell->text);
				}
				else *datalen = 0;
			}
			else *datalen = guiaGetTableCellText(control, (CHAR *)data, *datalen);
			break;

		case PANEL_CDROPBOX:
		case PANEL_DROPBOX:
			if (control->ctlhandle != NULL) {
				cell->dbcell.itemslctd = guiaGetTableCellSelection(control);
			}
			if (res->linemsgs) {
				if (*datalen > 5) *datalen = 5;
				msciton(cell->dbcell.itemslctd, data, *datalen);
			}
			else {
				if (cell->dbcell.itemslctd == 0) *datalen = 0;
				else {
					if (tblColumns[column].type == PANEL_DROPBOX) zh = cell->dbcell.db.itemarray;
					else zh = tblColumns[column].db.itemarray;
					if (zh == NULL) {
						*datalen = 0;
					}
					else {
						pzh = (ZHANDLE*) guiLockMem(zh);
						text = (CHAR*) guiLockMem(pzh[cell->dbcell.itemslctd - 1]);
						i1 = (INT) strlen(text);
						i1 = min(*datalen, i1);
						memcpy(data, text, i1);
						*datalen = i1;
						guiUnlockMem(pzh[cell->dbcell.itemslctd - 1]);
						guiUnlockMem(zh);
					}
				}
			}
			break;
		}
		guiUnlockMem(zhRow);
		guiUnlockMem(control->table.columns);
		break;

	default:
		return RC_INVALID_VALUE;
		break;
	}
	return 0;
}
#endif /* OS_WIN32GUI */

#if OS_WIN32GUI

static INT changeTextColorData(CHAR* data, INT datalen, CONTROL* control) {
	INT n1;
	CHAR work[128];
	if (data == NULL) return RC_INVALID_PARM;
	while (*data == ' ') {data++; datalen--;}
	for (n1 = 0; data[n1] != ' ' && n1 < datalen; n1++);
	if (GetColorNumberFromTextExt(data, n1, &control->changecolor, NULL) == RC_ERROR) return RC_ERROR;
	if (control->changetext != NULL) guiFreeMem(control->changetext);
	n1++;
	control->changetext = guiAllocString(data + n1, datalen - n1);
	if (control->changetext == NULL) {
		sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s", __FUNCTION__);
		guiaErrorMsg(work, datalen - n1);
		return RC_NO_MEM;
	}
	return 0;
}


static INT changeTextColorLine(CHAR* data, INT datalen, CONTROL* control) {
	INT n1;
	if (data == NULL) return RC_INVALID_PARM;
	while (*data == ' ') {data++; datalen--;}
	for (n1 = 0; data[n1] != ' ' && n1 < datalen; n1++);
	if (GetColorNumberFromTextExt(data, n1, &control->changecolor, NULL) == RC_ERROR) return RC_ERROR;
	n1++;
	numtoi(data + n1, datalen - n1, &n1);
	control->value3 = n1;
	return 0;
}

static INT changeListOrDropboxBGColorLine(CHAR* data, INT datalen, CONTROL* control) {
	INT n1;
	BOOL defcolor;
	if (data == NULL) return RC_INVALID_PARM;
	//if (control->type != TOOLBAR_DROPBOX) return RC_ERROR;
	while (*data == ' ') {data++; datalen--;}
	for (n1 = 0; data[n1] != ' ' && n1 < datalen; n1++);
	if (GetColorNumberFromTextExt(data, n1, &control->changecolor, &defcolor) == RC_ERROR) return RC_ERROR;
	n1++;
	numtoi(data + n1, datalen - n1, &n1);
	control->value3 = n1;
	control->userBGColor = !defcolor;
	return 0;
}

static INT changeListOrDropboxBGColorData(CHAR* data, INT datalen, CONTROL* control) {
	INT n1;
	BOOL defcolor;
	CHAR work[128];
	if (data == NULL) return RC_INVALID_PARM;
	while (*data == ' ') {data++; datalen--;}
	for (n1 = 0; data[n1] != ' ' && n1 < datalen; n1++);
	if (GetColorNumberFromTextExt(data, n1, &control->changecolor, &defcolor) == RC_ERROR) return RC_ERROR;
	if (control->changetext != NULL) guiFreeMem(control->changetext);
	n1++;
	control->changetext = guiAllocString(data + n1, datalen - n1);
	if (control->changetext == NULL) {
		sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s", __FUNCTION__);
		guiaErrorMsg(work, datalen - n1);
		return RC_NO_MEM;
	}
	control->userBGColor = !defcolor;
	return 0;
}
#endif

/*
 * change application window, the 'main' db/c window
 */
INT guiappchange(UCHAR *fnc, INT fnclen, UCHAR *data, INT datalen)
{
#if OS_WIN32GUI
	CHAR cmd[20];
	INT pos;
	UCHAR resname[MAXRESNAMELEN];
	RESHANDLE iconrh;

	/* convert command to lower case */
	for (pos = 0; pos < fnclen && fnc[pos] != ' ' && !isdigit(fnc[pos]); pos++) {
		cmd[pos] = tolower(fnc[pos]);
	}
	cmd[pos] = '\0';
	if (!strcmp(cmd, "desktopicon") || !strcmp(cmd, "dialogicon")) {
		if (datalen <= 0 || datalen > MAXRESNAMELEN) return RC_INVALID_VALUE;
		memset(resname, ' ', MAXRESNAMELEN);
		memcpy(resname, data, datalen);
		if ((iconrh = locateIconByName(resname)) == NULL) return RC_ERROR;
		if (guiaChangeAppIcon(*iconrh, cmd) == 0) {
			return 0;
		}
	}
	else if (!strcmp(cmd, "title") || !strcmp(cmd, "taskbartext")) {
		data[datalen] = '\0';
		if (guiaChangeAppWinTitle((CHAR *) data) == 0) return 0;
	}
	else if (!strcmp(cmd, "consolefocus")) {
		if (guiaChangeConsoleWinFocus() == 0) return 0;
	}
	else if (!strcmp(cmd, "notaskbarbutton")) {
		// Quietly ignore this, as of 16.1 the main db/c window does not appear on the taskbar
		//if (guiaChangeAppWinRemoveTaskbarButton() == 0)
			return 0;
	}
	return RC_ERROR;
#endif
	return RC_ERROR;
}


#if OS_WIN32GUI

static INT guireschangeother(RESHANDLE reshandle, INT cmd, UCHAR *data, INT datalen) {
	RESOURCE *res = *reshandle;
	INT i1;
	CHAR work[128];

	if (res->restype == RES_TYPE_OPENFILEDLG || res->restype == RES_TYPE_SAVEASFILEDLG) {
		if (cmd == GUI_FILENAME) {
			if (res->content != NULL) {
				guiFreeMem(res->content);
				res->content = NULL;
			}
			if (datalen) {
				res->content = guiAllocString(data, datalen);
				if (res->content == NULL) {
					sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at GUI_FILENAME", __FUNCTION__);
					guiaErrorMsg(work, datalen);
					return RC_NO_MEM;
				}
			}
		}
		else if (cmd == GUI_NAMEFILTER) {
			if (res->namefilter != NULL) {
				guiFreeMem(res->namefilter);
				res->namefilter = NULL;
			}
			/* The name filter list needs to end in two nulls */
			if (datalen) {
				for (i1 = 0; i1 < datalen; i1++) if (data[i1] == ',') data[i1] = '\0';
				res->namefilter = guiAllocString(data, datalen + 2);
				if (res->namefilter == NULL) {
					sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at GUI_NAMEFILTER", __FUNCTION__);
					guiaErrorMsg(work, datalen + 2);
					return RC_NO_MEM;
				}
				res->namefilter[datalen] = '\0';
				res->namefilter[datalen + 1] = '\0';
			}
		}
		else return RC_ERROR;
	}
	else if (res->restype == RES_TYPE_OPENDIRDLG) {
		if (cmd != GUI_DIRNAME) return RC_ERROR;
		if (res->content != NULL) guiFreeMem(res->content);
		if (datalen) res->content = guiAllocString(data, datalen);
		if (res->content == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at RES_TYPE_OPENDIRDLG", __FUNCTION__);
			guiaErrorMsg(work, datalen);
			return RC_NO_MEM;
		}
	}
	else if (res->restype == RES_TYPE_FONTDLG) {
		if (cmd != GUI_FONTNAME) return RC_ERROR;
		if (res->content != NULL) guiFreeMem(res->content);
		if (datalen) res->content = guiAllocString(data, datalen);
		if (res->content == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at RES_TYPE_FONTDLG", __FUNCTION__);
			guiaErrorMsg(work, datalen);
			return RC_NO_MEM;
		}
	}
	else if (res->restype == RES_TYPE_COLORDLG) {
		if (cmd != GUI_COLOR || datalen < 15) return RC_ERROR;
		num5toi(data, &i1);
		res->color = (INT32) i1;
		num5toi(data + 5, &i1);
		res->color += (INT32) (i1 << 8);
		num5toi(data + 10, &i1);
		res->color += (INT32) (i1 << 16);
	}
	else return RC_ERROR;
	return 0;
}

/**
 * Processes change commands to MENUs and POPUPMENUs.
 *
 * Does not have to verify that reshandle is valid. That is done by the caller.
 */
static INT guireschangemenu(RESHANDLE reshandle, INT cmd, MENUENTRY *menuitem, UCHAR *data, INT datalen)
{
	RESOURCE *res = *reshandle;
	CHAR work[64];

	switch(cmd) {
	case GUI_MARK:
		menuitem->style |= MENUSTYLE_MARKED;
		break;
	case GUI_UNMARK:
		menuitem->style &= ~MENUSTYLE_MARKED;
		break;
	case GUI_GRAY:
		menuitem->style |= MENUSTYLE_DISABLED;
		break;
	case GUI_AVAILABLE:
		menuitem->style &= ~MENUSTYLE_DISABLED;
		break;
	case GUI_TEXT:
		if (data == NULL) return RC_INVALID_VALUE;	/* It can be a zero-length string, it cannot be NULL */
		if (menuitem->text != NULL) guiFreeMem(menuitem->text);
		guiLockMem(res->content);
		menuitem->text = guiAllocString(data, datalen);
		guiUnlockMem(res->content);
		if (menuitem->text == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s", __FUNCTION__);
			guiaErrorMsg(work, datalen);
			return RC_NO_MEM;
		}
		break;
	case GUI_HIDE:
		if (menuitem->style & MENUSTYLE_HIDDEN) return 0;
		break;
	case GUI_SHOW:
		if (!(menuitem->style & MENUSTYLE_HIDDEN)) return 0;
		break;
	default:
		return RC_ERROR;
	}
	/**
	 * For popup menus, for hide and show,
	 * never call guia routines.
	 */
	if (res->restype != RES_TYPE_POPUPMENU || (cmd != GUI_HIDE && cmd != GUI_SHOW)) {
		if (res->winshow) {
			if (!(menuitem->style & MENUSTYLE_HIDDEN) || GUI_SHOW == cmd) {
				if (guiaChangeMenu(res, menuitem, cmd) < 0) {
					return RC_ERROR;
				}
			}
		}
	}
	if (GUI_HIDE == cmd) menuitem->style |= MENUSTYLE_HIDDEN;
	else if (GUI_SHOW == cmd) menuitem->style &= ~MENUSTYLE_HIDDEN;
	return 0;
}

static INT guireschangetoolbar(RESHANDLE reshandle, INT cmd, INT useritem, UCHAR *data, INT datalen) {

	RESOURCE *res = *reshandle;
	RESHANDLE iconrh;
	CONTROL *control;
	INT hsize, vsize, i1, n1, n2, n3;
	UCHAR resname[MAXRESNAMELEN];
	ZHANDLE ptr;
	UCHAR** stringArray;
	CHAR work[64];

	if (cmd == GUI_ITEMON) {
		res->itemmsgs = TRUE;
		return 0;
	}
	else if (cmd == GUI_ITEMOFF) {
		res->itemmsgs = FALSE;
		return 0;
	}
	else if (cmd == GUI_LINEON) {
		res->linemsgs = TRUE;
		return 0;
	}
	else if (cmd == GUI_LINEOFF) {
		res->linemsgs = FALSE;
		return 0;
	}
	else if (cmd == GUI_ADDPUSHBUTTON || cmd == GUI_ADDLOCKBUTTON || cmd == GUI_ADDDROPBOX || cmd == GUI_ADDSPACE) {
		/* adding a toolbar item */
		iconrh = NULL;
		if (datalen && cmd != GUI_ADDSPACE) {
			if (cmd == GUI_ADDDROPBOX) {
				if (datalen < 10) return RC_ERROR;
				num5toi(data, &hsize);
				num5toi(data + 5, &vsize);
			}
			else {
				if (datalen < 0 || datalen > MAXRESNAMELEN) return RC_INVALID_VALUE;
				memset(resname, ' ', MAXRESNAMELEN);
				memcpy(resname, data, datalen);
				if ((iconrh = locateIconByName(resname)) == NULL) return RC_ERROR;
			}
		}
		if (!res->entrycount) {
			ptr = guiAllocMem(sizeof(CONTROL));
			if (ptr == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s", __FUNCTION__);
				guiaErrorMsg(work, sizeof(CONTROL));
				return RC_NO_MEM;
			}
		}
		else {
			ptr = guiReallocMem(res->content, sizeof(CONTROL) * (res->entrycount + 1));
			if (ptr == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiReallocMem returned null in %s", __FUNCTION__);
				guiaErrorMsg(work, sizeof(CONTROL) * (res->entrycount + 1));
				return RC_NO_MEM;
			}
		}
		res->content = ptr;
		control = (CONTROL *) guiMemToPtr(res->content);
		if (res->entrycount) {
			i1 = 0;
			if (res->insertpos) {
				while (i1 < res->entrycount && res->insertpos != control->useritem) i1++, control++;
				if (i1 < res->entrycount && res->appendflag) i1++, control++;
			}
			else if (res->appendflag) {
				i1 = res->entrycount;
				control += res->entrycount;
			}
			memmove(control + 1, control, sizeof(CONTROL) * (res->entrycount - i1));
		}
		memset(control, 0, sizeof(CONTROL));
		control->useritem = useritem;
		control->res = reshandle;
		control->iconres = iconrh;
		if (cmd == GUI_ADDSPACE) {
			control->type = TOOLBAR_SPACE;
			useritem = 0;
		}
		else if (cmd == GUI_ADDPUSHBUTTON) {
			control->type = TOOLBAR_PUSHBUTTON;
		}
		else if (cmd == GUI_ADDLOCKBUTTON) {
			control->type = TOOLBAR_LOCKBUTTON;
		}
		else if (cmd == GUI_ADDDROPBOX) {
			control->type = TOOLBAR_DROPBOX;
			control->style |= CTRLSTYLE_INSERTORDER;
			control->insstyle = LIST_INSERTNORMAL;
			control->rect.left = control->rect.top = 0;
			control->rect.right = hsize;
			control->rect.bottom = vsize;
		}
		if (res->winshow != NULL) n1 = guiaChangeToolbar(res, control, cmd);
		else n1 = 0;
		res->entrycount++;
		return n1;
	}

	if (!useritem) {
		if (cmd == GUI_INSERTBEFORE) res->appendflag = FALSE;
		else if (cmd == GUI_INSERTAFTER) res->appendflag = TRUE;
		else return RC_ERROR;
		res->insertpos = 0;
		return 0;
	}

	control = (CONTROL *) guiMemToPtr(res->content);
	for (i1 = 0; i1 < res->entrycount && control->useritem != useritem; i1++, control++);
	if (i1 >= res->entrycount) return RC_ERROR;

	if (cmd == GUI_INSERTBEFORE) {
		res->insertpos = useritem;
		res->appendflag = FALSE;
		return 0;
	}
	else if (cmd == GUI_INSERTAFTER) {
		res->insertpos = useritem;
		res->appendflag = TRUE;
		return 0;
	}

	switch(cmd) {
	case GUI_MARK:
		if (control->type == TOOLBAR_DROPBOX) return RC_ERROR;
		control->style |= CTRLSTYLE_MARKED;
		break;
	case GUI_UNMARK:
		if (control->type == TOOLBAR_DROPBOX) return RC_ERROR;
		control->style &= ~CTRLSTYLE_MARKED;
		break;
	case GUI_AVAILABLE:
		control->style &= ~CTRLSTYLE_DISABLED;
		break;
	case GUI_GRAY:
		control->style |= CTRLSTYLE_DISABLED;
		break;
	case GUI_HELPTEXT:
	case GUI_TEXT:
		if (control->tooltip != NULL) guiFreeMem(control->tooltip);
		control->tooltip = guiAllocString(data, datalen);
		if (control->tooltip == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at GUI_TEXT", __FUNCTION__);
			guiaErrorMsg(work, datalen);
			return RC_NO_MEM;
		}
		break;
	case GUI_ICON:
		if (control->type == TOOLBAR_DROPBOX) return RC_ERROR;
		if (datalen <= 0 || datalen > MAXRESNAMELEN) return RC_INVALID_VALUE;
		memset(resname, ' ', MAXRESNAMELEN);
		memcpy(resname, data, datalen);
		if ((iconrh = locateIconByName(resname)) == NULL) return RC_ERROR;
		control->iconres = iconrh;
		break;
	case GUI_REMOVESPACEAFTER:
	case GUI_REMOVESPACEBEFORE:
	case GUI_REMOVECONTROL:
	case GUI_DELETE:
		if (cmd == GUI_DELETE && control->type == TOOLBAR_DROPBOX) {
			/* delete a single line inside dropbox */
			if (control->changetext != NULL) guiFreeMem(control->changetext);
			control->changetext = guiAllocString(data, datalen);
			if (control->changetext == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at GUI_DELETE", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
		}
		else {
			if (cmd == GUI_REMOVESPACEAFTER) {
				if (i1 == res->entrycount - 1 || (control + 1)->type != TOOLBAR_SPACE) {
					return RC_ERROR;
				}
				/* position to the space after the control identified by user */
				i1++;
				control++;
			}
			else if (cmd == GUI_REMOVESPACEBEFORE) {
				if (i1 == 0 || (control - 1)->type != TOOLBAR_SPACE) {
					return RC_ERROR;
				}
				/* position to the space before the control identified by user */
				i1--;
				control--;
			}
			guiaDeleteToolbarControl(res, control, i1);
			memmove(control, control + 1, sizeof(CONTROL) * (res->entrycount - i1 - 1));
			res->entrycount--;
			ptr = guiReallocMem(res->content, sizeof(CONTROL) * (res->entrycount));
			if (ptr != NULL) res->content = ptr;
			else {
				sprintf_s(work, ARRAYSIZE(work), "guiReallocMem failed in %s at GUI_DELETE", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
			if (i1 == res->insertpos) {
				res->insertpos = 0;
				res->appendflag = TRUE;
			}
			return 0;
		}
		break;
	case GUI_INSERT:
	case GUI_SELECT:
	case GUI_LOCATE:
		if (control->changetext != NULL) guiFreeMem(control->changetext);
		control->changetext = guiAllocString(data, datalen);
		if (control->changetext == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at GUI_INSERT", __FUNCTION__);
			guiaErrorMsg(work, datalen);
			return RC_NO_MEM;
		}
		break;
	case GUI_MINSERT:
		if (control->changetext != NULL) guiFreeMem(control->changetext);
		stringArray = SplitMinsertData(data, datalen, &n2);
		if (stringArray == NULL) return RC_NO_MEM;
		for (n3 = 0; n3 < n2; n3++) {
			control->changetext = stringArray[n3];
			n1 = guiaChangeControl(res, control, GUI_INSERT);
			if (n1 < 0) return RC_ERROR;
		}
		FreeMinsertArray(stringArray, n2);
		control->changetext = NULL;
		return 0;

	case GUI_ERASE:
	case GUI_FOCUS:
		break;
	case GUI_INSERTLINEBEFORE:
	case GUI_INSERTLINEAFTER:
		if (!(control->style & CTRLSTYLE_INSERTORDER)) return RC_ERROR;
		if (!datalen) {
			control->value3 = 0;
			break;
		}
		/* fall through */
	case GUI_SELECTLINE:
	case GUI_DELETELINE:
	case GUI_LOCATELINE:
		numtoi(data, datalen, &n1);
		control->value3 = n1;
		break;
	case GUI_TEXTCOLOR:
		if (n1 = setControlTextColor(control, data, datalen)) return n1;
		break;
	case GUI_TEXTCOLORDATA:
		if (i1 = changeTextColorData(data, datalen, control) != 0) return i1;
		break;
	case GUI_TEXTCOLORLINE:
		if (i1 = changeTextColorLine(data, datalen, control) != 0) return i1;
		break;
	case GUI_TEXTBGCOLORLINE:
		if (control->type != TOOLBAR_DROPBOX) return RC_ERROR;
		if (i1 = changeListOrDropboxBGColorLine(data, datalen, control) != 0) return i1;
		break;
	case GUI_TEXTBGCOLORDATA:
		if (control->type != TOOLBAR_DROPBOX) return RC_ERROR;
		if (i1 = changeListOrDropboxBGColorData(data, datalen, control) != 0) return i1;
		break;

	case GUI_TEXTSTYLEDATA:
	case GUI_TEXTSTYLELINE:
		i1 = guireschangeControlTextStyle(cmd, control, data, datalen);
		if (i1) return i1;
		break;
	default:
		return RC_ERROR;
	}
	n1 = guiaChangeToolbar(res, control, cmd);
	/* do not free changetext (it is moved to control->textval) */
	control->changetext = NULL;
	if (n1 < 0) return RC_ERROR;
	return 0;
}

/**
 * Returns a pointer to an array of pointers.
 * Each item in the array points to a null-terminated string.
 * This is malloc memory and must be freed by calling FreeMinsertArray.
 */
static UCHAR** SplitMinsertData(CHAR *data, INT datalen, INT* count)
{
	INT n1, n2;
	UCHAR **array;
	INT ALLOC_CHUNK = 50;
	INT allocated;
	INT used = -1;	// One less than actual count of strings in the array
	CHAR work[64];

	allocated = ALLOC_CHUNK;
	array = malloc(allocated * sizeof(void*));
	if (array == NULL)  {
		sprintf_s(work, ARRAYSIZE(work), "malloc failed in %s", __FUNCTION__);
		guiaErrorMsg(work, allocated * sizeof(void*));
		return NULL;
	}
	memset(array, '\0', allocated * sizeof(void*));
	for (n1 = n2 = 0; n1 < datalen; n1++) {
		if (data[n1] == '\\') {
			memmove(data + n1, data + (n1 + 1), sizeof(CHAR) * (datalen - n1 - 1));
			datalen--;
			//bug fix 20JUN2011 jpr Should not be doing this -- n1++; /* probably escaping a comma */
			continue;
		}
		else if (data[n1] == minsertSeparator || (n1 + 1) == datalen) {
			if ((n1 + 1) == datalen) n1++;
			if (used + 1 == allocated) {
				allocated += ALLOC_CHUNK;
				array = realloc(array, allocated * sizeof(void*));
				if (array == NULL)  {
					sprintf_s(work, ARRAYSIZE(work), "realloc failed in %s", __FUNCTION__);
					guiaErrorMsg(work, allocated * sizeof(void*));
					return NULL;
				}
			}
			array[++used] = guiAllocString(data + n2, n1 - n2);
			if (array[used] == NULL)  {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s", __FUNCTION__);
				guiaErrorMsg(work, n1 - n2);
				return NULL;
			}
			n2 = n1 + 1;
		}
	}
	/*
	 * Bug fix just before 16.1.3 on 13 DEC 2012  jpr
	 * Boundary conditions!
	 * If the very last fricken character seen is an escaped minsertSeparator, then we
	 * were not including the last line in the array.
	 */
	if (n1 - n2 > 0) {
		if (used + 1 == allocated) {
			allocated += 1;
			array = realloc(array, allocated * sizeof(void*));
			if (array == NULL)  {
				sprintf_s(work, ARRAYSIZE(work), "realloc failed in %s", __FUNCTION__);
				guiaErrorMsg(work, allocated * sizeof(void*));
				return NULL;
			}
		}
		array[++used] = guiAllocString(data + n2, n1 - n2);
		if (array[used] == NULL)  {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s", __FUNCTION__);
			guiaErrorMsg(work, n1 - n2);
			return NULL;
		}
	}
	*count = used + 1;
	return array;
}

static void FreeMinsertArray(UCHAR** array, INT count)
{
	INT i1;
	for (i1 = 0; i1 < count; i1++) {
		guiFreeMem(array[i1]);
	}
	free(array);
}

/**
 * Skip over any leading blanks in 'data'
 * and look for a recognized color name.
 * Return the TEXTCOLOR_? value.
 */
static INT GetColorNumberFromText(UCHAR* data) {
	UINT i1 = 0;
	if (data == NULL) return RC_INVALID_PARM;
	while (data[i1] == ' ') i1++;
	data += i1;
	if (!guimemicmp(data, "RED", 3))   {return TEXTCOLOR_RED;}
	if (!guimemicmp(data, "BLUE", 4))  {return TEXTCOLOR_BLUE;}
	if (!guimemicmp(data, "CYAN", 4))  {return TEXTCOLOR_CYAN;}
	if (!guimemicmp(data, "NONE", 4))  {return TEXTCOLOR_DEFAULT;}
	if (!guimemicmp(data, "WHITE", 5)) {return TEXTCOLOR_WHITE;}
	if (!guimemicmp(data, "BLACK", 5)) {return TEXTCOLOR_BLACK;}
	if (!guimemicmp(data, "GREEN", 5)) {return TEXTCOLOR_GREEN;}
	if (!guimemicmp(data, "YELLOW", 6)) {return TEXTCOLOR_YELLOW;}
	if (!guimemicmp(data, "MAGENTA", 7)) {return TEXTCOLOR_MAGENTA;}
	if (!guimemicmp(data, "DEFAULT", 7))  {return TEXTCOLOR_DEFAULT;}
	return RC_ERROR;
}

/**
 * Defined iff OS_WIN32GUI
 *
 * Parses <param>data</param> for one of the nine color names,
 * or looks for a number that is interpreted as an BGR value.
 * This number could be decimal, octal, or hex.
 * Sets <param>cref</param> to the color found.
 * <param>defcolor</param> will be set to TRUE if not NULL and the
 * color being returned is the system default color. False otherwise.
 *
 * Returns 0 if ok, RC_ERROR if anything goes wrong.
 *
 * Note that a COLORREF is organized as 0x00bbggrr
 * We expect the user to specify colors as BGR per the doc.
 */
static INT GetColorNumberFromTextExt(UCHAR* data, INT datalen, COLORREF* cref, BOOL *defcolor) {
	long l1;
	BYTE rr, gg, bb;
	UCHAR ldata[64];

	if (cref == NULL) return RC_ERROR;
	*cref = RGB(0, 0, 0); //In case of an error return, cref will be set to black
	if (data == NULL || datalen < 1) return RC_ERROR;
	if (datalen > 63) datalen = 63;
	int i2 = 0;
	// Squeeze out the blanks
	for (int i1 = 0; i1 < datalen; i1++) {
		if (data[i1] != ' ') ldata[i2++] = data[i1];
	}
	ldata[i2] = '\0';
	if (defcolor != NULL) *defcolor = FALSE;	/* Start by assuming that this will be false */
	if (isdigit(ldata[0])) {
		l1 = strtol(ldata, NULL, 0);
		// RGB Option!
		if (formatFor24BitColorIsRGB) {
			rr = (BYTE)((l1 & 0x00FF0000) >> 16);
			gg = (BYTE)((l1 & 0x0000FF00) >> 8);
			bb = (BYTE)(l1 & 0x000000FF);
		}
		else {
			bb = (BYTE)((l1 & 0x00FF0000) >> 16);
			gg = (BYTE)((l1 & 0x0000FF00) >> 8);
			rr = (BYTE)(l1 & 0x000000FF);
		}
		*cref = RGB(rr, gg, bb);
		return 0;
	}
	else {
		if (!guimemicmp(ldata, "RED", 3))   {*cref = GetColorefFromColorNumber(TEXTCOLOR_RED);}
		else if (!guimemicmp(ldata, "BLUE", 4))  {*cref = GetColorefFromColorNumber(TEXTCOLOR_BLUE);}
		else if (!guimemicmp(ldata, "CYAN", 4))  {*cref = GetColorefFromColorNumber(TEXTCOLOR_CYAN);}
		else if (!guimemicmp(ldata, "NONE", 4) || !guimemicmp(ldata, "DEFAULT", 7))
		{
			*cref = GetColorefFromColorNumber(TEXTCOLOR_DEFAULT);
			if (defcolor != NULL) *defcolor = TRUE;
		}
		else if (!guimemicmp(ldata, "WHITE", 5)) {*cref = GetColorefFromColorNumber(TEXTCOLOR_WHITE);}
		else if (!guimemicmp(ldata, "BLACK", 5)) {*cref = GetColorefFromColorNumber(TEXTCOLOR_BLACK);}
		else if (!guimemicmp(ldata, "GREEN", 5)) {*cref = GetColorefFromColorNumber(TEXTCOLOR_GREEN);}
		else if (!guimemicmp(ldata, "YELLOW", 6)) {*cref = GetColorefFromColorNumber(TEXTCOLOR_YELLOW);}
		else if (!guimemicmp(ldata, "MAGENTA", 7)) {*cref = GetColorefFromColorNumber(TEXTCOLOR_MAGENTA);}
		else return RC_ERROR;
	}
	return 0;
}

/**
 * Defined iff OS_WIN32GUI
 */
COLORREF GetColorefFromColorNumber(INT colornum) {
	switch (colornum) {
	case TEXTCOLOR_BLACK:  return RGB(0, 0, 0);
	case TEXTCOLOR_WHITE:  return RGB(255, 255, 255);
	case TEXTCOLOR_RED:    return RGB(255, 0, 0);
	case TEXTCOLOR_GREEN:  return RGB(0, 255, 0);
	case TEXTCOLOR_BLUE:   return RGB(0, 0, 255);
	case TEXTCOLOR_YELLOW: return RGB(255, 255, 0);
	case TEXTCOLOR_MAGENTA:return RGB(255, 0, 255);
	case TEXTCOLOR_CYAN:   return RGB(0, 255, 255);
	default: return RGB(0, 0, 0);
	}
}

/**
 * Defined iff OS_WIN32GUI
 *
 * Sets the textcolor and textfgcolor fields of the control structure based on the string value of data.
 * returns RC_INVALID_PARM if data is null.
 * If data is not recognized as a color, sets control->textcolor to TEXTCOLOR_DEFAULT
 * and returns RC_ERROR
 */
static INT setControlTextColor(CONTROL *control, UCHAR *data, INT datalen) {
	INT color;
	COLORREF cref;
	control->textcolor = TEXTCOLOR_DEFAULT;
	if (data == NULL) return RC_INVALID_PARM;
	color = GetColorNumberFromText(data);
	if (color != RC_ERROR) {
		control->textcolor = color;
		control->textfgcolor = RGB(0, 0, 0);
	}
	else {
		if (!GetColorNumberFromTextExt(data, datalen, &cref, NULL)) {
			control->textcolor = TEXTCOLOR_CUSTOM;
			control->textfgcolor = cref;
		}
		else return RC_ERROR;
	}
	return 0;
}

/**
 * Sets control->textbgcolor to the color designated by the data.
 * data can be a standard DX color name (or NONE) or an RGB value
 * specified in either decimal, octal, or hex.
 * 'none' needs special treatment because the default bg color for any
 * given control is probably not black.
 *
 * Returns zero for success or RC_INVALID_PARM if anything goes wrong.
 */
static INT setControlTextBGColor(CONTROL *control, UCHAR *data, INT datalen) {
	COLORREF cs;
	INT i1;
	BOOL defcolor;
	if (data == NULL) return RC_INVALID_PARM;
	i1 = GetColorNumberFromTextExt(data, datalen, &cs, &defcolor);
	if (i1 == RC_ERROR) return RC_INVALID_PARM;
	if (defcolor) {
		control->textbgcolor = RGB(0, 0, 0);
		control->userBGColor = FALSE;
	}
	else {
		control->textbgcolor = cs;
		control->userBGColor = TRUE;
	}
	return 0;
}

/*
 * This is called only for the TEXTFONT change command
 */
static INT setControlTextFont(CONTROL *control, UCHAR *data) {
	LOGFONT lf;
	HDC hdc;
	INT i1;

	if (data == NULL) return RC_INVALID_PARM;
	if (control->font == NULL) guiaSetCtlFont(control);
	if (!GetObject(control->font, sizeof(LOGFONT), (LPVOID) &lf)) return RC_ERROR;
	hdc = GetDC(guia_hwnd());
	i1 = parseFontString(data, &lf, hdc);
	if (i1) {
		ReleaseDC(guia_hwnd(), hdc);
		return i1;
	}
	control->font = getFont(&lf);
	ReleaseDC(guia_hwnd(), hdc);
	return 0;
}

/**
 * Processes change commands to panels and dialogs.
 *
 * Does not have to verify that reshandle is valid. That is done by the caller.
 *
 */
static INT guireschangepnldlg(RESHANDLE reshandle, INT cmd, CONTROL *control, UCHAR *data, INT datalen)
{
	RESOURCE *res;
	ZHANDLE newtitle;
	INT i1, n1, n2, n3; //, color;
	BOOL grphadfocus; //, defcolor;
	CONTROL *workcontrol, *firstcontrol;
	UCHAR resname[MAXRESNAMELEN];
	RESHANDLE iconrh;
	UCHAR **stringArray;
	TREESTRUCT *ptree;
	CHAR work[128];

	res = *reshandle;

	/**
	 * Check the commands that apply to the entire resource.
	 * These commands do not use a control.
	 */
	if (cmd == GUI_ITEMON) {
		res->itemmsgs = TRUE;
		return 0;
	}
	else if (cmd == GUI_ITEMOFF) {
		res->itemmsgs = FALSE;
		return 0;
	}
	else if (cmd == GUI_FOCUSON) {
		res->focusmsgs = TRUE;
		return 0;
	}
	else if (cmd == GUI_FOCUSOFF) {
		res->focusmsgs = FALSE;
		return 0;
	}
	else if (cmd == GUI_LINEON) {
		res->linemsgs = TRUE;
		return 0;
	}
	else if (cmd == GUI_LINEOFF) {
		res->linemsgs = FALSE;
		return 0;
	}
	else if (cmd == GUI_RIGHTCLICKON) {
		res->rclickmsgs = TRUE;
		return 0;
	}
	else if (cmd == GUI_RIGHTCLICKOFF) {
		res->rclickmsgs = FALSE;
		return 0;
	}
	else if (cmd == GUI_TITLE) {
		if (res->restype == RES_TYPE_DIALOG) {
			if (res->title != NULL) guiFreeMem(res->title);
			if (data == NULL || datalen < 1) return RC_INVALID_PARM;
			res->title = guiAllocString(data, datalen);
			if (res->title == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at GUI_TITLE", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
			if (res->winshow != NULL) return guiaChangeDlgTitle(res);
		}
		return 0; /* ignore a title command sent to a panel */
	}

	/**
	 * If we get here then we will be needing a control.
	 */
	if (control == NULL) return RC_INVALID_PARM;

	switch (cmd) {
	case GUI_TEXTBGCOLOR:
		switch(control->type) {
		case PANEL_POPBOX:
			if (i1 = setControlTextBGColor(control, data, datalen)) return i1;
			goto guireschangepnldlgend;
		default:
			return RC_INVALID_PARM;
		}
	case GUI_TEXTCOLOR:
		switch(control->type) {
		case PANEL_CVTEXT:
		case PANEL_CHECKBOX:
		case PANEL_LTCHECKBOX:
		case PANEL_DROPBOX:
		case PANEL_EDIT:
		case PANEL_FEDIT:
		case PANEL_LEDIT:
		case PANEL_LISTBOX:
		case PANEL_LISTBOXHS:
		case PANEL_MEDIT:
		case PANEL_MEDITHS:
		case PANEL_MEDITVS:
		case PANEL_MEDITS:
		case PANEL_MLEDIT:
		case PANEL_MLEDITHS:
		case PANEL_MLEDITVS:
		case PANEL_MLEDITS:
		case PANEL_MLISTBOX:
		case PANEL_MLISTBOXHS:
		case PANEL_PEDIT:
		case PANEL_PLEDIT:
		case PANEL_RVTEXT:
		case PANEL_TREE:
		case PANEL_VTEXT:
		case PANEL_POPBOX:
		case PANEL_TAB:
			if (i1 = setControlTextColor(control, data, datalen)) return i1;
			goto guireschangepnldlgend;
		case PANEL_TABLE:
			if (i1 = guiResChangeTable(reshandle, cmd, control, data, datalen)) {
				return i1;
			}
			goto guireschangepnldlgend;
		default:
			return RC_INVALID_PARM;
		}

	/*
	 * The TEXTFONT change command is only valid for Vtext, CVtext, and RVtext
	 */
	case GUI_TEXTFONT:
		switch(control->type) {
		case PANEL_CVTEXT:
		case PANEL_RVTEXT:
		case PANEL_VTEXT:
			if (i1 = setControlTextFont(control, data)) return i1;
			goto guireschangepnldlgend;
		default:
			return RC_INVALID_PARM;
		}
	default:
		break;
	}

	switch (control->type) {
	case PANEL_TAB:
		switch(cmd) {
		case GUI_HELPTEXT:
			if (data == NULL) return RC_INVALID_PARM;
			if (control->tooltip != NULL) guiFreeMem(control->tooltip);
			control->tooltip = guiAllocString(data, datalen);
			if (control->tooltip == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at TAB/GUI_HELPTEXT", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
			break;
		case GUI_LOCATETAB:
			break;
		case GUI_FOCUS:
			control->shownflag = TRUE;
			break;

		case GUI_TEXT:
			if (data == NULL) return RC_INVALID_PARM;
			newtitle = guiAllocString(data, datalen);
			if (newtitle == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at TAB/GUI_TEXT", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
			if (control->textval != NULL) guiFreeMem(control->textval);
			control->textval = newtitle;
			break;

		default:
			return RC_ERROR;
			break;
		}
		break;
	case PANEL_FEDIT:
		if (GUI_FEDITMASK == cmd || GUI_TEXT == cmd) {
			if (data == NULL) return RC_INVALID_PARM;
			guiLockMem(res->content);
			control->changetext = guiAllocString(data, datalen);
			if (control->changetext == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString returned null in %s at PANEL_FEDIT", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				guiUnlockMem(res->content);
				return RC_NO_MEM;
			}
			if (GUI_FEDITMASK == cmd) {
				if (control->textval == NULL) {
					control->textval = guiAllocMem(datalen + 1);
					if (control->textval == NULL) {
						sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s at PANEL_FEDIT", __FUNCTION__);
						guiaErrorMsg(work, datalen + 1);
						guiUnlockMem(res->content);
						return RC_NO_MEM;
					}
				}
				else {
					control->textval = guiReallocMem(control->textval, datalen + 1);
					if (control->textval == NULL) {
						sprintf_s(work, ARRAYSIZE(work), "guiReallocMem returned null in %s at PANEL_FEDIT", __FUNCTION__);
						guiaErrorMsg(work, datalen + 1);
						guiUnlockMem(res->content);
						return RC_NO_MEM;
					}
				}
			}
			else if (GUI_TEXT == cmd) {
				if (control->textval == NULL) {
					control->textval = guiAllocMem((INT) strlen((CHAR *) guiMemToPtr(control->mask)) + 1);
					if (control->textval == NULL) {
						sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s at PANEL_FEDIT", __FUNCTION__);
						guiaErrorMsg(work, (INT) strlen((CHAR *) guiMemToPtr(control->mask)) + 1);
						guiUnlockMem(res->content);
						return RC_NO_MEM;
					}
				}
			}
			guiUnlockMem(res->content);
			//if (control->changetext == NULL || control->textval == NULL) return RC_NO_MEM;
			break;
		}
		/* fall through */
	case PANEL_VTEXT:
	case PANEL_RVTEXT:
	case PANEL_CVTEXT:
	case PANEL_EDIT:
	case PANEL_MEDIT:
	case PANEL_MEDITHS:
	case PANEL_MEDITVS:
	case PANEL_MEDITS:
	case PANEL_PEDIT:
	case PANEL_LEDIT:
	case PANEL_MLEDIT:
	case PANEL_MLEDITHS:
	case PANEL_MLEDITVS:
	case PANEL_MLEDITS:
	case PANEL_PLEDIT:
		newtitle = NULL;
		switch(cmd) {

		/**
		 * Note: We attempt to assign helptext to WC_STATIC controls, but it does not work.
		 * I think that is because Windows does not report mouse events from such a control.
		 */
		case GUI_HELPTEXT:
			if (data == NULL) return RC_INVALID_PARM ;
			if (control->tooltip != NULL) guiFreeMem(control->tooltip);
			control->tooltip = guiAllocString(data, datalen);
			if (control->tooltip == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at EDIT/GUI_HELPTEXT", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
			break;
		case GUI_PASTE:
			if (control->type != PANEL_EDIT && control->type != PANEL_FEDIT &&
					control->type != PANEL_MEDIT && control->type != PANEL_MEDITHS &&
					control->type != PANEL_MEDITVS && control->type != PANEL_MEDITS &&
					control->type != PANEL_PEDIT &&	control->type != PANEL_LEDIT &&
					control->type != PANEL_MLEDIT && control->type != PANEL_PLEDIT &&
					control->type != PANEL_MLEDITHS && control->type != PANEL_MLEDITVS &&
					control->type != PANEL_MLEDITS) {
				return RC_ERROR;
			}
			/* Treat a NULL data and a zero-length data the same */
			/* With a PASTE command, 'no data' results in erasing the selected text */
			if (data == NULL) control->editbuffer = (UCHAR *) guiAllocString("", 0); /*return RC_INVALID_PARM;*/
			else control->editbuffer = (UCHAR *) guiAllocString(data, datalen);
			if (control->editbuffer == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at EDIT/GUI_PASTE", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
			break;
		case GUI_SETMAXCHARS:
			if (control->type != PANEL_MLEDIT && control->type != PANEL_LEDIT &&
					control->type != PANEL_PLEDIT && control->type != PANEL_MLEDITHS &&
					control->type != PANEL_MLEDITVS && control->type != PANEL_MLEDITS) {
				return RC_ERROR;
			}
			if (data == NULL) return RC_INVALID_PARM;
			numtoi(data, datalen, &i1);
			control->maxtextchars = i1;
			break;
		case GUI_TEXT:
			if (data == NULL) return RC_INVALID_PARM;
			newtitle = guiAllocString(data, datalen);
			if (newtitle == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at EDIT/GUI_TEXT", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
			/* fall through */
		case GUI_ERASE:
			if (control->textval != NULL) guiFreeMem(control->textval);
			control->textval = newtitle;
			break;
		case GUI_JUSTIFYLEFT:
			control->style &= ~(CTRLSTYLE_RIGHTJUSTIFIED | CTRLSTYLE_CENTERJUSTIFIED);
			control->style |= CTRLSTYLE_LEFTJUSTIFIED;
			break;
		case GUI_JUSTIFYRIGHT:
			control->style &= ~(CTRLSTYLE_LEFTJUSTIFIED | CTRLSTYLE_CENTERJUSTIFIED);
			control->style |= CTRLSTYLE_RIGHTJUSTIFIED;
			break;
		case GUI_JUSTIFYCENTER:
			control->style &= ~(CTRLSTYLE_LEFTJUSTIFIED | CTRLSTYLE_RIGHTJUSTIFIED);
			control->style |= CTRLSTYLE_CENTERJUSTIFIED;
			break;
		case GUI_AVAILABLE:
			control->style &= ~(CTRLSTYLE_DISABLED | CTRLSTYLE_SHOWONLY | CTRLSTYLE_READONLY);
			break;
		case GUI_GRAY:
			control->style &= ~(CTRLSTYLE_SHOWONLY | CTRLSTYLE_READONLY);
			control->style |= CTRLSTYLE_DISABLED;
			break;
		case GUI_SHOWONLY:
			control->style &= ~(CTRLSTYLE_DISABLED | CTRLSTYLE_READONLY);
			control->style |= CTRLSTYLE_SHOWONLY;
			break;
		case GUI_READONLY:
			control->style &= ~(CTRLSTYLE_DISABLED | CTRLSTYLE_SHOWONLY);
			control->style |= CTRLSTYLE_READONLY;
			break;
		case GUI_FOCUS:
			 break;
		default:
			return RC_ERROR;
		}
		break;
	case PANEL_RADIOBUTTON:
	case PANEL_LASTRADIOBUTTON:
		if (cmd == GUI_MARK) {
			for (workcontrol = control; workcontrol->value1 != 1; workcontrol--);
			grphadfocus = FALSE;
			for (;;){
				if (workcontrol != control) {
					workcontrol->style &= ~CTRLSTYLE_MARKED;
					if (guiaChangeControl(res, workcontrol, GUI_UNMARK)) return RC_ERROR;
					if (guiaGetFocusControl(res) == workcontrol) grphadfocus = TRUE;
				}
				if (ISLASTRADIO(workcontrol)) break;
				workcontrol++;
			}
			/*If the focus was in this radio group, then we must
			  set the focus to this newly marked button*/
			if (grphadfocus){
				guiaChangeControl(res, control, GUI_FOCUS);
			}
		}
		/* fall through */
	case PANEL_CHECKBOX:
	case PANEL_LTCHECKBOX:
	case PANEL_BUTTON:
	case PANEL_LOCKBUTTON:
	case PANEL_ICONLOCKBUTTON:
		switch(cmd) {
		case GUI_HELPTEXT:
			if (data == NULL) return RC_INVALID_PARM;
			if (control->tooltip != NULL) guiFreeMem(control->tooltip);
			control->tooltip = guiAllocString(data, datalen);
			if (control->tooltip == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at CHECKBOX/GUI_HELPTEXT", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
			break;
		case GUI_ICON:
			if (control->type == PANEL_ICONLOCKBUTTON) {
				if (datalen <= 0 || datalen > MAXRESNAMELEN) return RC_INVALID_VALUE;
				memset(resname, ' ', MAXRESNAMELEN);
				memcpy(resname, data, datalen);
				if ((iconrh = locateIconByName(resname)) == NULL) return RC_ERROR;
				if ((*iconrh)->hsize != (*control->iconres)->hsize || (*iconrh)->vsize != (*control->iconres)->vsize)
					return RC_INVALID_VALUE;
				else control->iconres = iconrh;
				break;
			}
			else return RC_ERROR;
		case GUI_TEXT:
			if ((control->type == PANEL_CHECKBOX ||
					control->type == PANEL_LTCHECKBOX ||
					control->type == PANEL_BUTTON ||
					control->type == PANEL_RADIOBUTTON ||
					control->type == PANEL_LASTRADIOBUTTON) && datalen == 0) {
				control->textval = guiAllocString((UCHAR *) " ", 1);
			}
			else {
				if (data == NULL) return RC_INVALID_PARM;
				control->textval = guiAllocString(data, datalen);
			}
			if (control->textval == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at CHECKBOX/GUI_TEXT", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return RC_NO_MEM;
			}
			break;
		case GUI_MARK:
			control->style |= CTRLSTYLE_MARKED;
			break;
		case GUI_UNMARK:
			control->style &= ~CTRLSTYLE_MARKED;
			break;
		case GUI_GRAY:
			control->style &= ~CTRLSTYLE_SHOWONLY;
			control->style |= CTRLSTYLE_DISABLED;
			break;
		case GUI_SHOWONLY:
			control->style &= ~CTRLSTYLE_DISABLED;
			control->style |= CTRLSTYLE_SHOWONLY;
			break;
		case GUI_AVAILABLE:
			control->style &= ~(CTRLSTYLE_DISABLED | CTRLSTYLE_SHOWONLY);
			break;
		case GUI_FOCUS:
			break;
		default:
			return RC_ERROR;
		}
		break;
	case PANEL_VICON:
		switch (cmd) {
		case GUI_ICON:
			if (datalen <= 0 || datalen > MAXRESNAMELEN) return RC_INVALID_VALUE;
			memset(resname, ' ', MAXRESNAMELEN);
			memcpy(resname, data, datalen);
			if ((iconrh = locateIconByName(resname))== NULL) return RC_ERROR;
			control->iconres = iconrh;
			break;
		case GUI_GRAY:
		case GUI_AVAILABLE:
			return 0;		// Avoid 794 error
		default:
			return RC_ERROR;
		}
		break;

	case PANEL_ICON:
		switch (cmd) {
		case GUI_GRAY:
		case GUI_AVAILABLE:
			return 0;		// Avoid 794 error
		}

	case PANEL_POPBOX:
		switch(cmd) {
		case GUI_TEXT:
			if (data == NULL) return RC_INVALID_PARM;
			control->textval = guiAllocString(data, datalen);
			if (control->textval == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at POPBOX/GUI_TEXT", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
			break;
		case GUI_GRAY:
			control->style &= ~CTRLSTYLE_SHOWONLY;
			control->style |= CTRLSTYLE_DISABLED;
			break;
		case GUI_AVAILABLE:
			control->style &= ~(CTRLSTYLE_DISABLED | CTRLSTYLE_SHOWONLY);
			break;
		case GUI_FOCUS:
			break;
		case GUI_ERASE:
			if (control->textval != NULL) guiFreeMem(control->textval);
			control->textval = NULL;
			break;
		case GUI_HELPTEXT:
			if (data == NULL) return RC_INVALID_PARM;
			if (control->tooltip != NULL) guiFreeMem(control->tooltip);
			control->tooltip = guiAllocString(data, datalen);
			if (control->tooltip == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at POPBOX/GUI_HELPTEXT", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
			break;
		default:
			return RC_ERROR;
		}
		break;
	case PANEL_PUSHBUTTON:
	case PANEL_DEFPUSHBUTTON:
	case PANEL_ICONPUSHBUTTON:
	case PANEL_ICONDEFPUSHBUTTON:
		firstcontrol = (CONTROL *) guiMemToPtr(res->content);
		switch(cmd) {
		case GUI_HELPTEXT:
			if (data == NULL) return RC_INVALID_PARM;
			if (control->tooltip != NULL) guiFreeMem(control->tooltip);
			control->tooltip = guiAllocString(data, datalen);
			if (control->tooltip == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at PUSHBUTTON/GUI_HELPTEXT", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
			break;
		case GUI_ICON:
			if (control->type == PANEL_ICONDEFPUSHBUTTON || control->type == PANEL_ICONPUSHBUTTON) {
				if (datalen <= 0 || datalen > MAXRESNAMELEN) return RC_INVALID_VALUE;
				memset(resname, ' ', MAXRESNAMELEN);
				memcpy(resname, data, datalen);
				if ((iconrh = locateIconByName(resname)) == NULL) return RC_ERROR;
				if ((*iconrh)->hsize != (*control->iconres)->hsize || (*iconrh)->vsize != (*control->iconres)->vsize)
					return RC_INVALID_VALUE;
				else control->iconres = iconrh;
				break;
			}
			else return RC_ERROR;
		case GUI_TEXT:
			if (data == NULL) return RC_INVALID_PARM;
			control->textval = guiAllocString(data, datalen);
			if (control->textval == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at PUSHBUTTON/GUI_TEXT", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
			break;
		case GUI_DEFPUSHBUTTON:
			res->ctlorgdflt = (INT) (control - firstcontrol + 1);
			break;
		case GUI_ESCPUSHBUTTON:
			if (!control->escflag) {
				for (workcontrol = firstcontrol, i1 = 0; i1 < res->entrycount; workcontrol++, i1++) {
					workcontrol->escflag = FALSE;
				}
				res->ctlescape = (INT) (control - firstcontrol + 1);
				control->escflag = TRUE;
			}
			break;
		case GUI_PUSHBUTTON:
			if (control->escflag) {
				control->escflag = FALSE;
				if (res->winshow != NULL && res->ctlescape == control - firstcontrol + 1) {
					res->ctlescape = 0;
				}
			}
			if (res->ctlorgdflt == (INT) (control - firstcontrol + 1)) res->ctlorgdflt = 0;
			break;
		case GUI_GRAY:
			control->style &= ~(CTRLSTYLE_SHOWONLY /*| CTRLSTYLE_NOFOCUS*/);
			control->style |= CTRLSTYLE_DISABLED;
			break;
		case GUI_SHOWONLY:
			control->style &= ~(CTRLSTYLE_DISABLED /*| CTRLSTYLE_NOFOCUS*/);
			control->style |= CTRLSTYLE_SHOWONLY;
			break;

		/*
		 * As of DX 15 this is not documented.
		 * Fix that in 16
		 */
		case GUI_NOFOCUS:
			control->style &= ~(CTRLSTYLE_DISABLED | CTRLSTYLE_SHOWONLY);
			control->style |= CTRLSTYLE_NOFOCUS;
			break;

		case GUI_AVAILABLE:
			control->style &= ~(CTRLSTYLE_DISABLED | CTRLSTYLE_SHOWONLY /*| CTRLSTYLE_NOFOCUS*/);
			break;
		case GUI_FOCUS:
			break;
		default:
			return RC_ERROR;
		}
		break;
	case PANEL_HSCROLLBAR:
	case PANEL_VSCROLLBAR:
	case PANEL_PROGRESSBAR:
		switch(cmd) {
		case GUI_HELPTEXT:
			if (data == NULL) return RC_INVALID_PARM;
			if (control->tooltip != NULL) guiFreeMem(control->tooltip);
			control->tooltip = guiAllocString(data, datalen);
			if (control->tooltip == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at SCROLLBAR/GUI_HELPTEXT", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
			break;
		case GUI_GRAY:
			control->style |= CTRLSTYLE_DISABLED;
			break;
		case GUI_AVAILABLE:
			control->style &= ~(CTRLSTYLE_DISABLED | CTRLSTYLE_SHOWONLY);
			break;
		case GUI_SHOWONLY:
			if (control->type != PANEL_PROGRESSBAR) {
				control->style &= ~(CTRLSTYLE_DISABLED /*| CTRLSTYLE_NOFOCUS*/);
				control->style |= CTRLSTYLE_SHOWONLY;
			}
			else return RC_ERROR;
			break;
		case GUI_FOCUS:
			break;
		case GUI_POSITION:
			if (datalen != 5 || num5toi(data, &n1) || n1 < control->value1 || n1 > control->value2) return RC_ERROR;
			control->value = n1;
			break;
		case GUI_RANGE:
			if (control->type == PANEL_PROGRESSBAR) {
				if (datalen < 10 || num5toi(data, &n1) || num5toi(&data[5], &n2) || n1 >= n2) return RC_ERROR;
				control->value1 = (USHORT) n1;
				control->value2 = (USHORT) n2;
			}
			else {
				if (datalen < 15 || num5toi(data, &n1) || num5toi(&data[5], &n2) || num5toi(&data[10], &n3)
					|| n1 >= n2 || n3 > n2 - n1) return RC_ERROR;
				control->value1 = (USHORT) n1;
				control->value2 = (USHORT) n2;
				control->value3 = (USHORT) n3;
			}
			/*
			 * If the current position of the scroll/progress bar is outside the new range,
			 * move it just inside.
			 */
			if (control->value < control->value1 || control->value2 < control->value) {
				if (control->value < control->value1) control->value = control->value1;
				else if (control->value2 < control->value) control->value = control->value2;
			}
			break;
		default:
			return RC_ERROR;
		}
		break;

	case PANEL_TREE:
		switch (cmd) {
		case GUI_HELPTEXT:
			if (data == NULL) return RC_INVALID_PARM;
			if (control->tooltip != NULL) guiFreeMem(control->tooltip);
			control->tooltip = guiAllocString(data, datalen);
			if (control->tooltip == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at TREE/GUI_HELPTEXT", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
			break;
		case GUI_ICON:
			if (datalen <= 0 || datalen > MAXRESNAMELEN) return RC_INVALID_VALUE;
			if (control->tree.cpos == NULL) return RC_ERROR;
			memset(resname, ' ', MAXRESNAMELEN);
			memcpy(resname, data, datalen);
			if ((iconrh = locateIconByName(resname)) == NULL) return RC_ERROR;
			control->iconres = iconrh;
			break;
		case GUI_INSERT:
		case GUI_INSERTBEFORE:
		case GUI_INSERTAFTER:
			if (data == NULL) return RC_INVALID_PARM;
			if (control->changetext != NULL) guiFreeMem(control->changetext);
			control->changetext = guiAllocString(data, datalen);
			if (control->changetext == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at TREE/GUI_INSERT", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
			break;

		case GUI_SETLINE:
		case GUI_EXPAND:
		case GUI_COLLAPSE:
			if (data == NULL) return RC_INVALID_PARM;
			numtoi(data, datalen, &n1);
			control->value3 = n1;
			if (control->value3 <= 0) return RC_ERROR;
			if (control->tree.level == 0) ptree = control->tree.root;
			else ptree = control->tree.parent->firstchild;
			i1 = 0;
			while (ptree != NULL && ++i1) ptree = ptree->nextchild;
			if (control->value3 > i1) return RC_ERROR;
			if (i1 == 0) break;
			if (cmd == GUI_SETLINE) control->tree.line = control->value3;
			if (control->tree.level == 0) ptree = control->tree.root;
			else ptree = control->tree.parent->firstchild;
			i1 = 1;
			while (control->value3 > i1) {
				ptree = ptree->nextchild;
				i1++;
			}
			if (cmd == GUI_SETLINE) {
				control->tree.cpos = ptree;
				return 0;					// No need to call into guiawin
			}
			control->tree.xpos = ptree;
			break;

		case GUI_ROOTLEVEL:
			if (control->tree.level > TREE_POS_ROOT) {
				control->tree.level = TREE_POS_ROOT;
				control->tree.line = 1;
				control->tree.parent = NULL;
				control->tree.cpos = control->tree.root;
			}
			return 0;

		case GUI_UPLEVEL:
			if (control->tree.line != TREE_POS_INSERT && control->tree.line >= 1) {
				control->tree.level++;
				control->tree.parent = control->tree.cpos;
				if (control->tree.cpos->firstchild != NULL) {
					control->tree.line = 1;
					control->tree.cpos = control->tree.cpos->firstchild;
				}
				else {
					control->tree.line = TREE_POS_INSERT;
				}
			}
			break;
		case GUI_DOWNLEVEL:
			if (control->tree.level != TREE_POS_ROOT && control->tree.level >= 1) {
				control->tree.level--;
				/* modify control->tree.line */
				if (control->tree.line == TREE_POS_INSERT) {
					if (control->tree.level == TREE_POS_ROOT) ptree = control->tree.root;
					else ptree = control->tree.cpos->parent->firstchild;
					control->tree.line = 1;
					while (ptree != control->tree.cpos) {
						ptree = ptree->nextchild;
						control->tree.line++;
					}
				}
				else {
					if (control->tree.parent->parent == NULL) ptree = control->tree.root;
					else ptree = control->tree.parent->parent->firstchild;
					control->tree.line = 1;
					while (ptree != control->tree.parent) {
						ptree = ptree->nextchild;
						control->tree.line++;
					}
				}
				if (control->tree.line != TREE_POS_INSERT) {
					control->tree.cpos = ptree;
					control->tree.parent = control->tree.cpos->parent;
				}
			}
			break;
		case GUI_SELECT:
			if (control->tree.cpos == NULL) return RC_ERROR;
			control->tree.spos = control->tree.cpos;
			break;
		case GUI_DESELECT:
			control->tree.spos = NULL;
			break;
		case GUI_ERASE:
		case GUI_DELETE:
		case GUI_FOCUS:
			break;
		case GUI_SETSELECTEDLINE:
			if (control->tree.spos != NULL) {
				control->tree.cpos = control->tree.spos;
				/* Set control->tree.level */
				control->tree.level = TREE_POS_ROOT;
				ptree = control->tree.parent = control->tree.spos->parent;
				while (ptree != NULL) {
					control->tree.level++;
					ptree = ptree->parent;
				}
				/* Set control->tree.line */
				if (control->tree.level == TREE_POS_ROOT) ptree = control->tree.root;
				else ptree = control->tree.spos->parent->firstchild;
				control->tree.line = 1;
				while (ptree != control->tree.cpos) {
					ptree = ptree->nextchild;
					control->tree.line++;
				}
				/* Special case, we just return from here because we do not want to call guiaChangeControl */
				return 0;
			}
			break;
		case GUI_AVAILABLE:
			control->style &= ~(CTRLSTYLE_DISABLED | CTRLSTYLE_SHOWONLY | CTRLSTYLE_READONLY);
			break;
		case GUI_GRAY:
			control->style &= ~(CTRLSTYLE_SHOWONLY | CTRLSTYLE_READONLY);
			control->style |= CTRLSTYLE_DISABLED;
			break;
		case GUI_READONLY:
			control->style &= ~(CTRLSTYLE_DISABLED | CTRLSTYLE_SHOWONLY);
			control->style |= CTRLSTYLE_READONLY;
			break;
		case GUI_TEXTCOLORDATA:
			if (i1 = changeTextColorData(data, datalen, control) != 0) return i1;
			break;
		case GUI_TEXTCOLORLINE:
			if (i1 = changeTextColorLine(data, datalen, control) != 0) return i1;
			break;

		case GUI_TEXTSTYLEDATA:
		case GUI_TEXTSTYLELINE:
			i1 = guireschangeControlTextStyle(cmd, control, data, datalen);
			if (i1) return i1;
			break;

		default:
			return RC_ERROR;
		}
		break;			// End of PANEL_TREE

	case PANEL_LISTBOX:
	case PANEL_LISTBOXHS:
	case PANEL_MLISTBOX:
	case PANEL_MLISTBOXHS:
	case PANEL_DROPBOX:
		switch (cmd) {
		case GUI_HELPTEXT:
			if (data == NULL) return RC_INVALID_PARM;
			if (control->tooltip != NULL) guiFreeMem(control->tooltip);
			control->tooltip = guiAllocString(data, datalen);
			if (control->tooltip == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at LISTBOX/GUI_HELPTEXT", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
			break;
		case GUI_GRAY:
			control->style &= ~CTRLSTYLE_SHOWONLY;
			control->style |= CTRLSTYLE_DISABLED;
			break;
		case GUI_SHOWONLY:
			control->style &= ~CTRLSTYLE_DISABLED;
			control->style |= CTRLSTYLE_SHOWONLY;
			break;
		case GUI_AVAILABLE:
			control->style &= ~(CTRLSTYLE_DISABLED | CTRLSTYLE_SHOWONLY);
			break;
		case GUI_SELECT:
		case GUI_DESELECT:
			if (control->type != PANEL_MLISTBOX && control->type != PANEL_MLISTBOXHS) return 0;
			/* fall through */
		case GUI_INSERT:
		case GUI_LOCATE:
		case GUI_DELETE:
			if (datalen < 1) {
				if (cmd == GUI_INSERT) return 0;
				else return RC_INVALID_PARM;
			}
			if (control->changetext != NULL) guiFreeMem(control->changetext);
			control->changetext = guiAllocString(data, datalen);
			if (control->changetext == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at LISTBOX/GUI_INSERT", __FUNCTION__);
				guiaErrorMsg(work, datalen);
				return RC_NO_MEM;
			}
			break;
		case GUI_MINSERT:
			if (data == NULL) return RC_INVALID_PARM;
			if (control->changetext != NULL) guiFreeMem(control->changetext);
			stringArray = SplitMinsertData(data, datalen, &n2);
			if (stringArray == NULL) return RC_NO_MEM;
			for (n3 = 0; n3 < n2; n3++) {
				control->changetext = stringArray[n3];
				n1 = guiaChangeControl(res, control, GUI_INSERT);
				if (n1 < 0) return RC_ERROR;
			}
			FreeMinsertArray(stringArray, n2);
			control->changetext = NULL;
			return 0;

		case GUI_INSERTBEFORE:
			/* If BOXALPHA then error */
			if (!(control->style & CTRLSTYLE_INSERTORDER)) return RC_ERROR;
			control->insstyle = LIST_INSERTBEFORE;
			if (control->changetext != NULL) guiFreeMem(control->changetext);
			if (!datalen) { /* set to start of list */
				control->inspos = 1;
				control->changetext = NULL;
			}
			else {
				control->changetext = guiAllocString(data, datalen);
				if (control->changetext == NULL) {
					sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at LISTBOX/GUI_INSERTBEFORE", __FUNCTION__);
					guiaErrorMsg(work, datalen);
					return RC_NO_MEM;
				}
			}
			break;
		case GUI_INSERTAFTER:
			/* If BOXALPHA then error */
			if (!(control->style & CTRLSTYLE_INSERTORDER)) return RC_ERROR;
			control->insstyle = LIST_INSERTAFTER;
			if (control->changetext != NULL) guiFreeMem(control->changetext);
			if (!datalen) { /* set to end of list */
				control->inspos = control->value1 + 1;
				control->changetext = NULL;
				control->insstyle = LIST_INSERTNORMAL;
			}
			else {
				control->changetext = guiAllocString(data, datalen);
				if (control->changetext == NULL) {
					sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at LISTBOX/GUI_INSERTAFTER", __FUNCTION__);
					guiaErrorMsg(work, datalen);
					return RC_NO_MEM;
				}
			}
			break;
		case GUI_SELECTALL:
		case GUI_DESELECTALL:
		case GUI_ERASE:
		case GUI_FOCUS:
			break;
		case GUI_INSERTLINEBEFORE:
		case GUI_INSERTLINEAFTER:
			if (!(control->style & CTRLSTYLE_INSERTORDER)) return RC_ERROR;
			if (!datalen) {
				control->value3 = 0;
				break;
			}
			/* no break */
		case GUI_SELECTLINE:
		case GUI_DESELECTLINE:
		case GUI_DELETELINE:
		case GUI_LOCATELINE:
			if (data == NULL) return RC_INVALID_PARM;
			numtoi(data, datalen, &n1);
			control->value3 = n1;
			break;

		case GUI_TEXTCOLORDATA:
			if (i1 = changeTextColorData(data, datalen, control) != 0) return i1;
			break;
		case GUI_TEXTCOLORLINE:
			if (i1 = changeTextColorLine(data, datalen, control) != 0) return i1;
			break;
		case GUI_TEXTBGCOLORDATA:
			if (i1 = changeListOrDropboxBGColorData(data, datalen, control) != 0) return i1;
			break;
		case GUI_TEXTBGCOLORLINE:
			if (i1 = changeListOrDropboxBGColorLine(data, datalen, control) != 0) return i1;
			break;

		case GUI_TEXTSTYLEDATA:
		case GUI_TEXTSTYLELINE:
			i1 = guireschangeControlTextStyle(cmd, control, data, datalen);
			if (i1) return i1;
			break;
		default:
			return RC_ERROR;
		}		// end of PANEL_ [LISTBOXxx | MLISTBOXxx | DROPBOX]
		break;

	case PANEL_TABLE:
		if (i1 = guiResChangeTable(reshandle, cmd, control, data, datalen)) {
			return i1;
		}
		break;

	default:
		return RC_ERROR;

	} /* switch (control->type) */

guireschangepnldlgend:
	n1 = guiaChangeControl(res, control, cmd);
	if (control->changetext != NULL) {
		guiFreeMem(control->changetext);
		control->changetext = NULL;
	}
	if (n1 < 0) {
		return RC_ERROR;
	}
	return 0;
}

/**
 * Examines data looking for PLAIN, BOLD, ITALIC, BOLDITALIC. Sets control->changestyle bits.
 * For cmd == TEXTSTYLEDATA, sets control->changetext
 * For cmd == TEXTSTYLELINE, sets control->value3
 */
static INT guireschangeControlTextStyle(INT cmd, CONTROL *control, UCHAR *data, INT datalen) {
	INT n1;
	CHAR work[128];
	if (data == NULL) return RC_INVALID_PARM;
	if (!guimemicmp(data, "PLAIN ", n1 = 6)) control->changestyle = LISTSTYLE_PLAIN;
	else if (!guimemicmp(data, "BOLD ", n1 = 5)) control->changestyle = LISTSTYLE_BOLD;
	else if (!guimemicmp(data, "ITALIC ", n1 = 7)) control->changestyle = LISTSTYLE_ITALIC;
	else if (!guimemicmp(data, "BOLDITALIC ", n1 = 11))
			control->changestyle = LISTSTYLE_ITALIC | LISTSTYLE_BOLD;
	else return RC_ERROR;
	if (cmd == GUI_TEXTSTYLEDATA) {
		if (control->changetext != NULL) guiFreeMem(control->changetext);
		control->changetext = guiAllocString(data + n1, datalen - n1);
		if (control->changetext == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s at GUI_TEXTSTYLExxx", __FUNCTION__);
			guiaErrorMsg(work, datalen - n1);
			return RC_NO_MEM;
		}
	}
	else {
		numtoi(data + n1, datalen - n1, &n1);
		control->value3 = n1;
	}
	return 0;
}

/**
 * The pvi calls are in here because a realloc can move memory. (heap!)
 * And we do not want the Windows thread touching that field while moving.
 */
static INT guiResChangeTableGrowRows(CONTROL *control) {
	CHAR work[64];
	if (control->table.rows == NULL) {
		control->table.numrowsalloc = TABLEROW_GROW_SIZE;
		control->table.rows = guiAllocMem(TABLEROW_GROW_SIZE * sizeof(ZHANDLE));
		if (control->table.rows == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, TABLEROW_GROW_SIZE * sizeof(ZHANDLE));
			return RC_NO_MEM;
		}
	}
	else if (control->table.numrows > control->table.numrowsalloc) {
		pvistart();
		control->table.numrowsalloc += TABLEROW_GROW_SIZE;
		control->table.rows = guiReallocMem(control->table.rows, control->table.numrowsalloc * sizeof(ZHANDLE));
		pviend();
		if (control->table.rows == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiReallocMem returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, control->table.numrowsalloc * sizeof(ZHANDLE));
			return RC_NO_MEM;
		}
	}
	return 0;
}

static void guiresfreetablerow(CONTROL *control, UINT row, PTABLECOLUMN tblColumns) {
	ZHANDLE *zhPtr;
	PTABLEROW trptr;
	UINT column;
	PTABLECELL cell;

	zhPtr = guiLockMem(control->table.rows);
	trptr = guiLockMem(zhPtr[row]);
	for (column = 0; column < control->table.numcolumns; column++) {
		if (ISTABLECOLUMNTEXT(tblColumns[column].type)) {
			cell = &trptr->cells[column];
			if (cell->text != NULL) guiFreeMem(cell->text);
		}
		else if (tblColumns[column].type == PANEL_DROPBOX)
		{
			cell = &trptr->cells[column];
			if (cell->dbcell.db.itemarray != NULL) guiresdestroytabledb(&cell->dbcell.db);
		}
	}
	guiUnlockMem(zhPtr[row]);
	guiFreeMem(zhPtr[row]);
	zhPtr[row] = NULL;
	guiUnlockMem(control->table.rows);
}


static INT guiResChangeTableColumnText(CONTROL *control, UCHAR *data, INT datalen) {

	PTABLECOLUMN tblColumns;
	UINT column;
	CHAR *cp1;
	CHAR work[64];

 	column = control->table.changeposition.x - 1;	// make it zero based
	if (column >= control->table.numcolumns) return RC_ERROR;
	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
	if (tblColumns[column].namelength < (UINT) (datalen + 1)) {
		tblColumns[column].name = guiReallocMem(tblColumns[column].name, datalen + 1);
		if (tblColumns[column].name == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiReallocMem returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return RC_NO_MEM;
		}
		tblColumns[column].namelength = datalen + 1;
	}
	cp1 = guiLockMem(tblColumns[column].name);
	memcpy(cp1, data, datalen);
	cp1[datalen] = '\0';
	guiUnlockMem(tblColumns[column].name);
	guiUnlockMem(control->table.columns);
	control->table.changetext = tblColumns[column].name;
	return 0;
}


/**
 * Given a pointer to a CONTROL struct that represents a table, and a row number (one-based)
 * return a ZHANDLE(unlocked) to the TABLEROW struct
 */
ZHANDLE guiTableLocateRow(CONTROL *control, INT row) {
	ZHANDLE zh, *zhPtr = guiLockMem(control->table.rows);
	zh = zhPtr[row - 1];
	guiUnlockMem(control->table.rows);
	return zh;
}

static INT guiResChangeTable(RESHANDLE reshandle, INT cmd, CONTROL *control, UCHAR *data, INT datalen) {

	ZHANDLE zh, zhNewRow, *zhPtr;
	PTABLEROW trptr, pNewRow;
	PTABLECOLUMN tblColumns;
	PTABLECELL cell;
	PTABLEDROPBOX dropbox;
	size_t bytecount;
	CHAR* pchar, work[128];
	UINT column, row, ctype;
	INT locateline;
	INT iInsertPos;
	INT i1, n1, n2;
	TABLECOLOR tbcolor;
	UCHAR** stringArray;
	COLORREF cref;

	switch(cmd) {

	case GUI_ADDROW:
		/* This command is applicable only to entire tables.
		 * Check the scope field for correctness.
		 */
		if (control->table.changescope != tableChangeScope_table) return RC_ERROR;
		control->table.numrows++;
		bytecount = sizeof(TABLEROW) + (control->table.numcolumns - 1) * sizeof(TABLECELL);
		zhNewRow = guiAllocMem((INT) bytecount);
		if (zhNewRow == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s at GUI_ADDROW", __FUNCTION__);
			guiaErrorMsg(work, (INT) bytecount);
			return RC_NO_MEM;
		}
		pNewRow = guiLockMem(zhNewRow);
		memset(pNewRow, 0, bytecount);
		for (i1 = 0; i1 < (INT)control->table.numcolumns; i1++) {
			pNewRow->cells[i1].textcolor.color = TEXTCOLOR_DEFAULT;
			pNewRow->cells[i1].textcolor.code = 0;
		}
		guiUnlockMem(zhNewRow);
		if ((i1 = guiResChangeTableGrowRows(control)) != 0) return i1;
		zhPtr = guiLockMem(control->table.rows);
		zhPtr[control->table.numrows - 1] = zhNewRow;
		guiUnlockMem(control->table.rows);
		break;

	case GUI_DELETEALLROWS:
		/* This command is applicable only to entire tables.
		 * Check the scope field for correctness.
		 */
		if (control->table.changescope != tableChangeScope_table) return RC_ERROR;
		if (control->table.numrows < 1 || control->table.rows == NULL) break;
		pvistart();
	 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
		for (row = 0; row < control->table.numrows; row++) {
			guiresfreetablerow(control, row, tblColumns);
		}
		guiUnlockMem(control->table.columns);
		guiFreeMem(control->table.rows);
		control->table.numrows = 0;
		control->table.rows = NULL;
		pviend();
		break;

	case GUI_DELETEROW:
		/* This command is applicable only to rows.
		 * Check the scope field for correctness.
		 */
		if (control->table.changescope != tableChangeScope_row) return RC_ERROR;
		if (control->table.numrows < 1 || control->table.rows == NULL) return RC_ERROR;
		row = control->table.changeposition.y; /* still one-based */
		if (row < 1 || control->table.numrows < row) return RC_ERROR;
	 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
		row--;		/* make it zero-based now */
		guiresfreetablerow(control, row, tblColumns);
		// move array around now
		zhPtr = guiLockMem(control->table.rows);
		memmove(zhPtr+row, zhPtr+row+1, (control->table.numrows - row - 1) * sizeof(ZHANDLE));
		guiUnlockMem(control->table.rows);

		control->table.numrows--;
		guiUnlockMem(control->table.columns);
		break;

	case GUI_INSERTROWBEFORE:
		/* This command is applicable only to rows.
		 * Check the scope field for correctness.
		 */
		if (control->table.changescope != tableChangeScope_row) return RC_ERROR;
		if (control->table.numrows < 1 || control->table.rows == NULL) return RC_ERROR;
		row = control->table.changeposition.y; /* still one-based */
		if (row < 1 || control->table.numrows < row) return RC_ERROR;
		bytecount = sizeof(TABLEROW) + (control->table.numcolumns - 1) * sizeof(TABLECELL);
		zhNewRow = guiAllocMem((INT) bytecount);
		if (zhNewRow == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s at GUI_INSERTROWBEFORE", __FUNCTION__);
			guiaErrorMsg(work, (DWORD)bytecount);
			return RC_NO_MEM;
		}
		pNewRow = guiLockMem(zhNewRow);
		memset(pNewRow, 0, bytecount);
		guiUnlockMem(zhNewRow);
		control->table.numrows++;
		if ((i1 = guiResChangeTableGrowRows(control)) != 0) return i1;
		row--;		/* make it zero-based now */
		zhPtr = guiLockMem(control->table.rows);
		memmove(zhPtr + row + 1, zhPtr + row, (control->table.numrows - row - 1) * sizeof(ZHANDLE));
		zhPtr[row] = zhNewRow;
		guiUnlockMem(control->table.rows);
		break;

	case GUI_FEDITMASK:
		if (data == NULL) return RC_INVALID_PARM;
	 	column = control->table.changeposition.x - 1;
		if (column >= control->table.numcolumns) return RC_ERROR;
	 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
		if (tblColumns == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiLockMem returned null in %s at GUI_FEDITMASK", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return RC_ERROR;
		}
	 	if (tblColumns[column].type != PANEL_FEDIT) {
			guiUnlockMem(control->table.columns);
	 		return RC_ERROR;
	 	}
		/* Allocate memory from heap for the text */
		tblColumns->mask = (tblColumns->mask == NULL) ? guiAllocMem(datalen + 1) : guiReallocMem(tblColumns->mask, datalen + 1);
		if (tblColumns->mask == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s at GUI_FEDITMASK", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return RC_NO_MEM;
		}

		/* Store the mask string in the table column structure */
		pchar = guiLockMem(tblColumns->mask);
		if (pchar == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiLockMem returned null in %s at GUI_FEDITMASK", __FUNCTION__);
			guiaErrorMsg(work, 2);
			return RC_ERROR;
		}
		memcpy(pchar, data, datalen);
		pchar[datalen] = '\0';
		guiUnlockMem(tblColumns->mask);
		control->table.changetext = tblColumns->mask;
	 	guiUnlockMem(control->table.columns);
		break;

	case GUI_TEXT:
		if (data == NULL) return RC_INVALID_PARM;
		// If the row number is not supplied, this is a request to change the column heading text
		if (control->table.changeposition.y < 1) {
			return guiResChangeTableColumnText(control, data, datalen);
		}
		if (control->table.numrows < 1 || control->table.rows == NULL) return RC_ERROR;
	 	column = control->table.changeposition.x - 1;
		row = control->table.changeposition.y; /* still one-based */
		if (row > control->table.numrows || column >= control->table.numcolumns) return RC_ERROR;
	 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
		if (tblColumns == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiLockMem returned null in %s at GUI_TEXT", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return RC_ERROR;
		}
	 	if (!ISTABLECOLUMNTEXT(tblColumns[column].type))
	 	{
			guiUnlockMem(control->table.columns);
	 		return RC_ERROR;
	 	}
		// Locate the TABLEROW struct
		zh = guiTableLocateRow(control, row);
		trptr = guiLockMem(zh);
		if (trptr == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiLockMem returned null in %s at GUI_TEXT", __FUNCTION__);
			guiaErrorMsg(work, 1);
			return RC_ERROR;
		}

		/* Allocate memory from heap for the text */
		cell = &trptr->cells[column];
		cell->text = (cell->text == NULL) ? guiAllocMem(datalen + 1) : guiReallocMem(cell->text, datalen + 1);
		if (cell->text == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s at GUI_TEXT", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return RC_NO_MEM;
		}

		/* Store the text string in the table cell structure */
		pchar = guiLockMem(cell->text);
		if (pchar == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiLockMem returned null in %s at GUI_TEXT", __FUNCTION__);
			guiaErrorMsg(work, 2);
			return RC_ERROR;
		}
		memcpy(pchar, data, datalen);
		pchar[datalen] = '\0';
		guiUnlockMem(cell->text);
		guiUnlockMem(zh);
		control->table.changetext = cell->text;
	 	guiUnlockMem(control->table.columns);
		break;

	case GUI_TEXTCOLOR:
		if (data == NULL) return RC_INVALID_PARM;
		i1 = GetColorNumberFromText(data);
		if (i1 == RC_ERROR) {
			i1 = GetColorNumberFromTextExt(data, datalen, &cref, NULL);
			if (i1 == RC_ERROR) return RC_ERROR;
			tbcolor.color = cref;
			tbcolor.code = 1;
		}
		else {
			tbcolor.color = i1;
			tbcolor.code = 0;
		}
		if (control->table.changescope == tableChangeScope_row) {
			/*
			 * Set the DB/C color number into the TABLEROW structure
			 * Error if the table has no rows
			 */
			if (control->table.numrows < 1 || control->table.rows == NULL) return RC_ERROR;
			row = control->table.changeposition.y; /* still one-based */
			if (row < 1 || row > control->table.numrows) return RC_ERROR;
			// Locate the TABLEROW struct
			zh = guiTableLocateRow(control, row);
			trptr = guiLockMem(zh);
			trptr->textcolor = tbcolor;
			guiUnlockMem(zh);
		}
		else if (control->table.changescope == tableChangeScope_column) {
			/**
			 * Determine the column, if out of range, error!
			 */
		 	column = control->table.changeposition.x - 1;
			if (column >= control->table.numcolumns) return RC_ERROR;
		 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
		 	if (!ISTABLECOLUMNTEXT(tblColumns[column].type))
		 	{
				guiUnlockMem(control->table.columns);
		 		return RC_ERROR;
		 	}
			tblColumns[column].textcolor = tbcolor;
		 	guiUnlockMem(control->table.columns);
		}
		else if (control->table.changescope == tableChangeScope_cell) {
			if (control->table.numrows < 1 || control->table.rows == NULL) return RC_ERROR;
			row = control->table.changeposition.y; /* still one-based */
			if (row < 1 || row > control->table.numrows) return RC_ERROR;
		 	column = control->table.changeposition.x - 1;
			if (column >= control->table.numcolumns) return RC_ERROR;
		 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
		 	if (!ISTABLECOLUMNTEXT(tblColumns[column].type))
		 	{
				guiUnlockMem(control->table.columns);
		 		return RC_ERROR;
		 	}
		 	guiUnlockMem(control->table.columns);
			// Locate the TABLEROW struct
			zh = guiTableLocateRow(control, row);
			trptr = guiLockMem(zh);
			cell = &trptr->cells[column];
			cell->textcolor = tbcolor;
			guiUnlockMem(zh);
		}
		else if (control->table.changescope == tableChangeScope_table) {
			control->table.textcolor = tbcolor;
		}
		break;

	case GUI_FOCUS:

		/**
		 * Just ignore an attempt to set a cell focus if the whole table is disabled
		 */
		if (control->style & CTRLSTYLE_DISABLED) break;

		/**
		 * Error if the table has no rows
		 */
		if (control->table.numrows < 1 || control->table.rows == NULL) return RC_ERROR;

		/**
		 * Determine the row and column, if they are out of range, error!
		 */
	 	column = control->table.changeposition.x - 1;
		row = control->table.changeposition.y; /* still one-based */
		if (row < 1 || row > control->table.numrows || column >= control->table.numcolumns) return RC_ERROR;
		break;

	case GUI_MARK:
	case GUI_UNMARK:
		if (control->table.numrows < 1 || control->table.rows == NULL) return RC_ERROR;
	 	column = control->table.changeposition.x - 1;
		row = control->table.changeposition.y; /* still one-based */
		if (row < 1 || row > control->table.numrows || column >= control->table.numcolumns) return RC_ERROR;
	 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
	 	if (tblColumns[column].type != PANEL_CHECKBOX) {
			guiUnlockMem(control->table.columns);
	 		return RC_ERROR;
	 	}

		// Locate the TABLEROW struct
		zh = guiTableLocateRow(control, row);
		trptr = guiLockMem(zh);
		if (cmd == GUI_MARK) trptr->cells[column].style |= CTRLSTYLE_MARKED;
		else if (cmd == GUI_UNMARK) trptr->cells[column].style &= ~CTRLSTYLE_MARKED;
		guiUnlockMem(zh);
	 	guiUnlockMem(control->table.columns);
		break;

	case GUI_MINSERT:

		/* Sanity checks */
		if (i1 = guiResChangeTableDBSanityChecks(cmd, control, data, datalen)) return i1;

		stringArray = SplitMinsertData(data, datalen, &n2);
		for (n1 = 0; n1 < n2; n1++) {
			if (i1 = guiResChangeTableInsert(control, stringArray[n1], (INT) strlen(stringArray[n1]))) return i1;
		}
		guiaChangeTableMinsert(*reshandle, control, stringArray, n2);
		FreeMinsertArray(stringArray, n2);
		break;

	case GUI_INSERT:

		/* Sanity checks */
		if (i1 = guiResChangeTableDBSanityChecks(cmd, control, data, datalen)) return i1;
		if (i1 = guiResChangeTableInsert(control, data, datalen)) return i1;
		break;

	case GUI_DELETE:

		/* Sanity checks */
		if (i1 = guiResChangeTableDBSanityChecks(cmd, control, data, datalen)) return i1;

	 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
	 	column = control->table.changeposition.x - 1;

	 	/* Identify the correct dropbox structure */
	 	ctype = tblColumns[column].type;
	 	if (ctype == PANEL_CDROPBOX) dropbox = &tblColumns[column].db;
	 	else {
			zh = guiTableLocateRow(control, control->table.changeposition.y);
			trptr = guiLockMem(zh);
			cell = &trptr->cells[column];
	 		dropbox = &cell->dbcell.db;
	 	}

		if (tblColumns[column].insertorder)
			i1 = guiListBoxAppendFind(dropbox->itemarray, dropbox->itemcount, control->changetext, &iInsertPos);
		else
			i1 = guiListBoxAlphaFind(dropbox->itemarray, dropbox->itemcount, control->changetext, &iInsertPos);

		guiFreeMem(control->changetext);
		control->changetext = NULL;
		if (i1) {
		 	if (ctype == PANEL_DROPBOX) guiUnlockMem(zh);
			guiUnlockMem(control->table.columns);
			return RC_ERROR;
		}

		guiResChangeTableDeleteFinish(dropbox, iInsertPos, ctype, cell, control);
	 	if (ctype == PANEL_DROPBOX) guiUnlockMem(zh);
		break;

	case GUI_DELETELINE:

		/* Sanity checks */
		if (i1 = guiResChangeTableDBSanityChecks(cmd, control, data, datalen)) return i1;

	 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
	 	column = control->table.changeposition.x - 1;

	 	/* Identify the correct dropbox structure */
	 	ctype = tblColumns[column].type;
	 	if (ctype == PANEL_CDROPBOX) dropbox = &tblColumns[column].db;
	 	else {
			zh = guiTableLocateRow(control, control->table.changeposition.y);
			trptr = guiLockMem(zh);
			cell = &trptr->cells[column];
			dropbox = &cell->dbcell.db;
	 	}

		numtoi(data, datalen, &n1);
		if (n1 < 1 || dropbox->itemcount < n1) {
		 	if (ctype == PANEL_DROPBOX) guiUnlockMem(zh);
			guiUnlockMem(control->table.columns);
			return RC_INVALID_PARM;
		}

		guiResChangeTableDeleteFinish(dropbox, (USHORT) n1, ctype, cell, control);
	 	if (ctype == PANEL_DROPBOX) guiUnlockMem(zh);
		break;

	case GUI_ERASE:

	 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
		if (tblColumns == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiLockMem returned null in %s at GUI_ERASE", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return RC_ERROR;
		}
	 	column = control->table.changeposition.x - 1;
	 	ctype = tblColumns[column].type;

		if (ISTABLECOLUMNTEXT(ctype)) {
			if (control->table.numrows < 1 || control->table.rows == NULL) return RC_ERROR;
			row = control->table.changeposition.y; /* still one-based */
			if (row < 1 || row > control->table.numrows || column >= control->table.numcolumns) return RC_ERROR;
			// Locate the TABLEROW struct
			zh = guiTableLocateRow(control, row);
			trptr = guiLockMem(zh);
			if (trptr == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiLockMem returned null in %s at GUI_ERASE", __FUNCTION__);
				guiaErrorMsg(work, 1);
				return RC_ERROR;
			}
			cell = &trptr->cells[column];
			if (cell->text == NULL) {
				cell->text = guiAllocMem(1);
				if (cell->text == NULL) {
					sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s at GUI_ERASE", __FUNCTION__);
					guiaErrorMsg(work, 1);
					return RC_NO_MEM;
				}
			}
			pchar = guiLockMem(cell->text);
			if (pchar == NULL) {
				sprintf_s(work, ARRAYSIZE(work), "guiLockMem returned null in %s at GUI_ERASE", __FUNCTION__);
				guiaErrorMsg(work, 2);
				return RC_ERROR;
			}
			pchar[0] = '\0';
			guiUnlockMem(cell->text);
			control->table.changetext = cell->text;
			guiUnlockMem(zh);
		 	guiUnlockMem(control->table.columns);
			break;
		}

		/* The rest of this section is for DROPBOX and CDROPBOX only */
		/* Sanity checks */
		if (i1 = guiResChangeTableDBSanityChecks(cmd, control, data, datalen)) {
		 	guiUnlockMem(control->table.columns);
			return i1;
		}

	 	/* Identify the correct dropbox structure */
	 	if (ctype == PANEL_CDROPBOX) dropbox = &tblColumns[column].db;
	 	else {
			zh = guiTableLocateRow(control, control->table.changeposition.y);
			trptr = guiLockMem(zh);
			cell = &trptr->cells[column];
			dropbox = &cell->dbcell.db;
	 	}

		/*
		 * The user may be asking to erase an empty dropbox
		 */
		if (dropbox->itemarray != NULL) {
			zhPtr = guiLockMem(dropbox->itemarray);
			for (i1 = 0; i1 < dropbox->itemcount; i1++) {
				guiFreeMem(zhPtr[i1]);
				zhPtr[i1] = NULL;
			}
			guiUnlockMem(dropbox->itemarray);
			dropbox->itemcount = 0;
		}
		dropbox->insstyle = LIST_INSERTNORMAL;

	 	if (ctype == PANEL_DROPBOX) guiUnlockMem(zh);
	 	guiUnlockMem(control->table.columns);
		break;

	case GUI_LOCATELINE:

		/* Sanity checks */
		if (i1 = guiResChangeTableDBSanityChecks(cmd, control, data, datalen)) return i1;

	 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
	 	column = control->table.changeposition.x - 1;

		row = control->table.changeposition.y; /* for convenience, and it is still one-based */

	 	/* Identify the correct dropbox structure */
	 	ctype = tblColumns[column].type;
	 	if (ctype == PANEL_CDROPBOX) dropbox = &tblColumns[column].db;
	 	else {
			zh = guiTableLocateRow(control, row);
			trptr = guiLockMem(zh);
			cell = &trptr->cells[column];
			dropbox = &cell->dbcell.db;
	 	}

		numtoi(data, datalen, &i1);
		locateline = i1;
		if (locateline > dropbox->itemcount) {
			guiUnlockMem(control->table.columns);
			/**
			 * We used to return RC_ERROR here.
			 * Changed on 07/17/12 (jpr) to simply ignore a bad line number.
			 * This makes it consistant with a stand-alone dropbox.
			 */
			break;
		}

	 	if (tblColumns[column].type == PANEL_CDROPBOX) {
	 		if (control->table.rows == NULL) return RC_ERROR;
	 		guiResChangeTableCDLocate(row, control, locateline);
	 	}
	 	else {
	 		cell->dbcell.itemslctd = locateline;
	 		guiUnlockMem(zh);
	 	}

	 	guiUnlockMem(control->table.columns);
		control->value3 = locateline;
		break;

	case GUI_LOCATE:

		/* Sanity checks */
		if (i1 = guiResChangeTableDBSanityChecks(cmd, control, data, datalen)) return i1;

	 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
	 	column = control->table.changeposition.x - 1;

		row = control->table.changeposition.y; /* for convenience, and it is still one-based */

	 	/* Identify the correct dropbox structure */
	 	ctype = tblColumns[column].type;
	 	if (ctype == PANEL_CDROPBOX) dropbox = &tblColumns[column].db;
	 	else {
			zh = guiTableLocateRow(control, row);
			trptr = guiLockMem(zh);
			cell = &trptr->cells[column];
			dropbox = &cell->dbcell.db;
	 	}

		if (tblColumns[column].insertorder)
			i1 = guiListBoxAppendFind(dropbox->itemarray, dropbox->itemcount, control->changetext, &locateline);
		else
			i1 = guiListBoxAlphaFind(dropbox->itemarray, dropbox->itemcount, control->changetext, &locateline);

		guiFreeMem(control->changetext);
		control->changetext = NULL;
		if (i1) {
			if (ctype == PANEL_DROPBOX) guiUnlockMem(zh);
		 	guiUnlockMem(control->table.columns);
			/**
			 * We used to return RC_ERROR here.
			 * Changed on 07/17/12 (jpr) to simply ignore it when the text is not found.
			 * This makes it consistant with a stand-alone dropbox.
			 */
			break;
		}

	 	if (ctype == PANEL_CDROPBOX) {
	 		if (control->table.rows == NULL) return RC_ERROR;
	 		guiResChangeTableCDLocate(row, control, locateline);
	 	}
	 	else {
	 		cell->dbcell.itemslctd = locateline;
	 		guiUnlockMem(zh);
	 	}

	 	guiUnlockMem(control->table.columns);
		control->value3 = locateline;
		break;

	case GUI_INSERTBEFORE:
	case GUI_INSERTAFTER:

		/* Sanity checks */
		if (i1 = guiResChangeTableDBSanityChecks(cmd, control, data, datalen)) return i1;

	 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
	 	column = control->table.changeposition.x - 1;

	 	/* Identify the correct dropbox structure */
	 	ctype = tblColumns[column].type;
		if (ctype == PANEL_CDROPBOX) dropbox = &tblColumns[column].db;
	 	else {
			zh = guiTableLocateRow(control, control->table.changeposition.y);
			trptr = guiLockMem(zh);
			cell = &trptr->cells[column];
			dropbox = &cell->dbcell.db;
	 	}

		if (datalen == 0) {
			if (cmd == GUI_INSERTBEFORE) {
				dropbox->insstyle = LIST_INSERTBEFORE;
				dropbox->inspos = 1;
			}
			else {
				dropbox->insstyle = LIST_INSERTNORMAL;
			}
		}
		else {
			i1 = guiListBoxAppendFind(dropbox->itemarray, dropbox->itemcount, control->changetext, &iInsertPos);
			guiFreeMem(control->changetext);
			control->changetext = NULL;
			if (i1) {
			 	if (ctype == PANEL_DROPBOX) guiUnlockMem(zh);
			 	guiUnlockMem(control->table.columns);
				return RC_ERROR;
			}

			dropbox->inspos = iInsertPos;
			dropbox->insstyle = (cmd == GUI_INSERTBEFORE) ? LIST_INSERTBEFORE : LIST_INSERTAFTER;
		}

	 	if (ctype == PANEL_DROPBOX) guiUnlockMem(zh);
	 	guiUnlockMem(control->table.columns);
		break;

	case GUI_INSERTLINEBEFORE:
	case GUI_INSERTLINEAFTER:

		/* Sanity checks */
		if (i1 = guiResChangeTableDBSanityChecks(cmd, control, data, datalen)) return i1;

	 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
	 	column = control->table.changeposition.x - 1;

	 	/*
	 	 * Identify the correct dropbox structure
	 	 * It is per-cell for DROPBOX and per-column for CDROPBOX
	 	 */
	 	ctype = tblColumns[column].type;
		if (ctype == PANEL_CDROPBOX) {
	 		dropbox = &tblColumns[column].db;
		}
	 	else {
			zh = guiTableLocateRow(control, control->table.changeposition.y);
			trptr = guiLockMem(zh);
			cell = &trptr->cells[column];
			dropbox = &cell->dbcell.db;
	 	}

		numtoi(data, datalen, &n1);
		if (n1 < 1) n1 = 1;
		else if (n1 > dropbox->itemcount) n1 = dropbox->itemcount;
		dropbox->inspos = n1;
		if (dropbox->inspos < 1) dropbox->inspos = 1;	// cannot be less than one.
		dropbox->insstyle = (cmd == GUI_INSERTLINEBEFORE) ? LIST_INSERTBEFORE : LIST_INSERTAFTER;

	 	if (ctype == PANEL_DROPBOX) guiUnlockMem(zh);
	 	guiUnlockMem(control->table.columns);
		break;

	case GUI_AVAILABLE:
		//
		//CODE We should try to code this.
		//     This is here to prevent an I794 when the user does a grayall or availableall.
		// as of 04/05/11 this is correctly cancelling a readonly state
		control->style &= ~CTRLSTYLE_DISABLED;
		control->style &= ~CTRLSTYLE_READONLY;
		break;

	case GUI_GRAY:
		//
		//CODE We should try to code this.
		//     This is here to prevent an I794 when the user does a grayall or availableall.
		control->style |= CTRLSTYLE_DISABLED;
		break;

	case GUI_READONLY:
		//
		// as of 04/05/11 this is working
		//
		control->style |= CTRLSTYLE_READONLY;
		break;

	default:	/* If it is not a command relevant to a table */
		return RC_ERROR;

	} /* switch(cmd) */

	return 0;
}

/**
 * Input value validation for commands that affect dropbox columns
 */
static INT guiResChangeTableDBSanityChecks(INT cmd, CONTROL *control, UCHAR *data, INT datalen) {

	PTABLECOLUMN tblColumns;
	UINT row, column;
	CHAR work[64];

	/* Is the column position out of range? This should never happen */
	if (control->table.changeposition.x == 0 || (UINT) control->table.changeposition.x > control->table.numcolumns)
		return RC_ERROR;

	/* Is this column in fact a dropbox column? */
 	tblColumns = guiLockMem(control->table.columns);
	column = control->table.changeposition.x - 1;
 	if (!ISTABLECOLUMNDROPBOX(tblColumns[column].type)) {
	 	guiUnlockMem(control->table.columns);
 		return RC_ERROR;
 	}

	/* Some commands require data */
	if (cmd == GUI_MINSERT || cmd == GUI_INSERT || cmd == GUI_DELETE || cmd == GUI_DELETELINE
		|| cmd == GUI_LOCATE || cmd == GUI_LOCATELINE || cmd == GUI_INSERTLINEBEFORE
		|| cmd == GUI_INSERTLINEAFTER)
	{
		if (data == NULL) {
		 	guiUnlockMem(control->table.columns);
			return RC_INVALID_PARM;
		}
	}

	/* Some commands require the column to be in insertorder */
	if (cmd == GUI_INSERTBEFORE || cmd == GUI_INSERTAFTER || cmd == GUI_INSERTLINEBEFORE || cmd == GUI_INSERTLINEAFTER)
	{
		if (!tblColumns[column].insertorder) {
		 	guiUnlockMem(control->table.columns);
			return RC_ERROR;
		}
	}

	/* For some commands, if the column is a DROPBOX, further validations are required */
	if (tblColumns[column].type == PANEL_DROPBOX) {
		if (cmd == GUI_DELETE || cmd == GUI_DELETELINE || cmd == GUI_ERASE
			|| cmd == GUI_LOCATE || cmd == GUI_LOCATELINE
			|| cmd == GUI_INSERT
			|| cmd == GUI_INSERTBEFORE || cmd == GUI_INSERTAFTER
			|| cmd == GUI_INSERTLINEBEFORE || cmd == GUI_INSERTLINEAFTER)
		{
			if (control->table.numrows < 1 || control->table.rows == NULL) {
				guiUnlockMem(control->table.columns);
				return RC_ERROR;
			}
		 	row = control->table.changeposition.y;
	 		if (row < 1 || row > control->table.numrows) {
			 	guiUnlockMem(control->table.columns);
	 			return RC_INVALID_PARM;
	 		}
		}
	}

	/* Some commands require a search string */
	if (cmd == GUI_DELETE || cmd == GUI_LOCATE
		|| (data != NULL && datalen > 0 && (cmd == GUI_INSERTBEFORE || cmd == GUI_INSERTAFTER)))
	{
		if (control->changetext != NULL) guiFreeMem(control->changetext);
		control->changetext = guiAllocString(data, datalen);
		if (control->changetext == NULL) {
			guiUnlockMem(control->table.columns);
			sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s", __FUNCTION__);
			guiaErrorMsg(work, datalen);
			return RC_NO_MEM;
		}
	}

 	guiUnlockMem(control->table.columns);
	return 0;
}

/**
 * Code common to GUI_DELETE and GUIDELETELINE
 */
static void guiResChangeTableDeleteFinish(PTABLEDROPBOX dropbox, INT iDeletePos,
		INT ctype, PTABLECELL cell, CONTROL *control)
{

	ZHANDLE *zhPtr, zh;
	UINT row;
	PTABLEROW trptr;
	UINT column = control->table.changeposition.x - 1;

	zhPtr = guiLockMem(dropbox->itemarray);
	zhPtr += iDeletePos - 1;
	guiFreeMem(*zhPtr);
	memmove(zhPtr, zhPtr + 1, (dropbox->itemcount - iDeletePos) * sizeof(ZHANDLE));
	dropbox->itemcount--;
	guiUnlockMem(dropbox->itemarray);

	/* May need to clear/move our selection value */
 	if (ctype == PANEL_DROPBOX) {
 		if (cell->dbcell.itemslctd == iDeletePos) cell->dbcell.itemslctd = 0;
 		else if (cell->dbcell.itemslctd > iDeletePos) cell->dbcell.itemslctd--;
 	}
 	/* If this is a CDROPBOX we want to check every row */
 	else {
 		for (row = 1; row <= control->table.numrows; row++) {
			zh = guiTableLocateRow(control, row);
			trptr = guiLockMem(zh);
			cell = &trptr->cells[column];
	 		if (cell->dbcell.itemslctd == iDeletePos) cell->dbcell.itemslctd = 0;
	 		else if (cell->dbcell.itemslctd > iDeletePos) cell->dbcell.itemslctd--;
	 		guiUnlockMem(zh);
 		}
 	}
 	guiUnlockMem(control->table.columns);
	control->value3 = iDeletePos;	/* Send this as one-based */
}

/**
 * @param row one-based
 * @param index also one-based
 *
 * Deals with a CDROPBOX column in a table, LOCATE and LOCATELINE changes.
 * For this type of table column, the programmer may optionally include row
 * information. If supplied then do just that cell, if not then do the entire column.
 */
static void guiResChangeTableCDLocate(UINT row, CONTROL *control, USHORT index) {

	ZHANDLE zh, *zhPtr;
	PTABLEROW trptr;
 	UINT column = control->table.changeposition.x - 1;

	if (row == 0) { /* row was not supplied, do all of them */
		zhPtr = guiLockMem(control->table.rows);
		for (; row < control->table.numrows; row++) {
			trptr = (TABLEROW *) guiLockMem(zhPtr[row]);
			trptr->cells[column].dbcell.itemslctd = index;
			guiUnlockMem(zhPtr[row]);
		}
		guiUnlockMem(control->table.rows);
	}
	else { /* row was supplied, do just that one */
		zh = guiTableLocateRow(control, row);
		trptr = guiLockMem(zh);
		trptr->cells[column].dbcell.itemslctd = index;
		guiUnlockMem(zh);
	}
}


/**
 * Assumptions
 *   data is not NULL
 *   the column is a DROPBOX or a CDROPBOX
 *   control->table.changeposition.x is in range
 */
static INT guiResChangeTableInsert(CONTROL *control, CHAR *data, INT datalen) {

	PTABLECOLUMN tblColumns;
	INT retval = 0;
	UINT row, column, ctype;
	ZHANDLE *zhPtr, zh = NULL;
	INT iInsertPos;
	PTABLEROW trptr;
	PTABLECELL cell;
	PTABLEDROPBOX dropbox;
	CHAR work[64];

 	column = control->table.changeposition.x - 1;
 	tblColumns = guiLockMem(control->table.columns);
 	ctype = tblColumns[column].type;

 	/*
 	 * It is an error to issue an insert command to a dropbox column without a row number
 	 */
 	if (ctype == PANEL_DROPBOX && control->table.changeposition.y == 0) {
 	 	guiUnlockMem(control->table.columns);
 		return RC_ERROR;
 	}

	control->table.changetext = guiAllocString(data, datalen);
	if (control->table.changetext == NULL) {
	 	guiUnlockMem(control->table.columns);
	 	sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s", __FUNCTION__);
		guiaErrorMsg(work, datalen);
		return RC_NO_MEM;
	}

 	if (ctype == PANEL_CDROPBOX) {
 		dropbox = &tblColumns[column].db;
 	}
 	else {
		zh = guiTableLocateRow(control, control->table.changeposition.y);
		trptr = guiLockMem(zh);
		cell = &trptr->cells[column];
		dropbox = &cell->dbcell.db;
 	}

	dropbox->itemcount++;
	if (dropbox->itemalloc == 0) {
		dropbox->itemarray = guiAllocMem(LISTITEM_GROW_SIZE * sizeof(ZHANDLE));
		if (dropbox->itemarray == NULL) {
		 	guiUnlockMem(control->table.columns);
		 	if (ctype == PANEL_DROPBOX) guiUnlockMem(zh);
		 	sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return RC_NO_MEM;
		}
		dropbox->itemalloc = LISTITEM_GROW_SIZE;
	}
	else if (dropbox->itemcount > dropbox->itemalloc) {
		dropbox->itemalloc += LISTITEM_GROW_SIZE;
		dropbox->itemarray = guiReallocMem(dropbox->itemarray, dropbox->itemalloc * sizeof(ZHANDLE));
		if (dropbox->itemarray == NULL) {
		 	guiUnlockMem(control->table.columns);
		 	if (ctype == PANEL_DROPBOX) guiUnlockMem(zh);
		 	sprintf_s(work, ARRAYSIZE(work), "guiReallocMem returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return RC_NO_MEM;
		}
	}
	zhPtr = guiLockMem(dropbox->itemarray);

	if (tblColumns[column].insertorder) {
		if (dropbox->insstyle == LIST_INSERTNORMAL) {			/* At the end */
			iInsertPos = dropbox->itemcount;
		}
		else if (dropbox->insstyle == LIST_INSERTBEFORE) {
			iInsertPos = dropbox->inspos;
			if (iInsertPos > dropbox->itemcount) iInsertPos = dropbox->itemcount;
		}
		else if (dropbox->insstyle == LIST_INSERTAFTER) {
			iInsertPos = dropbox->inspos + 1;
			if (iInsertPos > dropbox->itemcount) iInsertPos = dropbox->itemcount;
		}
	}
	else {
		guiListBoxAlphaFind(dropbox->itemarray, dropbox->itemcount - 1, control->table.changetext, &iInsertPos);
	}
	zhPtr += iInsertPos - 1;
	memmove(zhPtr + 1, zhPtr, (dropbox->itemcount - iInsertPos) * sizeof(ZHANDLE));
	*zhPtr = control->table.changetext;

	guiUnlockMem(dropbox->itemarray);
	control->value3 = dropbox->inspos;

	/* Our value for item selected might have to move */
	if (ctype == PANEL_DROPBOX) {
		if (cell->dbcell.itemslctd > iInsertPos) cell->dbcell.itemslctd++;
	 	guiUnlockMem(zh);
	}
	else {
 		for (row = 1; row <= control->table.numrows; row++) {
			zh = guiTableLocateRow(control, row);
			trptr = guiLockMem(zh);
			cell = &trptr->cells[column];
			if (cell->dbcell.itemslctd > iInsertPos) cell->dbcell.itemslctd++;
	 		guiUnlockMem(zh);
 		}
	}
 	guiUnlockMem(control->table.columns);

 	return retval;
}

/*
 * this function will search through a list box/drop box entries
 * (which are in alphabetical order), trying to find the target string
 * The return value in poffset is ONE-BASED
 * If not found, return value is RC_ERROR.
 * If found, return value is zero.
 */
INT guiListBoxAlphaFind(ZHANDLE itemarray, UINT arraysize, ZHANDLE zhTarget, INT *poffset)
{
	INT low, high, mid, cmpresult;
	CHAR *targetText = guiLockMem(zhTarget);
	ZHANDLE *array;
	CHAR *sourceText;

	/* use a binary search */
	if (itemarray == NULL) return RC_ERROR;
	low = 0;
	high = arraysize - 1;
	array = guiLockMem(itemarray);
	while (high >= low) {
		mid = (high + low) >> 1;
		sourceText = (CHAR*) guiLockMem(array[mid]);
		cmpresult = lstrcmpi(targetText, sourceText);
		guiUnlockMem(array[mid]);
		if (cmpresult == 0) {	/* found it! */
			*poffset = mid + 1;
			guiUnlockMem(zhTarget);
			guiUnlockMem(itemarray);
			return 0;
		}
		if (cmpresult < 0) high = mid - 1;
		else low = mid + 1;
	}
	*poffset = low + 1;
	guiUnlockMem(zhTarget);
	guiUnlockMem(itemarray);
	return RC_ERROR;
}

/*
 * this function will search through a list box/drop box entries
 * (which are in sequential order), trying to find the target string
 * If not found, return value is RC_ERROR
 * If found, return value is zero.
 */
INT guiListBoxAppendFind(ZHANDLE itemarray, UINT arraysize, ZHANDLE zhTarget, INT *poffset)
{
	ZHANDLE *array;
	CHAR *targetText = guiLockMem(zhTarget);
	CHAR *sourceText;
	UINT offset;
	CHAR firstchar;

	if (itemarray == NULL) return RC_ERROR;
	array = guiLockMem(itemarray);
	firstchar = targetText[0];
	for (offset = 0; offset < arraysize; offset++) {
		sourceText = (CHAR*) guiLockMem(array[offset]);
		if (sourceText[0] == firstchar) {
			if (!strcmp((CHAR *) targetText, sourceText)) {
				*poffset = offset + 1;
				guiUnlockMem(array[offset]);
				guiUnlockMem(itemarray);
				guiUnlockMem(zhTarget);
				return 0;  /* found it! */
			}
		}
		guiUnlockMem(array[offset]);
	}
	*poffset = 0;
	guiUnlockMem(zhTarget);
	guiUnlockMem(itemarray);
	return RC_ERROR;
}


/**
 * return -2 if an unknown command is encountered
 *        -1 if the end of the string is found
 *         0 if the token is an item number, itemnumber will have the value
 *           (and possibly subitemnumber)
 *        >0 if the token is a command, the return value is the command code
 */
static INT nextChangeStringToken(UCHAR *str, INT strlen, INT *strpos, INT *itemnumber, INT *subitemnumber) {
	CHAR cmd[20];
	INT pos, cmdval;

	*itemnumber = *subitemnumber = INT_MIN;
	if (*strpos >= strlen) return -1;
	while ((isspace(str[*strpos]) || str[*strpos] == ',') && *strpos < strlen) (*strpos)++;
	if (*strpos >= strlen) return -1;
	if (isdigit(str[*strpos])) {
		/* convert user item number to an integer */
		*itemnumber = 0;
		while (*strpos < strlen && isdigit(str[*strpos])) {
			*itemnumber = *itemnumber * 10 + str[*strpos] - '0';
			(*strpos)++;
		}
		while (*strpos < strlen && isspace(str[*strpos])) (*strpos)++;
		if (*strpos < strlen && str[*strpos] == '[') {
			do (*strpos)++; while (*strpos < strlen && isspace(str[*strpos]));
			while (*strpos < strlen && isdigit(str[*strpos])) {
				*subitemnumber = *subitemnumber * 10 + str[*strpos] - '0';
				(*strpos)++;
			}
			if (str[*strpos] != ']') return -2;
			(*strpos)++;
		}
		return 0;
	}
	else {
		/* Parse command STRING into cmd */
		for (pos = 0; *strpos < strlen && isalpha(str[*strpos]); pos++) {
			cmd[pos] = toupper(str[(*strpos)++]);
		}
		cmd[pos] = '\0';
		/* find cmd STRING in symtable */
		for (cmdval = 0; symtable[cmdval].name != NULL; cmdval++) {
			if (!strcmp(cmd, symtable[cmdval].name)) {
				return symtable[cmdval].value;
			}
		}
		return -2;
	}
}

#endif		// OS_WIN32GUI

INT guireschange3(RESHANDLE reshandle, INT cmd, UCHAR *data, INT datalen) {
#if OS_WIN32GUI
	RESOURCE *res = *reshandle;
	MENUENTRY *menuitem;
	INT i1, i2, cmd2;
	CONTROL *control;

	if (res->restype == RES_TYPE_MENU || res->restype == RES_TYPE_POPUPMENU) {
		if (cmd == GUI_GRAYALL || cmd == GUI_AVAILABLEALL) {
			menuitem = (MENUENTRY *) guiMemToPtr(res->content);
			cmd2 = (cmd == GUI_GRAYALL) ? GUI_GRAY : GUI_AVAILABLE;
			for (i1 = res->entrycount; i1 ; i1--, menuitem++) if (i2 = guireschangemenu(reshandle, cmd2, menuitem, NULL, 0)) return i2;
			return 0;
		}
		else return RC_INVALID_PARM;
	}
	else if (res->restype == RES_TYPE_PANEL || res->restype == RES_TYPE_DIALOG) {
		if (cmd == GUI_GRAYALL || cmd == GUI_AVAILABLEALL) {
			control = (CONTROL *) guiMemToPtr(res->content);
			cmd2 = (cmd == GUI_GRAYALL) ? GUI_GRAY : GUI_AVAILABLE;
			for (i1 = res->entrycount; i1 ; i1--, control++) {
				if (control->type == PANEL_TAB || control->useritem == 0) continue;
				if (i2 = guireschangepnldlg(reshandle, cmd2, control, NULL, 0)) return i2;
			}
			return 0;
		}
		else return guireschangepnldlg(reshandle, cmd, NULL, data, datalen);
	}
	else if (RES_TYPE_TOOLBAR == res->restype) {
		if (cmd == GUI_GRAYALL || cmd == GUI_AVAILABLEALL) {
			control = (CONTROL *) guiMemToPtr(res->content);
			cmd2 = (cmd == GUI_GRAYALL) ? GUI_GRAY : GUI_AVAILABLE;
			for (i1 = res->entrycount; i1 ; i1--, control++) {
				if (control->useritem == 0) continue;  /* This is a space */
				if (i2 = guireschangetoolbar(reshandle, cmd2, control->useritem, NULL, 0)) return i2;
			}
			return 0;
		}
		else return guireschangetoolbar(reshandle, cmd, 0, data, datalen);
	}
	else return guireschangeother(reshandle, cmd, data, datalen);
#else
	return -1;
#endif
}

INT guireschange2(RESHANDLE reshandle, INT cmd, INT useritem, INT subuseritem, UCHAR *data, INT datalen) {
#if OS_WIN32GUI
	RESOURCE *res;
	MENUENTRY *menuitem;
	INT i1;
	UINT column;
	CONTROL *control;
	PTABLECOLUMN tblColumns;

	res = *reshandle;
	if (res->restype == RES_TYPE_MENU || res->restype == RES_TYPE_POPUPMENU) {
		menuitem = (MENUENTRY *) guiMemToPtr(res->content);
		for (i1 = res->entrycount; i1 && menuitem->useritem != (ULONG) useritem; i1--, menuitem++);
		if (i1) return guireschangemenu(reshandle, cmd, menuitem, data, datalen);
		return RC_INVALID_PARM;
	}
	else if (RES_TYPE_TOOLBAR == res->restype) {
		return guireschangetoolbar(reshandle, cmd, useritem, data, datalen);
	}
	else if (res->restype == RES_TYPE_PANEL || res->restype == RES_TYPE_DIALOG) {
		if (res->content != NULL) {
			control = (CONTROL *) guiMemToPtr(res->content);
			for (i1 = res->entrycount; i1; i1--, control++) {
				if (control->useritem == useritem) {
					if (control->type == PANEL_TABLE) {
						control->table.changeposition.y = subuseritem;
						control->table.changescope = (subuseritem == INT_MIN) ? tableChangeScope_table : tableChangeScope_row;
					}
					break;
				}
				if (control->type == PANEL_TABLE) {
				 	tblColumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
				 	for (column = 0; column < control->table.numcolumns && tblColumns->useritem != useritem; column++, tblColumns++);
				 	guiUnlockMem(control->table.columns);
					if (column < control->table.numcolumns) {
						control->table.changeposition.x = column + 1;
						control->table.changeposition.y = (subuseritem == INT_MIN) ? 0 : subuseritem;
						control->table.changescope = (subuseritem == INT_MIN) ? tableChangeScope_column : tableChangeScope_cell;
						break;
					}
				}
			}
			if (!i1) {
				return RC_INVALID_PARM;
			}
		}
		i1 = guireschangepnldlg(reshandle, cmd, control, data, datalen);
		return i1;
	}
#endif
	return -1;
}

/**
 * change a resource
 */
INT guireschange(RESHANDLE reshandle, UCHAR *fnc, INT fnclen, UCHAR *data, UINT datalen)
{
#if OS_WIN32GUI
	INT useritem, subuseritem, state = 0;
	INT lastcmd, i1, i2;
	INT position = 0;
	RESHANDLE rh;

	for (rh = reshandlehdr; rh != NULL && rh != reshandle; rh = (*rh)->nextreshandle);
	if (rh == NULL) return RC_INVALID_HANDLE;

	for (;;) {
		i1 = nextChangeStringToken(fnc, fnclen, &position, &useritem, &subuseritem);
		if (i1 == -2) return RC_INVALID_VALUE; /* unrecognized command */
		if (i1 == -1) break;	/* end of command string reached */
		switch (state) {
		case 0:					/* just starting */
			if (i1 == 0) return RC_ERROR;		/* the command string did not start with a command */
			lastcmd = i1;
			state = 1;
			break;
		case 1:					/* last thing seen was a command */
			if (i1) {			/* a command was parsed, no intervening item numbers, execute the last command */
				i2 = guireschange3(rh, lastcmd, data, datalen);
				if (i2) return i2;
				lastcmd = i1;
			}
			else {				/* an item number was parsed, execute the last command with it */
				i2 = guireschange2(rh, lastcmd, useritem, subuseritem, data, datalen);
				if (i2) return i2;
				state = 2;
			}
			break;
		case 2:					/* last thing seen was an item number */
			if (i1) {			/* a command was parsed, save it */
				lastcmd = i1;
				state = 1;
			}
			else {				/* an item number was parsed, execute the last command with it */
				i2 = guireschange2(rh, lastcmd, useritem, subuseritem, data, datalen);
				if (i2) return i2;
			}
			break;
		}
	}
	if (state == 1) return guireschange3(rh, lastcmd, data, datalen);
	return 0;
#else
	return RC_ERROR;
#endif
}

/**
 * This function uses guiAllocMem to create a buffer as long as the longest individual change statement.
 */
INT guiMultiChange(RESHANDLE reshandle, UCHAR *data, INT datalen) {
#if OS_WIN32GUI

	/*
	 * Verify that the data are has a balanced set of curly braces
	 */
	CHAR *semicolon, c1, *chgreq, work[64];
	ZHANDLE zh;
	INT i1, i2, i3, charcount, maxChgreqLen = 0;
	INT state; /* 0 == a right brace has just been seen, 1 == looking for a right brace */

	if (data[0] != '{') return RC_ERROR;
	state = 1; /* looking for a right brace */
	charcount = 0;
	for (i1 = 1; i1 < datalen; i1++) {
		c1 = data[i1];
		switch (state) {
		case 0:
			if (c1 == '{') {
				state = 1;
				break;
			}
			else if (c1 != ' ') return RC_ERROR;
		case 1:
			if (c1 == '\\') {
				i1 += 2;
				charcount += 2;
				if (i1 >= datalen) break;
				c1 = data[i1];
			}
			if (c1 == '}') {
				state = 0;
				if (charcount > maxChgreqLen) maxChgreqLen = charcount;
				charcount = 0;
			}
			else ++charcount;
			break;
		}
	}
	if (state != 0 || maxChgreqLen < 1) return RC_ERROR;

	zh = guiAllocMem(maxChgreqLen + 1);  /* plus one for the null terminator */
	if (zh == NULL) {
		sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return RC_NO_MEM;
	}
	chgreq = guiLockMem(zh);
	if (chgreq == NULL) {
		sprintf_s(work, ARRAYSIZE(work), "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return RC_ERROR;
	}
	/*
	 * Break the data up into individual change statements and feed them to guireschange.
	 * Note that a 'multiple' change may not be nested.
	 */
	state = 1; /* Left brace seen */
	i2 = 0;
	for (i1 = 1; i1 < datalen; i1++) {
		c1 = data[i1];
		switch (state) {
		case 0:
			if (c1 == '{') state = 1;
			break;
		case 1:
			if (c1 == '\\') {
				if (data[i1 + 1] != '{' && data[i1 + 1] != '}') {
					chgreq[i2++] = c1;
					chgreq[i2++] = data[++i1];
					c1 = data[++i1];
				}
				else {
					chgreq[i2++] = data[++i1];
					c1 = data[++i1];
				}
			}
			if (c1 != '}') chgreq[i2++] = c1;
			else {
				chgreq[i2] = '\0';
				/* Feed to guireschange goes here */
				semicolon = strchr(chgreq, ';');
				if (semicolon == NULL) {
					i3 = guireschange(reshandle, chgreq, i2, NULL, 0);
				}
				else {
					i3 = guireschange(reshandle, chgreq, (INT)(semicolon - chgreq), semicolon + 1, (UINT)(i2 - 1 - (semicolon - chgreq)));
				}
				if (i3) goto errreturn;
				state = 0;
				i2 = 0;
			}
			break;
		}
	}
	guiUnlockMem(zh);
	guiFreeMem(zh);
	return 0;

errreturn:
	guiUnlockMem(zh);
	guiFreeMem(zh);
	return i3;
#else
	return RC_ERROR;
#endif
}

/**
 * Show a resource in a window
 */
LRESULT guiresshow(RESHANDLE reshandle, WINHANDLE winhandle, INT h, INT v)
{
#if OS_WIN32GUI
	RESHANDLE rh;
	RESOURCE *res;
	WINHANDLE wh;
	WINDOW *win;
	LRESULT rc;
	CHAR *errmsg;


	for (rh = reshandlehdr; rh != NULL && rh != reshandle; rh = (*rh)->nextreshandle);
	for (wh = winhandlehdr; wh != NULL && wh != winhandle; wh = (*wh)->nextwinhandle);
	if (rh == NULL || wh == NULL) {
		return RC_INVALID_HANDLE;
	}

	/**
	 * If the resource is already being shown on a window, hide it first
	 */
	if ((*rh)->winshow != NULL) guireshide(rh);

	win = *wh;
	res = *rh;
	res->winshow = wh;
	switch (res->restype) {
	case RES_TYPE_MENU:
		win->showmenu = rh;
		rc = guiaShowMenu(rh);
		break;
	case RES_TYPE_POPUPMENU:
		rc = guiaShowPopUpMenu(rh, h, v);
		break;
	case RES_TYPE_PANEL:
	case RES_TYPE_DIALOG:
		res->showresnext = NULL;
		if (win->showreshdr == NULL) win->showreshdr = rh;
		else {
			for (rh = win->showreshdr; (*rh)->showresnext != NULL; rh = (*rh)->showresnext);
			(*rh)->showresnext = reshandle;
			/* Return an error if the user defined more than 1 default pushbutton */
			if (res->restype == RES_TYPE_PANEL && res->ctlorgdflt != 0) {
				for (rh = win->showreshdr; (*rh)->showresnext != NULL; rh = (*rh)->showresnext) {
					if ((*rh) != res && (*rh)->ctlorgdflt != 0) return RC_ERROR;
				}
			}
		}
		if (res->restype != RES_TYPE_DIALOG) {
			if (win->pandlgscale && h != INT_MIN && v != INT_MIN) {
				h = (h * scalex) / scaley;
				v = (v * scalex) / scaley;
			}
		}
		if (res->ctlorgdflt != 0) {
			win->ctlorgdflt = (CONTROL *) guiMemToPtr(res->content) + res->ctlorgdflt - 1;
		}

		clearLastErrorMessage();
		rc = guiaShowControls(res, h, v);
		errmsg = guiaGetLastErrorMessage();
		if (strlen(errmsg) != 0) { setLastErrorMessage(errmsg); }

		if (win->ctlorgdflt != NULL) {
			win->ctldefault = win->ctlorgdflt;
		}
		break;

	case RES_TYPE_TOOLBAR:
		/*
		 * Only allow one toolbar to be shown on a window at a time.
		 * If one is already there, hide it.
		 */
		if (win->showtoolbar != NULL) guiaHideToolbar(*win->showtoolbar);
		win->showtoolbar = rh;
		rc = guiaShowToolbar(res);
		break;

	case RES_TYPE_OPENFILEDLG:
		rc = guiaShowOpenFileDlg(res);
		break;
	case RES_TYPE_OPENDIRDLG:
		rc = guiaShowOpenDirDlg(res);
		break;
	case RES_TYPE_SAVEASFILEDLG:
		rc = guiaShowSaveAsFileDlg(res);
		break;
	case RES_TYPE_FONTDLG:
		rc = guiaShowFontDlg(res);
		break;
	case RES_TYPE_COLORDLG:
		rc = guiaShowColorDlg(res);
		break;
	case RES_TYPE_ALERTDLG:
		rc = guiaShowAlertDlg(res);
		break;
	default:
		return RC_ERROR;
	}
	return(rc);
#else
	return RC_ERROR;
#endif
}

/*
 * hide a resource shown in a window
 * If the resource is not being shown, then quietly ignore this, no error.
 * If the resource is not a menu/dialog/panel/toolbar, has no effect, just returns zero.
 */
INT guireshide(RESHANDLE reshandle)
{
#if OS_WIN32GUI
	RESHANDLE rh, rh1;
	RESOURCE *res;
	WINDOW *win;
	INT retval;
	CHAR *errmsg;

	for (rh = reshandlehdr; rh != NULL && rh != reshandle; rh = (*rh)->nextreshandle);
	if (rh == NULL) return RC_INVALID_HANDLE;
	res = *rh;

	if (res->winshow == NULL) return 0;
	win = *res->winshow;
	switch (res->restype) {
	case RES_TYPE_MENU:
		win->showmenu = NULL;
		retval = guiaHideMenu(res);
		return retval;

	case RES_TYPE_PANEL:
	case RES_TYPE_DIALOG:
		if (win->ctldefault == res->ctldefault) win->ctldefault = NULL;
		if (win->showreshdr == reshandle) win->showreshdr = res->showresnext;
		else {
			for (rh = win->showreshdr; rh != NULL && rh != reshandle; rh = (*rh)->showresnext) rh1 = rh;
			if (rh == NULL) return RC_ERROR;
			(*rh1)->showresnext = (*rh)->showresnext;
		}
		/* Find and remember a user defined default pushbutton on this window */
		win->ctlorgdflt = win->ctldefault = NULL;
		for (rh = win->showreshdr; rh != NULL; rh = (*rh)->showresnext) {
			if ((*rh)->ctlorgdflt == 0) continue;
			win->ctldefault = win->ctlorgdflt = (CONTROL *) guiMemToPtr((*rh)->content) + (*rh)->ctlorgdflt - 1;
			break;
		}
		clearLastErrorMessage();
		retval = guiaHideControls(res);
		errmsg = guiaGetLastErrorMessage();
		if (strlen(errmsg) != 0) { setLastErrorMessage(errmsg); }
		return retval;

	case RES_TYPE_TOOLBAR:
		win->showtoolbar = NULL;
		retval = guiaHideToolbar(res);
		return retval;
	case RES_TYPE_POPUPMENU:
	case RES_TYPE_OPENFILEDLG:
	case RES_TYPE_SAVEASFILEDLG:
	case RES_TYPE_FONTDLG:
	case RES_TYPE_COLORDLG:
		return 0;
	}
	return RC_ERROR;
#else
	return -1;
#endif
}

INT guigetcontrolsize(RESHANDLE reshandle, INT useritem, INT *width, INT *height)
{
#if OS_WIN32GUI
	CONTROL *control;
	RESHANDLE rh;
	RESOURCE *res;
	INT i1, i2;
	INT rc;

	*width = 0;
	*height = 0;
	for (rh = reshandlehdr; rh != NULL && rh != reshandle; rh = (*rh)->nextreshandle);
	if (rh == NULL) return RC_INVALID_HANDLE;
	res = *rh;
	if (res->winshow == NULL) return RC_ERROR;
	control = (CONTROL *) guiMemToPtr(res->content);
	for (i1 = 0; i1 < res->entrycount && control->useritem != useritem; i1++, control++);
	if (i1 >= res->entrycount) return RC_ERROR;
	rc = guiaGetControlSize(control, &i1, &i2);
	*width = (i1 * scaley) / scalex;
	*height = (i2 * scaley) / scalex;
	return(rc);
#else
	return -1;
#endif
}

INT guipixcreate(INT bpp, INT h, INT v, PIXHANDLE *pixhandleptr)  /* create a pixel map */
{
	PIXHANDLE ph;
	PIXMAP *pix;
#if OS_WIN32GUI
	INT rc;
//#else
//	INT size;
#endif

	switch (bpp) {
	case 0:
#if OS_WIN32GUI
#else
		bpp = 24; // @suppress("No break at end of case")
#endif
	case 1:
	case 4:
	case 8:
	case 16:
	case 24:
		break;
	default:
		return RC_INVALID_VALUE;
	}

	ph = (PIXHANDLE) memalloc(sizeof(PIXMAP), MEMFLAGS_ZEROFILL);
	if (ph == NULL) return RC_NO_MEM;
	pix = *ph;
	pix->bpp = (UCHAR) bpp;
	pix->hsize = (USHORT) h;
	pix->vsize = (USHORT) v;
	pix->thispixhandle = ph;
#if OS_WIN32GUI
	if (rc = guiaCreatePixmap(ph)) {
		memfree((UCHAR **) ph);
		return(rc);
	}
#else
	if (bpp > 1) pix->bpp = 24;
	pix->bufsize = (((h * pix->bpp + 0x1F) & ~0x1F) >> 3) * v;  /* bytes per row times number of rows */
	(*ph)->pixbuf = memalloc(pix->bufsize, 0);
	pix = *ph;
	if (pix->pixbuf == NULL) {
		memfree((UCHAR **) ph);
		return RC_NO_MEM;
	}
#endif

	/* Insert this PIXHANDLE at the head of the linked list */
	pvistart();
	pix->nextpixhandle = pixhandlehdr;
	*pixhandleptr = pixhandlehdr = ph;
	pviend();
	return 0;
}


INT guipixdestroy(PIXHANDLE pixhandle)  /* destroy a pixel map */
{
	PIXHANDLE ph, ph1;
	INT rc = 0;

	for (ph = pixhandlehdr; ph != NULL && ph != pixhandle; ph = (*ph)->nextpixhandle) ph1 = ph;
	if (ph == NULL) return RC_INVALID_HANDLE;
#if OS_WIN32GUI
	if ((*ph)->winshow != NULL) guipixhide(ph);
#endif

	/* Remove this PIXHANDLE from the linked list */
	pvistart();
	if (ph == pixhandlehdr) pixhandlehdr = (*ph)->nextpixhandle;
	else (*ph1)->nextpixhandle = (*ph)->nextpixhandle;
	pviend();

#if OS_WIN32GUI
	rc = guiaDestroyPixmap(ph);
#else
	memfree((*ph)->pixbuf);
#endif
	memfree((UCHAR **) ph);
	return rc;
}

INT guipixshow(PIXHANDLE pixhandle, WINHANDLE winhandle, INT h, INT v)  /* show a pixel map in a window */
{
#if OS_WIN32GUI
	PIXHANDLE ph, ph1;
	WINHANDLE wh;
	WINDOW *win;
	PIXMAP *pix;
	INT rc;

	for (ph = pixhandlehdr; ph != NULL && ph != pixhandle; ph = (*ph)->nextpixhandle);
	for (wh = winhandlehdr; wh != NULL && wh != winhandle; wh = (*wh)->nextwinhandle);
	if (ph == NULL || wh == NULL) return RC_INVALID_HANDLE;
	if ((*ph)->winshow != NULL) guipixhide(ph);
	/**
	 * We must lock the following code where the pix linked list is altered.
	 * Over in guiawin, in the window procedure, in response to a WM_PAINT message,
	 * this list is traversed
	 */
	pvistart();
	pix = *ph;
	win = *wh;
	pix->hshow = (USHORT) h;
	pix->vshow = (USHORT) v;
	pix->showpixnext = NULL;
	if (win->showpixhdr == NULL) win->showpixhdr = ph;
	else {
		for (ph1 = win->showpixhdr; (*ph1)->showpixnext != NULL; ph1 = (*ph1)->showpixnext);
		(*ph1)->showpixnext = ph;
	}
	pix->winshow = wh;
	pviend();

	rc = guiaShowPixmap(ph);
	return(rc);
#else
	return RC_ERROR;
#endif
}

/**
 *  hide a pixel map
 *  Removes the PIXHANDLE from its WINDOWs linked list of shown images
 */
INT guipixhide(PIXHANDLE pixhandle)
{
#if OS_WIN32GUI
	PIXHANDLE ph, ph1;
	PIXMAP *pix;
	WINDOW *win;
	INT retcode;

	for (ph = pixhandlehdr; ph != NULL && ph != pixhandle; ph = (*ph)->nextpixhandle);
	if (ph == NULL) return RC_INVALID_HANDLE;
	pix = *ph;
	if (pix->winshow == NULL) return 0;
	/**
	 * We must lock the following code where the pix linked list is altered.
	 * Over in guiawin, in the window procedure, in response to a WM_PAINT message,
	 * this list is traversed
	 */
	pvistart();
	win = *pix->winshow;
	if (win->showpixhdr == ph) win->showpixhdr = pix->showpixnext;
	else {
		for (ph = win->showpixhdr; ph != NULL && ph != pixhandle; ph = (*ph)->showpixnext) ph1 = ph;
		if (ph == NULL) {
			pviend();
			return RC_ERROR;
		}
		(*ph1)->showpixnext = pix->showpixnext;
	}
	pviend();

	retcode = guiaHidePixmap(ph);
	(*ph)->winshow = NULL;
	return(retcode);
#else
	return RC_ERROR;
#endif
}

INT guijpgres(UCHAR *buffer, INT *hres, INT *vres)
{
	return(guiaJpgRes(buffer, hres, vres));
}

INT guitifres(INT index, UCHAR *buffer, INT *hres, INT *vres)
{
	return(guiaTifRes(index, buffer, hres, vres));
}

INT guijpgsize(UCHAR *buffer, INT *hsize, INT *vsize)
{
	return(guiaJpgSize(buffer, hsize, vsize));
}

INT guigifsize(UCHAR *buffer, INT *hsize, INT *vsize)
{
	return(guiaGifSize(buffer, hsize, vsize));
}

INT guitifsize(INT index, UCHAR *buffer, INT *hsize, INT *vsize)
{
	return(guiaTifSize(index, buffer, hsize, vsize));
}

INT guijpgbpp(UCHAR *buffer, INT *bpp)
{
	return(guiaJpgBPP(buffer, bpp));
}

INT guigifbpp(UCHAR *buffer, INT *bpp)
{
	return(guiaGifBPP(buffer, bpp));
}

INT guitifbpp(INT index, UCHAR *buffer, INT *bpp)
{
	return(guiaTifBPP(index, buffer, bpp));
}

INT guitifcount(UCHAR *buffer, INT *count)
{
	return(guiaTifImageCount(buffer, count));
}

INT guipixtotif(PIXHANDLE pixhandle, UCHAR *buffer, INT *pbufsize)
{
	PIXHANDLE ph;

	for (ph = pixhandlehdr; ph != NULL && ph != pixhandle; ph = (*ph)->nextpixhandle);
	if (ph == NULL) {
		return RC_INVALID_HANDLE;
	}
	return(guiaPixmapToTif(*ph, buffer, pbufsize));
}

INT guitiftopix(PIXHANDLE pixhandle, UCHAR **bufhandle)
{
	PIXHANDLE ph;

	for (ph = pixhandlehdr; ph != NULL && ph != pixhandle; ph = (*ph)->nextpixhandle);
	if (ph == NULL) return RC_INVALID_HANDLE;
	return(guiaTifToPixmap(ph, bufhandle));
}

INT guijpgtopix(PIXHANDLE pixhandle, UCHAR **bufhandle)
{
	PIXHANDLE ph;

	for (ph = pixhandlehdr; ph != NULL && ph != pixhandle; ph = (*ph)->nextpixhandle);
	if (ph == NULL) return RC_INVALID_HANDLE;
	return(guiaJpgToPixmap(ph, bufhandle));
}

INT guigiftopix(PIXHANDLE pixhandle, UCHAR **bufhandle)
{
	PIXHANDLE ph;

	for (ph = pixhandlehdr; ph != NULL && ph != pixhandle; ph = (*ph)->nextpixhandle);
	if (ph == NULL) return RC_INVALID_HANDLE;
	return(guiaGifToPixmap(ph, bufhandle));
}

INT guipixdrawstart(PIXHANDLE pixhandle)
{
	PIXHANDLE ph;

	if (drawpixhandle != NULL) return RC_ERROR;
	for (ph = pixhandlehdr; ph != NULL && ph != pixhandle; ph = (*ph)->nextpixhandle);
	if (ph == NULL) return RC_INVALID_HANDLE;
	drawpixhandle = ph;
	drawpixptr = *drawpixhandle;
	guiaDrawStart(ph);

#if OS_WIN32GUI
	guiaSetPen();  /* set pen to use values from previous draw */
#endif
	return 0;
}

void guipixdrawend()
{
	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) {
		guiaDrawEnd();
		drawpixhandle = NULL;
	}
}

void guipixdrawpos(INT h, INT v)
{
	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) {
		ZPOINTH(drawpixptr->pos1) = h;
		ZPOINTV(drawpixptr->pos1) = v;
	}
}

/**
 * Expects zero-based value
 */
void guipixdrawhpos(INT h)
{
	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) ZPOINTH(drawpixptr->pos1) = h;
}

/**
 * Expects zero-based value
 */
void guipixdrawvpos(INT v)
{
	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) ZPOINTV(drawpixptr->pos1) = v;
}

void guipixdrawhadj(INT ha)
{
	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) ZPOINTH(drawpixptr->pos1) += ha;
}

void guipixdrawvadj(INT va)
{
	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) ZPOINTV(drawpixptr->pos1) += va;
}

void guipixdrawcolor(INT red, INT green, INT blue)
{
	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) {
		drawpixptr->pencolor = (blue << 16) + (green << 8) + red;
#if OS_WIN32GUI
		guiaSetPen();
#endif
	}
}

void guipixdrawlinewidth(INT width)
{
#if OS_WIN32GUI
	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) {
		drawpixptr->penwidth = width;
		guiaSetPen();
	}
#endif
}

void guipixdrawlinetype(INT type)
{
#if OS_WIN32GUI
	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) {
		drawpixptr->pentype = type;
		guiaSetPen();
	}
#endif
}

void guipixdrawreplace(INT32 color1, INT32 color2)
{
#if OS_WIN32GUI
	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) {
		guiaDrawReplace(color1, color2);
	}
#endif
}

void guipixdrawerase()
{
	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) {
		ZPOINTH(drawpixptr->pos1) = ZPOINTV(drawpixptr->pos1) = 0;
		ZPOINTH(drawpixptr->pos2) = drawpixptr->hsize - 1;
		ZPOINTV(drawpixptr->pos2) = drawpixptr->vsize - 1;
		guiaDrawRect();
		ZPOINTH(drawpixptr->pos1) = ZPOINTV(drawpixptr->pos1) = 0;
	}
}

void guipixdrawdot(INT diameter)
{
	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) {
#if OS_WIN32GUI
		if (diameter == 0) guiaSetPixel();
		else {
			ZPOINTH(drawpixptr->pos2) = diameter;
			guiaDrawBigDot();
		}
#endif
		if (diameter == 0) guiaSetPixel();
	}
}

void guipixdrawcircle(INT diameter)
{
#if OS_WIN32GUI
	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) {
		ZPOINTH(drawpixptr->pos2) = diameter;
		guiaDrawCircle();
	}
#endif
}

/**
 * Expects zero-based values
 */
void guipixdrawline(INT h, INT v)
{
#if OS_WIN32GUI
	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) {
		ZPOINTH(drawpixptr->pos2) = h;
		ZPOINTV(drawpixptr->pos2) = v;
		guiaDrawLine();
		ZPOINTH(drawpixptr->pos1) = h;
		ZPOINTV(drawpixptr->pos1) = v;
	}
#endif
}

/**
 * Expects zero-based values
 */
void guipixdrawrectangle(INT h, INT v)
{
	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) {
		ZPOINTH(drawpixptr->pos2) = h;
		ZPOINTV(drawpixptr->pos2) = v;
		guiaDrawRect();
	}
}

/**
 * Expects zero-based values
 */
void guipixdrawbox(INT h, INT v)
{
#if OS_WIN32GUI
	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) {
		ZPOINTH(drawpixptr->pos2) = h;
		ZPOINTV(drawpixptr->pos2) = v;
		guiaDrawBox();
	}
#endif
}

/**
 * Expects zero-based values
 */
void guipixdrawtriangle(INT h1, INT v1, INT h2, INT v2)
{
#if OS_WIN32GUI
	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) {
		ZPOINTH(drawpixptr->pos2) = h1;
		ZPOINTV(drawpixptr->pos2) = v1;
		ZPOINTH(drawpixptr->pos3) = h2;
		ZPOINTV(drawpixptr->pos3) = v2;
		guiaDrawTriangle();
	}
#endif
}

void guipixdrawfont(UCHAR *font, INT length)
{
#if OS_WIN32GUI
	ZHANDLE texthandle;
	CHAR work[64];

	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) {
		texthandle = guiAllocString(font, length);
		if (texthandle == NULL) {
			sprintf(work, "guiAllocString failed in %s", __FUNCTION__);
			guiaErrorMsg(work, length);
			return;
		}
		guiaSetFont(texthandle);
		guiFreeMem(texthandle);
	}
#endif
}

void guipixdrawatext(UCHAR *text, INT length, INT angle)
{
#if OS_WIN32GUI
	ZHANDLE texthandle;
	CHAR work[64];

	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) {
		texthandle = guiAllocString(text, length);
		if (texthandle == NULL) {
			sprintf(work, "guiAllocString failed in %s", __FUNCTION__);
			guiaErrorMsg(work, length);
			return;
		}
		guiaDrawAngledText(texthandle, angle);
		guiFreeMem(texthandle);
	}
#endif
}

void guipixdrawtext(UCHAR *text, INT length, UINT align)
{
#if OS_WIN32GUI
	ZHANDLE texthandle;
	CHAR work[64];

	if (drawpixhandle != NULL && *drawpixhandle == drawpixptr) {
		texthandle = guiAllocString(text, length);
		if (texthandle == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiAllocString failed in %s", __FUNCTION__);
			guiaErrorMsg(work, length);
			return;
		}
		guiaDrawText(texthandle, align);
		guiFreeMem(texthandle);
	}
#endif
}

/**
 * Draw an image on another image without
 * stretch or clip
 */
void guipixdrawcopyimage(PIXHANDLE pixhandle)
{
#if OS_WIN32GUI
	PIXHANDLE ph;

	if (drawpixhandle == NULL || *drawpixhandle != drawpixptr) return;
	for (ph = pixhandlehdr; ph != NULL && ph != pixhandle; ph = (*ph)->nextpixhandle);
	if (ph == NULL) return;
	ZPOINTH(drawpixptr->pos2) = (*ph)->hsize;
	ZPOINTV(drawpixptr->pos2) = (*ph)->vsize;
	guiaStretchCopyPixmap(*ph, FALSE);
#endif
}

void guipixdrawclipimage(PIXHANDLE pixhandle, INT h1, INT v1, INT h2, INT v2)
{
#if OS_WIN32GUI
	PIXHANDLE ph;

	if (drawpixhandle == NULL || *drawpixhandle != drawpixptr) return;
	for (ph = pixhandlehdr; ph != NULL && ph != pixhandle; ph = (*ph)->nextpixhandle);
	if (ph == NULL) return;
	ZPOINTH(drawpixptr->pos2) = h1;
	ZPOINTV(drawpixptr->pos2) = v1;
	ZPOINTH(drawpixptr->pos3) = h2;
	ZPOINTV(drawpixptr->pos3) = v2;
	guiaClipCopyPixmap(*ph);
#endif
}

void guipixdrawstretchimage(PIXHANDLE pixhandle, INT h, INT v)
{
#if OS_WIN32GUI
	PIXHANDLE ph;

	if (drawpixhandle == NULL || *drawpixhandle != drawpixptr) return;
	for (ph = pixhandlehdr; ph != NULL && ph != pixhandle; ph = (*ph)->nextpixhandle);
	if (ph == NULL) return;
	ZPOINTH(drawpixptr->pos2) = h;
	ZPOINTV(drawpixptr->pos2) = v;
	guiaStretchCopyPixmap(*ph, TRUE);
#endif
}

void guipixdrawirotate(INT angle)
{
#if OS_WIN32GUI
	guiaDrawImageRotationAngle(angle);
#endif
}

/* return the color value of the pixel at the position h, v */
INT guipixgetcolor(PIXHANDLE pixhandle, INT32 *colorptr, INT h, INT v)
{
#if !OS_WIN32GUI
	INT i1;
	UCHAR *ptr;
	PIXMAP *pix;
#endif
	PIXHANDLE ph;

	for (ph = pixhandlehdr; ph != NULL && ph != pixhandle; ph = (*ph)->nextpixhandle);
	if (ph == NULL) return RC_INVALID_HANDLE;
#if OS_WIN32GUI
	guiaGetPixel(*pixhandle, colorptr, h, v);
#else
	pix = *ph;
	ptr = *pix->pixbuf + (((pix->hsize * pix->bpp + 0x1F) & ~0x1F) >> 3) * v;  /* beginning of row */
	if (pix->bpp == 1) {
		ptr += h >> 3; /* move 1 byte for every 8 horizontal pixels */
		if ((i1 = h % 8) == 7 && (*ptr & 0x01)) *colorptr = 0xFFFFFF;
		else if (i1 == 6 && (*ptr & 0x02)) *colorptr = 0xFFFFFF;
		else if (i1 == 5 && (*ptr & 0x04)) *colorptr = 0xFFFFFF;
		else if (i1 == 4 && (*ptr & 0x08)) *colorptr = 0xFFFFFF;
		else if (i1 == 3 && (*ptr & 0x10)) *colorptr = 0xFFFFFF;
		else if (i1 == 2 && (*ptr & 0x20)) *colorptr = 0xFFFFFF;
		else if (i1 == 1 && (*ptr & 0x40)) *colorptr = 0xFFFFFF;
		else if (i1 == 0 && (*ptr & 0x80)) *colorptr = 0xFFFFFF;
		else *colorptr = 0x000000;
	}
	else {
		ptr += h * 3;
		*colorptr = ptr[0] + ((INT) ptr[1] << 8) + ((INT) ptr[2] << 16);
	}
#endif
	return 0;
}

/* return the current drawing position */
INT guipixgetpos(PIXHANDLE pixhandle, INT *h, INT *v)
{
#if OS_WIN32GUI
	PIXHANDLE ph;
	PIXMAP *pix;

	for (ph = pixhandlehdr; ph != NULL && ph != pixhandle; ph = (*ph)->nextpixhandle);
	if (ph == NULL) return RC_INVALID_HANDLE;
	pix = *ph;
	*h = ZPOINTH(pix->pos1);
	*v = ZPOINTV(pix->pos1);
	return 0;
#else
	return -1;
#endif
}

/* return font size info of current draw font */
INT guipixgetfontsize(PIXHANDLE pixhandle, INT *height, INT *width)
{
#if OS_WIN32GUI
	PIXHANDLE ph;
	PIXMAP *pix;

	for (ph = pixhandlehdr; ph != NULL && ph != pixhandle; ph = (*ph)->nextpixhandle);
	if (ph == NULL) return RC_INVALID_HANDLE;
	pix = *ph;
	*height = pix->fontheight;
	*width = pix->fontwidth;
	return 0;
#else
	return -1;
#endif
}

/*
 * get text len from the clipboard - bytes
 */
INT guicbdgettextlen(size_t *cbTextLen)
{
#if OS_WIN32GUI
	return(guiaGetCBTextLen(cbTextLen));
#else
	return -1;
#endif
}

/* get text from the clipboard */
INT guicbdgettext(CHAR *text, size_t *cbText)
{
#if OS_WIN32GUI
	return(guiaGetCBText(text, cbText));
#else
	return -1;
#endif
}

/* put text into the clipboard */
INT guicbdputtext(UCHAR *text, INT size)
{
#if OS_WIN32GUI
	return(guiaPutCBText(text, size));
#else
	return -1;
#endif
}

INT guicbdres(INT *hres, INT *vres)
{
#if OS_WIN32GUI
	return(guiaClipboardRes(hres, vres));
#else
	return -1;
#endif
}

INT guicbdsize(INT *hsize, INT *vsize)
{
#if OS_WIN32GUI
	return(guiaClipboardSize(hsize, vsize));
#else
	return -1;
#endif
}

INT guicbdbpp(INT *bpp)
{
#if OS_WIN32GUI
	return(guiaClipboardBPP(bpp));
#else
	return -1;
#endif
}

/* get a pixel map from the clipboard */
INT guicbdgetpixmap(PIXHANDLE pixhandle)
{
#if OS_WIN32GUI
	PIXHANDLE ph;

	for (ph = pixhandlehdr; ph != NULL && ph != pixhandle; ph = (*ph)->nextpixhandle);
	if (ph == NULL) return RC_INVALID_HANDLE;
	return(guiaGetCBPixmap(*ph));
#else
	return -1;
#endif
}

/* put a pixel map into the clipboard */
INT guicbdputpixmap(PIXHANDLE pixhandle)
{
#if OS_WIN32GUI
	PIXHANDLE ph;

	for (ph = pixhandlehdr; ph != NULL && ph != pixhandle; ph = (*ph)->nextpixhandle);
	if (ph == NULL) return RC_INVALID_HANDLE;
	return(guiaPutCBPixmap(*ph));
#else
	return -1;
#endif
}

INT guipngres(UCHAR *buffer, INT *hres, INT *vres)
{
	return(guicPngRes(buffer, hres, vres));
}

INT guipngsize(UCHAR *buffer, INT *hsize, INT *vsize)
{
	return(guicPngSize(buffer, hsize, vsize));
}

INT guipngtopix(PIXHANDLE pixhandle, UCHAR **bufhandle)
{
	PIXHANDLE ph;

	for (ph = pixhandlehdr; ph != NULL && ph != pixhandle; ph = (*ph)->nextpixhandle);
	if (ph == NULL) return RC_INVALID_HANDLE;
	return(guicPngToPixmap(ph, bufhandle));
}

INT guipngbpp(UCHAR *buffer, INT *bpp)
{
	return(guicPngBPP(buffer, bpp));
}

#if OS_WIN32GUI

static INT num5toi(UCHAR *p1, INT *dest)  /* convert 5 characters to integer */
/* return 0 if 5 characters are valid, else return -1 */
{
	INT i1, n1, negative;

	negative = FALSE;
	*dest = n1 = 0;
	for (i1 = 0; i1 < 5 && p1[i1] == ' '; i1++);
	if (i1 == 5) return(-1);
	if (p1[i1] == '-') {
		negative = TRUE;
		if (++i1 == 5) return(-1);
	}
	while (i1 < 5 && isdigit(p1[i1])) n1 = n1 * 10 + p1[i1++] - '0';
	if (i1 < 5) return(-1);
	if (negative) n1 = (-n1);
	*dest = n1;
	return 0;

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

/*
 * parse fnc into useritem and return GUI_<cmd>
 * If command is invalid then return -1
 * fnc is of the form  "STRINGnnnnn", where STRING is one to 20 characters
 * of a command name and nnnnn is a 1 to 5 digit number representing the item number
 *
 * For tables, the format of the acceptable sting is modified.
 * It can be STRINGnnnnn[nnnnn]
 * Where the number in square brackets is a one-based row number. The item number in this case
 * would be that of a column in a table.
 *
 */
static INT parsefnc(UCHAR *fnc, INT fnclen, INT *useritem, INT *subuseritem)
{
	INT cmdval, pos;
	CHAR cmd[20];

	/* Parse command STRING into cmd */
	for (pos = 0; pos < fnclen && fnc[pos] != ' ' && !isdigit(fnc[pos]); pos++) {
		cmd[pos] = toupper(fnc[pos]);
	}
	cmd[pos] = 0;

	while (pos < fnclen && fnc[pos] == ' ') pos++;  /* skip leading blanks */

	/* convert user item number to an integer */
	*useritem = 0;
	while (pos < fnclen && isdigit(fnc[pos])) {
		*useritem = *useritem * 10 + fnc[pos] - '0';
		pos++;
	}

	/* Allow white space between the item number and a left bracket */
	while (pos < fnclen && isspace(fnc[pos])) pos++;

	/* If there is a subuseritem, convert it */
	if (pos < fnclen && fnc[pos] == '[') {
		*subuseritem = 0;
		do pos++; while (pos < fnclen && isspace(fnc[pos]));
		while (pos < fnclen && isdigit(fnc[pos])) {
			*subuseritem = *subuseritem * 10 + fnc[pos] - '0';
			pos++;
		}
	}

	/* find cmd STRING in symtable */
	for (cmdval = 0; symtable[cmdval].name != NULL; cmdval++) {
		if (!strcmp(cmd, symtable[cmdval].name)) {
			return(symtable[cmdval].value);
		}
	}

	return RC_ERROR;  /* not found */
}
#endif

/**
 * Compare src and dest converting each character using toupper
 * Return 0 if all match, 1 otherwise
 */
INT guimemicmp(void *src, void *dest, INT len)
{
	while(len--) if (toupper(((CHAR *) src)[len]) != toupper(((CHAR *)dest)[len])) return(1);
	return 0;
}

/*
 * Allocate local memory (malloc) and return its handle.
 * Note that the heap memory block allocated has sentinels.
 * Two INTs at the beginning and one INT at the end.
 */
ZHANDLE guiAllocMem(INT byteCount)
{
	BYTE *p1;

	/* round size up to an even sizeof(INT) */
	byteCount = (((byteCount - 1) / sizeof(INT)) + 1) * sizeof(INT);
	p1 = malloc(byteCount + 3 * sizeof(INT));
	if (p1 == NULL) {
		return NULL;
	}
	*((INT *) p1) = MEMBLKHDR - byteCount;
	*((INT *) (p1 + sizeof(INT))) = byteCount;
	*((INT *) (p1 + 2 * sizeof(INT) + byteCount)) = MEMBLKEND;
	if (byteCount > memmaxsize) memmaxsize = byteCount;
	return(p1 + 2 * sizeof(INT));
}

/*
 * allocate a string in local memory (malloc) and return its handle,
 * adding a '\0'
 * Note that the heap memory block allocated has sentinels.
 * Two INTs at the beginning and one INT at the end.
 */
ZHANDLE guiAllocString(BYTE *text, INT textlen)
{
	BYTE *p1;

	p1 = guiAllocMem(textlen + 1);
	if (p1 == NULL) {
		return(NULL);
	}
	memcpy(p1, text, textlen);
	p1[textlen] = '\0';
	return(p1);
}

/*
 * convert a ZHandle into a pointer
 */
void *guiMemToPtr(ZHANDLE memptr)
{
#if OS_WIN32GUI
	if (memptr == NULL) guiaErrorMsg("guiMemToPtr() failure", 1);
#endif
	return(memptr);
}


#if OS_WIN32

/**
 * Takes a standard DB/C font string and breaks it up.
 * Modifies elements in the LOGFONT.
 * May modify lfHeight, lfItalic, lfWeight, lfUnderline,
 * lfOutPrecision, lfClipPrecision, lfQuality, lfPitchAndFamily
 * lfFaceName
 *
 * Return 0 if no errors.
 */
INT parseFontString(CHAR* font, LOGFONT* lf, HDC hdc) {
	INT worklen, pos, i1, size;
	CHAR *ptr, work[256];
	LOGFONT lfwork;

	if (!*font) return 0;
	for (worklen = 0, ptr = font; *ptr; ptr++) work[worklen++] = toupper(*ptr);
	work[worklen] = '\0';

	if (work[0] == '(') pos = 0;
	else {
		for (pos = 0; work[pos] && work[pos] != '('; pos++);  /* find end of string or '(' */
		for (i1 = pos; work[i1 - 1] == ' '; i1--);	/* cutoff trailing blanks on font */
		work[i1] = '\0';
	}

	if (pos != worklen) {  /* '(' was found in original font name */
		ptr = work + pos + 1;
		while (*ptr == ' ') ptr++;
		while (*ptr != ')') {
			if (!*ptr) return -2;
			if (isdigit(*ptr)) {
				size = (int)strtod(ptr, &ptr);
				//for (size = *ptr++ - '0'; isdigit(*ptr); ptr++) size = size * 10 + *ptr - '0';
				if (!*ptr || !size) return -3;
				lf->lfHeight = size;
			}
			else if (!strncmp(ptr, "PLAIN", 5)) {
				lf->lfWeight = FW_NORMAL;
				lf->lfItalic = lf->lfUnderline = 0;
				ptr += 5;
			}
			else if (!strncmp(ptr, "BOLD", 4)) {
				lf->lfWeight = FW_BOLD;
				ptr += 4;
			}
			else if (!strncmp(ptr, "NOBOLD", 6)) {
				lf->lfWeight = FW_NORMAL;
				ptr += 6;
			}
			else if (!strncmp(ptr, "ITALIC", 6)) {
				lf->lfItalic = 1;
				ptr += 6;
			}
			else if (!strncmp(ptr, "NOITALIC", 8)) {
				lf->lfItalic = 0;
				ptr += 8;
			}
			else if (!strncmp(ptr, "UNDERLINE", 9)) {
				lf->lfUnderline = 1;
				ptr += 9;
			}
			else if (!strncmp(ptr, "NOUNDERLINE", 11)) {
				lf->lfUnderline = 0;
				ptr += 11;
			}
			else return -3;

			while (*ptr == ' ') ptr++;
			if (*ptr == ',') while (*++ptr == ' ');
			else if (*ptr != ')') return -3;
		}
	}
	if (work[0] != '(') {
		lfwork = *lf;
		lfwork.lfCharSet = ANSI_CHARSET;
		lfwork.lfFaceName[0] = '\0';
		if (!strcmp(work, "SYSTEM"))
			lfwork.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
		else if (!strcmp(work, "MONACO")) {
			lfwork.lfPitchAndFamily = FIXED_PITCH | FF_SWISS;
			lfwork.lfHeight++;
		}
		else if (!strcmp(work, "COURIER")) {
			lfwork.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
			//strcpy(lfwork.lfFaceName, "Courier");
		}
		else if (!strcmp(work, "TIMES")) {
			lfwork.lfPitchAndFamily = VARIABLE_PITCH | FF_ROMAN;
			//strcpy(lfwork.lfFaceName, "Times");
		}
		else if (!strcmp(work, "HELVETICA")) {
			lfwork.lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
			//strcpy(lfwork.lfFaceName, "Helvetica");
		}
		else {  /* native font specification */
			lfwork.lfPitchAndFamily = 0;
			lfwork.lfCharSet = DEFAULT_CHARSET;
			strncpy(lfwork.lfFaceName, work, LF_FACESIZE);
			lfwork.lfFaceName[LF_FACESIZE - 1] = '\0';
			i1 = EnumFontFamiliesEx(hdc, &lfwork, (FONTENUMPROC) EnumFontFamExProc, (LPARAM) lf, 0);
			if (i1) lfwork = *lf; /* font not found, so ignore */
		}
		*lf = lfwork;
	}
	return 0;
}

/*
 * Window's callback function from EnumFontFamiliesEx() call
 */
static int CALLBACK EnumFontFamExProc(ENUMLOGFONTEX *elf,
	NEWTEXTMETRICEX *ntm, DWORD fonttype, LPARAM data)
{
	LOGFONT *lf;

	lf = (LOGFONT *) data;
	switch (fonttype & (TRUETYPE_FONTTYPE | RASTER_FONTTYPE)) {
	case TRUETYPE_FONTTYPE:  /* 4 */
	case 0:  /* vector */
		lf->lfOutPrecision = elf->elfLogFont.lfOutPrecision;
		lf->lfClipPrecision = elf->elfLogFont.lfClipPrecision;
		lf->lfQuality = elf->elfLogFont.lfQuality;
		lf->lfPitchAndFamily = elf->elfLogFont.lfPitchAndFamily;
		strcpy(lf->lfFaceName, elf->elfLogFont.lfFaceName);
		/* we can make whatever type we want from this font */
		return FALSE;  /* stop enumeration */
	case RASTER_FONTTYPE:  /* 1 */
		/* if raster font is device supplied then */
		/* we can make a bold, italic, etc font from plain one */
		lf->lfOutPrecision = elf->elfLogFont.lfOutPrecision;
		lf->lfClipPrecision = elf->elfLogFont.lfClipPrecision;
		lf->lfQuality = elf->elfLogFont.lfQuality;
		lf->lfPitchAndFamily = elf->elfLogFont.lfPitchAndFamily;
		strcpy(lf->lfFaceName, elf->elfLogFont.lfFaceName);
		return FALSE; /* stop enumeration */
	}
	return TRUE;
}

#endif


/*
 * change the size of a local memory block
 */
ZHANDLE guiReallocMem(ZHANDLE memptr, INT size)
{
	INT allocsize;

	if (memptr == NULL) {
#if OS_WIN32GUI
		guiaErrorMsg("guiReallocMem() failure", 1);
#endif
		return(NULL);
	}
	memptr -= 2 * sizeof(INT);
	/* round size up to an even sizeof(INT) */
	size = (((size - 1) / sizeof(INT)) + 1) * sizeof(INT);
	allocsize = *((INT *) (memptr + sizeof(INT)));
	if (allocsize > memmaxsize || *((INT *) memptr) + allocsize != MEMBLKHDR) {
#if OS_WIN32GUI
		guiaErrorMsg("guiReallocMem() failure", 2);
#endif
		return(NULL);
	}
	if (*((INT *) (memptr + 2 * sizeof(INT) + allocsize)) != MEMBLKEND) {
#if OS_WIN32GUI
		guiaErrorMsg("guiReallocMem() failure", 3);
#endif
		return(NULL);
	}
	/* don't bother with shrinkages of less than 20 bytes */
	if (size < allocsize && allocsize - size < 20) return(memptr + 2 * sizeof(INT));
	memptr = realloc(memptr, size + 3 * sizeof(INT));
	if (memptr == NULL) {
		return NULL;
	}
	*((INT *) memptr) = MEMBLKHDR - size;
	*((INT *) (memptr + sizeof(INT))) = size;
	*((INT *) (memptr + 2 * sizeof(INT) + size)) = MEMBLKEND;
	if (size > memmaxsize) memmaxsize = size;
	return(memptr + 2 * sizeof(INT));
}

/*
 * Free a block of local heap memory
 * Return RC_ERROR if the sentinels have been damaged
 */
INT guiFreeMem(ZHANDLE memptr)
{
	INT allocsize;

	if (memptr == NULL) {
#if OS_WIN32GUI
		guiaErrorMsg("guiFreeMem() failure", 1);
#endif
		return RC_ERROR;
	}
	memptr -= 2 * sizeof(INT);
	allocsize = *((INT *) (memptr + sizeof(INT)));
	if (allocsize > memmaxsize || *((INT *) memptr) + allocsize != MEMBLKHDR) {
#if OS_WIN32GUI
		guiaErrorMsg("guiFreeMem() failure", 2);
#endif
		return RC_ERROR;
	}
	if (*((INT *) (memptr + 2 * sizeof(INT) + allocsize)) != MEMBLKEND) {
#if OS_WIN32GUI
		guiaErrorMsg("guiFreeMem() failure", 3);
#endif
		return RC_ERROR;
	}
	free(memptr);
	return 0;
}

/*
 * Lock a block of local memory and return a pointer
 * Checks the sentinels for corruption
 */
void *guiLockMem(ZHANDLE memptr)
{
	INT allocsize;
	BYTE *p1;

	if (memptr == NULL) {
#if OS_WIN32GUI
		guiaErrorMsg("guiLockMem() failure", 1);
#endif
		return NULL;
	}
	p1 = memptr - 2 * sizeof(INT);
	allocsize = *((INT*) (p1 + sizeof(INT)));
	if (allocsize > memmaxsize || *((INT*) p1) + allocsize != MEMBLKHDR) {
#if OS_WIN32GUI
		guiaErrorMsg("guiLockMem() failure", 2);
#endif
		return NULL;
	}
	if (*((INT*) (p1 + 2 * sizeof(INT) + allocsize)) != MEMBLKEND) {
#if OS_WIN32GUI
		guiaErrorMsg("guiLockMem() failure", 3);
#endif
		return(NULL);
	}
	return(memptr);
}

/*
 * Unlock a block of local memory
 * Checks the sentinels for corruption
 */
void guiUnlockMem(ZHANDLE memptr)
{
	INT allocsize;

	if (memptr == NULL) {
#if OS_WIN32GUI
		guiaErrorMsg("guiUnlockMem() failure", 1);
#endif
		return;
	}
	memptr -= 2 * sizeof(INT);
	allocsize = *((INT *) (memptr + sizeof(INT)));
	if (allocsize > memmaxsize || *((INT *) memptr) + allocsize != MEMBLKHDR) {
#if OS_WIN32GUI
		guiaErrorMsg("guiaUnlockMem() failure", 2);
#endif
		return;
	}
	if (*((INT *) (memptr + 2 * sizeof(INT) + allocsize)) != MEMBLKEND) {
#if OS_WIN32GUI
		guiaErrorMsg("guiUnlockMem() failure", 3);
#endif
	}
}

#if OS_WIN32GUI
static void clearLastErrorMessage() {
	lastErrorMessage[0] = '\0';
}

static void setLastErrorMessage(CHAR *msg) {
	strcpy_s(lastErrorMessage, ARRAYSIZE(lastErrorMessage), msg);
}

CHAR* guiGetLastErrorMessage () {
	return lastErrorMessage;
}
#else
CHAR* guiGetLastErrorMessage () {
	return "";
}
#endif
