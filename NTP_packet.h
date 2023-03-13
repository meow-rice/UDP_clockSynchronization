#ifndef mergeAndQuickSort_h
#define mergeAndQuickSort_h

#include <stdint.h>


struct server_stratum1;
struct server;
struct client;


struct NTP_packet{

private:
  struct header{
    uint8_t LI_VN_mode;
    uint8_t statum;
    uint8_t poll;
    uint8_t precision;
  };

public:

  header h;
  bool isServer;

  int32_t rootDelay;
  int32_t rootDispersion;
  referenceIdentifier





};



#endif
