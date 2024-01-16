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

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

import org.ptsw.sc.ListDrop.MultipleSelectionBox;
import org.ptsw.sc.ListDrop.SingleSelectionBox;
import org.ptsw.sc.Menus.SCMenuBar;
import org.ptsw.sc.Popbox.Popbox;
import org.ptsw.sc.SpecialDialogs.SCDirChooser;
import org.ptsw.sc.SpecialDialogs.SCFileChooser;
import org.ptsw.sc.Tabgroup.SCTab;
import org.ptsw.sc.Table.CellUtilities;
import org.ptsw.sc.Table.Column;
import org.ptsw.sc.Table.ColumnCDropBox;
import org.ptsw.sc.Table.ColumnDropBox;
import org.ptsw.sc.Table.Table;
import org.ptsw.sc.Tree.SCTree;
import org.ptsw.sc.xml.Attribute;
import org.ptsw.sc.xml.Element;

import javafx.beans.value.ChangeListener;
import javafx.beans.value.ObservableValue;
import javafx.collections.ObservableList;
import javafx.event.ActionEvent;
import javafx.event.EventHandler;
import javafx.geometry.Pos;
import javafx.scene.Node;
import javafx.scene.Parent;
import javafx.scene.control.Button;
import javafx.scene.control.CheckBox;
import javafx.scene.control.CheckMenuItem;
import javafx.scene.control.ComboBox;
import javafx.scene.control.ContextMenu;
import javafx.scene.control.Control;
import javafx.scene.control.Labeled;
import javafx.scene.control.MenuBar;
import javafx.scene.control.MenuItem;
import javafx.scene.control.RadioButton;
import javafx.scene.control.ScrollBar;
import javafx.scene.control.SelectionModel;
import javafx.scene.control.Tab;
import javafx.scene.control.TextField;
import javafx.scene.control.TextInputControl;
import javafx.scene.control.Toggle;
import javafx.scene.control.ToggleButton;
import javafx.scene.control.ToolBar;
import javafx.scene.control.Tooltip;
import javafx.scene.image.ImageView;
import javafx.scene.input.KeyCode;
import javafx.scene.input.KeyEvent;
import javafx.scene.input.MouseButton;
import javafx.scene.input.MouseEvent;
import javafx.scene.layout.Pane;
import javafx.scene.paint.Color;

public abstract class Resource {

	public static final String AVAILABLE = "available";
	public static final String AVAILABLEALL = "availableall";
	public static final String GRAY = "gray";
	public static final String GRAYALL = "grayall";
	public static final String HIDE = "hide";
	public static final String INSERT = "insert";
	public static final String INSERTAFTER = "insertafter";
	public static final String INSERTBEFORE = "insertbefore";
	public static final String INSERTLINEAFTER = "insertlineafter";
	public static final String INSERTLINEBEFORE = "insertlinebefore";
	public static final String LOCATE = "locate";
	public static final String LOCATELINE = "locateline";
	public static final String MARK = "mark";
	public static final String SHOW = "show";
	public static final String TEXTCOLOR = "textcolor";

	/**
	 * This is used for the TEXTCOLOR and TEXTBGCOLOR functions.
	 * I added to the DB/C list; darkGray, gray, lightGray since these were
	 * aleady static objects of the Color class.
	 */
	private static final Map<String, Color> colorTable = new HashMap<>(17);
	static {
		colorTable.put("BLACK", Color.BLACK);
		colorTable.put("BLUE", Color.BLUE);
		colorTable.put("CYAN", Color.CYAN);
		colorTable.put("DARKGRAY", Color.DARKGRAY);
		colorTable.put("GRAY", Color.GRAY);
		colorTable.put("GREEN", Color.GREEN);
		colorTable.put("LIGHTGRAY", Color.LIGHTGREY);
		colorTable.put("MAGENTA", Color.MAGENTA);
		colorTable.put("RED", Color.RED);
		colorTable.put("WHITE", Color.WHITE);
		colorTable.put("YELLOW", Color.YELLOW);
	}
	
	public class QueryChangeTarget {
		/**
		 * Zero based row number for changes/queries to tables
		 */
		private int row = -1;
		public Object target;
		/**
		 * @return Zero-based row number
		 */
		public int getRow() { return row; 	}
		public void setRow(int row) { this.row = row; }
	}

	protected int horzSize = -1;
	protected int vertSize = -1;
	protected boolean itemOn;
	protected boolean lineOn;
	protected WindowDevice visibleOnWindow;
	private boolean focusOn;
	public final HashMap<String, Object> encapsulatingWidgets = new HashMap<>();
	@SuppressWarnings("rawtypes")
	public final HashMap<String, Column> tableColumns = new HashMap<>();
	protected Object content;
	protected Exception prepException = null;
	protected Exception changeException = null;
	protected Exception queryException = null;
	protected String title;
	protected String specialDialogResourceName;

	public boolean ignorePositionChangeOnce;
	public boolean ignoreItemOnce;


	protected Resource (final Element element) throws Exception {
		assert element != null;
		if (Client.IsTrace()) System.out.println("Entering Resource<>");
		for(Attribute a1 : element.getAttributes()) {
			switch (a1.name) {
			case "hsize":
				horzSize = Integer.parseInt(a1.value.trim());
				break;
			case "vsize":
				vertSize = Integer.parseInt(a1.value.trim());
				break;
			case "title":
				title = a1.value;
				break;
			}
		}
		if (this instanceof Panel || this instanceof Dialog) {
			content = new Pane();
			final PanelDialogControlFactory cf = new PanelDialogControlFactory(this);
			SmartClientApplication.Invoke(new Runnable() {
				@Override
				public void run() {
					try {
						((Pane)content).getChildren().setAll(cf.createNodes(element.getChildren()));
					} catch (Exception e) {
						prepException = e;
						element.getChildren().clear();;
					}
					if (prepException != null) {
						if (Client.isDebug()) {
							System.err.printf("\tprepException.message=%s", prepException.getMessage());
							prepException.printStackTrace();
						}
					}
				}
			}, true);
		}
		if (prepException != null) {
			throw prepException;
		}
		if (Client.IsTrace()) System.out.println("Leaving Resource<>");
	}
	
	public abstract void Close();

