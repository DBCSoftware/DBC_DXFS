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
#include "gui.h"
#include "fontfiles.h"
#include "fio.h"
#include "procfontfiles.h"
#include "prt.h"
#include "base.h"

#if defined(Linux) && defined(i386)
#define LUFORMATTYPE "%u"
#else
#define LUFORMATTYPE "%zu"
#endif

static INT loadHtmxCmap(FONTDATA* ffd);
static INT readCmapTable(FONTDATA *ffd);
static INT readHmtxTable(FONTDATA *ffd);
static INT ReadFormat0(FONTDATA *ffd) ;
static INT ReadFormat4(FONTDATA *ffd) ;
static INT ReadFormat6(FONTDATA *ffd) ;

/*
 * Calls memalloc, might cause memory to move
 * Returns zero for success, RC_ERROR if font data not found
 */
INT getGlyphWidth(FONTDATA* fd, UCHAR charvalue, USHORT *gwidth) {
	INT i1, i2;
	if (fd->hmtx_data.glyphWidths == NULL) {
		i1 = loadHtmxCmap(fd);
		if (i1) return RC_ERROR;
	}

	if (fd->hmtx_data.glyphWidths == NULL) return RC_ERROR;
	*gwidth = fd->hmtx_data.glyphWidths[0]; // default value
	if (fd->cmap_data.Cmap != NULL) {
		for (i2 = 0; i2 < fd->cmap_data.CmapSize; i2++) {
			if (fd->cmap_data.Cmap[i2].characterCode == charvalue) {
				if ((i1 = fd->cmap_data.Cmap[i2].glyphIndex) >= fd->hhea_data.numberOfHMetrics) {
					i1 = fd->hhea_data.numberOfHMetrics - 1;
				}
				*gwidth = fd->hmtx_data.glyphWidths[i1];
				return 0;
			}
		}
	}
	return 0;
}

/*
 * Calls memalloc, might cause memory to move
 */
static INT loadHtmxCmap(FONTDATA* fd) {
	INT i1;
	FONTFILEDATA *ffd = &FontFiles.fontFileData[fd->ffdIndex];

	if ( (i1 = openFontFile(fd)) ) return i1;
	if ( (i1 = readHmtxTable(fd)) ) return i1;
	if ( (i1 = readCmapTable(fd)) ) return i1;
	fioclose(ffd->fileHandle);
	ffd->fileHandle = -1;
	return 0;
}

/*
 * Calls memalloc, might cause memory to move
 */
INT openFontFile(FONTDATA* fd) {

	FONTFILEDATA *ffd = &FontFiles.fontFileData[fd->ffdIndex];
	CHAR *ptr = guiLockMem(ffd->fileName);

	ffd->fileHandle = fioopen(ptr, FIO_M_SRO);
	if (ffd->fileHandle < 0) {
		FontFiles.error.error_code = 1;
		FontFiles.error.fioerror = ffd->fileHandle;
		FontFiles.error.prterror = 0;
		strcpy(FontFiles.error.msg, fioerrstr(ffd->fileHandle));
		sprintf(FontFiles.error.function, "%s",
					__FUNCTION__
			);
		guiUnlockMem(ffd->fileName);
		return RC_ERROR;
	}
	guiUnlockMem(ffd->fileName);
	return 0;
}

/**
 * Return zero if successful.
 * Return RC_ERROR if fail, with further info in FontFiles.error
 *
 * Calls memalloc, might cause memory to move
 */
