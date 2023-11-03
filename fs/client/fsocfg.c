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
 
/*
  The following functions may be part of your ODBC driver or in a separate DLL.

  The ODBC Installer allows a driver to control the management of
  data sources by calling the ConfigDSN entry point in the appropriate
  DLL.  When called, ConfigDSN receives four parameters:

	hwndParent --- Handle of the parent window for any dialogs which
				may need to be created.  If this handle is NULL,
				then no dialogs should be displayed (that is, the
				request should be processed silently).

	fRequest ----- Flag indicating the type of request (add, configure
				(edit), or remove).

	lpszDriver --- Pointer to a null-terminated string containing
				the name of your driver.  This is the same string you
				supply in the ODBC.INF file as your section header
				and which ODBC Setup displays to the user in lieu
				of the actual driver filename.  This string needs to
				be passed back to the ODBC Installer when adding a
				new data source name.

	lpszAttributes- Pointer to a list of null-terminated attribute
				keywords.  This list is similar to the list passed
				to SQLDriverConnect, except that each key-value
				pair is separated by a null-byte rather than a
				semicolon.  The entire list is then terminated with
				a null-byte (that is, two consecutive null-bytes
				mark the end of the list).  The keywords accepted
				should be those for SQLDriverConnect which are
				applicable, any new keywords you define for ODBC.INI,
				and any additional keywords you decide to document.

  ConfigDSN should return TRUE if the requested operation succeeds and
  FALSE otherwise.

  Your setup code should not write to ODBC.INI directly to add or remove
  data source names.  Instead, link with ODBCINST.LIB (the ODBC Installer
  library) and call SQLWriteDSNToIni and SQLRemoveDSNFromIni.
  Use SQLWriteDSNToIni to add data source names.  If the data source name
  already exists, SQLWriteDSNToIni will delete it (removing all of its
  associated keys) and rewrite it.  SQLRemoveDSNToIni removes a data
  source name and all of its associated keys.

  For NT compatibility, the driver code should not use the
  Get/WritePrivateProfileString windows functions for ODBC.INI, but instead,
  use SQLGet/SQLWritePrivateProfileString functions that are macros (16 bit) or
  calls to the odbcinst.dll (32 bit).
*/

#define INC_STDIO
#define INC_STDLIB
#define INC_STRING
#include "includes.h"
#include "fsodbcx.h"
#include "tcp.h"
#include <windowsx.h>
#include <odbcinst.h>
#include <sqlext.h>

#define MIN(x,y) ((x < y) ? x : y)

#define MAXPATHLEN         (255+1)           // Max path length (must be >= to below max sizes)
#define MAXKEYLEN          (15+1)            // Max keyword length
#define MAXDESC            (255+1)           // Max description length
#define MAXSERVER          (100+1)           // Max server ip address length
#define MAXPORT            (32+1)            // Max server port number length

#if OS_WIN32
extern HINSTANCE s_hModule;  // Saved module handle.
#else
#define lstrlen(a) strlen(a)
#define lstrcpy(a, b) strcpy(a, b)
#define lstrcmpi(a, b) strcasecmp(a, b)
#define _fmemcpy(a, b, c) memcpy(a, b, c)
#endif

extern const char EMPTYSTR[];

// ODBC.INI keywords
extern const char ODBC[];                 // ODBC application name
extern const char ODBC_INI[];             // ODBC initialization file
extern const char INI_KDATABASE[];        // Name of database (.dbd file)
extern const char INI_KDESCRIPTION[];     // Data source description
extern const char INI_KUID[];             // User Default ID
extern const char INI_KPASSWORD[];        // User Default Password
extern const char INI_KSERVER[];          // Server IP address
extern const char INI_KSERVERPORT[];      // Server port number
extern const char INI_KLOCALPORT[];       // Local port number
extern const char INI_KENCRYPTION[];      // Encryption

// Attribute key indexes (into an array of Attr structs, see below)
#define KEY_DSN 			0
#define KEY_FILEDSN			1
#define KEY_SAVEFILE		2
#define KEY_DESCRIPTION		3
#define KEY_DATABASE		4
#define KEY_UID				5
#define KEY_PASSWORD		6
#define KEY_SERVER			7
#define KEY_SERVERPORT		8
#define KEY_LOCALPORT		9
#define KEY_ENCRYPTION		10
#define NUMOFKEYS			11				// Number of keys supported

// Attribute string look-up table (maps keys to associated indexes)
static struct {
  char szKey[MAXKEYLEN];
  int iKey;
} s_aLookup[] = {
	"DSN",				KEY_DSN,
	"FileDSN",			KEY_FILEDSN,
	"SaveFile",			KEY_SAVEFILE,
	"Database",			KEY_DATABASE,
	"Description",		KEY_DESCRIPTION,
	"UID",				KEY_UID,
	"Password",			KEY_PASSWORD,
	"Server",			KEY_SERVER,
	"ServerPort",		KEY_SERVERPORT,
	"LocalPort",		KEY_LOCALPORT,
	"Encryption",		KEY_ENCRYPTION,
	"",			0
};

// Types -------------------------------------------------------------------
typedef struct tagAttr {
	BOOL  fSupplied;
	char  szAttr[MAXPATHLEN];
} Attr, *LPAttr;

// NOTE:  All these are used by the dialog procedures
typedef struct tagSETUPDLG {
	SQLHWND	hwndParent; 					// Parent window handle
	LPCSTR	lpszDrvr;						// Driver description
	Attr	aAttr[NUMOFKEYS];				// Attribute array
	char	szDSN[SQL_MAX_DSN_LENGTH];				// Original data source name
	BOOL	fNewDSN;						// New data source flag
	BOOL	fFileDSN;						// File DSN flag
} SETUPDLG, *LPSETUPDLG;

