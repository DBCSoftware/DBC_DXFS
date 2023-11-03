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
#define INC_STDLIB
#define INC_STRING
#include "includes.h"
#include "base.h"
#include "gui.h"
#include "guix.h"
#include "imgs.h"

#if OS_WIN32GUI
RGBQUAD quadclrs[256];
static void setDefaultPalette(void);
static INT set256GrayscalePalette(PIXMAP *pix);
#else
static void changecolor(PIXHANDLE, INT32, INT, INT);
#endif

#if OS_WIN32GUI
void forceRepaint(PIXHANDLE ph) {
	PIXMAP *pix = *ph;
	WINHANDLE wh = pix->winshow;
	WINDOW *win = *wh;
	RECT rect;

	rect.left = pix->hshow - win->scrollx;
	rect.top = pix->vshow - win->scrolly + win->bordertop;
	rect.right = rect.left + pix->hsize;
	rect.bottom = rect.top + pix->vsize;
	if (!IsRectEmpty(&rect)) {
		SendMessage(win->hwnd, GUIAM_SHOWPIX, (WPARAM) &rect, (LPARAM) ph);
	}
}
#endif

/*
 * Note, calls malloc and realloc, does not call memalloc
 * Returns pointer to memory block if success, NULL if error.
 * Returns number of bytes in the buffer in size.
 */
