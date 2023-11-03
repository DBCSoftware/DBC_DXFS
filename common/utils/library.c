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
#define INC_SIGNAL
#define INC_ERRNO
#include "includes.h"
#include "release.h"
#include "arg.h"
#include "base.h"
#include "dbccfg.h"
#include "evt.h"
#include "fio.h"
#include "library.h"
#include "tim.h"
#include "vid.h"

#define CBKFUNC void (*)(INT, INT, INT)

#define MAXFILENAME 200
#define DEATH_INTERRUPT		0
#define DEATH_INVPARM		1
#define DEATH_INVPARMVAL		2
#define DEATH_INIT			3
#define DEATH_OPEN			4
#define DEATH_CLOSE			5
#define DEATH_READ			6
#define DEATH_WRITE			7
#define DEATH_NOMEM			8
#define DEATH_RENAME		9
#define DEATH_LIBCORRUPT		10
#define DEATH_EVENT			11
#define DEATH_VIDEO			12

static char *errormsg[] = {
	"HALTED - user interrupt",
	"Invalid parameter ->",
	"Invalid parameter value ->",
	"Unable to initialize",
	"Unable to open",
	"Unable to close file",
	"Unable to read from file",
	"Unable to write to file",
	"Unable to allocate memory",
	"Unable to rename file",
	"Corrupt library file",
	"Unable to initialize events",
	"Unable to initialize video"
};

#define LIBERR_EXIST		0L
#define LIBERR_FNOTF		1L
#define LIBERR_INVTYPE		2L
#define LIBERR_INVPARM		3L

static char *liberrmsg[] = {
	"Member already exists",
	"File or member not found",
	"Invalid file type",
	"Invalid parameter ->"
};

/* local declarations */
static INT handle, handle2, interactive;
static OFFSET filepos;
static CHAR filename[MAXFILENAME];
static UCHAR **cpbuf;
static INT writesize = 16384;
static struct dirblk **dir;
static INT maxlist;			/* maximum number of members to list at once */
static INT events[2];
static UCHAR fmap[VID_MAXKEYVAL / 8 + 1];

/* routine declarations */
static void showstr(CHAR *);
static INT kbdget(UCHAR *, INT, INT);
static INT charget(INT);
static void mnamecpy(CHAR *, CHAR *, INT, INT);
static INT nextparm(CHAR *, CHAR *, INT, INT);
static void compresscmd(void);
static void listcmd(void);
static void addcmd(CHAR *, CHAR *);
static INT doadd(CHAR *, CHAR *);
static void replacecmd(CHAR *, CHAR *);
static void extractcmd(CHAR *, CHAR *);
static void deletecmd(CHAR *);
static INT dodelete(CHAR *);
static void namecmd(CHAR *, CHAR *);
static void timestamp(CHAR *, CHAR *);
static INT findmember(CHAR *, CHAR *);
static void putdir(void);
static void resetdir(struct dirblk *);
static void membercpy(struct mement *, struct mement*);
static void v_ctl(INT32);
static void liberr(INT, INT, CHAR *);
static void usage(INT);

#if OS_WIN32
__declspec (noreturn) static void libexit(INT);
__declspec (noreturn) static void death(INT, INT, CHAR *);
__declspec (noreturn) static void quitsig(INT);
#else
static void libexit(INT)  __attribute__((__noreturn__));
static void death(INT, INT, CHAR *)  __attribute__((__noreturn__));
static void quitsig(INT)  __attribute__((__noreturn__));
#endif


