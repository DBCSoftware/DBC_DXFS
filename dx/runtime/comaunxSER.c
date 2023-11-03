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
#define INC_SIGNAL
#define INC_CTYPE
#define INC_TYPES
#define INC_ERRNO
#include "includes.h"

#include <unistd.h>
#if defined(USE_POSIX_TERMINAL_IO)
#include <termios.h>
#else
#include <termio.h>
#endif
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>

#include "base.h"
#include "com.h"
#include "comx.h"
#include "evt.h"
#include "tim.h"

/* defines for os_serctl() */
#define COMCTL_SPEC_R 0x00000001
#define COMCTL_SPEC_r 0x00000002
#define COMCTL_SPEC_T 0x00000004
#define COMCTL_SPEC_t 0x00000008
#define COMCTL_SPEC_C 0x00000010
#define COMCTL_SPEC_c 0x00000020
#define COMCTL_SPEC_S 0x00000040
#define COMCTL_SPEC_s 0x00000080
#define COMCTL_SPEC_X 0x00000100
#define COMCTL_SPEC_x 0x00000200
#define COMCTL_SPEC_L 0x00000400
#define COMCTL_SPEC_l 0x00000800
#define COMCTL_FLOWS  0x000003F0

/* general communication routines */

/*
 * Is this ever called? I don't think so.
 */
INT os_cominit()
{
	return(0);
}

INT os_comopen(CHAR *channelname, CHANNEL *channel)
{
	if (!memcmp(channelname, "/dev/", 5)) {  /* backward compatibility support */
		channel->channeltype = CHANNELTYPE_SERIAL;
		return(os_seropen(channelname, channel));
	}
	comerror(channel, 0, "UNSUPPORTED DEVICE", 0);
	return(753);
}

void os_comclose(CHANNEL *channel)
{
}

INT os_comsend(CHANNEL *channel)
{
	comerror(channel, 0, "UNSUPPORTED DEVICE", 0);
	return(753);
}

INT os_comrecv(CHANNEL *channel)
{
	comerror(channel, 0, "UNSUPPORTED DEVICE", 0);
	return(753);
}

INT os_comclear(CHANNEL *channel, INT flag, INT *status)
{
	comerror(channel, 0, "UNSUPPORTED DEVICE", 0);
	return(753);
}

INT os_comctl(CHANNEL *channel, UCHAR *msgin, INT msginlen, UCHAR *msgout, INT *msgoutlen)
{
	comerror(channel, 0, "UNSUPPORTED DEVICE", 0);
	return(753);
}

/* serial communication routines */

#define COM_STATE_START		0x0001
#define COM_STATE_END		0x0002

static INT sercallback(void *, INT);
static void serreadprocess(CHANNEL *);
static INT serparse(CHANNEL *, CHAR *);
static INT sergetparm(UCHAR *, CHAR *);

