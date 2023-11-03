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
#include "tim.h"
#include "vid.h"

#define CBKFUNC void (*)(INT, INT, INT)

#define DUMPSIZ 16384
#define DEATH_INTERRUPT		0
#define DEATH_INVPARM		1
#define DEATH_INVPARMVAL		2
#define DEATH_INIT			3
#define DEATH_OPEN			4
#define DEATH_CLOSE			5
#define DEATH_READ			6
#define DEATH_WRITE			7
#define DEATH_EVENT			8
#define DEATH_VIDEO			9

static char *errormsg[] = {
	"HALTED - user interrupt",
	"Invalid parameter ->",
	"Invalid parameter value ->",
	"Unable to initialize",
	"Unable to open",
	"Unable to close file",
	"Unable to read from file",
	"Unable to write to file",
	"Unable to initialize events",
	"Unable to initialize video"
};

/* local declarations */
static INT fieldlen;
static UCHAR vidflg, recflg;
static INT handle, blksize, inblksz, offset;
static INT blkflg, modflg, hexflg, dspflg;
static OFFSET filepos, curblk;
static INT openflg;
static INT32 vidcmd[100];
static INT cmdcnt;
static CHAR filename[100];
static UCHAR fillchar, buffer[DUMPSIZ];
static INT events[2];
static UCHAR fmap[VID_MAXKEYVAL / 8 + 1];
static UCHAR dig1map[121] = {
	'0','0','0','0','0','0','0','0','0','0','0',
	'1','1','1','1','1','1','1','1','1','1','1',
	'2','2','2','2','2','2','2','2','2','2','2',
	'3','3','3','3','3','3','3','3','3','3','3',
	'4','4','4','4','4','4','4','4','4','4','4',
	'5','5','5','5','5','5','5','5','5','5','5',
	'6','6','6','6','6','6','6','6','6','6','6',
	'7','7','7','7','7','7','7','7','7','7','7',
	'8','8','8','8','8','8','8','8','8','8','8',
	'9','9','9','9','9','9','9','9','9','9','9',
	'.','.','.','.','.','.','.','.','.','.','.',
} ;
static UCHAR dig2map[121] = {
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
	'0','1','2','3','4','5','6','7','8','9','.',
};
#if OS_UNIX && defined(__IBM390)
static UCHAR asciiflag = TRUE;
#endif

/* routine declarations */
static void dspcmd(CHAR *);
static void chgblksz(void);
static void dcrcmd(void);
static void eofblk(void);
static void fillcmd(void);
static void inccmd(void);
static void chgfill(void);
static void modcmd(void);
static void newfile(void);
static void page(void);
static void cleanup(void);
static void readcmd(void);
static void search(void);
static void uncmprss(void);
static void writeblk(void);
static void help(void);
static void dspopts(void);
static void readblk(void);
static void showblk(void);
static void chkmod(void);
static INT guess_blksize(void);
static INT getstring(UCHAR *);
static OFFSET getoffset(void);
static INT getone(void);
static INT getyn(void);
static INT kbdget(CHAR *, INT);
static INT charget(INT);
static void dmpctox(INT, CHAR *);
static void dputmsg(INT);
static void usage(void);
static void death(INT, INT, CHAR *);
static void quitsig(INT);


