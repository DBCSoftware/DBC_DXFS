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
#define INC_ERRNO
#define INC_CTYPE
#include "includes.h"
#include "base.h"
#include "fio.h"
#include "gui.h"
#include "fontfiles.h"
#include "procfontfiles.h"
#include "prt.h"

#if defined(Linux) && defined(i386)
#define LUFORMATTYPE "%u"
#else
#define LUFORMATTYPE "%zu"
#endif

#if OS_UNIX
#include <iconv.h>
#endif

#define FONT_ARRAY_START_SIZE 200
#define FONT_ARRAY_INCREMENT_SIZE 100

#if OS_WIN32
	LCID lcid;
#endif

#define ISUNICODE(pid, peid) ((pid) == 0 || (pid) == 3 || ((pid) == 2 && (peid) == 1))

static INT getNextFontDataIndex(INT fileIndex);
static INT processFontFilesA(FONTFILEDATA *ffd, INT fontIndex);
static int readInt(FONTFILEDATA *ffd, INT *value);
static int readHeadTable(INT fontIndex);
static int readHheaTable(INT fontIndex);
static int readNameTable(INT fontIndex);
static int readOS2Table(INT fontIndex);
static int readPostTable(INT fontIndex);
static int readTableRecords(INT fontIndex);

/**
 * Return zero if successful.
 * Return RC_ERROR if fail with further info in FontFiles.error
 * Might move memory
 */
INT processFontFiles() {
	INT fileIndex, i1, i2, i3;
	CHAR *ptr;
	UINT uiWork;
	FONTFILEDATA *ffd;

#if OS_WIN32
	lcid = GetSystemDefaultLCID(); /* Returns the locale identifier for the system locale. */
	lcid = LOWORD(lcid);
#endif

	FontFiles.error.msg[0] = '\0';
	for (fileIndex = 0; fileIndex < FontFiles.numberOfFiles; fileIndex++) {
		ffd = &FontFiles.fontFileData[fileIndex];
		guiLockMem(ffd->fileName);
		ptr = guiMemToPtr(ffd->fileName);
		ffd->fileHandle = fioopen(ptr, FIO_M_SRO);
		guiUnlockMem(ffd->fileName);
		if (ffd->fileHandle < 0) {
			FontFiles.error.error_code = 1;
			FontFiles.error.fioerror = ffd->fileHandle;
			FontFiles.error.prterror = 0;
			strcpy(FontFiles.error.msg, fioerrstr(ffd->fileHandle));
			sprintf(FontFiles.error.function, "%s", __FUNCTION__);
			return RC_ERROR;
		}
		ffd->offset = 0;
		i2 = readInt(ffd, (INT*)&uiWork);
		if (i2) return i2;
		if (uiWork == 0x74746366 /* 'ttcf' */) {
			ffd->sfnt_version = uiWork;
			ffd->offset = 8;
			i2 = readUlong(ffd, &ffd->countOfTtcFonts);
			if (i2) return i2;
			ffd->offsetTable = (ULONG*)malloc(ffd->countOfTtcFonts * sizeof(ULONG));
			if (ffd->offsetTable == NULL) {
				FontFiles.error.error_code = 2;
				FontFiles.error.fioerror = 0;
				FontFiles.error.prterror = PRTERR_NOMEM;
				strcpy(FontFiles.error.msg, "malloc fail");
				sprintf(FontFiles.error.function, "%s", __FUNCTION__);
				return RC_ERROR;
			}
			for (i3 = 0; (ULONG)i3 < ffd->countOfTtcFonts; i3++) {
				i2 = readUlong(ffd, &ffd->offsetTable[i3]);
				if (i2) return i2;
			}
			for (i3 = 0; (ULONG)i3 < ffd->countOfTtcFonts; i3++) {
				i1 = getNextFontDataIndex(fileIndex);
				ffd->offset = ffd->offsetTable[i3];
				readInt(ffd, (INT*)&FontFiles.fontData[i1].sfnt_version);
				FontFiles.fontData[i1].ttcIndex = i3;
				FontFiles.numberOfFonts = i1 + 1;
				i2 = processFontFilesA(ffd, i1);
				if (i2) return i2;
			}
		}
		else if (uiWork == 0x00010000 || uiWork == 0x4F54544F /* 'OTTO' */) {
			i1 = getNextFontDataIndex(fileIndex);
			FontFiles.fontData[i1].sfnt_version = FontFiles.fontFileData[fileIndex].sfnt_version = uiWork;
			FontFiles.numberOfFonts = i1 + 1;
			ffd->offset = 4;
			i2 = processFontFilesA(ffd, i1);
			if (i2) return i2;
		}
		fioclose(ffd->fileHandle);
		ffd->fileHandle = -1;
	}

	FontFiles.error.error_code = 0;
	return 0;
}

