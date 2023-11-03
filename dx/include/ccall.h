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

void ccall(char *, unsigned char **);

#if OS_WIN32
static int (*dbcgetflags)(void);
static void (*dbcseteos)(int);
static void (*dbcsetequal)(int);
static void (*dbcsetless)(int);
static void (*dbcsetover)(int);
static unsigned char *(*dbcgetdata)(int);
static void (*dbcdeath)(int);
static void (*vidsuspend)(int);
static void (*vidresume)(void);
#endif

#if OS_UNIX
extern int dbcgetflags(void);
extern void dbcseteos(int);
extern void dbcsetequal(int);
extern void dbcsetless(int);
extern void dbcsetover(int);
extern unsigned char *dbcgetdata(int);
extern void dbcdeath(int);
#if 0
static void vidsuspend(int);
static void vidresume(void);
#endif
#endif


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

#if OS_WIN32
void ccallinit(dbcfunc)
void (*dbcfunc[])(void);
{
	dbcgetflags = (int (*)(void)) dbcfunc[0];
	dbcseteos = (void (*)(int)) dbcfunc[1];
	dbcsetequal = (void (*)(int)) dbcfunc[2];
	dbcsetless = (void (*)(int)) dbcfunc[3];
	dbcsetover = (void (*)(int)) dbcfunc[4];
	dbcgetdata = (unsigned char *(*)(int)) dbcfunc[7];
	dbcdeath = (void (*)(int)) dbcfunc[8];
	vidsuspend = (void (*)(int)) dbcfunc[9];
	vidresume = (void (*)(void)) dbcfunc[10];
}
#endif
