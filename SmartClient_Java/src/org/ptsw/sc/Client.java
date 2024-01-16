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

import java.awt.GraphicsConfiguration;
import java.awt.GraphicsDevice;
import java.awt.GraphicsEnvironment;
import java.awt.Point;
import java.awt.datatransfer.DataFlavor;
import java.awt.datatransfer.UnsupportedFlavorException;
import java.awt.geom.AffineTransform;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.net.Socket;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingQueue;

import org.ptsw.sc.Client.SomethingToDo.Type;
import org.ptsw.sc.Menus.SCContextMenu;
import org.ptsw.sc.Menus.SCMenuBar;
import org.ptsw.sc.SpecialDialogs.SCAlertDialog;
import org.ptsw.sc.SpecialDialogs.SCColorDialog;
import org.ptsw.sc.SpecialDialogs.SCDirChooser;
import org.ptsw.sc.SpecialDialogs.SCFileChooser;
import org.ptsw.sc.SpecialDialogs.SCFontDialog;
import org.ptsw.sc.xml.Attribute;
import org.ptsw.sc.xml.Element;
import org.ptsw.sc.xml.XMLOutputStream;

import javafx.application.Platform;
import javafx.event.Event;
import javafx.event.EventType;
import javafx.geometry.Point2D;
import javafx.scene.Node;
import javafx.scene.image.Image;
import javafx.scene.input.ClipboardContent;
import javafx.scene.input.MouseButton;
import javafx.scene.input.MouseEvent;

public class Client implements Runnable {

	enum RotationAngleInterpretation {
		NULL, OLD, NEW
	}
	
	public enum ControlType {
		NORMAL,
		READONLY,
		SHOWONLY
	}
	
	private static final int DSPATTR_UNDERLINE = 0x00000001;
	private static final int DSPATTR_BLINK     = 0x00000002;
	private static final int DSPATTR_BOLD      = 0x00000004;
	private static final int DSPATTR_REV       = 0x00000010;
	private static final int DSPATTR_GRAPHIC   = 0x00000020;

	public static final String ALERTDLG = "alertdlg";
	public static final String CLIPBOARD = "clipboard";
	public static final String COLORDLG = "colordlg";
	public static final String DIALOG = "dialog";
	public static final String FLOATWINDOW = "floatwindow";
	public static final String FONTDLG = "fontdlg";
	public static final String IMAGEROTATE = "imagerotate";
	public static final String ICON = "icon";
	public static final String IMAGE = "image";
	public static final String MENU = "menu";
	public static final String OPENFDLG = "openfdlg";
	public static final String OPENDDLG = "openddlg";
	public static final String POPUPMENU = "popupmenu";
	public static final String PANEL = "panel";
	public static final String SAVEFDLG = "saveasfdlg";
	public static final String STATUS = "status";
	public static final String TEXTROTATE = "textrotate";
	public static final String TOOLBAR = "toolbar";
	public static final String WINDOW = "window";
	
	private static final java.awt.Toolkit awtTool = java.awt.Toolkit.getDefaultToolkit();
	private static final java.awt.datatransfer.Clipboard awtClip = awtTool.getSystemClipboard();

	private static OutputStream sout;
	private static XMLOutputStream xmlout;
	private static Socket s1;
	private static int sport = 0;
	private static BlockingQueue<SomethingToDo> inOutQueue = new LinkedBlockingQueue<>();
	
	// traps
	List<Integer> straps; // trap characters to set on first openkds()
	List<trap> traps; // trap instances

	static class trap {
		boolean cval; // true if character
		// int options; // trap options 0x01 NORESET 0x02 PRIOR 0x04 DISABLE
		// (or'ed)
		int value; // timeout, timestamp, char value
	}


	private static ReceiveThread RecvThread;
	private static Thread keepAliveThread;
	private static Boolean abortflag = Boolean.FALSE;
	private static String syncstr = "        ";
	private static final Element eresult = new Element("r");
	private static Map<String, Object> properties;
	private static final Map<String, SCWindow> hwindows;
	private static final Map<String, FloatWindow> hfloatwins;
	private static final Map<String, Panel> hpanels;
	private static final Map<String, Dialog> hdialogs;
	private static final Map<String, IconResource> hicons;
	private static final Map<String, ImageResource> himages;
	private static final Map<String, SCMenuBar> hmenus;
	private static final Map<String, SCContextMenu> hpopmenus;
	private static final Map<String, SCToolBar> htoolbars;
	private static final Map<String, SCFileChooser> hofiledlgs;
	private static final Map<String, SCFileChooser> hsfiledlgs;
	private static final Map<String, SCDirChooser> hdirdlgs;
	private static final Map<String, SCAlertDialog> halertdlgs;
	private static final Map<String, SCColorDialog> hcolordlgs;
	private static final Map<String, SCFontDialog> hfontdlgs;
	private static Client thisClient;
	static char minsertSeparator = ',';
	static boolean oldEnterKey;
	//public static final Toolkit toolkit = Toolkit.getToolkit();
	//public static final FontLoader fontloader = toolkit.getFontLoader();

	static {
		hwindows = new HashMap<>();
		hfloatwins = new HashMap<>();
		hpanels = new HashMap<>();
		hdialogs = new HashMap<>();
		hicons = new HashMap<>();
		himages = new HashMap<>();
		hmenus = new HashMap<>();
		hpopmenus = new HashMap<>();
		htoolbars = new HashMap<>();
		hofiledlgs = new HashMap<>();
		hsfiledlgs = new HashMap<>();
		hdirdlgs = new HashMap<>();
		halertdlgs = new HashMap<>();
		hcolordlgs = new HashMap<>();
		hfontdlgs = new HashMap<>();
	}
	
	// keyin and display
	private KeyinDisplay w1 = null;
	private char keyincancelkey = 0;
	private int kdswinflag = 0;
	private boolean keyinendxkeys = false;
	private boolean keyinfkeyshift = false;
	private String tdbfilename = null;
	private String displayfont = null;
	static private int displayfontsize = 0;
	static int displayhsize = 80;
	static int displayvsize = 25;
	private int displayfontcharwidth = 0;
	private int displayfontcharheight = 0;
	static boolean displayautoroll = true;
	private String keyindevice = null;
	private char keyininterruptkey = 26;
	char[] keyinchars;
	boolean flowdown = false;
	int dbloffset; // graphics character offset in gchar array
	boolean dcflag = false;
	boolean latintransflag = false;
	private boolean ansi256ColorMode;

	/**
	 * Start off by assuming NULL, or unknown.
	 * If a preference is specified in the config then we honor it.
	 * If a preference is NOT specified
	 * 		If the server is version 16 or less then we change this to OLD
	 * 		else set it to NEW
	 */
	static RotationAngleInterpretation guiRotationPreference = RotationAngleInterpretation.NULL;
	static RotationAngleInterpretation printTextRotationPreference = RotationAngleInterpretation.NULL;
	
	static private String windowNameLastPOSN = "";
	static boolean gui24BitColorIsRGB;
	
	public static class SomethingToDo {
		enum Type {
			incoming,
			outgoing,
			keyinDisplay,
			terminate
		}
		Type type;
		Element element;
		
		SomethingToDo(Type type, Element element) {
			this.type = type;
			this.element = element;
		}
		
		SomethingToDo(Type type) {
			this(type, null);
		}
	}

	static void setAbort()
	{
		abortflag = Boolean.TRUE;
	}
	
	static Boolean getAbortflag() {
		return abortflag;
	}

	static Client getDefault() {
		return thisClient;
	}

	String getSyncstr() {
		return syncstr;
	}

	void setSyncstr(String syncstr) {
		Client.syncstr = syncstr;
	}

	public static boolean isDebug()
	{
		return SmartClientApplication.isDebug();
	}
	
	public static boolean IsTrace()
	{
		return SmartClientApplication.isTrace();
	}
	
	static boolean isGuiImageRotationAngleInterpretationOld() {
		return Client.guiRotationPreference == RotationAngleInterpretation.OLD;
	}
	
	public static boolean isPrintTextRotationAngleInterpretationOld() {
		return Client.printTextRotationPreference == RotationAngleInterpretation.OLD;
	}
	
