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

#ifdef DBCMP
#define EXTERN
#else
#define EXTERN extern
#endif

typedef struct syntaxstruct {
	UINT rule;			/* rule consisting of RULE_ and NEXT_ components */
	UINT (*rulefnc)(void);	/* function to call after processing the rule */
} SYNTAX;

typedef struct verbstruct {
	struct verbstruct * nextptr;	/* forward linked list pointer */
	USHORT verbcode;			/* verb code */
	SYNTAX * syntax;			/* pointer to syntax rules structure */
	CHAR *verbname;			/* upper case, zero-delimited verb name */
} VERB;

typedef struct ctlcodestruct {
	struct ctlcodestruct * nextptr;
	USHORT cccode;
	USHORT cctype;
	CHAR *ccname;
} CTLCODE;

#define DSPFLAG_VERBOSE	0x0001	/* display completion status */
#define DSPFLAG_COUNT	0x0002	/* display line count */
#define DSPFLAG_INCLUDE	0x0004	/* display include names */
#define DSPFLAG_PROGRAM	0x0008	/* display program name */
#define DSPFLAG_STATS	0x0010	/* display statistics */
#define DSPFLAG_START	0x0020	/* test for start of compilation listing */
#define DSPFLAG_COMPILE	0x0040	/* display compilation listing */
#define DSPFLAG_XML			0x0080	/* write to xml file */
#define DSPFLAG_CONSOLE		0x0100	/* running as console application */
#define DSPFLAG_XTRASTATS	0x0200	/* display statistics */

#define DBCMPFLAG_VERSION8	0x0001
#define DBCMPFLAG_VERSION9	0x0002

EXTERN int dbcmpflags;	/* see above values */

#define RULEFNC UINT (*)(void)

#define LABELSIZ 32
struct recordaccess {
	CHAR name[LABELSIZ];
	UCHAR rlevel;					/* routine level for variable name visibility */
	USHORT routineid;				/* routine level id number for variables */
	USHORT classid;				/* class identifier, 0 means symbol defined outside of class */
	INT defptr;	    /* pointer into recdef array */
	INT nextrec;	    /* pointer to next recordaccess struct in recindex2 */
};

/* macros defined for data alignment considerations */
#if (OS_WIN32 && defined(I386))
#define move2hhll(src, dest) (* (USHORT *) (dest) = (USHORT) (src))
#define load2hhll(y) (* (USHORT *) (y))
#define move4hhll(src, dest) (* (UINT *) (dest) = (UINT) (src))
#define load4hhll(y) (* (UINT *) (y))
#else
#define move2hhll(src, dest) (* (UCHAR *) (dest) = ((UCHAR) ((src) >> 8)), \
						* (UCHAR *) (dest + 1) = (UCHAR) (src))
#define load2hhll(y) (USHORT) ((((USHORT) *(y)) << 8) + ((USHORT) *(y + 1)))
#define move4hhll(src, dest) (* (UCHAR *) (dest) = ((UCHAR) ((src) >> 24)), \
						* (UCHAR *) (dest + 1) = ((UCHAR) ((src) >> 16)), \
						* (UCHAR *) (dest + 2) = ((UCHAR) ((src) >> 8)), \
						* (UCHAR *) (dest + 3) = (UCHAR) (src))
/**
 * Takes a pointer to a 4-byte area with a Big-Endian integer in it and returns
 * an integer in native format.
 */
#define load4hhll(y) \
	((((UINT) *(y)) << 24) + (((UINT) *(y + 1)) << 16) + (((UINT) *(y + 2)) << 8) + ((UINT) *(y + 3)))
#endif

/* basic parse rules, no checking is done */
#define RULE_NOPARSE 0
#define RULE_PARSE 1
#define RULE_PARSEUP 2
#define RULE_PARSEVAR 3