INT os_seropen(CHAR *channelname, CHANNEL *channel)
{
	UINT i1;
	INT retcode;

	if (memcmp(channelname, "/dev/", 5)) {
		comerror(channel, 0, "BAD DEVICE NAME", 0);
		return(753);
	}

	for (i1 = 0; channelname[i1] && !isspace(channelname[i1]) && channelname[i1] != ':' &&
	     channelname[i1] != ',' && channelname[i1] != ';'; i1++);

/*** CODE: DO WE NEED CHANNEL->NAME ??? ***/
/***       DO WE COMPARE TO OTHER FILES ALREADY OPENED BY OTHER CHANNELS. ***/
	if (i1 >= sizeof(channel->name)) {
		comerror(channel, 0, "DEVICE NAME TOO LONG", 0);
		return(753);
	}
	memcpy(channel->name, channelname, i1);
	channel->name[i1] = 0;
	channelname += i1;

	/* set default parameters */
	channel->recvend[0] = 1;
	channel->recvend[1] = 0x0D;		/* ^M */
	channel->sendend[0] = 2;
	channel->sendend[1] = 0x0D;		/* ^M^J */
	channel->sendend[2] = 0x0A;
	channel->recvignore[0] = 1;
	channel->recvignore[1] = 0x0A;	/* ^J */

	/* get parameters */
	retcode = serparse(channel, channelname);
	if (retcode < 0) {
		comerror(channel, 0, "INVALID PARAMETER STRING", 0);
		return(753);
	}

/*** CODE: MAYBE ADD CODE TO TIME OUT IF DEVICE OFF LINE ***/
	channel->handle = open(channel->name, O_RDWR);
	if (channel->handle == -1) {
/*** CODE: ADD SUPPORT FOR EMFILE && ENFILE ***/
		comerror(channel, 0, "OPEN FAILURE", errno);
		return(753);
	}
	channel->flags |= COM_FLAGS_OPEN;

	if(isatty(channel->handle)) {
		channel->flags |= COM_FLAGS_TTY;
#if defined(USE_POSIX_TERMINAL_IO)
		tcgetattr(channel->handle, &channel->termold);
#else
		ioctl(channel->handle, TCGETA, &channel->termold);
#endif
		memcpy((CHAR *) &channel->termnew, (CHAR *) &channel->termold, sizeof(channel->termnew));
		channel->termnew.c_iflag &= ~ICRNL;
		channel->termnew.c_iflag |= IGNBRK;
		channel->termnew.c_lflag = 0;  /* no signals, non-canonical, no echo, etc. */
		channel->termnew.c_cc[VMIN] = 0;
		channel->termnew.c_cc[VTIME] = 0;
		channel->termnew.c_oflag = 0;
	}

	/* set baud rate */
	i1 = 0;
	switch (channel->baud) {
	case 100:
		i1 = B110; break;
	case 200:
		i1 = B200; break;
	case 300:
		i1 = B300; break;
	case 600:
		i1 = B600; break;
	case 1200:
		i1 = B1200; break;
	case 1800:
		i1 = B1800; break;
	case 2400:
		i1 = B2400; break;
	case 4800:
		i1 = B4800; break;
	case 9600:
		i1 = B9600; break;
	case 19200:
		i1 = B19200; break;
#ifdef B38400
	case 38400:
		i1 = B38400; break;
#endif
#ifdef B57600
	case 57600:
		i1 = B57600; break;
#endif
#ifdef B115200
	case 115200:
		i1 = B115200; break;
#endif
	}
	if (i1) {
#if defined(__MACOSX) || defined(FREEBSD)
		cfsetispeed(&channel->termnew, (speed_t) i1);
		cfsetospeed(&channel->termnew, (speed_t) i1);
#else
		channel->termnew.c_cflag = (channel->termnew.c_cflag & ~CBAUD) | i1;
#endif
	}

	/* set parity */
	if (channel->parity == 'N') channel->termnew.c_cflag &= ~(PARENB | PARODD);
	else if (channel->parity == 'E') channel->termnew.c_cflag = (channel->termnew.c_cflag & ~PARODD) | PARENB;
	else if (channel->parity == 'O') channel->termnew.c_cflag |= PARENB | PARODD;

	/* set data bits */
	i1 = 0;
	switch (channel->databits) {
	case 5:
		i1 = CS5; break;
	case 6:
		i1 = CS6; break;
	case 7:
		i1 = CS7; break;
	case 8:
		i1 = CS8; break;
	}
	if (i1) channel->termnew.c_cflag = (channel->termnew.c_cflag & ~CSIZE) | i1;

	if (channel->stopbits == 1) channel->termnew.c_cflag &= ~CSTOPB;
	else if (channel->stopbits == 2) channel->termnew.c_cflag |= CSTOPB;

	if (channel->flags & COM_FLAGS_TTY) {
#if defined(USE_POSIX_TERMINAL_IO)
		tcsetattr(channel->handle, TCSADRAIN, &channel->termnew);
#else
		ioctl(channel->handle, TCSETAW, &channel->termnew);
#endif
		channel->flags |= COM_FLAGS_IOCTL;
	}

	if ( (i1 = evtdevinit(channel->handle, DEVPOLL_READ | DEVPOLL_HANGUP, sercallback, (void *) channel)) )
	{
		comerror(channel, 0, "EVTDEVINIT FAILURE", errno);
		os_serclose(channel);
		return(753);
	}
	channel->flags |= COM_FLAGS_DEVINIT;

	return(0);
}

