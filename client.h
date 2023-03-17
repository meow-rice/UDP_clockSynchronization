#include <time.h>

// maximum number of delays or offsets to calculate
#define MAX_NUM_MEASUREMENTS 100
// from NTP client example https://lettier.github.io/posts/2016-04-26-lets-make-a-ntp-client-in-c.html
#define NTP_TIMESTAMP_DELTA 2208988800ull

struct ntpTime {
	unsigned int intPart;
	unsigned int fractionPart;
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
struct ntpTime getCurrentTime(time_t baselineTime, clock_t precisionUnitsSinceBaseline);
// Calculate the difference in seconds between two NTP times.
int ntpTimeEquals(struct ntpTime t1, struct ntpTime t2); // return true if the times are equivalent
double timeDifference(struct ntpTime fisrtTime, struct ntpTime secondTime);
double calculateOffset(struct ntpTime T1, struct ntpTime T2, struct ntpTime T3, struct ntpTime T4);
double minOffset(double offsets[8]);
double calculateRoundtripDelay(struct ntpTime T1, struct ntpTime T2, struct ntpTime T3, struct ntpTime T4);
double minDelay(double delays[8]);
void sendMsg();
struct ntpPacket recvMsg();
void sortResponses(struct ntpPacket responses[8]);
