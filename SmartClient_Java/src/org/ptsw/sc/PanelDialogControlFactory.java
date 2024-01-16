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
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Map.Entry;

import org.ptsw.sc.ListDrop.Boxtabable;
import org.ptsw.sc.ListDrop.InsertOrderable;
import org.ptsw.sc.ListDrop.ListDropItem;
import org.ptsw.sc.ListDrop.SCDropbox;
import org.ptsw.sc.ListDrop.SCListbox;
import org.ptsw.sc.ListDrop.SCMListbox;
import org.ptsw.sc.Popbox.Popbox;
import org.ptsw.sc.Tabgroup.SCTabgroup;
import org.ptsw.sc.Table.Column;
import org.ptsw.sc.Table.RowData;
import org.ptsw.sc.Table.Table;
import org.ptsw.sc.Tree.SCTree;
import org.ptsw.sc.xml.Attribute;
import org.ptsw.sc.xml.Element;

import javafx.beans.value.ChangeListener;
import javafx.beans.value.ObservableValue;
import javafx.collections.ObservableList;
import javafx.event.EventHandler;
import javafx.geometry.Orientation;
import javafx.geometry.Pos;
import javafx.scene.Node;
import javafx.scene.Parent;
import javafx.scene.control.Button;
import javafx.scene.control.ComboBoxBase;
import javafx.scene.control.ContentDisplay;
import javafx.scene.control.Control;
import javafx.scene.control.Label;
import javafx.scene.control.RadioButton;
import javafx.scene.control.ScrollBar;
import javafx.scene.control.SelectionModel;
import javafx.scene.control.Tab;
import javafx.scene.control.TextField;
import javafx.scene.control.ToggleGroup;
import javafx.scene.control.Tooltip;
import javafx.scene.control.TreeView;
import javafx.scene.image.ImageView;
import javafx.scene.input.KeyEvent;
import javafx.scene.input.MouseButton;
import javafx.scene.input.MouseEvent;
import javafx.scene.layout.Region;
import javafx.scene.paint.Color;
import javafx.scene.shape.Rectangle;
import javafx.scene.text.Font;
import javafx.scene.text.TextAlignment;

public class PanelDialogControlFactory {

	private Node lastGrayableNode;
	private ShowOnlyable lastShowonlyable;
	private ReadOnlyable lastReadonlyable;
	private Region lastHelptextableControl;
	private Control lastNoFocusAble;
	private TextField lastJustifyableControl;
	private InsertOrderable lastInsertorderableControl;
	private Boxtabable lastBoxtabable;
	private final Point2D.Double pos = new Point2D.Double(0f, 0f);
	private SCFontAttributes currentFontAttributes;
	private Font currentFont;
	private Color currentTextColor;
	private final Resource parent;
	
	/**
	 * Sets the default font and text color
	 * @param res The DB/C Resource that is using this factory
	 */
	public PanelDialogControlFactory(final Resource res) {
		parent = res;
		currentFontAttributes = SCFontAttributes.getDefaultFontAttributes();
		currentFont = currentFontAttributes.getFont();
		currentTextColor = Color.BLACK;
	}

	public PanelDialogControlFactory(PanelDialogControlFactory inherit) {
		parent = inherit.getParent();
		currentFontAttributes = inherit.getCurrentFontAttributes();
		currentFont = currentFontAttributes.getFont();
		currentTextColor = inherit.getCurrentTextColor();
	}
	
