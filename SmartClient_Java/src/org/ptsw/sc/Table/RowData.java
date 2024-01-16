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

import org.ptsw.sc.Table.Item.BaseTableItem;
import org.ptsw.sc.Table.Item.CDropBoxTableItem;
import org.ptsw.sc.Table.Item.CheckboxTableItem;
import org.ptsw.sc.Table.Item.DropBoxTableItem;
import org.ptsw.sc.Table.Item.EditTableItem;
import org.ptsw.sc.Table.Item.TextTableItem;
import org.ptsw.sc.Table.Table.ColumnType;

import javafx.beans.property.ObjectProperty;
import javafx.beans.property.SimpleObjectProperty;
import javafx.collections.ObservableList;
import javafx.scene.control.TableColumn;
import javafx.scene.paint.Color;

public class RowData {

	private final BaseTableItem[] data;
	private ObjectProperty<Color> textColor = new SimpleObjectProperty<>(null);
	
	@SuppressWarnings({ "rawtypes", "unchecked" })
	RowData(ObservableList<TableColumn<RowData, ?>> columns) throws Exception {
		int numCols = columns.size();
		data = new BaseTableItem[numCols];
		for (int colIndex = 0; colIndex < numCols; colIndex++) {
			Column<RowData, ? extends BaseTableItem> column;
			ColumnType colType = ((Column)columns.get(colIndex)).getColumnType();
			switch (colType) {
			case CVTEXT:
			case RVTEXT:
			case VTEXT:
			case POPBOX:
				column = (Column<RowData, TextTableItem>)columns.get(colIndex);
				data[colIndex] = new TextTableItem(column);
				break;
			case CHECKBOX:
				column = (Column<RowData, CheckboxTableItem>)columns.get(colIndex);
				data[colIndex] = new CheckboxTableItem(this, column);
				break;
			case EDIT:
			case FEDIT:
			case LEDIT:
				column = (Column<RowData, EditTableItem>)columns.get(colIndex);
				data[colIndex] = new EditTableItem(this, column);
				break;
			case CDROPBOX:
				column = (ColumnCDropBox<RowData, CDropBoxTableItem>)columns.get(colIndex);
				data[colIndex] = new CDropBoxTableItem(column);
				break;
			case DROPBOX:
				column = (ColumnDropBox<RowData, DropBoxTableItem>)columns.get(colIndex);
				data[colIndex] = new DropBoxTableItem(column);
				break;
			default:
				throw new IllegalArgumentException(
						String.format("Unsupported column type %d", colType.getValue()));
			}
		}
	}

	BaseTableItem getCell(int colIndex) {
		BaseTableItem cellb = data[colIndex];
		return cellb;
	}
	
	/**
	 * @return true if the value of the text color property is not null
	 */
	boolean hasColor() {
		return textColor.get() != null;
	}
	
	ObjectProperty<Color> textColorProperty() { return textColor; }

}
