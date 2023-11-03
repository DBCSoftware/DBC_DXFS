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
#define INC_LIMITS
#define DBCMPA
#include "includes.h"
#include "release.h"
#include "base.h"
#include "dbcmp.h"

extern VERB * verbptr[];
extern CTLCODE * ctlcodeptr[];
 
static INT definemax;		/* length of define area */
static UCHAR **definepptr;	/* pointer to pointer that is define area */

/* the charmap table is an ASCII only translation table			*/
/* 0 means invalid character, cease compilation of line			*/
/* 1 means is end of input line (binary 0)						*/
/* 2 means is white space (blank or tab)						*/
/* 3 means is " character									*/
/* 4 (NOT USED) use to mean # or % (directive verb start character)	*/
/* 5 means alpha character, $ or _ (label and verb start character)	*/
/* 6 means is a digit										*/
/* 7 means is period										*/
/* 10 is TOKEN_ATSIGN (the @ character)							*/
/* 11 and up are other TOKEN_ delimiters						*/

UCHAR charmap[256] = { 1, 0, 0, 0, 0, 0, 0, 0,							/* 0 - 7 */
	0, 2, 0, 0, 0, 0, 0, 0,											/* 8 - 15 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,						/* 16 - 31 */
	2, TOKEN_BANG, 3, TOKEN_POUNDSIGN, 5, TOKEN_PERCENT, TOKEN_AMPERSAND, 0,	/* 32 - 39 */
	TOKEN_LPAREND, TOKEN_RPAREND, TOKEN_ASTERISK, TOKEN_PLUS,				/* 40 - 43 */
	TOKEN_COMMA, TOKEN_MINUS, 7, TOKEN_DIVIDE,							/* 44 - 47 */
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, TOKEN_COLON, TOKEN_SEMICOLON,				/* 48 - 59 */
	TOKEN_LESS, TOKEN_EQUAL, TOKEN_GREATER, 0,							/* 60 - 63 */
	TOKEN_ATSIGN, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,				/* 64 - 79 */
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, TOKEN_LBRACKET,						/* 80 - 91 */
	0, TOKEN_RBRACKET, 0, 5,											/* 92 - 95 */
	0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,						/* 96 - 111 */
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, TOKEN_ORBAR, 0, TOKEN_TILDE };	/* 112 - 126 */

/*
 * A DB/C label or verb can start with a $, upper-case letter, lower-case letter.
 * The body of a label can be a $, upper-case letter, underscore (95), lower-case letter,
 * 		digit, dot (46), @ (64)
 *
 */


/* values needed for getdefine() text subsitution */
#define MAX_EXPANSIONS 200
extern UINT linecounter[];
extern INT inclevel;

static INT savtokentype;

/* expression abstract syntax tree node structure definition */
#define ASTNODEMAX 150
typedef struct astnodestruct {
	USHORT type;		/* contains a NODETYPE_* value (defined below) */
	UCHAR upptr;		/* points to parent above */
	UCHAR ptr1;		/* points to first child, 0xFF for no children */
	UCHAR ptr2;		/* points to second child, 0xFF for no more children */
	UCHAR ptr3;		/* points to third child, 0xFF for no more children */
	UCHAR curptr;		/* points to current child branch in depth first search */
	UCHAR flag;		/* (XPARSE) set to TRUE if this value is to be      */
					/* logically negated -or- (XGENGOTOIFCODE) set to   */ 
					/* TRUE if the rightmost branch goto if code should */
					/* be reversed logically (including exchange of 	  */
					/* success, failure labels)					  */
	UCHAR grpnum;		/* (AND/OR op only) number of this AND/OR in an AND/OR group */
	CHAR numlit[10];	/* numeric literal for NODETYPE_NUMCONST */
	INT value;		/* contains value of operator, address of operand, or literal */
	UINT success;	/* contains label number of success label */
	UINT failure;	/* contains label number of failure label */
} ASTNODE;

/* static storage for expression abstract syntax tree */
static ASTNODE astnode[ASTNODEMAX];	
static UINT nodecnt, topnode;

/* static storage for reusable temporary variable code generation */
#define MAXTEMP 128			/* maximum temporaries (per expression) */
static UCHAR links[MAXTEMP], current[7], first[7], highuse;
static INT tempadr[MAXTEMP];

/* coding of type field of node structure */
/* 1 numeric constant range is 0 to 255, node is terminal, value contains constant value */
/* 2 numeric variable/literal - value contains address, ptr1-3 point to index value nodes */
/* 3 character variable/literal - value contains address, ptr1-3 point to index value nodes */
/* 4 flag reference, value is 0-9 = EQUAL, LESS, OVER, EOS, NOT EQUAL ... NOT EOS, GREATER, NOT GREATER */
/* 8 and above are operators, type is precedence, value is operator, ptr1-3 point to operands */
#define NODETYPE_NUMCONST 1
#define NODETYPE_NVARLIT 2
#define NODETYPE_CVARLIT 3
#define NODETYPE_FLAG 4
#define NODETYPE_OPERATOR 8

/* operators stored in value field of node structure */
/* unary operators associate right to left, binary operators left to right */
#define OP_BINARY_ 0x01
#define OP_NOPS_ 0x02
#define OP_COPS_ 0x04
#define OP_SAMEOPS_ 0x08
#define OP_COPNOP_ 0x10
#define OP_NUM_ 0x20
#define OP_CHAR_ 0x40

/* operator precedence is number in comment field */
#define OP_NEGATE ((OP_NOPS_ + OP_NUM_) * 256 + 64)					/* 8 */
#define OP_SIN ((OP_NOPS_ + OP_NUM_) * 256 + 41)						/* 8 */
#define OP_COS ((OP_NOPS_ + OP_NUM_) * 256 + 42)						/* 8 */
#define OP_TAN ((OP_NOPS_ + OP_NUM_) * 256 + 43)						/* 8 */
#define OP_ARCSIN ((OP_NOPS_ + OP_NUM_) * 256 + 44)					/* 8 */
#define OP_ARCCOS ((OP_NOPS_ + OP_NUM_) * 256 + 45)					/* 8 */
#define OP_ARCTAN ((OP_NOPS_ + OP_NUM_) * 256 + 46)					/* 8 */
#define OP_EXP ((OP_NOPS_ + OP_NUM_) * 256 + 47)						/* 8 */
#define OP_LOG ((OP_NOPS_ + OP_NUM_) * 256 + 48)						/* 8 */
#define OP_LOG10 ((OP_NOPS_ + OP_NUM_) * 256 + 49)					/* 8 */
#define OP_SQRT ((OP_NOPS_ + OP_NUM_) * 256 + 50)						/* 8 */
#define OP_ABS ((OP_NOPS_ + OP_NUM_) * 256 + 51)						/* 8 */
#define OP_FORMPTR ((OP_COPS_ + OP_NUM_) * 256 + 2)					/* 8 */
#define OP_LENGTHPTR ((OP_COPS_ + OP_NUM_) * 256 + 3)					/* 8 */
#define OP_SIZE ((OP_COPS_ + OP_NOPS_ + OP_NUM_) * 256 + 39)			/* 8 */
#define OP_LENGTH ((OP_COPS_ + OP_NOPS_ + OP_NUM_) * 256 + 40)			/* 8 */
#define OP_INT ((OP_COPS_ + OP_NOPS_ + OP_NUM_) * 256 + 14)				/* 8 */
#define OP_FORM ((OP_COPS_ + OP_NOPS_ + OP_NUM_) * 256 + 15)			/* 8 */
#define OP_FLOAT ((OP_COPS_ + OP_NOPS_ + OP_NUM_) * 256 + 16)			/* 8 */
#define OP_CHAR ((OP_NOPS_ + OP_CHAR_) * 256 + 20)					/* 8 */
#define OP_FCHAR ((OP_COPS_ + OP_CHAR_) * 256 + 162)					/* 8 */
#define OP_LCHAR ((OP_COPS_ + OP_CHAR_) * 256 + 163)					/* 8 */
#define OP_SQUEEZE ((OP_COPS_ + OP_CHAR_) * 256 + 59)					/* 8 */
#define OP_CHOP ((OP_COPS_ + OP_CHAR_) * 256 + 60)					/* 8 */
#define OP_TRIM ((OP_COPS_ + OP_CHAR_) * 256 + 241)					/* 8 */
#define OP_ISNULL ((OP_COPS_ + OP_NOPS_ + OP_NUM_) * 256 + 234)			/* 8 */
#define OP_NRESIZE ((OP_BINARY_ + OP_NOPS_ + OP_NUM_) * 256 + 61)		/* 7 */
#define OP_CRESIZE ((OP_BINARY_ + OP_COPNOP_ + OP_CHAR_) * 256 + 61)		/* 7 */
#define OP_POWER ((OP_BINARY_ + OP_NOPS_ + OP_NUM_) * 256 + 52)			/* 6 */
#define OP_MULTIPLY ((OP_BINARY_ + OP_NOPS_ + OP_NUM_) * 256 + 55)		/* 5 */
#define OP_DIVIDE ((OP_BINARY_ + OP_NOPS_ + OP_NUM_) * 256 + 56)			/* 5 */
#define OP_MODULUS ((OP_BINARY_ + OP_NOPS_ + OP_NUM_) * 256 + 57)		/* 5 */
#define OP_CONCAT ((OP_BINARY_ + OP_COPS_ + OP_CHAR_) * 256 + 62)		/* 4 */
#define OP_ADD ((OP_BINARY_ + OP_NOPS_ + OP_NUM_) * 256 + 53)			/* 4 */
#define OP_SUBTRACT ((OP_BINARY_ + OP_NOPS_ + OP_NUM_) * 256 + 54)		/* 4 */
#define OP_EQUAL ((OP_BINARY_ + OP_SAMEOPS_ + OP_NUM_) * 256 + 86)		/* 3 */
#define OP_NOTEQUAL ((OP_BINARY_ + OP_SAMEOPS_ + OP_NUM_) * 256 + 87)		/* 3 */
#define OP_LESS ((OP_BINARY_ + OP_SAMEOPS_ + OP_NUM_) * 256 + 88)		/* 3 */
#define OP_GREATER ((OP_BINARY_ + OP_SAMEOPS_ + OP_NUM_) * 256 + 89)		/* 3 */
#define OP_LESSEQ ((OP_BINARY_ + OP_SAMEOPS_ + OP_NUM_) * 256 + 90)		/* 3 */
#define OP_GREATEREQ ((OP_BINARY_ + OP_SAMEOPS_ + OP_NUM_) * 256 + 91)	/* 3 */
#define OP_NOT ((OP_COPS_ + OP_NOPS_ + OP_NUM_) * 256 + 63)				/* 2 */
#define OP_AND ((OP_BINARY_ + OP_NOPS_ + OP_COPS_ + OP_NUM_) * 256 + 82)	/* 1 */
#define OP_OR ((OP_BINARY_ + OP_NOPS_ + OP_COPS_ + OP_NUM_) * 256 + 83)	/* 0 */
#define HIGHEST_PRECEDENCE 8

#define XPREFIX 192		/* instruction that prefixes an expression instruction */

static INT getdefine(CHAR *, INT, INT);
static INT xparse(UINT);
static INT xoptast(UINT *);
static INT xgenexpcode(UINT);
static INT xgencode(UINT);
static INT xgengotoifcode(UINT, UINT, UINT);
static void xgenmainadr(UINT);
static void xgencodeadr(UINT);


