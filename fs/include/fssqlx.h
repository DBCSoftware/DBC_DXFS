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

#ifndef _FSSQLX_INCLUDED
#define _FSSQLX_INCLUDED

/* global definitions */

#define MAX_NAME_LENGTH		32		/* maximum length of data source, table, column, user name, etc. */
#define MAX_SELECT_TABLES	16		/* maximum number of tables in a select statement or query */
#define MAX_IDXKEYS			100		/* maximum number of index or aimdex keys */
#define MAX_COLKEYS			200		/* maximum number of columns associated with the index keys */

#define SQLERR_BADCONNECTID				-401
#define SQLERR_DBDOPENERROR				-501
#define SQLERR_DBDSYNTAX				-502
#define SQLERR_DBDAMBIGUOUS				-503
#define SQLERR_CFGNOTFOUND				-504
#define SQLERR_CFGSYNTAX				-505
#define SQLERR_NAMETOOBIG				-506
#define SQLERR_BADATTR					-507
#define SQLERR_TABLENOTFOUND			-508
#define SQLERR_COLUMNNOTFOUND			-509
#define SQLERR_BADRSID					-510
#define SQLERR_BADROWID					-511
#define SQLERR_BADCMD					-512
#define SQLERR_BADINDEX					-513
#define SQLERR_INTERNAL					-514
#define SQLERR_NOMEM					-515
#define SQLERR_NOACCESSCODE				-516
#define SQLERR_PARSE_NOMEM				-601
#define SQLERR_PARSE_TABLENOTFOUND		-602
#define SQLERR_PARSE_COLUMNNOTFOUND		-603
#define SQLERR_PARSE_BADNUMERIC			-604
#define SQLERR_PARSE_STRINGTRUNCATED	-605
#define SQLERR_PARSE_READONLY			-606
#define SQLERR_PARSE_NOUPDATE			-607
#define SQLERR_PARSE_NOFORUPDATE		-608
#define SQLERR_PARSE_TABLEALREADYEXISTS -609
#define SQLERR_PARSE_ERROR				-699
#define SQLERR_EXEC_TOOBIG				-701
#define SQLERR_EXEC_BADTABLE			-702
#define SQLERR_EXEC_BADWORKSET			-703
#define SQLERR_EXEC_BADPGM				-704
#define SQLERR_EXEC_BADCOL				-705
#define SQLERR_EXEC_BADDATAFILE			-706
#define SQLERR_EXEC_BADWORKFILE			-707
#define SQLERR_EXEC_BADROWID			-708
#define SQLERR_EXEC_BADPARM				-709
#define SQLERR_EXEC_DUPKEY				-710
#define SQLERR_EXEC_NOWRITE				-711
#define SQLERR_OTHER					-801

#define SQL_SUCCESS                0
#define SQL_SUCCESS_WITH_INFO      1
#define SQL_ERROR                 (-1)

/* TYPE_xxxx field used in COLUMN and COLREF */
/* for NUM, scale field is the number of digits to the right of the decimal point */
/* for DATE, scale 1: YYYYMMDD, 2: YYMMDD, 3: MMDDYYYY, 4: MMDDYY */
/* for TIME, scale 1: HHMMSS, 2: HHMM */
/* for TIMESTAMP, scale 1: YYYYMMDDHHMMSSPP, 2: YYYYMMDDHHMMSS */
#define TYPE_LITERAL		0	/* only used by the pseudo code */
#define TYPE_CHAR			1
#define TYPE_NUM			2
#define TYPE_POSNUM			3
#define TYPE_DATE			4
#define TYPE_TIME			5
#define TYPE_TIMESTAMP		6

#define LOCK_OFF		0
#define LOCK_RECORD		1
#define LOCK_FILE		2

/* table reference / column number is 0x7FF / 0xFFFFF */
/* probably can make table reference 0xFFF, but all variables may not */
/* be unsigned. literal offsets can be as high as column number */
#define COLNUM_MASK			0xFFFFF
#define TABREF_SHIFT		20
#define TABREF_WORKVAR		0x7FF		/* work/temp variable, colnum is work variable number (used in execplan) */
#define TABREF_LITEXP		0x7FE		/* literal/expression, colnum is index to S_VALUE or S_AEXP in specialcolumns (used in buildselectplan) */
#define TABREF_LITBUF		0x000		/* literal/variable from literal buffer */