/**
 * ffd			Pointer to a FONTFILEDATA structure
 * fontIndex	Index into the FONTDATA array
 * Might move memory
 */
static INT processFontFilesA(FONTFILEDATA *ffd, INT fontIndex) {
	INT i2;
	i2 = readUnsignedShort(ffd, (USHORT*)&FontFiles.fontData[fontIndex].numTables);
	if (i2) return i2;
	ffd->offset += 6;	/* searchRange, entrySelector, rangeShift */
	i2 = readTableRecords(fontIndex);
	if (i2) return i2;
	if (FontFiles.fontData[fontIndex].name.offset != 0) {
		i2 = readNameTable(fontIndex);
		if (i2) return i2;
	}
	if (FontFiles.fontData[fontIndex].os_2.offset != 0) {
		i2 = readOS2Table(fontIndex);
		if (i2) return i2;
	}
	if (FontFiles.fontData[fontIndex].head.offset != 0) {
		i2 = readHeadTable(fontIndex);
		if (i2) return i2;
	}
	if (FontFiles.fontData[fontIndex].hhea.offset != 0) {
		i2 = readHheaTable(fontIndex);
		if (i2) return i2;
	}
	if (FontFiles.fontData[fontIndex].post.offset != 0) {
		i2 = readPostTable(fontIndex);
		if (i2) return i2;
	}
	return 0;
}

/**
 * Uses realloc. Might move FontFiles.fontData
 */
static INT getNextFontDataIndex(INT fileIndex) {
	static INT fontdataAllocation;
	static INT fontdataNextIndex;

	if (FontFiles.fontData == NULL) {
		FontFiles.fontData = calloc(FONT_ARRAY_START_SIZE, sizeof(FONTDATA));
		FontFiles.fontData[0].ffdIndex = fileIndex;
		fontdataAllocation = FONT_ARRAY_START_SIZE;
		fontdataNextIndex = 1;
		FontFiles.fontData[0].ffdIndex = fileIndex;
		return 0;
	}

	if (fontdataNextIndex >= fontdataAllocation) {
		void *temp = FontFiles.fontData;
		fontdataAllocation += FONT_ARRAY_INCREMENT_SIZE;
		FontFiles.fontData = realloc(FontFiles.fontData, fontdataAllocation * sizeof(FONTDATA));
		if (FontFiles.fontData == NULL) {
			if (temp != NULL) free(temp);
			FontFiles.error.error_code = 2;
			FontFiles.error.fioerror = 0;
			FontFiles.error.prterror = PRTERR_NOMEM;
			strcpy(FontFiles.error.msg, "realloc fail");
			sprintf(FontFiles.error.function, "%s", __FUNCTION__);
			return RC_ERROR;
		}
	}
	memset(&FontFiles.fontData[fontdataNextIndex], 0, sizeof(FONTDATA));
	FontFiles.fontData[fontdataNextIndex].ffdIndex = fileIndex;
	return fontdataNextIndex++;
}

/**
 * index is into the FONTDATA array
 *
 * Return zero if successful.
 * Return RC_ERROR if fail, with further info in FontFiles.error
 */
