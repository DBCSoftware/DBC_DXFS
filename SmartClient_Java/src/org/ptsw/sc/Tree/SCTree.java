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

import org.ptsw.sc.Client;
import org.ptsw.sc.ClientUtils;
import org.ptsw.sc.ReadOnlyable;
import org.ptsw.sc.Resource;
import org.ptsw.sc.RightClickable;
import org.ptsw.sc.ShowOnlyable;
import org.ptsw.sc.Client.ControlType;

import javafx.beans.property.ObjectProperty;
import javafx.beans.property.SimpleObjectProperty;
import javafx.beans.value.ChangeListener;
import javafx.beans.value.ObservableValue;
import javafx.collections.ObservableList;
import javafx.event.EventHandler;
import javafx.scene.control.TreeCell;
import javafx.scene.control.TreeItem;
import javafx.scene.control.TreeItem.TreeModificationEvent;
import javafx.scene.control.TreeView;
import javafx.scene.image.Image;
import javafx.scene.image.ImageView;
import javafx.scene.input.MouseButton;
import javafx.scene.input.MouseEvent;
import javafx.scene.layout.Region;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;
import javafx.util.Callback;

public class SCTree<T> extends TreeView<T> implements ShowOnlyable, ReadOnlyable, RightClickable
{
	static final int INVALIDCURRENTLINE = -1;
	private Client.ControlType controlType = ControlType.NORMAL;
	private boolean reportRightClicks;
	private boolean ignoreOpenClose;
	private boolean ignoreSelection;
	private final ObjectProperty<Color> foregroundTextColor = new SimpleObjectProperty<>();
	private final Font treeFont;
	
	/**
	 * Also referred to in the db/c doc as the 'current indent level'
	 */
	private SCTreeItem<T> currentItem;
	
	public SCTree(final Resource parent, String item, int hsize, int vsize,
			Font currentFont, Color currentTextColor) {
		super(new SCTreeItem<T>());
		foregroundTextColor.set(currentTextColor);
		treeFont = currentFont;
		setId(item);
		setShowRoot(false);
		setPrefSize(hsize, vsize);
		setMinSize(Region.USE_PREF_SIZE, Region.USE_PREF_SIZE);
		rootlevel();
		getRoot().setExpanded(true);
		disabledProperty().addListener(new ChangeListener<Boolean>() {
			@Override
			public void changed(ObservableValue<? extends Boolean> observable,
					Boolean oldValue, Boolean newValue) {
				if (!newValue) controlType = ControlType.NORMAL;
			}
			
		});
		addEventFilter(MouseEvent.ANY, new EventHandler<MouseEvent>() {
			@Override
			public void handle(MouseEvent event) {
				if (controlType == ControlType.SHOWONLY) {
					event.consume();
					getScene().getWindow().fireEvent(event);
				}
				else if (reportRightClicks && Resource.isRightMouseEventReportable(event)) {
					Client.ControlButtonEvent(parent.getId(), SCTree.this, event);
					event.consume();
				}
			}
		});
		
		/*
		 * Look for OPEN and CLOS events
		 */
		getRoot().addEventHandler(TreeItem.<String>expandedItemCountChangeEvent(),
			new EventHandler<TreeItem.TreeModificationEvent<String>>(){
				@Override public void handle(TreeModificationEvent<String> event) {
					if (ignoreOpenClose) return;
					String dbcMessageCode = null;
					if (event.wasCollapsed()) dbcMessageCode = "clos";
					else if (event.wasExpanded()) dbcMessageCode = "open";
					if (dbcMessageCode != null) {
						TreeItem<String> ti1 = event.getTreeItem();
						String text =
							parent.isLineOn() ? ((SCTreeItem<String>)ti1).getIndexCSV() : ti1.getValue();
						String resId = parent.getId();
						String thisId = SCTree.this.getId();
						Client.TreeExpandedCollapsedEvent(dbcMessageCode, resId, thisId, text);
					}
				}
			}
		
		);
		
		getSelectionModel().selectedItemProperty().addListener(new ChangeListener<TreeItem<T>>() {
			@Override public void changed(ObservableValue<? extends TreeItem<T>> observable,
						TreeItem<T> oldValue, TreeItem<T> newValue)
			{
				if (!parent.isItemOn() || ignoreSelection) return;
				if (newValue != null) {
					SCTreeItem<T> ti1 = (SCTreeItem<T>) newValue;
					String text = parent.isLineOn() ? ti1.getIndexCSV() : ti1.toString();
					String resId = parent.getId();
					String thisId = SCTree.this.getId();
					Client.ItemMessage(resId, thisId, text);
				}
			}
		});
		
		setCellFactory(new Callback<TreeView<T>, TreeCell<T>>() {
			@Override public TreeCell<T> call(TreeView<T> param) {
				final TreeCell<T> cell = new TreeCell<>() {
					@Override protected void updateItem(T item, boolean empty) {
						super.updateItem(item, empty);
						if (item == null || empty) {
							setText(null);
							graphicProperty().unbind();
							textFillProperty().unbind();
							fontProperty().unbind();
							setGraphic(null);
						}
						else {
							SCTreeItem<T> treeItem = (SCTreeItem<T>) getTreeItem();
							if (treeItem != null) {
								graphicProperty().bind(treeItem.graphicProperty());
								setText((String) treeItem.getValue());
								textFillProperty().bind(treeItem.getForegroundTextColor());
								fontProperty().bind(treeItem.getFontProperty());
							}
							else {
								setText((String) item);
								graphicProperty().unbind();
								textFillProperty().unbind();
								fontProperty().unbind();
	                            setGraphic(null);
							}
						}
					}
				};
				cell.setOnMouseClicked(new EventHandler<MouseEvent>() {
					@Override public void handle(MouseEvent event) {
						if (event.getClickCount() == 2 && event.getButton() == MouseButton.PRIMARY) {
							SCTreeItem<T> treeItem = (SCTreeItem<T>) cell.getTreeItem();
							String text = parent.isLineOn() ? treeItem.getIndexCSV() : treeItem.toString();
							String resId = parent.getId();
							String thisId = SCTree.this.getId();
							Client.pickMessage(resId, thisId, text);
						}
					}
				});
				return cell;
			}
		});
	}

