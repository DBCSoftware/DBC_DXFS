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

import org.ptsw.sc.Client;
import org.ptsw.sc.ListDrop.ListDropItem;
import org.ptsw.sc.Table.Column;
import org.ptsw.sc.Table.ColumnCDropBox;
import org.ptsw.sc.Table.RowData;

import javafx.collections.ListChangeListener;
import javafx.collections.ObservableList;

public class CDropBoxTableItem extends DropTypeTableItem {

	private final ListChangeListener<ListDropItem> masterChangeListener = new ListChangeListener<>() {
		@Override public void onChanged(ListChangeListener.Change<? extends ListDropItem> change) {
		    while (change.next()) {
		    	if (Client.isDebug()) System.out.println("Was permutated? " + change.wasPermutated());
		        if (change.wasRemoved()) {
		        	if (Client.isDebug()) System.out.println("Was removed? true");
			        actualComboBox.getItems().removeAll(change.getRemoved());
		        }
		        if (change.wasAdded()) {
		        	if (Client.isDebug()) System.out.println("Was added? true");
			        actualComboBox.getItems().addAll(change.getAddedSubList());
		        }
		        if (Client.isDebug()) System.out.println("Was updated? " + change.wasUpdated());
		    }
		}
	};
	
	/**
	 * @param column
	 * @throws Exception 
	 */
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public CDropBoxTableItem(Column<RowData, ? extends BaseTableItem> column) throws Exception {
		super(column);
		try {
			ColumnCDropBox cColumn = (ColumnCDropBox)column;
			ObservableList<ListDropItem> masterOList = cColumn.getCdropboxList();
			actualComboBox.getItems().setAll(masterOList);
			masterOList.addListener(masterChangeListener);
		} catch (Exception e) {
			if (Client.isDebug()) e.printStackTrace();
		}
	}

	public void erase() {
		cdropbox.getSelectionModel().clearSelection();
	}

	/**
	 * Does not actually remove any line, just tries to deal
	 * with selection.
	 * When the master list line is removed, it will happen here too.
	 * 
	 * @param i1 Line number, zero-based
	 */
	public void deleteline(int i1) {
		int selectedindex = actualComboBox.getSelectionModel().getSelectedIndex();
		if (0 <= i1 && i1 < actualComboBox.getItems().size()) {
			if (i1 == selectedindex) actualComboBox.getSelectionModel().clearSelection();
		}
	}

}