// Prototypes --------------------------------------------------------------
LRESULT CALLBACK ConfigDlgProc(HWND, UINT, WPARAM, LPARAM);
static void CenterDialog(HWND);
static BOOL SetDSNAttributes(HWND, LPSETUPDLG);
static void ParseAttributes(LPCSTR, LPSETUPDLG);

INT ConfigFileDSN(SQLHWND hwnd, CHAR *lpszAttributes)
{
	INT i1, retcode;
	LPSETUPDLG lpsetupdlg;

	// Allocate attribute array
	lpsetupdlg = (LPSETUPDLG) malloc(sizeof(SETUPDLG));
	if (lpsetupdlg == NULL) return FALSE;
	memset(lpsetupdlg, 0, sizeof(SETUPDLG));

	// Parse attribute string
	if (lpszAttributes) ParseAttributes(lpszAttributes, lpsetupdlg);
	if (lpsetupdlg->aAttr[KEY_FILEDSN].fSupplied) {
		for (i1 = lstrlen(lpsetupdlg->aAttr[KEY_FILEDSN].szAttr); i1 && lpsetupdlg->aAttr[KEY_FILEDSN].szAttr[i1 - 1] != '\\' && lpsetupdlg->aAttr[KEY_FILEDSN].szAttr[i1 - 1] != '/' && lpsetupdlg->aAttr[KEY_FILEDSN].szAttr[i1 - 1] != ':'; i1--);
		lstrcpy(lpsetupdlg->aAttr[KEY_DSN].szAttr, lpsetupdlg->aAttr[KEY_FILEDSN].szAttr + i1);
	}
	else if (lpsetupdlg->aAttr[KEY_SAVEFILE].fSupplied) {
		for (i1 = lstrlen(lpsetupdlg->aAttr[KEY_SAVEFILE].szAttr); i1 && lpsetupdlg->aAttr[KEY_SAVEFILE].szAttr[i1 - 1] != '\\' && lpsetupdlg->aAttr[KEY_SAVEFILE].szAttr[i1 - 1] != '/' && lpsetupdlg->aAttr[KEY_SAVEFILE].szAttr[i1 - 1] != ':'; i1--);
		lstrcpy(lpsetupdlg->aAttr[KEY_DSN].szAttr, lpsetupdlg->aAttr[KEY_SAVEFILE].szAttr + i1);
	}
	else {
		free(lpsetupdlg);
		return -1;
	}

	// Add or Configure data source
	// Save passed variables for global access (e.g., dialog access)
	lpsetupdlg->hwndParent = hwnd;
//	lpsetupdlg->fNewDSN	 = (ODBC_ADD_DSN == fRequest);
	lpsetupdlg->fFileDSN = TRUE;

	// Display the appropriate dialog (if parent window handle supplied)
	if (hwnd) {
		// initialize any attributes not supplied
		// NOTE: Values supplied in the attribute string will always
		//		 override settings in file DSN
		if (lpsetupdlg->aAttr[KEY_FILEDSN].fSupplied) {
			if (!lpsetupdlg->aAttr[KEY_DATABASE].fSupplied) {
				if (!SQLReadFileDSN(lpsetupdlg->aAttr[KEY_FILEDSN].szAttr,
					ODBC, INI_KDATABASE,
					lpsetupdlg->aAttr[KEY_DATABASE].szAttr,
					sizeof(lpsetupdlg->aAttr[KEY_DATABASE].szAttr), NULL)) {
					lstrcpy(lpsetupdlg->aAttr[KEY_DATABASE].szAttr, lpsetupdlg->aAttr[KEY_DSN].szAttr);
					i1 = lstrlen(lpsetupdlg->aAttr[KEY_DATABASE].szAttr);
					if (i1 > 4 && !lstrcmpi(lpsetupdlg->aAttr[KEY_DATABASE].szAttr + i1 - 4, ".dsn"))
						lpsetupdlg->aAttr[KEY_DATABASE].szAttr[i1 - 4] = '\0';
				}
			}
			if (!lpsetupdlg->aAttr[KEY_UID].fSupplied) {
				SQLReadFileDSN(lpsetupdlg->aAttr[KEY_FILEDSN].szAttr,
					ODBC, INI_KUID,
					lpsetupdlg->aAttr[KEY_UID].szAttr,
					sizeof(lpsetupdlg->aAttr[KEY_UID].szAttr), NULL);
			}
			if (!lpsetupdlg->aAttr[KEY_PASSWORD].fSupplied) {
				SQLReadFileDSN(lpsetupdlg->aAttr[KEY_FILEDSN].szAttr,
					ODBC, INI_KPASSWORD,
					lpsetupdlg->aAttr[KEY_PASSWORD].szAttr,
					sizeof(lpsetupdlg->aAttr[KEY_PASSWORD].szAttr), NULL);
			}
			if (!lpsetupdlg->aAttr[KEY_SERVER].fSupplied) {
				SQLReadFileDSN(lpsetupdlg->aAttr[KEY_FILEDSN].szAttr,
					ODBC, INI_KSERVER,
					lpsetupdlg->aAttr[KEY_SERVER].szAttr,
					sizeof(lpsetupdlg->aAttr[KEY_SERVER].szAttr), NULL);
			}
			if (!lpsetupdlg->aAttr[KEY_SERVER].fSupplied) {
				SQLReadFileDSN(lpsetupdlg->aAttr[KEY_FILEDSN].szAttr,
					ODBC, INI_KSERVER,
					lpsetupdlg->aAttr[KEY_SERVER].szAttr,
					sizeof(lpsetupdlg->aAttr[KEY_SERVER].szAttr), NULL);
			}
			if (!lpsetupdlg->aAttr[KEY_SERVERPORT].fSupplied) {
				SQLReadFileDSN(lpsetupdlg->aAttr[KEY_FILEDSN].szAttr,
					ODBC, INI_KSERVERPORT,
					lpsetupdlg->aAttr[KEY_SERVERPORT].szAttr,
					sizeof(lpsetupdlg->aAttr[KEY_SERVERPORT].szAttr), NULL);
			}
			if (!lpsetupdlg->aAttr[KEY_LOCALPORT].fSupplied) {
				SQLReadFileDSN(lpsetupdlg->aAttr[KEY_FILEDSN].szAttr,
					ODBC, INI_KLOCALPORT,
					lpsetupdlg->aAttr[KEY_LOCALPORT].szAttr,
					sizeof(lpsetupdlg->aAttr[KEY_LOCALPORT].szAttr), NULL);
			}
			if (!lpsetupdlg->aAttr[KEY_ENCRYPTION].fSupplied) {
				if (!SQLReadFileDSN(lpsetupdlg->aAttr[KEY_FILEDSN].szAttr,
					ODBC, INI_KENCRYPTION,
					lpsetupdlg->aAttr[KEY_ENCRYPTION].szAttr,
					sizeof(lpsetupdlg->aAttr[KEY_ENCRYPTION].szAttr), NULL))
					strcpy(lpsetupdlg->aAttr[KEY_ENCRYPTION].szAttr, "Yes");
			}
		}
		else {
			if (!lpsetupdlg->aAttr[KEY_DATABASE].fSupplied) {
				lstrcpy(lpsetupdlg->aAttr[KEY_DATABASE].szAttr, lpsetupdlg->aAttr[KEY_DSN].szAttr);
				i1 = lstrlen(lpsetupdlg->aAttr[KEY_DATABASE].szAttr);
				if (i1 > 4 && !lstrcmpi(lpsetupdlg->aAttr[KEY_DATABASE].szAttr + i1 - 4, ".dsn"))
					lpsetupdlg->aAttr[KEY_DATABASE].szAttr[i1 - 4] = '\0';
			}
			if (!lpsetupdlg->aAttr[KEY_ENCRYPTION].fSupplied) {
				strcpy(lpsetupdlg->aAttr[KEY_ENCRYPTION].szAttr, "Yes");
			}
		}

		// Display dialog(s)
		retcode = (IDOK == DialogBoxParam(s_hModule,
									  MAKEINTRESOURCE(CONFIGFILEDSN),
									  hwnd,
									  (DLGPROC) ConfigDlgProc,
									  (LPARAM)lpsetupdlg)) ? 0 : 1;
		if (!retcode) {
			i1 = 0;
			/* put in reverse order as ODBC manager will reverse them again */
			if (lpsetupdlg->aAttr[KEY_ENCRYPTION].szAttr[0]) {
				lstrcpy(lpszAttributes + i1, INI_KENCRYPTION);
				i1 += lstrlen(lpszAttributes + i1);
				lpszAttributes[i1++] = '=';
				lstrcpy(lpszAttributes + i1, lpsetupdlg->aAttr[KEY_ENCRYPTION].szAttr);
				i1 += lstrlen(lpszAttributes + i1);
				lpszAttributes[i1++] = ';';
			}
			if (lpsetupdlg->aAttr[KEY_LOCALPORT].szAttr[0]) {
				lstrcpy(lpszAttributes + i1, INI_KLOCALPORT);
				i1 += lstrlen(lpszAttributes + i1);
				lpszAttributes[i1++] = '=';
				lstrcpy(lpszAttributes + i1, lpsetupdlg->aAttr[KEY_LOCALPORT].szAttr);
				i1 += lstrlen(lpszAttributes + i1);
				lpszAttributes[i1++] = ';';
			}
			if (lpsetupdlg->aAttr[KEY_SERVERPORT].szAttr[0]) {
				lstrcpy(lpszAttributes + i1, INI_KSERVERPORT);
				i1 += lstrlen(lpszAttributes + i1);
				lpszAttributes[i1++] = '=';
				lstrcpy(lpszAttributes + i1, lpsetupdlg->aAttr[KEY_SERVERPORT].szAttr);
				i1 += lstrlen(lpszAttributes + i1);
				lpszAttributes[i1++] = ';';
			}
			if (lpsetupdlg->aAttr[KEY_SERVER].szAttr[0]) {
				lstrcpy(lpszAttributes + i1, INI_KSERVER);
				i1 += lstrlen(lpszAttributes + i1);
				lpszAttributes[i1++] = '=';
				lstrcpy(lpszAttributes + i1, lpsetupdlg->aAttr[KEY_SERVER].szAttr);
				i1 += lstrlen(lpszAttributes + i1);
				lpszAttributes[i1++] = ';';
			}
			if (lpsetupdlg->aAttr[KEY_DATABASE].szAttr[0]) {
				lstrcpy(lpszAttributes + i1, INI_KDATABASE);
				i1 += lstrlen(lpszAttributes + i1);
				lpszAttributes[i1++] = '=';
				lstrcpy(lpszAttributes + i1, lpsetupdlg->aAttr[KEY_DATABASE].szAttr);
				i1 += lstrlen(lpszAttributes + i1);
				lpszAttributes[i1++] = ';';
			}
			if (lpsetupdlg->aAttr[KEY_PASSWORD].szAttr[0]) {
				lstrcpy(lpszAttributes + i1, INI_KPASSWORD);
				i1 += lstrlen(lpszAttributes + i1);
				lpszAttributes[i1++] = '=';
				lstrcpy(lpszAttributes + i1, lpsetupdlg->aAttr[KEY_PASSWORD].szAttr);
				i1 += lstrlen(lpszAttributes + i1);
				lpszAttributes[i1++] = ';';
			}
			if (lpsetupdlg->aAttr[KEY_UID].szAttr[0]) {
				lstrcpy(lpszAttributes + i1, INI_KUID);
				i1 += lstrlen(lpszAttributes + i1);
				lpszAttributes[i1++] = '=';
				lstrcpy(lpszAttributes + i1, lpsetupdlg->aAttr[KEY_UID].szAttr);
				i1 += lstrlen(lpszAttributes + i1);
				lpszAttributes[i1++] = ';';
			}
			lpszAttributes[i1] = '\0';
		}
	}
	else retcode = -1;

	free(lpsetupdlg);
	return retcode;
}