INT main(INT argc, CHAR *argv[])
{
	INT i1, quitflg, novidflag;
	CHAR termdef[MAX_NAMESIZE], indevice[MAX_NAMESIZE], outdevice[MAX_NAMESIZE];
	CHAR keymap[256], ukeymap[256], lkeymap[256];
	CHAR *ptr, cfgname[MAX_NAMESIZE];
	UCHAR c;
	FIOPARMS parms;
	VIDPARMS vidparms;

	arginit(argc, argv, &i1);
	signal(SIGINT, quitsig);

	/* initialize */
	if (meminit(32 << 10, 0, 64) == -1) death(DEATH_INIT, ERR_NOMEM, NULL);
	cfgname[0] = 0;
	while (!argget(ARG_NEXT | ARG_IGNOREOPT, (char *) buffer, sizeof(buffer))) {
		if (buffer[0] == '-') {
			if (buffer[1] == '?') usage();
			if (toupper(buffer[1]) == 'C' && toupper(buffer[2]) == 'F' &&
			    toupper(buffer[3]) == 'G' && buffer[4] == '=') strcpy(cfgname, (char *) &buffer[5]);
		}
	}
	if (cfginit(cfgname, FALSE)) death(DEATH_INIT, 0, cfggeterror());
	if (prpinit(cfggetxml(), CFG_PREFIX "cfg")) ptr = fioinit(NULL, FALSE);
	else ptr = fioinit(&parms, FALSE);
	if (ptr != NULL) death(DEATH_INIT, 0, ptr);

	ptr = (CHAR *) buffer;
	vidflg = FALSE;
	recflg = TRUE;
	blksize = 0;
	blkflg = TRUE;
	modflg = FALSE;
	inblksz = 0;
	filepos = curblk = 0;
	offset = 0;
	novidflag = FALSE;
	openflg = FIO_M_EXC | FIO_P_TXT;

	/* scan input file name */
	i1 = argget(ARG_FIRST, filename, sizeof(filename));
	if (i1 < 0) death(DEATH_INIT, i1, NULL);
	if (i1 == 1) filename[0] = 0;
	else if (filename[0] == '-') {
		strcpy(ptr, filename);
		filename[0] = 0;
		goto scanparm;
	}

	/* get parameters */
	for ( ; ; ) {
		i1 = argget(ARG_NEXT, ptr, sizeof(buffer));
		if (i1) {
			if (i1 < 0) death(DEATH_INIT, i1, NULL);
			break;
		}
scanparm:
		if (ptr[0] == '-') {
			switch (toupper(ptr[1])) {
				case '!':
					break;
				case 'B':
					if (ptr[2] == '=') {
						blksize = atoi(&ptr[3]);
						if (blksize < 0 || blksize > DUMPSIZ) death(DEATH_INVPARMVAL, 0, ptr);
					}
					if (!blksize) blksize = 400;
					recflg = FALSE;
					break;
				case 'C':
					if (toupper(ptr[2]) != 'F' || toupper(ptr[3]) != 'G' || ptr[4] != '=' || !ptr[5]) death(DEATH_INVPARM, 0, ptr);
					break;
				case 'J':
					if (toupper(ptr[2]) == 'R') openflg = FIO_M_SRO | FIO_P_TXT;
					else openflg = FIO_M_SHR | FIO_P_TXT;
					break;
				case 'V':
					novidflag = TRUE;
					break;
				default:
					death(DEATH_INVPARM, 0, ptr);
			}
		}
		else death(DEATH_INVPARM, 0, ptr);
	}

	for (i1 = 0; i1 <= 255; i1++) keymap[i1] = (CHAR) i1;
	memcpy(ukeymap, keymap, 256);
	memcpy(lkeymap, keymap, 256);
	for (i1 = 'A'; i1 <= 'Z'; i1++) {
		ukeymap[tolower(i1)] = (CHAR) i1;
		lkeymap[i1] = (CHAR) tolower(i1);
	}

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
	if ( (i1 = vidinit(&vidparms, events[0])) ) death(DEATH_VIDEO, i1, NULL);
	vidflg = TRUE;

	/* set up ending keys */
	memset(fmap, 0xFF, VID_MAXKEYVAL / 8 + 1);
	vidkeyinfinishmap(fmap);

	vidcmd[cmdcnt++] = VID_CURSOR_OFF;
	vidcmd[cmdcnt++] = VID_CURSOR_BLOCK;

	/* open the input file */
	if (filename[0]) {
		miofixname(filename, ".txt", FIXNAME_EXT_ADD);
		handle = fioopen(filename, openflg);
		if (handle < 0) death(DEATH_OPEN, handle, filename);
		if (recflg) blksize = guess_blksize();  /* guess block size using first record */
		readblk();
		showblk();
	}
	else handle = 0;

	/* dump program main option loop */
	quitflg = FALSE;
	while (!quitflg) {
		vidcmd[cmdcnt++] = VID_HORZ | 0;
		vidcmd[cmdcnt++] = VID_VERT | 0;
		vidcmd[cmdcnt++] = VID_EL;
		dspcmd("CMD: ");
		c = getone();
		switch(c) {
#if OS_UNIX && defined(__IBM390)
			case 'A':
				asciiflag = !asciiflag;
				dspopts();
				if (dspflg) showblk();
				break;
#endif
			case 'B':
				chgblksz();
				break;
			case 'C':
				hexflg = FALSE;
				dspopts();
				break;
			case 'D':
				dcrcmd();
				break;
			case 'E':
				eofblk();
				break;
			case 'F':
				fillcmd();
				break;
			case 'H':
				hexflg = TRUE;
				dspopts();
				break;
			case 'I':
				inccmd();
				break;
			case 'K':
				if (!handle) {
					dputmsg(7);
					break;
				}
				blkflg = TRUE;
				curblk = filepos / blksize;
				if (filepos != curblk * blksize) {
					if (inblksz) chkmod();
					filepos = curblk * blksize;
					readblk();
				}
				if (dspflg) showblk();
				dspopts();
				break;
			case 'L':
				chgfill();
				break;
			case 'M':
				modcmd();
				break;
			case 'N':
				newfile();
				break;
			case 'O':
				if (!handle) {
					dputmsg(7);
					break;
				}
				blkflg = FALSE;
				if (dspflg) showblk();
				dspopts();
				break;
			case 'P':
				page();
				break;
			case 'Q':
				quitflg = TRUE;
				break;
			case 'R':
				readcmd();
				break;
			case 'S':
				search();
				break;
			case 'U':
				uncmprss();
				break;
			case 'W':
				writeblk();
				break;
			case '?':
				help();
				break;
			default:
				dputmsg(1);
				break;
		}
	}
	cleanup();
	cfgexit();
	exit(0);
	return(0);
}

static void chgblksz()  /* change the block size */
{
	OFFSET lwork;

	vidcmd[cmdcnt++] = VID_HORZ | 10;
	vidcmd[cmdcnt++] = VID_VERT | 0;
	vidcmd[cmdcnt++] = VID_EL;
	dspcmd("Enter new block size: ");
	lwork = getoffset();
	if (lwork) blksize = (INT) lwork;
	if (handle) {
		curblk = filepos / blksize;
		readblk();
		showblk();
	}
	dspopts();
}

static void dcrcmd()  /* decrement the block number and display it */
{
	if (!handle) {
		dputmsg(7);
		return;
	}
	chkmod();
	filepos -= blksize;
	if (filepos < 0) filepos = 0;
	curblk = filepos / blksize;
	readblk();
	showblk();
}

