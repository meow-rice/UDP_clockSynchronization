/**
 * CSCI 5673 Distributed Systems
 * Programming Assignment 3 -- NTP
 * Due Friday, 3/17/23
 * John Salame
 * Maurice Alexander
 */

#include "client.h"
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#ifdef __unix__
	#include <arpa/inet.h>
	#include <sys/socket.h>
#elif defined(_WIN32) || defined(WIN32)
	#include <winsock2.h>
	#include <ws2tcpip.h>
	// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
	#pragma comment (lib, "Ws2_32.lib")
	#pragma comment (lib, "Mswsock.lib")
	#pragma comment (lib, "AdvApi32.lib")
#endif

// globals
char* serverName = NULL;
short serverPort = 123; // default server port
struct sockaddr_in serv_addr; // Server address data structure.
struct hostent* server;      // Server data structure.
int sockfd; // socket connection
char globalStratum = 0; // unspecified
double delays[8];
double offsets[8];
struct ntpTime org; // originate timestamp the client will send
struct ntpTime recvTimes[8]; // local time each response was received
struct ntpPacket responses[8];
int responsePos = 0; // index in responses we will read to
clock_t pollingInterval = 4 * 60 * CLOCKS_PER_SEC; // 4 minutes between bursts
const int packetSize = sizeof(struct ntpPacket);

void error(char* msg) {
	perror(msg); // print to stderr
	exit(0);
}

