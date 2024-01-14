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
 * Class file implements sequential, random, indexed and aim file io to an FS server.
 */
public final class File {

// constructor options
public static final int VARIABLE			= 0x00000001;
public static final int COMPRESSED			= 0x00000002;
public static final int TEXT				= 0x00000010;
public static final int DATA				= 0x00000020;
public static final int BINARY				= 0x00000040;
public static final int READONLY			= 0x00000100;
public static final int EXCLUSIVE			= 0x00000200;
public static final int DUP					= 0x00000400;
public static final int NODUP				= 0x00000800;
public static final int CASESENSITIVE		= 0x00001000;
public static final int LOCKAUTO			= 0x00002000;
public static final int LOCKSINGLE			= 0x00004000;
public static final int LOCKNOWAIT			= 0x00008000;
public static final int CREATEFILE			= 0x00010000;
public static final int CREATEONLY			= 0x00020000;
public static final int FILEOPEN			= 0x00100000;
public static final int IFILEOPEN			= 0x00200000;
public static final int AFILEOPEN			= 0x00400000;
public static final int CRLF				= 0x00800000;
public static final int EBCDIC				= 0x01000000;
public static final int COBOL				= 0x02000000;
public static final int NATIVE				= 0x04000000;

private Connection fs;					// connection instance
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

/**
 * Constructor.
 */
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
}

/**
 * Set index file name open parameter.
 */
public void setindexfile(String indexfilename)
{
	this.indexfilename = indexfilename;
}

/**
 * Get the file name open parameter.
 */
public String getfilename()
{
	if (indexfilename == null || indexfilename.length() == 0) return datafilename;
	return indexfilename;
}

/**
 * Set open options open parameter.
 */
public void setoptions(int options)
{
	this.options = options;
}

/**
 * Get open options open parameter.
 */
public int getoptions()
{
	return options;
}

/**
 * Set record size open parameter.
 */
public void setrecsize(int recsize)
{
	if (recsize > data.length - 200) data = new char[recsize + 200];
	this.recsize = recsize;
}

/**
 * Get record size open parameter.
 */
public int getrecsize()
{
	return recsize;
}

/**
 * Set key size open parameter.
 */
public void setkeysize(int keysize)
{
	this.keysize = keysize;
}

/**
 * Set key info open parameter.
 */
public void setkeyinfo(String keyinfo)
{
	this.keyinfo = keyinfo;
}

/**
 * Set aim match character open parameter.
 */
public void setmatchchar(char matchchar)
{
	this.matchchar = matchchar;
}

/**
 * Open or prepare a file or an indexed file.
 */
public void open() throws FileException
{
	if (fs == null || !fs.isConnected()) throwfileerror(653);
	synchronized(fs.getLock()) {
		if (datafilename == "" && indexfilename == "") throwfileerror(603);
		if ((options & (FILEOPEN | IFILEOPEN | AFILEOPEN)) == 0) throwfileerror(604);
		if (recsize < 1) throwfileerror(610);
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
		addtodata(Integer.toString(recsize));
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
			if (!result.equals("OK")) {
				throwfileerror(result);
			}
			int len = fs.recvlength();
			if (len != 8) throwfileerror(699);
			fsopenid = new String(data, 0, 8);
		}
		catch (ConnectionException e) {
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
public void close() throws FileException
{
	synchronized(fs.getLock()) {
		if (fsopenid == null) throwfileerror(701);
		try {
			fs.send(fsopenid, "CLOSE");
			fsopenid = null;
			String result = fs.recv();
			if (!result.equals("OK")) throwfileerror(result);
		}
		catch (ConnectionException e) {
			throwfileerror(699);
		}
	}
}

/**
 * Close a file and delete it.
 */
public void closedelete() throws FileException
{
	synchronized(fs.getLock()) {
		if (fsopenid == null) throwfileerror(701);
		try {
			fs.send(fsopenid, "CLOSEDEL");
			fsopenid = null;
			String result = fs.recv();
			if (!result.equals("OK")) throwfileerror(result);
		}
		catch (ConnectionException e) {
			throwfileerror(699);
		}
	}
}

/**
 * Is file open?
 */
public boolean isopen()
{
	if (fsopenid != null) return true;
	return false;
}

/**
 * Return the current file position.
 */
public long fposit() throws FileException
{
	synchronized(fs.getLock()) {
		String result = null;
		if (fsopenid == null) throwfileerror(701);
		try {
			fs.send(fsopenid, "FPOSIT");
			result = fs.recv(data);
		}
		catch (ConnectionException e) {
			throwfileerror(699);
		}
		if (!result.equals("OK")) throwfileerror(result);
		int len = fs.recvlength();
		return Long.parseLong(String.valueOf(data, 0, len).trim());
	}
}

/**
 * Set the current file position.
 * Return value 0 means file position set before end of file
 * Return value 1 means file position set at end of file
 * Return value 2 means file position not set because pos is past end of file
 */
public int reposit(long pos) throws FileException
{
	synchronized(fs.getLock()) {
		String result = null;
		if (fsopenid == null) throwfileerror(701);
		datasize = 0;
		addtodata(Long.toString(pos));
		try {
			fs.send(fsopenid, "REPOSIT", data, datasize);
			result = fs.recv();
		}
		catch (ConnectionException e) {
			throwfileerror(699);
		}
		if (result.equals("OK")) return 0;
		if (result.equals("ATEOF")) return 1;
		if (result.equals("PASTEOF")) return 2;
		throwfileerror(result);
	}
	return -1;  // to shut up compiler
}

/**
 * Set the current file position to the end of file.
 */
public int setposateof() throws FileException
{
	reposit(-1L);
	return 1;
}

/**
 * Read a random record.
 * Return the number of characters read, -1 if eof, -2 if record locked.
 */
public int readrandom(int recnum) throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		addtodata(Integer.toString(recnum));
		return read("READR");
	}
}

