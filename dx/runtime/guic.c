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

#define INC_STRING
#define INC_TIME
#include "includes.h"
#include "base.h"
#include "gui.h"
#include "guix.h"
#include "imgs.h"

/**
 * Defining PNG_INTERNAL is needed to get png.h to define prototypes for
 * png_get_x_pixels_per_inch and png_get_y_pixels_per_inch
 */
#define PNG_INTERNAL
#include "../png/png.h"

#include "dbc.h"
#include <assert.h>

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

static UCHAR *dest;
static png_infop info_ptr;
static png_uint_32 pix_width, pix_height;
static int png_color_type;
static png_structp png_ptr;
static png_uint_32 png_width;
static png_uint_32 png_height;
static UCHAR* png_io_buffer;
static OFFSET png_io_pointer;
static int png_bit_depth;
static png_bytepp rowpointers;
static png_uint_32 srcrowbytes;
static UINT destrowbytes;
static UINT rowmax;
static png_bytep source;

#if OS_WIN32GUI
static HWND hWnd;			/* main window */
static HDC hDC;             /* device context for main window */
static ZHANDLE tempbuf;
static UINT tbufsize;
#endif


static void png_cleanup(void);
static INT  png_setup_info(UCHAR* buffer);
static void png_error_fn(png_structp png_ptr, png_const_charp error_msg);
static void png_warning_fn(png_structp png_ptr, png_const_charp warning_msg);

/*
 * The following is now defined in png.h when PNG_INTERNAL is defined, so comment out this one.
 *
 * static void png_read_data(png_structp png_ptr, png_bytep data, png_size_t length);
 */

static INT topix1(PIXMAP *pix);
#if OS_WIN32GUI
static void packPalette(UCHAR *palette, INT numentries);
static INT topix4(PIXMAP *pix);
static INT topix8(PIXMAP *pix);
static INT copyPngToPixmap_to_4(PIXMAP *pix);
#endif
static INT topix24(PIXMAP *pix);
static INT allocMemAndReadImage(void);
static INT copyPngToPixmap(PIXMAP *pix);
static INT copyPngToPixmap_to_1(PIXMAP *pix, png_byte bwcutoff);

/* PNG Error handler */
#ifdef __GNUC__
static void png_error_fn(png_structp __attribute__ ((unused)) png_ptr, png_const_charp error_msg) {
#else
static void png_error_fn(png_structp png_ptr, png_const_charp error_msg) {
#endif
	dbcerrinfo(792, (UCHAR *)error_msg, (INT)strlen(error_msg));
}

/**
 * PNG Warning handler
 * We do nothing in here. We must do this because
 * the png system default is to send the warning message to stdout.
 * And we do not want that!
 */
static void png_warning_fn(png_structp png_ptr, png_const_charp warning_msg) {
}

/* PNG Read function */
static void dx_png_read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
	memcpy(data, &png_io_buffer[png_io_pointer],  length);
	png_io_pointer += length;
}

/**
 * Create the PNG main structure.
 * Set the I/O, error, and warning functions.
 * Create the PNG Info structure
 */
static INT png_setup_info(UCHAR* buffer) {

	/* Create the main PNG structure */
    png_ptr = png_create_read_struct
       (PNG_LIBPNG_VER_STRING, NULL, png_error_fn, png_warning_fn);
    if (png_ptr == NULL) return RC_ERROR;

    /* Tell the PNG system to use our read routine */
	png_set_read_fn(png_ptr, NULL, dx_png_read_data);

	/* Create the info structure */
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
	    png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
	    return RC_ERROR;
	}

    /* Initialize our i/o variables */
    png_io_buffer = buffer;
    png_io_pointer = 0;

    return 0;
}

