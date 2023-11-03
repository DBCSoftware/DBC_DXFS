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
#define INC_LIMITS
#define INC_SIGNAL
#define INC_ERRNO
#define INC_TIME
#include "includes.h"

#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/ipc.h>
#if defined(__MACOSX)
#include <semaphore.h> /* POSIX semaphores */
#else
#include <sys/sem.h>
#endif

#if !defined(SIGPOLL) & defined(SIGIO)
#define SIGPOLL SIGIO
#endif

#include "fio.h"
#include "base.h"

#if defined(__MACOSX)
#define POSIX_SEM_LOCK		0x0
#define POSIX_SEM_STLOCK	0x1
#define POSIX_SEM_UNLOCK	0x2
#endif

INT fioaoperr = 0;		/* variable to hold open errors */
INT fioaclerr = 0;		/* variable to hold close errors */
INT fioarderr = 0;		/* variable to hold read errors */
INT fioawrerr = 0;		/* variable to hold write errors */
INT fioaskerr = 0;		/* variable to hold seek errors */
INT fioalkerr = 0;		/* variable to hold lock errors */
INT fioaflerr = 0;		/* variable to hold flush errors */
INT fioatrerr = 0;		/* variable to hold truncate errors */
INT fioadlerr = 0;		/* variable to hold delete errors */
INT fiosemerr = 0;		/* variable to hold semaphore errors */

static INT fioaflags;
static OFFSET fileoffset = 0x7FFFFFF0;
static OFFSET recoffset = 0x40000000;
static OFFSET excloffset = 0x7FFFFFF8;
#if defined(__MACOSX)
static sem_t **semid;
static sem_t *handle_semid;
#else
static INT *semid;
static INT handle_semid;
#endif
static INT semmax;
static CHAR findfile[MAX_NAMESIZE + 1];
static DIR *findhandle = NULL;
static INT findpathlength;
static CHAR foundfile[MAX_NAMESIZE + 1];

static INT matchname(CHAR *name, CHAR *pattern);
static void sigevent(INT);


INT fioainit(INT *fioflags, FIOAINITSTRUCT *parms)
{
	fioaflags = *fioflags;
	fileoffset = parms->fileoffset;
	recoffset = parms->recoffset;
	excloffset = parms->excloffset;
	return(0);
}

/**
 * Possible error returns; ERR_LKERR, ERR_NOACC, ERR_EXIST, ERR_NOMEM, ERR_NOSEM,
 * 		ERR_FNOTF, ERR_EXCMF, ERR_OPERR
 * Does not move memory
 */
