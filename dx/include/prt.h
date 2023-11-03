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

#ifndef _PRT_INCLUDED
#define _PRT_INCLUDED

/**
 * Config entry dbcdx.print.textrotate=[old | new]
 */
#define TEXTROTATE "textrotate"

enum PRINT_24BITCOLOR_FORMAT {
	PRINT_24BITCOLOR_FORMAT_BGR,
	PRINT_24BITCOLOR_FORMAT_RGB
};

typedef struct prtinitinfo_struct {
	INT (*getserver)(CHAR *, INT);
	CHAR *translate;
	CHAR *program;
	CHAR *version;
	UINT pdfOldTabbing;
	UINT TextRotateOld;
	enum PRINT_24BITCOLOR_FORMAT colorFormat;
#if OS_UNIX
	INT locktype;
#endif
} PRTINITINFO;

#define anyUserSetPrintMargins(mgns) \
	(((mgns).margin != INVALID_PRINT_MARGIN) \
	|| ((mgns).top_margin != INVALID_PRINT_MARGIN) \
	|| ((mgns).bottom_margin != INVALID_PRINT_MARGIN) \
	|| ((mgns).left_margin != INVALID_PRINT_MARGIN) \
	|| ((mgns).right_margin != INVALID_PRINT_MARGIN) \
	)


typedef struct prtmargins_struct {
	DOUBLE margin;
	DOUBLE top_margin;
	DOUBLE bottom_margin;
	DOUBLE left_margin;
	DOUBLE right_margin;
} PRTMARGINS;


typedef struct prtoptions_struct {
	INT flags;
	INT copies;
	INT dpi;
	CHAR docname[128];
	CHAR paperbin[64];
	CHAR papersize[128];
	struct {
		CHAR prtname[128];
	} submit;
	PRTMARGINS margins;
} PRTOPTIONS;

/* flags used with getprintinfo */
#define GETPRINTINFO_PRINTERS	0x01
#define GETPRINTINFO_PAPERBINS	0x02
#define GETPRINTINFO_PAPERNAMES	0x03
#define GETPRINTINFO_MALLOC 4096

/* print init flags */
#if OS_UNIX
#define PRTLOCK_FILE	0x01
#define PRTLOCK_FCNTL	0x02
#endif

#if OS_WIN32
#ifdef PRT_C_
HGLOBAL prtnamehandle;	/* default print name handle */
HGLOBAL prtmodehandle;	/* default print mode handle */
#else
extern HGLOBAL prtnamehandle;	/* default print name handle */
extern HGLOBAL prtmodehandle;	/* default print mode handle */
#endif
#endif

#define PRTFLAG_SERVER		0x00010000
#define PRTFLAG_ALLOC		0x00020000
#define PRTFLAG_INIT		0x00040000
#define PRTFLAG_PAGE		0x00080000

/* print open flags */
#define PRT_TEST 0x00000001
#define PRT_WAIT 0x00000002

#define PRT_JDLG 0x00000010	/* display job dialog */
#define PRT_WDRV 0x00000020	/* print using windows device driver */
#define PRT_WRAW 0x00000040	/* print using windows raw mode */
#define PRT_PIPE 0x00000080	/* print destination is a pipe */

#define PRT_LCTL 0x00000100	/* device control characters printer language */
#define PRT_LPCL 0x00000200	/* PCL printer language */
#define PRT_LPSL 0x00000400	/* Postscript printer language */
#define PRT_LPDF 0x00000800 /* PDF printer language */

#define PRT_NORE 0x00001000	/* no reset on spool open (PCL only) */
#define PRT_LAND 0x00002000	/* landscape orientation (width is larger than height) */
#define PRT_PORT 0x00004000	/* portrait orientation (height is larger than width) */
#define PRT_DPLX 0x00008000	/* double sided printing */