/* token type checking rules */
/* any RULE_xxxx with SRC in xxxx must be the first (low order) rule */
/* any RULE_xxxx with CC in xxxx must be the second rule (RULE_xxxx >> 8) */
/* RULE_LITERAL, RULE_SLITERAL and RULE_SCON must each be the second rule */
/* rule processing returns variable address in codebuf, unless otherwise noted */
#define RULE_MOVESRC 4		/* if literal, return its address in codebuf */
#define RULE_LOADSRC 5
#define RULE_MATHSRC 6		/* if literal, return its address in codebuf */
#define RULE_NSRC 7			/* if literal, return its address in codebuf */
#define RULE_NSRCDCON 8
#define RULE_NSRCSCON 9
#define RULE_NSRCCONST 10	/* if lit, return adr in codebuf, unless 1 byte signed constant */
#define RULE_NSRCEQU 11		/* if literal, return its address in codebuf */
#define RULE_CNSRCCONST 12	/* if lit, return adr in codebuf, unless 1 byte signed numeric constant */
#define RULE_CNSRC 13		/* if literal, return its address in codebuf */
#define RULE_CSRC 14		/* if literal, return its address in codebuf */
#define RULE_CSRCCONST 15	/* if lit, return adr in codebuf, unless 1 byte constant */
#define RULE_CDATADEST 16
#define RULE_DISPLAYSRC 17
#define RULE_PRINTSRC 18
#define RULE_CNDATASRC 19	/* if literal, return its address in codebuf */
#define RULE_CVAR 20
#define RULE_CVARPTR 21
#define RULE_CARRAY 22
#define RULE_CARRAY1 23
#define RULE_CARRAY2 24
#define RULE_CARRAY3 25
#define RULE_NVAR 26
#define RULE_NARRAY 27
#define RULE_NARRAYELEMENT 28
#define RULE_CARRAYELEMENT 29
#define RULE_NARRAY1 30
#define RULE_NARRAY2 31
#define RULE_NARRAY3 32
#define RULE_ARRAY 33
#define RULE_CNVAR 34
#define RULE_CNDATADEST 35
#define RULE_MOVEDEST 36
#define RULE_CNVVARPTR 37
#define RULE_CNVVARSRC 38
#define RULE_MAKEVARDEST 39
#define RULE_VVARPTR 40
#define RULE_MATHDEST 41
#define RULE_LIST 42
#define RULE_EQUATE 43
#define RULE_DCON 44		/* returns non-negative integer value in symvalue */
#define RULE_XCON 45		/* returns non-negative integer value in symvalue */
#define RULE_SCON 46		/* returns signed integer value in symvalue */
#define RULE_LITERAL 47  	/* only returns literal in token */
#define RULE_SLITERAL 48 	/* only returns sliteral in token */
#define RULE_KYCC 49
#define RULE_DSCC 50
#define RULE_PRCC 51
#define RULE_RDCC 52
#define RULE_WTCC 53
#define RULE_CHGCC 54
#define RULE_LBLGOTO 55		/* returns label number in symvalue */
#define RULE_LBLSRC 56 		/* returns label number in symvalue */
#define RULE_LABEL 57
#define RULE_LBLVAREXT 58
#define RULE_LBLDEST 59	/* returns label number in symvalue */
#define RULE_FILE 60
#define RULE_AFILE 61
#define RULE_IFILE 62
#define RULE_AFIFILE 63
#define RULE_PFILE 64
#define RULE_COMFILE 65
#define RULE_DEVICE 66
#define RULE_RESOURCE 67
#define RULE_IMAGE 68
#define RULE_QUEUE 69
#define RULE_AFIDEVRES 70
#define RULE_ANYSRC 71
#define RULE_ANYVAR 72
#define RULE_ANYVARPTR 73
#define RULE_TRAPEVENT 74
#define RULE_OPTIONAL 75
#define RULE_EMPTYLIST 76
#define RULE_ASTERISK 77
#define RULE_OBJECT 78
#define RULE_OBJECTPTR 79
#define RULE_DAFB 80
#define RULE_CNSRCXCON 81

/* delimiter rules */
#define NEXT_NOPARSE 0x00000000
#define NEXT_OPT 0x01000000
#define NEXT_COMMA 0x02000000
#define NEXT_COMMAOPT (NEXT_COMMA | NEXT_OPT)
#define NEXT_SEMICOLON 0x04000000
#define NEXT_SEMIOPT (NEXT_SEMICOLON | NEXT_OPT)
#define NEXT_PREP 0x06000000
#define NEXT_PREPOPT (NEXT_PREP | NEXT_OPT)
#define NEXT_GIVINGOPT (0x08000000 | NEXT_OPT)
#define NEXT_PREPGIVING 0x0A000000
#define NEXT_IFOPT (0x0C000000 | NEXT_OPT)
#define NEXT_IFPREPOPT (0x0E000000 | NEXT_OPT)
#define NEXT_BLANKS 0x10000000
#define NEXT_COMMASEMI 0x12000000
#define NEXT_COMMARBRKT 0x14000000
#define NEXT_RBRACKET 0x16000000
#define NEXT_CASE 0x18000000
#define NEXT_LIST 0x1A000000
#define NEXT_SEMILIST 0x1C000000
#define NEXT_SEMILISTNOEND 0x1E000000
#define NEXT_RELOP 0x20000000
#define NEXT_COLON 0x22000000
#define NEXT_COLONOPT (NEXT_COLON | NEXT_OPT)
#define NEXT_REPEAT 0x40000000
#define NEXT_END (UINT) 0x80000000
#define NEXT_ENDOPT (UINT) (NEXT_END | NEXT_OPT)
#define NEXT_TERMINATE 0xFFFFFFFFL

