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

#ifndef _FSFILEIO_INCLUDED
#define _FSFILEIO_INCLUDED

#ifndef _OFFSET_TYPEDEF
#define _OFFSET_TYPEDEF
#if defined(WIN32) | defined(_WIN32) | defined(_WIN64) | defined(__NT__) | defined(__WINDOWS_386__)
typedef __int64 OFFSET;
#else
#if defined(UNIX) | defined(unix) || defined(__MACOSX)
#include <sys/types.h>
typedef off_t OFFSET;
#else
	#error  Appropriately define either WIN32 or UNIX to cause OFFSET to be defined
#endif
#endif
#endif

#define FS_TEXT				0x00000001
#define FS_DATA				0x00000002
#define FS_CRLF				0x00000004
#define FS_EBCDIC			0x00000008
#define FS_BINARY			0x00000010
#define FS_READONLY			0x00000020
#define FS_EXCLUSIVE		0x00000040
#define FS_CREATEONLY		0x00000080
#define FS_VARIABLE			0x00000100
#define FS_COMPRESSED		0x00000200
#define FS_DUP				0x00000400
#define FS_NODUP			0x00000800
#define FS_CASESENSITIVE	0x00001000
#define FS_LOCKAUTO			0x00002000
#define FS_LOCKSINGLE		0x00004000
#define FS_LOCKNOWAIT		0x00008000
#define FS_NOEXTENSION		0x00010000

extern int fsgetgreeting(char *server, int port, int encryptionflag, char *authfile, char *msg, int msglength);

#ifndef DX_SINGLEUSER
extern int fsgeterror(char *msg, int msglength);
#else
#if OS_UNIX
__attribute__((unused)) static int fsgeterror(char *msg, int msglength) { msg[0] = '\0'; return 0;}
#else
static int fsgeterror(char *msg, int msglength) { msg[0] = '\0'; return 0;}
#endif
#endif

extern int fsconnect(char *server, int serverport, int localport, int encryptionflag, char *authfile, char *database, char *user, char *password);
extern int fsdisconnect(int connecthandle);
extern int fsversion(int connecthandle, int *majorver, int *minorver);
extern int fsopen(int connecthandle, char *txtfilename, int options, int recsize);
extern int fsprep(int connecthandle, char *txtfilename, int options, int recsize);
extern int fsopenisi(int connecthandle, char *isifilename, int options, int recsize, int keysize);
extern int fsprepisi(int connecthandle, char *txtfilename, char *isifilename, int options, int recsize, int keysize);
extern int fsopenaim(int connecthandle, char *aimfilename, int options, int recsize, char matchchar);
extern int fsprepaim(int connecthandle, char *txtfilename, char *aimfilename, int options, int recsize, char *keyinfo, char matchchar);
extern int fsopenraw(int connecthandle, char *txtfilename, int options);
extern int fsprepraw(int connecthandle, char *txtfilename, int options);
extern int fsclose(int filehandle);
extern int fsclosedelete(int filehandle);
extern int fsfposit(int filehandle, OFFSET *offset);
extern int fsreposit(int filehandle, OFFSET offset);
extern int fssetposateof(int filehandle);
extern int fsreadrandom(int filehandle, OFFSET recnum, char *record, int length);
extern int fsreadrandomlock(int filehandle, OFFSET recnum, char *record, int length);
extern int fsreadnext(int filehandle, char *record, int length);
extern int fsreadnextlock(int filehandle, char *record, int length);
extern int fsreadprev(int filehandle, char *record, int length);
extern int fsreadprevlock(int filehandle, char *record, int length);
extern int fsreadsame(int filehandle, char *record, int length);
extern int fsreadsamelock(int filehandle, char *record, int length);
extern int fsreadkey(int filehandle, char *key, char *record, int length);
extern int fsreadkeylock(int filehandle, char *key, char *record, int length);
extern int fsreadkeynext(int filehandle, char *record, int length);
extern int fsreadkeynextlock(int filehandle, char *record, int length);
extern int fsreadkeyprev(int filehandle, char *record, int length);
extern int fsreadkeyprevlock(int filehandle, char *record, int length);
extern int fsreadaimkey(int filehandle, char *aimkey, char *record, int length);
extern int fsreadaimkeylock(int filehandle, char *aimkey, char *record, int length);
extern int fsreadaimkeys(int filehandle, char **aimkeys, int aimkeyscount, char *record, int length);
extern int fsreadaimkeyslock(int filehandle, char **aimkeys, int aimkeyscount, char *record, int length);
extern int fsreadaimnext(int filehandle, char *record, int length);
extern int fsreadaimnextlock(int filehandle, char *record, int length);
extern int fsreadaimprev(int filehandle, char *record, int length);
extern int fsreadaimprevlock(int filehandle, char *record, int length);
extern int fsreadraw(int filehandle, OFFSET offset, char *buffer, int length);
extern int fswriterandom(int filehandle, OFFSET recnum, char *record, int length);
extern int fswritenext(int filehandle, char *record, int length);
extern int fswriteateof(int filehandle, char *record, int length);
extern int fswritekey(int filehandle, char *key, char *record, int length);
extern int fswriteaim(int filehandle, char *record, int length);
extern int fswriteraw(int filehandle, OFFSET offset, char *buffer, int length);
extern int fsinsertkey(int filehandle, char *key);
extern int fsinsertkeys(int filehandle, char *record, int length);
extern int fsupdate(int filehandle, char *record, int length);
extern int fsdelete(int filehandle);
extern int fsdeletekey(int filehandle, char *key);
extern int fsdeletekeyonly(int filehandle, char *key);
extern int fsunlock(int filehandle, OFFSET offset);
extern int fsunlockall(int filehandle);
extern int fsweof(int filehandle, OFFSET offset);
extern int fscompare(int filehandle1, int filehandle2);
extern int fsfilelock(int filehandle);
extern int fsfileunlock(int filehandle);
extern int fsfilesize(int filehandle, OFFSET *size);
extern int fsrename(int filehandle, char *newname);
extern int fsexecute(int connecthandle, char *command);

#endif  /* _FSFILEIO_INCLUDED */
