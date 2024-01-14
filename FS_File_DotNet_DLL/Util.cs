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
 *
 ******************************************************************************/

using System;
using System.Collections.Generic;
using System.Text;

namespace dbcfs
{
    public class Util {

        private static readonly string tfmt = "yyyyMMddHHmmssff";
	
        public static string currentdatetime()
        {
            return DateTime.Now.ToString(tfmt);
        }

        public static string itos(int value, int size)
        {
	        int i1;
	        char[] dest = new char[size];
	        bool negative = false;
	        if (value < 0) {
		        negative = true;
		        value = (0 - value);
	        }
	        for (i1 = size - 1; i1 >= 0; ) {
		        dest[i1--] = (char) (value % 10 + '0');
		        value = value / 10;
		        if (value == 0) break;
	        }
	        if (i1 >= 0 && negative) dest[i1--] = '-';
	        while (i1 >= 0) dest[i1--] = ' ';
	        return new String(dest);
        }


        /**
         * The stos method returns a String that is the truncated or blank filled
         * on right value of the value parameter. The length of the String returned
         * is specified by the size parameter.
         * @param value
         * @param size
         * @return
         */
        public static String stos(String value, int size)
        {
	        int i1 = 0;
	        if (value != null) i1 = value.Length;
	        if (i1 == size) return value;
	        if (i1 < size) {
		        char[] work = new char[size];
                if (i1 > 0) value.CopyTo(0, work, 0, i1); //value.getChars(0, i1, work, 0);
		        while (i1 < size) work[i1++] = ' ';
		        return new String(work, 0, size);
	        }
	        return value.Substring(0, size);
        }

        public static String stonums(String value, int size1, int size2)
        {
	        int i1, i2, i3, n1, size, srclen;
	        size = size1;
	        if (size2 > 0) size += size2 + 1;
	        char[] work = new char[size];
	        for (i1 = 0; i1 < size1; work[i1++] = ' ');
	        if (size2 == 0) work[size1 - 1] = '0';
	        else {
		        work[i1++] = '.';
		        for (i2 = size2; i2-- > 0; work[i1++] = '0');
	        }
	        if (value == null) srclen = 0;
	        else srclen = value.Length;
	        n1 = value.IndexOf('.');
	        if (n1 == -1) {  // src has no dec pt
		        i2 = srclen;
		        if (i2 < size1) {
			        i1 = 0;
			        i3 = size1 - i2;
		        }
		        else {
			        i1 = i2 - size1;
			        i3 = 0;
		        }
	        }
	        else if (size2 == 0) {  // dest has no dec pt
		        i2 = n1;
		        if (i2 < size1) {
			        i1 = 0;
			        i3 = size1 - i2;
		        }
		        else {
			        i1 = i2 - size1;
			        i3 = 0;
		        }
	        }
	        else {  // both have dec pt
		        i2 = n1;
		        if (n1 < size1) {  // src has fewer digits left of dec pt
			        i1 = 0;
			        i3 = size1 - n1;
		        }
		        else {
			        i1 = n1 - size1;
			        i3 = 0;
		        }
	        }
            value.CopyTo(i1, work, i3, i2 - i1); //value.getChars(i1, i2, work, i3);
	        for (i1 = 0; i1 < size && work[i1] == ' '; i1++);
	        if (i1 == size) return zeronums(size1, size2);
	        while (work[i1] == '-') {
		        if (++i1 == size) return zeronums(size1, size2);
		        if (work[i1] != '0') break;
		        work[i1 - 1] = ' ';
		        work[i1] = '-';
	        }
	        while (work[i1] == '0') {
		        if (++i1 == size) return zeronums(size1, size2);
		        work[i1 - 1] = ' ';
	        }
	        for (i3 = 0; i1 < size && work[i1] >= '0' && work[i1] <= '9'; i1++) i3 = 1;
	        if (i1 < size) {
		        if (work[i1] == '.') i1++;
		        for (; i1 < size && work[i1] >= '0' && work[i1] <= '9'; i1++) i3 = 1;
		        if (i1 < size) return zeronums(size1, size2);
	        }
	        if (i3 == 0) return zeronums(size1, size2);
	        n1++;
	        if (size2 > 0 && n1 > 0 && n1 < srclen) {
		        i3 = srclen;
		        if (i3 - n1 > size2) i3 = n1 + size2;
		        //value.getChars(n1, i3, work, size1 + 1);
                value.CopyTo(n1, work, size1 + 1, i3 - n1);
	        }
	        return new String(work, 0, size);
        }

        public static String zeronums(int size1, int size2)
        {
	        if (size2 > 0) return "                     .000000000000000000000".Substring(21 - size1, size2 + 23);
	        switch (size1) {
	        case  1: return "0";
	        case  2: return " 0";
	        case  3: return "  0";
	        case  4: return "   0";
	        case  5: return "    0";
	        case  6: return "     0";
	        case  7: return "      0";
	        case  8: return "       0";
	        case  9: return "        0";
	        case 10: return "         0";
	        }
	        return "                    0".Substring(21 - size1);
        }

        /**
         * The itoszf method returns a String that is the integer value of the value parameter and is the
         * length specified by the size parameter. The value is truncated or zero filled on the left to
         * fit the length specified.
         */
        public static String itoszf(int number, int count)
        {
	        int i1;
	        if (count < 1) throw new Exception("Invalid parms for Util.itoszf()");
	        char[] dest = new char[count];
	        bool negative = false;
	        if (number < 0) {
		        negative = true;
		        number = (0 - number);
	        }
	        for (i1 = count - 1; i1 >= 0; ) {
		        dest[i1--] = (char) (number % 10 + '0');
		        number = number / 10;
		        if (number == 0) break;
	        }
	        if (i1 >= 0 && negative) dest[i1--] = '-';
	        while (i1 >= 0) dest[i1--] = '0';
	        return new String(dest);
        }

        public static void itoca(int number, char[] dest, int destoff, int count)
        {
	        int i1, i2;
	        bool negative = false;
	        i2 = destoff + count - 1;
	        if (count < 1 || i2 >= dest.Length) throw new Exception("Invalid parms for Util.itoca()");
	        if (number < 0) {
		        negative = true;
		        number = (0 - number);
	        }
	        for (i1 = 0; i1 < count; i1++) {
		        dest[i2--] = (char) (number % 10 + '0');
		        number = number / 10;
		        if (number == 0) break;
	        }
	        if (i1 < count && negative) {
		        dest[i2--] = '-';
		        i1++;
	        }
	        while (i1++ < count) dest[i2--] = ' ';
        }

        public static int catoi(char[] source, int sourceoff, int count)
        {
	        int i1, i2, number;
	        char c1;
	        i1 = sourceoff + count - 1;
	        if (count < 1 || i1 >= source.Length) throw new Exception("Invalid parms for Util.itcao()");
	        c1 = source[i1--];
	        count--;
	        if (c1 < '0' || c1 > '9') throw new Exception("Invalid character found in Util.catoi()");
	        number = c1 - '0';
	        i2 = 10;
	        while (count-- > 0) {
		        c1 = source[i1--];
		        if (c1 >= '0' && c1 <= '9') {
			        number = number + (c1 - '0') * i2;
			        i2 *= 10;
			        continue;
		        }
		        else if (c1 == '-') number = (0 - number);
		        else if (c1 != ' ') throw new Exception("Invalid character found in Util.catoi()");
		        break;
	        }
	        while (count-- > 0) {
		        if (source[i1--] != ' ') throw new Exception("Invalid character found in Util.catoi()");
	        }
	        return number;
        }

    }
}