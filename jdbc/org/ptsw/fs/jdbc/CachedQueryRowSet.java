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

import java.io.BufferedWriter;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.CharArrayReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStreamWriter;
import java.io.Reader;
import java.math.BigDecimal;
import java.net.URL;
import java.sql.Array;
import java.sql.Blob;
import java.sql.Clob;
import java.sql.Date;
import java.sql.NClob;
import java.sql.Ref;
import java.sql.ResultSetMetaData;
import java.sql.RowId;
import java.sql.SQLException;
import java.sql.SQLFeatureNotSupportedException;
import java.sql.SQLWarning;
import java.sql.SQLXML;
import java.sql.Statement;
import java.sql.Time;
import java.sql.Timestamp;
import java.text.MessageFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.List;
import java.util.Map;

import javax.sql.RowSet;
import javax.sql.RowSetListener;

@SuppressWarnings("deprecation")
public class CachedQueryRowSet implements RowSet {

private String myCmd = null;
private final Statement myStatement;
private final Connection myCon;
private final Message connectMsg; // Message object to use
private ResultSet myResultSet;
private boolean escapeProcessing = true;
private int queryTimeout;
private int FSMajorVersion;
private List<CommonType> cachedRowData;
private int currentRow; // 1 based
private int rowCount;
private int maxRows;
private boolean isAfterLast = false;
private boolean wasNull = false;

/**
 * @param connection
 */
public CachedQueryRowSet(java.sql.Connection connection) {
	myCon = (org.ptsw.fs.jdbc.Connection) connection;
	connectMsg = myCon.getMessage();
	FSMajorVersion = myCon.getFSMajorVersion();
	myStatement = new org.ptsw.fs.jdbc.Statement(connectMsg, myCon);
	cachedRowData = new ArrayList<CommonType>();
	currentRow = rowCount = maxRows = 0;
}

@Override
public String getUrl() throws SQLException {
	return myCon.getUrl();
}

@Override
public void setUrl(String arg0) throws SQLException {
	throw new SQLException("not supported");
}

@Override
public String getDataSourceName() {
	return null; // not supported
}

@Override
public void setDataSourceName(String arg0) throws SQLException {
	throw new SQLException("not supported");
}

@Override
public String getUsername() {
	return myCon.getUserName();
}

@Override
public void setUsername(String arg0) throws SQLException {
	throw new SQLException("not supported");
}

@Override
public String getPassword() {
	return null;
}

@Override
public void setPassword(String arg0) throws SQLException {
	throw new SQLException("not supported");
}

@Override
public int getTransactionIsolation() {
	return 0;
}

@Override
public void setTransactionIsolation(int arg0) throws SQLException {
	throw new SQLException("not supported");
}

@Override
public Map<String, Class<?>> getTypeMap() throws SQLException {
	return myCon.getTypeMap();
}

@Override
public void setTypeMap(Map<String, Class<?>> arg0) throws SQLException {
	throw new SQLException("not supported");
}

@Override
public String getCommand() {
	return myCmd;
}

/* (non-Javadoc)
 * @see javax.sql.RowSet#setCommand(java.lang.String)
 */
@Override
public void setCommand(String command) throws SQLException {
	myCmd = command;
}

@Override
public boolean isReadOnly() {
	return true;
}

@Override
public void setReadOnly(boolean readonly) throws SQLException {
	if (!readonly) throw new SQLException("not supported");
}

@Override
public int getMaxFieldSize() throws SQLException {
	throw new SQLException("not supported");
}

@Override
public void setMaxFieldSize(int arg0) throws SQLException {
	throw new SQLException("not supported");
}

@Override
public int getMaxRows() throws SQLException {
	return maxRows;
}

/* (non-Javadoc)
 * @see javax.sql.RowSet#setMaxRows(int)
 */
@Override
public void setMaxRows(int count) throws SQLException {
	maxRows = count;
}

@Override
public boolean getEscapeProcessing() throws SQLException {
	return escapeProcessing;
}

@Override
public void setEscapeProcessing(boolean esc) throws SQLException {
    escapeProcessing = esc;
	myStatement.setEscapeProcessing(esc);
}

@Override
public int getQueryTimeout() throws SQLException {
	return queryTimeout;
}

@Override
public void setQueryTimeout(int secs) throws SQLException {
	queryTimeout = secs;
}

@Override
public void setType(int arg0) throws SQLException {
	throw new SQLException("not supported");
}

@Override
public void setConcurrency(int arg0) throws SQLException {
	throw new SQLException("not supported");
}

@Override
public void setNull(int arg0, int arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setNull(int arg0, int arg1, String arg2) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setBoolean(int arg0, boolean arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setByte(int arg0, byte arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setShort(int arg0, short arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setInt(int arg0, int arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setLong(int arg0, long arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setFloat(int arg0, float arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setDouble(int arg0, double arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setBigDecimal(int arg0, BigDecimal arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setString(int arg0, String arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setBytes(int arg0, byte[] arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setDate(int arg0, Date arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setTime(int arg0, Time arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setTimestamp(int arg0, Timestamp arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setAsciiStream(int arg0, InputStream arg1, int arg2) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setBinaryStream(int arg0, InputStream arg1, int arg2) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setCharacterStream(int arg0, Reader arg1, int arg2) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setObject(int arg0, Object arg1, int arg2, int arg3) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setObject(int arg0, Object arg1, int arg2) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setObject(int arg0, Object arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setRef(int arg0, Ref arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setBlob(int arg0, Blob arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setClob(int arg0, Clob arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setArray(int arg0, Array arg1) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setDate(int arg0, Date arg1, Calendar arg2) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setTime(int arg0, Time arg1, Calendar arg2) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void setTimestamp(int arg0, Timestamp arg1, Calendar arg2) throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void clearParameters() throws SQLException {
	throw new SQLException("parameterized statement not supported");
}

@Override
public void execute() throws SQLException {
    if (myCmd == null) {
        throw new SQLException("sequence error, must call setCommand() before execute()");
    }
    if (maxRows <= 0) {
        throw new SQLException("sequence error, must call setMaxRows() before execute()");
    }
    String s1 = myCmd.toUpperCase();
    if (!s1.startsWith("SELECT")) {
        throw new SQLException(Driver.bun.getString("UnSuppSqlStmt"));
    }
    if (myResultSet != null) myResultSet.close();
    myResultSet = (org.ptsw.fs.jdbc.ResultSet) myStatement.executeQuery(myCmd);
    reset();
    if (!myResultSet.isEmpty) fetchBatchOfRows();
}

private void fetchBatchOfRows() throws SQLException {
	byte[] baData;
	String rtcd;
	synchronized (connectMsg.getSock()) {
		if (FSMajorVersion < 4) {
			throw new SQLException(Driver.bun.getString("BadFeature"));
		}
		else {
			connectMsg.setFunc("GETROWC");
			connectMsg.setFileId(myResultSet.getRsid());
			connectMsg.setData(String.valueOf(maxRows + 1));
			if (Driver.traceOn()) Driver.trace("CachedQueryRowSet : fetchBatchOfRows about to call sendMessage()");
			try {
				connectMsg.sendMessage();
			}
			catch (IOException ioex) {
				throw new SQLException(ioex.getLocalizedMessage(), connectMsg.getLastErrorCode());
			}
		}
		for (int i2 = 0; i2 <= maxRows; i2++, rowCount++) {    
		    try {
				connectMsg.recvMessage();
		    }
		    catch (IOException ioex) {
		        if (!ioex.getLocalizedMessage().equals(Driver.bun.getString("Outofsync"))) { 
		            // throw error for anything except "out of sync" error           
		            throw new SQLException(ioex.getLocalizedMessage(), connectMsg.getLastErrorCode());
		        }
		    }		        
			rtcd = connectMsg.getRtcd();
			if (rtcd.startsWith("OK")) {
			    if (i2 == maxRows) {
			        throw new SQLException(Driver.bun.getString("CachedReadFail"));
			    }
				baData = connectMsg.getData();
				if (Driver.traceOn()) Driver.trace("CachedQueryRowSet : fetchBatchOfRows baData = " + new String(baData));
				for (int i1 = 0, start = 0; i1 < myResultSet.getNumCols() && start < baData.length; start += myResultSet.getDisplayWidth(i1++)) {
					byte[] valueBa = new byte[myResultSet.getDisplayWidth(i1)];
					System.arraycopy(baData, start, valueBa, 0, valueBa.length);
					cachedRowData.add(new CommonType(valueBa, myResultSet.getColType(i1), myResultSet.getColSize(i1)));
				}
			}
			else if (rtcd.equals("SQL20000")) {
				break;
			}
			else {
				throw new SQLException(MessageFormat.format(Driver.bun.getString("UnknRtcd"), (Object[]) new String[] { rtcd }));
			}
		}
	}
}

@Override
public void addRowSetListener(RowSetListener arg0) {
    // TODO do we need code for this?
}

@Override
public void removeRowSetListener(RowSetListener arg0) {
    // TODO do we need code for this?
}

@Override
public boolean next() throws SQLException {
	if (isAfterLast) return false;
	++currentRow;
	if (currentRow > rowCount) {
	    isAfterLast = true;
	    return false;
	}
	clearWarnings();
	return true;
}

private void reset() throws SQLException {
    clearWarnings();
    wasNull = isAfterLast = false;
    currentRow = rowCount = 0;
    cachedRowData.clear();    
}

@Override
public void close() throws SQLException {
    reset();
	if (myResultSet != null) myResultSet.close();
    if (myStatement != null) myStatement.close();
}

@Override
public boolean wasNull() throws SQLException {
	return wasNull;
}

private boolean isOnValidRow() {
    return currentRow <= rowCount;
}

/**
 * Finds column data in ArrayList based on row and column index, then returns it
 * Parameter columnIndex and currentRow must be checked for validity before calling (checkCol does this)
 * 
 * @param columnIndex
 * @return CommonType
 */
private CommonType getColumnValue(int columnIndex) throws SQLException {
    return cachedRowData.get(((currentRow - 1) * myResultSet.getNumCols()) + columnIndex);
}

private boolean checkCol(final int columnIndex) throws SQLException {
	wasNull = false;
	if (columnIndex < 1 || columnIndex > myResultSet.getNumCols()) {
	    throw new SQLException(Driver.bun.getString("InvColIndex"));
	}
	if (!isOnValidRow()) throw new SQLException(Driver.bun.getString("InvCrntRow"));
	if (getColumnValue(columnIndex - 1).isNull()) {
		wasNull = true;
	}
	return wasNull;
}

@Override
public String getString(int columnIndex) throws SQLException {
	String s1 = null;
	if (checkCol(columnIndex)) {
		if (Driver.traceOn()) Driver.trace("@CachedQueryRowSet : getString(), checkCol failed, index = " + columnIndex);
		return null;
	}
	s1 = getColumnValue(columnIndex - 1).getString();
	return s1;
}

@Override
public boolean getBoolean(int columnIndex) throws SQLException {
	if (checkCol(columnIndex)) return false;
	return getColumnValue(columnIndex - 1).getBoolean();
}

@Override
public byte getByte(int columnIndex) throws SQLException {
	if (checkCol(columnIndex)) return 0;
	return getBytes(columnIndex)[0];
}

@Override
public short getShort(int columnIndex) throws SQLException {
	if (checkCol(columnIndex)) return 0;
	return getColumnValue(columnIndex - 1).getShort();
}

@Override
public int getInt(int columnIndex) throws SQLException {
	if (checkCol(columnIndex)) return 0;
	if (Driver.traceOn()) Driver.trace("@ResultSet : getInt(), column = " + columnIndex);
	return getColumnValue(columnIndex - 1).getInt();
}

@Override
public long getLong(int columnIndex) throws SQLException {
	if (checkCol(columnIndex)) return 0;
	return getColumnValue(columnIndex - 1).getLong();
}

@Override
public float getFloat(int columnIndex) throws SQLException {
	if (checkCol(columnIndex)) return 0;
	return getColumnValue(columnIndex - 1).getFloat();
}

@Override
public double getDouble(int columnIndex) throws SQLException {
	if (checkCol(columnIndex)) return 0;
	return getColumnValue(columnIndex - 1).getDouble();
}

@Override
public BigDecimal getBigDecimal(int columnIndex, int scale) throws SQLException {
	if (checkCol(columnIndex)) return null;
	return getColumnValue(columnIndex - 1).getBigDecimal(scale);
}

@Override
public byte[] getBytes(int columnIndex) throws SQLException {
	if (checkCol(columnIndex)) return null;
	return getColumnValue(columnIndex - 1).getBytes();
}

@Override
public Date getDate(int columnIndex) throws SQLException {
	if (checkCol(columnIndex)) return null;
	return getColumnValue(columnIndex - 1).getDate();
}

@Override
public Time getTime(int columnIndex) throws SQLException {
	if (checkCol(columnIndex)) return null;
	return getColumnValue(columnIndex - 1).getTime();
}

@Override
public Timestamp getTimestamp(int columnIndex) throws SQLException {
	if (checkCol(columnIndex)) return null;
	return getColumnValue(columnIndex - 1).getTimestamp();
}

@Override
public InputStream getAsciiStream(int columnIndex) throws SQLException {
	if (checkCol(columnIndex)) return null;
	return new ByteArrayInputStream(getColumnValue(columnIndex - 1).getBytes());
}

@Override
public InputStream getUnicodeStream(int columnIndex) throws SQLException {
	if (checkCol(columnIndex)) return null;
	byte[] b1 = getColumnValue(columnIndex - 1).getBytes();
	byte[] b2 = new byte[(b1.length) * 2];
	int pos1, pos2;
	for (pos1 = 0, pos2 = 0; pos1 < b1.length; pos1++) {
		pos2 = pos1 * 2;
		b2[pos2] = 0x00;
		b2[pos2 + 1] = b1[pos1];
		if (Driver.traceOn()) {
			StringBuffer sb = new StringBuffer();
			sb.append("ResultSet@getUnicodeStream : hi byte " + pos1 + " : ");
			for (int i2 = 7; i2 >= 0; i2--)
				sb.append((b2[pos2] >> i2) & 0x01);
			sb.append("  lo byte " + pos1 + " : ");
			for (int i2 = 7; i2 >= 0; i2--)
				sb.append((b2[pos2 + 1] >> i2) & 0x01);
			sb.append("  char " + pos1 + " : ");
			sb.append((char) ((b2[pos2 + 1] & 0x00FF) + ((b2[pos2] & 0x00FF) << 8)));
			Driver.trace(sb.toString());
		}
	}
	return new ByteArrayInputStream(b2);
}

@Override
public InputStream getBinaryStream(int columnIndex) throws SQLException {
	if (checkCol(columnIndex)) return null;
	return new ByteArrayInputStream(getBytes(columnIndex));
}

@Override
public String getString(String columnName) throws SQLException {
	return getString(findColumn(columnName));
}

@Override
public boolean getBoolean(String columnName) throws SQLException {
	return getBoolean(findColumn(columnName));
}

@Override
public byte getByte(String columnName) throws SQLException {
	return getByte(findColumn(columnName));
}

@Override
public short getShort(String columnName) throws SQLException {
	return getShort(findColumn(columnName));
}

@Override
public int getInt(String columnName) throws SQLException {
	return getInt(findColumn(columnName));
}

@Override
public long getLong(String columnName) throws SQLException {
	return getLong(findColumn(columnName));
}

@Override
public float getFloat(String columnName) throws SQLException {
	return getFloat(findColumn(columnName));
}

@Override
public double getDouble(String columnName) throws SQLException {
	return getDouble(findColumn(columnName));
}

@Override
public BigDecimal getBigDecimal(String columnName, int scale) throws SQLException {
	return getBigDecimal(findColumn(columnName), scale);
}

@Override
public byte[] getBytes(String columnName) throws SQLException {
	return getBytes(findColumn(columnName));
}

@Override
public Date getDate(String columnName) throws SQLException {
	return getDate(findColumn(columnName));
}

@Override
public Time getTime(String columnName) throws SQLException {
	return getTime(findColumn(columnName));
}

@Override
public Timestamp getTimestamp(String columnName) throws SQLException {
	return getTimestamp(findColumn(columnName));
}

@Override
public InputStream getAsciiStream(String columnName) throws SQLException {
	return getAsciiStream(findColumn(columnName));
}

@Override
public InputStream getUnicodeStream(String columnName) throws SQLException {
	return getUnicodeStream(findColumn(columnName));
}

@Override
public InputStream getBinaryStream(String columnName) throws SQLException {
	return getBinaryStream(findColumn(columnName));
}

@Override
public SQLWarning getWarnings() throws SQLException {
	return myResultSet.getWarnings();
}

@Override
public void clearWarnings() throws SQLException {
    myResultSet.clearWarnings();
}

@Override
public String getCursorName() throws SQLException {
	return myResultSet.getCursorName();
}

@Override
public ResultSetMetaData getMetaData() throws SQLException {
	return myResultSet.getMetaData();
}

@Override
public Object getObject(int columnIndex) throws SQLException {
	if (checkCol(columnIndex)) return null;
	return getColumnValue(columnIndex - 1).getObject();
}

@Override
public Object getObject(String columnName) throws SQLException {
	return getObject(findColumn(columnName));
}

@Override
public int findColumn(String columnName) throws SQLException {
	return myResultSet.findColumn(columnName);
}

@Override
public Reader getCharacterStream(int columnIndex) throws SQLException {
	if (checkCol(columnIndex)) return null;
	String tmp = getColumnValue(columnIndex - 1).getString();
	return new CharArrayReader(tmp.toCharArray());
}

@Override
public Reader getCharacterStream(String columnName) throws SQLException {
	return getCharacterStream(findColumn(columnName));
}

@Override
public BigDecimal getBigDecimal(int columnIndex) throws SQLException {
	if (checkCol(columnIndex)) return null;
	return getColumnValue(columnIndex - 1).getBigDecimal();
}

@Override
public BigDecimal getBigDecimal(String columnName) throws SQLException {
	return getBigDecimal(findColumn(columnName));
}

@Override
public boolean isBeforeFirst() throws SQLException {
	return (currentRow == 0);
}

@Override
public boolean isAfterLast() throws SQLException {
    return isAfterLast;
}

@Override
public boolean isFirst() throws SQLException {
    return (currentRow == 1);
}

@Override
public boolean isLast() throws SQLException {
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("InvOpFO"));
	if (rowCount >= 0 && currentRow == rowCount) return true;
	return false;
}

@Override
public void beforeFirst() throws SQLException {
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("InvOpFO"));

	// Call to first() clears warnings and sets isAfterLast to false;
	// Call to previous() sets currentRow to zero.
	if (first()) previous();
}

@Override
public void afterLast() throws SQLException {
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("InvOpFO"));
	if (isAfterLast || rowCount == 0) return;

	// Call to last() clears warnings.
	// Call to next() sets currentRow and isAfterLast correctly.
	if (last()) next();
}

@Override
public boolean first() throws SQLException {
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("InvOpFO"));
	if (rowCount <= 0) return false;
	currentRow = 1;
	clearWarnings();
	isAfterLast = false;
	return true;
}

@Override
public boolean last() throws SQLException {
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("InvOpFO"));
	if (rowCount <= 0) return false;
	currentRow = rowCount;
	clearWarnings();
	isAfterLast = false;
	return true;
}

@Override
public int getRow() throws SQLException {
	return currentRow;
}

@Override
public boolean absolute(int row) throws SQLException {
	if (FSMajorVersion < 3 || row == 0) throw new SQLException(Driver.bun.getString("InvOpFO"));
	if (row == 0) return false; // not valid
	if (row > 0) {
	    if (row == (rowCount + 1)) isAfterLast = true;
	    else if (row > rowCount) return false;
	    currentRow = row;
		isAfterLast = false;
	}
	else {
	    if ((rowCount + 1 + row) <= 0) return false; // not valid
	    currentRow = rowCount + 1 + row;
		isAfterLast = false;
	}
	clearWarnings();
	return true;
}

@Override
public boolean relative(int rows) throws SQLException {
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("InvOpFO"));

	if (!isOnValidRow()) throw new SQLException("Not on valid row.");
	if (rows == 0) return true;
	if (rows < 0) {
	    if ((currentRow + rows) > 0) {
	        currentRow += rows;
	    	isAfterLast = false;
	    }
	    else return false; // invalid
	}
	else {
	    if ((currentRow + rows) <= (rowCount + 1)) {
	        currentRow += rows;
	    	isAfterLast = (currentRow == (rowCount + 1)); 
	    }
	    else return false; // invalid
	}
	clearWarnings();
	return true;
}

@Override
public boolean previous() throws SQLException {
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("InvOpFO"));
	if (currentRow > 0) {
		--currentRow;
		clearWarnings();
		isAfterLast = false;
		return true;
	}
	return false;
}

/**
 * Gives a hint as to the direction in which the rows in this
 * <code>ResultSet</code> object will be processed. 
 * The initial value is determined by the 
 * <code>Statement</code> object
 * that produced this <code>ResultSet</code> object.
 * The fetch direction may be changed at any time.
 *
 * @param direction an <code>int</code> specifying the suggested
 *        fetch direction; one of <code>ResultSet.FETCH_FORWARD</code>, 
 *        <code>ResultSet.FETCH_REVERSE</code>, or
 *        <code>ResultSet.FETCH_UNKNOWN</code>
 * @exception SQLException if a database access error occurs or
 * the result set type is <code>TYPE_FORWARD_ONLY</code> and the fetch
 * direction is not <code>FETCH_FORWARD</code>
 * @since 1.2
 * @see Statement#setFetchDirection
 * @see #getFetchDirection
 */
@Override
public void setFetchDirection(int direction) throws SQLException {
	myResultSet.setFetchDirection(direction);
}

@Override
public int getFetchDirection() throws SQLException {
	return myResultSet.getFetchDirection();
}

@Override
public void setFetchSize(int rows) throws SQLException {
    myResultSet.setFetchSize(rows);
}

@Override
public int getFetchSize() throws SQLException {
    return myResultSet.getFetchSize();
}

@Override
public int getType() throws SQLException {
    return myResultSet.getType();
}

@Override
public int getConcurrency() throws SQLException {
    return myResultSet.getConcurrency();
}

@Override
public boolean rowUpdated() throws SQLException {
    return myResultSet.rowUpdated();
}

@Override
public boolean rowInserted() throws SQLException {
	return myResultSet.rowInserted();
}

@Override
public boolean rowDeleted() throws SQLException {
	return myResultSet.rowDeleted();
}

@Override
public void updateNull(int columnIndex) throws SQLException {
	if (getConcurrency() == CONCUR_READ_ONLY) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (checkCol(columnIndex)) return;
	getColumnValue(columnIndex - 1).setNull();
}

@Override
public void updateBoolean(int columnIndex, boolean x) throws SQLException {
	if (getConcurrency() == CONCUR_READ_ONLY) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (checkCol(columnIndex)) return;
	getColumnValue(columnIndex - 1).setBoolean(x);
}

@Override
public void updateByte(int columnIndex, byte x) throws SQLException {
	if (getConcurrency() == CONCUR_READ_ONLY) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (checkCol(columnIndex)) return;
	getColumnValue(columnIndex - 1).setBytes(new byte[] { x });
}

@Override
public void updateShort(int arg0, short arg1) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoUpdates"));
}

@Override
public void updateInt(int columnIndex, int x) throws SQLException {
	if (getConcurrency() == CONCUR_READ_ONLY) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (checkCol(columnIndex)) return;
	getColumnValue(columnIndex - 1).setBytes(String.valueOf(x).getBytes());
}

@Override
public void updateLong(int columnIndex, long x) throws SQLException {
	if (getConcurrency() == CONCUR_READ_ONLY) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (checkCol(columnIndex)) return;
	getColumnValue(columnIndex - 1).setBytes(String.valueOf(x).getBytes());
}

@Override
public void updateFloat(int columnIndex, float x) throws SQLException {
	if (getConcurrency() == CONCUR_READ_ONLY) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (checkCol(columnIndex)) return;
	getColumnValue(columnIndex - 1).setBytes(String.valueOf(x).getBytes());
}

@Override
public void updateDouble(int columnIndex, double x) throws SQLException {
	if (getConcurrency() == CONCUR_READ_ONLY) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (checkCol(columnIndex)) return;
	getColumnValue(columnIndex - 1).setBytes(String.valueOf(x).getBytes());
}

@Override
public void updateBigDecimal(int columnIndex, BigDecimal x) throws SQLException {
	if (getConcurrency() == CONCUR_READ_ONLY) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (checkCol(columnIndex)) return;
	getColumnValue(columnIndex - 1).setBytes(x.toString().getBytes());
}

@Override
public void updateString(int arg0, String arg1) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoUpdates"));
}

@Override
public void updateBytes(int columnIndex, byte[] x) throws SQLException {
	if (getConcurrency() == CONCUR_READ_ONLY) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (checkCol(columnIndex)) return;
	getColumnValue(columnIndex - 1).setBytes(x);
}

@Override
public void updateDate(int columnIndex, Date x) throws SQLException {
	if (getConcurrency() == CONCUR_READ_ONLY) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (checkCol(columnIndex)) return;
	getColumnValue(columnIndex - 1).setDate(x);
}

@Override
public void updateTime(int arg0, Time arg1) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoUpdates"));
}

@Override
public void updateTimestamp(int arg0, Timestamp arg1) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoUpdates"));
}

@Override
public void updateAsciiStream(int columnIndex, InputStream x, int length) throws SQLException {
	if (getConcurrency() == CONCUR_READ_ONLY) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (checkCol(columnIndex)) return;
	byte[] ba = new byte[length];
	try {
		x.read(ba, 0, length);
	}
	catch (IOException e) {
		throw new SQLException(e.getLocalizedMessage());
	}
	getColumnValue(columnIndex - 1).setBytes(ba);
}

@Override
public void updateBinaryStream(int columnIndex, InputStream x, int length) throws SQLException {
	if (getConcurrency() == CONCUR_READ_ONLY) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (checkCol(columnIndex)) return;
	byte[] ba = new byte[length];
	try {
		x.read(ba, 0, length);
	}
	catch (IOException e) {
		throw new SQLException(e.getLocalizedMessage());
	}
	getColumnValue(columnIndex - 1).setBytes(ba);
}

@Override
public void updateCharacterStream(int columnIndex, Reader x, int length) throws SQLException {
	if (getConcurrency() == CONCUR_READ_ONLY) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (FSMajorVersion < 3) throw new SQLException(Driver.bun.getString("NoUpdates"));
	if (checkCol(columnIndex)) return;
	ByteArrayOutputStream baos = new ByteArrayOutputStream(length);
	BufferedWriter bw = new BufferedWriter(new OutputStreamWriter(baos));
	try {
		for (int i1 = 0; i1 < length; i1++) {
			bw.write(x.read());
		}
		bw.flush();
	}
	catch (IOException e) {
		throw new SQLException(e.getLocalizedMessage());
	}
	getColumnValue(columnIndex - 1).setBytes(baos.toByteArray());
}

@Override
public void updateObject(int arg0, Object arg1, int arg2) throws SQLException {
    throw new SQLException(Driver.bun.getString("NoUpdates"));
}

@Override
public void updateObject(int arg0, Object arg1) throws SQLException {
    throw new SQLException(Driver.bun.getString("NoUpdates"));
}

@Override
public void updateNull(String columnName) throws SQLException {
	updateNull(findColumn(columnName));
}

@Override
public void updateBoolean(String columnName, boolean x) throws SQLException {
	updateBoolean(findColumn(columnName), x);
}

@Override
public void updateByte(String columnName, byte x) throws SQLException {
	updateByte(findColumn(columnName), x);
}

@Override
public void updateShort(String columnName, short x) throws SQLException {
	updateShort(findColumn(columnName), x);
}

@Override
public void updateInt(String columnName, int x) throws SQLException {
	updateInt(findColumn(columnName), x);
}

@Override
public void updateLong(String columnName, long x) throws SQLException {
	updateLong(findColumn(columnName), x);
}

@Override
public void updateFloat(String columnName, float x) throws SQLException {
	updateFloat(findColumn(columnName), x);
}

@Override
public void updateDouble(String columnName, double x) throws SQLException {
	updateDouble(findColumn(columnName), x);
}

@Override
public void updateBigDecimal(String columnName, BigDecimal x) throws SQLException {
	updateBigDecimal(findColumn(columnName), x);
}

@Override
public void updateString(String columnName, String x) throws SQLException {
	updateString(findColumn(columnName), x);
}

@Override
public void updateBytes(String columnName, byte[] x) throws SQLException {
	updateBytes(findColumn(columnName), x);
}

@Override
public void updateDate(String columnName, Date x) throws SQLException {
	updateDate(findColumn(columnName), x);
}

@Override
public void updateTime(String columnName, Time x) throws SQLException {
	updateTime(findColumn(columnName), x);
}

@Override
public void updateTimestamp(String columnName, Timestamp x) throws SQLException {
	updateTimestamp(findColumn(columnName), x);
}

@Override
public void updateAsciiStream(String columnName, InputStream x, int length) throws SQLException {
	updateAsciiStream(findColumn(columnName), x, length);
}

@Override
public void updateBinaryStream(String columnName, InputStream x, int length) throws SQLException {
    updateBinaryStream(findColumn(columnName), x, length);
}

@Override
public void updateCharacterStream(String columnName, Reader x, int length) throws SQLException {
    updateCharacterStream(findColumn(columnName), x, length);
}

@Override
public void updateObject(String columnName, Object x, int length) throws SQLException {
    updateObject(findColumn(columnName), x, length);
}

@Override
public void updateObject(String columnName, Object x) throws SQLException {
    updateObject(findColumn(columnName), x);
}

@Override
public void insertRow() throws SQLException {
	throw new SQLException(Driver.bun.getString("NoUpdates"));
}

@Override
public void updateRow() throws SQLException {
	synchronized (connectMsg.getSock()) {
		if (getConcurrency() == CONCUR_READ_ONLY) {
			throw new SQLException(Driver.bun.getString("NoUpdates"));
		}
		StringBuffer sb = null;
		connectMsg.setFunc("PSUPDATE");
		connectMsg.setFileId(myResultSet.getRsid());
		for (int i1 = 0; i1 < myResultSet.getNumCols(); i1++) {
			if (!getColumnValue(i1).isUpdated()) continue;
			if (sb == null) sb = new StringBuffer();
			sb.append(myResultSet.getColumnName(i1));
			sb.append(" = '");
			sb.append(getColumnValue(i1).getString());
			sb.append("',");
		}
		if (sb == null) return;
		connectMsg.setData(sb.substring(0, sb.length()));
		try {
			connectMsg.sendMessage();
			connectMsg.recvMessage(); //Must flush the 'OK' from incoming pipe
		}
		catch (IOException ioex) {
			throw new SQLException(ioex.getLocalizedMessage());
		}
		for (int i1 = 0; i1 < myResultSet.getNumCols(); i1++) {
			if (getColumnValue(i1).isUpdated()) getColumnValue(i1).clearUpdated();
		}
	}
}

@Override
public void deleteRow() throws SQLException {
    synchronized (connectMsg.getSock()) {
		if (getConcurrency() == CONCUR_READ_ONLY || !isOnValidRow()) {
			throw new SQLException(Driver.bun.getString("NoUpdates"));
		}
		connectMsg.setFunc("PSDELETE");
		connectMsg.setFileId(myResultSet.getRsid());
		connectMsg.setData(null);
		try {
			connectMsg.sendMessage();
			connectMsg.recvMessage(); //Must flush the 'OK' from incoming pipe
		}
		catch (IOException ioex) {
			throw new SQLException(ioex.getLocalizedMessage());
		}
		for (int i1 = 0; i1 < myResultSet.getNumCols(); i1++) {
		    cachedRowData.remove((currentRow - 1) * myResultSet.getNumCols());
		}
		myResultSet.clearWarnings(); 
	}
}

@Override
public void refreshRow() throws SQLException {
	throw new SQLException(Driver.bun.getString("NoUpdates"));
}

@Override
public void cancelRowUpdates() throws SQLException {
	if (getConcurrency() == CONCUR_READ_ONLY) throw new SQLException(Driver.bun.getString("NoUpdates"));
	for (int i1 = 0; i1 < myResultSet.getNumCols(); i1++) {
		if (getColumnValue(i1).isUpdated()) getColumnValue(i1).revert();
	}
}

@Override
public void moveToInsertRow() throws SQLException {
	throw new SQLException(Driver.bun.getString("NoUpdates"));
}

@Override
public void moveToCurrentRow() throws SQLException {
	throw new SQLException(Driver.bun.getString("NoUpdates"));
}

@Override
public Statement getStatement() throws SQLException {
	return myStatement;
}

@Override
public Object getObject(int arg0, Map<String, Class<?>> arg1) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoTypeMaps"));
}

@Override
public Ref getRef(int arg0) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoType") + " : Ref");
}

@Override
public Blob getBlob(int arg0) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoType") + " : Blob");
}

@Override
public Clob getClob(int i1) throws SQLException {
	if (checkCol(i1)) return null;
	return new org.ptsw.fs.jdbc.Clob(getColumnValue(i1 - 1).getString());
}

@Override
public Array getArray(int arg0) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoType") + " : Array");
}

@Override
public Object getObject(String arg0, Map<String, Class<?>> arg1) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoTypeMaps"));
}

@Override
public Ref getRef(String arg0) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoType") + " : Ref");
}

@Override
public Blob getBlob(String arg0) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoType") + " : Blob");
}

@Override
public Clob getClob(String colName) throws SQLException {
    return getClob(findColumn(colName));
}

@Override
public Array getArray(String arg0) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoType") + " : Array");
}

