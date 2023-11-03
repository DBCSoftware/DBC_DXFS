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

#include "includes.h"
#include "base.h"
#include "gui.h"
#define GUIADDE 1
#include "guix.h"

LRESULT CALLBACK guiaDdeClientProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK guiaDdeServerProc(HWND, UINT, WPARAM, LPARAM);

static void guiaDdeCallback(INT, BYTE *, ATOM, BYTE *, INT);

static INT ddeused;					/* number of used dde structures */

/**
// DDE management functions

// Message format for ddecb
// Byte Range       Description
// --------------   -----------
//  0 -  7          DDEMSG
//  8 - 11          function code
// 12 - 43          application
// 44 - 75          topic
// 76 - 97          item
// 98 - datalen     optional data
 */
INT guiddestart(BYTE *ddename, void (*callback)(INT, BYTE *, INT))
{
	INT namelen, namepos;
	DBCDDE *ddeptr;
	WNDCLASS class;

	if (!ddeinitflg) {
		class.style = 0;
		class.cbClsExtra = 0;
		class.cbWndExtra = 4;
		class.hInstance = guia_hinst();
		class.hIcon = NULL;
		class.hCursor = NULL;
		class.hbrBackground = NULL;
		class.lpszMenuName = NULL;
		class.lpszClassName = clientclass;
		class.lpfnWndProc = (LPVOID) guiaDdeClientProc;
		RegisterClass(&class);

		class.lpszClassName = serverclass;
		class.lpfnWndProc = (LPVOID) guiaDdeServerProc;
		RegisterClass(&class);
		ddeinitflg = TRUE;
	}

	for (ddeused = 0; ddeused < ddemax && ddehead[ddeused].type; ddeused++);
	if (ddeused == ddemax) {
		if (ddehead == NULL) ddeptr = (DBCDDE *) guiAllocMem((ddemax + 1) * sizeof(DBCDDE));
		else ddeptr = (DBCDDE *) guiReallocMem((ZHANDLE) ddehead, (ddemax + 1) * sizeof(DBCDDE));
		if (ddeptr == NULL) return RC_NO_MEM;
		ddehead = ddeptr;
		ddemax++;
	}
	ddeptr = &ddehead[ddeused];
	memset(ddeptr, 0, sizeof(DBCDDE));

	/* Parse/allocate application and topic atoms */
	namelen = (INT)strlen((CHAR *) ddename);
	for (namepos = 10; namepos < namelen && ddename[namepos] != '!'; namepos++);
	if (namepos >= namelen) return RC_ERROR;

	ddeptr->hApplication = guiAllocString(&ddename[10], namepos - 10);
	ddeptr->hTopic = guiAllocString(&ddename[namepos + 1], (INT)strlen((CHAR *) &ddename[namepos + 1]));
	if (ddeptr->hApplication == NULL || ddeptr->hTopic == NULL) goto guiddestartx;

	ddeptr->callback = callback;
	if (!guiamemicmp(ddename, (BYTE *) "DDESERVER", 9)) {
		SendMessage(guia_hwnd(), GUIAM_DDESERVER, (WPARAM) 0, (LPARAM) ddeptr);
		if (ddeptr->hwndOwner == NULL) return RC_ERROR;
		ddeptr->type = DDE_TYPE_OPEN | DDE_TYPE_WINDOW | DDE_TYPE_SERVER;
	}
	else if (!guiamemicmp(ddename, (BYTE *) "DDECLIENT", 9)) {
		ddeptr->aApplication = GlobalAddAtom((LPCTSTR) ddeptr->hApplication);
		ddeptr->aTopic = GlobalAddAtom((LPCTSTR) ddeptr->hTopic);
		if (!ddeptr->aApplication || !ddeptr->aTopic) goto guiddestartx;

		SendMessage(guia_hwnd(), GUIAM_DDECLIENT, (WPARAM) 0, (LPARAM) ddeptr);
		if (ddeptr->hwndOwner == NULL) goto guiddestartx;

		ddeptr->acktype = WM_DDE_INITIATE;
		SendMessage(HWND_BROADCAST, WM_DDE_INITIATE, (WPARAM) ddeptr->hwndOwner,
			MAKELPARAM(ddeptr->aApplication, ddeptr->aTopic));

		if (ddeptr->hwndRemote == NULL) {  /* no response from server */
/*** CODE: WILL THIS WORK FROM THIS THREAD ??? ***/
			DestroyWindow(ddeptr->hwndOwner);
			goto guiddestartx;
		}
		ddeptr->acktype = 0;

		GlobalDeleteAtom(ddeptr->aApplication);
		GlobalDeleteAtom(ddeptr->aTopic);
		ddeptr->type = DDE_TYPE_OPEN | DDE_TYPE_WINDOW | DDE_TYPE_CLIENT;
	}
	else goto guiddestartx;

	return(ddeused + 1);

guiddestartx:
	if (ddeptr->aApplication) GlobalDeleteAtom(ddeptr->aApplication);
	if (ddeptr->aTopic) GlobalDeleteAtom(ddeptr->aTopic);
	if (ddeptr->hApplication != NULL) guiFreeMem(ddeptr->hApplication);
	if (ddeptr->hTopic != NULL) guiFreeMem(ddeptr->hTopic);
	return RC_ERROR;
}