/* ConfigDSN ---------------------------------------------------------------
  Description:  ODBC Setup entry point
				This entry point is called by the ODBC Installer
				(see file header for more details)
  Input      :  hwnd ----------- Parent window handle
				fRequest ------- Request type (i.e., add, config, or remove)
				lpszDriver ----- Driver name
				lpszAttributes - data source attribute string
  Output     :  TRUE success, FALSE otherwise
--------------------------------------------------------------------------*/
BOOL CALLBACK ConfigDSN(	HWND	 hwnd,
					WORD	 fRequest,
					LPCSTR  lpszDriver,
					LPCSTR  lpszAttributes)
{
	BOOL  fSuccess;						   // Success/fail flag
	LPSETUPDLG lpsetupdlg;

	// Allocate attribute array
	lpsetupdlg = (LPSETUPDLG) malloc(sizeof(SETUPDLG));
	if (lpsetupdlg == NULL) return FALSE;
	memset(lpsetupdlg, 0, sizeof(SETUPDLG));

	// Parse attribute string
	if (lpszAttributes) ParseAttributes(lpszAttributes, lpsetupdlg);

	// Save original data source name
	if (lpsetupdlg->aAttr[KEY_DSN].fSupplied)
		lstrcpy(lpsetupdlg->szDSN, lpsetupdlg->aAttr[KEY_DSN].szAttr);
	else lpsetupdlg->szDSN[0] = '\0';

	// Remove data source
	if (ODBC_REMOVE_DSN == fRequest) {
		// Fail if no data source name was supplied
		if (!lpsetupdlg->aAttr[KEY_DSN].fSupplied) fSuccess = FALSE;
		// Otherwise remove data source from ODBC.INI
		else fSuccess = SQLRemoveDSNFromIni(lpsetupdlg->aAttr[KEY_DSN].szAttr);
	}

	// Add or Configure data source
	else {
		// Save passed variables for global access (e.g., dialog access)
		lpsetupdlg->hwndParent = hwnd;
		lpsetupdlg->lpszDrvr = lpszDriver;
		lpsetupdlg->fNewDSN	 = (ODBC_ADD_DSN == fRequest);
		lpsetupdlg->fFileDSN = FALSE;

		// Display the appropriate dialog (if parent window handle supplied)
		if (hwnd) {
			// initialize any attributes not supplied
			// NOTE: Values supplied in the attribute string will always
			//		 override settings in ODBC.INI
			if (lpsetupdlg->aAttr[KEY_DSN].fSupplied) {
				if (!lpsetupdlg->aAttr[KEY_DATABASE].fSupplied) {
					if (SQLGetPrivateProfileString(lpsetupdlg->szDSN, INI_KDATABASE,
						EMPTYSTR,
						lpsetupdlg->aAttr[KEY_DATABASE].szAttr,
						sizeof(lpsetupdlg->aAttr[KEY_DATABASE].szAttr),
						ODBC_INI)) {
/*** NOTE: UNCOMMENT IF USING AUTO SET FOR DATABASE (SEE BELOW 'NOTE') ***/
							/* lpsetupdlg->aAttr[KEY_DATABASE].fSupplied = TRUE */;
						}
				}
				if (!lpsetupdlg->aAttr[KEY_UID].fSupplied) {
					SQLGetPrivateProfileString(lpsetupdlg->szDSN, INI_KUID,
						EMPTYSTR,
						lpsetupdlg->aAttr[KEY_UID].szAttr,
						sizeof(lpsetupdlg->aAttr[KEY_UID].szAttr),
						ODBC_INI);
				}
				if (!lpsetupdlg->aAttr[KEY_PASSWORD].fSupplied) {
					SQLGetPrivateProfileString(lpsetupdlg->szDSN, INI_KPASSWORD,
						EMPTYSTR,
						lpsetupdlg->aAttr[KEY_PASSWORD].szAttr,
						sizeof(lpsetupdlg->aAttr[KEY_PASSWORD].szAttr),
						ODBC_INI);
				}
				if (!lpsetupdlg->aAttr[KEY_DESCRIPTION].fSupplied) {
					SQLGetPrivateProfileString(lpsetupdlg->szDSN, INI_KDESCRIPTION,
						EMPTYSTR,
						lpsetupdlg->aAttr[KEY_DESCRIPTION].szAttr,
						sizeof(lpsetupdlg->aAttr[KEY_DESCRIPTION].szAttr),
						ODBC_INI);
				}
				if (!lpsetupdlg->aAttr[KEY_SERVER].fSupplied) {
					SQLGetPrivateProfileString(lpsetupdlg->szDSN, INI_KSERVER,
						EMPTYSTR,
						lpsetupdlg->aAttr[KEY_SERVER].szAttr,
						sizeof(lpsetupdlg->aAttr[KEY_SERVER].szAttr),
						ODBC_INI);
				}
				if (!lpsetupdlg->aAttr[KEY_SERVERPORT].fSupplied) {
					if (!SQLGetPrivateProfileString(lpsetupdlg->szDSN, INI_KSERVERPORT,
						EMPTYSTR,
						lpsetupdlg->aAttr[KEY_SERVERPORT].szAttr,
						sizeof(lpsetupdlg->aAttr[KEY_SERVERPORT].szAttr),
						ODBC_INI)) {
#if 1
/*** CODE: REMOVE WHEN ALL BETA CUSTOMERS HAVE FIXED THEIR FILES ***/
						/* try old key value */
						SQLGetPrivateProfileString(lpsetupdlg->szDSN, "Server Port",
							EMPTYSTR,
							lpsetupdlg->aAttr[KEY_SERVERPORT].szAttr,
							sizeof(lpsetupdlg->aAttr[KEY_SERVERPORT].szAttr),
							ODBC_INI);
#endif
					}
				}
				if (!lpsetupdlg->aAttr[KEY_LOCALPORT].fSupplied) {
					if (!SQLGetPrivateProfileString(lpsetupdlg->szDSN, INI_KLOCALPORT,
						EMPTYSTR,
						lpsetupdlg->aAttr[KEY_LOCALPORT].szAttr,
						sizeof(lpsetupdlg->aAttr[KEY_LOCALPORT].szAttr),
						ODBC_INI)) {
#if 1
/*** CODE: REMOVE WHEN ALL BETA CUSTOMERS HAVE FIXED THEIR FILES ***/
						/* try old key value */
						SQLGetPrivateProfileString(lpsetupdlg->szDSN, "Local Port",
							EMPTYSTR,
							lpsetupdlg->aAttr[KEY_LOCALPORT].szAttr,
							sizeof(lpsetupdlg->aAttr[KEY_LOCALPORT].szAttr),
							ODBC_INI);
#endif
					}
				}
				if (!lpsetupdlg->aAttr[KEY_ENCRYPTION].fSupplied) {
					if (!SQLGetPrivateProfileString(lpsetupdlg->szDSN, INI_KENCRYPTION,
						EMPTYSTR,
						lpsetupdlg->aAttr[KEY_ENCRYPTION].szAttr,
						sizeof(lpsetupdlg->aAttr[KEY_ENCRYPTION].szAttr),
						ODBC_INI)) strcpy(lpsetupdlg->aAttr[KEY_ENCRYPTION].szAttr, tcpissslsupported() ? "Yes" : "No");
				}
			}
			else strcpy(lpsetupdlg->aAttr[KEY_ENCRYPTION].szAttr, tcpissslsupported() ? "Yes" : "No");

			// Display dialog(s)
			fSuccess = (IDOK == DialogBoxParam(s_hModule,
										  MAKEINTRESOURCE(CONFIGDSN),
										  hwnd,
										  (DLGPROC) ConfigDlgProc,
										  (LPARAM)lpsetupdlg));

			// Update ODBC.INI
			if (fSuccess) fSuccess = SetDSNAttributes(hwnd, lpsetupdlg);
		}
		else if (lpsetupdlg->aAttr[KEY_DSN].fSupplied)
			fSuccess = SetDSNAttributes(hwnd, lpsetupdlg);
		else fSuccess = FALSE;
	}

	free(lpsetupdlg);
	return fSuccess;
}