static int readPostTable(INT fontIndex) {
	FONTDATA *fd = &FontFiles.fontData[fontIndex];
	INT i1;
	FONTFILEDATA *ffd = &FontFiles.fontFileData[fd->ffdIndex];
	SHORT mantissa;
	USHORT fraction;

	FontFiles.error.msg[0] = '\0';
	ffd->offset = fd->post.offset;
	ffd->offset += 4;	/* Table version number */
	i1 = readSignedShort(ffd, &mantissa);
	if (i1) return i1;
	i1 = readUnsignedShort(ffd, &fraction);
	if (i1) return i1;
	fd->post_data.italicAngle = (double)mantissa + (double)fraction / 16384.0;
	ffd->offset += 2;	/* underlinePosition */
	ffd->offset += 2;	/* underlineThickness */
	i1 = readUlong(ffd, &fd->post_data.isFixedPitch);
	if (i1) return i1;
	FontFiles.error.error_code = 0;
	return 0;
}

/**
 * index is into the FONTDATA array
 *
 * Return zero if successful.
 * Return RC_ERROR if fail, with further info in FontFiles.error
 */
static int readHeadTable(INT fontIndex) {
	FONTDATA *fd = &FontFiles.fontData[fontIndex];
	INT i1;
	FONTFILEDATA *ffd = &FontFiles.fontFileData[fd->ffdIndex];

	FontFiles.error.msg[0] = '\0';
	ffd->offset = fd->head.offset;
	ffd->offset += 4;	/* Table version number */
	ffd->offset += 4;	/* fontRevision */
	ffd->offset += 4;	/* checkSumAdjustment */
	ffd->offset += 4;	/* magicNumber */
	ffd->offset += 2;	/* flags */
	i1 = readUnsignedShort(ffd, &fd->head_data.unitsPerEm);
	if (i1) return i1;
	ffd->offset += 8; /* created */
	ffd->offset += 8; /* modified */

	i1 = readSignedShort(ffd, &fd->head_data.xMin); /* xMin */
	if (i1) return i1;

	i1 = readSignedShort(ffd, &fd->head_data.yMin); /* yMin */
	if (i1) return i1;

	i1 = readSignedShort(ffd, &fd->head_data.xMax); /* xMax */
	if (i1) return i1;

	i1 = readSignedShort(ffd, &fd->head_data.yMax); /* yMax */
	if (i1) return i1;

	i1 = readUnsignedShort(ffd, &fd->head_data.macStyle);
	if (i1) return i1;
	FontFiles.error.error_code = 0;
	return 0;
}

/**
 * index is into the FONTDATA array
 *
 * Return zero if successful.
 * Return RC_ERROR if fail, with further info in FontFiles.error
 */
static int readHheaTable(INT fontIndex) {
	FONTDATA *fd = &FontFiles.fontData[fontIndex];
	INT i1;
	FONTFILEDATA *ffd = &FontFiles.fontFileData[fd->ffdIndex];

	FontFiles.error.msg[0] = '\0';
	ffd->offset = fd->hhea.offset;
	ffd->offset += 4;	/* Table version number */
	ffd->offset += 2;	/* Ascender */
	ffd->offset += 2;	/* Descender */
	ffd->offset += 2;	/* Line Gap */
	ffd->offset += 2;	/* advanceWidthMax */
	ffd->offset += 2;	/* minLeftSideBearing */
	ffd->offset += 2;	/* minRightSideBearing */
	ffd->offset += 2;	/* xMaxExtent */
	ffd->offset += 2;	/* caretSlopeRise */
	ffd->offset += 2;	/* caretSlopeRun */
	ffd->offset += 2;	/* caretOffset */
	ffd->offset += 8;	/* reserved */
	ffd->offset += 2;	/* metricDataFormat */
	i1 = readUnsignedShort(ffd, &fd->hhea_data.numberOfHMetrics);
	if (i1) return i1;

	FontFiles.error.error_code = 0;
	return 0;
}

/**
 * index is into the FONTDATA array
 *
 * Return zero if successful.
 * Return RC_ERROR if fail, with further info in FontFiles.error
 */
