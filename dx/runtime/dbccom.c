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
#include "includes.h"
#include "base.h"
#include "dbc.h"
#include "com.h" 
#include "evt.h"
#include "fio.h"
#include "tim.h"
#include "dbccfg.h"

#define FLAG_OPEN	0x01
#define FLAG_COMM	0x02
#define FLAG_FILL	0x04

typedef struct cominfostruct {
	INT flags;
	INT handle;
	INT sendevent;
	INT recvevent;
	INT module;
	INT offset;
	INT pgmmodule;
	INT datamodule;
	INT pcount;
} COMINFO;

static INT comhi, commax;	/* highest used & maximum size of com table */
static COMINFO **comtabptr;	/* memory pointer to com table */

#if OS_WIN32
HINSTANCE libinst;
#endif

static void fillrec(COMINFO *);
static INT comclosedavb(DAVB *);

#if OS_WIN32
static INT comload(void);
static void freecom(void);
#endif

void vcom()
{
	static UCHAR firstflg = TRUE;
	INT i1, i2, handle, listflg, mod, off, rc, refnum, recvevent, sendevent;
	INT savepcount, savepgmmodule, savedatamodule, status, statusx;
	INT flags, vx, events[EVT_MAX_EVENTS];
	UCHAR c1, *adr, *fmtadr;
	UINT bufpos, bufsize;
	DAVB *davb;
	COMINFO *com;
	
	vx = getbyte();
	if (vx != 0x07) davb = getdavb();
	switch (vx) {
		case 0x01:  /* comopen */
			if (firstflg) {
				firstflg = FALSE;
#if OS_WIN32
				rc = comload();
				if (rc) goto vcomerror;
#endif
				cominit();
			}
			getlastvar(&mod, &off);
 			cvtoname(getvar(VAR_READ));
			if (davb->refnum && (rc = comclosedavb(davb))) goto vcomerror;

			for (refnum = 0; refnum < comhi && (*comtabptr + refnum)->flags; refnum++);
			if (refnum == commax) {
				if (!commax) {
					i1 = 4;
					i2 = ((comtabptr = (COMINFO **) memalloc(4 * sizeof(COMINFO), 0)) == NULL);
				}
				else {
					i1 = commax << 1;
					i2 = memchange((UCHAR **) comtabptr, (INT) (i1 * sizeof(COMINFO)), 0);
				}
				/* reset memory */
				setpgmdata();
				if (i2) dbcerror(1630 - ERR_NOMEM);
				commax = i1;
			}

			rc = comopen(name, &handle);
			setpgmdata();
			if (rc) goto vcomerror;
			if ((sendevent = evtcreate()) == -1 || (recvevent = evtcreate()) == -1) dbcdeath(63);

			com = *comtabptr + refnum;
			memset((UCHAR *) com, 0, sizeof(COMINFO));
			com->flags = FLAG_OPEN;
			com->handle = handle;
			com->sendevent = sendevent;
			com->recvevent = recvevent;
			com->module = mod;
			com->offset = off;
			if (refnum == comhi) comhi = refnum + 1;
			(aligndavb(findvar(mod, off), NULL))->refnum = refnum + 1;
			return;

		case 0x02:  /* comclose */
			if (davb->refnum && (rc = comclosedavb(davb))) goto vcomerror;
			return;

		case 0x03:  /* comctl */
			adr = getvar(VAR_WRITE);
			cvtoname(adr);
			if (!davb->refnum) {
				for (i1 = 0; i1 < 9 && name[i1] && !isspace(name[i1]); i1++) name[i1] = (CHAR) toupper(name[i1]);
				name[i1] = 0;
				if (strcmp("GETERROR", name)) dbcerror(751);
				i1 = 0;
			}
			else i1 = (*comtabptr + davb->refnum - 1)->handle;
			lp = pl;
#if OS_WIN32
			if (!i1 && libinst == NULL) {
				strcpy(name, "DBCCOM.DLL not found or incompatible");
				i1 = (INT) strlen(name);
				if (i1 < lp) lp = i1;
				memcpy(adr + hl, name, (UINT) lp);
				rc = 0;
			}
			else
#endif
			rc = comctl(i1, (UCHAR *) name, (INT) strlen(name), &adr[hl], &lp);
			if (rc) goto vcomerror;
			if (lp) fp = 1;
			else fp = lp = 0;
			setfplp(adr);
			return;

		case 0x04:  /* comclear */
			if (!davb->refnum) dbcerror(751);
			com = *comtabptr + davb->refnum - 1;
			rc = comclear(com->handle, &status);
			if (rc) goto vcomerror;
			if (status & (COM_SEND_PEND | COM_RECV_PEND)) dbcflags |= DBCFLAG_LESS;
			else dbcflags &= ~DBCFLAG_LESS;
			return;

		case 0x05:  /* comtst */
			if (!davb->refnum) dbcerror(751);
			com = *comtabptr + davb->refnum - 1;
			rc = comstat(com->handle, &status);
			if (rc) goto vcomerror;

			if (status & COM_PER_ERROR) {
				dbcflags |= DBCFLAG_EQUAL | DBCFLAG_EOS | DBCFLAG_OVER | DBCFLAG_LESS;
				return;
			}

			/* send status */
			if (status & COM_SEND_PEND) {
				dbcflags &= ~DBCFLAG_EQUAL;
				dbcflags |= DBCFLAG_EOS;
			}
			else if (status & COM_SEND_DONE) {
				dbcflags |= DBCFLAG_EQUAL;
				dbcflags &= ~DBCFLAG_EOS;
			}
			else if (status & (COM_SEND_TIME | COM_SEND_ERROR)) dbcflags |= DBCFLAG_EQUAL | DBCFLAG_EOS;
			else dbcflags &= ~(DBCFLAG_EQUAL | DBCFLAG_EOS);

			/* receive status */
			if (status & COM_RECV_PEND) {
				dbcflags &= ~DBCFLAG_LESS;
				dbcflags |= DBCFLAG_OVER;
			}
			else if (status & (COM_RECV_DONE | COM_RECV_TIME | COM_RECV_ERROR)) {
				fillrec(com);
				if (status & COM_RECV_DONE) {
					dbcflags |= DBCFLAG_LESS;
					dbcflags &= ~DBCFLAG_OVER;
				}
				else dbcflags |= DBCFLAG_LESS | DBCFLAG_OVER;
			}
			else dbcflags &= ~(DBCFLAG_LESS | DBCFLAG_OVER);
			return;
		case 0x06:  /* comwait */
			if (!davb->refnum) dbcerror(751);
			if (fpicnt) filepi0();  /* cancel any filepi 4/13/04 jpr*/
			com = *comtabptr + davb->refnum - 1;
			rc = comstat(com->handle, &status);
			if (rc) goto vcomerror;
			if (status & (COM_RECV_DONE | COM_RECV_TIME | COM_RECV_ERROR)) {
				/* allow this to occur even if permanent error */
				fillrec(com);
				return;
			}
			if (status & (COM_SEND_DONE | COM_SEND_TIME | COM_SEND_ERROR | COM_PER_ERROR)) {
				return;
			}
			if (status & (COM_SEND_PEND | COM_RECV_PEND)) {
				i2 = 0;
				if (status & COM_SEND_PEND) events[i2++] = com->sendevent;
				if (status & COM_RECV_PEND) events[i2++] = com->recvevent;
				dbcwait(events, i2);

				if (status & COM_RECV_PEND) {
					rc = comstat(com->handle, &status);
					if (rc) goto vcomerror;
					if (status & (COM_RECV_DONE | COM_RECV_TIME | COM_RECV_ERROR)) fillrec(com);
				}
			}
			return;

		case 0x07:  /* comwait all */
			statusx = 0;
			for (i1 = i2 = 0, com = *comtabptr; i1++ < comhi; com++) {
				if (!com->flags) continue;
				rc = comstat(com->handle, &status);
				if (rc) goto vcomerror;
				if (status & (COM_RECV_DONE | COM_RECV_TIME | COM_RECV_ERROR)) fillrec(com);
				if (status & COM_SEND_PEND) events[i2++] = com->sendevent;
				if (status & COM_RECV_PEND) events[i2++] = com->recvevent;
				statusx |= status;
			}
			if (statusx & (COM_SEND_DONE | COM_SEND_TIME | COM_SEND_ERROR | COM_RECV_DONE | COM_RECV_TIME | COM_RECV_ERROR | COM_PER_ERROR)) return;
			if (statusx & (COM_SEND_PEND | COM_RECV_PEND)) {
				dbcwait(events, i2);
				if (statusx & COM_RECV_PEND) {
					for (i1 = 0, com = *comtabptr; i1++ < comhi; com++) {
						if (!com->flags) continue;
						rc = comstat(com->handle, &status);
						if (rc) goto vcomerror;
						if (status & (COM_RECV_DONE | COM_RECV_TIME | COM_RECV_ERROR)) fillrec(com);
					}
				}
			}
			return;
		case 0x08:  /* send */
			i1 = nvtoi(getvar(VAR_READ));  /* timeout value */
			bufpos = bufsize = flags = 0;
			listflg = LIST_READ | LIST_PROG | LIST_NUM1;
			for ( ; ; ) {
				if ((adr = getlist(listflg)) != NULL) {  /* it's a variable */
					if (vartype & (TYPE_LIST | TYPE_ARRAY)) {  /* list or array variable */
						listflg = (listflg & ~LIST_PROG) | LIST_LIST | LIST_ARRAY;
						continue;
					}
					recputv(adr, flags, dbcbuf, &bufpos, &bufsize, sizeof(dbcbuf), fmtadr);
					if ((listflg & LIST_PROG) && !(vartype & TYPE_LITERAL)) flags &= ~(RECFLAG_ZERO_FILL | RECFLAG_ZERO_SUPPRESS | RECFLAG_MINUS_OVERPUNCH | RECFLAG_FORMAT);
					continue;
				}
				if (!(listflg & LIST_PROG)) {
					listflg = (listflg & ~(LIST_LIST | LIST_ARRAY)) | LIST_PROG;
					flags &= ~(RECFLAG_ZERO_FILL | RECFLAG_ZERO_SUPPRESS | RECFLAG_MINUS_OVERPUNCH | RECFLAG_FORMAT);
					continue;
				}

				c1 = pgm[pcount - 1];
				if (c1 == 0xD2) flags |= RECFLAG_ZERO_FILL;
				else if (c1 == 0xF1) {
					flags |= RECFLAG_LOGICAL_LENGTH;
					flags &= ~RECFLAG_BLANK_SUPPRESS;
				}
				else if (c1 == 0xF2) flags &= ~(RECFLAG_LOGICAL_LENGTH | RECFLAG_BLANK_SUPPRESS);
				else if (c1 == 0xF3) flags |= RECFLAG_MINUS_OVERPUNCH;
				else if (c1 == 0xFA) {
					c1 = getbyte();
					if (c1 == 0x31) {
						fmtadr = getlist(listflg);
						flags |= RECFLAG_FORMAT;
					}
					else if (c1 == 0x37) {
						flags |= RECFLAG_BLANK_SUPPRESS;
						flags &= ~RECFLAG_LOGICAL_LENGTH;
					}
					else if (c1 == 0x38) flags |= RECFLAG_ZERO_SUPPRESS;
				}
				else if (c1 == 0xFB) {
					c1 = getbyte();
					if (bufpos < sizeof(dbcbuf)) {
						dbcbuf[bufpos++] = c1;
						if (bufpos > bufsize) bufsize = bufpos;
					}
				}
				else break;
			}

			if (!davb->refnum) dbcerror(751);
			com = *comtabptr + davb->refnum - 1;
			rc = comsend(com->handle, com->sendevent, i1, dbcbuf, (INT) bufsize);
			if (rc) goto vcomerror;
			return;
		case 0x09:  /* receive */
			i1 = nvtoi(getvar(VAR_READ));  /* timeout value */
			savepgmmodule = pgmmodule;
			savedatamodule = datamodule;
			savepcount = pcount;
/*** CODE: MAYBE ADD SUPPORT FOR CONTROL CODES ***/
			i2 = 0;
			while ((adr = getlist(LIST_WRITE | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1)) != NULL)
				if (vartype & (TYPE_CHAR | TYPE_NUM)) i2 += pl;
				else if (vartype & TYPE_INT) i2 += adr[1];
				else if (vartype & TYPE_FLOAT) {
					i2 += ((adr[0] << 3) & 0x18) | (adr[1] >> 5);
					if (adr[1] & 0x1F) i2 += (adr[1] & 0x1F) + 1;
				}
			if (!davb->refnum) dbcerror(751);
			com = *comtabptr + davb->refnum - 1;
			rc = comrecv(com->handle, com->recvevent, i1, i2);
			if (rc) goto vcomerror;
			com->flags |= FLAG_FILL;
			com->pgmmodule = savepgmmodule;
			com->datamodule = savedatamodule;
			com->pcount = savepcount;
			return;
		case 0x0A:  /* sendclr */
			if (!davb->refnum) dbcerror(751);
			com = *comtabptr + davb->refnum - 1;
			rc = comsclear(com->handle, &status);
			if (rc) goto vcomerror;
			if (status & COM_SEND_PEND) dbcflags |= DBCFLAG_LESS;
			else dbcflags &= ~DBCFLAG_LESS;
			return;
		case 0x0B:  /* recvclr */
			if (!davb->refnum) dbcerror(751);
			com = *comtabptr + davb->refnum - 1;
			rc = comrclear(com->handle, &status);
			if (rc) goto vcomerror;
			if (status & COM_RECV_PEND) dbcflags |= DBCFLAG_LESS;
			else dbcflags &= ~DBCFLAG_LESS;
			return;
	}
	{
		CHAR work[64];
		sprintf(work, "Bad COM opcode seen (%d)", vx);
		dbcerrinfo(505, (UCHAR*)work, -1);
	}

vcomerror:
	if (rc < 0) {
		if (rc == -1) rc = 753;
		else if (vx == 0x01) rc = 1630 - rc;
		else rc = 1730 - rc;
	}
	dbcerror(rc);
}

