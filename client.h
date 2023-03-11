// maximum number of delays or offsets to calculate
#define MAX_NUM_MEASUREMENTS 100

struct ntpTime {
	int intPart;
	int fractionPart;
};

struct ntpPacket {
	char li;
	char vn;
	char mode;
	char stratum;
	char poll;
	char precision;
	int rootDelay; // ignore
	int rootDispersion; // ignore
	int referenceIdentifier; // ignore
	struct ntpTime referenceTimestamp;
	struct ntpTime originTimestamp;
	struct ntpTime receiveTimestamp;
	struct ntpTime transitTimestamp;
};

void error(char* msg); // print error messages
void connectToServer(const char* hostName, short port);
// Return the current system time as an NTP time
struct ntpTime getCurrentTime();
// Calculate the difference in seconds between two NTP times.
double timeDifference(struct ntpTime fisrtTime, struct ntpTime secondTime);
double calculateOffset(struct ntpTime T1, struct ntpTime T2, struct ntpTime T3, struct ntpTime T4);
double minOffset(double offsets[8]);
double calculateRoundtripDelay(struct ntpTime T1, struct ntpTime T2, struct ntpTime T3, struct ntpTime T4);
double minDelay(double delays[8]);
void sendMsg();
struct ntpPacket recvMsg();
void sortResponses(struct ntpPacket responses[8]);