static int readOS2Table(INT fontIndex) {
	FONTDATA *fd = &FontFiles.fontData[fontIndex];
	FONTFILEDATA *ffd = &FontFiles.fontFileData[fd->ffdIndex];
	INT i1;

	FontFiles.error.msg[0] = '\0';
	ffd->offset = fd->os_2.offset;
	ffd->offset += 8;		/* version, xAvgCharWidth, usWeightClass, usWidthClass */
	i1 = readUnsignedShort(ffd, &fd->os2_data.fsType);
	if (i1) return i1;
	ffd->offset += 22;		/* ySubscriptXSize, ySubscriptYSize, ySubscriptXOffset, ySubscriptYOffset
		 	 	 	 	 	 ySuperscriptXSize, ySuperscriptYSize, ySuperscriptXOffset
		 	 	 	 	 	 ySuperscriptYOffset, yStrikeoutSize, yStrikeoutPosition
		 	 	 	 	 	 sFamilyClass */
	ffd->offset += 10;		/* PANOSE */
	ffd->offset += 16;		/* ulUnicodeRange 1-4 */
	ffd->offset += 10;		/* achVendID, fsSelection, usFirstCharIndex, usLastCharIndex */
	i1 = readSignedShort(ffd, &fd->os2_data.sTypoAscender);
	if (i1) return i1;
	i1 = readSignedShort(ffd, &fd->os2_data.sTypoDescender);
	if (i1) return i1;
	if (fd->os2_data.sTypoDescender > 0) fd->os2_data.sTypoDescender = (SHORT)(-fd->os2_data.sTypoDescender);

	ffd->offset += 16;	/* sTypoLineGap, usWinAscent, usWinDescent, ulCodePageRange 1-2, sxHeight */
	i1 = readSignedShort(ffd, &fd->os2_data.sCapHeight);
	if (i1) return i1;
	FontFiles.error.error_code = 0;
	return 0;
}

/**
 * index is into the FONTDATA array
 *
 * Return zero if successful.
 * Return RC_ERROR if fail, with further info in FontFiles.error
 * Might move memalloc memory
 */
