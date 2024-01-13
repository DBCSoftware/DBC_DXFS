/*******************************************************************************
 *
 * Copyright 2024 Portable Software Company
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
package org.ptsw.fs.jdbc;

import java.io.IOException;
import java.sql.RowIdLifetime;
import java.sql.SQLException;
import java.sql.Types;
import java.util.StringTokenizer;

/**
 * This class provides information about the database as a whole.
 *
 * <P>Many of the methods here return lists of information in ResultSets.
 * You can use the normal ResultSet methods such as getString and getInt 
 * to retrieve the data from these ResultSets.  If a given form of
 * metadata is not available, these methods should throw a SQLException.
 *
 * <P>Some of these methods take arguments that are String patterns.  These
 * arguments all have names such as fooPattern.  Within a pattern String, "%"
 * means match any substring of 0 or more characters, and "_" means match
 * any one character. Only metadata entries matching the search pattern 
 * are returned. If a search pattern argument is set to a null ref, it means 
 * that argument's criteria should be dropped from the search.
 * 
 * <P>A SQLException will be thrown if a driver does not support a meta
 * data method.  In the case of methods that return a ResultSet,
 * either a ResultSet (which may be empty) is returned or a
 * SQLException is thrown.
 */

public final class DatabaseMetaData implements java.sql.DatabaseMetaData {

private Message connectMsg; //the Message object used to communicate with
//the server.
private Driver driver; //the Driver object that made this Connection.
private org.ptsw.fs.jdbc.Connection con; //The connection object that made me.

private String userName;
private static final String GETATTR = "GETATTR";
private static final String CATALOG = "CATALOG";
private FSproperties fsprops;
private int FSMajorVersion;

void init(Connection con) {
	this.con = con;
	connectMsg = con.getMessage();
	driver = con.getDriver();
	userName = con.getUserName();
	fsprops = con.getFSproperties();
	FSMajorVersion = con.getFSMajorVersion();
}

//----------------------------------------------------------------------
// First, a variety of minor information about the target database.

/**
 * Can all the procedures returned by getProcedures be called by the
 * current user?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean allProceduresAreCallable() throws SQLException {
	return fsprops.getBoolean("allProceduresAreCallable");
}

/**
 * Can all the tables returned by getTable be SELECTed by the
 * current user?
 *
 * @return true if so 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean allTablesAreSelectable() throws SQLException {
	return fsprops.getBoolean("allTablesAreSelectable");
}

/**
 * What's the url for this database?
 *
 * @return the url or null if it can't be generated
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getURL() throws SQLException {
	return con.getUrl();
}

/**
 * What's our user name as known to the database?
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support this MetaData method.
 *
 * @return our database user name
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getUserName() throws SQLException {
	return userName;
}

/**
 * Is the database in read-only mode?
 *
 * @return always false
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean isReadOnly() throws SQLException {
	return false;
}

/**
 * Are NULL values sorted high?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean nullsAreSortedHigh() throws SQLException {
	return fsprops.getBoolean("nullsAreSortedHigh");
}

/**
 * Are NULL values sorted low?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean nullsAreSortedLow() throws SQLException {
	return fsprops.getBoolean("nullsAreSortedLow");
}

/**
 * Are NULL values sorted at the start regardless of sort order?
 *
 * @return true if so 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean nullsAreSortedAtStart() throws SQLException {
	return fsprops.getBoolean("nullsAreSortedAtStart");
}

/**
 * Are NULL values sorted at the end regardless of sort order?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean nullsAreSortedAtEnd() throws SQLException {
	return fsprops.getBoolean("nullsAreSortedAtEnd");
}

/**
 * What's the name of this database product?
 *
 * @return database product name
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getDatabaseProductName() throws SQLException {
	return fsprops.getString("getDatabaseProductName");
}

/**
 * What's the version of this database product?
 *
 * @return database version
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getDatabaseProductVersion() throws SQLException {
	return con.getFSVersion();
}

/**
 * What's the name of this JDBC driver?
 *
 * @return JDBC driver name
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getDriverName() throws SQLException {
	return driver.getClass().getName();
}

/**
 * What's the version of this JDBC driver?
 *
 * @return JDBC driver version
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getDriverVersion() throws SQLException {
	return ((String.valueOf(driver.getMajorVersion())) + "." + (String.valueOf(driver.getMinorVersion())));
}

/**
 * What's this JDBC driver's major version number?
 *
 * @return JDBC driver major version
 */
@Override
public int getDriverMajorVersion() {
	return driver.getMajorVersion();
}

/**
 * What's this JDBC driver's minor version number?
 *
 * @return JDBC driver minor version number
 */
@Override
public int getDriverMinorVersion() {
	return driver.getMinorVersion();
}

/**
 * Does the database store tables in a local file?
 *
 * @return always false
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean usesLocalFiles() throws SQLException {
	return false;
}

/**
 * Does the database use a file for each table?
 *
 * @return always false
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean usesLocalFilePerTable() throws SQLException {
	return false;
}

/**
 * Does the database treat mixed case unquoted SQL identifiers as
 * case sensitive and as a result store them in mixed case?
 *
 * A JDBC Compliant<sup><font size=-2>TM</font></sup> driver will always return false.
 *
 * @return always false 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsMixedCaseIdentifiers() throws SQLException {
	return false;
}

/**
 * Does the database treat mixed case unquoted SQL identifiers as
 * case insensitive and store them in upper case?
 *
 * @return always true 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean storesUpperCaseIdentifiers() throws SQLException {
	return true;
}

/**
 * Does the database treat mixed case unquoted SQL identifiers as
 * case insensitive and store them in lower case?
 *
 * @return always false 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean storesLowerCaseIdentifiers() throws SQLException {
	return false;
}

/**
 * Does the database treat mixed case unquoted SQL identifiers as
 * case insensitive and store them in mixed case?
 *
 * @return always false 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean storesMixedCaseIdentifiers() throws SQLException {
	return false;
}

/**
 * Does the database treat mixed case quoted SQL identifiers as
 * case sensitive and as a result store them in mixed case?
 *
 * A JDBC Compliant<sup><font size=-2>TM</font></sup> driver will always return true.
 *
 * @return always false
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsMixedCaseQuotedIdentifiers() throws SQLException {
	return false;
}

/**
 * Does the database treat mixed case quoted SQL identifiers as
 * case insensitive and store them in upper case?
 *
 * @return always true 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean storesUpperCaseQuotedIdentifiers() throws SQLException {
	return true;
}

/**
 * Does the database treat mixed case quoted SQL identifiers as
 * case insensitive and store them in lower case?
 *
 * @return always false 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean storesLowerCaseQuotedIdentifiers() throws SQLException {
	return false;
}

/**
 * Does the database treat mixed case quoted SQL identifiers as
 * case insensitive and store them in mixed case?
 *
 * @return always false 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean storesMixedCaseQuotedIdentifiers() throws SQLException {
	return false;
}

/**
 * What's the string used to quote SQL identifiers?
 * This returns a space " " if identifier quoting isn't supported.
 *
 * A JDBC Compliant<sup><font size=-2>TM</font></sup> 
 * driver always uses a double quote character.
 *
 * @return the quoting string
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getIdentifierQuoteString() throws SQLException {
	return fsprops.getString("getIdentifierQuoteString");
}

/**
 * Get a comma separated list of all a database's SQL keywords
 * that are NOT also SQL92 keywords.
 *
 * @return the list 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getSQLKeywords() throws SQLException {
	return fsprops.getString("getSQLKeywords");
}

/**
 * Gets a comma-separated list of math functions.  These are the 
 * X/Open CLI math function names used in the JDBC function escape 
 * clause.
 *
 * @return the list
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getNumericFunctions() throws SQLException {
	return fsprops.getString("getNumericFunctions");
}

/**
 * Gets a comma-separated list of string functions.  These are the 
 * X/Open CLI string function names used in the JDBC function escape 
 * clause.
 *
 * @return the list
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getStringFunctions() throws SQLException {
	return fsprops.getString("getStringFunctions");
}

/**
 * Gets a comma-separated list of system functions.  These are the 
 * X/Open CLI system function names used in the JDBC function escape 
 * clause.
 *
 * @return the list
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getSystemFunctions() throws SQLException {
	return fsprops.getString("getSystemFunctions");
}

/**
 * Get a comma separated list of time and date functions.
 *
 * @return the list
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getTimeDateFunctions() throws SQLException {
	return fsprops.getString("getTimeDateFunctions");
}

/**
 * Gets the string that can be used to escape wildcard characters.
 * This is the string that can be used to escape '_' or '%' in
 * the string pattern style catalog search parameters.
 *
 * <P>The '_' character represents any single character.
 * <P>The '%' character represents any sequence of zero or 
 * more characters.
 *
 * @return the string used to escape wildcard characters
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getSearchStringEscape() throws SQLException {
	return fsprops.getString("getSearchStringEscape");
}

/**
 * Get all the "extra" characters that can be used in unquoted
 * identifier names (those beyond a-z, A-Z, 0-9 and _).
 *
 *
 * @return the string containing the extra characters 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getExtraNameCharacters() throws SQLException {
	return fsprops.getString("getExtraNameCharacters");
}

//--------------------------------------------------------------------
// Functions describing which features are supported.

/**
 * Is "ALTER TABLE" with add column supported?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsAlterTableWithAddColumn() throws SQLException {
	return fsprops.getBoolean("supportsAlterTableWithAddColumn");
}

/**
 * Is "ALTER TABLE" with drop column supported?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsAlterTableWithDropColumn() throws SQLException {
	return fsprops.getBoolean("supportsAlterTableWithDropColumn");
}

/**
 * Is column aliasing supported? 
 *
 * <P>If so, the SQL AS clause can be used to provide names for
 * computed columns or to provide alias names for columns as
 * required.
 *
 * A JDBC Compliant<sup><font size=-2>TM</font></sup> driver always returns true.
 *
 * @return true if so 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsColumnAliasing() throws SQLException {
	return fsprops.getBoolean("supportsColumnAliasing");
}

/**
 * Are concatenations between NULL and non-NULL values NULL?
 *
 * For SQL-92 compliance, a JDBC technology-enabled driver will
 * return <code>true</code>.
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean nullPlusNonNullIsNull() throws SQLException {
	return fsprops.getBoolean("nullPlusNonNullIsNull");
}

/**
 * Is the CONVERT function between SQL types supported?
 *
 * @return always false
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsConvert() throws SQLException {
	return false;
}

/**
 * Is CONVERT between the given SQL types supported?
 *
 * @param fromType the type to convert from
 * @param toType the type to convert to     
 * @return always false
 * @exception SQLException if a database-access error occurs.
 * @see Types
 */
/* CODE: check server if it ever gets supported */
@Override
public boolean supportsConvert(int fromType, int toType) throws SQLException {
	return false;
}

/**
 * Are table correlation names supported?
 *
 * A JDBC Compliant<sup><font size=-2>TM</font></sup> driver always returns true.
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsTableCorrelationNames() throws SQLException {
	return fsprops.getBoolean("supportsTableCorrelationNames");
}

/**
 * If table correlation names are supported, are they restricted
 * to be different from the names of the tables?
 *
 * @return true if so 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsDifferentTableCorrelationNames() throws SQLException {
	return fsprops.getBoolean("supportsDifferentTableCorrelationNames");
}

/**
 * Are expressions in "ORDER BY" lists supported?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsExpressionsInOrderBy() throws SQLException {
	return fsprops.getBoolean("supportsExpressionsInOrderBy");
}

/**
 * Can an "ORDER BY" clause use columns not in the SELECT?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsOrderByUnrelated() throws SQLException {
	return fsprops.getBoolean("supportsOrderByUnrelated");
}

/**
 * Is some form of "GROUP BY" clause supported?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsGroupBy() throws SQLException {
	return fsprops.getBoolean("supportsGroupBy");
}

/**
 * Can a "GROUP BY" clause use columns not in the SELECT?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsGroupByUnrelated() throws SQLException {
	return fsprops.getBoolean("supportsGroupByUnrelated");
}

/**
 * Can a "GROUP BY" clause add columns not in the SELECT
 * provided it specifies all the columns in the SELECT?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsGroupByBeyondSelect() throws SQLException {
	return fsprops.getBoolean("supportsGroupByBeyondSelect");
}

/**
 * Is the escape character in "LIKE" clauses supported?
 *
 * A JDBC Compliant<sup><font size=-2>TM</font></sup> driver always returns true.
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsLikeEscapeClause() throws SQLException {
	return fsprops.getBoolean("supportsLikeEscapeClause");
}

/**
 * Are multiple ResultSets from a single execute supported?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsMultipleResultSets() throws SQLException {
	return fsprops.getBoolean("supportsMultipleResultSets");
}

/**
 * Can we have multiple transactions open at once (on different
 * connections)?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsMultipleTransactions() throws SQLException {
	return fsprops.getBoolean("supportsMultipleTransactions");
}

/**
 * Can columns be defined as non-nullable?
 *
 * A JDBC Compliant<sup><font size=-2>TM</font></sup> driver always returns true.
 *
 * @return always false
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsNonNullableColumns() throws SQLException {
	return false;
}

/**
 * Is the ODBC Minimum SQL grammar supported?
 *
 * All JDBC Compliant<sup><font size=-2>TM</font></sup> drivers must return true.
 *
 * @return <code>true</code> if so; <code>false</code> otherwise
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsMinimumSQLGrammar() throws SQLException {
	return fsprops.getBoolean("supportsMinimumSQLGrammar");
}

/**
 * Is the ODBC Core SQL grammar supported?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsCoreSQLGrammar() throws SQLException {
	return fsprops.getBoolean("supportsCoreSQLGrammar");
}

/**
 * Is the ODBC Extended SQL grammar supported?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsExtendedSQLGrammar() throws SQLException {
	return fsprops.getBoolean("supportsExtendedSQLGrammar");
}

/**
 * Is the ANSI92 entry level SQL grammar supported?
 *
 * All JDBC-Compliant drivers must return true.
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsANSI92EntryLevelSQL() throws SQLException {
	return fsprops.getBoolean("supportsANSI92EntryLevelSQL");
}

/**
 * Is the ANSI92 intermediate SQL grammar supported?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsANSI92IntermediateSQL() throws SQLException {
	return fsprops.getBoolean("supportsANSI92IntermediateSQL");
}

/**
 * Is the ANSI92 full SQL grammar supported?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsANSI92FullSQL() throws SQLException {
	return fsprops.getBoolean("supportsANSI92FullSQL");
}

/**
 * Is the SQL Integrity Enhancement Facility supported?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsIntegrityEnhancementFacility() throws SQLException {
	return fsprops.getBoolean("supportsIntegrityEnhancementFacility");
}

/**
 * Is some form of outer join supported?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsOuterJoins() throws SQLException {
	return fsprops.getBoolean("supportsOuterJoins");
}

/**
 * Are full nested outer joins supported?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsFullOuterJoins() throws SQLException {
	return fsprops.getBoolean("supportsFullOuterJoins");
}

/**
 * Is there limited support for outer joins?  (This will be true
 * if supportFullOuterJoins is true.)
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsLimitedOuterJoins() throws SQLException {
	return fsprops.getBoolean("supportsLimitedOuterJoins");
}

/**
 * What's the database vendor's preferred term for "schema"?
 *
 * @return the vendor term
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getSchemaTerm() throws SQLException {
	return fsprops.getString("getSchemaTerm");
}

/**
 * What's the database vendor's preferred term for "procedure"?
 *
 * @return the vendor term
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getProcedureTerm() throws SQLException {
	return fsprops.getString("getProcedureTerm");
}

/**
 * What's the database vendor's preferred term for "catalog"?
 *
 * @return the vendor term
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getCatalogTerm() throws SQLException {
	return fsprops.getString("getCatalogTerm");
}

/**
 * Does a catalog appear at the start of a qualified table name?
 * (Otherwise it appears at the end)
 *
 * @return true if it appears at the start 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean isCatalogAtStart() throws SQLException {
	return fsprops.getBoolean("isCatalogAtStart");
}

/**
 * What's the separator between catalog and table name?
 *
 * @return the separator string
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getCatalogSeparator() throws SQLException {
	return fsprops.getString("getCatalogSeparator");
}

/**
 * What's the database's default transaction isolation level?  The
 * values are defined in java.sql.Connection.
 *
 * @return the default isolation level 
 * @exception SQLException if a database-access error occurs.
 * @see Connection
 */
@Override
public int getDefaultTransactionIsolation() throws SQLException {
	return fsprops.getInt("getDefaultTransactionIsolation");
}

/**
 * Can a schema name be used in a data manipulation statement?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsSchemasInDataManipulation() throws SQLException {
	return fsprops.getBoolean("supportsSchemasInDataManipulation");
}

/**
 * Can a schema name be used in a procedure call statement?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsSchemasInProcedureCalls() throws SQLException {
	return fsprops.getBoolean("supportsSchemasInProcedureCalls");
}

/**
 * Can a schema name be used in a table definition statement?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsSchemasInTableDefinitions() throws SQLException {
	return fsprops.getBoolean("supportsSchemasInTableDefinitions");
}

/**
 * Can a schema name be used in an index definition statement?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsSchemasInIndexDefinitions() throws SQLException {
	return fsprops.getBoolean("supportsSchemasInIndexDefinitions");
}

/**
 * Can a schema name be used in a privilege definition statement?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsSchemasInPrivilegeDefinitions() throws SQLException {
	return fsprops.getBoolean("supportsSchemasInPrivilegeDefinitions");
}

/**
 * Can a catalog name be used in a data manipulation statement?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsCatalogsInDataManipulation() throws SQLException {
	return fsprops.getBoolean("supportsCatalogsInDataManipulation");
}

/**
 * Can a catalog name be used in a procedure call statement?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsCatalogsInProcedureCalls() throws SQLException {
	return fsprops.getBoolean("supportsCatalogsInProcedureCalls");
}

/**
 * Can a catalog name be used in a table definition statement?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsCatalogsInTableDefinitions() throws SQLException {
	return fsprops.getBoolean("supportsCatalogsInTableDefinitions");
}

/**
 * Can a catalog name be used in an index definition statement?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsCatalogsInIndexDefinitions() throws SQLException {
	return fsprops.getBoolean("supportsCatalogsInIndexDefinitions");
}

/**
 * Can a catalog name be used in a privilege definition statement?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsCatalogsInPrivilegeDefinitions() throws SQLException {
	return fsprops.getBoolean("supportsCatalogsInPrivilegeDefinitions");
}

/**
 * Is positioned DELETE supported?
 *
 * <P><B>FS2 NOTE:</B> Version 3.0 and newer support this
 *
 * @return always false
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsPositionedDelete() throws SQLException {
	return fsprops.getBoolean("supportsPositionedDelete");
}

/**
 * Is positioned UPDATE supported?
 *
 * <P><B>FS2 NOTE:</B> Version 3.0 and newer support this
 *
 * @return always false
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsPositionedUpdate() throws SQLException {
	return fsprops.getBoolean("supportsPositionedUpdate");
}

/**
 * Is SELECT for UPDATE supported?
 *
 * @return always true
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsSelectForUpdate() throws SQLException {
	return true;
}

/**
 * Are stored procedure calls using the stored procedure escape
 * syntax supported?
 *
 * @return always false 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsStoredProcedures() throws SQLException {
	return false;
}

/**
 * Are subqueries in comparison expressions supported?
 *
 * A JDBC Compliant<sup><font size=-2>TM</font></sup> driver always returns true.
 *
 * @return always false
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsSubqueriesInComparisons() throws SQLException {
	return fsprops.getBoolean("supportsSubqueriesInComparisons");
}

/**
 * Are subqueries in 'exists' expressions supported?
 *
 * A JDBC Compliant<sup><font size=-2>TM</font></sup> driver always returns true.
 *
 * @return always false
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsSubqueriesInExists() throws SQLException {
	return fsprops.getBoolean("supportsSubqueriesInExists");
}

/**
 * Are subqueries in 'in' statements supported?
 *
 * A JDBC Compliant<sup><font size=-2>TM</font></sup> driver always returns true.
 *
 * @return always false
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsSubqueriesInIns() throws SQLException {
	return fsprops.getBoolean("supportsSubqueriesInIns");
}

/**
 * Are subqueries in quantified expressions supported?
 *
 * A JDBC Compliant<sup><font size=-2>TM</font></sup> driver always returns true.
 *
 * @return always false
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsSubqueriesInQuantifieds() throws SQLException {
	return fsprops.getBoolean("supportsSubqueriesInQuantifieds");
}

/**
 * Are correlated subqueries supported?
 *
 * A JDBC Compliant<sup><font size=-2>TM</font></sup> driver always returns true.
 *
 * @return always false
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsCorrelatedSubqueries() throws SQLException {
	return fsprops.getBoolean("supportsCorrelatedSubqueries");
}

/**
 * Is SQL UNION supported?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsUnion() throws SQLException {
	return fsprops.getBoolean("supportsUnion");
}

/**
 * Is SQL UNION ALL supported?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsUnionAll() throws SQLException {
	return fsprops.getBoolean("supportsUnionAll");
}

/**
 * Can cursors remain open across commits? 
 * 
 * @return always false
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsOpenCursorsAcrossCommit() throws SQLException {
	return false;
}

/**
 * Can cursors remain open across rollbacks?
 * 
 * @return always false
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsOpenCursorsAcrossRollback() throws SQLException {
	return false;
}

/**
 * Can statements remain open across commits?
 * 
 * @return always false
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsOpenStatementsAcrossCommit() throws SQLException {
	return false;
}

/**
 * Can statements remain open across rollbacks?
 * 
 * @return always false
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsOpenStatementsAcrossRollback() throws SQLException {
	return false;
}

//----------------------------------------------------------------------
// The following group of methods exposes various limitations 
// based on the target database with the current driver.
// Unless otherwise specified, a result of zero means there is no
// limit, or the limit is not known.

/**
 * How many hex characters can you have in an inline binary literal?
 *
 * @return max literal length
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxBinaryLiteralLength() throws SQLException {
	return fsprops.getInt("getMaxBinaryLiteralLength");
}

/**
 * What's the max length for a character literal?
 *
 * @return max literal length
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxCharLiteralLength() throws SQLException {
	return fsprops.getInt("getMaxCharLiteralLength");
}

/**
 * What's the limit on column name length?
 *
 * @return max literal length
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxColumnNameLength() throws SQLException {
	return fsprops.getInt("getMaxColumnNameLength");
}

/**
 * What's the maximum number of columns in a "GROUP BY" clause?
 *
 * @return max number of columns
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxColumnsInGroupBy() throws SQLException {
	return fsprops.getInt("getMaxColumnsInGroupBy");
}

/**
 * What's the maximum number of columns allowed in an index?
 *
 * @return max columns
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxColumnsInIndex() throws SQLException {
	return fsprops.getInt("getMaxColumnsInIndex");
}

/**
 * What's the maximum number of columns in an "ORDER BY" clause?
 *
 * @return max columns
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxColumnsInOrderBy() throws SQLException {
	return fsprops.getInt("getMaxColumnsInOrderBy");
}

/**
 * What's the maximum number of columns in a "SELECT" list?
 *
 * @return max columns
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxColumnsInSelect() throws SQLException {
	return fsprops.getInt("getMaxColumnsInSelect");
}

/**
 * What's the maximum number of columns in a table?
 *
 * @return max columns
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxColumnsInTable() throws SQLException {
	return fsprops.getInt("getMaxColumnsInTable");
}

/**
 * How many active connections can we have at a time to this database?
 *
 * @return max connections
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxConnections() throws SQLException {
	return 32;
}

/**
 * What's the maximum cursor name length?
 *
 * @return max cursor name length in bytes
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxCursorNameLength() throws SQLException {
	return fsprops.getInt("getMaxCursorNameLength");
}

/**
 * What's the maximum length of an index (in bytes)?	
 *
 * @return max index length in bytes, which includes the composite of all
 *      the constituent parts of the index;
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxIndexLength() throws SQLException {
	return fsprops.getInt("getMaxIndexLength");
}

/**
 * What's the maximum length allowed for a schema name?
 *
 * @return max name length in bytes
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxSchemaNameLength() throws SQLException {
	return fsprops.getInt("getMaxSchemaNameLength");
}

/**
 * What's the maximum length of a procedure name?
 *
 * @return max name length in bytes 
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxProcedureNameLength() throws SQLException {
	return fsprops.getInt("getMaxProcedureNameLength");
}

/**
 * What's the maximum length of a catalog name?
 *
 * @return max name length in bytes
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxCatalogNameLength() throws SQLException {
	return fsprops.getInt("getMaxCatalogNameLength");
}

/**
 * What's the maximum length of a single row?
 *
 * @return max row size in bytes
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxRowSize() throws SQLException {
	return fsprops.getInt("getMaxRowSize");
}

/**
 * Did getMaxRowSize() include LONGVARCHAR and LONGVARBINARY
 * blobs?
 *
 * @return always false 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean doesMaxRowSizeIncludeBlobs() throws SQLException {
	return fsprops.getBoolean("doesMaxRowSizeIncludeBlobs");
}

/**
 * What's the maximum length of a SQL statement?
 *
 * @return max length in bytes
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxStatementLength() throws SQLException {
	return fsprops.getInt("getMaxStatementLength");
}

/**
 * How many active statements can we have open at one time to this
 * database?
 *
 * @return the maximum 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxStatements() throws SQLException {
	return 0;
}

/**
 * What's the maximum length of a table name?
 *
 * @return max name length in bytes
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxTableNameLength() throws SQLException {
	return fsprops.getInt("getMaxTableNameLength");
}

/**
 * What's the maximum number of tables in a SELECT?
 *
 * @return the maximum
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxTablesInSelect() throws SQLException {
	return fsprops.getInt("getMaxTablesInSelect");
}

/**
 * What's the maximum length of a user name?
 *
 * @return max name length  in bytes
 *      a result of zero means that there is no limit or the limit is not known
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getMaxUserNameLength() throws SQLException {
	return fsprops.getInt("getMaxUserNameLength");
}

//----------------------------------------------------------------------

/**
 * Are transactions supported? If not, invoking the method
 * <code>commit</code> is a noop and the
 * isolation level is TRANSACTION_NONE.
 *
 * @return <code>true</code> if transactions are supported; <code>false</code> otherwise 
 * @exception SQLException if a database access error occurs
 */
@Override
public boolean supportsTransactions() throws SQLException {
	return false;
}

/**
 * Does the database support the given transaction isolation level?
 *
 * @param level the values are defined in java.sql.Connection
 * @return true if so 
 * @exception SQLException if a database-access error occurs.
 * @see Connection
 */
@Override
public boolean supportsTransactionIsolationLevel(int level) throws SQLException {
	return level == java.sql.Connection.TRANSACTION_NONE;
}

/**
 * Are both data definition and data manipulation statements
 * within a transaction supported?
 *
 * @return true if so 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsDataDefinitionAndDataManipulationTransactions() throws SQLException {
	return fsprops.getBoolean("supportsDataDefinitionAndDataManipulationTransactions");
}

/**
 * Are only data manipulation statements within a transaction
 * supported?
 *
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean supportsDataManipulationTransactionsOnly() throws SQLException {
	return fsprops.getBoolean("supportsDataManipulationTransactionsOnly");
}

/**
 * Does a data definition statement within a transaction force the
 * transaction to commit?
 *
 * @return true if so 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean dataDefinitionCausesTransactionCommit() throws SQLException {
	return fsprops.getBoolean("dataDefinitionCausesTransactionCommit");
}

/**
 * Is a data definition statement within a transaction ignored?
 *
 * @return true if so 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean dataDefinitionIgnoredInTransactions() throws SQLException {
	return fsprops.getBoolean("dataDefinitionIgnoredInTransactions");
}

/**
 * Get a description of stored procedures available in a
 * catalog.
 *
 * <P>Only procedure descriptions matching the schema and
 * procedure name criteria are returned.  They are ordered by
 * PROCEDURE_SCHEM, and PROCEDURE_NAME.
 *
 * <P>Each procedure description has the the following columns:
 *  <OL>
 *	<LI><B>PROCEDURE_CAT</B> String => procedure catalog (may be null)
 *	<LI><B>PROCEDURE_SCHEM</B> String => procedure schema (may be null)
 *	<LI><B>PROCEDURE_NAME</B> String => procedure name
 *  <LI> reserved for future use
 *  <LI> reserved for future use
 *  <LI> reserved for future use
 *	<LI><B>REMARKS</B> String => explanatory comment on the procedure
 *	<LI><B>PROCEDURE_TYPE</B> short => kind of procedure:
 *      <UL>
 *      <LI> procedureResultUnknown - May return a result
 *      <LI> procedureNoResult - Does not return a result
 *      <LI> procedureReturnsResult - Returns a result
 *      </UL>
 *  </OL>
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support this MetaData method.
 *
 * @param catalog a catalog name; "" retrieves those without a
 * catalog; null means drop catalog name from the selection criteria
 * @param schemaPattern a schema name pattern; "" retrieves those
 * without a schema
 * @param procedureNamePattern a procedure name pattern 
 * @return ResultSet - each row is a procedure description 
 * @exception SQLException if a database-access error occurs.
 * @see #getSearchStringEscape 
 */
@Override
public java.sql.ResultSet getProcedures(String catalog, String schemaPattern, String procedureNamePattern) throws SQLException {
	throw new SQLException(Driver.bun.getString("MetaNotAvail"));
}

/**
 * Get a description of a catalog's stored procedure parameters
 * and result columns.
 *
 * <P>Only descriptions matching the schema, procedure and
 * parameter name criteria are returned.  They are ordered by
 * PROCEDURE_SCHEM and PROCEDURE_NAME. Within this, the return value,
 * if any, is first. Next are the parameter descriptions in call
 * order. The column descriptions follow in column number order.
 *
 * <P>Each row in the ResultSet is a parameter description or
 * column description with the following fields:
 *  <OL>
 *	<LI><B>PROCEDURE_CAT</B> String => procedure catalog (may be null)
 *	<LI><B>PROCEDURE_SCHEM</B> String => procedure schema (may be null)
 *	<LI><B>PROCEDURE_NAME</B> String => procedure name
 *	<LI><B>COLUMN_NAME</B> String => column/parameter name 
 *	<LI><B>COLUMN_TYPE</B> Short => kind of column/parameter:
 *      <UL>
 *      <LI> procedureColumnUnknown - nobody knows
 *      <LI> procedureColumnIn - IN parameter
 *      <LI> procedureColumnInOut - INOUT parameter
 *      <LI> procedureColumnOut - OUT parameter
 *      <LI> procedureColumnReturn - procedure return value
 *      <LI> procedureColumnResult - result column in ResultSet
 *      </UL>
 *  <LI><B>DATA_TYPE</B> short => SQL type from java.sql.Types
 *	<LI><B>TYPE_NAME</B> String => SQL type name
 *	<LI><B>PRECISION</B> int => precision
 *	<LI><B>LENGTH</B> int => length in bytes of data
 *	<LI><B>SCALE</B> short => scale
 *	<LI><B>RADIX</B> short => radix
 *	<LI><B>NULLABLE</B> short => can it contain NULL?
 *      <UL>
 *      <LI> procedureNoNulls - does not allow NULL values
 *      <LI> procedureNullable - allows NULL values
 *      <LI> procedureNullableUnknown - nullability unknown
 *      </UL>
 *	<LI><B>REMARKS</B> String => comment describing parameter/column
 *  </OL>
 *
 * <P><B>Note:</B> Some databases may not return the column
 * descriptions for a procedure. Additional columns beyond
 * REMARKS can be defined by the database.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support this MetaData method.
 *
 * @param catalog a catalog name; "" retrieves those without a
 * catalog; null means drop catalog name from the selection criteria
 * @param schemaPattern a schema name pattern; "" retrieves those
 * without a schema 
 * @param procedureNamePattern a procedure name pattern 
 * @param columnNamePattern a column name pattern 
 * @return ResultSet - each row is a stored procedure parameter or 
 *      column description 
 * @exception SQLException if a database-access error occurs.
 * @see #getSearchStringEscape 
 */
@Override
public java.sql.ResultSet getProcedureColumns(String catalog, String schemaPattern, String procedureNamePattern, String columnNamePattern) throws SQLException {
	throw new SQLException(Driver.bun.getString("MetaNotAvail"));
}

/**
 * Get a description of tables available in a catalog.
 *
 * <P>Only table descriptions matching the catalog, schema, table
 * name and type criteria are returned.  They are ordered by
 * TABLE_TYPE, TABLE_SCHEM and TABLE_NAME.
 *
 * <P>Each table description has the following columns:
 *  <OL>
 *	<LI><B>TABLE_CAT</B> String => table catalog (may be null)
 *	<LI><B>TABLE_SCHEM</B> String => table schema (may be null)
 *	<LI><B>TABLE_NAME</B> String => table name
 *	<LI><B>TABLE_TYPE</B> String => table type.  Typical types are "TABLE",
 *			"VIEW",	"SYSTEM TABLE", "GLOBAL TEMPORARY", 
 *			"LOCAL TEMPORARY", "ALIAS", "SYNONYM".
 *	<LI><B>REMARKS</B> String => explanatory comment on the table
 *  </OL>
 *
 * <P><B>Note:</B> Some databases may not return information for
 * all tables.
 *
 * @param catalog a catalog name; "" retrieves those without a
 * catalog; null means drop catalog name from the selection criteria
 * @param schemaPattern a schema name pattern; "" retrieves those
 * without a schema
 * @param tableNamePattern a table name pattern 
 * @param types a list of table types to include; null returns all types 
 * @return ResultSet - each row is a table description
 * @exception SQLException if a database-access error occurs.
 * @see #getSearchStringEscape 
 */
@Override
public java.sql.ResultSet getTables(String catalog, String schemaPattern, String tableNamePattern, String types[]) throws SQLException {

	String[] colNames = { "TABLE_CAT", "TABLE_SCHEM", "TABLE_NAME", "TABLE_TYPE", "REMARKS" };
	StringBuffer sbData;
	synchronized (connectMsg.getSock()) {
		connectMsg.setFunc((FSMajorVersion > 2) ? CATALOG : GETATTR);
		StringBuffer ds = new StringBuffer("TABLES");
		if (FSMajorVersion < 3) {
			dataAppend(ds, catalog);
			dataAppend(ds, schemaPattern);
			dataAppend(ds, tableNamePattern);
			if (types == null) dataAppend(ds, null);
			else {
				for (int i1 = 0; i1 < types.length; i1++)
					dataAppend(ds, types[i1]);
			}
		}
		else dataAppend(ds, tableNamePattern);

		connectMsg.setData(ds.toString());
		try {
			connectMsg.sendMessage();
			connectMsg.recvMessage();
		}
		catch (IOException ioex) {
			throw new SQLException(ioex.getLocalizedMessage());
		}
		String rtcd = connectMsg.getRtcd();
		if (Driver.traceOn()) Driver.trace("DatabaseMetaData@getTables : rtcd = " + rtcd);
		if (rtcd.startsWith("SET")) {
			String rawdata = new String(connectMsg.getData());
			if (rawdata == null || rawdata.length() == 0) {
				return new ResultSet(colNames); // FS 3
			}
			if (Driver.traceOn()) Driver.trace("DatabaseMetaData.getTables rawdata = '" + rawdata + "'");
			if (FSMajorVersion < 3) {
				StringTokenizer tok = new StringTokenizer(rawdata, "(", true);
				sbData = new StringBuffer(tok.nextToken());
				for (int i1 = 0; i1 < colNames.length; i1++) {
					sbData.append(colNames[i1]);
					sbData.append(tok.nextToken());
					sbData.append(tok.nextToken());
				}
			}
			else {
				StringTokenizer tok = new StringTokenizer(rawdata, " ");
				sbData = new StringBuffer(tok.nextToken()).append(' '); // result set ID
				sbData.append(tok.nextToken()).append(' '); // FS updatable
				sbData.append(tok.nextToken()); // number of rows
				tok.nextToken(); // throw away FS number of columns
				sbData.append(" 5 ");
				sbData.append(colNames[0]).append("(C1) ");
				sbData.append(colNames[1]).append("(C1) ");
				sbData.append(colNames[2]).append(tok.nextToken()).append(' '); // TABLE_NAME column
				sbData.append(colNames[3]).append("(C5) "); // TABLE_TYPE
				sbData.append(colNames[4]).append(tok.nextToken()); // REMARKS column
			}
			org.ptsw.fs.jdbc.ResultSet rs = new ResultSet(sbData.toString(), connectMsg, false, null);
			rs.setFSMajorVersion(FSMajorVersion);
			rs.setCatalogType(Driver.CATALOG_TABLES);
			return rs;
		}
		else {
			return new ResultSet(colNames);
		}
	}
}

private void dataAppend(StringBuffer sb, String s) {
	if (s == null && FSMajorVersion < 3) sb.append(" NULL");
	else if (s == null || s.length() == 0) sb.append(" \"\"");
	else if (s.indexOf(" ") != -1) sb.append(" \"" + s + "\"");
	else sb.append(" " + s);
}

/**
 * Get the schema names available in this database.  The results
 * are ordered by schema name.
 *
 * <P>The schema column is:
 *  <OL>
 *	<LI><B>TABLE_SCHEM</B> String => schema name
 *  </OL>
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support this MetaData method.
 *
 * @return ResultSet - each row has a single String column that is a
 * schema name 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public java.sql.ResultSet getSchemas() throws SQLException {
	throw new SQLException(Driver.bun.getString("MetaNotAvail"));
}

/**
 * Get the catalog names available in this database.  The results
 * are ordered by catalog name.
 *
 * <P>The catalog column is:
 *  <OL>
 *	<LI><B>TABLE_CAT</B> String => catalog name
 *  </OL>
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support this MetaData method.
 *
 * @return ResultSet - each row has a single String column that is a
 * catalog name 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public java.sql.ResultSet getCatalogs() throws SQLException {
	throw new SQLException(Driver.bun.getString("MetaNotAvail"));
}

/**
 * Retrieves the table types available in this database.  The results
 * are ordered by table type.
 *
 * <P>The table type is:
 *  <OL>
 *	<LI><B>TABLE_TYPE</B> String => table type.  Typical types are "TABLE",
 *			"VIEW",	"SYSTEM TABLE", "GLOBAL TEMPORARY", 
 *			"LOCAL TEMPORARY", "ALIAS", "SYNONYM".
 *  </OL>
 *
 * <P><B>FS NOTE:</B> FS does not support this MetaData method.
 *
 * @return a <code>ResultSet</code> object in which each row has a 
 *         single <code>String</code> column that is a table type 
 * @exception SQLException if a database access error occurs
 */
@Override
public java.sql.ResultSet getTableTypes() throws SQLException {
	if (FSMajorVersion < 4) throw new SQLException(Driver.bun.getString("MetaNotAvail"));
	else return ResultSet.EMPTY_SET;
}

/**
 * Get a description of table columns available in a catalog.
 *
 * <P>Only column descriptions matching the catalog, schema, table
 * and column name criteria are returned.  They are ordered by
 * TABLE_SCHEM, TABLE_NAME and ORDINAL_POSITION.
 *
 * <P>Each column description has the following columns:
 *  <OL>
 *	<LI><B>TABLE_CAT</B> String => table catalog (may be null)
 *	<LI><B>TABLE_SCHEM</B> String => table schema (may be null)
 *	<LI><B>TABLE_NAME</B> String => table name
 *	<LI><B>COLUMN_NAME</B> String => column name
 *	<LI><B>DATA_TYPE</B> short => SQL type from java.sql.Types
 *	<LI><B>TYPE_NAME</B> String => Data source dependent type name
 *	<LI><B>COLUMN_SIZE</B> int => column size.  For char or date
 *	    types this is the maximum number of characters, for numeric or
 *	    decimal types this is precision.
 *	<LI><B>BUFFER_LENGTH</B> is not used.
 *	<LI><B>DECIMAL_DIGITS</B> int => the number of fractional digits
 *	<LI><B>NUM_PREC_RADIX</B> int => Radix (typically either 10 or 2)
 *	<LI><B>NULLABLE</B> int => is NULL allowed?
 *      <UL>
 *      <LI> columnNoNulls - might not allow NULL values
 *      <LI> columnNullable - definitely allows NULL values
 *      <LI> columnNullableUnknown - nullability unknown
 *      </UL>
 *	<LI><B>REMARKS</B> String => comment describing column (may be null)
 * 	<LI><B>COLUMN_DEF</B> String => default value (may be null)
 *	<LI><B>SQL_DATA_TYPE</B> int => unused
 *	<LI><B>SQL_DATETIME_SUB</B> int => unused
 *	<LI><B>CHAR_OCTET_LENGTH</B> int => for char types the 
 *       maximum number of bytes in the column
 *	<LI><B>ORDINAL_POSITION</B> int	=> index of column in table 
 *      (starting at 1)
 *	<LI><B>IS_NULLABLE</B> String => "NO" means column definitely 
 *      does not allow NULL values; "YES" means the column might 
 *      allow NULL values.  An empty string means nobody knows.
 *  </OL>
 *
 * @param catalog a catalog name; "" retrieves those without a
 * catalog; null means drop catalog name from the selection criteria
 * @param schemaPattern a schema name pattern; "" retrieves those
 * without a schema
 * @param tableNamePattern a table name pattern 
 * @param columnNamePattern a column name pattern 
 * @return ResultSet - each row is a column description
 * @exception SQLException if a database-access error occurs.
 * @see #getSearchStringEscape 
 */
@Override
public java.sql.ResultSet getColumns(String catalog, String schemaPattern, String tableNamePattern, String columnNamePattern) throws SQLException {
	String[] colNames = { "TABLE_CAT", "TABLE_SCHEM", "TABLE_NAME", "COLUMN_NAME", "DATA_TYPE", "TYPE_NAME", "COLUMN_SIZE", "BUFFER_LENGTH", "DECIMAL_DIGITS",
			"NUM_PREC_RADIX", "NULLABLE", "REMARKS", "COLUMN_DEF", "SQL_DATA_TYPE", "SQL_DATETIME_SUB", "CHAR_OCTET_LENGTH", "ORDINAL_POSITION", "IS_NULLABLE" };
	StringBuffer sbData;
	synchronized (connectMsg.getSock()) {
		if (Driver.traceOn()) Driver.trace("DatabaseMetaData@getColumns : Entered");
		connectMsg.setFunc((FSMajorVersion > 2) ? CATALOG : GETATTR);
		StringBuffer ds = new StringBuffer("COLUMNS");
		if (FSMajorVersion < 3) {
			dataAppend(ds, catalog);
			dataAppend(ds, schemaPattern);
		}
		dataAppend(ds, tableNamePattern);
		dataAppend(ds, columnNamePattern);
		connectMsg.setData(ds.toString());
		try {
			connectMsg.sendMessage();
			connectMsg.recvMessage();
		}
		catch (IOException ioex) {
			throw new SQLException(ioex.getLocalizedMessage());
		}
		String rtcd = connectMsg.getRtcd();
		if (Driver.traceOn()) Driver.trace("DatabaseMetaData@getColumns : rtcd = " + rtcd);
		if (rtcd.startsWith("SET")) {
			String rawdata = new String(connectMsg.getData());
			if (rawdata == null || rawdata.length() == 0) {
				return new ResultSet(colNames); // FS 3
			}
			if (Driver.traceOn()) Driver.trace("DatabaseMetaData.getColumns rawdata = '" + rawdata + "'");
			if (FSMajorVersion < 3) {
				StringTokenizer tok = new StringTokenizer(rawdata, "(", true);
				sbData = new StringBuffer(tok.nextToken());
				for (int i1 = 0; i1 < colNames.length; i1++) {
					sbData.append(colNames[i1]);
					sbData.append(tok.nextToken());
					sbData.append(tok.nextToken());
				}
			}
			else {
				StringTokenizer tok = new StringTokenizer(rawdata, " ");
				sbData = new StringBuffer(tok.nextToken()).append(' '); // result set ID
				sbData.append(tok.nextToken()).append(' '); // FS updatable
				sbData.append(tok.nextToken()).append(' '); // number of rows
				sbData.append(String.valueOf(colNames.length)).append(' '); // number of columns
				tok.nextToken(); // throw away FS number of columns
				sbData.append(colNames[0]).append("(C1) ");
				sbData.append(colNames[1]).append("(C1) ");
				sbData.append(colNames[2]).append(tok.nextToken()).append(' '); //TABLE_NAME
				sbData.append(colNames[3]).append(tok.nextToken()).append(' '); //COLUMN_NAME
				sbData.append(colNames[4]).append("(N4) "); //DATA_TYPE
				tok.nextToken(); // throw away FS description of this column
				sbData.append(colNames[5]).append("(C9) "); //TYPE_NAME
				sbData.append(colNames[6]).append(tok.nextToken()).append(' '); //COLUMN_SIZE
				sbData.append(colNames[7]).append("(N1) "); //BUFFER_LENGTH
				sbData.append(colNames[8]).append(tok.nextToken()).append(' '); //DECIMAL_DIGITS
				sbData.append(colNames[9]).append("(N2) "); //NUM_PREC_RADIX
				sbData.append(colNames[10]).append("(N1) "); //NULLABLE
				sbData.append(colNames[11]).append(tok.nextToken()).append(' '); //REMARKS
				sbData.append(colNames[12]).append("(C1) "); //COLUMN_DEF
				sbData.append(colNames[13]).append("(N4) "); //SQL_DATA_TYPE
				sbData.append(colNames[14]).append("(N4) "); //SQL_DATETIME_SUB
				sbData.append(colNames[15]).append("(N5) "); //CHAR_OCTET_LENGTH
				sbData.append(colNames[16]).append(tok.nextToken()).append(' '); //ORDINAL_POSITION
				sbData.append(colNames[17]).append("(C3)"); //IS_NULLABLE
			}
			org.ptsw.fs.jdbc.ResultSet rs = new ResultSet(sbData.toString(), connectMsg, false, null);
			rs.setFSMajorVersion(FSMajorVersion);
			rs.setCatalogType(Driver.CATALOG_COLUMNS);
			return rs;
		}
		else return new ResultSet(colNames);
	}
}

/**
 * Get a description of the access rights for a table's columns.
 *
 * <P>Only privileges matching the column name criteria are
 * returned.  They are ordered by COLUMN_NAME and PRIVILEGE.
 *
 * <P>Each privilige description has the following columns:
 *  <OL>
 *	<LI><B>TABLE_CAT</B> String => table catalog (may be null)
 *	<LI><B>TABLE_SCHEM</B> String => table schema (may be null)
 *	<LI><B>TABLE_NAME</B> String => table name
 *	<LI><B>COLUMN_NAME</B> String => column name
 *	<LI><B>GRANTOR</B> => grantor of access (may be null)
 *	<LI><B>GRANTEE</B> String => grantee of access
 *	<LI><B>PRIVILEGE</B> String => name of access (SELECT, 
 *      INSERT, UPDATE, REFRENCES, ...)
 *	<LI><B>IS_GRANTABLE</B> String => "YES" if grantee is permitted 
 *      to grant to others; "NO" if not; null if unknown 
 *  </OL>
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support this MetaData method.
 *
 * @param catalog a catalog name; "" retrieves those without a
 * catalog; null means drop catalog name from the selection criteria
 * @param schema a schema name; "" retrieves those without a schema
 * @param table a table name
 * @param columnNamePattern a column name pattern 
 * @return ResultSet - each row is a column privilege description
 * @exception SQLException if a database-access error occurs.
 * @see #getSearchStringEscape 
 */
@Override
public java.sql.ResultSet getColumnPrivileges(String catalog, String schema, String table, String columnNamePattern) throws SQLException {
	throw new SQLException(Driver.bun.getString("MetaNotAvail"));
}

/**
 * Get a description of the access rights for each table available
 * in a catalog. Note that a table privilege applies to one or
 * more columns in the table. It would be wrong to assume that
 * this priviledge applies to all columns (this may be true for
 * some systems but is not true for all.)
 *
 * <P>Only privileges matching the schema and table name
 * criteria are returned.  They are ordered by TABLE_SCHEM,
 * TABLE_NAME, and PRIVILEGE.
 *
 * <P>Each privilige description has the following columns:
 *  <OL>
 *	<LI><B>TABLE_CAT</B> String => table catalog (may be null)
 *	<LI><B>TABLE_SCHEM</B> String => table schema (may be null)
 *	<LI><B>TABLE_NAME</B> String => table name
 *	<LI><B>GRANTOR</B> => grantor of access (may be null)
 *	<LI><B>GRANTEE</B> String => grantee of access
 *	<LI><B>PRIVILEGE</B> String => name of access (SELECT, 
 *      INSERT, UPDATE, REFRENCES, ...)
 *	<LI><B>IS_GRANTABLE</B> String => "YES" if grantee is permitted 
 *      to grant to others; "NO" if not; null if unknown 
 *  </OL>
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support this MetaData method.
 *
 * @param catalog a catalog name; "" retrieves those without a
 * catalog; null means drop catalog name from the selection criteria
 * @param schemaPattern a schema name pattern; "" retrieves those
 * without a schema
 * @param tableNamePattern a table name pattern 
 * @return ResultSet - each row is a table privilege description
 * @exception SQLException if a database-access error occurs.
 * @see #getSearchStringEscape 
 */
@Override
public java.sql.ResultSet getTablePrivileges(String catalog, String schemaPattern, String tableNamePattern) throws SQLException {
	throw new SQLException(Driver.bun.getString("MetaNotAvail"));
}

/**
 * Get a description of a table's optimal set of columns that
 * uniquely identifies a row. They are ordered by SCOPE.
 *
 * <P>Each column description has the following columns:
 *  <OL>
 *	<LI><B>SCOPE</B> short => actual scope of result
 *      <UL>
 *      <LI> bestRowTemporary - very temporary, while using row
 *      <LI> bestRowTransaction - valid for remainder of current transaction
 *      <LI> bestRowSession - valid for remainder of current session
 *      </UL>
 *	<LI><B>COLUMN_NAME</B> String => column name
 *	<LI><B>DATA_TYPE</B> short => SQL data type from java.sql.Types
 *	<LI><B>TYPE_NAME</B> String => Data source dependent type name
 *	<LI><B>COLUMN_SIZE</B> int => precision
 *	<LI><B>BUFFER_LENGTH</B> int => not used
 *	<LI><B>DECIMAL_DIGITS</B> short	 => scale
 *	<LI><B>PSEUDO_COLUMN</B> short => is this a pseudo column 
 *      like an Oracle ROWID
 *      <UL>
 *      <LI> bestRowUnknown - may or may not be pseudo column
 *      <LI> bestRowNotPseudo - is NOT a pseudo column
 *      <LI> bestRowPseudo - is a pseudo column
 *      </UL>
 *  </OL>
 *
 * @param catalog a catalog name; "" retrieves those without a
 * catalog; null means drop catalog name from the selection criteria
 * @param schema a schema name; "" retrieves those without a schema
 * @param table a table name
 * @param scope the scope of interest; use same values as SCOPE
 * @param nullable include columns that are nullable?
 * @return ResultSet - each row is a column description 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public java.sql.ResultSet getBestRowIdentifier(String catalog, String schema, String table, int scope, boolean nullable) throws SQLException {
	String[] colNames = { "SCOPE", "COLUMN_NAME", "DATA_TYPE", "TYPE_NAME", "COLUMN_SIZE", "BUFFER_LENGTH", "DECIMAL_DIGITS", "PSEUDO_COLUMN" };
	StringBuffer sbData;
	synchronized (connectMsg.getSock()) {
		if (Driver.traceOn()) Driver.trace("DatabaseMetaData@getBestRowIdentifier : Entered");
		connectMsg.setFunc((FSMajorVersion > 2) ? CATALOG : GETATTR);
		StringBuffer ds = new StringBuffer("BESTROWID");

		if (FSMajorVersion < 3) {
			dataAppend(ds, catalog);
			dataAppend(ds, schema);
			dataAppend(ds, table);
			dataAppend(ds, (scope == 0) ? " 0" : " 1");
		}
		else dataAppend(ds, table);

		connectMsg.setData(ds.toString());
		try {
			connectMsg.sendMessage();
			connectMsg.recvMessage();
		}
		catch (IOException ioex) {
			throw new SQLException(ioex.getLocalizedMessage());
		}
		String rtcd = connectMsg.getRtcd();
		if (Driver.traceOn()) Driver.trace("DatabaseMetaData@getBestRowIdentifier : rtcd = " + rtcd);
		if (rtcd.startsWith("SET")) {
			String rawdata = new String(connectMsg.getData());
			if (rawdata == null || rawdata.length() == 0) {
				return new ResultSet(colNames); // FS 3
			}
			if (Driver.traceOn()) Driver.trace("DatabaseMetaData.getBestRowIdentifier rawdata = '" + rawdata + "'");
			if (FSMajorVersion < 3) {
				StringTokenizer tok = new StringTokenizer(rawdata, "(", true);
				sbData = new StringBuffer(tok.nextToken());
				for (int i1 = 0; i1 < colNames.length; i1++) {
					sbData.append(colNames[i1]);
					sbData.append(tok.nextToken());
					sbData.append(tok.nextToken());
				}
			}
			else {
				StringTokenizer tok = new StringTokenizer(rawdata, " ");
				sbData = new StringBuffer(tok.nextToken()); // result set ID
				sbData.append(' ');
				sbData.append(tok.nextToken()).append(' '); // FS updatable
				sbData.append(tok.nextToken()).append(' '); // number of rows
				sbData.append(String.valueOf(colNames.length)).append(' '); // number of columns
				tok.nextToken(); // throw away FS number of columns
				sbData.append(colNames[0]).append("(N1) "); // Scope
				sbData.append(colNames[1]).append(tok.nextToken()).append(' '); // Column name
				sbData.append(colNames[2]).append("(N4) "); // DATA_TYPE
				tok.nextToken(); // throw away FS description of this column
				sbData.append(colNames[3]).append("(C9) "); // Type name
				sbData.append(colNames[4]).append(tok.nextToken()).append(' '); // Column size
				sbData.append(colNames[5]).append("(C1) "); // BUFFER_LENGTH
				sbData.append(colNames[6]).append(tok.nextToken()).append(' '); // Decimal digits
				sbData.append(colNames[7]).append("(N1)"); // Pseudo Column
			}
			org.ptsw.fs.jdbc.ResultSet rs = new ResultSet(sbData.toString(), connectMsg, false, null);
			rs.setFSMajorVersion(FSMajorVersion);
			rs.setCatalogType(Driver.CATALOG_BESTROWID);
			return rs;
		}
		else return new ResultSet(colNames);
	}
}

/**
 * Get a description of a table's columns that are automatically
 * updated when any value in a row is updated.  They are
 * unordered.
 *
 * <P>Each column description has the following columns:
 *  <OL>
 *	<LI><B>SCOPE</B> short => is not used
 *	<LI><B>COLUMN_NAME</B> String => column name
 *	<LI><B>DATA_TYPE</B> short => SQL data type from java.sql.Types
 *	<LI><B>TYPE_NAME</B> String => Data source dependent type name
 *	<LI><B>COLUMN_SIZE</B> int => precision
 *	<LI><B>BUFFER_LENGTH</B> int => length of column value in bytes
 *	<LI><B>DECIMAL_DIGITS</B> short	 => scale
 *	<LI><B>PSEUDO_COLUMN</B> short => is this a pseudo column 
 *      like an Oracle ROWID
 *      <UL>
 *      <LI> versionColumnUnknown - may or may not be pseudo column
 *      <LI> versionColumnNotPseudo - is NOT a pseudo column
 *      <LI> versionColumnPseudo - is a pseudo column
 *      </UL>
 *  </OL>
 *
 * @param catalog a catalog name; "" retrieves those without a
 * catalog; null means drop catalog name from the selection criteria
 * @param schema a schema name; "" retrieves those without a schema
 * @param table a table name
 * @return ResultSet - each row is a column description 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public java.sql.ResultSet getVersionColumns(String catalog, String schema, String table) throws SQLException {

	if (FSMajorVersion > 2) throw new SQLException(Driver.bun.getString("MetaNotAvail"));

	String[] colNames = { "SCOPE", "COLUMN_NAME", "DATA_TYPE", "TYPE_NAME", "COLUMN_SIZE", "BUFFER_LENGTH", "DECIMAL_DIGITS", "PSEUDO_COLUMN" };
	synchronized (connectMsg.getSock()) {
		if (Driver.traceOn()) Driver.trace("DatabaseMetaData@getVersionColumns : Entered");
		connectMsg.setFunc(GETATTR);
		StringBuffer ds = new StringBuffer("AUTOUPDATE");
		dataAppend(ds, catalog);
		dataAppend(ds, schema);
		dataAppend(ds, table);
		dataAppend(ds, " 1");
		connectMsg.setData(ds.toString());
		try {
			connectMsg.sendMessage();
			connectMsg.recvMessage();
		}
		catch (IOException ioex) {
			throw new SQLException(ioex.getLocalizedMessage());
		}
		String rtcd = connectMsg.getRtcd();
		if (Driver.traceOn()) Driver.trace("DatabaseMetaData@getVersionColumns : rtcd = " + rtcd);
		if (rtcd.startsWith("SET")) {
			String rawdata = new String(connectMsg.getData());
			if (rawdata == null || rawdata.length() == 0) {
				return new ResultSet(colNames); // FS 3
			}
			StringTokenizer tok = new StringTokenizer(rawdata, "(", true);
			StringBuffer sbData = new StringBuffer(tok.nextToken());
			for (int i1 = 0; i1 < colNames.length; i1++) {
				sbData.append(colNames[i1]);
				sbData.append(tok.nextToken());
				sbData.append(tok.nextToken());
			}
			org.ptsw.fs.jdbc.ResultSet rs = new ResultSet(sbData.toString(), connectMsg, false, null);
			rs.setFSMajorVersion(FSMajorVersion);
			return rs;
		}
		else return new ResultSet(colNames);
	}
}

/**
 * Get a description of a table's primary key columns.  They
 * are ordered by COLUMN_NAME.
 *
 * <P>Each primary key column description has the following columns:
 *  <OL>
 *	<LI><B>TABLE_CAT</B> String => table catalog (may be null)
 *	<LI><B>TABLE_SCHEM</B> String => table schema (may be null)
 *	<LI><B>TABLE_NAME</B> String => table name
 *	<LI><B>COLUMN_NAME</B> String => column name
 *	<LI><B>KEY_SEQ</B> short => sequence number within primary key
 *	<LI><B>PK_NAME</B> String => primary key name (may be null)
 *  </OL>
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support this MetaData method.
 *
 * @param catalog a catalog name; "" retrieves those without a
 * catalog; null means drop catalog name from the selection criteria
 * @param schema a schema name pattern; "" retrieves those
 * without a schema
 * @param table a table name
 * @return ResultSet - each row is a primary key column description 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public java.sql.ResultSet getPrimaryKeys(String catalog, String schema, String table) throws SQLException {
	String[] colNames = { "TABLE_CAT", "TABLE_SCHEM", "TABLE_NAME", "COLUMN_NAME", "KEY_SEQ", "PK_NAME" };
	StringBuffer sbData;
	synchronized (connectMsg.getSock()) {
		if (Driver.traceOn()) Driver.trace("DatabaseMetaData@getBestRowIdentifier : Entered");
		connectMsg.setFunc((FSMajorVersion > 2) ? CATALOG : GETATTR);
		StringBuffer ds = new StringBuffer("PRIMARY");

		if (FSMajorVersion < 3) {
			dataAppend(ds, catalog);
			dataAppend(ds, schema);
			dataAppend(ds, table);
		}
		else dataAppend(ds, table);

		connectMsg.setData(ds.toString());
		try {
			connectMsg.sendMessage();
			connectMsg.recvMessage();
		}
		catch (IOException ioex) {
			throw new SQLException(ioex.getLocalizedMessage());
		}
		String rtcd = connectMsg.getRtcd();
		if (Driver.traceOn()) Driver.trace("DatabaseMetaData@getPrimaryKeys : rtcd = " + rtcd);
		if (rtcd.startsWith("SET")) {
			String rawdata = new String(connectMsg.getData());
			if (rawdata == null || rawdata.length() == 0) {
				return new ResultSet(colNames); // FS 3
			}
			if (Driver.traceOn()) Driver.trace("DatabaseMetaData.getPrimaryKeys rawdata = '" + rawdata + "'");
			if (FSMajorVersion < 3) {
				StringTokenizer tok = new StringTokenizer(rawdata, "(", true);
				sbData = new StringBuffer(tok.nextToken());
				for (int i1 = 0; i1 < colNames.length; i1++) {
					sbData.append(colNames[i1]);
					sbData.append(tok.nextToken());
					sbData.append(tok.nextToken());
				}
			}
			else {
				StringTokenizer tok = new StringTokenizer(rawdata, " ");
				sbData = new StringBuffer(tok.nextToken()).append(' '); // result set ID
				sbData.append(tok.nextToken()).append(' '); // FS updatable
				sbData.append(tok.nextToken()); // number of rows
				tok.nextToken(); // throw away FS number of columns
				sbData.append(" 6 ");
				sbData.append(colNames[0]).append("(C1) "); // TABLE_CAT
				sbData.append(colNames[1]).append("(C1) "); // TABLE_SCHEM
				sbData.append(colNames[2]).append(tok.nextToken()).append(' '); // TABLE_NAME (from FS)
				sbData.append(colNames[3]).append(tok.nextToken()).append(' '); // COLUMN_NAME (from FS)
				sbData.append(colNames[4]).append(tok.nextToken()).append(' '); // KEY_SEQ (from FS)
				sbData.append(colNames[5]).append("(C1) "); // PK_NAME
			}
			org.ptsw.fs.jdbc.ResultSet rs = new ResultSet(sbData.toString(), connectMsg, false, null);
			rs.setFSMajorVersion(FSMajorVersion);
			rs.setCatalogType(Driver.CATALOG_PRIMARY);
			return rs;
		}
		else return new ResultSet(colNames);
	}
}

/**
 * Get a description of the primary key columns that are
 * referenced by a table's foreign key columns (the primary keys
 * imported by a table).  They are ordered by PKTABLE_CAT,
 * PKTABLE_SCHEM, PKTABLE_NAME, and KEY_SEQ.
 *
 * <P>Each primary key column description has the following columns:
 *  <OL>
 *	<LI><B>PKTABLE_CAT</B> String => primary key table catalog 
 *      being imported (may be null)
 *	<LI><B>PKTABLE_SCHEM</B> String => primary key table schema
 *      being imported (may be null)
 *	<LI><B>PKTABLE_NAME</B> String => primary key table name
 *      being imported
 *	<LI><B>PKCOLUMN_NAME</B> String => primary key column name
 *      being imported
 *	<LI><B>FKTABLE_CAT</B> String => foreign key table catalog (may be null)
 *	<LI><B>FKTABLE_SCHEM</B> String => foreign key table schema (may be null)
 *	<LI><B>FKTABLE_NAME</B> String => foreign key table name
 *	<LI><B>FKCOLUMN_NAME</B> String => foreign key column name
 *	<LI><B>KEY_SEQ</B> short => sequence number within foreign key
 *	<LI><B>UPDATE_RULE</B> short => What happens to 
 *       foreign key when primary is updated:
 *      <UL>
 *      <LI> importedNoAction - do not allow update of primary 
 *               key if it has been imported
 *      <LI> importedKeyCascade - change imported key to agree 
 *               with primary key update
 *      <LI> importedKeySetNull - change imported key to NULL if 
 *               its primary key has been updated
 *      <LI> importedKeySetDefault - change imported key to default values 
 *               if its primary key has been updated
 *      <LI> importedKeyRestrict - same as importedKeyNoAction 
 *                                 (for ODBC 2.x compatibility)
 *      </UL>
 *	<LI><B>DELETE_RULE</B> short => What happens to 
 *      the foreign key when primary is deleted.
 *      <UL>
 *      <LI> importedKeyNoAction - do not allow delete of primary 
 *               key if it has been imported
 *      <LI> importedKeyCascade - delete rows that import a deleted key
 *      <LI> importedKeySetNull - change imported key to NULL if 
 *               its primary key has been deleted
 *      <LI> importedKeyRestrict - same as importedKeyNoAction 
 *                                 (for ODBC 2.x compatibility)
 *      <LI> importedKeySetDefault - change imported key to default if 
 *               its primary key has been deleted
 *      </UL>
 *	<LI><B>FK_NAME</B> String => foreign key name (may be null)
 *	<LI><B>PK_NAME</B> String => primary key name (may be null)
 *	<LI><B>DEFERRABILITY</B> short => can the evaluation of foreign key 
 *      constraints be deferred until commit
 *      <UL>
 *      <LI> importedKeyInitiallyDeferred - see SQL92 for definition
 *      <LI> importedKeyInitiallyImmediate - see SQL92 for definition 
 *      <LI> importedKeyNotDeferrable - see SQL92 for definition 
 *      </UL>
 *  </OL>
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support this MetaData method.
 *
 * @param catalog a catalog name; "" retrieves those without a
 * catalog; null means drop catalog name from the selection criteria
 * @param schema a schema name pattern; "" retrieves those
 * without a schema
 * @param table a table name
 * @return ResultSet - each row is a primary key column description 
 * @exception SQLException if a database-access error occurs.
 * @see #getExportedKeys 
 */
@Override
public java.sql.ResultSet getImportedKeys(String catalog, String schema, String table) throws SQLException {
	throw new SQLException(Driver.bun.getString("MetaNotAvail"));
}

/**
 * Get a description of the foreign key columns that reference a
 * table's primary key columns (the foreign keys exported by a
 * table).  They are ordered by FKTABLE_CAT, FKTABLE_SCHEM,
 * FKTABLE_NAME, and KEY_SEQ.
 *
 * <P>Each foreign key column description has the following columns:
 *  <OL>
 *	<LI><B>PKTABLE_CAT</B> String => primary key table catalog (may be null)
 *	<LI><B>PKTABLE_SCHEM</B> String => primary key table schema (may be null)
 *	<LI><B>PKTABLE_NAME</B> String => primary key table name
 *	<LI><B>PKCOLUMN_NAME</B> String => primary key column name
 *	<LI><B>FKTABLE_CAT</B> String => foreign key table catalog (may be null)
 *      being exported (may be null)
 *	<LI><B>FKTABLE_SCHEM</B> String => foreign key table schema (may be null)
 *      being exported (may be null)
 *	<LI><B>FKTABLE_NAME</B> String => foreign key table name
 *      being exported
 *	<LI><B>FKCOLUMN_NAME</B> String => foreign key column name
 *      being exported
 *	<LI><B>KEY_SEQ</B> short => sequence number within foreign key
 *	<LI><B>UPDATE_RULE</B> short => What happens to 
 *       foreign key when primary is updated:
 *      <UL>
 *      <LI> importedNoAction - do not allow update of primary 
 *               key if it has been imported
 *      <LI> importedKeyCascade - change imported key to agree 
 *               with primary key update
 *      <LI> importedKeySetNull - change imported key to NULL if 
 *               its primary key has been updated
 *      <LI> importedKeySetDefault - change imported key to default values 
 *               if its primary key has been updated
 *      <LI> importedKeyRestrict - same as importedKeyNoAction 
 *                                 (for ODBC 2.x compatibility)
 *      </UL>
 *	<LI><B>DELETE_RULE</B> short => What happens to 
 *      the foreign key when primary is deleted.
 *      <UL>
 *      <LI> importedKeyNoAction - do not allow delete of primary 
 *               key if it has been imported
 *      <LI> importedKeyCascade - delete rows that import a deleted key
 *      <LI> importedKeySetNull - change imported key to NULL if 
 *               its primary key has been deleted
 *      <LI> importedKeyRestrict - same as importedKeyNoAction 
 *                                 (for ODBC 2.x compatibility)
 *      <LI> importedKeySetDefault - change imported key to default if 
 *               its primary key has been deleted
 *      </UL>
 *	<LI><B>FK_NAME</B> String => foreign key name (may be null)
 *	<LI><B>PK_NAME</B> String => primary key name (may be null)
 *	<LI><B>DEFERRABILITY</B> short => can the evaluation of foreign key 
 *      constraints be deferred until commit
 *      <UL>
 *      <LI> importedKeyInitiallyDeferred - see SQL92 for definition
 *      <LI> importedKeyInitiallyImmediate - see SQL92 for definition 
 *      <LI> importedKeyNotDeferrable - see SQL92 for definition 
 *      </UL>
 *  </OL>
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support this MetaData method.
 *
 * @param catalog a catalog name; "" retrieves those without a
 * catalog; null means drop catalog name from the selection criteria
 * @param schema a schema name pattern; "" retrieves those
 * without a schema
 * @param table a table name
 * @return ResultSet - each row is a foreign key column description 
 * @exception SQLException if a database-access error occurs.
 * @see #getImportedKeys 
 */
@Override
public java.sql.ResultSet getExportedKeys(String catalog, String schema, String table) throws SQLException {
	throw new SQLException(Driver.bun.getString("MetaNotAvail"));
}

/**
 * Get a description of the foreign key columns in the foreign key
 * table that reference the primary key columns of the primary key
 * table (describe how one table imports another's key.) This
 * should normally return a single foreign key/primary key pair
 * (most tables only import a foreign key from a table once.)  They
 * are ordered by FKTABLE_CAT, FKTABLE_SCHEM, FKTABLE_NAME, and
 * KEY_SEQ.
 *
 * <P>Each foreign key column description has the following columns:
 *  <OL>
 *	<LI><B>PKTABLE_CAT</B> String => primary key table catalog (may be null)
 *	<LI><B>PKTABLE_SCHEM</B> String => primary key table schema (may be null)
 *	<LI><B>PKTABLE_NAME</B> String => primary key table name
 *	<LI><B>PKCOLUMN_NAME</B> String => primary key column name
 *	<LI><B>FKTABLE_CAT</B> String => foreign key table catalog (may be null)
 *      being exported (may be null)
 *	<LI><B>FKTABLE_SCHEM</B> String => foreign key table schema (may be null)
 *      being exported (may be null)
 *	<LI><B>FKTABLE_NAME</B> String => foreign key table name
 *      being exported
 *	<LI><B>FKCOLUMN_NAME</B> String => foreign key column name
 *      being exported
 *	<LI><B>KEY_SEQ</B> short => sequence number within foreign key
 *	<LI><B>UPDATE_RULE</B> short => What happens to 
 *       foreign key when primary is updated:
 *      <UL>
 *      <LI> importedNoAction - do not allow update of primary 
 *               key if it has been imported
 *      <LI> importedKeyCascade - change imported key to agree 
 *               with primary key update
 *      <LI> importedKeySetNull - change imported key to NULL if 
 *               its primary key has been updated
 *      <LI> importedKeySetDefault - change imported key to default values 
 *               if its primary key has been updated
 *      <LI> importedKeyRestrict - same as importedKeyNoAction 
 *                                 (for ODBC 2.x compatibility)
 *      </UL>
 *	<LI><B>DELETE_RULE</B> short => What happens to 
 *      the foreign key when primary is deleted.
 *      <UL>
 *      <LI> importedKeyNoAction - do not allow delete of primary 
 *               key if it has been imported
 *      <LI> importedKeyCascade - delete rows that import a deleted key
 *      <LI> importedKeySetNull - change imported key to NULL if 
 *               its primary key has been deleted
 *      <LI> importedKeyRestrict - same as importedKeyNoAction 
 *                                 (for ODBC 2.x compatibility)
 *      <LI> importedKeySetDefault - change imported key to default if 
 *               its primary key has been deleted
 *      </UL>
 *	<LI><B>FK_NAME</B> String => foreign key name (may be null)
 *	<LI><B>PK_NAME</B> String => primary key name (may be null)
 *	<LI><B>DEFERRABILITY</B> short => can the evaluation of foreign key 
 *      constraints be deferred until commit
 *      <UL>
 *      <LI> importedKeyInitiallyDeferred - see SQL92 for definition
 *      <LI> importedKeyInitiallyImmediate - see SQL92 for definition 
 *      <LI> importedKeyNotDeferrable - see SQL92 for definition 
 *      </UL>
 *  </OL>
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support this MetaData method.
 *
 * @param primaryCatalog a catalog name; "" retrieves those without a
 * catalog; null means drop catalog name from the selection criteria
 * @param primarySchema a schema name pattern; "" retrieves those
 * without a schema
 * @param primaryTable the table name that exports the key
 * @param foreignCatalog a catalog name; "" retrieves those without a
 * catalog; null means drop catalog name from the selection criteria
 * @param foreignSchema a schema name pattern; "" retrieves those
 * without a schema
 * @param foreignTable the table name that imports the key
 * @return ResultSet - each row is a foreign key column description 
 * @exception SQLException if a database-access error occurs.
 * @see #getImportedKeys 
 */
@Override
public java.sql.ResultSet getCrossReference(String primaryCatalog, String primarySchema, String primaryTable, String foreignCatalog, String foreignSchema,
		String foreignTable) throws SQLException {
	throw new SQLException(Driver.bun.getString("MetaNotAvail"));
}

/**
 * Get a description of all the standard SQL types supported by
 * this database. They are ordered by DATA_TYPE and then by how
 * closely the data type maps to the corresponding JDBC SQL type.
 *
 * <P>Each type description has the following columns:
 *  <OL>
 *	<LI><B>TYPE_NAME</B> String => Type name
 *	<LI><B>DATA_TYPE</B> short => SQL data type from java.sql.Types
 *	<LI><B>PRECISION</B> int => maximum precision
 *	<LI><B>LITERAL_PREFIX</B> String => prefix used to quote a literal 
 *      (may be null)
 *	<LI><B>LITERAL_SUFFIX</B> String => suffix used to quote a literal 
 *       (may be null)
 *	<LI><B>CREATE_PARAMS</B> String => parameters used in creating 
 *      the type (may be null)
 *	<LI><B>NULLABLE</B> short => can you use NULL for this type?
 *      <UL>
 *      <LI> typeNoNulls - does not allow NULL values
 *      <LI> typeNullable - allows NULL values
 *      <LI> typeNullableUnknown - nullability unknown
 *      </UL>
 *	<LI><B>CASE_SENSITIVE</B> boolean=> is it case sensitive?
 *	<LI><B>SEARCHABLE</B> short => can you use "WHERE" based on this type:
 *      <UL>
 *      <LI> typePredNone - No support
 *      <LI> typePredChar - Only supported with WHERE .. LIKE
 *      <LI> typePredBasic - Supported except for WHERE .. LIKE
 *      <LI> typeSearchable - Supported for all WHERE ..
 *      </UL>
 *	<LI><B>UNSIGNED_ATTRIBUTE</B> boolean => is it unsigned?
 *	<LI><B>FIXED_PREC_SCALE</B> boolean => can it be a money value?
 *	<LI><B>AUTO_INCREMENT</B> boolean => can it be used for an 
 *      auto-increment value?
 *	<LI><B>LOCAL_TYPE_NAME</B> String => localized version of type name 
 *      (may be null)
 *	<LI><B>MINIMUM_SCALE</B> short => minimum scale supported
 *	<LI><B>MAXIMUM_SCALE</B> short => maximum scale supported
 *	<LI><B>SQL_DATA_TYPE</B> int => unused
 *	<LI><B>SQL_DATETIME_SUB</B> int => unused
 *	<LI><B>NUM_PREC_RADIX</B> int => usually 2 or 10
 *  </OL>
 *
 * @return <code>ResultSet</code> - each row is a SQL type description 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public java.sql.ResultSet getTypeInfo() throws SQLException {
	String[] colNames = { "TYPE_NAME", "DATA_TYPE", "PRECISION", "LITERAL_PREFIX", "LITERAL_SUFFIX", "CREATE_PARAMS", "NULLABLE", "CASE_SENSITIVE",
			"SEARCHABLE", "UNSIGNED_ATTRIBUTE", "FIXED_PREC_SCALE", "AUTO_INCREMENT", "LOCAL_TYPE_NAME", "MINIMUM_SCALE", "MAXIMUM_SCALE", "SQL_DATA_TYPE",
			"SQL_DATETIME_SUB", "NUM_PREC_RADIX" };
	StringBuffer sbData;

	synchronized (connectMsg.getSock()) {
		if (Driver.traceOn()) Driver.trace("DatabaseMetaData@getTypeInfo : Entered");
		if (FSMajorVersion < 3) {
			connectMsg.setFunc(GETATTR);
			StringBuffer ds = new StringBuffer("TYPEINFO");
			dataAppend(ds, " 0");
			connectMsg.setData(ds.toString());
			try {
				connectMsg.sendMessage();
				connectMsg.recvMessage();
			}
			catch (IOException ioex) {
				throw new SQLException(ioex.getLocalizedMessage());
			}
			String rtcd = connectMsg.getRtcd();
			if (Driver.traceOn()) Driver.trace("DatabaseMetaData@getTypeInfo : rtcd = " + rtcd);
			if (rtcd.startsWith("SET")) {
				String rawdata = new String(connectMsg.getData());
				if (rawdata == null || rawdata.length() == 0) {
					return new ResultSet(colNames); // FS 3
				}
				StringTokenizer tok = new StringTokenizer(rawdata, "(", true);
				sbData = new StringBuffer(tok.nextToken());
				for (int i1 = 0; i1 < colNames.length; i1++) {
					sbData.append(colNames[i1]);
					sbData.append(tok.nextToken());
					sbData.append(tok.nextToken());
				}
				org.ptsw.fs.jdbc.ResultSet rs = new ResultSet(sbData.toString(), connectMsg, false, null);
				rs.setFSMajorVersion(FSMajorVersion);
				rs.setCatalogType(Driver.CATALOG_TYPEINFO);
				return rs;
			}
			else {
				return new ResultSet(colNames);
			}
		}
		else { // No call to FS altogether, totally synthetic data!
			sbData = new StringBuffer("1 R 5 "); // result set ID, FS updatable, rows
			sbData.append(colNames.length).append(' '); // number of columns
			sbData.append(colNames[0]).append("(C9) "); // type name
			sbData.append(colNames[1]).append("(N4) "); // data type
			sbData.append(colNames[2]).append("(N5) "); // precision (aka column size)
			sbData.append(colNames[3]).append("(C1) "); // literal prefix
			sbData.append(colNames[4]).append("(C1) "); // literal suffix
			sbData.append(colNames[5]).append("(C15) "); // create params
			sbData.append(colNames[6]).append("(N1) "); // nullable
			sbData.append(colNames[7]).append("(C1) "); // case sensitive (Y/N)
			sbData.append(colNames[8]).append("(N1) "); // searchable
			sbData.append(colNames[9]).append("(C1) "); // unsigned attr (Y/N)
			sbData.append(colNames[10]).append("(C1) "); // fixed prec scale (Y/N)
			sbData.append(colNames[11]).append("(C1) "); // autoincrement (Y/N)
			sbData.append(colNames[12]).append("(C1) "); // local type name
			sbData.append(colNames[13]).append("(N2) "); // minimum scale
			sbData.append(colNames[14]).append("(N5) "); // maximum scale
			sbData.append(colNames[15]).append("(N4) "); // SQL_DATA_TYPE
			sbData.append(colNames[16]).append("(N4) "); // SQL_DATETIME_SUB
			sbData.append(colNames[17]).append("(N2)"); // NUM_PREC_RADIX
			org.ptsw.fs.jdbc.ResultSet rs = new ResultSet(sbData.toString(), connectMsg, false, null);
			rs.setFSMajorVersion(FSMajorVersion);
			rs.setCatalogType(Driver.CATALOG_TYPEINFO);
			return rs;
		}
	}
}

/**
 * Get a description of a table's indices and statistics. They are
 * ordered by NON_UNIQUE, TYPE, INDEX_NAME, and ORDINAL_POSITION.
 *
 * <P>Each index column description has the following columns:
 *  <OL>
 *	<LI><B>TABLE_CAT</B> String => table catalog (may be null)
 *	<LI><B>TABLE_SCHEM</B> String => table schema (may be null)
 *	<LI><B>TABLE_NAME</B> String => table name
 *	<LI><B>NON_UNIQUE</B> boolean => Can index values be non-unique? 
 *      false when TYPE is tableIndexStatistic
 *	<LI><B>INDEX_QUALIFIER</B> String => index catalog (may be null); 
 *      null when TYPE is tableIndexStatistic
 *	<LI><B>INDEX_NAME</B> String => index name; null when TYPE is 
 *      tableIndexStatistic
 *	<LI><B>TYPE</B> short => index type:
 *      <UL>
 *      <LI> tableIndexStatistic - this identifies table statistics that are
 *           returned in conjuction with a table's index descriptions
 *      <LI> tableIndexClustered - this is a clustered index
 *      <LI> tableIndexHashed - this is a hashed index
 *      <LI> tableIndexOther - this is some other style of index
 *      </UL>
 *	<LI><B>ORDINAL_POSITION</B> short => column sequence number 
 *      within index; zero when TYPE is tableIndexStatistic
 *	<LI><B>COLUMN_NAME</B> String => column name; null when TYPE is 
 *      tableIndexStatistic
 *	<LI><B>ASC_OR_DESC</B> String => column sort sequence, "A" => ascending, 
 *      "D" => descending, may be null if sort sequence is not supported; 
 *      null when TYPE is tableIndexStatistic	
 *	<LI><B>CARDINALITY</B> int => When TYPE is tableIndexStatistic, then 
 *      this is the number of rows in the table; otherwise, it is the 
 *      number of unique values in the index.
 *	<LI><B>PAGES</B> int => When TYPE is  tableIndexStatisic then 
 *      this is the number of pages used for the table, otherwise it 
 *      is the number of pages used for the current index.
 *	<LI><B>FILTER_CONDITION</B> String => Filter condition, if any.  
 *      (may be null)
 *  </OL>
 *
 * @param catalog a catalog name; "" retrieves those without a
 * catalog; null means drop catalog name from the selection criteria
 * @param schema a schema name pattern; "" retrieves those without a schema
 * @param table a table name  
 * @param unique when true, return only indices for unique values; 
 *     when false, return indices regardless of whether unique or not 
 * @param approximate when true, result is allowed to reflect approximate 
 *     or out of data values; when false, results are requested to be 
 *     accurate
 * @return ResultSet - each row is an index column description 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public java.sql.ResultSet getIndexInfo(String catalog, String schema, String table, boolean unique, boolean approximate) throws SQLException {
	String[] colNames = { "TABLE_CAT", "TABLE_SCHEM", "TABLE_NAME", "NON_UNIQUE", "INDEX_QUALIFIER", "INDEX_NAME", "TYPE", "ORDINAL_POSITION", "COLUMN_NAME",
			"ASC_OR_DESC", "CARDINALITY", "PAGES", "FILTER_CONDITION" };
	StringBuffer sbData;

	synchronized (connectMsg.getSock()) {
		if (Driver.traceOn()) Driver.trace("DatabaseMetaData@getIndexInfo : Entered");
		connectMsg.setFunc((FSMajorVersion > 2) ? CATALOG : GETATTR);
		StringBuffer ds = new StringBuffer("STATS");

		if (FSMajorVersion < 3) {
			dataAppend(ds, catalog);
			dataAppend(ds, schema);
		}
		dataAppend(ds, table);

		if (unique) ds.append(" 0");
		else ds.append(" 1");
		if (FSMajorVersion < 3) {
			if (approximate) ds.append(" 1");
			else ds.append(" 0");
		}

		connectMsg.setData(ds.toString());
		try {
			connectMsg.sendMessage();
			connectMsg.recvMessage();
		}
		catch (IOException ioex) {
			throw new SQLException(ioex.getLocalizedMessage());
		}
		String rtcd = connectMsg.getRtcd();
		if (Driver.traceOn()) Driver.trace("DatabaseMetaData@getIndexInfo : rtcd = " + rtcd);
		if (rtcd.startsWith("SET")) {
			String rawdata = new String(connectMsg.getData());
			if (rawdata == null || rawdata.length() == 0) {
				return new ResultSet(colNames); // FS 3
			}
			if (Driver.traceOn()) Driver.trace("DatabaseMetaData.getIndexInfo rawdata = '" + rawdata + "'");
			if (FSMajorVersion < 3) {
				StringTokenizer tok = new StringTokenizer(rawdata, "(", true);
				sbData = new StringBuffer(tok.nextToken());
				for (int i1 = 0; i1 < colNames.length; i1++) {
					sbData.append(colNames[i1]);
					sbData.append(tok.nextToken());
					sbData.append(tok.nextToken());
				}
			}
			else {
				StringTokenizer tok = new StringTokenizer(rawdata, " ");
				sbData = new StringBuffer(tok.nextToken()).append(' '); // result set ID
				sbData.append(tok.nextToken()).append(' '); // FS updatable
				sbData.append(tok.nextToken()).append(' '); // number of rows
				sbData.append(String.valueOf(colNames.length)).append(' '); // number of columns
				tok.nextToken(); // throw away FS number of columns
				sbData.append(colNames[0]).append("(C1) ");
				sbData.append(colNames[1]).append("(C1) ");
				sbData.append(colNames[2]).append(tok.nextToken()).append(' '); //TABLE_NAME
				sbData.append(colNames[3]).append(tok.nextToken()).append(' '); //NON_UNIQUE
				sbData.append(colNames[4]).append("(C1) "); //INDEX_QUALIFIER
				sbData.append(colNames[5]).append(tok.nextToken()).append(' '); //INDEX_NAME
				sbData.append(colNames[6]).append("(N1) "); //TYPE
				tok.nextToken(); // throw away FS description of this column
				sbData.append(colNames[7]).append(tok.nextToken()).append(' '); //ORDINAL_POSITION
				sbData.append(colNames[8]).append(tok.nextToken()).append(' '); //COLUMN_NAME
				sbData.append(colNames[9]).append(tok.nextToken()).append(' '); //ASC_OR_DESC
				sbData.append(colNames[10]).append("(N9) "); //CARDINALITY, not used
				sbData.append(colNames[11]).append("(N1) "); //PAGES, not used
				sbData.append(colNames[12]).append("(C1)"); //FILTER_CONDITION, not used
			}
			org.ptsw.fs.jdbc.ResultSet rs = new ResultSet(sbData.toString(), connectMsg, false, null);
			rs.setFSMajorVersion(FSMajorVersion);
			rs.setCatalogType(Driver.CATALOG_INDEXINFO);
			return rs;
		}
		else return new ResultSet(colNames);
	}
}

//--------------------------JDBC 2.0-----------------------------

/**
 * Does the database support the given result set type?
 *
 * @param type defined in <code>java.sql.ResultSet</code>
 * @return <code>true</code> if so; <code>false</code> otherwise
 * @exception SQLException if a database access error occurs
 * @see Connection
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0 API</a>
 */
@Override
public boolean supportsResultSetType(int type) throws SQLException {
	return (type == java.sql.ResultSet.TYPE_FORWARD_ONLY || type == java.sql.ResultSet.TYPE_SCROLL_INSENSITIVE);
}

/**
 * Does the database support the concurrency type in combination
 * with the given result set type?
 *
 * @param type defined in <code>java.sql.ResultSet</code>
 * @param concurrency type defined in <code>java.sql.ResultSet</code>
 * @return <code>true</code> if so; <code>false</code> otherwise
 * @exception SQLException if a database access error occurs
 * @see Connection
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0 API</a>
 */
@Override
public boolean supportsResultSetConcurrency(int type, int concurrency) throws SQLException {
	if (concurrency == java.sql.ResultSet.CONCUR_READ_ONLY) {
		if (type == java.sql.ResultSet.TYPE_FORWARD_ONLY || type == java.sql.ResultSet.TYPE_SCROLL_INSENSITIVE) return true;
		else return false;
	}
	else return false;
}

/**
 *
 * Indicates whether a result set's own updates are visible.
 *
 * @param result set type, i.e. ResultSet.TYPE_XXX
 * @return <code>true</code> if updates are visible for the result set type;
 *        <code>false</code> otherwise
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0 API</a>
 */
@Override
public boolean ownUpdatesAreVisible(int type) throws SQLException {
	return true;
}

/**
 *
 * Indicates whether a result set's own deletes are visible.
 *
 * @param result set type, i.e. ResultSet.TYPE_XXX
 * @return <code>true</code> if deletes are visible for the result set type;
 *        <code>false</code> otherwise
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0 API</a>
 */
@Override
public boolean ownDeletesAreVisible(int type) throws SQLException {
	return true;
}

/**
 *
 * Indicates whether a result set's own inserts are visible.
 *
 * @param result set type, i.e. ResultSet.TYPE_XXX
 * @return <code>true</code> if inserts are visible for the result set type;
 *        <code>false</code> otherwise
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0 API</a>
 */
@Override
public boolean ownInsertsAreVisible(int type) throws SQLException {
	return true;
}

/**
 *
 * Indicates whether updates made by others are visible.
 *
 * @param result set type, i.e. ResultSet.TYPE_XXX
 * @return <code>true</code> if updates made by others
 * are visible for the result set type;
 *        <code>false</code> otherwise
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0 API</a>
 */
@Override
public boolean othersUpdatesAreVisible(int type) throws SQLException {
	return false;
}

/**
 *
 * Indicates whether deletes made by others are visible.
 *
 * @param result set type, i.e. ResultSet.TYPE_XXX
 * @return <code>true</code> if deletes made by others
 * are visible for the result set type;
 *        <code>false</code> otherwise
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0 API</a>
 */
@Override
public boolean othersDeletesAreVisible(int type) throws SQLException {
	return false;
}

/**
 *
 * Indicates whether inserts made by others are visible.
 *
 * @param result set type, i.e. ResultSet.TYPE_XXX
 * @return true if updates are visible for the result set type
 * @return <code>true</code> if inserts made by others
 * are visible for the result set type;
 *        <code>false</code> otherwise
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0 API</a>
 */
@Override
public boolean othersInsertsAreVisible(int type) throws SQLException {
	return false;
}

/**
 * Indicates whether or not a visible row update can be detected by
 * calling the method <code>ResultSet.rowUpdated</code>.
 *
 * @param result set type, i.e. ResultSet.TYPE_XXX
 * @return <code>true</code> if changes are detected by the result set type;
 *         <code>false</code> otherwise
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0 API</a>
 */
@Override
public boolean updatesAreDetected(int type) throws SQLException {
	return false;
}

/**
 * Indicates whether or not a visible row delete can be detected by
 * calling ResultSet.rowDeleted().  If deletesAreDetected()
 * returns false, then deleted rows are removed from the result set.
 *
 * @param result set type, i.e. ResultSet.TYPE_XXX
 * @return true if changes are detected by the resultset type
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0 API</a>
 */
@Override
public boolean deletesAreDetected(int type) throws SQLException {
	return false;
}

/**
 * Indicates whether or not a visible row insert can be detected
 * by calling ResultSet.rowInserted().
 *
 * @param result set type, i.e. ResultSet.TYPE_XXX
 * @return true if changes are detected by the resultset type
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0 API</
 */
@Override
public boolean insertsAreDetected(int type) throws SQLException {
	return false;
}

/**
 * Indicates whether the driver supports batch updates.
 *
 * <P><B>FS2 NOTE:</B> The FS2 system does not support Batch Updates.  
 * This method will always return false.
 *
 * @return true if the driver supports batch updates; false otherwise
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0 API</a>
 */
@Override
public boolean supportsBatchUpdates() throws SQLException {
	return false;
}

/**
 *
 * Get a description of the user-defined types defined in a particular
 * schema.  Schema specific UDTs may have type JAVA_OBJECT, STRUCT, 
 * or DISTINCT.
 *
 * <P>Only types matching the catalog, schema, type name and type  
 * criteria are returned.  They are ordered by DATA_TYPE, TYPE_SCHEM 
 * and TYPE_NAME.  The type name parameter may be a fully qualified 
 * name.  In this case, the catalog and schemaPattern parameters are
 * ignored.
 *
 * <P>Each type description has the following columns:
 *  <OL>
 *	<LI><B>TYPE_CAT</B> String => the type's catalog (may be null)
 *	<LI><B>TYPE_SCHEM</B> String => type's schema (may be null)
 *	<LI><B>TYPE_NAME</B> String => type name
 *  <LI><B>CLASS_NAME</B> String => Java class name
 *	<LI><B>DATA_TYPE</B> String => type value defined in java.sql.Types.  
 *  One of JAVA_OBJECT, STRUCT, or DISTINCT
 *	<LI><B>REMARKS</B> String => explanatory comment on the type
 *  </OL>
 *
 * <P><B>Note:</B> If the driver does not support UDTs then an empty
 * result set is returned.
 *
 * @param catalog a catalog name; "" retrieves those without a
 *                catalog; null means drop catalog name from the selection criteria
 * @param schemaPattern a schema name pattern; "" retrieves those
 *                without a schema
 * @param typeNamePattern a type name pattern; may be a fully qualified
 *                name
 * @param types   a list of user-named types to include (JAVA_OBJECT, 
 *                STRUCT, or DISTINCT); null returns all types 
 * @return ResultSet - each row is a type description
 * @exception SQLException if a database-access error occurs.
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0 API</a>
 */
@Override
public java.sql.ResultSet getUDTs(String catalog, String schemaPattern, String typeNamePattern, int[] types) throws SQLException {
	String[] colNames = { "TYPE_CAT", "TYPE_SCHEM", "TYPE_NAME", "CLASS_NAME", "DATA_TYPE", "REMARKS" };

	return new ResultSet(colNames);
}

/**
 * Retrieves the connection that produced this metadata object.
 * <P>
 * @return the connection that produced this metadata object
 * @exception SQLException if a database access error occurs
 * @since 1.2
 */
@Override
public java.sql.Connection getConnection() throws SQLException {
	return con;
}

@Override
public boolean supportsSavepoints() throws SQLException {
	return false;
}

@Override
public boolean supportsNamedParameters() throws SQLException {
	return false;
}

@Override
public boolean supportsMultipleOpenResults() throws SQLException {
	return false;
}

@Override
public boolean supportsGetGeneratedKeys() throws SQLException {
	return false;
}

@Override
public java.sql.ResultSet getSuperTypes(String arg0, String arg1, String arg2) throws SQLException {
	return null;
}

@Override
public java.sql.ResultSet getSuperTables(String arg0, String arg1, String arg2) throws SQLException {
	return null;
}

@Override
public java.sql.ResultSet getAttributes(String arg0, String arg1, String arg2, String arg3) throws SQLException {
	return null;
}

@Override
public boolean supportsResultSetHoldability(int arg0) throws SQLException {
	return false;
}

@Override
public int getResultSetHoldability() throws SQLException {
	return 0;
}

@Override
public int getDatabaseMajorVersion() throws SQLException {
	return 0;
}

@Override
public int getDatabaseMinorVersion() throws SQLException {
	return 0;
}

@Override
public int getJDBCMajorVersion() throws SQLException {
	return 0;
}

@Override
public int getJDBCMinorVersion() throws SQLException {
	return 0;
}

@Override
public int getSQLStateType() throws SQLException {
	return 0;
}

@Override
public boolean locatorsUpdateCopy() throws SQLException {
	return false;
}

@Override
public boolean supportsStatementPooling() throws SQLException {
	return false;
}

@Override
public boolean autoCommitFailureClosesAllResultSets() throws SQLException {
	// TODO Auto-generated method stub
	return false;
}

@Override
public java.sql.ResultSet getClientInfoProperties() throws SQLException {
	// TODO Auto-generated method stub
	return null;
}

@Override
public java.sql.ResultSet getFunctionColumns(String catalog, String schemaPattern, String functionNamePattern, String columnNamePattern) throws SQLException {
	// TODO Auto-generated method stub
	return null;
}

@Override
public java.sql.ResultSet getFunctions(String catalog, String schemaPattern, String functionNamePattern) throws SQLException {
	// TODO Auto-generated method stub
	return null;
}

@Override
public java.sql.ResultSet getSchemas(String catalog, String schemaPattern) throws SQLException {
	// TODO Auto-generated method stub
	return null;
}

@Override
public boolean supportsStoredFunctionsUsingCallSyntax() throws SQLException {
	// TODO Auto-generated method stub
	return false;
}

@Override
public boolean isWrapperFor(Class<?> iface) throws SQLException {
	// TODO Auto-generated method stub
	return false;
}

@Override
public <T> T unwrap(Class<T> iface) throws SQLException {
	// TODO Auto-generated method stub
	return null;
}

@Override
public RowIdLifetime getRowIdLifetime() throws SQLException {
	return RowIdLifetime.ROWID_UNSUPPORTED;
}

@Override
public java.sql.ResultSet getPseudoColumns(String catalog, String schemaPattern, String tableNamePattern,
		String columnNamePattern) throws SQLException {
	// TODO Auto-generated method stub
	return null;
}

@Override
public boolean generatedKeyAlwaysReturned() throws SQLException {
	// TODO Auto-generated method stub
	return false;
}

}