	private Object createNode(Element e1) throws Exception {
		Button button = null;
		String item = e1.getAttributeValue("i");
		int hsize = (e1.hasAttribute("hsize")) ?
				Integer.parseInt(e1.getAttributeValue("hsize")): -1;
		int vsize = (e1.hasAttribute("vsize")) ?
				Integer.parseInt(e1.getAttributeValue("vsize")): -1;
		String text = (e1.hasAttribute("t")) ? e1.getAttributeValue("t") : "";
		switch (e1.name) {
		case "p":
			for(Attribute a2 : e1.getAttributes()) {
				switch (a2.name) {
				case "h":
					pos.x = Double.parseDouble(a2.value);
					break;
				case "v":
					pos.y = Double.parseDouble(a2.value);
					break;
				case "ha":
					pos.x += Double.parseDouble(a2.value);
					break;
				case "va":
					pos.y += Double.parseDouble(a2.value);
					break;
				}
			}
			break;
		case "atext":
			return createVariableText(item, text, TextAlignment.LEFT, -1);
		case "box":
			return createBox(hsize, vsize, text);
		case "boxtabs":
			if (lastBoxtabable != null) {
				processBoxtabs(lastBoxtabable, e1);
			}
			break;
		case "buttongroup":
			return createButtonGroup(e1.getChildren());
		case "catext":
			return createVariableText(item, text, TextAlignment.CENTER, hsize);
		case "checkbox":
			return createCheckbox(item, text, ContentDisplay.LEFT);
		case "ctext":
			return createStaticText(text, TextAlignment.CENTER, hsize);
		case "cvtext":
			return createVariableText(item, text, TextAlignment.CENTER, hsize);
		case "defpushbutton":
			button = createPushbutton(item, hsize, vsize, text);
			button.setDefaultButton(true); 
			return button;
		case "dropbox":
			return createDropbox(item, hsize, vsize);
		case "edit":
			return createEditBox(item, hsize, text, false, -1);
		case "escpushbutton":
			button = createPushbutton(item, hsize, vsize, text);
			button.setCancelButton(true);
			return button;
		case "fedit":
			return createFedit(item, hsize, e1.getAttributeValue("mask"), text);
		case "font":
			currentFontAttributes = currentFontAttributes.Merge(e1);
			currentFont = currentFontAttributes.getFont();
			break;
		case Resource.GRAY:
			if (lastGrayableNode != null) {
				lastGrayableNode.setDisable(true);
			}
			break;
		case "helptext":
			if (lastHelptextableControl != null) {
				((Control)lastHelptextableControl).setTooltip(new Tooltip(e1.getAttributeValue("t")));
			}
			break;
		case "hscrollbar":
			return createScrollbar(item, hsize, vsize, Orientation.HORIZONTAL);
			
		/**
		 * A VICON uses the 'icon' Element tag name and includes an item attribute
		 */
		case Client.ICON:
			return createIcon(item, e1.getAttributeValue(Client.IMAGE));

		case "iconpushbutton":
			IconResource ico1 = Client.getIcon(e1.getAttributeValue(Client.IMAGE));
			button = createPushbutton(item, hsize, vsize, ico1.getImage());
			return button;
		case "icondefpushbutton":
			ico1 = Client.getIcon(e1.getAttributeValue(Client.IMAGE));
			button = createPushbutton(item, hsize, vsize, ico1.getImage());
			button.setDefaultButton(true);
			return button;
		case "iconescpushbutton":
			ico1 = Client.getIcon(e1.getAttributeValue(Client.IMAGE));
			button = createPushbutton(item, hsize, vsize, ico1.getImage());
			button.setCancelButton(true);
			return button;
		case "insertorder":
			if (lastInsertorderableControl != null) {
				lastInsertorderableControl.setInsertOrder();
			}
			break;
		case "justifycenter":
			if (lastJustifyableControl != null) {
				lastJustifyableControl.setAlignment(Pos.CENTER);
			}
			break;
		case "justifyright":
			if (lastJustifyableControl != null) {
				lastJustifyableControl.setAlignment(Pos.CENTER_RIGHT);
			}
			break;
		case "ledit":
			int i1 = Integer.parseInt(e1.getAttributeValue("maxchars"));
			return createEditBox(item, hsize, text, false, i1);
		case "listbox":
		case "listboxhs":
			Node n1 = createListbox(item, hsize, vsize, false);
			/*
			 * Trying a couple of things to disable listbox scroll bars.
			 * The next bits of code do not work as of J2SE 1.8.0_11
			 * Maybe it never will.
			 */
			if (e1.name.equals("listbox")) {
				// Method one
				ScrollBar scrollBarh = (ScrollBar)n1.lookup(".scroll-bar:horizontal");
				if (scrollBarh != null) {
					scrollBarh.setDisable(true);
				}
				// Method two
				n1.setStyle("-fx-hbar-policy:never;");
			}
			else
				n1.setStyle("-fx-hbar-policy:as_needed;");
			return n1;
		case "ltcheckbox":
			return createCheckbox(item, text, ContentDisplay.RIGHT);
		case "mediths":
		case "medits":
		case "meditvs":
		case "medit":
			return createMeditbox(item, hsize, vsize, text, false);
		case "mledit":
		case "mlediths":
		case "mledits":
		case "mleditvs":
			SCTextArea mlbox1 = createMeditbox(item, hsize, vsize, text, true);
			i1 = Integer.parseInt(e1.getAttributeValue("maxchars"));
			mlbox1.setMaxlength(i1);
			return mlbox1;
		case "mlistbox":
		case "mlistboxhs":
			n1 = createListbox(item, hsize, vsize, true);
			/*
			 * The next bit of code does not work as of version b113 of JFX8.
			 * Maybe it never will.
			 */
			if (e1.name.equals("mlistbox")) 
				n1.setStyle("-fx-hbar-policy:never;");
			else
				n1.setStyle("-fx-hbar-policy:as_needed;");
			return n1;
		case "nofocus":
			if (lastNoFocusAble != null) lastNoFocusAble.setFocusTraversable(false);
			break;
		case "pedit":
			return createEditBox(item, hsize, text, true, -1);
		case "pledit":
			return createEditBox(item, hsize, text, true, Integer.parseInt(e1.getAttributeValue("maxchars")));
		case "popbox":
			return createPopbox(item, hsize);
		case "progressbar":
			return createProgressbar(item, hsize, vsize);
		case "pushbutton":
			return createPushbutton(item, hsize, vsize, e1.getAttributeValue("t"));
		case "ratext":
			return createVariableText(item, text, TextAlignment.RIGHT, -1);
		case "readonly":
			if (lastReadonlyable != null) {
				lastReadonlyable.setReadonly();
			}
			break;
		case "rtext":
			return createStaticText(text, TextAlignment.RIGHT, -1);
		case "rvtext":
			return createVariableText(item, text, TextAlignment.RIGHT, -1);
		case "showonly":
			if (lastShowonlyable != null) {
				lastShowonlyable.setShowonly();
			}
			break;
		case "tabgroup":
			return createTabgroup(hsize, vsize, e1);
		case "table":
			return createTable(item, hsize, vsize, e1);
		case "text":
			return createStaticText(text, TextAlignment.LEFT, -1);
		case Resource.TEXTCOLOR:
			currentTextColor = Resource.getColorExt(e1.getAttributeValue("color").trim().toUpperCase());
			break;
			
		case "tree":
			return createTree(item, hsize, vsize);
			
		case "vscrollbar":
			return createScrollbar(item, hsize, vsize, Orientation.VERTICAL);
		case "vtext":
			return createVariableText(item, text, TextAlignment.LEFT, -1);
		}
		return null;
	}

