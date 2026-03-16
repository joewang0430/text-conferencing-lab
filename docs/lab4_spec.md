ECE361 COMPUTER NETWORKS 

Text Conferencing 

H. TIMORABADI 

 
 
 
 
Objective 

In  this  lab,  you  will  use  UNIX  TCP  sockets  to  implement  a  simple  conferencing 
application. 

Reference 

•  Section  2.4  and  related  sections  on  Berkeley  API  from  Chapter  2  of  the 
Communication Networks by Alberto Leon-Garcia and Indra Widjaja, McGraw Hill, 
2nd Edition, 2004. 

•  The network socket programming “Beej’s Guide to Network Programming” available 

on the University Portal under the course section. 

Assignment Description 

In this assignment, you need to implement a text conferencing application. It consists of a 
server  with  registered  users,  and  several  client  nodes  which  are  the  end  points  for  a 
conference  session.  The  clients  may  be  in  different  networks.  TCP  will  be  used  to 
communicate between the clients and the server.  

Each  client  has  a  unique  name  referred  to  as  the  identifier  (ID).  The  ID  of  a  node  is 
unique among participating nodes. The server is assumed to have a list of client IDs that 
it will accept, and each ID is associated with  a password (perhaps  taken through a user 
registration procedure). A client must first log into the server with its ID and the server’s 
IP address. 

When  a  client  wishes  to  communicate  with  other  clients,  it  must  create  a  conference 
session on the server. A conference session is simply a list of clients that are sending text 
messages to one another. Any message sent by a single user is seen by all clients that are 
participating  in  the  conference  session.  It  is  the  server’s  responsibility  to  multicast  the 
text data to all participating clients. 

You will implement both the server and the client processes. 

Protocol  

We  use  two  categories  of  messages:  data  and  control.  Control  messages  are  used  for 
initiating the client-server interaction, session creation, invitation, termination, etc. Since 
you are using TCP, you do not need to use acknowledgements for reliability. You only 
need acknowledgements for success/failure conditions at the server, e.g. whether login is 
successful. 

You must use the following structure for data and control messages: 

 
 
 
 
 
struct message { 

unsigned int type; 
unsigned int size; 
unsigned char source[MAX_NAME]; 
unsigned char data[MAX_DATA]; 

}; 

The type field indicates the type of the message, as described in the table below. The size 
field  should  be  set  to  the  length  of  the  data.  The  source  field  contains  ID  of  the  client 
sending the message. 

The following table shows all control packets that must be implemented: 

Type 
LOGIN  

LO_ACK 

LO_NAK 

EXIT 

JOIN 

JN_ACK 

JN_NAK 

Packet Data 
<password> 

<reason for 
failure> 

<session ID> 
<session ID> 
<session ID, reason 
for failure> 

LEAVE_SESS   
NEW_SESS 

<session ID> 

NS_ACK 

MESSAGE 

QUERY 

QU_ACK 

<message data> 

<users and 
sessions> 

Table 1. Packet types 
Function 
Login with the server 
Acknowledge successful login 

Negative acknowledgement of login 

Exit from the server 
Join a conference session 
Acknowledge successful conference session join 

Negative acknowledgement of joining the session 

Leave a conference session 
Create new conference session 
Acknowledge new conference session 
Send a message to the session or display the message 
if it is received 
Get a list of online users and available sessions 

Reply followed by a list of users online 

Although you are required to use this protocol and struct, the serialization and 
deserialization approach is up to you. For example, you can use “:” to separate fields of 
the struct when you serialize it (as in the File Transfer lab), but if you want to experiment 
with a different approach that is fine too. 

 
 
 
 
 
 
 
 
 
 
 
Section 1 

Client Program (client.c):  

You  should  implement  a  client  program,  called  “client.c”,  in  C  on  a  UNIX  system.  Its 
command line input should be as follows: 

 client 

Upon execution, the client program will wait for user input on the command line. 

The login process should be clear from the available messages and command parameters. 
You  may  assume  that  the  passwords  are  sent  and  received  in  plain  text.  TCP  is  a 
bidirectional protocol, so once a connection has been established between the client and 
the server, communication may proceed. 