@Override
public Date getDate(int columnIndex, Calendar cal) throws SQLException {
	if (checkCol(columnIndex)) return null;
	return getColumnValue(columnIndex - 1).getDate(cal);
}

@Override
public Date getDate(String columnName, Calendar cal) throws SQLException {
	return getDate(findColumn(columnName), cal);
}

@Override
public Time getTime(int columnIndex, Calendar cal) throws SQLException {
	if (checkCol(columnIndex)) return null;
	return getColumnValue(columnIndex - 1).getTime(cal);
}

@Override
public Time getTime(String columnName, Calendar cal) throws SQLException {
	return getTime(findColumn(columnName), cal);
}

@Override
public Timestamp getTimestamp(int columnIndex, Calendar cal) throws SQLException {
	if (checkCol(columnIndex)) return null;
	return getColumnValue(columnIndex - 1).getTimestamp(cal);
}

@Override
public Timestamp getTimestamp(String columnName, Calendar cal) throws SQLException {
	return getTimestamp(findColumn(columnName), cal);
}

@Override
public URL getURL(int arg0) throws SQLException {
	return null; // return null
}

@Override
public URL getURL(String arg0) throws SQLException {
	return null; // return null
}

@Override
public void updateRef(int arg0, Ref arg1) throws SQLException {
	throw new SQLException("update not supported");
}

