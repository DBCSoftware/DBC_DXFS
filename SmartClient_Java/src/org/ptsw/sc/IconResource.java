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

import java.util.HashMap;

import org.ptsw.sc.xml.Attribute;
import org.ptsw.sc.xml.Element;

import javafx.scene.image.Image;
import javafx.scene.image.PixelWriter;
import javafx.scene.image.WritableImage;
import javafx.scene.paint.Color;

public class IconResource {

	private int colorbits;
	private String pixels;
	private final WritableImage image;
	private int horzSize;
	private int vertSize;
	private static final HashMap<Character, Color> fourColors= new HashMap<>(16);
	static {
		fourColors.put('0', Color.BLACK);
		fourColors.put('1', Color.DARKBLUE);
		fourColors.put('2', Color.DARKGREEN);
		fourColors.put('3', Color.DARKCYAN);
		fourColors.put('4', Color.DARKRED);
		fourColors.put('5', Color.DARKMAGENTA);
		fourColors.put('6', Color.BROWN);
		fourColors.put('7', Color.LIGHTGRAY);
		fourColors.put('8', Color.DARKGRAY);
		fourColors.put('9', Color.BLUE);
		fourColors.put('A', Color.GREEN);
		fourColors.put('B', Color.CYAN);
		fourColors.put('C', Color.RED);
		fourColors.put('D', Color.MAGENTA);
		fourColors.put('E', Color.YELLOW);
		fourColors.put('F', Color.WHITE);
	};
	
	IconResource(Element element) {
		for(Attribute a1 : element.getAttributes()) {
			switch (a1.name) {
			case "h":
				horzSize = Integer.parseInt(a1.value);
				break;
			case "v":
				vertSize = Integer.parseInt(a1.value);
				break;
			case "colorbits":
				colorbits = Integer.parseInt(a1.value);
			case "pixels":
				pixels = a1.value;
			}
		}
		
		image = new WritableImage(horzSize, vertSize);
		createImageFromPixels();
	}

	private void createImageFromPixels() {
		PixelWriter writer = image.getPixelWriter();
		int i1 = 0;
		for (int row = 0; row < vertSize; row++) {
			for (int col = 0; col < horzSize; col++) {
				char c1;
				switch (colorbits) {
				case 1:
					c1 = pixels.charAt(i1++);
					writer.setColor(col, row, c1 == 0x30 ? Color.BLACK : Color.WHITE);
					break;
				case 4:
					c1 = pixels.charAt(i1++);
					writer.setColor(col, row, fourColors.get(c1));
					break;
				case 24:
					if (pixels.charAt(i1) == 'T' || pixels.charAt(i1) == 't') {
						// Skip this pixel, it is transparent
						i1 += 6;
					}
					else {
						int blue = Integer.parseInt(pixels.substring(i1, i1 + 2), 16);
						i1 += 2;
						int green = Integer.parseInt(pixels.substring(i1, i1 + 2), 16);
						i1 += 2;
						int red = Integer.parseInt(pixels.substring(i1, i1 + 2), 16);
						i1 += 2;
						writer.setArgb(col, row, 0xFF000000 | (red << 16) | (green << 8) | blue);
					}
					break;
				}
			}
		}
	}

	public Image getImage() {
		return image;
	}
}