void guiddestop(INT ddehandle)
{
	DBCDDE *ddeptr;

	if (--ddehandle >= 0) {
		if (ddehandle < ddemax && (ddehead[ddehandle].type & DDE_TYPE_OPEN)) {
			ddeptr = &ddehead[ddehandle];
			ddeptr->type &= ~DDE_TYPE_OPEN;
			PostMessage(ddeptr->hwndOwner, WM_CLOSE, 0, 0L);
			guiFreeMem(ddeptr->hApplication);
			guiFreeMem(ddeptr->hTopic);
			ddeptr->hApplication = ddeptr->hTopic = NULL;
		}
	}
}

INT guiddechange(INT ddehandle, BYTE *fnc, INT fnclen, BYTE *data, INT datalen)
{
	ATOM aItem;
	BYTE work[100], respflg;
	INT oplen;
	DDEADVISE *lpDdeAdvise;
	DDEDATA *lpDdeData;
	DDEPOKE *lpDdePoke;
	BYTE *lpCommands;
	GLOBALHANDLE hCommands, hDdeAdvise, hDdeData, hDdePoke;
	DDEACK DdeAck;
	WORD wStatus, wCount;
	MSG msg;
	HWND hwndServer, hwndClient;
	DDEPEEK ddepeek;

	ddehandle--;
	if (ddehandle >= ddemax || !(ddehead[ddehandle].type & DDE_TYPE_OPEN)) return RC_ERROR;

	for (oplen = 0; oplen < fnclen && fnc[oplen] != '.'; oplen++);
	if (oplen < fnclen) {
		oplen++;
		memcpy(work, &fnc[oplen], fnclen - oplen);
		work[fnclen - oplen] = '\0';
		aItem = GlobalAddAtom((LPCTSTR) work);
	}
	else aItem = 0;

	if (ddehead[ddehandle].type & DDE_TYPE_SERVER) {  /* SERVER */
		hwndServer = ddehead[ddehandle].hwndOwner;
		hwndClient = ddehead[ddehandle].hwndRemote;
		if (oplen == 5 && !memcmp(fnc, "DATA.", 5)) {
			respflg = TRUE;
			pvistart();
			if (ddehead[ddehandle].iCount) {
				pviend();
				Sleep(1000);
				pvistart();
				ddehead[ddehandle].iCount--;
			}
			pviend();
		}
		else if (oplen == 7 && !memcmp(fnc, "ADVISE.", 7)) {
			respflg = FALSE;
		}
		else if (oplen == 5 && !memcmp(fnc, "NONE.", 5)) {
			/* No response to request so negative ack */
			DdeAck.bAppReturnCode = 0;
			DdeAck.reserved = 0;
			DdeAck.fBusy = DdeAck.fAck = FALSE;
			wStatus = *(WORD *)&DdeAck;
			if (!PostMessage(hwndClient, WM_DDE_ACK, (WPARAM) hwndServer,
					PackDDElParam(WM_DDE_ACK, wStatus, aItem))) GlobalDeleteAtom(aItem);
			return 0;
		}
		else return RC_ERROR;

		hDdeData = GlobalAlloc(GHND | GMEM_DDESHARE, sizeof(DDEDATA) + datalen + 3);
		if (hDdeData == NULL) return RC_ERROR;
		lpDdeData = (DDEDATA *) GlobalLock(hDdeData);
		if (lpDdeData == NULL) return RC_ERROR;
		lpDdeData->fResponse = respflg;
		lpDdeData->fRelease = FALSE;  /* server should delete hData */
		lpDdeData->cfFormat = CF_TEXT;
		lpDdeData->fAckReq = TRUE;
		memcpy(lpDdeData->Value, data, datalen);
		lpDdeData->Value[datalen] = 0x0d;
		lpDdeData->Value[datalen + 1] = 0x0a;
		lpDdeData->Value[datalen + 2] = 0;

		GlobalUnlock(hDdeData);

		if (!PostMessage(hwndClient, WM_DDE_DATA, (WPARAM) hwndServer,
				PackDDElParam(WM_DDE_DATA, (UINT_PTR) hDdeData, aItem))) {
			/* Unable to post message */
			if (hDdeData != NULL) GlobalFree(hDdeData);
			GlobalDeleteAtom(aItem);
		}
		else {
			pvistart();
			ddehead[ddehandle].hDdeData = hDdeData;
			if (respflg) ddehead[ddehandle].iCount++;
			pviend();
		}
	}
	else {  /* type = CLIENT */
		hwndServer = ddehead[ddehandle].hwndRemote;
		hwndClient = ddehead[ddehandle].hwndOwner;
		if (oplen == 8 && !memcmp(fnc, "REQUEST.", 8)) {
			if (!aItem) return RC_ERROR;
			PostMessage(hwndServer, WM_DDE_REQUEST, (WPARAM) hwndClient,
				PackDDElParam(WM_DDE_REQUEST, CF_TEXT, aItem));
		}
		else if (oplen == 5 && !memcmp(fnc, "DATA.", 5)) {  /* Poke */
			hDdePoke = GlobalAlloc(GHND | GMEM_DDESHARE, sizeof(DDEPOKE) + datalen + 3);
			if (hDdePoke == NULL) return RC_ERROR;
			ddehead[ddehandle].hDdeData = hDdePoke;
			lpDdePoke = (DDEPOKE *) GlobalLock(hDdePoke);
			if (lpDdePoke == NULL) return RC_ERROR;

			lpDdePoke->fRelease = 1;
			lpDdePoke->cfFormat = CF_TEXT;
			memcpy(lpDdePoke->Value, data, datalen);
			lpDdePoke->Value[datalen] = 0x0d;
			lpDdePoke->Value[datalen + 1] = 0x0a;
			lpDdePoke->Value[datalen + 2] = '\0';

			GlobalUnlock(hDdePoke);

			if (!PostMessage(hwndServer, WM_DDE_POKE, (WPARAM) hwndClient,
				PackDDElParam(WM_DDE_POKE, (UINT_PTR) hDdePoke, aItem)))
			{
				GlobalFree(hDdePoke);
				GlobalDeleteAtom(aItem);
				return RC_ERROR;
			}
		}
		else if (!memcmp(fnc, "HOTLINK.", 8)) {
			hDdeAdvise = GlobalAlloc(GHND | GMEM_DDESHARE, sizeof(DDEADVISE));
			lpDdeAdvise = (DDEADVISE *) GlobalLock(hDdeAdvise);
			lpDdeAdvise->fAckReq = TRUE;
			lpDdeAdvise->fDeferUpd = FALSE;
			lpDdeAdvise->cfFormat = CF_TEXT;

			GlobalUnlock(hDdeAdvise);

			if (!PostMessage(hwndServer, WM_DDE_ADVISE, (WPARAM) hwndClient,
					PackDDElParam(WM_DDE_ADVISE, (UINT_PTR) hDdeAdvise, aItem))) {
				GlobalFree(hDdeAdvise);
				GlobalDeleteAtom(aItem);
				return RC_ERROR;
			}
			DdeAck.fAck = FALSE;
			ddepeek.msg = &msg;
			ddepeek.hwnd = hwndClient;
			ddepeek.msgmin = WM_DDE_ACK;
			ddepeek.msgmax = WM_DDE_ACK;
			ddepeek.removeflags = PM_REMOVE;
			wCount = 0;
			while (wCount++ < 100) {
				if (SendMessage(guia_hwnd(), GUIAM_DDEPEEK, (WPARAM) 0, (LPARAM) &ddepeek)) {
					GlobalDeleteAtom(HIWORD(msg.lParam));
					wStatus = LOWORD(msg.lParam);
					DdeAck = *(DDEACK *)&wStatus;
					if (DdeAck.fAck == FALSE){
						GlobalFree(hDdeAdvise);
					}
					break;
				}
			}
			if (DdeAck.fAck != FALSE) {
				ddepeek.msgmin = WM_DDE_FIRST;
				ddepeek.msgmax = WM_DDE_FIRST;
				while (SendMessage(guia_hwnd(), GUIAM_DDEPEEK, (WPARAM) 0, (LPARAM) &ddepeek)) {
					DispatchMessage(&msg);
				}
			}
		}
		else if (oplen == 9 && !memcmp(fnc, "WARMLINK.", 9)) {
			hDdeAdvise = GlobalAlloc(GHND | GMEM_DDESHARE, sizeof(DDEADVISE));
			lpDdeAdvise = (DDEADVISE *) GlobalLock(hDdeAdvise);
			lpDdeAdvise->fAckReq = TRUE;
			lpDdeAdvise->fDeferUpd = TRUE;
			lpDdeAdvise->cfFormat = CF_TEXT;

			GlobalUnlock(hDdeAdvise);

			if (!PostMessage(hwndServer, WM_DDE_ADVISE, (WPARAM) hwndClient,
					PackDDElParam(WM_DDE_ADVISE, (UINT_PTR) hDdeAdvise, aItem))) {
				GlobalFree(hDdeAdvise);
				GlobalDeleteAtom(aItem);
				return RC_ERROR;
			}
			DdeAck.fAck = FALSE;
			ddepeek.msg = &msg;
			ddepeek.hwnd = hwndClient;
			ddepeek.msgmin = WM_DDE_ACK;
			ddepeek.msgmax = WM_DDE_ACK;
			ddepeek.removeflags = PM_REMOVE;
			wCount = 0;
			while (wCount++ < 100) {
				if (SendMessage(guia_hwnd(), GUIAM_DDEPEEK, (WPARAM) 0, (LPARAM) &ddepeek)) {
					GlobalDeleteAtom(HIWORD(msg.lParam));
					wStatus = LOWORD(msg.lParam);
					DdeAck = *(DDEACK *)&wStatus;
					if (DdeAck.fAck == FALSE){
						GlobalFree(hDdeAdvise);
					}
					break;
				}
			}
			if (DdeAck.fAck != FALSE) {
				ddepeek.msgmin = WM_DDE_FIRST;
				ddepeek.msgmax = WM_DDE_FIRST;
				while (SendMessage(guia_hwnd(), GUIAM_DDEPEEK, (WPARAM) 0, (LPARAM) &ddepeek)) {
					DispatchMessage(&msg);
				}
			}
		}
		else if (oplen == 7 && !memcmp(fnc, "NOLINK.", 7)) {
			PostMessage(hwndServer, WM_DDE_UNADVISE, (WPARAM) hwndClient,
					MAKELPARAM(CF_TEXT, aItem));
		}
		else if (oplen == 7 && !memcmp(fnc, "EXECUTE", 7)) {
			hCommands = GlobalAlloc(GHND | GMEM_DDESHARE, datalen + 1);
			lpCommands = (BYTE *) GlobalLock(hCommands);

			memcpy(lpCommands, data, datalen);
			lpCommands[datalen] = 0;

			GlobalUnlock(hCommands);

			if (!PostMessage(hwndServer, WM_DDE_EXECUTE,
					(WPARAM) hwndClient, (LPARAM) hCommands)) {
				GlobalFree(hCommands);
			}
			else {  /* Wait for ack here to make default WM_DDE_ACK easier */
				/*** CODE GOES HERE ***/
			}
		}
		else {
			return RC_ERROR;
		}
	}
	return 0;
}