static void eofblk()  /* go to EOF block */
{
	OFFSET lwork;

	if (!handle) {
		dputmsg(7);
		return;
	}
	chkmod();
	fiogetsize(handle, &lwork);
	curblk = lwork / blksize;
	if (curblk && (curblk * blksize) == lwork) curblk--;
	filepos = curblk * blksize;
	lwork -= filepos;
	offset = (INT)((lwork / 400) * 400);
	if (offset && offset == (INT)lwork) offset -= 400;
	readblk();
	showblk();
}

static void fillcmd()  /* fill all bytes in the current block to a value */
{
	INT i1;

	if (!handle) {
		dputmsg(7);
		return;
	}
	if (!inblksz) {
		dputmsg(3);
		return;
	}
	for (i1 = 0; i1 < inblksz; ) buffer[i1++] = fillchar;
	modflg = TRUE;
	offset = 0;
	showblk();
}

static void inccmd()  /* increment the block number and display it */
{
	if (!handle) {
		dputmsg(7);
		return;
	}
	chkmod();
	filepos += blksize;
	curblk++;
	readblk();
	showblk();
}

static void chgfill()
{
	INT c1, c2;
	CHAR work[64];

	vidcmd[cmdcnt++] = VID_HORZ | 10;
	vidcmd[cmdcnt++] = VID_VERT | 0;
	vidcmd[cmdcnt++] = VID_EL;
	dspcmd("Enter fill character: ");
	vidcmd[cmdcnt] = VID_END_FLUSH;
	vidput(vidcmd);
	cmdcnt = 0;
	fieldlen = kbdget(work, 63);
	work[fieldlen] = 0;
	if (!fieldlen) return;
	fillchar = work[0];
#if OS_UNIX && defined(__IBM390)
	if (!asciiflag) asciitoebcdic(&fillchar, 1);
#endif
	if (fieldlen == 4 && work[0] == '0' && (work[1] == 'x' || work[1] == 'X')) memmove(work, &work[1], fieldlen--);
	if (fieldlen == 3) {
		c1 = work[1];
		c2 = work[2];
		if ((work[0] == 'x' || work[0] == 'X') && isxdigit(c1) && isxdigit(c2)) {
			if (isdigit(c1)) c1 -= '0';
			else c1 = toupper(c1) - 'A' + 10;
			if (isdigit(c2)) c2 -= '0';
			else c2 = toupper(c2) - 'A' + 10;
			fillchar = (UCHAR)(c1 * 16 + c2);
		}
		else if (work[0] == '#' && (isdigit(c1) || c1 == '.') && (isdigit(c2) || c2 == '.')) {
			if (c1 == '.') fillchar = 238;
			else fillchar = (UCHAR)(128 + (c1 - '0') * 11);
			if (c2 == '.') fillchar += 10;
			else fillchar += (UCHAR)(c2 - '0');
		}
	}
	dspopts();
	vidcmd[cmdcnt] = VID_END_FLUSH;
	vidput(vidcmd);
	cmdcnt = 0;
}

static void modcmd()  /* modify the current block */
{
	INT i1, i2;
	OFFSET size;
	CHAR work[128];

	if (!handle) {
		dputmsg(7);
		return;
	}
	if (!inblksz) {
		fiogetsize(handle, &size);
		if (filepos != size) {
			dputmsg(3);
			return;
		}
	}
	vidcmd[cmdcnt++] = VID_HORZ | 10;
	vidcmd[cmdcnt++] = VID_VERT | 0;
	vidcmd[cmdcnt++] = VID_EL;
	if (inblksz) {
		dspcmd("Enter the offset to start modify at: ");
		i1 = (INT) getoffset();
		if (i1 < 0 || i1 > inblksz) {
			dputmsg(4);
			return;
		}
	}
	else i1 = 0;
	offset = (i1 / 400) * 400;
	vidcmd[cmdcnt++] = VID_HORZ | 10;
	vidcmd[cmdcnt++] = VID_VERT | 0;
	vidcmd[cmdcnt++] = VID_EL;
	if (hexflg) dspcmd("Enter hex bytes: ");
	else dspcmd("Enter string: ");
	if ( (i2 = getstring((UCHAR *) work)) ) {
		if (i1 + i2 > blksize) i2 = blksize - i1;
#if OS_UNIX && defined(__IBM390)
		if (!hexflg && !asciiflag) asciitoebcdic((UCHAR *) work, i2);
#endif
		memcpy(&buffer[i1], work, i2);
		if (i1 + i2 > inblksz) inblksz = i1 + i2;
		modflg = TRUE;
		showblk();
	}
	vidcmd[cmdcnt] = VID_END_FLUSH;
	vidput(vidcmd);
	cmdcnt = 0;
}