	/**
	 * Constructor of the singleton Client object.
	 * 
	 * 1. Create a TCP Socket to the server
	 * 2. Construct and send the <start> Element
	 * 3. Construct the TCP objects for comms with the runtime
	 * 4. Construct the ReceiveThread object
	 * 
	 * @throws Exception
	 */
	Client() throws Exception {
		thisClient = this;
		traps = new ArrayList<>();
		straps = new ArrayList<>();
		StringBuilder xmldata = new StringBuilder();
		String user = SmartClientApplication.getUsername();
		String directory = SmartClientApplication.getDirname();
		boolean encryption = SmartClientApplication.isEncrypted();
		int lport = SmartClientApplication.getLport();
		String parameters = SmartClientApplication.getCmdLineParameters();
		int port = SmartClientApplication.getHostPort();
		try {
			s1 = new Socket(SmartClientApplication.getHost(), port);
		}
		catch (Exception e) {
			if (Client.IsTrace() || Client.isDebug()) e.printStackTrace();
			throw new Exception("ERROR: Socket creation failed (port = " + port + ").");
		}		
		xmldata.append("<start");
		if (user != null) xmldata.append(" user=").append(user); 
		if (directory != null) xmldata.append(" dir=\"").append(directory).append('"');
		if (encryption) xmldata.append(" encryption=y");
		if (lport != 0) xmldata.append(" port=").append(SmartClientApplication.getServerSocket().getLocalPort());
		xmldata.append(" gui=y");
		xmldata.append('>').append(parameters).append("</start>");
		Element e1 = ClientUtils.SendElement(s1, xmldata.toString());
		try {
			s1.close();
		}
		catch (Exception e) { throw e; }
		if (e1 == null) {
			throw new Exception("ERROR: Sending <start> Element returned NULL");
		}
		else if (e1.name.equals("ok")) {
			if (lport == 0 && e1.getChildrenCount() != 0) {
				sport = Integer.parseInt((String) e1.getChild(0));
			}
		}
		else {
			throw new Exception("ERROR: " + ((e1.getChildrenCount() == 0) ? "Server connection error" : (String) e1.getChild(0)));
		}

		try {
			if (sport != 0) {
				s1 = new Socket(SmartClientApplication.getHost(), sport);
			}
			else {
				s1 = SmartClientApplication.getServerSocket().accept();
				SmartClientApplication.getServerSocket().close();
			}
			if (encryption) {
				try {
					// create encryption layer over plain socket
					s1 = new SecureSocket(s1, s1.getLocalAddress().getHostAddress(), s1.getLocalPort());
				}
				catch (Exception e) {
					throw new IOException(e.getMessage());
				}
			}
			s1.setTcpNoDelay(true);
			sout = new BufferedOutputStream(s1.getOutputStream());
			xmlout = new XMLOutputStream(sout) {
				@Override
				public void writeElement(Element element) throws java.io.IOException {
					if (element != null) {
						String s2 = element.toStringRaw();
						write(s2, 0, s2.length());
						flush();
					}
				}
			};
			RecvThread = new ReceiveThread(s1, SmartClientApplication.getServerSocket(), inOutQueue);
		}
		catch (IOException ioe1) {
			if (Client.IsTrace() || Client.isDebug()) ioe1.printStackTrace();
			System.err.println("ERROR: Connection to host could not be established.");
			return;
		}
	}
	
	static void SendElement(Element e1) throws IOException {
		ClientUtils.SendElement(sout, xmlout, e1);
	}
	
	/**
	 * sendResult
	 *
	 * @param error True if an error should be reported
	 *
	 */
	public static void SendResult(boolean error) throws IOException
	{
		SendResult(error, "0");
	}

	/**
	 * sendResult
	 *
	 * @param error True if an error should be reported
	 *
	 */
	static void SendResult(boolean error, String message) throws IOException
	{
		if (error) {
			Element r = new Element("r");
			r.addAttribute(new Attribute("e", message == null ? "No Message" : message));
			ClientUtils.SendElement(sout, xmlout, r);
		}
		else ClientUtils.SendElement(sout, xmlout, eresult);
	}

	static void SendResult(boolean error, Throwable throwable) throws IOException
	{
		if (error) {
			if (Client.isDebug()) throwable.printStackTrace(System.out);
			Element r = new Element("r");
			String message = throwable.getMessage();
			r.addAttribute(new Attribute("e", message == null ? throwable.getClass().getName() : message));
			ClientUtils.SendElement(sout, xmlout, r);
		}
		else ClientUtils.SendElement(sout, xmlout, eresult);
	}

	/**
	 * This is the "Main DX" loop
	 * It processes messages moving to and from the server
	 * 
	 * First step is to start the Receive Thread on a new thread
	 */
	@Override
	public void run() {
		int i1;
		IncomingXMLParser parser = new IncomingXMLParser(inOutQueue, sout, xmlout);
		Thread.currentThread().setName("Main DX");
		if (Client.isDebug()) System.out.println("Client Main DX thread Running!");
		Executors.defaultThreadFactory().newThread(RecvThread).start();
		
		// send version to server
		Element e1 = new Element("smartclient");
		e1.addAttribute(new Attribute("version", SmartClientApplication.VERSION));
		e1.addAttribute(ClientUtils.getUtcOffsetAttribute());
		try {
			ClientUtils.SendElement(sout, xmlout, e1);
		}
		catch (IOException ioe) {
			if (Client.isDebug()) System.err.println("ERROR: communicating to server");
			if (Client.IsTrace()) ioe.printStackTrace();
			terminate();
			return;
		}
		keepAliveThread = new KeepAlive();
		keepAliveThread.setName("Keep Alive");
		keepAliveThread.start();

		foreverloop:
			for (;;) {
				SomethingToDo todo;
				try {
					todo = inOutQueue.take();
				}
				catch (InterruptedException e) {
					continue;
				}
				if (todo != null) {
					switch (todo.type) {
					case terminate:
						break foreverloop;
					case incoming:
						try {
							parser.parse(todo.element);
						}
						catch (Exception ex) {
							if (Client.IsTrace() || Client.isDebug()) {
								ex.printStackTrace();
								System.err.println("ERROR: During the parsing of XML Element: " + todo.element.toString());
								System.err.println(ex);
							}
							break foreverloop;
						}
						break;
					case outgoing:
						try {
							ClientUtils.SendElement(sout, xmlout, todo.element);
						}
						catch (IOException ioe) {
							if (Client.IsTrace() || Client.isDebug()) ioe.printStackTrace();
							break foreverloop;
						}
						break;
					case keyinDisplay:
						i1 = w1.getaction();
						if (i1 == -1) {
							// break
							try {
								SendElement(new Element("break")); 
							}
							catch (IOException ioe) {
								break foreverloop;
							}
						}
						else if (i1 == -2) {
							// interrupt, do nothing
						}
						else if (i1 != 0) { 
							// trap
							Element t = new Element("t");
							t.addString(translatekey(i1));
							try {
								SendElement(t);
							}
							catch (IOException ioe) {
								break foreverloop;
							}
						}
						break;
					default:
						break;
					}
				}
			}
		if (Client.isDebug()) System.out.println("Client Main DX thread Stopping!");
		terminate();
	}

	void terminate() {
		if (Client.isDebug()) System.out.println("Entering Client.terminate");
		keepAliveThread.interrupt();
		try {
			if (w1 != null) {
				w1.actionpending = true;
				w1.recvchar((char) 0x03);
				w1.erase();
				try {
					w1.close();
				}
				catch (Exception e) { }
				w1 = null;
			}
		} 
		catch (Exception e1) { 
			e1.printStackTrace();
		}
		try {
			// wait 2 seconds to allow thread to notify server 
			// that keyin is being aborted
			Thread.sleep(2000); 
		}
		catch (Exception e1) { 
		}
		Platform.exit();
		Runtime.getRuntime().exit(0);
	}
	
	/**
	 * @param prefix
	 * @param element
	 */
	static void addProperty(String prefix, Element element) {
		if (Objects.isNull(properties)) properties = new HashMap<>();
		for (int i1 = 0; i1 < element.getChildrenCount(); i1++) {
			Object o1 = element.getChild(i1);
			prefix = prefix + "." + element.name;
			if (o1 instanceof String) properties.put(prefix, o1);
			else if (o1 instanceof Element) addProperty(prefix, (Element) o1);
		}
	}

	static Object getJavaSCProperty(String key) {
		if (properties == null) return null;
		Object o1 = properties.get("javasc." + key);
		return o1;
	}
	
	static Object getGuiProperty(String key) {
		if (properties == null) return null;
		Object o1 = properties.get("gui." + key);
		return o1;
	}

	static Object getPrintProperty(String key) {
		if (properties == null) return null;
		Object o1 = properties.get("print." + key);
		return o1;
	}

	static int getVerticalScreenResolution() {
		GraphicsEnvironment ge = GraphicsEnvironment.getLocalGraphicsEnvironment();
		GraphicsDevice gd = ge.getDefaultScreenDevice();
		GraphicsConfiguration gc = gd.getDefaultConfiguration();
		AffineTransform atDflt = gc.getDefaultTransform();
		atDflt.concatenate(gc.getNormalizingTransform());
		return (int)(72.00 * atDflt.getScaleY());
	}

	/**
	 * 'Preping' a float window makes it visible.
	 * There is no 'Show' function.
	 * 
	 * @param element The PREP Element from the server for this float window
	 * @throws IOException
	 */
	static void PrepFloatWindow(Element element) throws IOException {
		FloatWindow o1;
		String windowName = element.getAttributeValue(FLOATWINDOW);
		if (Client.isDebug()) {
			System.out.println("Entering Client.PrepFloatWindow, " + windowName);
			System.out.flush();
		}
		if (hwindows.containsKey(windowName)) {
			o1 = hfloatwins.get(windowName);
			o1.Close();
			hfloatwins.remove(windowName);
		}
		o1 = new FloatWindow(element);
		hfloatwins.put(windowName, o1);
		SendResult(false);
		if (Client.isDebug()) {
			System.out.println("Exiting Client.PrepWindow");
			System.out.flush();
		}
	}

