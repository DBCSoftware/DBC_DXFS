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
#define INC_WINDOWS
#include "includes.h"
#include "base.h"
#include "fsrun.h"
#include "fsfile.h"
#include "release.h"
#include "fio.h"
#include "tcp.h"
#include "util.h"

#define FLAGS_UPDATE    0x01
#define FLAGS_AIMFIXED  0x02

#define FILEBASE	501  /* must be at least 1 */

#define MAX_VOLUMESIZE	8
typedef struct {
	CHAR volume[MAX_VOLUMESIZE + 1];
	CHAR **path;
} VOLUME;

typedef struct {
	INT type;
	INT options;
	INT flags;
	INT handle;
	INT xhandle;
	INT reclen;
	INT eorsize;
	OFFSET aimrec;
	OFFSET aimlastpos;
	OFFSET aimnextpos;
} FILEINFO;

/* global variables */
extern INT fsflags;
extern CHAR workdir[256];

/* local variables */
static INT cid;							/* this connection id */
static FILEINFO *fileinfo;				/* FILEINFO ptr */
static INT openhi, openmax;
static OFFSET filepos;
static CHAR errormsg[256];

static INT readrec(FILEINFO *file, INT lockflag, OFFSET pos, UCHAR *rec);
static INT readaimrec(FILEINFO *file, INT lockflag, INT aionextflag, UCHAR *rec);
static void putfilename(INT filenum);

/*
 * start a connection
 * Note that this function calls cfginit which calls fioinit
 */
INT fileconnect(CHAR *cfgfile, CHAR *dbdfile, CHAR *user, CHAR *password, CHAR *logfile)
{
	errormsg[0] = '\0';
	cid = cfginit(cfgfile, dbdfile, user, password, logfile, NULL);
	if (cid == -1) {
		strncpy(errormsg, cfggeterror(), sizeof(errormsg) - 1);
		errormsg[sizeof(errormsg) - 1] = '\0';
	}
	return cid;
}

/*** CODE: THIS LOOKS PRETTY USELESS, VERIFY ***/
/* get file information */
INT filegetinfo(CHAR *input, INT inputsize, CHAR *output, INT *outputsize)
{
	CHAR work[64];

	errormsg[0] = '\0';
	if (inputsize > 62) return ERR_INVALIDSIZESPEC;
	memcpy(work, input, inputsize);
	work[inputsize] = 0;
/*** CODE: THIS IS INCOMPLETE AND NEEDS TO RETURN CORRECT INFO ***/
	if (!strcmp(work, "VERSION")) strcpy(output, FS_MAJOR_MINOR_STRING);
	else if (!strcmp(work, "MAJORVERSION")) strcpy(output, FS_MAJOR_STRING);
	else if (!strcmp(work, "MINORVERSION")) strcpy(output, FS_MINOR_STRING);
	else if (!strcmp(work, "NAMECASESENSITIVE")) strcpy(output, "N");
	else if (!strcmp(work, "NAMETOLOWER")) strcpy(output, "N");
	else if (!strcmp(work, "NAMETOUPPER")) strcpy(output, "N");
	else if (!strcmp(work, "EXTUPPER")) strcpy(output, "N");
	else if (!strcmp(work, "COMPRESSION")) strcpy(output, "Y");
	else if (!strcmp(work, "VOLUMES")) strcpy(output, "");
	else return ERR_INVALIDKEYWORD;
	*outputsize = (INT)strlen(output);
	return 0;
}

/* disconnect */
INT filedisconnect(INT connectid)
{
	INT i1, fileid;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	for (fileid = FILEBASE, i1 = 0; i1 < openhi; i1++, fileid++) fileclose(connectid, fileid, FALSE);
	cfgexit();
	cid = 0;
	return 0;
}

/*
 * open and prepare
 */
