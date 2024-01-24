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
 * */

package org.ptsw.sc;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executors;
import java.util.concurrent.FutureTask;

import org.ptsw.sc.xml.Element;

import javafx.application.Application;
import javafx.application.Platform;
import javafx.scene.control.Alert;
import javafx.scene.control.Alert.AlertType;
import javafx.scene.control.ButtonType;
import javafx.scene.image.Image;
import javafx.scene.input.Clipboard;
import javafx.stage.Stage;

public class SmartClientApplication extends Application {

	private static Application.Parameters appParms;
	static final int MAJORVERSION = 18;
	static final int MINORVERSION = 0;
	static final int BUILDVERSION = 2;
	static final String VERSION = MAJORVERSION + "." + MINORVERSION + '.' + BUILDVERSION;
	static final String NAMEANDVERSION = "DB/C SC Java " + VERSION;
	static final String DEFAULTFONTNAME = "Arial";
	static final double DEFAULTFONTSIZE = 9.3;
	private static boolean encrypted = true; // We default to encrypted communications
	private static boolean debug = false; // output debug info
	private static boolean trace = false; // output stack trace info
	private static boolean traceoutbound = false; // trace xml messages sent
	private static final String HELLOELEMENT = "<hello>" + NAMEANDVERSION + "</hello>";
	private static final String expectedServerGreeting = "DB/C SS ";
	static int ServerMajorVersion;
	private static String host, cmdLineParameters = "";
	private static int hostPort, lport;
	private static String username, dirname;
	private static ServerSocket serverSocket;
	private static Client client;
	public static final Clipboard clipboard = Clipboard.getSystemClipboard();
	
	/**
	 * By default we will report as errors any change or prep command that we do not support.
	 * This can be changed by the config option dbcdx.javasc.unsupported=ignore
	 */
	public static boolean ignoreUnsupportedChangeCommands = false;
	
	static boolean debugSuppressOutboundTraceWSIZ = true;
	static boolean debugSuppressOutboundTraceWPOS = true;
	
	private static final String bigIconPath = "Images/dbcbig.png";
	private static final String littleIconPath = "Images/dbcsmall.png";
	
	private static final File bigIconFile = new File(bigIconPath);
	private static final File littleIconFile = new File(littleIconPath);
	
	/**
	 * 32x32
	 */
	static Image bigIcon;
	/**
	 * 16x16
	 */
	static Image littleIcon;
	
	
	public static void main(String[] args) {
        launch(args);
    }
    
	/* (non-Javadoc)
	 * @see javafx.application.Application#init()
	 * 
	 * Read the command line for arguments that we recognize.
	 * Remember any that we don't for future passing to the runtime.
	 * Attempt First Contact. If all good then proceed, else die.
	 */
	@Override
	public void init() throws Exception {
		int i1;
		super.init();
		InputStream is1 = ClassLoader.getSystemResourceAsStream(bigIconPath);
		InputStream is2 = ClassLoader.getSystemResourceAsStream(littleIconPath);
		if (is1 == null || is2 == null) {
			if (bigIconFile.exists()) {
				bigIcon = new Image(bigIconFile.toURI().toURL().toString(), false);
			}
			if (littleIconFile.exists()) {
				littleIcon = new Image(littleIconFile.toURI().toURL().toString(), false);
			}
		}
		else {
			bigIcon = new Image(is1);
			littleIcon = new Image(is2);
		}
		Platform.setImplicitExit(false);
		appParms = getParameters();
		List<String> rawparms = appParms.getRaw();
		if (rawparms.size() < 1) {
			Usage();
			Platform.exit();
			return;
		}
		host = rawparms.get(0);
		for (i1 = 1, lport = -1, hostPort = 9735; i1 < rawparms.size(); i1++) {
			String s1 = rawparms.get(i1).trim();
			int i2 = s1.length();
			if (i2 > 11 && s1.substring(0, 11).equalsIgnoreCase("-localport=")) {
				lport = Integer.parseInt(s1.substring(11));
			}
			else if (i2 > 10 && s1.substring(0, 10).equalsIgnoreCase("-hostport=")) {
				hostPort = Integer.parseInt(s1.substring(10));
			}
			else if (i2 > 9 && s1.substring(0, 9).equalsIgnoreCase("-encrypt=")) {
				if (s1.substring(9).equalsIgnoreCase("off")) encrypted = false;
			}
			else if (i2 > 6 && s1.substring(0, 6).equalsIgnoreCase("-user=")) {
				username = s1.substring(6);
			}
			else if (i2 > 8 && s1.substring(0, 8).equalsIgnoreCase("-curdir=")) {
				dirname = s1.substring(8);
			}
			else if (i2 == 6 && s1.equalsIgnoreCase("-debug")) debug = true;
			else if (i2 == 6 && s1.equalsIgnoreCase("-trace")) trace = true; 
			else if (i2 == 8 && s1.equalsIgnoreCase("-traceob")) traceoutbound = true; 
			/*
			 * Absorb parameters that are used by other SCs (notably the 'C' SC)
			 */
			else if  (i2 == 7 && s1.equalsIgnoreCase("-xmllog")) /* do nothing */ ;
			else if  (i2 == 2 && s1.equalsIgnoreCase("-t")) /* do nothing */ ;
			else if  (i2 == 2 && s1.equalsIgnoreCase("-a")) /* do nothing */ ;
			else if  (i2 == 3 && s1.equalsIgnoreCase("-pl")) /* do nothing */ ;
			else cmdLineParameters += (s1 + " ");
		}
		if (debug) System.out.println("java.runtime.version=" + System.getProperty("java.runtime.version"));
		if (debug) {
			System.out.println("javafx.runtime.version=" + System.getProperty("javafx.runtime.version"));
			//Properties props = System.getProperties();
			//props.list(System.out);
		}
		
		if (!AttemptFirstContact(host, hostPort)) {
			Platform.exit();
			return;
		}
	}