static int readNameTable(INT fontIndex) {
	FONTDATA *fd = &FontFiles.fontData[fontIndex];
	FONTFILEDATA *ffd = &FontFiles.fontFileData[fd->ffdIndex];
	USHORT numRecords, startOfStorage;
	INT i1, i2, i3, k, length;
	USHORT platformID;
	USHORT platformEncodingID;
	USHORT languageID;
	USHORT nameID;
	USHORT cbName;
	USHORT stroffset;
	CHAR *ptrIn; /* Always ascii, never Unicode */
	struct names_tag {
		USHORT platformID;
		USHORT platformEncodingID;
		USHORT languageID;
		USHORT nameID;
		USHORT cbName;	/* In bytes, even for Unicode names */
		OFFSET  offset; /* From the beginning of the file */
	} **names = NULL;
	INT haveFamName = 0, havePSName = 0;

#if OS_WIN32
	BYTE fntName[256], fntNameUni[512], dummy[512];

#else
	CHAR fntName[256], fntNameUni[512], dummy[512];
	CHAR *inbuf, *outbuf;
#ifdef __MACOSX
	iconv_t convertor = iconv_open("Macintosh", "UTF-16LE");
#else
	iconv_t convertor = iconv_open("UTF-8", "UNICODE");
#endif
	size_t inbytesleft, outbytesleft;

	if (convertor == (iconv_t)(-1)) {
		FontFiles.error.error_code = 2;
		FontFiles.error.fioerror = 0;
		FontFiles.error.prterror = PRTERR_NATIVE;
		sprintf(FontFiles.error.msg, "iconv_open failed, errno=%d", errno);
		sprintf(FontFiles.error.function, "%s", __FUNCTION__);
		return RC_ERROR;
	}
#endif

	ffd->offset = fd->name.offset;
	ffd->offset += 2; 	/* skip format selector */
	i1 = readUnsignedShort(ffd, &numRecords);
	if (i1) return i1;
	if (numRecords < 1) {
		FontFiles.error.error_code = 2;
		FontFiles.error.fioerror = 0;
		FontFiles.error.prterror = PRTERR_INTERNAL;
		sprintf(FontFiles.error.msg, "Bad font file format");
		sprintf(FontFiles.error.function, "%s", __FUNCTION__);
		return RC_ERROR;
	}
	i1 = readUnsignedShort(ffd, &startOfStorage);
	if (i1) return i1;

	/*
	 * Allocate temporary storage for name table
	 */
	names = (struct names_tag**) memalloc(sizeof(struct names_tag) * numRecords, 0);
	if (names == NULL) {
		FontFiles.error.error_code = 2;
		FontFiles.error.fioerror = 0;
		FontFiles.error.prterror = PRTERR_NOMEM;
		sprintf(FontFiles.error.msg, "memalloc for name table failed, size=" LUFORMATTYPE,
				sizeof(struct names_tag) * numRecords);
		sprintf(FontFiles.error.function, "%s", __FUNCTION__);
		return RC_ERROR;
	}

	/*
	 * Store (almost) the entire name table into temporary memory
	 * Rejecting uninteresting IDs as we go
	 */
	for (k = 0, i1 = 0; k < numRecords; k++) {
		i2 = readUnsignedShort(ffd, &platformID);
		if (i2) return i2;
		i2 = readUnsignedShort(ffd, &platformEncodingID);
		if (i2) return i2;
		i2 = readUnsignedShort(ffd, &languageID);
		if (i2) return i2;
		i2 = readUnsignedShort(ffd, &nameID);
		if (i2) return i2;
		i2 = readUnsignedShort(ffd, &cbName); /* In bytes, even for Unicode names */
		if (i2) return i2;
		i2 = readUnsignedShort(ffd, &stroffset);
		if (i2) return i2;

        /*
		 * We only care about name IDs 1 and 6
         */
        if (nameID != FONTFAMILYNAMEID && nameID != FONTPOSTSCRIPTNAMEID) {
        	continue;
        }

        /*
         * For platformID 1 (Mac), only interested in Roman encoding (0)
         */
        if (platformID == 1 && platformEncodingID != 0) continue;

        /*
         * For platformID 3 (Windows), only interested in Symbol (0) or Unicode (1)
         */
        if (platformID == 3 && platformEncodingID > 1) continue;

#if OS_WIN32
        /**
         * On Windows, ignore any not in local language
         */
        if (platformID == 3 && languageID != lcid) continue;
#endif


		(*names)[i1].platformID = platformID;
		(*names)[i1].platformEncodingID = platformEncodingID;
		(*names)[i1].languageID = languageID;
		(*names)[i1].nameID = nameID;
		(*names)[i1].cbName = cbName;
		(*names)[i1].offset = fd->name.offset + startOfStorage + stroffset;
        i1++;

        if (nameID == FONTFAMILYNAMEID) haveFamName = 1;
        else if (nameID == FONTPOSTSCRIPTNAMEID) havePSName = 1;

        /* If we have both a Family Name and a PostScript name, we can quit */
        if (haveFamName && havePSName) break;

	}

	numRecords = i1;
	fd->names = (TTFONTNAME*)calloc(numRecords, sizeof(TTFONTNAME));
	if (fd->names == NULL) {
		FontFiles.error.error_code = 2;
		FontFiles.error.fioerror = 0;
		FontFiles.error.prterror = PRTERR_NOMEM;
		sprintf(FontFiles.error.msg, "calloc for TTFONTNAMEs failed, size=" LUFORMATTYPE,
				sizeof(TTFONTNAME) * numRecords);
		sprintf(FontFiles.error.function, "%s",
					__FUNCTION__
			);
		return RC_ERROR;
	}
	fd->numberOfNames = numRecords;

	for (k = 0; k < numRecords; k++) {
		platformID = (*names)[k].platformID;
		platformEncodingID = (*names)[k].platformEncodingID;
		length = (*names)[k].cbName;
		fd->names[k].nameID = (*names)[k].nameID;
		if (ISUNICODE(platformID, platformEncodingID)) {
			i2 = fioread(ffd->fileHandle, (*names)[k].offset, (UCHAR*)fntNameUni, length);
			if (i2 < 0) {
				FontFiles.error.error_code = 1;
				FontFiles.error.fioerror = i2;
				FontFiles.error.prterror = 0;
				strcpy(FontFiles.error.msg, fioerrstr(i2));
				sprintf(FontFiles.error.function, "%s",
					__FUNCTION__
			);
				return RC_ERROR;
			}
			/*
			 * Need to detect little-endian and run the below loop.
			 * Unicode is always big-endian in ttf files
			 * But a WCHAR on Intel is little-endian
			 */
			if (O32_HOST_ORDER == O32_LITTLE_ENDIAN) {
				for (i3 = 0; i3 < (length - 1); i3 += 2) {
					dummy[i3] = fntNameUni[i3 + 1];
					dummy[i3 + 1] = fntNameUni[i3];
				}
			}
			else memcpy(dummy, fntNameUni, length);
#if OS_WIN32
			if (!WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)dummy, length >> 1,
					(LPSTR)fntName, sizeof(fntName), NULL, NULL)) {
			}
#else
			inbytesleft = length;
			outbytesleft = sizeof(fntName);
			inbuf = dummy;
			outbuf = fntName;
			if (iconv(convertor,
					&inbuf, &inbytesleft, &outbuf, &outbytesleft) == (size_t)(-1)) {
			}
#endif
			fd->names[k].name = guiAllocString((BYTE*)fntName, length >> 1);
			if (fd->names[k].name == NULL) {
				FontFiles.error.error_code = 2;
				FontFiles.error.fioerror = 0;
				FontFiles.error.prterror = PRTERR_NOMEM;
				sprintf(FontFiles.error.msg, "guiAllocString for fntName failed, size=%d",
						length >> 1);
				sprintf(FontFiles.error.function, "%s", __FUNCTION__);
				return RC_ERROR;
			}
		}
		else {
			i2 = fioread(ffd->fileHandle, (*names)[k].offset, (UCHAR*)fntName, length);
			if (i2 < 0) {
				FontFiles.error.error_code = 1;
				FontFiles.error.fioerror = i2;
				FontFiles.error.prterror = 0;
				strcpy(FontFiles.error.msg, fioerrstr(i2));
				sprintf(FontFiles.error.function, "%s", __FUNCTION__);
				return RC_ERROR;
			}
			fd->names[k].name = guiAllocString((BYTE*)fntName, length);
			if (fd->names[k].name == NULL) {
				FontFiles.error.error_code = 2;
				FontFiles.error.fioerror = 0;
				FontFiles.error.prterror = PRTERR_NOMEM;
				sprintf(FontFiles.error.msg, "guiAllocString for fntName failed, size=%d",
						length);
				sprintf(FontFiles.error.function, "%s",
					__FUNCTION__
				);
				return RC_ERROR;
			}
		}

		/*
		 * For the Family Name, which we match on to find a font file,
		 * set up 'canonical' name.
		 * Squeeze out blanks and set all alpha to caps
		 */
		if (fd->names[k].nameID == FONTFAMILYNAMEID) {
			guiLockMem(fd->names[k].name);
			ptrIn = guiMemToPtr(fd->names[k].name);
			for (i1 = i2 = 0; i1 < (INT)strlen(ptrIn); i1++) {
				if (ptrIn[i1] != ' ') {
					dummy[i2++] = toupper(ptrIn[i1]);
				}
			}
			fd->names[k].canonicalName = guiAllocString((UCHAR*)dummy, i2);
			guiUnlockMem(fd->names[k].name);
		}
		else {
			fd->names[k].canonicalName = NULL;
		}

	}


	if (names != NULL) memfree((UCHAR**)names);
