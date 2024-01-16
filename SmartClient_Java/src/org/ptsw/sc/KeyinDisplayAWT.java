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

import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.Frame;
import java.awt.Graphics;
import java.awt.Image;
import java.awt.Insets;
import java.awt.Rectangle;
import java.awt.Toolkit;
import java.awt.event.InputEvent;
import java.awt.event.KeyEvent;
import java.awt.event.KeyListener;
import java.awt.event.WindowEvent;
import java.awt.event.WindowListener;

@SuppressWarnings("serial")
public class KeyinDisplayAWT extends Frame implements WindowListener, KeyListener {

	private KeyinDisplay thekdswin; // caller
	int hpos; // current screen horz position (0 based)
	int vpos; // current screen vert position (0 based)
	private int hsize; // screen width in characters
	private int vsize; // screen height in characters
	private int cursortype; // type of cursor (kdswin CURSOR_ value)
	private Font thefont; // the font
	private int charwidth; // font pixel width
	private int charheight; // font pixel height
	private int charascent; // font pixel distance to baseline
	private Image image; // screen image
	private int imageheight; // height of screen image in pixels
	private int imagewidth; // width of screen image in pixels
	private int gh1; // graphic horz position 1 within character rectange
	private int gh2; // graphic horz position 2 within character rectange
	private int gh3; // graphic horz position 3 within character rectange
	private int gv1; // graphic vert position 1 within character rectange
	private int gv2; // graphic vert position 2 within character rectange
	private int gv3; // graphic vert position 3 within character rectange
	private Insets imageinset; // border insets of window frame
	private char[] charbuffer; // screen image characters
	private int[] attrbuffer; // screen image attributes
	private awtblinkcursor bcthread;// thread to set/clear blinkstate
	boolean blinkstate; // cursor blink on/off
	private boolean ignoreCloseRequest;

	// coding of graphics characters
	private static final int HLS = 1; // single horz line
	private static final int HLD = 2; // double horz line
	private static final int VLS = 3; // single vert line
	private static final int VLD = 4; // double vert line
	private static final int ULS = 5; // upper left corner single lines
	private static final int ULH = 6; // upper left corner horz double line,
	// vert single line
	private static final int ULV = 7; // upper left corner vert double line,
	// horz single line
	private static final int ULD = 8; // upper left corner double lines
	private static final int URS = 9;
	private static final int URH = 10;
	private static final int URV = 11;
	private static final int URD = 12;
	private static final int LLS = 13;
	private static final int LLH = 14;
	private static final int LLV = 15;
	private static final int LLD = 16;
	private static final int LRS = 17;
	private static final int LRH = 18;
	private static final int LRV = 19;
	private static final int LRD = 20;
	private static final int UTS = 21;
	private static final int UTH = 22;
	private static final int UTV = 23;
	private static final int UTD = 24;
	private static final int DTS = 25;
	private static final int DTH = 26;
	private static final int DTV = 27;
	private static final int DTD = 28;
	private static final int LTS = 29;
	private static final int LTH = 30;
	private static final int LTV = 31;
	private static final int LTD = 32;
	private static final int RTS = 33;
	private static final int RTH = 34;
	private static final int RTV = 35;
	private static final int RTD = 36;
	private static final int CRS = 37;
	private static final int CRH = 38;
	private static final int CRV = 39;
	private static final int CRD = 40;

	private final boolean debug;
	
	// mapping of unicode to above values
	private static final int graphicmap[] = { HLS, HLS, VLS, VLS, HLS, HLS, VLS, VLS, HLS, HLS, VLS, VLS, ULS, ULS, ULS, ULS, URS, URS, URS, URS, LLS, LLS, LLS, LLS, LRS, LRS,
			LRS, LRS, RTS, RTS, RTS, RTS, RTS, RTS, RTS, RTS, LTS, LTS, LTS, LTS, LTS, LTS, LTS, LTS, DTS, DTS, DTS, DTS, DTS, DTS, DTS, DTS, UTS, UTS, UTS, UTS, UTS, UTS, UTS,
			UTS, CRS, CRS, CRS, CRS, CRS, CRS, CRS, CRS, CRS, CRS, CRS, CRS, CRS, CRS, CRS, CRS, HLS, HLS, VLS, VLS, HLD, VLD, ULH, ULV, ULD, URH, URV, URD, LLH, LLV, LLD, LRH,
			LRV, LRD, RTH, RTV, RTD, LTH, LTV, LTD, DTH, DTV, DTD, UTH, UTV, UTD, CRH, CRV, CRD };

