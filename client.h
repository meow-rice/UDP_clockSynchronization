#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <errno.h>
// Network stuff
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

// maximum number of delays or offsets to calculate
#define MAX_NUM_MEASUREMENTS 100
// from NTP client example https://lettier.github.io/posts/2016-04-26-lets-make-a-ntp-client-in-c.html
#define NTP_TIMESTAMP_DELTA 2208988800ull
// required to use #define instead of const for sizes of arrays at global level for some reason
#define NumMessages 8
const int packetSize = 48;
time_t pollInterval = 4 * 60; // 4 minutes between bursts; use this variable in the ntp packet poll variable
const signed char pollGlobal = 16; // poll every 16 seconds
// const signed char clockPrecision = floor(-log10(CLOCKS_PER_SEC) / log10(2.0)) + 1; // upper bound of log2(clock precision), to pass into NTP and to synchronize start time to the beginning of a second on the system clock
const signed char clockPrecision = floor(-9 / log10(2.0)) + 1; // upper limit of nanosecond precision in base 2
const double nanosecondCoefficient = pow(10, -9); // for use with representing timespecs in decimal form


struct ntpTime {
	uint32_t intPart;
	uint32_t fractionPart;
};

struct ntpPacket {
	uint8_t LI_VN_mode;
	char stratum;
	//these need to be 8-bit signed
	signed char poll;
	signed char precision;
	//these sizes are right but they're 32 bit float (custom type) in implementation
	int rootDelay; // ignore
	int rootDispersion; // ignore
	int referenceIdentifier; // ignore

	//these are chillin
	struct ntpTime referenceTimestamp;
	struct ntpTime originTimestamp;
	struct ntpTime receiveTimestamp;
	struct ntpTime transmitTimestamp;
};

void error(char* msg); // print error messages
void connectToServer(const char* hostName, short port);
void synchronizeStartClock(time_t* startTimeInSeconds, struct timespec* startTime);
// Return the current system time as an NTP time
struct ntpTime getCurrentTime(time_t baselineTime, clock_t precisionUnitsSinceBaseline);
// Calculate the difference in seconds between two NTP times.
int ntpTimeEquals(struct ntpTime t1, struct ntpTime t2); // return true if the times are equivalent
double timeDifference(struct ntpTime fisrtTime, struct ntpTime secondTime);
double calculateOffset(struct ntpTime T1, struct ntpTime T2, struct ntpTime T3, struct ntpTime T4);
double minOffset(double offsets[8]);
double calculateRoundtripDelay(struct ntpTime T1, struct ntpTime T2, struct ntpTime T3, struct ntpTime T4);
double minDelay(double delays[8]);
void sendMsg(int sockfd, struct ntpTime xmtTimes[], int sendPos, char stratum, struct ntpTime org, struct ntpTime lastRecvTime, struct tmiespec startTime, bool isServer, struct sockaddr_in* client, int* clientAddressSize);
// the parts of recvMsg past "int isServer" are what allow the server to respond to the client
struct ntpPacket recvMsg(int sockfd, struct ntpTime recvTimes[], int responsePos, struct ntpTime* org, struct ntpTime* lastRecvTime, struct timespec startTime, bool isServer, struct sockaddr_in* client, int* clientAddressSize);
void sortResponses(struct ntpPacket responses[8]);


// The better way to align the start of the program clock with the start of a second in system time
void synchronizeStartClock(time_t* startTimeInSeconds, struct timespec* startTime) {
	time_t startClockSynchronizer;
	time(&startClockSynchronizer);
	time_t tmpTimeT;
	struct timespec tmpClock;
	// loop until you reach the start of the next second
	do {
		time(&tmpTimeT); // record the time again after the wait
		timespec_get(&tmpClock, TIME_UTC); // unhandled exception
	} while (tmpTimeT == startClockSynchronizer);
	*startTimeInSeconds = tmpTimeT;
	*startTime = tmpClock;
}

// Return the current system time as an NTP time. I'm not sure how precise this method is.
// Constraint: Algorithm only works on a Little Endian system.
// Precondition: baselineTime and baselineInApplicationClock are measured at the same time prior to calling this function.
struct ntpTime getCurrentTime(struct timespec baselineTime) {
	struct timespec currentTime;
	int timeBase;
	if((timeBase = timespec_get(&currentTime, TIME_UTC)) < 0) {
		perror("Error reading current time\n");
		exit(1);
	}
	// printf("getCurrentTime() timespec_get() value is %f %s (%d)\n", (double) currentTime.tv_sec + nanosecondCoefficient * currentTime.tv_nsec, timeBase == TIME_UTC? "UTC" : "UNKNOWN TIMEBASE", timeBase);
	
