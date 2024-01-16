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

import javafx.geometry.Bounds;
import javafx.scene.text.Font;
import javafx.scene.text.Text;

public class FontMetrics {
	final private Text internal;
	public float ascent, descent, lineHeight;

	public float getLineHeight() {
		return lineHeight;
	}

	public FontMetrics(Font fnt) {
		internal = new Text();
		internal.setFont(fnt);
		Bounds b = internal.getLayoutBounds();
		lineHeight = (float) b.getHeight();
		ascent = (float) -b.getMinY();
		descent = (float) b.getMaxY();
	}

	public float computeStringWidth(String txt) {
		internal.setText(txt);
		return (float) internal.getLayoutBounds().getWidth();
	}
}
