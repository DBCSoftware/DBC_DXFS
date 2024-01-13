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

import java.math.BigDecimal;
import java.math.RoundingMode;
import java.sql.*;
import java.util.*;
import java.io.*;
import java.net.URL;

/**
 * <P>A SQL statement is pre-compiled and stored in a
 * PreparedStatement object. This object can then be used to
 * efficiently execute this statement multiple times. 
 *
 * <P><B>Note:</B> The setXXX methods for setting IN parameter values
 * must specify types that are compatible with the defined SQL type of
 * the input parameter. For instance, if the IN parameter has SQL type
 * Integer then setInt should be used.
 *
 * <p>If arbitrary parameter type conversions are required then the
 * setObject method should be used with a target SQL type.
 *
 * @see Connection#prepareStatement
 * @see ResultSet 
 */

public class PreparedStatement extends Statement implements java.sql.PreparedStatement {

private static final String QMARK = "?";
private String command;
private int howManyParameters;
private int setParameters;
private String[] parameters;

PreparedStatement(String sql, Message msg, Connection con) throws SQLException {
	super(msg, con);

	howManyParameters = 0;
	command = processEscapes(sql);
	StringTokenizer tok = new StringTokenizer(command, QMARK, true);
	while (tok.hasMoreTokens()) {
		String s = tok.nextToken();
		if (s.equals(QMARK)) howManyParameters++;
	}
	setParameters = 0;
	parameters = new String[howManyParameters];
	if (Driver.traceOn()) Driver.trace("PreparedStatement(), " + sql + ", parameter count = " + howManyParameters);
}

/**
 * A prepared SQL query is executed and its ResultSet is returned.
 *
 * @return a ResultSet that contains the data produced by the
 * query; never null
 * @exception SQLException if a database-access error occurs.
 */
@Override
public java.sql.ResultSet executeQuery() throws SQLException {

	StringBuffer sb = parseInParams();
	if (Driver.traceOn()) Driver.trace("PreparedStatement@executeQuery: sb = " + sb);
	return executeQuery(sb.toString());
}

/**
 * Execute a SQL INSERT, UPDATE or DELETE statement. 
 *
 * @return the row count for INSERT, UPDATE or DELETE
 * @exception SQLException if a database-access error occurs.
 */
@Override
public int executeUpdate() throws SQLException {
	StringBuffer sb = parseInParams();
	if (Driver.traceOn()) Driver.trace("PreparedStatement@executeUpdate: sb = " + sb);
	return executeUpdate(sb.toString());
}

/**
 * Set a parameter to SQL NULL.
 *
 * <P><B>Note:</B> You must specify the parameter's SQL type.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param sqlType SQL type code defined by java.sql.Types
 * @exception SQLException if a database-access error occurs.
 */
@Override
public void setNull(int parameterIndex, int sqlType) throws SQLException {
	checkParam(parameterIndex);
	parameters[parameterIndex - 1] = null;
}

/**
 * Sets the designated parameter to SQL <code>NULL</code>.
 * This version of the method <code>setNull</code> should
 * be used for user-defined types and REF type parameters.  Examples
 * of user-defined types include: STRUCT, DISTINCT, JAVA_OBJECT, and
 * named array types.
 *
 * <P><B>Note:</B> To be portable, applications must give the
 * SQL type code and the fully-qualified SQL type name when specifying
 * a NULL user-defined or REF parameter.  In the case of a user-defined type
 * the name is the type name of the parameter itself.  For a REF
 * parameter, the name is the type name of the referenced type.  If
 * a JDBC driver does not need the type code or type name information,
 * it may ignore it.
 *
 * Although it is intended for user-defined and Ref parameters,
 * this method may be used to set a null parameter of any JDBC type.
 * If the parameter does not have a user-defined or REF type, the given
 * typeName is ignored.
 *
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param sqlType a value from <code>java.sql.Types</code>
 * @param typeName the fully-qualified name of an SQL user-defined type;
 *  ignored if the parameter is not a user-defined type or REF
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC
 *      2.0 API</a>
 */
@Override
public void setNull(int parameterIndex, int sqlType, String typeName) throws SQLException {
	checkParam(parameterIndex);
	parameters[parameterIndex - 1] = null;
}

/**
 * Set a parameter to a Java boolean value.  The driver converts this
 * to a 'Y' or 'N' when it sends it to the database.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the parameter value
 * @exception SQLException if a database-access error occurs.
 */
@Override
public void setBoolean(int parameterIndex, boolean x) throws SQLException {
	checkParam(parameterIndex);
	parameters[parameterIndex - 1] = (x) ? "'Y'" : "'N'";
}

/**
 * Set a parameter to a Java byte value.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the parameter value
 * @exception SQLException if a database-access error occurs.
 */
@Override
public void setByte(int parameterIndex, byte x) throws SQLException {
	checkParam(parameterIndex);
	parameters[parameterIndex - 1] = new String(new byte[] { x });
}

/**
 * Set a parameter to a Java short value.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the parameter value
 * @exception SQLException if a database-access error occurs.
 */
@Override
public void setShort(int parameterIndex, short x) throws SQLException {
	checkParam(parameterIndex);
	parameters[parameterIndex - 1] = String.valueOf(x);
}

/**
 * Set a parameter to a Java int value.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the parameter value
 * @exception SQLException if a database-access error occurs.
 */
@Override
public void setInt(int parameterIndex, int x) throws SQLException {
	checkParam(parameterIndex);
	parameters[parameterIndex - 1] = String.valueOf(x);
}

/**
 * Set a parameter to a Java long value.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the parameter value
 * @exception SQLException if a database-access error occurs.
 */
@Override
public void setLong(int parameterIndex, long x) throws SQLException {
	checkParam(parameterIndex);
	parameters[parameterIndex - 1] = String.valueOf(x);
}

/**
 * Set a parameter to a Java float value.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the parameter value
 * @exception SQLException if a database-access error occurs.
 */
@Override
public void setFloat(int parameterIndex, float x) throws SQLException {
	checkParam(parameterIndex);
	parameters[parameterIndex - 1] = String.valueOf(x);
}

/**
 * Set a parameter to a Java double value.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the parameter value
 * @exception SQLException if a database-access error occurs.
 */
@Override
public void setDouble(int parameterIndex, double x) throws SQLException {
	checkParam(parameterIndex);
	parameters[parameterIndex - 1] = String.valueOf(x);
}

/**
 * Set a parameter to a java.lang.BigDecimal value.  
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the parameter value
 * @exception SQLException if a database-access error occurs.
 */
@Override
public void setBigDecimal(int parameterIndex, BigDecimal x) throws SQLException {
	checkParam(parameterIndex);
	parameters[parameterIndex - 1] = x.toString();
}

/**
 * Set a parameter to a Java String value.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the parameter value
 * @exception SQLException if a database-access error occurs.
 */
@Override
public void setString(int parameterIndex, String x) throws SQLException {
	checkParam(parameterIndex);
	parameters[parameterIndex - 1] = "'" + x + "'";
}

/**
 * Set a parameter to a Java array of bytes.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the parameter value 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public void setBytes(int parameterIndex, byte[] x) throws SQLException {
	checkParam(parameterIndex);
	parameters[parameterIndex - 1] = new String(x);
}

/**
 * Sets the designated parameter to a <code<java.sql.Date</code> value.
 * The driver converts this
 * to an SQL <code>DATE</code> value when it sends it to the database.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Date data type.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the parameter value
 * @exception SQLException if a database-access error occurs.
 */
@Override
public void setDate(int parameterIndex, java.sql.Date x) throws SQLException {
	checkParam(parameterIndex);
	parameters[parameterIndex - 1] = x.toString();
}

/**
 * Set a parameter to a java.sql.Time value.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Time data type.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the parameter value
 * @exception SQLException if a database-access error occurs.
 */
@Override
public void setTime(int parameterIndex, java.sql.Time x) throws SQLException {
	checkParam(parameterIndex);
	parameters[parameterIndex - 1] = x.toString();
}

/**
 * Set a parameter to a java.sql.Timestamp value.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Timestamp data type.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the parameter value 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public void setTimestamp(int parameterIndex, java.sql.Timestamp x) throws SQLException {
	checkParam(parameterIndex);
	parameters[parameterIndex - 1] = x.toString();
}

/**
 * Sets the designated parameter to the given input stream, which will have
 * the specified number of bytes.
 * When a very large ASCII value is input to a <code>LONGVARCHAR</code>
 * parameter, it may be more practical to send it via a
 * <code>java.io.InputStream</code>. Data will be read from the stream
 * as needed until end-of-file is reached.  The JDBC driver will
 * do any necessary conversion from ASCII to the database char format.
 *
 * <P><B>Note:</B> This stream object can either be a standard
 * Java stream object or your own subclass that implements the
 * standard interface.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the Java input stream that contains the ASCII parameter value
 * @param length the number of bytes in the stream
 * @exception SQLException if a database access error occurs
 */
@Override
public void setAsciiStream(int parameterIndex, java.io.InputStream x, int length) throws SQLException {
	checkParam(parameterIndex);
	byte[] ba = new byte[length];
	try {
		x.read(ba);
	}
	catch (IOException ex) {
		setParameters--;
		throw new SQLException(ex.getLocalizedMessage());
	}
	parameters[parameterIndex - 1] = new String(ba);
}

/**
 * When a very large UNICODE value is input to a LONGVARCHAR
 * parameter, it may be more practical to send it via a
 * java.io.InputStream. JDBC will read the data from the stream
 * as needed, until it reaches end-of-file.  The JDBC driver will
 * do any necessary conversion from UNICODE to the database char format.
 * 
 * <P><B>Note:</B> This stream object can either be a standard
 * Java stream object or your own subclass that implements the
 * standard interface.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...  
 * @param x the java input stream which contains the
 * UNICODE parameter value 
 * @param length the number of bytes in the stream 
 * @exception SQLException if a database-access error occurs.
 * @deprecated
 */
@Deprecated
@Override
public void setUnicodeStream(int parameterIndex, java.io.InputStream x, int length) throws SQLException {
	checkParam(parameterIndex);
	int i1 = 0, i2 = 0;
	byte[] ba = new byte[length];
	char[] ca = new char[length / 2];
	try {
		x.read(ba);
	}
	catch (IOException ex) {
		setParameters--;
		throw new SQLException(ex.getLocalizedMessage());
	}
	for (; i1 < ba.length; i1 += 2, i2++) {
		ca[i2] = (char) ((ba[i1 + 1] & 0x00FF) + ((ba[i1] & 0x00FF) << 8));
		if (Driver.traceOn()) {
			StringBuffer sb = new StringBuffer();
			byte[] btest = new byte[2];
			btest[1] = (byte) (ca[i2] & 0x00ff);
			btest[0] = (byte) ((ca[i2] & 0xff00) >> 8);
			sb.append("PreparedStatement@setUnicodeStream : hi byte " + i2 + " : ");
			for (int i = 7; i >= 0; i--)
				sb.append((btest[0] >> i) & 0x01);
			sb.append("  lo byte " + i2 + " : ");
			for (int i = 7; i >= 0; i--)
				sb.append((btest[1] >> i) & 0x01);
			sb.append("  char " + i2 + " : " + ca[i2]);
			Driver.trace(sb.toString());
		}
	}
	parameters[parameterIndex - 1] = "'" + String.valueOf(ca) + "'";
	if (Driver.traceOn()) Driver.trace("PreparedStatement@setUnicodeStream : " + String.valueOf(ca));
	return;
}

/**
 * When a very large binary value is input to a LONGVARBINARY
 * parameter, it may be more practical to send it via a
 * java.io.InputStream. JDBC will read the data from the stream
 * as needed, until it reaches end-of-file.
 * 
 * <P><B>Note:</B> This stream object can either be a standard
 * Java stream object or your own subclass that implements the
 * standard interface.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the java input stream which contains the binary parameter value
 * @param length the number of bytes in the stream 
 * @exception SQLException if a database-access error occurs.
 */
@Override
public void setBinaryStream(int parameterIndex, java.io.InputStream x, int length) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoBinary"));
}