Once the client has logged in and joined a session, it will need to wait for messages from 
the server and at the same time continue to wait for user input on the command line. You 
will need a strategy for dealing with these two I/O sources simultaneously (e.g., starting a 
receive thread, or using non-blocking sockets). 

The client must implement the following commands on the stdin file stream: 

/login <client ID> 
<password> <server-IP> 
<server-port> 

/logout 

/joinsession <session ID> 

/quit 

<text> 

Table 2. Client commands 
Log into the server at the given address and port. The IP 
address is specified in the dotted decimal format 
Log out of the server, but do not exit the client. The client 
should return to the same state as when you started 
running it 
Join the conference session with the given session ID 
Leave the currently established session 

Get the list of the connected clients and available sessions 
Terminate the program 
Send a message to the current conference session. The 
message is sent after the newline 

/leavesession 
/createsession <session ID>  Create a new conference session and join it 
/list 

Here is an example of how clients in a session would work: 

Client 1: 

1.  Run the program: ./client 
2.  /login jill eW94dsol 128.100.13.140 5000 
3.  /createsession lab_help 

Client 2: 

1.  Run the program: ./client 
2.  /login jack 432wlFd 128.100.13.140 5000 
3.  /list  /* Returns a list of users and the sessions they joined */ 
4.  /joinsession lab_help 
5.  Hi Jill! This is Jack. How are TCP sockets different from UDP sockets? 

 
 
In  this  section,  a  client  is  only  allowed  to  join  one  session.  As  such,  the  server  should 
check whether a client is currently in a session or not.  

Server Program (server.c): 

You should implement a server program, named “server.c”, in C on a UNIX system. Its 

command input should be as follow: 

 server <TCP port number to listen on> 

You should have a strategy to deal with unavailable port numbers. For example, you can 
use the UNIX netstat command to find out which ports are available on the system. 

Upon  execution,  the  server  should  wait  for  connections  from  the  clients  in  the  system. 
The server acts as both a conference session router and a database. 

For the database part:  

•  The server has  access  to  a  client  list with passwords (which can be hard coded 

into your program or kept as a database file).  

•  The server should keep an up-to-date list of all connected clients in the system, 
as well as their session id, and IP and port addresses. The server should delete a 
conference session when the last user of the session leaves. 

The  server  should  acknowledge  some  client  requests  (i.e.  login  and  join  session).  You 
should deal with the possibility of a client attempting to log in with an ID that is already 
connected.  

The  server  implements  the  conference  functionality  of  the  system  by  forwarding 
messages  intended  for  a  conference  session  to  all  the  clients  registered  for  that 
session. This process should be transparent to the receiving clients. 

 
 
 
 
Section 2: Additional Features 

In  this  section,  you  must  implement  TWO  additional  features.  Some  important 
constraints: 

1.  The  commands  from  section  1  (see  Table  2)  must  remain  unchanged. 
Implement new commands if needed, rather than modifying the existing ones. 
2.  Your additional features must not be subsets of each other. This is not an issue for 
the features listed below, but be careful of this if you propose your own features. 

A list of approved features is included below (remember: pick exactly TWO features). 
You can also propose your own features, but if you choose to go this route you must get 
approval  from  a  TA  either  on  Piazza  or  via  email.  You  are  allowed  to  implement  any 
features that are approved on Piazza, regardless of who proposed them. 

1.  Multiple sessions. 

a.  Client:  Allow  a  clients  to  join  multiple  sessions.  You  should  clearly 

indicate on the client’s terminal the session ID of every message. 

b.  Server:  Keep  all  sessions  that  the  client  joined.  You  should  carefully 

design the up-to-date list (i.e., the output of /list) to reflect this. 

2.  Session invites. 

a.  Client:  Implement  a  procedure  for  a  client  to  invite  other  clients  into  a 
session. You must provide a protocol for a client to either accept or refuse 
an invitation. 

b.  Server: The server should be able to forward invitations and corresponding 

messages to the specific clients. 

3.  Inactivity timer. 

