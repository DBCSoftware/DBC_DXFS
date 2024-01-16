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

import org.ptsw.sc.Client.ControlType;

import javafx.beans.value.ChangeListener;
import javafx.beans.value.ObservableValue;
import javafx.event.EventHandler;
import javafx.scene.control.CheckBox;
import javafx.scene.input.MouseEvent;

/**
 * Handles the DB/C 'showonly' control attribute
 * Note that the 'readonly' attribute is not supported by a DB/C checkbox
 */
public class SCCheckBox extends CheckBox implements ShowOnlyable, RightClickable {

	private Client.ControlType controlType = ControlType.NORMAL;
	private boolean reportRightClicks;

	public SCCheckBox(final Resource res, String text, String item) {
		super(text);
		setId(item);
		addEventFilter(MouseEvent.ANY, new EventHandler<MouseEvent>() {
			@Override
			public void handle(MouseEvent event) {
				if (controlType == ControlType.SHOWONLY) {
					event.consume();
					getScene().getWindow().fireEvent(event);
				}
				else if (reportRightClicks && Resource.isRightMouseEventReportable(event)) {
					Client.ControlButtonEvent(res.getId(), SCCheckBox.this, event);
				}
			}
		});
		disabledProperty().addListener(new ChangeListener<Boolean>() {

			@Override
			public void changed(ObservableValue<? extends Boolean> observable,
					Boolean oldValue, Boolean newValue) {
				if (!newValue) controlType = ControlType.NORMAL;
			}
			
		});
	}

	@Override
	public void setShowonly() {
		controlType = ControlType.SHOWONLY;
		setFocusTraversable(false);
	}

	@Override
	public boolean isShowonly() {
		return (controlType == ControlType.SHOWONLY);
	}

	@Override
	public void setRightClick(boolean state) {
		reportRightClicks = state;
	}

}
