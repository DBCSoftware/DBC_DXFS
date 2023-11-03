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
#define INC_LIMITS
#define INC_STRING
#include "includes.h"
#define DBCMPD
#include "dbcmp.h"

/* list CLASS statement keywords */
#define CKW_PARENT 1
#define CKW_MAKE 2
#define CKW_DESTROY 3

EXTERN UCHAR charmap[256];
static INT parsestate;

#define OBJECTSTATE_START 1
#define OBJECTSTATE_COMMAINITIAL 2
#define OBJECTSTATE_INITIALIZER 3

/* S_OBJECT_FNC */
/* Object variable processing */
UINT s_object_fnc()
{
	static INT inheritableflg;
	static UINT datatype;
	static UCHAR sc;
	
	if (1 == rulecnt) {
		codebufcnt = 0;
		inheritableflg = FALSE;
		sc = OBJECTSTATE_START;
		if (recordlevel) recdefhdr(label);
		datatype = codebuf[0];
		if (datatype == 234) {	
			putdata1(0xF3);  /* global prefix */
			putdata((UCHAR *) label, (INT) (strlen(label) + 1));
		}
		else if (withnamesflg) putdataname(label);
	}
	switch (sc) {
	case OBJECTSTATE_START:
		if (TOKEN_AMPERSAND == tokentype) {  /* OBJECT & (inherited or inheritable) variable */
			if (inheritableflg) {
				error(ERROR_SYNTAX);
				return(NEXT_TERMINATE);
			}
			if (datatype == 234) {
				error(ERROR_NOGLOBALINHERITS);
				return(NEXT_TERMINATE);
			}
			if ('&' == line[linecnt]) {  /* inherited */
				linecnt++;
				s_inherited_fnc(TYPE_OBJECT);
			}
			else if (!classlevel) error(ERROR_INHRTABLENOTALLOWED);
			else {
				inheritableflg = TRUE;
				return(RULE_PARSEVAR + NEXT_NOPARSE);
			}
			return(NEXT_TERMINATE);
		}
		symvalue = dataadr;
		if (tokentype == TOKEN_ATSIGN) {  /* OBJECT @ (address) variable */
			if (datatype == 234) {
				error(ERROR_GLOBALADRVAR);
				return(NEXT_TERMINATE);
			}
			symtype = TYPE_OBJECT | TYPE_PTR;
			if (line[linecnt] && charmap[line[linecnt]] != 2) sc = OBJECTSTATE_COMMAINITIAL;
			else putdatahhll(0xFC0D);
		}
		else {  /* OBJECT variable */
			symtype = TYPE_OBJECT;
			putdatahhll(0xFC18);
		}
		break;
	case OBJECTSTATE_COMMAINITIAL:
		whitespace();
		scantoken(SCAN_UPCASE);
		if (tokentype != TOKEN_WORD || strcmp((CHAR *) token, "INITIAL")) {
			error(ERROR_SYNTAX);
			return(NEXT_TERMINATE);
		}
		whitespace();
		sc = OBJECTSTATE_INITIALIZER;
		return(RULE_PARSEVAR + NEXT_NOPARSE);
	case OBJECTSTATE_INITIALIZER:
		if (tokentype != TOKEN_WORD) {
			error(ERROR_SYNTAX);
			return(NEXT_TERMINATE);
		}
		if (getdsym((CHAR *) token) == -1) {
			error(ERROR_VARNOTFOUND);
			return(NEXT_TERMINATE);
 		}
		if (symtype != TYPE_OBJECT) {
			error(ERROR_BADVARTYPE);
			return(NEXT_TERMINATE);
		}
		symtype = TYPE_OBJECT | TYPE_PTR;
		putdatahhll(0xFC5D);
		putdatallhh((USHORT) symvalue);
		putdata1((UCHAR)(symvalue >> 16));
		if (line[linecnt] && charmap[line[linecnt]] != 2) error(ERROR_SYNTAX);
		break;
	}
	if (sc == OBJECTSTATE_START || sc == OBJECTSTATE_COMMAINITIAL) {
		if (!recdefonly) {
			if ((symtype & TYPE_PTR) || datatype == 234) dataadr += 6;  /* address or global */
			else dataadr += 44;
		}
		if (recordlevel) putrecdsym(label);
		else if (!recdefonly) putdsym(label, inheritableflg);
		if (sc == OBJECTSTATE_COMMAINITIAL) return(RULE_NOPARSE + NEXT_COMMA);
	}
	if (recordlevel) {
		if ((symtype & TYPE_PTR) || datatype == 234) recdeftail(symtype, 6);
		else recdeftail(symtype, 44);
	}
	return(NEXT_TERMINATE);
}