static void newfile()  /* change to new dump file */
{
	INT i1, newhndl;
	CHAR newname[100];

	if (handle) chkmod();
	vidcmd[cmdcnt++] = VID_HORZ | 10;
	vidcmd[cmdcnt++] = VID_VERT | 0;
	vidcmd[cmdcnt++] = VID_EL;
	dspcmd("Enter new file name to dump: ");
	vidcmd[cmdcnt] = VID_END_FLUSH;
	vidput(vidcmd);
	cmdcnt = 0;
	i1 = kbdget(newname, 99);
	if (!i1) return;

	/* open the new input file */
	newname[i1] = 0;
	miofixname(newname, ".txt", FIXNAME_EXT_ADD);
	newhndl = fioopen(newname, openflg);
	if (newhndl < 0) {
		vidcmd[cmdcnt++] = VID_BEEP | 3;
		vidcmd[cmdcnt++] = VID_HORZ | 0;
		vidcmd[cmdcnt++] = VID_VERT | 1;
		vidcmd[cmdcnt++] = VID_EL;
		dspcmd("Unable to open : ");
		dspcmd(fioerrstr(newhndl));
		dspcmd(": ");
		dspcmd(newname);
		return;
	}
	if (handle) fioclose(handle);
	handle = newhndl;
	strcpy(filename, newname);
	inblksz = 0;
	filepos = curblk = 0;
	offset = 0;
	if (recflg) blksize = guess_blksize();  /* guess block size using first record */
	readblk();
	showblk();
	vidcmd[cmdcnt] = VID_END_FLUSH;
	vidput(vidcmd);
	cmdcnt = 0;
}

static void page()  /* display another page of the block */
{
	if (!handle) {
		dputmsg(7);
		return;
	}
	offset += 400;
	if (offset >= inblksz) offset = 0;
	showblk();
}

static void cleanup()  /* end of program */
{
	if (handle) {
		chkmod();
		fioclose(handle);
	}
	vidcmd[cmdcnt++] = VID_HORZ | 0;
	vidcmd[cmdcnt++] = VID_VERT | 23;
	vidcmd[cmdcnt++] = VID_END_FLUSH;
	vidput(vidcmd);
	cmdcnt = 0;
	videxit();
}

static void readcmd()  /* get the block number, read the block and display it */
{
	if (!handle) {
		dputmsg(7);
		return;
	}
	chkmod();
	if (blkflg) {
		vidcmd[cmdcnt++] = VID_HORZ | 10;
		vidcmd[cmdcnt++] = VID_VERT | 0;
		vidcmd[cmdcnt++] = VID_EL;
		dspcmd("Enter block number: ");
		curblk = getoffset();
		filepos = curblk * blksize;
	}
	else {
		vidcmd[cmdcnt++] = VID_HORZ | 10;
		vidcmd[cmdcnt++] = VID_VERT | 0;
		vidcmd[cmdcnt++] = VID_EL;
		dspcmd("Enter file position: ");
		filepos = getoffset();
	}
	readblk();
	showblk();
	vidcmd[cmdcnt] = VID_END_FLUSH;
	vidput(vidcmd);
	cmdcnt = 0;
}

/* SEARCH */
/* search for a character string */
static void search()
{
	INT i1, i2, i3, i4, i5;
	OFFSET pos;
	CHAR numb[17];
	UCHAR c, work[128];

	if (!handle) {
		dputmsg(7);
		return;
	}
	chkmod();
	vidcmd[cmdcnt++] = VID_HORZ | 10;
	vidcmd[cmdcnt++] = VID_VERT | 0;
	vidcmd[cmdcnt++] = VID_EL;
	if (blkflg) {
		dspcmd("Enter block number to start search at: ");
		pos = getoffset() * blksize;
	}
	else {
		dspcmd("Enter file position to start search at: ");
		pos = getoffset();
	}
	vidcmd[cmdcnt++] = VID_HORZ | 10;
	vidcmd[cmdcnt++] = VID_VERT | 0;
	vidcmd[cmdcnt++] = VID_EL;
	if (hexflg) dspcmd("Enter search string in hex bytes: ");
	else dspcmd("Enter search string: ");
	if (!(i3 = getstring(work))) return;
#if OS_UNIX && defined(__IBM390)
	if (!hexflg && !asciiflag) asciitoebcdic(work, i3);
#endif

	c = work[0];
	while (TRUE) {
		i2 = fioread(handle, pos, buffer, DUMPSIZ);
		if (i2 < 0) death(DEATH_READ, i2, NULL);
		if (i2 < i3) {
			vidcmd[cmdcnt++] = VID_HORZ | 0;
			vidcmd[cmdcnt++] = VID_VERT | 1;
			vidcmd[cmdcnt++] = VID_EL;
			vidcmd[cmdcnt++] = VID_BEEP | 2;
			dspcmd("End of file reached.");
			readblk();
			return;
		}
		i2 -= i3 - 1;
		for (i1 = 0; i1 < i2; i1++) {
			if (buffer[i1] == c) {
				for (i5 = 1; i5 < i3 && buffer[i1 + i5] == work[i5]; i5++);
				if (i5 == i3) {
					if (blkflg) {
						curblk = (pos + i1) / blksize;
						filepos = curblk * blksize;
						i4 = (INT)(pos + i1 - filepos);
						offset = (i4 / 400) * 400;
					}
					else {
						filepos = pos + i1;
						offset = 0;
					}
					readblk();
					showblk();
					vidcmd[cmdcnt++] = VID_HORZ | 0;
					vidcmd[cmdcnt++] = VID_VERT | 1;
					dspcmd("Found");
					if (blkflg) {
						dspcmd(" at offset ");
						mscitoa(i4, numb);
						dspcmd(numb);
					}
					dspcmd(".  Continue search? ");
					if (getyn() == 'N') return;
				}
			}
		}
		pos += i2;
		vidcmd[cmdcnt++] = VID_HORZ | 0;
		vidcmd[cmdcnt++] = VID_VERT | 1;
		vidcmd[cmdcnt++] = VID_EL;
		dspcmd("Searching at position ");
		mscofftoa(pos, numb);
		dspcmd(numb);
	}
}

