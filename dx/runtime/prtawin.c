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
#define INC_CTYPE
#define INC_STDLIB
#define INC_MATH
#include "includes.h"
#include "base.h"
#include "fio.h"
#include "gui.h"
#include "guix.h"
#include "dbcmd5.h"
#include "fontfiles.h"
#include "prt.h"
#include "prtx.h"
#include "fontcache.h"
#include <windowsx.h>
#if _MSC_VER >= 1800
#include <VersionHelpers.h>
#endif

typedef struct startdocstruct {
	HANDLE handle;
	DOCINFO *docinfo;
	UINT errval;
} STARTDOCINFO;

typedef struct {
	HANDLE threadhandle;
	HANDLE filehandle;
	HANDLE printhandle;
	INT type;
	INT flags;
	INT copies;
	CHAR filename[128];
} THREADINFO;

extern PRINTINFO **printtabptr;

static INT printbandingflag;
static THREADINFO **threadinfo;
static INT threadcnt;
static INT threadmax;
static UCHAR threadstop;
static CHAR printerrstr[256]; // @suppress("Unused variable declaration in file scope")

static DWORD printfile(DWORD);
static INT printbitblt(HDC, INT, INT, INT, INT, INT, INT, INT, INT, HBITMAP, HPALETTE, INT);
static INT getdevname(CHAR *, INT, HGLOBAL *);
static INT getnames(HGLOBAL, CHAR *, CHAR *, CHAR *);
static INT parsefont(CHAR *, HDC, LOGFONT *);

INT prtainit(PRTINITINFO *initinfo)
{
/*** MAY NEED TO KNOW IF CONSOLE OR NOT ***/
	return 0;
}

INT prtaexit()
{
	INT i1;

	if (threadcnt) {
		threadstop = TRUE;
		for (i1 = 0; i1 < threadcnt; i1++) {
			if ((*threadinfo + i1)->threadhandle == NULL) continue;
			WaitForSingleObject((*threadinfo + i1)->threadhandle, INFINITE);
			CloseHandle((*threadinfo + i1)->threadhandle);
		}
		threadcnt = 0;
	}
	return 0;
}

/*** CODE: SHOULD THIS RETURN LPT1 OR PRINT MANAGERS DEFAULT ***/
INT prtadefname(CHAR *prtname, INT size)
{
	INT len;
	CHAR *ptr;

	ptr = "LPT1";
	len = (INT)strlen(ptr) + 1;
	if (size > 0) {
		if (size > len) size = len;
		memcpy(prtname, ptr, size - 1);
		prtname[size - 1] = '\0';
	}
	return len;
}

/*
 * If the name is 'DEFAULT' then return PRTFLAG_TYPE_PRINTER.
 * If the name is 'DEFAULT(nnn)' then return PRTFLAG_TYPE_DRIVER.
 * If the name is 'PRN', 'AUX', 'NUL', 'LPTn', 'LPTnn', 'COMn', or 'COMnn' then return PRTFLAG_TYPE_DEVICE.
 * If the name is found in the printers' folder;
 *			and it has (nnn) after it, then return PRTFLAG_TYPE_DRIVER.
 * 			and it does not have (nnn) after it, then return PRTFLAG_TYPE_PRINTER.
 * Else return PRTFLAG_TYPE_FILE.
 */
INT prtanametype(CHAR *prtname)
{
	INT i1, i2, typeflag;
	CHAR prtwork[MAX_NAMESIZE], *ptr;
	INT allDigits;

	memset(prtwork, '\0', MAX_NAMESIZE);
	//if (IsWindows10OrGreater()) typeflag = PRTFLAG_TYPE_DRIVER;
	/*else*/ typeflag = PRTFLAG_TYPE_PRINTER;
	/* assumes trailing blanks are already chopped off */
	for (i1 = i2 = 0; prtname[i1]; i1++) prtwork[i2++] = toupper(prtname[i1]);
	if (i2 > 0 && (prtwork[i2 - 1] == '.' || prtwork[i2 - 1] == ':')) {
		i2--;
		if (!i2) return 0;
		prtwork[i2] = '\0';
	}
	else if (i2 && prtwork[i2 - 1] == ')') {
		/* The LAST char of the name is a right parens */
		allDigits = TRUE; /* Start by assuming it is a DPI */
		for (i1 = i2 - 1; i1 && prtwork[i1 - 1] != '('; i1--) {
			if (!isdigit((int)prtwork[i1 - 1]) && prtwork[i1 - 1] != ' ') allDigits = FALSE;
		}
		if (i1 && prtwork[i1 - 1] == '(') {
			for (i2 = i1 - 1; i2 && isspace((int)prtwork[i2 - 1]); i2--);
			typeflag = PRTFLAG_TYPE_DRIVER;
		}
		if (!i2) return 0;
		if (allDigits) prtwork[i2] = '\0';
	}
	else prtwork[i2] = '\0';

	if (!strcmp("PRN", prtwork)) return PRTFLAG_TYPE_DEVICE;
	if (!strcmp("AUX", prtwork)) return PRTFLAG_TYPE_DEVICE;
	if (!strcmp("NUL", prtwork)) return PRTFLAG_TYPE_DEVICE;
	if ((!memcmp(prtwork, "LPT", 3) || !memcmp(prtwork, "COM", 3)) &&
	    isdigit((int)prtwork[3]) && (!prtwork[4] || (isdigit((int)prtwork[4]) && !prtwork[5])))
			return PRTFLAG_TYPE_DEVICE;
	if (!strcmp("DEFAULT", prtwork)) return typeflag;

	ptr = prtwork;
	i1 = prtagetprinters(PRTGET_PRINTERS_FIND | PRTGET_PRINTERS_ALL, &ptr, 0);
	if (i1 == 0) {
		return typeflag;  /* found it */
	}
/*** CODE: IF i1 != -1 THEN IT IS AN ERROR VALUE, BUT JUST RETURN FILE TYPE FOR NOW ***/
	return PRTFLAG_TYPE_FILE;
}

/**
 * Possible return values;
 * 	0 for success
 *	RC_ERROR if the buffer is not large enough
 * 	PRTERR_NATIVE if a call to a win32 API goes bad
 * 	PRTERR_NOMEM if a request for memory goes bad, of any kind
 */
INT prtagetprinters(INT type, CHAR **buffer, UINT size)
{
	INT i1, i2, flags;
	DWORD bytes, cnt, level, len;
	CHAR work[1024], *ptr;
	UCHAR **pptr;
	LPPRINTER_INFO_4 printinfo4;

	len = 0;
	if (type & PRTGET_PRINTERS_DEFAULT) {
		if (GetProfileString("windows", "device", "", work, sizeof(work)) > 0) {  /* WinNT */
			if (strtok(work, ",") != NULL) {
				for (len = (DWORD) strlen(work); len && work[len - 1] == ' '; len--);
				if (len) {
					work[len] = '\0';
					if (type & PRTGET_PRINTERS_FIND) {
						if (!_stricmp(work, *buffer)) return 0;
					}
					else {
						if (len >= size) return RC_ERROR;
						memcpy(*buffer, work, ++len);
					}
				}
			}
		}
		if (!len) type &= ~PRTGET_PRINTERS_DEFAULT;
	}

	if (type & PRTGET_PRINTERS_ALL) {
		flags = PRINTER_ENUM_LOCAL;
		flags |= PRINTER_ENUM_CONNECTIONS;
		level = 4;
		if (!EnumPrinters(flags, NULL, level, NULL, 0, &bytes, &cnt)) {
			i1 = GetLastError();
			if (i1 != ERROR_INSUFFICIENT_BUFFER) {
				if (i1 != ERROR_INVALID_NAME) {
					prtaerror("EnumPrinters", i1);
					return PRTERR_NATIVE;
				}
				bytes = 0;
			}
		}
		if (bytes) {
			pptr = memalloc(bytes, 0);
			if (pptr == NULL) return PRTERR_NOMEM;
			if (EnumPrinters(flags, NULL, level, (LPBYTE) *pptr, bytes, &bytes, &cnt)) {
				printinfo4 = (LPPRINTER_INFO_4) *pptr;
				for (i1 = 0; (DWORD) i1 < cnt; i1++) {  /* try to find exact match */
					ptr = printinfo4[i1].pPrinterName;
					if (ptr == NULL) continue;
					for (i2 = (INT)strlen(ptr); i2 && ptr[i2 - 1] == ' '; i2--);
					if (!i2) continue;
					ptr[i2] = '\0';
					if (type & (PRTGET_PRINTERS_DEFAULT | PRTGET_PRINTERS_FIND)) {
						if (!_stricmp(ptr, *buffer)) {
							if (type & PRTGET_PRINTERS_FIND) {
								memfree(pptr);
								return 0;
							}
							continue;
						}
						if (type & PRTGET_PRINTERS_FIND) continue;
					}
					if (len + i2 >= size) {
						memfree(pptr);
						return RC_ERROR;
					}
					memcpy(*buffer + len, ptr, ++i2);
					len += i2;
				}
			}
			else {
				i1 = GetLastError();
				if (i1 != ERROR_INVALID_NAME) {  /* just in case */
					prtaerror("EnumPrinters", i1);
					memfree(pptr);
					return PRTERR_NATIVE;
				}
			}
			memfree(pptr);
		}
	}
	if (type & PRTGET_PRINTERS_FIND) {
		return RC_ERROR;
	}
	if (!len || (type & PRTGET_PRINTERS_ALL)) {
		if (len >= size) return RC_ERROR;
		(*buffer)[len] = '\0';
	}
	return 0;
}

/*
 * The prtagetpaperbins and prtagetpapernames functions
 * have identical start up code.
 * They both use this macro.
 */
#define PRTAGETPAPERINIT()\
		len = 0;\
		if (!prtname[0] || !_tcsicmp(prtname, "DEFAULT")) {\
			i1 = getdefdevname(&namehandle);\
			if (i1) {\
				if (i1 != -1) return i1;\
				prtputerror("no default name");\
				return PRTERR_BADOPT;\
			}\
		}\
		else {\
			i1 = prtagetprinters(PRTGET_PRINTERS_FIND | PRTGET_PRINTERS_ALL, &prtname, 0);\
			if (!i1) i1 = getdevname(prtname, FALSE, &namehandle);\
			if (i1) {\
				if (i1 != -1) return i1;\
				prtputerror("non-existent printer name");\
				return PRTERR_BADOPT;\
			}\
		}\
		/* load names into buffers */\
		if (getnames(namehandle, device, driver, port) == RC_ERROR) {\
			GlobalFree(namehandle);\
			if (len >= size) return RC_ERROR;\
			(*buffer)[len] = '\0';\
			return 0;\
		}

