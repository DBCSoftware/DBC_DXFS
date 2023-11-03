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

#define GUIAWIN2
#define INC_STDIO
#define INC_STDLIB
#define INC_CTYPE
#include "includes.h"
#include "base.h"
#include "dbc.h"
#include "gui.h"
#include "guix.h"
#include <windowsx.h>

static WNDCLASS wndclass;
static CHAR work[128];
static COLORREF black = RGB(0, 0, 0);
static COLORREF white = RGB(255, 255, 255);

/***********************************************************************************************************************
 *
 * Popbox
 *
 */

#define PBSTATESTRUCT_OFFSET  0

typedef struct tagPopboxState {
	CHAR *text;		/* with a trailing null */
	INT textHeight;
	INT textlen;		/* Not including the trailing null */
	BOOL buttonDown;
	BOOL inTable;
} POPBOXSTATE, *PPOPBOXSTATE;

static void moveCellsVert(HWND hwnd, PTABLESTATE tblState, int amount);
static void PBDrawText(HWND hwnd, HDC dc, PPOPBOXSTATE controlState, CHAR *text, LONG xpos, RECT rect);
static void PBDrawFocusRect(HDC dc, RECT rect);
static void PBSetButtonState(HWND hwnd, BOOL bDown);
static void PBDrawUpArrow(HDC dc, RECT rect, BOOL enabled);
static LRESULT PB_OnCreate(HWND hwnd, LPCREATESTRUCT cs);
static void PB_OnKey(HWND hwnd, UINT vk, BOOL bKeyDown, int cRepeat, UINT flags);
static void PB_OnLButtonDown(HWND hwnd, BOOL bDblClk, int x, int y, UINT uiKeyFlags);
static LRESULT PB_OnPaint(HWND hwnd);
static BOOL PBSetText(HWND hwnd, LPCSTR text);
LRESULT CALLBACK PopboxProc (HWND, UINT, WPARAM, LPARAM);
static INT buttonWidth = 13;
static COLORREF graytext;

BOOL isTableEnabled(HWND hwnd) {
	HWND wrkHwnd = hwnd;
	while (GetClassLong(wrkHwnd, GCW_ATOM) != TableClassAtom) {
		wrkHwnd = GetParent(wrkHwnd);
	}
	return IsWindowEnabled(wrkHwnd);
}

ATOM guiaRegisterPopbox(HINSTANCE hInstance)
{
	ATOM returnvalue;
	if (graytext == (COLORREF) 0) graytext = GetSysColor(COLOR_GRAYTEXT);

	wndclass.style         = CS_OWNDC;
	wndclass.lpfnWndProc   = PopboxProc ;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = sizeof(LONG_PTR);
	wndclass.hInstance     = hInstance ;
	wndclass.hIcon         = NULL;
	wndclass.hCursor       = LoadCursor (NULL, IDC_ARROW) ;
	wndclass.hbrBackground = NULL;
	wndclass.lpszMenuName  = NULL ;
	wndclass.lpszClassName = szPopboxClassName;
	returnvalue = RegisterClass(&wndclass);
	if (!returnvalue) {
		sprintf(work, "%s failed, e=%d", __FUNCTION__, (int) GetLastError());
		MessageBox (NULL, work, szPopboxClassName, MB_ICONERROR);
	}
	return returnvalue;
}

