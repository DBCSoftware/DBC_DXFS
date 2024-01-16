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

import java.awt.Toolkit;
import java.io.IOException;
import java.io.OutputStream;
import java.util.Objects;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CountDownLatch;

import org.ptsw.sc.Client.RotationAngleInterpretation;
import org.ptsw.sc.Client.SomethingToDo;
import org.ptsw.sc.Client.trap;
import org.ptsw.sc.Print.PrinterSupport;
import org.ptsw.sc.xml.Attribute;
import org.ptsw.sc.xml.Element;
import org.ptsw.sc.xml.XMLOutputStream;

import javafx.event.EventHandler;
import javafx.stage.WindowEvent;

public class IncomingXMLParser {

	static final int CC_HLN = 4086;
	static final int CC_VLN = 4087;
	static final int CC_CRS = 4088;
	static final int CC_ULC = 4089;
	static final int CC_URC = 4090;
	static final int CC_LLC = 4091;
	static final int CC_LRC = 4092;
	static final int CC_RTK = 4093;
	static final int CC_DTK = 4094;
	static final int CC_LTK = 4095;
	static final int CC_UTK = 4096;
	static final int CC_UPA = 4097;
	static final int CC_RTA = 4098;
	static final int CC_DNA = 4099;
	static final int CC_LFA = 4100;
	
	static final char gchar[] = { '\u2500', '\u2550', '\u2500',
			'\u2550', // HLN
			'\u2502', '\u2502', '\u2551', '\u2551', // VLN
			'\u253c', '\u256a', '\u256b', '\u256c', // CRS
			'\u250c', '\u2552', '\u2553', '\u2554', // ULC
			'\u2510', '\u2555', '\u2556', '\u2557', // URC
			'\u2514', '\u2558', '\u2559', '\u255a', // LLC
			'\u2518', '\u255b', '\u255c', '\u255d', // LRC
			'\u251c', '\u255e', '\u255f', '\u2560', // RTK
			'\u252c', '\u2564', '\u2565', '\u2566', // DTK
			'\u2524', '\u2561', '\u2562', '\u2563', // LTK
			'\u2534', '\u2567', '\u2568', '\u2569', // UTK
			'\u2191', '\u2191', '\u21d1', '\u21d1', // UPA
			'\u2192', '\u21d2', '\u2192', '\u21d2', // RTA
			'\u2193', '\u2193', '\u21d3', '\u21d3', // DNA
			'\u2190', '\u21d0', '\u2190', '\u21d0' // LFA
	};



	/**
	 * table to translate from PC BIOS characters to Latin 1
	 */
	static final char latin1map[] = { 
	0xC7, 0xFC, 0xE9, 0xE2, 0xE4, 0xE0, 0xE5, 0xE7,
	0xEA,
	0xEB,
	'?',
	0xEF,
	0xEE,
	0xEC,
	0xC4,
	0xC5, // 0x80 - 0x8F
	0xC9, 0xE6, 0xC6, 0xF4, 0xF6, 0xF2, 0xFB, 0xF9, 0xFF,
	0xD6,
	0xDC,
	0xA2,
	0xA3,
	0xA5,
	'?',
	'?', // 0x90 - 0x9F
	0xE1, 0xED, 0xF3, 0xFA, 0xF1, 0xD1, 0xAA, 0xBA, 0xBF, '?',
	0xAC,
	0xBD,
	0xBC,
	0xA1,
	0xAB,
	0xBB, // 0xA0 - 0xAF
	'?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
	'?',
	'?',
	'?',
	'?',
	'?', // 0xB0 - 0xBF
	'?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
	'?',
	'?',
	'?',
	'?', // 0xC0 - 0xCF
	'?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
	'?',
	'?',
	'?', // 0xD0 - 0xDF
	'?', 0xDF, '?', '?', '?', '?', 0xB5, '?', '?', '?', '?', '?', '?',
	0xF8, '?',
	'?', // 0xE0 - 0xEF
	'?', 0xB1, '?', '?', '?', '?', 0xF7, '?', 0xB0, '?', 0xB7, '?',
	'?', 0xB2, '?', '?' // 0xF0 - 0xFF
	};
	
	/**
	 * table to translate from Latin 1 to PC BIOS characters
	 */
	private static final char pcbiosmap[] = {
	'?', '?', '?', '?', '?', '?', '?', '?',
	'?',
	'?',
	'?',
	'?',
	'?',
	'?',
	'?',
	'?', // 0x80 - 0x8F
	'?', '?', '?', '?', '?', '?', '?', '?', '?',
	'?',
	'?',
	'?',
	'?',
	'?',
	'?',
	'?', // 0x90 - 0x9F
	'?', 0xAD, 0x9B, 0x9C, '?', 0x9D, 0x7C, '?', '?', '?',
	0xA6,
	0xAE,
	0xAA,
	0x2D,
	'?',
	'?', // 0xA0 - 0xAF
	0xF8, 0xF1, 0xFD, '?', '?', 0xE6, '?', 0xFA, '?', '?', 0xA7,
	0xAF,
	0xAC,
	0xAB,
	'?',
	0xA8, // 0xB0 - 0xBF
	'?', '?', '?', '?', 0x8E, 0x8F, 0x92, 0x80, '?', 0x90, '?', '?',
	'?',
	'?',
	'?',
	'?', // 0xC0 - 0xCF
	'?', 0xA5, '?', '?', '?', '?', 0x99, 0x78, '?', '?', '?', '?',
	0x9A, '?',
	'?',
	0xE1, // 0xD0 - 0xDF
	0x85, 0xA0, 0x83, '?', 0x84, 0x86, 0x91, 0x87, '?', 0x82, 0x88,
	0x89, 0x8D, 0xA1, 0x8C,
	0x8B, // 0xE0 - 0xEF
	'?', 0xA4, 0x95, 0xA2, 0x93, '?', 0x94, 0xF6, 0xED, 0x97, 0xA3,
	0x96, 0x81, '?', '?', 0x98 // 0xF0 - 0xFF
	};


	private final BlockingQueue<Client.SomethingToDo> inOutQueue;
	private static final Element sync = new Element("s");
	private final OutputStream sout;
	private final XMLOutputStream xmlout;
	private final Client client = Client.getDefault();
	private KeyinDisplay w1;
	
	IncomingXMLParser(BlockingQueue<SomethingToDo> inOutQueue2, OutputStream sout, XMLOutputStream xmlout)
	{
		this.inOutQueue = inOutQueue2;
		this.sout = sout;
		this.xmlout = xmlout;
	}
	
