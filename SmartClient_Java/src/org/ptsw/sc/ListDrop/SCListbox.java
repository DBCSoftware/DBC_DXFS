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

import org.ptsw.sc.Resource;

import javafx.collections.ObservableList;
import javafx.scene.control.ListView;
import javafx.scene.control.SelectionModel;
import javafx.scene.layout.Region;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;

/**
 * Represents a DB/C LISTBOX, single select
 * 
 * Readonly attribute is ignored for this control
 */
public class SCListbox extends SCListDropBase
{

	protected final ListView<ListDropItem> theBox;
	
	@SuppressWarnings("unchecked")
	public
	SCListbox(final Resource parent, String item, int hsize, int vsize, final Font font, final Color textColor) throws Exception {
		super(new ListView<ListDropItem>(), parent, textColor, font);
		theBox = (ListView<ListDropItem>) super.control;
		theBox.setPrefSize(hsize, vsize);
		theBox.setMinSize(Region.USE_PREF_SIZE, Region.USE_PREF_SIZE);
		theBox.setMaxSize(Region.USE_PREF_SIZE, Region.USE_PREF_SIZE);
		theBox.setId(item);
		theBox.setEditable(false);
		theBox.disabledProperty().addListener(disabledListener);
		droplistManager = new ItemListManager(theBox.getItems(), newCell);
		theBox.setCellFactory(cellFactory);
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.SCListDropBase#getItems()
	 */
	@Override
	public ObservableList<ListDropItem> getItems() {
		return theBox.getItems();
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.SCListDropBase#getSelectionModel()
	 */
	@Override
	public SelectionModel<ListDropItem> getSelectionModel() {
		return theBox.getSelectionModel();
	}

}
