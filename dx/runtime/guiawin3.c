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

#define GUIAWIN3
#define INC_STDIO
#define INC_STDLIB
#include "includes.h"
#include "base.h"
#include "gui.h"
#include "guix.h"
#include <windowsx.h>
#include <vssym32.h>
#include <uxtheme.h>

static COLORREF black = RGB(0, 0, 0);
static COLORREF white = RGB(255, 255, 255);
static WNDCLASS wndclass;
static CHAR work[128];

#ifndef STATE_SYSTEM_PRESSED
#define STATE_SYSTEM_PRESSED 0x00000008
#endif


/***********************************************************************************************************************
 *
 *
 * DXDropbox
 *
 *
 */


#define DBSTATESTRUCT_OFFSET  0
#define LISTBOX_WNDPROC_PROPERTY_STRING "com.dbcsoftware.dbcdx.LISTBOX_WNDPROC_HANDLE"
#define DROPBOX_HWND_PROPERTY_STRING "com.dbcsoftware.dbcdx.DROPBOX_HWND_HANDLE"


typedef struct tagDropboxState {
	DWORD stateButton;
	HWND hwndList;
	USHORT usDropboxHeight;
	USHORT selectedItem;
} DROPBOXSTATE, *PDROPBOXSTATE;

static ATOM ORIG_LISTBOX_WNDPROC;
static ATOM DROPBOX_HWND;
static void DBDrawDownArrow(HDC dc, RECT rect);
static void DBDrawFocusRect(HDC dc, RECT rect);
LRESULT CALLBACK DXDropboxProc (HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK DXListBoxProc (HWND, UINT, WPARAM, LPARAM);
static LRESULT DropBox_OnCreate(HWND hwnd, LPCREATESTRUCT cs);
static void DropBox_OnDestroy(HWND hwnd);
static LRESULT DropBox_OnGetText(HWND hwnd, int iCch, LPSTR buffer);
static void DropBox_OnKey(HWND hwnd, UINT vk, BOOL bKeyDown, int cRepeat, UINT flags);
static void DropBox_OnMouseMove(HWND hwnd, int x, int y, UINT uiKeyFlags);
static void DropBox_OnLButtonDown(HWND hwnd, BOOL bDblClk, int x, int y, UINT uiKeyFlags);
static void DropBox_OnLButtonUp(HWND hwnd, int x, int y, UINT uiKeyFlags);
static void DropBox_OnPaint(HWND hwnd);
static void DropBox_OnSetFont(HWND hwnd, HFONT font, BOOL redraw);
static INT buttonWidth = 13;

void guiaRegisterDXDropbox(HINSTANCE hInstance)
{
	wndclass.style         = CS_OWNDC;
	wndclass.lpfnWndProc   = DXDropboxProc ;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = sizeof(LONG_PTR);
	wndclass.hInstance     = hInstance ;
	wndclass.hIcon         = NULL;
	wndclass.hCursor       = LoadCursor (NULL, IDC_ARROW) ;
	wndclass.hbrBackground = NULL;
	wndclass.lpszMenuName  = NULL ;
	wndclass.lpszClassName = szDXDropboxClassName;
	if (!RegisterClass(&wndclass)) {
		sprintf(work, "%s failed, e=%d", __FUNCTION__, (int) GetLastError());
		MessageBox (NULL, work, szDXDropboxClassName, MB_ICONERROR);
	}

	ORIG_LISTBOX_WNDPROC = GlobalAddAtom(LISTBOX_WNDPROC_PROPERTY_STRING);
	DROPBOX_HWND = GlobalAddAtom(DROPBOX_HWND_PROPERTY_STRING);
	return;
}


LRESULT CALLBACK DXDropboxProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	RECT rect;
	PDROPBOXSTATE ctrlState;
	ZHANDLE zhState;
	INT i1, LBHeight;
	LRESULT l1;

	switch (message) {

	HANDLE_MSG(hwnd, WM_CREATE, DropBox_OnCreate);
	HANDLE_MSG(hwnd, WM_DESTROY, DropBox_OnDestroy);
	HANDLE_MSG(hwnd, WM_GETTEXT, DropBox_OnGetText);
	HANDLE_MSG(hwnd, WM_KEYDOWN, DropBox_OnKey);
	HANDLE_MSG(hwnd, WM_LBUTTONDOWN, DropBox_OnLButtonDown);
	HANDLE_MSG(hwnd, WM_LBUTTONUP, DropBox_OnLButtonUp);
	HANDLE_MSG(hwnd, WM_MOUSEMOVE, DropBox_OnMouseMove);
	HANDLE_MSG(hwnd, WM_PAINT, DropBox_OnPaint);
	HANDLE_MSG(hwnd, WM_SETFONT, DropBox_OnSetFont); // @suppress("Symbol is not resolved")

	case WM_SETFOCUS:
	case WM_KILLFOCUS:
		GetClientRect(hwnd, &rect);
		InflateRect(&rect, -1, -1);
		rect.right -= buttonWidth;
		InvalidateRect(hwnd, &rect, FALSE);
		guiaHandleTableFocusMessage(hwnd, message, FALSE);
		return 0;

	case CB_ADDSTRING:
		zhState = (ZHANDLE) GetWindowLongPtr(hwnd, DBSTATESTRUCT_OFFSET);
		ctrlState = (PDROPBOXSTATE) guiLockMem(zhState);
		if (ctrlState == NULL) {
			sprintf(work, "guiLockMem returned Null in %s, CB_ADDSTRING", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		(void)ListBox_AddString(ctrlState->hwndList, lParam);
		guiUnlockMem(zhState);
		return 0;

	case CB_INSERTSTRING:
		zhState = (ZHANDLE) GetWindowLongPtr(hwnd, DBSTATESTRUCT_OFFSET);
		ctrlState = (PDROPBOXSTATE) guiLockMem(zhState);
		if (ctrlState == NULL) {
			sprintf(work, "guiLockMem returned Null in %s, CB_INSERTSTRING", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		(void)ListBox_InsertString(ctrlState->hwndList, wParam, lParam);
		guiUnlockMem(zhState);
		return 0;

	case CB_DELETESTRING:
		zhState = (ZHANDLE) GetWindowLongPtr(hwnd, DBSTATESTRUCT_OFFSET);
		ctrlState = (PDROPBOXSTATE) guiLockMem(zhState);
		if (ctrlState == NULL) {
			sprintf(work, "guiLockMem returned Null in %s, CB_DELETESTRING", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		(void)ListBox_DeleteString(ctrlState->hwndList, wParam);
		guiUnlockMem(zhState);
		InvalidateRect(hwnd, NULL, FALSE);
		return 0;

	case CB_GETCURSEL:
		zhState = (ZHANDLE) GetWindowLongPtr(hwnd, DBSTATESTRUCT_OFFSET);
		ctrlState = (PDROPBOXSTATE) guiLockMem(zhState);
		if (ctrlState == NULL) {
			sprintf(work, "guiLockMem returned Null in %s, CB_GETCURSEL", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		i1 = ListBox_GetCurSel(ctrlState->hwndList);
		guiUnlockMem(zhState);
		InvalidateRect(hwnd, NULL, FALSE);
		return i1;

	case CB_SETCURSEL:
		zhState = (ZHANDLE) GetWindowLongPtr(hwnd, DBSTATESTRUCT_OFFSET);
		ctrlState = (PDROPBOXSTATE) guiLockMem(zhState);
		if (ctrlState == NULL) {
			sprintf(work, "guiLockMem returned Null in %s, CB_SETCURSEL", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		(void)ListBox_SetCurSel(ctrlState->hwndList, wParam);
		guiUnlockMem(zhState);
		InvalidateRect(hwnd, NULL, FALSE);
		return 0;

	case CB_RESETCONTENT:
		zhState = (ZHANDLE) GetWindowLongPtr(hwnd, DBSTATESTRUCT_OFFSET);
		ctrlState = (PDROPBOXSTATE) guiLockMem(zhState);
		if (ctrlState == NULL) {
			sprintf(work, "guiLockMem returned Null in %s, CB_RESETCONTENT", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		(void)ListBox_ResetContent(ctrlState->hwndList);
		guiUnlockMem(zhState);
		InvalidateRect(hwnd, NULL, FALSE);
		return 0;

	case CB_SHOWDROPDOWN:
		zhState = (ZHANDLE) GetWindowLongPtr(hwnd, DBSTATESTRUCT_OFFSET);
		ctrlState = (PDROPBOXSTATE) guiLockMem(zhState);
		if (ctrlState == NULL) {
			sprintf(work, "guiLockMem returned Null in %s, CB_SHOWDROPDOWN", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return 0;
		}
		if (wParam) {
			if (!IsWindowVisible(ctrlState->hwndList)) {
				i1 = ListBox_GetCount(ctrlState->hwndList);
				if (i1 < 1) LBHeight = ctrlState->usDropboxHeight;
				else {
					(void)ListBox_GetItemRect(ctrlState->hwndList, 0, &rect);
					i1 *= rect.bottom - rect.top + 1;
					LBHeight = min(i1, ctrlState->usDropboxHeight);
				}
				GetWindowRect(hwnd, &rect);
				SetWindowPos(ctrlState->hwndList, NULL,
					rect.left,
					rect.bottom,
					rect.right - rect.left, LBHeight + 1,	// Must add one pixel so a one line list will show.
					SWP_NOZORDER);
				SetWindowPos(ctrlState->hwndList, HWND_TOP,
					0, 0, 0, 0,
					SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
			}
		}
		else if (IsWindowVisible(ctrlState->hwndList)) {
			ShowWindow(ctrlState->hwndList, SW_HIDE);
			l1 = ListBox_GetCurSel(ctrlState->hwndList);
			if (ctrlState->selectedItem != l1) {
				PostMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(0, CBN_SELCHANGE), (LPARAM) hwnd);
			}
			ctrlState->selectedItem = (USHORT)l1;
		}
		guiUnlockMem(zhState);
		return TRUE;

	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
		if (!handleTableRightButtonClick(hwnd, message, lParam, wParam)) return 0;
		break;
	}
	l1 = DefWindowProc(hwnd, message, wParam, lParam);
	return l1;
}

static LRESULT DropBox_OnCreate(HWND hwnd, LPCREATESTRUCT cs) {

	ZHANDLE zhState;
	PDROPBOXSTATE ctrlState;
	DWORD lb_style;
	PTABLECOLUMN tblColumn;
	WNDPROC wpOld;

	/* Create a structure to contain my state info and attach it to the window */
	zhState = guiAllocMem(sizeof(DROPBOXSTATE));
	if (zhState == NULL) {
		sprintf(work, "guiAllocMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return FALSE;
	}
	ctrlState = guiLockMem(zhState);
	if (ctrlState == NULL) {
		sprintf(work, "guiLockMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return FALSE;
	}
	ZeroMemory(ctrlState, sizeof(DROPBOXSTATE));
	SetWindowLongPtr(hwnd, DBSTATESTRUCT_OFFSET, (LONG_PTR) zhState);
	tblColumn = (PTABLECOLUMN) cs->lpCreateParams;
	ctrlState->usDropboxHeight = tblColumn->dbopenht;
	ctrlState->selectedItem = CB_ERR;

	lb_style = WS_CHILD | WS_CLIPSIBLINGS | WS_BORDER | WS_VSCROLL | LBS_COMBOBOX;
	lb_style |= LBS_NOTIFY;
	if (cs->style & CBS_SORT) lb_style |= LBS_SORT;
	ctrlState->hwndList = CreateWindow(
		"ComboLBox",
		NULL, 	/* no text */
		lb_style,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		GetDesktopWindow(),
		NULL,		/* no menu or id */
		cs->hInstance,
		NULL
	);
	if (ctrlState->hwndList == NULL) {
		guiaErrorMsg("Create Embedded Listbox error", GetLastError());
		guiUnlockMem(zhState);
		guiFreeMem(zhState);
		return FALSE;
	}

	/* We must subclass the listbox so that we can see when it gets certain keystrokes */
	wpOld = (WNDPROC) SetWindowLongPtr(ctrlState->hwndList, GWLP_WNDPROC, (LONG_PTR) DXListBoxProc);
	SetProp(ctrlState->hwndList, MAKEINTATOM(ORIG_LISTBOX_WNDPROC), wpOld);
	SetProp(ctrlState->hwndList, MAKEINTATOM(DROPBOX_HWND), hwnd);

	guiUnlockMem(zhState);
	return TRUE;
}


/**
 * The ListBox is a child window of the desktop.
 * I think I need to destroy it here. I do not think Windows
 * will automatically destroy it.
 */
static void DropBox_OnDestroy(HWND hwnd) {

	ZHANDLE zhState;
	PDROPBOXSTATE ctrlState;

	zhState = (ZHANDLE) GetWindowLongPtr(hwnd, DBSTATESTRUCT_OFFSET);
	ctrlState = (PDROPBOXSTATE) guiLockMem(zhState);
	if (ctrlState == NULL) {
		sprintf(work, "guiLockMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}
	DestroyWindow(ctrlState->hwndList);
	guiFreeMem(zhState);
}


static LRESULT DropBox_OnGetText(HWND hwnd, int iCch, LPSTR buffer) {

	ZHANDLE zhState;
	PDROPBOXSTATE ctrlState;
	LRESULT i1, l1, tlen;
	CHAR txtbuf[MAXCBMSGSIZE - 5];

	zhState = (ZHANDLE) GetWindowLongPtr(hwnd, DBSTATESTRUCT_OFFSET);
	ctrlState = (PDROPBOXSTATE) guiLockMem(zhState);
	if (ctrlState == NULL) {
		sprintf(work, "guiLockMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return FALSE;
	}
	l1 = ListBox_GetCurSel(ctrlState->hwndList);
	if (l1 == CB_ERR) {
		buffer[0] = '\0';
		return 0;
	}
	tlen = ListBox_GetTextLen(ctrlState->hwndList, l1);
	if (tlen < 1) {
		buffer[0] = '\0';
		return 0;
	}
	(void)ListBox_GetText(ctrlState->hwndList, l1, txtbuf);
	i1 = min(tlen, iCch);
	strncpy(buffer, txtbuf, i1);
	guiUnlockMem(zhState);

	return i1;
}

static void DropBox_OnKey(HWND hwnd, UINT vk, BOOL bKeyDown, int cRepeat, UINT flags) {

	ZHANDLE zhState;
	PDROPBOXSTATE ctrlState;
	HWND hwndCells, hwndTable;
	LRESULT l1;
	RECT rect;

	if (!bKeyDown) return;

	zhState = (ZHANDLE) GetWindowLongPtr(hwnd, DBSTATESTRUCT_OFFSET);
	ctrlState = (PDROPBOXSTATE) guiLockMem(zhState);
	if (ctrlState == NULL) {
		sprintf(work, "guiLockMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}
	hwndCells = GetParent(hwnd);
	hwndTable = GetParent(hwndCells);
	if (vk == VK_UP || vk == VK_DOWN) {
		if (GetKeyState(VK_CONTROL) & 0x8000) {
			FORWARD_WM_KEYDOWN(hwndTable, vk, cRepeat, flags, SendMessage);
		}
		else {
			l1 = ListBox_GetCurSel(ctrlState->hwndList);
			if (vk == VK_DOWN) {
				if (l1 == CB_ERR) (void)ListBox_SetCurSel(ctrlState->hwndList, 0);
				else if (l1 + 1 < ListBox_GetCount(ctrlState->hwndList)) (void)ListBox_SetCurSel(ctrlState->hwndList, l1 + 1);
				InvalidateRect(hwnd, NULL, FALSE);
			}
			else if (l1 != CB_ERR) {
				if (l1 > 0) (void)ListBox_SetCurSel(ctrlState->hwndList, l1 - 1);
				InvalidateRect(hwnd, NULL, FALSE);
			}
		}
	}
	else if (vk == VK_TAB || vk == VK_RETURN) {
		if (IsWindowVisible(ctrlState->hwndList)) {
			if (GetCapture() == hwnd) ReleaseCapture();
			(void)ComboBox_ShowDropdown(hwnd, FALSE); // @suppress("Symbol is not resolved")
		}
		FORWARD_WM_KEYDOWN(hwndTable, vk, cRepeat, flags, SendMessage);
	}
	else if (vk == VK_F4) {
		if (!IsWindowVisible(ctrlState->hwndList)) {
			(void)ComboBox_ShowDropdown(hwnd, TRUE); // @suppress("Symbol is not resolved")
			SetFocus(ctrlState->hwndList);
			SetCapture(hwnd);
		}
		else  {
			if (GetCapture() == hwnd) ReleaseCapture();
			(void)ComboBox_ShowDropdown(hwnd, FALSE); // @suppress("Symbol is not resolved")
			SetFocus(hwnd);
			GetClientRect(hwnd, &rect);
			rect.right -= buttonWidth;
			InvalidateRect(hwnd, &rect, FALSE);
			UpdateWindow(hwnd);
		}
	}
	guiUnlockMem(zhState);
}

static void DropBox_OnLButtonDown(HWND hwnd, BOOL bDblClk, int x, int y, UINT uiKeyFlags) {

	ZHANDLE zhState;
	PDROPBOXSTATE ctrlState;
	POINT ptOrig, ptScreen, ptListBox;
	RECT rect;
	LRESULT lr1;

	zhState = (ZHANDLE) GetWindowLongPtr(hwnd, DBSTATESTRUCT_OFFSET);
	ctrlState = (PDROPBOXSTATE) guiLockMem(zhState);
	if (ctrlState == NULL) {
		sprintf(work, "guiLockMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}
	ptOrig.x = x;
	ptOrig.y = y;

	/*
	 * If the LB is not visible {
	 * 		If the mouse is over the button, depress it.
	 * 		Make LB visible.
	 * 		Set the focus to the LB and capture the mouse.
	 * 		Clear the LBUTTONINCLIENT flag
	 * }
	 *
	 * If the LB is visible {
	 * 		If the mouse is over the vert scroll bar {
	 * 			release capture
	 * 			send WM_NCLBUTTONDOWN to LB
	 * 			set capture
	 * 		}
	 * 		else if the mouse is over the client area {
	 * 			Get the item index from the list box and then set it as selected.
	 *	 		Set the LBUTTONINCLIENT flag
	 * 		}
	 * 		else if the mouse is anywhere else {
	 * 			release capture
	 * 			close the LB
	 * 			set focus to DB
	 * 		}
	 * }
	 *
	 */
	if (!IsWindowVisible(ctrlState->hwndList)) {
		/* If the mouse click is over the button, draw it pressed */
		GetClientRect(hwnd, &rect);
		rect.left = rect.right - buttonWidth;
		if (PtInRect(&rect, ptOrig)) {
			ctrlState->stateButton = STATE_SYSTEM_PRESSED;
			InvalidateRect(hwnd, &rect, FALSE);
			UpdateWindow(hwnd);
		}
		SendMessage(hwnd, CB_SHOWDROPDOWN, TRUE, 0);
		SetFocus(hwnd);
		SetCapture(hwnd);
	}
	else {
		ptScreen = ptOrig;
		ClientToScreen(hwnd, &ptScreen);
		GetWindowRect(ctrlState->hwndList, &rect);
		if (PtInRect(&rect, ptScreen)) {
			lr1 = SendMessage(ctrlState->hwndList, WM_NCHITTEST, 0, MAKELPARAM(ptScreen.x, ptScreen.y));
			if (lr1 == HTVSCROLL) {
				ReleaseCapture();
				SendMessage(ctrlState->hwndList, WM_NCLBUTTONDOWN, HTVSCROLL, MAKELPARAM(ptScreen.x, ptScreen.y));
				SetCapture(hwnd);
			}
			else if (lr1 == HTCLIENT) {
				ptListBox = ptScreen;
				ScreenToClient(ctrlState->hwndList, &ptListBox);
				lr1 = SendMessage(ctrlState->hwndList, LB_ITEMFROMPOINT, 0, MAKELPARAM(ptListBox.x, ptListBox.y));
				if (!HIWORD(lr1)) {
					(void)ListBox_SetCurSel(ctrlState->hwndList,LOWORD(lr1));
				}
			}
		}
		else {
			if (GetCapture() == hwnd) ReleaseCapture();
			(void)ComboBox_ShowDropdown(hwnd, FALSE); // @suppress("Symbol is not resolved")
			SetFocus(hwnd);
		}
	}

	guiUnlockMem(zhState);
}


static void DropBox_OnLButtonUp(HWND hwnd, int x, int y, UINT uiKeyFlags) {

	ZHANDLE zhState;
	PDROPBOXSTATE ctrlState;
	RECT rect;
	POINT ptScreen, ptListBox;

	zhState = (ZHANDLE) GetWindowLongPtr(hwnd, DBSTATESTRUCT_OFFSET);
	ctrlState = (PDROPBOXSTATE) guiLockMem(zhState);
	if (ctrlState == NULL) {
		sprintf(work, "guiLockMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}

	/* Draw the button as raised */
	if (ctrlState->stateButton == STATE_SYSTEM_PRESSED) {
		ctrlState->stateButton = 0;
		GetClientRect(hwnd, &rect);
		rect.left = rect.right - buttonWidth;
		InvalidateRect(hwnd, &rect, FALSE);
		UpdateWindow(hwnd);
	}

	/* If the LB is visible, and the mouse was over it,
	 * and the mouse is over the client area, close the LB
	 */
	if (IsWindowVisible(ctrlState->hwndList)) {
		ptScreen.x = x;
		ptScreen.y = y;
		ClientToScreen(hwnd, &ptScreen);
		GetWindowRect(ctrlState->hwndList, &rect);
		if (PtInRect(&rect, ptScreen)) {
			/* But is it over the vertical scroll bar? */
			ptListBox = ptScreen;
			ScreenToClient(ctrlState->hwndList, &ptListBox);
			GetClientRect(ctrlState->hwndList, &rect);
			if (PtInRect(&rect, ptListBox)) {
				if (GetCapture() == hwnd) ReleaseCapture();
				//(void)ComboBox_ShowDropdown(hwnd, FALSE);
				SendMessage(hwnd, CB_SHOWDROPDOWN, FALSE, 0);
				SetFocus(hwnd);
				GetClientRect(hwnd, &rect);
				rect.right -= buttonWidth;
				InvalidateRect(hwnd, &rect, FALSE);
				UpdateWindow(hwnd);
			}
		}
	}

	guiUnlockMem(zhState);
}

static void DropBox_OnMouseMove(HWND hwnd, int x, int y, UINT uiKeyFlags) {

	ZHANDLE zhState;
	PDROPBOXSTATE ctrlState;
	RECT rect1, rcClient;
	POINT ptScreen, ptOrig, ptListBox;
	LRESULT lr1, lr2;

	zhState = (ZHANDLE) GetWindowLongPtr(hwnd, DBSTATESTRUCT_OFFSET);
	ctrlState = (PDROPBOXSTATE) guiLockMem(zhState);
	if (ctrlState == NULL) {
		sprintf(work, "guiLockMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}

	/*
	 * If our button is pressed, and the left mouse button is down,
	 * and the mouse is not over our button, then raise the button
	 */
	if (ctrlState->stateButton == STATE_SYSTEM_PRESSED && uiKeyFlags & MK_LBUTTON) {
		GetClientRect(hwnd, &rcClient);
		rect1 = rcClient;
		rect1.left = rect1.right - buttonWidth;
		ptOrig.x = x;
		ptOrig.y = y;
		if (!PtInRect(&rect1, ptOrig)) {
			ctrlState->stateButton = 0;
			InvalidateRect(hwnd, &rect1, FALSE);
			UpdateWindow(hwnd);
		}
	}

	/*
	 * If the LB is visible, and the mouse is over it,
	 * then tell the LB to select the item under the mouse
	 */
	if (IsWindowVisible(ctrlState->hwndList)) {
		ptScreen.x = x;
		ptScreen.y = y;
		ClientToScreen(hwnd, &ptScreen);
		GetWindowRect(ctrlState->hwndList, &rect1);
		if (PtInRect(&rect1, ptScreen)) {
			ptListBox = ptScreen;
			ScreenToClient(ctrlState->hwndList, &ptListBox);
			lr1 = SendMessage(ctrlState->hwndList, LB_ITEMFROMPOINT, 0, MAKELPARAM(ptListBox.x, ptListBox.y));
			if (!HIWORD(lr1)) {
				lr2 = ListBox_GetCurSel(ctrlState->hwndList);
				if (LOWORD(lr1) != LOWORD(lr2)) {
					(void)ListBox_SetCurSel(ctrlState->hwndList,LOWORD(lr1));
				}
			}
		}
	}
	guiUnlockMem(zhState);
}

static void DropBox_OnPaint(HWND hwnd) {

	ZHANDLE zhState;
	PDROPBOXSTATE ctrlState;
	RECT rectClient, rect1, rect2;
	HDC dc;
	PAINTSTRUCT ps;
	LRESULT l1, tlen;
	UINT uFormat = DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX;
	CHAR txtbuf[MAXCBMSGSIZE - 5];
	HFONT hOldFont;

	zhState = (ZHANDLE) GetWindowLongPtr(hwnd, DBSTATESTRUCT_OFFSET);
	ctrlState = (PDROPBOXSTATE) guiLockMem(zhState);
	if (ctrlState == NULL) {
		sprintf(work, "guiLockMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}
	GetClientRect(hwnd, &rectClient);
	dc = BeginPaint(hwnd, &ps);

	rect1 = rectClient;
	rect1.right -= buttonWidth;

	if (IntersectRect(&rect2, &rect1, &ps.rcPaint)) {
		FillRect(dc, &rect1, getBackgroundBrush(hwnd));
		if (hwnd == GetFocus()) {
			InflateRect(&rect1, -1, -1);
			FillRect(dc, &rect1, GetSysColorBrush(COLOR_HIGHLIGHT));
			DBDrawFocusRect(dc, rect1);
		}

		/* If the LB has a selection, draw the text in our text area */
		l1 = ListBox_GetCurSel(ctrlState->hwndList);
		if (l1 != LB_ERR) {
			tlen = ListBox_GetTextLen(ctrlState->hwndList, l1);
			if (tlen > 0) {
				SetBkMode(dc, TRANSPARENT);
				if (hwnd == GetFocus()) SetTextColor(dc, white);
				else SendMessage(GetParent(hwnd), WM_CTLCOLORSTATIC, (WPARAM) dc, (LPARAM) hwnd);
				(void)ListBox_GetText(ctrlState->hwndList, l1, txtbuf);
				hOldFont = SelectObject(dc, GetWindowFont(ctrlState->hwndList));
				rect1.left += 6;
				DrawText(dc, txtbuf, (int)tlen, &rect1, uFormat);
				if (hOldFont != NULL) SelectObject(dc, hOldFont);
			}
		}
	}

	rect1 = rectClient;
	rect1.left = rect1.right - buttonWidth;

	if (IntersectRect(&rect2, &rect1, &ps.rcPaint)) {
		DrawEdge(dc, &rect1, ctrlState->stateButton == STATE_SYSTEM_PRESSED ? EDGE_SUNKEN : EDGE_RAISED,
				BF_ADJUST | BF_RECT);
		FillRect(dc, &rect1, GetSysColorBrush(COLOR_BTNFACE));
		DBDrawDownArrow(dc, rect1);
	}
	EndPaint(hwnd, &ps);
	guiUnlockMem(zhState);
}

static void DropBox_OnSetFont(HWND hwnd, HFONT font, BOOL redraw) {

	ZHANDLE zhState;
	PDROPBOXSTATE ctrlState;

	zhState = (ZHANDLE) GetWindowLongPtr(hwnd, DBSTATESTRUCT_OFFSET);
	ctrlState = (PDROPBOXSTATE) guiLockMem(zhState);
	if (ctrlState == NULL) {
		sprintf(work, "guiLockMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}
	SetWindowFont(ctrlState->hwndList, font, redraw); // @suppress("Symbol is not resolved")
	guiUnlockMem(zhState);
}

static void DBDrawDownArrow(HDC dc, RECT rect) {
	int i1 = (rect.bottom - rect.top) / 2 - 2;

	COLORREF color = black;
	SetPixel(dc, rect.left + 2, rect.top + i1, color);
	SetPixel(dc, rect.left + 3, rect.top + i1, color);
	SetPixel(dc, rect.left + 4, rect.top + i1, color);
	SetPixel(dc, rect.left + 5, rect.top + i1, color);
	SetPixel(dc, rect.left + 6, rect.top + i1++, color);
	SetPixel(dc, rect.left + 3, rect.top + i1, color);
	SetPixel(dc, rect.left + 4, rect.top + i1, color);
	SetPixel(dc, rect.left + 5, rect.top + i1++, color);
	SetPixel(dc, rect.left + 4, rect.top + i1, color);
}

static void DBDrawFocusRect(HDC dc, RECT rect) {
	int i1;
	for (i1 = rect.left + 1; i1 < rect.right; i1 += 2) SetPixel(dc, i1, rect.top, black);
	for (i1 = rect.left + 1; i1 < rect.right; i1 += 2) SetPixel(dc, i1, rect.bottom - 1, black);
	for (i1 = rect.top + 1; i1 < rect.bottom; i1 += 2) SetPixel(dc, rect.left, i1, black);
	for (i1 = rect.top + 1; i1 < rect.bottom; i1 += 2) SetPixel(dc, rect.right - 1, i1, black);
}

LRESULT CALLBACK DXListBoxProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

	WNDPROC wpOld = GetProp(hwnd, MAKEINTATOM(ORIG_LISTBOX_WNDPROC));
	HWND hwndDb;

	switch (message) {

	case WM_KEYDOWN:
		if (wParam == VK_F4) {
			hwndDb = GetProp(hwnd, MAKEINTATOM(DROPBOX_HWND));
			SendMessage(hwndDb, message, wParam, lParam);
			return 0;
		}
		break;

	case WM_DESTROY:
		SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR) wpOld);
		RemoveProp(hwnd, MAKEINTATOM(ORIG_LISTBOX_WNDPROC));
		break;
	}
	return CallWindowProc(wpOld, hwnd, message, wParam, lParam);
}

/***********************************************************************************************************************
 *
 *
 * DXCheckbox
 *
 *
 */


#define CBSTATESTRUCT_OFFSET  0
#define TBLCHECKBOXHEIGHT 13
#define TBLCHECKBOXWIDTH 13


typedef struct tagCheckboxState {
	BOOL selected;
} CHECKBOXSTATE, *PCHECKBOXSTATE;

static LRESULT CheckBox_OnCreate(HWND hwnd, LPCREATESTRUCT cs);
static void CheckBox_OnDestroy(HWND hwnd);
static int CheckBox_OnEraseBkgnd(HWND hwnd, HDC dc);
static void CheckBox_OnKey(HWND hwnd, UINT vk, BOOL bKeyDown, int cRepeat, UINT flags);
static void CheckBox_OnLButtonDown(HWND hwnd, BOOL bDblClk, int x, int y, UINT uiKeyFlags);
static void CheckBox_OnPaint(HWND hwnd);
static void DrawCheckbox(HDC dc, int cellwidth, int cellheight, BOOL selected, BOOL focus, BOOL avail);
static void DrawCheckmark(HDC dc, int x, int y, INT syscolorcode);
LRESULT CALLBACK DXCheckboxProc (HWND, UINT, WPARAM, LPARAM);

void guiaRegisterDXCheckbox(HINSTANCE hInstance)
{
	wndclass.style         = CS_OWNDC;
	wndclass.lpfnWndProc   = DXCheckboxProc ;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = sizeof(LONG_PTR);
	wndclass.hInstance     = hInstance ;
	wndclass.hIcon         = NULL;
	wndclass.hCursor       = LoadCursor (NULL, IDC_ARROW) ;
	wndclass.hbrBackground = NULL;
	wndclass.lpszMenuName  = NULL ;
	wndclass.lpszClassName = szDXCheckboxClassName;
	if (!RegisterClass(&wndclass)) {
		sprintf(work, "%s failed, e=%d", __FUNCTION__, (int) GetLastError());
		MessageBox (NULL, work, szDXCheckboxClassName, MB_ICONERROR);
	}

	return;
}


LRESULT CALLBACK DXCheckboxProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

	ZHANDLE zhState;
	PCHECKBOXSTATE ctrlState;

	switch (message) {

	HANDLE_MSG(hwnd, WM_CREATE, CheckBox_OnCreate);
	HANDLE_MSG(hwnd, WM_DESTROY, CheckBox_OnDestroy);
	HANDLE_MSG(hwnd, WM_ERASEBKGND, CheckBox_OnEraseBkgnd); // @suppress("Symbol is not resolved")
	HANDLE_MSG(hwnd, WM_KEYDOWN, CheckBox_OnKey);
	HANDLE_MSG(hwnd, WM_LBUTTONDOWN, CheckBox_OnLButtonDown);
	HANDLE_MSG(hwnd, WM_PAINT, CheckBox_OnPaint);

	case WM_SETFOCUS:
	case WM_KILLFOCUS:
		guiaHandleTableFocusMessage(hwnd, message, TRUE);
		return 0;

	case BM_SETCHECK:
		zhState = (ZHANDLE) GetWindowLongPtr(hwnd, CBSTATESTRUCT_OFFSET);
		ctrlState = (PCHECKBOXSTATE) guiLockMem(zhState);
		if (ctrlState == NULL) {
			sprintf(work, "guiLockMem returned Null in %s, BM_SETCHECK", __FUNCTION__);
			guiaErrorMsg(work, 0);
			return FALSE;
		}
		ctrlState->selected = (wParam == BST_CHECKED);
		guiUnlockMem(zhState);
		return 0;

	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
		if (!handleTableRightButtonClick(hwnd, message, lParam, wParam)) return 0;
		break;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

static LRESULT CheckBox_OnCreate(HWND hwnd, LPCREATESTRUCT cs) {

	ZHANDLE zhState;
	PCHECKBOXSTATE ctrlState;

	/* Create a structure to contain my state info and attach it to the window */
	zhState = guiAllocMem(sizeof(CHECKBOXSTATE));
	if (zhState == NULL) {
		sprintf(work, "guiAllocMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return FALSE;
	}
	ctrlState = guiLockMem(zhState);
	if (ctrlState == NULL) {
		sprintf(work, "guiLockMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return FALSE;
	}
	ZeroMemory(ctrlState, sizeof(CHECKBOXSTATE));
	SetWindowLongPtr(hwnd, CBSTATESTRUCT_OFFSET, (LONG_PTR) zhState);
	ctrlState->selected = (BOOL) (INT_PTR) cs->lpCreateParams;
	guiUnlockMem(zhState);
	return TRUE;
}


static void CheckBox_OnDestroy(HWND hwnd) {

	ZHANDLE zhState;

	zhState = (ZHANDLE) GetWindowLongPtr(hwnd, CBSTATESTRUCT_OFFSET);
	guiFreeMem(zhState);
}

static int CheckBox_OnEraseBkgnd(HWND hwnd, HDC dc) {
	RECT rect;

	GetClientRect(hwnd, &rect);
	FillRect(dc, &rect, getBackgroundBrush(hwnd));
	return 1;
}


static void CheckBox_OnKey(HWND hwnd, UINT vk, BOOL bKeyDown, int cRepeat, UINT flags) {

	HWND hwndCells, hwndTable;

	if (bKeyDown) {
		if (vk == VK_SPACE) {
			PostMessage(hwnd, WM_LBUTTONDOWN, (WPARAM) MK_LBUTTON, (LPARAM) 0);
		}
		else if (vk == VK_TAB || vk == VK_RETURN || ((vk == VK_UP || vk == VK_DOWN) && GetKeyState(VK_CONTROL) & 0x8000)) {
			hwndCells = GetParent(hwnd);
			hwndTable = GetParent(hwndCells);
			FORWARD_WM_KEYDOWN(hwndTable, vk, cRepeat, flags, PostMessage);
		}
	}
}

static void CheckBox_OnLButtonDown(HWND hwnd, BOOL bDblClk, int x, int y, UINT uiKeyFlags) {

	ZHANDLE zhState, zhTstate;
	PTABLESTATE tblState;
	PCHECKBOXSTATE ctrlState;
	HWND hwndCells, hwndTable;
	NMLISTVIEW nmlistview;

	hwndCells = GetParent(hwnd);
	hwndTable = GetParent(hwndCells);
	zhTstate = (ZHANDLE) GetWindowLongPtr(hwndTable, TBLSTATESTRUCT_OFFSET);
	tblState = (PTABLESTATE) guiLockMem(zhTstate);
	if (tblState->readonly) {
		// In readonly state, can get focus, but user cannot edit it
		guiUnlockMem(zhTstate);
		SetFocus(hwnd);
		InvalidateRect(hwnd, NULL, FALSE);
		return;
	}
	guiUnlockMem(zhTstate);

	zhState = (ZHANDLE) GetWindowLongPtr(hwnd, CBSTATESTRUCT_OFFSET);
	ctrlState = (PCHECKBOXSTATE) guiLockMem(zhState);
	if (ctrlState == NULL) {
		sprintf(work, "guiLockMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}
	ctrlState->selected = !ctrlState->selected;
	SetFocus(hwnd);
	InvalidateRect(hwnd, NULL, FALSE);
	ZeroMemory(&nmlistview, sizeof(NMLISTVIEW));
	nmlistview.uChanged = LVIF_STATE;
	if (ctrlState->selected) nmlistview.uNewState = LVIS_SELECTED;
	getColumnRowFromWindow(hwnd, hwndCells, &nmlistview.iSubItem, &nmlistview.iItem);
	nmlistview.hdr.hwndFrom = hwnd;
	nmlistview.hdr.code = LVN_ITEMCHANGED;
	SendMessage(hwndTable, WM_NOTIFY, (WPARAM) 0, (LPARAM) &nmlistview);
	guiUnlockMem(zhState);
}

static void CheckBox_OnPaint(HWND hwnd) {

	HDC dc;
	PAINTSTRUCT ps;
	RECT rect;
	ZHANDLE zhState;
	PCHECKBOXSTATE ctrlState;

	zhState = (ZHANDLE) GetWindowLongPtr(hwnd, CBSTATESTRUCT_OFFSET);
	ctrlState = (PCHECKBOXSTATE) guiLockMem(zhState);
	if (ctrlState == NULL) {
		sprintf(work, "guiLockMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return;
	}
	GetClientRect(hwnd, &rect);
	dc = BeginPaint(hwnd, &ps);
	DrawCheckbox(dc, rect.right, rect.bottom, ctrlState->selected, GetFocus() == hwnd,
			isTableEnabled(hwnd));
	EndPaint(hwnd, &ps);
	guiUnlockMem(zhState);
}

static void DrawCheckbox(HDC dc, int cellwidth, int cellheight, BOOL selected, BOOL focus, BOOL avail) {
	int x, y;
	RECT rect;

	x = (cellwidth - TBLCHECKBOXWIDTH) / 2;
	y = (cellheight - TBLCHECKBOXHEIGHT) / 2;
	rect.top = y + 1;
	rect.left = x + 2;
	rect.bottom = rect.top + TBLCHECKBOXHEIGHT;
	rect.right = rect.left + TBLCHECKBOXWIDTH;

	DrawEdge(dc, &rect, EDGE_SUNKEN, BF_ADJUST | BF_RECT);

	if (avail) FillRect(dc, &rect, (HBRUSH) (INT_PTR) (focus ? (COLOR_HIGHLIGHT + 1) : (COLOR_WINDOW + 1)));
	else FillRect(dc, &rect, (HBRUSH) (COLOR_BTNFACE + 1));
	if (selected) DrawCheckmark(dc, rect.left, rect.top,
			avail ? (focus ? COLOR_HIGHLIGHTTEXT : COLOR_BTNTEXT) : COLOR_GRAYTEXT);
}

static void DrawCheckmark(HDC dc, int x, int y, INT colorcode) {
	COLORREF color = GetSysColor(colorcode);
	int i1;
	x += 1;
	y += 3;
	for (i1 = 0; i1 < 3; i1++) SetPixel(dc, x, y + i1, color);
	x++;
	y += 1;
	for (i1 = 0; i1 < 3; i1++) SetPixel(dc, x, y + i1, color);
	x++;
	y += 1;
	for (i1 = 0; i1 < 3; i1++) SetPixel(dc, x, y + i1, color);
	x++;
	y -= 1;
	for (i1 = 0; i1 < 3; i1++) SetPixel(dc, x, y + i1, color);
	x++;
	y -= 1;
	for (i1 = 0; i1 < 3; i1++) SetPixel(dc, x, y + i1, color);
	x++;
	y -= 1;
	for (i1 = 0; i1 < 3; i1++) SetPixel(dc, x, y + i1, color);
	x++;
	y -= 1;
	for (i1 = 0; i1 < 3; i1++) SetPixel(dc, x, y + i1, color);
}

void drawTabHeaderFocusRectangle(CONTROL* ctl, BOOL hasFocus) {
	ctl->tab.hasFocus = hasFocus;
	if (hasFocus) TabCtrl_SetCurFocus(ctl->tabgroupctlhandle, ctl->tabsubgroup - 1);
	//InvalidateRect(ctl->tabgroupctlhandle, NULL, TRUE);
}

void clearAllTabControlFocus(HWND hwndWindow) {
	INT i1;
	CONTROL *control;
	RESOURCE *res;
	WINHANDLE winhandle;
	WINDOW *win;
	WINDOWINFO pwi;

	pwi.cbSize = sizeof(WINDOWINFO);
	GetWindowInfo(hwndWindow, &pwi);
	if (aDialogClass == pwi.atomWindowType) {
		RESHANDLE dialogreshandle = (RESHANDLE) GetWindowLongPtr(hwndWindow, GWLP_USERDATA);
		if (dialogreshandle == NULL) return;
		pvistart();
		res = *dialogreshandle;
		control = (CONTROL *) res->content;
		i1 = res->entrycount;
		while (i1--) {
			if (control->type == PANEL_TAB && control->tab.hasFocus) {
				drawTabHeaderFocusRectangle(control, FALSE);
			}
			control++;
		}
		pviend();
	}
	else {
		winhandle =  (WINHANDLE) GetWindowLongPtr(hwndWindow, GWLP_USERDATA);
		if (winhandle != NULL) {
			pvistart();
			win = *winhandle;
			if (win->showreshdr != NULL) {
				res = *win->showreshdr;
				while (res != NULL) {
					control = (CONTROL *) res->content;
					i1 = res->entrycount;
					while (i1--) {
						if (control->type == PANEL_TAB && control->tab.hasFocus) {
							drawTabHeaderFocusRectangle(control, FALSE);
						}
						control++;
					}
					if (res->showresnext == NULL) break;
					res = *res->showresnext;
				}
			}
			pviend();
		}
	}
}

static CONTROL* findControlInDialog(HWND hwndWindow, HWND hwndControl) {
	RESOURCE *res;
	RESHANDLE currentreshandle;
	CONTROL *control;
	INT iCurCtrl;

	currentreshandle = (RESHANDLE) GetWindowLongPtr(hwndWindow, GWLP_USERDATA);
	if (currentreshandle == NULL) return NULL;
	pvistart();
	res = *currentreshandle;
	while (res != NULL) {
		control = (CONTROL *) res->content;
		iCurCtrl = res->entrycount;
		while(iCurCtrl--) {
			if (control->ctlhandle == hwndControl) {
				pviend();
				return control;
			}
			control++;
		}
		if (res->showresnext == NULL) break;
		res = *res->showresnext;
	}
	pviend();
	return NULL;
}

CONTROL* FindControlInWindow(HWND hwndWindow, HWND hwndControl) {

	CONTROL *control;
	RESOURCE *res;
	WINHANDLE winhandle;
	WINDOW *win;
	INT i1;
	WINDOWINFO pwi;

	pwi.cbSize = sizeof(WINDOWINFO);
	GetWindowInfo(hwndWindow, &pwi);
	if (aDialogClass == pwi.atomWindowType) {
		return findControlInDialog(hwndWindow, hwndControl);
	}
	winhandle =  (WINHANDLE) GetWindowLongPtr(hwndWindow, GWLP_USERDATA);
	if (winhandle != NULL) {
		pvistart();
		win = *winhandle;
		if (win->showreshdr != NULL) {
			res = *win->showreshdr;
			while (res != NULL) {
				control = (CONTROL *) res->content;
				i1 = res->entrycount;
				while (i1--) {
					if (control->ctlhandle == hwndControl) {
						pviend();
						return control;
					}
					control++;
				}
				if (res->showresnext == NULL) break;
				res = *res->showresnext;
			}
		}
		if (win->showtoolbar != NULL) {
			res = *win->showtoolbar;
			control = (CONTROL *) res->content;
			i1 = res->entrycount;
			while (i1--) {
				if (control->ctlhandle == hwndControl) {
					pviend();
					return control;
				}
				control++;
			}
		}
		pviend();
	}
	return NULL;
}

/**
 * Draw a standard DX checkbox
 */
void OwnerDrawCheckBox(CONTROL *control, const DRAWITEMSTRUCT *pdi)
{
	RECT rect;
	RECT outerRect = pdi->rcItem;
	int cboxHeight = GetSystemMetrics (SM_CYVSCROLL) - 3;
	int cboxWidth = GetSystemMetrics (SM_CXVSCROLL) - 3;
	//HGDIOBJ hOldBrush;
	int avail = !(pdi->itemState & ODS_DISABLED);
	rect.top = (outerRect.bottom - cboxHeight) / 2;
	rect.bottom = rect.top + cboxHeight;
	if (control->type == PANEL_CHECKBOX) rect.left = 0;
	else rect.left = outerRect.right - cboxWidth - 1;
	rect.right = rect.left + cboxWidth;
	if (avail) FillRect(pdi->hDC, &rect, (HBRUSH)(COLOR_WINDOW + 1));
	else FillRect(pdi->hDC, &rect, hbrButtonFace);
	DrawEdge(pdi->hDC, &rect, EDGE_SUNKEN, BF_ADJUST | BF_RECT);
	if (control->style & CTRLSTYLE_MARKED) {
		DrawCheckmark(pdi->hDC, rect.left, rect.top,
					avail ? COLOR_BTNTEXT : COLOR_GRAYTEXT);
	}
	if (pdi->itemAction & ODA_DRAWENTIRE) {
		if (control->textval != NULL) {
			LPCSTR string = guiLockMem(control->textval);
			if (!avail) SetTextColor(pdi->hDC, GetSysColor(COLOR_GRAYTEXT));
			SetBkColor(pdi->hDC, GetSysColor(COLOR_BTNFACE));
			if (control->type == PANEL_CHECKBOX) outerRect.left += cboxWidth + 5;
			else outerRect.left += 2;
			DrawText(pdi->hDC, string, -1, &outerRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOCLIP);
			guiUnlockMem(control->textval);
		}
	}
}

/**
 * Handle user draw push buttons, icon buttons, list controls, etc...
 */
void WinProc_OnDrawItem(HWND hwnd, const DRAWITEMSTRUCT *pdi) {

	CONTROL *control;
	PMENUENTRY menuitem;
	HDC hDCIcon;
	RESOURCE *res;
	RECT rect;

	if (pdi->CtlType != ODT_MENU) {
		//-----------------------------------------------------------------------
		// CODE   No good! We cannot cache a pointer to CONTROL in the Windows widget, The real CONTROL
		//        may move due to a realloc. This can only happen to toolbars as they
		//        can have controls added and deleted after the prep.
		//control = (CONTROL *) GetWindowLongPtr(pdi->hwndItem, GWLP_USERDATA);
		//-----------------------------------------------------------------------
		control = FindControlInWindow(hwnd, pdi->hwndItem);
		if (control != NULL) {
			if (ISLISTBOX(control)) guiaOwnerDrawListbox(control, pdi);
			else if (ISCHECKBOX(control)) OwnerDrawCheckBox(control, pdi);
			else guiaOwnerDrawIcon(control, pdi);
		}
	}
	else {
		menuitem = (PMENUENTRY) pdi->itemData;
		if (((pdi->itemAction & ODA_DRAWENTIRE) || (pdi->itemAction & ODA_SELECT))
				&& menuitem != NULL && menuitem->iconres != NULL)
		{
			rect.left = pdi->rcItem.left;
			rect.right = pdi->rcItem.right;
			rect.top = pdi->rcItem.top;
			rect.bottom = pdi->rcItem.bottom;
			res = *menuitem->iconres;
			hDCIcon = CreateCompatibleDC(pdi->hDC);
			if (hDCIcon == NULL) return;
			SelectObject(hDCIcon, res->hbmicon);
			if (res->useTransBlt4Icon) {
				TransparentBlt(pdi->hDC, rect.left, rect.top, rect.right - rect.left,
				 rect.bottom - rect.top, hDCIcon, 0, 0, res->hsize, res->vsize, res->transColor);
			}
			else {
				BitBlt(pdi->hDC, rect.left, rect.top, rect.right - rect.left,
					rect.bottom - rect.top, hDCIcon, 0, 0, SRCCOPY);
			}
			DeleteDC(hDCIcon);
		}
	}
}

/**
 * Windows will call us for a Listbox because we use LBS_OWNERDRAWFIXED, we can safely ignore this.
 */
void WinProc_OnMeasureItem(HWND hwnd, MEASUREITEMSTRUCT *pmi) {

	PMENUENTRY menuentry;
	RESOURCE *res;

	if (pmi->CtlType != ODT_MENU) return;
	menuentry = (PMENUENTRY) pmi->itemData;
	res = *menuentry->iconres;
	pmi->itemWidth = res->hsize;
	pmi->itemHeight = res->vsize;
}

static void HideTabControls(CONTROL *currentcontrol) {
	RESOURCE *res;
	CONTROL *control;
	INT iCurCtrl, textlen;
	CHAR work[64];

	res = *currentcontrol->res;
	control = (CONTROL *) res->content;
	for (iCurCtrl = 0; iCurCtrl < res->entrycount; iCurCtrl++, control++) {
		if (control->type == PANEL_TAB || control->tabgroup != currentcontrol->tabgroup || !control->shownflag) continue;
		if (ISEDIT(control) && control->type != PANEL_FEDIT) {
			textlen = GetWindowTextLength(control->ctlhandle) + 1;
			if (control->textval != NULL) {
				control->textval = guiReallocMem(control->textval, textlen);
				if (control->textval == NULL) {
					sprintf_s(work, ARRAYSIZE(work), "Error in %s, guiReallocMem fail", __FUNCTION__);
					guiaErrorMsg(work, textlen);
					return;
				}
			}
			else {
				control->textval = guiAllocMem(textlen);
				if (control->textval == NULL) {
					sprintf_s(work, ARRAYSIZE(work), "Error in %s, guiAllocMem fail", __FUNCTION__);
					guiaErrorMsg(work, textlen);
					return;
				}
			}
			textlen = GetWindowText(control->ctlhandle, (LPSTR) control->textval, textlen);
			control->textval[textlen] = 0;
		}
		if (control->oldproc != NULL) SetWindowLongPtr(control->ctlhandle, GWLP_WNDPROC, (LONG_PTR) control->oldproc);
		ShowWindow(control->ctlhandle, SW_HIDE);
		control->shownflag = FALSE;
		if (res->ctlfocus == control) res->ctlfocus = NULL;
	}
}

/**
 * This always runs in the Windows thread
 */
void handleTabChangeChanging(HWND hwnd, LPARAM lParam, BOOL *focwasontab) {
	CONTROL *control, *currentcontrol;
	RESOURCE *res;
	INT i1, iCurCtrl, textlen;
	BOOL focset;
	LPNMHDR nmhdr = (LPNMHDR)lParam;
	if (nmhdr->code == TCN_SELCHANGING || nmhdr->code == TCN_SELCHANGE /* || nmhdr->code == GUIAM_TCN_SELCHANGE_NOFOCUS */)
	{
		currentcontrol = (CONTROL *) GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
		if (currentcontrol == NULL || !currentcontrol->tabgroup) return;
		pvistart();
		res = *currentcontrol->res;
		if (nmhdr->code == TCN_SELCHANGING) {
			HWND hwndCtrl;
			*focwasontab = FALSE;
			if ((hwndCtrl = GetFocus()) != NULL) {
				control = (CONTROL *) res->content;
				i1 = TabCtrl_GetCurSel(nmhdr->hwndFrom) + 1;
				for (iCurCtrl = 0; iCurCtrl < res->entrycount; iCurCtrl++, control++) {
					if (hwndCtrl == control->ctlhandle && control->tabgroup == currentcontrol->tabgroup
						&& control->tabsubgroup == i1) {
						if (res->focusmsgs) {
							rescb(res, DBC_MSGLOSEFOCUS, control->useritem, 0);
						}
						if ((*focwasontab = control->type == PANEL_TAB))
							drawTabHeaderFocusRectangle(control, FALSE);
						break;
					}
				}
			}
		}
		else if (nmhdr->code == TCN_SELCHANGE) {
			BOOL makecontrolWasCalled = FALSE;
			BOOL thereAreBoxes = FALSE;
			CONTROL* ctrls = (CONTROL *) res->content;
			WINDOW* win = *res->winshow;
			HideTabControls(currentcontrol);
			i1 = TabCtrl_GetCurSel(nmhdr->hwndFrom) + 1;
			focset = FALSE;
			for (int i2 = 0; i2 < res->entrycount; i2++)
			{
				control = &ctrls[i2];
				if (currentcontrol->tabgroup != control->tabgroup) continue;
				control->selectedtab = i1 - 1;
				if (control->tabsubgroup != i1) continue;
				if (control->type == PANEL_TAB) {
					if (res->itemmsgs && (textlen = (INT)strlen((LPSTR) control->textval))){
						memcpy(cbmsgdata, control->textval, min(textlen, MAXCBMSGSIZE - 17));
						rescb(res, DBC_MSGITEM, control->useritem, textlen);
					}
				}
				else {
					control->changeflag = TRUE;
					if (control->ctlhandle == NULL) {
						INT xoffset = (res->restype != RES_TYPE_DIALOG) ? win->bordertop - win->scrolly : 0;
						makecontrol(control, 0 - win->scrollx, xoffset, hwnd, TRUE);
						makecontrolWasCalled = TRUE;
						if (control->type == PANEL_BOXTITLE || control->type == PANEL_BOX) thereAreBoxes = TRUE;
					}
					else {
						ShowWindow(control->ctlhandle, SW_SHOW);
						// Set the control's window procedure
						// But not if it is a Table!
						if (control->type != PANEL_TABLE) setControlWndProc(control);
						if (ISEDIT(control) && control->type != PANEL_FEDIT) {
							SetWindowText(control->ctlhandle, (LPCSTR) control->textval);
						}
					}
					control->changeflag = FALSE;
				}
				control->shownflag = TRUE;

				if (!focset) {
					if (control->type == PANEL_TAB) {
						if (*focwasontab) {
							focset = TRUE;
							res->ctlfocus = control;
							drawTabHeaderFocusRectangle(control, TRUE);
							if (res->focusmsgs)	rescb(res, DBC_MSGFOCUS, control->useritem, 0);
						}
					}
					else if (res->ctlfocus != control && !*focwasontab
							&& control->useritem && !ISGRAY(control) && !ISSHOWONLY(control) && !ISNOFOCUS(control)
							&& control->type != PANEL_VTEXT && control->type != PANEL_RVTEXT && control->type != PANEL_CVTEXT) {
						if (ISRADIO(control)) {
							if (ISMARKED(control)) setctrlfocus(control);
						}
						else setctrlfocus(control);
						focset = TRUE;
						res->ctlfocus = control;
					}
				}
			}
			// Here is where we do things about boxes
			if (makecontrolWasCalled && thereAreBoxes) {
				for (int i2 = 0; i2 < res->entrycount; i2++) {
					control = &ctrls[i2];
					if (currentcontrol->tabgroup != control->tabgroup) continue;
					if (control->tabsubgroup != i1) continue;
					if (control->type == PANEL_BOXTITLE || control->type == PANEL_BOX) {
						RECT boxRect, ctrlRect, testRect;
						GetWindowRect(control->ctlhandle, &boxRect);
						// We have box, now find all controls in this tab that intersect with it
						for (int i3 = 0; i3 < res->entrycount; i3++) {
							CONTROL* ctrl3 = &ctrls[i3];
							if (ctrl3 == control) continue;
							if (currentcontrol->tabgroup != ctrl3->tabgroup) continue;
							if (ctrl3->tabsubgroup != i1 || ctrl3->type == PANEL_TAB) continue;
							GetWindowRect(ctrl3->ctlhandle, &ctrlRect);
							if (IntersectRect(&testRect, &boxRect, &ctrlRect))
								SetWindowPos(ctrl3->ctlhandle, control->ctlhandle, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
						}
					}
				}
			}
		}
		pviend();
	}
}