	/**
	 * Parses a string into a javafx.scene.paint.Color object
	 * 
	 * @param str A DX color name or a numeric string in RGB (dec, oct, or hex)
	 * @return a Color object or null if the string cannot be parsed as a color
	 */
	public static Color getColorExt(String str) {
		Integer int1;
		Color c1 = colorTable.get(str);
		if (c1 != null) return c1;
		try {
			int1 = Integer.decode(str.trim());
		} catch (NumberFormatException e) {
			// Do nothing
			return null;
		}
		int rgb = int1.intValue();
		int iBlue, iGreen, iRed;
		if (Client.gui24BitColorIsRGB) {
			iRed = (rgb & 0xFF0000) >> 16;
			iGreen = (rgb & 0x00FF00) >> 8;
			iBlue = rgb & 0x0000FF;
		}
		else {
			iBlue = (rgb & 0xFF0000) >> 16;
			iGreen = (rgb & 0x00FF00) >> 8;
			iRed = rgb & 0x0000FF;
		}
		double dRed = iRed / 255.0;
		double dGreen = iGreen / 255.0;
		double dBlue = iBlue / 255.0;
		return Color.color(dRed, dGreen, dBlue);
	}

	//public abstract void change(String func, String obj) throws Exception;
	
	public ChangeListener<Boolean> getFocusListener(final Node n1) {
		return new ChangeListener<>() {
			@Override
			public void changed(ObservableValue<? extends Boolean> observable,
					Boolean oldValue, Boolean newValue) {
				if (isFocusOn()) Client.FocXMessage(((Node)content).getId(), n1.getId(), newValue);
			}
		};
	}
	
	protected void addFocusListener(final Node n1) {
		n1.focusedProperty().addListener(getFocusListener(n1));
	}
	
	private EventHandler<ActionEvent> checkboxActionHandler;
	
	public EventHandler<ActionEvent> getCheckboxActionHandler() {
		if (checkboxActionHandler == null) {
			checkboxActionHandler = new EventHandler<>() {
				@Override
				public void handle(ActionEvent event) {
					if (itemOn) {
						CheckBox bx2 = (CheckBox)event.getSource();
						Client.ItemMessage(((Node)content).getId(), bx2.getId(), bx2.isSelected() ? "Y" : "N");
					}
				}
			};
		}
		return checkboxActionHandler;
	}
	
	private EventHandler<ActionEvent> pushbuttonActionHandler;
	
	public EventHandler<ActionEvent> getPushbuttonActionHandler() {
		if (pushbuttonActionHandler == null) {
			pushbuttonActionHandler = new EventHandler<>() {
				@Override
				public void handle(ActionEvent event) {
					Node n1 = (Node)event.getSource();
					Client.ButtonPushed(getId(), n1.getId());
				}
			};
		}
		return pushbuttonActionHandler;
	};
	

	
	private ChangeListener<Toggle> toggleListener;
	
	public ChangeListener<Toggle> getToggleListener() {
		if (toggleListener == null) {
			toggleListener= new ChangeListener<>() {
				@Override
				public void changed(ObservableValue<? extends Toggle> observable,
						Toggle oldValue, Toggle newValue) {
					if (itemOn && newValue != null) Client.ItemMessage(getId(), ((Control)newValue).getId(), "Y");
				}
			};
		}
		return toggleListener;
	};
	

	/**
	 * NKEY messages from a control do not check for any additional info like shift or cntl keys.
	 * We consume it here so the Window does not see it and also send a message.
	 * That would be contrary to the DB/C ref.
	 * 
	 * ? This Event Handler should be a Filter and not a Listener
	 */
	private EventHandler<KeyEvent> nkeyHandler;
	
	/**
	 * This is added as a Filter to Panel controls
	 * @return a KeyEvent EventHandler for DX 'NKEY' messages
	 */
	public EventHandler<KeyEvent> getNkeyHandler() {
		if (nkeyHandler == null) {
			nkeyHandler = new EventHandler<>() {
				@Override
				public void handle(KeyEvent event) {
					// Before sending an NKEY from a control, must see if it
					// is a menu accelerator. If so then only the MENU message
					// should be sent.
					if (visibleOnWindow != null && visibleOnWindow instanceof SCWindow) {
						SCMenuBar mb1 = Client.getMenuForWindow(visibleOnWindow);
						if (mb1 != null && mb1.isAccelerator(event)) {
							return; // Do not process it in any way
						}
					}
					// NKEY messages are always sent
					// Do not check ITEM flag
					KeyCode kc1 = event.getCode();
					Node source = (Node)event.getSource();
					if (kc1.isFunctionKey() || kc1.equals(KeyCode.ESCAPE)) {
						if (ClientUtils.FKeyMap.containsKey(kc1)) {
							Client.NkeyMessage(getId(), source.getId(), ClientUtils.FKeyMap.get(kc1).toString());
							event.consume();
						}
					}
				}
			};
		}
		return nkeyHandler;
	}
	
	
	/**
	 * Used to send ITEM messages for DB/C dropboxes
	 */
	private EventHandler<ActionEvent> dropboxSelectAction;
	
	public EventHandler<ActionEvent> getDropboxSelectAction(){
		if (dropboxSelectAction == null) {
			dropboxSelectAction= new EventHandler<>() {
				@SuppressWarnings("rawtypes")
				@Override
				public void handle(ActionEvent event) {
					if (ignoreItemOnce) {
						ignoreItemOnce = false;
						return;
					}
					ComboBox box = (ComboBox)event.getSource();
					if (box == null) {
						if (Client.isDebug()) System.err.println("In Panel.singleSelectAction, box is NULL!");
						return;
					}
					SelectionModel selmod = box.getSelectionModel();
					String item = box.getId();
					if (itemOn) {
						if (lineOn) {
							int i1 = selmod.getSelectedIndex();
							Client.ItemMessage(getId(), item, String.valueOf(i1 + 1));
						}
						else {
							Object value = box.getValue();
							Client.ItemMessage(getId(), item, value.toString());
						}
					}
				}
			};
		}
		return dropboxSelectAction;
	}
	
