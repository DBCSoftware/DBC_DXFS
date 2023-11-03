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
#include "includes.h"

#if OS_UNIX
#define ARRAYSIZE(A) (sizeof(A)/sizeof((A)[0]))
#else
#include "winnt.h"
#endif

#include "base.h"
#include "dbc.h"
#include "dbcclnt.h"
#include "fio.h"
#include "fsfileio.h"
#include "scfxdefs.h"

static CHAR sourceFile[128];
static CHAR destinationFile[128];
static UCHAR **fileBuffer, **encodedFileBuffer;
static CHAR filexfererrstr[256];

static void clientPutFile();
static void clientGetFile();
static INT fileXferErrValue(CHAR *str);

void checkClientFileTransferOptions() {
	CHAR *ptr;
	if (!prpget("client", "filetransfer", "clientdir", NULL, &ptr, 0)) {
		clientput("<" FILETRANSFERCLIENTDIRTAGNAME ">", -1);
		clientput(ptr, -1);
		clientput("</" FILETRANSFERCLIENTDIRTAGNAME ">", -1);
	}
}

/**
 * Transfer a file between the server and the Smart Client
 * The meaning of Get and Put are from the Client's perspective
 *
 * adr1 is always the source file name
 * adr2 is always the destination file name
 */
void clientFileTransfer(UCHAR vx, UCHAR *adr1, UCHAR *adr2) {
	INT i1, i2;
	cvtoname(adr1);
	for (i2 = 0; name[i2] == ' '; i2++);
	i1 = (INT)strlen(&name[i2]);
	memmove(sourceFile, &name[i2], i1 + 1);

	cvtoname(adr2);
	for (i2 = 0; name[i2] == ' '; i2++);
	i1 = (INT)strlen(&name[i2]);
	memmove(destinationFile, &name[i2], i1 + 1);

	switch (vx) {
	case VERB_CLIENTGETFILE:
		clientGetFile();
		break;
	case VERB_CLIENTPUTFILE:
		clientPutFile();
		break;
	}
}

/**
 * Utility function to help with the XML to the SC for the ClientPutFile verb
 */
static void clientPutFileSendXML(INT sendFileName, CHAR**bits, INT*moretocome) {
	CHAR work[512];
	ELEMENT *e1;
	ATTRIBUTE *a1;
	INT XferErrorCode;

	if (sendFileName) {
		clientput("<" FILETRANSFERCLIENTPUTELEMENTTAG " " FILETRANSFERCLIENTSIDEFILENAMETAG "=\"",
				-1); /* Client Side File Name */
		clientputdata(sourceFile, -1);
		clientput("\"/>", -1);
	}
	else
		clientput("<" FILETRANSFERCLIENTPUTELEMENTTAG " />", -1);

	if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) {
		dbcerror(798);
	}
	setpgmdata();
	e1 = (ELEMENT *) clientelement();
	if (e1 == NULL) dbcerror(798);
	*bits = NULL;
	*moretocome = FALSE;
	a1 = e1->firstattribute;
	if (a1 == NULL) dbcerror(798);
	while (a1 != NULL) {
		if (strcmp(a1->tag, FILETRANSFERBITSTAG) == 0) *bits = a1->value;
		else if (strcmp(a1->tag, FILETRANSFERMORETOCOMETAG) == 0)
			*moretocome = (a1->value[0] == 'Y');
		else if (strcmp(a1->tag, "e") == 0) goto errexit;
		a1 = a1->nextattribute;
	}
	clientrelease();
	return;

	/**
	 * <r e=fio xtra="-22">fio error on open in clientPutFile</r>
	 */
errexit:
	XferErrorCode = fileXferErrValue(a1->value);
	if (XferErrorCode == XFERERR_FIO) {
		ATTRIBUTE *a2 = a1->nextattribute;
		dbcerrinfo(798, (UCHAR*)fioerrstr(atoi(a2->value)), -1);
	}
	else if (XferErrorCode == XFERERR_NOMEM) {
		ATTRIBUTE *a2 = a1->nextattribute;
		sprintf(work, "%s bytes at client", a2->value);
		dbcerrinfo(1667, (UCHAR*)work, -1);
	}
	else if (e1->firstsubelement != NULL){
		ELEMENT *e2 = e1->firstsubelement;
		sprintf(work, "e=%d, '%s'", XferErrorCode, e2->tag);
		dbcerrinfo(1699, (UCHAR*)work, -1);
	}
	sprintf(work, "e=%d", XferErrorCode);
	dbcerrinfo(798, (UCHAR*)work, -1);
}

/**
 * Transfer a file from the Client to the Server
 * The source file is remote, the destination file is local
 */