/* S_CLASS_FNC */
/* Class definition/declaration processing */
UINT s_class_fnc()
{
	static CHAR parent[LABELSIZ], makename[LABELSIZ], destname[LABELSIZ];
	INT namelen;

	if (1 == rulecnt) {
		codebufcnt = 0;
		if (TOKEN_WORD == tokentype && !strcmp((CHAR *) token, "DEFINITION")) {	/* class definition */
			parent[0] = makename[0] = destname[0] = 0;
			kwdupchk(0xFF);
			parsestate = 6;
			putclass(label, NULL, CLASS_DEFINITION);
			if (line[linecnt] && charmap[line[linecnt]] != 2) return(RULE_NOPARSE + NEXT_LIST);
			startclassdef(label, NULL, NULL, NULL);
			return(NEXT_TERMINATE);
		}
		else {	/* class declaration */
			if (TOKEN_WORD == tokentype && !strcmp((CHAR *) token, "MODULE")) {	
				if ('=' == line[linecnt++]) {
					parsestate = 2;
					return(RULE_LITERAL + NEXT_NOPARSE);
				}
				else {
					error(ERROR_SYNTAX);
					return(NEXT_TERMINATE);
				}
			}
			else {
				putclass(label, NULL, CLASS_DECLARATION);
				return(NEXT_TERMINATE);
			}
		}
	}
	else {
		switch (parsestate) {
			case 1:	/* just scanned a keyword in a CLASS DEFINITION statement */
				if (tokentype != TOKEN_WORD) {
					error(ERROR_SYNTAX);
					return(NEXT_TERMINATE);
				}
				switch (token[0]) {
					case 'D':
						if (!strcmp((CHAR *) token, "DESTROY")) {
							if (kwdupchk(CKW_DESTROY) < 0) return(NEXT_TERMINATE);
							if (line[linecnt++] != '=') {
								error(ERROR_SYNTAX);
								return(NEXT_TERMINATE);
							}
							parsestate = 3;
							return(RULE_PARSEVAR + NEXT_LIST);
						}
						break;
					case 'M':
						if (!strcmp((CHAR *) token, "MAKE")) {
							if (kwdupchk(CKW_MAKE) < 0) return(NEXT_TERMINATE);
							if (line[linecnt++] != '=') {
								error(ERROR_SYNTAX);
								return(NEXT_TERMINATE);
							}
							parsestate = 4;
							return(RULE_PARSEVAR + NEXT_LIST);
						}
						break;
					case 'P':
						if (!strcmp((CHAR *) token, "PARENT")) {
							if (kwdupchk(CKW_PARENT) < 0) return(NEXT_TERMINATE);
							if (line[linecnt++] != '=') {
								error(ERROR_SYNTAX);
								return(NEXT_TERMINATE);
							}
							parsestate = 5;
							return(RULE_PARSEVAR + NEXT_LIST);
						}
						break;
				}
				if (rulecnt != 2) error(ERROR_SYNTAX);
				else startclassdef(label, NULL, NULL, NULL);
				return(NEXT_TERMINATE);
			case 2:	/* just scanned a module name literal on a CLASS declaration statement */
				putclass(label, (CHAR *) token, CLASS_DECLARATION);
				return(NEXT_TERMINATE);
			case 3:	/* just scanned a program label following a DESTROY keyword */	
				if (tokentype != TOKEN_WORD) {
					error(ERROR_SYNTAX);
					return(NEXT_TERMINATE);
				}
				namelen = (int)strlen((CHAR *) token);
				if (namelen >= LABELSIZ) {
					error(ERROR_LABELTOOLONG);
					return(NEXT_TERMINATE);
				}
				memcpy(destname, token, (size_t) (namelen + 1));
				break;
			case 4:	/* just scanned a program label following a MAKE keyword */	
				if (tokentype != TOKEN_WORD) {
					error(ERROR_SYNTAX);
					return(NEXT_TERMINATE);
				}
				namelen = (int)strlen((CHAR *) token);
				if (namelen >= LABELSIZ) {
					error(ERROR_LABELTOOLONG);
					return(NEXT_TERMINATE);
				}
				memcpy(makename, (CHAR *) token, (size_t)(namelen + 1));
				break;
			case 5:	/* just scanned a class name following a PARENT keyword */	
				if (tokentype != TOKEN_WORD) {
					error(ERROR_SYNTAX);
					return(NEXT_TERMINATE);
				}
				namelen = (int)strlen((CHAR *) token);
				if (namelen >= LABELSIZ) {
					error(ERROR_LABELTOOLONG);
					return(NEXT_TERMINATE);
				}
				memcpy(parent, token, (size_t) (namelen + 1));
				break;
			case 6:	/* just scanned a list delimiter following DEFINITION keyword */
				break;
			default:
				death(DEATH_INTERNAL1);
		}
	}
	parsestate = 1;
	if (delimiter) return(RULE_PARSEUP + NEXT_NOPARSE);
	startclassdef(label, parent, makename, destname);
	return(NEXT_TERMINATE);
}