/* ConfigDlgProc -----------------------------------------------------------
  Description:  Manage add data source name dialog
  Input      :  hdlg --- Dialog window handle
				wMsg --- Message
				wParam - Message parameter
				lParam - Message parameter
  Output     :  TRUE if message processed, FALSE otherwise
--------------------------------------------------------------------------*/
LRESULT CALLBACK ConfigDlgProc(HWND hdlg, UINT wMsg, WPARAM wParam,
		LPARAM lParam)
{
	switch (wMsg) {
	// Initialize the dialog
	case WM_INITDIALOG: {
		LPSETUPDLG lpsetupdlg;
		LPCSTR lpszDSN;

		SetWindowLongPtr(hdlg, DWLP_USER, (LONG_PTR) lParam);
		CenterDialog(hdlg);
		/*** CODE: FOLLOWING CAN CHANGE THE TITLE ***/
//		SetWindowText(hdlg, (LPSTR) companyinfo);
		lpsetupdlg = (LPSETUPDLG) lParam;
		lpszDSN = lpsetupdlg->aAttr[KEY_DSN].szAttr;
		// Initialize dialog fields
		SetDlgItemText(hdlg, IDC_DSNAME, lpszDSN);
		SetDlgItemText(hdlg, IDC_DATABASE,
				lpsetupdlg->aAttr[KEY_DATABASE].szAttr);
		SetDlgItemText(hdlg, IDC_DESCRIPTION,
				lpsetupdlg->aAttr[KEY_DESCRIPTION].szAttr);
		SetDlgItemText(hdlg, IDC_UID, lpsetupdlg->aAttr[KEY_UID].szAttr);
		SetDlgItemText(hdlg, IDC_PASSWORD,
				lpsetupdlg->aAttr[KEY_PASSWORD].szAttr);
		SetDlgItemText(hdlg, IDC_SERVER, lpsetupdlg->aAttr[KEY_SERVER].szAttr);
		SetDlgItemText(hdlg, IDC_SERVERPORT,
				lpsetupdlg->aAttr[KEY_SERVERPORT].szAttr);
		SetDlgItemText(hdlg, IDC_LOCALPORT,
				lpsetupdlg->aAttr[KEY_LOCALPORT].szAttr);
		CheckDlgButton(hdlg, IDC_ENCRYPTION,
				!_stricmp(lpsetupdlg->aAttr[KEY_ENCRYPTION].szAttr, "Yes"));
		if (!lpsetupdlg->fFileDSN) {
			SendDlgItemMessage(hdlg, IDC_DSNAME, EM_LIMITTEXT,
					(WPARAM) (SQL_MAX_DSN_LENGTH - 1), 0L);
		} else {
			EnableWindow(GetDlgItem(hdlg, IDC_DSNAME), FALSE);
		}
		SendDlgItemMessage(hdlg, IDC_DATABASE, EM_LIMITTEXT,
				(WPARAM) (MAXPATHLEN - 1), 0L);
		if (!lpsetupdlg->fFileDSN) {
			SendDlgItemMessage(hdlg, IDC_DESCRIPTION, EM_LIMITTEXT,
					(WPARAM) (MAXDESC - 1), 0L);
		} else {
			EnableWindow(GetDlgItem(hdlg, IDC_DESCRIPTION), FALSE);
		}
		SendDlgItemMessage(hdlg, IDC_UID, EM_LIMITTEXT,
				(WPARAM) (MAXSERVER - 1), 0L);
		SendDlgItemMessage(hdlg, IDC_SERVER, EM_LIMITTEXT,
				(WPARAM) (MAXSERVER - 1), 0L);
		SendDlgItemMessage(hdlg, IDC_SERVERPORT, EM_LIMITTEXT,
				(WPARAM) (MAXPORT - 1), 0L);
		SendDlgItemMessage(hdlg, IDC_LOCALPORT, EM_LIMITTEXT,
				(WPARAM) (MAXPORT - 1), 0L);
		if (!(lpsetupdlg->fFileDSN || tcpissslsupported())) {
			EnableWindow(GetDlgItem(hdlg, IDC_ENCRYPTION), FALSE);
		}
//		if (!lpsetupdlg->aAttr[KEY_DSN].szAttr[0] || !lpsetupdlg->aAttr[KEY_SERVER].szAttr[0])
//			EnableWindow(GetDlgItem(hdlg, IDOK), FALSE);
		return TRUE;						// Focus was not set
	}

		// Process buttons
	case WM_COMMAND:
		switch (GET_WM_COMMAND_ID(wParam, lParam)) {
		// Ensure the OK button is enabled only when a data source name
		// and server name is entered
		case IDC_DSNAME:
		case IDC_DATABASE:
		case IDC_SERVER:
			if (GET_WM_COMMAND_CMD(wParam, lParam) == EN_CHANGE) {
				char szItem[255];		// Edit control text
				LPSETUPDLG lpsetupdlg;

				lpsetupdlg = (LPSETUPDLG) (LONG_PTR) GetWindowLongPtr(hdlg,
						DWLP_USER);
				if (!lpsetupdlg->fFileDSN) {
					// Enable/disable the OK button
					EnableWindow(GetDlgItem(hdlg, IDOK),
					GetDlgItemText(hdlg, IDC_DSNAME, szItem, sizeof(szItem)) &&
					GetDlgItemText(hdlg, IDC_SERVER, szItem, sizeof(szItem)));
#if 0
					// set database if not already set
					/*** NOTE: THIS WILL ONLY WORK IF WE SET fSupplied WHEN WE GET THE DATA ***/
					if (GET_WM_COMMAND_ID(wParam, lParam) == IDC_DSNAME && !lpsetupdlg->aAttr[KEY_DATABASE].fSupplied) {
						GetDlgItemText(hdlg, IDC_DSNAME, szItem, sizeof(szItem));
						SetDlgItemText(hdlg, IDC_DATABASE, szItem);
					}
#endif
				} else {
					// Enable/disable the OK button
					EnableWindow(GetDlgItem(hdlg, IDOK),
							GetDlgItemText(hdlg, IDC_DATABASE, szItem,
									sizeof(szItem))
									&&
									GetDlgItemText(hdlg, IDC_SERVER, szItem,
											sizeof(szItem)));
				}
			}
			break;
		case IDC_ENCRYPTION:
#if 0 //def _DEBUG
			{
				LPSETUPDLG lpsetupdlg;

				lpsetupdlg = (LPSETUPDLG)GetWindowLong(hdlg, DWL_USER);
				if (lpsetupdlg->fFileDSN || tcpissslsupported()) {
					EnableWindow(GetDlgItem(hdlg, IDC_AUTHTEXT), IsDlgButtonChecked(hdlg, IDC_ENCRYPTION));
					EnableWindow(GetDlgItem(hdlg, IDC_AUTHENTICATION), IsDlgButtonChecked(hdlg, IDC_ENCRYPTION));
					return TRUE;
				}
			}
#endif
			break;

			// Accept results
		case IDOK:
			{
				LPSETUPDLG lpsetupdlg = (LPSETUPDLG) (LONG_PTR) GetWindowLongPtr(hdlg,
						DWLP_USER);
				// Retrieve dialog values
				GetDlgItemText(hdlg, IDC_DSNAME, lpsetupdlg->aAttr[KEY_DSN].szAttr,
						sizeof(lpsetupdlg->aAttr[KEY_DSN].szAttr));
				GetDlgItemText(hdlg, IDC_DATABASE,
						lpsetupdlg->aAttr[KEY_DATABASE].szAttr,
						sizeof(lpsetupdlg->aAttr[KEY_DATABASE].szAttr));
				GetDlgItemText(hdlg, IDC_DESCRIPTION,
						lpsetupdlg->aAttr[KEY_DESCRIPTION].szAttr,
						sizeof(lpsetupdlg->aAttr[KEY_DESCRIPTION].szAttr));
				GetDlgItemText(hdlg, IDC_UID, lpsetupdlg->aAttr[KEY_UID].szAttr,
						sizeof(lpsetupdlg->aAttr[KEY_UID].szAttr));
				GetDlgItemText(hdlg, IDC_PASSWORD,
						lpsetupdlg->aAttr[KEY_PASSWORD].szAttr,
						sizeof(lpsetupdlg->aAttr[KEY_PASSWORD].szAttr));
				GetDlgItemText(hdlg, IDC_SERVER,
						lpsetupdlg->aAttr[KEY_SERVER].szAttr,
						sizeof(lpsetupdlg->aAttr[KEY_SERVER].szAttr));
				GetDlgItemText(hdlg, IDC_SERVERPORT,
						lpsetupdlg->aAttr[KEY_SERVERPORT].szAttr,
						sizeof(lpsetupdlg->aAttr[KEY_SERVERPORT].szAttr));
				GetDlgItemText(hdlg, IDC_LOCALPORT,
						lpsetupdlg->aAttr[KEY_LOCALPORT].szAttr,
						sizeof(lpsetupdlg->aAttr[KEY_LOCALPORT].szAttr));
				if (IsDlgButtonChecked(hdlg, IDC_ENCRYPTION))
					strcpy(lpsetupdlg->aAttr[KEY_ENCRYPTION].szAttr, "Yes");
				else
					strcpy(lpsetupdlg->aAttr[KEY_ENCRYPTION].szAttr, "No");
			}
			/* no break */
		case IDCANCEL:
			EndDialog(hdlg, wParam);
			return TRUE;
		}
		break;
	}

	// Message not processed
	return FALSE;
}

