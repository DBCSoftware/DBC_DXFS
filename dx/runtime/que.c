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
#define INC_STRING
#define INC_TIME
#include "includes.h"
#include "base.h"
#include "que.h"
#include "evt.h"

typedef struct queuestruct {
	QUEHANDLE nextquehandle;  /* pointer to next queuestruct */
	UCHAR **qentries;	/* pointer to moveable memory of queue entries array */
	INT qhead;		/* queue array header index */
	INT qcount;		/* queue entries count */
	INT entrysize;		/* maximum length of each entry in queue */
	INT entrycount;	/* maximum number of entries in queue */
	INT eventid;		/* associated event or 0 */
	INT qlengths[1];	/* queue entries lengths */
} QUEUE;

static QUEHANDLE quehandlehdr = NULL;

INT quecreate(INT entries, INT size, INT eventid, QUEHANDLE *quehandleptr)
{
	QUEHANDLE qh;
	QUEUE *que;
	UCHAR **msgs;

	if (entries < 1 || size < 1) return RC_INVALID_PARM;  /* invalid parameters */

	*quehandleptr = NULL;
	qh = (QUEHANDLE) memalloc(sizeof(QUEUE) + (entries - 1) * sizeof(INT), MEMFLAGS_ZEROFILL);
	if (qh == NULL) goto quecreateerr2;
	msgs = memalloc(entries * size, 0);
	if (msgs == NULL) goto quecreateerr1;

	que = *qh;
	que->qentries = msgs;
	que->entrycount = entries;
	que->entrysize = size;
	que->eventid = eventid;
	pvistart();
	que->nextquehandle = quehandlehdr;
	*quehandleptr = quehandlehdr = qh;
	pviend();
	return(0);

quecreateerr1:
	memfree((UCHAR **) qh);
quecreateerr2:
	return RC_ERROR;
}

INT quedestroy(QUEHANDLE quehandle)
{
	QUEHANDLE qh, qh1;

	pvistart();
	for (qh = quehandlehdr; qh != NULL && qh != quehandle; qh = (*qh)->nextquehandle) qh1 = qh;
	if (qh == NULL) goto quedestroyerr;
	if (qh == quehandlehdr) quehandlehdr = (*qh)->nextquehandle;
	else (*qh1)->nextquehandle = (*qh)->nextquehandle;
	memfree((UCHAR **) (*qh)->qentries);
	memfree((UCHAR **) qh);
	pviend();
	return(0);
quedestroyerr:
	pviend();
	return(RC_ERROR);
}

void quedestroyall()
{
	while (quehandlehdr != NULL) quedestroy(quehandlehdr);
}

void queempty(QUEHANDLE quehandle)
{
	QUEHANDLE qh;
	QUEUE *que;

	for (qh = quehandlehdr; qh != NULL && qh != quehandle; qh = (*qh)->nextquehandle);
	if (qh == NULL) return;
	pvistart();
	que = *qh;
	que->qhead = que->qcount = 0;
	if (que->eventid) evtclear(que->eventid);
	pviend();
}

INT quetest(QUEHANDLE quehandle)
{
	INT i1;
	QUEHANDLE qh;
	QUEUE *que;

	for (qh = quehandlehdr; qh != NULL && qh != quehandle; qh = (*qh)->nextquehandle);
	if (qh == NULL) return(RC_INVALID_HANDLE);
	pvistart();
	que = *qh;	
	i1 = (que->qcount) ? 0 : 1;
	pviend();
	return(i1);
}

INT quewait(QUEHANDLE quehandle)
{
	QUEHANDLE qh;
	QUEUE *que;

	for (qh = quehandlehdr; qh != NULL && qh != quehandle; qh = (*qh)->nextquehandle);
	if (qh == NULL) return(RC_INVALID_HANDLE);
	que = *qh;
	return(evtwait(&que->eventid, 1));
}

INT queget(QUEHANDLE quehandle, UCHAR *message, INT *length)
{
	INT i1;
	QUEHANDLE qh;
	QUEUE *que;

	for (qh = quehandlehdr; qh != NULL && qh != quehandle; qh = (*qh)->nextquehandle);
	if (qh == NULL) return(RC_INVALID_HANDLE);
	pvistart();
	que = *qh;
	if (!que->qcount) {
		pviend();
		return(1);
	}
	i1 = que->qlengths[que->qhead];
	if (i1 < *length) *length = i1;
	memcpy(message, *(que->qentries) + (que->qhead * que->entrysize), *length);
	if (++(que->qhead) == que->entrycount) que->qhead = 0;
	if (!--(que->qcount) && que->eventid) evtclear(que->eventid);
	pviend();
	return(0);
}

INT queput(QUEHANDLE quehandle, UCHAR *message, INT length)
{
	INT rc, tail;
	QUEHANDLE qh;
	QUEUE *que;

	if (!length) return(RC_INVALID_PARM);
	for (qh = quehandlehdr; qh != NULL && qh != quehandle; qh = (*qh)->nextquehandle);
	if (qh == NULL) return(RC_INVALID_HANDLE);

	pvistart();
	que = *qh;
	if (length > que->entrysize) length = que->entrysize;
	tail = que->qhead + que->qcount;
	if (tail >= que->entrycount) tail -= que->entrycount;
	memcpy(*(que->qentries) + (tail * que->entrysize), message, length);
	que->qlengths[tail] = length;
	if (que->qcount == que->entrycount) {
		if (++(que->qhead) == que->entrycount) que->qhead = 0;
		rc = 1;
	}
	else {
		que->qcount++;
		rc = 0;
	}
	if (que->eventid) evtset(que->eventid);
	pviend();
	return(rc);
}

INT queputfirst(QUEHANDLE quehandle, UCHAR *message, INT length)
{
	INT rc;
	QUEHANDLE qh;
	QUEUE *que;

	if (!length) return(RC_INVALID_PARM);
	for (qh = quehandlehdr; qh != NULL && qh != quehandle; qh = (*qh)->nextquehandle);
	if (qh == NULL) return(RC_INVALID_HANDLE);

	pvistart();
	que = *qh;
	if (length > que->entrysize) length = que->entrysize;
	if (que->qcount == que->entrycount) rc = 1;
	else {
		que->qcount++;
		if ((que->qhead)-- == 0) que->qhead = que->entrycount - 1;
		rc = 0;
	}
	memcpy(*(que->qentries) + (que->qhead * que->entrysize), message, length);
	que->qlengths[que->qhead] = length;
	if (que->eventid) evtset(que->eventid);
	pviend();
	return(rc);
}
