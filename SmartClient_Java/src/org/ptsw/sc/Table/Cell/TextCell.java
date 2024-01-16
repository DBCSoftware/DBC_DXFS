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

import org.ptsw.sc.FontMetrics;
import org.ptsw.sc.Table.CellUtilities;
import org.ptsw.sc.Table.Column;
import org.ptsw.sc.Table.RowData;
import org.ptsw.sc.Table.Item.TextTableItem;

import javafx.geometry.Pos;
import javafx.scene.Node;
import javafx.scene.control.ContentDisplay;
import javafx.scene.control.Label;
import javafx.scene.control.TableCell;
import javafx.scene.text.Font;
import javafx.scene.text.TextAlignment;

public class TextCell extends TableCell<RowData, TextTableItem> {

	private TextAlignment textAlignment = TextAlignment.LEFT;
	private final FontMetrics fontMetrics;
	private final Label label;
	private final double colWidth;
	
	public TextCell(Column<?,?> column, Font font, FontMetrics fontMetrics) {
		colWidth = column.getPrefWidth();
		label = new Label();
		label.setFont(font);
        setText(null);
		this.fontMetrics = fontMetrics;
		setContentDisplay(ContentDisplay.GRAPHIC_ONLY);
	}

	@Override
	protected void updateItem(TextTableItem item,
			boolean empty) {
		super.updateItem(item, empty);
        if (item == null || empty) {
        	Node n1 = getGraphic();
        	if (n1 != null && n1 instanceof Label) {
        		((Label)n1).textProperty().unbind();
        		((Label)n1).textFillProperty().unbind();
        	}
            setGraphic(null);
        }
        else {
    		textAlignment = item.getTextAlignment();
    		label.textProperty().bind(item.textProperty());
    		label.textFillProperty().bind(item.textColorProperty());
    		setGraphic(label);
        }
	}
	
	@Override
	protected void layoutChildren() {
		setAlignment(Pos.CENTER_LEFT);
		super.layoutChildren();
		if (textAlignment != TextAlignment.LEFT) {
    		CellUtilities.setXPosition(label, textAlignment, fontMetrics, colWidth);
		}
	}
}
