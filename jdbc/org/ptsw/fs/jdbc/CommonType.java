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
import java.sql.SQLException;
import java.util.Calendar;

final class CommonType {

	private static final BigDecimal bdZero = new BigDecimal(0);
	private static final byte[] baBigOne = new BigDecimal(1).toString().getBytes();
	private static final byte[] baBigZero = new BigDecimal(0).toString().getBytes();
	private final String size;
	private final char type;
	private byte[] value;
	private final byte[] origValue;
	private boolean nullState = false;
	private boolean updated = false;
	private final boolean origNullState;

	CommonType(final byte[] ba, final char type, final String size) {
		this.type = type;
		this.size = size;
		value = ba;
		origValue = value.clone();
		origNullState = (type != 'C' && isAllBlanks(value));
		nullState = origNullState;
	}

	String getString() {
		if (nullState) return null;
		return new String(value);
	}

	void revert() {
		value = origValue.clone();
		nullState = origNullState;
	}

	void setNull() {
		if (type != 'C') {
			nullState = true;
			updated = true;
		}
	}

	boolean isNull() {
		return nullState;
	}

	Object getObject() throws SQLException {
		if (nullState) return null;
		if (type == 'C')
			return getString();
		else if (type == 'N')
			return getBigDecimal();
		else if (type == 'D')
			return getDate();
		else if (type == 'T')
			return getTime();
		else if (type == 'S')
			return getTimestamp();
		else
			return getString();
	}

	byte[] getBytes() {
		if (nullState) return null;
		return value;
	}

	void setBytes(byte[] ba) {
		value = ba;
		updated = true;
	}

	boolean isUpdated() {
		return updated;
	}

	void clearUpdated() {
		updated = false;
	}

	BigDecimal getBigDecimal(final int scale) throws SQLException {
		if (nullState) return null;
		String s1 = getString().trim();
		BigDecimal bd;
		try {
			bd = new BigDecimal(s1);
			if (bd.scale() != scale) bd = bd.setScale(scale, RoundingMode.HALF_UP);
		}
		catch (NumberFormatException ex) {
			throw new SQLException(Driver.bun.getString("BadNumber") + " '" + s1 + '\'');
		}
		return bd;
	}

	BigDecimal getBigDecimal() throws SQLException {
		if (nullState) return null;
		String s1 = getString().trim();
		try {
			return new BigDecimal(s1);
		}
		catch (NumberFormatException ex) {
			throw new SQLException(Driver.bun.getString("BadNumber") + " '" + s1 + '\'');
		}
	}

	boolean getBoolean() throws SQLException {
		if (nullState) return false;
		boolean rc;
		switch (type) {
		case 'N':
			BigDecimal bd = getBigDecimal();
			if (bd.compareTo(bdZero) == 0)
				rc = false;
			else
				rc = true;
			break;
		case 'C':
			String val = getString();
			if (val.equalsIgnoreCase("Y"))
				rc = true;
			else if (val.equalsIgnoreCase("N"))
				rc = false;
			else
				throw new SQLException("Invalid Boolean Value");
			break;
		default:
			throw new SQLException("Invalid Boolean Value");
		}
		return rc;
	}

	void setBoolean(boolean b) throws SQLException {
		switch (type) {
		case 'N':
			value = (b) ? baBigOne : baBigZero;
			updated = true;
			break;
		case 'C':
			value[0] = (byte) ((b) ? 'Y' : 'N');
			updated = true;
			break;
		default:
			throw new SQLException("Invalid column type for boolean");
		}
	}

	double getDouble() throws SQLException {
		if (nullState) return 0;
		String s1 = getString().trim();
		try {
			return Double.parseDouble(s1);
		}
		catch (NumberFormatException ex) {
			throw new SQLException(Driver.bun.getString("BadNumber") + " '" + s1 + '\'');
		}
	}

	float getFloat() throws SQLException {
		if (nullState) return 0;
		String s1 = getString().trim();
		try {
			return Float.parseFloat(s1);
		}
		catch (NumberFormatException ex) {
			throw new SQLException(Driver.bun.getString("BadNumber") + " '" + s1 + '\'');
		}
	}