	private TreeView<String> createTree(String item, int hsize, int vsize) {
		SCTree<String> tv1 = new SCTree<>(parent, item, hsize, vsize, currentFont, currentTextColor);
		tv1.relocate(pos.x, pos.y);
		lastGrayableNode = tv1;
		lastShowonlyable = tv1;
		lastReadonlyable = tv1;
		lastHelptextableControl = tv1;
		tv1.focusedProperty().addListener(parent.getFocusListener(tv1));
		tv1.addEventFilter(KeyEvent.KEY_PRESSED, parent.getNkeyHandler());
		return tv1;
	}
	
	@SuppressWarnings("rawtypes")
	private Table<RowData> createTable(String item, int hsize, int vsize, Element e1) throws Exception {
		Table<RowData> tbl1 = new Table<>(parent, item, hsize, vsize, e1, currentFont, currentTextColor);
		tbl1.relocate(pos.x, pos.y);
		lastGrayableNode = tbl1;
		for (Entry<? extends String, ? extends Column> entry : tbl1.getColumnsMap().entrySet()) {
			parent.tableColumns.put(entry.getKey(), entry.getValue());
		}
		
		tbl1.setOnMouseClicked(new EventHandler<MouseEvent>() {

			@Override
			public void handle(MouseEvent event) {
				if (event.getButton() != MouseButton.PRIMARY) return;
				Node lt = (Node)event.getTarget();
				boolean isTableHeader = false;
				String t2 = null;
				if (lt instanceof Label) t2 = ((Label)lt).getText();
				Parent pnt = lt.getParent();
				if (t2 == null && pnt instanceof Label) t2 = ((Label)pnt).getText();
				while (pnt != null) {
					/**
					 * This is the tricky part that may fail in the future
					 * I'm looking for this thing
					 *    com.sun.javafx.scene.control.skin.TableColumnHeader
					 * in the parentage of the mouse click target Node.
					 */
					if (pnt.getClass().getName().endsWith(".TableColumnHeader"))
					{
						isTableHeader = true;
						break;
					}
					if (pnt.equals(event.getSource())) break;
					pnt = pnt.getParent();
				}
				if (isTableHeader && t2 != null) {
					Column column = ((Table)event.getSource()).getColumnByText(t2);
					if (column != null) parent.tableColumnClicked(column.getId());
				}
			}
			
		});
		return tbl1;
	}
	
