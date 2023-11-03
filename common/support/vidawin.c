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
#include "includes.h"
#include "base.h"
#include "evt.h"
#include "vid.h"
#include "vidx.h"

#include <conio.h>
#if _MSC_VER >= 1800
#include <VersionHelpers.h>
#endif

#define INIT_MEMORY         0x00000001
#define INIT_CONSOLE_ACTIVE 0x00000004
#define INIT_NEW_CONSOLE    0x00000008
#define INIT_CONSOLE_MODE   0x00000010
#define INIT_THREAD         0x00000020
#define INIT_SIGNAL         0x00000040
//#define INIT_TERMDEF		0x00000100
#define INIT_INPUT			0x00000200
#define INIT_OUTPUT			0x00000400
#define INIT_STRING			0x00000800

#define INVALID_POSITION 0x20000000

#define FLAGS_IKEYS 0x0004
#define FLAGS_OLDBP 0x0008
#define FLAGS_NOCLO 0x0010

static INT flags = 0;

static INT (*vidcharcallbackfnc)(INT);
static INT breakeventid;
static HANDLE inhandle, outhandle, inactiveouthandle;
static DWORD saveinputmode;
static CONSOLE_SCREEN_BUFFER_INFO savecsbi;
static COORD cursor;
static CHAR_INFO *screenbufinfo;
static COORD screenbufsize;
static HANDLE keyhandle = NULL;
static DWORD keyid;
static CONSOLE_CURSOR_INFO savecci;
static INT xtop, xbottom, xleft, xright;
static INT cursortype;
static INT lastcursh, lastcursv;
static UINT caps1; // @suppress("Unused variable declaration in file scope")
static UINT caps2; // @suppress("Unused variable declaration in file scope")
static UINT inits;
static void cursorpos(void);
static WORD tociattr(INT);
static BOOL ctrlproc(DWORD);
static DWORD keyproc(DWORD);
static INT keyfixup(INPUT_RECORD *);

typedef HWND (*_GETCONSOLEWINDOW)(void);
static _GETCONSOLEWINDOW getconsolewindow;

/**
 * return 0 for success, RC_NO_MEM if a memory allocation fails, RC_ERROR for any other kind of problem
 */
INT vidastart(INT flag, INT eventid, CHAR *termdef, CHAR *indevice, CHAR *outdevice, CHAR *fontname, INT *hsize,
	INT *vsize, INT (*vidcharfnc)(INT), INT notused)
{
	INT i1, i2;
	UCHAR work[256];
	SECURITY_ATTRIBUTES sa;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	CHAR_INFO *ciptr;
	SMALL_RECT window;
	COORD maxwindow;
	HWND console;

	vidcharcallbackfnc = vidcharfnc;
	breakeventid = eventid;

	if (flag & VID_FLAG_IKEYS) flags |= FLAGS_IKEYS;
	if (flag & VID_FLAG_OLDBEEP) flags |= FLAGS_OLDBP;
	if (flag & VID_FLAG_NOCLOSE) flags |= FLAGS_NOCLO;

	inits = 0;
	if (AllocConsole()) inits |= INIT_NEW_CONSOLE;
	inhandle = GetStdHandle(STD_INPUT_HANDLE);
	outhandle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (inhandle == INVALID_HANDLE_VALUE || outhandle == INVALID_HANDLE_VALUE) {
		vidaexit();
		return RC_ERROR;
	}
	GetConsoleScreenBufferInfo(outhandle, &savecsbi);
	if (!(inits & INIT_NEW_CONSOLE) && !(flag & VID_FLAG_NOCON)) {
		inactiveouthandle = outhandle;
		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.lpSecurityDescriptor = NULL;
		sa.bInheritHandle = TRUE;
		outhandle = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CONSOLE_TEXTMODE_BUFFER, NULL);
		SetConsoleActiveScreenBuffer(outhandle);
		SetStdHandle(STD_OUTPUT_HANDLE, outhandle);
		SetStdHandle(STD_ERROR_HANDLE, outhandle);
		/* note: in some cases, the first getconsolescreenbufferinfo() may return */
		/*       zero size. getconsolescreenbufferinfo is called before */
		/*       createconsolescreenbuffer as createconsolescreenbuffer */
		/*       may return a screen that is a different size then the console */
		/*       that started dbc */
		if (!savecsbi.dwSize.X || !savecsbi.dwSize.Y) GetConsoleScreenBufferInfo(outhandle, &savecsbi);
		inits |= INIT_CONSOLE_ACTIVE;
	}
	if (!*hsize) *hsize = savecsbi.dwSize.X;
	if (*hsize == 0) *hsize = 80;  /* should not happen */
	if (!*vsize) *vsize = savecsbi.dwSize.Y;
	if (*vsize == 0) *vsize = 25;  /* should not happen */
	screenbufsize.X = *hsize;
	screenbufsize.Y = *vsize;
	screenbufinfo = malloc(screenbufsize.X * screenbufsize.Y * sizeof (CHAR_INFO));
	if (screenbufinfo == NULL) return RC_NO_MEM;
	GetConsoleScreenBufferInfo(outhandle, &csbi);
	window.Left = window.Top = 0;
	window.Bottom = csbi.srWindow.Bottom - csbi.srWindow.Top;
	window.Right = csbi.srWindow.Right - csbi.srWindow.Left;
	if (screenbufsize.Y <= window.Bottom || screenbufsize.X <= window.Right) {
		/* reduce window because new buffer is going to be smaller than window */
		if (screenbufsize.Y <= window.Bottom) window.Bottom = screenbufsize.Y - 1;
		if (screenbufsize.X <= window.Right) window.Right = screenbufsize.X - 1;
		SetConsoleWindowInfo(outhandle, TRUE, &window);
	}
	if (screenbufsize.Y != csbi.dwSize.Y || screenbufsize.X != csbi.dwSize.X)
		SetConsoleScreenBufferSize(outhandle, screenbufsize);
	if (screenbufsize.Y - 1 > window.Bottom || screenbufsize.X - 1 > window.Right) {
		window.Bottom = screenbufsize.Y - 1;
		window.Right = screenbufsize.X - 1;
		maxwindow = GetLargestConsoleWindowSize(outhandle);
		if (window.Bottom >= maxwindow.X) window.Bottom = maxwindow.X - 1;
		if (window.Right >= maxwindow.Y) window.Right = maxwindow.Y - 1;
		SetConsoleWindowInfo(outhandle, TRUE, &window);
	}
	ciptr = screenbufinfo;
	for (i1 = 0, i2 = screenbufsize.X * screenbufsize.Y; i1 < i2; i1++) {
		ciptr->Char.AsciiChar = ' ';
		(ciptr++)->Attributes = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
	}
	inits |= INIT_MEMORY;

	GetConsoleCursorInfo(outhandle, &savecci);
	GetConsoleMode(inhandle, &saveinputmode);
	SetConsoleMode(inhandle, (DWORD) 0);
	inits |= INIT_CONSOLE_MODE;

	cursortype = lastcursh = lastcursv = -1;
	xtop = xleft = INT_MAX;
	xbottom = xright = -1;

	/**
	 * For Win2000 and newer only, we can disable the CLOSE button on the
	 * console. The config option is dbcdx.display.console.closebutton=off
	 *
	 * 24JUL2019 Changed If test to IsWindowsVistaOrGreater from IsWindowsVersionOrGreater(5, 0, 0)
	 */
	if ((flags & FLAGS_NOCLO) && IsWindowsVistaOrGreater()) {
		HINSTANCE handle = LoadLibrary("Kernel32.dll");
		if (handle == NULL) {
			vidputerror("LoadLibrary(\"Kernel32.dll\") failed. ", VIDPUTERROR_NEW);
			sprintf(work, "GetLastError=%i", (int)GetLastError());
			vidputerror(work, VIDPUTERROR_APPEND);
			vidaexit();
			return RC_ERROR;
		}
		getconsolewindow = (_GETCONSOLEWINDOW) GetProcAddress(handle, "GetConsoleWindow");
		if (getconsolewindow == NULL) {
			vidputerror("GetProcAddress(handle, \"GetConsoleWindow\") failed", VIDPUTERROR_NEW);
			sprintf(work, "GetLastError=%i", (int)GetLastError());
			vidputerror(work, VIDPUTERROR_APPEND);
			vidaexit();
			return RC_ERROR;
		}
		console = getconsolewindow();
		if (console != NULL) {
			HMENU conmenu = GetSystemMenu(console, FALSE);
			DeleteMenu(conmenu, SC_CLOSE, MF_BYCOMMAND);
		}
	}

	if (vidcharcallbackfnc != NULL && (keyhandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) keyproc, NULL, 0, &keyid)) == NULL) {
		vidaexit();
		return RC_ERROR;
	}
	inits |= INIT_THREAD;
	SetConsoleCtrlHandler((PHANDLER_ROUTINE) ctrlproc, TRUE);
	inits |= INIT_SIGNAL;
	return(0);
}

