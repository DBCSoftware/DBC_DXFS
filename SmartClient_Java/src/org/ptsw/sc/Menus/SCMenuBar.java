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
import java.util.Map;

import org.ptsw.sc.Client;
import org.ptsw.sc.SCWindow;
import org.ptsw.sc.SmartClientApplication;
import org.ptsw.sc.WindowDevice;
import org.ptsw.sc.xml.Element;

import javafx.collections.ObservableList;
import javafx.event.EventHandler;
import javafx.scene.Parent;
import javafx.scene.control.Menu;
import javafx.scene.control.MenuBar;
import javafx.scene.control.MenuItem;
import javafx.scene.input.KeyCode;
import javafx.scene.input.KeyCombination;
import javafx.scene.input.KeyEvent;

/**
 * Note that because of the way that Menu objects and MenuBar objects interact, we are
 * unable to support a main menu item that has no children. It will never send a MENU message.
 */
public class SCMenuBar extends SCMenu {

	/**
	 * Map of menu items that are accelerated with one of the keys that is a problem
	 */
	private final Map<KeyCode, MenuItem> accelKeyMap = new HashMap<>();
	
	/**
	 * Certain keys are a problem for us when used as accelerator keys.
	 * We need to consume the key event and fire the action on the menu item
	 * This filter is added to the root of the scene graph.
	 */
	private final EventHandler<KeyEvent> keyEventFilter = new EventHandler<>() {
		@Override
		public void handle(KeyEvent event) {
			KeyCode kc = event.getCode();
			if (isKeyOfInterest(kc))
			{
				MenuItem mi1 = accelKeyMap.get(kc);
				if (mi1 != null) {
					mi1.fire();
					event.consume();
				}
			}
		}
	};
	
	private static boolean isKeyOfInterest(KeyCode keyCode) {
		return keyCode.equals(KeyCode.HOME) || keyCode.equals(KeyCode.END)
				|| keyCode.equals(KeyCode.PAGE_UP) || keyCode.equals(KeyCode.PAGE_DOWN);
	}
	
	public SCMenuBar(final Element element) throws Exception {
		super(element);
		content = new MenuBar();
		setId(element.getAttributeValue(Client.MENU));
		prepException = null;
		final MenuControlFactory cf = new MenuControlFactory(this);
		SmartClientApplication.Invoke(new Runnable() {
			@Override
			public void run() {
				try {
					((MenuBar)content).getMenus().setAll(cf.createMenuNodes(element.getChildren()));
				} catch (Exception e) {
					prepException = e;
					element.getChildren().clear();
				}
			}
		}, true);
		if (prepException != null) throw prepException;
	}

	@Override
	public void Close() {
		Hide();
	}


	@Override
	public void Hide() {
		if (visibleOnWindow != null) visibleOnWindow.HideResource(this);
	}

	/**
	 * This is only used for Home, End, Pgup, Pgdn without any modifying keys
	 * @param mi1 The MenuItem
	 * @param kc
	 */
	void addAcceleratedMenuItem(MenuItem mi1, KeyCombination keyCombo) {
		KeyCode kc = KeyCode.getKeyCode(keyCombo.getName());
//		if (Client.IsDebug()) {
//			if (kc == null) System.err.println("SCMenuBar.addAcceleratedMenuItem, kc is null");
//			else System.out.println("SCMenuBar.addAcceleratedMenuItem, kc is " + kc);
//		}
		if (kc != null) accelKeyMap.put(kc, mi1);
	}

	@Override
	public void SetVisibleOn(WindowDevice sCWindow) {
		assert sCWindow == null || sCWindow instanceof SCWindow;
		Parent rootPane;
		if (sCWindow != null) {
			rootPane = sCWindow.getStage().getScene().getRoot();
			rootPane.addEventFilter(KeyEvent.KEY_PRESSED, keyEventFilter);
		}
		else {
			rootPane = visibleOnWindow.getStage().getScene().getRoot();
			rootPane.removeEventFilter(KeyEvent.KEY_PRESSED, keyEventFilter);
		}
		super.SetVisibleOn(sCWindow);
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
		if (Client.IsTrace()) System.out.println("Entering SCMenuBar.Change");
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
							ObservableList<Menu> list = ((MenuBar)content).getMenus();
							for (Menu n1 : list) {
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

	public boolean isAccelerator(KeyEvent event) {
		KeyCode kc = event.getCode();
		return accelKeyMap.containsKey(kc);
	}
}