INT fileopen(INT connectid, CHAR *datafilename, INT options, INT reclen, CHAR *indexfilename, INT keylen, CHAR *keyinfo, INT matchchar, INT *fileid)
{
	INT i1, opennum, opts, retcode;
	CHAR textfilename[512];
	FILEINFO *f1;

	errormsg[0] = '\0';
	*fileid = 0;
	if (connectid != cid) return ERR_BADCONNECTID;
	for (opennum = 0; opennum < openhi && fileinfo[opennum].type; opennum++);
	if (opennum == openmax) {
		if (!opennum) {
			i1 = 128;
			f1 = (FILEINFO *) malloc(i1 * sizeof(FILEINFO));
		}
		else {
			i1 = openmax * 2;
			f1 = (FILEINFO *) realloc((CHAR *) fileinfo, i1 * sizeof(FILEINFO));
		}
		if (f1 == NULL) {
			if (!(options & FILEOPTIONS_RAWOPEN)) return -614;
			return ERR_NOMEM;
		}
		fileinfo = f1;
		openmax = i1;
	}
	f1 = &fileinfo[opennum];
	memset(f1, 0, sizeof(FILEINFO));
	if (options & (FILEOPTIONS_IFILEOPEN | FILEOPTIONS_AFILEOPEN)) {
		if (datafilename == NULL || !datafilename[0]) textfilename[0] = '\0';
		else strcpy(textfilename, datafilename);
		if (options & FILEOPTIONS_IFILEOPEN) {
			miofixname(indexfilename, ".isi", FIXNAME_EXT_ADD);
			if (options & FILEOPTIONS_CREATEFILE) opts = XIO_M_PRP | XIO_P_TXT;
			else if (options & FILEOPTIONS_CREATEONLY) opts = FIO_M_EFC | XIO_P_TXT;
			else if (options & FILEOPTIONS_EXCLUSIVE) opts = XIO_M_EXC | XIO_P_TXT;
			else if (options & FILEOPTIONS_READONLY) opts = XIO_M_SRO | XIO_P_TXT;
			else if (options & FILEOPTIONS_READACCESS) opts = XIO_M_SRA | XIO_P_TXT;
			else opts = XIO_M_SHR | XIO_P_TXT;
			if (!(options & (FILEOPTIONS_VARIABLE | FILEOPTIONS_COMPRESSED))) opts |= XIO_FIX | XIO_UNC;
			if (options & FILEOPTIONS_DUP) opts |= XIO_DUP;
			else if (options & FILEOPTIONS_NODUP) opts |= XIO_NOD;
			retcode = xioopen(indexfilename, opts, keylen, reclen, &filepos, textfilename);
		}
		else {
			miofixname(indexfilename, ".aim", FIXNAME_EXT_ADD);
			if ((options & (FILEOPTIONS_CREATEFILE | FILEOPTIONS_CREATEONLY)) && (keyinfo == NULL || !keyinfo[0])) return -608;
			if (options & FILEOPTIONS_CREATEFILE) opts = AIO_M_PRP | AIO_P_TXT;
			else if (options & FILEOPTIONS_CREATEONLY) opts = FIO_M_EFC | AIO_P_TXT;
			else if (options & FILEOPTIONS_EXCLUSIVE) opts = AIO_M_EXC | AIO_P_TXT;
			else if (options & FILEOPTIONS_READONLY) opts = AIO_M_SRO | AIO_P_TXT;
			else if (options & FILEOPTIONS_READACCESS) opts = AIO_M_SRA | AIO_P_TXT;
			else opts = AIO_M_SHR | AIO_P_TXT;
			if (!(options & (FILEOPTIONS_VARIABLE | FILEOPTIONS_COMPRESSED))) opts |= AIO_FIX | AIO_UNC;
			retcode = aioopen(indexfilename, opts, reclen, &filepos, textfilename, &i1, keyinfo, options & FILEOPTIONS_CASESENSITIVE, matchchar);
			if (retcode >= 0) {
				if (matchchar != 0) aiomatch(retcode, matchchar);
				if (i1) f1->flags |= FLAGS_AIMFIXED;
			}
		}
		if (retcode < 0) {
			strncpy(errormsg, indexfilename, sizeof(errormsg) - 1);
			errormsg[sizeof(errormsg) - 1] = '\0';
			if (retcode == ERR_FNOTF) {
				if (!(options & (FILEOPTIONS_CREATEFILE | FILEOPTIONS_CREATEONLY))) retcode = -601;
				else retcode = -603;
			}
			else if (retcode == ERR_NOACC) retcode = -602;
			else if (retcode == ERR_BADNM) retcode = -603;
			else if (retcode == ERR_EXIST) retcode = -605;
			else if (retcode == ERR_BADKL) retcode = -606;
			else if (retcode == ERR_NOMEM) retcode = -614;
			else if (retcode == ERR_IXDUP) retcode = -656;
			else retcode -= 1630;
			return retcode;
		}
		f1->xhandle = retcode;
		if (datafilename == NULL || !datafilename[0]) datafilename = textfilename;
	}
	if (datafilename == NULL || !datafilename[0]) {
		if (!(options & FILEOPTIONS_RAWOPEN)) return -603;
		return ERR_BADNM;
	}
	if (!(options & FILEOPTIONS_RAWOPEN)) {
		if (options & FILEOPTIONS_CREATEFILE) {
			if (!(options & (FILEOPTIONS_IFILEOPEN | FILEOPTIONS_AFILEOPEN))) opts = RIO_M_PRP | RIO_P_TXT;
			else opts = RIO_M_MTC | RIO_P_TXT;
		}
		else if (options & FILEOPTIONS_CREATEONLY) opts = RIO_M_EFC | RIO_P_TXT;
		else if (options & FILEOPTIONS_EXCLUSIVE) {
			if (!(options & (FILEOPTIONS_IFILEOPEN | FILEOPTIONS_AFILEOPEN))) opts = RIO_M_EXC | RIO_P_TXT;
			else opts = RIO_M_MXC | RIO_P_TXT;
		}
		else if (options & FILEOPTIONS_READONLY) opts = RIO_M_SRO | RIO_P_TXT;
		else if (options & FILEOPTIONS_READACCESS) opts = RIO_M_SRA | RIO_P_TXT;
		else opts = RIO_M_SHR | RIO_P_TXT;
		if (options & FILEOPTIONS_TEXT) opts |= RIO_T_TXT;
		else if (options & FILEOPTIONS_DATA) opts |= RIO_T_DAT;
		else if (options & FILEOPTIONS_CRLF) opts |= RIO_T_DOS;
		else if (options & FILEOPTIONS_EBCDIC) {
			if (options & FILEOPTIONS_IFILEOPEN) xioclose(f1->xhandle);
			else if (options & FILEOPTIONS_AFILEOPEN) aioclose(f1->xhandle);
			return -604;
		}
		else if (options & FILEOPTIONS_BINARY) opts |= RIO_T_BIN;
		else if (options & FILEOPTIONS_ANY) opts |= RIO_T_ANY;
		else opts |= RIO_T_STD;
		if (!(options & FILEOPTIONS_VARIABLE)) opts |= RIO_FIX;
		if (!(options & FILEOPTIONS_COMPRESSED)) opts |= RIO_UNC;
		if (options & FILEOPTIONS_NOEXTENSION) opts |= RIO_NOX;
		if ((fsflags & FSFLAGS_LOGFILE) && cfglogname(datafilename)) opts |= RIO_LOG;
		retcode = rioopen(datafilename, opts, 0, reclen);
		if (retcode < 0) {
			strncpy(errormsg, datafilename, sizeof(errormsg) - 1);
			errormsg[sizeof(errormsg) - 1] = '\0';
			if (options & FILEOPTIONS_IFILEOPEN) xioclose(f1->xhandle);
			else if (options & FILEOPTIONS_AFILEOPEN) aioclose(f1->xhandle);
			if (retcode == ERR_FNOTF) {
				if (!(options & (FILEOPTIONS_CREATEFILE | FILEOPTIONS_CREATEONLY))) retcode = -601;
				else retcode = -603;
			}
			else if (retcode == ERR_NOACC) retcode = -602;
			else if (retcode == ERR_BADNM) retcode = -603;
			else if (retcode == ERR_BADTP) retcode = -604;
			else if (retcode == ERR_EXIST) retcode = -605;
			else if (retcode == ERR_NOMEM) retcode = -614;
			else retcode -= 1630;
			return retcode;
		}
	}
	else {
		if (options & FILEOPTIONS_CREATEFILE) opts = FIO_M_PRP | FIO_P_TXT;
		else if (options & FILEOPTIONS_CREATEONLY) opts = FIO_M_EFC | FIO_P_TXT;
		else if (options & FILEOPTIONS_EXCLUSIVE) opts = FIO_M_EXC | FIO_P_TXT;
		else if (options & FILEOPTIONS_READONLY) opts = FIO_M_SRO | FIO_P_TXT;
		else if (options & FILEOPTIONS_READACCESS) opts = FIO_M_SRA | FIO_P_TXT;
		else opts = FIO_M_SHR | FIO_P_TXT;
		retcode = fioopen(datafilename, opts);
		if (retcode < 0) {
			strncpy(errormsg, datafilename, sizeof(errormsg) - 1);
			errormsg[sizeof(errormsg) - 1] = '\0';
			return retcode;
		}
	}
	f1->handle = retcode;
	if (!(options & FILEOPTIONS_RAWOPEN)) {
		f1->eorsize = rioeorsize(retcode);
		if (options & FILEOPTIONS_IFILEOPEN) f1->type = FILEOPTIONS_IFILEOPEN;
		else if (options & FILEOPTIONS_AFILEOPEN) f1->type = FILEOPTIONS_AFILEOPEN;
		else f1->type = FILEOPTIONS_FILEOPEN;
	}
	else f1->type = FILEOPTIONS_RAWOPEN;
	f1->options = options;
	f1->reclen = reclen;
	f1->aimrec = -1;
	if (opennum >= openhi) openhi = opennum + 1;
	*fileid = opennum + FILEBASE;
	return 0;
}

