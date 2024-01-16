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

package org.ptsw.sc.SpecialDialogs;

import java.util.Objects;

import org.ptsw.sc.Client;
import org.ptsw.sc.Resource;
import org.ptsw.sc.SCWindow;
import org.ptsw.sc.SmartClientApplication;
import org.ptsw.sc.xml.Element;

import javafx.application.Platform;
import javafx.event.ActionEvent;
import javafx.event.EventHandler;
import javafx.geometry.Insets;
import javafx.geometry.Orientation;
import javafx.geometry.Pos;
import javafx.scene.Node;
import javafx.scene.Scene;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.control.Slider;
import javafx.scene.control.Tooltip;
import javafx.scene.layout.HBox;
import javafx.scene.layout.VBox;
import javafx.scene.text.Text;
import javafx.stage.Modality;
import javafx.stage.Stage;
import javafx.stage.StageStyle;
import javafx.stage.Window;
import javafx.stage.WindowEvent;

public class SCColorDialog extends Resource {

	private final EventHandler<WindowEvent> closeHandler = arg0 -> {
		Object o1 = arg0.getSource();
		if (o1 instanceof myColorDialog) {
			myColorDialog source = (myColorDialog)o1;
			if (source.buttonPressed.equals(myColorDialog.OK)) {
				double red = source.redsp.getValue();
				double green = source.grnsp.getValue();
				double blue = source.blusp.getValue();
				String s1 = String.format("%05.0f%05.0f%05.0f", red, green, blue);
				Client.OkMessage(specialDialogResourceName, s1);
			}
			else Client.CancelMessage(specialDialogResourceName);
		}
		else Client.CancelMessage(specialDialogResourceName);
		try {
			Client.sendRMessageNoError();
		}
		catch (Exception e) {
			if (Client.isDebug()) System.err.println(e);
		}
	};
	
	private static final Text redlabel = new Text("R");
	private static final Text grnlabel = new Text("G");
	private static final Text blulabel = new Text("B");

	private class myColorDialog extends Stage {

		private static final String OK = "ok";
		private static final String CANC = "canc";
		
		private String buttonPressed;
		private final Slider redsp = new Slider(0, 255, 0);
		private final Slider grnsp = new Slider(0, 255, 0);
		private final Slider blusp = new Slider(0, 255, 0);
		private final Tooltip redtt = new Tooltip("Red");
		private final Tooltip grntt = new Tooltip("Green");
		private final Tooltip blutt = new Tooltip("Blue");
		private final Label lblRed = new Label("000");
		private final Label lblGreen = new Label("000");
		private final Label lblBlue = new Label("000"); 
		
		private final EventHandler<ActionEvent> handler = event -> {
			buttonPressed = ((Node)event.getSource()).getId();
			lblRed.textProperty().unbind();
			lblGreen.textProperty().unbind();
			lblBlue.textProperty().unbind();
			myColorDialog.this.close();
		};
		
		public myColorDialog(StageStyle style) {
			super(style);
			setTitle("RGB Color");
			initModality(Modality.APPLICATION_MODAL);
			redsp.setOrientation(Orientation.VERTICAL);
			redsp.setTooltip(redtt);
			grnsp.setOrientation(Orientation.VERTICAL);
			grnsp.setTooltip(grntt);
			blusp.setOrientation(Orientation.VERTICAL);
			blusp.setTooltip(blutt);
			
			lblRed.textProperty().bind(redsp.valueProperty().asString("%03.0f"));
			lblRed.setMinWidth(22);

			lblGreen.textProperty().bind(grnsp.valueProperty().asString("%03.0f"));
			lblGreen.setMinWidth(22);

			lblBlue.textProperty().bind(blusp.valueProperty().asString("%03.0f"));
			lblBlue.setMinWidth(22);

			VBox vgrpRed = new VBox(redlabel, redsp, lblRed);
			vgrpRed.setPadding(new Insets(9.0));
			vgrpRed.setSpacing(8.0);
			VBox vgrpGreen = new VBox(grnlabel, grnsp, lblGreen);
			vgrpGreen.setPadding(new Insets(9.0));
			vgrpGreen.setSpacing(8.0);
			VBox vgrpBlue = new VBox(blulabel, blusp, lblBlue);
			vgrpBlue.setPadding(new Insets(9.0));
			vgrpBlue.setSpacing(8.0);
			
			HBox hb1 = new HBox(vgrpRed, vgrpGreen, vgrpBlue);
			hb1.setSpacing(30);
			hb1.setAlignment(Pos.CENTER);
			Button OkBtn = new Button("OK");
			OkBtn.setId(OK);
			Button CancBtn = new Button("Cancel");
			CancBtn.setId(CANC);
			HBox hboxButtons = new HBox(OkBtn, CancBtn);
			hboxButtons.setSpacing(15);
			hboxButtons.setAlignment(Pos.CENTER);
			VBox vb1 = new VBox(hb1, hboxButtons);
			vb1.setAlignment(Pos.CENTER);
			vb1.setPadding(new Insets(9.0));
			vb1.setSpacing(17.0);
			OkBtn.setOnAction(handler);
			CancBtn.setOnAction(handler);
			vb1.setMinWidth(150);
			setScene(new Scene(vb1));
		}
	
	}

	private myColorDialog dialog;
	
	public SCColorDialog(Element element) throws Exception {
		super(element);
	}
	
	private void createDialog(final Window stage) {
		try {
			SmartClientApplication.runAndWait(() -> {
				dialog = new myColorDialog(StageStyle.UTILITY);
				dialog.addEventHandler(WindowEvent.WINDOW_HIDING, closeHandler);
				dialog.initOwner(stage);
			});
		}
		catch (RuntimeException e) {
			if (Client.isDebug()) System.err.println(e);
		}
	}
	
	@Override
	public void Close() {
		if (Objects.nonNull(dialog)) {
			dialog.close();
			dialog = null;
		}
	}
	
	@Override
	public void Hide() {
		/* Do nothing, should never be called */
	}
	
	public void showOn(SCWindow win1) {
		createDialog(win1.getStage());
		Platform.runLater(() -> {dialog.show();});
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.Resource#setId(java.lang.String)
	 */
	@Override
	public void setId(String id) {
		specialDialogResourceName = id;
	}

	/* (non-Javadoc)
	 * @see com.dbcswc.sc.Resource#getId()
	 */
	@Override
	public String getId() {
		return specialDialogResourceName;
	}

}
