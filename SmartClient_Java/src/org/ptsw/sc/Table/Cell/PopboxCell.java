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

import org.ptsw.sc.Popbox.Popbox;
import org.ptsw.sc.Table.Column;
import org.ptsw.sc.Table.RowData;
import org.ptsw.sc.Table.Item.TextTableItem;

import javafx.event.ActionEvent;
import javafx.event.EventHandler;
import javafx.scene.Node;
import javafx.scene.control.ContentDisplay;
import javafx.scene.control.TableCell;
import javafx.scene.text.Font;

public class PopboxCell extends TableCell<RowData, TextTableItem> {

	private final Column<?,?> column;
	private Popbox popbox;
	private final Font font;
	private final EventHandler<ActionEvent> actionHandler;

	public PopboxCell(Column<?,?> column, Font font, EventHandler<ActionEvent> actionHandler) {
		this.column = column;
		this.font = font;
		this.actionHandler = actionHandler;
        setText(null);
		setContentDisplay(ContentDisplay.GRAPHIC_ONLY);
	}


	@Override
	protected void updateItem(TextTableItem item,
			boolean empty) {
		super.updateItem(item, empty);
        if (item == null || empty) {
        	Node n1 = getGraphic();
        	if (n1 != null && n1 instanceof Popbox) {
        		((Popbox)n1).textProperty().unbind();
        		//((Popbox)n1).colorProperty().unbind();
        	}
            setGraphic(null);
        }
        else {
        	if (popbox == null) createPopbox();
    		popbox.textProperty().bind(item.textProperty());
    		//popbox.colorProperty().bind(item.textColorProperty());
    		popbox.setColor(item.textColorProperty().getValue());
    		setGraphic(popbox);
        }
	}
	
	private void createPopbox() {
		popbox = new Popbox(font, this.column.getPrefWidth());
		popbox.setText("");
		popbox.setInTable(true);
		popbox.setOnAction(actionHandler);
	}
}