#define tableptr(a) (*connection.htablearray + (a) - 1)
#define indexptr(a, b) (*(a)->hindexarray + (b) - 1)
#define columnptr(a, b) (*(a)->hcolumnarray + (b) - 1)
#define openfileptr(a) (*connection.hopenfilearray + (a) - 1)
#define gettabref(a) (INT)((UINT)(a) >> TABREF_SHIFT)
#define getcolnum(a) (INT)((a) & COLNUM_MASK)
#define gettabcolref(a, b) (UINT)(((a) << TABREF_SHIFT) + (b))
#define nameptr(a) (*connection.nametable + a)

/* forward reference definitions */

/* index key */
typedef struct IDXKEY_STRUCT {
	USHORT pos;
	USHORT len;
} IDXKEY, **HIDXKEY;

/* properties for tables and columns */
/* these are for external purposes and not used by FS */
typedef struct DBDPROP_STRUCT {
	INT key;
	INT value;
} DBDPROP, **HDBDPROP;

/* column key */
typedef struct COLKEY_STRUCT {
	UCHAR keytypes;			/* work mask of type of comparisons */
	UCHAR asciisortflag;	/* column is asciisorttype */
	SHORT next;				/* next column */
	SHORT alt;				/* alternate path at same level */
	USHORT len;				/* length of column */
	USHORT colnum;			/* column number associated with the key */
	USHORT keylen;			/* used to determine longest key path */
} COLKEY, **HCOLKEY;

/* index */
typedef struct INDEX_STRUCT {
	INT indexfilename;				/* index file name, offset into connection.nametable */
	UCHAR type;						/* type, ISAM or AIM */
#define INDEX_TYPE_ISAM	1
#define INDEX_TYPE_AIM	2
	UCHAR flags;					/* miscellaneous flags */
#define INDEX_DUPS		0x01
#define INDEX_FIXED		0x02
#define INDEX_DISTINCT	0x04
#define INDEX_ALLEQUAL	0x08
	INT keylength;					/* ISAM index: file key length */
	INT numkeys;					/* number of keys in the index */
	INT numcolkeys;					/* number of columns in the keys */
	HIDXKEY hkeys;					/* handle to array of keys this index */
	HCOLKEY hcolkeys;				/* handle to table of columns that make up the index key(s) */
} INDEX, **HINDEX;

/* column */
typedef struct COLUMN_STRUCT {
	UCHAR type;						/* SQL type of column: TYPE_xxxx */
	UCHAR scale;					/* scale of numeric columns */
	USHORT offset;					/* offset of column into row/record */
	USHORT fieldlen;				/* length of field in file */
	USHORT length;					/* length of column in table */
	INT name;						/* name of column, offset into connection.nametable  */
	INT label;						/* label name of column, offset into connection.nametable  */
	INT description;				/* description of this column, offset into connection.nametable  */
#define FORMAT_DATE_YEAR		1
#define FORMAT_DATE_YEAR2		2
#define FORMAT_DATE_MONTH		3
#define FORMAT_DATE_DAY			4
#define FORMAT_TIME_HOUR		5
#define FORMAT_TIME_MINUTES		6
#define FORMAT_TIME_SECONDS		7
#define FORMAT_TIME_FRACTION1	8
#define FORMAT_TIME_FRACTION2	9
#define FORMAT_TIME_FRACTION3	10
#define FORMAT_TIME_FRACTION4	11
	INT format;						/* format of date/time/timestamp data type, offset into connection.nametable */
	INT numdbdprops;				/* number of external properties */
	HDBDPROP hdbdproparray;			/* handle of array of external properties */
} COLUMN, **HCOLUMN;

/* table */
typedef struct TABLE_STRUCT {
	INT name;						/* name of table, offset into connection.nametable */
	INT textfilename;				/* text file name, offset into connection.nametable */
	INT description;				/* description of this table, offset into connection.nametable */
#define TABLE_FLAGS_COMPLETE	0x01
#define TABLE_FLAGS_READONLY	0x02
#define TABLE_FLAGS_NOUPDATE	0x04
#define TABLE_FLAGS_EORSIZE		0x08
#define TABLE_FLAGS_TEMPLATE	0x10
#define TABLE_FLAGS_TEMPLATEDIR	0x20
	UCHAR flags;					/* is index info complete (TRUE/FALSE) */
	UCHAR eorsize;					/* number of characters in end-of-record mark */
	INT filtercolumnnum;			/* column number of filter */
	INT filtervalue;				/* value of filter */
	INT reclength;					/* length of the row/record in the table/file */
	INT numcolumns;					/* number of columns in the table */
	HCOLUMN hcolumnarray;			/* handle of array of COLUMN structures */
	INT rioopenflags;				/* RIO_?_ open flags */
	INT lastopenfilenum;			/* openfilenum of last used OPENFILE structure */
	INT lockcount;					/* number of active table locks */
	INT lockhandle;					/* lock handle number */
	INT reclaimindex;				/* index number (1 = first) of reclaimation index */
	INT numindexes;					/* number of indexes */
	HINDEX hindexarray;				/* handle of array of INDEX structures */
	INT numdbdprops;				/* number of external properties */
	HDBDPROP hdbdproparray;			/* handle of array of external properties */
} TABLE, **HTABLE;

