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
import javafx.event.Event;
import javafx.event.EventHandler;
import javafx.scene.Parent;
import javafx.scene.control.TextArea;
import javafx.scene.input.KeyCode;
import javafx.scene.input.KeyEvent;
import javafx.scene.input.MouseEvent;

/**
 * 
 *  SCTextArea is an implementation of MEDITxx and MLEDITxx
 *
 */
class SCTextArea extends TextArea implements ReadOnlyable, ShowOnlyable, RightClickable {
	
	private Client.ControlType controlType = ControlType.NORMAL;
	private boolean reportRightClicks;
	private int maxlength = Integer.MAX_VALUE;
	private final boolean limited;

	SCTextArea(final Resource res, String text, boolean limited) {
		super(text);
		this.limited = limited;
		disabledProperty().addListener(new ChangeListener<Boolean>() {
			@Override
			public void changed(ObservableValue<? extends Boolean> observable,
					Boolean oldValue, Boolean newValue) {
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
					Client.ControlButtonEvent(res.getId(), SCTextArea.this, event);
					event.consume();
				}
			}
		});
		
		/**
		 * A TextArea likes to use the tab key to insert actual tabs.
		 * The DX standard is not to do that but instead treat it
		 * as a focus mover.
		 */
		addEventFilter(KeyEvent.KEY_PRESSED, new EventHandler<KeyEvent>() {
			@Override
			public void handle(KeyEvent event) {
				Parent parent = SCTextArea.this.getParent();
				if (parent != null && event.getCode() == KeyCode.TAB) {
					Event parentEvent = event.copyFor(parent, parent);
					parent.fireEvent(parentEvent);
			        event.consume();
				}  
			}
		});
	}

    void setMaxlength(int maxlength) {
        if (limited) this.maxlength = maxlength;
    }

    @Override
    public void replaceText(int start, int end, String text) {
    	if (controlType == ControlType.SHOWONLY ||controlType == ControlType.READONLY) return; 
    	if (!limited) super.replaceText(start, end, text);
    	else {
	        if (text.equals("")) {
	            super.replaceText(start, end, text);
	        } else if (getText().length() <= maxlength) {
	        	if (getText().length() + text.length() - (end - start) <= maxlength)
	        		super.replaceText(start, end, text);
	        }
    	}
    }

    @Override
    public void replaceSelection(String text) {
    	if (controlType == ControlType.SHOWONLY ||controlType == ControlType.READONLY) return; 
    	if (!limited) super.replaceSelection(text);
    	else {
	        if (text.equals("")) {
	            super.replaceSelection(text);
	        } else if (getText().length() <= maxlength) {
	            // Add characters, but don't exceed maxlength.
	            if (text.length() > maxlength - getText().length()) {
	                text = text.substring(0, maxlength- getText().length());
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
}