/*
 * The prtagetpaperbins and prtagetpapernames functions
 * both use this to move the data returned by DeviceCapabilities into
 * the db/c character array
 */
#define PRTAGETPAPERSTORE(memfactor)\
		for (i1 = 0; i1 < (INT) rc; ptr += (memfactor), i1++) {\
			for (i2 = (INT)strlen(ptr); i2 && ptr[i2 - 1] == ' '; i2--);\
			if (!i2) continue;\
			ptr[i2] = '\0';\
			if (len + i2 >= size) {\
				memfree(pptr);\
				GlobalFree(namehandle);\
				return RC_ERROR;\
			}\
			memcpy(*buffer + len, ptr, ++i2);\
			len += i2;\
		}\
		memfree(pptr);


INT prtagetpapernames(CHAR *prtname, CHAR **buffer, INT size)
{
	INT i1, i2, len;
	DWORD rc;
	CHAR device[80], driver[80], port[80], *ptr;
	UCHAR **pptr;
	HGLOBAL namehandle;

	PRTAGETPAPERINIT()

	rc = DeviceCapabilities((LPSTR) device, (LPSTR) port, DC_PAPERNAMES, (LPSTR) NULL, (LPDEVMODE) NULL);
	if ((INT) rc > 0) {
		pptr = memalloc(rc * 64, 0);
		if (pptr == NULL) {
			GlobalFree(namehandle);
			return PRTERR_NOMEM;
		}
		ptr = (CHAR *) *pptr;
		rc = DeviceCapabilities((LPSTR) device, (LPSTR) port, DC_PAPERNAMES, (LPSTR) ptr, (LPDEVMODE) NULL);
		PRTAGETPAPERSTORE(64)
	}

	GlobalFree(namehandle);
	if (len >= size) return RC_ERROR;
	(*buffer)[len] = '\0';
	return 0;
}

INT prtagetpaperbins(CHAR *prtname, CHAR **buffer, INT size)
{
	INT i1, i2, len;
	DWORD rc;
	CHAR device[80], driver[80], port[80], *ptr;
	UCHAR **pptr;
	HGLOBAL namehandle;

	PRTAGETPAPERINIT()

	rc = DeviceCapabilities((LPSTR) device, (LPSTR) port, DC_BINNAMES, (LPSTR) NULL, (LPDEVMODE) NULL);
	if ((INT) rc > 0) {
		pptr = memalloc(rc * 24, 0);
		if (pptr == NULL) {
			GlobalFree(namehandle);
			return PRTERR_NOMEM;
		}
		ptr = (CHAR *) *pptr;
		rc = DeviceCapabilities((LPSTR) device, (LPSTR) port, DC_BINNAMES, (LPSTR) ptr, (LPDEVMODE) NULL);
		PRTAGETPAPERSTORE(24)
	}

	GlobalFree(namehandle);
	if (len >= size) return RC_ERROR;
	(*buffer)[len] = '\0';
	return 0;
}

/*
 * Called only from prtclose in prt.c
 */
INT prtasubmit(CHAR *prtfile, PRTOPTIONS *options)
{
	INT i1, threadnum, type;
	DWORD threadid;
	HANDLE filehandle, printhandle;
	CHAR prtname[512], *ptr;

	for (threadnum = 0; threadnum < threadcnt; threadnum++) {
		if ((*threadinfo + threadnum)->threadhandle == NULL) break;
		if (WaitForSingleObject((*threadinfo + threadnum)->threadhandle, 0) != WAIT_OBJECT_0) continue;
		CloseHandle((*threadinfo + threadnum)->threadhandle);
		(*threadinfo + threadnum)->threadhandle = NULL;
		break;
	}
	if (threadnum == threadmax) {
		if (!threadmax) {
			i1 = 2;
			threadinfo = (THREADINFO **) memalloc(i1 * sizeof(THREADINFO), 0);
			if (threadinfo == NULL) return(ERR_NOMEM);
		}
		else {
			i1 = threadmax << 1;
			if (memchange((UCHAR **) threadinfo, i1 * sizeof(THREADINFO), 0)) return(ERR_NOMEM);
		}
		threadmax = i1;
	}

	strcpy(prtname, options->submit.prtname);
	if (!prtname[0] || !strcmp(prtname, "0")) strcpy(prtname, "DEFAULT");
	type = prtanametype(prtname);
	if (type == PRTFLAG_TYPE_PRINTER) {
		if (!_stricmp(prtname, "DEFAULT")) {
			ptr = prtname;
			i1 = prtagetprinters(PRTGET_PRINTERS_DEFAULT, &ptr, sizeof(prtname));
			if (i1 || !*ptr) {
				prtputerror(prtname);
				return PRTERR_BADOPT;
			}
		}
	}
	else if (type != PRTFLAG_TYPE_DEVICE) {
		prtputerror(prtname);
		return PRTERR_BADOPT;
	}

	/* free up some handles */
	fioclru(0);
	fioclru(0);
	fioclru(0);

	filehandle = CreateFile((LPCSTR) prtfile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (filehandle == INVALID_HANDLE_VALUE) {
		prtaerror("CreateFile", GetLastError());
		return PRTERR_OPEN;
	}

	if (type == PRTFLAG_TYPE_PRINTER) {
		if (!OpenPrinter((LPSTR) prtname, &printhandle, NULL)) {
			prtaerror("OpenPrinter", GetLastError());
			CloseHandle(filehandle);
			return PRTERR_OPEN;
		}
	}
	else {
		printhandle = CreateFile((LPCSTR) prtname, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		if (printhandle == INVALID_HANDLE_VALUE) {
			prtaerror("CreateFile", GetLastError());
			CloseHandle(filehandle);
			return PRTERR_OPEN;
		}
	}

	(*threadinfo + threadnum)->filehandle = filehandle;
	(*threadinfo + threadnum)->printhandle = printhandle;
	(*threadinfo + threadnum)->type = type;
	(*threadinfo + threadnum)->flags = options->flags;
	if (options->copies > 1) (*threadinfo + threadnum)->copies = options->copies;
	else (*threadinfo + threadnum)->copies = 1;
	strcpy((*threadinfo + threadnum)->filename, prtfile);
	(*threadinfo + threadnum)->threadhandle =
			CreateThread(NULL, 8192, (LPTHREAD_START_ROUTINE) printfile, (LPVOID) (INT_PTR) threadnum, 0, &threadid);
	if ((*threadinfo + threadnum)->threadhandle == NULL) {
		prtaerror("CreateThread", GetLastError());
		if (type == PRTFLAG_TYPE_PRINTER) {
			AbortPrinter(printhandle);
			ClosePrinter(printhandle);
		}
		else CloseHandle(printhandle);
		CloseHandle(filehandle);
		return PRTERR_NATIVE;
	}
	SetThreadPriority((*threadinfo + threadnum)->threadhandle, THREAD_PRIORITY_BELOW_NORMAL);
	if (threadnum == threadcnt) threadcnt++;
	return 0;
}

static DWORD printfile(DWORD threadnum)
{
	INT i3, i4, copies, flags, type;
	DWORD filebytes, prtbytes;
	CHAR prtfile[128];
	UCHAR buffer[4096];
	HANDLE filehandle, printhandle;
	DOC_INFO_1 docinfo;

	pvistart();
	filehandle = (*threadinfo + threadnum)->filehandle;
	printhandle = (*threadinfo + threadnum)->printhandle;
	type = (*threadinfo + threadnum)->type;
	flags = (*threadinfo + threadnum)->flags;
	copies = (*threadinfo + threadnum)->copies;
	strcpy(prtfile, (*threadinfo + threadnum)->filename);
	pviend();

	while (copies-- > 0) {
		if (type == PRTFLAG_TYPE_PRINTER) {
			docinfo.pDocName = prtfile;
			docinfo.pOutputFile = NULL;
			docinfo.pDatatype = "RAW";  /* for available types see EnumPrintProcessorDatatypes */
			if (!StartDocPrinter(printhandle, 1, (LPBYTE) &docinfo)) {
				AbortPrinter(printhandle);
				ClosePrinter(printhandle);
				CloseHandle(filehandle);
				ExitThread(1);
			}
		}
		SetFilePointer(filehandle, 0, NULL, FILE_BEGIN);
		for ( ; ; ) {
			if (!ReadFile(filehandle, (LPVOID) buffer, (DWORD) sizeof(buffer), &filebytes, NULL)) {
				if (type == PRTFLAG_TYPE_PRINTER) {
					AbortPrinter(printhandle);
					ClosePrinter(printhandle);
				}
				else CloseHandle(printhandle);
				CloseHandle(filehandle);
				ExitThread(1);
			}
			i4 = (INT) filebytes;
			if (!i4 && !(flags & PRT_NOEJ)) {
				buffer[0] = 0x0D;
				buffer[1] = 0x0C;
				i4 = 2;
			}
			for (i3 = 0; i3 < i4; ) {
				if (type == PRTFLAG_TYPE_PRINTER) {
					if (!WritePrinter(printhandle, (LPVOID) &buffer[i3], (DWORD)(i4 - i3), &prtbytes)) {
						AbortPrinter(printhandle);
						ClosePrinter(printhandle);
						CloseHandle(filehandle);
						ExitThread(1);
					}
				}
				else if (!WriteFile(printhandle, (LPVOID) &buffer[i3], (DWORD)(i4 - i3), &prtbytes, NULL)) {
					CloseHandle(printhandle);
					CloseHandle(filehandle);
					ExitThread(1);
				}
				if (prtbytes > 0) i3 += (INT) prtbytes;
				else if (threadstop) {
					if (type == PRTFLAG_TYPE_PRINTER) {
						AbortPrinter(printhandle);
						ClosePrinter(printhandle);
						CloseHandle(filehandle);
					}
					else {
						CloseHandle(printhandle);
						CloseHandle(filehandle);
					}
					ExitThread(1);
				}
			}
			if (!filebytes) break;
		}

		if (type == PRTFLAG_TYPE_PRINTER) {
			EndDocPrinter(printhandle);
		}
	}

	if (type == PRTFLAG_TYPE_PRINTER) ClosePrinter(printhandle);
	else CloseHandle(printhandle);
	CloseHandle(filehandle);

	if (flags & PRT_REMV) DeleteFile((LPCSTR) prtfile);
	return(0);
}

/**
 * Called from prtalloc if PRTFLAG_TYPE_DEVICE
 */
INT prtadevopen(INT prtnum, INT alloctype)
{
	INT commflag;
	DWORD err, bytes;
	HANDLE handle;
	CHAR prtname[MAX_NAMESIZE];
	PRINTINFO *print;
	DCB dcb;
	COMMCONFIG commconfig;

	print = *printtabptr + prtnum;
	strcpy(prtname, *print->prtname);
	commflag = FALSE;
	if (toupper(prtname[0]) == 'C' && toupper(prtname[1]) == 'O' && toupper(prtname[2]) == 'M') {
		memset(&commconfig, 0, sizeof(commconfig));
		commconfig.dwSize = sizeof(commconfig);
		commconfig.dcb.DCBlength = sizeof(DCB);
		bytes = sizeof(commconfig);
		/* If GetDefaultCommConfig succeeds, the return value is nonzero. */
		if (GetDefaultCommConfig((LPCSTR) prtname, &commconfig, &bytes)) {
			memcpy(&dcb, &commconfig.dcb, sizeof(dcb));
			commflag = TRUE;
		}
	}

	for ( ; ; ) {
		handle = CreateFile((LPCSTR) prtname, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (handle != INVALID_HANDLE_VALUE) break;
		err = GetLastError();
		if (err == ERROR_TOO_MANY_OPEN_FILES) {
			if (!fioclru(0)) continue;
		}
		prtaerror("CreateFile", err);
		if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND ||
		    err == ERROR_INVALID_DRIVE) return PRTERR_CREATE;
		if (err == ERROR_ACCESS_DENIED) return PRTERR_ACCESS;
		if (err == ERROR_INVALID_NAME || err == ERROR_BUFFER_OVERFLOW) return PRTERR_BADNAME;
		return PRTERR_OPEN;
	}
	if (commflag) SetCommState(handle, &dcb);
	print->prthandle.winhandle = handle;
	return 0;
}

INT prtadevclose(INT prtnum)
{
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	CloseHandle(print->prthandle.winhandle);
	return 0;
}

INT prtadevwrite(INT prtnum)
{
	INT i1, i2;
	DWORD cnt;
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	for (i1 = 0; ; ) {
		if (!WriteFile(print->prthandle.winhandle, (LPVOID)(*print->buffer + i1), (DWORD)(print->bufcnt - i1), &cnt, NULL)) {
			prtaerror("WriteFile", GetLastError());
			return PRTERR_WRITE;
		}
		i2 = (INT) cnt;
		i1 += i2;
		if (i1 >= print->bufcnt) break;
		Sleep(3000);
	}
	return 0;
}

/**
 * Called from prtalloc if PRTFLAG_TYPE_PRINTER
 */
INT prtaprtopen(INT prtnum)
{
	INT i1;
	DWORD err;
	HANDLE handle;
	CHAR prtname[MAX_NAMESIZE], *ptr;
	PRINTINFO *print;
	DOC_INFO_1 docinfo;

	print = *printtabptr + prtnum;
	strcpy(prtname, *print->prtname);
	if (!_stricmp(prtname, "DEFAULT")) {
		ptr = prtname;
		i1 = prtagetprinters(PRTGET_PRINTERS_DEFAULT, &ptr, sizeof(prtname));
		if (i1 || !*ptr) {
			prtputerror(prtname);
			return PRTERR_BADNAME;
		}
		print = *printtabptr + prtnum;
	}
	for ( ; ; ) {
		if (OpenPrinter((LPSTR) prtname, &handle, NULL)) break;
		err = GetLastError();
/*** CODE: WHAT IS "ERROR_NO_MORE_FILES" ? ***/
		if (err == ERROR_TOO_MANY_OPEN_FILES) {
			if (!fioclru(0)) continue;
		}
		prtaerror("OpenPrinter", err);
		if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND ||
		    err == ERROR_INVALID_DRIVE) return PRTERR_CREATE;
		if (err == ERROR_ACCESS_DENIED) return PRTERR_ACCESS;
		if (err == ERROR_INVALID_NAME || err == ERROR_BUFFER_OVERFLOW) return PRTERR_BADNAME;
		return PRTERR_OPEN;
	}

	docinfo.pDocName = *print->docname;
	docinfo.pOutputFile = NULL;
	docinfo.pDatatype = "RAW";
	for ( ; ; ) {
		if (StartDocPrinter(handle, 1, (LPBYTE) &docinfo)) break;
		err = GetLastError();
/*** CODE: WHAT IS "ERROR_NO_MORE_FILES" ? ***/
		if (err == ERROR_TOO_MANY_OPEN_FILES) {
			if (!fioclru(0)) continue;
		}
		AbortPrinter(handle);
		ClosePrinter(handle);
		prtaerror("StartDocPrinter", err);
		if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND ||
		    err == ERROR_INVALID_DRIVE) return PRTERR_CREATE;
		if (err == ERROR_ACCESS_DENIED) return PRTERR_ACCESS;
		if (err == ERROR_INVALID_NAME || err == ERROR_BUFFER_OVERFLOW) return PRTERR_BADNAME;
		return PRTERR_OPEN;
	}
	print->prthandle.winhandle = handle;
	return 0;
}

