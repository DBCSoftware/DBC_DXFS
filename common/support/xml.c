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
#include "includes.h"

#include "xml.h"
#include "base.h"

/* number of tags other than */
#define MAXTAGS 401
/* number of bytes in other tags text area */
#define MAXTAGCHARS 2000
/* maximum depth of XML tree */
#define MAXDEPTH 16
/* single character tags */
static CHAR *onechartag[] = {
	"a","b","c","d","e","f","g","h","i","j","k","l","m",
	"n","o","p","q","r","s","t","u","v","w","x","y","z"
};

/* hash table of pointers to dynamically encountered tags other than single character tags */
static CHAR *othertags[MAXTAGS];
static INT othertagscount = 0;

/* storage area for actual null terminated strings that are other tags */
static CHAR othertagstext[MAXTAGCHARS];
static INT othertagstextlen = 0;

/* hash code calculation, assume 2 char or longer tags */
#define hashcode(a, b) ((((a)[0] * 21) + (a)[1] + ((a)[(b) - 2] * 6) + ((a)[(b)-1] * 47) + (b * 107)) % MAXTAGS)

/* character types in the following translate table */ 
#define TT_DELIM	0x01
#define TT_SPACE	0x02
#define TT_SPECL	0x04
#define TT_LTGT	0x08
#define TT_SLASH	0x10
#define TT_NUM		0x20
#define TT_LOWER	0x40
#define TT_UPPER	0x80
/* NOTE: the last 3 must be in this order */
/* NOTE: upper is not being used currently and tags that contain them are invalid */

/* translate table */
static UCHAR chartype[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, TT_SPACE,						/* 0-9 */
	TT_DELIM, 0, 0, TT_DELIM, 0, 0, 0, 0, 0, 0,					/* 10-19 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,								/* 20-29 */
	0, 0, TT_SPACE, 0, TT_SPECL,								/* 30-34 */
	0, 0, 0, TT_SPECL, TT_SPECL,								/* 35-39 */
	0, 0, 0, 0, 0,												/* 40-44 */
	0, 0, TT_SLASH, TT_NUM, TT_NUM,								/* 45-49 */
	TT_NUM, TT_NUM, TT_NUM, TT_NUM, TT_NUM,						/* 50-54 */
	TT_NUM, TT_NUM, TT_NUM, 0, 0,								/* 55-59 */
	TT_LTGT, 0, TT_LTGT, 0, 0,									/* 60-64 */
	TT_UPPER, TT_UPPER, TT_UPPER, TT_UPPER, TT_UPPER,			/* 65-69 */
	TT_UPPER, TT_UPPER, TT_UPPER, TT_UPPER, TT_UPPER,			/* 70-74 */
	TT_UPPER, TT_UPPER, TT_UPPER, TT_UPPER, TT_UPPER,			/* 75-79 */
	TT_UPPER, TT_UPPER, TT_UPPER, TT_UPPER, TT_UPPER,			/* 80-84 */
	TT_UPPER, TT_UPPER, TT_UPPER, TT_UPPER, TT_UPPER,			/* 85-89 */
	TT_UPPER, 0, 0, 0, 0,										/* 90-94 */
	0, 0, TT_LOWER, TT_LOWER, TT_LOWER,							/* 95-99 */
	TT_LOWER, TT_LOWER, TT_LOWER, TT_LOWER, TT_LOWER,			/* 100-104 */
	TT_LOWER, TT_LOWER, TT_LOWER, TT_LOWER, TT_LOWER,			/* 105-109 */
	TT_LOWER, TT_LOWER, TT_LOWER, TT_LOWER, TT_LOWER,			/* 110-114 */
	TT_LOWER, TT_LOWER, TT_LOWER, TT_LOWER, TT_LOWER,			/* 115-119 */
	TT_LOWER, TT_LOWER, TT_LOWER, 0, 0,							/* 120-124 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 125-144 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 145-164 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 165-184 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 185-204 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 205-224 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 225-244 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0								/* 245-255 */
};

static CHAR xmlerrorstring[256];

