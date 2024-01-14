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
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

namespace dbcfs
{

    /// <summary>
    /// Implements a connection with a DB/C FS server.
    /// </summary>
    public class Connection
    {
        private static int CONNECTPORT = 9584;
        private const string HelloString = "                HELLO          0";
        private const string StartString = "                START   ";
        private const string aliveAck = "ALIVEACK";

        private string FSVersion;
        private int FSMajorVersion;
        private byte[] sendmsghdrtemplate;
        private byte[] sendmsghdr;
        private byte[] sendmsgdata;
        private byte[] recvmsghdr;
        private byte[] recvmsgdata;
        private int recvlength;
        private ManualResetEvent s_sockConnected;
        private TcpClient socket;
        private Stream netstream;
        private bool keepalive, connected;
        private string server;
        private string database;
        private string user;
        private Thread keepAliveThread;

        /// <summary>
        /// Constructor with no port arguments, no authentication
        /// </summary>
        /// <param name="server"></param>
        /// <param name="database"></param>
        /// <param name="user"></param>
        /// <param name="password"></param>
        public Connection(string server, string database, string user, string password)
            : this(server, -1, -1, database, user, password, false, null) { }

        /// <summary>
        /// Constructor with port arguments, no authentication
        /// </summary>
        /// <param name="server"></param>
        /// <param name="sport"></param>
        /// <param name="lport"></param>
        /// <param name="database"></param>
        /// <param name="user"></param>
        /// <param name="password"></param>
        public Connection(string server, Int32 sport, Int32 lport, string database, string user, string password)
            : this(server, sport, lport, database, user, password, false, null) { }

        /// <summary>
        /// Constructor with no port arguments but authentication.
        /// </summary>
        /// <param name="server"></param>
        /// <param name="database"></param>
        /// <param name="user"></param>
        /// <param name="password"></param>
        /// <param name="encrypt">always false for now</param>
        /// <param name="authfile">not used</param>
        public Connection(string server, string database, string user, string password, bool encrypt, string authfile)
	        : this(server, -1, -1, database, user, password, false, authfile) { }


        /// <summary>
        /// Constructor with port arguments and authentication
        /// </summary>
        /// <param name="server"></param>
        /// <param name="serverport"></param>
        /// <param name="localport"></param>
        /// <param name="database"></param>
        /// <param name="user"></param>
        /// <param name="password"></param>
        /// <param name="encrypt">always false for now</param>
        /// <param name="authfile">not used</param>
        public Connection(string server, Int32 serverport, Int32 localport, string database, string user, string password, bool encrypt, string authfile)
        {
            int i1, i2;
            TcpListener s_sock = null;

            if (serverport <= 0) serverport = CONNECTPORT;
            string greeting = getgreeting(server, serverport, false, authfile);
            if (greeting.StartsWith("DB/C FS "))
            {
                FSVersion = greeting.Substring(8);
                FSMajorVersion = parseMajorVersion(FSVersion);
            }
            else throwerror("Server (" + server + ") 'HELLO' response invalid: " + greeting);
            if (FSMajorVersion < 4) throwerror("Not compatible with FS version less than 4");

            recvmsghdr = new byte[24];
            sendmsghdr = new byte[40];
            sendmsgdata = new byte[256];
            sendmsghdrtemplate = new byte[40];
            Buffer.SetByte(sendmsghdrtemplate, 0, (byte)'Z');
            for (i1 = 1; i1 < 40; Buffer.SetByte(sendmsghdrtemplate, i1++, (byte)' ')) ;
            if (localport != 0)
            {
                /*
                 * This is the normal or default behavior.
                 * We open a server socket and listen for the
                 * FS server to come back to us
                 */
                s_sock = new TcpListener(IPAddress.Any, (localport == -1) ? 0 : localport);
                s_sockConnected = new ManualResetEvent(false);
                s_sockConnected.Reset();
                s_sock.Start();
                if (localport == -1) localport = ((IPEndPoint)s_sock.LocalEndpoint).Port;
                s_sock.BeginAcceptTcpClient(new AsyncCallback(DoAcceptFSServerCallback), s_sock);
            }
            socket = new TcpClient();
            socket.Connect(server, serverport);
            socket.NoDelay = true;
            netstream = new BufferedStream(socket.GetStream());
            netstream.WriteByte(0x1);
            netstream.WriteByte(0x2);
            netstream.WriteByte(0x3);
            netstream.WriteByte(0x4);
            netstream.WriteByte(0x5);
            netstream.WriteByte(0x6);
            netstream.WriteByte(0x7);
            netstream.WriteByte(0x8);
            netstream.Write(Encoding.UTF8.GetBytes(StartString), 0, Encoding.UTF8.GetByteCount(StartString));
			string cmd = localport.ToString() + ' ' + user;
            string lengthOfCmdAsString = cmd.Length.ToString();
            i1 = lengthOfCmdAsString.Length;
            string cmd2 = "        ".Substring(i1) + lengthOfCmdAsString;
            netstream.Write(Encoding.UTF8.GetBytes(cmd2), 0, Encoding.UTF8.GetByteCount(cmd2));
            netstream.Write(Encoding.UTF8.GetBytes(cmd), 0, Encoding.UTF8.GetByteCount(cmd));
            netstream.Flush();
            Buffer.SetByte(sendmsghdrtemplate, 0, 0x1); // prevent error in recv()
            string rtcd = recv();
            Buffer.SetByte(sendmsghdrtemplate, 0, (byte)'Z');
            if (!rtcd.Equals("OK"))
            {
                Decoder dcode1 = Encoding.UTF8.GetDecoder();
                int n1 = dcode1.GetCharCount(recvmsgdata, 0, recvlength);
                char[] chars = new char[n1];
                dcode1.GetChars(recvmsgdata, 0, recvlength, chars, 0);
                throwerror("Unable to START server:port " + server + "/" + serverport
                    + " : " + new String(chars));
            }
            netstream.Close();
            socket.Close();

            /*
             * Now establish the secondary connection,
             * between us and the dbcfsrun process
             */
            if (localport == 0)
            {
                Decoder dcode1 = Encoding.UTF8.GetDecoder();
                int n1 = dcode1.GetCharCount(recvmsgdata, 0, recvlength);
                char[] chars = new char[n1];
                dcode1.GetChars(recvmsgdata, 0, recvlength, chars, 0);
                socket = new TcpClient();
                socket.Connect(server, Int32.Parse(new String(chars)));
            }
            else
            {
                /*
                 * Default: Wait for the FS server to come back to us
                 */
                s_sockConnected.WaitOne();
            }
            netstream = new BufferedStream(socket.GetStream());
            socket.NoDelay = true;
            socket.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.KeepAlive, true);

