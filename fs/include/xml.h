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

#ifndef _XML_INCLUDED
#define _XML_INCLUDED

typedef struct ATTRIBUTE_STRUCT {
	struct ATTRIBUTE_STRUCT *nextattribute;
	CHAR *tag;
	CHAR *value;
	/*
	 * possible future use, count of bytes including terminating NULL
	 */
	size_t cbTag /*, cbValue */ ;
} ATTRIBUTE;

typedef struct ELEMENT_STRUCT {
	INT cdataflag;
	CHAR *tag;
	ATTRIBUTE *firstattribute;
	struct ELEMENT_STRUCT *nextelement;
	struct ELEMENT_STRUCT *firstsubelement;
} ELEMENT;

extern INT xmlparse(CHAR *input, INT inputsize, void *outputbuffer, size_t outputsize);
extern INT xmlflatten(ELEMENT *input, INT streamflag, CHAR *outputbuffer, size_t outputsize);
extern CHAR *xmlgeterror(void);

#endif  /* _XML_INCLUDED */
