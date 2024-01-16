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
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

import org.ptsw.sc.Client;
import org.ptsw.sc.FontMetrics;
import org.ptsw.sc.ImageResource;
import org.ptsw.sc.SCFontAttributes;
import org.ptsw.sc.SmartClientApplication;
import org.ptsw.sc.Print.PrinterSupport.PRTERR;
import org.ptsw.sc.xml.Attribute;
import org.ptsw.sc.xml.Element;

import javafx.print.PageLayout;
import javafx.print.PageOrientation;
import javafx.print.Paper;
import javafx.print.PaperSource;
import javafx.print.PrintResolution;
import javafx.print.PrintSides;
import javafx.print.Printer;
import javafx.print.PrinterAttributes;
import javafx.print.PrinterJob;
import javafx.print.Printer.MarginType;
import javafx.scene.Group;
import javafx.scene.Node;
import javafx.scene.canvas.Canvas;
import javafx.scene.canvas.GraphicsContext;
import javafx.scene.control.Label;
import javafx.scene.image.Image;
import javafx.scene.image.ImageView;
import javafx.scene.layout.Pane;
import javafx.scene.layout.Region;
import javafx.scene.paint.Color;
import javafx.scene.shape.Circle;
import javafx.scene.shape.Line;
import javafx.scene.shape.Polygon;
import javafx.scene.shape.Rectangle;
import javafx.scene.text.Font;
import javafx.scene.text.Text;
import javafx.scene.transform.Rotate;
import javafx.scene.transform.Scale;

@SuppressWarnings("unused")
class PrintJob {

	private static final float INCH = 72f; // JavaFX standard for printing ??
	private static final String DPIPattern = " *\\p{Digit}+ *";
	
	private SCFontAttributes fontAttributes;
	private Font font;
	private FontMetrics fontMetrics;
	private String docName;
	private Printer printer;
	private PrinterJob printerJob;
	private Group currentNode;
	private Point2D.Double currentPosition = new Point2D.Double();
	private Paper paper = Paper.NA_LETTER;
	private PageOrientation orient = PageOrientation.PORTRAIT;
	private double scale;
	private Color color = Color.BLACK;
	private double linewidth = 1f;
	private PrintSides sides = PrintSides.ONE_SIDED;
	private PaperSource source = PaperSource.AUTOMATIC;

	private class PrinterNameAndDPI {
		String name;
		int DPI;
		public PrinterNameAndDPI(String printerName) {
			name = printerName;
			DPI = -1;
		}
	}
	
	/**
	 * <prtopen s="Default"><doc s="DB/C"/></prtopen>
	 * @param element
	 * @throws Exception 
	 */
	PrintJob(Element element) throws SCPrintException {
		boolean jDLG = false;
		
		PrinterNameAndDPI pnai = new PrinterNameAndDPI(element.getAttributeValue("s"));
		processPrinterNameForDPI(pnai);
		if (pnai.name.toUpperCase().equals("DEFAULT")) {
			printer = Printer.getDefaultPrinter();
		}
		else {
			boolean foundIt = false;
			for (Printer prntr : Printer.getAllPrinters()) {
				if (prntr.getName().equals(pnai.name)) {
					printer = prntr;
					foundIt = true;
					break;
				}
			}
			if (!foundIt) {
				SCPrintException px = new SCPrintException(PRTERR.BADNAME,
						String.format("Unknown printer (%s)", pnai.name));
				throw px;
			}
		}
		PrinterAttributes printAttrs = printer.getPrinterAttributes();
		if (pnai.DPI == -1 /* If the user did NOT specify a DPI */ ) {
			PrintResolution pr1 = printAttrs.getDefaultPrintResolution();
			pnai.DPI = pr1.getFeedResolution();
		}
		scale = INCH / pnai.DPI;
		
		boolean foundIt;
		for (Object obj1 : element.getChildren()) {
			if (obj1 instanceof Element) {
				Element e1 = (Element) obj1;
				switch (e1.name) {
				case "bin":
					foundIt = false;
					String binO = e1.getAttributeValue("s").toUpperCase();
					Set<PaperSource> psSet = printAttrs.getSupportedPaperSources();
					for (PaperSource ps1 : psSet) {
						if (binO.equals(ps1.getName().toUpperCase())){
							foundIt = true;
							source = ps1;
							break;
						}
					}
					if (!foundIt) {
						throw new SCPrintException(PRTERR.UNSUPCLNT, "bin");
					}
					break;
				case "doc":
					docName = e1.getAttributeValue("s");
					break;
				case "duplex":
					sides = PrintSides.DUPLEX; // Safe, ignored if N/A
					break;
					
				case "lang": // Only 'NATIVE' is accepted
					String lang = e1.getAttributeValue("s").toUpperCase();
					if (!lang.equals("NATIVE")) {
						throw new SCPrintException(PRTERR.BADOPT, "Only L=NATIVE accepted");
					}
					break;
					
				case "orient":
					String LorP = e1.getAttributeValue("s");
					switch (LorP) {
					case "port":
						orient = PageOrientation.PORTRAIT;
						break;
					case "land":
						orient = PageOrientation.LANDSCAPE;
						break;
					}
					Set<PageOrientation> pageO = printAttrs.getSupportedPageOrientations();
					if (!pageO.contains(orient)) {
						throw new SCPrintException(PRTERR.UNSUPCLNT, "orientation");
					}
					break;
				case "paper":
					foundIt = false;
					Set<Paper> supportedPapers = printAttrs.getSupportedPapers();
					String paperO = e1.getAttributeValue("s").toUpperCase();
					for (Paper p1 : supportedPapers) {
						if (paperO.equals(p1.getName().toUpperCase())){
							foundIt = true;
							paper = p1;
							break;
						}
					}
					if (!foundIt) {
						throw new SCPrintException(PRTERR.UNSUPCLNT, "paper");
					}
					break;
				case "jdlg":
					jDLG = true;
					break;
				case "margins":
					/* Ignored, do nothing */
					break;
				}
			}
		}
		if (jDLG) {
			alloc();
			if (!printerJob.showPrintDialog(null)) {
				printerJob.cancelJob();
				throw new SCPrintException(PRTERR.CANCEL, null);
			}
		}
	}