INT prtaprtclose(INT prtnum)
{
	PRINTINFO *print;

	print = *printtabptr + prtnum;
/*** CODE: IF LAST CHAR TO BE PRINTED IS NOT x0C or x0Cx0D, THEN SHOULD WE DO EndPagePrinter() ***/
	EndDocPrinter(print->prthandle.winhandle);
	ClosePrinter(print->prthandle.winhandle);
	return 0;
}

INT prtaprtwrite(INT prtnum)
{
	INT i1, i2;
	DWORD cnt;
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	for (i1 = 0; ; ) {
		if (!WritePrinter(print->prthandle.winhandle, (LPVOID)(*print->buffer + i1), (DWORD)(print->bufcnt - i1), &cnt)) {
			prtaerror("WritePrinter", GetLastError());
			return PRTERR_WRITE;
		}
		i2 = (INT) cnt;
		i1 += i2;
		if (i1 >= print->bufcnt) break;
		Sleep(3000);
	}
	return 0;
}

INT prtadrvopen(INT prtnum)
{
	INT i1, i2, copies, duplex, err, iHRes, iVRes, first, last, orient;
	INT i1Save;
	CHAR device[128], driver[128], port[128], work[256], *paper, *source;
	UCHAR fileflag;
	HGLOBAL namehandle;
	HGLOBAL modehandle, newmodehandle;
	HDC printhandle;
	PRINTINFO *print;
	DOCINFO docinfo;
	LPDEVMODE devmode;
	PRINTDLG printdlg;
	TEXTMETRIC tm;

	print = *printtabptr + prtnum;
	copies = print->copies;
	orient = (print->openflags & PRT_LAND) ? DMORIENT_LANDSCAPE : (print->openflags & PRT_PORT) ? DMORIENT_PORTRAIT : 0;
	if (print->papersize != NULL) paper = *print->papersize;
	else paper = "";
	if (print->paperbin != NULL) source = *print->paperbin;
	else source = "";
	duplex = (print->openflags & PRT_DPLX) ? TRUE : FALSE;

	iHRes = iVRes = 0;
	strcpy(work, *print->prtname);
	i1 = (INT)strlen(work) - 1;
	if (i1 > 0 && work[i1] == ')') {
		// We have a trailing right parens
		for (i2 = i1--; i1 > 0 && work[i1] != '('; i1--);
		if (i1 > 0) {
			// Check here for 'all digits' between the parentheses. If not all digits,
			// assume that they are part of the printer name.
			// At this point i1 and i2 are indexes to the left and right parens respectively
			i1Save = i1++;
			while(work[i1] == ' ') {i1++;} // scan past any leading blanks
			if (!isdigit((int)work[i1])) goto notADPI; // Not a DPI
			while (isdigit((int)work[i1])) {i1++;}
			if (work[i1] != ' ' && work[i1] != ')') goto notADPI; // Not a DPI
			while(work[i1] == ' ') {i1++;} // scan past any trailing blanks
			if (work[i1] != ')') goto notADPI; // Not a DPI

			i1 = i1Save;
			work[i1] = work[i2] = '\0';
			for (i2 = 0, i1++; work[i1]; i1++) if (isdigit((int)work[i1])) i2 = i2 * 10 + work[i1] - '0';
			iHRes = iVRes = i2;
		}
		for (i1 = (INT)strlen(work) - 1; i1 >= 0 && work[i1] == ' '; i1--);
		work[i1 + 1] = '\0';
	}

notADPI:
	/* free up some handles */
	for (i1 = 5; --i1 >= 0; ) fioclru(0);

	modehandle = NULL;
	if (!_stricmp(work, "DEFAULT")) {
		if (prtnamehandle == NULL) {
			if (prtmodehandle != NULL) {  /* just incase PrintDlg creates this situation */
				GlobalFree(prtmodehandle);
				prtmodehandle = NULL;
			}
			i1 = getdefdevname(&prtnamehandle);
			if (i1) {
				if (i1 != -1) return i1;
				prtnamehandle = NULL;
			}
		}
		namehandle = prtnamehandle;
		modehandle = prtmodehandle;
	}
	else {
		i1 = getdevname(work, FALSE, &namehandle);
		if (i1) {
			if (i1 != -1) return i1;
			namehandle = NULL;
		}
	}
	/*
	 * Deref again. getdevname could move memory
	 */
	print = *printtabptr + prtnum;
	if (namehandle == NULL && !(print->openflags & PRT_JDLG)) {
		prtputerror("no default name");
		return PRTERR_BADOPT;
	}
	if (modehandle == NULL || (iVRes > 0 && iHRes > 0) || copies || orient || paper[0] || source[0] || duplex) {
		newmodehandle = getdevicemode(namehandle, modehandle, iHRes, iVRes, copies, orient, paper, source, duplex);
		if (modehandle != NULL) prtmodehandle = newmodehandle;
		modehandle = newmodehandle;
	}
	/* get info about default printer */
	fileflag = FALSE;
	if (print->openflags & PRT_JDLG) {
		memset(&printdlg, 0, sizeof(printdlg));
		printdlg.lStructSize = sizeof(printdlg);
		printdlg.hwndOwner = (HWND) GetForegroundWindow();
		printdlg.hDevMode = modehandle;
		printdlg.hDevNames = namehandle;
		printdlg.Flags = PD_NOSELECTION | PD_NOPAGENUMS | PD_USEDEVMODECOPIESANDCOLLATE;
		i1 = PrintDlg(&printdlg);
		if (!i1) {
			i1 = CommDlgExtendedError();
			if (i1) prtaerror("PrintDlg", i1);
			else prtputerror("");
			if (modehandle == NULL || modehandle != prtmodehandle) {
				if (printdlg.hDevMode != NULL) GlobalFree(printdlg.hDevMode);
			}
			else prtmodehandle = printdlg.hDevMode;
			if (namehandle == NULL || namehandle != prtnamehandle) {
				if (printdlg.hDevNames != NULL) GlobalFree(printdlg.hDevNames);
			}
			else prtnamehandle = printdlg.hDevNames;
			if (i1) return PRTERR_DIALOG;
			return PRTERR_CANCEL;
		}
		if (printdlg.Flags & PD_PRINTTOFILE) fileflag = TRUE;
		if (prtmodehandle != NULL && prtmodehandle != modehandle) GlobalFree(prtmodehandle);
		modehandle = prtmodehandle = printdlg.hDevMode;
		if (prtnamehandle != NULL && prtnamehandle != namehandle) GlobalFree(prtnamehandle);
		namehandle = prtnamehandle = printdlg.hDevNames;
	}

	/* get the device name, etc. */
	if (getnames(namehandle, device, driver, port) == RC_ERROR) {
		prtputerror("Invalid profile string");
		if (modehandle != NULL && modehandle != prtmodehandle) GlobalFree(modehandle);
		if (namehandle != NULL && namehandle != prtnamehandle) GlobalFree(namehandle);
		return PRTERR_BADOPT;
	}

	if (modehandle != NULL) devmode = (LPDEVMODE) GlobalLock(modehandle);
	else devmode = NULL;

	printhandle = CreateDC(driver, device, NULL, devmode);
	if (printhandle == NULL) prtaerror("CreateDC", GetLastError());

	if (devmode != NULL) {
		if (devmode->dmFields & DM_DEFAULTSOURCE) print->fmt.gui.binnumber = devmode->dmDefaultSource;
		GlobalUnlock(modehandle);
		print->fmt.gui.hdevmode = modehandle;
	}
	if (namehandle != NULL && namehandle != prtnamehandle) GlobalFree(namehandle);
	if (printhandle == NULL) return PRTERR_OPEN;

	memset(&docinfo, 0, sizeof(docinfo));
	docinfo.cbSize = sizeof(docinfo);
	docinfo.lpszDocName = *print->docname;
	if (fileflag) docinfo.lpszOutput = port;
	else docinfo.lpszOutput = NULL;
/*** CODE: SUPPORT BOTH CASE USING A CONSOLE APP FLAG ***/
	if (StartDoc(printhandle, &docinfo) <= 0) {
		err = GetLastError();
		DeleteDC(printhandle);
		if (err != ERROR_PRINT_CANCELLED) {
			prtaerror("StartDoc", err);
			return PRTERR_OPEN;
		}
		prtputerror("");
		return PRTERR_CANCEL;
	}
	print->fmt.gui.hdc = printhandle;
	strcpy(print->fmt.gui.device, device);
	strcpy(print->fmt.gui.port, port);
	memset(&print->fmt.gui.lf, 0, sizeof(print->fmt.gui.lf));
	print->fmt.gui.lf.lfWeight = FW_NORMAL;
	print->fmt.gui.lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
	print->fmt.gui.lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	print->fmt.gui.lf.lfQuality = DEFAULT_QUALITY;
	if (!parsefont("COURIER(12, PLAIN)", print->fmt.gui.hdc, &print->fmt.gui.lf)) {
		print->fmt.gui.hfont = getFont(&print->fmt.gui.lf);
		SelectObject(print->fmt.gui.hdc, print->fmt.gui.hfont);
	}

	print->fmt.gui.color = 0;
	print->fmt.gui.pos.x = print->fmt.gui.pos.y = 0;

	GetTextMetrics(print->fmt.gui.hdc, &tm);
	/* get maximum width */
	first = tm.tmFirstChar;
	last = tm.tmLastChar;
	if (first < 0 || last > 255 || first >= last) {
		first = 'A';
		last = 'Z';
	}
	/* NOTE: TMPF_FIXED_PITCH is opposite in appearance */
	if (tm.tmPitchAndFamily & TMPF_FIXED_PITCH) i2 = tm.tmMaxCharWidth;
	else i2 = tm.tmAveCharWidth;
	print->fmt.gui.fonthsize = (USHORT) i2;
	print->fmt.gui.fontvsize = (USHORT) (tm.tmHeight + tm.tmExternalLeading);

	print->fmt.gui.hsize = GetDeviceCaps(print->fmt.gui.hdc, HORZRES);
	print->fmt.gui.vsize = GetDeviceCaps(print->fmt.gui.hdc, VERTRES);

	if (iHRes <= 0 || iVRes <= 0) {
		print->fmt.gui.scalex = 3600;
		print->fmt.gui.scaley = 3600;
	}
	else {
		print->fmt.gui.scalex = (GetDeviceCaps(print->fmt.gui.hdc, LOGPIXELSX) * 3600) / iHRes;
		print->fmt.gui.scaley = (GetDeviceCaps(print->fmt.gui.hdc, LOGPIXELSY) * 3600) / iVRes;
	}
	return 0;
}

