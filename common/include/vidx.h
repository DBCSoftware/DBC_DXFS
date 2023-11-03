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

#define KEYBUFMAX 2048
#define OUTBUFSIZE 1024

/* do not change the order of roll defines */
#define ROLL_UP			1
#define ROLL_DOWN		2
#define ROLL_LEFT		3
#define ROLL_RIGHT		4

#define DSPATTR_UNDERLINE	0x00010000
#define DSPATTR_BLINK		0x00020000
#define DSPATTR_BOLD		0x00040000
#define DSPATTR_DIM			0x00080000
#define DSPATTR_REV			0x00100000
#define DSPATTR_GRAPHIC		0x00200000
#define DSPATTR_DISPDOWN	0x01000000
#define DSPATTR_DISPLEFT	0x02000000
#define DSPATTR_ROLL        0x04000000
#define DSPATTR_RAWMODE		0x10000000
#define DSPATTR_AUXPORT		0x20000000
#define DSPATTR_KYDSPIPE	0x40000000
//#define DSPATTR_FGCOLORMASK	0x00000F00
//#define DSPATTR_BGCOLORMASK	0x0000F000
#define DSPATTR_ATTRIBMASK	0x003F0000
//#define DSPATTR_FGCOLORSHIFT	8
//#define DSPATTR_BGCOLORSHIFT	12
#define DSPATTR_ATTRIBSHIFT	16

#if OS_WIN32
#define DSPATTR_BLACK		0x0000
#define DSPATTR_BLUE		0x0001
#define DSPATTR_GREEN		0x0002
#define DSPATTR_CYAN		0x0003
#define DSPATTR_RED			0x0004
#define DSPATTR_MAGENTA		0x0005
#define DSPATTR_YELLOW		0x0006
#define DSPATTR_WHITE		0x0007
#define DSPATTR_INTENSE		0x0008
#else
#define DSPATTR_BLACK		0x0000LL
#define DSPATTR_BLUE		0x0001LL
#define DSPATTR_GREEN		0x0002LL
#define DSPATTR_CYAN		0x0003LL
#define DSPATTR_RED			0x0004LL
#define DSPATTR_MAGENTA		0x0005LL
#define DSPATTR_YELLOW		0x0006LL
#define DSPATTR_WHITE		0x0007LL
#define DSPATTR_INTENSE		0x0008LL
#endif

#define CURSOR_NONE			(UCHAR) 0
#define CURSOR_BLOCK		(UCHAR) 1
#define CURSOR_ULINE		(UCHAR) 2
#define CURSOR_HALF			(UCHAR) 3

#define VIDERR_NOMEMRY		(-101) /* unable to allocate memory */
#define VIDERR_NOTRMDF		(-102) /* operating system does not support termdef file */
#define VIDERR_NOINDEV		(-103) /* operating system does not support stdin redirection */
#define VIDERR_NOOUTDEV		(-104) /* operating system does not support stdout redirection */
#define VIDERR_OPTRMDF		(-105) /* unable to open termdef file */
#define VIDERR_INVTRMDF		(-106) /* invalid termdef file */
#define VIDERR_OPINDEV		(-107) /* unable to open input device */
#define VIDERR_OPOUTDEV		(-108) /* unable to open output device */

/* display tables */
#define DSP_INIT		0
#define DSP_TERM		1
#define DSP_SCREEN		2
#define DSP_EDIT		3
#define DSP_WIN			4
#define DSP_FG_COLOR	5
#define DSP_BG_COLOR	6
#define DSP_GRAPH		7
#define DSP_SDSP		8
#define DSP_MAX			9

/* initialization controls */
#define DSP_INITIALIZE	0
#define DSP_INITMAX		1

/* terminate controls */
#define DSP_TERMINATE	0
#define DSP_TERMMAX		1

/* basic screen controls */
#define DSP_POSITION_CURSOR		0
#define DSP_POSITION_CURSOR_HORZ	1
#define DSP_POSITION_CURSOR_VERT	2
#define DSP_CLEAR_SCREEN			3
#define DSP_CLEAR_TO_SCREEN_END	4
#define DSP_CLEAR_TO_LINE_END		5
#define DSP_CURSOR_ON			6
#define DSP_CURSOR_OFF			7
#define DSP_CURSOR_BLOCK			8
#define DSP_CURSOR_UNDERLINE		9
#define DSP_REVERSE_ON			10
#define DSP_REVERSE_OFF			11
#define DSP_UNDERLINE_ON			12
#define DSP_UNDERLINE_OFF		13
#define DSP_BLINK_ON			14
#define DSP_BLINK_OFF			15
#define DSP_BOLD_ON				16
#define DSP_BOLD_OFF			17
#define DSP_DIM_ON				18
#define DSP_DIM_OFF				19
#define DSP_RIGHT_TO_LEFT_DSP_ON	20
#define DSP_RIGHT_TO_LEFT_DSP_OFF	21
#define DSP_DISPLAY_DOWN_ON		22
#define DSP_DISPLAY_DOWN_OFF		23
#define DSP_ALL_OFF				24
#define DSP_BELL				25
#define DSP_AUXPORT_ON			26
#define DSP_AUXPORT_OFF			27
#define DSP_CURSOR_HALF			28
// Two new things here for ansi256 mode May 2015 jpr
#define DSP_FGCOLOR_ANSI256		29
#define DSP_BGCOLOR_ANSI256		30
#define DSP_SCREENMAX			31

