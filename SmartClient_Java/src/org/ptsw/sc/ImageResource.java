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

import java.awt.Point;

import org.ptsw.sc.xml.Attribute;
import org.ptsw.sc.xml.Element;

import javafx.scene.Node;
import javafx.scene.canvas.Canvas;
import javafx.scene.canvas.GraphicsContext;
import javafx.scene.image.Image;
import javafx.scene.image.PixelFormat;
import javafx.scene.image.PixelReader;
import javafx.scene.image.PixelWriter;
import javafx.scene.image.WritableImage;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;

public class ImageResource {

	private int hsize;
	private int vsize;
	private int colorbits;
	private final Canvas canvas;
	private WindowDevice sCWindow;
	private Point position = new Point(0, 0);
	private SCFontAttributes currentFontAttributes;
	private Font currentFont;
	private FontMetrics fontMetrics;
	private int imageRotationAngle;
	private Image image;
	
	ImageResource(Element element) throws Exception {
		for (Attribute a1 : element.getAttributes()) {
			switch (a1.name) {
			case "h":
				hsize = Integer.parseInt(a1.value);
			case "v":
				vsize = Integer.parseInt(a1.value);
			case "colorbits":
				colorbits = Integer.parseInt(a1.value);
			}
		}
		if (colorbits != 1 && colorbits != 24) {
			throw new Exception("Java SC supports only 1 or 24 bit images");
		}
		currentFontAttributes = SCFontAttributes.getDefaultImageDrawingFontAttributes();
		currentFont = currentFontAttributes.getFont();
		canvas = new Canvas(hsize, vsize);
		GraphicsContext gc = canvas.getGraphicsContext2D();
		gc.setFill(Color.BLACK);
		gc.setStroke(Color.BLACK);
		gc.setLineWidth(1.1f);
		gc.setGlobalAlpha(1.0f);
		gc.setFont(currentFont);
		gc.fillRect(0, 0, hsize, vsize);
		fontMetrics = new FontMetrics(currentFont);
	}

	public Node getContent() { return canvas; }

