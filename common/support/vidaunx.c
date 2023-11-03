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
#define INC_SIGNAL
#define INC_ERRNO
#include "includes.h"
#include "base.h"
#include "evt.h"
#include "fio.h"
#include "vid.h"
#include "vidx.h"

#if OS_UNIX
#define ARRAYSIZE(A) (sizeof(A)/sizeof((A)[0]))
#endif

#include <sys/time.h>
#include <unistd.h>
#include <termios.h>
#include <ncurses.h>
#include <sys/types.h>
#include <fcntl.h>
#include <term.h>
#include <signal.h>

#ifndef STDIN_FILENO
#define STDIN_FILENO (fileno(stdin))
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO (fileno(stdout))
#endif

#define llhh(p) (((UINT)(*(p + 1)) << 8) + *(p))

#define CAP1_ASCII_CONTROL	0x00000001
#define CAP1_COLOR_ERASE	0x00000002
#define CAP1_NO_ROLL		0x00000004
#define CAP1_SCROLL_ERASE	0x00000008
#define CAP1_SCROLL_REPOS	0x00000010
#define CAP1_POS			0x00000020
#define CAP1_POS_HORZ		0x00000040
#define CAP1_POS_VERT		0x00000080
#define CAP1_ES				0x00000100
#define CAP1_EF				0x00000200
#define CAP1_EL				0x00000400
#define CAP1_ERASE_WIN		0x00000800
#define CAP1_ERASE_WIN_ATTR	0x00001000
#define CAP1_SET_TB			0x00002000
#define CAP1_SET_LR			0x00004000
#define CAP1_DISABLE_ENABLE	0x00008000
#define CAP1_INSERT_CHAR	0x00010000
#define CAP1_DELETE_CHAR	0x00020000
#define CAP1_INSERT_LINE	0x00040000
#define CAP1_DELETE_LINE	0x00080000
#define CAP1_CURSOR_UNDERLINE	0x00100000
#define CAP1_CURSOR_BLOCK	0x00200000
#define CAP1_CURSOR_HALF	0x00400000
#define CAP1_FGCOLOR_ANSI256 0x00800000
#define CAP1_BGCOLOR_ANSI256 0x01000000

#define CAP2_ROLL_UP		0x00000001
#define CAP2_ROLL_DOWN		0x00000002
#define CAP2_ROLL_LEFT		0x00000004
#define CAP2_ROLL_RIGHT		0x00000008
#define CAP2_WIN_UP			0x00000010
#define CAP2_WIN_DOWN		0x00000020
#define CAP2_WIN_LEFT		0x00000040
#define CAP2_WIN_RIGHT		0x00000080
#define CAP2_WIN_UP_ATTR		0x00000100
#define CAP2_WIN_DOWN_ATTR	0x00000200
#define CAP2_WIN_LEFT_ATTR	0x00000400
#define CAP2_WIN_RIGHT_ATTR	0x00000800
#define CAP2_SPECIAL_DISPLAY	0x00001000

#define INIT_TERMDEF		0x00000001
#define INIT_SETUPTERM		0x00000002
#define INIT_MEMORY			0x00000004
#define INIT_SIGNAL			0x00000008
#define INIT_INPUT			0x00000010
#define INIT_OUTPUT			0x00000020
#define INIT_TTYSAME		0x00000040
#define INIT_IOCTL			0x00000080
#define INIT_UNIX_DEVICE	0x00000100
#define INIT_STRING			0x00000200

#define INVALID_POSITION 0x20000000

struct DSPTAB {
	INT entries;
	UCHAR **tabptr;
};

static CHAR *XTERM_MOUSE_INIT = "\033[?1000h";
static CHAR *XTERM_MOUSE_RESET = "\033[?1000l";
static struct DSPTAB dsptab[DSP_MAX];
static INT dspmax[DSP_MAX];
static INT keymax[KBD_MAX];
static UCHAR **keytabptr;

static INT (*vidcharcallbackfnc)(INT);
static INT breakeventid;

/**
 * Original usage in an INT;
 * 		High order byte used for:DSPATTR_DISPDOWN, DSPATTR_DISPLEFT, DSPATTR_ROLL,
 *				DSPATTR_RAWMODE, DSPATTR_AUXPORT, DSPATTR_KYDSPIPE
 *		Next byte:DSPATTR_UNDERLINE, DSPATTR_BLINK, DSPATTR_BOLD, DSPATTR_DIM,
 * 				DSPATTR_REV, DSPATTR_GRAPHIC
 * 	    Next byte used for 4 bits of background color then 4-bits of foreground color
 * 	    Low byte is the acsii CHAR
 *
 * 'OLD' Color usage with UINT64;
 * 		Low order four bytes the same as above.
 * 		High order four bytes unused.
 *
 * 256 Color usage in a UINT64;
 * 		Numbering bytes using zero for the high order and 7 for the lowest.
 * 		Byte 0 and 1 - Not used
 * 		Byte 2 - Background color
 * 		Byte 3 - Foreground Color
 * 		Byte 4 - DSPATTR_DISPDOWN, DSPATTR_DISPLEFT, DSPATTR_ROLL,
 *					DSPATTR_RAWMODE, DSPATTR_AUXPORT, DSPATTR_KYDSPIPE
 *		Byte 5 - DSPATTR_UNDERLINE, DSPATTR_BLINK, DSPATTR_BOLD, DSPATTR_DIM,
 * 					DSPATTR_REV, DSPATTR_GRAPHIC
 * 		Byte 6 - Was the two four bit colors, unused for this new mode
 * 	    Byte 7 - is the acsii CHAR
 */
static UINT64 **screenbufferhandle;

static INT textwidth, textlength;
static INT interchartime;
static INT maxbot, maxrgt;
static INT cursortype;
static INT cursorh, cursorv;
static UINT64 lastattr;
static UINT64 lastinvalid;
static INT lastcursv, lastcursh;
static INT lasttop, lastbot, lastlft, lastrgt;
static INT lastdisable;
static UINT caps1, caps2, inits;
static INT fgcolormask, bgcolormask;

static struct sigaction oldint;
#ifdef SIGTSTP
static struct sigaction oldtstp;
#endif

static INT ttyin, ttyinflg;
static INT ttyout, ttyoutflg;
#if defined(USE_POSIX_TERMINAL_IO)
static struct termios terminold, terminnew;
static struct termios termoutold, termoutnew;
#else
static struct termio terminold, terminnew;
static struct termio termoutold, termoutnew;
#endif

static INT alrmhndl;
static INT multichr[256];
static INT multimax;
static INT xterm_mouse_flg;

static INT outcnt;
static UCHAR outbuf[OUTBUFSIZE];

static INT keyinsert(UCHAR *, INT, INT, UCHAR **, INT *, INT *, INT);
static void dsptabinit(struct DSPTAB *, INT, INT, INT);
static void cursorpos(void);
static void textput(INT);
static void attrput(UINT64);
static void scrollregion(INT, INT, INT, INT);
static INT termput(INT, INT);
static INT termparmput(INT, INT, INT, INT, INT, INT, INT, INT);
static INT charput(INT);
static INT readcallback(void *, INT);
static INT readtty(INT);
static void sigevent(INT);
static INT colortoansi[] = { 0, 4, 2, 6, 1, 5, 3, 7 };
static INT vidflags;

