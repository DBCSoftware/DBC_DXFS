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

import org.ptsw.sc.Table.Column;
import org.ptsw.sc.Table.RowData;
import org.ptsw.sc.Table.Item.DropBoxTableItem;

import javafx.scene.control.ContentDisplay;
import javafx.scene.control.TableCell;

public class DropboxCell extends TableCell<RowData, DropBoxTableItem> {

	@SuppressWarnings("unused")
	private final double colWidth;
	
	@SuppressWarnings({ })
	public DropboxCell (Column<?,?> column) throws Exception {
		colWidth = column.getPrefWidth();
		setText(null);
		setContentDisplay(ContentDisplay.GRAPHIC_ONLY);
	}
	
	@Override
	protected void updateItem(DropBoxTableItem item, boolean empty) {
		super.updateItem(item, empty);
        if (item == null || empty) {
            setGraphic(null);
        }
        else {
    		setGraphic(item.getControl());
        }
	}
}


