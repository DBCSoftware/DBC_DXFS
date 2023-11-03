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

#ifndef _DBCCFG_INCLUDED
#define _DBCCFG_INCLUDED

#include "xml.h"
 
#define CFG_MAX_KEYLENGTH 256
#define CFG_MAX_KEYDEPTH 4
#define CFG_MAX_ENTRYSIZE 16384
#define CFG_MAX_ERRMSGSIZE 128
 
/**
 * Reads configuration file from disk and calls appropriate routine to parse the
 * file and generate XML string.  Returns 0 on success, any other value returned
 * indicates that an error occurred.  Call cfggeterror() to return the error
 * message.
 */
INT cfginit(CHAR *, INT);
CHAR * cfggeterror(void);
ELEMENT * cfggetxml(void);
void cfgexit(void);

#endif
