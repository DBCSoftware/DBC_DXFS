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

import javafx.beans.property.ReadOnlyObjectWrapper;
import javafx.beans.value.ObservableValue;
import javafx.scene.control.TableCell;
import javafx.scene.control.TableColumn;
import javafx.scene.control.TableColumn.CellDataFeatures;
import javafx.scene.text.Font;
import javafx.util.Callback;

import org.ptsw.sc.Client;
import org.ptsw.sc.FontMetrics;
import org.ptsw.sc.Table.Cell.CDropboxCell;
import org.ptsw.sc.Table.Cell.CheckboxCell;
import org.ptsw.sc.Table.Cell.DropboxCell;
import org.ptsw.sc.Table.Cell.EditCell;
import org.ptsw.sc.Table.Cell.PopboxCell;
import org.ptsw.sc.Table.Cell.TextCell;
import org.ptsw.sc.Table.Item.CDropBoxTableItem;
import org.ptsw.sc.Table.Item.CheckboxTableItem;
import org.ptsw.sc.Table.Item.DropBoxTableItem;
import org.ptsw.sc.Table.Item.EditTableItem;
import org.ptsw.sc.Table.Item.TextTableItem;
import org.ptsw.sc.Table.Table.ColumnType;
import org.ptsw.sc.xml.Element;

final class ColumnFactory {

	static private Exception creatColException;

