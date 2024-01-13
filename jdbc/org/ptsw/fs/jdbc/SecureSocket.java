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

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.net.SocketException;
import java.security.SecureRandom;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;

import javax.net.ssl.HandshakeCompletedEvent;
import javax.net.ssl.HandshakeCompletedListener;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;

public final class SecureSocket extends Socket {

	private SSLSocket s;
	private static boolean debug = false;
	
	public static final String[] cipher_suites = {
			"TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384", 
			"TLS_DHE_RSA_WITH_AES_128_CBC_SHA",
			"TLS_DHE_DSS_WITH_AES_128_CBC_SHA",
			"TLS_RSA_WITH_AES_128_CBC_SHA",
			"TLS_DHE_RSA_WITH_AES_256_CBC_SHA",
			"TLS_DHE_DSS_WITH_AES_256_CBC_SHA",
			"TLS_RSA_WITH_AES_256_CBC_SHA"
		};
	
	/**
	 * Secure anonymous connection
	 **/
	public SecureSocket(String host, int port) throws Exception {
		this(null, host, port);
	}
	
	/**
	 * Secure anonymous connection
	 **/
	public SecureSocket(Socket s1, String host, int port) throws Exception {
		SSLSocketFactory sslFact;
		SSLContext sslc;
		SecureRandom srand = SecureRandom.getInstance("SHA1PRNG");
		srand.setSeed(System.currentTimeMillis());
	
		if (debug) {
			System.setProperty("javax.net.debug", "all");
		}
	
		/**
		 * Java insists on authenticating the server.
		 * This defeats that. We still get a secure key exchange and encrypted data flow.
		 */
		TrustManager[] trustAllCerts = new TrustManager[] { new X509TrustManager() {
			public void checkClientTrusted(X509Certificate[] arg0, String arg1) throws CertificateException {}
	
			public void checkServerTrusted(X509Certificate[] chain, String authType) throws CertificateException {}
	
			public X509Certificate[] getAcceptedIssuers() {
				return new X509Certificate[0];
			}
		} };
	
		sslc = SSLContext.getInstance("TLSv1.2");
		sslc.init(null, trustAllCerts, srand);
		sslFact = sslc.getSocketFactory();
		s = null;
		if (s1 != null) {
			s = (SSLSocket) sslFact.createSocket(s1, host, port, true);
		}
		else {
			s = (SSLSocket) sslFact.createSocket(host, port);
		}
		s.setEnabledCipherSuites(cipher_suites);
		s.setUseClientMode(true);
		s.setNeedClientAuth(false);
		if (debug) {
			s.addHandshakeCompletedListener(new HandshakeCompletedListener() {
				@Override
				public void handshakeCompleted(HandshakeCompletedEvent hce) {
					System.out.println("cipher suite: " + hce.getCipherSuite());
				}
			});
		}
	}

	@Override
	public void setSoLinger(boolean on, int linger) throws SocketException {
		if (s != null) s.setSoLinger(on, linger);
	}
	
	@Override
	public void setTcpNoDelay(boolean v1) throws SocketException {
		if (s != null) s.setTcpNoDelay(v1);
	}
	
	/**
	 * Get InputStream
	 *
	 * @return InputStream
	 * @exeception IOException if any IO error occurs
	 **/
	@Override
	public InputStream getInputStream() throws IOException {
		return (s == null) ? null : s.getInputStream();
	}
	
	/**
	 * Get OutputStream
	 *
	 * @return OutputStream
	 * @exeception IOException if any IO error occurs
	 **/
	@Override
	public OutputStream getOutputStream() throws IOException {
		return (s == null) ? null : s.getOutputStream();
	}
	
	/**
	 * Close socket
	 *
	 * @return void
	 * @exeception IOException if any IO error occurs
	 **/
	@Override
	public void close() throws IOException {
		if (s != null) {
			s.close();
			s = null;
		}
	}
	
	/**
	 * Turn on debug information
	 *
	 * @param value If you pass true to this function, tons of information about the handshake,
	 * cipher suites used, algorithms, keys, certificates, etc. get displayed to System.out 
	 **/
	public static void setDebug(boolean value) {
		debug = value;
	}

}