void os_serclose(CHANNEL *channel)
{
/*** CODE: DO WE WAIT FOR ANY SENDS/RECV TO COMPLETE ***/
	os_serclear(channel, COM_CLEAR_SEND | COM_CLEAR_RECV, 0);

	if (channel->flags & COM_FLAGS_DEVINIT) evtdevexit(channel->handle);
#if defined(USE_POSIX_TERMINAL_IO)
	if (channel->flags & COM_FLAGS_IOCTL) tcsetattr(channel->handle, TCSADRAIN, &channel->termold);
#else
	if (channel->flags & COM_FLAGS_IOCTL) ioctl(channel->handle, TCSETAW, &channel->termold);
#endif
	if (channel->flags & COM_FLAGS_OPEN) close(channel->handle);
	channel->flags &= ~(COM_FLAGS_OPEN | COM_FLAGS_TTY | COM_FLAGS_IOCTL | COM_FLAGS_DEVINIT);
}

INT os_sersend(CHANNEL *channel)
{
	INT flags, retcode, timehandle;
	UCHAR timestamp[16];

	if (channel->status & COM_SEND_MASK) os_serclear(channel, COM_CLEAR_SEND, NULL);

	if (channel->status & COM_PER_ERROR) return(753);

	channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_PEND;

/*** CODE: MAYBE SET NONBLOCK AFTER OPEN ***/
	flags = fcntl(channel->handle, F_GETFL, 0);
	fcntl(channel->handle, F_SETFL, flags | O_NONBLOCK);
	retcode = write(channel->handle, (CHAR *) channel->sendbuf, channel->sendlength);
	fcntl(channel->handle, F_SETFL, flags);

	if (retcode >= channel->sendlength) {  /* success */
		channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_DONE;
		evtset(channel->sendevtid);
		return(0);
	}
	if (retcode == -1) {
		if (errno != EAGAIN && errno != EINTR) {
			comerror(channel, COM_SEND_ERROR, "WRITE FAILURE", errno);
			return(0);
		}
		retcode = 0;
	}
	channel->senddone = retcode;

/*** CODE: MAYBE MOVE TIMEOUT CODE TO COM.C ***/
	if (channel->sendtimeout == 0) {
		channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_TIME;
		evtset(channel->sendevtid);
		return(0);
	}
	if (channel->sendtimeout > 0) {
		msctimestamp(timestamp);
		if (timadd(timestamp, channel->sendtimeout * 100) ||
		   (timehandle = timset(timestamp, channel->sendevtid)) < 0) {
			comerror(channel, 0, "SET TIMER FAILURE", 0);
			channel->status &= ~COM_SEND_MASK;
			return(753);
		}
		if (!timehandle) {
			channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_TIME;
			evtset(channel->sendevtid);
			return(0);
		}
		channel->sendth = timehandle;
	}

	channel->devpoll |= DEVPOLL_WRITE;
	evtdevset(channel->handle, channel->devpoll);
	return(0);
}

INT os_serrecv(CHANNEL *channel)
{
	INT timehandle;
	UCHAR timestamp[16];

	if (channel->status & COM_RECV_MASK) os_serclear(channel, COM_CLEAR_RECV, NULL);

	if (channel->status & COM_PER_ERROR) return(753);

	if (channel->recvstart[0]) channel->recvstate = COM_STATE_START;
	else channel->recvstate = COM_STATE_END;

	channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_PEND;

/*** CODE: MAYBE CALL SERCALLBACK AND FORCE A READ TO HAPPEN ***/
	serreadprocess(channel);
	if (!(channel->status & COM_RECV_PEND)) return(0);

/*** CODE: MAYBE MOVE TIMEOUT CODE TO COM.C ***/
	if (channel->recvtimeout == 0) {
		channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_TIME;
		evtset(channel->recvevtid);
		return(0);
	}
	if (channel->recvtimeout > 0) {
		msctimestamp(timestamp);
		if (timadd(timestamp, channel->recvtimeout * 100) ||
		   (timehandle = timset(timestamp, channel->recvevtid)) < 0) {
			comerror(channel, 0, "SET TIMER FAILURE", 0);
			channel->status &= ~COM_RECV_MASK;
			return(753);
		}
		if (!timehandle) {
			channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_TIME;
			evtset(channel->recvevtid);
			return(0);
		}
		channel->recvth = timehandle;
	}

	channel->devpoll |= DEVPOLL_READ;
	evtdevset(channel->handle, channel->devpoll);
	return(0);
}