INT vidastart(INT flag, INT eventid, CHAR *termdef, CHAR *indevice, CHAR *outdevice,
	CHAR *fontname, INT *hsize, INT *vsize, INT (*vidcharfnc)(INT), INT ict)
{
	static CHAR *vt100graphics = "-+,.qxnlkmjtwuv";
	static CHAR *defaultgraphics = "^><v-|+++++++++";
	static CHAR *XTERM_MOUSE_BUTTON = "\033[M";
	INT rc;
	INT i1, i2, i3, code, dspcnt, handle, maxsize, keyval, nlines;
	INT sectionid, sectlen, size, tablesize, tputsflg, *table;
	UINT64 *screenbuf;
	INT ascii_control_flg, color_erase_flg, no_roll_flg;
	INT scroll_erase_flg, scroll_repos_flg;
	LONG filepos;
	CHAR c1, *ptr;
	UCHAR work[256], *buf, *tmpbuf, **tmpbufptr;
	FILE *fileptr; // @suppress("Symbol is not resolved")
	struct sigaction act;

	dspmax[0] = DSP_INITMAX;
	dspmax[1] = DSP_TERMMAX;
	dspmax[2] = DSP_SCREENMAX;
	dspmax[3] = DSP_EDITMAX;
	dspmax[4] = DSP_WINMAX;
	dspmax[5] = DSP_FG_COLORMAX;
	dspmax[6] = DSP_BG_COLORMAX;
	dspmax[7] = DSP_GRAPHMAX;
	dspmax[8] = DSP_SDSPMAX;

	keymax[0] = KBD_FNCMAX;
	keymax[1] = KBD_SHIFTFNCMAX;
	keymax[2] = KBD_CTRLFNCMAX;
	keymax[3] = KBD_ALTFNCMAX;
	keymax[4] = KBD_ALTCHRMAX;
	keymax[5] = KBD_MISCMAX;
	keymax[6] = KBD_SKEYMAX;
	keymax[7] = KBD_SXKEYMAX;

	if (flag & VID_FLAG_COLORMODE_ANSI256) {
		colortoansi[0] = 0;
		colortoansi[1] = 1;
		colortoansi[2] = 2;
		colortoansi[3] = 3;
		colortoansi[4] = 4;
		colortoansi[5] = 5;
		colortoansi[6] = 6;
		colortoansi[7] = 7;
	}

	vidflags = flag;
/*** CODE: MAY WANT TO CHANGE VIDCHARFNC TO RETURN INT, TO DETECT OVERFLOW ***/
	vidcharcallbackfnc = vidcharfnc;
	breakeventid = eventid;
	if (1 <= ict && ict <= 9) interchartime = ict;
	else interchartime = 5;

	caps1 = caps2 = inits = 0;
	ascii_control_flg = color_erase_flg = no_roll_flg = FALSE;
	scroll_erase_flg = scroll_repos_flg = xterm_mouse_flg = FALSE;
	textwidth = textlength = 0;
	if (termdef == NULL || !termdef[0]) {  /* use terminfo */
		/* Initialize terminal definition */
		setupterm((CHAR *) 0, 1, (INT *) 0);
		inits |= INIT_SETUPTERM | INIT_MEMORY;

		/* capabilities section */
#ifdef back_color_erase
		if (back_color_erase) color_erase_flg = TRUE;
#endif
		if (lines > 0) textlength = lines;
		if (columns > 0) textwidth = columns;

		dsptab[DSP_INIT].entries = DSP_INITMAX;
		dsptab[DSP_TERM].entries = DSP_TERMMAX;
		dsptab[DSP_SCREEN].entries = DSP_SCREENMAX;
		dsptab[DSP_EDIT].entries = DSP_EDITMAX;
		dsptab[DSP_WIN].entries = DSP_WINMAX;
		dsptab[DSP_FG_COLOR].entries = 0;
		dsptab[DSP_BG_COLOR].entries = 0;
#ifdef max_colors
		if (max_colors > 0) {
			dsptab[DSP_FG_COLOR].entries = (max_colors < DSP_FG_COLORMAX) ? max_colors : DSP_FG_COLORMAX;
#ifdef max_pairs
			//if (max_colors * max_colors == max_pairs)
			//	dsptab[DSP_BG_COLOR].entries = (max_colors < DSP_BG_COLORMAX) ? max_colors : DSP_BG_COLORMAX;
			dsptab[DSP_BG_COLOR].entries = DSP_BG_COLORMAX;
#endif
		}
#endif
		dsptab[DSP_GRAPH].entries = DSP_GRAPHMAX;
		dsptab[DSP_SDSP].entries = 0;

		for (dspcnt = 0; dspcnt < DSP_MAX; dspcnt++) {
			if (!dsptab[dspcnt].entries) continue;
			tablesize = dsptab[dspcnt].entries * sizeof(INT);
			maxsize = dsptab[dspcnt].entries << 4;
			if (maxsize < 1024) maxsize = 1024;
			dsptab[dspcnt].tabptr = memalloc(tablesize + maxsize, 0);
			if (dsptab[dspcnt].tabptr == NULL) {
				vidaexit();
				vidputerror("memory allocation failed", VIDPUTERROR_NEW);
				return RC_ERROR;
			}

			table = (INT *) *dsptab[dspcnt].tabptr;
			buf = *dsptab[dspcnt].tabptr + tablesize;
			size = 0;
			for (i1 = 0; i1 < dsptab[dspcnt].entries; i1++) {
				ptr = NULL;
				tputsflg = TRUE;
				outcnt = 0;
				switch (dspcnt) {
					case DSP_INIT:  /* terminal initialization */
						nlines = 25;
						switch (i1) {
							case DSP_INITIALIZE:
								ptr = enter_ca_mode;
								if (keypad_xmit != NULL && keypad_xmit[0]) {
									if (ptr != NULL && ptr[0]) tputs(ptr, nlines, charput);
									ptr = keypad_xmit;
								}
								break;
						}
						break;
					case DSP_TERM:  /* terminal termination */
						nlines = 25;
						switch (i1) {
							case DSP_TERMINATE:
								ptr = keypad_local;
								if (exit_ca_mode != NULL && exit_ca_mode[0]) {
									if (ptr != NULL && ptr[0]) tputs(ptr, nlines, charput);
									ptr = exit_ca_mode;
								}
								break;
						}
						break;
					case DSP_SCREEN:  /* basic screen control section */
						nlines = 1;
						switch (i1) {
							case DSP_POSITION_CURSOR:
								ptr = cursor_address;
								tputsflg = FALSE;
								break;
							case DSP_POSITION_CURSOR_HORZ:
								ptr = column_address;
								tputsflg = FALSE;
								break;
							case DSP_POSITION_CURSOR_VERT:
								ptr = row_address;
								tputsflg = FALSE;
								break;
							case DSP_CLEAR_SCREEN:
								nlines = 25;
								ptr = clear_screen;
								break;
							case DSP_CLEAR_TO_SCREEN_END:
								nlines = 25;
								ptr = clr_eos;
								break;
							case DSP_CLEAR_TO_LINE_END:
								ptr = clr_eol;
								break;
							case DSP_CURSOR_ON:
								break;
							case DSP_CURSOR_OFF:
								ptr = cursor_invisible;
								break;
							case DSP_CURSOR_BLOCK:
								ptr = cursor_visible;
								break;
							case DSP_CURSOR_UNDERLINE:
								ptr = cursor_normal;
								break;
							case DSP_REVERSE_ON:
								ptr = enter_standout_mode;
								break;
							case DSP_REVERSE_OFF:
								ptr = exit_standout_mode;
								break;
							case DSP_UNDERLINE_ON:
								ptr = enter_underline_mode;
								break;
							case DSP_UNDERLINE_OFF:
								ptr = exit_underline_mode;
								break;
							case DSP_BLINK_ON:
								ptr = enter_blink_mode;
								break;
							case DSP_BLINK_OFF:
								break;
							case DSP_BOLD_ON:
								ptr = enter_bold_mode;
								break;
							case DSP_BOLD_OFF:
								break;
							case DSP_DIM_ON:
								ptr = enter_dim_mode;
								break;
							case DSP_DIM_OFF:
							case DSP_RIGHT_TO_LEFT_DSP_ON:
							case DSP_RIGHT_TO_LEFT_DSP_OFF:
							case DSP_DISPLAY_DOWN_ON:
							case DSP_DISPLAY_DOWN_OFF:
								break;
							case DSP_ALL_OFF:
								ptr = exit_attribute_mode;
								break;
							case DSP_BELL:
								ptr = bell;
								break;
							case DSP_AUXPORT_ON:
								ptr = prtr_on;
								break;
							case DSP_AUXPORT_OFF:
								ptr = prtr_off;
								break;
							case DSP_CURSOR_HALF:
								break;
						}
						break;
					case DSP_EDIT:  /* editing screen control section */
						nlines = 25;
						switch (i1) {
							case DSP_INSERT_CHAR:
								nlines = 1;
								ptr = insert_character;
								break;
							case DSP_DELETE_CHAR:
								nlines = 1;
								ptr = delete_character;
								break;
							case DSP_INSERT_LINE:
								ptr = insert_line;
								break;
							case DSP_DELETE_LINE:
								ptr = delete_line;
								break;
							case DSP_OPEN_LINE:
							case DSP_CLOSE_LINE:
								break;
						}
						break;
					case DSP_WIN:  /* window screen control section */
						nlines = 25;
						switch (i1) {
							case DSP_SCROLL_REGION_TB:
								ptr = change_scroll_region;
								tputsflg = FALSE;
								break;
							case DSP_SCROLL_REGION_LR:
								break;
							case DSP_SCROLL_UP:
								ptr = scroll_forward;
								break;
							case DSP_SCROLL_DOWN:
								ptr = scroll_reverse;
								break;
							case DSP_SCROLL_LEFT:
							case DSP_SCROLL_RIGHT:
							case DSP_SCROLL_WIN_UP:
							case DSP_SCROLL_WIN_DOWN:
							case DSP_SCROLL_WIN_LEFT:
							case DSP_SCROLL_WIN_RIGHT:
							case DSP_ERASE_WIN:
							case DSP_ENABLE_ROLL:
							case DSP_DISABLE_ROLL:
								break;
						}
						break;
					case DSP_FG_COLOR:
#ifdef set_foreground
						if (set_foreground != NULL && *set_foreground) {
							/*
							 * Set foreground color
							 */
							ptr = tparm((CHAR *) set_foreground, i1, 0, 0, 0, 0, 0, 0, 0, 0);
							tputs(ptr, 1, charput);  /* terminfo */
							if (outcnt) {
								nlines = outcnt;
								ptr = (CHAR *) outbuf;
								tputsflg = FALSE;
							}
						}
#ifdef set_a_foreground
						else {
#endif
#endif
#ifdef set_a_foreground
							/*
							 * Set foreground color using ANSI escape
							 */
							if (set_a_foreground != NULL && *set_a_foreground) {
								if (i1 < (INT)ARRAYSIZE(colortoansi)) {
									ptr = tparm((CHAR *) set_a_foreground, colortoansi[i1], 0, 0, 0, 0, 0, 0, 0, 0);
									tputs(ptr, 1, charput);  /* terminfo */
									if (outcnt) {
										nlines = outcnt;
										ptr = (CHAR *) outbuf;
										tputsflg = FALSE;
									}
								}
								else if (flag & VID_FLAG_COLORMODE_ANSI256){
									ptr = tparm((CHAR *) set_a_foreground, i1,
											0, 0, 0, 0, 0, 0, 0, 0);
									tputs(ptr, 1, charput);  /* terminfo */
									if (outcnt) {
										nlines = outcnt;
										ptr = (CHAR *) outbuf;
										tputsflg = FALSE;
									}
								}
							}
#ifdef set_foreground
						}
#endif
#endif
						break;
					case DSP_BG_COLOR:
#ifdef set_background
						if (set_background != NULL && *set_background) {
							/*
							 * Set background color
							 */
							ptr = tparm((CHAR *) set_background, i1, 0, 0, 0, 0, 0, 0, 0, 0);
							tputs(ptr, 1, charput);  /* terminfo */
							if (outcnt) {
								nlines = outcnt;
								ptr = (CHAR *) outbuf;
								tputsflg = FALSE;
							}
						}
#ifdef set_a_background
						else {
#endif
#endif
#ifdef set_a_background
							if (set_a_background != NULL && *set_a_background) {
								if (i1 < (INT)ARRAYSIZE(colortoansi)) {
									ptr = tparm((CHAR *) set_a_background,
											colortoansi[i1], 0, 0, 0, 0, 0, 0, 0, 0);
									tputs(ptr, 1, charput);  /* terminfo */
									if (outcnt) {
										nlines = outcnt;
										ptr = (CHAR *) outbuf;
										tputsflg = FALSE;
									}
								}
								else if (flag & VID_FLAG_COLORMODE_ANSI256){
									ptr = tparm((CHAR *) set_a_background, i1,
											0, 0, 0, 0, 0, 0, 0, 0);
									tputs(ptr, 1, charput);  /* terminfo */
									if (outcnt) {
										nlines = outcnt;
										ptr = (CHAR *) outbuf;
										tputsflg = FALSE;
									}
								}
							}
#ifdef set_background
						}
#endif
#endif
						break;
					case DSP_GRAPH:  /* graphic character display sequences */
						nlines = 1;
#ifdef acs_chars
						if (acs_chars != NULL && *acs_chars) {
							if (i1 == DSP_GRAPHIC_ON) ptr = enter_alt_charset_mode;
							else if (i1 == DSP_GRAPHIC_OFF) ptr = exit_alt_charset_mode;
							else if (i1 >= DSP_GRAPHIC_UP_ARROW && i1 <= DSP_GRAPHIC_UP_TICK) {
								c1 = vt100graphics[i1 - DSP_GRAPHIC_UP_ARROW];
								ptr = acs_chars;
								while (*ptr && *ptr != c1) ptr += 2;
								if (*ptr) {
									work[0] = *(ptr + 1);
									work[1] = 0;
									ptr = (CHAR *) work;
								}
								else ptr = NULL;
							}
						}
						else
#endif
						if (i1 >= DSP_GRAPHIC_UP_ARROW && i1 <= DSP_GRAPHIC_UP_TICK) {
							work[0] = defaultgraphics[i1 - DSP_GRAPHIC_UP_ARROW];
							work[1] = 0;
							ptr = (CHAR *) work;
						}
						break;
					case DSP_SDSP:
						break;
				}
				if (ptr == NULL || !*ptr) {
					table[i1] = -1;
					continue;
				}
				if (tputsflg) {
					tputs(ptr, nlines, charput);
					nlines = outcnt;
					ptr = (CHAR *) outbuf;
				}
				else if (!outcnt) outcnt = strlen(ptr) + 1;
				if (size + outcnt + 1 > maxsize) {
					i2 = size + outcnt + 1 - maxsize + ((dsptab[dspcnt].entries - i1) << 5);
					if (i2 < 1024) i2 = 1024;
					if (memchange(dsptab[dspcnt].tabptr, tablesize + maxsize + i2, 0)) {
						vidaexit();
						vidputerror("memory allocation failed", VIDPUTERROR_NEW);
						return RC_ERROR;
					}
					table = (INT *) *dsptab[dspcnt].tabptr;
					buf = *dsptab[dspcnt].tabptr + tablesize;
					maxsize += i2;
				}
				table[i1] = size;
				buf[size++] = (UCHAR) nlines;
				memcpy(&buf[size], ptr, outcnt);
				size += outcnt;
			}
			if (memchange(dsptab[dspcnt].tabptr, tablesize + size, 0)) {
				vidaexit();
				vidputerror("memory allocation failed", VIDPUTERROR_NEW);
				return RC_ERROR;
			}
		}
		outcnt = 0;

		maxsize = 1024;
		keytabptr = memalloc(maxsize, 0);
		if (keytabptr == NULL) {
			vidaexit();
			vidputerror("memory allocation failed", VIDPUTERROR_NEW);
			return RC_ERROR;
		}

		size = 0;
#ifdef NOF11_F20
		if (prpget("keyin", "f11", NULL, NULL, &ptr, PRP_LOWER) || strcmp(ptr, "on")) i3 = VID_F10;
		else
#endif
		i3 = VID_F20;
		for (i1 = VID_ENTER; i1 <= i3; i1++) {
			ptr = NULL;
			switch (i1) {
				case VID_ENTER: ptr = "\r"; break;
				case VID_ESCAPE:
					work[0] = 0x1b; work[1] = 0;
					ptr = (CHAR *) work;
					break;
				case VID_BKSPC:
#ifdef key_backspace
					if (key_backspace != NULL) ptr = key_backspace;
					else
#endif
					ptr = "\b";
					break;
				case VID_TAB:
					ptr = "\t";
					break;
				case VID_BKTAB:
#ifdef key_btab
					if (key_btab != NULL) ptr = key_btab;
#endif
					break;
				case VID_UP: ptr = key_up; break;
				case VID_DOWN: ptr = key_down; break;
				case VID_LEFT: ptr = key_left; break;
				case VID_RIGHT: ptr = key_right; break;
				case VID_INSERT: ptr = key_ic; break;
				case VID_DELETE: ptr = key_dc; break;
				case VID_HOME: ptr = key_home; break;
				case VID_END:
#ifdef key_end
					ptr = key_end;
#endif
					break;
				case VID_PGUP: ptr = key_ppage; break;
				case VID_PGDN: ptr = key_npage; break;
				case VID_F1: ptr = key_f1; break;
				case VID_F2: ptr = key_f2; break;
				case VID_F3: ptr = key_f3; break;
				case VID_F4: ptr = key_f4; break;
				case VID_F5: ptr = key_f5; break;
				case VID_F6: ptr = key_f6; break;
				case VID_F7: ptr = key_f7; break;
				case VID_F8: ptr = key_f8; break;
				case VID_F9: ptr = key_f9; break;
				case VID_F10: ptr = key_f10; break;
#ifdef key_f11
				case VID_F11: ptr = key_f11; break;
				case VID_F12: ptr = key_f12; break;
				case VID_F13: ptr = key_f13; break;
				case VID_F14: ptr = key_f14; break;
				case VID_F15: ptr = key_f15; break;
				case VID_F16: ptr = key_f16; break;
				case VID_F17: ptr = key_f17; break;
				case VID_F18: ptr = key_f18; break;
				case VID_F19: ptr = key_f19; break;
				case VID_F20: ptr = key_f20; break;
#endif
			}
			if (ptr == NULL || !*ptr) continue;
			i2 = strlen(ptr);
			if (keyinsert((UCHAR *) ptr, i2, i1, keytabptr, &size, &maxsize, FALSE)) {
				vidaexit();
				return RC_ERROR;
			}
		}
		if (keyinsert((UCHAR *) "\n", 1, VID_ENTER, keytabptr, &size, &maxsize, FALSE)) {
			vidaexit();
			return RC_ERROR;
		}
	}


	else {  /* termdef */
		inits |= INIT_TERMDEF | INIT_MEMORY;
		strcpy((CHAR *) work, termdef);
		miofixname((CHAR *) work, ".tdb", FIXNAME_EXT_ADD);
		handle = fioopen((CHAR *) work, FIO_M_SRO | FIO_P_TDB);
		if (handle < 0) {
			vidaexit();
			vidputerror("unable to open ", VIDPUTERROR_NEW);
			vidputerror((CHAR *) work, VIDPUTERROR_APPEND);
			vidputerror(", ", VIDPUTERROR_APPEND);
			vidputerror(fioerrstr(handle), VIDPUTERROR_APPEND);
			return RC_ERROR;
		}
		size = maxsize = 0;
		for (filepos = 0L; (rc = fioread(handle, filepos, work, 4)) >= 4; filepos += sectlen) {
			filepos += 4;
			sectionid = llhh(&work[2]);
			if (sectionid == 0) {
				rc = 0;
				break;
			}
			sectlen = llhh(work) - 4;
			if (sectlen == 0) continue;
			if (sectionid == 1) continue;  /* header */
			if (sectionid == 2) {  /* capabilities section */
				rc = fioread(handle, filepos, work, sectlen);
				if (rc < sectlen) {
					if (!rc) rc = 1;
					break;
				}
				if (sectlen >= 2) textlength = llhh(work);
				if (sectlen >= 4) textwidth = llhh(&work[2]);
				if (sectlen >= 6 && flag & VID_FLAG_USEMOUSE) xterm_mouse_flg = llhh(&work[4]);
				if (sectlen >= 8) scroll_repos_flg = llhh(&work[6]);
				if (sectlen >= 10) color_erase_flg = llhh(&work[8]);
				if (sectlen >= 12) no_roll_flg = llhh(&work[10]);
				if (sectlen >= 14) scroll_erase_flg = llhh(&work[12]);
				if (sectlen >= 16) ascii_control_flg = llhh(&work[14]);
				continue;
			}
			if (sectionid < 200) {  /* initialize, terminate & display section */
				if (sectionid >= 3 && sectionid <= 4) {
					// DSP_INIT and DSP_TERM
					code = sectionid - 3;
				}
				else if (sectionid >= 101 && sectionid <= 106) {
					// DSP_SCREEN, DSP_EDIT, DSP_WIN, DSP_FG_COLOR, DSP_BG_COLOR, DSP_GRAPH
					code = sectionid - 99;
				}
				else if (sectionid == 120) code = DSP_SDSP;
				else continue;
				dsptab[code].tabptr = memalloc(dspmax[code] * sizeof(INT) + sectlen, 0);
				if (dsptab[code].tabptr == NULL) {
					rc = ERR_NOMEM;
					break;
				}
				rc = fioread(handle, filepos, *dsptab[code].tabptr + dspmax[code] * sizeof(INT), sectlen);
				if (rc < sectlen) {
					if (!rc) rc = 1;
					break;
				}
				dsptabinit(&dsptab[code], dspmax[code], sectlen, code == DSP_SDSP);
				continue;
			}
			if (sectionid < 300) {  /* pre-version 9 keyin section */
				if (!maxsize) {
					maxsize = 2048;
					keytabptr = memalloc(maxsize, 0);
					if (keytabptr == NULL) {
						rc = ERR_NOMEM;
						break;
					}
				}
				if (sectionid >= 201 && sectionid <= 206) code = sectionid - 201;
				else if (sectionid >= 220 && sectionid <= 221) code = sectionid - 214;
				else continue;

				tmpbufptr = memalloc(sectlen, 0);
				if (tmpbufptr == NULL) {
					rc = ERR_NOMEM;
					break;
				}
				tmpbuf = *tmpbufptr;
				rc = fioread(handle, filepos, tmpbuf, sectlen);
				if (rc < sectlen) {
					memfree(tmpbufptr);
					if (!rc) rc = 1;
					break;
				}

				for (i1 = i2 = 0; i1 < keymax[code] && i2 < sectlen; i1++, i2 += tmpbuf[i2] + 1) {
					keyval = -1;
					switch (code) {
						case KBD_FNC:
							if (i1 < 10) keyval = VID_UP + i1;
							else keyval = VID_F1 + i1 - 10;
							break;
						case KBD_SHIFTFNC:
							if (i1 < 10) keyval = VID_SHIFTUP + i1;
							else keyval = VID_SHIFTF1 + i1 - 10;
							break;
						case KBD_CTRLFNC:
							if (i1 < 10) keyval = VID_CTLUP + i1;
							else keyval = VID_CTLF1 + i1 - 10;
							break;
						case KBD_ALTFNC:
							if (i1 < 10) keyval = VID_ALTUP + i1;
							else keyval = VID_ALTF1 + i1 - 10;
							break;
						case KBD_ALTCHR:
							keyval = VID_ALTA + i1;
							break;
						case KBD_MISC:
							switch (i1) {
								case 0: keyval = VID_ENTER; break;
								case 1: keyval = VID_BKSPC; break;
								case 2: keyval = VID_ESCAPE; break;
								case 3: keyval = VID_TAB; break;
								case 4: keyval = VID_BKTAB; break;
							}
							break;
						case KBD_SKEY:
							if (i2 + 1 < sectlen) keyval = tmpbuf[i2++];
							break;
						case KBD_SXKEY:
							if (i2 + 1 < sectlen) keyval = tmpbuf[i2++] + 256;
							break;
					}
					if (keyval == -1) break;
					if (!tmpbuf[i2]) continue;
					if (keyinsert((UCHAR *) &tmpbuf[i2 + 1], (INT) tmpbuf[i2], keyval, keytabptr, &size, &maxsize, code == KBD_MISC)) {
						memfree(tmpbufptr);
						fioclose(handle);
						vidaexit();
						return RC_ERROR;
					}
				}
				memfree(tmpbufptr);
				continue;
			}
			if (sectionid < 400) {  /* version 9 keyin section */
				if (sectionid != 301) continue;
				if (maxsize) {
					fioclose(handle);
					vidaexit();
					vidputerror("invalid termdef file (2 keyin sections)", VIDPUTERROR_NEW);
					return RC_ERROR;
				}
				maxsize = sectlen + 1;
				keytabptr = memalloc(maxsize, 0);
				if (keytabptr == NULL) {
					rc = ERR_NOMEM;
					break;
				}
				rc = fioread(handle, filepos, *keytabptr, sectlen);
				if (rc < sectlen) {
					if (!rc) rc = 1;
					break;
				}
				size = sectlen;
				(*keytabptr)[size] = 0;
			}
		}

		fioclose(handle);
		if (rc) {
			vidaexit();
			if (rc > 0) vidputerror("invalid termdef file", VIDPUTERROR_NEW);
			else if (rc == ERR_NOMEM) vidputerror("memory allocation failed", VIDPUTERROR_NEW);
			else {
				vidputerror("unable to read termdef file", VIDPUTERROR_NEW);
				vidputerror(", ", VIDPUTERROR_APPEND);
				vidputerror(fioerrstr(handle), VIDPUTERROR_APPEND);
			}
			return RC_ERROR;
		}

		if (xterm_mouse_flg) {
			keyinsert((UCHAR *) XTERM_MOUSE_BUTTON, (INT) strlen(XTERM_MOUSE_BUTTON), 1000, keytabptr, &size, &maxsize, 0);
		}

	}
	if (keytabptr == NULL) keytabptr = memalloc(2048, 0);
	memchange(keytabptr, size + 1, 0);
	buf = *keytabptr;
	multimax = 0;
	for (i1 = 0; i1 < 256; ) multichr[i1++] = -1;
	for (i1 = 0; buf[i1]; i1 += buf[i1] + 3) {
		if ((INT) buf[i1] > multimax) multimax = buf[i1];
		if (multichr[buf[i1 + 1]] == -1) multichr[buf[i1 + 1]] = i1;
	}
	if (multimax < 2) multimax = 2;  /* this will force readtty to be called back if buffer fills */

	if (!textwidth) textwidth = 80;
	if (!textlength) textlength = 25;
	if (!*hsize) *hsize = textwidth;
	if (!*vsize) *vsize = textlength;
	textwidth = *hsize;
	textlength = *vsize;
	screenbufferhandle = (UINT64 **) memalloc(textwidth * textlength * sizeof(UINT64), 0);
	if (screenbufferhandle == NULL) {
		vidaexit();
		vidputerror("memory allocation failed", VIDPUTERROR_NEW);
		return RC_ERROR;
	}
	screenbuf = *screenbufferhandle;
	for (i1 = 0, i2 = textwidth * textlength; i1 < i2; )
		screenbuf[i1++] = (((UINT64)DSPATTR_WHITE) << DSPATTR_FGCOLORSHIFT)
		| (((UINT64)DSPATTR_BLACK) << DSPATTR_BGCOLORSHIFT) | ' ';

	if (ascii_control_flg) caps1 |= CAP1_ASCII_CONTROL;
	if (color_erase_flg) caps1 |= CAP1_COLOR_ERASE;
	if (no_roll_flg) caps1 |= CAP1_NO_ROLL;
	if (scroll_erase_flg) caps1 |= CAP1_SCROLL_ERASE;
	if (scroll_repos_flg) caps1 |= CAP1_SCROLL_REPOS;
	if (DSP_POSITION_CURSOR < dsptab[DSP_SCREEN].entries && ((INT *) *dsptab[DSP_SCREEN].tabptr)[DSP_POSITION_CURSOR] != -1){
		caps1 |= CAP1_POS;
	}
	if (DSP_POSITION_CURSOR_HORZ < dsptab[DSP_SCREEN].entries && ((INT *) *dsptab[DSP_SCREEN].tabptr)[DSP_POSITION_CURSOR_HORZ] != -1) caps1 |= CAP1_POS_HORZ;
	if (DSP_POSITION_CURSOR_VERT < dsptab[DSP_SCREEN].entries && ((INT *) *dsptab[DSP_SCREEN].tabptr)[DSP_POSITION_CURSOR_VERT] != -1) caps1 |= CAP1_POS_VERT;
	if (DSP_CLEAR_SCREEN < dsptab[DSP_SCREEN].entries && ((INT *) *dsptab[DSP_SCREEN].tabptr)[DSP_CLEAR_SCREEN] != -1) caps1 |= CAP1_ES;
	if (DSP_CLEAR_TO_SCREEN_END < dsptab[DSP_SCREEN].entries && ((INT *) *dsptab[DSP_SCREEN].tabptr)[DSP_CLEAR_TO_SCREEN_END] != -1) caps1 |= CAP1_EF;
	if (DSP_CLEAR_TO_LINE_END < dsptab[DSP_SCREEN].entries && ((INT *) *dsptab[DSP_SCREEN].tabptr)[DSP_CLEAR_TO_LINE_END] != -1) caps1 |= CAP1_EL;
	if (DSP_CURSOR_UNDERLINE < dsptab[DSP_SCREEN].entries && ((INT *) *dsptab[DSP_SCREEN].tabptr)[DSP_CURSOR_UNDERLINE] != -1) caps1 |= CAP1_CURSOR_UNDERLINE;
	if (DSP_CURSOR_BLOCK < dsptab[DSP_SCREEN].entries && ((INT *) *dsptab[DSP_SCREEN].tabptr)[DSP_CURSOR_BLOCK] != -1) caps1 |= CAP1_CURSOR_BLOCK;


	if (DSP_CURSOR_HALF < dsptab[DSP_SCREEN].entries && ((INT *) *dsptab[DSP_SCREEN].tabptr)[DSP_CURSOR_HALF] != -1) {
		caps1 |= CAP1_CURSOR_HALF;
	}
	if (DSP_FGCOLOR_ANSI256 < dsptab[DSP_SCREEN].entries && ((INT *) *dsptab[DSP_SCREEN].tabptr)[DSP_FGCOLOR_ANSI256] != -1) {
		caps1 |= CAP1_FGCOLOR_ANSI256;
	}
	if (DSP_BGCOLOR_ANSI256 < dsptab[DSP_SCREEN].entries && ((INT *) *dsptab[DSP_SCREEN].tabptr)[DSP_BGCOLOR_ANSI256] != -1) {
		caps1 |= CAP1_BGCOLOR_ANSI256;
	}

	if (DSP_SCROLL_REGION_TB < dsptab[DSP_WIN].entries && ((INT *) *dsptab[DSP_WIN].tabptr)[DSP_SCROLL_REGION_TB] != -1)
		caps1 |= CAP1_SET_TB;
	if (DSP_SCROLL_REGION_LR < dsptab[DSP_WIN].entries && ((INT *) *dsptab[DSP_WIN].tabptr)[DSP_SCROLL_REGION_LR] != -1)
		caps1 |= CAP1_SET_LR;
	if (DSP_DISABLE_ROLL < dsptab[DSP_WIN].entries && ((INT *) *dsptab[DSP_WIN].tabptr)[DSP_ENABLE_ROLL] != -1 && ((INT *) *dsptab[DSP_WIN].tabptr)[DSP_DISABLE_ROLL] != -1) caps1 |= CAP1_DISABLE_ENABLE;
	for (i1 = 0; i1 < 4; i1++)
		if (DSP_INSERT_CHAR + i1 < dsptab[DSP_EDIT].entries && ((INT *) *dsptab[DSP_EDIT].tabptr)[DSP_INSERT_CHAR + i1] != -1)
			caps1 |= CAP1_INSERT_CHAR << i1;
	for (i1 = 0; i1 < 4; i1++)
		if (DSP_SCROLL_UP + i1 < dsptab[DSP_WIN].entries && ((INT *) *dsptab[DSP_WIN].tabptr)[DSP_SCROLL_UP + i1] != -1)
			caps2 |= CAP2_ROLL_UP << i1;
	if ((inits & INIT_TERMDEF) && dsptab[DSP_WIN].entries) {
		buf = *dsptab[DSP_WIN].tabptr + dsptab[DSP_WIN].entries * sizeof(INT);
		if (DSP_ERASE_WIN < dsptab[DSP_WIN].entries && ((INT *) *dsptab[DSP_WIN].tabptr)[DSP_ERASE_WIN] != -1) {
			caps1 |= CAP1_ERASE_WIN;
			i1 = ((INT *) *dsptab[DSP_WIN].tabptr)[DSP_ERASE_WIN];
			i2 = buf[i1++] - 1;
			while (i2-- > 0) {
				if (!buf[i1++]) {
					if ((buf[i1++] & 0x87) == 0x06) {
						caps1 |= CAP1_ERASE_WIN_ATTR;
						break;
					}
				}
			}
		}
		for (i3 = 0; i3 < 4; i3++) {
			if (DSP_SCROLL_WIN_UP + i3 < dsptab[DSP_WIN].entries
					&& ((INT *) *dsptab[DSP_WIN].tabptr)[DSP_SCROLL_WIN_UP + i3] != -1) {
				caps2 |= CAP2_WIN_UP << i3;
				i1 = ((INT *) *dsptab[DSP_WIN].tabptr)[DSP_SCROLL_WIN_UP + i3];
				i2 = buf[i1++] - 1;
				while (i2-- > 0) {
					if (!buf[i1++]) {
						if ((buf[i1++] & 0x87) == 0x06) {
							caps2 |= CAP2_WIN_UP_ATTR << i3;
							break;
						}
					}
				}
			}
		}
	}
	if (dsptab[DSP_SDSP].entries) caps2 |= CAP2_SPECIAL_DISPLAY;

	if (flag & VID_FLAG_COLORMODE_OLD) {
		fgcolormask = bgcolormask = 0;
		for (i1 = 0; i1 < 16; i1++) {
			if (i1 < dsptab[DSP_FG_COLOR].entries && ((INT *) *dsptab[DSP_FG_COLOR].tabptr)[i1] != -1)
				fgcolormask |= 1 << i1;
			if (i1 < dsptab[DSP_BG_COLOR].entries && ((INT *) *dsptab[DSP_BG_COLOR].tabptr)[i1] != -1)
				bgcolormask |= 1 << i1;
		}
	}

	cursortype = CURSOR_ULINE;
	lastattr = (((UINT64)DSPATTR_WHITE) << DSPATTR_FGCOLORSHIFT)
			| (((UINT64)DSPATTR_BLACK) << DSPATTR_BGCOLORSHIFT);
	if (flag & VID_FLAG_KYDSPIPE) lastattr |= DSPATTR_KYDSPIPE;
	if ((caps1 & CAP1_POS) || ((caps1 & CAP1_POS_HORZ) && (caps1 & CAP1_POS_VERT)))
		lastcursv = lastcursh = INVALID_POSITION;
	else lastcursv = lastcursh = 0;
	lasttop = lastlft = 0;
	lastbot = maxbot = textlength - 1;
	lastrgt = maxrgt = textwidth - 1;

	if (breakeventid) act.sa_handler = sigevent;
	else act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
#ifdef SA_INTERRUPT
	act.sa_flags |= SA_INTERRUPT;
#endif
	sigaction(SIGINT, &act, &oldint);
#ifdef SIGTSTP
	act.sa_handler = SIG_IGN;
	sigaction(SIGTSTP, &act, &oldtstp);
	if (oldtstp.sa_handler == SIG_DFL) {
		act.sa_handler = sigevent;
		sigaction(SIGTSTP, &act, NULL);
	}
#endif
	inits |= INIT_SIGNAL;

	if (indevice != NULL && indevice[0] != 0) {  /* redirected keyboard input */
		fileptr = fopen(indevice, "r");
		if (fileptr == NULL) {
			mscitoa(errno, (CHAR *) work);
			vidaexit();
			vidputerror("unable to open ", VIDPUTERROR_NEW);
			vidputerror(indevice, VIDPUTERROR_APPEND);
			vidputerror(", errno = ", VIDPUTERROR_APPEND);
			vidputerror((CHAR *) work, VIDPUTERROR_APPEND);
			return RC_ERROR;
		}
		ttyin = fileno(fileptr);
		inits |= INIT_INPUT;
	}
	else ttyin = STDIN_FILENO;

	if (outdevice != NULL && outdevice[0] != 0) {  /* redirected display output */
		fileptr = fopen(outdevice, "w");
		if (fileptr == NULL) {
			mscitoa(errno, (CHAR *) work);
			vidaexit();
			vidputerror("unable to open ", VIDPUTERROR_NEW);
			vidputerror(outdevice, VIDPUTERROR_APPEND);
			vidputerror(", errno = ", VIDPUTERROR_APPEND);
			vidputerror((CHAR *) work, VIDPUTERROR_APPEND);
			return RC_ERROR;
		}
		ttyout = fileno(fileptr);
		inits |= INIT_OUTPUT;
	}
	else ttyout = STDOUT_FILENO;

	ttyinflg = isatty(ttyin);
	ttyoutflg = isatty(ttyout);
	if (ttyinflg && ttyoutflg) {
		strcpy((CHAR *) work, (CHAR *) ttyname(ttyin));
		if (!strcmp((CHAR *) work, (CHAR *) ttyname(ttyout))) inits |= INIT_TTYSAME;
	}

	if (ttyinflg) {
#if defined(USE_POSIX_TERMINAL_IO)
		tcgetattr(ttyin, &terminold);
#else
		ioctl(ttyin, TCGETA, &terminold);
#endif
		memcpy((CHAR *) &terminnew, (CHAR *) &terminold, sizeof(terminnew));
		if (flag & VID_FLAG_XOFF) terminnew.c_iflag &= ~IXON;
		if (flag & VID_FLAG_XON) terminnew.c_iflag |= IXON;
		terminnew.c_iflag &= ~ICRNL;
		terminnew.c_iflag |= IGNBRK;
		terminnew.c_lflag = ISIG;  /* enable signals, non-canonical, no echo, etc. */
		terminnew.c_cc[VMIN] = 1;
		terminnew.c_cc[VTIME] = 0;
		if (inits & INIT_TTYSAME) terminnew.c_oflag = 0;
#if defined(USE_POSIX_TERMINAL_IO)
		tcsetattr(ttyin, TCSADRAIN, &terminnew);
#else
		ioctl(ttyin, TCSETAW, &terminnew);
#endif
	}
	if (ttyoutflg && !(inits & INIT_TTYSAME)) {
#if defined(USE_POSIX_TERMINAL_IO)
		tcgetattr(ttyin, &termoutold);
#else
		ioctl(ttyout, TCGETA, &termoutold);
#endif
		memcpy((CHAR *) &termoutnew, (CHAR *) &termoutold, sizeof(termoutnew));
		termoutnew.c_oflag = 0;  /* No output translation */
#if defined(USE_POSIX_TERMINAL_IO)
		tcsetattr(ttyin, TCSADRAIN, &termoutnew);
#else
		ioctl(ttyout, TCSETAW, &termoutnew);
#endif
	}
	inits |= INIT_IOCTL;

/*** CODE: HAVE UNIXINITDEV RETURN ERROR VALUES ***/

	if ( ( i1 = evtdevinit(ttyin, DEVPOLL_READ, readcallback, (void *) (ULONG) ttyin) ) ) {
		evtgeterror((CHAR *) work, sizeof(work));
		vidaexit();
		vidputerror((CHAR *) work, VIDPUTERROR_NEW);
		return i1;
	}
	inits |= INIT_UNIX_DEVICE;

	if (!(flag & VID_FLAG_KYDSPIPE)) {
		termput(DSP_INIT, DSP_INITIALIZE);
		if (xterm_mouse_flg) {
			buf = (UCHAR*)XTERM_MOUSE_INIT;
			i1 = strlen((const char *)buf);
			if (outcnt + i1 > (INT) sizeof(outbuf)) vidaflush();
			memcpy(&outbuf[outcnt], buf, i1);
			outcnt += i1;
		}
		inits |= INIT_STRING;
	}
	return 0;
}