/**
 * <P>In general, parameter values remain in force for repeated use of a
 * Statement. Setting a parameter value automatically clears its
 * previous value.  However, in some cases it is useful to immediately
 * release the resources used by the current parameter values; this can
 * be done by calling clearParameters.
 *
 */
@Override
public void clearParameters() {
	for (int i1 = 0; i1 < howManyParameters; parameters[i1++] = null)
		;
	setParameters = 0;
	return;
}

//----------------------------------------------------------------------
// Advanced features:

/**
 * <p>Sets the value of the designated parameter with the given object. The second
 * argument must be an object type; for integral values, the
 * <code>java.lang</code> equivalent objects should be used.
 *
 * <p>The given Java object will be converted to the given targetSqlType
 * before being sent to the database.
 *
 * If the object has a custom mapping (is of a class implementing the
 * interface <code>SQLData</code>),
 * the JDBC driver should call the method <code>SQLData.writeSQL</code> to write it
 * to the SQL data stream.
 * If, on the other hand, the object is of a class implementing
 * Ref, Blob, Clob, Struct,
 * or Array, the driver should pass it to the database as a value of the
 * corresponding SQL type.
 *
 * <p>Note that this method may be used to pass datatabase-
 * specific abstract data types.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the object containing the input parameter value
 * @param targetSqlType the SQL type (as defined in java.sql.Types) to be
 *                      sent to the database. The scale argument may further qualify this type.
 * @param scale for java.sql.Types.DECIMAL or java.sql.Types.NUMERIC types,
 *          this is the number of digits after the decimal point.  For all other
 *          types, this value will be ignored.
 * @exception SQLException if a database access error occurs
 * @see Types
 */