void compile()  /* main translation loop */
{
	UCHAR m, n, r, upchar;
	UCHAR rcount, ccrulecnt, noparseflg;
	INT i, litsize, savlinepos, xtype;
	VERB * vbp;
	SYNTAX * sptr;
	CTLCODE * ccp;
	UINT rule, ccrule;
	static SYNTAX uverbsyntax = { (UINT) 0, uverbfnc };

	dataadr = codeadr = 0;
	curerrcnt = ccrulecnt = codebufcnt = 0;
	classlevel = 0;
	curclassid = 0;
	line[0] = '\0';
	rlevelid[0] = 0;
	rlevel = 0;
	rlevelidcnt = 0;
	childclsflag = FALSE;
	if (!(dbcmpflags & DBCMPFLAG_VERSION8)) {
		if (!(dbcmpflags & DBCMPFLAG_VERSION9)) {
			symvalue = 1;
			symtype = TYPE_EQUATE;
			putdsym("DBC_DX", FALSE);
			symvalue = DX_MAJOR_VERSION;
		}
		else symvalue = 9;
		symtype = TYPE_EQUATE;
		putdsym("DBC_RELEASE", FALSE);
	}

stmtloop:
	if (line[0]) {
		if ((dspflags & DSPFLAG_COMPILE) && !curerrcnt) getlndsp();
		if (dbgflg && savcodeadr != codeadr) dbglinerec((INT) savlinecnt, savcodeadr);
	}
	if (getln()) return;
	savlinecnt = linecounter[inclevel];
	scrlparseflg = FALSE; /* set to TRUE when parsing scroll list in display */
	graphicparseflg = FALSE; /* set to TRUE when parsing a *rptchar, *rptdown in display */
	ccrulecnt = curerrcnt = tokentype = 0;
	m = n = 1;
	tmpvarflg = 1;   /* reuse temporary variables */
	constantfoldflg = TRUE;	/* turn constant folding on */
	linecnt = 1;
	nextflagsave = 3;
	if (charmap[line[0]] > 2) {  /* label exists */
		if (charmap[line[0]] != 5 || line[0] == '_') {
			if (nocompile) goto stmtloop;
			for (linecnt = 0; line[linecnt] && charmap[line[linecnt]] != 2; linecnt++) token[linecnt] = line[linecnt];
			token[linecnt] = '\0';
			error(ERROR_BADLABEL);
			goto stmtloop;
		}
		if (upcaseflg) {
			label[0] = toupper(line[0]);
			while ((charmap[line[linecnt]] == 5 ||
				  charmap[line[linecnt]] == 6 ||
				  charmap[line[linecnt]] == 7 ||
				  charmap[line[linecnt]] == TOKEN_ATSIGN) &&
				  linecnt < LABELSIZ-1)	{
					label[linecnt] = (UCHAR) toupper(line[linecnt]);
					linecnt++;
			}
		}
		else {
			label[0] = line[0];
			while ((charmap[line[linecnt]] == 5 ||
				  charmap[line[linecnt]] == 6 ||
				  charmap[line[linecnt]] == 7 ||
				  charmap[line[linecnt]] == TOKEN_ATSIGN) &&
				  linecnt < LABELSIZ-1) {
					label[linecnt] = line[linecnt];
					linecnt++;
			}
		}
		label[linecnt] = '\0';
		if (charmap[line[linecnt]] != 2 && line[linecnt]) {
			if (nocompile) goto stmtloop;
			for (linecnt = 0; line[linecnt] && charmap[line[linecnt]] != 2; linecnt++) token[linecnt] = line[linecnt];
			token[linecnt] = 0;
			error(ERROR_BADLABEL);
			goto stmtloop;
		}
		while (charmap[line[linecnt]] == 2) linecnt++;
		if (!line[linecnt]) {
			if (!nocompile) {
				if (dbgflg) dbglblrec((INT) linecounter[inclevel], label);
				usepsym(label, LABEL_DEF);
			}
			goto stmtloop;
		}
	}
	else {
		label[0] = '\0';
		whitespace(); /* advance linecnt over any spaces and tabs */
	}
	if (!line[linecnt]) goto stmtloop;
	if (charmap[line[linecnt]] == 5) {
		/* type 5 is all of the characters legal to start a verb with */
		verb[0] = (UCHAR) toupper(line[linecnt]);
	}
	else if (charmap[line[linecnt]] == TOKEN_POUNDSIGN ||
		    charmap[line[linecnt]] == TOKEN_PERCENT) {
		verb[0] = '%';
	}
	else if (nocompile) goto stmtloop;
	else goto badsyntax;
	linecnt++;
	/* types 5, 6, 7 are all of the characters legal to continue a verb with */
	while (charmap[line[linecnt]] > 4 && charmap[line[linecnt]] < 8 && m < (UCHAR)(VERBSIZE-1)) {
		verb[m++] = (UCHAR) toupper(line[linecnt]);
		linecnt++;
	}
	verb[m] = '\0';
	
	if (line[linecnt]) {
		if (charmap[line[linecnt++]] != 2) {
			if (nocompile) goto stmtloop;
			else {
				error(ERROR_BADVERBLEN);
				goto stmtloop;
			}
		}
		whitespace(); /* advance linecnt over any spaces and tabs */
	}
	/* Hash verb name into 127 buckets. 127 is prime */
	vbp = verbptr[((verb[m - 1] * 47) + verb[0] + verb[1] + m + m) & 0x7F];

	while (TRUE) {
		if (vbp == NULL) {
			if (nocompile) goto stmtloop;
			else if (label[0] != '\0') {
				if (dbgflg) dbglblrec((INT) linecounter[inclevel], label);
				usepsym(label, LABEL_DEF);
			}
			if (undfnexist) writeroutine();
			rulecnt = 0;
			sptr = &uverbsyntax;
			delimiter = 0;
			if ( (rule = uverbfnc()) ) {
				if (rule == NEXT_TERMINATE) goto endstmt;
				goto ruleloop;
			}
			error(ERROR_BADVERB);
			goto stmtloop;
		}
		if (!strcmp(vbp->verbname, verb)) break;
		vbp = vbp->nextptr;
	}

	if (nocompile) if ((vbp->verbcode < 164 || vbp->verbcode > 168) &&
				    (vbp->verbcode < 175 || vbp->verbcode > 180)) goto stmtloop;
	if (vbp->verbcode != 198 && vbp->verbcode != 199) {
		if (undfnexist) {
			if ((vbp->verbcode < 201 || vbp->verbcode > 218) && vbp->verbcode != 229 && vbp->verbcode != 233)
				writeroutine();
		}
		if (label[0] && vbp->verbcode && (vbp->verbcode < 193 || vbp->verbcode > 255)) {
			if (dbgflg) dbglblrec((INT) linecounter[inclevel], label);
			if (vbp->verbcode == 110 && verb[0] == 'R')
				usepsym(label, LABEL_ROUTINE);	 /* ROUTINE label */
			else
				if (!nocompile) usepsym(label, LABEL_DEF);	 /* LROUTINE or other label */
		}
		codebufcnt = 0;
		if (vbp->verbcode > 255) putcodehhll(vbp->verbcode);
		else {
			/**
			 * Change added 08/22/2012 (16.1.2+) for PPro
			 * If the -z flag is absent, a debug verb is ignored.
			 *
			 * Might need to put a config switch on this
			 */
			if (vbp->verbcode != 17 || dbgflg /* -z is on */) putcode1(vbp->verbcode);
		}
	}
	sptr = vbp->syntax;
	rule = sptr->rule;
	rulecnt = 0;

ruleloop:
	rulecnt++;
	elementref = FALSE;
	noparseflg = FALSE;
	symbasetype = 0;
	r = (UCHAR) rule;
	if (r <= RULE_PARSEVAR) {
		if (r == RULE_PARSEUP) scantoken(SCAN_UPCASE);
		else if (r == RULE_PARSEVAR) scantoken(upcaseflg);
		else if (r == RULE_PARSE) scantoken(SCAN_NOUPCASE);
		else if (r == RULE_NOPARSE) noparseflg = TRUE;
		if (savtokentype == TOKEN_COMMA && tokentype == 0) goto badsyntax;
		if (tokentype == TOKEN_DBLQUOTE) scantoken(SCAN_NOUPCASE + SCAN_SCANLITERAL);
		goto parsedelimiter;   
	}
	scantoken(upcaseflg);
	if (tokentype == TOKEN_DBLQUOTE) {
		graphicparseflg = FALSE;
		if (r == RULE_MOVESRC || r == RULE_MATHSRC ||
		    r == RULE_NSRC || r == RULE_ANYSRC || r == RULE_CNSRCCONST ||
		    r == RULE_NSRCEQU || r == RULE_NSRCSCON || r == RULE_LOADSRC) {
			scantoken(SCAN_NOUPCASE + SCAN_SCANLITERAL);
			if (r != RULE_ANYSRC && r != RULE_CNSRCCONST && (i = checknum(token)) >= 0 && (r != RULE_MOVESRC || token[strlen((CHAR *) token) - 1] != '.')) {
				symtype = TYPE_NUMBER;
				if (!i && r == RULE_MATHSRC) {
					symvalue = atoi((CHAR *) token);
					if (symvalue >= 0 && symvalue < 256) {
						putcodehhll((USHORT) 0xFF00 + (UINT) symvalue);
						goto parsedelimiter;
					}
					if (symvalue > -256 && symvalue < 0) {
						putcodehhll((USHORT) 0xFE00 - (UINT) symvalue);
						goto parsedelimiter;
					}
				}
				symvalue = makenlit((CHAR *) token);
				goto writesymadr0;
			}
			if (r == RULE_MOVESRC || r == RULE_CNSRCCONST || r == RULE_ANYSRC ||
			    r == RULE_LOADSRC) {
				symvalue = makeclit(token);
				symtype = TYPE_LITERAL;
				goto writesymadr0;
			}
			litsize = (INT)strlen((CHAR *) token);
			if (litsize >= MAXNUMLITSIZE) error(ERROR_LITTOOLONG);
			else error(ERROR_BADNUMLIT);
			goto stmtloop;
		}
		symtype = TYPE_LITERAL;
		if (r == RULE_CNDATASRC || r == RULE_DISPLAYSRC || r == RULE_PRINTSRC ||
		    r == RULE_CNSRC || r == RULE_CNSRCXCON || r == RULE_CSRC || r == RULE_CSRCCONST || r == RULE_ANYSRC) {
			scantoken(SCAN_NOUPCASE + SCAN_SCANLITERAL);
			if (r == RULE_CSRCCONST && strlen((CHAR *) token) == 1) {
				putcode1(token[0]);	/* 1 byte constant */
				goto parsedelimiter;
			}
			else {
				symvalue = makeclit(token);
				goto writesymadr0;
			}
		}
		if ((rule & 0xFF00) == RULE_LITERAL << 8 || r == RULE_LITERAL) {
			scantoken(SCAN_NOUPCASE + SCAN_SCANLITERAL);
			goto parsedelimiter;
		}
		if ((rule & 0xFF00) == RULE_SLITERAL << 8) {
			i = 1;
			if (line[linecnt + 1] == '#' && line[linecnt + 2] && line[linecnt + 3] == '"') i = 2;
			if (line[linecnt + i] && line[linecnt + i + 1] == '"') {
				token[0] = line[linecnt + i];
				token[1] = 0;
				linecnt += i + 2;
				symvalue = token[0];
				goto parsedelimiter;
			}
		}
		if (r == RULE_TRAPEVENT && line[linecnt+1] && line[linecnt+2] == '"') {
			putcode1(line[linecnt+1]);
			linecnt += 3;
			goto parsedelimiter;
		}
		goto badsyntax;
	}
	if (tokentype == TOKEN_LPAREND) {
		graphicparseflg = FALSE;
		if (!(xtype = xparse(')'))) goto stmtloop;
		if (!xgenexpcode(topnode)) goto stmtloop;
		xgencodeadr(topnode);
		symtype = TYPE_EXPRESSION;
		switch (r) {
			case RULE_MOVESRC:
			case RULE_LOADSRC:
			case RULE_MATHSRC:
			case RULE_CNDATASRC:
			case RULE_CNSRC:
			case RULE_CNSRCXCON:
			case RULE_DISPLAYSRC:
			case RULE_PRINTSRC:
			case RULE_CNSRCCONST:
			case RULE_ANYSRC:
				if (xtype == 2) tokentype = TOKEN_LITERAL;
				else tokentype = TOKEN_NUMBER;
				goto parsedelimiter;
			case RULE_NSRC:
			case RULE_NSRCDCON:
			case RULE_NSRCSCON:
			case RULE_NSRCCONST:
			case RULE_NSRCEQU:
				if (xtype == 2) {
					error(ERROR_BADEXPTYPE);
					goto stmtloop;
				}
				tokentype = TOKEN_NUMBER;
				goto parsedelimiter;
			case RULE_CSRC:
			case RULE_CSRCCONST:
				if (xtype == 1) {
					error(ERROR_BADEXPTYPE);
					goto stmtloop;
				}
				tokentype = TOKEN_LITERAL;
				goto parsedelimiter;
		}
		goto badsyntax;
	}
	if (tokentype == TOKEN_ASTERISK) {
		if ((UCHAR) (rule >> 16) == RULE_ASTERISK) goto endnext3;
		r = (UCHAR) (rule >> 8);  /* all CC rules are second */
		if (r < RULE_KYCC || r > RULE_CHGCC) goto badsyntax;
		scantoken((UINT) (upcaseflg + SCAN_NOADVANCE + SCAN_NOASTRKDEFINE));
		upchar = (UCHAR) toupper(token[0]);
		if (upchar == 'P' || upchar == 'T' || upchar == 'W') {
			if (scrlparseflg) {
				error(ERROR_BADSCROLLCODE);
				goto stmtloop;
			}
			if (graphicparseflg) {
				error(ERROR_BADRPTCHAR);
				goto stmtloop;
			}
			savlinepos = linecnt;
			scantoken(SCAN_UPCASE + SCAN_NOEXPDEFINE);
			switch (upchar) {
				case 'P':
					if (strcmp((CHAR *) token, "PL")) {
						if (strcmp((CHAR *) token, "POFF")) {
							if (strcmp((CHAR *) token, "PON")) {
								linecnt = savlinepos;
								ccrule = cc_other((UINT) r);
								if (ccrule == (UINT) -1) goto stmtloop;  /* error condition */
								else if (ccrule) goto endnext1;   /* get parameters */
								goto parsedelimiter;
							}
						}
					}
					break;
				case 'T':
					if (strcmp((CHAR *) token, "TAB") || (line[linecnt] != '=' && !isspace(line[linecnt]))) {
						if (strcmp((CHAR *) token, "TOFF")
							&& strcmp((CHAR *) token, "TEXTANGLE")
							&& strcmp((CHAR *) token, "TRIANGLE"))
						{
							linecnt = savlinepos;
							ccrule = cc_other((UINT) r);
							if (ccrule == (UINT) -1) goto stmtloop;	/* error condition */
							else if (ccrule) goto endnext1;	/* get parameters */
							goto parsedelimiter;
						}
					}
					break;
				case 'W':
					if (strcmp((CHAR *) token, "WHITE")) {
						linecnt = savlinepos;
						ccrule = cc_other((UINT) r);
						if (ccrule == (UINT) -1) goto stmtloop;  /* error condition */
						else if (ccrule) goto endnext1;	/* get parameters */
						goto parsedelimiter;
					}
					break;
			}
			linecnt = savlinepos;
		}
		else if (isdigit(line[linecnt])) {
			if (cc_other((UINT) r) == -1) goto stmtloop;  /* error condition */
			goto parsedelimiter;
		}
		savlinepos = linecnt;
		scantoken(SCAN_UPCASE + SCAN_NOEXPDEFINE);
		if (tokentype == TOKEN_PLUS) {
			tokentype = TOKEN_WORD;
			if (codebuf[0] == 32) strcpy((CHAR *) token, "KCON"); /* keyin */
			else strcpy((CHAR *) token, "SL"); /* display, print */
		}
		if (tokentype == TOKEN_MINUS) {
			tokentype = TOKEN_WORD;
			if (codebuf[0] == 32) strcpy((CHAR *) token, "KCOFF"); /* keyin */
			else strcpy((CHAR *) token, "PL"); /* display, print */
		}
		if (tokentype != TOKEN_WORD) goto badsyntax;
		ccp = ctlcodeptr[(token[1] * 2 + token[0] + strlen((CHAR *) token)) & 0x1F];
		while (TRUE) {
			if (ccp == NULL) {
				linecnt = savlinepos;
				ccrule = cc_other((UINT) r);
				if (ccrule == (UINT) -1) goto stmtloop;  /* error condition */
				else if (ccrule) goto endnext1;   /* get parameters */
				goto parsedelimiter;
			}
			if (!strcmp(ccp->ccname, (CHAR *) token)) break;
			ccp = ccp->nextptr;
		}
		symtype = TYPE_KEYWORD;
		switch (r) {
			case RULE_KYCC:
				if (!(ccp->cctype & CC_KEYIN)) goto badcc;
				break;
			case RULE_DSCC:
				if (!(ccp->cctype & CC_DISPLAY)) goto badcc;
				break;
			case RULE_PRCC:
				if (!(ccp->cctype & CC_PRINT)) {
					linecnt = savlinepos;
					if (cc_other((UINT) r) == -1) goto stmtloop;  /* error condition */
					else goto parsedelimiter;
				}
				break;
			case RULE_RDCC:
				if (!(ccp->cctype & CC_READ)) {
					linecnt = savlinepos;
					if (cc_other((UINT) r) == -1) goto stmtloop;  /* error condition */
					else goto parsedelimiter;
				}
				break;
			case RULE_WTCC:
				if (!(ccp->cctype & CC_WRITE)) {
					linecnt = savlinepos;
					if (cc_other((UINT) r) == -1) goto stmtloop;  /* error condition */
					else goto parsedelimiter;
				}
				break;
			case RULE_CHGCC:
				if (!(ccp->cctype & CC_CHANGE)) goto badcc;
				break;
		}
		if (scrlparseflg && !(ccp->cctype & CC_SCROLL)) {
			error(ERROR_BADSCROLLCODE);
			goto stmtloop;
		}
		if (graphicparseflg) {
			if (!(ccp->cctype & CC_GRAPHIC)) {
				error(ERROR_BADRPTCHAR);
				goto stmtloop;
			}
			else graphicparseflg = FALSE;
		}
		if (ccp->cccode > 255) putcodehhll(ccp->cccode);
		else putcode1((UCHAR) (ccp->cccode));
		if (ccp->cctype & CC_SPECIAL) {
			ccrule = cc_special(ccp->cccode, r);
			ccrulecnt = 0;
			if (ccrule == (UINT) -1) goto stmtloop;  /* error condition */
			else if (ccrule) goto endnext1;   /* get parameters */
			if (scrlparseflg) {
				ccrule = CCPARSE_SCRLR;
				rule = RULE_DISPLAYSRC + (RULE_DSCC << 8) + NEXT_COLONOPT;
			}
		}
		goto parsedelimiter;
	}
	rcount = 1;
checktype:
	if (tokentype == TOKEN_WORD) {
		graphicparseflg = FALSE;
		if (rcount == 1) {
			if (r == RULE_LBLGOTO) {
				usepsym((CHAR *) token, LABEL_REF);
				goto parsedelimiter;
			}
			if (r == RULE_LBLSRC) {
				usepsym((CHAR *) token, LABEL_REFFORCE);
				putcodehhll((USHORT) symvalue);
				goto parsedelimiter;
			}
			if (r == RULE_LABEL) {
				usepsym((CHAR *) token, LABEL_REFFORCE);
				putcodehhll(0xC180);
				putcodehhll((USHORT) symvalue);
				goto parsedelimiter;
			}
			if (r == RULE_LBLDEST) {
				usepsym((CHAR *) token, LABEL_REFFORCE);
				if (symtype != TYPE_PLABELVAR) {
					error(ERROR_LABELNOTFOUND);
					goto stmtloop;
				}
				putcodehhll((USHORT) symvalue);
				goto parsedelimiter;
			}
			if (r == RULE_LBLVAREXT) {
				usepsym((CHAR *) token, LABEL_REFFORCE);
				if (symtype != TYPE_PLABELVAR && symtype != TYPE_PLABELXREF) {
					error(ERROR_LABELNOTFOUND);
					goto stmtloop;
				}
				putcodehhll((USHORT) symvalue);
				goto parsedelimiter;
			}
			if (r != RULE_TRAPEVENT) {
				if (getdsym((CHAR *) token) == -1) {
					error(ERROR_VARNOTFOUND);
					/* The next IF is a bit of a kludge.
					 * When getdsym fails, it leaves symtype with core garbage.
					 * We need to do this for a rather specific case, a three operand FOR with an
					 * undefined variable as the third argument. If we do not do this then when we
					 * hit the REPEAT verb we will crash with 'internal error 3'.
					 * Setting symtype to something expected will cause the label table to be properly updated.
					 */
					if (r == RULE_NSRC) symtype = TYPE_NVAR;
				}
				symbasetype = (UCHAR) (symtype & ~TYPE_PTR);
				if (!symbasetype) goto stmtloop;
			}
		}

checktype1:
		switch (r) {
			case RULE_EQUATE:
				if (symtype == TYPE_EQUATE) {
					symtype = TYPE_INTEGER;
					symvalue = vmakenlit(symvalue);
					goto writesymadr0;
				}
				break;
			case RULE_NSRCEQU:
				if (symtype == TYPE_EQUATE) {
					symtype = TYPE_INTEGER;
					symvalue = vmakenlit(symvalue);
					goto writesymadr0;
				}
				if (symbasetype == TYPE_NVAR) goto writesymadr0;
				if (symbasetype == TYPE_NARRAY1 || symtype == TYPE_NVARPTRARRAY1) goto writesymadr1;
				if (symbasetype == TYPE_NARRAY2 || symtype == TYPE_NVARPTRARRAY2) goto writesymadr2;
				if (symbasetype == TYPE_NARRAY3 || symtype == TYPE_NVARPTRARRAY3) goto writesymadr3;
				break;
			case RULE_DCON:
			case RULE_XCON:
				if (symtype == TYPE_EQUATE) {
					symtype = TYPE_INTEGER;
					goto parsedelimiter;
				}
				break;
			case RULE_CNSRCXCON:
				if (symtype == TYPE_EQUATE) {
					symtype = TYPE_INTEGER;
					goto parsedelimiter;
				}
				if (symbasetype == TYPE_CVAR) goto writesymadr0;
				if (symbasetype == TYPE_CARRAY1 || symtype == TYPE_CVARPTRARRAY1) goto writesymadr1;
				if (symbasetype == TYPE_CARRAY2 || symtype == TYPE_CVARPTRARRAY2) goto writesymadr2;
				if (symbasetype == TYPE_CARRAY3 || symtype == TYPE_CVARPTRARRAY3) goto writesymadr3;
				if (symbasetype == TYPE_NVAR) goto writesymadr0;
				if (symbasetype == TYPE_NARRAY1 || symtype == TYPE_NVARPTRARRAY1) goto writesymadr1;
				if (symbasetype == TYPE_NARRAY2 || symtype == TYPE_NVARPTRARRAY2) goto writesymadr2;
				if (symbasetype == TYPE_NARRAY3 || symtype == TYPE_NVARPTRARRAY3) goto writesymadr3;
				break;
			case RULE_PRINTSRC:
				if (symbasetype == TYPE_IMAGE) goto writesymadr0; // @suppress("No break at end of case")
				// fall through
			case RULE_DISPLAYSRC:
				if (symtype == TYPE_EQUATE) {
					symtype = TYPE_INTEGER;
					putcode1(251);
					putcode1((UCHAR) symvalue);
					goto parsedelimiter;
				}
				if (symbasetype == TYPE_CVAR || symbasetype == TYPE_NVAR) goto writesymadr0;
				if (line[linecnt] == '[' || line[linecnt] == '(') {
					if (symbasetype == TYPE_CARRAY1 || symbasetype == TYPE_NARRAY1 || symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1) goto writesymadr1;
					else if (symbasetype == TYPE_CARRAY2 || symbasetype == TYPE_NARRAY2 || symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2) goto writesymadr2;
					else if (symbasetype == TYPE_CARRAY3 || symbasetype == TYPE_NARRAY3 || symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3) goto writesymadr3;
				}
				if (symbasetype == TYPE_LIST || symbasetype == TYPE_CVARLIST ||
					symbasetype == TYPE_CARRAY1 || symbasetype == TYPE_NARRAY1 ||
					symbasetype == TYPE_CARRAY2 || symbasetype == TYPE_NARRAY2 ||
					symbasetype == TYPE_CARRAY3 || symbasetype == TYPE_NARRAY3 ||
					symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1 ||
					symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2 ||
					symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3) goto writesymadr0;
				break;
			case RULE_SCON:
				if (symtype == TYPE_EQUATE) {
					symtype = TYPE_INTEGER;
					goto parsedelimiter;
				}
				break;
			case RULE_CDATADEST:
				if (symbasetype == TYPE_CVAR) goto writesymadr0;
				if (line[linecnt] == '[' || line[linecnt] == '(') {
					if (symbasetype == TYPE_CARRAY1 || symtype == TYPE_CVARPTRARRAY1) goto writesymadr1;
					else if (symbasetype == TYPE_CARRAY2 || symtype == TYPE_CVARPTRARRAY2) goto writesymadr2;
					else if (symbasetype == TYPE_CARRAY3 || symtype == TYPE_CVARPTRARRAY3) goto writesymadr3;
				}
				if (symbasetype == TYPE_LIST || symbasetype == TYPE_CVARLIST ||
					symbasetype == TYPE_CARRAY1 || symbasetype == TYPE_CARRAY2 ||
					symbasetype == TYPE_CARRAY3 || symtype == TYPE_CVARPTRARRAY1 ||
					symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_CVARPTRARRAY3) goto writesymadr0;
				break;
			case RULE_CSRCCONST:
			case RULE_CSRC:
			case RULE_CVAR:
				if (symbasetype == TYPE_CVAR) goto writesymadr0;
				if (symbasetype == TYPE_CARRAY1 || symtype == TYPE_CVARPTRARRAY1) goto writesymadr1;
				else if (symbasetype == TYPE_CARRAY2 || symtype == TYPE_CVARPTRARRAY2) goto writesymadr2;
				else if (symbasetype == TYPE_CARRAY3 || symtype == TYPE_CVARPTRARRAY3) goto writesymadr3;
				break;
			case RULE_CVARPTR:
				if (symtype == TYPE_CVARPTR) goto writesymadr0;
				if (symtype == TYPE_CVARPTRARRAY1) goto writesymadr1;
				if (symtype == TYPE_CVARPTRARRAY2) goto writesymadr2;
				if (symtype == TYPE_CVARPTRARRAY3) goto writesymadr3;
				break;
			case RULE_CARRAY:
				if (symbasetype == TYPE_CARRAY1 || symbasetype == TYPE_CARRAY2 || symbasetype == TYPE_CARRAY3) goto writesymadr0;
				break;
			case RULE_CARRAY1:
				if (symbasetype == TYPE_CARRAY1) goto writesymadr0;
				break;
			case RULE_CARRAY2:
				if (symbasetype == TYPE_CARRAY2) goto writesymadr0;
				break;
			case RULE_CARRAY3:
				if (symbasetype == TYPE_CARRAY3) goto writesymadr0;
				break;
			case RULE_CNDATASRC:
			case RULE_CNDATADEST:
				if (symbasetype == TYPE_CVAR || symbasetype == TYPE_NVAR) goto writesymadr0;
				if (line[linecnt] == '[' || line[linecnt] == '(') {
					if (symbasetype == TYPE_CARRAY1 || symbasetype == TYPE_NARRAY1 || symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1) goto writesymadr1;
					else if (symbasetype == TYPE_CARRAY2 || symbasetype == TYPE_NARRAY2 || symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2) goto writesymadr2;
					else if (symbasetype == TYPE_CARRAY3 || symbasetype == TYPE_NARRAY3 || symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3) goto writesymadr3;
				}
				if (symbasetype == TYPE_LIST || symbasetype == TYPE_CVARLIST ||
					symbasetype == TYPE_CARRAY1 || symbasetype == TYPE_NARRAY1 ||
					symbasetype == TYPE_CARRAY2 || symbasetype == TYPE_NARRAY2 ||
					symbasetype == TYPE_CARRAY3 || symbasetype == TYPE_NARRAY3 ||
					symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1 ||
					symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2 ||
					symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3) goto writesymadr0;
				break;
			case RULE_MOVEDEST:
				if (symbasetype == TYPE_CVAR || symbasetype == TYPE_NVAR ||
				    symbasetype == TYPE_LIST || symbasetype == TYPE_CVARLIST) goto writesymadr0;
				if (line[linecnt] == '[' || line[linecnt] == '(') {
					if (symbasetype == TYPE_CARRAY1 || symbasetype == TYPE_NARRAY1 || symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1) goto writesymadr1;
					else if (symbasetype == TYPE_CARRAY2 || symbasetype == TYPE_NARRAY2 || symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2) goto writesymadr2;
					else if (symbasetype == TYPE_CARRAY3 || symbasetype == TYPE_NARRAY3 || symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3) goto writesymadr3;
				}
				if (symbasetype == TYPE_LIST || symbasetype == TYPE_CVARLIST ||
					symbasetype == TYPE_CARRAY1 || symbasetype == TYPE_NARRAY1 ||
					symbasetype == TYPE_CARRAY2 || symbasetype == TYPE_NARRAY2 ||
					symbasetype == TYPE_CARRAY3 || symbasetype == TYPE_NARRAY3 ||
					symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1 ||
					symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2 ||
					symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3) goto writesymadr0;
				break;
			case RULE_MOVESRC:
				if (symtype == TYPE_EQUATE) {
					symvalue = vmakenlit(symvalue);
					symtype = TYPE_NUMBER;
					goto writesymadr0;
				}
				if (symbasetype == TYPE_CVAR || symbasetype == TYPE_NVAR || 
				    symbasetype == TYPE_LIST || symbasetype == TYPE_CVARLIST) goto writesymadr0;
				if (line[linecnt] == '[' || line[linecnt] == '(') {
					if (symbasetype == TYPE_CARRAY1 || symbasetype == TYPE_NARRAY1 || symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1) goto writesymadr1;
					else if (symbasetype == TYPE_CARRAY2 || symbasetype == TYPE_NARRAY2 || symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2) goto writesymadr2;
					else if (symbasetype == TYPE_CARRAY3 || symbasetype == TYPE_NARRAY3 || symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3) goto writesymadr3;
				}
				if (symbasetype == TYPE_CARRAY1 || symbasetype == TYPE_NARRAY1 ||
					symbasetype == TYPE_CARRAY2 || symbasetype == TYPE_NARRAY2 ||
					symbasetype == TYPE_CARRAY3 || symbasetype == TYPE_NARRAY3 ||
					symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1 ||
					symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2 ||
					symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3) goto writesymadr0;
				break;
			case RULE_NSRCCONST:
			case RULE_NSRCSCON:
			case RULE_NSRCDCON:
			case RULE_NSRC:
			case RULE_NVAR:
				if (symbasetype == TYPE_NVAR) goto writesymadr0;
				if (symbasetype == TYPE_NARRAY1 || symtype == TYPE_NVARPTRARRAY1) goto writesymadr1;
				if (symbasetype == TYPE_NARRAY2 || symtype == TYPE_NVARPTRARRAY2) goto writesymadr2;
				if (symbasetype == TYPE_NARRAY3 || symtype == TYPE_NVARPTRARRAY3) goto writesymadr3;
				break;
			case RULE_NARRAY:
				if (symbasetype == TYPE_NARRAY1 || symbasetype == TYPE_NARRAY2 || symbasetype == TYPE_NARRAY3) goto writesymadr0;
				break;
			case RULE_NARRAYELEMENT:
				if (line[linecnt] == '[' || line[linecnt] == '(') {
					if (symbasetype == TYPE_NARRAY1 || symtype == TYPE_NVARPTRARRAY1) goto writesymadr1;
					if (symbasetype == TYPE_NARRAY2 || symtype == TYPE_NVARPTRARRAY2) goto writesymadr2;
					if (symbasetype == TYPE_NARRAY3 || symtype == TYPE_NVARPTRARRAY3) goto writesymadr3;
				}
				else if (symbasetype == TYPE_NARRAY1 || symbasetype == TYPE_NARRAY2 || symbasetype == TYPE_NARRAY3) goto writesymadr0;
				break;
			case RULE_CARRAYELEMENT:
				if (line[linecnt] == '[' || line[linecnt] == '(') {
					if (symbasetype == TYPE_CARRAY1 || symtype == TYPE_CVARPTRARRAY1) goto writesymadr1;
					if (symbasetype == TYPE_CARRAY2 || symtype == TYPE_CVARPTRARRAY2) goto writesymadr2;
					if (symbasetype == TYPE_CARRAY3 || symtype == TYPE_CVARPTRARRAY3) goto writesymadr3;
				}
				else if (symbasetype == TYPE_CARRAY1 || symbasetype == TYPE_CARRAY2 || symbasetype == TYPE_CARRAY3) goto writesymadr0;
				break;
			case RULE_NARRAY1:
				if (symbasetype == TYPE_NARRAY1) goto writesymadr0;
				break;
			case RULE_NARRAY2:
				if (symbasetype == TYPE_NARRAY2) goto writesymadr0;
				break;
			case RULE_NARRAY3:
				if (symbasetype == TYPE_NARRAY3) goto writesymadr0;
				break;
			case RULE_ARRAY:
				if (symbasetype == TYPE_CARRAY1 || symbasetype == TYPE_NARRAY1 ||
					symbasetype == TYPE_CARRAY2 || symbasetype == TYPE_NARRAY2 ||
					symbasetype == TYPE_CARRAY3 || symbasetype == TYPE_NARRAY3) goto writesymadr0;
#if 0
/*** NOTE: Thought about supporting this so that verb support of "array" using ***/
/***       char/num/var [array-spec]@ could be supported.  Need to consider "carray", ***/
/***       "carray1", ..., "narray", ....  Problem is that rule_carray and rule_narray ***/
/***       are used in other places than verb types.  The reason that this was not ***/
/***       supported was because moveadr could not be used in the routine without ***/
/***       creating a type like var @[], var @[,], ....  We could create this type, ***/
/***       what about char and num as we would need something like char @[]@, etc ***/
/***       to accomplish the equivelant.  As to regards of creating a double pointer, ***/
/***       this already exists using type "any", var [1]@ and using storeadr. ***/
					symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1 || symtype == TYPE_VVARPTRARRAY1 ||
					symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2 || symtype == TYPE_VVARPTRARRAY2 ||
					symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3 || symtype == TYPE_VVARPTRARRAY3) goto writesymadr0;
#endif
				break;
			case RULE_LOADSRC:
			case RULE_CNSRC:
			case RULE_CNVAR:
			case RULE_CNSRCCONST:
				if (symbasetype == TYPE_CVAR || symbasetype == TYPE_NVAR) goto writesymadr0;
				if (symbasetype == TYPE_CARRAY1 || symbasetype == TYPE_NARRAY1 || symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1) goto writesymadr1;
				if (symbasetype == TYPE_CARRAY2 || symbasetype == TYPE_NARRAY2 || symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2) goto writesymadr2;
				if (symbasetype == TYPE_CARRAY3 || symbasetype == TYPE_NARRAY3 || symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3) goto writesymadr3;
				break;
			case RULE_CNVVARSRC:
				if (symbasetype == TYPE_CVAR || symbasetype == TYPE_NVAR) goto writesymadr0;
				if (line[linecnt] == '[' || line[linecnt] == '(') {
					if (symbasetype == TYPE_CARRAY1 || symbasetype == TYPE_NARRAY1 || symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1) goto writesymadr1;
					else if (symbasetype == TYPE_CARRAY2 || symbasetype == TYPE_NARRAY2 || symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2) goto writesymadr2;
					else if (symbasetype == TYPE_CARRAY3 || symbasetype == TYPE_NARRAY3 || symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3) goto writesymadr3;
				}
				else {
					if (symbasetype == TYPE_CARRAY1 || symbasetype == TYPE_NARRAY1 ||
					    symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1 ||
					    symbasetype == TYPE_CARRAY2 || symbasetype == TYPE_NARRAY2 ||
					    symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2 ||
					    symbasetype == TYPE_CARRAY3 || symbasetype == TYPE_NARRAY3 ||
					    symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3) goto writesymadr0;
				}
				if (symtype == TYPE_VVARPTR) goto writesymadr0;
				if (line[linecnt] == '[' || line[linecnt] == '(') {
					if (symtype == TYPE_VVARPTRARRAY1) goto writesymadr1;
					if (symtype == TYPE_VVARPTRARRAY2) goto writesymadr2;
					if (symtype == TYPE_VVARPTRARRAY3) goto writesymadr3;
				}
				break;
			case RULE_MATHSRC:
			case RULE_MATHDEST:
				if (symtype == TYPE_EQUATE) {
					if (r == RULE_MATHSRC) {
						symtype = TYPE_NUMBER;
						if (symvalue >= 0 && symvalue < 256) {
							putcodehhll((UINT) 0xFF00 + (UCHAR) symvalue);
							goto parsedelimiter;
						}
						else if (symvalue < 0 && symvalue > -256) {
							putcodehhll((UINT) 0xFE00 - (INT) symvalue);
							goto parsedelimiter;
						}
					}
					else break;
					symvalue = vmakenlit(symvalue);
					goto writesymadr0;
				}
				if (symbasetype == TYPE_NVAR) goto writesymadr0;
				if (line[linecnt] == '[' || line[linecnt] == '(') {
					if (symbasetype == TYPE_NARRAY1 || symtype == TYPE_NVARPTRARRAY1) goto writesymadr1;
					if (symbasetype == TYPE_NARRAY2 || symtype == TYPE_NVARPTRARRAY2) goto writesymadr2;
					if (symbasetype == TYPE_NARRAY3 || symtype == TYPE_NVARPTRARRAY3) goto writesymadr3;
				}
				if (symbasetype == TYPE_NARRAY1 || symbasetype == TYPE_NARRAY2 ||
				    symbasetype == TYPE_NARRAY3 || symtype == TYPE_NVARPTRARRAY1 ||
				    symtype == TYPE_NVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY3) goto writesymadr0;
				break;
			case RULE_VVARPTR:
				if (symtype == TYPE_VVARPTR) goto writesymadr0;
				if (line[linecnt] == '[' || line[linecnt] == '(') {
					if (symtype == TYPE_VVARPTRARRAY1) goto writesymadr1;
					if (symtype == TYPE_VVARPTRARRAY2) goto writesymadr2;
					if (symtype == TYPE_VVARPTRARRAY3) goto writesymadr3;
				}
				break;
			case RULE_CNVVARPTR:
				if (symtype == TYPE_CVARPTR || symtype == TYPE_NVARPTR || symtype == TYPE_VVARPTR) goto writesymadr0;
				if (symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1 || symtype == TYPE_VVARPTRARRAY1) goto writesymadr1;
				if (symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2 || symtype == TYPE_VVARPTRARRAY2) goto writesymadr2;
				if (symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3 || symtype == TYPE_VVARPTRARRAY3) goto writesymadr3;
				break;
			case RULE_MAKEVARDEST:
				if (symtype == TYPE_CVARPTR || symtype == TYPE_NVARPTR || 
				    symtype == TYPE_VVARPTR || symtype == TYPE_DEVICEPTR ||
				    symtype == TYPE_IMAGEPTR || symtype == TYPE_RESOURCEPTR ||
				    symtype == TYPE_OBJECTPTR) goto writesymadr0;
				if (line[linecnt] == '[' || line[linecnt] == '(') {
					if (symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1 || symtype == TYPE_VVARPTRARRAY1) goto writesymadr1;
					if (symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2 || symtype == TYPE_VVARPTRARRAY2) goto writesymadr2;
					if (symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3 || symtype == TYPE_VVARPTRARRAY3) goto writesymadr3;
				}
				else {
					if (symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1 || symtype == TYPE_VVARPTRARRAY1 ||
					    symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2 || symtype == TYPE_VVARPTRARRAY2 ||
					    symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3 || symtype == TYPE_VVARPTRARRAY3) goto writesymadr0;
				}
				if (symbasetype >= TYPE_CVAR &&
				    symbasetype <= TYPE_NARRAY3 &&
				    (symtype & TYPE_PTR)) goto writesymadr0;
				break;
			case RULE_LIST:
				if (symbasetype == TYPE_LIST || symbasetype == TYPE_CVARLIST) goto writesymadr0;
				break;
			case RULE_FILE:
				if (symbasetype == TYPE_FILE) goto writesymadr0;
				break;
			case RULE_AFILE:
				if (symbasetype == TYPE_AFILE) goto writesymadr0;
				break;
			case RULE_IFILE:
				if (symbasetype == TYPE_IFILE) goto writesymadr0;
				break;
			case RULE_AFIFILE:
				if (symbasetype == TYPE_FILE || symbasetype == TYPE_IFILE || symbasetype == TYPE_AFILE) goto writesymadr0;
				break;
			case RULE_PFILE:
				if (symbasetype == TYPE_PFILE) goto writesymadr0;
				break;
			case RULE_COMFILE:
				if (symbasetype == TYPE_COMFILE) goto writesymadr0;
				break;
			case RULE_DEVICE:
				if (symbasetype == TYPE_DEVICE) goto writesymadr0;
				break;
			case RULE_RESOURCE:
				if (symbasetype == TYPE_RESOURCE) goto writesymadr0;
				break;
			case RULE_IMAGE:
				if (symbasetype == TYPE_IMAGE) goto writesymadr0;
				break;
			case RULE_QUEUE:
				if (symbasetype == TYPE_QUEUE) goto writesymadr0;
				break;
			case RULE_AFIDEVRES:
				if (symbasetype == TYPE_FILE || symbasetype == TYPE_IFILE || symbasetype == TYPE_AFILE ||
					symbasetype == TYPE_DEVICE || symbasetype == TYPE_RESOURCE) goto writesymadr0;
				break;
			case RULE_ANYVARPTR:
				if (line[linecnt] == '[' || line[linecnt] == '(') {
					if (symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1 || symtype == TYPE_VVARPTRARRAY1) goto writesymadr1;
					if (symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2 || symtype == TYPE_VVARPTRARRAY2) goto writesymadr2;
					if (symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3 || symtype == TYPE_VVARPTRARRAY3) goto writesymadr3;
				}
				if (symbasetype <= TYPE_OBJECT && (symtype & TYPE_PTR)) goto writesymadr0;
				break;
			case RULE_ANYSRC:
			case RULE_ANYVAR:
				if (line[linecnt] == '[' || line[linecnt] == '(') {
					if (symbasetype == TYPE_CARRAY1 || symbasetype == TYPE_NARRAY1 ||
					    symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1 ||
					    symtype == TYPE_VVARPTRARRAY1) goto writesymadr1;
					if (symbasetype == TYPE_CARRAY2 || symbasetype == TYPE_NARRAY2 ||
					    symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2 ||
					    symtype == TYPE_VVARPTRARRAY2) goto writesymadr2;
					if (symbasetype == TYPE_CARRAY3 || symbasetype == TYPE_NARRAY3 ||
					    symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3 ||
					    symtype == TYPE_VVARPTRARRAY3) goto writesymadr3;
				}
				if (symbasetype <= TYPE_OBJECT) goto writesymadr0;
				break;
			case RULE_TRAPEVENT:
				if (!(r = (UCHAR) parseevent())) goto parsedelimiter;
				if (r == RULE_NOPARSE) goto stmtloop;
				goto checktype1;
			case RULE_OBJECT:
				if (symbasetype == TYPE_OBJECT) goto writesymadr0;
				break;
			case RULE_OBJECTPTR:
				if (symtype == TYPE_OBJECTPTR) goto writesymadr0;
				break;
			case RULE_DAFB:
				if (symbasetype == TYPE_OBJECT || symbasetype == TYPE_FILE ||
				    symbasetype == TYPE_IFILE || symbasetype == TYPE_AFILE ||
				    symbasetype == TYPE_PFILE || symbasetype == TYPE_COMFILE ||
				    symbasetype == TYPE_DEVICE || symbasetype == TYPE_RESOURCE) goto writesymadr0;
				break;
		}
	}
	else if (tokentype == TOKEN_NUMBER) {
		graphicparseflg = FALSE;
		switch (r) {
			case RULE_DISPLAYSRC:
			case RULE_PRINTSRC:
				if (!checkxcon(token)) {
					symtype = TYPE_NUMBER;
					putcode1(251);
					putcode1((UCHAR) symvalue);
					goto parsedelimiter;
				}
				else {
					error(ERROR_BADDCON);
					goto stmtloop;
				}
			case RULE_XCON:
				if (!checkxcon(token)) goto parsedelimiter;
				error(ERROR_BADDCON);
				goto stmtloop;
			case RULE_DCON:
			case RULE_SCON:
			case RULE_NSRCDCON:
			case RULE_NSRCSCON:
				if (r == RULE_NSRCDCON) {
					symvalue = atoi((CHAR *) token);
					symtype = TYPE_INTEGER;
					goto parsedelimiter;
				}
				else {
					if (!checkdcon(token)) goto parsedelimiter;
					error(ERROR_BADDCON);
					goto stmtloop;
				}
			case RULE_ANYSRC:
			case RULE_MOVESRC:
			case RULE_LOADSRC:
			case RULE_MATHSRC:
			case RULE_NSRC:
			case RULE_NSRCCONST:
			case RULE_CNSRC:
			case RULE_CNDATASRC:
			case RULE_CNSRCCONST:
			case RULE_NSRCEQU:
			case RULE_CNSRCXCON:
				if (r == RULE_CNSRCXCON && *token == '0') {
					if (!checkxcon(token)) goto parsedelimiter;
					error(ERROR_BADDCON);
					goto stmtloop;
				} 
				if ((i = checknum(token)) >= 0) {
					symtype = TYPE_NUMBER;
					if (!i) {
						if (r == RULE_MATHSRC) {
							symvalue = atoi((CHAR *) token);
							if (symvalue >= 0 && symvalue < 256) {
								putcodehhll((UINT) 0xFF00 + (UCHAR) symvalue);
								goto parsedelimiter;
							}
						}
						else if (r == RULE_NSRCCONST || r == RULE_CNSRCCONST) {
							symvalue = atoi((CHAR *) token);
							if (symvalue < 128) {
								symtype = TYPE_BYTECONST;
								putcode1((UCHAR) symvalue);
								goto parsedelimiter;
							}
						}
					}
					symvalue = makenlit((CHAR *) token);
					goto writesymadr0;
				}
				error(ERROR_BADNUMLIT);
				goto stmtloop;
		}
	}
	else if (tokentype == TOKEN_MINUS) {
		graphicparseflg = FALSE;
		scantoken(upcaseflg);
		if (r == RULE_MOVESRC || r == RULE_MATHSRC || r == RULE_NSRC ||
		    r == RULE_CNSRC || r == RULE_CNDATASRC || r == RULE_NSRCCONST ||
		    r == RULE_CNSRCCONST || r == RULE_DISPLAYSRC || r == RULE_NSRCEQU ||
		    r == RULE_PRINTSRC || r == RULE_ANYSRC || r == RULE_LOADSRC) {
			if (tokentype == TOKEN_NUMBER) {
				memmove(&token[1], token, strlen((CHAR *) token) + 1);
				token[0] = '-';
				if ((i = checknum(token)) >= 0) {
					symtype = TYPE_NUMBER;
					if (!i) {
						if (r == RULE_MATHSRC) {
							symvalue = atoi((CHAR *) token);
							if (symvalue > -256 && symvalue < 0) {
								putcodehhll((UINT) 0xFE00 - (INT) symvalue);
								goto parsedelimiter;
							}
						}
						else if (r == RULE_NSRCCONST || r == RULE_CNSRCCONST) {
							symvalue = atoi((CHAR *) token);
							if (symvalue > -128) {
								symtype = TYPE_BYTECONST;
								putcode1((UCHAR) symvalue);
								goto parsedelimiter;
							}
						}
					}
					symvalue = makenlit((CHAR *) token);
					goto writesymadr0;
				}
				error(ERROR_BADNUMLIT);
				goto stmtloop;
			}
		}
		if (r == RULE_NSRCSCON || (rule & 0xFF00) == RULE_SCON << 8) {
			if (tokentype == TOKEN_WORD) {
				if (getdsym((CHAR *) token) == -1) error(ERROR_VARNOTFOUND);
				if (symtype == TYPE_EQUATE) {
					symvalue *= (-1);
					symtype = TYPE_INTEGER;
					goto parsedelimiter;
				}
			}
			else if (tokentype == TOKEN_NUMBER) {
				if (!checkdcon(token)) {
					memmove(&token[1], token, strlen((CHAR *) token) + 1);
					token[0] = '-';
					symvalue *= (-1);
					goto parsedelimiter;
				}
			}
		}
		goto badsyntax;
	}
	else if (tokentype == TOKEN_SEMICOLON && r == RULE_EMPTYLIST) {
		delimiter = TOKEN_SEMICOLON;
		goto endnext4;
	}
	else if (tokentype == TOKEN_TILDE && r == RULE_ANYSRC) {
		scantoken(upcaseflg);
		if (tokentype != TOKEN_WORD) goto badsyntax;
		usepsym((CHAR *) token, LABEL_REFFORCE);
		putcodeadr(0x800000 + symvalue);
		goto parsedelimiter;
	}
	if (r == RULE_OPTIONAL) {
		symtype = tokentype = delimiter = 0;
		nextflag = 2;
		goto endnext4;
	}
	if (rcount == 1) {
		r = (UCHAR) (rule >> 8);
		rcount = 2;
		goto checktype;
	}
	if (rcount == 2) {
		r = (UCHAR) (rule >> 16);
		rcount = 3;
		goto checktype;
	}
	if (tokentype != TOKEN_WORD) goto badsyntax;
	error(ERROR_BADVARTYPE);
	goto stmtloop;