INT prtadrvclose(INT prtnum)
{
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	if (print->flags & PRTFLAG_PAGE) {
		print->flags &= ~PRTFLAG_PAGE;
		EndPage(print->fmt.gui.hdc);
	}
	EndDoc(print->fmt.gui.hdc);
	/*DeleteObject(*/SelectObject(print->fmt.gui.hdc, GetStockObject(SYSTEM_FONT));
	DeleteDC(print->fmt.gui.hdc);
	print->fmt.gui.hdc = NULL;
	GlobalFree(print->fmt.gui.hdevmode);
	prtmodehandle = NULL;
	return 0;
}

INT prtadrvpage(INT prtnum)
{
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	if (print->flags & PRTFLAG_PAGE) {
		EndPage(print->fmt.gui.hdc);
		print->flags &= ~PRTFLAG_PAGE;
	}
	print->fmt.gui.pos.x = print->fmt.gui.pos.y = 0;
	return 0;
}

static INT prtadrvckpg(INT prtnum) {
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	if (!(print->flags & PRTFLAG_PAGE)) {
		if (StartPage(print->fmt.gui.hdc) == SP_ERROR) {
			prtaerror("StartPage", GetLastError());
			return PRTERR_WRITE;
		}
		print->flags |= PRTFLAG_PAGE;
		SelectObject(print->fmt.gui.hdc, print->fmt.gui.hfont);
	}
	return 0;
}

INT prtadrvtext(INT prtnum, UCHAR *text, INT textlen)
{
	INT angle, linetype, rc;
	DOUBLE radians;
	SIZE size;
	POINT newpos = {0, 0};
	PRINTINFO *print;
	HFONT hfnt, hfntprev;

	if ( (rc = prtadrvckpg(prtnum)) ) return rc;
	print = *printtabptr + prtnum;

	if (print->linetype & PRT_TEXT_RIGHT && print->linetype & PRT_TEXT_CENTER) {
		CHAR errormsg[] = "Internal fail in prtadrvtext, Both *rj and *cj set";
		prtputerror(errormsg);
		return -1;
	}

	GetTextExtentPoint32(print->fmt.gui.hdc, (LPCSTR) text, textlen, &size);
	linetype = print->linetype;
	if ((linetype & PRT_TEXT_CENTER) && print->linevalue > size.cx)
		size.cx = (print->linevalue - size.cx) / 2;
	else linetype &= ~PRT_TEXT_CENTER;
	if ((linetype & PRT_TEXT_ANGLE) && print->lineangle != 90) {
		/* convert to counter-clockwise rotation with east = 0 */
		angle = print->lineangle;
		if (angle <= 90) angle = 90 - angle;
		else angle = 450 - angle;	
		radians = ((DOUBLE) angle * (DOUBLE) M_PI) / (DOUBLE) 180;
		size.cy = 0 - (INT)((DOUBLE) size.cx * cos(radians));
		size.cx = (INT)((DOUBLE) size.cx * sin(radians));
	}
	else {
		linetype &= ~PRT_TEXT_ANGLE;
		size.cy = 0;
	}
	if (print->linetype & PRT_TEXT_RIGHT) {
		print->fmt.gui.pos.x -= size.cx;
		print->fmt.gui.pos.y -= size.cy;
	}
	else if (linetype & PRT_TEXT_CENTER) {
		newpos = print->fmt.gui.pos;
		print->fmt.gui.pos.x += size.cx;
		print->fmt.gui.pos.y += size.cy;
		if ((linetype & PRT_TEXT_ANGLE) && print->lineangle != 90) {
			newpos.x += (INT)((DOUBLE) print->linevalue * sin(radians));
			newpos.y += 0 - (INT)((DOUBLE) print->linevalue * cos(radians));
		}
		else newpos.x += print->linevalue;
	}
	SetTextColor(print->fmt.gui.hdc, print->fmt.gui.color);
	SetBkMode(print->fmt.gui.hdc, TRANSPARENT);
	if ((linetype & PRT_TEXT_ANGLE) && print->lineangle != 90) {
		print->fmt.gui.lf.lfOrientation = print->fmt.gui.lf.lfEscapement = angle * 10;
		hfnt = getFont(&print->fmt.gui.lf);
		hfntprev = SelectObject(print->fmt.gui.hdc, hfnt);
		print->fmt.gui.lf.lfOrientation = print->fmt.gui.lf.lfEscapement = 0;
	}
	rc = 0;
	if (!TextOut(print->fmt.gui.hdc, print->fmt.gui.pos.x, print->fmt.gui.pos.y, (LPCSTR) text, textlen)) {
		prtaerror("TextOut", GetLastError());
		rc = -1;
		goto prtadrvtext_exit;
	}
	if (linetype & PRT_TEXT_CENTER) {
		print->fmt.gui.pos = newpos;
	}
	else {
		print->fmt.gui.pos.x += size.cx;
		print->fmt.gui.pos.y += size.cy;
	}

prtadrvtext_exit:
	if ((linetype & PRT_TEXT_ANGLE) && print->lineangle != 90) {
		SelectObject(print->fmt.gui.hdc, hfntprev);
	}
	return rc;
}

