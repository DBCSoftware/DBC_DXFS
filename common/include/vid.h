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

#ifndef _VID_INCLUDED
#define _VID_INCLUDED

#ifdef _MSC_VER
#  define snprintf _snprintf
#endif

struct vidparmsstruct {
	INT flag;				/* or of VID_FLAG_ bits, see below */
	INT numcolumns;		/* number of columns on display; 0 = use default */
	INT numlines;			/* number of lines on display; 0 = use default */
	INT interchartime;	/* tenths of a second to wait after the 1st char of a multi char sequence, unix only */
	CHAR *termdef;			/* pointer to termdef filename or NULL */
	CHAR *indevice;		/* pointer to input device name or NULL */
	CHAR *outdevice;		/* pointer to output device name or NULL */
	CHAR *fontname;		/* pointer to font name or NULL */
	CHAR *keymap;			/* pointer to keymap filename or NULL */
	CHAR *ukeymap;			/* pointer to upper case keymap filename or NULL */
	CHAR *lkeymap;			/* pointer to lower case keymap filename or NULL */
};

typedef struct vidparmsstruct VIDPARMS;

/* vidput defines */
#define VID_FINISH		0x00010000
#define VID_END_FLUSH	0x00020000
#define VID_END_NOFLUSH	0x00030000
#define VID_DISPLAY		0x00040000
#define VID_REPEAT		0x00050000
#define VID_DISP_CHAR	0x00060000
#define VID_DISP_SYM	0x00070000
#define VID_DSP_BLANKS	0x00080000
#define VID_HORZ		0x00090000
#define VID_VERT		0x000A0000
#define VID_HORZ_ADJ	0x000B0000
#define VID_VERT_ADJ	0x000C0000
#define VID_WIN_TOP		0x000D0000
#define VID_WIN_BOTTOM	0x000E0000
#define VID_WIN_LEFT	0x000F0000
#define VID_WIN_RIGHT	0x00100000
#define VID_WIN_RESET	0x00110000

#define VID_REV_ON		0x01000000
#define VID_REV_OFF		0x01010000
#define VID_UL_ON		0x01020000
#define VID_UL_OFF		0x01030000
#define VID_BLINK_ON	0x01040000
#define VID_BLINK_OFF	0x01050000
#define VID_BOLD_ON		0x01060000
#define VID_BOLD_OFF	0x01070000
#define VID_DIM_ON		0x01080000
#define VID_DIM_OFF		0x01090000
#define VID_RL_ON  	 	0x010A0000
#define VID_RL_OFF	 	0x010B0000
#define VID_DSPDOWN_ON	0x010C0000
#define VID_DSPDOWN_OFF	0x010D0000
#define VID_NL_ROLL_ON	0x010E0000
#define VID_NL_ROLL_OFF	0x010F0000
#define VID_RAW_ON		0x01100000
#define VID_RAW_OFF		0x01110000
#define VID_PRT_ON		0x01120000
#define VID_PRT_OFF		0x01130000
#define VID_CURSOR_ON	0x01140000
#define VID_CURSOR_OFF	0x01150000
#define VID_CURSOR_NORM	0x01160000
#define VID_CURSOR_ULINE	0x01170000
#define VID_CURSOR_HALF	0x01180000
#define VID_CURSOR_BLOCK	0x01190000
#define VID_HDBL_ON		0x011A0000
#define VID_HDBL_OFF	0x011B0000
#define VID_VDBL_ON		0x011C0000
#define VID_VDBL_OFF	0x011D0000
#define VID_FOREGROUND	0x011E0000
#define VID_BACKGROUND	0x011F0000

