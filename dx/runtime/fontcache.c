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
 ******************************************************************************
 *
 * Font creation and caching functions
 * This module is used only by the windows builds of the DX runtime.
 * It is used by dbcc.exe for printing only.
 *
 */

#define INC_STRING
#define INC_STDIO
#include "includes.h"
#include "base.h"

typedef struct cacheItem_tag {
	LOGFONT lf;
	HFONT hf;
	UCHAR lastused[16];
} CACHEITEM;

#define MAXCACHEITEMS 50
static CACHEITEM cache[MAXCACHEITEMS];
static INT cacheSize = 0;

static HFONT findCachedFont(LOGFONT *lf);
static HFONT createAndCacheFont(LOGFONT *lf);
static void compressCache(void);
static INT findLeastRecentlyUsedFont(void);
static void squeezeOutFont(INT index);

HFONT getFont(LOGFONT *lf) {
	HFONT hf;
	if ((hf = findCachedFont(lf)) != NULL) return hf;
	return createAndCacheFont(lf);
}

static HFONT findCachedFont(LOGFONT *lf) {
	INT i1;
	for (i1 = 0; i1 < cacheSize; i1++) {
		if (lf->lfHeight == cache[i1].lf.lfHeight &&
				lf->lfWidth == cache[i1].lf.lfWidth &&
				lf->lfEscapement == cache[i1].lf.lfEscapement &&
				lf->lfOrientation == cache[i1].lf.lfOrientation &&
				lf->lfWeight == cache[i1].lf.lfWeight &&
				lf->lfItalic == cache[i1].lf.lfItalic &&
				lf->lfUnderline == cache[i1].lf.lfUnderline &&
				lf->lfStrikeOut == cache[i1].lf.lfStrikeOut &&
				lf->lfCharSet == cache[i1].lf.lfCharSet &&
				lf->lfOutPrecision == cache[i1].lf.lfOutPrecision &&
				lf->lfClipPrecision == cache[i1].lf.lfClipPrecision &&
				lf->lfQuality == cache[i1].lf.lfQuality &&
				lf->lfPitchAndFamily == cache[i1].lf.lfPitchAndFamily &&
				!strcmp(lf->lfFaceName, cache[i1].lf.lfFaceName))
		{
			msctimestamp(cache[i1].lastused);
			return cache[i1].hf;
		}
	}
	return NULL;
}

static HFONT createAndCacheFont(LOGFONT *lf) {
	HFONT hf;
	if (cacheSize == MAXCACHEITEMS) compressCache();
	if (lf->lfHeight > 0) {
		int size = 96;
		lf->lfHeight = -((lf->lfHeight * size) / 72);
	}
	hf = CreateFontIndirect(lf);
	cache[cacheSize].lf = *lf;
	cache[cacheSize].hf = hf;
	msctimestamp(cache[cacheSize].lastused);
	cacheSize++;
	return hf;
}

static void compressCache(void) {
	INT i1, i2;
	for (i1 = 0; i1 < MAXCACHEITEMS / 10; i1++) {
		i2 = findLeastRecentlyUsedFont();
		if (i2 < 0) break;
		squeezeOutFont(i2);
	}
}

static INT findLeastRecentlyUsedFont(void) {
	INT i1, i2 = -1;
	UCHAR lowdate[] = "9999999999999999";
	for (i1 = 0; i1 < cacheSize; i1++) {
		if (memcmp(cache[i1].lastused, lowdate, 16) < 0) {
			memcpy(lowdate, cache[i1].lastused, 16);
			i2 = i1;
		}
	}
	return i2;
}

/**
 * <param name='index'>zero-based index of the cached font object to be removed from the cache</param>
 * <remarks>
 * <paramref name="index"/> is zero based, <c>cacheSize</c> is one based
 * </remarks>
 */
static void squeezeOutFont(INT index) {
	DeleteObject(cache[index].hf);
	/* Is index pointing to the last entry? */
	if (index != cacheSize - 1) {
		memmove((void*)&cache[index], (void*)&cache[index + 1], (cacheSize - index - 1) * sizeof(CACHEITEM));
	}
	cacheSize--;
}
