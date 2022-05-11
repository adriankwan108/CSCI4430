**Environment**
<pre>
C
Linux
Unix
</pre>

**Background**
<pre>
File Transfer Protocol (FTP) is a standard communication protocol 
used for transfering computer files from server to client.
FTP is built on a client-server model architecture 
using separate control and data connections between server and client.
TCP is transport protocol.

This programme will implement a multi-client file transfer application called MYFTP,
which is a simplified version of FTP.
</pre>

**Usage**
<pre>
MYFTP should includes basic functions of FTP, i.e. file upload/download and file listing.
It enables multiple clients to connect to the server simutaneously.
The client and server can reside in different machines with different operating system (Linux/Unix).

*But assume no two clients will upload or download a file with same name simultaneously.
</pre>

**Implementation Details**
<pre>
Please view the pdf.
</pre>

**Command-line interface**
Server:
```
sparc1> ./myftpserver PORT_NUMBER
```

Client:
```
client> ./myftpclient <server ip addr> <server port> <list|get|put> <file>
```