	int getInt() throws SQLException {
		if (nullState) return 0;
		String s1 = getString().trim();
		try {
			return Integer.parseInt(s1);
		}
		catch (NumberFormatException ex) {
			throw new SQLException(Driver.bun.getString("BadNumber") + " '" + s1 + '\'');
		}
	}

	long getLong() throws SQLException {
		if (nullState) return 0;
		String s1 = getString().trim();
		try {
			return Long.parseLong(s1);
		}
		catch (NumberFormatException ex) {
			throw new SQLException(Driver.bun.getString("BadNumber") + " '" + s1 + '\'');
		}
	}

	short getShort() throws SQLException {
		if (nullState) return 0;
		String s1 = getString().trim();
		try {
			return Short.parseShort(s1);
		}
		catch (NumberFormatException ex) {
			throw new SQLException(Driver.bun.getString("BadNumber") + " '" + s1 + '\'');
		}
	}

	void setDate(java.sql.Date x) throws SQLException {
		if (type == 'N' || type == 'T') throw new SQLException("Invalid column type for Date");
		if (type == 'C') {
			if (Integer.parseInt(size) < 10) throw new SQLException("Column width too small for Date");
			value = x.toString().getBytes();
		}
		else if (type == 'D') {
			value = x.toString().getBytes();
		}
		else if (type == 'S') {
			String baseTS = x.toString() + " 00:00:00.000000000";
			int i1 = Integer.parseInt(size);
			value = baseTS.substring(0, i1).getBytes();
		}
		updated = true;
	}

	/**
	 * Modified 01/29/01 to call the other routine which includes Timestamp
	 */
	java.sql.Date getDate() throws SQLException {
		return getDate(Calendar.getInstance());
	}

	/**
	 * Modified 01/29/01 to include Timestamp
	 */
	java.sql.Date getDate(Calendar cal) throws SQLException {
		if (nullState) return null;
		int iYear = 1970;
		int iMonth = Calendar.JANUARY;
		int iDay = 1;
		if ((type == 'N') || (type == 'T')) throw new SQLException(Driver.bun.getString("BadDate"));
		if (type == 'C') {
			if (value.length < 10) throw new SQLException(Driver.bun.getString("BadDate"));
		}
		try {
			if (type == 'D' || type == 'C' || type == 'S') {
				iYear = Integer.parseInt(new String(value, 0, 4));
				iMonth = Integer.parseInt(new String(value, 5, 2)) - 1;
				iDay = Integer.parseInt(new String(value, 8, 2));
			}
			else
				throw new SQLException(Driver.bun.getString("BadTime"));
			if (iYear < 0) throw new NumberFormatException();
			if (iMonth < 0 || iMonth > 11) throw new NumberFormatException();
			if (iDay < 0 || iDay > 31) throw new NumberFormatException();
		}
		catch (NumberFormatException ex) {
			throw new SQLException(Driver.bun.getString("BadDate"));
		}
		cal.set(iYear, iMonth, iDay);
		try {
			return new java.sql.Date(cal.getTime().getTime());
		}
		catch (Exception ex) {
			throw new SQLException(Driver.bun.getString("BadDate"));
		}
	}

	/**
	 * Modified 01/29/01 to call the other routine which includes Timestamp
	 */
	java.sql.Time getTime() throws SQLException {
		return getTime(Calendar.getInstance());
	}

