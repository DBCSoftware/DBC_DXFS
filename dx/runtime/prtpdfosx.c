
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
#define PRTPDFOSX_C_

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

#define ARRAYSIZE(A) (sizeof(A)/sizeof((A)[0]))

static CTFontRef currentCTFont;
static CGFloat GetLineHeightForFont(CTFontRef iFont);

static size_t PutBytes(void *info, const void *buffer, size_t count) {
	prtbufcopy((INT)(intptr_t)info, buffer, count);
	return count;
}

static void releasePutBytes(void *info) {
	/* do nothing */
}

INT prtpdfinit(INT prtnum)
{
	PRINTINFO *print;

	CFMutableDictionaryRef pdfDict = NULL;
	CGRect pageRect;
	CGDataConsumerCallbacks callBacks;

	prtSetPdfSize(prtnum);
	print = *printtabptr + prtnum;
	/* Set the scaling ratio, it does not change. We use it at every page start */
	print->fmt.pdf.scaleX = ((DOUBLE) (print->fmt.pdf.width) / (DOUBLE) print->fmt.pdf.gwidth); /* scale x */
	print->fmt.pdf.scaleY = ((DOUBLE) (print->fmt.pdf.height) / (DOUBLE) print->fmt.pdf.gheight); /* scale y */
	pdfDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(pdfDict, kCGPDFContextCreator, CFSTR(RELEASEPROGRAM RELEASE));
	pageRect.size.width = print->fmt.pdf.width;
	pageRect.size.height = print->fmt.pdf.height;

	callBacks.putBytes = PutBytes;
	callBacks.releaseConsumer = releasePutBytes;
	print->fmt.pdf.dataConsumer = CGDataConsumerCreate((void*)(size_t)prtnum, &callBacks);
	print->fmt.pdf.pdfContext = CGPDFContextCreate(print->fmt.pdf.dataConsumer, &pageRect, pdfDict);

	print->fmt.pdf.cgColor = CGColorCreateGenericRGB(0.0, 0.0, 0.0, 1.0);
	CGContextSetFillColorWithColor(print->fmt.pdf.pdfContext, print->fmt.pdf.cgColor);
	CGContextSetStrokeColorWithColor(print->fmt.pdf.pdfContext, print->fmt.pdf.cgColor);
	CFRelease(pdfDict);
	prtfont(prtnum + 1, "Courier(12,PLAIN)");
	print->fmt.pdf.lineWidth = 1.0;
	print->fmt.pdf.imagecount = 0; /* This will remain zero, just want to make sure */
	print->fmt.pdf.imgobjcount = 0; /* Count of CGImage objects in our cache */
	print->fmt.pdf.imgobjsize = 0; /* How much is allocated in the cache array */
	print->fmt.pdf.imgobjs = NULL;
	print->flags |= PRTFLAG_INIT;
	prtpdfpagestart(prtnum);
	return 0;
}

/*
 * prtnum is zero-based
 * Always returns zero
 */
INT prtpdfend(INT prtnum)
{
	PRINTINFO *print;
	INT i1;

	prtpdfpageend(prtnum);
	print = *printtabptr + prtnum;
	CGContextRelease(print->fmt.pdf.pdfContext);
	CGColorRelease(print->fmt.pdf.cgColor);
	CGDataConsumerRelease(print->fmt.pdf.dataConsumer);
	if (print->fmt.pdf.imgobjs != NULL) {
		for (i1 = 0; i1 < print->fmt.pdf.imgobjcount; i1++) {
			CGImageRef imgRef = (*print->fmt.pdf.imgobjs)[i1].imageRef;
			CGImageRelease(imgRef);
		}
		memfree((UCHAR **) print->fmt.pdf.imgobjs);
	}
	return 0;
}

