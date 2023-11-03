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
#define INC_MATH
#define INC_SETJMP
#define INC_STDLIB
#include "includes.h"
#include "dbc.h"

#if OS_WIN32
#include "xml.h"
#include "vid.h"
#endif

/* local variables */
static INT cverbd, cverbp;
static INT cverbc = -1;
static INT cverbf = -1;

#if OS_WIN32
typedef void (_CCALLINIT)(void (**)(void));
typedef void (_CCALL)(CHAR *, UCHAR **);
typedef void (_CVERBINIT)(void (**)(void));
typedef void (_CVERB)(CHAR *, UCHAR **);
static _CCALLINIT *ccallinit;
static _CCALL *ccall;
static _CVERBINIT *cverbinit;
static _CVERB *cverb;
static void (*dbcfunc[])(void) = {
	(void (*)(void)) dbcgetflags,
	(void (*)(void)) dbcseteos,
	(void (*)(void)) dbcsetequal,
	(void (*)(void)) dbcsetless,
	(void (*)(void)) dbcsetover,
	(void (*)(void)) dbcgetparm,
	(void (*)(void)) dbcresetparm,
	(void (*)(void)) dbcgetdata,
	(void (*)(void)) dbcdeath,
	(void (*)(void)) vidsuspend,
	(void (*)(void)) vidresume
};
static UCHAR ccallinitflag = 0;
static UCHAR cverbinitflag = 0;
static HINSTANCE ccallhandle;
static HINSTANCE cverbhandle;
#endif

#if OS_UNIX
extern void ccall(CHAR *, UCHAR **);
extern void cverb(CHAR *, UCHAR **);
#endif

#if OS_WIN32
static void loadccall(void);
static void loadcverb(void);
static void freeccall(void);
static void freecverb(void);
#endif


/* VCCALL */
void vccall()
{
	UINT i1;
	UCHAR *parmadr[200];

	cvtoname(getvar(VAR_READ));
	for (i1 = 0; (parmadr[i1] = getlist(LIST_READ | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1)) != NULL; )
		if (i1 < (sizeof(parmadr) / sizeof(*parmadr) - 1)) i1++;

#if OS_WIN32
	if (!ccallinitflag) loadccall();
#endif

	ccall(name, parmadr);
	/* reset memory pointers */
	setpgmdata();
}

/* VCVERB */
void vcverb()
{
	UINT i1;
	UCHAR *parmadr[400];

	cvtoname(getvar(VAR_READ));
	for (i1 = 0; pgm[pcount] < 0xF8; ) {
		parmadr[i1] = getvar(VAR_READ);
		if (i1 < (sizeof(parmadr) / sizeof(*parmadr) - 1)) i1++;
	}
	if (pgm[pcount] != 0xFF) {
		cverbc = cverbf = pcount;
		do {
			if ((i1 = pgm[pcount++] - 0xF7) == 7) i1 = 1;
			while (i1--) {
				if (pgm[pcount] == 0xFF) pcount += 2;  /* skip decimal contants */
				else getvar(VAR_READ);
			}
		} while (pgm[pcount] != 0xFF);
	}
	else cverbc = cverbf = -1;
	pcount++;
	cverbd = datamodule;
	cverbp = pgmmodule;

#if OS_WIN32
	if (!cverbinitflag) loadcverb();
#endif

	cverb(name, parmadr);
	cverbc = cverbf = -1;
	/* reset memory pointers */
	setpgmdata();
}

/* DBCGETFLAGS */
INT dbcgetflags()
{
	return(dbcflags & DBCFLAG_FLAGS);
}

/* DBCSETEOS */
void dbcseteos(INT value)
{
	if (value) dbcflags |= DBCFLAG_EOS;
	else dbcflags &= ~DBCFLAG_EOS;
}

/* DBCSETEQUAL */
void dbcsetequal(INT value)
{
	if (value) dbcflags |= DBCFLAG_EQUAL;
	else dbcflags &= ~DBCFLAG_EQUAL;
}

/* DBCSETLESS */
void dbcsetless(INT value)
{
	if (value) dbcflags |= DBCFLAG_LESS;
	else dbcflags &= ~DBCFLAG_LESS;
}

/* DBCSETOVER */
void dbcsetover(INT value)
{
	if (value) dbcflags |= DBCFLAG_OVER;
	else dbcflags &= ~DBCFLAG_OVER;
}

/* DBCGETPARM */
INT dbcgetparm(UCHAR *list[])
{
	INT i1, i2, i3, listcnt;
	UCHAR *adr1, *ptr;

	if ((ptr = getmpgm(cverbp)) == NULL) dbcerror(555);
	if (cverbc == -1 || (i1 = ptr[cverbc] - 0xF8) > 6) return(-1);  /* end of list */
	cverbc++;
	listcnt = 0;
	do {
		i2 = cverbd;
		getvarx(ptr, &cverbc, &i2, &i3);
		if (i1 == 6) list[listcnt++] = NULL;  /* =type */
		if ((adr1 = findvar(i2, i3)) == NULL) dbcerror(552);
		list[listcnt++] = adr1;
	} while (i1 != 6 && listcnt <= i1);
	return(listcnt - 1);
}

/* DBCRESETPARM */
void dbcresetparm()
{
	cverbc = cverbf;
}

/* DBCGETDATA */
UCHAR *dbcgetdata(INT mod)
{
	if ((UINT) mod == 0xFFFF) return(NULL);
	return(getmdata(mod));
}

#if OS_WIN32
static void loadccall()
{
	ccallhandle = LoadLibrary("DBCCCALL");
	if (ccallhandle == NULL) {
		errno = GetLastError();	//will be used if dbcdx.file.error=on
		dbcerror(653);
	}

	ccall = (_CCALL *) GetProcAddress(ccallhandle, "ccall");
	if (ccall == NULL) {
		errno = GetLastError();	//will be used if dbcdx.file.error=on
		FreeLibrary(ccallhandle);
		dbcerror(655);
	}

	ccallinit = (_CCALLINIT *) GetProcAddress(ccallhandle, "ccallinit");
	if (ccallinit != NULL) ccallinit(dbcfunc);
	ccallinitflag = 1;
	atexit(freeccall);
}

static void loadcverb()
{
	cverbhandle = LoadLibrary("DBCCVERB");
	if (cverbhandle == NULL) {
		errno = GetLastError();	//will be used if dbcdx.file.error=on
		dbcerror(653);
	}

	cverb = (_CVERB *) GetProcAddress(cverbhandle, "cverb");
	if (cverb == NULL) {
		errno = GetLastError();	//will be used if dbcdx.file.error=on
		FreeLibrary(cverbhandle);
		dbcerror(655);
	}

	cverbinit = (_CVERBINIT *) GetProcAddress(cverbhandle, "cverbinit");
	if (cverbinit != NULL) cverbinit(dbcfunc);
	cverbinitflag = 1;
	atexit(freecverb);
}

static void freeccall()
{
	FreeLibrary(ccallhandle);
}

static void freecverb()
{
	FreeLibrary(cverbhandle);
}
#endif
