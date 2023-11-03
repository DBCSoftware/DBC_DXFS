/*******************************************************************************
 *
 * Copyright 2023 Portable Software Company
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
 *
 ******************************************************************************/

#ifndef GLYPHWIDTHS_H_
#define GLYPHWIDTHS_H_
#include "includes.h"

#define FIRSTCHAR 32
#define LASTCHAR 255

USHORT getGlyphWidth(FONTDATA* ffd, UCHAR charvalue, USHORT *value);

#endif /* GLYPHWIDTHS_H_ */
