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

import org.ptsw.sc.Table.Column;
import org.ptsw.sc.Table.RowData;

import javafx.beans.property.BooleanProperty;
import javafx.beans.property.SimpleBooleanProperty;
import javafx.beans.value.ChangeListener;
import javafx.beans.value.ObservableValue;

public class CheckboxTableItem extends BaseTableItem {

	private BooleanProperty checked = new SimpleBooleanProperty();
	private final RowData rowData;
	
	@SuppressWarnings({ })
	public CheckboxTableItem(RowData rowData, final Column<RowData, ? extends BaseTableItem> column) {
		super(column);
		this.rowData = rowData;
		checkedProperty().addListener(new ChangeListener<Boolean>() {
			@Override
			public void changed(ObservableValue<? extends Boolean> observable,
					Boolean oldValue, Boolean newValue) {
				if (!column.ignoreItem) column.checkedChanged(CheckboxTableItem.this.rowData, newValue);
				else column.ignoreItem = false;
			}
		});
	}
	
	public BooleanProperty checkedProperty() { return checked; }

	public void setChecked(boolean value) {
		checked.setValue(value);
	}
	
	public boolean getChecked() { return checked.get(); }
	
	@Override
	public String toString() {
		return checked.toString();
	}
}
