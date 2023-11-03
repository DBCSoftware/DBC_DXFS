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
#define INC_STDIO
#define INC_TIME
#include "includes.h"
#include "gui.h"
#include "guix.h"
#include "base.h"

/* GIF BPP */
INT guiaGifBPP(UCHAR *buffer, INT *x)
{
	return 0;
}

/* GIF SIZE */
INT guiaGifSize(UCHAR *buffer, INT *x, INT *y)
{
	return 0;
}

/* GUIAGIFTOPIXMAP */
INT  guiaGifToPixmap(PIXHANDLE pix, UCHAR **buffer)
{
	return 0;
}

/* JPG BPP */
INT guiaJpgBPP(UCHAR *buffer, INT *x)
{
	return 0;
}

/* JPG Resolution */
INT guiaJpgRes(UCHAR *buffer, INT *xres, INT *yres)
{
	return 0;
}

/* JPG SIZE */
INT guiaJpgSize(UCHAR *buffer, INT *x, INT *y)
{
	return 0;
}

/* GUIAJPGTOPIXMAP */
INT guiaJpgToPixmap(PIXHANDLE pix, UCHAR **buffer)
{
	return 0;
}

/* GUIAPIXMAPTOTIF */
INT guiaPixmapToTif(PIXMAP *pix, UCHAR *buffer, INT *pbufsize)
{
	return 0;
}

/* TIFF BPP */
INT guiaTifBPP(INT imgnum, UCHAR *buffer, INT *x)
{
	return 0;
}

/* TIFF IMAGE COUNT */
INT guiaTifImageCount(UCHAR *buffer, INT *x)
{
	return 0;
}

/* TIFF Resolution */
INT guiaTifRes(INT imgnum, UCHAR *buffer, INT *xres, INT *yres)
{
	return 0;
}

/* TIFF SIZE */
INT guiaTifSize(INT imgnum, UCHAR *buffer, INT *x, INT *y)
{
	return 0;
}

/* GUIATIFTOPIXMAP */
INT guiaTifToPixmap(PIXHANDLE pix, UCHAR **buffer)
{
	return 0;
}

void guiaTifMaxStripSize(CHAR *ptr) {
}