INT prtadrvcircle(INT prtnum, INT r, INT fill) {
	PRINTINFO *print;
	HGDIOBJ hbrushOld, hpenOld;
	INT rc;
	RECT rect;

	if ( (rc = prtadrvckpg(prtnum)) ) return rc;
	print = *printtabptr + prtnum;
	r = (r * print->fmt.gui.scalex) / 3600;
	rect.left = print->fmt.gui.pos.x - r;
	rect.right = print->fmt.gui.pos.x + r; 
	rect.top = print->fmt.gui.pos.y - r;
	rect.bottom = print->fmt.gui.pos.y + r; 
	hbrushOld = SelectObject(print->fmt.gui.hdc, fill ? CreateSolidBrush(print->fmt.gui.color)
		: GetStockObject(HOLLOW_BRUSH));
	hpenOld = SelectObject(print->fmt.gui.hdc, CreatePen(PS_SOLID, 0, print->fmt.gui.color));
	if (!Ellipse(print->fmt.gui.hdc, rect.left, rect.top, rect.right, rect.bottom)) {
		prtaerror("Ellipse", GetLastError());
		rc = -1;
	}
	if (fill) DeleteObject(SelectObject(print->fmt.gui.hdc, hbrushOld));
	else SelectObject(print->fmt.gui.hdc, hbrushOld);
	DeleteObject(SelectObject(print->fmt.gui.hdc, hpenOld));
	return rc;
}

INT prtadrvtriangle(INT prtnum, INT x1, INT y1, INT x2, INT y2) {
	POINT v[3];
	PRINTINFO *print;
	INT rc;
	HGDIOBJ hbrushOld, hpenOld;

	if ( (rc = prtadrvckpg(prtnum)) ) return rc;
	print = *printtabptr + prtnum;
	v[0].x = print->fmt.gui.pos.x;
	v[0].y = print->fmt.gui.pos.y;
	v[1].x = (x1 * print->fmt.gui.scalex) / 3600;
	v[1].y = (y1 * print->fmt.gui.scalex) / 3600;
	v[2].x = (x2 * print->fmt.gui.scalex) / 3600;
	v[2].y = (y2 * print->fmt.gui.scalex) / 3600;
	hbrushOld = SelectObject(print->fmt.gui.hdc, CreateSolidBrush(print->fmt.gui.color));
	hpenOld = SelectObject(print->fmt.gui.hdc, CreatePen(PS_SOLID, 0, print->fmt.gui.color));
	if (!Polygon(print->fmt.gui.hdc, v, 3)) {
		prtaerror("Polygon", GetLastError());
		rc = -1;
	}
	DeleteObject(SelectObject(print->fmt.gui.hdc, hbrushOld));
	DeleteObject(SelectObject(print->fmt.gui.hdc, hpenOld));
	return rc;
}

INT prtadrvrect(INT prtnum, INT x, INT y, INT fill) {
	PRINTINFO *print;
	HGDIOBJ hbrushOld, hpenOld;
	INT rc;
	RECT rect;
	INT linewidth;
	HPEN pen;

	if ( (rc = prtadrvckpg(prtnum)) ) return rc;
	print = *printtabptr + prtnum;

	linewidth = print->linewidth;
	pen = CreatePen(PS_SOLID, linewidth == 1 ? 0 : linewidth, print->fmt.gui.color);

	x = (x * print->fmt.gui.scalex) / 3600;
	y = (y * print->fmt.gui.scalex) / 3600;
	if (print->fmt.gui.pos.x <= x) {	/* current point is left of given point */
		rect.left = print->fmt.gui.pos.x;
		rect.right = x;
	}
	else {	/* current point is right of given point */
		rect.left = x;
		rect.right = print->fmt.gui.pos.x;
	}
	if (print->fmt.gui.pos.y <= y) {	/* current point is below given point */
		rect.top = y;
		rect.bottom = print->fmt.gui.pos.y;
	}
	else {	/* current point is above given point */
		rect.top = print->fmt.gui.pos.y;
		rect.bottom = y;
	}
	/* Rectangle does not include x2,y2 so add one */
	rect.bottom++;
	rect.right++;

	hbrushOld = SelectObject(print->fmt.gui.hdc, fill ? CreateSolidBrush(print->fmt.gui.color)
		: GetStockObject(HOLLOW_BRUSH));
	hpenOld = SelectObject(print->fmt.gui.hdc, pen);
	if (!Rectangle(print->fmt.gui.hdc, rect.left, rect.top, rect.right, rect.bottom)) {
		prtaerror("Rectangle", GetLastError());
		rc = -1;
	}
	if (fill) DeleteObject(SelectObject(print->fmt.gui.hdc, hbrushOld));
	else SelectObject(print->fmt.gui.hdc, hbrushOld);
	DeleteObject(SelectObject(print->fmt.gui.hdc, hpenOld));
	return rc;
}

INT prtadrvline(INT prtnum, INT hpos, INT vpos)
{
	INT rc, linewidth;
	PRINTINFO *print;
	HPEN hPen;
	LOGBRUSH logbrush;

	if ( (rc = prtadrvckpg(prtnum)) ) return rc;
	print = *printtabptr + prtnum;
	linewidth = (print->linewidth * (print->fmt.gui.scalex + print->fmt.gui.scaley)) / 7200;
	if (!linewidth) linewidth = 1;
	
	logbrush.lbStyle = BS_SOLID;
	logbrush.lbColor = print->fmt.gui.color; 
	BeginPath(print->fmt.gui.hdc);
	hPen = SelectObject(print->fmt.gui.hdc, ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT, linewidth, &logbrush, 0, NULL));		

	hpos = (hpos * print->fmt.gui.scalex) / 3600;
	if (hpos > print->fmt.gui.hsize) hpos = print->fmt.gui.hsize;
	vpos = (vpos * print->fmt.gui.scaley) / 3600;
	if (vpos > print->fmt.gui.vsize) vpos = print->fmt.gui.vsize;

	MoveToEx(print->fmt.gui.hdc, print->fmt.gui.pos.x, print->fmt.gui.pos.y, NULL);
	LineTo(print->fmt.gui.hdc, hpos, vpos);

	/* LineTo does not include endpoint */
	SetPixel(print->fmt.gui.hdc, hpos, vpos, print->fmt.gui.color);
	EndPath(print->fmt.gui.hdc);
	StrokePath(print->fmt.gui.hdc);

	DeleteObject(SelectObject(print->fmt.gui.hdc, hPen));
	print->fmt.gui.pos.x = hpos;
	print->fmt.gui.pos.y = vpos;
	return 0;
}

INT prtadrvfont(INT prtnum, CHAR *font)
{
	INT i1, i2;
	HFONT hfont;
	TEXTMETRIC tm;
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	i1 = parsefont(font, print->fmt.gui.hdc, &print->fmt.gui.lf);
	if (i1) return i1;
	hfont = getFont(&print->fmt.gui.lf);
	SelectFont(print->fmt.gui.hdc, hfont);
	print->fmt.gui.hfont = hfont;

	GetTextMetrics(print->fmt.gui.hdc, &tm);
	/*
	 * NOTE: TMPF_FIXED_PITCH is opposite in appearance.
	 * If this bit is set the font is a variable pitch font.
	 */
	if (tm.tmPitchAndFamily & TMPF_FIXED_PITCH) i2 = tm.tmMaxCharWidth;
	else i2 = tm.tmAveCharWidth;
	print->fmt.gui.fonthsize = (USHORT) i2;
	print->fmt.gui.fontvsize = (USHORT) (tm.tmHeight + tm.tmExternalLeading);
	return 0;
}

INT prtadrvpixmap(INT prtnum, PIXHANDLE pixhandle)
{
	INT i1, i2, color, rc, bpp, hsize, vsize, fPrintDirect;
	UCHAR *pbits;
	BITMAP bm;
	PRINTINFO *print;
	PIXMAP *pix;
	HDC hdc;
	HBITMAP hbitmap, oldhbitmapsave;
	BITMAPINFO bmi;
	RGBQUAD rgbq;
	
	if ( (i1 = prtadrvckpg(prtnum)) ) return i1;
	print = *printtabptr + prtnum;
	pix = *pixhandle;

	if (pix->hbitmap == NULL) {  /* win32 console mode - generate DIbitmap */
		rc = 0;
		hdc = CreateCompatibleDC(NULL);
		if (hdc != NULL) {
			/* initialize BITMAPINFOHEADER in BITMAPINFO structure */
			bpp = pix->bpp;
			if (bpp == 1) {
				bmi.bmiColors[0].rgbBlue = 255;
				bmi.bmiColors[0].rgbGreen = 255;
				bmi.bmiColors[0].rgbRed = 255;
				rgbq.rgbBlue = 0;
				rgbq.rgbGreen = 0;
				rgbq.rgbRed = 0;
			}
			vsize = pix->vsize;
			hsize = pix->hsize;
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = hsize;
			bmi.bmiHeader.biHeight = vsize;
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = bpp;
			bmi.bmiHeader.biCompression = BI_RGB;
			bmi.bmiHeader.biSizeImage = ((((hsize * bpp) + 0x1F) & ~0x1F) >> 3) * vsize;
			bmi.bmiHeader.biXPelsPerMeter = 0;
			bmi.bmiHeader.biYPelsPerMeter = 0;
			bmi.bmiHeader.biClrUsed = 0;
			bmi.bmiHeader.biClrImportant = 0;
			hbitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void **) &pbits, NULL, 0);
			if (hbitmap != NULL) {
				oldhbitmapsave = SelectObject(hdc, hbitmap);
				for (i1 = 0; i1 < vsize; i1++) {
					for (i2 = 0; i2 < hsize; i2++) {
						guipixgetcolor(pixhandle, &color, i2, i1); 
						if (SetPixel(hdc, i2, i1, (COLORREF) color) == (COLORREF) -1) break;
					}
					if (i2 < hsize) break;   // If this happens, we must have broke early due to fail
				}
				if (i1 == vsize) {
					rc = StretchDIBits(print->fmt.gui.hdc,
							print->fmt.gui.pos.x, print->fmt.gui.pos.y,
			 				(hsize * print->fmt.gui.scalex) / 3600, (vsize * print->fmt.gui.scaley) / 3600,
							0, 0, hsize, vsize,
							pbits, &bmi,
							DIB_RGB_COLORS, SRCCOPY);
					if (!rc) prtaerror("StretchDIBits", GetLastError());
				}
				else prtaerror("SetPixel", GetLastError());
				SelectObject(hdc, oldhbitmapsave);  /* this could be moved up, but didn't want to interfere with above getlasterror() */
				DeleteObject(hbitmap);
			}
			else prtaerror("CreateDIBSection", GetLastError());
			DeleteDC(hdc);
		}
		else prtaerror("CreateCompatibleDC", GetLastError());
		if (!rc) return PRTERR_NATIVE;
		return 0;
	}

	/* window app code */
	GetObject(pix->hbitmap, sizeof(BITMAP), (LPVOID) &bm);
	fPrintDirect = bm.bmPlanes * bm.bmBitsPixel == 1 ? TRUE : FALSE;

	if (!printbandingflag) {
		rc = printbitblt(print->fmt.gui.hdc, print->fmt.gui.pos.x, print->fmt.gui.pos.y,
			(pix->hsize * print->fmt.gui.scalex) / 3600,
			(pix->vsize * print->fmt.gui.scaley) / 3600,
			0, 0, pix->hsize, pix->vsize,
			pix->hbitmap, pix->hpal, fPrintDirect);
	}