/**
 * Read a random record and lock.
 * Return the number of characters read, -1 if eof, -2 if record locked.
 */
public int readrandomlock(int recnum) throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		addtodata(Integer.toString(recnum));
		return read("READRL");
	}
}

/**
Read the next sequential record.
Return the number of characters read or throw FileException
*/
public int readnext() throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		return read("READS");
	}
}

/**
Read the next sequential record and lock.
Return the number of characters read or throw FileException
*/
public int readnextlock() throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		return read("READSL");
	}
}

/**
Read the prior sequential record.
Return the number of characters read or throw FileException
*/
public int readprev() throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		return read("READB");
	}
}

/**
Read the prior sequential record and lock.
Return the number of characters read or throw FileException
*/
public int readprevlock() throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		return read("READBL");
	}
}

/**
Read the same record that was last read.
Return the number of characters read or throw FileException
*/
public int readsame() throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		return read("READZ");
	}
}

/**
Read a record with a key.
Return the number of characters read or throw FileException
*/
public int readkey(String key) throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		addtodata(key);
		return read("READK");
	}
}

/**
Read a record with a key and lock.
Return the number of characters read or throw FileException
*/
public int readkeylock(String key) throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		addtodata(key);
		return read("READKL");
	}
}

/**
Read a record key sequentially.
Return the number of characters read or throw FileException
*/
public int readkeynext() throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		if ((options & AFILEOPEN) != 0) return read("READO");
		return read("READN");
	}
}

/**
Read a record key sequentially and lock.
Return the number of characters read or throw FileException
*/
public int readkeynextlock() throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		if ((options & AFILEOPEN) != 0) return read("READOL");
		return read("READNL");
	}
}

/**
Read a record key backward.
Return the number of characters read or throw FileException
*/
public int readkeyprev() throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		if ((options & AFILEOPEN) != 0) return read("READQ");
		return read("READP");
	}
}

/**
Read a record key backward and lock.
Return the number of characters read or throw FileException
*/
public int readkeyprevlock() throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		if ((options & AFILEOPEN) != 0) return read("READQL");
		return read("READPL");
	}
}

/**
Read a record set and the first record.
Return the number of characters read or throw FileException
*/
public int readaim(String s1) throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		addtodata("1 ");
		addtodata(Integer.toString(s1.length()));
		addtodata(" ");
		addtodata(s1);
		return read("READA");
	}
}

/**
Read a record set and the first record and lock
Return the number of characters read or throw FileException
*/
public int readaimlock(String s1) throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		addtodata("1 ");
		addtodata(Integer.toString(s1.length()));
		addtodata(" ");
		addtodata(s1);
		return read("READAL");
	}
}

