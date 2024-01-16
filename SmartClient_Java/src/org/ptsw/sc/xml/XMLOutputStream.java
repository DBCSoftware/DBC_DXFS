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

public class XMLOutputStream extends java.io.OutputStreamWriter
{

public XMLOutputStream(java.io.OutputStream out) throws java.io.IOException
{
	super(out, "UTF8");
}

public void writeElement(Element element) throws java.io.IOException
{
	if (element != null) {
		String s1 = element.toString();
		write(s1, 0, s1.length());
		flush();
	}
}

}