INT fioaopen(CHAR *filename, INT mode, INT type, FHANDLE *handle)
{
	/* following tables correspond to with each row representing the 9 open modes */
	/* in order and the columns representing fioxop:create = 0, fioxop:create = 1 */
	/* and fiotouch respectively */
	static INT openarg1[10][3] = {
		{O_RDONLY,			-1,							O_RDONLY},
		{O_RDONLY,			-1,							O_RDONLY},
		{O_RDWR,			-1,							O_RDWR},
		{O_RDONLY,			-1,							O_RDONLY},
		{O_RDWR,			-1,							O_RDWR},
		{O_RDWR,			-1,							O_RDWR},
		{O_RDWR | O_TRUNC,	O_RDWR | O_CREAT | O_TRUNC,	O_RDWR},
		{O_RDWR | O_TRUNC,	O_RDWR | O_CREAT | O_TRUNC,	O_RDWR},
		{O_RDWR,			O_RDWR | O_CREAT | O_EXCL,	O_RDWR},
		{O_RDWR,			O_RDWR | O_CREAT,			O_RDWR}
	};

	INT i1, i2, hndl, lckflg, newmax;
#if defined(__MACOSX)
	sem_t **newid, *semx;
#else
	INT *newid;
#endif
	CHAR workname[MAX_NAMESIZE + 1];
	key_t key;
	struct stat statbuf;
#ifdef _SYS_SEM_BUF_H
	union semun semarg;
#else
	union {
		INT val;
		struct semid_ds *buf;
		USHORT *array;
	} semarg;
#endif

	if (fioaflags & FIO_FLAG_SINGLEUSER) {
		fioaflags |= FIO_FLAG_EXCLOPENLOCK;
		if (mode == FIO_M_SHR) mode = FIO_M_EXC;
		else if (mode <= FIO_M_SRA) mode = FIO_M_ERO;
	}
	for (i1 = 0; filename[i1]; i1++) {
		if (filename[i1] == '\\') workname[i1] = '/';
		else workname[i1] = filename[i1];
	}
	workname[i1] = '\0';

	if (mode <= FIO_M_ERO) lckflg = FIOA_EXLCK | FIOA_RDLCK;
	else lckflg = FIOA_EXLCK | FIOA_WRLCK;
	for ( ; ; ) {
		if (type >= 1) i1 = type;
		else i1 = (fioaflags & FIO_FLAG_EXCLOPENLOCK) ? 2 : 0;
		hndl = open(workname, openarg1[mode - 1][i1], 0x01b6);
		if (hndl != -1 && (fioaflags & FIO_FLAG_EXCLOPENLOCK)) {
			i2 = fioalock(hndl, lckflg, excloffset, 0);
			if (i2) {
				close(hndl);
				return(i2);
			}
		}
		if (hndl == -1 || type != 0 || !(fioaflags & FIO_FLAG_EXCLOPENLOCK) || (mode != FIO_M_PRP && mode != FIO_M_MTC))
			break;
/*** MAYBE SAVE HANDLE AND REOPEN AND CLOSE HANDLE LATER ***/
		fioalock(hndl, FIOA_EXLCK | FIOA_UNLCK, excloffset, 0);
		close(hndl);
		type = 1;
	}

	if (hndl != -1) {
		if (mode == FIO_M_EFC && !type) {
			if (fioaflags & FIO_FLAG_EXCLOPENLOCK) fioalock(hndl, FIOA_EXLCK | FIOA_UNLCK, excloffset, 0);
			close(hndl);
			return(ERR_EXIST);
		}

		if (fioaflags & FIO_FLAG_SEMLOCK) {
			if (hndl >= semmax) {
				if (!semmax) newmax = 256;
				else newmax = semmax << 1;
				while (hndl >= newmax) newmax <<= 1;
#if defined(__MACOSX)
				if (!semmax) newid = (sem_t **) malloc(newmax * sizeof(sem_t *));
				else newid = (sem_t **) realloc(semid, newmax * sizeof(sem_t *));
#else
				if (!semmax) newid = (INT *) malloc(newmax * sizeof(INT));
				else newid = (INT *) realloc(semid, newmax * sizeof(INT));
#endif
				if (newid == NULL) {
					if (fioaflags & FIO_FLAG_EXCLOPENLOCK) fioalock(hndl, FIOA_EXLCK | FIOA_UNLCK, excloffset, 0);
					close(hndl);
					return(ERR_NOMEM);
				}
				semid = newid;
				semmax = newmax;
			}
			if (mode == FIO_M_SHR) {
				i1 = fstat(hndl, &statbuf);
				if (i1 != -1) {
					key = ((LONG) statbuf.st_ino << 16) + statbuf.st_dev;
#if defined(__MACOSX)
					semx = sem_open("dxfiosem", O_CREAT | O_EXCL, key, 0x1b6);
					if (semx != SEM_FAILED) {  /* created, now set semval to 1 */
						sem_open("dxfiosem", 0, 1, 0x1b6);
					}
					else if (errno == EEXIST) semx = sem_open("dxfiosem", 0, key, 0x1b6);
					if (semx == SEM_FAILED) i1 = -1;
				}
				if (i1 == -1) {
					fiosemerr = errno;
					if (fioaflags & FIO_FLAG_EXCLOPENLOCK) fioalock(hndl, FIOA_EXLCK | FIOA_UNLCK, excloffset, 0);
					close(hndl);
					return(ERR_NOSEM);
				}
			}
			else semx = (INT *) SEM_FAILED;
			semid[hndl] = semx;
#else
					i1 = semget(key, 1, IPC_CREAT | IPC_EXCL | 0x1b6);
					if (i1 != -1) {  /* created, now set semval to 1 */
						semarg.val = 1;
						semctl(i1, 0, SETVAL, semarg);
					}
					else if (errno == EEXIST) i1 = semget(key, 1, 0x1b6);
				}
				if (i1 == -1) {
					fiosemerr = errno;
					if (fioaflags & FIO_FLAG_EXCLOPENLOCK) fioalock(hndl, FIOA_EXLCK | FIOA_UNLCK, excloffset, 0);
					close(hndl);
					return(ERR_NOSEM);
				}
			}
			else i1 = -1;
			semid[hndl] = i1;
#endif
		}
		/* set close on exec */
		i1 = fcntl(hndl, F_GETFD, 0);
		if (i1 != -1) fcntl(hndl, F_SETFD, i1 | FD_CLOEXEC);

		*handle = hndl;
		return(0);
	}
	if (errno == ENOENT) return(ERR_FNOTF);
	if (errno == EACCES) return(ERR_NOACC);
	if (errno == EEXIST) return(ERR_EXIST);
	if (errno == EMFILE || errno == ENFILE) return(ERR_EXCMF);
	fioaoperr = errno;
	return(ERR_OPERR);
}