	KeyinDisplayAWT(KeyinDisplay k1, int h, int v, String font, int fontsize, int fontwidth, int fontheight) {
		super("DB/C");
		debug = Client.isDebug();
		if (debug) System.out.println("Entering KeyinDisplayAWT<>");
		addNotify();
		thekdswin = k1;
		if (h < 1)
			hsize = 80;
		else
			hsize = h;
		if (v < 1)
			vsize = 25;
		else
			vsize = v;
		if (font == null || font.length() == 0)
			font = "Monospaced";
		if (fontsize < 4 || fontsize > 72)
			fontsize = 14;
		setFont(new Font(font, Font.PLAIN, fontsize));
		thefont = getFont();
		FontMetrics fm1 = getFontMetrics(thefont);
		if (fontheight > 1)
			charheight = fontheight;
		else
			charheight = fm1.getHeight();
		if (fontwidth > 1)
			charwidth = fontwidth;
		else
			charwidth = fm1.charWidth('W');
		charascent = fm1.getAscent();
		gh2 = charwidth / 2;
		gh1 = gh2 - gh2 / 3;
		gh3 = gh2 + gh2 / 3;
		gv2 = charheight / 2;
		gv1 = gv2 - gv2 / 3;
		gv3 = gv2 + gv2 / 3;
		imageinset = getInsets();
		imageheight = vsize * charheight;
		imagewidth = hsize * charwidth;
		image = createImage(imagewidth, imageheight);
		charbuffer = new char[vsize * hsize];
		attrbuffer = new int[vsize * hsize];
		setLayout(null);
		pack();
		setSize(imagewidth + imageinset.right + imageinset.left, imageheight + imageinset.top + imageinset.bottom);
		setVisible(true);
		setSize(imagewidth + imageinset.right + imageinset.left, imageheight + imageinset.top + imageinset.bottom);
		addWindowListener(this);
		addKeyListener(this);
		bcthread = new awtblinkcursor(this);
		bcthread.setDaemon(true);
		bcthread.start();
		setFocusTraversalKeysEnabled(false);
	}

	public void kdsexit() {
		bcthread.interrupt();
		image.flush();
		setVisible(false);
		removeWindowListener(this);
		// removeKeyListener(this); // causes
		// java.lang.reflect.InvocationTargetException
		removeNotify();
		dispose();
	}

	public int kdsgethsize() {
		return hsize;
	}

	public int kdsgetvsize() {
		return vsize;
	}

	public void kdsflush() {
	}

	void kdscursor(int type) {
		cursortype = type;
		drawimagerect(vpos, vpos, hpos, hpos);
	}

	void kdspos(int h, int v) {
		int oldhpos = hpos;
		int oldvpos = vpos;
		hpos = h;
		vpos = v;
		if (cursortype != KeyinDisplay.CURSOR_NONE) {
			drawimagerect(oldvpos, oldvpos, oldhpos, oldhpos);
			drawimagerect(vpos, vpos, hpos, hpos);
		}
	}

	void kdseraserect(int top, int bottom, int left, int right, int dspattr) {
		int i1, i2;
		for (i1 = top; i1 <= bottom; i1++) {
			for (i2 = left; i2 <= right; i2++) {
				charbuffer[i1 * hsize + i2] = ' ';
				attrbuffer[i1 * hsize + i2] = dspattr;
			}
		}
		drawimagerect(top, bottom, left, right);
	}

	public char[] kdsgetrect(char[] ca1, boolean charsonly, int top, int bottom, int left, int right) {
		int i1, i2, i3;
		int length = ca1.length;
		i3 = 0;
		if (charsonly) {
			for (i1 = top; length > 0 && i1 <= bottom; i1++) {
				for (i2 = left; i2 <= right; i2++) {
					ca1[i3++] = charbuffer[i1 * hsize + i2];
					if (--length == 0)
						break;
				}
			}
		} else {
			length = length / 2;
			for (i1 = top; length > 0 && i1 <= bottom; i1++) {
				for (i2 = left; i2 <= right; i2++) {
					ca1[i3++] = charbuffer[i1 * hsize + i2];
					ca1[i3++] = (char) attrbuffer[i1 * hsize + i2];
					if (--length == 0)
						break;
				}
			}
		}
		return (ca1);
	}