/* used by xmlflatten and flattenstring */
#define FLATTEN_STREAM	0x01
#define FLATTEN_QUOTE	0x02

static INT flattenstring(CHAR *, INT, INT, CHAR *output, size_t cbOutput);
static INT invalidxml(CHAR *msg1, CHAR *msg2, INT len);
static CHAR *gettag(CHAR *ptr1, INT len);

/*
 * Returns zero for success, -1 if needs more memory, -2 for xml syntax error
 */
INT xmlparse(CHAR *input, INT inputsize, void *outputbuffer, size_t cbOutputbuf)
{
	INT i1, i2, base, endflag, len, level, value, taglen[MAXDEPTH];
	size_t lowmark, himark;
	CHAR chr, quote, *ptr1, *ptr2;
	ELEMENT *output, **elementptr, *outputpath[MAXDEPTH];
	ATTRIBUTE *attrib, **attribptr;

	xmlerrorstring[0] = '\0';
	lowmark = 0;
	himark = cbOutputbuf - cbOutputbuf % sizeof(void *);
	level = 0;
	while (inputsize > 0) {  /* parse next element's tag or character data */
		for (i1 = 0; chartype[((UCHAR *) input)[i1]] == TT_DELIM; ) if (++i1 == inputsize) break;
		if (i1 == inputsize) break;
		if (input[i1] == '<') {  /* parse element tag */
			input += i1;
			inputsize -= i1;
			if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
			while (chartype[*((UCHAR *) ++input)] & (TT_DELIM | TT_SPACE))
				if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
			if (*input == '/') {
				if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
				while (chartype[*((UCHAR *) ++input)] & (TT_DELIM | TT_SPACE)) if (!--inputsize)
					return invalidxml("unexpected end of data", NULL, 0);
				endflag = TRUE;
			}
			else endflag = FALSE;
			if (chartype[*((UCHAR *) input)] != TT_LOWER || !--inputsize)
				return invalidxml("invalid element tag", input, inputsize);
			for (ptr1 = input; chartype[*((UCHAR *) ++input)] >= TT_NUM; )
				if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
			for (ptr2 = input; chartype[*((UCHAR *) input)] & (TT_DELIM | TT_SPACE); input++)
				if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
			len = (INT)(ptr2 - ptr1);  /* ptr1 points to tag and len is its length */
			if (endflag) {  /* end of subelements (not a new element) */
				if (*input != '>') return invalidxml("element tag missing '>'", ptr1, len);
				input++;
				inputsize--;
				if (!level) return invalidxml("element underflow", ptr1, len);
				output = outputpath[--level];
				i2 = taglen[level];
				if (len != i2 || memcmp(ptr1, output->tag, i2)) {
					return invalidxml("element does not match pushed stack element", ptr1, len);
				}
				*elementptr = NULL;
				elementptr = &output->nextelement;
				continue;
			}
			ptr2 = gettag(ptr1, len);
			if (ptr2 == NULL) return -2;
			/* create new element */
			if (!lowmark) {  /* first time only */
				if ((lowmark += sizeof(ELEMENT)) > himark) return -1;
				output = (ELEMENT *) outputbuffer;
			}
			else {
				if (lowmark > (himark -= sizeof(ELEMENT))) return -1;
				output = (ELEMENT *)((CHAR *) outputbuffer + himark);
				*elementptr = output;
			}
			output->tag = ptr2;
			output->cdataflag = 0;

			/* process any attributes */
			attribptr = &output->firstattribute;
			while (chartype[*((UCHAR *) input)] == TT_LOWER) {  /* next attribute */
				if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
				for (ptr1 = input; chartype[*((UCHAR *) ++input)] >= TT_NUM; )
					if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
				i1 = (INT)(input - ptr1);
				ptr2 = gettag(ptr1, i1);
				if (ptr2 == NULL) return -2;
				if (lowmark > (himark -= sizeof(ATTRIBUTE))) return -1;
				ptr1 = (CHAR *) outputbuffer + lowmark;
				attrib = (ATTRIBUTE *)((CHAR *) outputbuffer + himark);
				*attribptr = attrib;
				attribptr = &attrib->nextattribute;
				attrib->tag = ptr2;
				attrib->cbTag = strlen(ptr2) + sizeof(CHAR);
				attrib->value = ptr1;
				for ( ; chartype[*((UCHAR *) input)] & (TT_DELIM | TT_SPACE); input++) if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
				if (*input != '=' || !--inputsize) return invalidxml("missing '=' character", ptr2, -1);
				while (chartype[*((UCHAR *) ++input)] & (TT_DELIM | TT_SPACE)) if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
				if (*input == '"' || *input == '\'') {
					quote = *input++;
					if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
				}
				else quote = 0;
				for ( ; ; ) {
					if (quote) {
						if (*input == quote) {
							input++;
							if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
							break;
						}
					}
					else if (chartype[*((UCHAR *) input)] & (TT_DELIM | TT_SPACE | TT_LTGT | TT_SLASH)) break;
					chr = *input++;
					if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
					if (chr == '&') {
						chr = *input++;
						if (chr == 'q') {
							if ((inputsize -= 5) <= 0 || memcmp(input, "uot;", 4)) return invalidxml("expected &quot;", input - 2, inputsize + 6);
							input += 4;
							chr = '"';
						}
						else if (chr == 'a') {
							if ((inputsize -= 4) <= 0) return invalidxml("expected &amp;/&apos;", input - 2, inputsize + 5);
							if (!memcmp(input, "mp;", 3)) {
								input += 3;
								chr = '&';
							}
							else if (--inputsize && !memcmp(input, "pos;", 4)) {
								input += 4;
								chr = '\'';
							}
							else return invalidxml("expected &amp;/&apos;", input - 2, inputsize + 6);
						}
						else if (chr != '#') {
							if (chr == 'l') chr = '<';
							else if (chr == 'g') chr = '>';
							else return invalidxml("expected &lt;/&gt;", input - 2, inputsize + 1);
							if ((inputsize -= 3) <= 0 || memcmp(input, "t;", 2)) return invalidxml("expected &lt;/&gt;", input - 2, inputsize + 4);
							input += 2;
						}
						else {  /* numeric escape */
							if (!--inputsize) return invalidxml("expected &#n;", input - 2, inputsize + 1);
							if (toupper(*input) == 'x') {
								if (!--inputsize) return invalidxml("expected &#xn;", input - 2, inputsize + 1);
								base = 16;
							}
							else if (*input == '0') base = 8;
							else base = 10;
							for (value = 0; ; ) {
								chr = *input++;
								if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
								if (isdigit(chr)) value = value * base + chr - '0';
								else if (chr == ';') break;
								else {
									if (base != 16) return invalidxml("invalid numeric escape", NULL, 0);
									chr = (CHAR) toupper(chr);
									if (chr < 'A' || chr > 'F') return invalidxml("invalid numeric escape", NULL, 0);
									value = value * 16 + chr - 'A' + 10;
								}
							}
							chr = (CHAR)((UCHAR) value);
						}
					}
					if (++lowmark > himark) return -1;
					/* do this because value is zero terminated */
					if (chr == '\0') chr = ' ';
					*ptr1++ = chr;
				}
				if (++lowmark > himark) return -1;
				*ptr1 = '\0';
				while (chartype[*((UCHAR *) input)] & (TT_DELIM | TT_SPACE)) {
					if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
					input++;
				}
			}
			*attribptr = NULL;

			/* check for end of element or start of subelements */
			if (*input == '/') {
				if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
				while (chartype[*((UCHAR *) ++input)] & (TT_DELIM | TT_SPACE)) if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
				endflag = TRUE;
			}
			else endflag = FALSE;
			if (*input != '>') return invalidxml("element tag missing '>'", output->tag, -1);
			input++;
			inputsize--;
			if (endflag) {
				elementptr = &output->nextelement;
				output->firstsubelement = NULL;
			}
			else {
				if (level == MAXDEPTH) return invalidxml("element stack overflow", NULL, 0);
				outputpath[level] = output;
				taglen[level++] = len;
				elementptr = &output->firstsubelement;
			}
		}
		else {  /* character data */
			/* create new element */
			if (!lowmark) {  /* first time only */
				if ((lowmark += sizeof(ELEMENT)) > himark) return -1;
				output = (ELEMENT *) outputbuffer;
			}
			else {
				if (lowmark > (himark -= sizeof(ELEMENT))) return -1;
				output = (ELEMENT *)((CHAR *) outputbuffer + himark);
				*elementptr = output;
			}
			ptr1 = (CHAR *) outputbuffer + lowmark;
			output->tag = ptr1;
			output->firstattribute = NULL;
			output->firstsubelement = NULL;
			elementptr = &output->nextelement;

			if (i1) {
				if ((lowmark += i1) > himark) return -1;
				memcpy(ptr1, input, i1);
				ptr1 += i1;
				input += i1;
				inputsize -= i1;
			}
			while (!(chartype[*((UCHAR *) input)] & TT_LTGT)) {
				chr = *input++;
				if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
				if (chr == '&') {
					chr = *input++;
					if (chr == 'q') {
						if ((inputsize -= 5) <= 0 || memcmp(input, "uot;", 4)) return invalidxml("expected &quot;", input - 2, inputsize + 6);
						input += 4;
						chr = '"';
					}
					else if (chr == 'a') {
						if ((inputsize -= 4) <= 0) return invalidxml("expected &amp;/&apos;", input - 2, inputsize + 5);
						if (!memcmp(input, "mp;", 3)) {
							input += 3;
							chr = '&';
						}
						else if (--inputsize && !memcmp(input, "pos;", 4)) {
							input += 4;
							chr = '\'';
						}
						else return invalidxml("expected &amp;/&apos;", input - 2, inputsize + 6);
					}
					else if (chr != '#') {
						if (chr == 'l') chr = '<';
						else if (chr == 'g') chr = '>';
						else return invalidxml("expected &lt;/&gt;", input - 2, inputsize + 1);
						if ((inputsize -= 3) <= 0 || memcmp(input, "t;", 2)) {
							return invalidxml("expected &lt;/&gt;", input - 2, inputsize + 4);
						}
						input += 2;
					}
					else {  /* numeric escape */
						if (!--inputsize) return invalidxml("expected &#n;", input - 2, inputsize + 1);
						if (toupper(*input) == 'x') {
							if (!--inputsize) return invalidxml("expected &#xn;", input - 2, inputsize + 1);
							base = 16;
						}
						else if (*input == '0') base = 8;
						else base = 10;
						for (value = 0; ; ) {
							chr = *input++;
							if (!--inputsize) return invalidxml("unexpected end of data", NULL, 0);
							if (isdigit(chr)) value = value * base + chr - '0';
							else if (chr == ';') break;
							else {
								if (base != 16) return invalidxml("invalid numeric escape", NULL, 0);
								chr = (CHAR) toupper(chr);
								if (chr < 'A' || chr > 'F') return invalidxml("invalid numeric escape", NULL, 0);
								value = value * 16 + chr - 'A' + 10;
							}
						}
						chr = (CHAR)((UCHAR) value);
					}
				}
				if (++lowmark > himark) return -1;
				*ptr1++ = chr;
			}
			if (*input != '<') return invalidxml("expected '<' after data", input, inputsize);
			if (++lowmark > himark) return -1;
			*ptr1 = '\0';
			output->cdataflag = (INT)(ptr1 - output->tag);
		}
	}
	if (!lowmark) return invalidxml("no element tags", NULL, 0);
	if (level) return invalidxml("missing element tag terminator", (outputpath[level - 1])->tag, -1);
	*elementptr = NULL;
	return 0;
}