typedef struct OPENFILE_STRUCT OPENFILE;
typedef struct OPENFILE_STRUCT **HOPENFILE;

typedef struct OPENINDEX_STRUCT OPENINDEX;
typedef struct OPENINDEX_STRUCT **HOPENINDEX;

typedef struct LOCKFILE_STRUCT LOCKFILE;
typedef struct LOCKFILE_STRUCT **HLOCKFILE;

typedef struct WORKSET_STRUCT WORKSET;
typedef struct WORKSET_STRUCT **HWORKSET;

typedef struct WORKSETINFO_STRUCT WORKSETINFO;
typedef struct WORKSETINFO_STRUCT **HWORKSETINFO;

typedef struct PLAN_STRUCT PLAN;
typedef struct PLAN_STRUCT **HPLAN;

typedef struct COLREF_STRUCT COLREF;
typedef struct COLREF_STRUCT **HCOLREF;

typedef struct PCODE_STRUCT PCODE;
typedef struct PCODE_STRUCT **HPCODE;

typedef UCHAR **HBUF;

/*
 * Represents a connection from the server's point of view
 */
typedef struct server_connection_struct {
/*** CODE: MOVE MOST USED ELEMENT TO THE FRONT ***/
	CHAR **nametable;				/* table for names and strings */
	INT nametablesize;				/* memory actually in use in the name table */
	INT nametablealloc;				/* total memory allocated for the name table */
	CHAR **dbdfilename;				/* name of dbd file */
	INT numtables;					/* number of tables in this database */
	INT numalltables;				/* includes template work tables */
	HTABLE htablearray;				/* handle of array of tables in this database */
	INT numopenfiles;				/* number of open files */
	HOPENFILE hopenfilearray;		/* handle of array of open files */
	INT numworksets;				/* number of work sets */
	HWORKSETINFO **hworksetarray;	/* handle of array of work sets */
	INT numlockfiles;				/* number of lock files */
	HLOCKFILE hlockfilearray;		/* handle of array of locked files */
	INT memresultsize;				/* size of in memory result buffer */
	INT updatelock;					/* update lock type */
	INT deletelock;					/* delete lock type */
	INT maxtablename;				/* longest table name length */
	INT maxtableindex;				/* longest table index name length */
	INT maxtableremarks;			/* longest table remarks length */
	INT maxcolumnname;				/* longest column name length */
	INT maxcolumnremarks;			/* longest column remarks length */
} CONNECTION;

/* description of an open file */
struct OPENFILE_STRUCT {
	SHORT inuseflag;					/* in use by an active result set (TRUE/FALSE) */
	USHORT lrucount;					/* least recently used count */
	INT tablenum;						/* table number (TABLE array offset + 1, 0 means closed) */
	INT textfilenum;					/* file number */
	INT reclength;						/* length of record (buffer alloc is length + 3) */
	OFFSET aimrec;						/* aim record to read */
	OFFSET aimlastpos;					/* last record position */
	OFFSET aimnextpos;					/* next record position */
	UCHAR **hrecbuf;					/* record buffer */
	INT **hopenindexarray;				/* handle of an array of filenums correspding with INDEX structures */
};

/* description of a lock file */
struct LOCKFILE_STRUCT {
	INT lockcount;						/* number of active table locks */
	INT textfilenum;					/* file number */
};

struct WORKSET_STRUCT {
	UCHAR rowdirtyflag;					/* if TRUE current row has been modified in memory */
	UCHAR bufdirtyflag;					/* if TRUE all rows have not been written to the file */
	CHAR updatable;						/* (R)ead, (W)rite, (U)known */
	INT numcolrefs;						/* column count */
	HCOLREF colrefarray;				/* handle to an array of COLREFs */
	INT rsnumcolumns;					/* number columns visible in the result set */
	INT rsrowbytes;						/* number of bytes in visible columns */
	INT rowid;							/* current row id */
	INT rowbytes;						/* number of bytes per row */
	INT rowcount;						/* number of rows */
	INT memrowalloc;					/* number of rows allocated in rows */
	INT memrowoffset;					/* first row in rows actual rowid */
	UCHAR **rows;						/* in memory rows */
	INT fileposalloc;					/* number of longs in fileposarray */
	INT fileposoffset;					/* first long in fileposarray correspond with rowid */
	OFFSET **fileposarray;				/* for update specified, row text file positions */
	INT fiofilenum;						/* work filenum for rows or fileposarray */
};