writesymadr0:
	putcodeadr(symvalue);
	goto parsedelimiter;
writesymadr1:
	if (!arraysymaddr(1)) goto stmtloop;
	goto parsedelimiter;
writesymadr2:
	if (!arraysymaddr(2)) goto stmtloop;
	goto parsedelimiter;
writesymadr3:
	if (!arraysymaddr(3)) goto stmtloop;
	goto parsedelimiter;

parsedelimiter:
	r = (UCHAR) (rule >> 24);
	nextflag = delimiter = 0;
	savtokentype = tokentype;
	switch (r) {
		case (NEXT_NOPARSE >> 24):
			goto endnext4;
		case (NEXT_END >> 24):
			nextflag = 2;
			if (ccrulecnt) goto endnext1;
			if (line[linecnt] && charmap[line[linecnt]] != 2) goto badsyntax;
			goto endnext4;
		case (NEXT_ENDOPT >> 24):
			nextflag = 2;
			goto endnext4;
		case (NEXT_SEMILIST >> 24):
			// fall through
		case (NEXT_SEMILISTNOEND >> 24):
			if (line[linecnt] == ';') {
				delimiter = TOKEN_SEMICOLON;
				linecnt++;
				if (r != (NEXT_SEMILISTNOEND >> 24)) nextflag = 2;
				goto endnext3;
			}
			// fall through
			/* no break */
		case (NEXT_COMMAOPT >> 24):
		case (NEXT_LIST >> 24):
			if (r == (NEXT_LIST >> 24) || r == (NEXT_SEMILIST >> 24)) nextflag = 1;
			delimiter = TOKEN_COMMA;
			if (line[linecnt] == ',') {
				linecnt++;
				goto endnext3;
			}
			if (line[linecnt] == ':') goto endnext2;
			if (line[linecnt] && charmap[line[linecnt]] != 2 && !noparseflg) {
				token[0] = line[linecnt];
				token[1] = '\0';
				error(ERROR_BADPREP);
				goto stmtloop;
			}
			delimiter = 0;
			nextflag = 2;
			goto endnext4;
		case (NEXT_COMMA >> 24):
			delimiter = TOKEN_COMMA;
			if (line[linecnt] == ',') {
				linecnt++;
				goto endnext3;
			}
			if (line[linecnt] == ':') goto endnext2;
			goto badsyntax;
		case (NEXT_IFOPT >> 24):
			if (line[linecnt] && charmap[line[linecnt]] != 2 && !noparseflg) {
				token[0] = line[linecnt];
				token[1] = '\0';
				error(ERROR_BADPREP);
				goto stmtloop;
			}
			whitespace();
			if ((line[linecnt] == 'I' || line[linecnt] == 'i') &&
				(line[linecnt + 1] == 'F' || line[linecnt + 1] == 'f') &&
				charmap[line[linecnt + 2]] == 2) {
				linecnt += 3;
				delimiter = TOKEN_IF;
				goto endnext3;
			}
			delimiter = 0;
			nextflag = 2;
			goto endnext4;
		case (NEXT_GIVINGOPT >> 24):
			delimiter = TOKEN_COMMA;
			if (line[linecnt] == ',') {
				linecnt++;
				goto endnext3;
			}
			else if (line[linecnt] == ':') goto endnext2;
			else {
				if (line[linecnt] && charmap[line[linecnt]] != 2 && !noparseflg) {
					token[0] = line[linecnt];
					token[1] = '\0';
					error(ERROR_BADPREP);
					goto stmtloop;
				}
				whitespace();
				scantoken(SCAN_UPCASE);
				if (tokentype == TOKEN_WORD && !strcmp((CHAR *) token, "GIVING")) {
					tokentype = savtokentype;
					goto endnext3;
				}
			}
			delimiter = 0;
			nextflag = 2;
			tokentype = savtokentype;
			goto endnext4;
		case (NEXT_IFPREPOPT >> 24):
		case (NEXT_PREP >> 24):
		case (NEXT_PREPOPT >> 24):
		case (NEXT_PREPGIVING >> 24):
			delimiter = TOKEN_PREP;
			if (line[linecnt] == ',') {
				linecnt++;
				delimtype = TOKEN_COMMA;
				goto endnext3;
			}
			if (line[linecnt] == ':') {
				delimtype = TOKEN_COMMA;
				goto endnext2;
			}
			if (line[linecnt] && charmap[line[linecnt]] != 2 && !noparseflg) {
				token[0] = line[linecnt];
				token[1] = 0;
				error(ERROR_BADPREP);
				goto stmtloop;
			}
			whitespace();
			/* special case for CLOCK - must save token (i.e. TIME) before scanning next token */
			memcpy(&token[200], &token[0], 12);
			scantoken(SCAN_UPCASE);
			if (tokentype == TOKEN_WORD) {
				if (!(i = strcmp((CHAR *) token, "TO"))) delimtype = TOKEN_TO;
				else if (!(i = strcmp((CHAR *) token, "FROM"))) delimtype = TOKEN_FROM;
				else if (!(i = strcmp((CHAR *) token, "IN"))) delimtype = TOKEN_IN;
				else if (!(i = strcmp((CHAR *) token, "INTO"))) delimtype = TOKEN_INTO;
				else if (!(i = strcmp((CHAR *) token, "BY"))) delimtype = TOKEN_BY;
				else if (!(i = strcmp((CHAR *) token, "OF"))) delimtype = TOKEN_OF;
				else if (!(i = strcmp((CHAR *) token, "WITH"))) delimtype = TOKEN_WITH;
				else if (!(i = strcmp((CHAR *) token, "USING"))) delimtype = TOKEN_USING;
				if (!i) {
					memcpy(token, &token[200], 12);
					tokentype = savtokentype;
					goto endnext3;
				}
			}
			tokentype = savtokentype;
			if (r == (NEXT_PREPOPT >> 24)) {
				delimiter = 0;
				nextflag = 2;
				goto endnext4;
			}
			if (r == (NEXT_IFPREPOPT >> 24)) {
				delimiter = TOKEN_IF;
				delimtype = TOKEN_IF;
				if (!strcmp((CHAR *) token, "IF")) goto endnext3;
				delimiter = 0;
				nextflag = 2;
				goto endnext4;
			}
			if (r == (NEXT_PREPGIVING >> 24) && !strcmp((CHAR *) token, "GIVING")) goto endnext3;
			error(ERROR_BADPREP);
			goto stmtloop;
		case (NEXT_SEMICOLON >> 24):
			delimiter = TOKEN_SEMICOLON;
			if (line[linecnt++] == ';') goto endnext3;
			error(ERROR_MISSINGSEMICOLON);
			goto stmtloop;
		case (NEXT_COMMASEMI >> 24):
			if (line[linecnt] == ',') {
				linecnt++;
				delimiter = TOKEN_COMMA;
				goto endnext3;
			}
			if (line[linecnt] == ':') {
				delimiter = TOKEN_COMMA;
				goto endnext2;
			}
			if (line[linecnt] == ';') {
				linecnt++;
				delimiter = TOKEN_SEMICOLON;
				goto endnext3;
			}
			goto badsyntax;
		case (NEXT_SEMIOPT >> 24):
			if (line[linecnt] == ';') {
				linecnt++;
				delimiter = TOKEN_SEMICOLON;
				goto endnext3;
			}
			if (line[linecnt]) {
				if (charmap[line[linecnt]] != 2 && !noparseflg) {
					token[0] = line[linecnt];
					token[1] = 0;
					error(ERROR_BADPREP);
					goto stmtloop;
				}
				linecnt++;
			}
			delimiter = 0;
			nextflag = 2;
			goto endnext4;
		case (NEXT_COLON >> 24):
			delimiter = TOKEN_COLON;
			if (line[linecnt++] == ':') {
				if (ccrulecnt) {
					whitespace();
					goto endnext1;
				}
				goto endnext4;
			}
			goto badsyntax;
		case (NEXT_COLONOPT >> 24):
			if (line[linecnt] == ':') {
				delimiter = TOKEN_COLON;
				linecnt++;
				whitespace();
				goto endnext1;
			}
			delimiter = 0;
			goto endnext1;
		case (NEXT_RBRACKET >> 24):
			whitespace();
			delimiter = TOKEN_RBRACKET;
			if (line[linecnt] == ']' || line[linecnt] == ')') {
				linecnt++;
				goto endnext4;
			}
			goto badsyntax;
		case (NEXT_COMMARBRKT >> 24):
			if (line[linecnt] == ',') {
				linecnt++;
				delimiter = TOKEN_COMMA;
				goto endnext3;
			}
			if (line[linecnt] == ':') {
				delimiter = TOKEN_COMMA;
				goto endnext2;
			}
			if (line[linecnt] == ']' || line[linecnt] == ')')	{
				linecnt++;
				delimiter = TOKEN_RBRACKET;
				goto endnext4;
			}
			goto badsyntax;
		case (NEXT_CASE >> 24):
			delimiter = 0;
			whitespace();
			do {
				scantoken(SCAN_UPCASE);
				if (tokentype == TOKEN_WORD && !strcmp((CHAR *) token, "OR")) delimiter = TOKEN_OR;
				else if (tokentype == TOKEN_ORBAR) delimiter = TOKEN_ORBAR;
				if (delimiter == TOKEN_ORBAR || delimiter == TOKEN_OR) {
					savlinepos = linecnt;
					whitespace();
					scantoken(SCAN_UPCASE);
					if (tokentype != TOKEN_COLON) {
						linecnt = savlinepos;
						tokentype = savtokentype;
						goto endnext3;
					}
				}
				if (tokentype == TOKEN_COLON) {
					if (dspflags & DSPFLAG_COMPILE) getlndsp();
					if (getln()) {
						error(ERROR_BADCONTLINE);
						goto stmtloop;
					}
					linecnt = 0;
					whitespace();
					if (!linecnt) {
						error(ERROR_BADCONTLINE);
						goto stmtloop;
					}
					if (delimiter) {
						tokentype = savtokentype;
						goto endnext3;
					}
					else delimiter = TOKEN_COLON;
				}
			} while (tokentype == TOKEN_COLON);
			if (delimiter == TOKEN_COLON) {
				error(ERROR_SYNTAX);
				goto stmtloop;
			}
			nextflag = 2;
			tokentype = savtokentype;
			goto endnext4;
		case (NEXT_RELOP >> 24):
			whitespace();
			scantoken(SCAN_UPCASE);
			if (tokentype == TOKEN_EQUAL || tokentype == TOKEN_NOTEQUAL ||
				tokentype == TOKEN_LESS || tokentype == TOKEN_NOTLESS ||
				tokentype == TOKEN_GREATER || tokentype == TOKEN_NOTGREATER) {
				delimiter = tokentype;
				tokentype = savtokentype;
				goto endnext3;
			}
			goto badsyntax;
		case (NEXT_REPEAT >> 24):
			nextflag = 1;
			goto endnext4;
	}
	death(DEATH_INTERNAL2);
