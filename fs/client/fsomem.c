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

#ifndef FSOMEM_DEBUG
#ifdef _DEBUG
#define FSOMEM_DEBUG 1
#else
#define FSOMEM_DEBUG 0
#endif
#endif

#if FSOMEM_DEBUG
#define INC_ASSERT
#endif

#define INC_STDIO
#define INC_STDLIB
#define INC_STRING
#include "includes.h"
#include "fsomem.h"

#if OS_WIN32
static HANDLE heaphandle = NULL;
static INT heapcount = 0;
#else
typedef void* LPVOID;
#endif

#if FSOMEM_DEBUG
typedef struct header1_struct {
	struct header1_struct *prev;
	struct header1_struct *next;
} HEADER1;
typedef struct header2_struct {
	INT size;
	INT count;
} HEADER2;
static HEADER1 firstheader, lastheader;
static INT alloccount;
#define SIZEOFHEADER1 sizeof(HEADER1)
#define SIZEOFHEADER2 sizeof(HEADER2)
#else
#define SIZEOFHEADER1 0
#define SIZEOFHEADER2 0
#endif

LPVOID allocmem(INT size, INT zeroflag)
{
	LPVOID memory;

/*** NOTE: DO NOT KNOW THE BENEFITS OF USING HEAP FUNCTIONS INSTEAD ***/
/***       OF RTL MALLOC, ETC.  SHOULD PROBABLY RUN TESTS.  ALSO, ***/
/***       WITH HEAP ROUTINES, COULD USE MORE THAN ONE HEAPCREATE ***/
/***       TO CREATE A HEAP WITH SIMILAR SIZES AS THIS WILL PREVENT ***/
/***       FRAGMENTATION ***/
#if OS_WIN32
	if (heaphandle == NULL) {
#ifdef _WIN64
		UINT initial = 0x1000000;
#else
		UINT initial = 0x0800000;
#endif
		/**
		 * 17 AUG 2011
		 * Start with 16MB for 64-bit, half that for 32 bit
		 */
		for (;;) {
			DWORD err;
			heaphandle = HeapCreate(0, initial, 0);
			if (heaphandle != NULL) break;
			err = GetLastError();
			if (err != ERROR_NOT_ENOUGH_MEMORY) return NULL;
			if (initial <= 0x2000) return NULL;
			initial -= 0x2000;		// 8K
		}
	}

	if (zeroflag) zeroflag = HEAP_ZERO_MEMORY;
	memory = HeapAlloc(heaphandle, zeroflag, size + SIZEOFHEADER1 + SIZEOFHEADER2 * 2);
	if (memory != NULL) {
#if FSOMEM_DEBUG
		if (!heapcount) {
			firstheader.next = &lastheader;
			lastheader.prev = &firstheader;
		}
		firstheader.next->prev = (HEADER1 *) memory;
		((HEADER1 *) memory)->next = firstheader.next;
		((HEADER1 *) memory)->prev = &firstheader;
		firstheader.next = (HEADER1 *) memory;
		(LPBYTE) memory += sizeof(HEADER1);
		((HEADER2 *) memory)->size = size;
		((HEADER2 *) memory)->count = ++alloccount;
		memcpy((LPBYTE) memory + size + sizeof(HEADER2), memory, sizeof(HEADER2));
		(LPBYTE) memory += sizeof(HEADER2);
#endif
		heapcount++;
	}
#else 
	memory = malloc(size);
	if (zeroflag) memset(memory, 0, size);
#endif	
	return memory;
}

LPVOID reallocmem(LPVOID memory, INT size, INT zeroflag)
{
	LPVOID newmemory;

#if FSOMEM_DEBUG
	assert(memory != NULL);
#endif
	if (memory == NULL) return NULL;
#if OS_WIN32
#if FSOMEM_DEBUG
	(LPBYTE) memory -= sizeof(HEADER2);
	assert(!IsBadWritePtr(memory, sizeof(HEADER2)));
	assert(!IsBadWritePtr(memory, ((HEADER2 *) memory)->size + sizeof(HEADER2) * 2));
	assert(!memcmp((LPBYTE) memory + ((HEADER2 *) memory)->size + sizeof(HEADER2), memory, sizeof(HEADER2)));
	(LPBYTE) memory -= sizeof(HEADER1);
	assert(!IsBadWritePtr(memory, sizeof(HEADER1)));
#endif

	if (zeroflag) zeroflag = HEAP_ZERO_MEMORY;
	newmemory = HeapReAlloc(heaphandle, zeroflag, memory, size + SIZEOFHEADER1 + SIZEOFHEADER2 * 2);
#if FSOMEM_DEBUG
	if (newmemory != NULL) {
		(LPBYTE) newmemory += sizeof(HEADER1);
		((HEADER2 *) newmemory)->size = size;
		memcpy((LPBYTE) newmemory + size + sizeof(HEADER2), newmemory, sizeof(HEADER2));
		(LPBYTE) newmemory += sizeof(HEADER2);
	}
#endif
#else 
	newmemory = realloc(memory, size);
	if (zeroflag) memset(newmemory, 0, size);	
#endif
	return newmemory;
}

void freemem(LPVOID memory)
{
	int i1;
	if (memory == NULL) return;
#if OS_WIN32
#if FSOMEM_DEBUG
	(LPBYTE) memory -= sizeof(HEADER2);
	assert(!IsBadWritePtr(memory, sizeof(HEADER2)));
	assert(!IsBadWritePtr(memory, ((HEADER2 *) memory)->size + sizeof(HEADER2) * 2));
	assert(!memcmp((LPBYTE) memory + ((HEADER2 *) memory)->size + sizeof(HEADER2), memory, sizeof(HEADER2)));
	(LPBYTE) memory -= sizeof(HEADER1);
	assert(!IsBadWritePtr(memory, sizeof(HEADER1)));
	((HEADER1 *) memory)->next->prev = ((HEADER1 *) memory)->prev;
	((HEADER1 *) memory)->prev->next = ((HEADER1 *) memory)->next;
#endif
	// If the HeapFree function succeeds, the return value is nonzero.
	i1 = HeapFree(heaphandle, 0, memory);
	if (i1 && heapcount) {
		if (!--heapcount) {
			int i1 = HeapDestroy(heaphandle);
			heaphandle = NULL;
		}
	}
#else
	free(memory);
#endif	
}