	@Override
	public void setReadonly() {
		controlType = ControlType.READONLY;
		setFocusTraversable(true);
	}

	@Override
	public boolean isReadonly() {
		return (controlType == ControlType.READONLY);
	}

	@Override
	public void setShowonly() {
		controlType = ControlType.SHOWONLY;
		setFocusTraversable(false);
	}

	@Override
	public boolean isShowonly() {
		return (controlType == ControlType.SHOWONLY);
	}

	@Override
	public void setRightClick(boolean state) {
		reportRightClicks = state;
	}

	public void rootlevel() {
		currentItem = (SCTreeItem<T>) getRoot();
	}

	@SuppressWarnings("unchecked")
	private SCTreeItem<T> getNewItem(String data) {
		return new SCTreeItem<>((T) data, foregroundTextColor, treeFont);
	}
	
	public void insert(String data) throws Exception {
		ObservableList<TreeItem<T>> children = currentItem.getChildren();
		SCTreeItem<T> t1 = getNewItem(data);
		int crntLinePos = currentItem.getCurrentLinePosition();
		if (crntLinePos == INVALIDCURRENTLINE) {
			children.add(t1);
			currentItem.setCurrentLinePosition(children.size() - 1);
		}
		else {
			children.add(crntLinePos + 1, t1);
			currentItem.setCurrentLinePosition(crntLinePos + 1);
		}
	}

	public void insertafter(String data) throws Exception {
		currentItem.getChildren().add(getNewItem(data));
		currentItem.setCurrentLinePosition(currentItem.getChildren().size() - 1);
	}

	public void insertbefore(String data) throws Exception {
		currentItem.getChildren().add(0, getNewItem(data));
		currentItem.setCurrentLinePosition(0);
	}

	public void setline(int i1) throws Exception {
		if (i1 < 0 || i1 >= currentItem.getChildren().size()) 
			throw new Exception(String.format("'setline' value is invalid (%d)", ++i1));
		currentItem.setCurrentLinePosition(i1);
	}

	public void select() throws Exception {
		int crntLinePos = currentItem.getCurrentLinePosition();
		if (crntLinePos != INVALIDCURRENTLINE) {
			TreeItem<T> ti1 = currentItem.getChildren().get(crntLinePos);
			ignoreSelection = true;
			getSelectionModel().select(ti1);
			ignoreSelection = false;
		}
		else throw new Exception("Bad 'select', no current line.");
	}

	public void downlevel() {
		if (currentItem.getParent() != null) currentItem = (SCTreeItem<T>) currentItem.getParent();
	}

	public void uplevel() {
		int crntLinePos = currentItem.getCurrentLinePosition();
		if (crntLinePos != INVALIDCURRENTLINE) {
			SCTreeItem<T> ti1 = (SCTreeItem<T>) currentItem.getChildren().get(crntLinePos);
			if (ti1 != null) currentItem = ti1;
		}
		// Ignore if not possible to 'uplevel', no error
	}

	/**
	 * @throws Exception Should only happen if there is an internal failure.
	 */
	public void setselectedline() throws Exception {
		TreeItem<T> selectedItem = getSelectionModel().getSelectedItem();
		if (selectedItem != null) {
			currentItem = (SCTreeItem<T>) selectedItem.getParent();
			int selectedIndex = currentItem.getChildren().indexOf(selectedItem);
			currentItem.setCurrentLinePosition(selectedIndex);
		}
	}

	public void erase() throws Exception {
		rootlevel();
		currentItem.getChildren().clear();
		currentItem.setCurrentLinePosition(INVALIDCURRENTLINE);
	}