static INT keyinsert(UCHAR *key, INT len, INT value, UCHAR **keytabptr, INT *size, INT *maxsize, INT replaceflg)
{
	INT i1, i2, i3, siz;
	UCHAR *buf;

	buf = *keytabptr;
	siz = *size;
	if (siz + len + 4 > *maxsize) {
		i2 = siz + len + 4 - *maxsize;
		if (i2 < 1024) i2 = 1024;
		if (memchange(keytabptr, *maxsize + i2, 0)) {
			vidputerror("memory allocation failed", VIDPUTERROR_NEW);
			return RC_ERROR;
		}
		buf = *keytabptr;
		*maxsize += i2;
	}

	for (i1 = 0; i1 < siz; i1 += buf[i1] + 3) {
		i2 = buf[i1];
		if (i2 > len) i2 = len;
		i3 = memcmp(key, &buf[i1 + 1], i2);
		if (i3 < 0 || (i3 == 0 && len > i2)) {
			memmove(&buf[i1 + len + 3], &buf[i1], siz - i1);
			break;
		}
		if (i3 == 0 && len == buf[i1]) {
			if (replaceflg) {
				buf[i1 + len + 1] = (UCHAR) value;
				buf[i1 + len + 2] = (UCHAR)(value >> 8);
			}
			return 0;
		}
	}
	buf[i1++] = (UCHAR) len;
	memcpy(&buf[i1], key, len);
	i1 += len;
	buf[i1] = (UCHAR) value;
	buf[i1 + 1] = (UCHAR)(value >> 8);
	siz += len + 3;
	buf[siz] = '\0';
	*size = siz;
	return 0;
}