/* close */
INT fileclose(INT connectid, INT fileid, INT deleteflag)
{
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];

	if (deleteflag) {
		if (f1->type != FILEOPTIONS_RAWOPEN) {
			riokill(f1->handle);
			if (f1->type == FILEOPTIONS_IFILEOPEN) xiokill(f1->xhandle);
			else if (f1->type == FILEOPTIONS_AFILEOPEN) aiokill(f1->xhandle);
		}
		else fiokill(f1->handle);
	}
	else {
		if (f1->type != FILEOPTIONS_RAWOPEN) {
			rioclose(f1->handle);
			if (f1->type == FILEOPTIONS_IFILEOPEN) xioclose(f1->xhandle);
			else if (f1->type == FILEOPTIONS_AFILEOPEN) aioclose(f1->xhandle);
		}
		else fioclose(f1->handle);
	}
	f1->type = f1->handle = 0;
	if (fileid == openhi - 1) openhi--;
	return 0;
}

/* fposit */
INT filefposit(INT connectid, INT fileid, OFFSET *filepos)
{
	INT retcode;
	OFFSET pos;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	if (fileinfo[fileid].type == FILEOPTIONS_RAWOPEN) return ERR_BADFUNC;
	retcode = riolastpos(fileinfo[fileid].handle, &pos);
	if (retcode < 0) {
		putfilename(fileinfo[fileid].handle);
		return retcode - 1730;
	}
	*filepos = pos;
	return 0;
}

/* reposit */
INT filereposit(INT connectid, INT fileid, OFFSET pos)
{
	INT retcode;
	OFFSET eofpos;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	if (fileinfo[fileid].type == FILEOPTIONS_RAWOPEN) return ERR_BADFUNC;
	if (pos < -1) return ERR_INVALIDVALUE;
	retcode = rioeofpos(fileinfo[fileid].handle, &eofpos);
	if (retcode < 0) {
		putfilename(fileinfo[fileid].handle);
		return retcode - 1730;
	}
	if (pos > eofpos) return 2;
	if (pos == -1) pos = eofpos;
	retcode = riosetpos(fileinfo[fileid].handle, pos);
	if (retcode < 0) {
		putfilename(fileinfo[fileid].handle);
		return retcode - 1730;
	}
	if (pos == eofpos) return 1;
	return 0;
}

/* read random, seq and back seq */
/* recnum:  -3 = back, -2 = same, -1 = seq, >= 0 is record number */
INT filereadrec(INT connectid, INT fileid, INT lockflag, OFFSET recnum, UCHAR *rec, INT *reclen)
{
	INT retcode;
	OFFSET pos;
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	if (f1->type == FILEOPTIONS_RAWOPEN) return ERR_BADFUNC;
	f1->flags &= ~FLAGS_UPDATE;
	if (f1->reclen > *reclen) return ERR_INVALIDSIZESPEC;
	if (f1->options & FILEOPTIONS_LOCKAUTO) lockflag = TRUE;
	if (lockflag && (f1->options & FILEOPTIONS_LOCKSINGLE)) riounlock(f1->handle, -1L);
	for ( ; ; ) {
		if (recnum == -3) {
			retcode = rioprev(f1->handle, rec, f1->reclen);
			if (retcode < 0) {
				if (retcode == -2) continue;
				if (retcode != -1) retcode -= 1730;
				break;
			}
			if (!lockflag) break;
		}
		if (recnum != -1) {
			if (recnum >= 0) pos = recnum * (f1->reclen + f1->eorsize);
			else riolastpos(f1->handle, &pos);
			riosetpos(f1->handle, pos);
		}
		if (lockflag) {
			retcode = riolock(f1->handle, f1->options & FILEOPTIONS_LOCKNOWAIT);
			if (retcode) {
				if (retcode < 0) retcode -= 1730;
				else retcode = -2;
				break;
			}
		}
		retcode = rioget(f1->handle, rec, f1->reclen);
		if (retcode < 0) {
			if (lockflag) {
				if (recnum == -1) riolastpos(f1->handle, &pos);
				riounlock(f1->handle, pos);
			}
			if (retcode == -2) {
				if (recnum < 0 && recnum != -2) continue;
				retcode = -720;
			}
			else if (retcode != -1) {
				if (retcode == ERR_SHORT) retcode = -715;
				else if (retcode == ERR_NOEOR) retcode = -716;
				else if (retcode == ERR_RANGE) retcode = -301;
				else if (retcode == ERR_NOEOF) retcode = 1730 - ERR_NOEOR;
				else retcode -= 1730;
			}
		}
		break;
	}
	if (retcode >= 0) {
		f1->flags |= FLAGS_UPDATE;
		*reclen = retcode;
		retcode = 0;
	}
	else putfilename(f1->handle);
	return retcode;
}