	private static void processPrinterNameForDPI(PrinterNameAndDPI pnai) {
		int lastIndexOfRParen = pnai.name.lastIndexOf(')');
		//
		// Do we have a right paren and is it the last character?
		//
		if (lastIndexOfRParen == -1 || lastIndexOfRParen != pnai.name.length() - 1) return;
		int lastIndexOfLParen = pnai.name.lastIndexOf('(');
		//
		// Do we have a left paren and is there anything in between the parens?
		//
		if (lastIndexOfLParen == -1 || lastIndexOfLParen == lastIndexOfRParen - 1) return;
		String temp = pnai.name.substring(lastIndexOfLParen + 1, lastIndexOfRParen);
		if (!temp.matches(DPIPattern)) return;
		//
		// User specified a DPI
		//
		pnai.DPI = Integer.parseInt(temp.trim());
		pnai.name = pnai.name.substring(0, lastIndexOfLParen);
		return;
	}

	void alloc() throws SCPrintException {
		if (printerJob == null) {
			PageLayout pageLayout =
					printer.createPageLayout(paper, orient , 0, 0, 0, 0);
			printerJob = PrinterJob.createPrinterJob(printer);
			printerJob.getJobSettings().setJobName(docName);
			printerJob.getJobSettings().setPageLayout(pageLayout);
			printerJob.getJobSettings().setPrintSides(sides);
			printerJob.getJobSettings().setPaperSource(source);
			fontAttributes = SCFontAttributes.getDefaultPrintFontAttributes();
			font = fontAttributes.getFont();
			fontMetrics = new FontMetrics(font);
			getFreshPage();
		}
	}

	void close() {
		if (printerJob != null) {
			if (currentNode != null) {
				printerJob.printPage(currentNode);
				currentNode = null;
			}
			boolean success = printerJob.endJob();
			if (Client.isDebug()) System.out.println("Job Ended, " + success);
			printerJob = null;
		}		
	}

