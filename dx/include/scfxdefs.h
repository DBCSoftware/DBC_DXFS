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

/**
 * These are defines and tables used in dbcclnt and the C Smart Clients
 */
#ifndef SCFILEXFERSTUFF_H
#define SCFILEXFERSTUFF_H
#define FILETRANSFERCLIENTGETELEMENTTAG "clientgetfile"
#define FILETRANSFERCLIENTPUTELEMENTTAG "clientputfile"
#define FILETRANSFERCLIENTDIRTAGNAME "filetransferclientdir"
#define FILETRANSFERCLIENTSIDEFILENAMETAG "csfname"
#define FILETRANSFERMORETOCOMETAG "moretocome"
#define FILETRANSFERBITSTAG "bits"
#define chunkSize (0x10000 - 16)

enum clientFileTransferErrorCode {
	XFERERR_NOLOCALDIR = 1,
	XFERERR_INTERNAL,
	XFERERR_NOMEM,
	XFERERR_FIO
};

struct filexfererrortable_tag {
	enum clientFileTransferErrorCode code;
	CHAR *stringValue;
};

struct filexfererrortable_tag filexfererrortable[] = {
	{XFERERR_NOLOCALDIR, "nolocaldir"},
	{XFERERR_INTERNAL, "internal"},
	{XFERERR_NOMEM, "nomem"},
	{XFERERR_FIO, "fio"}
};
#endif
