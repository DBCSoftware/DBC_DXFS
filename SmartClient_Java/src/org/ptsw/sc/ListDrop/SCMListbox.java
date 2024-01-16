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

import org.ptsw.sc.Client;
import org.ptsw.sc.ClientUtils;
import org.ptsw.sc.Resource;

import javafx.collections.ListChangeListener;
import javafx.collections.ObservableList;
import javafx.scene.control.MultipleSelectionModel;
import javafx.scene.control.SelectionMode;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;

/**
 * Represents a DB/C MLISTBOX, multiple select
 */
public class SCMListbox extends SCListbox implements MultipleSelectionBox {

	private boolean ignoreItemOnce;
	
	/**
	 * @param parent
	 * @param item
	 * @param hsize
	 * @param vsize
	 * @param font
	 * @param textColor
	 * @throws Exception
	 */
	public SCMListbox(final Resource parent, final String item, int hsize, int vsize,
			Font font, Color textColor) throws Exception
	{
		super(parent, item, hsize, vsize, font, textColor);
		theBox.getSelectionModel().setSelectionMode(SelectionMode.MULTIPLE);
		//theBox.getSelectionModel().selectedIndexProperty().removeListener(singleSelectionListener);
		theBox.getSelectionModel().getSelectedItems().addListener(new ListChangeListener<ListDropItem>() {

			@Override
			public void onChanged(ListChangeListener.Change<? extends ListDropItem> arg0)
			{
				if (parent.isItemOn()) {
					if (ignoreItemOnce) {
						ignoreItemOnce = false;
						return;
					}
					@SuppressWarnings("unchecked")
					ObservableList<ListDropItem> list = (ObservableList<ListDropItem>) arg0.getList();
					StringBuilder sb1 = new StringBuilder(64);
					if (parent.isLineOn()) {
						for (ListDropItem ld1 : list) {
							// Add 1 to convert to DB/C one-based indicies
							sb1.append(getItems().indexOf(ld1) + 1).append(',');
						}
					}
					else {
						for (ListDropItem ld1 : list) {
							sb1.append(ld1.getText()).append(',');
						}
					}
					if (sb1.length() > 1) sb1.setLength(sb1.length() - 1);
					Client.ItemMessage(parent.getId(), item, sb1.toString());
				}
			}
		});
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.SCListDropBase#getStatus()
	 */
	@Override
	public Object getStatus() {
		MultipleSelectionModel<ListDropItem> msl = (MultipleSelectionModel<ListDropItem>)getSelectionModel();
		if (msl.isEmpty()) return "";
		StringBuilder sb1 = new StringBuilder(64);
		if (parentResource.isLineOn()) {
			ObservableList<Integer> l1 = msl.getSelectedIndices();
			for (Integer ldi : l1) {
				// Add 1 to convert to DB/C one-based indicies
				sb1.append(ldi.intValue() + 1).append(',');
			}
			sb1.setLength(sb1.length() - 1);
		}
		else {
			ObservableList<ListDropItem> l1 = msl.getSelectedItems();
			for (ListDropItem ldi : l1) {
				sb1.append(ldi.getText()).append(',');
			}
			sb1.setLength(sb1.length() - 1);
		}
		return sb1.toString();
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.MultipleSelectionBox#deselect(java.lang.String)
	 * 
	 * If the line cannot be found, or it is not currently selected, ignore this
	 */
	@Override
	public void deselect(String text) {
		int matchingIndex = ClientUtils.findMatchingText(getItems(), text);
		if (matchingIndex < 0) return;
		if (Client.isDebug()) System.out.println("deselect " + text);
		if (parentResource.isItemOn())ignoreItemOnce = true;
		theBox.getSelectionModel().clearSelection(matchingIndex);
	}

	@Override
	public void deselectLine(int index) {
		if (parentResource.isItemOn())ignoreItemOnce = true;
		theBox.getSelectionModel().clearSelection(index);
	}

	@Override
	public void deselectAll() {
		if (parentResource.isItemOn())ignoreItemOnce = true;
		theBox.getSelectionModel().clearSelection();
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.SCListDropBase#locate(java.lang.String)
	 */
	@Override
	public void locate(String text) {
		deselectAll();
		if (parentResource.isItemOn())ignoreItemOnce = true;
		super.locate(text);
	}

	@Override
	public void locateLine(int i1) {
		deselectAll();
		if (parentResource.isItemOn())ignoreItemOnce = true;
		if (0 <= i1 && i1 < getItems().size()) getSelectionModel().select(i1);
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.MultipleSelectionBox#select(java.lang.String)
	 */
	@Override
	public void select(String text) {
		super.locate(text);
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.MultipleSelectionBox#selectLine(int)
	 */
	@Override
	public void selectLine(int index) {
		super.locateLine(index);
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.MultipleSelectionBox#selectAll()
	 */
	@Override
	public void selectAll() {
		if (parentResource.isItemOn())ignoreItemOnce = true;
		((MultipleSelectionModel<ListDropItem>)getSelectionModel()).selectAll();
	}

}
