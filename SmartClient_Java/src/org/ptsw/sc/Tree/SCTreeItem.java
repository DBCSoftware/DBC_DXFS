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

package org.ptsw.sc.Tree;

import java.util.Stack;

import javafx.beans.property.ObjectProperty;
import javafx.beans.property.ReadOnlyObjectProperty;
import javafx.beans.property.SimpleObjectProperty;
import javafx.scene.control.TreeItem;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;

public class SCTreeItem<T> extends TreeItem<T> {

	private int currentLinePosition = SCTree.INVALIDCURRENTLINE;
	private final ObjectProperty<Color> treeTextColor; 
	private ObjectProperty<Color> foregroundTextColor = new SimpleObjectProperty<>();
	private final ObjectProperty<Font> font;

	public SCTreeItem() {
		super();
		treeTextColor = null;
		font = null;
	}

	public SCTreeItem(T value, ObjectProperty<Color> foregroundTextColor, Font font) {
		super(value);
		this.foregroundTextColor.bind(foregroundTextColor);
		treeTextColor = foregroundTextColor;
		this.font = new SimpleObjectProperty<>(font);
	}

	/**
	 * @return The zero-based index of what this item considers its 'current line' to be.
	 * Or possibly SCTree.INVALIDCURRENTLINE if there is no current line.
	 */
	int getCurrentLinePosition() {
		return currentLinePosition;
	}

	/**
	 * @param currentLinePosition the currentLinePosition to set
	 * @throws Exception 
	 */
	void setCurrentLinePosition(int currentLinePosition) throws Exception {
		if (currentLinePosition == SCTree.INVALIDCURRENTLINE ||
				(0 <= currentLinePosition && currentLinePosition < getChildren().size())) {
			this.currentLinePosition = currentLinePosition;
		}
		else throw new Exception(String.format("Bad setCurrentLinePosition = %d", currentLinePosition));
	}
	
	@Override
	public String toString() {
		return (String) getValue();
	}
	
	/**
	 * @return The One-Based comma separated list of indexes leading from the root
	 * to this item.
	 */
	public String getIndexCSV() {
		TreeItem<T> ti1 = this;
		Stack<Integer> stack = new Stack<>();
		int li = getParent().getChildren().indexOf(ti1);
		stack.push(li + 1);
		while((ti1 = ti1.getParent()) != null) {
			if (ti1.getParent() == null) break;
			li = ti1.getParent().getChildren().indexOf(ti1);
			stack.push(li + 1);
		}
		StringBuilder sb1 = new StringBuilder();
		while(!stack.empty()) {
			sb1.append(stack.pop().intValue()).append(',');
		}
		sb1.setLength(sb1.length() - 1);
		return sb1.toString();
	}

	/**
	 * @return the foregroundTextColor
	 */
	public ReadOnlyObjectProperty<Color> getForegroundTextColor() {
		return foregroundTextColor;
	}

	void setColor(Color newColor) {
		if (newColor == null) {
			foregroundTextColor.bind(treeTextColor);
		}
		else {
			foregroundTextColor.unbind();
			foregroundTextColor.set(newColor);
		}
	}

	/**
	 * @return the Font Property
	 */
	public ReadOnlyObjectProperty<Font> getFontProperty() {
		return font;
	}
	
	public Font getFont() { return font.get(); }
	
	public void setFont(Font font) { this.font.set(font); }
}