	private Node createBox(int hsize, int vsize, String title) {
		if (title == null || title.length() < 1) {
			Rectangle bt1 = new Rectangle(hsize, vsize);
			bt1.setFill(null);
			bt1.setStroke(Color.BLACK);
			bt1.relocate(pos.x, pos.y);
			return bt1;
		}
		SCBoxTitle bt1 = new SCBoxTitle(hsize, vsize, title, currentFontAttributes
				, currentTextColor);
		bt1.relocate(pos.x, pos.y);
		return bt1;
	}
	
	private List<Node> createButtonGroup(Collection<Object> elements) {
		List<Node> list1 = new ArrayList<>();
		ToggleGroup tg1 = new ToggleGroup();
		Control lastButton = null;
		boolean firstButton = true;
		for (Object obj1 : elements) {
			Element e1 = (Element) obj1;
			switch (e1.name){
			case "button":
				final RadioButton tb1 = new RadioButton(e1.getAttributeValue("t"));
				tb1.setId(e1.getAttributeValue("i"));
				tb1.setToggleGroup(tg1);
				tb1.relocate(pos.x, pos.y);
				tb1.focusedProperty().addListener(parent.getFocusListener(tb1));
				list1.add(tb1);
				if (firstButton) {
					tb1.setSelected(true);
					firstButton = false;
				}
				else tb1.setFocusTraversable(false);
				lastButton = tb1;
				tb1.addEventFilter(KeyEvent.KEY_PRESSED, parent.getNkeyHandler());
				break;
			case "helptext":
				if (lastButton != null) {
					lastButton.setTooltip(new Tooltip(e1.getAttributeValue("t")));
				}
				break;
			case "p":
				for(Attribute a2 : e1.getAttributes()) {
					switch (a2.name) {
					case "h":
						pos.x = Double.parseDouble(a2.value);
						break;
					case "v":
						pos.y = Double.parseDouble(a2.value);
						break;
					case "ha":
						pos.x += Double.parseDouble(a2.value);
						break;
					case "va":
						pos.y += Double.parseDouble(a2.value);
						break;
					}
				}
				break;
			}
		}
		tg1.selectedToggleProperty().addListener(parent.getToggleListener());
		return list1;
	}
	


