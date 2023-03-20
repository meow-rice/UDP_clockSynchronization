/**
 * CSCI 5673 Distributed Systems
 * Programming Assignment 3 -- NTP
 * Due Tues, 3/21/23
 * John Salame
 * Maurice Alexander
 */

// NOTE: This server is built to only handle one client at a time for the whole 1 hour session.
// NOTE: With current implementation, you need to stop and restart the server in order to allow a new client to join
// Socket code is based on this example: https://www.ibm.com/docs/en/zos/2.1.0?topic=programs-c-socket-udp-server

#include "client.h" // has some constants and structs

// GLOBALS
const int isServer = 1;
struct ntpTime org;
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

    // start the clock
    clock_t startTime; // time on the clock when you started the program (we initialize this in main after the socket connects)
    time_t startTimeInSeconds; // system time when you started the program (measured at the same time as startTime)
	startTimeInSeconds = time(NULL); // system time, epoch 1970
	startTime = clock(); // processor time measured in CLOCKS_PER_SEC

    clientAddressSize = sizeof(client);

    // respond to requests from the client until we end the server with CTRL + C
    while(true) {
        // get a request (use 0 for responsePos, the array element we'll modify)
        // this function automatically sets org and lastRecvTime for us.
        recvMsg(serverfd, recvTimes, 0, &org, &lastRecvTime, startTimeInSeconds, startTime, isServer, &client, &clientAddressSize);
        printf("Received packet from client %s, port %d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        /*
        // build a connection back to the client
        // ASSUMPTION: Only one client will ever connect to this server in the server's lifetime!
        if(clientfd == 0) {
            printf("Creating connection back to client\n");
            if(connect(clientfd, (struct sockaddr*)&client, clientAddressSize)) {
                perror("Error creating connection back to client\n");
                exit(1);
            }
        }
        */

        // this function will automatically calculate the transmit time and use the previously found org and receive times.
        sendMsg(serverfd, xmtTimes, 0, globalStratum, org, lastRecvTime, startTimeInSeconds, startTime, isServer, &client, &clientAddressSize);
    }
    printf("Stopping program.\n");
    if(serverfd > 0)
        close(serverfd);
    return 0;
 }