void vidaexit()
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	SMALL_RECT window;

	if (!inits) return;

	if (inits & INIT_SIGNAL) {
		vidaflush();
		cursorpos();
		SetConsoleCtrlHandler((PHANDLER_ROUTINE) ctrlproc, FALSE);
	}
	if (inits & INIT_THREAD) {
		TerminateThread(keyhandle, 0);
		WaitForSingleObject(keyhandle, 500);
		CloseHandle(keyhandle);
	}

	if (inits & INIT_CONSOLE_MODE) {
		SetConsoleCursorInfo(outhandle, &savecci);
		SetConsoleMode(inhandle, saveinputmode);
	}
	if (inits & INIT_MEMORY) {
		/* reset buffer and window size */
		GetConsoleScreenBufferInfo(outhandle, &csbi);
		window.Left = window.Top = 0;
		window.Bottom = csbi.srWindow.Bottom - csbi.srWindow.Top;
		window.Right = csbi.srWindow.Right - csbi.srWindow.Left;
		if (savecsbi.dwSize.Y <= window.Bottom || savecsbi.dwSize.X <= window.Right) {
			/* reduce window because original buffer is smaller than current window */
			if (savecsbi.dwSize.Y <= window.Bottom) window.Bottom = savecsbi.dwSize.Y - 1;
			if (savecsbi.dwSize.X <= window.Right) window.Right = savecsbi.dwSize.X - 1;
			SetConsoleWindowInfo(outhandle, TRUE, &window);
			csbi.srWindow = window;
		}
		if (savecsbi.dwSize.Y != screenbufsize.Y || savecsbi.dwSize.X != screenbufsize.X)
			SetConsoleScreenBufferSize(outhandle, savecsbi.dwSize);
		if (savecsbi.srWindow.Top != csbi.srWindow.Top
				|| savecsbi.srWindow.Bottom != csbi.srWindow.Bottom
				|| savecsbi.srWindow.Left != csbi.srWindow.Left
				|| savecsbi.srWindow.Right != csbi.srWindow.Right)
			SetConsoleWindowInfo(outhandle, TRUE, &savecsbi.srWindow);
		free(screenbufinfo);
	}
	if (inits & INIT_NEW_CONSOLE) FreeConsole();
	else if (inits & INIT_CONSOLE_ACTIVE) {
		outhandle = inactiveouthandle;
		SetConsoleActiveScreenBuffer(outhandle);
		SetStdHandle(STD_OUTPUT_HANDLE, outhandle);
		SetStdHandle(STD_ERROR_HANDLE, outhandle);

		/**
		 * For Win2000 and newer only, we can disable the CLOSE button on the
		 * console. The config option is dbcdx.display.console.noclose=yes
		 * We must put it back when done.
		 */
		if ((flags & FLAGS_NOCLO)
				&& IsWindowsVersionOrGreater(5, 0, 0) /*osInfo.dwMajorVersion >= 5*/
				&& getconsolewindow != NULL) {
			HWND console = getconsolewindow();
			if (console != NULL) GetSystemMenu(console, TRUE);
		}
	}

	inits = 0;
}

