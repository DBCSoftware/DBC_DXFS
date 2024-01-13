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
import java.util.Map;

/**
 * JDBC 2.0
 * 
 * <p>
 * SQL arrays are mapped to the Array interface. By default, an Array is a
 * transaction duration reference to an SQL array. By default, an Array is
 * implemented using an SQL LOCATOR(array) internally.
 * 
 * <P>
 * <B>FS2 NOTE:</B> FS2 does not support the Array data type.
 * 
 */

public final class Array implements java.sql.Array {

	/**
	 * Returns the SQL type name of the elements in the array designated by this
	 * <code>Array</code> object. If the elements are a built-in type, it
	 * returns the database-specific type name of the elements. If the elements
	 * are a user-defined type (UDT), this method returns the fully-qualified
	 * SQL type name.
	 * 
	 * @return a <code>String</code> that is the database-specific name for a
	 *         built-in base type or the fully-qualified SQL type name for a
	 *         base type that is a UDT
	 * @exception SQLException
	 *                if an error occurs while attempting to access the type
	 *                name
	 * @since 1.2
	 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0
	 *      API</a>
	 * 
	 *      <P>
	 *      <B>FS2 NOTE:</B> FS2 does not support the Array data type.
	 * 
	 */
	@Override
	public String getBaseTypeName() throws SQLException {
		throw new SQLException(Driver.bun.getString("NoType") + " : Array");
	}

	/**
	 * Determine the code, from java.sql.Types, of the type of the elements of
	 * the array.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> FS2 does not support the Array data type.
	 * 
	 * @return the type code of the elements of the array.
	 */
	@Override
	public int getBaseType() throws SQLException {
		throw new SQLException(Driver.bun.getString("NoType") + " : Array");
	}

	/**
	 * Retrieve the contents of the SQL array designated by the object. Use the
	 * type-map associated with the connection for customizations of the
	 * type-mappings.
	 * 
	 * Conceptually, this method calls getObject() on each element of the array
	 * and returns a Java array containing the result. Except when the array
	 * element type maps to a Java primitive type, such as int, boolean, etc. In
	 * this case, an array of primitive type values, i.e. an array of int, is
	 * returned, not an array of Integer. This exception for primitive types
	 * should improve performance as well as usability.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> FS2 does not support the Array data type.
	 * 
	 * @return a Java array containing the ordered elements of the SQL array
	 *         designated by this object.
	 * @exception SQLException
	 *                if an error occurs while attempting to access the array
	 * @since 1.2
	 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0
	 *      API</a>
	 */
	@Override
	public Object getArray() throws SQLException {
		throw new SQLException(Driver.bun.getString("NoType") + " : Array");
	}

	/**
	 * Retrieve the contents of the SQL array designated by this object. Use the
	 * given @map for type-map customizations.
	 * 
	 * Conceptually, this method calls getObject() on each element of the array
	 * and returns a Java array containing the result. Except when the array
	 * element type maps to a Java primitive type, such as int, boolean, etc. In
	 * this case, an array of primitive type values, i.e. an array of int, is
	 * returned, not an array of Integer. This exception for primitive types
	 * should improve performance as well as usability.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> FS2 does not support the Array data type.
	 * 
	 * @param map
	 *            contains mapping of SQL type names to Java classes
	 * @return a Java array containing the ordered elements of the SQL array
	 *         designated by this object.
	 */
	@Override
	public Object getArray(Map<String, Class<?>> map) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoType") + " : Array");
	}

	/**
	 * Like getArray() above, but returns an array containing a slice of the SQL
	 * array, beginning with the given @index and containing up to @count
	 * successive elements of the SQL array. Use the type-map associated with
	 * the connection for customizations of the type-mappings.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> FS2 does not support the Array data type.
	 * 
	 * @index the array-index of the first element to retrieve
	 * @count the number of successive SQL array elements to retrieve
	 * @return an array containing up to @count elements of the SQL array,
	 *         beginning with element @index.
	 */
	@Override
	public Object getArray(long index, int count) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoType") + " : Array");
	}

	/**
	 * Like getArray() above, but returns an array containing a slice of the SQL
	 * array, beginning with the given @index and containing up to @count
	 * successive elements of the SQL array. Use the given @map for type-map
	 * customizations.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> FS2 does not support the Array data type.
	 * 
	 * @index the array-index of the first element to retrieve
	 * @count the number of successive SQL array elements to retrieve
	 * @param map
	 *            contains mapping of SQL user-defined types to classes
	 * @return an array containing up to @count elements of the SQL array,
	 *         beginning with element @index.
	 */
	@Override
	public Object getArray(long index, int count, Map<String, Class<?>> map) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoType") + " : Array");
	}

	/**
	 * Materialize the item designated by the Array as a ResultSet that contains
	 * a row for each element of the Array. The first column of each row
	 * contains the array index of the corresponding element in the Array. The
	 * second column contains the array element value. The rows are ordered in
	 * ascending order of the array-element indexes.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> FS2 does not support the Array data type.
	 * 
	 * @return a result set containing the elements of the array
	 */
	@Override
	public java.sql.ResultSet getResultSet() throws SQLException {
		throw new SQLException(Driver.bun.getString("NoType") + " : Array");
	}

	/**
	 * Materialize the item designated by the Array as a ResultSet that contains
	 * a row for each element of the Array. The first column of each row
	 * contains the array index of the corresponding element in the Array. The
	 * second column contains the array element value. The rows are ordered in
	 * ascending order of the array-element indexes. Use the given @map for
	 * type-map customizations.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> FS2 does not support the Array data type.
	 * 
	 * @param map
	 *            contains mapping of SQL user-defined types to classes
	 * @return a result set containing the elements of the array
	 */
	@Override
	public java.sql.ResultSet getResultSet(Map<String, Class<?>> map) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoType") + " : Array");
	}

	/**
	 * Materialize the designated sub-array as a ResultSet that contains a row
	 * for each element of the sub-array. The first column of each row contains
	 * the array index of the corresponding element in the Array. The second
	 * column contains the array element value. The rows are ordered in
	 * ascending order of the array-element indexes.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> FS2 does not support the Array data type.
	 * 
	 * @param index
	 *            the index of the first element to retrieve
	 * @param count
	 *            the number of successive SQL array elements to retrieve
	 * @return a result set containing the elements of the array
	 */
	@Override
	public java.sql.ResultSet getResultSet(long index, int count) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoType") + " : Array");
	}

	/**
	 * Materialize the designated sub-array as a ResultSet that contains a row
	 * for each element of the sub-array. The first column of each row contains
	 * the array index of the corresponding element in the Array. The second
	 * column contains the array element value. The rows are ordered in
	 * ascending order of the array-element indexes. Use the given @map for
	 * type-map customizations.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> FS2 does not support the Array data type.
	 * 
	 * @param index
	 *            the index of the first element to retrieve
	 * @param count
	 *            the number of successive SQL array elements to retrieve
	 * @param map
	 *            contains mapping of SQL user-defined types to classes
	 * 
	 * @return a result set containing the elements of the array
	 */
	@Override
	public java.sql.ResultSet getResultSet(long index, int count, Map<String, Class<?>> map) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoType") + " : Array");
	}

	@Override
	public void free() throws SQLException {
		throw new SQLException();

	}

}
