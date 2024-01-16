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

import java.util.Date;

/**
 * class kdswin An object of class kdswin represents a screen console and
 * keyboard The caller's coordinates are (1, 1) for upper left corner. The
 * kdswin internal coordinates are (0, 0) for upper left corner.
 */
public class KeyinDisplay {

	public static final int FLAG_DISPLAYCONSOLE = 0x00000001;
	public static final int FLAG_DISPLAYTERMINAL = 0x00000002;
	public static final int FLAG_KEYINUPCASE = 0x00000004;
	public static final int FLAG_KEYINREVCASE = 0x00000008;
	public static final int FLAG_CONSOLENOCLOSE = 0x00000010;

	public static final char KEY_ENTER = '\uF100'; // enter
	public static final char KEY_ESCAPE = '\uF101'; // escape
	public static final char KEY_BKSPC = '\uF102'; // backspace
	public static final char KEY_TAB = '\uF103'; // tab
	public static final char KEY_BKTAB = '\uF104'; // back tab

	public static final char KEY_UP = '\uF105'; // up arrow
	public static final char KEY_DOWN = '\uF106'; // down arrow
	public static final char KEY_LEFT = '\uF107'; // left arrow
	public static final char KEY_RIGHT = '\uF108'; // right arrow
	public static final char KEY_INSERT = '\uF109'; // insert
	public static final char KEY_DELETE = '\uF10A'; // delete
	public static final char KEY_HOME = '\uF10B'; // home
	public static final char KEY_END = '\uF10C'; // end
	public static final char KEY_PGUP = '\uF10D'; // page up
	public static final char KEY_PGDN = '\uF10E'; // page down

	public static final char KEY_SHIFTUP = '\uF10F'; // shift-up arrow
	public static final char KEY_SHIFTDOWN = '\uF110'; // shift-down arrow
	public static final char KEY_SHIFTLEFT = '\uF111'; // shift-left arrow
	public static final char KEY_SHIFTRIGHT = '\uF112'; // shift-right arrow
	public static final char KEY_SHIFTINSERT = '\uF113'; // shift-insert
	public static final char KEY_SHIFTDELETE = '\uF114'; // shift-delete
	public static final char KEY_SHIFTHOME = '\uF115'; // shift-home
	public static final char KEY_SHIFTEND = '\uF116'; // shift-end
	public static final char KEY_SHIFTPGUP = '\uF117'; // shift-page up
	public static final char KEY_SHIFTPGDN = '\uF118'; // shift-page down

	public static final char KEY_CTLUP = '\uF119'; // control-up arrow
	public static final char KEY_CTLDOWN = '\uF11A'; // control-down arrow
	public static final char KEY_CTLLEFT = '\uF11B'; // control-left arrow
	public static final char KEY_CTLRIGHT = '\uF11C'; // control-right arrow
	public static final char KEY_CTLINSERT = '\uF11D'; // control-insert
	public static final char KEY_CTLDELETE = '\uF11E'; // control-insert
	public static final char KEY_CTLHOME = '\uF11F'; // control-home
	public static final char KEY_CTLEND = '\uF120'; // control-end
	public static final char KEY_CTLPGUP = '\uF121'; // control-page up
	public static final char KEY_CTLPGDN = '\uF122'; // control-page down

	public static final char KEY_ALTUP = '\uF123'; // alt-up arrow
	public static final char KEY_ALTDOWN = '\uF124'; // alt-down arrow
	public static final char KEY_ALTLEFT = '\uF125'; // alt-left arrow
	public static final char KEY_ALTRIGHT = '\uF126'; // alt-right arrow
	public static final char KEY_ALTINSERT = '\uF127'; // alt-insert
	public static final char KEY_ALTDELETE = '\uF128'; // alt-insert
	public static final char KEY_ALTHOME = '\uF129'; // alt-home
	public static final char KEY_ALTEND = '\uF12A'; // alt-end
	public static final char KEY_ALTPGUP = '\uF12B'; // alt-page up
	public static final char KEY_ALTPGDN = '\uF12C'; // alt-page down

	public static final char KEY_F1 = '\uF12D'; // F1
	public static final char KEY_F2 = '\uF12E'; // F2
	public static final char KEY_F3 = '\uF12F'; // F3
	public static final char KEY_F4 = '\uF130'; // F4
	public static final char KEY_F5 = '\uF131'; // F5
	public static final char KEY_F6 = '\uF132'; // F6
	public static final char KEY_F7 = '\uF133'; // F7
	public static final char KEY_F8 = '\uF134'; // F8
	public static final char KEY_F9 = '\uF135'; // F9
	public static final char KEY_F10 = '\uF136'; // F10
	public static final char KEY_F11 = '\uF137'; // F11
	public static final char KEY_F12 = '\uF138'; // F12
	public static final char KEY_F13 = '\uF139'; // F13
	public static final char KEY_F14 = '\uF13A'; // F14
	public static final char KEY_F15 = '\uF13B'; // F15
	public static final char KEY_F16 = '\uF13C'; // F16
	public static final char KEY_F17 = '\uF13D'; // F17
	public static final char KEY_F18 = '\uF13E'; // F18
	public static final char KEY_F19 = '\uF13F'; // F19
	public static final char KEY_F20 = '\uF140'; // F20

	public static final char KEY_SHIFTF1 = '\uF141'; // shift-F1
	public static final char KEY_SHIFTF2 = '\uF142'; // shift-F2
	public static final char KEY_SHIFTF3 = '\uF143'; // shift-F3
	public static final char KEY_SHIFTF4 = '\uF144'; // shift-F4
	public static final char KEY_SHIFTF5 = '\uF145'; // shift-F5
	public static final char KEY_SHIFTF6 = '\uF146'; // shift-F6
	public static final char KEY_SHIFTF7 = '\uF147'; // shift-F7
	public static final char KEY_SHIFTF8 = '\uF148'; // shift-F8
	public static final char KEY_SHIFTF9 = '\uF149'; // shift-F9
	public static final char KEY_SHIFTF10 = '\uF14A'; // shift-F10
	public static final char KEY_SHIFTF11 = '\uF14B'; // shift-F11
	public static final char KEY_SHIFTF12 = '\uF14C'; // shift-F12
	public static final char KEY_SHIFTF13 = '\uF14D'; // shift-F13
	public static final char KEY_SHIFTF14 = '\uF14E'; // shift-F14
	public static final char KEY_SHIFTF15 = '\uF14F'; // shift-F15
	public static final char KEY_SHIFTF16 = '\uF150'; // shift-F16
	public static final char KEY_SHIFTF17 = '\uF151'; // shift-F17
	public static final char KEY_SHIFTF18 = '\uF152'; // shift-F18
	public static final char KEY_SHIFTF19 = '\uF153'; // shift-F19
	public static final char KEY_SHIFTF20 = '\uF154'; // shift-F20

	public static final char KEY_CTLF1 = '\uF155'; // control-F1
	public static final char KEY_CTLF2 = '\uF156'; // control-F2
	public static final char KEY_CTLF3 = '\uF157'; // control-F3
	public static final char KEY_CTLF4 = '\uF158'; // control-F4
	public static final char KEY_CTLF5 = '\uF159'; // control-F5
	public static final char KEY_CTLF6 = '\uF15A'; // control-F6
	public static final char KEY_CTLF7 = '\uF15B'; // control-F7
	public static final char KEY_CTLF8 = '\uF15C'; // control-F8
	public static final char KEY_CTLF9 = '\uF15D'; // control-F9
	public static final char KEY_CTLF10 = '\uF15E'; // control-F10
	public static final char KEY_CTLF11 = '\uF15F'; // control-F11
	public static final char KEY_CTLF12 = '\uF160'; // control-F12
	public static final char KEY_CTLF13 = '\uF161'; // control-F13
	public static final char KEY_CTLF14 = '\uF162'; // control-F14
	public static final char KEY_CTLF15 = '\uF163'; // control-F15
	public static final char KEY_CTLF16 = '\uF164'; // control-F16
	public static final char KEY_CTLF17 = '\uF165'; // control-F17
	public static final char KEY_CTLF18 = '\uF166'; // control-F18
	public static final char KEY_CTLF19 = '\uF167'; // control-F19
	public static final char KEY_CTLF20 = '\uF168'; // control-F20

	public static final char KEY_ALTF1 = '\uF169'; // alt-F1
	public static final char KEY_ALTF2 = '\uF16A'; // alt-F2
	public static final char KEY_ALTF3 = '\uF16B'; // alt-F3
	public static final char KEY_ALTF4 = '\uF16C'; // alt-F4
	public static final char KEY_ALTF5 = '\uF16D'; // alt-F5
	public static final char KEY_ALTF6 = '\uF16E'; // alt-F6
	public static final char KEY_ALTF7 = '\uF16F'; // alt-F7
	public static final char KEY_ALTF8 = '\uF170'; // alt-F8
	public static final char KEY_ALTF9 = '\uF171'; // alt-F9
	public static final char KEY_ALTF10 = '\uF172'; // alt-F10
	public static final char KEY_ALTF11 = '\uF173'; // alt-F11
	public static final char KEY_ALTF12 = '\uF174'; // alt-F12
	public static final char KEY_ALTF13 = '\uF175'; // alt-F13
	public static final char KEY_ALTF14 = '\uF176'; // alt-F14
	public static final char KEY_ALTF15 = '\uF177'; // alt-F15
	public static final char KEY_ALTF16 = '\uF178'; // alt-F16
	public static final char KEY_ALTF17 = '\uF179'; // alt-F17
	public static final char KEY_ALTF18 = '\uF17A'; // alt-F18
	public static final char KEY_ALTF19 = '\uF17B'; // alt-F19
	public static final char KEY_ALTF20 = '\uF17C'; // alt-F20