	private Node createCheckbox(final String item, String text, ContentDisplay dp) {
		SCCheckBox bx1 = new SCCheckBox(parent, text, item);
		bx1.setFont(currentFont);
		bx1.setTextFill(currentTextColor);
		bx1.relocate(pos.x, pos.y);
		bx1.setContentDisplay(dp); // NOT WORKING IN THIS VERSION OF FX, Can't do LTCHECKBOX
		bx1.setOnAction(parent.getCheckboxActionHandler());
		bx1.focusedProperty().addListener(parent.getFocusListener(bx1));
		bx1.addEventFilter(KeyEvent.KEY_PRESSED, parent.getNkeyHandler());
		lastGrayableNode = bx1;
		lastHelptextableControl = bx1;
		lastShowonlyable = bx1;
		return bx1;
	}
	
	@SuppressWarnings({ "unchecked", "rawtypes" })
	private Node createDropbox(final String item, final int hsize, final int vsize) throws Exception
	{
		SCDropbox db1;
		db1 = new SCDropbox(parent, item, hsize, currentFont, currentTextColor);
		db1.getControl().relocate(pos.x, pos.y);
		db1.getControl().focusedProperty().addListener(parent.getFocusListener(db1.getControl()));
		db1.getControl().addEventFilter(KeyEvent.KEY_PRESSED, parent.getNkeyHandler());
		lastGrayableNode = db1.getControl();
		lastShowonlyable = db1;
		lastHelptextableControl = db1.getControl();
		lastInsertorderableControl = db1;
		lastBoxtabable = db1;
		parent.encapsulatingWidgets.put(item, db1);
		((ComboBoxBase) db1.getControl()).setOnAction(parent.getDropboxSelectAction());
		return db1.getControl();
	}
	
	private Node createEditBox(final String item, int hsize, String initialText, boolean secret,
			int maxChars)
	{
		TextField sctf1 = secret ? new SCPasswordField(parent, initialText, (maxChars != -1), maxChars)
			: new SCTextField(parent, initialText, (maxChars != -1), maxChars);
		sctf1.setFont(currentFont);
		sctf1.setStyle(Resource.getTextFillStyle(currentTextColor));
		sctf1.relocate(pos.x, pos.y);
		sctf1.setPrefWidth(hsize);
		sctf1.setMinWidth(Region.USE_PREF_SIZE);
		sctf1.setId(item);
		lastGrayableNode = sctf1;
		lastHelptextableControl = sctf1;
		lastJustifyableControl = sctf1;
		if (secret) {
			lastReadonlyable = (SCPasswordField)sctf1;
			lastShowonlyable = (SCPasswordField)sctf1;
		}
		else {
			lastReadonlyable = (SCTextField)sctf1;
			lastShowonlyable = (SCTextField)sctf1;
		}
		
		// ITEM messages sent only if ITEMON
		sctf1.textProperty().addListener(new ChangeListener<String>() {
			@Override
			public void changed(ObservableValue<? extends String> observable,
					String oldValue, String newValue) {
				if (parent.isItemOn()) Client.ItemMessage(parent.getId(), item, newValue);
			}
		});
		
		// ESEL messages are always sent
		sctf1.selectedTextProperty().addListener(new ChangeListener<String>() {
			@Override
			public void changed(ObservableValue<? extends String> observable,
					String oldValue, String newValue) {
				Client.EselMessage(parent.getId(), item, newValue);
			}
		});
		sctf1.focusedProperty().addListener(parent.getFocusListener(sctf1));
		sctf1.addEventFilter(KeyEvent.KEY_PRESSED, parent.getNkeyHandler());
		return sctf1; 
	}
	
