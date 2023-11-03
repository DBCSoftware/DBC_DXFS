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
 
#ifndef _PRTX_INCLUDED
#define _PRTX_INCLUDED

#include "dbcmd5.h"

#ifndef FONTFILES_H_
#include "fontfiles.h"
#endif

#ifdef __MACOSX
#include <Carbon/Carbon.h>
#endif

typedef union {
	INT handle;
#if OS_WIN32
	HANDLE winhandle;
#endif
#if OS_UNIX
	FILE *stream;
#endif
} PRINTHANDLE;

/* stucture defines */
typedef struct {
	CHAR offset[24];
} PDFOFFSET;

/* Each of these represents an image that appeared on a print statement
 * but has not yet been written to disk */
typedef struct {
	UCHAR **image;
	INT hsize;
	INT vsize;
	INT bpp;
	INT bcount;						/* count of bytes in the image */
	md5_byte_t digest[16];			/* md5 of the encoded image */
	PIXHANDLE pixhandle;
} PDFIMAGE;

/* Each of these represents an image in a PDF file on disk */
typedef struct {
#ifndef __MACOSX
	INT objectnumber;				/* When image was written to disk, the object number assigned */
#else
	CGImageRef imageRef;	/* Image object used when it was printed to output */
#endif
	md5_byte_t digest[16];			/* The md5 digest of the image */
} PDFDISKIMAGE;

/* Each of these represents an embedded font in a PDF file on disk */
typedef struct {
	FONTFILEHANDLE ffh;
} PDFEFONT;

