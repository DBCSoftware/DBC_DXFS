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

package org.ptsw.sc.Table;

import java.util.ArrayList;

import org.ptsw.sc.ClientUtils;
import org.ptsw.sc.ListDrop.InsertOrderable;
import org.ptsw.sc.ListDrop.ItemListManager;
import org.ptsw.sc.ListDrop.ListDropItem;
import org.ptsw.sc.Resource.QueryChangeTarget;
import org.ptsw.sc.Table.Item.CDropBoxTableItem;
import org.ptsw.sc.xml.Element;

import javafx.collections.FXCollections;
import javafx.collections.ObservableList;

/**
 * Sub class of Column specially for CDropbox columns
 */
public class ColumnCDropBox<S, T extends CDropBoxTableItem> extends Column<RowData, CDropBoxTableItem>
{

	/**
	 * Observed for changes by items
	 */
	private final ObservableList<ListDropItem> cdropboxList = FXCollections.<ListDropItem>observableArrayList();

	private final ItemListManager listManager;
	
	public ColumnCDropBox(Element element, int colIndex) {
		super(element, colIndex);
		listManager = new ItemListManager(cdropboxList, createListDropItem);
		if (insertMode == InsertOrderable.Mode.ATEND) listManager.setInsertOrder();
	}

	/**
	 * @return the cdropboxList
	 */
	public ObservableList<ListDropItem> getCdropboxList() {
		return cdropboxList;
	}
	
	public void insert(String data) throws Exception {
		listManager.insert(data);
	}

	public void insertbefore(String data) throws Exception {
		listManager.insertbefore(data);
	}

	public void insertafter(String data) throws Exception {
		listManager.insertafter(data);
	}

	public void insertlinebefore(String data) throws Exception {
		listManager.insertlinebefore(data);
	}

	public void insertlineafter(String data) throws Exception {
		listManager.insertlineafter(data);
	}

	public void locate(String text, QueryChangeTarget qct) {
		ignoreItem = true;
		CDropBoxTableItem tableItem;
		if (qct.getRow() >= 0) { /* Just one row */
			tableItem = (CDropBoxTableItem) table.getCellData(qct.getRow(), columnIndex);
			tableItem.locate(text);
		}
		else { /* Every cell in this column */
			int rowCount = table.getRowCount();
			for (int row = 0; row < rowCount; row++) {
				tableItem = (CDropBoxTableItem) table.getCellData(row, columnIndex);
				tableItem.locate(text);
			}
		}
		ignoreItem = false;
	}

	public void locateLine(int lineIndex, QueryChangeTarget qct) {
		ignoreItem = true;
		CDropBoxTableItem tableItem;
		if (qct.getRow() >= 0) { /* Just one row */
			tableItem = (CDropBoxTableItem) table.getCellData(qct.getRow(), columnIndex);
			tableItem.locateLine(lineIndex);
		}
		else { /* Every cell in this column */
			int rowCount = table.getRowCount();
			for (int row = 0; row < rowCount; row++) {
				tableItem = (CDropBoxTableItem) table.getCellData(row, columnIndex);
				tableItem.locateLine(lineIndex);
			}
		}
		ignoreItem = false;
	}

	public void erase() {
		ignoreItem = true;
		listManager.erase();
		CDropBoxTableItem tableItem;
		int rowCount = table.getRowCount();
		for (int row = 0; row < rowCount; row++) {
			tableItem = (CDropBoxTableItem) table.getCellData(row, columnIndex);
			tableItem.erase();
		}
		ignoreItem = false;
	}
	
	public void delete(String text) {
		int i1 = ClientUtils.findMatchingText(cdropboxList, text);
		if (i1 >= 0) deleteline(i1);
	}
	
	/**
	 * Delete a line by its index
	 * DB/C rule is that if the removed item was the selected item, then
	 * the selection is cleared.
	 * @param lineIndex zero-based index
	 */
	public void deleteline(int lineIndex) {
		ignoreItem = true;
		CDropBoxTableItem tableItem;
		int rowCount = table.getRowCount();
		for (int row = 0; row < rowCount; row++) {
			tableItem = (CDropBoxTableItem) table.getCellData(row, columnIndex);
			tableItem.deleteline(lineIndex);
		}
		if (0 <= lineIndex && lineIndex < cdropboxList.size()) {
			cdropboxList.remove(lineIndex);
		}
		ignoreItem = false;
	}

	public void minsert(ArrayList<String> datums) throws Exception {
		listManager.minsert(datums);
	}
}