	public void Change(final Element element) throws Exception {
		Objects.requireNonNull(element, "element cannot be null");
		changeException = null;
		if (Client.IsTrace()) System.out.println("Entering Resource.Change");
		SmartClientApplication.Invoke(new Runnable() {
			
			boolean state;
			Color newColor;
			int i1;
			final String data = element.getAttributeValue("l");
			
			@SuppressWarnings({ "rawtypes" })
			@Override
			public void run() {
				forloop:
				for(Object o1 : element.getChildren()) {
					if (o1 instanceof Element) {
						Element e1 = (Element)o1;
						QueryChangeTarget qct = null;
						try {
							qct = Resource.this.findTarget(e1);
						}
						catch (Exception exc1) {
							changeException = exc1;
							break forloop;
						}
						Object target = qct.target;
						if (Client.isDebug()) System.out.printf("In Resource.Change, e1.name=%s\n", e1.name);
						switch (e1.name) {
						case "addrow":
							if (target instanceof Table) {
								try {
									((Table)target).addRow();
								}
								catch (Exception ex1) { changeException = ex1; break forloop;}
							}
							break;
						case AVAILABLE:
						case GRAY:
							if (target != null) {
								if (target instanceof Node) {
									setGrayAvailableState((Node) target, e1.name.equals(AVAILABLE));
								}
								else if (target instanceof MenuItem) {
									((MenuItem) target).setDisable(!e1.name.equals(AVAILABLE));
								}
								else if (target instanceof ControlWrapper) {
									Node nd1 = ((ControlWrapper) target).getControl();
									setGrayAvailableState(nd1, e1.name.equals(AVAILABLE));
								}
							}
							break;
						case AVAILABLEALL:
						case GRAYALL:
							state = e1.name.equals(AVAILABLEALL);
							if (content instanceof Parent) {
								ObservableList<Node> list;
								if (content instanceof ToolBar)
									list = ((ToolBar)content).getItems();
								else list = ((Parent)content).getChildrenUnmodifiable();
								for (Node n1 : list) setGrayAvailableState(n1, state); 
							}
							break;
							
						case "collapse":
							try {
								if (target instanceof SCTree<?>) {
									int i1 = Integer.parseInt(data.trim());
									((SCTree<?>)target).collapse(--i1);
								}
							}
							catch (Exception ex1) { changeException = ex1; break forloop;}
							break;
							
						case "defpushbutton":
							if (target instanceof Button) {
								((Button)target).setDefaultButton(true);
							}
							break;
							
						case "delete":
							try {
								if (target instanceof SingleSelectionBox) {
									((SingleSelectionBox)target).delete(data);
								}
								else if (target instanceof ColumnCDropBox) {
									((ColumnCDropBox)target).delete(data);
								}
								else if (target instanceof ColumnDropBox) {
									((ColumnDropBox)target).delete(data, qct.getRow());
								}
								else if (target instanceof SCTree<?>) {
									((SCTree<?>)target).delete();
								}
							}
							catch (Exception ex1) { changeException = ex1; break forloop;}
							break;
							
						case "deleteallrows":
							if (target instanceof Table) {
								((Table)target).deleteAllRows();
							}
							break;
							
						case "deleteline":
							i1 = Integer.parseInt(data.trim());
							if (target instanceof SingleSelectionBox) {
								((SingleSelectionBox)target).deleteline(i1 - 1);
							}
							else if (target instanceof ColumnCDropBox) {
								((ColumnCDropBox)target).deleteline(i1 - 1);
							}
							else if (target instanceof ColumnDropBox) {
								((ColumnDropBox)target).deleteline(i1 - 1, qct.getRow());
							}
							break;
							
						case "deleterow":
							if (target instanceof Table) {
								((Table)target).deleteRow(qct.getRow());
							}
							break;
							
						case "deselect":
							if (target instanceof MultipleSelectionBox) {
								((MultipleSelectionBox)target).deselect(data);
							}
							else if (target instanceof SCTree<?>) {
								((SCTree<?>)target).deselect();
							}
							break;
							
						case "deselectall":
							if (target instanceof MultipleSelectionBox) {
								((MultipleSelectionBox)target).deselectAll();
							}
							break;
							
						case "deselectline":
							if (target instanceof MultipleSelectionBox) {
								i1 = Integer.parseInt(data.trim());
								((MultipleSelectionBox)target).deselectLine(i1 - 1);
							}
							break;
							
						case "dirname":
							if (target instanceof SCDirChooser) {
								((SCDirChooser)target).dirname(data);
							}
							break;
							
						case "downlevel":
							if (target instanceof SCTree) {
								((SCTree)target).downlevel();
							}
							break;
							
						case "erase":
							try {
								ignoreItemOnce = true;
								if (target instanceof SCLabel) ((SCLabel)target).changeText("");
								else if (target instanceof TextInputControl) {
									((TextInputControl)target).clear();
								}
								else if (target instanceof SingleSelectionBox) {
									((SingleSelectionBox)target).erase();
								}
								else if (target instanceof Table) {
									((Table)target).deleteAllRows();
								}
								else if (target instanceof ColumnCDropBox) {
									// Can only be entire column
									((ColumnCDropBox)target).erase();
								}
								else if (target instanceof ColumnDropBox) {
									((ColumnDropBox)target).erase(qct.getRow());
								}
								else if (target instanceof SCTree<?>) {
									((SCTree<?>)target).erase();
								}
							}
							catch (Exception ex1) { changeException = ex1; break forloop;}
							break;
							
						case "escpushbutton":
							if (target instanceof Button) {
								((Button)target).setCancelButton(true);
							}
							break;

						case "expand":
							try {
								if (target instanceof SCTree<?>) {
									int i1 = Integer.parseInt(data.trim());
									((SCTree<?>)target).expand(--i1);
								}
							}
							catch (Exception ex1) { changeException = ex1; break forloop;}
							break;
							
						case "feditmask":
							if (target instanceof SCFedit) ((SCFedit)target).changeMask(data);
							break;
							
						case "filename":
							if (target instanceof SCFileChooser) {
								((SCFileChooser)target).filename(data);
							}
							break;
							
						case "focus":
							if (target instanceof Node) ((Node) target).requestFocus();
							else if (target instanceof ControlWrapper) {
								Node nd1 = ((ControlWrapper) target).getControl();
								nd1.requestFocus();
							}
							else if (target instanceof SCTab) {
								((SCTab)target).requestFocus();
							}
							break;
							
						case "focuson":
							setFocusOn(true);
							break;
						case "focusoff":
							setFocusOn(false);
							break;
							
						case "helptext":
							if (target instanceof ControlWrapper) target = ((ControlWrapper) target).getControl();
							if (target instanceof Control) {
								Control ctrl = (Control)target;
								Tooltip tp1 = ctrl.getTooltip();
								if (tp1 == null) ctrl.setTooltip(new Tooltip(data));
								else tp1.setText(data);
							}
							break;
							
						case Client.ICON:
							IconResource ico1 = Client.getIcon(data);
							if (ico1 != null) {
								try {
									if (target instanceof ImageView) {
										((ImageView)target).setImage(ico1.getImage());
									}
									else if (target instanceof SCPushbutton) {
										((SCPushbutton)target).setImage(ico1.getImage());
									}
									else if (target instanceof SCTree<?>) {
										((SCTree<?>)target).icon(ico1.getImage());
									}
								}
								catch (Exception ex1) { changeException = ex1; break forloop;}
							}
							break;
						
						case INSERT:
							try {
								if (target instanceof SingleSelectionBox) {
									((SingleSelectionBox)target).insert(data);
								}
								else if (target instanceof ColumnCDropBox) {
									((ColumnCDropBox)target).insert(data);
								}
								else if (target instanceof ColumnDropBox) {
									((ColumnDropBox)target).insert(data, qct.getRow());
								}
								else if (target instanceof SCTree<?>) {
									((SCTree<?>)target).insert(data);
								}
							}
							catch (Exception ex1) { changeException = ex1; break forloop;}
							break;

						case INSERTAFTER:
							try {
								if (target instanceof SingleSelectionBox) {
									((SingleSelectionBox)target).insertafter(data);
								}
								else if (target instanceof ColumnCDropBox) {
									((ColumnCDropBox)target).insertafter(data);
								}
								else if (target instanceof ColumnDropBox) {
									((ColumnDropBox)target).insertafter(data, qct.getRow());
								}
								else if (target instanceof SCTree<?>) {
									((SCTree<?>)target).insertafter(data);
								}
							}
							catch (Exception ex1) { changeException = ex1; break forloop;}
							break;
							
						case INSERTBEFORE:
							try {
								if (target instanceof SingleSelectionBox) {
									((SingleSelectionBox)target).insertbefore(data);
								}
								else if (target instanceof ColumnCDropBox) {
									((ColumnCDropBox)target).insertbefore(data);
								}
								else if (target instanceof ColumnDropBox) {
									((ColumnDropBox)target).insertbefore(data, qct.getRow());
								}
								else if (target instanceof SCTree<?>) {
									((SCTree<?>)target).insertbefore(data);
								}
							}
							catch (Exception ex1) { changeException = ex1;  break forloop;}
							break;
							
						case INSERTLINEAFTER:
							try {
								if (target instanceof SingleSelectionBox) {
									((SingleSelectionBox)target).insertlineafter(data);
								}
								else if (target instanceof ColumnCDropBox) {
									((ColumnCDropBox)target).insertlineafter(data);
								}
								else if (target instanceof ColumnDropBox) {
									((ColumnDropBox)target).insertlineafter(data, qct.getRow());
								}
							}
							catch (Exception ex1) { changeException = ex1;  break forloop;}
							break;
							
						case INSERTLINEBEFORE:
							try {
								if (target instanceof SingleSelectionBox) {
									((SingleSelectionBox)target).insertlinebefore(data);
								}
								else if (target instanceof ColumnCDropBox) {
									((ColumnCDropBox)target).insertlinebefore(data);
								}
								else if (target instanceof ColumnDropBox) {
									((ColumnDropBox)target).insertlinebefore(data, qct.getRow());
								}
							}
							catch (Exception ex1) { changeException = ex1;  break forloop;}
							break;
							
						case "insertrowbefore":
							try {
								if (target instanceof Table) {
									((Table)target).insertRowBefore(qct.getRow());
								}
							}
							catch (Exception ex1) { changeException = ex1;  break forloop;}
							break;
							
						case "itemon":
							itemOn = true;
							break;
						case "itemoff":
							itemOn = false;
							break;
							
						// The change/justify commands are only supported on a single line edit box
						case "justifyleft":
							if (target instanceof TextField) ((TextField)target).setAlignment(Pos.CENTER_LEFT);
							break;
						case "justifycenter":
							if (target instanceof TextField) ((TextField)target).setAlignment(Pos.CENTER);
							break;
						case "justifyright":
							if (target instanceof TextField) ((TextField)target).setAlignment(Pos.CENTER_RIGHT);
							break;

						case "lineon":
							lineOn = true;
							break;
						case "lineoff":
							lineOn = false;
							break;
							
						case "locatetab":
							if (target instanceof SCTab) {
								((SCTab)target).requestFocus();
							}
							break;
							
						case MARK:
						case "unmark":
							state = (e1.name.equals(MARK));
							if (target instanceof Toggle) {
								boolean save = itemOn;
								itemOn = false;
								((Toggle)target).setSelected(state);
								itemOn = save;
							}
							else if (target instanceof CheckBox) {
								((CheckBox)target).setSelected(state);
							}
							else if (target instanceof Column) {
								((Column)target).setCheckboxState(state, qct.getRow());
							}
							else if (target instanceof CheckMenuItem) {
								((CheckMenuItem)target).setSelected(state);
							}
							break;
							
						case LOCATE:
							ignoreItemOnce = true;
							if (target instanceof SingleSelectionBox) {
								((SingleSelectionBox)target).locate(data);
							}
							else if (target instanceof ColumnCDropBox) {
								((ColumnCDropBox)target).locate(data, qct);
							}
							else if (target instanceof ColumnDropBox) {
								((ColumnDropBox)target).locate(data, qct.getRow());
							}
							break;
							
						case LOCATELINE:
							ignoreItemOnce = true;
							i1 = Integer.parseInt(data.trim());
							if (target instanceof SingleSelectionBox) {
								((SingleSelectionBox)target).locateLine(i1 - 1);
							}
							else if (target instanceof ColumnCDropBox) {
								((ColumnCDropBox)target).locateLine(i1 - 1, qct);
							}
							else if (target instanceof ColumnDropBox) {
								((ColumnDropBox)target).locateLine(i1 - 1, qct.getRow());
							}
							break;
							
						case "minsert":
							ArrayList<String> datums = processMinsertData(data);
							try {
								if (target instanceof SingleSelectionBox) {
										((SingleSelectionBox)target).minsert(datums);
								}
								else if (target instanceof ColumnCDropBox<?, ?>) {
									((ColumnCDropBox<?, ?>)target).minsert(datums);
								}
								else if (target instanceof ColumnDropBox<?, ?>) {
									((ColumnDropBox<?, ?>)target).minsert(datums, qct.getRow());
								}
							}
							catch (Exception ex1) { changeException = ex1; break forloop;}
							break;
							
						case "multiple":
							try {
								multiChange(data);
							} catch (Exception e) {
								e.printStackTrace();
							}
							break;
						
						case "namefilter":
							if (target instanceof SCFileChooser) {
								((SCFileChooser)target).namefilter(data);
							}
							break;
							
						case "paste":
							if (target instanceof SCTextField) {
								((SCTextField)target).doPaste(data);
							}
							break;
							
						case "position":
							if (target instanceof ScrollBar) {
								double value = Double.parseDouble(data.trim());
								ignorePositionChangeOnce = true;
								((ScrollBar) target).setValue(value);
							}
							else if (target instanceof SCProgressbar) {
								int value = Integer.parseInt(data.trim());
								((SCProgressbar) target).setValue(value);
							}
							break;
						
						case "range":
							int low = Integer.parseInt(data.substring(0, 5).trim());
							int high = Integer.parseInt(data.substring(5, 10).trim());
							if (target instanceof ScrollBar) {
								int page = Integer.parseInt(data.substring(10, 15).trim());
								((ScrollBar) target).setMin(low);
								((ScrollBar) target).setMax(high + 1);
								((ScrollBar) target).setBlockIncrement(page);
								((ScrollBar) target).setVisibleAmount(1);
							}
							else if (target instanceof SCProgressbar) {
								((SCProgressbar) target).setRange(low, high);
							}
							break;
						case "readonly":
							if (target instanceof ReadOnlyable) {
								((ReadOnlyable)target).setReadonly();
							}
							break;
							
						case "rightclickon":
						case "rightclickoff":
							boolean state = e1.name.equals("rightclickon");
							for (Node n1 : ((Parent)content).getChildrenUnmodifiable()) {
								if (n1 instanceof RightClickable) {
									((RightClickable)n1).setRightClick(state);
								}
							}
							break;

						case "rootlevel":
							if (target instanceof SCTree) {
								((SCTree)target).rootlevel();
							}
							break;
							
						case "select":
							try {
								if (target instanceof MultipleSelectionBox) {
									((MultipleSelectionBox)target).select(data);
								}
								else if (target instanceof SCTree) {
									((SCTree)target).select();
								}
							}
							catch (Exception ex1) { changeException = ex1;  break forloop;}
							break;
							
						case "selectall":
							if (target instanceof MultipleSelectionBox) {
								((MultipleSelectionBox)target).selectAll();
							}
							break;
							
						case "selectline":
							if (target instanceof MultipleSelectionBox) {
								i1 = Integer.parseInt(data.trim());
								((MultipleSelectionBox)target).selectLine(i1 - 1);
							}
							break;
							
						case "setline":
							try {
								if (target instanceof SCTree) {
									int i1 = Integer.parseInt(data.trim());
									((SCTree)target).setline(--i1);
								}
							}
							catch (Exception ex1) { changeException = ex1;  break forloop;}
							break;
							
						case "setmaxchars":
							if (target instanceof SCTextField) {
								((SCTextField)target).setMaxlength(Integer.parseInt(data.trim()));
							}
							else if (target instanceof SCTextArea) {
								((SCTextArea)target).setMaxlength(Integer.parseInt(data.trim()));
							}
							break;
							
						case "setselectedline":
							try {
								if (target instanceof SCTree) {
									((SCTree)target).setselectedline();
								}
							}
							catch (Exception ex1) { changeException = ex1; break forloop;}
							break;
							
						case "showonly":
							if (target instanceof ShowOnlyable) {
								((ShowOnlyable)target).setShowonly();
							}
							break;
							
						case "text":
							// Labeled covers pushbuttons, checkboxes, radiobuttons
							// SCLabel is for our custom static and variable text controls
							// TextInputControl covers all edit/medit box types
							if (target instanceof SCLabel) ((SCLabel)target).changeText(data);
							else if (target instanceof SCFedit) ((SCFedit)target).changeText(data);
							else if (target instanceof Labeled) {
								((Labeled)target).setText(data);
							}
							else if (target instanceof TextInputControl) {
								((TextInputControl)target).setText(data);
							}
							else if (target instanceof Popbox) ((Popbox)target).setText(data);
							else if (target instanceof Tab) ((Tab)target).setText(data);
							else if (target instanceof Column) ((Column)target).setText(data, qct.row);
							else if (target instanceof MenuItem) ((MenuItem)target).setText(data);
							break;
							
						case "textbgcolor":
							newColor = getColorExt(data.trim().toUpperCase());
							if (target instanceof Popbox)
								((Popbox)target).textbgcolor(newColor);
							break;
						
						case "textbgcolordata":
							i1 = data.indexOf(' ');
							newColor = getColorExt(data.substring(0, i1).toUpperCase());
							String text = data.substring(i1 + 1, data.length());
							if (target instanceof SingleSelectionBox) {
								((SingleSelectionBox)target).textbgcolordata(newColor, text);
							}
							break;
							
						case "textbgcolorline":
							i1 = data.indexOf(' ');
							newColor = getColorExt(data.substring(0, i1).toUpperCase());
							int index = Integer.parseInt(data.substring(i1 + 1, data.length()).trim());
							if (target instanceof SingleSelectionBox) {
								((SingleSelectionBox)target).textbgcolorline(newColor, index - 1);
							}
							break;
							
						case TEXTCOLOR:
							newColor = getColorExt(data.toUpperCase());
							if (target instanceof Labeled) {
								((Labeled)target).setTextFill(newColor);
							}
							else if (target instanceof TextInputControl) {
								((TextInputControl)target).setStyle(getTextFillStyle(newColor));
							}
							else if (target instanceof SingleSelectionBox) {
								((SingleSelectionBox)target).textcolor(newColor);
							}
							else if (target instanceof Popbox) {
								((Popbox)target).setColor(newColor);
							}
							else if (target instanceof Table || target instanceof Column) {
								CellUtilities.setTextColor(qct, newColor);
							}
							else if (target instanceof SCTree<?>) {
								((SCTree<?>)target).setColor(newColor);
							}
							break;
							
						case "textcolordata":
							i1 = data.indexOf(' ');
							newColor = getColorExt(data.substring(0, i1).toUpperCase());
							text = data.substring(i1 + 1, data.length());
							if (target instanceof SingleSelectionBox) {
								((SingleSelectionBox)target).textcolordata(newColor, text);
							}
							else if (target instanceof SCTree<?>) {
								((SCTree<?>)target).textcolordata(newColor, text);
							}
							break;
							
						case "textcolorline":
							i1 = data.indexOf(' ');
							newColor = getColorExt(data.substring(0, i1).toUpperCase());
							index = Integer.parseInt(data.substring(i1 + 1, data.length()).trim());
							try {
								if (target instanceof SingleSelectionBox) {
									((SingleSelectionBox)target).textcolorline(newColor, index - 1);
								}
								else if (target instanceof SCTree<?>) {
									((SCTree<?>)target).textcolorline(newColor, index - 1);
								}
							}
							catch (Exception ex1) { changeException = ex1;  break forloop;}
							break;
							
						case "textfont":
							// textfont only supported for variable text controls
							if (target instanceof SCLabel) ((SCLabel)target).changeFont(data);
							break;
							
						case "textstyledata":
							i1 = data.indexOf(' ');
							text = data.substring(i1 + 1, data.length());
							String newStyle = data.substring(0, i1).toUpperCase();
							try {
								if (target instanceof SingleSelectionBox) {
									((SingleSelectionBox)target).textstyledata(newStyle, text);
								}
								else if (target instanceof SCTree<?>) {
									((SCTree<?>)target).textstyledata(newStyle, text);
								}
							}
							catch (Exception ex1) { changeException = ex1;  break forloop;}
							break;
							
						case "textstyleline":
							i1 = data.indexOf(' ');
							index = Integer.parseInt(data.substring(i1 + 1, data.length()).trim());
							newStyle = data.substring(0, i1).toUpperCase();
							try {
								if (target instanceof SingleSelectionBox) {
									((SingleSelectionBox)target).textstyleline(newStyle, index - 1);
								}
								else if (target instanceof SCTree<?>) {
									((SCTree<?>)target).textstyleline(newStyle, index - 1);
								}
							}
							catch (Exception ex1) { changeException = ex1;  break forloop;}
							break;
							
						case "title":
							if (Resource.this instanceof Dialog) {
								((Dialog)(Resource.this)).changeTitle(data);
							}
							break;
							
						case "uplevel":
							if (target instanceof SCTree) {
								((SCTree)target).uplevel();
							}
							break;
						}
					}
				}
			}
		}, true);
		if (changeException != null) throw changeException;
	}
	