	private SCFedit createFedit(final String item, int hsize, String mask, String initialText)
	{
		SCFedit fe1 = new SCFedit(parent, initialText, mask);
		fe1.setFont(currentFont);
		fe1.setStyle(Resource.getTextFillStyle(currentTextColor));
		fe1.relocate(pos.x, pos.y);
		fe1.setPrefWidth(hsize);
		fe1.setMinWidth(Region.USE_PREF_SIZE);
		fe1.setId(item);
		lastGrayableNode = fe1;
		lastHelptextableControl = fe1;
		lastReadonlyable = fe1;
		lastShowonlyable = fe1;
		fe1.focusedProperty().addListener(parent.getFocusListener(fe1));
		fe1.addEventFilter(KeyEvent.KEY_PRESSED, parent.getNkeyHandler());
		return fe1;
	}
	
	private ImageView createIcon(final String item, String iconName)
	{
		IconResource ico1 = Client.getIcon(iconName);
		if (ico1 != null) {
			ImageView iv1 = new ImageView(ico1.getImage());
			iv1.relocate(pos.x, pos.y);
			iv1.setId(item);
			return iv1;
		}
		return new ImageView();
	}
	
	private Node createListbox(final String item, final int hsize, final int vsize, boolean multiple) throws Exception
	{
		final SCListbox lb1;
		if (!multiple) lb1 = new SCListbox(parent, item, hsize, vsize, currentFont, currentTextColor);
		else  lb1 = new SCMListbox(parent, item, hsize, vsize, currentFont, currentTextColor);
		lb1.getControl().relocate(pos.x, pos.y);
		lb1.getControl().focusedProperty().addListener(parent.getFocusListener(lb1.getControl()));
		lb1.getControl().addEventFilter(KeyEvent.KEY_PRESSED, parent.getNkeyHandler());
		lastGrayableNode = lb1.getControl();
		lastShowonlyable = lb1;
		lastHelptextableControl = lb1.getControl();
		lastInsertorderableControl = lb1;
		lastBoxtabable = lb1;
		parent.encapsulatingWidgets.put(item, lb1);
		if (!multiple) {
			lb1.getSelectionModel().selectedIndexProperty().addListener(new ChangeListener<Number>() {
				@Override
				public void changed(ObservableValue<? extends Number> arg0,
						Number oldValue, Number newValue) {
					if (parent.ignoreItemOnce) {
						parent.ignoreItemOnce = false;
						return;
					}
					if (oldValue.intValue() != newValue.intValue() && !newValue.equals(-1)) {
						if (parent.isItemOn()) {
							if (parent.isLineOn()) {
								int i1 = newValue.intValue();
								Client.ItemMessage(parent.getId(), item, String.valueOf(i1 + 1));
							}
							else {
								SelectionModel<ListDropItem> selmod = lb1.getSelectionModel();
								ListDropItem c1 = selmod.getSelectedItem();
								Client.ItemMessage(parent.getId(), item, c1.getText());
							}
						}
					}
				}
			});
		}
		return lb1.getControl();
	}
	
	private SCTextArea createMeditbox(final String item, int hsize, int vsize, String initialText, boolean limited) {
		SCTextArea scta1 = new SCTextArea(parent, initialText, limited);
		scta1.setFont(currentFont);
		scta1.setStyle(Resource.getTextFillStyle(currentTextColor));
		scta1.relocate(pos.x, pos.y);
		scta1.setPrefSize(hsize, vsize);
		scta1.setMinSize(Region.USE_PREF_SIZE, Region.USE_PREF_SIZE);
		scta1.setId(item);
		lastGrayableNode = scta1;
		lastHelptextableControl = scta1;
		lastReadonlyable = scta1;
		lastShowonlyable = scta1;
		
		// ITEM messages sent only if ITEMON
		scta1.textProperty().addListener(new ChangeListener<String>() {
			@Override
			public void changed(ObservableValue<? extends String> observable,
					String oldValue, String newValue) {
				if (parent.isItemOn()) Client.ItemMessage(parent.getId(), item, newValue);
			}
		});
		// ESEL messages are always sent
		scta1.selectedTextProperty().addListener(new ChangeListener<String>() {
			@Override
			public void changed(ObservableValue<? extends String> observable,
					String oldValue, String newValue) {
				Client.EselMessage(parent.getId(), item, newValue);
			}
		});
		scta1.focusedProperty().addListener(parent.getFocusListener(scta1));
		scta1.addEventFilter(KeyEvent.KEY_PRESSED, parent.getNkeyHandler());
		return scta1;
	}
	
