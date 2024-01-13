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

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.net.ServerSocket;
import java.net.Socket;
import java.sql.DriverManager;
import java.sql.DriverPropertyInfo;
import java.sql.SQLException;
import java.sql.SQLFeatureNotSupportedException;
import java.util.Calendar;
import java.util.Enumeration;
import java.util.Iterator;
import java.util.MissingResourceException;
import java.util.Properties;
import java.util.ResourceBundle;
import java.util.StringTokenizer;
import java.util.logging.Logger;

/**
 * The interface that every driver class must implement.
 * <P>
 * The Java SQL framework allows for multiple database drivers.
 *
 * <P>
 * Each driver should supply a class that implements the Driver interface.
 *
 * <P>
 * The DriverManager will try to load as many drivers as it can find and then
 * for any given connection request, it will ask each driver in turn to try to
 * connect to the target URL.
 *
 * <P>
 * It is strongly recommended that each Driver class should be small and
 * standalone so that the Driver class can be loaded and queried without
 * bringing in vast quantities of supporting code.
 *
 * <P>
 * When a Driver class is loaded, it should create an instance of itself and
 * register it with the DriverManager. This means that a user can load and
 * register a driver by calling
 * 
 * <pre>
 *   <code>Class.forName("foo.bah.Driver")</code>
 * </pre>
 *
 * @see DriverManager
 * @see Connection
 */

public final class Driver implements java.sql.Driver {

	private static final int MAJORVERSION = 6;
	private static final int MINORVERSION = 0;
	private static final String PROTOCOL = "jdbc";
	private static final String[] SUBPROT = { "fs4", "fs5", "fs6" };
	private static final String msgbundlename = "com.dbcswc.fs.jdbc.Messages";
	public static boolean encryption = false;
	private boolean allow_encryption = true;
	private boolean allow_keepalive = true;
	private boolean keepalive = false;
	private int timeout = 10;
	private Socket fsock;
	private static int NPORT = 9584;
	private static int EPORT = 9585;
	private static int LPORT = -1; /* -1 = dynamic */
	/* 0 = use port returned from FS (sport) */
	/* >0 = user specified value is port */

	public static int compliance = 2; // JDBC 2.0 Compliant
	public static ResourceBundle bun; // Package visibility

	/*
	 * Some constants to help with the Catalog functions
	 */
	static final int CATALOG_NONE = 0;
	static final int CATALOG_TABLES = 1;
	static final int CATALOG_COLUMNS = 2;
	static final int CATALOG_BESTROWID = 3;
	static final int CATALOG_TYPEINFO = 4;
	static final int CATALOG_INDEXINFO = 5;
	static final int CATALOG_PRIMARY = 6;

	/*
	 * This static block creates an instance of myself and registers with the
	 * JDBC DriverManager.
	 *
	 * This will happen when the user app does the following:
	 *
	 * Class.forName("com.dbcswc.fs.jdbc.Driver")
	 *
	 */
	static {
		try {
			DriverManager.registerDriver(new Driver());
		}
		catch (SQLException e) {
			System.err.println("\n--- SQLException ---\n");
			System.err.println("Message:  " + e.getLocalizedMessage());
			e.printStackTrace(System.err);
			System.exit(1);
		}
	}

	/*
	 * This static block finds and initializes the messages data.
	 *
	 * This will be useful when we want messages in another language. End users
	 * could create their own messages.properties file.
	 */
	static {
		try {
			bun = ResourceBundle.getBundle(msgbundlename);
		}
		catch (MissingResourceException ex) {
			System.err.println("\n--- MissingResourceException ---\n");
			System.err.println(ex.getLocalizedMessage());
			System.err.println("Class Name : " + ex.getClassName());
			System.err.println("Key Name   : " + ex.getKey());
			ex.printStackTrace(System.err);
			System.exit(1);
		}
	}

	/*
	 * This static block sees if encryption is available
	 *
	 */
	static {
		try {
			Class.forName("javax.net.ssl.SSLServerSocket");
//			Provider provider = (Provider) Class.forName("com.sun.net.ssl.internal.ssl.Provider").newInstance();
//			if (java.security.Security.getProvider(provider.getName()) == null) {
//				SecurityManager security = System.getSecurityManager();
//				if (security != null) {
//					// check applet permission - checkSecurityAccess throws
//					// SecurityException
//					security.checkSecurityAccess("insertProvider." + provider.getName());
//				}
//			}
			encryption = true;
		}
		catch (Exception e) {
			if (traceOn()) trace(e.getMessage());
		}
	}