	public static final char KEY_ALTA = '\uF17D'; // alt-A
	public static final char KEY_ALTB = '\uF17E'; // alt-B
	public static final char KEY_ALTC = '\uF17F'; // alt-C
	public static final char KEY_ALTD = '\uF180'; // alt-D
	public static final char KEY_ALTE = '\uF181'; // alt-E
	public static final char KEY_ALTF = '\uF182'; // alt-F
	public static final char KEY_ALTG = '\uF183'; // alt-G
	public static final char KEY_ALTH = '\uF184'; // alt-H
	public static final char KEY_ALTI = '\uF185'; // alt-I
	public static final char KEY_ALTJ = '\uF186'; // alt-J
	public static final char KEY_ALTK = '\uF187'; // alt-K
	public static final char KEY_ALTL = '\uF188'; // alt-L
	public static final char KEY_ALTM = '\uF189'; // alt-M
	public static final char KEY_ALTN = '\uF18A'; // alt-N
	public static final char KEY_ALTO = '\uF18B'; // alt-O
	public static final char KEY_ALTP = '\uF18C'; // alt-P
	public static final char KEY_ALTQ = '\uF18D'; // alt-Q
	public static final char KEY_ALTR = '\uF18E'; // alt-R
	public static final char KEY_ALTS = '\uF18F'; // alt-S
	public static final char KEY_ALTT = '\uF190'; // alt-T
	public static final char KEY_ALTU = '\uF191'; // alt-U
	public static final char KEY_ALTV = '\uF192'; // alt-V
	public static final char KEY_ALTW = '\uF193'; // alt-W
	public static final char KEY_ALTX = '\uF194'; // alt-X
	public static final char KEY_ALTY = '\uF195'; // alt-Y
	public static final char KEY_ALTZ = '\uF196'; // alt-Z

	public static char KEY_INTERRUPT = '\uF1F8'; // interrupt
	public static char KEY_CANCEL = '\uF1F9'; // cancel
	public static final char KEY_TIMEOUT = '\uFFFF'; // timeout
	public static final char KEY_ALL = '\uFFFF'; // clear endkey all

	public boolean actionpending; // trap, interrupt or break is pending

	protected static final int MOVE_UP = 1;
	protected static final int MOVE_DOWN = 2;
	protected static final int MOVE_LEFT = 3;
	protected static final int MOVE_RIGHT = 4;

	protected static final int DSPATTR_BOLD        = 0x00000100;
	protected static final int DSPATTR_BLINK       = 0x00000200;
	protected static final int DSPATTR_UNDERLINE   = 0x00000400;
	protected static final int DSPATTR_REVCOLOR    = 0x00001000;
	protected static final int DSPATTR_FLOWDOWN    = 0x00002000;
	protected static final int DSPATTR_FGCOLORMASK = 0x000000FF;
	protected static final int DSPATTR_BGCOLORMASK = 0x00FF0000;
	protected static final int DSPATTR_FGCOLORSHIFT = 0;
	protected static final int DSPATTR_BGCOLORSHIFT = 16;

	protected static final int DSPATTR_BLACK = 0x00;
//	protected static final int DSPATTR_BLUE = 0x01;
//	protected static final int DSPATTR_GREEN = 0x02;
//	protected static final int DSPATTR_CYAN = 0x03;
//	protected static final int DSPATTR_RED = 0x04;
//	protected static final int DSPATTR_MAGENTA = 0x05;
//	protected static final int DSPATTR_YELLOW = 0x06;
	protected static final int DSPATTR_WHITE = 0x07;
	protected static final int DSPATTR_INTENSE = 0x08; // or this with a color

	protected static final int KYNATTR_EDIT = 0x00000001;
	protected static final int KYNATTR_EDITON = 0x00000002;
	protected static final int KYNATTR_DIGITENTRY = 0x00000008;
	protected static final int KYNATTR_JUSTRIGHT = 0x00000010;
	protected static final int KYNATTR_JUSTLEFT = 0x00000020;
	protected static final int KYNATTR_ZEROFILL = 0x00000040;
	protected static final int KYNATTR_AUTOENTER = 0x00000080;
	protected static final int KYNATTR_NOECHO = 0x00000100;
	protected static final int KYNATTR_ECHOSECRET = 0x00000200;
	protected static final int KYNATTR_UPPERCASE = 0x00010000;
	protected static final int KYNATTR_LOWERCASE = 0x00020000;
	protected static final int KYNATTR_INVERTCASE = 0x00040000;
	protected static final int KYNATTR_TIMEOUT = 0x00100000;
	protected static final int KYNATTR_DCOMMA = 0x00200000;

	protected static final int CURSORSTATE_NORM = 0;
	protected static final int CURSORSTATE_ON = 1;
	protected static final int CURSORSTATE_OFF = 2;

	protected static final int CURSOR_NONE = 0;
	protected static final int CURSOR_BLOCK = 1;
	protected static final int CURSOR_ULINE = 2;
	protected static final int CURSOR_HALF = 3;

	private static final int STATESIZE = 12;
	private static final int KEYINBUFFERSIZE = 160;
	private static final int MAXTRAPS = 128;
	private static final int MAXPENDINGTRAPS = 16;
	private static final int MAXENDKEYS = 128;

	public boolean interrupted = false;
	private boolean keyinupcase; // FLAG_KEYINUPPER was set
	private boolean keyinrevcase; // FLAG_KEYINREVERSE was set
	private int dspattr; // current display attribute - DSPATTR_xxx
	private int kynattr; // current keyin attribute - KYNATTR_xxx
	private int cursorstate; // cursor state - one of CURSORSTATE_xxx
	private int cursorshape; // if displayed, cursor shape - one of CURSOR_xxx,
								// but not _NONE
	private int hsize; // screen horizontal size
	private int vsize; // screen vertical size
	private int h; // cursor horz pos, zero based, screen relative
	private int v; // cursor vert pos, zero based, screen relative
	private int top; // current window top, zero based
	private int bottom; // current window bottom, zero based
	private int left; // current window left, zero based
	private int right; // current window right, zero based
	private int kllength; // length of *kl field
	private char eschar; // current echosecret character
	private char[] dspline; // working display line
	private char[] keyinchars; // working characters in keyin
	private char[] keyinbuffer; // keyin circular buffer
	private int bufferpos; // offset of oldest character in buffer
	private int buffernext; // offset of where to put next character
	private long timeout; // keyin timeout in milliseconds
	private char lastendkey; // last keyin end key
	private char[] endkeys; // end keys
	private int endkeysmaxuse; // index of highest used endkey (-1 if none)
	private boolean trapall; // trap all character and function keys
	private boolean trapfkeys; // trap all function keys
	private boolean trapchars; // trap all character keys
	private boolean trapallnoreset; // do not clear the trap all when a trap
									// occurs
	private boolean trapfkeysnoreset; // do not clear the trap all function keys
										// when a trap occurs
	private boolean trapcharsnoreset; // do not clear the trap all character
										// keys when a trap occurs
	private char[] trapchar; // trap character, '\uFFFF' if unused
	private boolean[] trapnoreset; // do not clear the trap when it occurs
	private int trapmaxuse; // index of highest used trap (-1 if none)
	private char[] pendingtraps; // array of activated traps ('\uFFFF' value is
									// end of trap chars)
	private boolean interruptpending; // interrupt occurred
	private boolean breakpending; // break pending

	private KeyinDisplayAWT awt; // instance of support routines for awt
	// private basedebug basedbg = null; // instance of debugger server
	private boolean ansi256ColorMode;
	private final boolean debug;

	/**
	 * Constructor
	 */
	public KeyinDisplay(int flag, int h, int v, String font, int fontsize,
			int charwidth, int charheight, String tdbfilename, String indevice, String outdevice)
	{
		this();
		if (debug) {
			System.out.println("Entering KeyinDisplay.<>");
		}
		kdssetup(flag, h, v, font, fontsize, charwidth, charheight, tdbfilename, indevice, outdevice);
	}

	/**
	 * Constructor
	 */
	private KeyinDisplay() {
		keyinbuffer = new char[KEYINBUFFERSIZE];
		trapchar = new char[MAXTRAPS];
		trapnoreset = new boolean[MAXTRAPS];
		trapmaxuse = -1;
		pendingtraps = new char[MAXPENDINGTRAPS];
		pendingtraps[0] = '\uFFFF';
		endkeys = new char[MAXENDKEYS];
		endkeys[0] = '\uF100';
		endkeysmaxuse = 0;
		lastendkey = 0;
		clearkeyahead();
		debug = SmartClientApplication.isDebug();
	}

	private void kdssetup(int flag, int hsize, int vsize, String font, int fontsize,
			int charwidth, int charheight, String tdbfilename, String indevice, String outdevice)
	{
		if (debug) {
			System.out.println("Entering KeyinDisplay.kdssetup");
		}
		keyinupcase = keyinrevcase = false;
		if ((flag & FLAG_KEYINUPCASE) != 0)
			keyinupcase = true;
		else if ((flag & FLAG_KEYINREVCASE) != 0)
			keyinrevcase = true;
		if ((flag & FLAG_DISPLAYCONSOLE) == 0 && (flag & FLAG_DISPLAYTERMINAL) == 0) {
			awt = new KeyinDisplayAWT(this, hsize, vsize, font, fontsize, charwidth, charheight);
			if ((flag & FLAG_CONSOLENOCLOSE) != 0)
				awt.setNoclose();
			this.hsize = awt.kdsgethsize();
			this.vsize = awt.kdsgetvsize();
		}
		keyinchars = new char[this.hsize];
		right = this.hsize - 1;
		bottom = this.vsize - 1;
		dspline = new char[this.hsize];
		dspattr = (DSPATTR_WHITE << DSPATTR_FGCOLORSHIFT) | (DSPATTR_BLACK << DSPATTR_BGCOLORSHIFT);
		eschar = '*';
		cursorstate = CURSORSTATE_NORM;
		cursorshape = CURSOR_BLOCK;
		if (awt != null)
			awt.kdscursor(CURSOR_NONE);
	}

