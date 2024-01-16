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

import java.util.ArrayList;

import org.ptsw.sc.ClientUtils;
import org.ptsw.sc.ListDrop.InsertOrderable;
import org.ptsw.sc.ListDrop.ItemListManager;
import org.ptsw.sc.Table.Column;
import org.ptsw.sc.Table.RowData;

public class DropBoxTableItem extends DropTypeTableItem {

	private final ItemListManager listManager;

	public DropBoxTableItem(Column<RowData, ? extends BaseTableItem> column) throws Exception {
		super(column);
		listManager = new ItemListManager(actualComboBox.getItems(), column.createListDropItem);
		if (column.getInsertMode() == InsertOrderable.Mode.ATEND) listManager.setInsertOrder();
	}

	public void insert(String data) throws Exception {
		listManager.insert(data);
	}

	public void erase() {
		actualComboBox.getSelectionModel().clearSelection();
		listManager.erase();
	}

	public void insertafter(String data) throws Exception {
		listManager.insertafter(data);
	}

	public void insertbefore(String data) throws Exception {
		listManager.insertbefore(data);
	}

	public void insertlinebefore(String data) throws Exception {
		listManager.insertlinebefore(data);
	}

	public void insertlineafter(String data) throws Exception {
		listManager.insertlineafter(data);
	}
	
	public void delete(String text) {
		int i1 = ClientUtils.findMatchingText(actualComboBox.getItems(), text);
		if (i1 >= 0) deleteline(i1);
	}

	public void deleteline(int lineIndex) {
		actualComboBox.getItems().remove(lineIndex);
	}

	public void minsert(ArrayList<String> datums) throws Exception {
		listManager.minsert(datums);
	}
}