/* PNG Resolution */
INT guicPngRes(UCHAR *buffer, INT *xres, INT *yres)
{
	INT i1;

	if ( (i1 = png_setup_info(buffer)) ) return i1;

	/* Tell the PNG system to read image information */
	png_read_info(png_ptr, info_ptr);

	/* Access the data from the PNG info structure */
	*xres = png_get_x_pixels_per_inch(png_ptr, info_ptr);
	*yres = png_get_y_pixels_per_inch(png_ptr, info_ptr);

	png_cleanup();
	return 0;
}

/* PNG SIZE */
INT guicPngSize(UCHAR *buffer, INT *x, INT *y)
{
	INT i1;

	if ( (i1 = png_setup_info(buffer)) ) return i1;

	/* Tell the PNG system to read image information */
	png_read_info(png_ptr, info_ptr);

	/* Access the data from the PNG info structure */
	*x = png_get_image_width(png_ptr, info_ptr);
	*y = png_get_image_height(png_ptr, info_ptr);

	png_cleanup();
	return 0;
}

/* PNG BPP */
INT guicPngBPP(UCHAR *buffer, INT *x)
{
	INT i1;
	png_byte channels, bitdepth;

	if ( (i1 = png_setup_info(buffer)) ) return i1;

	/* Tell the PNG system to read image information */
	png_read_info(png_ptr, info_ptr);

	/* Access the data from the PNG info structure */
	channels = png_get_channels(png_ptr, info_ptr);
	bitdepth = png_get_bit_depth(png_ptr, info_ptr);
	*x = channels * bitdepth;

	png_cleanup();
	return 0;
}

/*
 * GUIAPNGTOPIXMAP
 *
 * bufhandle is from memalloc, it is allocated in dbcctl.c function ctlload.
 * It is the size of the image file and is expected to already contain the
 * entire image file.
 */
INT guicPngToPixmap(PIXHANDLE pixhandle, UCHAR **bufhandle)
{
	UINT i1;
	INT rc;
	PIXMAP *pix = *pixhandle;

	assert(pix != NULL);
#if OS_WIN32GUI
	assert(pix->hbitmap != NULL);
	assert(pix->pbmi != NULL);
#endif

	pix_width = pix->hsize;
	pix_height = pix->vsize;
	destrowbytes = ((pix_width * pix->bpp + 31) / 32) * 4;
	if ( (i1 = png_setup_info(*bufhandle)) ) return i1;

#if OS_WIN32GUI
	hWnd = guia_hwnd();
	if ((hDC = GetDC(hWnd)) == NULL) return RC_ERROR;
#endif

	/* Tell the PNG system to read image information */
    png_read_info(png_ptr, info_ptr);

	png_get_IHDR(png_ptr, info_ptr, &png_width, &png_height,
		&png_bit_depth, &png_color_type, NULL, NULL, NULL);

	/* This is the number of rows that we will ultimately transfer to the image variable */
	rowmax = min(png_height, pix_height);

	/* We handle only up to 24 bit color, so if the image is 48 bit, strip it */
	if (png_bit_depth == 16) {
		png_set_strip_16(png_ptr);
		png_bit_depth = 8;
	}

	/* We do not handle an alpha channel, so if there is one, get rid of it */
	if (png_color_type & PNG_COLOR_MASK_ALPHA) {
		png_set_strip_alpha(png_ptr);
	}

	rc = 0;
	switch (pix->bpp) {
		case 1:
			rc = topix1(pix);
			break;
		case 4:
#if OS_WIN32GUI
			rc = topix4(pix);
#else
			rc = topix24(pix);
#endif
			break;
		case 8:
#if OS_WIN32GUI
			rc = topix8(pix);
#else
			rc = topix24(pix);
#endif
			break;
		case 16:	/* We do not use 16 bit images. If the user says 16, we always use 24 */
		case 24:
			rc = topix24(pix);
			break;
	}
	png_cleanup();
#if OS_WIN32GUI
	if (pix->winshow != NULL) forceRepaint(pixhandle);
#endif
	return rc;
}

/**
 * Free memory for PNG structures and rowbytes
 */
