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

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;

public class XMLInputStream extends BufferedReader
{

private Parser parser;

public XMLInputStream(InputStream in) throws java.io.IOException
{
	super(new InputStreamReader(in, "US-ASCII"));
}

public XMLInputStream(Reader in)
{
	super(in);
}

public Element readElement() throws Exception
{
	if (parser == null) parser = new Parser(this);
	return parser.getNextElement();
}

}