/* a working set of rows */
struct WORKSETINFO_STRUCT {
	SHORT count;						/* 0 = unused, > 0 = number of worksets */
	HPLAN hplan;						/* if plan required, handle of plan to execute */
	struct corrtable_struct **corrtable;  /* table of column and table correlations (alias) (transferred from plan) */
	WORKSET workset[1];
};

/* execution plan */
struct PLAN_STRUCT {
#define PLAN_FLAG_FORUPDATE		0x01	/* used for selects with 'for updates' */
#define PLAN_FLAG_LOCKNOWAIT	0x02	/* NOWAIT option used with 'for update' */
#define PLAN_FLAG_VALIDREC		0x04	/* current record is valid for modification */
#define PLAN_FLAG_SORT			0x08
#define PLAN_FLAG_ERROR			0x10
	UCHAR flags;						/* miscellaneous flags */
	CHAR updatable;						/* (R)ead, (W)rite, (U)known */
	UCHAR multirowcnt;					/* count of number of multi-row result sets */
	UCHAR tablelockcnt;					/* number of times table has been locked */
	OFFSET filepos;						/* last file position read (update position) */
	INT numtables;						/* number of tables */
	INT tablenums[MAX_SELECT_TABLES];	/* handle of an array of tablenums */
	HCOLREF colrefarray;				/* handle to array of COLREFs used by program */
	INT numworksets;					/* number of working sets */
	INT maxexpsize;						/* maximum size for an expression result */
	INT **worksetcolrefinfo;			/* handle of an array of ints, in triplets (numworksets * 3 ints) */
										/* the first int is the starting offset */
										/* in colrefarray for the columns in this workset */
										/* the second int the number number of columns */
										/* that are visible as columns in the result set */
										/* the third int is the total number of columns */
										/* in this workset */
	UCHAR **literals;					/* literals - to be moved to literal buffer (BUF=1) */
	INT numvars;						/* number of variables */
	INT pgmcount;						/* number of program operations in ops */
	INT worksetnum;						/* workset number executed */
	HPCODE pgm;							/* handle of program operations */
	INT **filerefmap;					/* array of file numbers mapping */
	long **savevars;					/* array of variables (NULL on first execution, saved between executions) */
	struct corrtable_struct **corrtable;  /* table of column and table correlations (alias) (transferred from stmt) */
};

/* definition of a buffer column */
struct COLREF_STRUCT {
	UCHAR type;							/* type of column */
	UCHAR scale;						/* additional length information */
	USHORT offset;						/* offset of start of column within buffer */
	USHORT length;						/* length of column within buffer */
	USHORT tablenum;					/* tablenum (needed for result set) */
	USHORT columnnum;					/* for tables, index of column in hcolumnarray (needed for result set) */
	UINT tabcolnum;						/* table reference & column number */
										/* if colref is associated with the result set, */
										/* then this can be used to find the alias; */
										/* otherwise if associated with set function, it */
										/* is the column to operate on */
};

/* definition of a program operation */
struct PCODE_STRUCT {
	INT code;							/* operation:  OP_xxxx */
	INT op1;							/* first operand */
	INT op2;							/* second operand */
	INT op3;							/* third operand */
};

/* OP_xxx are the operations for the program execution engine */
/*		tableref is the open file reference number (1 is first), mapped to actual openfilenum */
/*		indexnum is the index number, 0 means use the text file, no index */
/*		worksetref is the workset reference number (table references + 1 is first), mapped to actual workset index */
/*		var is variable number (1 is first), which holds a long value */
/*		colref is the colrefnumber, high order bits is workset number, low order bits is column number */
/*		if workset number is 0, then it is a literal and low order bits is offset in literals */
/*		address is offset in pgm, 0 is offset of first instruction */

									/*	--var1---		-var2---		--var3--- */