	// public synchronized void interrupt(basedebug bd) {
	// basedbg = bd;
	// notifyAll();
	// }

	public final void requestFocus() {
		if (awt != null)
			awt.requestFocus();
	}

	/**
	 * Destroy the window and/or restore the screen and keyboard state
	 */
	public final void close() {
		if (awt != null)
			awt.kdsexit();
	}

	public final static void setCancelkey(char key) {
		KEY_CANCEL = key;
	}

	public final static void setInterruptkey(char key) {
		KEY_INTERRUPT = key;
	}

	/**
	 * Flush the output. Return this kdswin.
	 */
	public final void flush() {
		if (awt != null)
			awt.kdsflush();
	}

	/**
	 * Display the string. Return this kdswin.
	 */
	public final KeyinDisplay display(String s1) {
		int i1;
		if (s1 == null)
			return this;
		int len = s1.length();
		if (len == 0)
			return this;
		if ((dspattr & DSPATTR_FLOWDOWN) != 0)
			i1 = bottom - v + 1;
		else
			i1 = right - h + 1;
		if (len > i1)
			len = i1;
		s1.getChars(0, len, dspline, 0);
		if ((dspattr & DSPATTR_FLOWDOWN) != 0) {
			putrect(dspline, 0, len, true, v, v + len - 1, h, h, dspattr);
			v += len;
			if (v > bottom)
				v = bottom;
		} else {
			putrect(dspline, 0, len, true, v, v, h, h + len - 1, dspattr);
			h += len;
			if (h > right)
				h = right;
		}
		if (cursorstate != CURSORSTATE_OFF)
			sethv(h, v);
		return this;
	}

	/**
	 * Display an array of characters. Return this kdswin.
	 */
	public final KeyinDisplay display(char[] ca1, int start, int length) {
		int i1;
		if (ca1 != null && length > 0 && start >= 0 && start < ca1.length) {
			if ((dspattr & DSPATTR_FLOWDOWN) != 0) {
				i1 = bottom - v + 1;
				if (length > i1)
					length = i1;
				putrect(ca1, start, length, true, v, bottom, h, h, dspattr);
				v += length;
				if (v > bottom)
					v = bottom;
			} else {
				i1 = right - h + 1;
				if (length > i1)
					length = i1;
				putrect(ca1, start, length, true, v, v, h, right, dspattr);
				h += length;
				if (h > right)
					h = right;
			}
		}
		return this;
	}

	/**
	 * Display an array of characters, followed by some blanks. Return this
	 * kdswin.
	 */
	public final KeyinDisplay display(char[] ca1, int start, int length, int blanks) {
		int i1;
		if (ca1 != null && length > 0 && start >= 0 && start < ca1.length) {
			if ((dspattr & DSPATTR_FLOWDOWN) != 0) {
				i1 = bottom - v + 1;
				if (length > i1)
					length = i1;
				putrect(ca1, start, length, true, v, bottom, h, h, dspattr);
				v += length;
				if (v > bottom)
					v = bottom;
				else {
					i1 = bottom - v + 1;
					if (blanks > i1)
						blanks = i1;
					for (i1 = 0; i1 < blanks; dspline[i1++] = ' ')
						;
					putrect(dspline, 0, blanks, true, v, bottom, h, h, dspattr);
					v += blanks;
					if (v > bottom)
						v = bottom;
				}
			} else {
				i1 = right - h + 1;
				if (length > i1)
					length = i1;
				putrect(ca1, start, length, true, v, v, h, right, dspattr);
				h += length;
				if (h > right)
					h = right;
				else {
					i1 = right - h + 1;
					if (blanks > i1)
						blanks = i1;
					for (i1 = 0; i1 < blanks; dspline[i1++] = ' ')
						;
					putrect(dspline, 0, blanks, true, v, v, h, right, dspattr);
					h += blanks;
					if (h > right)
						h = right;
				}
			}
			if (cursorstate != CURSORSTATE_OFF)
				sethv(h, v);
		}
		return this;
	}

	/**
	 * Display a character. Return this kdswin.
	 */
	public final KeyinDisplay display(char c1) {
		dspline[0] = c1;
		putrect(dspline, 0, 1, true, v, v, h, h, dspattr);
		if (h < right)
			h++;
		if (cursorstate != CURSORSTATE_OFF)
			sethv(h, v);
		return this;
	}

	/**
	 * Display a character n1 times. Return this kdswin.
	 */
	public final KeyinDisplay display(char c1, int n1) {
		int i1;
		if (n1 < 1)
			return this;
		if ((dspattr & DSPATTR_FLOWDOWN) != 0) {
			i1 = bottom - v + 1;
			if (n1 > i1)
				n1 = i1;
			for (i1 = 0; i1 < n1; dspline[i1++] = c1)
				;
			putrect(dspline, 0, n1, true, v, bottom, h, h, dspattr);
			v += n1;
			if (v > bottom)
				v = bottom;
		} else {
			i1 = right - h + 1;
			if (n1 > i1)
				n1 = i1;
			for (i1 = 0; i1 < n1; dspline[i1++] = c1)
				;
			putrect(dspline, 0, n1, true, v, v, h, right, dspattr);
			h += n1;
			if (h > right)
				h = right;
		}
		if (cursorstate != CURSORSTATE_OFF)
			sethv(h, v);
		return this;
	}

	/**
	 * Set a keyin end key.
	 */
	public final void setendkey(char chr) {
		int i1;
		if (chr == '\uFFFF')
			return;
		for (i1 = 0; i1 <= endkeysmaxuse; i1++) {
			if (endkeys[i1] == '\uFFFF') {
				endkeys[i1] = chr;
				return;
			}
		}
		if (i1 < MAXENDKEYS) {
			endkeysmaxuse = i1;
			endkeys[i1] = chr;
		}
	}

	/**
	 * Clear a keyin end key. If key is KEY_ALL then clear all ending keys, but
	 * set '\uF100' (ENTER).
	 */
	public final void clearendkey(char chr) {
		int i1;
		if (chr == KEY_ALL) {
			endkeys[0] = KEY_ENTER;
			endkeysmaxuse = 0;
			return;
		}
		for (i1 = 0; i1 <= endkeysmaxuse; i1++) {
			if (endkeys[i1] == chr) {
				endkeys[i1] = '\uFFFF';
				break;
			}
		}
		for (i1 = endkeysmaxuse; i1 >= 0 && endkeys[i1] == '\uFFFF'; i1--)
			;
		endkeysmaxuse = i1;
	}

	/**
	 * Get the last end key.
	 */
	public final char getendkey() {
		return lastendkey;
	}

	/**
	 * Set a trap key. If key is -1, then trap on all keys. If key is -2, then
	 * trap on character keys. If key is -3, then trap on function keys.
	 * Otherwise key is the charcter trap to set. id is a value provided and
	 * used by caller.
	 */
	public final synchronized void settrapkey(int key, boolean noreset) {
		int i1, i2;
		char c1;
		if (key == -1) {
			trapall = true;
			trapallnoreset = noreset;
			trapfkeys = trapchars = false;
			trapmaxuse = -1;
		} else if (key == -2) {
			trapchars = true;
			trapcharsnoreset = noreset;
			for (i1 = 0; i1 <= trapmaxuse; i1++) {
				if (trapchar[i1] < KEY_ENTER)
					trapchar[i1] = '\uFFFF';
			}
			trapall = false;
		} else if (key == -3) {
			trapfkeys = true;
			trapfkeysnoreset = noreset;
			for (i1 = 0; i1 <= trapmaxuse; i1++) {
				if (trapchar[i1] >= KEY_ENTER)
					trapchar[i1] = '\uFFFF';
			}
			trapall = false;
		} else if (key >= 0) {
			c1 = Character.toUpperCase((char) key);
			for (i1 = 0, i2 = -1; i1 <= trapmaxuse; i1++) {
				if (trapchar[i1] == c1)
					break; // i1 is the index of same char
				if (i2 == -1 && trapchar[i1] == '\uFFFF')
					i2 = i1; // i2 is the first empty one
			}
			if (i1 > trapmaxuse) {
				if (i2 == -1) {
					if (trapmaxuse == MAXTRAPS - 1)
						return;
					i1 = ++trapmaxuse;
				} else
					i1 = i2;
			}
			trapchar[i1] = c1;
			trapnoreset[i1] = noreset;
		}
		i1 = bufferpos;
		i2 = buffernext;
		if (pendingtraps[0] == '\uFFFF') {
			while (i1 != i2) {
				c1 = keyinbuffer[i1++];
				if (i1 == KEYINBUFFERSIZE)
					i1 = 0;
				if (checktrapkey(c1))
					bufferpos = buffernext = i1 = i2 = 0;
			}
		}
	}

