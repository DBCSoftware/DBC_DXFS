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
import java.awt.Point;
import java.awt.Toolkit;
import java.util.Objects;

import org.ptsw.sc.xml.Attribute;
import org.ptsw.sc.xml.Element;

import javafx.beans.property.ReadOnlyProperty;
import javafx.beans.value.ChangeListener;
import javafx.beans.value.ObservableValue;
import javafx.collections.ObservableList;
import javafx.event.EventHandler;
import javafx.scene.Cursor;
import javafx.scene.Node;
import javafx.scene.Scene;
import javafx.scene.canvas.Canvas;
import javafx.scene.input.KeyCode;
import javafx.scene.input.KeyEvent;
import javafx.scene.input.MouseEvent;
import javafx.scene.layout.Pane;
import javafx.stage.Stage;

public abstract class WindowDevice {

	static final Toolkit dftk = Toolkit.getDefaultToolkit();
	protected String name;
	protected String title;
	protected Stage stage;
	protected Scene scene;
	protected boolean reportMouseMovement = false;
	protected Point lastMouseMovement = new Point(-1, -1);
	protected String xPropertyName;
	protected String yPropertyName;

	/**
	 * This is the size of the 'window' as the db/c programmer sees it.
	 * But in here it is the size of the 'center' pane. It does not include the
	 * height of the toolbar, or statusbar, or decorations.
	 */
	protected Dimension size = new Dimension(-1, -1);
	
	protected Point position = new Point(-1, -1);

	/**
	 * The next two boolean fields are used to suppress
	 * WPOS messages when not desired, such as during a 
	 * programmatic position change
	 */
	protected boolean ignoreXChangeOnce = false;
	protected boolean ignoreYChangeOnce = false;
	
	/**
	 * Set to true if 'noscrollbars' is in the window prep string
	 */
	protected boolean suppressScrollbars = false;
	
	/**
	 * This listens for size and position changes to the window
	 * for WPOS messages
	 */
	protected final ChangeListener<Number> winPosChangeListener = new ChangeListener<>(){
		@Override
		public void changed(ObservableValue<? extends Number> observable,
				Number oldValue, Number newValue) {
			String propName = ((ReadOnlyProperty<? extends Number>)observable).getName();
			if (propName.equals(xPropertyName) || propName.equals(yPropertyName)) {
				if (ignoreXChangeOnce && propName.equals(xPropertyName)) {
					ignoreXChangeOnce = false;
					return;
				}
				if (ignoreYChangeOnce && propName.equals(yPropertyName)) {
					ignoreYChangeOnce = false;
					return;
				}
				position.x = stage.xProperty().intValue();
				position.y = stage.yProperty().intValue();
				Client.WindowChangedPos(name, position.x, position.y);
			}
		}
	};

	/**
	 * A window sends mouse button UP DN messages with the position, even if mouseoff.
	 * If mouseon, it also sends position and exit messages.
	 */
	protected final EventHandler<MouseEvent> mouseEventHandler = new EventHandler<>() {

		@Override
		public void handle(MouseEvent event) {
			if (event.getEventType() == MouseEvent.MOUSE_PRESSED
					|| event.getEventType() == MouseEvent.MOUSE_RELEASED
					|| (event.getEventType() == MouseEvent.MOUSE_CLICKED && event.getClickCount() == 2))
			{
				Client.windowMouseButtonEvent(name, event);
			}
			else if (event.getEventType() == MouseEvent.MOUSE_MOVED && reportMouseMovement) {
				Point newLoc = new Point((int)event.getX(), (int)event.getY());
				if (!newLoc.equals(lastMouseMovement)) {
					Client.windowMouseMovedEvent(name, lastMouseMovement, newLoc);
					lastMouseMovement = newLoc;
				}
			}
			else if (event.getEventType() == MouseEvent.MOUSE_EXITED_TARGET && reportMouseMovement) {
				Point mLoc = new Point((int)event.getX(), (int)event.getY());
				if (Client.isDebug()) System.out.println("Mouse Exited " + mLoc.x + ":" + mLoc.y);
				Client.WindowMouseExit(name, calculateMouseExitMode(mLoc));
			}
		}
	};
	
	protected final ChangeListener<Boolean> focusListener = new ChangeListener<>() {
		@Override public void changed(
				ObservableValue<? extends Boolean> observable,
				Boolean oldValue, Boolean newValue) {
			if (newValue) Client.WindowFocused(name);
		}
	};
	
