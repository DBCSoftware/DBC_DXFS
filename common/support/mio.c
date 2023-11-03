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
#define INC_CTYPE
#define INC_TIME
#include "includes.h"
#include "base.h"
#include "fio.h"

#if OS_UNIX
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

#if OS_WIN32
#define GETENV(p) getenv(p)
#endif

#if OS_UNIX
#define GETENV(p) getenv(p)
#endif

#define MEMBERSIZ 8
#define VOLUMESIZ 8

/* local declarations */
static CHAR volume[VOLUMESIZ + 8];
static CHAR member[MEMBERSIZ + 1];

#define MAX_EXTENSION_LENGTH 32

/*
 * Alter file name to be compatible
 * Always returns 0
 * Does not move memory
 */
INT miofixname(CHAR *nam, CHAR *ext, INT opt)
{
	INT i1, i2, i3, i4, fioflags;
	CHAR extension[MAX_EXTENSION_LENGTH + 1]; /* Allow for terminating null */
	UCHAR state;

	fioflags = fiogetflags();

	/* squeeze out blanks in filename first */
	if (fioflags & FIO_FLAG_NAMEBLANKSSQUEEZE) {
		for (i1 = i2 = 0; nam[i1]; i1++) if (nam[i1] != ' ') nam[i2++] = nam[i1];
		nam[i2] = '\0';
	}
	else {
		i2 = (INT)strlen(nam);
		if (!(fioflags & FIO_FLAG_NAMEBLANKSNOCHOP)) {
			while (i2 > 0 && nam[i2 - 1] == ' ') i2--;
			nam[i2] = '\0';
		}
	}

	extension[0] = member[0] = volume[0] = '\0';
	state = 0;
	if (nam[0] == '/' || nam[0] == '\\' || nam[0] == '.') state = 1;
	for (i1 = i2; i1--; ) {
		i3 = i2 - i1;
		if (nam[i1] == ':') {
			if (state != 0) break;
			if (!(fioflags & FIO_FLAG_NAMEBLANKSSQUEEZE)) while (nam[i1 + i3 - 1] == ' ') i3--;
			if (i3 > VOLUMESIZ + 1) break;
			memcpy(volume, &nam[i1 + 1], i3 - 1);
			volume[i3 - 1] = '\0';
			if (fiocvtvol(volume, NULL)) {
				volume[0] = '\0';
				break;
			}
			nam[i1] = '\0';
			i2 = i1;
			state = 1;
		}
		else if (nam[i1] == '.') {
			if (state == 2 || i3 > MAX_EXTENSION_LENGTH) break;
			if ((fioflags & (FIO_FLAG_COMPATRMS | FIO_FLAG_COMPATRMSX | FIO_FLAG_COMPATRMSY)) && i3 == 5) {
				i4 = (INT)strlen(&nam[i1 + 1]);
				if (i4 > MAX_EXTENSION_LENGTH) i4 = MAX_EXTENSION_LENGTH;
				strncpy(extension, &nam[i1 + 1], i4);
				extension[i4] = '\0';
				for (i3 = 0; i3 < 4; i3++) extension[i3] = (UCHAR) toupper(extension[i3]);
				if (!strcmp(extension, "TEXT")) strcpy(extension, ".TXT");
				else if (!strcmp(extension, "ISAM")) strcpy(extension, ".ISI");
				else {
					i4 = (INT)strlen(&nam[i1]);
					if (i4 > MAX_EXTENSION_LENGTH) i4 = MAX_EXTENSION_LENGTH;
					strncpy(extension, &nam[i1], i4 + 1);
					if (fioflags & (FIO_FLAG_COMPATRMS | FIO_FLAG_COMPATRMSX)) extension[4] = '\0';
				}
			}
			else {
				i4 = (INT)strlen(&nam[i1]);
				if (i4 > MAX_EXTENSION_LENGTH) i4 = MAX_EXTENSION_LENGTH;
				strncpy(extension, &nam[i1], i4 + 1);
			}
			nam[i1] = '\0';
			i2 = i1;
			state = 2;
		}
		else if (nam[i1] == '(') {
			if ((state == 0 || (state == 1 && !volume[0])) && nam[i2 - 1] == ')' && i3 <= MEMBERSIZ + 2) {
				memcpy(member, &nam[i1 + 1], i3 - 2);
				member[i3 - 2] = '\0';
				nam[i1] = '\0';
				i2 = i1;
			}
		}
		else if (nam[i1] == '/') {
			if (!(fioflags & (FIO_FLAG_COMPATDOS | FIO_FLAG_COMPATRMS
				| FIO_FLAG_COMPATRMSX | FIO_FLAG_COMPATRMSY)) || i3 > 5) break;
			if (nam[0] == '/' || nam[0] == '\\' || nam[0] == '.') break;
			nam[i1++] = '.';
		}
		else if (nam[i1] == '\\') break;
	}
	if (i1 < 0 && i2 > 8 && (fioflags & FIO_FLAG_COMPATRMS)) nam[8] = '\0';

	if (((opt & FIXNAME_EXT_ADD) && !extension[0]) || ((opt & FIXNAME_EXT_REPLACE) && extension[0])) {
		if (!member[0]) strcpy(extension, ext);
		if (fioflags & FIO_FLAG_EXTCASEUPPER) for (i1 = 0; extension[i1]; i1++) extension[i1] = (CHAR) toupper(extension[i1]);
	}
 	if ((fioflags & FIO_FLAG_NAMEBLANKSSQUEEZE) || !(fioflags & FIO_FLAG_NAMEBLANKSNOCHOP)) {
		for (i1 = (INT)strlen(nam); i1 && nam[i1 - 1] == ' '; i1--);
		nam[i1] = '\0';
		for (i1 = (INT)strlen(extension); i1 && extension[i1 - 1] == ' '; i1--);
		extension[i1] = '\0';
	}
	strcat(nam, extension);
	if ((fioflags & (FIO_FLAG_NAMECASEUPPER | FIO_FLAG_NAMECASELOWER | FIO_FLAG_PATHCASEUPPER | FIO_FLAG_PATHCASELOWER))
	    && ((opt & FIXNAME_EXT_ADD) || !(opt & FIXNAME_EXT_REPLACE))) {
#if OS_WIN32
		for (i1 = (INT)strlen(nam) - 1; i1 >= 0 && nam[i1] != '\\' && nam[i1] != '/' && nam[i1] != ':'; i1--) {
			if (fioflags & FIO_FLAG_NAMECASEUPPER) nam[i1] = (CHAR) toupper(nam[i1]);
			else if (fioflags & FIO_FLAG_NAMECASELOWER) nam[i1] = tolower(nam[i1]);
		}
#endif
#if OS_UNIX
		for (i1 = (INT)strlen(nam) - 1; i1 >= 0 && nam[i1] != '/' && nam[i1] != '\\'; i1--) {
			if (fioflags & FIO_FLAG_NAMECASEUPPER) nam[i1] = (CHAR) toupper(nam[i1]);
			else if (fioflags & FIO_FLAG_NAMECASELOWER) nam[i1] = tolower(nam[i1]);
		}
#endif
		if (fioflags & (FIO_FLAG_PATHCASEUPPER | FIO_FLAG_PATHCASELOWER)) {
			for (i1--; i1 >= 0; i1--) {
				if (fioflags & FIO_FLAG_PATHCASEUPPER) nam[i1] = (CHAR) toupper(nam[i1]);
				else nam[i1] = tolower(nam[i1]);
			}
		}
	}
	if (volume[0] && !(opt & FIXNAME_PAR_DBCVOL)) {
		i1 = (INT)strlen(nam);
		nam[i1++] = ':';
		strcpy(&nam[i1], volume);
	}
	if (member[0] && !(opt & FIXNAME_PAR_MEMBER)) {
		i1 = (INT)strlen(nam);
		nam[i1++] = '(';
		strcpy(&nam[i1], member);
		i1 += (INT)strlen(member);
		nam[i1++] = ')';
	}
	return 0;
}