@Override
public void setObject(int parameterIndex, Object x, int targetSqlType, int scale) throws SQLException {
	String paramOut = null;
	Class<?> classtype;
	String classname;
	classtype = x.getClass();
	classname = classtype.getName();
	if (Driver.traceOn()) Driver.trace("PreparedStatement@setObject : Object Name = " + classname);
	if (Driver.traceOn()) Driver.trace("PreparedStatement@setObject : Object Value = " + x.toString());
	if (Driver.traceOn()) Driver.trace("PreparedStatement@setObject : targetSqlType = " + targetSqlType);
	if (Driver.traceOn()) Driver.trace("PreparedStatement@setObject : scale = " + scale);
	/* check for invalid conversions */
	checkParam(parameterIndex);
	setParameters--;
	switch (targetSqlType) {
	case java.sql.Types.BINARY:
	case java.sql.Types.VARBINARY:
	case java.sql.Types.LONGVARBINARY:
		throw new SQLException(Driver.bun.getString("NoBinary"));
	}
	if (classtype.isArray()) {
		classtype = classtype.getComponentType();
		classname = classtype.getName();
		if (classname.endsWith("byte")) throw new SQLException(Driver.bun.getString("NoBinary"));
		else throw new SQLException(Driver.bun.getString("InvObject") + " : " + classname + "[]");
	}

	if (!(classname.endsWith("String") || classname.endsWith("BigDecimal") || classname.endsWith("Boolean") || classname.endsWith("Integer")
			|| classname.endsWith("Long") || classname.endsWith("Float") || classname.endsWith("Double") || classname.endsWith("Date")
			|| classname.endsWith("Time") || classname.endsWith("Timestamp"))) throw new SQLException(Driver.bun.getString("InvObject") + " : " + classname);
	/* end invalid conversion checks */

	/* these types must be sent to the server with single quotes around them */
	if (targetSqlType == java.sql.Types.BIT || targetSqlType == java.sql.Types.CHAR || targetSqlType == java.sql.Types.VARCHAR
			|| targetSqlType == java.sql.Types.LONGVARCHAR) {
		if (classname.endsWith("String")) paramOut = new String("'" + x + "'");
		else if (classname.endsWith("Boolean")) {
			paramOut = (((Boolean) x).booleanValue()) ? "'Y'" : "'N'";
		}
		else paramOut = new String("'" + x.toString() + "'");
	}
	/* these types must be converted to a BigDecimal to apply the scale */
	else if (targetSqlType == java.sql.Types.NUMERIC || targetSqlType == java.sql.Types.DECIMAL) {
		BigDecimal bd;
		if (classname.endsWith("String")) {
			try {
				bd = new BigDecimal(((String) x).trim());
			}
			catch (NumberFormatException ex) {
				throw new SQLException(Driver.bun.getString("BadNumber") + " : " + x);
			}
		}
		else if (classname.endsWith("Boolean")) {
			bd = new BigDecimal((((Boolean) x).booleanValue()) ? 1 : 0);
		}
		else bd = new BigDecimal(x.toString());
		if (scale > 0) bd = bd.setScale(scale, RoundingMode.HALF_UP);
		paramOut = bd.toString();
	}
	/* these types must be sent to the server with no quotes around them */
	else if ((targetSqlType == java.sql.Types.TINYINT) || (targetSqlType == java.sql.Types.SMALLINT) || (targetSqlType == java.sql.Types.INTEGER)
			|| (targetSqlType == java.sql.Types.BIGINT) || (targetSqlType == java.sql.Types.REAL) || (targetSqlType == java.sql.Types.FLOAT)
			|| (targetSqlType == java.sql.Types.DOUBLE)) {
		if (classname.endsWith("String")) {
			if ((targetSqlType == java.sql.Types.TINYINT) || (targetSqlType == java.sql.Types.SMALLINT) || (targetSqlType == java.sql.Types.INTEGER)
					|| (targetSqlType == java.sql.Types.BIGINT)) try {
				paramOut = (Long.valueOf((String) x)).toString();
			}
			catch (NumberFormatException ex) {
				throw new SQLException(Driver.bun.getString("BadNumber") + " : " + x);
			}
			else try {
				paramOut = (Double.valueOf((String) x)).toString();
			}
			catch (NumberFormatException ex) {
				throw new SQLException(Driver.bun.getString("BadNumber") + " : " + x);
			}
		}
		else if (classname.endsWith("Boolean")) {
			paramOut = (((Boolean) x).booleanValue()) ? "1" : "0";
		}
		else paramOut = x.toString();
	}
	/* invalid type */
	else {
		throw new SQLException(Driver.bun.getString("BadSqlType") + " : " + targetSqlType);
	}

	parameters[parameterIndex - 1] = paramOut;
	setParameters++;
}

