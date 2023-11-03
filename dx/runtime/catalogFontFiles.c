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

#define CATFONTFILES_C_

#define INC_STDIO
#define INC_STDLIB
#define INC_STRING
#define INC_LIMITS
#include "includes.h"
#include "catfontfiles.h"
#include "base.h"
#include "prt.h"
#include "gui.h"
#include "fontfiles.h"

#if OS_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __MACOSX
#include <sys/syslimits.h>
#endif
#include <dirent.h>
#ifdef SCO5
#define PATH_MAX 256
#endif
#endif

static INT addFileToTempStruct(CHAR *cFileName, ULONG fileSize);
static INT countFiles(void);
static INT initializeTempFontFiles(void);
static CHAR *fontFileExtensions[] = {".ttf", ".otf", ".ttc"};
static size_t sizeOfFontFileExtensions = sizeof(fontFileExtensions) / sizeof(void*);

#define TEMPFONTFILE_NAME_ARRAY_START_SIZE 200
#define TEMPFONTFILE_NAME_ARRAY_INCREMENT_SIZE 100

/*
 * Temporary structure used to hold font file names while we enumerate them
 */
struct tempfontfiles_tag {
	ZHANDLE **fileNames;
	ULONG **fileSizes;
	INT sizeOfFilesArray;   /* Actual size of the fileNames array */
	INT numberOfFiles;		/* Slots in the fileNames array that are in use */
} tempFontFiles;


INT haveFontFilesBeenCataloged() {
	return (FontFiles.fontFileData != NULL);
}

/*
 * Gather a list of files in the font directories that have an extension of ttf
 * or otf
 * This routine does not open or read any actual files so it does not use 'fio'.
 * It uses OS specific calls to get file names from directories.
 * Might move memory
 */
