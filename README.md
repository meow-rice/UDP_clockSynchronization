# UDP_clockSynchronization
Clock synchronization project for CSCI 5673

### Compiling
`make` to compile both client and server

### Running
`./client 2` to connect to local server on port 8100
`./client test` or `./client 2 test` in order to use short program length and burst intervals (for testing purposes only)

### Limitations
Note: The client currently does not do retransmissions (just best-effort UDP)  
Note: The client and server are built for Linux and are not portable.