	/**
	 * Clear a trap key. If key is -1, then clear traps on all keys. If key is
	 * -2, then clear traps on character keys. If key is -3, then clear traps on
	 * function keys. If key is >= 0, then clear that character trap.
	 */
	public final synchronized void cleartrapkey(int key) {
		int i1;
		if (key == -1) {
			trapall = trapfkeys = trapchars = false;
			trapmaxuse = -1;
			return;
		}
		if (key == -2) {
			if (trapall && !trapfkeys) {
				trapall = false;
				trapfkeys = true;
			}
			trapchars = false;
			for (i1 = 0; i1 <= trapmaxuse; i1++) {
				if (trapchar[i1] < KEY_ENTER)
					trapchar[i1] = '\uFFFF';
			}
		} else if (key == -3) {
			if (trapall && !trapchars) {
				trapall = false;
				trapchars = true;
			}
			trapfkeys = false;
			for (i1 = 0; i1 <= trapmaxuse; i1++) {
				if (trapchar[i1] >= KEY_ENTER)
					trapchar[i1] = '\uFFFF';
			}
		} else if (key >= 0) {
			char c1 = Character.toUpperCase((char) key);
			for (i1 = 0; i1 <= trapmaxuse; i1++) {
				if (trapchar[i1] == c1) {
					trapchar[i1] = '\uFFFF';
					break;
				}
			}
		}
		for (i1 = trapmaxuse; i1 >= 0 && trapchar[i1] == '\uFFFF'; i1--)
			;
		trapmaxuse = i1;
	}

	/**
	 * Get the action id. If break or interrupt occurred, destroy all pending
	 * traps. Return -1 if break, -2 if interrupt or the key value of the oldest
	 * trap that occured.
	 */
	public final int getaction() {
		if (!actionpending)
			return 0;
		if (breakpending) {
			pendingtraps[0] = '\uFFFF';
			actionpending = interruptpending = breakpending = false;
			return -1;
		}
		if (interruptpending) {
			pendingtraps[0] = '\uFFFF';
			actionpending = interruptpending = false;
			return -2;
		}
		int trapchar = pendingtraps[0];
		for (int i1 = 0; i1 < MAXPENDINGTRAPS - 1; i1++) {
			pendingtraps[i1] = pendingtraps[i1 + 1];
			if (pendingtraps[i1] == '\uFFFF')
				break;
		}
		if (pendingtraps[0] == '\uFFFF')
			actionpending = false;
		return trapchar;
	}

	/**
	 * Check the character to see if it is a trap key. If it is a trap key, move
	 * it to pendingtraps and set actionpending. Return true if it is a trap
	 * key, else false.
	 */
	private synchronized boolean checktrapkey(char c1) {
		int i1;
		boolean trapfound = false;
		c1 = Character.toUpperCase(c1);
		if (trapall) {
			trapfound = true;
			if (!trapallnoreset) {
				trapall = trapfkeys = trapchars = false;
				trapmaxuse = -1;
			}
		} else if (c1 < '\uF000') {
			if (trapchars) {
				trapfound = true;
				if (!trapcharsnoreset)
					cleartrapkey(-2);
			}
		} else if (trapfkeys) {
			trapfound = true;
			if (!trapfkeysnoreset)
				cleartrapkey(-3);
		}
		if (!trapfound) {
			for (i1 = 0; i1 <= trapmaxuse; i1++) {
				if (c1 == trapchar[i1]) {
					if (!trapnoreset[i1]) {
						trapchar[i1] = '\uFFFF';
						if (i1 == trapmaxuse)
							trapmaxuse--;
					}
					trapfound = true;
					break;
				}
			}
		}
		if (trapfound) {
			for (i1 = 0; i1 < MAXPENDINGTRAPS && pendingtraps[i1] != '\uFFFF'; i1++)
				;
			if (i1 < MAXPENDINGTRAPS) {
				pendingtraps[i1] = c1;
				pendingtraps[i1 + 1] = '\uFFFF';
				actionpending = true;
				Client.getDefault().queueKDAction();
				return true;
			}
		}
		return false;
	}

	/**
	 * Keyin a character string. Return the number of characters entered. Set
	 * endkey.
	 */
	public final int charkeyin(char[] dest, int initiallength) {
		return keyin(dest, false, dest.length, initiallength);
	}

	/**
	 * Keyin a numeric string. right is number of digits right of decimal place.
	 * Return the number of characters entered. Set endkey.
	 */
	public final int numkeyin(char[] dest, int length, int right) {
		return keyin(dest, true, length, right);
	}

	/**
	 * Clear keyahead buffer.
	 */
	public final synchronized void clearkeyahead() {
		bufferpos = buffernext = 0;
	}

	/**
	 * Newline. Return this kdswin.
	 */
	public final KeyinDisplay nl() {
		return nl(1);
	}

	/**
	 * Newline. Return this kdswin.
	 */
	public final KeyinDisplay nl(int n1) {
		if (n1 > 0) {
			h = left;
			for (v += n1; v > bottom; v--) {
				moverect(MOVE_UP, top, bottom, left, right);
				if (awt != null)
					awt.kdseraserect(bottom, bottom, left, right, dspattr);
			}
			sethv(h, v);
		}
		return this;
	}

	/**
	 * Set the horizontal and vertical cursor position. Return this kdswin.
	 */
	public final KeyinDisplay hv(int h1, int v1) {
		if (h1 < 1 || h1 > right - left + 1)
			return this;
		if (v1 < 1 || v1 > bottom - top + 1)
			return this;
		h = h1 + left - 1;
		v = v1 + top - 1;
		sethv(h, v);
		return this;
	}

	/**
	 * Set the horizontal cursor position. Return this kdswin.
	 */
	public final KeyinDisplay h(int h1) {
		if (h1 < 1 || h1 > right - left + 1)
			return this;
		h = h1 + left - 1;
		sethv(h, v);
		return this;
	}

	/**
	 * Set the vertical cursor position. Return this kdswin.
	 */
	public final KeyinDisplay v(int v1) {
		if (v1 < 1 || v1 > bottom - top + 1)
			return this;
		v = v1 + top - 1;
		sethv(h, v);
		return this;
	}

	/**
	 * Adjust the horizontal cursor position. Return this kdswin.
	 */
	public final KeyinDisplay ha(int ha1) {
		ha1 += h;
		if (ha1 < left || ha1 > right)
			return this;
		h = ha1;
		sethv(h, v);
		return this;
	}

	/**
	 * Adjust the vertical cursor position. Return this kdswin.
	 */
	public final KeyinDisplay va(int va1) {
		va1 += v;
		if (va1 < top || va1 > bottom)
			return this;
		v = va1;
		sethv(h, v);
		return this;
	}

	/**
	 * Set the window top and bottom boundaries. Return this kdswin.
	 */
	public final KeyinDisplay setswtb(int newtop, int newbottom) {
		if (newtop < 1 || newtop > vsize || newbottom < 1 || newbottom > vsize || newtop > newbottom)
			return this;
		top = newtop - 1;
		bottom = newbottom - 1;
		h = left;
		v = top;
		sethv(h, v);
		return this;
	}

	/**
	 * Set the window left and right boundaries. Return this kdswin.
	 */
	public final KeyinDisplay setswlr(int newleft, int newright) {
		if (newleft < 1 || newleft > hsize || newright < 1 || newright > hsize || newleft > newright)
			return this;
		left = newleft - 1;
		right = newright - 1;
		h = left;
		v = top;
		sethv(h, v);
		return this;
	}

	/**
	 * Reset the window boundaries. Return this kdswin.
	 */
	public final KeyinDisplay resetsw() {
		top = left = h = v = 0;
		bottom = vsize - 1;
		right = hsize - 1;
		sethv(h, v);
		return this;
	}

	/**
	 * Set the cursor position to the upper left corner of the window. Return
	 * this kdswin.
	 */
	public final KeyinDisplay homeup() {
		h = left;
		v = top;
		sethv(h, v);
		return this;
	}

	/**
	 * Set the cursor position to the lower left corner of the window. Return
	 * this kdswin.
	 */
	public final KeyinDisplay homedown() {
		h = left;
		v = bottom;
		sethv(h, v);
		return this;
	}

	/**
	 * Set the cursor position to the upper right corner of the window. Return
	 * this kdswin.
	 */
	public final KeyinDisplay endup() {
		h = right;
		v = top;
		sethv(h, v);
		return this;
	}

	/**
	 * Set the cursor position to the lower right corner of the window. Return
	 * this kdswin.
	 */
	public final KeyinDisplay enddown() {
		h = right;
		v = bottom;
		sethv(h, v);
		return this;
	}

	/**
	 * Set the window to display characters downward. Return this kdswin.
	 */
	public final KeyinDisplay flowdown() {
		dspattr |= DSPATTR_FLOWDOWN;
		return this;
	}

	/**
	 * Set the window to display characters to the right. Return this kdswin.
	 */
	public final KeyinDisplay flowright() {
		dspattr &= ~DSPATTR_FLOWDOWN;
		return this;
	}

	/**
	 * Erase the window. Return this kdswin.
	 */
	public final KeyinDisplay erase() {
		if (awt != null)
			awt.kdseraserect(top, bottom, left, right, dspattr);
		h = left;
		v = top;
		sethv(h, v);
		return this;
	}

	/**
	 * Erase from the current position to the end of the window. Return this
	 * kdswin.
	 */
	public final KeyinDisplay erasefrom() {
		eraseline();
		if (v < bottom) {
			if (awt != null)
				awt.kdseraserect(v + 1, bottom, left, right, dspattr);
		}
		return this;
	}

	/**
	 * Erase from the current position to the end of the line. Return this
	 * kdswin.
	 */
	public final KeyinDisplay eraseline() {
		if (awt != null)
			awt.kdseraserect(v, v, h, right, dspattr);
		return this;
	}

	/**
	 * Roll up the characters in the window. Return this kdswin.
	 */
	public final KeyinDisplay rollup() {
		moverect(MOVE_UP, top, bottom, left, right);
		if (awt != null)
			awt.kdseraserect(bottom, bottom, left, right, dspattr);
		return this;
	}