a.  Server:  Disconnect  clients  that  have  been  inactive  for  a  long  time, 
regardless  of  whether  they’re  in  a  session.  Note:  the  timer  and 
disconnection decision must be implemented at the server. 
b.  Client: Gracefully handle being disconnected from the server. 

4.  Private messaging. Clients can send private messages to each other, regardless of 
which session the sender and receiver  are in  (if any). Private messages  are only 
viewable by the target client, and it must be clear who the sender is. 

5.  User  registration.  If  a  client  does  not  already  have  a  username  and  password  to 
log  in  with,  it  can  register  with  the  server  to  create  an  account.  This  login 
information  must  be  persistent,  i.e.,  if  the  server  is  restarted,  any  users  who 
registered dynamically must still be able to log in. 

6.  Session admins.  When a client  creates a session, they become the admin of that 
session. A session admin can kick another client out of the session. They can also 
give the admin position to someone else. There must be a way for other clients to 
see who the admin of a session is. 

You should provide a text file to explain your design and implementation. 

 
Deliverables 

The following should be submitted and be available for your demo lab session: 

•  The  client  and  server  programs.  All  code  must  be  readable,  with  meaningful 
comments.  If  you  use  encryption  for  the  passwords,  make  sure  you  use  simple 
passwords and make them available separately. Your code must be able to run on 
the UG machines. 
It  wouldn’t  be  a  bad  idea  to  have  a  set  of  meaningful  test  cases  or  scenarios, 
which will test most of the functionality. 

• 

•  Set of Makefiles and/or UNIX scripts that simplify the compilation. 
•  The text file to explain Section 2.  

Notes 

•  This lab is long so you should allocate a proper amount of time for finishing it.  
•  For easy inspection, you may want to separate your code into more than two files; 

however, you will neither be punished nor rewarded for this. 

•  For  electronic  submission,  only  one  person  in  the  group  should  submit  the  file. 
You  should create a tar  ball (e.g., a4.tar.gz) with  all the files  needed to  compile 
and  run  your  programs.  You  should  also  include  a  Makefile  in  the  tar  ball  that 
compiles all your source code into two executables: one named server and one 
named client. 

•  The following command can be used to tar your files (where X is the lab number, 

ranging from 4 to 5 for the Text Conferencing lab): 
  tar –czvf aX.tar.gz <files to tar> 

•  Use the following command on the eecg UNIX system to submit your code: 

  submitece361s X aX.tar.gz 

•  You can perform the electronic submission any number of times before the actual 
deadline. A resubmission of a file with the same name simply overwrites the old 
version. The following command lists the files you have submitted: 
  submitece361s -l X 

•  You  are  expected  to  explain  the  design  choices  made  in  your  code  to  the  TAs 
during your demo  lab session. The lab specifications  are similar to  but  different 
from  those  in  previous  years  in  subtle  ways.  This  will  make  it  easy  to  spot 
plagiarized code. 

 
\n\n# Tutorial / Hints\n
ECE361 Text Conferencing Lab Tutorial 

Getting Started 
Organizing Your Development 
The text conferencing lab is broken down into 2 sections, each due in a different practical. This lab is much 
longer than the file transfer lab and is more open-ended; plan your time accordingly and remember to 
test frequently. If you are working on the client and your partner is working on the server (or vice versa), 
remember to try integrating your code early. In particular, make sure you agree on message formats. 

At the server, one of the first problems you will run into is how to deal with requests from multiple clients 
simultaneously. Similarly, at the client, you will quickly encounter a problem where you need to wait for 
user input on the command line and messages from the server simultaneously. Both are I/O multiplexing 
problems, and there are (at least!) two common approaches for dealing with them: 

1.  Synchronous I/O multiplexing. With this approach, we use special functions to block on multiple 
I/O sources simultaneously—the key point being that as soon as any of them have a message, our 
program unblocks. Blocking refers to a state where our program is stuck waiting for messages. 
2.  Multithreading. With this approach, we use different threads to block on each I/O source at the 
same time. Multithreading is a powerful programming technique, but can be difficult to debug if 
you’re not aware of the issues that can arise (e.g., synchronization). 