	private SCProgressbar createProgressbar(final String item, int hsize, int vsize) {
		SCProgressbar pbar1 = new SCProgressbar(parent, item, hsize, vsize);
		pbar1.relocate(pos.x, pos.y);
		pbar1.addEventFilter(KeyEvent.KEY_PRESSED, parent.getNkeyHandler());
		return pbar1;
	}
	
	private Popbox createPopbox(String item, int hsize) {
		Popbox pbox1 = new Popbox(parent, item, hsize, currentTextColor);
		pbox1.setFont(currentFont);
		pbox1.relocate(pos.x, pos.y);
		pbox1.focusedProperty().addListener(parent.getFocusListener(pbox1));
		pbox1.addEventFilter(KeyEvent.KEY_PRESSED, parent.getNkeyHandler());
		pbox1.setOnAction(parent.getPushbuttonActionHandler());
		lastGrayableNode = pbox1;
		lastHelptextableControl = pbox1;
		return pbox1;
	}


	private Button createPushbutton(final String item, int hsize, int vsize, Object data) {
		final Button btn1 = new SCPushbutton(parent, data, item);
		btn1.setFont(currentFont);
		//btn1.setTextFill(currentTextColor); // DB/C pushbuttons do not support text color
		btn1.relocate(pos.x, pos.y);
		btn1.setPrefSize(hsize, vsize);
		btn1.setMinSize(Region.USE_PREF_SIZE, Region.USE_PREF_SIZE);
		btn1.setOnAction(parent.getPushbuttonActionHandler());
		btn1.focusedProperty().addListener(parent.getFocusListener(btn1));
		btn1.addEventFilter(KeyEvent.KEY_PRESSED, parent.getNkeyHandler());
		lastGrayableNode = btn1;
		lastHelptextableControl = btn1;
		lastNoFocusAble = btn1;
		return btn1;
	}

	private SCScrollBar createScrollbar(final String item, int hsize, int vsize, Orientation orient)
	{
		final SCScrollBar sb1 = new SCScrollBar(item, orient, hsize, vsize);
		sb1.relocate(pos.x, pos.y);
		sb1.setStyle(Resource.getTextFillStyle(currentTextColor));
		sb1.focusedProperty().addListener(parent.getFocusListener(sb1));
		sb1.addEventFilter(KeyEvent.KEY_PRESSED, parent.getNkeyHandler());
		lastGrayableNode = sb1;
		lastShowonlyable = sb1;
		sb1.valueProperty().addListener(new ChangeListener<Number>() {
			@Override
			public void changed(ObservableValue<? extends Number> observable,
					Number oldValue, Number newValue) {
				if (oldValue.intValue() != newValue.intValue()) {
					if(parent.ignorePositionChangeOnce) parent.ignorePositionChangeOnce = false;
					else Client.SBMessage("tm", parent.getId(), item, newValue.intValue());
				}
			}
		});
		//
		// The next thing needs to be this way. The ScrollBar consumes the event
		//
		sb1.addEventFilter(MouseEvent.MOUSE_PRESSED, new EventHandler<MouseEvent>() {
			@Override
			public void handle(MouseEvent event) {
				if (!sb1.isTracking() && !sb1.isShowonly()) {
					sb1.setTracking(true);
					Client.SBMessage("tm", parent.getId(), item, (int) sb1.getValue());
				}
			}
		});
		sb1.setOnMouseReleased(new EventHandler<MouseEvent>() {
			@Override
			public void handle(MouseEvent event) {
				if (sb1.isTracking() && !sb1.isShowonly()) {
					sb1.setTracking(false);
					Client.SBMessage("tf", parent.getId(), item, (int) sb1.getValue());
				}
			}
		});
		return sb1;
	}
	
