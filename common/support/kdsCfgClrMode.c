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
#include "vid.h"
#include "xml.h"

static void setVidFlags(INT *flag);

void kdsConfigColorMode(INT *flag) {
#if OS_UNIX
	CHAR *ptr;
	if (!prpget("display", "colormode", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "ansi256"))
		*flag |= VID_FLAG_COLORMODE_ANSI256;
	else *flag |= VID_FLAG_COLORMODE_OLD;
#endif
#if OS_WIN32
	*flag |= VID_FLAG_COLORMODE_OLD;
#endif
	setVidFlags(flag);
}

void kdsConfigColorModeSC(ELEMENT *e1, INT *flag) {
#if OS_UNIX
	ELEMENT *e2;
	*flag |= VID_FLAG_COLORMODE_OLD;
	if (strcmp(e1->tag, "colormode") == 0) {
		if ((e2 = e1->firstsubelement) != NULL) {
			if (strcmp(e2->tag, "ansi256") == 0) {
				*flag |= VID_FLAG_COLORMODE_ANSI256;
			}
		}
	}
#endif
#if OS_WIN32
	*flag |= VID_FLAG_COLORMODE_OLD;
#endif
	setVidFlags(flag);
}

/**
 * So far, we are only expanding things to 64-bit on Unix. Windows remains at 32-bit.
 * ansi256 mode is not valid for Windows and cannot happen.
 */
static void setVidFlags(INT *flag) {
	if (*flag & VID_FLAG_COLORMODE_ANSI256) {
		CELLWIDTH = 4;
		DSP_FG_COLORMAX = 256;
		DSP_BG_COLORMAX = 256;
#if OS_UNIX
		DSPATTR_FGCOLORMASK	= 0x000000FF00000000LL;
		DSPATTR_BGCOLORMASK	= 0x0000FF0000000000LL;
		DSPATTR_FGCOLORSHIFT = 32;
		DSPATTR_BGCOLORSHIFT = 40;
#endif
	}
	else if (*flag & VID_FLAG_COLORMODE_OLD) {
		CELLWIDTH = 3;
		DSP_FG_COLORMAX = 16;
		DSP_BG_COLORMAX = 16;
		DSPATTR_FGCOLORMASK	= 0x00000F00;
		DSPATTR_BGCOLORMASK	= 0x0000F000;
		DSPATTR_FGCOLORSHIFT = 8;
		DSPATTR_BGCOLORSHIFT = 12;
	}
}