/**
 * This method is like setObject above, but assumes a scale of zero.
 *
 * @exception SQLException if a database-access error occurs.
 */
@Override
public void setObject(int parameterIndex, Object x, int targetSqlType) throws SQLException {
	setObject(parameterIndex, x, targetSqlType, 0);
}

/**
 * <p>Sets the value of the designated parameter using the given object.
 * The second parameter must be of type <code>Object</code>; therefore, the
 * <code>java.lang</code> equivalent objects should be used for built-in types.
 *
 * <p>The JDBC specification specifies a standard mapping from
 * Java <code>Object</code> types to SQL types.  The given argument
 * will be converted to the corresponding SQL type before being
 * sent to the database.
 *
 * <p>Note that this method may be used to pass datatabase-
 * specific abstract data types, by using a driver-specific Java
 * type.
 *
 * If the object is of a class implementing the interface <code>SQLData</code>,
 * the JDBC driver should call the method <code>SQLData.writeSQL</code>
 * to write it to the SQL data stream.
 * If, on the other hand, the object is of a class implementing
 * Ref, Blob, Clob, Struct,
 * or Array, then the driver should pass it to the database as a value of the
 * corresponding SQL type.
 *
 * This method throws an exception if there is an ambiguity, for example, if the
 * object is of a class implementing more than one of the interfaces named above.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the object containing the input parameter value
 * @exception SQLException if a database access error occurs
 */
