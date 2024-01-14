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

package com.dbcswc.fs;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketException;
import java.net.UnknownHostException;
//import java.security.Provider;
//import java.security.Security;
import java.util.logging.Level;

/**
 * Class Connection implements a connection with a DB/C FS server.
 */
public final class Connection implements AutoCloseable {

	private static final int CONNECTPORT = 9584;
	private static boolean encryption = false;

	private boolean keepalive = false;
	private Thread keepAliveThread;
	private int timeout;
	private Socket socket;
	private OutputStream out;
	private InputStream in;
	private boolean connected;
	private byte[] sendmsghdrtemplate;
	private byte[] sendmsghdr;
	private byte[] sendmsgdata;
	private byte[] recvmsghdr;
	private byte[] recvmsgdata;
	private int recvlength;
	private String server;
	private String database;
	private String user;
	private String FSVersion;
	private int FSMajorVersion;
	private Object lock = new Object();

	/*
	 * This static block sees if encryption is available
	 */
	static {
		try {
			Class.forName("javax.net.ssl.SSLServerSocket");
//			Provider provider = (java.security.Provider)
//					Class.forName("com.sun.net.ssl.internal.ssl.Provider").newInstance();
//			if (Security.getProvider(provider.getName()) == null) {
//				SecurityManager security = System.getSecurityManager();
//				if (security != null) {
//					// check applet permission - checkSecurityAccess throws
//					// SecurityException
//					security.checkSecurityAccess("insertProvider."
//							+ provider.getName());
//				}
//			}
			encryption = true;
		} catch (Exception e) {
			Util.logger.log(Level.CONFIG, "Checking for javax.net.ssl.SSLServerSocket", e); }
	}

	/**
	 * Constructor with no port arguments.
	 */
	public Connection(String server, String database, String user,
			String password) throws ConnectionException {
		this(server, -1, -1, database, user, password, encryption, null);
	}

	/**
	 * Constructor with port arguments.
	 */
	public Connection(String server, int sport, int lport, String database,
			String user, String password) throws ConnectionException {
		this(server, sport, lport, database, user, password, encryption, null);
	}

	/**
	 * Constructor with no port arguments but authentication.
	 *
	 * @param server IP address of FS server
	 * @param database FS database name
	 * @param user	FS USer name
	 * @param password	FS Password
	 * @param encrypt Use encryption
	 * @param authfile Should be null
	 * @throws ConnectionException
	 */
	public Connection(String server, String database, String user,
			String password, boolean encrypt, String authfile)
			throws ConnectionException {
		this(server, -1, -1, database, user, password, encrypt, authfile);
	}

