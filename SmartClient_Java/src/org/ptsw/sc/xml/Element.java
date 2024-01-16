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

import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.LinkedHashSet;
import java.util.Objects;

public class Element implements Cloneable
{
	public String name;
	public static final Charset ISOLatin = Charset.forName("ISO-8859-1");
	private ArrayList<Attribute> attributes;
	private LinkedHashSet<Object> children;

	/**
	 * Construct an Element with the given tag name
	 * @param name the tag name
	 */
	public Element(String name)
	{
		this.name = Objects.requireNonNull(name, "Invalid element name").trim();
		if (this.name.length() == 0) throw new IllegalArgumentException("Invalid element name");
	}

	/**
	 * constructor with a String subelement
	 * @param name Element Tag
	 * @param text Becomes a String child Element
	 */
	public Element(String name, String text)
	{
		this.name = Objects.requireNonNull(name, "Invalid element name").trim();
		if (this.name.length() == 0) throw new IllegalArgumentException("Invalid element name");
		if (Objects.nonNull(text)) {
			children = new LinkedHashSet<>();
			children.add(text);
		}
	}

	/**
	 * Convenience constructor, takes an integer
	 * second argument.
	 */
	public Element(String name, int data)
	{
		this(name, String.valueOf(data));
	}

	// add an attribute to this element
	public final Element addAttribute(Attribute attr)
	{
		if (attr == null) throw new IllegalArgumentException("Invalid attribute");
		if (attributes == null) attributes = new ArrayList<>();
		attributes.add(attr);
		return this;
	}

	public final void removeAttribute(Attribute attr) {
		if (attributes != null) attributes.remove(attr);
	}
	
	// add an Element to the subelements of this element
	public final Element addElement(Element element)
	{
		Objects.requireNonNull(element, "Invalid element");
		if (Objects.isNull(children)) children = new LinkedHashSet<>();
		children.add(element);
		return this;
	}

	// add an array of Elements
	public final Element addElement(Element[] elements) {
		if (Objects.isNull(children)) children = new LinkedHashSet<>();
		children.addAll(Arrays.asList(elements));
		return this;
	}

	/**
	 * @return true if this Element has any child Elements
	 */
	public final boolean hasChildren() {
		return children != null && children.size() > 0;
	}

	// add a String to the subelements of this element
	public final Element addString(String s1)
	{
		if (Objects.isNull(children)) children = new LinkedHashSet<>();
		children.add(s1);
		return this;
	}

	// return the number of attributes
	public final int getAttributeCount()
	{
		if (Objects.isNull(attributes)) return 0;
		return attributes.size();
	}

	// get the Attribute by its index
	public final Attribute getAttribute(int index)
	{
		if (Objects.isNull(attributes) || index < 0 || index >= attributes.size()) return null;
		return attributes.get(index);
	}

	/**
	 * Find an attribute by its name
	 * 
	 * @param attrname the name of the attribute
	 * @return The attribute, or null if not found
	 */
	public final Attribute getAttribute(String attrname)
	{
		if (Objects.isNull(attributes)) return null;
		for (Attribute a1 : attributes) {
			if (a1.name.equals(attrname)) return a1;
		}
		return null;
	}

	/**
	 * Determine existance of an attribute by its name
	 * 
	 * @param attrname the name of the attribute
	 * @return true if found
	 */
	public final boolean hasAttribute(String attrname)
	{
		if (Objects.isNull(attributes)) return false;
		Attribute a1 = getAttribute(attrname);
		return a1 != null;
	}

	/**
	 * Find an attribute value by its attribute name
	 * 
	 * @param attrname The name of the Attribute.
	 * @return attribute value as a String or null if the Attribute cannot be found.
	 */
	public final String getAttributeValue(String attrname)
	{
		if (Objects.isNull(attributes)) return null;
		Attribute a1 = getAttribute(attrname);
		return (a1 == null) ? null : a1.value;
	}

	// return the number of children
	public final int getChildrenCount()
	{
		if (Objects.isNull(children)) return 0;
		return children.size();
	}

