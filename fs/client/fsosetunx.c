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

#if OS_UNIX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <odbcinst.h>
#include <odbcinstext.h>

int ODBCINSTGetProperties(HODBCINSTPROPERTY hLastProperty)
{
	hLastProperty->pNext 				= (HODBCINSTPROPERTY)malloc(sizeof(ODBCINSTPROPERTY));
	hLastProperty 						= hLastProperty->pNext;
	memset(hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
    hLastProperty->nPromptType			= ODBCINST_PROMPTTYPE_TEXTEDIT;
	strncpy(hLastProperty->szName, "Database", INI_MAX_PROPERTY_NAME);
	strncpy(hLastProperty->szValue, "", INI_MAX_PROPERTY_VALUE);

	hLastProperty->pNext 				= (HODBCINSTPROPERTY)malloc( sizeof(ODBCINSTPROPERTY));
	hLastProperty 						= hLastProperty->pNext;
	memset( hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType			= ODBCINST_PROMPTTYPE_TEXTEDIT;
	strncpy( hLastProperty->szName, "Server", INI_MAX_PROPERTY_NAME);
	strncpy( hLastProperty->szValue, "localhost", INI_MAX_PROPERTY_VALUE);

	hLastProperty->pNext 				= (HODBCINSTPROPERTY)malloc( sizeof(ODBCINSTPROPERTY));
	hLastProperty 						= hLastProperty->pNext;
	memset( hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType			= ODBCINST_PROMPTTYPE_TEXTEDIT;
	strncpy( hLastProperty->szName, "UID", INI_MAX_PROPERTY_NAME);
	strncpy( hLastProperty->szValue, "", INI_MAX_PROPERTY_VALUE);

	hLastProperty->pNext 				= (HODBCINSTPROPERTY)malloc( sizeof(ODBCINSTPROPERTY));
	hLastProperty 						= hLastProperty->pNext;
	memset( hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType			= ODBCINST_PROMPTTYPE_TEXTEDIT;
	strncpy( hLastProperty->szName, "Password", INI_MAX_PROPERTY_NAME);
	strncpy( hLastProperty->szValue, "", INI_MAX_PROPERTY_VALUE);

	hLastProperty->pNext 				= (HODBCINSTPROPERTY)malloc( sizeof(ODBCINSTPROPERTY));
	hLastProperty 						= hLastProperty->pNext;
	memset( hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType			= ODBCINST_PROMPTTYPE_TEXTEDIT;
	strncpy( hLastProperty->szName, "ServerPort", INI_MAX_PROPERTY_NAME);
	strncpy( hLastProperty->szValue, "", INI_MAX_PROPERTY_VALUE);

	hLastProperty->pNext 				= (HODBCINSTPROPERTY)malloc( sizeof(ODBCINSTPROPERTY));
	hLastProperty 						= hLastProperty->pNext;
	memset( hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType			= ODBCINST_PROMPTTYPE_TEXTEDIT;
	strncpy( hLastProperty->szName, "LocalPort", INI_MAX_PROPERTY_NAME);
	strncpy( hLastProperty->szValue, "", INI_MAX_PROPERTY_VALUE);

	hLastProperty->pNext 				= (HODBCINSTPROPERTY)malloc( sizeof(ODBCINSTPROPERTY));
	hLastProperty 						= hLastProperty->pNext;
	memset( hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType			= ODBCINST_PROMPTTYPE_TEXTEDIT;
	strncpy( hLastProperty->szName, "Encryption", INI_MAX_PROPERTY_NAME);
	strncpy( hLastProperty->szValue, "yes", INI_MAX_PROPERTY_VALUE);
				
	return 1;
}
#endif