	void parse(final Element element) throws IOException {
		Objects.requireNonNull(element, "element cannot be null");
		Attribute a1;
		String s1;
		Object o1;
		int i1, i2;
		
		if (Client.isDebug() && !element.name.equals("d")) {
			System.out.printf("Entering parse for '%s'\n", element.name);
		}
		element.name = element.name.intern();
		switch (element.name.charAt(0)) {
		case 'c':
			if (element.name == "change") {
				if ((element.hasAttribute(Client.WINDOW)) || (element.hasAttribute("application"))) parseDeviceChange(element);
				else parseResourceChange(element);
			}
			else if (element.name.equals("ce")) { // clearendkey
				if (element.getChildrenCount() != 0) {
					if (w1 == null) w1 = client.getW1();
					for (i1 = 0; i1 < element.getChildrenCount(); i1++) {
						o1 = element.getChild(i1);
						if (o1 instanceof String) {
							s1 = (String) o1;
							for (i2 = 0; i2 < s1.length(); i2++) w1.clearendkey(s1.charAt(i2));
						}
						else { // Element
							if (((Element) o1).name.equals("all")) {
								w1.clearendkey(KeyinDisplay.KEY_ALL);
							}
							else {
								s1 = (String) ((Element) o1).getChild(0); // assume <c>number</c>
								w1.clearendkey(translatekey(s1)); // convert to unicode and clear
							}
						}
					}
				}
			}
			else if (element.name == "close") {
				if (element.hasAttribute(Client.WINDOW)) {
					Client.CloseWindow(element);
				}
				else if (element.hasAttribute(Client.PANEL)) {
					Client.ClosePanel(element);
				}
				else if (element.hasAttribute(Client.DIALOG)) {
					Client.CloseDialog(element);
				}
				else if (element.hasAttribute(Client.ICON)) {
					Client.CloseIcon(element);
				}
				else if (element.hasAttribute(Client.IMAGE)) {
					Client.CloseImage(element);
				}
				else if (element.hasAttribute(Client.MENU)) {
					Client.CloseMenu(element);
				}
				else if (element.hasAttribute(Client.TOOLBAR)) {
					Client.CloseToolbar(element);
				}
				else if (element.hasAttribute(Client.OPENFDLG)) {
					Client.CloseOpenFileDialog(element);
				}
				else if (element.hasAttribute(Client.OPENDDLG)) {
					Client.CloseOpenDirDialog(element);
				}
				else if (element.hasAttribute(Client.SAVEFDLG)) {
					Client.CloseSaveFileDialog(element);
				}
				else if (element.hasAttribute(Client.ALERTDLG)) {
					Client.CloseAlertDialog(element);
				}
				else if (element.hasAttribute(Client.COLORDLG)) {
					Client.CloseColorDialog(element);
				}
				else if (element.hasAttribute(Client.FONTDLG)) {
					Client.CloseFontDialog(element);
				}
			}
			else if (element.name == FileTransferSupport.FILETRANSFERCLIENTGETELEMENTTAG
					|| element.name == FileTransferSupport.FILETRANSFERCLIENTPUTELEMENTTAG) {
				FileTransferSupport.ClientServerFileTransfer(element);
			}
			break;
		case 'd':
			if (element.name.length() == 1) { // display
				
				if (element.getChildrenCount() != 0) {
					if (w1 == null) {
						// make sure <d><b/></d> does not cause console window creation
						if (!(element.getChildrenCount() == 1 && 
								(element.getChild(0) instanceof Element) &&  
								((Element) element.getChild(0)).name.equals("b"))) 
						{
							w1 = client.getW1();
						}
					}
					for (i1 = 0; i1 < element.getChildrenCount(); i1++) {			
						o1 = element.getChild(i1);
						if (o1 instanceof String) {
							if (client.latintransflag) {
								w1.display(maptranslate(false, ((String)o1).toCharArray(), ((String)o1).length()), 0, ((String) o1).length());
							}
							else w1.display((String) o1);
							continue;
						}
						doviddisplaycc((Element) o1);
					}
				}
				
				for (Object e2 : element.getChildren()) {
					if (e2 instanceof Element && ((Element)e2).name.equals("b")){
						SCWindow.dftk.beep();
					}
				}
			}
			else if (element.name == "draw") {
				if (element.hasAttribute(Client.IMAGE)) {
					Client.DrawOnImage(element);
				}
			}
			break;
		case 'g':
			if (element.name == "getimagebits") {
				Client.GetImageBits(element);
			}
			else if (element.name == "getclipboard") {
				if (element.hasAttribute("text")) 
					Client.getClipboard();
				else if (element.hasAttribute("image")) {
					String imageName = element.getAttributeValue("image");
					ImageResource ires = Client.GetImage(imageName);
					if (ires != null) Client.getClipboard(ires);
				}
			}
			else if (element.name == "getwindow") {
				if (w1 == null) w1 = client.getW1();
				Element e2 = new Element("getwindow");
				e2.addAttribute(new Attribute("t", String.valueOf(w1.getwintop() - 1)));
				e2.addAttribute(new Attribute("b", String.valueOf(w1.getwinbottom() - 1)));
				e2.addAttribute(new Attribute("l", String.valueOf(w1.getwinleft() - 1)));
				e2.addAttribute(new Attribute("r", String.valueOf(w1.getwinright() - 1)));
				ClientUtils.SendElement(sout, xmlout, e2);
			}
			break;
		case 'h':
			if (element.name == "hide") {
				if (element.hasAttribute(Client.PANEL)) {
					Client.HidePanel(element);
				}
				else if (element.hasAttribute(Client.DIALOG)) {
					Client.HideDialog(element);
				}
				else if (element.hasAttribute(Client.IMAGE)) {
					Client.HideImage(element);
				}
				else if (element.hasAttribute(Client.MENU)) {
					Client.HideMenu(element);
				}
				else if (element.hasAttribute(Client.TOOLBAR)) {
					Client.HideToolbar(element);
				}
				else if (element.hasAttribute(Client.OPENFDLG)
						|| element.hasAttribute(Client.OPENDDLG)
						|| element.hasAttribute(Client.SAVEFDLG)
						|| element.hasAttribute(Client.ALERTDLG)
						|| element.hasAttribute(Client.FONTDLG)
						|| element.hasAttribute(Client.COLORDLG))
				{
					/* Do Nothing, this command is ignored */
					Client.sendRMessageNoError();
				}
				else Client.sendRMessageNoError();
			}
			break;
		case 'k':
			if (element.name.length() == 1) { // keyin
				int i3, width, right, len, lastendkey;
				width = right = len = 0;
				if (element.getChildrenCount() != 0) {
					if (w1 == null) w1 = client.getW1();
					for (i1 = 0; i1 < element.getChildrenCount(); i1++) {			
						o1 = element.getChild(i1);
						if (o1 instanceof String) {
							if (client.latintransflag) {
								w1.display(maptranslate(false, ((String)o1).toCharArray(), ((String)o1).length()), 0, ((String) o1).length());
							}
							else w1.display((String) o1);
							continue;
						}
						Element e1 = (Element) o1;
						if (e1.name.equals("cf") || e1.name.equals("cn")) {
							Element result = new Element("r");
							s1 = null;
							i2 = i3 = width = 0;
							
							if ((a1 = e1.getAttribute("w")) != null) {
								width = Integer.parseInt(a1.value);
							}
							if ((a1 = e1.getAttribute("r")) != null) {
								right = Integer.parseInt(a1.value);
							}
							if ((a1 = e1.getAttribute("kl")) != null) {
								w1.keyinlength(Integer.parseInt(a1.value));
							}
							if ((a1 = e1.getAttribute("edit")) != null) {
								w1.edit();
							}
							if ((a1 = e1.getAttribute("de")) != null) {
								w1.digitentry();
							}
							
							if (e1.getChildrenCount() != 0) {
								o1 = e1.getChild(0);
								if (o1 instanceof String) s1 = (String) o1;
								else s1 = (String) ((Element) o1).getChild(0); // <c>number</c>
								if (s1 == null) i3 = 0;
								else {
									i3 = s1.length();
									if (client.latintransflag) {
										s1 = new String(maptranslate(false, s1.toCharArray(), i3));
									}
								}
							}
							else i3 = 0;
							
							if (width == 0) continue;
							
							// Numeric entry		
							if (e1.name.equals("cn")) { 
								if (w1.isedit()) {
									for (i2 = 0; i2 < width; i2++) {
										if (s1 != null && i2 < i3) client.keyinchars[i2] = s1.charAt(i2);
										else client.keyinchars[i2] = ' ';
									}
								}
								len = w1.numkeyin(client.keyinchars, width, right);
								if (len > 0) { 
									if (client.latintransflag) maptranslate(true, client.keyinchars, len);
									if (client.dcflag) s1 = new String(client.keyinchars, 0, len).replace('.', ',');
									else s1 = new String(client.keyinchars, 0, len);
									result.addString(s1); 
								}
							}
							
							// Character entry
							else { 
								char[] tmp = new char[width];
								if (w1.isedit()) {
									for (i2 = 0; i2 < width; i2++) {
										if (s1 != null && i2 < i3) tmp[i2] = s1.charAt(i2);
										else tmp[i2] = ' ';
									}
								}
								len = w1.charkeyin(tmp, width);
								if (len > 0) {
									i3 = width - 1;
									for (i2 = len; i2 < width; tmp[i2++] = ' ');
									if (client.latintransflag) maptranslate(true, tmp, len);
									s1 = String.copyValueOf(tmp, 0, len);
									if (len > 0) result.addString(s1);
								}
							}

							i2 = w1.getaction();

							if (i2 == 0) {
								lastendkey = w1.getendkey();
								if (lastendkey != KeyinDisplay.KEY_TIMEOUT && lastendkey != 0) {
									result.addAttribute(new Attribute("e", Client.translatekey(lastendkey)));
								}
								else if (lastendkey == KeyinDisplay.KEY_TIMEOUT) {
									result.addAttribute(new Attribute("e", "0"));
								}
								ClientUtils.SendElement(sout, xmlout, result);
							}
							else if (i2 == -1) {
								// break
								ClientUtils.SendElement(sout, xmlout, result);
								ClientUtils.SendElement(sout, xmlout, new Element("break")); 
								return;
							}
							else if (i2 == -2) {
								// interrupt
								result.addAttribute(new Attribute("e", Client.translatekey(KeyinDisplay.KEY_INTERRUPT)));
								ClientUtils.SendElement(sout, xmlout, result);
							}
							else { 
								// trap
								Element t = new Element("t");
								t.addString(Client.translatekey(i2));
								ClientUtils.SendElement(sout, xmlout, t);
								ClientUtils.SendElement(sout, xmlout, result);
							}

							w1.digitentryoff(); // make certain digit entry is turned off
						}
						else if (e1.name.equals("dcon")) {
							client.dcflag = true;
							w1.dcon();
						}
						else if (e1.name.equals("dcoff")) {
							client.dcflag = false;
							w1.dcoff();
						}
						else if (e1.name.equals("editon")) {
							w1.editon();
						}
						else if (e1.name.equals("editoff")) {
							w1.editoff();
						}
						else if (e1.name.equals("eon")) w1.echoon();
						else if (e1.name.equals("eoff")) w1.echooff();
						else if (e1.name.equals("eson")) w1.echosecreton();
						else if (e1.name.equals("esoff")) w1.echosecretoff();
						else if (e1.name.equals("eschar")) {
							if ((s1 = (String) e1.getChild(0)) != null) {
								w1.echosecretchar(s1.charAt(0));
							}
						}
						else if (e1.name.equals("uc")) {
							w1.uppercase();
						}
						else if (e1.name.equals("lc")) {
							w1.lowercase();
						}
						else if (e1.name.equals("it")) {
							w1.invertcase();
						}
						else if (e1.name.equals("in")) {
							w1.normalcase();
						}
						else if (e1.name.equals("cl")) {
							w1.clearkeyahead();
						}
						else if (e1.name.equals("kcon")) {
							w1.autoenteron();
						}
						else if (e1.name.equals("kcoff")) {
							w1.autoenteroff();
						}
						else if (e1.name.equals("timeout")) {
							if ((a1 = e1.getAttribute("n")) != null) {
								if (Integer.parseInt(a1.value) == 65535) {
									w1.cleartimeout();
								}
								else {
									w1.timeout(Integer.parseInt(a1.value));
								}
							}
						}
						else doviddisplaycc(e1);
						// Not supported in JX:
						// else if (e1.name.equals("insmode")) { }
						// else if (e1.name.equals("ovsmode")) { }
						// else if (e1.name.equals("clickon")) { }
						// else if (e1.name.equals("clickoff")) { }
					}
					w1.cleartimeout(); // make certain that timeout is cleared
				}
			}
			break;
		case 'm':
			if (element.name == "msgbox") {
				final CountDownLatch latch = new CountDownLatch(1);
				final EventHandler<WindowEvent> evthndlr = arg0 -> latch.countDown();
				final String dialogText = element.getAttributeValue("text");
				final String dialogTitle = element.getAttributeValue("title");
				SmartClientApplication.Invoke(new Runnable() {
					@Override
					public void run() {
						Msgbox mbox1 = new Msgbox(dialogTitle, dialogText);
						mbox1.setOnCloseRequest(evthndlr);
						mbox1.setOnHiding(evthndlr);
						mbox1.show();
					}
				}, false);
				do {
					try {
						latch.await();
					} catch (InterruptedException e) { /* do nothing */ }
				} while (latch.getCount() > 0);			break;
			}
			break;
		case 'p':
			if (element.name == "prep") {
				if (element.hasAttribute(Client.WINDOW)) {
					Client.PrepWindow(element);
				}
				else if (element.hasAttribute(Client.FLOATWINDOW)) {
					Client.PrepFloatWindow(element);
				}
				else if (element.hasAttribute(Client.PANEL)) {
					Client.PrepPanel(element);
				}
				else if (element.hasAttribute(Client.ICON)) {
					Client.PrepIcon(element);
				}
				else if (element.hasAttribute(Client.DIALOG)) {
					Client.PrepDialog(element);
				}
				else if (element.hasAttribute(Client.IMAGE)) {
					Client.PrepImage(element);
				}
				else if (element.hasAttribute(Client.MENU) || element.hasAttribute(Client.POPUPMENU)) {
					Client.PrepMenu(element);
				}
				else if (element.hasAttribute(Client.TOOLBAR)) {
					Client.PrepToolbar(element);
				}
				else if (element.hasAttribute(Client.OPENFDLG)) {
					Client.PrepOpenFileDialog(element);
				}
				else if (element.hasAttribute(Client.SAVEFDLG)) {
					Client.PrepSaveFileDialog(element);
				}
				else if (element.hasAttribute(Client.OPENDDLG)) {
					Client.PrepOpenDirDialog(element);
				}
				else if (element.hasAttribute(Client.COLORDLG)) {
					Client.PrepColorDialog(element);
				}
				else if (element.hasAttribute(Client.FONTDLG)) {
					Client.PrepFontDialog(element);
				}
				else if (element.hasAttribute(Client.ALERTDLG)) {
					Client.PrepAlertDialog(element);
				}
			}
			else if (element.name.startsWith("prt"))
				PrinterSupport.parsePrinterXMLCommand(element, sout, xmlout);
			break;

		case 'q':
			if (element.name == "quit") { // terminate client
				Client.setAbort();
				Client.SomethingToDo todo = new Client.SomethingToDo(Client.SomethingToDo.Type.terminate);
				inOutQueue.add(todo);
			}
			else if (element.name == "query") {
				if (element.hasAttribute(Client.PANEL)) {
					Client.QueryPanel(element);
				}
				else if (element.hasAttribute(Client.DIALOG)) {
					Client.QueryDialog(element);
				}
				else if (element.hasAttribute(Client.MENU) || element.hasAttribute(Client.POPUPMENU)) {
					Client.QueryMenu(element);
				}
				else if (element.hasAttribute(Client.TOOLBAR)) {
					Client.QueryToolbar(element);
				}
				else if (element.hasAttribute(Client.WINDOW)) {
					Client.QueryWindow(element);
				}
				else if (element.hasAttribute(Client.CLIPBOARD)) {
					Client.QueryClipboard(element);
				}
			}
			break;
		case 'r':
			if (element.name == "rollout") {
				if (element.getChildrenCount() != 0) {
					Element r1 = new Element("r");
					s1 = (String) element.getChild(0);
					Process p1;
					try {
						String opsys = System.getProperty("os.name").trim().toLowerCase();
						if (opsys.startsWith("windows")) {
							s1 = "cmd.exe /c " + s1;
							p1 = Runtime.getRuntime().exec(s1);
						}
						else {
							String[] cmd = {"/bin/sh", "-c", s1};
							p1 = Runtime.getRuntime().exec(cmd);
						}
						
						if (p1.waitFor() != 0) r1.addAttribute(new Attribute("e", "1"));
						if (Client.isDebug()) {
							System.out.println("ClientRollout, process p1 returned " + p1.exitValue());
						}
					}
					catch (Exception ex1) {
						r1.addAttribute(new Attribute("e", "1"));
						if (Client.isDebug()) System.out.println("ClientRollout, Exception " + ex1);
					}
					ClientUtils.SendElement(sout, xmlout, r1);				
				}	
			}
		case 's':
			if (element.name == "s") {
				ClientUtils.SendElement(sout, xmlout, sync);
			}
			else if (element.name == "show") {
				processShow(element);
			}
			else if (element.name == "smartserver") {
				parseSmartserverXml(element);
				processGuiConfigOptions();
				processPrintConfigOptions();
				processJavaSCConfigOptions();
			}
			else if (element.name == "storeimagebits") {
				try {
					Client.StoreImageBits(element);
				}
				catch (Exception ex1) {
					System.err.println(ex1.getMessage());
					ex1.printStackTrace();
				}
			}
			else if (element.name == "setclipboard") {
				if (element.hasAttribute("text")) 
					Client.setClipboard(element.getAttributeValue("text"));
				else if (element.hasAttribute("image")) {
					String imageName = element.getAttributeValue("image");
					ImageResource ires = Client.GetImage(imageName);
					if (ires != null) Client.setClipboard(ires);
				}
			}
			else if (element.name.equals("se")) { // setendkey
				if (element.getChildrenCount() != 0) {
					if (w1 == null) w1 = client.getW1();
					for (i1 = 0; i1 < element.getChildrenCount(); i1++) {
						o1 = element.getChild(i1);
						if (o1 instanceof String) {
							s1 = (String) o1;
							for (i2 = 0; i2 < s1.length(); i2++) w1.setendkey(s1.charAt(i2));
						}
						else {
							s1 = (String) ((Element) o1).getChild(0); // assume <c>number</c>
							w1.setendkey(translatekey(s1));
						}
					}
				}		
			}
			else if (element.name.equals("scrnrest")) {
				client.dovidrestore(0x01, (String) element.getChild(0));
			}
			break;
		case 't':
			if (element.name.equals("ts")) { // trap
				if (element.getChildrenCount() != 0) {
					String value;
					if (w1 == null) w1 = client.getW1();
					for (int i4 = 0; i4 < element.getChildrenCount(); i4++) {
						o1 = element.getChild(i4);
						if (o1 instanceof String) {
							value = (String) o1;
							for (i1 = 0; i1 < value.length(); i1++) {
								// check to see if event already exists and remove it
								for (int i3 = 0; i3 < client.traps.size(); i3++) { 
									if (client.traps.get(i3).value == value.charAt(i1)) {
										client.traps.remove(i3);
										break;
									}
								}
								if (w1 == null) client.straps.add(Integer.valueOf(value.charAt(i1)));
								else w1.settrapkey(value.charAt(i1), true);	
								trap t1 = new Client.trap();
								t1.cval = true;
								t1.value = value.charAt(i1);
								client.traps.add(t1);
							}
						}
						else {
							value = (String) ((Element) o1).getChild(0); // assume <c>number</c>		
							i1 = Client.translatekey(value);		
							// check to see if event already exists and remove it
							for (int i3 = 0; i3 < client.traps.size(); i3++) { 
								if (client.traps.get(i3).value == i1) {
									client.traps.remove(i3);
									break;
								}
							}
							if (w1 == null) client.straps.add(Integer.valueOf(i1)); 
							else w1.settrapkey(i1, true);
							trap t1 = new trap();
							t1.value = i1;
							t1.cval = false;
							client.traps.add(t1);
						}
					}
				}
			}
			else if (element.name.equals("tc")) { // trapclr
				if (element.getChildrenCount() != 0) {
					if (w1 == null) w1 = client.getW1();
					for (int i4 = 0; i4 < element.getChildrenCount(); i4++) {
						o1 = element.getChild(i4);
						if (o1 instanceof String) {
							s1 = (String) o1;
							for (i1 = 0; i1 < s1.length(); i1++) {
								for (i2 = 0; i2 < client.traps.size(); i2++) {
									if (client.traps.get(i2).value == s1.charAt(i1)) {
										w1.cleartrapkey(s1.charAt(i1));
										client.traps.remove(i2);
										break;
									}
								}						
							}
						}
						else { // Element
							if (((Element) o1).name.equals("all")) {
								if (w1 != null) w1.cleartrapkey(-1);
								client.traps.clear();
								s1 = null;
							}
							else {
								s1 = (String) ((Element) o1).getChild(0); // assume <c>number</c>
								i1 = Client.translatekey(s1);
								for (i2 = 0; i2 < client.traps.size(); i2++) {
									if (client.traps.get(i2).value == i1) {
										w1.cleartrapkey(i1);
										client.traps.remove(i2);
										break;
									}
								}
							}
						}
					}
				}
			}
			break;
		case 'w':
			if (element.name.equals("winrest")) {
				client.dovidrestore(0x02, (String) element.getChild(0));
			}
			break;
		default:
			System.err.println("WARNING: Unknown Element '" + element.name + "' found.");
		}
	}

