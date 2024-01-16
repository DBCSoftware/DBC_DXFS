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

import java.awt.Dimension;

import org.ptsw.sc.xml.Element;

import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.stage.Modality;
import javafx.stage.Stage;
import javafx.stage.StageStyle;

public class Dialog extends Resource {

	private Stage stage;
	private Scene scene;
	
	public Dialog(Element element) throws Exception {
		super(element);
		if (Client.IsTrace()) System.out.println("Entering Dialog<>");
		setId(element.getAttributeValue(Client.DIALOG));
	}

	public String getTitle() { return title; }
	
	public Dimension getSize() { return new Dimension(horzSize, vertSize); }

	public void showOn(final SCWindow win1, final int X, final int Y) {
		if (Client.IsTrace()) System.out.println("Entering Dialog.showOn");
		SmartClientApplication.Invoke(new Runnable() {
			@Override
			public void run() {
				stage = new Stage(StageStyle.DECORATED);  // Creates a new instance of decorated Stage.
				stage.setTitle(getTitle());
				stage.initOwner(win1.getStage());
				stage.initModality(Modality.WINDOW_MODAL);
				Dimension size = getSize();
				scene = new Scene((Parent) getContent(), size.width, size.height);
				stage.setScene(scene);
				stage.sizeToScene();
				if (X != -1 && Y != -1) {
					stage.setX(X);
					stage.setY(Y);
				}
				else stage.centerOnScreen();
				stage.show();
				SetVisibleOn(win1);
			}
		}, true);
	}

	@Override
	public void Hide() {
		SmartClientApplication.Invoke(new Runnable() {
			@Override
			public void run() {
				stage.hide();
			}
		}, true);
	}

	@Override
	public void Close() {
		if (stage != null) {
			SmartClientApplication.Invoke(new Runnable() {
				@Override
				public void run() {
					stage.close();
				}
			}, true);
		}
	}

	void changeTitle(String data) {
		title = data;
		stage.setTitle(getTitle());
	}
	
}