void comevents(INT refnum, INT *events, INT *cnt)
{
	INT i1, i2;
	COMINFO *com;

	if (!commax) {
		*cnt = 0;
		return;
	}

	i1 = refnum;
	if (i1) i1--;
	for (i2 = 0, com = *comtabptr + i1; i1++ < comhi && i2 + 2 <= *cnt; com++) {
		if (!com->flags) {
			if (refnum) break;
			continue;
		}
		events[i2++] = com->sendevent;
		events[i2++] = com->recvevent;
		if (refnum) break;
	}
	*cnt = i2;
}

/* flag an open comfile as common (don't close) */
void comflag(INT refnum)
{
	(*comtabptr + refnum - 1)->flags |= FLAG_COMM;
}

/* close com files for a given module. if mod <= 0, close all com files. */
/* if mod == 0, clear refnum in corresponding davb */
void comclosemod(INT mod)
{
	INT i1, newcomhi;
	COMINFO *com;

	if (!commax) return;

	newcomhi = 0;
	for (i1 = 0, com = *comtabptr; i1++ < comhi; com++) {
		if (!com->flags) continue;
		if (com->flags & FLAG_COMM) {  /* ignore common com files */
			com->flags &= ~FLAG_COMM;
			newcomhi = i1;
			continue;
		}
		if (mod <= 0 || mod == com->module) {  /* close com file */
			comclose(com->handle);
			evtdestroy(com->sendevent);
			evtdestroy(com->recvevent);
			com->flags = 0;
			/* clear refnum in davb */
			if (mod == 0) (aligndavb(findvar(com->module, com->offset), NULL))->refnum = 0;
		}
		else newcomhi = i1;
	}
	comhi = newcomhi;

	if (mod == -1) comexit();
}