void vidasuspend(INT attr)
{
	vidaflush();
	cursorpos();
	if (keyhandle != NULL) SuspendThread(keyhandle);
	SetConsoleMode(inhandle, saveinputmode);
}

void vidaresume(void)
{
	SetConsoleMode(inhandle, (DWORD) 0);
	if (keyhandle != NULL) ResumeThread(keyhandle);
}

void vidaflush()
{
	SMALL_RECT destrect;
	COORD hv;

	if (xtop == INT_MAX) return;
	hv.X = destrect.Left = (SHORT) xleft;
	hv.Y = destrect.Top = (SHORT) xtop;
	destrect.Bottom = (SHORT) xbottom;
	destrect.Right = (SHORT) xright;
	WriteConsoleOutput(outhandle, screenbufinfo, screenbufsize, hv, &destrect);
	xtop = xleft = INT_MAX;
	xbottom = xright = -1;
}

void vidacursor(INT type, INT attr)
{
	CONSOLE_CURSOR_INFO cci;

	if (type == cursortype) return;

	cursortype = type;
	if (type == CURSOR_NONE) {
		cci.bVisible = FALSE;
		cci.dwSize = 100;
	}
	else {
		cursorpos();
		cci.bVisible = TRUE;
		if (type == CURSOR_BLOCK) cci.dwSize = 99;
		else if (type == CURSOR_ULINE) cci.dwSize = 10;
		else if (type == CURSOR_HALF) cci.dwSize = 50;
		else return;
	}
	SetConsoleCursorInfo(outhandle, &cci);
}

void vidaposition(INT h, INT v, INT attr)
{
	if (cursor.X == h && cursor.Y == v) return;
	cursor.X = h;
	cursor.Y = v;
	if (cursortype != CURSOR_NONE) cursorpos();
}

/**
 * Low order byte of charattr is ignored unless string is NULL
 */
void vidaputstring(UCHAR *string, INT length, INT charattr)
{
	static UCHAR graphics[4][16] = {  /* added 0x2B to fill out row to 16 bytes, so bit op's can be used */
		{ 0xC4, 0xB3, 0xC5, 0xDA, 0xBF, 0xC0, 0xD9, 0xC3, 0xC2, 0xB4, 0xC1, 0x18, 0x1A, 0x19, 0x1B, 0x2B },
		{ 0xCD, 0xB3, 0xD8, 0xD5, 0xB8, 0xD4, 0xBE, 0xC6, 0xD1, 0xB5, 0xCF, 0x18, 0x1A, 0x19, 0x1B, 0x2B },
		{ 0xC4, 0xBA, 0xD7, 0xD6, 0xB7, 0xD3, 0xBD, 0xC7, 0xD2, 0xB6, 0xD0, 0x18, 0x1A, 0x19, 0x1B, 0x2B },
		{ 0xCD, 0xBA, 0xCE, 0xC9, 0xBB, 0xC8, 0xBC, 0xCC, 0xCB, 0xB9, 0xCA, 0x18, 0x1A, 0x19, 0x1B, 0x2B }
	};
	INT i1;
	UCHAR c1;
	WORD ciattr;
	CHAR_INFO *ciptr;

	if (length < 1) return;
	if (charattr & (DSPATTR_RAWMODE | DSPATTR_AUXPORT | DSPATTR_KYDSPIPE)) return;
	i1 = length;
	ciptr = screenbufinfo + cursor.Y * screenbufsize.X + cursor.X;
	ciattr = tociattr(charattr);
	if (!(charattr & DSPATTR_DISPDOWN)) {
		if (string == NULL) {
			c1 = (UCHAR) charattr;
			if (charattr & DSPATTR_GRAPHIC) c1 = graphics[c1 >> 6][c1 & 0x0F];
			while (i1--) {
				ciptr->Char.AsciiChar = (CHAR) c1;
				(ciptr++)->Attributes = ciattr;
			}
		}
		else {
			while (i1--) {
				c1 = *string++;
				if (charattr & DSPATTR_GRAPHIC) c1 = graphics[c1 >> 6][c1 & 0x0F];
				ciptr->Char.AsciiChar = (CHAR) c1;
				(ciptr++)->Attributes = ciattr;
			}
		}
		if ((INT) cursor.Y < xtop) xtop = cursor.Y;
		if ((INT) cursor.Y > xbottom) xbottom = cursor.Y;
		if ((INT) cursor.X < xleft) xleft = cursor.X;
		if ((INT)(cursor.X + length - 1) > xright) xright = cursor.X + length - 1;
		cursor.X += length;
	}
	else {
		while (i1--) {
			if (string == NULL) c1 = (UCHAR) charattr;
			else c1 = *string++;
			if (charattr & DSPATTR_GRAPHIC) c1 = graphics[c1 >> 6][c1 & 0x0F];
			ciptr->Char.AsciiChar = (CHAR) c1;
			ciptr->Attributes = ciattr;
			ciptr += screenbufsize.X;
		}
		if ((INT) cursor.Y < xtop) xtop = cursor.Y;
		if ((INT)(cursor.Y + length - 1) > xbottom) xbottom = cursor.Y + length - 1;
		if ((INT) cursor.X < xleft) xleft = cursor.X;
		if ((INT) cursor.X > xright) xright = cursor.X;
		cursor.Y += length;
	}
	if (cursortype != CURSOR_NONE) cursorpos();
}

