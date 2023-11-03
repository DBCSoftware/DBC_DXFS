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
#define INC_STDLIB
#define INC_CTYPE
#define INC_SIGNAL
#define INC_ERRNO
#include "includes.h"

#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>

#if !defined(SIGPOLL) & defined(SIGIO)
#define SIGPOLL SIGIO
#endif

#include "base.h"
#include "fio.h"
#include "gui.h"
#include "guix.h"
#include "dbcmd5.h"
#include "prt.h"
#include "prtx.h"
#ifdef CUPS
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <cups/ppd.h>
#endif
extern PRINTINFO **printtabptr;

static INT lcktype = PRTLOCK_FILE;

static void prtsleep(INT);
static void sigevent(INT sig);
static INT isCupsPrinterDest(CHAR *prtname);
static INT isCUPSInstalled(void);
static void buildSpoolCommand(CHAR *prtfile, PRTOPTIONS *options, INT lptype, CHAR *work);
static void buildCupsSpoolCommand(CHAR *prtfile, PRTOPTIONS *options, INT lptype, CHAR *work);

INT prtainit(PRTINITINFO *initinfo)
{
	if (initinfo->locktype) lcktype = initinfo->locktype;
	return 0;
}

INT prtaexit()
{
	lcktype = PRTLOCK_FILE;
	return 0;
}

INT prtagetpaperinfo(CHAR *prtname, CHAR **buffer, INT size, INT infoType) {
#ifdef CUPS
	INT len, i1, i2;
	int num_dests;
	cups_dest_t *dests, *dest;
	CHAR printer[MAX_NAMESIZE], *instance, *pbuffer;
	const char *filename;
	ppd_file_t *ppd;
	ppd_option_t *option = NULL;
	ppd_choice_t *choice;

	memset(*buffer, 0, size);
/* Make our own copy of the destination name */
	strcpy(printer, prtname);

/* If the destination has an instance name, it must be removed */
	if ((instance = strrchr(printer, '/')) != NULL) *instance++ = '\0';
	else instance = NULL;
	
/* Get the destination object */
	num_dests = cupsGetDests(&dests);
	if (num_dests == 0) return 0;
	dest = cupsGetDest(printer, instance, num_dests, dests);
	if (dest == NULL) {
		cupsFreeDests(num_dests, dests);
		return PRTERR_BADOPT;
	}
	
	if ((filename = cupsGetPPD(dest->name)) == NULL) {
		cupsFreeDests(num_dests, dests);
		return 0;
	}
	
	cupsFreeDests(num_dests, dests);
	if ((ppd = ppdOpenFile(filename)) == NULL) {
		unlink(filename);
		return 0;
	}
	
	if (infoType == GETPRINTINFO_PAPERBINS)
		option = ppdFindOption(ppd, "InputSlot");
	else if (infoType == GETPRINTINFO_PAPERNAMES)
		option = ppdFindOption(ppd, "PageSize");
	if (option == NULL) {
		ppdClose(ppd);
		unlink(filename);
		return 0;
	}

	pbuffer = *buffer;
	len = 0;
	for (i1 = option->num_choices, choice = option->choices; i1 > 0; i1--, choice++) {
		i2 = strlen(choice->choice) + 1;
		len += i2;
		if (len > size) break;
		memcpy(pbuffer, choice->choice, i2);
		pbuffer += i2;
	}

	ppdClose(ppd);
	unlink(filename);
#else
	**buffer = '\0';
#endif
	return 0;
}