/* MAIN */
INT main(INT argc, char *argv[])
{
	INT i;
	//, start
	INT result, novidflag;
	OFFSET filesize;
	CHAR termdef[MAX_NAMESIZE], indevice[MAX_NAMESIZE], outdevice[MAX_NAMESIZE];
	CHAR keymap[256], ukeymap[256], lkeymap[256];
	CHAR cfgname[MAX_NAMESIZE], work[MAXFILENAME], work2[MAXFILENAME], *ptr;
	FIOPARMS parms;
	VIDPARMS vidparms;

	arginit(argc, argv, &i);
	signal(SIGINT, quitsig);

	/* initialize */
	novidflag = 0;
	cfgname[0] = '\0';
	while (!argget(ARG_NEXT | ARG_IGNOREOPT, (char *) work, sizeof(work))) {
		if (work[0] == '-') {
			if (work[1] == '?') {
				usage(-1);
				exit(1);
			}
			if (toupper(work[1]) == 'C' && toupper(work[2]) == 'F' &&
			    toupper(work[3]) == 'G' && work[4] == '=') strcpy(cfgname, (char *) &work[5]);
			else if (toupper(work[1]) == 'V') novidflag = 1;
		}
	}
	if (cfginit(cfgname, FALSE)) death(DEATH_INIT, 0, cfggeterror());
	if (prpinit(cfggetxml(), CFG_PREFIX "cfg")) ptr = fioinit(NULL, FALSE);
	else ptr = fioinit(&parms, FALSE);
	if (ptr != NULL) death(DEATH_INIT, 0, ptr);

	for (i = 0; i <= 255; i++) keymap[i] = (CHAR) i;
	memcpy(ukeymap, keymap, 256);
	memcpy(lkeymap, keymap, 256);
	for (i = 'A'; i <= 'Z'; i++) {
		ukeymap[tolower(i)] = (CHAR) i;
		lkeymap[i] = (CHAR) tolower(i);
	}

	interactive = FALSE;
	if (argc - novidflag == 2) {
		if ((events[0] = evtcreate()) == -1 || (events[1] = evtcreate()) == -1) death(DEATH_EVENT, 0, NULL);
		memset(&vidparms, 0, sizeof(VIDPARMS));
		kdsConfigColorMode(&vidparms.flag);
		if (!prpget("display", "termdef", NULL, NULL, &ptr, 0)) {
			strcpy(termdef, ptr);
			vidparms.termdef = termdef;
		}
		if (!novidflag) {
			if (!prpget("keyin", "device", NULL, NULL, &ptr, 0)) {
				strcpy(indevice, ptr);
				vidparms.indevice = indevice;
			}
			if (!prpget("display", "device", NULL, NULL, &ptr, 0)) {
				strcpy(outdevice, ptr);
				vidparms.outdevice = outdevice;
			}
		}
		if (!prpget("keyin", "translate", NULL, NULL, &ptr, 0)) {
			if (prptranslate(ptr, (UCHAR *) keymap)) {
				dspstring("Unable to read keyin translate\n");
				cfgexit();
				exit(1);
			}
			vidparms.keymap = keymap;
		}
		if (!prpget("keyin", "uppertranslate", NULL, NULL, &ptr, 0)) {
			if (prptranslate(ptr, (UCHAR *) ukeymap)) {
				dspstring("Unable to read keyin upper translate\n");
				cfgexit();
				exit(1);
			}
			vidparms.ukeymap = ukeymap;
		}
		if (!prpget("keyin", "lowertranslate", NULL, NULL, &ptr, 0)) {
			if (prptranslate(ptr, (UCHAR *) lkeymap)) {
				dspstring("Unable to read keyin lower translate\n");
				cfgexit();
				exit(1);
			}
			vidparms.lkeymap = lkeymap;
		}
		if (!prpget("display", "columns", NULL, NULL, &ptr, 0)) vidparms.numcolumns = atoi(ptr);
		if (!prpget("display", "lines", NULL, NULL, &ptr, 0)) vidparms.numlines = atoi(ptr);
		if (!prpget("keyin", "ict", NULL, NULL, &ptr, 0)) vidparms.interchartime = atoi(ptr);
		i = vidinit(&vidparms, events[0]);
		if (i) death(DEATH_VIDEO, i, NULL);
		interactive = TRUE;

		/* set up ending keys */
		memset(fmap, 0xFF, VID_MAXKEYVAL / 8 + 1);
		vidkeyinfinishmap(fmap);

		v_ctl(VID_CURSOR_OFF);
		v_ctl(VID_CURSOR_BLOCK);
		v_ctl(VID_ES);
		v_ctl(VID_CURSOR_ULINE);
		vidgetwin(&i, &maxlist, &i, &i);
		maxlist -= 4;
	}

	handle = 0;
	/* scan input file name */
	i = argget(ARG_FIRST, filename, sizeof(filename));
	if (i < 0) death(DEATH_INIT, i, NULL);
	if (i || filename[0] == '-') {
		death(DEATH_INVPARM, 0, "Library-filename required on command line");
	}

	/* allocate memory for directory block reads */
	dir = (struct dirblk **) memalloc(DIRSIZE, 0);
	if (dir == NULL) death(DEATH_NOMEM, 0, NULL);

	/* allocate memory for copy buffer */
	while (writesize >= 4096) {
		cpbuf = memalloc(writesize, 0);
		if (cpbuf != NULL) break;
		writesize -= 1024;
	}
	if (cpbuf == NULL) {
		death(DEATH_NOMEM, 0, NULL);
	}

	/* open the input file */
	miofixname(filename, ".lib", FIXNAME_EXT_ADD);
	handle = fioopen(filename, FIO_M_EXC | FIO_P_TXT);
	if (handle == ERR_FNOTF) handle = fioopen(filename, FIO_M_EFC | FIO_P_TXT);
	if (handle < 0) death(DEATH_OPEN, 0, filename);

	fiogetsize(handle, &filesize);
	if (filesize == 0) {
		resetdir(*dir);
		filepos = 0;
		putdir();
		v_ctl(VID_HORZ);
		v_ctl(VID_VERT | 4);
		showstr("Library created (");
		showstr(filename);
		showstr(")");
		if (interactive) {
			v_ctl(VID_HORZ);
			v_ctl(VID_VERT | 1);
		}
		else v_ctl(VID_NL);
	}

	/* get parameter */
	//start = 1;
	while ((result = nextparm("CMD: ", work, 1, TRUE)) > 0) {
		if (interactive) {
			v_ctl(VID_NL);
			if (work[1] == 'Q') break;
		}
		switch(work[1]) {
			case 'A':
				result = nextparm("Name of file to add: ", work, MAXFILENAME, FALSE);
				if (result < 1 || !work[0]) {
					v_ctl(VID_VERT | 2);
					v_ctl(VID_HORZ);
					v_ctl(VID_EF);
					break;
				}
				nextparm("Member name (Enter if same): ", work2, MEMBERSIZE, TRUE);
				addcmd(work, work2);
				break;
			case 'C':
				compresscmd();
				break;
			case 'D':
				result = nextparm("Name of member to delete: ", work2, MEMBERSIZE, TRUE);
				if (result < 1 || !work2[0])	{
					v_ctl(VID_VERT | 2);
					v_ctl(VID_HORZ);
					v_ctl(VID_EF);
					break;
				}
				deletecmd(work2);
				break;
			case 'E':
				result = nextparm("Name of member to extract: ", work2, MEMBERSIZE, TRUE);
				if (result < 1 || !work2[0])	{
					v_ctl(VID_VERT | 2);
					v_ctl(VID_HORZ);
					v_ctl(VID_EF);
					break;
				}
				nextparm("File name (Enter if same): ", work, MAXFILENAME, FALSE);
				extractcmd(work2, work);
				break;
			case 'L':
				listcmd();
				break;
			case 'N':
				result = nextparm("Enter member name to rename ", work, MEMBERSIZE, TRUE);
				if (result < 1 || !work[0])  {
					v_ctl(VID_VERT | 2);
					v_ctl(VID_HORZ);
					v_ctl(VID_EF);
					break;
				}
				result = nextparm("Enter new member name ", work2, MEMBERSIZE, TRUE);
				if (result < 1 || !work2[0])	{
					v_ctl(VID_VERT | 2);
					v_ctl(VID_HORZ);
					v_ctl(VID_EF);
					break;
				}
				namecmd(work, work2);
				break;
			case 'R':
				result = nextparm("Name of file replacing library member: ", work, MAXFILENAME, FALSE);
				if (result < 1 || !work[0]) {
					v_ctl(VID_VERT | 2);
					v_ctl(VID_HORZ);
					v_ctl(VID_EF);
					break;
				}
				nextparm("Member name being replaced (Enter if same): ", work2, MEMBERSIZE, TRUE);
				replacecmd(work, work2);
				break;
			case '?':
				if (interactive) {
					usage(0);
					break;
				}
				// fall through
				/* no break */
			case 'I':
			case 'V':
			case 'S':
			case '-':
				if (!interactive) break;
				// fall through
				/* no break */
			default:
				if ((UCHAR) work[1] < 0x20 || (UCHAR) work[1] > 0x7F) work[1] = '\0';
				else work[2] = '\0';
				liberr(LIBERR_INVPARM, 0, &work[1]);
				break;
		}
		if (interactive) {
			v_ctl(VID_HORZ);
			v_ctl(VID_VERT | 1);
		}
	}
	libexit(0);
	return(0);
}

