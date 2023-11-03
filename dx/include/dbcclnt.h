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

#ifndef _DBCCLNT_INCLUDED
#define _DBCCLNT_INCLUDED

#include "xml.h"

#define CLIENTSEND_EVENT	0x01
#define CLIENTSEND_WAIT		0x02
#define CLIENTSEND_SERIAL	0x04
#define CLIENTSEND_RELEASE	0x08
#define CLIENTSEND_CONTWAIT	0x10

#define CLIENTVIDPUT_SAVE	0x01
#define CLIENTVIDPUT_SEND	0x02

#define CLIENTKEYIN_NUM		0x01
#define CLIENTKEYIN_NUMRGT	0x02
#define CLIENTKEYIN_RV		0x04

#define CLIENT_VID_WIN		1
#define CLIENT_VID_SCRN		2
#define CLIENT_VID_CHAR		3
#define CLIENT_VID_STATE	4

extern void checkClientFileTransferOptions();
extern void clientcancelputs(void);
extern INT clientclearendkey(UCHAR *);
extern void clientClockVersion(CHAR *work, size_t cbWork);
extern INT clientevents(INT, INT, void (*)(ELEMENT *));
extern void clientFileTransfer(UCHAR vx, UCHAR *adr1, UCHAR *adr2);
extern INT clientgeterror(CHAR*, INT);
extern INT clientkeyin(UCHAR*, INT, INT *, INT, INT);
extern INT clientstart(CHAR*, INT, INT, INT, INT, FILE*, CHAR*);
extern void clientend(CHAR*);

extern void clientput(CHAR *, INT);
extern void clientputdata(CHAR *, INT);
extern ELEMENT *clientelement(void);
extern void clientputnum(INT);
extern INT clientsend(INT, INT);
extern void clientrelease(void);

extern INT clientsendgreeting(void);
extern INT clientvidinit(void);
extern void clientstatesave(UCHAR *);
extern void clientstaterestore(UCHAR *);
extern INT clientvidput(INT32 *, INT);
extern void clientvidreset(void);

extern INT clientsetendkey(UCHAR *);
extern INT clienttrapstart(UCHAR *, INT);
extern INT clienttrapget(INT *);
extern INT clienttrappeek(INT *);
extern INT clientvidsave(INT, UCHAR *, INT *);
extern INT clientvidrestore(INT, UCHAR *, INT);
extern INT clientvidsize(INT);
extern void clientvidgetpos(INT *, INT *);
extern void clientvidgetwin(INT *, INT *, INT *, INT *);
extern void setClientrollout(INT);

#endif  /* _DBCCLNT_INCLUDED */
