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

#include "includes.h"
#include "dsr.h"

int dsropen(name)
char *name;
{
	return(0);
}

int dsrclose(handle)
int handle;
{
	return(0);
}

int dsrquery(handle, function, buffer, length)
int handle;
char *function, *buffer;
int *length;
{
	return(0);
}

int dsrchange(int handle, char *function, char *buffer, int length)
{
	return(0);
}

int dsrload(handle, imagevar)
int handle;
void *imagevar;
{
	return(0);
}

int dsrstore(handle, imagevar)
int handle;
void *imagevar;
{
	return(0);
}

int dsrlink(handle, qhandle)
int handle;
int qhandle;
{
	return(0);
}

int dsrunlink(handle, qhandle)
int handle;
int qhandle;
{
	return(0);
}