INT prtagetprinters(CHAR **buffer, UINT size) {
#ifdef CUPS
	UINT len;
	INT i1, i2;
	int num_dests;
	cups_dest_t *dests, *pdests;
	CHAR work[256];
	CHAR *pbuffer;

	**buffer = 0;
	len = 0;
	dests = NULL;
	num_dests = cupsGetDests(&dests);
	if (dests != NULL) {
		pbuffer = *buffer;
		for (i1 = 0, pdests = dests; i1 < num_dests; i1++, pdests++) {
			if (pdests->is_default) {
				strcpy(work, pdests->name);
				if (pdests->instance != NULL && strlen(pdests->instance) > 0) {
					strcat(work, "/");
					strcat(work, pdests->instance);
				}
				i2 = strlen(work) + 1;
				len += i2;
				if (len > size) break;
				memcpy(pbuffer, work, i2);
				pbuffer += i2;
				break;
			}
		}
		for (i1 = 0, pdests = dests; i1 < num_dests; i1++, pdests++) {
			if (pdests->is_default) continue;
			strcpy(work, pdests->name);
			if (pdests->instance != NULL && strlen(pdests->instance) > 0) {
				strcat(work, "/");
				strcat(work, pdests->instance);
			}
			i2 = strlen(work) + 1;
			len += i2;
			if (len > size) break;
			memcpy(pbuffer, work, i2);
			pbuffer += i2;
		}
	}
	cupsFreeDests(num_dests, dests);
#else
	**buffer = 0;
#endif
	return 0;
}

INT prtadefname(CHAR *prtname, INT size)
{
	INT len;
	CHAR *ptr;

	ptr = "/dev/lp";
	len = strlen(ptr) + 1;
	if (size > 0) {
		if (size > len) size = len;
		memcpy(prtname, ptr, size - 1);
		prtname[size - 1] = 0;
	}
	return len;
}

/*
 * Returns PRTFLAG_TYPE_DEVICE if the name begins with /dev/
 * Returns PRTFLAG_TYPE_PIPE if the name is exactly lp or lpr
 * If the name matches exactly a CUPS destination, then return PRTFLAG_TYPE_CUPS
 * Returns PRTFLAG_TYPE_FILE if none of the above are true
 */
INT prtanametype(CHAR *prtname)
{
	INT i1, i2;
	CHAR prtwork[MAX_NAMESIZE];

	/* squeeze out blanks */
	for (i1 = i2 = 0; prtname[i1]; i1++) if (prtname[i1] != ' ') prtwork[i2++] = prtname[i1];
	if (!i2) return 0; /* If name was all blank or zero length */
	prtwork[i2] = '\0';

	if (!memcmp(prtwork, "/dev/", 5)) {
		return PRTFLAG_TYPE_DEVICE;
	}
	if (!memcmp(prtwork, "lp", 2)) {
		for (i1 = 0; isspace(prtname[i1]); i1++);
		if (!memcmp(prtname + i1, "lp", 2)) {
			i1 += 2;
			if (prtname[i1] == 'r') i1++;
			if (isspace(prtname[i1]) || !prtname[i1]) {
				return PRTFLAG_TYPE_PIPE;
			}
		}
	}
	if (isCupsPrinterDest(prtwork)) return PRTFLAG_TYPE_CUPS;
	return PRTFLAG_TYPE_FILE;
}

/**
 * If CUPS is not compiled in, return false.
 * If the config property dbcdx.print.cups=no is present, return false.
 * Else ask the CUPS system if it recognizes the destination.
 */
static INT isCupsPrinterDest(CHAR* prtname) {
	INT retval = 0;
#ifdef CUPS
	int num_dests;
	cups_dest_t *dests;
	CHAR printer[MAX_NAMESIZE], *instance;
	CHAR *ptr;

	if (!prpget("print", "cups", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "no"))
		return retval;

	if ( (num_dests = cupsGetDests(&dests)) ) {
		strcpy(printer, prtname);
		if ((instance = strchr(printer, '/')) != NULL) *instance++ = '\0';
		if (cupsGetDest(printer, instance, num_dests, dests) != NULL) retval = 1;
		cupsFreeDests(num_dests, dests);
	}
#endif
	return retval;
}

/**
 * We assume that if we are compiled with CUPS defined, then CUPS is installed.
 * Use dbcdx.print.cups=no if it really is not installed.
 */
static INT isCUPSInstalled() {
#ifdef CUPS
	INT retval = 1;
	CHAR *ptr;
	if (!prpget("print", "cups", NULL, NULL, &ptr, PRP_LOWER) && !strcmp(ptr, "no")) retval = 0;
#else
	INT retval = 0;
#endif
	return retval;
}