	/**
	 * Roll down the characters in the window. Return this kdswin.
	 */
	public final KeyinDisplay rolldown() {
		moverect(MOVE_DOWN, top, bottom, left, right);
		if (awt != null)
			awt.kdseraserect(top, top, left, right, dspattr);
		return this;
	}

	/**
	 * Roll left the characters in the window. Return this kdswin.
	 */
	public final KeyinDisplay rollleft() {
		moverect(MOVE_LEFT, top, bottom, left, right);
		if (awt != null)
			awt.kdseraserect(top, bottom, right, right, dspattr);
		return this;
	}

	/**
	 * Roll right the characters in the window. Return this kdswin.
	 */
	public final KeyinDisplay rollright() {
		moverect(MOVE_RIGHT, top, bottom, left, right);
		if (awt != null)
			awt.kdseraserect(top, bottom, left, left, dspattr);
		return this;
	}

	/**
	 * Open an edit line. Return this kdswin.
	 */
	public final KeyinDisplay openline() {
		moverect(MOVE_DOWN, v + 1, bottom, left, right);
		moverect(MOVE_DOWN, v, v + 1, h, right);
		return this;
	}

	/**
	 * Close an edit line. Return this kdswin.
	 */
	public final KeyinDisplay closeline() {
		moverect(MOVE_UP, v, v + 1, h, right);
		moverect(MOVE_UP, v + 1, bottom, left, right);
		return this;
	}

	/**
	 * Insert an edit line. Return this kdswin.
	 */
	public final KeyinDisplay insertline() {
		moverect(MOVE_DOWN, v, bottom, left, right);
		return this;
	}

	/**
	 * Delete a line. Return this kdswin.
	 */
	public final KeyinDisplay deleteline() {
		moverect(MOVE_UP, v, bottom, left, right);
		return this;
	}

	/**
	 * Insert a character at position h1, v1. Return this kdswin.
	 */
	public final KeyinDisplay insertchar(int h1, int v1) {
		int i1, i2;
		if (h1 < 1 || h1 > right - left + 1)
			return this;
		if (v1 < 1 || v1 > bottom - top + 1)
			return this;
		h1 = h1 + left - 1;
		v1 = v1 + top - 1;
		if (v1 < v || (v1 == v && h1 < h))
			return this;
		char[] ca1 = new char[(right - left + 1) * (v1 - v + 1) * 2];
		char[] ca2 = new char[ca1.length];
		getrect(ca1, false, v, v1, left, right);
		for (i1 = 0; i1 < ca1.length; i1++)
			ca2[i1] = ca1[i1];
		i1 = (h - left) * 2;
		i2 = ((v1 - v + 1) * (right - left + 1) - (h - left) - (right - h1)) * 2;
		i2 += i1;
		while (i1 < i2 - 2) {
			ca2[i1 + 2] = ca1[i1];
			ca2[i1 + 3] = ca1[i1 + 1];
			i1 += 2;
		}
		putrect(ca2, 0, ca2.length, false, v, v1, left, right, dspattr);
		ca2[0] = ' ';
		putrect(ca2, 0, 1, true, v, v, h, h, dspattr);
		return this;
	}

	/**
	 * Delete a character at position h1, v1. Return this kdswin.
	 */
	public final KeyinDisplay deletechar(int h1, int v1) {
		int i1, i2;
		if (h1 < 1 || h1 > right - left + 1)
			return this;
		if (v1 < 1 || v1 > bottom - top + 1)
			return this;
		h1 = h1 + left - 1;
		v1 = v1 + top - 1;
		if (v1 < v || (v1 == v && h1 < h))
			return this;
		char[] ca1 = new char[(right - left + 1) * (v1 - v + 1) * 2];
		getrect(ca1, false, v, v1, left, right);
		i1 = (h - left) * 2;
		i2 = ((v1 - v + 1) * (right - left + 1) - (h - left) - (right - h1)) * 2;
		i2 += i1;
		while (i1 < i2 - 2) {
			ca1[i1] = ca1[i1 + 2];
			ca1[i1 + 1] = ca1[i1 + 3];
			i1 += 2;
		}
		putrect(ca1, 0, ca1.length, false, v, v1, left, right, dspattr);
		ca1[0] = ' ';
		putrect(ca1, 0, 1, true, v1, v1, h1, h1, dspattr);
		return this;
	}

	/**
	 * Set the foreground color. Return this kdswin.
	 */
	public final KeyinDisplay setcolor(int color) {
		dspattr &= ~DSPATTR_FGCOLORMASK;
		dspattr |= (color & 0xFF) << DSPATTR_FGCOLORSHIFT;
		return this;
	}

	public final KeyinDisplay setcolor(String name) {
		return setcolor(DXColor.getColorCode(name));
	}
	
	/**
	 * Set the background color. Return this kdswin.
	 */
	public final KeyinDisplay setbgcolor(int color) {
		dspattr &= ~DSPATTR_BGCOLORMASK;
		dspattr |= (color & 0xFF) << DSPATTR_BGCOLORSHIFT;
		return this;
	}

	public final KeyinDisplay setbgcolor(String name) {
		return setbgcolor(DXColor.getColorCode(name));
	}
	
	/**
	 * Turn on reverse video. Return this kdswin.
	 */
	public final KeyinDisplay revon() {
		dspattr |= DSPATTR_REVCOLOR;
		return this;
	}

	/**
	 * Turn off reverse video. Return this kdswin.
	 */
	public final KeyinDisplay revoff() {
		dspattr &= ~DSPATTR_REVCOLOR;
		return this;
	}

	/**
	 * Turn on bold display. Return this kdswin.
	 */
	public final KeyinDisplay boldon() {
		dspattr |= DSPATTR_BOLD;
		return this;
	}

	/**
	 * Turn off bold display. Return this kdswin.
	 */
	public final KeyinDisplay boldoff() {
		dspattr &= ~DSPATTR_BOLD;
		return this;
	}

	/**
	 * Turn on underline display. Return this kdswin.
	 */
	public final KeyinDisplay underlineon() {
		dspattr |= DSPATTR_UNDERLINE;
		return this;
	}

	/**
	 * Turn off underline display. Return this kdswin.
	 */
	public final KeyinDisplay underlineoff() {
		dspattr &= ~DSPATTR_UNDERLINE;
		return this;
	}

	/**
	 * Turn on blinking display. Return this kdswin.
	 */
	public final KeyinDisplay blinkon() {
		dspattr |= DSPATTR_BLINK;
		return this;
	}

	/**
	 * Turn off blinking display. Return this kdswin.
	 */
	public final KeyinDisplay blinkoff() {
		dspattr &= ~DSPATTR_BLINK;
		return this;
	}

	/**
	 * Turn off reverse, bold, underline and blinking display. Return this
	 * kdswin.
	 */
	public final KeyinDisplay alloff() {
		dspattr &= ~(DSPATTR_REVCOLOR | DSPATTR_BOLD | DSPATTR_UNDERLINE | DSPATTR_BLINK);
		return this;
	}

	/**
	 * Set the cursor display to display normally. Return this kdswin.
	 */
	public final KeyinDisplay cursornorm() {
		cursorstate = CURSORSTATE_NORM;
		setcursor(CURSOR_NONE);
		return this;
	}

	/**
	 * Set the cursor display to always display. Return this kdswin.
	 */
	public final KeyinDisplay cursoron() {
		cursorstate = CURSORSTATE_ON;
		setcursor(CURSOR_BLOCK);
		return this;
	}

	/**
	 * Set the cursor display to never display. Return this kdswin.
	 */
	public final KeyinDisplay cursoroff() {
		cursorstate = CURSORSTATE_OFF;
		setcursor(CURSOR_NONE);
		return this;
	}

	/**
	 * Set the cursor shape to block. Return this kdswin.
	 */
	public final KeyinDisplay cursorblock() {
		cursorshape = CURSOR_BLOCK;
		if (cursorstate == CURSORSTATE_ON)
			setcursor(CURSOR_BLOCK);
		return this;
	}

	/**
	 * Set the cursor shape to half. Return this kdswin.
	 */
	public final KeyinDisplay cursorhalf() {
		cursorshape = CURSOR_HALF;
		if (cursorstate == CURSORSTATE_ON)
			setcursor(CURSOR_HALF);
		return this;
	}

	/**
	 * Set the cursor shape to underline. Return this kdswin.
	 */
	public final KeyinDisplay cursoruline() {
		cursorshape = CURSOR_ULINE;
		if (cursorstate == CURSORSTATE_ON)
			setcursor(CURSOR_ULINE);
		return this;
	}

	/**
	 * Return the current cursor horizontal position.
	 */
	public final int geth() {
		return h - left + 1;
	}

	/**
	 * Return the current cursor vertical position. public final int getv()
	 */
	public final int getv() {
		return v - top + 1;
	}

	/**
	 * Return the window top boundary.
	 */
	public final int getwintop() {
		return top + 1;
	}

	/**
	 * Return the window bottom boundary.
	 */
	public final int getwinbottom() {
		return bottom + 1;
	}

	/**
	 * Return the window left boundary.
	 */
	public final int getwinleft() {
		return left + 1;
	}

	/**
	 * Return the window right boundary.
	 */
	public final int getwinright() {
		return right + 1;
	}

	/**
	 * Save the characters in the current window. If ca1 is null, don't move any
	 * characters. Return the number of characters needed to store the data.
	 */
	public final int charsave(char[] ca1) {
		if (ca1 != null)
			getrect(ca1, true, top, bottom, left, right);
		return (bottom - top + 1) * (right - left + 1);
	}