static void dsptabinit(struct DSPTAB *dsp, INT entries, INT len, INT specialflg)
{
	INT i1, i2, *table;
	UCHAR *buf;

	table = (INT *) *dsp->tabptr;
	buf = *dsp->tabptr + entries * sizeof(INT);
	for (i1 = 0; i1 < entries; ) table[i1++] = -1;
	for (i1 = i2 = 0; i1 < entries && i2 < len; i1++, i2 += buf[i2] + 1) {
		if (specialflg) {
			table[buf[i2]] = i2 + 1;
			i2++;
		}
		else if (buf[i2]) table[i1] = i2;
	}
	dsp->entries = entries;
}

void vidaexit()
{
	INT i1;
	UCHAR *buf;

	if (inits & INIT_STRING) {  /* reset video display */
		if (lastattr & DSPATTR_AUXPORT) attrput(lastattr & ~DSPATTR_AUXPORT);
		if (!(lastattr & DSPATTR_KYDSPIPE)) {
			attrput((((UINT64)DSPATTR_WHITE) << DSPATTR_FGCOLORSHIFT)
					| (((UINT64)DSPATTR_BLACK) << DSPATTR_BGCOLORSHIFT));
			if (lastdisable) {
				termput(DSP_WIN, DSP_ENABLE_ROLL);
				lastdisable = 0;
			}
			scrollregion(0, maxbot, 0, maxrgt);
			cursorpos();
			if (cursortype == CURSOR_NONE) vidacursor(CURSOR_ULINE, lastattr);
		}
		termput(DSP_TERM, DSP_TERMINATE);
		if (xterm_mouse_flg) {
			buf = (UCHAR*)XTERM_MOUSE_RESET;
			i1 = strlen((const char *)buf);
			if (outcnt + i1 > (INT) sizeof(outbuf)) vidaflush();
			memcpy(&outbuf[outcnt], buf, i1);
			outcnt += i1;
		}
		vidaflush();
	}

	if (inits & INIT_UNIX_DEVICE) evtdevexit(ttyin);
	if (inits & INIT_IOCTL) {
#if defined(USE_POSIX_TERMINAL_IO)
		if (ttyinflg) tcsetattr(ttyin, TCSADRAIN, &terminold);
		if (ttyoutflg && !(inits & INIT_TTYSAME)) tcsetattr(ttyout, TCSADRAIN, &termoutold);
#else
		if (ttyinflg) ioctl(ttyin, TCSETAW, &terminold);
		if (ttyoutflg && !(inits & INIT_TTYSAME)) ioctl(ttyout, TCSETAW, &termoutold);
#endif
	}
	if (inits & INIT_OUTPUT) close(ttyout);
	if (inits & INIT_INPUT) close(ttyin);

	if (inits & INIT_SIGNAL) {
		sigaction(SIGINT, &oldint, NULL);
#ifdef SIGTSTP
		sigaction(SIGINT, &oldtstp, NULL);
#endif
	}

	if (inits & INIT_MEMORY) {  /* free memory allocation */
		memfree((UCHAR **) screenbufferhandle);
		screenbufferhandle = NULL;
		memfree(keytabptr);
		keytabptr = NULL;
		for (i1 = 0; i1 < DSP_MAX; i1++) {
			memfree(dsptab[i1].tabptr);
			dsptab[i1].tabptr = NULL;
		}
	}

#if !defined(Linux)
	if (inits & INIT_SETUPTERM) reset_shell_mode();
#endif
	inits = 0;
}

