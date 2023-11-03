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

#ifndef _BASE_INCLUDED
#define _BASE_INCLUDED

#include "xml.h"

#ifdef __cplusplus
extern "C" {            /* Assume C declarations for C++ */
#endif /* __cplusplus */

/* memory management functions */
#define MEMFLAGS_ZEROFILL	0x0100

extern INT meminit(UINT, UINT, INT);
extern UCHAR **memalloc(INT countOfBytes, INT);
extern INT memchange(UCHAR **, UINT, INT);
extern void memfree(UCHAR **);
extern INT membuffer(UCHAR **, void (*)(INT), INT);
extern void membufend(UCHAR **);
extern void memcompact(void);
extern void membase(UCHAR **, UINT *, INT *);
extern INT isValidHandle(UCHAR **pptr);
extern UINT walkmemory(void);

/* property handling functions */

/* return value of element */
#define PRP_GETVALUE	0x00
/* get value of first child */
#define PRP_GETCHILD	0x02
/* get value of next sybling */
#define PRP_NEXT		0x04
/* convert value/name to lower case */
#define PRP_LOWER		0x10
/* convert value/name to upper case */
#define PRP_UPPER		0x20

#define DEBUG_NOTIME		0x01
#define DEBUG_NONEWLINE		0x02

extern int prpinit(ELEMENT *, char *);
extern int prpname(char **);
ELEMENT *prpgetprppos(void);
void prpsetprppos(ELEMENT *);
void SetDebugLogFileName(CHAR* name);
extern int prpgetvol(CHAR *, CHAR *, INT);
/**
 * Return 0 if found, 1 if not
 */
extern int prpget(char *, char *, char *, char *, char **, int);
extern int prptranslate(char *, unsigned char *);

/* prevent interrupt functions */
extern void pvistart(void);
extern void pviend(void);
#if OS_WIN32
extern LPCRITICAL_SECTION pvi_getcrit(void);
extern LONG pvi_getpviflag(void);
#endif

/* console display functions */
extern void dspsilent(void);
extern void dspstring(char *);
extern void dspchar(char);
extern void dspflush(void);

/* miscellaneous functions */
extern INT fromprintable(UCHAR *, INT, UCHAR *, INT *);
extern INT makeprintable(UCHAR *idata, INT ilen, UCHAR *odata, INT *olen);
extern void mscatooff(CHAR *, OFFSET *);
extern void mscntoi(UCHAR *, INT *, INT);
extern void mscntooff(UCHAR *, OFFSET *, INT);
extern void msc9tooff(UCHAR *, OFFSET *);
extern void msc6xtooff(UCHAR *, OFFSET *);
#ifndef _Out_
#define _Out_
#endif
extern int mscis_integer(CHAR* src, INT srclength);
extern int mscitoa(INT src, _Out_ CHAR *dest);
extern int mscofftoa(OFFSET, CHAR *);
extern void msciton(INT, UCHAR *, INT);
extern void mscoffton(OFFSET, UCHAR *, INT);
extern void mscoffto9(OFFSET, UCHAR *);
extern void mscoffto6x(OFFSET, UCHAR *);
extern void msctimestamp(UCHAR *);
extern void mscgetparms(CHAR ***, INT *, CHAR *);
extern void debugmessage(CHAR* msg, INT flags);
extern void debugmessageW(wchar_t* msg, INT flags);
extern void debugflock(INT flag);
extern void itonum5(INT, UCHAR *);
#ifdef NEED_FCVT
extern CHAR *fcvt(double value, INT ndec, INT *decptr, INT *signptr);
#endif

/**
 *
 * File Descriptors on Linux are always INT
 * File Handles on Windows can be 32 or 64
 * So we define our own
 *
 */
#ifndef FHANDLE_DEFINED
#if OS_WIN32
typedef HANDLE FHANDLE;
#elif OS_UNIX
typedef INT FHANDLE;
#endif
#define FHANDLE_DEFINED
#endif

#ifdef __cplusplus
}                       /* End of extern "C" { */
#endif       /* __cplusplus */

#endif  /* _BASE_INCLUDED */
