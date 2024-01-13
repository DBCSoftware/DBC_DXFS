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

import java.io.InputStream;
import java.io.Reader;
import java.math.BigDecimal;
import java.net.URL;
import java.sql.*;
import java.sql.Blob;
import java.sql.Clob;
import java.util.Calendar;
import java.util.Map;

/**
 * <P>CallableStatement is used to execute SQL stored procedures.
 *
 * <P>JDBC provides a stored procedure SQL escape that allows stored
 * procedures to be called in a standard way for all RDBMS's. This
 * escape syntax has one form that includes a result parameter and one
 * that does not. If used, the result parameter must be registered as
 * an OUT parameter. The other parameters may be used for input,
 * output or both. Parameters are refered to sequentially, by
 * number. The first parameter is 1.
 *
 * <P><blockquote><pre>
 *   {?= call &lt;procedure-name&gt;[&lt;arg1&gt;,&lt;arg2&gt;, ...]}
 *   {call &lt;procedure-name&gt;[&lt;arg1&gt;,&lt;arg2&gt;, ...]}
 * </pre></blockquote>
 *    
 * <P>IN parameter values are set using the set methods inherited from
 * PreparedStatement. The type of all OUT parameters must be
 * registered prior to executing the stored procedure; their values
 * are retrieved after execution via the get methods provided here.
 *
 * <P>A Callable statement may return a ResultSet or multiple
 * ResultSets. Multiple ResultSets are handled using operations
 * inherited from Statement.
 *
 * <P>For maximum portability, a call's ResultSets and update counts
 * should be processed prior to getting the values of output
 * parameters.
 *
 * <P><B>FS2 NOTE:</B> The FS2 system does not support Callable Statements.  
 * Attempting to use this class will always result in an SQLException.
 *
 * @see Connection#prepareCall
 * @see ResultSet 
 */

