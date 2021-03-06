
#ifndef BECOMMON_H
#define BECOMMON_H

#include "CWCommon.h"

typedef unsigned char u_char;

#define TRUE    1 
#define FALSE   0 

//LEN
#define   MAC_ADDR_LEN    6
#define   MAX_VER_LEN     16
#define	TIME_LEN            4


//TYPE
#define	BE_CAPWAP_HEADER     101
#define	BE_CONNECT_EVENT		1
#define	BE_CONNECT_EVENT_RSP		2
#define	BE_MONITOR_EVENT_REQUSET 3
#define	BE_MONITOR_EVENT_RESPONSE 4
#define	BE_CONFIG_EVENT_REQUSET  5
#define	BE_CONFIG_EVENT_RESPONSE  6
#define	BE_UPGRADE_EVENT_REQUEST  7
#define	BE_UPGRADE_EVENT_RESPONSE 8

#define	BE_PORTAL_EVENT_REQUEST  11
#define	BE_PORTAL_EVENT_RESPONSE 12
#define	BE_WTP_EVENT_REQUEST  21
#define	BE_WTP_EVENT_RESPONSE 22
#define	BE_SYSTEM_EVENT_REQUEST  31
#define	BE_SYSTEM_EVENT_RESPONSE 32


//result code,4BYTE
typedef enum {
	CW_SUCCESS				= 0, //	Success
	CW_FAILURE_AC_LIST			= 1, // AC List message MUST be present
	CW_SUCCESS_NAT				= 2, // NAT detected
	CW_FAILURE				= 3, // unspecified
	CW_FAILURE_RES_DEPLETION		= 4, // Resource Depletion
	CW_FAILURE_UNKNOWN_SRC			= 5, // Unknown Source
	CW_FAILURE_INCORRECT_DATA		= 6, // Incorrect Data
	CW_FAILURE_ID_IN_USE			= 7, // Session ID Alreadyin Use
	CW_FAILURE_WTP_HW_UNSUPP		= 8, // WTP Hardware not supported
	CW_FAILURE_BINDING_UNSUPP		= 9, // Binding not supported
	CW_FAILURE_UNABLE_TO_RESET		= 10, // Unable to reset
	CW_FAILURE_FIRM_WRT_ERROR		= 11, // Firmware write error
	CW_FAILURE_SERVICE_PROVIDED_ANYHOW	= 12, // Unable to apply requested configuration 
	CW_FAILURE_SERVICE_NOT_PROVIDED	= 13, // Unable to apply requested configuration
	CW_FAILURE_INVALID_CHECKSUM		= 14, // Image Data Error: invalid checksum
	CW_FAILURE_INVALID_DATA_LEN		= 15, // Image Data Error: invalid data length
	CW_FAILURE_OTHER_ERROR			= 16, // Image Data Error: other error
	CW_FAILURE_IMAGE_ALREADY_PRESENT	= 17, // Image Data Error: image already present
	CW_FAILURE_INVALID_STATE		= 18, // Message unexpected: invalid in current state
	CW_FAILURE_UNRECOGNIZED_REQ		= 19, // Message unexpected: unrecognized request
	CW_FAILURE_MISSING_MSG_ELEM		= 20, // Failure: missing mandatory message element
	CW_FAILURE_UNRECOGNIZED_MSG_ELEM	= 21,  // Failure: unrecognized message element
	CW_FAILURE_WTP_NOT_CONNECTED	= 22 ,// Failure:WTP not online
	CW_FAILURE_WTP_IMAGE_PATH_ERROR	= 23 ,//Image Data Error:WTP image path error
	CW_FAILURE_WTP_UPGRADING_REJECT_NWEUPGRADE	= 24 //WTP upgrading ,reject new upgrade
} CWResultCode;


//system code,1 BYTE
typedef enum {
	SYSTEM_RESET			= 1, //	��λ
	SYSTEM_REBOOT			= 2 // ����

} SystemCode;

//regisit mac
typedef enum {
	active			= 0, // ע��
	disactive			= 1 // δע��
} ActiveCode;