/**
 * Returns volume in arg1 and member name in arg two
 * Does not move memory
 */
void miogetname(CHAR **vol, CHAR **mem)
{
	*vol = volume;
	*mem = member;
}

/**
 * Return 0 if success, 1 if the var cannot be found
 * Does not move memory
 */
INT miogetenv(CHAR *var, CHAR **pptr)
{
	CHAR *ptr;

	ptr = (CHAR *) GETENV(var);
	if (ptr != NULL) {
		*pptr = ptr;
		return 0;
	}
	return 1;
}

/**
 * scan semi-colon delimited token into dest
 * return 0 if something found, 1 if no more
 * Does not move memory
 */
INT miostrscan(CHAR *source, CHAR *dest)
{
	INT retflg;

	retflg = 1;
	while (*source == ' ') source++;
	while (*source && *source != ';') {
		*dest++ = *source;
		*source++ = ' ';
		retflg = 0;
	}
	if (*source == ';') *source = ' ';
	*dest = '\0';
	return(retflg);
}

/* return bytes from the start of a file */
INT miogetfilebytes(CHAR *filename, UCHAR *memory, INT length)
{
	FHANDLE handle;
	INT count;
	CHAR workname[MAX_NAMESIZE];

	strcpy(workname, filename);  /* fioaopen does slash conversion */
	if (fioaopen(workname, FIO_M_SRO, 2, &handle)) return(ERR_FNOTF);
	fioaread(handle, memory, length, 0, &count);
	fioaclose(handle);
	if (count != length) return RC_ERROR;
	return(0);
}