INT os_serclear(CHANNEL *channel, INT flag, INT *status)
{
	INT devpoll;

	devpoll = channel->devpoll;

	if (channel->status & COM_SEND_PEND) {
		if (channel->sendtimeout > 0 && evttest(channel->sendevtid)) {  /* timeout happened */
			channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_TIME;
			devpoll &= ~DEVPOLL_WRITE;
		}
	}
	if (channel->status & COM_RECV_PEND) {
		if (channel->recvtimeout > 0 && evttest(channel->recvevtid)) {  /* timeout happened */
			channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_TIME;
			devpoll &= ~DEVPOLL_READ;
		}
	}

	if (status != NULL) *status = channel->status;

	if (flag & COM_CLEAR_SEND) {
		channel->status &= ~COM_SEND_MASK;
		if (channel->sendth) {
			timstop(channel->sendth);
			channel->sendth = 0;
		}
		evtclear(channel->sendevtid);
		devpoll &= ~DEVPOLL_WRITE;
	}
	if (flag & COM_CLEAR_RECV) {
		channel->status &= ~COM_RECV_MASK;
		if (channel->recvth) {
			timstop(channel->recvth);
			channel->recvth = 0;
		}
		evtclear(channel->recvevtid);
		devpoll &= ~DEVPOLL_READ;
	}

/*** CODE: THIS TURNING OFF OF CALL BACK IS REALLY ONLY ***/
/***       NEEDED IN THE CALL BACK ROUTINE ***/
	if (devpoll != channel->devpoll) {
		evtdevset(channel->handle, devpoll);
		channel->devpoll = devpoll;
	}

	return(0);
}

