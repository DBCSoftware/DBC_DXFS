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

#define FILEOPTIONS_VARIABLE		0x00000001
#define FILEOPTIONS_COMPRESSED		0x00000002
#define FILEOPTIONS_CASESENSITIVE	0x00000004
#define FILEOPTIONS_NOEXTENSION		0x00000008
#define FILEOPTIONS_DUP				0x00000010
#define FILEOPTIONS_NODUP			0x00000020
#define FILEOPTIONS_TEXT			0x00000100
#define FILEOPTIONS_DATA			0x00000200
#define FILEOPTIONS_CRLF			0x00000400
#define FILEOPTIONS_EBCDIC			0x00000800
#define FILEOPTIONS_BINARY			0x00001000
#define FILEOPTIONS_ANY				0x00002000
#define FILEOPTIONS_READONLY		0x00004000
#define FILEOPTIONS_READACCESS		0x00008000
#define FILEOPTIONS_EXCLUSIVE		0x00010000
#define FILEOPTIONS_CREATEFILE		0x00020000
#define FILEOPTIONS_CREATEONLY		0x00040000
#define FILEOPTIONS_LOCKAUTO		0x00100000
#define FILEOPTIONS_LOCKSINGLE		0x00200000
#define FILEOPTIONS_LOCKNOWAIT		0x00400000
#define FILEOPTIONS_FILEOPEN		0x01000000
#define FILEOPTIONS_IFILEOPEN		0x02000000
#define FILEOPTIONS_AFILEOPEN		0x04000000
#define FILEOPTIONS_RAWOPEN			0x08000000

INT fileconnect(CHAR *, CHAR *, CHAR *, CHAR *, CHAR *logfile);
INT filegetinfo(CHAR *, INT, CHAR *, INT *);
INT filedisconnect(INT);
INT fileopen(INT, CHAR *, INT, INT, CHAR *, INT, CHAR *, INT, INT *);
INT fileclose(INT, INT, INT);
INT filefposit(INT, INT, OFFSET *);
INT filereposit(INT, INT, OFFSET);
INT filereadrec(INT, INT, INT, OFFSET, UCHAR *, INT *);
INT filereadkey(INT, INT, INT, UCHAR *, INT, UCHAR *, INT *);
INT filereadaim(INT, INT, INT, INT, UCHAR **, INT *, UCHAR *, INT *);
INT filereadnext(INT, INT, INT, INT, UCHAR *, INT *);
INT filereadraw(INT, INT, OFFSET, UCHAR *, INT *);
INT filewriterec(INT, INT, OFFSET, UCHAR *, INT);
INT filewritekey(INT, INT, UCHAR *, INT, UCHAR *, INT);
INT filewriteraw(INT, INT, OFFSET, UCHAR *, INT);
INT fileinsert(INT, INT, UCHAR *, INT);
INT fileupdate(INT, INT, UCHAR *, INT);
INT filedelete(INT, INT, UCHAR *, INT);
INT filedeletek(INT, INT, UCHAR *, INT);
INT fileunlockrec(INT, INT, OFFSET);
INT fileunlockall(INT, INT);
INT filecompare(INT, INT, INT);
INT filelock(INT, INT);
INT fileunlock(INT, INT);
INT fileweof(INT, INT, OFFSET);
INT filesize(INT, INT, OFFSET *);
INT filerename(INT, INT, UCHAR *, INT);
INT filecommand(INT, UCHAR *, INT);
CHAR *filemsg(void);