	protected void multiChange(String value) throws Exception {
		String func, data;

		/*
		 * Break up the value into chgreqs
		 */
		StringBuilder sb1 = new StringBuilder();
		int state = 0; // Looking for a left brace
		for (int i1 = 0; i1 < value.length(); i1++) {
			char c1 = value.charAt(i1);
			switch (state) {
			case 0:
				if (c1 == '{') {
					state = 1;
					sb1.setLength(0);
				}
				break;
			case 1:
				if (c1 != '}') sb1.append(c1);
				else {
					int i2 = sb1.indexOf(";");
					if (i2 == -1) {
						func = sb1.toString();
						data = null;
					} else {
						func = sb1.substring(0, i2);
						data = sb1.substring(i2 + 1);
					}
					if (func.indexOf(',') != -1)
						handleMultiCommandChange(func, data);
					else {
						this.change(func, data);
					}
					state = 0;
				}
				break;
			}
		}
	}

	/**
	 * 
	 Examples;
	 * 
	 * lineon,focuson
	 * 
	 * focus100,lineon
	 * 
	 * focus,100,lineon
	 * 
	 * focus100,200,lineon
	 * 
	 * focus 100, 200, lineon
	 * 
	 * focus,100,200,lineon
	 * 
	 * focus, 100, 100, lineon
	 * 
	 * 
	 * 
	 * When scanning letters; (state 1) If letter, accumulate, repeat state 1 If
	 * whitespace, goto state 5 if a digit is seen then save command, start new
	 * item, goto state 3 if a comma is seen then save command goto state 2
	 * Anything else is error
	 * 
	 * comma seen after a command; (state 2) If whitespace, repeat state 2 If
	 * next char is a digit, start saving a new item number, goto state 3 If
	 * letter, execute last, start new command, goto state 1
	 * 
	 * 
	 * when scanning digits; (state 3) note that the only valid ending
	 * conditions are comma and EOL. If comma is seen then goto state 4
	 * 
	 * 
	 * comma seen after an item number; (state 4) If whitespace, repeat state 4.
	 * If letter, execute last, clear item, start new command, goto state 1 If
	 * number, execute last, start new item, goto state 3
	 * 
	 * whitespace seen during command scan; (state 5) If whitespace, repeat
	 * state 5 if a digit is seen then save command, goto state 3 if a comma is
	 * seen then save command, goto state 2 Anything else is error
	 * 
	 * @param func
	 * @param data
	 * @throws BaseException
	 */
	protected void handleMultiCommandChange(String func, String data) throws Exception {
		int state = 0, i1;
		char c1;
		int n1 = func.length();
		StringBuilder sbfunc = new StringBuilder();
		StringBuilder sbitem = new StringBuilder();
		String lastFunc = null;

		for (i1 = 0; i1 < n1; i1++) {
			c1 = func.charAt(i1);
			switch (state) {
			case 0: // Just starting to scan
				if (Character.isLetter(c1)) {
					state = 1;
					sbfunc.setLength(0);
					sbfunc.append(c1);
				} else if (!Character.isWhitespace(c1))
					throw new Exception("1st char of multi-function change must be a letter or whitespace.");
				break;
			case 1: // Scanning letters
				if (Character.isLetter(c1))
					sbfunc.append(c1);
				else if (Character.isWhitespace(c1))
					state = 5;
				else {
					lastFunc = sbfunc.toString();
					if (c1 == ',')
						state = 2;
					else if (Character.isDigit(c1)) {
						state = 3;
						sbitem.setLength(0);
						sbitem.append(c1);
					} else
						throw new Exception("Bad syntax in multi-function change");
				}
				break;
			case 2: // We are looking at 1st char after a comma, after a command
				if (Character.isWhitespace(c1))
					break;
				if (Character.isLetter(c1)) {
					if (lastFunc != null)
						change(lastFunc + sbitem.toString(), data);
					sbfunc.setLength(0);
					sbfunc.append(c1);
					state = 1;
				} else if (Character.isDigit(c1)) {
					sbitem.setLength(0);
					sbitem.append(c1);
					state = 3;
				}
				break;
			case 3: // Scanning digits
				if (Character.isDigit(c1))
					sbitem.append(c1);
				else if (c1 == ',')
					state = 4;
				else
					throw new Exception("Bad syntax in multi-function change");
				break;
			case 4: // We are looking at 1st char after a comma after digits
				if (Character.isWhitespace(c1))
					break;
				if (Character.isLetter(c1)) {
					if (lastFunc != null)
						change(lastFunc + sbitem.toString(), data);
					sbfunc.setLength(0);
					sbfunc.append(c1);
					sbitem.setLength(0);
					state = 1;
				} else if (Character.isDigit(c1)) {
					if (lastFunc != null)
						change(lastFunc + sbitem.toString(), data);
					sbitem.setLength(0);
					sbitem.append(c1);
					state = 3;
				} else
					throw new Exception("Bad syntax in multi-function change");
				break;
			case 5:
				if (Character.isWhitespace(c1))
					break;
				if (Character.isDigit(c1)) {
					lastFunc = sbfunc.toString();
					sbitem.setLength(0);
					sbitem.append(c1);
					state = 3;
				} else if (c1 == ',') {
					lastFunc = sbfunc.toString();
					state = 2;
				} else
					throw new Exception("Bad syntax in multi-function change");
				break;
			}
		}
		if (state == 1 || state == 5) {
			lastFunc = sbfunc.toString();
			change(lastFunc + sbitem.toString(), data);
		} else if (state == 3) {
			change(lastFunc + sbitem.toString(), data);
		} else if (state == 2 || state == 4) {
			throw new Exception("Bad syntax in multi-function change");
		}
	}