/**
Read a record set and the first record using multiple keys.
Return the number of characters read or throw FileException
*/
public int readaim(String[] sa1) throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		int count = sa1.length;
		addtodata(Integer.toString(count));
		addtodata(" ");
		for (int i1 = 0; i1 < count; i1++) {
			if (i1 > 0) addtodata(" ");
			addtodata(Integer.toString(sa1[i1].length()));
			addtodata(" ");
			addtodata(sa1[i1]);
		}
		return read("READA");
	}
}

/**
Read a record set and the first record using multiple keys and lock.
Return the number of characters read or throw FileException
*/
public int readaimlock(String[] sa1) throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		int count = sa1.length;
		addtodata(Integer.toString(count));
		addtodata(" ");
		for (int i1 = 0; i1 < count; i1++) {
			if (i1 > 0) addtodata(" ");
			addtodata(Integer.toString(sa1[i1].length()));
			addtodata(" ");
			addtodata(sa1[i1]);
		}
		return read("READAL");
	}
}

/**
Write a random record.
*/
public void writerandom(int reclen, int recnum) throws FileException
{
	synchronized(fs.getLock()) {
		String result = null;
		if (fsopenid == null) throwfileerror(701);
		datasize = 0;
		addtodata(Long.toString(recnum));
		addtodata(" ");
		try {
			fs.send(fsopenid, "WRITER", data, datasize, recordbuffer, reclen);
			result = fs.recv();
		}
		catch (ConnectionException e) {
			throwfileerror(699);
		}
		if (!result.equals("OK")) throwfileerror(result);
	}
}

/**
Write the next sequential record.
*/
public void writenext(int reclen) throws FileException
{
	synchronized(fs.getLock()) {
		String result = null;
		if (fsopenid == null) throwfileerror(701);
		try {
			fs.send(fsopenid, "WRITES", recordbuffer, reclen);
			result = fs.recv();
		}
		catch (ConnectionException e) {
			throwfileerror(699);
		}
		if (!result.equals("OK")) throwfileerror(result);
	}
}

/**
 * Write the record at end of file.
 */
public void writeateof(int reclen) throws FileException
{
	synchronized(fs.getLock()) {
		String result = null;
		if (fsopenid == null) throwfileerror(701);
		try {
			fs.send(fsopenid, "WRITEE", recordbuffer, reclen);
			result = fs.recv();
		}
		catch (ConnectionException e) {
			throwfileerror(699);
		}
		if (!result.equals("OK")) throwfileerror(result);
	}
}

/**
 * Write the record and key.
 */
public void writekey(int reclen, String key) throws FileException
{
	synchronized(fs.getLock()) {
		String result = null;
		if (fsopenid == null) throwfileerror(701);
		datasize = 0;
		addtodata(Integer.toString(key.length()));
		addtodata(" ");
		addtodata(key);
		addtodata(" ");
		try {
			fs.send(fsopenid, "WRITEK", data, datasize, recordbuffer, reclen);
			result = fs.recv();
		}
		catch (ConnectionException e) {
			throwfileerror(699);
		}
		if (!result.equals("OK")) throwfileerror(result);
	}
}

/**
Write the record and associative index info.
*/
public void writeaim(int reclen) throws FileException
{
	synchronized(fs.getLock()) {
		String result = null;
		if (fsopenid == null) throwfileerror(701);
		try {
			fs.send(fsopenid, "WRITEA", recordbuffer, reclen);
			result = fs.recv();
		}
		catch (ConnectionException e) {
			throwfileerror(699);
		}
		if (!result.equals("OK")) throwfileerror(result);
	}
}

/**
Insert a key.
*/
public void insertkey(String key) throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		addtodata(key);
		change("INSERT");
	}
}

/**
Insert the associative key information.
*/
public void insertkeys(int reclen) throws FileException
{
	synchronized(fs.getLock()) {
		String result = null;
		if (fsopenid == null) throwfileerror(701);
		try {
			fs.send(fsopenid, "INSERT", recordbuffer, reclen);
			result = fs.recv();
		}
		catch (ConnectionException e) {
			throwfileerror(699);
		}
		if (!result.equals("OK")) throwfileerror(result);
	}
}

