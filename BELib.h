
#ifndef BELIB_H
#define BELIB_H

#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include "CWCommon.h"
#include "CWVendorPayloads.h"
#include "BECommon.h"
#include "WUM.h"

#define BE_SOCKET_ENABLE 1
#define BE_SOCKET_DISABLE 0
#define BE_SOCKET_INDEX 0

#define SA const struct sockaddr
//#define BE_SERVER_ADDRESS "10.131.160.37"
#define BE_SERVER_ADDRESS "192.168.8.123"
#define BE_SERVER_PORT	8888

#define BE_MAX_PACKET_LEN 80000

int FindApIndex(u_char* apMac);
char BESetWumValues(u_char* apMac, int socketIndex, CWProtocolVendorSpecificValues* vendorValues);
char BESetApValues(u_char* apMac, int socketIndex, CWVendorXMLValues* Values);
char BESetSysValues(u_char* apMac, int socketIndex, SystemCode sysCode);
char BESetPortalValues(u_char* apMac, int socketIndex, CWVendorPortalValues* portalValues);

char* AssembleBEheader(char* buf,int *len,int apId,char *xml);
void SendBEResponseDirectly(int type,u_char *apMac,int socketIndex,CWResultCode resultCode);
void SendBEResponse(char* buf,int len,int apId);
//int BEServerConnect(char *address, int port);
void SendBERequest(char* buf,int len);

int CWXMLSetValues(int selection, int socketIndex, CWVendorXMLValues* xmlValues) ;
int CWPortalSetValues(int selection, int socketIndex, CWVendorPortalValues* portalValues);
int CWSysSetValues(int selection, int socketIndex,SystemCode sysCode );

#define Swap16(s) ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))
#define Swap32(l) (((l) >> 24) |  (((l)&0x00ff0000) >> 8)  |  (((l) & 0x0000ff00) << 8)  | ((l) << 24))
#define Swap64(ll) (((ll) >> 56) |\
					(((ll) & 0x00ff000000000000) >> 40) |\
					(((ll) & 0x0000ff0000000000) >> 24) |\
					(((ll) & 0x000000ff00000000) >> 8)	|\
					(((ll) & 0x00000000ff000000) << 8)	|\
					(((ll) & 0x0000000000ff0000) << 24) |\
					(((ll) & 0x000000000000ff00) << 40) |\
					(((ll) << 56)))

#define FRAGMENT_SIZE 4000

#define MIN(a,b) (a < b) ? (a) : (b)

struct version_info {
	char major;
	char minor;
	char revision;
	int size;
};

CWResultCode CheckUpgradeVersion(u_char* apMac, int socketIndex, char *cup_path);
char UpgradeVersion(u_char* apMac, int socketIndex,void *cup, struct version_info update_v);
int CWWumSetValues(int selection, int socketIndex, CWProtocolVendorSpecificValues* vendorValues);

#define UPGRADE_FAILED     2
#define UPGRADE_SUCCESS  3

#endif /* BELIB_H */