INT xmlflatten(ELEMENT *input, INT streamflag, CHAR *outputbuffer, size_t cbOutputbuf) // @suppress("No return") // @suppress("Type cannot be resolved")
{
	INT i1, len, level, taglen[MAXDEPTH];
	CHAR *ptr1, *ptr2, *ptr3;
	ELEMENT *inputpath[MAXDEPTH];
	ATTRIBUTE *attrib;
	LONG outputBytes = (LONG)cbOutputbuf;

	xmlerrorstring[0] = '\0';
	ptr1 = outputbuffer;
	level = 0;
	for ( ; ; ) {
		if (!input->cdataflag) {  /* element */
			if (--outputBytes < 0) return RC_ERROR;
			*ptr1++ = '<';
			for (len = 0, ptr2 = input->tag; ptr2[len]; len++) {
				if (--outputBytes < 0) return RC_ERROR;
				*ptr1++ = ptr2[len];
			}
			attrib = input->firstattribute;
			while (attrib != NULL) {
				ptr2 = attrib->tag;
				ptr3 = attrib->value;
				attrib = attrib->nextattribute;
				i1 = (INT)strlen(ptr2);
				if ((outputBytes -= i1 + 2) < 0) return RC_ERROR;
				*ptr1++ = ' ';
				if (i1 == 1) *ptr1++ = *ptr2;
				else {
					memcpy(ptr1, ptr2, i1);
					ptr1 += i1;
				}
				*ptr1++ = '=';
				i1 = flattenstring(ptr3, 0, (streamflag) ? FLATTEN_STREAM : FLATTEN_QUOTE, ptr1,
						(size_t)outputBytes);
				if (i1 < 0) return i1;
				ptr1 += i1;
				outputBytes -= i1;
			}
			if (input->firstsubelement != NULL) {
				if (--outputBytes < 0) return RC_ERROR;
				*ptr1++ = '>';
				if (level == MAXDEPTH) return RC_NO_MEM;
				inputpath[level] = input;
				taglen[level++] = len;
				input = input->firstsubelement;
				continue;
			}
			if ((outputBytes -= 2) < 0) return RC_ERROR;
			*ptr1++ = '/';
			*ptr1++ = '>';
		}
		else {  /* character data */
			i1 = flattenstring(input->tag, input->cdataflag, 0, ptr1, (size_t)outputBytes);
			if (i1 < 0) {
				return i1;
			}
			ptr1 += i1;
			outputBytes -= i1;
		}
		while (input->nextelement == NULL) {
			if (!level) {
				return (INT)(ptr1 - outputbuffer);
			}
			input = inputpath[--level];
			i1 = taglen[level];
			if ((outputBytes -= i1 + 3) < 0) return RC_ERROR;
			*ptr1++ = '<';
			*ptr1++ = '/';
			if (i1 == 1) *ptr1++ = *input->tag;
			else {
				memcpy(ptr1, input->tag, i1);
				ptr1 += i1;
			}
			*ptr1++ = '>';
		}
		input = input->nextelement;
	}
}

