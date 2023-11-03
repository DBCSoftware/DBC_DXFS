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
#define INC_CTYPE
#define INC_TIME
#define INC_SIGNAL
#define INC_LIMITS
#define INC_ERRNO
#define INC_MATH
#define INC_LIMITS
#include "includes.h"
#include "base.h"
#include "fio.h"

#define MEMINIT 256
#define MEMBSIZE 32

#if OS_UNIX
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/utsname.h>
#if 0
#if defined(Linux)
#include <execinfo.h>
#endif
#endif
#endif

struct memdef {
	UCHAR *bufptr;
	UINT size;
	INT membptr;	/* -1 for memalloc, >=0 for membuffer */
	struct memdef *prev;
	struct memdef *next;
};

struct membdef {
	struct memdef *memptr;
	INT prev;
	INT next;
	void (*callback)(INT);
	INT cbparm;
};

/* variables for mem functions */
static struct memdef *memtab;
static struct memdef *firstmemtab;
static struct memdef *lastmemtab;
static struct memdef *freememtab;

static UCHAR **membtabptr;
static INT membmax;
static INT freemembtab;
static INT firstmembtab;
static INT lastmembtab;

static UCHAR *mallocptr;
static UCHAR *membuf;
static UINT memmax;
static UINT_PTR memsize;

static UCHAR allocflg = FALSE;
static UINT memhole;
static INT meminitsize;
static UINT memmaxsize;
static INT meminitnbufs;

/* variables for prp functions */
static ELEMENT *prptree, *prppos;

/* variables for pvi functions */
#if OS_WIN32
static LONG pviflag;
static CRITICAL_SECTION crit;
LPCRITICAL_SECTION pvi_getcrit() { return &crit; }
LONG pvi_getpviflag() { return pviflag; }
#endif

#if OS_UNIX
static INT pvicnt = 0;
#if defined(SA_RESTART) | defined(SA_INTERRUPT) | defined(SA_RESETHAND)
static sigset_t newmask, oldmask;
#endif
#endif

/* variable for dsp functions */
static int silentflag = FALSE;

static FILE *debugfile; // @suppress("Type cannot be resolved")
#if OS_UNIX
static INT debugelock = FALSE;
#endif

/* function prototypes */
static INT membufremove(UINT);
static struct memdef *memchkptr(UCHAR **);
static UCHAR *memoryalloc(UINT, struct memdef **, struct memdef **);

#if 0
static void logMemcompactEvent(CHAR * optionalInfo);
static USHORT circularEventLogNumberOfEntries;
static USHORT circularEventLogLastIndexUsed;
static INT circularEventLogRecycling;	// boolean
static LOGENTRY *internalLog;
INT keepCircularEventLog;    	// boolean
USHORT circularEventLogSize;	// number of slots in the log
static INT *logPcountPtr;
static UCHAR *logVcodePtr;
static UCHAR *logVXcodePtr;
static INT logPcount;
static UCHAR logVbcode;
static UCHAR logLsvbcode;
#endif /* SVR5 */

/* MEMINIT */
INT meminit(UINT maxsize, UINT size, INT nbufs)
{
	UINT_PTR minsize;
	ptrdiff_t i1;
	UCHAR *ptr;
	struct memdef *memptr;

	/* release the memory allocated */
	if (!maxsize) {
		if (allocflg) {
			free((void *) mallocptr);
			do {
				memptr = memtab;
				memtab = (struct memdef *) memtab->bufptr;
				free((void *) memptr);
			} while (memtab != NULL);
			allocflg = FALSE;
		}
		return(0);
	}

	maxsize = (maxsize + 0x0FFF) & ~0x0FFF;
	if (!size) size = maxsize;
	else {
		size = (size + 0x0FFF) & ~0x0FFF;
		if (size > maxsize) size = maxsize;
	}

	/* try to change the current size of memory allocated */
	if (allocflg) {
		memcompact();
#if OS_WIN32
		pvistart();
#endif
		minsize = memsize + 0x10;
		minsize = (minsize + 0x0FFF) & ~0x0FFF;
		if (size < (UINT)minsize) size = (UINT)minsize;
		ptr = NULL;
		while (size >= (UINT)minsize) {
			if (size == memmax) {
#if OS_WIN32
				pviend();
#endif
				return(0);
			}
			ptr = (UCHAR *) realloc(mallocptr, size);
			if (ptr != NULL) break;
			if (size < memmax) {
#if OS_WIN32
				pviend();
#endif
				return(0);
			}
			size -= 0x1000;
		}
		if (ptr == NULL) {
#if OS_WIN32
			pviend();
#endif
			return(1);
		}
		if ((i1 = ((UINT_PTR)mallocptr & 0x0F) - ((UINT_PTR)ptr & 0x0F))) {
			/* new address does not have the same 4 bit offset */
			if (!((UINT_PTR)mallocptr & 0x0F)) i1 += 0x10;
			else if (!((UINT_PTR)ptr & 0x0F)) i1 -= 0x10;
			if (i1 > 0) memmove(ptr + i1, ptr, minsize - i1);
			else memmove(ptr, ptr - i1, minsize + i1);
		}
		meminitsize = size;
		mallocptr = ptr;
		if ((UINT_PTR) ptr & 0x0F) {
			ptr += 0x10 - ((UINT_PTR) ptr & 0x0F);
			size -= 0x10;
		}
		if (ptr != membuf) {
			i1 = ptr - membuf;
			membuf = ptr;
			memptr = lastmemtab;
			while (memptr != NULL) {
				memptr->bufptr += i1;
				memptr = memptr->prev;
			}
		}
		memmax = size;
		memmaxsize = maxsize;
		if (nbufs > 0) {
			if (++nbufs & 0x01) nbufs++;
			meminitnbufs = nbufs;
		}
#if OS_WIN32
		pviend();
#endif
		return(0);
	}

	/* new memory allocation */
	if (nbufs <= 0) nbufs = MEMINIT;
	/* allocate memory pointer memory */
	if (++nbufs & 0x01) nbufs++;
	memtab = (struct memdef *) malloc(nbufs * sizeof(struct memdef));
	if (memtab == NULL) return RC_ERROR;

	/* allocate memory */
	while (size >= 1024) {
		mallocptr = (UCHAR *) malloc(size);
		if (mallocptr != NULL) break;
		size -= 1024;
	}
	if (mallocptr == NULL) {
		free((void *) memtab);
		return RC_ERROR;
	}
	meminitsize = size;
	membuf = mallocptr;
	if ((UINT_PTR) membuf & 0x0F) {
		membuf += 0x10 - ((UINT_PTR) membuf & 0x0F);
		size -= 0x10;
	}
	memmax = size;
	memmaxsize = maxsize;
	meminitnbufs = nbufs;

	firstmemtab = lastmemtab = freememtab = NULL;
	memtab->bufptr = NULL;
	memtab->size = nbufs;
	while (--nbufs > 0) {
		memtab[nbufs].bufptr = (UCHAR *) freememtab;
		freememtab = &memtab[nbufs];
	}
	membtabptr = NULL;
	membmax = 0;
	firstmembtab = lastmembtab = freemembtab = -1;
	memsize = 0;
	memhole = 0;
	allocflg = TRUE;
	return(0);
}

/*
 * MEMALLOC
 * Returns NULL if unable to allocate
 */
