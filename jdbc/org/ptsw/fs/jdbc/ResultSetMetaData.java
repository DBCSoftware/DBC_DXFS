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

import java.sql.SQLException;
import java.sql.Types;

/**
 * An object that can be used to get information about the types 
 * and properties of the columns in a <code>ResultSet</code> object.
 * The following code fragment creates the <code>ResultSet</code> object rs,
 * creates the <code>ResultSetMetaData</code> object rsmd, and uses rsmd
 * to find out how many columns rs has and whether the first column in rs
 * can be used in a <code>WHERE</code> clause.
 * <PRE>
 *
 *     ResultSet rs = stmt.executeQuery("SELECT a, b, c FROM TABLE2");
 *     ResultSetMetaData rsmd = rs.getMetaData();
 *     int numberOfColumns = rsmd.getColumnCount();
 *     boolean b = rsmd.isSearchable(1);
 *
 * </PRE>
 */

public final class ResultSetMetaData implements java.sql.ResultSetMetaData {

private boolean realData; //Meta-Data or Real Data?
private ResultSet rs; //Who made me?

ResultSetMetaData(ResultSet rs, boolean rd) {
	this.rs = rs;
	realData = rd;
	if (Driver.traceOn()) Driver.trace("ResultSetMetaData() realData = " + realData);
}

/**
 * What's a column's table's catalog name?
 *
 * <P><B>FS Note:</B> The FS system does not support catalogs.
 *
 * @param column the first column is 1, the second is 2, ...
 * @return column name or "" if not applicable.
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getCatalogName(int column) throws SQLException {
	String s = "";
	if (Driver.traceOn()) Driver.trace("ResultSetMetaData@getCatalogName s = " + s);
	return s;
}

/**
 * What's the number of columns in the ResultSet?
 *
 * @return the number
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getColumnCount() throws SQLException {
	return rs.getNumCols();
}

/**
 * What's the column's normal max width in chars?
 *
 * @param column the first column is 1, the second is 2, ...
 * @return max width
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getColumnDisplaySize(int column) throws SQLException {
	return rs.getDisplayWidth(column - 1);
}

/**
 * What's the suggested column title for use in printouts and
 * displays?
 *
 * @param column the first column is 1, the second is 2, ...
 * @return a String
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getColumnLabel(int column) throws SQLException {
	String label = rs.getColumnLabel(column - 1);
	if (label != null) return label;
	label = rs.getColumnAlias(column - 1);
	return (label == null) ? rs.getColumnName(column - 1) : label;
}

/**
 * What's the column's name?
 *
 * @param column the first column is 1, the second is 2, ...
 * @return column name
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getColumnName(int column) throws SQLException {
	String label = rs.getColumnAlias(column - 1);
	return (label == null) ? rs.getColumnName(column - 1) : label;
}

/**
 * What's a column's SQL type?
 *
 * @param column the first column is 1, the second is 2, ...
 * @return SQL type
 * @exception SQLException if a database-access error occurs.
 * @see Types
 */
@Override
public int getColumnType(int column) throws SQLException {
	switch (rs.getColType(column - 1)) {
	case 'C':
		return java.sql.Types.CHAR;
	case 'N':
		return java.sql.Types.NUMERIC;
	case 'D':
		return java.sql.Types.DATE;
	case 'T':
		return java.sql.Types.TIME;
	case 'S':
		return java.sql.Types.TIMESTAMP;
	default:
		return java.sql.Types.OTHER;
	}
}

/**
 * What's a column's data source specific type name?
 *
 * @param column the first column is 1, the second is 2, ...
 * @return type name
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getColumnTypeName(int column) throws SQLException {
	return rs.getColumnTypeName(column - 1);
}

/**
 * What's a column's number of decimal digits?
 *
 * @param column the first column is 1, the second is 2, ...
 * @return precision
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getPrecision(int column) throws SQLException {
	return rs.getPrecision(column - 1);
}

/**
 * What's a column's number of digits to right of the decimal point?
 *
 * @param column the first column is 1, the second is 2, ...
 * @return scale
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int getScale(int column) throws SQLException {
	return rs.getScale(column - 1);
}

/**
 * What's a column's table's schema?
 *
 * <P><B>FS Note:</B> The FS system does not support schemas.
 *
 * @param column the first column is 1, the second is 2, ...
 * @return schema name or "" if not applicable
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getSchemaName(int column) throws SQLException {
	String s = "";
	if (Driver.traceOn()) Driver.trace("ResultSetMetaData@getSchemaName s = " + s);
	return s;
}

/**
 * What's a column's table name? 
 *
 * @return table name or "" if not applicable
 * @exception SQLException if a database-access error occurs.
 */
@Override
public String getTableName(int column) throws SQLException {
	String s = rs.getTableName(column - 1);
	if (Driver.traceOn()) Driver.trace("ResultSetMetaData@getTableName s = " + s);
	return (s == null) ? "" : s;
}

/**
 * Is the column automatically numbered, thus read-only?
 *
 * NOTE FS does not support auto-increment columns.
 *
 * @param column the first column is 1, the second is 2, ...
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean isAutoIncrement(int column) throws SQLException {
	return false;
}

/**
 * Does a column's case matter?
 *
 * @param column the first column is 1, the second is 2, ...
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean isCaseSensitive(int column) throws SQLException {
	return false;
}

/**
 * Is the column a cash value?
 *
 * <P><B>FS2 NOTE:</B> FS2 does not have a CURRENCY or MONEY type so we can't tell
 * if a numeric value is being used for money or not.
 * Always use DECIMAL or NUMERIC (they are identical) for 
 * monetary values.
 *
 * @param column the first column is 1, the second is 2, ...
 * @return always false
 */
@Override
public boolean isCurrency(int column) {
	return false;
}

/**
 * Will a write on the column definitely succeed?	
 *
 * @param column the first column is 1, the second is 2, ...
 * @return always true
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean isDefinitelyWritable(int column) throws SQLException {
	return true;
}

/**
 * Can you put a NULL in this column?		
 *
 * @param column the first column is 1, the second is 2, ...
 * @return columnNoNulls, columnNullable or columnNullableUnknown
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int isNullable(int column) throws SQLException {

	switch (rs.getColType(column - 1)) {
	case 'D':
	case 'N':
	case 'T':
	case 'S':
		return columnNullable;
	case 'C':
		return columnNoNulls;
	default:
		return columnNullableUnknown;
	}
}

/**
 * Is a column definitely not writable?
 *
 * <P><B>FS2 NOTE</B> All columns are writable.
 *
 * @param column the first column is 1, the second is 2, ...
 * @return always true
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean isReadOnly(int column) throws SQLException {
	return false;
}

/**
 * Can the column be used in a where clause?
 *
 * <P><B>FS NOTE:</B> All columns can be used in a where clause.
 *
 * @param column the first column is 1, the second is 2, ...
 * @return always true
 */
@Override
public boolean isSearchable(int column) {
	return true;
}

/**
 * Is the column a signed number?
 *
 * @param column the first column is 1, the second is 2, ...
 * @return true if so
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean isSigned(int column) throws SQLException {

	if (getColumnType(column) == java.sql.Types.NUMERIC) return true;
	else return false;
}

/**
 * Is it possible for a write on the column to succeed?
 *
 * <P><B>FS NOTE</B> All columns are writable.
 *
 * @param column the first column is 1, the second is 2, ...
 * @return always true
 * @exception SQLException if a database-access error occurs.
 */
@Override
public boolean isWritable(int column) throws SQLException {
	return true;
}

//--------------------------JDBC 2.0-----------------------------------

/**
 * <p>Returns the fully-qualified name of the Java class whose instances
 * are manufactured if the method <code>ResultSet.getObject</code>
 * is called to retrieve a value
 * from the column.  <code>ResultSet.getObject</code> may return a subclass of the
 * class returned by this method.
 *
 * @return the fully-qualified name of the class in the Java programming
 *         language that would be used by the method
 * <code>ResultSet.getObject</code> to retrieve the value in the specified
 * column. This is the class name used for custom mapping.
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC
 *      2.0 API</a>
 */
@Override
public String getColumnClassName(int column) throws SQLException {
	switch (rs.getColType(column - 1)) {
	case 'C':
		return new String("java.lang.String");
	case 'N':
		return new String("java.math.BigDecimal");
	case 'D':
		return new String("java.sql.Date");
	case 'T':
		return new String("java.sql.Time");
	case 'S':
		return new String("java.sql.Timestamp");
	default:
		return new String("java.lang.String");
	}
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

}