            StringBuilder sbdata = new StringBuilder(user);
            sbdata.Append(" " + password);
            sbdata.Append(" FILE");
            sbdata.Append(" " + database);
            sbdata.Append(" CLIENT");
            send(null, "CONNECT", sbdata.ToString());

            rtcd = recv();
            if (!rtcd.Equals("OK"))
            {
                Decoder dcode1 = Encoding.UTF8.GetDecoder();
                int n1 = dcode1.GetCharCount(recvmsgdata, 0, recvlength);
                char[] chars = new char[n1];
                dcode1.GetChars(recvmsgdata, 0, recvlength, chars, 0);
                throwerror(
                    "Unable to connect to server/database: " + server + "/" + database + ":" +
                    ((recvmsgdata == null) ? "null" : new String(chars))
                );
            }
            i2 = recvlength;
            for (i1 = 0; i1 < 8; i1++) sendmsghdrtemplate[8 + i1] = recvmsgdata[i1];

		    if (i2 == 16) {
			    keepalive = true;
                Decoder dcode1 = Encoding.UTF8.GetDecoder();
                int n1 = dcode1.GetCharCount(recvmsgdata, 8, 8);
                char[] chars = new char[n1];
                dcode1.GetChars(recvmsgdata, 8, 8, chars, 0);
                int timeout = Int32.Parse(new String(chars)); //TODO = Integer.parseInt(new String(recvmsgdata, 8, 8).trim());

                keepAliveThread = new Thread(sendKeepAlive);
                keepAliveThread.Start(timeout);
		    }
	        connected = true;
	        this.server = server;
	        this.database = database;
	        this.user = user;
        }

        public void disconnect()
        {
	        lock (this) {
	            try {
		            send(null, "DISCNCT");
		            recv();
		            if (keepalive) keepAliveThread.Interrupt();		
		            netstream.Close();
		            socket.Close();
	            }
	            catch (Exception) { }
	        }
	        netstream = null;
	        socket = null;
	        connected = false;
        }


        /// <summary>
        /// Thread that sends the 'keep alive' messages
        /// </summary>
        /// <param name="oto">Time interval in seconds</param>
        private void sendKeepAlive(object oto)
        {
            int timeInMilliseconds = (int)oto * 1000;
            byte[] bytes = Encoding.UTF8.GetBytes(aliveAck);
            int byteCount = Encoding.UTF8.GetByteCount(aliveAck);
            for (; ; )
            {
                try
                {
                    Thread.Sleep(timeInMilliseconds);
                    sendack(bytes, byteCount);
                }
                catch (Exception) { break; }
            }
        }