endnext1:
	ccrulecnt++;
	if (ccrulecnt > 1) {
		if (symtype == TYPE_INTEGER) {
			if (token[0] == '-' && symvalue > -256)	{
				symvalue *= (-1);
				putcodehhll((USHORT) (0xFE00 + (UCHAR) symvalue));
			}
			else if (symvalue < 256) putcodehhll((USHORT) (0xFF00 + (UCHAR) symvalue));
			else if (symvalue <= USHRT_MAX){
				putcode1(0xFD);
				putcodehhll((USHORT) symvalue);
			}
			else {
				/* Create a FORM field in the data area and point to it */
				symvalue = makenlit((CHAR*)token);
				symtype = TYPE_NUMBER;
				putcodeadr(symvalue);
			}
		}
	}
	rule = 0;
	switch(ccrule) {
		case CCPARSE_NVARCON:
			if (ccrulecnt == 1) {
				if (!scrlparseflg) rule = RULE_NSRCDCON + (RULE_DCON << 8) + NEXT_END;
				else {
					rule = RULE_NSRCDCON + (RULE_DCON << 8) + NEXT_COLONOPT;
					ccrule = CCPARSE_SCRLR;
				}
			}
			break;
		case CCPARSE_2NVARCON:
			if (ccrulecnt == 1) rule = RULE_NSRCDCON + (RULE_DCON << 8) + NEXT_COLON;
			else if (ccrulecnt == 2) rule = RULE_NSRCDCON + (RULE_DCON << 8) + NEXT_END;
			break;
		case CCPARSE_NVARNCON:
			if (ccrulecnt == 1) rule = RULE_NSRCSCON + (RULE_SCON << 8) + NEXT_END;
			break;
		case CCPARSE_4NVARCON:
			if (ccrulecnt < 4) rule = RULE_NSRCDCON + (RULE_DCON << 8) + NEXT_COLON;
			else if (ccrulecnt == 4) rule = RULE_NSRCDCON + (RULE_DCON << 8) + NEXT_END;
			break;
		case CCPARSE_CVARLIT_NVARCON:
			if (ccrulecnt == 1) {
				if (graphicparseflg) rule = RULE_CSRC + (RULE_DSCC << 8) + NEXT_COLON;
				else rule = RULE_CSRC + (RULE_PRCC << 8) + NEXT_COLON;
			}
			else if (ccrulecnt == 2)	rule = RULE_NSRCDCON + (RULE_DCON << 8) + NEXT_END;
			break;
		case CCPARSE_CVAR_NVARCON:
			if (ccrulecnt == 1) rule = RULE_CSRC + NEXT_COLON;
			else if (ccrulecnt == 2) rule = RULE_NSRCDCON + (RULE_DCON << 8) + NEXT_END;
			break;
		case CCPARSE_SCRLR:
			rulecnt = 0;
			if (scrlparseflg) {
				if (delimiter != TOKEN_COLON)	{
					ccrule = 0;
					putcode1(0xFF);
					scrlparseflg = FALSE;
					rule = RULE_DISPLAYSRC + (RULE_DSCC << 8) + NEXT_SEMILIST;
					goto parsedelimiter;
				}
				else {
					whitespace();
					if (!line[linecnt]) {
						rule = RULE_DISPLAYSRC + (RULE_DSCC << 8) + NEXT_COLONOPT;
						goto endnext2;
					}
				}
			}
			scrlparseflg = TRUE;
			rule = RULE_DISPLAYSRC + (RULE_DSCC << 8) + NEXT_COLONOPT;
			break;
		case CCPARSE_SCREND:
			putcode1(0xFF);
			scrlparseflg = FALSE;
			rule = RULE_DISPLAYSRC + (RULE_DSCC << 8) + NEXT_SEMILIST;
			goto parsedelimiter;
		case CCPARSE_CVARLIT:
			if (ccrulecnt == 1) rule = RULE_CSRC + NEXT_END;
			break;
		case CCPARSE_PNVAR:	/* *P <adr>:<?> case */
			if (ccrulecnt == 1) rule = RULE_NSRC + NEXT_COLON;
			else if (ccrulecnt == 2) rule = RULE_NSRCDCON + (RULE_DCON << 8) + NEXT_END;
			else if (symtype == TYPE_INTEGER && symvalue < 256) {
				codebufcnt--;
				codebuf[codebufcnt - 1] = (UCHAR) symvalue;
				codebuf[savcodebufcnt] = 201;
			}
			break;
		case CCPARSE_PCON:	 /* *P <con>:<?> case */
			if (ccrulecnt == 1) rule = RULE_NSRCDCON + (RULE_DCON << 8) + NEXT_END;
			else if (codebuf[savcodebufcnt] == 199 && symtype == TYPE_INTEGER && symvalue < 256) {
				codebufcnt--;
				codebuf[codebufcnt - 1] = (UCHAR) symvalue;
				codebuf[savcodebufcnt] = 201;
			}
			else if (codebuf[savcodebufcnt] == 198) {
				if (symtype == TYPE_INTEGER && symvalue < 256) {
					codebufcnt--;
					codebuf[codebufcnt - 1] = (UCHAR) symvalue;
				}
				else codebuf[savcodebufcnt] = 200;
			}
			break;
	}
	if (!rule) {
		ccrulecnt = 0;
		rule = sptr->rule;
		goto parsedelimiter;
	}
	goto ruleloop;
endnext2:
	while (TRUE) {
		if (dspflags & DSPFLAG_COMPILE) getlndsp();
		if (getln()) {
			error(ERROR_BADCONTLINE);
			goto stmtloop;
		}
		for (n = 0; charmap[line[n]] == 2; n++);
		if (!line[n]) continue;
		if (n) break;
		error(ERROR_BADCONTLINE);
		goto stmtloop;
	}
	linecnt = n;
	if (scrlparseflg) goto ruleloop;
	else goto endnext4;
endnext3:
	whitespace();
endnext4:
	if ((void *) sptr->rulefnc != NULL) {
		rule = (*(sptr->rulefnc))();
		if (rule) {
			if (rule == NEXT_TERMINATE) goto endstmt;
			nextflagsave = nextflag;
			goto ruleloop;
		}
		if (nextflagsave < 3) {
			nextflag = nextflagsave;
			nextflagsave = 3;
		}
	}
	if (nextflag == 1) {
		rule = sptr->rule;
		goto ruleloop;
	}
	if (nextflag == 0) {
		sptr++;
		rule = sptr->rule;
		goto ruleloop;
	}