CHAR *encodeimage(PIXHANDLE pixhandle, INT *size)
{
	INT i1, i2, i3, i4, i5, bytes, length, len, widthBytesOut;
	UCHAR *bits, *newbits, *result, *ptr1;
	PIXMAP *pix;
#if OS_WIN32GUI
	BITMAP bm;
	UCHAR *ptrend;
#else
	INT32 color;
	INT widthBytesIn;
#endif

	pix = *pixhandle;
	if (pix->bpp == 1) {
		widthBytesOut = (pix->hsize + 7) >> 3;
		len = pix->vsize * widthBytesOut;
	}
	else {
		bytes = pix->vsize * pix->hsize;
		if (pix->bpp == 4) len = (bytes / 2) + ((bytes % 2) ? 1 : 0); /* 1:2 */
		else len = bytes * 3; /* 3:1 */
	}
	bits = (UCHAR *) calloc(len, sizeof(UCHAR));
	if (bits == NULL) return NULL;
#if OS_WIN32GUI
	GetObject(pix->hbitmap, sizeof(BITMAP), &bm);
	bm.bmWidthBytes = ((pix->hsize * bm.bmBitsPixel + 0x1F) & ~0x1F) >> 3; // needed on Win2000??
	ptrend = (UCHAR *) bm.bmBits + bm.bmWidthBytes * (bm.bmHeight - 1);
	if (bm.bmBitsPixel == 1) i3 = 7;
	else if (bm.bmBitsPixel == 8) {
		if (pix->isGrayScaleJpeg) {
			if (set256GrayscalePalette(pix) == RC_NO_MEM) return NULL;
		}
		else setDefaultPalette();
	}
	else i3 = 0;

	for (i1 = i4 = i5 = 0; i1 < pix->vsize; i1++) {
		/* bitmap scanlines are stored in reverse order, so start from the end */
		ptr1 = ptrend - bm.bmWidthBytes * i1;
		if (pix->bpp == 1) for (i2 = 0; i2 < widthBytesOut; i2++) bits[i5++] = *ptr1++;
		else {
			if (bm.bmBitsPixel == 1) i5 = 7;
			else if (bm.bmBitsPixel == 4) i5 = 0;
			for (i2 = 0; i2 < bm.bmWidth; i2++) {
				if (bm.bmBitsPixel == 4) {
					if ((i3 & 0x1) == (i5 & 0x1)) {
						if (!(i3++ & 0x1)) bits[i4] = (*ptr1 & 0xF0); /* left */ 
						else bits[i4++] |= (*ptr1 & 0x0F); /* right */
					}
					else {
						/* row of pixels starts on right 4 bits of input */
						if (!(i3++ & 0x1)) bits[i4] = (*ptr1 & 0x0F) << 4; /* left */
						else bits[i4++] |= (*ptr1 & 0xF0) >> 4; /* right */
					}
					if ((i5 & 0x1)) ptr1++;
					i5++;
				}
				else if (bm.bmBitsPixel == 8) {
					bits[i4] = quadclrs[*ptr1].rgbBlue;
					bits[i4 + 1] = quadclrs[*ptr1].rgbGreen;
					bits[i4 + 2] = quadclrs[*ptr1].rgbRed;
					ptr1++;
					i4 += 3;
				}
				else {
					bits[i4++] = *ptr1++;
					bits[i4++] = *ptr1++;
					bits[i4++] = *ptr1++;
				}
			}
		}
	}
	
#else
	if (pix->bpp == 1) {
		ptr1 = *pix->pixbuf;
		widthBytesIn = ((pix->hsize + 0x1F) & ~0x1F) >> 3; /* rounded up to nearest four bytes */
	}
	for (i2 = i4 = i5 = 0, i3 = 7; i2 < pix->vsize; i2++) {
		if (pix->bpp == 1) {
			memcpy(bits + widthBytesOut * i2, ptr1 + widthBytesIn * i2, widthBytesOut);
		}
		else {
			for (i1 = 0; i1 < pix->hsize; i1++) {
				if (guipixgetcolor(pixhandle, &color, i1, i2)) {
					*size = 0;
					free(bits);
					return NULL;
				}
				if (pix->bpp == 4) {
					if (!(i5++ & 0x1)) { /* even */
						if (color == 0x0) bits[i4] = 0 << 4;
						else if (color == 0x800000) bits[i4] = 0x1 << 4;
						else if (color == 0x008000) bits[i4] = 0x2 << 4;
						else if (color == 0x808000) bits[i4] = 0x3 << 4;
						else if (color == 0x000080) bits[i4] = 0x4 << 4;
						else if (color == 0x800080) bits[i4] = 0x5 << 4;
						else if (color == 0x008080) bits[i4] = 0x6 << 4;
						else if (color == 0xC0C0C0) bits[i4] = 0x7 << 4;
						else if (color == 0x808080) bits[i4] = 0x8 << 4;
						else if (color == 0xFF0000) bits[i4] = 0x9 << 4;
						else if (color == 0x00FF00) bits[i4] = 0xA << 4;
						else if (color == 0xFFFF00) bits[i4] = 0xB << 4;
						else if (color == 0x0000FF) bits[i4] = 0xC << 4;
						else if (color == 0xFF00FF) bits[i4] = 0xD << 4;
						else if (color == 0x00FFFF) bits[i4] = 0xE << 4;
						else if (color == 0xFFFFFF) bits[i4] = 0xF << 4;
					}
					else {
						if (color == 0x0) bits[i4] |= 0;
						else if (color == 0x800000) bits[i4] |= 0x1;
						else if (color == 0x008000) bits[i4] |= 0x2;
						else if (color == 0x808000) bits[i4] |= 0x3;
						else if (color == 0x000080) bits[i4] |= 0x4;
						else if (color == 0x800080) bits[i4] |= 0x5;
						else if (color == 0x008080) bits[i4] |= 0x6;
						else if (color == 0xC0C0C0) bits[i4] |= 0x7;
						else if (color == 0x808080) bits[i4] |= 0x8;
						else if (color == 0xFF0000) bits[i4] |= 0x9;
						else if (color == 0x00FF00) bits[i4] |= 0xA;
						else if (color == 0xFFFF00) bits[i4] |= 0xB;
						else if (color == 0x0000FF) bits[i4] |= 0xC;
						else if (color == 0xFF00FF) bits[i4] |= 0xD;
						else if (color == 0x00FFFF) bits[i4] |= 0xE;
						else if (color == 0xFFFFFF) bits[i4] |= 0xF;
						i4++;
					}
				}
				else {
					bits[i4++] = (color & 0xFF0000) >> 16;
					bits[i4++] = (color & 0x00FF00) >> 8;
					bits[i4++] = color & 0x0000FF;
				}
			}
		}
	}
#endif
	/* a buffer 50% larger should suffice */
	result = (UCHAR *) malloc(((len / 2 + 1) * 3 + 4) * sizeof(UCHAR));
	if (result == NULL) {
		free(bits);
		return NULL;
	}
	if (runlengthencode(bits, len, result, &length, (len / 2 + 1) * 3 + 4) < 0) {
		free(bits);
		free(result);
		return NULL;
	}
	/* need more than 33% more room for 3:4 conversion to printable */
	newbits = (UCHAR *) realloc(bits, (length / 3 + 1) * 4 * sizeof(UCHAR));
	if (newbits == NULL) {
		free(bits);
		free(result);
		return NULL;
	}
	bits = newbits;
	makeprintable(result, length, bits, size);
	free(result);
	bits[*size] = 0;
	return (CHAR *) bits;
}