UCHAR **memalloc(INT size, INT flags)
{
	UINT_PTR minsize;
	INT nbufs;
	ULONG i1;
	ptrdiff_t i2;
	UCHAR *ptr;
	struct memdef *memptr, *leftptr, *rightptr;

	if (!allocflg && meminit(512 << 10, 0, 0) == -1) return(NULL);

	if (size < 0) return(NULL);
	if (!size) size = 0x10;
	size = (size + 0x0F) & ~0x0F;

	if (freememtab == NULL) {
		nbufs = meminitnbufs;
		for ( ; ; ) {
			i1 = nbufs * sizeof(struct memdef);
			memptr = (struct memdef *) malloc(i1);
			if (memptr != NULL) break;

			/* free up some memory, minimum of 4K */
			i1 = (i1 + 0x0FFF) & ~0x0FFF;
			minsize = memsize + 0x10;
			if (i1 > memmax - minsize) {
				memcompact();
				if (i1 > memmax - minsize) {
					if (membufremove(i1 - (memmax - (UINT)minsize)) != -1) memcompact();
					if (i1 > memmax - minsize) return(NULL);
				}
			}
#if OS_WIN32
			pvistart();
#endif
			ptr = (UCHAR *) realloc(mallocptr, memmax - i1);
			if (ptr == NULL) {
#if OS_WIN32
				pviend();
#endif
				return(NULL);
			}
			if ((i2 = ((UINT_PTR) mallocptr & 0x0F) - ((UINT_PTR) ptr & 0x0F))) {
				/* new address does not have the same 4 bit offset */
				if (!((UINT_PTR) mallocptr & 0x0F)) i2 += 0x10;
				else if (!((UINT_PTR) ptr & 0x0F)) i2 -= 0x10;
				if (i2 > 0) memmove(ptr + i2, ptr, minsize - i2);
				else memmove(ptr, ptr - i2, minsize + i2);
			}
			mallocptr = ptr;
			if ((UINT_PTR) ptr & 0x0F) {
				ptr += 0x10 - ((UINT_PTR) ptr & 0x0F);
				i1 += 0x10;
			}
			if (ptr != membuf) {  /* memory moved */
				i2 = ptr - membuf;
				membuf = ptr;
				memptr = lastmemtab;
				while (memptr != NULL) {
					memptr->bufptr += i2;
					memptr = memptr->prev;
				}
			}
			memmax -= i1;
#if OS_WIN32
			pviend();
#endif
		}
		memptr->bufptr = (UCHAR *) memtab;
		memptr->size = nbufs;
		memtab = memptr;
		while (--nbufs) {
			memptr[nbufs].bufptr = (UCHAR *) freememtab;
			freememtab = &memptr[nbufs];
		}
	}

	ptr = memoryalloc(size, &leftptr, &rightptr);
	if (ptr == NULL) return(NULL);
	if (flags & MEMFLAGS_ZEROFILL) memset(ptr, 0, size);

	memptr = freememtab;
	freememtab = (struct memdef *) freememtab->bufptr;
	memptr->bufptr = ptr;
	memptr->size = size;
	memptr->membptr = -1;
	memptr->prev = leftptr;
	memptr->next = rightptr;
	if (leftptr != NULL) leftptr->next = memptr;
	else firstmemtab = memptr;
	if (rightptr != NULL) rightptr->prev = memptr;
	else lastmemtab = memptr;
	return(&memptr->bufptr);
}

UINT walkmemory() {
	struct memdef *memptr;
	UINT retval = 0;
	memptr = firstmemtab;
	while (memptr != NULL) {
		retval += memptr->size;
		memptr = memptr->next;
	};
	return retval;
}

/**
 * MEMCHANGE
 *
 * flags can be MEMFLAGS_ZEROFILL or zero.
 *
 * return 0 if successful, RC_ERROR if failure.
 *
 * Might move memory
 *
 */
INT memchange(UCHAR **pptr, UINT size, INT flags)
{
	UINT_PTR i1, i2;
	UCHAR *ptr;
	struct memdef *memptr, *leftptr1, *rightptr1, *leftptr2, *rightptr2;

	memptr = memchkptr(pptr);
	if (memptr == NULL || memptr->membptr != -1) return RC_ERROR;

	leftptr1 = memptr->prev;
	rightptr1 = memptr->next;
	if (!size) size = 0x10;
	size = (size + 0x0F) & ~0x0F;

	if (size == memptr->size) return(0);
	if (size > memptr->size) {
		if (rightptr1 == NULL) i1 = memmax - memsize;
		else i1 = (rightptr1->bufptr - memptr->bufptr) - memptr->size;
		if (size > memptr->size + i1) {
			if (leftptr1 == NULL) ptr = membuf;
			else ptr = leftptr1->bufptr + leftptr1->size;
			i2 = memptr->bufptr - ptr;
			if (size <= memptr->size + i1 + i2) {
				memmove(ptr, memptr->bufptr, memptr->size);
				memptr->bufptr = ptr;
				i1 += i2;
			}
			else if (rightptr1 == NULL) {
				memcompact();
				i1 = memmax - memsize;
			}
		}
	}
	else i1 = 0;
	if (size <= memptr->size + i1) {
		if ((flags & MEMFLAGS_ZEROFILL) && size > memptr->size) {
			memset(memptr->bufptr + memptr->size, 0, size - memptr->size);
		}
		if (memptr == lastmemtab) {
			if (size >= memptr->size) memsize += size - memptr->size;
			else memsize -= memptr->size - size;
		}
		else if (size < memptr->size && memptr->size - size > memhole) memhole = memptr->size - size;
		memptr->size = size;
		return(0);
	}

	ptr = memoryalloc(size, &leftptr2, &rightptr2);
	if (ptr == NULL) return RC_ERROR;
	if ((flags & MEMFLAGS_ZEROFILL) && size > memptr->size) memset(ptr + memptr->size, 0, size - memptr->size);

	if (rightptr1 == NULL && size <= memptr->size + memmax - memsize) {
		/* memoryalloc alloc'd more memory, otherwise it would return null */
		memsize += size - memptr->size;
		memptr->size = size;
		return(0);
	}

	/* error if rightptr1 = null or new pointers = old pointers */
	if (rightptr1 == NULL || leftptr1 == leftptr2 || rightptr1 == rightptr2) {
		fputs("MEMCHANGE INTERNAL ERROR 1\n", stderr);
		exit(1);
	}
	/* unlink old memory pointer */
	rightptr1->prev = leftptr1;
	if (leftptr1 != NULL) leftptr1->next = rightptr1;
	else firstmemtab = rightptr1;
	if (memptr->size > memhole) memhole = memptr->size;

	/* transfer memory to new position and insert pointer into linked list */
	memcpy(ptr, memptr->bufptr, memptr->size);
	memptr->bufptr = ptr;
	memptr->size = size;
	memptr->prev = leftptr2;
	memptr->next = rightptr2;
	if (leftptr2 != NULL) leftptr2->next = memptr;
	else firstmemtab = memptr;
	if (rightptr2 != NULL) rightptr2->prev = memptr;
	else lastmemtab = memptr;
	return(0);
}

/* MEMFREE */
void memfree(UCHAR **pptr)
{
	struct memdef *memptr, *leftptr, *rightptr;

	memptr = memchkptr(pptr);
	if (memptr == NULL) return;
	if (memptr->membptr != -1) membufend(pptr);

#if OS_WIN32
	pvistart();
#endif
	leftptr = memptr->prev;
	rightptr = memptr->next;
	memptr->bufptr = (UCHAR *) freememtab;
	freememtab = memptr;
	if (rightptr != NULL) {
		rightptr->prev = leftptr;
		if (memptr->size > memhole) memhole = memptr->size;
	}
	else {
		lastmemtab = leftptr;
		if (leftptr == NULL) {
			memsize = 0;
			memhole = 0;
		}
		else memsize = (leftptr->bufptr - membuf) + leftptr->size;
	}
	if (leftptr != NULL) leftptr->next = rightptr;
	else firstmemtab = rightptr;
#if OS_WIN32
	pviend();
#endif
	return;
}

/*
 * MEMBUFFER
 *
 * Returns zero for success, RC_ERROR if there are memory problems
 */
INT membuffer(UCHAR **pptr, void (*callback)(INT), INT cbparm)
{
	INT i1;
	struct memdef *memptr;
	struct membdef *membtab;

	memptr = memchkptr(pptr);
	if (memptr == NULL || (void *) callback == NULL) return RC_ERROR;

	if (memptr->membptr != -1) membufend(pptr);

	if (freemembtab == -1) {  /* try to expand membtab */
		if (!membmax) {
			membtabptr = memalloc(MEMBSIZE * sizeof(struct membdef), 0);
			if (membtabptr == NULL) return RC_ERROR;
		}
		else if (memchange(membtabptr, (membmax + MEMBSIZE) * sizeof(struct membdef), 0) == RC_ERROR)
			return RC_ERROR;
		membtab = (struct membdef *) *membtabptr;

		for (i1 = membmax + MEMBSIZE; i1-- > membmax; ) {
			membtab[i1].next = freemembtab;
			freemembtab = i1;
		}
		membmax += MEMBSIZE;
	}
	else membtab = (struct membdef *) *membtabptr;

	i1 = freemembtab;
	freemembtab = membtab[freemembtab].next;
	memptr->membptr = i1;

	membtab[i1].memptr = (struct memdef *) &memptr->bufptr;
	membtab[i1].callback = callback;
	membtab[i1].cbparm = cbparm;

	/* add to the end of the buffer list */
	membtab[i1].prev = lastmembtab;
	membtab[i1].next = -1;
	if (lastmembtab != -1) membtab[lastmembtab].next = i1;
	else firstmembtab = i1;
	lastmembtab = i1;
	return(0);
}

/* MEMBUFEND */
void membufend(UCHAR **pptr)
{
	INT i1;
	struct memdef *memptr;
	struct membdef *membptr, *membtab;

	memptr = memchkptr(pptr);
	if (memptr == NULL) return;

	if (memptr->membptr == -1) return;

	/* remove it from buffer list */
	i1 = memptr->membptr;
	memptr->membptr = -1;
	membtab = (struct membdef *) *membtabptr;
	membptr = &membtab[i1];
	if (membptr->next != -1) membtab[membptr->next].prev = membptr->prev;
	else lastmembtab = membptr->prev;
	if (membptr->prev != -1) membtab[membptr->prev].next = membptr->next;
	else firstmembtab = membptr->next;
	membptr->next = freemembtab;
	freemembtab = i1;
}

