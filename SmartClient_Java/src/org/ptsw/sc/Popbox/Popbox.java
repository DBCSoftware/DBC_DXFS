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

package org.ptsw.sc.Popbox;

import org.ptsw.sc.Client;
import org.ptsw.sc.ClientUtils;
import org.ptsw.sc.Resource;

import javafx.beans.property.ObjectProperty;
import javafx.beans.property.ReadOnlyObjectProperty;
import javafx.beans.property.SimpleObjectProperty;
import javafx.beans.property.SimpleStringProperty;
import javafx.beans.property.StringProperty;
import javafx.beans.value.ChangeListener;
import javafx.event.ActionEvent;
import javafx.event.EventHandler;
import javafx.geometry.Insets;
import javafx.geometry.Orientation;
import javafx.scene.control.Button;
import javafx.scene.control.ContentDisplay;
import javafx.scene.control.Label;
import javafx.scene.input.KeyCode;
import javafx.scene.input.KeyEvent;
import javafx.scene.input.MouseButton;
import javafx.scene.layout.Background;
import javafx.scene.layout.BackgroundFill;
import javafx.scene.layout.Border;
import javafx.scene.layout.BorderStroke;
import javafx.scene.layout.BorderStrokeStyle;
import javafx.scene.layout.BorderWidths;
import javafx.scene.layout.CornerRadii;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.Region;
import javafx.scene.paint.Color;
import javafx.scene.shape.SVGPath;
import javafx.scene.text.Font;
import javafx.scene.text.Text;
import javafx.stage.Screen;

public class Popbox extends HBox {

	private final static double dpi = Screen.getPrimary().getDpi();
	private static final Background defaultBackground = new Background(new BackgroundFill(Color.WHITE, null, null));
	private static final Background focusedBackground = new Background(new BackgroundFill(Color.BLUE, null, null));
	private StringProperty text = new SimpleStringProperty();
	private ObjectProperty<Font> font = new SimpleObjectProperty<>();
	private final Resource resource;
	private EventHandler<ActionEvent> actionHandler;
	private boolean inTable = false;
	private final Label label = new Label();
	private final Button button = new Button();
	private double textHeight;
	private Background currentBackground;
	private Color currentTextFill;
	private double scale;
	
	/**
	 * Sized for a 14 pt font
	 * Which is actually about 20 pixels high.
	 */
	private final static String SVGdata = "M 5 5 L 15 5 L 10 15 z";
	private final SVGPath tripath = new SVGPath();
	
	public Popbox(Resource resource, String item, int hsize, Color textColor) {
		if (Client.isDebug()) System.out.println("Entering Popbox(Resource resource, String item, int hsize, Color textColor)");
		currentTextFill = textColor;
		label.setTextFill(currentTextFill);
		setId(item);
		setWidth(hsize);
		this.resource = resource;
		setBorder(new Border(new BorderStroke(Color.BLACK, BorderStrokeStyle.SOLID, CornerRadii.EMPTY, new BorderWidths(0.5))));
		setBackground(defaultBackground);
		currentBackground = defaultBackground;
		setupNode();

	}

	public Popbox(Font font, double hsize) {
		if (Client.isDebug()) System.out.println("Entering Popbox(Font font, double width)");
		setFont(font);
		resource = null;
		setWidth(hsize - 4);
		setBackground(defaultBackground);
		currentBackground = defaultBackground;
		setupNode();
		setupEventHandlersFilters();
		addEventFilter(KeyEvent.KEY_PRESSED, ke -> {
			if (ke.getCode() == KeyCode.ENTER) ke.consume();
		});
	}
	
	private void setupNode() {
		label.textProperty().bind(text);
		label.setMaxWidth(Double.MAX_VALUE);
		label.setMaxHeight(Double.MAX_VALUE);
		HBox.setHgrow(label, Priority.ALWAYS);
		HBox.setMargin(label, new Insets(0, 0, 0, 2));
		button.prefHeightProperty().bind(prefHeightProperty());
		button.setMaxHeight(Region.USE_PREF_SIZE);
		button.setMinHeight(Double.MIN_VALUE);
		button.setMinWidth(Double.MIN_VALUE);
		label.heightProperty().addListener((ChangeListener<Number>) (observable, oldValue, newValue) -> {
			double contentHeight = newValue.doubleValue();
			button.setPrefWidth(0.78f * contentHeight);
			button.setMaxWidth(Region.USE_PREF_SIZE);
			requestLayout();
		});
		getChildren().addAll(label, button);
		focusedProperty().addListener((ChangeListener<Boolean>) (observable, oldValue, newValue) -> {
			if (newValue.booleanValue()) {
				setBackground(focusedBackground);
				label.setTextFill(Color.WHITE);
			}
			else {
				setBackground(currentBackground);
				label.setTextFill(currentTextFill);
			}
		});
	}
	