/* S_ENDCLASS_FNC */
/* End class definition block processing */
UINT s_endclass_fnc()
{
	codebufcnt = 0;
	endclassdef();
	return(NEXT_TERMINATE);
}

/* S_METHOD_FNC */
/* METHOD declaration processing */
UINT s_method_fnc()
{
	codebufcnt = 0;
	usepsym(label, LABEL_METHOD);
	return(NEXT_TERMINATE);
}

/* S_MAKE_FNC */
/* MAKE statement processing */
UINT s_make_fnc()
{
#define MAKE_STATE_CALLLIST		0
#define MAKE_STATE_CLASSCVARLIT	1
#define MAKE_STATE_MODCVARLIT		2
	static UCHAR linecntsave;
	static UCHAR parsestate;
	CHAR modname[LABELSIZ];

	if (1 == rulecnt) {
		linecntsave = linecnt;
		return(RULE_PARSEVAR + NEXT_NOPARSE);
	}
	if (2 == rulecnt) {
		if (TOKEN_LITERAL == tokentype) {		/* MAKE <obj>, <cvarlit>, <cvarlit> [, <CALL list> ] syntax */
			parsestate = MAKE_STATE_CLASSCVARLIT;
			linecnt = linecntsave;
			return(RULE_CSRC + NEXT_PREP);	/* parse class name <cvarlit> */
		}
		if (tokentype != TOKEN_WORD) {
			error(ERROR_SYNTAX);
			return(NEXT_TERMINATE);
		}
		if (getclass((CHAR *) token, modname) == RC_ERROR) {
			if (getdsym((CHAR *) token) != -1) { 
				parsestate = MAKE_STATE_CLASSCVARLIT;
				linecnt = linecntsave;
				return(RULE_CSRC + NEXT_PREP);	/* parse class name <cvarlit> */
			}
			else {
				error(ERROR_CLASSNOTFOUND);
				return(NEXT_TERMINATE);
			}
		}

		/* MAKE <obj>, <class> [, <CALL list> ] syntax */
		parsestate = MAKE_STATE_CALLLIST;
		putcodeadr(makeclit(token));
		putcodeadr(makeclit((UCHAR *) modname));
		return(RULE_NOPARSE + NEXT_PREPOPT);
	}
	if (MAKE_STATE_CLASSCVARLIT == parsestate) {
		parsestate = MAKE_STATE_MODCVARLIT;
		return(RULE_CSRC + NEXT_PREPOPT);	/* parse mod name <cvarlit> */
	}
	if (!delimiter) {			/* no CALL list in this MAKE statement */
		putcode1(0xFF);
		return(NEXT_TERMINATE);
	}
	return(0);
}

