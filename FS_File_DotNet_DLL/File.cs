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
    public class File
    {
        // constructor options
        public static readonly int VARIABLE;
        public static readonly int COMPRESSED;
        public static readonly int TEXT;
        public static readonly int DATA;
        public static readonly int CRLF;
        public static readonly int EBCDIC;
        public static readonly int BINARY;
        public static readonly int READONLY;
        public static readonly int EXCLUSIVE;
        public static readonly int DUP;
        public static readonly int NODUP;
        public static readonly int CASESENSITIVE;
        public static readonly int LOCKAUTO;
        public static readonly int LOCKSINGLE;
        public static readonly int LOCKNOWAIT;
        public static readonly int CREATEFILE;
        public static readonly int CREATEONLY;
        public static readonly int FILEOPEN;
        public static readonly int IFILEOPEN;
        public static readonly int AFILEOPEN;
        public static readonly int COBOL;
        public static readonly int NATIVE;

        static File()
        {
            VARIABLE        = 0x00000001;
            COMPRESSED		= 0x00000002;
            TEXT			= 0x00000010;
            DATA			= 0x00000020;
            BINARY			= 0x00000040;
            READONLY		= 0x00000100;
            EXCLUSIVE		= 0x00000200;
            DUP				= 0x00000400;
            NODUP			= 0x00000800;
            CASESENSITIVE	= 0x00001000;
            LOCKAUTO		= 0x00002000;
            LOCKSINGLE		= 0x00004000;
            LOCKNOWAIT		= 0x00008000;
            CREATEFILE		= 0x00010000;
            CREATEONLY		= 0x00020000;
            FILEOPEN		= 0x00100000;
            IFILEOPEN		= 0x00200000;
            AFILEOPEN		= 0x00400000;
            CRLF			= 0x00800000;
            EBCDIC			= 0x01000000;
            COBOL			= 0x02000000;
            NATIVE			= 0x04000000;
        }

        private Connection fs;
        private String fsopenid;				// fsid returned by open call to fs
        private String datafilename;			// open parameter: data file name (.TXT)
        private String indexfilename;			// open parameter: data file name (.ISI or .AIM)
        private int options;					// open parameter: options
        private int recsize;					// open parameter: record size
        private int keysize;					// open parameter: key size
        private String keyinfo;					// open parameter: key spec
        private char matchchar;					// open parameter: aim match character
        private char[] recordbuffer;			// user supplied read/write buffer
        private int datasize;					// data size also used as offset by addtodata
        private char[] data = new char[1000];	// send/recv work buffer

        public File(Connection fs)
        {
            this.fs = fs;
        }

        /**
         * Set data file name open parameter.
         */
        public void setdatafile(String datafilename)
        {
            this.datafilename = datafilename;
            //System.Console.WriteLine("Data file name set to " + this.datafilename);
        }

        /**
        Set index file name open parameter.
        */
        public void setindexfile(String indexfilename)
        {
            this.indexfilename = indexfilename;
        }

        /**
        Get the file name open parameter.
        */
        public String getfilename()
        {
            if (indexfilename == null || indexfilename.Length == 0) return datafilename;
            return indexfilename;
        }

        /**
        Set open options open parameter.
        */
        public void setoptions(int options)
        {
            this.options = options;
        }

        /**
        Get open options open parameter.
        */
        public int getoptions()
        {
            return options;
        }

        /**
        Set record size open parameter.
        */
        public void setrecsize(int recsize)
        {
            if (recsize > data.Length - 200) data = new char[recsize + 200];
            this.recsize = recsize;
            //System.Console.WriteLine("Record size set to " + this.recsize);
        }

        /**
        Get record size open parameter.
        */
        public int getrecsize()
        {
            return recsize;
        }

        /**
        Set key size open parameter.
        */
        public void setkeysize(int keysize)
        {
            this.keysize = keysize;
        }

        /**
        Set key info open parameter.
        */
        public void setkeyinfo(String keyinfo)
        {
            this.keyinfo = keyinfo;
        }

        /**
        Set aim match character open parameter.
        */
        public void setmatchchar(char matchchar)
        {
            this.matchchar = matchchar;
        }

        /**
         * Open or prepare a file or an indexed file.
         */
        public void open() 
        {
	        lock(fs.getLock()) {
		        if (fs == null) throwfileerror(653);
		        if (datafilename == "" && indexfilename == "") throwfileerror(603);
		        if ((options & (FILEOPEN | IFILEOPEN | AFILEOPEN)) == 0) throwfileerror(604);
		        if (recsize < 1) throwfileerror(610);
                //System.Console.WriteLine("In FIle.open()");
		        String s1 = null;
		        if ((options & FILEOPEN) != 0) s1 = datafilename;
		        else s1 = indexfilename;
		        datasize = 0;
		        if (datafilename != null) addtodata(" TEXTFILENAME=\"" + datafilename + "\"");
		        if (indexfilename != null) addtodata(" INDEXFILENAME=\"" + indexfilename + "\"");
		        if ((options & COMPRESSED) != 0) {
			        addtodata(" COMPRESSED");
			        options |= VARIABLE;
		        }
		        if ((options & VARIABLE) == 0) addtodata(" FIXLEN=");
		        else addtodata(" VARLEN=");
		        addtodata(recsize.ToString());
		        if ((options & TEXT) != 0) addtodata(" TEXT");
		        if ((options & DATA) != 0) addtodata(" DATA");
		        if ((options & CRLF) != 0) addtodata(" CRLF");
		        if ((options & EBCDIC) != 0) addtodata(" EBCDIC");
		        if ((options & BINARY) != 0) addtodata(" BINARY");
		        if ((options & IFILEOPEN) != 0) {
			        if (keysize != 0) addtodata(" KEYLEN=" + keysize);
		        }
		        if ((options & AFILEOPEN) != 0) {
			        if (keyinfo != null) addtodata(" AIMKEY=\"" + keyinfo + "\"");
			        if (matchchar != 0) addtodata(" MATCH=" + matchchar);
			        if ((options & CASESENSITIVE) != 0) addtodata(" CASESENSITIVE");
		        }
		        if ((options & LOCKAUTO) != 0) addtodata(" LOCKAUTO");
		        if ((options & LOCKSINGLE) != 0) addtodata(" LOCKSINGLE");
		        if ((options & LOCKNOWAIT) != 0) addtodata(" LOCKNOWAIT");
		        if ((options & FILEOPEN) != 0) s1 = "OPENF";
		        else if ((options & IFILEOPEN) != 0) s1 = "OPENI";
		        else if ((options & AFILEOPEN) != 0) s1 = "OPENA";
		        if ((options & CREATEFILE) != 0) s1 += "P";
		        else if ((options & CREATEONLY) != 0) s1 += "Q";
		        else if ((options & EXCLUSIVE) != 0) s1 += "E";
		        else s1 += "S";
		        try {
			        fs.send(null, s1, data, datasize);
			        String result = fs.recv(data);
			        if (!result.Equals("OK")) {
				        throwfileerror(result);
			        }
			        int len = fs.Recvlength();
			        if (len != 8) throwfileerror(699);
			        fsopenid = new String(data, 0, 8);
		        }
		        catch (ConnectionException) {
			        throwfileerror(699);
		        }
	        }
        }

        /**
         * Set the current record buffer.
         */
        public void setrecordbuffer(char[] newrecbuf)
        {
            recordbuffer = newrecbuf;
        }


        /**
         * Close a file.
         */
        public void close()
        {
	        lock(fs.getLock()) {
		        if (fsopenid == null) throwfileerror(701);
		        try {
                    //System.Console.WriteLine("In File.close, fsopenid = " + fsopenid);
			        fs.send(fsopenid, "CLOSE");
			        fsopenid = null;
			        String result = fs.recv();
			        if (!result.Equals("OK")) throwfileerror(result);
		        }
		        catch (ConnectionException) {
			        throwfileerror(699);
		        }
	        }
        }

        /**
         * Close a file and delete it.
         */
        public void closedelete()
        {
	        lock(fs.getLock()) {
		        if (fsopenid == null) throwfileerror(701);
		        try {
			        fs.send(fsopenid, "CLOSEDEL");
			        fsopenid = null;
			        String result = fs.recv();
			        if (!result.Equals("OK")) throwfileerror(result);
		        }
		        catch (ConnectionException) {
			        throwfileerror(699);
		        }
	        }
        }

        /**
         * Is file open?
         */
        public bool isopen()
        {
	        if (fsopenid != null) return true;
	        return false;
        }

        /**
         * Return the current file position.
         */
        public long fposit()
        {
	        lock(fs.getLock()) {
		        String result = null;
		        if (fsopenid == null) throwfileerror(701);
		        try {
			        fs.send(fsopenid, "FPOSIT");
			        result = fs.recv(data);
		        }
		        catch (ConnectionException) {
			        throwfileerror(699);
		        }
		        if (!result.Equals("OK")) throwfileerror(result);
		        int len = fs.Recvlength();
                string s2 = new String(data, 0, len).Trim();
                return Int64.Parse(s2);
		        //return Long.parseLong(String.valueOf(data, 0, len).trim());
	        }
        }

        /**
         * Set the current file position.
         * Return value 0 means file position set before end of file
         * Return value 1 means file position set at end of file
         * Return value 2 means file position not set because pos is past end of file
         */
        public int reposit(long pos)
        {
	        lock(fs.getLock()) {
		        String result = null;
		        if (fsopenid == null) throwfileerror(701);
		        datasize = 0;
		        addtodata(pos.ToString());
		        try {
			        fs.send(fsopenid, "REPOSIT", data, datasize);
			        result = fs.recv();
		        }
		        catch (ConnectionException) {
			        throwfileerror(699);
		        }
		        if (result.Equals("OK")) return 0;
		        if (result.Equals("ATEOF")) return 1;
		        if (result.Equals("PASTEOF")) return 2;
		        throwfileerror(result);
	        }
	        return -1;  // to shut up compiler
        }

        /**
         * Set the current file position to the end of file.
         */
        public int setposateof()
        {
	        reposit(-1L);
	        return 1;
        }

        /**
         * Read a random record.
         * Return the number of characters read, -1 if eof, -2 if record locked.
         */
        public int readrandom(int recnum)
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        addtodata(recnum.ToString());
		        return read("READR");
	        }
        }

        /**
         * Read a random record and lock.
         * Return the number of characters read, -1 if eof, -2 if record locked.
         */
        public int readrandomlock(int recnum)
        {
	        lock(fs.getLock()) {
		        datasize = 0;
                addtodata(recnum.ToString());
		        return read("READRL");
	        }
        }

        /**
         * Read the next sequential record.
         * Return the number of characters read or throw FileException
         */
        public int readnext()
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        return read("READS");
	        }
        }

        /**
         * Read the next sequential record and lock.
         * Return the number of characters read or throw FileException
         */
        public int readnextlock()
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        return read("READSL");
	        }
        }

        /**
         * Read the prior sequential record.
         * Return the number of characters read or throw FileException
         */
        public int readprev()
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        return read("READB");
	        }
        }

        /**
         * Read the prior sequential record and lock.
         * Return the number of characters read or throw FileException
         */
        public int readprevlock()
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        return read("READBL");
	        }
        }

        /**
         * Read the same record that was last read.
         * Return the number of characters read or throw FileException
         */
        public int readsame()
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        return read("READZ");
	        }
        }

        /**
         * Read a record with a key.
         * Return the number of characters read or throw FileException
         */
        public int readkey(String key)
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        addtodata(key);
		        return read("READK");
	        }
        }

        /**
         * Read a record with a key and lock.
         * Return the number of characters read or throw FileException
         */
        public int readkeylock(String key)
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        addtodata(key);
		        return read("READKL");
	        }
        }

        /**
         * Read a record key sequentially.
         * Return the number of characters read or throw FileException
         */
        public int readkeynext()
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        if ((options & AFILEOPEN) != 0) return read("READO");
		        return read("READN");
	        }
        }

        /**
         * Read a record key sequentially and lock.
         * Return the number of characters read or throw FileException
         */
        public int readkeynextlock()
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        if ((options & AFILEOPEN) != 0) return read("READOL");
		        return read("READNL");
	        }
        }

        /**
         * Read a record key backward.
         * Return the number of characters read or throw FileException
         */
        public int readkeyprev()
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        if ((options & AFILEOPEN) != 0) return read("READQ");
		        return read("READP");
	        }
        }

        /**
         * Read a record key backward and lock.
         * Return the number of characters read or throw FileException
         */
        public int readkeyprevlock()
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        if ((options & AFILEOPEN) != 0) return read("READQL");
		        return read("READPL");
	        }
        }

        /**
         * Read a record set and the first record.
         * Return the number of characters read or throw FileException
         */
        public int readaim(String s1)
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        addtodata("1 ");
		        addtodata(s1.Length.ToString());
		        addtodata(" ");
		        addtodata(s1);
		        return read("READA");
	        }
        }

        /**
         * Read a record set and the first record and lock
         * Return the number of characters read or throw FileException
         */
        public int readaimlock(String s1)
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        addtodata("1 ");
                addtodata(s1.Length.ToString());
		        addtodata(" ");
		        addtodata(s1);
		        return read("READAL");
	        }
        }

        /**
         * Read a record set and the first record using multiple keys.
         * Return the number of characters read or throw FileException
         */
        public int readaim(String[] sa1)
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        int count = sa1.Length;
                addtodata(count.ToString());
		        addtodata(" ");
		        for (int i1 = 0; i1 < count; i1++) {
			        if (i1 > 0) addtodata(" ");
                    addtodata(sa1[i1].Length.ToString());
			        addtodata(" ");
			        addtodata(sa1[i1]);
		        }
		        return read("READA");
	        }
        }

        /**
         * Read a record set and the first record using multiple keys and lock.
         * Return the number of characters read or throw FileException
         */
        public int readaimlock(String[] sa1)
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        int count = sa1.Length;
		        addtodata(count.ToString());
		        addtodata(" ");
		        for (int i1 = 0; i1 < count; i1++) {
			        if (i1 > 0) addtodata(" ");
                    addtodata(sa1[i1].Length.ToString());
			        addtodata(" ");
			        addtodata(sa1[i1]);
		        }
		        return read("READAL");
	        }
        }

        /**
         * Write a random record.
         */
        public void writerandom(int reclen, int recnum)
        {
	        lock(fs.getLock()) {
		        String result = null;
		        if (fsopenid == null) throwfileerror(701);
		        datasize = 0;
		        addtodata(recnum.ToString());
		        addtodata(" ");
		        try {
			        fs.send(fsopenid, "WRITER", data, datasize, recordbuffer, reclen);
			        result = fs.recv();
		        }
		        catch (ConnectionException) {
			        throwfileerror(699);
		        }
		        if (!result.Equals("OK")) throwfileerror(result);
	        }
        }

        /**
         * Write the next sequential record.
         */
        public void writenext(int reclen)
        {
	        lock(fs.getLock()) {
		        String result = null;
		        if (fsopenid == null) throwfileerror(701);
		        try {
			        fs.send(fsopenid, "WRITES", recordbuffer, reclen);
			        result = fs.recv();
		        }
		        catch (ConnectionException) {
			        throwfileerror(699);
		        }
		        if (!result.Equals("OK")) throwfileerror(result);
	        }
        }

        /**
         * Write the record at end of file.
         */
        public void writeateof(int reclen)
        {
	        lock(fs.getLock()) {
		        String result = null;
		        if (fsopenid == null) throwfileerror(701);
		        try {
			        fs.send(fsopenid, "WRITEE", recordbuffer, reclen);
			        result = fs.recv();
		        }
		        catch (ConnectionException) {
			        throwfileerror(699);
		        }
		        if (!result.Equals("OK")) throwfileerror(result);
	        }
        }

        /**
         * Write the record and key.
         */
        public void writekey(int reclen, String key)
        {
	        lock(fs.getLock()) {
		        String result = null;
		        if (fsopenid == null) throwfileerror(701);
		        datasize = 0;
                addtodata(key.Length.ToString());
		        addtodata(" ");
		        addtodata(key);
		        addtodata(" ");
		        try {
			        fs.send(fsopenid, "WRITEK", data, datasize, recordbuffer, reclen);
			        result = fs.recv();
		        }
		        catch (ConnectionException) {
			        throwfileerror(699);
		        }
		        if (!result.Equals("OK")) throwfileerror(result);
	        }
        }

        /**
         * Write the record and associative index info.
         */
        public void writeaim(int reclen)
        {
	        lock(fs.getLock()) {
		        String result = null;
		        if (fsopenid == null) throwfileerror(701);
		        try {
			        fs.send(fsopenid, "WRITEA", recordbuffer, reclen);
			        result = fs.recv();
		        }
		        catch (ConnectionException) {
			        throwfileerror(699);
		        }
		        if (!result.Equals("OK")) throwfileerror(result);
	        }
        }

        /**
         * Insert a key.
         */
        public void insertkey(String key)
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        addtodata(key);
		        change("INSERT");
	        }
        }

        /**
         * Insert the associative key information.
         */
        public void insertkeys(int reclen)
        {
	        lock(fs.getLock()) {
		        String result = null;
		        if (fsopenid == null) throwfileerror(701);
		        try {
			        fs.send(fsopenid, "INSERT", recordbuffer, reclen);
			        result = fs.recv();
		        }
		        catch (ConnectionException) {
			        throwfileerror(699);
		        }
		        if (!result.Equals("OK")) throwfileerror(result);
	        }
        }

        /**
         * Update the current record.
         */
        public void update(int reclen)
        {
	        lock(fs.getLock()) {
		        String result = null;
		        if (fsopenid == null) throwfileerror(701);
		        try {
			        fs.send(fsopenid, "UPDATE", recordbuffer, reclen);
			        result = fs.recv();
		        }
		        catch (ConnectionException) {
			        throwfileerror(699);
		        }
		        if (!result.Equals("OK")) throwfileerror(result);
	        }
        }

        /**
         * Delete the current record and its index key.
         */
        public void delete()
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        change("DELETEC");
	        }
        }

        /**
         * Delete a record and its key by the key.
         */
        public void delete(String key)
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        addtodata(key);
		        change("DELETE");
	        }
        }

        /**
         * Delete a key.
         */
        public void deletekey(String key)
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        addtodata(key);
		        change("DELETEK");
	        }
        }

        /**
         * Unlock one record.
         */
        public void unlock(long pos)
        {
            lock (fs.getLock())
            {
		        datasize = 0;
		        addtodata(pos.ToString());
		        change("UNLOCK");
	        }
        }

        /**
         * Unlock all records.
         */
        public void unlock()
        {
            lock (fs.getLock())
            {
		        datasize = 0;
		        change("UNLOCK");
	        }
        }

        /**
         * Write an end of file.
         */
        public void weof(long pos)
        {
            lock (fs.getLock())
            {
		        datasize = 0;
                addtodata(pos.ToString());
		        change("WEOF");
	        }
        }

        /**
         * Compare this File with File parameter
         */
        public int compare(File file)
        {
	        String result = null;
            lock (fs.getLock())
            {
		        datasize = 0;
		        addtodata(file.getfsopenid());
		        if (fsopenid == null || file.getfsopenid() == null) {
			        throwfileerror(701);
		        }
		        try {
			        fs.send(fsopenid, "COMPARE", data, datasize);
			        result = fs.recv();
		        }
		        catch (ConnectionException) {
			        throwfileerror(699);
		        }
		        if (result.Equals("LESS")) return -1;
		        else if (result.Equals("GREATER")) return 1;
	        }
	        return 0;
        }

        /**
         * Lock the file.
         */
        public void filelock()
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        change("FLOCK");
	        }
        }

        /**
         * Unlock the file.
         */
        public void fileunlock()
        {
	        lock(fs.getLock()) {
		        datasize = 0;
		        change("FUNLOCK");
	        }
        }

        /**
         * Execute a utility
         */
        public int command(String cmd)
        {
	        String result = null;
	        if (fsopenid == null) throwfileerror(701);
	        lock(fs.getLock()) {
		        datasize = 0;
		        addtodata(cmd);
		        try {
			        fs.send(fsopenid, "COMMAND", data, datasize);
			        result = fs.recv();
		        }
		        catch (ConnectionException) {
			        throwfileerror(699);
		        }
		        if (!result.Equals("OK")) return -1;
	        }
	        return 0;
        }

        /**
         * Generic read routine.
         */
        private int read(String func)
        {
	        String result = null;
	        if (fsopenid == null) throwfileerror(701);
	        try {
		        if (datasize > 0) fs.send(fsopenid, func, data, datasize);
		        else fs.send(fsopenid, func);
		        result = fs.recv(recordbuffer);
	        }
	        catch (ConnectionException) {
		        throwfileerror(699);
	        }
	        if (result.Equals("NOREC")) return -1;
	        if (result.Equals("LOCKED")) return -2;
	        if (!result.Equals("OK")) throwfileerror(result);
	        return fs.Recvlength();
        }

        /**
         * Generic change routine.
         */
        private void change(String func)
        {
	        String result = null;
	        if (fsopenid == null) throwfileerror(701);
	        try {
		        if (datasize > 0) fs.send(fsopenid, func, data, datasize);
		        else fs.send(fsopenid, func);
		        result = fs.recv();
	        }
	        catch (ConnectionException) {
		        throwfileerror(699);
	        }
	        if (!result.Equals("OK")) throwfileerror(result);
        }

        /**
         * Add to the data to send.
         */
        private void addtodata(String s1)
        {
            int len = s1.Length;
            char[] car = s1.ToCharArray();
            Array.Copy(car, 0, data, datasize, len);
            //Buffer.BlockCopy(car, 0, data, datasize, len);
            //s1.getChars(0, len, data, datasize);
            datasize += len;
        }

        /**
         * Throw a file io error.
         */
        private static void throwfileerror(String s1)
        {
	        int n1 = 0;
	        int len = s1.Length;
	        char c1;
            char[] s1a = s1.ToCharArray();
	        for (int i1 = 0; i1 < len; i1++) {
		        c1 = s1a[i1]; //s1.CharAt(i1);
		        if (c1 >= '0' && c1 <= '9') n1 = n1 * 10 + c1 - '0';
	        }
	        throw new FileException(n1);
        }

        /**
         * Throw a file io error.
         */
        private static void throwfileerror(int n1)
        {
	        throw new FileException(n1);
        }

        /**
         * Get fsopenid
         */
        private String getfsopenid()
        {
            return fsopenid;
        }
    }
}