In this document, we will go over the basics of these approaches so that you can choose which is best for 
you. Note that your client and server can use different approaches. 

Caution: you may find sample code (e.g., in Beej’s guide) that uses multiple processes to handle requests 
at the server. We do not recommend that approach. Processes are much harder to use for this lab than 
threads,  since  threads  share  the  same  address  space  (and  hence  variables)  but  processes  don’t.  This 
makes it difficult to communicate (e.g., share variables) between processes. 

Testing 
You are required to run your server on a different UG machine than your client(s) when testing your 
code. To ensure proper handling of edge cases, we suggest you test your code with at least 3 clients 
connected to the server; it is fine if these clients are all running on the same machine as each other. 

In the lab sessions, we will ask you to demonstrate the correct functionality of the commands given in 
the lab handout. We have also thought of some scenarios that constitute edge cases! 

Readings 
This lab assumes familiarity with the sockets library used in the File Transfer lab. However, in this lab you 
must  use  TCP  instead  of  UDP;  accordingly,  the  programming  techniques  used  will  be  different.  The 
following  sections  will  provide  reference  material  on  TCP,  synchronous  I/O  multiplexing,  and 
multithreading. 

 
FAQ 
Section 1 
Q: When you send a message in a conference, does that message need to become visible to everyone 
in that session? 
A: Yes, if a user has joined a session then any messages they type (which are not commands) should go 
to everyone in that same session. This text conferencing lab is basically having a server set up a group 
chat between a set of people. 

Q: Is it okay to hardcode a maximum number of sessions at the server? 
A: Yes. Have a graceful way of handling the edge case though. 

Q: Why am I getting an undefined reference to pthread_x? 
A: You are probably not linking with the pthread library. Add LDFLAGS=-pthread to your makefile. 

Q: Why does recv read 0 bytes? 
A: Receiving a message of length 0 when using TCP on Linux means the connection was closed by the 
other end. 

Section 2 
Q: How many additional features do we need to implement? 
A: Two! The marks are split between them evenly. 

Q: What should go in the text file? How long does it need to be? 
A: You should explain what your new functionalities are, how they work, and the rationale behind your 
design (there are no length requirements, so it doesn’t have to be long). Imagine you are reading 
through your code a year from now, trying to remember what new features you chose for this lab and 
how they worked. Now imagine you open your explanation file. What would be helpful to have in there? 

 
 
 
Function Reference: TCP 
In the File Transfer lab, we saw that UDP sockets were message-oriented. Among other things, this 
meant that one call to sendto at the transmitter corresponded to one call to recvfrom at the 
receiver. This is not the case for TCP. 

TCP sockets are stream-oriented (or byte-oriented), meaning that message boundaries are not 
preserved. To put this another way, TCP sockets do not tell us where one message ends and another 
begins—all that TCP sees is a sequence of bytes. The restructuring of received data must be done at the 
application layer, by us. Take a minute to let this sink in! 

As an example, suppose I am the transmitter and you are the receiver. At some point, I send you the 
byte sequence “hello”. A little while later, I send you “world”. 

-  Case 1: If you call the receive function with a buffer of size 10 (big enough to fit everything I sent 

you) then you will read out “helloworld”. 

-  Case 2: If instead you call receive with a size-7 buffer, you will read out “hellowo”. If you call 

receive again you will read out “rld”. 

This example illustrates two major differences from UDP. First, if we transmit two separate byte 
sequences, the receiver cannot distinguish them—instead, it just sees one long sequence. Second, if you 
call the receive function with a buffer that is not sufficiently large to empty the receive queue, then the 
leftover bytes will be returned on subsequent receive calls (rather than being discarded). 