        private void sendack(byte[] bytes, int byteCount)
        {
	        lock (this) {
		        try {
                    netstream.Write(bytes, 0, byteCount);
                    netstream.Flush();
		        }
		        catch (IOException) {
			        throwerror("IO exception encountered in sendack");
		        }
	        }
        }



        /**
         * Send a message.
         */
        internal void send(string fsid, string func)
        {
	        send(fsid, func, (string) null);
        }

        /**
         * Send a message.
         */
        internal void send(string fsid, string func, char[] data, int datalen)
        {
	        send(fsid, func, new String(data, 0, datalen));
        }

        /**
         * Send a message.
         */
        internal void send(string fsid, string func, char[] data1, int data1len, char[] data2, int data2len)
        {
	        send(fsid, func, new String(data1, 0, data1len) + new String(data2, 0, data2len));
        }

        internal void send(string fsid, string func, string data)
        {
	        int i1, i2, i3;
	        lock (this) {
		        if (sendmsghdrtemplate[0] == 'Z') sendmsghdrtemplate[0] = (byte) 'A';
		        else sendmsghdrtemplate[0]++;
                Buffer.BlockCopy(sendmsghdrtemplate, 0, sendmsghdr, 0, 40);
		        //for (i1 = 0; i1 < 40; i1++) sendmsghdr[i1] = sendmsghdrtemplate[i1];
                if (fsid != null && fsid.Length == 8)
                {
                    char[] fsa = fsid.ToCharArray();
                    for (i1 = 0; i1 < 8; i1++) sendmsghdr[16 + i1] = (byte)fsa[i1];
                }
                char[] fca = func.ToCharArray();
		        for (i1 = 0, i2 = fca.Length; i1 < i2; i1++) {
			        sendmsghdr[24 + i1] = (byte) fca[i1];
		        }
		        if (data == null) i1 = i2 = 0;
		        else i1 = i2 = data.Length;
		        for (i3 = 39; i3 >= 32; i3--) {
			        sendmsghdr[i3] = (byte) (i1 % 10 + '0');
			        i1 /= 10;
		        }
		        try {
			        netstream.Write(sendmsghdr, 0, 40);
			        if (i2 > 0) {
                        char[] dataArray = data.ToCharArray();
				        if (i2 > sendmsgdata.Length) sendmsgdata = new byte[i2];
                        for (i1 = 0; i1 < i2; i1++) sendmsgdata[i1] = (byte) dataArray[i1];
				        netstream.Write(sendmsgdata, 0, i2);
			        }
                    netstream.Flush();
		        }
		        catch (IOException) {
			        throwerror("IO exception encountered in send");
		        }
	        }
        }

        internal string recv(char[] data)
        {
	        string rtcd = recv();
	        if (data == null && recvlength > 0) data = new char[recvlength];
            Array.Copy(recvmsgdata, 0, data, 0, recvlength);
	        //for (int i1 = 0; i1 < recvlength; i1++) data[i1] = (char) recvmsgdata[i1];
	        return rtcd;
        }

        internal string recv()
        {
	        int i1, i2, len;
	        char c1;
	        try {
		        for (i1 = i2 = 0;;) {
			        i2 = netstream.Read(recvmsghdr, i1, recvmsghdr.Length - i1);
			        if (i2 == -1) throwerror("EOF encountered in recv");
			        i1 += i2;
			        if (i1 == recvmsghdr.Length) break;				
		        }
		        if (recvmsghdr[0] != sendmsghdrtemplate[0]) throwerror("Send/recv protocol error");
		        for (i1 = 16, len = 0; i1 < 24; i1++) {
			        c1 = (char) recvmsghdr[i1];
			        if (c1 >= '0' && c1 <= '9') len = len * 10 + c1 - '0';
		        }
		        if (len > 0) {
			        if (recvmsgdata == null || recvmsgdata.Length < len) recvmsgdata = new byte[len];
			
			        for (i1 = i2 = 0;;) {
				        i2 = netstream.Read(recvmsgdata, i1, len - i1);
				        if (i2 == -1) throwerror("EOF encountered in recv");
				        i1 += i2;
				        if (i1 == len) break;			
			        }		
			
		        }
		        recvlength = len;
	        }
	        catch (IOException) {
		        throwerror("IO exception encountered in recv");
	        }
            Decoder dcode1 = Encoding.UTF8.GetDecoder();
            int n1 = dcode1.GetCharCount(recvmsghdr, 8, 8);
            char[] chars = new char[n1];
            dcode1.GetChars(recvmsghdr, 8, 8, chars, 0);
            return (new String(chars)).Trim();
        }