INT prtasubmit(CHAR *prtfile, PRTOPTIONS *options)
{
	INT i1, i2, i3, lptype;
	CHAR work[1024], *path;

	/* free several handles */
	fioclru(0);
	fioclru(0);
	fioclru(0);

/*** CODE: REPLACE WITH UNIX FUNCTION TO DETERMINE IF A BINARY IS IN THE PATH ***/
/***       IF ONE EXISTS.  ***/
	lptype = 0;
	path = getenv("PATH");
	for (i1 = i2 = 0; ; i1++) {
		if (path[i1] == ':' || !path[i1]) {
			while (isspace(path[i2])) i2++;
			for (i3 = i1; i3 > i2 && isspace(path[i3 - 1]); i3--);
			if (i2 < i3) {
				i3 -= i2;
				memcpy(work, path + i2, i3);
				if (work[i3 - 1] != '/') work[i3++] = '/';
				strcpy(work + i3, "lp");
				if (access(work, X_OK) != -1) lptype |= 0x01;
#ifndef __hpux			/* hpux does not have a *real* lpr command, it is only a link to lp */
				strcpy(work + i3, "lpr");
				if (access(work, X_OK) != -1) lptype |= 0x02;
#endif
			}
			if (!path[i1]) break;
			i2 = i1 + 1;
		}
	}
	if (!lptype) {
		prtputerror("unable to find lp or lpr");
		return PRTERR_ACCESS;
	}

	if (isCUPSInstalled()) buildCupsSpoolCommand(prtfile, options, lptype, work);
	else buildSpoolCommand(prtfile, options, lptype, work);
	
	if (system(work)) {
		memmove(work + 8, work, strlen(work) + 1);
		memcpy(work, "system(\"", 8);
		strcat(work, "\") failed");
		prtputerror(work);
		return PRTERR_NATIVE;
	}
	return 0;
}

static void buildCupsSpoolCommand(CHAR *prtfile, PRTOPTIONS *options, INT lptype, CHAR *work) {

	if (lptype & 0x02) strcpy(work, "lpr ");
	else strcpy(work, "lp ");
	if (options->copies > 1) {
		strcat(work, "-o copies=");
		mscitoa(options->copies, work + strlen(work));
		strcat(work, " ");
	}
	if (options->flags & PRT_BANR) strcat(work, "-o job-sheets=standard ");
	if (options->submit.prtname[0]) {
		if (lptype & 0x02) strcat(work, "-P ");
		else strcat(work, "-d ");
		strcat(work, options->submit.prtname);
		strcat(work, " ");
	}
	if (options->paperbin[0]) {
		strcat(work, "-o InputSlot=");
		strcat(work, options->paperbin);
		strcat(work, " ");
	}
	if (options->papersize[0]) {
		strcat(work, "-o PageSize=");
		strcat(work, options->papersize);
		strcat(work, " ");
	}
	if (options->flags & PRT_LAND) strcat(work, "-o landscape ");
	if (options->flags & PRT_DPLX) {
		if (options->flags & PRT_LAND) strcat(work, "-o sides=two-sided-short-edge ");
		else strcat(work, "-o sides=two-sided-long-edge ");
	}
	if (options->flags & PRT_REMV && lptype & 0x02) strcat(work, "-r ");
	if (options->docname[0]) {
		if (lptype & 0x02) strcat(work, "-T ");
		else strcat(work, "-t ");
		strcat(work, options->docname);
		strcat(work, " ");
	}
	strcat(work, prtfile);
}

static void buildSpoolCommand(CHAR *prtfile, PRTOPTIONS *options, INT lptype, CHAR *work) {

	if (lptype & 0x02) {			/* lpr */
		strcpy(work, "lpr -s ");	/* -s is to create symbolic link */
		if (!(options->flags & PRT_BANR)) strcat(work, "-h ");  /* default is to print banner */
		if (options->copies > 1) {
			strcat(work, "-#");
			mscitoa(options->copies, work + strlen(work));
			strcat(work, " ");
		}
		if (options->submit.prtname[0]) {
			strcat(work, "-P");
			strcat(work, options->submit.prtname);
			strcat(work, " ");
		}
		if (options->flags & PRT_LCTL) {
			if (options->flags & PRT_NOEJ) strcat(work, "-l ");
		}
		else strcat(work, "-f ");
		if (options->flags & PRT_REMV) strcat(work, "-r ");
	}
	else if (lptype & 0x01) {
		strcpy(work, "lp -s ");  /* -s is to suppress messages from print service */
		if (!(options->flags & PRT_BANR)) strcat(work, "-onobanner ");  /* default is to print banner */
		if (options->copies > 1) {
			strcat(work, "-n");
			mscitoa(options->copies, work + strlen(work));
			strcat(work, " ");
		}
		if (options->submit.prtname[0]) {
			strcat(work, "-d");
			strcat(work, options->submit.prtname);
			strcat(work, " ");
		}
		if (options->flags & PRT_NOEJ) strcat(work, "-onofilebreak ");  /* may only work with multiple files */
	}
	strcat(work, prtfile);
}

