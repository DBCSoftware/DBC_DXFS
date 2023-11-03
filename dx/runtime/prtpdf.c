
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
#define PRTPDF_C_

#define INC_STDIO
#define INC_STDLIB
#define INC_STRING
#define INC_MATH
#include "includes.h"
#include "release.h"
#include "base.h"
#include "dbcmd5.h"
#include "gui.h"
#include "guix.h"
#include "prt.h"
#include "prtx.h"
#include "fio.h"
#include "afmarrays.h"
#include "glyphWidths.h"
#include "ttcFontFiles.h"
#include "../zlib/zlib.h"

INT prtpdfinit(INT prtnum)
{
	INT i1;
	PRINTINFO *print;
	PDFOFFSET **pdfoff;
	INT **pointer;

	prtSetPdfSize(prtnum);

	pdfoff = (PDFOFFSET **) memalloc(sizeof(PDFOFFSET) * 16, 0);
	if (pdfoff == NULL) return PRTERR_NOMEM;

	pointer = (INT **) memalloc(sizeof(INT) * 16, 0);
	if (pointer == NULL) {
		memfree((UCHAR**)pdfoff);
		return PRTERR_NOMEM;
	}
	print = *printtabptr + prtnum;
	print->fmt.pdf.posx = 0;
	print->fmt.pdf.posy = 0;
	print->fmt.pdf.color = 0;
	print->flags |= PRTFLAG_INIT;
	print->pagecount = 1;

	print->fmt.pdf.offsetsize = 16;
	print->fmt.pdf.offsets = pdfoff;

	print->fmt.pdf.pageobjnums = pointer;
	print->fmt.pdf.pageobjarraylen = 16;
	print->fmt.pdf.pageobjarraysize = 0;

	print->fmt.pdf.imagecount = 0;
	print->fmt.pdf.imgobjcount = 0;
	strcpy((*print->fmt.pdf.offsets)[0].offset, "0000000000 65535 f \n");
	print->fmt.pdf.offsetcount = 1;
	i1 = prtbufcopy(prtnum, "%PDF-1.4\n%\241\242\243\244\n", -1);
	if (!i1) i1 = prtpdfoffset(prtnum); /* INFO start */
	print = *printtabptr + prtnum;
	if (!i1) i1 = prtbufcopy(prtnum, "1 0 obj\n<<\n/Creator (", -1);
	if (!i1) i1 = prtbufcopy(prtnum, program, -1);
	if (!i1) i1 = prtbufcopy(prtnum, ")\n/Producer (", -1);
	if (!i1) i1 = prtbufcopy(prtnum, version, -1);
	if (!i1) i1 = prtbufcopy(prtnum, ")\n>>\nendobj\n", -1);
	print->fmt.pdf.objectcount = 1;
	if (!i1) i1 = prtpdfpagestart(prtnum);
	if (!i1) i1 = prtfont(prtnum + 1, "COURIER(12,PLAIN)");
	return i1;
}

INT prtpdfcolor(INT prtnum, INT color)
{
	CHAR w1[32], work[72];
	PRINTINFO *print;
	INT i1, i2, i3;

	print = *printtabptr + prtnum;
	if (color == 0x000000) strcpy(work, "0 G 0 g\n");
	else if (color == 0xFFFFFF) strcpy(work, "1 G 1 g\n");
	else {
		if (color == 0xFF0000) strcpy(w1, "1 0 0");
		else if (color == 0x00FF00) strcpy(w1, "0 1 0");
		else if (color == 0x0000FF) strcpy(w1, "0 0 1");
		else {
			i1 = color >> 16;
			i2 = (color & 0x00FF00) >> 8;
			i3 = color & 0x0000FF;
			sprintf(w1, "%.3f %.3f %.3f", (DOUBLE) i1 / 255.00, (DOUBLE) i2 / 255.00,
				(DOUBLE) i3 / 255.00);
		}
		sprintf(work, "%s RG %s rg\n", w1, w1);
	}
	i1 = prtbufcopy(prtnum, work, -1);
	if (i1) return i1;
	print->fmt.pdf.color = color;
	return 0;
}

INT prtpdfpagestart(INT prtnum)
{
	PRINTINFO *print;

	INT i1;
	CHAR work[64];
	i1 = prtpdfoffset(prtnum);  /* PAGE CONTENTS */
	print = *printtabptr + prtnum;
	if (!i1) i1 = prtbufcopy(prtnum, work, mscitoa(print->fmt.pdf.objectcount + 1, work));
	if (!i1) i1 = prtbufcopy(prtnum, " 0 obj\n<<\n/Length ", -1);
	if (i1) return i1;
	i1 = prtbufcopy(prtnum, work, mscitoa(print->fmt.pdf.objectcount + 2, work));
	if (!i1) i1 = prtbufcopy(prtnum, " 0 R\n>>\nstream\n", -1);
	if (i1) return i1;
	print->fmt.pdf.pagestart = print->filepos + print->bufcnt;
	if (!i1) i1 = prtbufcopy(prtnum, "1 0 0 -1 0 ", -1);
	if (!i1) i1 = prtbufcopy(prtnum, work, mscitoa(print->fmt.pdf.height, work));
	if (!i1) i1 = prtbufcopy(prtnum, " cm\n", -1);
	if (!i1) i1 = prtpdfcolor(prtnum, print->fmt.pdf.color);
	if (!i1) i1 = prtpos(prtnum + 1, 0, 0);
	if (i1) return i1;
	print->fmt.pdf.imagecount = 0;

	if (print->fmt.pdf.pageobjarraysize == print->fmt.pdf.pageobjarraylen) {
		print->fmt.pdf.pageobjarraylen += 16;
		i1 = memchange((UCHAR **) print->fmt.pdf.pageobjnums, sizeof(INT) * print->fmt.pdf.pageobjarraylen, 0);
		if (i1) return PRTERR_NOMEM;
		print = *printtabptr + prtnum;
	}
	(*print->fmt.pdf.pageobjnums)[print->fmt.pdf.pageobjarraysize++] = print->fmt.pdf.objectcount + 1;
	return 0;
}