	time_t integerSecondsSinceBaseline = currentTime.tv_sec - baselineTime.tv_sec;
	// attempting to extract fraction part and int part without losing precision https://stackoverflow.com/questions/5589383/extract-fractional-part-of-double-efficiently-in-c
	long diffInNanoseconds = currentTime.tv_nsec - baselineTime.tv_nsec;
	// If the difference in nanoseconds is negative, then reduce the intPart by 1 second and increase the fractionPart by 1 second
	if(diffInNanoseconds < 0) {
		integerSecondsSinceBaseline -= 1;
		diffInNanoseconds += 1000000000;
	}
	double diffInFractionalSeconds = nanosecondCoefficient * diffInNanoseconds;
	time_t currentTimeSince1900 = baselineTime.tv_sec + integerSecondsSinceBaseline + NTP_TIMESTAMP_DELTA;
	
	struct ntpTime ret;
	ret.intPart = (unsigned int) currentTimeSince1900; // must fit in 32 bits
	ret.fractionPart = (unsigned int) (diffInFractionalSeconds * pow(2,32)); // fractional part represented as a sequence of 32 bits, assuming little endian
	return ret;
}

// Send the ntp packet.
void sendMsg(int sockfd, struct ntpTime xmtTimes[], int sendPos, char stratum, struct ntpTime org, struct ntpTime lastRecvTime, struct timespec startTime, bool isServer, struct sockaddr_in* client, int* clientAddressSize) {
	char li = 0;
	char vn = 4;
	char mode = 3; // client

	//polling rate is set to 16 seconds
	//precision is clockPrecision from client.h, which is log2 of the time.h clock() CLOCKS_PER_SEC
	//reference timestamp is not used in this assignment. just passing org it doesn't matter

	struct ntpTime xmtTime = getCurrentTime(startTime);
	xmtTimes[sendPos] = xmtTime;
	// printf("Transmit time is %u.%f\n", xmtTime.intPart, pow(2, -32) * (double) xmtTime.fractionPart);
	// li_vn_mode = li (2 bits), vn (3 bits), mode (3 bits)
	uint8_t li_vn_mode = mode | (vn << 3) | (li << 6); // becomes 35 instead of 27 because we use version 4
	struct ntpPacket packet = {li_vn_mode, stratum, pollGlobal, clockPrecision,  0,0,0, org, org, lastRecvTime, xmtTime};

	char buffer[packetSize];
	memset(buffer, 0, packetSize);
	//packet only lives for the length of this call so there's no point in allocating
	//extra memory for network byte order.

	//bytes lack endianess..
	memcpy(buffer+0, &packet.LI_VN_mode, 1);
	memcpy(buffer+1, &packet.stratum, 1);
	memcpy(buffer+2, &packet.poll, 1);
	memcpy(buffer+3, &packet.precision, 1);

	//these go into network byte order.
	/*reference:
	uint32_t htonl(uint32_t hostlong);
	uint16_t htons(uint16_t hostshort);
	uint32_t ntohl(uint32_t netlong);
	uint16_t ntohs(uint16_t netshort);
	*/
	/*
	ignore, i'm just writing to tally offset (these types would need to be changed anyway)
	memcpy(buffer+4, packet.rootDelay, 4);
	memcpy(buffer+8, packet.rootDispersion, 4);
	memcpy(buffer+12, packet.rootDelay, 4);
	*/

	packet.referenceTimestamp.intPart = htonl(packet.referenceTimestamp.intPart);
	packet.referenceTimestamp.fractionPart = htonl(packet.referenceTimestamp.fractionPart);

	packet.originTimestamp.intPart = htonl(packet.originTimestamp.intPart);
	packet.originTimestamp.fractionPart = htonl(packet.originTimestamp.fractionPart);

	packet.receiveTimestamp.intPart = htonl(packet.receiveTimestamp.intPart);
	packet.receiveTimestamp.fractionPart = htonl(packet.receiveTimestamp.fractionPart);

	packet.transmitTimestamp.intPart = htonl(packet.transmitTimestamp.intPart);
	packet.transmitTimestamp.fractionPart = htonl(packet.transmitTimestamp.fractionPart);

	memcpy(buffer+16, &packet.referenceTimestamp.intPart, 4);
	memcpy(buffer+20, &packet.referenceTimestamp.fractionPart, 4);

	memcpy(buffer+24, &packet.originTimestamp.intPart, 4);
	memcpy(buffer+28, &packet.originTimestamp.fractionPart, 4);

	memcpy(buffer+32, &packet.receiveTimestamp.intPart, 4);
	memcpy(buffer+36, &packet.receiveTimestamp.fractionPart, 4);

	memcpy(buffer+40, &packet.transmitTimestamp.intPart, 4);
	memcpy(buffer+44, &packet.transmitTimestamp.fractionPart, 4);

	if(isServer) {
		// https://pubs.opengroup.org/onlinepubs/009604499/functions/sendto.html
		if (sendto(sockfd, buffer, packetSize, 0, (struct sockaddr*)client, (socklen_t)*clientAddressSize) < 0) {
			perror("Error sending packet using sendto\n");
			exit(1);
		}
	}
	else {
		// send over socket https://www.geeksforgeeks.org/socket-programming-cc/
		send(sockfd, buffer, packetSize, 0);
	}
}

