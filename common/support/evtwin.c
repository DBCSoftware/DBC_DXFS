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
#define INC_CTYPE
#define INC_STDLIB
#define INC_ERRNO
#include "includes.h"
#include "base.h"
#include "evt.h"

static HANDLE eventhandle[EVT_MAX_EVENTS];
static INT eventroof;
static INT multiplex[EVT_MAX_EVENTS];
static INT *multiplexflagptr;
static INT multiplexflagbit;
static INT critflag;
static CRITICAL_SECTION crit;
static CHAR evterrorstring[256];

INT evtcreate()
{
	INT i1;

	if (!critflag) {
		InitializeCriticalSection(&crit);
		critflag = TRUE;
	}
	EnterCriticalSection(&crit);
	for (i1 = 0; i1 < eventroof && eventhandle[i1] != NULL; i1++);
	if (i1 == EVT_MAX_EVENTS) goto evterr1;
	eventhandle[i1] = CreateEvent(NULL, /* cannot be inherited, gets a default security descriptor */
		TRUE,                           /* this is a manual reset event */
		FALSE,                          /* initial state is not signaled */
		NULL);                          /* no name */
	if (eventhandle[i1] == NULL) goto evterr2;
	if (i1 >= eventroof) eventroof = i1 + 1;
	LeaveCriticalSection(&crit);
	return EVTBASE + i1;
evterr1:
	LeaveCriticalSection(&crit);
	evtputerror("exceeding maximum number of events");
	return RC_ERROR;
evterr2:
	LeaveCriticalSection(&crit);
	evtputerror("call to CreateEvent failed");
	return RC_ERROR;
}

void evtdestroy(INT evtid)
{
	INT i1;

	i1 = evtid - EVTBASE;
	if (i1 < 0 || i1 >= EVT_MAX_EVENTS) return;
	EnterCriticalSection(&crit);
 	CloseHandle(eventhandle[i1]);
	eventhandle[i1] = NULL;
	multiplex[i1] = FALSE;
	while (eventroof-- && eventhandle[eventroof] == NULL);
	eventroof++;
	LeaveCriticalSection(&crit);
}

/**
 * Set the specified event to the nonsignaled state
 */
void evtclear(INT evtid)
{
	INT i1;

	i1 = evtid - EVTBASE;
	if (i1 < 0 || i1 >= EVT_MAX_EVENTS) return;
	EnterCriticalSection(&crit);
	ResetEvent(eventhandle[i1]);
	if (multiplex[i1] && (*multiplexflagptr & multiplexflagbit)) {
		for (i1 = 0; i1 < eventroof; i1++) {
			if (multiplex[i1] && WaitForSingleObject(eventhandle[i1], 0) == WAIT_OBJECT_0) break;
		}
		if (i1 == eventroof) *multiplexflagptr &= ~multiplexflagbit;
	}
	LeaveCriticalSection(&crit);
}

/**
 * Set the specified event to the signaled state
 */
void evtset(INT evtid)
{
	INT i1;

	i1 = evtid - EVTBASE;
	if (i1 < 0 || i1 >= EVT_MAX_EVENTS) return;
	EnterCriticalSection(&crit);
	if (multiplex[i1]) *multiplexflagptr |= multiplexflagbit;
	SetEvent(eventhandle[i1]);
	LeaveCriticalSection(&crit);
}

/**
 * Tests the specified event.
 * Return 1 if the event is signaled.
 * Return 0 if it is not signaled.
 * Return -1 if the event id is invalid
 */
INT evttest(INT evtid)
{
	INT i1;

	i1 = evtid - EVTBASE;
	if (i1 < 0 || i1 >= EVT_MAX_EVENTS || eventhandle[i1] == NULL) return -1;
	if (WaitForSingleObject(eventhandle[i1], 0) != WAIT_OBJECT_0) return 0;
	return 1;
}

/**
 * Wait for any one of the events in the array of event ids to become signaled.
 * The return value is the id of the event that became signaled.
 */
INT evtwait(INT *evtidarray, INT count)
{
	INT i1, i2, i3, workevent[EVT_MAX_EVENTS];
	HANDLE workhandle[EVT_MAX_EVENTS];

	for (i2 = i3 = 0; i2 < count; i2++) {
		if (!evtidarray[i2]) continue;
		i1 = evtidarray[i2] - EVTBASE;
		if (i1 < 0 || i1 >= EVT_MAX_EVENTS || eventhandle[i1] == NULL) return -1;
		workhandle[i3] = eventhandle[i1];
		workevent[i3++] = i2;
	}
	if (!i3) return -1;
	i1 = WaitForMultipleObjects(i3, workhandle, FALSE, INFINITE) - WAIT_OBJECT_0;
	if (i1 >= 0 && i1 < i3) return workevent[i1];
	return -1;
}

/*
 * This is called from functions in dbcctl only. It is always called with the same arguments
 * evtidarray is maineventids
 * count is maineventidcount
 * flagptr is always the field 'action' over in dbciex
 * flagbit is always ACTION_EVENT
 */
void evtmultiplex(INT *evtidarray, INT count, INT *flagptr, INT flagbit)
{
	INT i1;

	if (count < 0) return;
	EnterCriticalSection(&crit);
	for (i1 = 0; i1 < eventroof; ) multiplex[i1++] = FALSE;
	multiplexflagptr = flagptr;
	multiplexflagbit = flagbit;
	*multiplexflagptr &= ~multiplexflagbit;
	while (count--) {
		i1 = evtidarray[count] - EVTBASE;
		if (i1 < 0 || i1 >= EVT_MAX_EVENTS || eventhandle[i1] == NULL) break;
		multiplex[i1] = TRUE;
		/*
		 * WaitForSingleObject with a wait time of 0 will always return immediately.
		 * If the Event object was signaled, WAIT_OBJECT_0 will be returned.
		 * Else WAIT_TIMEOUT(258) will be returned
		 */
		if (WaitForSingleObject(eventhandle[i1], 0) == WAIT_OBJECT_0) {
			*multiplexflagptr |= multiplexflagbit;
		}
	}
	LeaveCriticalSection(&crit);
}

INT evtregister(void (*callback)(void))
{
/*** CODE GOES HERE ***/
	evtputerror("evtregister not supported");
	return -1;
}

void evtunregister(void (*callback)(void))
{
/*** CODE GOES HERE ***/
}

void evtactionflag(INT *flag, INT bit)
{
/*** CODE GOES HERE ***/
}

void evtpoll()
{
/*** CODE GOES HERE ***/
}

INT evtgeterror(CHAR *errorstr, INT size)
{
	INT length;

	length = (INT)strlen(evterrorstring);
	if (--size >= 0) {
		if (size > length) size = length;
		if (size) memcpy(errorstr, evterrorstring, size);
		errorstr[size] = 0;
		evterrorstring[0] = 0;
	}
	return length;
}

INT evtputerror(CHAR *errorstr)
{
	INT length;

	length = (INT)strlen(errorstr);
	if ((UINT)length > sizeof(evterrorstring) - 1) length = sizeof(evterrorstring) - 1;
	if (length) memcpy(evterrorstring, errorstr, length);
	evterrorstring[length] = 0;
	return length;
}