/* return time stamp of last file change */
INT miogetfilechgtime(CHAR *filename, UCHAR *timestamp)
{
#if OS_WIN32
	INT i1;
	HANDLE handle;
	WIN32_FIND_DATA fileinfo;
	SYSTEMTIME systime;

	for (i1 = 0; filename[i1] && filename[i1] != '*' && filename[i1] != '?'; i1++);
	if (filename[i1]) return RC_ERROR;
	handle = FindFirstFile(filename, &fileinfo);
	if (handle == INVALID_HANDLE_VALUE) return RC_ERROR;
	FindClose(handle);

	if (!FileTimeToSystemTime(&fileinfo.ftLastWriteTime, &systime)) return RC_ERROR;
	i1 = systime.wYear;
	timestamp[0] = i1 / 1000 + '0';
	i1 %= 1000;
	timestamp[1] = i1 / 100 + '0';
	i1 %= 100;
	timestamp[2] = i1 / 10 + '0';
	timestamp[3] = i1 % 10 + '0';
	timestamp[4] = systime.wMonth / 10 + '0';
	timestamp[5] = systime.wMonth % 10 + '0';
	timestamp[6] = systime.wDay / 10 + '0';
	timestamp[7] = systime.wDay % 10 + '0';
	timestamp[8] = systime.wHour / 10 + '0';
	timestamp[9] = systime.wHour % 10 + '0';
	timestamp[10] = systime.wMinute / 10 + '0';
	timestamp[11] = systime.wMinute % 10 + '0';
	timestamp[12] = systime.wSecond / 10 + '0';
	timestamp[13] = systime.wSecond % 10 + '0';
	i1 = systime.wMilliseconds / 10;
	timestamp[14] = i1 / 10 + '0';
	timestamp[15] = i1 % 10 + '0';
#endif

#if OS_UNIX
	INT i1;
	time_t time1;
	struct stat statbuf;
	struct tm *today;

	for (i1 = 0; filename[i1] && filename[i1] != '*' && filename[i1] != '['; i1++);
	if (filename[i1]) return RC_ERROR;
	if (stat(filename, &statbuf) == -1) return RC_ERROR;
	time1 = (time_t) statbuf.st_mtime;
	today = localtime(&time1);

	i1 = today->tm_year + 1900;
	timestamp[0] = i1 / 1000 + '0';
	i1 %= 1000;
	timestamp[1] = i1 / 100 + '0';
	i1 %= 100;
	timestamp[2] = i1 / 10 + '0';
	timestamp[3] = i1 % 10 + '0';
	i1 = today->tm_mon + 1;
	timestamp[4] = i1 / 10 + '0';
	timestamp[5] = i1 % 10 + '0';
	timestamp[6] = today->tm_mday / 10 + '0';
	timestamp[7] = today->tm_mday % 10 + '0';
	timestamp[8] = today->tm_hour / 10 + '0';
	timestamp[9] = today->tm_hour % 10 + '0';
	timestamp[10] = today->tm_min / 10 + '0';
	timestamp[11] = today->tm_min % 10 + '0';
	timestamp[12] = today->tm_sec / 10 + '0';
	timestamp[13] = today->tm_sec % 10 + '0';
	timestamp[14] = timestamp[15] = '0';
#endif

	return(0);
}