#define VID_KBD_RESET	0x01300000
#define VID_KBD_DE		0x01310000
#define VID_KBD_JR		0x01320000
#define VID_KBD_JL		0x01330000
#define VID_KBD_ZF		0x01340000
#define VID_KBD_EDIT	0x01350000
#define VID_KBD_UC		0x01360000
#define VID_KBD_LC		0x01370000
#define VID_KBD_IC		0x01380000
#define VID_KBD_ESON	0x01390000
#define VID_KBD_ESOFF	0x013A0000
#define VID_KBD_EON		0x013B0000
#define VID_KBD_EOFF	0x013C0000
#define VID_KBD_KCON	0x013D0000
#define VID_KBD_KCOFF	0x013E0000
#define VID_KBD_ED_ON	0x013F0000
#define VID_KBD_ED_OFF	0x01400000
#define VID_KBD_CK_ON	0x01410000
#define VID_KBD_CK_OFF	0x01420000
#define VID_KBD_OVS_ON	0x01430000
#define VID_KBD_OVS_OFF	0x01440000
#define VID_KBD_IN		0x01450000
#define VID_KBD_ECHOCHR	0x01460000
#define VID_KBD_TIMEOUT	0x01470000
#define VID_KBD_F_CNCL1	0x01480000
#define VID_KBD_F_CNCL2	0x01490000
#define VID_KBD_F_INTR	0x014A0000

#define VID_ES			0x02000000
#define VID_EF			0x02010000
#define VID_EL			0x02020000
#define VID_RU			0x02030000
#define VID_RD			0x02040000
#define VID_RL			0x02050000
#define VID_RR			0x02060000
#define VID_NL			0x02070000
#define VID_LF			0x02080000
#define VID_HU			0x02090000
#define VID_HD			0x020A0000
#define VID_EU			0x020B0000
#define VID_ED			0x020C0000
#define VID_OL			0x020D0000
#define VID_CL			0x020E0000
#define VID_IL			0x020F0000
#define VID_DL			0x02100000
#define VID_INSCHAR		0x02110000
#define VID_DELCHAR		0x02120000
#define VID_BEEP		0x02130000
#define VID_SOUND		0x02140000
#define VID_DROPSW		0x02150000
#define VID_CR			0x02160000

/* Graphics characters */
#define VID_SYM_HLN		0x0000
#define VID_SYM_VLN		0x0001
#define VID_SYM_CRS		0x0002
#define VID_SYM_ULC		0x0003
#define VID_SYM_URC		0x0004
#define VID_SYM_LLC		0x0005
#define VID_SYM_LRC		0x0006
#define VID_SYM_RTK		0x0007
#define VID_SYM_DTK		0x0008
#define VID_SYM_LTK		0x0009
#define VID_SYM_UTK		0x000A
#define VID_SYM_UPA		0x000B
#define VID_SYM_RTA		0x000C
#define VID_SYM_DNA		0x000D
#define VID_SYM_LFA		0x000E

/* flag values for vidparms structure */
#define VID_FLAG_KYDSPIPE	0x00000001
#define VID_FLAG_EDITMODE	0x00000002
#define VID_FLAG_EDITSPECIAL	0x00000004
#if OS_WIN32
#define VID_FLAG_IKEYS		0x00000100
#define VID_FLAG_NOCON		0x00000200
#define VID_FLAG_OLDBEEP	0x00000400
#define VID_FLAG_SHAKEON	0x00000800
#define VID_FLAG_CTSRTS		0x00001000
#define VID_FLAG_NOCLOSE    0x00002000
#endif
#if OS_UNIX
#define VID_FLAG_XOFF		0x00000100
#define VID_FLAG_XON		0x00000200
#define VID_FLAG_USEMOUSE  0x00000400
#endif

// New VID_FLAG_* for character mode color modes
// As of 21MAY2015  for DX 16.3 (??) jpr
#define VID_FLAG_COLORMODE_OLD      0x00010000
#define VID_FLAG_COLORMODE_RGB      0x00020000
#define VID_FLAG_COLORMODE_ANSI256  0x00040000


/* flag values for vidkeyinstart */
#define VID_KEYIN_NUMERIC	0x01
#define VID_KEYIN_COMMA		0x02

/* defines for key values */
#define VID_ENTER       256  /* enter */
#define VID_ESCAPE      257  /* escape */
#define VID_BKSPC       258  /* backspace */
#define VID_TAB         259  /* tab */
#define VID_BKTAB       260  /* back tab */