	/**
	 * Modified 01/29/01 to include Timestamp
	 */
	java.sql.Time getTime(Calendar cal) throws SQLException {
		if (nullState) return null;
		int iHour = 0;
		int iMinute = 0;
		int iSecond = 0;
		if ((type == 'N') || (type == 'D')) throw new SQLException(Driver.bun.getString("BadTime"));
		if (type == 'C') {
			if (value.length < 8) throw new SQLException(Driver.bun.getString("BadTime"));
		}
		try {
			if (type == 'T' || type == 'C') {
				iHour = Integer.parseInt(new String(value, 0, 2));
				iMinute = Integer.parseInt(new String(value, 3, 2));
				iSecond = Integer.parseInt(new String(value, 6, 2));
			}
			else if (type == 'S') {
				iHour = Integer.parseInt(new String(value, 9, 2));
				iMinute = Integer.parseInt(new String(value, 12, 2));
				iSecond = Integer.parseInt(new String(value, 15, 2));
			}
			else
				throw new SQLException(Driver.bun.getString("BadTime"));
			if (iHour < 0 || iHour > 24) throw new NumberFormatException();
			if (iMinute < 0 || iMinute > 60) throw new NumberFormatException();
			if (iSecond < 0 || iSecond > 60) throw new NumberFormatException();
		}
		catch (NumberFormatException ex) {
			throw new SQLException(Driver.bun.getString("BadTime"));
		}
		cal.set(Calendar.HOUR_OF_DAY, iHour);
		cal.set(Calendar.MINUTE, iMinute);
		cal.set(Calendar.SECOND, iSecond);
		try {
			return new java.sql.Time(cal.getTime().getTime());
		}
		catch (Exception ex) {
			throw new SQLException(Driver.bun.getString("BadTime"));
		}
	}

	/**
	 * New 01/29/01
	 */
	java.sql.Timestamp getTimestamp() throws SQLException {
		return getTimestamp(Calendar.getInstance());
	}

	/**
	 * New 01/29/01
	 */
	java.sql.Timestamp getTimestamp(Calendar cal) throws SQLException {
		if (nullState) return null;
		int iYear = 1970;
		int iMonth = Calendar.JANUARY;
		int iDay = 1;
		int iHour = 0;
		int iMinute = 0;
		int iSecond = 0;
		if (type == 'N') throw new SQLException(Driver.bun.getString("BadTime"));
		if (type == 'C') {
			if (value.length < 19) throw new SQLException(Driver.bun.getString("BadTime"));
		}
		try {
			if (type == 'S' || type == 'C' || type == 'D') {
				iYear = Integer.parseInt(new String(value, 0, 4));
				iMonth = Integer.parseInt(new String(value, 5, 2)) - 1;
				iDay = Integer.parseInt(new String(value, 8, 2));
				if (iYear < 1900) throw new NumberFormatException();
				if (iMonth < 0 || iMonth > 11) throw new NumberFormatException();
				if (iDay < 0 || iDay > 31) throw new NumberFormatException();
			}
			if (type == 'S' || type == 'C' || type == 'T') {
				if (type == 'S' || type == 'C') {
					iHour = Integer.parseInt(new String(value, 11, 2));
					iMinute = Integer.parseInt(new String(value, 14, 2));
					iSecond = Integer.parseInt(new String(value, 17, 2));
				}
				else {
					iHour = Integer.parseInt(new String(value, 0, 2));
					iMinute = Integer.parseInt(new String(value, 3, 2));
					iSecond = Integer.parseInt(new String(value, 6, 2));
				}
				if (iHour < 0 || iHour > 24) throw new NumberFormatException();
				if (iMinute < 0 || iMinute > 60) throw new NumberFormatException();
				if (iSecond < 0 || iSecond > 60) throw new NumberFormatException();
			}
		}
		catch (NumberFormatException ex) {
			throw new SQLException(Driver.bun.getString("BadTime"));
		}
		if (type != 'T') cal.set(iYear, iMonth, iDay);
		if (type != 'D') {
			cal.set(Calendar.HOUR_OF_DAY, iHour);
			cal.set(Calendar.MINUTE, iMinute);
			cal.set(Calendar.SECOND, iSecond);
		}
		try {
			return new java.sql.Timestamp(cal.getTime().getTime());
		}
		catch (Exception ex) {
			throw new SQLException(Driver.bun.getString("BadTime"));
		}
	}

	static boolean isAllBlanks(byte[] ba) {

		for (int i1 = 0; i1 < ba.length; i1++) {
			if (ba[i1] != ' ') return false;
		}
		return true;
	}

}