	public void delete() throws Exception {
		int crntLinePos = currentItem.getCurrentLinePosition();
		if (crntLinePos != INVALIDCURRENTLINE) {
			TreeItem<T> selectedItem = getSelectionModel().getSelectedItem();
			TreeItem<T> itemToBeDeleted = currentItem.getChildren().get(crntLinePos);
			currentItem.getChildren().remove(itemToBeDeleted);
			currentItem.setCurrentLinePosition(INVALIDCURRENTLINE);
			if (selectedItem != null && selectedItem.equals(itemToBeDeleted)) {
				if (Client.isDebug()) System.out.printf("Item '%s' was the selected item", itemToBeDeleted);
				getSelectionModel().selectPrevious();
			}
		}
		else throw new Exception("Bad 'delete', no current line.");
	}

	public String getStatus() {
		TreeItem<T> selectedItem = getSelectionModel().getSelectedItem();
		return (String) ((selectedItem == null) ? "" : selectedItem.getValue());
	}

	/**
	 * @param index Zero-based index of the child item of the current item to be expanded.
	 * @throws Exception 
	 */
	public void expand(int index) throws Exception {
		if (index < 0 || index >= currentItem.getChildren().size()) 
			throw new Exception(String.format("'expand' value is invalid (%d)", ++index));
		TreeItem<T> item = currentItem.getChildren().get(index);
		ignoreOpenClose = true;
		item.setExpanded(true);
		ignoreOpenClose = false;
	}

	/**
	 * @param index Zero-based index of the child item of the current item to be collapsed.
	 * @throws Exception 
	 */
	public void collapse(int index) throws Exception {
		if (index < 0 || index >= currentItem.getChildren().size()) 
			throw new Exception(String.format("'collapse' value is invalid (%d)", ++index));
		TreeItem<T> item = currentItem.getChildren().get(index);
		ignoreOpenClose = true;
		item.setExpanded(false);
		ignoreOpenClose = false;
	}

	public void icon(Image image) throws Exception {
		int crntLinePos = currentItem.getCurrentLinePosition();
		if (crntLinePos != INVALIDCURRENTLINE) {
			TreeItem<T> ti1 = currentItem.getChildren().get(crntLinePos);
			ti1.setGraphic(new ImageView(image));
		}
		else throw new Exception("Bad 'icon', no current line.");
	}

	public void deselect() {
		int crntLinePos = currentItem.getCurrentLinePosition();
		if (crntLinePos != INVALIDCURRENTLINE) {
			TreeItem<T> ti1 = currentItem.getChildren().get(crntLinePos);
			if (getSelectionModel().getSelectedItem().equals(ti1)) {
				getSelectionModel().clearSelection();
			}
		}
	}

	public void setColor(Color newColor) {
		foregroundTextColor.set(newColor!= null ? newColor : Color.BLACK);
	}

	public void textcolordata(Color newColor, String text) {
		SCTreeItem<T> item = findItem(text);
		if (item != null) {
			if (Client.isDebug()) System.out.println("Found it!");
			item.setColor(newColor);
		}
	}
	
	private SCTreeItem<T> findItem(String text) {
		TreeItem<T> rootItem = getRoot();
		return searchChildren(rootItem, text);
	}

	private SCTreeItem<T> searchChildren(TreeItem<T> item, String text) {
		if (item.isLeaf()) {
			return (SCTreeItem<T>) (item.toString().equals(text) ? item : null);
		}
		for (TreeItem<T> item1 : item.getChildren()) {
			if (item1.toString().equals(text)) return (SCTreeItem<T>) item1;
			if (item1.isLeaf()) continue;
			SCTreeItem<T> item2 = searchChildren(item1, text);
			if (item2 != null) return item2;
		}
		return null;
	}

	public void textcolorline(Color newColor, int index) throws Exception {
		if (index < 0 || index >= currentItem.getChildren().size()) 
			throw new Exception(String.format("'textcolorline' value is invalid (%d)", ++index));
		SCTreeItem<T> item = (SCTreeItem<T>) currentItem.getChildren().get(index);
		item.setColor(newColor);
	}

	public void textstyledata(String newStyle, String text) throws Exception {
		SCTreeItem<T> item = findItem(text);
		if (item != null) {
			Font newFont = ClientUtils.applyDBCStyle(item.getFont(), newStyle, getId());
			item.setFont(newFont);
		}
	}

	public void textstyleline(String newStyle, int index) throws Exception {
		if (index < 0 || index >= currentItem.getChildren().size()) 
			throw new Exception(String.format("'textstyleline' value is invalid (%d)", ++index));
		SCTreeItem<T> item = (SCTreeItem<T>) currentItem.getChildren().get(index);
		Font newFont = ClientUtils.applyDBCStyle(item.getFont(), newStyle, getId());
		item.setFont(newFont);
	}
}
