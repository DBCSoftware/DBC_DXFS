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

package org.ptsw.sc.ListDrop;

import java.util.ArrayList;

import org.ptsw.sc.Client;
import org.ptsw.sc.ClientUtils;
import org.ptsw.sc.ControlWrapper;
import org.ptsw.sc.Resource;
import org.ptsw.sc.RightClickable;
import org.ptsw.sc.ShowOnlyable;
import org.ptsw.sc.Client.ControlType;

import javafx.beans.property.ObjectProperty;
import javafx.beans.property.ReadOnlyStringProperty;
import javafx.beans.property.SimpleObjectProperty;
import javafx.beans.property.SimpleStringProperty;
import javafx.beans.value.ChangeListener;
import javafx.beans.value.ObservableValue;
import javafx.collections.ObservableList;
import javafx.event.EventHandler;
import javafx.geometry.Pos;
import javafx.scene.control.Control;
import javafx.scene.control.ListCell;
import javafx.scene.control.ListView;
import javafx.scene.control.SelectionModel;
import javafx.scene.input.MouseEvent;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;
import javafx.util.Callback;

/**
 * Base class for DB/C Dropbox, Listbox, MListbox
 */
abstract class SCListDropBase implements ShowOnlyable, RightClickable, SingleSelectionBox, ControlWrapper,
	InsertOrderable, Boxtabable
{

	protected final Callback<ListView<ListDropItem>, ListCell<ListDropItem>> cellFactory =
			new Callback<>()
	{
		@Override
		public ListCell<ListDropItem> call(ListView<ListDropItem> list) {
			return new SCListCell(SCListDropBase.this);
		}
	};

	protected final EventHandler<MouseEvent> mouseListener = new EventHandler<>() {
		@Override
		public void handle(MouseEvent event) {
			if (controlType == ControlType.SHOWONLY) {
				event.consume();
				control.getScene().getWindow().fireEvent(event);
			}
			else if (reportRightClicks && Resource.isRightMouseEventReportable(event)) {
				Client.ControlButtonEvent(parentResource.getId(), control, event);
			}
		}
	};
	
	protected final ChangeListener<Boolean> disabledListener = new ChangeListener<>() {
		@Override
		public void changed(ObservableValue<? extends Boolean> observable,
				Boolean oldValue, Boolean newValue) {
			if (!newValue) controlType = ControlType.NORMAL;
		}
	};
	
	protected static final String BACKGROUNDTRANSPARENT = "-fx-background-color: transparent;";
	protected static final ReadOnlyStringProperty transparentBackground =
			new SimpleStringProperty(BACKGROUNDTRANSPARENT);
	protected Client.ControlType controlType = ControlType.NORMAL;
	protected ObjectProperty<Color> currentColor;
	protected final Font initialFont;
	protected final Resource parentResource;
	protected boolean reportRightClicks;
	protected final Control control;
	protected int[] Boxtabs;
	protected Pos[] BoxtabsTextAlignment;
	protected ItemListManager droplistManager;
	
	protected final Callback<String, ListDropItem> newCell = new Callback<>() {

		@Override
		public ListDropItem call(String text) {
			if (SCListDropBase.this instanceof SCDropbox)
				return new DropItem(currentColor, initialFont, text, SCListDropBase.this);
			else if (SCListDropBase.this instanceof SCListbox)
				return new ListItem(currentColor, initialFont, text, SCListDropBase.this);
			return null;
		}
		
	};
	
	SCListDropBase(Control control, Resource parent, Color textColor, Font font) throws Exception {
		this.control = control;
		this.parentResource = parent;
		currentColor = new SimpleObjectProperty<>(textColor);
		if (font != null) initialFont = font;
		else initialFont = Font.getDefault();
		this.control.addEventFilter(MouseEvent.ANY, mouseListener);
		this.control.disabledProperty().addListener(disabledListener);
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.SingleSelectionBox#getParentResource()
	 */
	@Override
	public Resource getParentResource() {
		return parentResource;
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.ShowOnlyable#setShowonly()
	 */
	@Override
	public void setShowonly() {
		controlType = ControlType.SHOWONLY;
		control.setFocusTraversable(false);
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.ShowOnlyable#isShowonly()
	 */
	@Override
	public boolean isShowonly() {
		return (controlType == ControlType.SHOWONLY);
	}

	/* (non-Javadoc)
	 *  @see com.dbcswc.sc.InsertOrderable#setInsertOrder()
	 */
	@Override
	public void setInsertOrder() {
		droplistManager.setInsertOrder();
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.SingleSelectionBox#getId()
	 */
	@Override
	public String getId() {
		return control.getId();
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.NodeWrapper#getNode()
	 */
	@Override
	public Control getControl() {
		return control;
	}
	
	abstract SelectionModel<ListDropItem> getSelectionModel();
	
	abstract ObservableList<ListDropItem> getItems();

	protected boolean isMultipleSelect() {
		return (this instanceof MultipleSelectionBox);
	}
	
	@Override
	public void delete(String text) {
		int i1 = ClientUtils.findMatchingText(getItems(), text);
		if (i1 >= 0) deleteline(i1);
	}
	
	/**
	 * Delete a line by its index
	 * DB/C rule is that if the removed item was the selected item, then
	 * the selection is cleared.
	 * @param i1 zero-based index
	 */
	@Override
	public void deleteline(int i1) {
		int selectedindex = getSelectionModel().getSelectedIndex();
		if (0 <= i1 && i1 < getItems().size()) {
			getItems().remove(i1);
			if (i1 == selectedindex) getSelectionModel().clearSelection();
		}
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.ListDrop.SingleSelectionBox#erase()
	 */
	@Override
	public void erase() {
		getSelectionModel().clearSelection();
		droplistManager.erase();
	}
	
	@Override
	public void locate(String text) {
		int i1 = ClientUtils.findMatchingText(getItems(), text);
		if (i1 >= 0) getSelectionModel().select(i1);
	}
	
	@Override
	public void locateLine(int i1) {
		if (Client.isDebug()) System.out.printf("SCListDropBase.locateLine %d\n", i1);
		if (0 <= i1 && i1 < getItems().size()) getSelectionModel().select(i1);
		else if (i1 == -1) {
			getSelectionModel().clearSelection();
		}
	}

	@Override
	public void textbgcolorline(Color newColor, int index) {
		getItems().get(index).setBackgroundColor(newColor);
	}

	@Override
	public void textbgcolordata(Color newColor, String text) {
		int index = ClientUtils.findMatchingText(getItems(), text);
		if (index >= 0) textbgcolorline(newColor, index);
	}

	@Override
	public void textcolordata(Color newColor, String text) {
		int index = ClientUtils.findMatchingText(getItems(), text);
		if (index >= 0) textcolorline(newColor, index);
	}
	
	@Override
	public void textcolor(Color newColor) {
		currentColor.setValue(newColor);
	}

	@Override
	public void textcolorline(Color newColor, int index) {
		ListDropItem item = getItems().get(index);
		item.setSpecialFGColor(newColor);
	}

	@Override
	public void textstyleline(String newStyle, int index) throws Exception {
		ListDropItem cell = getItems().get(index);
		Font newFont = ClientUtils.applyDBCStyle(cell.font.get(), newStyle, getId());
		cell.font.setValue(newFont);
	}

	@Override
	public void textstyledata(String newStyle, String text) throws Exception {
		int index = ClientUtils.findMatchingText(getItems(), text);
		if (index >= 0) textstyleline(newStyle, index);
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.RightClickable#setRightClick(boolean)
	 */
	@Override
	public void setRightClick(boolean state) {
		reportRightClicks = state;
	}

	@Override
	public Object getStatus() {
		if (parentResource.isLineOn()) {
			int index = getSelectionModel().getSelectedIndex() + 1;
			if (index > 0) return index;
			return "";
		}
		else {
			ListDropItem cell = getSelectionModel().getSelectedItem();
			return (cell != null) ? cell.getText() : "";
		}
	}


	/* (non-Javadoc)
	 * @see com.dbcswc.sc.Boxtabable#setBoxtabs(int[], javafx.scene.text.TextAlignment[])
	 */
	@Override
	public void setBoxtabs(int[] tabsList, Pos[] taList) {
		Boxtabs = tabsList;
		BoxtabsTextAlignment = taList;
	}

	@Override
	public void insert(String data) throws Exception {
		droplistManager.insert(data);
	}
	
	@Override
	public void minsert(ArrayList<String> datums) throws Exception {
		droplistManager.minsert(datums);
	}

	@Override
	public void insertafter(String data) throws Exception {
		droplistManager.insertafter(data);
	}
	
	/**
	 * @param data Insert before matching text, or if zero-length, insert at beginning
	 * @throws Exception
	 */
	@Override
	public void insertbefore(String data) throws Exception {
		droplistManager.insertbefore(data);
	}

	@Override
	public void insertlineafter(String data) throws Exception {
		droplistManager.insertlineafter(data);
	}

	@Override
	public void insertlinebefore(String data) throws Exception {
		droplistManager.insertlinebefore(data);
	}

}
