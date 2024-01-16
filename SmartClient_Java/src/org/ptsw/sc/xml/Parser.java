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

package org.ptsw.sc.xml;

import java.io.IOException;
import java.io.PushbackReader;
import java.io.Reader;

public class Parser
{

private static final char[] META_PREFIX_CHAR_ARRAY = {'<', '?'};
private static final char[] META_POSTFIX_CHAR_ARRAY = {'?', '>'};

private final char[] input_char_array = new char[META_PREFIX_CHAR_ARRAY.length];
private PushbackReader in;
private String token;
private boolean iswordtoken;
private char peekchar;
private char nextchar = '\uFFFF';
private StringBuilder peeksb = new StringBuilder();
private StringBuilder worksb = new StringBuilder();
private StringBuilder builtinsb = new StringBuilder();

public Parser(Reader in)
{
	this.in = new PushbackReader(in, META_PREFIX_CHAR_ARRAY.length + 1);
	try {
		if (isFirstElementMeta(this.in)) skipMeta(this.in);
	}
	catch (IOException e1) {
		System.err.println("Error in org.ptsw.sc.Parser<init>");
		e1.printStackTrace();
	}
}

private static void skipMeta(Reader in) throws IOException {
	int c1 = in.read();
	int c2 = in.read();
	while (c2 != -1) {
		if (c1 == META_POSTFIX_CHAR_ARRAY[0] && c2 == META_POSTFIX_CHAR_ARRAY[1]) return;
		c1 = c2;
		c2 = in.read();
	}
}

private boolean isFirstElementMeta(PushbackReader in) throws IOException {
	int i2 = input_char_array.length;
	int i1 = in.read(input_char_array, 0, i2);
	if (i1 == -1) return false;
	if (i1 < i2) {
		in.unread(input_char_array, 0, i1);
		return false;
	}
	else {
		for (i1 = 0; i1 < i2; i1++)
			if (input_char_array[i1] != META_PREFIX_CHAR_ARRAY[i1]) {
				in.unread(input_char_array, 0, i2);
				return false;
			}
	}
	return true;
}

public Element getNextElement() throws Exception
{
	String s1;
	if (!gettoken()) return null;
	if (!token.equals("<")) throw new Exception("< expected: " + token);
	if (!gettoken()) throw new Exception("premature end of file");
	if (!iswordtoken) throw new Exception("invalid syntax: " + token);
	Element element = new Element(token);
	while (true) {
		if (!gettoken()) throw new Exception("premature end of file");
		if (token.equals(">")) break;
		if (token.equals("/")) {
			if (!gettoken()) throw new Exception("premature end of file");
			if (token.equals(">")) return element;
			throw new Exception("invalid syntax: " + token);
		}
		if (!iswordtoken) throw new Exception("invalid syntax: " + token);
		s1 = token;
		if (!gettoken()) throw new Exception("premature end of file");
		if (!token.equals("=")) throw new Exception("= expected: " + token);
		if (!gettoken()) throw new Exception("premature end of file");
		element.addAttribute(new Attribute(s1, token));
	}
	while (true) {
		if (!peek(true)) throw new Exception("premature end of file");
		if (peekchar != '<') {
			element.addString(gettext());
		}		
		else {
			if (!peek()) throw new Exception("premature end of file");
			if (peekchar == '/') break;
			element.addElement(getNextElement());
		}
	}
	gettoken();
	gettoken();
	if (!gettoken()) throw new Exception("premature end of file");
	if (!token.equals(element.name)) throw new Exception("invalid element nesting: " + token);
	gettoken();
	if (!token.equals(">")) throw new Exception("> expected: " + token);
	return element;
}

private String gettext() throws Exception
{
	int i1, n1;
	char c1;
	worksb.setLength(0);
	n1 = peeksb.length();
	while (true) {
		if (nextchar != '\uFFFF') {
			c1 = nextchar;
			nextchar = '\uFFFF';
		}
		else if (n1 != 0) {
			c1 = peeksb.charAt(--n1);
			peeksb.setLength(n1);
		}
		else {
			i1 = in.read();
			if (i1 == -1) break;
			c1 = (char) i1;
		}
		if (c1 == '&') c1 = getbuiltin();
		else if (c1 == '<') {
			nextchar = c1;
			break;
		}
		worksb.append(c1);
	}
	return worksb.toString();
}

/**
 * 
 * @return false if there are no more tokens, true if there was one and then 'token' is set to it.
 * @throws Exception
 */
private boolean gettoken() throws Exception
{
	int i1, n1;
	char c1;
	int quote = 0;
	worksb.setLength(0);
	n1 = peeksb.length();
	while (true) {
		if (nextchar != '\uFFFF') {
			c1 = nextchar;
			nextchar = '\uFFFF';
		}
		else if (n1 != 0) {
			c1 = peeksb.charAt(--n1);
			peeksb.setLength(n1);
		}
		else {
			i1 = in.read();
			if (i1 == -1) {
				token = null;
				return false;
			}
			c1 = (char) i1;
		}
		if (Character.isWhitespace(c1)) continue;
		if (c1 == '&') {
			c1 = getbuiltin();
			n1 = peeksb.length();
		}
		break;
	}
	if (c1 == '"') quote = 1;
	else if (c1 == '\'') quote = 2;
	else {
		worksb.append(c1);
		if (!Character.isJavaIdentifierPart(c1) && c1 != '.') {
			token = worksb.toString();
			return true;
		}
	}
	while (true) {
		if (n1 != 0) {
			c1 = peeksb.charAt(--n1);
			peeksb.setLength(n1);
		}
		else {
			i1 = in.read();
			if (i1 == -1) break;
			c1 = (char) i1;
		}
		if (c1 == '&') c1 = getbuiltin();
		else if (quote == 1 && c1 == '"') break;
		else if (quote == 2 && c1 == '\'') break;
		else if (quote == 0 && !Character.isJavaIdentifierPart(c1) && c1 != '.') {
			nextchar = c1;
			break;
		}
		worksb.append(c1);
	}
	token = worksb.toString();
	if (quote == 0 && token.length() > 0 && Character.isJavaIdentifierStart(token.charAt(0)))
		iswordtoken = true;
	else
		iswordtoken = false;
	return true;
}

private boolean peek() throws Exception 
{
	return peek(false);
}

private boolean peek(boolean allow_space) throws Exception
{
	while (true) {
		if (nextchar != '\uFFFF') {
			peekchar = nextchar;
			nextchar = '\uFFFF';
		}
		else {
			int n1 = in.read();
			if (n1 == -1) return false;
			peekchar = (char) n1;
		}
		peeksb.insert(0, peekchar);
		// preserve newlines and tabs
		if (peekchar == 0x000A || peekchar == 0x0009) break;
		if (!Character.isWhitespace(peekchar) || (allow_space && (peekchar == ' '))) {
			break;
		}
	}
	return true;
}

private char getbuiltin() throws Exception
{
	int i1, n1;
	char c1 = ' ';
	builtinsb.setLength(0);
	n1 = peeksb.length();
	while (c1 != ';') {
		if (n1 != 0) {
			c1 = peeksb.charAt(--n1);
			peeksb.setLength(n1);
		}
		else {
			i1 = in.read();
			if (i1 == -1) break;
			c1 = (char) i1;
		}
		builtinsb.append(c1);
	}
	String s1 = builtinsb.toString();
	if (s1.equals("lt;")) return '<';
	if (s1.equals("gt;")) return '>';
	if (s1.equals("amp;")) return '&';
	if (s1.equals("apos;")) return '\'';
	if (s1.equals("quot;")) return '"';
	if (s1.charAt(0) == '#') {
		int semicpos = s1.indexOf(';');
		if (semicpos >= 2) {
			try {
				c1 = (char) Integer.parseInt(s1.substring(1, semicpos));
				return c1;
			} catch (NumberFormatException e) {
				// do nothing, fall through to throw
			}
		}
	}
	throw new Exception("invalid builtin &" + s1);
}

}
