# UDP_clockSynchronization
Clock synchronization project for CSCI 5673

## Group Members
John Salame
Maurice Alexander

### Timing Studies
The graphData files contain delays, offests, min delay, and min offest for each burst.  
The rawMeasurementData files contain values of T1, T2, T3, and T4 for each packet. The data is split into seconds and milliseconds.  
The files have IP addresses embedded in their names.
* 10.128.0.23 is for two machines in the same LAN.
* 34.27.33.102 is for a laptop connecting to a server on the Cloud.
* 132.163.96.1 is for the public server.

#### Explanation of Timing Studies
For two machines in the same LAN, the offset is consistently around -3 milliseconds. The delays are about 1/10th of that and positive (about 3 in a thousand).  
For a laptop connecting to a Cloud server, the offset is around -8 milliseconds and the delays are around two hundredths of a second.  
For the public server, the offset is a few milliseconds (negative) but less stable than the same-LAN machines. The delay is one or two hundredths of a second.

The Cloud server has the worst accuracy, as expected because the cluster is not in Colorado. The LAN had good accuracy due to low latency within the Google Cloud cluster.
The public server's accuracy was somewhere in between. We chose a NIST server in Boulder.


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

#### Running in the Cloud
In Google Cloud, go to Compute Engine > VM and create instance with all default settings.
When it comes up, click the SSH button.
```
sudo apt-get update
sudo apt-get install make
sudo apt-get install g++
make
```
Do the above for two virtual machines.  
Then, go to VPC Network > Firewall and add a firewall rule to allow port 8100.
* Create Firewall Policy (near the top)
* name: allow8100
* Continue
* Add Rule
* * Priority 0
* * Direction Ingress, allow on Match
* * Source IP range 0.0.0.0/0
* * Protocols and ports -> specified protocols and ports
* * UDP 8100
* Continue
* Associate with default network
* Create

On the sever VM, do ./server
On the client VM for LAN, get the server VM's internal IP from Google Cloud Compute Engine and do `./client 2 --server ip_address`  
For connection from your own computer outside the Cloud, use the public/external IP.

### Limitations
Note: The client uses retransmission to re-send packets, with an interval of 8 seconds between packets (8 is also the retransmission timeout). This decision was made due to the message in Slack: 
```
All users should ensure that their software NEVER queries a server more frequently than once every 4 seconds. Systems that exceed this rate will be refused service. In extreme cases, systems that exceed this limit may be considered as attempting a denial-of-service attack.
```
Note: There may be fewer than 8 delays and offsets if the packets are not all received within the burst window, even with retransmission.
Note: The client and server are built for Linux and are not portable.  
Note: The server uses its system clock as a time source rather than another NTP server or other external source.  
