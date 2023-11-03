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

/**
 * The order that these appear is significant.
 * In the prtfont function in prt.c there is an array of font names.
 * We index into that array and the index number is used throughout the pdf prt code.
 * For convenience we order them here the same as that array.
 * That index number is also used in the pdf file.
 */
extern unsigned short AFM_Courier[];
extern unsigned short AFM_Courier_Oblique[];
extern unsigned short AFM_Courier_Bold[];
extern unsigned short AFM_Courier_BoldOblique[];
extern unsigned short AFM_Times_Roman[];
extern unsigned short AFM_Times_Italic[];
extern unsigned short AFM_Times_Bold[];
extern unsigned short AFM_Times_BoldItalic[];
extern unsigned short AFM_Helvetica[];
extern unsigned short AFM_Helvetica_Oblique[];
extern unsigned short AFM_Helvetica_Bold[];
extern unsigned short AFM_Helvetica_BoldOblique[];

/**
 * A one-dimensional array of one-dimensional arrays, i.e. a 'ragged' array
 */
unsigned short *AFM_fonts[] = {AFM_Courier, AFM_Courier_Oblique, AFM_Courier_Bold, AFM_Courier_BoldOblique,\
		AFM_Times_Roman, AFM_Times_Italic, AFM_Times_Bold, AFM_Times_BoldItalic,\
		AFM_Helvetica, AFM_Helvetica_Oblique, AFM_Helvetica_Bold, AFM_Helvetica_BoldOblique};
extern unsigned short AFM_font_array_sizes[];