/* editing screen controls */
#define DSP_INSERT_CHAR		0
#define DSP_DELETE_CHAR		1
#define DSP_INSERT_LINE		2
#define DSP_DELETE_LINE		3
#define DSP_OPEN_LINE		4
#define DSP_CLOSE_LINE		5
#define DSP_EDITMAX			6

/* window screen controls */
#define DSP_SCROLL_REGION_TB		0
#define DSP_SCROLL_REGION_LR		1
#define DSP_SCROLL_UP			2
#define DSP_SCROLL_DOWN			3
#define DSP_SCROLL_LEFT			4
#define DSP_SCROLL_RIGHT			5
#define DSP_SCROLL_WIN_UP		6
#define DSP_SCROLL_WIN_DOWN		7
#define DSP_SCROLL_WIN_LEFT		8
#define DSP_SCROLL_WIN_RIGHT		9
#define DSP_ERASE_WIN			10
#define DSP_ENABLE_ROLL			11
#define DSP_DISABLE_ROLL			12
#define DSP_WINMAX				13

/* foreground color display sequences */
//#define DSP_FG_BLACK		0
//#define DSP_FG_BLUE			1
//#define DSP_FG_GREEN		2
//#define DSP_FG_CYAN			3
//#define DSP_FG_RED			4
//#define DSP_FG_MAGENTA		5
//#define DSP_FG_YELLOW		6
//#define DSP_FG_WHITE		7
//#define DSP_FG_COLORMAX		16

/* background color display sequences */
//#define DSP_BG_BLACK		0
//#define DSP_BG_BLUE			1
//#define DSP_BG_GREEN		2
//#define DSP_BG_CYAN			3
//#define DSP_BG_RED			4
//#define DSP_BG_MAGENTA		5
//#define DSP_BG_YELLOW		6
//#define DSP_BG_WHITE		7
//#define DSP_BG_COLORMAX		16

/* graphic display sequences */
#if OS_UNIX
#define DSP_GRAPHIC_ON				0
#define DSP_GRAPHIC_OFF				1
#define DSP_GRAPHIC_UP_ARROW			2
#define DSP_GRAPHIC_RIGHT_ARROW		3
#define DSP_GRAPHIC_LEFT_ARROW		4
#define DSP_GRAPHIC_DOWN_ARROW		5
#define DSP_GRAPHIC_HORZ_LINE			6
#define DSP_GRAPHIC_VERT_LINE			7
#define DSP_GRAPHIC_CROSS			8
#define DSP_GRAPHIC_UPPER_LEFT_CORNER	9
#define DSP_GRAPHIC_UPPER_RIGHT_CORNER	10
#define DSP_GRAPHIC_LOWER_LEFT_CORNER	11
#define DSP_GRAPHIC_LOWER_RIGHT_CORNER	12
#define DSP_GRAPHIC_RIGHT_TICK		13
#define DSP_GRAPHIC_DOWN_TICK			14
#define DSP_GRAPHIC_LEFT_TICK			15
#define DSP_GRAPHIC_UP_TICK			16
#define DSP_GRAPHMAX				50
#endif

/* special display sequences */
#define DSP_SDSPMAX			256

/* keyin tables */
#define KBD_FNC		0
#define KBD_SHIFTFNC	1
#define KBD_CTRLFNC		2
#define KBD_ALTFNC		3
#define KBD_ALTCHR		4
#define KBD_MISC		5
#define KBD_SKEY		6
#define KBD_SXKEY		7
#define KBD_MAX		8

/* keyin maximums */
#define KBD_FNCMAX		30
#define KBD_SHIFTFNCMAX	30
#define KBD_CTRLFNCMAX	30
#define KBD_ALTFNCMAX	30
#define KBD_ALTCHRMAX	26
#define KBD_MISCMAX		5
#define KBD_SKEYMAX		256
#define KBD_SXKEYMAX	256

extern INT vidastart(INT, INT, CHAR *, CHAR *, CHAR *, CHAR *, INT *, INT *, INT (*)(INT), INT);
extern void vidaexit(void);
extern void vidaresume(void);
#if OS_UNIX
extern void vidacursor(INT, UINT64);
extern void vidaposition(INT, INT, UINT64);
extern void vidadeletechar(INT, INT, INT, INT, INT, INT, UINT64);
extern void vidaputstring(UCHAR *, INT, UINT64);
extern void vidamove(INT, INT, INT, INT, INT, UINT64);
extern void vidaeraserect(INT, INT, INT, INT, UINT64);
extern void vidainsertchar(INT, INT, INT, INT, INT, INT, UINT64);
extern void vidaputrect(UCHAR *, INT, INT, INT, INT, INT, UINT64);
extern void vidasound(INT, INT, UINT64, INT);
extern void vidasuspend(UINT64);
#else
extern void vidacursor(INT, INT);
extern void vidaposition(INT, INT, INT);
extern void vidadeletechar(INT, INT, INT, INT, INT, INT, INT);
extern void vidaputstring(UCHAR *, INT, INT);
extern void vidamove(INT, INT, INT, INT, INT, INT);
extern void vidaeraserect(INT, INT, INT, INT, INT);
extern void vidainsertchar(INT, INT, INT, INT, INT, INT, INT);
extern void vidaputrect(UCHAR *, INT, INT, INT, INT, INT, INT);
extern void vidasound(INT, INT, INT, INT);
extern void vidasuspend(INT);
#endif
extern void vidaflush(void);
extern void vidagetrect(UCHAR *, INT, INT, INT, INT, INT);
extern INT vidainterface(UCHAR *, INT *);
extern void setflag(INT);
