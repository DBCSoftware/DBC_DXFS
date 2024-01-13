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

import java.util.Hashtable;

class FSproperties {

private int version;
private static final Integer intzero = Integer.valueOf(0);
private static final Integer int31 = Integer.valueOf(31);
private static final Integer int15 = Integer.valueOf(15);
private static final Integer int50 = Integer.valueOf(50);
private static final Integer int100 = Integer.valueOf(100);
private static final Integer int32760 = Integer.valueOf(32760);
private static final Integer int65500 = Integer.valueOf(65500);

private static final Hashtable<String, Comparable<?>> props = new Hashtable<String, Comparable<?>>(101);
static {
	props.put("allProceduresAreCallable", Boolean.FALSE);
	props.put("allTablesAreSelectable", Boolean.TRUE);
	props.put("dataDefinitionCausesTransactionCommit", Boolean.FALSE);
	props.put("dataDefinitionIgnoredInTransactions", Boolean.FALSE);
	props.put("doesMaxRowSizeIncludeBlobs", Boolean.FALSE);
	props.put("getCatalogSeparator", "");
	props.put("getCatalogTerm", "");
	props.put("getDatabaseProductName", "DB/C FS");
	props.put("getDefaultTransactionIsolation", Integer.valueOf(java.sql.Connection.TRANSACTION_NONE));
	props.put("getExtraNameCharacters", "$");
	props.put("getIdentifierQuoteString", " ");
	props.put("getMaxBinaryLiteralLength", intzero);
	props.put("getMaxCatalogNameLength", intzero);
	props.put("getMaxCharLiteralLength", int65500);
	props.put("getMaxColumnNameLength", int31);
	props.put("getMaxColumnsInGroupBy", intzero);
	props.put("getMaxColumnsInIndex", int50);
	props.put("getMaxColumnsInOrderBy", int100);
	props.put("getMaxColumnsInSelect", intzero);
	props.put("getMaxColumnsInTable", intzero);
	props.put("getMaxCursorNameLength", int31);
	props.put("getMaxIndexLength", intzero);
	props.put("getMaxProcedureNameLength", intzero);
	props.put("getMaxRowSize", int32760);
	props.put("getMaxSchemaNameLength", intzero);
	props.put("getMaxStatementLength", intzero);
	props.put("getMaxTableNameLength", int31);
	props.put("getMaxTablesInSelect", int15);
	props.put("getMaxUserNameLength", int31);
	props.put("getNumericFunctions", "");
	props.put("getProcedureTerm", "procedure");
	props.put("getSchemaTerm", "");
	props.put("getSearchStringEscape", "");
	props.put("getSQLKeywords", "");
	props.put("getStringFunctions", "");
	props.put("getSystemFunctions", "");
	props.put("getTimeDateFunctions", "");
	props.put("isCatalogAtStart", Boolean.FALSE);
	props.put("nullPlusNonNullIsNull", Boolean.TRUE);
	props.put("nullsAreSortedAtEnd", Boolean.FALSE);
	props.put("nullsAreSortedAtStart", Boolean.FALSE);
	props.put("nullsAreSortedHigh", Boolean.FALSE);
	props.put("nullsAreSortedLow", Boolean.TRUE);
	props.put("supportsAlterTableWithAddColumn", Boolean.FALSE);
	props.put("supportsAlterTableWithDropColumn", Boolean.FALSE);
	props.put("supportsANSI92EntryLevelSQL", Boolean.TRUE);
	props.put("supportsANSI92FullSQL", Boolean.FALSE);
	props.put("supportsANSI92IntermediateSQL", Boolean.FALSE);
	props.put("supportsCatalogsInDataManipulation", Boolean.FALSE);
	props.put("supportsCatalogsInIndexDefinitions", Boolean.FALSE);
	props.put("supportsCatalogsInPrivilegeDefinitions", Boolean.FALSE);
	props.put("supportsCatalogsInProcedureCalls", Boolean.FALSE);
	props.put("supportsCatalogsInTableDefinitions", Boolean.FALSE);
	props.put("supportsColumnAliasing", Boolean.TRUE);
	props.put("supportsCoreSQLGrammar", Boolean.FALSE);
	props.put("supportsCorrelatedSubqueries", Boolean.FALSE);
	props.put("supportsDataDefinitionAndDataManipulationTransactions", Boolean.FALSE);
	props.put("supportsDataManipulationTransactionsOnly", Boolean.FALSE);
	props.put("supportsDifferentTableCorrelationNames", Boolean.FALSE);
	props.put("supportsExpressionsInOrderBy", Boolean.FALSE);
	props.put("supportsExtendedSQLGrammar", Boolean.FALSE);
	props.put("supportsFullOuterJoins", Boolean.FALSE);
	props.put("supportsGroupBy", Boolean.FALSE);
	props.put("supportsGroupByBeyondSelect", Boolean.FALSE);
	props.put("supportsGroupByUnrelated", Boolean.FALSE);
	props.put("supportsIntegrityEnhancementFacility", Boolean.FALSE);
	props.put("supportsLikeEscapeClause", Boolean.FALSE);
	props.put("supportsLimitedOuterJoins", Boolean.TRUE);
	props.put("supportsMinimumSQLGrammar", Boolean.TRUE);
	props.put("supportsMultipleResultSets", Boolean.FALSE);
	props.put("supportsMultipleTransactions", Boolean.FALSE);
	props.put("supportsOrderByUnrelated", Boolean.FALSE);
	props.put("supportsOuterJoins", Boolean.TRUE);
	props.put("supportsPositionedDelete", Boolean.FALSE);
	props.put("supportsPositionedUpdate", Boolean.FALSE);
	props.put("supportsSchemasInDataManipulation", Boolean.FALSE);
	props.put("supportsSchemasInIndexDefinitions", Boolean.FALSE);
	props.put("supportsSchemasInPrivilegeDefinitions", Boolean.FALSE);
	props.put("supportsSchemasInProcedureCalls", Boolean.FALSE);
	props.put("supportsSchemasInTableDefinitions", Boolean.FALSE);
	props.put("supportsSubqueriesInComparisons", Boolean.FALSE);
	props.put("supportsSubqueriesInExists", Boolean.FALSE);
	props.put("supportsSubqueriesInIns", Boolean.FALSE);
	props.put("supportsSubqueriesInQuantifieds", Boolean.FALSE);
	props.put("supportsTableCorrelationNames", Boolean.TRUE);
	props.put("supportsUnion", Boolean.FALSE);
	props.put("supportsUnionAll", Boolean.FALSE);
}

private static final Hashtable<String, Comparable<?>> props3 = new Hashtable<String, Comparable<?>>(13);
static {
	props3.put("getNumericFunctions", "AVG,MIN,MAX,COUNT,SUM");
	props3.put("supportsOrderByUnrelated", Boolean.TRUE);
	props3.put("supportsPositionedDelete", Boolean.TRUE);
	props3.put("supportsPositionedUpdate", Boolean.TRUE);
}

FSproperties(int ver) {
	version = ver;
}

boolean getBoolean(String key) {
	Object o = getValue(key);
	if (o instanceof Boolean) return ((Boolean) o).booleanValue();
	else throw new IllegalArgumentException("'" + key + "' is not a Boolean value");
}

String getString(String key) {
	Object o = getValue(key);
	if (o instanceof String) return (String) o;
	else throw new IllegalArgumentException("'" + key + "' is not a String value");
}

int getInt(String key) {
	Object o = getValue(key);
	if (o instanceof Integer) return ((Integer) o).intValue();
	else throw new IllegalArgumentException("'" + key + "' is not an int value");
}

private Object getValue(String key) {
	Object o = null;
	if (version > 2 && props3.containsKey(key)) {
		o = props3.get(key);
	}
	else if (props.containsKey(key)) {
		o = props.get(key);
	}
	if (o == null) throw new IllegalArgumentException("'" + key + "' is not in FS properties table");
	return o;
}

}