	/**
	 * @param text The Text
	 * @param ta Justification
	 * @param width How wide, used only for centered text
	 */
	private Node createStaticText(final String text, TextAlignment ta, double width) {
		SCLabel label = new SCLabel(text, ta, width, currentFontAttributes, pos, currentTextColor);
		//label.relocate(pos.x, pos.y);
		return label;
	}

	private Node createTabgroup(int hsize, int vsize, Element element) throws Exception {
		SCTabgroup tg1 = new SCTabgroup(hsize, vsize, element, this);
		tg1.relocate(pos.x, pos.y);
		tg1.focusedProperty().addListener(parent.getFocusListener(tg1));
		tg1.addEventFilter(KeyEvent.KEY_PRESSED, parent.getNkeyHandler());
		tg1.selectionModelProperty().get().selectedItemProperty().addListener(
				new ChangeListener<Tab>()
		{
			@Override public void changed(ObservableValue<? extends Tab> observable,
					Tab oldValue, Tab newValue) {
				if (parent.isItemOn()) {
					Client.ItemMessage(parent.getId(), newValue.getId(), newValue.getText());
				}
			}
		});
		ObservableList<Tab> tabs = tg1.getTabs();
		for (int i1 = 0; i1 < element.getChildrenCount(); i1++) {
			Object o1 = element.getChild(i1);
			if (o1 instanceof Element) {
				Element e2 = (Element)o1;
				if (e2.name.equals("tab")) {
					parent.encapsulatingWidgets.put(e2.getAttributeValue("i"), tabs.get(i1));
				}
			}
		}
		return tg1;
	}
	
	/**
	 * In DB/C world, variable text controls CAN be set initially to gray
	 * @param item The DB/C Item number (as a string)
	 * @param text The Text
	 * @param ta Justification
	 * @param width How wide, used only for centered text
	 */
	private Node createVariableText(final String item, final String text, TextAlignment ta, double width) {
		SCLabel label = new SCLabel(text, ta, width, currentFontAttributes, pos, currentTextColor);
		lastGrayableNode = label;
		label.setId(item);
		//label.relocate(pos.x, pos.y);
		return label;
	}

	/**
	 * @return the currentSCFontAttributes
	 */
	SCFontAttributes getCurrentFontAttributes() {
		return currentFontAttributes;
	}

	/**
	 * @return the currentTextColor
	 */
	Color getCurrentTextColor() {
		return currentTextColor;
	}

	@SuppressWarnings("unchecked")
	public Collection<Node> createNodes(Collection<Object> children) throws Exception {
		List<Node> list1 = new ArrayList<>();
		for(Object o1 : children) {
			if (o1 instanceof Element) {
				Object n1 = createNode((Element)o1);
				if (n1 != null) {
					if (n1 instanceof Node) list1.add((Node)n1);
					else if (n1 instanceof List) list1.addAll((List<Node>)n1);
				}
			}
		}
		return list1;
	}
	
	private static void processBoxtabs(Boxtabable boxtabable, Element bte) {
		String tabsString = bte.getAttributeValue("tabs");
		String jFlagsString = bte.getAttributeValue("j");
		String[] tabs = tabsString.split(":");
		int[] tabsI = new int[tabs.length];
		int i1 = 0;
		for (String t1 : tabs) { tabsI[i1++] = Integer.parseInt(t1); }
		Pos[] jFlags = new Pos[tabs.length];
		for (i1 = 0; i1 < tabs.length; i1++) {
			char c1 = jFlagsString.charAt(i1);
			switch (c1) {
			case 'l':
				jFlags[i1] = Pos.CENTER_LEFT;
				break;
			case 'r':
				jFlags[i1] = Pos.CENTER_RIGHT;
				break;
			case 'c':
				jFlags[i1] = Pos.CENTER;
				break;
			}
		}
		boxtabable.setBoxtabs(tabsI, jFlags);
	}

	/**
	 * @return The parent DB/C Resource
	 */
	public Resource getParent() {
		return parent;
	}
}
