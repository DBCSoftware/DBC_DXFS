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

package org.ptsw.sc.Print;

import java.awt.geom.Point2D;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.ptsw.sc.Client;
import org.ptsw.sc.ClientUtils;
import org.ptsw.sc.xml.Attribute;
import org.ptsw.sc.xml.Element;
import org.ptsw.sc.xml.XMLOutputStream;

import javafx.print.Paper;
import javafx.print.PaperSource;
import javafx.print.Printer;
import javafx.scene.shape.Rectangle;

@SuppressWarnings("serial")
class SCPrintException extends Exception {
	private final PrinterSupport.PRTERR errorCode;
	SCPrintException(PrinterSupport.PRTERR errorCode, String message) {
		super(message);
		this.errorCode = errorCode;
	}
	/**
	 * @return the errorCode
	 */
	PrinterSupport.PRTERR getErrorCode() {
		return errorCode;
	}
}

public class PrinterSupport {

	/**
	 * These are printer error codes that mirror the ones defined in the runtime C code.
	 * Only a few of them are used here.
	 * I built the entire list just to be complete.
	 */
	public enum PRTERR {
		INUSE, EXIST, OFFLINE, CANCEL, BADNAME,
		BADOPT, NOTOPEN, OPEN, CREATE, ACCESS,
		WRITE, NOSPACE, NOMEM, SERVER, DIALOG,
		NATIVE, BADTRANS, TOOLONG,
		SPLCLOSE, UNSUPCLNT;
		
		@Override
		public String toString() {
			switch (this) {
			case ACCESS: return "access";
			case BADNAME: return "badname";
			case BADOPT: return "badopt";
			case BADTRANS: return "badtrans";
			case CANCEL: return "cancel";
			case CREATE: return "create";
			case DIALOG: return "dialog";
			case EXIST: return "exist";
			case INUSE: return "inuse";
			case NATIVE: return "native";
			case NOMEM: return "nomem";
			case NOSPACE: return "nospace";
			case NOTOPEN: return "notopen";
			case OFFLINE: return "offline";
			case OPEN: return "open";
			case SERVER: return "server";
			case SPLCLOSE: return "splclose";
			case TOOLONG: return "toolong";
			case UNSUPCLNT: return "unsupported";
			case WRITE: return "write";
			default: return "client";
			}
		}
	}
	
	private static final Map<Integer, PrintJob> openPrinters = new HashMap<>();
	private static final Element simpleResponse = new Element("r");
	
	private static List<String> getPrinters() {
		List<String> list = new ArrayList<>();
		for (Printer printer : Printer.getAllPrinters()) {
			list.add(printer.getName());
		}
		return list;
	}

	private static List<String> getBins(String printer) {
		List<String> list = new ArrayList<>();
		for (Printer p2 : Printer.getAllPrinters()) {
			if (p2.getName().equals(printer)) {
				Set<PaperSource> sps = p2.getPrinterAttributes().getSupportedPaperSources();
				for (PaperSource ps : sps) list.add(ps.getName());
			}
		}
		return list;
	}

	private static List<String> getPapers(String printer) {
		List<String> list = new ArrayList<>();
		for (Printer p2 : Printer.getAllPrinters()) {
			if (p2.getName().equals(printer)) {
				Set<Paper> sps = p2.getPrinterAttributes().getSupportedPapers();
				for (Paper ps : sps) list.add(ps.getName());
			}
		}
		return list;
	}

	/**
	 * @param element The <prtopen Element
	 * @return The ID to be used by the runtime to refer to this printer
	 * @throws Exception 
	 */
	private static String prtOpen(Element element) throws SCPrintException {
		Integer i1 = Integer.valueOf(openPrinters.size() + 1);
		openPrinters.put(i1, new PrintJob(element));
		return i1.toString();
	}

	/**
	 * <prtclose h=1/>
	 * @param element
	 */
	private static void prtClose(Element element) {
		Integer id = Integer.parseInt(element.getAttributeValue("h")); // will autobox
		PrintJob pj = openPrinters.get(id);
		if (pj != null) pj.close();
		openPrinters.remove(id);
	}

	/**
	 * Close any open print jobs and purge my list
	 */
	private static void prtExit() {
		openPrinters.clear();
	}

	private static void prtAlloc(String printerId) throws SCPrintException {
		if (Client.isDebug())
			System.out.printf("PrinterSupport.prtAlloc, id=%s\n", printerId);
		PrintJob pj = openPrinters.get(Integer.parseInt(printerId));
		if (pj != null) pj.alloc();
	}

	private static void prtText(Element element) throws SCPrintException {
		Integer id = Integer.parseInt(element.getAttributeValue("h")); // will autobox
		PrintJob pj = openPrinters.get(id);
		if (pj != null) pj.text(element.getChildren());
	}

