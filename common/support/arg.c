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
#include "arg.h"
#include "fio.h"
#include "base.h"

static int argcnt;
static char **argvar = NULL;
static int argnext;

void arginit(int argc, char **argv, int *displayflag)
{
	if (displayflag != NULL) *displayflag = TRUE;
	if (argc > 1 && argv[argc - 1][0] == '-' && argv[argc - 1][1] == '-' && !argv[argc - 1][2]) {
		argc--;
		if (displayflag != NULL) *displayflag = FALSE;
	}
	if (argc > 1 && argv[argc - 1][0] == '-' && argv[argc - 1][1] == '+' && !argv[argc - 1][2]) {
		argc--;
		if (displayflag != NULL) *displayflag = TRUE;
	}

	argcnt = argc;
	argvar = argv;
	argnext = 1;
}

int argget(int flags, char *buf, int size)
{
	static int optnext;
	static char **optbuf = NULL;
	int i1, i2, i3, i4, handle, quoteflag;
	OFFSET offset;
	char *argptr, *bufptr, work[MAX_NAMESIZE];
	unsigned char c1;

	if (argvar == NULL || !(flags & (ARG_FIRST | ARG_NEXT | ARG_PROGRAM))) return ERR_INVAR;

	if (flags & ARG_PROGRAM) argptr = argvar[0];
	else {
		if (flags & ARG_FIRST) {
			argnext = 1;
			if (optbuf != NULL) {
				memfree((unsigned char **) optbuf);
				optbuf = NULL;
			}
		}
		for ( ; ; ) {
			if (optbuf != NULL) {
				argptr = *optbuf + optnext;
				if (!*argptr) {
					memfree((unsigned char **) optbuf);
					optbuf = NULL;
					continue;
				}
				optnext += (INT)(strlen(argptr) + 1);
			}
			else {
				if (argnext >= argcnt) return 1;
				argptr = argvar[argnext++];
				if (argptr[0] == '-' && toupper(argptr[1]) == 'O' && argptr[2] == '=' && argptr[3]) {
					if (flags & ARG_IGNOREOPT) continue;
					/* open file through fio to use search path */
					strcpy(work, &argptr[3]);
					miofixname(work, ".txt", FIXNAME_EXT_ADD);
					handle = fioopen(work, FIO_M_SRO | FIO_P_TXT);
					if (handle < 0) return ERR_NOOPT;
					fiogetsize(handle, &offset);
					i1 = (int) offset;
					optbuf = (char **) memalloc(i1 + 1, 0);
					if (optbuf == NULL) return ERR_NOMEM;
					i2 = fioread(handle, 0, (unsigned char *) *optbuf, i1);
					fioclose(handle);
					if (i2 != i1) {
						memfree((unsigned char **) optbuf);
						optbuf = NULL;
						return ERR_RDOPT;
					}
		
					/* clean-up options file */
					for (i2 = i3 = i4 = 0, bufptr = *optbuf, quoteflag = FALSE; i2 < i1; ) {
						c1 = (unsigned char) bufptr[i2++];
						if (c1 == '"') {
							quoteflag = !quoteflag;
							continue;
						}
						if (c1 == '\\' && bufptr[i2] == '"') c1 = (unsigned char) bufptr[i2++];
						else if (c1 == 0x0D || c1 == 0x0A || c1 == 0x1A || c1 == 0xFA || c1 == 0xFB || (!quoteflag && isspace(c1))) {
							if (i4 != i3) {
								bufptr[i4++] = '\0';
								i3 = i4;
							}
							quoteflag = FALSE;
							continue;
						}
						bufptr[i4++] = (char) c1;
					}
					if (i4 != i3) bufptr[i4++] = '\0';
					bufptr[i4] = '\0';
					optnext = 0;
					continue;
				}
			}
			break;
		}
	}

	i1 = (INT)(strlen(argptr) + 1);
	if (i1 > size) return ERR_SHORT;
	memcpy(buf, argptr, i1);
	return 0;
}