/**
 * Used from dbcsc.c in response to a storeimagebits command
 * Used from vprint in dbcprt.c when an image is printed and we are in an SC scenario
 * Used from ctlstore in dbcctl.c in response to a STORE verb and we are in an SC scenario
 */
INT decodeimage(PIXHANDLE pixhandle, UCHAR *bits, INT size)
{
	INT i1, i2, i3, i4, i5, bytes, length, len, widthBytesSrc;
	PIXMAP *pix;
	UCHAR *result, *data;
#if OS_WIN32GUI
	DWORD dwWaitResult;
	BITMAP bm;
	UCHAR *ptr1, *ptrend;	
#else
	INT32 color, widthBytesDest;
	UCHAR *srcptr, *destptr;
#endif

	pix = *pixhandle;
#if OS_WIN32GUI
	dwWaitResult = WaitForSingleObject(pix->hPaintingMutex, INFINITE);
	if (dwWaitResult == WAIT_FAILED) {
		guiaErrorMsg("Failure waiting for pix mutex", GetLastError());
		return RC_ERROR;
	}
#endif

	/* need 25% less space to store non printable characters */
	result = (UCHAR *) malloc((((size >> 2) + 1) * 3 + 10));
	if (result == NULL) return RC_NO_MEM;
	fromprintable(bits, size, result, &length);

	if (pix->bpp == 1) {
		widthBytesSrc = (pix->hsize + 7) >> 3;
		len = pix->vsize * widthBytesSrc;
	}
	else {
		bytes = pix->vsize * pix->hsize;
		if (pix->bpp == 4) len = (bytes / 2) + ((bytes % 2) ? 1 : 0); /* 1:2 */
		else len = bytes * 3; /* 3:1 */
	}

	data = (UCHAR *) malloc(len * sizeof(UCHAR));
	if (data == NULL) {
		free(result);
		return RC_NO_MEM;
	}	
	if (runlengthdecode(result, length, data, &len) < 0) {
		free(result);
		free(data);
		return 0;
	} 
	free(result);

#if OS_WIN32GUI 
	GetObject((*pixhandle)->hbitmap, sizeof(BITMAP), &bm);
	bm.bmWidthBytes = ((pix->hsize * bm.bmBitsPixel + 0x1F) & ~0x1F) >> 3;
	memset(bm.bmBits, 0x00, bm.bmHeight * bm.bmWidthBytes);
	ptrend = (UCHAR *) bm.bmBits + bm.bmWidthBytes * (bm.bmHeight - 1);
	for (i1 = i3 = i4 = 0; i1 < pix->vsize; i1++) {
		
		ptr1 = ptrend - bm.bmWidthBytes * i1;		
		
		if (bm.bmBitsPixel == 1) memcpy(ptr1, data + i1 * widthBytesSrc, widthBytesSrc);
		else if (bm.bmBitsPixel == 24) {
			i2 = 3 * bm.bmWidth;
			memcpy(ptr1, &data[i4], i2);
			i4 += i2;
		}
		else  {
			for (i2 = i5 = 0; i2 < bm.bmWidth; i2++) {
				if (bm.bmBitsPixel == 4) {
					if ((i3 & 0x1) == (i5 & 0x1)) {
						if (!(i3++ & 0x1)) *ptr1 |= (data[i4] & 0xF0); /* left */ 
						else *ptr1 |= (data[i4++] & 0x0F); /* right */
					}
					else {
						/* row of pixels starts on right 4 bits of input */
						if (!(i3++ & 0x1)) *ptr1 |= (data[i4] & 0xF0) >> 4; /* right */	
						else *ptr1 |= (data[i4++] & 0x0F) << 4; /* left */
					}
					if ((i5 & 0x1)) ptr1++;
					i5++;
				}
				else if (bm.bmBitsPixel == 8) {
					i5 = GetNearestPaletteIndex(pix->hpal, RGB(data[i4 + 2], data[i4 + 1], data[i4]));
					if ((UINT) i5 == CLR_INVALID) *ptr1 = 0xFF;
					else *ptr1 = i5;
					ptr1++;
					i4 += 3;
				}
			}
		}
	}
#else
	if (pix->bpp == 1) {
		srcptr = data;
		destptr = *pix->pixbuf;
		/*
		 * Round up the destination bytes per row to the nearest 4-byte multiple
		 */
		widthBytesDest = ((pix->hsize + 0x1F) & ~0x1F) >> 3;
		for (i1 = 0; i1 < pix->vsize; i1++) {
			memcpy(destptr, srcptr, widthBytesSrc);
			srcptr += widthBytesSrc;
			destptr += widthBytesDest;
		}
		goto enddecodeimage;
	}
	for (i2 = i4 = i5 = 0, i3 = 7; i2 < pix->vsize; i2++) {
		for (i1 = 0; i1 < pix->hsize; i1++) {
			if (pix->bpp == 4) {
				if (!(i5++ & 0x1)) i3 = data[i4] >> 4;
				else i3 = data[i4++] & 0x0F;
				switch(i3) {
					case 0x0: changecolor(pixhandle, 0x000000, i1, i2); break; /* dark black */
					case 0x1: changecolor(pixhandle, 0x800000, i1, i2); break; /* dark blue */
					case 0x2: changecolor(pixhandle, 0x008000, i1, i2); break; /* dark green */
					case 0x3: changecolor(pixhandle, 0x808000, i1, i2); break; /* dark cyan */
					case 0x4: changecolor(pixhandle, 0x000080, i1, i2); break; /* dark red */
					case 0x5: changecolor(pixhandle, 0x800080, i1, i2); break; /* dark magenta */
					case 0x6: changecolor(pixhandle, 0x008080, i1, i2); break; /* dark yellow */
					case 0x7: changecolor(pixhandle, 0xC0C0C0, i1, i2); break; /* light gray */
					case 0x8: changecolor(pixhandle, 0x808080, i1, i2); break; /* medium gray */
					case 0x9: changecolor(pixhandle, 0xFF0000, i1, i2); break; /* blue */
					case 0xA: changecolor(pixhandle, 0x00FF00, i1, i2); break; /* green */
					case 0xB: changecolor(pixhandle, 0xFFFF00, i1, i2); break; /* cyan */
					case 0xC: changecolor(pixhandle, 0x0000FF, i1, i2); break; /* red */
					case 0xD: changecolor(pixhandle, 0xFF00FF, i1, i2); break; /* magenta */
					case 0xE: changecolor(pixhandle, 0x00FFFF, i1, i2); break; /* yellow */
					case 0xF: changecolor(pixhandle, 0xFFFFFF, i1, i2); break; /* white */
				}
			}
			else {
				color = (data[i4] << 16) | (data[i4 + 1] << 8) | data[i4 + 2];
				changecolor(pixhandle, color, i1, i2);
				i4 += 3;
			}
		}
	}
enddecodeimage:
#endif

#if OS_WIN32GUI
	ReleaseMutex(pix->hPaintingMutex);
#endif
	free(data);
	return 0;
}