#define CCPARSE_NVARCON 1
#define CCPARSE_2NVARCON 2
#define CCPARSE_NVARNCON 3
#define CCPARSE_4NVARCON 4
#define CCPARSE_CVARLIT_NVARCON 5
#define CCPARSE_CVAR_NVARCON 6
#define CCPARSE_SCRLR 7
#define CCPARSE_SCREND 8
#define CCPARSE_CVARLIT 9
#define CCPARSE_PNVAR 10
#define CCPARSE_PCON 11

#define TYPE_PTR 0x01
#define TYPE_CVAR 2
#define TYPE_CVARPTR (TYPE_CVAR | TYPE_PTR)
#define TYPE_CARRAY1 4
#define TYPE_CARRAY2 6
#define TYPE_CARRAY3 8
#define TYPE_CVARPTRARRAY1 (10 | TYPE_PTR)
#define TYPE_CVARPTRARRAY2 (12 | TYPE_PTR)
#define TYPE_CVARPTRARRAY3 (14 | TYPE_PTR)
#define TYPE_NVAR 16
#define TYPE_NVARPTR (TYPE_NVAR | TYPE_PTR)
#define TYPE_NARRAY1 18
#define TYPE_NARRAY2 20
#define TYPE_NARRAY3 22
#define TYPE_NVARPTRARRAY1 (24 | TYPE_PTR)
#define TYPE_NVARPTRARRAY2 (26 | TYPE_PTR)
#define TYPE_NVARPTRARRAY3 (28 | TYPE_PTR)
#define TYPE_VVARPTR (30 | TYPE_PTR)
#define TYPE_VVARPTRARRAY1 (32 | TYPE_PTR)
#define TYPE_VVARPTRARRAY2 (34 | TYPE_PTR)
#define TYPE_VVARPTRARRAY3 (36 | TYPE_PTR)
#define TYPE_LIST 38
#define TYPE_CVARLIST 40
#define TYPE_FILE 42
#define TYPE_IFILE 44
#define TYPE_AFILE 46
#define TYPE_COMFILE 48
#define TYPE_PFILE 50
#define TYPE_DEVICE 52
#define TYPE_DEVICEPTR (TYPE_DEVICE | TYPE_PTR)
#define TYPE_QUEUE 54
#define TYPE_RESOURCE 56
#define TYPE_RESOURCEPTR (TYPE_RESOURCE | TYPE_PTR)
#define TYPE_IMAGE 58
#define TYPE_IMAGEPTR (TYPE_IMAGE | TYPE_PTR)
#define TYPE_OBJECT 60
#define TYPE_OBJECTPTR (TYPE_OBJECT | TYPE_PTR)
#define TYPE_EQUATE 62
#define TYPE_MAXDATAVAR 126
#define TYPE_EXPRESSION 127
#define TYPE_PLABEL 128		/* program label */
#define TYPE_PLABELVAR 129	/* label variable */
#define TYPE_PLABELXREF 130	/* external label reference */
#define TYPE_PLABELMREF 131	/* method label reference */
#define TYPE_PLABELXDEF 132	/* external label definition */
#define TYPE_PLABELMDEF 133	/* method label definition */
#define TYPE_CLASS 200
#define TYPE_DECIMAL 250
#define TYPE_INTEGER 251
#define TYPE_LITERAL 252
#define TYPE_NUMBER 253
#define TYPE_BYTECONST 254
#define TYPE_KEYWORD 255

#define CC_KEYIN 0x01
#define CC_DISPLAY 0x02
#define CC_SCROLL 0x04
#define CC_GRAPHIC 0x08
#define CC_PRINT 0x10
#define CC_READ 0x20
#define CC_WRITE 0x40
#define CC_SPECIAL 0x80
#define CC_CHANGE 0x100