void vidasuspend(UINT64 attr)
{
	if (attr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE)) {
		if ((attr & DSPATTR_AUXPORT) && !(lastattr & DSPATTR_AUXPORT)) attrput(attr);
	}
	else {
		attrput(attr & ~DSPATTR_GRAPHIC);
		if (lastdisable) {
			termput(DSP_WIN, DSP_ENABLE_ROLL);
			lastdisable = 0;
		}
		scrollregion(0, maxbot, 0, maxrgt);
		if (cursortype == CURSOR_NONE) cursorpos();
	}
	vidaflush();
	evtdevstop(ttyin);
#if defined(USE_POSIX_TERMINAL_IO)
	if (ttyinflg) tcsetattr(ttyin, TCSADRAIN, &terminold);
	if (ttyoutflg && !(inits & INIT_TTYSAME)) tcsetattr(ttyout, TCSADRAIN, &termoutold);
#else
	if (ttyinflg) ioctl(ttyin, TCSETAW, &terminold);
	if (ttyoutflg && !(inits & INIT_TTYSAME)) ioctl(ttyout, TCSETAW, &termoutold);
#endif
}

void vidaresume()
{
#if defined(USE_POSIX_TERMINAL_IO)
	if (ttyinflg) tcsetattr(ttyin, TCSADRAIN, &terminnew);
	if (ttyoutflg && !(inits & INIT_TTYSAME)) tcsetattr(ttyout, TCSADRAIN, &termoutnew);
#else
	if (ttyinflg) ioctl(ttyin, TCSETAW, &terminnew);
	if (ttyoutflg && !(inits & INIT_TTYSAME)) ioctl(ttyout, TCSETAW, &termoutnew);
#endif
	evtdevstart(ttyin);
	if (!(lastattr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE))) {
		lastinvalid |= DSPATTR_FGCOLORMASK | DSPATTR_BGCOLORMASK | (lastattr & (DSPATTR_UNDERLINE | DSPATTR_BLINK | DSPATTR_BOLD | DSPATTR_DIM | DSPATTR_REV));
		if ((caps1 & CAP1_POS) || ((caps1 & CAP1_POS_HORZ) && (caps1 & CAP1_POS_VERT)))
			lastcursv = lastcursh = INVALID_POSITION;
	}
}

void vidaflush()
{
	if (outcnt) {
		write(ttyout, (CHAR *) outbuf, outcnt);
		outcnt = 0;
	}
}

void vidacursor(INT type, UINT64 attr)
{
	INT i1;

	if (cursortype == type || (attr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE))) return;
	if (lastattr & DSPATTR_AUXPORT) attrput(lastattr & ~DSPATTR_AUXPORT);

	cursortype = type;
	if (cursortype == CURSOR_NONE) i1 = DSP_CURSOR_OFF;
	else {
		cursorpos();
		if (cursortype == CURSOR_BLOCK && (caps1 & CAP1_CURSOR_BLOCK)) i1 = DSP_CURSOR_BLOCK;
		else if (cursortype == CURSOR_HALF && (caps1 & CAP1_CURSOR_HALF)) i1 = DSP_CURSOR_HALF;
		else if (caps1 & CAP1_CURSOR_UNDERLINE) i1 = DSP_CURSOR_UNDERLINE;
		else i1 = DSP_CURSOR_ON;
	}
	termput(DSP_SCREEN, i1);
}

void vidaposition(INT h, INT v, UINT64 attr)
{
	if ((cursorh == h && cursorv == v) || (attr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE))) return;
	if (lastattr & DSPATTR_AUXPORT) attrput(lastattr & ~DSPATTR_AUXPORT);

	cursorh = h;
	cursorv = v;
	if (cursortype != CURSOR_NONE) cursorpos();
}

void vidaputstring(UCHAR *string, INT length, UINT64 charattr)
{
	INT i1;
	UINT64 *screenbuf;
	UCHAR c1;

	/* low order byte of charattr is ignored unless string is NULL */
	if (length < 1) return;

	i1 = length;
	if (charattr & (DSPATTR_RAWMODE | DSPATTR_AUXPORT | DSPATTR_KYDSPIPE)) {
		/* not supporting graphic characters */
		if (charattr & DSPATTR_GRAPHIC) return;
		if ((charattr & DSPATTR_AUXPORT) && !(lastattr & DSPATTR_AUXPORT)) attrput(charattr);
		if (string == NULL) c1 = (UCHAR) charattr;
		while (i1--) {
			if (string != NULL) c1 = *string++;
			if (outcnt == sizeof(outbuf)) vidaflush();
			outbuf[outcnt++] = c1;
		}
/*** NOT UPDATING cursorh ANYMORE, BUT MAYBE lastcursh NEEDS TO BE INVALIDATED ***/
/***		if ((caps1 & CAP1_POS) || ((caps1 & CAP1_POS_HORZ) && (caps1 & CAP1_POS_VERT))) ***/
/***			lastcursv = INVALID_POSITION; ***/
		return;
	}

	screenbuf = *screenbufferhandle + cursorv * textwidth + cursorh;
/*** CODE: DOES NOT SUPPORT DSPATTR_DISPLEFT WHICH IS NOT CURRENTLY USED ***/
	if (!(charattr & DSPATTR_DISPDOWN)) {
		if (string == NULL) while (i1--) *screenbuf++ = charattr;
		else {
			charattr &= ~((UINT64)0xFF);
			while (i1--) *screenbuf++ = charattr | *string++;
		}
		textput(length);
		cursorh += length;
	}
	else {
		while (i1--) {
			if (string != NULL) charattr = (charattr & ~0x000000FF) | *string++;
			*screenbuf = charattr;
			screenbuf += textwidth;
			textput(1);
			cursorv++;
		}
	}
}

void vidaeraserect(INT top, INT bottom, INT left, INT right, UINT64 attr)
{
	INT i1, bgcolor, fgcolor, len, savecursorh, savecursorv, textflg, workattr;
	UINT64 *screenbuf;

	if (attr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE)) return;
	if (lastattr & DSPATTR_AUXPORT) attrput(lastattr & ~DSPATTR_AUXPORT);

	savecursorh = cursorh;
	savecursorv = cursorv;

	fgcolor = (attr & DSPATTR_FGCOLORMASK) >> DSPATTR_FGCOLORSHIFT;
	bgcolor = (attr & DSPATTR_BGCOLORMASK) >> DSPATTR_BGCOLORSHIFT;
	workattr = attr;
	if (workattr & DSPATTR_REV) {
		if (vidflags | VID_FLAG_COLORMODE_OLD) {
			if ((fgcolormask & bgcolormask & (0x01 << fgcolor)) && (fgcolormask & bgcolormask & (0x01 << bgcolor))) {
				i1 = fgcolor;
				fgcolor = bgcolor;
				bgcolor = i1;
				workattr &= ~DSPATTR_REV;
			}
		}
		else if (vidflags | VID_FLAG_COLORMODE_ANSI256) {
			i1 = fgcolor;
			fgcolor = bgcolor;
			bgcolor = i1;
			workattr &= ~DSPATTR_REV;
		}
	}

	textflg = FALSE;
	if (!(caps1 & CAP1_COLOR_ERASE) && (bgcolor != 0 || (workattr & DSPATTR_REV))) {
		if (caps1 & CAP1_ERASE_WIN_ATTR) goto erase1;
		textflg = TRUE;
		goto erase2;
	}
	if (right == maxrgt) {
		if (top == bottom && (caps1 & CAP1_EL)) {
			attrput(attr);
			cursorv = top;
			cursorh = left;
			cursorpos();
			termput(DSP_SCREEN, DSP_CLEAR_TO_LINE_END);
			goto erase2;
		}
		if (bottom == maxbot && !left) {
			if (!top && (caps1 & CAP1_ES)) {
				attrput(attr);
				termput(DSP_SCREEN, DSP_CLEAR_SCREEN);
				if ((caps1 & CAP1_POS) || ((caps1 & CAP1_POS_HORZ) && (caps1 & CAP1_POS_VERT)))
					lastcursv = lastcursh = INVALID_POSITION;
				goto erase2;
			}
			else if (caps1 & CAP1_EF) {
				attrput(attr);
				cursorv = top;
				cursorh = left;
				cursorpos();
				termput(DSP_SCREEN, DSP_CLEAR_TO_SCREEN_END);
				goto erase2;
			}
		}
	}
	if (caps1 & CAP1_ERASE_WIN) {
		if (caps1 & CAP1_ERASE_WIN_ATTR) {
erase1:
			if (vidflags & VID_FLAG_COLORMODE_OLD) {
				if (workattr & DSPATTR_REV) {
					i1 = fgcolor;
					fgcolor = bgcolor;
					bgcolor = i1;
				}
				if ((workattr & DSPATTR_BOLD) && fgcolor < 8) fgcolor += 8;
			}
		}
		else attrput(attr);
		//TODO 256 Colors
		termparmput(DSP_WIN, DSP_ERASE_WIN, top, bottom, left, right, bottom - top + 1, (bgcolor << 4) + fgcolor);
		goto erase2;
	}
	if ((caps1 & CAP1_SCROLL_ERASE) && ((!top && bottom == maxbot) || (caps1 & CAP1_SET_TB)) &&
	   ((!left && right == maxrgt) || (caps1 & CAP1_SET_LR)) && (caps1 & CAP1_ES)) {
		scrollregion(top, bottom, left, right);
		attrput(attr);
		termput(DSP_SCREEN, DSP_CLEAR_SCREEN);
		if ((caps1 & CAP1_POS) || ((caps1 & CAP1_POS_HORZ) && (caps1 & CAP1_POS_VERT)))
			lastcursv = lastcursh = INVALID_POSITION;
		goto erase2;
	}
	if (right == maxrgt && (caps1 & CAP1_EL)) {
		attrput(attr);
		for (cursorv = top, cursorh = left; cursorv <= bottom; cursorv++) {
			cursorpos();
			termput(DSP_SCREEN, DSP_CLEAR_TO_LINE_END);
		}
		goto erase2;
	}
	textflg = TRUE;

erase2:
	if (!(caps1 & CAP1_POS)) textflg = FALSE;
	cursorh = left;
	attr = (attr & ~0xFF) | ' ';
	len = right - left + 1;
	for (cursorv = top; cursorv <= bottom; cursorv++) {
		screenbuf = *screenbufferhandle + cursorv * textwidth + left;
		for (i1 = len; i1--; ) *screenbuf++ = attr;
		if (textflg) textput(len);
	}
	cursorh = savecursorh;
	cursorv = savecursorv;
	if (cursortype != CURSOR_NONE) cursorpos();
}

void vidagetrect(UCHAR *mem, INT top, INT bottom, INT left, INT right, INT charonly)
{
	INT i1, i2, chr;
	UINT64 *screenbuf, chr64;

	/**
	 * In 'OLD' mode, each char/attribute is 1 or 3 bytes, 1st is char, 2nd is color byte, 3rd is attribute
	 */
	if (charonly) {
		for (i1 = top; i1 <= bottom; i1++) {
			screenbuf = *screenbufferhandle + i1 * textwidth + left;
			for (i2 = left; i2 <= right; i2++) {
				chr = *screenbuf++;
				if (chr & DSPATTR_GRAPHIC) chr = ' ';
				*mem++ = (UCHAR) chr;
			}
		}
	}
	else {
		for (i1 = top; i1 <= bottom; i1++) {
			screenbuf = *screenbufferhandle + i1 * textwidth + left;
			for (i2 = left; i2 <= right; i2++) {
				if (vidflags & VID_FLAG_COLORMODE_OLD) {
					chr = *screenbuf++;
					*mem++ = (UCHAR) chr;			// The ascii CHAR
					*mem++ = (UCHAR)(chr >> 8);		// The two 4-bit colors
					*mem++ = (UCHAR)(chr >> 16);	// The attributes
				}
				else if (vidflags & VID_FLAG_COLORMODE_ANSI256) {
					chr64 = *screenbuf++;
					*mem++ = (UCHAR) chr64;			// The ascii CHAR
					*mem++ = (UCHAR)(chr64 >> 16);	// The attributes
					*mem++ = (UCHAR)(chr64 >> 32);	// The fore color
					*mem++ = (UCHAR)(chr64 >> 40);	// The back color
				}
			}
		}
	}
}

