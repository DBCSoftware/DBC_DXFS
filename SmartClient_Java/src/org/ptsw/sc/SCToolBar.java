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

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

import org.ptsw.sc.ListDrop.SCDropbox;
import org.ptsw.sc.xml.Element;

import javafx.collections.ObservableList;
import javafx.event.ActionEvent;
import javafx.event.EventHandler;
import javafx.scene.Node;
import javafx.scene.control.Button;
import javafx.scene.control.ButtonBase;
import javafx.scene.control.ComboBox;
import javafx.scene.control.ContentDisplay;
import javafx.scene.control.Control;
import javafx.scene.control.Separator;
import javafx.scene.control.ToggleButton;
import javafx.scene.control.ToolBar;
import javafx.scene.control.Tooltip;
import javafx.scene.image.ImageView;
import javafx.scene.paint.Color;

public class SCToolBar extends Resource {

	enum InsertMode {
		ATEND,
		AFTERITEM,
		ATBEGINNING,
		BEFOREITEM
	}
	
	private Node insertBeforeAfterItem;
	
	private  EventHandler<ActionEvent> actionHandler = new EventHandler<> () {
		@Override public void handle(ActionEvent event) {
			if (event.getSource() instanceof ToggleButton) {
				if (isItemOn()) {
					ToggleButton btn = (ToggleButton) event.getSource();
					Client.ItemMessage(getId(), btn.getId(), btn.isSelected() ? "Y" : "N");
				}
			}
			else if (event.getSource() instanceof Button) {
				Button btn = (Button) event.getSource();
				Client.ButtonPushed(getId(), btn.getId());
			}
		}
	};
	
	private Node lastGrayable;
	private InsertMode insertMode = InsertMode.ATEND;
	private final Map<String, SCDropbox> dboxMap = new HashMap<>();

	protected SCToolBar(final Element element) throws Exception {
		super(element);
		assert element.hasAttribute(Client.TOOLBAR);
		content = new ToolBar();
		setId(element.getAttributeValue(Client.TOOLBAR));
		prepException = null;
		
		/**
		 * Default state for db/c toolbars is ITEMON
		 */
		itemOn = true;

		SmartClientApplication.Invoke(new Runnable() {
			@Override
			public void run() {
				try {
					int childCount;
					if (( childCount = element.getChildrenCount() ) > 0) {
						for (int i1 = 0; i1 < childCount; i1++) {
							Element e1 = (Element) element.getChild(i1);
							switch (e1.name) {
							case "dropbox":
								((ToolBar)content).getItems().add(makeDropbox(e1));
								break;
							case "textpushbutton":
								((ToolBar)content).getItems().add(makeTextpushbutton(e1));
								break;
							case "pushbutton":
								((ToolBar)content).getItems().add(makePushbutton(e1));
								break;
							case "lockbutton":
								((ToolBar)content).getItems().add(makeLockbutton(e1));
								break;
							case GRAY:
								if (lastGrayable != null) lastGrayable.setDisable(true);
								break;
							case "space":
								((ToolBar)content).getItems().add(new Separator());
								break;
							}
						}
					}
				} catch (Exception e) {
					prepException = e;
					element.getChildren().clear();;
				}
			}
		}, true);
		if (prepException != null) throw prepException;
	}

	private Control makeTextpushbutton(Element e1) {
		return makeTextpushbutton(e1.getAttributeValue("i"), e1.getAttributeValue("t"));
	}
	
	private Control makeTextpushbutton(String item, String text) {
		ButtonBase btn1 = new Button(text);
		btn1.setContentDisplay(ContentDisplay.TEXT_ONLY);
		btn1.setId(item);
		btn1.setOnAction(actionHandler);
		lastGrayable = btn1;
		return btn1;
	}
	
	@Override
	public void Close() {
		Hide();
	}

	@Override
	public void Hide() {
		if (visibleOnWindow != null) visibleOnWindow.HideResource(this);
	}

	private Control makeDropbox(Element e1) throws Exception {
		return makeDropbox(e1.getAttributeValue("i"), Integer.parseInt(e1.getAttributeValue("h1")),
				Integer.parseInt(e1.getAttributeValue("v1")));
	}
	
	private Control makeDropbox(String item, int hsize, int vsize) throws Exception {
		SCDropbox dbox = new SCDropbox(this, item, hsize, null, Color.BLACK);
		dboxMap.put(item, dbox);	
		dbox.setInsertOrder();		// Always insert order when on a toolbar
		return dbox.getControl();
	}

	private ButtonBase makePushbutton(Element e1) 
	{
		return makePushbutton(e1.getAttributeValue("i"), e1.getAttributeValue("t"),
				e1.getAttributeValue(Client.ICON));
	}
	
