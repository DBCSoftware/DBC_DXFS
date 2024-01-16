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

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Arrays;
import java.util.Objects;

import org.ptsw.sc.xml.Attribute;
import org.ptsw.sc.xml.Element;


public class FileTransferSupport {

	public static final String FILETRANSFERCLIENTGETELEMENTTAG = "clientgetfile";
	public static final String FILETRANSFERCLIENTPUTELEMENTTAG = "clientputfile";
	public static final String FILETRANSFERCLIENTDIRTAGNAME = "filetransferclientdir";

	public static final String FILETRANSFERCLIENTSIDEFILENAMETAG = "csfname";
	public static final String FILETRANSFERMORETOCOMETAG = "moretocome";
	public static final String FILETRANSFERBITSTAG = "bits";

	private static final int chunk = (0x10000 - 16);
	private static final byte[] byteArrayForBinary = new byte[chunk];
	private static final byte[] byteArrayForEncoded = new byte[chunk * 4 / 3 + 64];
	private static File directory;
	private enum State {idle, busy};
	private static State state = State.idle;
	private static FileOutputStream incomingFile;
	private static FileInputStream outgoingFile;
	private static long outgoingFileLength, bytesRemainingToBeSent;

	static void ClientServerFileTransfer(Element element) throws IOException {
		Objects.requireNonNull(element, "element cannot be null");
		if (element.name.equals(FILETRANSFERCLIENTGETELEMENTTAG)) ClientGetFile(element);
		else if (element.name.equals(FILETRANSFERCLIENTPUTELEMENTTAG))  ClientPutFile(element);
	}
	
	private static void ClientGetFile(Element element) throws IOException {
		Objects.requireNonNull(element, "element cannot be null");
		boolean moretocome = element.getAttributeValue(FILETRANSFERMORETOCOMETAG).charAt(0) == 'Y';
		String bits = element.getAttributeValue(FILETRANSFERBITSTAG);
		if (state == State.idle) {
			incomingFile = new FileOutputStream(new File(directory,
					element.getAttributeValue(FILETRANSFERCLIENTSIDEFILENAMETAG)));
			if (bits != null && bits.length() >= 1) {
				incomingFile.write(byteArrayForBinary, 0, fromprintable(bits.getBytes()));
			}
			if (moretocome) state = State.busy;
			else incomingFile.close();
		}
		else {
			if (bits != null && bits.length() >= 1) {
				incomingFile.write(byteArrayForBinary, 0, fromprintable(bits.getBytes()));
			}
			if (!moretocome) {
				incomingFile.close();
				state = State.idle;
			}
		}
		Client.SendResult(false);
	}
	
	private static void ClientPutFile(Element element) throws IOException {
		int bytesRead;
		int encodedLength;
		Element e1;
		if (Client.IsTrace()) {
			System.out.println("Entering ClientPutFile in FileTransferSupport");
			System.out.println("\tstate = " + ((state == State.idle) ? "Idle" : "Busy"));
		}
		if (state == State.idle) {
			boolean moretocome;
			String fileName = element.getAttributeValue(FILETRANSFERCLIENTSIDEFILENAMETAG);
			File file = new File(directory, fileName);
			try {
				outgoingFile = new FileInputStream(file);
			}
			catch (FileNotFoundException e) {
				Element e2 = new Element("r");
				Attribute a1 = new Attribute("e", "fio");
				Attribute a2 = new Attribute("xtra", "-22");
				e2.addAttribute(a1);
				e2.addAttribute(a2);
				Client.SendElement(e2);
				return;
			}
			if (Client.IsTrace()) {
				System.out.println("\tfile found, " + file.getCanonicalPath());
			}
			e1 = new Element("r");
			bytesRemainingToBeSent = outgoingFileLength = file.length();
			moretocome = outgoingFileLength > chunk;
			if (Client.IsTrace()) {
				System.out.println("\toutgoingFileLength, " + outgoingFileLength);
				System.out.println("\tmoretocome, " + Boolean.toString(moretocome));
			}
			if (moretocome) {
				e1.addAttribute(new Attribute(FILETRANSFERMORETOCOMETAG, "Y"));
				bytesRead = outgoingFile.read(byteArrayForBinary);
				bytesRemainingToBeSent -= bytesRead;
				encodedLength = makeprintable(byteArrayForBinary, bytesRead);
				e1.addAttribute(new Attribute(FILETRANSFERBITSTAG, new String(byteArrayForEncoded, 0, 
						encodedLength)));
				state = State.busy;
			}
			else {
				if (outgoingFileLength < 1) {
					e1.addAttribute(new Attribute(FILETRANSFERBITSTAG, ""));
					e1.addAttribute(new Attribute(FILETRANSFERMORETOCOMETAG, "N"));
				}
				else {
					bytesRead = outgoingFile.read(byteArrayForBinary, 0, (int)outgoingFileLength);
					if (Client.IsTrace()) {
						System.out.println("\tbytesRead, " + bytesRead);
					}
					bytesRemainingToBeSent -= bytesRead;
					encodedLength = makeprintable(byteArrayForBinary, bytesRead);
					e1.addAttribute(new Attribute(FILETRANSFERBITSTAG, new String(byteArrayForEncoded, 0,
							encodedLength)));
					if (bytesRemainingToBeSent < 1) {
						e1.addAttribute(new Attribute(FILETRANSFERMORETOCOMETAG, "N"));
						outgoingFile.close();
					}
					else {
						e1.addAttribute(new Attribute(FILETRANSFERMORETOCOMETAG, "Y"));
						state = State.busy;
					}
				}
			}
		}
		else {
			e1 = new Element("r");
			bytesRead = outgoingFile.read(byteArrayForBinary);
			bytesRemainingToBeSent -= bytesRead;
			encodedLength = makeprintable(byteArrayForBinary, bytesRead);
			e1.addAttribute(new Attribute(FILETRANSFERBITSTAG, new String(byteArrayForEncoded, 0,
					encodedLength)));
			if (bytesRemainingToBeSent < 1) {
				e1.addAttribute(new Attribute(FILETRANSFERMORETOCOMETAG, "N"));
				outgoingFile.close();
				state = State.idle;
			}
			else {
				e1.addAttribute(new Attribute(FILETRANSFERMORETOCOMETAG, "Y"));
			}
		}
		Client.SendElement(e1);
	}