void vidaeraserect(INT top, INT bottom, INT left, INT right, INT attr)
{
	INT i1, i2;
	CHAR_INFO ci, *ciptr;

	if (top < xtop) xtop = top;
	if (bottom > xbottom) xbottom = bottom;
	if (left < xleft) xleft = left;
	if (right > xright) xright = right;

	ci.Char.AsciiChar = ' ';
	ci.Attributes = tociattr(attr);
	for (i1 = top; i1 <= bottom; i1++) {
		ciptr = screenbufinfo + i1 * screenbufsize.X + left;
		for (i2 = left; i2 <= right; i2++) *ciptr++ = ci;
	}
}

/**
 * Each char/attribute is 1 or 3 bytes, 1st is char, 2nd is color byte, 3rd is attribute
 */
void vidagetrect(UCHAR *mem, INT top, INT bottom, INT left, INT right, INT charonly)
{
	INT i1, i2;
	SMALL_RECT destrect;
	CHAR_INFO *ciptr;
	COORD hv;

	vidaflush();
	hv.X = destrect.Left = left;
	hv.Y = destrect.Top = top;
	destrect.Right = right;
	destrect.Bottom = bottom;
	ReadConsoleOutput(outhandle, screenbufinfo, screenbufsize, hv, &destrect);

	if (charonly) {
		for (i1 = top; i1 <= bottom; i1++) {
			ciptr = screenbufinfo + i1 * screenbufsize.X + left;
			for (i2 = left; i2 <= right; i2++) *mem++ = (ciptr++)->Char.AsciiChar;
		}
	}
	else {
		for (i1 = top; i1 <= bottom; i1++) {
			ciptr = screenbufinfo + i1 * screenbufsize.X + left;
			for (i2 = left; i2 <= right; i2++) {
				*mem++ = ciptr->Char.AsciiChar;
#ifdef I386
				*((WORD *) mem) = (ciptr++)->Attributes;
#else
				memcpy(mem, &ciptr->Attributes, 2);
				ciptr++;
#endif
				mem += 2;
			}
		}
	}
}

void vidaputrect(UCHAR *mem, INT top, INT bottom, INT left, INT right, INT charonly, INT attr)
/* each char/attribute is 1 or 3 bytes, 1st is char, 2nd is color byte, 3rd is attribute */
{
	INT i1, i2;
	CHAR_INFO *ciptr;
	WORD ciattr;

	if (top < xtop) xtop = top;
	if (bottom > xbottom) xbottom = bottom;
	if (left < xleft) xleft = left;
	if (right > xright) xright = right;

	if (charonly) {
		ciattr = tociattr(attr);
		for (i1 = top; i1 <= bottom; i1++) {
			ciptr = screenbufinfo + i1 * screenbufsize.X + left;
			for (i2 = left; i2 <= right; i2++) {
				ciptr->Char.AsciiChar = *mem++;
				(ciptr++)->Attributes = ciattr;
			}
		}
	}
	else {
		for (i1 = top; i1 <= bottom; i1++) {
			ciptr = screenbufinfo + i1 * screenbufsize.X + left;
			for (i2 = left; i2 <= right; i2++) {
				ciptr->Char.AsciiChar = *mem++;
#ifdef I386
				(ciptr++)->Attributes = *((WORD *) mem);
#else
				memcpy(&ciptr->Attributes, mem, 2);
				ciptr++;
#endif
				mem += 2;
			}
		}
	}
}

void vidamove(INT direction, INT top, INT bottom, INT left, INT right, INT attr)
{
	INT i1, i2;
	CHAR_INFO *srcinfo, *destinfo, ciblank;

	if ((top == bottom && (direction == ROLL_UP || direction == ROLL_DOWN)) ||
	    (left == right && (direction == ROLL_LEFT || direction == ROLL_RIGHT))) {
		vidaeraserect(top, bottom, left, right, attr);
		return;
	}

	if (attr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE)) return;

	ciblank.Char.AsciiChar = ' ';
	ciblank.Attributes = tociattr(attr);
	if (direction == ROLL_UP) {
		for (i1 = top; i1 < bottom; i1++) {
			destinfo = screenbufinfo + i1 * screenbufsize.X + left;
			srcinfo = destinfo + screenbufsize.X;
			for (i2 = left; i2 <= right; i2++) *destinfo++ = *srcinfo++;
		}
		destinfo = screenbufinfo + bottom * screenbufsize.X + left;
		for (i2 = left; i2 <= right; i2++) *destinfo++ = ciblank;
	}
	else if (direction == ROLL_DOWN) {
		for (i1 = bottom; i1 > top; i1--) {
			destinfo = screenbufinfo + i1 * screenbufsize.X + left;
			srcinfo = destinfo - screenbufsize.X;
			for (i2 = left; i2 <= right; i2++) *destinfo++ = *srcinfo++;
		}
		destinfo = screenbufinfo + top * screenbufsize.X + left;
		for (i2 = left; i2 <= right; i2++) *destinfo++ = ciblank;
	}
	else if (direction == ROLL_LEFT) {
		for (i1 = top; i1 <= bottom; i1++) {
			destinfo = screenbufinfo + i1 * screenbufsize.X + left;
			srcinfo = destinfo + 1;
			for (i2 = left; i2 < right; i2++) *destinfo++ = *srcinfo++;
			*destinfo = ciblank;
		}
	}
	else if (direction == ROLL_RIGHT) {
		for (i1 = top; i1 <= bottom; i1++) {
			destinfo = screenbufinfo + i1 * screenbufsize.X + right;
			srcinfo = destinfo - 1;
			for (i2 = left; i2 < right; i2++) *destinfo-- = *srcinfo--;
			*destinfo = ciblank;
		}
	}
	else return;

	if (top < xtop) xtop = top;
	if (bottom > xbottom) xbottom = bottom;
	if (left < xleft) xleft = left;
	if (right > xright) xright = right;
}

