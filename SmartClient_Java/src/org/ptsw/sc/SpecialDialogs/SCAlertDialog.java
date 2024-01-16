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

import javafx.application.Platform;
import javafx.stage.StageStyle;
import javafx.stage.WindowEvent;

import org.ptsw.sc.Client;
import org.ptsw.sc.Msgbox;
import org.ptsw.sc.Resource;
import org.ptsw.sc.SCWindow;
import org.ptsw.sc.SmartClientApplication;
import org.ptsw.sc.xml.Attribute;
import org.ptsw.sc.xml.Element;

public class SCAlertDialog extends Resource {

	private Msgbox dialog;
	
	public SCAlertDialog(Element element) throws Exception {
		super(element);
		String text = "";
		for (Attribute a1 : element.getAttributes()) {
			switch (a1.name) {
				case "text":
					text = a1.value;
				break;
			}
		}
		final String t1 = text;
		try {
			SmartClientApplication.runAndWait(() -> {
				dialog = new Msgbox("Alert", t1);
				dialog.initStyle(StageStyle.UTILITY);
				dialog.addEventHandler(WindowEvent.WINDOW_HIDDEN, arg0 -> {
					Client.OkMessage(specialDialogResourceName);
					try {
						Client.sendRMessageNoError();
					}
					catch (Exception e) {
						if (Client.isDebug()) System.err.println(e);
					}
				});
			});
		}
		catch (RuntimeException e) {
			if (Client.isDebug()) System.err.println(e);
		}
	}

	@Override
	public void Close() {
		SmartClientApplication.runAndWait(() -> {
			dialog.close();
		});
		dialog = null;
	}

	@Override
	public void Hide() {
		/* Do nothing, should never be called */
	}

	public void showOn(SCWindow win1) {
		dialog.initOwner(win1.getStage());
		Platform.runLater(() -> {dialog.show();});
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

}
