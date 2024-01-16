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

import org.ptsw.sc.Client;
import org.ptsw.sc.SmartClientApplication;
import org.ptsw.sc.xml.Element;

import javafx.collections.ObservableList;
import javafx.scene.control.ContextMenu;
import javafx.scene.control.MenuItem;

public class SCContextMenu extends SCMenu {

	public SCContextMenu(final Element element) throws Exception {
		super(element);
		assert element != null && element.hasAttribute(Client.POPUPMENU);
		if (Client.IsTrace()) System.out.println("At top of SCContextMenu<>");
		prepException = null;
		final MenuControlFactory cf = new MenuControlFactory(this);
		SmartClientApplication.Invoke(new Runnable() {
			@Override
			public void run() {
				try {
					content = new ContextMenu();
					((ContextMenu)content).getItems().setAll(cf.createNodes(element.getChildren()));
				} catch (Exception e) {
					prepException = e;
					element.getChildren().clear();
				}
			}
		}, true);
		setId(element.getAttributeValue(Client.POPUPMENU));
		if (prepException != null) throw prepException;
	}

	@Override
	public void Hide() {
		SmartClientApplication.Invoke(new Runnable() {
			@Override
			public void run() {
				((ContextMenu)content).hide();
			}
		}, true);
		visibleOnWindow = null;
	}

	/**
	 * Need to override SCMenu.Change for a couple of things that
	 * need special treatment by MenuBars.
	 * <p>
	 * GRAYALL, AVAILABLEALL
	 * @see com.dbcswc.sc.SCMenu#Change(org.ptsw.sc.xml.Element)
	 */
	@Override
	public void Change(Element element) throws Exception {
		if (Client.IsTrace()) System.out.println("Entering SCContextMenu.Change");
		Collection<Object> echildren = element.getChildren();
		for (Object obj1 : echildren) {
			if (obj1 instanceof Element) {
				final Element e1 = (Element) obj1;
				e1.name.intern();
				switch (e1.name) {
				case AVAILABLEALL:
				case GRAYALL:
					final boolean state = e1.name.equals(AVAILABLEALL);
					/*
					 * Must recurse down through items here 
					 */
					SmartClientApplication.Invoke(new Runnable() {
						@Override
						public void run() {
							ObservableList<MenuItem> list = ((ContextMenu)content).getItems();
							for (MenuItem n1 : list) {
								setGrayAvailableMenuState(n1, state);
							}
						}
					}, true);
					echildren.remove(obj1);
					break;
					
				}
			}
		}
		super.Change(element);
	}
}