#define TOKEN_WORD 7
#define TOKEN_LITERAL 8
#define TOKEN_NUMLIT 9
#define TOKEN_NUMBER 10
#define TOKEN_LESS 11
#define TOKEN_GREATER 12
#define TOKEN_NOTLESS 13
#define TOKEN_NOTGREATER 14
#define TOKEN_EQUAL 15
#define TOKEN_NOTEQUAL 16
#define TOKEN_ATSIGN 17
#define TOKEN_POUND 18
#define TOKEN_BANG 19
#define TOKEN_POUNDSIGN 20
#define TOKEN_PERCENT 21
#define TOKEN_AMPERSAND 22
#define TOKEN_LPAREND 23
#define TOKEN_RPAREND 24
#define TOKEN_ASTERISK 25
#define TOKEN_PLUS 26
#define TOKEN_MINUS 27
#define TOKEN_COMMA 28
#define TOKEN_DIVIDE 29
#define TOKEN_SEMICOLON 30
#define TOKEN_COLON 31
#define TOKEN_LBRACKET 32
#define TOKEN_RBRACKET 33
#define TOKEN_TILDE 34
#define TOKEN_ORBAR 35
#define TOKEN_OR 36
#define TOKEN_IF 37
#define TOKEN_PREP 38
#define TOKEN_DBLQUOTE 39
#define TOKEN_PERIOD 40
#define TOKEN_TO 41
#define TOKEN_FROM 42
#define TOKEN_IN 43
#define TOKEN_INTO 44
#define TOKEN_BY 45
#define TOKEN_OF 46
#define TOKEN_WITH 47
#define TOKEN_USING 48

