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
import javafx.geometry.Orientation;
import javafx.scene.control.ScrollBar;
import javafx.scene.input.MouseEvent;

/**
 * Handles the DB/C 'showonly' control attribute
 * Helps keep track of whether a mouse is pressed on this control
 */
class SCScrollBar extends ScrollBar implements ShowOnlyable {

	private boolean tracking;
	private Client.ControlType controlType = ControlType.NORMAL;
	
	/**
	 * @return the tracking
	 */
	boolean isTracking() {
		return tracking;
	}

	/**
	 * @param tracking Set whether or not we are currently tracking mouse dragging of the thumb
	 */
	void setTracking(boolean tracking) {
		this.tracking = tracking;
	}

	public SCScrollBar(String item, Orientation orient, double hsize, double vsize) {
		setId(item);
		setOrientation(orient);
		setPrefWidth(hsize);
		setPrefHeight(vsize);
		setMinWidth(USE_PREF_SIZE);
		setMinHeight(USE_PREF_SIZE);
		setFocusTraversable(true);
		setMax(1.0);
		
		/**
		 * DB/C Scroll Bars do not respond to change/rightclickon
		 * They pass the event to the containing window
		 */
		addEventFilter(MouseEvent.ANY, new EventHandler<MouseEvent>() {
			@Override
			public void handle(MouseEvent event) {
				getScene().getWindow().fireEvent(event);
				if (controlType == ControlType.SHOWONLY) {
					event.consume();
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

	/**
	 * @return the showonly
	 */
	@Override
	public boolean isShowonly() {
		return (controlType == ControlType.SHOWONLY);
	}

}