@Override
public void setObject(int parameterIndex, Object x) throws SQLException {
	String paramOut;
	Class<?> classtype;
	String classname;

	checkParam(parameterIndex);
	setParameters--;
	if (x == null) {
		classtype = null;
		classname = null;
	}
	else {
		classtype = x.getClass();
		classname = classtype.getName();
	}
	if (classtype != null && classtype.isArray()) {
		classtype = classtype.getComponentType();
		classname = classtype.getName();
		if (classname.endsWith("byte")) throw new SQLException(Driver.bun.getString("NoBinary"));
		else throw new SQLException(Driver.bun.getString("InvObject") + " : " + classname + "[]");
	}
	if (classname == null) {
		paramOut = "";
	}
	else if (classname.endsWith("String") && x != null) paramOut = "'" + x + "'";
	else if (classname.endsWith("Boolean") && x != null) {
		paramOut = (((Boolean) x).booleanValue()) ? "'Y'" : "'N'";
	}
	else if (((classname.endsWith("BigDecimal")) || (classname.endsWith("Integer")) || (classname.endsWith("Long"))
			|| (classname.endsWith("Float"))
			|| (classname.endsWith("Double"))) && x != null) paramOut = x.toString();
	else if (classname.endsWith("Timestamp") && x != null) paramOut = "'" + x.toString() + "'";
	else if ((classname.endsWith("Date") || classname.endsWith("Time")) && x != null) paramOut = x.toString();
	else throw new SQLException(Driver.bun.getString("InvObject") + " : " + classname);
	parameters[parameterIndex - 1] = paramOut;
	setParameters++;
}