	static void createColumns(final Table<?> table, Element e1) throws Exception {
		final Font font = table.getFont();
		final FontMetrics fontMetrics = table.getFontMetrics();
		int colIndex = 0;
		creatColException = null;
		for (Object o1 : e1.getChildren()) {
			if (!(o1 instanceof Element)) continue;
			Element e2 = (Element) o1;
			String ename = e2.name.intern();
			if (ename == "c") {
				ColumnType type =  ColumnType.fromInteger(Integer.parseInt(e2.getAttributeValue("c")));
				switch (type) {
				case VTEXT:
				case CVTEXT:
				case RVTEXT:
				case POPBOX:
					Column<RowData, TextTableItem> cText = new Column<>(e2, colIndex++);
					cText.setEditable(false);
					cText.setCellValueFactory(
						new Callback<TableColumn.CellDataFeatures<RowData, TextTableItem>, ObservableValue<TextTableItem>>()
						{
							@Override
							public ObservableValue<TextTableItem> call(CellDataFeatures<RowData, TextTableItem> param)
							{
								Column<RowData, TextTableItem> col = (Column<RowData, TextTableItem>) param.getTableColumn();
								int colIndex = col.getColumnIndex();
								RowData rowData = param.getValue();
								TextTableItem cell = (TextTableItem) rowData.getCell(colIndex);
								return new ReadOnlyObjectWrapper<>(cell);
							}
						}
					);
					cText.setCellFactory(
						new Callback<TableColumn<RowData, TextTableItem>, TableCell<RowData, TextTableItem>>() 
						{
							@SuppressWarnings({ "rawtypes", "unchecked" })
							@Override
							public TableCell<RowData, TextTableItem> call(TableColumn<RowData, TextTableItem> column)
							{
								if (((Column)column).getColumnType() == ColumnType.POPBOX)
									return new PopboxCell((Column)column, font, ((Column)column).getPopboxActionHandler());
					            return new TextCell(((Column)column), font, fontMetrics);
							}
						}
					);
					table.getColumns().add(cText);
					break;
					
				case CHECKBOX:
					Column<RowData, CheckboxTableItem> cCheck = new Column<>(e2, colIndex++);
					cCheck.setEditable(true);
					cCheck.setCellFactory(
						new Callback<TableColumn<RowData, CheckboxTableItem>, TableCell<RowData, CheckboxTableItem>>() 
						{
							@SuppressWarnings({ "rawtypes" })
							@Override
							public TableCell<RowData, CheckboxTableItem> call(
									final TableColumn<RowData, CheckboxTableItem> column)
							{
								final Callback<Integer, ObservableValue<Boolean>> callback =
										new Callback<>()
								{
									@Override
									public ObservableValue<Boolean> call(Integer rowIndex) {
										int col = ((Column)column).getColumnIndex();
										CheckboxTableItem cell = (CheckboxTableItem) ((Table)table).getCellData(rowIndex, col);
										return cell.checkedProperty();
									}
								};
								return new CheckboxCell(callback);
							}
						}
					);
					table.getColumns().add(cCheck);
					break;
					
				case EDIT:
				case FEDIT:
				case LEDIT:
					Column<RowData, EditTableItem> cEdit = new Column<>(e2, colIndex++);
					cEdit.setEditable(true);
					cEdit.setCellValueFactory(new Callback<CellDataFeatures<RowData, EditTableItem>, ObservableValue<EditTableItem>>() {
						@Override public ObservableValue<EditTableItem> call(
								CellDataFeatures<RowData, EditTableItem> param) {
							Column<RowData, EditTableItem> col = (Column<RowData, EditTableItem>) param.getTableColumn();
							int colIndex = col.getColumnIndex();
							RowData rowData = param.getValue();
							EditTableItem cell = (EditTableItem) rowData.getCell(colIndex);
							return new ReadOnlyObjectWrapper<>(cell);
						}
					});
					cEdit.setCellFactory(
						new Callback<TableColumn<RowData, EditTableItem>, TableCell<RowData, EditTableItem>>() 
						{
							@SuppressWarnings("rawtypes")
							@Override
							public TableCell<RowData, EditTableItem> call(TableColumn<RowData, EditTableItem> column)
							{
								return new EditCell((Column)column, font, fontMetrics);
							}
						}
					);
					table.getColumns().add(cEdit);
					break;
				case CDROPBOX:
					Column<RowData, CDropBoxTableItem> cCdbox
							= new ColumnCDropBox<RowData, CDropBoxTableItem>(e2, colIndex++);
					cCdbox.setEditable(false);
					cCdbox.setCellValueFactory(new Callback<CellDataFeatures<RowData, CDropBoxTableItem>, ObservableValue<CDropBoxTableItem>>()
					{
						@Override public ObservableValue<CDropBoxTableItem> call(
								CellDataFeatures<RowData, CDropBoxTableItem> param)
						{
							Column<RowData, CDropBoxTableItem> col = (Column<RowData, CDropBoxTableItem>) param.getTableColumn();
							int colIndex = col.getColumnIndex();
							RowData rowData = param.getValue();
							CDropBoxTableItem cell = (CDropBoxTableItem) rowData.getCell(colIndex);
							return new ReadOnlyObjectWrapper<>(cell);
						}
					});
					cCdbox.setCellFactory(
						new Callback<TableColumn<RowData, CDropBoxTableItem>, TableCell<RowData, CDropBoxTableItem>>() 
						{
							@SuppressWarnings({ "rawtypes" })
							@Override
							public TableCell<RowData, CDropBoxTableItem> call(TableColumn<RowData, CDropBoxTableItem> column)
							{
								try {
									return new CDropboxCell((Column)column);
								} catch (Exception e) {
									//TODO Does not work. Enter a message into the ToDo queue for sending
									creatColException = e;
									System.err.printf("\nException! %s\n",e);
								}
								return null;
							}
						}
					);
					table.getColumns().add(cCdbox);
					if (Client.isDebug()) System.out.printf("CDROPBOX(%d) column created\n",
							cCdbox.getColumnIndex());
					break;
				case DROPBOX:
					Column<RowData, DropBoxTableItem> cdbox = new ColumnDropBox<RowData, DropBoxTableItem>(e2, colIndex++);
					cdbox.setEditable(false);
					cdbox.setCellValueFactory(new Callback<CellDataFeatures<RowData, DropBoxTableItem>, ObservableValue<DropBoxTableItem>>()
					{
						@SuppressWarnings("rawtypes")
						@Override public ObservableValue<DropBoxTableItem> call(
								CellDataFeatures<RowData, DropBoxTableItem> param)
						{
							Column col = (Column) param.getTableColumn();
							int colIndex = col.getColumnIndex();
							RowData rowData = param.getValue();
							DropBoxTableItem cell = (DropBoxTableItem) rowData.getCell(colIndex);
							return new ReadOnlyObjectWrapper<>(cell);
						}
					});
					cdbox.setCellFactory(
						new Callback<TableColumn<RowData, DropBoxTableItem>, TableCell<RowData, DropBoxTableItem>>() 
						{
							@SuppressWarnings({ "rawtypes" })
							@Override
							public TableCell<RowData, DropBoxTableItem> call(TableColumn<RowData, DropBoxTableItem> column)
							{
								try {
									return new DropboxCell((Column)column);
								} catch (Exception e) {
									//TODO Does not work. Enter a message into the ToDo queue for sending
									creatColException = e;
									System.err.printf("\nException! %s\n",e);
								}
								return null;
							}
						}
					);
					table.getColumns().add(cdbox);
					break;
				}
			}
		}
		if (creatColException != null) throw creatColException;
	}
}