#define OP_CLEAR			1		/*	tableref */
#define OP_KEYINIT			2		/*	tableref		indexnum		optional var:src (-1=xiofindlast) */
#define OP_KEYLIKE			3		/*	colref			colref (like)   colref (escape character) */
#define OP_SETFIRST			4		/*	tableref		indexnum */
#define OP_SETLAST			5		/*	tableref		indexnum */
#define OP_READNEXT			6		/*	tableref		indexnum		var:dest (0 1) */
#define OP_READPREV			7		/*	tableref		indexnum		var:dest (0 1) */
#define OP_READBYKEY		8		/*	tableref		indexnum		var:dest (0 1) */
#define OP_READBYKEYREV		9		/*	tableref		indexnum		var:dest (0 1) */
#define OP_READPOS			10		/*	tableref		colref (pos)	var:dest (0=deleted 1=success) */
#define OP_UNLOCK			11		/*	tableref */
#define OP_WRITE			12		/*	tableref */
#define OP_UPDATE			13		/*	tableref */
#define OP_DELETE			14		/*	tableref */
#define OP_TABLELOCK		15		/*	tableref */
#define OP_TABLEUNLOCK		16		/*	tableref */
#define OP_WORKINIT			17		/*	worksetref		var:src (required rows, 0 means at least 2) */
#define OP_WORKNEWROW		18		/*	worksetref */
#define OP_WORKFREE			19		/*	worksetref */
#define OP_WORKGETROWID		20		/*	worksetref		var:dest */
#define OP_WORKSETROWID		21		/*	worksetref		var:src (rowid) */
#define OP_WORKGETROWCOUNT	22		/*	worksetref		var:dest */
#define OP_WORKSORT			24		/*	worksetref		var:src (number of keys) */
#define OP_WORKUNIQUE		25		/*	worksetref		var:src (number of keys) */
#define OP_SORTSPEC			26		/*	colref			var:src (key number: 1 is first) */
#define OP_SORTSPECD		27		/*	colref			var:src (key number: 1 is first) */
#define OP_SET				28		/*	var:dest		longvalue:src */
#define OP_INCR				29		/*	var:dest		longvalue:src */
#define OP_MOVE				30		/*	var:src			var:dest */
#define OP_ADD				31		/*	var:src1		var:src2		var:dest */
#define OP_SUB				32		/*	var:src1		var:src2		var:dest */
#define OP_MULT				33		/*	var:src1		var:src2		var:dest */
#define OP_DIV				34		/*	var:src1		var:src2		var:dest */
#define OP_NEGATE			35		/*	var:src			var:dest */
#define OP_MOVETOCOL		36		/*	var:src			colref:dest */
#define OP_FPOSTOCOL		37		/*	colref:dest */
#define OP_COLMOVE			38		/*	colref:src		colref:dest */
#define OP_COLADD			39		/*	colref:src1		colref:src2		colref:dest */
#define OP_COLSUB			40		/*	colref:src1		colref:src2		colref:dest */
#define OP_COLMULT			41		/*	colref:src1		colref:src2		colref:dest */
#define OP_COLDIV			42		/*	colref:src1		colref:src2		colref:dest */
#define OP_COLNEGATE		43		/*	colref:src		colref:dest */
#define OP_COLLOWER			44		/*	colref:src		colref:dest */
#define OP_COLUPPER			45		/*	colref:src		colref:dest */
#define OP_COLNULL			46		/*	colref:dest */
#define OP_COLTEST			47		/*	colref:src1		var:dest (-1 0 1) */
#define OP_COLCOMPARE		48		/*	colref:src1		colref:src2		var:dest (-1 0 1 = src1-src2) */
#define OP_COLISNULL		49		/*	colref:src		var:dest (0 1) */
#define OP_COLLIKE			50		/*	colref:src1		colref:src2		var:dest (-1 0 1 = src1-src2) */
#define OP_COLBINSET		51		/*	colref:dest		longvalue:src */
#define OP_COLBININCR		52		/*	colref:dest		longvalue:src */
#define OP_GOTO				53		/*	address */
#define OP_GOTOIFNOTZERO	54		/*	address			var:src */
#define OP_GOTOIFZERO		55		/*	address			var:src */
#define OP_GOTOIFPOS		56		/*	address			var:src */
#define OP_GOTOIFNOTPOS		57		/*	address			var:src */
#define OP_GOTOIFNEG		58		/*	address			var:src */
#define OP_GOTOIFNOTNEG		59		/*	address			var:src */
#define OP_CALL				60		/*	address */
#define OP_RETURN			61		/* */
#define OP_FINISH			62		/* */
#define OP_TERMINATE		63		/* */
#define OP_COLCAST			64		/*	colref:src		colref:dest */
#define OP_COLCONCAT		65		/*	colref:src1		colref:src2		colref:dest */
#define OP_COLSUBSTR_LEN	66		/*	colref:src1		colref:src2		colref:dest */
#define OP_COLSUBSTR_POS	67		/*	colref:src1		colref:src2		colref:dest */
#define OP_COLTRIM_L		68		/*	colref:src1		colref:src2		colref:dest */
#define OP_COLTRIM_T		69		/*	colref:src1		colref:src2		colref:dest */
#define OP_COLTRIM_B		70		/*	colref:src1		colref:src2		colref:dest */
#define OP_GOTOIFTRUE		OP_GOTOIFNOTZERO
#define OP_GOTOIFFALSE		OP_GOTOIFZERO