#define PRT_NOEJ 0x00010000	/* suppress the trailing form feed */
#define PRT_COMP 0x00020000	/* compression is on for DB/C type file */
#define PRT_APPD 0x00040000	/* output is appended to the end of file */
#define PRT_NOEX 0x00080000	/* do not add .prt extension */

#define PRT_SBMT 0x00100000
#define PRT_BANR 0x00200000
#define PRT_REMV 0x00400000
#define PRT_CLNT 0x00800000 /* use smart client for printing */

/* print text flags */
#define PRT_TEXT_RIGHT	0x01
#define PRT_TEXT_CENTER	0x02
#define PRT_TEXT_ANGLE	0x04

#define PRTERR_INUSE	1
#define PRTERR_EXIST	2
#define PRTERR_OFFLINE	3
#define PRTERR_CANCEL	4
#define PRTERR_BADNAME	5
#define PRTERR_BADOPT	6
#define PRTERR_NOTOPEN	7
#define PRTERR_OPEN		8
#define PRTERR_CREATE	9
#define PRTERR_ACCESS	10
#define PRTERR_WRITE	11
#define PRTERR_NOSPACE	12
#define PRTERR_NOMEM	13
#define PRTERR_SERVER	14
#define PRTERR_DIALOG	15
#define PRTERR_NATIVE	16
#define PRTERR_BADTRANS	17
#define PRTERR_TOOLONG	18
#define PRTERR_CLIENT	19
#define PRTERR_SPLCLOSE 20
#define PRTERR_INTERNAL 21
#define PRTERR_UNSUPCLNT 22
#define PRTERR_MAXVALUE 22

#define WIDTH_LETTER ((DOUBLE) 8.5)
#define WIDTH_LEGAL ((DOUBLE) 8.5)
#define WIDTH_COMPUTER ((DOUBLE) 11.0)  /* Actually ANSI B 'Tabloid' */
#define WIDTH_A3 ((DOUBLE) 11.6929)		/* 297 mm */
#define WIDTH_A4 ((DOUBLE) 8.2677)		/* 210 mm */
#define WIDTH_B4 ((DOUBLE) 9.8425)		/* 250 mm */
#define WIDTH_B5 ((DOUBLE) 6.9291)		/* 176 mm */

#define HEIGHT_LETTER ((DOUBLE) 11.0)
#define HEIGHT_LEGAL ((DOUBLE) 14.0)
#define HEIGHT_COMPUTER ((DOUBLE) 17.0)
#define HEIGHT_A3 ((DOUBLE) 16.5354)	/* 420 mm */
#define HEIGHT_A4 ((DOUBLE) 11.6929)	/* 297 mm */
#define HEIGHT_B4 ((DOUBLE) 13.8976)	/* 353 mm */
#define HEIGHT_B5 ((DOUBLE) 9.8425)		/* 250 mm */

/*
 * In the default user space system, units are 1/72 of an inch.
 * This is modifiable in pdf version 1.6 and newer. But we don't
 * want to get into that.
 */
#define PDFUSERSPACEUNITS 72

#define INVALID_PRINT_MARGIN (-1.0)
/*
 * Default margins for PDF/PS in inches
 */
#define MARGIN_LEFT ((DOUBLE) 0.25)
#define MARGIN_RIGHT ((DOUBLE) 0.25)
#define MARGIN_TOP ((DOUBLE) 0.25)
#define MARGIN_BOTTOM ((DOUBLE) 0.25)
/*
 * Page horizontal margin in inches, total of both sides, i.e. 1/4 inch on each side
 */
#define MARGINX (MARGIN_LEFT + MARGIN_RIGHT)
/*
 * Page vertical margin in inches
 */
#define MARGINY (MARGIN_TOP + MARGIN_BOTTOM)