INT os_serctl(CHANNEL *channel, UCHAR *msgin, INT msginlen, UCHAR *msgout, INT *msgoutlen)
{
	INT i1, i2, ctlflag, len, value;
	CHAR work[256], *msgptr;
	UCHAR newstartend[MAXSTARTEND], *ptr;

	while (msginlen > 0 && isspace(msgin[msginlen - 1])) msginlen--;
	if (msginlen >= (INT) sizeof(work)) msginlen = sizeof(work) - 1;
	for (i1 = 0; i1 < msginlen && msgin[i1] != '='; i1++)
		work[i1] = (CHAR) toupper(msgin[i1]);
	work[i1] = 0;
	if (i1++ < msginlen) {  /* has '=', assume keyword */
		if (!strcmp(work, "OUTSTART")) ptr = channel->sendstart;
		else if (!strcmp(work, "OUTEND")) ptr = channel->sendend;
		else {
			if (!strcmp(work, "INSTART")) ptr = channel->recvstart;
			else if (!strcmp(work, "INEND")) ptr = channel->recvend;
			else if (!strcmp(work, "IGNORE")) ptr = channel->recvignore;
			else if (!strcmp(work, "LENGTH")) ptr = NULL;
			else {
				comerror(channel, 0, "INVALID COMCTL REQUEST", 0);
				return(753);
			}
			if (channel->status & COM_RECV_PEND) {
				comerror(channel, 0, "COMCTL INVALID DURING PENDING RECEIVE", 0);
				return(753);
			}
		}
		if (ptr != NULL) {  /* start/end/ignore string */
			memcpy(work, msgin + i1, msginlen - i1);
			work[msginlen - i1] = 0;
			if (sergetparm(newstartend, work) < 0) {
				comerror(channel, 0, "INVALID COMCTL CHARACTER SEQUENCE", 0);
				return(753);
			}
			for (i2 = len = 0; i2++ < (INT) ptr[0]; ) {
				value = ptr[i2];
				if (value >= 32 && value <= 126) {
					if (value == '\\' || value == '^') work[len++] = '\\';
					work[len++] = value;
				}
				else if (value >= 1 && value <= 31) {
					work[len++] = '^';
					work[len++] = (CHAR)(value - 1 + 'A');
				}
				else {
					work[len++] = '\\';
					work[len++] = (CHAR)(value / 100 + '0');
					work[len++] = (CHAR)((value % 100) / 10 + '0');
					work[len++] = (CHAR)(value / 100 + '0');
				}
			}
			memcpy(ptr, newstartend, MAXSTARTEND);
		}
		else {  /* length */
			for (len = i1, value = 0; len < msginlen; len++)
				if (isdigit(msgin[len])) value = value * 10 + msgin[len] - '0';
			i2 = channel->recvlimit;
			channel->recvlimit = value;
			len = 32;
			do work[--len] = (CHAR)(i2 % 10 + '0');
			while (i2 /= 10);
			memcpy(work, &work[len], 32 - len);
			len = 32 - len;
		}
		if (*msgoutlen >= i1 + len) {
			memmove(msgout, msgin, i1);
			memcpy(&msgout[i1], work, len);
			*msgoutlen = i1 + len;
		}
		else *msgoutlen = 0;
		return(0);
	}

/*** CODE: CANCEL ANY PENDING SENDS OR RECEIVES ? ***/
	if (msginlen) {
		msgptr = (CHAR *) msgin;
		ctlflag = 0;
		while (msgptr < (CHAR *) msgin + msginlen) {
			switch (*msgptr++) {
			case 'X':
				ctlflag |= COMCTL_SPEC_X; break;
			case 'x':
				ctlflag |= COMCTL_SPEC_x; break;
			default:
				continue;
			}
		}
		switch (ctlflag & COMCTL_FLOWS) {
		case (COMCTL_SPEC_X):
			channel->termnew.c_iflag |= IXON | IXOFF;
			break;
		case (COMCTL_SPEC_x):
			channel->termnew.c_iflag &= ~(IXON | IXOFF);
			break;
		default:
			comerror(channel, 0, "INVALID COMCTL OPTION", 0);
			return(775);
		}
		if (channel->flags & COM_FLAGS_TTY) {
#if defined(USE_POSIX_TERMINAL_IO)
			tcsetattr(channel->handle, TCSADRAIN, &channel->termnew);
#else
			ioctl(channel->handle, TCSETAW, &channel->termnew);
#endif
		}
	}
	*msgoutlen = 0;
	return(0);
}

