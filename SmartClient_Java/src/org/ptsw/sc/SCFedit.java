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

import org.ptsw.sc.Client.ControlType;

import javafx.beans.value.ChangeListener;
import javafx.beans.value.ObservableValue;
import javafx.event.EventHandler;
import javafx.scene.control.TextField;
import javafx.scene.input.MouseEvent;

class SCFedit extends TextField implements ReadOnlyable, ShowOnlyable,
		RightClickable {

	private Client.ControlType controlType = ControlType.NORMAL;
	private boolean reportRightClicks;
	private String mask, unmaskedText;
	private int lft, rgt; // Will be zero, zero unless this is a numeric mask
	@SuppressWarnings("unused")
	private int maxChars;
	private boolean isNumMask;

	/**
	 * The Masked text
	 */
	private String text;

	SCFedit(final Resource res, String initialText, String mask) {
		super(initialText);
		this.mask = mask;
		isNumMask = isnummask();
		calcMaxChars();
		unmaskedText = initialText;
		text = applyMask();
		super.setText(text);
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
					Client.ControlButtonEvent(res.getId(), SCFedit.this, event);
					event.consume();
				}
			}
		});

		focusedProperty().addListener(new ChangeListener<Boolean>() {
			@Override
			public void changed(ObservableValue<? extends Boolean> observable,
					Boolean oldValue, Boolean newValue) {
				if (newValue) {
					SCFedit.super.setText(unmaskedText);
					selectAll();
				}
				else {
					unmaskedText = getText();
					text = applyMask();
					SCFedit.super.setText(text);
				}
			}
		});
		
		selectedTextProperty().addListener(new ChangeListener<String>() {
			@Override
			public void changed(ObservableValue<? extends String> observable,
					String oldValue, String newValue) {
				Client.EselMessage(res.getId(), getId(), newValue);
			}
		});
		
		textProperty().addListener(new ChangeListener<String>() {
			@Override
			public void changed(ObservableValue<? extends String> observable,
					String oldValue, String newValue) {
				if (res.itemOn) {
					unmaskedText = newValue;
					text = applyMask();
					Client.ItemMessage(res.getId(), getId(), text);
				}
			}
		});
	}

	void changeText(String value)
	{
		if (mask != null && value != null && value.length() > 0) {
			unmaskedText = value;
			value = text = applyMask();
		}
		else if (mask != null) value = "";
		super.setText(value);
		
	}

	void changeMask(String data) {
		mask = data;
		isNumMask = isnummask();
		calcMaxChars();
		unmaskedText = "";
		text = applyMask();
		super.setText(text);
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
	public void setReadonly() {
		controlType = ControlType.READONLY;
		setFocusTraversable(true);
	}

	@Override
	public boolean isReadonly() {
		return (controlType == ControlType.READONLY);
	}
	
	@Override
	public void setRightClick(boolean state) {
		reportRightClicks = state;
	}

	/**
	 * Apply mask to field unmaskedText
	 * 
	 */
	String applyMask() {
		int i1, i2, i3, spos, mpos, dpos;
		int ml = mask.length();
		int sl = unmaskedText.length();
		int l1, r1, negate;
		boolean zeroflg;
		StringBuilder rs = new StringBuilder(ml);
		if (!isNumMask) {
			for (mpos = spos = 0; mpos < ml;) {
				if (mask.charAt(mpos) == '9' || mask.charAt(mpos) == 'Z') { /*
																			 * start
																			 * of
																			 * numeric
																			 * field
																			 */
					/*
					 * CODE: verify that .charAt() calls will not cause
					 * exceptions
					 */
					for (i1 = 1; mpos + i1 < ml && (mask.charAt(mpos + i1) == '9' || mask.charAt(mpos + i1) == 'Z'); i1++)
						;
					for (i2 = 0; spos + i2 < sl && ((sl > 0 && Character.isDigit(unmaskedText.charAt(spos + i2))) || (sl > 0 && unmaskedText.charAt(spos + i2) == ' ')); i2++)
						;
					if (i1 > i2) {
						for (i3 = i1 - i2; i3-- > 0;) {
							if (mask.charAt(mpos++) == '9')
								rs.append('0');
							else
								rs.append(' ');
						}
					} else if (i1 < i2)
						i2 = i1;
					while (i2-- > 0) {
						if (mask.charAt(mpos++) == '9' && (sl > 0 && (unmaskedText.charAt(spos) == ' '))) {
							rs.append('0');
						} else
							rs.append(unmaskedText.charAt(spos));
						spos++;
					}
				} else if (mask.charAt(mpos) == 'A' || mask.charAt(mpos) == 'U' || mask.charAt(mpos) == 'L') { /*
																												 * any
																												 * char
																												 */
					if (spos < sl) {
						if (mask.charAt(mpos) == 'U') {
							rs.append(Character.toUpperCase(unmaskedText.charAt(spos)));
						} else if (mask.charAt(mpos) == 'L') {
							rs.append(Character.toLowerCase(unmaskedText.charAt(spos)));
						} else
							rs.append(unmaskedText.charAt(spos));
						spos++;
					}
					mpos++;
				} else {
					if (mask.charAt(mpos) == '\\') {
						if (++mpos == ml)
							break;
					}
					if (sl > 0 && spos < sl && (unmaskedText.charAt(spos) == mask.charAt(mpos))) {
						spos++;
					}
					rs.append(mask.charAt(mpos++));
				}
			}
		}
		else {
			l1 = lft;
			r1 = rgt;
			if (text != null)
				rs.append(text);
			while (rs.length() < ml)
				rs.append(' ');
			dpos = mpos = spos = 0;
			while (mpos < ml && (mask.charAt(mpos) != 'Z' && mask.charAt(mpos) != '9' && mask.charAt(mpos) != '.')) {
				if (mask.charAt(mpos) == '\\')
					mpos++;
				if (unmaskedText.charAt(spos) == mask.charAt(mpos))
					spos++;
				rs.setCharAt(dpos++, mask.charAt(mpos++));
			}
			while (spos < sl && unmaskedText.charAt(spos) == ' ')
				spos++;
			negate = 0;
			if (spos < sl && unmaskedText.charAt(spos) == '-') {
				spos++;
				if (mask.charAt(mpos) == 'Z')
					negate = 1; /* must have a 'Z' format to support negatives */
			}
			while (spos < sl && unmaskedText.charAt(spos) == '0')
				spos++;
			zeroflg = true;
			for (i1 = i2 = 0; i1 + spos < sl && Character.isDigit(unmaskedText.charAt(spos + i1)); i1++) {
				if (unmaskedText.charAt(spos + i1) != '0')
					zeroflg = false;
			}
			i3 = 0;
			if (i1 + spos < sl && unmaskedText.charAt(spos + i1) == '.') {
				for (i3 = i1 + 1; i2 < r1 && i3 + spos < sl && Character.isDigit(unmaskedText.charAt(spos + i3)); i2++, i3++)
					if (unmaskedText.charAt(spos + i3) != '0')
						zeroflg = false;
			}
			if (zeroflg)
				negate = 0;
			if (i1 + negate < l1) {
				do {
					if (mpos < ml && mask.charAt(mpos++) == 'Z') {
						if (negate > 0 && mpos < ml && mask.charAt(mpos) != 'Z') {
							negate = 0;
							rs.setCharAt(dpos, '-');
						} else
							rs.setCharAt(dpos, ' ');
					} else
						rs.setCharAt(dpos, '0');
					dpos++;
				} while (i1 + negate < --l1);
			} else if (i1 + negate > l1) {
				negate = 0;
				spos += i1 - l1;
				i1 = l1;
			}
			if (negate > 0)
				rs.setCharAt(dpos++, '-');
			if (i1 > 0) {
				for (i3 = 0; i3 < i1; i3++) {
					if (spos < sl)
						rs.setCharAt(dpos + i3, unmaskedText.charAt(spos++));
				}
				dpos += i1;
			}
			if (r1 > 0) {
				rs.setCharAt(dpos++, '.');
				if (i2 > 0) {
					spos++;
					for (i3 = 0; spos < sl && i3 < i2; i3++) {
						rs.setCharAt(dpos + i3, unmaskedText.charAt(spos++));
					}
					dpos += i2;
					r1 -= i2;
				}
				if (r1 > 0) {
					for (i3 = 0; i3 < r1; i3++) {
						rs.setCharAt(dpos + i3, '0');
					}
					dpos += r1;
				}
			}
		}
		return rs.toString();
	}


	/**
	 * Calculate the maximum number of characters that can be entered into
	 * unmaskedText.
	 */
	private void calcMaxChars() {
		for (int i1 = 0; i1 < mask.length(); i1++) {
			switch (mask.charAt(i1)) {
			case 'A':
			case 'U':
			case 'L':
			case 'Z':
			case '9':
				maxChars++;
				break;
			case '.':
				if (isNumMask)
					maxChars++;
				break;
			default:
				if (!isNumMask)
					maxChars++;
				break;
			}
		}
	}

	/**
	 * A mask is a number mask if it fits the following regular expression
	 * 
	 * Z*9*(\.9+)
	 * 
	 * We don't use JDK regular expressions because we must also count
	 * the mask positions left and right of the decimal.
	 */
	private boolean isnummask() {
		int state = 0; // scanning past zero or more Z
		int i1 = 0;
		while (i1 < mask.length()) {
			char mc = mask.charAt(i1);
			switch (state) {
			case 0:
				if (mc == 'Z') {
					i1++;
					lft++;
				} else
					state++;
				break;
			case 1:
				if (mc == '9') {
					i1++;
					lft++;
				} else if (mc == '.') {
					state++;
					i1++;
				} else
					return false;
				break;
			case 2:
				if (mc == '9') {
					i1++;
					rgt++;
				} else
					return false;
				break;
			}
		}
		return (lft == 0 && rgt == 0) ? false : true;
	}
}