/* NEXTPARM */
static INT nextparm(CHAR *prompt, CHAR *str, INT cnt, INT ucaseflag)
{
	static CHAR parmsave[MAXFILENAME];
	static INT parmpos;
	INT i;
	CHAR *save;

	if (interactive) {
		showstr(prompt);
		if (cnt == 1) {
			str[0] = '-';
			str[1] = (UCHAR) charget(FALSE);
			if (ucaseflag) str[1] = toupper(str[1]);
		}
		else kbdget((UCHAR *) str, cnt, ucaseflag);
	}
	else {
		if (!parmsave[parmpos]) {
			(*str) = '\0';
			parmpos = 0;
			i = argget(ARG_NEXT, parmsave, sizeof(parmsave));
			if (i) {
				if (i < 0) death(DEATH_INIT, i, NULL);
				parmsave[parmpos] = '\0';
				return(0);	/* no more parameters */
			}
		}
		save = str;
		/* Copy string if not a "CMD: " or if first char is not - */
		if (cnt == 1 || parmsave[parmpos] != '-') do {
			*save = parmsave[parmpos++];
			if (ucaseflag) *save = toupper(*save);
			if (*save == '=') continue;
			save++;
		} while (parmpos < MAXFILENAME && parmsave[parmpos] && parmsave[parmpos] != ',' && parmsave[parmpos] != '=');
		*save = '\0';
	}
	return(1);
}