	/**
	 * Constructor with port arguments and authentication.
	 *
	 * @param server IP address of FS server
	 * @param sport  server port
	 * @param lport  local port
	 * @param database FS database name
	 * @param user	FS USer name
	 * @param password	FS Password
	 * @param encrypt Use encryption
	 * @param authfile Should be null
	 * @throws ConnectionException
	 */
	public Connection(String server, int sport, int lport, String database,
			String user, String password, boolean encrypt, String authfile)
			throws ConnectionException {
		int i1, i2/* , i3, i4*/;
		ServerSocket s_sock = null; // fs3
		String rtcd;
		StringBuffer sbdata;
		if (!encryption && encrypt) {
			throw new ConnectionException(
					"JSSE is required for encrypted connections");
		}
		if (sport <= 0) {
			if (encrypt)
				sport = CONNECTPORT + 1;
			else
				sport = CONNECTPORT;
		}
		String greeting = getgreeting(server, sport, encrypt, authfile);
		if (greeting.startsWith("DB/C FS ")) {
			FSVersion = greeting.substring(8);
			FSMajorVersion = parseMajorVersion(FSVersion);
		} else {
			throwerror("Server (" + server + ") 'HELLO' response invalid: "
					+ greeting);
		}
		recvmsghdr = new byte[24];
		sendmsghdr = new byte[40];
		sendmsgdata = new byte[256];
		sendmsghdrtemplate = new byte[40];
		sendmsghdrtemplate[0] = (byte) 'Z';
		for (i1 = 1; i1 < 40; sendmsghdrtemplate[i1++] = (byte) ' ')
			;

		try {
			if (FSMajorVersion >= 3) {
				String cmd;
				Thread tt = null;
				Socket tsock = null;
				if (lport != 0) {
					s_sock = new ServerSocket((lport == -1) ? 0 : lport);
					if (lport == -1)
						lport = s_sock.getLocalPort(); // dynamic
					tt = new Thread(new waitStart(s_sock));
					tt.start();
				}
				try {
					if (encrypt) {
						try {
							/*** CODE GOES HERE FOR AUTHFILE ***/
							tsock = new SecureSocket(server, sport);
						} catch (Exception e) {
							throw new IOException(e.getMessage());
						}
					} else {
						tsock = new Socket(server, sport);
					}
					tsock.setTcpNoDelay(true);
					in = new DataInputStream(tsock.getInputStream());
					out = new DataOutputStream(tsock.getOutputStream());
					out.write(0x1);
					out.write(0x2);
					out.write(0x3);
					out.write(0x4);
					out.write(0x5);
					out.write(0x6);
					out.write(0x7);
					out.write(0x8);
					((DataOutputStream) out)
							.writeBytes("                START   ");
					cmd = String.valueOf(lport) + ' ' + user;
					i1 = String.valueOf(cmd.length()).length();
					((DataOutputStream) out).writeBytes("        "
							.substring(i1) + cmd.length());
					((DataOutputStream) out).writeBytes(cmd);
					out.flush();
					sendmsghdrtemplate[0] = 0x1; // prevent error in recv()
					rtcd = recv();
					sendmsghdrtemplate[0] = (byte) 'Z';
				} catch (ConnectionException ce) {
					if (tsock != null && !tsock.isClosed()) tsock.close();
					throw new RuntimeException(ce.getMessage());
				} catch (SocketException se) {
					throw new RuntimeException(se.getMessage());
				} catch (IOException ioe) {
					throw new RuntimeException(ioe.getMessage());
				}
				if (!rtcd.equals("OK")) {
					if (tsock != null && !tsock.isClosed()) tsock.close();
					throw new RuntimeException("Unable to START server:port "
							+ server + "/" + sport + " : "
							+ new String(recvmsgdata, 0, recvlength));
				}
				in.close();
				out.close();
				tsock.close();
				if (lport == 0) {
					// make use of FS sport option
					socket = new Socket(server, Integer.parseInt(new String(
							recvmsgdata, 0, recvlength)));
				} else {
					try {
						tt.join();
					} catch (InterruptedException ie) {
					}
				}
				if (encrypt) {
					try {
						// layer secure socket over existing plain socket
						socket = new SecureSocket(socket, socket
								.getLocalAddress().getHostAddress(),
								socket.getLocalPort());
					} catch (Exception e) {
						throw new IOException(e.getMessage());
					}
				}
				in = new FSInputStream(socket.getInputStream());
				out = new FSOutputStream(socket.getOutputStream());
			} else {
				socket = new Socket(server, sport);
				in = new BufferedInputStream(socket.getInputStream());
				out = new BufferedOutputStream(socket.getOutputStream());
			}
			socket.setTcpNoDelay(true);
			socket.setKeepAlive(true);
		} catch (UnknownHostException e) {
			throwerror("Server unavailable: " + server, e);
		} catch (IOException e) {
			throwerror("IO exception encountered while connecting to server: "
					+ server, e);
		}

		sbdata = new StringBuffer(user);
		sbdata.append(" " + password);
		sbdata.append(" " + "FILE");
		sbdata.append(" " + database);
		sbdata.append(" CLIENT"); /* keepalive */
		send(null, "CONNECT", sbdata.toString());
		rtcd = recv();
		if (!rtcd.equals("OK")) {
			throwerror("Unable to connect to server/database: "
					+ server
					+ "/"
					+ database
					+ ":"
					+ ((recvmsgdata == null) ? "null" : new String(recvmsgdata,
							0, recvlength)));
		}
		i2 = recvlength;
		for (i1 = 0; i1 < 8; i1++)
		sendmsghdrtemplate[8 + i1] = recvmsgdata[i1];
		if (i2 == 16) {
			keepalive = true;
			timeout = Integer
					.parseInt(new String(recvmsgdata, 8, 8).trim());
			keepAliveThread = new sendKeepAlive(timeout);
			keepAliveThread.start();
		}
		connected = true;
		this.server = server;
		this.database = database;
		this.user = user;
		Util.logger.info("Connection established, srvr=" + this.server + ", user=" + this.user
				+ ", db=" + this.database);
	}

