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

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.ptsw.sc.Client;
import org.ptsw.sc.FontMetrics;
import org.ptsw.sc.Resource;
import org.ptsw.sc.ListDrop.ListDropItem;
import org.ptsw.sc.Table.Item.BaseTableItem;
import org.ptsw.sc.xml.Element;

import javafx.beans.property.ObjectProperty;
import javafx.beans.property.SimpleObjectProperty;
import javafx.scene.control.TableColumn;
import javafx.scene.control.TableView;
import javafx.scene.layout.Region;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;

public class Table<R extends RowData> extends TableView<RowData> {

	private static final Color defaultTextColor = Color.BLACK;
	
	public enum ColumnType {
		EDIT(6), CHECKBOX(7), 
		DROPBOX(20), VTEXT (25),
		RVTEXT (26),
		FEDIT(29),	/* NOT SUPPORTED, ACCEPTED AND TREATED LIKE 'EDIT' */
		LEDIT(33),
		CVTEXT (47),
		POPBOX(50), CDROPBOX(51);

		private int value;

        private ColumnType(int value) {
        	this.value = value;
        }
        
        public int getValue() { return value; }
        
        public static ColumnType fromInteger(int x) {
            switch(x) {
            case 6:
            	return EDIT;
            case 7:
            	return CHECKBOX;
            case 20:
            	return DROPBOX;
            case 25:
                return VTEXT;
            case 26:
                return RVTEXT;
            case 29:
            	return FEDIT;
            case 33:
            	return LEDIT;
            case 47:
                return CVTEXT;
            case 50:
            	return POPBOX;
            case 51:
            	return CDROPBOX;
            default:
            	throw new IllegalArgumentException(
						String.format("Unsupported Table.ColumnType %d", x));            
            }
        }
	};

	private ObjectProperty<Color> textColor = new SimpleObjectProperty<>(defaultTextColor);
	private final Font font;
	private final FontMetrics fontMetrics;
	private final Resource parent;
	
	public Table(Resource parent, String item, int hsize, int vsize, Element e1, Font font, Color color) throws Exception {
		super();
		this.font = font;
		fontMetrics = new FontMetrics(font);
		this.parent = parent;
		textColor.set(color);
		setId(item);
		setPrefSize(hsize, vsize);
		setMinSize(Region.USE_PREF_SIZE, Region.USE_PREF_SIZE);
		getSelectionModel().setCellSelectionEnabled(true);
		setEditable(true);
		ColumnFactory.createColumns(this, e1);
	}

	public void addRow() throws Exception {
		getItems().add(new RowData(getColumns()));
	}

	public void deleteAllRows() {
		getItems().clear();
	}

	public int getRowCount() {
		return getItems().size();
	}

	/**
	 * @return the textColor
	 */
	public Color getTextColor() {
		return textColor.get();
	}

	/**
	 * If newColor is null, will set text color to the default ('Black')
	 * @param newColor the text Color to set
	 */
	public void setTextColor(Color newColor) {
		if (newColor != null) textColor.set(newColor);
		else textColor.set(defaultTextColor); /* 'NONE' from DB/C */
	}
	
	public ObjectProperty<Color> textColorProperty() { return textColor; }

	@SuppressWarnings("rawtypes")
	public Map<? extends String, ? extends Column> getColumnsMap() {
		List<TableColumn<RowData,?>> cols = getColumns();
		Map<String,Column<?,?>> map = new HashMap<>(cols.size());
		for (TableColumn tc1 : cols) {
			map.put(tc1.getId(), (Column<?, ?>) tc1);
		}
		return map;
	}

	/**
	 * @return the font
	 */
	public Font getFont() {
		return font;
	}

	/**
	 * @return the FontMetrics
	 */
	FontMetrics getFontMetrics() {
		return fontMetrics;
	}

	/**
	 * @param row Zero-based index of row to be removed
	 */
	public void deleteRow(int row) {
		getItems().remove(row);
	}

	int getRowIndex(RowData rd1) {
		return getItems().indexOf(rd1);
	}
	
	RowData getRowData(int row) { return getItems().get(row); }
	
	BaseTableItem getCellData(int row, int col) {
		RowData rd1 = getItems().get(row);
		return rd1.getCell(col);
	}
	

	/**
	 * @param row Zero-based index of row to be added
	 * @throws Exception 
	 */
	public void insertRowBefore(int row) throws Exception {
		RowData newRow = new RowData(getColumns());
		getItems().add(row, newRow);
	}

	void checkedChanged(int rowIndex, String columnId, Boolean newValue) {
		if (parent.isItemOn()) {
			final StringBuilder sb1 = new StringBuilder(6);
			sb1.append(String.format("%5d", rowIndex + 1));
			sb1.append(newValue ? "Y" : "N");
			Client.ItemMessage(parent.getId(), columnId, sb1.toString());
		}
	}

	@SuppressWarnings("rawtypes")
	public Column getColumnByText(String t2) {
		for (TableColumn<RowData, ?> col : getColumns()) {
			if (col.getText().equals(t2)) return (Column) col;
		}
		return null;
	}

	void editChanged(int rowIndex, String columnId, String newValue) {
		if (parent.isItemOn()) {
			final StringBuilder sb1 = new StringBuilder(6);
			sb1.append(String.format("%5d", rowIndex + 1));
			sb1.append(newValue);
			Client.ItemMessage(parent.getId(), columnId, sb1.toString());
		}
	}

	/**
	 * @return the parent
	 */
	Resource getParentResource() {
		return parent;
	}

	/**
	 * This should not be called if ITEM is OFF, it assumes it is ON
	 * 
	 * @param rowIndex Zero-based index of the Cell's row
	 * @param columnId	Column ID, as db/c knows it
	 * @param index Zero-based index of item selected in the dropbox
	 * @param ldi Item selected in the dropbox
	 */
	public void dropboxChanged(int rowIndex, String columnId, int index, ListDropItem ldi) {
		final StringBuilder sb1 = new StringBuilder(16);
		sb1.append(String.format("%5d", rowIndex + 1));
		if (parent.isLineOn()) sb1.append(String.format("%5d", index + 1));
		else sb1.append(ldi.getText());
		Client.ItemMessage(parent.getId(), columnId, sb1.toString());
	}
}