	void kdsputrect(char[] ca1, int start, int length, boolean charsonly,
			int top, int bottom, int left, int right, int dspattr) {
		int i1, i2;
		int i3 = start;
		if (charsonly) {
			for (i1 = top; length > 0 && i1 <= bottom; i1++) {
				for (i2 = left; i2 <= right; i2++) {
					charbuffer[i1 * hsize + i2] = ca1[i3++];
					attrbuffer[i1 * hsize + i2] = dspattr;
					if (--length == 0)
						break;
				}
			}
		} else {
			length = length / 2;
			for (i1 = top; length > 0 && i1 <= bottom; i1++) {
				for (i2 = left; i2 <= right; i2++) {
					charbuffer[i1 * hsize + i2] = ca1[i3++];
					attrbuffer[i1 * hsize + i2] = ca1[i3++];
					if (--length == 0)
						break;
				}
			}
		}
		drawimagerect(top, bottom, left, right);
	}

	void kdsmoverect(int direction, int top, int bottom, int left, int right) {
		int i, j;
		switch (direction) {
		case KeyinDisplay.MOVE_UP:
			for (i = top; i <= bottom; i++) {
				for (j = left; j <= right; j++) {
					if (i != bottom) {
						charbuffer[i * hsize + j] = charbuffer[(i + 1) * hsize + j];
						attrbuffer[i * hsize + j] = attrbuffer[(i + 1) * hsize + j];
					} else {
						charbuffer[i * hsize + j] = ' ';
						attrbuffer[i * hsize + j] = 0;
					}
				}
			}
			break;
		case KeyinDisplay.MOVE_DOWN:
			for (i = bottom; i >= top; i--) {
				for (j = left; j <= right; j++) {
					if (i != top) {
						charbuffer[i * hsize + j] = charbuffer[(i - 1) * hsize + j];
						attrbuffer[i * hsize + j] = attrbuffer[(i - 1) * hsize + j];
					} else {
						charbuffer[i * hsize + j] = ' ';
						attrbuffer[i * hsize + j] = 0;
					}
				}
			}
			break;
		case KeyinDisplay.MOVE_LEFT:
			for (i = top; i <= bottom; i++) {
				for (j = left; j <= right; j++) {
					if (j != right) {
						charbuffer[i * hsize + j] = charbuffer[i * hsize + j + 1];
						attrbuffer[i * hsize + j] = attrbuffer[i * hsize + j + 1];
					} else {
						charbuffer[i * hsize + j] = ' ';
						attrbuffer[i * hsize + j] = 0;
					}
				}
			}
			break;
		case KeyinDisplay.MOVE_RIGHT:
			for (i = top; i <= bottom; i++) {
				for (j = right; j >= left; j--) {
					if (j != left) {
						charbuffer[i * hsize + j] = charbuffer[i * hsize + j - 1];
						attrbuffer[i * hsize + j] = attrbuffer[i * hsize + j - 1];
					} else {
						charbuffer[i * hsize + j] = ' ';
						attrbuffer[i * hsize + j] = 0;
					}
				}
			}
			break;
		}
		drawimagerect(top, bottom, left, right);
	}

	public void kdsbeep() {
		Toolkit.getDefaultToolkit().beep();
		return;
	}

