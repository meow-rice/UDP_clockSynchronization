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

//needed for file write (switching to C++)
#include <iostream>
#include <fstream>
#include <climits>

using namespace std;

// globals
char* serverName = NULL;
short serverPort = 123; // default server port
struct sockaddr_in serv_addr; // Server address data structure.
struct hostent* server;      // Server data structure.
int sockfd; // socket connection
char globalStratum = 0; // start at 0 (unspecified)
// required to use #define instead of const for sizes of arrays at global level for some reason
#define NumMessages 8
double delays[NumMessages];
double offsets[NumMessages];
struct ntpTime org; // originate timestamp the client will send
struct ntpTime xmtTimes[NumMessages]; // local time we sent each request
struct ntpTime recvTimes[NumMessages]; // local time each response was received
struct ntpTime lastRecvTime;
struct ntpPacket responses[NumMessages];
int responsePos = 0; // index in responses we will read to
clock_t pollingInterval = 4 * 60 * CLOCKS_PER_SEC; // 4 minutes between bursts
clock_t startTime; // time on the clock when you started the program (we initialize this in main after the socket connects)
time_t startTimeInSeconds; // system time when you started the program (measured at the same time as startTime)
const int packetSize = 48;

signed char pollGlobal = 16; //do not need to implement poll frequency algorithm.

void error(char* msg) {
	perror(msg); // print to stderr
	exit(0);
}

