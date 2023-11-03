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

#ifndef _INCLUDES_INCLUDED
#define _INCLUDES_INCLUDED


/* define the OS_ values as 0 or 1 */

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64) || defined(__NT__) || defined(__WINDOWS_386__)
#define OS_WIN32 1
#ifdef _WINDOWS
#define OS_WIN32GUI 1
#else
#define OS_WIN32GUI 0
#endif
#else
#define OS_WIN32 0
#define OS_WIN32GUI 0
#endif

#if defined(UNIX) || defined(unix) || defined (__unix) || defined(__MACOSX)
#define OS_UNIX 1
#else
#define OS_UNIX 0
#endif

/* system include files */
#if OS_WIN32
#define memicmp(a, b, c) _memicmp(a, b, c)
#ifndef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#endif
#define STRICT
/* windows defaults to 64, unixware defaults to 1024 */
#define FD_SETSIZE 256
#ifdef INC_IPV6
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <windows.h>
#include <tchar.h>
#include <wchar.h>
#pragma warning(disable : 4005)

#else /* OS_WIN32 */
#include <inttypes.h>
typedef uint64_t ULONG_PTR;
typedef int64_t LONG_PTR;
typedef LONG_PTR LRESULT;
#define _T
#define SOCKET int
#define UINT_PTR ULONG
#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif
#endif

#include <stddef.h>

#ifdef INC_STDIO
#include <stdio.h>
#endif

#ifdef INC_STRING
#include <string.h>
#endif

#ifdef INC_CTYPE
#include <ctype.h>
#endif

#ifdef INC_STDLIB
#include <stdlib.h>
#endif

#ifdef INC_TIME
#include <time.h>
#endif

#ifdef INC_SIGNAL
#include <signal.h>
#endif

#ifdef INC_ERRNO
#include <errno.h>
#endif

#ifdef INC_ASSERT
#include <assert.h>
#endif

#ifdef INC_FLOAT
#include <float.h>
#endif

#ifdef INC_LIMITS
#include <limits.h>
#endif

#ifdef INC_LOCALE
#include <locale.h>
#endif

#ifdef INC_MATH
#define _USE_MATH_DEFINES
#include <math.h>
#endif

#ifdef INC_SETJMP
#include <setjmp.h>
#endif

#ifdef INC_STDARG
#include <stdarg.h>
#endif

#ifdef INC_SQL
#include <sql.h>  /* From ODBC SDK */
#endif

#ifdef INC_SQLEXT
#include <sqlext.h>  /* From ODBC SDK */
#endif

#if defined(INC_STDINT)
#include <stdint.h>
#endif

/* TRUE, FALSE definitions */
#if OS_UNIX
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#endif

/*
 * Maximum length of the Smart Client type string
 */
#define MAX_SCSUBTYPE_CHARS 12

/* standard data type definitions */
#if (OS_WIN32 == 0)

#ifdef FREEBSD
typedef unsigned char BYTE;
#endif

/* source files that include sqltypes.h will have these already defined */
#if !defined(FS_UNIX_ODBC) && !defined(__SQLTYPES_H) && !defined(_SQLTYPES_H)
typedef char CHAR;
typedef wchar_t WCHAR;
//#ifdef UNICODE
//typedef WCHAR TCHAR;
//#else
//typedef CHAR TCHAR;
//#endif
typedef unsigned char BYTE;
typedef unsigned char UCHAR;
typedef unsigned int UINT;
typedef unsigned long int ULONG;
typedef unsigned short int USHORT;
#endif
typedef short int SHORT;
typedef int INT;
typedef long int LONG;
#include <inttypes.h>
typedef int32_t INT32;
typedef uint64_t UINT64;
typedef uint32_t UINT32;
typedef double DOUBLE;
#ifdef FREEBSD
typedef BYTE BOOLEAN;
#endif
#endif


#if OS_WIN32 && defined(WIN32_LEAN_AND_MEAN)
typedef double DOUBLE;
#endif

typedef double DOUBLE64;

#ifndef _OFFSET_TYPEDEF
#define _OFFSET_TYPEDEF
#if OS_WIN32
typedef __int64 OFFSET; // @suppress("Type cannot be resolved")
#else
#if OS_UNIX
#include <sys/types.h>
typedef off_t OFFSET;
#else
typedef long OFFSET;
#endif
#endif
#endif

/* return codes */
#define RC_ERROR (-1)
#define RC_INVALID_HANDLE (-2)
#define RC_NO_MEM (-3)
#define RC_NAME_TOO_LONG (-4)
#define RC_NOT_FOUND (-5)
#define RC_INVALID_PARM (-6)
#define RC_INVALID_VALUE (-7)
#define RC_CANCEL (-8)


enum
{
    O32_LITTLE_ENDIAN = 0x03020100ul,
    O32_BIG_ENDIAN = 0x00010203ul,
};

static const union {
	unsigned char bytes[4];
#ifdef _WIN32
	__int32 value;
#else
	UINT32 value;
#endif
} o32_host_order = { { 0, 1, 2, 3 } };

#define O32_HOST_ORDER (o32_host_order.value)


#if OS_UNIX
#ifdef FORCE_STROPTS_DEFS
 #include "stropts.h"
#else
 #if !defined(__MACOSX) && !defined(Linux) && !defined(FREEBSD)
  #include <stropts.h>
 #endif
#endif
#endif

#endif  /* _INCLUDES_INCLUDED */
