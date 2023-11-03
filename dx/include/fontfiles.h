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

#ifndef FONTFILES_H_
#define FONTFILES_H_
#include "includes.h"

#define FONTFAMILYNAMEID 1
#define FONTPOSTSCRIPTNAMEID 6

typedef struct fontfilehandle_tag {
	/*
	 * Index into the array of FONTDATA structures
	 */
	INT fontIndex;
	/*
	 * Index into the TTFONTNAME array of the FONTDATA that matched on nameID
	 */
	INT ttnameIndex;
	/*
	 * Temporary storage for the object number of the width table during output
	 */
	INT widthObjectNumber;
	/*
	 * Temporary storage for the object number of the /Type/Font object during output
	 */
	INT tFontObjectNumber;
	/*
	 * Temporary storage for the object number of the /Type/FontDescriptor object during output
	 */
	INT tDescriptorObjectNumber;
	/*
	 * Temporary storage for the /BaseFont name used
	 * Needs to match between the /Font and the /FontDescriptor
	 */
	CHAR basefont[128];
	INT tFontProgramObjectNumber;
} FONTFILEHANDLE;

typedef struct fontTableDictStructEntry {
	ULONG offset;	/* offset of table from beginning of font file */
	ULONG length;	/* length in bytes of table */
} FTDICTENTRY;

typedef struct TTFontName {
	/*
	 * Store these as ascii always, never Unicode
	 */
	ZHANDLE name;
	ZHANDLE canonicalName; /* Set to all caps and blanks squeezed out */
	USHORT nameID;		/* 1 for family name, 6 for Postscript name */
} TTFONTNAME;

#define IS_FONT_BOLD(index) ((FontFiles.fontData[index].head_data.macStyle & 0x0001) != 0)
#define IS_FONT_ITALIC(index) ((FontFiles.fontData[index].head_data.macStyle & 0x0002) != 0)
#define IS_FONT_EMBEDABLE(index) (FontFiles.fontData[index].os2_data.fsType != 0x0002)

/*
 * One instance of this structure for each font.
 * Because of TTC files, there might be more of these than files.
 */
typedef struct fontDataStruct {
	UINT sfnt_version;
	UINT ffdIndex; 		/* Index into the FONTFILEDATA array of the matching file */
	UINT ttcIndex;		/* Zero based index into a ttc file, only used if this font is from a ttc */
	TTFONTNAME *names;	/* Pointer to array of TTFONTNAME structures */
	USHORT numTables;
	USHORT numberOfNames;
	FTDICTENTRY cmap;	/* Character to glyph mapping */
	FTDICTENTRY head;	/* Font header */
	FTDICTENTRY hhea;	/* Horizontal header */
	FTDICTENTRY hmtx;	/* Horizontal metrics, Glyph widths*/
	FTDICTENTRY name;	/* Naming table */
	FTDICTENTRY os_2;	/* OS/2 and Windows specific metrics */
	FTDICTENTRY post;	/* PostScript information */
	struct _head {
		USHORT unitsPerEm;
		SHORT xMin;
		SHORT yMin;
		SHORT xMax;
		SHORT yMax;
		USHORT macStyle; /* Bit 0: Bold (if set to 1), Bit 1: Italic (if set to 1) */
	} head_data;
	struct _hhea {
		/*
		 * "Number of hMetric entries in 'hmtx' table"
		 */
		USHORT numberOfHMetrics;
	} hhea_data;
	struct _cmap {
		ULONG cmap10;		/* Offset to Mac format 0 cmap table, 0 if does not exist */
		ULONG cmap16;		/* Offset to Mac format 6 cmap table, 0 if does not exist */
		ULONG cmap34;		/* Offset to Windows format 4 cmap table, 0 if does not exist */
		USHORT numTables;
		struct _cmap_tables {
			USHORT platformID;
			USHORT encodingID;
			USHORT format;
			ULONG offset;
		} *cmap_tables; /*Pointer to array of structs with numTables entries */
		USHORT CmapSize; /* number of entries in the below array */
		struct _cmap_glyphs {
			UCHAR characterCode;
			USHORT glyphIndex;
		} *Cmap;
	} cmap_data;
	struct _os2 {
		USHORT	fsType; /* If this is exactly 0x0002, then it MAY NOT be embedded */
		SHORT sTypoAscender; /* The typographic ascender for this font. */
		SHORT sTypoDescender; /* The typographic descender for this font. */
		/**
		 * This metric specifies the distance between the baseline and the
		 * approximate height of uppercase letters measured in FUnits.
		 */
		SHORT sCapHeight;
	} os2_data;
	struct _hmtx {
		USHORT *glyphWidths; /* Pointer to array of glyph widths */
	} hmtx_data;
	struct _post {
		/**
		 * Italic angle in counter-clockwise degrees from the vertical.
		 * Zero for upright text, negative for text that leans to the right (forward).
		 */
		DOUBLE italicAngle;
		/*
		 * Set to 0 if the font is proportionally spaced, non-zero if the font is not
		 * proportionally spaced (i.e. monospaced).
		 */
		ULONG isFixedPitch;
	} post_data;
} FONTDATA;

/*
 * One instance of this structure for each physical file
 */
typedef struct fontFileDataStruct {
	/*
	 * Used for ttc files only
	 */
	ULONG *offsetTable;
	ZHANDLE fileName;  /* with full path */
	UINT sfnt_version;
	ULONG fileLength;		/* Length in bytes of the file */
	ULONG countOfTtcFonts;	/* Only used if this is a TTC file */
	INT fileHandle;			/* Open fio file handle to use in read routines, -1 if not open */
	OFFSET offset;			/* Current file offset for reading, advanced by read routines */
} FONTFILEDATA;

typedef struct fontFiles_tag {
	struct _error {
	 	/*
	 	 * 0 = no error
	 	 * 1 = fio error
	 	 * 2 = a PRTERR_xxx code
	 	 */
		INT error_code;
		CHAR function[64];	/* Name of function reporting the error */
		/*
		 * If fio error, the fio error code; e.g. ERR_NOTOP
		 * These error codes are all negative
		 */
		INT fioerror;
		/*
		 * If non fio error, the PRT error code; e.g. PRTERR_ACCESS
		 * These error codes are all greater than zero
		 */
		INT prterror;
		CHAR msg[128];		/*  */
	} error;
	/*
	 * Number of font files found in the system
	 */
	INT numberOfFiles;
	/*
	 * Pointer to an array of FONTFILEDATA structures.
	 * Allocated once via calloc and cannot move thereafter
	 */
	FONTFILEDATA *fontFileData;
	/*
	 * Number of fonts found in the system
	 */
	INT numberOfFonts;
	/*
	 * Pointer to an array of FONTDATA structures.
	 */
	FONTDATA *fontData;
} FONTFILES;

/*
 * The singleton instance of the FONTFILES structure
 */
#ifdef CATFONTFILES_C_
FONTFILES FontFiles;
#else
extern FONTFILES FontFiles;
#endif

/*
 * Functions that will be called from DX
 */

int locateFontFileByName(CHAR* name, INT bold, INT italic, FONTFILEHANDLE* ffh);
INT openFontFile(FONTDATA* ffd);


#endif /* FONTFILES_H_ */