	/**
	 * This only comes into play when 'change/multiple' is used.
	 * Then we have to parse raw change strings.
	 * The guts of that code is in my superclass.
	 */
	public void change(String func, String data) throws Exception {
		if (Client.isDebug()) System.out.printf("Change, func = '%s', data='%s'\n", func, data);
		// Strategy, parse 'func' and 'data' into an Element and feed it back to ourselves.
		StringBuilder cmd1 = new StringBuilder(32);
		StringBuilder itm1 = new StringBuilder(4);
		for(int i1 = 0; i1 < func.length(); i1++) {
			char c1 = func.charAt(i1);
			if (Character.isDigit(c1)) itm1.append(c1);
			else cmd1.append(c1);
		}
		Element chg = new Element("chg"); //name is irrelevant, it is ignored in this use
		if (data != null) chg.addAttribute(new Attribute("l", data));
		Element child = new Element(cmd1.toString());
		if (itm1.length() > 0) child.addAttribute(new Attribute("n", itm1.toString()));
		chg.addElement(child);
		if (Client.isDebug()) System.out.printf("Change, Element = '%s'\n", chg.toStringRaw());
		Change(chg);
	}
	
	/**
	 * Breaks up an minsert list into a Vector of Strings
	 * @param list
	 * @return
	 */
	static ArrayList<String> processMinsertData(final String list)
	{
		ArrayList<String> v1 = new ArrayList<>();
		StringBuilder sb1 = new StringBuilder();
		for (int i1 = 0; i1 < list.length(); i1++) {
			char c1 = list.charAt(i1);
			if (c1 == Client.minsertSeparator) {
				v1.add(sb1.toString());
				sb1.setLength(0);
			}
			else {
				if (c1 == '\\' && (i1 + 1) <= list.length()) sb1.append(list.charAt(++i1));
				else sb1.append(c1);
			}
		}
		if (sb1.length() > 0) v1.add(sb1.toString());
		return v1;
	}