	void draw(final Element element) {
		assert element != null;
		if (Client.isDebug())
			System.out.printf("Entering ImageResource.draw on thread %s\n", Thread.currentThread().getName());
		SmartClientApplication.Invoke(new Runnable() {
			@Override
			public void run() {
				int x1, y1, x2, y2;
				GraphicsContext gc = canvas.getGraphicsContext2D();
				for (Object o1 : element.getChildren()) {
					if (!(o1 instanceof Element)) continue;
					Element e1 = (Element) o1;
					switch (e1.name) {
					case "atext":
						String txt1 = e1.getAttributeValue("t");
						int angle = Integer.parseInt(e1.getAttributeValue("n"));
						if (Client.isGuiImageRotationAngleInterpretationOld()) angle -= 90;
						if (angle != 0) atext(gc, txt1, angle);
						else gc.fillText(txt1, position.x, position.y + fontMetrics.getLineHeight());
						break;
					case "bigdot":
						int radius = Integer.parseInt(e1.getAttributeValue("s"));
						gc.fillOval(position.x - radius, position.y - radius, 2 * radius, 2 * radius);
						break;
					case "brush":
						if (e1.hasAttribute("w")) {
							// LINEWIDTH
							gc.setLineWidth(Integer.parseInt(e1.getAttributeValue("w")));
						}
						else if (e1.hasAttribute("t")) {
							// LINETYPE
							@SuppressWarnings("unused")
							String lt = e1.getAttributeValue("t");
							// Valid: 'solid'  'revdot'
							// No way to support this yet in FX
						}
						break;
					case "clipimage":
						ImageResource otherImage = Client.GetImage(e1.getAttributeValue(Client.IMAGE));
						if (otherImage != null) {
							clipImage(gc, e1, otherImage.getContent().snapshot(null, null));
						}
						break;
					case "circle":
						radius = Integer.parseInt(e1.getAttributeValue("r"));
						gc.strokeOval(position.x - radius, position.y - radius, 2 * radius, 2 * radius);
						break;
					case "color":
						Color clr = Color.rgb(Integer.parseInt(e1.getAttributeValue("r")),
								Integer.parseInt(e1.getAttributeValue("g")),
								Integer.parseInt(e1.getAttributeValue("b")), 1.0f);
						gc.setFill(clr);
						gc.setStroke(clr);
						break;
					case "ctext":
						txt1 = e1.getAttributeValue("t");
						float stringWidth = fontMetrics.computeStringWidth(txt1);
						int areaWidth = Integer.parseInt(e1.getAttributeValue("hsize"));
						double X = position.x + ((areaWidth - stringWidth) / 2f);
						gc.fillText(txt1, X, position.y + fontMetrics.getLineHeight());
						break;
					case "dbox":
						box(gc, e1, false);
						break;
					case "dimage":
						otherImage = Client.GetImage(e1.getAttributeValue(Client.IMAGE));
						if (otherImage != null) {
							dImage(gc, otherImage.getContent().snapshot(null, null));
						}
						break;
					case "dot":
						gc.strokeLine(position.x, position.y, position.x, position.y);
						break;
					case "erase":
						gc.fillRect(0, 0, hsize, vsize);
						break;
					case "font":
						currentFontAttributes = currentFontAttributes.Merge(e1);
						currentFont = currentFontAttributes.getFont();
						fontMetrics = new FontMetrics(currentFont);
						gc.setFont(currentFont);
						break;
					case "irotate":
						angle = Integer.parseInt(e1.getAttributeValue("n"));
						setImageRotationAngle(angle);
						break;
					case "line":
						x1 = Integer.parseInt(e1.getAttributeValue("h"));
						y1 = Integer.parseInt(e1.getAttributeValue("v"));
						gc.strokeLine(position.x, position.y, x1, y1);
						position.x = x1;
						position.y = y1;
						break;
					case "linefeed":
						position.y += fontMetrics.getLineHeight();
						break;
					case "newline":
						position.x = 0;
						position.y += fontMetrics.getLineHeight();
						break;
					case "p": // Absolute values are one-based on the wire
						for (Attribute a1 : e1.getAttributes()) {
							switch (a1.name) {
							case "h":
								position.x = Integer.parseInt(a1.value) - 1;
								break;
							case "v":
								position.y = Integer.parseInt(a1.value) - 1;
								break;
							case "ha":
								position.x += Integer.parseInt(a1.value);
								break;
							case "va":
								position.y += Integer.parseInt(a1.value);
								break;
							}
						}
						break;
					case "rectangle":
						box(gc, e1, true);
						break;
					case "replace":
						replace(gc, e1);
						break;
					case "rtext":
						txt1 = e1.getAttributeValue("t");
						stringWidth = fontMetrics.computeStringWidth(txt1);
						gc.fillText(txt1, position.x - stringWidth, position.y + fontMetrics.getLineHeight());
						break;
					case "stretchimage":
						otherImage = Client.GetImage(e1.getAttributeValue(Client.IMAGE));
						if (otherImage != null) {
							stretchImage(gc, e1, otherImage.getContent().snapshot(null, null));
						}
						break;
					case "text":
						txt1 = e1.getAttributeValue("t");
						gc.fillText(txt1, position.x, position.y + fontMetrics.getLineHeight());
						position.x += fontMetrics.computeStringWidth(txt1);
						break;
						
					case "triangle":
						x1 = Integer.parseInt(e1.getAttributeValue("h1"));
						y1 = Integer.parseInt(e1.getAttributeValue("v1"));
						x2 = Integer.parseInt(e1.getAttributeValue("h2"));
						y2 = Integer.parseInt(e1.getAttributeValue("v2"));
						double[] xPoints = {position.x, x1, x2};
						double[] yPoints = {position.y, y1, y2};
						gc.fillPolygon(xPoints, yPoints, 3);
						break;
					}
				}
			}
		}, true);
		if (Client.isDebug()) {
			System.out.println("Leaving ImageResource.draw");
			System.out.flush();
		}
	}

	private void stretchImage(GraphicsContext gc, Element e1, Image img) {
		int x1 = Integer.parseInt(e1.getAttributeValue("h1"));
		int y1 = Integer.parseInt(e1.getAttributeValue("v1"));
		if (imageRotationAngle == 0)
			gc.drawImage(img, position.x, position.y, x1, y1);
		else {
			gc.save();
			gc.translate(position.x, position.y);
			gc.rotate(imageRotationAngle);
			gc.drawImage(img, 0, 0, x1, y1);
			gc.restore();
		}
	}
	