	void text(Collection<Object> children) throws SCPrintException {
		int repeat;
		double radius;
		double textWidth;
		Point2D.Double otherPoint = new Point2D.Double();
		for (Object obj1 : children) {
			if (obj1 instanceof Element) {
				final Element e1 = (Element) obj1;
				switch (e1.name) {
				case "bigd":
					radius = Integer.parseInt(e1.getAttributeValue("r")) * scale;
					Circle bigdot = new Circle(currentPosition.x, currentPosition.y, radius, color);
					bigdot.setSmooth(true);
					currentNode.getChildren().add(bigdot);
					break;
					
				case "box":
					otherPoint.x = Integer.parseInt(e1.getAttributeValue("x")) * scale;
					otherPoint.y = Integer.parseInt(e1.getAttributeValue("y")) * scale;
					Rectangle box = PrinterSupport.calculateRectangle(currentPosition, otherPoint);
					box.setStrokeWidth(linewidth);
					box.setStroke(color);
					box.setFill(Color.TRANSPARENT);
					currentNode.getChildren().add(box);
					break;
					
				case "c":
					currentPosition.x = 0f;
					break;
					
				case "circ":
					radius = Integer.parseInt(e1.getAttributeValue("r")) * scale;
					Circle circle = new Circle(currentPosition.x, currentPosition.y, radius, Color.TRANSPARENT);
					circle.setStrokeWidth(linewidth);
					circle.setStroke(color);
					circle.setSmooth(true);
					currentNode.getChildren().add(circle);
					break;
					
				case "color":		//<color n="11823615" r="255" g="105" b="180"/>
					int r = Integer.parseInt(e1.getAttributeValue("r"));
					int g = Integer.parseInt(e1.getAttributeValue("g"));
					int b = Integer.parseInt(e1.getAttributeValue("b"));
					color = Color.rgb(r,  g,  b);
					break;
				case "d":
					printSomeText(e1, (String) e1.getChild(0));
					break;
				case "dimage":		//<dimage image="2"/>
					ImageResource ires = Client.GetImage(e1.getAttributeValue("image"));
					if (ires != null) printImage(ires);
					break;
				case "f":
					formFeed(e1);
					break;
				case "font":
					fontAttributes = fontAttributes.Merge(e1);
					font = fontAttributes.getFont();
					fontMetrics = new FontMetrics(font);
					break;
				case "h":
					currentPosition.x = Integer.parseInt(e1.getAttributeValue("n")) * scale;
					break;
				case "ha":
					currentPosition.x += Integer.parseInt(e1.getAttributeValue("n")) * scale;
					break;
				case "l":
					if (e1.hasAttribute("n")) {
						repeat = Integer.parseInt(e1.getAttributeValue("n"));
						currentPosition.y += repeat * fontMetrics.getLineHeight();
					}
					else currentPosition.y += fontMetrics.getLineHeight();
					break;
					
				case "line":		//<line h="10" v="1500"/>  One-Based!
					double endH = (Integer.parseInt(e1.getAttributeValue("h")) - 1) * scale;
					double endV = (Integer.parseInt(e1.getAttributeValue("v")) - 1) * scale;
					Line line = new Line(currentPosition.x, currentPosition.y, endH, endV);
					line.setStrokeWidth(linewidth);
					line.setStroke(color);
					currentNode.getChildren().add(line);
					currentPosition.setLocation(endH, endV);
					break;
					
				case "lw":			//<lw n="1"/>
					linewidth = Math.max(1f, Integer.parseInt(e1.getAttributeValue("n")) * scale);
					break;
					
				case "n":
					currentPosition.x = 0f;
					if (e1.hasAttribute("n")) {
						repeat = Integer.parseInt(e1.getAttributeValue("n"));
						currentPosition.y += repeat * fontMetrics.getLineHeight();
					}
					else currentPosition.y += fontMetrics.getLineHeight();
					break;
					
				case "p":		//<prttext h="1"><p h="600" v="600"/>
					currentPosition.x = Integer.parseInt(e1.getAttributeValue("h")) * scale;
					currentPosition.y = Integer.parseInt(e1.getAttributeValue("v")) * scale;
					break;
					
				case "rect":
					otherPoint.x = Integer.parseInt(e1.getAttributeValue("x")) * scale;
					otherPoint.y = Integer.parseInt(e1.getAttributeValue("y")) * scale;
					Rectangle rect = PrinterSupport.calculateRectangle(currentPosition, otherPoint);
					rect.setStroke(color);
					rect.setFill(color);
					currentNode.getChildren().add(rect);
					break;
					
				case "rpt":		//<rpt n="15">87</rpt>
					char c1 = (char) Integer.parseInt((String)e1.getChild(0));
					int n = Integer.parseInt(e1.getAttributeValue("n"));
					StringBuilder sb1 = new StringBuilder(n);
					for (int i1 = 0; i1 < n; i1++) sb1.append(c1);
					printSomeText(e1, sb1.toString());
					break;
					
				case "t":		// The number comes over the wire already adjusted to zero-based
					int count = Integer.parseInt(e1.getAttributeValue("n"));
					textWidth = fontMetrics.computeStringWidth("A");
					currentPosition.x = count * textWidth;
					break;
					
				case "tria":		// <tria x1="600" y1="100" x2="1099" y2="600"/>
					Polygon polygon = new Polygon(
							currentPosition.x, currentPosition.y,
							Integer.parseInt(e1.getAttributeValue("x1")) * scale,
							Integer.parseInt(e1.getAttributeValue("y1")) * scale,
							Integer.parseInt(e1.getAttributeValue("x2")) * scale,
							Integer.parseInt(e1.getAttributeValue("y2")) * scale
					    );
					polygon.setFill(color);
					polygon.setSmooth(true);
					currentNode.getChildren().add(polygon);
					break;
					
				case "v":
					currentPosition.y = Integer.parseInt(e1.getAttributeValue("n")) * scale;
					break;
					
				case "va":
					currentPosition.y += Integer.parseInt(e1.getAttributeValue("n")) * scale;
					break;
				}
			}
		}
	}
	
