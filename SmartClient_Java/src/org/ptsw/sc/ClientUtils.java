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
 * *
 */

package org.ptsw.sc;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.Socket;
import java.util.Date;
import java.util.HashMap;
import java.util.Map;
import java.util.TimeZone;

import org.ptsw.sc.ListDrop.ListDropItem;
import org.ptsw.sc.xml.Attribute;
import org.ptsw.sc.xml.Element;
import org.ptsw.sc.xml.XMLInputStream;
import org.ptsw.sc.xml.XMLOutputStream;

import javafx.collections.ObservableList;
import javafx.scene.input.KeyCode;
import javafx.scene.text.Font;
import javafx.scene.text.FontPosture;
import javafx.scene.text.FontWeight;

public final class ClientUtils {

	public static final Map<KeyCode, Integer> FKeyMap = new HashMap<>(32);
	static final Map<KeyCode, Integer> LetterKeyMap = new HashMap<>(32);
	static {
		FKeyMap.put(KeyCode.ENTER, 256);
		FKeyMap.put(KeyCode.ESCAPE, 257);
		FKeyMap.put(KeyCode.BACK_SPACE, 258);
		FKeyMap.put(KeyCode.TAB, 259); // Shift-TAB is 260
		FKeyMap.put(KeyCode.UP, 261);
		FKeyMap.put(KeyCode.DOWN, 262);
		FKeyMap.put(KeyCode.LEFT, 263);
		FKeyMap.put(KeyCode.RIGHT, 264);
		FKeyMap.put(KeyCode.INSERT, 265);
		FKeyMap.put(KeyCode.DELETE, 266);
		FKeyMap.put(KeyCode.HOME, 267);
		FKeyMap.put(KeyCode.END, 268);
		FKeyMap.put(KeyCode.PAGE_UP, 269);
		FKeyMap.put(KeyCode.PAGE_DOWN, 270);
		FKeyMap.put(KeyCode.F1, 301);
		FKeyMap.put(KeyCode.F2, 302);
		FKeyMap.put(KeyCode.F3, 303);
		FKeyMap.put(KeyCode.F4, 304);
		FKeyMap.put(KeyCode.F5, 305);
		FKeyMap.put(KeyCode.F6, 306);
		FKeyMap.put(KeyCode.F7, 307);
		FKeyMap.put(KeyCode.F8, 308);
		FKeyMap.put(KeyCode.F9, 309);
		FKeyMap.put(KeyCode.F10, 310);
		FKeyMap.put(KeyCode.F11, 311);
		FKeyMap.put(KeyCode.F12, 312);
		FKeyMap.put(KeyCode.ESCAPE, 257);
		
		//
		// This is used for NKEY Alt-LetterKeys from an empty Window 
		// (See page 106 in the dx16 ref)
		// The bias for the Alt key is already built in
		//
		LetterKeyMap.put(KeyCode.A, 381);
		LetterKeyMap.put(KeyCode.B, 382);
		LetterKeyMap.put(KeyCode.C, 383);
		LetterKeyMap.put(KeyCode.D, 384);
		LetterKeyMap.put(KeyCode.E, 385);
		LetterKeyMap.put(KeyCode.F, 386);
		LetterKeyMap.put(KeyCode.G, 387);
		LetterKeyMap.put(KeyCode.H, 388);
		LetterKeyMap.put(KeyCode.I, 389);
		LetterKeyMap.put(KeyCode.J, 390);
		LetterKeyMap.put(KeyCode.K, 391);
		LetterKeyMap.put(KeyCode.L, 392);
		LetterKeyMap.put(KeyCode.M, 393);
		LetterKeyMap.put(KeyCode.N, 394);
		LetterKeyMap.put(KeyCode.O, 395);
		LetterKeyMap.put(KeyCode.P, 396);
		LetterKeyMap.put(KeyCode.Q, 397);
		LetterKeyMap.put(KeyCode.R, 398);
		LetterKeyMap.put(KeyCode.S, 399);
		LetterKeyMap.put(KeyCode.T, 400);
		LetterKeyMap.put(KeyCode.U, 401);
		LetterKeyMap.put(KeyCode.V, 402);
		LetterKeyMap.put(KeyCode.W, 403);
		LetterKeyMap.put(KeyCode.X, 404);
		LetterKeyMap.put(KeyCode.Y, 405);
		LetterKeyMap.put(KeyCode.Z, 406);
	}
	

	/**
	 * Making this private makes sure an instance cannot be instanciated
	 */
	private ClientUtils() {}
	
