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
#define INC_STDLIB
#include "includes.h"
#include "base.h"
#include "util.h"
#include "fio.h"
#include "dbc.h"

static INT fileHasBeenTestedForExistance;
static INT permError;
static INT eorsize;

static void createUntrappedErrorLogFile(void);
#if 0 && OS_UNIX && defined(Linux)
static void logDumpVcodeLog(INT fileHandle, HANDLE osHandle);
#endif

void logUntrappedError(CHAR *errormsg, INT errornum)
{
	INT i1;
	INT fileHandle;
	UCHAR tstamp[18];
	UCHAR buffer[512];
	OFFSET fpos = 0;
#if OS_WIN32
	HANDLE osHandle;
#else
	int osHandle;
#endif

	if (permError) return;
#if OS_WIN32
	eorsize = 2;
#else
	eorsize = 1;
#endif

	if (!fileHasBeenTestedForExistance) {
		i1 = utility(UTIL_EXIST, writeUntrappedErrorsFile, -1);
		if (i1 == UTILERR_FIND) createUntrappedErrorLogFile();
		else permError = i1; // Will be zero if file exists and no error happened
		if (permError) return;
		fileHasBeenTestedForExistance = TRUE;
	}
	// Open shared read/write
	fileHandle = fioopen(writeUntrappedErrorsFile, FIO_P_WRK | FIO_M_SHR);
	if (fileHandle < 0) {
		permError = fileHandle;
		return;
	}
	// LOCK
	i1 = fioflck(fileHandle);
	if (i1) {
		permError = i1;
		return;
	}
	// WRITE
	memset(tstamp, '\0', sizeof(tstamp));
	msctimestamp(tstamp);
	strncpy((CHAR*)buffer, (CHAR*)tstamp, 17);
	strcat((CHAR*)buffer, " ");
	strcat((CHAR*)buffer, errormsg);
	// Set up End of record
	if (eorsize == 2) strcat((CHAR*)buffer, "\x0d\x0a"); // CR LF
	else strcat((CHAR*)buffer, "\x0a");	// LF
	// Append to file, seek to the end of the file
	osHandle = fiogetOSHandle(fileHandle);
	i1= fioalseek(osHandle, 0L, 2, &fpos);
	if (i1) {
		permError = i1;
		return;
	}

	fiowrite(fileHandle, fpos, buffer, strlen((CHAR*)buffer));
	// UNLOCK
	fiofulk(fileHandle);
	// CLOSE
	fioclose(fileHandle);
}

static void createUntrappedErrorLogFile()
{
	INT i1;

	/* Exclusive read/write prepare, fail if existing */
	i1 = fioopen(writeUntrappedErrorsFile, FIO_P_WRK | FIO_M_EFC);
	if (i1 == ERR_EXIST) {
		/*
		 * Another dbc process must have just created it.
		 * So just return
		 */
	}
	else if (i1 > 0) {
		/*
		 * File has been created and is now open in exclusive mode.
		 * Close it!
		 */
		fioclose(i1);
	}
	else {
		/*
		 * Some other error has occurred. Give up
		 */
		permError = i1;
	}
	return;
}