/* Command routines */
/* COMPRESSCMD */
static void compresscmd()
{
	INT srchandle, num, member, result, nbytes;
	OFFSET srcpos, size, length, nextpos;
	CHAR newname[MAXFILENAME], *sysname;
	struct dirblk **olddir;

	if (handle) {
		sysname = fioname(handle);
		if (strlen(sysname) < MAXFILENAME) strcpy(filename, sysname);
		fioclose(handle);
	}

	strcpy(newname, filename);
	miofixname(newname, ".___", FIXNAME_EXT_ADD | FIXNAME_EXT_REPLACE);

	if ((handle = fioopen(newname, FIO_M_EXC | FIO_P_WRK)) > 0) fiokill(handle);
	if ((num = fioarename(filename, newname)) < 0) death(DEATH_RENAME, num, "Rename failed ");

	handle = fioopen(filename, FIO_M_EFC | FIO_P_WRK);
	if (handle < 1) death(DEATH_OPEN, handle, filename);

	srchandle = fioopen(newname, FIO_M_SRO | FIO_P_WRK);
	if (srchandle < 1) death(DEATH_OPEN, srchandle, newname);

	resetdir(*dir);
	num = 0;
	olddir = (struct dirblk **) memalloc(DIRSIZE, 0);
	if (olddir == NULL) death(DEATH_NOMEM, 0, NULL);

	srcpos = 0;
	filepos = 0;
	size = 0;
	nbytes = fioread(srchandle, srcpos, (UCHAR *) *olddir, DIRSIZE);
	if (nbytes != DIRSIZE) death(DEATH_LIBCORRUPT, 0, NULL);
	size += DIRSIZE;
	mscoffto9(0, (UCHAR *) (*dir)->header.filepos);
	do {
		for (member = 0; member < NUMMEMBERS; member++) {
			if (memcmp((*olddir)->member[member].status, CURREVISION, STATSIZE) == 0) {
				membercpy(&((*dir)->member[num]), &((*olddir)->member[member]));
				msc9tooff((UCHAR *) (*olddir)->member[member].length, &length);
				msc9tooff((UCHAR *) (*olddir)->member[member].filepos, &srcpos);

				/* copy file from src (at srcpos,length) to newfile (at size, length) */
				mscoffto9(size, (UCHAR *) (*dir)->member[num].filepos);
				do {
					nbytes = fioread(srchandle, srcpos, *cpbuf, writesize);
					if (nbytes >= length) {
						result = fiowrite(handle, size, *cpbuf, (INT) length);
						if (result < 0) death(DEATH_WRITE, result, NULL);
						size += length;
						length = 0;
						srcpos += length;
					}
					else {
						if (nbytes < 0) death(DEATH_READ, (INT) nbytes, NULL);
						result = fiowrite(handle, size, *cpbuf, writesize);
						if (result < 0) death(DEATH_WRITE, result, NULL);
						size += writesize;
						length -= writesize;
						srcpos += writesize;
					}
				} while (length != 0);
				num++;
				if (num == NUMMEMBERS) {
					mscoffto9(size, (UCHAR *) (*dir)->header.filepos);
					putdir();
					resetdir(*dir);
					num = 0;
					filepos = size;
					size += DIRSIZE;
				}
			}
		}

		msc9tooff((UCHAR *) (*olddir)->header.filepos, &nextpos);
		if (nextpos != 0) {
			srcpos = nextpos;
			nbytes = fioread(srchandle, srcpos, (UCHAR *) *olddir, DIRSIZE);
			if (nbytes != DIRSIZE) death(DEATH_LIBCORRUPT, 0, NULL);
		}
	} while(nextpos != 0);
	putdir();

	memfree ((UCHAR **) olddir);
	fiokill(srchandle);
}

/* LISTCMD */
static void listcmd()
{
	static UCHAR buildline[34];
	INT num, count, key, bytecnt;

	filepos = 0; /* restart global filepos */
	v_ctl(VID_VERT | 2);
	v_ctl(VID_HORZ);
	v_ctl(VID_EF);
	v_ctl(VID_WIN_TOP | 4);
	v_ctl(VID_VERT);
	v_ctl(VID_HORZ);
	do {
		bytecnt = fioread(handle, filepos, (UCHAR *) *dir, DIRSIZE);
		if (bytecnt != DIRSIZE) death(DEATH_LIBCORRUPT, 0, NULL);
		for (num = 0, count = 0; num < NUMMEMBERS && (*dir)->member[num].status[0] != ' '; num++) {
			if (!memcmp((*dir)->member[num].status, CURREVISION, STATSIZE)) {
				if (interactive && count == maxlist) {
					showstr("Press space for next page, ENTER for next line and ESCAPE to stop");
					key = charget(FALSE);
					v_ctl(VID_HORZ);
					v_ctl(VID_EF);
					if (key == VID_ENTER) count--;
					else if (key == VID_ESCAPE) break;
					else count = 0;
				}
				memcpy(buildline, (*dir)->member[num].name, 8);
				buildline[8] = ' ';
				memcpy(&buildline[9], (*dir)->member[num].length, 9);
				buildline[18] = ' ';
				memcpy(&buildline[19], (*dir)->member[num].date, 2);
				buildline[21] = '-';
				memcpy(&buildline[22], &((*dir)->member[num].date[2]), 2);
				buildline[24] = '-';
				memcpy(&buildline[25], &((*dir)->member[num].date[4]), 2);
				buildline[27] = ' ';
				memcpy(&buildline[28], (*dir)->member[num].time, 2);
				buildline[30] = ':';
				memcpy(&buildline[31], &((*dir)->member[num].time[2]), 2);
				buildline[33] = '\0';
				showstr((CHAR *) buildline);
				v_ctl(VID_NL);
				count++;
			}
		}
		msc9tooff((UCHAR *) (*dir)->header.filepos, &filepos);
	} while (filepos != 0);
	v_ctl(VID_WIN_TOP);
}