	/*
	 * This static block checks for JDBC availability
	 *
	 */
	static {
		try {
			DriverManager.getLogWriter(); // Try a JDBC 2.0 static call
		}
		catch (java.lang.NoSuchMethodError e) {
			compliance = 1; // JDBC 1.0
		}
	}

	static {
		try {
			String property = System.getProperty("com.dbcswc.fs.jdbc.log");
			if (compliance >= 2 && property != null && property.equalsIgnoreCase("true")) {
				DriverManager.setLogWriter(new PrintWriter(new FileOutputStream("jdbc.log")));
			}
		}
		catch (Exception e) {
		}
	}

	/**
	 * This method allows for testing whether or not debugging messages should
	 * be displayed.
	 * <P>
	 * example: if (Driver.traceOn()) Driver.trace("message");
	 *
	 */
	public static boolean traceOn() {
		if (compliance >= 2) { return (DriverManager.getLogWriter() != null); }
		return false;
	}

	/**
	 * This method outputs debugging messages if they are enabled.
	 * <P>
	 * example: if (Driver.traceOn()) Driver.trace("message");
	 *
	 */
	static void trace(final String s) {
		Calendar now = Calendar.getInstance();
		String s1 = String.format("%tH:%tM:%tS:%tL", now, now, now, now);
		DriverManager.println(s1 + " " + s);
	}

	/**
	 * Returns true if the driver thinks that it can open a connection to the
	 * given URL. Typically drivers will return true if they understand the
	 * subprotocol specified in the URL and false if they don't.
	 *
	 * @param url
	 *            The URL of the database.
	 * @return True if this driver can connect to the given URL.
	 */
	@Override
	public boolean acceptsURL(final String url) {
		return (getSubname(url) != null);
	}

