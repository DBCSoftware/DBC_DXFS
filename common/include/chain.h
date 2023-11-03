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

#define RESTART1	0x01
#define RESTART2	0x02
#define SYSTEM		0x04
#define COMPILE	0x08

#define COMPILING		'0'
#define COMPILED		'1'
#define EXECUTING		'2'
#define ABORTED		'3'
#define FINISHED		'4'
#define END_OF_EXTENT	'9'

#define COMMAND	' '
#define COMMENT	':'
#define PAUSE		'*'
#define ABORTOFF	'A'
#define ABORTON	'B'
#define ABTIF		'C'
#define ERRABORT	'D'
#define ERRGOTO	'E'
#define ERRGOTOX	'F'
#define GOTO		'G'
#define GOTOX		'H'
#define KEYBOARD	'I'
#define LABEL		'J'
#define LOG		'K'
#define LOGOFF		'L'
#define NOABORT	'M'
#define SOUNDR		'N'
#define STAMP		'O'
#define SYSTEMON	'P'
#define SYSTEMOFF	'Q'
#define TERMINATE	'R'

#define RECMAX 256
#define HDRSIZ 38
#define EOFSIZ 19
#define SYMNAMSIZ 12

#define DEATH_INTERRUPT		0
#define DEATH_INVPARM		1
#define DEATH_INVPARMVAL		2
#define DEATH_INIT			3
#define DEATH_NOMEM			4
#define DEATH_OPEN			5
#define DEATH_CREATE		6
#define DEATH_CLOSE			7
#define DEATH_READ			8
#define DEATH_WRITE			9
#define DEATH_NOLOOP		10
#define DEATH_INVWORK		11
#define DEATH_TOOMANYSYM		12
#define DEATH_CIRCULAR		13
#define DEATH_INVDIRECT		14
#define DEATH_INVNUM		15
#define DEATH_NOQUOTE		16
#define DEATH_NOPAREN		17
#define DEATH_INVCHAR		18
#define DEATH_TOOMANYEXP		19
#define DEATH_INVEXP		20
#define DEATH_PROG1			21
#define DEATH_NOOP			22
#define DEATH_NOFILE		23
#define DEATH_DOUNTIL		24
#define DEATH_WHILEEND		25
#define DEATH_IFXIF			26
#define DEATH_IFELSEXIF		27
#define DEATH_TOOMANYLOOP	28
#define DEATH_TOOMANYINC		29
#define DEATH_TOOMANYLAB		30
#define DEATH_DUPLAB		31
#define DEATH_INVOPER		32
#define DEATH_WRITEEOF		33
#define DEATH_WRITEDEL		34
#define DEATH_ZERODIV		35

extern INT compile(CHAR *);
extern void execution(INT, CHAR *);
extern void usage(void);

#if OS_WIN32
__declspec (noreturn) extern void death(INT, INT, CHAR *, INT);
#else
extern void death(INT, INT, CHAR *, INT)  __attribute__((__noreturn__));
#endif