static INT sercallback(void *handle, INT polltype)
{
	INT count, devpoll, flags, retcode;
	CHANNEL *channel;

	channel = (CHANNEL *) handle;
	if (channel->channeltype != CHANNELTYPE_SERIAL) return(1);

	devpoll = channel->devpoll;

	if (polltype & DEVPOLL_WRITE) {
		devpoll &= ~DEVPOLL_WRITE;
		if (channel->status & COM_SEND_PEND) {
			count = channel->sendlength - channel->senddone;
			flags = fcntl(channel->handle, F_GETFL, 0);
			fcntl(channel->handle, F_SETFL, flags | O_NONBLOCK);
			retcode = write(channel->handle, (CHAR *)(channel->sendbuf + channel->senddone), count);
			fcntl(channel->handle, F_SETFL, flags);

			if (retcode >= count) {  /* success */
				channel->status = (channel->status & ~COM_SEND_MASK) | COM_SEND_DONE;
				evtset(channel->sendevtid);
			}
			else	if (retcode == -1 && errno != EAGAIN && errno != EINTR)
				comerror(channel, COM_SEND_ERROR, "WRITE FAILURE", errno);
			else {
				if (retcode > 0) channel->senddone += retcode;
				devpoll |= DEVPOLL_WRITE;
			}
		}
	}

	if (polltype & DEVPOLL_READ) {
		devpoll &= ~DEVPOLL_READ;
		if (channel->status & COM_RECV_PEND) {
			if (channel->recvhead > channel->recvdone) {
				if (channel->recvhead < channel->recvtail) {
					channel->recvtail -= channel->recvhead;
					memmove(&channel->recvbuf[channel->recvdone], &channel->recvbuf[channel->recvhead], channel->recvtail);
				}
				else channel->recvtail = 0;
				channel->recvhead = channel->recvdone;
				channel->recvtail += channel->recvdone;
			}
/*** MAYBE ONLY READ FOR NUMBER OF BYTES NEEDED SO THERE WILL BE LESS ***/
/*** IN BUFFER WHEN THE CLOSE HAPPENS ***/
#ifdef __DGUX__
/*** NEEDED BECAUSE POLL(2) INCORRECTLY USES VMIN/VTIME ***/
			if (channel->flags & COM_FLAGS_TTY) {
				channel->termnew.c_cc[VMIN] = 0;
				ioctl(channel->handle, TCSETA, &channel->termnew);
			}
#endif
			retcode = read(channel->handle, &channel->recvbuf[channel->recvtail], channel->recvbufsize - channel->recvtail);
#ifdef __DGUX__
/*** NEEDED BECAUSE POLL(2) INCORRECTLY USES VMIN/VTIME ***/
			if (channel->flags & COM_FLAGS_TTY) {
				channel->termnew.c_cc[VMIN] = 1;
				ioctl(channel->handle, TCSETA, &channel->termnew);
			}
#endif
			if (retcode == -1 && errno != EINTR) comerror(channel, COM_RECV_ERROR, "READ FAILURE", errno);
			else {
				if (retcode > 0) channel->recvtail += retcode;
				if (channel->recvtail != channel->recvhead) serreadprocess(channel);
				if (channel->status & COM_RECV_PEND) devpoll |= DEVPOLL_READ;
			}
		}
	}

	if (devpoll != channel->devpoll) {
		evtdevset(channel->handle, devpoll);
		channel->devpoll = devpoll;
	}
	return(0);
}

static void serreadprocess(CHANNEL *channel)
{
	INT i1, done, head, ignorelen, length, limit, matchlen, tail;
	UCHAR chr, matchchr, *buffer, *ignore, *match;

	head = channel->recvhead;
	tail = channel->recvtail;
	buffer = channel->recvbuf;

	if (channel->recvstate == COM_STATE_START) {
		matchlen = channel->recvstart[0];
		match = channel->recvstart + 1;
		matchchr = match[0];
		for ( ; head < tail; head++) {
			if (buffer[head] != matchchr) continue;
			if (head + matchlen > tail) break;
			for (i1 = 1; i1 < matchlen && buffer[head + i1] == match[i1]; i1++);
			if (i1 == matchlen) {
				head += matchlen;
				channel->recvstate = COM_STATE_END;
				break;
			}
		}
	}

	if (channel->recvstate == COM_STATE_END) {
		done = channel->recvdone;
		length = channel->recvlength;
/*** CODE: IF 0 MAYBE SET TO START STRING AND SEARCH FOR THAT ***/
		if ( (matchlen = channel->recvend[0]) ) {
			match = channel->recvend + 1;
			matchchr = match[0];
		}
		if ( (ignorelen = channel->recvignore[0]) )
			ignore = channel->recvignore + 1;
		if (!(limit = channel->recvlimit) && !matchlen) limit = length;
		for ( ; head < tail; head++) {
			chr = buffer[head];
			if (ignorelen) {
				for (i1 = 0; i1 < ignorelen && chr != ignore[i1]; i1++);
				if (i1 < ignorelen) continue;
			}
			if (matchlen && chr == matchchr) {
				if (head + matchlen > tail) break;
				for (i1 = 1; i1 < matchlen && buffer[head + i1] == match[i1]; i1++);
				if (i1 == matchlen) {
					head += matchlen;
					channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_DONE;
					evtset(channel->recvevtid);
					break;
				}
			}
			if (done < length || limit) buffer[done++] = chr;
			if (done == limit) {
				head++;
				channel->status = (channel->status & ~COM_RECV_MASK) | COM_RECV_DONE;
				evtset(channel->recvevtid);
				break;
			}
		}
		channel->recvdone = done;
	}
	channel->recvhead = head;
}