	/**
	 * Try to make a database connection to the given URL. The driver should
	 * return "null" if it realizes it is the wrong kind of driver to connect to
	 * the given URL. This will be common, as when the JDBC driver manager is
	 * asked to connect to a given URL it passes the URL to each loaded driver
	 * in turn.
	 *
	 * <P>
	 * The driver should raise a SQLException if it is the right driver to
	 * connect to the given URL, but has trouble connecting to the database.
	 *
	 * <P>
	 * The java.util.Properties argument can be used to passed arbitrary string
	 * tag/value pairs as connection arguments. Normally at least "user" and
	 * "password" properties should be included in the Properties.
	 *
	 * <P>
	 * The url should look like; jdbc:fs3:<subname>
	 *
	 * <P>
	 * <subname> = hostname/databasename
	 *
	 * <P>
	 * e.g. jdbc:fs4:rhapsody/orders Hostname is mandatory. If a databasename is
	 * not specified, the driver will attempt to connect to the database
	 * DEFAULT.
	 *
	 * The properties that should be supplied are username and password. If
	 * username is not supplied, the driver will attempt to connect with
	 * username DEFAULTUSER. If password is not supplied, the driver will
	 * attempt to connect with the password PASSWORD.
	 *
	 * @param url
	 *            The URL of the database to connect to
	 * @param info
	 *            A list of arbitrary string tag/value pairs as connection
	 *            arguments; normally at least a "user" and "password" property
	 *            should be included
	 * @return a Connection to the URL
	 * @exception SQLException
	 *                if a database-access error occurs.
	 */
	@SuppressWarnings("null")
	@Override
	public java.sql.Connection connect(final String url, final Properties info) throws SQLException {
		int i1, FSMajorVersion;
		byte[] sdata;
		String FSVersion, userName, password, subname, host, dbdname, param, rtcd, connectionId;
		String[] infoValues = new String[2];
		Message connectMsg;
		StringTokenizer tok;
		StringBuffer sbdata;

		if (traceOn()) trace("Driver@connect, url = '" + url + "'");
		subname = getSubname(url); // pull the subname off of the full URL
		if (subname == null) return null;
		// parse host:port
		tok = new StringTokenizer(subname, "/");
		if (tok.hasMoreTokens()) {
			host = tok.nextToken();
			if ((i1 = host.indexOf(':')) > 0) {
				try {
					NPORT = Integer.parseInt(host.substring(i1 + 1));
				}
				catch (NumberFormatException nfe) {
					throw new SQLException(bun.getString("badURL"));
				}
				EPORT = NPORT;
				host = host.substring(0, i1);
			}
		}
		else
			throw new SQLException(bun.getString("badURL"));
		// parse database;key=value;key=value;...
		if (tok.hasMoreTokens()) {
			tok = new StringTokenizer(tok.nextToken(), ";");
			dbdname = tok.nextToken();
			if (tok.hasMoreTokens()) {
				for (;;) {
					param = tok.nextToken();
					if (param.toLowerCase().startsWith("localport")) {
						try {
							LPORT = Integer.parseInt(param.substring(10));
						}
						catch (NumberFormatException nfe) {
							throw new SQLException(bun.getString("badURL"));
						}
					}
					else if (param.toLowerCase().startsWith("encryption")) {
						if (param.substring(11).equalsIgnoreCase("off")) allow_encryption = false;
					}
					else if (param.toLowerCase().startsWith("keepalive")) {
						if (param.substring(10).equalsIgnoreCase("off")) allow_keepalive = false;
					}
					else if (param.toLowerCase().startsWith("authfile")) {
						// do nothing
					}
					if (!tok.hasMoreTokens()) break;
				}
			}
		}
		else
			dbdname = "DEFAULT";

		FSVersion = sendHello(host);
		FSMajorVersion = parseMajorVersion(FSVersion);

		parseConnectProps(info, infoValues);
		if (infoValues[0] == null) infoValues[0] = "DEFAULTUSER";
		if (infoValues[1] == null) infoValues[1] = "PASSWORD";
		userName = infoValues[0];
		password = infoValues[1];

		if (traceOn()) {
			trace("Driver@connect Socket to " + host + ", encryption=" + encryption);
		}
		try {
			if (FSMajorVersion >= 3) {
				ServerSocket s_sock = null;
				Socket tsock;
				String cmd;
				Thread tt = null;
				if (LPORT != 0) {
					s_sock = new ServerSocket((LPORT == -1) ? 0 : LPORT);
					tt = new Thread(new waitForConnect(s_sock));
					tt.start();
				}
				if (allow_encryption && encryption) {
					try {
						tsock = new SecureSocket(host, EPORT);
					}
					catch (Exception e) {
						throw new IOException(e.getMessage());
					}
				}
				else
					tsock = new Socket(host, NPORT);
				tsock.setTcpNoDelay(true);
				connectMsg = new Message(tsock, true);
				connectMsg.setFunc("START");
				cmd = new String(String.valueOf((LPORT == -1) ? s_sock.getLocalPort() : LPORT) + " " + userName);
				connectMsg.setData(cmd);
				connectMsg.sendMessage();
				connectMsg.recvMessage();
				rtcd = connectMsg.getRtcd().toUpperCase();
				if (!rtcd.equals("OK")) throw new IOException(Driver.bun.getString("ConnRefused"));
				tsock.close();
				if (LPORT == 0) {
					// make use of FS sport option
					fsock = new Socket(host, Integer.parseInt(new String(connectMsg.getData())));
				}
				else {
					try {
						// fsock gets initialized in tt thread
						tt.join();
					}
					catch (InterruptedException e) {
					}
				}
				if (allow_encryption && encryption) {
					try {
						fsock = new SecureSocket(fsock, fsock.getLocalAddress().getHostAddress(), fsock.getLocalPort());
					}
					catch (Exception e) {
						throw new IOException(e.getMessage());
					}
				}
				connectMsg = new Message(fsock, true);
			}
			else {
				fsock = new Socket(host, NPORT);
				connectMsg = new Message(fsock, false);
				allow_keepalive = false;
			}
		}
		catch (IOException ex) {
			throw new SQLException(ex.getLocalizedMessage());
		}

		connectMsg.setFunc("CONNECT");
		sbdata = new StringBuffer(userName);
		sbdata.append(" " + password);
		sbdata.append(" " + "DATABASE");
		sbdata.append(" " + dbdname);
		if (allow_keepalive) {
			sbdata.append(" CLIENT"); /* keep alive */
		}
		connectMsg.setData(sbdata.toString());
		sbdata = null;
		try {
			connectMsg.sendMessage();
			connectMsg.recvMessage();
			rtcd = connectMsg.getRtcd().toUpperCase();
			if (traceOn()) trace("Driver@connect rtcd = " + rtcd);
			if (!rtcd.equals("OK")) throw new IOException(bun.getString("ConnRefused"));
			sdata = connectMsg.getData();
			if (connectMsg.getDataSize() == 16) {
				timeout = Integer.parseInt(new String(sdata, 8, 8).trim());
				keepalive = true;
			}
		}
		catch (IOException ex) {
			throw new SQLException(ex.getLocalizedMessage(), connectMsg.getLastErrorCode());
		}
		if (traceOn()) trace("Driver@connect data = " + new String(sdata));
		connectionId = new String(sdata, 0, 8);
		connectMsg.setConnectionId(connectionId);
		java.sql.Connection conn = new Connection(connectMsg, url, this);
		((org.ptsw.fs.jdbc.Connection) conn).setUserName(userName);
		((org.ptsw.fs.jdbc.Connection) conn).setFSVersion(FSVersion);

		return conn;
	}