static INT readHmtxTable(FONTDATA *fd) {
	FONTFILEDATA *ffd = &FontFiles.fontFileData[fd->ffdIndex];
	UINT i1, i2;
	UCHAR **tempSpace;
	USHORT maxCount = fd->hhea_data.numberOfHMetrics;
	UINT uiWork;
	ULONG tableByteCount;

	ffd->offset = fd->hmtx.offset;
	FontFiles.error.msg[0] = '\0';
	fd->hmtx_data.glyphWidths = (USHORT*)calloc(maxCount, sizeof(USHORT));
	if (fd->hmtx_data.glyphWidths == NULL) {
		FontFiles.error.error_code = 2;
		FontFiles.error.fioerror = 0;
		FontFiles.error.prterror = PRTERR_NOMEM;
		sprintf(FontFiles.error.msg, "calloc for glyphWidths table failed, size=" LUFORMATTYPE,
				maxCount * sizeof(USHORT));
		sprintf(FontFiles.error.function, "%s",
					__FUNCTION__
			);
		return RC_ERROR;
	}
	/**
	 * For efficiency, read in the entire glyph width table with a single call to fioread
	 */
	tableByteCount = maxCount * (2 * sizeof(USHORT));
	tempSpace = memalloc(tableByteCount, 0);
	if (tempSpace == NULL) {
		FontFiles.error.error_code = 2;
		FontFiles.error.fioerror = 0;
		FontFiles.error.prterror = PRTERR_NOMEM;
		sprintf(FontFiles.error.msg, "memalloc for temp glyphWidths table failed, size=%lu",
				tableByteCount);
		sprintf(FontFiles.error.function, "%s",
					__FUNCTION__
			);
		return RC_ERROR;
	}
	/* read it all */
	i2 = fioread(ffd->fileHandle, ffd->offset, *tempSpace, tableByteCount);
	if ((INT)i2 < 0) {
		FontFiles.error.error_code = 1;
		FontFiles.error.fioerror = i2;
		strcpy(FontFiles.error.msg, fioerrstr(i2));
		sprintf(FontFiles.error.function, "%s",
					__FUNCTION__
			);
		return RC_ERROR;
	}
	/* Load table from temp space */
	for (i1 = 0; i1 < maxCount; i1++) {
		i2 = i1 * (2 * sizeof(USHORT));
		uiWork = ((UINT)((*tempSpace)[i2]) << 8) + (*tempSpace)[i2 + 1];
		fd->hmtx_data.glyphWidths[i1] = (USHORT)((1000 * uiWork) / fd->head_data.unitsPerEm);
	}
	memfree(tempSpace);

	FontFiles.error.error_code = 0;
	return 0;
}

/**
 * Return zero if successful.
 * Return RC_ERROR if fail, with further info in FontFiles.error
 *
 * Might call memalloc and might therefore cause memory to move
 */