/* MEMBUFREMOVE */
static INT membufremove(UINT size)
{
	struct memdef *memptr;
	struct membdef *membptr;

	while (firstmembtab != -1) {
		/* front of the buffer list has the oldest buffer */
		membptr = &((struct membdef *) *membtabptr)[firstmembtab];
		(*membptr->callback)(membptr->cbparm);
		memptr = membptr->memptr;
		if (memptr->size > size) size = 0;
		else size -= memptr->size;
		memfree((UCHAR **) memptr);
		if (!size) return(0);
	}
	return RC_ERROR;
}

/* MEMCOMPACT */
void memcompact()
{
	INT i1;
	struct memdef *memptr;

	if (!allocflg || !memhole) {
#if 0
		if (keepCircularEventLog) logMemcompactEvent("-NoOp");
#endif
		return;
	}
#if 0
	if (keepCircularEventLog) logMemcompactEvent(NULL);
#endif

#if OS_WIN32
	pvistart();
#endif
	memhole = 0;
	i1 = 0;
	memptr = firstmemtab;
	while (memptr != NULL) {
		if (memptr->bufptr != &membuf[i1]) {
			memmove(&membuf[i1], memptr->bufptr, memptr->size);
			memptr->bufptr = &membuf[i1];
		}
		i1 += memptr->size;
		memptr = memptr->next;
	}
	memsize = i1;
#if OS_WIN32
	pviend();
#endif
}

/* MEMBASE */
void membase(UCHAR **base, UINT *size, INT *asize)
{
	if (!allocflg) {
		*base = NULL;
		*size = 0;
		*asize = 0;
	}
	else {
		*base = membuf;
		*size = (UINT)memsize;
		*asize = memmax;
	}
}

/* MEMCHKPTR */
static struct memdef *memchkptr(UCHAR **pptr)
{
	struct memdef *memptr;

	if (!allocflg || pptr == NULL) return(NULL);

	memptr = (struct memdef *) pptr;
	if (memptr == NULL || memptr->bufptr == NULL) {  /* should not happen */
		fputs("*** invalid memory pointer (1) ***\n", stdout);
		exit(1);
		return(NULL);
	}
	if (memptr->bufptr < membuf) {  /* should not happen */
		fputs("*** invalid memory pointer (2) ***\n", stdout);
		exit(1);
		return(NULL);
	}
	if (memptr->bufptr + memptr->size > membuf + memsize) {  /* should not happen */
		fputs("*** invalid memory pointer (3) ***\n", stdout);
		exit(1);
		return(NULL);
	}
	return(memptr);
}

/**
 * Returns TRUE if the memalloc handle (arg 1) is valid
 */
INT isValidHandle(UCHAR **pptr)
{
	struct memdef *memptr;
	if (!allocflg || pptr == NULL) return FALSE;
	memptr = (struct memdef *) pptr;
	if (memptr == NULL || memptr->bufptr == NULL) return FALSE;
	if (memptr->bufptr < membuf) return FALSE;
	if (memptr->bufptr + memptr->size > membuf + memsize) return FALSE;
	return TRUE;
}


/* MEMORYALLOC */
static UCHAR *memoryalloc(UINT size, struct memdef **leftptr, struct memdef **rightptr)
{
	UINT_PTR minsize;
	ULONG i1;
	ptrdiff_t i2;
	UCHAR *ptr;
	struct memdef *memptr;

/*** CODE ENHANCEMENT:  MAYBE KEEP A LINKED LIST OF HOLES ***/
/*** COULD YOU GAPS IN MEMORY TO BE ELEMENTS OF LINKED LIST ***/
/*** ADDRESS IN MEMORY IS WHERE THE HOLE IS ***/
/*** 8 BYTES FOR ADDRESS OF NEXT LINKED LIST ***/
/*** 4 BYTES FOR SIZE OF HOLE ***/
#ifdef MEMFILL
	INT newmemhole;
	struct memdef *memptr1, *memptr2;
#endif

#ifdef MEMFILL
	/* try to fill holes in memory */
	if (size <= memhole) {
		newmemhole = 0;
		memptr1 = lastmemtab;
		while (memptr1 != NULL) {
			memptr2 = memptr1->prev;
			if (memptr2 == NULL) ptr = membuf;
			else ptr = memptr2->bufptr + memptr2->size;
			i1 = memptr1->bufptr - ptr;
			if (size <= i1) break;
			if (i1 > newmemhole) newmemhole = i1;
			memptr1 = memptr2;
		}
		if (memptr1 != NULL) {
			*leftptr = memptr2;
			*rightptr = memptr1;
			return(ptr);
		}
		memhole = newmemhole;
	}
#endif

	/* try to get memory from end of memory */
	if (memsize > memmax || size > memmax - memsize) {
		memcompact();
		if (memsize > memmax || size > memmax - memsize) {
			if (membufremove(size - (memmax - (UINT)memsize)) != -1) memcompact();
			if (memsize > memmax || size > memmax - memsize) {
				/* try to get more memory */
				i1 = memmax + (meminitsize >> 1);
				if (i1 < memmax + (size << 1)) i1 = memmax + (size << 1);
				i1 = (i1 + 0x0FFFL) & ~0x0FFFL;
				if (i1 > memmaxsize) i1 = memmaxsize;
#if OS_WIN32
				pvistart();
#endif
				minsize = memsize + 0x10;
				ptr = NULL;
				while (i1 >= minsize + size) {
					ptr = (UCHAR *) realloc(mallocptr, i1);
					if (ptr != NULL) break;
					i1 -= 0x1000;
				}
				if (ptr == NULL) {
#if OS_WIN32
					pviend();
#endif
					return(NULL);
				}
				if ((i2 = ((UINT_PTR)mallocptr & 0x0FU) - ((UINT_PTR)ptr & 0x0FU))) {
					/* new address does not have the same 4 bit offset */
					if (!((UINT_PTR)mallocptr & 0x0FU)) i2 += 0x10;
					else if (!((UINT_PTR)ptr & 0x0FU)) i2 -= 0x10;
					if (i2 > 0) memmove(ptr + i2, ptr, minsize - i2);
					else memmove(ptr, ptr - i2, minsize + i2);
				}
				mallocptr = ptr;
				if ((UINT_PTR)ptr & 0x0FU) {
					ptr += 0x10 - ((UINT_PTR)ptr & 0x0FU);
					i1 -= 0x10;
				}
				if (ptr != membuf) {  /* memory moved */
					i2 = ptr - membuf;
					membuf = ptr;
					memptr = lastmemtab;
					while (memptr != NULL) {
						memptr->bufptr += i2;
						memptr = memptr->prev;
					}
				}
				memmax = i1;
#if OS_WIN32
				pviend();
#endif
			}
		}
	}

	ptr = membuf + memsize;
	memsize += size;
	*leftptr = lastmemtab;
	*rightptr = NULL;
	return(ptr);
}

/**
 * Expects a null-termianted string of bytes.
 * Will accept leading whitespace.
 * Will accept a minus sign before the first digit.
 * Returns TRUE if the string is syntactically an integer
 */
int mscis_integer(CHAR* src, INT srclength)
{
	int state = 0;
	int i1;
	for (i1 = 0; i1 < srclength; i1++) {
		char c1 = src[i1];
		switch (state) {
			case 0:			// Scanning whitespace
				if (isspace(c1)) continue;
				if (c1 == '-' || isdigit(c1)) {
					state = 1;
					continue;
				}
				return FALSE;
			case 1:
				if (isdigit(c1)) continue;
				return FALSE;
		}
	}
	return TRUE;
}

void mscatooff(CHAR *src, OFFSET *dest)
{
	INT i1, negflg;
	OFFSET num;

	negflg = FALSE;
	for (i1 = -1, num = 0; src[++i1]; )
		if (isdigit((int)src[i1])) num = num * 10 + src[i1] - '0';
		else if (src[i1] == '-') negflg = TRUE;
	if (negflg) *dest = -num;
	else *dest = num;
}

void mscntoi(UCHAR *src, INT *dest, INT srcsize)
{
	INT i1, negflg, num;

	negflg = FALSE;
	for (i1 = -1, num = 0; ++i1 < srcsize; )
		if (isdigit((int)src[i1])) num = num * 10 + src[i1] - '0';
		else if (src[i1] == '-') negflg = TRUE;
	if (negflg) *dest = -num;
	else *dest = num;
}

void mscntooff(UCHAR *src, OFFSET *dest, INT n)
{
	INT i1, negflg;
	OFFSET num;

	negflg = FALSE;
	for (i1 = -1, num = 0L; ++i1 < n; )
		if (isdigit((int)src[i1])) num = num * 10 + src[i1] - '0';
		else if (src[i1] == '-') negflg = TRUE;
	if (negflg) *dest = -num;
	else *dest = num;
}

