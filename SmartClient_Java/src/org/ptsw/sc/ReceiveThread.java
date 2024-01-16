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

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.concurrent.BlockingQueue;

import org.ptsw.sc.Client.SomethingToDo;
import org.ptsw.sc.xml.Element;
import org.ptsw.sc.xml.XMLInputStream;

public class ReceiveThread implements Runnable {

	private final InputStream sin;
	private final Socket s1;
	private ServerSocket ssocket;
	private Element inbuf = null;
	private final BlockingQueue<Client.SomethingToDo> inOutQueue;
	
	/**
	 * Constructor
	 * Just save the arguments
	 * 
	 * @param s1 The incoming socket
	 * @param ssocket The outgoing socket
	 * @param inOutQueue2	The Queue for 'things to do'
	 * @throws IOException 
	 */
	ReceiveThread(Socket s1, ServerSocket ssocket,
			BlockingQueue<SomethingToDo> inOutQueue2) throws IOException
	{
		this.s1 = s1;
		this.sin = new BufferedInputStream(s1.getInputStream());
		this.ssocket = ssocket;
		this.inOutQueue = inOutQueue2;
	}
	
	@Override
	public void run() {
		int i1;
		char c1;
		//boolean first = true; 
		StringBuilder length = new StringBuilder(7);
		String s2;
		Thread.currentThread().setName("ReceiveThread");
		if (Client.isDebug()){
			System.out.println("ReceiveThread Running!");
			System.out.flush();
		}
		for ( ; ; ) {
			try {
				/* it appears that BufferedInputStream.available() never returns > 0 */
				/* when encryption is used, unless a read() is done first, so that   */
				/* is why the boolean "first" is used.                               */ 
				//if (Client.IsDebug())System.out.println("ReceiveThread - entering while loop");
				//while (!first && sin.available() <= 0 && !Client.getAbortflag()) {
				//	Thread.yield();
				//}
				//if (Client.IsDebug())System.out.println("ReceiveThread - leaving while loop");
				if (Client.getAbortflag()) {
					try {
						s1.close();
					}
					catch (Exception e1) { }
					ssocket = null;
					if (Client.isDebug())System.out.println("ReceiveThread Stopping! (A)");
					return;
				}
				s2 = "";
				length.setLength(0);
				for (i1 = 0; i1 < 16; i1++) {
					if (i1 > 7) { /* length */
						c1 = (char) sin.read();
						length.append(c1);
					}
					else {
						s2 += (char) sin.read(); /* sync message */
					}
				}
				//if (first) first = false; 
				Client.getDefault().setSyncstr(s2);
				byte[] b = new byte[Integer.parseInt(length.toString().trim())];
				i1 = 0;
				do {
					i1 += sin.read(b, i1, b.length - i1);
				}
				while (i1 != b.length);
				XMLInputStream xmlin = new XMLInputStream(new ByteArrayInputStream(b)); 			
				while (true) {
					try {
						inbuf = xmlin.readElement();
					}
					catch (Exception e0) {
						if (Client.IsTrace() || Client.isDebug()) e0.printStackTrace();
						System.err.println("ERROR: Failure reading XML " + (new String(b)).trim());
						xmlin.close();
						break;
					}	
					if (inbuf == null) break;
					Client.SomethingToDo todo = new Client.SomethingToDo(Client.SomethingToDo.Type.incoming, inbuf);
					if (Client.IsTrace()) {
						String tstr = todo.element.toString();
						if (!tstr.startsWith("<storeimagebits")) {
							System.out.println("inbuf: " + tstr);
							System.out.flush();
						}
						else {
							System.out.println("inbuf: " + tstr.substring(0, 80));
							System.out.flush();
						}
					}
					inOutQueue.add(todo);
					// The 'quit' Element is always sent by itself and nothing ever follows it
					if (todo.element.name.equals("quit")) {
						Client.setAbort();
						break;
					}
				}
				xmlin.close();
			}
			catch (IOException ioe) {
				if (Client.getAbortflag()) continue;
				if (Client.IsTrace() || Client.isDebug()) ioe.printStackTrace();
				break;
			}
			catch (Exception e2) {
				if (Client.IsTrace() || Client.isDebug()) e2.printStackTrace();
				break;
			}
		}
		try {
			if (ssocket != null) s1.close();
		}
		catch (Exception e1) { }
		ssocket = null;
		if (Client.isDebug())System.out.println("ReceiveThread Stopping! (B)");
	}

}