	/**
	 * NKEY messages for F1 through F12 from a Window add information about the state
	 * of the Shift and Ctrl keys 
	 * (NKEY messages from a Control do not)
	 * 
	 * ALT-Letter is sent as NKEY, Alt-digit is sent as CHAR
	 * 
	 */
	protected EventHandler<KeyEvent> keyEventHandler = new EventHandler<>() {
		@Override
		public void handle(KeyEvent event) {
			if (event.getEventType().equals(KeyEvent.KEY_TYPED)) {
				String str1 = event.getCharacter();
				char char1 = str1.charAt(0);
				if (Character.isLetterOrDigit(char1)) {
					Client.CharMessage(name, char1);
				}
			}
			else if (event.getEventType().equals(KeyEvent.KEY_PRESSED)) {
				KeyCode kc1 = event.getCode();
				if (event.isAltDown()) {
					if (kc1.isLetterKey() && ClientUtils.LetterKeyMap.containsKey(kc1)) {
						Integer i1 = ClientUtils.LetterKeyMap.get(kc1);
						Client.NkeyMessage(name, i1.toString());
					}
					//
					// Alt-digit combos are sent as CHAR as if alt was not there
					//
					else if (kc1.isDigitKey() && !kc1.isKeypadKey()) {
						Client.CharMessage(name, event.getText().charAt(0));
					}
				}
				else if (ClientUtils.FKeyMap.containsKey(kc1)) {
					int i1 = ClientUtils.FKeyMap.get(kc1).intValue();
					if (kc1.isFunctionKey()) {
						if (event.isControlDown()) i1 += 40;
						else if (event.isShiftDown()) i1 += 20;
					}
					// No KeyCode for Back_Tab, deal with it.
					else if (kc1.equals(KeyCode.TAB) && event.isShiftDown()) i1++;
					else if (kc1.isNavigationKey() || kc1.equals(KeyCode.INSERT)
						|| kc1.equals(KeyCode.DELETE))
					{
						if (event.isShiftDown()) i1 += 10;
						else if (event.isControlDown()) i1 += 20;
					}
					Client.NkeyMessage(name, String.valueOf(i1));
				}
			}
		}
	};
	
	public WindowDevice(Element element) {
		Objects.requireNonNull(element, "element cannot be null");
		if (Client.IsTrace()) System.out.println("Entering WindowDevice<>");
		for(Attribute a1 : element.getAttributes()) {
			switch (a1.name) {
			case "hsize":
				size.width = Integer.parseInt(a1.value);
				break;
			case "vsize":
				size.height = Integer.parseInt(a1.value);
				break;
			case "posx":
				position.x = Integer.parseInt(a1.value);
				break;
			case "posy":
				position.y = Integer.parseInt(a1.value);
				break;
			case "title":
				title = a1.value;
				break;
			case "scrollbars":
				if (a1.value.equals("no")) suppressScrollbars = true;
				break;
			case "noclose":
			case "nofocus":
			case "notaskbarbutton":
			case "pandlgscale":
				if (!SmartClientApplication.ignoreUnsupportedChangeCommands) {
					throw new UnsupportedOperationException(String.format(
							"Java SC does not support %s in a window prep string", a1.name));
				}
				break; /* Not Supported */
			}
		}
		if (position.x == -1 || position.y == -1) {
			Dimension dim = dftk.getScreenSize();
			position.x = (dim.width - size.width) / 2;
			position.y = (dim.height - size.height) / 2;
		}
	}
	
	protected void Change(Element element) {
		Objects.requireNonNull(element, "element cannot be null");
		int hpos = -1, vpos = -1;
		for(final Attribute a1 : element.getAttributes()) {
			switch (a1.name) {
			case "h":
				hpos = Integer.parseInt(a1.value);
				break;
			case "v":
				vpos = Integer.parseInt(a1.value);
				break;
			case "f":
				if (a1.value.equals("desktopicon")) {
					if (!SmartClientApplication.ignoreUnsupportedChangeCommands) {
						throw new UnsupportedOperationException(String.format(
								"Java SC does not support %s in a window change command", a1.value));
					}
					element.getAttributes().remove(a1);
				}
				break;
			case "focus":
				if (a1.value.equals("y")) stage.requestFocus();
				else {
					if (!SmartClientApplication.ignoreUnsupportedChangeCommands) {
						throw new UnsupportedOperationException(
								"Java SC does not support UNFOCUS in a window change command");
					}
				}
				break;
			case "mouse":
				reportMouseMovement = (a1.value.equals("y"));
				break;
			case "pointer":
				changePointer(a1.value);
				break;
			case "title":
				SmartClientApplication.Invoke(new Runnable() {
					@Override
					public void run() {
						stage.setTitle(a1.value);
					}
				}, false);
				break;
			case "caret":
			case "caretposh":
			case "caretposv":
			case "caretsize":
				if (!SmartClientApplication.ignoreUnsupportedChangeCommands) {
					throw new UnsupportedOperationException(
							"Java SC does not support any CARET window change commands");
				}
				break;
			case "hsbpos":
			case "vsbpos":
			case "hsboff":
			case "vsboff":
			case "hsbrange":
			case "vsbrange":
				if (!SmartClientApplication.ignoreUnsupportedChangeCommands) {
					throw new UnsupportedOperationException(
							"Java SC does not support any SCROLLBAR window change commands");
				}
				break;
			case "ttwidth": // Undocumented 'helptextwidth' 
				if (!SmartClientApplication.ignoreUnsupportedChangeCommands) {
					throw new UnsupportedOperationException(
							"Java SC does not support 'helptextwidth' in a window change command");
				}
				break;
			}
		}
		if (hpos != -1 && vpos != -1) {
			position.x = hpos;
			position.y = vpos;
			ignoreXChangeOnce = ignoreYChangeOnce = true;
			stage.setX(hpos);
			stage.setY(vpos);
		}
	}
	