/**
 * This is used when doing direct-to-CUPS printing
 */
#ifdef CUPS
INT prtacupssubmit(CHAR *prtfile, PRTOPTIONS *options) {
	INT i1;
	int num_options, num_dests;
	char buffer[64], *instance, printer[MAX_NAMESIZE];
	cups_option_t *cups_options;
	cups_dest_t *dests, *dest;
	
	num_options = 0;
	cups_options = NULL;

/* Make our own copy of the destination name */
	strcpy(printer, options->submit.prtname);

/* If the destination has an instance name, it must be removed */
	if ((instance = strrchr(printer, '/')) != NULL) *instance++ = '\0';
	else instance = NULL;
	
/* Get the destination object */
	num_dests = cupsGetDests(&dests);
	if (num_dests == 0) return 0;
	dest = cupsGetDest(printer, instance, num_dests, dests);
	if (dest == NULL) {
		cupsFreeDests(num_dests, dests);
		return 0;	/* Printer error here? */
	}
	
/* Get all system defined options for this destination and put them into the option array */
	for (i1 = 0; i1 < dest->num_options; i1++)
		num_options = cupsAddOption(dest->options[i1].name, dest->options[i1].value, num_options, &cups_options);
	cupsFreeDests(num_dests, dests);

/* Add user requested options */
	if (options->copies > 1) {
		sprintf(buffer, "%d", options->copies);
		num_options = cupsAddOption("copies", buffer, num_options, &cups_options);
	}
	if (options->flags & PRT_BANR) {
		num_options = cupsParseOptions("job-sheets=standard", num_options, &cups_options);
	}
	if (options->flags & PRT_LAND)
		num_options = cupsAddOption("landscape", "true", num_options, &cups_options);
	if (options->flags & PRT_DPLX) {
		if (options->flags & PRT_LAND)
			num_options = cupsParseOptions("sides=two-sided-short-edge", num_options, &cups_options);
		else
			num_options = cupsParseOptions("sides=two-sided-long-edge", num_options, &cups_options);
	}
	if (options->paperbin[0])
		num_options = cupsAddOption("InputSlot", options->paperbin, num_options, &cups_options);
	if (options->papersize[0])
		num_options = cupsAddOption("PageSize", options->papersize, num_options, &cups_options);

	i1 = cupsPrintFile(printer, prtfile, options->docname, num_options, cups_options);
	cupsFreeOptions(num_options, cups_options);
	if (options->flags & PRT_REMV) fioadelete(prtfile);
	return (i1) ? 0 : cupsLastError();
}
#endif

