# UDP_clockSynchronization
Clock synchronization project for CSCI 5673

### Compiling
`make` to compile both client and server

### Running
`./client` to connect to public NTP server
`./client 2` to connect to local server on port 8100
`./client test` or `./client 2 test` in order to use short program length and burst intervals (for testing purposes only)

### Limitations
Note: The client uses retransmission to re-send packets, with an interval of 8 seconds between packets (8 is also the retransmission timeout). This decision was made due to the message in Slack: 
```
All users should ensure that their software NEVER queries a server more frequently than once every 4 seconds. Systems that exceed this rate will be refused service. In extreme cases, systems that exceed this limit may be considered as attempting a denial-of-service attack.
```
Note: There may be fewer than 8 delays and offsets if the packets are not all received within the burst window, even with retransmission.
Note: The client and server are built for Linux and are not portable.