LRESULT CALLBACK guiaDdeServerProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	DBCDDE *ddeptr;
	DDEACK DdeAck;
	HGLOBAL hDdeAdvise, hDdePoke;
	DDEADVISE *lpDdeAdvise;
	DDEPOKE *lpDdePoke;
	WORD wStatus;
	INT ddepos, datalen;
	BYTE *data;
	ATOM aApp, aTopic, aItem;
	UINT_PTR nItem, nFormat;

	switch(message) {
/*** CODE: MAKE DELETION OF RESOURCES WORK CORRECTLY ***/
	case WM_DDE_ACK:  /* need to delete according to what was sent */
		for (ddepos = 0; ddepos < ddemax && ddehead[ddepos].hwndOwner != hwnd; ddepos++);
		UnpackDDElParam(WM_DDE_ACK, lParam, (PUINT_PTR)&DdeAck, (PUINT_PTR)&aItem);
		if (ddepos < ddemax) {
			pvistart();
			if (ddehead[ddepos].hwndRemote != NULL) {
				GlobalFree(ddehead[ddepos].hDdeData);
				ddehead[ddepos].hDdeData = NULL;
				if (DdeAck.fAck && ddehead[ddepos].iCount > 0) ddehead[ddepos].iCount--;
			}
			pviend();
		}
		GlobalDeleteAtom(aItem);
		FreeDDElParam(WM_DDE_ACK, lParam);
		break;
	case WM_DDE_INITIATE:
		for (ddepos = 0; ddepos < ddemax && ddehead[ddepos].hwndOwner != hwnd; ddepos++);
		if (ddepos < ddemax) {
			ddeptr = &ddehead[ddepos];
			aApp = GlobalAddAtom((LPCTSTR) ddeptr->hApplication);
			if (!aApp) break;
			aTopic = GlobalAddAtom((LPCTSTR) ddeptr->hTopic);
			if (!aTopic) break;
			if ((!LOWORD(lParam) || LOWORD(lParam) == aApp) &&
				(!HIWORD(lParam) || HIWORD(lParam) == aTopic)) {
				SendMessage((HWND) wParam, WM_DDE_ACK, (WPARAM) hwnd, MAKELPARAM(aApp, aTopic));
				ddeptr->hwndRemote = (HWND) wParam;
			}
			else {
				GlobalDeleteAtom(aApp);
				GlobalDeleteAtom(aTopic);
			}
			return 0;
		}
		break;
	case WM_DDE_REQUEST:
		for (ddepos = 0; ddepos < ddemax && ddehead[ddepos].hwndOwner != hwnd; ddepos++);
		UnpackDDElParam(WM_DDE_REQUEST, lParam, &nFormat, (PUINT_PTR)&aItem);
		FreeDDElParam(WM_DDE_REQUEST, lParam);
		if (ddepos < ddemax) {
			ddeptr = &ddehead[ddepos];
			if (!ddeptr->type) break;
			ddeptr->hwndRemote = (HWND) wParam;
			if (nFormat == CF_TEXT) {
				guiaDdeCallback(ddepos, (BYTE *) "REQU", aItem, NULL, 0);
				GlobalDeleteAtom(aItem);
				/* do not send ack */
				/* Let DB/C server code respond with data or none (neg ack) */
			}
			else {  /* unsupported format so ret neg ack */
				DdeAck.bAppReturnCode = 0;
				DdeAck.reserved = 0;
				DdeAck.fBusy = FALSE;
				DdeAck.fAck = FALSE;
				wStatus = *(WORD *)&DdeAck;
				if (!PostMessage((HWND) wParam, WM_DDE_ACK, (WPARAM) hwnd,
					PackDDElParam(WM_DDE_ACK, wStatus, aItem))) GlobalDeleteAtom(aItem);
			}
			return 0;
		}
		break;
	case WM_DDE_ADVISE:
		for (ddepos = 0; ddepos < ddemax && ddehead[ddepos].hwndOwner != hwnd; ddepos++);
		UnpackDDElParam(WM_DDE_ADVISE, lParam, (PUINT_PTR)&hDdeAdvise, (PUINT_PTR)&nItem);
		FreeDDElParam(WM_DDE_ADVISE, lParam);
		if (ddepos < ddemax) {
			ddeptr = &ddehead[ddepos];
			if (!ddeptr->type) break;
			aItem = (ATOM) nItem;
			lpDdeAdvise = (DDEADVISE *) GlobalLock(hDdeAdvise);
			if (lpDdeAdvise->cfFormat == CF_TEXT) {
				guiaDdeCallback(ddepos, (lpDdeAdvise->fDeferUpd ? (BYTE *) "WARM" : (BYTE *) "HOTL"), aItem, NULL, 0);
			}
			else {  /* Unsupported format so ret neg ack */
				DdeAck.bAppReturnCode = 0;
				DdeAck.reserved = 0;
				DdeAck.fBusy = FALSE;
				DdeAck.fAck = FALSE;
				wStatus = *(WORD *)&DdeAck;
				if (!PostMessage((HWND) wParam, WM_DDE_ACK, (WPARAM) hwnd,
					PackDDElParam(WM_DDE_ACK, wStatus, aItem))) GlobalDeleteAtom(aItem);
			}
			GlobalUnlock(hDdeAdvise);
			GlobalFree(hDdeAdvise);
			goto sendack;
		}
		break;
	case WM_DDE_UNADVISE:
		for (ddepos = 0; ddepos < ddemax && ddehead[ddepos].hwndOwner != hwnd; ddepos++);
		if (ddepos < ddemax) {
			ddeptr = &ddehead[ddepos];
			if (!ddeptr->type) break;
			aItem = HIWORD(lParam);
			if (LOWORD(lParam) == CF_TEXT) {
				guiaDdeCallback(ddepos, (BYTE *) "NOLI", aItem, NULL, 0);
				goto sendack;
			}
			else {  /* Unsupported format so ret neg ack */
				DdeAck.bAppReturnCode = 0;
				DdeAck.reserved = 0;
				DdeAck.fBusy = FALSE;
				DdeAck.fAck = FALSE;
				wStatus = *(WORD *)&DdeAck;
				if (!PostMessage((HWND) wParam, WM_DDE_ACK, (WPARAM) hwnd,
					PackDDElParam(WM_DDE_ACK, wStatus, aItem))) GlobalDeleteAtom(aItem);
			}
			return 0;
		}
		break;
	case WM_DDE_EXECUTE:
		for (ddepos = 0; ddepos < ddemax && ddehead[ddepos].hwndOwner != hwnd; ddepos++);
		if (ddepos < ddemax) {
			ddeptr = &ddehead[ddepos];
			if (!ddeptr->type) break;
			data = GlobalLock((HGLOBAL) lParam);
			datalen = (INT)strlen((CHAR *) data);
			guiaDdeCallback(ddepos, (BYTE *) "EXEC", 0, data, datalen);
			DdeAck.bAppReturnCode = 0;
			DdeAck.reserved = 0;
			DdeAck.fBusy = FALSE;
			DdeAck.fAck = FALSE;
			wStatus = *(WORD *)&DdeAck;
			if (!PostMessage((HWND) wParam, WM_DDE_ACK, (WPARAM) hwnd,
				PackDDElParam(WM_DDE_ACK, wStatus, lParam))) {
				GlobalUnlock((HGLOBAL) lParam);
				GlobalFree((HGLOBAL) lParam);
			}
			return 0;
		}
		break;
	case WM_DDE_POKE:
		for (ddepos = 0; ddepos < ddemax && ddehead[ddepos].hwndOwner != hwnd; ddepos++);
		if (ddepos < ddemax) {
			ddeptr = &ddehead[ddepos];
			if (!ddeptr->type) break;
			UnpackDDElParam(WM_DDE_POKE, lParam, (PUINT_PTR) &hDdePoke, (PUINT_PTR)&nItem);
			FreeDDElParam(WM_DDE_POKE, lParam);
			aItem = (ATOM) nItem;
			lpDdePoke = (DDEPOKE *) GlobalLock(hDdePoke);
			if (lpDdePoke->cfFormat == CF_TEXT) {
				data = lpDdePoke->Value;
				datalen = (INT)strlen((CHAR *) data);
				guiaDdeCallback(ddepos, (BYTE *) "DATA", aItem, data, datalen);
			}
			else {  /* unsupported format so ret neg ack */
				data = NULL;
				DdeAck.bAppReturnCode = 0;
				DdeAck.reserved = 0;
				DdeAck.fBusy = FALSE;
				DdeAck.fAck = FALSE;
				wStatus = *(WORD *)&DdeAck;
				if (!PostMessage((HWND) wParam, WM_DDE_ACK, (WPARAM) hwnd,
					PackDDElParam(WM_DDE_ACK, wStatus, aItem))) GlobalDeleteAtom(aItem);
			}
			if (data != NULL) {
				if (lpDdePoke->fRelease) {
					GlobalUnlock(hDdePoke);
					GlobalFree(hDdePoke);
				}
				else GlobalUnlock(hDdePoke);
				DdeAck.bAppReturnCode = 0;
				DdeAck.reserved = 0;
				DdeAck.fBusy = FALSE;
				DdeAck.fAck = TRUE;
				wStatus = *(WORD *)&DdeAck;
				if (!PostMessage((HWND) wParam, WM_DDE_ACK, (WPARAM) hwnd,
					PackDDElParam(WM_DDE_ACK, wStatus, aItem))) {
					GlobalDeleteAtom(aItem);
				}
			}
			else GlobalUnlock(hDdePoke);
			return 0;
		}
		break;
	case WM_DDE_TERMINATE:
		for (ddepos = 0; ddepos < ddemax && ddehead[ddepos].hwndOwner != hwnd; ddepos++);
		if (ddepos < ddemax) {
			ddeptr = &ddehead[ddepos];
			if (ddeptr->hDdeData != NULL) {
				GlobalFree(ddeptr->hDdeData);
				ddeptr->hDdeData = NULL;
			}
			if (ddeptr->type & DDE_TYPE_WINDOW) {
				if (!(ddeptr->type & DDE_TYPE_TERMINATE)) {
					ddeptr->type |= DDE_TYPE_TERMINATE;
					PostMessage((HWND) wParam, WM_DDE_TERMINATE, (WPARAM) hwnd, 0L);
				}
				ddeptr->hwndRemote = NULL;
				DestroyWindow(hwnd);
			}
			ddeptr->type = 0;
		}
		return 0;
	case WM_CLOSE:
		for (ddepos = 0; ddepos < ddemax && ddehead[ddepos].hwndOwner != hwnd; ddepos++);
		if (ddepos < ddemax) {
			ddeptr = &ddehead[ddepos];
			if (ddeptr->hDdeData != NULL) {
				GlobalFree(ddeptr->hDdeData);
				ddeptr->hDdeData = NULL;
			}
			ddeptr->type |= DDE_TYPE_TERMINATE;
			PostMessage((HWND) ddeptr->hwndRemote, WM_DDE_TERMINATE, (WPARAM) hwnd, 0L);
		}
		return 0;
	}
	return(DefWindowProc(hwnd, message, wParam, lParam));