#define ERROR_ERROR 0
#define ERROR_VARNOTFOUND 1
#define ERROR_BADVARNAME 2
#define ERROR_BADVARTYPE 3
#define ERROR_LABELNOTFOUND 4
#define ERROR_BADLABEL 5
#define ERROR_VARTOOBIG 6
#define ERROR_DUPVAR 7
#define ERROR_DUPLABEL 8
#define ERROR_NOLABEL 9
#define ERROR_INCLUDENOTFOUND 10
#define ERROR_INCLUDEFILEBAD 11
#define ERROR_DBCFILEBAD 12
#define ERROR_DBGFILEBAD 13
#define ERROR_CLOCK 14
#define ERROR_OPENMODE 15
#define ERROR_BADVERB 16
#define ERROR_MISSINGSEMICOLON 17
#define ERROR_MISSINGIF 18
#define ERROR_MISSINGENDIF 19
#define ERROR_MISSINGLOOP 20
#define ERROR_MISSINGREPEAT 21
#define ERROR_MISSINGSWITCH 22
#define ERROR_MISSINGENDSWITCH 23
#define ERROR_MISSINGXIF 24
#define ERROR_INCLUDEDEPTH 25
#define ERROR_BADKEYWORD 26
#define ERROR_BADRECSIZE 27
#define ERROR_BADKEYLENGTH 28
#define ERROR_BADNUMLIT 29
#define ERROR_BADDCON 30
#define ERROR_DCONTOOBIG 31
#define ERROR_NOCOLON 32
#define ERROR_BADINTCON 33
#define ERROR_BADVALUE 34
#define ERROR_BADEQUATEUSE 35
#define ERROR_BADLISTCONTROL 36
#define ERROR_CCIFDEPTH 37
#define ERROR_IFDEPTH 38
#define ERROR_LOOPDEPTH 39
#define ERROR_SWITCHDEPTH 40
#define ERROR_BADLISTCOMMA 41
#define ERROR_BADCONTLINE 42
#define ERROR_CHRLITTOOBIG 43
#define ERROR_BADARRAYSPEC 44
#define ERROR_LINETOOLONG 45
#define ERROR_MAXDEFINE 46
#define ERROR_EXPSYNTAX 47
#define ERROR_BADPREPMODE 48
#define ERROR_BADPREP 49
#define ERROR_SYNTAX 50
#define ERROR_BADNUMSPEC 51
#define ERROR_BADLIT 52
#define ERROR_SQLSYNTAX 53
#define ERROR_SQLTOOBIG 54
#define ERROR_SQLFROM 55
#define ERROR_SQLINTO 56
#define ERROR_SQLVARMAX 57
#define ERROR_BADCHAR 58
#define ERROR_CCMATCH 59
#define ERROR_BADEXPTYPE 60
#define ERROR_BADGLOBAL 61
#define ERROR_BADPARMTYPE 62
#define ERROR_NOCASE 63
#define ERROR_EXPTOOBIG 64
#define ERROR_BADVERBNAME 65
#define ERROR_BADARRAYINDEX 66
#define ERROR_BADCOMMON 67
#define ERROR_NOIF 68
#define ERROR_NOTARRAY 69
#define ERROR_BADTABVALUE 70
#define ERROR_BADSRCREAD 71
#define ERROR_BADIFLEVEL 72
#define ERROR_BADLOOPLEVEL 73
#define ERROR_BADSWITCHLEVEL 74
#define ERROR_MISSINGLIST 75
#define ERROR_MISSINGLISTEND 76
#define ERROR_BADLISTENTRY 77
#define ERROR_BADSIZE 78
#define ERROR_GLOBALADRVAR 79
#define ERROR_NOATSIGN 81
#define ERROR_NOCCIF 82
#define ERROR_NOENTRIES 83
#define ERROR_NOSIZE 84
#define ERROR_NOH 85
#define ERROR_NOV 86
#define ERROR_NOKEYS 87
#define ERROR_DUPKEYWORD 88
#define ERROR_MISSINGOPERAND 89
#define ERROR_DUPUVERB 90
#define ERROR_TOOMANYNONPOS 91
#define ERROR_POSPARMNOTFIRST 92
#define ERROR_BADEXPOP 93
#define ERROR_ROUTINEDEPTH 94
#define ERROR_MISSINGROUTINE 95
#define ERROR_BADPLABEL 96
#define ERROR_BADKEYWORDPARM 97
#define ERROR_TOOMANYKEYWRDPARM 98
#define ERROR_TOOMANYEXPANSIONS 99
#define ERROR_BADENDKEYVALUE 100
#define ERROR_MISSINGRECORD 101
#define ERROR_RECORDREDEF 102
#define ERROR_RECORDDEPTH 103
#define ERROR_BADSCROLLCODE 104
#define ERROR_BADSCREND 105
#define ERROR_FIXVARFILE 106
#define ERROR_COMPUNCOMPFILE 107
#define ERROR_FIXCOMPFILE 108
#define ERROR_DUPNODUPFILE 109
#define ERROR_DIFFARRAYDIM 110
#define ERROR_LITTOOLONG 111
#define ERROR_MISSINGKEYS 112
#define ERROR_MISSINGRECORDEND 113
#define ERROR_EQNOTFOUND 114
#define ERROR_BADRPTCHAR 115
#define ERROR_BADEXTERNAL 116
#define ERROR_BADVERBLEN 117
#define ERROR_RECNOTFOUND 118
#define ERROR_MISSINGCCDIF 119
#define ERROR_MISSINGCCDENDIF 120
#define ERROR_DUPDEFINELABEL 121
#define ERROR_CLASSNOTFOUND 122
#define ERROR_DUPCLASS 123
#define ERROR_CLASSDEPTH 124
#define ERROR_MISSINGCLASSDEF 125
#define ERROR_AMBIGUOUSCLASSMOD 126
#define ERROR_LABELTOOLONG 127
#define ERROR_INHRTDNOTALLOWED 128
#define ERROR_NOGLOBALINHERITS 129
#define ERROR_NORECORDINHRT 130
#define ERROR_NOROUTINEINHRT 131
#define ERROR_UNKNOWNMETHOD 133
#define ERROR_BADMETHODCALL 134
#define ERROR_BADCOLORBITS 135
#define ERROR_BADINITLIST 136
#define ERROR_ILLEGALNORESET 137
#define ERROR_INVALCLASS 138
#define ERROR_INHRTABLENOTALLOWED 139
#define ERROR_LISTNOTINHERITABLE 140
#define ERROR_MISSINGCLASSEND 141
#define ERROR_CLOCKCLIENT 142
#define ERROR_INTERNAL 143