void vidaputrect(UCHAR *mem, INT top, INT bottom, INT left, INT right, INT charonly, UINT64 attr)
{
	INT i1, chr, savecursorh, savecursorv;
	UINT64 *screenbuf;
	UCHAR theChar;
	UINT64 aTtrs, fclr, bclr;

	/* each char/attribute is 1 or 3 bytes, 1st is char, 2nd is color byte, 3rd is attribute */
	if (attr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE)) return;

	savecursorh = cursorh;
	savecursorv = cursorv;
	cursorh = left;
	attr &= ~(0xFFLL);
	if (charonly) {
		for (cursorv = top; cursorv <= bottom; cursorv++) {
			screenbuf = *screenbufferhandle + cursorv * textwidth + left;
			for (i1 = left; i1 <= right; i1++)
				*screenbuf++ = *mem++ | (attr & ~DSPATTR_GRAPHIC);
			textput(right - left + 1);
		}
	}
	else {
		for (cursorv = top; cursorv <= bottom; cursorv++) {
			screenbuf = *screenbufferhandle + cursorv * textwidth + left;
			for (i1 = left; i1 <= right; i1++) {
				if (vidflags & VID_FLAG_COLORMODE_OLD) {
					chr = *mem++;
					chr |= (INT)(*mem++) << 8;
					chr |= (INT)(*mem++) << 16;
					*screenbuf++ = chr;
				}
				else if (vidflags & VID_FLAG_COLORMODE_ANSI256) {
					theChar = *mem++;
					aTtrs = *mem++;
					fclr = *mem++;
					bclr = *mem++;
					*screenbuf++ = theChar | (aTtrs << 16) | (fclr << DSPATTR_FGCOLORSHIFT) | (bclr << DSPATTR_BGCOLORSHIFT);
				}
			}
			textput(right - left + 1);
		}
	}
	cursorh = savecursorh;
	cursorv = savecursorv;
	if (cursortype != CURSOR_NONE) cursorpos();
}

/**
 * 'direction' is one of ROLL_UP(1), ROLL_DOWN(2), ROLL_LEFT(3), ROLL_RIGHT(4)
 */
void vidamove(INT direction, INT top, INT bottom, INT left, INT right, UINT64 attr)
{
	INT i1, i2, bgcolor, fgcolor, savecursorh, savecursorv, textflg, workattr;
	UINT64 *src, *dest;

	if ((top == bottom && (direction == ROLL_UP || direction == ROLL_DOWN)) ||
	    (left == right && (direction == ROLL_LEFT || direction == ROLL_RIGHT))) {
		vidaeraserect(top, bottom, left, right, attr);
		return;
	}

	if (attr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE)) return;
	if (lastattr & DSPATTR_AUXPORT) attrput(lastattr & ~DSPATTR_AUXPORT);

	attr = (attr & ~0xFF) | ' ';
	if (direction == ROLL_UP) {
		for (i1 = top; i1 < bottom; i1++) {
			dest = *screenbufferhandle + i1 * textwidth + left;
			src = dest + textwidth;
			for (i2 = left; i2 <= right; i2++) *dest++ = *src++;
		}
		dest = *screenbufferhandle + bottom * textwidth + left;
		for (i2 = left; i2 <= right; i2++) *dest++ = attr;
	}
	else if (direction == ROLL_DOWN) {
		for (i1 = bottom; i1 > top; i1--) {
			dest = *screenbufferhandle + i1 * textwidth + left;
			src = dest - textwidth;
			for (i2 = left; i2 <= right; i2++) *dest++ = *src++;
		}
		dest = *screenbufferhandle + top * textwidth + left;
		for (i2 = left; i2 <= right; i2++) *dest++ = attr;
	}
	else if (direction == ROLL_LEFT) {
		for (i1 = top; i1 <= bottom; i1++) {
			dest = *screenbufferhandle + i1 * textwidth + left;
			src = dest + 1;
			for (i2 = left; i2 < right; i2++) *dest++ = *src++;
			*dest = attr;
		}
	}
	else if (direction == ROLL_RIGHT) {
		for (i1 = top; i1 <= bottom; i1++) {
			dest = *screenbufferhandle + i1 * textwidth + right;
			src = dest - 1;
			for (i2 = left; i2 < right; i2++) *dest-- = *src--;
			*dest = attr;
		}
	}
	else return;

	if (lastdisable) {
		termput(DSP_WIN, DSP_ENABLE_ROLL);
		lastdisable = 0;
	}

	savecursorh = cursorh;
	savecursorv = cursorv;

	fgcolor = (attr & DSPATTR_FGCOLORMASK) >> DSPATTR_FGCOLORSHIFT;
	bgcolor = (attr & DSPATTR_BGCOLORMASK) >> DSPATTR_BGCOLORSHIFT;
	workattr = attr;
	if (workattr & DSPATTR_REV) {
		if (vidflags | VID_FLAG_COLORMODE_OLD) {
			if ((fgcolormask & bgcolormask & (0x01 << fgcolor)) && (fgcolormask & bgcolormask & (0x01 << bgcolor))) {
				i1 = fgcolor;
				fgcolor = bgcolor;
				bgcolor = i1;
				workattr &= ~DSPATTR_REV;
			}
		}
		else if (vidflags | VID_FLAG_COLORMODE_ANSI256) {
			i1 = fgcolor;
			fgcolor = bgcolor;
			bgcolor = i1;
			workattr &= ~DSPATTR_REV;
		}
	}

	if (!(caps1 & CAP1_COLOR_ERASE) && (bgcolor != 0 || (workattr & DSPATTR_REV))) textflg = 1;
	else textflg = 0;
	if (lasttop == top && lastbot == bottom && lastlft == left && lastrgt == right
			&& (caps2 & (CAP2_ROLL_UP << (direction - ROLL_UP)))) {
		attrput(attr);
		if (direction == ROLL_UP) cursorv = bottom;
		else cursorv = top;
		if (direction == ROLL_LEFT) cursorh = right;
		else cursorh = left;
		cursorpos();
		termput(DSP_WIN, DSP_SCROLL_UP + (direction - ROLL_UP));
	}
	else if (caps2 & (CAP2_WIN_UP << (direction - ROLL_UP))) {
		if (caps2 & (CAP2_WIN_UP_ATTR << (direction - ROLL_UP))) {
			if (workattr & DSPATTR_REV) {
				i1 = fgcolor;
				fgcolor = bgcolor;
				bgcolor = i1;
			}
			if ((workattr & DSPATTR_BOLD) && fgcolor < 8) fgcolor += 8;
			textflg = 0;
		}
		else attrput(attr);
		//TODO 256 Color
		termparmput(DSP_WIN, DSP_SCROLL_WIN_UP + (direction - ROLL_UP), top, bottom, left, right, 1,
				(bgcolor << 4) + fgcolor);
	}
	else if (((!top && bottom == maxbot) || (caps1 & CAP1_SET_TB)) &&
	   ((!left && right == maxrgt) || (caps1 & CAP1_SET_LR)) && (caps2 & (CAP2_ROLL_UP << (direction - ROLL_UP)))) {
		scrollregion(top, bottom, left, right);
		attrput(attr);
		if (direction == ROLL_UP) cursorv = bottom;
		else cursorv = top;
		if (direction == ROLL_LEFT) cursorh = right;
		else cursorh = left;
		cursorpos();
		termput(DSP_WIN, DSP_SCROLL_UP + (direction - ROLL_UP));
	}
	else if ((bottom == maxbot || (caps1 & CAP1_SET_TB)) && !left && right == maxrgt && ((direction == ROLL_UP && (caps1 & CAP1_DELETE_LINE)) || (direction == ROLL_DOWN && (caps1 & CAP1_INSERT_LINE)))) {
		scrollregion(lasttop, bottom, left, right);
		attrput(attr);
		cursorv = top;
		cursorh = left;
		cursorpos();
		if (direction == ROLL_UP) i1 = DSP_DELETE_LINE;
		else i1 = DSP_INSERT_LINE;
		termput(DSP_EDIT, i1);
	}
	else if ((right == maxrgt || (caps1 & CAP1_SET_LR)) && ((direction == ROLL_LEFT && (caps1 & CAP1_DELETE_CHAR)) || (direction == ROLL_RIGHT && (caps1 & CAP1_INSERT_CHAR)))) {
		scrollregion(lasttop, lastbot, left, right);
		attrput(attr);
		if (direction == ROLL_LEFT) i1 = DSP_DELETE_CHAR;
		else i1 = DSP_INSERT_CHAR;
		cursorh = left;
		for (cursorv = top; cursorv <= bottom; cursorv++) {
			cursorpos();
			termput(DSP_EDIT, i1);
		}
	}
	else textflg = -1;

	if (textflg == 1) {
		if (direction == ROLL_UP) top = bottom;
		else if (direction == ROLL_DOWN) bottom = top;
		else if (direction == ROLL_LEFT) left = right;
		else if (direction == ROLL_RIGHT) right = left;
	}
	if (textflg) {
		if (caps1 & CAP1_POS) {
			cursorh = left;
			for (cursorv = top; cursorv <= bottom; cursorv++) textput(right - left + 1);
		}
		else if (textflg == -1 && direction == ROLL_UP && cursorv == lastbot) {
			if (outcnt == sizeof(outbuf)) vidaflush();
			outbuf[outcnt++] = '\n';
		}
	}
	cursorh = savecursorh;
	cursorv = savecursorv;
	if (cursortype != CURSOR_NONE) cursorpos();
}

void vidainsertchar(INT h, INT v, INT top, INT bottom, INT left, INT right, UINT64 charattr)
{
	INT endh, len, linecnt;
	UINT64 *screenbuf, *src, *dest;

	if (charattr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE)) return;

	endh = h;
	if (--h < left) {
		h = right;
		v--;
	}
	linecnt = 0;
	screenbuf = *screenbufferhandle;
	while (v > cursorv || (v == cursorv && h == right)) {
		if (h == right) {
			src = screenbuf + v * textwidth + right;
			dest = screenbuf + (v + 1) * textwidth + left;
			*dest = *src;
			if (--h < left) {
				h = right;
				v--;
			}
			linecnt++;
		}
		else {
			src = screenbuf + v * textwidth + h;
			dest = src + 1;
			for (len = h - left + 1; len--; ) *dest-- = *src--;
			h = right;
			v--;
		}
	}
	if (cursorh != right) {
		src = screenbuf + v * textwidth + h;
		dest = src + 1;
		for (len = h - cursorh + 1; len--; ) *dest-- = *src--;
	}
	dest = screenbuf + cursorv * textwidth + cursorh;
	*dest = charattr;
	if (!linecnt) textput(endh - cursorh + 1);
	else {
		textput(right - cursorh + 1);
		h = cursorh;
		v = cursorv;
		cursorh = left;
		for (cursorv++; ; cursorv++) {
			if (!--linecnt) {
				textput(endh - left + 1);
				break;
			}
			textput(right - left + 1);
		}
		cursorh = h;
		cursorv = v;
	}
	if (cursortype != CURSOR_NONE) cursorpos();
}

void vidadeletechar(INT h, INT v, INT top, INT bottom, INT left, INT right, UINT64 charattr)
{
	INT endh, endv, len, linecnt;
	UINT64 *screenbuf, *src, *dest;

	if (charattr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE)) return;

	endh = h;
	endv = v;
	h = cursorh;
	v = cursorv;
	if (++h > right) {
		h = left;
		v++;
	}
	linecnt = 0;
	screenbuf = *screenbufferhandle;
	while (v < endv || (v == endv && h == left)) {
		if (h == left) {
			src = screenbuf + v * textwidth + left;
			dest = screenbuf + (v - 1) * textwidth + right;
			*dest = *src;
			if (++h > right) {
				h = left;
				v++;
			}
			linecnt++;
		}
		else {
			src = screenbuf + v * textwidth + h;
			dest = src - 1;
			for (len = right - h + 1; len--; ) *dest++ = *src++;
			h = left;
			v++;
		}
	}
	if (endh != left) {
		src = screenbuf + v * textwidth + h;
		dest = src - 1;
		for (len = endh - h + 1; len--; ) *dest++ = *src++;
	}
	dest = screenbuf + endv * textwidth + endh;
	*dest = charattr;
	if (!linecnt) textput(endh - cursorh + 1);
	else {
		textput(right - cursorh + 1);
		h = cursorh;
		v = cursorv;
		cursorh = left;
		for (cursorv++; ; cursorv++) {
			if (!--linecnt) {
				textput(endh - left + 1);
				break;
			}
			textput(right - left + 1);
		}
		cursorh = h;
		cursorv = v;
	}
	if (cursortype != CURSOR_NONE) cursorpos();
}

void vidasound(INT pitch, INT time, UINT64 attr, INT beepflag)
{
	if ((attr & (DSPATTR_AUXPORT | DSPATTR_KYDSPIPE)) || time < 2) return;
	if (lastattr & DSPATTR_AUXPORT) attrput(lastattr & ~DSPATTR_AUXPORT);

	termput(DSP_SCREEN, DSP_BELL);
	vidaflush();
}