void msc9tooff(UCHAR *src, OFFSET *dest)
{
	INT i1;
	OFFSET num;

	num = 0;
	i1 = 0;
	do if (isdigit((int)src[i1])) num = num * 10 + src[i1] - '0';
	while (++i1 < 9);

	*dest = num;
}

void msc6xtooff(UCHAR *src, OFFSET *dest)
{
	INT i1;
	OFFSET num;

	num = 0L;
	i1 = 0;
	do num = (num << 8) | src[i1];
	while (++i1 < 6);

	*dest = num;
}

/**
 * Using strcpy, convert the src to a decimal ascii string and
 * copy it to dest.
 * There will be a NULL right after the digits string
 */
int mscitoa(INT src, CHAR *dest)
{
	INT i1, negflg;
	CHAR work[16];

	if (src < 0) {
		src = -src;
		negflg = TRUE;
	}
	else negflg = FALSE;

	work[sizeof(work) - 1] = '\0';
	i1 = sizeof(work) - 1;
	do work[--i1] = (CHAR)(src % 10 + '0');
	while (src /= 10);
	if (negflg) work[--i1] = '-';
	strcpy(dest, &work[i1]);
	return sizeof(work) - 1 - i1;
}

int mscofftoa(OFFSET src, CHAR *dest)
{
	INT i1, negflg;
	CHAR work[32];

	if (src < 0) {
		src = -src;
		negflg = TRUE;
	}
	else negflg = FALSE;

	work[sizeof(work) - 1] = '\0';
	i1 = sizeof(work) - 1;
	do work[--i1] = (CHAR)(src % 10 + '0');
	while (src /= 10);
	if (negflg) work[--i1] = '-';
	strcpy(dest, &work[i1]);
	return sizeof(work) - 1 - i1;
}

/**
 * Right justify the integer input into the output field n characters wide.
 */
void msciton(INT src, UCHAR *dest, INT n)
{
	INT negflg, i1, i2;
	UCHAR *bigsrc;

	if (n <= 0) return;
	if (src < 0) {
		if (src == (INT) 0x80000000) {
			bigsrc = (UCHAR *) "-2147483648";
			for (i1 = 10, i2 = n - 1; i1 >= 0 && i2 >= 0; i1--, i2--) {
				dest[i2] = bigsrc[i1];
			}
			return;
		}
		src = -src;
		negflg = TRUE;
	}
	else negflg = FALSE;

	do dest[--n] = (CHAR)(src % 10 + '0');
	while ((src /= 10) && n);
	if (negflg && n) dest[--n] = '-';
	while (n) dest[--n] = ' ';
}

void mscoffton(OFFSET src, UCHAR *dest, INT n)
{
	INT negflg;

	if (n <= 0) return;
	if (src < 0L) {
		src = -src;
		negflg = TRUE;
	}
	else negflg = FALSE;

	do dest[--n] = (CHAR)(src % 10 + '0');
	while ((src /= 10) && n);
	if (negflg && n) dest[--n] = '-';
	while (n) dest[--n] = ' ';
}

void mscoffto9(OFFSET src, UCHAR *dest)
{
	INT i1;

	if (src < 0) return;
	i1 = 9;
	do dest[--i1] = (CHAR)(src % 10 + '0');
	while ((src /= 10) && i1);
	while (i1) dest[--i1] = ' ';
}

void mscoffto6x(OFFSET src, UCHAR *dest)
{
	INT i1;

	if (src < 0) return;
	i1 = 6;
	do {
		dest[--i1] = (UCHAR) src;
		src >>= 8;
	} while (i1);
}

void msctimestamp(UCHAR *dest)
{
#if OS_WIN32
	INT i1;
	SYSTEMTIME systime;

	GetLocalTime(&systime);
	for (i1 = 4; --i1 >= 0; systime.wYear /= 10) dest[i1] = (UCHAR)(systime.wYear % 10 + '0');
	dest[4] = (UCHAR)(systime.wMonth / 10 + '0');
	dest[5] = (UCHAR)(systime.wMonth % 10 + '0');
	dest[6] = (UCHAR)(systime.wDay / 10 + '0');
	dest[7] = (UCHAR)(systime.wDay % 10 + '0');
	dest[8] = (UCHAR)(systime.wHour / 10 + '0');
	dest[9] = (UCHAR)(systime.wHour % 10 + '0');
	dest[10] = (UCHAR)(systime.wMinute / 10 + '0');
	dest[11] = (UCHAR)(systime.wMinute % 10 + '0');
	dest[12] = (UCHAR)(systime.wSecond / 10 + '0');
	dest[13] = (UCHAR)(systime.wSecond % 10 + '0');
	dest[14] = (UCHAR)(systime.wMilliseconds / 100 + '0');
	dest[15] = (UCHAR)((systime.wMilliseconds / 10) % 10 + '0');
#endif

#if OS_UNIX
	INT i1, i2;
	struct timeval tv;
	struct timezone tz;
	struct tm *today;
	time_t timer;

	/*time(&timer);*/
	gettimeofday(&tv, &tz);
	timer = tv.tv_sec;
	today = localtime(&timer);
	for (i1 = 4, i2 = today->tm_year + 1900; --i1 >= 0; i2 /= 10) dest[i1] = (UCHAR)(i2 % 10 + '0');
	dest[4] = (UCHAR)((today->tm_mon + 1) / 10 + '0');
	dest[5] = (UCHAR)((today->tm_mon + 1) % 10 + '0');
	dest[6] = (UCHAR)(today->tm_mday / 10 + '0');
	dest[7] = (UCHAR)(today->tm_mday % 10 + '0');
	dest[8] = (UCHAR)(today->tm_hour / 10 + '0');
	dest[9] = (UCHAR)(today->tm_hour % 10 + '0');
	dest[10] = (UCHAR)(today->tm_min / 10 + '0');
	dest[11] = (UCHAR)(today->tm_min % 10 + '0');
	dest[12] = (UCHAR)(today->tm_sec / 10 + '0');
	dest[13] = (UCHAR)(today->tm_sec % 10 + '0');
	i1 = tv.tv_usec / 10000;
	dest[14] = (UCHAR)(i1 / 10 + '0');
	dest[15] = (UCHAR)(i1 % 10 + '0');
	/*dest[14] = dest[15] = '0';*/
#endif
}

/**
 * Returns zero for success.
 * Returns RC_ERROR if anthing goes wrong
 */
int prpinit(ELEMENT *root, CHAR *xmlprefix)
{
	ELEMENT *e1;
	if (root == NULL) return RC_ERROR;
	e1 = root;
	while (e1 != NULL) {
		if (!strcmp(e1->tag, xmlprefix)) break;
		e1 = e1->nextelement;
	}
	if (e1 == NULL) return RC_ERROR;
	prptree = e1;
	return 0;
}

int prpname(char **value) {
	if (prppos == NULL) return 1;
	*value = prppos->tag;
	return 0;
}

ELEMENT *prpgetprppos() {
	return prppos;
}

void prpsetprppos(ELEMENT *position) {
	prppos = position;
}

/**
 * Return 0 if found, 1 if not found
 * Does not move memory
 */
INT prpgetvol(CHAR *volname, CHAR *ptr, INT maxlen) {
	ELEMENT *e1, *e2, *e3;
	INT i1, i2;
	if (prptree == NULL) return 1;
	e1 = prptree->firstsubelement;
	while (e1 != NULL) {
		if (!strcmp(e1->tag, "file")) {
			e2 = e1->firstsubelement;
			while (e2 != NULL) {
				if (!strcmp(e2->tag, "volume")) {
					e3 = e2->firstsubelement;
					if (e3 != NULL && !strcmp(e3->tag, "name")) {
						if (e3->firstsubelement != NULL && e3->firstsubelement->cdataflag && !strcmp(e3->firstsubelement->tag, volname))
						{
							/* match */
						 	for (i1 = 0, i2 = maxlen - 1, e3 = e3->nextelement; e3 != NULL; e3 = e3->nextelement, i1++) {
								if (!(e3->firstsubelement != NULL && e3->firstsubelement->cdataflag)) break;
								i2 -= (INT)strlen(e3->firstsubelement->tag);
								if (i1) {
									if (--i2 > 0) strcat(ptr, ";");
								}
								if (i2 > 0) strcat(ptr, e3->firstsubelement->tag);
							}
							if (ptr[0]) return 0;
							return 1;
						}
					}
				}
				e2 = e2->nextelement;
			}
		}
		e1 = e1->nextelement;
	}
	return 1;
}

/**
 * Return 0 if found, 1 if not
 */