	@Override
	public void keyPressed(KeyEvent kevent) {
		int key = kevent.getKeyCode();
		int modifier = kevent.getModifiersEx();
		int c1 = 0;
		switch (key) {
		case KeyEvent.VK_UP:
			c1 = KeyinDisplay.KEY_UP;
			break;
		case KeyEvent.VK_DOWN:
			c1 = KeyinDisplay.KEY_DOWN;
			break;
		case KeyEvent.VK_LEFT:
			c1 = KeyinDisplay.KEY_LEFT;
			break;
		case KeyEvent.VK_RIGHT:
			c1 = KeyinDisplay.KEY_RIGHT;
			break;
		case KeyEvent.VK_INSERT:
			c1 = KeyinDisplay.KEY_INSERT;
			break;
		case KeyEvent.VK_DELETE:
			c1 = KeyinDisplay.KEY_DELETE;
			break;
		case KeyEvent.VK_HOME:
			c1 = KeyinDisplay.KEY_HOME;
			break;
		case KeyEvent.VK_END:
			c1 = KeyinDisplay.KEY_END;
			break;
		case KeyEvent.VK_PAGE_UP:
			c1 = KeyinDisplay.KEY_PGUP;
			break;
		case KeyEvent.VK_PAGE_DOWN:
			c1 = KeyinDisplay.KEY_PGDN;
			break;
		}
		if (c1 != 0) {
			if ((modifier & InputEvent.SHIFT_DOWN_MASK) != 0)
				c1 += (KeyinDisplay.KEY_SHIFTUP - KeyinDisplay.KEY_UP);
			else if ((modifier & InputEvent.CTRL_DOWN_MASK) != 0)
				c1 += (KeyinDisplay.KEY_CTLUP - KeyinDisplay.KEY_UP);
			else if ((modifier & InputEvent.ALT_DOWN_MASK) != 0)
				c1 += (KeyinDisplay.KEY_ALTUP - KeyinDisplay.KEY_UP);
		} else {
			switch (key) {
			case KeyEvent.VK_F1:
				c1 = KeyinDisplay.KEY_F1;
				break;
			case KeyEvent.VK_F2:
				c1 = KeyinDisplay.KEY_F2;
				break;
			case KeyEvent.VK_F3:
				c1 = KeyinDisplay.KEY_F3;
				break;
			case KeyEvent.VK_F4:
				c1 = KeyinDisplay.KEY_F4;
				break;
			case KeyEvent.VK_F5:
				c1 = KeyinDisplay.KEY_F5;
				break;
			case KeyEvent.VK_F6:
				c1 = KeyinDisplay.KEY_F6;
				break;
			case KeyEvent.VK_F7:
				c1 = KeyinDisplay.KEY_F7;
				break;
			case KeyEvent.VK_F8:
				c1 = KeyinDisplay.KEY_F8;
				break;
			case KeyEvent.VK_F9:
				c1 = KeyinDisplay.KEY_F9;
				break;
			case KeyEvent.VK_F10:
				c1 = KeyinDisplay.KEY_F10;
				break;
			case KeyEvent.VK_F11:
				c1 = KeyinDisplay.KEY_F11;
				break;
			case KeyEvent.VK_F12:
				c1 = KeyinDisplay.KEY_F12;
				break;
			}
			if (c1 != 0) {
				if ((modifier & InputEvent.SHIFT_DOWN_MASK) != 0)
					c1 += (KeyinDisplay.KEY_SHIFTF1 - KeyinDisplay.KEY_F1);
				else if ((modifier & InputEvent.CTRL_DOWN_MASK) != 0)
					c1 += (KeyinDisplay.KEY_CTLF1 - KeyinDisplay.KEY_F1);
				else if ((modifier & InputEvent.ALT_DOWN_MASK) != 0)
					c1 += (KeyinDisplay.KEY_ALTF1 - KeyinDisplay.KEY_F1);
			}
		}
		if (c1 != 0)
			thekdswin.recvchar((char) c1);
	}

	@Override
	public synchronized void keyReleased(KeyEvent kevent) {
	}

	@Override
	public synchronized void keyTyped(KeyEvent kevent) {
		char c1 = kevent.getKeyChar();
		if (c1 == 10 || c1 == 13)
			c1 = KeyinDisplay.KEY_ENTER;
		else if (c1 == 27)
			c1 = KeyinDisplay.KEY_ESCAPE;
		else if (c1 == 8)
			c1 = KeyinDisplay.KEY_BKSPC;
		else if (c1 == 9) {
			if (kevent.isShiftDown())
				c1 = KeyinDisplay.KEY_BKTAB;
			else
				c1 = KeyinDisplay.KEY_TAB;
		}
		thekdswin.recvchar(c1);
	}

	@Override
	public void windowOpened(WindowEvent wevent) {
	}

	@Override
	public void windowIconified(WindowEvent wevent) {
	}

	@Override
	public void windowDeiconified(WindowEvent wevent) {
	}

	@Override
	public void windowClosed(WindowEvent wevent) {
	}

	@Override
	public void windowClosing(WindowEvent wevent) {
		if (!ignoreCloseRequest)
			thekdswin.recvchar(KeyinDisplay.KEY_INTERRUPT);
	}

	@Override
	public void windowActivated(WindowEvent wevent) {
	}

	@Override
	public void windowDeactivated(WindowEvent wevent) {
	}

	/* (non-Javadoc)
	 * @see java.awt.Window#paint(java.awt.Graphics)
	 */
	@Override
	public void paint(Graphics g) {
		update(g);
	}

