/**
 * CSCI 5673 Distributed Systems
 * Programming Assignment 3 -- NTP
 * Due Tues, 3/21/23
 * John Salame
 * Maurice Alexander
 */

 // NOTE: This client currently does not do retransmissions of dropped packets; it simply does best-effort.

#include "client.h"
#include <stdlib.h>
#include <sys/ioctl.h>

//needed for file write (switching to C++)
#include <iostream>
#include <fstream>
#include <climits>

using namespace std;

// globals
int sockfd; // socket connection
char globalStratum = 0; // start at 0 (unspecified)
double delays[NumMessages];
double offsets[NumMessages];
struct ntpTime org; // originate timestamp the client will send
struct ntpTime xmtTimes[NumMessages]; // local time we sent each request
struct ntpTime recvTimes[NumMessages]; // local time each response was received
struct ntpTime lastRecvTime;
struct ntpPacket responses[NumMessages];
int responsePos = 0; // index in responses we will read to

void error(char* msg) {
	perror(msg); // print to stderr
	exit(0);
}

void connectToServerUnix(const char* hostName, short port) {
	struct sockaddr_in serv_addr; // Server address data structure.
	struct hostent* server;      // Server data structure.
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
double minOffset(double offsets[NumMessages]) {
	double min = offsets[0];
	for (int i = 1; i < NumMessages; ++i) {
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
double minDelay(double delays[NumMessages]) {
	double min = delays[0];
	for (int i = 1; i < NumMessages; ++i) {
		if (delays[i] < min) {
			min = delays[i];
		}
	}
	return min;
}

// In case responses arrive out of order or dropped packets, we need to sort by sequential time.
// Return the index of the first element that does not have a match
int sortResponses(struct ntpPacket responses[NumMessages], struct ntpTime xmtTimes[NumMessages], struct ntpTime recvTimes[NumMessages]) {
	int unsolved = 0; // index of the start of the unsorted portion
	// Match our transmit time with the server's origin time
	for(int i = 0; i < NumMessages; ++i) {
		struct ntpTime xmtTime = xmtTimes[i];
		// do not consider empty transmit times as a match and do not try to move them to the solved array
		if(xmtTime.intPart == 0) {
			continue;
		}
		int found = -1; // index of the response with origin time matching client transmit time
		for(int j = unsolved; j < NumMessages; ++j) {
			if(ntpTimeEquals(responses[j].originTimestamp, xmtTime)) {
				found = j;
				break;
			}
		}
		// every time we find a match, move it to the "solved" part of the array and shrink the unsolved portion
		if (found >= 0) {
			if(found != i) {
				// swap locations i and j
				struct ntpPacket tmpResponse = responses[unsolved];
				responses[unsolved] = responses[found];
				responses[found] = tmpResponse;
				// swap the corresponding receive times as well, so we know when we received the response
				struct ntpTime tmpTime = recvTimes[unsolved];
				recvTimes[unsolved] = recvTimes[found];
				recvTimes[found] = tmpTime;
				// bring the transmit time where it belongs at the end of the solved array
				tmpTime = xmtTimes[unsolved];
				xmtTimes[unsolved] = xmtTimes[i];
				xmtTimes[i] = tmpTime;
			}
			++unsolved;
		}
	}
	return unsolved; // index of the stuff we should override
}

// a helper function we're no longer using
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
	// Common variables and default configuration
	char* serverName = NULL;
	short serverPort = 123; // default server port
	char defaultServerName[] = "132.163.96.1";
	char localhost[] = "localhost";
	char* tmp = defaultServerName; // tmp will point to a different string if we use non-default options for serverName
	ofstream graphFile;
	ofstream measurementFile;
	//TODO: the inputs to the main need to get parsed to take a strings as input.
	string graphData = "graphData.csv";
	string rawMeasurementData = "rawMeasurementData.csv";
	graphFile.open(graphData);
	measurementFile.open(rawMeasurementData);
	time_t programLength = 3600; // number of seconds to run the program (should be 1 hour for the real thing)
	time_t timeBetweenBursts = pollInterval; // value may come from client.h
	time_t timeBetweenPackets = 8; // According to Slack, NIST server may deny service if the packets are sent less than 4 seconds apart. Use this for retransmission as well.
	struct timespec startTime; // time on the clock when you started the program (we initialize this in main after the socket connects)
	time_t startTimeInSeconds; // system time when you started the program (measured at the same time as startTime)


	// QUALITY OF LIFE MESSAGES AND CONFIGURATION

	//state format
	graphFile<<"Format: Number of successful Messages (say n<=8), Burst #, offset_1, delay_1, ... , offset_n, delay_n, offset for minimum delay, minimum delay\n";
	measurementFile<<" Burst #, T_1^1, T_2^1, T_3^1, T_4^1, ... , T_1^n, T_2^n, T_3^n, T_4^n, (where n<=8 is the number of successful messages) \n";

	// Command line option 2 is local server
	printf("Checking for special command line options\n");
	if(argc > 1 && strlen(argv[1]) == 1 && argv[1][0] == '2') {
		tmp = localhost;
		serverPort = 8100;
	}

	// check if the user passed in a server name
	int serverNameIndex = 0;
	char serverNamePrompt[] = "--server";
	const int serverArgBufSize = 512;
	char serverArg[serverArgBufSize];
	for(int i = 0; i < argc; ++i) {
		if(strcmp(argv[i], serverNamePrompt) == 0 && argc > i + 1) {
			// read the next argument
			serverNameIndex = i+1;
			// copy server IP or hostname from command line into serverArg buffer while avoiding the possibility of buffer overlow
			strncpy(serverArg, argv[serverNameIndex], serverArgBufSize);
			tmp = serverArg;
		}
	}

	// Prepare server name
	int hostnameLen = strlen(tmp) + 1; // add 1 to make room for null terminator
	serverName = (char*) malloc(hostnameLen);
	strncpy(serverName, tmp, hostnameLen); // set the server name

	// If a command line option is "test", then use short program length and intervals
	for(int i = 1; i < argc; ++i) {
		if(strcmp(argv[i], "test") == 0) {
			if(i == serverNameIndex) {
				perror("Bad command line arguments; 'test' collides with expected position of server name");
				exit(1);
			}
			// JUST FOR TESTING, reduce program length to one minute and time between bursts to 10 seconds
			programLength = 60;
			timeBetweenBursts = 10;
			timeBetweenPackets = 1; // no delay, just burst as fast as possible until all packets are recovered
			// END OF CHANGES FOR TESTING
		}
	}

	// TODO: Possibly provide server port by command line args and modify the global serverPort

	printf("Server set to burst every %ld seconds with %ld seconds between consecutive packets for %ld minutes\n", timeBetweenBursts, timeBetweenPackets, programLength / 60);
	printf("Press CTRL + C to stop the server\n");

	// PREPARE DATA
	// initialize org
	org.intPart = 0;
	org.fractionPart = 0;
	// zero out the responses, xmtTimes, and recvTimes
	memset((char*)&xmtTimes, 0, NumMessages*sizeof(struct ntpTime));
	memset((char*)&recvTimes, 0, NumMessages*sizeof(struct ntpTime));
	memset((char*)&lastRecvTime, 0, sizeof(struct ntpTime));
	memset((char*)&responses, 0, NumMessages*packetSize);



	// PREPARE TO START THE PROGRAM

	// connect to the server (connect doesn't really mean anythng since this is UDP, but it does set up sockets and server addresses)
	printf("Connecting to server %s at port %d\n", serverName, serverPort);
	connectToServer(serverName, serverPort);

	// start the clock, as close as possible to the start of a new second
	synchronizeStartClock(&startTimeInSeconds, &startTime);
	printf("Finished synchronizing the start clock\n");


	// BEGIN PROGRAM BEHAVIOR

	size_t burstNumber = 0;
	size_t numMessages;
	time_t startOfBurst;
	time_t timeOfPrevPacket; // when did you send the last packet?
	time_t curTime; // current system time
	time(&curTime);
	int sendPos;

	// loop until program should end
	while (curTime - startTimeInSeconds < programLength) {
		burstNumber++;
		numMessages = 0;
		responsePos = 0; // start reading things into the start of the length <NumMessages> arrays
		time(&curTime);
		startOfBurst = curTime;
		timeOfPrevPacket = 0;
		sendPos = 0; // governs data related to sent packets, such as transmit times
		printf("Starting burst %ld\n", burstNumber);

		// zero out the delays, offsets, and response arrays
		memset((char*)&delays, 0, NumMessages * sizeof(double));
		memset((char*)&offsets, 0, NumMessages * sizeof(double));
		memset((char*)&responses, 0, NumMessages*packetSize);

		// Listen for responses. Exit the loop once the timer runs out or both the sendPos and responsePos max out
		while (curTime - startOfBurst < timeBetweenBursts && (sendPos < NumMessages || responsePos < NumMessages)) {

			// send a packet if the timeBetweenPackets has passed.
			if(curTime - timeOfPrevPacket >= timeBetweenPackets) {
				sendMsg(sockfd, xmtTimes, sendPos, globalStratum, org, lastRecvTime, startTime, false, NULL, NULL);
				printf("Sent msg with sendPos %d, transmit time %f\n", sendPos, xmtTimes[sendPos].intPart + pow(2, -32) * xmtTimes[sendPos].fractionPart);
				++sendPos;
				time(&timeOfPrevPacket);
			}

			// If we have sent 8 packets, then compact all the results and set the sendPos and responsePos to the index after all the correct message pairs
			if(sendPos >= NumMessages) {
				sendPos = responsePos = sortResponses(responses, xmtTimes, recvTimes);
			}
			
			// read a response if one exists
			int bytesToRead = packetSize;
			if (!ioctl(sockfd, FIONREAD, &bytesToRead) && bytesToRead == packetSize && responsePos < NumMessages) {
				responses[responsePos] = recvMsg(sockfd, recvTimes, responsePos, &org, &lastRecvTime, startTime, false, NULL, NULL);
				globalStratum = responses[responsePos].stratum;
				printf("Received msg with responsePos %d, origin time %f\n", responsePos, responses[responsePos].originTimestamp.intPart + pow(2, -32)*responses[responsePos].originTimestamp.fractionPart);
				++responsePos;
			}

			// If we have received 8 packets, then compact all the results and set the sendPos and responsePos to the index after all the correct message pairs
			if(responsePos >= NumMessages) {
				sendPos = responsePos = sortResponses(responses, xmtTimes, recvTimes);
			}

			time(&curTime);
		}
		// Burst is finished.
		int junkIndex = sortResponses(responses, xmtTimes, recvTimes);
		// 0 out the data for lost responses
		memset((char*)&responses[junkIndex], 0, (NumMessages - junkIndex) * sizeof(double));
		memset((char*)&xmtTimes[junkIndex], 0, (NumMessages - junkIndex) * sizeof(double));
		memset((char*)&recvTimes[junkIndex], 0, (NumMessages - junkIndex) * sizeof(double));
		
		// calculate offsets and delays
		measurementFile<<burstNumber<<',';
		responsePos = NumMessages; // make sure we cycle through all responses, even ones that were lost. We could probably just go up to junkIndex if I wrote my sort function correctly.
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
		printf("\n\n");
		//write min
		graphFile<<offsetForMinDelay<<','<<_minDelay<<'\n';

		// wait the rest of the 4-minute delay
		while (curTime - startOfBurst < timeBetweenBursts) {
			time(&curTime);
		}
	}

	free(serverName); // free the memory
	close(sockfd);
	graphFile.close();
	measurementFile.close();
	printf("Program completed.\n");
	return 0;
}