	private ButtonBase makePushbutton(String item, String helpText, String iconName) {
		ButtonBase btn1 = new Button();
		btn1.setContentDisplay(ContentDisplay.GRAPHIC_ONLY);
		btn1.setId(item);
		if (helpText != null && helpText.length() > 0)
			btn1.setTooltip(new Tooltip(helpText));
		IconResource iRes = Client.getIcon(iconName);
		if (iRes != null) btn1.setGraphic(new ImageView(iRes.getImage()));
		lastGrayable = btn1;
		btn1.setOnAction(actionHandler);
		return btn1;
	}
	
	private ButtonBase makeLockbutton(Element e1)
	{
		return makeLockbutton(e1.getAttributeValue("i"), e1.getAttributeValue("t"),
				e1.getAttributeValue(Client.ICON));
	}
	
	private ButtonBase makeLockbutton(String item, String helpText, String iconName) {
		ButtonBase btn1 = new ToggleButton();
		btn1.setContentDisplay(ContentDisplay.GRAPHIC_ONLY);
		btn1.setId(item);
		if (helpText != null && helpText.length() > 0)
			btn1.setTooltip(new Tooltip(helpText));
		IconResource iRes = Client.getIcon(iconName);
		if (iRes != null) btn1.setGraphic(new ImageView(iRes.getImage()));
		lastGrayable = btn1;
		btn1.setOnAction(actionHandler);
		return btn1;
	}
	