	static boolean isEncrypted() {
		return encrypted;
	}

	static boolean isDebug() {
		return debug;
	}

	static boolean isTrace() {
		return trace;
	}

	static boolean isTraceoutbound() {
		return traceoutbound;
	}

	static String getHost() {
		return host;
	}

	static String getCmdLineParameters() {
		return cmdLineParameters;
	}

	static int getHostPort() {
		return hostPort;
	}

	static int getLport() {
		return lport;
	}

	static String getUsername() {
		return username;
	}

	static String getDirname() {
		return dirname;
	}

	static ServerSocket getServerSocket() {
		return serverSocket;
	}

	/*
	 * @see javafx.application.Application#start(javafx.stage.Stage)
	 */
	@Override
	public void start(Stage stage) throws Exception {
		stage.close();
		if (lport != 0) {
			try {
				serverSocket = new ServerSocket((lport == -1) ? 0 : lport);
			}
			catch (SecurityException e0) {
				if (trace || debug) e0.printStackTrace();
				System.err.println("ERROR: Permission to create ServerSocket denied (port = " + ((lport == -1) ? 0 : lport) + ").");
				Platform.exit();
				return;
			}
			catch (IOException ioe1) {
				if (trace || debug) ioe1.printStackTrace();
				System.err.println("ERROR: ServerSocket creation failed (port = " + ((lport == -1) ? 0 : lport) + ").");
				Platform.exit();
				return;
			}
		}
		/*
		 * Construct the Client object
		 * Start it running on a new thread
		 */
		client = new Client();
		Executors.defaultThreadFactory().newThread(client).start();
		if (trace) {
			System.out.println("Leaving SmartClientApplication.start");
			System.out.flush();
		}
	}
	
	/**
	 * Output 'Usage' verbiage 
	 */
	private static void Usage() {
		System.out.println("SMART CLIENT release " + VERSION + "  (c) Copyright 2020 Portable Software Company");
		System.out.println("Usage:  Client host [-hostport=port] [-localport=port] [-encrypt=<on|off>]");
		System.out.println("\t\t[-user=username] [-curdir=directory] [-lp] [DX parameters...]");
	}

	/**
	 * Used to get things done on the FX Application Thread that need to be done there.
	 * 
	 * Wrote this to deal with lack of, say, Platform.runAndWait(Runnable runable)
	 * 
	 * If we are already on that thread, just do it and ignore the wait parameter.
	 * If no wait, just queue it to run later.
	 * If wait, embed the caller's Runnable in my own Runnable so that a Latch
	 * can be used to wait for completion.
	 * 
	 * @param runnable
	 * @param wait
	 */
	public static void Invoke(final Runnable runnable, boolean wait)
	{
		if (Platform.isFxApplicationThread()) runnable.run();
		else {
			if (!wait) Platform.runLater(runnable);
			else {
				final CountDownLatch latch = new CountDownLatch(1);
				Platform.runLater(() -> {
					runnable.run();
					latch.countDown();
				});
				do {
					try {
						latch.await();
					} catch (InterruptedException e) { /* do nothing */ }
				} while (latch.getCount() > 0);
			}
		}
	}
	
