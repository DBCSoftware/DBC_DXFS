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

#if OS_WIN32
#include <windows.h>
#endif

#if OS_WIN32
#define DllExport   __declspec( dllexport )
#else
#define DllExport
#endif


#include "includes.h"

DllExport int nioopen(char *, int, int);
DllExport int nioprep(char *, char *, int);
DllExport int nioclose(int);
DllExport int niordseq(int, unsigned char *, int);
DllExport int niowtseq(int, unsigned char *, int);
DllExport int niordrec(int, OFFSET, unsigned char *, int);
DllExport int niowtrec(int, OFFSET, unsigned char *, int);
DllExport int niordkey(int, char *, unsigned char *, int);
DllExport int niordks(int, unsigned char *, int);
DllExport int niordkp(int, unsigned char *, int);
DllExport int niowtkey(int, unsigned char *, int);
DllExport int nioupd(int, unsigned char *, int);
DllExport int niodel(int, char *);
DllExport int niolck(int *);
DllExport int nioulck(void);
DllExport int niofpos(int, OFFSET *);
DllExport int niorpos(int, OFFSET);
DllExport int nioclru(void);
DllExport int nioerror(void);


#if OS_WIN32
BOOL WINAPI DLLEntryPoint(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			break;
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
			break;
		case DLL_PROCESS_DETACH:
			break;
	}
	return(TRUE);
}
#endif
