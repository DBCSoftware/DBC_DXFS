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

package org.ptsw.sc.ListDrop;

import javafx.beans.property.ObjectProperty;
import javafx.beans.property.SimpleObjectProperty;
import javafx.beans.value.ChangeListener;
import javafx.beans.value.ObservableValue;
import javafx.geometry.Pos;
import javafx.scene.Node;
import javafx.scene.control.ContentDisplay;
import javafx.scene.control.Label;
import javafx.scene.layout.Pane;
import javafx.scene.layout.Region;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;

/**
 * Represents the data for a single line item in a DB/C dropbox or listbox
 */
public abstract class ListDropItem {

	enum foregroundColorState {
		DEFAULT, /* FG Color is that of the parent 'box' */
		SPECIAL  /* FG Color has been set on this individual line */
	}
	
	//protected static final String BACKGROUNDLIGHTGRAY = "-fx-background-color: lightgray;";
	//protected static final String BACKGROUNDBLUE = "-fx-background-color: blue;";
	
	/*
	 * In DB/C world, the text of an item in a dropbox or listbox cannot be changed
	 */
	private final String[] textParts;
	
	/*
	 * These are created new every time this item is attached to a cell
	 */
	protected Label[] labelParts;
	
	/*
	 * Not used locally. The Style Property of the List Cell that is currently
	 * displaying me is bound to this property
	 */
	//protected final SimpleObjectProperty<String> bgColorStyle;
	
	final ObjectProperty<Font> font;
	private final ObjectProperty<Color> foregroundColor;
	
	private foregroundColorState fgColorState = foregroundColorState.DEFAULT;
	private Color defaultFGColor;
	private Color specialFGColor;
	private final SCListDropBase listDropBase;
	
	ListDropItem(ObservableValue<Color> color, Font font, String text, SCListDropBase parent) {
		//bgColorStyle = new SimpleObjectProperty<String>(SCListDropBase.BACKGROUNDTRANSPARENT);
		defaultFGColor = color.getValue();
		specialFGColor = null;
		this.font = new SimpleObjectProperty<>(font);
		foregroundColor = new SimpleObjectProperty<>(defaultFGColor);
		listDropBase = parent;
		
		color.addListener((ChangeListener<Color>) (obsrvbl, oldColor, newColor) -> setDefaultFGColor(newColor));
		if (listDropBase != null && listDropBase.Boxtabs != null) {
			if (text.indexOf('\t') != -1) {
				int i1 = 0;
				String [] strings = text.split("\t");
				textParts = new String[strings.length];
				for (String s1 : strings) {
					textParts[i1++] = s1;
				}
			}
			else textParts = new String[] {text};
		}
		else textParts = new String[] {text};
		labelParts = new Label[textParts.length];
	}
	
	@Override
	public String toString() {
		StringBuilder sb1 = new StringBuilder();
		for (String s1 : textParts) {
			sb1.append(s1).append('\t');
		}
		sb1.setLength(sb1.length() - 1);
		return sb1.toString();
	}

//	private boolean isListView() {
//		return listDropBase.getNode() instanceof ListView;
//	}
	
//	private boolean isComboBox() {
//		return listDropBase.getNode() instanceof ComboBox;
//	}
	
//	abstract void setFGColor();
	
//	abstract void setBGColor();
	
	void setBackgroundColor(Color color) {
		//if (color == null) backgroundColorStyle = null;
		//else backgroundColorStyle = Resource.getBackgroundColorStyle(color);
		//setBGColor();
	}

	void setDefaultFGColor(Color newColor) {
		defaultFGColor = newColor;
		if (fgColorState == foregroundColorState.DEFAULT) foregroundColor.set(defaultFGColor);
		//setFGColor();
	}

	void setSpecialFGColor(Color newColor) {
		specialFGColor = newColor;
		if (specialFGColor != null) {
			fgColorState = foregroundColorState.SPECIAL;
			foregroundColor.set(specialFGColor);
		}
		else {
			fgColorState = foregroundColorState.DEFAULT;
			foregroundColor.set(defaultFGColor);
		}
		//setFGColor();
	}

	//void setSelected(boolean selected) {
		//this.selected = selected;
		//setColors();
	//}

	public String getText() {
		return toString();
	}

	/*
	 * TODO Can be optimized for lists? Only need to create fresh Labels for ComboBox I think
	 */
	private Label[] createLabel()
	{
		int i1 = 0;
		int xpos = 0;
		for (String s1 : textParts) {
			Label label1 = new Label(s1);
			label1.setContentDisplay(ContentDisplay.TEXT_ONLY);
			label1.fontProperty().bind(this.font);
			label1.textFillProperty().bind(foregroundColor);
			/*
			 * The background of the Label is always transparent
			 */
			label1.styleProperty().setValue(SCListDropBase.BACKGROUNDTRANSPARENT);
			if (listDropBase != null && listDropBase.Boxtabs != null) {
				int width = listDropBase.Boxtabs[i1];
				Pos ta = listDropBase.BoxtabsTextAlignment[i1];
				label1.setAlignment(ta);
				label1.relocate(xpos,  0);
				label1.setPrefWidth(width);
				label1.setMinWidth(Region.USE_PREF_SIZE);
				label1.setMaxWidth(Region.USE_PREF_SIZE);
				xpos += width;
			}
			labelParts[i1++] = label1;
		}
		//setFGColor();
		return labelParts;
	}

	
	/**
	 * This method creates a Node for the ListCell of the required type
	 * so that outside this class, the exact type need not be known.
	 * @return A Node for use in a SCListCell as the Graphic
	 */
	Node getNode() {
		Pane p1 = new Pane();
		p1.styleProperty().setValue(SCListDropBase.BACKGROUNDTRANSPARENT);
		p1.getChildren().setAll(createLabel());
		return p1;
	}
}