	/* (non-Javadoc)
	 * @see java.awt.Container#update(java.awt.Graphics)
	 */
	@Override
	public synchronized void update(Graphics g) {
		Rectangle r1 = g.getClipBounds();
		int top = r1.y;
		int bottom = r1.y + r1.height - 1;
		int left = r1.x;
		int right = r1.x + r1.width - 1;
		g.drawImage(image, left, top, right + 1, bottom + 1, left - imageinset.left,
				top - imageinset.top, right - imageinset.left + 1, bottom - imageinset.top + 1, null, null);
	}

	synchronized void drawimagerect(int top, int bottom, int left, int right) {
		int h1, v1, i1, n1, index1;
		int index2 = 0;
		int attr;
		int hvsize = hsize * vsize;
		Graphics gimage = image.getGraphics();
		gimage.setFont(thefont);
		if (left == right) {
			for (v1 = top; v1 <= bottom; v1++) {
				i1 = v1 * hsize + left;
				attr = attrbuffer[i1];
				if ((attr & KeyinDisplay.DSPATTR_REVCOLOR) != 0) {
					index1 = (attr & KeyinDisplay.DSPATTR_BGCOLORMASK) >> KeyinDisplay.DSPATTR_BGCOLORSHIFT;
					index2 = (attr & KeyinDisplay.DSPATTR_FGCOLORMASK) >> KeyinDisplay.DSPATTR_FGCOLORSHIFT;
				} else {
					index1 = (attr & KeyinDisplay.DSPATTR_FGCOLORMASK) >> KeyinDisplay.DSPATTR_FGCOLORSHIFT;
					index2 = (attr & KeyinDisplay.DSPATTR_BGCOLORMASK) >> KeyinDisplay.DSPATTR_BGCOLORSHIFT;
				}
				if (v1 == vpos && left == hpos && blinkstate && cursortype != KeyinDisplay.CURSOR_NONE) {
					if (index2 == 7 || index2 == 15)
						gimage.setColor(DXColor.getColor(0));
					else
						gimage.setColor(DXColor.getColor(15));
					n1 = 0;
					if (cursortype == KeyinDisplay.CURSOR_ULINE)
						n1 = charheight - 2;
					else if (cursortype == KeyinDisplay.CURSOR_HALF)
						n1 = charheight / 2;
					gimage.fillRect(left * charwidth, v1 * charheight + n1, charwidth, charheight - n1);
					continue;
				}
				if ((attr & KeyinDisplay.DSPATTR_BOLD) != 0)
					index1 |= KeyinDisplay.DSPATTR_INTENSE;
				gimage.setColor(DXColor.getColor(index2));
				gimage.fillRect(left * charwidth, v1 * charheight, charwidth, charheight);
				gimage.setColor(DXColor.getColor(index1));
				if (charbuffer[i1] >= '\u2190')
					drawgraphic(gimage, charbuffer[i1], left * charwidth, v1 * charheight);
				else
					gimage.drawChars(charbuffer, i1, 1, left * charwidth, v1 * charheight + charascent);
			}
		} else {
			for (v1 = top; v1 <= bottom; v1++) {
				i1 = v1 * hsize + left;
				attr = attrbuffer[i1];
				if ((attr & KeyinDisplay.DSPATTR_REVCOLOR) != 0)
					index1 = (attr & KeyinDisplay.DSPATTR_FGCOLORMASK) >> KeyinDisplay.DSPATTR_FGCOLORSHIFT;
				else
					index1 = (attr & KeyinDisplay.DSPATTR_BGCOLORMASK) >> KeyinDisplay.DSPATTR_BGCOLORSHIFT;
				gimage.setColor(DXColor.getColor(index1));
				for (n1 = 1, h1 = left;;) {
					i1 = v1 * hsize + h1 + n1;
					if (i1 < hvsize) {
						attr = attrbuffer[i1];
						if ((attr & KeyinDisplay.DSPATTR_REVCOLOR) != 0)
							index2 = (attr & KeyinDisplay.DSPATTR_FGCOLORMASK) >> KeyinDisplay.DSPATTR_FGCOLORSHIFT;
						else
							index2 = (attr & KeyinDisplay.DSPATTR_BGCOLORMASK) >> KeyinDisplay.DSPATTR_BGCOLORSHIFT;
						if (index1 == index2) {
							n1++;
							if (h1 + n1 <= right)
								continue;
						}
					}
					gimage.fillRect(h1 * charwidth, v1 * charheight, charwidth * n1, charheight);
					h1 += n1;
					if (h1 > right) break;
					index1 = index2;
					gimage.setColor(DXColor.getColor(index1));
					n1 = 1;
				}
				for (h1 = left; h1 <= right; h1++) {
					i1 = v1 * hsize + h1;
					attr = attrbuffer[i1];
					if (v1 == vpos && h1 == hpos && blinkstate && cursortype != KeyinDisplay.CURSOR_NONE) {
						if ((attr & KeyinDisplay.DSPATTR_REVCOLOR) != 0)
							index2 = (attr & KeyinDisplay.DSPATTR_FGCOLORMASK) >> KeyinDisplay.DSPATTR_FGCOLORSHIFT;
						else
							index2 = (attr & KeyinDisplay.DSPATTR_BGCOLORMASK) >> KeyinDisplay.DSPATTR_BGCOLORSHIFT;
						if (index2 == 7 || index2 == 15)
							index1 = 0;
						else
							index1 = 15;
						gimage.setColor(DXColor.getColor(index1));
						n1 = 0;
						if (cursortype == KeyinDisplay.CURSOR_ULINE)
							n1 = charheight - 2;
						else if (cursortype == KeyinDisplay.CURSOR_HALF)
							n1 = charheight / 2;
						gimage.fillRect(h1 * charwidth, v1 * charheight + n1, charwidth, charheight - n1);
						continue;
					}
					if ((attr & KeyinDisplay.DSPATTR_REVCOLOR) != 0)
						index2 = (attr & KeyinDisplay.DSPATTR_BGCOLORMASK) >> KeyinDisplay.DSPATTR_BGCOLORSHIFT;
					else
						index2 = (attr & KeyinDisplay.DSPATTR_FGCOLORMASK) >> KeyinDisplay.DSPATTR_FGCOLORSHIFT;
					if ((attr & KeyinDisplay.DSPATTR_BOLD) != 0)
						index2 |= KeyinDisplay.DSPATTR_INTENSE;
					if (index1 != index2) {
						index1 = index2;
						gimage.setColor(DXColor.getColor(index1));
					}
					if (charbuffer[i1] >= '\u2190')
						drawgraphic(gimage, charbuffer[i1], h1 * charwidth, v1 * charheight);
					else
						gimage.drawChars(charbuffer, i1, 1, h1 * charwidth, v1 * charheight + charascent);
				}
			}
		}
		gimage.dispose();
		repaint(left * charwidth + imageinset.left, top * charheight + imageinset.top,
				charwidth * (right - left + 1), charheight * (bottom - top + 1));
	}

