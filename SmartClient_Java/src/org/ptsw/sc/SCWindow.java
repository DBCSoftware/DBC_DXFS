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
import java.awt.Insets;
import java.util.Objects;

import org.ptsw.sc.Menus.SCContextMenu;
import org.ptsw.sc.Menus.SCMenu;
import org.ptsw.sc.Menus.SCMenuBar;
import org.ptsw.sc.xml.Attribute;
import org.ptsw.sc.xml.Element;

import javafx.beans.property.ReadOnlyProperty;
import javafx.beans.value.ChangeListener;
import javafx.beans.value.ObservableValue;
import javafx.collections.ObservableList;
import javafx.concurrent.Task;
import javafx.event.EventHandler;
import javafx.geometry.Bounds;
import javafx.geometry.Point2D;
import javafx.scene.Node;
import javafx.scene.Scene;
import javafx.scene.control.ContextMenu;
import javafx.scene.control.MenuBar;
import javafx.scene.control.ScrollPane;
import javafx.scene.control.ScrollPane.ScrollBarPolicy;
import javafx.scene.control.TextField;
import javafx.scene.control.ToolBar;
import javafx.scene.image.Image;
import javafx.scene.input.KeyEvent;
import javafx.scene.input.MouseEvent;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.Pane;
import javafx.scene.layout.VBox;
import javafx.stage.Stage;
import javafx.stage.StageStyle;
import javafx.stage.WindowEvent;

public class SCWindow extends WindowDevice {

	private BorderPane rootPane;
	private ScrollPane mainCenterPane;
	private Insets insets;
		
	private TextField statusbar = null;
	
	/**
	 * The next boolean field is used to suppress
	 * WSIZ messages when not desired, such as during a 
	 * programmatic size change
	 */
	private boolean ignoreSizeOnce = false;
	
	private boolean ignoreMinimizeOnce = false;
	
	private final ChangeListener<Number> winWsizListener;
	
	private boolean fixsize = false;
	private boolean maximize = false;
	
	enum FChangeType {
		MINIMIZE,
		MAXIMIZE,
		RESTORE
	}

