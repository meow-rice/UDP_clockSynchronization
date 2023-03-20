#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
// Network stuff
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

// maximum number of delays or offsets to calculate
#define MAX_NUM_MEASUREMENTS 100
// from NTP client example https://lettier.github.io/posts/2016-04-26-lets-make-a-ntp-client-in-c.html
#define NTP_TIMESTAMP_DELTA 2208988800ull
const int packetSize = 48;
const signed char pollGlobal = 16; //do not need to implement poll frequency algorithm.

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
// Return the current system time as an NTP time
struct ntpTime getCurrentTime(time_t baselineTime, clock_t precisionUnitsSinceBaseline);
// Calculate the difference in seconds between two NTP times.
int ntpTimeEquals(struct ntpTime t1, struct ntpTime t2); // return true if the times are equivalent
double timeDifference(struct ntpTime fisrtTime, struct ntpTime secondTime);
double calculateOffset(struct ntpTime T1, struct ntpTime T2, struct ntpTime T3, struct ntpTime T4);
double minOffset(double offsets[8]);
double calculateRoundtripDelay(struct ntpTime T1, struct ntpTime T2, struct ntpTime T3, struct ntpTime T4);
double minDelay(double delays[8]);
void sendMsg(int sockfd, struct ntpTime xmtTimes[], int sendPos, char stratum, struct ntpTime org, struct ntpTime lastRecvTime, time_t startTimeInSeconds, clock_t startTime);
struct ntpPacket recvMsg(int sockfd, struct ntpTime recvTimes[], int responsePos, struct ntpTime* org, struct ntpTime* lastRecvTime, time_t startTimeInSeconds, clock_t startTime);
void sortResponses(struct ntpPacket responses[8]);

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

// Send the ntp packet.
void sendMsg(int sockfd, struct ntpTime xmtTimes[], int sendPos, char stratum, struct ntpTime org, struct ntpTime lastRecvTime, time_t startTimeInSeconds, clock_t startTime) {
	char li = 0;
	char vn = 4;
	char mode = 3; // client

	//polling rate is set to 16 seconds
	//precision is set by received messages only
	//reference timestamp is not used in this assignment. just passing org it doesn't matter

	struct ntpTime xmtTime = getCurrentTime(startTimeInSeconds, startTime);
	xmtTimes[sendPos] = xmtTime;
	// li_vn_mode = li (2 bits), vn (3 bits), mode (3 bits)
	uint8_t li_vn_mode = mode | (vn << 3) | (li << 6); // becomes 35 instead of 27 because we use version 4
	struct ntpPacket packet = {li_vn_mode, stratum, pollGlobal, 0,  0,0,0, org, org, lastRecvTime, xmtTime};

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

	// send over socket https://www.geeksforgeeks.org/socket-programming-cc/
	send(sockfd, buffer, packetSize, 0);
}

struct ntpPacket recvMsg(int sockfd, struct ntpTime recvTimes[], int responsePos, struct ntpTime* org, struct ntpTime* lastRecvTime, time_t startTimeInSeconds, clock_t startTime) {
	char buffer[packetSize];
	struct ntpPacket ret;
	// clear memory in case it holds garbage values
	memset(buffer, 0, packetSize);
	memset((void*)&ret, 0, packetSize); // zero out the values in the packet object
	// Read the socket into a byte buffer.
	read(sockfd, buffer, packetSize);

	// set the client's receive time
	recvTimes[responsePos] = getCurrentTime(startTimeInSeconds, startTime);
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

	// Set org equal to the server's receive time (pull it out of the packet)
	*org = ret.receiveTimestamp;
	return ret;
}