/* S_INHERITED_FNC */
/* Inherited variable processing */
/* vartype variable type code, TYPE_??? */
void s_inherited_fnc(UINT vartype)
{
	UCHAR typebyte;
	if (!childclsflag) {
		error(ERROR_INHRTDNOTALLOWED);
		return;
	}
	if (recordlevel) {
		error(ERROR_NORECORDINHRT);
		return;
	}
	if (rlevel) {
		error(ERROR_NOROUTINEINHRT);
		return;
	}
	switch (vartype) {
		case TYPE_CVAR:
			if ('@' == line[linecnt]) {
				if ('[' == line[++linecnt]) {
					if (']' == line[++linecnt]) {
						symtype = TYPE_CARRAY1 | TYPE_PTR; 
						linecnt++;
						typebyte = 0xB1;		/* CHAR &&@[] */
					}
					else if (',' == line[linecnt] &&
						    ']' == line[linecnt + 1]) {
						symtype = TYPE_CARRAY2 | TYPE_PTR; 
						linecnt += 2;
						typebyte = 0xB2;	/* CHAR &&@[,] */
					}
					else if (',' == line[linecnt] &&
						    ',' == line[linecnt + 1] &&
						    ']' == line[linecnt + 2]) {
						symtype = TYPE_CARRAY3 | TYPE_PTR; 
						linecnt += 3;
						typebyte = 0xB3;	/* CHAR &&@[,,] */
					}
					else {
						error(ERROR_SYNTAX);
						return;
					}
				}
				else {
					symtype = TYPE_CVARPTR; 
					typebyte = 0xA0;	/* CHAR &&@ */
				}
			}
			else if ('[' == line[linecnt]) {
				if (']' == line[++linecnt]) {
					if ('@' == line[++linecnt]) {
						symtype = TYPE_CVARPTRARRAY1;
						linecnt++;
						typebyte = 0x91;		/* CHAR &&[]@ */
					}
					else {
						symtype = TYPE_CARRAY1;
						typebyte = 0x81;		/* CHAR &&[] */
					}
				}
				else if (',' == line[linecnt] &&
					    ']' == line[linecnt + 1]) {
					linecnt += 2;
					if ('@' == line[linecnt]) {
						symtype = TYPE_CVARPTRARRAY2;
						linecnt++;
						typebyte = 0x92;	/* CHAR &&[,]@ */
					}
					else {
						symtype = TYPE_CARRAY2;
						typebyte = 0x82;	/* CHAR &&[,] */
					}
				}
				else if (',' == line[linecnt] &&
					    ',' == line[linecnt + 1] &&
					    ']' == line[linecnt + 2]) {
					linecnt += 3;
					if ('@' == line[linecnt]) {
						symtype = TYPE_CVARPTRARRAY3;
						linecnt++;
 						typebyte = 0x93;	/* CHAR &&[,,]@ */
					}
					else {
						symtype = TYPE_CARRAY3;
						typebyte = 0x83;	/* CHAR &&[,,] */
					}
				}
				else {
					error(ERROR_SYNTAX);
					return;
				}
			}
 			else {
				symtype = TYPE_CVAR; 
				typebyte = 0x70;	/* CHAR && */
			}
			break;
		case TYPE_NVAR:
			if ('@' == line[linecnt]) {
				if ('[' == line[++linecnt]) {
					if (']' == line[++linecnt]) {
						symtype = TYPE_NARRAY1 | TYPE_PTR; 
						linecnt++;
						typebyte = 0xB5;	/* NUM &&@[] */
					}
					else if (',' == line[linecnt] &&
						    ']' == line[linecnt + 1]) {
						symtype = TYPE_NARRAY2 | TYPE_PTR; 
						linecnt += 2;
						typebyte = 0xB6;	/* NUM &&@[,] */
					}
					else if (',' == line[linecnt] &&
						    ',' == line[linecnt + 1] &&
						    ']' == line[linecnt + 2]) {
						symtype = TYPE_NARRAY3 | TYPE_PTR; 
						linecnt += 3;
						typebyte = 0xB7;	/* NUM &&@[,,] */
					}
					else {
						error(ERROR_SYNTAX);
						return;
					}
				}
				else {
					symtype = TYPE_NVARPTR; 
					typebyte = 0xA1;	/* NUM &&@ */
				}
			}
			else if ('[' == line[linecnt]) {
				if (']' == line[++linecnt]) {
					if ('@' == line[++linecnt]) {
						symtype = TYPE_NVARPTRARRAY1;
						linecnt++;
						typebyte = 0x95;	/* NUM &&[]@ */
					}
					else {
						symtype = TYPE_NARRAY1;
						typebyte = 0x85;	/* NUM &&[] */
					}
				}
				else if (',' == line[linecnt] &&
					    ']' == line[linecnt + 1]) {
					linecnt += 2;
					if ('@' == line[linecnt]) {
						symtype = TYPE_NVARPTRARRAY2;
						linecnt++;
						typebyte = 0x96;	/* NUM &&[,]@ */
					}
					else {
						symtype = TYPE_NARRAY2;
						typebyte = 0x86;	/* NUM &&[,] */
					}
				}
				else if (',' == line[linecnt] &&
					    ',' == line[linecnt + 1] &&
					    ']' == line[linecnt + 2]) {
					linecnt += 3;
					if ('@' == line[linecnt]) {
						symtype = TYPE_NVARPTRARRAY3;
						linecnt++;
						typebyte = 0x97;	/* NUM &&[,,]@ */
					}
					else {
						symtype = TYPE_NARRAY3;
						typebyte = 0x87;	/* NUM &&[,,] */
					}
				}
				else {
					error(ERROR_SYNTAX);
					return;
				}
			}
 			else {
				symtype = TYPE_NVAR; 
				typebyte = 0x71;	/* NUM && */
			}
			break;
		case TYPE_VVARPTR:
			if ('@' == line[linecnt]) {
				symtype = TYPE_VVARPTR; 
				linecnt++;
				typebyte = 0xA7;		/* VAR &&@ */
			}
			else if ('[' == line[linecnt]) {
				if (']' == line[++linecnt] &&
				    '@' == line[linecnt + 1]) {
					symtype = TYPE_VVARPTRARRAY1; 
					linecnt += 2;
					typebyte = 0x99;	/* VAR &&[]@ */
				}
				else if (',' == line[linecnt] &&
					    ']' == line[linecnt + 1] &&
					    '@' == line[linecnt + 2]) {
					symtype = TYPE_VVARPTRARRAY2; 
					linecnt += 3;
					typebyte = 0x9A;	/* VAR &&[,]@ */
				}
				else if (',' == line[linecnt] &&
					    ',' == line[linecnt + 1] &&
					    ']' == line[linecnt + 2] &&
					    '@' == line[linecnt + 3]) {
					symtype = TYPE_VVARPTRARRAY3; 
					linecnt += 4;
					typebyte = 0x9B;	/* VAR &&[,,]@ */
				}
				else {
					error(ERROR_SYNTAX);
					return;
				}
			}
			else {
				error(ERROR_SYNTAX);
				return;
			}
			break;
		case TYPE_LIST:
			if ('@' == line[linecnt]) {
				symtype = TYPE_LIST | TYPE_PTR;
				linecnt++;
				typebyte = 0xA5;		/* LIST &&@ */
			}
			else {
				/* LIST & not supported */
				error(ERROR_LISTNOTINHERITABLE);
				return;
			}
			break;
		case TYPE_FILE:
			if ('@' == line[linecnt]) {
				symtype = TYPE_FILE | TYPE_PTR;
				linecnt++;
				typebyte = 0xA2;		/* FILE &&@ */
			}
			else {
				symtype = TYPE_FILE;
				typebyte = 0x72;		/* FILE && */
			}
			break;
		case TYPE_IFILE:
			if ('@' == line[linecnt]) {
				symtype = TYPE_IFILE | TYPE_PTR;
				linecnt++;
				typebyte = 0xA3;		/* IFILE &&@ */
			}
			else {
				symtype = TYPE_IFILE;
				typebyte = 0x73;		/* IFILE && */
			}
			break;
		case TYPE_AFILE:
			if ('@' == line[linecnt]) {
				symtype = TYPE_AFILE | TYPE_PTR;
				linecnt++;
				typebyte = 0xA4;		/* AFILE &&@ */
			}
			else {
				symtype = TYPE_AFILE;
				typebyte = 0x74;		/* AFILE && */
			}
			break;
		case TYPE_COMFILE:
			if ('@' == line[linecnt]) {
				symtype = TYPE_COMFILE | TYPE_PTR;
				linecnt++;
				typebyte = 0xA6;		/* COMFILE &&@ */
			}
			else {
				symtype = TYPE_COMFILE;
				typebyte = 0x76;		/* COMFILE && */
			}
			break;
		case TYPE_PFILE:
			if ('@' == line[linecnt]) {
				symtype = TYPE_PFILE | TYPE_PTR;
				linecnt++;
				typebyte = 0xAC;		/* PFILE &&@ */
			}
			else {
				symtype = TYPE_PFILE;
				typebyte = 0x7C;		/* PFILE && */
			}
			break;
		case TYPE_DEVICE:
			if ('@' == line[linecnt]) {
				symtype = TYPE_DEVICE | TYPE_PTR;
				linecnt++;
				typebyte = 0xAB;		/* DEVICE &&@ */
			}
			else {
				symtype = TYPE_DEVICE;
				typebyte = 0x7B;		/* DEVICE && */
			}
			break;
		case TYPE_QUEUE:
			if ('@' == line[linecnt]) {
				symtype = TYPE_QUEUE | TYPE_PTR;
				linecnt++;
				typebyte = 0xA9;		/* QUEUE &&@ */
			}
			else {
				symtype = TYPE_QUEUE;
				typebyte = 0x79;		/* QUEUE && */
			}
			break;
		case TYPE_RESOURCE:
			if ('@' == line[linecnt]) {
				symtype = TYPE_RESOURCE | TYPE_PTR;
				linecnt++;
				typebyte = 0xAA;		/* RESOURCE &&@ */
			}
			else {
				symtype = TYPE_RESOURCE;
				typebyte = 0x7A;		/* RESOURCE && */
			}
			break;
		case TYPE_IMAGE:
			if ('@' == line[linecnt]) {
				symtype = TYPE_IMAGE | TYPE_PTR;
				linecnt++;
				typebyte = 0xA8;		/* IMAGE &&@ */
			}
			else {
				symtype = TYPE_IMAGE;
				typebyte = 0x78;		/* IMAGE && */
			}
			break;
		case TYPE_OBJECT:
			if ('@' == line[linecnt]) {
				symtype = TYPE_OBJECT | TYPE_PTR;
				linecnt++;
				typebyte = 0xAD;		/* OBJECT &&@ */
			}
			else {
				symtype = TYPE_OBJECT;
				typebyte = 0x7D;		/* OBJECT && */
			}
			break;
		default:
			death(DEATH_INTERNAL5);
	}
	if (line[linecnt] && charmap[line[linecnt]] != 2) {	/* check for whitespace preceeding comments */
		error(ERROR_SYNTAX);
		return;
	}
	symvalue = dataadr;
	dataadr += 6;
	putdatahhll((UINT) 0xFC00 + typebyte);
	putdsym(label, TRUE);
}