#if 0
/*** CODE: ADD THIS IF PRINTBANDING IS REQUIRED. ADDITIONALLY, INCLUDE IN ***/
/***       SAME IN CONSOLE MODE CODE ***/
	else {  /* print using banding method */
	INT iEscaperc, iBltWidth, iBltHeight, iPosX, iPosY;
	POINT ptBandRect[2];
	RECT rcBandRect;
		rc = 0;
		for ( ; ; ) {
			if ((iEscaperc = Escape(print->fmt.gui.hdc, NEXTBAND, 0, (LPSTR) NULL, (LPSTR) &rcBandRect)) <= 0) {
				prtaerror("Escape", iEscaperc);
/*** CODE: SHOULD RC BE SET HERE AND THEN BREAK OUT ***/
			}
			if (IsRectEmpty(&rcBandRect)) break;

			ptBandRect[0].x = rcBandRect.left;
			ptBandRect[0].y = rcBandRect.top;
			ptBandRect[1].x = rcBandRect.right;
			ptBandRect[1].y = rcBandRect.bottom;
			DPtoLP(print->fmt.gui.hdc, ptBandRect, 2);

			if (print->fmt.gui.pos.x < ptBandRect[1].x &&	
				print->fmt.gui.pos.y < ptBandRect[1].y &&
				print->fmt.gui.pos.x + pix->hsize >= ptBandRect[0].x &&
				print->fmt.gui.pos.y + pix->vsize >= ptBandRect[0].y) {
				hsize = pix->hsize;
				if (print->fmt.gui.pos.x < ptBandRect[0].x) {
					iPosX = ptBandRect[0].x;
					hsize -= (ptBandRect[0].x - print->fmt.gui.pos.x);
				}
				else iPosX = print->fmt.gui.pos.x;

				vsize = pix->vsize;
				if (print->fmt.gui.pos.y < ptBandRect[0].y) {
					iPosY = ptBandRect[0].y;
					vsize -= (ptBandRect[0].y - print->fmt.gui.pos.y);
				}
				else iPosY = print->fmt.gui.pos.y;
    				
				iBltWidth = ptBandRect[1].x - iPosX;
				if (iBltWidth > hsize) iBltWidth = hsize;
	  	
				iBltHeight = ptBandRect[1].y - iPosY;
				if (iBltHeight > vsize) iBltHeight = vsize;

				if ((rc = printbitblt(print->fmt.gui.hdc, iPosX, iPosY,
					iBltWidth, iBltHeight, 0, 0, iBltWidth, iBltHeight,
					pix->hbitmap, pix->hpal, fPrintDirect)) < 0) {
					break;
				}
			}
		}
	}
#endif
	return rc;
}

static INT printbitblt(HDC hdcDest, INT iDestX, INT iDestY, INT iDestWidth, INT iDestHeight,
	INT iSrcX, INT iSrcY, INT iSrcWidth, INT iSrcHeight, HBITMAP hbmSrc, HPALETTE hpalSrc, INT directflg)
{
	BITMAPINFO *pbmi;
	HDC hdcSrc, hdcSrcMem;
	INT bltmode, iClrUsed, iBitmapRes, iBitCount;
	BOOL rc;
	UCHAR *pBits;
	HBITMAP hbmOldDisplay;
	HPALETTE oldhPalette;
	BITMAP bm;
#ifdef WRITEDIB
	static INT curdib;
	CHAR dibname[13];
#endif

	/* create a display device context */
	hdcSrc = GetDC(NULL);

	/* try a direct BLT from the display driver -> printer driver */
	bltmode = GetStretchBltMode(hdcDest);
	SetStretchBltMode(hdcDest, STRETCH_DELETESCANS);
	if (directflg) {
		/* create a memory DC compatible with display device context */
		hdcSrcMem = CreateCompatibleDC(hdcSrc);

		/* select the source bitmap's palette into memory DC (if one exists) */
		oldhPalette = NULL;
		if (hpalSrc != NULL) {
			oldhPalette = SelectPalette(hdcSrcMem, hpalSrc, TRUE);
			RealizePalette(hdcSrcMem);
		}
		hbmOldDisplay = SelectObject(hdcSrcMem, hbmSrc);
		rc = StretchBlt(hdcDest, iDestX, iDestY, iDestWidth, iDestHeight,
					hdcSrcMem, iSrcX, iSrcY, iSrcWidth, iSrcHeight, SRCCOPY);
		SelectObject(hdcSrcMem, hbmOldDisplay);
		if (oldhPalette != NULL) {
			SelectPalette(hdcSrcMem, oldhPalette, TRUE);
			RealizePalette(hdcSrcMem);
		}
		DeleteDC(hdcSrcMem);		
		if (rc) {
			SetStretchBltMode(hdcDest, bltmode);
			ReleaseDC(NULL, hdcSrc);
			return 0;
		}
	}

	/* not direct or FAILED - perhaps device driver(s) do not support direct BLT */
	/* try blting to intermediate DIB, then to printer */

	/* select the source bitmap's palette into display DC (if one exists) */
	oldhPalette = NULL;
	if (hpalSrc != NULL) {
		oldhPalette = SelectPalette(hdcSrc, hpalSrc, TRUE);
		RealizePalette(hdcSrc);
	}

	GetObject(hbmSrc, sizeof(BITMAP), (LPVOID) &bm);
	iBitmapRes = bm.bmPlanes * bm.bmBitsPixel;
	if (1 == iBitmapRes) {
		iClrUsed = 2;
		iBitCount = 1;
	}
	else if (iBitmapRes <= 4) {
		iClrUsed = 16;
		iBitCount = 4;
	}
	else if (iBitmapRes <= 8) {
		iClrUsed = 256;
		iBitCount = 8;
	}
	else {
		iClrUsed = 0;
		iBitCount = 24;
	}

	pbmi = (BITMAPINFO *) malloc(sizeof(BITMAPINFOHEADER) + iClrUsed * sizeof(RGBQUAD));
	if (pbmi == NULL) {
		SetStretchBltMode(hdcDest, bltmode);
		if (oldhPalette != NULL) {
			SelectPalette(hdcSrc, oldhPalette, TRUE);
			RealizePalette(hdcSrc);
		}
		ReleaseDC(NULL, hdcSrc);
		prtputerror("malloc failed in printbitblt");
		return PRTERR_NOMEM;
	}

	/* initialize BITMAPINFOHEADER in BITMAPINFO structure */
	pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pbmi->bmiHeader.biWidth = bm.bmWidth;
	pbmi->bmiHeader.biHeight = bm.bmHeight;
	pbmi->bmiHeader.biPlanes = 1;
	pbmi->bmiHeader.biBitCount = iBitCount;
	pbmi->bmiHeader.biCompression = BI_RGB;
	pbmi->bmiHeader.biSizeImage = ((((bm.bmWidth*iBitCount) + 31) & ~31) >> 3) * bm.bmHeight;
	pbmi->bmiHeader.biXPelsPerMeter = 0;
	pbmi->bmiHeader.biYPelsPerMeter = 0;
	pbmi->bmiHeader.biClrUsed = 0;
	pbmi->bmiHeader.biClrImportant = 0;

	pBits = (UCHAR *) malloc(pbmi->bmiHeader.biSizeImage);
	if (pBits == NULL) {
		free((void *) pbmi);
		SetStretchBltMode(hdcDest, bltmode);
		if (oldhPalette != NULL) {
			SelectPalette(hdcSrc, oldhPalette, TRUE);
			RealizePalette(hdcSrc);
		}
		ReleaseDC(NULL, hdcSrc);
		prtputerror("malloc failed in printbitblt");
		return PRTERR_NOMEM;
	}

	/* do NOT select hbmSrc into the hdcSrc DC before this call */
	if (!GetDIBits(hdcSrc, hbmSrc, 0, bm.bmHeight, pBits, pbmi, DIB_RGB_COLORS)) {
		prtaerror("GetDIBits", GetLastError());
		free((void *) pBits);
		free((void *) pbmi);
		SetStretchBltMode(hdcDest, bltmode);
		if (oldhPalette != NULL) {
			SelectPalette(hdcSrc, oldhPalette, TRUE);
			RealizePalette(hdcSrc);
		}
		ReleaseDC(NULL, hdcSrc);
		return PRTERR_NATIVE;
	}
	pbmi->bmiHeader.biClrUsed = iClrUsed;
	pbmi->bmiHeader.biClrImportant = iClrUsed;

#ifdef WRITEDIB
	_tcscpy(dibname, "testpic .bmp");
	dibname[7] = (curdib++ & 0x07) + '0';
	WriteDIB (dibname, pBits, pbmi);
#endif
	if (oldhPalette != NULL) {
		SelectPalette(hdcSrc, oldhPalette, TRUE);
		RealizePalette(hdcSrc);
	}
	rc = StretchDIBits(hdcDest,
					iDestX, iDestY,
		 			iDestWidth, iDestHeight,
					iSrcX, iSrcY,
					iSrcWidth, iSrcHeight,
					pBits, pbmi,
					DIB_RGB_COLORS, SRCCOPY);
	if (!rc) prtaerror("StretchDIBits", GetLastError());
	free((void *) pBits);
	free((void *) pbmi);
	SetStretchBltMode(hdcDest, bltmode);
	ReleaseDC(NULL, hdcSrc);
	return (rc) ? 0 : PRTERR_NATIVE;
}

