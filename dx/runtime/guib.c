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

#define GUIB
#define INC_STRING
#define INC_CTYPE
#define INC_STDLIB
#define INC_LIMITS
#define INC_SETJMP
#define INC_MATH
#include "includes.h"
#include "base.h"
#include "gui.h"
#include "guix.h"
#include "fio.h"
#include "imgs.h"
#include "tiffRLE.h"

#if OS_WIN32
__declspec (noreturn) extern void dbcerrinfo(INT, UCHAR *, INT);
__declspec (noreturn) extern void dbcexit(INT);
__declspec (noreturn) extern void dbcerrjmp(void);
__declspec (noreturn) extern void dbcerror(INT num);
__declspec (noreturn) extern void dbcdeath(INT);
#else
extern void dbcerrinfo(INT, UCHAR *, INT)  __attribute__((__noreturn__));
extern void dbcexit(INT)  __attribute__((__noreturn__));
extern void dbcerrjmp(void)  __attribute__((__noreturn__));
extern void dbcerror(INT num)  __attribute__((__noreturn__));
extern void dbcdeath(INT)  __attribute__((__noreturn__));
#endif

/* stuff for IJG Jpeg library and jpgtopix routines*/
#define XMD_H  /* tell jpeg include not to redefine INT16 and INT32 */
#include "../jpeg/jinclude.h"
#undef FAR
#include "../jpeg/jpeglib.h"

static USHORT tifone = 1;
#define TIFF_BIGENDIAN      0x4d4d
#define TIFF_LITTLEENDIAN   0x4949
#define TIFF_VERSION        42
#define TIF_MACHINE_TYPE() (SHORT) ((*((CHAR *) &tifone) == 1) ? TIFF_LITTLEENDIAN : TIFF_BIGENDIAN)
#define TIF_IFD_SIZE(a) (sizeof(SHORT) + (a)*TIFF_TAG_SIZE + sizeof(INT))
#define TIFFTAG_IMAGEWIDTH      256 /* image width in pixels */
#define TIFFTAG_IMAGELENGTH     257 /* image height in pixels */
#define TIFFTAG_BITSPERSAMPLE   258 /* bits per channel (sample) */
#define TIFFTAG_COMPRESSION     259 /* data compression technique */
#define TIFFTAG_PHOTOMETRIC     262 /* photometric interpretation */
#define TIFFTAG_FILLORDER       266 /* how do bits sequence into bytes? default is 1 */
#define TIFFTAG_STRIPOFFSETS    273 /* offsets to data strips */
#define TIFFTAG_SAMPLESPERPIXEL 277 /* samples per pixel */
#define TIFFTAG_ROWSPERSTRIP    278 /* rows per strip of data */
#define TIFFTAG_STRIPBYTECOUNTS 279 /* bytes counts for strips */
#define TIFFTAG_XRESOLUTION     282 /* pixels/resolution in x */
#define TIFFTAG_YRESOLUTION     283 /* pixels/resolution in y */
#define TIFFTAG_GROUP3OPTIONS   292 /* Now known as T4Options */
#define TIFFTAG_GROUP4OPTIONS   293 /* Now known as T6Options */
#define TIFFTAG_RESOLUTIONUNIT  296 /* 2=Inch(DEFAULT), 3=Centimeter */
#define TIFFTAG_PREDICTOR       317 /* LZW stuff */
#define TIFFTAG_COLORMAP        320 /* RGB map for pallette image */
#define TIFF_SHORT       3          /* 16-bit unsigned integer */
#define TIFF_LONG        4          /* 32-bit unsigned integer */
#define TIFF_RATIONAL    5          /* 64-bit unsigned fraction */
#define COMPRESSION_NONE         1  /* dump mode */
#define COMPRESSION_CCITTRLE     2  /* CCITT modified Huffman RLE */
#define COMPRESSION_CCITTFAX3    3  /* CCITT Group 3 fax encoding */
#define COMPRESSION_CCITTFAX4    4  /* CCITT Group 4 fax encoding */
#define COMPRESSION_LZW          5  /* Lempel-Ziv  & Welch */
#define COMPRESSION_PACKBITS 32773  /* Macintosh RLE */
#define PHOTOMETRIC_PALETTE      3  /* color map indexed */
#define PHOTOMETRIC_RGB          2  /* RGB color model */
#define PHOTOMETRIC_MINISWHITE   0  /* min value is white */
#define PHOTOMETRIC_MINISBLACK   1  /* min value is black */

/*
 * GIF Header and Logical Screen Descriptor
 */
static struct {
	CHAR sig[3];
	CHAR version[3];
	UINT screenwidth, screenheight;
	UCHAR flags, background, aspect;
} gh;

static struct {
	UINT left, top, width, length;
	UCHAR flags;
} iblk;

typedef struct {
	UCHAR leadzeroes;
	UCHAR bitstring;
} RUNCODE;


#if OS_WIN32GUI
static HDC hdc;               /* device context for main window */
static HWND hwnd;
static BITMAPINFO *pbmi;	/*The BITMAPINFO structure defines the dimensions and color information for a DIB. */
#endif

static INT TIFF_HEADER_SIZE = 2 * sizeof(SHORT) + sizeof(INT);
static INT TIFF_TAG_SIZE = 2 * sizeof(SHORT) + 2 * sizeof(INT);
static INT oldbwtiff = FALSE; /*Do we do it the old, FUBAR way, or the right way? */
static INT numcolors;
static UCHAR *bits, *bitsptr; /* pixmap bit pointers */
static UCHAR *bits_end;       /* pointer to first byte past pixmap */
static INT bitsrowbytes;     /* pixmap bytes per row */
static INT bpp;              /* pixmap bits per pixel */
static INT hsize;            /* pixmap horizontal size */
static INT vsize;            /* pixmap vertical size */
static INT cgsize;           /* working size of cg */
static INT swapflag;         /* on read, byte swap required */
static INT Compression;      /* tiff compression type */
static INT Photometric;			/* Photometric Intrepetation 0=WhiteIsZero */
static SHORT TwoDim;          /* flag for two-D T4 Encoding */
static SHORT DimFlag;         /* flag for row state */
static SHORT UncAllow;        /* flag for allowing uncmompressed mode in T4 and T6 */
static SHORT Predictor;       /* Differencing Predictor for LZW */
static UCHAR *tifstrip;       /* pointer to a tif strip */
static UINT tifstripsize;     /* number of bytes in tif strip */
static INT gifimagewidth;         /* width of image */
static INT gifimagelength;        /* length of image */
static UINT SamplesPerPixel;
static INT tifimagewidth;    /* ImageWidth from tif image header */
static INT tifimagelength;   /* ImageLEngth from tif image header */
static INT tifxchgbw;        /* black and White exchange based on PhotoMetricInterpretation */
static UCHAR colors[768];     /* pixel values for translation to color */
static UCHAR flags;                /* image flags */
static UCHAR **uncmpstrip;  /* uncompressed strip */
static INT uncmpstripsize;   /* uncompressed strip size */
static INT lzwmask[13] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1FF, 0x3FF, 0x7FF, 0xFFF };

static UCHAR t2mask[9] = { 0x00, 0x80, 0x40, 0x20, 0x10,
					0x08, 0x04, 0x02, 0x01};
static UCHAR t1mask[9] = { 0x00, 0x01, 0x02, 0x04, 0x08,
					0x10, 0x20, 0x40, 0x80};
static UCHAR *maskin;

static UINT RowsPerStrip;                   /* number of rows in a strip */
static UCHAR **table;                   /* decompression arrays */
static SHORT **tableprefix;
static RUNCODE **blackbits;
static RUNCODE **whitebits;
static SHORT **WCode;                   /* Run-Length Codes */
static SHORT **BCode;
static SHORT **Reference;               /* Changing Elements for reference line */
static SHORT **Current;                 /* Changing Elements on current line */
static INT BitsLeftOut, BitsLeftIn;     /* Input and Output streams for bi-level compression */
/*********************************************************/
/*             Decompression Variables                   */

static UCHAR *gifline;             /* linebuffer of a gif image */
static INT codebits;               /* initial code size */
static INT bits2;                  /* first table location after root */
static INT codesize;               /* current code size */
static INT codesize2;              /* next codesize */
static INT nextcode;               /* next available table entry */
static INT thiscode;               /* code being expanded */
static INT oldtoken;               /* Last symbol decoded */
static INT currentcode;            /* code just read */
static INT oldcode;                /* previous code */
static INT bitsleft;               /* bits left in current byte of block */
static INT blocksize;              /* size of block in bytes */
static INT posbyte;                   /* position to write in line buffer */
static UCHAR *datacurr;            /* pointer to current byte in block */
static UCHAR *dataend;             /* pointer to last byte in block */
static UCHAR *block;               /* block being expanded */
static UCHAR *u;                   /* pointer into firstcodestack */
static UCHAR *firstcodestack;      /* stack for building strings for output */
static UCHAR *lastcodestack;       /* holds last character of table entries */
static INT *codestack;             /* holds table entry to be combined with */
							   /* lastcodestack to form output string */
static UCHAR *strque;              /* holds output string */
static INT quecurr;                /* current position of que */
static INT queend;                 /* position of last entry in que */
static SHORT queempty;             /* flag set if que is empty */

static INT wordmasktable[] = {		/* wordmasktable is for extracting codes from the */
	0x0000, 0x0001, 0x0003, 0x0007,	/* compressed data */
	0x000f, 0x001f, 0x003f, 0x007f,
	0x00ff, 0x01ff, 0x03ff, 0x07ff,
	0x0fff, 0x1fff, 0x3fff, 0x7fff
};

/* finite state machine state defines */
#define FINISH 0
#define START 1
#define A 2
#define B 3
#define C 4
#define D 5
#define E 6
#define F 7
#define G 8
#define H 9
#define I 10
#define J 11
#define K 12
#define L 13
#define ONE 1
#define TWO 2
#define THREE 3
#define FOUR 4
#define FIVE 5
#define SIX 6
#define END 7
#define MODE_ERROR 8
#define VL3 1
#define VL2 2
#define VL1 3
#define VR3 4
#define VR2 5
#define VR1 6
#define V0 7
#define HORZ 9
#define PASS 10
#define EOL 11
#define MODE_UNCMP 12
#define UNC_RUN1 1
#define UNC_RUN2 2
#define UNC_RUN3 3
#define UNC_RUN4 4
#define UNC_RUN5 5
#define UNC_RUN6 6
#define UNC_EXIT 7

#if OS_WIN32GUI
static PLOGPALETTE pLogPal;   /* logical palette built from a TIFF image palette */
#endif

static INT totif1(UCHAR *);
#if OS_WIN32GUI
static void totifn(UCHAR *);
#endif
static INT totif24(UCHAR *);
static INT fromtif(PIXHANDLE, UCHAR **);
static INT fromtif1(void);
static INT fromtif2(void);
static INT fromtif3(void);
static INT fromtif4(void);
static INT fromtif5(void);
static INT fromtif6(void);
static INT fromtif7(void);
static INT fromtif8(void);
static INT fromtif9(void);
static INT fromtif10(void);
static INT fromtif11(void);
static INT fromtif11L(void);
static INT fromtif12(void);
static INT fromtif13(void);
static INT fromtif13_RLE(void);
static INT fromtif14(void);
static UCHAR *uncmp(void);
static INT checkmap(INT, UCHAR *);
static USHORT getshort(UCHAR *);
static UINT getint(UCHAR *);
static void tcodeinit(void);
static INT endofline(INT);
static INT bilevelunc(void);
static INT decode1drow(void);
static INT decode1drow_RLE(void);
static INT decode2drow(void);
static INT fillspan(INT, INT, UCHAR *);
static INT fillspan1(INT, INT, UCHAR *);
static INT fillspan4(INT, INT, UCHAR *);
static INT fillspan8(INT, INT, UCHAR *);
static INT fillspan24(INT, INT, UCHAR *);
static INT fromgif(UCHAR **);
static INT fromgif1(UCHAR *);
static INT fromgif2(UCHAR *);
static INT fromgif3(UCHAR *);
static INT fromgif4(UCHAR *);
static void recordCurrentRun(INT*, INT*, INT);
static INT uncompGif(UCHAR **cg);
static INT uncompinit(UCHAR **);
static void extensionGif(UCHAR **cg);
static void runcodeinit(void);
static INT outputcode(INT, INT, UCHAR **);
static INT findlength(UCHAR *, INT);
static INT outputeofb(UCHAR **);
static UCHAR* getTiffTagValueAddr(UCHAR* ptififd, INT index);
static void setTiffHeader(UCHAR*, SHORT, SHORT, INT);
static void setTiffHeader1stIFD(UCHAR*, INT);
static void setTiffTag(UCHAR* ptififd, INT index, SHORT type, SHORT size, INT count, INT value);
static void setTiffIFDNextOffset(UCHAR* ptififd, INT numTags, INT value);
void jpegtermsource(j_decompress_ptr);
static void jpegsetupsource(j_decompress_ptr, UCHAR *);
void jpegerrorexit(j_common_ptr);
void jpeginitsource(j_decompress_ptr);
boolean jpegfillinputbuffer(j_decompress_ptr);
void jpegskipinputdata(j_decompress_ptr, long);

/**
 * Parse ptr into a number. Interpret it as KBytes
 * Set userTifMaxstripsize (in guix.h) to this value.
 */
void guiaTifMaxStripSize(CHAR *ptr) {
	long l1 = strtol(ptr, NULL, 10);
	userTifMaxstripsize = (UINT) (l1 << 10);
}

/* GUIAPIXMAPTOTIF */
INT guiaPixmapToTif(PIXMAP *pix, UCHAR *buffer, INT *pbufsize)
{

	CHAR *ptr;
	INT i1;
#if OS_WIN32GUI
	INT line;
	HPALETTE oldhPalette;
	HCURSOR hCursor;
	UINT olderrormode;
#endif

	if (!prpget("gui", "bwtiff", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) oldbwtiff = TRUE;
	else oldbwtiff = FALSE;
	bpp = pix->bpp;
#if OS_WIN32GUI
	hwnd = guia_hwnd();
	hdc = GetDC(hwnd);
	if (!bpp) {
		bpp = GetDeviceCaps(hdc, BITSPIXEL) * GetDeviceCaps(hdc, PLANES);
		if (bpp == 32 || bpp == 16) bpp = 24;
	}
	if (bpp <= 1) numcolors = 2;
	else if (bpp <= 4) numcolors = 16;
	else if (bpp <= 8) numcolors = 256;
	else numcolors = 0;
	if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24) return RC_ERROR;
#else
	if (bpp != 1 && bpp != 24) return RC_ERROR;
#endif
	hsize = pix->hsize;
	vsize = pix->vsize;
	bitsrowbytes = ((hsize * bpp + 31) / 32) * 4;

#if OS_WIN32GUI
	if (pix->hpal != NULL) {
		oldhPalette = SelectPalette(hdc, pix->hpal, TRUE);
		RealizePalette(hdc);
	}
	else oldhPalette = NULL;

	hCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
	ShowCursor(TRUE);

	if (NULL == pix->pbmi) {
		pbmi = (BITMAPINFO *) malloc(sizeof(BITMAPINFOHEADER) + (numcolors * sizeof(RGBQUAD)));
		if (NULL == pbmi) return RC_NO_MEM;
		memset(&pbmi->bmiHeader, 0, sizeof(BITMAPINFOHEADER));
		pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		pbmi->bmiHeader.biWidth = hsize;
		pbmi->bmiHeader.biHeight = vsize;
		pbmi->bmiHeader.biPlanes = 1;
		pbmi->bmiHeader.biBitCount = (unsigned short) bpp;
		pbmi->bmiHeader.biCompression = BI_RGB;
		pbmi->bmiHeader.biSizeImage = bitsrowbytes * vsize;
		pbmi->bmiHeader.biXPelsPerMeter = 1000;
		pbmi->bmiHeader.biYPelsPerMeter = 1000;
		pbmi->bmiHeader.biClrUsed = numcolors;
		pbmi->bmiHeader.biClrImportant = numcolors;
		pix->pbmi = pbmi;
	}
	else pbmi = pix->pbmi;

	line = 0;
	bits = (UCHAR *) malloc(bitsrowbytes * vsize);
	if (NULL == bits) return RC_NO_MEM;
	GetDIBits(hdc, pix->hbitmap, line, vsize, bits, pbmi, DIB_RGB_COLORS);
	olderrormode = SetErrorMode(SEM_NOALIGNMENTFAULTEXCEPT);
#else
	bits = *pix->pixbuf;
#endif

	bits_end = bits + bitsrowbytes * vsize;
	/*cg = buffer;*/
	cgsize = *pbufsize;
	setTiffHeader(buffer, TIF_MACHINE_TYPE(), TIFF_VERSION, TIFF_HEADER_SIZE);
	if (bpp == 1) {
		if ((i1 = totif1(buffer)) < 0) return i1;
	}
#if OS_WIN32GUI
	else if (bpp == 24) {
		i1 = totif24(buffer);
		if (i1 != 0) return i1;
	}
	else totifn(buffer);
#else
	else {
		i1 = totif24(buffer);
		if (i1 != 0) return i1;
	}
#endif
#if OS_WIN32GUI
	SetErrorMode(olderrormode);
	if (oldhPalette != NULL) {
		SelectPalette(hdc, oldhPalette, TRUE);
		RealizePalette(hdc);
	}

	ShowCursor(FALSE);
	SetCursor(hCursor);

	free(bits);
	ReleaseDC(hwnd, hdc);
#endif
	*pbufsize = cgsize;
	if (!cgsize) return RC_INVALID_HANDLE;
	return(0);
}

/*JPEG*/

/*Create a structure to control the input to the jpeg library.
  Use their struct plus a couple of fields of our own. */
typedef struct {
	struct jpeg_source_mgr pub;
	UCHAR *srcbuffer;
	UCHAR *nextinputbyte;
} JPEGSRCMGR;

typedef JPEGSRCMGR *JPEGSRCMGRPTR;

/*Create a structure to control fatal errors. Override the jpeg library
  routine error_exit in JERROR.C */
typedef struct {
	struct jpeg_error_mgr pub;
	jmp_buf jpegsetjumpbuffer;
} JPEGERRORMGR;

typedef JPEGERRORMGR *JPEGERRORMGRPTR;

void jpeginitsource(j_decompress_ptr dinfo)
{
	/*this is a no-op*/
}

boolean jpegfillinputbuffer(j_decompress_ptr dinfo)
{
	JPEGSRCMGRPTR src;
	src = (JPEGSRCMGRPTR) dinfo->src;
	src->pub.next_input_byte = src->nextinputbyte += 16;
	src->pub.bytes_in_buffer = 16;
	return (TRUE);
}

void jpegskipinputdata(j_decompress_ptr dinfo, long nbytes)
{
	JPEGSRCMGRPTR src;

	src = (JPEGSRCMGRPTR) dinfo->src;
	if (nbytes > 0){
		while (nbytes > (long) src->pub.bytes_in_buffer){
			nbytes -= (long) src->pub.bytes_in_buffer;
			jpegfillinputbuffer(dinfo);
		}
		src->pub.next_input_byte += (size_t) nbytes;
		src->pub.bytes_in_buffer -= (size_t) nbytes;
	}
}

void jpegtermsource(j_decompress_ptr dinfo)
{
	/*this is a no-op*/
}

/* The next routine replaces the jpeg library routine 'jpeg_stdio_src' */
static void jpegsetupsource(j_decompress_ptr dinfo, UCHAR *bptr)
{
	JPEGSRCMGRPTR src;

	if (dinfo->src == NULL){
		dinfo->src = (struct jpeg_source_mgr *)
			(*dinfo->mem->alloc_small) ((j_common_ptr) dinfo,
			JPOOL_PERMANENT, sizeof(JPEGSRCMGR));
		src = (JPEGSRCMGRPTR) dinfo->src;
		src->srcbuffer = bptr;
	}
	src = (JPEGSRCMGRPTR) dinfo->src;
	src->pub.init_source = jpeginitsource;
	src->pub.fill_input_buffer = jpegfillinputbuffer;
	src->pub.skip_input_data = jpegskipinputdata;
	src->pub.term_source = jpegtermsource;
	src->pub.bytes_in_buffer = 16;
	src->pub.next_input_byte = src->srcbuffer;
	src->nextinputbyte = src->srcbuffer;
}

/* Error exit override, the jpeg library default routine simply stops the program */
void jpegerrorexit(j_common_ptr dinfo)
{
	JPEGERRORMGRPTR errorptr;
	errorptr = (JPEGERRORMGRPTR) dinfo->err;
	(*dinfo->err->output_message) (dinfo);
	longjmp(errorptr->jpegsetjumpbuffer, 1);
}