/* ADDCMD */
static void addcmd(CHAR *filename, CHAR *name) 
{
	static CHAR fname[MAXFILENAME];
	static CHAR mname[MEMBERSIZE + 1];
	INT pos, mark;

	v_ctl(VID_VERT | 2);
	v_ctl(VID_HORZ);
	v_ctl(VID_EF);
	v_ctl(VID_VERT | 4);
	strcpy(fname, filename);
	miofixname(fname, ".txt", FIXNAME_EXT_ADD);
	if (!*name){
		mark = -1;
		for (pos = 0; fname[pos] != 0; pos++)
			if (fname[pos] == '/' || fname[pos] == '\\' || fname[pos] == ':')
				mark = pos;
		mnamecpy(mname, &fname[mark + 1], MEMBERSIZE, 1);
	} else mnamecpy(mname, name, MEMBERSIZE, 1);
	if (doadd(fname, mname)) {
		showstr(mname);
		showstr(" added");
		v_ctl(VID_NL);
	};
}

/* DOADD */
static INT doadd(CHAR *filename, CHAR *name)
{
	INT found, nbytes, num, result;
	OFFSET nextpos, filepos2, filesize;

	found = 0;

	if ((num = findmember(name, CURREVISION)) >= 0) {
		liberr(LIBERR_EXIST, 0, name);
		return(0);
	}
	handle2 = fioopen(filename, FIO_P_SRC | FIO_M_SRO);
	if (handle2 < 0) {
		liberr(LIBERR_FNOTF, handle2, filename);
		return(0);
	}
#if OS_VMS
	if ((num = fiotype(handle2)) != VIO_TYP_DBC) {
		liberr(LIBERR_INVTYPE, num, filename);
		return(0);
	};
#endif
	/* find place to add new member */
	filepos = 0;
	nextpos = filepos;
	do {
		/* Read directory block */
		filepos = nextpos;
		nbytes = fioread(handle, filepos, (UCHAR *) *dir, DIRSIZE);
		if (nbytes != DIRSIZE) death(DEATH_LIBCORRUPT, 0, NULL);

		/* Find next empty slot */
		for (num = 0; num < NUMMEMBERS &&
			!(found = ((*dir)->member[num].status[0] == ' ')); num++);

		/* location of next dir block */
		if (!found) msc9tooff((UCHAR *) (*dir)->header.filepos, &nextpos); 
	} while (nextpos != 0 && !found);
	if (!found) {  /* need to add a new directory block */
		fiogetsize(handle, &nextpos);
		mscoffto9(nextpos, (UCHAR *) (*dir)->header.filepos);
		putdir();
		filepos = nextpos;
		resetdir(*dir);
		num = 0;
		putdir();
	}
	fiogetsize(handle, &nextpos);
	memcpy((*dir)->member[num].status, CURREVISION, STATSIZE);
	mnamecpy((*dir)->member[num].name, name, MEMBERSIZE, 0);
	mscoffto9(nextpos, (UCHAR *)(*dir)->member[num].filepos);
	fiogetsize(handle2, &filesize);
	mscoffto9(filesize, (UCHAR *)(*dir)->member[num].length);
	timestamp((*dir)->member[num].time, (*dir)->member[num].date);
	putdir();

	/* copy file into library */
	fiogetsize(handle, &filepos);
	filepos2 = 0;
	do {
		nbytes = fioread(handle2, filepos2, *cpbuf, writesize);
		if (nbytes < 0) {
			death(DEATH_READ, (INT) nbytes, NULL);
		}
		result = fiowrite(handle, filepos, *cpbuf, nbytes);
		if (result < 0) {
			death(DEATH_WRITE, result, NULL);
		}
		filepos2 += nbytes;
		filepos += nbytes;
	} while (nbytes == writesize);
	fioclose(handle2);
	return(1);
}

/* REPLACECMD */
static void replacecmd(CHAR *filename, CHAR *name)
{
	static CHAR mname[MEMBERSIZE + 1];
	static CHAR fname[MAXFILENAME];
	INT pos, mark;

	v_ctl(VID_VERT | 2);
	v_ctl(VID_HORZ);
	v_ctl(VID_EF);
	v_ctl(VID_VERT | 4);
	strcpy(fname, filename);
	miofixname(fname, ".txt", FIXNAME_EXT_ADD);
	if (!*name) {
		mark = -1;
		for (pos = 0; fname[pos] != 0; pos++)
			if (fname[pos] == '/' || fname[pos] == '\\' || fname[pos] == ':') mark = pos;
		mnamecpy(mname, &fname[mark + 1], MEMBERSIZE, 1);
	}
	else mnamecpy(mname, name, MEMBERSIZE, 1);
	handle2 = fioopen(fname, FIO_P_SRC | FIO_M_SRO);
	if (handle2 < 0) {
		liberr(LIBERR_FNOTF, handle2, fname);
		return;
	};
	fioclose(handle2);
	if (dodelete(mname) && doadd(fname, mname)) {
		showstr(mname);
		showstr(" replaced");
		v_ctl(VID_NL);
	}
}

