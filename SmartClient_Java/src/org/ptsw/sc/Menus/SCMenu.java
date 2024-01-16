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

import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import org.ptsw.sc.Client;
import org.ptsw.sc.Resource;
import org.ptsw.sc.SmartClientApplication;
import org.ptsw.sc.xml.Element;

import javafx.collections.ObservableList;
import javafx.event.ActionEvent;
import javafx.event.EventHandler;
import javafx.scene.control.Menu;
import javafx.scene.control.MenuItem;

/**
 * Parent class for SCMenuBar and SCContextMenu
 */
public class SCMenu extends Resource {

	/**
	 * Map of all menu items for use by change and query commands
	 */
	protected final Map<String, MenuItem> menuItemMap = new HashMap<>();
	
	private final EventHandler<ActionEvent> menuActionHandler = new EventHandler<>() {
		@Override public void handle(ActionEvent event) {
			MenuItem mi1 = (MenuItem) event.getSource();
			Client.MenuMessage(getId(), mi1.getId());
		}
	};
	
	/* (non-Javadoc)
	 * @see com.dbcswc.sc.Resource#findTarget(org.ptsw.sc.Element)
	 */
	@Override
	protected QueryChangeTarget findTarget(Element e1) throws Exception {
		if (!e1.hasAttribute("n")) return null;
		QueryChangeTarget qct = new QueryChangeTarget();
		String item = e1.getAttributeValue("n");
		qct.target = menuItemMap.get(item);
		if (qct.target == null) {
			String m1 = String.format("Change directed at unknown menu item (%s)", item);
			throw new Exception(m1);
		}
		return qct;
	}

	/**
	 * @return the menuActionHandler
	 */
	EventHandler<ActionEvent> getMenuActionHandler() {
		return menuActionHandler;
	}

	public SCMenu(Element element) throws Exception {
		super(element);
	}

	@Override
	public void Close() {
		/* Do nothing here */
	}

	/**
	 * @return the menuItemMap
	 */
	Map<String, MenuItem> getMenuItemMap() {
		return menuItemMap;
	}

	@Override
	public void Hide() {
		/* Do nothing here */
	}

	protected static void setGrayAvailableMenuState(MenuItem n1, boolean state) {
		n1.setDisable(!state);
		if (n1 instanceof Menu) {
			ObservableList<MenuItem> list = ((Menu)n1).getItems();
			for (MenuItem item : list) {
				setGrayAvailableMenuState(item, state);
			}
		}
	}
	
	/**
	 * Need to override Resource.Change for a handful of things that
	 * need special treatment by Menus.
	 * 
	 * Using explicit Iterator below to avoid ConcurrentModificationException
	 * <p>
	 * GRAYALL, AVAILABLEALL
	 * @see org.ptsw.sc.Resource#Change(org.ptsw.sc.xml.Element)
	 */
	@Override
	public void Change(Element element) throws Exception {
		if (Client.IsTrace()) System.out.println("Entering SCMenu.Change");
		Collection<Object> echildren = element.getChildren();
		Iterator<Object> iter = echildren.iterator();
		while (iter.hasNext()) {
			Object obj1 = iter.next();
			if (obj1 instanceof Element) {
				final Element e1 = (Element) obj1;
				e1.name.intern();
				switch (e1.name) {
				/*
				 * Put here because these change commands apply ONLY to MenuBars and ContextMenus
				 */
				case HIDE:
				case SHOW:
					final QueryChangeTarget qct = findTarget(e1);
					if (qct.target instanceof MenuItem) {
						SmartClientApplication.Invoke(new Runnable() {
							@Override
							public void run() {
								((MenuItem) qct.target).setVisible(e1.name.equals(SHOW));
							}
						}, true);
					}
					iter.remove();
					break;
				}
			}
		}
		super.Change(element);
	}
}