INT prtpdfpageend(INT prtnum)
{
	PRINTINFO *print;
	INT i1, i2, pagesize, srcsize;
	UCHAR bpp;
	CHAR work[128];

	/* finish up previous page */
	print = *printtabptr + prtnum;
	pagesize = (INT)(print->filepos + print->bufcnt - print->fmt.pdf.pagestart);

	i1 = prtbufcopy(prtnum, "endstream\nendobj\n", -1);
	if (i1) return i1;
	i1 = prtpdfoffset(prtnum);  /* PAGE LENGTH */
	if (i1) return i1;
	print = *printtabptr + prtnum;
	i1 = prtbufcopy(prtnum, work, mscitoa(print->fmt.pdf.objectcount + 2, work));
	if (!i1) i1 = prtbufcopy(prtnum, " 0 obj\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, work, mscitoa(pagesize, work));
	if (!i1) i1 = prtbufcopy(prtnum, "\nendobj\n", -1);
	if (i1) return i1;
	print->fmt.pdf.objectcount += 2;  /* page obj + length obj */

	/* write out any images stored in memory, then free up memory */
	if (print->fmt.pdf.imagecount > 0) {
		for (i2 = 0; i2 < print->fmt.pdf.imagecount; i2++) {
			i1 = prtpdfoffset(prtnum); /* IMAGE XOBJECT */
			if (i1) return i1;
			print = *printtabptr + prtnum;
			bpp = (*print->fmt.pdf.images)[i2].bpp;
			srcsize = (*print->fmt.pdf.images)[i2].bcount;
			i1 = prtbufcopy(prtnum, work, mscitoa(print->fmt.pdf.objectcount + 1, work));
			if (!i1) i1 = prtbufcopy(prtnum, " 0 obj\n<<", -1);
			if (!i1) i1 = prtbufcopy(prtnum, "\n/Subtype /Image", -1);
			if (!i1) i1 = prtbufcopy(prtnum, "\n/Width ", -1);
			if (!i1) i1 = prtbufcopy(prtnum, work, mscitoa((*print->fmt.pdf.images)[i2].hsize, work));
			if (!i1) i1 = prtbufcopy(prtnum, "\n/Height ", -1);
			if (!i1) i1 = prtbufcopy(prtnum, work, mscitoa((*print->fmt.pdf.images)[i2].vsize, work));
			if (!i1) {
				if (bpp == 1)
					i1 = prtbufcopy(prtnum, "\n/BitsPerComponent 1\n/ColorSpace /DeviceGray", -1);
				else
					i1 = prtbufcopy(prtnum, "\n/BitsPerComponent 8\n/ColorSpace /DeviceRGB", -1);
			}
			if (!i1) i1 = prtbufcopy(prtnum, "\n/Filter [ /RunLengthDecode ]", -1);
			if (!i1) i1 = prtbufcopy(prtnum, "\n/Length ", -1);
			if (!i1) i1 = prtbufcopy(prtnum, work, mscitoa(srcsize, work));
			if (!i1) i1 = prtbufcopy(prtnum, "\n>>", -1);
			if (!i1) i1 = prtbufcopy(prtnum, "\nstream\n", -1);
			if (!i1) i1 = prtbufcopy(prtnum, (CHAR *) *(*print->fmt.pdf.images)[i2].image, srcsize);
			if (!i1) i1 = prtbufcopy(prtnum, "\nendstream\nendobj\n", -1);
			if (i1) return i1;
			memcpy((*print->fmt.pdf.imgobjs)[print->fmt.pdf.imgobjcount].digest,
					(*print->fmt.pdf.images)[i2].digest, 16);
			(*print->fmt.pdf.imgobjs)[print->fmt.pdf.imgobjcount++].objectnumber = print->fmt.pdf.objectcount + 1;
			print->fmt.pdf.objectcount += 1;
		}
		while (print->fmt.pdf.imagecount--) {
			memfree((*print->fmt.pdf.images)[print->fmt.pdf.imagecount].image);
		}
	}
	return 0;
}

/*
 * Uses memchange, could move memory
 */
INT prtpdfoffset(INT prtnum)
{
	INT i1, cnt;
	CHAR *ptr;
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	cnt = print->fmt.pdf.offsetcount;
	if (cnt == print->fmt.pdf.offsetsize) {
		i1 = print->fmt.pdf.offsetsize << 1;
		if (memchange((UCHAR **) print->fmt.pdf.offsets, sizeof(PDFOFFSET) * i1, 0) < 0) return PRTERR_NOMEM;
		print = *printtabptr + prtnum;
		print->fmt.pdf.offsetsize = i1;
	}

	ptr = (*print->fmt.pdf.offsets)[cnt].offset;
	mscoffton(print->filepos + print->bufcnt, (UCHAR *) ptr, 10);
	for (i1 = 0; ptr[i1] == ' '; ) ptr[i1++] = '0';
	strcpy(ptr + 10, " 00000 n \n");  /* must have space before end of line marker */
	print->fmt.pdf.offsetcount++;
	return 0;
}

/**
 * Many thanks to a web article by G. Adam Stanislav on how to do this.
 * PDF does not have a primitive to draw a circle.
 * We have to approximate it with four bezier curves.
 */
INT prtpdfcircle(INT prtnum, INT r, INT fill) {
	PRINTINFO *print = *printtabptr + prtnum;
	DOUBLE d1 = ((DOUBLE) (r * print->fmt.pdf.width) / (DOUBLE) print->fmt.pdf.gwidth); /* scale radius */
	DOUBLE kappa = 0.5522847498;
	DOUBLE kd1 = kappa * d1;
	CHAR work[128];
	INT i1;
	struct {
		DOUBLE x;
		DOUBLE y;
	} P0, P1, P2, P3;

	/*
	 * The radius comes to us in DB/C (dpi) units.
	 * It must be scaled to PDF (1/72 in) units
	 */
	P0.x = print->fmt.pdf.posx - d1;
	P0.y = print->fmt.pdf.posy;
	P1.x = print->fmt.pdf.posx - d1;
	P1.y = print->fmt.pdf.posy - kd1;
	P2.x = print->fmt.pdf.posx - kd1;
	P2.y = print->fmt.pdf.posy - d1;
	P3.x = print->fmt.pdf.posx;
	P3.y = print->fmt.pdf.posy - d1;
	sprintf(work, "%.2f %.2f m\n", P0.x, P0.y);
	i1 = prtbufcopy(prtnum, work, -1);
	if (i1) return i1;
	sprintf(work, "%.2f %.2f %.2f %.2f %.2f %.2f c\n", P1.x, P1.y, P2.x, P2.y, P3.x, P3.y);
	i1 = prtbufcopy(prtnum, work, -1);
	if (i1) return i1;
	P1.x = print->fmt.pdf.posx + kd1;
	P1.y = print->fmt.pdf.posy - d1;
	P2.x = print->fmt.pdf.posx + d1;
	P2.y = print->fmt.pdf.posy - kd1;
	P3.x = print->fmt.pdf.posx + d1;
	P3.y = print->fmt.pdf.posy;
	sprintf(work, "%.2f %.2f %.2f %.2f %.2f %.2f c\n", P1.x, P1.y, P2.x, P2.y, P3.x, P3.y);
	i1 = prtbufcopy(prtnum, work, -1);
	if (i1) return i1;
	P1.x = print->fmt.pdf.posx + d1;
	P1.y = print->fmt.pdf.posy + kd1;
	P2.x = print->fmt.pdf.posx + kd1;
	P2.y = print->fmt.pdf.posy + d1;
	P3.x = print->fmt.pdf.posx;
	P3.y = print->fmt.pdf.posy + d1;
	sprintf(work, "%.2f %.2f %.2f %.2f %.2f %.2f c\n", P1.x, P1.y, P2.x, P2.y, P3.x, P3.y);
	i1 = prtbufcopy(prtnum, work, -1);
	if (i1) return i1;
	P1.x = print->fmt.pdf.posx - kd1;
	P1.y = print->fmt.pdf.posy + d1;
	P2.x = print->fmt.pdf.posx - d1;
	P2.y = print->fmt.pdf.posy + kd1;
	P3.x = P0.x;
	P3.y = P0.y;
	sprintf(work, "%.2f %.2f %.2f %.2f %.2f %.2f c", P1.x, P1.y, P2.x, P2.y, P3.x, P3.y);
	i1 = prtbufcopy(prtnum, work, -1);
	if (i1) return i1;
	if (fill) i1 = prtbufcopy(prtnum, " B\n", -1);
	else i1 = prtbufcopy(prtnum, " S\n", -1);
	if (i1) return i1;
	sprintf(work, "%.2f %.2f m\n", print->fmt.pdf.posx, print->fmt.pdf.posy);
	i1 = prtbufcopy(prtnum, work, -1);
	return i1;
}

/**
 * The current position is one apex, the arguments are the other two
 *
 * The x and y coordinates come to us in DB/C (dpi) units.
 * They must be scaled to PDF (1/72 in) units
 */
INT prtpdftriangle(INT prtnum, INT x1, INT y1, INT x2, INT y2) {
	PRINTINFO *print = *printtabptr + prtnum;
	DOUBLE d1 = ((DOUBLE) (x1 * print->fmt.pdf.width) / (DOUBLE) print->fmt.pdf.gwidth); /* scale x */
	DOUBLE d2 = ((DOUBLE) (y1 * print->fmt.pdf.height) / (DOUBLE) print->fmt.pdf.gheight); /* scale y */
	DOUBLE d3 = ((DOUBLE) (x2 * print->fmt.pdf.width) / (DOUBLE) print->fmt.pdf.gwidth); /* scale x */
	DOUBLE d4 = ((DOUBLE) (y2 * print->fmt.pdf.height) / (DOUBLE) print->fmt.pdf.gheight); /* scale y */
	INT i1;
	CHAR work[128];

	sprintf(work, "%.2f %.2f l\n%.2f %.2f l b\n", d1, d2, d3, d4);
	i1 = prtbufcopy(prtnum, work, -1);
	if (i1) return i1;
	sprintf(work, "%.2f %.2f m\n", print->fmt.pdf.posx, print->fmt.pdf.posy);
	i1 = prtbufcopy(prtnum, work, -1);
	return i1;
}

/*
 * The x and y coordinates come to us in DB/C (dpi) units.
 * They must be scaled to PDF (1/72 in) units
 */
INT prtpdfrect(INT prtnum, INT x, INT y, INT fill) {
	PRINTINFO *print = *printtabptr + prtnum;
	DOUBLE d1 = ((DOUBLE) (x * print->fmt.pdf.width) / (DOUBLE) print->fmt.pdf.gwidth); /* scale x */
	DOUBLE d2 = ((DOUBLE) (y * print->fmt.pdf.height) / (DOUBLE) print->fmt.pdf.gheight); /* scale y */
	/* We need to find the coordinates of the lower left corner */
	DOUBLE llx = (d1 < print->fmt.pdf.posx) ? d1 : print->fmt.pdf.posx;
	DOUBLE lly = (d2 < print->fmt.pdf.posy) ? d2 : print->fmt.pdf.posy;
	DOUBLE width, height;
	CHAR work[128];
	INT i1;

	/*
	 * Now the width and height
	 */
	width = fabs(d1 - print->fmt.pdf.posx);
	height = fabs(d2 - print->fmt.pdf.posy);

	if (fill) sprintf(work, "%.2f %.2f %.2f %.2f re B\n", llx, lly, width, height);
	else sprintf(work, "%.2f %.2f %.2f %.2f re S\n", llx, lly, width, height);
	i1 = prtbufcopy(prtnum, work, -1);
	if (i1) return i1;
	sprintf(work, "%.2f %.2f m\n", print->fmt.pdf.posx, print->fmt.pdf.posy);
	i1 = prtbufcopy(prtnum, work, -1);
	return i1;
}

INT prtpdfimage(INT prtnum, void *pixhandle) {

	PIXMAP *pix;
	INT i1, i2, hsize, vsize, bcount, rlelength, image_name, found;
	UCHAR bpp, **prtimg, *ptr, *ptrend, *scanline;
	CHAR work[64];
	PRINTINFO *print;
	PDFIMAGE pdfimage;
	INT32 color;
	md5_state_t md5state;
	md5_byte_t this_digest[16];
#if OS_WIN32GUI
	BITMAP bm;
#else
	UINT scanlinewidth;
#endif

	pix = *((PIXHANDLE)pixhandle);
	hsize = pix->hsize;
	vsize = pix->vsize;
	bpp = pix->bpp;

	print = *printtabptr + prtnum;
	if (print->fmt.pdf.imagecount == print->fmt.pdf.imagesize) {
		if (!print->fmt.pdf.imagesize) {
			print->fmt.pdf.imagesize = 4;
			prtimg = memalloc(sizeof(PDFIMAGE) * print->fmt.pdf.imagesize, 0);
			if (prtimg == NULL) return PRTERR_NOMEM;
			print = *printtabptr + prtnum;
			print->fmt.pdf.images = (PDFIMAGE **) prtimg;
		}
		else {
			print->fmt.pdf.imagesize <<= 1;
			i1 = memchange((UCHAR **) print->fmt.pdf.images, sizeof(PDFIMAGE) * print->fmt.pdf.imagesize, 0);
			if (i1) return PRTERR_NOMEM;
			print = *printtabptr + prtnum;
		}
	}
	i1 = prtSetPdfImageCacheSize(prtnum);
	if (i1) return i1;
	print = *printtabptr + prtnum;
	if (bpp == 1) bcount = vsize * ((hsize + 0x07 & ~0x07) >> 3);
	else bcount = vsize * hsize * 3;

	/* Make sure the rle_buffer is long enough */
	i1 = (INT)(bcount * 1.1);
	if (rle_buffer == NULL) {
		rle_buffer = (UCHAR *) malloc(i1);
		if (rle_buffer == NULL) {
			prtputerror("malloc failure in prtpdfimage");
			return PRTERR_NOMEM;
		}
		rle_buffer_size = i1;
	}
	else if (rle_buffer_size < (UINT) i1) {
		rle_buffer = (UCHAR *) realloc(rle_buffer, i1);
		if (rle_buffer == NULL) {
			prtputerror("realloc failure in prtpdfimage");
			return PRTERR_NOMEM;
		}
		rle_buffer_size = i1;
	}

	prtimg = memalloc(bcount, 0);
	if (prtimg == NULL) return PRTERR_NOMEM;
	print = *printtabptr + prtnum;
	ptr = *prtimg;
	pix = *((PIXHANDLE)pixhandle);
	(*print->fmt.pdf.images)[print->fmt.pdf.imagecount].hsize = hsize;
	(*print->fmt.pdf.images)[print->fmt.pdf.imagecount].vsize = vsize;
	(*print->fmt.pdf.images)[print->fmt.pdf.imagecount].pixhandle = (PIXHANDLE) pixhandle;
	(*print->fmt.pdf.images)[print->fmt.pdf.imagecount].bpp = bpp;
#if OS_WIN32GUI
	if (GetObject(pix->hbitmap, sizeof(BITMAP), &bm) == 0) return GetLastError();
	bm.bmWidthBytes = ((pix->hsize * bm.bmBitsPixel + 0x1F) & ~0x1F) >> 3; /* needed on Win2000?? */
	ptrend = (UCHAR *) bm.bmBits + bm.bmWidthBytes * (bm.bmHeight - 1);
#else
	scanlinewidth = (hsize * bpp + 0x1F & ~0x1F) >> 3;
	ptrend = (UCHAR *) *pix->pixbuf + scanlinewidth * (vsize - 1);
#endif
	if (bpp == 1) {
		/* We need to move data one byte at a time because both Windows bitmaps and
		 * our own pixmaps round the number of bytes in a scanline up to a multiple of 4.
		 * But PDF image data is expected to be rounded up only to the nearest byte.
		 * No extra padding bytes are allowed, they would screw up the image */
		i2 = (hsize + 0x07 & ~0x07) >> 3;
#if OS_WIN32GUI
		for (scanline = ptrend; scanline >= (UCHAR *) bm.bmBits; scanline -= bm.bmWidthBytes) {
			memcpy(ptr, scanline, i2);
			ptr += i2;
		}
#else
		/* In our pixmap, a 0 is black. For pdf, a 0 is black. */
		for (scanline = *pix->pixbuf; scanline <= ptrend; scanline += scanlinewidth) {
			memcpy(ptr, scanline, i2);
			ptr += i2;
		}
#endif
	}
	else {
		for (i1 = 0; i1 < vsize; i1++) {
			for (i2 = 0; i2 < hsize; i2++) {
				/* color = 0x00BBGGRR */
#if OS_WIN32GUI
				color = prtgetcolor(bm, ptrend, i2, i1);
#else
				guipixgetcolor((PIXHANDLE) pixhandle, &color, i2, i1);
#endif
				*ptr++ = (UCHAR) color;				/* Red */
				*ptr++ = (UCHAR)(color >> 8);		/* Green */
				*ptr++ = (UCHAR)(color >> 16);		/* Blue */
			}
		}
	}

	/* Now that we have the uncompressed image data in a format acceptable to PDF */
	/* RunLengthEncode it */
	rlelength = compress_rle(*prtimg, rle_buffer, bcount);

	/* Rearrange memory */
	if (memchange(prtimg, rlelength, 0)) return PRTERR_NOMEM;
	print = *printtabptr + prtnum;
	memcpy(*prtimg, rle_buffer, rlelength);
	(*print->fmt.pdf.images)[print->fmt.pdf.imagecount].image = prtimg;
	(*print->fmt.pdf.images)[print->fmt.pdf.imagecount].bcount = rlelength;

	/* Calculate the md5 for the encoded bit stream and save it */
	dbc_md5_init(&md5state);
	dbc_md5_append(&md5state, *prtimg, rlelength);
	dbc_md5_finish(&md5state, this_digest);
	memcpy((*print->fmt.pdf.images)[print->fmt.pdf.imagecount].digest, this_digest, 16);

	/* If there are other images on this page, see if any match */
	found = 0;
	if (print->fmt.pdf.imagecount) {
		for (i1 = 0; i1 < print->fmt.pdf.imagecount; i1++) {
			memcpy(&pdfimage, &(*print->fmt.pdf.images)[i1], sizeof(PDFIMAGE));
			if (pdfimage.hsize != hsize || pdfimage.vsize != vsize || pdfimage.bpp != bpp) continue;
			if (memcmp(this_digest, pdfimage.digest, 16) == 0) {
				image_name = print->fmt.pdf.imgobjcount + i1;
				found = i1 + 1;
				break;
			}
		}
	}

	/* If not found so far, and there are other images already on disk, look for a match there too */
	if (!found && print->fmt.pdf.imgobjcount) {
		for (i1 = 0; i1 < print->fmt.pdf.imgobjcount; i1++) {
			if (memcmp(this_digest, (*print->fmt.pdf.imgobjs)[i1].digest, 16) == 0) {
				image_name = i1;
				found = i1 + 1;
				break;
			}
		}
	}

	if (found) {
		memfree(prtimg);
	}
	else {
		image_name = print->fmt.pdf.imgobjcount + print->fmt.pdf.imagecount;
		print->fmt.pdf.imagecount++;
	}

	hsize *= 72;
	if (((hsize % print->dpi) << 1) >= print->dpi) hsize += print->dpi;
	hsize /= print->dpi;
	vsize *= 72;
	if (((vsize % print->dpi) << 1) >= print->dpi) vsize += print->dpi;
	vsize /= print->dpi;
	memcpy(work, "S\nq\n", 4);
	i1 = mscitoa(hsize, work + 4) + 4;
	memcpy(work + i1, " 0 0 ", 5);
	i1 += 5;
	i1 += mscitoa(-vsize, work + i1);
	work[i1++] = ' ';
	i1 += prtdbltoa(print->fmt.pdf.posx, work + i1, 2);
	work[i1++] = ' ';
	i1 += prtdbltoa(print->fmt.pdf.posy + (DOUBLE) vsize, work + i1, 2);
	memcpy(work + i1, " cm\n/Im", 7);
	i1 += 7;
	i1 += mscitoa(image_name, work + i1);
	memcpy(work + i1, " Do\nQ\n", 6);
	i1 = prtbufcopy(prtnum, work, i1 + 6);
	return i1;
}

INT prtpdfline(INT prtnum, INT xpos, INT ypos) {
	PRINTINFO *print;
	DOUBLE d1, d2;
	INT i1;
	CHAR work[64];

	print = *printtabptr + prtnum;
	d1 = ((DOUBLE) (xpos * print->fmt.pdf.width) / (DOUBLE) print->fmt.pdf.gwidth); /* scale x */
	d2 = ((DOUBLE) (ypos * print->fmt.pdf.height) / (DOUBLE) print->fmt.pdf.gheight); /* scale y */
	i1 = prtdbltoa(d1, work, 2);
	work[i1++] = ' ';
	i1 += prtdbltoa(d2, work + i1, 2);
	memcpy(work + i1, " l\nS\n", 5);
	i1 += 5;
	i1 += prtdbltoa(d1, (CHAR *)(work + i1), 2);
	work[i1++] = ' ';
	i1 += prtdbltoa(d2, work + i1, 2);
	memcpy(work + i1, " m\n", 3);
	i1 = prtbufcopy(prtnum, work, i1 + 3);
	if (i1) return i1;
	print->fmt.pdf.posx = d1;
	print->fmt.pdf.posy = d2;
	return 0;
}

/**
 * prtnum The ZERO-Based index into printtabptr
 */
INT prtpdfpos(INT prtnum, INT xpos, INT ypos) {

	INT i1;
	DOUBLE d1, d2;
	CHAR work[64];
	PRINTINFO *print;

	*prterrorstring = '\0';
	i1 = prtflushline(prtnum, FALSE);
	if (i1) return i1;
	print = *printtabptr + prtnum;
	d1 = ((DOUBLE)(xpos * print->fmt.pdf.width) / (DOUBLE) print->fmt.pdf.gwidth); /* scale x */
	d2 = ((DOUBLE)(ypos * print->fmt.pdf.height) / (DOUBLE) print->fmt.pdf.gheight); /* scale y */
	i1 = prtdbltoa(d1, work, 2);
	work[i1++] = ' ';
	i1 += prtdbltoa(d2, work + i1, 2);
	memcpy(work + i1, " m\n", 3);
	i1 = prtbufcopy(prtnum, work, i1 + 3);
	if (i1) return i1;
	print->fmt.pdf.posx = d1;
	print->fmt.pdf.posy = d2;
	return 0;
}

INT prtpdffont(INT prtnum, CHAR *name, INT bold, INT italic, INT pointsize)
{
	PRINTINFO *print;
	FONTFILEHANDLE fontFileHandle;
	INT i1;
	INT fontselected = -1;
	PDFEFONT *pdfefont;

	print = *printtabptr + prtnum;
	if (!strcmp(name, "SYMBOL")) {
		fontselected = 0x0C;
	}
	else if (!strcmp(name, "ZAPFDINGBATS") || !strcmp(name, "DINGBATS")) {
		fontselected = 0x0D;
	}
	else {
		if (!strcmp(name, "COURIER")) fontselected = 0x00;
		else if (!strcmp(name, "TIMES")) fontselected = 0x04;
		else if (!strcmp(name, "HELVETICA")) fontselected = 0x08;
		else if (!strcmp(name, "SYSTEM")) fontselected = 0x08;
		if (fontselected != -1) {
			if (italic) fontselected += 0x01;
			if (bold) fontselected += 0x02;
		}
	}
	if (fontselected == -1) {
		i1 = locateFontFileByName(name, bold, italic, &fontFileHandle);
		if (i1) return i1;
		print = *printtabptr + prtnum;	/* locateFontFileByName could move memory */
		/* if not found, default to courier */
		if (fontFileHandle.fontIndex == -1) {
			fontselected = 0;
		}
		else {
			/*
			 * Look through our array of efonts in use to see if we already have this one
			 */
			for (i1 = 0; i1 < print->fmt.pdf.efontsinuse; i1++) {
				pdfefont = &(*print->fmt.pdf.efonts)[i1];
				if (pdfefont->ffh.fontIndex == fontFileHandle.fontIndex) break;
			}
			if (i1 < print->fmt.pdf.efontsinuse) { /* We have already seen this one */
				fontselected = PDF_STD_FONT_BASE_COUNT + i1 + 1;
			}
			else {
				print->fmt.pdf.efontsinuse++;
				if (print->fmt.pdf.efontsinuse == 1) {
					print->fmt.pdf.efonts = (PDFEFONT **)memalloc(sizeof(PDFEFONT), 0);
					if (print->fmt.pdf.efonts == NULL) {
						prtputerror("memalloc failure in prtpdffont");
						return PRTERR_NOMEM;
					}
				}
				else {
					 if (memchange((UCHAR**)print->fmt.pdf.efonts,
							 print->fmt.pdf.efontsinuse * sizeof(PDFEFONT), 0) != 0) {
						prtputerror("memalloc failure in prtpdffont");
						return PRTERR_NOMEM;
					 }
				}
				print = *printtabptr + prtnum; /* In case calls to our memory routines moved it */
				fontselected = print->fmt.pdf.efontsinuse + PDF_STD_FONT_BASE_COUNT;
				(*print->fmt.pdf.efonts)[print->fmt.pdf.efontsinuse - 1].ffh = fontFileHandle;
			}
		}
	}
	/*
	 * The fontselected field will be 0 to PDF_STD_FONT_BASE_COUNT for one of the standard fonts.
	 * For embedded fonts it will start at PDF_STD_FONT_BASE_COUNT + 1 and go up for each additional one.
	 */
	sprintf(print->fmt.pdf.fontname, "/F%d %d Tf\n", fontselected + 1, pointsize);
	if (fontselected <= PDF_STD_FONT_BASE_COUNT) print->fmt.pdf.fontused[fontselected] = 1;
	print->fmt.pdf.fontselected = fontselected;
	return 0;
}

INT prtpdfend(INT prtnum)
{
	PRINTINFO *print;
	INT i1, i2, i3, i4;
	INT objectcount, fontcounter, fontstart, pageparent, resources;
	INT efontindex, ttfnameindex;
	UINT flags;
	USHORT gwidth;
	CHAR *fname;
	UCHAR **fontFileItself, **fontFileCompressed;
	OFFSET off;
	CHAR work[256], widths[256], wrkname[64];
	FONTFILEHANDLE *ffh;
	FONTDATA *fd;
	FONTFILEDATA *ffd;
	TTFONTNAME *ttfn;
	ULONG destLength;

	i1 = prtpdfpageend(prtnum);
	print = *printtabptr + prtnum;
	if (i1) goto prtpdfend1;

	objectcount = print->fmt.pdf.objectcount + 1;
	fontcounter = 0;
	for (i3 = fontstart = objectcount; i3 < fontstart + 14; i3++) {
		if (print->fmt.pdf.fontused[i3 - fontstart] == 0) continue;
		i1 = prtpdfoffset(prtnum);
		print = *printtabptr + prtnum;
		if (!i1) i1 = prtbufcopy(prtnum, work, mscitoa(fontstart + fontcounter, work));
		print->fmt.pdf.fontobjn[i3 - fontstart] = fontstart + fontcounter;
		if (!i1) i1 = prtbufcopy(prtnum, " 0 obj\n<<\n/Type /Font\n/Subtype /TrueType", -1);
		if (!i1) i1 = prtbufcopy(prtnum, "\n/BaseFont /", -1);
		if (!i1) i1 = prtbufcopy(prtnum, fontnames[i3 - fontstart], -1);
		if (!i1) i1 = prtbufcopy(prtnum, "\n/Encoding /WinAnsiEncoding\n>>\nendobj\n", -1);
		if (i1) goto prtpdfend1;
		objectcount++;
		fontcounter++;
	}
	/* After this point, fontcounter is not used */

	if (print->fmt.pdf.efontsinuse) {
		/* Calculate the /BaseFont name to use and save it */
		for (i3 = 0; i3 < print->fmt.pdf.efontsinuse; i3++) {
			ffh = &(*print->fmt.pdf.efonts)[i3].ffh;
			efontindex = ffh->fontIndex;
			ttfnameindex = ffh->ttnameIndex;
			fd = &FontFiles.fontData[efontindex];
			fname = NULL;
			if (fd->numberOfNames > 1) {
				/* Try for Postscript name */
				for (i2 = 0; i2 < fd->numberOfNames; i2++) {
					ttfn = &fd->names[i2];
					if (ttfn->nameID == FONTPOSTSCRIPTNAMEID) {
						fname = guiMemToPtr(ttfn->name);
						break;
					}
				}
			}
			if (fname == NULL) 	/* Use this if we have nothing else */
			{
				strcpy(wrkname, guiMemToPtr(fd->names[ttfnameindex].canonicalName));
				if (IS_FONT_BOLD(efontindex) || IS_FONT_ITALIC(efontindex)) {
					if (IS_FONT_BOLD(efontindex) && IS_FONT_ITALIC(efontindex)) strcat(wrkname, ",BoldItalic");
					else if (IS_FONT_ITALIC(efontindex)) strcat(wrkname, ",Italic");
					else if (IS_FONT_BOLD(efontindex)) strcat(wrkname, ",Bold");
				}
				fname = wrkname;
			}
			strcpy(ffh->basefont, fname); /* Save for later use */
		}

		/**
		 * Write out the font file itself
		*/
		for (i3 = 0; i3 < print->fmt.pdf.efontsinuse; i3++) {
			ffh = &(*print->fmt.pdf.efonts)[i3].ffh;
			efontindex = ffh->fontIndex;
			ttfnameindex = ffh->ttnameIndex;
			fd = &FontFiles.fontData[efontindex];

			i1 = prtpdfoffset(prtnum);
			if (i1) goto prtpdfend1;
			print = *printtabptr + prtnum;
			i1 = prtbufcopy(prtnum, work, mscitoa((ffh->tFontProgramObjectNumber = objectcount), work));
			if (i1) goto prtpdfend1;
			ffd = &FontFiles.fontFileData[fd->ffdIndex];

			if (ffd->offsetTable == NULL) {
				sprintf(work, " 0 obj\n<<\n  /Length1 %lu\n", (ULONG)ffd->fileLength);
			}
			else {
				i1 = makeTtfFromTtc(fd, &destLength, &fontFileItself);
				if (i1) {
					if (i1 == RC_NO_MEM) i1 = PRTERR_NOMEM;
					goto prtpdfend1;
				}
				sprintf(work, " 0 obj\n<<\n  /Length1 %lu\n", destLength);
			}
			if (!i1) i1 = prtbufcopy(prtnum, work, -1);
			sprintf(work, "  /Length %d 0 R\n", objectcount + 1);
			if (!i1) i1 = prtbufcopy(prtnum, work, -1);
			if (!i1) i1 = prtbufcopy(prtnum, "  /Filter /FlateDecode\n>>\nstream\n", -1);

			if (ffd->offsetTable == NULL) {
				fontFileItself = memalloc((INT)ffd->fileLength, 0);
				print = *printtabptr + prtnum;
				if (fontFileItself == NULL) {
					i1 = PRTERR_NOMEM;
					goto prtpdfend1;
				}
				i1 = openFontFile(fd);
				print = *printtabptr + prtnum; /* openFontFile can move memory */
				if (i1) {
					i1 = PRTERR_OPEN;
					goto prtpdfend1;
				}
				i1 = fioread(ffd->fileHandle, 0, *fontFileItself, (INT)ffd->fileLength);
				print = *printtabptr + prtnum;
				if (i1 < 0) {
					FontFiles.error.error_code = 1;
					FontFiles.error.fioerror = i1;
					strcpy(FontFiles.error.msg, fioerrstr(i1));
					sprintf(FontFiles.error.function, "%s", __FUNCTION__);
					i1 = PRTERR_NATIVE;
					goto prtpdfend1;
				}
				fioclose(ffd->fileHandle);
				ffd->fileHandle = -1;
				destLength = (ULONG)(ffd->fileLength * 1.005);
				fontFileCompressed = memalloc(destLength, 0);
				print = *printtabptr + prtnum;
				if (fontFileCompressed == NULL) {
					memfree(fontFileItself);
					i1 = PRTERR_NOMEM;
					goto prtpdfend1;
				}
				i2 = compress(*fontFileCompressed, &destLength, *fontFileItself, (uLong)ffd->fileLength);
			}
			else {
				ULONG cDestLength;
				cDestLength = (ULONG)(destLength * 1.005);
				fontFileCompressed = memalloc(cDestLength, 0);
				print = *printtabptr + prtnum;
				if (fontFileCompressed == NULL) {
					memfree(fontFileItself);
					i1 = PRTERR_NOMEM;
					goto prtpdfend1;
				}
				i2 = compress(*fontFileCompressed, &cDestLength, *fontFileItself, (uLong)destLength);
				destLength = cDestLength;
			}

			memfree(fontFileItself);
			if (i2 != Z_OK) {
				memfree(fontFileCompressed);
				i1 = PRTERR_NATIVE;
				goto prtpdfend1;
			}
			i1 = prtbufcopy(prtnum, (CHAR*)*fontFileCompressed, destLength);
			memfree(fontFileCompressed);
			if (i1) goto prtpdfend1;
			i1 = prtbufcopy(prtnum, "\nendstream\nendobj\n\n", -1);
			if (i1) goto prtpdfend1;
			objectcount++;

			i1 = prtpdfoffset(prtnum);
			if (i1) goto prtpdfend1;
			print = *printtabptr + prtnum;
			sprintf(work, "%d 0 obj\n%lu\nendobj\n\n", objectcount, destLength);
			i1 = prtbufcopy(prtnum, work, -1);
			if (i1) goto prtpdfend1;
			objectcount++;
		}


		/* Output the FontDescriptor objects */
		for (i3 = 0; i3 < print->fmt.pdf.efontsinuse; i3++) {
			ffh = &(*print->fmt.pdf.efonts)[i3].ffh;
			efontindex = ffh->fontIndex;
			ttfnameindex = ffh->ttnameIndex;
			fd = &FontFiles.fontData[efontindex];
			i1 = prtpdfoffset(prtnum);
			if (i1) goto prtpdfend1;
			print = *printtabptr + prtnum;
			i1 = prtbufcopy(prtnum, work, mscitoa((ffh->tDescriptorObjectNumber = objectcount), work));
			if (!i1) i1 = prtbufcopy(prtnum, " 0 obj\n<<\n  /Type /FontDescriptor\n", -1);
			sprintf(work, "  /FontName /%s\n", ffh->basefont);
			if (!i1) i1 = prtbufcopy(prtnum, work, -1);

			flags = 0;
			if (fd->post_data.isFixedPitch) flags |= 1;
			if ((fd->head_data.macStyle & 2) != 0) flags |= 0x00040;
			if ((fd->head_data.macStyle & 1) != 0) flags |= 0x40000;
			/*
			 * This is in iTextSharp, ignore for now, may need it eventually
			flags |= fontSpecific ? 4 : 32;
			*/
			flags |= 4;
			sprintf(work, "  /Flags %u\n", flags); /* Look in TrueTypeFont.cs in iTextSharp line 1081 */
			if (!i1) i1 = prtbufcopy(prtnum, work, -1);
			if (!i1) i1 = prtbufcopy(prtnum, "  /StemV 80\n", -1);

			sprintf(work, "  /Ascent %ld\n",
					(long int)((int)fd->os2_data.sTypoAscender * 1000 / fd->head_data.unitsPerEm));
			if (!i1) i1 = prtbufcopy(prtnum, work, -1);

			sprintf(work, "  /Descent %ld\n",
					(long int)((int)fd->os2_data.sTypoDescender * 1000 / fd->head_data.unitsPerEm));
			if (!i1) i1 = prtbufcopy(prtnum, work, -1);

			sprintf(work, "  /CapHeight %ld\n",
					(long int)((int)fd->os2_data.sCapHeight * 1000 / fd->head_data.unitsPerEm));
			if (!i1) i1 = prtbufcopy(prtnum, work, -1);

			sprintf(work, "  /ItalicAngle %f\n", fd->post_data.italicAngle);
			if (!i1) i1 = prtbufcopy(prtnum, work, -1);

			sprintf(work, "  /FontBBox [%ld %ld %ld %ld]\n",
					(long int)((int)fd->head_data.xMin * 1000 / fd->head_data.unitsPerEm),
					(long int)((int)fd->head_data.yMin * 1000 / fd->head_data.unitsPerEm),
					(long int)((int)fd->head_data.xMax * 1000 / fd->head_data.unitsPerEm),
					(long int)((int)fd->head_data.yMax * 1000 / fd->head_data.unitsPerEm)
					);
			if (!i1) i1 = prtbufcopy(prtnum, work, -1);
			sprintf(work, "  /FontFile2 %d 0 R\n", ffh->tFontProgramObjectNumber);
			i1 = prtbufcopy(prtnum, work, -1);
			if (i1) goto prtpdfend1;

			prtbufcopy(prtnum, ">>\nendobj\n", -1);
			objectcount++;
		}

		/* Output glyph widths */
		for (i3 = 0; i3 < print->fmt.pdf.efontsinuse; i3++) {
			ffh = &(*print->fmt.pdf.efonts)[i3].ffh;
			i1 = prtpdfoffset(prtnum);
			if (i1) goto prtpdfend1;
			print = *printtabptr + prtnum;
			sprintf(work, "%d 0 obj\n[\n", (ffh->widthObjectNumber = objectcount));
			i1 = prtbufcopy(prtnum, work, -1);
			if (i1) goto prtpdfend1;
			fd = &FontFiles.fontData[ffh->fontIndex];
			strcpy(widths, "  ");
			for (i4 = FIRSTCHAR; i4 <= LASTCHAR; i4++) {
				if (i4 != FIRSTCHAR && !(i4 % 16)) {
					strcat(widths, "\n");
					prtbufcopy(prtnum, widths, -1);
					strcpy(widths, "  ");
				}
				i1 = getGlyphWidth(fd, (UCHAR)i4, &gwidth);
				if (i1) goto prtpdfend1;
				sprintf(work, "%hu  ", gwidth);
				strcat(widths, work);
			}
			if (strlen(widths) > 2) prtbufcopy(prtnum, widths, -1);
			prtbufcopy(prtnum, "\n]\nendobj\n", -1);
			objectcount++;
		}
		print = *printtabptr + prtnum; /* getGlyphWidth could move memory */

		for (i3 = 0; i3 < print->fmt.pdf.efontsinuse; i3++) {
			ffh = &(*print->fmt.pdf.efonts)[i3].ffh;
			i1 = prtpdfoffset(prtnum);
			if (i1) goto prtpdfend1;
			print = *printtabptr + prtnum;
			i1 = prtbufcopy(prtnum, work, mscitoa((ffh->tFontObjectNumber = objectcount), work));
			if (!i1) i1 = prtbufcopy(prtnum, " 0 obj\n<<\n  /Type /Font\n", -1);
			if (!i1) i1 = prtbufcopy(prtnum, "  /Subtype /TrueType\n", -1);
			sprintf(work, "  /BaseFont /%s\n", ffh->basefont);
			if (!i1) i1 = prtbufcopy(prtnum, work, -1);
			if (!i1) i1 = prtbufcopy(prtnum, "  /Encoding /WinAnsiEncoding\n", -1);
			sprintf(work, "  /FirstChar %u\n", FIRSTCHAR);
			if (!i1) i1 = prtbufcopy(prtnum, work, -1);
			sprintf(work, "  /LastChar %u\n", LASTCHAR);
			if (!i1) i1 = prtbufcopy(prtnum, work, -1);
			sprintf(work, "  /Widths %d 0 R\n", ffh->widthObjectNumber);
			if (!i1) i1 = prtbufcopy(prtnum, work, -1);
			sprintf(work, "  /FontDescriptor %d 0 R\n", ffh->tDescriptorObjectNumber);
			if (!i1) i1 = prtbufcopy(prtnum, work, -1);
			if (!i1) i1 = prtbufcopy(prtnum, ">>\nendobj\n", -1);
			if (i1) goto prtpdfend1;
			objectcount++;
		}
	}
	objectcount--;
	resources = ++objectcount;
	i1 = prtpdfoffset(prtnum);  /* RESOURCES */
	print = *printtabptr + prtnum;
	sprintf(work, "%d 0 obj", objectcount);
	if (!i1) i1 = prtbufcopy(prtnum, work, -1);
	if (!i1) i1 = prtbufcopy(prtnum, "\n<<\n/Font <<", -1);
	if (i1) goto prtpdfend1;
	for (i4 = 0 /*, i5 = 0*/; i4 <= PDF_STD_FONT_BASE_COUNT; i4++) {
		if (print->fmt.pdf.fontused[i4] == 1) {
			memcpy(work, " /F", 3);
			i1 = mscitoa(i4 + 1, work + 3) + 3;
			work[i1++] = ' ';
			i1 += mscitoa(print->fmt.pdf.fontobjn[i4], work + i1);
			memcpy(work + i1, " 0 R", 4);
			i1 = prtbufcopy(prtnum, work, i1 + 4);
			if (i1) goto prtpdfend1;
		}
	}
	for (i4 = 0; i4 < print->fmt.pdf.efontsinuse; i4++) {
		ffh = &(*print->fmt.pdf.efonts)[i4].ffh;
		sprintf(work, " /F%d %d 0 R", PDF_STD_FONT_BASE_COUNT + i4 + 2, ffh->tFontObjectNumber);
		i1 = prtbufcopy(prtnum, work, -1);
		if (i1) goto prtpdfend1;
	}
	i1 = prtbufcopy(prtnum, " >>\n", 4);
	if (i1) goto prtpdfend1;
	if (print->fmt.pdf.imgobjcount > 0) {
		i1 = prtbufcopy(prtnum, "/XObject <<", -1);
		if (i1) goto prtpdfend1;
		for (i2 = 0; i2 < print->fmt.pdf.imgobjcount; i2++) {
			memcpy(work, " /Im", 4);
			i1 = mscitoa(i2, work + 4) + 4;
			work[i1++] = ' ';
			i1 += mscitoa((*print->fmt.pdf.imgobjs)[i2].objectnumber, work + i1);
			memcpy(work + i1, " 0 R", 4);
			i1 = prtbufcopy(prtnum, work, i1 + 4);
			if (i1) goto prtpdfend1;
		}
		i1 = prtbufcopy(prtnum, " >>\n", 4);
		if (i1) goto prtpdfend1;
	}
	i1 = prtbufcopy(prtnum, "/ProcSet [ /PDF /Text ", -1);
	if (!i1 && print->fmt.pdf.imgobjcount > 0) i1 = prtbufcopy(prtnum, "/ImageC /ImageB ", 16);
	if (!i1) i1 = prtbufcopy(prtnum, "]\n>>\nendobj\n", -1);
	if (i1) goto prtpdfend1;

	/* pages */
	pageparent = ++objectcount;
	i1 = prtpdfoffset(prtnum);
	print = *printtabptr + prtnum;
	if (!i1) i1 = prtbufcopy(prtnum, work, mscitoa(objectcount, work));
	if (!i1) i1 = prtbufcopy(prtnum, " 0 obj\n<<\n/Type /Pages", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "\n/Count ", -1);
	if (!i1) i1 = prtbufcopy(prtnum, work, mscitoa(print->pagecount, work));
	if (!i1) i1 = prtbufcopy(prtnum, "\n/Kids [ ", -1);
	if (i1) goto prtpdfend1;
	for (i2 = 0, i3 = objectcount + 1; i2 < print->pagecount; i2++, i3++) {
		i1 = mscitoa(i3, work);
		memcpy(work + i1, " 0 R ", 5);
		i1 = prtbufcopy(prtnum, work, i1 + 5);
		if (i1) goto prtpdfend1;
		if (!(i2 % 15)) {
			if (prtbufcopy(prtnum, "\n", 1)) goto prtpdfend1; /* break apart long lines */
		}
	}
	i1 = prtbufcopy(prtnum, "]\n/MediaBox [ 0 0 ", -1);
	if (i1) goto prtpdfend1;
	i1 = mscitoa(print->fmt.pdf.width, work);
	work[i1++] = ' ';
	i1 += mscitoa(print->fmt.pdf.height, work + i1);
	i1 = prtbufcopy(prtnum, work, i1);
	if (!i1) i1 = prtbufcopy(prtnum, " ]\n>>\nendobj\n", -1);
	if (i1) goto prtpdfend1;

	for (i2 = 0; i2 < print->fmt.pdf.pageobjarraysize; i2++) {
		objectcount++;
		i1 = prtpdfoffset(prtnum);  /* PAGE OBJECT */
		print = *printtabptr + prtnum;
		if (!i1) i1 = prtbufcopy(prtnum, work, mscitoa(objectcount, work));
		if (!i1) i1 = prtbufcopy(prtnum, " 0 obj\n<<\n/Type /Page\n/Parent ", -1);
		if (!i1) i1 = prtbufcopy(prtnum, work, mscitoa(pageparent, work));
		if (!i1) i1 = prtbufcopy(prtnum, " 0 R\n", 5);
		if (!i1) i1 = prtbufcopy(prtnum, "/Resources ", -1);
		if (!i1) i1 = prtbufcopy(prtnum, work, mscitoa(resources, work));
		if (!i1) i1 = prtbufcopy(prtnum, " 0 R\n/Contents ", -1);
		if (!i1) i1 = prtbufcopy(prtnum, work, mscitoa((*print->fmt.pdf.pageobjnums)[i2], work));
		if (!i1) i1 = prtbufcopy(prtnum, " 0 R\n>>\nendobj\n", -1);
		if (i1) goto prtpdfend1;
	}

	/* catalog start */
	objectcount++;
	i1 = prtpdfoffset(prtnum);
	print = *printtabptr + prtnum;
	sprintf(work, "%d 0 obj\n<<\n/Type /Catalog", objectcount);
	if (!i1) i1 = prtbufcopy(prtnum, work, -1);
	sprintf(work, "\n/Pages %d 0 R\n>>\nendobj\n", pageparent);
	if (!i1) i1 = prtbufcopy(prtnum, work, -1);
	if (i1) goto prtpdfend1;

	off = print->filepos + print->bufcnt;
	i1 = prtbufcopy(prtnum, "xref\n0 ", -1);
	if (!i1) i1 = prtbufcopy(prtnum, work, mscitoa(print->fmt.pdf.offsetcount, work));
	if (!i1) i1 = prtbufcopy(prtnum, "\n", 1);
	if (i1) goto prtpdfend1;
	for (i3 = 0; i3 < print->fmt.pdf.offsetcount; i3++) {
		if (prtbufcopy(prtnum, (*print->fmt.pdf.offsets)[i3].offset, -1)) goto prtpdfend1;
	}
	i1 = prtbufcopy(prtnum, "trailer\n<<\n/Size ", -1);
	if (!i1) i1 = prtbufcopy(prtnum, work, mscitoa(objectcount + 1, work));
	if (!i1) i1 = prtbufcopy(prtnum, "\n/Root ", -1);
	if (!i1) i1 = prtbufcopy(prtnum, work, mscitoa(objectcount, work));
	if (!i1) i1 = prtbufcopy(prtnum, " 0 R\n/Info 1 0 R\n>>\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, "startxref\n", -1);
	if (!i1) i1 = prtbufcopy(prtnum, work, mscofftoa(off, work));
	if (!i1) i1 = prtbufcopy(prtnum, "\n%%EOF\n", -1);

prtpdfend1:
	if (print->fmt.pdf.imagecount > 0) {
		while (print->fmt.pdf.imagecount--) memfree((*print->fmt.pdf.images)[print->fmt.pdf.imagecount].image);
	}
	if (print->fmt.pdf.images != NULL) memfree((UCHAR**)print->fmt.pdf.images);
	memfree((UCHAR **) print->fmt.pdf.imgobjs);
	memfree((UCHAR **) print->fmt.pdf.offsets);
	memfree((UCHAR **) print->fmt.pdf.pageobjnums);
	if (print->fmt.pdf.efonts != NULL) {
		memfree((UCHAR **) print->fmt.pdf.efonts);
		print->fmt.pdf.efonts = NULL;
		print->fmt.pdf.efontsinuse = 0;
	}
	if (i1) {
		sprintf(work, "error in prtpdfend(), i1=%i", i1);
		prtputerror(work);
	}
	return i1;
}

INT prtflushlinepdf(INT prtnum, INT linecnt) {
	INT i1, i2, i3, angle, cnt, linetype, linevalue, move_position, size, width;
	DOUBLE fontHeight;
	FONTDATA *fd;
	PRINTINFO *print;
	UCHAR *line, *buf;
	USHORT usWork;
	CHAR work[256];
	USHORT *afmArray;
	INT entriesInAfmArray;
	DOUBLE d1, d2, charWidth, dwidth, newposx, newposy, radians, sizex, sizey;
	INT entriesInAfmFonts = sizeof(AFM_fonts) / sizeof(unsigned short*);

	print = *printtabptr + prtnum;

	/**
	 * The Text Matrix must change the y dimension so that
	 * positive goes up. Otherwise the glyphs paint upside down
	 */
	linetype = print->linetype;
	move_position = !(linetype & PRT_TEXT_ANGLE);
	i1 = prtbufcopy(prtnum, "n\nBT\n", -1);
	if (i1) return i1;

	/* Use afm tables for accurate width of built in fonts */
	if (print->fmt.pdf.fontselected >= 0 && print->fmt.pdf.fontselected < entriesInAfmFonts) {
		line = *print->linebuf;
		afmArray = AFM_fonts[print->fmt.pdf.fontselected];
		entriesInAfmArray = AFM_font_array_sizes[print->fmt.pdf.fontselected];
		for (i2 = 0, dwidth = 0.0; i2 < linecnt; i2++) {
			i3 = line[i2];
			i3 -= 32; /* The AFM arrays start with the blank character */
			if (0 <= i3 && i3 < entriesInAfmArray) {
				charWidth = afmArray[i3];
				dwidth += (charWidth * print->fontheight) / (DOUBLE)1e3;
			}
			else {
				if (print->fmt.pdf.fontselected > 3) dwidth += ((7 * print->fontheight) / 16);  /* times & helvetica */
				else dwidth += ((5 * print->fontheight) / 8);  /* courier */
			}
		}
		width = (INT) (dwidth + 0.5);
	}
	else if (print->fmt.pdf.fontselected > PDF_STD_FONT_BASE_COUNT) {
		/* line = *print->linebuf; */
		fontHeight = print->fontheight;
		i3 = print->fmt.pdf.fontselected - PDF_STD_FONT_BASE_COUNT - 1;
		i3 = (*print->fmt.pdf.efonts)[i3].ffh.fontIndex;
		fd = &FontFiles.fontData[i3];
		for (i2 = 0, dwidth = 0.0; i2 < linecnt; i2++) {
			i1 = getGlyphWidth(fd, (*(*printtabptr + prtnum)->linebuf)[i2], &usWork);
			if (i1) charWidth = 600;
			else charWidth = usWork;
			dwidth += (charWidth * fontHeight) / (DOUBLE)1e3;
		}
		width = (INT) (dwidth + 0.5);
		print = *printtabptr + prtnum; /* getGlyphWidth could move memory */
	}

	else {
		/*
		 * calculate length of text, making best guess for width without font metrics
		 */
		if (print->fmt.pdf.fontselected & 0x0C) width = linecnt * ((7 * print->fontheight) / 16);  /* times & helvetica */
		else width = linecnt * ((5 * print->fontheight) / 8);  /* courier */
	}

	sizex = width;
	if (linetype & PRT_TEXT_CENTER) {
		linevalue = (INT)((DOUBLE) print->linevalue * (72 / (DOUBLE) print->dpi)); /* scale */
		if (linevalue > width) sizex = (DOUBLE)(linevalue - width) / 2.00;
		else linetype &= ~PRT_TEXT_CENTER; /* text is wider than centering space */
	}

	if ((linetype & PRT_TEXT_ANGLE) && print->lineangle != 90) {
		angle = print->lineangle;
		/* convert to east = 0 (keep clockwise rotation) */
		if ((angle -= 90) < 0) angle += 360;
		radians = ((DOUBLE) angle * (DOUBLE) M_PI) / (DOUBLE) 180;
		sizey = sizex * sin(radians);
		sizex = sizex * cos(radians);
	}
	else {
		linetype &= ~PRT_TEXT_ANGLE;
		sizey = 0;
	}

	if (linetype & PRT_TEXT_CENTER) {
		newposx = print->fmt.pdf.posx;
		newposy = print->fmt.pdf.posy;
		print->fmt.pdf.posx += sizex;
		print->fmt.pdf.posy += sizey;
		if (linetype & PRT_TEXT_ANGLE) {
			newposx += linevalue * cos(radians);
			newposy += linevalue * sin(radians);
		}
		else newposx += linevalue;
	}
	else if (linetype & PRT_TEXT_RIGHT) {
		print->fmt.pdf.posx -= sizex;
		print->fmt.pdf.posy -= sizey;
	}
	if (linetype & PRT_TEXT_ANGLE) {  /* rotate text matrix */
		d1 = cos(radians);
		d2 = sin(radians);
		sprintf(work, "%.2f %.2f %.2f %.2f %.2f %.2f Tm\n", d1, d2, d2, 0 - d1,
			print->fmt.pdf.posx, print->fmt.pdf.posy);
		i1 = prtbufcopy(prtnum, work, -1);
	}
	else {
		i1 = prtbufcopy(prtnum, "1 0 0 -1 0 0 Tm\n", -1);
		if (i1) return i1;
		/*
		 * We had to re-invert the y axis in the Tm operator just above, otherwise the text
		 * glyphs painted upsidedown. As a consequence, the y coordinate in the next
		 * line must be negative
		 */
		sprintf(work, "%.2f %.2f Td\n", print->fmt.pdf.posx, 0 - (print->fmt.pdf.posy + (DOUBLE) print->fontheight - 1.00));
		i1 = prtbufcopy(prtnum, work, -1);
	}
	if (i1) return i1;
	i1 = prtbufcopy(prtnum, print->fmt.pdf.fontname, -1);
	if (!i1) i1 = prtbufcopy(prtnum, "(", 1);
	if (i1) return i1;

	/* copy line data to buffer */
	cnt = print->bufcnt;
	size = print->bufsize;
	line = *print->linebuf;
	buf = *print->buffer;
	for (i2 = 0; i2 < linecnt; ) {
		if (cnt + 2 > size) {
			print->bufcnt = cnt;
			i1 = prtflushbuf(prtnum);
			if (i1) return i1;
			cnt = print->bufcnt;
		}
		if (line[i2] == '(' || line[i2] == ')' || line[i2] == '\\') buf[cnt++] = '\\';
		buf[cnt++] = line[i2++];
	}
	print->bufcnt = cnt;
	/* add data postfix */
	i1 = prtbufcopy(prtnum, ") Tj\nET\n", 8);
	if (i1) return i1;
	/* set new position */
	if (move_position) {
		if (linetype & PRT_TEXT_CENTER) {
			print->fmt.pdf.posx = newposx;
			print->fmt.pdf.posy = newposy;
		}
		else {
			print->fmt.pdf.posx += sizex;
			print->fmt.pdf.posy += sizey;
		}
	}
	sprintf(work, "%.2f %.2f m\n", print->fmt.pdf.posx, print->fmt.pdf.posy);
	i1 = prtbufcopy(prtnum, work, -1);
	if (i1) return i1;
	return 0;
}

/*
 * A PDF needs linewidth in user space units, 1/72 inch
 * A DB/C programmer expresses it in pixels. Typically these are 1/300 inch (or 300 DPI)
 * Convert.
 */
INT prtpdflinewidth(INT prtnum, INT width) {
	INT i1;
	CHAR work[64];
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	i1 = prtdbltoa((DOUBLE) (width * PDFUSERSPACEUNITS) / (DOUBLE) print->dpi, work, 2);
	memcpy(work + i1, " w\n", 3);
	i1 = prtbufcopy(prtnum, work, i1 + 3);
	if (i1) return i1;
	return 0;
}

/**
 * Takes a zero-based prtnum
 */
INT prtpdftab(INT prtnum, INT pos, UINT oldTabbing)
{
	INT i1, i3;
	PRINTINFO *print;
	DOUBLE charWidth, dwidth;
	FONTDATA *fd;
	USHORT usWork;

	*prterrorstring = '\0';
	if (pos < 0) return 0;

	print = *printtabptr + prtnum;
	i1 = prtflushline(prtnum, FALSE);
	if (i1) return i1;

	print = *printtabptr + prtnum;
	print->linebgn = print->linepos = pos;  /* incase we are better off using blanks to position */

	if (oldTabbing) {
		print->fmt.pdf.posx = (DOUBLE) (pos * ((7 * print->fontheight) / 12));
		return 0;
	}

	/* Is it the built-in Courier? */
	if (0 <= print->fmt.pdf.fontselected && print->fmt.pdf.fontselected <= 3) {
		charWidth = AFM_Courier[0]; /* All Courier widths are the same */
		dwidth = (charWidth * print->fontheight) / (DOUBLE)1e3;
		print->fmt.pdf.posx = (DOUBLE) (pos * dwidth);
		return 0;
	}

	/* If not built-in, is it monospaced? */
	if (print->fmt.pdf.fontselected > PDF_STD_FONT_BASE_COUNT) {
		i3 = print->fmt.pdf.fontselected - PDF_STD_FONT_BASE_COUNT - 1;
		i3 = (*print->fmt.pdf.efonts)[i3].ffh.fontIndex;
		fd = &FontFiles.fontData[i3];
		if (fd->post_data.isFixedPitch) {
			i1 = getGlyphWidth(fd, ' ', &usWork);
			if (i1) charWidth = 600;
			else charWidth = usWork;
			print = *printtabptr + prtnum;	/* getGlyphWidth could move memory */
			dwidth = (charWidth * print->fontheight) / (DOUBLE)1e3;
			print->fmt.pdf.posx = (DOUBLE) (pos * dwidth);
			return 0;
		}
	}

	print->fmt.pdf.posx = (DOUBLE) (pos * ((7 * print->fontheight) / 12));
	return 0;
}