/**
 * Possible return values;
 * 	0 for success
 * 	RC_ERROR if prtname is not large enough
 * 	PRTERR_NATIVE if a call to a win32 API goes bad
 * 	PRTERR_NOSPACE if a call to GlobalAlloc or GlobalLock goes bad, of any kind
 * 	PRTERR_NOMEM if a call to memalloc goes bad
 */
INT getdefdevname(HGLOBAL *ghandle)
{
	INT i1;
	CHAR prtname[512], *ptr;

	ptr = prtname;
	i1 = prtagetprinters(PRTGET_PRINTERS_DEFAULT, &ptr, sizeof(prtname));
	if (i1 || !*ptr) return (i1) ? i1 : -1;

	return getdevname(prtname, TRUE, ghandle);
}

/**
 * Parameters;
 * 	prtname The name of the printer, it is passed to OpenPrinter
 *  ghandle On successful return, handle to a GMEM_MOVEABLE block that starts with a DEVNAMES structure
 *  	and is followed by the name strings of the driver, printer, and port
 *  defaultflag Determines the value of DEVNAMES->wDefault
 *
 * Calls memalloc, could move memory!
 *
 * Possible return values;
 * 	0 for success
 * 	PRTERR_NATIVE if a call to a win32 API goes bad
 * 	PRTERR_NOSPACE if a call to GlobalAlloc or GlobalLock goes bad, of any kind
 * 	PRTERR_NOMEM if a call to memalloc goes bad
 */
static INT getdevname(CHAR *prtname, INT defaultflag, HGLOBAL *ghandle)
{
	INT driverlen, portlen, printerlen;
	WORD offset;
	DWORD bytes;
	UCHAR **pptr;
	HANDLE handle;
	LPPRINTER_INFO_2 prtinfo2;
	DEVNAMES *devnames;

	if (!OpenPrinter(prtname, &handle, NULL)) {
		prtaerror("OpenPrinter", GetLastError());
		return PRTERR_NATIVE;
	}
	GetPrinter(handle, 2, NULL, 0, &bytes);
	pptr = memalloc(bytes, 0);
	if (pptr == NULL) return PRTERR_NOMEM;
	if (!GetPrinter(handle, 2, (LPBYTE) *pptr, bytes, &bytes)) {
		prtaerror("GetPrinter", GetLastError());
		memfree(pptr);
		ClosePrinter(handle);
		return PRTERR_NATIVE;
	}
	ClosePrinter(handle);
	prtinfo2 = (LPPRINTER_INFO_2) *pptr;
    driverlen = (INT)strlen(prtinfo2->pDriverName) + 1;
    printerlen = (INT)strlen(prtinfo2->pPrinterName) + 1;
    portlen = (INT)strlen(prtinfo2->pPortName) + 1;

	*ghandle = GlobalAlloc(GMEM_MOVEABLE, sizeof(DEVNAMES) + (driverlen + printerlen + portlen) * sizeof(CHAR));
	if (*ghandle == NULL) {
		prtaerror("GlobalAlloc", GetLastError());
		memfree(pptr);
		return PRTERR_NOSPACE;
	}
	devnames = (LPDEVNAMES) GlobalLock(*ghandle);
	if (devnames == NULL) {
		prtaerror("GlobalLock", GetLastError());
		GlobalFree(*ghandle);
		memfree(pptr);
		return PRTERR_NOSPACE;
	}

	offset = sizeof(DEVNAMES);
	devnames->wDriverOffset = offset;
	memcpy((LPSTR) devnames + offset, prtinfo2->pDriverName, driverlen * sizeof(CHAR));
	offset += driverlen * (WORD)sizeof(CHAR);
	devnames->wDeviceOffset = offset;
	memcpy((LPSTR) devnames + offset, prtinfo2->pPrinterName, printerlen * sizeof(CHAR));
	offset += printerlen * (WORD)sizeof(CHAR);
	devnames->wOutputOffset = offset;
	memcpy((LPSTR) devnames + offset, prtinfo2->pPortName, portlen * sizeof(CHAR));
	devnames->wDefault = (defaultflag) ? DN_DEFAULTPRN : 0;

	GlobalUnlock(*ghandle);
	memfree(pptr);
	return 0;
}

HGLOBAL getdevicemode(HGLOBAL nameshandle, HGLOBAL modehandle, INT hres, INT vres, INT copies, INT orient,
		CHAR *papername, CHAR *binname, INT duplex)
{
	INT i1, i2, i3, paperbin, papersize;
	DWORD rc, rc2;
	CHAR device[MAX_NAMESIZE], driver[MAX_NAMESIZE], port[MAX_NAMESIZE];
	HGLOBAL newmodehandle, memhandle;
	HANDLE printhandle;
	WORD *wptr;
	LONG *lptr;
	CHAR *ptr, *ptr2;
	LPDEVMODE devmode;
	BOOL found;

	if (papername != NULL && !papername[0]) papername = NULL;
	if (binname != NULL && !binname[0]) binname = NULL;
	/* load names into buffers */
	if (getnames(nameshandle, device, driver, port) == RC_ERROR || !OpenPrinter(device, &printhandle, NULL)) {
		if (modehandle != NULL) GlobalFree(modehandle);
		return NULL;
	}

	papersize = paperbin = 0;
	if ((hres > 0 && vres > 0) || copies || orient == DMORIENT_LANDSCAPE || papername != NULL || binname != NULL || duplex) {
		/* determine capabilities */
		if (hres > 0 && vres > 0) {
			rc = DeviceCapabilities((LPCSTR) device, (LPCSTR) port, DC_ENUMRESOLUTIONS, (LPSTR) NULL, (LPDEVMODE) NULL);
			if ((INT) rc > 0) {
				memhandle = GlobalAlloc(GMEM_MOVEABLE, 2 * rc * sizeof(LONG));
				if (memhandle != NULL) {
					ptr = (CHAR *) GlobalLock(memhandle);
					if (ptr != NULL) {
						lptr = (LONG *) ptr;
						rc = DeviceCapabilities((LPCSTR) device, (LPCSTR) port, DC_ENUMRESOLUTIONS, (LPSTR) ptr, (LPDEVMODE) NULL);
						for (i1 = i2 = i3 = 0; i1 < (INT) rc; i1++, lptr += 2) {
							if (lptr[0] >= hres && lptr[1] >= vres) {
								if (!i2 || lptr[0] < i2 || lptr[1] < i3) {
									i2 = lptr[0];
									i3 = lptr[1];
									if (i2 == hres && i3 == vres) break;
								}
							}
						}
						hres = i2;
						vres = i3;
						GlobalUnlock(memhandle);
					}
					else hres = vres = 0;
					GlobalFree(memhandle);
				}
				else hres = vres = 0;
			}
			else hres = vres = 0;
		}
		if (copies) {
			rc = DeviceCapabilities((LPCSTR) device, (LPCSTR) port, DC_COPIES, (LPSTR) NULL, (LPDEVMODE) NULL);
			if ((INT) rc != -1) {
				if (copies > (INT) rc) copies = (INT) rc;
			}
			else copies = 0;
		}
		if (orient == DMORIENT_LANDSCAPE) {
			rc = DeviceCapabilities((LPCSTR) device, (LPCSTR) port, DC_ORIENTATION, (LPSTR) NULL, (LPDEVMODE) NULL);
/*** CODE: MAY WANT TO COMMENT OUT BECAUSE WINFAX RETURNS 0 EVEN THOUGH IT SUPPORTS LANDSCAPE ***/
			if ((INT) rc <= 0) orient = 0;
		}
		if (duplex) {
			rc = DeviceCapabilities((LPCSTR) device, (LPCSTR) port, DC_DUPLEX, (LPSTR) NULL, (LPDEVMODE) NULL);
			if ((INT) rc > 0)
				if (orient == DMORIENT_LANDSCAPE)
					duplex = DMDUP_HORIZONTAL;
				else
					duplex = DMDUP_VERTICAL;
			else
				duplex = 0;
		}
		if (papername != NULL) {
			rc = DeviceCapabilities((LPCSTR) device, (LPCSTR) port, DC_PAPERNAMES, (LPSTR) NULL, (LPDEVMODE) NULL);
			rc2 = DeviceCapabilities((LPCSTR) device, (LPCSTR) port, DC_PAPERS, (LPSTR) NULL, (LPDEVMODE) NULL);
			if ((INT) rc > 0 && rc == rc2) {  /* must be same size to assume 1 to 1 correspondence */
				memhandle = GlobalAlloc(GMEM_MOVEABLE, rc * 64);
				if (memhandle != NULL) {
					ptr = (CHAR *) GlobalLock(memhandle);
					if (ptr != NULL) {
						wptr = (WORD *) ptr;
						rc = DeviceCapabilities((LPCSTR) device, (LPCSTR) port, DC_PAPERNAMES, (LPSTR) ptr, (LPDEVMODE) NULL);
						found = FALSE;
						/*
						 * This is what we were doing
						 *  for (i1 = 0; i1 < (INT) rc && _tcsicmp(papername, &ptr[i1 * 64]); i1++);
						 *
						 * This was an exact match only. But some printer drivers include extra info
						 * after the usual paper name. e.g. Legal (8.5" x 14")
						 *
						 */
						for (i1 = 0; i1 < (INT) rc; i1++) {
							if (_tcsicmp(papername, &ptr[i1 * 64]) == 0) {
								found = TRUE;
								break; /* Exact match, go for it */
							}
						}
						if (!found) { /* Try something a little more complicated */
							i3 = (INT)(strlen(papername) * sizeof(CHAR));
							for (i1 = 0; i1 < (INT) rc; i1++) {
								ptr2 = &ptr[i1 * 64];
#pragma warning(suppress:4996)
								if (_memicmp(papername, ptr2, i3) == 0) { /* if all the chars in papername match... */
									/* See if the next char in the driver string is a blank */
									if (*(ptr2 + i3) == ' ') {
										found = TRUE;
										break;
									}
								}
							}
						}
						if (found) {
							rc2 = DeviceCapabilities((LPCSTR) device, (LPCSTR) port, DC_PAPERS, (LPSTR) ptr, (LPDEVMODE) NULL);
							if (i1 < (INT) rc2) papersize = wptr[i1];
						}
						GlobalUnlock(memhandle);
					}
					GlobalFree(memhandle);
				}
			}
		}
		if (binname != NULL) paperbin = prtadrvgetpaperbinnumber(device, port, binname);
	}

	/* get printer driver device mode buffer */
	newmodehandle = NULL;

	/* get the number of bytes in the full DEVMODE buffer, including device-dependent part */
   	rc = DocumentProperties(NULL, printhandle, device, NULL, NULL, 0);	  
	if ((INT) rc <= 0) {
		goto getdevicemodex;
	}

	if (modehandle == NULL || rc != GlobalSize(modehandle)) {
		/* allocate storage for the DEVMODE buffer */
		newmodehandle = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, rc);
		if (newmodehandle == NULL) {
			goto getdevicemodex;
		}
		devmode = (LPDEVMODE) GlobalLock(newmodehandle);
		if (devmode == NULL) {
			GlobalFree(newmodehandle);
			newmodehandle = NULL;
			goto getdevicemodex;
		}
		/* Get the current printer settings */
/*** NOTE: IF WANT INTERACTIVE, USE "DM_IN_PROMPT | DM_OUT_BUFFER" AND SET WINDOW HANDLE ***/
/*** PROBLEM:  DOES NOT USE SAME COMMON DIALOG BOX AS PrintDlg() ***/
   		if (DocumentProperties(NULL, printhandle, device, devmode, NULL, DM_OUT_BUFFER) != IDOK) {
			GlobalFree(newmodehandle);
			newmodehandle = NULL;
			goto getdevicemodex;
   		}
	}
	else {
		newmodehandle = modehandle;
		modehandle = NULL;
		devmode = (LPDEVMODE) GlobalLock(newmodehandle);
		if (devmode == NULL) {
			GlobalFree(newmodehandle);
			newmodehandle = NULL;
			goto getdevicemodex;
		}
	}

	if ((hres > 0 && vres > 0) || copies || orient || papersize || paperbin || duplex) {
		/* change printer settings */
/*** CODE: DON'T UNDO PREVIOUS SETTINGS ***/
/***		devmode->dmFields = 0; ***/
		if (hres > 0 && vres > 0) {
			devmode->dmFields |= DM_PRINTQUALITY | DM_YRESOLUTION;
			devmode->dmPrintQuality = hres;
			devmode->dmYResolution = vres;
		}
		if (copies) {
			devmode->dmFields |= DM_COPIES;
			devmode->dmCopies = copies;
			if (copies > 1) {
				devmode->dmFields |= DM_COLLATE;
				devmode->dmCollate = DMCOLLATE_TRUE;
			}
		}
		if (orient) {
			devmode->dmFields |= DM_ORIENTATION;
			devmode->dmOrientation = orient;
		}
		if (papersize >= DMPAPER_FIRST) {
			devmode->dmFields |= DM_PAPERSIZE;
			devmode->dmPaperSize = papersize;
		}
		if (paperbin) {
			devmode->dmFields |= DM_DEFAULTSOURCE;
			devmode->dmDefaultSource = paperbin;
		}
		if (duplex) {
			devmode->dmFields |= DM_DUPLEX;
			devmode->dmDuplex = duplex;
		}
		if (DocumentProperties(NULL, printhandle, device, devmode, devmode, DM_IN_BUFFER | DM_OUT_BUFFER) != IDOK) {
			GlobalFree(newmodehandle);
			newmodehandle = NULL;
			goto getdevicemodex;
		}
	} 
	GlobalUnlock(newmodehandle);

