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

#define INC_LIMITS
#define INC_STDIO
#define INC_TIME
#include "includes.h"
#include "dbccfg.h"
#include "dbcmp.h"
#include "base.h"

CHAR *errormsg(INT n)  /* return error message description */
{
	switch (n) {  /* non-fatal errors, values from 0 to 199 */
		case ERROR_ERROR: return("* ERROR * ");
		case ERROR_VARNOTFOUND: return("Undefined variable: ");
		case ERROR_BADVARNAME: return("Invalid variable name: ");
		case ERROR_BADVARTYPE: return("Wrong type of variable: ");
		case ERROR_LABELNOTFOUND: return("Undefined execution label: ");
		case ERROR_BADLABEL: return("Invalid execution label name: ");
		case ERROR_VARTOOBIG: return("Variable size is too large");
		case ERROR_DUPVAR: return("Duplicate variable name");
		case ERROR_DUPLABEL: return("Duplicate execution label or label variable");
		case ERROR_NOLABEL: return("Label is required");
		case ERROR_INCLUDENOTFOUND: return("Include file not found");
		case ERROR_INCLUDEFILEBAD: return("Include file type invalid or cannot be opened");
		case ERROR_DBCFILEBAD: return("Error creating .dbc file");
		case ERROR_DBGFILEBAD: return("Error creating debug file");
		case ERROR_CLOCK: return("Invalid CLOCK identifier");
		case ERROR_CLOCKCLIENT: return("Invalid CLOCKCLIENT identifier");
		case ERROR_OPENMODE: return("Invalid OPEN mode");
		case ERROR_BADVERB: return("Invalid verb");
		case ERROR_MISSINGSEMICOLON: return("Missing semicolon");
		case ERROR_MISSINGIF: return("Missing IF");
		case ERROR_MISSINGENDIF: return("Missing ENDIF");
		case ERROR_MISSINGLOOP: return("Missing LOOP");
		case ERROR_MISSINGREPEAT: return("Missing REPEAT");
		case ERROR_MISSINGSWITCH: return("Missing SWITCH");
		case ERROR_MISSINGENDSWITCH: return("Missing ENDSWITCH");
		case ERROR_MISSINGXIF: return("Missing %ENDIF or XIF");
		case ERROR_INCLUDEDEPTH: return("Includes nested too deep");
		case ERROR_BADKEYWORD: return("Invalid keyword: ");
		case ERROR_BADRECSIZE: return("Record size is invalid");
		case ERROR_BADKEYLENGTH: return("Key length is invalid");
		case ERROR_BADNUMLIT: return("Numeric literal expected");
		case ERROR_BADDCON: return("Decimal number is expected");
		case ERROR_DCONTOOBIG: return("Decimal constant value is too large");
		case ERROR_NOCOLON: return("Colon expected");
		case ERROR_BADINTCON: return("Invalid octal or hexadecimal number");
		case ERROR_BADVALUE: return("Invalid value");
		case ERROR_BADEQUATEUSE: return("Invalid use of EQUATE label");
		case ERROR_BADLISTCONTROL: return("Invalid list control");
		case ERROR_CCIFDEPTH: return("%IF nesting is too deep");
		case ERROR_IFDEPTH: return("IF nesting is too deep");
		case ERROR_LOOPDEPTH: return("LOOP nesting is too deep");
		case ERROR_SWITCHDEPTH: return("SWITCH nesting is too deep");
		case ERROR_BADLISTCOMMA: return("Invalid list separator");
		case ERROR_BADCONTLINE: return("Continuation not found");
		case ERROR_CHRLITTOOBIG: return("Character literal is too long");
		case ERROR_BADARRAYSPEC: return("Invalid array specification");
		case ERROR_LINETOOLONG: return("Line is too long");
		case ERROR_MAXDEFINE: return("DEFINE table overflow");
		case ERROR_EXPSYNTAX: return("Error in expression");
		case ERROR_BADPREPMODE: return("Invalid PREP mode");
		case ERROR_BADPREP: return("Invalid preposition: ");
		case ERROR_SYNTAX: return("Invalid syntax");
		case ERROR_BADNUMSPEC: return("Numeric value is too large");
		case ERROR_BADLIT: return("Invalid literal");
		case ERROR_SQLSYNTAX: return("Invalid syntax in SQL statement");
		case ERROR_SQLTOOBIG: return("SQL statement too long (max 127)");
		case ERROR_SQLFROM: return("Invalid use of FROM keyword");
		case ERROR_SQLINTO: return("Invalid use of INTO keyword");
		case ERROR_SQLVARMAX: return("Too many variables in SQL statement");
		case ERROR_BADCHAR: return("Invalid character");
		case ERROR_CCMATCH: return("Mismatched conditional compilation directives");
		case ERROR_BADEXPTYPE: return("Expression result is wrong type");
		case ERROR_BADGLOBAL: return("Invalid use of global type variable");
		case ERROR_BADPARMTYPE: return("User verb parameter type does not match user verb definition");
		case ERROR_NOCASE: return("CASE statement must follow SWITCH statement");
		case ERROR_EXPTOOBIG: return("Expression too complex");
		case ERROR_BADVERBNAME: return("Invalid user verb name");
		case ERROR_BADARRAYINDEX: return("Array index is bad type or value");
		case ERROR_BADCOMMON: return("Common area invalid");
		case ERROR_NOIF: return("IF keyword not found");
		case ERROR_NOTARRAY: return("Array variable name expected");
		case ERROR_BADTABVALUE: return("Tab value is invalid");
		case ERROR_BADSRCREAD: return("Error reading program source record");
		case ERROR_BADIFLEVEL: return("Statement is at wrong IF level");
		case ERROR_BADLOOPLEVEL: return("Statement is at wrong LOOP level");
		case ERROR_BADSWITCHLEVEL: return("Statement is at wrong SWITCH level");
		case ERROR_MISSINGLIST: return("Missing LIST statement");
		case ERROR_MISSINGLISTEND: return("Missing LISTEND");
		case ERROR_BADLISTENTRY: return("Invalid VARLIST entry");
		case ERROR_BADSIZE: return("Invalid size specified");
		case ERROR_GLOBALADRVAR: return("Global address variables are not allowed");
		case ERROR_NOATSIGN: return("@ expected");
		case ERROR_NOCCIF: return("Missing %IF");
		case ERROR_NOENTRIES: return("ENTRIES= is missing");
		case ERROR_NOSIZE: return("SIZE= is missing");
		case ERROR_NOH: return("H= is missing");
		case ERROR_NOV: return("V= is missing");
		case ERROR_NOKEYS: return("KEYS parameter missing");
		case ERROR_DUPKEYWORD: return("Keyword already specified");
		case ERROR_MISSINGOPERAND: return("Required operand missing");
		case ERROR_DUPUVERB: return("Duplicate user verb definition");
		case ERROR_TOOMANYNONPOS: return("Only one non-positional parameter is allowed");
		case ERROR_POSPARMNOTFIRST: return("Positional parameters must precede other parameters");
		case ERROR_BADEXPOP: return("Expression operand is wrong type");
		case ERROR_ROUTINEDEPTH: return("Routines nested too deep");
		case ERROR_MISSINGROUTINE: return("Missing ROUTINE");
		case ERROR_BADPLABEL: return("Label must be defined before first use");
		case ERROR_BADKEYWORDPARM: return("Bad type parameter in verb prototype");
		case ERROR_TOOMANYKEYWRDPARM: return("Only five type parameters are allowed for each keyword");
		case ERROR_TOOMANYEXPANSIONS: return("Too many define expansions");
		case ERROR_BADENDKEYVALUE: return("Bad endkey literal value");
		case ERROR_MISSINGRECORD: return("Record is undefined");
		case ERROR_RECORDREDEF: return("Record already is defined");
		case ERROR_RECORDDEPTH: return("Nested records not allowed");
		case ERROR_BADSCROLLCODE: return("Invalid code in scroll list");
		case ERROR_BADSCREND: return("*SCREND only valid in a scroll list");
		case ERROR_FIXVARFILE: return("FIX and VAR are mutually exclusive keywords");
		case ERROR_COMPUNCOMPFILE: return("COMP and UNCOMP are mutually exclusive keywords");
		case ERROR_FIXCOMPFILE: return("FIX and COMP are mutually exclusive keywords");
		case ERROR_DUPNODUPFILE: return("DUP and NODUP are mutually exclusive keywords");
		case ERROR_DIFFARRAYDIM: return("Array dimensions are incompatible");
		case ERROR_LITTOOLONG: return("Literal too long");
		case ERROR_MISSINGKEYS: return("The keyword KEYS= is required");
		case ERROR_MISSINGRECORDEND: return("Missing RECORDEND");
		case ERROR_EQNOTFOUND: return("Undefined equate label: ");
		case ERROR_BADRPTCHAR: return("Invalid code following *RPTCHAR or *RPTDOWN");
		case ERROR_BADEXTERNAL: return("External declared following reference");
		case ERROR_BADVERBLEN: return("User verb name too long");
		case ERROR_RECNOTFOUND: return("Undefined record definition: ");
		case ERROR_MISSINGCCDIF: return("Missing if conditional compiler directive");
		case ERROR_MISSINGCCDENDIF: return("Missing %ENDIF or XIF conditional compiler directive");
		case ERROR_DUPDEFINELABEL: return("Duplicate define name");
		case ERROR_CLASSNOTFOUND: return("Class not declared: ");
		case ERROR_DUPCLASS: return("Duplicate class name");
		case ERROR_CLASSDEPTH: return("Nested class definitions not allowed");
		case ERROR_MISSINGCLASSDEF: return("Missing CLASS DEFINITION");
		case ERROR_AMBIGUOUSCLASSMOD: return("Ambiguous class module declaration");
		case ERROR_LABELTOOLONG: return("Label too long");
		case ERROR_INHRTDNOTALLOWED: return("Inherited variables allowed only within a child CLASS DEFINITION");
		case ERROR_NOGLOBALINHERITS: return("Global variables cannot be inherited");
		case ERROR_NORECORDINHRT: return("Inherited variables cannot exist within a RECORD/RECORDEND pair");
		case ERROR_NOROUTINEINHRT: return("Inherited variables cannot exist within a ROUTINE/ENDROUTINE pair");
		case ERROR_UNKNOWNMETHOD: return("Call label not declared as a method");
		case ERROR_BADMETHODCALL: return("Illegal call to a method label");
		case ERROR_BADCOLORBITS: return("Illegal colorbits value: must be 1, 4, 8, 16 or 24");
		case ERROR_BADINITLIST: return("Invalid initializer list");
		case ERROR_ILLEGALNORESET: return("Illegal use of NORESET with ANYQUEUE, QUEUE or COMFILE");
		case ERROR_INVALCLASS: return("Invalid class name");
		case ERROR_INHRTABLENOTALLOWED: return("Inheritable variables allowed only within a CLASS DEFINITION");
		case ERROR_LISTNOTINHERITABLE: return("A LIST var that is not an address variable is not inheritable");
		case ERROR_MISSINGCLASSEND: return("Missing ENDCLASS statement");
		case ERROR_INTERNAL: return("Internal failure");
	}
	switch (n) {  /* fatal errors, values from 200 to 249 */
		case DEATH_BADPARM: return("Invalid parameter:  ");
		case DEATH_BADPARMVALUE: return("Invalid parameter value:  ");
		case DEATH_BADLIBFILE: return("Unable to open library file:  ");
		case DEATH_SRCFILENOTFOUND: return("Program source file not found");
		case DEATH_BADSRCFILE: return("Unable to access program source file");
		case DEATH_NOMEM: return("Not enough memory");
		case DEATH_INITPARM: return("Unable to initialize parameters, check -O option");
		case DEATH_LIBMAX: return("Too many -L parameters specified");
		case DEATH_BADPRTFILE: return("Unable to create print file");
		case DEATH_BADWRKFILE: return("Unable to open work file");
		case DEATH_BADXLTFILE: return("Unable to open or read translate file");
		case DEATH_BADWRKREAD: return("Error reading cross reference work file");
		case DEATH_BADDBCFILE: return("Unable to create .dbc file");
		case DEATH_BADDBGFILE: return("Unable to create debug file");
		case DEATH_BADSRCREAD: return("Unable to read input file");
		case DEATH_BADDBCWRITE: return("Error while writing .dbc file");
		case DEATH_BADDBGWRITE: return("Error while writing .dbg file");
		case DEATH_PGMTOOBIG: return("Program too large");
		case DEATH_DATATOOBIG: return("Data area too large");
		case DEATH_MAXROUTINES: return("Too many external routines");
		case DEATH_MAXEXTERNALS: return("Too many external references");
		case DEATH_MAXSYMBOLS: return("Symbol table overflow - too many execution labels and variables");
		case DEATH_INTERNAL1: return("INTERNAL ERROR 1");
		case DEATH_INTERNAL2: return("INTERNAL ERROR 2");
		case DEATH_INTERNAL3: return("INTERNAL ERROR 3");
		case DEATH_INTERNAL4: return("INTERNAL ERROR 4");
		case DEATH_NOENVFILE: return("Unable to open environment file");
		case DEATH_UNSTAMPED: return("Unstamped version of dbcmp");
		case DEATH_BADWRKWRITE: return("Error while writing to work file");
		case DEATH_INTERNAL5: return("INTERNAL ERROR 5");
		case DEATH_INTERNAL6: return("INTERNAL ERROR 6");
		case DEATH_INTERNAL7: return("INTERNAL ERROR 7");
		case DEATH_INTERNAL8: return(getInteralError8Message());
		case DEATH_BADXMLFILE: return("Unable to create xml file");
		case DEATH_BADXMLWRITE: return("Error while writing xml file");
		case DEATH_BADCFGFILE: return("configuration file empty or missing dbcdxcfg element");
		case DEATH_BADCFGOTHER: return(cfggeterror());
 	}
	switch (n) {	/* warning messages */
		case WARNING_WARNING: return("* WARNING * ");
		case WARNING_COMMACOMMENT: return("Comment that starts with a comma following a statement");
		case WARNING_IFCOMMENT: return("Comment following a statement that optionally allows an IF keyword");
		case WARNING_PARENCOMMENT: return("Comment that starts with a parentheses following a statement");
		case WARNING_NOTENOUGHENDROUTINES:
			return ("There is an excess of ROUTINE/LROUTINE statements over ENDROUTINE statements");
		case WARNING_2COMMENT:
			return ("Comment seen on executable statement");
	}
	switch (n) {  /* halt and command line help messages */
		case 1000: return("\nHALTED - user interrupt\n");
	}
	return("* UNKNOWN ERROR *");
}