#define VID_UP          261  /* up arrow */
#define VID_DOWN        262  /* down arrow */
#define VID_LEFT        263  /* left arrow */
#define VID_RIGHT       264  /* right arrow */
#define VID_INSERT      265  /* insert */
#define VID_DELETE      266  /* delete */
#define VID_HOME        267  /* home */
#define VID_END         268  /* end */
#define VID_PGUP        269  /* page up */
#define VID_PGDN        270  /* page down */

#define VID_SHIFTUP     271  /* shift-up arrow */
#define VID_SHIFTDOWN   272  /* shift-down arrow */
#define VID_SHIFTLEFT   273  /* shift-left arrow */
#define VID_SHIFTRIGHT  274  /* shift-right arrow */
#define VID_SHIFTINSERT 275  /* shift-insert */
#define VID_SHIFTDELETE 276  /* shift-delete */
#define VID_SHIFTHOME   277  /* shift-home */
#define VID_SHIFTEND    278  /* shift-end */
#define VID_SHIFTPGUP   279  /* shift-page up */
#define VID_SHIFTPGDN   280  /* shift-page down */

#define VID_CTLUP       281  /* control-up arrow */
#define VID_CTLDOWN     282  /* control-down arrow */
#define VID_CTLLEFT     283  /* control-left arrow */
#define VID_CTLRIGHT    284  /* control-right arrow */
#define VID_CTLINSERT   285  /* control-insert */
#define VID_CTLDELETE   286  /* control-insert */
#define VID_CTLHOME     287  /* control-home */
#define VID_CTLEND      288  /* control-end */
#define VID_CTLPGUP     289  /* control-page up */
#define VID_CTLPGDN     290  /* control-page down */

#define VID_ALTUP       291  /* alt-up arrow */
#define VID_ALTDOWN     292  /* alt-down arrow */
#define VID_ALTLEFT     293  /* alt-left arrow */
#define VID_ALTRIGHT    294  /* alt-right arrow */
#define VID_ALTINSERT   295  /* alt-insert */
#define VID_ALTDELETE   296  /* alt-insert */
#define VID_ALTHOME     297  /* alt-home */
#define VID_ALTEND      298  /* alt-end */
#define VID_ALTPGUP     299  /* alt-page up */
#define VID_ALTPGDN     300  /* alt-page down */

#define VID_F1          301  /* F1 */
#define VID_F2          302  /* F2 */
#define VID_F3          303  /* F3 */
#define VID_F4          304  /* F4 */
#define VID_F5          305  /* F5 */
#define VID_F6          306  /* F6 */
#define VID_F7          307  /* F7 */
#define VID_F8          308  /* F8 */
#define VID_F9          309  /* F9 */
#define VID_F10         310  /* F10 */
#define VID_F11         311  /* F11 */
#define VID_F12         312  /* F12 */
#define VID_F13         313  /* F13 */
#define VID_F14         314  /* F14 */
#define VID_F15         315  /* F15 */
#define VID_F16         316  /* F16 */
#define VID_F17         317  /* F17 */
#define VID_F18         318  /* F18 */
#define VID_F19         319  /* F19 */
#define VID_F20         320  /* F20 */

#define VID_SHIFTF1     321  /* shift-F1 */
#define VID_SHIFTF2     322  /* shift-F2 */
#define VID_SHIFTF3     323  /* shift-F3 */
#define VID_SHIFTF4     324  /* shift-F4 */
#define VID_SHIFTF5     325  /* shift-F5 */
#define VID_SHIFTF6     326  /* shift-F6 */
#define VID_SHIFTF7     327  /* shift-F7 */
#define VID_SHIFTF8     328  /* shift-F8 */
#define VID_SHIFTF9     329  /* shift-F9 */
#define VID_SHIFTF10    330  /* shift-F10 */
#define VID_SHIFTF11    331  /* shift-F11 */
#define VID_SHIFTF12    332  /* shift-F12 */
#define VID_SHIFTF13    333  /* shift-F13 */
#define VID_SHIFTF14    334  /* shift-F14 */
#define VID_SHIFTF15    335  /* shift-F15 */
#define VID_SHIFTF16    336  /* shift-F16 */
#define VID_SHIFTF17    337  /* shift-F17 */
#define VID_SHIFTF18    338  /* shift-F18 */
#define VID_SHIFTF19    339  /* shift-F19 */
#define VID_SHIFTF20    340  /* shift-F20 */