	private class waitForConnect implements Runnable {

		private ServerSocket s_sock;

		private waitForConnect(ServerSocket s) {
			if (traceOn()) trace("In waitForConnect<>");
			s_sock = s;
		}

		@Override
		public void run() {
			if (traceOn()) trace("In waitForConnect.run");
			try {
				fsock = s_sock.accept();
			}
			catch (Exception e) {
				throw new RuntimeException(e.getMessage());
			}
		}

	} // end of class waitForConnect

	/**
	 * Get the driver's major version number.
	 */
	@Override
	public int getMajorVersion() {
		return MAJORVERSION;
	}

	/**
	 * Get the driver's minor version number.
	 */
	@Override
	public int getMinorVersion() {
		return MINORVERSION;
	}

	/**
	 * <p>
	 * The getPropertyInfo method is intended to allow a generic GUI tool to
	 * discover what properties it should prompt a human for in order to get
	 * enough information to connect to a database. Note that depending on the
	 * values the human has supplied so far, additional values may become
	 * necessary, so it may be necessary to iterate though several calls to
	 * getPropertyInfo.
	 *
	 * @param url
	 *            The URL of the database to connect to. FS ignores this
	 *            parameter since we always require USER and PASSWORD.
	 * @param info
	 *            A proposed list of tag/value pairs that will be sent on
	 *            connect open.
	 * @return An array of DriverPropertyInfo objects describing possible
	 *         properties. This array may be an empty array if no properties are
	 *         required.
	 */
	@Override
	public DriverPropertyInfo[] getPropertyInfo(final String url, final Properties info) {
		DriverPropertyInfo[] arDPI = new DriverPropertyInfo[2];
		arDPI[0] = new DriverPropertyInfo("USER", null);
		arDPI[0].required = true;
		arDPI[1] = new DriverPropertyInfo("PASSWORD", null);
		arDPI[1].required = true;
		if (info != null) {
			String[] infoValues = new String[2];
			parseConnectProps(info, infoValues);
			if (infoValues[0] != null) arDPI[1].value = infoValues[0];
			if (infoValues[1] != null) arDPI[0].value = infoValues[1];
		}
		return arDPI;
	}

	/**
	 * Report whether the Driver is a genuine JDBC COMPLIANT (tm) driver. A
	 * driver may only report "true" here if it passes the JDBC compliance
	 * tests, otherwise it is required to return false.
	 *
	 * JDBC compliance requires full support for the JDBC API and full support
	 * for SQL 92 Entry Level. It is expected that JDBC compliant drivers will
	 * be available for all the major commercial databases.
	 *
	 * This method is not intended to encourage the development of non-JDBC
	 * compliant drivers, but is a recognition of the fact that some vendors are
	 * interested in using the JDBC API and framework for lightweight databases
	 * that do not support full database functionality, or for special databases
	 * such as document information retrieval where a SQL implementation may not
	 * be feasible.
	 *
	 * <P>
	 * <B>FS NOTE: </B> The FS system is not fully JDBC compliant because we do
	 * not support transactions and other features.
	 *
	 */
	@Override
	public boolean jdbcCompliant() {
		return false;
	}