	/**
	 * Get greeting message with port argument.
	 * 
	 * @param server IP address of FS server
	 * @param port Can be -1 for default
	 * @param encrypt Use SSL
	 * @param authfile Not Used, can be null
	 */
	public static String getgreeting(String server, int port, boolean encrypt,
			String authfile /* not used */) throws ConnectionException {
		//System.out.printf("Connection.getGreeting srvr=%s. port=%d, encrypt=%b\n",
		//		server, port, encrypt);
		Socket socket = null;
		DataOutputStream out = null;
		DataInputStream in = null;
		byte[] msgdata;
		if (!encryption && encrypt) {
			throw new ConnectionException(
					"JSSE is required for encrypted connections");
		}
		if (port <= 0) {
			if (encrypt)
				port = CONNECTPORT + 1;
			else
				port = CONNECTPORT;
		}
		try {
			if (encrypt) {
				try {
					/*** CODE GOES HERE FOR AUTHFILE ***/
					socket = new SecureSocket(server, port);
				} catch (Exception e) {
					throw new IOException(e.getMessage());
				}
			} else
				socket = new Socket(server, port);
			socket.setTcpNoDelay(true);
			in = new DataInputStream(socket.getInputStream());
			out = new DataOutputStream(socket.getOutputStream());
		} catch (UnknownHostException e1) {
			throw new ConnectionException("Server unavailable: " + server);
		} catch (IOException e2) {
			ConnectionException ce = new ConnectionException(
					"IO exception encountered while connecting to server: "
							+ server, e2);
			throw ce;
		}
		byte[] msghdr = new byte[24];
		try {
			out.write(0x1);
			out.write(0x2);
			out.write(0x3);
			out.write(0x4);
			out.write(0x5);
			out.write(0x6);
			out.write(0x7);
			out.write(0x8);
			out.writeBytes("                HELLO          0");
			out.flush();
			in.readFully(msghdr);
			int len = 0;
			char c1;
			for (int i1 = 16; i1 < 24; i1++) {
				c1 = (char) msghdr[i1];
				if (c1 >= '0' && c1 <= '9')
					len = len * 10 + c1 - '0';
			}
			msgdata = new byte[len];
			in.readFully(msgdata);
			in.close();
			out.close();
			socket.close();
		} catch (IOException e4) {
			throw new ConnectionException(
					"IO exception encountered in greeting message");
		}
		return new String(msgdata);
	}

	/**
	 * Get greeting message with port argument.
	 */
	public static String getgreeting(String server, int port)
			throws ConnectionException {
		return getgreeting(server, port, encryption, null);
	}

	/**
	 * Get greeting message without port argument.
	 */
	public static String getgreeting(String server) throws ConnectionException {
		return getgreeting(server, 0, encryption, null);
	}

	private class waitStart implements Runnable {
		private ServerSocket s1;

		private waitStart(ServerSocket s1) {
			this.s1 = s1;
		}

		public void run() {
			try {
				socket = s1.accept();
			} catch (Exception e) {
				throw new RuntimeException(e.getMessage());
			}
		}
	}

	/*
	 * @see java.lang.Object#finalize()
	 */
//	@Override
//	protected void finalize() throws Throwable {
//		if (connected && out != null)
//			disconnect();
//	}

	/**
	 * Disconnect.
	 */
	public void disconnect() {
		Util.logger.info("Connection disconnected, srvr=" + this.server + ", user=" + this.user
				+ ", db=" + this.database);
		synchronized (lock) {
			try {
				send(null, "DISCNCT");
				recv();
				if (keepalive)
					keepAliveThread.interrupt();
				in.close();
				out.close();
				socket.close();
			} catch (Exception e) {
				Util.logger.log(Level.WARNING, "", e); }
		}
		in = null;
		out = null;
		socket = null;
		connected = false;
	}

	/**
	 * Send a message.
	 */
	void send(String fsid, String func) throws ConnectionException {
		send(fsid, func, (String) null);
	}

	/**
	 * Send a message.
	 */
	void send(String fsid, String func, char[] data, int datalen)
			throws ConnectionException {
		send(fsid, func, new String(data, 0, datalen));
	}

	/**
	 * Send a message.
	 */
	void send(String fsid, String func, char[] data1, int data1len,
			char[] data2, int data2len) throws ConnectionException {
		send(fsid, func, new String(data1, 0, data1len)
				+ new String(data2, 0, data2len));
	}