/* all of the following have not changed substantially since FSS 1.0 */

/* operand or operator types for arithmetics expressions in an UPDATE and
   logical expressions in the WHERE clause of a SELECT, UPDATE or DELETE.
   keep relational operators under the value of 256 */
#define OPCOLUMN		0				/* operand is column reference */
#define OPVALUE			1				/* operand is character literal or numeric constant */
#define OPEXP			2				/* operand is expression */
#define OPMULT			3
#define OPDIV			4
#define OPADD			5
#define OPSUB			6
#define OPNEGATE		7
#define OPEQ			8
#define OPLT			9
#define OPGT			10
#define OPLE			11
#define OPGE			12
#define OPNE			13
#define OPLIKE			14
#define OPAND			15
#define OPOR			16
#define OPNOT			17
/* Note: do not change the order of the set functions */
#define OPCOUNTALL		18
#define OPCOUNT			19
#define OPSUM			20
#define OPAVG			21
#define OPMIN			22
#define OPMAX			23
#define OPCOUNTDISTINCT	24
#define OPSUMDISTINCT	25
#define OPAVGDISTINCT	26
#define OPMINDISTINCT	27
#define OPMAXDISTINCT	28
#define OPNULL			29
#define OPLOWER			30
#define OPUPPER			31
#define OPCAST			32
#define OPCONCAT		33
#define OPSUBSTRLEN		34
#define OPSUBSTRPOS		35
#define OPTRIML			36
#define OPTRIMT			37
#define OPTRIMB			38

/* join type */
#define JOIN_INNER		1
#define JOIN_LEFT_OUTER	2

/* These definitions and structures are for processing an SQL statement */
/* structure types */
#define STYPE_SELECT      1			/* SQL SELECT statement */
#define STYPE_INSERT      2			/* SQL INSERT statement */
#define STYPE_UPDATE      3			/* SQL UPDATE statement */
#define STYPE_DELETE      4			/* SQL DELETE statement */
#define STYPE_LOCK        5			/* SQL LOCK statement */
#define STYPE_UNLOCK      6			/* SQL UNLOCK statement */
#define STYPE_CREATE      7			/* SQL CREATE statement */
#define STYPE_DROP        8			/* SQL DROP statement */
#define STYPE_ALTER       9			/* SQL ALTER statement */
/* #define STYPE_GRANT	  */		/* SQL GRANT statement */
/* #define STYPE_REVOKE   */		/* SQL REVOKE statement */


#define STYPE_VALUE      11			/* a value */
#define STYPE_SETCOLUMN  12			/* a column and its value in an UPDATE statement */
#define STYPE_WHERE      13			/* the predicates in an SQL WHERE clause */
#define STYPE_AEXP       14			/* arithmetic expression in an UPDATE */

/* Generic definition of any of the other structures */
typedef struct S_GENERIC {
	INT stype;					/* structure type STYPE_xxxx */
} S_GENERIC;

/* This structure describes a value */
typedef struct S_VALUE {
	INT stype;					/* structure type STYPE_VALUE */
	INT length;					/* length of value */
	INT litoffset;				/* offset + 1 in literal buffer assigned by makelitfromsvalue or 0 if not referenced */
	UCHAR escape[1];			/* escape character for LIKE */
	UCHAR data[1];				/* value, N.B. THIS FIELD MUST REMAIN LAST IN THE STRUCT!!!*/
} S_VALUE;

/* structure of an entry in an arithmetic expression */
typedef struct AEXPENTRY {
	INT type;					/* type of entry */
	UINT tabcolnum;				/* if type is OPCOLUMN */
	S_VALUE **valuep;			/* if type is OPVALUE */
	union {
		struct {				/* CAST operator: */
			INT type;			/* destination data type of column */
			INT length;			/* destination size of column */
			INT scale;			/* destination date format or decimals to the right */
		} cast;
	} info;
} AEXPENTRY;

/* Structure describing an arithmetic expression, used to specify the values of
   columns in an UPDATE. The entries are operands and operators in postfix form */
typedef struct S_AEXP {
	INT stype;					/* structure type STYPE_AEXP */
	INT numentries;				/* number of entries in the expression */
	AEXPENTRY aexpentries[1];	/* array of entries in postfix */
} S_AEXP;