static INT readCmapTable(FONTDATA *fd) {
	FONTFILEDATA *ffd = &FontFiles.fontFileData[fd->ffdIndex];
	INT i1, i2;
	USHORT format;

	ffd->offset = fd->cmap.offset;
	FontFiles.error.msg[0] = '\0';
	ffd->offset += 2;  /* Table version number */
	i1 = readUnsignedShort(ffd, &fd->cmap_data.numTables);
	if (i1) return i1;

	fd->cmap_data.cmap_tables = calloc(fd->cmap_data.numTables, sizeof(fd->cmap_data.cmap_tables[0]));
	if (fd->cmap_data.cmap_tables == NULL) {
		FontFiles.error.error_code = 2;
		FontFiles.error.fioerror = 0;
		FontFiles.error.prterror = PRTERR_NOMEM;
		sprintf(FontFiles.error.msg, "calloc for cmap table failed, size=" LUFORMATTYPE,
				fd->cmap_data.numTables * sizeof(fd->cmap_data.cmap_tables[0]));
		sprintf(FontFiles.error.function, "%s",
					__FUNCTION__
			);
		return RC_ERROR;
	}

	for (i2 = 0; i2 < fd->cmap_data.numTables; i2++) {
		i1 = readUnsignedShort(ffd, &fd->cmap_data.cmap_tables[i2].platformID);
		if (i1) return i1;
		i1 = readUnsignedShort(ffd, &fd->cmap_data.cmap_tables[i2].encodingID);
		if (i1) return i1;
		i1 = readUlong(ffd, &fd->cmap_data.cmap_tables[i2].offset);
		if (i1) return i1;
	}

	/* Now go back and get the format for each table */
	for (i2 = 0; i2 < fd->cmap_data.numTables; i2++) {
		ffd->offset = fd->cmap.offset + fd->cmap_data.cmap_tables[i2].offset;
		i1 = readUnsignedShort(ffd, &fd->cmap_data.cmap_tables[i2].format);
		if (i1) return i1;
	}

	for (i1 = 0; i1 < fd->cmap_data.numTables; i1++) {
		if (fd->cmap_data.cmap_tables[i1].platformID == 1) {
			if (fd->cmap_data.cmap_tables[i1].format == 0) fd->cmap_data.cmap10 = fd->cmap_data.cmap_tables[i1].offset;
			else if (fd->cmap_data.cmap_tables[i1].format == 6) fd->cmap_data.cmap16 = fd->cmap_data.cmap_tables[i1].offset;
		}
		else if (fd->cmap_data.cmap_tables[i1].platformID == 3) {
			if (fd->cmap_data.cmap_tables[i1].format == 4) fd->cmap_data.cmap34 = fd->cmap_data.cmap_tables[i1].offset;
		}
	}

	/* Prefer a Mac (Platform ID 1) and table format 0 or 6.
	   Fallback if above not found, Windows format 4. */
	if (fd->cmap_data.cmap10) {
		ffd->offset = fd->cmap.offset + fd->cmap_data.cmap10;
		i1 = readUnsignedShort(ffd, &format);
		if (i1) return i1;
		switch (format) {
		case 0:
			i1 = ReadFormat0(fd);
			break;
		case 4:
			i1 = ReadFormat4(fd);
			break;
		case 6:
			i1 = ReadFormat6(fd);
			break;
		}
		if (i1) return i1;
	}
	else if (fd->cmap_data.cmap16) {
		ffd->offset = fd->cmap.offset + fd->cmap_data.cmap16;
		i1 = readUnsignedShort(ffd, &format);
		if (i1) return i1;
		i1 = ReadFormat6(fd);
		if (i1) return i1;
	}
	else if (fd->cmap_data.cmap34) {
		ffd->offset = fd->cmap.offset + fd->cmap_data.cmap34;
		i1 = readUnsignedShort(ffd, &format);
		if (i1) return i1;
		i1 = ReadFormat4(fd);
		if (i1) return i1;
	}

	FontFiles.error.error_code = 0;
	return 0;
}

static INT ReadFormat0(FONTDATA *fd) {
	INT i1;
	UINT i2, uint;
	FONTFILEDATA *ffd = &FontFiles.fontFileData[fd->ffdIndex];

	if (fd->cmap_data.Cmap != NULL) free(fd->cmap_data.Cmap);
	fd->cmap_data.Cmap = calloc(256, sizeof(fd->cmap_data.Cmap[0]));
	if (fd->cmap_data.Cmap == NULL) {
		FontFiles.error.error_code = 2;
		FontFiles.error.fioerror = 0;
		FontFiles.error.prterror = PRTERR_NOMEM;
		sprintf(FontFiles.error.msg, "calloc for Cmap table failed, size=" LUFORMATTYPE,
				256 * sizeof(fd->cmap_data.Cmap[0]));
		sprintf(FontFiles.error.function, "%s",
					__FUNCTION__
			);
		return RC_ERROR;
	}

	/*
	 * ffd->cmap.offset is the offset of the cmap tables area (the cmap header)
	 * ffd->cmap_data.cmap10 is the offset from the start of the cmap area to the Mac format 0 table.
	 * Add 6 for the 3 USHORTS in the table before the 256 BYTE array.
	 */
	ffd->offset = fd->cmap.offset + fd->cmap_data.cmap10 + 6;

	for (i2 = 0; i2 < 256; i2++) {
		fd->cmap_data.Cmap[i2].characterCode = (UCHAR)i2;
		i1 = readUnsignedByte(ffd, &uint);
		if (i1) return i1;
		fd->cmap_data.Cmap[i2].glyphIndex = uint;
	}
	fd->cmap_data.CmapSize = 256;
	return 0;
}