	public void setFont(Font font) {
		this.font.setValue(font);
		setHeight(getFont().getSize() * 1.8f);
		label.setFont(font);
		Text t1 = new Text("ABC");
		t1.setFont(getFont());
		t1.snapshot(null, null);
		textHeight = t1.getLayoutBounds().getHeight();
		scale = textHeight * 72f / (dpi * 14f);
		tripath.setContent(SVGdata);
		if (!inTable) {
			tripath.setFill(Color.BLACK);
			tripath.setStroke(Color.BLACK);
			scale *= 0.75f;
		}
		else {
			tripath.setFill(Color.DARKSLATEGRAY);
			tripath.setStroke(Color.DARKSLATEGRAY);
			scale *= 0.7f;
		}
		tripath.setRotate(180f);
		tripath.setScaleX(scale);
		tripath.setScaleY(scale);
		button.setGraphic(tripath);
		button.setContentDisplay(ContentDisplay.GRAPHIC_ONLY);
	}
	
    public void setText(String value) {
        textProperty().set(value);
    }

    public String getText() {
    	return textProperty().get();
    }

    public StringProperty textProperty() {
    	return text;
    }
    
    public ReadOnlyObjectProperty<Font> fontProperty() {
    	return font;
    }
    
	public void setColor(Color newColor) {
		currentTextFill = newColor;
		label.setTextFill(newColor);
	}
	
    public Font getFont() {
    	return fontProperty().get();
    }
    
    Resource getResource() { return resource; }
    
	@Override
	public boolean isResizable() {
		return false;
	}
	
	@Override
	public Orientation getContentBias() {
		return null;
	}

	public void setOnAction(EventHandler<ActionEvent> actionHandler) {
		this.actionHandler = actionHandler;
	}
	
	EventHandler<ActionEvent> getActionHandler() { return actionHandler; }

	/**
	 * @return the inTable
	 */
	public boolean isInTable() {
		return inTable;
	}

	/**
	 * @param inTable the inTable to set
	 */
	public void setInTable(boolean inTable) {
		this.inTable = inTable;
		setBorder(Border.EMPTY);
		if (Client.isDebug()) System.out.printf("Popbox.setInTable, in=%b\n", inTable);
	}

	private void setupEventHandlersFilters() {
		addEventFilter(javafx.scene.input.MouseEvent.MOUSE_PRESSED, me -> {
	    	me.consume();
	    	requestFocus();
	    	button.arm();
		});
		addEventFilter(javafx.scene.input.MouseEvent.MOUSE_CLICKED, me -> {
	    	me.consume();
	    	requestFocus();
		});
		addEventFilter(javafx.scene.input.MouseEvent.MOUSE_RELEASED, me -> {
	    	me.consume();
		    if (me.getButton() == MouseButton.PRIMARY) {
		    	getActionHandler().handle(new ActionEvent(this, null));
		    }
	    	requestFocus();
	    	button.disarm();
		});
		addEventFilter(KeyEvent.KEY_RELEASED, ke -> {
			if (ke.getCode() == KeyCode.ENTER) {
				ke.consume();
			}
		});
	}

	/*
	 * Called only for stand alone Popbox. Not for InTable
	 */
	public void renderPart() {
		setupEventHandlersFilters();
		addEventFilter(KeyEvent.KEY_PRESSED, ke -> {
			if (ke.getCode() == KeyCode.ENTER) {
				ke.consume();
				Client.NkeyMessage(this.getResource().getId(),
						getId(), ClientUtils.FKeyMap.get(KeyCode.ENTER).toString());
			}
		});
		button.setId(getId());
	}

	public void textbgcolor(Color newColor) {
		currentBackground = new Background(new BackgroundFill(newColor, null, null));
		setBackground(currentBackground);
	}
}