LRESULT CALLBACK PopboxProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC dc;
	RECT rect;
	LPWINDOWPOS winpos;
	PPOPBOXSTATE controlState;
	TEXTMETRIC tm;
	ZHANDLE zh;
	CHAR work[128];

	switch (message) {

	HANDLE_MSG(hwnd, WM_CREATE, PB_OnCreate);
	HANDLE_MSG(hwnd, WM_KEYDOWN, PB_OnKey);
	HANDLE_MSG(hwnd, WM_KEYUP, PB_OnKey);
	HANDLE_MSG(hwnd, WM_LBUTTONDOWN, PB_OnLButtonDown);
	HANDLE_MSG(hwnd, WM_PAINT, PB_OnPaint);

	case WM_DESTROY:
		zh = (ZHANDLE) GetWindowLongPtr(hwnd, PBSTATESTRUCT_OFFSET);
		controlState = (PPOPBOXSTATE) guiLockMem(zh);
		if (controlState == NULL) {
			sprintf(work, "guiLockMem returned null in %s, WM_DESTROY", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		if (controlState->text != NULL) free(controlState->text);
		guiFreeMem(zh);
		SetWindowLongPtr(hwnd, PBSTATESTRUCT_OFFSET, (LONG_PTR) NULL);
		return 0;

	case WM_LBUTTONUP:
		zh = (ZHANDLE) GetWindowLongPtr(hwnd, PBSTATESTRUCT_OFFSET);
		controlState = (PPOPBOXSTATE) guiLockMem(zh);
		if (controlState == NULL) {
			sprintf(work, "guiLockMem returned null in %s, WM_LBUTTONUP", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		if (controlState->buttonDown) PBSetButtonState(hwnd, FALSE);
		guiUnlockMem(zh);
		return 0;

	case WM_SETFOCUS:
	case WM_KILLFOCUS:

		zh = (ZHANDLE) GetWindowLongPtr(hwnd, PBSTATESTRUCT_OFFSET);
		/**
		 * Fix problem found by Ryan J. at PPro.
		 * When a popbox is on a dialog being destroyed, these messages can come to us
		 * after the WM_DESTROY. So we just give up here.
		 * 20 DEC 2012 jpr
		 */
		if (zh == NULL) return 0;

		GetClientRect(hwnd, &rect);
		InflateRect(&rect, -1, -1);
		rect.right -= buttonWidth;
		InvalidateRect(hwnd, &rect, FALSE);

		controlState = (PPOPBOXSTATE) guiLockMem(zh);
		if (controlState == NULL) {
			sprintf(work, "guiLockMem returned null in %s, %s", __FUNCTION__,
					message == WM_SETFOCUS ? "WM_SETFOCUS" : "WM_KILLFOCUS");
			guiaErrorMsg(work, 0);
			return 0;
		}
		if (controlState->inTable) guiaHandleTableFocusMessage(hwnd, message, FALSE);
		guiUnlockMem(zh);
		return 0;

	case WM_SETFONT:
		zh = (ZHANDLE) GetWindowLongPtr(hwnd, PBSTATESTRUCT_OFFSET);
		controlState = (PPOPBOXSTATE) guiLockMem(zh);
		if (controlState == NULL) {
			sprintf(work, "guiLockMem returned null in %s, WM_SETFONT", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		dc = GetDC(hwnd);
		SelectObject(dc, (HFONT) wParam);
		GetTextMetrics(dc, &tm);
		controlState->textHeight = tm.tmHeight;
		guiUnlockMem(zh);
		break;

	case WM_WINDOWPOSCHANGING:
		zh = (ZHANDLE) GetWindowLongPtr(hwnd, PBSTATESTRUCT_OFFSET);
		controlState = (PPOPBOXSTATE) guiLockMem(zh);
		if (controlState == NULL) {
			sprintf(work, "guiLockMem returned null in %s, WM_WINDOWPOSCHANGING", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		winpos = (LPWINDOWPOS) lParam;
		if (winpos->cy == 0)  {
			/* Force a window height based on text height */
			winpos->cy = controlState->textHeight + 6;
		}
		guiUnlockMem(zh);
		return 0;

	case WM_SETTEXT:
		return PBSetText(hwnd, (CHAR *) lParam);

	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
		if (!handleTableRightButtonClick(hwnd, message, lParam, wParam)) return 0;
		break;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}

/**
 * If this popbox was created outside a table, then the lpParam argument of the CreateWindow call
 * is a pointer to the DX CONTROL structure.
 * If it was created as a cell in a table, this parameter will be NULL.
 */
static LRESULT PB_OnCreate(HWND hwnd, LPCREATESTRUCT cs) {

	ZHANDLE zh;
	PPOPBOXSTATE controlState;
	CHAR work[64];

	zh = guiAllocMem(sizeof(POPBOXSTATE));
	if (zh == NULL) {
		sprintf(work, "guiAllocMem failure in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return FALSE;
	}
	controlState = (PPOPBOXSTATE)guiLockMem(zh);
	if (controlState == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return FALSE;
	}
	ZeroMemory(controlState, sizeof(POPBOXSTATE));
	controlState->inTable = cs->lpCreateParams == NULL;
	guiUnlockMem(zh);
	SetWindowLongPtr(hwnd, PBSTATESTRUCT_OFFSET, (LONG_PTR) zh);
	if (cs->lpszName != NULL) PBSetText(hwnd, cs->lpszName);
	return TRUE;
}


static void PB_OnKey(HWND hwnd, UINT vk, BOOL bKeyDown, int cRepeat, UINT flags) {

	HWND hwndCells, hwndTable;

	if (bKeyDown) {
		if (vk == VK_SPACE) PBSetButtonState(hwnd, TRUE);
		else if (vk == VK_DOWN || vk == VK_UP || vk == VK_TAB || vk == VK_RETURN) {
			hwndCells = GetParent(hwnd);
			hwndTable = GetParent(hwndCells);
			if (hwndTable != NULL) FORWARD_WM_KEYDOWN(hwndTable, vk, cRepeat, flags, PostMessage);
		}
	}
	else {
		if (vk == VK_SPACE) PBSetButtonState(hwnd, FALSE);
	}
}

/**
 * Change on 05/21/2012
 * Don't restrict mouse clicks to just the button, react if the mouse click
 * is anywhere in the box.
 * This is in line with the Doc and with the JavaSC
 */
static void PB_OnLButtonDown(HWND hwnd, BOOL bDblClk, int x, int y, UINT flags) {
	POINT pnt;
	RECT rect;

	SetFocus(hwnd);
	pnt.x = x;
	pnt.y = y;
	GetClientRect(hwnd, &rect);
	//rect.left = rect.right - buttonWidth;
	if (PtInRect(&rect, pnt)) PBSetButtonState(hwnd, TRUE);
}


static LRESULT PB_OnPaint(HWND hwnd) {

	RECT rect, rect2;
	ZHANDLE zh;
	PPOPBOXSTATE controlState;
	HDC dc;
	PAINTSTRUCT ps;
	COLORREF bgcolor;
	HBRUSH bgbrush;
	BOOL isEnabled = IsWindowEnabled(hwnd);

	GetClientRect(hwnd, &rect);
	zh = (ZHANDLE) GetWindowLongPtr(hwnd, PBSTATESTRUCT_OFFSET);
	controlState = (PPOPBOXSTATE) guiLockMem(zh);
	if (controlState == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	dc = BeginPaint(hwnd, &ps);

	// Paint the overall background
	if (controlState->inTable) bgbrush = getBackgroundBrush(hwnd);
	else {
		if (isEnabled) {
			SendMessage(GetParent(hwnd), WM_CTLCOLORSTATIC, (WPARAM) dc, (LPARAM) hwnd);
			bgcolor = GetBkColor(dc);
			bgbrush = CreateSolidBrush(bgcolor);
		}
		else {
			bgbrush = GetSysColorBrush(COLOR_BTNFACE);
		}
	}
	FillRect(dc, &rect, bgbrush);

	// If not gray and has focus, draw things the focus way
	if (isEnabled && hwnd == GetFocus()) {
		rect2 = rect;
		rect2.right -= buttonWidth;
		InflateRect(&rect2, -1, -1);
		FillRect(dc, &rect2, GetSysColorBrush(COLOR_HIGHLIGHT));
		PBDrawFocusRect(dc, rect2);
	}

	if (controlState->textlen) {
		if (controlState->text != NULL && strlen(controlState->text)) {
			rect2 = rect;
			rect2.right -= buttonWidth;
			InflateRect(&rect2, -1, -1);
			PBDrawText(hwnd, dc, controlState, controlState->text, rect.left + 3, rect2);
		}
	}

	/* Now draw the button */
	rect.left = rect.right - buttonWidth;
	DrawEdge(dc, &rect, controlState->buttonDown ? EDGE_SUNKEN : EDGE_RAISED, BF_ADJUST | BF_RECT);
	FillRect(dc, &rect, GetSysColorBrush(COLOR_BTNFACE));
	PBDrawUpArrow(dc, rect, IsWindowEnabled(hwnd));
	EndPaint(hwnd, &ps);
	if (!controlState->inTable) DeleteObject(bgbrush);
	guiUnlockMem(zh);
	return 0;
}

/**
 * Set the state of the Popbox button
 */
static void PBSetButtonState(HWND hwnd, BOOL bDown) {
	ZHANDLE zh;
	PPOPBOXSTATE controlState;
	RECT rect;
	BOOL bWasDown;

	zh = (ZHANDLE) GetWindowLongPtr(hwnd, PBSTATESTRUCT_OFFSET);
	controlState = (PPOPBOXSTATE) guiLockMem(zh);
	if (controlState == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}
	bWasDown = controlState->buttonDown;
	controlState->buttonDown = bDown;
	GetClientRect(hwnd, &rect);
	rect.left = rect.right - buttonWidth;
	InvalidateRect(hwnd, &rect, FALSE);
	UpdateWindow(hwnd);
	guiUnlockMem(zh);
	if (bWasDown && !bDown) PostMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hwnd), BN_CLICKED), (LPARAM) hwnd);
	if (bDown) SetCapture(hwnd);
	else if (GetCapture() == hwnd) ReleaseCapture();
}

static BOOL PBSetText(HWND hwnd, LPCSTR text) {

	PPOPBOXSTATE controlState;
	INT textlen;
	ZHANDLE zh;

	textlen = (text == NULL) ? 0 : (INT)strlen(text);
	if (textlen < 0) return FALSE;

	zh = (ZHANDLE) GetWindowLongPtr(hwnd, PBSTATESTRUCT_OFFSET);
	controlState = (PPOPBOXSTATE) guiLockMem(zh);
	if (controlState == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return FALSE;
	}
	if (textlen == 0) {
		if (controlState->textlen != 0) {
			free(controlState->text);
			controlState->textlen = 0;
			controlState->text = NULL;
		}
	}
	else {
		if (controlState->textlen != textlen) {
			if (controlState->textlen == 0) controlState->text = malloc(textlen + 1);
			else controlState->text = realloc(controlState->text, textlen + 1);
			if (controlState->text == NULL) {
				sprintf(work, "malloc/realloc returned null in %s", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return FALSE;
			}
			controlState->textlen = textlen;
		}
		strcpy(controlState->text, text);
	}
	guiUnlockMem(zh);
	return TRUE;
}

/**
 * Wrote my own focus rectangle drawing routine because I could not get DrawFocusRect to use the colors I wanted.
 * And LineTo does not draw one pixel dots.
 *
 * Draws a dashed rectangle one pixel inside the given one
 */
static void PBDrawFocusRect(HDC dc, RECT rect) {
	int i1;
	for (i1 = rect.left + 1; i1 < rect.right; i1 += 2) {
		SetPixel(dc, i1, rect.top, black);
		SetPixel(dc, i1, rect.bottom - 1, black);
	}
	for (i1 = rect.top + 1; i1 < rect.bottom; i1 += 2) {
		SetPixel(dc, rect.left, i1, black);
		SetPixel(dc, rect.right - 1, i1, black);
	}
}

static void PBDrawUpArrow(HDC dc, RECT rect, BOOL enabled) {
	int i1 = (rect.bottom - rect.top - 3) / 2 + 2;

	COLORREF color = (enabled) ? black : graytext;
	SetPixel(dc, rect.left + 2, rect.top + i1, color);
	SetPixel(dc, rect.left + 3, rect.top + i1, color);
	SetPixel(dc, rect.left + 4, rect.top + i1, color);
	SetPixel(dc, rect.left + 5, rect.top + i1, color);
	SetPixel(dc, rect.left + 6, rect.top + i1--, color);
	SetPixel(dc, rect.left + 3, rect.top + i1, color);
	SetPixel(dc, rect.left + 4, rect.top + i1, color);
	SetPixel(dc, rect.left + 5, rect.top + i1--, color);
	SetPixel(dc, rect.left + 4, rect.top + i1, color);
	if (!enabled) {
		i1 += 3;
		SetPixel(dc, rect.left + 3, rect.top + i1, white);
		SetPixel(dc, rect.left + 4, rect.top + i1, white);
		SetPixel(dc, rect.left + 5, rect.top + i1, white);
		SetPixel(dc, rect.left + 6, rect.top + i1, white);
		SetPixel(dc, rect.left + 7, rect.top + i1, white);
	}
}

static void PBDrawText(HWND hwnd, HDC dc, PPOPBOXSTATE controlState, CHAR *text, LONG xpos, RECT rect) {

	LONG ypos;

	SetBkMode(dc, TRANSPARENT);
	if (IsWindowEnabled(hwnd)) {
		if (hwnd == GetFocus()) SetTextColor(dc, white);
		else SendMessage(GetParent(hwnd), WM_CTLCOLORSTATIC, (WPARAM) dc, (LPARAM) hwnd);
	}
	else {
		SetTextColor(dc, graytext);
	}
	ypos = (rect.bottom + 1 - controlState->textHeight) / 2;
	ExtTextOut(dc, xpos, ypos, ETO_CLIPPED, &rect, text, (UINT)strlen(text), NULL);
}

/***********************************************************************************************************************
 *
 *
 * Table
 * Version 1.0
 * This version creates a window for each cell.
 * This does not scale well at all!
 *
 *
 */

#define CELL_STATE_PROPERTY_STRING "com.dbcsoftware.dbcdx.CELL_STATE_STRUCT_HANDLE"
#define STATIC_WNDPROC_PROPERTY_STRING "com.dbcsoftware.dbcdx.STATIC_WNDPROC_HANDLE"
#define EDIT_WNDPROC_PROPERTY_STRING "com.dbcsoftware.dbcdx.EDIT_WNDPROC_HANDLE"

#define TABLE_COLORSOURCE_DEFAULT 0
#define TABLE_COLORSOURCE_TABLE   1
#define TABLE_COLORSOURCE_COLUMN  2
#define TABLE_COLORSOURCE_ROW     3
#define TABLE_COLORSOURCE_CELL    4

/*
 * This macro returns true if the column type can take a text color command
 */
#define ISCOLUMNCOLOR(type) (type == PANEL_VTEXT || \
	type == PANEL_RVTEXT || \
	type == PANEL_CVTEXT || \
	type == PANEL_EDIT || \
	type == PANEL_LEDIT || \
	type == PANEL_POPBOX)

#define ISEDITCELL(type) (type == PANEL_EDIT || \
	type == PANEL_LEDIT || \
	type == PANEL_FEDIT)

typedef struct tagCellState {
	COLORREF color;
	HFONT font;
	UINT colIndex;		/* zero based */
	UINT type;			/* Uses PANEL_* codes */
	UINT colorSource;	/* Used for precedence testing */
	BOOL selected;		/* Used only for checkbox style cells */
} CELLSTATE, *PCELLSTATE;

static ATOM CELL_STATE_PROPERTY;
static ATOM ORIG_STATIC_WNDPROC;
static ATOM ORIG_EDIT_WNDPROC;
static int HScrollBarHeight, VScrollBarWidth;
static void addHscrollBar(HWND hwnd, PTABLESTATE tblState);
static void addVscrollBar(HWND hwnd, PTABLESTATE tblState);
static void changeCellColor(HWND hwnd, COLORREF cref, UINT source, BOOL force);
static void ensureVisibility(HWND hwnd, UINT column, UINT row);
static LRESULT Table_OnCBAddString(HWND hwnd, CHAR *string);
static LRESULT Table_OnCBDeleteString(HWND hwnd, INT position);
static LRESULT Table_OnCBGetCurSel(HWND hwnd);
static LRESULT Table_OnCBInsertString(HWND hwnd, INT position, CHAR *string);
static LRESULT Table_OnCBResetContent(HWND hwnd);
static LRESULT Table_OnCBSetCurSel(HWND hwnd, INT position);
static LRESULT Table_OnColorCell(HWND hwnd, INT row, INT column);
static LRESULT Table_OnColorColumn(HWND hwnd, INT column);
static LRESULT Table_OnColorRow(HWND hwnd, INT row);
static LRESULT Table_OnColorTable(HWND hwnd);
static LRESULT Table_OnCreate(HWND hwnd, LPCREATESTRUCT cs);
static int Table_OnEraseBkgnd(HWND, HDC);
static void Table_OnHscroll(HWND hwnd, HWND hwndScrollBar, UINT code, int position);
static void Table_OnKey(HWND hwnd, UINT vk, BOOL bKeyDown, int cRepeat, UINT flags);
static void Table_OnSetFocus(HWND hwnd, HWND lastFocus);
static void Table_OnVscroll(HWND hwnd, HWND hwndScrollBar, UINT code, int position);
static LRESULT TableCells_OnCommand(HWND hwnd, INT id, HWND hwndControl, INT code);
static LRESULT TableCells_OnPaint(HWND hwnd);
static void TableCells_OnParentNotify(HWND hwnd, UINT msg, HWND hwndChild, UINT idChild);
static LRESULT TableCells_OnCtlColor(HWND hwnd, HDC dc, HWND hwndEdit, INT type);
static void removeHscrollBar(HWND hwnd, PTABLESTATE tblState);
static void removeVscrollBar(HWND hwnd, PTABLESTATE tblState);
static void addTableRow(HWND hwnd, CONTROL *control, PTABLESTATE pTableState, UINT rownum);
static void changeCellsWindowSize(PTABLESTATE tblState, INT amount);
static HWND GetCellAt(PTABLESTATE, UINT column, UINT row);
static INT GetCellColorSource(HWND hwnd);
static void GetColumnRowFromPoint(PTABLESTATE tblState, int *column, int *row, POINT pnt);
static void GetPointFromColumnRow(PTABLESTATE tblState, UINT column, UINT row, POINT *pnt);
static void TableCheckHScrollBar(HWND hwnd, PTABLESTATE tblState);
static void TableCheckVScrollBar(HWND hwnd, PTABLESTATE tblState);
LRESULT CALLBACK TableProc (HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK TableStaticCellProc (HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK TableEditCellProc (HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK TableCellsProc (HWND, UINT, WPARAM, LPARAM);


static TABLECOLOR NullTableColor = {TEXTCOLOR_DEFAULT, 0};

void guiaRegisterTable(HINSTANCE hInstance)
{
	wndclass.style         = CS_OWNDC;
	wndclass.lpfnWndProc   = TableProc ;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = sizeof(LONG_PTR);
	wndclass.hInstance     = hInstance ;
	wndclass.hIcon         = NULL;
	wndclass.hCursor       = LoadCursor (NULL, IDC_ARROW) ;
	wndclass.hbrBackground = NULL; //GetSysColorBrush(COLOR_WINDOW);
	wndclass.lpszMenuName  = NULL ;
	wndclass.lpszClassName = szTableClassName;
	if (!(TableClassAtom = RegisterClass(&wndclass))) {
		sprintf(work, "%s failed, e=%d", __FUNCTION__, (int) GetLastError());
		MessageBox (NULL, work, szTableClassName, MB_ICONERROR);
	}

	HScrollBarHeight = GetSystemMetrics(SM_CYHSCROLL);
	VScrollBarWidth = GetSystemMetrics(SM_CXVSCROLL);

	wndclass.style         = CS_OWNDC;
	wndclass.lpfnWndProc   = TableCellsProc ;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = sizeof(LONG_PTR);
	wndclass.hInstance     = hInstance ;
	wndclass.hIcon         = NULL;
	wndclass.hCursor       = LoadCursor (NULL, IDC_ARROW) ;
	wndclass.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
	wndclass.lpszMenuName  = NULL ;
	wndclass.lpszClassName = szTableCellsClassName;
	if (!RegisterClass(&wndclass)) {
		sprintf(work, "RegisterClass in %s failed, e=%d", __FUNCTION__, (int) GetLastError());
		MessageBox (NULL, work, szTableCellsClassName, MB_ICONERROR);
	}
	CELL_STATE_PROPERTY = GlobalAddAtom(CELL_STATE_PROPERTY_STRING);
	ORIG_STATIC_WNDPROC = GlobalAddAtom(STATIC_WNDPROC_PROPERTY_STRING);
	ORIG_EDIT_WNDPROC = GlobalAddAtom(EDIT_WNDPROC_PROPERTY_STRING);
	return;
}

static int TableCells_OnEraseBkgnd(HWND hwnd, HDC dc) {
	RECT rect;

	GetClientRect(hwnd, &rect);
	FillRect(dc, &rect, getBackgroundBrush(hwnd));
	return 1;
}

LRESULT CALLBACK TableCellsProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
		HANDLE_MSG(hwnd, WM_COMMAND, TableCells_OnCommand);
		HANDLE_MSG(hwnd, WM_PAINT, TableCells_OnPaint);
		HANDLE_MSG(hwnd, WM_PARENTNOTIFY, TableCells_OnParentNotify);
		HANDLE_MSG(hwnd, WM_CTLCOLOREDIT, TableCells_OnCtlColor);
		HANDLE_MSG(hwnd, WM_CTLCOLORSTATIC, TableCells_OnCtlColor);
		HANDLE_MSG(hwnd, WM_ERASEBKGND, TableCells_OnEraseBkgnd); // @suppress("Symbol is not resolved")

	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
		if (!handleTableRightButtonClick(hwnd, message, lParam, wParam)) return 0;
		break;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}

static LRESULT TableCells_OnCtlColor(HWND hwnd, HDC dc, HWND hwndEdit, INT code) {
	PCELLSTATE pCstate;
	ZHANDLE zhCstate;

	zhCstate = (ZHANDLE) GetProp(hwndEdit, MAKEINTATOM(CELL_STATE_PROPERTY));
	pCstate = guiLockMem(zhCstate);
	if (pCstate == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	SetTextColor(dc, isTableEnabled(hwnd) ? pCstate->color : GetSysColor(COLOR_GRAYTEXT));
	if (code == CTLCOLOR_EDIT) SetBkColor(dc, getBackgroundColor(hwnd));
	guiUnlockMem(zhCstate);
	return (LRESULT) getBackgroundBrush(hwnd);
}

static LRESULT TableCells_OnCommand(HWND hwnd, INT id, HWND hwndControl, INT code) {

	PCELLSTATE pCstate;
	ZHANDLE zhCstate;
	NMLISTVIEW nmlistview;
	HWND hwndParent;
	CONTROL *control;
	RESOURCE *res;

	zhCstate = (ZHANDLE) GetProp(hwndControl, MAKEINTATOM(CELL_STATE_PROPERTY));
	if (zhCstate == NULL) return 0;	// Could happen if windows calls us early, e.g. EN_UPDATE
	pCstate = guiLockMem(zhCstate);
	if (pCstate == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}

	// We get a WM_COMMAND/BN_CLICKED message from a popbox being pushed
	if (code == BN_CLICKED) {
		if (pCstate->type == PANEL_POPBOX) {
			// Fill in nmlistview and pass this message to my parent
			ZeroMemory(&nmlistview, sizeof(NMLISTVIEW));
			nmlistview.hdr.hwndFrom = hwndControl;
			nmlistview.hdr.code = BN_CLICKED;
			getColumnRowFromWindow(hwndControl, hwnd, (int*) &nmlistview.iSubItem, (int*) &nmlistview.iItem) ;
			SendMessage(GetParent(hwnd), WM_NOTIFY, (WPARAM) 0, (LPARAM) &nmlistview);
		}
	}

	// We get a WM_COMMAND/EN_CHANGED message from an edit box
	else if (code == EN_CHANGE) {
		if (pCstate->type == PANEL_EDIT || pCstate->type == PANEL_LEDIT) {
			hwndParent = GetParent(hwnd);
			control = (CONTROL *) GetWindowLongPtr(hwndParent, GWLP_USERDATA);
			if (!control->changeflag) {
				res = *(control->res);
				if (res->itemmsgs) {
					// Fill in nmlistview and pass this message to my parent
					ZeroMemory(&nmlistview, sizeof(NMLISTVIEW));
					nmlistview.hdr.hwndFrom = hwndControl;
					nmlistview.hdr.code = LVN_ITEMCHANGED;
					nmlistview.uChanged = LVIF_TEXT;
					getColumnRowFromWindow(hwndControl, hwnd, (int*) &nmlistview.iSubItem, (int*) &nmlistview.iItem) ;
					SendMessage(hwndParent, WM_NOTIFY, (WPARAM) 0, (LPARAM) &nmlistview);
				}
			}
		}
	}
	// We get a WM_COMMAND/CBN_SELCHANGE message from a DROPBOX or CDROPBOX
	else if (code == CBN_SELCHANGE) {
		if (pCstate->type == PANEL_DROPBOX || pCstate->type == PANEL_CDROPBOX) {
			hwndParent = GetParent(hwnd);
			control = (CONTROL *) GetWindowLongPtr(hwndParent, GWLP_USERDATA);
			if (!control->changeflag) {
				res = *(control->res);
				if (res->itemmsgs) {
					// Fill in nmlistview and pass this message to my parent
					ZeroMemory(&nmlistview, sizeof(NMLISTVIEW));
					nmlistview.hdr.hwndFrom = hwndControl;
					nmlistview.hdr.code = LVN_ITEMCHANGED;
					nmlistview.uChanged = LVIF_STATE;
					nmlistview.uNewState = ComboBox_GetCurSel(hwndControl);
					getColumnRowFromWindow(hwndControl, hwnd, (int*) &nmlistview.iSubItem, (int*) &nmlistview.iItem) ;
					SendMessage(hwndParent, WM_NOTIFY, (WPARAM) 0, (LPARAM) &nmlistview);
				}
			}
		}
	}

	guiUnlockMem(zhCstate);
	return 0;
}

static LRESULT TableCells_OnPaint(HWND hwnd) {

	ZHANDLE zhTstate;
	PTABLESTATE tblState;
	HDC dc;
	PAINTSTRUCT ps;
	int y1, y2, x, y, i1;
	COLORREF cellBorderColor;

	zhTstate = (ZHANDLE) GetWindowLongPtr(GetParent(hwnd), TBLSTATESTRUCT_OFFSET);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	cellBorderColor = isTableEnabled(hwnd) ? GetSysColor(COLOR_BTNFACE) : GetSysColor(COLOR_GRAYTEXT);
	dc = BeginPaint(hwnd, &ps);
	// Paint the grid!
	y1 = ps.rcPaint.top / tblState->cellHeight * tblState->cellHeight + tblState->cellHeight - 1;
	y2 = ps.rcPaint.bottom / tblState->cellHeight * tblState->cellHeight;
	for (y = y1; y <= y2; y += tblState->cellHeight) {
		for (x = ps.rcPaint.left; x <= ps.rcPaint.right; x++) SetPixel(dc, x, y, cellBorderColor);
	}
	for (x = i1 = 0; (UINT) i1 < tblState->numcolumns; i1++) {
		x += tblState->columns[i1].columnWidth;
		if ( x > ps.rcPaint.right) break;
		if (x < ps.rcPaint.left) continue;
		for (y = ps.rcPaint.top; y <= ps.rcPaint.bottom; y++) SetPixel(dc, x, y, cellBorderColor);
	}
	EndPaint(hwnd, &ps);
	guiUnlockMem(zhTstate);
	return 0;
}

static void TableCells_OnParentNotify(HWND hwnd, UINT msg, HWND hwndChild, UINT idChild) {

	ZHANDLE zhCstate;

	if (msg == WM_DESTROY) {
		zhCstate = (ZHANDLE) RemoveProp(hwndChild, MAKEINTATOM(CELL_STATE_PROPERTY));
		guiFreeMem(zhCstate);
	}
}

void getColumnRowFromWindow(HWND hwnd, HWND hwndCells, int *column, int *row) {

	PTABLESTATE tblState;
	RECT rect;
	POINT point;
	ZHANDLE zhTstate;
	HWND hwndTable = GetParent(hwndCells);

	GetWindowRect(hwnd, &rect);
	point.x = rect.left;
	point.y = rect.top;
	ScreenToClient(hwndCells, &point);
	zhTstate = (ZHANDLE) GetWindowLongPtr(hwndTable, TBLSTATESTRUCT_OFFSET);
	if (zhTstate == NULL) {
		*column = -1;
		*row = -1;
	}
	else {
		tblState = (PTABLESTATE) guiLockMem(zhTstate);
		if (tblState == NULL) {
			sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, 0);
			*column = -1;
			*row = -1;
			return;
		}
		GetColumnRowFromPoint(tblState, column, row, point);
		guiUnlockMem(zhTstate);
	}
}

void guiaHandleTableFocusMessage(HWND hwnd, UINT message, BOOL invalidate) {
	HWND hwndCells, hwndTable;
	int col, row;
	CONTROL *control;
	RESOURCE *res;
	PCELLSTATE pCstate;
	ZHANDLE zhCstate;

	if (invalidate) InvalidateRect(hwnd, NULL, FALSE);
	hwndCells = GetParent(hwnd);
	hwndTable = GetParent(hwndCells);
	control = (CONTROL *) GetWindowLongPtr(hwndTable, GWLP_USERDATA);
	res = *control->res;
	if (res->focusmsgs) {
		getColumnRowFromWindow(hwnd, hwndCells, &col, &row);
		if (col >= 0 && row >= 0) {
			if (message == WM_SETFOCUS) {
				zhCstate = (ZHANDLE) GetProp(hwnd, MAKEINTATOM(CELL_STATE_PROPERTY));
				pCstate = guiLockMem(zhCstate);
				if (pCstate == NULL) {
					sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
					guiaErrorMsg(work, 0);
					return;
				}
				if (ISEDITCELL(pCstate->type)) PostMessage(hwnd, EM_SETSEL, 0, -1);
				guiUnlockMem(zhCstate);
				guiaTableSendFocusMessage(DBC_MSGFOCUS, control, col, row);
			}
			else if (message == WM_KILLFOCUS) guiaTableSendFocusMessage(DBC_MSGLOSEFOCUS, control, col, row);
		}
	}
}

/**
 * Send a formated message to the DX runtime.
 * This is intended for FOCS and FOCL messages
 * We assume that the caller has checked that focus messages are desired.
 * column and row are zero based.
 *
 */
void guiaTableSendFocusMessage(CHAR* message, CONTROL *control, UINT column, UINT row) {
	RESOURCE *res = *control->res;
	PTABLECOLUMN tblcolumns = (TABLECOLUMN *) guiLockMem(control->table.columns);
	if (tblcolumns == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}
	sprintf((CHAR *)cbmsgdata, "%5d", row + 1);
	rescb(res, message, tblcolumns[column].useritem, 5);
	guiUnlockMem(control->table.columns);
}

LRESULT CALLBACK TableProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	UINT i1, i2;
	UINT numberOfRowsThatNeedToBeMoved;
	INT yPos, textlen;
	PTABLESTATE tblState;
	CONTROL *control;
	PTABLECOLUMN tblcolumns;
	LPLVITEM plvitem;
	POINT point;
	HWND hwnd2;
	HDWP hDefWinPos;
	PTABLEROW tblrows;
	ZHANDLE zhTstate, zhRows, zhCstate;
	NMHDR *pnmhdr;
	LPNMHEADER pnmheader;
	LPNMLISTVIEW pnmlistview;
	PCELLSTATE pCstate;
	RESOURCE *res;
	int column, row;
	HDITEM hdi;
	SCROLLINFO si;

	si.cbSize = sizeof(si);

	switch (message) {

	HANDLE_MSG(hwnd, WM_CREATE, Table_OnCreate);
	HANDLE_MSG(hwnd, WM_ERASEBKGND, Table_OnEraseBkgnd); // @suppress("Symbol is not resolved")
	HANDLE_MSG(hwnd, WM_HSCROLL, Table_OnHscroll);
	HANDLE_MSG(hwnd, WM_KEYDOWN, Table_OnKey);
	HANDLE_MSG(hwnd, WM_VSCROLL, Table_OnVscroll);
	HANDLE_MSG(hwnd, WM_SETFOCUS, Table_OnSetFocus);

	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
		control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (!sendRightMouseMessageFromPanel(hwnd, message, control, lParam, (UINT) wParam)) return 0;
		break;

	case WM_DESTROY:
		zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
		guiFreeMem(zhTstate);
		SetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET, (LONG_PTR) NULL);
		return 0;

	/**
	 * Consult the control structure and set our internal flag
	 * for readonly state on/off
	 */
	case DXTBL_READONLY:
		zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
		tblState = (PTABLESTATE) guiLockMem(zhTstate);
		if (tblState == NULL) {
			sprintf(work, "guiLockMem returned null in %s, DXTBL_ADDROW", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		tblState->readonly = (control->style & CTRLSTYLE_READONLY) != 0;
		guiUnlockMem(zhTstate);
		return 0;

	/*
	 * control->table.numrows has already been incremented in gui.c
	 */
	case DXTBL_ADDROW:
		zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
		tblState = (PTABLESTATE) guiLockMem(zhTstate);
		if (tblState == NULL) {
			sprintf(work, "guiLockMem returned null in %s, DXTBL_ADDROW", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		addTableRow(tblState->hwndCells, control, tblState, control->table.numrows - 1);
		changeCellsWindowSize(tblState, tblState->cellHeight);
		TableCheckVScrollBar(hwnd, tblState);
		TableCheckHScrollBar(hwnd, tblState);
		guiUnlockMem(zhTstate);
		return 0;

	case DXTBL_FOCUSCELL:
		zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
		tblState = (PTABLESTATE) guiLockMem(zhTstate);
		if (tblState == NULL) {
			sprintf(work, "guiLockMem returned null in %s, DXTBL_FOCUSCELL", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		if (ISCOLUMNFOCUSABLE(tblState->columns[lParam].type)) {
			hwnd2 = GetCellAt(tblState, (UINT) lParam, (UINT) wParam);
			if (hwnd2 != NULL) {
				if (SetFocus(hwnd2) != NULL) {
					control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
					pvistart();
					res = *control->res;
					res->ctlfocus = control;
					pviend();
					ensureVisibility(hwnd, (UINT) lParam, (UINT) wParam);
				}
			}
		}
		guiUnlockMem(zhTstate);
		return 0;

	case DXTBL_COLORROW:
		return Table_OnColorRow(hwnd, (INT) wParam);

	case DXTBL_COLORCOLUMN:
		return Table_OnColorColumn(hwnd, (INT) lParam);

	case DXTBL_COLORCELL:
		return Table_OnColorCell(hwnd, (INT) wParam, (INT) lParam);

	case DXTBL_COLORTABLE:
		return Table_OnColorTable(hwnd);

	case CB_ADDSTRING:
		return Table_OnCBAddString(hwnd, (CHAR*) lParam);

	case CB_INSERTSTRING:
		return Table_OnCBInsertString(hwnd, (INT)wParam, (CHAR*) lParam);

	case CB_DELETESTRING:
		return Table_OnCBDeleteString(hwnd, (INT)wParam);

	case CB_GETCURSEL:
		return Table_OnCBGetCurSel(hwnd);

	case CB_RESETCONTENT:
		return Table_OnCBResetContent(hwnd);

	case CB_SETCURSEL:
		return Table_OnCBSetCurSel(hwnd, (INT)wParam);

	case LVM_DELETEALLITEMS:
		/* control->table.numrows has already been zeroed */
		zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
		tblState = (PTABLESTATE) guiLockMem(zhTstate);
		if (tblState == NULL) {
			sprintf(work, "guiLockMem returned null in %s, LVM_DELETEALLITEMS", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		for (row = 0; row < tblState->table.cy / tblState->cellHeight; row++) {
			for (column = 0; (UINT) column < control->table.numcolumns; column++) {
				DestroyWindow(GetCellAt(tblState, column, row));
			}
		}
		tblState->table.cy = 0;
		SetWindowPos(tblState->hwndCells, NULL, 0, tblState->headerCellHeight, max(tblState->table.cx, tblState->visible.cx), 0,
			SWP_NOZORDER);
		SetWindowPos(tblState->hwndHeader, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
		TableCheckVScrollBar(hwnd, tblState);
		TableCheckHScrollBar(hwnd, tblState);
		guiUnlockMem(zhTstate);
		si.fMask = SIF_POS;
		si.nPos = 0;
		SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
		SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

		return TRUE;

	case LVM_DELETEITEM:
		/* control->table.numrows has already been decremented */
		zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
		tblState = (PTABLESTATE) guiLockMem(zhTstate);
		if (tblState == NULL) {
			sprintf_s(work, ARRAYSIZE(work), "guiLockMem returned null in %s, LVM_DELETEITEM", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		yPos = (INT)wParam;
		for (i1 = 0; i1 < control->table.numcolumns; i1++) {
			DestroyWindow(GetCellAt(tblState, i1, yPos));
		}
		numberOfRowsThatNeedToBeMoved = control->table.numrows - yPos;		/* Number of rows that need to be moved */
		hDefWinPos = BeginDeferWindowPos(numberOfRowsThatNeedToBeMoved * control->table.numcolumns);
		for (i1 = yPos + 1; i1 < control->table.numrows + 1; i1++) {
			for (i2 = 0; i2 < control->table.numcolumns; i2++) {
				GetPointFromColumnRow(tblState, i2, i1, &point);
				hwnd2 = ChildWindowFromPoint(tblState->hwndCells, point);
				if (hwnd2 == NULL) continue;
				DeferWindowPos(hDefWinPos, hwnd2, NULL,
						point.x, point.y - tblState->cellHeight, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
			}
		}
		EndDeferWindowPos(hDefWinPos);
		changeCellsWindowSize(tblState, -tblState->cellHeight);
		TableCheckVScrollBar(hwnd, tblState);
		/*
		 * Fix up an annoying situation.
		 *
		 * If the row deleted was the bottom row, and some of it was visible,
		 * then the scroll bar and the cell window position are now a bit out of sync.
		 * Solution, 'snap' the cell window so that the new bottom row is just
		 * completely visible at the bottom of the table window.
		 */
		if (numberOfRowsThatNeedToBeMoved == 0) {
			POINT point;
			RECT rect;
			LONG zz;

			GetWindowRect(tblState->hwndCells, &rect);
			point.x = rect.left;
			point.y = rect.top;
			ScreenToClient(hwnd, &point);
			/*
			 * At this point, 'point' is the upper left of the cells window relative to the table window
			 */
			if (tblState->vscroll) {
				zz = point.y + tblState->table.cy;
				/*
				 * zz should now be the Y coord of the bottom of the cells window relative to the table window
				 */
				if (zz < (tblState->visible.cy + tblState->headerCellHeight)) {
					moveCellsVert(hwnd, tblState, (tblState->visible.cy + tblState->headerCellHeight) - zz);
				}
			}
			else {
				// Make sure that the top row is totally visible
				if (point.y != tblState->headerCellHeight)
					SetWindowPos(tblState->hwndCells, NULL, 0, tblState->headerCellHeight, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
			}
		}
		guiUnlockMem(zhTstate);
		return 0;

	case DXTBL_INSERTROW:
		/* control->table.numrows has already been incremented */
		zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
		tblState = (PTABLESTATE) guiLockMem(zhTstate);
		if (tblState == NULL) {
			sprintf(work, "guiLockMem returned null in %s, DXTBL_INSERTROW", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		yPos = (INT)lParam;
		i1 = control->table.numrows - yPos;		/* Number of rows that need to be moved */
		hDefWinPos = BeginDeferWindowPos(i1 * control->table.numcolumns);
		for (i1 = yPos; i1 < control->table.numrows; i1++) {
			for (i2 = 0; i2 < control->table.numcolumns; i2++) {
				GetPointFromColumnRow(tblState, i2, i1, &point);
				hwnd2 = ChildWindowFromPoint(tblState->hwndCells, point);
				if (hwnd2 == NULL) continue;
				DeferWindowPos(hDefWinPos, hwnd2, NULL, point.x, point.y + tblState->cellHeight, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
			}
		}
		EndDeferWindowPos(hDefWinPos);
		addTableRow(tblState->hwndCells, control, tblState, yPos);
		changeCellsWindowSize(tblState, tblState->cellHeight);
		TableCheckVScrollBar(hwnd, tblState);
		guiUnlockMem(zhTstate);
		return 0;

	case LVM_SETITEMTEXT:
		plvitem = (LPLVITEM) lParam;
		zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
		tblState = (PTABLESTATE) guiLockMem(zhTstate);
		if (tblState == NULL) {
			sprintf(work, "guiLockMem returned null in %s, LVM_SETITEMTEXT", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		if ((INT)wParam >= 0) {
			hwnd2 = GetCellAt(tblState, (UINT) plvitem->iSubItem, (UINT) wParam);
			SetWindowText(hwnd2, plvitem->pszText);
			InvalidateRect(hwnd2, NULL, FALSE);
		}
		else {		// This is a request to change the column heading
			hdi.mask = HDI_TEXT;
			hdi.pszText = plvitem->pszText;
			hdi.cchTextMax = (int)strlen(hdi.pszText);
			Header_SetItem(tblState->hwndHeader, plvitem->iSubItem, &hdi);
		}
		guiUnlockMem(zhTstate);
		return 0;

	case LVM_SETITEMSTATE:
		plvitem = (LPLVITEM) lParam;
		zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
		tblState = (PTABLESTATE) guiLockMem(zhTstate);
		if (tblState == NULL) {
			sprintf(work, "guiLockMem returned null in %s, LVM_SETITEMSTATE", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		hwnd2 = GetCellAt(tblState, (UINT) plvitem->iSubItem, (UINT) wParam);
		SendMessage(hwnd2, BM_SETCHECK, plvitem->state & LVIS_SELECTED ? BST_CHECKED : BST_UNCHECKED, (LPARAM) 0);
		InvalidateRect(hwnd2, NULL, FALSE);
		guiUnlockMem(zhTstate);
		return 0;

	case WM_NOTIFY:
		pnmhdr = (NMHDR*) lParam;
		if (pnmhdr->code == LVN_ITEMCHANGED) {
			pnmlistview = (LPNMLISTVIEW) lParam;
			if (pnmlistview->uChanged == LVIF_STATE) {
				zhCstate = (ZHANDLE) GetProp(pnmhdr->hwndFrom, MAKEINTATOM(CELL_STATE_PROPERTY));
				pCstate = guiLockMem(zhCstate);
				if (pCstate == NULL) {
					sprintf(work, "guiLockMem returned null for pCstate in %s, WM_NOTIFY/LVN_ITEMCHANGED/LVIF_STATE", __FUNCTION__);
					guiaErrorMsg(work, 0);
					return 0;
				}
				if (pCstate->type == PANEL_CHECKBOX) {
					control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
					res = *(control->res);
					zhRows = guiTableLocateRow(control, pnmlistview->iItem + 1);
					tblrows = (PTABLEROW) guiLockMem(zhRows);
					if (tblrows == NULL) {
						sprintf(work, "guiLockMem returned null for tblrows(PANEL_CHECKBOX) in %s, WM_NOTIFY/LVN_ITEMCHANGED", __FUNCTION__);
						guiaErrorMsg(work, 0);
						return 0;
					}
					if (pnmlistview->uNewState == LVIS_SELECTED) tblrows->cells[pnmlistview->iSubItem].style |= CTRLSTYLE_MARKED;
					else tblrows->cells[pnmlistview->iSubItem].style &= ~CTRLSTYLE_MARKED;
					guiUnlockMem(zhRows);
					if (res->itemmsgs) {
						tblcolumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
						if (tblcolumns == NULL) {
							sprintf(work, "guiLockMem returned null for tblcolumns(PANEL_CHECKBOX) in %s, WM_NOTIFY/LVN_ITEMCHANGED", __FUNCTION__);
							guiaErrorMsg(work, 0);
							return 0;
						}
						sprintf((CHAR *)cbmsgdata, "%5d%s", pnmlistview->iItem + 1, (pnmlistview->uNewState == LVIS_SELECTED) ? "Y" : "N");
						rescb(*control->res, DBC_MSGITEM, tblcolumns[pnmlistview->iSubItem].useritem, 6);
						guiUnlockMem(control->table.columns);
					}
				}
				else if (pCstate->type == PANEL_DROPBOX || pCstate->type == PANEL_CDROPBOX) {
					control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
					res = *(control->res);
					if (res->itemmsgs) {
						tblcolumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
						if (tblcolumns == NULL) {
							sprintf(work, "guiLockMem returned null for tblcolumns(PANEL_[C]DROPBOX) in %s, WM_NOTIFY/LVN_ITEMCHANGED", __FUNCTION__);
							guiaErrorMsg(work, 0);
							return 0;
						}
						if (res->linemsgs) {
							sprintf((CHAR *)cbmsgdata, "%5d%5d", pnmlistview->iItem + 1, pnmlistview->uNewState + 1);
							rescb(*control->res, DBC_MSGITEM, tblcolumns[pnmlistview->iSubItem].useritem, 10);
						}
						else {
							sprintf((CHAR *)cbmsgdata, "%5d", pnmlistview->iItem + 1);
							textlen = GetWindowText(pnmhdr->hwndFrom, (CHAR *)cbmsgdata + 5, MAXCBMSGSIZE - 5);
							rescb(*control->res, DBC_MSGITEM, tblcolumns[pnmlistview->iSubItem].useritem, textlen + 5);
						}
						guiUnlockMem(control->table.columns);
					}
				}
				guiUnlockMem(zhCstate);
			}
			else if (pnmlistview->uChanged == LVIF_TEXT) {
				zhCstate = (ZHANDLE) GetProp(pnmhdr->hwndFrom, MAKEINTATOM(CELL_STATE_PROPERTY));
				pCstate = guiLockMem(zhCstate);
				if (pCstate == NULL) {
					sprintf(work, "guiLockMem returned null for pCstate in %s, WM_NOTIFY/LVN_ITEMCHANGED/LVIF_TEXT", __FUNCTION__);
					guiaErrorMsg(work, 0);
					return 0;
				}
				if (pCstate->type == PANEL_EDIT || pCstate->type == PANEL_LEDIT) {
					// We only get here if the user changed the edit box contents via the UI and item messages are on
					control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
					tblcolumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
					if (tblcolumns == NULL) {
						sprintf(work, "guiLockMem returned null for tblcolumns(PANEL_[L]EDIT) in %s, WM_NOTIFY/LVN_ITEMCHANGED", __FUNCTION__);
						guiaErrorMsg(work, 0);
						return 0;
					}
					sprintf((CHAR *)cbmsgdata, "%5d", pnmlistview->iItem + 1);
					textlen = GetWindowText(pnmhdr->hwndFrom, (CHAR *)cbmsgdata + 5, MAXCBMSGSIZE - 5);
					rescb(*control->res, DBC_MSGITEM, tblcolumns[pnmlistview->iSubItem].useritem, textlen + 5);
					guiUnlockMem(control->table.columns);
				}
				guiUnlockMem(zhCstate);
			}
		}
		else if (pnmhdr->code == BN_CLICKED) {
			pnmlistview = (LPNMLISTVIEW) lParam;
			control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			tblcolumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
			if (tblcolumns == NULL) {
				sprintf(work, "guiLockMem returned null for tblcolumns in %s, WM_NOTIFY/BN_CLICKED", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return 0;
			}
			sprintf((CHAR *)cbmsgdata, "%5d", pnmlistview->iItem + 1);
			rescb(*control->res, DBC_MSGPUSH, tblcolumns[pnmlistview->iSubItem].useritem, 5);
			guiUnlockMem(control->table.columns);
		}
		else if (pnmhdr->code == HDN_ITEMCLICK) {
			pnmheader = (LPNMHEADER) lParam;
			control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			tblcolumns = (PTABLECOLUMN) guiLockMem(control->table.columns);
			if (tblcolumns == NULL) {
				sprintf(work, "guiLockMem returned null for tblcolumns in %s, WM_NOTIFY/HDN_ITEMCLICK", __FUNCTION__);
				guiaErrorMsg(work, 0);
				return 0;
			}
			rescb(*control->res, DBC_MSGPUSH, tblcolumns[pnmheader->iItem].useritem, 0);
			guiUnlockMem(control->table.columns);
		}
		else if (pnmhdr->code == (UINT)NM_RCLICK) {
			PostMessage(hwnd, WM_RBUTTONUP, (WPARAM)NULL, MAKELPARAM(0, 0));
		}
		else if (pnmhdr->code == HDN_BEGINTRACK) return TRUE; /* Prevent column width changing */
		return 0;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}

static void moveCellsHorz(HWND hwnd, PTABLESTATE tblState, int amount) {
	RECT rect;
	POINT point;

	GetWindowRect(tblState->hwndCells, &rect);
	point.x = rect.left;
	point.y = rect.top;
	ScreenToClient(hwnd, &point);
	point.x += amount;
	SetWindowPos(tblState->hwndCells, NULL, point.x, point.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	SetWindowPos(tblState->hwndHeader, NULL, point.x, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

static void moveCellsVert(HWND hwnd, PTABLESTATE tblState, int amount) {
	RECT rect;
	POINT point;

	GetWindowRect(tblState->hwndCells, &rect);
	point.x = rect.left;
	point.y = rect.top;
	ScreenToClient(hwnd, &point);
	point.y += amount;
	SetWindowPos(tblState->hwndCells, NULL, point.x, point.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

static void ensureVisibility(HWND hwnd, UINT column, UINT row) {
	PTABLESTATE tblState;
	ZHANDLE zhTstate;
	RECT rcCell, rcTable, rcHeader, rect2;
	HWND hwndCell;
	INT moveVert = 0, moveHorz = 0;
	SCROLLINFO si;

	zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState == NULL) {
		sprintf(work, "guiLockMem returned null for tblState in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}
	hwndCell = GetCellAt(tblState, column, row);
	if (hwndCell == NULL) return;

	// Get the rectangle of the cell in screen coordinates;
	GetWindowRect(hwndCell, &rcCell);

	// Get the rectangle of the table client area
	GetClientRect(hwnd, &rcTable);

	// Allow for the height of the header
	GetClientRect(tblState->hwndHeader, &rcHeader);
	rcTable.top += rcHeader.bottom;

	// Convert table rect from client to screen coordinates
	MapWindowPoints(hwnd, GetDesktopWindow(), (LPPOINT) &rcTable, 2);

	// Decide how the cell lies relative to the table
	IntersectRect(&rect2, &rcTable, &rcCell);
	if (EqualRect(&rect2, &rcCell)) {
		// No movement required, the cell is completely visible
		guiUnlockMem(zhTstate);
		return;
	}
	if (rcCell.top < rcTable.top) {
		moveVert = rcTable.top - rcCell.top;
	}
	else if (rcCell.bottom > rcTable.bottom) {
		moveVert = -(rcCell.bottom - rcTable.bottom);
	}

	if (rcCell.left < rcTable.left) {
		moveHorz = rcTable.left - rcCell.left;
	}
	else if (rcCell.right > rcTable.right) {
		moveHorz = -(rcCell.right - rcTable.right);
	}

	if (moveHorz) {
		moveCellsHorz(hwnd, tblState, moveHorz);
		si.cbSize = sizeof(si);
		si.fMask = SIF_POS;
		GetScrollInfo(hwnd, SB_HORZ, &si);
		si.nPos -= moveHorz;
		SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
	}

	if (moveVert) {
		moveCellsVert(hwnd, tblState, moveVert);
		si.cbSize = sizeof(si);
		si.fMask = SIF_POS;
		GetScrollInfo(hwnd, SB_VERT, &si);
		si.nPos -= moveVert;
		SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
	}
	guiUnlockMem(zhTstate);
}

static TABLECOLOR GetCellColorCode(CONTROL *control, INT row, INT column) {
	PTABLECELL cell;
	ZHANDLE zh;
	PTABLEROW trowptr;

	pvistart();
	zh = guiTableLocateRow(control, row + 1);
	trowptr = guiLockMem(zh);
	if (trowptr == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return NullTableColor;
	}
	cell = &trowptr->cells[column];
	guiUnlockMem(zh);
	pviend();
	return cell->textcolor;
}

static TABLECOLOR GetRowColorCode(CONTROL *control, INT row) {
	ZHANDLE zh;
	PTABLEROW trowptr;

	pvistart();
	zh = guiTableLocateRow(control, row + 1);
	trowptr = guiLockMem(zh);
	if (trowptr == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return NullTableColor;
	}
	guiUnlockMem(zh);
	pviend();
	return trowptr->textcolor;
}

static TABLECOLOR GetColumnColorCode(CONTROL *control, INT column) {
	PTABLECOLUMN columns;

	columns = (PTABLECOLUMN) guiLockMem(control->table.columns);
	if (columns == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return NullTableColor;
	}
	guiUnlockMem(control->table.columns);
	return columns[column].textcolor;
}

static TABLECOLOR GetColorFallbackFromColumnLevel(CONTROL *control, INT row, INT column, INT *source) {
	TABLECOLOR tbcolor = control->table.textcolor;
	if (tbcolor.code == 1 || tbcolor.color != TEXTCOLOR_DEFAULT) {
		*source = TABLE_COLORSOURCE_TABLE;
		return tbcolor;
	}
	*source = TABLE_COLORSOURCE_DEFAULT;
	return NullTableColor;
}

static TABLECOLOR GetColorFallbackFromRowLevel(CONTROL *control, INT row, INT column, INT *source) {
	TABLECOLOR tbcolor = GetColumnColorCode(control, column);
	if (tbcolor.code == 1 || tbcolor.color != TEXTCOLOR_DEFAULT) {
		*source = TABLE_COLORSOURCE_COLUMN;
		return tbcolor;
	}
	tbcolor = control->table.textcolor;
	if (tbcolor.code == 1 || tbcolor.color != TEXTCOLOR_DEFAULT) {
		*source = TABLE_COLORSOURCE_TABLE;
		return tbcolor;
	}
	*source = TABLE_COLORSOURCE_DEFAULT;
	return NullTableColor;
}

static TABLECOLOR GetColorFallbackFromCellLevel(CONTROL *control, INT row, INT column, INT *source) {
	TABLECOLOR tbcolor = GetRowColorCode(control, row);
	if (tbcolor.code == 1 || tbcolor.color != TEXTCOLOR_DEFAULT) {
		*source = TABLE_COLORSOURCE_ROW;
		return tbcolor;
	}
	tbcolor = GetColumnColorCode(control, column);
	if (tbcolor.code == 1 || tbcolor.color != TEXTCOLOR_DEFAULT) {
		*source = TABLE_COLORSOURCE_COLUMN;
		return tbcolor;
	}
	tbcolor = control->table.textcolor;
	if (tbcolor.code == 1 || tbcolor.color != TEXTCOLOR_DEFAULT) {
		*source = TABLE_COLORSOURCE_TABLE;
		return tbcolor;
	}
	*source = TABLE_COLORSOURCE_DEFAULT;
	return NullTableColor;
}

static LRESULT Table_OnColorTable(HWND hwnd) {
	CONTROL *control;
	COLORREF cref, defcref;
	PTABLECOLUMN columns;
	UINT row, column;
	PTABLESTATE tblState;
	HWND hwnd2;
	ZHANDLE zhTstate;
	TABLECOLOR tbcolor;
	BOOL fallback;

	zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState == NULL) {
		sprintf(work, "guiLockMem returned null for tblState in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	tbcolor = control->table.textcolor;
	if (tbcolor.code == 1) {
		cref = tbcolor.color;
		fallback = FALSE;
	}
	else if (tbcolor.color != TEXTCOLOR_DEFAULT) {
		cref = GetColorefFromColorNumber(tbcolor.color);
		fallback = FALSE;
	}
	else {
		defcref = GetColorefFromColorNumber(TEXTCOLOR_DEFAULT);
		fallback = TRUE;
	}

	columns = (PTABLECOLUMN) guiLockMem(control->table.columns);
	if (columns == NULL) {
		sprintf(work, "guiLockMem returned null for columns in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	for (column = 0; column < control->table.numcolumns; column++) {
		if (ISCOLUMNCOLOR(columns[column].type)) {
			for (row = 0; row < control->table.numrows; row++) {
				hwnd2 = GetCellAt(tblState, column, row);
				if (!fallback) changeCellColor(hwnd2, cref, TABLE_COLORSOURCE_TABLE, FALSE);
				else {
					if (GetCellColorSource(hwnd2) <= TABLE_COLORSOURCE_TABLE) {
						changeCellColor(hwnd2, defcref, TABLE_COLORSOURCE_DEFAULT, TRUE);
					}
				}
			}
		}
	}
	guiUnlockMem(zhTstate);
	return 0;
}

/*
 * Locate all CELLSTATE structures for the given column (zero-based)
 * and set the color member to an RGB corresponding to TABLECOLUMN.textcolor
 */
static LRESULT Table_OnColorColumn(HWND hwnd, INT column) {
	PTABLESTATE tblState;
	ZHANDLE zhTstate;
	CONTROL *control;
	UINT row;
	HWND hwnd2;
	COLORREF cref;
	TABLECOLOR tbcolor;
	INT source;
	BOOL fallback;

	zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	tbcolor = GetColumnColorCode(control, column);
	if (tbcolor.code == 1) {
		cref = tbcolor.color;
		fallback = FALSE;
	}
	else if (tbcolor.color != TEXTCOLOR_DEFAULT) {
		cref = GetColorefFromColorNumber(tbcolor.color);
		fallback = FALSE;
	}
	else fallback = TRUE;

	for (row = 0; row < control->table.numrows; row++) {
		hwnd2 = GetCellAt(tblState, column, row);
		if (!fallback) changeCellColor(hwnd2, cref, TABLE_COLORSOURCE_COLUMN, FALSE);
		else {
			if (GetCellColorSource(hwnd2) <= TABLE_COLORSOURCE_COLUMN) {
				tbcolor = GetColorFallbackFromColumnLevel(control, row, column, &source);
				cref = (tbcolor.code == 0) ? GetColorefFromColorNumber(tbcolor.color) : tbcolor.color;
				changeCellColor(hwnd2, cref, source, TRUE);
			}
		}
	}
	guiUnlockMem(zhTstate);
	return 0;
}

/*
 * Locate all CELLSTATE structures for the given row (zero-based)
 * and set the color member to an RGB corresponding to TABLEROW.textcolor
 */
static LRESULT Table_OnColorRow(HWND hwnd, INT row) {
	PTABLESTATE tblState;
	ZHANDLE zhTstate;
	TABLECOLOR tbcolor;
	INT source;
	UINT column;
	PTABLECOLUMN columns;
	CONTROL *control;
	COLORREF cref;
	HWND hwnd2;
	BOOL fallback;

	zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState == NULL) {
		sprintf(work, "guiLockMem returned null for tblState in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	tbcolor = GetRowColorCode(control, row);		/* DB/C color code, e.g. TEXTCOLOR_RED */
	if (tbcolor.code == 1) {
		cref = tbcolor.color;
		fallback = FALSE;
	}
	else if (tbcolor.color != TEXTCOLOR_DEFAULT) {
		cref = GetColorefFromColorNumber(tbcolor.color);
		fallback = FALSE;
	}
	else fallback = TRUE;

	columns = (PTABLECOLUMN) guiLockMem(control->table.columns);
	if (columns == NULL) {
		sprintf(work, "guiLockMem returned null for columns in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	for (column = 0; column < control->table.numcolumns; column++) {
		if (ISCOLUMNCOLOR(columns[column].type)) {
			hwnd2 = GetCellAt(tblState, column, row);
			if (!fallback) changeCellColor(hwnd2, cref, TABLE_COLORSOURCE_ROW, FALSE);
			else {
				if (GetCellColorSource(hwnd2) <= TABLE_COLORSOURCE_ROW) {
					tbcolor = GetColorFallbackFromRowLevel(control, row, column, &source);
					cref = (tbcolor.code == 0) ? GetColorefFromColorNumber(tbcolor.color) : tbcolor.color;
					changeCellColor(hwnd2, cref, source, TRUE);
				}
			}
		}
	}
	guiUnlockMem(control->table.columns);
	guiUnlockMem(zhTstate);
	return 0;
}

static LRESULT Table_OnColorCell(HWND hwnd, INT row, INT column) {
	CONTROL *control;
	COLORREF cref;
	ZHANDLE zhTstate;
	PTABLESTATE tblState;
	HWND hwnd2;
	INT source;
	TABLECOLOR tbcolor;
	BOOL force = TRUE;	/* Can always be TRUE, cell level has highest priority */

	control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	tbcolor = GetCellColorCode(control, row, column);
	if (tbcolor.code == 1) {
		source = TABLE_COLORSOURCE_CELL;
		cref = tbcolor.color;
	}
	else if (tbcolor.color == TEXTCOLOR_DEFAULT) {
		tbcolor = GetColorFallbackFromCellLevel(control, row, column, &source);
		cref = GetColorefFromColorNumber(tbcolor.color);
	}
	else {
		source = TABLE_COLORSOURCE_CELL;
		cref = GetColorefFromColorNumber(tbcolor.color);
	}

	zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState == NULL) {
		sprintf(work, "guiLockMem returned null for tblState in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}

	hwnd2 = GetCellAt(tblState, column, row);
	changeCellColor(hwnd2, cref, source, force);

	guiUnlockMem(zhTstate);
	return 0;
}

static INT GetCellColorSource(HWND hwnd) {
	ZHANDLE zhCstate;
	PCELLSTATE pCstate;
	INT source;

	zhCstate = (ZHANDLE) GetProp(hwnd, MAKEINTATOM(CELL_STATE_PROPERTY));
	pCstate = guiLockMem(zhCstate);
	if (pCstate == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	source = pCstate->colorSource;
	guiUnlockMem(zhCstate);
	return source;
}

static void changeCellColor(HWND hwnd, COLORREF cref, UINT source, BOOL force) {
	ZHANDLE zhCstate;
	PCELLSTATE pCstate;

	zhCstate = (ZHANDLE) GetProp(hwnd, MAKEINTATOM(CELL_STATE_PROPERTY));
	pCstate = guiLockMem(zhCstate);
	if (pCstate == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}
	if (force || source >= pCstate->colorSource ) {
		pCstate->color = cref;
		pCstate->colorSource = source;
		InvalidateRect(hwnd, NULL, FALSE);
	}
	guiUnlockMem(zhCstate);
}

static LRESULT Table_OnCBDeleteString(HWND hwnd, INT position) {
	PTABLESTATE tblState;
	ZHANDLE zhTstate;
	CONTROL *control;
	UINT i1;
	HWND hwnd2;

	zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState == NULL) {
		sprintf(work, "guiLockMem returned null for tblState in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (tblState->columns[control->table.changeposition.x - 1].type == PANEL_CDROPBOX) {
		for (i1 = 0; i1 < control->table.numrows; i1++) {
			hwnd2 = GetCellAt(tblState, control->table.changeposition.x - 1, i1);
			SendMessage(hwnd2, CB_DELETESTRING, (WPARAM)(int)position, 0L);
			//ComboBox_DeleteString(hwnd2, position);
		}
	}
	else {
		hwnd2 = GetCellAt(tblState, control->table.changeposition.x - 1, control->table.changeposition.y - 1);
		SendMessage(hwnd2, CB_DELETESTRING, (WPARAM)(int)position, 0L);
		//ComboBox_DeleteString(hwnd2, position);
	}
	guiUnlockMem(zhTstate);
	return 0;
}

static LRESULT Table_OnCBGetCurSel(HWND hwnd) {
	PTABLESTATE tblState;
	ZHANDLE zhTstate;
	CONTROL *control;
	UINT column;
	HWND hwnd2;

	zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState == NULL) {
		sprintf(work, "guiLockMem returned null for tblState in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	column = control->table.changeposition.x - 1;

	hwnd2 = GetCellAt(tblState, column, control->table.changeposition.y - 1);
	return ComboBox_GetCurSel(hwnd2);
}

static LRESULT Table_OnCBSetCurSel(HWND hwnd, INT position) {
	PTABLESTATE tblState;
	ZHANDLE zhTstate;
	CONTROL *control;
	UINT i1, column;
	HWND hwnd2;

	zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState == NULL) {
		sprintf(work, "guiLockMem returned null for tblState in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	column = control->table.changeposition.x - 1;
	switch (tblState->columns[column].type) {

	case PANEL_CDROPBOX:
		if (control->table.changeposition.y < 1) {
			for (i1 = 0; i1 < control->table.numrows; i1++) {
				hwnd2 = GetCellAt(tblState, column, i1);
				SendMessage(hwnd2, CB_SETCURSEL, (WPARAM)(int)position, 0L);
				//ComboBox_SetCurSel(hwnd2, position);
			}
		}
		else {
			hwnd2 = GetCellAt(tblState, column, control->table.changeposition.y - 1);
			SendMessage(hwnd2, CB_SETCURSEL, (WPARAM)(int)position, 0L);
			//ComboBox_SetCurSel(hwnd2, position);
		}
		break;

	case PANEL_DROPBOX:
		hwnd2 = GetCellAt(tblState, column, control->table.changeposition.y - 1);
		SendMessage(hwnd2, CB_SETCURSEL, (WPARAM)(int)position, 0L);
		//ComboBox_SetCurSel(hwnd2, position);
		break;

	}
	guiUnlockMem(zhTstate);
	return 0;
}

static LRESULT Table_OnCBAddString(HWND hwnd, CHAR *string) {
	PTABLESTATE tblState;
	ZHANDLE zhTstate;
	CONTROL *control;
	UINT i1;
	HWND hwnd2;

	zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (tblState->columns[control->table.changeposition.x - 1].type == PANEL_CDROPBOX) {
		for (i1 = 0; i1 < control->table.numrows; i1++) {
			hwnd2 = GetCellAt(tblState, control->table.changeposition.x - 1, i1);
			SendMessage(hwnd2, CB_ADDSTRING, 0L, (LPARAM)(LPCSTR)string);
			//ComboBox_AddString(hwnd2, string);
		}
	}
	else {
		hwnd2 = GetCellAt(tblState, control->table.changeposition.x - 1, control->table.changeposition.y - 1);
		SendMessage(hwnd2, CB_ADDSTRING, 0L, (LPARAM)(LPCSTR)string);
		//ComboBox_AddString(hwnd2, string);
	}

	guiUnlockMem(zhTstate);
	return 0;
}

static LRESULT Table_OnCBInsertString(HWND hwnd, INT position, CHAR *string) {
	PTABLESTATE tblState;
	ZHANDLE zhTstate;
	CONTROL *control;
	UINT i1;
	HWND hwnd2;

	zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (tblState->columns[control->table.changeposition.x - 1].type == PANEL_CDROPBOX) {
		for (i1 = 0; i1 < control->table.numrows; i1++) {
			hwnd2 = GetCellAt(tblState, control->table.changeposition.x - 1, i1);
			SendMessage(hwnd2, CB_INSERTSTRING, (WPARAM)(int)position, (LPARAM)(LPCSTR)string);
			//ComboBox_InsertString(hwnd2, position, string);
		}
	}
	else {
		hwnd2 = GetCellAt(tblState, control->table.changeposition.x - 1, control->table.changeposition.y - 1);
		SendMessage(hwnd2, CB_INSERTSTRING, (WPARAM)(int)position, (LPARAM)(LPCSTR)string);
		//ComboBox_InsertString(hwnd2, position, string);
	}

	guiUnlockMem(zhTstate);
	return 0;
}

static LRESULT Table_OnCBResetContent(HWND hwnd) {
	PTABLESTATE tblState;
	ZHANDLE zhTstate;
	CONTROL *control;
	UINT i1;
	HWND hwnd2;

	zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (tblState->columns[control->table.changeposition.x - 1].type == PANEL_CDROPBOX) {
		for (i1 = 0; i1 < control->table.numrows; i1++) {
			hwnd2 = GetCellAt(tblState, control->table.changeposition.x - 1, i1);
			SendMessage(hwnd2, CB_RESETCONTENT, 0L, 0L);
			//ComboBox_ResetContent(hwnd2);
		}
	}
	else {
		hwnd2 = GetCellAt(tblState, control->table.changeposition.x - 1, control->table.changeposition.y - 1);
		SendMessage(hwnd2, CB_RESETCONTENT, 0L, 0L);
		//ComboBox_ResetContent(hwnd2);
	}

	guiUnlockMem(zhTstate);
	return 0;
}


static LRESULT Table_OnCreate(HWND hwnd, LPCREATESTRUCT cs) {

	CONTROL *control;
	INT size;
	ZHANDLE zhTstate;
	PTABLESTATE tblState;
	PTABLECOLUMN tblcolumns;
	LPCSTR columnstring;	// will hold dereferenced pointer to the column title
	SCROLLINFO si;
	RECT rect;
	UINT i1;
	HDLAYOUT hdl;
	WINDOWPOS wp;
	HDITEM hdi;

	// Get a pointer to the DX CONTROL structure associated with this table
	control = (CONTROL *) cs->lpCreateParams;

	// Allocate heap memory for state info and save the pointer to it
	size = sizeof(TABLESTATE) + (control->table.numcolumns - 1) * sizeof(struct columnsTag);
	zhTstate = guiAllocMem(size);
	if (zhTstate == NULL) {
		sprintf(work, "guiAllocMem failure in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return FALSE;
	}
	SetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET, (LONG_PTR) zhTstate);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState == NULL) {
		sprintf(work, "guiLockMem returned null for tblState in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	ZeroMemory(tblState, size);
	tblState->font = control->font;
	tblState->numcolumns = control->table.numcolumns;
	tblState->readonly = (control->style & CTRLSTYLE_READONLY) != 0;

	tblcolumns = (TABLECOLUMN *) guiLockMem(control->table.columns);
	if (tblcolumns == NULL) {
		sprintf(work, "guiLockMem returned null for tblcolumns in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}

	// As a starting point, set the scroll bars to inactive
	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_RANGE;
	si.nMin = si.nMax = 0;
	SetScrollInfo(hwnd, SB_VERT, &si, FALSE);
	SetScrollInfo(hwnd, SB_HORZ, &si, FALSE);
	tblState->hscroll = tblState->vscroll = FALSE;

	/* Set up the visible area */
	GetClientRect(hwnd, &rect);
	tblState->visible.cx = rect.right;
	tblState->visible.cy = rect.bottom;

	/* Calculate the total width of all the columns */
	/* And while we are here, fill in the column types */
	tblState->table.cx = 0;
	for (i1 = 0; i1 < control->table.numcolumns; i1++) {
		tblState->table.cx += tblcolumns[i1].width;
		tblState->columns[i1].columnWidth = tblcolumns[i1].width;
		tblState->columns[i1].type = tblcolumns[i1].type;
	}

	/* Create the header window */
#ifndef HDS_NOSIZING
#define HDS_NOSIZING      0x0800
#endif
#ifndef HDF_FIXEDWIDTH
#define HDF_FIXEDWIDTH    0x0100
#endif
	if (!control->table.noheader) {
		tblState->hwndHeader = CreateWindow(WC_HEADER,
			NULL, WS_CHILD | WS_VISIBLE | HDS_HORZ | HDS_NOSIZING  | HDS_BUTTONS,
			0, 0, 0, 0,
			hwnd, NULL, NULL, NULL);
		hdl.prc = &rect;
		hdl.pwpos = &wp;
		rect.right = max(tblState->table.cx, tblState->visible.cx);
		SendMessage(tblState->hwndHeader, WM_SETFONT, (WPARAM) tblState->font, (LPARAM) NULL);
		Header_Layout(tblState->hwndHeader, &hdl);
		SetWindowPos(tblState->hwndHeader, NULL, wp.x, wp.y, wp.cx, wp.cy, wp.flags | SWP_NOZORDER);
		tblState->headerCellHeight = wp.cy;
		tblState->cellHeight = tblState->headerCellHeight - 2;
		tblState->visible.cy -= wp.cy;
	}
	else {
		tblState->hwndHeader = CreateWindow(WC_HEADER,
			NULL, WS_CHILD | HDS_HORZ | HDS_NOSIZING  | HDS_BUTTONS,
			0, 0, 0, 0,
			hwnd, NULL, NULL, NULL);
		hdl.prc = &rect;
		hdl.pwpos = &wp;
		rect.right = max(tblState->table.cx, tblState->visible.cx);
		SendMessage(tblState->hwndHeader, WM_SETFONT, (WPARAM) tblState->font, (LPARAM) NULL);
		Header_Layout(tblState->hwndHeader, &hdl);
		tblState->cellHeight = wp.cy - 2;
		DestroyWindow(tblState->hwndHeader);
		tblState->hwndHeader = NULL;
	}

	/* Create the window to hold the cells */
	tblState->hwndCells = CreateWindow(szTableCellsClassName,
		NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
		0, tblState->headerCellHeight,
		max(tblState->table.cx, tblState->visible.cx), 0,
		hwnd, NULL, NULL, NULL);

	if (!control->table.noheader) {
		/* Insert the column titles into the header window */
		for (i1 = 0; i1 < control->table.numcolumns; i1++) {
			hdi.mask = HDI_FORMAT | HDI_TEXT | HDI_WIDTH;
			hdi.cxy = tblcolumns[i1].width;
			columnstring = guiLockMem(tblcolumns[i1].name);
			if (columnstring == NULL) {
				sprintf(work, "guiLockMem returned null for columnstring in %s", __FUNCTION__);
				guiaErrorMsg(work, i1);
				return FALSE;
			}
			hdi.pszText = (LPSTR) columnstring;
			hdi.cchTextMax = (int)strlen(hdi.pszText);
			hdi.fmt = HDF_CENTER | HDF_FIXEDWIDTH;
			Header_InsertItem(tblState->hwndHeader, i1, &hdi);
			guiUnlockMem(tblcolumns[i1].name);
		}
	}
	guiUnlockMem(control->table.columns);

	// If this table needs an hscroll, add it.
	if (tblState->table.cx > tblState->visible.cx) addHscrollBar(hwnd, tblState);

	// If any rows exist in the CONTROL structure, make them
	for (i1 = 0; i1 < control->table.numrows; i1++) addTableRow(tblState->hwndCells, control, tblState, i1);
	changeCellsWindowSize(tblState, (INT)(tblState->cellHeight * control->table.numrows));

	// If this table needs a vscroll, add one.
	if (!tblState->vscroll && tblState->table.cy > tblState->visible.cy) addVscrollBar(hwnd, tblState);

	guiUnlockMem(zhTstate);
	return TRUE;
}

COLORREF getBackgroundColor(HWND hwnd) {
	return isTableEnabled(hwnd) ? GetSysColor(COLOR_WINDOW) : GetSysColor(COLOR_BTNFACE);
}

HBRUSH getBackgroundBrush(HWND hwnd) {
	return isTableEnabled(hwnd) ? GetSysColorBrush(COLOR_WINDOW) : hbrButtonFace;
}

static int Table_OnEraseBkgnd(HWND hwnd, HDC dc) {
	RECT rect;

	GetClientRect(hwnd, &rect);
	FillRect(dc, &rect, getBackgroundBrush(hwnd));
	return 1;
}

static void Table_OnHscroll(HWND hwnd, HWND hwndScrollBar, UINT code, int position) {

	ZHANDLE zhTstate;
	PTABLESTATE tblState;
	SCROLLINFO si;
	INT xPos;

	zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState == NULL) {
		sprintf(work, "guiLockMem returned null for in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	GetScrollInfo(hwnd, SB_HORZ, &si);
	xPos = si.nPos;
	switch(code) {
	case SB_LINELEFT:
		si.nPos--;
		break;
	case SB_LINERIGHT:
		si.nPos++;
		break;
	case SB_PAGELEFT:
		si.nPos -= si.nPage;
		break;
	case SB_PAGERIGHT:
		si.nPos += si.nPage;
		break;
	case SB_THUMBTRACK:
		si.nPos = si.nTrackPos;
		break;
	}
	si.fMask = SIF_POS;
	SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
	GetScrollInfo(hwnd, SB_HORZ, &si);
	if (si.nPos != xPos) {
		moveCellsHorz(hwnd, tblState, xPos - si.nPos);
	}
	guiUnlockMem(zhTstate);
}

static void Table_OnKey(HWND hwnd, UINT vk, BOOL bKeyDown, int cRepeat, UINT flags) {

	int col, row, oldRow, oldColumn;
	ZHANDLE zhTstate;
	PTABLESTATE tblState;
	HWND hwnd2;
	CONTROL *control;
	LRESULT l1;
	BOOL shift, tabout;

	if (bKeyDown && (vk == VK_UP || vk == VK_DOWN) && GetKeyState(VK_CONTROL) & 0x8000) {
		/* If we get here, the focus is in a focusable column
		 * and we should move the focus up or down
		 */
		zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
		tblState = (PTABLESTATE) guiLockMem(zhTstate);
		if (tblState == NULL) {
			sprintf(work, "guiLockMem returned null for tblState in %s", __FUNCTION__);
			guiaErrorMsg(work, 1);
			return;
		}
		hwnd2 = GetFocus();
		getColumnRowFromWindow(hwnd2, tblState->hwndCells, &col, &row);
		oldRow = row;
		oldColumn = col;
		if (vk == VK_UP) {
			if (row == 0) return;
			else row--;
		}
		else row++;
		hwnd2 = GetCellAt(tblState, col, row);
		if (hwnd2 != NULL) {
			control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			SetFocus(hwnd2);
			ensureVisibility(hwnd, col, row);
		}
		guiUnlockMem(zhTstate);
	}
	else if (bKeyDown && (vk == VK_TAB || vk == VK_RETURN) && !(GetKeyState(VK_CONTROL) & 0x8000)) {

		/* If we get here, the focus is in a focusable column
		 * and we should move the focus left or right
		 */
		l1 = tabout = FALSE;
		shift = GetKeyState(VK_SHIFT) & 0x8000;
		zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
		tblState = (PTABLESTATE) guiLockMem(zhTstate);
		if (tblState == NULL) {
			sprintf(work, "guiLockMem returned null for tblState in %s", __FUNCTION__);
			guiaErrorMsg(work, 2);
			return;
		}
		if (IsChild(tblState->hwndCells, GetFocus())) {
			control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			getColumnRowFromWindow(GetFocus(), tblState->hwndCells, &col, &row);
			for (;;) {
				if (shift) {
					while (col > 0) {
						col--;
						if (ISCOLUMNFOCUSABLE(tblState->columns[col].type)) {
							SetFocus(GetCellAt(tblState, col, row));
							l1 = TRUE;
							break;
						}
					}
				}
				else {
					while (col < (int) tblState->numcolumns - 1) {
						col++;
						if (ISCOLUMNFOCUSABLE(tblState->columns[col].type)) {
							SetFocus(GetCellAt(tblState, col, row));
							l1 = TRUE;
							break;
						}
					}
				}
				if (l1) break;
				if (!shift) {
					if ((UINT) row < control->table.numrows - 1) {
						row++;
						col = -1;
					}
					else {
						tabout = TRUE;
						break;
					}
				}
				else {
					if (row > 0) {
						row--;
						col = tblState->numcolumns;
					}
					else {
						tabout = TRUE;
						break;
					}
				}
			}
		}
		guiUnlockMem(zhTstate);
		if (l1) ensureVisibility(hwnd, (UINT) col, (UINT) row);
		else if (tabout) guiaTabToControl(*control->res, control, !(GetKeyState(VK_SHIFT) & 0x8000));
	}
}

/*
 * When the table is being destroyed, we can get here after the WM_DESTROY message has been
 * processed. So we must check for a NULL table state pointer.
 *
 */
static void Table_OnSetFocus(HWND hwnd, HWND lastFocus) {

	ZHANDLE zhTstate;
	PTABLESTATE tblState;
	UINT i1;
	CONTROL *control;
	RESOURCE *res;

	/* Locate the upper left most cell that can have the focus */
	zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
	if (zhTstate == NULL) return;
	control = (CONTROL *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (control->table.numrows >= 1) {
		tblState = (PTABLESTATE) guiLockMem(zhTstate);
		if (tblState == NULL) {
			sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, control->table.numrows);
			return;
		}
		for (i1 = 0; i1 < tblState->numcolumns; i1++) {
			if (ISCOLUMNFOCUSABLE(tblState->columns[i1].type)) {
				SetFocus(GetCellAt(tblState, i1, 0));
				ensureVisibility(hwnd, i1, 0);
				break;
			}
		}
		guiUnlockMem(zhTstate);
	}

	/**
	 * Whenever a cell in a table has the focus, we tell the DX system that the table itself
	 * has the focus.
	 * This is necessary so that when the focus switches to another app and back, we
	 * will reset the focus to the correct cell.
	 */
	pvistart();
	res = *control->res;
	res->ctlfocus = control;
	pviend();
}

static void Table_OnVscroll(HWND hwnd, HWND hwndScrollBar, UINT code, int position) {

	ZHANDLE zhTstate;
	PTABLESTATE tblState;
	SCROLLINFO si;
	INT yPos;

	zhTstate = (ZHANDLE) GetWindowLongPtr(hwnd, TBLSTATESTRUCT_OFFSET);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState == NULL) {
		sprintf_s(work, ARRAYSIZE(work), "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	GetScrollInfo(hwnd, SB_VERT, &si);
	yPos = si.nPos;
	switch(code) {
	case SB_LINEUP:
		si.nPos -= tblState->cellHeight;
		break;
	case SB_LINEDOWN:
		si.nPos += tblState->cellHeight;
		break;
	case SB_PAGEUP:
		si.nPos -= si.nPage;
		break;
	case SB_PAGEDOWN:
		si.nPos += si.nPage;
		break;
	case SB_THUMBTRACK:
		si.nPos = si.nTrackPos;
		break;
	}
	si.fMask = SIF_POS;
	SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
	GetScrollInfo(hwnd, SB_VERT, &si);
	if (si.nPos != yPos) {
		moveCellsVert(hwnd, tblState, yPos - si.nPos);
	}
	guiUnlockMem(zhTstate);
}


static void TableCheckVScrollBar(HWND hwnd, PTABLESTATE tblState) {
	SCROLLINFO si;

	/* If we have a vertical scroll bar, do we still need it? */
	if (tblState->vscroll) {
		if (tblState->table.cy <= tblState->visible.cy) removeVscrollBar(hwnd, tblState);
		else {
			si.cbSize = sizeof(si);
			si.fMask = SIF_RANGE;
			si.nMin = 0;
			si.nMax = tblState->table.cy;
			SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
		}
	}
	/* If we do not have one, we might need it now? */
	else if (tblState->table.cy > tblState->visible.cy) addVscrollBar(hwnd, tblState);

}

static void TableCheckHScrollBar(HWND hwnd, PTABLESTATE tblState) {
	SCROLLINFO si;

	/* If we have a horizontal scroll bar, do we still need it? */
	if (tblState->hscroll) {
		if (tblState->table.cx <= tblState->visible.cx) removeHscrollBar(hwnd, tblState);
		else {
			si.cbSize = sizeof(si);
			si.fMask = SIF_RANGE;
			si.nMin = 0;
			si.nMax = tblState->table.cx;
			SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
		}
	}
	/* If we do not have one, we might need it now? */
	else if (tblState->table.cx > tblState->visible.cx) addHscrollBar(hwnd, tblState);

}

/*
 * Changes the value of tblState->table.cy
 * Does not move the cells window
 */
static void changeCellsWindowSize(PTABLESTATE tblState, INT amount) {
	tblState->table.cy += amount;
	SetWindowPos(tblState->hwndCells, NULL, 0, 0, max(tblState->table.cx, tblState->visible.cx), tblState->table.cy,
		SWP_NOMOVE | SWP_NOZORDER);
}

static void removeVscrollBar(HWND hwnd, PTABLESTATE tblState) {
	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE;
	si.nMin = si.nMax = 0;
	SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
	tblState->vscroll = FALSE;
	tblState->visible.cx += VScrollBarWidth;
	if (tblState->hscroll) {
		if (tblState->table.cx <= tblState->visible.cx) {
			SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
		}
		else {
			si.fMask = SIF_PAGE;
			si.nPage = tblState->visible.cx;
			SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
		}
	}
}

static void removeHscrollBar(HWND hwnd, PTABLESTATE tblState) {
	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE;
	si.nMin = si.nMax = 0;
	SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
	tblState->hscroll = FALSE;
	tblState->visible.cy += HScrollBarHeight;
	if (tblState->vscroll) {
		if (tblState->table.cy <= tblState->visible.cy) {
			SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
		}
		else {
			si.fMask = SIF_PAGE;
			si.nPage = tblState->visible.cy;
			SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
		}
	}
}

/**
 * Return the window handle of the cell at (zero-based) column and row
 */
static HWND GetCellAt(PTABLESTATE tblState, UINT column, UINT row) {
	POINT pnt;
	HWND hwnd;
	GetPointFromColumnRow(tblState, column, row, &pnt);
	hwnd = ChildWindowFromPoint(tblState->hwndCells, pnt);
	return (hwnd == tblState->hwndCells) ? NULL : hwnd;
}

/**
 * Given a zero-based row and column, calculate the pixel position of the upper left corner
 */
static void GetPointFromColumnRow(PTABLESTATE tblState, UINT column, UINT row, POINT *pnt) {
	UINT i1;
	pnt->y = row * tblState->cellHeight;
	for (i1 = 0, pnt->x = 0; i1 < column; i1++) pnt->x += tblState->columns[i1].columnWidth;
	if (column) pnt->x++;
}

/**
 * Given a Point relative to the Cells window, calculate the zero-based row and column
 */
static void GetColumnRowFromPoint(PTABLESTATE tblState, int *column, int *row, POINT pnt) {
	UINT i1, i2;
	*row = 0;
	while (pnt.y >= tblState->cellHeight) {
		(*row)++;
		pnt.y -= tblState->cellHeight;
	}
	i1 = 0;
	i2 = tblState->columns[i1].columnWidth;
	while ((UINT) pnt.x >= i2) {
		i1++;
		i2 += tblState->columns[i1].columnWidth;
	}
	*column = i1;
}

static void addHscrollBar(HWND hwnd, PTABLESTATE tblState) {
	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE;
	si.nMin = 0;
	si.nMax = tblState->table.cx;
	SetScrollInfo(hwnd, SB_HORZ, &si, FALSE);
	tblState->hscroll = TRUE;
	tblState->visible.cy -= HScrollBarHeight;
	/* If there was no vscroll before, we might need one now */
	if (!tblState->vscroll) {
		if (tblState->table.cy > tblState->visible.cy) addVscrollBar(hwnd, tblState);
	}
	else {
		si.fMask = SIF_PAGE;
		si.nPage = tblState->visible.cy;
		SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
	}
	si.fMask = SIF_PAGE;
	si.nPage = tblState->visible.cx;
	SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
}


static void addVscrollBar(HWND hwnd, PTABLESTATE tblState) {
	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE;
	si.nMin = 0;
	si.nMax = tblState->table.cy;
	SetScrollInfo(hwnd, SB_VERT, &si, FALSE);
	tblState->vscroll = TRUE;
	tblState->visible.cx -= VScrollBarWidth;
	/* If there was no hscroll before, we might need one now */
	if (!tblState->hscroll) {
		if (tblState->table.cx > tblState->visible.cx) addHscrollBar(hwnd, tblState);
	}
	else {
		si.fMask = SIF_PAGE;
		si.nPage = tblState->visible.cx;
		SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
	}
	si.fMask = SIF_PAGE;
	si.nPage = tblState->visible.cy;
	SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

/**
 * hwnd is a handle to a window of class szTableCellsClassName
 *
 * rownum is zero-based
 */
static void addTableRow(HWND hwnd, CONTROL *control, PTABLESTATE pTableState, UINT rownum) {
	UINT column, i2;
	int hpos;
	HWND hwnd2;
	WNDPROC wpOld;
	ZHANDLE zhRows, zhState;
	PCELLSTATE pCstate;
	PTABLECOLUMN columns;
	PTABLEROW tblrows;
	PTABLEDROPBOX dropbox;
	CHAR *pchar = NULL;
	SIZE szCell;
	POINT ptCell;
	DWORD winstyle;
	PTABLECELL cell;
	ZHANDLE *zhArray;

	hpos = 0;
	columns = (PTABLECOLUMN) guiLockMem(control->table.columns);
	if (columns == NULL) {
		sprintf(work, "guiLockMem returned null for columns in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}
	zhRows = guiTableLocateRow(control, rownum + 1);
	tblrows = (PTABLEROW) guiLockMem(zhRows);
	if (tblrows == NULL) {
		sprintf(work, "guiLockMem returned null for tblrows in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}

	ptCell.y = rownum * pTableState->cellHeight;
	szCell.cy = pTableState->cellHeight - 1;
	for (column = 0; column < control->table.numcolumns; column++) {
		zhState = guiAllocMem(sizeof(CELLSTATE));
		if (zhState == NULL) {
			sprintf(work, "Out of guiAllocMem memory in %s", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return;
		}
		pCstate = guiLockMem(zhState);
		if (pCstate == NULL) {
			sprintf(work, "guiLockMem returned null for pCstate in %s", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return;
		}
		ZeroMemory(pCstate, sizeof(CELLSTATE));
		pCstate->colIndex = column;
		pCstate->colorSource = TABLE_COLORSOURCE_DEFAULT;
		szCell.cx = columns[column].width;
		ptCell.x = hpos;
		if (column) {
			ptCell.x++;
			szCell.cx--;
		}
		cell = &tblrows->cells[column];
		if (cell->textcolor.code == 1) {
			pCstate->color = cell->textcolor.color;
			pCstate->colorSource = TABLE_COLORSOURCE_CELL;
		}
		else if (cell->textcolor.color != TEXTCOLOR_DEFAULT) {
			pCstate->color = GetColorefFromColorNumber(cell->textcolor.color);
			pCstate->colorSource = TABLE_COLORSOURCE_CELL;
		}
		else if (tblrows->textcolor.code == 1) {
			pCstate->color = tblrows->textcolor.color;
			pCstate->colorSource = TABLE_COLORSOURCE_ROW;
		}
		else if (tblrows->textcolor.color != TEXTCOLOR_DEFAULT) {
			pCstate->color = GetColorefFromColorNumber(tblrows->textcolor.color);
			pCstate->colorSource = TABLE_COLORSOURCE_ROW;
		}
		else if (columns[column].textcolor.code == 1) {
			pCstate->color = columns[column].textcolor.color;
			pCstate->colorSource = TABLE_COLORSOURCE_COLUMN;
		}
		else if (columns[column].textcolor.color != TEXTCOLOR_DEFAULT) {
			pCstate->color = GetColorefFromColorNumber(columns[column].textcolor.color);
			pCstate->colorSource = TABLE_COLORSOURCE_COLUMN;
		}
		else if (control->table.textcolor.code == 1) {
			pCstate->color = control->table.textcolor.color;
			pCstate->colorSource = TABLE_COLORSOURCE_TABLE;
		}
		else if (control->table.textcolor.color != TEXTCOLOR_DEFAULT) {
			pCstate->color = GetColorefFromColorNumber(control->table.textcolor.color);
			pCstate->colorSource = TABLE_COLORSOURCE_TABLE;
		}
		pCstate->type = columns[column].type;
		switch (pCstate->type) {
		case PANEL_VTEXT:
		case PANEL_RVTEXT:
		case PANEL_CVTEXT:
			if (cell->text == NULL) pchar = NULL;
			else {
				pchar = guiLockMem(cell->text);
				if (pchar == NULL) {
					sprintf(work, "guiLockMem returned null for pchar[TEXT] in %s", __FUNCTION__);
					guiaErrorMsg(work, 0);
					return;
				}
			}
			winstyle = WS_CHILD | WS_VISIBLE;
			hwnd2 = CreateWindow(WC_STATIC,
				(LPCSTR) pchar,
				winstyle,
				ptCell.x, ptCell.y,
				szCell.cx, szCell.cy,
				hwnd, NULL, NULL, 0
			);
			if (cell->text != NULL) guiUnlockMem(cell->text);
			if (hwnd2 == NULL) {
				guiaErrorMsg("Create vtext/rvtext table cell error", GetLastError());
				return;
			}
			SetProp(hwnd2, MAKEINTATOM(CELL_STATE_PROPERTY), zhState);
			pCstate->font = pTableState->font;
			guiUnlockMem(zhState);
			wpOld = (WNDPROC) SetWindowLongPtr(hwnd2, GWLP_WNDPROC, (LONG_PTR) TableStaticCellProc);
			SetProp(hwnd2, MAKEINTATOM(ORIG_STATIC_WNDPROC), wpOld);
			break;

		case PANEL_CHECKBOX:
			if (!dxcheckboxclassregistered) {
				guiaRegisterDXCheckbox(guia_hinst());
				dxcheckboxclassregistered = TRUE;
			}
			winstyle = WS_CHILD | WS_VISIBLE;
			hwnd2 = CreateWindow(szDXCheckboxClassName,
				(LPCSTR) NULL,
				winstyle,
				ptCell.x, ptCell.y,
				szCell.cx, szCell.cy,
				hwnd, NULL, NULL, (LPVOID) (UINT_PTR) (cell->style & CTRLSTYLE_MARKED)
			);
			guiUnlockMem(zhState);
			if (hwnd2 == NULL) {
				guiaErrorMsg("Create checkbox table cell error", GetLastError());
				return;
			}
			SetProp(hwnd2, MAKEINTATOM(CELL_STATE_PROPERTY), zhState);
			break;

		case PANEL_POPBOX:
			if (cell->text == NULL) pchar = NULL;
			else {
				pchar = guiLockMem(cell->text);
				if (pchar == NULL) {
					sprintf(work, "guiLockMem returned null for pchar[POPBOX] in %s", __FUNCTION__);
					guiaErrorMsg(work, 0);
					return;
				}
			}
			hwnd2 = CreateWindow(szPopboxClassName,
				(LPCSTR) pchar,
				WS_CHILD | WS_VISIBLE,
				ptCell.x, ptCell.y,
				szCell.cx, szCell.cy,
				hwnd, NULL, NULL, NULL
			);
			if (cell->text != NULL) guiUnlockMem(cell->text);
			guiUnlockMem(zhState);
			if (hwnd2 == NULL) {
				guiaErrorMsg("Create popbox table cell error", GetLastError());
				return;
			}
			SetProp(hwnd2, MAKEINTATOM(CELL_STATE_PROPERTY), zhState);
			SendMessage(hwnd2, WM_SETFONT, (WPARAM) pTableState->font, MAKELPARAM(TRUE, 0));
			break;

		case PANEL_EDIT:
		case PANEL_LEDIT:
		case PANEL_FEDIT:
			if (cell->text == NULL) pchar = NULL;
			else {
				pchar = guiLockMem(cell->text);
				if (pchar == NULL) {
					sprintf(work, "guiLockMem returned null for pchar[EDIT] in %s", __FUNCTION__);
					guiaErrorMsg(work, 0);
					return;
				}
			}
			winstyle = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;
			if (columns[column].justify == CTRLSTYLE_LEFTJUSTIFIED) winstyle |= ES_LEFT;
			else if (columns[column].justify == CTRLSTYLE_RIGHTJUSTIFIED) winstyle |= ES_RIGHT;
			else if (columns[column].justify == CTRLSTYLE_CENTERJUSTIFIED) winstyle |= ES_CENTER;
			hwnd2 = CreateWindow(WC_EDIT,
				(LPCSTR) pchar,
				winstyle,
				ptCell.x, ptCell.y,
				szCell.cx, szCell.cy,
				hwnd, NULL, NULL, 0
			);
			if (cell->text != NULL) guiUnlockMem(cell->text);
			guiUnlockMem(zhState);
			if (hwnd2 == NULL) {
				guiaErrorMsg("Create edit table cell error", GetLastError());
				return;
			}
			SetProp(hwnd2, MAKEINTATOM(CELL_STATE_PROPERTY), zhState);
			SendMessage(hwnd2, WM_SETFONT, (WPARAM) pTableState->font, MAKELPARAM(TRUE, 0));

			wpOld = (WNDPROC) SetWindowLongPtr(hwnd2, GWLP_WNDPROC, (LONG_PTR) TableEditCellProc);
			SetProp(hwnd2, MAKEINTATOM(ORIG_EDIT_WNDPROC), wpOld);

			if (columns[column].type == PANEL_LEDIT) {
				SendMessage(hwnd2, EM_LIMITTEXT, (WPARAM) columns[column].maxchars, 0);
			}
			else if (columns[column].type == PANEL_FEDIT) {
				pchar = guiLockMem(columns[column].mask);
				if (pchar == NULL) {
					sprintf(work, "guiLockMem returned null for pchar[FEDIT] in %s", __FUNCTION__);
					guiaErrorMsg(work, 0);
					return;
				}
				SendMessage(hwnd2, EM_LIMITTEXT, (WPARAM) strlen(pchar), 0);
				guiUnlockMem(columns[column].mask);
			}
			break;

		case PANEL_CDROPBOX:
		case PANEL_DROPBOX:
			if (!dxdropboxclassregistered) {
				guiaRegisterDXDropbox(guia_hinst());
				dxdropboxclassregistered = TRUE;
			}
			winstyle = WS_CHILD | WS_VISIBLE;
			if (!columns[column].insertorder) winstyle |= CBS_SORT;
			hwnd2 = CreateWindow(szDXDropboxClassName,
				(LPCSTR) NULL,
				winstyle,
				ptCell.x, ptCell.y,
				szCell.cx, szCell.cy,
				hwnd, NULL, NULL,
				(LPVOID) &columns[column]
			);
			guiUnlockMem(zhState);
			if (hwnd2 == NULL) {
				guiaErrorMsg("Create dropbox table cell error", GetLastError());
				return;
			}
			SetProp(hwnd2, MAKEINTATOM(CELL_STATE_PROPERTY), zhState);
			SetWindowFont(hwnd2, (WPARAM) pTableState->font, MAKELPARAM(FALSE, 0)); // @suppress("Symbol is not resolved")

			// Fill in any data already in the column structure
			if (columns[column].type == PANEL_CDROPBOX) {
				dropbox = &columns[column].db;
			}
			else if (columns[column].type == PANEL_DROPBOX) {
				dropbox = &cell->dbcell.db;
			}
			if (dropbox->itemarray != NULL) {
				zhArray = guiLockMem(dropbox->itemarray);
				if (zhArray == NULL) {
					sprintf(work, "guiLockMem returned null for zhArray[[C]DROPBOX] in %s", __FUNCTION__);
					guiaErrorMsg(work, 0);
					return;
				}
				for (i2 = 0; i2 < dropbox->itemcount; i2++) {
					pchar = guiLockMem(zhArray[i2]);
					if (pchar == NULL) {
						sprintf(work, "guiLockMem returned null for pchar[[C]DROPBOX] in %s", __FUNCTION__);
						guiaErrorMsg(work, i2);
						return;
					}
					SendMessage(hwnd2, CB_INSERTSTRING, (WPARAM)(int)i2, (LPARAM)(LPCSTR)pchar);
					//ComboBox_InsertString(hwnd2, i2, pchar);
					guiUnlockMem(zhArray[i2]);
				}
				guiUnlockMem(dropbox->itemarray);
				if (cell->dbcell.itemslctd > 0) {
					SendMessage(hwnd2, CB_SETCURSEL, (WPARAM)(int)cell->dbcell.itemslctd - 1, 0L);
					//ComboBox_SetCurSel(hwnd2, cell->dbcell.itemslctd - 1);
				}
			}
			break;

		}
		hpos += columns[column].width;
	}

	guiUnlockMem(control->table.columns);
	guiUnlockMem(zhRows);
}

/**
 * This function subclasses the WC_EDIT cells.
 */
LRESULT CALLBACK TableEditCellProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND hwndCells, hwndTable;
	ZHANDLE zhTstate;
	PTABLESTATE tblState;
	WNDPROC wpOld = GetProp(hwnd, MAKEINTATOM(ORIG_EDIT_WNDPROC));

	switch (message) {

	case WM_KEYDOWN:
		if (wParam == VK_TAB || wParam == VK_RETURN
			|| ((wParam == VK_UP || wParam == VK_DOWN) && GetKeyState(VK_CONTROL) & 0x8000))
		{
			hwndCells = GetParent(hwnd);
			hwndTable = GetParent(hwndCells);
			if (hwndTable != NULL) FORWARD_WM_KEYDOWN(hwndTable, wParam, (int)(short)LOWORD(lParam),
					(UINT)HIWORD(lParam), PostMessage);
		}
		break;

	case WM_CHAR:
		if (wParam == VK_TAB || wParam == VK_RETURN) return 0;
		hwndCells = GetParent(hwnd);
		hwndTable = GetParent(hwndCells);
		zhTstate = (ZHANDLE) GetWindowLongPtr(hwndTable, TBLSTATESTRUCT_OFFSET);
		tblState = (PTABLESTATE) guiLockMem(zhTstate);
		if (tblState->readonly) {
			if (isprint((int)wParam) || wParam == VK_BACK) {
				guiUnlockMem(zhTstate);
				return 0;
			}
		}
		guiUnlockMem(zhTstate);
		break;

	case WM_DESTROY:
		RemoveProp(hwnd, MAKEINTATOM(ORIG_EDIT_WNDPROC));
		SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR) wpOld);
		break;

	case WM_SETFOCUS:
	case WM_KILLFOCUS:
		guiaHandleTableFocusMessage(hwnd, message, FALSE);
		break;

	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
		if (!handleTableRightButtonClick(hwnd, message, lParam, wParam)) return 0;
		break;
	}

	return CallWindowProc(wpOld, hwnd, message, wParam, lParam);
}

/**
 * We check the resource 'rightclick' flag at this level and don't
 * bother the table window process if it is off.
 */
int handleTableRightButtonClick(HWND hwnd, UINT message, LPARAM lParam, WPARAM wParam)
{
	HWND tableHwnd;
	HWND tempHwnd = hwnd;
	WINDOWINFO wi;
	CONTROL *control;
	UCHAR rclick;
	RESOURCE *res;
	POINT pt;

	/*
	 * Go up the chain of parent HWNDs until the table
	 * is found.
	 */
	wi.cbSize = sizeof(WINDOWINFO);
	while ((tempHwnd = GetAncestor(tempHwnd, GA_PARENT)) != NULL) {
		GetWindowInfo(tempHwnd, &wi);
		if (wi.atomWindowType == TableClassAtom) break;
	}
	if (tempHwnd == NULL) return 1;
	tableHwnd = tempHwnd;

	control = (CONTROL *) GetWindowLongPtr(tableHwnd, GWLP_USERDATA);

	pvistart();
	res = *control->res;
	rclick = res->rclickmsgs;
	pviend();
	if (!rclick) return 1;

	pt.x = GET_X_LPARAM(lParam);
	pt.y = GET_Y_LPARAM(lParam);
	ClientToScreen(hwnd, &pt);
	ScreenToClient(tableHwnd, &pt);
	PostMessage(tableHwnd, message, wParam, MAKELPARAM(pt.x, pt.y));
	return 0;
}

/**
 * This function subclasses the WC_STATIC windows that are used for cells of VTEXT, CVTEXT, and RVTEXT types.
 */
LRESULT CALLBACK TableStaticCellProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC dc;
	HFONT hOldFont;
	RECT rect;
	PAINTSTRUCT ps;
	ZHANDLE zhCstate;
	PCELLSTATE pCstate;
	int tlen;
	CHAR txtbuf[260];
	UINT uFormat;
	WNDPROC wpOld;

	wpOld = GetProp(hwnd, MAKEINTATOM(ORIG_STATIC_WNDPROC));

	switch (message) {

	case WM_PAINT:
		zhCstate = (ZHANDLE) GetProp(hwnd, MAKEINTATOM(CELL_STATE_PROPERTY));
		pCstate = guiLockMem(zhCstate);
		if (pCstate == NULL) {
			sprintf(work, "guiLockMem returned null in %s, WM_PAINT", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		GetClientRect(hwnd, &rect);
		dc = BeginPaint(hwnd, &ps);
		FillRect(dc, &rect, getBackgroundBrush(hwnd));
		switch (pCstate->type) {

		case PANEL_VTEXT:
		case PANEL_RVTEXT:
		case PANEL_CVTEXT:
			SetBkMode(dc, TRANSPARENT);
			SetTextColor(dc, isTableEnabled(hwnd) ? pCstate->color : GetSysColor(COLOR_GRAYTEXT));
			uFormat = DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX;
			if (pCstate->type == PANEL_RVTEXT) uFormat |= DT_RIGHT;
			else if (pCstate->type == PANEL_CVTEXT) uFormat |= DT_CENTER;
			tlen = GetWindowTextLength(hwnd);
			if (tlen > 0) {
				GetWindowText(hwnd, txtbuf, sizeof(txtbuf));
				hOldFont = SelectObject(dc, pCstate->font);
				rect.left += 6;
				rect.right -= 4;
				DrawText(dc, txtbuf, tlen, &rect, uFormat);
				if (hOldFont != NULL) SelectObject(dc, hOldFont);
			}
			break;
		}
		EndPaint(hwnd, &ps);
		guiUnlockMem(zhCstate);
		return 0;

	case WM_DESTROY:
		SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR) wpOld);
		RemoveProp(hwnd, MAKEINTATOM(ORIG_STATIC_WNDPROC));
		break;

	}
	return CallWindowProc(wpOld, hwnd, message, wParam, lParam);
}


INT guiaGetTableCellText(CONTROL *control, CHAR *data, INT datalen) {

	PTABLESTATE tblState;
	HWND hwnd2;
	ZHANDLE zhTstate;

	if (control->ctlhandle == NULL) return 0;	/* Might happen if table is on a never-seen-yet tab */
	zhTstate = (ZHANDLE) GetWindowLongPtr(control->ctlhandle, TBLSTATESTRUCT_OFFSET);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState == NULL) {
		sprintf(work, "guiLockMem returned null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	hwnd2 = GetCellAt(tblState, control->table.changeposition.x - 1, control->table.changeposition.y - 1);
	guiUnlockMem(zhTstate);
	if (hwnd2 == NULL) return 0;
	return guiaGetWinText(hwnd2, data, datalen);
}

INT guiaGetTableCellSelection(CONTROL *control) {

	INT i1 = ComboBox_GetCurSel(control->ctlhandle);
	return (i1 == CB_ERR) ? 0 : i1 + 1;
}

/**
 * We must find all edit cells and update the internal text.
 * This is called on the Windows message thread from hidecontrols() over in guiawin.c
 */
void hidetablecontrols(CONTROL *control)
{
	PTABLECOLUMN columns;
	PTABLEROW tblrows;
	UINT i1, i2;
	PTABLESTATE tblState;
	HWND hwnd2;
	INT textlen;
	ZHANDLE zhRows, zhTstate;
	CHAR *text;
	LRESULT l1;

	columns = guiLockMem(control->table.columns);
	if (columns == NULL) {
		sprintf(work, "guiLockMem returned null for columns in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}
	zhTstate = (ZHANDLE) GetWindowLongPtr(control->ctlhandle, TBLSTATESTRUCT_OFFSET);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState == NULL) {
		sprintf(work, "guiLockMem returned null for tblState in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}
	for (i1 = 0; i1 < control->table.numrows; i1++) {
		zhRows = guiTableLocateRow(control, i1 + 1);
		tblrows = guiLockMem(zhRows);
		if (tblrows == NULL) {
			sprintf(work, "guiLockMem returned null for tblrows in %s", __FUNCTION__);
			guiaErrorMsg(work, i1);
			return;
		}
		for (i2 = 0; i2 < control->table.numcolumns; i2++) {
			switch (columns[i2].type) {
			case PANEL_EDIT:
			case PANEL_LEDIT:
				hwnd2 = GetCellAt(tblState, i2, i1);
				textlen = GetWindowTextLength(hwnd2);
				if (textlen == 0) {
					if (tblrows->cells[i2].text != NULL) {
						guiFreeMem(tblrows->cells[i2].text);
						tblrows->cells[i2].text = NULL;
					}
				}
				else {
					textlen++;	/* account for the terminator */
					if (tblrows->cells[i2].text != NULL) {
						tblrows->cells[i2].text = guiReallocMem(tblrows->cells[i2].text, textlen * sizeof(CHAR));
						if (tblrows->cells[i2].text == NULL) {
							sprintf(work, "guiReallocMem returned null for text[L]EDIT in %s", __FUNCTION__);
							guiaErrorMsg(work, textlen * sizeof(CHAR));
							return;
						}
					}
					else {
						tblrows->cells[i2].text = guiAllocMem(textlen * sizeof(CHAR));
						if (tblrows->cells[i2].text == NULL) {
							sprintf(work, "guiAllocMem returned null for text[L]EDIT in %s", __FUNCTION__);
							guiaErrorMsg(work, textlen * sizeof(CHAR));
							return;
						}
					}

					text = guiLockMem(tblrows->cells[i2].text);
					if (text == NULL) {
						sprintf(work, "guiLockMem returned null for text[L]EDIT in %s", __FUNCTION__);
						guiaErrorMsg(work, i2);
						return;
					}
					GetWindowText(hwnd2, text, textlen);
					guiUnlockMem(tblrows->cells[i2].text);
				}
				break;
			case PANEL_CDROPBOX:
			case PANEL_DROPBOX:
				hwnd2 = GetCellAt(tblState, i2, i1);
				l1 = ComboBox_GetCurSel(hwnd2);
				tblrows->cells[i2].dbcell.itemslctd = (l1 == CB_ERR) ? 0 : (USHORT) (l1 + 1);
				break;
			}
		}
		guiUnlockMem(zhRows);
	}
	guiUnlockMem(zhTstate);
	guiUnlockMem(control->table.columns);
}