endstmt:
	putcodeout();	/* flush codebuf to main buffer */
	if (warninglevel) {
		whitespace();
		if (warninglevel & WARNING_LEVEL_1) {
			if (',' == line[linecnt]) warning(WARNING_COMMACOMMENT);
/*** 1/4/2002: REMOVE '(' IF THERE ARE ANY COMPLAINTS ***/
			if ('(' == line[linecnt] || ')' == line[linecnt]) warning(WARNING_PARENCOMMENT);
		}
		if (line[linecnt] && (warninglevel & WARNING_LEVEL_2)) {
			UINT (*rf)(void);
			if (vbp == NULL && !isspace(line[linecnt])) {
				warning(WARNING_2COMMENT);
				goto stmtloop;
			}
			if (vbp != NULL && vbp->syntax != NULL) {
				rf = vbp->syntax->rulefnc;
				/* not a problem if this is a data statement */
				if (rf != s_charnum_fnc && rf != s_list_fnc && rf != s_record_fnc
						&& rf != s_file_fnc && rf != s_varlist_fnc
						&& rf != s_init_fnc && rf != s__ifdef_fnc
						&& rf != s__conditional_fnc && rf != s_equate_fnc
						&& rf != s_pfile_fnc && rf != s_include_fnc
						&& rf != s__if_fnc && rf != s_liston_fnc
						&& rf != s_endif_fnc && rf != s_define_fnc
						&& rf != s_label_fnc && rf != s_routine_fnc
						&& rf != s_endswitch_fnc && rf != s_class_fnc
						&& rf != s_object_fnc)
				{
					warning(WARNING_2COMMENT);
				}
			}
		}
	}
	goto stmtloop;

badsyntax:
	if (nocompile) goto stmtloop;
	delimiter = 0;
	error(ERROR_SYNTAX);
	goto stmtloop;
badcc:
	error(ERROR_BADLISTCONTROL);
	goto stmtloop;
}

UCHAR arraysymaddr(UINT n)	/* write sym address of an array of n dimension */
{
	UCHAR m;
	UCHAR savsymtype, savsymbase;
	INT xtype;

	elementref = TRUE;
	savsymtype = symtype;
	savsymbase = symbasetype;
	savtokentype = tokentype;
	putcode1(0xC0);
	putcodeadr(symvalue);
	m = line[linecnt++];
	if (m == '[') m = ']';
	else if (m == '(') m = ')';
	else {
		error(ERROR_BADARRAYINDEX);
		return(FALSE);
	}
	while (--n) {
		if (!(xtype = xparse(','))) return(FALSE);
		if (xtype == 2) {
			error(ERROR_BADEXPTYPE);
			return(FALSE);
		}
		if (!xgenexpcode(topnode)) return(FALSE);
		xgencodeadr(topnode);
	}
	if (!(xtype = xparse(m))) return(FALSE);
	if (xtype == 2) {
		error(ERROR_BADEXPTYPE);
		return(FALSE);
	}
	if (!xgenexpcode(topnode)) return(FALSE);
	xgencodeadr(topnode);
	symtype = savsymtype;
	symbasetype = savsymbase;
	tokentype = savtokentype;
	return(TRUE);
}

/* SCANTOKEN */
/* Scan a token from the source file */
void scantoken(UINT flagmask)
{
	UCHAR n, m, upflag, noexpflag, litflag, alphaflag, noadvflag, noastrkflag;
	INT linecntsave;

	upflag = flagmask & SCAN_UPCASE;
	noexpflag = flagmask & SCAN_NOEXPDEFINE;
	litflag = flagmask & SCAN_SCANLITERAL;
	alphaflag = flagmask & SCAN_ALPHAONLY;
	noadvflag = flagmask & SCAN_NOADVANCE;
	noastrkflag = flagmask & SCAN_NOASTRKDEFINE;

	if (noadvflag) linecntsave = linecnt;
scantokenstart:
	if (litflag) {	/* start of literal */
		n = 0;
		if (charmap[line[++linecnt]] == 3) {  /* 3 is a double quote, empty literal */
			linecnt++;
			token[0] = '\0';
			tokentype = TOKEN_LITERAL;
			if (noadvflag) linecnt = linecntsave;
			return;
		}
		while (n < MAXLITERALSIZE) {
			if (!line[linecnt]) {
				error(ERROR_SYNTAX);
				token[0] = 0;
				tokentype = 0;
				if (noadvflag) linecnt = linecntsave;
				return;
			}
			if (line[linecnt] == '#') {
				if (!(token[n++] = line[++linecnt])) {
					token[0] = 0;
					tokentype = 0;
					if (noadvflag) linecnt = linecntsave;
					return;
				}
				linecnt++;
				continue;
			}
			if (line[linecnt] == '"' && line[++linecnt] != '"') {
				token[n] = 0;
				n = 0;
				break;
			}
			token[n++] = line[linecnt++];
		}
		if (n >= MAXLITERALSIZE) {
			line[linecnt] = 0;
			error(ERROR_LITTOOLONG);
			token[0] = 0;
			tokentype = 0;
			if (noadvflag) linecnt = linecntsave;
			return;
		}
		tokentype = TOKEN_LITERAL;
		if (noadvflag) linecnt = linecntsave;
		return;
	}
	if ((n = charmap[line[linecnt]]) == 5) {  /* label start */
		m = linecnt;
		n = 1;
		if (upflag && (!definecnt || noexpflag || upcaseflg)) {
			token[0] = (UCHAR) toupper(line[linecnt]);
			linecnt++;
			if (alphaflag) {
				while (charmap[line[linecnt]] == 5) {
					token[n++] = (UCHAR) toupper(line[linecnt]);
					linecnt++;
				}
			}
			else {
				while ((charmap[line[linecnt]] > 4 && charmap[line[linecnt]] < 11) ||
					   charmap[line[linecnt]] == TOKEN_ATSIGN) {
					token[n++] = (UCHAR) toupper(line[linecnt]);
					linecnt++;
				}
			}
		}
		else {
			token[0] = line[linecnt++];
			if (alphaflag) {
				while (charmap[line[linecnt]] == 5) {
					token[n++] = line[linecnt];
					linecnt++;
				}
			}
			else {
				while ((charmap[line[linecnt]] > 4 && charmap[line[linecnt]] < 11) ||
					   charmap[line[linecnt]] == TOKEN_ATSIGN) {
					token[n++] = line[linecnt];
					linecnt++;
				}
			}
		}
		token[n] = '\0';
		if (!noexpflag && definecnt) {
			if (!getdefine((CHAR *) token, m, (INT) noastrkflag)) goto scantokenstart;
			if (upflag) for (n = 0; token[n]; n++) token[n] = (UCHAR) toupper(token[n]);
		}
		tokentype = TOKEN_WORD;
		if (noadvflag) linecnt = linecntsave;
		return;
	}
	if (n == 3) {
		token[0] = '"';
		token[1] = '\0';
		tokentype = TOKEN_DBLQUOTE;
		if (noadvflag) linecnt = linecntsave;
		return;
	}
	if (n == TOKEN_POUND) {
		tokentype = TOKEN_POUND;
		token[0] = '#';
		token[1] = '\0';
		linecnt++;
		if (noadvflag) linecnt = linecntsave;
		return;
	}
	if ((n == TOKEN_BANG) && (line[linecnt+1] == '=')) {
		tokentype = TOKEN_NOTEQUAL;
		token[0] = '!';
		token[1] = '=';
		token[2] = '\0';
		linecnt += 2;
		if (noadvflag) linecnt = linecntsave;
		return;
	}
	if (n >= TOKEN_LESS) {	/* one or two character token */
		if (n <= TOKEN_GREATER && line[linecnt + 1] == '=') {
			if (n == TOKEN_LESS) tokentype = TOKEN_NOTGREATER;
			else tokentype = TOKEN_NOTLESS;
			token[0] = line[linecnt];
			token[1] = '=';
			token[2] = '\0';
			linecnt += 2;
			if (noadvflag) linecnt = linecntsave;
			return;
		}
		if ((n == TOKEN_LESS) && (line[linecnt + 1] == '>')) {
			tokentype = TOKEN_NOTEQUAL;
			token[0] = '<';
			token[1] = '>';
			token[2] = '\0';
			linecnt += 2;
			if (noadvflag) linecnt = linecntsave;
			return;
		}
		token[0] = line[linecnt++];
		token[1] = '\0';
		tokentype = n;
		if (noadvflag) linecnt = linecntsave;
		return;
	}
	if (n > 5) {  /* digit or period */
		n = 1;
		if ((token[0] = line[linecnt++]) != '.') {
			if (token[0] == '0' && (line[linecnt] == 'X' || line[linecnt] == 'x')) {
				token[n++] = line[linecnt++];
				while (charmap[line[linecnt]] == 6 ||
					  (line[linecnt] >= 'a' && line[linecnt] <= 'f') ||
					  (line[linecnt] >= 'A' && line[linecnt] <= 'F')) {
					 token[n++] = line[linecnt++];
				}
			}
			else {
				while (charmap[line[linecnt]] == 6) token[n++] = line[linecnt++];
				if (line[linecnt] == '.') {
					linecnt++;
					token[n++] = '.';
					while (charmap[line[linecnt]] == 6) token[n++] = line[linecnt++];
				}
			}
		}
		else {
			if (charmap[line[linecnt]] != 6) {
				tokentype = TOKEN_PERIOD;
				if (noadvflag) linecnt = linecntsave;
				return;
			}
			while (charmap[line[linecnt]] == 6) token[n++] = line[linecnt++];
		}
		if (n + ((tokentype == TOKEN_MINUS) ? 1 : 0) > MAXNUMLITSIZE) {
			error(ERROR_LITTOOLONG);
			token[0] = '\0';
			tokentype = 0;
		}
		else {
			token[n] = '\0';
			tokentype = TOKEN_NUMBER;
		}
		if (noadvflag) linecnt = linecntsave;
		return;
	}
	
	token[0] = '\0';
	tokentype = 0;
	if (noadvflag) linecnt = linecntsave;
}

/**
 * parse an expression creating parse tree
 * echar is ending character (only valid characters are ')', ',' and ']'
 * return value is 0 if error, 1 if exp is numeric, 2 if exp is character
 */