/* EXTRACTCMD */
static void extractcmd(CHAR *name, CHAR *filename)
{
	static CHAR fname[MAXFILENAME];
	static CHAR mname[MEMBERSIZE + 1];
	UINT nameflag;
	INT outfile;
	INT pos, mark;
	INT nbytes, obytes;
	OFFSET infilepos, outpos, len;

	v_ctl(VID_VERT | 2);
	v_ctl(VID_HORZ);
	v_ctl(VID_EF);
	v_ctl(VID_VERT | 4);
	if ((nameflag = !*filename)) {
		strcpy(fname, name);
		pos = 0;
		while (fname[pos]) {
			fname[pos] = (UCHAR)tolower(fname[pos]);
			pos++;
		}
	}
	else strcpy(fname, filename);
	miofixname(fname, ".txt", FIXNAME_EXT_ADD);
	mark = -1;
	if (nameflag) {
		for (pos = 0; fname[pos] != 0; pos++)
			if(fname[pos] == '/' || fname[pos] == '\\' || fname[pos] == ':') mark = pos;
		mnamecpy(mname, &fname[mark+1], MEMBERSIZE, 1);
	}
	else mnamecpy(mname, name, MEMBERSIZE, 1);

	if ((pos = findmember(mname, CURREVISION)) < 0) {
		liberr(LIBERR_FNOTF, 0, mname);
		return;
	}
	msc9tooff((UCHAR *)(*dir)->member[pos].filepos, &infilepos);
	msc9tooff((UCHAR *)(*dir)->member[pos].length, &len);
	outfile = fioopen(fname, FIO_P_WRK | FIO_M_PRP);
	if (outfile < 1) death(DEATH_OPEN, outfile, fname);
	outpos = 0;
	while (len) {
		nbytes = fioread(handle, infilepos, *cpbuf, writesize);
		if (nbytes < 0) death(DEATH_READ, (INT) nbytes, NULL);
		if (len < writesize) {
			obytes = fiowrite(outfile, outpos, *cpbuf, (INT) len);
			len = 0;
		} else {
			obytes = fiowrite(outfile, outpos, *cpbuf, writesize);
			len -= writesize;
		}
		if (obytes < 0) death(DEATH_WRITE, obytes, NULL);
		infilepos += writesize;
		outpos += writesize;
	}
	fioclose(outfile);
	showstr(mname);
	showstr(" extracted to ");
	showstr(fname);
	v_ctl(VID_NL);
}

/* DELETECMD */
static void deletecmd(CHAR *name)
{
	static CHAR mname[MEMBERSIZE + 1];

	v_ctl(VID_VERT | 2);
	v_ctl(VID_HORZ);
	v_ctl(VID_EF);
	v_ctl(VID_VERT | 4);
	mnamecpy(mname, name, MEMBERSIZE, 1);
	
	if (dodelete(mname)) {
		showstr(mname);
		showstr(" deleted");
		v_ctl(VID_NL);
	}
}

/* DODELETE */
static INT dodelete(CHAR name[])
{
	INT num;

	/* delete older revision if it exists */
	num = findmember(name, OLDREVISION);
	if (num >= 0) {
		memcpy((*dir)->member[num].status, DELETED, STATSIZE);
		putdir();
	}

	/* delete current revision */
	num = findmember(name, CURREVISION);
	if (num < 0) {
		liberr(LIBERR_FNOTF, 0, name);
		return(0);
	}
	else {
		memcpy((*dir)->member[num].status, DELETED, STATSIZE);
		putdir();
	}
	return(1);
}

/* NAMECMD */
static void namecmd(CHAR *name, CHAR *newname)
{
	static CHAR oldname[9];
	static CHAR replacename[9];
	INT num;

	v_ctl(VID_VERT | 2);
	v_ctl(VID_HORZ);
	v_ctl(VID_EF);
	v_ctl(VID_VERT | 4);
	if (!*newname){
		liberr(LIBERR_INVPARM, 0, "Destination missing");
		return;
	}

	mnamecpy(oldname, name, MEMBERSIZE, 1);
	mnamecpy(replacename, newname, MEMBERSIZE, 1);
	num = findmember(replacename, CURREVISION);
	if (num >= 0) {
		liberr(LIBERR_EXIST, 0, replacename);
		return;
	}
	num = findmember(oldname, CURREVISION);
	if (num < 0) {
		liberr(LIBERR_FNOTF, 0, oldname);
		return;
	}
	mnamecpy((*dir)->member[num].name, replacename, MEMBERSIZE, 0);
	putdir();
	showstr(oldname);
	showstr(" renamed to ");
	showstr(replacename);
	v_ctl(VID_NL);
}

/* Library Misc functions */
/* TIMESTAMP */
static void timestamp(CHAR *stime, CHAR *date)
{
	static UCHAR tmp[16];
	
	msctimestamp(tmp);
	stime[0] = tmp[8]; stime[1] = tmp[9];  /* hh */
	stime[2] = tmp[10]; stime[3] = tmp[11];  /* min */ 
	date[0] = tmp[4]; date[1] = tmp[5];  /* month */
	date[2] = tmp[6]; date[3] = tmp[7];  /* day */ 
	date[4] = tmp[2]; date[5] = tmp[3];  /* year */ 
}