static void clientPutFile() {
	INT filehandle, incomingLength, bytesToWrite;
	CHAR *bits, work[256];
	INT i1, moretocome = FALSE, binBufSize;
	UCHAR **binBuff;
	OFFSET offset = 0;

	clientPutFileSendXML(TRUE, &bits, &moretocome);

	/* Open the file, even if the file is fragmented, this is done only once */
	filehandle = fioopen(destinationFile, FIO_M_PRP | FIO_P_CFT);
	if (filehandle < 0 || bits == NULL) {
		if (dsperrflg) {
			sprintf(work, "f='%s', msg='%s'", destinationFile, fioerrstr(filehandle));
			dbcerrinfo(613, (UCHAR *) work, -1);
		}
		else dbcerror(613);
	}
	incomingLength = (INT) strlen(bits);
	if (incomingLength == 0) {
		/* Zero-length file! This is ok. */
		fioclose(filehandle);
		return;
	}
	/*
	 * Set the buffer size for the converted-to-binary-from-base64 bytes
	 * If this file is fragmented, use the chunk size, else use the size
	 * of this on-and-only fragment.
	 */
	binBufSize = moretocome ? chunkSize : (((incomingLength >> 2) + 1) * 3 + 10);

	/* Allocate the buffer, even if the file is fragmented, this is done only once */
	binBuff = memalloc(binBufSize, 0);
	if (binBuff == NULL) {
		if (dsperrflg) {
			sprintf(work, "memalloc failed for %d bytes in %s", binBufSize,
#ifndef NO__FUNCTION__
				__FUNCTION__);
#else
				"clientPutFile");
#endif
			dbcerrinfo(1667, (UCHAR *) work, -1);
		}
		else dbcerror(1667);
	}

	for (;;) {
		/* Convert from base-64 to binary and write it */
		fromprintable((UCHAR*)bits, incomingLength, *binBuff, &bytesToWrite);
		i1 = fiowrite(filehandle, offset, *binBuff, bytesToWrite);
		if (i1 < 0) {
			if (dsperrflg) {
				sprintf(work, "f='%s', msg='%s'", destinationFile, fioerrstr(filehandle));
				dbcerrinfo(706, (UCHAR *) work, -1);
			}
			else dbcerror(706);
		}
		if (!moretocome) break;
		offset += bytesToWrite;
		clientPutFileSendXML(FALSE, &bits, &moretocome);
		incomingLength = (INT) strlen(bits);
	}

	fioclose(filehandle);
	memfree(binBuff);
	return;
}

/**
 * Transfer a file from the Server to the Client
 * The source file is local, the destination file is remote
 */
static void clientGetFile() {
	INT i1, handle;
	INT encodeBufferSize = (chunkSize / 3 + 1) * 4 + 128 /* for good luck */ ;
	INT errorcode;
	OFFSET fileSize, bytesRemaining;
	OFFSET offset;
	ELEMENT *e1;
	UINT encodedDataSize;
	//CHAR work2[MAX_PATH + 64];

	fileBuffer = NULL;
	encodedFileBuffer = NULL;
	filexfererrstr[0] = '\0';
	handle = fioopen(sourceFile, FIO_M_SRO | FIO_P_CFT);
	setpgmdata();
	if (handle < 0) {
		errorcode = 601;
		sprintf(filexfererrstr, "f=\"%s\", msg=\"%s\"", sourceFile, fioerrstr(handle));
		goto errexit;
	}
	i1 = fiogetsize(handle, &fileSize);
	setpgmdata();
	if (i1 < 0) {
		errorcode = 1699;
		sprintf(filexfererrstr, "f=\"%s\", msg=\"%s\"", sourceFile, fioerrstr(handle));
		goto errexit;
	}
	if (fileSize == 0) { /* Zero length file! */
		clientput("<" FILETRANSFERCLIENTGETELEMENTTAG " " FILETRANSFERCLIENTSIDEFILENAMETAG "=\"",
				-1); /* Client Side File Name */
		clientputdata(destinationFile, -1);
		clientput("\" " FILETRANSFERBITSTAG "=\"\"", -1);
		clientput(" " FILETRANSFERMORETOCOMETAG "=\"N\"/>", -1);
		if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) {
			errorcode = 798;
			goto errexit;
		}
		e1 = (ELEMENT *) clientelement(); /* assume <r> element */
		clientrelease();
		if (e1 == NULL) {
			errorcode = 798;
			goto errexit;
		}
		if (e1->firstattribute != NULL) { /* assume e attribute, error message! */
			errorcode = 798;
			i1 = fileXferErrValue(e1->firstattribute->value);
			if (e1->firstsubelement != NULL && e1->firstsubelement->cdataflag) {
				sprintf(filexfererrstr, "%s, errcode=%d", e1->firstsubelement->tag, i1);
			}
			else {
				sprintf(filexfererrstr, "errcode=%d", i1);
			}
			goto errexit;
		}
		return;
	}

	/**
	 * Allocate a buffer to hold 'chunkSize' bytes
	 */
	fileBuffer = memalloc((INT)chunkSize, 0);
	setpgmdata();
	if (fileBuffer == NULL) {
		errorcode = 1667;
		sprintf(filexfererrstr, "memalloc failed for %u bytes in %s", (UINT)chunkSize,
#ifndef NO__FUNCTION__
				__FUNCTION__);
#else
				"clientGetFile");