void connectToServerUnix(const char* hostName, short port) {
	// socket code copied from https://lettier.github.io/posts/2016-04-26-lets-make-a-ntp-client-in-c.html
	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // Create a UDP socket.
	if (sockfd < 0){
		//reformat for g++ compiler
		//char errText[] = {"ERROR opening socket"};
		error("Error opening socket.");
		std::cout<<"Error opening socket\n";

	}


	server = gethostbyname(hostName); // Convert URL to IP.

	if (server == NULL){
		error("ERROR, no such host");
		std::cout<<"Error, no such host\n";

	}


	// on changing bzero and bcopy https://stackoverflow.com/questions/18330673/bzero-bcopy-versus-memset-memcpy
	// Zero out the server address structure.
	memset((char*)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	// Copy the server's IP address to the server address structure.
	memmove((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr, server->h_length);
	// Convert the port number integer to network big-endian style and save it to the server address structure.
	serv_addr.sin_port = htons(port);
	if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
		error("ERROR connecting");
		std::cout<<"Error Connecting.\n";
	}else{
		std::cout<<"Connected!\n";
	}


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
	// I use long because it's simpler than worrying about negative numbers that come from unsigned ints
	long secondsDifference = (long) secondTime.intPart - (long) firstTime.intPart; // allow for negative time differences
	double fractionalDifference = pow(2,-32) * ((long) secondTime.fractionPart - (long) firstTime.fractionPart);
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
	char li = 0;
	char n = 4;
	char mode = 3; // client
	char stratum = globalStratum;

	//polling rate is set to 16 seconds
	//precision is set by received messages only
	//reference timestamp is not used in this assignment. just passing org it doesn't matter

	struct ntpTime xmtTime = getCurrentTime(startTimeInSeconds, startTime);
	xmtTimes[responsePos] = xmtTime;
	// li_vn_mode =
	uint8_t li_vn_mode = mode | (n << 3); // becomes 35 instead of 27 because we use version 4
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

struct ntpPacket recvMsg() {
	char buffer[packetSize];
	struct ntpPacket ret;
	// clear memory in case it holds garbage values
	memset(buffer, 0, packetSize);
	memset((void*)&ret, 0, packetSize); // zero out the values in the packet object
	// Read the socket into a byte buffer.
	read(sockfd, buffer, packetSize);

	// set the client's receive time
	recvTimes[responsePos] = getCurrentTime(startTimeInSeconds, startTime);
	lastRecvTime = recvTimes[responsePos];


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
	org = ret.receiveTimestamp;
	return ret;
}

// In case responses arrive out of order or dropped packets, we need to sort by sequential time.
void sortResponses(struct ntpPacket responses[8]) {
	// Match our transmit time with the server's origin time
	for(int i = 0; i < 8; ++i) {
		struct ntpTime xmtTime = xmtTimes[i];
		int found = -1; // index of the response with origin time matching client transmit time
		for(int j = 0; j < 8; ++j) {
			if(ntpTimeEquals(responses[j].originTimestamp, xmtTime)) {
				found = j;
			}
		}
		if (found >= 0) {
			// swap locations i and j
			struct ntpPacket tmp = responses[i];
			responses[i] = responses[found];
			responses[found] = tmp;
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

	ofstream graphFile;
	ofstream measurementFile;
	//TODO: the inputs to the main need to get parsed to take a strings as input.
	//TODO: write/parse csv file for rawMeasurementData
	string graphData = "graphData.csv";
	string rawMeasurementData = "rawMeasurementData.csv";
	graphFile.open(graphData);
	measurementFile.open(rawMeasurementData);

	//state format
	graphFile<<"Format: Number of successful Messages (say n<=8), Burst #, offset_1, delay_1, ... , offset_n, delay_n, offset for minimum delay, minimum delay\n";
	measurementFile<<" Burst #, T_1^1, T_2^1, T_3^1, T_4^1, ... , T_1^n, T_2^n, T_3^n, T_4^n, (where n<=8 is the number of successful messages) \n";
	// Times are based on time.h https://www.tutorialspoint.com/c_standard_library/time_h.htm
	clock_t programLength = 60 * CLOCKS_PER_SEC; // number of seconds to run the program (should be 1 hour for the real thing)
	// clock_t timeBetweenBursts = pollingInterval;
	// JUST FOR TESTING, reduce time between bursts to 10 seconds
	clock_t timeBetweenBursts = 10 * CLOCKS_PER_SEC;
	clock_t startOfBurst;
	clock_t curTime; // time passed since the start time
	printf("Server set to burst every %ld seconds for %ld minutes\n", timeBetweenBursts / CLOCKS_PER_SEC, programLength / 60 / CLOCKS_PER_SEC);
	printf("Press CTRL + C to stop the server\n");

	size_t burstNumber = 0;
	size_t numMessages;

	// Default server name
	// char defaultServerName[] = "localhost";
	char defaultServerName[] = "132.163.96.1";
	// TODO: have tmp point to a command line argument if we have a command line argument for server name
	char* tmp = defaultServerName;
	int hostnameLen = strlen(tmp) + 1; // add 1 to make room for null terminator
	serverName = (char*) malloc(hostnameLen);
	strncpy(serverName, tmp, hostnameLen); // set the server name

	// TODO: Possibly provide server port by command line args and modify the global serverPort

	// initialize org
	org.intPart = 0;
	org.fractionPart = 0;

	// zero out the responses and recvTimes
	memset((char*)&recvTimes, 0, NumMessages*sizeof(struct ntpTime));
	memset((char*)&lastRecvTime, 0, sizeof(struct ntpTime));
	memset((char*)&responses, 0, NumMessages*packetSize);

	// connect to the server
	printf("Connecting to server %s at port %d\n", serverName, serverPort);
	connectToServer(serverName, serverPort);


	// start the clock
	startTimeInSeconds = time(NULL); // system time, epoch 1970
	startTime = clock(); // processor time measured in CLOCKS_PER_SEC
	curTime = clock() - startTime; // time since the start of the program

	// loop until program should end
	while (curTime < programLength) {
		burstNumber++;
		numMessages = 0;
		responsePos = 0; // start reading things into the start of the length 8 arrays
		curTime = clock() - startTime; // time since the start of the program
		startOfBurst = curTime;
		printf("Starting burst %ld\n", burstNumber);

		// zero out the delays and offsets arrays
		memset((char*)&delays, 0, 8 * sizeof(double));
		memset((char*)&offsets, 0, 8 * sizeof(double));

		// Do a burst (send all msgs without waiting for a response)
		for(responsePos = 0; responsePos < 8; ++responsePos) {
			sendMsg();
		}
		responsePos = 0; // start reading at the start of the array again
		// Listen for responses
		while (curTime - startOfBurst < timeBetweenBursts && responsePos < 8) {
			int bytesToRead = packetSize;
			// read a response if one exists
			if (!ioctl(sockfd, FIONREAD, &bytesToRead) && bytesToRead == packetSize && responsePos < 8) {
				responses[responsePos] = recvMsg();
				globalStratum = responses[responsePos].stratum;
				++responsePos;
			}
			curTime = clock() - startTime;
		}
		// Burst is finished.
		responsePos = 8; // make sure we cycle through all responses, even ones that were lost.
		sortResponses(responses);
		// calculate offsets and delays
		measurementFile<<burstNumber<<',';
		for (int i = 0; i < responsePos; i++) {
			struct ntpPacket packet = responses[i];
			struct ntpTime T1 = packet.originTimestamp;
			// change delay and offset for missing responses to be super high
			if(T1.intPart == 0) {
				offsets[i] = UINT_MAX;
				delays[i] = UINT_MAX;
			}
			else {
				struct ntpTime T2 = packet.receiveTimestamp;
				struct ntpTime T3 = packet.transmitTimestamp;
				struct ntpTime T4 = recvTimes[i];
				offsets[i] = calculateOffset(T1, T2, T3, T4);
				delays[i] = calculateRoundtripDelay(T1, T2, T3, T4);

				numMessages+=1;
				//write to measurementFile
				//recall format measurementFile<<"Format: Burst #, T_1^1, T_2^1, T_3^1, T_4^1, ... , T_1^n, T_2^n, T_3^n, T_4^n,\n";

				measurementFile<<((double)T1.intPart + pow(2,-32) * T1.fractionPart)<<',';
				measurementFile<<((double)T2.intPart + pow(2,-32) * T2.fractionPart)<<',';
				measurementFile<<((double)T3.intPart + pow(2,-32) * T3.fractionPart)<<',';
				measurementFile<<((double)T4.intPart + pow(2,-32) * T4.fractionPart);
			}
			if(i==responsePos-1){measurementFile<<'\n';}
			else{
				measurementFile<<',';
			}
			printf("Packet %d has delay %f, offset %f\n", i, delays[i], offsets[i]);
		}

		//recall format:
		/*
		graphFile<<"Format: Number of Sucessful Messages (say n), Burst #, offset_1, delay_1, ... , offset_n, delay_n, offset for minimum delay, minimum delay\n";
		//note number of sucessful meassages n(mod 4) can be deduced from the format.
		*/

		graphFile<<numMessages<<','<<burstNumber<<',';
		double _minDelay = UINT_MAX, offsetForMinDelay = UINT_MAX;
		for(size_t i = 1; i<NumMessages; i++){
			if(offsets[i] < UINT_MAX){
				std::cout<<offsets[i]<<','<<delays[i]<<',';
				graphFile<<offsets[i]<<','<<delays[i]<<',';
				//computationally more efficient to find min here.
				if(delays[i]<_minDelay){
					offsetForMinDelay = offsets[i];
					_minDelay = delays[i];
				}
			}
		}
		//write min
		graphFile<<offsetForMinDelay<<','<<_minDelay<<'\n';

		// wait the rest of the 4-minute delay
		while (curTime - startOfBurst < timeBetweenBursts) {
			curTime = clock() - startTime;
		}
		printf("\n");
	}

	free(serverName); // free the memory
	close(sockfd);
	graphFile.close();
	measurementFile.close();
	printf("Program completed.\n");
	return 0;
}