/* send positive acknowledgment */
sendack:
	DdeAck.bAppReturnCode = 0;
	DdeAck.reserved = 0;
	DdeAck.fBusy = FALSE;
	DdeAck.fAck = TRUE;
	wStatus = *(WORD *)&DdeAck;
	if (!PostMessage((HWND) wParam, WM_DDE_ACK, (WPARAM) hwnd,
		PackDDElParam(WM_DDE_ACK, wStatus, aItem))) {
		GlobalDeleteAtom(aItem);
	}
	return 0;
}

void guiaDdeCallback(INT ddepos, BYTE *fnc, ATOM aItem, BYTE *data, INT datalen)
{
	INT len, bytes;
	BYTE *msg, *ptr;
	DBCDDE *ddeptr;

	ddeptr = &ddehead[ddepos];
	if (!(ddeptr->type & DDE_TYPE_OPEN)) return;  /* callbacks can still occur after close */

	len = 98 + datalen;
	msg = guiAllocMem(len);
	if (msg == NULL) return;
	memset(msg, ' ', len);
	memcpy(msg, "DDEMSG  ", 8);
	memcpy(&msg[8], fnc, strlen((CHAR *) fnc));
	memcpy(&msg[12], ddeptr->hApplication, strlen((CHAR *) ddeptr->hApplication));
	memcpy(&msg[44], ddeptr->hTopic, strlen((CHAR *) ddeptr->hTopic));
	if (aItem) {
		bytes = GlobalGetAtomName(aItem, (LPSTR) &msg[76], 22);
		msg[76 + bytes] = ' ';
	}
	ptr = msg;
	if (datalen) memcpy(&ptr[98], data, datalen);

	ddeptr->callback(ddepos + 1, msg, len);
	guiFreeMem(msg);
}