static INT xparse(UINT echar)
{
	UCHAR i, n, current, state, tempnode, arraytop[32];
	UCHAR nestlevel, neststack[32], curelement[32], statesave[32];
	CHAR uptoken[TOKENSIZE];
	USHORT parendlevel, operator_prec;
	INT operator, linecntsave;

/* the state variable is defined as follows: */
/* state defines the state of the parser */
/* state = 0 means await terminal element or unary operator */
/* state = 1 means await terminal or unary op, no indexes left */
/* state = 2 means await terminal or unary op, 1 index left */
/* state = 3 means await terminal or unary op, 2 indexes left */
/* state = 4 means await binary operator or expression end */
/* state = 5 means await binary op or ']' or ')' (check neststack) */
/* state = 6 means await binary op or comma, 1 indexes left */
/* state = 7 means await binary op or comma, 2 indexes left */
/* state = 8 means await 1 dimension array '[' or '(' */
/* state = 9 means await 2 dimension array '[' or '(' */
/* state = 10 means await 3 dimension array '[' or '(' */
/* the starting value of state is 0 */
/* valid ending value of state is 4 */

	current = topnode = nestlevel = state = 0;
	parendlevel = 0;
	nodecnt = 1;
	astnode[0].upptr = astnode[0].ptr1 = 0xFF;
xparseloop:
	symtype = symbasetype = 0;
	whitespace();
	scantoken(upcaseflg);
	switch (tokentype) {
		case TOKEN_WORD:
			for (n = 0; (uptoken[n] = (CHAR) toupper(token[n])); n++);
			getdsym((CHAR *) token);
			if (state <= 3) {
				operator = -1;
				switch (uptoken[0]) {
					case 'A':
						if (!strcmp(uptoken, "ABS")) {
							operator = OP_ABS;
							break;
						}
						if (!strcmp(uptoken, "ARCSIN")) {
							operator = OP_ARCSIN;
							break;
						}
						if (!strcmp(uptoken, "ARCCOS")) {
							operator = OP_ARCCOS;
							break;
						}
						if (!strcmp(uptoken, "ARCTAN")) {
							operator = OP_ARCTAN;
							break;
						}
						break;
					case 'C':
						if (!strcmp(uptoken, "CHAR")) {
							operator = OP_CHAR;
							break;
						}
						if (!strcmp(uptoken, "CHOP")) {
							operator = OP_CHOP;
							break;
						}
						if (!strcmp(uptoken, "COS")) {
							operator = OP_COS;
							break;
						}
						break;
					case 'E':
						if (!strcmp(uptoken, "EQUAL")) {
							operator = 0;
							break;
						}
						if (!strcmp(uptoken, "EOS")) {
							operator = 3;
							break;
						}
						if (!strcmp(uptoken, "EXP")) {
							operator = OP_EXP;
							break;
						}
						break;
					case 'F':
						if (!strcmp(uptoken, "FORM")) {
							operator = OP_FORM;
							break;
						}
						if (!strcmp(uptoken, "FLOAT")) {
							operator = OP_FLOAT;
							break;
						}
						if (!strcmp(uptoken, "FORMPTR")) {
							operator = OP_FORMPTR;
							break;
						}
						if (!strcmp(uptoken, "FCHAR")) {
							operator = OP_FCHAR;
							break;
						}
						break;
					case 'G':
						if (!strcmp(uptoken, "GREATER")) {
							operator = 8;
							break;
						}
						break;
					case 'I':
						if (!strcmp(uptoken, "INT")) {
							operator = OP_INT;
							break;
						}
						if (!strcmp(uptoken, "ISNULL")) {
							operator = OP_ISNULL;
							break;
						}
						break;
					case 'L':
						if (!strcmp(uptoken, "LESS")) {
							operator = 1;
							break;
						}
						if (!strcmp(uptoken, "LENGTH")) {
							operator = OP_LENGTH;
							break;
						}
						if (!strcmp(uptoken, "LENGTHPTR")) {
							operator = OP_LENGTHPTR;
							break;
						}
						if (!strcmp(uptoken, "LCHAR")) {
							operator = OP_LCHAR;
							break;
						}
						if (!strcmp(uptoken, "LOG")) {
							operator = OP_LOG;
							break;
						}
						if (!strcmp(uptoken, "LOG10")) {
							operator = OP_LOG10;
							break;
						}
						break;
					case 'N':
						if (!strcmp(uptoken, "NOT")) {
							operator = OP_NOT;
							break;
						}
						break;
					case 'O':
						if (!strcmp(uptoken, "OVER")) {
							operator = 2;
							break;
						}
						break;
					case 'S':
						if (!strcmp(uptoken, "SIZE")) {
							operator = OP_SIZE;
							break;
						}
						if (!strcmp(uptoken, "SQUEEZE")) {
							operator = OP_SQUEEZE;
							break;
						}
						if (!strcmp(uptoken, "SIN")) {
							operator = OP_SIN;
							break;
						}
						if (!strcmp(uptoken, "SQRT")) {
							operator = OP_SQRT;
							break;
						}
						break;
					case 'T':
						if (!strcmp(uptoken, "TRIM")) {
							operator = OP_TRIM;
							break;
						}
						if (!strcmp(uptoken, "TAN")) {
							operator = OP_TAN;
							break;
						}
						break;
					case 'Z':
						if (!strcmp(uptoken, "ZERO")) {
							operator = 0;
							break;
						}
				}
				if (operator >= 0) {
					if (!symtype) {	
						/* operator does not conflict with name in symbol table */
						if (operator <= 8) goto change4;
						else goto change5;
					}
					else {
						/* operator conflicts with name in symbol table */
						/* operator has precedence if not followed by a binary operator or array ref */
						symbasetype = (UCHAR) (symtype & ~TYPE_PTR);
						if ((line[linecnt] != '[' && line[linecnt] != '(') || 
						    (symbasetype != TYPE_NARRAY1 && symbasetype != TYPE_NARRAY2 && symbasetype != TYPE_NARRAY3 &&
						     symbasetype != TYPE_CARRAY1 && symbasetype != TYPE_CARRAY2 && symbasetype != TYPE_CARRAY3 &&
						    	symtype != TYPE_CVARPTRARRAY1 && symtype != TYPE_CVARPTRARRAY2 && symtype != TYPE_CVARPTRARRAY3 &&
						    	symtype != TYPE_NVARPTRARRAY1 && symtype != TYPE_NVARPTRARRAY2 && symtype != TYPE_NVARPTRARRAY3)) {
							linecntsave = linecnt;
							whitespace();
							scantoken(SCAN_UPCASE);
							linecnt = linecntsave;
							if (tokentype == TOKEN_WORD) {
								if (strcmp((CHAR *) token, "AND") && strcmp((CHAR *) token, "OR")) {
									if (operator <= 8) goto change4;
									else goto change5;
								}
							}
							else {
								if (tokentype == TOKEN_LPAREND) {
									if (operator <= 8) goto change4;
									else goto change5;
								}
								tokentype = TOKEN_WORD;
							}
									
							
						}
					}
				}
			}
			else {
				if (!strcmp(uptoken, "AND")) {
					operator = OP_AND;
					goto change6;
				}
				else if (!strcmp(uptoken, "OR")) {
					operator = OP_OR;
					goto change6;
				}
				goto syntaxerror;
			}
			if (!symtype) {
				error(ERROR_VARNOTFOUND);
				return(0);
			}
			symbasetype = (UCHAR) (symtype & ~TYPE_PTR);
			if (symtype == TYPE_EQUATE) {
				astnode[current].numlit[0] = 0;
				astnode[current].value = symvalue;
				goto change1;
			}
			astnode[current].value = symvalue;
			if (symbasetype == TYPE_NVAR) goto change2;
			if (symbasetype == TYPE_CVAR) goto change3;
			astnode[current].ptr1 = nodecnt;
			astnode[nodecnt].upptr = current;
			astnode[current].ptr2 = astnode[current].ptr3 = 0xFF;
			if (++nodecnt == ASTNODEMAX) goto exptoobigerror;
			if (symbasetype == TYPE_NARRAY1 || symtype == TYPE_NVARPTRARRAY1) goto change2;
			if (symbasetype == TYPE_CARRAY1 || symtype == TYPE_CVARPTRARRAY1) goto change3;
			astnode[current].ptr2 = nodecnt;
			astnode[nodecnt].upptr = current;
			if (++nodecnt == ASTNODEMAX) goto exptoobigerror;
			if (symbasetype == TYPE_NARRAY2 || symtype == TYPE_NVARPTRARRAY2) goto change2;
			if (symbasetype == TYPE_CARRAY2 || symtype == TYPE_CVARPTRARRAY2) goto change3;
			astnode[current].ptr3 = nodecnt;
			astnode[nodecnt].upptr = current;
			if (++nodecnt == ASTNODEMAX) goto exptoobigerror;
			if (symbasetype == TYPE_NARRAY3 || symtype == TYPE_NVARPTRARRAY3) goto change2;
			if (symbasetype == TYPE_CARRAY3 || symtype == TYPE_CVARPTRARRAY3) goto change3;
			error(ERROR_BADVARTYPE);
			return(0);
		case TOKEN_NUMBER:
			if (!checkdcon(token) && (INT) strlen((CHAR *) token) < 10) {
				symvalue = atol((CHAR *) token);
				astnode[current].value = symvalue;
				if (symvalue >= 256) {
					strcpy(astnode[current].numlit, (CHAR *) token);
				}
				else astnode[current].numlit[0] = 0;
				goto change1;
			}
			astnode[current].value = makenlit((CHAR *) token);
			goto change2;
		case TOKEN_DBLQUOTE:
			scantoken(SCAN_NOUPCASE + SCAN_SCANLITERAL);
			astnode[current].value = makeclit(token);
			goto change3;
		case TOKEN_LBRACKET:
			if (state < 8) goto syntaxerror;
			state -= 7;
			statesave[nestlevel] = state;
			curelement[nestlevel] = 1;
			arraytop[nestlevel] = current;
			current = astnode[current].ptr1;
			astnode[current].ptr1 = 0xFF;
			neststack[nestlevel++] = 0;
			goto xparseloop;
		case TOKEN_RBRACKET:
			if (state >= 8) goto syntaxerror;
			if (!nestlevel) {
				if (state == 4 && echar == ']') goto done;
				goto syntaxerror;
			}
			if (state >= 1 && state <= 3) goto syntaxerror;
			state = statesave[--nestlevel] + 4;
			if (state != 4 || neststack[nestlevel])
				if (state != 5) goto syntaxerror;
			state = 4;
			current = arraytop[nestlevel];
			goto xparseloop;
		case TOKEN_LPAREND:
			if (state > 7) {
				curelement[nestlevel] = 1;
				arraytop[nestlevel] = current;
				state -= 7;
				statesave[nestlevel] = state;
				current = astnode[current].ptr1;
				astnode[current].ptr1 = 0xFF;
				neststack[nestlevel++] = 1;
			}
			else {
				if (state > 3) goto syntaxerror;
				parendlevel += 1;
				neststack[nestlevel++] = 2;
			}
			goto xparseloop;
		case TOKEN_RPAREND:
			if (!nestlevel) {
				if (state == 4 && echar == ')') goto done;
				goto syntaxerror;
			}	
			if (state >= 1 && state <= 3) goto syntaxerror;
			if (neststack[--nestlevel] == 1) {
				state = statesave[nestlevel] + 4;
				if (state == 5) {
					current = arraytop[nestlevel];
					state = 4;
					goto xparseloop;
				}
			}
			if (state > 3 && state < 8 && neststack[nestlevel] == 2) {
				parendlevel -= 1;
				goto xparseloop;
			}
			goto syntaxerror;
		case TOKEN_COMMA:
			if (state >= 1 && state <= 3) goto syntaxerror;
			if (nestlevel) state = statesave[nestlevel - 1] + 4;
			if (state == 6 || state == 7) {
				state -= 5;
				if (nestlevel) statesave[nestlevel - 1] = state;
				current = arraytop[nestlevel - 1];
				if (curelement[nestlevel - 1]++ == 1) current = astnode[current].ptr2;
				else current = astnode[current].ptr3;
				astnode[current].ptr1 = 0xFF;
				goto xparseloop;
			}
			if (echar == ',' && state == 4) goto done;
			goto syntaxerror;
		case TOKEN_PLUS:
			if (state < 4) goto xparseloop;
			if (state > 7) goto syntaxerror;
			tempnode = current;
			operator_prec = NODETYPE_OPERATOR + parendlevel * (HIGHEST_PRECEDENCE + 1) + 4;
			while (tempnode != topnode && operator_prec <= astnode[astnode[tempnode].upptr].type)
				tempnode = astnode[tempnode].upptr;
			if (astnode[tempnode].type == NODETYPE_CVARLIT ||
			    (astnode[tempnode].type >= NODETYPE_OPERATOR &&
			    (astnode[tempnode].value & OP_CHAR_*256))) operator = OP_CONCAT;
			else operator = OP_ADD;
			goto change6;
		case TOKEN_MINUS:
			if (state < 4) {
				operator = OP_NEGATE;
				goto change5;
			}
			if (state < 8) {
				operator = OP_SUBTRACT;
				goto change6;
			}
			goto syntaxerror;
		case TOKEN_NOTEQUAL:
			operator = OP_NOTEQUAL;
			goto change6;
		case TOKEN_BANG:
			if (line[linecnt] == '=') {
				linecnt++;
				operator = OP_NOTEQUAL;
			}
			else {
				tempnode = current;
				operator_prec = NODETYPE_OPERATOR + parendlevel * (HIGHEST_PRECEDENCE + 1) + 7;
				while (tempnode != topnode && operator_prec <= astnode[astnode[tempnode].upptr].type)
					tempnode = astnode[tempnode].upptr;
				if (astnode[tempnode].type == NODETYPE_CVARLIT ||
				    (astnode[tempnode].type >= NODETYPE_OPERATOR &&
				    (astnode[tempnode].value & OP_CHAR_*256))) operator = OP_CRESIZE;
				else operator = OP_NRESIZE;
			}
			goto change6;
		case TOKEN_PERCENT:
			operator = OP_MODULUS;
			goto change6;
		case TOKEN_AMPERSAND:
			operator = OP_AND;
			goto change6;
		case TOKEN_ASTERISK:
			if (line[linecnt] == '*') {
				linecnt++;
				operator = OP_POWER;
			}
			else operator = OP_MULTIPLY;
			goto change6;
		case TOKEN_DIVIDE:
			operator = OP_DIVIDE;
			goto change6;
		case TOKEN_COLON:
			if (dspflags & DSPFLAG_COMPILE) getlndsp();
			while (!getln()) {
				for (linecnt = 0; charmap[line[linecnt]] == 2; linecnt++);
				if (line[linecnt]) {
					if (linecnt) {
						if (echar == ',' && state == 4) goto done;
						else goto xparseloop;
					}
					break;
				}
				if (dspflags & DSPFLAG_COMPILE) getlndsp();
			}
			error(ERROR_BADCONTLINE);
			return(0);
		case TOKEN_NOTGREATER:
			operator = OP_LESSEQ;
			goto change6;
		case TOKEN_LESS:
			operator = OP_LESS;
			goto change6;
		case TOKEN_EQUAL:
			operator = OP_EQUAL;
			goto change6;
		case TOKEN_NOTLESS:
			operator = OP_GREATEREQ;
			goto change6;
		case TOKEN_GREATER:
			operator = OP_GREATER;
			goto change6;
		case TOKEN_ORBAR:
			operator = OP_OR;
			goto change6;
	}
syntaxerror:
	error(ERROR_SYNTAX);
	return(0);

exptoobigerror:
	error(ERROR_EXPTOOBIG);
	return(0);

change1:  /* change state after finding numeric constant value */
	astnode[current].type = NODETYPE_NUMCONST;
	if (state > 0x07) goto syntaxerror;
	state += 4;
	goto xparseloop;

change2:  /* change state after finding numeric variable or literal */
	astnode[current].type = NODETYPE_NVARLIT;
	if (state > 0x07) goto syntaxerror;
	if (symbasetype == TYPE_NARRAY1 || symtype == TYPE_NVARPTRARRAY1) state = 8;
	else if (symbasetype == TYPE_NARRAY2 || symtype == TYPE_NVARPTRARRAY2) state = 9;
	else if (symbasetype == TYPE_NARRAY3 || symtype == TYPE_NVARPTRARRAY3) state = 10;
	else state += 4;
	goto xparseloop;

change3:  /* change state after finding character variable or literal */
	astnode[current].type = NODETYPE_CVARLIT;
	if (state > 0x07) goto syntaxerror;
	if (tokentype == TOKEN_WORD) {
		if (symbasetype == TYPE_CARRAY1 || symtype == TYPE_CVARPTRARRAY1) state = 8;
		else if (symbasetype == TYPE_CARRAY2 || symtype == TYPE_CVARPTRARRAY2) state = 9;
		else if (symbasetype == TYPE_CARRAY3 || symtype == TYPE_CVARPTRARRAY3) state = 10;
		else state += 4;
	}
	else state += 4;
	goto xparseloop;

change4:  /* change state after finding flag */
	astnode[current].type = NODETYPE_FLAG;
	astnode[current].value = operator;
	if (state > 0x07) goto syntaxerror;
	state += 4;
	goto xparseloop;

change5:  /* don't change state after finding unary operator */
	if (state > 3) goto syntaxerror;
	if (operator == OP_NOT) astnode[current].type = NODETYPE_OPERATOR + parendlevel * (HIGHEST_PRECEDENCE + 1) + 2;
	else astnode[current].type = NODETYPE_OPERATOR + parendlevel * (HIGHEST_PRECEDENCE + 1) + 8;
	astnode[current].value = operator;
	astnode[current].ptr1 = nodecnt;
	astnode[current].ptr2 = 0xFF;
	astnode[nodecnt].upptr = current;
	current = nodecnt++;
	if (nodecnt == ASTNODEMAX) goto exptoobigerror;
	astnode[current].ptr1 = 0xFF;
	goto xparseloop;

change6:  /* change state after finding binary operator */
	if (state < 4 || state > 7) goto syntaxerror;
	state -= 4;
	switch (operator) {
		case OP_CRESIZE:
		case OP_NRESIZE:
			operator_prec = 7; break;
		case OP_POWER:
			operator_prec = 6; break;
		case OP_MULTIPLY:
		case OP_DIVIDE:
		case OP_MODULUS:
			operator_prec = 5; break;
		case OP_CONCAT:
		case OP_ADD:
		case OP_SUBTRACT:
			operator_prec = 4; break;
		case OP_EQUAL:
		case OP_NOTEQUAL:
		case OP_LESS:
		case OP_GREATER:
		case OP_LESSEQ:
		case OP_GREATEREQ:
			operator_prec = 3; break;
		case OP_AND:
			operator_prec = 1; break;
		case OP_OR:
			operator_prec = 0;
	}
	operator_prec = NODETYPE_OPERATOR + parendlevel * (HIGHEST_PRECEDENCE + 1) + operator_prec;
	while (current != topnode && (operator_prec <= astnode[astnode[current].upptr].type)) current = astnode[current].upptr;
	if (current == topnode) {
		if (operator == OP_POWER && astnode[current].value == OP_POWER) {
			current = nodecnt++;
			if (nodecnt == ASTNODEMAX) goto exptoobigerror;
			astnode[astnode[topnode].ptr2].upptr = current;
			astnode[current].ptr1 = astnode[topnode].ptr2;
			astnode[current].upptr = topnode;
			astnode[topnode].ptr2 = current;
		}
		else {
			astnode[topnode].upptr = nodecnt;
			astnode[nodecnt].ptr1 = topnode;
			astnode[nodecnt].upptr = 0xFF;
			current = topnode = nodecnt++;
			if (nodecnt == ASTNODEMAX) goto exptoobigerror;
		}
	}
	else {
		i = astnode[current].upptr;
		if (astnode[i].ptr1 == current) astnode[i].ptr1 = nodecnt;
		else if (astnode[i].ptr2 == current) astnode[i].ptr2 = nodecnt;
		else if (astnode[i].ptr3 == current) astnode[i].ptr3 = nodecnt;
		astnode[nodecnt].upptr = i;
		astnode[nodecnt].ptr1 = current;
		astnode[current].upptr = nodecnt;
		current = nodecnt++;
		if (nodecnt == ASTNODEMAX) goto exptoobigerror;
	}
	astnode[current].type = operator_prec;
	astnode[current].value = operator;
	astnode[current].ptr2 = nodecnt;
	astnode[current].ptr3 = 0xFF;
	astnode[nodecnt].upptr = current;
	astnode[nodecnt].ptr1 = 0xFF;
	current = nodecnt++;
	goto xparseloop;

/* type checking and code generation */
done:
	if (parendlevel || nestlevel || !nodecnt) goto syntaxerror;
	if (astnode[topnode].type == NODETYPE_CVARLIT ||
	    (astnode[topnode].type >= NODETYPE_OPERATOR &&
		(astnode[topnode].value & OP_CHAR_*256))) return(2);     /* character expression */
	return(1);	 /* numeric expression (arithmetic or logical) */
}

static INT xoptast(UINT *highnode)		/* optimize abstract syntax tree */
/* This routine will optimize an abstract syntax tree generated by xparse() */
/* by pushing NOTs as far down the tree as possible (preferably immediately */
/* above the AST leaves).  This will be implemented by doing a depth-first  */
/* search of the abstract syntax tree recording any logical operands to be	 */
/* negated (via De Morgan's logic laws, i.e. not (a && b) = not a || not b) */
/* and actually implementing the logical negation on the way back up the    */
/* tree.  Note: All existing NOTs will be removed and replaced with new 	 */
/* ones at the leaf area of the tree.  Also, this optimization if for 	 */
/* expressions that will generate goto if code (i.e. IF, ELSE IF, BREAK,	 */
/* WHILE, etc.) not numeric/character expressions used in MOVE like 		 */
/* statements.  This is because the NOTs can be consumed by reversing the   */
/* goto if logic.												 */
{
	UINT curnode, lastnode, notflag, travupflag;

	curnode = *highnode;
	notflag = FALSE;
	astnode[curnode].curptr = 0;	/* look down leftmost branch first */
	travupflag = FALSE;
	do {
		if (!travupflag) {	/* last movement was downward in AST */
			astnode[curnode].flag = notflag;
		}
		else {			/* last movement was upward in AST */
			notflag = astnode[curnode].flag;
			travupflag = FALSE;
		}
		if (astnode[curnode].type >= NODETYPE_OPERATOR) {
			if (astnode[curnode].value == OP_NOT) notflag = !notflag;
			else if (astnode[curnode].value != OP_AND &&
				    astnode[curnode].value != OP_OR) notflag = FALSE;
		}
		else notflag = FALSE;
		astnode[curnode].curptr++;	/* move down next branch */
		lastnode = curnode;
		switch (astnode[curnode].curptr) {
			case 1:
				curnode = astnode[curnode].ptr1;
				break;
			case 2:
				curnode = astnode[curnode].ptr2;
				break;
			case 3:
				curnode = astnode[curnode].ptr3;
				break;
			default:
				curnode = 0xFF;
		}
		if (curnode == 0xFF) { /* continue back up tree */
			travupflag = TRUE;	
			curnode = astnode[lastnode].upptr;
			if (astnode[lastnode].type >= NODETYPE_OPERATOR && 
			    astnode[lastnode].value == OP_NOT) { /* remove all NOTs encountered */
				astnode[astnode[lastnode].ptr1].upptr = curnode;
				if (curnode != 0xFF) {
					if (astnode[curnode].ptr1 == lastnode) astnode[curnode].ptr1 = astnode[lastnode].ptr1;
					else if (astnode[curnode].ptr2 == lastnode) astnode[curnode].ptr2 = astnode[lastnode].ptr1;
					else if (astnode[curnode].ptr3 == lastnode) astnode[curnode].ptr3 = astnode[lastnode].ptr1;
				}
				else *highnode = astnode[lastnode].ptr1;
			}
			else if (astnode[lastnode].flag) { /* logically reverse this node */					
				if (astnode[lastnode].type >= NODETYPE_OPERATOR && 
				    astnode[lastnode].value == OP_OR) astnode[lastnode].value = OP_AND;
				else if (astnode[lastnode].type >= NODETYPE_OPERATOR && 
					    astnode[lastnode].value == OP_AND) astnode[lastnode].value = OP_OR;
				else {
					astnode[nodecnt].upptr = curnode;
					astnode[nodecnt].type = NODETYPE_OPERATOR;
					astnode[nodecnt].ptr1 = lastnode;
					astnode[nodecnt].ptr2 = 0xFF;
					astnode[nodecnt].value = OP_NOT;
					astnode[lastnode].upptr = nodecnt;
					if (curnode == 0xFF) *highnode = nodecnt;
					else {
						if (astnode[curnode].ptr1 == lastnode) astnode[curnode].ptr1 = nodecnt;
						else if (astnode[curnode].ptr2 == lastnode) astnode[curnode].ptr2 = nodecnt;
						else if (astnode[curnode].ptr3 == lastnode) astnode[curnode].ptr3 = nodecnt;
					}
					if (++nodecnt == ASTNODEMAX) {
						error(ERROR_EXPTOOBIG);
						return(FALSE);
					}
				}
			}
			notflag = FALSE;
		}
		else {			   /* traverse down next branch */	
			astnode[curnode].curptr = 0;	
			astnode[curnode].flag = FALSE;
			continue;
		}
	} while (curnode != 0xFF);
	return(TRUE);	
}

/**
 * highnode: node in abstract syntax tree to start generating from
 * This routine traverses the abstract syntax tree from highnode downward
 * in a depth first fashion in order to generate code for numerical,
 * character and logical expressions (another form of numerical).  Code
 * is generated only when traversing back up the tree and a operator AST
 * node is left.  The temporary variable destination of the the unary or
 * binary operation is stored in that operator AST node as it becomes
 * a variable AST node.
 */
static INT xgenexpcode(UINT highnode)	/* generate expression code */
{
	UINT curnode, lastnode, treeupptr;
	SHORT flagtype;	/* EOS, EQUAL, LESS, OVER, etc. */

	curnode = highnode;
	treeupptr = astnode[curnode].upptr;
	astnode[curnode].curptr = 0;		/* look down leftmost branch first */
	do {
		astnode[curnode].curptr++;	/* move down next branch */
		lastnode = curnode;	
		switch (astnode[curnode].curptr) {
			case 1:
				curnode = astnode[curnode].ptr1;
				break;
			case 2:
				curnode = astnode[curnode].ptr2;
				break;
			case 3:
				curnode = astnode[curnode].ptr3;
				break;
			default:
				curnode = 0xFF;
		}
		if (curnode == 0xFF) { /* continue back up tree */
			curnode = astnode[lastnode].upptr;
			if (astnode[lastnode].type >= NODETYPE_OPERATOR) {
				if (!xgencode(lastnode)) return(FALSE);
			}
			else if (astnode[lastnode].type == NODETYPE_FLAG) {
				flagtype = (SHORT) astnode[lastnode].value;
				while (curnode != treeupptr) {
					if (astnode[curnode].type < NODETYPE_OPERATOR || 
					    astnode[curnode].value != OP_NOT) break;
					if (flagtype < 4) flagtype += 4;
					else if (flagtype < 8) flagtype -= 4;
					else if (flagtype == 8) flagtype = 9;
				 	else flagtype = 8;
					lastnode = curnode;
					curnode = astnode[curnode].upptr;
				}
				putmainhhll((USHORT) (88 * 256 + 71 + (UCHAR) (flagtype)));
				astnode[lastnode].type = NODETYPE_NVARLIT;
				astnode[lastnode].value = xtempvar(TEMPTYPE_INT);
				astnode[lastnode].ptr1 = 0xFF;
				xgenmainadr(lastnode);
			}
		}
		else {			   /* traverse down next branch */	
			astnode[curnode].curptr = 0;	
			continue;
		}
	} while (curnode != treeupptr);
	return(TRUE);
}

/* curnode: AST operator node (either binary or unary)
 * generate expression code
 * This routine uses the AST node curnode to generate code for a binary
 * or unary operation retrieving the operand variables or literals from
 * immediately below.
 */
