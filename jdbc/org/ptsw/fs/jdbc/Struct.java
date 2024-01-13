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

/**
 * JDBC 2.0
 *
 * <p>The Struct interface represents the default mapping for an SQL
 * structured type.  By default, the contents of a Struct are
 * materialized when the structured instance is produced.  And,
 * by default, an instance of Struct is valid as long as the 
 * application has a reference to it.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Struct data type.
 *
 */

public final class Struct implements java.sql.Struct {

/**
 * @returns the fully qualified type name of the SQL structured 
 * type for which this is the generic representation.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Struct data type.
 *
 */
@Override
public String getSQLTypeName() throws SQLException {
	throw new SQLException(Driver.bun.getString("NoType") + " : Struct");
}

/**
 * Produce the ordered values of the attributes of the SQL 
 * structured type. Use the type-map associated with the 
 * connection for customizations of the type-mappings.
 *
 * Conceptually, this method calls getObject() on each attribute
 * of the structured type and returns a Java array containing 
 * the result.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Struct data type.
 *
 * @return an array containing the ordered attribute values
 */
@Override
public Object[] getAttributes() throws SQLException {
	throw new SQLException(Driver.bun.getString("NoType") + " : Struct");
}

/**
 * Produce the ordered values of the attributes of the SQL 
 * structured type. Use the given @map for type-map 
 * customizations.
 *
 * Conceptually, this method calls getObject() on each attribute
 * of the structured type and returns a Java array containing
 * the result.
 *
 * <P><B>FS2 NOTE:</B> FS2 does not support the Struct data type.
 *
 * @param map contains mapping of SQL type names to Java classes
 * @return an array containing the ordered attribute values
 *
 * @see java.sql.Struct#getAttributes(java.util.Map)
 */
@Override
public Object[] getAttributes(java.util.Map<String,Class<?>> map) throws SQLException {
	throw new SQLException(Driver.bun.getString("NoType") + " : Struct");
}
}