	/**
	 * Check for right mouse; pressed, released, double-clicked
	 * @param event The Mouse Event
	 * @return Whether it qualifies as reportable
	 */
	public static boolean isRightMouseEventReportable(MouseEvent event)
	{
		if (event.getButton() == MouseButton.SECONDARY) {
			if (event.getEventType() == MouseEvent.MOUSE_PRESSED
					|| event.getEventType() == MouseEvent.MOUSE_RELEASED
					|| (event.getEventType() == MouseEvent.MOUSE_CLICKED && event.getClickCount() == 2))
				return true;
		}
		return false;
	}

	public boolean isLineOn() {
		return lineOn;
	}

	/**
	 * @return the itemOn
	 */
	public boolean isItemOn() {
		return itemOn;
	}
	
	
	/**
	 * Locate the target of a change or query by its db/c 'item' number
	 * @param e1 The element that may contain an 'n' attribute pointing to the widget
	 * @return target If attribute 'n' exists and Object is found, if attribute 'n' does not exist then null.
	 * @throws Exception if attribute 'n' exists and target not found
	 */
	protected QueryChangeTarget findTarget(Element e1) throws Exception {
		QueryChangeTarget qct = new QueryChangeTarget();
		if (e1.hasAttribute("n")) {
			String item = e1.getAttributeValue("n");
			int i1 = item.indexOf('[');
			if (i1 != -1) {
				int i2 = item.indexOf(']');
				String rowstring = item.substring(i1 + 1, i2);
				qct.setRow((Integer.parseInt(rowstring.trim())) - 1);
				item = item.substring(0, i1);
			}
			if (encapsulatingWidgets.containsKey(item)) {
				qct.target = encapsulatingWidgets.get(item);
			}
			else if (tableColumns.containsKey(item)) {
				qct.target = tableColumns.get(item);
			}
			else {
				qct.target = ((Node)content).lookup("#" + item);
			}
			if (qct.target == null) {
				String m1 = String.format("Change/Query directed at unknown item (%s)", item);
				throw new Exception(m1);
			}
		}
		return qct;
	}

//	public Font getCurrentFont()
//	{
//		return currentFont;
//	}