	private static void parseDeviceChange(Element element) throws IOException {
		assert element != null;
		if (element.hasAttribute(Client.WINDOW)) Client.ChangeWindow(element);
		else if (element.hasAttribute("application")) {
			Client.changeApplication(element);
		}
	}

	private static void parseResourceChange(Element element) throws IOException {
		assert element != null && (element.hasAttribute(Client.PANEL)
				|| element.hasAttribute(Client.DIALOG)
				|| element.hasAttribute(Client.MENU)
				|| element.hasAttribute(Client.TOOLBAR)
				|| element.hasAttribute(Client.OPENFDLG)
				|| element.hasAttribute(Client.SAVEFDLG)
				|| element.hasAttribute(Client.OPENDDLG)
				);
		if (element.hasAttribute(Client.PANEL)) Client.ChangePanel(element);
		else if (element.hasAttribute(Client.DIALOG)) Client.ChangeDialog(element);
		else if (element.hasAttribute(Client.MENU)) Client.ChangeMenu(element);
		else if (element.hasAttribute(Client.TOOLBAR)) Client.ChangeToolbar(element);
		else if (element.hasAttribute(Client.OPENFDLG)) Client.ChangeOpenFileDlg(element);
		else if (element.hasAttribute(Client.SAVEFDLG)) Client.ChangeSaveFileDlg(element);
		else if (element.hasAttribute(Client.OPENDDLG)) Client.ChangeOpenDirDlg(element);
	}

