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

import java.io.IOException;
import java.net.Socket;
import java.sql.Array;
import java.sql.Blob;
import java.sql.Clob;
import java.sql.DriverManager;
import java.sql.NClob;
import java.sql.SQLClientInfoException;
import java.sql.SQLException;
import java.sql.SQLFeatureNotSupportedException;
import java.sql.SQLWarning;
import java.sql.SQLXML;
import java.sql.Savepoint;
import java.sql.Struct;
import java.util.Enumeration;
import java.util.Map;
import java.util.Properties;
import java.util.Vector;
import java.util.concurrent.Executor;

/**
 * <P>
 * A Connection represents a session with a specific database. Within the
 * context of a Connection, SQL statements are executed and results are
 * returned.
 *
 * <P>
 * A Connection's database is able to provide information describing its tables,
 * its supported SQL grammar, its stored procedures, the capabilities of this
 * connection, etc. This information is obtained with the
 * <code>getMetaData</code> method.
 *
 * <P>
 * <B>Note:</B> By default the Connection automatically commits changes after
 * executing each statement. If auto commit has been disabled, an explicit
 * <code>commit</code> must be called explicitly; otherwise, database changes
 * will not be saved.
 *
 * @see DriverManager#getConnection
 * @see Statement
 * @see ResultSet
 * @see DatabaseMetaData
 *      <P>
 *      Methods that are new in the JDBC 2.0 API are tagged @since 1.2.
 */

public final class Connection implements java.sql.Connection, AutoCloseable {

	private static final boolean autoCommitState = true;
	private Message connectMsg;
	private final Vector<SQLWarning> warnings = new Vector<>();
	private boolean connectionClosed;
	private final String url;
	private final Driver driver; // the Driver object that owns this connection.
	private Vector<Statement> stmtVector; // A list of statement objects created
											// by myself.
	private boolean readOnly = false;
	private String userName; // User name we are logged in with
	private int FSMajorVersion; // Version of FS server that we are connected to
	private String FSVersion; // Longer version of above, with minor release
								// stuff.
	private static final FSproperties[] fsprops = { null, null, null, new FSproperties(3), new FSproperties(4),
			new FSproperties(5), new FSproperties(6) };
	private Thread keepAliveThread;

	Connection(Message msg, String url, Driver driver) {
		connectMsg = msg;
		connectionClosed = false;
		this.url = url;
		this.driver = driver;
		stmtVector = new Vector<Statement>();
		if (driver.getkeepalive()) {
			keepAliveThread = new SendKeepAliveThread();
			keepAliveThread.start();
		}
	}

	/**
	 * Clears all warnings reported for this <code>Connection</code> object.
	 * After a call to this method, the method <code>getWarnings</code> returns
	 * null until a new warning is reported for this Connection.
	 *
	 */
	@Override
	public void clearWarnings() {
		warnings.removeAllElements();
	}

	/**
	 * In some cases, it is desirable to immediately release a Connection's
	 * database and JDBC resources instead of waiting for them to be
	 * automatically released; the close method provides this immediate release.
	 *
	 * <P>
	 * <B>Note:</B> A Connection is automatically closed when it is garbage
	 * collected. Certain fatal errors also result in a closed Connection.
	 *
	 * @exception SQLException
	 *                if a database-access error occurs.
	 */
	@Override
	public void close() throws SQLException {
		synchronized (connectMsg.getSock()) {
			clearWarnings();
			if (connectionClosed) {
				if (Driver.traceOn()) Driver.trace("Connection@close : connection already closed.");
				return;
			}
			connectMsg.setFunc("DISCNCT");
			connectMsg.setData(null);
			try {
				connectMsg.sendMessage();
				connectMsg.recvMessage();
				if (driver.getkeepalive()) keepAliveThread.interrupt();
				Socket sock = connectMsg.getSock();
				sock.close();
			}
			catch (IOException iox) {
				throw new SQLException(iox.getLocalizedMessage());
			}
			connectMsg = null;
			stmtVector.removeAllElements();
			connectionClosed = true;
		}
	}

	/**
	 * Commit makes all changes made since the previous commit/rollback
	 * permanent and releases any database locks currently held by the
	 * Connection. This method should only be used when auto commit has been
	 * disabled.
	 *
	 * <P>
	 * <B>FS NOTE:</B> This call will be ignored. The FS system does not support
	 * transactions, thus auto commit is <I>always</I> enabled.
	 *
	 * @see #setAutoCommit
	 */
	@Override
	public void commit() {
	}