/**
 * Some prepared statements return multiple results; the execute
 * method handles these complex statements as well as the simpler
 * form of statements handled by executeQuery and executeUpdate.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support multiple return results.
 * This method is analogous to Statement execute().
 *
 * @exception SQLException if a database-access error occurs.
 * @see Statement#execute
 */
@Override
public boolean execute() throws SQLException {
	String cmd = parseInParams().toString();
	if (Driver.traceOn()) Driver.trace("PreparedStatement@execute: sb = " + cmd);
	return execute(cmd);
}

/*
 * This method parses the parameters and prepares the query string to
 * be sent to the database.
 *
 */
private StringBuffer parseInParams() throws SQLException {
	if (setParameters != howManyParameters) throw new SQLException(Driver.bun.getString("InvParamNum"));
	int i1 = 0;
	StringBuffer sb = new StringBuffer();
	StringTokenizer tok = new StringTokenizer(command, QMARK, true);

	while (tok.hasMoreTokens()) {
		String s = tok.nextToken();
		if (s.equals(QMARK)) {
			if (parameters[i1] != null) sb.append(parameters[i1]);
			else sb.append("NULL");
			i1++;
		}
		else sb.append(s);
	}
	return sb;
}

/*
 * This method checks to see that the parameter number is in bounds.
 *
 */