Suggestion: A common technique for dealing with the stream-oriented nature of TCP is to transmit the 
length of the message before transmitting the message itself (you need to figure out an unambiguous 
way to do that). Once the receiver reads out this length, it will know how many more bytes it needs to 
read before it reaches the message boundary. If the receiver reads exactly this many bytes (no more and 
no less), we will effectively have message-oriented TCP. This is not difficult, but it is more nuanced than 
it appears at first glance. For example, suppose we find out that the message is 900 bytes long. We then 
make a call to our receive function with a buffer size of 900 bytes. If 500 bytes are already available, 
then our receive function will return immediately after populating our buffer with those 500 bytes. We 
then need to be careful that next time we call our receive function, we pass in a buffer of only 400 bytes. 
If we passed in a buffer of 900 bytes again, we might accidentally receive part of the next message, 
which could be complicated to deal with! 

A few exercises related to this suggestion (not for marks): 

-  How can we transmit the message length unambiguously? That is, how does the receiver know 

which bytes contain the message length? 

-  How can we ensure that the receiver reads the message length from the socket without reading 

any of the message contents? 

-  What unintended consequences might arise from passing in, e.g., a 2000 byte receive buffer 

when trying to read the message length from the socket? 

socket 
We are already familiar with socket, thanks to the File Transfer lab! The one thing worth pointing out 
is that since we must use TCP for this lab, we will need to pass in SOCK_STREAM as the socket type. 

connect 
connect is used to try to establish a TCP connection. Usually, we use this as a client when trying to 
establish a connection with a server. If connect is successful, then we can start sending and receiving 
data in a reliable, stream-oriented manner. 

1. Socket FD 

2. Desired address information 

3. Size of address information 

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen); 

Arguments 

1.  The socket FD we want to use to establish the connection. 
2.  (POINTER) Address we want to connect with. Fill this in with the server’s address information. 
3.  Size of the struct sockaddr passed in for argument 2. 

Return Value 
Returns 0 on success and -1 on failure. If connect fails, it is likely because your server is not running, or 
you entered its address information incorrectly. 

References 

-  https://man7.org/linux/man-pages/man2/connect.2.html 

listen 
Typically, a server will have one special socket called a listener, which does nothing except wait for 
incoming connection requests. When a connection request arrives, the listener reaches under the hood 
in the socket library, indicates that there is a pending connection, and then continues listening. This is 
because the listener’s only job is to listen—it is not responsible for dealing with the connection requests 
(that is what accept is for). 

To mark a socket as a listener, we pass it into the listen function. This function will return 
immediately, and (assuming success) the socket will automatically start listening for connection requests 
in the background. 

1. Listener FD  2. Size of backlog queue 

int listen(int sockfd, int backlog); 

Arguments 

1.  The socket we wish to mark as a listener. 
2.  The maximum number of pending connection requests. This value matters if connection 

requests arrive faster than the server can respond to them. 

Return Value 
Returns 0 on success and -1 on failure. 

References 

-  https://man7.org/linux/man-pages/man2/listen.2.html 

 
 
accept 
Whenever the listener indicates that there is a pending connection, the server can complete that 
connection by calling accept. This function does not make the listener part of the connection; instead, 
it creates a new socket to complete the connection. This way, the listener can keep on listening. 

1. Listener FD 

2. Address information of peer 

int accept(int sockfd, struct sockaddr *restrict addr, 
           socklen_t *restrict addrlen); 

3. Size of peer’s address information 

Arguments (inputs) 

1.  Socket FD of the listener. This socket must be bound and listening. 

Arguments (outputs) 

2.  (POINTER) Like recvfrom, you need to create an empty struct sockaddr_storage and 
pass it into  accept via pointer. The address information of peer (connection initiator) will be 
automatically filled in. 

3.  (POINTER) Same purpose as the addrlen argument for recvfrom. Initialize it the same way. 

Return Value 
Returns the FD for the newly created socket upon success and -1 on failure. To communicate with the 
peer who requested the connection, you will use this new socket. 

References 

-  https://man7.org/linux/man-pages/man2/accept.2.html 

send 
send is used to send a message through a connected socket. As mentioned earlier, be cautious about 
the fact that TCP does not preserve message boundaries. One nice thing about being connected is that 
we don’t need to pass in the destination address every time we send or receive; this is remembered by 
the socket library under the hood. Alternatively, the standard file I/O function write can be used 
instead of send. 

1. Socket FD 

