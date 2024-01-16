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

import org.ptsw.sc.xml.Attribute;
import org.ptsw.sc.xml.Element;

import javafx.event.EventHandler;
import javafx.scene.Scene;
import javafx.scene.control.ScrollPane;
import javafx.scene.control.ScrollPane.ScrollBarPolicy;
import javafx.scene.input.KeyEvent;
import javafx.scene.input.MouseEvent;
import javafx.scene.layout.Pane;
import javafx.stage.Stage;
import javafx.stage.StageStyle;
import javafx.stage.Window;
import javafx.stage.WindowEvent;

class FloatWindow extends WindowDevice {

	private ScrollPane rootPane;
	private Window owner;
	
	/********************************************************************************
	 * 
	 * Constructor
	 * 
	 * The runtime will *always* send a size, even if the user does not specify one.
	 * It will also *always* send a title. If the user does not specify one, it will send
	 * the device name as the title.
	 * 
	 * @param element The XML 'PREP' Element from the server
	 */
	FloatWindow(Element element) {
		super(element);
		name = element.getAttributeValue(Client.FLOATWINDOW);
		if (Client.isDebug()) System.out.println("Entering FloatWindow, " + name);
		for(Attribute a1 : element.getAttributes()) {
			switch (a1.name) {
			case "owner":
				SCWindow windev = Client.getWindow(a1.value);
				if (windev != null) owner = windev.getStage();
				break;
			}
		}
		
		SmartClientApplication.Invoke(new Runnable() {
			@Override public void run() {
				stage = new Stage(StageStyle.DECORATED);  // Creates a new instance of decorated Stage.
				stage.setTitle(title);
				stage.setResizable(false);
				rootPane = new ScrollPane(new Pane());
				if (suppressScrollbars) {
					rootPane.setHbarPolicy(ScrollBarPolicy.NEVER);
					rootPane.setVbarPolicy(ScrollBarPolicy.NEVER);
				}
				scene = new Scene(rootPane, size.width, size.height);
				stage.setScene(scene);
				stage.sizeToScene();
				stage.setX(position.x);
				stage.setY(position.y);
				stage.setOnCloseRequest(new EventHandler<WindowEvent>() {
	                @Override
	                public void handle(WindowEvent event) {
	                    event.consume();
	                    Client.WindowCloseRequest(name);
	                }
	            });
				xPropertyName = stage.xProperty().getName();
				yPropertyName = stage.yProperty().getName();
				stage.addEventHandler(KeyEvent.ANY, keyEventHandler);
				stage.addEventHandler(MouseEvent.ANY, mouseEventHandler);
				stage.xProperty().addListener(winPosChangeListener);
				stage.yProperty().addListener(winPosChangeListener);
				if (owner != null) stage.initOwner(owner);
				stage.show();
				stage.focusedProperty().addListener(focusListener);
			}
		}, true);
	}

	void Close() {
		SmartClientApplication.Invoke(new Runnable() {
			@Override
			public void run() {
				stage.close();
			}
		}, true);
	}

	@Override
	protected Pane getContentPane() {
		return (Pane) rootPane.getContent();
	}

	@Override
	public void HideResource(Resource res) {
		assert res instanceof Panel;
		if (res instanceof Panel) HidePanel((Panel)res);
	}

}
