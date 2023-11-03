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

#ifndef _UTIL_INCLUDED
#define _UTIL_INCLUDED

#define UTIL_FIXUP_CHOP		0x00001
#define UTIL_FIXUP_UNIX		0x00002
#define UTIL_FIXUP_ARGV		0x00004
#define UTIL_FIXUP_OLDWIN	0x00008		/* convert windows string to FS 3.0 compatible */

#define UTIL_AIMDEX		0x00001
#define UTIL_BUILD		0x00002
#define UTIL_COPY		0x00004
#define UTIL_CREATE		0x00008
#define UTIL_DELETE		0x00010	/* FS only */
#define UTIL_ENCODE		0x00020
#define UTIL_ERASE		0x00040
#define UTIL_EXIST		0x00080
#define UTIL_INDEX		0x00100
#define UTIL_REFORMAT	0x00200
#define UTIL_RENAME		0x00400
#define UTIL_SORT		0x00800

#define UTIL_1STFILE	0x01000	/* internal use only */
#define UTIL_2NDFILE	0x02000	/* internal use only */
#define UTIL_2NDCHECK	0x04000	/* internal use only */

#define UTILERR_OPEN		1
#define UTILERR_CREATE		2
#define UTILERR_FIND		4
#define UTILERR_CLOSE		5
#define UTILERR_DELETE		6
#define UTILERR_READ		7
#define UTILERR_WRITE		8
#define UTILERR_CHANGED		14
#define UTILERR_LONGARG		9
#define UTILERR_BADARG		10
#define UTILERR_BADHDR		11
#define UTILERR_BADFILE		12
#define UTILERR_INTERNAL	13
#define UTILERR_NOMEM		15
#define UTILERR_BADEOF		16
#define UTILERR_BADOPT		17
#define UTILERR_TRANSLATE	18
#define UTILERR_SORT		19
#define UTILERR_DUPS		20
#define UTILERR_RENAME		21

extern INT utilfixupargs(INT type, CHAR *srcbuf, INT srclen, CHAR *destbuf, INT destlen);
extern INT utility(INT type, CHAR *args, INT length);
extern INT utilgeterrornum(INT errornum, CHAR *errorstr, INT size);
extern INT utilgeterror(CHAR *errorstr, INT size);
extern INT utilputerror(CHAR *errorstr);

#endif  /* _UTIL_INCLUDED */
