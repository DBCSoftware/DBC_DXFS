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

#ifndef _DBC_TCP_INCLUDED
#define _DBC_TCP_INCLUDED

#include <stdio.h>
#include <openssl/bio.h>

#if !OS_WIN32
#ifndef INADDR_NONE
#define INADDR_NONE (unsigned long) 0xFFFFFFFF
#endif
#ifndef SOCKET
typedef int SOCKET;
#endif
#endif

#define TCP_UTF8	0x01	/* convert to/from utf8 */
#define TCP_SSL		0x02	/* support ssl */
#define TCP_EINTR	0x04	/* interrupt causes -1 return */
#define TCP_CLNT	0x08	/* default for tcpconnect & tcpaccept */
#define TCP_SERV	0x10	/* for tcpconnect & tcpaccept */

extern SOCKET tcpconnect(char *server, int port, int tcpflags, BIO *authfile, int timeout);
extern SOCKET tcplisten(int port, int *listenport);
extern SOCKET tcpaccept(SOCKET sockethandle, int tcpflags, BIO *authfile /*char *authfile*/, int timeout);
extern int tcprecv(SOCKET sockethandle, unsigned char *buffer, int length, int flags, int timeout);
extern int tcpsend(SOCKET sockethandle, unsigned char *buffer, int length, int flags, int timeout);
extern int tcpitoa(intptr_t src, char *dest);
extern void tcpiton(int src, unsigned char *dest, int n);
extern int tcpntoi(unsigned char *src, int n, int *dest);
extern int tcpquotedlen(unsigned char *string, int length);
extern int tcpquotedcopy(unsigned char *dest, unsigned char *src, int srclen);
extern int tcpnextdata(unsigned char *data, int datalen, int *offset, int *nextoffset);
extern char *tcpgeterror(void);
extern void tcpseterror(char *);
extern void tcpcleanup(void);
extern int tcpissslsupported(void);
extern void tcpstartssllog(FILE *log);
extern int tcpsslsinit(SOCKET socket, BIO* input /*char *certificate*/);
//extern int tcpsslcinit(SOCKET socket  /*, BIO *input char *certificate*/);
extern int tcpsslcomplete(SOCKET socket);

#endif