#if !OS_WIN32GUI
/**
 * Set the color value of the pixel at the position h, v
 * 'color' is assumed to be BGR
 */
static void changecolor(PIXHANDLE ph, INT32 color, INT h, INT v)
{
	UCHAR *ptr;
	PIXMAP *pix = *ph;
	ptr = *pix->pixbuf + (((pix->hsize * pix->bpp + 0x1F) & ~0x1F) >> 3) * v;  /* beginning of row */
	ptr += h * 3;
	ptr[2] = color >> 16;
	ptr[1] = (color >> 8) & 0xFF;
	ptr[0] = color & 0xFF;
}
#endif

/**
 * Run Length Encode 
 */
INT runlengthencode(UCHAR *idata, INT ilen, UCHAR *odata, INT *olen, INT max)
{
	INT i1, cflag, count, end, flag, fpos;
	UCHAR c1, c2;
	
	*olen = 0;
	for (fpos = 0, flag = end = FALSE ; ; ) {
		for (cflag = FALSE, count = 1; ; count++) {
			if (fpos + count - 1 >= ilen) {
				end = TRUE;
				count--; 
				break;
			}
			c1 = idata[fpos + count - 1];
			if (cflag && c1 == c2) {
				count -= 2;
				flag = TRUE;
				break;
			}
			c2 = c1;
			cflag = TRUE;
			if (count == 128) {
				break;
			}
		}

		if (count > 0) {
			odata[(*olen)++] = (UCHAR) (count - 1);
			if (*olen > max) return -1;
			while (count--) {
				odata[(*olen)++] = idata[fpos++];
				if (*olen > max) return -1;
			}
		}
	
		if (flag) {
			i1 = 2;
			fpos++;
			while ((++fpos < ilen) && (idata[fpos] == c1)) {
				i1++;
				if (i1 == 128) {
					fpos++;
					break;
				}
			}
			odata[(*olen)++] = (UCHAR) (257 - i1); 
			if (*olen > max) return -1;
			odata[(*olen)++] = c1;
			if (*olen > max) return -1;
			flag = FALSE;
		}

		if (end) break;
	}
	
	odata[(*olen)++] = (UCHAR) 0x80; /* EOD */
	if (*olen > max) return -1;
				
	/* Make sure data is exactly divisible by 3 for */  
	/* later conversion to printable characters */

	while (*olen % 3) {
		odata[(*olen)++] = (UCHAR) 0x00; 
		if (*olen > max) return -1;
	}	
	return 0;
}