void vidainsertchar(INT h, INT v, INT top, INT bottom, INT left, INT right, INT charattr)
{
	INT endh, endv, len;
	CHAR_INFO *srcinfo, *destinfo, ciwork;

	if (charattr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE)) return;

	endh = h;
	endv = v;
	if (--h < left) {
		h = right;
		v--;
	}
	while (v > cursor.Y || (v == cursor.Y && h == right)) {
		if (h == right) {
			srcinfo = screenbufinfo + v * screenbufsize.X + right;
			destinfo = screenbufinfo + (v + 1) * screenbufsize.X + left;
			*destinfo = *srcinfo;
			if (--h < left) {
				h = right;
				v--;
			}
		}
		else {
			srcinfo = screenbufinfo + v * screenbufsize.X + h;
			destinfo = srcinfo + 1;
			for (len = h - left + 1; len--; ) *destinfo-- = *srcinfo--;
			h = right;
			v--;
		}
	}
	if (cursor.X != right) {
		srcinfo = screenbufinfo + v * screenbufsize.X + h;
		destinfo = srcinfo + 1;
		for (len = h - cursor.X + 1; len--; ) *destinfo-- = *srcinfo--;
	}
	ciwork.Char.AsciiChar = charattr & 0xFF;
	ciwork.Attributes = tociattr(charattr);
	destinfo = screenbufinfo + cursor.Y * screenbufsize.X + cursor.X;
	*destinfo = ciwork;
	if ((INT) cursor.Y == endv) {
		if ((INT) cursor.X < xleft) xleft = cursor.X;
		if (endh > xright) xright = endh;
	}
	else {
		if (left < xleft) xleft = left;
		if (right > xright) xright = right;
	}
	if ((INT) cursor.Y < xtop) xtop = cursor.Y;
	if (endv > xbottom) xbottom = endv;
}

void vidadeletechar(INT h, INT v, INT top, INT bottom, INT left, INT right, INT charattr)
{
	INT endh, endv, len;
	CHAR_INFO *srcinfo, *destinfo, ciwork;

	if (charattr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE)) return;

	endh = h;
	endv = v;
	h = cursor.X;
	v = cursor.Y;
	if (++h > right) {
		h = left;
		v++;
	}
	while (v < endv || (v == endv && h == left)) {
		if (h == left) {
			srcinfo = screenbufinfo + v * screenbufsize.X + left;
			destinfo = screenbufinfo + (v - 1) * screenbufsize.X + right;
			*destinfo = *srcinfo;
			if (++h > right) {
				h = left;
				v++;
			}
		}
		else {
			srcinfo = screenbufinfo + v * screenbufsize.X + h;
			destinfo = srcinfo - 1;
			for (len = right - h + 1; len--; ) *destinfo++ = *srcinfo++;
			h = left;
			v++;
		}
	}
	if (endh != left) {
		srcinfo = screenbufinfo + v * screenbufsize.X + h;
		destinfo = srcinfo - 1;
		for (len = endh - h + 1; len--; ) *destinfo++ = *srcinfo++;
	}
	ciwork.Char.AsciiChar = charattr & 0xFF;
	ciwork.Attributes = tociattr(charattr);
	destinfo = screenbufinfo + endv * screenbufsize.X + endh;
	*destinfo = ciwork;
	if ((INT) cursor.Y == endv) {
		if ((INT) cursor.X < xleft) xleft = cursor.X;
		if (endh > xright) xright = endh;
	}
	else {
		if (left < xleft) xleft = left;
		if (right > xright) xright = right;
	}
	if ((INT) cursor.Y < xtop) xtop = cursor.Y;
	if (endv > xbottom) xbottom = endv;
}

void vidasound(INT pitch, INT time, INT attr, INT beepflag)
{
	if (attr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE)) return;
	vidaflush();
	if (beepflag && (flags & FLAGS_OLDBP)) MessageBeep(MB_OK);
	else Beep(pitch, time * 100);
}

static void cursorpos()
{
	if ((INT) cursor.X == lastcursh && (INT) cursor.Y == lastcursv) return;
	if (xtop != INT_MAX) vidaflush();
	SetConsoleCursorPosition(outhandle, cursor);
	lastcursh = cursor.X;
	lastcursv = cursor.Y;
}

