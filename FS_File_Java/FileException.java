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

package com.dbcswc.fs;

/**
 * Class FileException implements file class exceptions.
 */
public final class FileException extends Exception {

	private static final long serialVersionUID = 9134111949401124739L;
	private int error;
	private String info;

	/**
	 * Constructor with just an error number.
	 */
	public FileException(int error) {
		this.error = error;
	}

	/**
	 * Constructor with an error number and extra information
	 */
	public FileException(int error, String info) {
		this.error = error;
		this.info = info;
	}

	/**
	 * Return the error code.
	 */
	public int geterror() {
		return error;
	}

	/**
	 * Return the extra information.
	 */
	public String getinfo() {
		return info;
	}

	/**
	 * toString.
	 */
	public String toString() {
		return "FileException:" + error + ((info == null) ? "" : (":" + info));
	}
}
