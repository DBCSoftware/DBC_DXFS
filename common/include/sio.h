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

typedef struct {
	INT start;
	INT end;
	UCHAR typeflg;
} SIOKEY;

#define SIO_ASCEND		0x01
#define SIO_DESCEND		0x02
#define SIO_POSITION	0x04
#define SIO_NUMERIC		0x08

#define SIOFLAGS_DSPPHASE	0x01
#define SIOFLAGS_DSPEXTRA	0x02
#define SIOFLAGS_DSPERROR	0x04

/* function prototypes */

/* sio.c */
#define siokill() sioexit()
extern INT sioinit(INT len, INT cnt, SIOKEY **keypptr, CHAR *wrkdir, CHAR *wrkname, INT size, INT flags, void (*dspcb)(CHAR *));
extern INT sioexit(void);
extern UCHAR *sioputstart(void);
extern INT sioputend(void);
extern INT sioget(UCHAR **ptr);
/*** NOTE: SIOSORT IS OBSOLETE, DO NOT USE IN NEW CODE ***/
extern INT siosort(INT, INT, SIOKEY **, INT (*)(UCHAR *), INT (*)(UCHAR *), CHAR *, CHAR *, INT, INT, void (*)(CHAR *));
extern CHAR *siogeterr(void);