/**
 * Format 6 is a trimmed table mapping. It is similar to format 0 but can have
 * less than 256 entries.
 */
static INT ReadFormat6(FONTDATA *fd) {
	INT i1, i2;
	USHORT firstCode, entryCount;
	FONTFILEDATA *ffd = &FontFiles.fontFileData[fd->ffdIndex];

	if (fd->cmap_data.Cmap != NULL) free(fd->cmap_data.Cmap);

	/*
	 * ffd->cmap.offset is the offset of the cmap tables area (the cmap header)
	 * ffd->cmap_data.cmap16 is the offset from the start of the cmap area to the Mac format 6 table.
	 * Add 6 for the 3 USHORTS in the table before data.  (format, length, language)
	 */
	ffd->offset = fd->cmap.offset + fd->cmap_data.cmap16 + 6;
	i1 = readUnsignedShort(ffd, &firstCode);
	if (i1) return i1;
	i1 = readUnsignedShort(ffd, &entryCount);
	if (i1) return i1;

	fd->cmap_data.Cmap = calloc(entryCount, sizeof(fd->cmap_data.Cmap[0]));
	if (fd->cmap_data.Cmap == NULL) {
		FontFiles.error.error_code = 2;
		FontFiles.error.fioerror = 0;
		FontFiles.error.prterror = PRTERR_NOMEM;
		sprintf(FontFiles.error.msg, "calloc for Cmap table failed, size=" LUFORMATTYPE,
				entryCount * sizeof(fd->cmap_data.Cmap[0]));
		sprintf(FontFiles.error.function, "%s",
					__FUNCTION__
			);
		return RC_ERROR;
	}

	for (i2 = 0; i2 < entryCount; i2++) {
		fd->cmap_data.Cmap[i2].characterCode = (CHAR)(i2 + firstCode);
		i1 = readUnsignedShort(ffd, &fd->cmap_data.Cmap[i2].glyphIndex);
		if (i1) return i1;
	}
	fd->cmap_data.CmapSize = entryCount;
	return 0;
}

/*
 * Calls memalloc, might cause memory to move
 */