	/**
	 * Restore the characters into the current window.
	 */
	public final void charrestore(char[] ca1, int start, int length) {
		if (ca1 != null && length > 0 && start >= 0 && start < ca1.length) {
			putrect(ca1, start, length, true, top, bottom, left, right, dspattr);
		}
	}

	/**
	 * Restore the characters into the current window.
	 */
	public final void charrestore(String s1) {
		if (s1 != null)
			putrect(s1.toCharArray(), 0, s1.length() - 1, true, top, bottom, left, right, dspattr);
	}

	/**
	 * Return the number of characters that winsave needs to store the window
	 * data.
	 */
	public final int winsize() {
		return (bottom - top + 1) * (right - left + 1) * 2;
	}

	/**
	 * Save the characters and attributes in the current window. If ca1 is null,
	 * don't move any characters. Return the number of characters needed to
	 * store the data.
	 */
	public final int winsave(char[] ca1) {
		if (ca1 != null)
			getrect(ca1, false, top, bottom, left, right);
		return (bottom - top + 1) * (right - left + 1) * 2;
	}

	/**
	 * Restore the characters and attributes into the current window. length is
	 * the number of characters to use from ca1.
	 */
	public final void winrestore(char[] ca1, int length) {
		if (ca1 != null && length > 1) {
			length = length / 2 * 2;
			putrect(ca1, 0, length, false, top, bottom, left, right, dspattr);
		}
	}

	/**
	 * Return the number of characters that screensave needs to store the screen
	 * data.
	 */
	public final int screensize() {
		return hsize * vsize * 2 + STATESIZE;
	}

	/**
	 * Save the kdswin state and all characters and attributes. If ca1 is null,
	 * don't move any characters. Return the number of characters needed to
	 * store the data.
	 */
	public final int screensave(char[] ca1) {
		int i1 = hsize * vsize * 2;
		if (ca1 != null) {
			getrect(ca1, false, 0, vsize - 1, 0, hsize - 1);
			statesave1(ca1, i1);
		}
		return i1 + STATESIZE;
	}

	/**
	 * Restore the kdswin state and all characters and attributes. length is the
	 * number of characters to use from ca1.
	 */
	public final boolean screenrestore(char[] ca1, int length) {
		if (ca1 == null)
			return false;
		putrect(ca1, 0, length, false, 0, vsize - 1, 0, hsize - 1, dspattr);
		int i1 = hsize * vsize * 2;
		if (length < i1 + STATESIZE)
			return false;
		return staterestore1(ca1, i1);
	}

	/**
	 * Return the number of characters that statesave needs to store the state
	 * data.
	 */
	public final static int statesize() {
		return STATESIZE;
	}

	/**
	 * Save the kdswin state. If ca1 is null, don't move any characters. Return
	 * the number of characters needed to store the data.
	 */
	public final int statesave(char[] ca1) {
		statesave1(ca1, 0);
		return STATESIZE;
	}

	/**
	 * Restore the kdswin state. length is the number of characters to use from
	 * ca1.
	 */
	public final boolean staterestore(char[] ca1, int length) {
		if (ca1 == null || length < STATESIZE)
			return false;
		return staterestore1(ca1, 0);
	}

	/**
	 * Set the horizontal and vertical cursor position.
	 */
	private void sethv(int h1, int v1) {
		if (awt != null)
			awt.kdspos(h, v);
		return;
	}

	/**
	 * Set the cursor visibility
	 */
	private void setcursor(int type) {
		if (awt != null)
			awt.kdscursor(type);
	}

	/**
	 * Get characters into a rectange on the screen
	 */
	private void getrect(char[] ca1, boolean charsonly, int top, int bottom, int left, int right) {
		if (awt != null)
			awt.kdsgetrect(ca1, charsonly, top, bottom, left, right);
		return;
	}

	/**
	 * Put characters from a rectange on the screen
	 */
	private void putrect(char[] ca1, int start, int length, boolean charsonly,
			int top, int bottom, int left, int right, int dspattr) {
		if (awt != null)
			awt.kdsputrect(ca1, start, length, charsonly, top, bottom, left, right, dspattr);
		return;
	}

	/**
	 * Move a rectange of characters on the screen
	 */
	private void moverect(int direction, int top, int bottom, int left, int right) {
		if (awt != null)
			awt.kdsmoverect(direction, top, bottom, left, right);
	}

	private void statesave1(char[] ca1, int start) {
		if (start + STATESIZE > ca1.length)
			return;
		int i1 = start;
		ca1[i1++] = (char) h;
		ca1[i1++] = (char) v;
		ca1[i1++] = (char) top;
		ca1[i1++] = (char) bottom;
		ca1[i1++] = (char) left;
		ca1[i1++] = (char) right;
		ca1[i1++] = (char) dspattr;
		ca1[i1++] = (char) cursorstate;
		ca1[i1++] = (char) cursorshape;
		ca1[i1++] = '\u0DBC';
		ca1[i1++] = '\u0DBC';
		ca1[i1++] = '\u0DBC';
	}

	private boolean staterestore1(char[] ca1, int start) {
		if (start + STATESIZE > ca1.length)
			return false;
		if (ca1[start + STATESIZE - 1] != '\u0DBC')
			return false;
		int i1 = start;
		h = ca1[i1++];
		v = ca1[i1++];
		top = ca1[i1++];
		bottom = ca1[i1++];
		left = ca1[i1++];
		right = ca1[i1++];
		dspattr = ca1[i1++];
		cursorstate = ca1[i1++];
		cursorshape = ca1[i1++];
		sethv(h, v);
		if (cursorstate == CURSORSTATE_ON)
			setcursor(cursorshape);
		else
			setcursor(CURSOR_NONE);
		return true;
	}

	/**
	 * Beep.
	 */
	public final void beep() {
		if (awt != null)
			awt.kdsbeep();
	}

	/**
	 * Turn on display and keyin of a comma instead of a period for numeric
	 * values. Return this kdswin.
	 */
	public final KeyinDisplay dcon() {
		kynattr |= KYNATTR_DCOMMA;
		return this;
	}

	/**
	 * Turn off dcon for display and keyin. Return this kdswin.
	 */
	public final KeyinDisplay dcoff() {
		kynattr &= ~KYNATTR_DCOMMA;
		return this;
	}

	/**
	 * Turn on edit for next keyin only. Return this kdswin.
	 */
	public final KeyinDisplay edit() {
		kynattr |= KYNATTR_EDIT;
		return this;
	}

	/**
	 * Turn on edit for all following keyins. Return this kdswin.
	 */
	public final KeyinDisplay editon() {
		kynattr |= KYNATTR_EDITON;
		return this;
	}

	/**
	 * Turn off edit for all keyins only. Return this kdswin.
	 */
	public final KeyinDisplay editoff() {
		kynattr &= ~KYNATTR_EDITON;
		return this;
	}

	/**
	 * Return true or false indicating whether EDIT | EDITON is on.
	 */
	public boolean isedit() {
		if ((kynattr & (KYNATTR_EDIT | KYNATTR_EDITON)) != 0)
			return true;
		return false;
	}

	/**
	 * Turn on digit entry for next keyin only. Return this kdswin.
	 */
	public final KeyinDisplay digitentry() {
		kynattr |= KYNATTR_DIGITENTRY;
		return this;
	}

	/**
	 * Turn off digit entry. Return this kdswin.
	 */
	public final KeyinDisplay digitentryoff() {
		kynattr &= ~KYNATTR_DIGITENTRY;
		return this;
	}

	/**
	 * Turn on justify right for next keyin only. Return this kdswin.
	 */
	public final KeyinDisplay justifyright() {
		kynattr |= KYNATTR_JUSTRIGHT;
		return this;
	}

	/**
	 * Turn on justify left for next keyin only. Return this kdswin.
	 */
	public final KeyinDisplay justifyleft() {
		kynattr |= KYNATTR_JUSTLEFT;
		return this;
	}

	/**
	 * Turn on zero fill entry for next keyin only. Return this kdswin.
	 */
	public final KeyinDisplay zerofill() {
		kynattr |= KYNATTR_ZEROFILL;
		return this;
	}

	/**
	 * Turn on auto enter for all following keyins. Return this kdswin.
	 */
	public final KeyinDisplay autoenteron() {
		kynattr |= KYNATTR_AUTOENTER;
		return this;
	}

	/**
	 * Turn off auto enter for all keyins only. Return this kdswin.
	 */
	public final KeyinDisplay autoenteroff() {
		kynattr &= ~KYNATTR_AUTOENTER;
		return this;
	}

	/**
	 * Turn off character echo for all following keyins. Return this kdswin.
	 */
	public final KeyinDisplay echooff() {
		kynattr |= KYNATTR_NOECHO;
		return this;
	}

	/**
	 * Turn on character echo for all keyins only. Return this kdswin.
	 */
	public final KeyinDisplay echoon() {
		kynattr &= ~KYNATTR_NOECHO;
		return this;
	}

	/**
	 * Set then echo secret character for all following keyins. Return this
	 * kdswin.
	 */
	public final KeyinDisplay echosecretchar(char c1) {
		eschar = c1;
		return this;
	}

	/**
	 * Turn on echo secret for all following keyins. Return this kdswin.
	 */
	public final KeyinDisplay echosecreton() {
		kynattr |= KYNATTR_ECHOSECRET;
		return this;
	}

	/**
	 * Turn off echo secret for all keyins only. Return this kdswin.
	 */
	public final KeyinDisplay echosecretoff() {
		kynattr &= ~KYNATTR_ECHOSECRET;
		return this;
	}

