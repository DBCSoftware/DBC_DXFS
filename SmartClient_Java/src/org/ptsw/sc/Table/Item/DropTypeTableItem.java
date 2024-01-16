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

package org.ptsw.sc.Table.Item;

import org.ptsw.sc.ClientUtils;
import org.ptsw.sc.ListDrop.ListDropItem;
import org.ptsw.sc.ListDrop.SCDropbox;
import org.ptsw.sc.Table.Column;
import org.ptsw.sc.Table.RowData;

import javafx.scene.Node;
import javafx.scene.control.ComboBox;
import javafx.scene.paint.Color;

public class DropTypeTableItem extends BaseTableItem {

	protected SCDropbox cdropbox;
	protected ComboBox<ListDropItem> actualComboBox;
	
	@SuppressWarnings("unchecked")
	public DropTypeTableItem(Column<RowData, ? extends BaseTableItem> column) throws Exception {
		super(column);
		cdropbox = new SCDropbox(column.getPrefWidth() - 5f, column.getFont(),
				Color.BLACK);
		actualComboBox = (ComboBox<ListDropItem>)cdropbox.getControl();
		actualComboBox.setOnAction(column.getDropboxActionHandler());
	}

	public Node getControl() {
		return cdropbox.getControl();
	}

	/**
	 * @return Zero-based index of item selected in the ComboBox
	 */
	public int getSelectedIndex() {
		return cdropbox.getSelectionModel().getSelectedIndex();
	}

	public ListDropItem getSelectedItem() {
		return cdropbox.getSelectionModel().getSelectedItem();
	}

	public void locateLine(int i1) {
		if (0 <= i1 && i1 < actualComboBox.getItems().size()) actualComboBox.getSelectionModel().select(i1);
		else if (i1 == -1) {
			actualComboBox.getSelectionModel().clearSelection();
		}
	}
	
	public void locate(String text) {
		int i1 = ClientUtils.findMatchingText(actualComboBox.getItems(), text);
		if (i1 >= 0) actualComboBox.getSelectionModel().select(i1);
	}
}