getdevicemodex:
	ClosePrinter(printhandle);
	if (modehandle != NULL) GlobalFree(modehandle);
	return newmodehandle;
}

INT prtadrvchangepaperbin(INT prtnum, INT paperbin)
{
	PRINTINFO *print;
	HANDLE printhandle;
	LONG rc;
	HGLOBAL hdevmode = NULL;
	LPDEVMODE lpdevmode;
	CHAR errstr[128];

	print = *printtabptr + prtnum;
	
	if (!OpenPrinter(print->fmt.gui.device, &printhandle, NULL)) {
		sprintf_s(errstr, ARRAYSIZE(errstr), "\tOpenPrinter failure = %d", (int) GetLastError());
		prtputerror(errstr);
		return 1;
	}

	if (print->fmt.gui.hdevmode == NULL) {
	   	rc = DocumentProperties(NULL, printhandle, print->fmt.gui.device, NULL, NULL, 0);
	   	if (rc <= 0) {
	   		sprintf_s(errstr, ARRAYSIZE(errstr), "\tDocumentProperties failure = %d", (int) GetLastError());
			prtputerror(errstr);
			ClosePrinter(printhandle);
			return 1;
	   	}
		hdevmode = GlobalAlloc(GMEM_MOVEABLE, rc);
		if (hdevmode == NULL) {
			sprintf_s(errstr, ARRAYSIZE(errstr), "\tGlobalAlloc failure = %d", (int) GetLastError());
			prtputerror(errstr);
			ClosePrinter(printhandle);
			return 1;
		}
		lpdevmode = (LPDEVMODE) GlobalLock(hdevmode);
		if (DocumentProperties(NULL, printhandle, print->fmt.gui.device, lpdevmode, NULL, DM_OUT_BUFFER) < 0) {
			ClosePrinter(printhandle);
			GlobalUnlock(hdevmode);
			GlobalFree(hdevmode);
			sprintf_s(errstr, ARRAYSIZE(errstr), "\tDocumentProperties failure = %d", (int) GetLastError());
			prtputerror(errstr);
			return 1;
		}
		GlobalUnlock(hdevmode);
		print->fmt.gui.hdevmode = hdevmode;
	}
	lpdevmode = (LPDEVMODE) GlobalLock(print->fmt.gui.hdevmode);
	if (lpdevmode == NULL) {
		prtaerror("GlobalLock", GetLastError());
		ClosePrinter(printhandle);
		return 1;
	}
	lpdevmode->dmDefaultSource = paperbin;
	lpdevmode->dmFields |= DM_DEFAULTSOURCE;

	if (DocumentProperties(NULL, printhandle, print->fmt.gui.device, lpdevmode, lpdevmode, DM_OUT_BUFFER | DM_IN_BUFFER) < 0) {
		sprintf_s(errstr, ARRAYSIZE(errstr), "\tDocumentProperties failure = %d", (int) GetLastError());
		prtputerror(errstr);
		ClosePrinter(printhandle);
		if (hdevmode != NULL) GlobalUnlock(hdevmode);
		return 1;
	}
	if (ResetDC(print->fmt.gui.hdc, lpdevmode) != NULL) {
		print->fmt.gui.binnumber = paperbin;
	}
	else {
		prtaerror("ResetDC", (int) GetLastError());
		return 1;
	}
	ClosePrinter(printhandle);
	GlobalUnlock(print->fmt.gui.hdevmode);
	return 0;
}

INT prtadrvgetpaperbinnumber(CHAR *device, CHAR *port, CHAR *binname)
{
	INT paperbin, i1;
	DWORD rc, rc2;
	HGLOBAL memhandle;
	CHAR *ptr;
	
	paperbin = 0;
	rc = DeviceCapabilities((LPSTR) device, (LPSTR) port, DC_BINNAMES, (LPSTR) NULL, (LPDEVMODE) NULL);
	rc2 = DeviceCapabilities((LPSTR) device, (LPSTR) port, DC_BINS, (LPSTR) NULL, (LPDEVMODE) NULL);
	if (rc > 0 && rc == rc2) {  /* must be same size to assume 1 to 1 correspondence */
		memhandle = GlobalAlloc(GMEM_MOVEABLE, rc * 24);
		if (memhandle != NULL) {
			ptr = (CHAR *) GlobalLock(memhandle);
			if (ptr != NULL) {
				rc = DeviceCapabilities((LPSTR) device, (LPSTR) port, DC_BINNAMES, (LPSTR) ptr, (LPDEVMODE) NULL);
				for (i1 = 0; i1 < (INT) rc && _tcsicmp(binname, ptr + i1 * 24); i1++);
				if (i1 < (INT) rc) {
					rc = DeviceCapabilities((LPSTR) device, (LPSTR) port, DC_BINS, (LPSTR) ptr, (LPDEVMODE) NULL);
					//wuptr = (WORD *) ptr;
					if (i1 < (INT) rc) paperbin = ((WORD *) ptr)[i1];
				}
				GlobalUnlock(memhandle);
			}
			GlobalFree(memhandle);
		}
	}
	return paperbin;
}

/**
 * Returns RC_ERROR if nameshandle is NULL or it cannot be locked.
 * Returns zero if success.
 */
static INT getnames(HGLOBAL nameshandle, CHAR *device, CHAR *driver, CHAR *port)
{
	LPDEVNAMES devnames;

	if (nameshandle == NULL || (devnames = (LPDEVNAMES) GlobalLock(nameshandle)) == NULL) return RC_ERROR;
	strcpy(driver, (LPSTR) devnames + devnames->wDriverOffset);
	strcpy(device, (LPSTR) devnames + devnames->wDeviceOffset);
	strcpy(port, (LPSTR) devnames + devnames->wOutputOffset);

	GlobalUnlock(nameshandle);
	return(0);
}

static INT parsefont(CHAR *font, HDC hdc, LOGFONT *lf)
{
	INT i1, size, saveHeight;

	
	saveHeight = lf->lfHeight;
	i1 = parseFontString(font, lf, hdc);	/* This is in gui.c */
	if (i1) return i1;
	if (saveHeight != lf->lfHeight) {
		size = GetDeviceCaps(hdc, LOGPIXELSY);
		lf->lfHeight = -((lf->lfHeight * size) / 72);
	}
	return 0;
}

void prtaerror(CHAR *function, INT err)
{
	CHAR num[32], work[256];

	strcpy(work, function);
	strcat(work, " failure");
	if (err) {
		_itoa(err, num, 10);
		strcat(work, ", error = ");
		strcat(work, num);
	}
	prtputerror(work);
}