static INT serparse(CHANNEL *channel, CHAR *pos1)
{
	CHAR *pos2;

	if (*pos1 == ':') pos1++;
	if (!*pos1) return(0);

	pos2 = strchr(pos1, ',');  /* BAUD RATE */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		channel->baud = 100 * atoi(pos1);
	}
	else pos2++;
	if ((pos1 = pos2) == NULL) return(1);

	pos2 = strchr(pos1, ',');  /* PARITY */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		channel->parity = (CHAR) toupper(*pos1);
	}
	else pos2++;
	if ((pos1 = pos2) == NULL) return(2);

	pos2 = strchr(pos1, ',');  /* DATA BITS */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		channel->databits = atoi(pos1);
	}
	else pos2++;
	if ((pos1 = pos2) == NULL) return(3);

	pos2 = strchr(pos1, ',');  /* STOP BITS */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		channel->stopbits = atoi(pos1);
	}
	else pos2++;
	if ((pos1 = pos2) == NULL) return(4);

	pos2 = strchr(pos1, ',');  /* RECV START */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		if (sergetparm(channel->recvstart, pos1) < 0) return RC_ERROR;
	}
	else {
		channel->recvstart[0] = 0;
		pos2++;
	}
	if ((pos1 = pos2) == NULL) return(5);

	pos2 = strchr(pos1, ',');  /* RECV END */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		if (sergetparm(channel->recvend, pos1) < 0) return RC_ERROR;
	}
	else {
		channel->recvend[0] = 0;
		pos2++;
	}
	if ((pos1 = pos2) == NULL) return(6);
	pos2 = strchr(pos1, ',');  /* SEND START */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		if (sergetparm(channel->sendstart, pos1) < 0) return RC_ERROR;
	}
	else {
		channel->sendstart[0] = 0;
		pos2++;
	}
	if ((pos1 = pos2) == NULL) return(7);

	pos2 = strchr(pos1, ',');  /* SEND END */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		if (sergetparm(channel->sendend, pos1) < 0) return RC_ERROR;
	}
	else {
		channel->sendend[0] = 0;
		pos2++;
	}
	if ((pos1 = pos2) == NULL) return(8);

	pos2 = strchr(pos1, ',');  /* RECV IGNORE */
	if (pos2 != pos1) {
		if (pos2 != NULL) *pos2++ = 0;
		if (sergetparm(channel->recvignore, pos1) < 0) return RC_ERROR;
	} else {
		channel->recvignore[0] = 0;
		pos2++;
	}
	if (pos2 == NULL) return(9);

	channel->recvlimit = atoi(pos2);
	return(10);
}

static INT sergetparm(UCHAR *deststr, CHAR *strptr)
{
	INT i1, destpos, cnt;

	destpos = 0;
	while (*strptr) {
		destpos++;
		if (destpos == MAXSTARTEND) return RC_ERROR;
		if (*strptr == '\\') {
			strptr++;
			if (*strptr == '\\' || *strptr == '^') i1 = *strptr;
			else {
				i1 = 0;
				for (cnt = 0; cnt < 3; cnt++) {
					if (!isdigit(*strptr)) return RC_ERROR;
					i1 = i1 * 10 + *strptr - '0';
					strptr++;
				}
				strptr--;  /* incremented by one too many */
				if (i1 > 255) return RC_ERROR;
			}
			deststr[destpos] = (UCHAR) i1;
		}
		else if (*strptr == '^') {
			strptr++;
			i1 = toupper(*strptr) - 'A' + 1;
			if (i1 < 1 || i1 > 31) return RC_ERROR;
			deststr[destpos] = (UCHAR) i1;
		}
		else deststr[destpos] = *strptr;
		strptr++;
	}
	deststr[0] = destpos;  /* save length of deststr in first byte */
	return(0);
}