/* structure of an entry in a logical expression in a WHERE clause */
typedef struct LEXPENTRY {
	INT type;					/* type */
	INT left;					/* if operator, index of left child */
	INT right;					/* if operator, index of right child */
	INT parent;					/* index of parent */
	INT flags;					/* general flag for any use */
	UINT tabcolnum;				/* if type is OPCOLUMN */
	S_VALUE **valuep;			/* if type is OPVALUE */
	S_AEXP **expp;				/* if type is OPEXP */
} LEXPENTRY;

/* Structure describing a logical expression, used to specify the search condition
   of a WHERE clause in a SELECT, UPDATE or DELETE statement. The entries are operands
   and operators in postfix form as a tree structure. */

typedef struct S_WHERE {
	INT stype;					/* structure type STYPE_WHERE */
	INT numentries;				/* number of entries in the expression */
	LEXPENTRY lexpentries[1];	/* array of entries */
} S_WHERE;

/* This structure describes a column and its value used in an UDPATE */
typedef struct {
	INT stype;					/* structure type STYPE_SETCOLUMN */
	UINT columnnum;				/* column handle of column */
	S_VALUE **valuep;			/* pointer to value for character column */
	S_AEXP **expp;				/* pointer to expression description for numeric column */
} S_SETCOLUMN;

/* description of a column/table correlation (alias) */
typedef struct corrname_struct {
	INT tablenum;						/* table number (zero if column definition or reference) */
	UINT tabcolnum;						/* column table/column number (zero if table or undefined column) */
	UCHAR *expstring;					/* pointer to column expression */
	INT explength;						/* length of column expression */
	CHAR name[MAX_NAME_LENGTH + 1];		/* correlation name, eg. FROM customer AS c */
} CORRINFO;

/* table of column/table correlations (alias) */
typedef struct corrtable_struct {
	INT count;
	INT size;
	CORRINFO info[1];
} CORRTABLE, **HCORRTABLE;

typedef struct setfunc_struct {
	INT type;
	UINT tabcolnum;
} SETFUNC, **HSETFUNC;

/* This structure describes an SQL SELECT statement.
   SELECT [ALL | DISTINCT] {* | columnref [,columnref]...} FROM tableref [,tableref]...
   [WHERE search condition] [ORDER BY {columnref | number} [ASC | DESC] ...]
*/
typedef struct S_SELECT {
	INT stype;					/* structure type STYPE_SELECT */
	CHAR distinctflag;			/* ALL rows or only DISTINCT ones, TRUE = DISTINCT */
	CHAR forupdateflag;			/* FOR UPDATE specified */
	CHAR nowaitflag;			/* NOWAIT on locks, option for update */
	INT numtables;				/* number of tables in FROM list */
	INT tablenums[MAX_SELECT_TABLES];  /* array of tablenums */
	CHAR tablejointypes[MAX_SELECT_TABLES];  /* array of join type ([0] is join of table[0] and [1] */
	S_WHERE **joinonconditions[MAX_SELECT_TABLES];  /* array of join ON conditions */
	INT numcolumns;				/* number of columns specified */
	UINT **tabcolnumarray;		/* array of tablenum/columnnums, or one of the following */
								/* TABREF_LITEXP/index to S_VALUE or S_AEXP in specialcolumns */
	INT numspecialcolumns;		/* number of special column entries */
	S_GENERIC ****specialcolumns;  /* handle of an array of S_GENERIC handles for special columns */
	S_WHERE **where;			/* pointer to description of WHERE predicates */
	INT numordercolumns;		/* number of columns in ORDER BY list */
	UINT **ordertabcolnumarray;	/* array of column handles in ORDER BY list */
	UCHAR **orderascflagarray;	/* array of ascending/descending order, TRUE = ASC */
	UINT setfuncdistinctcolumn;	/* distinct column associated with one or more set functions */
	INT numsetfunctions;		/* number of set function/columns */
	SETFUNC **setfunctionarray;	/* handle of array of set function structures */
	INT numgroupcolumns;		/* number of columns in GROUP BY list */
	UINT **grouptabcolnumarray;	/* array of column handles in GROUP BY list */
	S_WHERE **having;			/* pointer to description of HAVING predicates */
	HCORRTABLE corrtable;		/* table of column and table correlations (alias) */
} S_SELECT;

/* This structure describes an SQL INSERT statement.
   INSERT INTO table [(column [,column]... )] VALUES (value [,value]...)
*/
typedef struct S_INSERT {
	INT stype;					/* structure type STYPE_INSERT */
	INT tablenum;				/* table to insert to */
	INT numcolumns;				/* number of columns/values specified */
	UINT **tabcolnumarray;		/* array of tablenum/columnnums or NULL if only values */
	INT numvalues;				/* number of columns/values specified */
	S_VALUE ****valuesarray;	/* ptr to array of ptrs to values for each column in table */
} S_INSERT;