#if OS_UNIX
	iconv_close(convertor);
#endif
	FontFiles.error.error_code = 0;
	return 0;
}

/**
 * Read Table Records, get offset and length
 * for tables we care about.
 *
 * index is into the FONTDATA array
 *
 * Return zero if ok.
 * If fail, return RC_ERROR with further info in FontFiles.error
 */
static int readTableRecords(INT fontIndex) {
	FONTDATA *fd = &FontFiles.fontData[fontIndex];
	FONTFILEDATA *ffd = &FontFiles.fontFileData[fd->ffdIndex];
	CHAR tag[5];
	INT i1, i2;
	INT n1 = fd->numTables;
	ULONG tabOffset, tabLength;

	tag[4] = '\0';
	for (i1 = 0; i1 < n1; i1++) {
		i2 = fioread(ffd->fileHandle, ffd->offset, (UCHAR*)tag, 4);
		if (i2 < 0) {
			FontFiles.error.error_code = 1;
			FontFiles.error.fioerror = i2;
			strcpy(FontFiles.error.msg, fioerrstr(i2));
			sprintf(FontFiles.error.function, "%s", __FUNCTION__);
			return RC_ERROR;
		}
		ffd->offset += 8; /* go past tag and checksum */
		i2 = readUlong(ffd, &tabOffset);
		if (i2) return i2;
		i2 = readUlong(ffd, &tabLength);
		if (i2) return i2;
		if (!strcmp(tag, "cmap")) {	/* Character to glyph mapping */
			fd->cmap.length = tabLength;
			fd->cmap.offset = tabOffset;
		}
		else if (!strcmp(tag, "head")) {	/* Font header */
			fd->head.length = tabLength;
			fd->head.offset = tabOffset;
		}
		else if (!strcmp(tag, "hhea")) {	/* Horizontal header */
			fd->hhea.length = tabLength;
			fd->hhea.offset = tabOffset;
		}
		else if (!strcmp(tag, "hmtx")) {	/* Horizontal metrics */
			fd->hmtx.length = tabLength;
			fd->hmtx.offset = tabOffset;
		}
		else if (!strcmp(tag, "name")) {	/* Naming table */
			fd->name.length = tabLength;
			fd->name.offset = tabOffset;
		}
		else if (!strcmp(tag, "OS/2")) {	/* OS/2 and Windows specific metrics */
			fd->os_2.length = tabLength;
			fd->os_2.offset = tabOffset;
		}
		else if (!strcmp(tag, "post")) {	/* PostScript information */
			fd->post.length = tabLength;
			fd->post.offset = tabOffset;
		}
	}
	FontFiles.error.error_code = 0;
	return 0;
}

