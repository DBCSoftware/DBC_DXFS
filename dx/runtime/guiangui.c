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
#define INC_TIME
#include "includes.h"
#include "base.h"
#include "gui.h"
#include "guix.h"

static PIXMAP *drawpixptr;

/*
 * start a drawing command sequence
 */
void guiaDrawStart(PIXHANDLE ppixDraw)
{
	drawpixptr = *ppixDraw;
}

/*
 * end a drawing command sequence
 */
void guiaDrawEnd()
{
	drawpixptr = NULL;
}

/*
 * draw one pixel
 */
void guiaSetPixel()
{
	INT h, v, bpp, hsize, scansize;
	INT32 color;
	UCHAR *pix;
	
	h = ZPOINTH(drawpixptr->pos1);
	v = ZPOINTV(drawpixptr->pos1);
	hsize = drawpixptr->hsize;
	bpp = drawpixptr->bpp;
	color = drawpixptr->pencolor;
	pix = *drawpixptr->pixbuf;

	if (bpp == 1) {
		scansize =( (hsize + 0x1F) & ~0x1F) >> 3;		/* each row is a multiple of 4 bytes */
		pix += v * scansize;							/* point at the row */
		pix += h >> 3;									/* point to the byte */
		h &= 0x07;										/* h now indicates the bit */
		if (color == 0) *pix &= ~(0x80 >> h);			/* force the bit to zero (black) */
		else *pix |= (0x80 >> h);						/* force the bit to one (white) */
	}
}

/*
 * draw a rectangle
 */
void guiaDrawRect()
{
	ZRECT rect;
	INT bpp, hsize, vsize, scansize, h;
	INT32 color, h1;
	UCHAR *pix, *startrow, *endrow, *rowptr, *firstbyte, *lastbyte, *colptr, *ptr;
	
	hsize = drawpixptr->hsize;
	vsize = drawpixptr->vsize;
	bpp = drawpixptr->bpp;
	color = drawpixptr->pencolor;
	pix = *drawpixptr->pixbuf;

	rect.top = ZPOINTV(drawpixptr->pos1);
	rect.left = ZPOINTH(drawpixptr->pos1);
	rect.bottom = ZPOINTV(drawpixptr->pos2);
	rect.right = ZPOINTH(drawpixptr->pos2);

	/* make sure the rectangle is within the image bounds */
	if (rect.top < 0) rect.top = 0;
	if (rect.bottom >= vsize) rect.bottom = vsize - 1;
	if (rect.left < 0) rect.left = 0;
	if (rect.right >= hsize) rect.right = hsize - 1;
#if !defined(Linux) && !defined(FREEBSD)
#pragma warning(disable : 4554)
#endif
	
	scansize = ((hsize * bpp + 0x1F) & ~0x1F) >> 3;		/* each row is a multiple of 4 bytes */
	startrow = pix + (rect.top * scansize);
	endrow = pix + (rect.bottom * scansize);
	if (bpp == 1) {
		for (rowptr = startrow; rowptr <= endrow; rowptr += scansize) {
			firstbyte = rowptr + (rect.left >> 3);					/* point to the first byte */
			lastbyte = rowptr + (rect.right >> 3);					/* point to the last byte */
			for (colptr = firstbyte; colptr <= lastbyte; colptr++) {
				if (colptr == firstbyte) 
					for (h = rect.left & 0x07; h<8; h++)
						if (color == 0) *colptr &= ~(0x80 >> h);					/* force the bit to zero (black) */
						else *colptr |= (0x80 >> h);								/* force the bit to one (white) */
				else if (colptr == lastbyte)
					for (h = 0; h <= (rect.right & 0x07); h++)
						if (color == 0) *colptr &= ~(0x80 >> h);					/* force the bit to zero (black) */
						else *colptr |= (0x80 >> h);								/* force the bit to one (white) */
				else {
						if (color == 0) *colptr = 0x00;
						else *colptr = 0xFF;
				}
			}
		}
	}
	else {
		UCHAR blue = (color & 0x00FF0000) >> 16;
		UCHAR green = (color & 0x0000FF00) >> 8;
		UCHAR red = color & 0x000000FF;
		for (rowptr = startrow; rowptr <= endrow; rowptr += scansize) {
			ptr = rowptr + (rect.left * 3);					/* point to the first byte */
			for (h1 = rect.left; h1 <= rect.right; h1++) {
				*ptr++ = red;
				*ptr++ = green;
				*ptr++ = blue;
			}
		}
	}
}