INT fioaclose(FHANDLE handle)
{
	if (fioaflags & FIO_FLAG_EXCLOPENLOCK) fioalock(handle, FIOA_EXLCK | FIOA_UNLCK, excloffset, 0);

	if (close(handle) == -1) {
		fioaclerr = errno;
		return(ERR_CLERR);
	}
	return(0);
}

INT fioaread(FHANDLE handle, UCHAR *buffer, INT nbyte, OFFSET offset, INT *bytes)
{
	INT i1;

	if (offset != -1 && (i1 = fioalseek(handle, offset, 0, NULL))) return(i1);
	i1 = read(handle, (CHAR *) buffer, nbyte);
	if (i1 == -1) {
		fioarderr = errno;
		return(ERR_RDERR);
	}

	if (bytes != NULL) *bytes = i1;
	return(0);
}

/**
 * Note that HANDLE on Unix is defined in includes.h as INT
 */
INT fioawrite(FHANDLE handle, UCHAR *buffer, size_t nbyte, OFFSET offset, size_t *bytes)
{
	INT i1;

	if (!nbyte) return(0);

	if (bytes != NULL) *bytes = 0;
	if (offset != -1 && (i1 = fioalseek(handle, offset, 0, NULL))) return(i1);

	i1 = write(handle, (CHAR *) buffer, nbyte);
	if (i1 == -1) {
		fioawrerr = errno;
		return(ERR_WRERR);
	}
	if (bytes != NULL) *bytes = i1;
	if (i1 != (INT)nbyte) {
#if defined(__osf__) && defined(__alpha)
		write(handle, (CHAR *) buffer, nbyte);
		fioawrerr = errno;
#else
		fioawrerr = -1;
#endif
		return(ERR_WRERR);
	}
	return(0);
}

/**
 * Possible error returns; ERR_SKERR (-50)
 */
INT fioalseek(FHANDLE handle, OFFSET offset, INT method, OFFSET *filepos)
{
	OFFSET pos;
	INT movemethod;

	if (method == 0) movemethod = SEEK_SET;
	else if (method == 1) movemethod = SEEK_CUR;
	else if (method == 2) movemethod = SEEK_END;
	else {
		fioaskerr = (0 - method);
		return(ERR_SKERR);
	}
	pos = lseek(handle, offset, movemethod);
	if (pos == -1) {
		fioaskerr = errno;
		return(ERR_SKERR);
	}

	if (filepos != NULL) *filepos = pos;
	return(0);
}

/**
 * Returns; ERR_LKERR, ERR_NOACC, or zero for success
 * Does not move memory
 */