/* CenterDialog ------------------------------------------------------------
	Description:  Center the dialog over the frame window
	Input	   :  hdlg -- Dialog window handle
	Output	   :  None
--------------------------------------------------------------------------*/
static void CenterDialog(HWND hdlg)
{
	HWND	hwndFrame;
	RECT	rcDlg, rcScr, rcFrame;
	int cx, cy;

	hwndFrame = GetParent(hdlg);

	GetWindowRect(hdlg, &rcDlg);
	cx = rcDlg.right  - rcDlg.left;
	cy = rcDlg.bottom - rcDlg.top;

	GetClientRect(hwndFrame, &rcFrame);
	ClientToScreen(hwndFrame, (LPPOINT)(&rcFrame.left));
	ClientToScreen(hwndFrame, (LPPOINT)(&rcFrame.right));
	rcDlg.top    = rcFrame.top  + (((rcFrame.bottom - rcFrame.top) - cy) >> 1);
	rcDlg.left   = rcFrame.left + (((rcFrame.right - rcFrame.left) - cx) >> 1);
	rcDlg.bottom = rcDlg.top  + cy;
	rcDlg.right  = rcDlg.left + cx;

	GetWindowRect(GetDesktopWindow(), &rcScr);
	if (rcDlg.bottom > rcScr.bottom) {
		rcDlg.bottom = rcScr.bottom;
		rcDlg.top    = rcDlg.bottom - cy;
	}
	if (rcDlg.right  > rcScr.right) {
		rcDlg.right = rcScr.right;
		rcDlg.left  = rcDlg.right - cx;
	}

	if (rcDlg.left < 0) rcDlg.left = 0;
	if (rcDlg.top  < 0) rcDlg.top  = 0;

	MoveWindow(hdlg, rcDlg.left, rcDlg.top, cx, cy, TRUE);
	return;
}

