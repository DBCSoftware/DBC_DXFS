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

import java.awt.geom.Point2D;

import javafx.scene.control.Label;
import javafx.scene.paint.Color;
import javafx.scene.text.TextAlignment;

class SCLabel extends Label {

	private final TextAlignment ta;
	private final double width;
	private SCFontAttributes fontAttributes;
	private FontMetrics fMetrics;
	private double x;
	private double y;
	
	SCLabel(String text, TextAlignment ta, double width, SCFontAttributes fontAttributes,
			Point2D.Double position, Color color) {
		super(text);
		this.ta = ta;
		this.x = position.x;
		this.y = position.y;
		this.width = width;
		this.fontAttributes = fontAttributes;
		setFont(fontAttributes.getFont());
		fMetrics = new FontMetrics(getFont());
		setTextFill(color);
		autosize();
		setPosition();
	}

	void changeText(String data) {
		super.setText(data);
		setPosition();
	}

	private void setPosition()
	{
		if (ta == TextAlignment.RIGHT || ta == TextAlignment.CENTER) {
			double lx = x;
			double computedStringWidth = fMetrics.computeStringWidth(getText());
			if (ta == TextAlignment.RIGHT) lx -= computedStringWidth;
			else lx += (width - computedStringWidth) / 2.0;
			relocate(lx, y);
		}
		else relocate(x, y);
	}

	void changeFont(String fontString) {
		fontAttributes = fontAttributes.Merge(fontString);
		setFont(fontAttributes.getFont());
		fMetrics = new FontMetrics(getFont());
		setPosition();
	}
}
