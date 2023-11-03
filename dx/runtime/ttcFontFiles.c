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
#define INC_STRING
#define INC_TIME
#include "includes.h"
#include "base.h"
#include "gui.h"
#include "fontfiles.h"
#include "fio.h"
#include "prt.h"

static void localReadUlong(UCHAR *ch, ULONG *dest);
static void localWriteUlong(UCHAR *dest, ULONG src);

/*
 * Contains routines to synthesize a TTF file in memory
 * from a TTC
 */

/*
 * Calls memalloc, could move memory
 */
INT makeTtfFromTtc(FONTDATA *fd, ULONG *bufLength, UCHAR ***buffer) {
	INT i1, i2;
	FONTFILEDATA *ffd = &FontFiles.fontFileData[fd->ffdIndex];
	ULONG offsetInTtc, overallFileSize, tableRecordsSize, destOffset;
	typedef struct tth_tag {
		CHAR ctag[4];	/* 4 -byte identifier */
		ULONG checkSum;	/* CheckSum for this table */
		ULONG offset;	/* Offset from beginning of TrueType font file */
		ULONG length;	/* Length of this table */
	} TABLERECORD;
	TABLERECORD **tableRecords;
	UCHAR **tempTableRecords, **ttfFile;

	if (ffd->offsetTable == NULL) {
		FontFiles.error.error_code = 2;
		FontFiles.error.fioerror = 0;
		FontFiles.error.prterror = PRTERR_BADOPT;
		sprintf(FontFiles.error.msg, "%s not a ttc", (CHAR*)guiMemToPtr(ffd->fileName));
		sprintf(FontFiles.error.function, "%s", __FUNCTION__);
		return RC_ERROR;
	}
	offsetInTtc = ffd->offsetTable[fd->ttcIndex];
	/**
	 * How to calculate the length of the file?
	 * Every ttf has 12 bytes at the beginning, the 'Offset Table'
	 * plus 16 times numTables. The 'Table Record' entries
	 */
	tableRecordsSize = fd->numTables * 4 * sizeof(ULONG);
	overallFileSize = 12 + tableRecordsSize;
	tempTableRecords = memalloc(tableRecordsSize, MEMFLAGS_ZEROFILL);
	i1 = openFontFile(fd);
	if (i1) return RC_ERROR;
	i1 = fioread(ffd->fileHandle, offsetInTtc + 12, *tempTableRecords, tableRecordsSize);
	if (i1 < 0) {
		FontFiles.error.error_code = 1;
		FontFiles.error.fioerror = i1;
		strcpy(FontFiles.error.msg, fioerrstr(i1));
		sprintf(FontFiles.error.function, "%s", __FUNCTION__);
		return RC_ERROR;
	}
	tableRecords = (TABLERECORD **)memalloc(tableRecordsSize, MEMFLAGS_ZEROFILL);
	if (tableRecords == NULL) return RC_NO_MEM;

	/*
	 * The table length recorded is the actual length. But zero-filled padding
	 * is added to the end to round it up to a multiple of four.
	 * So add in now all the table lengths, rounded up a bit.
	 */
	for (i1 = 0; i1 < fd->numTables; i1++) {
		TABLERECORD *tr = &(*tableRecords)[i1];
		ULONG rlength;
		memcpy(tr->ctag, (*tempTableRecords) + i1 * 4 * sizeof(ULONG), 4);
		localReadUlong((*tempTableRecords) + i1 * 4 * sizeof(ULONG) + 4, &tr->checkSum);
		localReadUlong((*tempTableRecords) + i1 * 4 * sizeof(ULONG) + 8, &tr->offset);
		localReadUlong((*tempTableRecords) + i1 * 4 * sizeof(ULONG) + 12, &tr->length);
		rlength = (tr->length + 3) & (ULONG)(~0x0003);
		overallFileSize += rlength;
	}
	memfree(tempTableRecords);
	*bufLength = overallFileSize;
	ttfFile = memalloc(overallFileSize, MEMFLAGS_ZEROFILL);
	if (ttfFile == NULL) return RC_NO_MEM;

	/*
	 * Write the first twelve bytes from the ttf-inside-ttc file to our memory file
	 */
	i1 = fioread(ffd->fileHandle, offsetInTtc, *ttfFile, 12);
	if (i1 < 0) {
		FontFiles.error.error_code = 1;
		FontFiles.error.fioerror = i1;
		strcpy(FontFiles.error.msg, fioerrstr(i1));
		sprintf(FontFiles.error.function, "%s", __FUNCTION__);
		return RC_ERROR;
	}

	/*
	 * We defer writing out the table of tables for now. We have to fixup the offsets first
	 */
	destOffset = 12 + tableRecordsSize; /* This is where the next table goes in ttfFile */
	for (i1 = 0; i1 < fd->numTables; i1++) {
		ULONG rlength;
		TABLERECORD *tr = &(*tableRecords)[i1];
		i2 = fioread(ffd->fileHandle, tr->offset, *ttfFile + destOffset, tr->length);
		if (i2 < 0) {
			FontFiles.error.error_code = 1;
			FontFiles.error.fioerror = i2;
			strcpy(FontFiles.error.msg, fioerrstr(i2));
			sprintf(FontFiles.error.function, "%s", __FUNCTION__);
			return RC_ERROR;
		}
		tr->offset = destOffset;
		rlength = (tr->length + 3) & (ULONG)(~0x0003);
		destOffset += rlength;
	}

	/*
	 * Now write out the table of tables
	 */
	destOffset = 12;
	for (i1 = 0; i1 < fd->numTables; i1++) {
		TABLERECORD *tr = &(*tableRecords)[i1];
		memcpy(*ttfFile + destOffset, tr->ctag, 4);
		destOffset += 4;
		localWriteUlong(*ttfFile + destOffset, tr->checkSum);
		destOffset += 4;
		localWriteUlong(*ttfFile + destOffset, tr->offset);
		destOffset += 4;
		localWriteUlong(*ttfFile + destOffset, tr->length);
		destOffset += 4;
	}

	memfree((UCHAR**)tableRecords);
	fioclose(ffd->fileHandle);
	ffd->fileHandle = -1;
	*buffer = ttfFile;
	return 0;
}

static void localReadUlong(UCHAR *ch, ULONG *dest) {
	*dest = (ULONG)((ch[0] << 24) + (ch[1] << 16) + (ch[2] << 8) + ch[3]);
}

static void localWriteUlong(UCHAR *dest, ULONG src) {
	dest[0] = (UCHAR)((src & 0xFF000000) >> 24);
	dest[1] = (UCHAR)((src & 0x00FF0000) >> 16);
	dest[2] = (UCHAR)((src & 0x0000FF00) >> 8);
	dest[3] = src & 0x000000FF;
}
