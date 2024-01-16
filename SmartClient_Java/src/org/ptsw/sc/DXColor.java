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

import java.awt.Color;

public class DXColor {

	private static final Color BLACK = new Color(0x00, 0x00, 0x00);
	private static final Color RED = new Color(0x80, 0x00, 0x00);
	private static final Color GREEN = new Color(0x00, 0x80, 0x00);
	private static final Color YELLOW = new Color(0x80, 0x80, 0x00);
	private static final Color BLUE = new Color(0x00, 0x00, 0x80); 
	private static final Color MAGENTA = new Color(0x80, 0x00, 0x80);
	private static final Color CYAN = new Color(0x00, 0x80, 0x80);
	private static final Color WHITE = new Color(0xc0, 0xc0, 0xc0);

	private static Color[] basicColors;
	private static Color[] brightColors;
	private static Color[] cubeColors;
	private static Color[] grays;
	
	private static boolean ansiMode;

	static byte getColorCode(String name) {
		switch (name) {
			case "black": return 0;
			case "blue": return 4;
			case "green": return 2;
			case "cyan": return 6;
			case "red": return 1;
			case "magenta": return 5;
			case "yellow": return 3;
			case "white": return 7;
			case "bgblack": return 0;
			case "bgblue": return 4;
			case "bggreen": return 2;
			case "bgcyan": return 6;
			case "bgred": return 1;
			case "bgmagenta": return 5;
			case "bgyellow": return 3;
			case "bgwhite": return 7;
		}
		return 0;
	}
	
	static Color getColor(int clrCode) {
		if (basicColors == null) init();
		if ( 0 <= clrCode && clrCode <= 7) return basicColors[clrCode];
		else if (8 <= clrCode && clrCode <=15) return brightColors[clrCode - 8];
		else if (!ansiMode) {
			return getColor(clrCode % 16);
		}
		if (16 <= clrCode && clrCode <= 231) return cubeColors[clrCode - 16];
		else if (232 <= clrCode && clrCode <= 255) return grays[clrCode - 232];
		return Color.PINK;
	}
	
	static void init() {
		ansiMode = Client.getDefault().isAnsi256ColorMode();
		if (Client.isDebug()) System.out.println("DXColor.init, ansiMode=" + ansiMode);
		basicColors = new Color[8];
		brightColors = new Color[8];
		basicColors[0] = BLACK;
		basicColors[1] = RED;
		basicColors[2] = GREEN;
		basicColors[3] = YELLOW;
		basicColors[4] = BLUE;
		basicColors[5] = MAGENTA;
		basicColors[6] = CYAN;
		basicColors[7] = WHITE;
		
		if (ansiMode) {
			brightColors[0] = new Color(0x80, 0x80, 0x80);
			brightColors[1] = new Color(0xFF, 0x00, 0x00);
			brightColors[2] = new Color(0x00, 0xFF, 0x00);
			brightColors[3] = new Color(0xFF, 0xFF, 0x00);
			brightColors[4] = new Color(0x00, 0x00, 0xFF);
			brightColors[5] = new Color(0xFF, 0x00, 0xFF);
			brightColors[6] = new Color(0x00, 0xFF, 0xFF);
			brightColors[7] = new Color(0xFF, 0xFF, 0xFF);
			initRamps();
			initGrays();
		}
		else {
			brightColors[0] = new Color(0x80, 0x80, 0x80); // Gray
			brightColors[1] = new Color(0x00, 0x00, 0xFF); // Blue
			brightColors[2] = new Color(0x00, 0xFF, 0x00); // Green
			brightColors[3] = new Color(0x00, 0xFF, 0xFF); // Cyan
			brightColors[4] = new Color(0xFF, 0x00, 0x00); // Red
			brightColors[5] = new Color(0xFF, 0x00, 0xFF); // Magenta
			brightColors[6] = new Color(0xFF, 0xFF, 0x00); // Yellow
			brightColors[7] = new Color(0xFF, 0xFF, 0xFF); // White
		}
	}
	
	private static void initRamps() {
		cubeColors = new Color[6 * 6 * 6];
		int[] iii = {0x00, 0x5f, 0x87, 0xaf, 0xd7, 0xff};
		for (int ired = 0; ired < 6; ired++) {
			for (int igreen = 0; igreen < 6; igreen++) {
				for (int iblue = 0; iblue < 6; iblue++) {
					int index = iblue + (igreen * 6) + (ired * 36);
					cubeColors[index] = new Color(iii[ired], iii[igreen], iii[iblue]);
				}
			}
		}
	}
	
	private static void initGrays() {
		grays = new Color[24];
		int ired = 0x08, iblue = 0x08, igreen = 0x08;
		for (int i1 = 0; i1 < 24; ired += 0x0a, igreen += 0x0a, iblue += 0x0a, i1++) {
			grays[i1] = new Color(ired, igreen, iblue);
		}
	}
}