static void uncmprss()  /* uncompress a section of a DB/C compressed file */
{
	INT i1, i2, i3;
	CHAR work[128];

	if (!handle) {
		dputmsg(7);
		return;
	}
	vidcmd[cmdcnt++] = VID_HORZ | 10;
	vidcmd[cmdcnt++] = VID_VERT | 0;
	vidcmd[cmdcnt++] = VID_EL;
	dspcmd("Enter the offset to start at: ");
	offset = (INT) getoffset();
	if (offset < 0 || offset > inblksz) {
		dputmsg(4);
		return;
	}
	for (i1 = offset, i2 = 0; i2 < 80 && buffer[i1] != 0xfa && i1 < inblksz; ) {
		/* normal chars */
		if (buffer[i1] < 0x80) work[i2++] = buffer[i1++];
		/* space compression */
		else if (buffer[i1] == 0xf9) {
			if (i1 < inblksz) {
				i3 = buffer[i1 + 1];
				if (i3 + i2 > 80) i3 = 80 - i2;
				memset(&work[i2], ' ', i3);
				i2 += i3;
				i1 += 2;
			}
			else i1++;
		}
		/* digit compression */
		else if (buffer[i1] < 0xf9) {
			work[i2++] = dig1map[buffer[i1] - 128];
			work[i2++] = dig2map[buffer[i1] - 128];
			i1++;
		}
	}
	work[i2] = 0;
	vidcmd[cmdcnt++] = VID_HORZ | 0;
	vidcmd[cmdcnt++] = VID_VERT | 1;
	vidcmd[cmdcnt++] = VID_EL;
	dspcmd(work);
}

static void writeblk()  /* write the current block to disk */
{
	INT i1;

	if (!handle) {
		dputmsg(7);
		return;
	}
	if (!modflg) {
		dputmsg(2);
		return;
	}
	if (!inblksz) {
		dputmsg(3);
		return;
	}
	i1 = fiowrite(handle, filepos, buffer, inblksz);
	if (i1 < 0) death(DEATH_WRITE, i1, NULL);
	vidcmd[cmdcnt++] = VID_HORZ | 10;
	vidcmd[cmdcnt++] = VID_VERT | 0;
	vidcmd[cmdcnt++] = VID_EL;
	dspcmd("Written");
	modflg = FALSE;
}

static void help()  /* display help information */
{
	dspflg = FALSE;
	vidcmd[cmdcnt++] = VID_HORZ | 0;
	vidcmd[cmdcnt++] = VID_VERT | 1;
	vidcmd[cmdcnt++] = VID_EF;
	dspopts();
	dspcmd("The commands available are:");
	vidcmd[cmdcnt++] = VID_NL;
#if OS_UNIX && defined(__IBM390)
	dspcmd("   A = toggle ASCII/EBCDIC mode");
	vidcmd[cmdcnt++] = VID_NL;
#endif
	dspcmd("   B = change BLOCK SIZE");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   C = change to CHARACTER mode");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   D = DECREMENT the current block pointer and display that block");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   E = go to EOF");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   F = FILL all bytes in the current block with fill character");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   H = change to HEX mode");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   I = INCREMENT the current block pointer and display that block");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   K = read by block #");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   L = change fill character");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   M = MODIFY bytes in the current block");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   N = specify a NEW FILE to dump");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   O = read by position");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   P = show another PAGE of the block (for blocks > 400 bytes)");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   Q = QUIT and exit to operating system");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   R = READ in a block of data and display it");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   S = SEARCH the file for occurrence of a string");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   U = UNCOMPRESS a portion of the current block");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   W = WRITE out the current block");
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("   ? = display this screen");
	vidcmd[cmdcnt++] = VID_NL;
	vidcmd[cmdcnt] = VID_END_FLUSH;
	vidput(vidcmd);
	cmdcnt = 0;
}

static void dspopts()
{
	CHAR work[17];

	vidcmd[cmdcnt++] = VID_HORZ | 0;
	vidcmd[cmdcnt++] = VID_VERT | 1;
	vidcmd[cmdcnt++] = VID_EL;
	dspcmd("Options:  Blocksize = ");
	mscitoa(blksize, work);
	dspcmd(work);
	if (blkflg) dspcmd(", Read by block");
	else dspcmd(", Read by position");
	if (hexflg) dspcmd(", Hex mode");
	else dspcmd(", Character mode");
#if OS_UNIX && defined(__IBM390)
	dspcmd(", Fill = 0x");
#else
	dspcmd(", Fill char = 0x");
#endif
	dmpctox(fillchar, work);
	dspcmd(work);
#if OS_UNIX && defined(__IBM390)
	if (asciiflag) dspcmd(", ASCII");
	else dspcmd(", EBCDIC");
#endif
	vidcmd[cmdcnt++] = VID_NL;
	vidcmd[cmdcnt] = VID_END_FLUSH;
	vidput(vidcmd);
	cmdcnt = 0;
}

