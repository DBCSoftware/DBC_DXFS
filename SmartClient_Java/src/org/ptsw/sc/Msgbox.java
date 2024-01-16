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

import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.Scene;
import javafx.scene.control.Button;
import javafx.scene.layout.VBox;
import javafx.scene.text.Text;
import javafx.stage.Modality;
import javafx.stage.Stage;

public class Msgbox extends Stage {

	public Msgbox(String title, String message) {
		initModality(Modality.APPLICATION_MODAL);
		setTitle(title);
		Button btn1 = new Button("Ok");
		VBox vb1 = new VBox();
		vb1.getChildren().addAll(new Text(message), btn1);
		vb1.setAlignment(Pos.CENTER);
		vb1.setPadding(new Insets(9.0));
		vb1.setSpacing(17.0);
		btn1.setOnAction(event -> {
			Msgbox.this.hide();
			Msgbox.this.close();
		});
		vb1.setMinWidth(150);
		setScene(new Scene(vb1));
	}
}