#define VID_CTLF1       341  /* control-F1 */
#define VID_CTLF2       342  /* control-F2 */
#define VID_CTLF3       343  /* control-F3 */
#define VID_CTLF4       344  /* control-F4 */
#define VID_CTLF5       345  /* control-F5 */
#define VID_CTLF6       346  /* control-F6 */
#define VID_CTLF7       347  /* control-F7 */
#define VID_CTLF8       348  /* control-F8 */
#define VID_CTLF9       349  /* control-F9 */
#define VID_CTLF10      350  /* control-F10 */
#define VID_CTLF11      351  /* control-F11 */
#define VID_CTLF12      352  /* control-F12 */
#define VID_CTLF13      353  /* control-F13 */
#define VID_CTLF14      354  /* control-F14 */
#define VID_CTLF15      355  /* control-F15 */
#define VID_CTLF16      356  /* control-F16 */
#define VID_CTLF17      357  /* control-F17 */
#define VID_CTLF18      358  /* control-F18 */
#define VID_CTLF19      359  /* control-F19 */
#define VID_CTLF20      360  /* control-F20 */

#define VID_ALTF1       361  /* alt-F1 */
#define VID_ALTF2       362  /* alt-F1 */
#define VID_ALTF3       363  /* alt-F1 */
#define VID_ALTF4       364  /* alt-F1 */
#define VID_ALTF5       365  /* alt-F1 */
#define VID_ALTF6       366  /* alt-F1 */
#define VID_ALTF7       367  /* alt-F1 */
#define VID_ALTF8       368  /* alt-F1 */
#define VID_ALTF9       369  /* alt-F1 */
#define VID_ALTF10      370  /* alt-F1 */
#define VID_ALTF11      371  /* alt-F1 */
#define VID_ALTF12      372  /* alt-F1 */
#define VID_ALTF13      373  /* alt-F1 */
#define VID_ALTF14      374  /* alt-F1 */
#define VID_ALTF15      375  /* alt-F1 */
#define VID_ALTF16      376  /* alt-F1 */
#define VID_ALTF17      377  /* alt-F1 */
#define VID_ALTF18      378  /* alt-F1 */
#define VID_ALTF19      379  /* alt-F1 */
#define VID_ALTF20      380  /* alt-F20 */

#define VID_ALTA        381  /* alt-A */
#define VID_ALTB        382  /* alt-B */
#define VID_ALTC        383  /* alt-C */
#define VID_ALTD        384  /* alt-D */
#define VID_ALTE        385  /* alt-E */
#define VID_ALTF        386  /* alt-F */
#define VID_ALTG        387  /* alt-G */
#define VID_ALTH        388  /* alt-H */
#define VID_ALTI        389  /* alt-I */
#define VID_ALTJ        390  /* alt-J */
#define VID_ALTK        391  /* alt-K */
#define VID_ALTL        392  /* alt-L */
#define VID_ALTM        393  /* alt-M */
#define VID_ALTN        394  /* alt-N */
#define VID_ALTO        395  /* alt-O */
#define VID_ALTP        396  /* alt-P */
#define VID_ALTQ        397  /* alt-Q */
#define VID_ALTR        398  /* alt-R */
#define VID_ALTS        399  /* alt-S */
#define VID_ALTT        400  /* alt-T */
#define VID_ALTU        401  /* alt-U */
#define VID_ALTV        402  /* alt-V */
#define VID_ALTW        403  /* alt-W */
#define VID_ALTX        404  /* alt-X */
#define VID_ALTY        405  /* alt-Y */
#define VID_ALTZ        406  /* alt-Z */

#define VID_INTERRUPT   505
#define VID_CANCEL      506

#define VID_MAXKEYVAL   506