/* read by indexed key */
INT filereadkey(INT connectid, INT fileid, INT lockflag, UCHAR *key, INT keylen, UCHAR *rec, INT *reclen)
{
	INT flags, retcode, flckflag;
	OFFSET pos;
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	if (f1->type != FILEOPTIONS_IFILEOPEN) return ERR_BADFUNC;
	flags = f1->flags;
	f1->flags &= ~FLAGS_UPDATE;
	if (f1->reclen > *reclen) return ERR_INVALIDSIZESPEC;
	if (f1->options & FILEOPTIONS_LOCKAUTO) lockflag = TRUE;
	if (lockflag && (f1->options & FILEOPTIONS_LOCKSINGLE)) riounlock(f1->handle, -1L);
	flckflag = FALSE;
	for (pos = ~0; ; ) {  /* set to invalid position value */
		if (key != NULL && keylen) {
			retcode = fioflck(f1->handle);
			if (retcode) {
				putfilename(f1->handle);
				return retcode - 1730;
			}
			retcode = xiofind(f1->xhandle, key, keylen);
			if (retcode != 1 || lockflag) {
				fiofulk(f1->handle);
				if (retcode != 1) {
					if (retcode < 0) {
						putfilename(f1->xhandle);
						if (retcode == ERR_BADKY) retcode = -721;
						else retcode -= 1730;
					}
					else retcode = -1;
					return retcode;
				}
			}
			else flckflag = TRUE;
		}
		else {
			/* null key read */
			if (!(flags & FLAGS_UPDATE) || xiofind(f1->xhandle, NULL, 0) != 1) {
				if (!(flags & FLAGS_UPDATE)) putfilename(f1->handle);
				else putfilename(f1->xhandle);
				return -718;
			}
			riolastpos(f1->handle, &filepos);
			pos = filepos;  /* flag 720 error below */
		}
		retcode = readrec(f1, lockflag, filepos, rec);
		if (flckflag) fiofulk(f1->handle);
		else if (retcode == -720 && filepos != pos) {  /* record probably deleted between index and text access */
			pos = filepos;
			continue;
		}
		break;
	}
	if (retcode > 0) {
		*reclen = retcode;
		f1->flags |= FLAGS_UPDATE;
		retcode = 0;
	}
	else if (retcode != -2) {
		putfilename(f1->handle);
		if (retcode >= -1) retcode = -720;
	}
	return retcode;
}

/* read by aim key */
INT filereadaim(INT connectid, INT fileid, INT lockflag, INT keycount, UCHAR **key, INT *keylen, UCHAR *rec, INT *reclen)
{
	INT i1, i2, i3, flags, nullread, retcode;
	OFFSET pos;
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	if (f1->type != FILEOPTIONS_AFILEOPEN) return ERR_BADFUNC;
	flags = f1->flags;
	f1->flags &= ~FLAGS_UPDATE;
	if (f1->reclen > *reclen) return ERR_INVALIDSIZESPEC;
	if (f1->options & FILEOPTIONS_LOCKAUTO) lockflag = TRUE;
	if (lockflag && (f1->options & FILEOPTIONS_LOCKSINGLE)) riounlock(f1->handle, -1L);
	nullread = TRUE;
	for (i1 = 0; i1 < keycount; i1++) {
		i2 = keylen[i1];
		if (i2 <= 0) {
			if (!i2) continue;
/*** CODE: SHOULDN'T THIS BE SOMETHING ELSE ***/
			return ERR_INVALIDSIZESPEC;
		}
		if (nullread) {
			nullread = FALSE;
			f1->aimrec = -1L;
			i3 = aionew(f1->xhandle);
			if (i3) {
				putfilename(f1->xhandle);
				return i3 - 1730;
			}
		}
		i3 = aiokey(f1->xhandle, key[i1], i2);
		if (i3) {
			putfilename(f1->xhandle);
			if (i3 == ERR_BADKY) return -712;
			return i3 - 1730;
		}
	}
	if (!nullread) retcode = readaimrec(f1, lockflag, AIONEXT_NEXT, rec);
	else {
		if (!(flags & FLAGS_UPDATE)) return -718;
		riolastpos(f1->handle, &pos);
		retcode = readrec(f1, lockflag, pos, rec);
	}
	if (retcode >= 0) {
		*reclen = retcode;
		f1->flags |= FLAGS_UPDATE;
		retcode = 0;
	}
	else if (retcode != -2) putfilename(f1->handle);
	return retcode;
}

/* read next or prior key (index or aim) */
INT filereadnext(INT connectid, INT fileid, INT lockflag, INT nextflag, UCHAR *rec, INT *reclen)
{
	INT retcode;
	OFFSET pos;
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	if (f1->type != FILEOPTIONS_IFILEOPEN && f1->type != FILEOPTIONS_AFILEOPEN) return ERR_BADFUNC;
	f1->flags &= ~FLAGS_UPDATE;
	if (f1->reclen > *reclen) return ERR_INVALIDSIZESPEC;
	if (f1->options & FILEOPTIONS_LOCKAUTO) lockflag = TRUE;
	if (lockflag && (f1->options & FILEOPTIONS_LOCKSINGLE)) riounlock(f1->handle, -1L);
	if (f1->type == FILEOPTIONS_IFILEOPEN) {
		retcode = fioflck(f1->handle);
		if (retcode) {
			putfilename(f1->xhandle);
			return retcode - 1730;
		}
		for (pos = ~0; ; ) {  /* set to invalid position value */
			if (nextflag) retcode = xionext(f1->xhandle);
			else retcode = xioprev(f1->xhandle);
			if (retcode != 1 || lockflag) {
				fiofulk(f1->handle);
				if (retcode != 1) {
					if (retcode < 0) {
						putfilename(f1->xhandle);
						if (retcode == ERR_BADKY) retcode = -721;
						else retcode -= 1730;
					}
					else retcode = -1;
					return retcode;
				}
			}
			retcode = readrec(f1, lockflag, filepos, rec);
			if (!lockflag) fiofulk(f1->handle);
			else if (retcode == -720 && filepos != pos) {
				pos = filepos;
				/* try to reread index key by backing up */
				retcode = fioflck(f1->handle);
				if (retcode) {
					putfilename(f1->xhandle);
					return retcode - 1730;
				}
				if (nextflag) retcode = xioprev(f1->xhandle);
				else retcode = xionext(f1->xhandle);
				if (retcode < 0) {
					fiofulk(f1->handle);
					putfilename(f1->xhandle);
					return retcode - 1730;
				}
				continue;
			}
			if (retcode >= -1 && retcode <= 0) retcode = -720;
			break;
		}
	}
	else retcode = readaimrec(f1, lockflag, (nextflag) ? AIONEXT_NEXT : AIONEXT_PREV, rec);
	if (retcode >= 0) {
		*reclen = retcode;
		f1->flags |= FLAGS_UPDATE;
		retcode = 0;
	}
	else putfilename(f1->handle);
	return retcode;
}