	/**
	 * @return the focusOn
	 */
	public boolean isFocusOn() {
		return focusOn;
	}

	/**
	 * @param focusOn the focusOn to set
	 */
	public void setFocusOn(boolean focusOn) {
		this.focusOn = focusOn;
	}
	
	public String getId() {
		assert content instanceof Node || content instanceof ContextMenu;
		if (content instanceof Node) return ((Node)content).getId();
		if (content instanceof ContextMenu) return ((ContextMenu)content).getId();
		return "?????";
	}
	
	public void setId(String id) {
		assert content instanceof Node || content instanceof ContextMenu :
			content.getClass().getName();
		if (content instanceof Node) ((Node)content).setId(id);
		else if (content instanceof ContextMenu) ((ContextMenu)content).setId(id);
	}
	
	public Object getContent() {
		if (Client.isDebug()) {
			System.out.printf("Entering Resource.getContent, content is a %s\n", content.getClass().getName());
			System.out.flush();
			if (content instanceof Pane) {
				for (Node node : ((Pane)content).getChildrenUnmodifiable()) {
					System.out.printf("\tnode is a %s\n", node.getClass().getName());
				}
			}
		}
		if (content instanceof Pane) {
			for (Node node : ((Pane)content).getChildrenUnmodifiable()) {
				if (node instanceof Popbox) {
					((Popbox)node).renderPart();
				}
			}
		}
		return content;
	}
	
