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

#ifndef _FSRUN_INCLUDED
#define _FSRUN_INCLUDED

#define ERR_ALREADYCONNECTED		-1001
#define ERR_NOTCONNECTED			-1002
#define ERR_BADFUNC					-1003
#define ERR_MSGHDRDATASIZE			-1004
#define ERR_BADINTVALUE				-1005
#define ERR_INVALIDKEYWORD			-1006
#define ERR_REQUIREDKEYWORDMISSING	-1007
#define ERR_INVALIDSIZESPEC			-1008
#define ERR_INITFAILED				-1009
#define ERR_BADCONNECTID			-1010
#define ERR_INVALIDATTR				-1011
#define ERR_INVALIDVALUE			-1012
#define ERR_BADFILEID				-1013

#define FSFLAGS_SHUTDOWN	0x0001
#define FSFLAGS_LOGFILE		0x0002
#define FSFLAGS_DEBUG1		0x0100
#define FSFLAGS_DEBUG2		0x0200
#define FSFLAGS_DEBUG3		0x0400
#define FSFLAGS_DEBUG4		0x0800
#define FSFLAGS_DEBUG5		0x1000
#define FSFLAGS_DEBUG6		0x2000
#define FSFLAGS_DEBUG7		0x4000
#define FSFLAGS_DEBUG8		0x8000

/* function prototypes */

/* dbcfsrun.c */
extern void debug1(CHAR *);
extern INT getlicensenum(void);

/* Thinking of using something like this to replace arguments 2->4 of cfgaddname */
typedef struct NAMETABLE_STRUCT {
	CHAR **table;			/* memalloc pointer to table for names and strings */
	INT size;				/* memory actually in use in the name table */
	INT alloc;				/* total memory allocated for the name table */
} NAMETABLE;

/* dbcfscfg.c */
/*** CODE: THIS MAY BE TEMPORARY UNTIL I CAN FIGURE OUT WHAT TO DO ABOUT CONNECTION ***/
#ifndef _FSSQLX_INCLUDED
#include "fssqlx.h"
#endif
extern INT cfginit(CHAR *, CHAR *, CHAR *, CHAR *, CHAR *, CONNECTION *);
extern void cfgexit(void);
extern INT cfglogname(CHAR *);
extern INT cfggetentry(CHAR *, CHAR *, CHAR *, INT);
extern INT cfgaddname(CHAR *, CHAR ***nametable, INT *nametablesize, INT *nametablealloc);
extern INT writedbdfile(CONNECTION *);
extern INT coltype(CHAR *, INT, COLUMN *, CHAR *);
extern CHAR *cfggeterror(void);

#endif  /* _FSRUN_INCLUDED */
