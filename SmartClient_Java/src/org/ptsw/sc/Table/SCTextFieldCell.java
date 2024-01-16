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

package org.ptsw.sc.Table;

import javafx.scene.control.TextField;

/**
 * Needed to subclass TextField for use as an 'edit' mode object in
 * an Edit or Ledit table column type.
 * 
 * Note: We do not allow the Paste or Setmaxchars change commands to a table
 */
public class SCTextFieldCell extends TextField {

	private boolean limited = false;
	private int maxlength = Integer.MAX_VALUE;
	
	public SCTextFieldCell(String string) {
		super(string);
	}

	/**
	 * @param maxlength The maximum number of characters the user is allowed
	 * to enter into this text field
	 */
	public void setMaxlength(int maxlength) {
		this.maxlength = maxlength;
		limited = true;
	}

	@Override
    public void replaceText(int start, int end, String text) {
    	if (!limited) super.replaceText(start, end, text);
    	else {
	        String txt2 = (getText() == null) ? "" : getText();
	        if (text.equals("")) {
	            super.replaceText(start, end, text);
	        }
	        else if (txt2.length() <= maxlength) {
	        	if (txt2.length() + text.length() - (end - start) <= maxlength)
	        		super.replaceText(start, end, text);
	        }
    	}
    }

    /**
     * @param text
     */
    @Override
    public void replaceSelection(String text) {
    	if (!limited) super.replaceSelection(text);
    	else {
	        if (text.equals("")) {
	            super.replaceSelection(text);
	        } else if (getText().length() <= maxlength) {
	            // Add characters, but don't exceed maxlength.
	            if (text.length() > maxlength - getText().length()) {
	                text = text.substring(0, maxlength- getText().length());
	            }
	            super.replaceSelection(text);
	        }
    	}
    }


}