int prpget(char *key1, char *key2, char *key3, char *key4, char **value, int flags) {
	int reduced = FALSE;
	char *ptr;
	ATTRIBUTE *a1;
	ELEMENT *e1, *e2, *e3, *e4;
	if (prptree == NULL) return 1;
	a1 = prptree->firstattribute;
	while (a1 != NULL) {
		if (!strcmp(a1->tag, "reduced")) {
			reduced = (a1->value[0] == 'y') ? TRUE : FALSE;
			break;
		}
		a1 = a1->nextattribute;
	}
	if (flags & PRP_NEXT) {
		if (prppos == NULL) return 1;
		prppos = prppos->nextelement;
		if (prppos == NULL || prppos->firstsubelement == NULL) {
			if (reduced) {
				return 1;
			}
			else {
/** CODE: Need ability to search for additional values in tree that is not reduced **/
				return 1;
			}
		}
		*value = prppos->firstsubelement->tag;
		if (flags & (PRP_LOWER | PRP_UPPER)) {
			for (ptr = *value; *ptr; ptr++) {
				if (flags & PRP_LOWER) *ptr = (char) tolower(*ptr);
				else *ptr = (char) toupper(*ptr);
			}
		}
		return 0;
	}
	else {
/** TODO: Use of recursion here would make code easier to maintain **/
		prppos = NULL;
		e1 = prptree->firstsubelement;
		while (e1 != NULL) {
			if (!strcmp(e1->tag, key1)) {
				if (key2) {
					e2 = e1->firstsubelement;
					while (e2 != NULL) {
						if (!strcmp(e2->tag, key2)) {
							if (key3) {
								e3 = e2->firstsubelement;
								while (e3 != NULL) {
									if (!strcmp(e3->tag, key3)) {
										if (key4) {
											e4 = e3->firstsubelement;
											while (e4 != NULL) {
												if (!strcmp(e4->tag, key4)) {
													if (flags & PRP_GETCHILD) {
														if (e4->firstsubelement == NULL) return 1;
														prppos = e4->firstsubelement;
													}
													else {
														prppos = e4;
													}
													if (prppos->firstsubelement == NULL) return 1;
													/* match complete */
													*value = prppos->firstsubelement->tag;
													if (flags & (PRP_LOWER | PRP_UPPER)) {
														for (ptr = *value; *ptr; ptr++) {
															if (flags & PRP_LOWER) *ptr = (char) tolower(*ptr);
															else *ptr = (char) toupper(*ptr);
														}
													}
													return 0;
												}
												e4 = e4->nextelement;
											}
											if (reduced) return 1;
										}
										else {
											if (flags & PRP_GETCHILD) {
												if (e3->firstsubelement == NULL) return 1;
												prppos = e3->firstsubelement;
											}
											else {
												prppos = e3;
											}
											if (prppos->firstsubelement == NULL) return 1;
											/* match complete */
											*value = prppos->firstsubelement->tag;
											if (flags & (PRP_LOWER | PRP_UPPER)) {
												for (ptr = *value; *ptr; ptr++) {
													if (flags & PRP_LOWER) *ptr = (char) tolower(*ptr);
													else *ptr = (char) toupper(*ptr);
												}
											}
											return 0;
										}
									}
									e3 = e3->nextelement;
								}
								if (reduced) return 1;
							}
							else {
								if (flags & PRP_GETCHILD) {
									if (e2->firstsubelement == NULL) return 1;
									prppos = e2->firstsubelement;
								}
								else {
									prppos = e2;
								}
								if (prppos->firstsubelement == NULL) return 1;
								/* match complete */
								*value = prppos->firstsubelement->tag;
								if (flags & (PRP_LOWER | PRP_UPPER)) {
									for (ptr = *value; *ptr; ptr++) {
										if (flags & PRP_LOWER) *ptr = (char) tolower(*ptr);
										else *ptr = (char) toupper(*ptr);
									}
								}
								return 0;
							}
						}
						e2 = e2->nextelement;
					}
					if (reduced) return 1;
				}
				else {
					if (flags & PRP_GETCHILD) {
						if (e1->firstsubelement == NULL) return 1;
						prppos = e1->firstsubelement;
					}
					else {
						prppos = e1;
					}
					if (prppos->firstsubelement == NULL) return 1;
					/* match complete */
					*value = prppos->firstsubelement->tag;
					if (flags & (PRP_LOWER | PRP_UPPER)) {
						for (ptr = *value; *ptr; ptr++) {
							if (flags & PRP_LOWER) *ptr = (char) tolower(*ptr);
							else *ptr = (char) toupper(*ptr);
						}
					}
					return 0;
				}
			}
			e1 = e1->nextelement;
		}
	}

	return 1; /* not found */
}

int prptranslate(char *ptr, unsigned char *map)
{
	int i1, i2, i3;

	for ( ; ; ) {
		while (isspace((int)*ptr)) ptr++;
		if (*ptr == ';') {
			ptr++;
			continue;
		}
		if (!*ptr) return 0;
		if (!isdigit((int)*ptr)) return RC_ERROR;
		for (i1 = 0; isdigit((int)*ptr); ptr++) i1 = i1 * 10 + *ptr - '0';
		while (isspace((int)*ptr)) ptr++;
		if (*ptr == '-') {
			ptr++;
			while (isspace((int)*ptr)) ptr++;
			if (!isdigit((int)*ptr)) return RC_ERROR;
			for (i2 = 0; isdigit((int)*ptr); ptr++) i2 = i2 * 10 + *ptr - '0';
			while (isspace((int)*ptr)) ptr++;
		}
		else i2 = i1;
		if (*ptr != ':') return RC_ERROR;
		ptr++;
		while (isspace((int)*ptr)) ptr++;
		if (!isdigit((int)*ptr)) return RC_ERROR;
		for (i3 = 0; isdigit((int)*ptr); ptr++) i3 = i3 * 10 + *ptr - '0';
		if (i1 > UCHAR_MAX || i2 > UCHAR_MAX || i3 + (i2 - i1) > UCHAR_MAX) return RC_ERROR;
		while (i1 <= i2) map[i1++] = (unsigned char) i3++;
	}
	return 0;
}

void pvistart()
{
#if OS_WIN32
	if (!InterlockedCompareExchange(&pviflag, (LONG)TRUE, (LONG)FALSE)) {
		InitializeCriticalSection(&crit);
	}
	EnterCriticalSection(&crit);
#endif

#if OS_UNIX
	if (pvicnt++) return;
#if defined(SA_RESTART) | defined(SA_INTERRUPT) | defined(SA_RESETHAND)
	sigemptyset(&newmask);
	sigaddset(&newmask, SIGINT);
	sigaddset(&newmask, SIGQUIT);
	sigaddset(&newmask, SIGALRM);
	sigaddset(&newmask, SIGUSR1);
	sigaddset(&newmask, SIGUSR2);
#if defined(SIGPOLL) && defined(I_SETSIG)
	sigaddset(&newmask, SIGPOLL);
#else
#if defined(SIGIO) && defined(O_ASYNC)
	sigaddset(&newmask, SIGIO);
#endif
#endif
#ifdef SIGTSTP
	sigaddset(&newmask, SIGTSTP);
#endif
	sigprocmask(SIG_BLOCK, &newmask, &oldmask);
#else
	sighold(SIGINT);
	sighold(SIGQUIT);
	sighold(SIGALRM);
	sighold(SIGUSR1);
	sighold(SIGUSR2);
#if defined(SIGPOLL) && defined(I_SETSIG)
	sighold(SIGPOLL);
#else
#if defined(SIGIO) && defined(O_ASYNC)
	sighold(SIGIO);
#endif
#endif
#ifdef SIGTSTP
	sighold(SIGTSTP);
#endif
#endif
#endif
}

void pviend()
{
#if OS_WIN32
	if (pviflag) LeaveCriticalSection(&crit);
#endif

#if OS_UNIX
	if (!pvicnt) return;
	if (--pvicnt) return;
#if defined(SA_RESTART) | defined(SA_INTERRUPT) | defined(SA_RESETHAND)
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
#else
	sigrelse(SIGINT);
	sigrelse(SIGQUIT);
	sigrelse(SIGALRM);
	sigrelse(SIGUSR1);
	sigrelse(SIGUSR2);
#if defined(SIGPOLL) && defined(I_SETSIG)
	sigrelse(SIGPOLL);
#else
#if defined(SIGIO) && defined(O_ASYNC)
	sigrelse(SIGIO);
#endif
#endif
#ifdef SIGTSTP
	sigrelse(SIGTSTP);
#endif
#endif
#endif
}

void dspsilent()
{
	silentflag = TRUE;
}