	/**
	 * @return the stage
	 */
	public Stage getStage() {
		return stage;
	}

	protected abstract Pane getContentPane();
	
	public abstract void HideResource(Resource res);
	
	protected void HidePanel(final Panel panel)
	{
		Objects.requireNonNull(panel, "panel cannot be null");
		final Pane pn1 = getContentPane();
		SmartClientApplication.Invoke(new Runnable() {
			@Override
			public void run() {
				pn1.getChildren().remove(panel.getContent());
			}
		}, true);
		panel.SetVisibleOn(null);
	}
	

	/**
	 * "appstarting",
	 * "arrow", "handpoint", "wait", "cross", "ibeam", "help", "uparrow", "sizeall", "sizens", "sizewe",
	 * "sizenesw", "sizenwse", and "no".
	 */
	protected void changePointer(String pntr) {
		Cursor crsr = null;
		switch (pntr) {
		case "appstarting":
			break;
		case "arrow":
			crsr = Cursor.DEFAULT;
			break;
		case "cross":
			crsr = Cursor.CROSSHAIR;
			break;
		case "handpoint":
			crsr = Cursor.HAND;
			break;
		case "help":
			break;
		case "ibeam":
			crsr = Cursor.TEXT;
			break;
		case "no":
			break;
		case "sizeall":
			crsr = Cursor.MOVE;
			break;
		case "sizenesw":
			crsr = Cursor.NE_RESIZE;
			break;
		case "sizens":
			crsr = Cursor.V_RESIZE;
			break;
		case "sizenwse":
			crsr = Cursor.NW_RESIZE;
			break;
		case "sizewe":
			crsr = Cursor.H_RESIZE;
			break;
		case "uparrow":
			crsr = Cursor.N_RESIZE;
			break;
		case "wait":
			crsr = Cursor.WAIT;
			break;
		}
		if (crsr != null) scene.setCursor(crsr);
	}
	
	protected void ShowPanel(final Panel panel, final int X, final int Y) {
		Objects.requireNonNull(panel, "panel cannot be null");
		if (Client.isDebug()) {
			System.out.println("Entering WindowDevice.ShowPanel");
			System.out.flush();
		}
		SmartClientApplication.Invoke(() -> {
			if (X != -1 && Y != -1) ((Node) panel.getContent()).relocate(X, Y);
			(getContentPane()).getChildren().add((Node) panel.getContent());
		}, true);
		panel.SetVisibleOn(this);
	}

	public void HideImage(final ImageResource img1) {
		Objects.requireNonNull(img1, "img1 cannot be null");
		final Pane pn1 = getContentPane();
		SmartClientApplication.Invoke(new Runnable() {
			@Override
			public void run() {
				pn1.getChildren().remove(img1.getContent());
			}
		}, true);
		img1.SetVisibleOn(null);
	}
	
	void ShowImage(final ImageResource ir, final int X, final int Y) {
		Objects.requireNonNull(ir, "ir cannot be null");
		if (Client.IsTrace()) System.out.printf("Entering WindowDevice.ShowImage at %d:%d\n", X, Y);
		SmartClientApplication.Invoke(new Runnable() {
			@Override
			public void run() {
				ObservableList<Node> olist = (getContentPane()).getChildren();
				Canvas n1 = (Canvas) ir.getContent();
				if (olist.contains(n1)) olist.remove(n1);
				if (X != -1 && Y != -1) n1.relocate(X, Y);
				olist.add(n1);
			}
		}, true);
		ir.SetVisibleOn(this);
	}
	
	private String calculateMouseExitMode(Point loc) {
		Objects.requireNonNull(loc, "loc cannot be null");
		String func;
		if (loc.x >= size.width) {
			if (loc.y < 0) {
				if (loc.x - size.width > 0 - loc.y)
					func = "posr";
				else
					func = "post";
			} else if (loc.y < size.height) {
				func = "posr";
			} else {
				if (loc.y - size.height > loc.x - size.width) {
					func = "posb";
				} else
					func = "posr";
			}
		} else if (loc.x >= 0) {
			if (loc.y < 0)
				func = "post";
			else
				func = "posb";
		} else {
			if (loc.y < 0) {
				if (loc.x < loc.y)
					func = "posl";
				else
					func = "post";
			} else if (loc.y <= size.height) {
				func = "posl";
			} else {
				if (loc.y - size.height > 0 - loc.x) {
					func = "posb";
				} else
					func = "posl";
			}
		}
		return func;
	}

}