/* find member name in library and return member number in current dir block */
/* return value of -1 means search fails */
/* FINDMEMBER */
static INT findmember(CHAR *name, CHAR *status)
{
	static CHAR mname[MEMBERSIZE];
	INT bytecnt, found, count, pos;
	OFFSET nextpos;

	filepos = 0;
	found = 0;

	/* copy name into proper length string */
	memset(mname, ' ', MEMBERSIZE);
	for (count = 0; count < MEMBERSIZE && name[count] != 0;count++)
		mname[count] = name[count];
	
	/* search for member name */
	do {
		bytecnt = fioread(handle, filepos, (UCHAR *) *dir, DIRSIZE);
		if (bytecnt != DIRSIZE) death(DEATH_LIBCORRUPT, 0, NULL);
		for (count = 0; count < NUMMEMBERS && !found; count++) {
			found = TRUE;
			for (pos = 0; pos < MEMBERSIZE && mname[pos]; pos++) {
				if (toupper((*dir)->member[count].name[pos]) != toupper(mname[pos])) {
					found = FALSE;
					break;
				}
			}
			found = (found && !memcmp((*dir)->member[count].status, status, STATSIZE));
		}

		msc9tooff((UCHAR *)(*dir)->header.filepos, &nextpos);
		if (!found) filepos = nextpos;
	} while (!found && nextpos != 0);

	if (found) return (--count);
	else return (-1);
}

/* PUTDIR */
static void putdir()
{
	INT ioerror;

	ioerror = fiowrite(handle, filepos, (UCHAR *) *dir, DIRSIZE);
	if (ioerror < 0) death(DEATH_WRITE, ioerror, "Could not update library file");
}

/* RESETDIR */
static void resetdir(struct dirblk *dir)
{
	int memnum;

	memcpy(dir->header.title, "$LIBRARY R8.0   ", 16);
	memset(dir->header.filepos, '0', 8);
	dir->header.filepos[8] = '\0';
	memset(dir->header.blanks, ' ', 23);
	for (memnum = 0; memnum < NUMMEMBERS; memnum++) {
		memset(dir->member[memnum].status, ' ', STATSIZE);
		memset(dir->member[memnum].name, ' ', MEMBERSIZE);
		memset(dir->member[memnum].blanks, ' ', 4);
		memset(dir->member[memnum].filepos, ' ', 9);
		memset(dir->member[memnum].length, ' ', 9);
		memset(dir->member[memnum].time, ' ', 4);
		memset(dir->member[memnum].date, ' ', 6);
		memset(dir->member[memnum].moreblanks, ' ', 4);
	}
}

/* MEMBERCPY */
static void membercpy(struct mement *dest, struct mement *src)
{
	memcpy(dest->status, src->status, STATSIZE);
	memcpy(dest->name, src->name, MEMBERSIZE);
	memcpy(dest->filepos, src->filepos, 9);
	memcpy(dest->length, src->length, 9);
	memcpy(dest->time, src->time, 4);
	memcpy(dest->date, src->date, 6);
}


/* Support functions */
/* MNAMECPY - Specialized function to copy member names from src to	*/
/*		    dest by converting all chars to upper case and		*/
/*		    appending spaces to the end to extend dest to length	*/
/*		    size.  The value pointed to by src string is terminated */
/*		    by either a '.' or a '\0'.   If the value nullterm != 0 */
/*		    dest is null-terminated.							*/
static void mnamecpy(CHAR *dest, CHAR *src, INT size, INT nullterm)
{
	INT pos;

	memcpy(dest, src, size);
	for (pos = 0; pos < size && src[pos] != 0 && src[pos] != '.'; pos++) {
		dest[pos] = toupper(src[pos]);
	}
	while (pos < size) dest[pos++] = ' ';
	if (nullterm) dest[size] = '\0';
}

/* SHOWSTR */
static void showstr(CHAR *str)
{
	INT cmdcnt;
	INT32 vidcmd[5];
	
	if (interactive) {
		vidcmd[0] = VID_DISPLAY | (INT32)strlen(str);
		cmdcnt = 1;
		if (sizeof(void *) > sizeof(INT32)) {
			memcpy((void *) &vidcmd[cmdcnt], (void *) &str, sizeof(void *));
			cmdcnt += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
		}
		else *(UCHAR **)(&vidcmd[cmdcnt++]) = (UCHAR *) str;
		vidcmd[cmdcnt] = VID_END_NOFLUSH;
		vidput(vidcmd);
	}
	else dspstring(str);
}

/* V_CTL */
static void v_ctl(INT32 ctl)
{
	INT32 vidcmd[2];

	if (interactive) {
		vidcmd[0] = ctl;
		vidcmd[1] = VID_END_NOFLUSH;
		vidput(vidcmd);
	}
	else if (ctl == VID_NL) dspchar('\n');
}

static INT kbdget(UCHAR *field, INT maxlen, INT ucaseflag)
{
	INT pos, c;

	pos = 0;
	if (ucaseflag) v_ctl(VID_KBD_UC);
	while ((c = charget(TRUE)) != VID_ENTER) {
		if (c == VID_BKSPC) {
			if (pos) {
				pos--;
				v_ctl(VID_HORZ_ADJ | 0xFFFF);
				v_ctl(VID_DISP_CHAR | ' ');
				v_ctl(VID_HORZ_ADJ | 0xFFFF);
			}
		}
		else if (c >= 32 && c < 256 && pos < maxlen) {
			field[pos++] = (UCHAR) c;
			v_ctl(VID_DISP_CHAR | c);
		}
	}
	v_ctl(VID_NL);
	field[pos] = '\0';
	if (ucaseflag) v_ctl(VID_KBD_IN);
	return(pos);
}

