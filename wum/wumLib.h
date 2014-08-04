
#include <stdio.h>
#include <string.h>
#define QUIT_MSG 0
#define LIST_MSG 1
#define CONF_UPDATE_MSG 2

#define MSG_ELEMENT_TYPE_VENDOR_WUM 3
#define MSG_ELEMENT_TYPE_VENDOR_XML 4

#define WTP_STATE_REQUEST     11
#define WTP_CONFIG_REQUEST     1
#define WTP_CONFIG_RESPONSE    2

#define WTP_UPDATE_REQUEST      3
#define WTP_UPDATE_RESPONSE     4

#define WTP_CUP_FRAGMENT        5
#define WTP_CUP_ACK             6

#define WTP_COMMIT_UPDATE       7
#define WTP_COMMIT_ACK          8

#define WTP_CANCEL_UPDATE_REQUEST       9
#define WTP_CANCEL_UPDATE_RESPONSE       10

#define SUCCESS	0
#define ERROR	1

#define FRAGMENT_SIZE 4000

//xml 
#define XML_PATH "./apconfig.xml"

struct WTPInfo {
	int wtpId;
	char *name;
};

struct version_info {
	char major;
	char minor;
	char revision;
	int size;
};

struct config_info {
	int resultCode;
	char *xml;
};

int ACServerConnect(char *address, int port);
void ACServerDisconnect(int acserver);
struct WTPInfo *ACServerWTPList(int acserver, int *nWTPs);	
void freeWTPList(struct WTPInfo *wtpList, int nWTPs);

int WUMGetWTPVersion(int acserver, int wtpId, struct version_info *);
int WUMReadCupVersion(char *cup_pathname, struct version_info *update_v);
int WUMUpdate(int acserver, int wtpId, void *cup_buf, struct version_info update_v);
int WUMSendCancelRequest(int acserver, int wtpId);

int WUMConfigWTPByXML(int acserver, int wtpId, struct config_info *c_info, char wum_type);