	private static void prtRelease(String printerId) {
		PrintJob pj = openPrinters.get(Integer.valueOf(printerId));
		if (pj != null) pj.release();
	}

	private static void prtSetup() {
		// TODO Auto-generated method stub
		
	}

	public static void parsePrinterXMLCommand(Element element, OutputStream sout,
			XMLOutputStream xmlout) throws IOException
	{
		if (element.name.equals("prtget")) {
			Element r1 = new Element("r");
			List<String> printers = getPrinters();
			for (String p1 : printers) r1.addElement(new Element("p", p1));
			ClientUtils.SendElement(sout, xmlout, r1);				
		}
		else if (element.name.equals("prtbin")) {
			Element r1 = new Element("r");
			List<String> bins = getBins((String)element.getChild(0));
			for (String p1 : bins) r1.addElement(new Element("p", p1));
			ClientUtils.SendElement(sout, xmlout, r1);				
		}
		else if (element.name.equals("prtpnames")) {
			Element r1 = new Element("r");
			List<String> bins = getPapers((String)element.getChild(0));
			for (String p1 : bins) r1.addElement(new Element("p", p1));
			ClientUtils.SendElement(sout, xmlout, r1);				
		}
		else if (element.name.equals("prtopen")) {
			String id;
			try {
				id = prtOpen(element);
			}
			catch (SCPrintException e){
				Element r1 = new Element("r");
				r1.addAttribute(new Attribute("e", e.getErrorCode().toString()));
				String msg = e.getMessage();
				if (msg != null && msg.length() > 0) r1.addString(msg);
				ClientUtils.SendElement(sout, xmlout, r1);
				return;
			}
			ClientUtils.SendElement(sout, xmlout, new Element("r", id));
			
		}
		else if (element.name.equals("prtalloc")) {
			try {
				prtAlloc(element.getAttributeValue("h"));
			}
			catch (SCPrintException e){
				Element r1 = new Element("r");
				r1.addAttribute(new Attribute("e", e.getErrorCode().toString()));
				r1.addString(e.getMessage());
				ClientUtils.SendElement(sout, xmlout, r1);
				return;
			}
			ClientUtils.SendElement(sout, xmlout, simpleResponse);				
		}
		else if (element.name.equals("prttext")) {
			try {
				prtText(element);
			}
			catch (SCPrintException e){
				Element r1 = new Element("r");
				r1.addAttribute(new Attribute("e", e.getErrorCode().toString()));
				r1.addString(e.getMessage());
				ClientUtils.SendElement(sout, xmlout, r1);
				return;
			}
			ClientUtils.SendElement(sout, xmlout, simpleResponse);				
		}
		else if (element.name.equals("prtclose")) {
			prtClose(element);
			ClientUtils.SendElement(sout, xmlout, simpleResponse);				
		}
		else if (element.name.equals("prtexit")) {
			prtExit();
			ClientUtils.SendElement(sout, xmlout, simpleResponse);				
		}
		else if (element.name.equals("prtrel")) {
			prtRelease(element.getAttributeValue("h"));
			ClientUtils.SendElement(sout, xmlout, simpleResponse);				
		}
		else if (element.name.equals("prtsetup")) {
			prtSetup();
			ClientUtils.SendElement(sout, xmlout, simpleResponse);				
		}
	}
	
	/**
	 * @param drawPoint The current print position
	 * @param point The point given in the *rectangle or *box print control
	 * @return A javafx.scene.shape.Rectangle object positioned and sized
	 */
	static Rectangle calculateRectangle(Point2D.Double drawPoint, Point2D.Double iPnt) {
		Rectangle rect = new Rectangle();
		/*
		 * fillRect and drawRect draw the rectangle with the upper left corner
		 * at the coordinates set by the first two arguments. The Point from the
		 * prtObj is the diagonally opposite corner from the draw point. So we
		 * have to calculate the actual draw point and the width/height
		 */
		if ((int) drawPoint.x <= iPnt.x) { /*
											 * current draw point is left of
											 * given point
											 */
			rect.setX(drawPoint.x);
			rect.setWidth(iPnt.x - drawPoint.x);
		} else { /* current draw point is right of given point */
			rect.setX(iPnt.x);
			rect.setWidth(drawPoint.x - iPnt.x);
		}
		if ((int) drawPoint.y <= iPnt.y) { /*
											 * current draw point is above given
											 * point
											 */
			rect.setY(drawPoint.y);
			rect.setHeight(iPnt.y - drawPoint.y);
		} else { /* current draw point is below given point */
			rect.setY(iPnt.y);
			rect.setHeight(drawPoint.y - iPnt.y);
		}
		return rect;
	}


}