static void cursorpos()
{
	INT cnt, h, v, lasth, lastv;
	UINT poscaps;

	if (cursorh == lastcursh && cursorv == lastcursv) return;
	h = cursorh;
	v = cursorv;
	if (caps1 & CAP1_SCROLL_REPOS) {
		/* this may invalidate last cursor position */
		if (h < lastlft || h > lastrgt || v < lasttop || v > lastbot)
			scrollregion(0, maxbot, 0, maxrgt);
		h -= lastlft;
		v -= lasttop;
	}
	poscaps = caps1 & (CAP1_POS | CAP1_POS_HORZ | CAP1_POS_VERT);
	if (cursorh == lastcursh) {
		if (poscaps & CAP1_POS_VERT) poscaps &= ~CAP1_POS;
		poscaps &= ~CAP1_POS_HORZ;
	}
	else if (cursorv == lastcursv) {
		if (poscaps & CAP1_POS_HORZ) poscaps &= ~CAP1_POS;
		poscaps &= ~CAP1_POS_VERT;
	}
	if (caps1 & CAP1_ASCII_CONTROL) {
		lasth = lastcursh;
		lastv = lastcursv;
		cnt = 0;
		if (caps1 & CAP1_SCROLL_REPOS) {
			lasth -= lastlft;
			lastv -= lasttop;
			if (lastcursh < lastlft || lastcursh > lastrgt || lastcursv < lasttop || lastcursv > lastbot) cnt += 10;
		}
		else if (lastcursh > maxrgt || lastcursv > maxbot) cnt += 10;
		if (!h) {
			if (lasth) cnt++;
		}
		else if (h <= lasth) cnt += lasth - h;
		else if (!poscaps) cnt += h - lasth;
		else cnt += 10;
		if (v >= lastv) cnt += v - lastv;
		else cnt += 10;
		if (cnt <= 6 || !poscaps) {
			if (outcnt + cnt >= (INT) sizeof(outbuf)) vidaflush();
			if (!h) {
				if (lasth) outbuf[outcnt++] = '\r';
			}
			else if (h < lasth) {
				memset(&outbuf[outcnt], '\b', lasth - h);
				outcnt += lasth - h;
			}
			else if (h > lasth) {
				memset(&outbuf[outcnt], ' ', h - lasth);
				outcnt += h - lasth;
			}
			if (v > lastv) {
				memset(&outbuf[outcnt], '\n', v - lastv);
				outcnt += v - lastv;
			}
			lastcursh = cursorh;
			if (cursorv > lastcursv) lastcursv = cursorv;
			return;
		}
	}
	if (poscaps & CAP1_POS) termparmput(DSP_SCREEN, DSP_POSITION_CURSOR, v, h, 0, 0, 0, 0);
	else {
		if (poscaps & CAP1_POS_HORZ) termparmput(DSP_SCREEN, DSP_POSITION_CURSOR_HORZ, h, 0, 0, 0, 0, 0);
		if (poscaps & CAP1_POS_VERT) termparmput(DSP_SCREEN, DSP_POSITION_CURSOR_VERT, v, 0, 0, 0, 0, 0);
	}
	lastcursh = cursorh;
	lastcursv = cursorv;
}

static void textput(INT length)
{
	INT i1, chr;
	UINT64 *screenbuf;

/*** CODE: VERIFY DISPLAYING IN BOTTOM LEFT OF SCROLL REGION DOES NOT CAUSE ***/
/***       THE SCROLL REGION TO ROLL. IN THEORY, ROLLING LEFT MAY ALSO HAVE ***/
/***       TO BE PREVENTED, BUT NO KNOWN TERMINAL DOES THIS ***/
	if (cursorv == maxbot && cursorh + length == textwidth && !(caps1 & CAP1_NO_ROLL)) {
		if (caps1 & CAP1_DISABLE_ENABLE) {
			if (!lastdisable) lastdisable = !termput(DSP_WIN, DSP_DISABLE_ROLL);
		}
		else length--;
	}
	cursorpos();
	lastcursh += length;
	screenbuf = *screenbufferhandle + cursorv * textwidth + cursorh;
	if (lastinvalid) attrput(*screenbuf);
	for ( ; ; ) {
		for ( ; length && lastattr == (*screenbuf & ~0xFFLL); length--) {
			chr = (UCHAR) *screenbuf++;
			if (!(lastattr & DSPATTR_GRAPHIC)) {
				if (!(caps2 & CAP2_SPECIAL_DISPLAY) || termput(DSP_SDSP, chr)) {
					if (outcnt == sizeof(outbuf)) vidaflush();
					outbuf[outcnt] = (UCHAR) chr;
					outcnt++;
				}
			}
			else {
				i1 = chr & 0x3F;
				if (i1 < VID_SYM_UPA) {
					i1 += 6;
					if (chr & 0x40) i1 += 11;
					if (chr & 0x80) i1 += 22;
				}
				else i1 -= VID_SYM_UPA - 2;
				if (termput(DSP_GRAPH, i1)) {
					screenbuf--;
					*screenbuf = (*screenbuf & ~(DSPATTR_GRAPHIC | 0xFF)) | ' ';
					break;
				}
			}
		}
		if (!length) break;
		attrput(*screenbuf);
	}
}

static void attrput(UINT64 attr)
{
	static UINT64 realattr = 0;
	INT i1, allflg, newbgcolor, newfgcolor, oldbgcolor, oldfgcolor;

	attr &= ~0xFFLL;
	if (attr == lastattr && !lastinvalid) return;
	/* calculate the values of old colors */
	oldfgcolor = (lastattr & DSPATTR_FGCOLORMASK) >> DSPATTR_FGCOLORSHIFT;
	oldbgcolor = (lastattr & DSPATTR_BGCOLORMASK) >> DSPATTR_BGCOLORSHIFT;
	if (lastattr & DSPATTR_REV) {
		if (vidflags | VID_FLAG_COLORMODE_OLD) {
			if ((fgcolormask & bgcolormask & (0x01 << oldfgcolor)) && (fgcolormask & bgcolormask & (0x01 << oldbgcolor))) {
				i1 = oldfgcolor;
				oldfgcolor = oldbgcolor;
				oldbgcolor = i1;
				lastattr &= ~DSPATTR_REV;
			}
		}
		else if (vidflags | VID_FLAG_COLORMODE_ANSI256) {
			i1 = oldfgcolor;
			oldfgcolor = oldbgcolor;
			oldbgcolor = i1;
			lastattr &= ~DSPATTR_REV;
		}
	}
	if (lastattr & DSPATTR_BOLD) {
		if (vidflags | VID_FLAG_COLORMODE_OLD) {
			if (!(lastattr & DSPATTR_REV)) {
				if (oldfgcolor < 8 && (fgcolormask & (0x0100 << oldfgcolor))) oldfgcolor += 8;
			}
			else if (oldbgcolor < 8 && (bgcolormask & (0x0100 << oldbgcolor))) oldbgcolor += 8;
		}
	}
	if (vidflags | VID_FLAG_COLORMODE_OLD) {
		if (oldfgcolor >= 8 && oldfgcolor < 16 && !(fgcolormask & (0x01 << oldfgcolor))
				&& (fgcolormask & (0x01 << (oldfgcolor - 8)))){
			oldfgcolor -= 8;
		}
	}

	/* calculate the values of new colors */
	lastattr = attr;
	newfgcolor = (attr & DSPATTR_FGCOLORMASK) >> DSPATTR_FGCOLORSHIFT;
	newbgcolor = (attr & DSPATTR_BGCOLORMASK) >> DSPATTR_BGCOLORSHIFT;
	if (attr & DSPATTR_REV) {
		if (vidflags | VID_FLAG_COLORMODE_OLD) {
			if ((fgcolormask & bgcolormask & (0x01 << newfgcolor)) && (fgcolormask & bgcolormask & (0x01 << newbgcolor))) {
				i1 = newfgcolor;
				newfgcolor = newbgcolor;
				newbgcolor = i1;
				attr &= ~DSPATTR_REV;
			}
		}
		else if (vidflags | VID_FLAG_COLORMODE_ANSI256) {
			i1 = newfgcolor;
			newfgcolor = newbgcolor;
			newbgcolor = i1;
			attr &= ~DSPATTR_REV;
		}
	}
	if (attr & DSPATTR_BOLD && vidflags | VID_FLAG_COLORMODE_OLD) {
		if (!(attr & DSPATTR_REV)) {
			if (newfgcolor < 8 && (fgcolormask & (0x0100 << newfgcolor))) {
				newfgcolor += 8;
				attr &= ~DSPATTR_BOLD;
			}
		}
		else if (newbgcolor < 8 && (bgcolormask & (0x0100 << newbgcolor))) {
			newbgcolor += 8;
			attr &= ~DSPATTR_BOLD;
		}
	}
	if (vidflags | VID_FLAG_COLORMODE_OLD) {
		if (newfgcolor >= 8 && newfgcolor < 16 && !(fgcolormask & (0x01 << newfgcolor))
				&& (fgcolormask & (0x01 << (newfgcolor - 8))))
		{
			newfgcolor -= 8;
			attr |= DSPATTR_BOLD;
		}
	}

	/* force invalid attributes to reset themselves */
	if (lastinvalid) {
		if (lastinvalid & DSPATTR_FGCOLORMASK) oldfgcolor = -1;
		if (lastinvalid & DSPATTR_BGCOLORMASK) oldbgcolor = -1;
		realattr &= ~(lastinvalid & attr
				& (DSPATTR_UNDERLINE | DSPATTR_BLINK | DSPATTR_BOLD | DSPATTR_DIM | DSPATTR_REV | DSPATTR_GRAPHIC | DSPATTR_AUXPORT));
		lastinvalid = 0;
	}

	/* turn off the old attributes no longer needed */
	allflg = FALSE;
	if ((realattr & DSPATTR_UNDERLINE) && !(attr & DSPATTR_UNDERLINE)) {
		if (!termput(DSP_SCREEN, DSP_UNDERLINE_OFF)) realattr &= ~DSPATTR_UNDERLINE;
		else allflg = TRUE;
	}
	if ((realattr & DSPATTR_BLINK) && !(attr & DSPATTR_BLINK)) {
		if (!termput(DSP_SCREEN, DSP_BLINK_OFF)) realattr &= ~DSPATTR_BLINK;
		else allflg = TRUE;
	}
	if ((realattr & DSPATTR_BOLD) && !(attr & DSPATTR_BOLD)) {
		if (!termput(DSP_SCREEN, DSP_BOLD_OFF)) realattr &= ~DSPATTR_BOLD;
		else allflg = TRUE;
	}
	if ((realattr & DSPATTR_DIM) && !(attr & DSPATTR_DIM)) {
		if (!termput(DSP_SCREEN, DSP_DIM_OFF)) realattr &= ~DSPATTR_DIM;
		else allflg = TRUE;
	}
	if ((realattr & DSPATTR_REV) && (!(attr & DSPATTR_REV) || oldfgcolor != newfgcolor || oldbgcolor != newbgcolor)) {
		if (!termput(DSP_SCREEN, DSP_REVERSE_OFF)) {
			if (attr & DSPATTR_REV) oldfgcolor = oldbgcolor = -1;
			realattr &= ~DSPATTR_REV;
		}
		else allflg = TRUE;
	}
	if ((realattr & DSPATTR_GRAPHIC) && !(attr & DSPATTR_GRAPHIC)) {
		if (!termput(DSP_GRAPH, DSP_GRAPHIC_OFF)) realattr &= ~DSPATTR_GRAPHIC;
		else allflg = TRUE;
	}
	if ((realattr & DSPATTR_AUXPORT) && !(attr & DSPATTR_AUXPORT)) {
		if (!termput(DSP_SCREEN, DSP_AUXPORT_OFF)) realattr &= ~DSPATTR_AUXPORT;
		else allflg = TRUE;
	}
	if (allflg && !termput(DSP_SCREEN, DSP_ALL_OFF)) {
		realattr = 0;
		oldfgcolor = oldbgcolor = -1;
	}

	if (oldfgcolor != newfgcolor) {
		if (vidflags | VID_FLAG_COLORMODE_ANSI256 && caps1 & CAP1_FGCOLOR_ANSI256) {
			termparmput(DSP_SCREEN, DSP_FGCOLOR_ANSI256, newfgcolor, 0, 0, 0, 0, 0);
		}
		else termput(DSP_FG_COLOR, newfgcolor);
	}
	if (oldbgcolor != newbgcolor) {
		if (vidflags | VID_FLAG_COLORMODE_ANSI256 && caps1 & CAP1_BGCOLOR_ANSI256) {
			termparmput(DSP_SCREEN, DSP_BGCOLOR_ANSI256, newbgcolor, 0, 0, 0, 0, 0);
		}
		else termput(DSP_BG_COLOR, newbgcolor);
	}
	/* turn on the new attributes if not already on */
	if ((attr & DSPATTR_UNDERLINE) && !(realattr & DSPATTR_UNDERLINE) && !termput(DSP_SCREEN, DSP_UNDERLINE_ON)) realattr |= DSPATTR_UNDERLINE;
	if ((attr & DSPATTR_BLINK) && !(realattr & DSPATTR_BLINK) && !termput(DSP_SCREEN, DSP_BLINK_ON)) realattr |= DSPATTR_BLINK;
	if ((attr & DSPATTR_BOLD) && !(realattr & DSPATTR_BOLD) && !termput(DSP_SCREEN, DSP_BOLD_ON)) realattr |= DSPATTR_BOLD;
	if ((attr & DSPATTR_DIM) && !(realattr & DSPATTR_DIM) && !termput(DSP_SCREEN, DSP_DIM_ON)) realattr |= DSPATTR_DIM;
	if ((attr & DSPATTR_REV) && !(realattr & DSPATTR_REV) && !termput(DSP_SCREEN, DSP_REVERSE_ON)) realattr |= DSPATTR_REV;
	if ((attr & DSPATTR_GRAPHIC) && !(realattr & DSPATTR_GRAPHIC) && !termput(DSP_GRAPH, DSP_GRAPHIC_ON)) realattr |= DSPATTR_GRAPHIC;
	if ((attr & DSPATTR_AUXPORT) && !(realattr & DSPATTR_AUXPORT) && !termput(DSP_SCREEN, DSP_AUXPORT_ON)) realattr |= DSPATTR_AUXPORT;
}