	/**
	 * Turn on echo upper case for all following keyins. Return this kdswin.
	 */
	public final KeyinDisplay uppercase() {
		kynattr |= KYNATTR_UPPERCASE;
		return this;
	}

	/**
	 * *lc
	 * 
	 * Turn on echo lower case for all following keyins. Return this kdswin.
	 */
	public final KeyinDisplay lowercase() {
		kynattr &= ~(KYNATTR_UPPERCASE | KYNATTR_INVERTCASE);
		kynattr |= KYNATTR_LOWERCASE;
		return this;
	}

	/**
	 * *it
	 * 
	 * Turn on echo case invert for all following keyins. Return this kdswin.
	 */
	public final KeyinDisplay invertcase() {
		kynattr &= ~(KYNATTR_UPPERCASE | KYNATTR_LOWERCASE);
		kynattr |= KYNATTR_INVERTCASE;
		return this;
	}

	/**
	 * *in
	 * 
	 * Change to normal case echo for all following keyins. Return this kdswin.
	 */
	public final KeyinDisplay normalcase() {
		kynattr &= ~(KYNATTR_UPPERCASE | KYNATTR_LOWERCASE | KYNATTR_INVERTCASE);
		return this;
	}

	/**
	 * Set keyin timeout. Return this kdswin.
	 */
	public final KeyinDisplay timeout(int secs) {
		if (secs >= 0) {
			timeout = (long) secs * 1000;
			kynattr |= KYNATTR_TIMEOUT;
		}
		return this;
	}

	/**
	 * Clear keyin timeout. Return this kdswin.
	 */
	public final KeyinDisplay cleartimeout() {
		kynattr &= ~KYNATTR_TIMEOUT;
		return this;
	}

	/**
	 * Change to normal for following keyins. Return this kdswin.
	 */
	public final KeyinDisplay keyinreset() {

		kynattr &= (KYNATTR_UPPERCASE | KYNATTR_LOWERCASE | KYNATTR_INVERTCASE);
		return this;
	}

	// return next character, KEY_TIMEOUT if timeout occurs, or 0 if
	// actionpending is set
	// wait if necessary
	private synchronized char getchar() {
		long time1 = 0;
		long time2 = 0;
		long waittime = 0;
		char chr;
		boolean timeoutflag = ((kynattr & KYNATTR_TIMEOUT) != 0);
		if (timeoutflag && (bufferpos == buffernext)) {
			time1 = (new Date()).getTime();
			waittime = timeout;
		}
		while (bufferpos == buffernext) {
			if (actionpending)
				return 0;
			try {
				if (timeoutflag) {
					// note: wait(0) is the same as wait()
					if (waittime > 0)
						wait(waittime);
				} else
					wait();
			} catch (InterruptedException e) {
				continue;
			}
			// if (basedbg != null) { // debugger stopped execution
			// basedbg.sourcedebug(basedbg.lastclass, basedbg.linenum);
			// basedbg = null;
			// }
			if (bufferpos != buffernext)
				break;
			if (timeoutflag) {
				time2 = (new Date()).getTime();
				waittime = timeout - (time2 - time1);
				if (waittime < 1)
					return KEY_TIMEOUT;
			}
		}
		if (actionpending)
			return 0;
		chr = keyinbuffer[bufferpos++];
		if (bufferpos == KEYINBUFFERSIZE)
			bufferpos = 0;
		return chr;
	}

	/**
	 * Setup field length for keyin using *kl kllength == 0 means that *kl was
	 * not used
	 */
	public void keyinlength(int i1) {
		kllength = i1;
	}