/* ParseAttributes ---------------------------------------------------------
  Description:  Parse attribute string moving values into the aAttr array
  Input      :  lpszAttributes - Pointer to attribute string
  Output     :  None (global aAttr normally updated)
--------------------------------------------------------------------------*/
static void ParseAttributes(LPCSTR lpszAttributes, LPSETUPDLG lpsetupdlg)
{
	LPCSTR lpsz;
	LPCSTR lpszStart;
	char aszKey[MAXKEYLEN];
	int iElement;
	ptrdiff_t cbKey;

	for (lpsz=lpszAttributes; *lpsz; lpsz++) {
		//  Extract key name (e.g., DSN), it must be terminated by an equals
		lpszStart = lpsz;
		for ( ; ; lpsz++) {
			if (!*lpsz) return;		// No key was found
			if (*lpsz == '=') break;		// Valid key found
		}
		// Determine the key's index in the key table (-1 if not found)
		iElement = -1;
		cbKey = lpsz - lpszStart;
		if (cbKey < (ptrdiff_t)sizeof(aszKey)) {
			int j;

			_fmemcpy(aszKey, lpszStart, cbKey);
			aszKey[cbKey] = '\0';
			for (j = 0; *s_aLookup[j].szKey; j++) {
				if (!lstrcmpi(s_aLookup[j].szKey, aszKey)) {
					iElement = s_aLookup[j].iKey;
					break;
				}
			}
		}

		// Locate end of key value
		lpszStart = ++lpsz;
		for ( ; *lpsz; lpsz++);

		// Save value if key is known
		// NOTE: This code assumes the szAttr buffers in aAttr have been
		//	   zero initialized
		if (iElement >= 0) {
			lpsetupdlg->aAttr[iElement].fSupplied = TRUE;
			_fmemcpy(lpsetupdlg->aAttr[iElement].szAttr, lpszStart,
				MIN(lpsz - lpszStart + 1, (int) (sizeof(lpsetupdlg->aAttr[0].szAttr) - 1)));
		}
	}
}