	private static void processShow(Element element) throws IOException {
		assert element != null;
		String panelName = null;
		String dialogName = null;
		String windowName = null;
		String imageName = null;
		String menuName = null;
		String toolName = null;
		String ofiledlgName = null;
		String sfiledlgName = null;
		String odirdlgName = null;
		String alrtdlgName = null;
		String colordlgName = null;
		String fontdlgName = null;
		int h = -1;
		int v = -1;
		for (Attribute a1 : element.getAttributes()) {
			switch (a1.name){
			case Client.PANEL:
				panelName = a1.value;
				break;
			case Client.DIALOG:
				dialogName = a1.value;
				break;
			case Client.WINDOW:
				windowName = a1.value;
				break;
			case Client.IMAGE:  // <show image="2" window="main0002" h="39" v="9"/>
				imageName = a1.value;
				break;
			case "h":
				h = Integer.parseInt(a1.value); // This comes over the wire adjusted to zero-based
				break;
			case "v":
				v = Integer.parseInt(a1.value); // This comes over the wire adjusted to zero-based
				break;
			case Client.MENU:
				menuName = a1.value;
				break;
			case Client.TOOLBAR:
				toolName = a1.value;
				break;
			case Client.OPENFDLG:
				ofiledlgName = a1.value;
				break;
			case Client.SAVEFDLG:
				sfiledlgName = a1.value;
				break;
			case Client.OPENDDLG:
				odirdlgName = a1.value;
				break;
			case Client.ALERTDLG:
				alrtdlgName = a1.value;
				break;
			case Client.COLORDLG:
				colordlgName = a1.value;
				break;
			case Client.FONTDLG:
				fontdlgName = a1.value;
				break;
			}
		}
		if (panelName != null && windowName != null) {
			Client.ShowPanelOnWindow(panelName, windowName, h, v);
		}
		else if (dialogName != null && windowName != null) {
			Client.ShowDialogOnWindow(dialogName, windowName, h, v);
		}
		else if (imageName != null && windowName != null) {
			Client.ShowImageOnWindow(imageName, windowName, h, v);
		}
		else if (menuName != null && windowName != null) {
			Client.ShowMenuOnWindow(menuName, windowName, h, v);
		}
		else if (toolName != null && windowName != null) {
			Client.ShowToolBarOnWindow(toolName, windowName);
		}
		else if (ofiledlgName != null && windowName != null) {
			Client.ShowOpenFileDlgOnWindow(ofiledlgName, windowName);
		}
		else if (sfiledlgName != null && windowName != null) {
			Client.ShowSaveFileDlgOnWindow(sfiledlgName, windowName);
		}
		else if (odirdlgName != null && windowName != null) {
			Client.ShowOpenDirDlgOnWindow(odirdlgName, windowName);
		}
		else if (alrtdlgName != null && windowName != null) {
			Client.ShowAlertDlgOnWindow(alrtdlgName, windowName);
		}
		else if (colordlgName != null && windowName != null) {
			Client.ShowColorDlgOnWindow(colordlgName, windowName);
		}
		else if (fontdlgName != null && windowName != null) {
			Client.ShowFontDlgOnWindow(fontdlgName, windowName);
		}
		else Client.SendResult(false);
	}
	