#define DEATH_BADPARM 201
#define DEATH_BADPARMVALUE 202
#define DEATH_BADLIBFILE 203
#define DEATH_SRCFILENOTFOUND 204
#define DEATH_BADSRCFILE 205
#define DEATH_NOMEM 206
#define DEATH_INITPARM 207
#define DEATH_LIBMAX 208
#define DEATH_BADPRTFILE 209
#define DEATH_BADWRKFILE 210
#define DEATH_BADXLTFILE 211
#define DEATH_BADWRKREAD 212
#define DEATH_BADDBCFILE 213
#define DEATH_BADDBGFILE 214
#define DEATH_BADSRCREAD 215
#define DEATH_BADDBCWRITE 216
#define DEATH_BADDBGWRITE 217
#define DEATH_PGMTOOBIG 218
#define DEATH_DATATOOBIG 219
#define DEATH_MAXROUTINES 220
#define DEATH_MAXEXTERNALS 221
#define DEATH_MAXSYMBOLS 222
#define DEATH_NOTSUPPORTED 223
#define DEATH_INTERNAL1 224
#define DEATH_INTERNAL2 225
#define DEATH_INTERNAL3 226
#define DEATH_INTERNAL4 227
#define DEATH_NOENVFILE 228
#define DEATH_UNSTAMPED 229
#define DEATH_BADWRKWRITE 230
#define DEATH_INTERNAL5 231
#define DEATH_INTERNAL6 232
#define DEATH_INTERNAL7 233
#define DEATH_BADXMLFILE 234
#define DEATH_BADXMLWRITE 235
#define DEATH_BADCFGFILE 236
#define DEATH_BADCFGOTHER 237
#define DEATH_INTERNAL8 238

#define WARNING_WARNING 500
#define WARNING_COMMACOMMENT 501
#define WARNING_IFCOMMENT 502
#define WARNING_PARENCOMMENT 503
#define WARNING_NOTENOUGHENDROUTINES 504
#define WARNING_2COMMENT 505

/* class flag values */
#define CLASS_DECLARATION 0
#define CLASS_DEFINITION 1

/* usepsym() commands */
#define LABEL_REF 0
#define LABEL_REFFORCE 1
#define LABEL_DEF 2
#define LABEL_VARDEF 3
#define LABEL_EXTERNAL 4
#define LABEL_ROUTINE 5
#define LABEL_METHOD 6
#define LABEL_METHODREF 7
#define LABEL_METHODREFDEF 8

/* scantoken() commands */
#define SCAN_NOUPCASE 0
#define SCAN_UPCASE 1
#define SCAN_EXPDEFINE 0
#define SCAN_NOEXPDEFINE 2
#define SCAN_NOSCANLIT 0
#define SCAN_SCANLITERAL 4
#define SCAN_ALPHANUM 0
#define SCAN_ALPHAONLY 8
#define SCAN_ADVANCE 0
#define SCAN_NOADVANCE 16
#define SCAN_ASTRKDEFINE 0
#define SCAN_NOASTRKDEFINE 32

#define LINESIZE 255			/* maximum source line size */
#define MAXLITERALSIZE 253
#define MAXNUMLITSIZE 31
#define VERBSIZE 32
#define RLEVELMAX 64

#define CLASSDATASIZE 32768		/* size of static class data buffer */
#define CODEBUFSIZE 65536		/* size of static code buffer */
#define MAINBUFINCREMENT 16384	/* increment size of growing main code buffer */
#define DATABUFINCREMENT 16384	/* increment size of growing data buffer */

#define TOKENSIZE 255
#define MAXINTERNALERROR8SIZE 256
#define SIMPLELABELBIAS ((UINT)0x8000)

EXTERN UCHAR line[LINESIZE + 1];	/* source line buffer */
EXTERN INT linecnt;			/* current position with line buffer */
EXTERN INT dataadr;			/* current data area offset */
EXTERN INT savdataadr;			/* data offset when line was read in */
EXTERN UINT savlinecnt;			/* line count when line was read in */
EXTERN INT codeadr;			/* current program area offset */
EXTERN INT savcodeadr;			/* current program area offset when next line was read in (modified for certain verbs) */
EXTERN INT savcodebufcnt;		/* save value of codebufcnt */
EXTERN CHAR verb[VERBSIZE];		/* the verb token */
EXTERN CHAR label[LABELSIZ];	/* source line label */
EXTERN UCHAR token[TOKENSIZE];	/* token returned by scantoken */
EXTERN INT tokentype;			/* token type returned by scantoken */
EXTERN INT delimiter;			/* token type of delimiter */
EXTERN INT delimtype;			/* token type of preposition delimiter (i.e. into, by, to, from, etc.) */
EXTERN UCHAR codebuf[CODEBUFSIZE];	/* current instruction code buffer */
EXTERN INT codebufcnt;			/* position within codebuf */
EXTERN INT rulecnt;				/* rule number of parsing this statement */
/*
 * 0 = no warnings,
 * 0x01 = level one warning messages, set by -k
 * 0x10 = warn if any comments seen on executable statement, set by -kx
 */