	/**
	 * Invokes a Runnable in JFX Thread and waits until it's finished. 
	 * Similar to SwingUtilities.invokeAndWait.
	 * This method is not intended to be called from the FAT, but when this happens the
	 * runnable is executed synchronously. 
	 *
	 * @param runnable
	 *            The Runnable that has to be executed on JFX application thread.
	 * @throws RuntimeException which wraps a possible InterruptedException or ExecutionException 
	 */
	static public void runAndWait(final Runnable runnable) {
		// running this from the FAT 
		if (Platform.isFxApplicationThread()) {
			runnable.run();
			return;
		}
		
		// run from a separate thread
		try {
			FutureTask<Void> future = new FutureTask<>(runnable, null);
			Platform.runLater(future);
			future.get();
		}
		catch (InterruptedException | ExecutionException e) {
			throw new RuntimeException(e);
		}
	}
	
	/**
	 * Invokes a Callable in JFX Thread and waits until it's finished. 
	 * Similar to SwingUtilities.invokeAndWait.
	 * This method is not intended to be called from the FAT, but when this happens the
	 * callable is executed synchronously. 
	 *
	 * @param callable
	 *            The Callable that has to be executed on JFX application thread.
	 * @return the result of callable.call();
	 * @throws RuntimeException which wraps a possible Exception, InterruptedException or ExecutionException 
	 */
	static public <V> V runAndWait(final Callable<V> callable) {
		// running this from the FAT 
		if (Platform.isFxApplicationThread()) {
			try {
				return callable.call();
			}
			catch (Exception e) {
				throw new RuntimeException(e);
			}
		}
		
		// run from a separate thread
		try {
			FutureTask<V> future = new FutureTask<>(callable);
			Platform.runLater(future);
			return future.get();
		}
		catch (InterruptedException | ExecutionException e) {
			throw new RuntimeException(e);
		}
	}
	
	/**
	 * Attempt first contact, send a <hello> Element
	 * 
	 * Check the format of the returned XML
	 * Check the major version of the server
	 * 
	 * If this returns true then we can continue with the next phase, the <start> message
	 * 
	 */
	private static boolean AttemptFirstContact(String host, int port)
	{
		Socket sock1;
		Element e1;
		try {
			sock1 = new Socket(host, port);
		}
		catch (Exception e) {
			if (trace || debug) e.printStackTrace();
			System.err.println("ERROR: Socket creation failed (port = " + port + ").");
			return false;
		}
		try {
			e1 = ClientUtils.SendElement(sock1, HELLOELEMENT);
			sock1.close();
		}
		catch (Exception e) {
			System.err.println(e.toString());
			return false;
		}
		if (e1 == null) {
			System.err.println("ERROR: Null return from server");
			return false;
		}
		if (e1.name.equals("err") && e1.getChildrenCount() == 1) {
			Runnable runMe = () -> {
				Alert alert4 = new Alert(AlertType.ERROR, (String)e1.getChild(0), ButtonType.CLOSE);
				alert4.setHeaderText("Server says:");
				alert4.showAndWait();
			};
			Invoke(runMe, true);
			return false;
		}
		if (!e1.name.equals("ok") || e1.getChildrenCount() != 1) {
			System.err.println("ERROR: Server returned incorrect XML(A)");
			return false;
		}
		Object obj1 = e1.getChild(0);
		if (!(obj1 instanceof String) || !((String)obj1).startsWith(expectedServerGreeting)) {
			System.err.println("ERROR: Server returned incorrect XML(B)");
			return false;
		}
		String temp = ((String)obj1).substring(expectedServerGreeting.length());
		int i1 = temp.indexOf('.');
		if (i1 < 1) {
			// If no decimal point found, test for open source version, might be simply 101.
			// i.e. 'DB/C SS 101'
			try { ServerMajorVersion = Integer.parseInt(temp.substring(0)); }
			catch (NumberFormatException nfe) {
				System.err.println("ERROR: Server Major version not found");
				return false;
			}
			if (ServerMajorVersion > MAJORVERSION && ServerMajorVersion < 101) {
				System.err.println("ERROR: Server Major version out of range");
				return false;
			}
		}
		else {
			ServerMajorVersion = Integer.parseInt(temp.substring(0,  i1));
			if (ServerMajorVersion > MAJORVERSION) {
				System.err.println("ERROR: Server version is too high for me");
				return false;
			}
			if (ServerMajorVersion < 14) {
				System.err.println("ERROR: Server version is too old for me");
				return false;
			}
		}
		if (trace) {
			System.out.println("Leaving AttemptFirstContact");
			System.out.flush();
		}
		return true;
	}
}