private void checkParam(int parameterIndex) throws SQLException {
	if (parameterIndex < 1 || parameterIndex > howManyParameters) throw new SQLException(Driver.bun.getString("InvParIndex"));
	setParameters++;
}

//--------------------------JDBC 2.0-----------------------------

/**
 * Adds a set of parameters to this <code>PreparedStatement</code>
 * object's batch of commands.
 *
 * 
 * <P><B>FS2 NOTE:</B> FS2 does not support Batch processing.
 *
 * @exception SQLException if a database access error occurs
 * @see Statement#addBatch
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC
 *      2.0 API</a>
 */
@Override
public void addBatch() throws SQLException {
	throw new SQLException(Driver.bun.getString("NoBatch"));
}

/**
 * Sets the designated parameter to the given <code>Reader</code>
 * object, which is the given number of characters long.
 * When a very large UNICODE value is input to a <code>LONGVARCHAR</code>
 * parameter, it may be more practical to send it via a
 * <code>java.io.Reader</code> object. The data will be read from the stream
 * as needed until end-of-file is reached.  The JDBC driver will
 * do any necessary conversion from UNICODE to the database char format.
 *
 * <P><B>Note:</B> This stream object can either be a standard
 * Java stream object or your own subclass that implements the
 * standard interface.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the java reader which contains the UNICODE data
 * @param length the number of characters in the stream
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC
 *      2.0 API</a>
 */
@Override
public void setCharacterStream(int parameterIndex, java.io.Reader reader, int length) throws SQLException {
	checkParam(parameterIndex);
	char[] ca = new char[length];
	try {
		reader.read(ca);
	}
	catch (IOException ex) {
		setParameters--;
		throw new SQLException(ex.getLocalizedMessage());
	}
	parameters[parameterIndex - 1] = new String("'" + String.valueOf(ca) + "'");
}

/**
 * Sets the designated parameter to the given
 *  <code>REF(&lt;structured-type&gt;)</code> value.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Ref data type.
 *
 * @param i the first parameter is 1, the second is 2, ...
 * @param x an SQL <code>REF</code> value
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC
 *      2.0 API</a>
 */
@Override
public void setRef(int i, java.sql.Ref x) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoType") + " : Ref");
}

/**
 * Sets the designated parameter to the given
 *  <code>Blob</code> object.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Blob data type.
 *
 * @param i the first parameter is 1, the second is 2, ...
 * @param x a <code>Blob</code> object that maps an SQL <code>BLOB</code> va
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC
 *      2.0 API</a>
 */
@Override
public void setBlob(int i, java.sql.Blob x) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoType") + " : Blob");
}

/**
 * Sets the designated parameter to the given
 *  <code>Clob</code> object.
 *
 * @param i the first parameter is 1, the second is 2, ...
 * @param x a <code>Clob</code> object that maps an SQL <code>CLOB</code> va
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC
 *      2.0 API</a>
 */
@Override
public void setClob(int i, java.sql.Clob x) throws SQLException {
	setCharacterStream(i, x.getCharacterStream(), (int) x.length());
}

/**
 * Sets the designated parameter to the given
 *  <code>Array</code> object.
 * Sets an Array parameter.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Array data type.
 *
 * @param i the first parameter is 1, the second is 2, ...
 * @param x an <code>Array</code> object that maps an SQL <code>ARRAY</code> value
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC
 *      2.0 API</a>
 */
@Override
public void setArray(int i, java.sql.Array x) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoType") + " : Array");
}

/**
 * The number, types and properties of a ResultSet's columns
 * are provided by the getMetaData method.
 *
 * @return the description of a ResultSet's columns
 * @exception SQLException if a database-access error occurs.
 */
@Override
public java.sql.ResultSetMetaData getMetaData() throws SQLException {
	java.sql.ResultSet rs = getResultSet();
	return (rs == null) ? null : rs.getMetaData();
}