static void readblk()  /* read the current block */
{
	if (handle) {
		inblksz = fioread(handle, filepos, buffer, blksize);
		if (inblksz < 0) death(DEATH_READ, inblksz, NULL);
	}
	else inblksz = 0;
	if (offset >= inblksz) offset = 0;
}

static void showblk()  /* display the current block */
{
	INT i1, i2, i3, i4;
	CHAR work[100], work3[3];
	UCHAR c1;

	dspflg = TRUE;
	vidcmd[cmdcnt++] = VID_HORZ | 0;
	vidcmd[cmdcnt++] = VID_VERT | 1;
	vidcmd[cmdcnt++] = VID_EF;
	vidcmd[cmdcnt++] = VID_NL;
	dspcmd("filepos: ");
	mscofftoa(filepos, work);
	dspcmd(work);
	if (blkflg) {
		dspcmd("   block: ");
		mscofftoa(curblk, work);
		dspcmd(work);
	}
	dspcmd("   blksize: ");
	mscitoa(blksize, work);
	dspcmd(work);
	dspcmd("   file: ");
	dspcmd(filename);
	vidcmd[cmdcnt++] = VID_NL;
	if (!inblksz) {
		vidcmd[cmdcnt++] = VID_NL;
		dspcmd("Nothing to display (EOF or error occurred)");
		vidcmd[cmdcnt++] = VID_NL;
		return;
	}
	if ( (i1 = offset % 400) ) offset -= i1;
	offset = (offset / 20) * 20;
	for (i1 = 0; i1 < 20; i1++) {
		if ((offset + i1 * 20) >= inblksz) return;
		msciton(offset + i1 * 20, (UCHAR *) work, 5);
		work[5] = work[6] = ' ';
		work[7] = '*';
		for (i2 = offset + i1 * 20, i3 = 0, i4 = 8; i3 < 20; i3++) {
			if (i2 < inblksz) {
				dmpctox(buffer[i2++], work3);
				work[i4++] = work3[0];
				work[i4++] = work3[1];
				if (i3 == 4 || i3 == 9 || i3 == 14) work[i4++] = '-';
			}
			else {
				work[i4++] = ' ';
				work[i4++] = ' ';
				if (i3 == 4 || i3 == 9 || i3 == 14) work[i4++] = ' ';
			}
		}
		work[i4++] = '*';
		work[i4++] = ' ';
		work[i4++] = ' ';
		work[i4++] = '*';
		for (i2 = offset + i1 * 20, i3 = 0; i3 < 20; i2++, i3++) {
			c1 = buffer[i2];
#if OS_UNIX && defined(__IBM390)
			if (!asciiflag) ebcdictoascii(&c1, 1);
#endif
			if (c1 > 31 && c1 < 127 && i2 < inblksz) work[i4++] = (CHAR) c1;
			else work[i4++] = ' ';
		}
		work[i4++] = '*';
		work[i4] = 0;
		vidcmd[cmdcnt++] = VID_NL;
		dspcmd(work);
	}
	vidcmd[cmdcnt] = VID_END_FLUSH;
	vidput(vidcmd);
	cmdcnt = 0;
}

static void chkmod()  /* check for current block modified */
{
	if (!modflg) return;
	vidcmd[cmdcnt++] = VID_HORZ | 10;
	vidcmd[cmdcnt++] = VID_VERT | 0;
	vidcmd[cmdcnt++] = VID_EL;
	dspcmd("Do you want the pending modifications written to disk ? ");
	if (getyn() == 'Y') writeblk();
	else modflg = FALSE;
}

static INT guess_blksize()  /* make an educated guess at the block size */
{
	INT i1, i2, cmp;
	OFFSET pos;
	UCHAR c;

#if VMS
/*** LOOK AT W_MRS IF NOT DB/C TYPE FILE ***/
#endif
	pos = 0L;
	i2 = fioread(handle, pos, buffer, DUMPSIZ);
	if (i2 < 0) death(DEATH_READ, i2, NULL);
	if (!i2) return(400);
	c = buffer[0];
	if (c == 0x7f || c == 0xff) {  /* at least first record is deleted */
		for (i1 = 0; c == buffer[i1]; ) {
			if (++i1 == i2) {
				pos += i1;
				i2 = fioread(handle, pos, buffer, DUMPSIZ);
				if (i2 < 0) death(DEATH_READ, i2, NULL);
				if (!i2 || pos >= 65536) return(400);
				i1 = 0;
			}
		}
		pos += i1;
		i2 = fioread(handle, pos, buffer, DUMPSIZ);
		if (i2 < 0) death(DEATH_READ, i2, NULL);
	}
	cmp = FALSE;
	for (i1 = 0; i1 < i2; i1++) {  /* look for EOR character */
		if (buffer[i1] == 0xfa) {
			if (cmp) break;
			return(i1 + 1);
		}
		if (buffer[i1] == 0x0d) {
			if (i1 + 1 < i2 && buffer[i1 + 1] == 0x0a) i1++;
			return(i1 + 1);
		}
		if (buffer[i1] == 0x0a) return(i1 + 1);
		if (buffer[i1] >= 0x80) cmp = TRUE;
#if OS_UNIX && defined(__IBM390)
		if (buffer[i1] == 0x15) {
			asciiflag = FALSE;
			return(i1 + 1);
		}
#endif
	}
	return(400);
}