	private void printSomeText(Element e1, String text) throws SCPrintException {
		if (e1.hasAttribute("rj") && e1.hasAttribute("cj")) {
			SCPrintException px = new SCPrintException(PRTERR.BADOPT,
					"*cj and *rj not allowed together");
			throw px;
		}
		float textWidth = fontMetrics.computeStringWidth(text);
		Text t1 = new Text(text);
		t1.setFont(font);
		t1.setFill(color);
		
		if (e1.hasAttribute("ang")) {
			Attribute angAttr = e1.getAttribute("ang");
			int angle = Integer.parseInt(angAttr.value);
			if (Client.isPrintTextRotationAngleInterpretationOld()) angle -= 90;
			if (angle == 0) e1.removeAttribute(angAttr);
			else {
				if (e1.hasAttribute("rj")) {
					t1.relocate(currentPosition.x - textWidth, currentPosition.y);
					Rotate rot = new Rotate(angle, textWidth, 0f - (0.5 * fontMetrics.getLineHeight()));
					t1.getTransforms().add(rot);
				}
				else if (e1.hasAttribute("cj")) {
					int cSpace = (int) (Integer.parseInt(e1.getAttributeValue("cj")) * scale);
					if (textWidth < cSpace) {
						double xSpace = (cSpace - textWidth) / 2;
						t1.relocate(currentPosition.x + xSpace, currentPosition.y);
						Rotate rot = new Rotate(angle, -xSpace, 0f - (0.5 * fontMetrics.getLineHeight()));
						t1.getTransforms().add(rot);
					}
					else printTextwithAngle(t1, angle);		
				}
				else printTextwithAngle(t1, angle);
				currentNode.getChildren().add(t1);
				return;
			}
		}
		
		if (e1.hasAttribute("rj")) {
			t1.relocate(currentPosition.x - textWidth, currentPosition.y);
		}
		else if (e1.hasAttribute("cj")) {
			int cSpace = (int) (Integer.parseInt(e1.getAttributeValue("cj")) * scale);
			if (textWidth > cSpace) {
				t1.relocate(currentPosition.x, currentPosition.y);
				currentPosition.x += textWidth;
			}
			else {
				double xpos = currentPosition.x + (cSpace / 2) - (textWidth / 2);
				t1.relocate(xpos, currentPosition.y);
				currentPosition.x += cSpace;
			}
		}
		else {
			t1.relocate(currentPosition.x, currentPosition.y);
			currentPosition.x += textWidth;
		}
		currentNode.getChildren().add(t1);
	}
	
	private void printTextwithAngle(Text t1, int angle) {
		t1.relocate(currentPosition.x, currentPosition.y);
		t1.getTransforms().add(new Rotate(angle, 0f, 0f - (0.5 * fontMetrics.getLineHeight())));
	}
	
	private void printImage(final ImageResource ires) {
		final Canvas canvas = ires.getCanvas();
		SmartClientApplication.Invoke(new Runnable() {
			double width = canvas.getWidth() * scale;
			double height = canvas.getHeight() * scale;
			@Override
			public void run() {
				Image otherImage = canvas.snapshot(null, null);
				ImageView iv1 = new ImageView();
				iv1.setFitHeight(height);
				iv1.setFitWidth(width);
				iv1.setSmooth(true);
				iv1.setImage(otherImage);
				iv1.relocate(currentPosition.x, currentPosition.y);
				currentNode.getChildren().add(iv1);
			}
		}, true);
	}
	
	private void formFeed(Element e1) {
		int repeat;
		if (e1.hasAttribute("n")) {
			repeat = Integer.parseInt(e1.getAttributeValue("n"));
		}
		else repeat = 1;
		for (int i1 = 0; i1 < repeat; i1++) { 
			printerJob.printPage(currentNode);
			getFreshPage();
		}
	}

	private void getFreshPage() {
		currentNode = new Group();
		currentNode.setAutoSizeChildren(false);
		currentPosition.setLocation(0f, 0f);
	}
	
	/**
	 * From the dx16 PDF:
	 * 
	 * The release statement causes the currently allocated printer device to be deallocated. The release
	 * statement is ignored if the current print output is to a file. The release operation does not
	 * cause a splclose to occur. If a printer device is not currently allocated then an allocation of
	 * the device is attempted. If the device is successfully allocated, it is then deallocated and
	 * the over flag is cleared. Other wise, the over flag is set.
	 * The behavior of release on a non-allocated device may be operating system dependent
	 *
	 */
	void release() {
		
	}
	
}