	/**
	 * Return number of characters returned in dest
	 * if numflag, then length2 is count of digits right of decimal point
	 * if !numflag & EDIT, then length2 is the initial length of the characters
	 * in dest
	 * 
	 * @param dest
	 * @param numflag
	 * @param length1
	 * @param length2
	 * @return
	 */
	private int keyin(char[] dest, boolean numflag, int length1, int length2) {
		int i1;
		char chr, chr1, pholder;
		boolean editflag = false;
		int charcount = 0;
		int starth = h;
		int length;
		int klleft = 0;

		if (length1 < 1 || dest == null)
			return 0;
		if ((kynattr & KYNATTR_EDIT) != 0) {
			editflag = true;
			kynattr = (kynattr & ~KYNATTR_EDIT);
		} else if ((kynattr & KYNATTR_EDITON) != 0)
			editflag = true;
		if (editflag || kllength > 0) {
			sethv(h, v);
			i1 = right - h + 1;
			if (length1 > i1)
				length1 = i1;
			for (i1 = 0; i1 < length1; i1++) {
				dspline[i1] = keyinchars[i1] = dest[i1];
			}
			if (!numflag) {
				for (i1 = length2; i1 < length1; i1++)
					dspline[i1] = keyinchars[i1] = ' ';
			}
			putrect(dspline, 0, ((kllength > 0) ? kllength : length1), true, v, v, h, right, dspattr);
		}
		if (cursorstate != CURSORSTATE_OFF) {
			sethv(h, v);
			setcursor(cursorshape);
		}
		flush();
		if (length1 > dest.length)
			length1 = dest.length;
		if (length2 < 0)
			length2 = 0;
		else if (length2 >= length1) {
			if (numflag)
				length2 = length1 - 1;
			else
				length2 = length1;
		}
		lastendkey = 0;

		if (kllength > 0)
			length = kllength;
		else
			length = length1;

		mainloop: while (true) {
			chr = getchar();
			if (actionpending)
				break mainloop;
			if (chr == KEY_TIMEOUT) {
				lastendkey = chr;
				break mainloop;
			}
			if (chr < '\uF000') {
				if ((kynattr & KYNATTR_DIGITENTRY) != 0) {
					if (chr < '0' || chr > '9') {
						beep();
						continue;
					}
				} else {
					if (keyinupcase)
						chr = Character.toUpperCase(chr);
					else if (keyinrevcase) {
						if (Character.isUpperCase(chr))
							chr = Character.toLowerCase(chr);
						else
							chr = Character.toUpperCase(chr);
					}
					chr1 = Character.toUpperCase(chr);
					if ((kynattr & KYNATTR_UPPERCASE) != 0)
						chr = chr1;
					else if ((kynattr & KYNATTR_LOWERCASE) != 0)
						chr = Character.toLowerCase(chr);
					else if ((kynattr & KYNATTR_INVERTCASE) != 0) {
						if (chr == chr1)
							chr = Character.toLowerCase(chr);
						else
							chr = chr1;
					}
				}
			}
			for (i1 = 0; i1 <= endkeysmaxuse; i1++) {
				if (chr == endkeys[i1]) {
					if (!numflag && (editflag || kllength > 0) && (chr == KEY_BKSPC || chr == KEY_CANCEL || (chr >= KEY_LEFT && chr <= KEY_END)))
						;
					else {
						lastendkey = chr;
						break mainloop;
					}
				}
			}
			if (chr < '\uF000' && chr >= 32) {
				if (kllength > 0) {
					if (klleft + charcount == length1) {
						beep();
						continue;
					}
				} else if (charcount == length) {
					beep();
					continue;
				}
				if (numflag) {
					if ((kynattr & KYNATTR_DCOMMA) != 0)
						pholder = ','; // *dcon
					else
						pholder = '.';
					if (chr == '-') {
						if (length2 == length - 1) {
							beep();
							continue;
						}
						for (i1 = 0; i1 < charcount; i1++) {
							chr1 = keyinchars[i1];
							if (chr1 == '-' || chr == pholder || (chr1 >= '0' && chr1 <= '9')) {
								beep();
								continue;
							}
						}
					} else if (chr == pholder) {
						if (length2 == 0) {
							beep();
							continue;
						}
						for (i1 = 0; i1 < charcount && keyinchars[i1] != pholder; i1++)
							;
						if (i1 < charcount) {
							beep();
							continue;
						}
					} else if (chr == ' ') {
						for (i1 = 0; i1 < charcount && keyinchars[i1] == ' '; i1++)
							;
						if (i1 < charcount) {
							beep();
							continue;
						}
					} else if (chr < '0' || chr > '9') {
						beep();
						continue;
					} else if (length2 > 0) {
						for (i1 = 0; i1 < charcount && keyinchars[i1] != pholder; i1++)
							;
						if (i1 == charcount) {
							if (i1 == length - length2 - 1) {
								beep();
								continue;
							}
						} else if (charcount - i1 > length2) {
							beep();
							continue;
						}
					}
				}
				if (kllength > 0) {
					for (i1 = length1 - 1; i1 > klleft + charcount; i1--)
						dspline[i1] = keyinchars[i1] = keyinchars[i1 - 1];
					dspline[klleft + charcount] = keyinchars[klleft + charcount] = chr;
					putrect(dspline, klleft, kllength, true, v, v, h - charcount, right, dspattr);
					flush();
					if (length2 < kllength) {
						charcount++;
						if (h < right)
							sethv(++h, v);
					}
					if (length2 < length1)
						length2++;
					if (charcount + klleft == length1 && (kynattr & KYNATTR_AUTOENTER) != 0) {
						lastendkey = 0;
						break;
					}
					continue;
				} else if (editflag) {
					for (i1 = length - 1; i1 > charcount; i1--)
						dspline[i1] = keyinchars[i1] = keyinchars[i1 - 1];
					dspline[charcount] = keyinchars[charcount] = chr;
					putrect(dspline, charcount, length - charcount, true, v, v, h, right, dspattr);
					if (h < right)
						sethv(++h, v);
					flush();
					charcount++;
					if (length2 < length)
						length2++;
					if (charcount == length && (kynattr & KYNATTR_AUTOENTER) != 0) {
						lastendkey = 0;
						break;
					}
					continue;
				}
				keyinchars[charcount] = chr;
				if ((kynattr & KYNATTR_ECHOSECRET) == 0)
					dspline[charcount] = chr;
				else
					dspline[charcount] = eschar;
				if ((kynattr & KYNATTR_NOECHO) == 0) {
					if (numflag && (editflag || kllength > 0)) {
						for (i1 = 1; i1 < length; dspline[i1++] = ' ')
							;
						putrect(dspline, 0, length, true, v, v, h, right, dspattr);
						editflag = false;
					} else
						putrect(dspline, charcount, 1, true, v, v, h, h, dspattr);
					if (h < right)
						sethv(++h, v);
					flush();
				}
				charcount++;
				if (charcount == length && (kynattr & KYNATTR_AUTOENTER) != 0) {
					lastendkey = 0;
					break;
				}
				continue;
			}
			if (chr == KEY_CANCEL) {
				if (kllength > 0) {
					for (i1 = 0; i1 < length1; i1++)
						dspline[i1] = keyinchars[i1] = ' ';
					h = starth;
					if (charcount + h > right)
						charcount = right - h;
					putrect(dspline, 0, kllength, true, v, v, h, right, dspattr);
					sethv(h, v);
					flush();
					charcount = 0;
					if (!numflag)
						length2 = 0;
				} else {
					for (i1 = 0; i1 < charcount; i1++)
						dspline[i1] = keyinchars[i1] = ' ';
					if (editflag || (kynattr & KYNATTR_NOECHO) == 0) {
						h = starth;
						if (charcount + h > right)
							charcount = right - h;
						putrect(dspline, 0, charcount, true, v, v, h, right, dspattr);
						sethv(h, v);
						flush();
					}
					charcount = 0;
					if (!numflag && (editflag || kllength > 0))
						length2 = 0;
				}
			} else if (!(editflag || kllength > 0)) {
				if (charcount > 0 && chr == KEY_BKSPC) {
					charcount--;
					dspline[charcount] = keyinchars[charcount] = ' ';
					if ((kynattr & KYNATTR_NOECHO) == 0) {
						if (starth + charcount <= right)
							sethv(--h, v);
						putrect(dspline, charcount, 1, true, v, v, h, h, dspattr);
						flush();
					}
				}
			} else if (chr == KEY_BKSPC) {
				if (kllength > 0) {
					if (charcount == 0 && klleft > 0) {
						// delete characters to left of window that can't be
						// seen
						length2--;
						for (i1 = klleft - 1; i1 < length1 - 1; i1++)
							keyinchars[i1] = dspline[i1] = dspline[i1 + 1];
						dspline[length1 - 1] = keyinchars[length1 - 1] = ' ';
						klleft--;
						flush();
					} else if (charcount + klleft == length2 - 1 && length2 > kllength) {
						dspline[length2 - 2] = dspline[length2 - 1];
						dspline[length2 - 1] = ' ';
						length2--;
						charcount--;
						sethv(--h, v);
						putrect(dspline, charcount + klleft, kllength - charcount, true, v, v, h, right, dspattr);
						flush();
					} else if (charcount > 0) { // delete characters inside
												// window
						charcount--;
						length2--;
						for (i1 = charcount + klleft; i1 < length1 - 1; i1++)
							keyinchars[i1] = dspline[i1] = dspline[i1 + 1];
						dspline[length1 - 1] = keyinchars[length1 - 1] = ' ';
						if (starth + charcount <= right)
							sethv(--h, v);
						putrect(dspline, charcount + klleft, kllength - charcount, true, v, v, h, right, dspattr);
						flush();
					}
				} else if (charcount > 0) {
					charcount--;
					length2--;
					for (i1 = charcount; i1 < length - 1; i1++)
						keyinchars[i1] = dspline[i1] = dspline[i1 + 1];
					dspline[length - 1] = keyinchars[length - 1] = ' ';
					if (starth + charcount <= right)
						sethv(--h, v);
					putrect(dspline, charcount, length - charcount, true, v, v, h, right, dspattr);
					flush();
				}
			} else if (chr == KEY_DELETE) {
				if (kllength > 0) {
					if (charcount == kllength - 1) {
						if ((charcount + klleft + 1) < length2) {
							// we are on right edge of window with more stuff to
							// the right
							length2--;
							for (i1 = charcount + klleft; i1 < length1 - 1; i1++)
								keyinchars[i1] = dspline[i1] = dspline[i1 + 1];
							dspline[length1 - 1] = keyinchars[length1 - 1] = ' ';
							sethv(h, v);
							flush();
						} else {
							if (true) {
								// we are on right edge with nothing to the
								// right
								dspline[length2 - 1] = keyinchars[length2 - 1] = ' ';
								length2--;
								putrect(dspline, klleft + charcount, 1, true, v, v, h, right, dspattr);
								sethv(h--, v);
								charcount--;
								flush();
							}
						}
					} else if ((length2 - klleft) + charcount > 0 && dspline[klleft + charcount] != ' ') {
						length2--;
						for (i1 = charcount + klleft; i1 < length1 - 1; i1++)
							keyinchars[i1] = dspline[i1] = dspline[i1 + 1];
						dspline[length1 - 1] = keyinchars[length1 - 1] = ' ';
						putrect(dspline, charcount + klleft, kllength - charcount, true, v, v, h, right, dspattr);
						sethv(h, v);
						flush();
					}
				} else if (length2 > charcount) {
					length2--;
					for (i1 = charcount; i1 < length - 1; i1++)
						keyinchars[i1] = dspline[i1] = dspline[i1 + 1];
					dspline[length - 1] = keyinchars[length - 1] = ' ';
					putrect(dspline, charcount, length - charcount, true, v, v, h, right, dspattr);
					sethv(h, v);
					flush();
				}
			} else if (chr == KEY_HOME) {
				charcount = 0;
				h = starth;
				sethv(h, v);
				if (kllength > 0) {
					klleft = 0;
					putrect(dspline, 0, kllength, true, v, v, h, right, dspattr);
				}
				flush();
			} else if (chr == KEY_END) {
				if (kllength > 0) {
					if (kllength <= length2) {
						charcount = kllength;
						h = starth + kllength;
						sethv(h, v);
						klleft = length2 - kllength;
						putrect(dspline, length2 - kllength, kllength, true, v, v, h - kllength, right, dspattr);
					} else {
						i1 = charcount;
						do {
							i1++;
							charcount++;
							sethv(++h, v);
							flush();
						} while (dspline[i1] != ' ');
					}
					flush();
				} else {
					charcount = length2;
					h = starth + length2;
					if (h > right)
						h = right;
					sethv(h, v);
					flush();
				}
			} else if (chr == KEY_LEFT) {
				if (kllength > 0 && charcount >= 0) {
					if (charcount == 0 && klleft > 0) {
						klleft--;
						putrect(dspline, klleft, kllength, true, v, v, h, right, dspattr);
						flush();
					} else if (charcount > 0) {
						charcount--;
						sethv(--h, v);
						flush();
					}
				} else if (charcount > 0) {
					charcount--;
					sethv(--h, v);
					flush();
				}
			} else if (chr == KEY_RIGHT) {
				if (kllength > 0) {
					if ((charcount + 1 + klleft) < length2) { // we can shift
																// window right
						if (charcount == kllength - 1) { // we are on right edge
															// of window
							klleft++;
							putrect(dspline, klleft, kllength, true, v, v, h - charcount, right, dspattr);
							flush();
						} else {
							charcount++;
							sethv(++h, v);
							flush();
						}
					} else if (charcount < kllength - 1) {
						if (charcount + 1 + klleft == length2) {
							charcount++;
							sethv(++h, v);
							flush();
						}
					}
				} else if (charcount < length2) {
					charcount++;
					sethv(++h, v);
					flush();
				}
			}
		}
		if (editflag) {
			if (numflag)
				charcount = length;
			else
				charcount = length2;
		}
		if (kllength > 0) {
			if (numflag)
				charcount = length1;
			else
				charcount = length2;
			for (i1 = 0; i1 < length1; i1++) {
				if (numflag && keyinchars[i1] == ',' && ((kynattr & KYNATTR_DCOMMA) != 0)) {
					dest[i1] = '.';
				} else
					dest[i1] = keyinchars[i1];
			}
		} else {
			for (i1 = 0; i1 < charcount; i1++) {
				if (numflag && keyinchars[i1] == ',' && ((kynattr & KYNATTR_DCOMMA) != 0)) {
					dest[i1] = '.';
				} else
					dest[i1] = keyinchars[i1];
			}
		}
		if (cursorstate != CURSORSTATE_ON)
			setcursor(CURSOR_NONE);
		flush();
		return charcount;
	}

	// is called as part of another thread to receive characters
	synchronized public void recvchar(char c1) {
		if (c1 == 3) {
			actionpending = breakpending = true;
			Client.getDefault().queueKDAction();
		}
		else if (c1 == 26) {
			actionpending = interruptpending = true;
			Client.getDefault().queueKDAction();
		}
		else if (checktrapkey(c1))
			bufferpos = buffernext = 0;
		else {
			if ((buffernext == bufferpos - 1) || ((buffernext == KEYINBUFFERSIZE - 1) && bufferpos == 0))
				return;
			keyinbuffer[buffernext++] = c1;
			if (buffernext == KEYINBUFFERSIZE)
				buffernext = 0;
		}
		notifyAll();
	}

	void setAnsi256ColorMode(boolean b) {
		ansi256ColorMode = b;
		if (debug && ansi256ColorMode) {
			System.out.println("In KeyinDisplay, Color mode is Ansi256");
		}
	}

	boolean isAnsi256ColorMode() {
		return ansi256ColorMode;
	}


}
