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

package org.ptsw.sc.SpecialDialogs;

import java.io.File;
import java.io.IOException;

import org.ptsw.sc.Resource;
import org.ptsw.sc.SCWindow;
import org.ptsw.sc.SmartClientApplication;
import org.ptsw.sc.xml.Attribute;
import org.ptsw.sc.xml.Element;

import javafx.collections.ObservableList;
import javafx.stage.FileChooser;
import javafx.stage.FileChooser.ExtensionFilter;

public class SCFileChooser extends Resource {

	private String defaultFname;
	private boolean multi;
	public FileChooser fileChooser = new FileChooser();
	
	public SCFileChooser(Element element) throws Exception {
		super(element);
		String userDir = System.getProperty("user.dir");
		fileChooser.setInitialDirectory(new File(userDir));
		for (Attribute a1 : element.getAttributes()) {
			switch (a1.name) {
			case "multiple":
				multi = true;
				break;
			case "default":
				defaultFname = a1.value;
				break;
			}
		}
	}

	@Override public void Close() {
		fileChooser = null;
	}

	@Override public void Hide() {
		/* Do nothing, should never be called */
	}

	/**
	 * @param win1 The DB/C window device to own the dialog
	 * @throws IOException
	 */
	public void showOpenOn(final SCWindow win1) throws IOException {
		SmartClientApplication.Invoke(() -> {
			if (defaultFname != null) fileChooser.setInitialFileName(defaultFname);
			SCFileChooser.this.specialDialogReturned(
				!multi ? fileChooser.showOpenDialog(win1.getStage())
					: fileChooser.showOpenMultipleDialog(win1.getStage())
			);
		}, false);
	}

	/**
	 * @param win1 The DB/C window device to own the dialog
	 * @throws IOException
	 */
	public void showSaveOn(final SCWindow win1) throws IOException {
		SmartClientApplication.Invoke(() -> {
			if (defaultFname != null) fileChooser.setInitialFileName(defaultFname);
			SCFileChooser.this.specialDialogReturned(fileChooser.showSaveDialog(win1.getStage()));
		}, false);
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.Resource#setId(java.lang.String)
	 */
	@Override
	public void setId(String id) {
		specialDialogResourceName = id;
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.Resource#getId()
	 */
	@Override
	public String getId() {
		return specialDialogResourceName;
	}

	public void filename(String data) {
		defaultFname = data;
	}

	public void namefilter(String data) {
		String[] items = data.split(",");
		ObservableList<ExtensionFilter> list = fileChooser.getExtensionFilters();
		for (int i1 = 0; i1 < items.length; i1 += 2) {
			list.add(new ExtensionFilter(items[i1], items[i1 + 1]));
		}
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.Resource#findTarget(org.ptsw.sc.Element)
	 */
	@Override
	protected QueryChangeTarget findTarget(Element e1) throws Exception {
		QueryChangeTarget qct = new QueryChangeTarget();
		qct.target = this;
		return qct;
	}
}