INT catalogFontFiles() {
	INT i1;

	if ((i1 = initializeTempFontFiles()) != 0) return i1;
	if ((i1 = countFiles()) != 0) return i1;
	/**
	 * The countFiles method could report success even if no files at all are found.
	 * Watch for this! It was going through to calloc which was returning a NULL.
	 * jpr 01/16/2013
	 */
	if (tempFontFiles.numberOfFiles == 0) goto exit;
	FontFiles.fontFileData = (FONTFILEDATA*)calloc(tempFontFiles.numberOfFiles, sizeof(FONTFILEDATA));
	FontFiles.numberOfFiles = tempFontFiles.numberOfFiles;
	if (FontFiles.fontFileData == NULL)
	{
#if defined(Linux) && defined(i386)
		sprintf(FontFiles.error.msg, "calloc failed in %s (%d*%u=%u)",
#else
		sprintf(FontFiles.error.msg, "calloc failed in %s (%d*%zu=%zu)",
#endif
		__FUNCTION__ ,tempFontFiles.numberOfFiles, sizeof(FONTFILEDATA), tempFontFiles.numberOfFiles * sizeof(FONTFILEDATA));
		prtputerror(FontFiles.error.msg);
		return PRTERR_NOMEM;
	}
	for (i1 = 0; i1 < FontFiles.numberOfFiles; i1++) {
		FontFiles.fontFileData[i1].fileName = (*tempFontFiles.fileNames)[i1];
		FontFiles.fontFileData[i1].fileLength = (*tempFontFiles.fileSizes)[i1];
	}
exit:
	memfree((UCHAR**)tempFontFiles.fileNames);
	memfree((UCHAR**)tempFontFiles.fileSizes);
	return 0;
}

#if OS_UNIX
static CHAR pathSep[] = "/";
static INT traverseDir(char * dirpath);

/*
 * Unix version
 * Return zero if success, non-negative if error
 */
static INT countFiles() {
	INT i1, i2;
	CHAR *ptr;

	/*
	 * Code here to read and parse out an entry in the DX config file that has
	 * a list of directories to search.
	 */
	for (i1 = 0; ; i1 = PRP_NEXT) {
		if (prpget("file", "fonts", "dir", NULL, &ptr, i1)) break;
		/*i2 = (INT)strlen(ptr);*/
		i2 = traverseDir(ptr);
		if (i2) return i2;
	}
	return 0;
}

/*
 * Unix version
 * Return TRUE if success, FALSE if file is not kind we want.
 */
static INT isFileTypeWhatWeWant(CHAR *fullPath, OFFSET *size) {
	struct stat st;
	size_t len, i1;
	CHAR suffix[4];
	lstat(fullPath, &st);
	if (S_ISREG(st.st_mode)) {
		len = strlen(fullPath);
		memcpy(suffix, &fullPath[(len-4)], 4 * sizeof(CHAR));
		for (i1 = 0; i1 < sizeOfFontFileExtensions; i1++) {
			if (!guimemicmp(suffix, fontFileExtensions[i1], 4 * sizeof(CHAR))) {
				*size = st.st_size;
				return TRUE;
			}
		}
	}
	return FALSE;
}

static INT traverseDir(char * dirpath) {
	CHAR entry_path[PATH_MAX + 1];
	size_t pathlen;
	OFFSET fsize;
	DIR *dir;
	INT i1;
	struct dirent *entry;

	strncpy(entry_path, dirpath, sizeof(entry_path));
	pathlen=strlen(dirpath);
	if (entry_path[pathlen - 1] != pathSep[0]) {
		entry_path[pathlen] = '/';
		entry_path[pathlen + 1] = '\0';
		++pathlen;
	}
	dir = opendir(dirpath);
	if (dir == NULL) {
		return PRTERR_BADNAME;
	}
	while ((entry = readdir(dir)) != NULL) {
		strncpy(entry_path + pathlen, entry->d_name, sizeof(entry_path) - pathlen);
		if (isFileTypeWhatWeWant(entry_path, &fsize)) {
			i1 = addFileToTempStruct(entry_path, fsize);
			if (i1) return i1;
		}
	}
	closedir(dir);
	return 0;
}
#endif

#if OS_WIN32

#define SYSTEMROOT "systemroot"
static CHAR pathSep[] = "\\";
static CHAR Fonts[] = "Fonts";


/*
 * Windows version
 * Return TRUE if success, FALSE if file is not kind we want.
 */
static INT isFileTypeWhatWeWant(WIN32_FIND_DATA *ffd) {
	size_t len, i1;
	CHAR suffix[4];
	if (ffd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return FALSE;
	if (ffd->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) return FALSE;
	len = strlen(ffd->cFileName);
	if (len < 5) return FALSE;
	memcpy(suffix, &ffd->cFileName[(len-4)], 4 * sizeof(CHAR));
	for (i1 = 0; i1 < sizeOfFontFileExtensions; i1++) {
#pragma warning(suppress:4996)
		if (!_memicmp(suffix, fontFileExtensions[i1], 4 * sizeof(CHAR))) return TRUE;
	}
	return FALSE;
}

/*
 * Windows version
 * Return zero if success, non-negative if error (a PRTERR_xxx code)
 * Might move memory
 */
static INT countFiles() {
	HANDLE fhandle;
	WIN32_FIND_DATA ffd;
	CHAR **sysroot;
	size_t requiredSize;
	CHAR fontsFullPath[MAX_PATH], fontFullPath[MAX_PATH];
	INT i1;

	sprintf(FontFiles.error.msg, "%s", __FUNCTION__);
	getenv_s( &requiredSize, NULL, 0, SYSTEMROOT); // returns # of characters, not bytes
	sysroot = (CHAR**)memalloc((INT)(requiredSize * sizeof(CHAR)), 0);
	if (!sysroot)
	{
		sprintf(FontFiles.error.msg, "memalloc failed in %s", __FUNCTION__);
		prtputerror(FontFiles.error.msg);
		return PRTERR_NOMEM;
	}
	getenv_s( &requiredSize, *sysroot, requiredSize, SYSTEMROOT);
	strcpy(fontsFullPath, *sysroot);
	strcat(fontsFullPath, pathSep);
	strcat(fontsFullPath, Fonts);
	strcat(fontsFullPath, pathSep);
	strcat(fontsFullPath, "*");
	fhandle = FindFirstFile(fontsFullPath, &ffd);
	if (fhandle == INVALID_HANDLE_VALUE) {
		if ((i1 = GetLastError()) != ERROR_FILE_NOT_FOUND) {
			sprintf(FontFiles.error.msg, "FindFirstFile failed in %s(%d)", __FUNCTION__, i1);
			prtputerror(FontFiles.error.msg);
			return PRTERR_NATIVE;
		}
		return 0;
	}
	do {
		if (isFileTypeWhatWeWant(&ffd)) {
			strcpy(fontFullPath, *sysroot);
			strcat(fontFullPath, pathSep);
			strcat(fontFullPath, Fonts);
			strcat(fontFullPath, pathSep);
			strcat(fontFullPath, ffd.cFileName);
			i1 = addFileToTempStruct(fontFullPath, ffd.nFileSizeLow);
			if (i1) return i1;
		}
	} while (FindNextFile(fhandle, &ffd));
	memfree((UCHAR**)sysroot);
	return 0;
}
#endif

/*
 * Function used by both Windows and Unix
 * Return zero if success, PRTERR_NOMEM is only possible failure.
 * Calls memchange, could move memory
 */
static INT addFileToTempStruct(CHAR *cFileName, ULONG fileSize) {
	UINT newSize;
	ZHANDLE ptr;
	INT i1;

	ptr = guiAllocString((BYTE*)cFileName, (INT)strlen(cFileName));
	if (ptr == NULL) {
		sprintf(FontFiles.error.msg, "guiAllocString failed in %s",	__FUNCTION__);
		prtputerror(FontFiles.error.msg);
		return PRTERR_NOMEM;
	}
	if (tempFontFiles.numberOfFiles == tempFontFiles.sizeOfFilesArray) {
		tempFontFiles.sizeOfFilesArray += TEMPFONTFILE_NAME_ARRAY_INCREMENT_SIZE;
		newSize = tempFontFiles.sizeOfFilesArray * sizeof(void*);
		if ((i1 = memchange((UCHAR**)tempFontFiles.fileNames, newSize, 0)) != 0) {
			sprintf(FontFiles.error.msg, "memchange failed in %s(1:%d)", __FUNCTION__, i1);
			prtputerror(FontFiles.error.msg);
			return PRTERR_NOMEM;
		}
		newSize = tempFontFiles.sizeOfFilesArray * sizeof(ULONG);
		if ((i1 = memchange((UCHAR**)tempFontFiles.fileSizes, newSize, 0)) != 0) {
			sprintf(FontFiles.error.msg, "memchange failed in %s(2:%d)", __FUNCTION__, i1);
			prtputerror(FontFiles.error.msg);
			return PRTERR_NOMEM;
		}
	}
	(*tempFontFiles.fileNames)[tempFontFiles.numberOfFiles] = ptr;
	(*tempFontFiles.fileSizes)[tempFontFiles.numberOfFiles] = fileSize;
	tempFontFiles.numberOfFiles++;
	return 0;
}

/**
 * Function used by both Windows and Unix
 * Returns zero if success, PRTERR_NOMEM is only possible failure.
 */
static INT initializeTempFontFiles() {
	tempFontFiles.sizeOfFilesArray = TEMPFONTFILE_NAME_ARRAY_START_SIZE;
	tempFontFiles.fileNames = (ZHANDLE **) memalloc(tempFontFiles.sizeOfFilesArray * sizeof(void*), 0);
	tempFontFiles.fileSizes = (ULONG **) memalloc(tempFontFiles.sizeOfFilesArray * sizeof(ULONG), 0);
	if (tempFontFiles.fileNames == NULL || tempFontFiles.fileSizes == NULL)
	{
		sprintf(FontFiles.error.msg, "%s", __FUNCTION__);
		prtputerror(FontFiles.error.msg);
		return PRTERR_NOMEM;
	}
	tempFontFiles.numberOfFiles = 0;
	return 0;
}