/* GETSTRING */
/* input a string of characters, and return its length */
static INT getstring(UCHAR *s)
{
	INT c1, c2, i1, i2, i3;
	CHAR work[128];

	vidcmd[cmdcnt] = VID_END_FLUSH;
	vidput(vidcmd);
	cmdcnt = 0;
	i3 = kbdget(work, 127);
	work[i3] = 0;
	if (hexflg) {
		for (i1 = 0, i2 = 0; i1 < i3 - 1; i2++) {
			if (work[i1] != '#') {
				c1 = work[i1++];
				c2 = work[i1++];
				if (!isxdigit(c1) || !isxdigit(c2)) {
					dputmsg(6);
					return(0);
				}
				if (isdigit(c1)) c1 -= '0';
				else c1 = toupper(c1) - 'A' + 10;
				if (isdigit(c2)) c2 -= '0';
				else c2 = toupper(c2) - 'A' + 10;
				s[i2] = (UCHAR)(c1 * 16 + c2);
			}
			else {
				if (++i1 == i3 - 1) break;
				c1 = work[i1++];
				c2 = work[i1++];
				if ((!isdigit(c1) && c1 != '.') || (!isdigit(c2) && c2 != '.')) {
					dputmsg(6);
					return(0);
				}
				if (c1 == '.') s[i2] = 238;
				else s[i2] = (UCHAR)(128 + (c1 - '0') * 11);
				if (c2 == '.') s[i2] += 10;
				else s[i2] += (UCHAR)(c2 - '0');
			}
		}
		if (i1 < i3) dputmsg(5);
		return(i2);
	}
	else {
		strcpy((CHAR *) s, work);
		return(i3);
	}
}

/* GETOFFSET */
/* input a long and return it */
static OFFSET getoffset() // @suppress("No return")
{
	INT i1, i2;
	OFFSET lwork;
	CHAR work[64];

	while (TRUE) {
		if (cmdcnt) {
			vidcmd[cmdcnt] = VID_END_FLUSH;
			vidput(vidcmd);
			cmdcnt = 0;
		}
		i1 = kbdget(work, 63);
		for (i2 = 0, lwork = 0; i2 < i1; i2++) {
			if (work[i2] == ' ') continue;
			if (!isdigit(work[i2])) break;
			lwork = lwork * 10 + work[i2] - '0';
		}
		if (i2 == i1) return(lwork);
		vidcmd[cmdcnt++] = VID_BEEP | 3;
		vidcmd[cmdcnt++] = VID_HORZ | 0;
		vidcmd[cmdcnt++] = VID_VERT | 1;
		vidcmd[cmdcnt++] = VID_EL;
		dspcmd("Please enter a valid number: ");
	}
}

/* GETONE */
/* get a one character command */
static INT getone()
{
	INT i1;

	vidcmd[cmdcnt++] = VID_CURSOR_ON;
	vidcmd[cmdcnt++] = VID_KBD_UC;
	vidcmd[cmdcnt] = VID_END_FLUSH;
	vidput(vidcmd);
	cmdcnt = 0;
	i1 = charget(TRUE);
	if (i1 >= 32 && i1 < 256) vidcmd[cmdcnt++] = VID_DISP_CHAR | (USHORT) i1;
	vidcmd[cmdcnt++] = VID_KBD_IN;
	vidcmd[cmdcnt] = VID_END_FLUSH;
	vidput(vidcmd);
	cmdcnt = 0;
	return(i1);
}

/* GETYN */
/* get a one character command, must be Y or N */
static INT getyn() // @suppress("No return")
{
	INT i1;

	while (TRUE) {
		i1 = getone();
		if (i1 == 'Y' || i1 == 'N') return(i1);
		vidcmd[cmdcnt++] = VID_BEEP | 3;
		vidcmd[cmdcnt++] = VID_HORZ | 0;
		vidcmd[cmdcnt++] = VID_VERT | 1;
		vidcmd[cmdcnt++] = VID_EL;
		dspcmd("Please respond with Y or N: ");
	}
}

/* KBDGET */
static INT kbdget(CHAR *s, INT len)
{
	INT i1, i2;

	vidcmd[cmdcnt++] = VID_CURSOR_ON;
	vidcmd[cmdcnt] = VID_END_FLUSH;
	vidput(vidcmd);
	cmdcnt = 0;
	i1 = 0;
	while ((i2 = charget(TRUE)) != VID_ENTER) {
		if (i2 == VID_BKSPC) {
			if (i1) {
				i1--;
				vidcmd[cmdcnt++] = VID_HORZ_ADJ | (USHORT) -1;
				vidcmd[cmdcnt++] = VID_DISP_CHAR | ' ';
				vidcmd[cmdcnt++] = VID_HORZ_ADJ | (USHORT) -1;
			}
		}
		else if (i2 >= 32 && i2 < 256 && i1 < len) {
			s[i1++] = (CHAR) i2;
			vidcmd[cmdcnt++] = VID_DISP_CHAR | (USHORT) i2;
		}
		else vidcmd[cmdcnt++] = VID_BEEP | 3;
		vidcmd[cmdcnt] = VID_END_FLUSH;
		vidput(vidcmd);
		cmdcnt = 0;
	}
	return(i1);
}