	private void drawgraphic(Graphics gimage, int n1, int left, int top) {
		int h1 = left;
		int h2 = left + gh1;
		int h3 = left + gh2;
		int h4 = left + gh3;
		int h5 = left + charwidth - 1;
		int v1 = top;
		int v2 = top + gv1;
		int v3 = top + gv2;
		int v4 = top + gv3;
		int v5 = top + charheight - 1;
		if (n1 < 0x2500) {
			if (n1 == 0x2190) {
				gimage.drawLine(h1, v3, h3, v2);
				gimage.drawLine(h1, v3, h3, v4);
				gimage.drawLine(h1, v3, h5, v3);
			} else if (n1 == 0x2191) {
				gimage.drawLine(h3, v1 + 2, h1 + 1, v3);
				gimage.drawLine(h3, v1 + 2, h5 - 1, v3);
				gimage.drawLine(h3, v1 + 2, h3, v5 - 2);
			} else if (n1 == 0x2192) {
				gimage.drawLine(h5, v3, h3, v2);
				gimage.drawLine(h5, v3, h3, v4);
				gimage.drawLine(h5, v3, h1, v3);
			} else if (n1 == 0x2193) {
				gimage.drawLine(h3, v5 - 2, h1 + 1, v3);
				gimage.drawLine(h3, v5 - 2, h5 - 1, v3);
				gimage.drawLine(h3, v5 - 2, h3, v1 + 2);
			}
			return;
		}
		n1 -= 0x2500;
		if (n1 > graphicmap.length)
			return;
		n1 = graphicmap[n1];
		switch (n1) {
		case HLS:
			gimage.drawLine(h1, v3, h5, v3);
			return;
		case HLD:
			gimage.drawLine(h1, v2, h5, v2);
			gimage.drawLine(h1, v4, h5, v4);
			return;
		case VLS:
			gimage.drawLine(h3, v1, h3, v5);
			return;
		case VLD:
			gimage.drawLine(h2, v1, h2, v5);
			gimage.drawLine(h4, v1, h4, v5);
			return;
		case ULS:
			gimage.drawLine(h3, v3, h5, v3);
			gimage.drawLine(h3, v3, h3, v5);
			return;
		case ULH:
			gimage.drawLine(h3, v2, h5, v2);
			gimage.drawLine(h3, v4, h5, v4);
			gimage.drawLine(h3, v2, h3, v5);
			return;
		case ULV:
			gimage.drawLine(h2, v3, h5, v3);
			gimage.drawLine(h2, v3, h2, v5);
			gimage.drawLine(h4, v3, h4, v5);
			return;
		case ULD:
			gimage.drawLine(h2, v2, h5, v2);
			gimage.drawLine(h4, v4, h5, v4);
			gimage.drawLine(h2, v2, h2, v5);
			gimage.drawLine(h4, v4, h4, v5);
			return;
		case URS:
			gimage.drawLine(h1, v3, h3, v3);
			gimage.drawLine(h3, v3, h3, v5);
			return;
		case URH:
			gimage.drawLine(h1, v2, h3, v2);
			gimage.drawLine(h1, v4, h3, v4);
			gimage.drawLine(h3, v2, h3, v5);
			return;
		case URV:
			gimage.drawLine(h1, v3, h4, v3);
			gimage.drawLine(h2, v3, h2, v5);
			gimage.drawLine(h4, v3, h4, v5);
			return;
		case URD:
			gimage.drawLine(h1, v2, h4, v2);
			gimage.drawLine(h1, v4, h2, v4);
			gimage.drawLine(h2, v4, h2, v5);
			gimage.drawLine(h4, v2, h4, v5);
			return;
		case LLS:
			gimage.drawLine(h3, v3, h5, v3);
			gimage.drawLine(h3, v1, h3, v3);
			return;
		case LLH:
			gimage.drawLine(h3, v2, h5, v2);
			gimage.drawLine(h3, v4, h5, v4);
			gimage.drawLine(h3, v1, h3, v4);
			return;
		case LLV:
			gimage.drawLine(h2, v3, h5, v3);
			gimage.drawLine(h2, v1, h2, v3);
			gimage.drawLine(h4, v1, h4, v3);
			return;
		case LLD:
			gimage.drawLine(h4, v2, h5, v2);
			gimage.drawLine(h2, v4, h5, v4);
			gimage.drawLine(h2, v1, h2, v4);
			gimage.drawLine(h4, v1, h4, v2);
			return;
		case LRS:
			gimage.drawLine(h1, v3, h3, v3);
			gimage.drawLine(h3, v1, h3, v3);
			return;
		case LRH:
			gimage.drawLine(h1, v2, h3, v2);
			gimage.drawLine(h1, v4, h3, v4);
			gimage.drawLine(h3, v1, h3, v4);
			return;
		case LRV:
			gimage.drawLine(h1, v3, h4, v3);
			gimage.drawLine(h2, v1, h2, v3);
			gimage.drawLine(h4, v1, h4, v3);
			return;
		case LRD:
			gimage.drawLine(h1, v2, h2, v2);
			gimage.drawLine(h1, v4, h4, v4);
			gimage.drawLine(h2, v1, h2, v2);
			gimage.drawLine(h4, v1, h4, v4);
			return;
		case UTS:
			gimage.drawLine(h1, v3, h5, v3);
			gimage.drawLine(h3, v1, h3, v3);
			return;
		case UTH:
			gimage.drawLine(h1, v2, h5, v2);
			gimage.drawLine(h1, v4, h5, v4);
			gimage.drawLine(h3, v1, h3, v2);
			return;
		case UTV:
			gimage.drawLine(h1, v3, h5, v3);
			gimage.drawLine(h2, v1, h2, v3);
			gimage.drawLine(h4, v1, h4, v3);
			return;
		case UTD:
			gimage.drawLine(h1, v2, h2, v2);
			gimage.drawLine(h4, v2, h5, v2);
			gimage.drawLine(h1, v4, h5, v4);
			gimage.drawLine(h2, v1, h2, v2);
			gimage.drawLine(h4, v1, h4, v2);
			return;
		case DTS:
			gimage.drawLine(h1, v3, h5, v3);
			gimage.drawLine(h3, v3, h3, v5);
			return;
		case DTH:
			gimage.drawLine(h1, v2, h5, v2);
			gimage.drawLine(h1, v4, h5, v4);
			gimage.drawLine(h3, v4, h3, v5);
			return;
		case DTV:
			gimage.drawLine(h1, v3, h5, v3);
			gimage.drawLine(h2, v3, h2, v5);
			gimage.drawLine(h4, v3, h4, v5);
			return;
		case DTD:
			gimage.drawLine(h1, v2, h5, v2);
			gimage.drawLine(h1, v4, h2, v4);
			gimage.drawLine(h4, v4, h5, v4);
			gimage.drawLine(h2, v4, h2, v5);
			gimage.drawLine(h4, v4, h4, v5);
			return;
		case LTS:
			gimage.drawLine(h1, v3, h3, v3);
			gimage.drawLine(h3, v1, h3, v5);
			return;
		case LTH:
			gimage.drawLine(h1, v2, h3, v2);
			gimage.drawLine(h1, v4, h3, v4);
			gimage.drawLine(h3, v1, h3, v5);
			return;
		case LTV:
			gimage.drawLine(h1, v3, h2, v3);
			gimage.drawLine(h2, v1, h2, v5);
			gimage.drawLine(h4, v1, h4, v5);
			return;
		case LTD:
			gimage.drawLine(h1, v2, h2, v2);
			gimage.drawLine(h1, v4, h2, v4);
			gimage.drawLine(h2, v1, h2, v2);
			gimage.drawLine(h2, v4, h2, v5);
			gimage.drawLine(h4, v1, h4, v5);
			return;
		case RTS:
			gimage.drawLine(h3, v3, h5, v3);
			gimage.drawLine(h3, v1, h3, v5);
			return;
		case RTH:
			gimage.drawLine(h3, v2, h5, v2);
			gimage.drawLine(h3, v4, h5, v4);
			gimage.drawLine(h3, v1, h3, v5);
			return;
		case RTV:
			gimage.drawLine(h4, v3, h5, v3);
			gimage.drawLine(h2, v1, h2, v5);
			gimage.drawLine(h4, v1, h4, v5);
			return;
		case RTD:
			gimage.drawLine(h4, v2, h5, v2);
			gimage.drawLine(h4, v4, h5, v4);
			gimage.drawLine(h2, v1, h2, v5);
			gimage.drawLine(h4, v1, h4, v2);
			gimage.drawLine(h4, v4, h4, v5);
			return;
		case CRS:
			gimage.drawLine(h1, v3, h5, v3);
			gimage.drawLine(h3, v1, h3, v5);
			return;
		case CRH:
			gimage.drawLine(h1, v2, h5, v2);
			gimage.drawLine(h1, v4, h5, v4);
			gimage.drawLine(h3, v1, h3, v5);
			return;
		case CRV:
			gimage.drawLine(h1, v3, h5, v3);
			gimage.drawLine(h2, v1, h2, v5);
			gimage.drawLine(h4, v1, h4, v5);
			return;
		case CRD:
			gimage.drawLine(h1, v2, h2, v2);
			gimage.drawLine(h4, v2, h5, v2);
			gimage.drawLine(h1, v4, h2, v4);
			gimage.drawLine(h4, v4, h5, v4);
			gimage.drawLine(h2, v1, h2, v2);
			gimage.drawLine(h4, v1, h4, v2);
			gimage.drawLine(h2, v4, h2, v5);
			gimage.drawLine(h4, v4, h4, v5);
			return;
		}
	}

	void setNoclose() {
		ignoreCloseRequest = true;
	}

}

class awtblinkcursor extends Thread {

	private KeyinDisplayAWT thisawt;

	awtblinkcursor(KeyinDisplayAWT awt) {
		thisawt = awt;
	}

	@Override
	public void run() {
		while (true) {
			try {
				Thread.sleep(250L);
			} catch (InterruptedException e) {
			}
			thisawt.blinkstate = true;
			thisawt.drawimagerect(thisawt.vpos, thisawt.vpos, thisawt.hpos, thisawt.hpos);
			try {
				Thread.sleep(250L);
			} catch (InterruptedException e) {
			}
			thisawt.blinkstate = false;
			thisawt.drawimagerect(thisawt.vpos, thisawt.vpos, thisawt.hpos, thisawt.hpos);
		}
	}

}