void dspstring(char *str)
{
	static int firstflag = TRUE;
	static int dspmapflag = FALSE;
	static unsigned char dspmap[UCHAR_MAX + 1];
	int i1;
	char *ptr, work[256];

	if (!silentflag) {
		if (str == NULL) {
			fflush(stdout);
			return;
		}
		if (firstflag) {
			firstflag = FALSE;
			if (!prpget("display", "translate", NULL, NULL, &ptr, 0)) {
				for (i1 = 0; i1 <= UCHAR_MAX; i1++) dspmap[i1] = (unsigned char) i1;
				if (prptranslate(ptr, dspmap)) fputs("Invalid translate-spec for dbcdx.display.translate, translate not used\n", stdout);
				else dspmapflag = TRUE;
			}
		}
		if (dspmapflag) {
			while (*str) {
				for (i1 = 0; i1 < (INT) (sizeof(work) - 1) && str[i1]; i1++) work[i1] = dspmap[(unsigned char)str[i1]];
				work[i1] = '\0';
				fputs(work, stdout);
				str += i1;
			}
		}
		else fputs(str, stdout);
	}
}

void dspchar(char chr)
{
	char work[2];

	work[0] = chr;
	work[1] = '\0';
	dspstring(work);
}

void dspflush()
{
	if (!silentflag) fflush(stdout);
}

#if 0
/* memmove is necessary only for those runtime libraries that don't already include it */
char *memmove(s1, s2, n)
char *s1, *s2;
int n;
{
	char *s;

	s = s1;
	if ((unsigned int) s2 > (unsigned int) s1) while (n--) *s1++ = *s2++;
	else if ((unsigned int) s2 < (unsigned int) s1) while (n--) s1[n] = s2[n];
	return(s);
}
#endif

#ifndef basename
static CHAR basename[64] = "debug";
#endif

void SetDebugLogFileName(CHAR* name)
{
	strcpy(basename, name);
}

static CHAR* debugfname(void) {
	static CHAR work[64];
	memset(work, 0, sizeof(work));
	memcpy(work, basename, strlen(basename));
#if OS_UNIX
	mscitoa(getpid(), work + strlen(basename) - 1);
#endif
	strcat(work, ".txt");
	return work;
}

void debugflock(INT lockflag) {
#if OS_UNIX
	struct flock lock;
	if (debugfile == NULL) {
		debugfile = fopen(debugfname(), "w");
		if (debugfile == NULL) return;
	}
	memset(&lock, 0, sizeof(lock));
	if (lockflag) {
		lock.l_type = F_WRLCK;
		fcntl(fileno(debugfile), F_SETLKW, &lock);
		debugelock = TRUE;
	}
	else {
		lock.l_type = F_UNLCK;
		fcntl(fileno(debugfile), F_SETLK, &lock);
		debugelock = FALSE;
	}
#endif
}

void debugmessage(CHAR *message, INT flags)
{
	CHAR work[32];
#if OS_WIN32
	SYSTEMTIME systime;
#else
	struct timeval systime;
	int wSecond;
	int wMilliseconds;
	struct flock lock;
#endif

	memset(work, '\0', sizeof(work));
	pvistart();
	if (debugfile == NULL) {
		debugfile = fopen(debugfname(), "w");
		if (debugfile == NULL) {
			pviend();
			return;
		}
	}
#if OS_UNIX
	if (debugelock == FALSE) {
		memset(&lock, 0, sizeof(lock));
		lock.l_type = F_WRLCK;
		fcntl(fileno(debugfile), F_SETLKW, &lock);
	}
#endif
	if (!(flags & DEBUG_NOTIME)) {
#if OS_WIN32
		GetLocalTime(&systime);
		work[0] = systime.wSecond / 10 + '0';
		work[1] = systime.wSecond % 10 + '0';
		work[2] = '.';
		work[3] = systime.wMilliseconds / 100 + '0';
		work[6] = ':';
		work[7] = ' ';
		work[8] = '\0';
		work[4] = (systime.wMilliseconds % 100) / 10 + '0';
		work[5] = systime.wMilliseconds % 10 + '0';
#else
		gettimeofday(&systime, NULL);
		wSecond = systime.tv_sec % 60;
		work[0] = wSecond / 10 + '0';
		work[1] = wSecond % 10 + '0';
		work[2] = '.';
		wMilliseconds = systime.tv_usec / 1000;
		work[3] = wMilliseconds / 100 + '0';
		work[4] = (wMilliseconds % 100) / 10 + '0';
		work[5] = wMilliseconds % 10 + '0';
		work[6] = ':';
		work[7] = ' ';
#endif
		fputs(work, debugfile);
	}
	if (message != NULL) {
		if (message[0] == '\0') fputs("(zero len str)", debugfile);
		else fputs(message, debugfile);
	}
	else fputs("NULL", debugfile);
	if (!(flags & DEBUG_NONEWLINE)) fputc('\n', debugfile);
	fflush(debugfile);
	pviend();
#if OS_UNIX
	if (debugelock == FALSE) {
		memset(&lock, 0, sizeof(lock));
		lock.l_type = F_UNLCK;
		fcntl(fileno(debugfile), F_SETLK, &lock);
	}
#endif
}

#if OS_WIN32
void debugmessageW(wchar_t *message, INT flags)
{
	WCHAR work[32];
	SYSTEMTIME systime;

	memset(work, '\0', sizeof(work));
	pvistart();
	if (debugfile == NULL) {
		debugfile = fopen(debugfname(), "w");
		if (debugfile == NULL) {
			pviend();
			return;
		}
	}
	if (!(flags & DEBUG_NOTIME)) {
		GetLocalTime(&systime);
		work[0] = systime.wSecond / 10 + L'0';
		work[1] = systime.wSecond % 10 + L'0';
		work[2] = L'.';
		work[3] = systime.wMilliseconds / 100 + L'0';
		work[6] = L':';
		work[7] = L' ';
		work[8] = L'\0';
		work[4] = (systime.wMilliseconds % 100) / 10 + L'0';
		work[5] = systime.wMilliseconds % 10 + L'0';
		fputws(work, debugfile);
	}
	if (message != NULL) {
		if (message[0] == L'\0') fputws(L"(zero len str)", debugfile);
		else fputws(message, debugfile);
	}
	else fputws(L"NULL", debugfile);
	if (!(flags & DEBUG_NONEWLINE)) fputwc(L'\n', debugfile);
	fflush(debugfile);
	pviend();
}
#endif

/*
 * convert integer to 5 characters, blank filled
 */
void itonum5(INT n1, UCHAR *p1)
{
	INT i1, i2;

	i2 = n1;
	if (i2 < 0) i2 = (-i2);
	for (i1 = 4; i1 >= 0; ) {
		p1[i1--] = i2 % 10 + '0';
		if (!(i2 /= 10)) break;
	}
	if (n1 < 0 && i1 >= 0) p1[i1--] = '-';
	while (i1 >= 0) p1[i1--] = ' ';
}

/**
 * Expand printable characters.
 * 3 Characters generated for every 4 input characters
 *
 */
INT fromprintable(UCHAR *idata, INT ilen, UCHAR *odata, INT *olen)
{
	INT i1, i2;
	UCHAR c1, c2, c3, c4;
	UINT mod = (ilen % 4);

	/* parse forwards so idata and odata can be the same buffer */
	for (i1 = i2 = 0; i1 + 3 < ilen; ) {
		c1 = (UCHAR)(idata[i1++] - '?');
		c2 = (UCHAR)(idata[i1++] - '?');
		c3 = (UCHAR)(idata[i1++] - '?');
		c4 = (UCHAR)(idata[i1++] - '?');
		odata[i2++] = (UCHAR)((c1 & 0x3F) + (c2 << 6));
		odata[i2++] = (UCHAR)(((c2 & 0x3C) >> 2) + (c3 << 4));
		odata[i2++] = (UCHAR)(((c3 & 0x30) >> 4) + (c4 << 2));
	}
	if (i1 < ilen && i1 + 1 != ilen) {
		/* either 2 or 3 additional characters of input */
		/* means one or two additional chars output */
		c1 = (UCHAR)(idata[i1++] - '?');
		c2 = (UCHAR)(idata[i1++] - '?');
		if (i1 < ilen) c3 = (UCHAR)(idata[i1] - '?');
		else c3 = 0;
		odata[i2++] = (UCHAR)((c1 & 0x3F) + ((c2 & 0x03) << 6));
		if (mod == 3) odata[i2++] = (UCHAR)(((c2 & 0x3C) >> 2) + (c3 << 4));
		//if (i1 < ilen) odata[i2++] = (UCHAR)((c3 & 0x30) >> 4);
	}
	if (olen != NULL) *olen = i2;
	return 0;
}

/**
 * Convert ASCII 0x00-0xFF data to printable characters (0x3F-7E)
 * 4 Characters generated for every 3
 *
 * If (ilen % 3) == 1 then two additional characters are output,
 * If (ilen % 3) == 2 then three additional characters are output.
 */