INT prtadevopen(INT prtnum, INT alloctype)
{
	INT i1, err, handle/*, oldalarm*/, sigerr/*, slept*/;
	INT TIMEOUT = 20;
	CHAR prtname[MAX_NAMESIZE], work[100], tmpname[64];
	PRINTINFO *print;
	time_t time1;
	struct flock lckbuf;
	struct itimerval itimer, oldtimer, dummy_timer, slept_timer;
#if 1 | defined(SA_RESTART) | defined(SA_INTERRUPT) | defined(SA_RESETHAND)
	sigset_t newmask, oldmask;
	struct sigaction act, oact;
#else
	void (*osig)(INT);
#endif

	print = *printtabptr + prtnum;
	strcpy(prtname, *print->prtname);
	if (lcktype == PRTLOCK_FILE) {  /* create lock file */
		strcpy(tmpname, "/tmp/LCK..");
		strcat(tmpname, &prtname[5]);

		/* loop waiting for lock file to become available */
		for ( ; ; ) {
			do {
				handle = open(tmpname, O_RDWR | O_CREAT | O_EXCL, 0x01B6);
			} while (handle == -1 && (errno == EMFILE || errno == ENFILE) && !fioclru(0));
			if (handle != -1) break;
			if (errno != EEXIST && errno != EACCES) {
				prtaerror("temp open", errno);
				if (errno == ENOTDIR) return PRTERR_OPEN;
				return PRTERR_ACCESS;
			}
			if (!(alloctype & PRT_WAIT)) return PRTERR_INUSE;
			prtsleep(6);
		}

		/* write info to lock file */
		mscitoa(getpid(), work);
		strcat(work, "\n");
		write(handle, work, strlen(work));
		time(&time1);
		strcpy(work, ctime(&time1));
		write(handle, work, strlen(work));
		close(handle);
	}

	/* disable common signals from interrupting lock */
#if 1 | defined(SA_RESTART) | defined(SA_INTERRUPT) | defined(SA_RESETHAND)
	sigemptyset(&newmask);
	sigaddset(&newmask, SIGUSR1);
	sigaddset(&newmask, SIGUSR2);
#ifdef SIGPOLL
	sigaddset(&newmask, SIGPOLL);
#endif
#ifdef SIGTSTP
	sigaddset(&newmask, SIGTSTP);
#endif
	sigprocmask(SIG_BLOCK, &newmask, &oldmask);
#else
	sighold(SIGUSR1);
	sighold(SIGUSR2);
#ifdef SIGPOLL
	sighold(SIGPOLL);
#endif
#ifdef SIGTSTP
	sighold(SIGTSTP);
#endif
#endif
	/*oldalarm = alarm(0);*/
	timerclear(&itimer.it_value);
	timerclear(&itimer.it_interval);
	setitimer(ITIMER_REAL, &itimer, &oldtimer);
#if 1 | defined(SA_RESTART) | defined(SA_INTERRUPT) | defined(SA_RESETHAND)
	act.sa_handler = sigevent;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
#ifdef SA_INTERRUPT
	act.sa_flags |= SA_INTERRUPT;
#endif
	sigerr = sigaction(SIGALRM, &act, &oact);
#else
	sigerr = ((osig = sigset(SIGALRM, sigevent)) == SIG_ERR) ? -1 : 0;
#endif
	if (sigerr != -1) {
		/*alarm(TIMEOUT);*/
		timerclear(&itimer.it_value);
		timerclear(&itimer.it_interval);
		itimer.it_value.tv_sec = TIMEOUT;
		setitimer(ITIMER_REAL, &itimer, &dummy_timer);
	}

	/* open device */
	do handle = open(prtname, O_WRONLY);
	while (handle == -1 && (errno == EMFILE || errno == ENFILE) && !fioclru(0));
	err = errno;

	if (sigerr != -1) {
		/*slept = TIMEOUT - alarm(0);*/
		timerclear(&itimer.it_value);
		timerclear(&itimer.it_interval);
		setitimer(ITIMER_REAL, &itimer, &slept_timer);
		slept_timer.it_value.tv_sec = TIMEOUT - slept_timer.it_value.tv_sec;
#if defined(SA_RESTART) | defined(SA_INTERRUPT) | defined(SA_RESETHAND)
		sigaction(SIGALRM, &oact, NULL);
#else
		sigset(SIGALRM, osig);
#endif
	}
	else {
		/*slept = 0;*/
		slept_timer.it_value.tv_sec = 0;
	}
	if (/*oldalarm > 0*/ oldtimer.it_value.tv_sec > 0) {
		/*
		 * oldalarm = (oldalarm > slept) ? oldalarm - slept : 1;
		 * alarm(oldalarm);
		 */
		if (oldtimer.it_value.tv_sec > slept_timer.it_value.tv_sec) oldtimer.it_value.tv_sec = oldtimer.it_value.tv_sec - slept_timer.it_value.tv_sec;
		else oldtimer.it_value.tv_sec = 1;
		setitimer(ITIMER_REAL, &oldtimer, &dummy_timer);
	}
	/* enable common signals */
#if defined(SA_RESTART) | defined(SA_INTERRUPT) | defined(SA_RESETHAND)
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
#else
	sigrelse(SIGUSR1);
	sigrelse(SIGUSR2);
#ifdef SIGPOLL
	sigrelse(SIGPOLL);
#endif
#ifdef SIGTSTP
	sigrelse(SIGTSTP);
#endif
#endif
	if (handle == -1) {  /* open failed */
		prtaerror("open", errno);
		if (lcktype == PRTLOCK_FILE) unlink(tmpname);
		if (err == EINTR || err == EACCES) return PRTERR_ACCESS;
		return PRTERR_OPEN;
	}

	if (lcktype == PRTLOCK_FCNTL) {  /* lock device */
		lckbuf.l_type = F_WRLCK;
		lckbuf.l_whence = 0;
		lckbuf.l_start = 0;
		lckbuf.l_len = 0;
		if (!(alloctype & PRT_WAIT)) i1 = F_SETLK;
		else i1 = F_SETLKW;
		i1 = fcntl(handle, i1, &lckbuf);
		if (i1 == -1) {
			prtaerror("fcntl", errno);
			if (!(alloctype & PRT_WAIT) && errno == EACCES) i1 = PRTERR_INUSE;
			else i1 = PRTERR_OPEN;
			close(handle);
			return i1;
		}
	}
	/* set close on exec */
	i1 = fcntl(handle, F_GETFD, 0);
	if (i1 != -1) fcntl(handle, F_SETFD, i1 | FD_CLOEXEC);
	print->prthandle.handle = handle;
	return 0;
}

