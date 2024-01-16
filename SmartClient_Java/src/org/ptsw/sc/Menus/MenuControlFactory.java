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

package org.ptsw.sc.Menus;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.ptsw.sc.Client;
import org.ptsw.sc.IconResource;
import org.ptsw.sc.Resource;
import org.ptsw.sc.xml.Element;

import javafx.scene.control.CheckMenuItem;
import javafx.scene.control.Menu;
import javafx.scene.control.MenuItem;
import javafx.scene.control.SeparatorMenuItem;
import javafx.scene.image.ImageView;
import javafx.scene.input.KeyCombination;

public class MenuControlFactory {

	private final SCMenu parent;
	private MenuItem lastGrayable;
	private static final Map<String, KeyCombination> accKeys = new HashMap<>(128);
	
	static {
		List<String> Flist = Arrays.asList("F1", "F2", "F3", "F3", "F3", "F4", "F5", "F6",
				"F7", "F8", "F9", "F10", "F11", "F12");
		List<String> Alist = Arrays.asList("A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", 
				"L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z");
		for (String f : Flist) { accKeys.put(f, KeyCombination.valueOf(f)); }
		for (String f : Flist) { accKeys.put("ALT" + f, KeyCombination.valueOf("Alt+" + f)); }
		for (String f : Flist) { accKeys.put("CTRL" + f, KeyCombination.valueOf("Ctrl+" + f)); }
		for (String f : Flist) { accKeys.put("SHIFT" + f, KeyCombination.valueOf("Shift+" + f)); }
		for (String f : Flist) { accKeys.put("CTRLSHIFT" + f, KeyCombination.valueOf("Ctrl+Shift+" + f)); }
		
		for (String a : Alist) { accKeys.put("CTRL" + a,  KeyCombination.valueOf("Ctrl+" + a)); }
		for (String a : Alist) { accKeys.put("CTRLSHIFT" + a,  KeyCombination.valueOf("Ctrl+Shift+" + a)); }
		
		accKeys.put("CTRLCOMMA", KeyCombination.valueOf("Ctrl+,"));
		accKeys.put("CTRLPERIOD", KeyCombination.valueOf("Ctrl+."));
		accKeys.put("CTRLFSLASH", KeyCombination.valueOf("Ctrl+/"));
		accKeys.put("CTRLBSLASH", KeyCombination.valueOf("Ctrl+\\"));
		accKeys.put("CTRLSEMICOLON", KeyCombination.valueOf("Ctrl+;"));
		
		accKeys.put("CTRLQUOTE", KeyCombination.valueOf("Ctrl+Quote"));
		accKeys.put("CTRLLBRACKET", KeyCombination.valueOf("Ctrl+["));
		accKeys.put("CTRLRBRACKET", KeyCombination.valueOf("Ctrl+]"));
		accKeys.put("CTRLMINUS", KeyCombination.valueOf("Ctrl+-"));
		accKeys.put("CTRLEQUAL", KeyCombination.valueOf("Ctrl+="));
		
		accKeys.put("INSERT", KeyCombination.valueOf("Insert"));
		accKeys.put("DELETE", KeyCombination.valueOf("Delete"));
		accKeys.put("HOME", KeyCombination.valueOf("Home"));
		accKeys.put("END", KeyCombination.valueOf("End"));
		accKeys.put("PGUP", KeyCombination.valueOf("Page Up"));
		accKeys.put("PGDN", KeyCombination.valueOf("Page Down"));
	}

	public MenuControlFactory(Resource resource) {
		parent = (SCMenu) resource;
	}

	/**
	 * Used by MenuBars
	 * @param children
	 * @return a possibly empty ArrayList of Menu objects
	 * @throws Exception
	 */
	public Collection<Menu> createMenuNodes(Collection<Object> children) throws Exception {
		List<Menu> list1 = new ArrayList<>();
		for(Object o1 : children) {
			if (o1 instanceof Element) {
				Menu n1 = createMenuNode((Element)o1);
				if (n1 != null) list1.add(n1);
			}
		}
		return list1;
	}

	/**
	 * Used by ContextMenus
	 * @param children
	 * @return a possibly empty ArrayList of MenuItem objects
	 * @throws Exception
	 */
	public Collection<MenuItem> createNodes(Collection<Object> children) throws Exception {
		List<MenuItem> list1 = new ArrayList<>();
		for(Object o1 : children) {
			if (o1 instanceof Element) {
				MenuItem n1 = createNode((Element)o1);
				if (n1 != null) list1.add(n1);
			}
		}
		return list1;
	}


