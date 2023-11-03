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
 ******************************************************************************
 *
 *
 * openssl genrsa -out selfsigned.key 4096
 * openssl req -new -key selfsigned.key -out selfsigned.csr \
 *  -sha256 -subj "/C=US/ST=Wyoming/L=SelfSigned/O=Portable Software/OU=Org/CN=localhost"
 * openssl x509 -req -days 9000 -in selfsigned.csr -signkey selfsigned.key -out selfsigned.crt
 * cat selfsigned.crt selfsigned.key > dbcserver.crt
 *
 */
#include <openssl/bio.h>

#define INC_STRING 1
#include "includes.h"
#include "base.h"

static const char* defaultCertFileName = "dbcserver.crt";
static char certBioErrorMsg[300];
static const char* c1 = "-----BEGIN CERTIFICATE-----";
static const char* c2 = "-----END CERTIFICATE-----";
static const char* c3 = "-----BEGIN RSA PRIVATE KEY-----";
static const char* c4 = "-----END RSA PRIVATE KEY-----";
static BIO *memoryBio = NULL;
static CHAR* buffer;
static FILE* crtfile;

/**
 * Called only from two places in clientstart() in dbcclnt.c
 * Only when this is a server
 */
BIO* GetCertBio(char* certificatefilename) {

	if (memoryBio == NULL) {
		certBioErrorMsg[0] = '\0';
		if (certificatefilename == NULL || strlen(certificatefilename) == 0) certificatefilename = (char*)defaultCertFileName;
		crtfile = fopen(certificatefilename, "rb");
		if (crtfile == NULL) {
			sprintf(certBioErrorMsg, "Failure opening certificate file. File name used='%s'", certificatefilename);
			return NULL;
		}
		fseek(crtfile, 0, SEEK_END);
		size_t filesize = ftell(crtfile);
		if ((long int) filesize < 0) {
			sprintf(certBioErrorMsg, "Failure getting size of certificate file. File name used='%s'", certificatefilename);
			return NULL;
		}
		fseek(crtfile, 0, SEEK_SET);
		buffer = (CHAR *) malloc(filesize + sizeof(CHAR));
		if (buffer == NULL) {
			fclose(crtfile);
			sprintf(certBioErrorMsg, "Failure getting certificate file '%s'. Unable to allocate memory for a read buffer", certificatefilename);
			return NULL;
		}
		size_t i1 = fread(buffer, 1, filesize, crtfile);
		if (i1 != filesize) {
			free(buffer);
			sprintf(certBioErrorMsg, "Failure reading certificate file. File name used='%s'", certificatefilename);
			return NULL;
		}
		if (filesize < 150) {
			sprintf(certBioErrorMsg, "Failure in the certificate file '%s'. It is mal-formed", certificatefilename);
			return NULL;
		}
		if (strstr((const char *)(buffer), c1) == NULL || strstr((const char *)(buffer), c2) == NULL) {
			sprintf(certBioErrorMsg, "Failure in the certificate file '%s'. It is missing the certificate", certificatefilename);
			return NULL;
		}
		if (strstr((const char *)(buffer), c3) == NULL || strstr((const char *)(buffer), c4) == NULL) {
			sprintf(certBioErrorMsg, "Failure in the certificate file '%s'. It is missing the key", certificatefilename);
			return NULL;
		}
		// The below call creates a read-only memory BIO
		memoryBio = BIO_new_mem_buf(buffer, (int)filesize);
		fclose(crtfile);
	}
	else BIO_reset(memoryBio);
	return memoryBio;
}

void FreeCertBio() {
	if (memoryBio != NULL) {
		BIO_vfree(memoryBio);
		free(buffer);
		memoryBio = NULL;
		buffer = NULL;
	}
}

CHAR* GetCertBioErrorMsg() {
	return certBioErrorMsg;
}