/**
Update the current record.
*/
public void update(int reclen) throws FileException
{
	synchronized(fs.getLock()) {
		String result = null;
		if (fsopenid == null) throwfileerror(701);
		try {
			fs.send(fsopenid, "UPDATE", recordbuffer, reclen);
			result = fs.recv();
		}
		catch (ConnectionException e) {
			throwfileerror(699);
		}
		if (!result.equals("OK")) throwfileerror(result);
	}
}

/**
Delete the current record and its index key.
*/
public void delete() throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		change("DELETEC");
	}
}

/**
Delete a record and its key by the key.
*/
public void delete(String key) throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		addtodata(key);
		change("DELETE");
	}
}

/**
Delete a key.
*/
public void deletekey(String key) throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		addtodata(key);
		change("DELETEK");
	}
}

/**
Unlock one record.
*/
public void unlock(long pos) throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		addtodata(Long.toString(pos));
		change("UNLOCK");
	}
}

/**
Unlock all records.
*/
public void unlock() throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		change("UNLOCK");
	}
}

/**
Write an end of file.
*/
public void weof(long pos) throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		addtodata(Long.toString(pos));
		change("WEOF");
	}
}

/**
Compare this File with File parameter
*/
public int compare(File file) throws FileException
{
	String result = null;
	synchronized(fs.getLock()) {
		datasize = 0;
		addtodata(file.getfsopenid());
		if (fsopenid == null || file.getfsopenid() == null) {
			throwfileerror(701);
		}
		try {
			fs.send(fsopenid, "COMPARE", data, datasize);
			result = fs.recv();
		}
		catch (ConnectionException e) {
			throwfileerror(699);
		}
		if (result.equals("LESS")) return -1;
		else if (result.equals("GREATER")) return 1;
	}
	return 0;
}

/**
Lock the file.
*/
public void filelock() throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		change("FLOCK");
	}
}

/**
Unlock the file.
*/
public void fileunlock() throws FileException
{
	synchronized(fs.getLock()) {
		datasize = 0;
		change("FUNLOCK");
	}
}

/**
 * Execute a utility
 */
public int command(String cmd) throws FileException
{
	String result = null;
	if (fsopenid == null) throwfileerror(701);
	synchronized(fs.getLock()) {
		datasize = 0;
		addtodata(cmd);
		try {
			fs.send(fsopenid, "COMMAND", data, datasize);
			result = fs.recv();
		}
		catch (ConnectionException e) {
			throwfileerror(699);
		}
		if (!result.equals("OK")) return -1;
	}
	return 0;
}

/**
Generic read routine.
*/
private int read(String func) throws FileException
{
	String result = null;
	if (fsopenid == null) throwfileerror(701);
	try {
		if (datasize > 0) fs.send(fsopenid, func, data, datasize);
		else fs.send(fsopenid, func);
		result = fs.recv(recordbuffer);
	}
	catch (ConnectionException e) {
		throwfileerror(699);
	}
	if (result.equals("NOREC")) return -1;
	if (result.equals("LOCKED")) return -2;
	if (!result.equals("OK")) throwfileerror(result);
	return fs.recvlength();
}

/**
Generic change routine.
*/
private void change(String func) throws FileException
{
	String result = null;
	if (fsopenid == null) throwfileerror(701);
	try {
		if (datasize > 0) fs.send(fsopenid, func, data, datasize);
		else fs.send(fsopenid, func);
		result = fs.recv();
	}
	catch (ConnectionException e) {
		throwfileerror(699);
	}
	if (!result.equals("OK")) throwfileerror(result);
}

/**
 * Add to the data to send.
 */
private void addtodata(String s1)
{
	int len = s1.length();
	s1.getChars(0, len, data, datasize);
	datasize += len;
}

/**
 * Throw a file io error.
 */
private static void throwfileerror(String s1) throws FileException
{
	int n1 = 0;
	int len = s1.length();
	char c1;
	for (int i1 = 0; i1 < len; i1++) {
		c1 = s1.charAt(i1);
		if (c1 >= '0' && c1 <= '9') n1 = n1 * 10 + c1 - '0';
	}
	throw new FileException(n1);
}

/**
 * Throw a file io error.
 */
private static void throwfileerror(int n1) throws FileException
{
	throw new FileException(n1);
}

/**
 * Get fsopenid
 */
private String getfsopenid() {
	return fsopenid;
}

}