	/**
	 * sendElement
	 *
	 * @param element  The XML Element to send to the server.
	 *
	 */
	public synchronized static void SendElement(OutputStream sout, XMLOutputStream xmlout, Element element) throws IOException
	{
		String spaces = "        ";
		String rawString = element.toStringRaw();
		String header = String.valueOf(rawString.length());  	
		try {
			sout.write(Client.getDefault().getSyncstr().getBytes(), 0, 8); 
			sout.write((spaces.substring(header.length()) + header).getBytes(), 0, 8);
			xmlout.writeElement(element);
	  	}
		catch (Exception e0) {
			if (!Client.getAbortflag()) {
				throw new IOException();
			}
		}
		if (SmartClientApplication.isTraceoutbound()) {
			if (SmartClientApplication.debugSuppressOutboundTraceWSIZ && rawString.startsWith("<wsiz")) return;
			if (SmartClientApplication.debugSuppressOutboundTraceWPOS && rawString.startsWith("<wpos")) return;
			if (!rawString.startsWith("<alivechk")) {
				System.out.println("outbuf: " + rawString);
				System.out.flush();
			}
		}
	}


	/**
	 * Sends an XML message to the server on Socket s1 and
	 * waits for a message to be returned.
	 * 
	 * @param s1 The open Socket to use
	 * @param xmldata The data to send
	 * @return An Element returned by the server. NULL if failure.
	 */
	static Element SendElement(Socket s1, String xmldata)
	{
		int i1;
		byte[] b;
		String header;
		StringBuilder length;
		XMLInputStream xmlin2;
		Element result = null;
		length = new StringBuilder(8);
		header = String.valueOf(xmldata.length());	
		try {
			while (header.length() != 8) header = ' ' + header;
			s1.getOutputStream().write("        ".getBytes());
			s1.getOutputStream().write(header.getBytes());
			s1.getOutputStream().write(xmldata.toString().getBytes());
			for (i1 = 0; i1 < 16; i1++) {
				if (i1 > 7) length.append((char) s1.getInputStream().read());
				else s1.getInputStream().read();
			}
			b = new byte[Integer.parseInt(length.toString().trim())];
			i1 = 0;
			do {
				i1 += s1.getInputStream().read(b, i1, b.length - i1);
			}
			while (i1 != b.length);
			xmlin2 = new XMLInputStream(new ByteArrayInputStream(b)); 			
			try {
				result = xmlin2.readElement(); // read server return message
				xmlin2.close();
			}
			catch (Exception e0) {
				if (Client.IsTrace() || Client.isDebug()) {
					e0.printStackTrace();
					System.err.println("ERROR: Failure reading XML " + (new String(b)).trim());
				}
				xmlin2.close();
				return null;
			}
		}
		catch (IOException e1) {
			if (Client.IsTrace() || Client.isDebug()) {
				e1.printStackTrace();
				System.err.println("ERROR: Couldn't send XML to server.");
			}
			return null;
		}
		if (SmartClientApplication.isTraceoutbound()) {
			if (!xmldata.startsWith("<alivechk")) {
				System.out.println("outbuf: " + xmldata);
				System.out.flush();
			}
		}
		if (Client.IsTrace()) {
			System.out.println("inbuf: " + result.toStringRaw());
			System.out.flush();
		}
		return result;
	}

	static Attribute getUtcOffsetAttribute() {
		int seconds = TimeZone.getDefault().getOffset((new Date()).getTime()) / 1000;
		char sign = (seconds < 0) ? '-' : '+';
		int minutes = Math.abs(seconds) / 60;
		int residualMinutes = minutes % 60;
		int hours = (minutes - residualMinutes) / 60;
		return new Attribute("utcoffset", sign + String.format("%02d%02d", hours, residualMinutes));
	}

	/**
	 * @param textToFind
	 * @return Zero-based index of matching item, -1 if not found
	 */
	public static int findMatchingText(ObservableList<ListDropItem> theList, String textToFind) 
	{
		for (int i1 = 0; i1 < theList.size(); i1++) {
			if (textToFind.compareTo(theList.get(i1).getText()) == 0) return i1;
		}
		return -1;
	}

	public static Font applyDBCStyle(Font oldFont, String style, String item) throws Exception {
		String family = oldFont.getFamily();
		double size = oldFont.getSize();
		Font newFont = null;
		switch (style) {
		case "BOLD":
			newFont = Font.font(family, FontWeight.BOLD, FontPosture.REGULAR, size);
			break;
		case "BOLDITALIC":
			newFont = Font.font(family, FontWeight.BOLD, FontPosture.ITALIC, size);
			break;
		case "ITALIC":
			newFont = Font.font(family, FontWeight.NORMAL, FontPosture.ITALIC, size);
			break;
		case "PLAIN":
			newFont = Font.font(family, FontWeight.NORMAL, FontPosture.REGULAR, size);
			break;
		default:
			String s1 = String.format("Invalid style, %s, for item %s", style, item);
			throw new Exception(s1);
		}
		return newFont;
	}

}
