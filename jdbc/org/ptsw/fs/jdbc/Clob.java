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

import java.io.ByteArrayInputStream;
import java.io.OutputStream;
import java.io.Reader;
import java.io.StringReader;
import java.io.Writer;
import java.sql.SQLException;

/**
 * JDBC 2.0
 *
 * <p>By default, a Clob is a transaction duration reference to a 
 * character large object.
 */

public final class Clob implements java.sql.Clob {

private final String value;

public Clob(String x) {
	value = x;
}

/**
 * Returns the number of characters
 * in the <code>CLOB</code> value
 * designated by this <code>Clob</code> object.
 *
 * @return    length of the <code>CLOB</code> in characters
 * @since 1.2
 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0 API</a>
 */
@Override
public long length() {
	return value.length();
}

/**
 * Get the Clob contents as an ascii stream.
 *
 * @return an ascii stream containing the CLOB data
 */
@Override
public java.io.InputStream getAsciiStream() {
	return new ByteArrayInputStream(value.getBytes());
}

/**
 * Return copy of the substring of the CLOB at the requested position.
 *
 * @param pos is the first character of the substring to be extracted.
 *            This is a ONE-BASED value!
 * @param length is the number of consecutive characters to be copied.
 *
 * @return a String containing the substring of the CLOB
 * @exception SQLException if index out of bounds.
 */
@Override
public String getSubString(long pos, int length) throws SQLException {
	String tmp = null;
	try {
		tmp = value.substring((int) (pos - 1), (int) (pos + length - 1));
	}
	catch (StringIndexOutOfBoundsException ex) {
		throw new SQLException(ex.getLocalizedMessage());
	}
	return tmp;
}

/**
 * Get the Clob contents as a Unicode stream.
 *
 * @return a Unicode stream containing the CLOB data
 */
@Override
public java.io.Reader getCharacterStream() {
	return new StringReader(value);
}

/** 
 * Determine the character position at which the given substring 
 * @searchstr appears in the CLOB.  Begin search at position @start.
 * Return -1 if the substring does not appear in the CLOB.
 *
 * @param searchstr is the substring to search for.
 * @param start     is the position at which to begin searching.
 *                  This is a ONE-BASED value;
 * @return the position at which the substring appears, else -1.
 */
@Override
public long position(String searchstr, long start) {
	return value.indexOf(searchstr, (int) (start - 1));
}

/** 
 * Determine the character position at which the given substring 
 * @searchstr appears in the CLOB.  Begin search at position @start.
 * Return -1 if the substring does not appear in the CLOB.
 *
 * @param searchstr is the substring to search for.
 * @param start     is the position at which to begin searching.
 *                  This is a ONE-BASED value;
 * @return the position at which the substring appears, else -1.
 */
@Override
public long position(java.sql.Clob searchstr, long start) throws SQLException {
	String temp = searchstr.getSubString(1, (int) searchstr.length());
	return value.indexOf(temp, (int) start);
}

@Override
public int setString(long arg0, String arg1) throws SQLException {
	return 0;
}

@Override
public int setString(long arg0, String arg1, int arg2, int arg3) throws SQLException {
	return 0;
}

@Override
public OutputStream setAsciiStream(long arg0) throws SQLException {
	return null;
}

@Override
public Writer setCharacterStream(long arg0) throws SQLException {
	return null;
}

@Override
public void truncate(long arg0) throws SQLException {
}

@Override
public void free() throws SQLException {
}

@Override
public Reader getCharacterStream(long pos, long length) throws SQLException {
	return null;
}

} // end of class Clob