static INT xgencode(UINT curnode)
{
	USHORT nodetype1, nodetype2;
	INT lvalue, rvalue;
	INT tempvartype = -1;
	UCHAR savvalue, optype, operands;

	nodetype1 = astnode[astnode[curnode].ptr1].type;
	operands = (UCHAR) (astnode[curnode].value >> 8);
	savvalue = optype = (UCHAR) astnode[curnode].value;
	if (nodetype1 >= NODETYPE_OPERATOR) {
		if (astnode[astnode[curnode].ptr1].value & OP_NUM_) nodetype1 = NODETYPE_NVARLIT;
		else nodetype1 = NODETYPE_CVARLIT;
	}
	if (operands & OP_BINARY_) {
		nodetype2 = astnode[astnode[curnode].ptr2].type;
		if (nodetype2 >= NODETYPE_OPERATOR) {
			if (astnode[astnode[curnode].ptr2].value & OP_NUM_) nodetype2 = NODETYPE_NVARLIT;
			else nodetype2 = NODETYPE_CVARLIT;
		}
	}
	else nodetype2 = 0;
	if (optype == OP_CRESIZE % 256 && (operands & OP_CHAR_)) tempvartype = TEMPTYPE_DIM256;
	else {
		switch (optype) {
			case OP_SIN % 256:
			case OP_COS % 256:
			case OP_TAN % 256:
			case OP_ARCSIN % 256:
			case OP_ARCCOS % 256:
			case OP_ARCTAN % 256:
			case OP_EXP % 256:
			case OP_LOG % 256:
			case OP_LOG10 % 256:
			case OP_SQRT % 256:
				putmain1(XPREFIX);
				tempvartype = TEMPTYPE_FLOAT;
				break;
			case OP_FCHAR % 256:
			case OP_LCHAR % 256:
				tempvartype = TEMPTYPE_DIM1;
				break;
			case OP_SQUEEZE % 256:
			case OP_CHOP % 256:
			case OP_TRIM % 256:
				putmain1(XPREFIX);
				tempvartype = TEMPTYPE_DIM256;
				break;
			case OP_CONCAT % 256:
				tempvartype = TEMPTYPE_DIM256;
				break;
			case OP_ADD % 256:
			case OP_SUBTRACT % 256:
			case OP_MULTIPLY % 256:
			case OP_DIVIDE % 256:
			case OP_MODULUS % 256:
			case OP_NRESIZE % 256:
			case OP_NEGATE % 256:
				tempvartype = TEMPTYPE_ALTFORM31;
				break;
			case OP_POWER % 256:
			case OP_ABS % 256:
				putmain1(XPREFIX);
				tempvartype = TEMPTYPE_ALTFORM31;
				break;
			case OP_AND % 256:
			case OP_OR % 256:
			case OP_NOT % 256:
				tempvartype = TEMPTYPE_INT;
				break;
			case OP_SIZE % 256:
			case OP_LENGTH % 256:
			case OP_ISNULL % 256:
				putmain1(XPREFIX);
				putmainhhll((UINT) 88 * (UINT) 256 + optype);
				tempvartype = TEMPTYPE_INT;
				break;
			case OP_FORMPTR % 256:
				optype = 39;
				putmainhhll((UINT) XPREFIX * (UINT) 256 + optype);
				tempvartype = TEMPTYPE_INT;
				break;
			case OP_LENGTHPTR % 256:
				optype = 40;
				putmainhhll((UINT) XPREFIX * (UINT) 256 + optype);
				tempvartype = TEMPTYPE_INT;
				break;
			case OP_INT % 256:
				optype = 217;
				tempvartype = TEMPTYPE_INT;
				break;
			case OP_FORM % 256:
				optype = 216;
				tempvartype = TEMPTYPE_FORM31;
				break;
			case OP_FLOAT % 256:
				optype = 218;
				tempvartype = TEMPTYPE_FLOAT;
				break;
			case OP_CHAR % 256:
				optype = 215;
				tempvartype = TEMPTYPE_DIM256;
				break;
			case OP_EQUAL % 256:
			case OP_NOTEQUAL % 256:
			case OP_LESS % 256:
			case OP_GREATER % 256:
			case OP_LESSEQ % 256:
			case OP_GREATEREQ % 256:
				tempvartype = TEMPTYPE_INT;
				break;
			default:
				error(ERROR_INTERNAL);
				return FALSE;
		}
	}
	if ((operands & OP_COPS_) && (!(operands & OP_NOPS_))) {
		if (nodetype1 <= NODETYPE_NVARLIT ||
		    (nodetype2 && nodetype2 <= NODETYPE_NVARLIT)) {
			error(ERROR_BADEXPOP);
			return(FALSE);
		}
	}
	else if ((operands & OP_NOPS_) && (!(operands & OP_COPS_))) {
		if (nodetype1 >= NODETYPE_CVARLIT ||
		    (nodetype2 && nodetype2 >= NODETYPE_CVARLIT)) {
			error(ERROR_BADEXPOP);
			return(FALSE);
		}
	}
	else if (operands & OP_SAMEOPS_) {
		if ((nodetype1 == NODETYPE_CVARLIT && nodetype2 != NODETYPE_CVARLIT) ||
		    (nodetype2 == NODETYPE_CVARLIT && nodetype1 != NODETYPE_CVARLIT)) {
			error(ERROR_BADEXPOP);
			return(FALSE);
		}
	}
	else if (operands & OP_COPNOP_) {
		if (nodetype1 <= NODETYPE_NVARLIT || nodetype2 >= NODETYPE_CVARLIT) {
			error(ERROR_BADEXPOP);
			return(FALSE);
		}
	}
	if ((savvalue >= (UCHAR) OP_POWER && savvalue <= (UCHAR) OP_MODULUS) ||
	    (savvalue >= (UCHAR) OP_EQUAL && savvalue <= (UCHAR) OP_GREATEREQ))
	{
		if ((operands & OP_SAMEOPS_) && nodetype1 == NODETYPE_CVARLIT &&
		    nodetype2 == NODETYPE_CVARLIT) {
			putmainhhll((UINT) (88 * 256 + optype));
			xgenmainadr(astnode[curnode].ptr1);
			xgenmainadr(astnode[curnode].ptr2);
		}
		else {	
			if (constantfoldflg && 
			    nodetype1 == NODETYPE_NUMCONST &&
			    nodetype2 == NODETYPE_NUMCONST &&
			    savvalue != (UCHAR) OP_POWER) {
				/* optimization: fold numeric constants */
				lvalue = astnode[astnode[curnode].ptr1].value;
				rvalue = astnode[astnode[curnode].ptr2].value;
				switch (savvalue) {
					case OP_ADD % 256:
						astnode[curnode].value = lvalue + rvalue;
						break;
					case OP_SUBTRACT % 256:
						astnode[curnode].value = lvalue - rvalue;
						break;
					case OP_MULTIPLY % 256:
						astnode[curnode].value = lvalue * rvalue;
						break;
					case OP_DIVIDE % 256:
						astnode[curnode].value = lvalue / rvalue;
						break;
					case OP_MODULUS % 256:
						astnode[curnode].value = lvalue % rvalue;
						break;
 					case OP_EQUAL % 256:
						astnode[curnode].value = lvalue == rvalue;
						break;
					case OP_NOTEQUAL % 256:
						astnode[curnode].value = lvalue != rvalue;
						break;
					case OP_LESS % 256:
						astnode[curnode].value = lvalue < rvalue;
						break;
					case OP_GREATER % 256:
						astnode[curnode].value = lvalue > rvalue;
						break;
					case OP_LESSEQ % 256:
						astnode[curnode].value = lvalue <= rvalue;
						break;
					case OP_GREATEREQ % 256:
						astnode[curnode].value = lvalue >= rvalue;
						break;
				}
				astnode[curnode].type = NODETYPE_NUMCONST;
				astnode[curnode].numlit[0] = 0;
				astnode[curnode].ptr1 = 0xFF;
				return(TRUE);
			}
			putmainhhll((UINT) (88 * 256 + optype));
			xgenmainadr(astnode[curnode].ptr2);
			xgenmainadr(astnode[curnode].ptr1);
		}
	}
	else {
		if (optype != OP_SIZE % 256 && 
		    optype != OP_LENGTH % 256 &&
		    optype != OP_ISNULL % 256) putmainhhll((UINT) (88 * 256 + optype));
		xgenmainadr(astnode[curnode].ptr1);
		if (operands & OP_BINARY_) xgenmainadr(astnode[curnode].ptr2);
	}
	if (operands & OP_NUM_) astnode[curnode].type = NODETYPE_NVARLIT;
	else astnode[curnode].type = NODETYPE_CVARLIT;
	astnode[curnode].ptr1 = 0xFF;
	astnode[curnode].value = xtempvar(tempvartype);
	xgenmainadr(curnode);	/* generate code for temporary */
	return(TRUE);
}

/* generate gotoif expression code (for IF/THEN, LOOP/REPEAT, etc. type structures) */
/* highnode   node in abstract syntax tree to start generating from */
/* dropflg    set to TRUE if drop through code is in SUCCESS condition */
/*            set to FALSE if drop through code is in FAILURE condition */
/* nondroplbl non drop-through label */
static INT xgengotoifcode(UINT highnode, UINT dropflg, UINT nondroplbl)
{
	USHORT nodetype1 = 0, nodetype2;
	UINT droplbl;
	UCHAR operands;
	UINT curnode, lastnode, revflg, andflg, optawayflg;
	INT lvalue, rvalue;
	
	curnode = highnode;
	astnode[curnode].grpnum = 0;		/* keep count of ANDs in AND group */
								/* ORs in OR group, start at zero  */
	astnode[curnode].curptr = 0;		/* look down leftmost branch first */
	astnode[curnode].flag = FALSE;
	if (dropflg) {	/* drop through is success */
		droplbl = astnode[curnode].success = getlabelnum();
		astnode[curnode].failure = nondroplbl;
		revflg = TRUE;
		if (astnode[curnode].type >= NODETYPE_OPERATOR &&
		    astnode[curnode].value == OP_OR) {
			andflg = FALSE;
			astnode[curnode].flag = TRUE;
		}
		else andflg = TRUE;
	}
	else {		/* drop through is failure */
		astnode[curnode].success = nondroplbl;
		droplbl = astnode[curnode].failure = getlabelnum();
		revflg = FALSE;
		if (astnode[curnode].type >= NODETYPE_OPERATOR &&
		    astnode[curnode].value == OP_AND) {
			andflg = TRUE;
			astnode[curnode].flag = TRUE;
		}
		else andflg = FALSE;
	}

	while (curnode != 0xFF) {
		if (astnode[curnode].type >= NODETYPE_OPERATOR &&
		    astnode[curnode].value == OP_AND) andflg = revflg = TRUE;
		else if (astnode[curnode].type >= NODETYPE_OPERATOR &&
		    astnode[curnode].value == OP_OR) andflg = revflg = FALSE;
		else if (astnode[curnode].type >= NODETYPE_OPERATOR &&
			    astnode[curnode].value == OP_NOT) { /* remove NOT, reverse GOTO */
			lastnode = astnode[curnode].upptr;
			astnode[astnode[curnode].ptr1].upptr = lastnode;
			astnode[astnode[curnode].ptr1].success = astnode[curnode].success;
			astnode[astnode[curnode].ptr1].failure = astnode[curnode].failure;
			if (lastnode != 0xFF) {
				if (astnode[lastnode].ptr1 == curnode) {
					astnode[lastnode].ptr1 = astnode[curnode].ptr1;
					astnode[astnode[curnode].ptr1].flag = FALSE;
				}
				else if (astnode[lastnode].ptr2 == curnode) {
					astnode[lastnode].ptr2 = astnode[curnode].ptr1;
					astnode[astnode[curnode].ptr1].flag = astnode[lastnode].flag;
				}
			}
			else {
				//highnode = astnode[curnode].ptr1;
				astnode[astnode[curnode].ptr1].flag = FALSE;
			}
			curnode = astnode[curnode].ptr1;
			revflg = !revflg;
			continue;
		}
		else {		/* comparison operator, literal or variable */
			optawayflg = FALSE;		/* assume this _GOTO will be generated */
			if (astnode[curnode].flag) revflg = !revflg;
			if (astnode[curnode].type >= NODETYPE_OPERATOR) {
				operands = (UCHAR) (astnode[curnode].value >> 8);
				if (operands & OP_BINARY_) {
					if (!xgenexpcode(astnode[curnode].ptr1)) return(FALSE);
					if (!xgenexpcode(astnode[curnode].ptr2)) return(FALSE);
					nodetype1 = astnode[astnode[curnode].ptr1].type;
					nodetype2 = astnode[astnode[curnode].ptr2].type;
					if (operands & OP_SAMEOPS_) {
						if ((nodetype1 == NODETYPE_CVARLIT && nodetype2 != NODETYPE_CVARLIT) ||
						    (nodetype2 == NODETYPE_CVARLIT && nodetype1 != NODETYPE_CVARLIT)) {
							error(ERROR_BADEXPOP);
							return(FALSE);
						}
					}
				}
				if (astnode[curnode].value >= OP_EQUAL
					&& astnode[curnode].value <= OP_GREATEREQ
					&& nodetype1 == NODETYPE_NUMCONST
					&& nodetype2 == NODETYPE_NUMCONST)
				{
					/* OPTIMIZATION: constant folding */
					lvalue = astnode[astnode[curnode].ptr1].value;
					rvalue = astnode[astnode[curnode].ptr2].value;
					switch (astnode[curnode].value) {
						case OP_EQUAL:
							revflg = !revflg;
							// fall through
							/* no break */
						case OP_NOTEQUAL:
							if (revflg) {
								if (lvalue == rvalue) putcode1(201);
								else optawayflg = TRUE;
							}
							else {
								if (lvalue != rvalue) putcode1(201);
								else optawayflg = TRUE;
							}
							break;
						case OP_LESS:
							revflg = !revflg;
							// fall through
							/* no break */
						case OP_GREATEREQ:
							if (revflg) {
								if (lvalue < rvalue) putcode1(201);
								else optawayflg = TRUE;
							}
							else {
								if (lvalue >= rvalue) putcode1(201);
								else optawayflg = TRUE;
							}
							break;
						case OP_GREATER:
							revflg = !revflg;
							// fall through
							/* no break */
						case OP_LESSEQ:
							if (revflg) {
								if (lvalue > rvalue) putcode1(201);
								else optawayflg = TRUE;
							}
							else {
								if (lvalue <= rvalue) putcode1(201);
								else optawayflg = TRUE;
							}
							break;
					}
				}
				else {
					switch (astnode[curnode].value) {
						case OP_EQUAL:
							revflg = !revflg;
							// fall through
							/* no break */
						case OP_NOTEQUAL:
							if (revflg) putcode1(195);
							else putcode1(196);
							xgencodeadr(astnode[curnode].ptr1);
							xgencodeadr(astnode[curnode].ptr2);
							break;
						case OP_LESS:
							revflg = !revflg;
							// fall through
							/* no break */
						case OP_GREATEREQ:
							if (revflg) putcode1(197);
							else putcode1(200);
							xgencodeadr(astnode[curnode].ptr1);
							xgencodeadr(astnode[curnode].ptr2);
							break;
						case OP_GREATER:
							revflg = !revflg;
							// fall through
							/* no break */
						case OP_LESSEQ:
							if (revflg) putcode1(198);
							else putcode1(199);
							xgencodeadr(astnode[curnode].ptr1);
							xgencodeadr(astnode[curnode].ptr2);
							break;
						default:
							if (revflg) putcode1(194);
							else putcode1(193);
							if (!xgenexpcode(curnode)) return(FALSE);
							xgencodeadr(curnode);
					}
				}
			}
			else {	/* literal or variable */
				if (!xgenexpcode(curnode)) return(FALSE);
				if (astnode[curnode].type == NODETYPE_NUMCONST) {
					/* OPTIMIZATION: constant folding */
					lvalue = astnode[curnode].value;
					if (revflg) {
						if (!lvalue) putcode1(201);
						else optawayflg = TRUE;
					}
					else {
						if (lvalue) putcode1(201);
						else optawayflg = TRUE;
					}
				}
				else {
					if (revflg) putcode1(194);
					else putcode1(193);
					xgencodeadr(curnode);
				}
			}
			putcodeout();
			if (!optawayflg) {
				if (andflg) {		/* generating AND code */
					if (astnode[curnode].flag) {
						makepoffset(astnode[curnode].success);
					}
					else makepoffset(astnode[curnode].failure);
				}
				else {			/* generating OR code */
					if (astnode[curnode].flag) {
						makepoffset(astnode[curnode].failure);
					}
					else makepoffset(astnode[curnode].success);
				}
				putcodeout();
			}
			curnode = astnode[curnode].upptr;
			continue;
		}
		astnode[curnode].curptr++;	/* move down next branch */
		lastnode = curnode;	
		switch (astnode[curnode].curptr) {
			case 1:	/* traverse down to the left */
				curnode = astnode[curnode].ptr1;
				astnode[curnode].flag = FALSE;
				break;
			case 2:	/* traverse down to the right */
				curnode = astnode[lastnode].ptr2;
				astnode[curnode].flag = astnode[lastnode].flag;
				break;
			default:	/* traverse back up */
				curnode = astnode[curnode].upptr;
				if (curnode != 0xFF) {	
					if ((astnode[lastnode].type >= NODETYPE_OPERATOR &&
					    astnode[curnode].type >= NODETYPE_OPERATOR &&
					    astnode[curnode].curptr == 1) || astnode[curnode].grpnum)
					{
			    		if (astnode[lastnode].value == OP_AND &&
			    		 	    astnode[curnode].value == OP_OR)
			    		{
							deflabelnum(astnode[lastnode].failure);	
						}
						else if (astnode[lastnode].value == OP_OR &&
			    		 	         astnode[curnode].value == OP_AND)
						{
							deflabelnum(astnode[lastnode].success);	
						}
					}
				}
				continue;
		}
		if (astnode[curnode].type >= NODETYPE_OPERATOR &&
		    astnode[curnode].value == OP_AND) {		/* AND group */
		    	if (astnode[lastnode].type >= NODETYPE_OPERATOR &&
			    astnode[lastnode].value == OP_OR) { /* AND group within OR group */
	     		if (astnode[lastnode].curptr == 1 || astnode[lastnode].grpnum) {
					astnode[curnode].failure = getlabelnum();
					astnode[curnode].flag = TRUE;
				}
				else {	/* rightmost AND group within OR group */
					astnode[curnode].failure = astnode[lastnode].failure;
					astnode[curnode].flag = !astnode[lastnode].flag;
				}
				astnode[curnode].grpnum = 0;
			}
			else {	/* AND node part of existing AND group */
				astnode[curnode].failure = astnode[lastnode].failure;
	     		if (astnode[lastnode].curptr == 1) {
					astnode[curnode].flag = FALSE;
					astnode[curnode].grpnum = astnode[lastnode].grpnum + 1;
				}
				else {
					astnode[curnode].flag = astnode[lastnode].flag;
					astnode[curnode].grpnum = astnode[lastnode].grpnum;
				}
			}
			astnode[curnode].success = astnode[lastnode].success;
		}
		else if (astnode[curnode].type >= NODETYPE_OPERATOR &&
			    astnode[curnode].value == OP_OR) {	/* OR group */
		    	if (astnode[lastnode].type >= NODETYPE_OPERATOR &&
			    astnode[lastnode].value == OP_AND) { /* OR group within an AND group */
               	if (astnode[lastnode].curptr == 1 || astnode[lastnode].grpnum) {
					astnode[curnode].success = getlabelnum();
					astnode[curnode].flag = TRUE;
				}
				else {	/* rightmost OR group within an AND group */
					astnode[curnode].success = astnode[lastnode].success;
					astnode[curnode].flag = !astnode[lastnode].flag;
				}
				astnode[curnode].grpnum = 0;
			}
			else {	/* OR node part of existing OR group */
				astnode[curnode].success = astnode[lastnode].success;
	     		if (astnode[lastnode].curptr == 1) {
					astnode[curnode].flag = FALSE;
					astnode[curnode].grpnum = astnode[lastnode].grpnum + 1;
				}
				else {
					astnode[curnode].flag = astnode[lastnode].flag;
					astnode[curnode].grpnum = astnode[lastnode].grpnum;
				}
			}
			astnode[curnode].failure = astnode[lastnode].failure;
		}
		else {
			astnode[curnode].success = astnode[lastnode].success;
			astnode[curnode].failure = astnode[lastnode].failure;
		}
		astnode[curnode].curptr = 0;	
	}
	putcodeout();
	deflabelnum(droplbl);		/* define drop label */
	return(TRUE);
}