/**
 * Run Length Decode 
 */
INT runlengthdecode(UCHAR *idata, INT ilen, UCHAR *odata, INT *olen)
{
	INT i1, i2, i3, limit = *olen;
	for (*olen = i1 = 0; i1 < ilen; i1++) {
		i2 = (INT) idata[i1];
		if (i2 < 128) { /* No Run */ 
			for (++i2, i3 = 0; i3 < i2; i3++) {
				if (i1 + 1 >= ilen) return RC_ERROR;
				else {
					if (*olen + 1 > limit) {
						return -2;
					}
					odata[(*olen)++] = idata[++i1];
				}
			}
		}
		else if (i2 > 128) { /* Run */
			if (i1 + 1 >= ilen) return RC_ERROR;
			for (i1++, i3 = 0; i3 < (257 - i2); i3++) {
				if (*olen + 1 > limit) {
					return -2;
				}
				odata[(*olen)++] = idata[i1];
			}
		}
		else return 0; /* EOD */
	}
	
	return 0;
}

#if OS_WIN32GUI
static void setDefaultPalette(void)
{
	INT iCurClr, iCurRed, iCurGreen, iCurBlue;
	UCHAR ucGrayShade;
	static INT complete = FALSE;

	if (complete) return;

/* fill in colors used for 16 and 256 color palette modes */
	for (iCurClr = 0; iCurClr < 16; iCurClr++) {
		quadclrs[iCurClr].rgbBlue = DBC_BLUE(dwDefaultPal[iCurClr]);
		quadclrs[iCurClr].rgbGreen = DBC_GREEN(dwDefaultPal[iCurClr]);
		quadclrs[iCurClr].rgbRed = DBC_RED(dwDefaultPal[iCurClr]);
		quadclrs[iCurClr].rgbReserved = 0;
	}

/* fill in colors used only in 256 color palette modes */
	for (iCurRed = 0; iCurRed < 256; iCurRed += 51) {
		for (iCurGreen = 0; iCurGreen < 256; iCurGreen += 51) {
			for (iCurBlue = 0; iCurBlue < 256; iCurBlue += 51) {
				quadclrs[iCurClr].rgbBlue = (UCHAR) iCurBlue;
				quadclrs[iCurClr].rgbGreen = (UCHAR) iCurGreen;
				quadclrs[iCurClr].rgbRed = (UCHAR) iCurRed;
				quadclrs[iCurClr++].rgbReserved = 0;
			}
		}
	}
/* finish off with a spectrum of gray shades */
	ucGrayShade = 8;
	for (; iCurClr < 256; iCurClr++) {
		quadclrs[iCurClr].rgbBlue = ucGrayShade;
		quadclrs[iCurClr].rgbGreen = ucGrayShade;
		quadclrs[iCurClr].rgbRed = ucGrayShade;
		quadclrs[iCurClr].rgbReserved = 0;
		ucGrayShade += 8;
	}
	
	complete = TRUE;
}

