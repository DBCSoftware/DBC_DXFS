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
Class ConnectionException implements basic exception mechanism for connections.
*/
public final class ConnectionException extends Exception {

private static final long serialVersionUID = -4498648158578343809L;
private String info;

/**
Constructor.
*/
ConnectionException(String info)
{
	this.info = info;
}

public ConnectionException(String string, Exception e2) {
	super(string, e2);
}

/**
Return the error information.
*/
public String getinfo()
{
	return info;
}

/**
Return the error information.
*/
public String toString()
{
	return info;
}

}