/* convert from vid attr to CHAR_INFO Attributes */
static WORD tociattr(INT attr)
{
	WORD ciattr;
	INT i1, fgcolor, bgcolor;

	fgcolor = (attr & DSPATTR_FGCOLORMASK) >> DSPATTR_FGCOLORSHIFT;
	bgcolor = (attr & DSPATTR_BGCOLORMASK) >> DSPATTR_BGCOLORSHIFT;
	if (attr & DSPATTR_REV) {
		i1 = fgcolor;
		fgcolor = bgcolor;
		bgcolor = i1;
	}
	if (attr & DSPATTR_BOLD) fgcolor |= DSPATTR_INTENSE;

	ciattr = 0;
	if (fgcolor & DSPATTR_INTENSE) {
		fgcolor &= ~DSPATTR_INTENSE;
		ciattr |= FOREGROUND_INTENSITY;
	}
	switch (fgcolor) {
		case DSPATTR_BLACK:
			break;
		case DSPATTR_BLUE:
			ciattr |= FOREGROUND_BLUE;
			break;
		case DSPATTR_GREEN:
			ciattr |= FOREGROUND_GREEN;
			break;
		case DSPATTR_CYAN:
			ciattr |= FOREGROUND_BLUE | FOREGROUND_GREEN;
			break;
		case DSPATTR_RED:
			ciattr |= FOREGROUND_RED;
			break;
		case DSPATTR_MAGENTA:
			ciattr |= FOREGROUND_BLUE | FOREGROUND_RED;
			break;
		case DSPATTR_YELLOW:
			ciattr |= FOREGROUND_GREEN | FOREGROUND_RED;
			break;
		case DSPATTR_WHITE:
			ciattr |= FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
			break;
	}
	if (bgcolor & DSPATTR_INTENSE) {
		bgcolor &= ~DSPATTR_INTENSE;
		ciattr |= BACKGROUND_INTENSITY;
	}
	switch (bgcolor) {
		case DSPATTR_BLACK:
			break;
		case DSPATTR_BLUE:
			ciattr |= BACKGROUND_BLUE;
			break;
		case DSPATTR_GREEN:
			ciattr |= BACKGROUND_GREEN;
			break;
		case DSPATTR_CYAN:
			ciattr |= BACKGROUND_BLUE | BACKGROUND_GREEN;
			break;
		case DSPATTR_RED:
			ciattr |= BACKGROUND_RED;
			break;
		case DSPATTR_MAGENTA:
			ciattr |= BACKGROUND_BLUE | BACKGROUND_RED;
			break;
		case DSPATTR_YELLOW:
			ciattr |= BACKGROUND_GREEN | BACKGROUND_RED;
			break;
		case DSPATTR_WHITE:
			ciattr |= BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED;
			break;
	}
	return(ciattr);
}

static BOOL ctrlproc(DWORD type)
{
/*** CODE CHECK: REMOVE NEXT LINE WHEN BUG IN WINDOWS API GETS FIXED ***/
	type = CTRL_BREAK_EVENT;
	switch (type) {
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
			evtset(breakeventid);
			Sleep(1000);
			return(FALSE);
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			vidaexit();
			exit(1);
	}
	return(FALSE);
}