static INT charget(INT cursflg)
{
	INT i1, i2;
	INT32 vidcmd[2];
	UCHAR work[8];

	if (cursflg) vidcmd[0] = VID_CURSOR_ON;
	else vidcmd[0] = VID_CURSOR_OFF;
	vidcmd[1] = VID_END_FLUSH;
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

static void dmpctox(INT n, CHAR* s)  /* convert a character to 2 hexadecimal digits */
{
	INT i1;

	i1 = n >> 4;
	if (i1 > 9) s[0] = (UCHAR)(i1 - 10 + 'A');
	else s[0] = (UCHAR)(i1 + '0');
	i1 = n & 0x0f;
	if (i1 > 9) s[1] = (UCHAR)(i1 - 10 + 'A');
	else s[1] = (UCHAR)(i1 + '0');
	s[2] = 0;
}

static void dputmsg(INT n)  /* print out a message */
{
	vidcmd[cmdcnt++] = VID_BEEP | 3;
	vidcmd[cmdcnt++] = VID_HORZ | 0;
	vidcmd[cmdcnt++] = VID_VERT | 1;
	vidcmd[cmdcnt++] = VID_EL;
	switch (n) {
		case 1:
			dspcmd("Error - invalid command");
			break;
		case 3:
			dspcmd("No modifications have been made to the current block");
			break;
		case 4:
			dspcmd("There is no current block");
			break;
		case 5:
			dspcmd("Invalid offset entered");
			break;
		case 6:
			dspcmd("An even number of hexadecimal characters must be entered");
			break;
		case 7:
			dspcmd("Hexadecimal characters only must be entered");
			break;
		case 8:
			dspcmd("There is no current file");
			break;
	}
	vidcmd[cmdcnt] = VID_END_FLUSH;
	vidput(vidcmd);
	cmdcnt = 0;
}

static void dspcmd(CHAR *s)
{
	vidcmd[cmdcnt++] = VID_DISPLAY | (INT32)strlen(s);
	if (sizeof(void *) > sizeof(INT32)) {
		memcpy((void *) &vidcmd[cmdcnt], (void *) &s, sizeof(void *));
		cmdcnt += (sizeof(void *) + sizeof(INT32) - 1) / sizeof(INT32);
	}
	else *(UCHAR **)(&vidcmd[cmdcnt++]) = (UCHAR *) s;
	vidcmd[cmdcnt] = VID_END_FLUSH;
	vidput(vidcmd);
	cmdcnt = 0;
}

/* USAGE */
static void usage()
{
	dspstring("DUMP command  " RELEASEPROGRAM RELEASE COPYRIGHT);
	dspchar('\n');
	dspstring("Usage:  dump file [-B[=n]] [-J[R]] [-O=optfile] [-V]\n");
	exit(1);
}

/* DEATH */
static void death(INT n, INT e, CHAR *s)
{
#if OS_WIN32
	INT eventid;
#endif
	CHAR work[17];

	if (vidflg) {
		vidcmd[cmdcnt++] = VID_HORZ | 0;
		vidcmd[cmdcnt++] = VID_VERT | 1;
		vidcmd[cmdcnt++] = VID_EF;
		if (n < (INT) (sizeof(errormsg) / sizeof(*errormsg))) dspcmd(errormsg[n]);
		else {
			mscitoa(n, work);
			dspcmd("*** UNKNOWN ERROR ");
			dspcmd(work);
			dspcmd("***");
		}
		if (e) {
			dspcmd(": ");
			dspcmd(fioerrstr(e));
		}
		if (s != NULL) {
			dspcmd(": ");
			dspcmd(s);
		}
		vidcmd[cmdcnt++] = VID_NL;
		vidcmd[cmdcnt++] = VID_HORZ | 0;
		vidcmd[cmdcnt++] = VID_VERT | 23;
		vidcmd[cmdcnt] = VID_END_FLUSH;
		vidput(vidcmd);
#if OS_WIN32
		if ((eventid = evtcreate()) != -1) {
			msctimestamp(work);
			timadd(work, 200);
			timset(work, eventid);
			evtwait(&eventid, 1);
			evtdestroy(eventid);
		}
#endif
		videxit();
	}
	else {
		if (n < (INT) (sizeof(errormsg) / sizeof(*errormsg))) dspstring(errormsg[n]);
		else {
			mscitoa(n, work);
			dspstring("*** UNKNOWN ERROR ");
			dspstring(work);
			dspstring("***");
		}
		if (e) {
			dspstring(": ");
			dspstring(fioerrstr(e));
		}
		if (s != NULL) {
			dspstring(": ");
			dspstring(s);
		}
		dspchar('\n');
	}
	cfgexit();
	exit(1);
}

/* QUITSIG */
static void quitsig(INT sig)
{
	signal(SIGINT, SIG_IGN);
	if (vidflg) {
		vidcmd[cmdcnt++] = VID_HORZ | 0;
		vidcmd[cmdcnt++] = VID_VERT | 1;
		vidcmd[cmdcnt++] = VID_EF;
		dspcmd(errormsg[DEATH_INTERRUPT]);
		vidcmd[cmdcnt++] = VID_NL;
		vidcmd[cmdcnt] = VID_END_FLUSH;
		vidput(vidcmd);
		cmdcnt = 0;
		videxit();
	}
	else {
		dspchar('\n');
		dspstring(errormsg[DEATH_INTERRUPT]);
		dspchar('\n');
	}
	cfgexit();
	exit(sig);
}
