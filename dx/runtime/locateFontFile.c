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
#include "fio.h"
#include "prt.h"
#include "gui.h"
#include "fontfiles.h"
#include "catfontfiles.h"
#include "procfontfiles.h"

/**
 * Return zero if successful. Information in ffh will be set.
 *
 * If fail, return one of the PRTERR_xxx codes. ffh->fontfilesIndex will be -1
 * Might move memory
 */
INT locateFontFileByName(CHAR* fontFamilyName, INT bold, INT italic, FONTFILEHANDLE* ffh) {
	INT i1, i2, len1, len2;
	FONTDATA *fd;
	TTFONTNAME *ttfn;
	void *vptr1, *vptr2;

	if (!haveFontFilesBeenCataloged()) {
		i1 = catalogFontFiles();
		if (i1) return i1;
		i1 = processFontFiles();
		if (i1) {
			if (FontFiles.error.error_code == 2) {
				if (FontFiles.error.msg[0] != '\0') prtputerror(FontFiles.error.msg);
				return FontFiles.error.prterror;
			}
			else if (FontFiles.error.error_code == 1) {
				if (FontFiles.error.msg[0] != '\0') prtputerror(FontFiles.error.msg);
				switch (FontFiles.error.fioerror) {
				case ERR_FNOTF:
					return PRTERR_OPEN;

				case ERR_NOACC:
					return PRTERR_ACCESS;

				case ERR_BADNM:
					return PRTERR_BADNAME;

				case ERR_NOMEM:
					return PRTERR_NOMEM;

				default:
					return PRTERR_NATIVE;
				}
			}
		}
	}

	ffh->fontIndex = -1;  /* Start with 'not found' */
	for (i1 = 0; i1 < FontFiles.numberOfFonts; i1++) {
		if (bold != IS_FONT_BOLD(i1)) continue;
		if (italic != IS_FONT_ITALIC(i1)) continue;
		if (!IS_FONT_EMBEDABLE(i1)) continue;
		fd = &FontFiles.fontData[i1];
		for (i2 = 0; i2 < fd->numberOfNames; i2++) {
			ttfn = &fd->names[i2];
			if (ttfn->nameID != FONTFAMILYNAMEID) continue;
			vptr1 = fontFamilyName;
			guiLockMem(ttfn->canonicalName);
			vptr2 = guiMemToPtr(ttfn->canonicalName);
			len1 = (INT)strlen(vptr1);
			len2 = (INT)strlen(vptr2);
			if (len1 == len2 && memcmp(vptr1, vptr2, len1) == 0) {
				ffh->fontIndex = i1;
				ffh->ttnameIndex = i2;
				guiUnlockMem(ttfn->canonicalName);
				goto locate_exit;
			}
			guiUnlockMem(ttfn->canonicalName);
		}
	}

locate_exit:
	return 0;
}