@Override
public void updateRef(String arg0, Ref arg1) throws SQLException {
	throw new SQLException("update not supported");
}

@Override
public void updateBlob(int arg0, Blob arg1) throws SQLException {
	throw new SQLException("update not supported");
}

@Override
public void updateBlob(String arg0, Blob arg1) throws SQLException {
	throw new SQLException("update not supported");
}

@Override
public void updateClob(int arg0, Clob arg1) throws SQLException {
	throw new SQLException("update not supported");
}

@Override
public void updateClob(String arg0, Clob arg1) throws SQLException {
	throw new SQLException("update not supported");
}

@Override
public void updateArray(int arg0, Array arg1) throws SQLException {
	throw new SQLException("update not supported");
}

@Override
public void updateArray(String arg0, Array arg1) throws SQLException {
	throw new SQLException("update not supported");
}

/* (non-Javadoc)
 * @see javax.sql.RowSet#setAsciiStream(int, java.io.InputStream)
 */
@Override
public void setAsciiStream(int parameterIndex, InputStream x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setAsciiStream(String parameterName, InputStream x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setAsciiStream(String parameterName, InputStream x, int length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setBigDecimal(String parameterName, BigDecimal x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setBinaryStream(int parameterIndex, InputStream x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setBinaryStream(String parameterName, InputStream x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setBinaryStream(String parameterName, InputStream x, int length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setBlob(int parameterIndex, InputStream inputStream) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setBlob(String parameterName, Blob x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setBlob(String parameterName, InputStream inputStream) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setBlob(int parameterIndex, InputStream inputStream, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setBlob(String parameterName, InputStream inputStream, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setBoolean(String parameterName, boolean x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setByte(String parameterName, byte x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setBytes(String parameterName, byte[] x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setCharacterStream(int parameterIndex, Reader reader) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setCharacterStream(String parameterName, Reader reader) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setCharacterStream(String parameterName, Reader reader, int length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setClob(int parameterIndex, Reader reader) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setClob(String parameterName, Clob x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setClob(String parameterName, Reader reader) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setClob(int parameterIndex, Reader reader, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setClob(String parameterName, Reader reader, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setDate(String parameterName, Date x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setDate(String parameterName, Date x, Calendar cal) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setDouble(String parameterName, double x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setFloat(String parameterName, float x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setInt(String parameterName, int x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setLong(String parameterName, long x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setNCharacterStream(int parameterIndex, Reader value) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setNCharacterStream(String parameterName, Reader value) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setNCharacterStream(int parameterIndex, Reader value, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setNCharacterStream(String parameterName, Reader value, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setNClob(String parameterName, Reader reader) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setNClob(int parameterIndex, Reader reader) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setNClob(String parameterName, Reader reader, long length) throws SQLException {
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
public void setNString(String parameterName, String value) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setNull(String parameterName, int sqlType) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setNull(String parameterName, int sqlType, String typeName) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setObject(String parameterName, Object x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setObject(String parameterName, Object x, int targetSqlType) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setObject(String parameterName, Object x, int targetSqlType, int scale) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setShort(String parameterName, short x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setString(String parameterName, String x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setTime(String parameterName, Time x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setTime(String parameterName, Time x, Calendar cal) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void setTimestamp(String parameterName, Timestamp x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void setTimestamp(String parameterName, Timestamp x, Calendar cal) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void setURL(int parameterIndex, URL x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public int getHoldability() throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public Reader getNCharacterStream(int columnIndex) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public Reader getNCharacterStream(String columnLabel) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public String getNString(int columnIndex) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public String getNString(String columnLabel) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public boolean isClosed() throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void updateAsciiStream(int columnIndex, InputStream x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateAsciiStream(String columnLabel, InputStream x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateAsciiStream(int columnIndex, InputStream x, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateAsciiStream(String columnLabel, InputStream x, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateBinaryStream(int columnIndex, InputStream x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateBinaryStream(String columnLabel, InputStream x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateBinaryStream(int columnIndex, InputStream x, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateBinaryStream(String columnLabel, InputStream x, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateBlob(int columnIndex, InputStream inputStream) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateBlob(String columnLabel, InputStream inputStream) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateBlob(int columnIndex, InputStream inputStream, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateBlob(String columnLabel, InputStream inputStream, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateCharacterStream(int columnIndex, Reader x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateCharacterStream(String columnLabel, Reader reader) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateCharacterStream(int columnIndex, Reader x, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateCharacterStream(String columnLabel, Reader reader, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateClob(int columnIndex, Reader reader) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateClob(String columnLabel, Reader reader) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateClob(int columnIndex, Reader reader, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateClob(String columnLabel, Reader reader, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateNCharacterStream(int columnIndex, Reader x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateNCharacterStream(String columnLabel, Reader reader) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateNCharacterStream(int columnIndex, Reader x, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateNCharacterStream(String columnLabel, Reader reader, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateNClob(int columnIndex, Reader reader) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateNClob(String columnLabel, Reader reader) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateNClob(int columnIndex, Reader reader, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateNClob(String columnLabel, Reader reader, long length) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateNString(int columnIndex, String nString) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateNString(String columnLabel, String nString) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public boolean isWrapperFor(Class<?> iface) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public <T> T unwrap(Class<T> iface) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public RowId getRowId(int columnIndex) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public RowId getRowId(String columnLabel) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void updateRowId(int columnIndex, RowId x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateRowId(String columnLabel, RowId x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateNClob(int columnIndex, NClob nClob) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateNClob(String columnLabel, NClob nClob) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public NClob getNClob(int columnIndex) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public NClob getNClob(String columnLabel) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public SQLXML getSQLXML(int columnIndex) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public SQLXML getSQLXML(String columnLabel) throws SQLException {
	throw new SQLFeatureNotSupportedException();
}

@Override
public void updateSQLXML(int columnIndex, SQLXML xmlObject) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void updateSQLXML(String columnLabel, SQLXML xmlObject) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void setSQLXML(int parameterIndex, SQLXML xmlObject) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void setSQLXML(String parameterName, SQLXML xmlObject) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public void setRowId(int parameterIndex, RowId x) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
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
public void setNClob(int parameterIndex, NClob value) throws SQLException {
	throw new SQLFeatureNotSupportedException();
	
}

@Override
public <T> T getObject(int columnIndex, Class<T> type) throws SQLException {
	// TODO Auto-generated method stub
	return null;
}

@Override
public <T> T getObject(String columnLabel, Class<T> type) throws SQLException {
	// TODO Auto-generated method stub
	return null;
}

}
