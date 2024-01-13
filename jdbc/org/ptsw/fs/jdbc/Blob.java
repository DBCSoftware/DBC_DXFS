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
import java.io.OutputStream;
import java.sql.SQLException;

/**
 * JDBC 2.0
 *
 * <p>By default, a Blob is a transaction duration reference to a 
 * binary large object. By default, a Blob is implemented using a
 * LOCATOR(blob) internally.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Blob data type.
 *
 */

public final class Blob implements java.sql.Blob {

/**
 * The length of the Binary Large OBject in bytes.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Blob data type.
 *
 * @return length of the BLOB in bytes
 */
@Override
public long length() throws SQLException {
	throw new SQLException(Driver.bun.getString("NoBinary"));
}

/**
 * Return a copy of the contents of the BLOB at the requested 
 * position.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Blob data type.
 *
 * @param pos is the first byte of the blob to be extracted.
 * @param length is the number of consecutive bytes to be copied.
 *
 * @return a byte array containing a portion of the BLOB
 */
@Override
public byte[] getBytes(long pos, int length) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoBinary"));
}

/**
 * Retrieve the entire BLOB as a stream.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Blob data type.
 *
 * @return a stream containing the BLOB data
 */
@Override
public java.io.InputStream getBinaryStream() throws SQLException {
	throw new SQLException(Driver.bun.getString("NoBinary"));
}

/** 
 * Determine the byte position at which the given byte pattern 
 * @pattern starts in the BLOB.  Begin search at position @start.
 * Return -1 if the pattern does not appear in the BLOB.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Blob data type.
 *
 * @param  pattern is the pattern to search for.
 * @param start is the position at which to begin searching.
 * @return the position at which the pattern appears, else -1.
 */
@Override
public long position(byte pattern[], long start) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoBinary"));
}

/** 
 * Determine the byte position at which the given pattern 
 * @pattern starts in the BLOB.  Begin search at position @start.
 * Return -1 if the pattern does not appear in the BLOB.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Blob data type.
 *
 * @param searchstr is the pattern to search for.
 * @param start is the position at which to begin searching.
 * @return the position at which the pattern appears, else -1.
 */
@Override
public long position(java.sql.Blob pattern, long start) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoBinary"));
}

@Override
public int setBytes(long arg0, byte[] arg1) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoBinary"));
}

@Override
public int setBytes(long arg0, byte[] arg1, int arg2, int arg3) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoBinary"));
}

@Override
public OutputStream setBinaryStream(long arg0) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoBinary"));
}

@Override
public void truncate(long arg0) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoBinary"));
}

@Override
public void free() throws SQLException {
	throw new SQLException();
}

@Override
public InputStream getBinaryStream(long pos, long length) throws SQLException {
	// TODO Auto-generated method stub
	return null;
}

}