	/**
	 * Need to override Resource.Change for a number of things that
	 * need special treatment by ToolBars.
	 * <p>
	 * @see org.ptsw.sc.Resource#Change(org.ptsw.sc.xml.Element)
	 */
	@Override
	public void Change(Element element) throws Exception {
		if (Client.IsTrace()) System.out.println("Entering SCToolBar.Change");
		final String data = element.getAttributeValue("l");
		final Collection<Object> echildren = element.getChildren();
		changeException = null;
		SmartClientApplication.Invoke(new Runnable() {
			@Override public void run() {
				String itemName;
				Node n2;
				SCDropbox dbox ;
				breakforloop:
				for (Object obj1 : echildren) {
					if (obj1 instanceof Element) {
						final Element e1 = (Element) obj1;
						e1.name.intern();
						switch (e1.name) {
						case "adddropbox": // <change toolbar="main0004" l="0014000140"><adddropbox n="400"/>
							String sh1 = data.substring(0, 5);
							String sv1 = data.substring(5);
							try {
								int hsize = Integer.parseInt(sh1);
								int vsize = Integer.parseInt(sv1);
								insert(makeDropbox(e1.getAttributeValue("n"), hsize, vsize));
							} catch (Exception e) {
								changeException = e;
								break breakforloop;
							}
							echildren.remove(obj1);
							break;
						case "addpushbutton": // <change toolbar="main0004" l="icon1"><addpushbutton n="400"/>
							insert(makePushbutton(e1.getAttributeValue("n"), null, data));
							echildren.remove(obj1);
							break;
						case "addlockbutton":
							insert(makeLockbutton(e1.getAttributeValue("n"), null, data));
							echildren.remove(obj1);
							break;
						case "addspace":
							insert(new Separator());
							echildren.remove(obj1);
							break;
						case "addtextpushbutton":
							insert(makeTextpushbutton(e1.getAttributeValue("n"), data));
							echildren.remove(obj1);
							break;
							
						/**
						 * A delete command might be to remove a button
						 * or it might be a delete sent to a dropbox
						 */
						case "delete":
							itemName = e1.getAttributeValue("n");
							n2 = ((ToolBar)content).lookup("#" + itemName);
							if (n2 != null) {
								if (n2 instanceof ButtonBase)
									((ToolBar)content).getItems().remove(n2);
								else if (n2 instanceof ComboBox<?>) {
									dbox = dboxMap.get(itemName);
									if (dbox != null) dbox.delete(data);
								}
							}
							else {
								changeException = new Exception(
										String.format("change/delete to unknown item (%s)", itemName));
								break breakforloop;
							}
							echildren.remove(obj1);
							break;
						case "deleteline":
							dbox = dboxMap.get(e1.getAttributeValue("n"));
							if (dbox != null) {
								int i1 = Integer.parseInt(data);
								dbox.deleteline(i1);
							}
							echildren.remove(obj1);
							break;
											
						case "erase":
							dbox = dboxMap.get(e1.getAttributeValue("n"));
							if (dbox != null) dbox.erase();
							echildren.remove(obj1);
							break;
							
						case "helptext":
						case "text":
							itemName = e1.getAttributeValue("n");
							n2 = ((ToolBar)content).lookup("#" + itemName);
							if (n2 != null && n2 instanceof ButtonBase) {
								((Control)n2).setTooltip(new Tooltip(data));
							}
							else {
								changeException = new Exception(
										String.format("change/%s to unknown/invalid item (%s)", e1.name, itemName));
								break breakforloop;
							}
							echildren.remove(obj1);
							break;
							
						case Client.ICON:
							itemName = e1.getAttributeValue("n");
							n2 = ((ToolBar)content).lookup("#" + itemName);
							if (n2 != null && n2 instanceof ButtonBase) {
								if (((ButtonBase)n2).getContentDisplay().equals(ContentDisplay.GRAPHIC_ONLY))
								{
									IconResource iRes = Client.getIcon(data);
									if (iRes != null) {
										((ButtonBase)n2).setGraphic(new ImageView(iRes.getImage()));
									}
								}
							}
							else {
								changeException = new Exception(
										String.format("change/%s to unknown/invalid item (%s)", Client.ICON, itemName));
								break breakforloop;
							}
							echildren.remove(obj1);
							break;
							
						case INSERT: // <change toolbar="main0004" l="Some Text!"><insert n="600"/>
							dbox = dboxMap.get(e1.getAttributeValue("n"));
							if (dbox != null) {
								try {
									dbox.insert(data);
								} catch (Exception e) {
									changeException = e;
									break breakforloop;
								}
							}
							echildren.remove(obj1);
							break;
							
						/**
						 * It is not possible to send an INSERTAFTER or an INSERTBEFORE to
						 * a dropbox on a toolbar. These changes are used at the Toolbar
						 * level to control the insertion of new controls
						 */
						case INSERTAFTER:
							itemName = e1.getAttributeValue("n");
							if (itemName == null) {
								insertMode = InsertMode.ATEND;
							}
							else {
								n2 = ((ToolBar)content).lookup("#" + itemName);
								if (n2 != null) {
									insertBeforeAfterItem = n2;
									insertMode = InsertMode.AFTERITEM;
								}
							}
							echildren.remove(obj1);
							break;
						case INSERTBEFORE:
							itemName = e1.getAttributeValue("n");
							if (itemName == null) {
								insertMode = InsertMode.ATBEGINNING;
							}
							else {
								n2 = ((ToolBar)content).lookup("#" + itemName);
								if (n2 != null) {
									insertBeforeAfterItem = n2;
									insertMode = InsertMode.BEFOREITEM;
								}
							}
							echildren.remove(obj1);
							break;
							
							
						case INSERTLINEAFTER:
							dbox = dboxMap.get(e1.getAttributeValue("n"));
							if (dbox != null) {
								try {
									dbox.insertlineafter(data);
								} catch (Exception e) {
									changeException = e;
									break breakforloop;
								}
							}
							echildren.remove(obj1);
							break;
						case INSERTLINEBEFORE:
							dbox = dboxMap.get(e1.getAttributeValue("n"));
							if (dbox != null) {
								try {
									dbox.insertlinebefore(data);
								} catch (Exception e) {
									changeException = e;
									break breakforloop;
								}
							}
							echildren.remove(obj1);
							break;
							
						case LOCATE:
							dbox = dboxMap.get(e1.getAttributeValue("n"));
							if (dbox != null) {
								try {
									dbox.locate(data);
								} catch (Exception e) {
									changeException = e;
									break breakforloop;
								}
							}
							echildren.remove(obj1);
							break;
						case LOCATELINE:
							dbox = dboxMap.get(e1.getAttributeValue("n"));
							if (dbox != null) {
								try {
									int i1 = Integer.parseInt(data);
									dbox.locateLine(i1);
								} catch (Exception e) {
									changeException = e;
									break breakforloop;
								}
							}
							echildren.remove(obj1);
							break;
							
						case MARK:
						case "unmark":
							itemName = e1.getAttributeValue("n");
							n2 = ((ToolBar)content).lookup("#" + itemName);
							if (n2 != null && n2 instanceof ToggleButton) {
								((ToggleButton)n2).setSelected(e1.name.equals(MARK));
							}
							echildren.remove(obj1);
							break;
							
						case "minsert":
							dbox = dboxMap.get(e1.getAttributeValue("n"));
							if (dbox != null) {
								ArrayList<String> datums = processMinsertData(data);
								try {
									dbox.minsert(datums);
								} catch (Exception e) {
									changeException = e;
									break breakforloop;
								}
							}
							echildren.remove(obj1);
							break;
							
						case "removecontrol":
							itemName = e1.getAttributeValue("n");
							n2 = ((ToolBar)content).lookup("#" + itemName);
							if (n2 != null) {
								((ToolBar)content).getItems().remove(n2);
							}
							echildren.remove(obj1);
							break;
						case "removespaceafter":
							itemName = e1.getAttributeValue("n");
							n2 = ((ToolBar)content).lookup("#" + itemName);
							if (n2 != null) {
								ObservableList<Node> list = ((ToolBar)content).getItems();
								int index = list.indexOf(n2);
								if (index == list.size() - 1
										|| !(list.get(index + 1) instanceof Separator)) {
									changeException = new Exception("Bad change/removespaceafter");
									break breakforloop;
								}
								else list.remove(index + 1);
							}
							else {
								changeException = new Exception(
										String.format("change/removespaceafter unknown item (%s)", itemName));
								break breakforloop;
							}
							echildren.remove(obj1);
							break;
						case "removespacebefore":
							itemName = e1.getAttributeValue("n");
							n2 = ((ToolBar)content).lookup("#" + itemName);
							if (n2 != null) {
								ObservableList<Node> list = ((ToolBar)content).getItems();
								int index = list.indexOf(n2);
								if (index < 1 || !(list.get(index - 1) instanceof Separator)) {
									changeException = new Exception("Bad change/removespacebefore");
									break breakforloop;
								}
								else list.remove(index - 1);
							}
							else {
								changeException = new Exception(
										String.format("change/removespacebefore unknown item (%s)", itemName));
								break breakforloop;
							}
							echildren.remove(obj1);
							break;
							
						case TEXTCOLOR:
							Color newColor = getColorExt(data.toUpperCase());
							dbox = dboxMap.get(e1.getAttributeValue("n"));
							if (dbox != null) {
								try {
									dbox.textcolor(newColor);
								} catch (Exception e) {
									changeException = e;
									break breakforloop;
								}
							}
							else {
								itemName = e1.getAttributeValue("n");
								n2 = ((ToolBar)content).lookup("#" + itemName);
								if (n2 != null && n2 instanceof Button) {
									if (((Button)n2).getContentDisplay() == ContentDisplay.TEXT_ONLY) {
										((Button)n2).setTextFill(newColor);
									}
								}								
							}
							echildren.remove(obj1);
							break;
							
						case "textcolordata":
							dbox = dboxMap.get(e1.getAttributeValue("n"));
							if (dbox != null) {
								int i1 = data.indexOf(' ');
								newColor = getColorExt(data.substring(0, i1).toUpperCase());
								String text = data.substring(i1 + 1, data.length());
								dbox.textcolordata(newColor, text);
							}
							echildren.remove(obj1);
							break;
							
						case "textcolorline":
							dbox = dboxMap.get(e1.getAttributeValue("n"));
							if (dbox != null) {
								int i1 = data.indexOf(' ');
								newColor = getColorExt(data.substring(0, i1).toUpperCase());
								int index = Integer.parseInt(data.substring(i1 + 1, data.length()));
								dbox.textcolorline(newColor, index);
							}
							echildren.remove(obj1);
							break;
							
						case "textstyledata":
							dbox = dboxMap.get(e1.getAttributeValue("n"));
							if (dbox != null) {
								int i1 = data.indexOf(' ');
								String text = data.substring(i1 + 1, data.length());
								String newStyle = data.substring(0, i1).toUpperCase();
								try {
									dbox.textstyledata(newStyle, text);
								} catch (Exception e) {
									changeException = e;
									break breakforloop;
								}
							}
							echildren.remove(obj1);
							break;
							
						case "textstyleline":
							dbox = dboxMap.get(e1.getAttributeValue("n"));
							if (dbox != null) {
								int i1 = data.indexOf(' ');
								int index = Integer.parseInt(data.substring(i1 + 1, data.length()));
								String newStyle = data.substring(0, i1).toUpperCase();
								try {
									dbox.textstyleline(newStyle, index);
								} catch (Exception e) {
									changeException = e;
									break breakforloop;
								}
							}
							echildren.remove(obj1);
							break;							
						}
					}
				}
			}
		}, true);
		if (changeException != null) throw changeException;
		super.Change(element);
	}
	
	private void insert(Node node) {
		ObservableList<Node> list = ((ToolBar)content).getItems(); 
		int index;
		switch (insertMode) {
		case ATEND:
			list.add(node);
			break;
		case AFTERITEM:
			index = list.indexOf(insertBeforeAfterItem);
			if (index >= 0) list.add(index + 1, node);
			break;
		case ATBEGINNING:
			list.add(0, node);
			break;
		case BEFOREITEM:
			index = list.indexOf(insertBeforeAfterItem);
			if (index >= 0) list.add(index, node);
			break;
		default:
			break;
		}
	}
}
