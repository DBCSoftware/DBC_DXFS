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

import javafx.beans.value.ObservableValue;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;

class ListItem extends ListDropItem {

	ListItem(ObservableValue<Color> color, Font font, String text, SCListDropBase parent) {
		super(color, font, text, parent);
	}
}