	static void CloseWindow(Element element) {
		String windowName = element.getAttributeValue(WINDOW);
		if (hwindows.containsKey(windowName)) {
			SCWindow o1 = hwindows.remove(windowName);
			if (o1 != null) {
				o1.Close();		
			}
		}
		else if (hfloatwins.containsKey(windowName)) {
			FloatWindow o1 = hfloatwins.remove(windowName);
			if (o1 != null) {
				o1.Close();		
			}
		}
	}

	/**
	 * 'Preping' a window makes it visible.
	 * There is no 'Show' function.
	 * 
	 * @param element The PREP Element from the server for this window
	 * @throws IOException
	 */
	static void PrepWindow(Element element) throws IOException {
		SCWindow o1;
		String windowName = element.getAttributeValue(WINDOW);
		if (Client.isDebug()) {
			System.out.println("Entering Client.PrepWindow, " + windowName);
			System.out.flush();
		}
		if (hwindows.containsKey(windowName)) {
			o1 = hwindows.get(windowName);
			o1.Close();
			hwindows.remove(windowName);
		}
		o1 = new SCWindow(element);
		hwindows.put(windowName, o1);
		SendResult(false);
		if (Client.isDebug()) {
			System.out.println("Exiting Client.PrepWindow");
			System.out.flush();
		}
	}

	static void ChangeWindow(Element element) throws IOException {
		String windowName = element.getAttributeValue(WINDOW);
		if (IsTrace()) System.out.println("Client.ChangeWindow " + windowName);
		if (hwindows.containsKey(windowName)) {
			SCWindow o1 = hwindows.get(windowName);
			o1.Change(element);
		}
		else if (hfloatwins.containsKey(windowName)) {
			FloatWindow o1 = hfloatwins.get(windowName);
			o1.Change(element);
		}
		SendResult(false);
	}
	
	static void ChangePanel(Element element) throws IOException {
		Panel o1;
		String panelName = element.getAttributeValue(PANEL);
		if (isDebug()) System.out.println("Client.ChangePanel " + panelName);
		if (hpanels.containsKey(panelName)) {
			o1 = hpanels.get(panelName);
			try {
				o1.Change(element);
			} catch (Exception e) {
				SendResult(true, e.getMessage());
				return;
			}
		}
		SendResult(false);
	}
	
	static void ChangeDialog(Element element) throws IOException {
		Dialog o1;
		String dialogName = element.getAttributeValue(DIALOG);
		if (isDebug()) System.out.println("Client.ChangeDialog " + dialogName);
		if (hdialogs.containsKey(dialogName)) {
			o1 = hdialogs.get(dialogName);
			try {
				o1.Change(element);
			} catch (Exception e) {
				SendResult(true, e.getMessage());
				return;
			}
		}
		SendResult(false);
	}
	
	static void ChangeMenu(Element element) throws IOException {
		assert element != null && element.hasAttribute(MENU);
		String menuName = element.getAttributeValue(MENU);
		if (isDebug()) System.out.println("Client.ChangeMenu " + menuName);
		if (hmenus.containsKey(menuName)) {
			SCMenuBar o1;
			o1 = hmenus.get(menuName);
			try {
				o1.Change(element);
			} catch (Throwable e) {
				SendResult(true, e);
				return;
			}
		}
		else if (hpopmenus.containsKey(menuName)) {
			SCContextMenu o1;
			o1 = hpopmenus.get(menuName);
			try {
				o1.Change(element);
			} catch (Throwable e) {
				SendResult(true, e);
				return;
			}
		}
		SendResult(false);
	}
	
	static void QueryWindow(Element element) throws IOException {
		assert element != null && element.hasAttribute(WINDOW) && element.hasAttribute("f");
		SCWindow scw1;
		String winName = element.getAttributeValue(WINDOW);
		int ftype = Integer.parseInt(element.getAttributeValue("f"));
		if (isDebug()) System.out.println("Client.QueryWindow " + winName);
		if (hwindows.containsKey(winName)) {
			if (ftype == 3) {
				scw1 = hwindows.get(winName);
				int result = 0;
				try {
					result = scw1.GetStatusbarHeight();
				}
				catch (Exception e) {
					SendResult(true, e.getMessage());
					return;
				}
				Element e1 = new Element("statusbarheight");
				e1.addAttribute(new Attribute(WINDOW, winName)); 
				e1.addAttribute(new Attribute("t", result));
				inOutQueue.offer(new SomethingToDo(Type.outgoing, e1));
			}
			else SendResult(true);
		}
		else SendResult(true);
	}

	static void QueryPanel(Element element) throws IOException {
		assert element != null && element.hasAttribute(PANEL);
		Panel o1;
		String panelName = element.getAttributeValue(PANEL);
		int ftype = Integer.parseInt(element.getAttributeValue("f"));
		if (isDebug()) System.out.println("Client.QueryPanel " + panelName);
		if (hpanels.containsKey(panelName)) {
			o1 = hpanels.get(panelName);
			String result = null;
			try {
				result = o1.Query(element);
			}
			catch (Exception e) {
				SendResult(true, e.getMessage());
				return;
			}
			Element e1 = new Element((ftype == 2) ? "getrowcount" : STATUS);
			e1.addAttribute(new Attribute(PANEL, panelName)); 
			e1.addAttribute(new Attribute("t", result)); 
			inOutQueue.offer(new SomethingToDo(Type.outgoing, e1));
		}
		else SendResult(true);
	}
	
	static void QueryDialog(Element element) throws IOException {
		assert element != null && element.hasAttribute(DIALOG);
		Dialog o1;
		String dialogName = element.getAttributeValue(DIALOG);
		int ftype = Integer.parseInt(element.getAttributeValue("f"));
		if (isDebug()) System.out.println("Client.QueryDialog " + dialogName);
		if (hdialogs.containsKey(dialogName)) {
			o1 = hdialogs.get(dialogName);
			String result = null;
			try {
				result = o1.Query(element);
			}
			catch (Exception e) {
				SendResult(true, e.getMessage());
				return;
			}
			Element e1 = new Element((ftype == 2) ? "getrowcount" : STATUS);
			e1.addAttribute(new Attribute(DIALOG, dialogName)); 
			e1.addAttribute(new Attribute("t", result)); 
			inOutQueue.offer(new SomethingToDo(Type.outgoing, e1));
		}
		else SendResult(true);
	}
	
	static void QueryMenu(Element element) throws IOException {
		assert element != null && element.hasAttribute(MENU);
		String menuName = element.getAttributeValue(MENU);
		if (isDebug()) System.out.println("Client.QueryMenu " + menuName);
		String result = null;
		if (hmenus.containsKey(menuName)) {
			SCMenuBar o1;
			if (hmenus.containsKey(menuName)) {
				o1 = hmenus.get(menuName);
				try {
					result = o1.Query(element);
				}
				catch (Throwable e) {
					SendResult(true, e);
					return;
				}
			}
		}
		else if (hpopmenus.containsKey(menuName)) {
			SCContextMenu o1;
			if (hpopmenus.containsKey(menuName)) {
				o1 = hpopmenus.get(menuName);
				try {
					result = o1.Query(element);
				}
				catch (Throwable e) {
					SendResult(true, e);
					return;
				}
			}
		}
		Element e1 = new Element(STATUS);
		e1.addAttribute(new Attribute(MENU, menuName)); 
		e1.addAttribute(new Attribute("t", result)); 
		inOutQueue.offer(new SomethingToDo(Type.outgoing, e1));
	}
	
	/**
	 * Called by a Window Device when the close button is clicked.
	 * Send the request to the server
	 *
	 * @param name The window name
	 */
	static void WindowCloseRequest(String name) {
		Element e1 = new Element("clos").addAttribute(new Attribute("n", name));
		inOutQueue.add(new SomethingToDo(Type.outgoing, e1));
	}

	static void windowMouseMovedEvent(String name, Point oldLoc, Point newLoc) {
		Element e1 = new Element("posn");
		if (!windowNameLastPOSN.equals(name)) {
			e1.addAttribute(new Attribute("n", name));
			windowNameLastPOSN = name;
		}
		if (oldLoc.x != newLoc.x) e1.addAttribute(new Attribute("h", newLoc.x)); 
		if (oldLoc.y != newLoc.y) e1.addAttribute(new Attribute("v", newLoc.y)); 
		inOutQueue.offer(new SomethingToDo(Type.outgoing, e1));
	}
	
	public static void ControlButtonEvent(String resourceName, Node control, MouseEvent event) {
		EventType<? extends Event> evnt1 = event.getEventType();
		StringBuilder sb1 = new StringBuilder(4);
		sb1.append('r');
		if (evnt1 == MouseEvent.MOUSE_PRESSED) sb1.append("bdn");
		else if (evnt1 == MouseEvent.MOUSE_RELEASED) sb1.append("bup");
		else if (evnt1 == MouseEvent.MOUSE_CLICKED) sb1.append("bdc");
		Element e1 = new Element(sb1.toString()).addAttribute(new Attribute("n", resourceName));
		e1.addAttribute(new Attribute("i", control.getId()));
		Point2D pt1 = control.localToScene(event.getX(), event.getY());
		e1.addAttribute(new Attribute("h", (int)pt1.getX()));
		e1.addAttribute(new Attribute("v", (int)pt1.getY()));
		addMkeysAttribute(e1, event);
		inOutQueue.add(new SomethingToDo(Type.outgoing, e1));
	}
	