/**
 * Sets the designated parameter to the given <code>java.sql.Date</code> value
 * using the given <code>Calendar</code> object.  The driver uses
 * the <code>Calendar</code> object to construct an SQL <code>DATE</code> value
 * which the driver then sends to the database.  With a
 * a <code>Calendar</code> object, the driver can calculate the date
 * taking into account a custom timezone.  If no
 * <code>Calendar</code> object is specified, the driver uses the default
 * timezone, which is that of the virtual machine running the application.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Date data type at this time.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the parameter value
 * @param cal the <code>Calendar</code> object the driver will use
 *            to construct the date
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC
 *      2.0 API</a>
 */
@Override
public void setDate(int parameterIndex, java.sql.Date x, Calendar cal) throws SQLException {
	checkParam(parameterIndex);
	cal.setTime(x);
	parameters[parameterIndex - 1] = new java.sql.Date(cal.getTime().getTime()).toString();
}

/**
 * Sets the designated parameter to the given <code>java.sql.Time</code> val
 * using the given <code>Calendar</code> object.  The driver uses
 * the <code>Calendar</code> object to construct an SQL <code>TIME</code> va
 * which the driver then sends to the database.  With a
 * a <code>Calendar</code> object, the driver can calculate the time
 * taking into account a custom timezone.  If no
 * <code>Calendar</code> object is specified, the driver uses the default
 * timezone, which is that of the virtual machine running the application.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Time data type at this time.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the parameter value
 * @param cal the <code>Calendar</code> object the driver will use
 *            to construct the time
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC
 *      2.0 API</a>
 */
@Override
public void setTime(int parameterIndex, java.sql.Time x, Calendar cal) throws SQLException {
	checkParam(parameterIndex);
	cal.setTime(x);
	parameters[parameterIndex - 1] = new java.sql.Time(cal.getTime().getTime()).toString();
}

/**
 * Sets the designated parameter to the given <code>java.sql.Timestamp</code>
 * using the given <code>Calendar</code> object.  The driver uses
 * the <code>Calendar</code> object to construct an SQL <code>TIMESTAMP</code>
 * which the driver then sends to the database.  With a
 * a <code>Calendar</code> object, the driver can calculate the timestamp
 * taking into account a custom timezone.  If no
 * <code>Calendar</code> object is specified, the driver uses the default
 * timezone, which is that of the virtual machine running the application.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Timestamp data type at this time.
 *
 * @param parameterIndex the first parameter is 1, the second is 2, ...
 * @param x the parameter value
 * @param cal the <code>Calendar</code> object the driver will use
 *            to construct the timestamp
 * @exception SQLException if a database access error occurs
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC
 *      2.0 API</a>
 */
@Override
public void setTimestamp(int parameterIndex, java.sql.Timestamp x, Calendar cal) throws SQLException {
	checkParam(parameterIndex);
	cal.setTime(x);
	parameters[parameterIndex - 1] = new java.sql.Timestamp(cal.getTime().getTime()).toString();
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
public void setAsciiStream(int parameterIndex, InputStream x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setAsciiStream(int parameterIndex, InputStream x, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setBinaryStream(int parameterIndex, InputStream x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setBinaryStream(int parameterIndex, InputStream x, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setBlob(int parameterIndex, InputStream inputStream) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setBlob(int parameterIndex, InputStream inputStream, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setCharacterStream(int parameterIndex, Reader reader) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setCharacterStream(int parameterIndex, Reader reader, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setClob(int parameterIndex, Reader reader) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setClob(int parameterIndex, Reader reader, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setNCharacterStream(int parameterIndex, Reader value) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setNCharacterStream(int parameterIndex, Reader value, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setNClob(int parameterIndex, Reader reader) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setNClob(int parameterIndex, Reader reader, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setNString(int parameterIndex, String value) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setRowId(int parameterIndex, RowId x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setNClob(int parameterIndex, NClob value) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setSQLXML(int parameterIndex, SQLXML xmlObject) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

}