/* return the last error message */
CHAR *xmlgeterror()
{
	return xmlerrorstring;
}

/**
 * If flag is FLATTEN_QUOTE always put double quotes around 'value'.
 * If flag is FLATTEN_STREAM, use them only if necessary
 */
static INT flattenstring(CHAR *value, INT len, INT flag, CHAR *output, size_t cbOutput)
{
	INT i1;
	CHAR chr, work[16], *ptr1;
	LONG outputBytes = (LONG) cbOutput;

	if (flag & FLATTEN_STREAM) {
		for (len = 0; value[len]; len++) {
			if (chartype[(UCHAR) value[len]] & TT_SPACE) flag |= FLATTEN_QUOTE;
			else if (chartype[(UCHAR) value[len]] & (TT_SPECL | TT_LTGT)) flag &= ~FLATTEN_STREAM;
		}
	}
	ptr1 = output;
	if (flag & FLATTEN_QUOTE) {
		if ((outputBytes -= 2) < 0) return -1;
		*ptr1++ = '"';
	}
	if (flag & FLATTEN_STREAM) {  /* already inspected above */
		if ((outputBytes -= len) < 0) return -1;
		if (len == 1) *ptr1++ = *value;
		else {
			memcpy(ptr1, value, len);
			ptr1 += len;
		}
	}
	else {  /* check for special characters */
		while (TRUE) {
			chr = *value++;
			if (flag) {
				if (!chr) break;
			}
			else if (!len--) break;
			if (chartype[(UCHAR) chr] & (TT_SPECL | TT_LTGT)) {
				if (chr == '<' || chr == '>') {
					if ((outputBytes -= 4) < 0) return -1;
					if (chr == '<') memcpy(ptr1, "&lt;", 4);
					else memcpy(ptr1, "&gt;", 4);
					ptr1 += 4;
				}
				else if (chr == '&') {
					if ((outputBytes -= 5) < 0) return -1;
					memcpy(ptr1, "&amp;", 5);
					ptr1 += 5;
				}
				else if (chr == '"' || chr == '\'') {
					if ((outputBytes -= 6) < 0) return -1;
					if (chr == '"') memcpy(ptr1, "&quot;", 6);
					else memcpy(ptr1, "&apos;", 6);
					ptr1 += 6;
				}
			}
			else if ((UCHAR) chr < 0x20 || (UCHAR) chr >= 0x7F) {
				work[sizeof(work) - 1] = ';';
				i1 = sizeof(work) - 1;
				do work[--i1] = (CHAR)((UCHAR) chr % 10 + '0');
				while ( (chr = (UCHAR) ((UCHAR) chr) / 10) );
				work[--i1] = '#';
				work[--i1] = '&';
				if ((outputBytes -= sizeof(work) - i1) < 0) return -1;
				memcpy(ptr1, &work[i1], sizeof(work) - i1);
				ptr1 += sizeof(work) - i1;
			}
			else {
				if (--outputBytes < 0) return -1;
				*ptr1++ = chr;
			}
		}
	}
	if (flag & FLATTEN_QUOTE) *ptr1++ = '"';
	return (INT)(ptr1 - output);
}