/* renmame old module number to new module number */
void comrenmod(INT oldmod, INT newmod)
{
	INT i1;
	COMINFO *com;

	if (!commax) return;

	for (i1 = 0, com = *comtabptr; i1++ < comhi; com++)
		if (com->flags && oldmod == com->module) com->module = newmod;
}

static INT comclosedavb(DAVB *davb)
{
	INT i1;
	COMINFO *com;

	com = *comtabptr + davb->refnum - 1;

	i1 = comclose(com->handle);
	evtdestroy(com->sendevent);
	evtdestroy(com->recvevent);
	com->flags = 0;
	if (davb->refnum == comhi) comhi--;
	davb->refnum = 0;
	return(i1);
}

static void fillrec(COMINFO *com)
{
	INT i1, errnum, bufpos, bufsize, savepcount;
	UCHAR *adr, *savepgm, *savedata;

	if (!(com->flags & FLAG_FILL)) return;
	com->flags &= ~FLAG_FILL;

	bufsize = sizeof(dbcbuf);
	i1 = comrecvget(com->handle, dbcbuf, &bufsize);
	if (i1) {
		if (i1 < 0) dbcerror(1730 - i1);
		dbcerror(i1);
	}

	savepgm = pgm;
	savedata = data;
	savepcount = pcount;
	pgm = getmpgm(com->pgmmodule);
	data = getmdata(com->datamodule);
	pcount = com->pcount;

	bufpos = errnum = 0;
/*** CODE: MAYBE ADD SUPPORT FOR CONTROL CODES ***/
	while ((adr = getlist(LIST_WRITE | LIST_PROG | LIST_LIST | LIST_ARRAY | LIST_NUM1)) != NULL)
		if ((i1 = recgetv(adr, 0, dbcbuf, &bufpos, bufsize)) && !errnum) errnum = i1;
	if (errnum) dbcerror(errnum);

	pgm = savepgm;
	data = savedata;
	pcount = savepcount;
}