INT makeprintable(UCHAR *idata, INT ilen, _Out_ UCHAR *odata, _Out_ INT *olen)
{
	INT i2;
	UCHAR c1, c2, c3;

	if (ilen < 0) return -1;

	/* parse backwards so idata and odata can be the same buffer */
	i2 = (ilen / 3) << 2;
	if (ilen % 3) i2 += ilen % 3 + 1;
	if (olen != NULL) *olen = i2;
	if (i2 & 0x03) {  /* additional 2 or 3 output characters */
		if (i2 & 0x01) {  /* 3 output characters */
			c2 = idata[--ilen];
			odata[--i2] = (UCHAR)((c2 >> 4) + '?');
		}
		else c2 = 0;
		c1 = idata[--ilen];
		odata[--i2] = (UCHAR)((c1 >> 6) + ((c2 & 0x0F) << 2) + '?');
		odata[--i2] = (UCHAR)((c1 & 0x3F) + '?');
	}
	while (ilen) {
		c3 = idata[--ilen];
		c2 = idata[--ilen];
		c1 = idata[--ilen];
		odata[--i2] = (UCHAR)((c3 >> 2) + '?');
		odata[--i2] = (UCHAR)((c2 >> 4) + ((c3 & 0x03) << 4) + '?');
		odata[--i2] = (UCHAR)((c1 >> 6) + ((c2 & 0x0F) << 2) + '?');
		odata[--i2] = (UCHAR)((c1 & 0x3F) + '?');
	}
	return 0;
}


#ifdef NEED_FCVT
CHAR *fcvt(double value, INT ndec, INT *decptr, INT *signptr)
{
	static CHAR str[50];
	INT lft;

	if (*signptr = (value < 0)) value *= -1.00; /* remove sign */
	if (value >= 1) while ((lft = (int) (log10(value) + 1)) > 31) value /= 10;
	else lft = 0;
	if (lft + ndec > 31) ndec = 31 - lft;
	sprintf(str, "%-.*f", ndec, value);
	if (!lft) memmove(str, &str[2], 31 - lft);
	else if (lft < 31) memmove(&str[lft], &str[lft + 1], 31 - lft);
	*decptr = lft;
	return(str);
}
#endif


#if 0

void logSetPcountPtr(INT *pcount) { logPcountPtr = pcount; }
void logSetVcodePtr(UCHAR *vbcode) { logVcodePtr = vbcode; }
void logSetVXcodePtr(UCHAR *vx) { logVXcodePtr = vx; }
static void logAddVandPInfo(CHAR *work);

void logGrabIEXData(INT pcount, UCHAR vbcode, UCHAR lsvbcode)
{
	logPcount = pcount;
	logVbcode = vbcode;
	logLsvbcode = lsvbcode;
}

/**
 * Format log time stamp as
 * yyyymmdd hh:mm:ss:hh   (Local time)
 */
static void logTimeStampForDump(CHAR *buffer,
#if OS_UNIX
		struct timeval logentrytime)
#elif OS_WIN32
		SYSTEMTIME logentrytime)
#endif
{
	char time_string[40];
#if OS_UNIX
	char huns[4];
	int hundredths;
	struct tm* plocaltm = localtime(&logentrytime.tv_sec);
	strftime(time_string, sizeof(time_string), "%Y%m%d %H:%M:%S:", plocaltm);
	hundredths = logentrytime.tv_usec / 10000;
	sprintf(huns, "%.2d ", hundredths);
	strcat(time_string, huns);
#elif OS_WIN32
#endif
	strcat(buffer, time_string);
}

static void logSetTime(LOGENTRY *le) {
#if OS_UNIX
	gettimeofday(&(le->logentrytime), NULL);
#elif OS_WIN32
#endif
}

static void logTerminateLogOutputBuffer(CHAR* buffer) {
#if OS_UNIX
	strcat(buffer, "\x0a");	// LF
#elif OS_WIN32
	strcat(buffer, "\x0d\x0a"); // CR LF
#endif
}

static void logWriteEntry(LOGENTRY *le) {
	if (internalLog == NULL) {
		internalLog = malloc(circularEventLogSize * sizeof(LOGENTRY));
		circularEventLogLastIndexUsed = (USHORT)(SHORT)-1;
		circularEventLogNumberOfEntries = 0;
	}
	if (++circularEventLogLastIndexUsed == circularEventLogSize) {
		circularEventLogLastIndexUsed = 0;
		circularEventLogRecycling = 1;
	}
	memcpy(&(internalLog[circularEventLogLastIndexUsed]), le, sizeof(LOGENTRY));
	if (circularEventLogNumberOfEntries < circularEventLogSize) circularEventLogNumberOfEntries++;
}

static void logBuildEntry(enum LOGEVENTTYPE type, void* ldata, int doStackTrace) {
	LOGENTRY le;
	memset(&le, 0, sizeof(LOGENTRY));
	logSetTime(&le);
	le.eventtype = type;
	le.vbcode = *logVcodePtr;
	le.vx = *logVXcodePtr;
	le.pcount = logPcount;
	switch (type) {
		case LET_loadmod:
			memcpy(&le.DATA.lmedata, (LOADMODEVENTDATA*)ldata, sizeof(LOADMODEVENTDATA));
			break;
		case LET_chain:
			memcpy(&le.DATA.cedata, (CHAINEVENTDATA*)ldata, sizeof(CHAINEVENTDATA));
			break;
		default:
			strncpy(le.DATA.string, (CHAR*)ldata, LOGDATAMAXSIZE);
			break;
	}
#if OS_UNIX && defined(Linux) && !defined(_AIX)
	if (doStackTrace) {
		le.backtrace = malloc(100 * sizeof(void*));
	    le.sizeOfBacktrace = backtrace(le.backtrace, 100);
	    if (le.sizeOfBacktrace < 100)
	    	le.backtrace = realloc(le.backtrace, le.sizeOfBacktrace * sizeof(void*));
	}
#endif
	logWriteEntry(&le);
}

void logOverflowEvent(CHAR *info) {
	static char verbiage[] = "overflow";
	CHAR work[LOGDATAMAXSIZE];
	strcpy(work, verbiage);
	if (info != NULL) strcat(work, info);
	logAddVandPInfo(work);
	logBuildEntry(LET_overflow, work, FALSE);
}

static void logMemcompactEvent(CHAR *optionalInfo) {
	static char verbiage[] = "memcompact";
	CHAR work[LOGDATAMAXSIZE];
	strcpy(work, verbiage);
	if (optionalInfo != NULL) strcat(work, optionalInfo);
	//logAddVandPInfo(work);
	logBuildEntry(LET_memcompact, work, FALSE);
}

void logChainEvent(CHAINEVENTDATA *data) {
	logBuildEntry(LET_chain, data, FALSE);
}

static void logAddPInfo(CHAR *work) {
	CHAR work2[32];
	if (logPcountPtr != NULL) {
		sprintf(work2, ", pcount=%d", *logPcountPtr);
		strcat(work, work2);
	}
}

void logLoadmodEvent(LOADMODEVENTDATA *data) {
	logBuildEntry(LET_loadmod, data, FALSE);
}

void logUnloadEvent(CHAR *moduleName) {
	CHAR work[256];
	strcpy(work, "About to unload ");
	if (moduleName != NULL) strcat(work, moduleName);
	else strcat(work, "All");
	logAddPInfo(work);
	logBuildEntry(LET_unload, work, FALSE);
}

void logMainEvent() {
	logBuildEntry(LET_main, "dbc starting", FALSE);
}

void logMakeEvent(CHAR *name, CHAR *work)
{
	CHAR work2[LOGDATAMAXSIZE];
	strcpy(work2, "'MAKE'");
	if (name != NULL) {
		strcat(work2, ", name=");
		strcat(work2, name);
	}
	if (work != NULL) {
		strcat(work2, ", work=");
		strcat(work2, work);
	}
	logAddPInfo(work2);
	logBuildEntry(LET_make, work2, FALSE);
}

static void logAddVandPInfo(CHAR *work) {
	CHAR work2[32];
	logAddPInfo(work);
	if (logVcodePtr != NULL) {
		sprintf(work2, ", vb=%#x", (UINT)*logVcodePtr);
		strcat(work, work2);
	}
	if (logVXcodePtr != NULL && *logVcodePtr == 0x58) {
		sprintf(work2, ", vx=%#x", (UINT)*logVXcodePtr);
		strcat(work, work2);
	}
}

void logWriteBufferToFile(CHAR*buffer, INT fileHandle, FHANDLE osHandle) {
	OFFSET fpos = 0;
	logTerminateLogOutputBuffer(buffer);
	if (fioalseek(osHandle, 0L, 2, &fpos)) return;
	fiowrite(fileHandle, fpos, (UCHAR*)buffer, strlen(buffer));
}