	static void windowMouseButtonEvent(String name, MouseEvent event) {
		EventType<? extends Event> evnt1 = event.getEventType();
		StringBuilder sb1 = new StringBuilder(4);
		if (event.getButton() == MouseButton.PRIMARY) sb1.append('l');
		else if (event.getButton() == MouseButton.SECONDARY) sb1.append('r');
		else if (event.getButton() == MouseButton.MIDDLE) sb1.append('m');
		if (evnt1 == MouseEvent.MOUSE_PRESSED) sb1.append("bdn");
		else if (evnt1 == MouseEvent.MOUSE_RELEASED) sb1.append("bup");
		else if (evnt1 == MouseEvent.MOUSE_CLICKED) sb1.append("bdc");
		Element e1 = new Element(sb1.toString()).addAttribute(new Attribute("n", name));
		e1.addAttribute(new Attribute("h", (int)event.getX()));
		e1.addAttribute(new Attribute("v", (int)event.getY()));
		addMkeysAttribute(e1, event);
		inOutQueue.add(new SomethingToDo(Type.outgoing, e1));
	}
	
	static private void addMkeysAttribute(Element e1, MouseEvent event)
	{
		EventType<? extends Event> evnt1 = event.getEventType();
		if (evnt1 == MouseEvent.MOUSE_PRESSED
				|| evnt1 == MouseEvent.MOUSE_RELEASED
				|| evnt1 == MouseEvent.MOUSE_CLICKED) {
			if (event.isShiftDown() || event.isControlDown()) {
				if (event.isShiftDown() && event.isControlDown())
					e1.addAttribute(new Attribute("mkeys", 3));
				else if (event.isControlDown())
					e1.addAttribute(new Attribute("mkeys", 1));
				else e1.addAttribute(new Attribute("mkeys", 2));
			}
		}
	}
	 
	static void WindowMinimized(String name) {
		Element e1 = new Element("wmin").addAttribute(new Attribute("n", name));
		inOutQueue.add(new SomethingToDo(Type.outgoing, e1));
	}

	static void WindowFocused(String name) {
		Element e1 = new Element("wact").addAttribute(new Attribute("n", name));
		inOutQueue.add(new SomethingToDo(Type.outgoing, e1));
	}

	/**
	 * Send a WSIZ message
	 * @param name Window name
	 * @param width New width
	 * @param height New height
	 */
	static void WindowChangedSize(String name, int width, int height) {
		Element e1 = new Element("wsiz").addAttribute(new Attribute("n", name));
		e1.addAttribute(new Attribute("h", width));
		e1.addAttribute(new Attribute("v", height));
		inOutQueue.offer(new SomethingToDo(Type.outgoing, e1));
	}

	static void WindowChangedPos(String name, int x, int y) {
		Element e1 = new Element("wpos").addAttribute(new Attribute("n", name));
		e1.addAttribute(new Attribute("h", x));
		e1.addAttribute(new Attribute("v", y));
		inOutQueue.add(new SomethingToDo(Type.outgoing, e1));
	}

	static void WindowMouseExit(String name, String mouseExitMode) {
		Element e1 = new Element(mouseExitMode).addAttribute(new Attribute("n", name));
		inOutQueue.offer(new SomethingToDo(Type.outgoing, e1));
	}


	/**
	 * Keep Alive thread
	 *
	 */
	private final class KeepAlive extends Thread {

		private final SomethingToDo ka; 
		
		KeepAlive() {
			ka = new SomethingToDo(Type.outgoing, new Element("alivechk")); 
		}
		
		/* (non-Javadoc)
		 * @see java.lang.Runnable#run()
		 */
		@Override
		public void run() {
		    try {
		        sleep(20000);
		    } catch (InterruptedException e1) {/* DO NOTHING */}
		    for (;;) {
		        if (abortflag) return;
		        try {
		            sleep(30000);
					inOutQueue.put(ka);
		        } catch (InterruptedException e) {/* DO NOTHING */}
		    }
		}

	}

	static void PrepMenu(Element element) throws IOException {
		assert element.hasAttribute(MENU) || element.hasAttribute(POPUPMENU);
		String menuName;
		if (element.hasAttribute(MENU)) {
			SCMenuBar menu;
			menuName = element.getAttributeValue(MENU);
			if (hmenus.containsKey(menuName)) {
				menu = hmenus.get(menuName);
				menu.Close();
				hmenus.remove(menuName);
			}
			try {
				menu = new SCMenuBar(element);
			} catch (Throwable e) {
				SendResult(true, e);
				return;
			}
			hmenus.put(menuName, menu);
		}
		else if (element.hasAttribute(POPUPMENU)) {
			SCContextMenu menu;
			menuName = element.getAttributeValue(POPUPMENU);
			if (hpopmenus.containsKey(menuName)) {
				menu = hpopmenus.get(menuName);
				menu.Close();
				hpopmenus.remove(menuName);
			}
			try {
				menu = new SCContextMenu(element);
			} catch (Throwable e) {
				SendResult(true, e);
				return;
			}
			hpopmenus.put(menuName, menu);
		}
		SendResult(false);
	}

	static void ShowMenuOnWindow(String menuName, String windowName, int h, int v) throws IOException
	{
		if (Client.isDebug()) {
			System.out.println("Entering Client.ShowMenuOnWindow");
			System.out.flush();
		}
		if (!hmenus.containsKey(menuName) && !hpopmenus.containsKey(menuName)) {
			SendResult(true);
		}
		if (!hwindows.containsKey(windowName)) {
			SendResult(true);
		}
		SCWindow win1 = hwindows.get(windowName);
		if (hmenus.containsKey(menuName)) win1.ShowMenuBar(hmenus.get(menuName));
		else win1.ShowPopupMenu(hpopmenus.get(menuName), h, v);
		SendResult(false);
	}

	static void HideMenu(Element element) throws IOException
	{
		assert element != null && element.hasAttribute(MENU);
		String 	menuName = element.getAttributeValue(MENU);
		if (hmenus.containsKey(menuName)) {
			hmenus.get(menuName).Hide();
		}
		else if (hpopmenus.containsKey(menuName)) {
			hpopmenus.get(menuName).Hide();
		}
		SendResult(false);
	}
	
	static void CloseMenu(Element element) {
		assert element != null && element.hasAttribute(MENU);
		String menuName = element.getAttributeValue(MENU);
		if (hmenus.containsKey(menuName)) {
			SCMenuBar o1 = hmenus.remove(menuName);
			if (o1 != null) o1.Close();
		}
		else if (hpopmenus.containsKey(menuName)) {
			SCContextMenu o1 = hpopmenus.remove(menuName);
			if (o1 != null) o1.Close();
		}
	}

	static void PrepPanel(Element element) throws IOException {
		assert element != null && element.hasAttribute(PANEL);
		Panel o1;
		String panelName = element.getAttributeValue(PANEL);
		if (hpanels.containsKey(panelName)) {
			o1 = hpanels.get(panelName);
			o1.Close();
			hpanels.remove(panelName);
			if (Client.isDebug())
				System.out.printf("In Client.PrepPanel, dup panel name %s, old one closed\n", panelName);
		}
		try {
			o1 = new Panel(element);
		} catch (Throwable e) {
			SendResult(true, e);
			return;
		}
		hpanels.put(panelName, o1);
		SendResult(false);
	}

	static void PrepDialog(Element element) throws IOException {
		Dialog o1;
		String dialogName = element.getAttributeValue(DIALOG);
		if (hdialogs.containsKey(dialogName)) {
			o1 = hdialogs.get(dialogName);
			o1.Close();
			hdialogs.remove(dialogName);
		}
		try {
			o1 = new Dialog(element);
		} catch (Throwable e) {
			SendResult(true, e);
			return;
		}
		hdialogs.put(dialogName, o1);
		SendResult(false);
	}
	
	static void ClosePanel(Element element) {
		Panel o1 = hpanels.remove(element.getAttributeValue(PANEL));
		if (o1 != null) {
			o1.Close();		
		}
	}

	static void CloseDialog(Element element) {
		Dialog o1 = hdialogs.remove(element.getAttributeValue(DIALOG));
		if (o1 != null) {
			o1.Close();		
		}
	}

	static void PrepImage(Element element) throws IOException {
		assert element != null && element.hasAttribute(IMAGE);
		ImageResource o1;
		String imageName = element.getAttributeValue(IMAGE);
		if (himages.containsKey(imageName)) {
			himages.remove(imageName);
		}
		try {
			o1 = new ImageResource(element);
		} catch (Exception e) {
			SendResult(true, e.getMessage());
			return;
		}
		himages.put(imageName, o1);
		o1.getContent().setId(imageName);
		SendResult(false);
	}
	
