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
import java.util.Collection;
import java.util.LinkedList;

import org.ptsw.sc.ClientUtils;
import org.ptsw.sc.ListDrop.InsertOrderable.Mode;

import javafx.collections.ObservableList;
import javafx.util.Callback;

/**
 * Objects that need to manage an ObservableList used by a ComboBox or ListView
 * create one of these to handle all insert operations
 */
public class ItemListManager {

	private final ObservableList<ListDropItem> theList;
	private final Callback<String, ListDropItem> getNewCell;
	
	/*
	 * The default insert mode is Alpha, unless an <INSERTMODE/> element
	 * is seen in the prep string right after the <dropbox .../> element
	 */
	private InsertOrderable.Mode insertMode = Mode.ALPHA;
	private String insertBeforeAfterText;
	private int insertBeforeAfterZeroBasedIndex;

	public ItemListManager(ObservableList<ListDropItem> list, Callback<String, ListDropItem> getCell) {
		theList = list;
		this.getNewCell = getCell;
	}

	public void setInsertOrder() {
		insertMode = Mode.ATEND;
	}
	
	public void erase() {
		theList.clear();
		if (insertMode != InsertOrderable.Mode.ALPHA) insertMode = InsertOrderable.Mode.ATEND;
	}
	
	public void insert(String data) throws Exception {
		int i1;
		ListDropItem newCell = getNewCell.call(data);
		//if (Client.IsDebug()) System.out.printf("In ItemListManager.insert %s\n", insertMode.toString());
		switch (insertMode) {
		case ATEND:
			try {
				theList.add(newCell);
			}
			catch (Throwable t) {
				t.printStackTrace();
			}
			break;
		case AFTERLINE:
			theList.add(insertBeforeAfterZeroBasedIndex + 1, newCell);
			break;
		case AFTERTEXT:
			i1 = ClientUtils.findMatchingText(theList, insertBeforeAfterText);
			if (i1 >= 0) theList.add(i1 + 1, newCell);
			break;
		case ALPHA:
			for (i1 = 0; i1 < theList.size(); i1++) {
				if (data.compareTo(theList.get(i1).getText()) < 0) {
					theList.add(i1, newCell);
					newCell = null;
					break;
				}
			}
			if (newCell != null) theList.add(newCell);
			break;
		case ATSTART:
			theList.add(0, newCell);
			break;
		case BEFORELINE:
			theList.add(insertBeforeAfterZeroBasedIndex, newCell);
			break;
		case BEFORETEXT:
			i1 = ClientUtils.findMatchingText(theList, insertBeforeAfterText);
			if (i1 >= 0) {
				theList.add(i1, newCell);
				/*
				 * Weird and crappy special case for this one thing to be consistent with the C code
				 * We need to change the 'beforetext' to what we just inserted.
				 */
				insertBeforeAfterText = data;
			}
			break;
		default:
			break;
		}
	}
	
	public void minsert(ArrayList<String> datums) throws Exception {
		if (insertMode == Mode.ALPHA) {
			for (String s1 : datums) {
				insert(s1);
			}
		}
		else {
			Collection<ListDropItem> cellSet = new LinkedList<>();
			int i1 = 0;
			for (String s1 : datums) {
				cellSet.add(getNewCell.call(s1));
			}
			switch (insertMode) {
			case ATEND:
				theList.addAll(cellSet);
				break;
			case AFTERLINE:
				theList.addAll(insertBeforeAfterZeroBasedIndex + 1, cellSet);
				break;
			case AFTERTEXT:
				i1 = ClientUtils.findMatchingText(theList, insertBeforeAfterText);
				if (i1 >= 0) theList.addAll(i1 + 1, cellSet);
				break;
			case ALPHA:
				break;
			case ATSTART:
				theList.addAll(0, cellSet);
				break;
			case BEFORELINE:
				theList.addAll(insertBeforeAfterZeroBasedIndex, cellSet);
				break;
			case BEFORETEXT:
				i1 = ClientUtils.findMatchingText(theList, insertBeforeAfterText);
				if (i1 >= 0) theList.addAll(i1, cellSet);
				break;
			default:
				break;
			}
		}
	}
	
	public void insertafter(String data) throws Exception {
		if (insertMode == Mode.ALPHA) {
			String s1 = "Cannot change insert mode of Alphabetical dropbox";
			throw new Exception(s1);
		}
		if (data == null || data.length() < 1) insertMode = Mode.ATEND;
		else {
			insertBeforeAfterText = data;
			insertMode = Mode.AFTERTEXT;
		}
	}
	
	/**
	 * @param data Insert before matching text, or if zero-length, insert at beginning
	 * @throws Exception
	 */
	public void insertbefore(String data) throws Exception {
		if (insertMode == Mode.ALPHA) {
			String s1 = "Cannot change insert mode of Alphabetical dropbox";
			throw new Exception(s1);
		}
		if (data == null || data.length() < 1) insertMode = Mode.ATSTART;
		else {
			insertBeforeAfterText = data;
			insertMode = Mode.BEFORETEXT;
		}
	}

	public void insertlineafter(String data) throws Exception {
		setIL(data, Mode.AFTERLINE);
	}

	public void insertlinebefore(String data) throws Exception {
		setIL(data, Mode.BEFORELINE);
	}

	private void setIL(String data, InsertOrderable.Mode mode) throws Exception
	{
		if (insertMode == Mode.ALPHA) {
			String s1 = "Cannot change insert mode of Alphabetical dropbox";
			throw new Exception(s1);
		}
		int index;
		if (data != null && data.length() > 0) {
			try {
				index = Integer.parseInt(data);
				insertBeforeAfterZeroBasedIndex = index - 1;
				insertMode = mode;
			}
			catch (NumberFormatException ex) {
				/* Do Nothing */
			}
		}
	}
}
