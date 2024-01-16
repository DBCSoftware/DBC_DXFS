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

package org.ptsw.sc.Table.Cell;

import org.ptsw.sc.Client;
import org.ptsw.sc.Table.RowData;
import org.ptsw.sc.Table.Item.CheckboxTableItem;

import javafx.beans.value.ObservableValue;
import javafx.geometry.Pos;
import javafx.scene.control.ContentDisplay;
import javafx.scene.control.cell.CheckBoxTableCell;
import javafx.util.Callback;

public class CheckboxCell extends CheckBoxTableCell<RowData, CheckboxTableItem> {

	public CheckboxCell(
            final Callback<Integer, ObservableValue<Boolean>> getSelectedProperty)
	{
		super(getSelectedProperty);
		setContentDisplay(ContentDisplay.GRAPHIC_ONLY);
		setEditable(true);
		if (Client.isDebug()) System.out.println("CheckboxCell created");
	}
	
	@Override
	protected void layoutChildren() {
		setAlignment(Pos.CENTER);
		super.layoutChildren();
	}
}
