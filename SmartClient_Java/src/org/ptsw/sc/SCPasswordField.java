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
import javafx.scene.control.IndexRange;
import javafx.scene.control.PasswordField;
import javafx.scene.input.MouseEvent;

public class SCPasswordField extends PasswordField implements ReadOnlyable, ShowOnlyable, RightClickable {

private boolean limited;
private Client.ControlType controlType = ControlType.NORMAL;
private int maxlength = Integer.MAX_VALUE;
private boolean reportRightClicks;
private static final IndexRange nullSelection = new IndexRange(0, 0);
private IndexRange currentSelection = nullSelection;
private int currentCaretPosition = 0;
private boolean programmatic = false;

SCPasswordField(final Resource res, String text, boolean limited, int maxLength) {
	if (text != null && text.length() > 0) setText(text);
	this.limited = limited;
	if (limited) this.maxlength = maxLength;
	disabledProperty().addListener(new ChangeListener<Boolean>() {
		@Override
		public void changed(ObservableValue<? extends Boolean> observable, Boolean oldValue, Boolean newValue) {
			if (!newValue) controlType = ControlType.NORMAL;
		}

	});
	addEventFilter(MouseEvent.ANY, new EventHandler<MouseEvent>() {
		@Override
		public void handle(MouseEvent event) {
			if (controlType == ControlType.SHOWONLY) {
				event.consume();
				getScene().getWindow().fireEvent(event);
			}
			else if (reportRightClicks && Resource.isRightMouseEventReportable(event)) {
				Client.ControlButtonEvent(res.getId(), SCPasswordField.this, event);
				event.consume();
			}
		}
	});
	selectionProperty().addListener(new ChangeListener<IndexRange>() {
		@Override
		public void changed(ObservableValue<? extends IndexRange> observable, IndexRange oldValue, IndexRange newValue) {
			if (isFocused()) currentSelection = newValue;
		}
	});
	focusedProperty().addListener(new ChangeListener<Boolean>() {
		@Override
		public void changed(ObservableValue<? extends Boolean> observable, Boolean oldValue, Boolean newValue) {
			if (!newValue) currentCaretPosition = caretPositionProperty().intValue();
		}
	});
}

public void setMaxlength(int maxlength) {
	if (limited) this.maxlength = maxlength;
}

@Override
public void replaceText(int start, int end, String text) {
	if (!programmatic && (controlType == ControlType.SHOWONLY || controlType == ControlType.READONLY)) return;
	if (!limited) super.replaceText(start, end, text);
	else {
		if (text.equals("")) {
			super.replaceText(start, end, text);
		}
		else if (getText().length() <= maxlength) {
			if (getText().length() + text.length() - (end - start) <= maxlength) super.replaceText(start, end, text);
		}
	}
}

/**
 * Looks like we need this when doing a ctrl-V into the edit box when something
 * is selected. Needs work. Should replace characters up to maxlength
 * 
 * @param text
 */
@Override
public void replaceSelection(String text) {
	if (!programmatic && (controlType == ControlType.SHOWONLY || controlType == ControlType.READONLY)) return;
	if (!limited) super.replaceSelection(text);
	else {
		if (text.equals("")) {
			super.replaceSelection(text);
		}
		else if (getText().length() <= maxlength) {
			// Add characters, but don't exceed maxlength.
			if (text.length() > maxlength - getText().length()) {
				text = text.substring(0, maxlength - getText().length());
			}
			super.replaceSelection(text);
		}
	}
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
public void setReadonly() {
	controlType = ControlType.READONLY;
	setFocusTraversable(true);
}

@Override
public boolean isReadonly() {
	return (controlType == ControlType.READONLY);
}

@Override
public void setRightClick(boolean state) {
	reportRightClicks = state;
}

/**
 * Note: No need to suppress the ITEM message sent from code in
 * PanelDialogControlFactory DX Sends the ITEM message in this case even though
 * it is a programmatic change
 * 
 * @param data
 *            New text
 */
void doPaste(String data) {
	programmatic = true;
	if (currentSelection.getLength() > 0) {
		if (data == null) data = "";
		replaceText(currentSelection, data);
		currentSelection = nullSelection;
	}
	else if (data != null && currentCaretPosition >= 0) {
		insertText(currentCaretPosition, data);
	}
	programmatic = false;
}

}