/*
 * routine sets the xmlerrorstring variable and returns a negative value (-2)
 */
static INT invalidxml(CHAR *msg1, CHAR *msg2, INT len)
{
	INT i1;

	strcpy(xmlerrorstring, msg1);
	if (msg2 != NULL && len) {
		if (len == -1) len = (INT)strlen(msg2);
		if (len > 100) len = 100;
		i1 = (INT)strlen(xmlerrorstring);
		xmlerrorstring[i1++] = ':';
		xmlerrorstring[i1++] = ' ';
		memcpy(xmlerrorstring + i1, msg2, len);
		i1 += len;
		if (len == 100) {
			memcpy(xmlerrorstring + i1, " ...", 4);
			i1 += 4;
		}
		xmlerrorstring[i1] = '\0';
	}
	return -2;
}

/* routine returns a pointer to the permanently allocated zero delimited tag */
static CHAR *gettag(CHAR *worktag, INT worklen)
{
	INT n1;
	CHAR c1, *ptr1;
	
	if (worklen == 1) {  /* one character tag */
		c1 = *worktag;
		if (c1 < 'a' || c1 > 'z') {
			invalidxml("non lower case tag encountered", worktag, worklen);
			return NULL;
		}
		return onechartag[c1 - 'a'];
	}
	if (worklen < 1) {
		invalidxml("internal error A", NULL, 0);
		return NULL;
	}
	if (othertagscount == 0) {
		for (n1 = 0; n1 < MAXTAGS; n1++) othertags[n1] = NULL;
	}
	n1 = hashcode(worktag, worklen);
	while (TRUE) {
		ptr1 = othertags[n1];
		if (ptr1 == NULL) {
			if (++othertagscount > MAXTAGS - 25) {
				invalidxml("too many different tags", worktag, worklen);
				return NULL;
			}
			othertags[n1] = ptr1 = &othertagstext[othertagstextlen];
			othertagstextlen += (worklen + 1);
			if (othertagstextlen >= MAXTAGCHARS) {
				invalidxml("tag storage overflow", worktag, worklen);
				return NULL;
			}
			memcpy(ptr1, worktag, worklen);
			*(ptr1 + worklen) = '\0';
			break;
		}
		if (!memcmp(ptr1, worktag, worklen) && !*(ptr1 + worklen)) break;
		if (++n1 == MAXTAGS) n1 = 0;
	}
	return ptr1;
}
