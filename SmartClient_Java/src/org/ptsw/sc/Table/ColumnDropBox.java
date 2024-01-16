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

import org.ptsw.sc.Table.Item.DropBoxTableItem;
import org.ptsw.sc.xml.Element;

public class ColumnDropBox<S, T extends DropBoxTableItem> extends Column<RowData, DropBoxTableItem> {

	public ColumnDropBox(Element element, int colIndex) {
		super(element, colIndex);
	}

	@SuppressWarnings("rawtypes")
	public void insert(String data, int row) throws Exception {
		if (0 <= row && row < ((Table)getTableView()).getRowCount()) {
			DropBoxTableItem item = getCellData(row);
			item.insert(data);
		}
	}

	@SuppressWarnings("rawtypes")
	public void erase(int row) {
		if (0 <= row && row < ((Table)getTableView()).getRowCount()) {
			DropBoxTableItem item = getCellData(row);
			item.erase();
		}
	}

	@SuppressWarnings("rawtypes")
	public void insertafter(String data, int row) throws Exception {
		if (0 <= row && row < ((Table)getTableView()).getRowCount()) {
			DropBoxTableItem item = getCellData(row);
			item.insertafter(data);
		}
	}

	@SuppressWarnings("rawtypes")
	public void insertbefore(String data, int row) throws Exception {
		if (0 <= row && row < ((Table)getTableView()).getRowCount()) {
			DropBoxTableItem item = getCellData(row);
			item.insertbefore(data);
		}
	}

	@SuppressWarnings("rawtypes")
	public void insertlineafter(String data, int row) throws Exception {
		if (0 <= row && row < ((Table)getTableView()).getRowCount()) {
			DropBoxTableItem item = getCellData(row);
			item.insertlineafter(data);
		}
	}

	@SuppressWarnings("rawtypes")
	public void insertlinebefore(String data, int row) throws Exception {
		if (0 <= row && row < ((Table)getTableView()).getRowCount()) {
			DropBoxTableItem item = getCellData(row);
			item.insertlinebefore(data);
		}
	}

	@SuppressWarnings("rawtypes")
	public void delete(String data, int row) {
		if (0 <= row && row < ((Table)getTableView()).getRowCount()) {
			DropBoxTableItem item = getCellData(row);
			item.delete(data);
		}
	}

	@SuppressWarnings("rawtypes")
	public void deleteline(int i1, int row) {
		if (0 <= row && row < ((Table)getTableView()).getRowCount()) {
			DropBoxTableItem item = getCellData(row);
			item.deleteline(i1);
		}
	}

	@SuppressWarnings("rawtypes")
	public void locate(String data, int row) {
		if (0 <= row && row < ((Table)getTableView()).getRowCount()) {
			DropBoxTableItem item = getCellData(row);
			item.locate(data);
		}
	}

	@SuppressWarnings("rawtypes")
	public void locateLine(int lineIndex, int row) {
		if (0 <= row && row < ((Table)getTableView()).getRowCount()) {
			DropBoxTableItem item = getCellData(row);
			item.locateLine(lineIndex);
		}
	}

	@SuppressWarnings("rawtypes")
	public void minsert(ArrayList<String> datums, int row) throws Exception {
		if (0 <= row && row < ((Table)getTableView()).getRowCount()) {
			DropBoxTableItem item = getCellData(row);
			item.minsert(datums);
		}
	}

}