INT fioalock(FHANDLE handle, INT type, OFFSET offset, INT timeout)
{
	INT i1, err, /*oldalarm,*/ sem, sigerr/*, slept*/;
	struct flock lckbuf;
	struct itimerval itimer, oldtimer, dummy_timer, slept_timer;
#if !defined(__MACOSX)
	struct sembuf *sbufptr;
	static struct sembuf semlck = { 0, -1, SEM_UNDO };
	static struct sembuf semtstlck = { 0, -1, SEM_UNDO | IPC_NOWAIT };
	static struct sembuf semunlck = { 0, 1, SEM_UNDO };
#else
	INT semtype;
#endif
	sigset_t newmask, oldmask;
	struct sigaction act, oact;

	if (type & FIOA_FLLCK) offset = fileoffset;
	else if (type & FIOA_RCLCK) offset += recoffset;

#if defined(__MACOSX)
	if (!(sem = ((fioaflags & FIO_FLAG_SEMLOCK) && (type & FIOA_FLLCK) && semid[handle] != SEM_FAILED))) { /* fcntl type lock */
#else
	if (!(sem = ((fioaflags & FIO_FLAG_SEMLOCK) && (type & FIOA_FLLCK) && semid[handle] != -1))) { /* fcntl type lock */
#endif
		if (type & FIOA_WRLCK) lckbuf.l_type = F_WRLCK;
		else if (type & FIOA_RDLCK) lckbuf.l_type = F_RDLCK;
		else {
			lckbuf.l_type = F_UNLCK;
			timeout = 0;
		}
		lckbuf.l_whence = SEEK_SET; //0;
		lckbuf.l_start = offset;
		lckbuf.l_len = 1;
		if (timeout) type = F_SETLKW;
		else type = F_SETLK;
#if defined(__MACOSX)
		handle_semid = (sem_t *) (unsigned long)handle;
#else
		/* INT    */
		handle_semid = handle;
#endif
	}
	else {  /* semaphore type lock */
		if (type & (FIOA_WRLCK | FIOA_RDLCK)) {
#if defined(__MACOSX)
			if (timeout) semtype = POSIX_SEM_LOCK;
			else semtype = POSIX_SEM_STLOCK;
#else
			if (timeout) sbufptr = &semlck;
			else sbufptr = &semtstlck;
#endif
		}
		else {
#if defined(__MACOSX)
			semtype = POSIX_SEM_UNLOCK;
#else
			sbufptr = &semunlck;
#endif
			timeout = 0;
		}
		handle_semid = semid[handle];
	}

	/* disable common signals from interrupting lock */
	if (timeout) {
		sigemptyset(&newmask);
		if (timeout < 0) sigaddset(&newmask, SIGALRM);
		sigaddset(&newmask, SIGUSR1);
		sigaddset(&newmask, SIGUSR2);
#ifdef SIGPOLL
		sigaddset(&newmask, SIGPOLL);
#endif
#ifdef SIGTSTP
		sigaddset(&newmask, SIGTSTP);
#endif
		sigprocmask(SIG_BLOCK, &newmask, &oldmask);
		if (timeout > 0) {  /* set alarm to interrupt lock */
			/*oldalarm = alarm(0);*/
			timerclear(&itimer.it_value);
			timerclear(&itimer.it_interval);
			setitimer(ITIMER_REAL, &itimer, &oldtimer);

			act.sa_handler = sigevent;
			sigemptyset(&act.sa_mask);
			act.sa_flags = 0;
#ifdef SA_INTERRUPT
			act.sa_flags |= SA_INTERRUPT;
#endif
			sigerr = sigaction(SIGALRM, &act, &oact);
			if (sigerr != -1) {
				/*alarm(timeout);*/
				timerclear(&itimer.it_value);
				timerclear(&itimer.it_interval);
				itimer.it_value.tv_sec = timeout;
				setitimer(ITIMER_REAL, &itimer, &dummy_timer);
			}
		}
	}

/*** RACE CONDITION BETWEEN ALARM AND LOADING HANDLE_SEMID INTO REGISTER ***/
/*** LONGJMP/SIGLONGJMP CAN FIX THIS BUT IT WILL NOT WORK CORRECTLY UNDER ULTRIX ***/
	if (!sem) { /* fcntl type lock */
		i1 = fcntl((INT) (unsigned long)handle_semid, type, &lckbuf);
	}
	else {
#if defined(__MACOSX)
		switch (semtype) {
			case POSIX_SEM_LOCK:
				i1 = sem_wait(handle_semid);
				break;
			case POSIX_SEM_STLOCK:
				i1 = sem_trywait(handle_semid);
				break;
			case POSIX_SEM_UNLOCK:
				i1 = sem_post(handle_semid);
				break;
		}
#else
		i1 = semop(handle_semid, sbufptr, 1);
#endif
	}
	err = errno;

	if (timeout) {
		if (timeout > 0) {  /* stop alarm */
			if (sigerr != -1) {
				/*slept = timeout - alarm(0);*/
				timerclear(&itimer.it_value);
				timerclear(&itimer.it_interval);
				setitimer(ITIMER_REAL, &itimer, &slept_timer);
				slept_timer.it_value.tv_sec = timeout - slept_timer.it_value.tv_sec;
				sigaction(SIGALRM, &oact, NULL);
			}
			else {
				/*slept = 0;*/
				slept_timer.it_value.tv_sec = 0;
			}
			/*
			if (oldalarm > 0) {
				oldalarm = (oldalarm > slept) ? oldalarm - slept : 1;
				alarm(oldalarm);
			}*/
			if (oldtimer.it_value.tv_sec > 0) {
				if (oldtimer.it_value.tv_sec > slept_timer.it_value.tv_sec)
					oldtimer.it_value.tv_sec = oldtimer.it_value.tv_sec - slept_timer.it_value.tv_sec;
				else oldtimer.it_value.tv_sec = 1;
				setitimer(ITIMER_REAL, &oldtimer, &dummy_timer);
			}
		}
		/* enable common signals */
		sigprocmask(SIG_SETMASK, &oldmask, NULL);
	}
#if defined(__MACOSX)
	if (i1 != -1) return(0);
	if (handle_semid == SEM_FAILED || err == EAGAIN || (!sem && err == EACCES)) return(ERR_NOACC);
#else
	if (i1 != -1) {
		return(0);
	}
	if (handle_semid == -1 || err == EAGAIN || (!sem && err == EACCES)) {
		return(ERR_NOACC);
	}
#endif

	fioalkerr = err;
	return(ERR_LKERR);
}

INT fioaflush(FHANDLE handle)
{
	if (fsync(handle) == -1) {
		fioaflerr = errno;
		return(ERR_OPERR);
	}
	return(0);
}

INT fioatrunc(FHANDLE handle, OFFSET size)
{
	INT i1;

	if (size == -1 && (i1 = fioalseek(handle, 0, 2, &size))) return(i1);
	if (ftruncate(handle, size) == -1) {
		fioatrerr = errno;
		return(ERR_OPERR);
	}
	if (size != -1 && (i1 = fioalseek(handle, size, 0, NULL))) return(i1);
	return(0);
}

INT fioadelete(CHAR *filename)
{
	INT i1;
#if defined(__MACOSX)
	sem_t *semx;
#endif
	CHAR workname[MAX_NAMESIZE + 1];
	key_t key;
	struct stat statbuf;

	for (i1 = 0; filename[i1]; i1++) {
		if (filename[i1] == '\\') workname[i1] = '/';
		else workname[i1] = filename[i1];
	}
	workname[i1] = '\0';

	if (fioaflags & FIO_FLAG_SEMLOCK) {
		i1 = stat(workname, &statbuf);
		if (i1 != -1) {
			key = ((LONG) statbuf.st_ino << 16) + statbuf.st_dev;
#if defined(__MACOSX)
			sem_unlink("dxfiosem");
#else
			i1 = semget(key, 1, 0x1b6);
			if (i1 != -1) semctl(i1, 0, IPC_RMID, NULL);
#endif
		}
	}

	if (unlink(workname) == -1) {
		fioadlerr = errno;
		return(ERR_DLERR);
	}
	return(0);
}

INT fioarename(CHAR *oldname, CHAR *newname)
{
	INT i1;
	CHAR workname1[MAX_NAMESIZE + 1], workname2[MAX_NAMESIZE + 1];

	for (i1 = 0; oldname[i1]; i1++) {
		workname1[i1] = (oldname[i1] == '\\') ? '/' : oldname[i1];
//		if (oldname[i1] == '\\') workname1[i1] = '/';
//		else workname1[i1] = oldname[i1];
	}
	workname1[i1] = '\0';
	for (i1 = 0; newname[i1]; i1++) {
		workname2[i1] = (newname[i1] == '\\') ? '/' : newname[i1];
//		if (newname[i1] == '\\') workname2[i1] = '/';
//		else workname2[i1] = newname[i1];
	}
	workname2[i1] = '\0';

	if (link(workname1, workname2) || unlink(workname1)) {
		if (errno == ENOENT) return ERR_FNOTF;
		if (errno == EEXIST) return ERR_NOACC;
		return ERR_RENAM;
	}
	return 0;
}

INT fioafindfirst(CHAR *path, CHAR *file, CHAR **found)
{
	INT i1;
	CHAR workname[MAX_NAMESIZE + 1];
	struct dirent *dirinfo;

	for (i1 = 0; path[i1]; i1++) {
		if (path[i1] == '\\') workname[i1] = '/';
		else workname[i1] = path[i1];
	}
	if (!i1) workname[i1++] = '.';
	workname[i1] = '\0';
	findpathlength = i1;
	strcpy(foundfile, workname);
	if (foundfile[findpathlength - 1] != '/') foundfile[findpathlength++] = '/';
	if (*file) strcpy(findfile, file);
	else strcpy(findfile, "*");
	if (findhandle != NULL) closedir(findhandle);
	findhandle = opendir(workname);
	if (findhandle == NULL) {
		if (errno == ENOENT || errno == ENOTDIR) return ERR_FNOTF;
		if (errno == EACCES) return ERR_NOACC;
		if (errno == EMFILE || errno == ENFILE) return ERR_EXCMF;
		fioaoperr = errno;
		return ERR_OPERR;
	}
	for ( ; ; ) {
		dirinfo = readdir(findhandle);
		if (dirinfo == NULL) {
			closedir(findhandle);
			findhandle = NULL;
			return ERR_FNOTF;
		}
		if (matchname(dirinfo->d_name, findfile)) break;
	}
	strcpy(foundfile + findpathlength, dirinfo->d_name);
	*found = foundfile;
	return 0;
}

INT fioafindnext(CHAR **found)
{
	struct dirent *dirinfo;

	if (findhandle == NULL) return ERR_NOTOP;

	for ( ; ; ) {
		dirinfo = readdir(findhandle);
		if (dirinfo == NULL) {
			closedir(findhandle);
			findhandle = NULL;
			return ERR_FNOTF;
		}
		if (matchname(dirinfo->d_name, findfile)) break;
	}
	strcpy(foundfile + findpathlength, dirinfo->d_name);
	*found = foundfile;
	return 0;
}

INT fioafindclose()
{
	if (findhandle == NULL) return ERR_NOTOP;

	closedir(findhandle);
	findhandle = NULL;
	return 0;
}

INT fioaslash(CHAR *filename)
{
	INT i1;

	for (i1 = strlen(filename) - 1; i1 >= 0 && filename[i1] != '/' && filename[i1] != '\\'; i1--);
	return(i1);
}

/**
 * Append a slash to filename if it does not already end with one,
 * then NULL terminate it.
 */
void fioaslashx(CHAR *filename)
{
	INT i1;

	i1 = strlen(filename);
	if (i1 && filename[i1 - 1] != '/' && filename[i1 - 1] != '\\') {
		filename[i1] = '/';
		filename[i1 + 1] = '\0';
	}
}

static INT matchname(CHAR *name, CHAR *pattern)
{
	INT i1, i2;

	for (i1 = i2 = 0; pattern[i1]; i2++, i1++) {
		if (pattern[i1] == '*') {
			do if (!pattern[++i1]) return TRUE;
			while (pattern[i1] == '*');
			while (name[i2]) {
				if ((pattern[i1] == '?' || pattern[i1] == name[i2]) && matchname(name + i2, pattern + i1)) return TRUE;
				i2++;
			}
			return FALSE;
		}
		if (pattern[i1] == '?') {
			if (!name[i2]) return FALSE;
		}
		else {
			if (pattern[i1] == '\\' && (pattern[i1 + 1] == '\\' || pattern[i1 + 1] == '*' || pattern[i1 + 1] == '?')) i1++;
			if (pattern[i1] != name[i2]) return FALSE;
		}
	}
	return (!name[i2]);
}

static void sigevent(INT sig)
{
	switch (sig) {
		case SIGALRM:
#if defined(__MACOSX)
			handle_semid = (sem_t *) -1;
#else
			handle_semid = -1;
#endif
			break;
	}
}
