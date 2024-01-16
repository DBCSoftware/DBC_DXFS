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

import javafx.scene.control.Label;
import javafx.scene.layout.Pane;
import javafx.scene.paint.Color;
import javafx.scene.shape.Polyline;
import javafx.scene.text.Text;

class SCBoxTitle extends Pane {

	private static final double LABELXPOS = 9.6;
	private static final double LABELHINSET = 2.6;
	
	SCBoxTitle(int hsize, int vsize, String title,
			SCFontAttributes fontAttributes, Color color) {
		Label lbl1 = new Label(title);
		lbl1.setFont(fontAttributes.getFont());
		lbl1.setTextFill(color);
		lbl1.autosize();
		
		// Super Kludge waiting for better API in FX 3
		Text t1 = new Text(title);
		t1.setFont(lbl1.getFont());
		t1.snapshot(null, null);
		double textWidth = t1.getLayoutBounds().getWidth();
		double th2 = t1.getLayoutBounds().getHeight() / 2.0;

		lbl1.relocate(LABELXPOS, 0.0);
		getChildren().add(lbl1);
		
		Polyline pl1 = new Polyline();
		pl1.getPoints().addAll(new Double[]{
				LABELXPOS - LABELHINSET, th2,
				0.0, th2,
				0.0, (double) vsize,
				(double) hsize, (double) vsize,
				(double) hsize, th2,
				LABELXPOS + textWidth + LABELHINSET, th2});

		pl1.setStrokeWidth(0.6);
		getChildren().add(pl1);
	}

}