	private void clipImage(GraphicsContext gc, Element e1, Image img) {
		int x1 = Integer.parseInt(e1.getAttributeValue("h1")) - 1;
		int y1 = Integer.parseInt(e1.getAttributeValue("v1")) - 1;
		int x2 = Integer.parseInt(e1.getAttributeValue("h2")) - 1;
		int y2 = Integer.parseInt(e1.getAttributeValue("v2")) - 1;
		int width = Math.abs(x2 - x1);
		int height = Math.abs(y2 - y1);
		if (imageRotationAngle == 0)
			gc.drawImage(img, x1, y1, width, height, position.x, position.y, width, height);
		else {
			gc.save();
			gc.translate(position.x, position.y);
			gc.rotate(imageRotationAngle);
			gc.drawImage(img, x1, y1, width, height, 0, 0, width, height);
			gc.restore();
		}
	}

	private void dImage(GraphicsContext gc, Image img) {
		if (imageRotationAngle == 0) gc.drawImage(img, position.x, position.y);
		else {
			gc.save();
			gc.translate(position.x, position.y);
			gc.rotate(imageRotationAngle);
			gc.drawImage(img, 0, 0);
			gc.restore();
		}
	}
	
	private void atext(GraphicsContext gc, String txt1, int angle) {
		gc.save();
		gc.translate(position.x, position.y);
		gc.rotate(angle);
		gc.fillText(txt1, 0, fontMetrics.getLineHeight());
		gc.restore();
	}
	
	private void setImageRotationAngle(int angle) {
		if (Client.isGuiImageRotationAngleInterpretationOld()) {
			imageRotationAngle = angle - 90;
		}
		else imageRotationAngle = angle;
	}
	
	private void box(GraphicsContext gc, Element e1, boolean fillit) {
		int x1 = Integer.parseInt(e1.getAttributeValue("h"));
		int y1 = Integer.parseInt(e1.getAttributeValue("v"));
		int leftX = Math.min(position.x, x1);
		int topY = Math.min(position.y, y1);
		int width = Math.abs(position.x - x1) + 1;
		int height = Math.abs(position.y - y1) + 1;
		if (fillit) gc.fillRect(leftX, topY, width, height);
		else gc.strokeRect(leftX, topY, width, height);
	}

	/**
	 * Causes a crash as of b114
	 * @param gc
	 * @param e1
	 */
	private void replace(GraphicsContext gc, Element e1) {
		if (!SmartClientApplication.ignoreUnsupportedChangeCommands)
			throw new UnsupportedOperationException("The JavaSC does not support draw/replace");
		int r, g, b;
		Attribute a1, a2, a3;
		a1 = e1.getAttribute(0);
		a2 = e1.getAttribute(1);
		a3 = e1.getAttribute(2);
		int oldRed = Integer.parseInt(a1.value);
		int oldGreen = Integer.parseInt(a2.value); 
		int oldBlue = Integer.parseInt(a3.value);
		a1 = e1.getAttribute(3);
		a2 = e1.getAttribute(4);
		a3 = e1.getAttribute(5);
		r = Integer.parseInt(a1.value);
		g = Integer.parseInt(a2.value); 
		b = Integer.parseInt(a3.value);
		Color newColor = Color.rgb(r, g, b);
		PixelWriter pw = gc.getPixelWriter();
		WritableImage wi = canvas.snapshot(null, null);
		PixelReader pr = wi.getPixelReader();
		int[] buffer = new int[hsize * vsize];
		pr.getPixels(0, 0, hsize, vsize, PixelFormat.getIntArgbInstance(), buffer, 0, hsize);
		int row = 0, col = 0;
		for (; row < vsize; row++) {
			for (col = 0; col < hsize; col++) {
				int oldPixel = buffer[row * hsize + col];
				if (((oldPixel & 0x00ff0000) >> 16) == oldRed
						&& ((oldPixel & 0x0000ff00) >> 8) == oldGreen
						&& (oldPixel & 0x000000ff) == oldBlue)
				{
					pw.setColor(col, row, newColor);
				}
			}
		}
	}
	
	void SetVisibleOn(WindowDevice sCWindow) {
		this.sCWindow = sCWindow;
	}

