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
import org.ptsw.sc.Resource;

import javafx.collections.ObservableList;
import javafx.scene.control.ComboBox;
import javafx.scene.control.SelectionModel;
import javafx.scene.layout.Region;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;

/**
 * Represents a DB/C DROPBOX
 * 
 * Readonly attribute is ignored for this control
 */
public final class SCDropbox extends SCListDropBase
	{

	private final ComboBox<ListDropItem> theBox;
	
	/**
	 * @param parent
	 * @param item
	 * @param hsize
	 * @param font
	 * @param textColor
	 * @throws Exception
	 */
	@SuppressWarnings("unchecked")
	public SCDropbox(final Resource parent, String item, int hsize, final Font font, final Color textColor)
			throws Exception
	{
		super(new ComboBox<ListDropItem>(), parent, textColor, font);
		theBox = (ComboBox<ListDropItem>) super.control;
		theBox.setId(item);
		init(hsize);
	}

	@SuppressWarnings("unchecked")
	public SCDropbox(double hsize, final Font font, final Color textColor) throws Exception
	{
		super(new ComboBox<ListDropItem>(), null, textColor, font);
		theBox = (ComboBox<ListDropItem>) super.control;
		init(hsize);
		if (Client.isDebug()) System.out.println("SCDropbox created");
	}

	private void init(double hsize) {
		theBox.setEditable(false);
		theBox.setCellFactory(cellFactory);
		theBox.setButtonCell(cellFactory.call(null));
		theBox.setMinWidth(Region.USE_PREF_SIZE);
		theBox.setPrefWidth(hsize);
		droplistManager = new ItemListManager(theBox.getItems(), newCell);
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