INT prtadevclose(INT prtnum)
{
	CHAR tmpname[18], *ptr;
	PRINTINFO *print;
	struct flock lckbuf;

	print = *printtabptr + prtnum;
	if (lcktype == PRTLOCK_FCNTL) {
		lckbuf.l_type = F_UNLCK;
		lckbuf.l_whence = 0;
		lckbuf.l_start = 0;
		lckbuf.l_len = 0;
		fcntl(print->prthandle.handle, F_SETLK, &lckbuf);
	}
	close(print->prthandle.handle);
	if (lcktype == PRTLOCK_FILE) {
		strcpy(tmpname, "/tmp/LCK..");
		ptr = *print->prtname;
		strcat(tmpname, &ptr[5]);
		unlink(tmpname);
	}
	return 0;
}

/*
 * We block the processing of SIGALRM around the call to write(...)
 * Some SCO serial device drivers can't handle it and they fail
 * with EINTR.
 */
INT prtadevwrite(INT prtnum)
{
	sigset_t set, oset;
	INT i1, i2;
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	for (i1 = 0; ; ) {
		sigemptyset(&set);
		sigaddset(&set, SIGALRM);
		sigprocmask(SIG_BLOCK, &set, &oset);
		i2 = write(print->prthandle.handle, *print->buffer + i1, print->bufcnt - i1);
		sigprocmask(SIG_SETMASK, &oset, NULL);
		if (i2 == -1) {
			prtaerror("write", errno);
			return PRTERR_WRITE;
		}
		i1 += i2;
		if (i1 >= print->bufcnt) break;
		prtsleep(6);
	}
	return 0;
}

#ifdef CUPS
INT prtacupsopen(INT prtnum, INT alloctype) {
	PRINTINFO *print;
	CHAR filename[MAX_NAMESIZE];
	INT handle;

#if CUPS_VERSION_MINOR >= 2
	cupsTempFile2(filename, sizeof(filename));
#else
	cupsTempFile(filename, sizeof(filename));
#endif
	print = *printtabptr + prtnum;
	handle = fioopen(filename, FIO_M_EXC);
	if (handle < 0) return handle;
	print->prthandle.handle = handle;
	return 0;
}

INT prtacupsclose(INT prtnum)
{
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	fioclose(print->prthandle.handle);
	return 0;
}
#endif

