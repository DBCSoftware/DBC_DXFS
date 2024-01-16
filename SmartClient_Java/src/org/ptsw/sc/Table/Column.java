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

import javafx.beans.property.ObjectProperty;
import javafx.beans.property.ReadOnlyObjectProperty;
import javafx.beans.property.SimpleObjectProperty;
import javafx.beans.value.ChangeListener;
import javafx.beans.value.ObservableValue;
import javafx.event.ActionEvent;
import javafx.event.EventHandler;
import javafx.scene.Node;
import javafx.scene.control.TableCell;
import javafx.scene.control.TableColumn;
import javafx.scene.control.TableView;
import javafx.scene.layout.Region;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;
import javafx.scene.text.TextAlignment;
import javafx.util.Callback;

import org.ptsw.sc.Client;
import org.ptsw.sc.Resource;
import org.ptsw.sc.ListDrop.DropItem;
import org.ptsw.sc.ListDrop.InsertOrderable;
import org.ptsw.sc.ListDrop.ListDropItem;
import org.ptsw.sc.Table.Cell.CDropboxCell;
import org.ptsw.sc.Table.Cell.DropboxCell;
import org.ptsw.sc.Table.Cell.PopboxCell;
import org.ptsw.sc.Table.Item.BaseTableItem;
import org.ptsw.sc.Table.Item.CheckboxTableItem;
import org.ptsw.sc.Table.Item.DropTypeTableItem;
import org.ptsw.sc.Table.Item.TextTableItem;
import org.ptsw.sc.Table.Table.ColumnType;
import org.ptsw.sc.xml.Element;

public class Column<S, T extends BaseTableItem> extends TableColumn<RowData, T>
{
	/**
	 * Just use BLACK for now.
	 * We will revisit the text color thing at a later time
	 */
	public static final ReadOnlyObjectProperty<Color> defaultTextColor
			= new SimpleObjectProperty<>(Color.BLACK);

	protected final ColumnType columnType;
	protected final int columnIndex;
	protected final ObjectProperty<Color> textColor = new SimpleObjectProperty<>(null);
	protected Table<RowData> table;
	protected Font tableFont;
	
	/**
	 * Used to prevent ITEM messages from being sent
	 * when the change is programmatic
	 */
	public boolean ignoreItem;
	
	/**
	 * These next three are used by both CDROPBOX and DROPBOX columns
	 */
	protected EventHandler<ActionEvent> dropTypeBoxActionHandler;
	protected InsertOrderable.Mode insertMode = InsertOrderable.Mode.ALPHA;
	public final Callback<String, ListDropItem> createListDropItem = new Callback<>()
	{
		@Override public ListDropItem call(String text) {
			ListDropItem item = new DropItem(defaultTextColor, getFont(), text, null);
			return item;
		}
	};

	/**
	 * DB/C Does not allow the justification of an edit column to change
	 */
	private final TextAlignment textAlignment;
	
	/**
	 * DB/C Does not allow the MaxChars of an LEDIT column to change
	 */
	private final int maxCharacters;
	
	/**
	 * The MASK to use for an FEDIT column
	 */
	private final String fmask;
	
	public Column(Element element, int colIndex) {
		super(element.getAttributeValue("t"));
		this.columnType = ColumnType.fromInteger(Integer.parseInt(element.getAttributeValue("c")));
		int width = Integer.parseInt(element.getAttributeValue("h"));
		columnIndex = colIndex;
		setId(element.getAttributeValue("i"));
		setPrefWidth(width);
		setMinWidth(Region.USE_PREF_SIZE);
		setResizable(false);
		setSortable(false);
		if (element.hasAttribute("justify")) {
			String js = element.getAttributeValue("justify");
			switch (js) {
			case "right":
				textAlignment = TextAlignment.RIGHT;
				break;
			case "center":
				textAlignment = TextAlignment.CENTER;
				break;
			default:
				textAlignment = TextAlignment.LEFT;
				break;
			}
		}
		else textAlignment = TextAlignment.LEFT;
		/**
		 * If this is an LEDIT column, we set the maxChars. It cannot be changed once the
		 * table is prepped.
		 */
		if (this.columnType == ColumnType.LEDIT && element.hasAttribute("p3")) {
			maxCharacters = Integer.parseInt(element.getAttributeValue("p3"));
		}
		else maxCharacters = Integer.MAX_VALUE;

		/**
		 * If this is an FEDIT column, we set the mask. It cannot be changed once the
		 * table is prepped.
		 */
		if (this.columnType == ColumnType.FEDIT && element.hasAttribute("p3")) {
			fmask = element.getAttributeValue("p3");
		}
		else fmask = null;
		tableViewProperty().addListener(new ChangeListener<TableView<RowData>>() {
			@Override public void changed(
					ObservableValue<? extends TableView<RowData>> observable,
					TableView<RowData> oldValue, TableView<RowData> newValue) {
				table = (Table<RowData>) newValue;
			}
		});
		
		if (this.columnType == ColumnType.CDROPBOX || this.columnType == ColumnType.DROPBOX) {
			if (element.hasAttribute("order")) {
				String ord = element.getAttributeValue("order");
				if (ord.equals("alpha")) /* Do Nothing */;
				else if (ord.equals(Resource.INSERT)) insertMode = InsertOrderable.Mode.ATEND;
			}
		}
	}