// Structure to normalize STATE saves
struct _state_save {
	USHORT wsclft;
	USHORT wsctop;
	USHORT wscrgt;
	USHORT wscbot;
	USHORT vidh;
	USHORT vidv;
#if OS_UNIX
	UINT64 dspattr;
#else
	INT dspattr;
#endif
	UCHAR viddblflag;
	UCHAR vidcurmode;
	UCHAR vidcurtype;
	UCHAR keyinechochar;
	INT kynattr;
	UCHAR validmap[(VID_MAXKEYVAL >> 3) + 1];
	UCHAR finishmap[(VID_MAXKEYVAL >> 3) + 1];
};
typedef struct _state_save STATESAVE;

#define VIDPUTERROR_NEW		0x00
#define VIDPUTERROR_APPEND	0x01
#define VIDPUTERROR_INSERT	0x02

#ifdef EXTERN
#undef EXTERN
#endif
#ifdef VIDMAIN
#define EXTERN
#else
#define EXTERN extern
#endif
#if OS_UNIX
EXTERN UINT64 DSPATTR_FGCOLORMASK;
EXTERN UINT64 DSPATTR_BGCOLORMASK;
EXTERN UINT64 dspattr;			/* current vid attributes UNIX */
#else
EXTERN INT DSPATTR_FGCOLORMASK;
EXTERN INT DSPATTR_BGCOLORMASK;
EXTERN INT32 dspattr;			/* current vid attributes Windows */
#endif
EXTERN INT CELLWIDTH;
EXTERN INT DSP_FG_COLORMAX;
EXTERN INT DSP_BG_COLORMAX;
EXTERN INT DSPATTR_FGCOLORSHIFT;
EXTERN INT DSPATTR_BGCOLORSHIFT;
EXTERN INT wsclft, wscrgt;		/* window left, right boundaries */
EXTERN INT wsctop, wscbot;		/* window top, bottom boundaries */
/* Color values */
EXTERN INT VID_BLACK;
EXTERN INT VID_BLUE;
EXTERN INT VID_GREEN;
EXTERN INT VID_CYAN;
EXTERN INT VID_RED;
EXTERN INT VID_MAGENTA;
EXTERN INT VID_YELLOW;
EXTERN INT VID_WHITE;


extern INT vidinit(VIDPARMS *, INT);
extern void videxit(void);
extern void vidsuspend(INT);
extern void vidresume(void);
extern void vidreset(void);
extern INT vidput(INT32 *);
extern void vidgetsize(INT *, INT *);
extern void vidgetpos(INT *, INT *);
extern void vidgetwin(INT *, INT *, INT *, INT *);
extern INT vidwinsize(void);
extern INT vidscreensize(void);
extern INT vidstatesize(void);
extern INT vidsavechar(UCHAR *, INT *);
extern void vidrestorechar(UCHAR *, INT);
extern INT vidsavewin(UCHAR *, INT *);
extern INT vidrestorewin(UCHAR *, INT);
extern INT vidsavestate(UCHAR *, INT *);
extern INT vidrestorestate(UCHAR *, INT);
extern INT vidsavescreen(UCHAR *, INT *);
extern INT vidrestorescreen(UCHAR *, INT);
extern void vidtrapstart(UCHAR *, INT);
extern INT vidtrapend(INT *);
extern INT vidtrapget(INT *);
extern void vidkeyingetfinishmap(UCHAR *);
extern void vidkeyinfinishmap(UCHAR *);
extern void vidkeyinvalidmap(UCHAR *);
extern INT vidkeyinstart(UCHAR *, INT, INT, INT, INT, INT);
extern INT vidkeyinend(UCHAR *, INT *, INT *);
extern void vidkeyinabort(void);
extern void vidkeyinclear(void);
extern INT vidgeterror(CHAR *errorstr, INT size);
extern INT vidputerror(CHAR *errorstr, INT type);

extern void kdsConfigColorMode(INT *flag);
extern void kdsConfigColorModeSC(ELEMENT *e1, INT *flag);
//extern void termParmPutHelper(INT len, INT *outcnt, UCHAR *buf, UCHAR *outbuf,
//		INT p1, INT p2, INT p3, INT p4, INT p5, INT p6);

#endif  /* _VID_INCLUDED */