static INT ReadFormat4(FONTDATA *fd) {
	int i1, i2, i3, glyphIdCount;
	USHORT table_length, segCount, **deltas, **start_counts, **end_counts;
	int idx, glyphIdx;
	USHORT **range_offsets, **glyph_IDs;
	FONTFILEDATA *ffd = &FontFiles.fontFileData[fd->ffdIndex];

	if (fd->cmap_data.Cmap != NULL) free(fd->cmap_data.Cmap);
	/*
	 * ffd->cmap.offset is the offset of the cmap tables area (the cmap header)
	 * ffd->cmap_data.cmap34 is the offset from the start of the cmap area to the Windows format 4 table.
	 * Add 2 for the USHORT in the table before the data. (format)
	 */
	ffd->offset = fd->cmap.offset + fd->cmap_data.cmap34 + 2;
	i1 = readUnsignedShort(ffd, &table_length);
	if (i1) return i1;
	ffd->offset += 2; /* language */
	i1 = readUnsignedShort(ffd, &segCount); /* segCountX2 */
	if (i1) return i1;
	segCount /= 2;		/* VERY IMPORTANT! For some reason, the segcount in the file is times 2 */
	ffd->offset += 6;	/* searchRange, entrySelector, rangeShift */

	/*
	 * loop to read end counts
	 */
	end_counts = (USHORT**)memalloc(segCount * sizeof(USHORT), 0);
	if (end_counts == NULL) {
		FontFiles.error.error_code = 2;
		FontFiles.error.fioerror = 0;
		FontFiles.error.prterror = PRTERR_NOMEM;
		sprintf(FontFiles.error.msg, "memalloc for end_counts failed, size=" LUFORMATTYPE,
				segCount * sizeof(USHORT));
		sprintf(FontFiles.error.function, "%s",
					__FUNCTION__
			);
		return RC_ERROR;
	}
	for (i1 = 0; i1 < segCount; ++i1) {
		i2 = readUnsignedShort(ffd, &(*end_counts)[i1]);
		if (i2) return i2;
	}

	/*
	 * loop to read start counts
	 */
	ffd->offset += 2; /* reservedPad */
	start_counts = (USHORT**)memalloc(segCount * sizeof(USHORT), 0);
	if (start_counts == NULL) {
		FontFiles.error.error_code = 2;
		FontFiles.error.fioerror = 0;
		FontFiles.error.prterror = PRTERR_NOMEM;
		sprintf(FontFiles.error.msg, "memalloc for start_counts failed, size=" LUFORMATTYPE,
				segCount * sizeof(USHORT));
		sprintf(FontFiles.error.function, "%s",
					__FUNCTION__
			);
		return RC_ERROR;
	}
	for (i1 = 0; i1 < segCount; ++i1) {
		i2 = readUnsignedShort(ffd, &(*start_counts)[i1]);
		if (i2) return i2;
	}

	/*
	 * loop to read deltas
	 */
	deltas = (USHORT**)memalloc(segCount * sizeof(USHORT), 0);
	if (deltas == NULL) {
		FontFiles.error.error_code = 2;
		FontFiles.error.fioerror = 0;
		FontFiles.error.prterror = PRTERR_NOMEM;
		sprintf(FontFiles.error.msg, "memalloc for deltas failed, size=" LUFORMATTYPE,
				segCount * sizeof(USHORT));
		sprintf(FontFiles.error.function, "%s",
					__FUNCTION__
			);
		return RC_ERROR;
	}
	for (i1 = 0; i1 < segCount; ++i1) {
		i2 = readUnsignedShort(ffd, &(*deltas)[i1]);
		if (i2) return i2;
	}

	/*
	 * loop to read range_offsets
	 */
	range_offsets = (USHORT**)memalloc(segCount * sizeof(USHORT), 0);
	if (range_offsets == NULL) {
		FontFiles.error.error_code = 2;
		FontFiles.error.fioerror = 0;
		FontFiles.error.prterror = PRTERR_NOMEM;
		sprintf(FontFiles.error.msg, "memalloc for range_offsets failed, size=" LUFORMATTYPE,
				segCount * sizeof(USHORT));
		sprintf(FontFiles.error.function, "%s",
					__FUNCTION__
			);
		return RC_ERROR;
	}
	for (i1 = 0; i1 < segCount; ++i1) {
		i2 = readUnsignedShort(ffd, &(*range_offsets)[i1]);
		if (i2) return i2;
	}

	/*
	 * Loop to read glyph_IDs
	 */
	fd->cmap_data.CmapSize = 0;
	glyphIdCount = table_length / 2 - 8 - segCount * 4;
	if (glyphIdCount > 0) { /* There might not be any! */
		glyph_IDs = (USHORT**)memalloc(glyphIdCount * sizeof(USHORT), 0);
		if (glyph_IDs == NULL) {
			FontFiles.error.error_code = 2;
			FontFiles.error.fioerror = 0;
			FontFiles.error.prterror = PRTERR_NOMEM;
			sprintf(FontFiles.error.msg, "memalloc for glyph_IDs failed, size=" LUFORMATTYPE,
					glyphIdCount * sizeof(USHORT));
			sprintf(FontFiles.error.function, "%s",
					__FUNCTION__
			);
			return RC_ERROR;
		}
		for (i1 = 0; i1 < glyphIdCount; i1++) {
			i2 = readUnsignedShort(ffd, &(*glyph_IDs)[i1]);
			if (i2) return i2;
		}
	}
	else return 0;

	/*
	 * Pass 1 to determine the number of character codes so we can allocate memory
	 */
	for (i1 = 0; i1 < segCount; ++i1) {
		for (i2 = (*start_counts)[i1]; i2 <= (*end_counts)[i1] && i2 != 0xFFFF; ++i2) {
			if ((*range_offsets)[i1] != 0) {
				idx = i1 + (*range_offsets)[i1] / 2 - segCount + i2 - (*start_counts)[i1];
				if (idx >= glyphIdCount) continue;
			}
			fd->cmap_data.CmapSize++;
		}
	}
	fd->cmap_data.Cmap = calloc(fd->cmap_data.CmapSize, sizeof(fd->cmap_data.Cmap[0]));
	if (fd->cmap_data.Cmap == NULL) {
		FontFiles.error.error_code = 2;
		FontFiles.error.fioerror = 0;
		FontFiles.error.prterror = PRTERR_NOMEM;
		sprintf(FontFiles.error.msg, "calloc for Cmap failed, size=" LUFORMATTYPE,
				fd->cmap_data.CmapSize * sizeof(fd->cmap_data.Cmap[0]));
		sprintf(FontFiles.error.function, "%s",
					__FUNCTION__
			);
		return RC_ERROR;
	}

	/*
	 * Pass 2, get the data!
	 */
	i3 = 0;
	for (i1 = 0; i1 < segCount; ++i1) {
		for (i2 = (*start_counts)[i1]; i2 <= (*end_counts)[i1] && i2 != 0xFFFF; ++i2) {
			if ((*range_offsets)[i1] == 0) {
				glyphIdx = (i2 + (*deltas)[i1]) & 0xFFFF;
			}
			else {
				idx = i1 + (*range_offsets)[i1] / 2 - segCount + i2 - (*start_counts)[i1];
				if (idx >= glyphIdCount) continue;
				glyphIdx = ((*glyph_IDs)[idx] + (*deltas)[i1]) & 0xFFFF;
			}
			if (i2 > 0xff) break;
			if (i3 >= fd->cmap_data.CmapSize) {
				FontFiles.error.error_code = 2;
				FontFiles.error.fioerror = 0;
				FontFiles.error.prterror = PRTERR_INTERNAL;
				sprintf(FontFiles.error.msg, "Internal error, CmapSize=%hu, i3=%d",
						fd->cmap_data.CmapSize, i3);
				sprintf(FontFiles.error.function, "%s",
							__FUNCTION__
					);
				return RC_ERROR;
			}
			fd->cmap_data.Cmap[i3].characterCode = i2;
			fd->cmap_data.Cmap[i3++].glyphIndex = glyphIdx;
		}
	}
	if (i3 < fd->cmap_data.CmapSize) {
		void *temp = fd->cmap_data.Cmap;
		fd->cmap_data.Cmap = realloc(fd->cmap_data.Cmap, i3 * sizeof(fd->cmap_data.Cmap[0]));
		if (fd->cmap_data.Cmap == NULL) {
			if (temp != NULL) free(temp);
			FontFiles.error.error_code = 2;
			FontFiles.error.fioerror = 0;
			FontFiles.error.prterror = PRTERR_NOMEM;
			sprintf(FontFiles.error.msg, "realloc for Cmap failed, size=" LUFORMATTYPE,
					fd->cmap_data.CmapSize * sizeof(fd->cmap_data.Cmap[0]));
			sprintf(FontFiles.error.function, "%s",
						__FUNCTION__
				);
			return RC_ERROR;
		}
		fd->cmap_data.CmapSize = i3;
	}

	memfree((UCHAR**)end_counts);
	memfree((UCHAR**)start_counts);
	memfree((UCHAR**)deltas);
	memfree((UCHAR**)range_offsets);
	if (glyphIdCount > 0) memfree((UCHAR**)glyph_IDs);
	return 0;
}