	/**
	 * @return The Collection of child objects of this element
	 */
	public final Collection<Object> getChildren()
	{
		if (Objects.isNull(children)) children = new LinkedHashSet<>();
		return children;
	}

	// return a child by its index
	public final Object getChild(int index)
	{
		if (Objects.isNull(children) || index < 0 || index >= children.size()) return null;
		return children.toArray()[index];
	}


	public final void clearChildren() {
		if (Objects.nonNull(children)) children.clear();
	}

	/**
	 *  Find an element via its tag name
	 */
	public final Element getElement(String elementName)
	{
		if (Objects.nonNull(children) && elementName != null) {
			for (Object obj1 : children) {
				if (!(obj1 instanceof Element)) continue;
				Element element = (Element) obj1;
				if (element.name.equals(elementName)) return element;
			}
		}
		return null; 
	}

	/**
	 * return this element and its subtree as String (with newlines are inserted)
	 */
	@Override
	public final String toString()
	{
		StringBuilder sb1 = new StringBuilder();
		addelementtosb(this, sb1, true);
		return sb1.toString();
	}

	/**
	 * return this element and its subtree as String (with newlines are inserted)
	 */
	public final String toStringRaw()
	{
		StringBuilder sb1 = new StringBuilder();
		addelementtosb(this, sb1, false);
		return sb1.toString();
	}

	/**
	 * return a new Element with all new subElements
	 */
	@SuppressWarnings("unchecked")
	@Override
	public final Object clone()
	{
		Element element = new Element(name);
		if (Objects.nonNull(attributes)) element.attributes = (ArrayList<Attribute>) attributes.clone();
		if (Objects.nonNull(children)) {
			element.children = new LinkedHashSet<>();
			for (Object o1 : children) {
				if (!(o1 instanceof Element)) element.children.add(o1);
				else element.children.add(((Element) o1).clone());
			}
		}
		return element;
	}


	private void addelementtosb(Element element, StringBuilder sb1, boolean emitnl)
	{
	 	sb1.append('<').append(element.name);
	 	ArrayList<Attribute> work = element.attributes;
		if (work != null) {
			for (Attribute attr : work) {
				sb1.append(' ').append(attr.toString());
			}
		}
		if (!element.hasChildren()) {
			if (emitnl) sb1.append("/>\n");
			else sb1.append("/>");
		}
		else {
			sb1.append('>');
			for (Object o1 : element.getChildren()) {
				if (o1 instanceof Element) addelementtosb((Element) o1, sb1, emitnl);
				else {
					setbuiltin(sb1, o1.toString());
				}
			}
			if (emitnl) sb1.append("</").append(element.name).append(">\n");
			else sb1.append("</").append(element.name).append(">");
		}
	}

	private static int unsignedToBytes(byte b) {
		return b & 0xFF;
	}
	
	static void setbuiltin(StringBuilder sb1, String input) {
		char c1;
		if (input != null) {
			for (int i1 = 0; i1 < input.length(); i1++) {
				c1 = input.charAt(i1);
				if (c1 == '>') sb1.append("&gt;");
				else if (c1 == '<') sb1.append("&lt;");
				else if (c1 == '&') sb1.append("&amp;");
				else if (c1 == '\'') sb1.append("&apos;");
				else if (c1 == '"') sb1.append("&quot;");
				else if (c1 < 0x20 || c1 >= 0x7F) {
					String s2 = input.substring(i1, i1 + 1);
					byte[] b1 = s2.getBytes(ISOLatin);
					/**
					 * If a question mark is returned, then the encoding is telling us that
					 * it cannot do it. So we strip it out.
					 * 
					 * 09 OCT 2012 jpr
					 */
					if (b1[0] != 0x3f) sb1.append("&#").append(unsignedToBytes(b1[0])).append(';');
				}
				else sb1.append(c1);
	 		}
		}
	}

	/**
	 * @return The Element's Attributes. An empty collection if none 
	 */
	public Collection<Attribute> getAttributes() {
		if (Objects.isNull(attributes)) attributes = new ArrayList<>();
		return attributes;
	}

}