static void scrollregion(INT top, INT bottom, INT left, INT right)
{
	if (top != lasttop || bottom != lastbot) {
		if (!termparmput(DSP_WIN, DSP_SCROLL_REGION_TB, top, bottom, 0, 0, 0, 0)) {
			lasttop = top;
			lastbot = bottom;
			if ((caps1 & CAP1_POS) || ((caps1 & CAP1_POS_HORZ) && (caps1 & CAP1_POS_VERT)))
				lastcursv = lastcursh = INVALID_POSITION;
		}
	}
	if (left != lastlft || right != lastrgt) {
		if (!termparmput(DSP_WIN, DSP_SCROLL_REGION_LR, left, right, 0, 0, 0, 0)) {
			lastlft = left;
			lastrgt = right;
			if ((caps1 & CAP1_POS) || ((caps1 & CAP1_POS_HORZ) && (caps1 & CAP1_POS_VERT)))
				lastcursv = lastcursh = INVALID_POSITION;
		}
	}
}

static INT termput(INT table, INT function)
{
	INT i1, len;
	UCHAR *buf;


	if (function >= dsptab[table].entries) {
		return RC_ERROR;
	}
	if ((i1 = ((INT *) *dsptab[table].tabptr)[function]) == -1) {
		return RC_ERROR;
	}
	buf = *dsptab[table].tabptr + dsptab[table].entries * sizeof(INT) + i1;
	len = *buf++;
	if (outcnt + len > (INT) sizeof(outbuf)) vidaflush();
	memcpy(&outbuf[outcnt], buf, len);
	outcnt += len;
	return 0;
}

static INT termparmput(INT table, INT function, INT p1, INT p2, INT p3, INT p4, INT p5, INT p6)
{
	INT i1, len, type, value = 0;
	UCHAR chr, work[17], *buf;

	if (function >= dsptab[table].entries) return RC_ERROR;
	if ((i1 = ((INT *) *dsptab[table].tabptr)[function]) == -1) return RC_ERROR;

	buf = *dsptab[table].tabptr + dsptab[table].entries * sizeof(INT) + i1;
	len = *buf++;
	if (inits & INIT_TERMDEF) {
		while (len-- > 0) {
			if ((UINT)(outcnt + 10) > sizeof(outbuf)) vidaflush();
			chr = *buf++;
			if (chr) outbuf[outcnt++] = chr;
			else {
				if (len-- > 0) chr = *buf++;
				if (!chr) outbuf[outcnt++] = 0;
				else {  /* parm */
					if (!(chr & 0x80)) {
						switch (chr & 0x07) {
							case 1:  /* 1st parm */
								value = p1;
								break;
							case 2:  /* 2nd parm */
								value = p2;
								break;
							case 3:  /* 3rd parm */
								value = p3;
								break;
							case 4:  /* 4th parm */
								value = p4;
								break;
							case 5:  /* 5th parm */
								value = p5;
								break;
							case 6:  /* 6th parm */
								value = p6;
								break;
						}
					}
					else {
						switch (chr & 0x07) {
							case 0:  /* -2nd parm */
								value = -p2;
								break;
							case 1:  /* 2nd parm - 1st parm */
								value = p2 - p1;
								break;
							case 2:  /* -4th parm */
								value = -p4;
								break;
							case 3:  /* 4th parm - 3rd parm */
								value = p4 - p3;
								break;
						}
					}
					if ((chr & 0x08) && len-- > 0) value += *buf++;  /* add next byte */

					type = chr & 0x70;
					if (type == 0x10 || type == 0x20) {  /* ASCII characters */
						if (type == 0x10) {  /* 1 or more ASCII characters */
							i1 = 0;
							do work[i1++] = (UCHAR)(value % 10) + '0';
							while (value /= 10);
							while (i1--) outbuf[outcnt++] = work[i1];
						}
						else {  /* 2 ASCII characters */
							value %= 100;
							outbuf[outcnt++] = value / 10 + '0';
							outbuf[outcnt++] = value % 10 + '0';
						}
					}
					else if (type == 0x40) {  /* special format t */
						outbuf[outcnt++] = (UCHAR)(0x20 | ((value >> 5) & 0x1f));
						outbuf[outcnt++] = (UCHAR)(0x20 | (value & 0x1f));
					}
					else if (type == 0x30 || type == 0x50) {  /* special format z or n */
						if (type == 0x30 || value) {
							outbuf[outcnt++] = (UCHAR)(0x30 | ((value >> 4) & 0x0f));
							outbuf[outcnt++] = (UCHAR)(0x30 | (value & 0x0f));
							if (type == 0x50) outbuf[outcnt++] = 0x30;
						}
					}
					else outbuf[outcnt++] = (UCHAR) value;  /* binary byte */
				}
			}
		}
	}
	else tputs(tparm((CHAR *) buf, p1, p2, 0, 0, 0, 0, 0, 0, 0), len, charput);  /* terminfo */
	return 0;
}

static INT charput(
		INT chr
		)
{
	if (outcnt >= (INT) sizeof(outbuf)) vidaflush();
	outbuf[outcnt++] = (UCHAR) chr;
	return (INT) chr;
}

static INT readcallback(void *handle, INT polltype)
{
	if (polltype & DEVPOLL_BLOCK) polltype = -1;
	else polltype = 0;

	return readtty(polltype);
}

static INT readtty(INT waitflg)
{
	static INT bufcnt = 0;
	static UCHAR buf[256];
	INT cnt, key, len, oldalarm, oldcnt, sigerr, slept, timeoutflg;
	INT x, y;
	UCHAR *keytab;
	struct sigaction act, oact;
	/*struct itimerval itimer, oldtimer, dummy_timer, slept_timer;*/
	if (bufcnt == sizeof(buf)) goto readtty1;
	if (ttyinflg) {
		/* assume same device (handle) as ttyin */
		if (waitflg >= 0) {
			if (terminnew.c_cc[VMIN] != 0 || terminnew.c_cc[VTIME] != waitflg) {
				terminnew.c_cc[VMIN] = 0;
				terminnew.c_cc[VTIME] = waitflg;
#if defined(USE_POSIX_TERMINAL_IO)
				tcsetattr(ttyin, TCSANOW, &terminnew);
#else
/*** NOTE: CHANGED FROM TCSETAW BECAUSE AIX WILL HANG IF SIGPOLL OCCURS AT SAME TIME ***/
				ioctl(ttyin, TCSETA, &terminnew);
#endif
			}
		}
		else if (terminnew.c_cc[VMIN] != 1) {
			terminnew.c_cc[VMIN] = 1;
			terminnew.c_cc[VTIME] = 0;
#if defined(USE_POSIX_TERMINAL_IO)
			tcsetattr(ttyin, TCSANOW, &terminnew);
#else
/*** NOTE: CHANGED FROM TCSETAW BECAUSE AIX WILL HANG IF SIGPOLL OCCURS AT SAME TIME ***/
			ioctl(ttyin, TCSETA, &terminnew);
#endif
		}
	}
	else if (waitflg > 0) {
		oldalarm = alarm(0);
		/*
		timerclear(&itimer.it_value);
		timerclear(&itimer.it_interval);
		setitimer(ITIMER_REAL, &itimer, &oldtimer);*/

		act.sa_handler = sigevent;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
#ifdef SA_INTERRUPT
		act.sa_flags |= SA_INTERRUPT;
#endif
		sigerr = sigaction(SIGALRM, &act, &oact);
		alrmhndl = ttyin;
		if (sigerr != -1) {
			alarm(1);
			/*
			timerclear(&itimer.it_value);
			timerclear(&itimer.it_interval);
			itimer.it_value.tv_sec = 1;
			setitimer(ITIMER_REAL, &itimer, &dummy_timer);*/
		}
/*** RACE CONDITION BETWEEN ALARM AND LOADING ALRMHNDL INTO REGISTER ***/
/*** LONGJMP/SIGLONGJMP CAN FIX THIS BUT IT WILL NOT WORK CORRECTLY UNDER ULTRIX ***/
		cnt = read(alrmhndl, (CHAR *) &buf[bufcnt], sizeof(buf) - bufcnt);
		if (sigerr != -1) {
			slept = 1 - alarm(0);
			/*
			timerclear(&itimer.it_value);
			timerclear(&itimer.it_interval);
			setitimer(ITIMER_REAL, &itimer, &slept_timer);
			slept_timer.it_value.tv_sec = 1 - slept_timer.it_value.tv_sec;*/
			sigaction(SIGALRM, &oact, NULL);
		}
		else {
			slept = 0;
			/*slept_timer.it_value.tv_sec = 0;*/
		}
		if (oldalarm > 0) {
			oldalarm = (oldalarm > slept) ? oldalarm - slept : 1;
			alarm(oldalarm);
		}
		/*
		if (oldtimer.it_value.tv_sec > 0) {
			if (oldtimer.it_value.tv_sec > slept_timer.it_value.tv_sec) oldtimer.it_value.tv_sec = oldtimer.it_value.tv_sec - slept_timer.it_value.tv_sec;
			else oldtimer.it_value.tv_sec = 1;
			setitimer(ITIMER_REAL, &oldtimer, &dummy_timer);
		}*/
		if (cnt > 0) {
			bufcnt += cnt;
		}
		return 0;  /* being called recursively, return */
	}

	if (ttyinflg) {
		cnt = read(ttyin, (CHAR *) &buf[bufcnt], sizeof(buf) - bufcnt);
	}
	else {  /* read from file or pipe */
		cnt = read(ttyin, (CHAR *) &buf[bufcnt], 1);
		if (!cnt) return 1;
	}
	if (cnt <= 0) {
		if (!bufcnt) return cnt;
		cnt = 0;
	}
	bufcnt += cnt;
	if (waitflg > 0) return 0;  /* return if being called recursively */

readtty1:
	keytab = *keytabptr;
	for (cnt = 0; cnt < bufcnt && cnt + multimax <= (INT) sizeof(buf); cnt += len) {
		if (multichr[buf[cnt]] != -1) {
			timeoutflg = FALSE;
			for (key = multichr[buf[cnt]]; ; key += keytab[key] + 3) {
				len = 1;
				if (!keytab[key] || buf[cnt] != keytab[key + 1]) {
					key = buf[cnt];
					break;
				}
				if (timeoutflg && (INT)(cnt + keytab[key]) > bufcnt) continue;
readtty2:
				for ( ; len < (INT) keytab[key] && cnt + len < bufcnt && buf[cnt + len] == keytab[key + len + 1]; len++);
				if (len == keytab[key]) {
					key = llhh(&keytab[key + len + 1]);
					break;
				}
				if (cnt + len < bufcnt || timeoutflg) continue;
				oldcnt = bufcnt;
				readtty(interchartime);		/* dbcdx.keyin.ict */
				if (bufcnt > oldcnt) goto readtty2;
				timeoutflg = TRUE;
			}
		}
		else {
			key = buf[cnt];
			len = 1;
		}
		if (key == 1000) {
			len += 3;			/* point past button indicator and coordinates */
			switch (buf[cnt + len - 3]) {
				case ' ':		/* left */
					key *= 10;
					break;
				case '!':		/* middle */
					key *= 30;
					break;
				case '"':		/* right */
					key *= 50;
					break;
				default:
					continue;		/* discard this mouse message, probably a button up message */
			}
			x = buf[cnt + len - 2] - 040;
			y = buf[cnt + len - 1] - 040;
			key += 200 * (y - 1) + x;
		}
/*** CODE: MABE MAKE CALLBACK FUNCTION RETURN ERROR IF OVERFLOW AND BREAK ***/
		if (vidcharcallbackfnc(key) == -1) break;
	}

	if (cnt < bufcnt) memmove(buf, &buf[cnt], bufcnt - cnt);
	bufcnt -= cnt;
	return (bufcnt) ? -1 : 0;
}

static void sigevent(INT sig)
{
#ifdef SIGTSTP
	INT h, v, type;
	struct sigaction act, oldact;
	sigset_t mask;
#endif

	switch (sig) {
		case SIGALRM:
			alrmhndl = -1;
			break;
		case SIGINT:
			evtset(breakeventid);
			break;
#ifdef SIGTSTP
		case SIGTSTP:
			/* position at bottom of screen and reset terminal */
			h = cursorh;
			v = cursorv;
			vidaposition(0, maxbot, lastattr);
			type = cursortype;
			vidacursor(CURSOR_ULINE, lastattr);
			vidasuspend(lastattr);

			/* unblock stop signal and set to default */
			sigemptyset(&mask);
			sigaddset(&mask, SIGTSTP);
			sigprocmask(SIG_UNBLOCK, &mask, NULL);
			act.sa_handler = SIG_DFL;
			sigemptyset(&act.sa_mask);
			act.sa_flags = 0;
			sigaction(SIGTSTP, &act, &oldact);
			/* suspend ourselves */
			kill(getpid(), SIGTSTP);

			/* restore stop signal */
			sigaction(SIGTSTP, &oldact, NULL);

			/* set terminal and restore screen */
			vidaresume();
			vidacursor(CURSOR_NONE, lastattr);
			for (cursorh = cursorv = 0; cursorv <= maxbot; cursorv++) textput(textwidth);
			vidaposition(h, v, lastattr);
			vidacursor(type, lastattr);
			vidaflush();
			break;
#endif
	}
}