static void png_cleanup() {
	png_uint_32 i1;
	if (rowpointers != NULL) {
		for (i1 = 0; i1 < png_height; i1++) png_free(png_ptr, rowpointers[i1]);
		png_free(png_ptr, rowpointers);
	}
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	png_ptr = NULL;
	info_ptr = NULL;
	rowpointers = NULL;
}

/**
 * We are filling a 1bpp image variable.
 * If the PNG image is 1 bit grayscale, it is a simple copy.
 * If the PNG image is 2 or 4 bit grayscale, have the libpng expand it to 8 and we dither it ourselves.
 * If the PNG image is 8 bit grayscale, have the libpng convert it to RGB and dither it.
 * If the PNG image is RGB, have the libpng dither it.
 * If the PNG image is PALETTE, have the libpng convert it to rgb and dither it.
 */
static INT topix1(PIXMAP *pix) {
	INT rc;
	png_color b_and_w[] = {
		{  0,   0,   0},
		{255, 255, 255}
	};

	if (png_color_type == PNG_COLOR_TYPE_GRAY || png_color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
		if (png_bit_depth == 1) {
			if ( (rc = allocMemAndReadImage()) ) return rc;
			return copyPngToPixmap(pix);
		}
		else if (png_bit_depth == 2 || png_bit_depth == 4) {
			png_set_expand_gray_1_2_4_to_8(png_ptr);
			if ( (rc = allocMemAndReadImage()) ) return rc;
			return copyPngToPixmap_to_1(pix, (png_byte) 127);
		}
		else if (png_bit_depth == 8) {
			png_set_gray_to_rgb(png_ptr);
			png_set_quantize(png_ptr, b_and_w, 2, 2, NULL, 1);
			if ( (rc = allocMemAndReadImage()) ) return rc;
			return copyPngToPixmap_to_1(pix, (png_byte) 0);
		}
	}
	else if (png_color_type == PNG_COLOR_TYPE_RGB || png_color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
		//png_set_dither(png_ptr, b_and_w, 2, 2, NULL, 1);
		png_set_quantize(png_ptr, b_and_w, 2, 2, NULL, 1);
		if ( (rc = allocMemAndReadImage()) ) return rc;
		return copyPngToPixmap_to_1(pix, (png_byte) 0);
	}
	else if (png_color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png_ptr);
		png_set_quantize(png_ptr, b_and_w, 2, 2, NULL, 1);
		if ( (rc = allocMemAndReadImage()) ) return rc;
		return copyPngToPixmap_to_1(pix, (png_byte) 0);
	}
	return RC_ERROR;	/* If we get here, then color_type was an unknown value */
}

/**
 * We are filling a 24bpp image variable.
 * Paletted and grayscale PNGs are converted by the libpng to RGB
 */
static INT topix24(PIXMAP *pix) {
	INT rc;

	/* Expand paletted colors into true RGB triplets */
	if (png_color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png_ptr);
#if OS_WIN32GUI
		png_set_bgr(png_ptr);
#endif
	}
#if OS_WIN32GUI
	/* If we are windows gui, reverse RGB to BGR */
	else if (png_color_type == PNG_COLOR_TYPE_RGB || png_color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
		png_set_bgr(png_ptr);
	}
#endif
	else if (png_color_type == PNG_COLOR_TYPE_GRAY || png_color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
		if (png_bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
		png_set_gray_to_rgb(png_ptr);
	}

	if ( (rc = allocMemAndReadImage()) ) return rc;
	rc = copyPngToPixmap(pix);
	return rc;
}

#if OS_WIN32GUI

