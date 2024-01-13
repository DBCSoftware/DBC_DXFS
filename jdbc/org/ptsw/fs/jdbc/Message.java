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

package org.ptsw.fs.jdbc;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.net.Socket;
import java.sql.SQLException;

final class Message {

private static final byte[] messageId = { (byte) '1', (byte) '2', (byte) '3', (byte) '4', (byte) '5', (byte) '6', (byte) '7', (byte) '8' };
private final byte[] connectionId = new byte[8];
private final byte[] fileId = new byte[8];
private final byte[] func = new byte[8];
private final byte[] size = new byte[8];
private final ByteArrayOutputStream indata = new ByteArrayOutputStream();
private byte[] outdata = null;
private final byte[] msid = new byte[8];
private final byte[] rtcd = new byte[8];
private String sRtcd;
private final Socket sock;
private final BufferedWriter bos;
private final BufferedReader bis;
private String lastErrorCode;
private int dataSize;

Message(final Socket sock, boolean useUTF8) throws IOException {
	this.sock = sock;
	if (useUTF8) {
		bos = new BufferedWriter(new OutputStreamWriter(sock.getOutputStream(), "UTF8"));
		bis = new BufferedReader(new InputStreamReader(sock.getInputStream(), "UTF8"));
		return;
	}
	bos = new BufferedWriter(new OutputStreamWriter(sock.getOutputStream()));
	bis = new BufferedReader(new InputStreamReader(sock.getInputStream()));
}

Socket getSock() {
	return this.sock;
}

void setConnectionId(final String connectionId) throws SQLException {
	internalSet(connectionId, this.connectionId);
}

void setFileId(final String fileId) throws SQLException {
	internalSet(fileId, this.fileId);
}

void setFunc(String func) throws SQLException {
	if (Driver.traceOn()) Driver.trace("Message.setFunc '" + func + "'");
	internalSet(func, this.func);
}

void setData(String data) {
	if (data == null) outdata = null;
	else outdata = data.getBytes();
}

void setData(int num) {
	outdata = String.valueOf(num).getBytes();
}

String getRtcd() {
	return sRtcd.trim();
}

byte[] getData() {
	return indata.toByteArray();
}

int getDataSize() {
	return dataSize;
}

String getLastErrorCode() {
	return lastErrorCode;
}

void sendMessage(String func) throws IOException {
	synchronized (Message.class.getClass()) {
		for (int i1 = 0; i1 < func.length(); bos.write(func.charAt(i1++)))
			;
		bos.flush();
	}
}

void sendMessage() throws IOException {
	int i1, i2;
	byte[] dataLength;
	synchronized (Message.class.getClass()) {
		for (i1 = 0; i1 < messageId.length; bos.write(messageId[i1++]))
			;
		if (Driver.traceOn()) Driver.trace("Message@sendMessage : messageId = '" + new String(messageId) + "'");
		if (connectionId[0] == 0) blankFill(connectionId);
		for (i1 = 0; i1 < connectionId.length; bos.write(connectionId[i1++]))
			;
		if (fileId[0] == 0) blankFill(fileId);
		for (i1 = 0; i1 < fileId.length; bos.write(fileId[i1++]))
			;
		for (i1 = 0; i1 < func.length; bos.write(func[i1++]))
			;
		if (outdata != null) {
			dataLength = String.valueOf(outdata.length).getBytes();
		}
		else {
			dataLength = String.valueOf(0).getBytes();
		}
		i1 = dataLength.length - 1;
		i2 = size.length - 1;
		for (; i1 > -1; i1--, i2--) {
			size[i2] = dataLength[i1];
		}
		for (; i2 > -1; i2--) {
			size[i2] = (byte) ' ';
		}
		for (i1 = 0; i1 < size.length; bos.write(size[i1++]))
			;
		if (outdata != null) {
			for (i1 = 0; i1 < outdata.length; bos.write(outdata[i1++]))
				;
			if (Driver.traceOn()) Driver.trace("Message@sendMessage : data = " + new String(outdata));
		}
		bos.flush();
	}
}

void recvMessage() throws IOException {
	int i1;
	for (i1 = 0; i1 < msid.length; msid[i1++] = (byte) bis.read())
		;
	synchronized (Message.class.getClass()) {
		if (Driver.traceOn()) Driver.trace("Message@revcMessage : msid = '" + new String(msid) + "'");
		for (i1 = 0; i1 < rtcd.length; rtcd[i1++] = (byte) bis.read())
			;
		sRtcd = new String(rtcd).toUpperCase();
		if (Driver.traceOn()) Driver.trace("Message@revcMessage : rtcd = " + sRtcd);
		for (i1 = 0; i1 < size.length; size[i1++] = (byte) bis.read())
			;
		if (Driver.traceOn()) Driver.trace("Message@revcMessage : size = '" + new String(size) + "'");
		try {
			dataSize = Integer.parseInt(new String(size).trim());
		}
		catch (NumberFormatException ex) {
			dataSize = 0;
		}

		if (Driver.traceOn()) Driver.trace("Message@recvMessage : dataSize = " + dataSize);
		indata.reset();
		if (dataSize > 0) {
			for (i1 = 0; i1 < dataSize; i1++) {
				indata.write(bis.read());
			}
		}
		String sData = indata.toString();
		if (Driver.traceOn()) Driver.trace("Message@recvMessage : sData = " + sData);
		if (sRtcd.startsWith("ERR")) {
			lastErrorCode = sRtcd.substring(3, 8).trim();
			if (Driver.traceOn()) Driver.trace("Message@recvMessage : throwing IO 1");
			throw new IOException(sData);
		}
		for (int i = 0; i < msid.length; i++) {
			if (msid[i] != messageId[i]) {
				if (Driver.traceOn()) Driver.trace("Message@recvMessage : throwing IO 2");
				throw new IOException(Driver.bun.getString("Outofsync"));
			}
		}
	}
}

private static void blankFill(byte[] ba) {
	for (int i = ba.length - 1; i > -1; ba[i--] = (byte) ' ')
		;
}

private static void internalSet(final String src, final byte[] dst) throws SQLException {
	byte[] temp = src.getBytes();
	try {
		System.arraycopy(temp, 0, dst, 0, temp.length);
		int i2 = temp.length - 1;
		for (int i1 = dst.length - 1; i1 > i2; dst[i1--] = (byte) ' ')
			;
	}
	catch (RuntimeException e) {
		throw new SQLException(e.getLocalizedMessage());
	}
}

}
