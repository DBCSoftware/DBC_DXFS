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

import org.ptsw.sc.FontMetrics;
import org.ptsw.sc.Resource.QueryChangeTarget;
import org.ptsw.sc.Table.Item.BaseTableItem;
import org.ptsw.sc.Table.Item.TextTableItem;
import org.ptsw.sc.Table.Item.BaseTableItem.TextColorSource;

import javafx.scene.control.Label;
import javafx.scene.paint.Color;
import javafx.scene.text.TextAlignment;

public final class CellUtilities {

	/**
	 * @param qct A QueryChangeTarget object containing the Table or Column target
	 * and a row index if applicable
	 * @param newColor Which can be null
	 */
	@SuppressWarnings("unchecked")
	public static void setTextColor(QueryChangeTarget qct, Color newColor) {
		if (qct.target instanceof Table) setTextColor((Table<RowData>)qct.target, qct.getRow(), newColor);
		else if (qct.target instanceof Column<?, ?>)
			setTextColor((Column<RowData, TextTableItem>)qct.target, qct.getRow(), newColor);
	}
	
	private static void setTextColor(Table<RowData> table, int row, Color newColor) {
		if (row < 0) table.setTextColor(newColor);
		else setTextColorRow(table, row, newColor);
	}

	private static void setTextColor(Column<RowData, TextTableItem> column, int row, Color newColor) {
		if (row < 0) setTextColorColumn(column, newColor);
		else setTextColorCell(column, row, newColor);
	}

	
	private static void setTextColorCell(Column<RowData, TextTableItem> column,
			int row, Color newColor) {
		BaseTableItem cell = column.getCellData(row);
		if (newColor != null) {
			cell.setColorCell(newColor);
		}
		else {
			Table<RowData> table = (Table<RowData>)column.getTableView();
			RowData rowData = table.getRowData(row);
			if (rowData.hasColor()) {
				cell.bindColor(TextColorSource.ROW, rowData.textColorProperty());
			}
			else {
				if (column.hasColor()) {
					cell.bindColor(TextColorSource.COLUMN, column.textColorProperty());
				}
				else {
					cell.bindColor(TextColorSource.TABLE, table.textColorProperty());
				}
			}
		}
	}

	private static void setTextColorColumn(Column<RowData, TextTableItem> column, Color newColor)
	{
		if (column.hasColor() && newColor != null) {
			column.textColorProperty().set(newColor);
		}
		else if (column.hasColor() && newColor == null) {
			Table<RowData> table = (Table<RowData>)column.getTableView();
			int numRows = table.getRowCount();
			for (int rowIndex = 0; rowIndex< numRows; rowIndex++) {
				BaseTableItem cell = table.getCellData(rowIndex, column.getColumnIndex());
				switch (cell.getTextColorSource()) {
				case CELL:
				case ROW:
					/* do nothing */
					break;
				case COLUMN:
				case TABLE: /* Should not happen but ignore */
					cell.bindColor(TextColorSource.TABLE, table.textColorProperty());
					break;
				}
			}
			column.textColorProperty().set(null);
		}
		else if (!column.hasColor() && newColor != null) {
			Table<RowData> table = (Table<RowData>)column.getTableView();
			int numRows = table.getRowCount();
			for (int rowIndex = 0; rowIndex< numRows; rowIndex++) {
				BaseTableItem cell = table.getCellData(rowIndex, column.getColumnIndex());
				switch (cell.getTextColorSource()) {
				case CELL:
				case ROW:
					/* do nothing */
					break;
				case COLUMN:
					/* Should not happen but fall through anyway */
				case TABLE:
					cell.bindColor(TextColorSource.COLUMN, column.textColorProperty());
					break;
				}
			}
			column.textColorProperty().set(newColor);
		}
		else if (!column.hasColor() && newColor == null) {
			/* This is a do nothing case */
		}
	}

	@SuppressWarnings("unchecked")
	private static void setTextColorRow(Table<RowData> table, int row, Color newColor) {
		RowData rowData = table.getRowData(row);
		if (rowData.hasColor() && newColor != null) {
			rowData.textColorProperty().set(newColor);
		}
		else if (rowData.hasColor() && newColor == null) {
			int numCols = table.getColumns().size();
			for (int colIndex = 0; colIndex< numCols; colIndex++) {
				BaseTableItem cell = table.getCellData(row, colIndex);
				switch (cell.getTextColorSource()) {
				case CELL:
					/* do nothing */
					break;
				case ROW:
					Column<RowData, TextTableItem> column;
					column = (Column<RowData, TextTableItem>) table.getColumns().get(colIndex);
					if (column.hasColor()) {
						cell.bindColor(TextColorSource.COLUMN, column.textColorProperty());
					}
					else {
						cell.bindColor(TextColorSource.TABLE, table.textColorProperty());
					}
					break;
				case COLUMN:
				case TABLE:
					/* neither of these should happen but ignore, do nothing */
					break;
				}
			}
			rowData.textColorProperty().set(null);
		}
		else if (!rowData.hasColor() && newColor != null) {
			int numCols = table.getColumns().size();
			for (int colIndex = 0; colIndex< numCols; colIndex++) {
				BaseTableItem cell = table.getCellData(row, colIndex);
				switch (cell.getTextColorSource()) {
				case CELL:
					/* do nothing */
					break;
				case ROW:
					/* Should not happen but fall through anyway */
				case COLUMN:
				case TABLE:
					cell.bindColor(TextColorSource.ROW, rowData.textColorProperty());
					break;
				}
			}
			rowData.textColorProperty().set(newColor);
		}
		else if (!rowData.hasColor() && newColor == null) {
			/* This is a do nothing case */
		}
	}
	
	@SuppressWarnings("incomplete-switch")
	public static void setXPosition(Label label, TextAlignment textAlignment, FontMetrics fontMetrics,
			double colWidth)
	{
		String text = label.getText();
		if (textAlignment == TextAlignment.LEFT || text == null || text.length() < 1) return;
		float stringWidth = fontMetrics.computeStringWidth(text);
		switch (textAlignment) {
		case RIGHT:
			double d1 = colWidth - stringWidth - 4;
			if (d1 < 2f) d1 = 2f;
			label.setLayoutX(d1);
			break;
		case CENTER:
			d1 = (colWidth - stringWidth) / 2f;
			label.setLayoutX(d1);
			break;
		}
	}

}