#ifdef __unix__
void connectToServerUnix(const char* hostName, short port) {
	// socket code copied from https://lettier.github.io/posts/2016-04-26-lets-make-a-ntp-client-in-c.html
	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // Create a UDP socket.
	if (sockfd < 0)
		error("ERROR opening socket");

	server = gethostbyname(hostName); // Convert URL to IP.
	if (server == NULL)
		error("ERROR, no such host");

	// on changing bzero and bcopy https://stackoverflow.com/questions/18330673/bzero-bcopy-versus-memset-memcpy
	// Zero out the server address structure.
	memset((char*)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	// Copy the server's IP address to the server address structure.
	memmove((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr, server->h_length);
	// Convert the port number integer to network big-endian style and save it to the server address structure.
	serv_addr.sin_port = htons(port);
	if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR connecting");
}
#endif

#if defined(_WIN32) || defined(WIN32)
void connectToServerWindows(const char* hostName, short port) {
	// Tutorial: https://learn.microsoft.com/en-us/windows/win32/winsock/complete-client-code?source=recommendations
	WSADATA wsaData;
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo* result = NULL,
		* ptr = NULL,
		hints;
	int iResult = 0;
	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
}
#endif

void connectToServer(const char* hostName, short port) {
#ifdef __unix__
	connectToServerUnix(hostName, port);
#elif defined(_WIN32) || defined(WIN32)
	connectToServerWindows(hostName, port);
#endif
}

// Return the current system time as an NTP time
struct ntpTime getCurrentTime() {
	// TODO: Implement this function
	struct ntpTime ret;
	ret.intPart = 0;
	ret.fractionPart = 0;
	return ret;
}
// Calculate the difference in seconds between two NTP times.
double timeDifference(struct ntpTime fisrtTime, struct ntpTime secondTime) {
	// TODO: Implement this function
	return 0.0;
}
double calculateOffset(struct ntpTime T1, struct ntpTime T2, struct ntpTime T3, struct ntpTime T4) {
	// TODO: Implement this function
	return 0.0;
}
double minOffset(double offsets[8]) {
	double min = offsets[0];
	for (int i = 1; i < 8; ++i) {
		if (offsets[i] < min) {
			min = offsets[i];
		}
	}
	return min;
}
double calculateRoundtripDelay(struct ntpTime T1, struct ntpTime T2, struct ntpTime T3, struct ntpTime T4) {
	// TODO: Implement this function
	return 0.0;
}
double minDelay(double delays[8]) {
	double min = delays[0];
	for (int i = 1; i < 8; ++i) {
		if (delays[i] < min) {
			min = delays[i];
		}
	}
	return min;
}

// Send the ntp packet.
void sendMsg() {
	char buf[packetSize];
	// set every byte in the ntpPacket object to 0 so stuff we don't care about is 0
	memset(buf, 0, packetSize);
	char li = 0;
	char n = 4;
	char mode = 3; // client
	char stratum = globalStratum;
	if (org.intPart != 0) {
		org = getCurrentTime();
	}
	// TODO: Fill out other values
	//    Figure out if polling interval in the packet is 4 minutes or something else. If it is 4 minutes, use value from pollingInterval global variable.
	//    Figure out how to calculate precision (probably uses CLOCKS_PER_SECOND from time.h)
	// TODO: Move packet into byte buffer
	// TODO: Send the byte buffer over the socket.
}

struct ntpPacket recvMsg() {
	char buf[packetSize];
	struct ntpPacket ret;
	// clear memory in case it holds garbage values
	memset(buf, 0, packetSize);
	memset((void*)&ret, 0, packetSize); // zero out the values in the packet object
	// TODO: Read the socket into a byte buffer.
	recvTimes[responsePos] = getCurrentTime();
	// TODO: parse byte buffer into an ntpPacket object (ret).
	return ret;
}

// In case responses arrive out of order, we need to sort by sequential time. Maybe it would be by server's org, rcv, or xmt.
// I'm not sure how it would handle out-of-order receipt by the server.
void sortResponses(struct ntpPacket responses[8]) {
	// TODO: Implement this function
}

int main(int argc, char** argv) {
	// Times are based on time.h https://www.tutorialspoint.com/c_standard_library/time_h.htm
	clock_t programLength = 60 * CLOCKS_PER_SEC; // number of seconds to run the program (should be 1 hour for the real thing)
	clock_t timeBetweenBursts = pollingInterval;
	clock_t startTime; // time on the clock when you started the program (we initialize this after the socket connects)
	clock_t startOfBurst;
	clock_t curTime; // time passed since the start time

	// Default server name
	char defaultServerName[] = "localhost";
	// TODO: have tmp point to a command line argument if we have a command line argument for server name
	char* tmp = defaultServerName;
	int hostnameLen = strlen(tmp) + 1; // add 1 to make room for null terminator
	serverName = malloc(hostnameLen);
	strncpy(serverName, tmp, hostnameLen); // set the server name

	// TODO: Possibly provide server port by command line args and modify the global serverPort

	// zero out the delays and offsets arrays
	memset((char*)&delays, 0, 8 * sizeof(double));
	memset((char*)&offsets, 0, 8 * sizeof(double));

	// initialize org
	org.intPart = 0;
	org.fractionPart = 0;

	// zero out the responses and recvTimes
	memset((char*)&recvTimes, 0, 8*sizeof(struct ntpTime));
	memset((char*)&responses, 0, 8*packetSize);

	// connect to the server
	connectToServer(serverName, serverPort);

	// start the clock
	startTime = clock();

	// loop until program should end
	while (curTime < programLength) {
		responsePos = 0; // start reading things into the start of the length 8 arrays
		int prevResponsePos = -1;
		curTime = clock() - startTime; // time since the start of the program
		startOfBurst = curTime;
		// Do a burst
		while (curTime - startOfBurst < timeBetweenBursts && responsePos < 8) {
			// For now, send the messages in the burst one-at-a-time
			if (responsePos != prevResponsePos) {
				sendMsg();
				prevResponsePos = responsePos; // catch up so we never call sendMsg() until after we successfully receive a message
			}

			// TODO: Figure out if we need the burst packets to occur at two-second intervals as stated on page 3

			// check if the socket has our message https://stackoverflow.com/questions/5168372/how-to-interrupt-a-thread-which-is-waiting-on-recv-function
			// note: the ioctlsocket might be Windows dependent. If it is, we can find a replacement, but this is the general idea. A return value of 0 means success.
			long unsigned int bytesToRead = packetSize;
			if (!ioctlsocket(sockfd, FIONREAD, &bytesToRead) && bytesToRead == (long unsigned int) packetSize && responsePos < 8) {
				responses[responsePos] = recvMsg();
				globalStratum = responses[responsePos].stratum;
				++responsePos;
			}
			curTime = clock() - startTime;
		}
		// Burst is finished.
		sortResponses(responses);
		for (int i = 0; i < responsePos; i++) {
			// TODO: calculate offsets and delays for all responses we received
		}
		double _minDelay = minDelay(delays);
		double _minOffset = minOffset(offsets);
		// TODO: Write delay and offset data to file
	}

	free(serverName); // free the memory
	return 0;
}