EXTERN INT warninglevel;
#define WARNING_LEVEL_1 0x0001
#define WARNING_LEVEL_2 0x0010

EXTERN INT warning_2fewendroutine;  /* Report unclosed routines at compile end */
EXTERN UCHAR nocompile;			/* %if control flag */
EXTERN INT dspflags;			/* display options */
EXTERN UCHAR liston;			/* liston control flag */
EXTERN UCHAR upcaseflg;			/* if TRUE, labels are case insensitive */
EXTERN UINT rlevel;				/* current routine level */
EXTERN UINT rlevelid[RLEVELMAX];	/* identifiers for all 16 routine levels */
EXTERN UINT rlevelidcnt;			/* id number for each routine */
EXTERN UINT symtype;			/* symbol type returned by get?sym */
EXTERN UINT symbasetype;			/* symbol type ignoring address variable */
EXTERN INT symvalue;			/* symbol value returned by get?sym */
EXTERN UCHAR filedflt;			/* file default file type */
EXTERN UCHAR ifiledflt;			/* ifile default file type */
EXTERN UCHAR afiledflt;			/* afile default file type */
EXTERN UCHAR dbgflg;			/* .dbg file is created */
EXTERN UINT undfnexist;			/* num undefined routine address variables in existance */
EXTERN INT curerrcnt;			/* number of errors for this line */
EXTERN UCHAR scrlparseflg;		/* set to TRUE when currently parsing a scroll list in a display statment */
EXTERN UCHAR graphicparseflg;		/* set to TRUE when currently parsing a *rptchar, *rptdown in a display statment */
EXTERN UCHAR elementref; 		/* flag set if array element reference in made within current ruleloop */
EXTERN UCHAR withnamesflg;		/* flag set to TRUE if the names of each variable are being stored with RECORD/LIST definition */
EXTERN UCHAR constantfoldflg;		/* flat set to TRUE if the constant folding optimization is turned on */
EXTERN INT recdefonly;			/* set to TRUE if record is being defined only */
EXTERN struct recordaccess **recindexptr;	/* hashed record names, ptrs to defs. */
EXTERN INT currechash;			/* Index into the record structure array of the currently in process record */
EXTERN INT tabflag; 			/* set to TRUE if tabbing control code was encountered */
EXTERN INT tmpvarflg;			/* set to 1 if temporary variables can be reused, 2 if not */
EXTERN INT recordlevel;			/* depth of record definition */
EXTERN UCHAR nextflag;			/* 0 = next, 1 = repeat, 2 = stmt end, 3 = uninitialized */
EXTERN UCHAR nextflagsave;		/* save value of next flag */
EXTERN INT definecnt;			/* offset in define area of first unused entry */
EXTERN INT childclsflag;			/* set to TRUE if currently compiling a child class definition block */
EXTERN UCHAR classlevel;			/* contains 0 for normal main-level code compiliation, 1 for code compiliation within a class definition */
EXTERN USHORT curclassid;		/* contains the current class ID value (for scope information) */

/* Undefined routine address variables are stored as	*/
/* follows:									*/
/*											*/
/* Pos. 0 (1 byte) - length n of address var. name	*/
/* Pos. 1 (n bytes) - undefined address var. name 	*/
/*											*/
EXTERN UCHAR **addrfixup;	/* storage of undefined addr vars */
EXTERN UINT addrfixupsize;	/* size (in bytes) of addrfixup in use */
EXTERN UINT addrfixupalloc;	/* size (in bytes) of addrfixup allocated */
EXTERN UCHAR rtncode;		/* ROUTINE or LROUTINE prg code stored */

/* define the types of temporary variables created by xtempvar() */
#define TEMPTYPE_DIM1 0
#define TEMPTYPE_DIM256 1
#define TEMPTYPE_INT 2
#define TEMPTYPE_ALTFORM31 3
#define TEMPTYPE_FORM31 4
#define TEMPTYPE_FLOAT 5
#define TEMPTYPE_OBJECT 6

