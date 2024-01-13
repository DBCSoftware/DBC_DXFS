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
 * A Ref is a reference to an SQL structured value in the database.  A
 * Ref can be saved to persistent storage.  A Ref is dereferenced by
 * passing it as a parameter to an SQL statement and executing the 
 * statement.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Ref data type.
 *
 */

public final class Ref implements java.sql.Ref {

/**
 * Get the fully qualified SQL structured type name of the 
 * referenced item.
 * 
 * <P><B>FS2 NOTE:</B> FS2 does not support the Ref data type.
 *
 * @return fully qualified SQL structured type name of the referenced item.
 *
 */
@Override
public String getBaseTypeName() throws SQLException {
	throw new SQLException(Driver.bun.getString("NoType") + " : Ref");
}

/* (non-Javadoc)
 * @see java.sql.Ref#getObject(java.util.Map)
 */
@Override
public Object getObject(Map<String,Class<?>> arg0) throws SQLException {
	return null;
}

@Override
public Object getObject() throws SQLException {
	return null;
}

@Override
public void setObject(Object arg0) throws SQLException {
}

}