2. Message buffer 

4. Options 

3. Size of message buffer 

ssize_t send(int sockfd, const void *buf, size_t len, int flags); 

Arguments 

1.  The socket FD tells send which socket you want to use to send messages. This socket must be 

connected. 

2.  (POINTER) Pointer to the message (i.e., byte sequence) you want to send. 
3.  Length of the message you want to send (i.e., number of bytes). 

 
 
 
 
 
4.  This argument allows you to specify transmit options, if needed. 

Return Value 
Returns the number of bytes sent on success and -1 on failure. 

References 

-  https://man7.org/linux/man-pages/man2/send.2.html 

recv 
recv is used to receive bytes from a socket. Alternatively, the standard file I/O function read can be 
used instead of recv. 

As has been emphasized throughout this document, when we use TCP the connection becomes stream-
oriented. What this means for us, as programmers, is we need to keep track of where one message ends 
and another begins (one strategy for this was suggested in at the start of the Function Reference: TCP 
section). In any case, messages are not given to us atomically as with UDP, so we need to exercise more 
caution with the recv function to make sure that the logic for receiving messages is correct. 

3. Size of receive buffer 

1. Socket FD 

2. Receive buffer 

4. Options 

ssize_t recv(int sockfd, void *buf, size_t len, int flags); 

Arguments 

1.  The socket FD tells recv which socket you want to receive bytes from. 
2.  (POINTER) Address of buffer for recv to copy received bytes into. 
3.  Length of your receive buffer. This can be thought of as the maximum number of bytes you wish 
to receive. This is a bit different than the story for UDP, and again it comes back to TCP not 
preserving message boundaries (sometimes we might not want to receive more than X bytes, 
because we know that the message currently being received is X bytes long). 

4.  This argument allows you to specify some options, if needed. 

Return Value 
Returns the number of bytes received on success, -1 on failure, and 0 if the connection was closed from 
the other side (i.e., by the peer). 

References 

-  https://man7.org/linux/man-pages/man2/recv.2.html 

 
 
 
Function Reference: Synchronous I/O Multiplexing 
This is a particularly powerful technique for dealing with the I/O multiplexing problem at the server (it can 
also be used at the client though). Some references to help get started include: 

- 
- 

Section 7.3 in Beej’s Guide: https://beej.us/guide/bgnet/html/#select. Highly recommended! 
Linux manual page on select: https://man7.org/linux/man-pages/man2/select.2.html 

If you plan on using synchronous I/O multiplexing, you must read section 7.3 from Beej’s Guide because 
it contains an excellent introduction to the topic as well as sample code. There is not much we can add 
that hasn’t already been said, so this section will be shorter than the others. 

select 
Synchronous I/O multiplexing is possible because the select function allows us to monitor sets of FDs 
simultaneously. This is summarized well in Beej’s guide, “select() gives you the power to monitor several 
sockets at the same time. It’ll tell you which ones are ready for reading, which are ready for writing, and 
which sockets have raised exceptions, if you really want to know that”. 

2. Set of FDs to watch for reading 

3. Set of FDs to watch for writing 

1. Highest FD + 1 

int select(int nfds, fd_set *restrict readfds, fd_set *restrict writefds, 
           fd_set *restrict exceptfds, struct timeval *restrict timeout); 

4. Set of FDs to watch for exceptions 

5. Timeout period 

Arguments 

1.  You need to pass in the value of the largest FD in your set, plus one. Make sure you keep this 

value up to date when you add and remove FDs from the set. 

2.  (POINTER) Here, you specify the set of FDs that you want select to monitor for readability. 

From the manual, “A file descriptor is ready for reading if a read operation will not block…After 
select() has returned, readfds will be cleared of all file descriptors except for those that are 
ready for reading”. This last part means that prior to calling select, you might want to make a 
copy of your readfds if you plan to use it again. 

3.  (POINTER) Set of FDs that you want select to monitor for writability. You can pass in NULL if 

you don’t need this. 

4.  (POINTER) Set of FDs that you want select to monitor for exceptions. You can pass in NULL if 

