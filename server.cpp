/**
 * CSCI 5673 Distributed Systems
 * Programming Assignment 3 -- NTP
 * Due Tues, 3/21/23
 * John Salame
 * Maurice Alexander
 */

// NOTE: This server is built to only handle one client at a time for the whole 1 hour session.
//    This is not actually confirmed. We have tested with only one client.
// Socket code is based on this example: https://www.ibm.com/docs/en/zos/2.1.0?topic=programs-c-socket-udp-server

#include "client.h" // has some constants and structs

// GLOBALS
const bool isServer = true;
struct ntpTime org;
// I don't actually use arrays, I only use the first element. It is just to integrate with sendMsg and recvMsg.
struct ntpTime xmtTimes[NumMessages]; // local time we sent each request
struct ntpTime recvTimes[NumMessages]; // local time each response was received
struct ntpTime lastRecvTime;
char globalStratum = 1;

int main(int argc, char** argv) {
    short port = 8100;
    int serverfd, clientAddressSize;
    serverfd = clientAddressSize = 0;
    struct sockaddr_in server, client;
    int addrlen = sizeof(server);
    clientAddressSize = sizeof(client);

    // Create server socket (file descriptor, not connection)
    if ((serverfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket failed");
        exit(1);
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    if (bind(serverfd, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("bind failed");
        exit(1);
    }
    printf("Bound the socket to port %d and ready to receive packets\n", port);

    // initialize org
	org.intPart = 0;
	org.fractionPart = 0;

	// zero out the responses and recvTimes
    memset((char*)&xmtTimes, 0, NumMessages*sizeof(struct ntpTime));
	memset((char*)&recvTimes, 0, NumMessages*sizeof(struct ntpTime));
	memset((char*)&lastRecvTime, 0, sizeof(struct ntpTime));

    // start the clock
    clock_t startTime; // time on the clock when you started the program (we initialize this in main after the socket connects)
    time_t startTimeInSeconds; // system time when you started the program (measured at the same time as startTime)
	startTimeInSeconds = time(NULL); // system time, epoch 1970
	startTime = clock(); // processor time measured in CLOCKS_PER_SEC

    // respond to requests from the client until we end the server with CTRL + C
    while(true) {
        // get a request (use 0 for responsePos, the array element we'll modify)
        // this function automatically sets org and lastRecvTime for us.
        recvMsg(serverfd, recvTimes, 0, &org, &lastRecvTime, startTimeInSeconds, startTime, isServer, &client, &clientAddressSize);
        printf("Received packet from client %s, port %d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        // printf("Org is %u.%f\n", org.intPart, pow(2, -32) * (double) org.fractionPart);
        // printf("Last receive time is %u.%f\n", lastRecvTime.intPart, pow(2, -32) * (double) lastRecvTime.fractionPart);

        // this function will automatically calculate the transmit time and use the previously found org and receive times.
        sendMsg(serverfd, xmtTimes, 0, globalStratum, org, lastRecvTime, startTimeInSeconds, startTime, isServer, &client, &clientAddressSize);
    }
    printf("Stopping program.\n");
    if(serverfd > 0)
        close(serverfd);
    return 0;
 }