INT prtapipopen(INT prtnum, INT alloctype)
{
	INT i1;
	PRINTINFO *print;

	fioclru(0);  /* free a couple of handles */
	fioclru(0);
	print = *printtabptr + prtnum;
	if ((print->prthandle.stream = popen(*print->prtname, "w")) == NULL) {
		prtaerror("popen", errno);
		if (errno == ENOENT || errno == ENOTDIR) return PRTERR_OPEN;
		if (errno == EMFILE || errno == ENFILE) return PRTERR_OPEN;
		return PRTERR_ACCESS;
	}
	/* set close on exec */
	i1 = fcntl(fileno(print->prthandle.stream), F_GETFD, 0);
	if (i1 != -1) fcntl(fileno(print->prthandle.stream), F_SETFD, i1 | FD_CLOEXEC);
	return 0;
}

INT prtapipclose(INT prtnum)
{
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	pclose(print->prthandle.stream);
	return 0;
}

INT prtapipwrite(INT prtnum)
{
	INT i1;
	PRINTINFO *print;

	print = *printtabptr + prtnum;
	i1 = write(fileno(print->prthandle.stream), *print->buffer, print->bufcnt);
	if (i1 != print->bufcnt) {
		if (i1 == -1) prtaerror("write", errno);
		else prtaerror("write = 0", 0);
		return PRTERR_WRITE;
	}
	return 0;
}

void prtaerror(CHAR *function, INT err)
{
	INT i1, negflg;
	CHAR num[32], work[256];

	strcpy(work, function);
	strcat(work, " failure");
	if (err) {
		if (err < 0) {
			err = -err;
			negflg = TRUE;
		}
		else negflg = FALSE;
		num[sizeof(num) - 1] = 0;
		i1 = sizeof(num) - 1;
		do num[--i1] = (CHAR)(err % 10 + '0');
		while (err /= 10);
		if (negflg) num[--i1] = '-';
		strcat(work, ", errno = ");
		strcat(work, &num[i1]);
	}
	prtputerror(work);
}

static void prtsleep(INT seconds)
{
	/*INT oldalarm, sigerr, slept;*/
	INT sigerr;
	struct itimerval itimer, oldtimer, dummy_timer, slept_timer;
#if defined(SA_RESTART) | defined(SA_INTERRUPT) | defined(SA_RESETHAND)
	struct sigaction act, oact;
#else
	void (*osig)(INT);
#endif

	if (seconds <= 0) return;

	/*oldalarm = alarm(0);*/
	timerclear(&itimer.it_value);
	timerclear(&itimer.it_interval);
	setitimer(ITIMER_REAL, &itimer, &oldtimer);
#if defined(SA_RESTART) | defined(SA_INTERRUPT) | defined(SA_RESETHAND)
	act.sa_handler = sigevent;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
#ifdef SA_INTERRUPT
	act.sa_flags |= SA_INTERRUPT;
#endif
	sigerr = sigaction(SIGALRM, &act, &oact);
#else
	sigerr = ((osig = sigset(SIGALRM, sigevent)) == SIG_ERR) ? -1 : 0;
#endif
	if (sigerr != -1) {
		/*alarm(seconds);*/
		timerclear(&itimer.it_value);
		timerclear(&itimer.it_interval);
		itimer.it_value.tv_sec = seconds;
		setitimer(ITIMER_REAL, &itimer, &dummy_timer);

		pause();
		/*slept = seconds - alarm(0);*/
		timerclear(&itimer.it_value);
		timerclear(&itimer.it_interval);
		setitimer(ITIMER_REAL, &itimer, &slept_timer);
		slept_timer.it_value.tv_sec = seconds - slept_timer.it_value.tv_sec;
#if defined(SA_RESTART) | defined(SA_INTERRUPT) | defined(SA_RESETHAND)
		sigaction(SIGALRM, &oact, NULL);
#else
		sigset(SIGALRM, osig);
#endif
		if (/*oldalarm > 0*/ oldtimer.it_value.tv_sec > 0) {
			/**
			 * oldalarm = (oldalarm > slept) ? oldalarm - slept : 1;
			 * alarm(oldalarm);
			 */
			if (oldtimer.it_value.tv_sec > slept_timer.it_value.tv_sec) oldtimer.it_value.tv_sec = oldtimer.it_value.tv_sec - slept_timer.it_value.tv_sec;
			else oldtimer.it_value.tv_sec = 1;
			setitimer(ITIMER_REAL, &oldtimer, &dummy_timer);
		}
	}
}

static void sigevent(INT sig)
{
	switch (sig) {
		case SIGALRM:
			break;
	}
}