/* read directly from the file */
INT filereadraw(INT connectid, INT fileid, OFFSET pos, UCHAR *rec, INT *reclen)
{
	INT retcode;
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	if (f1->type != FILEOPTIONS_RAWOPEN) return ERR_BADFUNC;
	retcode = fioread(f1->handle, pos, rec, *reclen);
	if (retcode >= 0) {
		*reclen = retcode;
		retcode = 0;
	}
	else putfilename(f1->handle);
	return retcode;
}

static INT readrec(FILEINFO *f1, INT lockflag, OFFSET pos, UCHAR *rec)
{
	INT retcode;

	errormsg[0] = '\0';
	riosetpos(f1->handle, pos);
	if (lockflag) {
		retcode = riolock(f1->handle, f1->options & FILEOPTIONS_LOCKNOWAIT);
		if (retcode) {
			if (retcode < 0) retcode -= 1730;
			else retcode = -2;
			return retcode;
		}
	}
	retcode = rioget(f1->handle, rec, f1->reclen);
	if (retcode < 0) {
		if (lockflag) riounlock(f1->handle, pos);
		if (retcode == -2) retcode = -720;
		else if (retcode != -1) {
			if (retcode == ERR_SHORT) retcode = -715;
			else if (retcode == ERR_NOEOR) retcode = -716;
			else if (retcode == ERR_RANGE) retcode = -301;
			else if (retcode == ERR_NOEOF) retcode = 1730 - ERR_NOEOR;
			else retcode -= 1730;
		}
	}
	return retcode;
}

static INT readaimrec(FILEINFO *f1, INT lockflag, INT aionextflag, UCHAR *rec) // @suppress("No return")
{
#define AIMREC_FLAGS_FORWARD	0x01
#define AIMREC_FLAGS_FIXED	0x02
#define AIMREC_FLAGS_CHECKPOS	0x04

	INT flags, retcode;
	OFFSET eofpos, pos;

	flags = 0;
	if (aionextflag == AIONEXT_NEXT) flags |= AIMREC_FLAGS_FORWARD;
	if (f1->flags & FLAGS_AIMFIXED) flags |= AIMREC_FLAGS_FIXED;
	if (f1->aimrec != -1) {
		if (aionextflag == AIONEXT_NEXT) riosetpos(f1->handle, f1->aimnextpos);
		else riosetpos(f1->handle, f1->aimlastpos);
		flags |= AIMREC_FLAGS_CHECKPOS;
	}
	for ( ; ; ) {
	/* read an aim record and see if it matches the criteria */
		if (f1->aimrec == -1) {
			retcode = aionext(f1->xhandle, aionextflag);
			if (retcode != 1) {
				if (retcode == 2) retcode = -1;
				else if (retcode == 3) retcode = -713;
				else if (retcode == ERR_BADKY) retcode = -712;
				else if (retcode == ERR_RANGE) retcode = -714;
				else retcode -= 1730;
				return retcode;
			}
	
			/* a possible record exists */
			if (flags & AIMREC_FLAGS_FIXED) {  /* fixed length records */
				pos = filepos * (f1->reclen + f1->eorsize);
				riosetpos(f1->handle, pos);
			}
			else {  /* variable length records */
				pos = filepos << 8;
				if (aionextflag == AIONEXT_PREV) {
					pos += 256;
					rioeofpos(f1->handle, &eofpos);
					if (pos > eofpos) pos = eofpos;
				}
				riosetpos(f1->handle, pos);
				if (pos) {
					rioget(f1->handle, rec, f1->reclen);
					if (aionextflag == AIONEXT_PREV) {
						rionextpos(f1->handle, &pos);
						riosetpos(f1->handle, pos);
					}
				}
				f1->aimrec = filepos;
			}
			flags &= ~AIMREC_FLAGS_CHECKPOS;
		}
		do {
			if (flags & (AIMREC_FLAGS_FORWARD | AIMREC_FLAGS_FIXED)) {
				if ((flags & AIMREC_FLAGS_CHECKPOS) && (f1->aimnextpos - 1) >> 8 != f1->aimrec) {
					f1->aimrec = -1;
					break;
				}
				retcode = rioget(f1->handle, rec, f1->reclen);
			}
			else retcode = rioprev(f1->handle,  rec, f1->reclen);
			if (retcode < 0 && retcode != -2) {
				f1->aimrec = -1;
				if (retcode != -1) {
					if (retcode == ERR_SHORT) retcode = -715;
					else if (retcode == ERR_NOEOR) retcode = -716;
					else if (retcode == ERR_RANGE) retcode = -301;
					else if (retcode == ERR_NOEOF) retcode = 1730 - ERR_NOEOR;
					else retcode -= 1730;
				}
				return retcode;
			}
			if (!(flags & AIMREC_FLAGS_FIXED)) {
				riolastpos(f1->handle, &pos);
				f1->aimlastpos = pos;
				if (pos) pos = (pos - 1) >> 8;
				if (pos != f1->aimrec) {
					f1->aimrec = -1;
					break;
				}
				pos = f1->aimlastpos;
				rionextpos(f1->handle, &f1->aimnextpos);
			}
			if (retcode == -2 || aiochkrec(f1->xhandle,  rec, retcode)) continue;
			if (!lockflag) return retcode;

			/* lock the reocrd */
			riosetpos(f1->handle, pos);
			retcode = riolock(f1->handle, f1->options & FILEOPTIONS_LOCKNOWAIT);
			if (retcode != 0) {
				if (retcode < 0) retcode -= 1730;
				else retcode = -2;
				return retcode;
			}
			retcode = rioget(f1->handle,  rec, f1->reclen);
			if (retcode >= 0 && !aiochkrec(f1->xhandle,  rec, retcode)) return retcode;
			riounlock(f1->handle, pos);
			if (retcode < 0 && retcode != -2) {
				if (retcode != -1) {
					if (retcode == ERR_SHORT) retcode = -715;
					else if (retcode == ERR_NOEOR) retcode = -716;
					else if (retcode == ERR_RANGE) retcode = -301;
					else if (retcode == ERR_NOEOF) retcode = 1730 - ERR_NOEOR;
					else retcode -= 1730;
				}
				return retcode;
			}
		} while (!((flags |= AIMREC_FLAGS_CHECKPOS) & AIMREC_FLAGS_FIXED));
	}
#undef AIMREC_FLAGS_FORWARD
#undef AIMREC_FLAGS_FIXED
#undef AIMREC_FLAGS_CHECKPOS
}

