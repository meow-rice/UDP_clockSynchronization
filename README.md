# UDP_clockSynchronization
Clock synchronization project for CSCI 5673

### Compiling
`make` to compile both client and server

### Running
#### Running the Server
`./server` to run the server
#### Running the Client
`./client` to connect to public NTP server. Without a `--server` argument, it connects to `132.163.96.1`.  
`./client 2` to connect to a server on port 8100 (this is the server we made). Without a `--server` argument, it connects to a server on localhost.  
`./client test` or `./client 2 test` in order to use short program length and burst intervals (for testing purposes only)  
`./client --server ip_or_hostname` to specify the server to connect to.  
Full syntax: `./client [2] [--server ip_or_hostname] [test]`

### Limitations
Note: The client uses retransmission to re-send packets, with an interval of 8 seconds between packets (8 is also the retransmission timeout). This decision was made due to the message in Slack: 
```
All users should ensure that their software NEVER queries a server more frequently than once every 4 seconds. Systems that exceed this rate will be refused service. In extreme cases, systems that exceed this limit may be considered as attempting a denial-of-service attack.
```
Note: There may be fewer than 8 delays and offsets if the packets are not all received within the burst window, even with retransmission.
Note: The client and server are built for Linux and are not portable.  
Note: The server uses its system clock as a time source rather than another NTP server or other external source.  