	/********************************************************************************
	 * 
	 * Constructor
	 * 
	 * @param element The XML 'PREP' Element from the server
	 */
	SCWindow(Element element) {
		super(element);
		name = element.getAttributeValue(Client.WINDOW);
		if (Client.isDebug()) System.out.println("Entering SCWindow, " + name);
		for(Attribute a1 : element.getAttributes()) {
			switch (a1.name) {
			case "fixsize":
				if (a1.value.equals("yes")) fixsize = true;
				break;
			case "maximize":
				if (a1.value.equals("yes")) maximize = true;
				break;
			case "statusbar":
				statusbar = new TextField();
				statusbar.setFocusTraversable(false);
				statusbar.setText(String.valueOf(' '));
				break;
			}
		}
		if (title == null) title = name;
		if (size.width == -1 || size.height == -1) {
			Dimension dim = dftk.getScreenSize();
			size.width = dim.width / 2;
			size.height = dim.height / 2;
		}
		
		/**
		 * Create a listener for the WSIZ messages
		 */
		winWsizListener = new ChangeListener<>() {

			@Override
			public void changed(ObservableValue<? extends Number> observable,
					Number oldValue, Number newValue)
			{
				if (Client.isDebug()) {
					String propName = ((ReadOnlyProperty<? extends Number>)observable).getName();
					System.out.println("WSIZ prop is " + propName);
				}
				int newWidth = (int) scene.getWidth();
				int newHeight = (int) scene.getHeight();
				if (size.width != newWidth || size.height != newHeight) {
					if (ignoreSizeOnce) {
						ignoreSizeOnce = false;
					}
					else {
						size.width = (int) scene.getWidth();
						size.height = (int) scene.getHeight();
						Client.WindowChangedSize(name, size.width, size.height);
					}
				}
			}
			
		};
		
		SmartClientApplication.Invoke(new Runnable() {
			@Override
			public void run() {
				stage = new Stage(StageStyle.DECORATED);  // Creates a new instance of decorated Stage.
				stage.setTitle(title);
				if (fixsize) stage.setResizable(false);
				rootPane = new BorderPane();
				mainCenterPane = new ScrollPane(new Pane());
				if (suppressScrollbars || fixsize) {
					mainCenterPane.setHbarPolicy(ScrollBarPolicy.NEVER);
					mainCenterPane.setVbarPolicy(ScrollBarPolicy.NEVER);
				}
				rootPane.setCenter(mainCenterPane);
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
				stage.iconifiedProperty().addListener(new ChangeListener<Boolean>() {
					@Override
					public void changed(
							ObservableValue<? extends Boolean> observable,
							Boolean oldValue, Boolean newValue) {
						if (!ignoreMinimizeOnce && newValue) {
							Client.WindowMinimized(name);
							ignoreMinimizeOnce = false;
						}
					}
				});
				xPropertyName = stage.xProperty().getName();
				yPropertyName = stage.yProperty().getName();
				stage.addEventHandler(KeyEvent.ANY, keyEventHandler);
				stage.addEventHandler(MouseEvent.ANY, mouseEventHandler);

				if (maximize) stage.setMaximized(true);

				if (SmartClientApplication.bigIcon != null || SmartClientApplication.littleIcon != null) {
					ObservableList<Image> l1 = stage.getIcons();
					l1.clear();
					if (SmartClientApplication.bigIcon != null) l1.add(SmartClientApplication.bigIcon);
					if (SmartClientApplication.littleIcon != null) l1.add(SmartClientApplication.littleIcon);
				}
				stage.show();
				stage.focusedProperty().addListener(focusListener);
				
				/**
				 * We get and remember the size of the stage's 'decorations'
				 */
				insets = new Insets((int)scene.getY(), (int)scene.getX(),
						(int)(stage.getHeight() - scene.getY() - scene.getHeight()),
						(int)(stage.getWidth() - scene.getX() - scene.getWidth()));
				
				if (statusbar != null) {
					rootPane.setBottom(statusbar);
					setStageHeight();
				}
				stage.xProperty().addListener(winPosChangeListener);
				stage.yProperty().addListener(winPosChangeListener);
				scene.heightProperty().addListener(winWsizListener);
				scene.widthProperty().addListener(winWsizListener);
				if (Client.isDebug()) {
					System.out.printf("Insets top:bottom %d:%d\n", insets.top, insets.bottom);
					System.out.printf("Windows height = %g\n", stage.getHeight());
				}
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
	protected void Change(Element element) {
		super.Change(element);
		int hsize = -1, vsize = -1;
		FChangeType f_Change = null;
		for(Attribute a1 : element.getAttributes()) {
			switch (a1.name) {
			case "hsize":
				hsize = Integer.parseInt(a1.value);
				break;
			case "vsize":
				vsize = Integer.parseInt(a1.value);
				break;
			case "f":
				if (a1.value.equals("minimize")) f_Change = FChangeType.MINIMIZE;
				else if (a1.value.equals("maximize")) f_Change = FChangeType.MAXIMIZE;
				else if (a1.value.equals("restore")) f_Change = FChangeType.RESTORE;
				break;
			case "statusbar":
				if (a1.value.equals("n")) RemoveStatusBar();
				break;
			case "statusbartext":
				changeStatusBar(a1.value);
				break;
			}
		}
		if (vsize != -1 && hsize != -1) {
			size.width = hsize;
			size.height = vsize;
			ignoreSizeOnce = true;
			stage.setWidth(size.width + insets.left + insets.right);
			setStageHeight();
		}
		if (f_Change == FChangeType.MINIMIZE) {
			ignoreSizeOnce = true;
			ignoreXChangeOnce = ignoreYChangeOnce = true;
			ignoreMinimizeOnce = true;
			stage.setIconified(true);
		}
		else if (f_Change == FChangeType.MAXIMIZE) {
			ignoreSizeOnce = true;
			ignoreXChangeOnce = ignoreYChangeOnce = true;
			stage.setMaximized(true);
		}
		else if (f_Change == FChangeType.RESTORE) {
			ignoreSizeOnce = true;
			ignoreXChangeOnce = ignoreYChangeOnce = true;
			if (stage.isIconified()) stage.setIconified(false);
			else if (stage.isMaximized()) stage.setMaximized(false);
		}
	}
	
	private void RemoveStatusBar()
	{
		if (Objects.nonNull(statusbar)) {
			SmartClientApplication.Invoke(new Runnable() {
				@Override
				public void run() {
					rootPane.setBottom(null);
				}
			}, true);
			statusbar = null;
			setStageHeight();
		}
	}
	
	private void changeStatusBar(final String barText)
	{
		boolean needHeightAdjustment = (statusbar == null);
		SmartClientApplication.Invoke(new Runnable() {
			@Override public void run() {
				if (Objects.nonNull(statusbar)) statusbar.setText(barText);
				else {
					statusbar = new TextField();
					statusbar.setFocusTraversable(false);
					statusbar.setText(barText);
					rootPane.setBottom(statusbar);
				}
			}
		}, true);
		if (needHeightAdjustment) setStageHeight();
	}
	
	@Override
	public void HideResource(Resource res) {
		assert res instanceof Panel || res instanceof SCMenu || res instanceof SCToolBar;
		if (res instanceof Panel) HidePanel((Panel)res);
		else if (res instanceof SCMenu) {
			assert res instanceof SCMenuBar && res.getContent() instanceof MenuBar;
			SmartClientApplication.Invoke(new Runnable() {
				@Override public void run() {
					Node topNode = rootPane.getTop();
					if (topNode instanceof MenuBar) {
						rootPane.setTop(null);
					}
					else if (topNode instanceof VBox) {
						Node oneNode = ((VBox)topNode).getChildren().get(1);
						assert oneNode instanceof ToolBar;
						rootPane.setTop(oneNode);
					}
				}
			}, true);
			setStageHeight();
			res.SetVisibleOn(null);
		}
		else if (res instanceof SCToolBar) {
			assert res.getContent() instanceof ToolBar;
			SmartClientApplication.Invoke(new Runnable() {
				@Override public void run() {
					Node topNode = rootPane.getTop();
					if (topNode instanceof ToolBar) {
						rootPane.setTop(null); // We were alone up here
					}
					else if (topNode instanceof VBox) {
						Node zeroNode = ((VBox)topNode).getChildren().get(0);
						assert zeroNode instanceof MenuBar;
						rootPane.setTop(zeroNode);
					}
				}
			}, true);
			setStageHeight();
			res.SetVisibleOn(null);
		}
	}
	
	void ShowToolBar(final SCToolBar scToolBar) {
		SmartClientApplication.Invoke(new Runnable() {
			@Override
			public void run() {
				Node topNode = rootPane.getTop();
				Node toolBar = (Node) scToolBar.getContent();
				if (topNode == null) {
					rootPane.setTop(toolBar);
				}
				else {
					assert topNode instanceof MenuBar;
					rootPane.setTop(new VBox(topNode, toolBar));
				}
			}
		}, true);
		scToolBar.SetVisibleOn(this);
		setStageHeight();
	}
	
	void ShowMenuBar(final SCMenuBar scMenuBar) {
		SmartClientApplication.Invoke(new Runnable() {
			@Override
			public void run() {
				Node topNode = rootPane.getTop();
				Node menuBar = (Node) scMenuBar.getContent();
				if (topNode == null) {
					rootPane.setTop(menuBar);
				}
				else {
					assert topNode instanceof ToolBar;
					rootPane.setTop(new VBox(menuBar, topNode));
				}
			}
		}, true);
		scMenuBar.SetVisibleOn(this);
		setStageHeight();
	}
	
	public void ShowPopupMenu(final SCContextMenu scContextMenu, final int h, final int v)
	{
		final Node anchorNode = getContentPane();
		SmartClientApplication.Invoke(new Runnable() {
			@Override
			public void run() {
				Point2D pnt = anchorNode.localToScreen(h, v);
				((ContextMenu)scContextMenu.getContent()).show(anchorNode, pnt.getX(), pnt.getY());
			}
		}, true);
		scContextMenu.SetVisibleOn(this);
	}

	private void setStageHeight() {
		if (stage.isMaximized() || stage.isFullScreen() || stage.isIconified()) return;
		final Node topnode = rootPane.getTop();
		final Node bottomnode = rootPane.getBottom();
		final double stageHeight = size.height + insets.top + insets.bottom;
		Task<Void> task = new Task<>() {
			@Override
			protected Void call() throws Exception {
				double topHeight = 0f;
				double bottomHeight = 0f;
				if (topnode != null) topHeight = calcNodeHeight(topnode, 20f);
				if (bottomnode != null) bottomHeight = calcNodeHeight(bottomnode, 20f);
				final double finalHeight = stageHeight + topHeight + bottomHeight;
				SmartClientApplication.Invoke(new Runnable() {
					@Override public void run() {
						ignoreSizeOnce = true;
						stage.setHeight(finalHeight);
					}
				}, false);
				return null;
			}
			
			private double calcNodeHeight(Node node, double min) {
				double height = 0f;
				int counter = 0;
				while (true) {
					Bounds bounds = node.boundsInLocalProperty().getValue();
					height = bounds.getMaxY() - bounds.getMinY() + 1;
					if (height > min) break;
					try {
						Thread.sleep(100);
					} catch (InterruptedException e) { /* Do Nothing */ }
					counter++;
					if (counter > 19) {
						if (Client.isDebug()) {
							SCWindow.dftk.beep();
							System.err.println("Failed to calc Node Height");
						}
						break;
					}
				}
				return height;
			}
		};
		Thread th = new Thread(task);
		th.setDaemon(true);
		th.start();
	}

	int GetStatusbarHeight() {
		final Node bottomnode = rootPane.getBottom();
		if (bottomnode == null || !(bottomnode instanceof TextField)) return 0;
		Bounds bounds = bottomnode.boundsInLocalProperty().getValue();
		double height = bounds.getMaxY() - bounds.getMinY() + 1;
		return (int) height;
	}

	@Override
	protected Pane getContentPane() {
		return (Pane) mainCenterPane.getContent();
	}
}