	/**
	 * @param element
	 * @throws NumberFormatException
	 */
	private void parseSmartserverXml(Element element) throws NumberFormatException {
		Element e1;
		Element e2;
		int i1;
		int i2;
		Object o1;
		if (element.getChildrenCount() != 0) {
			for (i1 = 0; i1 < element.getChildrenCount(); i1++) {
				if (element.getChild(i1) instanceof Element) {
					e1 = (Element) element.getChild(i1);
					if (e1.name.equals("keyin")) {
//						if ((e2 = e1.getElement("case")) != null) {
//							if (e2.getChildrenCount() != 0) {
//								if (((String) e2.getChild(0)).equals("upper")) {
//									kdswinflag |= KeyinDisplay.FLAG_KEYINUPCASE;
//								}
//								else if (((String) e2.getChild(0)).equals("reverse")) {
//									kdswinflag |= KeyinDisplay.FLAG_KEYINREVCASE;
//								}
//							}
//						}
//						if ((e2 = e1.getElement("cancelkey")) != null) {
//							if (e2.getChildrenCount() != 0) {
//								keyincancelkey = parsekeyinchar((String) e2.getChild(0));
//							}
//						}
//						if ((e2 = e1.getElement("interruptkey")) != null) {
//							if (e2.getChildrenCount() != 0) {
//								keyininterruptkey = parsekeyinchar((String) e2.getChild(0));
//							}
//						}
//						if ((e2 = e1.getElement("endkey")) != null) {
//							if (e2.getChildrenCount() != 0) {
//								if (((String) e2.getChild(0)).equals("xkeys")) {
//									keyinendxkeys = true;
//								}
//							}
//						}						
//						if ((e2 = e1.getElement("device")) != null) {
//							if (e2.getChildrenCount() != 0) {
//								keyindevice = (String) e2.getChild(0);
//							}
//						}
//						if ((e2 = e1.getElement("fkeyshift")) != null) {
//							if (e2.getChildrenCount() != 0) {
//								if (((String) e2.getChild(0)).equals("old")) {
//									keyinfkeyshift = true;
//								}
//							}
//						}
					}
					else if (e1.name.equals("display")) {
						for (i2 = 0; i2 < e1.getChildrenCount(); i2++) {
							e2 = (Element) e1.getChild(i2);
							if (e2.name.equals("autoroll")) {
								if (e2.hasChildren() && ((String) e2.getChild(0)).equals("off"))
									Client.displayautoroll = false;
							}
							else if (e2.name.equals("columns")) {
								if (e2.hasChildren()) Client.displayhsize = Integer.parseInt((String) e2.getChild(0));
							}
//							else if (e2.name.equals("console")) {
//								for (int i3 = 0; i3 < e2.getChildrenCount(); i3++) {								
//									Element e3 = (Element) e2.getChild(i3);
//									if (e3.name.equals("noclose")) {
//										if (e3.hasChildren() && ((String) e3.getChild(0)).equals("yes")) kdswinflag |= KeyinDisplay.FLAG_CONSOLENOCLOSE;
//									}
//								}
//							}
//							else if (e2.name.equals("device")) {
//								if (e2.hasChildren()) {
//									if (((String) e2.getChild(0)).equals("console")) kdswinflag |= KeyinDisplay.FLAG_DISPLAYCONSOLE;
//									else if (((String) e2.getChild(0)).equals("terminal")) kdswinflag |= KeyinDisplay.FLAG_DISPLAYTERMINAL;
//								}
//							}
//							else if (e2.name.equals("font")) {
//								if (e2.hasChildren()) displayfont = (String) e2.getChild(0);
//							}
//							else if (e2.name.equals("fontcharheight")) {
//								if (e2.hasChildren()) displayfontcharheight = Integer.parseInt((String) e2.getChild(0));
//							}
//							else if (e2.name.equals("fontcharwidth")) {
//								if (e2.hasChildren()) displayfontcharwidth = Integer.parseInt((String) e2.getChild(0));
//							}
//							else if (e2.name.equals("fontsize")) {
//								if (e2.hasChildren()) displayfontsize = Integer.parseInt((String) e2.getChild(0));
//							}
							else if (e2.name.equals("lines")) {
								if (e2.hasChildren()) Client.displayvsize = Integer.parseInt((String) e2.getChild(0));
							}
//							else if (e2.name.equals("termdef")) {
//								if (e2.hasChildren()) tdbfilename = (String) e2.getChild(0);
//							}
							else if (e2.name.equals("colormode")) {
								if (e2.hasChildren()) {
									if (((String) e2.getChild(0)).equals("ansi256")) {
										client.setAnsi256ColorMode(true);
									}
								}
							}
						}
					}
					else if (e1.name.equals("gui")) {
						for (i2 = 0; i2 < e1.getChildrenCount(); i2++) {
							o1 = e1.getChild(i2);
							if (o1 instanceof Element) Client.addProperty("gui", (Element) o1);
						}
					}
					else if (e1.name.equals(FileTransferSupport.FILETRANSFERCLIENTDIRTAGNAME)) {
						FileTransferSupport.setClientSideDirectory((String) e1.getChild(0));
					}
					else if (e1.name.equals("javasc")) {
						for (i2 = 0; i2 < e1.getChildrenCount(); i2++) {
							o1 = e1.getChild(i2);
							if (o1 instanceof Element) Client.addProperty("javasc", (Element) o1);
						}
					}
				}
			}
		}
	}