#endif
		goto errexit;
	}

	/**
	 * Allocate a buffer to hold the base-64 encoded chunk of the file
	 */
	encodedFileBuffer = memalloc(encodeBufferSize, 0);
	setpgmdata();
	if (encodedFileBuffer == NULL) {
		errorcode = 1667;
		sprintf(filexfererrstr, "memalloc failed for %d bytes in %s", encodeBufferSize,
#ifndef NO__FUNCTION__
				__FUNCTION__);
#else
				"clientGetFile");
#endif
		goto errexit;
	}

	bytesRemaining = fileSize;
	offset = 0;
	do {
		INT bytesToSend;
		bytesToSend = (bytesRemaining <= chunkSize) ? (INT)bytesRemaining : chunkSize;
		i1 = fioread(handle, offset, *fileBuffer, bytesToSend);
		setpgmdata();
		if (i1 < 0) {
			errorcode = 1668;
			sprintf(filexfererrstr, "f='%s', msg='%s'", sourceFile, fioerrstr(i1));
			goto errexit;
		}
		makeprintable(*fileBuffer, bytesToSend, *encodedFileBuffer, (INT*)&encodedDataSize);
		(*encodedFileBuffer)[encodedDataSize] = 0;
		bytesRemaining -= bytesToSend;
		offset += bytesToSend;

		/* Send the file to the SC */
		clientput("<" FILETRANSFERCLIENTGETELEMENTTAG " " FILETRANSFERCLIENTSIDEFILENAMETAG "=\"",
				-1); /* Client Side File Name */
		clientputdata(destinationFile, -1);
		clientput("\" " FILETRANSFERBITSTAG "=\"", -1);
		clientput((CHAR*)*encodedFileBuffer, encodedDataSize);
		clientput("\" " FILETRANSFERMORETOCOMETAG "=\"", -1);
		(bytesRemaining > 0) ? clientput("Y\"/>", -1) : clientput("N\"/>", -1);
		if (clientsend(CLIENTSEND_WAIT | CLIENTSEND_SERIAL, 0) < 0) {
			errorcode = 798;
			goto errexit;
		}
		e1 = (ELEMENT *) clientelement(); /* assume <r> element */
		clientrelease();
		if (e1 == NULL) {
			errorcode = 798;
			goto errexit;
		}
		if (e1->firstattribute != NULL) { /* assume e attribute, error message! */
			errorcode = 798;
			i1 = fileXferErrValue(e1->firstattribute->value);
			if (e1->firstsubelement != NULL && e1->firstsubelement->cdataflag) {
				sprintf(filexfererrstr, "%s, errcode=%d", e1->firstsubelement->tag, i1);
			}
			else {
				sprintf(filexfererrstr, "errcode=%d", i1);
			}
			goto errexit;
		}

	} while (bytesRemaining > 0);


	memfree(fileBuffer);
	memfree(encodedFileBuffer);
	fioclose(handle);
	setpgmdata();
	return;

errexit:
	if (fileBuffer != NULL) memfree(fileBuffer);
	if (encodedFileBuffer != NULL) memfree(encodedFileBuffer);
	clientrelease();
	fioclose(handle);
	setpgmdata();
	if (dsperrflg && filexfererrstr[0] != '\0') {
		dbcerrinfo(errorcode, (UCHAR *) filexfererrstr, -1);
	}
	else dbcerror(errorcode);
}

static INT fileXferErrValue(CHAR *str) {
	INT i2;
	for (i2 = 0; (UINT)i2 < ARRAYSIZE(filexfererrortable); i2++) {
		if (strcmp(str, filexfererrortable[i2].stringValue) == 0) {
			return filexfererrortable[i2].code;
		}
	}
	return 799;
}

void clientClockVersion(CHAR *work, size_t cbWork)
{
#if OS_WIN32
	DBG_UNREFERENCED_PARAMETER(work);
	DBG_UNREFERENCED_PARAMETER(cbWork);
#endif
}
