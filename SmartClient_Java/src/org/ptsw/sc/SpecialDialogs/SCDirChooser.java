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

import org.ptsw.sc.Client;
import org.ptsw.sc.Resource;
import org.ptsw.sc.SCWindow;
import org.ptsw.sc.SmartClientApplication;
import org.ptsw.sc.xml.Attribute;
import org.ptsw.sc.xml.Element;

import javafx.stage.DirectoryChooser;

public class SCDirChooser extends Resource {

	private DirectoryChooser dirChooser = new DirectoryChooser();
	private File defaultDir;
	private String title;

	public SCDirChooser(Element element) throws Exception {
		super(element);
		for (Attribute a1 : element.getAttributes()) {
			switch (a1.name) {
			case "default":
				defaultDir = new File(a1.value);
				break;
			case "title":
				title = a1.value;
				break;
			case "devicefilter":
				if (!SmartClientApplication.ignoreUnsupportedChangeCommands)
					throw new UnsupportedOperationException("The JSC does not support 'devicefilter'");
			}
		}
	}

	@Override
	public void Close() {
		dirChooser = null;
	}

	@Override
	public void Hide() {
		/* Do nothing, should never be called */
	}

	/**
	 * @param win1 The DB/C window device to own the dialog
	 * @throws IOException
	 */
	public void showOn(final SCWindow win1) throws IOException {
		SmartClientApplication.Invoke(() -> {
			if (defaultDir != null) dirChooser.setInitialDirectory(defaultDir);
			if (title != null) dirChooser.setTitle(title);
			SCDirChooser.this.specialDialogReturned(
				dirChooser.showDialog(win1.getStage())
			);
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

	/**
	 * Change/dirname method
	 * @param data the directory to start in
	 */
	public void dirname(String data) {
		if (Client.isDebug()) {
			System.out.printf("SCDirChooser.dirname = %s\n", data);
		}
		defaultDir = new File(data);
		if (Client.isDebug()) {
			System.out.printf("SCDirChooser.dirname = %s\n", defaultDir.toString());
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
