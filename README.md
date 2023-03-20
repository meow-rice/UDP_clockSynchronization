# UDP_clockSynchronization
Clock synchronization project for CSCI 5673

`make` to compile both client and server

`./client 2` to connect to local server on port 8100

Note: The client currently does not do retransmissions (just best-effort UDP)  
Note: The server must be reset each time the client runs. It currently only accepts one client in its entire lifetime.