static void xgenmainadr(UINT v)  /* generate a variable reference in the main code area */
{
	UCHAR i, j;

	i = (UCHAR) v;
xgenmainadrloop:
	if (astnode[i].type == NODETYPE_NUMCONST) {
/*** REMOVED BELOW CODE BECAUSE IT FAILS WITH DRAW. TRIED TO USE DRAWFLAG, ***/
/*** BUT THIS CAUSED ARRAY REFERENCES TO FAIL. WILL DEAL WITH IN V11.0 ***/
#if 0
		if (astnode[i].value >= 0 && astnode[i].value < 256) {
			putmainhhll(0xFF00 + (UCHAR) astnode[i].value);
		}
		else {
#endif
			if (!astnode[i].numlit[0]) {
				astnode[i].value = vmakenlit(astnode[i].value);
			}
			else {
				astnode[i].value = makenlit(astnode[i].numlit);
			}
			astnode[i].type = NODETYPE_NVARLIT;
			putmainadr(astnode[i].value);
#if 0
		}
#endif
	}
	else {
		if (astnode[i].ptr1 != 0xFF) {
			putmain1(0xC0);
			putmainadr(astnode[i].value);
			i = astnode[i].ptr1;
			goto xgenmainadrloop;
		}
		putmainadr(astnode[i].value);
	}
	while (i != v) {
		j = i;
		i = astnode[j].upptr;
		if (j == astnode[i].ptr1) {
			if (astnode[i].ptr2 != 0xFF) {
				i = astnode[i].ptr2;
				goto xgenmainadrloop;
			}
		}
		else if (j == astnode[i].ptr2) {
			if (astnode[i].ptr3 != 0xFF) {
				i = astnode[i].ptr3;
				goto xgenmainadrloop;
			}
		}
	}
}

static void xgencodeadr(UINT v)  /* generate a var ref in the secondary code buffer */
{
	UCHAR i, j;

	i = (UCHAR) v;
xgencodeadrloop:
	if (astnode[i].type == NODETYPE_NUMCONST) {
/*** REMOVED BELOW CODE BECAUSE IT FAILS WITH DRAW. TRIED TO USE DRAWFLAG, ***/
/*** BUT THIS CAUSED ARRAY REFERENCES TO FAIL. WILL DEAL WITH IN V11.0 ***/
#if 0
		if (astnode[i].value >= 0 && astnode[i].value < 256) {
			putcodehhll(0xFF00 + (UCHAR) astnode[i].value);
		}
		else {
#endif
			if (!astnode[i].numlit[0]) {
				astnode[i].value = vmakenlit(astnode[i].value);
			}
			else {
				astnode[i].value = makenlit(astnode[i].numlit);
			}
			astnode[i].type = NODETYPE_NVARLIT;
			putcodeadr(astnode[i].value);
#if 0
		}
#endif
	}
	else {
		if (astnode[i].ptr1 != 0xFF) {
			putcode1(0xC0);
			putcodeadr(astnode[i].value);
			i = astnode[i].ptr1;
			goto xgencodeadrloop;
		}
		putcodeadr(astnode[i].value);
	}
	while (i != v) {
		j = i;
		i = astnode[j].upptr;
		if (j == astnode[i].ptr1) {
			if (astnode[i].ptr2 != 0xFF) {
				i = astnode[i].ptr2;
				goto xgencodeadrloop;
			}
		}
		else if (j == astnode[i].ptr2) {
			if (astnode[i].ptr3 != 0xFF) {
				i = astnode[i].ptr3;
				goto xgencodeadrloop;
			}
		}
	}
}

void xtempvarreset()	/* reset the temporary variable management */
{
	memset(current, 0, 7);
	memset(first, 0, 7);
	memset(links, 0, MAXTEMP);
	highuse = 0;
}

/**
 * Return the address of a temporary variable
 * NOTE the values for global tmpvarflg are as follows:
 *	tmpvarflg = 1 : free all reusable temporary variables
 *	tmpvarflg = 2 : allocate a new reusable temporary variable or reuse next temporary
 *	tmpvarflg = 3 : generate a unique temporary variable that is not reused
 */
INT xtempvar(INT type)
{
	UCHAR n;
	INT adr;

	if (tmpvarflg == 1) {
		memcpy(current, first, 6);  /* free all temp variables */
		tmpvarflg++;
	}
	if (tmpvarflg != 3 && (n = current[type])) {	/* found one */
		current[type] = links[n];
		return(tempadr[n]);
	}
	/* must allocate a new one */
	adr = dataadr;
	switch (type) {
		case TEMPTYPE_DIM1: 
			putdata1(1);
			dataadr += 4;
			break;
		case TEMPTYPE_DIM256: 
			putdatahhll(0xFC1F);
			putdatallhh(256);
			dataadr += 256 + 7;
			break;
		case TEMPTYPE_INT:
			putdatahhll(0xF7BF);
			dataadr += 6;
			break;
		case TEMPTYPE_ALTFORM31:
		case TEMPTYPE_FORM31:
			putdata1(0x9F);
			dataadr += 32;
			break;
		case TEMPTYPE_FLOAT: 
			putdatahhll(0xF7DF);
			putdata1(0);
			dataadr += 10;
			break;
		case TEMPTYPE_OBJECT:
			putdatahhll(0xFC18);
			dataadr += 44;
			break;
	}
	if (highuse == MAXTEMP - 1) error(ERROR_EXPTOOBIG);
	else if (tmpvarflg != 3) {  /* not unique */
		tempadr[++highuse] = adr;
		if (!(n = first[type])) first[type] = highuse;
		else {
			while (links[n]) n = links[n];
			links[n] = highuse;
		}
	}
	return(adr);
}


INT parseevent()  /* parse an event */
/* return 0 if event is keyword or literal and is completely parsed */
/* return RULE_xxxx to complete parsing of a variable */
{
	INT n, m;
	UINT ucwork;
	CHAR work[40];
	static struct kwstruct {
		CHAR *name;
		SHORT value;
	} keywords[] = {  /* THESE HAVE TO BE ALPHABETIZED */
		{"ALL", 0},				/* 0 */
		{"ALLCHARS", 133},	/* 1 */
		{"ALLFKEYS", 136},	/* 2 */
		{"ALLKEYS", 134},	/* 3 */
 		{"ANYQUEUE", 135},	/* 4 */
 		{"BACKSPACE", 131},	/* 5 */
		{"BKSPC", 131},		/* 6 */
		{"BKTAB", 130},		/* 7 */
		{"CANCEL", 132},	/* 8 */
		{"CFAIL", 14},			/* 9 */
		{"DEBUG", -1},		/* 10 */
		{"DEL", 23},			/* 11 */
		{"DELETE", 23},		/* 12 */
		{"DOWN", 19},		/* 13 */
		{"END", 25},			/* 14 */
		{"ENTER", 30},		/* 15 */
		{"ESC", 29},			/* 16 */
		{"FORMAT", 13},	/* 17 */
		{"HOME", 24},		/* 18 */
		{"INS", 22},				/* 19 */
		{"INSERT", 22},		/* 20 */
		{"INT", 17},				/* 21 */
		{"INTERRUPT", 17},	/* 22 */
		{"IO", 15},				/* 23 */
		{"LEFT", 20},			/* 24 */
		{"PARITY", 11},		/* 25 */
		{"PGDN", 27},			/* 26 */
		{"PGUP", 26},			/* 27 */
		{"PRTOFL", -1},		/* 28 */
		{"QUEUE", 135},	/* 29 */
		{"RANGE", 12},		/* 30 */
		{"RIGHT", 21},		/* 31 */
		{"SPOOL", 16},		/* 32 */
		{"TAB", 129},			/* 33 */
		{"UNDO", 29},		/* 34 */
		{"UP", 18},				/* 35 */
		{"XFAIL", 141},		/* 36 */
		{"~", -1}					/* 37 */
	};
	
	symtype = TYPE_KEYWORD;
	for (n = 0; (work[n] = (CHAR) toupper(token[n])); n++);
	switch (work[0]) {
		case 'A': n = 0; break;
		case 'B':	n = 5; break;
		case 'C': n = 8; break;
		case 'D': n = 10; break;
		case 'E': n = 14; break;
		case 'F':
			if (n == 2 && work[1] > '0' && work[1] <= '9') {
				putcode1((UCHAR) (work[1] - '0'));
				return(0);
			}
			if (n == 3) {
				if (work[1] == '1' && isdigit(work[2])) {
					ucwork = (work[2] - '0' + 10);
					if (work[2] > '0') ucwork += 200;
					putcode1(ucwork);
					return(0);
				}
				if (work[1] == '2' && work[2] == '0') {
					putcode1(220);
					return(0);
				}
			}
			n = 17;
			break;
		case 'H': n = 18; break;
		case 'I': n = 19; break;
		case 'L': n = 24; break;
		case 'P': n = 25; break;
		case 'Q': n = 29; break;
		case 'R': n = 30; break;
		case 'S': n = 32; break;
		case 'T': n = 33; break;
		case 'U': n = 34; break;
		case 'X': n = 36; break;
		default: n = 37;
	}
	while (TRUE) {  /* look for a keyword */
		m = strcmp(keywords[n].name, work);
		if (m < 0) {
			n++;
			continue;
		}
		if (!m) break;
		/* its not a keyword, it must be a character variable */
		putcode1(128);
		if (getdsym((CHAR *) token) == -1) {
			error(ERROR_VARNOTFOUND);
			return(RULE_NOPARSE);
		}
		symbasetype = (UCHAR) (symtype & ~TYPE_PTR);
		return(RULE_CVAR);
	}
	/* found a keyword */
	m = keywords[n].value;
	if (m >= 0) {  /* regular keyword */
		putcode1((UCHAR) m);
		return(0);
	}
	else {  /* ignore this TRAP/TRAPCLR statement */
		codebufcnt = 0;
		return(0);
	}
}

/* flag   if FALSE, reverse to NOT condition/function key */
/*        return cond or xcond value, else return -1 with linecnt not altered */
INT parseflag(INT flag)  /* parse named flags and function keys for IF clause */
{
	UCHAR i, j, k, l;

	if (tokentype != TOKEN_WORD) goto parseflag1;
	if (strcmp((CHAR *) token, "NOT")) i = j = k = l = 0;
	else {
		whitespace();
		scantoken(SCAN_UPCASE);
		if (tokentype != TOKEN_WORD) goto parseflag1;
   		l = 1;
		i = 4;
		j = 10;
 		k = 20;
	}
	if (flag) {  /* reverse the condition */
		if (i) i = j = k = l = 0;
		else {
			l = 1;
			i = 4;
			j = 10;
			k = 20;
		}
	}
	switch (token[0]) {
		case 'B':
			if (!strcmp((CHAR *) token, "BKTAB")) return(k + 55);
			goto parseflag1;
		case 'D':
			if (!strcmp((CHAR *) token, "DOWN")) return(k + 42);
			if (!strcmp((CHAR *) token, "DELETE")) return(k + 46);
			goto parseflag1;
		case 'E':
			if (!strcmp((CHAR *) token, "EQUAL")) return(i);
			if (!strcmp((CHAR *) token, "EOS")) return(i + 3);
			if (!strcmp((CHAR *) token, "ESC")) return(k + 52);
			if (!strcmp((CHAR *) token, "END")) return(k + 48);
			if (!strcmp((CHAR *) token, "ENTER")) return(k + 53);
			goto parseflag1;
		case 'F':
			if (!isdigit(token[1])) goto parseflag1;
			if (!token[2]) {
				if (token[1] == '0') goto parseflag1;
				return(j + token[1] - '0' + 20);
			}
			if (isdigit(token[2]) && !token[3]) {
				if (token[1] == '1') {
					if (token[2] == '0') return(j + 30);
					return(j + token[2] - '0' + 80);
				}
				if (token[1] == '2' && token[2] == '0') return(j + 90);
			}
			goto parseflag1;
		case 'G':
			if (!strcmp((CHAR *) token, "GREATER")) return(l + 8); 
			goto parseflag1;
		case 'H':
			if (!strcmp((CHAR *) token, "HOME")) return(k + 47);
			goto parseflag1;
		case 'I':
			if (!strcmp((CHAR *) token, "INSERT")) return(k + 45);
			goto parseflag1;
		case 'L':
			if (!strcmp((CHAR *) token, "LESS")) return(i + 1);
			if (!strcmp((CHAR *) token, "LEFT")) return(k + 43);
			goto parseflag1;
		case 'O':
			if (!strcmp((CHAR *) token, "OVER")) return(i + 2);
			goto parseflag1;
		case 'P':
			if (!strcmp((CHAR *) token, "PGUP")) return(k + 49);
			if (!strcmp((CHAR *) token, "PGDN")) return(k + 50);
			goto parseflag1;
		case 'R':
			if (!strcmp((CHAR *) token, "RIGHT")) return(k + 44);
			goto parseflag1;
		case 'T':
			if (!strcmp((CHAR *) token, "TAB")) return(k + 54);
			goto parseflag1;
		case 'U':
			if (!strcmp((CHAR *) token, "UNDO")) return(k + 52);
			if (!strcmp((CHAR *) token, "UP")) return(k + 41);
			goto parseflag1;
		case 'Z':
			if (!strcmp((CHAR *) token, "ZERO")) return(i);
	}

parseflag1:
	return(-1);
}

/**
 * parse an if expression and gen goto code
 *
 * labelnum		non-drop through label number
 * flag			if flag = TRUE, drop through if exp/cond is TRUE, otherwise ... 
 */
INT parseifexp(UINT labelnum, INT flag)
{
	INT condval;
	UCHAR linecntsave;

	linecntsave = linecnt;
	scantoken(SCAN_UPCASE);
	if (tokentype != TOKEN_LPAREND) {
		if ((condval = parseflag(flag)) < 0) {
			linecnt = linecntsave;
			scantoken(SCAN_UPCASE); /* rescan not */
			if (!strcmp((CHAR *) token, "NOT")) {
				flag = !flag;
				whitespace();
				scantoken(SCAN_UPCASE);
			}
			else {
				error(ERROR_SYNTAX);
				return(-1);
			}
		}
		else {
			makecondgoto(labelnum, condval);
			return(0);
		}
	}
	if (tokentype == TOKEN_LPAREND) {
		if (!xparse(')')) return(-1);
		if (!xoptast(&topnode)) return(-1);
		if (!xgengotoifcode(topnode, (UINT) flag, labelnum)) return(-1);
	}
	else {
		error(ERROR_SYNTAX);
		return(-1);
	}
	return(0);
}

void putdefine(CHAR *s)  /* save DEFINE definition */
{
	INT n, m;
	CHAR *tableptr;

	m = (INT)strlen(s);
	n = (m + (INT)strlen((CHAR *) &line[linecnt]) + 3);
	if (definecnt + n >= definemax) {
		definemax += 300;
		if (!definecnt) {
			if ((definepptr = memalloc(definemax, 0)) == NULL) death(DEATH_NOMEM);
		}
		else if (memchange(definepptr, definemax, 0)) death(DEATH_NOMEM);
	}
	strcpy((CHAR *)(*definepptr + definecnt + 1), s);
	tableptr = (CHAR *)(*definepptr + definecnt + m + 2);
	n = 0;
	if (line[linecnt] == '\"') {
		while (line[++linecnt] != '\"') {
			if (!line[linecnt]) {
				error(ERROR_BADLIT);
				return;
			}
			if (line[linecnt] == '#') *tableptr = line[++linecnt];
			else *tableptr = line[linecnt];
			tableptr++;
			n++;
		}
	}
	else {
		while(line[linecnt] && charmap[line[linecnt]] != 2) {
			*tableptr = line[linecnt++];
			tableptr++;
			n++;
		}
	}
	(*tableptr) = 0;
	(*definepptr)[definecnt] = n + m + 3;
	definecnt += n + m + 3;
}

INT chkdefine(CHAR *defstr) /* if token defstr is a DEFINE label, return TRUE, else FALSE */
{
	UCHAR *ptr, *endptr;

	if (!definecnt) return(FALSE);
	ptr = (*definepptr);
	endptr = ptr + definecnt;
	while (strcmp((CHAR *) ptr + 1, defstr)) {
		ptr += *ptr;
		if (ptr >= endptr) return(FALSE);
	}
	return(TRUE);
}

static INT getdefine(CHAR *s, INT n, INT noastrkflag)  /* if token s is a DEFINE, return 0 and replace in line */
{
	UCHAR *ptr, *endptr;
	INT i, j, k, m;
	static UINT savelinecnt;
	static INT saveinclevel, numexp;

	ptr = (*definepptr);
	endptr = ptr + definecnt;
	while (strcmp((CHAR *) ptr + 1, s)) {
		ptr += *ptr;
		if (ptr >= endptr) return(1);
	}
	ptr += strlen((CHAR *) ptr + 1) + 2;
	if (noastrkflag && *ptr == '*') return(1);
	if (saveinclevel == inclevel) {
		if (savelinecnt == linecounter[inclevel]) {
			if (++numexp > MAX_EXPANSIONS) {
				error(ERROR_TOOMANYEXPANSIONS);
				return(1);
			}
		}
		else {
			savelinecnt = linecounter[inclevel];
			numexp = 1;
		}
	}
	else {
		saveinclevel = inclevel;
		savelinecnt = linecounter[inclevel];
		numexp = 1;
	}
	j = (INT)strlen((CHAR *) ptr);				/* length of replacement string */
	k = linecnt - n;						/* length of target string */
	m = (INT)strlen((CHAR *) &line[linecnt]);		/* length of remaining line (following linecnt) */
	i = j - k;							/* delta of string lengths */
	if (linecnt + m + i > LINESIZE - 1) {
		error(ERROR_LINETOOLONG);
		return(1);
	}
	if (i) memmove(&line[linecnt + i], &line[linecnt], (size_t) m);
	line[linecnt + m + i] = 0;
	memcpy(&line[n], ptr, (size_t) j);
	linecnt = n;
	return(0);
}

INT checknum(UCHAR *s)  /* return -1 if zero delimited string is not a valid DATABUS number, else return the number of right digits */
{
	INT i, result;

	for (i = 0, result = -1; s[i] == ' '; i++);
	if (s[i] == '-') i++;
	for ( ; isdigit(s[i]); i++) result = 0;
	if (s[i] == '.') i++;
	for ( ; isdigit(s[i]); i++) {
		if (result == -1) result = 1;
		else result++;
	}
	if (s[i]) result = -1;
	if (i > MAXNUMLITSIZE) result = -1;
	return(result);
}

/**
 * s is a zero delimited string containing a number
 * return 0 if s is a valid hex, octal or decimal value 0 to (64K - 1)
 * set symtype and symvalue, return non-zero if error
 */
INT checkxcon(UCHAR *s)
{
	UINT n;
	UCHAR c;

	n = 0;
	if (*s == '0') {
		if (*(++s) == 'X' || *s == 'x') {  /* its hexadecimal */
			while ( (c = *(++s)) ) {
				if (isdigit(c)) n = n * 16 + c - '0';
				else {
					c = (UCHAR) toupper(c);
					if (c < 'A' || c > 'F') return(1);
					n = n * 16 + c - 'A' + 10;
				}
				if (n >= 64 * 1024) return(1);
			}
		}
		else {  /* its octal */
			while ( (c = *(s++)) ) {
				if (c < '0' || c > '7') return(1);
				n = n * 8 + c - '0';
				if (n >= 64 * 1024) return(1);
			}
		}
	}
	else {  /* its decimal */
		while ( (c = *s++) ) {
			if (!isdigit(c)) return(1);
			n = n * 10 + c - '0';
			if (n >= 64 * 1024) return(1);
		}
	}
	symvalue = n;
	symtype = TYPE_INTEGER;
	return(0);
}

/*
 * s is a zero delimited string containing a number
 * return 0 if s is a valid decimal value 0 to (64K - 1)
 * set symtype and symvalue, return non-zero if error
 */
INT checkdcon(UCHAR *s)
{
	UCHAR c;
	UINT n;

	n = 0;
	while ( (c = *s++) ) {
		if (!isdigit(c)) return 1;
		n = n * 10 + c - '0';
		if (n >= 64 * 1024) return 1;
	}
	symvalue = n;
	symtype = TYPE_INTEGER;
	return 0;
}