static DWORD keyproc(DWORD parm)
{
	INT chr, newnumpad, numpad;
	UINT i1;
	DWORD keystate, numrec;
	INPUT_RECORD inrec[128];

	numpad = 0;
	for ( ; ; ) {
		if (WaitForSingleObject(inhandle, INFINITE) != WAIT_OBJECT_0) continue;
		if (!ReadConsoleInput(inhandle, inrec, ARRAYSIZE(inrec) /*sizeof(inrec) / sizeof(*inrec)*/, &numrec)) {
			continue;
		}
		for (i1 = 0; i1 < numrec; i1++) {
			if (inrec[i1].EventType == KEY_EVENT) {
				if (!inrec[i1].Event.KeyEvent.bKeyDown) {
					if (inrec[i1].Event.KeyEvent.wVirtualKeyCode == VK_MENU) {
						if (numpad & 0xFF) while (vidcharcallbackfnc(numpad & 0xFF) == -1) Sleep(10);
						numpad = 0;
					}
					continue;
				}
				keystate = inrec[i1].Event.KeyEvent.dwControlKeyState;
				chr = (UCHAR) inrec[i1].Event.KeyEvent.uChar.AsciiChar;
				if (chr && (chr != 0xE0 || !(keystate & ENHANCED_KEY))) {
					numpad = 0;
					if (chr == 0x0A || chr == 0x0D) chr = VID_ENTER;
					else if (chr == 0x08) chr = VID_BKSPC;
					else if (chr == 0x1B) chr = VID_ESCAPE;
					else if (chr == 0x09) {
						if (!(keystate & SHIFT_PRESSED)) chr = VID_TAB;
						else chr = VID_BKTAB;
					}
					else if (keystate & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) {
						if (chr >= 'A' && chr <= 'Z') chr = VID_ALTA + chr - 'A';
						else if (chr >= 'a' && chr <= 'z') chr = VID_ALTA + chr - 'a';
					}
					else if (chr == 0x03 && breakeventid) {
						evtset(breakeventid);
						continue;
					}
				}
				else {
					newnumpad = 0;
					switch (inrec[i1].Event.KeyEvent.wVirtualKeyCode) {
						case VK_BACK:
							chr = VID_BKSPC;
							break;
						case VK_TAB:
							if (!(keystate & SHIFT_PRESSED)) chr = VID_TAB;
							else chr = VID_BKTAB;
							break;
						case VK_CLEAR:
							if ((keystate & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) && !(keystate & ENHANCED_KEY)) newnumpad = numpad * 10 + 5;
							break;
						case VK_RETURN:
							chr = VID_ENTER;
							break;
						case VK_ESCAPE:
							chr = VID_ESCAPE;
							break;
						case VK_SPACE:
							chr = ' ';
							break;
						case VK_PRIOR:
							if ((keystate & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) && !(keystate & ENHANCED_KEY)) newnumpad = numpad * 10 + 9;
							else chr = VID_PGUP;
							break;
						case VK_NEXT:
							if ((keystate & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) && !(keystate & ENHANCED_KEY)) newnumpad = numpad * 10 + 3;
							else chr = VID_PGDN;
							break;
						case VK_END:
							if ((keystate & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) && !(keystate & ENHANCED_KEY)) newnumpad = numpad * 10 + 1;
							else chr = VID_END;
							break;
						case VK_HOME:
							if ((keystate & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) && !(keystate & ENHANCED_KEY)) newnumpad = numpad * 10 + 7;
							else chr = VID_HOME;
							break;
						case VK_LEFT:
							if ((keystate & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) && !(keystate & ENHANCED_KEY)) newnumpad = numpad * 10 + 4;
							else chr = VID_LEFT;
							break;
						case VK_UP:
							if ((keystate & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) && !(keystate & ENHANCED_KEY)) newnumpad = numpad * 10 + 8;
							else chr = VID_UP;
							break;
						case VK_RIGHT:
							if ((keystate & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) && !(keystate & ENHANCED_KEY)) newnumpad = numpad * 10 + 6;
							else chr = VID_RIGHT;
							break;
						case VK_DOWN:
							if ((keystate & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) && !(keystate & ENHANCED_KEY)) newnumpad = numpad * 10 + 2;
							else chr = VID_DOWN;
							break;
						case VK_INSERT:
							if ((keystate & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) && !(keystate & ENHANCED_KEY)) newnumpad = numpad * 10;
							else chr = VID_INSERT;
							break;
						case VK_DELETE:
							chr = VID_DELETE;
							break;
						case VK_F1:
							chr = VID_F1;
							break;
						case VK_F2:
							chr = VID_F2;
							break;
						case VK_F3:
							chr = VID_F3;
							break;
						case VK_F4:
							chr = VID_F4;
							break;
						case VK_F5:
							chr = VID_F5;
							break;
						case VK_F6:
							chr = VID_F6;
							break;
						case VK_F7:
							chr = VID_F7;
							break;
						case VK_F8:
							chr = VID_F8;
							break;
						case VK_F9:
							chr = VID_F9;
							break;
						case VK_F10:
							chr = VID_F10;
							break;
						case VK_F11:
							chr = VID_F11;
							break;
						case VK_F12:
							chr = VID_F12;
							break;
						case VK_F13:
							chr = VID_F13;
							break;
						case VK_F14:
							chr = VID_F14;
							break;
						case VK_F15:
							chr = VID_F15;
							break;
						case VK_F16:
							chr = VID_F16;
							break;
						case VK_F17:
							chr = VID_F17;
							break;
						case VK_F18:
							chr = VID_F18;
							break;
						case VK_F19:
							chr = VID_F19;
							break;
						case VK_F20:
							chr = VID_F20;
							break;
					}
					numpad = newnumpad;
					if (!chr) continue;

					if (keystate & SHIFT_PRESSED) {
						if (keystate & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) continue;
						if (chr >= VID_F1 && chr <= VID_F20) chr = VID_SHIFTF1 + chr - VID_F1;
						else if (chr >= VID_UP && chr <= VID_PGDN) chr = VID_SHIFTUP + chr - VID_UP;
					}
					else if (keystate & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)) {
						if (keystate & (SHIFT_PRESSED | RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) continue;
						if (chr >= VID_F1 && chr <= VID_F20) chr = VID_CTLF1 + chr - VID_F1;
						else if (chr >= VID_UP && chr <= VID_PGDN) chr = VID_CTLUP + chr - VID_UP;
					}
					else if (keystate & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) {
						if (keystate & (SHIFT_PRESSED | RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)) continue;
						if (chr >= VID_F1 && chr <= VID_F20) chr = VID_ALTF1 + chr - VID_F1;
						else if (chr >= VID_UP && chr <= VID_PGDN) chr = VID_ALTUP + chr - VID_UP;
					}
				}
				while (inrec[i1].Event.KeyEvent.wRepeatCount--)
					while (vidcharcallbackfnc(chr) == -1) Sleep(10);
			}
		}
	}
	return(0);
}

#define MAXSCAN 128
static UCHAR keydata[MAXSCAN + 1][8];	/* key chars: [0] = normal */
								/*            [1] = shifted */
								/*            [2] = ctrl-alt */
								/*            [3] = shifted ctrl-alt */
								/*            [4] = normal w/ caps lock */
								/*            [5] = shifted w/ caps lock */
								/*            [6] = ctrl-alt w/ caps lock */
								/*            [7] = shifted ctrl-alt w/ caps lock */
static UCHAR keydead[MAXSCAN + 1];		/* bit mask for dead keys (low order = normal) */

