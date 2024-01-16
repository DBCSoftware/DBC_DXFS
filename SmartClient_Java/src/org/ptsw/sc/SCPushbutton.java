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

package org.ptsw.sc;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javafx.event.EventHandler;
import javafx.scene.control.Button;
import javafx.scene.control.ContentDisplay;
import javafx.scene.image.Image;
import javafx.scene.image.ImageView;
import javafx.scene.input.MouseEvent;

class SCPushbutton extends Button implements RightClickable {

	private static final String pat = "[^&]&[a-zA-Z0-9]";
	private static final Pattern amperAsMnemonicPat = Pattern.compile(pat);
	private boolean reportRightClicks;

	public SCPushbutton(final Resource res, Object data, String item) {
		super();
		if (data instanceof String) {
			int i1;
			String sdata = checkForMnemonic((String)data);
			i1 = sdata.indexOf("&&");
			if (i1 != -1) {
				sdata = sdata.substring(0, i1 + 1) + sdata.substring(i1 + 2);
			}
			i1 = sdata.indexOf("\\&");
			if (i1 != -1) {
				sdata = sdata.substring(0, i1) + sdata.substring(i1 + 1);
			}
			setText(sdata);
			setContentDisplay(ContentDisplay.TEXT_ONLY);
		}
		else if (data instanceof Image) {
			setGraphic(new ImageView((Image)data));
			setContentDisplay(ContentDisplay.GRAPHIC_ONLY);
		}
		setId(item);
		addEventFilter(MouseEvent.ANY, new EventHandler<MouseEvent>() {
			@Override
			public void handle(MouseEvent event) {
				if (reportRightClicks && Resource.isRightMouseEventReportable(event)) {
					Client.ControlButtonEvent(res.getId(), SCPushbutton.this, event);
				}
			}
		});
	}

	@Override
	public void setRightClick(boolean state) {
		reportRightClicks = state;
	}

	void setImage(Image image) {
		setGraphic(new ImageView(image));
		setContentDisplay(ContentDisplay.GRAPHIC_ONLY);
	}
	
	static String checkForMnemonic(String data) {
		Matcher amperAsMnemonicMatcher = amperAsMnemonicPat.matcher(data);
        if (!amperAsMnemonicMatcher.find()) return data;
        int i1 = amperAsMnemonicMatcher.start();
        return data.substring(0, i1 + 1) + "_" + data.substring(i1 + 2);
	}
}
