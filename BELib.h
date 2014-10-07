
#ifndef BELIB_H
#define BELIB_H

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

char BESetApValues(char* apMac, int socketIndex, CWVendorXMLValues* xmlValues);
char* AssembleBEheader(char* buf,int *len,int apId);
void SendBEResponse(char* buf,int len,int apId);
//int BEServerConnect(char *address, int port);
void SendBERequest(char* buf,int len);

int CWXMLSetValues(int selection, int socketIndex, CWVendorXMLValues* xmlValues) ;

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


#endif /* BELIB_H */