#if 0
static INT keyinit()
{
	INT i1, i2, result;
	UINT spcscan, vk;
	BYTE kbuf[256], keystate[256];
	UCHAR cbuf[4];

	/* save keyboard state because ToAscii bug can alter it */
	if (!GetKeyboardState(keystate)) memset(keystate, 0, sizeof(keystate));

	spcscan = MapVirtualKey(VK_SPACE, 0);  /* read space scancode */
	memset(kbuf, 0, sizeof(kbuf));  /* all keys up */

	/* clear any pending dead keys */
	ToAscii(VK_SPACE, spcscan, kbuf, (LPWORD) cbuf, 0);

	for (i1 = 1; i1 <= MAXSCAN; i1++) {
		vk = MapVirtualKey(i1, 1);  /* find the associated virtual key */
		for (i2 = 0; i2 < 8; i2++) {
			kbuf[VK_SHIFT] = ((UCHAR) i2 & 0x01) << 7;
			kbuf[VK_CONTROL] = ((UCHAR) i2 & 0x02) << 6;
			kbuf[VK_MENU] = ((UCHAR) i2 & 0x02) << 6;
			kbuf[VK_CAPITAL] = ((UCHAR) i2 & 0x04) >> 2;

			result = ToAscii(vk, i1, kbuf, (LPWORD) cbuf, 0);
			if (!result) cbuf[0] = 0;
			else if (result < 0) {  /* dead key in cbuf[0] */
				keydead[i1] |= (UCHAR)(0x01 << i2);
				kbuf[VK_MENU] = 0;
				ToAscii(VK_SPACE, spcscan, kbuf, (LPWORD) cbuf, 0);
			}
			else if (result == 2) cbuf[0] = cbuf[2];  /* this shouldn't happen */
			/* else assume 1 length character in cbuf[0] */

			if (cbuf[0] >= 128) {  /* convert from windows code page to MS-DOS code page */
				cbuf[1] = 0;
				CharToOem((LPCTSTR) cbuf, (LPSTR) cbuf);
			}

			keydata[i1][i2] = cbuf[0];
		}
	}

	/* restore keyboard state because ToAscii bug can alter it */
	SetKeyboardState(keystate);
	return(0);
}
#endif

static INT keyfixup(INPUT_RECORD *inrec) // @suppress("Unused static function")
{
	static BYTE kbuf[256];
	static INT deadscan;
	static INT deadvkey;
	static INT deadstate;
	INT chr, result, scan, state, vkey;
	DWORD keystate;
	UCHAR cbuf[4];

	scan = inrec->Event.KeyEvent.wVirtualScanCode;
	vkey = inrec->Event.KeyEvent.wVirtualKeyCode;
	state = 0;
	keystate = inrec->Event.KeyEvent.dwControlKeyState;
	if (keystate & SHIFT_PRESSED) state += 1;
	if (keystate & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)) {
		if (keystate & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) state += 2;
		else scan = MAXSCAN + 1;
	}
	else if ((flags & FLAGS_IKEYS) && (keystate & RIGHT_ALT_PRESSED)) state += 2;
	if (keystate & CAPSLOCK_ON) state += 4;

	chr = 0;
	if (scan <= MAXSCAN) {
		if (scan == 53 && vkey == 189 && (keystate & ENHANCED_KEY)) {
			/* need this to fix Win95 bug */
			inrec->Event.KeyEvent.uChar.AsciiChar = '/';
		}
		else if (keydata[scan][state]) {
			if (keydead[scan] & (0x01 << state)) {  /* dead key */
				deadscan = scan;
				deadvkey = vkey;
				deadstate = state;
				inrec->Event.KeyEvent.uChar.AsciiChar = 0;
				return(0);
			}
			if (deadscan) {  /* last key was a dead key */
				/* clear any pending dead keys */
				kbuf[VK_MENU] = 0;
				ToAscii(VK_SPACE, MapVirtualKey(VK_SPACE, 0), kbuf, (LPWORD) cbuf, 0);

				/* convert dead key */
				kbuf[VK_SHIFT] = ((UCHAR) deadstate & 0x01) << 7;
				kbuf[VK_CONTROL] = ((UCHAR) deadstate & 0x02) << 6;
				kbuf[VK_MENU] = ((UCHAR) deadstate & 0x02) << 6;
				kbuf[VK_CAPITAL] = ((UCHAR) deadstate & 0x04) >> 2;
				ToAscii(deadvkey, deadscan, kbuf, (LPWORD) cbuf, 0);

				/* convert character */
				kbuf[VK_SHIFT] = ((UCHAR) state & 0x01) << 7;
				kbuf[VK_CONTROL] = ((UCHAR) state & 0x02) << 6;
				kbuf[VK_MENU] = ((UCHAR) state & 0x02) << 6;
				kbuf[VK_CAPITAL] = ((UCHAR) state & 0x04) >> 2;
				result = ToAscii(inrec->Event.KeyEvent.wVirtualKeyCode, scan, kbuf, (LPWORD) cbuf, 0);
				if (result >= 1) {
					if (result > 1) {
						if (cbuf[0] >= 128) {  /* convert from windows code page to MS-DOS code page */
							cbuf[1] = 0;
							CharToOem((LPCTSTR) cbuf, (LPSTR) cbuf);
						}
						chr = cbuf[0];
						cbuf[0] = cbuf[2];
					}
					if (cbuf[0] >= 128) {  /* convert from windows code page to MS-DOS code page */
						cbuf[1] = 0;
						CharToOem((LPCTSTR) cbuf, (LPSTR) cbuf);
					}
					inrec->Event.KeyEvent.uChar.AsciiChar = cbuf[0];
				}
				else {
					if (result < 0) {  /* dead key shouldn't happen */
						kbuf[VK_MENU] = 0;
						ToAscii(VK_SPACE, MapVirtualKey(VK_SPACE, 0), kbuf, (LPWORD) cbuf, 0);
					}
					inrec->Event.KeyEvent.uChar.AsciiChar = keydata[scan][state];
				}
			}
			else inrec->Event.KeyEvent.uChar.AsciiChar = keydata[scan][state];
		}
		else if (state & 0x02) {  /* ctrl-alt char not supported */
			state &= ~0x03;
			if (keydata[scan][state] || keydata[scan][state + 1]) inrec->Event.KeyEvent.uChar.AsciiChar = 0;
		}
	}
	if (vkey != VK_SHIFT && vkey != VK_CONTROL && vkey != VK_MENU &&
	    vkey != VK_PAUSE && vkey != VK_CAPITAL && vkey != VK_NUMLOCK &&
	    vkey != VK_SCROLL) deadscan = 0;
	return(chr);
}