	/**
	 * Creates a <code>Statement</code> object for sending SQL statements to the
	 * database. SQL statements without parameters are normally executed using
	 * Statement objects. If the same SQL statement is executed many times, it
	 * is more efficient to use a <code>PreparedStatement</code> object.
	 *
	 * Result sets created using the returned Statement will have forward-only
	 * type, and read-only concurrency, by default.
	 *
	 * @return a new Statement object
	 * @exception SQLException
	 *                if a database-access error occurs.
	 */
	@Override
	public java.sql.Statement createStatement() throws SQLException {
		if (connectionClosed) { throw new SQLException(Driver.bun.getString("ConnClosed")); }
		Statement stmt = new Statement(connectMsg, this);
		stmtVector.addElement(stmt);
		return stmt;
	}

	/**
	 * Get the current auto commit state.
	 *
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support transactions and is
	 * therefore always in auto commit mode. This method will always return
	 * true.
	 *
	 * @return always true.
	 * @see #setAutoCommit
	 */
	@Override
	public boolean getAutoCommit() {
		return autoCommitState;
	}

	/**
	 * Return the Connection's current catalog name.
	 *
	 * <P>
	 * <B>FS NOTE:</B> The FS system does not support catalogs. This method will
	 * always return null.
	 *
	 * @return always null
	 */
	@Override
	public String getCatalog() {
		return null;
	}

	/**
	 * Gets the metadata regarding this connection's database. A Connection's
	 * database is able to provide information describing its tables, its
	 * supported SQL grammar, its stored procedures, the capabilities of this
	 * connection, and so on. This information is made available through a
	 * DatabaseMetaData object.
	 *
	 * @return a DatabaseMetaData object for this Connection
	 * @exception SQLException
	 *                if a database-access error occurs.
	 */
	@Override
	public java.sql.DatabaseMetaData getMetaData() throws SQLException {
		if (connectionClosed) { throw new SQLException(Driver.bun.getString("ConnClosed")); }
		DatabaseMetaData dbmd = new DatabaseMetaData();
		dbmd.init(this);
		return dbmd;
	}

	/**
	 * Get this Connection's current transaction isolation mode.
	 *
	 * <P>
	 * <B>FS NOTE:</B> The FS system does not support transactions.
	 *
	 * @return TRANSACTION_NONE
	 */
	@Override
	public int getTransactionIsolation() {
		return TRANSACTION_NONE;
	}

	/**
	 * The first warning reported by calls on this Connection is returned.
	 *
	 * <P>
	 * <B>Note:</B> Subsequent warnings will be chained to this SQLWarning.
	 *
	 * @return the first SQLWarning or null
	 */
	@Override
	public SQLWarning getWarnings() {
		if (warnings.isEmpty()) return null;
		return warnings.firstElement();
	}

	/**
	 * Tests to see if a Connection is closed.
	 *
	 * @return true if the connection is closed; false if it's still open
	 */
	@Override
	public boolean isClosed() {
		return connectionClosed;
	}

	/**
	 * Tests to see if the connection is in read-only mode.
	 *
	 * @return true if connection is read-only
	 */
	@Override
	public boolean isReadOnly() {
		return readOnly;
	}

	/**
	 * Converts the given SQL statement into the system's native SQL grammar. A
	 * driver may convert the JDBC sql grammar into its system's native SQL
	 * grammar prior to sending it; this method returns the native form of the
	 * statement that the driver would have sent.
	 *
	 * @param sql
	 *            a SQL statement that may contain one or more '?' parameter
	 *            placeholders
	 * @return the native form of this statement
	 * @exception SQLException
	 *                if a database-access error occurs.
	 */

	/*
	 * CODE: possible to change this so it works? (prepared statements, escape
	 * sequences)
	 */
	@Override
	public String nativeSQL(String query) {
		return query;
	}

