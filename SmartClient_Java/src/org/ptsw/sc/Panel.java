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

import org.ptsw.sc.xml.Element;

import javafx.scene.Node;
import javafx.scene.layout.Pane;


public class Panel extends Resource {

	Panel(final Element element) throws Exception {
		super(element);
		if (Client.IsTrace()) System.out.println("Entering Panel<>");
		setId(element.getAttributeValue(Client.PANEL));
	}

	@Override
	public void Close() {
		if (((Node)content).isVisible()) Hide();
		SmartClientApplication.Invoke(new Runnable() {
			@Override
			public void run() {
				((Node)content).setVisible(false);
				((Pane)content).getChildren().clear();
			}
		}, false);
	}

	@Override
	public void Hide()
	{
		if (visibleOnWindow != null) visibleOnWindow.HideResource(this);
	}
	

}