struct ntpPacket recvMsg(int sockfd, struct ntpTime recvTimes[], int responsePos, struct ntpTime* org, struct ntpTime* lastRecvTime, struct timespec startTime, bool isServer, struct sockaddr_in* client, int* clientAddressSize) {
	char buffer[packetSize];
	struct ntpPacket ret;
	// clear memory in case it holds garbage values
	memset(buffer, 0, packetSize);
	memset((void*)&ret, 0, packetSize); // zero out the values in the packet object
	// Read the socket into a byte buffer.
	if (isServer) {
		// hopefully this grabs the whole packet at once
		// https://pubs.opengroup.org/onlinepubs/007904875/functions/recvfrom.html
		if ( recvfrom(sockfd, buffer, packetSize, 0, (struct sockaddr*)client, (socklen_t*)clientAddressSize) < 0 ) {
			perror("Error reading socket while receiving message\n");
		}
	}
	else {
		read(sockfd, buffer, packetSize);
	}

	// set the client's receive time
	recvTimes[responsePos] = getCurrentTime(startTime);
	*lastRecvTime = recvTimes[responsePos];


	// copy from buffer into ret
	memcpy(&ret.LI_VN_mode, buffer+0, 1);
	memcpy(&ret.stratum, buffer+1, 1);
	memcpy(&ret.poll, buffer+2, 1);
	memcpy(&ret.precision, buffer+3, 1);

	//skip to timestamps for this project.
	memcpy(&ret.referenceTimestamp.intPart, buffer+16, 4);
	memcpy(&ret.referenceTimestamp.fractionPart, buffer+20, 4);

	memcpy(&ret.originTimestamp.intPart, buffer+24, 4);
	memcpy(&ret.originTimestamp.fractionPart, buffer+28, 4);

	memcpy(&ret.receiveTimestamp.intPart, buffer+32, 4);
	memcpy(&ret.receiveTimestamp.fractionPart, buffer+36, 4);

	memcpy(&ret.transmitTimestamp.intPart, buffer+40, 4);
	memcpy(&ret.transmitTimestamp.fractionPart, buffer+44, 4);

	//change to host order
	ret.referenceTimestamp.intPart = ntohl(ret.referenceTimestamp.intPart);
	ret.referenceTimestamp.fractionPart = ntohl(ret.referenceTimestamp.fractionPart);

	ret.originTimestamp.intPart = ntohl(ret.originTimestamp.intPart);
	ret.originTimestamp.fractionPart = ntohl(ret.originTimestamp.fractionPart);

	ret.receiveTimestamp.intPart = ntohl(ret.receiveTimestamp.intPart);
	ret.receiveTimestamp.fractionPart = ntohl(ret.receiveTimestamp.fractionPart);

	ret.transmitTimestamp.intPart = ntohl(ret.transmitTimestamp.intPart);
	ret.transmitTimestamp.fractionPart = ntohl(ret.transmitTimestamp.fractionPart);

	// Set org equal to the server's send time (pull it out of the packet)
	*org = ret.transmitTimestamp;
	return ret;
}