	public ObjectProperty<Color> textColorProperty() { return textColor; }
	
	/**
	 * @return the columnType
	 */
	public ColumnType getColumnType() {
		return columnType;
	}

	/**
	 * @return the columnIndex
	 */
	int getColumnIndex() {
		return columnIndex;
	}
	
	public Font getFont() {
		if (tableFont == null) tableFont = table.getFont();
		return tableFont;
	}

	/**
	 * Expects the row argument to be zero-based
	 * If the row part is -1, this is a request to change the heading text
	 * @param text
	 * @param row
	 */
	public void setText(String text, int row) {
		if (row < 0) {
			setText(text);
		}
		else {
			BaseTableItem cd1 = table.getCellData(row, columnIndex);
			if (cd1 instanceof TextTableItem) {
				ignoreItem = true;
				((TextTableItem)cd1).setText(text);
			}
		}
	}

	/**
	 * @return true if the value of the text color property is not null
	 */
	public boolean hasColor() {
		return textColor.get() != null;
	}

	public String getStatus(int row) {
		BaseTableItem tableItem = table.getCellData(row, columnIndex);
		if (tableItem instanceof TextTableItem) return ((TextTableItem)tableItem).getText();
		if (tableItem instanceof CheckboxTableItem) return ((CheckboxTableItem)tableItem).getChecked() ? "Y" : "N";
		if (tableItem instanceof DropTypeTableItem) {
			if (table.getParentResource().isLineOn()) {
				return String.format("%5d", ((DropTypeTableItem)tableItem).getSelectedIndex() + 1);
			}
			ListDropItem ldi = ((DropTypeTableItem)tableItem).getSelectedItem();
			return ldi != null ? ldi.getText() : "";
		}
		return "";
	}

	public void setCheckboxState(boolean state, int row) {
		BaseTableItem cd1 = table.getCellData(row, columnIndex);
		if (cd1 instanceof CheckboxTableItem) {
			ignoreItem = true;
			((CheckboxTableItem)cd1).setChecked(state);
		}
	}

	public void checkedChanged(RowData rowData, Boolean newValue) {
		int rowIndex = table.getRowIndex(rowData);
		table.checkedChanged(rowIndex, getId(), newValue);
	}

	public void editChanged(RowData rowData, String newValue) {
		int rowIndex = table.getRowIndex(rowData);
		table.editChanged(rowIndex, getId(), newValue);
	}

	/**
	 * @return the textAlignment
	 */
	public TextAlignment getTextAlignment() {
		return textAlignment;
	}

	/**
	 * @return the maxCharacters
	 */
	public int getMaxCharacters() {
		return maxCharacters;
	}

	/**
	 * @return the fmask
	 */
	public String getFmask() {
		return fmask;
	}

	private EventHandler<ActionEvent> popboxActionHandler;
	
	@SuppressWarnings("rawtypes")
	EventHandler<ActionEvent> getPopboxActionHandler() {
		if (popboxActionHandler == null) {
			final String resName = table.getParentResource().getId();
			popboxActionHandler = new EventHandler<>() {
				@Override
				public void handle(ActionEvent event) {
					Node n2 = ((Node)event.getSource()).getParent();
					while (!(n2 instanceof PopboxCell)) { n2 = n2.getParent(); }
					int row = ((TableCell)n2).getIndex();
					Client.TablePopboxButtonPushed(resName, getId(), String.format("%5d", row + 1));
				}
			};
		}
		return popboxActionHandler;
	}
	
	@SuppressWarnings({ "rawtypes"})
	public EventHandler<ActionEvent> getDropboxActionHandler() {
		if (dropTypeBoxActionHandler == null) {
			dropTypeBoxActionHandler = new EventHandler<>() {
				@Override public void handle(ActionEvent event) {
					if (table.getParentResource().isItemOn()) {
						if (ignoreItem) return;
						Node n2 = ((Node)event.getSource()).getParent();
						while (!(n2 instanceof CDropboxCell) && !(n2 instanceof DropboxCell)) {
							n2 = n2.getParent();
						}
						int rowIndex = ((TableCell)n2).getIndex();
						DropTypeTableItem item = (DropTypeTableItem) ((TableCell)n2).getItem();
						int selectedIndex = item.getSelectedIndex();
						ListDropItem ldi = item.getSelectedItem();
						table.dropboxChanged(rowIndex, getId(), selectedIndex, ldi);
					}
				}
			};
		}
		return dropTypeBoxActionHandler;
	}

	/**
	 * @return the insertMode
	 */
	public InsertOrderable.Mode getInsertMode() {
		return insertMode;
	}
}