	/**
	 * Creates a <code>CallableStatement</code> object for calling database
	 * stored procedures. The <code>CallableStatement</code> object provides
	 * methods for setting up its IN and OUT parameters, and methods for
	 * executing the call to a stored procedure.
	 *
	 * <P>
	 * <B>Note:</B> This method is optimized for handling stored procedure call
	 * statements. Some drivers may send the call statement to the database when
	 * the method <code>prepareCall</code> is done; others may wait until the
	 * <code>CallableStatement</code> object is executed. This has no direct
	 * effect on users; however, it does affect which method throws certain
	 * SQLExceptions.
	 *
	 * Result sets created using the returned CallableStatement will have
	 * forward-only type and read-only concurrency, by default.
	 *
	 * <P>
	 * <B>FS2 NOTE:</B> This method should not be used. The FS2 system does not
	 * support stored procedures. This method will always result in an
	 * exception.
	 *
	 * @param sql
	 *            a SQL statement that may contain one or more '?' parameter
	 *            placeholders. Typically this statement is a JDBC function call
	 *            escape string.
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public java.sql.CallableStatement prepareCall(String sql) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 *
	 * Creates a <code>PreparedStatement</code> object for sending parameterized
	 * SQL statements to the database.
	 *
	 * A SQL statement with or without IN parameters can be pre-compiled and
	 * stored in a PreparedStatement object. This object can then be used to
	 * efficiently execute this statement multiple times.
	 *
	 * <P>
	 * <B>Note:</B> This method is optimized for handling parametric SQL
	 * statements that benefit from precompilation. If the driver supports
	 * precompilation, the method <code>prepareStatement</code> will send the
	 * statement to the database for precompilation. Some drivers may not
	 * support precompilation. In this case, the statement may not be sent to
	 * the database until the <code>PreparedStatement</code> is executed. This
	 * has no direct effect on users; however, it does affect which method
	 * throws certain SQLExceptions.
	 *
	 *
	 * Result sets created using the returned PreparedStatement will have
	 * forward-only type and read-only concurrency, by default.
	 *
	 * @param sql
	 *            a SQL statement that may contain one or more '?' IN parameter
	 *            placeholders
	 * @return a new PreparedStatement object containing the pre-compiled
	 *         statement
	 * @exception SQLException
	 *                if a database-access error occurs.
	 */
	@Override
	public java.sql.PreparedStatement prepareStatement(String sql) throws SQLException {
		if (connectionClosed) { throw new SQLException(Driver.bun.getString("ConnClosed")); }
		PreparedStatement ps = new PreparedStatement(sql, connectMsg, this);
		stmtVector.addElement(ps);
		return ps;
	}