#if OS_WIN32
static INT comload()
{
 	DLLSTRUCT dllinitstr;

	libinst = LoadLibrary("DBCCOM.DLL");
	if (libinst == NULL) {
		return(653);
	}
	cominit = (_COMINIT *)GetProcAddress(libinst, "cominit");
	comexit = (_COMINIT *)GetProcAddress(libinst, "comexit");
	comopen = (_COMOPEN *)GetProcAddress(libinst, "comopen");
	comclose = (_COMCLOSE *)GetProcAddress(libinst, "comclose");
	comsend = (_COMSEND *)GetProcAddress(libinst, "comsend");
	comrecv = (_COMRECV *)GetProcAddress(libinst, "comrecv");
	comrecvget = (_COMRECVGET *)GetProcAddress(libinst, "comrecvget");
	comclear = (_COMCLEAR *)GetProcAddress(libinst, "comclear");
	comrclear = (_COMRCLEAR *)GetProcAddress(libinst, "comrclear");
	comsclear = (_COMSCLEAR *)GetProcAddress(libinst, "comsclear");
	comstat = (_COMSTAT *)GetProcAddress(libinst, "comstat");
	comctl = (_COMCTL *)GetProcAddress(libinst, "comctl");
	dllinit = (_DLLINIT *)GetProcAddress(libinst, "dllinit"); 
	if (cominit == NULL || comexit == NULL || comopen == NULL || comclose == NULL ||
	    comsend == NULL || comrecv == NULL || comrecv == NULL || comclear == NULL ||
	    comrclear == NULL || comsclear == NULL || comstat == NULL || comctl == NULL ||
		dllinit == NULL) {
		FreeLibrary(libinst);
		libinst = NULL;
		return(655);
	}

	dllinitstr.evtcreate = evtcreate;
	dllinitstr.evtdestroy = evtdestroy;
	dllinitstr.evtset = evtset;
	dllinitstr.evtclear = evtclear;
	dllinitstr.evttest = evttest;
	dllinitstr.evtwait = evtwait;
	dllinitstr.evtstartcritical = pvistart;
	dllinitstr.evtendcritical = pviend;
	dllinitstr.timset = timset;
	dllinitstr.timstop = timstop;
	dllinitstr.timadd = timadd;
	dllinitstr.msctimestamp = msctimestamp;
	dllinitstr.cfggetxml = cfggetxml;
	dllinit(&dllinitstr);

	atexit(freecom);
	return(0);
}

static void freecom() {
	FreeLibrary(libinst);
}
#endif