/* SetDSNAttributes --------------------------------------------------------
  Description:  Write data source attributes to ODBC.INI
  Input      :  hwnd - Parent window handle (plus globals)
  Output     :  TRUE if successful, FALSE otherwise
--------------------------------------------------------------------------*/
static BOOL SetDSNAttributes(HWND hwndParent, LPSETUPDLG lpsetupdlg)
{
	LPCSTR	lpszDSN;  // Pointer to data source name

	lpszDSN = lpsetupdlg->aAttr[KEY_DSN].szAttr;

	// Validate arguments
	if (lpsetupdlg->fNewDSN && !*lpsetupdlg->aAttr[KEY_DSN].szAttr) return FALSE;

	// Write the data source name
	if (!SQLWriteDSNToIni(lpszDSN, lpsetupdlg->lpszDrvr)) {
		if (hwndParent) {
			char  szBuf[MAXPATHLEN];
			char  szMsg[MAXPATHLEN];

			LoadString(s_hModule, IDS_BADDSN, szBuf, sizeof(szBuf));
			wsprintf(szMsg, szBuf, lpszDSN);
			LoadString(s_hModule, IDS_MSGTITLE, szBuf, sizeof(szBuf));
			MessageBox(hwndParent, szMsg, szBuf, MB_ICONEXCLAMATION | MB_OK);
		}
		return FALSE;
	}

	// Update ODBC.INI
	// Save the value if the data source is new, if it was edited, or if
	// it was explicitly supplied
	if (hwndParent || lpsetupdlg->aAttr[KEY_DATABASE].fSupplied)
		SQLWritePrivateProfileString(lpszDSN, INI_KDATABASE,
			lpsetupdlg->aAttr[KEY_DATABASE].szAttr, ODBC_INI);
	if (hwndParent || lpsetupdlg->aAttr[KEY_DESCRIPTION].fSupplied)
		SQLWritePrivateProfileString(lpszDSN, INI_KDESCRIPTION,
			lpsetupdlg->aAttr[KEY_DESCRIPTION].szAttr, ODBC_INI);
	if (hwndParent || lpsetupdlg->aAttr[KEY_UID].fSupplied)
		SQLWritePrivateProfileString(lpszDSN, INI_KUID,
			lpsetupdlg->aAttr[KEY_UID].szAttr, ODBC_INI);
	if (hwndParent || lpsetupdlg->aAttr[KEY_PASSWORD].fSupplied)
		SQLWritePrivateProfileString(lpszDSN, INI_KPASSWORD,
			lpsetupdlg->aAttr[KEY_PASSWORD].szAttr, ODBC_INI);
	if (hwndParent || lpsetupdlg->aAttr[KEY_SERVER].fSupplied)
		SQLWritePrivateProfileString(lpszDSN, INI_KSERVER,
			lpsetupdlg->aAttr[KEY_SERVER].szAttr, ODBC_INI);
	if (hwndParent || lpsetupdlg->aAttr[KEY_SERVERPORT].fSupplied) {
		SQLWritePrivateProfileString(lpszDSN, INI_KSERVERPORT,
			lpsetupdlg->aAttr[KEY_SERVERPORT].szAttr, ODBC_INI);
#if 1
/*** CODE: REMOVE WHEN ALL BETA CUSTOMERS HAVE FIXED THEIR FILES ***/
		/* remove old key value */
		SQLWritePrivateProfileString(lpsetupdlg->szDSN, "Server Port",
			NULL, ODBC_INI);
#endif
	}
	if (hwndParent || lpsetupdlg->aAttr[KEY_LOCALPORT].fSupplied) {
		SQLWritePrivateProfileString(lpszDSN, INI_KLOCALPORT,
			lpsetupdlg->aAttr[KEY_LOCALPORT].szAttr, ODBC_INI);
#if 1
/*** CODE: REMOVE WHEN ALL BETA CUSTOMERS HAVE FIXED THEIR FILES ***/
		/* remove old key value */
		SQLWritePrivateProfileString(lpsetupdlg->szDSN, "Local Port",
			NULL, ODBC_INI);
#endif
	}
	if (hwndParent || lpsetupdlg->aAttr[KEY_ENCRYPTION].fSupplied)
		SQLWritePrivateProfileString(lpszDSN, INI_KENCRYPTION,
			lpsetupdlg->aAttr[KEY_ENCRYPTION].szAttr, ODBC_INI);
	// If the data source name has changed, remove the old name
	if (lpsetupdlg->aAttr[KEY_DSN].fSupplied &&
		lstrcmpi(lpsetupdlg->szDSN, lpsetupdlg->aAttr[KEY_DSN].szAttr)) {
		SQLRemoveDSNFromIni(lpsetupdlg->szDSN);
	}
	return TRUE;
}