	public ObservableList<Node> getChildren() {
		if (this instanceof Panel || this instanceof Dialog) return ((Pane)content).getChildren();
		else if (this instanceof SCMenuBar) ((MenuBar)content).getMenus();
		return null;
	}

	public static String getTextFillStyle(Color color) {
		StringBuilder sb1 = new StringBuilder(24);
		sb1.append("-fx-text-fill: ");
		String s1 = color.toString().substring(2, 8);
		sb1.append('#').append(s1).append(';');
		return sb1.toString();
	}
	
	public static String getBackgroundColorStyle(Color color) {
		StringBuilder sb1 = new StringBuilder(24);
		sb1.append("-fx-background-color: ");
		String s1 = color.toString().substring(2, 8);
		sb1.append('#').append(s1).append(';');
		return sb1.toString();
	}
	
	public void SetVisibleOn(WindowDevice sCWindow) {
		if (sCWindow != null && content instanceof Parent) {
			if (!(content instanceof ToolBar)) {
				SmartClientApplication.Invoke(new Runnable() {
					@Override
					public void run() {
						for (Node n1 : ((Parent)content).getChildrenUnmodifiable()) {
							if (n1.isFocusTraversable()) {
								n1.requestFocus();
								break;
							}
						}
					}
				}, false);
			}
		}
		visibleOnWindow = sCWindow;
	}

	WindowDevice GetVisibleOn()
	{
		return visibleOnWindow;
	}

	public abstract void Hide();
	
	/**
	 * @param target The Node to change
	 * @param state The state, true == Available
	 */
	protected static void setGrayAvailableState(Node target, boolean state) {
		target.setDisable(!state);
	}
	
	protected String Query(final Element element) throws Exception {
		final StringBuilder sb1 = new StringBuilder();
		final int ftype = Integer.parseInt(element.getAttributeValue("f").trim());
		queryException = null;
		SmartClientApplication.Invoke(new Runnable() {
			@SuppressWarnings("rawtypes")
			@Override
			public void run() {
				QueryChangeTarget qct = null;
				try {
					qct = findTarget(element);
				}
				catch (Exception exc1) {
					queryException = exc1;
					return;
				}
				if (qct.target instanceof Button) {
					sb1.append('N');	// Always 'not selected'
					sb1.append(((Node)qct.target).isDisable() ? 'G' : 'A');
				}
				if (qct.target instanceof ToggleButton) {
					sb1.append( ((ToggleButton)qct.target).isSelected() ? 'Y' : 'N');
					sb1.append(((Node)qct.target).isDisable() ? 'G' : 'A');
				}
				else if (qct.target instanceof RadioButton) {
					sb1.append( ((ToggleButton)qct.target).isSelected() ? 'Y' : 'N');
					sb1.append(((Node)qct.target).isDisable() ? 'G' : 'A');
				}
				else if (qct.target instanceof CheckBox) {
					sb1.append( ((CheckBox)qct.target).isSelected() ? 'Y' : 'N');
					sb1.append(((Node)qct.target).isDisable() ? 'G' : 'A');
				}
				else if (qct.target instanceof CheckMenuItem) {
					sb1.append( ((CheckMenuItem)qct.target).isSelected() ? 'Y' : 'N');
					sb1.append(((CheckMenuItem)qct.target).isDisable() ? 'G' : 'A');
				}
				else if (qct.target instanceof Labeled) sb1.append(((Labeled)qct.target).getText());
				else if (qct.target instanceof TextInputControl) sb1.append(((TextInputControl)qct.target).getText());
				else if (qct.target instanceof ScrollBar) sb1.append(((ScrollBar)qct.target).valueProperty().intValue());
				else if (qct.target instanceof SCProgressbar) {
					SCProgressbar pb1 = (SCProgressbar) qct.target;
					sb1.append(pb1.getIntValue());
				}
				else if (qct.target instanceof SingleSelectionBox) {
					sb1.append(((SingleSelectionBox)qct.target).getStatus());
				}
				else if (qct.target instanceof Table) {
					if (ftype == 2) {
						int i1 = ((Table)qct.target).getRowCount();
						sb1.append(String.format("%5d", i1));
					}
					else
						// I think this is trapped at the server and we would never see it
						queryException = new Exception(String.format("Bad Query target, Table(%s)",
							((Control)qct.target).getId()));
				}
				else if (qct.target instanceof Column) {
					if (qct.getRow() < 0) 
						// I think this is trapped at the server and we would never see it
						queryException = new Exception(String.format("Bad Query target, Column(%s)",
							((Control)qct.target).getId()));
					else {
						sb1.append(((Column)qct.target).getStatus(qct.getRow()));
					}
				}
				else if (qct.target instanceof SCTree<?>) {
					sb1.append(((SCTree<?>)qct.target).getStatus());
				}
			}
		}, true);
		if (queryException != null) throw queryException;
		return sb1.toString();
	}

	public void tableColumnClicked(String id) {
		Client.ButtonPushed(getId(), id);
	}
	
	@SuppressWarnings("unchecked")
	public void specialDialogReturned(Object object) {
		if (object == null) {
			Client.CancelMessage(getId());
		}
		else if (object instanceof File){
			Client.OkMessage(getId(), ((File)object));
			if (this instanceof SCFileChooser) {
				File d2;
				try {
					d2 = new File(((File)object).getCanonicalFile().getParent());
					((SCFileChooser)this).fileChooser.setInitialDirectory(d2);
				} catch (IOException e) {
					/* Do Nothing */
				}
			}
		}
		else if (object instanceof List){
			Client.OkMessage(getId(), (List<File>) object);
		}
	}

}
