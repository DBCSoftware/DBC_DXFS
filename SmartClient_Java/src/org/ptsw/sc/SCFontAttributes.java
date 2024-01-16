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
import java.util.HashSet;
import java.util.Map;
import java.util.StringTokenizer;

import org.ptsw.sc.xml.Attribute;
import org.ptsw.sc.xml.Element;

import javafx.scene.text.Font;
import javafx.scene.text.FontPosture;
import javafx.scene.text.FontWeight;

/**
 * Class to manage font attributes as the DB/C programmer sees them
 * 
 * Underline not supported
 */
public class SCFontAttributes {
	
	private static final HashSet<String> installedFamilies = new HashSet<>(Font.getFamilies());
	private static int resolution = Client.getVerticalScreenResolution();
	
	/**
	 * Map font names as DB/C knows them to what Java uses.
	 */
	private static final Map<String, String> DBCFontNames = new HashMap<>(5);
	static {
		DBCFontNames.put("COURIER", "Monospaced");
		DBCFontNames.put("HELVETICA", "SansSerif");
		DBCFontNames.put("SYSTEM", "Arial");
		DBCFontNames.put("TIMES", "Serif");
	}

	private String name = SmartClientApplication.DEFAULTFONTNAME;
	private double size = SmartClientApplication.DEFAULTFONTSIZE;
	private boolean italic;
	private boolean bold;
	
	/**
	 * Used to construct a default font attributes object.
	 * This would be the starting point for a new Panel or Dialog
	 */
	private SCFontAttributes() {
	}

	public Font getFont()
	{
		return getFont(resolution);
	}

	public Font getFont(float res)
	{
		double sz = (size * res) / 72.00;
		if (!bold && !italic) return Font.font(name, FontWeight.NORMAL, sz);
		if (bold && !italic) return Font.font(name, FontWeight.BOLD, sz);
		if (!bold && italic) return Font.font(name, FontPosture.ITALIC, sz);
		if (bold && italic) return Font.font(name, FontWeight.BOLD, FontPosture.ITALIC, sz);
		return null;
	}

	public SCFontAttributes Merge(Element e1) {
		if (e1.hasAttribute("n")) {
			String newName = e1.getAttributeValue("n");
			if (newName.equalsIgnoreCase("DEFAULT")) name = SmartClientApplication.DEFAULTFONTNAME;
			else {
				if (DBCFontNames.containsKey(newName)) name = DBCFontNames.get(newName);
				else {
					if (installedFamilies.contains(newName)) name = newName;
				}
			}
		}
		if (e1.hasAttribute("italic")) {
			italic = e1.getAttributeValue("italic").equalsIgnoreCase("yes");
		}
		if (e1.hasAttribute("bold")) {
			bold = e1.getAttributeValue("bold").equalsIgnoreCase("yes");
		}
		if (e1.hasAttribute("s")) {
			size = Double.parseDouble(e1.getAttributeValue("s"));
		}
		return this;
	}
	
	SCFontAttributes Merge(String fontString) {
		int index = fontString.indexOf("(");
		Element e1 = new Element("x"); // name not used
		if (index == -1 || index > 0) {
			if (index == -1) {
				e1.addAttribute(new Attribute("n", fontString.toUpperCase()));
			} else {
				e1.addAttribute(new Attribute("n", fontString.substring(0, index).toUpperCase()));
			}
		}
		if (index != -1) {
			StringTokenizer st = new StringTokenizer(fontString.substring(index), " ,()");
			while (st.hasMoreElements()) {
				String s = st.nextToken();
				if (Character.isDigit(s.charAt(0))) {
					e1.addAttribute(new Attribute("s", String.valueOf(Double.parseDouble(s))));
				} else {
					if (s.equalsIgnoreCase("BOLD")) {
						e1.addAttribute(new Attribute("bold", "yes"));
					} else if (s.equalsIgnoreCase("NOBOLD")) {
						e1.addAttribute(new Attribute("bold", "no"));
					} else if (s.equalsIgnoreCase("ITALIC")) {
						e1.addAttribute(new Attribute("italic", "yes"));
					} else if (s.equalsIgnoreCase("NOITALIC")) {
						e1.addAttribute(new Attribute("italic", "no"));
					} else if (s.equalsIgnoreCase("PLAIN")) {
						e1.addAttribute(new Attribute("bold", "no"));
						e1.addAttribute(new Attribute("italic", "no"));
					}
				}
			}
		}
		return this.Merge(e1);
	}
	
	/**
	 * 'Factory' to create a clone of the default DB/C font attributes
	 * @return A freshly created default fonts object 
	 */
	static SCFontAttributes getDefaultFontAttributes()
	{
		return new SCFontAttributes();
	}

	/**
	 * 'Factory' to create a clone of the default DB/C font attributes for printing
	 *
	 * Note: "COURIER(12, PLAIN)" is the db/c standard for the default printer font.
	 * but for unknown reasons we need to use one about 3/4 of that size to match
	 * the output from our C code.
	 * 
	 * @return A freshly created default fonts object 
	 */
	public static SCFontAttributes getDefaultPrintFontAttributes()
	{
		SCFontAttributes sc = new SCFontAttributes();
		sc.name = "Monospaced";
		sc.size = 8.9;
		return sc;
	}

	static SCFontAttributes getDefaultImageDrawingFontAttributes()
	{
		SCFontAttributes sc = new SCFontAttributes();
		sc.name = "SansSerif";
		sc.size = 12;
		return sc;
	}
}