	/**
	 * Drops all changes made since the previous commit/rollback and releases
	 * any database locks currently held by this Connection. This method should
	 * be used only when auto- commit has been disabled.
	 *
	 * <P>
	 * <B>FS2 NOTE:</B> This call should not be used. The FS2 system does not
	 * support transactions, thus auto commit is <I>always</I> enabled.
	 *
	 * @exception SQLException
	 *                always.
	 * @see #setAutoCommit
	 */
	@Override
	public void rollback() throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCommitRollback"));
	}

	/**
	 * Sets this connection's auto-commit mode. If a connection is in
	 * auto-commit mode, then all its SQL statements will be executed and
	 * committed as individual transactions. Otherwise, its SQL statements are
	 * grouped into transactions that are terminated by a call to either the
	 * method <code>commit</code> or the method <code>rollback</code>. By
	 * default, new connections are in auto-commit mode.
	 *
	 * The commit occurs when the statement completes or the next execute
	 * occurs, whichever comes first. In the case of statements returning a
	 * ResultSet, the statement completes when the last row of the ResultSet has
	 * been retrieved or the ResultSet has been closed. In advanced cases, a
	 * single statement may return multiple results as well as output parameter
	 * values. In these cases the commit occurs when all results and output
	 * parameter values have been retrieved.
	 *
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support transactions. The driver
	 * is always in auto commit mode.
	 *
	 * @param autoCommit
	 *            true does nothing; false throws an exception
	 * @exception SQLException
	 *                if method attempts to disable auto commit.
	 */
	@Override
	public void setAutoCommit(boolean enableAutoCommit) throws SQLException {
		if (!enableAutoCommit) throw new SQLException(Driver.bun.getString("NoCommitRollback"));
	}

	/**
	 * Sets a catalog name in order to select a subspace of this Connection's
	 * database in which to work. If the driver does not support catalogs, it
	 * will silently ignore this request.
	 *
	 * <P>
	 * <B>FS NOTE:</B> The FS system does not support catalogs. This method will
	 * be ignored.
	 */
	@Override
	public void setCatalog(String s) {
	}

	/**
	 * Puts this connection in read-only mode as a hint to enable database
	 * optimizations.
	 *
	 * <P>
	 * <B>Note:</B> This method cannot be called while in the middle of a
	 * transaction. <I>(FS2 does not support transactions.)</I>
	 *
	 * @param readOnly
	 *            true enables read-only mode; false disables read-only mode.
	 * @exception SQLException
	 *                if a database-access error occurs.
	 */
	@Override
	public void setReadOnly(boolean readOnly) throws SQLException {
		synchronized (connectMsg.getSock()) {
			if (connectionClosed || readOnly == this.readOnly) return;
			connectMsg.setFunc("SETATTR");
			connectMsg.setData(readOnly ? "READONLY Y" : "READONLY N");
			try {
				connectMsg.sendMessage();
				connectMsg.recvMessage();
				this.readOnly = readOnly;
			}
			catch (IOException iox) {
				throw new SQLException(iox.getLocalizedMessage());
			}
		}
	}

	/**
	 * Attempts to change the transaction isolation level to the one given. The
	 * constants defined in the interface <code>Connection</code> are the
	 * possible transaction isolation levels.
	 *
	 * <P>
	 * <B>Note:</B> This method cannot be called while in the middle of a
	 * transaction.
	 *
	 * <P>
	 * <B>FS2 NOTE:</B> The FS2 system does not support transactions. This
	 * method will always result in an SQLException.
	 *
	 * @param level
	 *            one of the TRANSACTION_* isolation values with the exception
	 *            of TRANSACTION_NONE; some databases may not support other
	 *            values
	 * @exception SQLException
	 *                always.
	 * @see DatabaseMetaData#supportsTransactionIsolationLevel
	 */
	@Override
	public void setTransactionIsolation(int level) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoTransactions"));
	}

	/*
	 * Package visibility accessor routine to get the current url.
	 *
	 * @return String, the current url
	 */
	String getUrl() {
		return url;
	}

	/*
	 * Package visibility accessor routine to get the current Message object.
	 *
	 * @return a Message object
	 */
	Message getMessage() {
		return connectMsg;
	}

	/*
	 * Package visibility accessor routine to get the owning Driver object.
	 *
	 * @return a Driver object
	 */
	Driver getDriver() {
		return driver;
	}

	/*
	 * Package visibility accessor routine to get the list of statements.
	 *
	 * @return an Enumeration object for stmtVector
	 */
	Enumeration<Statement> getStatementList() {
		return stmtVector.elements();
	}

	/*
	 * Package visibility accesor routine to get the size of the statement
	 * vector.
	 *
	 * @return size of Statement Vector
	 */
	int getNumStatements() {
		return stmtVector.size();
	}

	/*
	 * Package visibility accesor routine to remove a Statement from the
	 * Statement Vector (upon closing the Statement)
	 */
	boolean closeStatement(Statement stmt) {
		return stmtVector.removeElement(stmt);
	}

	void setUserName(String un) {
		userName = un;
	}

	String getUserName() {
		return userName;
	}

	void setFSVersion(String ver) {
		FSVersion = ver;
		FSMajorVersion = Driver.parseMajorVersion(ver);
	}

	String getFSVersion() {
		return FSVersion;
	}

	int getFSMajorVersion() {
		return FSMajorVersion;
	}

	FSproperties getFSproperties() {
		if (FSMajorVersion > 0 && FSMajorVersion < fsprops.length)
			return fsprops[FSMajorVersion];
		else
			throw new IllegalArgumentException("FSMajorVersion is unknown");
	}