	/**
	 * Set the directory to be used for file transfer
	 * @param dir The directory path as a String
	 */
	static void setClientSideDirectory(String dir) {
		directory = new File(dir);
	}

	/**
	 * Convert ASCII 0x00-0xFF data to printable characters (0x3F-7E)
	 * 4 Characters generated for every 3
	 *
	 * @param in	The array of binary bytes to be converted
	 * @param len	How many bytes to be converted
	 * @return		The count of printable bytes output
	 */
	static int makeprintable(byte[] in, int len) 
	{
		int i1, i2;
		byte c1, c2, c3;
		int mod = len % 3;
		byte[] inCopy;
		
		if (mod == 0) inCopy = in;
		else inCopy = Arrays.copyOf(in, (mod == 0) ? len : len + (3 - mod));

		if (Client.IsTrace()) {
			System.out.println("Entering makeprintable in FIleTransferSupport");
			System.out.println("\tlen=" + len + ", mod=" + mod + ", inCopy.length=" + inCopy.length);
		}
		//while (in.length % 3 > 0) in.append(0);
		for (i1 = 0, i2 = 0; i1 + 2 < len; ) {
			c1 = inCopy[i1++];
			c2 = inCopy[i1++];
			c3 = inCopy[i1++];
			byteArrayForEncoded[i2++] = (byte) ((byte)(c1 & 0x3f) + 0x3f);
			byteArrayForEncoded[i2++] = (byte) (((byte)(c1 >> 6) & 0x03) + (byte)((byte)(c2 & 0x0f) << 2) + 0x3f);
			byteArrayForEncoded[i2++] = (byte) (((byte)(c2 >> 4) & 0x0f) + (byte)((byte)(c3 & 0x03) << 4) + 0x3f);
			byteArrayForEncoded[i2++] = (byte) (((byte)(c3 >> 2) & 0x3f) + 0x3f);
		}
		if (mod == 1) { /* need two more bytes output */
			c1 = inCopy[i1++];
			c2 = 0;
			byteArrayForEncoded[i2++] = (byte) ((byte)(c1 & 0x3f) + 0x3f);
			byteArrayForEncoded[i2++] = (byte) (((byte)(c1 >> 6) & 0x03) + ((byte)(c2 & 0x0f) << 2) + 0x3f);
		}
		else if (mod == 2) { /* need three more bytes output */
			c1 = inCopy[i1++];
			c2 = inCopy[i1++];
			c3 = 0;
			byteArrayForEncoded[i2++] = (byte) ((byte)(c1 & 0x3f) + 0x3f);
			byteArrayForEncoded[i2++] = (byte) (((byte)(c1 >> 6) & 0x03) + (byte)((byte)(c2 & 0x0f) << 2) + 0x3f);
			byteArrayForEncoded[i2++] = (byte) (((byte)(c2 >> 4) & 0x0f) + (byte)((byte)(c3 & 0x03) << 4) + 0x3f);
		}
		return i2;
	}

	/**
	 * Convert printable characters to Binary
	 * 3 Characters generated for every 4 input characters
	 * Borrowed this code from ImageSupport and hacked it
	 */
	private static int fromprintable(byte[] in)
	{
		int i1, i2, ilen = in.length;
		byte c1, c2, c3, c4;	
		int mod = in.length % 4;

		for (i1 = 0, i2 = 0; i1 + 3 < ilen; ) {
			c1 = (byte) (in[i1++] - 0x3f); // '?'
			c2 = (byte) (in[i1++] - 0x3f);
			c3 = (byte) (in[i1++] - 0x3f);
			c4 = (byte) (in[i1++] - 0x3f);
			byteArrayForBinary[i2++] = (byte) ((byte)(c1 & 0x3f) + ((byte)(c2 & 0x03) << 6));
			byteArrayForBinary[i2++] = (byte) ((byte)((byte)(c2 & 0x3c) >> 2) + (byte)((byte)(c3 & 0x0f) << 4));
			byteArrayForBinary[i2++] = (byte) ((byte)((byte)(c3 & 0x30) >> 4) + (byte)((byte)(c4 & 0x3f) << 2));
		}
		if (i1 < ilen && i1 + 1 != ilen) {
			/* either 2 or 3 additional characters of input */
			/* means one or two additional chars output */
			c1 = (byte)(in[i1++] - 0x3f);
			c2 = (byte)(in[i1++] - 0x3f);
			if (i1 < ilen) c3 = (byte)(in[i1] - 0x3f);
			else c3 = 0;
			byteArrayForBinary[i2++] = ((byte)((c1 & 0x3F) + ((c2 & 0x03) << 6)));
			if (mod == 3) byteArrayForBinary[i2++] = ((byte)(((c2 & 0x3C) >> 2) + (c3 << 4)));
		}
		return i2;
	}

}