		/// <summary>
		/// Called by the OS when the dbcfsrun process connects back to us
		/// </summary>
        public void DoAcceptFSServerCallback(IAsyncResult ar)
        {
            TcpListener listener = (TcpListener)ar.AsyncState;
            socket = listener.EndAcceptTcpClient(ar);
            s_sockConnected.Set();
        }

        private void throwerror(string message)
        {
	        try { netstream.Close(); }
	        catch (Exception) { }
	        try { socket.Close(); }
	        catch (Exception) { }
            netstream = null;
            socket = null;
	        throw new ConnectionException(message);
        }



        /// <summary>
        /// Get greeting message without port argument.
        /// </summary>
        /// <param name="server"></param>
        /// <returns>The greeting message from the FS Server</returns>
        public static string getgreeting(string server)
        {
	        return getgreeting(server, 0, false, null);
        }


        /// <summary>
        /// Get greeting message with port and authentication
        /// </summary>
        /// <param name="server">Server as URL or IP address</param>
        /// <param name="port">If zero or less, use the default</param>
        /// <param name="encrypt">always false for now</param>
        /// <param name="authfile">not used</param>
        /// <returns>The greeting message from the FS Server</returns>
        public static string getgreeting(string server, Int32 port, bool encrypt, string authfile /* not used */)
        {
            TcpClient socket;
            int n1;

            if (port <= 0) port = CONNECTPORT;
            socket = new TcpClient();
            socket.Connect(server, port);
            socket.NoDelay = true;
            Stream netstream = new BufferedStream(socket.GetStream());
            netstream.WriteByte(0x1);
            netstream.WriteByte(0x2);
            netstream.WriteByte(0x3);
            netstream.WriteByte(0x4);
            netstream.WriteByte(0x5);
            netstream.WriteByte(0x6);
            netstream.WriteByte(0x7);
            netstream.WriteByte(0x8);
            netstream.Write(Encoding.UTF8.GetBytes(HelloString), 0, Encoding.UTF8.GetByteCount(HelloString));
            netstream.Flush();
            byte[] msghdr = new byte[24];
            int numberOfBytesRead = 0;
            int numBytesToRead = msghdr.Length;
            while (numBytesToRead > 0)
            {
                n1 = netstream.Read(msghdr, numberOfBytesRead, numBytesToRead);
                if (n1 == 0) break;
                numberOfBytesRead += n1;
                numBytesToRead -= n1;
            }
            int len = 0;
            char c1;
            for (int i1 = 16; i1 < 24; i1++)
            {
                c1 = (char)msghdr[i1];
                if (c1 >= '0' && c1 <= '9') len = len * 10 + c1 - '0';
            }
            byte[] msgdata = new byte[len];
            numberOfBytesRead = 0;
            numBytesToRead = len;
            while (numBytesToRead > 0)
            {
                int n = netstream.Read(msgdata, numberOfBytesRead, numBytesToRead);
                if (n == 0) break;
                numberOfBytesRead += n;
                numBytesToRead -= n;
            }
            Decoder dcode1 = Encoding.UTF8.GetDecoder();
            n1 = dcode1.GetCharCount(msgdata, 0, numberOfBytesRead);
            char[] chars = new char[n1];
            dcode1.GetChars(msgdata, 0, numberOfBytesRead, chars, 0);
            netstream.Close();
            socket.Close();
            return new String(chars);
        }

        private static int parseMajorVersion(string v) 
        {
	        int i1 = v.LastIndexOf(' ');
	        int i2 = v.IndexOf('.');
	        String v1;
	        if (i2 < 0) v1 = v.Substring(i1 + 1);
	        else v1 = v.Substring(i1 + 1, i2);
	        try {
		        i1 = Int32.Parse(v1);
	        }
	        catch(Exception) { return -1; }
	        return i1;
        }

        /**
         * Return the server associated with this connection.
         */
        public string getserver()
        {
            return server;
        }

        /**
         * Return the database associated with this connection.
         */
        public string getdatabase()
        {
            return database;
        }

        /**
         * Return the user associated with this connection.
         */
        public string getuser()
        {
            return user;
        }

        ~Connection()
        {
            if (connected) disconnect();
        }

        internal object getLock() { return this; }

        /**
         * Return the last message data length.
         */
        internal int Recvlength()
        {
            return recvlength;
        }


    }
}
