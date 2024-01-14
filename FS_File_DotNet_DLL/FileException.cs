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
using System.Linq;
using System.Text;

namespace dbcfs
{
    /**
     * Class FileException implements file class exceptions.
     */
    public class FileException : System.Exception
    {

        private int error;
        private string info;

        /**
        Constructor with just an error number.
        */
        public FileException(int error)
        {
            this.error = error;
        }

        /**
        Constructor with an error number and extra information
        */
        public FileException(int error, String info)
        {
            this.error = error;
            this.info = info;
        }

        /**
        Return the error code.
        */
        public int geterror()
        {
            return error;
        }

        /// <summary>
        /// Return the extra information.
        /// </summary>
        /// <returns></returns>
        public String getinfo()
        {
            return info;
        }

        /**
         * ToString.
         */
        public override String ToString()
        {
            return "FileException:" + error + ((info == null) ? "" : (":" + info));
        }
    }
}