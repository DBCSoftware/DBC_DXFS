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

import javafx.event.EventHandler;
import javafx.scene.control.ProgressBar;
import javafx.scene.input.MouseEvent;

class SCProgressbar extends ProgressBar implements RightClickable {

	private int low = 1;
	private int high = 100;
	private int value = 1;
	private boolean reportRightClicks;
	
	public SCProgressbar(final Resource res, final String item, int hsize, int vsize) {
		super();
		setId(item);
		setPrefSize(hsize, vsize);
		setMinSize(USE_PREF_SIZE, USE_PREF_SIZE);
		setProgress(0.0f);
		setFocusTraversable(true);
		addEventFilter(MouseEvent.ANY, new EventHandler<MouseEvent>() {
			@Override
			public void handle(MouseEvent event) {
				if (reportRightClicks && Resource.isRightMouseEventReportable(event)) {
					Client.ControlButtonEvent(res.getId(), SCProgressbar.this, event);
				}
			}
		});
	}

	void setRange(int low, int high) {
		this.low = low;
		this.high = high;
		if (high < low) setProgress(INDETERMINATE_PROGRESS);
		else if (low <= value && value <= high) setValue(value);
		else setValue(low);
	}

	void setValue(int value) {
		if (high >= low && (low <= value && value <= high)) {
			double scaledValue = (double)(value - low) / (double)(high - low + 1);
			setProgress(scaledValue);
			this.value = value;
		}
		else setProgress(INDETERMINATE_PROGRESS);
	}

	@Override
	public void setRightClick(boolean state) {
		reportRightClicks = state;
	}

	int getIntValue() {
		double prog = getProgress();
		if (prog == INDETERMINATE_PROGRESS) return 0;
		double scaledValue = prog * (high - low + 1);
		return (int)(scaledValue + low);
	}
}