public final class CallableStatement extends PreparedStatement implements
		java.sql.CallableStatement {

	CallableStatement(String sql, Message msg, Connection con) throws SQLException {
		super(sql, msg, con);
	}

	/**
	 * Before executing a stored procedure call, you must explicitly call registerOutParameter to register the
	 * java.sql.Type of each out parameter.
	 * 
	 * <P>
	 * <B>Note:</B> When reading the value of an out parameter, you must use the getXXX method whose Java type XXX
	 * corresponds to the parameter's registered SQL type.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method
	 * will always result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2,...
	 * @param sqlType
	 *            SQL type code defined by java.sql.Types; for parameters of type Numeric or Decimal use the
	 *            version of registerOutParameter that accepts a scale value
	 * @exception SQLException
	 *                always.
	 * @see Type
	 */
	@Override
	public void registerOutParameter(int parameterIndex, int sqlType) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Use this version of registerOutParameter for registering Numeric or Decimal out parameters.
	 * 
	 * <P>
	 * <B>Note:</B> When reading the value of an out parameter, you must use the getXXX method whose Java type XXX
	 * corresponds to the parameter's registered SQL type.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @param sqlType
	 *            use either java.sql.Type.NUMERIC or java.sql.Type.DECIMAL
	 * @param scale
	 *            a value greater than or equal to zero representing the desired number of digits to the right of the
	 *            decimal point
	 * @exception SQLException
	 *                always.
	 * @see Type
	 */
	@Override
	public void registerOutParameter(int parameterIndex, int sqlType, int scale)
			throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Use this version of registerOutParameter for registering Numeric or Decimal out parameters.
	 * 
	 * <P>
	 * <B>Note:</B> When reading the value of an out parameter, you must use the getXXX method whose Java type XXX
	 * corresponds to the parameter's registered SQL type.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @param sqlType
	 *            use either java.sql.Type.NUMERIC or java.sql.Type.DECIMAL
	 * @param typeName
	 *            a fully qualified name of an SQL structured type
	 * 
	 * @exception SQLException
	 *                always.
	 * @see Type
	 */
	@Override
	public void registerOutParameter(int parameterIndex, int sqlType, String typeName)
			throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * An OUT parameter may have the value of SQL NULL; wasNull reports whether the last value read has this special
	 * value.
	 * 
	 * <P>
	 * <B>Note:</B> You must first call getXXX on a parameter to read its value and then call wasNull() to see if the
	 * value was SQL NULL.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @return true if the last parameter read was SQL NULL
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public boolean wasNull() throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Get the value of a CHAR, VARCHAR, or LONGVARCHAR parameter as a Java String.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @return the parameter value; if the value is SQL NULL, the result is null
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public String getString(int parameterIndex) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Get the value of a BIT parameter as a Java boolean.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @return the parameter value; if the value is SQL NULL, the result is false
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public boolean getBoolean(int parameterIndex) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Get the value of a TINYINT parameter as a Java byte.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @return the parameter value; if the value is SQL NULL, the result is 0
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public byte getByte(int parameterIndex) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Get the value of a SMALLINT parameter as a Java short.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @return the parameter value; if the value is SQL NULL, the result is 0
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public short getShort(int parameterIndex) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Get the value of an INTEGER parameter as a Java int.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @return the parameter value; if the value is SQL NULL, the result is 0
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public int getInt(int parameterIndex) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Get the value of a BIGINT parameter as a Java long.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @return the parameter value; if the value is SQL NULL, the result is 0
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public long getLong(int parameterIndex) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Get the value of a FLOAT parameter as a Java float.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @return the parameter value; if the value is SQL NULL, the result is 0
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public float getFloat(int parameterIndex) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Get the value of a DOUBLE parameter as a Java double.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @return the parameter value; if the value is SQL NULL, the result is 0
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public double getDouble(int parameterIndex) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Get the value of a NUMERIC parameter as a java.math.BigDecimal object.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * 
	 * @param scale
	 *            a value greater than or equal to zero representing the desired number of digits to the right of the
	 *            decimal point
	 * 
	 * @return the parameter value; if the value is SQL NULL, the result is null
	 * @exception SQLException
	 *                always.
	 * @deprecated
	 */
	@Deprecated
	@Override
	public BigDecimal getBigDecimal(int parameterIndex, int scale) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Get the value of a SQL BINARY or VARBINARY parameter as a Java byte[]
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @return the parameter value; if the value is SQL NULL, the result is null
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public byte[] getBytes(int parameterIndex) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Get the value of a SQL DATE parameter as a java.sql.Date object
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @return the parameter value; if the value is SQL NULL, the result is null
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public java.sql.Date getDate(int parameterIndex) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Get the value of a SQL TIME parameter as a java.sql.Time object.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @return the parameter value; if the value is SQL NULL, the result is null
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public java.sql.Time getTime(int parameterIndex) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Get the value of a SQL TIMESTAMP parameter as a java.sql.Timestamp object.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @return the parameter value; if the value is SQL NULL, the result is null
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public java.sql.Timestamp getTimestamp(int parameterIndex) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	// ----------------------------------------------------------------------
	// Advanced features:

	/**
	 * Get the value of a parameter as a Java object.
	 * 
	 * <p>
	 * This method returns a Java object whose type coresponds to the SQL type that was registered for this parameter
	 * using registerOutParameter.
	 * 
	 * <p>
	 * Note that this method may be used to read datatabase-specific, abstract data types. This is done by specifying a
	 * targetSqlType of java.sql.types.OTHER, which allows the driver to return a database-specific Java type.
	 * 
	 * JDBC 2.0
	 * 
	 * The behavior of method getObject() is extended to materialize data of SQL user-defined types. When the OUT
	 * parameter @i is a UDT value, the behavior of this method is as if it were a call to: getObject(i,
	 * this.getConnection().getTypeMap())
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            The first parameter is 1, the second is 2, ...
	 * @return A java.lang.Object holding the OUT parameter value.
	 * @exception SQLException
	 *                always.
	 * @see Types
	 */
	@Override
	public Object getObject(int parameterIndex) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	// --------------------------JDBC 2.0-----------------------------

	/**
	 * JDBC 2.0
	 * 
	 * Get the value of a NUMERIC parameter as a java.math.BigDecimal object.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @return the parameter value (full precision); if the value is SQL NULL, the result is null
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public BigDecimal getBigDecimal(int parameterIndex) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * JDBC 2.0
	 * 
	 * Returns an object representing the value of OUT parameter @i. Use the @map to determine the class from which to
	 * construct data of SQL structured and distinct types.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param i
	 *            the first parameter is 1, the second is 2, ...
	 * @param map
	 *            the mapping from SQL type names to Java classes
	 * @return a java.lang.Object holding the OUT parameter value.
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public Object getObject(int i, Map<String, Class<?>> map) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * JDBC 2.0
	 * 
	 * Get a REF(&lt;structured-type&gt;) OUT parameter.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param i
	 *            the first parameter is 1, the second is 2, ...
	 * @return an object representing data of an SQL REF Type
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public java.sql.Ref getRef(int i) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * JDBC 2.0
	 * 
	 * Get a BLOB OUT parameter.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param i
	 *            the first parameter is 1, the second is 2, ...
	 * @return an object representing a BLOB
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public java.sql.Blob getBlob(int i) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * JDBC 2.0
	 * 
	 * Get a CLOB OUT parameter.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param i
	 *            the first parameter is 1, the second is 2, ...
	 * @return an object representing a CLOB
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public java.sql.Clob getClob(int i) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * JDBC 2.0
	 * 
	 * Get an Array OUT parameter.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param i
	 *            the first parameter is 1, the second is 2, ...
	 * @return an object representing an SQL array
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public java.sql.Array getArray(int i) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Get the value of a SQL DATE parameter as a java.sql.Date object
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @return the parameter value; if the value is SQL NULL, the result is null
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public java.sql.Date getDate(int parameterIndex, Calendar cal) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Get the value of a SQL TIME parameter as a java.sql.Time object.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @return the parameter value; if the value is SQL NULL, the result is null
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public java.sql.Time getTime(int parameterIndex, Calendar cal) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Get the value of a SQL TIMESTAMP parameter as a java.sql.Timestamp object.
	 * 
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support Callable Statements. Attempting to use this method will always
	 * result in an SQLException.
	 * 
	 * @param parameterIndex
	 *            the first parameter is 1, the second is 2, ...
	 * @return the parameter value; if the value is SQL NULL, the result is null
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public java.sql.Timestamp getTimestamp(int parameterIndex, Calendar cal)
			throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	@Override
	public void registerOutParameter(String arg0, int arg1) throws SQLException {
	}

	@Override
	public void registerOutParameter(String arg0, int arg1, int arg2) throws SQLException {
	}

	@Override
	public void registerOutParameter(String arg0, int arg1, String arg2)
			throws SQLException {
	}

	@Override
	public URL getURL(int arg0) throws SQLException {
		return null;
	}

	@Override
	public void setURL(String arg0, URL arg1) throws SQLException {
	}

	@Override
	public void setNull(String arg0, int arg1) throws SQLException {
	}

	@Override
	public void setBoolean(String arg0, boolean arg1) throws SQLException {
	}

	@Override
	public void setByte(String arg0, byte arg1) throws SQLException {
	}

	@Override
	public void setShort(String arg0, short arg1) throws SQLException {
	}

	@Override
	public void setInt(String arg0, int arg1) throws SQLException {
	}

	@Override
	public void setLong(String arg0, long arg1) throws SQLException {
	}

	@Override
	public void setFloat(String arg0, float arg1) throws SQLException {
	}

	@Override
	public void setDouble(String arg0, double arg1) throws SQLException {
	}

	@Override
	public void setBigDecimal(String arg0, BigDecimal arg1) throws SQLException {
	}

	@Override
	public void setString(String arg0, String arg1) throws SQLException {
	}

	@Override
	public void setBytes(String arg0, byte[] arg1) throws SQLException {
	}

	@Override
	public void setTime(String arg0, Time arg1) throws SQLException {
	}

	@Override
	public void setTimestamp(String arg0, Timestamp arg1) throws SQLException {
	}

	@Override
	public void setAsciiStream(String arg0, InputStream arg1, int arg2)
			throws SQLException {
	}

	@Override
	public void setBinaryStream(String arg0, InputStream arg1, int arg2)
			throws SQLException {
	}

	@Override
	public void setObject(String arg0, Object arg1, int arg2, int arg3)
			throws SQLException {
	}

	@Override
	public void setObject(String arg0, Object arg1, int arg2) throws SQLException {
	}

	@Override
	public void setObject(String arg0, Object arg1) throws SQLException {
	}

	@Override
	public void setCharacterStream(String arg0, Reader arg1, int arg2)
			throws SQLException {
	}

	@Override
	public void setTime(String arg0, Time arg1, Calendar arg2) throws SQLException {
	}

	@Override
	public void setTimestamp(String arg0, Timestamp arg1, Calendar arg2)
			throws SQLException {
	}

	@Override
	public void setNull(String arg0, int arg1, String arg2) throws SQLException {
	}

	@Override
	public String getString(String arg0) throws SQLException {
		return null;
	}

	@Override
	public boolean getBoolean(String arg0) throws SQLException {
		return false;
	}

	@Override
	public byte getByte(String arg0) throws SQLException {
		return 0;
	}

	@Override
	public short getShort(String arg0) throws SQLException {
		return 0;
	}

	@Override
	public int getInt(String arg0) throws SQLException {
		return 0;
	}

	@Override
	public long getLong(String arg0) throws SQLException {
		return 0;
	}

	@Override
	public float getFloat(String arg0) throws SQLException {
		return 0;
	}

	@Override
	public double getDouble(String arg0) throws SQLException {
		return 0;
	}

	@Override
	public byte[] getBytes(String arg0) throws SQLException {
		return null;
	}

	@Override
	public Time getTime(String arg0) throws SQLException {
		return null;
	}

	@Override
	public Timestamp getTimestamp(String arg0) throws SQLException {
		return null;
	}

	@Override
	public Object getObject(String arg0) throws SQLException {
		return null;
	}

	@Override
	public BigDecimal getBigDecimal(String arg0) throws SQLException {
		return null;
	}

	@Override
	public Object getObject(String arg0, Map<String, Class<?>> arg1) throws SQLException {
		return null;
	}

	@Override
	public java.sql.Array getArray(String arg0) throws SQLException {
		return null;
	}

	@Override
	public java.sql.Blob getBlob(String arg0) throws SQLException {
		return null;
	}

	@Override
	public java.sql.Clob getClob(String arg0) throws SQLException {
		return null;
	}

	@Override
	public java.sql.Ref getRef(String arg0) throws SQLException {
		return null;
	}

	@Override
	public Time getTime(String arg0, Calendar arg1) throws SQLException {
		return null;
	}

	@Override
	public Timestamp getTimestamp(String arg0, Calendar arg1) throws SQLException {
		return null;
	}

	@Override
	public URL getURL(String arg0) throws SQLException {
		return null;
	}

	@Override
	public void setURL(int arg0, URL arg1) throws SQLException {
	}

	@Override
	public ParameterMetaData getParameterMetaData() throws SQLException {
		return null;
	}

	@Override
	public boolean getMoreResults(int arg0) throws SQLException {
		return false;
	}

	@Override
	public java.sql.ResultSet getGeneratedKeys() throws SQLException {
		return null;
	}

	@Override
	public int executeUpdate(String arg0, int arg1) throws SQLException {
		return 0;
	}

	@Override
	public int executeUpdate(String arg0, int[] arg1) throws SQLException {
		return 0;
	}

	@Override
	public int executeUpdate(String arg0, String[] arg1) throws SQLException {
		return 0;
	}

	@Override
	public boolean execute(String arg0, int arg1) throws SQLException {
		return false;
	}

	@Override
	public boolean execute(String arg0, int[] arg1) throws SQLException {
		return false;
	}

	@Override
	public boolean execute(String arg0, String[] arg1) throws SQLException {
		return false;
	}

	@Override
	public int getResultSetHoldability() throws SQLException {
		return 0;
	}

	@Override
	public void setDate(String arg0, Date arg1) throws SQLException {
	}

	@Override
	public void setDate(String arg0, Date arg1, Calendar arg2) throws SQLException {
	}

	@Override
	public Date getDate(String arg0) throws SQLException {
		return null;
	}

	@Override
	public Date getDate(String arg0, Calendar arg1) throws SQLException {
		return null;
	}

	@Override
	public Reader getCharacterStream(int parameterIndex) throws SQLException {
		return null;
	}

	@Override
	public Reader getCharacterStream(String parameterName) throws SQLException {
		return null;
	}

	@Override
	public Reader getNCharacterStream(int parameterIndex) throws SQLException {
		return null;
	}

	@Override
	public Reader getNCharacterStream(String parameterName) throws SQLException {
		return null;
	}

	@Override
	public String getNString(int parameterIndex) throws SQLException {
		return null;
	}

	@Override
	public String getNString(String parameterName) throws SQLException {
		return null;
	}

	@Override
	public void setAsciiStream(String parameterName, InputStream x) throws SQLException {
	}

	@Override
	public void setAsciiStream(String parameterName, InputStream x, long length)
			throws SQLException {
	}

	@Override
	public void setBinaryStream(String parameterName, InputStream x) throws SQLException {
	}

	@Override
	public void setBinaryStream(String parameterName, InputStream x, long length)
			throws SQLException {
	}

	@Override
	public void setBlob(String parameterName, Blob x) throws SQLException {
	}

	@Override
	public void setBlob(String parameterName, InputStream inputStream)
			throws SQLException {
	}

	@Override
	public void setBlob(String parameterName, InputStream inputStream, long length)
			throws SQLException {
	}

	@Override
	public void setCharacterStream(String parameterName, Reader reader)
			throws SQLException {
	}

	@Override
	public void setCharacterStream(String parameterName, Reader reader, long length)
			throws SQLException {
	}

	@Override
	public void setClob(String parameterName, Clob x) throws SQLException {
	}

	@Override
	public void setClob(String parameterName, Reader reader) throws SQLException {
	}

	@Override
	public void setClob(String parameterName, Reader reader, long length)
			throws SQLException {
	}

	@Override
	public void setNCharacterStream(String parameterName, Reader value)
			throws SQLException {
	}

	@Override
	public void setNCharacterStream(String parameterName, Reader value, long length)
			throws SQLException {
	}

	@Override
	public void setNClob(String parameterName, Reader reader) throws SQLException {
	}

	@Override
	public void setNClob(String parameterName, Reader reader, long length)
			throws SQLException {
	}

	@Override
	public void setNString(String parameterName, String value) throws SQLException {
	}

	@Override
	public RowId getRowId(int parameterIndex) throws SQLException {
		return null;
	}

	@Override
	public RowId getRowId(String parameterName) throws SQLException {
		return null;
	}

	@Override
	public void setRowId(String parameterName, RowId x) throws SQLException {
		throw new SQLFeatureNotSupportedException();
	}

	@Override
	public void setNClob(String parameterName, NClob value) throws SQLException {
		throw new SQLFeatureNotSupportedException();
	}

	@Override
	public NClob getNClob(int parameterIndex) throws SQLException {
		return null;
	}

	@Override
	public NClob getNClob(String parameterName) throws SQLException {
		return null;
	}

	@Override
	public void setSQLXML(String parameterName, SQLXML xmlObject) throws SQLException {
		throw new SQLFeatureNotSupportedException();
	}

	@Override
	public SQLXML getSQLXML(int parameterIndex) throws SQLException {
		return null;
	}

	@Override
	public SQLXML getSQLXML(String parameterName) throws SQLException {
		return null;
	}

	@Override
	public void closeOnCompletion() throws SQLException {
		// TODO Auto-generated method stub
		
	}

	@Override
	public boolean isCloseOnCompletion() throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public <T> T getObject(int parameterIndex, Class<T> type) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public <T> T getObject(String parameterName, Class<T> type) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

}