	private static void processPrintConfigOptions() {
		// Decide on print text rotation angle interpretation here
		if (Client.printTextRotationPreference == RotationAngleInterpretation.NULL) {
			String rot = (String) Client.getPrintProperty(Client.TEXTROTATE);
			if (SmartClientApplication.ServerMajorVersion < 17) {
				if (rot != null && rot.equals("new"))
					Client.printTextRotationPreference = RotationAngleInterpretation.NEW;
				else 
					Client.printTextRotationPreference = RotationAngleInterpretation.OLD;
			}
			else {
				if (rot != null && rot.equals("old"))
					Client.printTextRotationPreference = RotationAngleInterpretation.OLD;
				else
					Client.printTextRotationPreference = RotationAngleInterpretation.NEW;
			}
		}
	}
	
	private static void processJavaSCConfigOptions() {
		String value = (String) Client.getJavaSCProperty("unsupported");
		if (value != null) {
			if (value.equalsIgnoreCase("ignore")) SmartClientApplication.ignoreUnsupportedChangeCommands = true;
		}
	}
	
	private static void processGuiConfigOptions()
	{
		String value = (String) Client.getGuiProperty("minsert.separator");
		if (value != null) {
			if (Character.isDigit(value.charAt(0)))
			{
				if (value.charAt(0)== '0')
				{
					if (value.charAt(1) == 'X' || value.charAt(1) == 'x')
					{
						Client.minsertSeparator = (char) Integer.parseInt(value, 16);
					}
					else
					{
						Client.minsertSeparator = (char) Integer.parseInt(value, 8);
					}
				}
				else
				{
					Client.minsertSeparator = (char) Integer.parseInt(value);
				}
			}
			else
			{
				Client.minsertSeparator = value.charAt(0);
			}
		}
		value = (String) Client.getGuiProperty("enterkey");
		if (value != null && value.equalsIgnoreCase("old")) {
			Client.oldEnterKey = true;
		}

		String userFontName = (String) Client.getGuiProperty("ctlfont");
		if (userFontName != null) {
			// TODO DO we support this? And if so, how?
//			HashMap<TextAttribute, Object> tas = new HashMap<TextAttribute, Object>(9);
//			tas.putAll(Client.defaultFont.getAttributes());
//			String javaFontName = Client.DBCFontNames.get(userFontName.toUpperCase());
//			if (javaFontName != null) {
//				tas.put(TextAttribute.FAMILY, javaFontName);
//			} else {
//				tas.put(TextAttribute.FAMILY, userFontName);
//			}
//			Client.defaultFont = Font.getFont(tas);
		}
		
		String irot = (String) Client.getGuiProperty(Client.IMAGEROTATE);
		if (irot != null) {
			if (irot.equalsIgnoreCase("old"))
				Client.guiRotationPreference = RotationAngleInterpretation.OLD;
			else if (irot.equalsIgnoreCase("new"))
				Client.guiRotationPreference = RotationAngleInterpretation.NEW;
		}
		
		String rrggbb = (String) Client.getGuiProperty("color");
		if (rrggbb != null && rrggbb.equals("rgb")) Client.gui24BitColorIsRGB = true;
	}

	static char[] maptranslate(boolean output, char[] data, int length) {
		for (int i1 = 0; i1 < length; i1++) {
			if (data[i1] >= 0x80) {
				if (!output)
					data[i1] = latin1map[data[i1] - 0x80];
				else
					data[i1] = pcbiosmap[data[i1] - 0x80];
			}
		}
		return data; // for convenience
	}

	/*
	 * Translate DX compatable key value to unicode key value
	 */
	private static char translatekey(String value) {
		int key = Integer.parseInt(value);
		if (key == 505)
			return KeyinDisplay.KEY_INTERRUPT; // formula below would be off by
												// 1 for this
		return (char) (key + ((key >= 256) ? (int) '\uF000' : 0));
	}