	// <prep menu="main0004">
	//   <main i="100" t="Sin&amp;gle"><gray/>
	//     <mitem i="110" t="Item 110"/>
	//     <submenu i="120" t="Item 120">
	//       <mitem i="122" t="Item 122"/>
	//       <mitem i="124" t="Item 124"/>
	//     </submenu>
	//     <mitem i="130" t="Item 130"/>
	//   </main>
	//   <main i="200" t="Double &amp;&amp; More!"/>
	//   <main i="300" t="Ite&amp;m 3">
	//     <mitem i="310" t="Item 3-1" ck="1"/>
	//     <iconitem i="320" t="Item 3-2" icon="mainico1" k="DELETE"/>
	//     <mitem i="330" t="Item 3-3" k="HOME"/>
	//     <gray/>
	//     <mitem i="340" t="Item 3-4" k="END"/>
	//     <line/>
	//     <mitem i="350" t="Item 3-5" k="PGUP"/>
	//     </main>
	// </prep>

	private MenuItem createNode(Element e1) throws Exception {
		MenuItem mi1 = null;
		if (e1.name.equals("mitem") || e1.name.equals("iconitem")) {
			if (e1.hasAttribute("ck")) 
				mi1 = new CheckMenuItem(mnemonic(e1.getAttributeValue("t")));
			else mi1 = new MenuItem(mnemonic(e1.getAttributeValue("t")));
			commonMenuItemStuff(e1, mi1);
		}
		else if (e1.name.equals("line")) {
			mi1 = new SeparatorMenuItem();
		}
		else if (e1.name.equals(Resource.GRAY)) {
			if (lastGrayable != null) {
				lastGrayable.setDisable(true);
				lastGrayable = null;
			}
		}
		else if (e1.name.equals("submenu") || e1.name.equals("iconsubmenu")) {
			mi1 = createMenuNode(e1);
		}
		return mi1;
	}
	
	private Menu createMenuNode(Element element) throws Exception {
		//if (!element.name.equals("main") && !element.name.equals("submenu")) return null;
		Menu menu = new Menu(mnemonic(element.getAttributeValue("t")));
		commonMenuItemStuff(element, menu);
		if (element.hasChildren()) {
			for(Object o1 : element.getChildren()) {
				if (o1 instanceof Element) {
					MenuItem mi1;
					Element e1 = (Element)o1;
					if (e1.name.equals("mitem") || e1.name.equals("iconitem")) {
						if (e1.hasAttribute("ck")) 
							mi1 = new CheckMenuItem(mnemonic(e1.getAttributeValue("t")));
						else mi1 = new MenuItem(mnemonic(e1.getAttributeValue("t")));
						commonMenuItemStuff(e1, mi1);
						menu.getItems().add(mi1);
					}
					else if (e1.name.equals("line")) {
						menu.getItems().add(new SeparatorMenuItem());
					}
					else if (e1.name.equals(Resource.GRAY)) {
						if (lastGrayable != null) {
							lastGrayable.setDisable(true);
							lastGrayable = null;
						}
					}
					else if (e1.name.equals("submenu") || e1.name.equals("iconsubmenu")) {
						menu.getItems().add(createMenuNode(e1));
					}
				}
			}
		}
		else {
			/*
			 * As of JavaFX 8, we can't get informed of a mouse click
			 * on a main menu bar menu item that has no children
			 */
		}
		return menu;
	}

	private void commonMenuItemStuff(Element e1, MenuItem mi1) throws Exception {
		mi1.setId(e1.getAttributeValue("i"));
		parent.getMenuItemMap().put(mi1.getId(), mi1);
		if (!e1.name.equals("submenu") && !e1.name.equals("iconsubmenu") && !e1.name.equals("main"))
			mi1.setOnAction(parent.getMenuActionHandler());
		if (e1.hasAttribute("k")) {
			String dbcKeycode = e1.getAttributeValue("k").toUpperCase();
			KeyCombination kc = accKeys.get(dbcKeycode);
			if (kc == null) {
				throw new Exception("Bad accelerator key code:" + dbcKeycode);
			}
			mi1.setAccelerator(kc);
			if (parent instanceof SCMenuBar)
				((SCMenuBar)parent).addAcceleratedMenuItem(mi1, kc);
		}
		if (e1.hasAttribute(Client.ICON)) {
			String icoName = e1.getAttributeValue(Client.ICON);
			IconResource icoRes = Client.getIcon(icoName);
			if (icoRes == null)
				throw new Exception("Unknown icon " + icoName);
			mi1.setGraphic(new ImageView(icoRes.getImage()));
		}
		lastGrayable = mi1;
	}
	
	private static String mnemonic(String text) {
		int i1 = text.indexOf('&');
		if (i1 != -1) {
			if (text.charAt(i1 + 1) != '&') return text.substring(0, i1) + '_' + text.substring(i1 + 1);
			return text.substring(0, i1) + text.substring(i1 + 1);
		}
		return text;
	}
	
	/**
	 * @return The parent DB/C Resource
	 */
	public Resource getParent() {
		return parent;
	}
}