/*
 * write seq, at eof, or by record number
 * recnum:  -3 = at eof, -1 = seq, >= 0 is record number
 */
INT filewriterec(INT connectid, INT fileid, OFFSET recnum, UCHAR *rec, INT reclen)
{
	INT retcode;
	OFFSET pos;
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	if (f1->type == FILEOPTIONS_RAWOPEN) return ERR_BADFUNC;
	f1->flags &= ~FLAGS_UPDATE;
	if (recnum >= 0) {
		if (f1->options & (FILEOPTIONS_VARIABLE | FILEOPTIONS_COMPRESSED)) {
			putfilename(f1->handle);
			return -703;
		}
		pos = (OFFSET) recnum * (f1->reclen + f1->eorsize);
		riosetpos(f1->handle, pos);
	}
	else if (recnum == -3) {
		retcode = fioflck(f1->handle);
		if (retcode) {
			putfilename(f1->handle);
			return retcode - 1730;
		}
		rioeofpos(f1->handle, &pos);
		riosetpos(f1->handle, pos);
	}	
	retcode = rioput(f1->handle,  rec, reclen);
	if (retcode < 0) {
		putfilename(f1->handle);
		if (retcode == ERR_WRERR) retcode = -706;
		else retcode -= 1730;
	}
	else {
		f1->flags |= FLAGS_UPDATE;
		retcode = 0;
	}
	if (f1->options & FILEOPTIONS_LOCKAUTO) riounlock(f1->handle, pos);
	if (recnum == -3) fiofulk(f1->handle);
	return retcode;
}

/**
 * write through index
 */
INT filewritekey(INT connectid, INT fileid, UCHAR *key, INT keylen, UCHAR *rec, INT reclen)
{
	INT retcode;
	OFFSET pos;
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	if (f1->type == FILEOPTIONS_RAWOPEN) return ERR_BADFUNC;
	f1->flags &= ~FLAGS_UPDATE;
	if (fileinfo[fileid].type == FILEOPTIONS_IFILEOPEN) {
		if (keylen <= 0) {
			if (!keylen) {
				putfilename(f1->xhandle);
				return -708;
			}
			return ERR_INVALIDSIZESPEC;
		}
		retcode = fioflck(f1->handle);
		if (retcode) {
			putfilename(f1->handle);
			return retcode - 1730;
		}
		if (!(f1->options & (FILEOPTIONS_VARIABLE | FILEOPTIONS_COMPRESSED)) && !xiogetrec(f1->xhandle)) pos = filepos;
		else rioeofpos(f1->handle, &pos);
		filepos = pos;
		retcode = xioinsert(f1->xhandle,  key, keylen);
		if (retcode) {
			putfilename(f1->xhandle);
			if (retcode == 1) retcode = -709;
			else if (retcode == 2) retcode = -719;
			else if (retcode == ERR_BADKY) retcode = -721;
			else retcode -= 1730;
		}
	}
	else if (fileinfo[fileid].type == FILEOPTIONS_AFILEOPEN) {
		retcode = fioflck(f1->handle);
		if (retcode) {
			putfilename(f1->handle);
			return retcode - 1730;
		}
		if (!(f1->options & (FILEOPTIONS_VARIABLE | FILEOPTIONS_COMPRESSED)) && (f1->flags & FLAGS_AIMFIXED) && !aiogetrec(f1->xhandle)) {
			pos = filepos * (f1->reclen + f1->eorsize);
		}
		else {
			rioeofpos(f1->handle, &pos);
			if (f1->flags & FLAGS_AIMFIXED) filepos = pos / (f1->reclen + f1->eorsize);
			else if (pos) filepos = (pos - 1) / 256;
			else filepos = 0;
		}
		retcode = aioinsert(f1->xhandle,  rec);
		if (retcode) {
			putfilename(f1->xhandle);
			if (retcode == ERR_WRERR) retcode = -706;
			else retcode -= 1730;
		}
	}
	else return ERR_BADFUNC;
	if (!retcode) {
		riosetpos(f1->handle, pos);
		retcode = rioput(f1->handle,  rec, reclen);
		if (retcode < 0) {
			putfilename(f1->handle);
			if (retcode == ERR_WRERR) retcode = -706;
			else retcode -= 1730;
		}
		else {
			f1->flags |= FLAGS_UPDATE;
			retcode = 0;
		}
	}
	fiofulk(f1->handle);
	return retcode;
}

/* write directly to the file */
INT filewriteraw(INT connectid, INT fileid, OFFSET pos, UCHAR *rec, INT reclen)
{
	INT retcode;
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	if (f1->type != FILEOPTIONS_RAWOPEN) return ERR_BADFUNC;
	retcode = fiowrite(f1->handle, pos,  rec, reclen);
	if (retcode > 0) retcode = 0;
	else putfilename(f1->handle);
	return retcode;
}