you don’t need this. 

5.  (POINTER) Specifies the maximum amount of time that select should wait before returning. 

You can pass in NULL if you don’t need this. 

Return Value 
Returns the total number of FDs contained in the modified FD sets (read, write, except) on success, -1 on 
failure, and 0 on timeout. 

References 
Section 7.3 in Beej’s Guide and the Linux manual page on select, linked at the start of this section. 

 
 
 
 
 
 
Function Reference: Multithreading 
Multithreading can be achieved using the pthreads library. Before using this library, you will need to add 
the following line to your makefile: LDFLAGS=-pthread. Some guides for getting started include: 

1.  POSIX Threads Programming: https://hpc-tutorials.llnl.gov/posix/ 
2.  Linux Tutorial: POSIX Threads: https://www.cs.cmu.edu/afs/cs/academic/class/15492-

f07/www/pthreads.html 

3.  Multithreaded Programming (POSIX pthreads tutorial): https://randu.org/tutorials/threads/ 

The key thing to understand is that multiple threads can be used to run tasks in your program 
independently. When programming with multiple threads, we often think of the threads as executing 
concurrently. This can be helpful for identifying scenarios in which threads may conflict with each other, 
and thus need to communicate among themselves or synchronize. In reality, threads do not always 
execute concurrently since their execution is determined by the scheduler within the operating system 
(OS)—this is a detail we will revisit soon. 

When you create a thread, you pass it an entry point, which is a function for it to execute. All programs 
consist of at least one thread (called the main thread) which is given the main function as its entry 
point. With pthreads, to create a thread which runs the function my_func and passes argument 
my_arg into my_func, you can run the following code in your main thread: 

pthread_t new_thread; 
pthread_create(new_thread, NULL, my_func, (void*)&my_arg); 

A few things to note: 

1.  When the new thread returns from my_func, the new thread shuts down. As such, if 
my_func is an infinite loop, then the new thread will continue running forever. 
It may be the case that you want your main thread to wait for the new thread to shut down, in 
this case you can call the function pthread_join from your main thread. Note that this will 
not cause the thread to shut down, you need to figure out some other way of doing that. 

2. 

Before going further, let’s return to the I/O multiplexing problem for the text conferencing lab, where 
you wish to receive messages from multiple sources simultaneously. A multithreaded approach to 
solving this issue would begin by using separate threads to listen to each I/O source independently. For 
example, you could use your main thread to listen to one I/O source and create threads (along with 
functions for them to run) that listen to the other I/O sources. When one of the sources produces a 
message, the thread that’s listening to it will then be able to respond to it while the other threads 
continue listening (or responding) to their own I/O sources. Note that you can call send from one 
thread and recv from another thread on the same socket without issues (source). 

When designing a multithreaded program, remember that threads may execute concurrently (but not 
always). Whether this happens depends on how your OS chooses to schedule the threads, and what the 
state of each thread is. It is possible for a thread to be blocked/waiting, which means it is waiting for a 
resource that is currently being used by a different thread. An example of such a resource is a mutex, 
which we will discuss next. 

 
An important thing to know about threads (of the same process) is that they share the same address 
space, i.e., they can all access and modify global variables in your program. Without going into too much 
detail, let's just say that multiple threads accessing shared variables at the same time can lead to 
unexpected behaviour. To deal with such scenarios, we can make use of a synchronization primitive 
called a mutex. A nice summary can be found on this site: "a Mutex is a lock that we set before using a 
shared resource and release after using it. When the lock is set, no other thread can access the locked 
region of code". When the mutex lock is set, another thread which tries to acquire it will be put into the 
blocked state; this thread will be unblocked when it is its turn to access the shared resource. The 
functions you'll need to know about for mutexes are: 

1.  pthread_mutex_init 
2.  pthread_mutex_lock, pthread_mutex_unlock 

You are not required to use mutexes in your code, but they could be helpful. Remember: if you have 
multiple threads reading or writing the same resource (e.g., variable or data structure), it is almost 
certainly the case that you need to use a mutex to control access to that resource. This site shows an 
example of a mutex in action. 