#if OS_UNIX && defined(Linux) && !defined(_AIX)
static void logDumpEventStackTrace(INT index, INT fileHandle, FHANDLE osHandle) {
	CHAR buffer[1024];
	LOGENTRY *le;
	void *sbuffer;
	char **strings;
	int nptrs, j1;
	le = &internalLog[index];
	sbuffer = le->backtrace;
	nptrs = le->sizeOfBacktrace;
	if (nptrs > 0) {
		strings = backtrace_symbols(sbuffer, nptrs);
		if (strings != NULL) {
			for (j1 = 0; j1 < nptrs; j1++) {
				sprintf(buffer, "\t%s", strings[j1]);
				logWriteBufferToFile(buffer, fileHandle, osHandle);
			}
		}
		free(strings);
		free(sbuffer);
		internalLog[index].backtrace = NULL;
		internalLog[index].sizeOfBacktrace = 0;
	}
}
#endif

static void logDumpLoadmodChainFlags(CHAR* buffer, INT flags, INT error, INT fileHandle, FHANDLE osHandle) {
	CHAR work2[64];
	if (flags & LOGEVENTDATA_OFLAG_dataxcnt) {
		strcpy(buffer, "                     dataxcnt flowed over 0x7FFF");
		logWriteBufferToFile(buffer, fileHandle, osHandle);
	}

	if (flags & LOGEVENTDATA_OFLAG_pgmxcnt) {
		strcpy(buffer, "                     pgmxcnt flowed over");
		logWriteBufferToFile(buffer, fileHandle, osHandle);
	}

	if (flags & LOGEVENTDATA_OFLAG_pgmwork_codeptr_NotSet) {
		strcpy(buffer, "                     pgmwork_codeptr_NotSet");
		logWriteBufferToFile(buffer, fileHandle, osHandle);
	}

	if (flags & LOGEVENTDATA_OFLAG_error) {
		sprintf(work2, "                     error=%d", error);
		strcat(buffer, work2);
		logWriteBufferToFile(buffer, fileHandle, osHandle);
	}
}

static void logDumpChainEvent(CHAR* buffer, LOGENTRY *logentry, INT fileHandle, FHANDLE osHandle) {
	CHAINEVENTDATA *cedata = &logentry->DATA.cedata;
	CHAR work2[64];
	strcat(buffer, "About to chain ");
	if (cedata->moduleName[0] != '\0') strcat(buffer, cedata->moduleName);
	else strcat(buffer, "NULL");
	sprintf(work2, ", pcount=%d", logentry->pcount);
	strcat(buffer, work2);
	sprintf(work2, ", ModVer=%u", (UINT)cedata->moduleVersion);
	strcat(buffer, work2);
	logWriteBufferToFile(buffer, fileHandle, osHandle);

	if (cedata->newDatatabNumberOfEntries != -1 || cedata->newPgmtabNumberOfEntries != -1) {
		strcpy(buffer, "                     ");
		if (cedata->newDatatabNumberOfEntries != -1) {
			sprintf(work2, "newDatatabEntryCount=%d  ", cedata->newDatatabNumberOfEntries);
			strcat(buffer, work2);
		}
		if (cedata->newPgmtabNumberOfEntries != -1) {
			sprintf(work2, "newPgmtabEntryCount=%d", cedata->newPgmtabNumberOfEntries);
			strcat(buffer, work2);
		}
		logWriteBufferToFile(buffer, fileHandle, osHandle);
	}
	logDumpLoadmodChainFlags(buffer, cedata->oflags, cedata->error, fileHandle, osHandle);
}

static void logDumpLoadmodEvent(CHAR* buffer, LOGENTRY *logentry, INT fileHandle, FHANDLE osHandle) {
	static char lmverbiage1[] = "loadmod ";
	static char lmverbiage2[] = "ploadmod ";
	CHAR work2[64];
	LOADMODEVENTDATA *ledata = &logentry->DATA.lmedata;

	strcat(buffer, "About to ");
	strcat(buffer, ledata->permanent ? lmverbiage2 : lmverbiage1);
	if (ledata->moduleName[0] != '\0') strcat(buffer, ledata->moduleName);
	else strcat(buffer, "NULL");
	if (ledata->instanceName[0] != '\0' && strlen(ledata->instanceName) > 0) {
		sprintf(work2, "<%s>", ledata->instanceName);
		strcat(buffer, work2);
	}
	sprintf(work2, " pgmmod=%d", ledata->pgmmod);
	strcat(buffer, work2);

	strcat(buffer, " fm ");
	if (ledata->loaderName[0] != '\0' && strlen(ledata->loaderName) > 0)
		strcat(buffer, ledata->loaderName);
	else strcat(buffer, "unknown");

	sprintf(work2, ", pcount=%d", logentry->pcount);
	strcat(buffer, work2);
	logWriteBufferToFile(buffer, fileHandle, osHandle);

	strcpy(buffer, "                     ");
	sprintf(work2, "dupcode=%d", ledata->dupcode);
	strcat(buffer, work2);
	sprintf(work2, ", existingInstanceFound=%d", ledata->existingInstanceFound);
	strcat(buffer, work2);
	sprintf(work2, ", ModVer=%u", (UINT)ledata->moduleVersion);
	strcat(buffer, work2);
	logWriteBufferToFile(buffer, fileHandle, osHandle);

	strcpy(buffer, "                     ");
	sprintf(work2, "previousCurrentInstance=%d", ledata->previousCurrentInstance);
	strcat(buffer, work2);
	if (ledata->newDatatabNumberOfEntries != -1) {
		sprintf(work2, ", newDatatabEntryCount=%d", ledata->newDatatabNumberOfEntries);
		strcat(buffer, work2);
	}
	if (ledata->newPgmtabNumberOfEntries != -1) {
		sprintf(work2, ", newPgmtabEntryCount=%d", ledata->newPgmtabNumberOfEntries);
		strcat(buffer, work2);
	}
	logWriteBufferToFile(buffer, fileHandle, osHandle);
	logDumpLoadmodChainFlags(buffer, ledata->oflags, ledata->error, fileHandle, osHandle);
}

static void logDumpBuildBufferAndOutput(USHORT index, INT fileHandle, FHANDLE osHandle) {
	CHAR buffer[1024];
	CHAR work2[32];
	LOGENTRY *logentry = &internalLog[index];

	buffer[0] = '\0';
	logTimeStampForDump(buffer, logentry->logentrytime);
	switch (logentry->eventtype) {
		case LET_chain:
			logDumpChainEvent(buffer, logentry, fileHandle, osHandle);
			break;
		case LET_loadmod:
			logDumpLoadmodEvent(buffer, logentry, fileHandle, osHandle);
			break;
		case LET_memcompact:
			strcat(buffer, logentry->DATA.string);
			sprintf(work2, ", pcount=%d", logentry->pcount);
			strcat(buffer, work2);
			sprintf(work2, ", vb=%#x", (UINT) logentry->vbcode);
			strcat(buffer, work2);
			if (logentry->vbcode == 0x58) {
				sprintf(work2, ", vx=%#x", (UINT) logentry->vx);
				strcat(buffer, work2);
			}
			logWriteBufferToFile(buffer, fileHandle, osHandle);
			break;
		default:
			strcat(buffer, logentry->DATA.string);
			logWriteBufferToFile(buffer, fileHandle, osHandle);
			break;
	}
}

void logDumpLog(INT fileHandle, FHANDLE osHandle) {
	CHAR buffer[1024];
	USHORT entryCount = circularEventLogRecycling ? circularEventLogSize : circularEventLogNumberOfEntries;
	USHORT index = circularEventLogRecycling ? circularEventLogLastIndexUsed : 0;
#if OS_UNIX && defined(Linux)
	struct utsname ubuf;
#endif
	strcpy((CHAR*)buffer, "Log dump");
#if OS_UNIX && defined(Linux)
	if (!uname(&ubuf)) {
		strcat((CHAR*)buffer, " uname.sysname=");
		strcat((CHAR*)buffer, ubuf.sysname);
		strcat((CHAR*)buffer, " uname.machine=");
		strcat((CHAR*)buffer, ubuf.machine);
	}
#endif
	logWriteBufferToFile(buffer, fileHandle, osHandle);
	if (entryCount == 0) {
		strcpy(buffer, "\tNo Entries");
		logWriteBufferToFile(buffer, fileHandle, osHandle);
		return;
	}

	if (!circularEventLogRecycling) {
		do {
			logDumpBuildBufferAndOutput(index, fileHandle, osHandle);
#if OS_UNIX && defined(Linux) && !defined(_AIX)
			if (internalLog[index].sizeOfBacktrace > 0) logDumpEventStackTrace(index, fileHandle, osHandle);
#endif
		} while (++index < entryCount);
	}
	else {
		int counter = 0;
		if (++index == circularEventLogSize) index = 0;
		for (; counter < entryCount; counter++) {
			logDumpBuildBufferAndOutput(index, fileHandle, osHandle);
#if OS_UNIX && defined(Linux) && !defined(_AIX)
			if (internalLog[index].sizeOfBacktrace > 0) logDumpEventStackTrace(index, fileHandle, osHandle);
#endif
			if (++index == circularEventLogSize) index = 0;
		}
	}
}
#endif   /* SVR5 */