/* whitespace macro */
#define whitespace() { while (charmap[line[linecnt]] == 2) linecnt++; }

/* dbcmp.c prototypes */
INT main(INT, CHAR **);
INT srcopen(CHAR *);
INT srcclose(void);
void saveline(UCHAR *, INT);
void restoreline(UCHAR *, INT *);
INT getln(void);
void getlndsp(void);
void vexpand(void);
INT makenlit(CHAR *);
INT vmakenlit(INT);
INT makeclit(UCHAR *);
UINT getlabelnum(void);
void deflabelnum(UINT);
void makegoto(UINT);
void mainpoffset(UINT);
void makepoffset(UINT);
void makecondgoto(UINT, INT);
void maincondgoto(UINT, INT);
INT getdsym(CHAR *);
INT getpsym(CHAR *);
void putdsym(CHAR *, INT);
INT usepsym(CHAR *, UINT);
void dbcmem(UCHAR ***, UINT);
void putarrayshape(INT, UINT *);
void getarrayshape(INT, UINT *);
void putcode1(UINT);
void putcodellhh(UINT);
void putcodelmh(INT);
void putcodehhll(UINT);
void putcodeadr(INT);
void putmain1(UINT);
void putmainhhll(UINT);
void putmainlmh(INT);
void putmainadr(INT);
void putcodeout(void);
void putdata1(UINT);
void putdatahhll(UINT);
void putdatallhh(UINT);
void putdata(UCHAR *, INT);
void error(INT);
void death(INT);
void warning(INT);
void dbglblrec(INT, CHAR *);
void dbglinerec(INT, INT);
void dbgrtnrec(CHAR *);
void dbgendrec(void);
INT startclassdef(CHAR *, CHAR *, CHAR *, CHAR *);
void endclassdef(void);
INT putclass(CHAR *, CHAR *, INT);
INT getclass(CHAR *, CHAR *);
INT makemethodref(UINT);
CHAR* getInteralError8Message(void);
void setInteralError8Message(CHAR*);

/* dbcmpa.c prototypes */
void compile(void);
UCHAR arraysymaddr(UINT);
void scantoken(UINT);
INT parseflag(INT);
INT parseevent(void);
INT parseifexp(UINT, INT);
void putdefine(CHAR *);
INT chkdefine(CHAR *);
INT checknum(UCHAR *);
INT checkxcon(UCHAR *);
INT checkdcon(UCHAR *);
INT xtempvar(INT);
void xtempvarreset(void);

/* dbcmpb.c function prototypes */
void checknesting(void);
void savecode(INT);
void restorecode(INT);
INT kwdupchk(UINT);
void putdataname(CHAR *);
void setifopdir(INT);
UINT s_charnum_fnc(void);
UINT s__conditional_fnc(void);
UINT s_define_fnc(void);
UINT s_equate_fnc(void);
UINT s_file_fnc(void);
UINT s__if_fnc(void);
UINT s_init_fnc(void);
UINT s_label_fnc(void);
UINT s_list_fnc(void);
UINT s__ifdef_fnc(void);
UINT s_pfile_fnc(void);
UINT s_record_fnc(void);
UINT s_routine_fnc(void);
UINT s_include_fnc(void);
UINT s_endif_fnc(void);
UINT s_liston_fnc(void);
UINT s_varlist_fnc(void);


/* dbcmpc.c function prototypes */
INT cc_other(UINT);
INT cc_special(UINT, UINT);
UINT s_uverb_types(CHAR *);
UCHAR baseclass(UINT);
INT getrecdef(CHAR *);
INT putrecdef(CHAR *, INT);
void makelist(INT);
void recdefhdr(CHAR *);
void recdeftail(UINT, UINT);
void putdef1(UINT);
void putdefhhll(UINT);
void putdefllhh(UINT);
void putdef(UCHAR *, UINT);
void putdeflmh(INT);
INT putrecdsym(CHAR *);
INT writeroutine(void);
UINT uverbfnc(void);
UINT s_uverb_fnc(void);

/* dbcmpd.c prototypes */
UINT s_object_fnc(void);
UINT s_class_fnc(void);
UINT s_endclass_fnc(void);
UINT s_endswitch_fnc(void);
UINT s_method_fnc(void);
UINT s_make_fnc(void);
void s_inherited_fnc(UINT);

/* dbcmperr.c prototypes */
CHAR *errormsg(INT);
