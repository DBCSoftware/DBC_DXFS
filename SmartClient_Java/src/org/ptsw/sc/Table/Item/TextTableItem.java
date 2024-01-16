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

import javafx.beans.property.SimpleStringProperty;
import javafx.beans.property.StringProperty;
import javafx.scene.text.TextAlignment;

public class TextTableItem extends BaseTableItem {

	private StringProperty text = new SimpleStringProperty();
	private final TextAlignment textAlignment;
	
	@SuppressWarnings({ })
	public TextTableItem(Column<RowData, ? extends BaseTableItem> column) {
		super(column);
		switch (columnType) {
		case CVTEXT:
			textAlignment = TextAlignment.CENTER;
			break;
		case RVTEXT:
			textAlignment = TextAlignment.RIGHT;
			break;
		case VTEXT:
			textAlignment = TextAlignment.LEFT;
			break;
		case EDIT:
		case LEDIT:
			textAlignment = column.getTextAlignment();
			break;
		default:
			textAlignment = TextAlignment.LEFT;
			break;
		}
	}
	
	public TextAlignment getTextAlignment() {
		return textAlignment;
	}

	public StringProperty textProperty() { return text; }

	public void setText(String text2) { text.setValue(text2); }
	
	public String getText() { return text.get(); }
	
	@Override
	public String toString() {
		return text.get();
	}
}