	/*
	 * Given the url return the subname. If the Protocol is not PROTOCOL, or the
	 * subprotocol is not one of SUBPROT[], then return null. If the subname is
	 * missing return null.
	 */
	private static String getSubname(final String url) {
		StringTokenizer tok = new StringTokenizer(url, ":");
		String subname = null;
		if (tok.countTokens() >= 3) {
			if (tok.nextToken().equalsIgnoreCase(PROTOCOL)) {
				String tok2 = tok.nextToken();
				for (int i1 = 0; i1 < SUBPROT.length; i1++) {
					if (tok2.equalsIgnoreCase(SUBPROT[i1])) {
						subname = tok.nextToken();
						if (tok.hasMoreTokens()) subname += ":" + tok.nextToken();
						break;
					}
				}
			}
		}
		if (traceOn()) trace("2) subname = " + subname);
		return subname;
	}

	/*
	 * utility method to investigate the Properties object passed to Connect and
	 * get the user(name) and password. These two are required for FS.
	 */
	private static void parseConnectProps(final Properties info, String[] values) {
		if (compliance <= 1) {
			// JDBC 1.0 use Enumeration
			for (Enumeration<?> en = info.propertyNames(); en.hasMoreElements();) {
				String work = (String) en.nextElement();
				if (work.equalsIgnoreCase("USER") || work.equalsIgnoreCase("NAME")) {
					values[0] = info.getProperty(work);
				}
				else if (work.equalsIgnoreCase("PASSWORD")) {
					values[1] = info.getProperty(work);
				}
			}
		}
		else {
			// JDBC 2.0 use Iterator
			for (Iterator<Object> iter = info.keySet().iterator(); iter.hasNext();) {
				String work = (String) iter.next();
				if (work.equalsIgnoreCase("USER") || work.equalsIgnoreCase("NAME")) {
					values[0] = info.getProperty(work);
				}
				else if (work.equalsIgnoreCase("PASSWORD")) {
					values[1] = info.getProperty(work);
				}
			}
		}
	}

	/**
	 * Send a HELLO message to see if the FS is there, and get the version
	 *
	 * @return the FS database version string
	 */
	private String sendHello(String host) throws SQLException {

		Message connectMsg;
		Socket sock;
		String rtcd;
		try {
			if (allow_encryption && encryption) {
				try {
					sock = new SecureSocket(host, EPORT);
					sock.setTcpNoDelay(true);
				}
				catch (Exception e) {
					throw new IOException(e.getMessage());
				}
			}
			else
				sock = new Socket(host, NPORT);
			connectMsg = new Message(sock, false);
		}
		catch (IOException ex) {
			throw new SQLException(ex.getLocalizedMessage());
		}
		connectMsg.setFunc("HELLO");
		try {
			connectMsg.sendMessage();
			connectMsg.recvMessage();
			rtcd = connectMsg.getRtcd().toUpperCase();
			if (!rtcd.equals("OK")) throw new IOException(bun.getString("ConnRefused"));
			sock.close();
		}
		catch (IOException ex) {
			throw new SQLException(ex.getLocalizedMessage(), connectMsg.getLastErrorCode());
		}
		String v = new String(connectMsg.getData());
		if (traceOn()) trace("Hello response : " + v);
		if (v.startsWith("DB/C FS "))
			v = v.substring(8);
		else
			return null;

		return v;
	}

	static int parseMajorVersion(String v) {
		int i1 = v.lastIndexOf(' ');
		int i2 = v.indexOf('.');
		String v1;
		if (i2 < 0)
			v1 = v.substring(i1 + 1);
		else
			v1 = v.substring(i1 + 1, i2);
		try {
			i1 = Integer.parseInt(v1);
		}
		catch (NumberFormatException e) {
			return -1;
		}
		return i1;
	}

	public boolean getkeepalive() {
		return keepalive;
	}

	public int gettimeout() {
		return timeout;
	}

	@Override
	public Logger getParentLogger() throws SQLFeatureNotSupportedException {
		// TODO Auto-generated method stub
		return null;
	}

}