	static void PrepAlertDialog(Element element) throws IOException {
		Objects.requireNonNull(element, "element cannot be null");
		SCAlertDialog ad1;
		String alrtdlgName = element.getAttributeValue(ALERTDLG);
		if (halertdlgs.containsKey(alrtdlgName)) {
			halertdlgs.remove(alrtdlgName);
		}
		try {
			ad1 = new SCAlertDialog(element);
			ad1.setId(alrtdlgName);
		}
		catch (Exception e) {
			SendResult(true, e.getMessage());
			return;
		}
		halertdlgs.put(alrtdlgName, ad1);
		SendResult(false);
	}
	
	static void PrepColorDialog(Element element) throws IOException {
		Objects.requireNonNull(element, "element cannot be null");
		SCColorDialog cd1;
		String clrdlgName = element.getAttributeValue(COLORDLG);
		if (hcolordlgs.containsKey(clrdlgName)) {
			hcolordlgs.remove(clrdlgName);
		}
		try {
			cd1 = new SCColorDialog(element);
			cd1.setId(clrdlgName);
		}
		catch (Exception e) {
			SendResult(true, e.getMessage());
			return;
		}
		hcolordlgs.put(clrdlgName, cd1);
		SendResult(false);
	}
	
	static void PrepFontDialog(Element element) throws IOException {
		Objects.requireNonNull(element, "element cannot be null");
		SCFontDialog fd1;
		String fntdlgName = element.getAttributeValue(FONTDLG);
		if (hfontdlgs.containsKey(fntdlgName)) {
			hfontdlgs.remove(fntdlgName);
		}
		try {
			fd1 = new SCFontDialog(element);
			fd1.setId(fntdlgName);
		}
		catch (Exception e) {
			SendResult(true, e.getMessage());
			return;
		}
		hfontdlgs.put(fntdlgName, fd1);
		SendResult(false);
	}
	
	static void PrepOpenDirDialog(Element element) throws IOException {
		assert element != null && element.hasAttribute(OPENDDLG);
		SCDirChooser dc;
		String odirdlgName = element.getAttributeValue(OPENDDLG);
		if (hdirdlgs.containsKey(odirdlgName)) {
			hdirdlgs.remove(odirdlgName);
		}
		try {
			dc = new SCDirChooser(element);
			dc.setId(odirdlgName);
		} catch (Exception e) {
			SendResult(true, e.getMessage());
			return;
		}
		hdirdlgs.put(odirdlgName, dc);
		SendResult(false);
	}
	
	static void PrepOpenFileDialog(Element element) throws IOException {
		assert element != null && element.hasAttribute(OPENFDLG);
		SCFileChooser fc;
		String ofiledlgName = element.getAttributeValue(OPENFDLG);
		if (hofiledlgs.containsKey(ofiledlgName)) {
			hofiledlgs.remove(ofiledlgName);
		}
		try {
			fc = new SCFileChooser(element);
			fc.setId(ofiledlgName);
		} catch (Exception e) {
			SendResult(true, e.getMessage());
			return;
		}
		hofiledlgs.put(ofiledlgName, fc);
		SendResult(false);
	}
	
	static void PrepSaveFileDialog(Element element) throws IOException {
		assert element != null && element.hasAttribute(SAVEFDLG);
		SCFileChooser fc;
		String sfiledlgName = element.getAttributeValue(SAVEFDLG);
		if (hsfiledlgs.containsKey(sfiledlgName)) {
			hsfiledlgs.remove(sfiledlgName);
		}
		try {
			fc = new SCFileChooser(element);
			fc.setId(sfiledlgName);
		} catch (Exception e) {
			SendResult(true, e.getMessage());
			return;
		}
		hsfiledlgs.put(sfiledlgName, fc);
		SendResult(false);
	}
	
	static void CloseOpenDirDialog(Element element) {
		assert element != null && element.hasAttribute(OPENDDLG);
		SCDirChooser o1 = hdirdlgs.remove(element.getAttributeValue(OPENDDLG));
		if (o1 != null) {
			o1.Close();		
		}
	}
	
	static void CloseAlertDialog(Element element) {
		assert element != null && element.hasAttribute(ALERTDLG);
		SCAlertDialog o1 = halertdlgs.remove(element.getAttributeValue(ALERTDLG));
		if (o1 != null) {
			o1.Close();		
		}
	}
	
	static void CloseColorDialog(Element element) {
		assert element != null && element.hasAttribute(COLORDLG);
		SCColorDialog o1 = hcolordlgs.remove(element.getAttributeValue(COLORDLG));
		if (o1 != null) {
			o1.Close();		
		}
	}
	
	static void CloseFontDialog(Element element) {
		assert element != null && element.hasAttribute(FONTDLG);
		SCFontDialog o1 = hfontdlgs.remove(element.getAttributeValue(FONTDLG));
		if (o1 != null) {
			o1.Close();		
		}
	}
	
	static void CloseOpenFileDialog(Element element) {
		assert element != null && element.hasAttribute(OPENFDLG);
		SCFileChooser o1 = hofiledlgs.remove(element.getAttributeValue(OPENFDLG));
		if (o1 != null) {
			o1.Close();		
		}
	}
	
	static void CloseSaveFileDialog(Element element) {
		assert element != null && element.hasAttribute(SAVEFDLG);
		SCFileChooser o1 = hsfiledlgs.remove(element.getAttributeValue(SAVEFDLG));
		if (o1 != null) {
			o1.Close();		
		}
	}
	
	static void PrepIcon(Element element) throws IOException {
		assert element != null && element.hasAttribute(ICON);
		IconResource o1;
		String iconName = element.getAttributeValue(ICON);
		if (hicons.containsKey(iconName)) {
			hicons.remove(iconName);
		}
		try {
			o1 = new IconResource(element);
		} catch (Exception e) {
			SendResult(true, e.getMessage());
			return;
		}
		hicons.put(iconName, o1);
		SendResult(false);
	}
	
	static void CloseIcon(Element element) {
		hicons.remove(element.getAttributeValue(ICON));
	}

	static void CloseImage(Element element) {
		himages.remove(element.getAttributeValue(IMAGE));
	}

	/**
	 * Get an Icon by name
	 * @param name The Icon's Name
	 * @return The Icon or null if it cannot be found
	 */
	public static IconResource getIcon(String name) {
		return hicons.get(name);
	}
	

	static void ShowOpenDirDlgOnWindow(final String dlgName, String windowName) throws IOException {
		if (!hdirdlgs.containsKey(dlgName)) {
			SendResult(true);
		}
		if (!hwindows.containsKey(windowName)) {
			SendResult(true);
		}
		final SCWindow win1 = hwindows.get(windowName);
		hdirdlgs.get(dlgName).showOn(win1);
		SendResult(false);
	}
	
	static void ShowOpenFileDlgOnWindow(final String dlgName, String windowName) throws IOException {
		if (!hofiledlgs.containsKey(dlgName)) {
			SendResult(true);
		}
		if (!hwindows.containsKey(windowName)) {
			SendResult(true);
		}
		final SCWindow win1 = hwindows.get(windowName);
		hofiledlgs.get(dlgName).showOpenOn(win1);
		SendResult(false);
	}
	
	static void ShowSaveFileDlgOnWindow(final String dlgName, String windowName) throws IOException {
		if (!hsfiledlgs.containsKey(dlgName)) {
			SendResult(true);
		}
		if (!hwindows.containsKey(windowName)) {
			SendResult(true);
		}
		final SCWindow win1 = hwindows.get(windowName);
		hsfiledlgs.get(dlgName).showSaveOn(win1);
		SendResult(false);
	}
	
	static void ShowAlertDlgOnWindow(final String dlgName, String windowName) throws IOException {
		if (!halertdlgs.containsKey(dlgName)) {
			SendResult(true, "Unknown dialog name");
		}
		if (!hwindows.containsKey(windowName)) {
			SendResult(true, "Unknown window");
		}
		final SCWindow win1 = hwindows.get(windowName);
		halertdlgs.get(dlgName).showOn(win1);
		// An ALERT dialog does NOT send an acknowledgment until
		// dismissed, then it send an OK message with n=<dialog name>
	}
	
	static void ShowColorDlgOnWindow(final String dlgName, String windowName) throws IOException {
		if (!hcolordlgs.containsKey(dlgName)) {
			SendResult(true, "Unknown dialog name");
		}
		if (!hwindows.containsKey(windowName)) {
			SendResult(true, "Unknown window");
		}
		final SCWindow win1 = hwindows.get(windowName);
		hcolordlgs.get(dlgName).showOn(win1);
		// An COLOR dialog does NOT send an acknowledgment until
		// dismissed, then it send an OK message with n=<dialog name> and t=<color>
	}
	
	static void ShowFontDlgOnWindow(final String dlgName, String windowName) throws IOException {
		if (!hfontdlgs.containsKey(dlgName)) {
			SendResult(true, "Unknown dialog name");
		}
		if (!hwindows.containsKey(windowName)) {
			SendResult(true, "Unknown window");
		}
		final SCWindow win1 = hwindows.get(windowName);
		hfontdlgs.get(dlgName).showOn(win1);
		// A FONT dialog does NOT send an acknowledgment until
		// dismissed, then it send an OK message with n=<dialog name> and t=<font>
	}
	
