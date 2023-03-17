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
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
// Network stuff
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

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
struct ntpTime xmtTimes[8]; // local time we sent each request
struct ntpTime recvTimes[8]; // local time each response was received
struct ntpPacket responses[8];
int responsePos = 0; // index in responses we will read to
clock_t pollingInterval = 4 * 60 * CLOCKS_PER_SEC; // 4 minutes between bursts
clock_t startTime; // time on the clock when you started the program (we initialize this in main after the socket connects)
time_t startTimeInSeconds; // system time when you started the program (measured at the same time as startTime)
const int packetSize = sizeof(struct ntpPacket);

void error(char* msg) {
	perror(msg); // print to stderr
	exit(0);
}

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

void connectToServer(const char* hostName, short port) {
	connectToServerUnix(hostName, port);
}

// Return the current system time as an NTP time. I'm not sure how precise this method is.
// Constraint: Algorithm only works on a Little Endian system.
// Precondition: baselineTime and baselineInApplicationClock are measured at the same time prior to calling this function.
struct ntpTime getCurrentTime(time_t baselineTime, clock_t baselineInApplicationClock) {
	clock_t currentTime = clock();
	clock_t diffInTimeUnits = currentTime - baselineInApplicationClock;
	double diffInSeconds = (double) diffInTimeUnits / CLOCKS_PER_SEC;
	// attempting to extract fraction part and int part without losing precision https://stackoverflow.com/questions/5589383/extract-fractional-part-of-double-efficiently-in-c
	double diffInFractionalSeconds = diffInSeconds - floor(diffInSeconds);
	int integerSecondsSinceBaseline = (int) floor(diffInSeconds);
	time_t currentTimeSince1900 = baselineTime + integerSecondsSinceBaseline - NTP_TIMESTAMP_DELTA;

	struct ntpTime ret;
	ret.intPart = (unsigned int) currentTimeSince1900; // must fit in 32 bits
	ret.fractionPart = (unsigned int) (diffInFractionalSeconds * pow(2,32)); // fractional part represented as a sequence of 32 bits, assuming little endian
	return ret;
}
int ntpTimeEquals(struct ntpTime t1, struct ntpTime t2) {
	return (t1.intPart == t2.intPart && t1.fractionPart == t2.fractionPart);
}
// Calculate the difference in seconds between two NTP times.
// Return secondTime - firstTime
double timeDifference(struct ntpTime firstTime, struct ntpTime secondTime) {
	unsigned int secondsDifference = secondTime.intPart - firstTime.intPart;
	double fractionalDifference = pow(2,-32) * (secondTime.fractionPart - firstTime.fractionPart);
	return secondsDifference + fractionalDifference;
}
double calculateOffset(struct ntpTime T1, struct ntpTime T2, struct ntpTime T3, struct ntpTime T4) {
	// return 1/2 * [(T2-T1) + (T3 - T4)]
	return 0.5 * (timeDifference(T1, T2) + timeDifference(T4, T3));
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
	// return (T4 - T1) - (T3 - T2)
	return timeDifference(T1, T4) - timeDifference(T2, T3);
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
		org = getCurrentTime(startTimeInSeconds, startTime);
	}
	// TODO: Fill out other values
	//    Figure out if polling interval in the packet is 4 minutes or something else. If it is 4 minutes, use value from pollingInterval global variable.
	//    Figure out how to calculate precision (probably uses CLOCKS_PER_SECOND from time.h)
	struct ntpTime xmtTime = getCurrentTime(startTimeInSeconds, startTime);
	xmtTimes[responsePos] = xmtTime;
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
	recvTimes[responsePos] = getCurrentTime(startTimeInSeconds, startTime);
	// TODO: parse byte buffer into an ntpPacket object (ret).
	return ret;
}

// In case responses arrive out of order, we need to sort by sequential time. Maybe it would be by server's org, rcv, or xmt.
// I'm not sure how it would handle out-of-order receipt by the server.
void sortResponses(struct ntpPacket responses[8]) {
	// TODO: Implement this function
	// Match our transmit time with the server's origin time
	for(int i = 0; i < 8; ++i) {
		struct ntpTime xmtTime = xmtTimes[i];
		for(int j = i; j < 8; j++) {
			if(ntpTimeEquals(responses[j].originTimestamp, xmtTime)) {
				// swap locations i and j
				struct ntpPacket tmp = responses[i];
				responses[i] = responses[j];
				responses[j] = tmp;
			}
		}
	}
}

void testTimeEquals() {
	struct ntpTime t1, t2, t3, t4;
	t1.intPart = t2.intPart = t3.intPart = 200;
	t4.intPart = 300;
	// t1 and t2 should match. t1 and t3 differ by fractionPart. t1 and t4 differ by intPart
	t1.fractionPart = t2.fractionPart = t4.fractionPart = 50;
	t3.fractionPart = 100;
	printf("T1 and T2 agree (should be true): %d\n", ntpTimeEquals(t1, t2));
	printf("T1 and T3 agree (should be false): %d\n", ntpTimeEquals(t1, t3));
	printf("T1 and T4 agree (should be false): %d\n", ntpTimeEquals(t1, t4));
}

int main(int argc, char** argv) {
	// Times are based on time.h https://www.tutorialspoint.com/c_standard_library/time_h.htm
	clock_t programLength = 3600 * CLOCKS_PER_SEC; // number of seconds to run the program (should be 1 hour for the real thing)
	clock_t timeBetweenBursts = pollingInterval;
	clock_t startOfBurst;
	clock_t curTime; // time passed since the start time

	// Default server name
	// char defaultServerName[] = "localhost";
	char defaultServerName[] = "132.163.96.5";
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
	startTimeInSeconds = time(NULL); // system time, epoch 1970
	startTime = clock(); // processor time measured in CLOCKS_PER_SEC
	
	// USE SMALL VALUES OF TIME FOR TESTING!
	programLength = 60 * CLOCKS_PER_SEC; // number of seconds to run the program (should be 1 hour for the real thing)
	timeBetweenBursts = 5 * CLOCKS_PER_SEC; // show the timer every 5 seconds
	time_t timerAccuracyChecker;
	struct ntpTime startTimeAsNtp = getCurrentTime(startTimeInSeconds, startTime);

	// REMOVE THIS LATER
	testTimeEquals();

	// loop until program should end
	while (curTime < programLength) {
		responsePos = 0; // start reading things into the start of the length 8 arrays
		curTime = clock() - startTime; // time since the start of the program
		startOfBurst = curTime;
		
		// Right now, just test timing accuracy after each "burst"
		timerAccuracyChecker = time(NULL) - startTimeInSeconds; // how much time has passed in seconds; ground truth
		printf("Expected time difference: %ld seconds\n", timerAccuracyChecker);
		struct ntpTime currentTimeAsNtp = getCurrentTime(startTimeInSeconds, startTime);
		double timeDiffTest = timeDifference(startTimeAsNtp, currentTimeAsNtp);
		printf("Actual time difference: %f seconds\n\n", timeDiffTest);

		// Do a burst (send all msgs without waiting for a response)
		for(responsePos = 0; responsePos < 8; ++responsePos) {
			sendMsg();
		}
		responsePos = 0; // start reading at the start of the array again
		// Listen for responses
		while (curTime - startOfBurst < timeBetweenBursts && responsePos < 8) {
			int bytesToRead = packetSize;
			if (!ioctl(sockfd, FIONREAD, &bytesToRead) && bytesToRead == packetSize && responsePos < 8) {
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
