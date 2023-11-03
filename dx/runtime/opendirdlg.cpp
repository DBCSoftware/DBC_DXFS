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

#ifdef __cplusplus
extern "C" {            /* Assume C declarations for C++ */
#endif /* __cplusplus */

#include "includes.h"
#include "base.h"
#include "gui.h"
#include "guix.h"
#include <ShlObj.h>
#include <Unknwn.h>
#include <ShObjIdl.h>

	class myFilterClass : public IFolderFilter { // @suppress("Symbol is not resolved")

	public:

		LPCSTR deviceFilters;

		CHAR path[MAX_PATH]; // @suppress("Type cannot be resolved") // @suppress("Symbol is not resolved")

		ULONG STDMETHODCALLTYPE AddRef() { return 0; }

		ULONG STDMETHODCALLTYPE Release() { return 0; }

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) {
			if (ppvObject == NULL) return E_POINTER; // @suppress("Symbol is not resolved")
			if (riid == IID_IFolderFilter) { // @suppress("Symbol is not resolved")
				*ppvObject = this;
				return S_OK; // @suppress("Symbol is not resolved")
			}
			*ppvObject = NULL;
			return E_NOINTERFACE; // @suppress("Symbol is not resolved")
		}

		HRESULT STDMETHODCALLTYPE ShouldShow(IShellFolder *psf,
		    PCIDLIST_ABSOLUTE pidlFolder,
		    PCUITEMID_CHILD pidlItem)
		{
			IShellItem *psi;
			LPWSTR ptr;
			CHAR seps[] = ",";
			HRESULT returnVal = S_OK; // @suppress("Type cannot be resolved") // @suppress("Symbol is not resolved")
			HRESULT hr = guiaSHCreateItemWithParent(NULL, psf, pidlItem, IID_PPV_ARGS(&psi)); // @suppress("Type cannot be resolved") // @suppress("Function cannot be resolved")
			if (SUCCEEDED(hr)) // @suppress("Function cannot be resolved")
			{
				HRESULT hr2 = psi->GetDisplayName(SIGDN_FILESYSPATH, &ptr); // @suppress("Type cannot be resolved") // @suppress("Method cannot be resolved")
				if (hr2 == S_OK) { // @suppress("Symbol is not resolved")
					WideCharToMultiByte(CP_ACP, NULL, ptr, -1, path, MAX_PATH, NULL, NULL); // @suppress("Function cannot be resolved") // @suppress("Symbol is not resolved")
					CoTaskMemFree(ptr); // @suppress("Function cannot be resolved")
					if (deviceFilters != NULL) {
						returnVal = S_FALSE; // @suppress("Symbol is not resolved")
						CHAR *dupFilters = _strdup(deviceFilters); // @suppress("Type cannot be resolved") // @suppress("Function cannot be resolved")
						char *token = strtok(dupFilters, seps); // @suppress("Function cannot be resolved")
						while (token != NULL) {
							int i1 = strcmp(path, token); // @suppress("Function cannot be resolved")
							if (i1 == 0) { /* An exact match is always acceptable */
								returnVal = S_OK; // @suppress("Symbol is not resolved")
								break;
							}
							else if (i1 > 0) { /* If path is longer than token, does it start with token? */
								if (!strncmp(path, token, strlen(token))) { // @suppress("Function cannot be resolved")
									returnVal = S_OK; // @suppress("Symbol is not resolved")
									break;
								}
							}
							token = strtok(NULL, seps);
						}
						free(dupFilters);
					}
				}
				psi->Release();
			}
			return returnVal;
		}

		HRESULT STDMETHODCALLTYPE GetEnumFlags(IShellFolder *psf,
		    PCIDLIST_ABSOLUTE pidlFolder,
		    HWND *phwnd,
		    DWORD *pgrfFlags)
		{
			return S_OK;
		}

	} myFilter;

	int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData) {
		IFolderFilterSite *LPIFFS;
		DIRDLGINFO *dinfo;
		switch (uMsg) {
			case BFFM_INITIALIZED:
				dinfo = (DIRDLGINFO *) lpData;
				if (dinfo->pszStartDir != NULL) SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)dinfo->pszStartDir);
				myFilter.deviceFilters = dinfo->deviceFilters;
				break;
			case BFFM_IUNKNOWN:
				if (lParam != NULL && guiaSHCreateItemWithParent != NULL) {
					if (((LPUNKNOWN)lParam)->QueryInterface(IID_IFolderFilterSite, (void**)&LPIFFS) == S_OK) {
						LPIFFS->SetFilter(&myFilter);
						LPIFFS->Release();
					}
				}
				break;
		}
		return 0;
	}

#ifdef __cplusplus
}            /* Assume C declarations for C++ */
#endif /* __cplusplus */