	void Hide() {
		if (sCWindow != null) sCWindow.HideImage(this);
	}

	/**
	 * This is called as a result of the 'load image, device' verb being executed
	 * where the device is opened to an image file of some kind.
	 * @param element
	 * @throws Exception 
	 */
	void Load(Element element) throws Exception {
		int i1, i2, i3, i4;
		int bpp = Integer.parseInt(element.getAttributeValue("bpp"));
		if (bpp != 1 && bpp != 24) {
			throw new Exception("Only 1 and 24 bit images are supported");
		}
		int hsize = Integer.parseInt(element.getAttributeValue("h"));
		int vsize = Integer.parseInt(element.getAttributeValue("v"));
		String incomingBits = element.getAttributeValue("bits");
		StringBuilder sb1;
		StringBuilder sb2;
		
		sb1 = new StringBuilder(incomingBits);
		sb2 = new StringBuilder();
		
		fromprintable(sb1, sb2);
		runlengthdecode(sb2, sb1);
		
		WritableImage wi = new WritableImage(hsize, vsize);
		PixelWriter pw = wi.getPixelWriter();
		for (i1 = i4 = 0, i3 = 7; i1 < vsize; i1++) {
			for (i2 = 0; i2 < hsize; i2++) {
				if (bpp == 1) {
					if ((sb1.charAt(i4) & (1 << i3)) > 0) pw.setColor(i2, i1, Color.WHITE);
					else pw.setColor(i2, i1, Color.BLACK);
					if (--i3 < 0) {
						i3 = 7;
						i4++;
					}											 				
				}
				else {
					int argb = 0xFF000000;
					argb |= sb1.charAt(i4++) + (sb1.charAt(i4++) << 8) +  (sb1.charAt(i4++) << 16);
					pw.setArgb(i2, i1, argb);
				}
			}
		}
		GraphicsContext gc = canvas.getGraphicsContext2D();
		gc.drawImage(wi, 0, 0);
		if (Client.isDebug()) System.out.printf("In ImageResource.Load, this image is %s\n",
				canvas.getId());
	}

	/**
	 * Convert ASCII 0x00-0xFF data to printable characters (0x3F-7E)
	 * 4 Characters generated for every 3
	 *
	 */
	public static void makeprintable(StringBuilder in, StringBuilder out) 
	{
		int i1;
		char c1, c2, c3;

		out.setLength(0);
		while (in.length() % 3 > 0) in.append(0);
		for (i1 = 0; i1 < in.length(); ) {
			c1 = in.charAt(i1++);
			c2 = in.charAt(i1++);
			c3 = in.charAt(i1++);
			out.append((char) ((c1 & 0x3f) + '?'));
			out.append((char) ((c1 >> 6) + ((c2 & 0x0f) << 2) + '?'));
			out.append((char) ((c2 >> 4) + ((c3 & 0x03) << 4) + '?'));
			out.append((char) ((c3 >> 2) + '?'));
		} 
	}

	/*
	 * Convert printable characters to Run Length Encoded Data
	 * 3 Characters generated for every 4 input characters
	 *
	**/
	public static void fromprintable(StringBuilder in, StringBuilder out) 
	{
		int i1;
		char c1, c2, c3, c4;

		out.setLength(0);
		for (i1 = 0; i1 < in.length(); ) {
			c1 = (char) (in.charAt(i1++) - '?');
			c2 = (char) (in.charAt(i1++) - '?');
			c3 = (char) (in.charAt(i1++) - '?');
			c4 = (char) (in.charAt(i1++) - '?');
			out.append((char) ((c1 & 0x3f) + ((c2 & 0x03) << 6)));
			out.append((char) (((c2 & 0x3c) >> 2) + ((c3 & 0x0f) << 4)));
			out.append((char) (((c3 & 0x30) >> 4) + ((c4 & 0x3f) << 2)));
		} 
	}

