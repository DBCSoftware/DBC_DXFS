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

package org.ptsw.sc.ListDrop;

import org.ptsw.sc.Client;

import javafx.event.EventHandler;
import javafx.scene.control.ContentDisplay;
import javafx.scene.control.ListCell;
import javafx.scene.input.MouseButton;
import javafx.scene.input.MouseEvent;

/**
 * Objects of this class are used for the ListCells that are the visible
 * parts of ComboBoxes and ListViews (dropboxes and listboxes)
 */
public class SCListCell extends ListCell<ListDropItem> {

	private final SCListDropBase base;

	SCListCell(SCListDropBase base) {
		super();
		this.base = base;
		setContentDisplay(ContentDisplay.GRAPHIC_ONLY);
		
		/*
		 * TODO DO we ever send a PICK from a dropbox? If no then
		 * see about optimizing this so it's only for listboxes.
		 */
		addEventFilter(MouseEvent.MOUSE_CLICKED, new EventHandler<MouseEvent>() {
			@Override
			public void handle(MouseEvent event) {
				if (getGraphic() == null) return;
				if (event.getClickCount() == 2 && event.getButton() == MouseButton.PRIMARY) {
					ListDropItem li = SCListCell.this.getItem();
					if (Client.isDebug()) System.out.printf("D=PICK on '%s'\n", li.getText());
					Client.pickMessage(SCListCell.this.base.parentResource.getId(), SCListCell.this.base.getId(),
							(SCListCell.this.base.parentResource.isLineOn()
									? Integer.toString(getIndex() + 1) : li.getText())); 
				}
			}
		});
	}

	@Override
	protected void updateItem(ListDropItem item, boolean empty) {
		super.updateItem(item, empty); // DO NOT REMOVE, MUST BE CALLED!
		if (item == null || empty) {
			setGraphic(null);
		}
		else {
			setGraphic(item.getNode());
		}
	}
}