#define PDF_STD_FONT_BASE_COUNT 13
typedef struct printinfo_struct {
	PRTMARGINS margins;
	INT flags;						/* type, format, allocation and init flags */
	INT openflags;					/* flags passed in to open */
	INT copies;						/* number of copies */
	INT dpi;						/* dpi value */
	CHAR **prtname;					/* spool file name */
	CHAR **papersize;				/* paper size (name of paper type) */
	CHAR **paperbin;				/* paper source (name of paper bin) */
	CHAR **docname;					/* document name */

	/*
	 * print map table, for translation
	 */
	UCHAR **map;

	CHAR allocinfostart;			/* NOTE: everything below gets cleared on free */
	PRINTHANDLE prthandle;
	INT linebgn;
	INT linepos;
	INT linecnt;
	INT linesize;
	INT linetype;
	INT linevalue;
	INT lineangle;
	INT imageangle;
	UCHAR **linebuf;
	INT bufcnt;
	INT bufsize;
	UCHAR **buffer;
	OFFSET filepos;
	INT pagecount;
	INT linewidth;
	INT fontheight;					/* current, in points */
	union {
		struct {
			UCHAR prefix;			/* prefix character for ANSI control */
		} fcc;
		struct {
			/*
			 * This contains the font identifying PDF string as it needs to appear in a text object.
			 * Generally it will look like this...
			 *
			 * /Fxx <size> Tf\n
			 *
			 * Where the xx is a number that is arbitrary but we by convention use a one-based
			 * index into the list of standard font names that appears in the prtfont
			 * and prtflushline functions.
			 */
			CHAR fontname[32];
			INT32 color;			/* color */
			/*
			 * current x position, *not* scaled on OSX, scaled everywhere else
			 */
			DOUBLE posx;
			/*
			 * current y position, *not* scaled on OSX, scaled everywhere else
			 */
			DOUBLE posy;
			/*
			 * Width of paper in PDF device space units which are 1/72 inch, less a margin.
			 *
			 * e.g. For letter size in portrait mode, the physical size is 8 1/2 v 11 inches
			 * after subtracting margins this would be 8 x 10 1/2 inches
			 * Multiplying this by 72 gives 576 x 756
			 *
			 * This value and the next (height) are written into the Pages object
			 * in the /MediaBox rectangle
			 *
			 * A typical Pages object entry in the pdf file would look like this
			 * <<
			 * /Type /Pages
			 * /Kids [ 8 0 R ... ]
			 * /MediaBox [ 0 0 576 756 ]
			 * >>
			 *
			 */
			INT width;
			/*
			 * Height of paper, in 1/72", see above commments
			 */
			INT height;
			INT gwidth;				/* dots width of paper */
			INT gheight;			/* dots height of paper */
			/*
			 * zero-based index into list of standard fonts;
			 * Courier, Times, Helvetica in Plain, Italic, Bold, and BoldItalic,
			 * Symbol and finally, Dingbats.
			 *
			 * This will be a number larger than PDF_STD_FONT_BASE_COUNT if an embedded font is current
			 */
			INT fontselected;
			/*
			 * Array of booleans for pdf file generation
			 * index is 'fontselected' (only slots 0 through PDF_STD_FONT_BASE_COUNT are used)
			 * Only used for standard fonts. Irrelevant to embedded fonts
			 * (16 so that it is a multiple of 4)
			 */
			unsigned char fontused[16];
			/*
			 * Holds the pdf file object numbers assigned for use in the
			 * Resource Dictionary object
			 */
			unsigned short fontobjn[16];
			OFFSET pagestart;		/* file position of start of page */
			INT objectcount;		/* number of objects in the document */
			INT offsetcount;		/* number of offsets */
			INT offsetsize;			/* size of offset array */
			/*
			 * Number of images that need to be written after the end of the page
			 * This is not used on OSX and will always be zero.
			 */
			INT imagecount;
			INT imagesize;			/* size of images array, in units of PDFIMAGE */
			INT imgobjcount;		/* number of objects in use*/
			INT imgobjsize;			/* size of image object array allocated, in units of sizeof(PDFDISKIMAGE)*/
			PDFOFFSET **offsets;	/* byte offsets for PDF xref table */
			PDFIMAGE **images;		/* array of image structures for current page*/
			PDFDISKIMAGE **imgobjs;	/* points to an array storing object numbers and md5 digests*/
			INT **pageobjnums;		/* array of object numbers assigned to pages */
			INT pageobjarraylen;	/* number of elements allocated in the above array */
			INT pageobjarraysize;	/* the number of elements in use in the above array */
			INT efontsinuse;		/* the count of distinct embedded fonts used (size of below array) */
			PDFEFONT **efonts;		/* points to an array of structures defining embedded fonts */
#ifdef __MACOSX
			CGContextRef pdfContext;
			CGFloat scaleX, scaleY;
			CGFloat lineWidth;
			CGColorRef cgColor;		/* For quartz, it is easier to store it this way, 'color' is not used */
			CGDataConsumerRef dataConsumer;
#endif
		} pdf;
		struct {
			INT width;				/* scaled width of paper */
			INT height;				/* scaled height of paper */
			INT posx;				/* x position */
			INT posy;				/* y position */	
		} psl;
#if OS_WIN32
		struct {
			INT32 color;			/* color */
			USHORT hsize;			/* horz size of print page in pixels */
			USHORT vsize;			/* vert size of print page in pixels */
			POINT pos;				/* current pixel print position, scaled */
			USHORT fonthsize;		/* current font typical character width */
			USHORT fontvsize;		/* current font character height including white space */
			INT scalex;				/* x scale factor times 3600 for print scaling */
			INT scaley;				/* y scale factor times 3600 for print scaling */
			HDC hdc;				/* handle of the printer device context */
			LOGFONT lf;				/* logical font structure */
			HFONT hfont;			/* handle of the logical font */
			CHAR device[128];		/* Place to save device name as reported by GetPrinter() */
			CHAR port[128];			/* Place to save port name as reported by GetPrinter() */
			WORD binnumber;			/* Place to save the paper bin number as reported by DeviceCapabilities with DC_BINS */
			HGLOBAL hdevmode;		/* Save handle to devmode structure used in CreateDC */
		} gui;
#endif
	} fmt;
} PRINTINFO;

#ifdef PRT_C_
PRINTINFO **printtabptr;
CHAR program[64];
CHAR version[32];
UCHAR *rle_buffer;
UINT rle_buffer_size;
CHAR prterrorstring[256];
CHAR *fontnames[] = {
	"Courier", "Courier-Oblique", "Courier-Bold", "Courier-BoldOblique",
	"Times-Roman", "Times-Italic", "Times-Bold", "Times-BoldItalic",
	"Helvetica", "Helvetica-Oblique", "Helvetica-Bold", "Helvetica-BoldOblique",
	"Symbol", "ZapfDingbats"
};
#else
extern PRINTINFO **printtabptr;
extern CHAR program[64];
extern CHAR version[32];
extern UCHAR *rle_buffer;
extern UINT rle_buffer_size;
extern CHAR prterrorstring[256];
extern CHAR *fontnames[];
#endif


