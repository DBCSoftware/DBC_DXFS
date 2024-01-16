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

package org.ptsw.sc.ListDrop;

import java.util.ArrayList;

import org.ptsw.sc.Resource;

import javafx.scene.paint.Color;

public interface SingleSelectionBox {
	void delete(String text);
	void deleteline(int index);
	void erase();
	void insert(String data) throws Exception;
	void minsert(ArrayList<String> datums) throws Exception;
	void insertbefore(String data) throws Exception;
	void insertafter(String data) throws Exception;
	void insertlineafter(String data) throws Exception;
	void insertlinebefore(String data) throws Exception;
	void locate(String text);
	void locateLine(int index);
	void textbgcolordata(Color newColor, String text);
	void textbgcolorline(Color newColor, int index);
	void textcolor(Color newColor);
	void textcolorline(Color newColor, int index);
	void textcolordata(Color newColor, String text);
	void textstyleline(String newStyle, int index) throws Exception;
	void textstyledata(String newStyle, String text) throws Exception;
	Resource getParentResource();
	String getId();
	Object getStatus();
	//Font getInitialfont();
}