static CTLineRef GetCTLine(CFStringRef iString) {
	CTLineRef line;
	CFAttributedStringRef aString;
	CFStringRef keys[] = { kCTFontAttributeName };
	CFTypeRef values[] = { currentCTFont };
	CFDictionaryRef attributes =
			CFDictionaryCreate(kCFAllocatorDefault, (const void**)&keys,
					(const void**)&values, ARRAYSIZE(keys),
					&kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	aString = CFAttributedStringCreate(kCFAllocatorDefault, iString, attributes);
	CFRelease(attributes);
	line = CTLineCreateWithAttributedString(aString);
	CFRelease(aString);
	return line;
}

/**
 * Used for no-center no-rightjustify no-angle text printing
 */
static void textPrintNormal(PRINTINFO *print, CFStringRef string, CGFloat posx, CGFloat posy)
{
	CTLineRef line = GetCTLine(string);
	CGRect rect = CTLineGetImageBounds(line, print->fmt.pdf.pdfContext);
	CGContextSetTextPosition(print->fmt.pdf.pdfContext, posx, posy);
	if (line == NULL) line = GetCTLine(string);
	CTLineDraw(line, print->fmt.pdf.pdfContext);
	posx += rect.size.width;
	print->fmt.pdf.posx = posx / print->fmt.pdf.scaleX;
	CFRelease(line);
}

INT prtpdfosxtext(INT prtnum, UCHAR *string, UCHAR chr, INT len, INT blanks, INT type, INT value, INT angle)
{
	PRINTINFO *print;
	INT len1;
	CGFloat posx, posy;
	CGFloat scaleY;
	CGPoint textPosition;
	CFStringRef string1;
	CTLineRef line = NULL;
	UCHAR **dummy;

	print = *printtabptr + prtnum;
	scaleY = ((DOUBLE) (print->fmt.pdf.gheight) / (DOUBLE) print->fmt.pdf.height);
	/*
	 * DB/C programmers expect that the current position is at the upper left of the text character cell.
	 * But the CG call uses the lower left. We need to move the text down the page by the height
	 * of the font. (positive y direction)
	 */
	posy = (print->fmt.pdf.posy * print->fmt.pdf.scaleY) + print->fontheight;

	/*
	 * Figure the length including any blanks
	 */
	len1 = len + blanks;
	dummy = memalloc(len1 + 1, 0);
	if (string != NULL) memcpy(*dummy, string, len);
	else memset(*dummy, chr, len);
	if (blanks) memset(*dummy + len, ' ', blanks);
	(*dummy)[len1] = '\0';
	/*
	 * Make a CFString and free the dummy C string
	 */
	string1 = CFStringCreateWithCString (kCFAllocatorDefault, (const char *)*dummy, kCFStringEncodingMacRoman);
	memfree((void*)dummy);

	posx = print->fmt.pdf.posx * print->fmt.pdf.scaleX;

	if (type == 0) {  // 'Normal' printing
		textPrintNormal(print, string1, posx, posy);
		CFRelease(string1);
		return 0;
	}

	if (type & PRT_TEXT_RIGHT /* *rj */) {
		line = GetCTLine(string1);
		CGRect rect = CTLineGetImageBounds(line, print->fmt.pdf.pdfContext);
		posx -= rect.size.width;
	}
	else if (type & PRT_TEXT_CENTER /* *cj */) {
		line = GetCTLine(string1);
		CGRect rect = CTLineGetImageBounds(line, print->fmt.pdf.pdfContext);
		posx += ((value * print->fmt.pdf.scaleX) - rect.size.width) / 2;
	}
	if (type & PRT_TEXT_ANGLE) {
		DOUBLE radians;
		CGAffineTransform fontMatrix = CTFontGetMatrix(currentCTFont);
		CGGlyph **glyphs;
		UniChar **characters;
		size_t count = CFStringGetLength(string1);
		characters = (UniChar **)memalloc(sizeof(UniChar) * count, 0);
		glyphs = (CGGlyph **)memalloc(sizeof(CGGlyph) * count, 0);
		CFStringGetCharacters(string1, CFRangeMake(0, count), *characters);
		CTFontGetGlyphsForCharacters(currentCTFont, *characters, *glyphs, count);
		/* convert to east = 0 (keep clockwise rotation) */
		if ((angle -= 90) < 0) angle += 360;
		radians = ((DOUBLE) angle * (DOUBLE) M_PI) / (DOUBLE) 180;
		CGContextSaveGState(print->fmt.pdf.pdfContext);
		CGContextSetFont(print->fmt.pdf.pdfContext, CTFontCopyGraphicsFont(currentCTFont, NULL));
		CGContextSetFontSize(print->fmt.pdf.pdfContext, print->fontheight);
		CGContextSetTextMatrix(print->fmt.pdf.pdfContext,
				CGAffineTransformRotate(fontMatrix, -radians));
		CGContextShowGlyphsAtPoint (print->fmt.pdf.pdfContext, posx, posy - print->fontheight,
				*glyphs, count);
		memfree((void*)characters);
		memfree((void*)glyphs);
		CGContextSetTextMatrix(print->fmt.pdf.pdfContext, CGAffineTransformIdentity);
		CGContextRestoreGState(print->fmt.pdf.pdfContext);
	}
	if (!(type & PRT_TEXT_ANGLE)) {
		CGContextSetTextPosition(print->fmt.pdf.pdfContext, posx, posy);
		if (line == NULL) line = GetCTLine(string1);
		CTLineDraw(line, print->fmt.pdf.pdfContext);
	}
	if (line != NULL) CFRelease(line);
	CFRelease(string1);

	//Advance our position to the end of the text just drawn
	textPosition = CGContextGetTextPosition(print->fmt.pdf.pdfContext);
	print->fmt.pdf.posx = textPosition.x / print->fmt.pdf.scaleX;
	return 0;
}

INT prtpdfosxlf(INT prtnum, INT cnt) {
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	//print->fmt.pdf.posy += print->fontheight / print->fmt.pdf.scaleY;   // font height in points scaled up to user DPI
	print->fmt.pdf.posy += 0.0 - (GetLineHeightForFont(currentCTFont) / print->fmt.pdf.scaleY);
	return 0;
}

INT prtpdffont(INT prtnum, CHAR *name, INT bold, INT italic, INT pointsize)
{
	CTFontSymbolicTraits iTraits = 0;
	CFNumberRef symTraits;
	CGAffineTransform fontMatrix = {1., 0., 0., -1., 0., 0.};
	PRINTINFO *print;
	CTFontDescriptorRef fontDescriptor;
	CFDictionaryRef fontAttributes, fontTraits;
	CFStringRef fontFamilyName;
	CFStringRef attributeKeys[] = { kCTFontFamilyNameAttribute, kCTFontTraitsAttribute };
	CFTypeRef attributeValues[2];
	CFStringRef traitKeys[] = { kCTFontSymbolicTrait };
	CFTypeRef traitValues[1];

	fontFamilyName = CFStringCreateWithCString (kCFAllocatorDefault, name, kCFStringEncodingMacRoman);
	attributeValues[0] = fontFamilyName;
	if (bold) iTraits |= kCTFontBoldTrait;
	if (italic) iTraits |= kCTFontItalicTrait;
	symTraits = CFNumberCreate(NULL, kCFNumberSInt32Type, &iTraits);
	traitValues[0] = symTraits;
	fontTraits = CFDictionaryCreate(kCFAllocatorDefault, (const void**)&traitKeys,
			(const void**)&traitValues, ARRAYSIZE(traitKeys),
			&kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	attributeValues[1] = fontTraits;

	fontAttributes = CFDictionaryCreate(kCFAllocatorDefault, (const void**)&attributeKeys,
							(const void**)&attributeValues, ARRAYSIZE(attributeKeys),
							&kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	fontDescriptor = CTFontDescriptorCreateWithAttributes(fontAttributes);

	print = *printtabptr + prtnum;
	print->fontheight = pointsize;
	if (currentCTFont != NULL) CFRelease(currentCTFont);
	currentCTFont = CTFontCreateWithFontDescriptor(fontDescriptor, pointsize, &fontMatrix);
	CFRelease(fontFamilyName);
	CFRelease(fontAttributes);
	CFRelease(fontDescriptor);
	CFRelease(fontTraits);
	CFRelease(symTraits);
	return 0;
}

/**
 * prtnum zero-based index into printtabptr
 */
INT prtpdfline(INT prtnum, INT xpos, INT ypos) {
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	CGContextSaveGState(print->fmt.pdf.pdfContext);
	CGContextScaleCTM(print->fmt.pdf.pdfContext, print->fmt.pdf.scaleX, print->fmt.pdf.scaleY);
	CGContextBeginPath(print->fmt.pdf.pdfContext);
	CGContextMoveToPoint(print->fmt.pdf.pdfContext, print->fmt.pdf.posx, print->fmt.pdf.posy);
	CGContextAddLineToPoint(print->fmt.pdf.pdfContext, xpos, ypos);
	CGContextStrokePath(print->fmt.pdf.pdfContext);
	CGContextRestoreGState(print->fmt.pdf.pdfContext);

	print->fmt.pdf.posx = xpos;
	print->fmt.pdf.posy = ypos;
	return 0;
}

/**
 * Expects pos to be zero-based
 */
INT prtpdftab(INT prtnum, INT pos, UINT oldTabbing)
{
	PRINTINFO *print;
	ULONG isMono;
	int i1;
	WCHAR strA = L'A';
	UniChar **sample;		// 'A'

	*prterrorstring = '\0';
	if (pos < 0) return 0;

	print = *printtabptr + prtnum;
	if (pos == 0) {
		print->fmt.pdf.posx = (DOUBLE) 0.0;
		return 0;
	}
	/* Determine if the font is monospaced */
	CTFontSymbolicTraits fst = CTFontGetSymbolicTraits(currentCTFont);
	isMono = !((fst & kCTFontMonoSpaceTrait) == 0);

	if (isMono){
		sample = (UniChar**)memalloc(pos * sizeof(UniChar), 0);
		for (i1 = 0; i1 < pos; i1++) (*sample)[i1] = strA;
		CFStringRef bstring = CFStringCreateWithCharacters (NULL, *sample, (CFIndex)pos);
		CTLineRef line = GetCTLine(bstring);
		CGRect rect = CTLineGetImageBounds(line, print->fmt.pdf.pdfContext);
		print->fmt.pdf.posx = (DOUBLE) rect.size.width / print->fmt.pdf.scaleX;
		CFRelease(line);
		CFRelease(bstring);
		memfree((UCHAR**)sample);
	}
	else {
		print->fmt.pdf.posx = (DOUBLE) (pos * ((7 * print->fontheight) / 12)) / print->fmt.pdf.scaleX;
	}
	return 0;
}

/**
 * prtnum The ZERO-Based index into printtabptr
 */
INT prtpdfpos(INT prtnum, INT xpos, INT ypos) {
	PRINTINFO *print;

	*prterrorstring = '\0';
	print = *printtabptr + prtnum;
	/* In OSX-PDF world, we keep the current position in db/c user coordinates */
	print->fmt.pdf.posx = xpos;
	print->fmt.pdf.posy = ypos;
	return 0;
}

INT prtpdfpagestart(INT prtnum)
{
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	CGPDFContextBeginPage(print->fmt.pdf.pdfContext, NULL);
	CGContextSetLineWidth(print->fmt.pdf.pdfContext, (print->fmt.pdf.lineWidth * print->dpi) / PDFUSERSPACEUNITS);

	/* We have to set the CTM at start of every page */

	/* Move the origin from the lower left to the upper left */
	CGContextTranslateCTM(print->fmt.pdf.pdfContext, 0, print->fmt.pdf.height);
	/* 'Flip' the Y axis */
	CGContextScaleCTM(print->fmt.pdf.pdfContext, 1, -1);

	CGContextSetFillColorWithColor(print->fmt.pdf.pdfContext, print->fmt.pdf.cgColor);
	CGContextSetStrokeColorWithColor(print->fmt.pdf.pdfContext, print->fmt.pdf.cgColor);
	return 0;
}

INT prtpdfpageend(INT prtnum)
{
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	CGPDFContextEndPage(print->fmt.pdf.pdfContext);
	return 0;
}

static void printImage(PRINTINFO *print, CGRect rect, CGImageRef image) {
	CGContextSaveGState(print->fmt.pdf.pdfContext);
	/* 'un-flip' the Y coord and scale at the same time*/
	CGContextScaleCTM(print->fmt.pdf.pdfContext, print->fmt.pdf.scaleX, -print->fmt.pdf.scaleY);
	/* Adjust the Y position for un-flipped */
	rect.origin.y = 0 - rect.origin.y - rect.size.height;
	CGContextDrawImage(print->fmt.pdf.pdfContext, rect, image);
	CGContextRestoreGState(print->fmt.pdf.pdfContext);
}

/*
 * The central function here is
 * CGContextDrawImage(CGContextRef c, CGRect rect, CGImageRef image)
 *
 * The CGImageRef will be created by a call to CGImageCreate(
 *     size_t width,
	   size_t height,
	   size_t bitsPerComponent,
	   size_t bitsPerPixel,
	   size_t bytesPerRow,
	   CGColorSpaceRef colorspace,
	   CGBitmapInfo bitmapInfo,
	   CGDataProviderRef provider,
	   const CGFloat decode[],
	   bool shouldInterpolate,
	   CGColorRenderingIntent intent)
 *
 * I think that to make the provider I will use CGDataProviderCreateWithCFData
 * (CFDataRef data)
 *
 * We need to use a CFData object to take advantage of OSX memory. We could cache our own bitmap
 * but that would eat a lot of memalloc.
 *
 * Our cache will use an md5 on the pixmap data as the key.
 * We will need to release the image at the end of PDF production
 *
 */
INT prtpdfimage(INT prtnum, void *pixhandle)
{
	PRINTINFO *print;
	PIXMAP *pix;
	md5_state_t md5state;
	md5_byte_t this_digest[16];
	UINT bytesPerRow;
	INT i1, i2, bcount, /*pixbufsize,*/ objIndex;
	INT32 color;
	UCHAR *ptrend, *scanline;
	CGRect rect;
	CGImageRef image;
	CFDataRef mdata;
	CGDataProviderRef provider;
	CGColorSpaceRef cs;

	i1 = prtSetPdfImageCacheSize(prtnum);
	if (i1) return i1;
	print = *printtabptr + prtnum;
	pix = *((PIXHANDLE)pixhandle);
	rect.origin.x = print->fmt.pdf.posx;
	rect.origin.y = print->fmt.pdf.posy;
	rect.size.width = pix->hsize;
	rect.size.height = pix->vsize;

	/* Get the md5 of the *pix->pixbuf */
	dbc_md5_init(&md5state);
	dbc_md5_append(&md5state, *pix->pixbuf, pix->bufsize);
	dbc_md5_finish(&md5state, this_digest);

	if (print->fmt.pdf.imgobjcount) {		/* There are already images in use, see if the md5 matches one */
		INT found = -1;
		for (i1 = 0; i1 < print->fmt.pdf.imgobjcount; i1++) {
			if (memcmp(this_digest, (*print->fmt.pdf.imgobjs)[i1].digest, 16) == 0) {
				found = i1;
				break;
			}
		}
		if (found != -1) { /* We are basically done, print the image and quit */
			printImage(print, rect, (*print->fmt.pdf.imgobjs)[found].imageRef);
			return 0;
		}
	}
	objIndex = print->fmt.pdf.imgobjcount;
	memcpy((*print->fmt.pdf.imgobjs)[objIndex].digest, this_digest, 16);

	mdata = CFDataCreate(NULL, *pix->pixbuf, pix->bufsize);
	provider = CGDataProviderCreateWithCFData(mdata);
	CFRelease(mdata);

	if (pix->bpp == 1) {
		cs = CGColorSpaceCreateWithName(kCGColorSpaceGenericGray);
		bytesPerRow = (pix->hsize + 0x07 & ~0x07) >> 3;
		image = CGImageCreate(pix->hsize, pix->vsize, 1, 1, bytesPerRow, cs,
				0, provider, NULL, TRUE, kCGRenderingIntentDefault);
	}
	else {
		cs = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
		bytesPerRow = (pix->hsize * pix->bpp + 0x1F & ~0x1F) >> 3;
		image = CGImageCreate(pix->hsize, pix->vsize, 8, 24, bytesPerRow, cs,
				0, provider, NULL, TRUE, kCGRenderingIntentDefault);
	}
	CGColorSpaceRelease(cs);
	CGDataProviderRelease(provider);
	printImage(print, rect, image);
	(*print->fmt.pdf.imgobjs)[objIndex].imageRef = image;
	print->fmt.pdf.imgobjcount++;
	return 0;
}

/**
 * The current position is one corner, the arguments
 * are the diagonally opposite corner.
 */
INT prtpdfrect(INT prtnum, INT x, INT y, INT fill)
{
	PRINTINFO *print = *printtabptr + prtnum;
	CGRect rect;
	/* We need to find the coordinates of the upper left corner */
	INT ulx = (x > print->fmt.pdf.posx) ? print->fmt.pdf.posx : x;
	INT uly = (y > print->fmt.pdf.posy) ? print->fmt.pdf.posy : y;

	rect = CGRectMake(ulx, uly, fabs(x - print->fmt.pdf.posx), fabs(y - print->fmt.pdf.posy));
	CGContextSaveGState(print->fmt.pdf.pdfContext);
	CGContextScaleCTM(print->fmt.pdf.pdfContext, print->fmt.pdf.scaleX, print->fmt.pdf.scaleY);
	if (fill) CGContextFillRect(print->fmt.pdf.pdfContext, rect);
	else CGContextStrokeRect(print->fmt.pdf.pdfContext, rect);
	CGContextRestoreGState(print->fmt.pdf.pdfContext);
	return 0;
}

/*
 * The current position is one apex, the arguments are the other two.
 */
INT prtpdftriangle(INT prtnum, INT x1, INT y1, INT x2, INT y2)
{
	PRINTINFO *print = *printtabptr + prtnum;
	CGPoint points[] = {CGPointMake(print->fmt.pdf.posx, print->fmt.pdf.posy),
			CGPointMake(x1, y1), CGPointMake(x2, y2)};

	CGContextSaveGState(print->fmt.pdf.pdfContext);
	CGContextScaleCTM(print->fmt.pdf.pdfContext, print->fmt.pdf.scaleX, print->fmt.pdf.scaleY);
	CGContextBeginPath(print->fmt.pdf.pdfContext);
	CGContextAddLines(print->fmt.pdf.pdfContext, points, 3);
	CGContextFillPath(print->fmt.pdf.pdfContext);
	CGContextRestoreGState(print->fmt.pdf.pdfContext);
	return 0;
}

/*
 * The center of the circle is at the current position
 * 'r' is the radius
 *
 */
INT prtpdfcircle(INT prtnum, INT r, INT fill)
{
	PRINTINFO *print = *printtabptr + prtnum;
	CGRect rect = CGRectMake(print->fmt.pdf.posx - r, print->fmt.pdf.posy - r,
			2.0 * r, 2.0 * r);
	CGContextSaveGState(print->fmt.pdf.pdfContext);
	CGContextScaleCTM(print->fmt.pdf.pdfContext, print->fmt.pdf.scaleX, print->fmt.pdf.scaleY);
	if (fill) CGContextFillEllipseInRect(print->fmt.pdf.pdfContext, rect);
	else CGContextStrokeEllipseInRect(print->fmt.pdf.pdfContext, rect);
	CGContextRestoreGState(print->fmt.pdf.pdfContext);
	return 0;
}

/*
 * prtnum is zero-based index into printtabptr
 * color is 0x00RRGGBB
 */
INT prtpdfcolor(INT prtnum, INT color)
{
	PRINTINFO *print = *printtabptr + prtnum;
	CGFloat red, green, blue;

	/* Save it for future use, need to set colors at each page start */
	CGColorRelease(print->fmt.pdf.cgColor);
	red = (CGFloat)((DOUBLE)((color & 0xFF0000) >> 16) / 255.0);
	green = (CGFloat)((DOUBLE)((color & 0x00FF00) >> 8) / 255.0);
	blue = (CGFloat)((DOUBLE)(color & 0x0000FF) / 255.0);
	print->fmt.pdf.cgColor = CGColorCreateGenericRGB(red, green, blue, 1.0);
	CGContextSetFillColorWithColor(print->fmt.pdf.pdfContext, print->fmt.pdf.cgColor);
	CGContextSetStrokeColorWithColor(print->fmt.pdf.pdfContext, print->fmt.pdf.cgColor);
	return 0;
}

INT prtflushlinepdf(INT prtnum, INT linecnt)
{
	return 0;
}

/*
 * A PDF needs linewidth in user space units, 1/72 inch
 * A DB/C programmer expresses it in pixels. Typically these are 1/300 inch (or 300 DPI)
 * Convert.
 */
INT prtpdflinewidth(INT prtnum, INT width) {
	PRINTINFO *print = *printtabptr + prtnum;

	print->fmt.pdf.lineWidth = width;
	CGContextSetLineWidth(print->fmt.pdf.pdfContext, (print->fmt.pdf.lineWidth * print->dpi) / PDFUSERSPACEUNITS);
	//	INT i1;
	//	i1 = prtdbltoa((DOUBLE) (width * PDFUSERSPACEUNITS) / (DOUBLE) print->dpi, work, 2);
	//	memcpy(work + i1, " w\n", 3);
	//	i1 = prtbufcopy(prtnum, work, i1 + 3);
	//	if (i1) return i1;
	return 0;
}

/**
 * Because our CTFont has a transform matrix in it
 * that flips the y axis, this function returns
 * a negative value.
 */
static CGFloat GetLineHeightForFont(CTFontRef iFont)
{
    CGFloat lineHeight = 0.0;

    // Get the ascent from the font, already scaled for the font's size
    lineHeight += CTFontGetAscent(iFont);

    // Get the descent from the font, already scaled for the font's size
    lineHeight += CTFontGetDescent(iFont);

    // Get the leading from the font, already scaled for the font's size
    lineHeight += CTFontGetLeading(iFont);
    return lineHeight;
}
