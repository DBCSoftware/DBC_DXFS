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
#define INC_CTYPE
#define INC_SQL
#define INC_SQLEXT
#include "includes.h"
#include "fsodbc.h"


/**
 * Thread safe
 */
void error_init(ERRORS *errors)
 {
	errors->crsrows = 0;
	errors->dynfunc[0] = '\0';
	errors->dfcode = SQL_DIAG_UNKNOWN_STATEMENT;
	errors->numerrors = 0;
	errors->rowcount = 0;
 }

/**
 * Thread safe
 */
void error_record(ERRORS *errors, char *sqlstatep, char *textp1, char *textp2, int nativeerror)
{
	size_t i1, i2;
	int numerrors;

	numerrors = errors->numerrors;  /* for easier reference */
	if (numerrors < MAX_ERRORS) {
		strcpy((char*)errors->errorinfo[numerrors].sqlstate, sqlstatep);
		i1 = 0;
		if (textp1 != NULL) {
			i1 = strlen(textp1);
			if (i1 > sizeof(errors->errorinfo[numerrors].text) - 1) i1 = sizeof(errors->errorinfo[numerrors].text) - 1;
			memcpy(errors->errorinfo[numerrors].text, textp1, i1);
		}
		if (textp2 != NULL) {
			i2 = strlen(textp2);
			if (i2 > (sizeof(errors->errorinfo[numerrors].text) - 3 - i1)) i2 = sizeof(errors->errorinfo[numerrors].text) - 3 - i1;
			if (i2 > 0) {
				errors->errorinfo[numerrors].text[i1++] = ':';
				errors->errorinfo[numerrors].text[i1++] = ' ';
				memcpy(&errors->errorinfo[numerrors].text[i1], textp2, i2);
				i1 += i2;
			}
		}
		errors->errorinfo[numerrors].text[i1] = '\0';
		errors->errorinfo[numerrors].native = nativeerror;
		errors->errorinfo[numerrors].columnNumber = SQL_COLUMN_NUMBER_UNKNOWN;
		errors->errorinfo[numerrors].rowNumber = SQL_ROW_NUMBER_UNKNOWN;
		errors->numerrors++;
	}
}