	/**
	 * Send a message.
	 */
	void send(String fsid, String func, String data) throws ConnectionException {
		int i1, i2, i3;
		if (out == null) {
			throwerror(String.format("The connection OutputStream is null in send(fsid=%s, func=%s, String data)",
					fsid, func));
		}
		synchronized (lock) {
			if (sendmsghdrtemplate[0] == 'Z')
				sendmsghdrtemplate[0] = (byte) 'A';
			else
				sendmsghdrtemplate[0]++;
			for (i1 = 0; i1 < 40; i1++)
				sendmsghdr[i1] = sendmsghdrtemplate[i1];
			if (fsid != null && fsid.length() == 8) {
				for (i1 = 0; i1 < 8; i1++)
					sendmsghdr[16 + i1] = (byte) fsid.charAt(i1);
			}
			for (i1 = 0, i2 = func.length(); i1 < i2; i1++) {
				sendmsghdr[24 + i1] = (byte) func.charAt(i1);
			}
			if (data == null)
				i1 = i2 = 0;
			else
				i1 = i2 = data.length();
			for (i3 = 39; i3 >= 32; i3--) {
				sendmsghdr[i3] = (byte) (i1 % 10 + '0');
				i1 /= 10;
			}
			try {
				out.write(sendmsghdr, 0, 40);
				if (i2 > 0) {
					if (i2 > sendmsgdata.length)
						sendmsgdata = new byte[i2];
					for (i1 = 0; i1 < i2; i1++)
						sendmsgdata[i1] = (byte) data.charAt(i1);
					out.write(sendmsgdata, 0, i2);
				}
				out.flush();
			}
			catch (IOException e) {
				throwerror("IO exception encountered in send(String fsid, String func, String data)", e);
			}
			catch (Exception excp) {
				throwerror("General Exception in send(String fsid, String func, String data)", excp);
			}
		}
	}

	private void sendack() throws ConnectionException {
		synchronized (lock) {
			try {
				out.write("ALIVEACK".getBytes(), 0, 8);
				out.flush();
			} catch (IOException e) {
				throwerror("IO exception encountered in send", e);
			}
		}
	}

	/**
	 * Receive a message. Return the return code.
	 */
	String recv(char[] data) throws ConnectionException {
		String rtcd = recv();
		if (data == null && recvlength > 0)
			data = new char[recvlength];
		for (int i1 = 0; i1 < recvlength; i1++)
			data[i1] = (char) recvmsgdata[i1];
		return rtcd;
	}

	/**
	 * Receive a message. Return the return code.
	 */
	String recv() throws ConnectionException {
		int i1, i2, len;
		char c1;
		try {
			for (i1 = i2 = 0;;) {
				i2 = in.read(recvmsghdr, i1, recvmsghdr.length - i1);
				if (i2 == -1)
					throwerror("EOF encountered in recv");
				i1 += i2;
				if (i1 == recvmsghdr.length)
					break;
			}
			if (recvmsghdr[0] != sendmsghdrtemplate[0])
				throwerror("Send/recv protocol error");
			for (i1 = 16, len = 0; i1 < 24; i1++) {
				c1 = (char) recvmsghdr[i1];
				if (c1 >= '0' && c1 <= '9')
					len = len * 10 + c1 - '0';
			}
			if (len > 0) {
				if (recvmsgdata == null || recvmsgdata.length < len)
					recvmsgdata = new byte[len];

				for (i1 = i2 = 0;;) {
					i2 = in.read(recvmsgdata, i1, len - i1);
					if (i2 == -1)
						throwerror("EOF encountered in recv");
					i1 += i2;
					if (i1 == len)
						break;
				}

			}
			recvlength = len;
		} catch (Exception e) {
			Util.logger.log(Level.SEVERE, "", e);
			throwerror("IO exception encountered in recv", e);
		}
		return (new String(recvmsghdr, 8, 8)).trim();
	}

	/**
	 * Return the last message data length.
	 */
	int recvlength() {
		return recvlength;
	}

	/**
	 * Return the server associated with this connection.
	 */
	public String getserver() {
		return server;
	}

	/**
	 * Return the database associated with this connection.
	 */
	public String getdatabase() {
		return database;
	}

	/**
	 * Return the user associated with this connection.
	 */
	public String getuser() {
		return user;
	}

	/**
	 * Throw an ConnectionException.
	 */
	private void throwerror(String message, Exception e) throws ConnectionException {
		Util.logger.log(Level.SEVERE, message, e);
		throwerror(message);
	}
	