/* insert key */
INT fileinsert(INT connectid, INT fileid, UCHAR *buf, INT buflen)
{
	INT retcode;
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	if (f1->type == FILEOPTIONS_IFILEOPEN) {
		if (buflen <= 0) {
			if (!buflen) {
				putfilename(f1->xhandle);
				return -708;
			}
			return ERR_INVALIDSIZESPEC;
		}
		retcode = fioflck(f1->handle);
		if (retcode) {
			putfilename(f1->handle);
			return retcode - 1730;
		}
		riogrplpos(f1->handle, &filepos);
		retcode = xioinsert(f1->xhandle,  buf, buflen);
		if (retcode) {
			putfilename(f1->xhandle);
			if (retcode == 1) retcode = -709;
			else if (retcode == 2) retcode = -719;
			else if (retcode == ERR_BADKY) retcode = -721;
			else retcode -= 1730;
		}
	}
	else if (fileinfo[fileid].type == FILEOPTIONS_AFILEOPEN) {
		retcode = fioflck(f1->handle);
		if (retcode) {
			putfilename(f1->handle);
			return retcode - 1730;
		}
		riogrplpos(f1->handle, &filepos);
		if (f1->flags & FLAGS_AIMFIXED) filepos /= (f1->reclen + f1->eorsize);
		else if (filepos) filepos = (filepos - 1) / 256;
		retcode = aioinsert(f1->xhandle,  buf);
		if (retcode) {
			putfilename(f1->xhandle);
			if (retcode == ERR_WRERR) retcode = -706;
			else retcode -= 1730;
		}
	}
	else return ERR_BADFUNC;
	fiofulk(f1->handle);
	return retcode;
}

/* update */
INT fileupdate(INT connectid, INT fileid, UCHAR *rec, INT reclen)
{
	INT retcode;
	OFFSET pos;
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	if (f1->type == FILEOPTIONS_RAWOPEN) return ERR_BADFUNC;
	if (!(f1->flags & FLAGS_UPDATE)) {
		putfilename(f1->handle);
		return -710;
	}
	if (f1->options & FILEOPTIONS_COMPRESSED) {
		putfilename(f1->handle);
		return -711;
	}
	f1->flags &= ~FLAGS_UPDATE;
	riolastpos(f1->handle, &pos);
	riosetpos(f1->handle, pos);
	retcode = rioput(f1->handle,  rec, reclen);
	if (retcode) {
		putfilename(f1->handle);
		retcode -= 1730;
	}
	else f1->flags |= FLAGS_UPDATE;
	return retcode;
}

/* delete */
INT filedelete(INT connectid, INT fileid, UCHAR *key, INT keylen)
{
	INT retcode;
	OFFSET pos;
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	if (f1->type == FILEOPTIONS_RAWOPEN) return ERR_BADFUNC;
	if (keylen < 0) return ERR_INVALIDSIZESPEC;
	if (fileinfo[fileid].type == FILEOPTIONS_IFILEOPEN) {
		retcode = fioflck(f1->handle);
		if (retcode) {
			putfilename(f1->handle);
			return retcode - 1730;
		}
		retcode = xiodelete(f1->xhandle, key, keylen, FALSE);
		if (retcode != 0) {
			if (retcode != 1) {
				putfilename(f1->xhandle);
				if (retcode < 0) {
					if (retcode == ERR_BADKY) retcode = -721;
					else retcode -= 1730;
				}
				else retcode = -717;
			}
		}
		else {
			riosetpos(f1->handle, filepos);
			retcode = riodelete(f1->handle, f1->reclen);
			riounlock(f1->handle, filepos);
			if (retcode) {
				if (retcode == ERR_ISDEL) retcode = 0;
				else {
					putfilename(f1->handle);
					retcode -= 1730;
				}
			}
			else if (!(f1->options & (FILEOPTIONS_VARIABLE | FILEOPTIONS_COMPRESSED))) xiodelrec(f1->xhandle);
		}
	}
	else {
		if (!(f1->flags & FLAGS_UPDATE)) return 0;
		retcode = fioflck(f1->handle);
		if (retcode) return retcode - 1730;
		riolastpos(f1->handle, &pos);
		riosetpos(f1->handle, pos);
		retcode = riodelete(f1->handle, f1->reclen);
		riounlock(f1->handle, pos);
		if (retcode) {
			if (retcode == ERR_ISDEL) retcode = 0;
			else {
				putfilename(f1->handle);
				retcode -= 1730;
			}
		}
		else if (f1->type == FILEOPTIONS_AFILEOPEN
				&& !(f1->options & (FILEOPTIONS_VARIABLE | FILEOPTIONS_COMPRESSED))
				&& (f1->flags & FLAGS_AIMFIXED))
		{
			filepos = pos / (f1->reclen + f1->eorsize);
			aiodelrec(f1->xhandle);
		}
	}
	fiofulk(f1->handle);
	return retcode;
}

/* delete key */
INT filedeletek(INT connectid, INT fileid, UCHAR *key, INT keylen)
{
	INT retcode;
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	if (f1->type != FILEOPTIONS_IFILEOPEN) return ERR_BADFUNC;
	if (keylen < 0) return ERR_INVALIDSIZESPEC;
	retcode = fioflck(f1->handle);
	if (retcode) return retcode - 1730;
	retcode = xiodelete(f1->xhandle, key, keylen, FALSE);
	if (retcode != 0) {
		if (retcode != 1) {
			putfilename(f1->xhandle);
			if (retcode < 0) {
				if (retcode == ERR_BADKY) retcode = -721;
				else retcode -= 1730;
			}
			else retcode = -717;
		}
	}
	fiofulk(f1->handle);
	return retcode;
}

/* unlock one record */
INT fileunlockrec(INT connectid, INT fileid, OFFSET pos)
{
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	if (f1->type == FILEOPTIONS_RAWOPEN) return ERR_BADFUNC;
	riounlock(f1->handle, pos);
	return 0;
}

/* unlock all records */
INT fileunlockall(INT connectid, INT fileid)
{
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	if (f1->type == FILEOPTIONS_RAWOPEN) return ERR_BADFUNC;
	riounlock(f1->handle, -1L);
	return 0;
}