/* This structure describes an SQL UPDATE statement.
   UPDATE table SET column = value [, column = value]... [WHERE search condition]
*/
typedef struct S_UPDATE {
	INT stype;					/* structure type STYPE_UPDATE */
	INT tablenum;				/* table to update */
	INT numcolumns;				/* number of columns specified */
	S_SETCOLUMN **setcolumnarray;	/* array of descriptions of columns and values */
	S_WHERE **where;			/* pointer to description of WHERE predicates */
} S_UPDATE;

/* This structure describes an SQL DELETE statement.
   DELETE FROM table [WHERE search condition]
*/
typedef struct S_DELETE {
	INT stype;					/* structure type STYPE_DELETE */
	INT tablenum;				/* table to delete from */
	S_WHERE **where;			/* pointer to description of WHERE predicates */
} S_DELETE;

/* This structure describes an SQL LOCK/UNLOCK statement.
   LOCK/UNLOCK TABLE table
*/
typedef struct s_lock_struct {
	INT stype;					/* structure type STYPE_INSERT */
	INT tablenum;				/* table to insert to */
} S_LOCK;

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

INT sqlsuccess(void);
INT sqlerrnum(INT);
INT sqlerrmsg(CHAR *);
INT sqlerrnummsg(INT, CHAR *, CHAR *);
void sqlswi(void);

/* fssql2.c */
extern INT getcolumninfo(CHAR *table, CHAR *column, INT *rsid);
extern INT getprimaryinfo(CHAR *table, INT *rsid);
extern INT getbestrowid(CHAR *table, INT *rsid);
extern INT getindexinfo(CHAR *table, INT allindexes, INT *rsid);
extern INT gettableinfo(CHAR *table, INT *rsid);
extern INT execstmt(UCHAR *stmtstring, INT stmtsize, INT *rsid);
extern INT execnonestmt(UCHAR *stmtstring, INT stmtsize, UCHAR *result, INT *length);
extern INT getrow(INT rsid, INT type, LONG recnum, UCHAR *row, INT *rowsize);
extern INT posupdate(INT rsid, UCHAR *stmtstring, INT stmtsize);
extern INT posdelete(INT rsid);
extern INT getrsupdatable(INT rsid, CHAR *updatable);
extern INT getrsrowcount(INT rsid, LONG *rowcount);
extern INT getrscolcount(INT rsid);
extern CHAR *getrscolinfo(INT rsid, INT column);
extern INT discard(INT rsid);
extern INT allocworkset(INT count);
extern void freeworkset(INT worksetnum);
extern INT maketemplate(CHAR *tablename, INT *tablenum);
extern INT maketablecomplete(INT tablenum);
extern INT opentabletextfile(INT tablenum);
extern INT opentableindexfile(INT openfilenum, INT indexnum);
extern void closetablefiles(INT openfilenum);
extern void closetableindex(INT tablenum, INT i1, INDEX *idx1);
extern void closefilesinalltables(void);
extern INT locktextfile(INT openfilenum);
extern INT unlocktextfile(INT lockfilenum);
extern void closetablelocks(void);

/* fssql3.c */
extern INT parsesqlstmt(UCHAR *stmtstring, INT stmtlen, S_GENERIC ***stmt);
extern INT scanselectstmt(UCHAR *stmtstring, INT stmtlen, S_GENERIC ***stmt);
extern void freestmt(S_GENERIC **stmt);

/* fssql4.c */
extern INT buildselectplan(S_SELECT **stmt, HPLAN *phplan, INT resultonlyflag);
extern INT buildinsertplan(S_INSERT **stmt, HPLAN *phplan);
extern INT buildupdateplan(S_UPDATE **stmt, HPLAN *phplan);
extern INT builddeleteplan(S_DELETE **stmt, HPLAN *phplan);
extern INT buildlockplan(S_LOCK **stmt, HPLAN *phplan);
extern INT buildunlockplan(S_LOCK **stmt, HPLAN *phplan);
extern INT appendupdateplan(S_UPDATE **stmt, HPLAN hplan);
extern void freeplan(HPLAN hplan);

/* fssql5.c */
extern INT execplan(HPLAN hplan, LONG *arg1, LONG *arg2, LONG *arg3);
extern INT readwks(INT worksetnum, INT count);

#endif  /* _FSSQLX_INCLUDED */