	private void doviddisplaycc(Element e1) 
	{
		KeyinDisplay w1 = client.getW1();
		String s1, name;
		Attribute a1, a2, a3, a4;
		Element e2;
		
		name = e1.name.intern();
		if (name == "p") {
			if ((a1 = e1.getAttribute("h")) != null) {
				w1.h(Integer.parseInt(a1.value) + 1);
			}
			if ((a2 = e1.getAttribute("v")) != null) {
				w1.v(Integer.parseInt(a2.value) + 1);
			}					
		}
		else if (name == "h") {
			if ((s1 = (String) e1.getChild(0)) != null) {
				w1.h(Integer.parseInt(s1) + 1);
			}
		}
		else if (name == "v") {
			if ((s1 = (String) e1.getChild(0)) != null) {
				w1.v(Integer.parseInt(s1) + 1);
			}
		}
		else if (name == "ha") {
			if ((s1 = (String) e1.getChild(0)) != null) {
				w1.ha(Integer.parseInt(s1));
			}
		}
		else if (name == "va") {
			if ((s1 = (String) e1.getChild(0)) != null) {
				w1.va(Integer.parseInt(s1));
			}
		}
		else if (name == "editon") w1.editon();
		else if (name == "editoff") w1.editoff();
		else if (name == "hu") w1.homeup();
		else if (name == "hd") w1.homedown();
		else if (name == "eu") w1.endup();
		else if (name == "ed") w1.enddown();
		else if (name == "it") w1.invertcase();
		else if (name == "in") w1.normalcase();
		else if (name == "nl") {
			if (Client.displayautoroll) w1.nl();
			else {
				w1.h(1);
				w1.va(1);
			}
		}
		else if (name == "cr") w1.h(1);
		else if (name == "lf") w1.va(1);
		else if (name == "cursor") {
			if ((s1 = (String) e1.getChild(0)) != null) {
				if (s1.equals("on")) w1.cursoron();
				else if (s1.equals("off")) w1.cursoroff();
				else if (s1.equals("norm")) w1.cursornorm();
				else if (s1.equals("uline")) w1.cursoruline();
				else if (s1.equals("half")) w1.cursorhalf();
				else if (s1.equals("block")) w1.cursorblock();
			}
		}
		else if (name == "ru") {
			w1.rollup();
		}
		else if (name == "rd") {
			w1.rolldown();
		}
		else if (name == "scrright") {
			if (w1 == null) client.openkds();
			w1.rollright();
			w1.homeup();
			if (!client.flowdown) w1.flowdown();
			for (int i1 = 0; i1 < e1.getChildrenCount(); i1++) {			
				Object o1 = e1.getChild(i1);
				if (o1 instanceof String) {
					if (client.latintransflag) {
						w1.display(maptranslate(false, ((String)o1).toCharArray(), ((String)o1).length()), 0, ((String) o1).length());
					}
					else w1.display((String) o1);
					continue;
				}
				e2 = (Element) o1;
				doviddisplaycc(e2);
			}
			if (!client.flowdown) w1.flowright();
		}
		else if (name == "scrleft") {
			if (w1 == null) client.openkds();
			w1.rollleft();
			w1.endup();
			if (!client.flowdown) w1.flowdown();
			for (int i1 = 0; i1 < e1.getChildrenCount(); i1++) {			
				Object o1 = e1.getChild(i1);
				if (o1 instanceof String) {
					if (client.latintransflag) {
						w1.display(maptranslate(false, ((String)o1).toCharArray(), ((String)o1).length()), 0, ((String) o1).length());
					}
					else w1.display((String) o1);
					continue;
				}
				e2 = (Element) o1;
				doviddisplaycc(e2);
			}
			if (!client.flowdown) w1.flowright();
		}
		else if (name == "opnlin") w1.openline();
		else if (name == "clslin") w1.closeline();
		else if (name == "inslin") w1.insertline();
		else if (name == "dellin") w1.deleteline();
		else if (name == "inschr") {
			if ((a1 = e1.getAttribute("h")) != null && (a2 = e1.getAttribute("v")) != null) {
				w1.insertchar(Integer.parseInt(a1.value), Integer.parseInt(a2.value));
			}
		}
		else if (name == "delchr") {
			if ((a1 = e1.getAttribute("h")) != null && (a2 = e1.getAttribute("v")) != null) {
				w1.deletechar(Integer.parseInt(a1.value) + 1, Integer.parseInt(a2.value) + 1);
			}
		}
		else if (name == "es") w1.erase();
		else if (name == "el") w1.eraseline();
		else if (name == "ef") w1.erasefrom();
		else if (name == "rptchar") {
			if ((a1 = e1.getAttribute("n")) != null) {
				if ((e2 = e1.getElement("c")) != null) {
					if ((s1 = (String) e2.getChild(0)) != null) {		
						w1.display(gchar[Integer.parseInt(s1) * 4 + client.dbloffset], Integer.parseInt(a1.value));
					}
				}
				else if ((e2 = e1.getElement("hln")) != null) {
					w1.display(gchar[(CC_HLN - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("vln")) != null) {
					w1.display(gchar[(CC_VLN - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("crs")) != null) {
					w1.display(gchar[(CC_CRS - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("ulc")) != null) {
					w1.display(gchar[(CC_ULC - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("urc")) != null) {
					w1.display(gchar[(CC_URC - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("llc")) != null) {
					w1.display(gchar[(CC_LLC - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("lrc")) != null) {
					w1.display(gchar[(CC_LRC - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("rtk")) != null) {
					w1.display(gchar[(CC_RTK - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("dtk")) != null) {
					w1.display(gchar[(CC_DTK - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("ltk")) != null) {
					w1.display(gchar[(CC_LTK - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("utk")) != null) {
					w1.display(gchar[(CC_UTK - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("upa")) != null) {
					w1.display(gchar[(CC_UPA - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("dna")) != null) {
					w1.display(gchar[(CC_DNA - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("lfa")) != null) {
					w1.display(gchar[(CC_LFA - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("rta")) != null) {
					w1.display(gchar[(CC_RTA - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}						
				else {
					s1 = (String) e1.getChild(0);
					if (s1 != null) {
						if (s1.length() > 0) {
							if (client.latintransflag) {
								s1 = new String(maptranslate(true, s1.toCharArray(), 1));
							}
							w1.display(s1.charAt(0), Integer.parseInt(a1.value));
						}
					}
				}
			}
		}
		else if (name == "rptdown") {
			if (!client.flowdown) w1.flowdown();
			if ((a1 = e1.getAttribute("n")) != null) {
				if ((e2 = e1.getElement("c")) != null) {
					if ((s1 = (String) e2.getChild(0)) != null) {		
						w1.display(gchar[Integer.parseInt(s1) * 4 + client.dbloffset], Integer.parseInt(a1.value));
					}
				}
				else if ((e2 = e1.getElement("hln")) != null) {
					w1.display(gchar[(CC_HLN - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("vln")) != null) {
					w1.display(gchar[(CC_VLN - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("crs")) != null) {
					w1.display(gchar[(CC_CRS - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("ulc")) != null) {
					w1.display(gchar[(CC_ULC - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("urc")) != null) {
					w1.display(gchar[(CC_URC - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("llc")) != null) {
					w1.display(gchar[(CC_LLC - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("lrc")) != null) {
					w1.display(gchar[(CC_LRC - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("rtk")) != null) {
					w1.display(gchar[(CC_RTK - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("dtk")) != null) {
					w1.display(gchar[(CC_DTK - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("ltk")) != null) {
					w1.display(gchar[(CC_LTK - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("utk")) != null) {
					w1.display(gchar[(CC_UTK - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("upa")) != null) {
					w1.display(gchar[(CC_UPA - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("dna")) != null) {
					w1.display(gchar[(CC_DNA - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("lfa")) != null) {
					w1.display(gchar[(CC_LFA - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}
				else if ((e2 = e1.getElement("rta")) != null) {
					w1.display(gchar[(CC_RTA - CC_HLN) * 4 + client.dbloffset], Integer.parseInt(a1.value));
				}						
				else {
					s1 = (String) e1.getChild(0);
					if (s1 != null) {
						if (s1.length() > 0) {
							if (client.latintransflag) {
								s1 = new String(maptranslate(true, s1.toCharArray(), 1));
							}
							w1.display(s1.charAt(0), Integer.parseInt(a1.value));
						}
					}
				}
			}
			if (!client.flowdown) w1.flowright();
		}
		else if (name == "b") Toolkit.getDefaultToolkit().beep();
		else if (name == "revon") w1.revon();
		else if (name == "revoff") w1.revoff();
		else if (name == "boldon") w1.boldon();
		else if (name == "boldoff") w1.boldoff();
		else if (name == "alloff") w1.alloff();
		else if (name == "blinkon") w1.blinkon();
		else if (name == "blinkoff") w1.blinkoff();
		else if (name == "ulon") w1.underlineon();
		else if (name == "uloff") w1.underlineoff();
		
		else if (name == "black" || name == "blue" || name == "green" || name == "magenta"
				|| name == "red" || name == "cyan" || name == "yellow" || name == "white") {
			w1.setcolor(name);
		}
		else if (name == "color") {
			if ((a1 = e1.getAttribute("v")) != null) w1.setcolor(Integer.parseInt(a1.value));
		}
		
		else if (name == "bgblack" || name == "bgblue" || name == "bggreen" || name == "bgmagenta"
				|| name == "bgred" || name == "bgcyan" || name == "bgyellow" || name == "bgwhite") {
			w1.setbgcolor(name);
		}
		else if (name == "bgcolor") {
			if ((a1 = e1.getAttribute("v")) != null) w1.setbgcolor(Integer.parseInt(a1.value));
		}
		
		else if (name == "dblon") client.dbloffset = 3;
		else if (name == "dbloff") client.dbloffset = 0;
		else if (name == "hdblon") client.dbloffset |= 1;
		else if (name == "hdbloff") client.dbloffset &= 2;
		else if (name == "vdblon") client.dbloffset |= 2;	
		else if (name == "vdbloff") client.dbloffset &= 1;
		else if (name == "hln") {
			w1.display(gchar[(CC_HLN - CC_HLN) * 4 + client.dbloffset]);
		}
		else if (name == "vln") {
			w1.display(gchar[(CC_VLN - CC_HLN) * 4 + client.dbloffset]);
		}
		else if (name == "crs") {
			w1.display(gchar[(CC_CRS - CC_HLN) * 4 + client.dbloffset]);
		}
		else if (name == "ulc") {
			w1.display(gchar[(CC_ULC - CC_HLN) * 4 + client.dbloffset]);
		}
		else if (name == "urc") {
			w1.display(gchar[(CC_URC - CC_HLN) * 4 + client.dbloffset]);
		}
		else if (name == "llc") {
			w1.display(gchar[(CC_LLC - CC_HLN) * 4 + client.dbloffset]);
		}
		else if (name == "lrc") {
			w1.display(gchar[(CC_LRC - CC_HLN) * 4 + client.dbloffset]);
		}
		else if (name == "rtk") {
			w1.display(gchar[(CC_RTK - CC_HLN) * 4 + client.dbloffset]);
		}
		else if (name == "dtk") {
			w1.display(gchar[(CC_DTK - CC_HLN) * 4 + client.dbloffset]);
		}
		else if (name == "ltk") {
			w1.display(gchar[(CC_LTK - CC_HLN) * 4 + client.dbloffset]);
		}
		else if (name == "utk") {
			w1.display(gchar[(CC_UTK - CC_HLN) * 4 + client.dbloffset]);
		}
		else if (name == "upa") {
			w1.display(gchar[(CC_UPA - CC_HLN) * 4 + client.dbloffset]);
		}
		else if (name == "dna") {
			w1.display(gchar[(CC_DNA - CC_HLN) * 4 + client.dbloffset]);
		}
		else if (name == "lfa") {
			w1.display(gchar[(CC_LFA - CC_HLN) * 4 + client.dbloffset]);
		}
		else if (name == "rta") {
			w1.display(gchar[(CC_RTA - CC_HLN) * 4 + client.dbloffset]);
		}
		else if (name == "setsw") {
			a1 = e1.getAttribute("t");
			a2 = e1.getAttribute("b");
			a3 = e1.getAttribute("l");
			a4 = e1.getAttribute("r");
			
			if (a1 != null && a2 != null) {
				w1.setswtb(Integer.parseInt(a1.value) + 1, Integer.parseInt(a2.value) + 1);
			}
			else if (a1 != null && a2 == null) {
				int i1 = w1.getwinbottom();
				if (Integer.parseInt(a1.value) + 1 > i1) i1 = Integer.parseInt(a1.value) + 1;
				w1.setswtb(Integer.parseInt(a1.value) + 1, i1);
			}
			else if (a1 == null && a2 != null) {
				w1.setswtb(w1.getwintop(), Integer.parseInt(a2.value) + 1);
			}
			
			if (a3 != null && a4 != null) {
				w1.setswlr(Integer.parseInt(a3.value) + 1, Integer.parseInt(a4.value) + 1);
			}
			else if (a3 != null && a4 == null) {
				w1.setswlr(Integer.parseInt(a3.value) + 1, w1.getwinleft());
			}
			else if (a3 == null && a4 != null) {
				w1.setswlr(w1.getwinright(), Integer.parseInt(a4.value) + 1);
			}
		}
		else if (name == "resetsw") w1.resetsw();
		else if (name == "wait") {
			if ((a1 = e1.getAttribute("n")) != null) {
				try {
					// This is the way JX does this... 
					// it doesn't allow a trap to get triggered.
					Thread.sleep(Integer.parseInt(a1.value) * 1000); 
				}
				catch (InterruptedException e) { }
			}
		}
		else if (name == "autorollon") {
			Client.displayautoroll = true;
		}
		else if (name == "autorolloff") {
			Client.displayautoroll = false;
		}
		// Not supported in kds:
		// else if (e1.name.equals("click")) { }
		// else if (e1.name.equals("rawon")) { }
		// else if (e1.name.equals("rawoff")) { }
	}			


}