/* compare 2 files */
INT filecompare(INT connectid, INT fileid1, INT fileid2)
{
	INT i1;
	CHAR *ptr1, *ptr2;
	FILEINFO *f1, *f2;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid1 -= FILEBASE;
	if (fileid1 < 0 || fileid1 >= openhi || !fileinfo[fileid1].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid1];
	fileid2 -= FILEBASE;
	if (fileid2 < 0 || fileid2 >= openhi || !fileinfo[fileid2].type) return ERR_BADFILEID;
	f2 = &fileinfo[fileid2];
	ptr1 = fioname(f1->handle);
	ptr2 = fioname(f2->handle);
	if (ptr1 == NULL || ptr2 == NULL) return -1730 + ERR_NOTOP;
/*** CODE: FOR UNIX MAYBE USE STAT() TO GET INODE VALUE ***/
	i1 = strcmp(ptr1, ptr2);
	if (i1 < 0) return -1;
	if (i1 > 0) return 1;
	return 0;
}

/* lock file */
INT filelock(INT connectid, INT fileid)
{
	INT retcode, array[2];
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	array[0] = f1->handle;
	array[1] = 0;
	retcode = fiolock(array);
	if (retcode) {
		putfilename(f1->handle);
		retcode -= 1730;
	}
	return retcode;
}

/* unlock file */
INT fileunlock(INT connectid, INT fileid)
{
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	fiounlock(f1->handle);
	return 0;
}

/* write end of file */
INT fileweof(INT connectid, INT fileid, OFFSET pos)
{
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	if (f1->type == FILEOPTIONS_RAWOPEN) return ERR_BADFUNC;
	if (pos >= 0) {
		pos = pos * (f1->reclen + f1->eorsize);
		riosetpos(f1->handle, pos);
	}
	rioweof(f1->handle);
	return 0;
}

/* size */
INT filesize(INT connectid, INT fileid, OFFSET *filepos)
{
	INT retcode;
	OFFSET pos;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	retcode = fiogetsize(fileinfo[fileid].handle, &pos);
	if (retcode < 0) {
		putfilename(fileinfo[fileid].handle);
		return retcode - 1730;
	}
	*filepos = pos;
	return 0;
}

/* rename */
INT filerename(INT connectid, INT fileid, UCHAR *filename, INT filenamelen)
{
	INT retcode;
	CHAR newname[MAX_NAMESIZE];
	FILEINFO *f1;

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	fileid -= FILEBASE;
	if (fileid < 0 || fileid >= openhi || !fileinfo[fileid].type) return ERR_BADFILEID;
	f1 = &fileinfo[fileid];
	if (f1->type != FILEOPTIONS_RAWOPEN) return ERR_BADFUNC;
	if (filenamelen <= 0) return ERR_INVALIDVALUE;
	strncpy(newname, (CHAR *) filename, filenamelen);
	newname[filenamelen] = '\0';
	miofixname(newname, ".txt", FIXNAME_EXT_ADD);
	retcode = fiorename(f1->handle, newname);
	if (retcode) {
		if (retcode == ERR_FNOTF) retcode = -601;
		else if (retcode == ERR_NOACC) retcode = -605;
		else retcode = 1630 - retcode;
		strncpy(errormsg, newname, sizeof(errormsg) - 1);
		errormsg[sizeof(errormsg) - 1] = '\0';
	}
	f1->type = f1->handle = 0;
	if (fileid == openhi - 1) openhi--;
	return retcode;
}

/* execute command */
INT filecommand(INT connectid, UCHAR *cmd, INT cmdlen)
{
	INT i1, nextoffset, offset, type;
	CHAR work[2048];

	errormsg[0] = '\0';
	if (connectid != cid) return ERR_BADCONNECTID;
	offset = 0;
	i1 = tcpnextdata((UCHAR *) cmd, cmdlen, &offset, &nextoffset);
	if (i1 <= 0) return ERR_BADFUNC;
	memcpy(work, cmd + offset, i1);
	work[i1] = '\0';
	if (!strcmp(work, "AIMDEX")) type = UTIL_AIMDEX;
	else if (!strcmp(work, "BUILD")) type = UTIL_BUILD;
	else if (!strcmp(work, "COPY")) type = UTIL_COPY;
	else if (!strcmp(work, "CREATE")) type = UTIL_CREATE;
	else if (!strcmp(work, "ENCODE")) type = UTIL_ENCODE;
	else if (!strcmp(work, "ERASE")) type = UTIL_ERASE;
	else if (!strcmp(work, "EXIST")) type = UTIL_EXIST;
	else if (!strcmp(work, "INDEX")) type = UTIL_INDEX;
	else if (!strcmp(work, "REFORMAT")) type = UTIL_REFORMAT;
	else if (!strcmp(work, "RENAME")) type = UTIL_RENAME;
	else if (!strcmp(work, "SORT")) type = UTIL_SORT;
/*** CODE: CREATE MORE MEANIFUL ERROR ***/
	else return ERR_BADFUNC;
/*** CODE: CREATE MORE MEANIFUL ERROR ***/
	if (nextoffset == cmdlen) return ERR_BADFUNC;

	if (type == UTIL_INDEX || type == UTIL_SORT) {
		/* append -w to override any passed in values */
		cmdlen -= nextoffset;
		memcpy(work, cmd + nextoffset, cmdlen);
		if (workdir[0]) {
			strcpy(work + cmdlen, " -w=");
			cmdlen += 4;
			strcpy(work + cmdlen, workdir);
			cmdlen += (INT)strlen(workdir);
		}
		else {
			strcpy(work + cmdlen, " -w=.");
			cmdlen += 5;
		}
		cmd = (UCHAR *) work;
		nextoffset = 0;
	}
	i1 = utility(type, (CHAR *)(cmd + nextoffset), cmdlen - nextoffset);
	if (i1) {
		utilgeterrornum(i1, errormsg, sizeof(errormsg));
		i1 = (INT)strlen(errormsg);
		if ((size_t) (i1 + 2) < sizeof(errormsg)) {
			utilgeterror(errormsg + i1 + 2, sizeof(errormsg) - (i1 + 2));
			if (errormsg[i1 + 2]) {
				errormsg[i1] = ',';
				errormsg[i1 + 1] = ' ';
			}
		}
		return -1;
	}
	return 0;
}

/* return the connection error message */
CHAR *filemsg()
{
	return errormsg;
}

static void putfilename(INT filenum)
{
	CHAR *ptr;

	ptr = fioname(filenum);
	if (ptr != NULL) {
		strncpy(errormsg, ptr, sizeof(errormsg) - 1);
		errormsg[sizeof(errormsg) - 1] = '\0';
	}
}
