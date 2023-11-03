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
#include "includes.h"
#include "nio.h"
#include "base.h"

DllExport int nioopen(char *name, int index, int exclusive)
{
	return 0;
}

DllExport int nioprep(char *name, char *options, int index)
{
	return RC_ERROR;
}

DllExport int nioclose(int handle)
{
	return RC_ERROR;
}

DllExport int niordseq(int handle, unsigned char *buffer, int length)
{
	return RC_ERROR;
}

DllExport int niowtseq(int handle, unsigned char *buffer, int length)
{
	return RC_ERROR;
}

DllExport int niordrec(int handle, OFFSET recnum, unsigned char *buffer, int length)
{
	return RC_ERROR;
}

DllExport int niowtrec(int handle, OFFSET recnum, unsigned char *buffer, int length)
{
	return RC_ERROR;
}

DllExport int niordkey(int handle, char *key, unsigned char *buffer, int length)
{
	return RC_ERROR;
}

DllExport int niordks(int handle, unsigned char *buffer, int length)
{
	return RC_ERROR;
}

DllExport int niordkp(int handle, unsigned char *buffer, int length)
{
	return RC_ERROR;
}

DllExport int niowtkey(int handle, unsigned char *buffer, int length)
{
	return RC_ERROR;
}

DllExport int nioupd(int handle, unsigned char *buffer, int length)
{
	return RC_ERROR;
}

DllExport int niodel(int handle, char *key)
{
	return RC_ERROR;
}

DllExport int niolck(int* handle)
{
	return RC_ERROR;
}

DllExport int nioulck()
{
	return RC_ERROR;
}

DllExport int niofpos(int handle, OFFSET *pos)
{
	return RC_ERROR;
}

DllExport int niorpos(int handle, OFFSET pos)
{
	return RC_ERROR;
}

DllExport int nioclru()
{
	return RC_ERROR;
}

DllExport int nioerror()
{
	return RC_ERROR;
}