	/**
	 * Throw an ConnectionException.
	 */
	private void throwerror(String message) throws ConnectionException {
		try {
			in.close();
		} catch (Exception e) {
			Util.logger.log(Level.WARNING, message, e);
		}
		try {
			out.close();
		} catch (Exception e) {
			Util.logger.log(Level.WARNING, message, e);
		}
		try {
			socket.close();
		} catch (Exception e) {
			Util.logger.log(Level.WARNING, message, e);
		}
		in = null;
		out = null;
		socket = null;
		connected = false;
		throw new ConnectionException(message);
	}

	private static int parseMajorVersion(final String v) {
		int i1 = v.lastIndexOf(' ');
		int i2 = v.indexOf('.');
		String v1;
		if (i2 < 0)
			v1 = v.substring(i1 + 1);
		else
			v1 = v.substring(i1 + 1, i2);
		try {
			i1 = Integer.parseInt(v1);
		} catch (NumberFormatException e) {
			return -1;
		}
		return i1;
	}

	public Object getLock() {
		return lock;
	}

	private class sendKeepAlive extends Thread {

		private final long timeout;

		private sendKeepAlive(int timeout) {
			this.timeout = timeout * 1000L;
		}

		public void run() {
			for (;;) {
				try {
					Thread.sleep(timeout);
					sendack();
				} catch (Exception e) {
					break;
				}
			}
		}

	} // end of class sendKeepAlive

	private class FSOutputStream extends BufferedOutputStream {

		public FSOutputStream(OutputStream out) {
			super(out);
		}

		/*
		 * (non-Javadoc) We must override write() methods to perform Java UTF8
		 * encoding
		 * 
		 * @see java.io.BufferedOutputStream#write(byte[], int, int)
		 */
		@Override
		public void write(byte[] b, int off, int len) throws IOException {
			for (int i1 = off; i1 < (off + len); i1++)
				write(b[i1]);
		}

		/*
		 * (non-Javadoc) We must override write() methods to perform Java UTF8
		 * encoding
		 * 
		 * @see java.io.BufferedOutputStream#write(int)
		 */
		@Override
		public void write(int data) throws IOException {
			if (data < 0)
				data += 256;
			if (data <= 0x7F && data != 0x00) {
				super.write(data);
			} else if (data <= 0x07FF) {
				super.write((byte) (0xC0 | (data >> 6)));
				super.write((byte) (0x80 | (data & 0x3F)));
			} else {
				super.write((byte) (0xE0 | (data >> 12)));
				super.write((byte) (0x80 | (data >> 6) & 0x3F));
				super.write((byte) (0x80 | (data & 0x3F)));
			}
		}

	} // end of class FSOutputStream

	private class FSInputStream extends BufferedInputStream {

		public FSInputStream(InputStream in) {
			super(in);
		}

		/*
		 * (non-Javadoc) We must override read() methods to perform Java UTF8
		 * encoding
		 * 
		 * @see java.io.BufferedInputStream#read(byte[], int, int)
		 */
		@Override
		public int read(byte[] b, int off, int len) throws IOException {
			int i1, i2 = 0, i3;
			for (i1 = off; i1 < (off + len); i1++) {
				i3 = read();
				if (i3 == -1)
					return -1;
				b[i1] = (byte) i3;
				i2++;
			}
			return i2;
		}

		/*
		 * (non-Javadoc) We must override read() methods to perform Java UTF8
		 * encoding
		 * 
		 * @see java.io.BufferedInputStream#read()
		 */
		@Override
		public int read() throws IOException {
			int i1, i2, c1;
			c1 = super.read();
			if (c1 == -1)
				return -1;
			if (c1 >= 0x80) {
				if (c1 < 0xE0) {
					c1 = (c1 - 0xC0) << 6;
					i1 = super.read();
					if (i1 == -1)
						return -1;
					c1 += i1 - 0x80;
				} else {
					c1 = ((c1 - 0xE0) << 12);
					i1 = super.read();
					if (i1 == -1)
						return -1;
					i2 = super.read();
					if (i2 == -1)
						return -1;
					c1 += (i1 - 0x80) << 6;
					c1 += i2 - 0x80;
				}
			}
			return c1;
		}

	} // end of class FSInputStream

	boolean isConnected() {
		return connected;
	}

	@Override
	public void close() throws Exception {
		if (connected && out != null)
			disconnect();
	}
}
