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
#include "fio.h"
#include "gui.h"
#include "guix.h"
#include "dbcmd5.h"
#include "prt.h"
#include "prtx.h"

INT prtadrvpagesetup()
{
	INT i1, returnVal = 0;
	PAGESETUPDLG psd;

	/* free up some handles */
	for (i1 = 5; --i1 >= 0; ) fioclru(0);

	if (prtnamehandle == NULL) {
		i1 = getdefdevname(&prtnamehandle);
		if (i1) {
			if (i1 != -1) return i1;
			prtnamehandle = NULL;
		}
	}
	if (prtmodehandle == NULL) prtmodehandle = getdevicemode(prtnamehandle, NULL, 0, 0, 0, 0, NULL, 0, 0);
	memset(&psd, 0, sizeof(PAGESETUPDLG));
	psd.lStructSize = sizeof(PAGESETUPDLG);
	psd.hwndOwner = GetForegroundWindow();
	psd.hDevMode = prtmodehandle;
	psd.hDevNames = prtnamehandle;
	/* We do not honor the margin settings anyway, so always gray them */
	psd.Flags = PSD_DISABLEMARGINS;
	i1 = PageSetupDlg(&psd);
	if (!i1) {
		i1 = CommDlgExtendedError();
		if (i1) {
			prtaerror("PageSetupDlg", i1);
			returnVal = PRTERR_DIALOG;
		}
		else {
			prtputerror("");
			returnVal = PRTERR_CANCEL;
		}
	}
	else {
		prtmodehandle = psd.hDevMode;
		prtnamehandle = psd.hDevNames;
	}
	return returnVal;
}