/**
 * Read four bytes from the file at offset
 * and return them in value.
 * In font files, an INT is always in big-endian order.
 *
 * Return zero if ok, RC_ERROR if not with further info in FontFiles.error
 */
static int readInt(FONTFILEDATA *ffd, INT *value) {
	unsigned char ch[4];
	INT i3;

	ch[0] = ch[1] = ch[2] = ch[3] = 0;
	i3 = fioread(ffd->fileHandle, ffd->offset, &ch[0], 4);
	if (i3 < 0) {
		FontFiles.error.error_code = 1;
		FontFiles.error.fioerror = i3;
		strcpy(FontFiles.error.msg, fioerrstr(i3));
		sprintf(FontFiles.error.function, "%s",
#ifndef NO__FUNCTION__
					__FUNCTION__
#else
					"readInt"
#endif
		);
		return RC_ERROR;
	}
	*value = ((ch[0] << 24) + (ch[1] << 16) + (ch[2] << 8) + ch[3]);
	ffd->offset += 4;
	FontFiles.error.error_code = 0;
	return 0;
}

/**
 * Read next two bytes from the file at offset
 * and return them in value.
 * In font files, a USHORT is always in big-endian order.
 *
 * Return zero if ok, RC_ERROR if not with further info in FontFiles.error
 */