#define BE_ACTCODE_LEN       1
//without type, length
#define BE_TYPE_LEN        2
#define BE_LENGTH_LEN    4
#define BE_TYPELEN_LEN   (BE_TYPE_LEN + BE_LENGTH_LEN)
#define BE_CODE_LEN (sizeof(CWResultCode))


#define BE_HEADER_MIN_LEN	(MAC_ADDR_LEN) + (TIME_LEN)
//#define BE_HEADER_MAX_LEN	(MAC_ADDR_LEN+2*2+2*MAX_VER_LEN+TIME_LEN)
//struct 4 size
#pragma pack(1)
typedef struct {
	unsigned short type;
	unsigned int length;
	unsigned int timestamp; 
	unsigned char apMac[MAC_ADDR_LEN];
	
//may	
	//unsigned short hwVerLen;
	//char  hwVer[MAX_VER_LEN];
	//unsigned short swVerLen;
	//char  swVer[MAX_VER_LEN];
	//char  timestamp[TIME_LEN];
}BEHeader;
#pragma pack()

#define BE_CONNECT	1
#define BE_DISCONNECT 2
#define BE_CONNECT_EVENT_LEN 1
#define BE_CONNECT_EVENT_CONNECT 1
#define BE_CONNECT_EVENT_DISCONNECT 2

//ap req
#pragma pack(1)
typedef struct {
	unsigned short type;
	unsigned int length;
	char  state;
	char* xml;
}BEconnectEvent;
#pragma pack()
//alarm ?
#pragma pack(1)
typedef struct {
	unsigned short type;
	unsigned int length;
}BEmonitorEventRequest;
#pragma pack()

#pragma pack(1)
typedef struct {
	unsigned short type;
	unsigned int  length;
	CWResultCode resultCode;
	char*  xml;
}BEmonitorEventResponse;
#pragma pack()

#pragma pack(1)
typedef struct {
	unsigned short type;
	unsigned int  length;
	char* xml;
}BEconfigEventRequest;
#pragma pack()

#pragma pack(1)
typedef struct {
	unsigned short type;
	unsigned int length;
	CWResultCode resultCode;
}BEconfigEventResponse;
#pragma pack()

/*
//update need fragment
typedef struct {
	unsigned short type;
	unsigned short length;
	unsigned short hwVerLen;
	char  hwVer[MAX_VER_LEN];
	unsigned short swVerLen;
	char  swVer[MAX_VER_LEN];
 	unsigned int  ImageLen;
	char* image;//fragment
}BEupgradeEventRequest;
*/

#pragma pack(1)
typedef struct {
	unsigned short type;
	unsigned int length;
	char* filePath;//fragment
}BEupgradeEventRequest;
#pragma pack()

#pragma pack(1)
typedef struct {
	unsigned short type;
	unsigned int length;
	CWResultCode resultCode;
}BEupgradeEventResponse;
#pragma pack()


//ap req
#pragma pack(1)
typedef struct {
	unsigned short type;
	unsigned int length;
	char* xml;
}BEwtpEventRequest;
#pragma pack()

#pragma pack(1)
typedef struct {
	unsigned short type;
	unsigned int length;
	CWResultCode resultCode;
}BEwtpEventResponse;
#pragma pack()

//4bit
#pragma pack(1)
typedef struct {
	unsigned short type;
	unsigned int length;
	unsigned short TotalFileNum;
	unsigned short FileNo;
	unsigned short EncodeNameLen;
	unsigned int    EncodeContentLen;
	char* EncodeName;
	char* EncodeContent;
}BEPortalEventRequest;
#pragma pack()

#pragma pack(1)
typedef struct {
	unsigned short type;
	unsigned int length;
	unsigned short TotalFileNum;
	unsigned short FileNo;
	CWResultCode resultCode;
}BEPortalEventResponse;
#pragma pack()

#pragma pack(1)
typedef struct {
	unsigned short type;
	unsigned int length;
	unsigned char operation;
	//1:Reset 2:Reboot
}BEsystemEventRequest;
#pragma pack()

#pragma pack(1)
typedef struct {
	unsigned short type;
	unsigned int length;
	CWResultCode resultCode;
}BEsystemEventResponse;
#pragma pack()



#endif /* BECOMMON_H */