	static void ShowImageOnWindow(String imageName, String windowName, int X, int Y) throws IOException {
		if (Client.isDebug()) {
			System.out.println("Entering Client.ShowImageOnWindow");
			System.out.flush();
		}
		if (!himages.containsKey(imageName)) {
			SendResult(true);
		}
		if (!hwindows.containsKey(windowName) && !hfloatwins.containsKey(windowName)) {
			SendResult(true);
		}
		WindowDevice win1 =
				hwindows.containsKey(windowName) ? hwindows.get(windowName) : hfloatwins.get(windowName);
		win1.ShowImage(himages.get(imageName), X, Y);
		SendResult(false);
	}
	
	static void ShowPanelOnWindow(String panelName, String windowName, int X, int Y) throws IOException {
		if (Client.isDebug()) {
			System.out.println("Entering Client.ShowPanelOnWindow");
			System.out.flush();
		}
		if (!hpanels.containsKey(panelName)) {
			SendResult(true);
		}
		if (!hwindows.containsKey(windowName) && !hfloatwins.containsKey(windowName)) {
			SendResult(true);
		}
		WindowDevice win1 =
				hwindows.containsKey(windowName) ? hwindows.get(windowName) : hfloatwins.get(windowName);
		win1.ShowPanel(hpanels.get(panelName), X, Y);
		SendResult(false);
	}

	static void ShowDialogOnWindow(String dialogName, String windowName, int X, int Y) throws IOException {
		if (Client.isDebug()) {
			System.out.printf("Entering Client.ShowDialog %s on Window %s\n", dialogName, windowName);
			System.out.flush();
		}
		if (!hdialogs.containsKey(dialogName)) {
			SendResult(true);
		}
		if (!hwindows.containsKey(windowName)) {
			SendResult(true);
		}
		SCWindow win1 = hwindows.get(windowName);
		hdialogs.get(dialogName).showOn(win1, X, Y);
		SendResult(false);
	}

	static void HidePanel(Element element) throws IOException
	{
		assert element != null && element.hasAttribute(PANEL);
		String panelName = element.getAttributeValue(PANEL);
		if (!hpanels.containsKey(panelName)) {
			SendResult(true);
		}
		Panel pnl = hpanels.get(panelName);
		pnl.Hide();
		SendResult(false);
	}
	
	static void HideDialog(Element element) throws IOException
	{
		assert element != null && element.hasAttribute(DIALOG);
		String dialogName = element.getAttributeValue(DIALOG);
		if (!hdialogs.containsKey(dialogName)) {
			SendResult(true);
		}
		Dialog dlg1 = hdialogs.get(dialogName);
		dlg1.Hide();
		SendResult(false);
	}
	
	static void HideImage(Element element) throws IOException
	{
		String imageName = element.getAttributeValue(IMAGE);
		if (!himages.containsKey(imageName)) {
			SendResult(true);
		}
		ImageResource im1 = himages.get(imageName);
		im1.Hide();
		SendResult(false);
	}
	
	public static void ButtonPushed(String resourceName, String item) {
		Element e1 = new Element("push").addAttribute(new Attribute("n", resourceName));
		e1.addAttribute(new Attribute("i", item));
		inOutQueue.offer(new SomethingToDo(Type.outgoing, e1));
	}

	public static void CancelMessage(String id) {
		NITMessage("canc", id, null, null);
	}
	
	public static void OkMessage(String id) {
		NITMessage("ok", id, null, null);
	}

	public static void OkMessage(String id, String data) {
		NITMessage("ok", id, null, data);
	}

	public static void OkMessage(String id, File file) {
		try {
			NITMessage("ok", id, null, file.getCanonicalPath());
		} catch (IOException e) {
			Element e1 = new Element("r").addAttribute(new Attribute("e", e.getMessage()));
			inOutQueue.offer(new SomethingToDo(Type.outgoing, e1));
		}
	}

	public static void OkMessage(String id, List<File> fileList) {
		try {
			StringBuilder sb1 = new StringBuilder();
			for (File f1 : fileList) {
				sb1.append(f1.getCanonicalPath()).append(',');
			}
			sb1.setLength(sb1.length() - 1);
			NITMessage("ok", id, null, sb1.toString());
		} catch (IOException e) {
			Element e1 = new Element("r").addAttribute(new Attribute("e", e.getMessage()));
			inOutQueue.offer(new SomethingToDo(Type.outgoing, e1));
		}
	}

	/**
	 * 
	 * @param resourceName the resource name
	 * @param columnId The Column item
	 * @param rowIndex Expecting One-based, five characters wide
	 */
	public static void TablePopboxButtonPushed(String resourceName, String columnId, String rowIndex) {
		NITMessage("push", resourceName, columnId, rowIndex);
	}

	public static void ItemMessage(String resourceName, String item, String data) {
		NITMessage("item", resourceName, item, data);
	}

	static void EselMessage(String resourceName, String item, String data) {
		NITMessage("esel", resourceName, item, data);
	}

	public static void pickMessage(String resourceName, String item, String data) {
		NITMessage("pick", resourceName, item, data);
	}

	public static void TreeExpandedCollapsedEvent(String msgCode, String resourceName, String item,
			String data) {
		NITMessage(msgCode, resourceName, item, data);
	}

	public static void MenuMessage(String resourceName, String item) {
		if (Client.isDebug()) System.out.printf("Client.MenuMessage from = '%s'\n", resourceName);
		NITMessage(MENU, resourceName, item, null);
	}
	
	/**
	 * For NKEY messages from a Control
	 * @param resourceName Name of Panel or Dialog
	 * @param item	DB/C Item number
	 * @param data  key code
	 */
	public static void NkeyMessage(String resourceName, String item, String data) {
		NITMessage("nkey", resourceName, item, data);
	}

	/**
	 * For NKEY messages from a Window
	 * @param windowName Name of Window
	 * @param data  key code
	 */
	static void NkeyMessage(String windowName, String data) {
		NITMessage("nkey", windowName, null, data);
	}

	public static void sendRMessageNoError() {
		inOutQueue.offer(new SomethingToDo(Type.outgoing, eresult));
	}
	
	/**
	 * For CHAR messages from a Window
	 * @param windowName Name of Window
	 * @param char1 The Key (e.g. 'a', 'b', '4')
	 */
	static void CharMessage(String windowName, char char1) {
		NITMessage("char", windowName, null, String.valueOf(char1));
	}

	static void FocXMessage(String resourceName, String item, boolean x) {
		NITMessage(x ? "focs" : "focl", resourceName, item, null);
	}

	private static void NITMessage(String mcode, String sourceName, String item, String data) {
		Element e1 = new Element(mcode).addAttribute(new Attribute("n", sourceName));
		if (item != null) e1.addAttribute(new Attribute("i", item));
		if (data != null) e1.addAttribute(new Attribute("t", data));
		inOutQueue.offer(new SomethingToDo(Type.outgoing, e1));
	}

	static void SBMessage(String postfix, String resourceName, String item, int intValue) {
		Element e1 = new Element("sb" + postfix).addAttribute(new Attribute("n", resourceName));
		e1.addAttribute(new Attribute("i", item));
		e1.addAttribute(new Attribute("pos", intValue));
		inOutQueue.offer(new SomethingToDo(Type.outgoing, e1));
	}

	static void DrawOnImage(Element element) {
		String imageName = element.getAttributeValue(IMAGE);
		ImageResource ir = himages.get(imageName);
		if (ir != null) ir.draw(element);
	}

	static void StoreImageBits(Element element) throws Exception {
		String imageName = element.getAttributeValue(IMAGE);
		ImageResource ir = himages.get(imageName);
		if (ir != null) ir.Load(element);
	}

	static void GetImageBits(Element element) {
		String imageName = element.getAttributeValue(IMAGE);
		ImageResource ir = himages.get(imageName);
		Element e1 = new Element("image");
		e1.addAttribute(new Attribute("image", imageName));
		e1.addAttribute(new Attribute("bpp", ir.getColorbits()));
		e1.addAttribute(new Attribute("bits", ir.getimagebits().toString()));
		inOutQueue.offer(new SomethingToDo(Type.outgoing, e1));
	}
	
	/**
	 * @param imageName The db/c name of the image resource
	 * @return The ImageResource object or null if not found
	 */
	public static ImageResource GetImage(String imageName) {
		return himages.get(imageName);
	}

	static void PrepToolbar(Element element) throws IOException {
		assert element != null && element.hasAttribute(TOOLBAR);
		String toolName = element.getAttributeValue(TOOLBAR);
		SCToolBar tool;
		if (htoolbars.containsKey(toolName)) {
			tool = htoolbars.get(toolName);
			tool.Close();
			htoolbars.remove(toolName);
		}
		try {
			tool = new SCToolBar(element);
		} catch (Exception e) {
			if (Client.isDebug()) {
				e.printStackTrace();
			}
			SendResult(true, e.getMessage());
			return;
		}
		htoolbars.put(toolName, tool);
		SendResult(false);
	}

	static void CloseToolbar(Element element) {
		assert element != null && element.hasAttribute(TOOLBAR);
		SCToolBar o1 = htoolbars.remove(element.getAttributeValue(TOOLBAR));
		if (o1 != null) {
			o1.Close();		
		}
	}

