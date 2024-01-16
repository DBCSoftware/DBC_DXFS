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

package org.ptsw.sc.Tabgroup;

import javafx.scene.control.Tab;
import javafx.scene.control.TabPane;
import javafx.scene.layout.Background;
import javafx.scene.layout.BackgroundFill;
import javafx.scene.layout.Border;
import javafx.scene.layout.BorderStroke;
import javafx.scene.layout.BorderStrokeStyle;
import javafx.scene.layout.CornerRadii;
import javafx.scene.paint.Color;

import org.ptsw.sc.PanelDialogControlFactory;
import org.ptsw.sc.Resource;
import org.ptsw.sc.RightClickable;
import org.ptsw.sc.xml.Element;

public class SCTabgroup extends TabPane implements RightClickable {

	@SuppressWarnings("unused")
	private final Resource parent;
	
	/**
	 * @param res
	 * @param hsize
	 * @param vsize
	 * @param element
	 * @param cf A PanelDialogControlFactory that carries in the font and text color of the surrounding resource
	 * @throws Exception
	 */
	public SCTabgroup(int hsize, int vsize, Element element, PanelDialogControlFactory cf)
			throws Exception {
		parent = cf.getParent();
		setPrefSize(hsize, vsize);
		setMinSize(USE_PREF_SIZE, USE_PREF_SIZE);
		setTabClosingPolicy(TabPane.TabClosingPolicy.UNAVAILABLE);
		setBackground(new Background(new BackgroundFill(Color.GHOSTWHITE, CornerRadii.EMPTY, null)));
		setBorder(new Border(new BorderStroke(Color.GRAY, BorderStrokeStyle.SOLID, null, null)));
		
		// <tabgroup hsize=456 vsize=385>
		//		<tab i=145 t="Tab 1"></tab>
		//		<tab i=150 t="Tab 2"></tab>
		//		<tab i=155 t="Tab 3"></tab>
		// </tabgroup>

		for (int i1 = 0; i1 < element.getChildrenCount(); i1++) {
			Object o1 = element.getChild(i1);
			if (o1 instanceof Element) {
				Element e2 = (Element)o1;
				if (e2.name.equals("tab")) getTabs().add(new SCTab(e2, new PanelDialogControlFactory(cf)));
			}
		}
//		addEventHandler(MouseEvent.ANY, new EventHandler<MouseEvent>() {
//			@Override
//			public void handle(MouseEvent event) {
//				event.consume();
//				getScene().getWindow().fireEvent(event);
//			}
//		});
	}

	@Override
	public void setRightClick(boolean state) {
		for (Tab tb1 : getTabs()) {
			((SCTab)tb1).setRightClick(state);
		}
	}

}
