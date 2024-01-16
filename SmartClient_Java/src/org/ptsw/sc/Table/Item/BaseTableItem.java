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
import org.ptsw.sc.Table.Table;
import org.ptsw.sc.Table.Table.ColumnType;

import javafx.beans.property.ObjectProperty;
import javafx.beans.property.ReadOnlyObjectProperty;
import javafx.beans.property.SimpleObjectProperty;
import javafx.scene.paint.Color;

public class BaseTableItem {

	public enum TextColorSource {
		/* In priority order low to high */
		TABLE, COLUMN, ROW, CELL;  
	};

	protected final Column<RowData, ? extends BaseTableItem> column;
	protected final ColumnType columnType;
	private TextColorSource textColorSource;
	private ObjectProperty<Color> textColor = new SimpleObjectProperty<>();

	BaseTableItem(Column<RowData, ? extends BaseTableItem> column) {
		this.column = column;
		columnType = column.getColumnType();
		
		/*
		 * If column has a color honor it
		 */
		if (column.hasColor()) {
			textColorSource = TextColorSource.COLUMN;
			textColor.bind(column.textColorProperty());
		}
		else {
			textColorSource = TextColorSource.TABLE;
			textColor.bind(((Table<RowData>)column.getTableView()).textColorProperty());
		}
	}

	public ObjectProperty<Color> textColorProperty() { return textColor; }

	/**
	 * @return the textColorSource
	 */
	public TextColorSource getTextColorSource() {
		return textColorSource;
	}

	/**
	 * @param textColorSource the textColorSource to set
	 */
	public void setColorCell(Color color) {
		textColorSource = TextColorSource.CELL;
		textColor.unbind();
		textColor.set(color);
	}
	
	public void bindColor(TextColorSource source, ReadOnlyObjectProperty<Color> property) {
		textColorSource = source;
		textColor.bind(property);
	}

	public Column<RowData, ? extends BaseTableItem> getColumn() {
		return column;
	}
}
