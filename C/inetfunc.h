#ifndef __BVBS_ZSH_INETFUNC__
#define __BVBS_ZSH_INETFUNC__

#include <stdbool.h> // for bool
#include <stdint.h> // for uint32_t, uint8_t

typedef struct {
	char* ipv4;
	uint32_t metric;
	bool isDefault;
	uint8_t IPV4cidr;
	char* device;
	char* linkspeed;
	char* routedNet; //not used if isDefault is true
} NetDevice;

typedef struct NetList_t NetList;
struct NetList_t {
	NetDevice dev;
	NetList* next;
	NetList* prev;
};

typedef struct {
	char* BasicIPInfo;
	char* AdditionalIPInfo;
	char* RouteInfo;
} IpTransportStruct;

IpTransportStruct GetBaseIPString();
#endif
