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
import org.ptsw.sc.Table.SCTextFieldCell;
import org.ptsw.sc.Table.Item.BaseTableItem;
import org.ptsw.sc.Table.Item.EditTableItem;
import org.ptsw.sc.Table.Table.ColumnType;

import javafx.beans.binding.Bindings;
import javafx.beans.value.ChangeListener;
import javafx.event.EventHandler;
import javafx.geometry.Pos;
import javafx.scene.Node;
import javafx.scene.control.ContentDisplay;
import javafx.scene.control.Label;
import javafx.scene.control.TableCell;
import javafx.scene.input.KeyCode;
import javafx.scene.input.KeyEvent;
import javafx.scene.text.Font;
import javafx.scene.text.TextAlignment;

public class EditCell extends TableCell<RowData, EditTableItem> {

    /***************************************************************************
     *                                                                         *
     * Fields                                                                  *
     *                                                                         *
     **************************************************************************/    
	private final TextAlignment textAlignment;
	private final FontMetrics fontMetrics;
	private final Label label;
	private final double colWidth;
	private final Font font;
	private final ColumnType colType;
    private SCTextFieldCell textField;    
    
    private final ChangeListener<? super String> itemMessageListener = (observable, oldValue, newValue) -> {
		EditTableItem etItem = EditCell.this.getItem();
		if (etItem != null) {
			Column<RowData, ? extends BaseTableItem> column = etItem.getColumn();
			RowData rowdata = etItem.getRowData();
			if (!column.ignoreItem) column.editChanged(rowdata, newValue);
			else column.ignoreItem = false;
		}
    };
    
	/**
	 * The maximum number of characters allowed in the field of an LEDIT column
	 */
	private int maxCharacters = Integer.MAX_VALUE;
	
	/**
	 * The MASK to use for an FEDIT column
	 */
	@SuppressWarnings("unused")
	private String fmask;
    
    /**
     * Creates an EditCell
     * @param fontMetrics 
     */
    public EditCell(Column<?,?> column, Font font, FontMetrics fontMetrics) {
		colWidth = column.getPrefWidth();
		label = new Label();
		label.setFont(font);
		label.setMinWidth(USE_PREF_SIZE);
		label.setPrefWidth(colWidth);
        setText(null);
        this.font = font;
		this.fontMetrics = fontMetrics;
		colType = column.getColumnType();
		textAlignment = column.getTextAlignment();
		setContentDisplay(ContentDisplay.GRAPHIC_ONLY);
    	setEditable(true);
		if (column.getColumnType() == ColumnType.LEDIT) {
			maxCharacters = column.getMaxCharacters();
		}
		else if (column.getColumnType() == ColumnType.FEDIT) {
			fmask = column.getFmask();
		}
    }

    @Override
    protected void updateItem(EditTableItem item, boolean empty) {
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
            if (isEditing()) {
                if (textField != null) {
                    textField.setText(getString());
                }
                setGraphic(textField);
            } else {
        		label.textProperty().bind(item.textProperty());
        		label.textFillProperty().bind(item.textColorProperty());
            	setGraphic(label);
            }
        }
    }

    @Override
    public void startEdit() {    	
		super.startEdit();
		if (textField == null) {
			createTextField();
		}
		else textField.setText(getString());
		setGraphic(textField);
		textField.selectAll();
		textField.requestFocus();
		textField.textProperty().addListener(itemMessageListener);
    }
    
    @Override
    public void cancelEdit() {
    	textField.textProperty().removeListener(itemMessageListener);
        super.cancelEdit();
        setGraphic(label);
    }

    @Override
    public void commitEdit(EditTableItem item) {
    	textField.textProperty().removeListener(itemMessageListener);
    	String newText = textField.getText();
    	if (getItem() != null) getItem().setText(newText);
    	super.commitEdit(item);
    }
    
    private void createTextField() {
        textField = new SCTextFieldCell(getString());
        textField.setMinWidth(this.getWidth() - this.getGraphicTextGap()*2);
        textField.setFont(font);
        textField.setOnKeyPressed(new EventHandler<KeyEvent>() {
            @Override public void handle(KeyEvent t) {
                if (t.getCode() == KeyCode.ENTER || t.getCode() == KeyCode.TAB) {
                    commitEdit(getItem());
                } else if (t.getCode() == KeyCode.ESCAPE) {
                    cancelEdit();
                }
            }
        });
        textField.disableProperty().bind(Bindings.not(
                getTableView().editableProperty().and(
                getTableColumn().editableProperty()).and(
                editableProperty())
            ));
		if (colType == ColumnType.LEDIT) textField.setMaxlength(maxCharacters);
    }
   
    private String getString() {
    	String x1 = (getItem() == null) ? "" : getItem().getText();
        return x1;
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