	/** 
	 * Run Length Encode 
	 */
	public static void runlengthencode(StringBuilder in, StringBuilder out) 
	{
		int fpos, i1, count;
		boolean flag, end, cflag;
		char c1, c2;
		
		out.setLength(0);
		
		for (fpos = 0, c1 = c2 = (char) 0, flag = end = false ; ; ) {
			for (cflag = false, count = 1; ; count++) {
				if (fpos + count - 1 >= in.length()) {
					end = true;
					count--; 
					break;
				}
				c1 = in.charAt(fpos + count - 1);
				if (cflag && c1 == c2) {
					count -= 2;
					flag = true;
					break;
				}
				c2 = c1;
				cflag = true;
				if (count == 128) {
					break;
				}
			}

			if (count > 0) {
				out.append((char) (count - 1));
				while (count-- > 0) {
					out.append(in.charAt(fpos++));
				}
			}
		
			if (flag) {
				i1 = 2;
				fpos++;
				while ((++fpos < in.length()) && (in.charAt(fpos) == c1)) {
					i1++;
					if (i1 == 128) {
						fpos++;
						break;
					}
				}
				out.append((char) (257 - i1)); 
				out.append(c1);
				flag = false;
			}

			if (end) break;
		}
		
		out.append((char) 0x80);
					
		/* Make sure data is exactly divisible by 3 for */  
		/* later conversion to printable characters */

		while ((out.length() % 3) > 0) out.append((char) 0x00);
	}

	/** 
	 * Run Length Decode 
	 */
	public static void runlengthdecode(StringBuilder in, StringBuilder out) 
	{
		int i1, i2, i3;
		
		out.setLength(0);
		for (i1 = 0; i1 < in.length(); i1++) {
			i2 = in.charAt(i1);
			if (i2 < 128) { // No Run 
				for (++i2, i3 = 0; i3 < i2; i3++) {
					out.append(in.charAt(++i1));
				}
			}
			else if (i2 > 128) { // Run 
				for (i1++, i3 = 0; i3 < (257 - i2); i3++) {
					out.append(in.charAt(i1));
				}
			}
			else break;
		}
	}

	/**
	 * @return the canvas
	 */
	public Canvas getCanvas() {
		return canvas;
	}

	public StringBuilder getimagebits() {
		SmartClientApplication.Invoke(new Runnable() {
			@Override
			public void run() {
				image = canvas.snapshot(null, null);
			}
		}, true);
		PixelReader pr1 = image.getPixelReader();
		StringBuilder sb2 = new StringBuilder(hsize * vsize * 3);
		int[] buffer = new int[hsize * vsize];
		pr1.getPixels(0, 0, hsize, vsize, PixelFormat.getIntArgbInstance(), buffer, 0, hsize);
		
		int pixel;
		char[] ca = new char[1];
		if (colorbits == 24) {
			for (int row = 0; row < vsize; row++) {
				for (int col = 0; col < hsize; col++) {
					pixel = buffer[row * hsize + col] & 0x00FFFFFF;
					ca[0] = (char) (pixel & 0x000000FF);
					sb2.append(ca);
					ca[0] = (char) ((pixel & 0x0000FF00) >> 8);
					sb2.append(ca);
					ca[0] = (char) ((pixel & 0x00FF0000) >> 16);
					sb2.append(ca);
				}
			}
		}
		else {
			byte b1;
			int i1;
			for (int row = 0; row < vsize; row++) {
				i1 = 7;
				b1 = 0;
				for (int col = 0; col < hsize; col++) {
					pixel = buffer[row * hsize + col] & 0x00FFFFFF;
					if (pixel == 0xFFFFFF) b1 |= (1 << i1);
					if (--i1 < 0) {
						ca[0] = (char) b1;
						ca[0] &= 0x00ff;
						sb2.append(ca);
						i1 = 7;
						b1 = 0;
					}
				}
				if (i1 != 7) { // If the pixels per row is not a multiple of eight
					ca[0] = (char) b1;
					ca[0] &= 0x00ff;
					sb2.append(ca);
				}
			}
		}

		StringBuilder sb3 = new StringBuilder();
		runlengthencode(sb2, sb3);
		makeprintable(sb3, sb2);
		return sb2;
	}

	/**
	 * @return The bits per pixel
	 */
	public int getColorbits() {
		return colorbits;
	}

	void setImage(Image img) {
		GraphicsContext gc = canvas.getGraphicsContext2D();
		gc.save();
		gc.setFill(Color.BLACK);
		gc.fillRect(0, 0, hsize, vsize);
		gc.drawImage(img, 0, 0);
		gc.restore();
	}
}