/* GUIAJPGTOPIXMAP */
INT guiaJpgToPixmap(PIXHANDLE pixhandle, UCHAR **bufhandle)
{
	struct jpeg_decompress_struct dinfo;
	JPEGERRORMGR jerr;
	INT rc, numcolors;
	JSAMPARRAY scanlinefromijg;
	JSAMPROW scanline0;
	UINT i, rowstride, scansize, copysize;
	UCHAR *temp;           /* workspace for bit packing */
	PIXMAP *pix;

#if OS_WIN32GUI
	UINT bmisize, colorpos;
	JSAMPROW colormap0, colormap1, colormap2;
	BITMAP bmp;
	LPBITMAPINFOHEADER lpbmih;
	LPRGBQUAD lprgb;
	HPALETTE oldhPalette = NULL, newhPalette;
	HCURSOR hCursor;
	HDC hDC;               /* device context for main window */
	HWND hWnd;
	PLOGPALETTE pLogPal;
	CHAR work[64];
#else
	INT j;
#endif

	dinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpegerrorexit;
	if (setjmp(jerr.jpegsetjumpbuffer)){
		jpeg_destroy_decompress(&dinfo);
		return  RC_ERROR;
	}

	pix = *pixhandle;
#if OS_WIN32GUI
	hCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
	ShowCursor(TRUE);
	if (!GetObject(pix->hbitmap, sizeof(BITMAP), &bmp)){
		ShowCursor(FALSE);
		SetCursor(hCursor);
		return  RC_ERROR;
	}
	if (bmp.bmBitsPixel < 16) numcolors = 1 << bmp.bmBitsPixel;
	else numcolors = 0;

	/* Allocate space for the full DIB header including color table. */
	/* If bpp is >= 16 then there is no color table. */
	/* The DIB will be built as 24 bpp in that case. */
	bmisize = sizeof(BITMAPINFOHEADER) + numcolors * sizeof(RGBQUAD);
	if ((lpbmih = (LPBITMAPINFOHEADER) malloc(bmisize)) == NULL){
		ShowCursor(FALSE);
		SetCursor(hCursor);
		return RC_NO_MEM;
	}
	memset(lpbmih, 0, bmisize);
#else
	if (pix->bpp == 1) numcolors = 2;
	else numcolors = 0;
#endif

	jpeg_create_decompress(&dinfo);
	jpegsetupsource(&dinfo, *bufhandle); /* my routine */
	jpeg_read_header(&dinfo, TRUE);

	/*If we are using 256 or less colors then ask the jpeg
	  library software to compute the best palette */
	if (numcolors) {
		dinfo.quantize_colors = TRUE;
		dinfo.desired_number_of_colors = numcolors;
		dinfo.two_pass_quantize = TRUE;
	}
	jpeg_calc_output_dimensions(&dinfo);
	rowstride = dinfo.output_width * dinfo.output_components;

	/*The memory allocated in the next line is freed at jpeg_finish*/
	if ((scanlinefromijg = (*dinfo.mem->alloc_sarray)((j_common_ptr) &dinfo, JPOOL_IMAGE, rowstride, 1)) == NULL) {
#if OS_WIN32GUI
		free(lpbmih);
		ShowCursor(FALSE);
		SetCursor(hCursor);
#endif
		return RC_ERROR;
	}
	jpeg_start_decompress(&dinfo);

#if OS_WIN32GUI
	/* Set up the parameters for the DIB */
	/* The Jpeg library always returns either 8 or 24 bpp */
	lpbmih->biSize = sizeof(BITMAPINFOHEADER);
	lpbmih->biWidth = bmp.bmWidth;
	/* Win95 supports top-down DIBs by setting the Height negative.*/
	/* I would like to use this but Win3.1 doesn't know about it!  */
	lpbmih->biHeight = bmp.bmHeight;
	lpbmih->biPlanes = 1;
	if (numcolors) lpbmih->biBitCount = 8;
	else lpbmih->biBitCount = 24;
	bitsrowbytes = ((bmp.bmWidth * lpbmih->biBitCount + 31) & ~31) >> 3;
	lpbmih->biCompression = BI_RGB;
	lpbmih->biSizeImage = bitsrowbytes * bmp.bmHeight;
	lpbmih->biXPelsPerMeter = 0;
	lpbmih->biYPelsPerMeter = 0;
	lpbmih->biClrUsed = 0;
	lpbmih->biClrImportant = 0;
	if ((bits = (UCHAR *)calloc(lpbmih->biSizeImage, sizeof(BYTE))) == NULL){
		ShowCursor(FALSE);
		SetCursor(hCursor);
		jpeg_abort_decompress(&dinfo);
		return RC_NO_MEM;
	}
	bitsptr = bits + (bitsrowbytes * (bmp.bmHeight - 1));
#else
	bitsrowbytes = ((pix->hsize * pix->bpp + 0x1F) & ~0x1F) >> 3;
	bitsptr = bits = *pix->pixbuf;;
#endif
	scansize = (pix->vsize < dinfo.output_height) ? (UINT) pix->vsize : dinfo.output_height;
	copysize = (pix->hsize < dinfo.output_width) ? (UINT) pix->hsize: dinfo.output_width;
	scanline0 = scanlinefromijg[0];
	if (numcolors) {
		while (dinfo.output_scanline < scansize){
			/* Call the Jpeg library to get a scan line and move it to the DIB bitmap */
			jpeg_read_scanlines(&dinfo, scanlinefromijg, 1);
#if OS_WIN32GUI
			memcpy(bitsptr, scanline0, copysize);
			bitsptr -= bitsrowbytes;
#else
			temp = bitsptr;
			for (i = 0; i < copysize; ) {
				j = (scanline0[i++] << 7) & 0x80;
				j += (scanline0[i++] << 6) & 0x40;
				j += (scanline0[i++] << 5) & 0x20;
				j += (scanline0[i++] << 4) & 0x10;
				j += (scanline0[i++] << 3) & 0x08;
				j += (scanline0[i++] << 2) & 0x04;
				j += (scanline0[i++] << 1) & 0x02;
				j += (scanline0[i++]) & 0x01;
				*bitsptr++ = (UCHAR ) j;
			}
			bitsptr = temp + bitsrowbytes;
#endif
		}
	}
	else {
#if OS_WIN32
		__try { // @suppress("Statement has no effect") // @suppress("Symbol is not resolved")
#endif
		while (dinfo.output_scanline < scansize){
			jpeg_read_scanlines(&dinfo, scanlinefromijg, 1);
			scanline0 = scanlinefromijg[0];
			temp = bitsptr;
			if (dinfo.output_components == 1) {			/* JPEG image is grayscale */
				for (i = 0; i < copysize; i++, scanline0++){
					*bitsptr++ = scanline0[0];
					*bitsptr++ = scanline0[0];
					*bitsptr++ = scanline0[0];
				}
			}
			else {										/* JPEG image is full color */
				for (i = 0; i < copysize; i++, scanline0 += 3){
#if OS_WIN32GUI
					*bitsptr++ = scanline0[2];
					*bitsptr++ = scanline0[1];
					*bitsptr++ = scanline0[0];
#else
					*bitsptr++ = scanline0[0];
					*bitsptr++ = scanline0[1];
					*bitsptr++ = scanline0[2];
#endif
				}
			}
#if OS_WIN32GUI
			bitsptr = temp - bitsrowbytes;
#else
			bitsptr = temp + bitsrowbytes;
#endif
		}
#if OS_WIN32
	}
	__except( EXCEPTION_EXECUTE_HANDLER ) {
		CHAR str1[64];
		int i1 = GetExceptionCode();
		sprintf(str1, "Windows exception code %d during JPEG processing", i1);
		dbcerrinfo(792, str1, -1);
	}
#endif

	}
#if OS_WIN32GUI
	hWnd = guia_hwnd();
	if ((hDC = GetDC(hWnd)) == NULL){
		free(lpbmih);
		free(bits);
		ShowCursor(FALSE);
		SetCursor(hCursor);
		jpeg_abort_decompress(&dinfo);
		return RC_ERROR;
	}
	if (numcolors) {
		if (pix->hpal != NULL) oldhPalette = SelectPalette(hDC, pix->hpal, TRUE);
		/*Allocate space for a logical palette*/
		if ((pLogPal = (PLOGPALETTE) guiAllocMem(sizeof(LOGPALETTE)
			+ (dinfo.actual_number_of_colors - 1) * sizeof(PALETTEENTRY))) == NULL) {
			ReleaseDC(hWnd, hDC);
			free(lpbmih);
			free(bits);
			ShowCursor(FALSE);
			SetCursor(hCursor);
			jpeg_abort_decompress(&dinfo);
			sprintf(work, "guiAllocMem returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, sizeof(LOGPALETTE) + (dinfo.actual_number_of_colors - 1) * sizeof(PALETTEENTRY));
			return  RC_ERROR;
		}
		/*Copy the colors from the Jpeg library stuff to the BITMAPINFO structure*/
		lpbmih->biClrUsed = dinfo.actual_number_of_colors;
		lprgb = (LPRGBQUAD)((LPSTR)lpbmih + (WORD)lpbmih->biSize);
		colormap0 = dinfo.colormap[0];
		if (dinfo.output_components == 1) {			/* JPEG image is grayscale */
			for (colorpos = 0; colorpos < lpbmih->biClrUsed; colorpos++){
				lprgb[colorpos].rgbRed = lprgb[colorpos].rgbGreen =
					lprgb[colorpos].rgbBlue = GETJSAMPLE(colormap0[colorpos]);
			}
			pix->isGrayScaleJpeg = TRUE;
		}
		else {										/* JPEG image is full color */
			colormap1 = dinfo.colormap[1];
			colormap2 = dinfo.colormap[2];
			for (colorpos = 0; colorpos < lpbmih->biClrUsed; colorpos++){
				lprgb[colorpos].rgbRed   = GETJSAMPLE(colormap0[colorpos]);
				lprgb[colorpos].rgbGreen = GETJSAMPLE(colormap1[colorpos]);
				lprgb[colorpos].rgbBlue  = GETJSAMPLE(colormap2[colorpos]);
			}
		}
		/*Now copy the colors from the BITMAPINFO to the LOGPALETTE*/
		pLogPal->palVersion = 0x300;  /* Windows 3.0 or later */
		pLogPal->palNumEntries = (WORD)lpbmih->biClrUsed;
		for (colorpos = 0; colorpos < lpbmih->biClrUsed; colorpos++, lprgb++){
			pLogPal->palPalEntry[colorpos].peRed = lprgb->rgbRed;
			pLogPal->palPalEntry[colorpos].peGreen = lprgb->rgbGreen;
			pLogPal->palPalEntry[colorpos].peBlue = lprgb->rgbBlue;
			pLogPal->palPalEntry[colorpos].peFlags = 0;
		}
		newhPalette = CreatePalette(pLogPal);
		guiFreeMem((UCHAR *) pLogPal);
		SelectPalette(hDC, newhPalette, FALSE);
		RealizePalette(hDC);
		if (pix->hpal != NULL) DeleteObject(pix->hpal);
		pix->hpal = newhPalette;
	}
	else	pLogPal = NULL;

	/*
	 * The SetDIBits function sets the pixels in a compatible bitmap (DDB) using the color data found in the specified DIB .

		hDC
		[in] Handle to a device context.

		pix->hbitmap
		[in] Handle to the compatible bitmap (DDB) that is to be altered using the color data from the specified DIB.

		uStartScan
		[in] Specifies the starting scan line for the device-independent color data in the array pointed to by the lpvBits parameter.

		bmp.bmHeight
		[in] Specifies the number of scan lines found in the array containing device-independent color data.

		bits
		[in] Pointer to the DIB color data, stored as an array of bytes.
		The format of the bitmap values depends on the biBitCount member of the BITMAPINFO structure pointed to by the lpbmi parameter.

		lpbmi
		[in] Pointer to a BITMAPINFO structure that contains information about the DIB.

		fuColorUse
		[in] Specifies whether the bmiColors member of the BITMAPINFO structure was provided and,
		if so, whether bmiColors contains explicit red, green, blue (RGB) values or palette indexes.
		DIB_RGB_COLORS  = The color table is provided and contains literal RGB values.
	*/
	if (SetDIBits(hDC, pix->hbitmap, 0, bmp.bmHeight, bits, (LPBITMAPINFO)lpbmih, DIB_RGB_COLORS) == 0) rc = -1;
	else rc = 0;
	free(bits);
	free(lpbmih);

	if (oldhPalette != NULL) {
		SelectPalette(hDC, oldhPalette, TRUE);
		RealizePalette(hDC);
	}
	if (!rc && pix->winshow != NULL) forceRepaint(pixhandle);
	ReleaseDC(hWnd, hDC);
	ShowCursor(FALSE);
	SetCursor(hCursor);
	if (dinfo.output_height > (UINT)bmp.bmHeight) jpeg_abort_decompress(&dinfo);
#else
	rc = 0;
	if (dinfo.output_height > (UINT)pix->vsize) jpeg_abort_decompress(&dinfo);
#endif
	else jpeg_finish_decompress(&dinfo);
	jpeg_destroy_decompress(&dinfo);
	return(rc);
}

/* GUIATIFTOPIXMAP */
INT guiaTifToPixmap(PIXHANDLE pixhandle, UCHAR **bufhandle)
{
	INT rc;
#if OS_WIN32GUI
	CHAR work[64];
#endif

#if OS_WIN32GUI
	INT line, colorpos, scansize;
	HCURSOR hCursor;
	HPALETTE hPalette, oldhPalette, newhPalette;
	UINT olderrormode;
#endif

	bpp = (*pixhandle)->bpp;
#if OS_WIN32GUI
	hwnd = guia_hwnd();
	hdc = GetDC(hwnd);
	if (!bpp) {
		bpp = GetDeviceCaps(hdc, BITSPIXEL) * GetDeviceCaps(hdc, PLANES);
		if (bpp == 32 || bpp == 16) bpp = 24;
	}
#endif
#if OS_WIN32GUI
	if (bpp <= 1) numcolors = 2;
	else if (bpp <= 4) numcolors = 16;
	else if (bpp <= 8) numcolors = 256;
	else numcolors = 0;
	if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24) return RC_ERROR;
#else
	if (bpp <= 1) numcolors = 2;
	else numcolors = 0;
	if (bpp != 1 && bpp != 24) return RC_ERROR;
#endif
	hsize = (*pixhandle)->hsize;
	vsize = (*pixhandle)->vsize;
#if OS_WIN32GUI
	bitsrowbytes = ((hsize * bpp + 31) / 32) * 4;
	bits = (UCHAR *) malloc(bitsrowbytes * vsize);
	if (NULL == bits) return RC_NO_MEM;

	if (numcolors) {
		pLogPal = (PLOGPALETTE) guiAllocMem(sizeof(LOGPALETTE) + (numcolors - 1) * sizeof(PALETTEENTRY));
		if (pLogPal == NULL) {
			free(bits);
			sprintf(work, "guiAllocMem returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, sizeof(LOGPALETTE) + (numcolors - 1) * sizeof(PALETTEENTRY));
			return RC_NO_MEM;
		}
		pLogPal->palVersion = 0x300;  /* Windows 3.0 or later */
		pLogPal->palNumEntries = numcolors;
	}
	else pLogPal = NULL;

	if (NULL == (*pixhandle)->pbmi) {	/* This should not happen! */
		pbmi = (BITMAPINFO *) malloc(sizeof(BITMAPINFOHEADER) + (numcolors * sizeof(RGBQUAD)));
		if (NULL == pbmi) {
			if (pLogPal != NULL) guiFreeMem((UCHAR*) pLogPal);
			free(bits);
			return RC_NO_MEM;
		}
		memset(&pbmi->bmiHeader, 0, sizeof(BITMAPINFOHEADER));
		pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		pbmi->bmiHeader.biWidth = hsize;
		pbmi->bmiHeader.biHeight = vsize;
		pbmi->bmiHeader.biPlanes = 1;
		pbmi->bmiHeader.biBitCount = (unsigned short) bpp;
		pbmi->bmiHeader.biCompression = BI_RGB;
		pbmi->bmiHeader.biSizeImage = bitsrowbytes * vsize;
		pbmi->bmiHeader.biXPelsPerMeter = 1000;
		pbmi->bmiHeader.biYPelsPerMeter = 1000;
		pbmi->bmiHeader.biClrUsed = numcolors;
		pbmi->bmiHeader.biClrImportant = 0;		/* Zero means all colors are required */
		(*pixhandle)->pbmi = pbmi;
	}
	else pbmi = (*pixhandle)->pbmi;

	hCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
	ShowCursor(TRUE);
	if ((*pixhandle)->hpal != NULL) {
		oldhPalette = SelectPalette(hdc, (*pixhandle)->hpal, TRUE);
		RealizePalette(hdc);
	}
	else oldhPalette = NULL;

	line = 0;

#if 0 /* See if removeing this causes any problems, it looks completely unnecessary to me */
	GetDIBits(hdc, (*pixhandle)->hbitmap, /* [in] handle to a DDB */
		line, vsize,
		bits,		/* [out] Pointer to a buffer to receive the bitmap data. */
		pbmi,		/* [in/out] Pointer to a BITMAPINFO structure that specifies the desired format for the DIB data. */
		DIB_RGB_COLORS);
#endif

	olderrormode = SetErrorMode(SEM_NOALIGNMENTFAULTEXCEPT);
#else
	bits = *((*pixhandle)->pixbuf);
	bitsrowbytes = (((*pixhandle)->hsize * (*pixhandle)->bpp + 0x1F) & ~0x1F) >> 3;
#endif

	bits_end = bits + bitsrowbytes * vsize;
#if OS_WIN32GUI
	bitsptr = bits_end - bitsrowbytes;
#else
	bitsptr = bits;
#endif
	/*cg = buffer;*/
	rc = fromtif(pixhandle /*(*pixhandle)->tifnum*/, bufhandle);
#if OS_WIN32GUI
	SetErrorMode(olderrormode);
	if (!rc) {
		if (vsize > tifimagelength) {
			line = vsize - tifimagelength;
			bitsptr = bits_end - bitsrowbytes * tifimagelength;
			scansize = tifimagelength;
		}
		else {
			line = 0;
			bitsptr = bits;
			scansize = vsize;
		}
		if (numcolors) {
			for (colorpos = 0; colorpos < numcolors; colorpos++) {
				pbmi->bmiColors[colorpos].rgbRed = pLogPal->palPalEntry[colorpos].peRed;
				pbmi->bmiColors[colorpos].rgbGreen = pLogPal->palPalEntry[colorpos].peGreen;
				pbmi->bmiColors[colorpos].rgbBlue = pLogPal->palPalEntry[colorpos].peBlue;
			}
			newhPalette = CreatePalette(pLogPal);
			hPalette = SelectPalette(hdc, newhPalette,
					(BOOL)((*pixhandle)->winshow == NULL || (*(*pixhandle)->winshow)->hwnd != GetActiveWindow()));
			RealizePalette(hdc);
			if (oldhPalette == NULL) oldhPalette = hPalette;
			if ((*pixhandle)->hpal != NULL) DeleteObject((*pixhandle)->hpal);
			(*pixhandle)->hpal = newhPalette;
		}

		SetDIBits(hdc,	/* This is actually ignored */
			(*pixhandle)->hbitmap,	/* The destination of this operation, a DDB */
			line,		/* starting scan line in bitsptr */
			scansize,	/* number of scan lines in bitsptr */
			bitsptr,	/* [in] pointer to DIB data */
			(*pixhandle)->pbmi,	/* [in] pointer to a BITMAPINFO */
			DIB_RGB_COLORS
		);
		if (oldhPalette != NULL) {
			SelectPalette(hdc, oldhPalette, TRUE);
			RealizePalette(hdc);
		}
	}
	if (pLogPal) guiFreeMem((UCHAR*) pLogPal);
	free(bits);
	ReleaseDC(hwnd, hdc);
	if (!rc && (*pixhandle)->winshow != NULL) forceRepaint(pixhandle);
	ShowCursor(FALSE);
	SetCursor(hCursor);
#endif
	return(rc);
}

/*
 * TOTIF1
 * convert from 1 bpp pixmap to tif
 * Lots of calls to memalloc in here, could cause compaction
 */
static INT totif1(UCHAR *cg)
{
	INT m, n, row, strip, stripcnt, RowsPerStrip;
	INT cgoffset = 0;
	UCHAR *dest, *deststart, *rowend, *nextrow, *ptififd;
	SHORT **temp, numTags = 9, *pstripcnttable;
	INT zeroes, bitvalue, codeindex;
	INT white, *pstripofftable;
	INT curindex, refindex;
	INT rowtotal, runlength;
	INT mode, diff;
	INT i1, tiffData;
	INT passflag;       /* a pass has occured */

/* create the header */
	if (cgsize < 200) {
		cgsize = 0;
		return 0;
	}
	if ((Reference = (SHORT **) memalloc(hsize * sizeof(SHORT), 0)) == NULL) {
		cgsize = 0;
		return RC_NO_MEM;
	}
	if ((Current = (SHORT **) memalloc(hsize * sizeof(SHORT), 0)) == NULL) {
		cgsize = 0;
		return RC_NO_MEM;
	}
	if ((blackbits = (RUNCODE **) memalloc(104 * sizeof(RUNCODE), 0)) == NULL) {
		cgsize = 0;
		return RC_NO_MEM;
	}
	if ((whitebits = (RUNCODE **) memalloc(104 * sizeof(RUNCODE), 0)) == NULL) {
		cgsize = 0;
		return RC_NO_MEM;
	}

	/* Use 8K strips */
	if (vsize > (8 * 8192)) RowsPerStrip = 1;
	else RowsPerStrip = (8 * 8192) / vsize;
	stripcnt = (INT) floor((vsize + RowsPerStrip - 1) / RowsPerStrip);

	cgoffset += TIFF_HEADER_SIZE;
	setTiffHeader1stIFD(cg, cgoffset);
	tiffData = cgoffset + TIF_IFD_SIZE(numTags);

	ptififd = cg + cgoffset;
	memcpy(ptififd, &numTags, sizeof(SHORT));
	setTiffIFDNextOffset(ptififd, numTags, 0);
	setTiffTag(ptififd, 0, TIFFTAG_IMAGEWIDTH,      TIFF_LONG,  1, hsize);
	setTiffTag(ptififd, 1, TIFFTAG_IMAGELENGTH,     TIFF_LONG,  1, vsize);
	setTiffTag(ptififd, 2, TIFFTAG_BITSPERSAMPLE,   TIFF_SHORT, 1, 1);
	setTiffTag(ptififd, 3, TIFFTAG_COMPRESSION,     TIFF_SHORT, 1, COMPRESSION_CCITTFAX4);
	setTiffTag(ptififd, 4, TIFFTAG_PHOTOMETRIC,     TIFF_SHORT, 1, PHOTOMETRIC_MINISWHITE);
	setTiffTag(ptififd, 6, TIFFTAG_SAMPLESPERPIXEL, TIFF_SHORT, 1, 1);
	setTiffTag(ptififd, 7, TIFFTAG_ROWSPERSTRIP,    TIFF_LONG,  1, RowsPerStrip);

	if (stripcnt == 1) {
		setTiffTag(ptififd, 8, TIFFTAG_STRIPBYTECOUNTS, TIFF_SHORT,  stripcnt, 0);
		pstripcnttable = (SHORT *)getTiffTagValueAddr(ptififd, 8);
		setTiffTag(ptififd, 5, TIFFTAG_STRIPOFFSETS,    TIFF_LONG,  stripcnt, 0);
		pstripofftable = (INT *)getTiffTagValueAddr(ptififd, 5);
	}
	else if (stripcnt == 2) {
		setTiffTag(ptififd, 8, TIFFTAG_STRIPBYTECOUNTS, TIFF_SHORT,  stripcnt, 0);
		pstripcnttable = (SHORT *)getTiffTagValueAddr(ptififd, 8);
		setTiffTag(ptififd, 5, TIFFTAG_STRIPOFFSETS,    TIFF_LONG,  stripcnt, tiffData);
		pstripofftable = (INT *) (cg + tiffData);
		tiffData += stripcnt * sizeof(INT);
	}
	else {
		setTiffTag(ptififd, 8, TIFFTAG_STRIPBYTECOUNTS, TIFF_SHORT,  stripcnt, tiffData);
		pstripcnttable = (SHORT *) (cg + tiffData);
		tiffData += stripcnt * sizeof(SHORT);
		setTiffTag(ptififd, 5, TIFFTAG_STRIPOFFSETS,    TIFF_LONG,  stripcnt, tiffData);
		pstripofftable = (INT *) (cg + tiffData);
		tiffData += stripcnt * sizeof(INT);
	}
	runcodeinit();
	dest = cg + tiffData;

/* compress the bytes */
#if OS_WIN32GUI
	bitsptr = bits_end - bitsrowbytes;
#else
	bitsptr = bits;
#endif

	for (row = strip = 0; strip < stripcnt; strip++) {  /* loop each strip */
		deststart = dest;
		*pstripofftable++ = (INT) (dest - cg);

		memset (*Reference, 0x00, hsize * sizeof(SHORT));
		memset (*Current, 0x00, hsize * sizeof(SHORT));
		(*Reference)[0] = (SHORT) -hsize;
		BitsLeftOut = 8;

		for (n = 0; n < RowsPerStrip && row < vsize; n++, row++) {  /* loop each row */
			m = (hsize + 7) / 8;
			if (dest - cg + m * 2 >= cgsize) {
				memfree((UCHAR **)Reference);
				memfree((UCHAR **)Current);
				memfree((UCHAR **)blackbits);
				memfree((UCHAR **)whitebits);
				return RC_NO_MEM;
			}
			rowtotal = 0;
			curindex = 0;
			refindex = 0;
			white = 1;
			BitsLeftIn = 8;
			rowend = bitsptr + bitsrowbytes;
#if OS_WIN32GUI
			nextrow = bitsptr - bitsrowbytes;
#else
			nextrow = bitsptr + bitsrowbytes;
#endif
			passflag = 0;

			while (rowtotal < hsize || passflag) {
				mode = 0;
				if (!passflag)
					runlength = findlength(rowend, white);
				else
					passflag = 0;
				if (rowtotal + runlength > hsize)
					runlength = hsize - rowtotal;
				if (white == 1) {
					while ((*Reference)[refindex] >= -rowtotal || (*Reference)[refindex] >= 0) {
						if ((*Reference)[refindex] == 0 && (*Reference)[refindex + 1] == 0) {
							(*Reference)[refindex] = (SHORT) -hsize;
							break;
						}
						if ((*Reference)[refindex] == 0 && (*Reference)[refindex + 1] < runlength && refindex == 0 && rowtotal == 0)
							break;
						if (runlength == 0 && refindex == 0)
							break;
						if (runlength < 4 && rowtotal == 0 && (*Reference)[refindex] == 0)
							break;
						refindex++;
					}
				}
				else {
					while ((*Reference)[refindex] <= rowtotal || (*Reference)[refindex] <= 0) {
						if ((*Reference)[refindex] == 0 && (*Reference)[refindex + 1] == 0) {
							(*Reference)[refindex] = (SHORT) hsize;
							break;
						}
						refindex++;
					}
				}
				if (white == 1) {
					if ((*Reference)[refindex + 1] < (runlength + rowtotal) && (*Reference)[refindex + 1] != 0)
						mode = PASS;
					else
						diff = (runlength + rowtotal) + (*Reference)[refindex];
				}
				else {
					if ((*Reference)[refindex + 1] > -(rowtotal + runlength) && (*Reference)[refindex + 1] != 0)
						mode = PASS;
					else
						diff = (rowtotal + runlength) - (*Reference)[refindex];
				}
				rowtotal += runlength;
				if (!mode) {
					if (diff < 4 && diff > -4) {
						switch (diff) {
						case 0:
							mode = V0;
							break;
						case 1:
							mode = VR1;
							break;
						case 2:
							mode = VR2;
							break;
						case 3:
							mode = VR3;
							break;
						case -1:
							mode = VL1;
							break;
						case -2:
							mode = VL2;
							break;
						case -3:
							mode = VL3;
							break;
						}
						if (white == 1) {
							(*Current)[curindex++] = -rowtotal;
							white = 0;
						}
						else {
							(*Current)[curindex++] = rowtotal;
							white = 1;
						}
					}
					else
						mode = HORZ;
				}
				switch (mode) {
				case VL3:
					outputcode(5, 2, &dest);
					break;
				case VL2:
					outputcode(4, 2, &dest);
					break;
				case VL1:
					outputcode(1, 2, &dest);
					break;
				case VR3:
					outputcode(5, 3, &dest);
					break;
				case VR2:
					outputcode(4, 3, &dest);
					break;
				case VR1:
					outputcode(1, 3, &dest);
					break;
				case V0:
					outputcode(0, 1, &dest);
					break;
				case HORZ:
					outputcode(2, 1, &dest);
					for (i1 = 0; i1 < 2; i1++) {
						if (i1 == 1) {
							runlength = findlength(rowend, white);
							if (rowtotal + runlength > hsize)
								runlength = hsize - rowtotal;
							rowtotal += runlength;
						}
						for ( ; ; ) {
							if (runlength < 64)
								codeindex = runlength;
							else if (runlength >= 2624)			/* jpr 1/23/01 */
								codeindex = 103;				/* jpr 1/23/01 */
							else
								codeindex = 63 + ((runlength - (runlength & 0x3F)) >> 6);
							if (white == 1) {
								zeroes = (*whitebits)[codeindex].leadzeroes;
								bitvalue = (*whitebits)[codeindex].bitstring;
								outputcode(zeroes, bitvalue, &dest);
							}
							else {
								zeroes = (*blackbits)[codeindex].leadzeroes;
								bitvalue = (*blackbits)[codeindex].bitstring;
								outputcode(zeroes, bitvalue, &dest);
							}
							if (codeindex < 64) break;
							runlength -= (codeindex - 63) << 6;
						}
						if (white == 1) {
							(*Current)[curindex++] = -rowtotal;
							white = 0;
						}
						else {
							(*Current)[curindex++] = rowtotal;
							white = 1;
						}
					}
					break;
				case PASS:
					outputcode(3, 1, &dest);
					if (white == 1) {
						runlength = rowtotal - (*Reference)[refindex + 1];
						rowtotal -= runlength;
					}
					else {
						runlength = rowtotal + (*Reference)[refindex + 1];
						rowtotal -= runlength;
					}
					passflag = 1;
					break;
				}
				if (refindex > 0) refindex--;
			}
			temp = Reference;
			Reference = Current;
			Current = temp;
			memset(*Current, 0x00, hsize * sizeof(SHORT));
			bitsptr = nextrow;
		}         /* for(each row in strip) */

		outputeofb(&dest);

		/* store byte count of strip */
		*pstripcnttable++ = (SHORT) (dest - deststart);
	}
	cgsize = (INT) (ptrdiff_t) (dest - cg);
	memfree((UCHAR **)Reference);
	memfree((UCHAR **)Current);
	memfree((UCHAR **)blackbits);
	memfree((UCHAR **)whitebits);
	return 0;
}

#if OS_WIN32GUI
/* TOTIFN */
static void totifn(UCHAR *cg)  /* convert from 4 or 8 bpp pixmap to tif */
{
	INT k, l, n, row, strip, stripcnt, RowsPerStrip, strip1offset;
	SHORT colormapsize, *pstripcnttable, numTags = 10;
	INT *pstripofftable, cgoffset = 0, tiffData;
	UCHAR *src, *dest, *deststart, *ptififd;

	if (cgsize < 200) {
		cgsize = 0;
		return;
	}
	if (bpp == 4) colormapsize = 96;
	else if (bpp == 8) colormapsize = 1536;
	else {
		cgsize = 0;
		return;
	}

	/* Use 8K strips */
	if (vsize > ((8 / bpp) * 8192)) RowsPerStrip = 1;
	else RowsPerStrip = ((8 / bpp) * 8192) / vsize;
	stripcnt = (INT) floor((vsize + RowsPerStrip - 1) / RowsPerStrip);

	cgoffset += TIFF_HEADER_SIZE;
	setTiffHeader1stIFD(cg, cgoffset);
	tiffData = cgoffset + TIF_IFD_SIZE(numTags);

	ptififd = cg + cgoffset;
	memcpy(ptififd, &numTags, sizeof(SHORT));
	setTiffIFDNextOffset(ptififd, numTags, 0);
	setTiffTag(ptififd, 0, TIFFTAG_IMAGEWIDTH,      TIFF_LONG,  1, hsize);
	setTiffTag(ptififd, 1, TIFFTAG_IMAGELENGTH,     TIFF_LONG,  1, vsize);
	setTiffTag(ptififd, 2, TIFFTAG_BITSPERSAMPLE,   TIFF_SHORT, 1, (SHORT)bpp);
	setTiffTag(ptififd, 3, TIFFTAG_COMPRESSION,     TIFF_SHORT, 1, COMPRESSION_NONE);
	setTiffTag(ptififd, 4, TIFFTAG_PHOTOMETRIC,     TIFF_SHORT, 1, PHOTOMETRIC_PALETTE);
	setTiffTag(ptififd, 6, TIFFTAG_SAMPLESPERPIXEL, TIFF_SHORT, 1, 1);
	setTiffTag(ptififd, 7, TIFFTAG_ROWSPERSTRIP,    TIFF_LONG,  1, RowsPerStrip);
	setTiffTag(ptififd, 9, TIFFTAG_COLORMAP,        TIFF_SHORT, colormapsize / 2, tiffData);

	/* store the color map */
	for (n = 0, k = colormapsize / 6, dest = cg + tiffData; n < k; n++) {
		*(USHORT *) (dest + n * 2)  = ((USHORT) pbmi->bmiColors[n].rgbRed) << 8;
		*(USHORT *) (dest + k * 2 + n * 2) = ((USHORT) pbmi->bmiColors[n].rgbGreen) << 8;
		*(USHORT *) (dest + k * 4 + n * 2) = ((USHORT) pbmi->bmiColors[n].rgbBlue) << 8;
	}

	tiffData += colormapsize;
	if (stripcnt == 1) {
		setTiffTag(ptififd, 8, TIFFTAG_STRIPBYTECOUNTS, TIFF_SHORT, stripcnt, 0);
		setTiffTag(ptififd, 5, TIFFTAG_STRIPOFFSETS, TIFF_LONG, stripcnt, 0);
		pstripcnttable = (SHORT *)getTiffTagValueAddr(ptififd, 8);
		pstripofftable = (INT *)getTiffTagValueAddr(ptififd, 5);
	}
	else if (stripcnt == 2) {
		setTiffTag(ptififd, 5, TIFFTAG_STRIPOFFSETS, TIFF_LONG, stripcnt, tiffData);
		pstripofftable = (INT *) (cg + tiffData);
		tiffData += stripcnt * sizeof(INT);
		setTiffTag(ptififd, 8, TIFFTAG_STRIPBYTECOUNTS, TIFF_SHORT, stripcnt, 0);
		pstripcnttable = (SHORT *)getTiffTagValueAddr(ptififd, 8);
	}
	else {
		setTiffTag(ptififd, 8, TIFFTAG_STRIPBYTECOUNTS, TIFF_SHORT, stripcnt, tiffData);
		pstripcnttable = (SHORT *) (cg + tiffData);
		tiffData += stripcnt * sizeof(SHORT);
		setTiffTag(ptififd, 5, TIFFTAG_STRIPOFFSETS, TIFF_LONG, stripcnt, tiffData);
		pstripofftable = (INT *) (cg + tiffData);
		tiffData += stripcnt * sizeof(INT);
	}
	strip1offset = tiffData;

/* store the bytes */
	src = bits_end - bitsrowbytes;
	dest = cg + strip1offset;
	if (bpp == 4) k = hsize / 2;  /* k is bytes per row in dest */
	else k = hsize;
	for (row = strip = 0; strip < stripcnt; strip++) {  /* loop each strip */
		deststart = dest;
		*pstripofftable++ = (INT) (dest - cg);
		for (l = 0; l < RowsPerStrip && row < vsize; l++, row++) {  /* loop each row */
			if (dest - cg + k >= cgsize) {
				cgsize = 0;
				return;
			}
			memcpy(dest, src, k);
			dest += k;
			src -= bitsrowbytes;
		}
		*pstripcnttable++ = (SHORT) (dest - deststart);
	}
	cgsize = (INT) (ptrdiff_t) (dest - cg);
	return;
}
#endif


/* TOTIF24 */
static INT totif24(UCHAR *cg)  /* convert from 24 bpp pixmap to tif */
{
	INT k, l, m, row, strip, stripcnt, RowsPerStrip, strip1offset;
	SHORT *pstripcnttable;
	INT *pstripofftable, numTags = 9, tiffData;
	UCHAR *src, *dest, *deststart, *oldbitsptr, *ptififd;


	if (cgsize < 200) {
		cgsize = 0;
		return 0;
	}

	/* Use 8K strips */
	if ((vsize * 3) > 8192) RowsPerStrip = 1;
	else RowsPerStrip = 8192 / (vsize * 3);
	stripcnt = (INT) floor((vsize + RowsPerStrip - 1) / RowsPerStrip);
	setTiffHeader1stIFD(cg, TIFF_HEADER_SIZE);
	tiffData = TIFF_HEADER_SIZE + TIF_IFD_SIZE(numTags);

	ptififd = cg + TIFF_HEADER_SIZE;
	memcpy(ptififd, &numTags, sizeof(SHORT));
	setTiffIFDNextOffset(ptififd, numTags, 0);
	setTiffTag(ptififd, 0, TIFFTAG_IMAGEWIDTH,      TIFF_LONG,  1, hsize);
	setTiffTag(ptififd, 1, TIFFTAG_IMAGELENGTH,     TIFF_LONG,  1, vsize);
	setTiffTag(ptififd, 2, TIFFTAG_BITSPERSAMPLE,   TIFF_SHORT, 3, tiffData);
	setTiffTag(ptififd, 3, TIFFTAG_COMPRESSION,     TIFF_SHORT, 1, COMPRESSION_NONE);
	setTiffTag(ptififd, 4, TIFFTAG_PHOTOMETRIC,     TIFF_SHORT, 1, PHOTOMETRIC_RGB);
	setTiffTag(ptififd, 6, TIFFTAG_SAMPLESPERPIXEL, TIFF_SHORT, 1, 3);
	setTiffTag(ptififd, 7, TIFFTAG_ROWSPERSTRIP,    TIFF_LONG,  1, RowsPerStrip);

	dest = cg + tiffData;
	* (SHORT *) (dest) = 8;
	dest += sizeof(SHORT);
	* (SHORT *) (dest) = 8;
	dest += sizeof(SHORT);
	* (SHORT *) (dest) = 8;
	//dest += sizeof(SHORT); Adjusting pointer is 'pointless' at this point!!
	tiffData += 3 * sizeof(SHORT);

	if (stripcnt == 1) {
		setTiffTag(ptififd, 5, TIFFTAG_STRIPOFFSETS, TIFF_LONG, stripcnt, 0);
		setTiffTag(ptififd, 8, TIFFTAG_STRIPBYTECOUNTS, TIFF_SHORT, stripcnt, 0);
		pstripcnttable = (SHORT *)getTiffTagValueAddr(ptififd, 8);
		pstripofftable = (INT *)getTiffTagValueAddr(ptififd, 5);
	}
	else if (stripcnt == 2) {
		setTiffTag(ptififd, 5, TIFFTAG_STRIPOFFSETS, TIFF_LONG, stripcnt, tiffData);
		pstripofftable = (INT *) (cg + tiffData);
		tiffData += stripcnt * sizeof(INT);
		setTiffTag(ptififd, 8, TIFFTAG_STRIPBYTECOUNTS, TIFF_SHORT, stripcnt, 0);
		pstripcnttable = (SHORT *)getTiffTagValueAddr(ptififd, 8);
	}
	else {
		setTiffTag(ptififd, 8, TIFFTAG_STRIPBYTECOUNTS, TIFF_SHORT, stripcnt, tiffData);
		pstripcnttable = (SHORT *) (cg + tiffData);
		tiffData += stripcnt * sizeof(SHORT);
		setTiffTag(ptififd, 5, TIFFTAG_STRIPOFFSETS, TIFF_LONG, stripcnt, tiffData);
		pstripofftable = (INT *) (cg + tiffData);
		tiffData += stripcnt * sizeof(INT);
	}
	strip1offset = tiffData;

/* store the bytes */
#if OS_WIN32GUI
	src = bits_end - bitsrowbytes;
#else
	src = bits;
#endif
	dest = cg + strip1offset;
	k = hsize * 3;  /* k is bytes per row in dest */
	for (row = strip = 0; strip < stripcnt; strip++) {  /* loop each strip */
		deststart = dest;
		*pstripofftable++ = (INT) (dest - cg);
		for (l = 0; l < RowsPerStrip && row < vsize; l++, row++) {  /* loop each row */
			if (dest - cg + k >= cgsize) {
				cgsize = 0;
				return RC_NO_MEM;
			}
			oldbitsptr = src;
			for (m = 0; m < hsize; m++) {
#if OS_WIN32GUI
				dest[0] = src[2];
				dest[1] = src[1];
				dest[2] = src[0];
				dest += 3;
				src += 3;
#else
				*dest++ = *src++;
				*dest++ = *src++;
				*dest++ = *src++;
#endif
			}
#if OS_WIN32GUI
			src = oldbitsptr - bitsrowbytes;
#else
			src = oldbitsptr + bitsrowbytes;
#endif
		}
		*pstripcnttable++ = (SHORT) (dest - deststart);
	}
	cgsize = (INT) (ptrdiff_t) (dest - cg);
	return 0;
}

/* JPG Resolution */
INT guiaJpgRes(UCHAR *buffer, INT *xres, INT *yres)
{
	struct jpeg_decompress_struct dinfo;
	JPEGERRORMGR jerr;

	*xres = 0;
	*yres = 0;

	dinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpegerrorexit;
	if (setjmp(jerr.jpegsetjumpbuffer)){
		jpeg_destroy_decompress(&dinfo);
		return  RC_ERROR;
	}

	jpeg_create_decompress(&dinfo);
	jpegsetupsource(&dinfo, buffer);
	jpeg_read_header(&dinfo, TRUE);

	*xres = dinfo.X_density;
	*yres = dinfo.Y_density;
	if (dinfo.density_unit == 2) {
		/* convert from centimeters to inches */
		*xres = (INT) ((double) *xres / 2.54);
		*yres = (INT) ((double) *yres / 2.54);
	}

	jpeg_destroy_decompress(&dinfo);
	if (*xres == 1 && *yres == 1) { /* 1:1 ratio */
		*xres = 0;
		*yres = 0;
	}

	if (*xres < 0 || *yres < 0) return RC_ERROR;

	return(0);
}

/* JPG BPP */
INT guiaJpgBPP(UCHAR *buffer, INT *x)
{
	struct jpeg_decompress_struct dinfo;
	JPEGERRORMGR jerr;

	dinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpegerrorexit;
	if (setjmp(jerr.jpegsetjumpbuffer)){
		jpeg_destroy_decompress(&dinfo);
		return  RC_ERROR;
	}

	jpeg_create_decompress(&dinfo);
	jpegsetupsource(&dinfo, buffer);
	jpeg_read_header(&dinfo, TRUE);

	*x = dinfo.num_components * 8;
	if (!*x) *x = 24;

	jpeg_destroy_decompress(&dinfo);
	return(0);
}

/* JPG SIZE */
INT guiaJpgSize(UCHAR *buffer, INT *x, INT *y)
{
	struct jpeg_decompress_struct dinfo;
	JPEGERRORMGR jerr;

	dinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpegerrorexit;
	if (setjmp(jerr.jpegsetjumpbuffer)){
		jpeg_destroy_decompress(&dinfo);
		return  RC_ERROR;
	}

	jpeg_create_decompress(&dinfo);
	jpegsetupsource(&dinfo, buffer);
	jpeg_read_header(&dinfo, TRUE);

	*x = dinfo.image_width;
	*y = dinfo.image_height;
	jpeg_destroy_decompress(&dinfo);

	if (!*x && !*y) return RC_ERROR;
	return(0);
}

/* GIF BPP */
/* CODE: There is a flaw in this, it assumes that all GIFs have a Global Color Table,
 * the spec does not require this. If a Local Color Table is used then this would return
 * the wrong answer */
INT guiaGifBPP(UCHAR *cg, INT *x)
{
	UCHAR flags;

	/*cg = buffer;*/
	*x = 0;

	if ((CHAR) *cg != 'G' && (CHAR) *(cg + 1) != 'I' && (CHAR) *(cg + 2) != 'F') {
		return(-3);  /* check to see if gif file, return error if not */
	}
	cg += 10;
	flags = *cg;  /* get global flags */
	flags &= 0x07; /* mask off right 3 bits, bits per pixel */
	*x = ((INT) flags) + 1;

	if (!*x) *x = 24;

	return(0);
}

/* GIF SIZE */
INT guiaGifSize(UCHAR *cg, INT *x, INT *y)
{
	INT colormaplen;
	UCHAR flags;

	/*cg = buffer;*/
	*x = 0;
	*y = 0;

	if ((CHAR) *cg != 'G' && (CHAR) *(cg + 1) != 'I' && (CHAR) *(cg + 2) != 'F') {
		return(-3);  /* check to see if gif file, return error if not */
	}
	cg += 10;
	flags = *cg;  /* get global flags */
	cg += 3;
	if (flags & 0x80) {  /* test if there is a global color map */
		colormaplen = 3 * (1 << ((flags & 7) + 1)); /* bytes of colormap */
		cg = cg + colormaplen;
	}

	while (*cg == ',' || *cg == '!' || *cg == ';') {    /* loop to read image blocks */
		if (*cg == ',') {
			cg += 5;
			*x = ((USHORT) (*(cg + 1)) << 8) + (USHORT) *(cg);
			*y = ((USHORT) (*(cg + 3)) << 8) + (USHORT) (*(cg + 2));
			break;
		}
		else if (*cg == '!') extensionGif(&cg);
		else return RC_ERROR; /* EOF */
	}

	if (!*x && !*y) return RC_ERROR;
	return(0);
}

/**
 * TIFF Resolution
 *
 * return resolution in inches in xres and yres
 */
INT guiaTifRes(INT imgnum, UCHAR *cg, INT *xres, INT *yres)
{
	UINT i1, i2, stripnum, type, length, offset, units;
	double x, y;
	UCHAR *p;

	/*cg = buffer;*/

	units = 0;
	x = 0;
	y = 0;

	if (TIF_MACHINE_TYPE() == *(USHORT *) cg) swapflag = FALSE;
	else swapflag = TRUE;

	p = cg + getint(cg + 4);  /* pointer to IFD */

	while (--imgnum > 0) {
		stripnum = getshort(p);  /* number of tag fields */
		for (p += 2; stripnum--; p += 12) ; /* skip tag fields */
		i1 = getint(p);
		if (i1 == 0) break;
		p = cg + i1; /* get 4 byte next IFD offset */
	}

	stripnum = getshort(p);  /* number of tag fields */

	for (p += 2; stripnum--; p += 12) {
		type = getshort(p + 2);
		length = getint(p + 4);
		if (length > 1 || type == 4 || type == 5) {
			offset = getint(p + 8);
		}
		else if (type == 3) {
			offset = getshort(p + 8);
		}
		else {
			offset = *(p + 8);
		}
		switch (getshort(p)) {
		case TIFFTAG_RESOLUTIONUNIT :  /* ResolutionUnit */
			units = offset;
			break;
		case TIFFTAG_XRESOLUTION: /* XResolution */
			i1 = getint(cg + offset);
			i2 = getint(cg + offset + 4);
			if (i2) x = i1 / i2;
			break;
		case TIFFTAG_YRESOLUTION: /* YResolution */
			i1 = getint(cg + offset + 4);
			if (i1) y = getint(cg + offset) / i1;
			break;
		}
	}

	if (units == 3) {
		/* convert from centimeters to inches */
		x = x / 2.54;
		y = y / 2.54;
	}
	if (x == 1) x = 0; /* be consistant with other image types */
	if (y == 1) y = 0;
	*xres = (INT) x;
	*yres = (INT) y;

	if (*xres < 0 ||  *yres < 0) return RC_ERROR;
	return(0);
}

/**
 * TIFF BPP
 */
INT guiaTifBPP(INT imgnum, UCHAR *cg, INT *x)
{
	INT i1, stripnum, type, length, offset;
	INT bps, spp;
	UCHAR *p;

	/*cg = buffer;*/
	*x = 0;
	bps = 1;	/* Per tiff spec 6.0 dtd 03 JUN 92, page 29, the default is one. */
	spp = 1;

	if (TIF_MACHINE_TYPE() == *(USHORT *) cg) swapflag = FALSE;
	else swapflag = TRUE;

	p = cg + getint(cg + 4);  /* pointer to IFD */

	while (--imgnum > 0) {
		stripnum = getshort(p);  /* number of tag fields */
		for (p += 2; stripnum--; p += 12) ; /* skip tag fields */
		i1 = getint(p);
		if (i1 == 0) break;
		p = cg + i1; /* get 4 byte next IFD offset */
	}

	stripnum = getshort(p);  /* number of tag fields */

	for (p += 2; stripnum--; p += 12) {
		type = getshort(p + 2);
		length = getint(p + 4);
		if (length > 1 || type == 4) offset = getint(p + 8);
		else if (type == 3) offset = getshort(p + 8);
		else offset = *(p + 8);
		switch (getshort(p)) {
		case TIFFTAG_BITSPERSAMPLE:  /* BitsPerSample */
			if (length == 3) bps = getshort(cg + getint(p + 8));
			else bps = offset;
			break;
		case TIFFTAG_SAMPLESPERPIXEL:  /* SamplesPerPixel */
			spp = offset;
			break;
		}
	}

	if (!bps) return RC_ERROR;

	*x = bps * spp;
	return(0);
}

/* TIFF IMAGE COUNT */
INT guiaTifImageCount(UCHAR *cg, INT *x)
{
	INT i1, stripnum;
	INT count;
	UCHAR *p;

	/*cg = buffer;*/

	if (TIF_MACHINE_TYPE() == *(USHORT *) cg) swapflag = FALSE;
	else swapflag = TRUE;

	p = cg + getint(cg + 4);  /* pointer to IFD */

	for (count = 1; ; count++) {
		stripnum = getshort(p);  /* number of tag fields */
		for (p += 2; stripnum--; p += 12) ; /* skip tag fields */
		i1 = getint(p);
		if (i1 == 0) break;
		p = cg + i1; /* get 4 byte next IFD offset */
	}

	*x = count;
	return(0);
}

/* TIFF SIZE */
INT guiaTifSize(INT imgnum, UCHAR *cg, INT *x, INT *y)
{
	INT i1, stripnum, type, length, offset;
	UCHAR *p;

	/*cg = buffer;*/
	*x = 0;
	*y = 0;

	if (TIF_MACHINE_TYPE() == *(USHORT *) cg) swapflag = FALSE;
	else swapflag = TRUE;

	p = cg + getint(cg + 4);  /* pointer to IFD */

	while (--imgnum > 0) {
		stripnum = getshort(p);  /* number of tag fields */
		for (p += 2; stripnum--; p += 12) ; /* skip tag fields */
		i1 = getint(p);
		if (i1 == 0) break;
		p = cg + i1; /* get 4 byte next IFD offset */
	}

	stripnum = getshort(p);  /* number of tag fields */

	for (p += 2; stripnum--; p += 12) {
		type = getshort(p + 2);
		length = getint(p + 4);
		if (length > 1 || type == 4) offset = getint(p + 8);
		else if (type == 3) offset = getshort(p + 8);
		else offset = *(p + 8);
		switch (getshort(p)) {
		case TIFFTAG_IMAGEWIDTH:  /* width */
			*x = offset;
			break;
		case TIFFTAG_IMAGELENGTH:  /* height */
			*y = offset;
			break;
		}
	}
	if (!*x && !*y) return RC_ERROR;
	return(0);
}

/*
 * FROMTIF
 *
 * cghandle is a memalloc handle to the compressed graphics buffer
 */
static INT fromtif(PIXHANDLE pixhandle, UCHAR **cghandle)  /* handle initial convert from tif processing */
{
	INT colorpos, colorval, colorspan, retval, stripnum;
	INT i1;
	UCHAR *p;
	CHAR *ptr;
	INT tag;            /* field tag */
	INT type;           /* field numeric type */
	INT count;			/* The number of values, 'count' of the indicated type*/
	UINT offset;		/* field offset */
	UINT BitsPerSample;
	INT colormaplen, colormapoff;
	UINT stripcount = UINT_MAX;
	INT stripoffsetstype, stripoffsets;
	UINT stripbytecountstype, stripbytecounts;
	INT mapentries;
	INT imgnum = (*pixhandle)->tifnum;
#if OS_WIN32GUI
	INT deltar, deltag, deltab;
	INT clrdist, minclrdist;
	INT bestclr, palcolor;
	USHORT red, green, blue;
#endif

	if (TIF_MACHINE_TYPE() == *((USHORT *) (*cghandle))) swapflag = FALSE;
	else swapflag = TRUE;

	if (!prpget("gui", "bwtiff", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "old")) oldbwtiff = TRUE;
	else oldbwtiff = FALSE;

	Photometric = PHOTOMETRIC_MINISWHITE;		/* default */
	if (oldbwtiff) tifxchgbw = 0;
	else tifxchgbw = 0xff;

	Predictor = 1;
	TwoDim = 0;
	UncAllow = 0;
	maskin = t1mask;	/* fill order */
	colormapoff = -1;
	/* set some defaults because bilevel images don't always define these - SSN */
	BitsPerSample = SamplesPerPixel = 1;
	Compression = COMPRESSION_NONE;
	RowsPerStrip = UINT_MAX;		/* default value is 2**32 - 1 */

	p = *cghandle + getint(*cghandle + 4);  /* ptr to IFD */

	while (--imgnum > 0) {
		stripnum = getshort(p);  /* number of tag fields */
		for (p += 2; stripnum--; p += 12) ; /* skip tag fields */
		i1 = getint(p);
		if (i1 == 0) break;
		p = *cghandle + i1; /* get 4 byte next IFD offset */
	}

	stripnum = getshort(p);  /* number of tag fields */

	for (p += 2; stripnum--; p += 12) {
		tag = getshort(p);
		type = getshort(p + 2);
		count = getint(p + 4);
		if (count > 1 || type == 4) offset = getint(p + 8);
		else if (type == 3) offset = getshort(p + 8);
		else offset = *(p + 8);
		switch (tag) {
		case TIFFTAG_IMAGEWIDTH:  /* ImageWidth */
			tifimagewidth = offset;
			break;
		case TIFFTAG_IMAGELENGTH:  /* ImageLength */
			tifimagelength = offset;
			break;
		case TIFFTAG_BITSPERSAMPLE:  /* BitsPerSample */
			if (count == 3) BitsPerSample = getshort(*cghandle + getint(p + 8));
			else BitsPerSample = offset;
			break;
		case TIFFTAG_COMPRESSION:  /* Compression */
			Compression = offset;
			break;
		case TIFFTAG_PHOTOMETRIC:  /* PhotometricInterpretation */
			Photometric = offset;
			if (Photometric == PHOTOMETRIC_MINISBLACK) {
				if (oldbwtiff) tifxchgbw = 0xff;
				else tifxchgbw = 0;
			}
			break;
		case TIFFTAG_FILLORDER:      /* Fill Order */
			if (offset == 2) maskin = t2mask;
			break;
		case TIFFTAG_STRIPOFFSETS:  /* StripOffsets */
			stripoffsetstype = type;
			stripcount = count;
			if (count == 1 || (count == 2 && type == 3)) stripoffsets = (INT) (ptrdiff_t) (p + 8 - *cghandle);
			else stripoffsets = offset;
			break;
		case TIFFTAG_SAMPLESPERPIXEL:  /* SamplesPerPixel */
			SamplesPerPixel = offset;
			break;
		case TIFFTAG_ROWSPERSTRIP:  /* RowsPerStrip */
			RowsPerStrip = offset;
			break;
		case TIFFTAG_STRIPBYTECOUNTS:
			stripbytecountstype = type;
			if (count == 1 || (count == 2 && type == 3)) stripbytecounts = (UINT) (ptrdiff_t) (p + 8 - *cghandle);
			else stripbytecounts = offset;
			break;
		case TIFFTAG_GROUP3OPTIONS: /* new name: T4 Options */
			if (offset & 0x00000001) TwoDim = 1;
			break;
		case TIFFTAG_GROUP4OPTIONS: /* new name: T6 Options */
			UncAllow = (SHORT) (offset | 0x0002);
			break;
		case TIFFTAG_PREDICTOR: /* LZW Predictor */
			if (offset == 2) {
				Predictor = 2;
			}
			break;
		case TIFFTAG_COLORMAP:  /* ColorMap */
			colormaplen = count;
			colormapoff = offset;
			break;
		}
	}

	/**
	 * The Tiff tag StripOffsets is required
	 * This happens if it is missing
	 */
	if (stripcount == UINT_MAX) return RC_INVALID_PARM;

	if (bpp == 1 || (BitsPerSample == 1
		&& Compression != COMPRESSION_NONE
		&& Compression != COMPRESSION_LZW
		&& Compression != COMPRESSION_PACKBITS))
	{
		if (SamplesPerPixel != 1 || BitsPerSample != 1) return RC_NO_MEM;
		if (Compression != COMPRESSION_NONE
			&& Compression != COMPRESSION_CCITTRLE
			&& Compression != COMPRESSION_LZW
			&& Compression != COMPRESSION_CCITTFAX3
			&& Compression != COMPRESSION_CCITTFAX4
			&& Compression != COMPRESSION_PACKBITS)
		{
			return RC_NAME_TOO_LONG;
		}
#if OS_WIN32GUI
		if (pLogPal) {
			pLogPal->palPalEntry[0].peRed = 0;
			pLogPal->palPalEntry[0].peGreen = 0;
			pLogPal->palPalEntry[0].peBlue = 0;
			pLogPal->palPalEntry[0].peFlags = 0;
			pLogPal->palPalEntry[1].peRed = 255;
			pLogPal->palPalEntry[1].peGreen = 255;
			pLogPal->palPalEntry[1].peBlue = 255;
			pLogPal->palPalEntry[1].peFlags = 0;
		}
#endif
		if (Compression == COMPRESSION_LZW || Compression == COMPRESSION_PACKBITS) {
			uncmpstrip = memalloc(userTifMaxstripsize > UNCMPSIZE ? userTifMaxstripsize : UNCMPSIZE, 0);
			if (uncmpstrip == NULL) return RC_NO_MEM;
		}
		if (Compression == COMPRESSION_LZW) {
			if ((table = memalloc(4096, 0)) == NULL)
				return RC_NO_MEM;
			if ((tableprefix = (SHORT **) memalloc(4096 * sizeof(SHORT), 0)) == NULL)
				return RC_NO_MEM;
		}
		if (Compression == COMPRESSION_CCITTRLE
			|| Compression == COMPRESSION_CCITTFAX3 || Compression == COMPRESSION_CCITTFAX4)
		{
			if ((WCode = (SHORT **) memalloc(4200 * sizeof(SHORT), 0)) == NULL)
				return RC_NO_MEM;
			if ((BCode = (SHORT **) memalloc(8400 * sizeof(SHORT), 0)) == NULL)
				return RC_NO_MEM;
			tcodeinit();
		}
		if ((Compression == COMPRESSION_CCITTFAX3 && TwoDim == 1)
			|| Compression == COMPRESSION_CCITTFAX4)
		{
			if ((Reference = (SHORT **) memalloc(tifimagewidth * sizeof(SHORT), 0)) == NULL)
				return RC_NO_MEM;
			memset (*Reference, 0x00, tifimagewidth * sizeof(SHORT));
			if ((Current = (SHORT **) memalloc(tifimagewidth * sizeof(SHORT), 0)) == NULL)
				return RC_NO_MEM;
			memset (*Current, 0x00, tifimagewidth * sizeof(SHORT));
		}
		for (stripnum = 0; (UINT)stripnum < stripcount; stripnum++) {
			if (stripoffsetstype == 3) {
				tifstrip = *cghandle + getshort(*cghandle + stripoffsets + stripnum * 2);
			}
			else tifstrip = *cghandle + getint(*cghandle + stripoffsets + stripnum * 4);
			if (stripbytecountstype == 3) {
				tifstripsize = getshort(*cghandle + stripbytecounts + stripnum * 2);
			}
			else tifstripsize = getint(*cghandle + stripbytecounts + stripnum * 4);
			if (Compression == COMPRESSION_NONE) {
				retval = fromtif1();
			}
			else if (Compression == COMPRESSION_LZW) {
				retval = fromtif11L();
			}
			else if (Compression == COMPRESSION_CCITTFAX3 && TwoDim == 0) {
				retval = fromtif13();
			}
			else if (Compression == COMPRESSION_CCITTRLE) {
				retval = fromtif13_RLE();
			}
			else if (Compression == COMPRESSION_CCITTFAX3) {
				retval = fromtif2();
			}
			else if (Compression == COMPRESSION_CCITTFAX4) {
				retval = fromtif14();
			}
			else {
				retval = fromtif3();
			}
			if (retval) {
				if (Compression == COMPRESSION_LZW || Compression == COMPRESSION_PACKBITS)
					memfree(uncmpstrip);
				if (Compression == COMPRESSION_LZW) {
					memfree((UCHAR **)table);
					memfree((UCHAR **)tableprefix);
				}
				if (Compression == COMPRESSION_CCITTRLE
					|| Compression == COMPRESSION_CCITTFAX3 || Compression == COMPRESSION_CCITTFAX4)
				{
					memfree((UCHAR **)WCode);
					memfree((UCHAR **)BCode);
				}
				if ((Compression == COMPRESSION_CCITTFAX3 && TwoDim == 1)
					|| Compression == COMPRESSION_CCITTFAX4)
				{
					memfree((UCHAR **)Reference);
					memfree((UCHAR **)Current);
				}
				return(retval);
			}
		}
		if (Compression == COMPRESSION_LZW || Compression == COMPRESSION_PACKBITS)
			memfree((UCHAR **)uncmpstrip);
		if (Compression == COMPRESSION_LZW) {
			memfree((UCHAR **)table);
			memfree((UCHAR **)tableprefix);
		}
		if (Compression == COMPRESSION_CCITTRLE
			|| Compression == COMPRESSION_CCITTFAX3 || Compression == COMPRESSION_CCITTFAX4)
		{
			memfree((UCHAR **)WCode);
			memfree((UCHAR **)BCode);
		}
		if ((Compression == COMPRESSION_CCITTFAX3 && TwoDim == 1)
			|| Compression == COMPRESSION_CCITTFAX4)
		{
			memfree((UCHAR **)Reference);
			memfree((UCHAR **)Current);
		}
		return(0);
	}
	if (Compression != COMPRESSION_NONE && Compression != COMPRESSION_LZW
		&& Compression != COMPRESSION_PACKBITS)
		return(-4);
	if (SamplesPerPixel == 3) {
		if (bpp != 24 || BitsPerSample != 8) return(-3);
	}
	else if (SamplesPerPixel == 1) {
		if (colormapoff == -1) {  /* no ColorMap specified */
			if (BitsPerSample != 1) {
				/* Build the gray scale color table based on BitsPerSample */
				if (BitsPerSample == 4 || bpp == 4) colorspan = 16;
				else if (BitsPerSample == 8) colorspan = 1;
				else return RC_INVALID_PARM;
				/* otherwise try loading as a Gray Scale bitmap */
				if (Photometric == PHOTOMETRIC_MINISBLACK) {
					for (colorpos = 0, colorval = 0; colorval <= 255; colorval += colorspan) {
						if (bpp == 24) {
							colors[colorpos++] = colorval;
							colors[colorpos++] = colorval;
							colors[colorpos++] = colorval;
						}
						else {
#if OS_WIN32GUI
							pLogPal->palPalEntry[colorpos].peRed = colorval;
							pLogPal->palPalEntry[colorpos].peGreen = colorval;
							pLogPal->palPalEntry[colorpos].peBlue = colorval;
							pLogPal->palPalEntry[colorpos].peFlags = 0;
#endif
							colors[colorpos] = colorpos;
							colorpos++;
						}
					}
				}
				else {
					for (colorpos = 0, colorval = 255; colorval >= 0; colorval -= colorspan) {
						if (bpp == 24) {
							colors[colorpos++] = colorval;
							colors[colorpos++] = colorval;
							colors[colorpos++] = colorval;
						}
						else {
#if OS_WIN32GUI
							pLogPal->palPalEntry[colorpos].peRed = colorval;
							pLogPal->palPalEntry[colorpos].peGreen = colorval;
							pLogPal->palPalEntry[colorpos].peBlue = colorval;
							pLogPal->palPalEntry[colorpos].peFlags = 0;
#endif
							colors[colorpos] = colorpos;
							colorpos++;
						}
					}
				}
			}
#if OS_WIN32GUI
			else if (pLogPal) {
				pLogPal->palPalEntry[0].peRed = 0;
				pLogPal->palPalEntry[0].peGreen = 0;
				pLogPal->palPalEntry[0].peBlue = 0;
				pLogPal->palPalEntry[0].peFlags = 0;
				pLogPal->palPalEntry[1].peRed = 255;
				pLogPal->palPalEntry[1].peGreen = 255;
				pLogPal->palPalEntry[1].peBlue = 255;
				pLogPal->palPalEntry[1].peFlags = 0;
			}
#endif
		}
		else {
			mapentries = checkmap(colormaplen / 3, *cghandle + colormapoff);


			for (colorpos = 0, colorval = (colormaplen * 2) / 3, p = *cghandle + colormapoff; colormaplen; colormaplen -= 3) {
				if (bpp == 24) {
					if (mapentries == 16) {
						colors[colorpos++] = getshort(p) >> 8;
						colors[colorpos++] = getshort(p + colorval) >> 8;
						colors[colorpos++] = getshort(p + colorval + colorval) >> 8;
					}
					else {
						colors[colorpos++] = (UCHAR)getshort(p);
						colors[colorpos++] = (UCHAR)getshort(p + colorval);
						colors[colorpos++] = (UCHAR)getshort(p + colorval + colorval);
					}
				}
				else {
#if OS_WIN32GUI
					if (colorpos >= numcolors) {
						if (mapentries == 16) {
							red = getshort(p) >> 8;
							green = getshort(p + colorval) >> 8;
							blue = getshort(p + colorval + colorval) >> 8;
						}
						else {
							red = getshort(p);
							green = getshort(p + colorval);
							blue = getshort(p + colorval + colorval);
						}
						minclrdist = INT_MAX;
						for (palcolor = 0; palcolor < numcolors; palcolor++) {
							deltar = pLogPal->palPalEntry[palcolor].peRed - red;
							deltag = pLogPal->palPalEntry[palcolor].peGreen - green;
							deltab =  pLogPal->palPalEntry[palcolor].peBlue - blue;
							clrdist = deltar * deltar + deltag * deltag + deltab * deltab;
							if (clrdist < minclrdist) {
								minclrdist = clrdist;
								bestclr = palcolor;
							}
						}
						colors[colorpos] = bestclr;
						colorpos++;
					}
					else {
						if (mapentries == 16) {
							pLogPal->palPalEntry[colorpos].peRed = getshort(p) >> 8;
							pLogPal->palPalEntry[colorpos].peGreen = getshort(p + colorval) >> 8;
							pLogPal->palPalEntry[colorpos].peBlue = getshort(p + colorval + colorval) >> 8;
							pLogPal->palPalEntry[colorpos].peFlags = 0;
						}
						else {
							pLogPal->palPalEntry[colorpos].peRed = (UCHAR)getshort(p);
							pLogPal->palPalEntry[colorpos].peGreen = (UCHAR)getshort(p + colorval);
							pLogPal->palPalEntry[colorpos].peBlue = (UCHAR)getshort(p + colorval + colorval);
							pLogPal->palPalEntry[colorpos].peFlags = 0;
						}
						colors[colorpos] = colorpos;
						colorpos++;
					}
#else
#if OS_MAC
					rgb.red = * (USHORT *) p;
					rgb.green = * (USHORT *) (p + colorval);
					rgb.blue = * (USHORT *) (p + colorval + colorval);
					colors[colorpos++] = (UCHAR) Color2Index(&rgb);
#endif
#endif
				}
				p += 2;			/* A tiff color map uses two bytes for each entry, i.e. 48 bit color! */
			}
		}
	}
	else return RC_NO_MEM;       /* This does not sound right? */

	if (Compression == COMPRESSION_LZW || Compression == COMPRESSION_PACKBITS) {
		uncmpstrip = memalloc(userTifMaxstripsize > UNCMPSIZE ? userTifMaxstripsize : UNCMPSIZE, 0);
		if (uncmpstrip == NULL) return RC_NO_MEM;
	}
	if (Compression == COMPRESSION_LZW) {
		if ((table = memalloc(4096, 0)) == NULL)
			return RC_NO_MEM;
		if ((tableprefix = (SHORT **) memalloc(4096 * sizeof(SHORT), 0)) == NULL)
			return RC_NO_MEM;
	}

	/*
	 * Need to fix up some things here in case the above memalloc calls caused compaction
	 */
#if !OS_WIN32GUI
	bitsptr = bits = *((*pixhandle)->pixbuf);
#endif
	for (stripnum = 0; (UINT)stripnum < stripcount; stripnum++) {
		if (stripoffsetstype == TIFF_SHORT) {
			tifstrip = *cghandle + getshort(*cghandle + stripoffsets + stripnum * sizeof(USHORT));
		}
		else tifstrip = *cghandle + getint(*cghandle + stripoffsets + stripnum * sizeof(UINT));

		if (stripbytecountstype == TIFF_SHORT) {
			tifstripsize = getshort(*cghandle + stripbytecounts + stripnum * sizeof(USHORT));
		}
		else
			tifstripsize = getint(*cghandle + stripbytecounts + stripnum * sizeof(UINT));

		retval = -3;

		if (bpp == 4) {
			if (BitsPerSample == 4) retval = fromtif5();
			else if (BitsPerSample == 1) retval = fromtif10();
		}
		else if (bpp == 8) {
			if (BitsPerSample == 4) retval = fromtif6();
			else if (BitsPerSample == 8) retval = fromtif8();
			else if (BitsPerSample == 1) retval = fromtif11();
		}
		else if (bpp == 24) {
			if (SamplesPerPixel == 3) retval = fromtif4();
			else if (BitsPerSample == 4) retval = fromtif7();
			else if (BitsPerSample == 8) retval = fromtif9();
			else if (BitsPerSample == 1) retval = fromtif12();
		}
		if (retval) {
			if (Compression == COMPRESSION_LZW || Compression == COMPRESSION_PACKBITS)
				memfree((UCHAR **)uncmpstrip);
			if (Compression == COMPRESSION_LZW) {
				memfree((UCHAR **)table);
				memfree((UCHAR **)tableprefix);
			}
			return(retval);
		}
	}
	if (Compression == COMPRESSION_LZW || Compression == COMPRESSION_PACKBITS)
		memfree((UCHAR **)uncmpstrip);
	if (Compression == COMPRESSION_LZW) {
		memfree((UCHAR **)table);
		memfree((UCHAR **)tableprefix);
	}
	return(0);
}

/* FROMTIF11L */
static INT fromtif11L(void) /* convert 1 bpp LZW tiff strip to 1 bpp pixmap */
{
	UCHAR *srcptr;
	if ((srcptr = uncmp()) == NULL) return(0);
	tifstripsize = uncmpstripsize;
	tifstrip = srcptr;
	return fromtif1();
}

/*
 * FROMTIF1
 *
 * convert 1 bpp uncompressed tiff strip to 1 bpp pixmap
 */
static INT fromtif1(void)
{
	UINT j, n, bcnt;

	j = (hsize + 7) / 8;
	n = (tifimagewidth + 7) / 8;
	if (j > n) j = n;
#if OS_WIN32GUI
	while (bitsptr >= bits && tifstripsize >= n) {
#else
	while (bitsptr < bits_end && tifstripsize >= n) {
#endif
		memcpy(bitsptr, tifstrip, j);
		if (tifxchgbw) for (bcnt = 0; bcnt < j; bcnt++) bitsptr[bcnt] ^= tifxchgbw;
#if OS_WIN32GUI
		bitsptr -= bitsrowbytes;
#else
		bitsptr += bitsrowbytes;
#endif
		tifstrip += n;
		tifstripsize -= n;
	}
	return(0);
}

/* FROMTIF2 */
static INT fromtif2(void)  /* convert 1 bpp group 3 compressed tiff strip to 1 bpp pixmap */
{
	UCHAR *oldbitsptr;
	INT RowCount;      /* Counter for number of rows Decoded */
	INT retval;
	SHORT **temp;

	BitsLeftIn = 8;
	DimFlag = 1;

	oldbitsptr = bitsptr;
	retval = decode1drow();
	if (retval == -1) {
		bitsptr = oldbitsptr;
		return(0);
	}
	bitsptr = oldbitsptr;

#if OS_WIN32GUI
	for (RowCount = 0; (UINT)RowCount <= RowsPerStrip && bitsptr >= bits; RowCount++) {
#else
	for (RowCount = 0; (UINT)RowCount <= RowsPerStrip && bitsptr < bits_end; RowCount++) {
#endif
		oldbitsptr = bitsptr;
		if (!DimFlag) retval = decode2drow();
		else retval = decode1drow();
		if (retval == -1) {
			bitsptr = oldbitsptr;
			return(0);
		}

		if (tifxchgbw)
			for (bitsptr = oldbitsptr; bitsptr < oldbitsptr + bitsrowbytes; bitsptr++)
				*bitsptr ^= tifxchgbw;

#if OS_WIN32GUI
		bitsptr = oldbitsptr - bitsrowbytes;
#else
		bitsptr = oldbitsptr + bitsrowbytes;
#endif
		temp = Reference;
		Reference = Current;
		Current = temp;
		memset(*Current, 0x00, tifimagewidth * sizeof(SHORT));
	}
	return(0);
}

/*
 * FROMTIF3
 * convert 1 bpp COMPRESSION_PACKBITS compressed tiff strip to 1 bpp pixmap
 */
static INT fromtif3(void)
{
	char c;
	INT tiff_row_used, smallwidth, pixwidth, tifwidth, state;

	pixwidth = (hsize + 7) / 8;
	tifwidth = (tifimagewidth + 7) / 8;
	if (hsize < tifimagewidth) smallwidth = pixwidth;
	else smallwidth = tifwidth;
	tiff_row_used = state = 0;

#if OS_WIN32GUI
	while (bitsptr >= bits) {
#else
	while (bitsptr < bits_end) {
#endif
		if (!state) {
			if (!tifstripsize--) return(0);
			state = *(CHAR *) tifstrip++;
			if (state > 127 && state <= 255) state -= 256;
			if (state == 128) {
				state = 0;
				continue;
			}
			else {
				c = *tifstrip++;
				tifstripsize--;
			}
		}
		else if (state > 0) {
			c = *tifstrip++;
			tifstripsize--;
			state--;
		}
		else state++;
		if (tiff_row_used++ < smallwidth) {
			*bitsptr++ = c ^ 0xFF;
		}
		if (tiff_row_used == tifwidth) {
			tiff_row_used = 0;
#if OS_WIN32GUI
			bitsptr -= bitsrowbytes + smallwidth;
#else
			bitsptr += bitsrowbytes - smallwidth;
#endif
		}
	}
	return(0);
}

/* FROMTIF4 */
static INT fromtif4(void)  /* convert 24 bpp tiff strip to 24 bpp pixmap */
{
	INT i, j, k;
	UCHAR *srcptr, *oldsrcptr, *oldbitsptr;

	if ((srcptr = uncmp()) == NULL)
		return(0);
	if (hsize < tifimagewidth) j = hsize;
	else j = tifimagewidth;
	k = tifimagewidth*3;

	if (Compression == COMPRESSION_LZW && Predictor == 2)
	{
		while (uncmpstripsize > 0 && bitsptr < bits_end) {
			oldbitsptr = bitsptr;
			oldsrcptr = srcptr;
#if OS_WIN32GUI
			*(bitsptr + 2) = *srcptr;
			*(bitsptr + 1) = *(srcptr + 1);
			*bitsptr = *(srcptr + 2);
			bitsptr += 3;
			srcptr += 3;
			for (i = 1; i < j && bitsptr >= bits; i++) {
				*(bitsptr + 2) = *srcptr + *(bitsptr - 1);
				*(bitsptr + 1) = *(srcptr + 1) + *(bitsptr - 2);
				*bitsptr = *(srcptr + 2) + *(bitsptr - 3);
				bitsptr += 3;
				srcptr += 3;
			}
			bitsptr = oldbitsptr - bitsrowbytes;
#else
			*bitsptr = *srcptr;
			*(bitsptr + 1) = *(srcptr + 1);
			*(bitsptr + 2) = *(srcptr + 2);
			bitsptr += 3;
			srcptr += 3;
			for (i = 1; i < j && bitsptr < bits_end; i++) {
				*bitsptr = *srcptr + *(bitsptr - 3);
				*(bitsptr + 1) = *(srcptr + 1) + *(bitsptr - 2);
				*(bitsptr + 2) = *(srcptr + 2) + *(bitsptr - 1);
				bitsptr += 3;
				srcptr += 3;
			}
			bitsptr = oldbitsptr + bitsrowbytes;
#endif
			srcptr = oldsrcptr + k;
			uncmpstripsize -= k;
		}
	}

	else {
		while (bitsptr < bits_end && uncmpstripsize > 0) {
			oldbitsptr = bitsptr;
			oldsrcptr = srcptr;
#if OS_WIN32GUI
			for (i = 0; bitsptr >= bits && i < j; i++) {
				*(bitsptr + 2) = *srcptr;
				*(bitsptr + 1) = *(srcptr + 1);
				*bitsptr = *(srcptr + 2);
#else
			for (i = 0; bitsptr < bits_end && i < j; i++) {
				*bitsptr = *srcptr;
				*(bitsptr + 1) = *(srcptr + 1);
				*(bitsptr + 2) = *(srcptr + 2);
#endif
				bitsptr += 3;
				srcptr += 3;
			}
#if OS_WIN32GUI
			bitsptr = oldbitsptr - bitsrowbytes;
#else
			bitsptr = oldbitsptr + bitsrowbytes;
#endif
			srcptr = oldsrcptr + k;
			uncmpstripsize -= k;
		}
	}
	return(0);
}

/* FROMTIF5 */
static INT fromtif5(void)  /* convert 4 bpp tiff strip to 4 bpp pixmap */
{
	INT i, j, k;
	UCHAR *srcptr, *oldbitsptr, *oldsrcptr;
	UCHAR temp;

	srcptr = uncmp();
	if (hsize < tifimagewidth) j = hsize;
	else j = tifimagewidth;
	k = (tifimagewidth + 1) / 2;

	if (Compression == COMPRESSION_LZW && Predictor == 2) {
#if OS_WIN32GUI
		while (bitsptr >= bits && uncmpstripsize > 0) {
#else
		while (bitsptr < bits_end && uncmpstripsize > 0) {
#endif
			oldbitsptr = bitsptr;
			oldsrcptr = srcptr;
			temp = *bitsptr = colors[*srcptr & 0x0F];
			temp = *bitsptr++ += (colors[((*srcptr++ & 0xF0) >> 4) + temp] << 4);
#if OS_WIN32GUI
			for (i = 2; bitsptr >= bits && i < j; i++) {
#else
			for (i = 2; bitsptr < bits_end && i < j; i++) {
#endif
				if (i & 1) temp = *bitsptr++ += (colors[((*srcptr++ & 0xF0) >> 4) + temp] << 4);
				else temp = *bitsptr = colors[(*srcptr & 0x0F) + (temp >> 4)];
			}
#if OS_WIN32GUI
			bitsptr = oldbitsptr - bitsrowbytes;
#else
			bitsptr = oldbitsptr + bitsrowbytes;
#endif
			srcptr = oldsrcptr + k;
			uncmpstripsize -= k;
		}
	}
	else {
#if OS_WIN32GUI
		while (bitsptr >= bits && uncmpstripsize > 0) {
#else
		while (bitsptr < bits_end && uncmpstripsize > 0) {
#endif
			oldbitsptr = bitsptr;
			oldsrcptr = srcptr;
#if OS_WIN32GUI
			for (i = 0; bitsptr >= bits && i < j; i++) {
#else
			for (i = 0; bitsptr < bits_end && i < j; i++) {
#endif
				if (!(i & 1)) *bitsptr = *srcptr & 0xF0;
				else *bitsptr++ += *srcptr++ & 0x0F;
			}
#if OS_WIN32GUI
			bitsptr = oldbitsptr - bitsrowbytes;
#else
			bitsptr = oldbitsptr + bitsrowbytes;
#endif
			srcptr = oldsrcptr + k;
			uncmpstripsize -= k;
		}
	}
	return(0);
}

/* FROMTIF6 */
static INT fromtif6(void)  /* convert 4 bpp tiff strip to 8 bpp pixmap */
{
	INT i, j, k;
	UCHAR *srcptr, *oldbitsptr, *oldsrcptr;

	srcptr = uncmp();
	if (hsize < tifimagewidth) j = hsize;
	else j = tifimagewidth;
	k = (tifimagewidth + 1) / 2;

	if (Compression == COMPRESSION_LZW && Predictor == 2) {
#if OS_WIN32GUI
		while (bitsptr >= bits && uncmpstripsize > 0) {
#else
		while (bitsptr < bits_end && uncmpstripsize > 0) {
#endif
			oldbitsptr = bitsptr;
			oldsrcptr = srcptr;
			*bitsptr++ = colors[*srcptr & 0x0F];
			*bitsptr = colors[((*srcptr++ & 0xF0) >> 4) + *(bitsptr - 1)];
			bitsptr++;
#if OS_WIN32GUI
			for (i = 2; bitsptr >= bits && i < j; i++) {
#else
			for (i = 2; bitsptr < bits_end && i < j; i++) {
#endif
				if (i & 1) *bitsptr = colors[((*srcptr++ & 0xF0) >> 4) + *(bitsptr - 1)];
				else *bitsptr = colors[(*srcptr & 0x0F) + *(bitsptr - 1)];
				bitsptr++;
			}
#if OS_WIN32GUI
			bitsptr = oldbitsptr - bitsrowbytes;
#else
			bitsptr = oldbitsptr + bitsrowbytes;
#endif
			srcptr = oldsrcptr + k;
			uncmpstripsize -= k;
		}
	}
	else {
#if OS_WIN32GUI
		while (bitsptr >= bits && uncmpstripsize > 0) {
#else
		while (bitsptr < bits_end && uncmpstripsize > 0) {
#endif
			oldbitsptr = bitsptr;
			oldsrcptr = srcptr;
#if OS_WIN32GUI
			for (i = 0; bitsptr >= bits && i < j; i++) {
#else
			for (i = 0; bitsptr < bits_end && i < j; i++) {
#endif
				if (!(i & 1)) *bitsptr++ = colors[(*srcptr & 0xF0) >> 4];
				else *bitsptr++ = colors[*srcptr++ & 0x0F];
			}
#if OS_WIN32GUI
			bitsptr = oldbitsptr - bitsrowbytes;
#else
			bitsptr = oldbitsptr + bitsrowbytes;
#endif
			srcptr = oldsrcptr + k;
			uncmpstripsize -= k;
		}
	}
	return(0);
}

/* FROMTIF7 */
static INT fromtif7(void)  /* convert 4 bpp tiff strip to 24 bpp pixmap */
{
	INT i, j, k, m;
	UCHAR *srcptr, *oldbitsptr, *oldsrcptr;
	UCHAR temp;

	srcptr = uncmp();
	if (hsize < tifimagewidth) j = hsize;
	else j = tifimagewidth;
	k = (tifimagewidth + 1) / 2;

#if OS_WIN32GUI

	if (Compression == COMPRESSION_LZW && Predictor == 2) {
		while (bitsptr >= bits && uncmpstripsize > 0) {
			oldbitsptr = bitsptr;
			oldsrcptr = srcptr;
			temp = m = (*srcptr & 0x0F);
			m *= 3;
			*(bitsptr + 2) = colors[m];
			*(bitsptr + 1) = colors[m + 1];
			*bitsptr = colors[m + 2];
			bitsptr += 3;
			for (i = 1; bitsptr >= bits && i < j; i++) {
				if (i & 1) temp = m = (((*srcptr++ & 0xF0) >> 4) + temp)  & 0x0F;
				else temp = m = ((*srcptr & 0x0F) + temp) & 0x0F;
				m *= 3;
				*(bitsptr + 2) = colors[m];
				*(bitsptr + 1) = colors[m + 1];
				*bitsptr = colors[m + 2];
				bitsptr += 3;
			}
			bitsptr = oldbitsptr - bitsrowbytes;
			srcptr = oldsrcptr + k;
			uncmpstripsize -= k;
		}
	}
	else {
		while (bitsptr >= bits && uncmpstripsize > 0) {
			oldbitsptr = bitsptr;
			oldsrcptr = srcptr;
			for (i = 0; bitsptr >= bits && i < j; i++) {
				if (!(i & 1)) m = (*srcptr & 0xF0) >> 4;
				else m = (*srcptr++ & 0x0F);
				m *= 3;
				*(bitsptr + 2) = colors[m];
				*(bitsptr + 1) = colors[m + 1];
				*bitsptr = colors[m + 2];
				bitsptr += 3;
			}
			bitsptr = oldbitsptr - bitsrowbytes;
			srcptr = oldsrcptr + k;
			uncmpstripsize -= k;
		}
	}
	return(0);
}

#else

	if (Compression == COMPRESSION_LZW && Predictor == 2) {
		while (bitsptr < bits_end && uncmpstripsize > 0) {
			oldbitsptr = bitsptr;
			oldsrcptr = srcptr;
			temp = m = (*srcptr & 0x0F);
			m *= 3;
			*bitsptr = colors[m];
			*(bitsptr + 1) = colors[m + 1];
			*(bitsptr + 2) = colors[m + 2];
			bitsptr += 3;
			for (i = 1; bitsptr < bits_end && i < j; i++) {
				if (i & 1) temp = m = (((*srcptr++ & 0xF0) >> 4) + temp)  & 0x0F;
				else temp = m = ((*srcptr & 0x0F) + temp) & 0x0F;
				m *= 3;
				*bitsptr = colors[m];
				*(bitsptr + 1) = colors[m + 1];
				*(bitsptr + 2) = colors[m + 2];
				bitsptr += 3;
			}
			bitsptr = oldbitsptr + bitsrowbytes;
			srcptr = oldsrcptr + k;
			uncmpstripsize -= k;
		}
	}
	else {
		while (bitsptr < bits_end && uncmpstripsize > 0) {
			oldbitsptr = bitsptr;
			oldsrcptr = srcptr;
			for (i = 0; bitsptr < bits_end && i < j; i++) {
				if (i & 1) {
					m = (*srcptr++ & 0x0F);
				}
				else {
					m = (*srcptr & 0xF0) >> 4;
				}
				m *= 3;
				*bitsptr = colors[m];
				*(bitsptr + 1) = colors[m + 1];
				*(bitsptr + 2) = colors[m + 2];
				bitsptr += 3;
			}
			bitsptr = oldbitsptr + bitsrowbytes;
			srcptr = oldsrcptr + k;
			uncmpstripsize -= k;
		}
	}
	return(0);
}
#endif


/*
 * FROMTIF8
 * convert 8 bpp tiff strip to 8 bpp pixmap
 */
static INT fromtif8(void)
{
	INT i, j;
	UCHAR *p, *oldbitsptr;

	if (hsize < tifimagewidth) j = hsize;
	else j = tifimagewidth;
	p = uncmp();

	if (Compression == COMPRESSION_LZW && Predictor == 2) {
#if OS_WIN32GUI
		while (bitsptr >= bits && uncmpstripsize > 0) {
#else
		while (bitsptr < bits_end && uncmpstripsize > 0) {
#endif
			oldbitsptr = bitsptr;
			*bitsptr++ = colors[*p++];
			for (i = 1; i < j; i++) {
				*p += *(p - 1);
				*bitsptr++ = colors[*p++];
			}
#if OS_WIN32GUI
			bitsptr = oldbitsptr - bitsrowbytes;
#else
			bitsptr = oldbitsptr + bitsrowbytes;
#endif
			p += tifimagewidth;
			uncmpstripsize -= tifimagewidth;
		}
	}
	else {
#if OS_WIN32GUI
		while (bitsptr >= bits && uncmpstripsize >= j) {
			oldbitsptr = bitsptr;
			for (i = 0; bitsptr >= bits && i < j; i++)
				*bitsptr++ = colors[*p++];
			bitsptr = oldbitsptr - bitsrowbytes;
#else
		while (bitsptr < bits_end && uncmpstripsize > 0) {
			oldbitsptr = bitsptr;
			for (i = 0; bitsptr < bits_end && i < j; i++)
				*bitsptr++ = colors[*p++];
			bitsptr = oldbitsptr + bitsrowbytes;
#endif
			p += tifimagewidth - j;
			uncmpstripsize -= tifimagewidth;
		}
	}
	return(0);
}

/*
 * FROMTIF9
 *
 * convert 8 bpp tiff strip to 24 bpp pixmap
 *
 */
static INT fromtif9(void)
{
	INT i, j, k;
	UCHAR *p;

	if (hsize < tifimagewidth) j = hsize;
	else j = tifimagewidth;

	/*
	 * The next call depends on dereferenced memalloc pointers and must be fixed
	 */
	p = uncmp();

	if (Compression == COMPRESSION_LZW && Predictor == 2) {
#if OS_WIN32GUI
		while (bitsptr >= bits && uncmpstripsize > 0) {
#else
		while (bitsptr < bits_end && uncmpstripsize > 0) {
#endif
			k = *p++ * 3;
#if OS_WIN32GUI
			*(bitsptr + 2) = colors[k];
			*(bitsptr + 1) = colors[k + 1];
			*bitsptr = colors[k + 2];
#else
			*bitsptr = colors[k];
			*(bitsptr + 1) = colors[k + 1];
			*(bitsptr + 2) = colors[k + 2];
#endif
			bitsptr += 3;
#if OS_WIN32GUI
			for (i = 1; bitsptr >= bits && i < j; i++) {
#else
			for (i = 1; bitsptr < bits_end && i < j; i++) {
#endif
				*p += *(p - 1);
				k = *p++ * 3;
#if OS_WIN32GUI
				*(bitsptr + 2) = colors[k];
				*(bitsptr + 1) = colors[k + 1];
				*bitsptr = colors[k + 2];
				bitsptr += 3;
			}
			bitsptr -= bitsrowbytes + j * 3;
#else
				*bitsptr = colors[k];
				*(bitsptr + 1) = colors[k + 1];
				*(bitsptr + 2) = colors[k + 2];
				bitsptr += 3;
			}
			bitsptr += bitsrowbytes - j * 3;
#endif
			p += tifimagewidth - j;
			uncmpstripsize -= tifimagewidth;
		}
	}
	else {
#if OS_WIN32GUI
		while (bitsptr >= bits && uncmpstripsize > 0) {
#else
		while (bitsptr < bits_end && uncmpstripsize > 0) {
#endif
#if OS_WIN32GUI
			for (i = 0; bitsptr >= bits && i < j; i++) {
#else
			for (i = 0; bitsptr < bits_end && i < j; i++) {
#endif
				k = *p++ * 3;
#if OS_WIN32GUI
				*(bitsptr + 2) = colors[k];
				*(bitsptr + 1) = colors[k + 1];
				*bitsptr = colors[k + 2];
				bitsptr += 3;
			}
			bitsptr -= bitsrowbytes + j * 3;
#else
				*bitsptr = colors[k];
				*(bitsptr + 1) = colors[k + 1];
				*(bitsptr + 2) = colors[k + 2];
				bitsptr += 3;
			}
			bitsptr += bitsrowbytes - j * 3;
#endif
			p += tifimagewidth - j;
			uncmpstripsize -= tifimagewidth;
		}
	}
	return(0);
}

/* FROMTIF10 */
static INT fromtif10(void)  /* convert 1 bpp tiff strip to 4 bpp pixmap */
{
	INT row_pos, row_bytes, tifbyte, bitmask, k;
	UCHAR *tifptr, *oldbitsptr, *oldtifptr;

	tifptr = uncmp();
	if (hsize < tifimagewidth) row_bytes = hsize;
	else row_bytes = tifimagewidth;
	k = (tifimagewidth + 1) / 8;
#if OS_WIN32GUI
	while (bitsptr >= bits && uncmpstripsize > 0) {
#else
	while (bitsptr < bits_end && uncmpstripsize > 0) {
#endif
		bitmask = 0x80;
		oldbitsptr = bitsptr;
		oldtifptr = tifptr;
#if OS_WIN32GUI
		for (row_pos = 0; bitsptr >= bits && row_pos < row_bytes; row_pos++) {
#else
		for (row_pos = 0; bitsptr < bits_end && row_pos < row_bytes; row_pos++) {
#endif
			tifbyte = (*tifptr & bitmask);
			if (tifxchgbw) tifbyte = (tifbyte ? 0 : 1);

			if (row_pos & 1) {
				*bitsptr = (*bitsptr & 0x10) | (tifbyte ? 0x01 : 0);
				bitsptr++;
			}
			else *bitsptr = (*bitsptr & 0x01) | (tifbyte ? 0x10 : 0);

			if (bitmask > 1) bitmask >>= 1;
			else {
				tifptr++;
				bitmask = 0x80;
			}
		}
#if OS_WIN32GUI
		bitsptr = oldbitsptr - bitsrowbytes;
#else
		bitsptr = oldbitsptr + bitsrowbytes;
#endif
		tifptr = oldtifptr + k;
		uncmpstripsize -= k;
	}
	return(0);
}

/* FROMTIF11 */
static INT fromtif11(void)  /* convert 1 bpp tiff strip to 8 bpp pixmap */
{
	INT row_pos, row_bytes, tifbyte, bitmask, k;
	UCHAR *tifptr, *oldbitsptr, *oldtifptr;

	tifptr = uncmp();
	if (hsize < tifimagewidth) row_bytes = hsize;
	else row_bytes = tifimagewidth;
	k = (tifimagewidth + 1) / 8;
#if OS_WIN32GUI
	while (bitsptr >= bits && uncmpstripsize > 0) {
#else
	while (bitsptr < bits_end && uncmpstripsize > 0) {
#endif
		bitmask = 0x80;
		oldbitsptr = bitsptr;
		oldtifptr = tifptr;
#if OS_WIN32GUI
		for (row_pos = 0; bitsptr >= bits && row_pos < row_bytes; row_pos++) {
#else
		for (row_pos = 0; bitsptr < bits_end && row_pos < row_bytes; row_pos++) {
#endif
			tifbyte = (*tifptr & bitmask);
			if (tifxchgbw) tifbyte = (tifbyte ? 0 : 1);

			*bitsptr++ = (tifbyte ? 1 : 0);

			if (bitmask > 1) bitmask >>= 1;
			else {
				tifptr++;
				bitmask = 0x80;
			}
		}
#if OS_WIN32GUI
		bitsptr = oldbitsptr - bitsrowbytes;
#else
		bitsptr = oldbitsptr + bitsrowbytes;
#endif
		tifptr = oldtifptr + k;
		uncmpstripsize -= k;
	}
	return(0);
}

/* FROMTIF12 */
static INT fromtif12(void)  /* convert 1 bpp tiff strip to 24 bpp pixmap */
{
	INT row_pos, row_bytes, tifbyte, bitmask, k;
	UCHAR *tifptr, *oldbitsptr, *oldtifptr;

	tifptr = uncmp();
	if (hsize < tifimagewidth) row_bytes = hsize;
	else row_bytes = tifimagewidth;
	k = (tifimagewidth + 1) / 8;
#if OS_WIN32GUI
	while (bitsptr >= bits && uncmpstripsize > 0) {
#else
	while (bitsptr < bits_end && uncmpstripsize > 0) {
#endif
		bitmask = 0x80;
		oldbitsptr = bitsptr;
		oldtifptr = tifptr;
#if OS_WIN32GUI
		for (row_pos = 0; bitsptr >= bits && row_pos < row_bytes; row_pos++) {
#else
		for (row_pos = 0; bitsptr < bits_end && row_pos < row_bytes; row_pos++) {
#endif
			tifbyte = (*tifptr & bitmask);
			if (tifxchgbw) tifbyte = (tifbyte ? 0 : 1);
			memset(bitsptr, (tifbyte ? 0xff : 0), 3);
			bitsptr += 3;

			if (bitmask > 1) bitmask >>= 1;
			else {
				tifptr++;
				bitmask = 0x80;
			}
		}
#if OS_WIN32GUI
		bitsptr = oldbitsptr - bitsrowbytes;
#else
		bitsptr = oldbitsptr + bitsrowbytes;
#endif
		tifptr = oldtifptr + k;
		uncmpstripsize -= k;
	}
	return(0);
}

/**
 * FROMTIF13_RLE
 *
 * rle tiff to 1 bpp
 */
static INT fromtif13_RLE(void) {

	UCHAR *oldbitsptr;
	INT RowCount;      /* Counter for number of rows Decoded */
	INT retval;

	BitsLeftIn = 8;
#if OS_WIN32GUI
	for (RowCount = 0; (UINT)RowCount < RowsPerStrip && bitsptr >= bits; RowCount++) {
#else
	for (RowCount = 0; (UINT)RowCount < RowsPerStrip && bitsptr < bits_end; RowCount++) {
#endif
		oldbitsptr = bitsptr;
		retval = decode1drow_RLE();
		if (retval == -1) {
			bitsptr = oldbitsptr;
			return(0);
		}

		if (tifxchgbw)
			for (bitsptr = oldbitsptr; bitsptr < oldbitsptr + bitsrowbytes; bitsptr++)
				*bitsptr ^= tifxchgbw;

#if OS_WIN32GUI
		bitsptr = oldbitsptr - bitsrowbytes;
#else
		bitsptr = oldbitsptr + bitsrowbytes;
#endif
	}
	return(0);
}

/**
 * FROMTIF13
 *
 * group 3 (1d) tiff to 1 bpp
 */
static INT fromtif13(void)    /* rle or group 3 (1d) tiff to 1 bpp */
{
	UCHAR *oldbitsptr;
	INT RowCount;      /* Counter for number of rows Decoded */
	INT retval;

	BitsLeftIn = 8;
	oldbitsptr = bitsptr;
	retval = decode1drow();
	if (retval == -1) {
		bitsptr = oldbitsptr;
		return(0);
	}
	bitsptr = oldbitsptr;

#if OS_WIN32GUI
	for (RowCount = 0; (UINT)RowCount < RowsPerStrip && bitsptr >= bits; RowCount++) {
#else
	for (RowCount = 0; (UINT)RowCount < RowsPerStrip && bitsptr < bits_end; RowCount++) {
#endif
		oldbitsptr = bitsptr;
		retval = decode1drow();
		if (retval == -1) {
			bitsptr = oldbitsptr;
			return(0);
		}

		if (tifxchgbw)
			for (bitsptr = oldbitsptr; bitsptr < oldbitsptr + bitsrowbytes; bitsptr++)
				*bitsptr ^= tifxchgbw;

#if OS_WIN32GUI
		bitsptr = oldbitsptr - bitsrowbytes;
#else
		bitsptr = oldbitsptr + bitsrowbytes;
#endif
	}
	return(0);
}


/* FROMTIF14 */
static INT fromtif14(void)         /* group 4 tiff to 1 bpp */
{
	UCHAR *oldbitsptr;
	INT RowCount;      /* Counter for number of rows Decoded */
	INT retval;
	SHORT **temp;

	BitsLeftIn = 8;

	memset(*Current, 0x00, tifimagewidth * sizeof(SHORT));
	memset(*Reference, 0x00, tifimagewidth * sizeof(SHORT));
	(*Reference)[0] = (SHORT) -tifimagewidth;

#if OS_WIN32GUI
	for (RowCount = 0; (UINT) RowCount < RowsPerStrip && bitsptr >= bits; RowCount++) {
#else
	for (RowCount = 0; (UINT) RowCount < RowsPerStrip && bitsptr < bits_end; RowCount++) {
#endif
		oldbitsptr = bitsptr;
		retval = decode2drow();
		if (retval == -1) {
			bitsptr = oldbitsptr;
			return(0);
		}
		if (retval == 1) return(0);

		if (tifxchgbw)
			for (bitsptr = oldbitsptr; bitsptr < oldbitsptr + bitsrowbytes && bitsptr < bits_end; bitsptr++) *bitsptr = ~*bitsptr;

#if OS_WIN32GUI
		bitsptr = oldbitsptr - bitsrowbytes;
#else
		bitsptr = oldbitsptr + bitsrowbytes;
#endif

		temp = Reference;
		Reference = Current;
		Current = temp;
		memset(*Current, 0x00, tifimagewidth * sizeof(SHORT));
	}
	return(0);
}

static INT whiteRLE_TerminalTableLookup(int cw, int numBits) {
	const int entries = sizeof(TIFFRleTermWhiteTable) / sizeof(TIFFRleTabEnt);
	int i1;
	for (i1 = 0; i1 < entries; i1++) {
		if (cw == TIFFRleTermWhiteTable[i1].codeWord && TIFFRleTermWhiteTable[i1].Width == numBits) {
			return TIFFRleTermWhiteTable[i1].Param;
		}
	}
	return -1;
}

static INT whiteRLE_MakeupTableLookup(int cw, int numBits) {
	const int entries = sizeof(TIFFRleMakeupWhiteTable) / sizeof(TIFFRleTabEnt);
	int i1;
	for (i1 = 0; i1 < entries; i1++) {
		if (cw == TIFFRleMakeupWhiteTable[i1].codeWord && TIFFRleMakeupWhiteTable[i1].Width == numBits) {
			return TIFFRleMakeupWhiteTable[i1].Param;
		}
	}
	return -1;
}

static INT blackRLE_TerminalTableLookup(int cw, int numBits) {
	const int entries = sizeof(TIFFRleTermBlackTable) / sizeof(TIFFRleTabEnt);
	int i1;
	for (i1 = 0; i1 < entries; i1++) {
		if (cw == TIFFRleTermBlackTable[i1].codeWord && TIFFRleTermBlackTable[i1].Width == numBits) {
			return TIFFRleTermBlackTable[i1].Param;
		}
	}
	return -1;
}

static INT blackRLE_MakeupTableLookup(int cw, int numBits) {
	const int entries = sizeof(TIFFRleMakeupBlackTable) / sizeof(TIFFRleTabEnt);
	int i1;
	for (i1 = 0; i1 < entries; i1++) {
		if (cw == TIFFRleMakeupBlackTable[i1].codeWord && TIFFRleMakeupBlackTable[i1].Width == numBits) {
			return TIFFRleMakeupBlackTable[i1].Param;
		}
	}
	return -1;
}

static INT blackAndWhiteRLE_MakeupTableLookup(int cw, int numBits) {
	const int entries = sizeof(TIFFRleBandWMakeupTable) / sizeof(TIFFRleTabEnt);
	int i1;
	for (i1 = 0; i1 < entries; i1++) {
		if (cw == TIFFRleBandWMakeupTable[i1].codeWord && TIFFRleBandWMakeupTable[i1].Width == numBits) {
			return TIFFRleBandWMakeupTable[i1].Param;
		}
	}
	return -1;
}

static INT  decode1drow_RLE(void) {

	INT White = 1;      /* color of current run length */
	INT RunLength;      /* Decode Run Length */
	INT RowTotal = 0;   /* Total Decoded Length for current row */
	INT width;
	UCHAR *rowend;
	int codeWord;
	int codeWordBitCount;

	if (hsize < tifimagewidth) width = bitsrowbytes;
	else width = ((tifimagewidth + 31) / 32) * 4;
	rowend = bitsptr + width;
	for (;;) {
		codeWord = 0;
		codeWordBitCount = 0;
		RunLength = 0;
		if (White) {
			for (;;) {
				if (BitsLeftIn == 0) {
					BitsLeftIn = 8;
					tifstrip++;
				}
				codeWord <<= 1;
				if (*tifstrip & maskin[BitsLeftIn--]) codeWord++;
				codeWordBitCount++;

				// mimimum white codeword length is 4, plus a bit of optimization
				if (codeWordBitCount < 4 || (codeWordBitCount == 4 && codeWord < 7)) continue;
				// maximum white codeword length is 12
				if (codeWordBitCount > 12) {
					return RC_ERROR;
				}
				int i1 = whiteRLE_TerminalTableLookup(codeWord, codeWordBitCount);
				if (i1 != -1) {
					RunLength += i1;
					break;
				}
				i1 = whiteRLE_MakeupTableLookup(codeWord, codeWordBitCount);
				if (i1 != -1) {
					RunLength += i1;
					codeWord = codeWordBitCount = 0;
					continue;
				}
				i1 = blackAndWhiteRLE_MakeupTableLookup(codeWord, codeWordBitCount);
				if (i1 != -1) {
					RunLength += i1;
					codeWord = codeWordBitCount = 0;
				}
			}
		}
		else {
			for (;;) {
				if (BitsLeftIn == 0) {
					BitsLeftIn = 8;
					tifstrip++;
				}
				codeWord <<= 1;
				if (*tifstrip & maskin[BitsLeftIn--]) codeWord++;
				codeWordBitCount++;

				// mimimum black codeword length is 2
				if (codeWordBitCount < 2) continue;
				if (codeWordBitCount >= 2 && codeWordBitCount <= 4) {
					if (codeWord != 2 && codeWord != 3) continue;
				}
				else if (codeWordBitCount == 5) {
					if (codeWord != 3) continue;
				}
				// maximum black codeword length is 13
				if (codeWordBitCount > 13) {
					return RC_ERROR;
				}
				int i1 = blackRLE_TerminalTableLookup(codeWord, codeWordBitCount);
				if (i1 != -1) {
					RunLength += i1;
					break;
				}
				i1 = blackRLE_MakeupTableLookup(codeWord, codeWordBitCount);
				if (i1 != -1) {
					RunLength += i1;
					codeWord = codeWordBitCount = 0;
					continue;
				}
				i1 = blackAndWhiteRLE_MakeupTableLookup(codeWord, codeWordBitCount);
				if (i1 != -1) {
					RunLength += i1;
					codeWord = codeWordBitCount = 0;
				}
			}
		}
		RowTotal += RunLength;
		if (RowTotal > tifimagewidth) return RC_ERROR;

		fillspan(White, RunLength, rowend);
		if (RowTotal == tifimagewidth) {
			BitsLeftIn = 8;
			tifstrip++;
			break;
		}
		if (White) White = 0;
		else White = 1;
	}
	return 0;
}

/* DECODE1DROW */
static INT decode1drow(void)
{
	INT White = 1;      /* color of current run length */
	INT RunLength;      /* Decode Run Length */
	INT RowTotal = 0;   /* Total Decoded Length for current row */
	INT Index = 0;      /* Index into WCode and BCode array */
	INT CurIndex = 0;   /* Current Position in Current array */
					/* bit extractor mask */
	INT width;
	UCHAR *rowend;

	BitsLeftOut = 8;
	RowTotal = 0;

	if (hsize < tifimagewidth) width = bitsrowbytes;
	else {
		if (bpp == 1)
			width = ((tifimagewidth + 31) / 32) * 4;
		if (bpp == 4)
			width = ((tifimagewidth + 7) / 8) * 4;
		if (bpp == 8)
			width = ((tifimagewidth + 3) / 4) * 4;
		if (bpp == 24) {
#if OS_WIN32GUI
			width = 3 * tifimagewidth;
#else
			width = 3 * tifimagewidth;
#endif
		}
	}
	rowend = bitsptr + width;

	for ( ; ; ) {
		Index = 0;
		if (White == 1) {
			for ( ; ; ) {       /* Get Code */
				if (BitsLeftIn == 0) {
					BitsLeftIn = 8;
					tifstrip++;
				}
				if (*tifstrip & maskin[BitsLeftIn--]) Index = (2 * Index) + 2;
				else Index = (2 * Index) + 1;
				if (Index >= 4200) return RC_ERROR;
				if ((*WCode)[Index] == -1) continue;
				RunLength = (*WCode)[Index];
				break;
			}
		}
		else {
			for ( ; ; ) {  /* Get Code */
				if (BitsLeftIn == 0) {
					BitsLeftIn = 8;
					tifstrip++;
				}
				if (*tifstrip & maskin[BitsLeftIn--]) Index = (2 * Index) + 2;
				else Index = (2 * Index) + 1;
				if (Index > 8400) return RC_ERROR;
				if ((*BCode)[Index] == -1) continue;
				RunLength = (*BCode)[Index];
				break;
			}
		}
		if (RunLength == -3) {
			endofline(5);
			RunLength = -2;
		}
		if (RunLength == -2) {
			if (Compression == COMPRESSION_CCITTFAX3) {
				if (TwoDim == 1) {
					if (BitsLeftIn == 0) {
						BitsLeftIn = 8;
						tifstrip++;
					}
					if (*tifstrip & maskin[BitsLeftIn--]) DimFlag = 1;
					else DimFlag = 0;
				}
				return(0);
			}
			return RC_ERROR;
		}
		RowTotal += RunLength;
		if (RowTotal > tifimagewidth) return RC_ERROR;

		fillspan(White, RunLength, rowend);

		if (RunLength < 64) {
			if (White == 1) {
				White = 0;
				if (TwoDim == 1) (*Current)[CurIndex++] = -RowTotal;
			}
			else {
				White = 1;
				if (TwoDim == 1) (*Current)[CurIndex++] = RowTotal;
			}
		}

		if (RowTotal == tifimagewidth && Compression == COMPRESSION_CCITTRLE) {
			BitsLeftIn = 8;
			tifstrip++;
			break;
		}
	}
	return(0);
}

/* DECODE2DROW */
static INT decode2drow(void)
{
	UCHAR *rowend;
	INT retval, count;
	INT Index = 0;      /* Index into BCode and WCode */
	INT RefIndex;
	INT curIndex;
	INT CurState;
	INT Value;
	INT RunLength, RunLengthAdj;
	INT white = 1;
	INT width;
	INT VertAdj;
	INT EOLFlag = 0;
	INT mode;
	INT rowTotal;

	if (hsize < tifimagewidth) width = bitsrowbytes;
	else {
		if (bpp == 1) width = ((tifimagewidth + 31) / 32) * 4;
		else if (bpp == 4) width = ((tifimagewidth + 7) / 8) * 4;
		else if (bpp == 8) width = ((tifimagewidth + 3) / 4) * 4;
		else width = 3 * tifimagewidth;
	}

	rowend = bitsptr + width;
	curIndex = 0;
	RefIndex = 0;
	rowTotal = 0;
	BitsLeftOut = 8;

	for ( ; ; ) {
		CurState = START;
		Value = FINISH;
		while (CurState != FINISH) {  /* mode FSM */
			if (BitsLeftIn == 0) {
				BitsLeftIn = 8;
				tifstrip++;
			}
			switch (CurState) {
			case START:
				if (*tifstrip & maskin[BitsLeftIn--]) {
					Value = V0;									/* 1 */
					VertAdj = 0;
					CurState = FINISH;
				}
				else CurState = A;
				break;
			case A:
				if (*tifstrip & maskin[BitsLeftIn--]) CurState = C;
				else CurState = B;
				break;
			case B:
				if (*tifstrip & maskin[BitsLeftIn--]) {
					Value = HORZ;								/* 001 */
					CurState = FINISH;
				}
				else CurState = D;
				break;
			case C:
				if (*tifstrip & maskin[BitsLeftIn--]) {
					Value = VR1;								/* 011 */
					VertAdj = 1;
					CurState = FINISH;
				}
				else {
					Value = VL1;								/* 010 */
					VertAdj = -1;
					CurState = FINISH;
				}
				break;
			case D:
				if (*tifstrip & maskin[BitsLeftIn--]) {
					Value = PASS;								/* 0001 */
					CurState = FINISH;
				}
				else CurState = E;
				break;
			case E:
				if (*tifstrip & maskin[BitsLeftIn--]) CurState = G;
				else CurState = F;
				break;
			case F:
				if (*tifstrip & maskin[BitsLeftIn--]) CurState = I;
				else CurState = H;
				break;
			case G:
				if (*tifstrip & maskin[BitsLeftIn--]) {
					Value = VR2;								/* 000011 */
					VertAdj = 2;
					CurState = FINISH;
				}
				else {
					Value = VL2;								/* 000010 */
					VertAdj = -2;
					CurState = FINISH;
				}
				break;
			case H:
				if (*tifstrip & maskin[BitsLeftIn--]) CurState = J;
				else {
					Value = EOL;								/* 0000000 */
					CurState = FINISH;
				}
				break;
			case I:
				if (*tifstrip & maskin[BitsLeftIn--]) {
					Value = VR3;								/* 0000011 */
					VertAdj = 3;
					CurState = FINISH;
				}
				else {
					Value = VL3;								/* 0000010 */
					VertAdj = -3;
					CurState = FINISH;
				}
				break;
			case J:
				if (*tifstrip & maskin[BitsLeftIn--]) CurState = K;
				else {
					Value = MODE_ERROR;
					CurState = FINISH;
				}
				break;
			case K:
				if (*tifstrip & maskin[BitsLeftIn--]) CurState = L;
				else {
					Value = MODE_ERROR;
					CurState = FINISH;
				}
				break;
			case L:
				if (*tifstrip & maskin[BitsLeftIn--]) {
					Value = MODE_UNCMP;
					CurState = FINISH;
				}
				else {
					Value = MODE_ERROR;
					CurState = FINISH;
				}
				break;
			}
		}
		switch (Value) {
		case VL3:
		case VL2:
		case VL1:
		case VR3:
		case VR2:
		case VR1:
		case V0:
			if (white == 1) {
				while ((*Reference)[RefIndex] >= -rowTotal || (*Reference)[RefIndex] >= 0) {
					if (RefIndex == 1 && (*Reference)[RefIndex] > 0 && curIndex == 0 && rowTotal == 0) {
						RefIndex = 0;
						break;
					}
					if ((*Reference)[RefIndex] ==  0 && RefIndex != 0) {
						(*Reference)[RefIndex] = (SHORT) -tifimagewidth;
						break;
					}
					RefIndex++;
				}
				RunLength = -((*Reference)[RefIndex]) - rowTotal + VertAdj;
			}
			else {
				while ((*Reference)[RefIndex] <= rowTotal || (*Reference)[RefIndex] <= 0) {
					if ((*Reference)[RefIndex] == 0 && RefIndex != 0) {
						(*Reference)[RefIndex] = (SHORT) tifimagewidth;
						break;
					}
					RefIndex++;
				}
				RunLength = (*Reference)[RefIndex] - rowTotal + VertAdj;
			}
			rowTotal += RunLength;

			if (Compression == COMPRESSION_CCITTFAX4) {
				if (rowTotal >= tifimagewidth) {
					RunLength -= (rowTotal - tifimagewidth);
					retval = fillspan(white, RunLength, rowend);
					if (retval == -1) return RC_ERROR;
					recordCurrentRun(&curIndex, &white, rowTotal);
					EOLFlag = 1;
					break;
				}
			}
			else if (rowTotal > tifimagewidth) return RC_ERROR;

			retval = fillspan(white, RunLength, rowend);
			if (retval == -1) return RC_ERROR;
			recordCurrentRun(&curIndex, &white, rowTotal);
			if (VertAdj < 0 && RefIndex > 0) RefIndex--;
			break;
		case MODE_ERROR:
			return RC_ERROR;
		case HORZ:
			for (count = 0; count < 2; ) {
				Index = 0;
				if (white == 1) {
					for ( ; ; ) {       /* Get Code */
						if (BitsLeftIn == 0) {
							BitsLeftIn = 8;
							tifstrip++;
						}
						if (*tifstrip & maskin[BitsLeftIn--]) Index = (2 * Index) + 2;
						else Index = (2 * Index) + 1;
						if (Index >= 4200) return RC_ERROR;
						if ((*WCode)[Index] == -1) continue;
						RunLength = (*WCode)[Index];
						break;
					}
				}
				else {
					for ( ; ; ) {       /* Get Code */
						if (BitsLeftIn == 0) {
							BitsLeftIn = 8;
							tifstrip++;
						}
						if (*tifstrip & maskin[BitsLeftIn--]) Index = (2 * Index) + 2;
						else Index = (2 * Index) + 1;
						if (Index > 8400) return RC_ERROR;
						if ((*BCode)[Index] == -1) continue;
						RunLength = (*BCode)[Index];
						break;
					}
				}
				rowTotal += RunLength;

				if (Compression == COMPRESSION_CCITTFAX4) {
					if (rowTotal >= tifimagewidth) {
						RunLengthAdj = RunLength - (rowTotal - tifimagewidth);
						retval = fillspan(white, RunLengthAdj, rowend);
						if (retval == -1) return RC_ERROR;
						if (RunLength < 64 && count == 1) {
							recordCurrentRun(&curIndex, &white, rowTotal);
							EOLFlag = 1;
							break;
						}
					}
				}
				else if (rowTotal > tifimagewidth) return RC_ERROR;
				retval = fillspan(white, RunLength, rowend);
				if (retval == -1) return RC_ERROR;
				if (RunLength < 64) {
					count++;
					recordCurrentRun(&curIndex, &white, rowTotal);
				}
			}
			break;
		case PASS:
			if (white == 1) {
				while ((*Reference)[RefIndex] >= -rowTotal || (*Reference)[RefIndex] >= 0) {
					if ((*Reference)[RefIndex] ==  0 && RefIndex != 0) {
						(*Reference)[RefIndex] = (SHORT) tifimagewidth;
						break;
					}
					if (rowTotal == 0 && (*Reference)[1] > 0) break;
					RefIndex++;
				}
				if ((*Reference)[RefIndex] != tifimagewidth) RefIndex++;
				RunLength = (*Reference)[RefIndex] - rowTotal;

			}
			else {
				while ((*Reference)[RefIndex] <= rowTotal || (*Reference)[RefIndex] <= 0) {
					if ((*Reference)[RefIndex] == 0 && RefIndex != 0) {
						(*Reference)[RefIndex] = (SHORT) -tifimagewidth;
						break;
					}
					RefIndex++;
				}
				if ((*Reference)[RefIndex] != -tifimagewidth) RefIndex++;
				RunLength =  -((*Reference)[RefIndex]) - rowTotal;
			}
			rowTotal += RunLength;

			if (Compression == COMPRESSION_CCITTFAX4) {
				if (rowTotal >= tifimagewidth) {
					RunLength -= (rowTotal - tifimagewidth);
					retval = fillspan(white, RunLength, rowend);
					if (retval == -1) return RC_ERROR;
					EOLFlag = 1;
					break;
				}
			}
			else if (rowTotal > tifimagewidth) return RC_ERROR;

			retval = fillspan(white, RunLength, rowend);
			if (retval == -1) return RC_ERROR;
			break;
		case EOL:
			if (Compression == COMPRESSION_CCITTFAX3) retval = endofline(0);
			else {
				retval = endofline(0);
				if (retval == MODE_ERROR) return RC_ERROR;
				retval = endofline(-7);
				if (retval == MODE_ERROR) return RC_ERROR;
				return(1);
			}
			if (retval == MODE_ERROR) return RC_ERROR;
			if (Compression == COMPRESSION_CCITTFAX3) {
				EOLFlag = 1;
				if (BitsLeftIn == 0) {
					BitsLeftIn = 8;
					tifstrip++;
				}
				if (*tifstrip & maskin[BitsLeftIn--]) DimFlag = 1;
				else DimFlag = 0;
			}
			break;
		case MODE_UNCMP:
			do {
				mode = bilevelunc();
				switch (mode) {
				case UNC_RUN1:
				case UNC_RUN2:
				case UNC_RUN3:
				case UNC_RUN4:
				case UNC_RUN5:
					RunLength = mode - 1;
					rowTotal += RunLength;
					if (rowTotal >= tifimagewidth) return RC_ERROR;
					retval = fillspan(1, RunLength, rowend);
					if (retval == -1) return RC_ERROR;
					rowTotal++;
					(*Current)[curIndex++] = rowTotal;
					retval = fillspan(0, 1, rowend);
					if (retval == -1) return RC_ERROR;
					(*Current)[curIndex] = rowTotal;
					break;
				case UNC_RUN6:
					RunLength = 5;
					rowTotal += RunLength;
					if (rowTotal > tifimagewidth) return RC_ERROR;
					retval = fillspan(1, RunLength, rowend);
					if (retval == -1) return RC_ERROR;
					(*Current)[curIndex] = rowTotal;
					break;
				case MODE_ERROR:
					return RC_ERROR;
				}
			} while (mode != UNC_EXIT);
			break;
		}
		if (EOLFlag) break;
	}
	return(0);
}

static void recordCurrentRun(INT *curIndex, INT *white, INT rowTotal) {
	(*curIndex)++;
	if (*white == 1) {
		*white = 0;
		if (rowTotal > 0 || *curIndex > 1) (*Current)[*curIndex] = -rowTotal;
		else *curIndex = 0;
	}
	else {
		*white = 1;
		(*Current)[*curIndex] = rowTotal;
	}
}

/* BILEVELUNC */
static INT bilevelunc(void)  /* handle 2D uncompressed mode */
{
	INT CurState;
	INT Value;

	CurState = START;
	Value = FINISH;
	while (CurState != FINISH) {  /* mode FSM */
		if (BitsLeftIn == 0) {
			BitsLeftIn = 8;
			tifstrip++;
		}
		switch (CurState) {
		case START:
			if (*tifstrip & maskin[BitsLeftIn--]) {
				Value = UNC_RUN1;
				CurState = FINISH;
			}
			else CurState = A;
			break;
		case A:
			if (*tifstrip & maskin[BitsLeftIn--]) {
				Value = UNC_RUN2;
				CurState = FINISH;
			}
			else CurState = B;
			break;
		case B:
			if (*tifstrip & maskin[BitsLeftIn--]) {
				Value = UNC_RUN3;
				CurState = FINISH;
			}
			else CurState = C;
			break;
		case C:
			if (*tifstrip & maskin[BitsLeftIn--]) {
				Value = UNC_RUN4;
				CurState = FINISH;
			}
			else CurState = D;
			break;
		case D:
			if (*tifstrip & maskin[BitsLeftIn--]) {
				Value = UNC_RUN5;
				CurState = FINISH;
			}
			else CurState = E;
			break;
		case E:
			if (*tifstrip & maskin[BitsLeftIn--]) {
				Value = UNC_RUN6;
				CurState = FINISH;
			}
			else CurState = F;
			break;
		case F:
			if (*tifstrip & maskin[BitsLeftIn--]) {
				Value = UNC_EXIT;
				CurState = FINISH;
			}
			else {
				Value = MODE_ERROR;
				CurState = FINISH;
			}
			break;
		}
	}
	return(Value);
}

/* ENDOFLINE */
static INT endofline(INT count)   /* handle the code 0000 00 in group 3 */ // @suppress("No return")
{
	while (count < 4) {
		if (BitsLeftIn == 0) {
			BitsLeftIn = 8;
			tifstrip++;
		}
		if (*tifstrip & maskin[BitsLeftIn--]) return(MODE_ERROR);
		else count++;
	}
	for ( ; ; ) {
		if (BitsLeftIn == 0) {
			BitsLeftIn = 8;
			tifstrip++;
		}
		if (*tifstrip & maskin[BitsLeftIn--]) return(0);
	}
}

/* FILLSPAN */
static INT fillspan(INT White, INT RunLength, UCHAR *rowend)  /* write a color run */
{
	INT retval;

	if (RunLength < 0) return RC_ERROR;

	retval = -1;
	if (bpp == 1) retval = fillspan1(White, RunLength, rowend);
	else if (bpp == 4) retval = fillspan4(White, RunLength, rowend);
	else if (bpp == 8) retval = fillspan8(White, RunLength, rowend);
	else if (bpp == 24) retval = fillspan24(White, RunLength, rowend);
	return(retval);
}

/* FILLSPAN1 */
static INT fillspan1(INT White, INT RunLength, UCHAR *rowend)    /* write a 1bpp color run */
{
	UCHAR bmask[9] = { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
	UCHAR wmask[9] = { 0xFF, 0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F };

	while (bitsptr < rowend) {
		if (White == 1) {
			while (RunLength && BitsLeftOut && BitsLeftOut < 8) {
				*bitsptr &= wmask[BitsLeftOut];
				BitsLeftOut--;
				RunLength--;
			}
			while (RunLength && BitsLeftOut == 8 && bitsptr < rowend) {
				if(RunLength > 7) {
					*bitsptr = 0;
					bitsptr++;
					RunLength -= 8;
				}
				else {
					while (RunLength) {
						*bitsptr &= wmask[BitsLeftOut];
						BitsLeftOut--;
						RunLength--;
					}
				}
			}
		}
		else {
			while (RunLength && BitsLeftOut && BitsLeftOut < 8) {
				*bitsptr |= bmask[BitsLeftOut];
				BitsLeftOut--;
				RunLength--;
			}
			while (RunLength && BitsLeftOut == 8 && bitsptr < rowend) {
				if(RunLength > 7) {
					*bitsptr = 0xFF;
					bitsptr++;
					RunLength -= 8;
				}
				else {
					while (RunLength) {
						*bitsptr |= bmask[BitsLeftOut];
						BitsLeftOut--;
						RunLength--;
					}
				}
			}
		}
		if (BitsLeftOut == 0) {
			BitsLeftOut = 8;
			bitsptr++;
		}
		if (!RunLength) break;
	}
	return(0);
}

/* FILLSPAN4 */
static INT fillspan4(INT White, INT RunLength, UCHAR *rowend)  /* write a 4bpp color run */
{
	while (bitsptr < rowend && RunLength > 0) {
		if (White == 1) {
			if (BitsLeftOut == 8) *bitsptr = 0x00;
			else if (BitsLeftOut == 4) *bitsptr &= 0x10;
		}
		else {
			if (BitsLeftOut == 8) *bitsptr = 0x10;
			else if (BitsLeftOut == 4) *bitsptr |= 0x01;
		}
		RunLength--;
		BitsLeftOut -= 4;
		if (BitsLeftOut <= 0) {
			BitsLeftOut = 8;
			bitsptr++;
		}
	}
	return(0);
}

/* FILLSPAN8 */
static INT fillspan8(INT White, INT RunLength, UCHAR *rowend)  /* write a 8bpp color run */
{
	INT RowLength;

	if ((rowend - bitsptr) > RunLength) {
		if (White == 1) memset(bitsptr, 0x00, RunLength);
		else memset(bitsptr, 0x01, RunLength);
		bitsptr += RunLength;
	}
	else if ((rowend - bitsptr) > 0) {
		RowLength = (INT) (ptrdiff_t) (rowend - bitsptr);
		if (White == 1) memset(bitsptr, 0x00, RowLength);
		else memset(bitsptr, 0x01, RowLength);
		bitsptr += RowLength;
	}
	return(0);
}

/*
 * FILLSPAN24
 * Use the TIF default of MINISWHITE
 * Bits will be adjusted later for our internal standard of zero-is-black
 */
static INT fillspan24(INT White, INT RunLength, UCHAR *rowend)  /* write a 24bpp color run */
{
#if OS_WIN32GUI
	INT ExpLength;
	INT RowLength;

	ExpLength = RunLength * 3;

	if ((rowend - bitsptr) > ExpLength) {
		if (White) memset(bitsptr, 0x00, ExpLength);
		else memset(bitsptr, 0xff, ExpLength);
		bitsptr += ExpLength;
	}
	else if ((rowend - bitsptr) > 0) {
		RowLength = (INT) (ptrdiff_t) (rowend - bitsptr);
		if (White) memset(bitsptr, 0x00, RowLength);
		else memset(bitsptr, 0xff, RowLength);
		bitsptr += RowLength;
	}
#else
	if (rowend > bits_end) return(0);
	while (bitsptr < rowend && RunLength) {
		if (White) {
			*bitsptr = 0x00;
			*(bitsptr + 1) = 0x00;
			*(bitsptr + 2) = 0x00;
		}
		else {
			*bitsptr = 0xff;
			*(bitsptr + 1) = 0xff;
			*(bitsptr + 2) = 0xff;
		}
		bitsptr += 3;
		RunLength--;
	}
#endif
	return(0);
}

/* OUTPUTEOFB */
static INT outputeofb(UCHAR **dest)   /* place EOFB and zero padding on end of strip */
{
	INT count, i1;
	UCHAR onesmask[9] = { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
	UCHAR zeromask[9] = { 0xFF, 0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F };

	for (i1 = 0; i1 < 2; i1++) {
		count = 11;
		while (count--) {
			if (BitsLeftOut == 0) {
				BitsLeftOut = 8;
				(*dest)++;
			}
			**dest &= zeromask[BitsLeftOut--];
		}
		if (BitsLeftOut == 0) {
			BitsLeftOut = 8;
			(*dest)++;
		}
		**dest |= onesmask[BitsLeftOut--];
	}
	**dest &= 0xFF << BitsLeftOut;
	BitsLeftOut = 8;
	(*dest)++;
	return(0);
}

/* OUTPUTCODE */
static INT outputcode(INT zeroes, INT bitvalue, UCHAR **dest)  /* output a group 4 code */
{
	INT bitvalueptr = 8;
	UCHAR onesmask[9] = { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
	UCHAR zeromask[9] = { 0xFF, 0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F };

	while (zeroes--) {
		if (BitsLeftOut == 0) {
			BitsLeftOut = 8;
			(*dest)++;
		}
		**dest &= zeromask[BitsLeftOut--];
	}

	while (!(bitvalue & onesmask[bitvalueptr])) bitvalueptr--;

	while (bitvalueptr) {
		if (BitsLeftOut == 0) {
			BitsLeftOut = 8;
			(*dest)++;
		}
		if (bitvalue & onesmask[bitvalueptr--]) **dest |= onesmask[BitsLeftOut--];
		else **dest &= zeromask[BitsLeftOut--];
	}
	return(0);
}


/* FINDLENGTH */
static INT findlength(UCHAR *rowend, INT white)  /* findrunlength of pixmap */
{
	UCHAR onesmask[9] = { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
	INT count = 0;

	if (BitsLeftIn == 0) {
		BitsLeftIn = 8;
		bitsptr++;
	}
	if ((white == 1 && !oldbwtiff) || (white == 0 && oldbwtiff)) {
		while (bitsptr < rowend && (*bitsptr & onesmask[BitsLeftIn])) {			/* jpr 11/30/05 */
			BitsLeftIn--;
			if (BitsLeftIn == 0) {
				BitsLeftIn = 8;
				bitsptr++;
			}
			count++;
		}
	}
	else {
		while (bitsptr < rowend && !(*bitsptr & onesmask[BitsLeftIn])) {		/* jpr 11/30/05 */
			BitsLeftIn--;
			if (BitsLeftIn == 0) {
				BitsLeftIn = 8;
				bitsptr++;
			}
			count++;
		}
	}
	return(count);
}


/*
 * UNCMP
 *
 * uncompress one strip
 * depends on fields;tifstrip, tifstripsize, uncmpstripsize, uncmpstrip, Compression,
 * 		tifimagewidth, RowsPerStrip, SamplesPerPixel, codebits
 */
static UCHAR *uncmp(void)
{
	UCHAR c, *p, *lastaddr;
	INT tablecnt, code, toldcode, i, j, k, m;
	char *run;
	UINT bcUncmp = 0, maxUncmpBytes;
	UINT Uncmpsize = userTifMaxstripsize > UNCMPSIZE ? userTifMaxstripsize : UNCMPSIZE;

	if (Compression == COMPRESSION_NONE) {  /* no compression */
		uncmpstripsize = tifstripsize;
		return(tifstrip);
	}

	p = (*uncmpstrip);
	if (Compression == COMPRESSION_PACKBITS) {  /* PackBits */
		run = (char *) tifstrip;
		while (tifstripsize > 0) {
			k = *run;
			if (k < 0) {
				k = 1 - k;
				run++;
				memset(p, *run, k);
				p += k;
				run++;
				tifstripsize -= 2;
			}
			else if (k++ < 128) {
				run++;
				memcpy(p, run, k);
				p += k;
				run += k;
				tifstripsize -= (k + 1);
			}
		}
	}
	else
	{  /* LZW */

		/* There are three independant tests to decide when to stop
		   decoding. (Necessitated by odd little variations in TIFF output)
		   1) We see a code 257 which means STOP!
		   2) We exhaust the bytes in the Tiff strip.
		   3) We have output the maximum possible number of uncompressed bytes
		*/
		lastaddr = tifstrip + tifstripsize - 1;
		/* We don't have to worry about BitsPerSample in the following
		   statement because LZW is never used for less than 8 bits per pixel images.
		   Strictly speaking the TIFF standard allows it, but in practice
		   it is never done.
		   Except of course by good old Jon Ezard and company! They found one
		   on or about 5/1/2003 */
		maxUncmpBytes = tifimagewidth * RowsPerStrip * SamplesPerPixel;
		tablecnt = 258;     /* protect against bad first code */
		m = 8;  /* number of bits left in current byte (always 1 to 8) */
		codebits = 9;  /* number of bits per code */
		for (code = 0; ; ) {
			if (tifstrip > lastaddr) break;
			toldcode = code;
			i = codebits - m;
			code = ((*tifstrip++) << i);
			if (i > 7) {
				i -= 8;
				code += ((*tifstrip++) << i);
			}
			m = 8 - i;  /* i is number of bits to use from current byte */
			if (i) code += ((*tifstrip) >> m);
			code = code & lzwmask[codebits];
			if (code < 258) {
				if (code == 257)
					break;
				if (code == 256) {  /* clear table code */
					codebits = 9;
					for (tablecnt = 0; tablecnt < 258; tablecnt++) {
						(*table)[tablecnt] = tablecnt;
						(*tableprefix)[tablecnt] = (SHORT) -1;
					}
					continue;
				}
				*p++ = c = (UCHAR) code;
				if (++bcUncmp >= maxUncmpBytes) break;
				if (toldcode == 256) continue;
			}
			else if (code < tablecnt) {  /* is in table */
				i = Uncmpsize;
				j = code;
				for ( ; ; ) {
					(*uncmpstrip)[--i] = (*table)[j];
					if (i < 0) {
						return(NULL);
					}
					if ((*tableprefix)[j] == (SHORT) -1) break;
					j = (*tableprefix)[j];
				}
				j = Uncmpsize - i;
				memcpy(p, &((*uncmpstrip)[i]), j);
				bcUncmp += j;
				c = *p;
				p += j;
				if (bcUncmp >= maxUncmpBytes) break;
			}
			else if (code > tablecnt) {
				return(NULL);
			}
			else {  /* it's not in table */
				i = Uncmpsize - 1;
				j = toldcode;
				for ( ; ; ) {
					(*uncmpstrip)[--i] = (*table)[j];
					if (i < 0) {
						return(NULL);
					}
					if ((*tableprefix)[j] == (SHORT) -1) break;
					j = (*tableprefix)[j];
				}
				c = (*uncmpstrip)[Uncmpsize - 1] = (*table)[j];
				j = Uncmpsize - i;
				memcpy(p, &((*uncmpstrip)[i]), j);
				bcUncmp += j;
				p += j;
				if (bcUncmp >= maxUncmpBytes) break;
			}
			/* add string to table */
			(*table)[tablecnt] = c;
			(*tableprefix)[tablecnt++] = toldcode;
			if (tablecnt == 511 || tablecnt == 1023 || tablecnt == 2047)
				codebits++;
		}
	}
	uncmpstripsize = (INT) (ptrdiff_t) (p - (*uncmpstrip));
	return((*uncmpstrip));
}

/* CHECKMAP */
static INT checkmap(INT colormaplen, UCHAR *p)
{
	INT colorval;

	colorval = colormaplen * 2;
	while (colormaplen-- > 0) {
		if (getshort(p) >= 256 || getshort(p + colorval) >= 256 || getshort(p + colorval + colorval) >= 256)
			return (16);
		p += 2;
	}
	return(8);
}

/* GETSHORT */
static USHORT getshort(UCHAR *p)  /* return 2 byte number (possibly in other endian format) */
{
	union {  /* use union for alignment */
		UCHAR s[2];
		USHORT num;
	} work;

	if (!swapflag) memcpy(work.s, p, sizeof(USHORT));
	else {
		work.s[0] = p[1];
		work.s[1] = p[0];
	}
	return(work.num);
}

/* GETINT */
static UINT getint(UCHAR *p)  /* return 4 byte number (possibly in other endian format) */
{
	union {  /* use union for alignment */
		UCHAR s[4];
		UINT num;
	} work;

	if (!swapflag) memcpy(work.s, p, sizeof(UINT));
	else {
		work.s[0] = p[3];
		work.s[1] = p[2];
		work.s[2] = p[1];
		work.s[3] = p[0];
	}
	return(work.num);
}

/* TCODEINIT */
static void tcodeinit(void)   /* Initialize Run-Length Codes Array */
{
	memset(*WCode, 0xFF, 4200 * sizeof(SHORT));
	memset(*BCode, 0xFF, 8400 * sizeof(SHORT));

	(*BCode)[1078] = 0;
	(*BCode)[9] = 1;
	(*BCode)[6] = 2;
	(*BCode)[5] = 3;
	(*BCode)[10] = 4;
	(*BCode)[18] = 5;
	(*BCode)[17] = 6;
	(*BCode)[34] = 7;
	(*BCode)[68] = 8;
	(*BCode)[67] = 9;
	(*BCode)[131] = 10;
	(*BCode)[132] = 11;
	(*BCode)[134] = 12;
	(*BCode)[259] = 13;
	(*BCode)[262] = 14;
	(*BCode)[535] = 15;
	(*BCode)[1046] = 16;
	(*BCode)[1047] = 17;
	(*BCode)[1031] = 18;
	(*BCode)[2150] = 19;
	(*BCode)[2151] = 20;
	(*BCode)[2155] = 21;
	(*BCode)[2102] = 22;
	(*BCode)[2087] = 23;
	(*BCode)[2070] = 24;
	(*BCode)[2071] = 25;
	(*BCode)[4297] = 26;
	(*BCode)[4298] = 27;
	(*BCode)[4299] = 28;
	(*BCode)[4300] = 29;
	(*BCode)[4199] = 30;
	(*BCode)[4200] = 31;
	(*BCode)[4201] = 32;
	(*BCode)[4202] = 33;
	(*BCode)[4305] = 34;
	(*BCode)[4306] = 35;
	(*BCode)[4307] = 36;
	(*BCode)[4308] = 37;
	(*BCode)[4309] = 38;
	(*BCode)[4310] = 39;
	(*BCode)[4203] = 40;
	(*BCode)[4204] = 41;
	(*BCode)[4313] = 42;
	(*BCode)[4314] = 43;
	(*BCode)[4179] = 44;
	(*BCode)[4180] = 45;
	(*BCode)[4181] = 46;
	(*BCode)[4182] = 47;
	(*BCode)[4195] = 48;
	(*BCode)[4196] = 49;
	(*BCode)[4177] = 50;
	(*BCode)[4178] = 51;
	(*BCode)[4131] = 52;
	(*BCode)[4150] = 53;
	(*BCode)[4151] = 54;
	(*BCode)[4134] = 55;
	(*BCode)[4135] = 56;
	(*BCode)[4183] = 57;
	(*BCode)[4184] = 58;
	(*BCode)[4138] = 59;
	(*BCode)[4139] = 60;
	(*BCode)[4185] = 61;
	(*BCode)[4197] = 62;
	(*BCode)[4198] = 63;
	(*BCode)[1038] = 64;
	(*BCode)[4295] = 128;
	(*BCode)[4296] = 192;
	(*BCode)[4186] = 256;
	(*BCode)[4146] = 320;
	(*BCode)[4147] = 384;
	(*BCode)[4148] = 448;
	(*BCode)[8299] = 512;
	(*BCode)[8300] = 576;
	(*BCode)[8265] = 640;
	(*BCode)[8266] = 704;
	(*BCode)[8267] = 768;
	(*BCode)[8268] = 832;
	(*BCode)[8305] = 896;
	(*BCode)[8306] = 960;
	(*BCode)[8307] = 1024;
	(*BCode)[8308] = 1088;
	(*BCode)[8309] = 1152;
	(*BCode)[8310] = 1216;
	(*BCode)[8273] = 1280;
	(*BCode)[8274] = 1344;
	(*BCode)[8275] = 1408;
	(*BCode)[8276] = 1472;
	(*BCode)[8281] = 1536;
	(*BCode)[8282] = 1600;
	(*BCode)[8291] = 1664;
	(*BCode)[8292] = 1728;
	(*BCode)[2055] = 1792;
	(*BCode)[2059] = 1856;
	(*BCode)[2060] = 1920;
	(*BCode)[4113] = 1984;
	(*BCode)[4114] = 2048;
	(*BCode)[4115] = 2112;
	(*BCode)[4116] = 2176;
	(*BCode)[4117] = 2240;
	(*BCode)[4118] = 2304;
	(*BCode)[4123] = 2368;
	(*BCode)[4124] = 2432;
	(*BCode)[4125] = 2496;
	(*BCode)[4126] = 2560;
	(*BCode)[4096] = -2;          /* End-Of-Line */
	(*BCode)[2047] = -3;
	(*WCode)[308] = 0;
	(*WCode)[70] = 1;
	(*WCode)[22] = 2;
	(*WCode)[23] = 3;
	(*WCode)[26] = 4;
	(*WCode)[27] = 5;
	(*WCode)[29] = 6;
	(*WCode)[30] = 7;
	(*WCode)[50] = 8;
	(*WCode)[51] = 9;
	(*WCode)[38] = 10;
	(*WCode)[39] = 11;
	(*WCode)[71] = 12;
	(*WCode)[66] = 13;
	(*WCode)[115] = 14;
	(*WCode)[116] = 15;
	(*WCode)[105] = 16;
	(*WCode)[106] = 17;
	(*WCode)[166] = 18;
	(*WCode)[139] = 19;
	(*WCode)[135] = 20;
	(*WCode)[150] = 21;
	(*WCode)[130] = 22;
	(*WCode)[131] = 23;
	(*WCode)[167] = 24;
	(*WCode)[170] = 25;
	(*WCode)[146] = 26;
	(*WCode)[163] = 27;
	(*WCode)[151] = 28;
	(*WCode)[257] = 29;
	(*WCode)[258] = 30;
	(*WCode)[281] = 31;
	(*WCode)[282] = 32;
	(*WCode)[273] = 33;
	(*WCode)[274] = 34;
	(*WCode)[275] = 35;
	(*WCode)[276] = 36;
	(*WCode)[277] = 37;
	(*WCode)[278] = 38;
	(*WCode)[295] = 39;
	(*WCode)[296] = 40;
	(*WCode)[297] = 41;
	(*WCode)[298] = 42;
	(*WCode)[299] = 43;
	(*WCode)[300] = 44;
	(*WCode)[259] = 45;
	(*WCode)[260] = 46;
	(*WCode)[265] = 47;
	(*WCode)[266] = 48;
	(*WCode)[337] = 49;
	(*WCode)[338] = 50;
	(*WCode)[339] = 51;
	(*WCode)[340] = 52;
	(*WCode)[291] = 53;
	(*WCode)[292] = 54;
	(*WCode)[343] = 55;
	(*WCode)[344] = 56;
	(*WCode)[345] = 57;
	(*WCode)[346] = 58;
	(*WCode)[329] = 59;
	(*WCode)[330] = 60;
	(*WCode)[305] = 61;
	(*WCode)[306] = 62;
	(*WCode)[307] = 63;
	(*WCode)[58] = 64;
	(*WCode)[49] = 128;
	(*WCode)[86] = 192;
	(*WCode)[182] = 256;
	(*WCode)[309] = 320;
	(*WCode)[310] = 384;
	(*WCode)[355] = 448;
	(*WCode)[356] = 512;
	(*WCode)[359] = 576;
	(*WCode)[358] = 640;
	(*WCode)[715] = 704;
	(*WCode)[716] = 768;
	(*WCode)[721] = 832;
	(*WCode)[722] = 896;
	(*WCode)[723] = 960;
	(*WCode)[724] = 1024;
	(*WCode)[725] = 1088;
	(*WCode)[726] = 1152;
	(*WCode)[727] = 1216;
	(*WCode)[728] = 1280;
	(*WCode)[729] = 1344;
	(*WCode)[730] = 1408;
	(*WCode)[663] = 1472;
	(*WCode)[664] = 1536;
	(*WCode)[665] = 1600;
	(*WCode)[87] = 1664;
	(*WCode)[666] = 1728;
	(*WCode)[2055] = 1792;
	(*WCode)[2059] = 1856;
	(*WCode)[2060] = 1920;
	(*WCode)[4113] = 1984;
	(*WCode)[4114] = 2048;
	(*WCode)[4115] = 2112;
	(*WCode)[4116] = 2176;
	(*WCode)[4117] = 2240;
	(*WCode)[4118] = 2304;
	(*WCode)[4123] = 2368;
	(*WCode)[4124] = 2432;
	(*WCode)[4125] = 2496;
	(*WCode)[4126] = 2560;
	(*WCode)[4096] = -2;
	(*WCode)[2047] = -3;
}
/* RUNCODEINIT */
static void runcodeinit(void)
{
	(*blackbits)[0].leadzeroes = 4;
	(*blackbits)[0].bitstring = 55;
	(*blackbits)[1].leadzeroes = 1;
	(*blackbits)[1].bitstring = 2;
	(*blackbits)[2].leadzeroes = 0;
	(*blackbits)[2].bitstring = 3;
	(*blackbits)[3].leadzeroes = 0;
	(*blackbits)[3].bitstring = 2;
	(*blackbits)[4].leadzeroes = 1;
	(*blackbits)[4].bitstring = 3;
	(*blackbits)[5].leadzeroes = 2;
	(*blackbits)[5].bitstring = 3;
	(*blackbits)[6].leadzeroes = 2;
	(*blackbits)[6].bitstring = 2;
	(*blackbits)[7].leadzeroes = 3;
	(*blackbits)[7].bitstring = 3;
	(*blackbits)[8].leadzeroes = 3;
	(*blackbits)[8].bitstring = 5;
	(*blackbits)[9].leadzeroes = 3;
	(*blackbits)[9].bitstring = 4;
	(*blackbits)[10].leadzeroes = 4;
	(*blackbits)[10].bitstring = 4;
	(*blackbits)[11].leadzeroes = 4;
	(*blackbits)[11].bitstring = 5;
	(*blackbits)[12].leadzeroes = 4;
	(*blackbits)[12].bitstring = 7;
	(*blackbits)[13].leadzeroes = 5;
	(*blackbits)[13].bitstring = 4;
	(*blackbits)[14].leadzeroes = 5;
	(*blackbits)[14].bitstring = 7;
	(*blackbits)[15].leadzeroes = 4;
	(*blackbits)[15].bitstring = 24;
	(*blackbits)[16].leadzeroes = 5;
	(*blackbits)[16].bitstring = 23;
	(*blackbits)[17].leadzeroes = 5;
	(*blackbits)[17].bitstring = 24;
	(*blackbits)[18].leadzeroes = 6;
	(*blackbits)[18].bitstring = 8;
	(*blackbits)[19].leadzeroes = 4;
	(*blackbits)[19].bitstring = 103;
	(*blackbits)[20].leadzeroes = 4;
	(*blackbits)[20].bitstring = 104;
	(*blackbits)[21].leadzeroes = 4;
	(*blackbits)[21].bitstring = 108;
	(*blackbits)[22].leadzeroes = 5;
	(*blackbits)[22].bitstring = 55;
	(*blackbits)[23].leadzeroes = 5;
	(*blackbits)[23].bitstring = 40;
	(*blackbits)[24].leadzeroes = 6;
	(*blackbits)[24].bitstring = 23;
	(*blackbits)[25].leadzeroes = 6;
	(*blackbits)[25].bitstring = 24;
	(*blackbits)[26].leadzeroes = 4;
	(*blackbits)[26].bitstring = 202;
	(*blackbits)[27].leadzeroes = 4;
	(*blackbits)[27].bitstring = 203;
	(*blackbits)[28].leadzeroes = 4;
	(*blackbits)[28].bitstring = 204;
	(*blackbits)[29].leadzeroes = 4;
	(*blackbits)[29].bitstring = 205;
	(*blackbits)[30].leadzeroes = 5;
	(*blackbits)[30].bitstring = 104;
	(*blackbits)[31].leadzeroes = 5;
	(*blackbits)[31].bitstring = 105;
	(*blackbits)[32].leadzeroes = 5;
	(*blackbits)[32].bitstring = 106;
	(*blackbits)[33].leadzeroes = 5;
	(*blackbits)[33].bitstring = 107;
	(*blackbits)[34].leadzeroes = 4;
	(*blackbits)[34].bitstring = 210;
	(*blackbits)[35].leadzeroes = 4;
	(*blackbits)[35].bitstring = 211;
	(*blackbits)[36].leadzeroes = 4;
	(*blackbits)[36].bitstring = 212;
	(*blackbits)[37].leadzeroes = 4;
	(*blackbits)[37].bitstring = 213;
	(*blackbits)[38].leadzeroes = 4;
	(*blackbits)[38].bitstring = 214;
	(*blackbits)[39].leadzeroes = 4;
	(*blackbits)[39].bitstring = 215;
	(*blackbits)[40].leadzeroes = 5;
	(*blackbits)[40].bitstring = 108;
	(*blackbits)[41].leadzeroes = 5;
	(*blackbits)[41].bitstring = 109;
	(*blackbits)[42].leadzeroes = 4;
	(*blackbits)[42].bitstring = 218;
	(*blackbits)[43].leadzeroes = 4;
	(*blackbits)[43].bitstring = 219;
	(*blackbits)[44].leadzeroes = 5;
	(*blackbits)[44].bitstring = 84;
	(*blackbits)[45].leadzeroes = 5;
	(*blackbits)[45].bitstring = 85;
	(*blackbits)[46].leadzeroes = 5;
	(*blackbits)[46].bitstring = 86;
	(*blackbits)[47].leadzeroes = 5;
	(*blackbits)[47].bitstring = 87;
	(*blackbits)[48].leadzeroes = 5;
	(*blackbits)[48].bitstring = 100;
	(*blackbits)[49].leadzeroes = 5;
	(*blackbits)[49].bitstring = 101;
	(*blackbits)[50].leadzeroes = 5;
	(*blackbits)[50].bitstring = 82;
	(*blackbits)[51].leadzeroes = 5;
	(*blackbits)[51].bitstring = 83;
	(*blackbits)[52].leadzeroes = 6;
	(*blackbits)[52].bitstring = 36;
	(*blackbits)[53].leadzeroes = 6;
	(*blackbits)[53].bitstring = 55;
	(*blackbits)[54].leadzeroes = 6;
	(*blackbits)[54].bitstring = 56;
	(*blackbits)[55].leadzeroes = 6;
	(*blackbits)[55].bitstring = 39;
	(*blackbits)[56].leadzeroes = 6;
	(*blackbits)[56].bitstring = 40;
	(*blackbits)[57].leadzeroes = 5;
	(*blackbits)[57].bitstring = 88;
	(*blackbits)[58].leadzeroes = 5;
	(*blackbits)[58].bitstring = 89;
	(*blackbits)[59].leadzeroes = 6;
	(*blackbits)[59].bitstring = 43;
	(*blackbits)[60].leadzeroes = 6;
	(*blackbits)[60].bitstring = 44;
	(*blackbits)[61].leadzeroes = 5;
	(*blackbits)[61].bitstring = 90;
	(*blackbits)[62].leadzeroes = 5;
	(*blackbits)[62].bitstring = 102;
	(*blackbits)[63].leadzeroes = 5;
	(*blackbits)[63].bitstring = 103;
	(*blackbits)[64].leadzeroes = 6;
	(*blackbits)[64].bitstring = 15;
	(*blackbits)[65].leadzeroes = 4;
	(*blackbits)[65].bitstring = 200;
	(*blackbits)[66].leadzeroes = 4;
	(*blackbits)[66].bitstring = 201;
	(*blackbits)[67].leadzeroes = 5;
	(*blackbits)[67].bitstring = 91;
	(*blackbits)[68].leadzeroes = 6;
	(*blackbits)[68].bitstring = 51;
	(*blackbits)[69].leadzeroes = 6;
	(*blackbits)[69].bitstring = 52;
	(*blackbits)[70].leadzeroes = 6;
	(*blackbits)[70].bitstring = 53;
	(*blackbits)[71].leadzeroes = 6;
	(*blackbits)[71].bitstring = 108;
	(*blackbits)[72].leadzeroes = 6;
	(*blackbits)[72].bitstring = 109;
	(*blackbits)[73].leadzeroes = 6;
	(*blackbits)[73].bitstring = 74;
	(*blackbits)[74].leadzeroes = 6;
	(*blackbits)[74].bitstring = 75;
	(*blackbits)[75].leadzeroes = 6;
	(*blackbits)[75].bitstring = 76;
	(*blackbits)[76].leadzeroes = 6;
	(*blackbits)[76].bitstring = 77;
	(*blackbits)[77].leadzeroes = 6;
	(*blackbits)[77].bitstring = 114;
	(*blackbits)[78].leadzeroes = 6;
	(*blackbits)[78].bitstring = 115;
	(*blackbits)[79].leadzeroes = 6;
	(*blackbits)[79].bitstring = 116;
	(*blackbits)[80].leadzeroes = 6;
	(*blackbits)[80].bitstring = 117;
	(*blackbits)[81].leadzeroes = 6;
	(*blackbits)[81].bitstring = 118;
	(*blackbits)[82].leadzeroes = 6;
	(*blackbits)[82].bitstring = 119;
	(*blackbits)[83].leadzeroes = 6;
	(*blackbits)[83].bitstring = 82;
	(*blackbits)[84].leadzeroes = 6;
	(*blackbits)[84].bitstring = 83;
	(*blackbits)[85].leadzeroes = 6;
	(*blackbits)[85].bitstring = 84;
	(*blackbits)[86].leadzeroes = 6;
	(*blackbits)[86].bitstring = 85;
	(*blackbits)[87].leadzeroes = 6;
	(*blackbits)[87].bitstring = 90;
	(*blackbits)[88].leadzeroes = 6;
	(*blackbits)[88].bitstring = 91;
	(*blackbits)[89].leadzeroes = 6;
	(*blackbits)[89].bitstring = 100;
	(*blackbits)[90].leadzeroes = 6;
	(*blackbits)[90].bitstring = 101;
	(*blackbits)[91].leadzeroes = 7;
	(*blackbits)[91].bitstring = 8;
	(*blackbits)[92].leadzeroes = 7;
	(*blackbits)[92].bitstring = 12;
	(*blackbits)[93].leadzeroes = 7;
	(*blackbits)[93].bitstring = 13;
	(*blackbits)[94].leadzeroes = 7;
	(*blackbits)[94].bitstring = 18;
	(*blackbits)[95].leadzeroes = 7;
	(*blackbits)[95].bitstring = 19;
	(*blackbits)[96].leadzeroes = 7;
	(*blackbits)[96].bitstring = 20;
	(*blackbits)[97].leadzeroes = 7;
	(*blackbits)[97].bitstring = 21;
	(*blackbits)[98].leadzeroes = 7;
	(*blackbits)[98].bitstring = 22;
	(*blackbits)[99].leadzeroes = 7;
	(*blackbits)[99].bitstring = 23;
	(*blackbits)[100].leadzeroes = 7;
	(*blackbits)[100].bitstring = 28;
	(*blackbits)[101].leadzeroes = 7;
	(*blackbits)[101].bitstring = 29;
	(*blackbits)[102].leadzeroes = 7;
	(*blackbits)[102].bitstring = 30;
	(*blackbits)[103].leadzeroes = 7;
	(*blackbits)[103].bitstring = 31;
	(*whitebits)[0].leadzeroes = 2;
	(*whitebits)[0].bitstring = 53;
	(*whitebits)[1].leadzeroes = 3;
	(*whitebits)[1].bitstring = 7;
	(*whitebits)[2].leadzeroes = 1;
	(*whitebits)[2].bitstring = 7;
	(*whitebits)[3].leadzeroes = 0;
	(*whitebits)[3].bitstring = 8;
	(*whitebits)[4].leadzeroes = 0;
	(*whitebits)[4].bitstring = 11;
	(*whitebits)[5].leadzeroes = 0;
	(*whitebits)[5].bitstring = 12;
	(*whitebits)[6].leadzeroes = 0;
	(*whitebits)[6].bitstring = 14;
	(*whitebits)[7].leadzeroes = 0;
	(*whitebits)[7].bitstring = 15;
	(*whitebits)[8].leadzeroes = 0;
	(*whitebits)[8].bitstring = 19;
	(*whitebits)[9].leadzeroes = 0;
	(*whitebits)[9].bitstring = 20;
	(*whitebits)[10].leadzeroes = 2;
	(*whitebits)[10].bitstring = 7;
	(*whitebits)[11].leadzeroes = 1;
	(*whitebits)[11].bitstring = 8;
	(*whitebits)[12].leadzeroes = 2;
	(*whitebits)[12].bitstring = 8;
	(*whitebits)[13].leadzeroes = 4;
	(*whitebits)[13].bitstring = 3;
	(*whitebits)[14].leadzeroes = 0;
	(*whitebits)[14].bitstring = 52;
	(*whitebits)[15].leadzeroes = 0;
	(*whitebits)[15].bitstring = 53;
	(*whitebits)[16].leadzeroes = 0;
	(*whitebits)[16].bitstring = 42;
	(*whitebits)[17].leadzeroes = 0;
	(*whitebits)[17].bitstring = 43;
	(*whitebits)[18].leadzeroes = 1;
	(*whitebits)[18].bitstring = 39;
	(*whitebits)[19].leadzeroes = 3;
	(*whitebits)[19].bitstring = 12;
	(*whitebits)[20].leadzeroes = 3;
	(*whitebits)[20].bitstring = 8;
	(*whitebits)[21].leadzeroes = 2;
	(*whitebits)[21].bitstring = 23;
	(*whitebits)[22].leadzeroes = 5;
	(*whitebits)[22].bitstring = 3;
	(*whitebits)[23].leadzeroes = 4;
	(*whitebits)[23].bitstring = 4;
	(*whitebits)[24].leadzeroes = 1;
	(*whitebits)[24].bitstring = 40;
	(*whitebits)[25].leadzeroes = 1;
	(*whitebits)[25].bitstring = 43;
	(*whitebits)[26].leadzeroes = 2;
	(*whitebits)[26].bitstring = 19;
	(*whitebits)[27].leadzeroes = 1;
	(*whitebits)[27].bitstring = 36;
	(*whitebits)[28].leadzeroes = 2;
	(*whitebits)[28].bitstring = 24;
	(*whitebits)[29].leadzeroes = 6;
	(*whitebits)[29].bitstring = 2;
	(*whitebits)[30].leadzeroes = 6;
	(*whitebits)[30].bitstring = 3;
	(*whitebits)[31].leadzeroes = 3;
	(*whitebits)[31].bitstring = 26;
	(*whitebits)[32].leadzeroes = 3;
	(*whitebits)[32].bitstring = 27;
	(*whitebits)[33].leadzeroes = 3;
	(*whitebits)[33].bitstring = 18;
	(*whitebits)[34].leadzeroes = 3;
	(*whitebits)[34].bitstring = 19;
	(*whitebits)[35].leadzeroes = 3;
	(*whitebits)[35].bitstring = 20;
	(*whitebits)[36].leadzeroes = 3;
	(*whitebits)[36].bitstring = 21;
	(*whitebits)[37].leadzeroes = 3;
	(*whitebits)[37].bitstring = 22;
	(*whitebits)[38].leadzeroes = 3;
	(*whitebits)[38].bitstring = 23;
	(*whitebits)[39].leadzeroes = 2;
	(*whitebits)[39].bitstring = 40;
	(*whitebits)[40].leadzeroes = 2;
	(*whitebits)[40].bitstring = 41;
	(*whitebits)[41].leadzeroes = 2;
	(*whitebits)[41].bitstring = 42;
	(*whitebits)[42].leadzeroes = 2;
	(*whitebits)[42].bitstring = 43;
	(*whitebits)[43].leadzeroes = 2;
	(*whitebits)[43].bitstring = 44;
	(*whitebits)[44].leadzeroes = 2;
	(*whitebits)[44].bitstring = 45;
	(*whitebits)[45].leadzeroes = 5;
	(*whitebits)[45].bitstring = 4;
	(*whitebits)[46].leadzeroes = 5;
	(*whitebits)[46].bitstring = 5;
	(*whitebits)[47].leadzeroes = 4;
	(*whitebits)[47].bitstring = 10;
	(*whitebits)[48].leadzeroes = 4;
	(*whitebits)[48].bitstring = 11;
	(*whitebits)[49].leadzeroes = 1;
	(*whitebits)[49].bitstring = 82;
	(*whitebits)[50].leadzeroes = 1;
	(*whitebits)[50].bitstring = 83;
	(*whitebits)[51].leadzeroes = 1;
	(*whitebits)[51].bitstring = 84;
	(*whitebits)[52].leadzeroes = 1;
	(*whitebits)[52].bitstring = 85;
	(*whitebits)[53].leadzeroes = 2;
	(*whitebits)[53].bitstring = 36;
	(*whitebits)[54].leadzeroes = 2;
	(*whitebits)[54].bitstring = 37;
	(*whitebits)[55].leadzeroes = 1;
	(*whitebits)[55].bitstring = 88;
	(*whitebits)[56].leadzeroes = 1;
	(*whitebits)[56].bitstring = 89;
	(*whitebits)[57].leadzeroes = 1;
	(*whitebits)[57].bitstring = 90;
	(*whitebits)[58].leadzeroes = 1;
	(*whitebits)[58].bitstring = 91;
	(*whitebits)[59].leadzeroes = 1;
	(*whitebits)[59].bitstring = 74;
	(*whitebits)[60].leadzeroes = 1;
	(*whitebits)[60].bitstring = 75;
	(*whitebits)[61].leadzeroes = 2;
	(*whitebits)[61].bitstring = 50;
	(*whitebits)[62].leadzeroes = 2;
	(*whitebits)[62].bitstring = 51;
	(*whitebits)[63].leadzeroes = 2;
	(*whitebits)[63].bitstring = 52;
	(*whitebits)[64].leadzeroes = 0;
	(*whitebits)[64].bitstring = 27;
	(*whitebits)[65].leadzeroes = 0;
	(*whitebits)[65].bitstring = 18;
	(*whitebits)[66].leadzeroes = 1;
	(*whitebits)[66].bitstring = 23;
	(*whitebits)[67].leadzeroes = 1;
	(*whitebits)[67].bitstring = 55;
	(*whitebits)[68].leadzeroes = 2;
	(*whitebits)[68].bitstring = 54;
	(*whitebits)[69].leadzeroes = 2;
	(*whitebits)[69].bitstring = 55;
	(*whitebits)[70].leadzeroes = 1;
	(*whitebits)[70].bitstring = 100;
	(*whitebits)[71].leadzeroes = 1;
	(*whitebits)[71].bitstring = 101;
	(*whitebits)[72].leadzeroes = 1;
	(*whitebits)[72].bitstring = 104;
	(*whitebits)[73].leadzeroes = 1;
	(*whitebits)[73].bitstring = 103;
	(*whitebits)[74].leadzeroes = 1;
	(*whitebits)[74].bitstring = 204;
	(*whitebits)[75].leadzeroes = 1;
	(*whitebits)[75].bitstring = 205;
	(*whitebits)[76].leadzeroes = 1;
	(*whitebits)[76].bitstring = 210;
	(*whitebits)[77].leadzeroes = 1;
	(*whitebits)[77].bitstring = 211;
	(*whitebits)[78].leadzeroes = 1;
	(*whitebits)[78].bitstring = 212;
	(*whitebits)[79].leadzeroes = 1;
	(*whitebits)[79].bitstring = 213;
	(*whitebits)[80].leadzeroes = 1;
	(*whitebits)[80].bitstring = 214;
	(*whitebits)[81].leadzeroes = 1;
	(*whitebits)[81].bitstring = 215;
	(*whitebits)[82].leadzeroes = 1;
	(*whitebits)[82].bitstring = 216;
	(*whitebits)[83].leadzeroes = 1;
	(*whitebits)[83].bitstring = 217;
	(*whitebits)[84].leadzeroes = 1;
	(*whitebits)[84].bitstring = 218;
	(*whitebits)[85].leadzeroes = 1;
	(*whitebits)[85].bitstring = 219;
	(*whitebits)[86].leadzeroes = 1;
	(*whitebits)[86].bitstring = 152;
	(*whitebits)[87].leadzeroes = 1;
	(*whitebits)[87].bitstring = 153;
	(*whitebits)[88].leadzeroes = 1;
	(*whitebits)[88].bitstring = 154;
	(*whitebits)[89].leadzeroes = 1;
	(*whitebits)[89].bitstring = 24;
	(*whitebits)[90].leadzeroes = 1;
	(*whitebits)[90].bitstring = 155;
	(*whitebits)[91].leadzeroes = 7;
	(*whitebits)[91].bitstring = 8;
	(*whitebits)[92].leadzeroes = 7;
	(*whitebits)[92].bitstring = 12;
	(*whitebits)[93].leadzeroes = 7;
	(*whitebits)[93].bitstring = 13;
	(*whitebits)[94].leadzeroes = 7;
	(*whitebits)[94].bitstring = 18;
	(*whitebits)[95].leadzeroes = 7;
	(*whitebits)[95].bitstring = 19;
	(*whitebits)[96].leadzeroes = 7;
	(*whitebits)[96].bitstring = 20;
	(*whitebits)[97].leadzeroes = 7;
	(*whitebits)[97].bitstring = 21;
	(*whitebits)[98].leadzeroes = 7;
	(*whitebits)[98].bitstring = 22;
	(*whitebits)[99].leadzeroes = 7;
	(*whitebits)[99].bitstring = 23;
	(*whitebits)[100].leadzeroes = 7;
	(*whitebits)[100].bitstring = 28;
	(*whitebits)[101].leadzeroes = 7;
	(*whitebits)[101].bitstring = 29;
	(*whitebits)[102].leadzeroes = 7;
	(*whitebits)[102].bitstring = 30;
	(*whitebits)[103].leadzeroes = 7;
	(*whitebits)[103].bitstring = 31;
	return;
}

/* GUIAGIFTOPIXMAP */
INT  guiaGifToPixmap(PIXHANDLE pixhandle, UCHAR **bufhandle)
{
	INT rc;
	PIXMAP *pix;

#if OS_WIN32GUI
	INT line, colorpos, scansize;
	HCURSOR hCursor;
	HPALETTE oldhPalette, newhPalette;
	CHAR work[64];
#endif

	pix = *pixhandle;
	bpp = pix->bpp;

#if OS_WIN32GUI
	hwnd = guia_hwnd();
	hdc = GetDC(hwnd);
	if (!bpp) {
		bpp = GetDeviceCaps(hdc, BITSPIXEL) * GetDeviceCaps(hdc, PLANES);
		if (bpp == 32 || bpp == 16) bpp = 24;
	}
	if (bpp <= 1) numcolors = 2;
	else if (bpp <= 4) numcolors = 16;
	else if (bpp <= 8) numcolors = 256;
	else numcolors = 0;
	if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24) return RC_ERROR;
#else
	if (bpp <= 1) numcolors = 2;
	else numcolors = 0;
	if (bpp != 1 && bpp != 24) return RC_ERROR;
#endif

	hsize = pix->hsize;
	vsize = pix->vsize;

#if OS_WIN32GUI
	if (numcolors) {
		pLogPal = (PLOGPALETTE)  guiAllocMem(sizeof(LOGPALETTE) + numcolors * sizeof(PALETTEENTRY));
		if (pLogPal == NULL) {
			sprintf(work, "guiAllocMem returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, sizeof(LOGPALETTE) + numcolors * sizeof(PALETTEENTRY));
			return RC_NO_MEM;
		}
		pLogPal->palVersion = 0x300;  /* Windows 3.0 or later */
		pLogPal->palNumEntries = numcolors;
	}
	else pLogPal = NULL;

	bitsrowbytes = ((hsize * bpp + 0x1F) & ~0x1F) >> 3;
	if (NULL == pix->pbmi) {
		pbmi = (BITMAPINFO *) malloc(sizeof(BITMAPINFOHEADER) + (numcolors * sizeof(RGBQUAD)));
		if (NULL == pbmi) {
			sprintf(work, "malloc returned null in %s", __FUNCTION__);
			guiaErrorMsg(work, sizeof(BITMAPINFOHEADER) + (numcolors * sizeof(RGBQUAD)));
			return RC_NO_MEM;
		}
		memset(&pbmi->bmiHeader, 0, sizeof(BITMAPINFOHEADER));
		pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		pbmi->bmiHeader.biWidth = hsize;
		pbmi->bmiHeader.biHeight = vsize;
		pbmi->bmiHeader.biPlanes = 1;
		pbmi->bmiHeader.biBitCount = (unsigned short) bpp;
		pbmi->bmiHeader.biCompression = BI_RGB;
		pbmi->bmiHeader.biSizeImage = bitsrowbytes * vsize;
		pbmi->bmiHeader.biXPelsPerMeter = 1000;
		pbmi->bmiHeader.biYPelsPerMeter = 1000;
		pbmi->bmiHeader.biClrUsed = numcolors;
		pbmi->bmiHeader.biClrImportant = numcolors;
		pix->pbmi = pbmi;
	}
	else pbmi = pix->pbmi;

	bits = (UCHAR *) malloc(bitsrowbytes * vsize);
	if (bits == NULL) {
		if(numcolors) guiFreeMem((UCHAR*) pLogPal);
		return RC_NO_MEM;
	}

	hCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
	ShowCursor(TRUE);

	if (pix->hpal != NULL) {
		oldhPalette = SelectPalette(hdc, pix->hpal, TRUE);
		RealizePalette(hdc);
	}
	else oldhPalette = NULL;

	line = 0;
	GetDIBits(hdc, pix->hbitmap, line, vsize, bits, pbmi, DIB_RGB_COLORS);
#else
	bits = *pix->pixbuf;
	bitsrowbytes = ((hsize * bpp + 0x1F) & ~0x1F) >> 3;
#endif

	if ((firstcodestack = (UCHAR *) malloc(4096)) == NULL) {
#if OS_WIN32GUI
		free(bits);
		if (numcolors) guiFreeMem((UCHAR*) pLogPal);
		ShowCursor(FALSE);
		SetCursor(hCursor);
#endif
		return RC_NO_MEM;
	}

	if ((lastcodestack = (UCHAR *) malloc(4096)) == NULL) {
		free(firstcodestack);
#if OS_WIN32GUI
		free(bits);
		if (numcolors) guiFreeMem((UCHAR*) pLogPal);
		ShowCursor(FALSE);
		SetCursor(hCursor);
#endif
		return RC_NO_MEM;
	}

	if ((strque = (UCHAR *) malloc(4096)) == NULL) {
		free(firstcodestack);
		free(lastcodestack);
#if OS_WIN32GUI
		free(bits);
		if (numcolors) guiFreeMem((UCHAR*) pLogPal);
		ShowCursor(FALSE);
		SetCursor(hCursor);
#endif
		return RC_NO_MEM;
	}

	if ((codestack = (INT *) calloc(4096, sizeof(INT))) == NULL) {
		free(strque);
		free(firstcodestack);
		free(lastcodestack);
#if OS_WIN32GUI
		free(bits);
		if (numcolors) guiFreeMem((UCHAR*) pLogPal);
		ShowCursor(FALSE);
		SetCursor(hCursor);
#endif
		return RC_NO_MEM;
	}

	bits_end = bits + bitsrowbytes * vsize;
#if OS_WIN32GUI
	bitsptr = bits_end - bitsrowbytes;
#else
	bitsptr = bits;
#endif
	/*cg = buffer;*/
	rc = fromgif(bufhandle);

#if OS_WIN32GUI
	if (!rc) {
		if (vsize > gifimagelength) {
			line = vsize - gifimagelength;
			bitsptr = bits_end - bitsrowbytes * gifimagelength;
			scansize = gifimagelength;
		}
		else {
			line = 0;
			bitsptr = bits;
			scansize = vsize;
		}

		if (numcolors) {
			for (colorpos = 0; colorpos < numcolors; colorpos++) {
				pbmi->bmiColors[colorpos].rgbRed = pLogPal->palPalEntry[colorpos].peRed;
				pbmi->bmiColors[colorpos].rgbGreen = pLogPal->palPalEntry[colorpos].peGreen;
				pbmi->bmiColors[colorpos].rgbBlue = pLogPal->palPalEntry[colorpos].peBlue;
			}
			newhPalette = CreatePalette(pLogPal);
			guiFreeMem((UCHAR*) pLogPal);
			if (pix->winshow != NULL && (*pix->winshow)->hwnd == GetActiveWindow()) {
				SelectPalette(hdc, newhPalette, FALSE);
			}
			else SelectPalette(hdc, newhPalette, TRUE);
			RealizePalette(hdc);
			if (pix->hpal != NULL) DeleteObject(pix->hpal);
			pix->hpal = newhPalette;
		}

		SetDIBits(hdc, pix->hbitmap, line, scansize, bitsptr, pbmi, DIB_RGB_COLORS);
		if(oldhPalette != NULL) {
			SelectPalette(hdc, oldhPalette, TRUE);
			RealizePalette(hdc);
		}
	}

	free(strque);
	free(firstcodestack);
	free(lastcodestack);
	free(codestack);
	free(bits);
	if (NULL == pix->pbmi) free(pbmi);
	ReleaseDC(hwnd, hdc);
	if (!rc && pix->winshow != NULL) forceRepaint(pixhandle);
	ShowCursor(FALSE);
	SetCursor(hCursor);
#endif
	return(rc);
}


/* FROMGIF */
static INT fromgif(UCHAR **bufhandle)
{
	INT i1, colormaplen, colorpos, retval;
	USHORT green, blue, red;
	UCHAR *cg = *bufhandle;

#if OS_WIN32GUI
	INT deltar, deltag, deltab;
	INT clrdist, minclrdist;
	INT bestclr, palcolor;
#endif

	gh.sig[0] = *cg;
	gh.sig[1] = *(cg + 1);
	gh.sig[2] = *(cg + 2);
	if (memcmp(gh.sig, "GIF", 3)) return RC_ERROR;  /* check to see if gif file, return error value? if not */
	cg += 10;		/* Advance pointer to Screen and Color Map Information */
	gh.flags = *cg;	/* get global flags */
	cg += 3;		/* Advance pointer to 1st byte after the header */
	if (gh.flags & 0x80) {  /* test if there is a global color map */
		colormaplen = 1 << ((gh.flags & 7) + 1); /* number of colormap entries */

		/* If the bpp of the PIXMAP we are moving this GIF into is 1, then
		 * the colors array is not used at all. We still have to advance the
		 * cg pointer past the Global Color Table */
		if (bpp == 1) {
			cg = cg + (3 * colormaplen);
#if OS_WIN32GUI
			/* If this is the gui version of the runtime, the logical palette
			 * is expected later on to have something in it.
			 * We fake it here with a black and white entry */
			pLogPal->palPalEntry[0].peRed = (UCHAR) 0;
			pLogPal->palPalEntry[0].peGreen = (UCHAR) 0;
			pLogPal->palPalEntry[0].peBlue = (UCHAR) 0;
			pLogPal->palPalEntry[0].peFlags = 0;
			pLogPal->palPalEntry[1].peRed = (UCHAR) 0xff;
			pLogPal->palPalEntry[1].peGreen = (UCHAR) 0xff;
			pLogPal->palPalEntry[1].peBlue = (UCHAR) 0xff;
			pLogPal->palPalEntry[1].peFlags = 0;
#endif
			goto checklocalcolormap;
		}

		for (i1 = 0, colorpos = 0; i1 < colormaplen; i1++, cg += 3) {
			red = *cg;
			green = *(cg + 1);
			blue = *(cg + 2);
			if (bpp == 24) {
				colors[colorpos++] = (UCHAR) red;
				colors[colorpos++] = (UCHAR) green;
				colors[colorpos++] = (UCHAR) blue;
			}
			else {
#if OS_WIN32GUI
				if (colorpos >= numcolors) {
					minclrdist = INT_MAX;
					for (palcolor = 0; palcolor < numcolors; palcolor++) {
						deltar = pLogPal->palPalEntry[palcolor].peRed - red;
						deltag = pLogPal->palPalEntry[palcolor].peGreen - green;
						deltab =  pLogPal->palPalEntry[palcolor].peBlue - blue;
						clrdist = deltar * deltar + deltag * deltag + deltab * deltab;
						if (clrdist < minclrdist) {
							minclrdist = clrdist;
							bestclr = palcolor;
						}
					}
					pLogPal->palPalEntry[bestclr].peRed = (UCHAR) red;
					pLogPal->palPalEntry[bestclr].peGreen = (UCHAR) green;
					pLogPal->palPalEntry[bestclr].peBlue = (UCHAR) blue;
					pLogPal->palPalEntry[bestclr].peFlags = 0;
					colors[colorpos] = bestclr;
				}
				else {
					pLogPal->palPalEntry[colorpos].peRed = (UCHAR) red;
					pLogPal->palPalEntry[colorpos].peGreen = (UCHAR) green;
					pLogPal->palPalEntry[colorpos].peBlue = (UCHAR) blue;
					pLogPal->palPalEntry[colorpos].peFlags = 0;
					colors[colorpos] = colorpos;
				}
				colorpos++;
#endif
#if OS_MAC
				rgb.red = * (USHORT *) cg;
				rgb.green = * (USHORT *) (cg + 1);
				rgb.blue = * (USHORT *) (cg + 2);
				colors[colorpos++] = (UCHAR) Color2Index(&rgb);
#endif
			}
		}
	}
	checklocalcolormap:

	/* loop to read image blocks */
	while (*cg == 0x2C /* Local Image Descriptor */
				|| *cg == 0x21 /* Extension Introducer */
				|| *cg == 0x3B /* Trailer */) {
		if (*cg == 0x2C) {

			cg += 5;	/* Skip X and Y position words */
			/* GIF uses 'little-endian' philosophy */
			gifimagewidth = ((USHORT) (*(cg + 1)) << 8) + (USHORT) *(cg);
			gifimagelength = ((USHORT) (*(cg + 3)) << 8) + (USHORT) (*(cg + 2));
			cg += 4;	/* Advance to the Image and Color Table Data Information byte */
			iblk.flags = *cg;
			cg++;	/* Point at 1st byte of image data or, maybe, a local color table */
			if (iblk.flags & 0x80) {	/* Is there a local color map? */
				colormaplen = 1 << ((iblk.flags & 7) + 1); /* number of entries in the colormap */

				/* If the bpp of the PIXMAP we are moving this GIF into is 1, then
				 * the colors array is not used at all. We still have to advance the
				 * cg pointer past the Local Color Table */
				if (bpp == 1) {
					cg = cg + (3 * colormaplen);
					goto endoflocalcolormap;
				}
				for (i1 = 0, colorpos = 0; i1 < colormaplen; i1++, cg += 3) {
					red = *cg;
					green = *(cg + 1);
					blue = *(cg + 2);
					if (bpp == 24) {
						colors[colorpos++] = (UCHAR) red;
						colors[colorpos++] = (UCHAR) green;
						colors[colorpos++] = (UCHAR) blue;
					}
					else {
#if OS_WIN32GUI
						if (colorpos >= numcolors) {
							minclrdist = INT_MAX;
							for (palcolor = 0; palcolor < numcolors; palcolor++) {
								deltar = pLogPal->palPalEntry[palcolor].peRed - red;
								deltag = pLogPal->palPalEntry[palcolor].peGreen - green;
								deltab =  pLogPal->palPalEntry[palcolor].peBlue - blue;
								clrdist = deltar * deltar + deltag * deltag + deltab * deltab;
								if (clrdist < minclrdist) {
									minclrdist = clrdist;
									bestclr = palcolor;
								}
							}
							pLogPal->palPalEntry[bestclr].peRed = (UCHAR) red;
							pLogPal->palPalEntry[bestclr].peGreen = (UCHAR) green;
							pLogPal->palPalEntry[bestclr].peBlue = (UCHAR) blue;
							pLogPal->palPalEntry[bestclr].peFlags = 0;
							colors[colorpos] = bestclr;
						}
						else {
							pLogPal->palPalEntry[colorpos].peRed = (UCHAR) red;
							pLogPal->palPalEntry[colorpos].peGreen = (UCHAR) green;
							pLogPal->palPalEntry[colorpos].peBlue = (UCHAR) blue;
							pLogPal->palPalEntry[colorpos].peFlags = 0;
							colors[colorpos] = colorpos;
						}
						colorpos++;
#endif
#if OS_MAC
						rgb.red = * (USHORT *) cg;
						rgb.green = * (USHORT *) (cg + 1);
						rgb.blue = * (USHORT *) (cg + 2);
						colors[colorpos++] = (UCHAR) Color2Index(&rgb);
#endif
					}
				}
			}

			endoflocalcolormap:
			if (*cg == 0x3B /* Trailer byte */) return(-3);

			flags = iblk.flags;
			retval = -3;
			if (bpp == 1) retval = fromgif1(cg);
			else if (bpp == 4) retval = fromgif2(cg);
			else if (bpp == 8) retval = fromgif3(cg);
			else if (bpp == 24) retval = fromgif4(cg);
			if (retval) return(retval);
		}
		else if (*cg == 0x21) {  /* extension block */
			extensionGif(&cg);
		}
		else return(0);  /* end of file */
	}
	return(0);
}

/* FROMGIF1 */
static INT fromgif1(UCHAR *cg) /* convert gif to 1 bpp Pix */
{
	INT j, q, r, s, i1;
	UCHAR lace;    /* flag true if image laced */
	UCHAR tmp;

	if (hsize < gifimagewidth) j = hsize;
	else j = gifimagewidth;
	if ((gifline = (UCHAR *) malloc(gifimagewidth)) == NULL) return RC_NO_MEM;

	tmp = 0;
	if (flags & 0x40) lace = 1;  /* check lacing */
	else lace = 0;

	if ( (i1 = uncompinit(&cg)) ) {           /* initialize values for decompression */
		free(gifline);
		return i1;
	}

	if (lace) {
		for (q = 0; q < gifimagelength; q += 8) {
			if (uncompGif(&cg)) {
				free(gifline);
				return(-3);
			}
			if (q < vsize) {              /* into *gifline */
#if OS_WIN32GUI
				for (bitsptr = bits_end - (q + 1) * bitsrowbytes, r = 0; r < j; r += 8) {
#else
				for (bitsptr = bits + q * bitsrowbytes, r = 0; r < j; r += 8) {
#endif
					for (s = 7; s >= 0; s--) {
						if (*(gifline + r + s)) tmp |= 0x01 << (7 - s);
					}
					*bitsptr++ = tmp;
					tmp = 0;
				}
			}
		}
		for (q = 4; q < gifimagelength; q += 8) {
			if (uncompGif(&cg)) {
				free(gifline);
				return(-3);
			}
			if (q < vsize) {              /* into *gifline */
#if OS_WIN32GUI
				for (bitsptr = bits_end - (q + 1) * bitsrowbytes, r = 0; r < j; r += 8) {
#else
				for (bitsptr = bits + q * bitsrowbytes, r = 0; r < j; r += 8) {
#endif
					for (s = 7; s >= 0; s--) {
						if (*(gifline + r + s)) tmp |= 0x01 << (7 - s);
					}
					*bitsptr++ = tmp;
					tmp = 0;
				}
			}
		}
		for (q = 2; q < gifimagelength; q += 4) {
			if (uncompGif(&cg)) {
				free(gifline);
				return(-3);
			}
			if (q < vsize) {              /* into *gifline */
#if OS_WIN32GUI
				for (bitsptr = bits_end - (q + 1) * bitsrowbytes, r = 0; r < j; r += 8) {
#else
				for (bitsptr = bits + q * bitsrowbytes, r = 0; r < j; r += 8) {
#endif
					for (s = 7; s >= 0; s--) {
						if (*(gifline + r + s)) tmp |= 0x01 << (7 - s);
					}
					*bitsptr++ = tmp;
					tmp = 0;
				}
			}
		}
		for (q = 1; q < gifimagelength; q += 2) {
			if (uncompGif(&cg)) {
				free(gifline);
				return(-3);
			}
			if (q < vsize) {              /* into *gifline */
#if OS_WIN32GUI
				for (bitsptr = bits_end - (q + 1) * bitsrowbytes, r = 0; r < j; r += 8) {
#else
				for (bitsptr = bits + q * bitsrowbytes, r = 0; r < j; r += 8) {
#endif
					for (s = 7; s >= 0; s--) {
						if (*(gifline + r + s)) tmp |= 0x01 << (7 - s);
					}
					*bitsptr++ = tmp;
					tmp = 0;
				}
			}
		}

	}
	else {
		for (q = 0; q < gifimagelength; q++) {
			if (uncompGif(&cg)) {
				free(gifline);
				return(-3);
			}
			if (q < vsize) {              /* into *gifline */
#if OS_WIN32GUI
				for (bitsptr = bits_end - (q + 1) * bitsrowbytes, r = 0; r < j; r += 8) {
#else
				for (bitsptr = bits + q * bitsrowbytes, r = 0; r < j; r += 8) {
#endif
					for (s = 7; s >= 0; s--) {
						if (*(gifline + r + s)) tmp |= 0x01 << (7 - s);
					}
					*bitsptr++ = tmp;
					tmp = 0;
				}
			}
		}
	}
	free(gifline);
	free(block);
	return(0);
}

/* FROMGIF2 */
static INT fromgif2(UCHAR *cg)  /* convert gif image to 4 bpp Pix */
{

	INT j, q, i, i1;
	UCHAR lace;    /* flag true if image laced */
	UCHAR tmp;

	if (hsize < gifimagewidth) j = hsize;
	else j = gifimagewidth;
	if ((gifline = (UCHAR *) malloc(gifimagewidth)) == NULL) return RC_NO_MEM;

	tmp = 0;
	if (flags & 0x40) lace = 1;  /* check lacing */
	else lace = 0;

	if ( (i1 = uncompinit(&cg)) ) {           /* initialize values for decompression */
		free(gifline);
		return i1;
	}

	if (lace) {
		for (q = 0; q < gifimagelength; q += 8) {
			if (uncompGif(&cg)) {
				free(gifline);
				return(-3);
			}
			if (q < vsize) {              /* into *gifline */
#if OS_WIN32GUI
				for (i = 0, bitsptr = bits_end - (q + 1) * bitsrowbytes; i < j; i += 2) {
#else
				for (i = 0, bitsptr = bits + q * bitsrowbytes; i < j; i += 2) {
#endif
					tmp |= colors[*(gifline + i)] << 4;
					tmp |= colors[*(gifline + i + 1)];
					*bitsptr++ = tmp;
					tmp = 0;
				}
			}
		}
		for (q = 4; q < gifimagelength; q += 8) {
			if (uncompGif(&cg)) {
				free(gifline);
				return(-3);
			}
			if (q < vsize) {              /* into *gifline */
#if OS_WIN32GUI
				for (i = 0, bitsptr = bits_end - (q + 1) * bitsrowbytes; i < j; i += 2) {
#else
				for (i = 0, bitsptr = bits + q * bitsrowbytes; i < j; i += 2) {
#endif
					tmp |= colors[*(gifline + i)] << 4;
					tmp |= colors[*(gifline + i + 1)];
					*bitsptr++ = tmp;
					tmp = 0;
				}
			}
		}
		for (q = 2; q < gifimagelength; q += 4) {
			if (uncompGif(&cg)) {
				free(gifline);
				return RC_NO_MEM;
			}
			if (q < vsize) {              /* into *gifline */
#if OS_WIN32GUI
				for (i = 0, bitsptr = bits_end - (q + 1) * bitsrowbytes; i < j; i += 2) {
#else
				for (i = 0, bitsptr = bits + q * bitsrowbytes; i < j; i += 2) {
#endif
					tmp |= colors[*(gifline + i)] << 4;
					tmp |= colors[*(gifline + i + 1)];
					*bitsptr++ = tmp;
					tmp = 0;
				}
			}
		}
		for (q = 1; q < gifimagelength; q +=2) {
			if (uncompGif(&cg)) {
				free(gifline);
				return RC_NO_MEM;
			}
			if (q < vsize) {              /* into *gifline */
#if OS_WIN32GUI
				for (i = 0, bitsptr = bits_end - (q + 1) * bitsrowbytes; i < j; i += 2) {
#else
				for (i = 0, bitsptr = bits + q * bitsrowbytes; i < j; i += 2) {
#endif
					tmp |= colors[*(gifline + i)] << 4;
					tmp |= colors[*(gifline + i + 1)];
					*bitsptr++ = tmp;
					tmp = 0;
				}
			}
		}
	}
	else {
		for (q = 0; q < gifimagelength; q++) {
			if (uncompGif(&cg)) {
				free(gifline);
				return RC_NO_MEM;
			}
			if (q < vsize) {              /* into *gifline */
#if OS_WIN32GUI
				for (i = 0, bitsptr = bits_end - (q + 1) * bitsrowbytes; i < j; i += 2) {
#else
				for (i = 0, bitsptr = bits + q * bitsrowbytes; i < j; i += 2) {
#endif
					tmp |= colors[*(gifline + i)] << 4;
					tmp |= colors[*(gifline + i + 1)];
					*bitsptr++ = tmp;
					tmp = 0;
				}
			}
		}
	}
	free(block);
	free(gifline);
	return(0);
}

/*
 * FROMGIF3
 *
 * convert gif to 8 bpp Pix
 */
static INT fromgif3(UCHAR *cg)
{
	INT j, q, i, i1;
	UCHAR lace;    /* flag true if image laced */

	if (hsize < gifimagewidth) j = hsize;
	else j = gifimagewidth;
	if ((gifline = (UCHAR *) malloc(gifimagewidth)) == NULL) return RC_NO_MEM;

	if (flags & 0x40) lace = 1;  /* check lacing */
	else lace = 0;

	if ( (i1 = uncompinit(&cg)) ) {           /* initialize values for decompression */
		free(gifline);
		return i1;
	}

	if (lace) {
		for (q = 0; q < gifimagelength; q += 8) {
			if (uncompGif(&cg)) {
				free(gifline);
				return RC_NO_MEM;
			}
			if (q < vsize) {  /* into *gifline */
#if OS_WIN32GUI
				for (i = 0, bitsptr = bits_end - (q + 1) * bitsrowbytes; i < j; i++) {
#else
				for (i = 0, bitsptr = bits + q * bitsrowbytes; i < j; i++) {
#endif
					*bitsptr++ = colors[*(gifline + i)];
				}
			}
		}
		for (q = 4; q < gifimagelength; q += 8) {
			if (uncompGif(&cg)) {
				free(gifline);
				return RC_NO_MEM;
			}
			if (q < vsize) {              /* into *gifline */
#if OS_WIN32GUI
				for (i = 0, bitsptr = bits_end - (q + 1) * bitsrowbytes; i < j; i++) {
#else
				for (i = 0, bitsptr = bits + q * bitsrowbytes; i < j; i++) {
#endif
					*bitsptr++ = colors[*(gifline + i)];
				}
			}
		}
		for (q = 2; q < gifimagelength; q += 4) {
			if (uncompGif(&cg)) {
				free(gifline);
				return RC_NO_MEM;
			}
			if (q < vsize) {              /* into *gifline */
#if OS_WIN32GUI
				for (i = 0, bitsptr = bits_end - (q + 1) * bitsrowbytes; i < j; i++) {
#else
				for (i = 0, bitsptr = bits + q * bitsrowbytes; i < j; i++) {
#endif
					*bitsptr++ = colors[*(gifline + i)];
				}
			}
		}
		for (q = 1; q < gifimagelength; q += 2) {
			if (uncompGif(&cg)) {
				free(gifline);
				return RC_NO_MEM;
			}
			if (q < vsize) {              /* into *gifline */
#if OS_WIN32GUI
				for (i = 0, bitsptr = bits_end - (q + 1) * bitsrowbytes; i < j; i++) {
#else
				for (i = 0, bitsptr = bits + q * bitsrowbytes; i < j; i++) {
#endif
					*bitsptr++ = colors[*(gifline + i)];
				}
			}
		}
	}
	else {
		for (q = 0; q < gifimagelength; q++) {
			if (uncompGif(&cg)) {
				free(gifline);
				return RC_NO_MEM;
			}
			if (q < vsize) {              /* into *gifline */
#if OS_WIN32GUI
				for (i = 0, bitsptr = bits_end - (q + 1) * bitsrowbytes; i < j; i++) {
#else
				for (i = 0, bitsptr = bits + q * bitsrowbytes; i < j; i++) {
#endif
					*bitsptr++ = colors[*(gifline + i)];
				}
			}
		}
	}
	free(block);
	free(gifline);
	return(0);
}

/* FROMGIF4 */
static INT fromgif4(UCHAR *cg)     /* convert gif to 24 bpp Pix */
{
	INT j, q, s, clr, i1;
	UCHAR lace;    /* flag true if image laced */

	if (hsize < gifimagewidth) j = hsize;
	else j = gifimagewidth;
	if ((gifline = (UCHAR *) malloc(gifimagewidth)) == NULL)
		return RC_NO_MEM;

	if (flags & 0x40) lace = 1;  /* check lacing */
	else lace = 0;

	if ( (i1 = uncompinit(&cg)) ) {           /* initialize values for decompression */
		free(gifline);
		return i1;
	}

	if (lace) {
		for (q = 0; q < gifimagelength; q += 8) {
			if (uncompGif(&cg)) {
				free(gifline);
				return RC_NO_MEM;
			}
			if (q < vsize) {                   /* into *gifline */
#if OS_WIN32GUI
				bitsptr = bits_end - (q + 1) * bitsrowbytes;
#else
				bitsptr = bits + q * bitsrowbytes;
#endif
				for (s = 0; s < j; s++) {
					clr = (*(gifline + s)) * 3;
#if OS_WIN32GUI
					*bitsptr++ = colors[clr + 2];
					*bitsptr++ = colors[clr + 1];
					*bitsptr++ = colors[clr];
#else
					*bitsptr++ = colors[clr];
					*bitsptr++ = colors[clr + 1];
					*bitsptr++ = colors[clr + 2];
#endif
				}
			}
		}
		for (q = 4; q < gifimagelength; q += 8) {
			if (uncompGif(&cg)) {
				free(gifline);
				return RC_NO_MEM;
			}
			if (q < vsize) {                   /* into *gifline */
#if OS_WIN32GUI
				bitsptr = bits_end - (q + 1) * bitsrowbytes;
#else
				bitsptr = bits + q * bitsrowbytes;
#endif
				for (s = 0; s < j; s++) {
					clr = (*(gifline + s)) * 3;
#if OS_WIN32GUI
					*bitsptr++ = colors[clr + 2];
					*bitsptr++ = colors[clr + 1];
					*bitsptr++ = colors[clr];
#else
					*bitsptr++ = colors[clr];
					*bitsptr++ = colors[clr + 1];
					*bitsptr++ = colors[clr + 2];
#endif
				}
			}
		}
		for (q = 2; q < gifimagelength; q += 4) {
			if (uncompGif(&cg)) {
				free(gifline);
				return RC_NO_MEM;
			}
			if (q < vsize) {                   /* into *gifline */
#if OS_WIN32GUI
				bitsptr = bits_end - (q + 1) * bitsrowbytes;
#else
				bitsptr = bits + q * bitsrowbytes;
#endif
				for (s = 0; s < j; s++) {
					clr = (*(gifline + s)) * 3;
#if OS_WIN32GUI
					*bitsptr++ = colors[clr + 2];
					*bitsptr++ = colors[clr + 1];
					*bitsptr++ = colors[clr];
#else
					*bitsptr++ = colors[clr];
					*bitsptr++ = colors[clr + 1];
					*bitsptr++ = colors[clr + 2];
#endif
				}
			}
		}
		for (q = 1; q < gifimagelength; q += 2) {
			if (uncompGif(&cg)) {
				free(gifline);
				return RC_NO_MEM;
			}
			if (q < vsize) {                   /* into *gifline */
#if OS_WIN32GUI
				bitsptr = bits_end - (q + 1) * bitsrowbytes;
#else
				bitsptr = bits + q * bitsrowbytes;
#endif
				for (s = 0; s < j; s++) {
					clr = (*(gifline + s)) * 3;
#if OS_WIN32GUI
					*bitsptr++ = colors[clr + 2];
					*bitsptr++ = colors[clr + 1];
					*bitsptr++ = colors[clr];
#else
					*bitsptr++ = colors[clr];
					*bitsptr++ = colors[clr + 1];
					*bitsptr++ = colors[clr + 2];
#endif
				}
			}
		}
	}
	else {
		for (q = 0; q < gifimagelength; q++) {
			if (uncompGif(&cg)) {
				free(gifline);
				return RC_NO_MEM;
			}
			if (q < vsize) {                   /* into *gifline */
#if OS_WIN32GUI
				bitsptr = bits_end - (q + 1) * bitsrowbytes;
#else
				bitsptr = bits + q * bitsrowbytes;
#endif
				for (s = 0; s < j; s++) {
					clr = (*(gifline + s)) * 3;
#if OS_WIN32GUI
					*bitsptr++ = colors[clr + 2];
					*bitsptr++ = colors[clr + 1];
					*bitsptr++ = colors[clr];
#else
					*bitsptr++ = colors[clr];
					*bitsptr++ = colors[clr + 1];
					*bitsptr++ = colors[clr + 2];
#endif
				}
			}
		}
	}
	free(block);
	free(gifline);
	return(0);
}


/* UNCOMP */
static INT uncompGif(UCHAR **cg)
{

	for ( ; ; ) {
		if(queempty) {
			if (bitsleft == 8) {
				if (++datacurr >= dataend) {
					blocksize = *(*cg)++;
					memcpy(block, *cg, blocksize);
					*cg += blocksize;
					dataend = (datacurr = block) + blocksize;
				}
				bitsleft = 0;
			}
			thiscode = *datacurr;
			if ((currentcode = (codesize + bitsleft)) <= 8) {
				*datacurr >>= codesize;
				bitsleft = currentcode;
			}
			else {
				if (++datacurr >= dataend) {
					blocksize = *(*cg)++;
					memcpy(block, *cg, blocksize);
					*cg += blocksize;
					dataend = (datacurr = block) + blocksize;
				}

				thiscode |= *datacurr << (8 - bitsleft);
				if (currentcode <= 16) *datacurr >>= (bitsleft = currentcode - 8);
				else {
					if(++datacurr >= dataend) {
						blocksize = *(*cg)++;
						memcpy(block, *cg, blocksize);
						*cg += blocksize;
						dataend = (datacurr = block) + blocksize;
					}
					thiscode |= *datacurr << (16 - bitsleft);
					*datacurr >>= (bitsleft = currentcode - 16);
				}
			}
			thiscode &= wordmasktable[codesize];
			currentcode = thiscode;

			if (thiscode > nextcode) return RC_ERROR;
			if (thiscode == (bits2 + 1)) break;
			if (thiscode == bits2) {  /* Clear Code */
				nextcode = bits2 + 2;
				codesize = codebits + 1;
				codesize2 = 1 << codesize;
				oldtoken = oldcode = -1;
				continue;
			}

			u = firstcodestack;      /* initialize stack pointer */
			if (thiscode == nextcode) {
				*u++ = oldtoken;
				thiscode = oldcode;
			}
			while (thiscode >= bits2) {
				*u++ = lastcodestack[thiscode];
				thiscode = codestack[thiscode];
			}

			oldtoken = thiscode;
			for ( ; ; ) {
				strque[queend++] = thiscode;
				queempty = 0;
				if (u <= firstcodestack) break;
				thiscode = *--u;
			}

			if (nextcode < 4096 && oldcode != -1) {
				codestack[nextcode] = oldcode;
				lastcodestack[nextcode] = oldtoken;
				if (++nextcode >= codesize2 && codesize < 12) codesize2 = 1 << ++codesize;
			}
			oldcode = currentcode;
		}
		while (posbyte < gifimagewidth) {
			if (quecurr < queend && !queempty) gifline[posbyte++] = strque[quecurr++];
			else {
				quecurr = queend = 0;
				queempty = 1;
				break;
			}
		}
		if (posbyte == gifimagewidth) {
			posbyte = 0;
			break;
		}
	}
	return(0);
}

/*
 * UNCOMPINIT
 *
 * returns 0 if Ok, RC_NO_MEM if not
 */
static INT uncompinit(UCHAR **cg)  /* initialize values for image decompression */
{
	codebits = *(*cg)++;  /* initial codesize from buffer */

	posbyte = 0;                /* initial index of line buffer */
	queempty = 1;            /* output queue starts empty */

	quecurr = queend = 0;    /* current and end for output queue */

	if ((block = (UCHAR *) malloc(255)) == NULL) return RC_NO_MEM;

	datacurr = dataend = block;  /* initialize current and end pointers */
						    /* for block buffer */
	bitsleft = 8;                /* unread bits in first character in block */
	bits2 = 1 << codebits;       /* define first available table location */
	nextcode = bits2 + 2;    /* first available table location for codes */
	codesize = codebits + 1; /* initial code size */
	codesize2 = 1 << codesize;    /* next code size */
	oldcode = oldtoken = -1;

	return(0);
}

/* EXTENSION */
static void extensionGif(UCHAR **cg)
{
	int i1;

	*cg += 2;
	do if ((i1 = *(*cg)++) != 0) *cg += i1;
	while (i1 > 0);
	return;
}

static void setTiffHeader(UCHAR* ptr, SHORT tmtype, SHORT ver, INT firstifd)
{
	INT offset = 0;
	memcpy(ptr, &tmtype, sizeof(SHORT));
	offset += sizeof(SHORT);
	memcpy(ptr + offset, &ver, sizeof(SHORT));
	offset += sizeof(SHORT);
	memcpy(ptr + offset, &firstifd, sizeof(INT));
}

static void setTiffHeader1stIFD(UCHAR* ptr, INT firstifd)
{
	INT offset;
	offset = sizeof(SHORT) + sizeof(SHORT);
	memcpy(ptr + offset, &firstifd, sizeof(INT));
}

static void setTiffTag(UCHAR* ptififd, INT index, SHORT type, SHORT size, INT count, INT value)
{
	INT offset = sizeof(SHORT);		/* accomodate IFD.NumDirEntries */

	ptififd += index * TIFF_TAG_SIZE;
	memcpy(ptififd + offset, &type, sizeof(SHORT));
	offset += sizeof(SHORT);
	memcpy(ptififd + offset, &size, sizeof(SHORT));
	offset += sizeof(SHORT);
	memcpy(ptififd + offset, &count, sizeof(INT));
	offset += sizeof(INT);
	memcpy(ptififd + offset, &value, sizeof(INT));
}

static void setTiffIFDNextOffset(UCHAR* ptififd, INT numTags, INT value)
{
	memcpy (ptififd + sizeof(SHORT) + numTags * TIFF_TAG_SIZE, &value, sizeof(INT));
}

static UCHAR* getTiffTagValueAddr(UCHAR* ptififd, INT index)
{
	return ptififd + sizeof(SHORT) + index * TIFF_TAG_SIZE + 2 * sizeof(SHORT) + sizeof(INT);
}