//	@Override
//	protected void finalize() throws Throwable {
//		try {
//			close();
//		}
//		finally {
//			super.finalize();
//		}
//	}

	// --------------------------JDBC 2.0-----------------------------

	/**
	 *
	 * Creates a <code>Statement</code> object that will generate
	 * <code>ResultSet</code> objects with the given type and concurrency. This
	 * method is the same as the <code>createStatement</code> method above, but
	 * it allows the default result set type and result set concurrency type to
	 * be overridden.
	 *
	 * <P>
	 * <B>FS2 NOTE:</B> At the present time, Only TYPE_FORWARD_ONLY and
	 * CONCUR_READ_ONLY ResultSets are supported. Since this is the default for
	 * a createStatement call, this method is functionally identical to
	 * createStatement() above.
	 *
	 * @param resultSetType
	 *            a result set type, see ResultSet.TYPE_XXX
	 * @param resultSetConcurrency
	 *            a concurrency type, see ResultSet.CONCUR_XXX
	 * @return a new Statement object
	 * @exception SQLException
	 *                if a database-access error occurs.
	 * @since 1.2
	 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0
	 *      API</a>
	 */
	@Override
	public java.sql.Statement createStatement(int resultSetType, int resultSetConcurrency) throws SQLException {
		if (resultSetType != java.sql.ResultSet.TYPE_FORWARD_ONLY
				&& resultSetType != java.sql.ResultSet.TYPE_SCROLL_INSENSITIVE) {
			SQLWarning w = new SQLWarning(
					"ResultSet type not supported. " + "Only TYPE_FORWARD_ONLY/TYPE_SCROLL_INSENSITIVE supported.");
			if (warnings.isEmpty())
				warnings.add(w);
			else
				warnings.firstElement().setNextWarning(w);
		}
		if (resultSetConcurrency != java.sql.ResultSet.CONCUR_READ_ONLY) {
			SQLWarning w = new SQLWarning(
					"ResultSet concurrency type not supported.  Only CONCUR_READ_ONLY supported.");
			if (warnings.isEmpty())
				warnings.add(w);
			else
				warnings.firstElement().setNextWarning(w);
		}
		return (createStatement());
	}

	/**
	 * Creates a <code>PreparedStatement</code> object that will generate
	 * <code>ResultSet</code> objects with the given type and concurrency. This
	 * method is the same as the <code>prepareStatement</code> method above, but
	 * it allows the default result set type and result set concurrency type to
	 * be overridden.
	 *
	 * <P>
	 * <B>FS2 NOTE:</B> At the present time, Only TYPE_FORWARD_ONLY and
	 * CONCUR_READ_ONLY ResultSets are supported. Since this is the default for
	 * a prepareStatement call, this method is functionally identical to
	 * prepareStatement() above.
	 *
	 * @param resultSetType
	 *            a result set type, see ResultSet.TYPE_XXX
	 * @param resultSetConcurrency
	 *            a concurrency type, see ResultSet.CONCUR_XXX
	 * @return a new PreparedStatement object containing the pre-compiled SQL
	 *         statement
	 * @exception SQLException
	 *                if a database-access error occurs.
	 * @since 1.2
	 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0
	 *      API</a>
	 */
	@Override
	public java.sql.PreparedStatement prepareStatement(String sql, int resultSetType, int resultSetConcurrency)
			throws SQLException {
		if (resultSetType != java.sql.ResultSet.TYPE_FORWARD_ONLY
				&& resultSetType != java.sql.ResultSet.TYPE_SCROLL_INSENSITIVE) {
			SQLWarning w = new SQLWarning(
					"ResultSet type not supported. " + "Only TYPE_FORWARD_ONLY/TYPE_SCROLL_INSENSITIVE supported.");
			if (warnings.isEmpty())
				warnings.add(w);
			else
				warnings.firstElement().setNextWarning(w);
		}
		if (resultSetConcurrency != java.sql.ResultSet.CONCUR_READ_ONLY) {
			SQLWarning w = new SQLWarning(
					"ResultSet concurrency type not supported.  Only CONCUR_READ_ONLY supported.");
			if (warnings.isEmpty())
				warnings.add(w);
			else
				warnings.firstElement().setNextWarning(w);
		}
		return (prepareStatement(sql));
	}

	/**
	 *
	 * Creates a <code>CallableStatement</code> object that will generate
	 * <code>ResultSet</code> objects with the given type and concurrency. This
	 * method is the same as the <code>prepareCall</code> method above, but it
	 * allows the default result set type and result set concurrency type to be
	 * overridden.
	 *
	 * <P>
	 * <B>FS2 NOTE:</B> This method should not be used. The FS2 system does not
	 * support stored procedures. This method will always result in an
	 * exception.
	 *
	 * @param resultSetType
	 *            a result set type, see ResultSet.TYPE_XXX
	 * @param resultSetConcurrency
	 *            a concurrency type, see ResultSet.CONCUR_XXX
	 * @return a new CallableStatement object containing the pre-compiled SQL
	 *         statement
	 * @exception SQLException
	 *                always.
	 * @since 1.2
	 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0
	 *      API</a>
	 */
	@Override
	public java.sql.CallableStatement prepareCall(String sql, int resultSetType, int resultSetConcurrency)
			throws SQLException {
		throw new SQLException(Driver.bun.getString("NoCallableStatement"));
	}

	/**
	 * Gets the type map object associated with this connection. Unless the
	 * application has added an entry to the type map, the map returned will be
	 * empty.
	 *
	 * <P>
	 * <B>FS NOTE:</B> This method should not be used. The FS system does not
	 * support TypeMaps. This method will always result in an exception.
	 *
	 *
	 * @return the <code>java.util.Map</code> object associated with this
	 *         <code>Connection</code> object
	 * @since 1.2
	 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0
	 *      API</a>
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public Map<String, Class<?>> getTypeMap() throws SQLException {
		throw new SQLException(Driver.bun.getString("NoTypeMaps"));
	}

	/**
	 * Installs the given type map as the type map for this connection. The type
	 * map will be used for the custom mapping of SQL structured types and
	 * distinct types.
	 *
	 * <P>
	 * <B>FS NOTE:</B> This method should not be used. The FS system does not
	 * support TypeMaps. This method will always result in an exception.
	 *
	 * @param the
	 *            <code>java.util.Map</code> object to install as the
	 *            replacement for this <code>Connection</code> object's default
	 *            type map
	 * @since 1.2
	 * @see <a href="package-summary.html#2.0 API">What Is in the JDBC 2.0
	 *      API</a>
	 * @exception SQLException
	 *                always.
	 */
	@Override
	public void setTypeMap(Map<String, Class<?>> map) throws SQLException {
		throw new SQLException(Driver.bun.getString("NoTypeMaps"));
	}

	class SendKeepAliveThread extends Thread {

		@Override
		public void run() {
			for (;;) {
				try {
					Thread.sleep(driver.gettimeout() * 1000L);
					connectMsg.sendMessage("ALIVEACK");
					if (isInterrupted()) break;
				}
				catch (Exception e) {
					break;
				}
			}
		}

	} // end of class sendKeepAlive

	@Override
	public void setHoldability(int arg0) throws SQLException {
	}

	@Override
	public int getHoldability() throws SQLException {
		return 0;
	}

	@Override
	public Savepoint setSavepoint() throws SQLException {
		return null;
	}

	@Override
	public Savepoint setSavepoint(String arg0) throws SQLException {
		return null;
	}

	@Override
	public void rollback(Savepoint arg0) throws SQLException {
	}

	@Override
	public void releaseSavepoint(Savepoint arg0) throws SQLException {
	}

	@Override
	public java.sql.Statement createStatement(int arg0, int arg1, int arg2) throws SQLException {
		return null;
	}

	@Override
	public java.sql.PreparedStatement prepareStatement(String arg0, int arg1, int arg2, int arg3) throws SQLException {
		return null;
	}

	@Override
	public java.sql.CallableStatement prepareCall(String arg0, int arg1, int arg2, int arg3) throws SQLException {
		return null;
	}

	@Override
	public java.sql.PreparedStatement prepareStatement(String arg0, int arg1) throws SQLException {
		return null;
	}

	@Override
	public java.sql.PreparedStatement prepareStatement(String arg0, int[] arg1) throws SQLException {
		return null;
	}

	@Override
	public java.sql.PreparedStatement prepareStatement(String arg0, String[] arg1) throws SQLException {
		return null;
	}

	@Override
	public Array createArrayOf(String typeName, Object[] elements) throws SQLException {
		return null;
	}

	@Override
	public Blob createBlob() throws SQLException {
		return null;
	}

	@Override
	public Clob createClob() throws SQLException {
		return null;
	}

	@Override
	public Struct createStruct(String typeName, Object[] attributes) throws SQLException {
		return null;
	}

	@Override
	public Properties getClientInfo() throws SQLException {
		return null;
	}

	@Override
	public String getClientInfo(String name) throws SQLException {
		return null;
	}

	@Override
	public boolean isValid(int timeout) throws SQLException {
		return false;
	}

	@Override
	public boolean isWrapperFor(Class<?> iface) throws SQLException {
		return false;
	}

	@Override
	public <T> T unwrap(Class<T> iface) throws SQLException {
		return null;
	}

	@Override
	public NClob createNClob() throws SQLException {
		throw new SQLFeatureNotSupportedException();
	}

	@Override
	public SQLXML createSQLXML() throws SQLException {
		throw new SQLFeatureNotSupportedException();
	}

	@Override
	public void setClientInfo(String name, String value) throws SQLClientInfoException {
		throw new SQLClientInfoException("not supported", null);
	}

	@Override
	public void setClientInfo(Properties properties) throws SQLClientInfoException {
		throw new SQLClientInfoException("not supported", null);
	}

	@Override
	public void setSchema(String schema) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public String getSchema() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void abort(Executor executor) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setNetworkTimeout(Executor executor, int milliseconds) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public int getNetworkTimeout() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

}