	static void HideToolbar(Element element) throws IOException {
		assert element != null && element.hasAttribute(TOOLBAR);
		String toolName = element.getAttributeValue(TOOLBAR);
		if (!htoolbars.containsKey(toolName)) {
			SendResult(true);
		}
		SCToolBar tll = htoolbars.get(toolName);
		tll.Hide();
		SendResult(false);
	}

	static void QueryToolbar(Element element) throws IOException {
		assert element != null && element.hasAttribute(TOOLBAR);
		SCToolBar o1;
		String toolName = element.getAttributeValue(TOOLBAR);
		if (isDebug()) System.out.println("Client.QueryToolbar " + toolName);
		if (htoolbars.containsKey(toolName)) {
			o1 = htoolbars.get(toolName);
			String result = null;
			try {
				result = o1.Query(element);
			}
			catch (Exception e) {
				SendResult(true, e.getMessage());
				return;
			}
			Element e1 = new Element(STATUS);
			e1.addAttribute(new Attribute(TOOLBAR, toolName)); 
			e1.addAttribute(new Attribute("t", result)); 
			inOutQueue.offer(new SomethingToDo(Type.outgoing, e1));
		}
		else SendResult(true);
	}

	static void ChangeOpenDirDlg(Element element) throws IOException {
		assert element != null && element.hasAttribute(OPENDDLG);
		SCDirChooser o1;
		String odirName = element.getAttributeValue(OPENDDLG);
		if (isDebug()) System.out.println("Client.ChangeOpenDirDlg " + odirName);
		if (hdirdlgs.containsKey(odirName)) {
			o1 = hdirdlgs.get(odirName);
			try {
				o1.Change(element);
			} catch (Exception e) {
				SendResult(true, e.getMessage());
				return;
			}
		}
		SendResult(false);
	}
	
	static void ChangeOpenFileDlg(Element element) throws IOException {
		assert element != null && element.hasAttribute(OPENFDLG);
		SCFileChooser o1;
		String ofileName = element.getAttributeValue(OPENFDLG);
		if (isDebug()) System.out.println("Client.ChangeOpenFileDlg " + ofileName);
		if (hofiledlgs.containsKey(ofileName)) {
			o1 = hofiledlgs.get(ofileName);
			try {
				o1.Change(element);
			} catch (Exception e) {
				SendResult(true, e.getMessage());
				return;
			}
		}
		SendResult(false);
	}
	
	static void ChangeSaveFileDlg(Element element) throws IOException {
		assert element != null && element.hasAttribute(SAVEFDLG);
		SCFileChooser o1;
		String sfileName = element.getAttributeValue(SAVEFDLG);
		if (isDebug()) System.out.println("Client.ChangeSaveFileDlg " + sfileName);
		if (hsfiledlgs.containsKey(sfileName)) {
			o1 = hsfiledlgs.get(sfileName);
			try {
				o1.Change(element);
			} catch (Exception e) {
				SendResult(true, e.getMessage());
				return;
			}
		}
		SendResult(false);
	}
	
	static void ChangeToolbar(Element element) throws IOException {
		assert element != null && element.hasAttribute(TOOLBAR);
		SCToolBar o1;
		String toolName = element.getAttributeValue(TOOLBAR);
		if (isDebug()) System.out.println("Client.ChangeToolbar " + toolName);
		if (htoolbars.containsKey(toolName)) {
			o1 = htoolbars.get(toolName);
			try {
				o1.Change(element);
			} catch (Exception e) {
				SendResult(true, e.getMessage());
				return;
			}
		}
		SendResult(false);
	}

	static void ShowToolBarOnWindow(String toolName, String windowName) throws IOException {
		if (Client.isDebug()) {
			System.out.println("Entering Client.ShowToolBarOnWindow");
			System.out.flush();
		}
		if (!htoolbars.containsKey(toolName)) {
			SendResult(true);
		}
		if (!hwindows.containsKey(windowName)) {
			SendResult(true);
		}
		SCWindow win1 = hwindows.get(windowName);
		win1.ShowToolBar(htoolbars.get(toolName));
		SendResult(false);
	}

	/**
	 * Used only for image statistics
	 * Will always respond '24' for bitsperpixel
	 * Will always respond ' 0 0' for resolution
	 * Uses AWT for image size. It seems to be more accurate (?!)
	 * @param element
	 */
	static void QueryClipboard(final Element element) {
		Runnable runnable = new Runnable() {
			Element e1;
			{
				e1 = new Element(STATUS);
				e1.addAttribute(element.getAttribute(CLIPBOARD));
			}
			
			@Override public void run() {
				String ftype = element.getAttributeValue("f");
				boolean hasImage = awtClip.isDataFlavorAvailable(DataFlavor.imageFlavor);
				int width = 0;
				int height = 0;
				switch (ftype) {
				case "bitsperpixel":
					e1.addAttribute(new Attribute("h", hasImage ? "24" : "0")); 
					break;
				case "resolution":
					e1.addAttribute(new Attribute("h", "0")); 
					e1.addAttribute(new Attribute("v", "0")); 
					break;
				case "imagesize":
					if (hasImage) {
						try {
							java.awt.Image awtImg = (java.awt.Image) awtClip.getData(DataFlavor.imageFlavor);
							width = awtImg.getWidth(null);
							height = awtImg.getHeight(null);
						} catch (UnsupportedFlavorException | IOException e) {
							/* Do Nothing */
						}
					}
					e1.addAttribute(new Attribute("h", width)); 
					e1.addAttribute(new Attribute("v", height)); 
					break;
				}
				inOutQueue.offer(new SomethingToDo(Type.outgoing, e1));
			}
		};
		SmartClientApplication.Invoke(runnable, true);
	}

	/**
	 * The runtime is not expecting a return for this command
	 * @param text The text to put on the Clipboard
	 */
	static void setClipboard(String text) {
		final ClipboardContent cc = new ClipboardContent();
		cc.putString(text);
		SmartClientApplication.Invoke(new Runnable() {
			@Override public void run() {
				SmartClientApplication.clipboard.setContent(cc);
			}
		}, true);
	}

	/**
	 * The runtime is not expecting a return for this command
	 * @param ires The ImageResource to put on the Clipboard
	 */
	static void setClipboard(ImageResource ires) {
		final ClipboardContent cc = new ClipboardContent();
		final Node n1 = ires.getCanvas();
		SmartClientApplication.Invoke(() -> {
			Image img = n1.snapshot(null, null);
			cc.putImage(img);
			SmartClientApplication.clipboard.setContent(cc);
		}, true);
	}

	/**
	 * Get text from the clipboard and return it to the server
	 * Returns the text as the one-and-only child Element of the return Element
	 */
	static void getClipboard() {
		final Element e1 = new Element("r");
		SmartClientApplication.Invoke(() -> {
			String s1 = SmartClientApplication.clipboard.getString();
			if (s1 == null) s1 = "";
			e1.addString(s1);
		}, true);
		inOutQueue.offer(new SomethingToDo(Type.outgoing, e1));
	}

	/**
	 * The server does NOT expect a return from this one.
	 * We load the local Image from the clipboard
	 * @param ires The ImageResource to be loaded into
	 */
	static void getClipboard(final ImageResource ires) {
		SmartClientApplication.Invoke(() -> {
			if (SmartClientApplication.clipboard.hasImage()) {
				if (Client.isDebug()) System.out.println("Client.getClipboard(Image)!");
				ires.setImage(SmartClientApplication.clipboard.getImage());
			}
		}, true);
	}

	public static SCWindow getWindow(String value) {
		if (hwindows.containsKey(value)) return hwindows.get(value);
		return null;
	}

	public static void changeApplication(Element element) throws IOException {
		Attribute a1;
		if ((a1 = element.getAttribute("f")) != null) {
			if (a1.value.equalsIgnoreCase("taskbartext") || a1.value.equalsIgnoreCase("desktopicon")) {
				a1 = element.getAttribute("l");
				if (a1.value.equalsIgnoreCase("desktopicon")) {
					//TODO
				}
				else if (a1.value.equalsIgnoreCase("taskbartext")) {
					//TODO
				}
			}
		}
		SendResult(false);
	}

	/**
	 * If a MenuBar is currently visible on the given window, return
	 * the SCMenuBar, else null
	 * @param visibleOnWindow
	 * @return The SCMenuBar or null
	 */ 
	public static SCMenuBar getMenuForWindow(WindowDevice visibleOnWindow) {
		for (SCMenuBar menu : hmenus.values()) {
			if (menu.visibleOnWindow == visibleOnWindow) {
				return menu;
			}
		}
		return null;
	}
	/*
	 * Translate unicode key value to DX compatable key value
	 */
	static String translatekey(int key) {
		return String.valueOf(key - ((key >= '\uF000') ? (int) '\uF000' : 0));
	}

	/*
	 * Translate DX compatable key value to unicode key value
	 */
	static char translatekey(String value) {
		int key = Integer.parseInt(value);
		if (key == 505)
			return KeyinDisplay.KEY_INTERRUPT; // formula below would be off by
												// 1 for this
		return (char) (key + ((key >= 256) ? (int) '\uF000' : 0));
	}
	