extern INT compress_rle(UCHAR *src, UCHAR *dst, INT src_size);
extern INT prtalloc(INT prtnum, INT type);
extern INT prtbigdot(INT prtnum, INT r);
extern INT prtbox(INT prtnum, INT x, INT y);
extern INT prtbufcopy(INT prtnum, const CHAR *string, INT len);
extern INT prtcircle(INT prtnum, INT r);
extern INT prtclose(INT prtnum, PRTOPTIONS *options);
extern INT prtcolor(INT prtnum, INT color);
extern INT prtcr(INT prtnum, INT cnt);
extern INT prtdbltoa(DOUBLE d1, CHAR *str, INT precision);
extern INT prtdefname(CHAR *prtname, INT size);
extern INT prtexit(void);
extern INT prtff(INT prtnum, INT cnt);
extern INT prtffbin(INT prtnum, CHAR *binname);
extern INT prtflush(INT prtnum);
extern INT prtflushbuf(INT prtnum);
extern INT prtflushline(INT prtnum, INT truncateflag);
extern INT prtflushlinepdf(INT prtnum, INT linecnt);
extern INT prtfont(INT prtnum, CHAR *font);
extern INT prtfree(INT prtnum);
#if OS_WIN32 && OS_WIN32GUI
extern INT32 prtgetcolor(BITMAP bm, UCHAR *ptrend, INT h1, INT v1);
#endif
extern INT prtgeterror(CHAR *errorstr, INT size);
extern INT prtgetpaperbins(CHAR *prtname, CHAR **buffer, INT size);
extern INT prtgetpapernames(CHAR *prtname, CHAR **buffer, INT size);
extern INT prtgetprinters(CHAR **buffer, UINT size);
extern INT prtimage(INT prtnum, void *pixhandle);
extern INT prtinit(PRTINITINFO *initinfo);
extern INT prtisdevice(INT prtnum);
extern INT prtlf(INT prtnum, INT cnt);
extern INT prtline(INT prtnum, INT hpos, INT vpos);
extern INT prtlinewidth(INT prtnum, INT width);
extern INT prtnameisdevice(CHAR *prtname);
extern INT prtnl(INT prtnum, INT cnt);
extern INT prtopen(CHAR *prtname, PRTOPTIONS *options, INT *prtnum);
extern INT prtpagesetup(void);
extern INT prtpdfcircle(INT prtnum, INT r, INT fill);
extern INT prtpdfcolor(INT prtnum, INT color);
extern INT prtpdfend(INT prtnum);
extern INT prtpdffont(INT prtnum, CHAR *name, INT bold, INT italic, INT pointsize);
extern INT prtpdfimage(INT prtnum, void *pixhandle);
extern INT prtpdfinit(INT prtnum);
extern INT prtpdfline(INT prtnum, INT xpos, INT ypos);
extern INT prtpdflinewidth(INT prtnum, INT width);
extern INT prtpdfoffset(INT prtnum);
extern INT prtpdfpageend(INT prtnum);
extern INT prtpdfpagestart(INT prtnum);
extern INT prtpdfpos(INT prtnum, INT xpos, INT ypos);
extern INT prtpdfrect(INT prtnum, INT x, INT y, INT fill);
extern INT prtpdftab(INT prtnum, INT pos, UINT oldTabbing);
extern INT prtpdftriangle(INT prtnum, INT x1, INT y1, INT x2, INT y2);
extern INT prtpos(INT prtnum, INT hpos, INT vpos);
extern INT prtputerror(CHAR *errorstr);
extern INT prtrect(INT prtnum, INT x, INT y);
extern void prtSetPdfSize(INT prtnum);
extern INT prtSetPdfImageCacheSize(INT prtnum);
extern INT prttab(INT prtnum, INT pos);
extern INT prttext(INT prtnum, UCHAR *string, UCHAR chr, INT len, INT blanks, INT type, INT value, INT angle);
extern INT prttriangle(INT prtnum, INT x1, INT y1, INT x2, INT y2);
extern void InitializePrintOptions(PRTOPTIONS *options);

#endif  /* _PRT_INCLUDED */