static INT set256GrayscalePalette(PIXMAP *pix) {
	int rc;
	HWND hWnd = guia_hwnd();
	HDC hDC;               /* device context for main window */
	BITMAPINFO *pbmi2;
	RGBQUAD *pColor;
	
	if ((hDC = GetDC(hWnd)) == NULL){
		setDefaultPalette();
		return 0;
	}
	pbmi2 = (BITMAPINFO *) malloc(sizeof(BITMAPINFOHEADER) + (256 * sizeof(RGBQUAD)));
	if (pbmi2 == NULL) {
		return RC_NO_MEM;
	}
	memset(&pbmi2->bmiHeader, 0, sizeof(BITMAPINFOHEADER));
	pbmi2->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pbmi2->bmiHeader.biWidth = pix->hsize;
	pbmi2->bmiHeader.biHeight = 0 - pix->vsize;
	pbmi2->bmiHeader.biPlanes = 1;
	pbmi2->bmiHeader.biBitCount = (unsigned short) pix->bpp;
	pbmi2->bmiHeader.biCompression = BI_RGB;
	pbmi2->bmiHeader.biSizeImage = 0;
	pbmi2->bmiHeader.biXPelsPerMeter = 1000;
	pbmi2->bmiHeader.biYPelsPerMeter = 1000;
	pbmi2->bmiHeader.biClrUsed = 256;
	pbmi2->bmiHeader.biClrImportant = 256;

	
	rc = GetDIBits(hDC, pix->hbitmap, 0, pix->vsize, NULL, pbmi2, DIB_RGB_COLORS);
	if (rc == 0) {
		ReleaseDC(hWnd, hDC);
		setDefaultPalette();
		return 0;
	}
	ReleaseDC(hWnd, hDC);

	/*
	 * Move the colors from our stack area to the global area (quadclrs)
	 */
	pColor = (RGBQUAD*)((LPSTR)pbmi2 + (WORD)(pbmi2->bmiHeader.biSize));
	memcpy(quadclrs, pColor, 256 * sizeof(RGBQUAD));
	
	free(pbmi2);
	return 0;
}

#endif