	public void queueKDAction() {
		SomethingToDo todo = new SomethingToDo(Type.keyinDisplay);
		inOutQueue.add(todo);
	}

	KeyinDisplay getW1() {
		if (w1 == null) {
			if (isDebug()) {
				System.out.println("Entering Client.getW1");
			}
			w1 = openkds();
		}
		return w1;
	}

	KeyinDisplay openkds()
	{
		int i1;
		if (isDebug()) {
			System.out.println("Entering Client.openkds");
		}
		w1 = new KeyinDisplay(kdswinflag, displayhsize, displayvsize, displayfont, displayfontsize, displayfontcharwidth, displayfontcharheight, tdbfilename, keyindevice, null);
		w1.cursornorm();
		keyinchars = new char[32];
		if (keyincancelkey != 0) KeyinDisplay.setCancelkey(keyincancelkey);
		if (keyininterruptkey != 0) KeyinDisplay.setInterruptkey(keyininterruptkey);
		for (i1 = KeyinDisplay.KEY_F1; i1 <= KeyinDisplay.KEY_F20; i1++) w1.setendkey((char) i1);
		for (i1 = 0; i1 < straps.size(); i1++) {
			w1.settrapkey(straps.get(i1).intValue(), true);
		}
		straps.clear();
		w1.setendkey(KeyinDisplay.KEY_UP);
		w1.setendkey(KeyinDisplay.KEY_DOWN);
		w1.setendkey(KeyinDisplay.KEY_LEFT);
		w1.setendkey(KeyinDisplay.KEY_RIGHT);
		if (keyinendxkeys) {
			w1.setendkey(KeyinDisplay.KEY_HOME);
			w1.setendkey(KeyinDisplay.KEY_END);
			w1.setendkey(KeyinDisplay.KEY_PGUP);
			w1.setendkey(KeyinDisplay.KEY_PGDN);
			w1.setendkey(KeyinDisplay.KEY_INSERT);
			w1.setendkey(KeyinDisplay.KEY_DELETE);
			w1.setendkey(KeyinDisplay.KEY_TAB);
			w1.setendkey(KeyinDisplay.KEY_BKTAB);
			w1.setendkey(KeyinDisplay.KEY_ESCAPE);
		}
		if (keyinfkeyshift) {
			for (i1 = KeyinDisplay.KEY_SHIFTF1; i1 <= KeyinDisplay.KEY_SHIFTF10; i1++) w1.setendkey((char) i1);
		}
		return w1;
	}
	
	/**
	 * dovidrestore
	 *
	 */
	int dovidrestore(int type, String buffer)
	{
		int i1, i2, i3, attribflag, bottom, cols, dblflag;
		int left, len, repeat, right, rows, top, pos, size, ptr;
		char[] statesize;
		char chr, rptchar = 0;
		
		dbloffset = 0;	
		size = buffer.length();
		statesize = new char[KeyinDisplay.statesize()];
		w1.statesave(statesize);
		
		if (type == 0x01) { // scrn
			w1.resetsw(); 
			cols = w1.getwinright();
			rows = w1.getwinbottom();
		}
		else { // win
			top = w1.getwintop();
			bottom = w1.getwinbottom();
			left = w1.getwinleft();
			right = w1.getwinright();
			cols = right - left + 1;
			rows = bottom - top + 1;
		}
		if (type != 0x03) { // !char
			w1.alloff(); 
			w1.cursoroff();
		}
		attribflag = 0;
		dblflag = 0;
		pos = ptr = len = repeat = 0;
		for (i1 = 0; i1 < rows && (size > 0 || repeat > 0); i1++) {
			w1.h(1);
			w1.v(i1 + 1);
			for (i2 = 0; i2 < cols; ) {
				if (repeat == 0) {  // normally this will be true unless repeat span lines 
					if (len > 0 && (i2 + len == cols || size == 0 || buffer.charAt(pos) == '~' || buffer.charAt(pos) == '^' || buffer.charAt(pos) == '!' || buffer.charAt(pos) == '`' || buffer.charAt(pos) == '@')) {
						// display string
						if (len == 1) rptchar = buffer.charAt(ptr);
						String s1 = buffer.substring(ptr, ptr + len);
						if (latintransflag) {
							w1.display(IncomingXMLParser.maptranslate(true, s1.toCharArray(), s1.length()), 0, s1.length());
						}
						else w1.display(s1);
						i2 += len;
						len = 0;
						continue;
					}
					if (size >= 2 && (buffer.charAt(pos) == '~' || buffer.charAt(pos) == '^' || buffer.charAt(pos) == '!')) {
						if (buffer.charAt(pos) == '~') {
							i3 = buffer.charAt(pos + 1) - '?';
							attribflag ^= i3;
							if ((attribflag & DSPATTR_REV) > 0) {
								if ((i3 & DSPATTR_REV) > 0) w1.revon();
								else w1.revoff();
							}
							if ((attribflag & DSPATTR_BOLD) > 0) {
								if ((i3 & DSPATTR_BOLD) > 0) w1.boldon();
								else w1.boldoff();
							}
							if ((attribflag & DSPATTR_UNDERLINE) > 0) {
								if ((i3 & DSPATTR_UNDERLINE) > 0) w1.underlineon();
								else w1.underlineoff();
							}
							if ((attribflag & DSPATTR_BLINK) > 0) {
								if ((i3 & DSPATTR_BLINK) > 0) w1.blinkon();
								else w1.blinkoff();
							}
							attribflag = i3;
							pos += 2;
							size -= 2;
						}
						else if (buffer.charAt(pos) == '^') {
							if (Character.isDigit(buffer.charAt(pos + 1))) {
								int clr = 0, i4 = 1;
								while (Character.isDigit(buffer.charAt(pos + i4))) {
									clr = clr * 10 + buffer.charAt(pos + i4) - '0';
									i4++;
								}
								w1.setcolor(clr);
								pos += i4;
								size -= i4;
							}
							else {
								w1.setcolor(buffer.charAt(pos + 1) - '?');
								pos += 2;
								size -= 2;
							}
						}
						else if (buffer.charAt(pos) == '!') {
							if (Character.isDigit(buffer.charAt(pos + 1))) {
								int clr = 0, i4 = 1;
								while (Character.isDigit(buffer.charAt(pos + i4))) {
									clr = clr * 10 + buffer.charAt(pos + i4) - '0';
									i4++;
								}
								w1.setbgcolor(clr);
								pos += i4;
								size -= i4;
							}
							else {
								w1.setbgcolor(buffer.charAt(pos + 1) - '?');
								pos += 2;
								size -= 2;
							}
						}
						continue;
					}
					if (size >= 2 && buffer.charAt(pos) == '`') {
						repeat = buffer.charAt(pos + 1) - ' ' + 4;
						pos += 2;
						size -= 2;
					}
					if (size >= 1 && buffer.charAt(pos) == '@') {
						pos++;
						size--;
					}
					if (size <= 0) {  /* should not happen */
						repeat = 0;
						break;
					}
					if (repeat == 0 && ((attribflag & DSPATTR_GRAPHIC) == 0)) {
						if (len == 0) ptr = pos;
						len++;
						pos++;
						size--;
						continue;
					}
				}
				if ((attribflag & DSPATTR_GRAPHIC) > 0) {
					chr = (char) (buffer.charAt(pos) - '?');
					i3 = chr & 0x30;
					dblflag ^= i3;
					if ((dblflag & 0x10) > 0) {
						if ((i3 & 0x10) > 0) dbloffset |= 1;
						else dbloffset &= 2;
					}
					if ((dblflag & 0x20) > 0) {
						if ((i3 & 0x20) > 0) dbloffset |= 2;
						else dbloffset &= 1;
					}
					dblflag = i3;
					if (((chr & ~0x30) * 4 + dbloffset) >= (char) 0x3C) rptchar = '?';
					else rptchar = IncomingXMLParser.gchar[(chr & ~0x30) * 4 + dbloffset];
					if (repeat == 0) w1.display(rptchar);
				}
				else {
					rptchar = buffer.charAt(pos);
					if (repeat == 0) {		
						if (latintransflag) {
							if (rptchar >= 0x80) rptchar = IncomingXMLParser.latin1map[rptchar - 0x80];
						}					
						w1.display(rptchar);
					}
				}
				if (repeat > 0) {
					i3 = repeat;
					if (i3 > cols - i2) i3 = cols - i2;
					if (latintransflag) {
						if (rptchar >= 0x80) rptchar = IncomingXMLParser.latin1map[rptchar - 0x80];
					}	
					w1.display(rptchar, i3);
					repeat -= i3;
				}
				else i3 = 1;
				if (repeat == 0) {
					pos++;
					size--;
				}
				i2 += i3;
			}
		}
		w1.staterestore(statesize, statesize.length);
		return 0;
	}

	void setAnsi256ColorMode(boolean b) {
		ansi256ColorMode = b;
		if (isDebug() && ansi256ColorMode) {
			System.out.println("In KeyinDisplay, Color mode is Ansi256");
		}
	}

	boolean isAnsi256ColorMode() {
		return ansi256ColorMode;
	}
}