static INT charget(INT cursflg)
{
	INT i1, i2;
	INT32 vidcmd[2];
	UCHAR work[8];

	if (cursflg) vidcmd[0] = VID_CURSOR_ON;
	else vidcmd[0] = VID_CURSOR_OFF;
	vidcmd[1] = VID_END_NOFLUSH;
	vidput(vidcmd);
	evtclear(events[1]);
	if (vidkeyinstart(work, 1, 1, 1, FALSE, events[1])) {
/*** CODE: ADD ERROR CODE ***/
	}
	evtwait(events, 2);
	if (evttest(events[0])) quitsig(SIGINT);
	vidkeyinend(work, &i1, &i2);
	if (i2 < 0) i2 = 0;
	return(i2);
}

/* LIBERR */
static void liberr(INT n, INT e, CHAR *s)
{
	CHAR work[17];

	v_ctl(VID_HORZ);
	v_ctl(VID_EL);
	if (n < (INT) (sizeof(liberrmsg) / sizeof(*liberrmsg))) showstr(liberrmsg[n]);
	else {
		mscitoa(n, work);
		showstr("*** UNKNOWN ERROR ");
		showstr(work);
		showstr("***");
	}
	if (e) {
		showstr(": ");
		showstr(fioerrstr(e));
	}
	if (s != NULL) {
		showstr(": ");
		showstr(s);
	}
	v_ctl(VID_NL);
}

static void libexit(INT code)
{
#if OS_WIN32
	INT eventid;
	CHAR work[17];
#endif

	if (interactive) {
		vidreset();
#if OS_WIN32
		if ((eventid = evtcreate()) != -1) {
			msctimestamp(work);
			timadd(work, 200);
			timset(work, eventid);
			evtwait(&eventid, 1);
		}
#endif
		videxit();
	}
	cfgexit();
	exit(code);
}

/* USAGE */
static void usage(INT cmdlineflg)  /* error condition */
{
	if (cmdlineflg == -1) {
		dspstring("LIBRARY command  " RELEASEPROGRAM RELEASE COPYRIGHT);
		dspchar('\n');
		dspstring("Usage: LIBRARY filename [-A=filename [name]] [-C] [-E=name [filename]]\n");
		dspstring("               [-D=name] [-L] [-N=name name] [-O=filename]\n");
		dspstring("               [-R=filename [name]]\n");
	}
	else if (cmdlineflg) {
		showstr("LIBRARY command release ");
		showstr(RELEASE);
		showstr(COPYRIGHT);
		v_ctl(VID_NL);
		showstr("Usage: LIBRARY filename [-A=filename [name]] [-C] [-E=name [filename]]");
		v_ctl(VID_NL);
		showstr("               [-D=name] [-L] [-N=name name] [-O=filename]");
		v_ctl(VID_NL);
		showstr("               [-R=filename [name]]");
		v_ctl(VID_NL);
	}
	else {
		v_ctl(VID_VERT | 2);
		v_ctl(VID_HORZ);
		v_ctl(VID_EF);
		v_ctl(VID_VERT | 4);
		v_ctl(VID_NL); showstr("The commands available are:");
		v_ctl(VID_NL); showstr("   A filename[,name] = Add a member to the library");
		v_ctl(VID_NL); showstr("   R filename[,name] = Replace a member in the library");
		v_ctl(VID_NL); showstr("   E name[,filename] = Extract a member from the library");
		v_ctl(VID_NL); showstr("   D name = Delete a member of the library");
		v_ctl(VID_NL); showstr("   N name,name = Renames a member of the library");
		v_ctl(VID_NL); showstr("   L = List the library members, sizes, timestamps");
		v_ctl(VID_NL); showstr("   ? = display this help screen");
		v_ctl(VID_NL); showstr("   Q = quit and exit to operating system");
		v_ctl(VID_NL);
	}
	return;
}

/* DEATH */
static void death(INT n, INT e, CHAR *s)
{
	CHAR work[17];

	if (n == DEATH_INVPARM || n == DEATH_INVPARMVAL) usage(!interactive || n == DEATH_INVPARM);
	v_ctl(VID_NL);
	if (n < (INT) (sizeof(errormsg) / sizeof(*errormsg))) showstr(errormsg[n]);
	else {
		mscitoa(n, work);
		showstr("*** UNKNOWN ERROR ");
		showstr(work);
		showstr("***");
	}
	if (e) {
		showstr(": ");
		showstr(fioerrstr(e));
	}
	if (s != NULL) {
		showstr(": ");
		showstr(s);
	}
	v_ctl(VID_NL);
	libexit(1);
}

/* QUITSIG */
#if defined(__GNUC__)
static void quitsig(__attribute__ ((unused)) INT value)
#else
static void quitsig(INT value)
#endif
{
	signal(SIGINT, SIG_IGN);
	if (interactive) videxit();
	death(DEATH_INTERRUPT, 0, NULL);
}