#define PRTFLAG_TYPE_FILE			0x00000001
#define PRTFLAG_TYPE_RECORD	0x00000002
#define PRTFLAG_TYPE_DEVICE		0x00000004
#define PRTFLAG_TYPE_PRINTER	0x00000008
#define PRTFLAG_TYPE_DRIVER		0x00000010
#define PRTFLAG_TYPE_PIPE			0x00000020
#define PRTFLAG_TYPE_CUPS		0x00000040
#define PRTFLAG_TYPE_MASK		0x0000007F

#define PRTFLAG_FORMAT_FCC		0x00000100
#define PRTFLAG_FORMAT_DCC	0x00000200
#define PRTFLAG_FORMAT_GUI		0x00000400
#define PRTFLAG_FORMAT_PCL		0x00000800
#define PRTFLAG_FORMAT_PSL		0x00001000
#define PRTFLAG_FORMAT_PDF		0x00002000
#define PRTFLAG_FORMAT_MASK	0x00003F00

#define PRTGET_PRINTERS_DEFAULT	0x01
#define PRTGET_PRINTERS_ALL		0x02
#define PRTGET_PRINTERS_FIND	0x04

#if OS_WIN32
INT getdefdevname(HGLOBAL *);
HGLOBAL getdevicemode(HGLOBAL, HGLOBAL, INT, INT, INT, INT, CHAR *, CHAR *, INT);
#endif
void prtaerror(CHAR *function, INT err);

extern INT prtainit(PRTINITINFO *initinfo);
extern INT prtaexit(void);
extern INT prtadefname(CHAR *prtname, INT size);
extern INT prtanametype(CHAR *prtname);
#if OS_UNIX
extern INT prtagetpaperinfo(CHAR *prtname, CHAR **buffer, INT size, INT infoType);
#else
extern INT prtagetpaperbins(CHAR *prtname, CHAR **buffer, INT size);
extern INT prtagetpapernames(CHAR *prtname, CHAR **buffer, INT size);
#endif
extern INT prtasubmit(CHAR *prtfile, PRTOPTIONS *options);
extern INT prtadevopen(INT prtnum, INT alloctype);
extern INT prtadevclose(INT prtnum);
extern INT prtadevwrite(INT prtnum);
#if OS_WIN32
extern INT prtagetprinters(INT type, CHAR **buffer, UINT size);
extern INT prtaprtopen(INT prtnum);
extern INT prtaprtclose(INT prtnum);
extern INT prtaprtwrite(INT prtnum);
extern INT prtadrvgetpaperbinnumber(CHAR *device, CHAR *port, CHAR *binname);
extern INT prtadrvchangepaperbin(INT prtnum, INT paperbin);
extern INT prtadrvopen(INT prtnum);
extern INT prtadrvclose(INT prtnum);
extern INT prtadrvpage(INT prtnum);
extern INT prtadrvrect(INT prtnum, INT x, INT y, INT fill);
extern INT prtadrvcircle(INT prtnum, INT r, INT fill);
extern INT prtadrvtriangle(INT prtnum, INT x1, INT y1, INT x2, INT y2);
extern INT prtadrvtext(INT prtnum, UCHAR *text, INT textlen);
extern INT prtadrvline(INT prtnum, INT hpos, INT vpos);
extern INT prtadrvfont(INT prtnum, CHAR *font);
extern INT prtadrvpixmap(INT prtnum, PIXHANDLE pixhandle);
#if OS_WIN32GUI
extern INT prtadrvpagesetup(void);
#endif
#endif
#if OS_UNIX
#ifdef CUPS
INT prtacupsopen(INT prtnum, INT alloctype);
INT prtacupsclose(INT prtnum);
#endif
extern INT prtacupssubmit(CHAR *prtfile, PRTOPTIONS *options);
extern INT prtagetprinters(CHAR **buffer, UINT size);
extern INT prtapipopen(INT prtnum, INT alloctype);
extern INT prtapipclose(INT prtnum);
extern INT prtapipwrite(INT prtnum);
#endif

#ifdef __MACOSX
INT prtpdfosxtext(INT prtnum, UCHAR *string, UCHAR chr, INT len, INT blanks, INT type, INT value, INT angle);
INT prtpdfosxlf(INT prtnum, INT cnt);
#endif

#endif  /* _PRTX_INCLUDED */
