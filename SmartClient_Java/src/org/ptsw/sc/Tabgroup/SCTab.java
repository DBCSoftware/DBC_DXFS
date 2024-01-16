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

import javafx.collections.ObservableList;
import javafx.scene.Node;
import javafx.scene.control.Tab;
import javafx.scene.input.MouseEvent;
import javafx.scene.layout.Pane;

import org.ptsw.sc.Client;
import org.ptsw.sc.PanelDialogControlFactory;
import org.ptsw.sc.Resource;
import org.ptsw.sc.RightClickable;
import org.ptsw.sc.xml.Element;

public class SCTab extends Tab implements RightClickable {

	private boolean reportRightClicks;
	private final Resource parent;
	
	public SCTab(Element element, PanelDialogControlFactory cf) throws Exception {
		super(element.getAttributeValue("t"));
		parent = cf.getParent();
		setId(element.getAttributeValue("i"));
		setContent(new Pane());
		getContent().setId(getId());
		getContent().addEventFilter(MouseEvent.ANY, (event) -> {
				if (reportRightClicks && Resource.isRightMouseEventReportable(event)) {
					Client.ControlButtonEvent(parent.getId(), SCTab.this.getContent(), event);
				}
			}
		);
		if (element.hasChildren()) {
			ObservableList<Node> children = ((Pane)getContent()).getChildren();
			children.setAll(cf.createNodes(element.getChildren()));
		}
	}

	@Override
	public void setRightClick(boolean state) {
		reportRightClicks = state;
	}

	public void requestFocus() {
		getTabPane().getSelectionModel().select(this);
	}
}