INT readUnsignedShort(FONTFILEDATA *ffd, USHORT *value) {
	unsigned char ch[2];
	INT i3;
	ch[0] = ch[1] = 0;
	i3 = fioread(ffd->fileHandle, ffd->offset, &ch[0], 2);
	if (i3 < 0) {
		FontFiles.error.error_code = 1;
		FontFiles.error.fioerror = i3;
		strcpy(FontFiles.error.msg, fioerrstr(i3));
		sprintf(FontFiles.error.function, "%s",
#ifndef NO__FUNCTION__
					__FUNCTION__
#else
					"readUnsignedShort"
#endif
		);
		return RC_ERROR;
	}
	*value = ((ch[0] << 8) + ch[1]);
	ffd->offset += 2;
	FontFiles.error.error_code = 0;
	return 0;
}

/**
 * Read next two bytes from the file at offset
 * and return them in value.
 * In font files, a SHORT is always in big-endian order.
 *
 * Return zero if ok, RC_ERROR if not with further info in FontFiles.error
 */
INT readSignedShort(FONTFILEDATA *ffd, SHORT *value) {
	unsigned char ch[2];
	INT i3;
	ch[0] = ch[1] = 0;
	i3 = fioread(ffd->fileHandle, ffd->offset, &ch[0], 2);
	if (i3 < 0) {
		FontFiles.error.error_code = 1;
		FontFiles.error.fioerror = i3;
		strcpy(FontFiles.error.msg, fioerrstr(i3));
		sprintf(FontFiles.error.function, "%s",
#ifndef NO__FUNCTION__
					__FUNCTION__
#else
					"readSignedShort"
#endif
		);
		return RC_ERROR;
	}
	*value = (SHORT)((ch[0] << 8) + ch[1]);
	ffd->offset += 2;
	FontFiles.error.error_code = 0;
	return 0;
}

/**
 * Read next four bytes from the file at offset
 * and return them in value.
 * In font files, an ULONG is always in big-endian order and is a 32-bit unsigned integer.
 *
 * Return zero if ok, RC_ERROR if not with further info in FontFiles.error
 */
INT readUlong(FONTFILEDATA *ffd, ULONG *value) {
	unsigned char ch[4];
	INT i3;

	FontFiles.error.error_code = 0;
	ch[0] = ch[1] = ch[2] = ch[3] = 0;
	i3 = fioread(ffd->fileHandle, ffd->offset, &ch[0], 4);
	if (i3 < 0) {
		FontFiles.error.error_code = 1;
		FontFiles.error.fioerror = i3;
		strcpy(FontFiles.error.msg, fioerrstr(i3));
		sprintf(FontFiles.error.function, "%s",
#ifndef NO__FUNCTION__
					__FUNCTION__
#else
					"readUlong"
#endif
		);
		return RC_ERROR;
	}
	*value = (ULONG)((ch[0] << 24) + (ch[1] << 16) + (ch[2] << 8) + ch[3]);
	ffd->offset += 4;
	FontFiles.error.error_code = 0;
	return 0;
}

/**
 * Read next byte from the file
 * and return it as an unsigned int.
 *
 * Return zero if ok, RC_ERROR if not with further info in FontFiles.error
 */
INT readUnsignedByte(FONTFILEDATA *ffd, UINT *value) {
	unsigned char ch[1];
	INT i3;
	ch[0] = 0;
	i3 = fioread(ffd->fileHandle, ffd->offset, &ch[0], 1);
	if (i3 < 0) {
		FontFiles.error.error_code = 1;
		FontFiles.error.fioerror = i3;
		strcpy(FontFiles.error.msg, fioerrstr(i3));
		sprintf(FontFiles.error.function, "%s",
#ifndef NO__FUNCTION__
					__FUNCTION__
#else
					"readUnsignedByte"
#endif
		);
		return RC_ERROR;
	}
	*value = (UINT) ch[0];
	ffd->offset += 1;
	FontFiles.error.error_code = 0;
	return 0;
}
