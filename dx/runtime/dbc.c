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
#include "dbc.h"
#include "base.h"
#if OS_WIN32
#include "svc.h"
#include <VersionHelpers.h>
#endif

static INT mainargc;
static CHAR **mainargv, **mainenvp;

#if OS_WIN32
//static OSVERSIONINFO osInfo;
#define MAXARGS 50
static SVCINFO serviceinfo = {
	"DbcdxService", "DB/C DX Service"
};
#endif

static int dbcdxstart(int agrc, char **argv);
#if OS_WIN32
static void dbcdxstop(void);
#endif


INT main(INT argc, CHAR **argv, CHAR **envp)
{
	dbcflags |= DBCFLAG_CONSOLE;
	return mainentry(argc, argv, envp);
}

INT mainentry(INT argc, CHAR **argv, CHAR **envp)
{
#if OS_WIN32
	INT i1, i2, i3, svcflags;
	CHAR work[16], *password, *user, *argvwork[MAXARGS];

#endif

	/*
	 * Build this very early in case the user clicks the tray icon very quickly
	 */
	buildDXAboutInfoString();

	mainargc = argc;
	mainargv = argv;
	mainenvp = envp;

#if OS_WIN32
#define SVCFLAG_INSTALL		0x01
#define SVCFLAG_UNINSTALL	0x02
#define SVCFLAG_START		0x04
#define SVCFLAG_STOP		0x08
#define SVCFLAG_VERBOSE		0x10
//	osInfo.dwOSVersionInfoSize = sizeof(osInfo);
//	if (!GetVersionEx(&osInfo)) {
//		badanswer(GetLastError());
//	}
	/**
	 * If the OS is WinXP or newer, and this is DBCC
	 *
	 * Don't remove this! It is no longer documented but some users may be using this.
	 * It's to run dbcc in the background all the time by itself. Has nothing to do with DBCD
	 */
	if (IsWindowsXPOrGreater() && (dbcflags & DBCFLAG_CONSOLE))
	//if (osInfo.dwPlatformId == VER_PLATFORM_WIN32_NT && (dbcflags & DBCFLAG_CONSOLE))
	{
		svcflags = 0;
		password = user = NULL;
		for (i1 = 0; ++i1 < argc; ) {
			if (argv[i1][0] == '-') {
				for (i2 = 0; argv[i1][i2 + 1] && argv[i1][i2 + 1] != '=' && i2 < (INT) sizeof(work) - 1; i2++)
					work[i2] = (CHAR) toupper(argv[i1][i2 + 1]);
				work[i2] = '\0';
				if (!strcmp(work, "DISPLAY")) {
					if (argv[i1][i2 + 1] == '=' && argv[i1][i2 + 2]) serviceinfo.servicedisplayname = argv[i1] + i2 + 2;
				}
				else if (!strcmp(work, "INSTALL")) svcflags |= SVCFLAG_INSTALL;
				else if (!strcmp(work, "PASSWORD")) {
					if (argv[i1][i2 + 1] == '=') password = argv[i1] + i2 + 2;
				}
				else if (!strcmp(work, "SERVICE")) {
					if (argv[i1][i2 + 1] == '=' && argv[i1][i2 + 2]) serviceinfo.servicename = argv[i1] + i2 + 2;
				}
				else if (!strcmp(work, "START")) svcflags |= SVCFLAG_START;
				else if (!strcmp(work, "STOP")) svcflags |= SVCFLAG_STOP;
				else if (!strcmp(work, "UNINSTALL")) svcflags |= SVCFLAG_UNINSTALL;
				else if (!strcmp(work, "USER")) {
					if (argv[i1][i2 + 1] == '=') user = argv[i1] + i2 + 2;
				}
				else if (!strcmp(work, "VERBOSE")) svcflags |= SVCFLAG_VERBOSE;
			}
		}
		if (svcflags & SVCFLAG_STOP) {
			i1 = svcstop(&serviceinfo);
			if (i1 == -1) fprintf(stdout, "Unable to stop %s: %s\n", serviceinfo.servicename, svcgeterror());
			else if (svcflags & SVCFLAG_VERBOSE) {
				if (i1 == 1) fprintf(stdout, "%s is not running\n", serviceinfo.servicename);
				else fprintf(stdout, "%s stopped\n", serviceinfo.servicename);
			}
		}
		if (svcflags & SVCFLAG_UNINSTALL) {
			i1 = svcremove(&serviceinfo);
			if (i1 == -1) fprintf(stdout, "Unable to remove %s: %s\n", serviceinfo.servicename, svcgeterror());
			else if (svcflags & SVCFLAG_VERBOSE) {
				if (i1 == 1) fprintf(stdout, "%s is not installed\n", serviceinfo.servicename);
				else fprintf(stdout, "%s removed\n", serviceinfo.servicename);
			}
		}
		if (svcflags & SVCFLAG_INSTALL) {
			for (i1 = i2 = 0; i1 < argc; i1++) {
				if (i1 && argv[i1][0] == '-') {
					for (i3 = 0; argv[i1][i3 + 1] && argv[i1][i3 + 1] != '=' && i3 < (INT) sizeof(work) - 1; i3++) work[i3] = (CHAR) toupper(argv[i1][i3 + 1]);
					work[i3] = 0;
					if (!strcmp(work, "INSTALL")) continue;
					else if (!strcmp(work, "PASSWORD")) continue;
					else if (!strcmp(work, "START")) continue;
					else if (!strcmp(work, "STOP")) continue;
					else if (!strcmp(work, "UNINSTALL")) continue;
					else if (!strcmp(work, "USER")) continue;
					else if (!strcmp(work, "VERBOSE")) continue;
				}
				if (i2 < MAXARGS - 1) argvwork[i2++] = argv[i1];
			}
			argvwork[i2] = NULL;
			if (svcinstall(&serviceinfo, user, password, i2, argvwork) == -1)
				fprintf(stdout, "Unable to install %s: %s\n", serviceinfo.servicename, svcgeterror());
			else if (svcflags & SVCFLAG_VERBOSE) fprintf(stdout, "%s installed\n", serviceinfo.servicename);
		}
		if (svcflags & SVCFLAG_START) {
			i1 = svcstart(&serviceinfo);
			if (i1 == -1) fprintf(stdout, "Unable to start %s: %s\n", serviceinfo.servicename, svcgeterror());
			else if (svcflags & SVCFLAG_VERBOSE) {
				if (i1 == 1) fprintf(stdout, "%s has finished\n", serviceinfo.servicename);
				else if (svcflags & SVCFLAG_VERBOSE) fprintf(stdout, "%s started\n", serviceinfo.servicename);
			}
		}
		if (svcflags) exit(0);

		if (svcisstarting(&serviceinfo)) {
			for (i1 = i2 = 0; i1 < argc; i1++) {
				if (i1 && argv[i1][0] == '-') {
					for (i3 = 0; argv[i1][i3 + 1] && argv[i1][i3 + 1] != '=' && i3 < (INT) sizeof(work) - 1; i3++) work[i3] = (CHAR) toupper(argv[i1][i3 + 1]);
					work[i3] = 0;
					if (!strcmp(work, "DISPLAY")) continue;
					else if (!strcmp(work, "SERVICE")) continue;
				}
				if (i2 < MAXARGS - 1) argvwork[i2++] = argv[i1];
			}
			argvwork[i2] = NULL;
			if (i2 != argc) {
				mainargc = i2;
				mainargv = argvwork;  /* ok on stack, as svcrun does not return until service is done */
			}
			dbcflags |= DBCFLAG_SERVICE;
			svcrun(&serviceinfo, dbcdxstart, dbcdxstop);
			return 0;
		}
	}
#undef SVCFLAG_INSTALL
#undef SVCFLAG_UNINSTALL
#undef SVCFLAG_START
#undef SVCFLAG_STOP
#undef SVCFLAG_VERBOSE
#endif
	return dbcdxstart(0, NULL);
}

static int dbcdxstart(INT argc, CHAR **argv)
{
#if OS_WIN32
	INT i1, i2;
	CHAR *argvwork[MAXARGS];

	if (argc > 1) {  /* append additional arguments (can only happen from demand serice startup) */
		for (i1 = i2 = 0; i1 < mainargc; i1++) if (i2 < MAXARGS - 1) argvwork[i2++] = mainargv[i1];
		for (i1 = 0; ++i1 < argc; ) if (i2 < MAXARGS - 1) argvwork[i2++] = argv[i1];
		argvwork[i2] = NULL;
		mainargc = i2;
		mainargv = argvwork;
	}
#endif

	dbcinit(mainargc, mainargv, mainenvp);
	dbciex();
	return(0);
}

#if OS_WIN32
/* only called by service routines */
static void dbcdxstop()
{
	dbcshutdown();
	dbcsetevent();
	svcstatus(SVC_STOPPING, 5000 + 100);
}
#endif