static INT topix4(PIXMAP *pix) {
	INT rc;
	UCHAR *palette = guiAllocMem(16 * sizeof(RGBQUAD));
	CHAR work[64];

	if (palette == NULL) {
		sprintf(work, "guiAllocMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	guiaSetDefaultPalette(16, (RGBQUAD*)palette);
	packPalette(palette, 16);
	if (png_color_type == PNG_COLOR_TYPE_GRAY || png_color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
		if (png_bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
		png_set_gray_to_rgb(png_ptr);
		png_set_quantize(png_ptr, (png_colorp) palette, 16, 16, NULL, 1);
	}
	else if (png_color_type == PNG_COLOR_TYPE_RGB || png_color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
		png_set_quantize(png_ptr, (png_colorp) palette, 16, 16, NULL, 1);
	}
	else if (png_color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png_ptr);
		png_set_quantize(png_ptr, (png_colorp) palette, 16, 16, NULL, 1);
	}
	guiFreeMem(palette);
	if (rc = allocMemAndReadImage()) return rc;
	return copyPngToPixmap_to_4(pix);
}

static INT topix8(PIXMAP *pix) {
	INT rc;
	UCHAR *palette = guiAllocMem(256 * sizeof(RGBQUAD));
	CHAR work[64];

	if (palette == NULL) {
		sprintf(work, "guiAllocMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	guiaSetDefaultPalette(256, (RGBQUAD*)palette);
	packPalette(palette, 256);
	if (png_color_type == PNG_COLOR_TYPE_GRAY || png_color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
		if (png_bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
		png_set_gray_to_rgb(png_ptr);
		png_set_quantize(png_ptr, (png_colorp) palette, 256, 256, NULL, 1);
	}
	else if (png_color_type == PNG_COLOR_TYPE_RGB || png_color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
		png_set_quantize(png_ptr, (png_colorp) palette, 256, 256, NULL, 1);
	}
	else if (png_color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png_ptr);
		png_set_quantize(png_ptr, (png_colorp) palette, 256, 256, NULL, 1);
	}
	guiFreeMem(palette);
	if (rc = allocMemAndReadImage()) return rc;
	return copyPngToPixmap(pix);
}

#endif


/**
 * Allocates memory for the raw PNG image data and populates it.
 * Sets srcrowbytes.
 */
static INT allocMemAndReadImage() {
	UINT i1;

	png_read_update_info(png_ptr, info_ptr);
	srcrowbytes = (png_uint_32) png_get_rowbytes(png_ptr, info_ptr);
	rowpointers = png_malloc(png_ptr, png_height * sizeof(png_bytep));
	if (rowpointers == NULL) return RC_NO_MEM;
	for (i1 = 0; i1 < png_height; i1++) {
		rowpointers[i1] = png_malloc(png_ptr, srcrowbytes);
		if (rowpointers[i1] == NULL) return RC_NO_MEM;
	}
	png_read_image(png_ptr, rowpointers);
	png_read_end(png_ptr, NULL);
	return 0;
}

/**
 * Copy the raw PNG data to a pixmap.
 * This routine assumes that the PNG data format and the PIXMAP data formats are identical.
 * It copies rows using memcpy.
 */
static INT copyPngToPixmap(PIXMAP *pix) {
	INT rc = 0;
	UINT i1;
	UINT rowbytes = min(destrowbytes, srcrowbytes);
#if OS_WIN32GUI
	CHAR work[64];
#endif

#if OS_WIN32GUI
	tbufsize = destrowbytes * pix_height;
	tempbuf = guiAllocMem(tbufsize);
	if (tempbuf == NULL) {
		sprintf_s(work, ARRAYSIZE(work), "guiAllocMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, tbufsize);
		return 0;
	}
	ZeroMemory(tempbuf, tbufsize);
	dest = tempbuf + (destrowbytes * (pix_height - 1));
	for (i1 = 0; i1 < rowmax; i1++) {
		source = rowpointers[i1];
		memcpy(dest, source, rowbytes);
		dest -= destrowbytes;
	}
	/*
	 * If SetDIBits fails, it returns zero, else it returns number of scan lines copied.
	 */
	rc = SetDIBits(hDC, pix->hbitmap, 0, pix_height, tempbuf, pix->pbmi, DIB_RGB_COLORS);
	guiFreeMem(tempbuf);
#else
	for (i1 = 0, dest = *pix->pixbuf; i1 < rowmax; i1++) {
		source = rowpointers[i1];
		memcpy(dest, source, rowbytes);
		dest += destrowbytes;
	}
#endif
	return rc;
}

/**
 * The PNG data is one byte per pixel. Each byte is compared to the bwcutoff argument.
 * If it is larger, then the pixel is white, else it is black.
 * We are copying this to a 1bpp image
 */
static INT copyPngToPixmap_to_1(PIXMAP *pix, png_byte bwcutoff) {
	INT rc = 0, i3;
	UINT i1, i2;
	UCHAR *destptr;
	UINT maxcolumns = min(pix_width, png_width);
#if OS_WIN32GUI
	CHAR work[64];
#endif

#if OS_WIN32GUI
	tbufsize = destrowbytes * pix_height;
	tempbuf = guiAllocMem(tbufsize);
	if (tempbuf == NULL) {
		sprintf(work, "guiAllocMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	ZeroMemory(tempbuf, tbufsize);
	dest = tempbuf + (destrowbytes * (pix_height - 1));
#else
	dest = *pix->pixbuf;
#endif
	for (i1 = 0; i1 < rowmax; i1++) {
		source = rowpointers[i1];
		memset(dest, 0, destrowbytes);
		destptr = dest;
		for (i2 = 0, i3 = 7; i2 < maxcolumns; i2++) {
			if (*(source + i2) > bwcutoff) *destptr |= 1 << i3;
			if (--i3 < 0) {
				i3 = 7;
				destptr++;
			}
		}
#if OS_WIN32GUI
		dest -= destrowbytes;
#else
		dest += destrowbytes;
#endif
	}
#if OS_WIN32GUI
	rc = SetDIBits(hDC, pix->hbitmap, 0, pix_height, tempbuf, pix->pbmi, DIB_RGB_COLORS);
	guiFreeMem(tempbuf);
#endif
	return rc;
}

#if OS_WIN32GUI
/**
 * Convert a palette with 32 bit entries to one with 24 bit entries.
 * Throw away the fourth byte. We also have palettes in windoze form of BGR, change to RGB.
 */
static void packPalette(UCHAR *palette, INT numentries) {
	UCHAR *sp, *dp;
	UCHAR r, g, b;
	INT i1;
	dp = palette;
	sp = palette;
	for (i1 = 0; i1 < numentries; i1++) {
		r = *(sp + 2);
		g = *(sp + 1);
		b = *sp;
		*dp++ = r;
		*dp++ = g;
		*dp++ = b;
		sp += 4;
	}
}

static INT copyPngToPixmap_to_4(PIXMAP *pix) {
	INT rc, i3;
	UINT i1, i2;
	UINT maxcolumns = min(pix_width, png_width);
	UCHAR *destptr;
	CHAR work[64];

	tbufsize = destrowbytes * pix_height;
	tempbuf = guiAllocMem(tbufsize);
	if (tempbuf == NULL) {
		sprintf(work, "guiAllocMem returned Null in %s", __FUNCTION__);
		guiaErrorMsg(work, 0);
		return 0;
	}
	ZeroMemory(tempbuf, tbufsize);
	dest = tempbuf + (destrowbytes * (pix_height - 1));
	for (i1 = 0; i1 < rowmax; i1++) {
		source = rowpointers[i1];
		memset(dest, 0, destrowbytes);
		destptr = dest;
		for (i2 = 0, i3 = 1; i2 < maxcolumns; i2++) {
			if (i3) {
				*destptr |= (*(source + i2)) << 4;
				i3 = 0;
			}
			else {
				*destptr |= *(source + i2);
				i3 = 1;
				destptr++;
			}
		}
		dest -= destrowbytes;
	}
	rc = SetDIBits(hDC, pix->hbitmap, 0, pix_height, tempbuf, pix->pbmi, DIB_RGB_COLORS);
	guiFreeMem(tempbuf);
	return rc;
}

#endif
