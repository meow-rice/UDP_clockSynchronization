/**
 * CSCI 5673 Distributed Systems
 * Programming Assignment 3 -- NTP
 * Due Tues, 3/21/23
 * John Salame
 * Maurice Alexander
 */

// NOTE: This server is built to only handle one client at a time for the whole 1 hour session.
// NOTE: With current implementation, you need to stop and restart the server in order to allow a new client to join
// Socket code is based on this example but modified for UDP: https://www.geeksforgeeks.org/socket-programming-cc/

#include "client.h" // has some constants and structs

// GLOBALS
struct ntpTime org;
struct ntpTime xmtTimes[NumMessages]; // local time we sent each request
struct ntpTime recvTimes[NumMessages]; // local time each response was received
struct ntpTime lastRecvTime;
char globalStratum = 1;

int main(int argc, char** argv) {
    short port = 8100;
    int maxConnections = 1;
    int server_fd, clientConnection, valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[packetSize] = { 0 };

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket failed");
        exit(1);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(1);
    }
    printf("Bound the socket to port %d and ready to receive packets\n", port);

    // start the clock
    clock_t startTime; // time on the clock when you started the program (we initialize this in main after the socket connects)
    time_t startTimeInSeconds; // system time when you started the program (measured at the same time as startTime)
	startTimeInSeconds = time(NULL); // system time, epoch 1970
	startTime = clock(); // processor time measured in CLOCKS_PER_SEC

    // respond to requests from the client until we end the server with CTRL + C
    while(true) {
        // get a request (use 0 for responsePos, the array element we'll modify)
        // this function automatically sets org and lastRecvTime for us.
        recvMsg(server_fd, recvTimes, 0, &org, &lastRecvTime, startTimeInSeconds, startTime);
        printf("Received packet");
        // this function will automatically calculate the transmit time and use the previously found org and receive times.
        sendMsg(server_fd, xmtTimes, 0, globalStratum, org, lastRecvTime, startTimeInSeconds, startTime);
    }
    printf("Stopping program.\n");
    return 0;
 }