LRESULT CALLBACK guiaDdeClientProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WORD wStatus;
	INT ddepos, ackflg;
	UINT nItem;
	BYTE *ptr;
	ATOM aItem;
	DBCDDE *ddeptr;
	HGLOBAL hDdeData;
	DDEDATA *lpDdeData;
	DDEACK DdeAck;

	switch (message) {
	case WM_DDE_ACK:  /* need to delete according to what was sent */
		for (ddepos = 0; ddepos < ddemax && ddehead[ddepos].hwndOwner != hwnd; ddepos++);
		if (ddepos < ddemax) {
			ddeptr = &ddehead[ddepos];
			if (ddeptr->acktype == WM_DDE_INITIATE) {
				GlobalDeleteAtom(HIWORD(lParam));
				GlobalDeleteAtom(LOWORD(lParam));
				if (ddeptr->hwndRemote == NULL) ddeptr->hwndRemote = (HWND) wParam;
				else if ((HWND) wParam != ddeptr->hwndRemote) {  /* response from more than 1 server */
					PostMessage((HWND) wParam, WM_DDE_TERMINATE, (WPARAM) hwnd, 0L);
				}
			}
			else {
				UnpackDDElParam(message, lParam, (PUINT_PTR)&wStatus, (PUINT_PTR) &aItem);
				FreeDDElParam(message, lParam);
				DdeAck = *(DDEACK *)&wStatus;
				if (!DdeAck.fAck) {  /* NAK */
					guiaDdeCallback(ddepos, (BYTE *) "NONE", aItem, NULL, 0);
					GlobalFree(ddeptr->hDdeData);
				}
				GlobalDeleteAtom(aItem);
			}
			return 0;
		}
		break;
	case WM_DDE_DATA:
		for (ddepos = 0; ddepos < ddemax && ddehead[ddepos].hwndOwner != hwnd; ddepos++);
		if (ddepos < ddemax) {
			if (!ddehead[ddepos].type) break;
			UnpackDDElParam(WM_DDE_DATA, lParam, (PUINT_PTR) &hDdeData, (PUINT_PTR) &nItem);
			FreeDDElParam(WM_DDE_DATA, lParam);
			aItem = (ATOM) nItem;
/*** NOTE: ADDED TEST FOR DESCARDED OR ZERO SIZE GLOBAL MEMORY HANDLE.  DO NOT KNOW IF THIS ***/
/***       IS CORRECT, BUT IT WILL BEHAVE AS IF GLOBAL MEMORY HANDLE IS NULL.  03-15-01 ***/
			if (hDdeData == NULL || (lpDdeData = (DDEDATA *) GlobalLock(hDdeData)) == NULL) {  /* warm link reply */
				guiaDdeCallback(ddepos, (BYTE *) "ADVI", aItem, NULL, 0);
			}
			else {  /* hot link or request reply */
				if (lpDdeData->cfFormat == CF_TEXT) {
					ptr = &lpDdeData->Value[0];
					guiaDdeCallback(ddepos,
						(lpDdeData->fResponse ? (BYTE *) "DATA" : (BYTE *) "ADVI"),
						aItem, ptr, (INT)strlen((CHAR *) ptr));
				}

				if (lpDdeData->fRelease == TRUE) {
					GlobalUnlock(hDdeData);
					GlobalFree(hDdeData);
					ackflg = FALSE;
				}
				else {
					ackflg = lpDdeData->fAckReq;
					GlobalUnlock(hDdeData);
				}

				/* Acknowledge if ack flag */
				if (ackflg) {
					DdeAck.fAck = TRUE;
					if (!PostMessage((HWND) wParam, WM_DDE_ACK, (WPARAM) hwnd,
							PackDDElParam(WM_DDE_ACK, *(UINT*)&DdeAck, aItem))) {
						GlobalDeleteAtom(aItem);
						break;
					}
				}
				else GlobalDeleteAtom(aItem);
			}
			return 0;
		}
		break;
	case WM_DDE_TERMINATE:
		for (ddepos = 0; ddepos < ddemax && ddehead[ddepos].hwndOwner != hwnd; ddepos++);
		if (ddepos < ddemax) {
			if (ddehead[ddepos].type & DDE_TYPE_WINDOW) {
				if (!(ddehead[ddepos].type & DDE_TYPE_TERMINATE)) {
					ddehead[ddepos].type |= DDE_TYPE_TERMINATE;
					PostMessage((HWND) wParam, WM_DDE_TERMINATE, (WPARAM) hwnd, 0L);
				}
				ddehead[ddepos].hwndRemote = NULL;
				DestroyWindow(hwnd);
			}
			ddehead[ddepos].type = 0;
		}
		return 0;
	case WM_CLOSE:
		for (ddepos = 0; ddepos < ddemax && ddehead[ddepos].hwndOwner != hwnd; ddepos++);
		if (ddepos < ddemax) {
			ddehead[ddepos].type |= DDE_TYPE_TERMINATE;
			PostMessage((HWND) ddehead[ddepos].hwndRemote, WM_DDE_TERMINATE, (WPARAM) hwnd, 0L);
		}
		return 0;
	}
	return(DefWindowProc(hwnd, message, wParam, lParam));
}

