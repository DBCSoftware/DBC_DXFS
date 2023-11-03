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
#define INC_SQL
#define INC_SQLEXT
#define INC_IPV6
#include "includes.h"
#if OS_WIN32
#include <windowsx.h>

HINSTANCE s_hModule;  /* Saved module handle. */
static LONG threadcount = 0;
static DWORD synccount = 0;
static DWORD syncthread;
#else
#if defined(PTHREADS)
#include <pthread.h>
static pthread_t syncthread;
#else
static int syncthread;
#endif
static int threadcount = 0;
static int synccount = 0;
#endif
#include "base.h"
#include "fsodbc.h"

#if 0
/* DEBUG CODE */
static char *savefunc;
static DWORD saveproc;
static DWORD savethrd;
#endif

#if _MSC_VER >= 1700 && defined(_WIN64) // only for vs 2012 or later
//static void crash_me()
//{
//    char *p = 0;
//    *p = 0;
//}
//
//__declspec(noreturn) void __cdecl __report_rangecheckfailure()
//{
//        crash_me();
//}
#endif

//int getSyncThread() {
//	return syncthread;
//}

void entersync()
{
#if OS_WIN32
	DWORD thread;
	if (threadcount == 0) return;	
	thread = GetCurrentThreadId();
#else
#if defined(PTHREADS)
	pthread_t thread;
	if (threadcount == 0) return;
	thread = pthread_self();	
#else
	int thread;
	if (threadcount == 0) return;	
	thread = 1;
#endif	
#endif
	if (synccount != 0 && syncthread == thread) {
		return;
	}
	pvistart();
	syncthread = thread;
	synccount++;

	// What happens if this thread is put to sleep right here, and another thread enters this function?
	// the above if() should be false and not run, synccount will be non-zero, but is there
	// any way that syncthread and thread could be equal?
	//
}

void exitsync()
{
	if (threadcount == 0) return;
	if (synccount == 0) {
		return;
	}
	syncthread = 0;
	synccount--;
	pviend();
}

#if OS_WIN32
INT WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		s_hModule = hinstDLL;
		break;
	case DLL_PROCESS_DETACH:
		if (pvi_getpviflag()) {
			DeleteCriticalSection(pvi_getcrit());
		}
		break;
	case DLL_THREAD_ATTACH:
		InterlockedIncrement(&threadcount);
		//threadcount++;
		break;
	case DLL_THREAD_DETACH:
		if (synccount && syncthread == GetCurrentThreadId()) {
			exitsync();
		}
		InterlockedDecrement(&threadcount);
		//threadcount--;
		break;
	default:
		break;
	}
    return TRUE;
}
#else
void _init() 
{
	threadcount++;
}
void _fini()
{
	threadcount--;
}
#endif
