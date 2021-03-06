
#include "CWAC.h"
#include "ACAppsProtocol.h"
#include "CWVendorPayloads.h"
#include "BECommon.h"
#include "BELib.h"
#include <time.h>

#define LISTEN_PORT 8888
//#define LISTEN_PORT 5246
#define COMMAND_BUFFER_SIZE 5120
#define WTP_LIST_BUFFER_SIZE 1024

int is_valid_wtp_index(int index);
int Readn(int sock, void *buf, size_t n);

/********************************************************************
 * Now the only parameter need by the application thread manager	*
 * is the index of socket.											*
 ********************************************************************/

typedef struct {
	int index;
} CWInterfaceThreadArg;


int FindApIndex(u_char* apMac)
{
	int numActiveWTPs =0,k = 0,i,j,finded = -1;

	if(apMac == NULL)
	{
		CWLog("FindApIndex apMac == NULL");
		return FALSE;
	}

	if(!CWErr(CWThreadMutexLock(&gActiveWTPsMutex))) {
		CWLog("Error locking the gActiveWTPsMutex mutex");
		return finded;
	}
	numActiveWTPs = gActiveWTPs;
	CWLog("FindApIndex gActiveWTPs = %d",gActiveWTPs);
	CWThreadMutexUnlock(&gActiveWTPsMutex);
	
	k = numActiveWTPs;
	if(!k)
	{
		CWLog("numActiveWTPs = 0,no connect ap !");
		return finded;
	}
	
	for(i=0; i<CW_MAX_WTP && k ; i++) 
	{
		if(gWTPs[i].currentState == CW_ENTER_RUN)  
		{
			k--;
			for (j = 0; j < MAC_ADDR_LEN; j++) 
			{
				if (apMac[j] == gWTPs[i].MAC[j]) 
				{
					if (j == (MAC_ADDR_LEN - 1))
					{	
						finded = i;
						CWLog("[F:%s, L:%d] finded = %d,thread:%x",__FILE__,__LINE__,finded,(unsigned int)gWTPs[i].thread);
						//if(!CWXMLSetValues(i, socketIndex, xmlValues))
							//return FALSE;
						break;
					}
				}
				else
				{
					break;

				}
			}

			if(finded >=0)
			{
				break;
			}
		}
	}

	return finded;

}

char BESetWumValues(u_char* apMac, int socketIndex, CWProtocolVendorSpecificValues* vendorValues)
{
	int finded = -1,interfaceResult = 0;

	if(apMac == NULL)
	{
		CWLog("BESetWumValues apMac == NULL");
		return FALSE;
	}

	if(vendorValues == NULL)
	{
		CWLog("BESetWumValues portalValues == NULL");
		return FALSE;
	}
	finded = FindApIndex(apMac);

	//no this ap online
	if(finded <0)
	{
		CWLog("[F:%s, L:%d] can't find this ap:%x:%x:%x:%x:%x:%x",__FILE__,__LINE__,apMac[0],
			apMac[1],apMac[2],apMac[3],apMac[4],apMac[5]);
		return FALSE;
	}
	
	if(finded >=0)
	{
		
		if(!CWWumSetValues(finded, socketIndex, vendorValues))
			return FALSE;


		interfaceResult = gWTPs[finded].interfaceResult;

		if(interfaceResult == UPGRADE_FAILED)
		{
			CWLog("[F:%s, L:%d] interfaceResult= UPGRADE_FAILED",__FILE__,__LINE__);
			return FALSE;
		}
		
		if(interfaceResult == CW_FAILURE_WTP_UPGRADING_REJECT_NWEUPGRADE)
		{
			CWLog("[F:%s, L:%d] interfaceResult= CW_FAILURE_WTP_UPGRADING_REJECT_NWEUPGRADE",__FILE__,__LINE__);
			return CW_FAILURE_WTP_UPGRADING_REJECT_NWEUPGRADE;
		}

		if(interfaceResult == CW_FALSE)
		{
			CWLog("[F:%s, L:%d] interfaceResult= 0 , Fail !",__FILE__,__LINE__);
			return FALSE;
		}
	}
	return TRUE;
}


char BESetApValues(u_char* apMac, int socketIndex, CWVendorXMLValues* xmlValues)
{
	int finded = -1;

	if(apMac == NULL)
	{
		CWLog("BESetApValues apMac == NULL");
		return FALSE;
	}

	if(xmlValues == NULL)
	{
		CWLog("BESetApValues portalValues == NULL");
		return FALSE;
	}
	
	finded = FindApIndex(apMac);

//no this ap online
	if(finded <0)
	{
		CWLog("[F:%s, L:%d] can't find this ap:%x:%x:%x:%x:%x:%x",__FILE__,__LINE__,apMac[0],
			apMac[1],apMac[2],apMac[3],apMac[4],apMac[5]);
		//
			return FALSE;
	}
	if(finded >=0)
	{
		if(!CWXMLSetValues(finded, socketIndex, xmlValues))
			return FALSE;
	}
	
	if(gWTPs[finded].interfaceResult == CW_FALSE)
	{
		CWLog("[F:%s, L:%d] interfaceResult= 0 , Fail !",__FILE__,__LINE__);
		return FALSE;
	}
	return TRUE;
}


char BESetPortalValues(u_char* apMac, int socketIndex, CWVendorPortalValues* portalValues)
{
	int finded = -1;

	if(apMac == NULL)
	{
		CWLog("BESetPortalValues apMac == NULL");
		return FALSE;
	}

	if(portalValues == NULL)
	{
		CWLog("BESetPortalValues portalValues == NULL");
		return FALSE;
	}
	
	finded = FindApIndex(apMac);

//no this ap online
	if(finded <0)
	{
		CWLog("[F:%s, L:%d] can't find this ap:%x:%x:%x:%x:%x:%x",__FILE__,__LINE__,apMac[0],
			apMac[1],apMac[2],apMac[3],apMac[4],apMac[5]);
		//
			return FALSE;
	}
	if(finded >=0)
	{
		if(!CWPortalSetValues(finded, socketIndex, portalValues))
			return FALSE;
	}
	if(gWTPs[finded].interfaceResult == CW_FALSE)
	{
		CWLog("[F:%s, L:%d] interfaceResult= 0 , Fail !",__FILE__,__LINE__);
		return FALSE;
	}
	return TRUE;
}

char BESetSysValues(u_char* apMac, int socketIndex, SystemCode sysCode)
{
	int finded = -1;

	if(apMac == NULL)
	{
		CWLog("BESetSysValues apMac == NULL");
		return FALSE;
	}
	
	finded = FindApIndex(apMac);

//no this ap online
	if(finded <0)
	{
		CWLog("[F:%s, L:%d] can't find this ap:%x:%x:%x:%x:%x:%x",__FILE__,__LINE__,apMac[0],
			apMac[1],apMac[2],apMac[3],apMac[4],apMac[5]);
		//
			return FALSE;
	}
	
	if(finded >=0)
	{
		if(!CWSysSetValues(finded, socketIndex, sysCode))
			return FALSE;
	}
	
	if(gWTPs[finded].interfaceResult == CW_FALSE)
	{
		CWLog("[F:%s, L:%d] interfaceResult= 0 , Fail !",__FILE__,__LINE__);
		return FALSE;
	}
	return TRUE;
}


char BESetActValues(u_char* apMac, int socketIndex, ActiveCode actCode)
{
	int finded = -1;

	if(apMac == NULL)
	{
		CWLog("BESetSysValues apMac == NULL");
		return FALSE;
	}
	
	finded = FindApIndex(apMac);

//no this ap online
	if(finded <0)
	{
		CWLog("[F:%s, L:%d] can't find this ap:%x:%x:%x:%x:%x:%x",__FILE__,__LINE__,apMac[0],
			apMac[1],apMac[2],apMac[3],apMac[4],apMac[5]);
		//
			return FALSE;
	}
	
	if(finded >=0)
	{
		if(!CWActSetValues(finded, socketIndex, actCode))
			return FALSE;
	}
	
	if(gWTPs[finded].interfaceResult == CW_FALSE)
	{
		CWLog("[F:%s, L:%d] interfaceResult= 0 , Fail !",__FILE__,__LINE__);
		return FALSE;
	}
	return TRUE;
}


char* AssembleBEheader(char* buf,int *len,int apId,char *xml)
{
	BEHeader beHeader;
	char *rsp = NULL,*rspStart = NULL;
	int i,packetLen = 0;
	time_t timestamp;
	unsigned short type = 0;
	unsigned int length = 0;

	
	if(buf == NULL || len == NULL  )
	{
		CWLog("AssembleBEheader buf == NULL || len == NULL ");
		return NULL;
	}

	CWLog("[F:%s, L:%d] :buf len = %d",__FILE__,__LINE__,*len);
	
	memcpy((char*)&type, buf, BE_TYPE_LEN);
	memcpy((char*)&length,buf+BE_TYPE_LEN,BE_LENGTH_LEN);
	
	
	CWLog("[F:%s, L:%d] :AssembleBEheader type = %d",__FILE__,__LINE__,type);
	if(*len > BE_MAX_PACKET_LEN)
	{
		CWLog("AssembleBEheader Error len > 80000");
		return NULL;
	}
	
	beHeader.length =*len  + TIME_LEN + MAC_ADDR_LEN;
	packetLen = BE_TYPELEN_LEN + beHeader.length; 


	beHeader.type = htons(BE_CAPWAP_HEADER);
	CWLog("[F:%s, L:%d] :beHeader.length = %d",__FILE__,__LINE__,beHeader.length);
	beHeader.length =Swap32(beHeader.length);
	
	time(&timestamp);
	CWLog("[F:%s, L:%d] :beHeader.timestamp = %d",__FILE__,__LINE__,timestamp);
	beHeader.timestamp =Swap32(timestamp);
	
	if((gWTPs[apId].currentState == CW_ENTER_RUN))
	{
		for(i=0; i<MAC_ADDR_LEN; i++)
		{
			beHeader.apMac[i] =  gWTPs[apId].MAC[i];
		}
	}
	
	CWLog("[F:%s, L:%d] :beHeader.mac = %x:%x:%x:%x:%x:%x",__FILE__,__LINE__,beHeader.apMac[0],beHeader.apMac[1],beHeader.apMac[2],beHeader.apMac[3],beHeader.apMac[4],beHeader.apMac[5]);

	CW_CREATE_STRING_ERR(rsp, packetLen+1, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});				
	memset(rsp, 0, packetLen+1);
	//�ֽڶ���
	//memcpy(rsp,(char*)&beHeader, BE_TYPELEN_LEN+TIME_LEN+MAC_ADDR_LEN);
	rspStart = rsp;

	memcpy(rsp,(char*)&(beHeader.type), BE_TYPE_LEN);
	rsp = rsp + BE_TYPE_LEN;
	memcpy(rsp,(char*)&(beHeader.length), BE_LENGTH_LEN);
	rsp = rsp + BE_LENGTH_LEN;
	memcpy(rsp,(char*)&(beHeader.timestamp), TIME_LEN);
	rsp = rsp + TIME_LEN;
	memcpy(rsp,(char*)&(beHeader.apMac), MAC_ADDR_LEN);
	rsp = rsp + MAC_ADDR_LEN;	

	//*xml
	if(xml == NULL)
	{	
		memcpy(rsp,(char*)&(type), BE_TYPE_LEN);
		rsp = rsp + BE_TYPE_LEN;
		memcpy(rsp,(char*)&(length), BE_LENGTH_LEN);
		rsp = rsp + BE_LENGTH_LEN;
		memcpy(rsp,buf+BE_TYPELEN_LEN, *len-BE_TYPELEN_LEN);
	}
	if( xml && type ==( htons( BE_CONNECT_EVENT) ))
	{
		CWLog("[F:%s, L:%d] :AssembleBEheader  htons( BE_CONNECT_EVENT)= %d",__FILE__,__LINE__, htons( BE_CONNECT_EVENT));
		memcpy(rsp,(char*)&(type), BE_TYPE_LEN);
		rsp = rsp + BE_TYPE_LEN;
		memcpy(rsp,(char*)&(length), BE_LENGTH_LEN);
		rsp = rsp + BE_LENGTH_LEN;
		memcpy(rsp,buf+BE_TYPELEN_LEN, BE_CONNECT_EVENT_LEN);
		rsp = rsp + BE_CONNECT_EVENT_LEN;
		memcpy(rsp,xml, *len - BE_TYPELEN_LEN -BE_CONNECT_EVENT_LEN );
	}
	if( xml && type ==( htons( BE_MONITOR_EVENT_RESPONSE) ))
	{
		CWLog("[F:%s, L:%d] :AssembleBEheader  htons( BE_MONITOR_EVENT_RESPONSE)= %d",__FILE__,__LINE__, htons( BE_MONITOR_EVENT_RESPONSE));
		memcpy(rsp,(char*)&(type), BE_TYPE_LEN);
		rsp = rsp + BE_TYPE_LEN;
		memcpy(rsp,(char*)&(length), BE_LENGTH_LEN);
		rsp = rsp + BE_LENGTH_LEN;
		memcpy(rsp,buf+BE_TYPELEN_LEN, BE_CODE_LEN);
		rsp = rsp + sizeof(CWResultCode);
		memcpy(rsp,xml, *len - BE_TYPELEN_LEN -BE_CODE_LEN );
	}
	if( xml && type == (htons( BE_WTP_EVENT_REQUEST) ))
	{
		CWLog("[F:%s, L:%d] :AssembleBEheader  htons( BE_WTP_EVENT_REQUEST)= %d",__FILE__,__LINE__, htons( BE_WTP_EVENT_REQUEST));
		memcpy(rsp,(char*)&(type), BE_TYPE_LEN);
		rsp = rsp + BE_TYPE_LEN;
		memcpy(rsp,(char*)&(length), BE_LENGTH_LEN);
		rsp = rsp + BE_LENGTH_LEN;
		memcpy(rsp,xml, *len - BE_TYPELEN_LEN);
	}
	
	
	*len = packetLen;
	
	CWLog("[F:%s, L:%d] :packetLen = %d",__FILE__,__LINE__,*len);
	
	return rspStart;
}


void SendBEResponseDirectly(int type,u_char *apMac,int socketIndex,CWResultCode resultCode)
{
	int length,BESize = 0,n = 0;
	int ret;
	BEHeader beHeader;
	char *rsp = NULL,*rspStart = NULL;
	int i,packetLen = 0;
	time_t timestamp;

	if(apMac == NULL)
	{
		CWLog("SendBEResponseDirectly apMac == NULL");
		return;
	}
	
	type =htons( type) ;
	length = Swap32(sizeof(CWResultCode));
	ret = Swap32(resultCode);
		
	BESize = BE_TYPELEN_LEN+sizeof(CWResultCode);

	beHeader.length =BESize  + TIME_LEN + MAC_ADDR_LEN;
	packetLen = BE_TYPELEN_LEN + beHeader.length; 
	
	time(&timestamp);
	CWLog("[F:%s, L:%d] :beHeader.timestamp = %d",__FILE__,__LINE__,timestamp);
	
	beHeader.timestamp =Swap32(timestamp);
	beHeader.type = htons(BE_CAPWAP_HEADER);
	CWLog("[F:%s, L:%d] :beHeader.length = %d",__FILE__,__LINE__,beHeader.length);
	beHeader.length =Swap32(beHeader.length);

	
	for(i=0; i<MAC_ADDR_LEN; i++)
	{
		beHeader.apMac[i] = apMac[i];
	}
	
	CWLog("[F:%s, L:%d] :beHeader.mac = %x:%x:%x:%x:%x:%x",__FILE__,__LINE__,beHeader.apMac[0],beHeader.apMac[1],beHeader.apMac[2],beHeader.apMac[3],beHeader.apMac[4],beHeader.apMac[5]);
	CW_CREATE_STRING_ERR(rsp, packetLen+1, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return;});				
	memset(rsp, 0, packetLen+1);
	rspStart = rsp;
	//�ֽڶ���
	
	memcpy(rsp,(char*)&(beHeader.type), BE_TYPE_LEN);
	rsp = rsp + BE_TYPE_LEN;
	memcpy(rsp,(char*)&(beHeader.length), BE_LENGTH_LEN);
	rsp = rsp + BE_LENGTH_LEN;
	memcpy(rsp,(char*)&(beHeader.timestamp), TIME_LEN);
	rsp = rsp + TIME_LEN;
	memcpy(rsp,(char*)&(beHeader.apMac), MAC_ADDR_LEN);
	rsp = rsp + MAC_ADDR_LEN;	
	
	memcpy(rsp,&type, BE_TYPE_LEN);
	rsp = rsp + BE_TYPE_LEN;	
	memcpy(rsp,&length, BE_LENGTH_LEN);
	rsp = rsp + BE_LENGTH_LEN;
	memcpy(rsp,(char*)&ret, BE_CODE_LEN);
	
	if(!CWErr(CWThreadMutexLock(&appsManager.socketMutex[socketIndex]))) {
		CWLog("Error locking numSocketFree Mutex");
		return;
	}

	while(n != packetLen)
	{
		if ( (n += Writen(appsManager.appSocket[socketIndex], rspStart, packetLen))  < 0 ) {
			//CWThreadMutexUnlock(&appsManager.socketMutex[socketIndex]);
			CWLog("Error write appsManager.appSocket[socketIndex]");
			break;
		}
		CWLog("[F:%s, L:%d] Writen n:%d",__FILE__,__LINE__,n);
	}

	CWThreadMutexUnlock(&appsManager.socketMutex[socketIndex]);

	CW_FREE_OBJECT(rspStart);

}
void SendBEResponse(char* buf,int len,int apId)
{
	int n,socketIndex;
	n = 0;
	
	if(buf == NULL)
	{
		CWLog("SendBEResponse buf == NULL");
		return;
	}
	CWLog("[F:%s, L:%d] SendBEResponse len:%d",__FILE__,__LINE__,len);
	if(len > BE_MAX_PACKET_LEN)
	{
		CWLog("SendBEResponse Error len > 80000");
		return;
	}
	
	socketIndex = gWTPs[apId].applicationIndex;
	
	if(!CWErr(CWThreadMutexLock(&appsManager.socketMutex[socketIndex]))) {
		CWLog("Error locking numSocketFree Mutex");
		return;
	}

	while(n != len)
	{
		if ( (n += Writen(appsManager.appSocket[socketIndex], buf, len))  < 0 ) {
			//CWThreadMutexUnlock(&appsManager.socketMutex[socketIndex]);
			CWLog("Error write appsManager.appSocket[socketIndex]");
			break;
		}
		CWLog("[F:%s, L:%d] Writen n:%d",__FILE__,__LINE__,n);
	}
	CWThreadMutexUnlock(&appsManager.socketMutex[socketIndex]);

}

//Raydez say close every time
//add Resp
void SendBERequest(char* buf,int len)
{
	int ret,n,sock,optValue = 1;

	int *iPtr;
	
	n = 0;
	ret = 0;

	if(buf == NULL)
	{
		CWLog("SendBERequest buf == NULL");
		return;
	}
	struct sockaddr_in servaddr;
	//struct sockaddr_in clientaddr;

	char *address = gACBEServerAddr;
	int port = gACBEServerPort;
	
	CWLog("SendBERequest ,addr = %s,port = %d...... ",address,port);
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		CWLog("SendBERequest socket init error ");
		close(sock);
		return ;
	}

//bind src port
	if((iPtr = ((int*)CWThreadGetSpecific(&gIndexSpecific))) == NULL) {

		CWLog("Error find ap index!");
		close(sock);
		return ;
	}

#if 0
	bzero(&clientaddr, sizeof (struct sockaddr_in));
	clientaddr.sin_family = AF_INET;
	clientaddr.sin_port = htons((BE_SOCKET_PORT_MIN+*iPtr));
	inet_pton(AF_INET, address, &clientaddr.sin_addr);

	if (  bind(sock, (struct sockaddr *) &clientaddr, sizeof(struct sockaddr_in)) < 0 ) {
		CWLog("Error on Binding");
		close(sock);
		return;
	}
#endif

	bzero(&servaddr, sizeof (struct sockaddr_in));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	inet_pton(AF_INET, address, &servaddr.sin_addr);

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optValue, sizeof(int)) == -1) 
	{
		CWLog("Error on socket creation on Interface");
		close(sock);
	}

	//setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO, (char *)&nNetTimeout,sizeof(int));
	//setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO, (char *)&nNetTimeout,sizeof(int));
	//ret=select(sock+1,&fdRead,NULL,NULL,&TimeOut);
	
	if ((ret = connect(sock, (SA*) &servaddr, sizeof(struct sockaddr_in)) )< 0) {
		CWLog("SendBERequest connect error,ret = %d",ret);
		return ;
	}
	

	CWLog("[F:%s, L:%d] SendBERequset len:%d",__FILE__,__LINE__,len);
	if(len > BE_MAX_PACKET_LEN)
	{
		CWLog("SendBERequset Error len > 80000");
		return;
	}
	
	while(n != len)
	{
		if ( (n += Writen(sock, buf, len))  < 0 ) {
			//CWThreadMutexUnlock(&appsManager.appClientSocketMutex);
			CWLog("Error write appsManager.appClientSocket");
			break;
		}
//		CWLog("[F:%s, L:%d] Writen n:%d !=len",__FILE__,__LINE__,n);
	}
	CWLog("[F:%s, L:%d] Writen n:%d",__FILE__,__LINE__,n);

	BEHeader beHeader;
	int finded = -1;
	unsigned short beType;
	int beLen;
	char ac;
	ActiveCode actCo;


	CWLog("[F:%s, L:%d] ",__FILE__,__LINE__);
	

	if(is_valid_wtp_index(*iPtr) != CW_TRUE)
	{
		CWLog("AP not run ,no need response ...");
		goto quit_manage;
	}

	int flags;
	flags=fcntl(sock,F_GETFL,0);
	fcntl(sock,F_SETFL,flags|O_NONBLOCK);
	
	fd_set fdRead;
	struct timeval TimeOut;
	TimeOut.tv_sec=3;
	TimeOut.tv_usec=0;

	FD_ZERO(&fdRead);
	FD_SET(sock,&fdRead);
	
	CWLog("[F:%s, L:%d] select begin,wait 3s",__FILE__,__LINE__);
	ret=select(sock+1,&fdRead,NULL,NULL,&TimeOut);
	CWLog("[F:%s, L:%d] select end,wait 3s ",__FILE__,__LINE__);
	if(ret < 0)
	{
		CWLog("Error on receive BEHeader !");
		goto quit_manage;
	}

	CWLog("[F:%s, L:%d] ",__FILE__,__LINE__);

	if(ret == 0)
	{
		CWLog("Time out, Nothing on receive BE !");
		goto quit_manage;
	}

	if(!FD_ISSET(sock, &fdRead))
	{
		CWLog("Sock not avaliable !");
		goto quit_manage;	
	}
	if ( ( n = Readn(sock, &beHeader.type, BE_TYPE_LEN))> 0 ) 
	{
			//type
			beHeader.type = ntohs(beHeader.type );
			if(beHeader.type != BE_CAPWAP_HEADER)
			{
				CWLog("Error on receive BEHeader !,type = %d",beHeader.type);
				goto quit_manage;
			}
			CWLog("Receive BEHeader ...");

			//len
			if  ((n = Readn(sock, &beHeader.length, BE_LENGTH_LEN)) < 0 )
			{
					CWLog("Error while reading from socket.");
					goto quit_manage;
			}
			beHeader.length = Swap32(beHeader.length);
			if(beHeader.length < BE_HEADER_MIN_LEN )
			{
				CWLog("Error beHeader.length = %d not in range !",beHeader.length);
				goto quit_manage;
			}	
			CWLog("Receive BEHeader.length = %d",beHeader.length);

			//timestamp
			if  ((n = Readn(sock, &beHeader.timestamp, TIME_LEN)) < 0 )
			{
					CWLog("Error while reading from socket.");
					goto quit_manage;
			}
			beHeader.timestamp = Swap32(beHeader.timestamp);
			
			CWLog("Receive BEHeader.timestamp = %d",beHeader.timestamp);

			//apmac
			if ((n = Readn(sock, beHeader.apMac, MAC_ADDR_LEN )) < 0 ) 
			{
				CWLog("Error while reading from socket.");
				goto quit_manage;
			}
#if 0
			memset(macTemp,0,MAC_ADDR_LEN);
			memcpy(macTemp,beHeader.apMac,MAC_ADDR_LEN);
			
			for(i = (MAC_ADDR_LEN -1);i >= 0;i--)
			{
				j = MAC_ADDR_LEN -1 -i;
				beHeader.apMac[j] = macTemp[i];				
			}
#endif			
			CWLog("Receive beHeader.apMac = %x:%x:%x:%x:%x:%x ",
								beHeader.apMac[0],
								beHeader.apMac[1],
								beHeader.apMac[2],
								beHeader.apMac[3],
								beHeader.apMac[4],
							   	beHeader.apMac[5]);


			//test
#if 0
			//BE: ap connect
			BEconnectEvent beConEve;
			int BESize;
			char *beResp = NULL;
			beConEve.type = htons(BE_CONNECT_EVENT);
			beConEve.length = htons(BE_CONNECT_EVENT_LEN);
			CWLog("[F:%s, L:%d] :-------------sendpacket(start)-------------------------",__FILE__,__LINE__);
			CWLog("[F:%s, L:%d] :connectevent type=%d,length=%d,state=%d",__FILE__,__LINE__,BE_CONNECT_EVENT,BE_CONNECT_EVENT_LEN,BE_CONNECT_EVENT_CONNECT);
			//beConEve.type = BE_CONNECT_EVENT;
			//beConEve.length = BE_CONNECT_EVENT_LEN;
			beConEve.state = BE_CONNECT_EVENT_CONNECT;
			BESize = BE_CONNECT_EVENT_LEN + BE_TYPELEN_LEN;
			wtpIndex = 0;
			
			gWTPs[wtpIndex].applicationIndex = socketIndex;
			gWTPs[wtpIndex].isNotFree = TRUE;
			gWTPs[wtpIndex].currentState = CW_ENTER_RUN;
			
			for(i=0; i<MAC_ADDR_LEN; i++)
			{
				gWTPs[wtpIndex].MAC[i] = 88;
				//CWLog("[F:%s, L:%d] :i=%d,wtpIndex= %d,gWTPs[wtpIndex].MAC[i] = %x",__FILE__,__LINE__,i,wtpIndex,gWTPs[wtpIndex].MAC[i]);	
			}
		
			beResp = AssembleBEheader((char*)&beConEve,&BESize,wtpIndex);
			CWLog("[F:%s, L:%d] :BESize = %d",__FILE__,__LINE__,BESize);
			if(beResp)
			{
				//SendBERequest(beResp,BESize);
				SendBEResponse(beResp,BESize,wtpIndex);
			CWLog("[F:%s, L:%d] :-------------sendpacket(end)-------------------------",__FILE__,__LINE__);
				CW_FREE_OBJECT(beResp);
			}
			else
			{
				CWLog("Error AssembleBEheader !");
				goto quit_manage;
			}
#endif	
			//	

			finded = FindApIndex(beHeader.apMac);
			if(finded < 0)
			{
				CWLog("[F:%s, L:%d] can't find this ap",__FILE__,__LINE__);
				goto quit_manage;
			}
			if ( ( n = Readn(sock, &beType, BE_TYPE_LEN) ) > 0 ) 
			{
				beType = ntohs(beType);
				if(beType == BE_CONNECT_EVENT_RSP)
				{
				CWLog("Receive BE_CONNECT_EVENT_RSP !");
				
				//len
				if ( (n = Readn(sock, &beLen, BE_LENGTH_LEN) )< 0 )
				{
						CWLog("Error while reading from socket.");
						goto quit_manage;
				}
				beLen = Swap32(beLen);
				if(beLen)
				{
					CWLog("Error beLen = %d not in range !",beLen);
					goto quit_manage;
				}	
				CWLog("Receive beLen = %d",beLen);


				//active
				if ( (n = Readn(sock, &ac, BE_ACTCODE_LEN) )< 0 )
				{
						CWLog("Error while reading from socket.");
						goto quit_manage;
				}
				actCo = ac;
				CWLog("Receive ActiveCode = %d",actCo);


				if(finded < 0)
				{
					CWLog("[F:%s, L:%d] can't find this ap",__FILE__,__LINE__);
					goto quit_manage;
				}

#if 0
//test
				char *temp = NULL;
				temp = "<?xml version=\"1.0\"?><config><SSID>1</SSID></config>";
				
				payloadSize = strlen(temp);
			       BEmonitorEventResponse beMonitorEventResp;
				beMonitorEventResp.type =htons( BE_MONITOR_EVENT_RESPONSE) ;
				beMonitorEventResp.length = htons(payloadSize);
				
				CW_CREATE_STRING_ERR(beMonitorEventResp.xml, payloadSize, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});				
				memset(beMonitorEventResp.xml, 0, payloadSize);
				memcpy(beMonitorEventResp.xml, "12345678", payloadSize);
				BESize = BE_TYPELEN_LEN+payloadSize;

				beResp = AssembleBEheader((char*)&beMonitorEventResp,&BESize,WTPIndex);
				CW_FREE_OBJECT(beMonitorEventResp.xml);

				if(beResp)
				{
					//SendBERequest(beResp,BESize);
					SendBEResponse(beResp,BESize,wtpIndex);
					CW_FREE_OBJECT(beResp);
				}
				else
				{
					CWLog("Error AssembleBEheader !");
					goto quit_manage;
				}	
#endif
				if(BESetActValues(beHeader.apMac, 0, actCo) == CW_FALSE)
				{
					CWLog("BESetActValues fail !");	
				}

				goto quit_manage;
			}
				
			}
			
		}
quit_manage:
	
	if (close(sock) < 0) {
		CWLog("SendBERequest close error!");
	}
	
	return;
}


char UpgradeVersion(u_char* apMac, int socketIndex,void *cup, struct version_info update_v)
{
	int ret = FALSE;
	int i, left, toSend, sent;
	CWVendorWumValues *wumValues = NULL;
	CWProtocolVendorSpecificValues *vendorValues = NULL;

	if(apMac == NULL || cup == NULL)
	{
		CWLog("[F:%s, L:%d]UpgradeVersion apMac == NULL || cup == NULL",__FILE__,__LINE__);
		return ret;
	}

	CW_CREATE_OBJECT_SIZE_ERR(wumValues, sizeof(CWVendorWumValues), {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});
	CW_CREATE_OBJECT_ERR(vendorValues, CWProtocolVendorSpecificValues, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});

	memset((char*)wumValues,0,sizeof(CWVendorWumValues));
	memset((char*)vendorValues,0,sizeof(CWProtocolVendorSpecificValues));
	
	//CW_CREATE_OBJECT_ERR(vendorValues->payload, CWVendorWumValues, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});
	//memset((char*)vendorValues->payload,0,sizeof(CWVendorWumValues));

	 vendorValues->vendorPayloadType = CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_WUM;

	//WTP_UPDATE_REQ
	CWLog("[F:%s, L:%d] WTP_UPDATE_REQ begin. ..... ",__FILE__,__LINE__);
	wumValues->type = WTP_UPDATE_REQUEST;
	wumValues->_major_v_ = update_v.major;
	wumValues->_minor_v_ = update_v.minor;
	wumValues->_revision_v_ = update_v.revision;
	wumValues->_pack_size_ = update_v.size;
	//wumValues._pack_size_ = ntohl(wumValues._pack_size_);
	
	
	//CWLog("[F:%s, L:%d] wumValues->_pack_size_ = %d ",__FILE__,__LINE__,wumValues->_pack_size_);
	
	//memcpy((char*)(vendorValues->payload),(char*)wumValues,sizeof(CWVendorWumValues));
	vendorValues->payload = NULL;
	vendorValues->payload = wumValues;
	ret = BESetWumValues(apMac, socketIndex, vendorValues);
	if(!ret)
	{
		CWLog("[F:%s, L:%d] BESetWumValues fail ! ",__FILE__,__LINE__);
		CW_FREE_OBJECT(wumValues);
		CW_FREE_OBJECT(vendorValues);
		return ret;
	}
	if(ret == CW_FAILURE_WTP_UPGRADING_REJECT_NWEUPGRADE)
	{
		CWLog("[F:%s, L:%d] BESetWumValues  CW_FAILURE_WTP_UPGRADING_REJECT_NWEUPGRADE, stop !! ",__FILE__,__LINE__);
		CW_FREE_OBJECT(wumValues);
		CW_FREE_OBJECT(vendorValues);
		return ret;
	}
	 //WTP_CUP_FRAGMENT
	CWLog("[F:%s, L:%d] WTP_CUP_FRAGMENT begin. ..... ",__FILE__,__LINE__);
	memset((char*)wumValues,0,sizeof(CWVendorWumValues));
	wumValues->type = WTP_CUP_FRAGMENT;
	
	int seqNum = 0;
	int fragSize = 0;
	
	sent = 0;
	left = update_v.size;
	toSend = MIN(FRAGMENT_SIZE, left);
	CWLog("[F:%s, L:%d] seqNum = %d,fragSize  = %d,sent = %d ",__FILE__,__LINE__,seqNum,fragSize,sent);
	for (i = 0; left > 0; i++) {
	//if (WUMSendFragment(acserver, wtpId, cup_buf + sent, toSend, i)) {
	//	fprintf(stderr, "Error while sending fragment #%d\n", i);
	//	return ERROR;
	//}
		//seqNum = ntohl(i);
		//fragSize = ntohl(toSend);
		seqNum = i;
		fragSize = toSend;

		//CW_CREATE_OBJECT_SIZE_ERR(wumValues->_cup_, fragSize, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});
		//CW_CREATE_OBJECT_SIZE_ERR(vendorValues.payload, sizeof(CWVendorWumValues), {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});
		//memset(wumValues->args.cup.buf,0,fragSize);
		CWLog("[F:%s, L:%d] ",__FILE__,__LINE__);
		//memset((char*)wumValues,0,sizeof(CWVendorWumValues));
		CWLog("[F:%s, L:%d]  ",__FILE__,__LINE__);
		//memset((char*)vendorValues->payload,0,sizeof(CWVendorWumValues));
		CWLog("[F:%s, L:%d] seqNum = %d,fragSize  = %d,sent = %d ",__FILE__,__LINE__,seqNum,fragSize,sent);

		//CW_FREE_OBJECT(wumValues->_cup_);
		wumValues->_cup_ = NULL;
		wumValues->_cup_ = (char*)cup + sent;
		//memcpy((char*)(wumValues->args.cup.buf),(char*) (cup + sent), fragSize);
		wumValues->_seq_num_ = seqNum;
		wumValues->_cup_fragment_size_ = fragSize;
		
		vendorValues->payload = NULL;
		//memcpy((char*)(vendorValues->payload),(char*)wumValues,sizeof(CWVendorWumValues));
		vendorValues->payload = wumValues;

		CWLog("[F:%s, L:%d] -------%d  fragment send----------",__FILE__,__LINE__,seqNum);
		usleep(100);
		ret = BESetWumValues(apMac, socketIndex,vendorValues);
		if(!ret)
		{
			CWLog("[F:%s, L:%d] BESetWumValues fail ! ",__FILE__,__LINE__);
			//CW_FREE_OBJECT(wumValues->_cup_);
			wumValues->_cup_ = NULL;
			CW_FREE_OBJECT(wumValues);
			CW_FREE_OBJECT(vendorValues);
			return ret;
		}
		CWLog("[F:%s, L:%d] -------%d  fragment recv----------",__FILE__,__LINE__,seqNum);
		left -= toSend;
		sent += toSend;
		toSend = MIN(FRAGMENT_SIZE, left);	
	} 

	//WTP_COMMIT_UPDATE
	CWLog("[F:%s, L:%d] WTP_COMMIT_UPDATE begin. ..... ",__FILE__,__LINE__);
	//CW_FREE_OBJECT(wumValues->_cup_);
	memset(wumValues,0,sizeof(CWVendorWumValues));
	wumValues->type = WTP_COMMIT_UPDATE; 

	//memset(vendorValues.payload,0,sizeof(CWVendorWumValues));
	vendorValues->payload = NULL;
	vendorValues->payload = wumValues;
	//memcpy((char*)(vendorValues->payload),(char*)&wumValues,sizeof(CWVendorWumValues));
	ret = BESetWumValues(apMac, socketIndex, vendorValues);
	if(!ret)
	{
		CWLog("[F:%s, L:%d] BESetWumValues fail ! ",__FILE__,__LINE__);
		wumValues->_cup_ = NULL;
		CW_FREE_OBJECT(wumValues);
		CW_FREE_OBJECT(vendorValues);
		return ret;
	}
	//AC use
	//CW_FREE_OBJECT(wumValues->_cup_);
	//CW_FREE_OBJECT(wumValues);
	//CW_FREE_OBJECT(vendorValues);
	
	return ret;
}

//only xx.tar can be anylisis
CWResultCode CheckUpgradeVersion(u_char* apMac, int socketIndex, char *cup_path)
{
	int ret=FALSE;
	CWResultCode rc = CW_FAILURE;
	
	struct version_info update_v;
	struct stat s_buf;
	void *cup;

	if (cup_path == NULL) {
		CWLog( "In order to execute an update, an update package must be specified! (-f pathname)\n");
		rc = CW_FAILURE_WTP_IMAGE_PATH_ERROR;
		return rc;
	}

	if(apMac == NULL)
	{
		CWLog("[F:%s, L:%d]CheckUpgradeVersion apMac == NULL ",__FILE__,__LINE__);
		return rc;
	}
	
	int fd = open(cup_path, O_RDONLY);
	if (fd < 0) {
		CWLog("open error");
		rc = CW_FAILURE_WTP_IMAGE_PATH_ERROR;
		return rc;
	}
	
	if (stat(cup_path, &s_buf) != 0) {
		CWLog( "Stat error!.\n");
		return rc;
	}
	//test
	update_v.major = 1;
	update_v.minor = 1;
	update_v.revision = 1;
		
	update_v.size = s_buf.st_size;
	CWLog("[F:%s, L:%d] update_v->size = %d\n",__FILE__,__LINE__,update_v.size);
		
	cup = mmap(NULL, update_v.size, PROT_READ, MAP_SHARED , fd, 0);
	if (cup == NULL) {
		CWLog("mmap error");
		return rc;
	}
	
 	ret = UpgradeVersion(apMac, socketIndex,cup, update_v);
	
	munmap(cup, update_v.size);
	close(fd);
	if(ret == TRUE)
	{
		rc = CW_SUCCESS;
	}
	
	return rc;
}



/************************************************************************
 * CWOFDMSetValues provide to set the command values (type, parameters,	*
 * output socket) on the correct wtp structure.							*
 ************************************************************************/
int CWXMLSetValues(int selection, int socketIndex, CWVendorXMLValues* xmlValues) {

	if(xmlValues == NULL)
	{
		CWLog("[F:%s, L:%d]CWXMLSetValues xmlValues == NULL ",__FILE__,__LINE__);
		return FALSE;
	}
	if(selection <0 ||selection >= CW_MAX_WTP)
	{
		CWLog("[F:%s, L:%d]selection <0 ||selection >= CW_MAX_WTP ",__FILE__,__LINE__);
		return FALSE;
	}
	if(!CWErr(CWThreadMutexLock(&gWTPs[selection].wtpMutex))) {
		CWLog("Error locking the gWTPsMutex!");
		return FALSE;
	}
	
	CW_CREATE_OBJECT_ERR(gWTPs[selection].vendorValues, CWProtocolVendorSpecificValues, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL);CWThreadMutexUnlock(&gWTPs[selection].wtpMutex); return 0;});
	gWTPs[selection].vendorValues->payload = NULL;
	gWTPs[selection].vendorValues->vendorPayloadLen = 0;
	gWTPs[selection].applicationIndex = socketIndex;
	//CWLog("[F:%s, L:%d] ",__FILE__,__LINE__);
	
	if(xmlValues->payloadLen && xmlValues->payload)
	{
		CW_CREATE_STRING_ERR(gWTPs[selection].vendorValues->payload, xmlValues->payloadLen+1, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL);CWThreadMutexUnlock(&gWTPs[selection].wtpMutex); return 0;});
		memset(gWTPs[selection].vendorValues->payload,0,xmlValues->payloadLen+1);
		memcpy((char*)gWTPs[selection].vendorValues->payload, xmlValues->payload,xmlValues->payloadLen);
		CWLog("gWTPs[%d].vendorValues->payload :%s", selection, gWTPs[selection].vendorValues->payload);
		CWLog("gWTPs[%d].vendorValues->payload Len:%d", selection, strlen(gWTPs[selection].vendorValues->payload));
		gWTPs[selection].vendorValues->vendorPayloadLen = strlen(gWTPs[selection].vendorValues->payload);
	}
	if(xmlValues->wum_type == WTP_CONFIG_REQUEST)
	{
		gWTPs[selection].vendorValues->vendorPayloadType = CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_CONFIG;
		gWTPs[selection].interfaceCommand = WTP_CONFIG_CMD;
	}
	else if(xmlValues->wum_type == WTP_STATE_REQUEST)
	{
		gWTPs[selection].vendorValues->vendorPayloadType = CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_STATE;
		gWTPs[selection].interfaceCommand = WTP_STATE_CMD;
	}
	else
	{
		CWLog("[F:%s, L:%d]  Unknown wum_type:%d",__FILE__,__LINE__,xmlValues->wum_type);
		CWThreadMutexUnlock(&gWTPs[selection].wtpMutex);
		return FALSE;
	}
	CWThreadMutexUnlock(&gWTPs[selection].wtpMutex);
	

	if (	(gWTPs[selection].wtpState == CW_RUN))
	{
		if(!CWErr(CWThreadMutexLock(&(gWTPs[selection].wtpMutex))))
		{
			CWLog("Error locking wtpMutex !");
			return FALSE;
		}
		CWSignalThreadCondition(&gWTPs[selection].wtpWait);
		CWThreadMutexUnlock(&(gWTPs[selection].wtpMutex));
		
		if(!CWErr(CWThreadMutexLock(&(gWTPs[selection].interfaceMutex)))) {
			CWLog("Error locking interfaceMutex !");
			return FALSE;
		}
		CWWaitThreadCondition(&gWTPs[selection].interfaceComplete, &gWTPs[selection].interfaceMutex);
		CWThreadMutexUnlock(&(gWTPs[selection].interfaceMutex));
	}
	else
	{
		CWLog("%s  %d WTP[%d] not online, BE request cannel !",__FILE__,__LINE__,selection);
		gWTPs[selection].interfaceCommand = NO_CMD;
		return FALSE;
	}
	
	return TRUE;
}


int CWPortalSetValues(int selection, int socketIndex, CWVendorPortalValues* portalValues) {
	
	if(portalValues == NULL)
	{
		CWLog("[F:%s, L:%d]CWPortalSetValues portalValues == NULL ",__FILE__,__LINE__);
		return FALSE;
	}
	if(!portalValues->EncodeNameLen || !portalValues->EncodeContentLen)
	{
		CWLog("[F:%s, L:%d]portalValues>EncodeNameLen = 0 ||portalValues->EncodeContentLen =0,Error! ",__FILE__,__LINE__);
		return FALSE;
	}
	if(portalValues->EncodeName == NULL)
	{
		CWLog("[F:%s, L:%d]CWPortalSetValues portalValues->EncodeName == NULL ",__FILE__,__LINE__);
		return FALSE;
	}
	if(portalValues->EncodeContent == NULL)
	{
		CWLog("[F:%s, L:%d]CWPortalSetValues portalValues->EncodeContent == NULL ",__FILE__,__LINE__);
		return FALSE;
	}

	if(selection <0 ||selection >= CW_MAX_WTP )
	{
		CWLog("[F:%s, L:%d]selection <0 ||selection >= CW_MAX_WTP ",__FILE__,__LINE__);
		return FALSE;
	}

	gWTPs[selection].vendorPortalValues = NULL;
	
	//CWThreadMutexLock(&(gWTPs[selection].interfaceMutex));
	//portal to vendor	
	CW_CREATE_OBJECT_ERR(gWTPs[selection].vendorPortalValues, CWProtocolVendorPortalValues, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});
	gWTPs[selection].vendorPortalValues->TotalFileNum= portalValues->TotalFileNum;
	gWTPs[selection].vendorPortalValues->FileNo= portalValues->FileNo;
	gWTPs[selection].vendorPortalValues->EncodeNameLen= portalValues->EncodeNameLen;
	gWTPs[selection].vendorPortalValues->EncodeContentLen= portalValues->EncodeContentLen;
	//CWThreadMutexUnlock(&(gWTPs[selection].interfaceMutex));
	
	CWLog("[F:%s, L:%d]gWTPs[selection].vendorPortalValues->TotalFileNum = %d ",__FILE__,__LINE__,gWTPs[selection].vendorPortalValues->TotalFileNum);
	CWLog("[F:%s, L:%d]gWTPs[selection].vendorPortalValues->FileNo = %d ",__FILE__,__LINE__,gWTPs[selection].vendorPortalValues->FileNo);
	CWLog("[F:%s, L:%d]gWTPs[selection].vendorPortalValues->EncodeNameLen = %d ",__FILE__,__LINE__,gWTPs[selection].vendorPortalValues->EncodeNameLen);
	CWLog("[F:%s, L:%d]gWTPs[selection].vendorPortalValues->EncodeContentLen = %d ",__FILE__,__LINE__,gWTPs[selection].vendorPortalValues->EncodeContentLen);

	int seqNum;
	int fragSize;
	int sent,left,toSend,i;
	sent = 0;
	left = gWTPs[selection].vendorPortalValues->EncodeContentLen;
	toSend = MIN(FRAGMENT_SIZE, left);
	for (i = 0; left > 0; i++) {
		seqNum = i;
		fragSize = toSend;

		if(left <= toSend)
		{
			gWTPs[selection].vendorPortalValues->isLast = TRUE;
		}
		else
		{
			gWTPs[selection].vendorPortalValues->isLast = FALSE;
		}
		gWTPs[selection].vendorPortalValues->SeqNum= seqNum;
		gWTPs[selection].vendorPortalValues->EncodeContentLen= fragSize;
		gWTPs[selection].vendorPortalValues->EncodeName = NULL;
		CW_CREATE_STRING_ERR(gWTPs[selection].vendorPortalValues->EncodeName, gWTPs[selection].vendorPortalValues->EncodeNameLen+1, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL);return 0;});
		memset(gWTPs[selection].vendorPortalValues->EncodeName,0,gWTPs[selection].vendorPortalValues->EncodeNameLen+1);
		memcpy(gWTPs[selection].vendorPortalValues->EncodeName,  portalValues->EncodeName,portalValues->EncodeNameLen);
		CWLog("gWTPs[%d].vendorPortalValues->EncodeName, :%s", selection, gWTPs[selection].vendorPortalValues->EncodeName);
		CWLog("gWTPs[%d].vendorPortalValues->EncodeName Len:%d", selection, gWTPs[selection].vendorPortalValues->EncodeNameLen);
		//gWTPs[selection].vendorValues->vendorPayloadLen = strlen(gWTPs[selection].vendorValues->payload);
		gWTPs[selection].vendorPortalValues->EncodeContent = NULL;
		CW_CREATE_STRING_ERR(gWTPs[selection].vendorPortalValues->EncodeContent, gWTPs[selection].vendorPortalValues->EncodeContentLen+1, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});
		memset(gWTPs[selection].vendorPortalValues->EncodeContent,0,gWTPs[selection].vendorPortalValues->EncodeContentLen+1);
		memcpy(gWTPs[selection].vendorPortalValues->EncodeContent,  portalValues->EncodeContent+sent,gWTPs[selection].vendorPortalValues->EncodeContentLen);
		CWLog("fragment EncodeContentLen Len:%d", selection, gWTPs[selection].vendorPortalValues->EncodeContentLen);

		//portalValues->EncodeContent = portalValues->EncodeContent + toSend;

		CWLog("[F:%s, L:%d] -------%d  portal fragment send----------",__FILE__,__LINE__,seqNum);
		/*
		ret = BESetWumValues(apMac, socketIndex,vendorValues);
		if(!ret)
		{
			CWLog("[F:%s, L:%d] BESetWumValues fail ! ",__FILE__,__LINE__);
			return ret;
		}
		*/
		//CWThreadMutexLock(&(gWTPs[selection].interfaceMutex));
		gWTPs[selection].applicationIndex = socketIndex;
		gWTPs[selection].interfaceCommand = PORTAL_MSG_CMD;
		//CWThreadMutexUnlock(&gWTPsMutex);
		
		if (	(gWTPs[selection].wtpState == CW_RUN) )
		{
			if(!CWErr(CWThreadMutexLock(&(gWTPs[selection].wtpMutex))))
			{
				CWLog("Error locking wtpMutex !");
				return FALSE;
			}
			CWSignalThreadCondition(&gWTPs[selection].wtpWait);
			CWThreadMutexUnlock(&(gWTPs[selection].wtpMutex));
			
			if(!CWErr(CWThreadMutexLock(&(gWTPs[selection].interfaceMutex)))) {
				CWLog("Error locking interfaceMutex !");
				return FALSE;
			}
			CWWaitThreadCondition(&gWTPs[selection].interfaceComplete, &gWTPs[selection].interfaceMutex);
			CWThreadMutexUnlock(&(gWTPs[selection].interfaceMutex));
		}
		else
		{
			CWLog("%s  %d WTP[%d] not online, BE request cannel !",__FILE__,__LINE__,selection);
			gWTPs[selection].interfaceCommand = NO_CMD;
			return FALSE;
		}
	
		CWLog("[F:%s, L:%d] -------%d  portal fragment recv----------",__FILE__,__LINE__,seqNum);
		left -= toSend;
		sent += toSend;
		CWLog("[F:%s, L:%d] portal fileContent sended %d ",__FILE__,__LINE__,sent);
		toSend = MIN(FRAGMENT_SIZE, left);	
	} 

#if 0
	if(gWTPs[selection].vendorPortalValues->EncodeContentLen)
	{
		
		CW_CREATE_STRING_ERR(gWTPs[selection].vendorPortalValues->EncodeContent, gWTPs[selection].vendorPortalValues->EncodeContentLen+1, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});
		memset(gWTPs[selection].vendorPortalValues->EncodeContent,0,gWTPs[selection].vendorPortalValues->EncodeContentLen+1);
		memcpy(gWTPs[selection].vendorPortalValues->EncodeContent,  portalValues->EncodeContent,portalValues->EncodeContentLen);
		//CWLog("gWTPs[%d].vendorPortalValues->EncodeContent, :%s", selection, gWTPs[selection].vendorPortalValues->EncodeContent);
		CWLog("gWTPs[%d].vendorPortalValues->EncodeContent Len:%d", selection, gWTPs[selection].vendorPortalValues->EncodeContentLen);
		//gWTPs[selection].vendorValues->vendorPayloadLen = strlen(gWTPs[selection].vendorValues->payload);
	}
#endif

	return TRUE;
}




int CWActSetValues(int selection, int socketIndex,ActiveCode actCode ) {
	
	if(selection <0 ||selection >= CW_MAX_WTP)
	{
		CWLog("[F:%s, L:%d]selection <0 ||selection >= CW_MAX_WTP ",__FILE__,__LINE__);
		return FALSE;
	}
	
	if(actCode == active)
	{
		gWTPs[selection].interfaceCommand = ACTIVE_MSG_CMD;
	}
	if(actCode == disactive)
	{
		gWTPs[selection].interfaceCommand = UNACTIVE_MSG_CMD;
	}
	
	gWTPs[selection].applicationIndex = socketIndex;

	CWLog("[F:%s, L:%d] CWSysSetValues begin ...",__FILE__,__LINE__);	

	if (	(gWTPs[selection].wtpState == CW_RUN) )
	{
		if(!CWErr(CWThreadMutexLock(&(gWTPs[selection].wtpMutex))))
		{
			CWLog("Error locking wtpMutex !");
			return FALSE;
		}
		CWSignalThreadCondition(&gWTPs[selection].wtpWait);
		CWThreadMutexUnlock(&(gWTPs[selection].wtpMutex));
		
		if(!CWErr(CWThreadMutexLock(&(gWTPs[selection].interfaceMutex)))) {
			CWLog("Error locking interfaceMutex !");
			return FALSE;
		}
		CWWaitThreadCondition(&gWTPs[selection].interfaceComplete, &gWTPs[selection].interfaceMutex);
		CWThreadMutexUnlock(&(gWTPs[selection].interfaceMutex));
	}
	else
	{
		CWLog("%s  %d WTP[%d] not online, BE request cannel !",__FILE__,__LINE__,selection);
		gWTPs[selection].interfaceCommand = NO_CMD;
		return FALSE;
	}
	
	CWLog("[F:%s, L:%d] CWActSetValues end ...",__FILE__,__LINE__);	
	
	return TRUE;
}




int CWSysSetValues(int selection, int socketIndex,SystemCode sysCode ) {
	
	if(selection <0 ||selection >= CW_MAX_WTP)
	{
		CWLog("[F:%s, L:%d]selection <0 ||selection >= CW_MAX_WTP ",__FILE__,__LINE__);
		return FALSE;
	}
	
	if(sysCode == SYSTEM_RESET)
	{
		gWTPs[selection].interfaceCommand = CLEAR_CONFIG_MSG_CMD;
	}
	if(sysCode == SYSTEM_REBOOT)
	{
		gWTPs[selection].interfaceCommand = SYSTEM_REBOOT_MSG_CMD;
	}
	
	gWTPs[selection].applicationIndex = socketIndex;

	CWLog("[F:%s, L:%d] CWSysSetValues begin ...",__FILE__,__LINE__);	

	if (	(gWTPs[selection].wtpState == CW_RUN) )
	{
		if(!CWErr(CWThreadMutexLock(&(gWTPs[selection].wtpMutex))))
		{
			CWLog("Error locking wtpMutex !");
			return FALSE;
		}
		CWSignalThreadCondition(&gWTPs[selection].wtpWait);
		CWThreadMutexUnlock(&(gWTPs[selection].wtpMutex));
		
		if(!CWErr(CWThreadMutexLock(&(gWTPs[selection].interfaceMutex)))) {
			CWLog("Error locking interfaceMutex !");
			return FALSE;
		}
		CWWaitThreadCondition(&gWTPs[selection].interfaceComplete, &gWTPs[selection].interfaceMutex);
		CWThreadMutexUnlock(&(gWTPs[selection].interfaceMutex));
	}
	else
	{
		CWLog("%s  %d WTP[%d] not online, BE request cannel !",__FILE__,__LINE__,selection);
		gWTPs[selection].interfaceCommand = NO_CMD;
		return FALSE;
	}
	
	CWLog("[F:%s, L:%d] CWSysSetValues end ...",__FILE__,__LINE__);	
	
	return TRUE;
}

int CWWumSetValues(int selection, int socketIndex, CWProtocolVendorSpecificValues* vendorValues) {

	if(vendorValues == NULL)
	{
		CWLog("CWWumSetValues portalValues == NULL");
		return FALSE;
	}
	
	if(selection <0 ||selection >= CW_MAX_WTP)
	{
		CWLog("[F:%s, L:%d]selection <0 ||selection >= CW_MAX_WTP ",__FILE__,__LINE__);
		return FALSE;
	}
	
	if(!CWErr(CWThreadMutexLock(&gWTPs[selection].wtpMutex))) {
		CWLog("Error locking the &gWTPsMutex !");
		return FALSE;
	}
	
	gWTPs[selection].vendorValues = vendorValues;
	gWTPs[selection].vendorValues->payload = vendorValues->payload;
	
	//CWLog("[F:%s, L:%d] gWTPs[%d].vendorValues.payload ->wumValues",__FILE__,__LINE__,selection);
	//CWVendorWumValues *wumValues = NULL;
	//wumValues = (CWVendorWumValues *)(gWTPs[selection].vendorValues->payload);
	//CWLog("wumValues.type = %d,wumValues._major_v_ =%d",__FILE__,__LINE__,wumValues->type,wumValues->_major_v_ );
	//CWLog("wumValues._minor_v_ = %d,wumValues._revision_v_ =%d",__FILE__,__LINE__,wumValues->_minor_v_,wumValues->_revision_v_ );
	//CWLog("wumValues._pack_size_ = %d,",__FILE__,__LINE__,wumValues->_pack_size_);

	//CWLog("wumValues._cup_fragment_size_ = %d,",__FILE__,__LINE__,wumValues->_cup_fragment_size_);
	//CWLog("wumValues._seq_num_ = %d,",__FILE__,__LINE__,wumValues->_seq_num_);

	gWTPs[selection].interfaceCommand = WTP_UPDATE_CMD;
	gWTPs[selection].applicationIndex = socketIndex;
	CWThreadMutexUnlock(&gWTPs[selection].wtpMutex);
	CWLog("[F:%s, L:%d] CWWumSetValues begin ...",__FILE__,__LINE__);

	if (	(gWTPs[selection].wtpState == CW_RUN) )
	{
		if(!CWErr(CWThreadMutexLock(&(gWTPs[selection].wtpMutex))))
		{
			CWLog("Error locking wtpMutex !");
			return FALSE;
		}
		CWSignalThreadCondition(&gWTPs[selection].wtpWait);
		CWThreadMutexUnlock(&(gWTPs[selection].wtpMutex));
		
		if(!CWErr(CWThreadMutexLock(&(gWTPs[selection].interfaceMutex)))) {
			CWLog("Error locking interfaceMutex !");
			return FALSE;
		}
		CWWaitThreadCondition(&gWTPs[selection].interfaceComplete, &gWTPs[selection].interfaceMutex);
		CWThreadMutexUnlock(&(gWTPs[selection].interfaceMutex));
	}
	else
	{
		CWLog("%s  %d WTP[%d] not online, BE request cancel !",__FILE__,__LINE__,selection);
		gWTPs[selection].interfaceCommand = NO_CMD;
		return FALSE;
	}
	
	CWLog("[F:%s, L:%d] CWWumSetValues end ...",__FILE__,__LINE__);	
	
	return TRUE;
}	



/************************************************************************
 * CWManageApplication is the function that provide the management of	*
 * interaction with a single application.								*
 * -------------------------------------------------------------------- *
 * The messages used are defined in ACAppsProtocol.h					*
 ************************************************************************/

CW_THREAD_RETURN_TYPE CWManageApplication(void* arg) {
	
	int socketIndex = ((CWInterfaceThreadArg*)arg)->index;
	CWSocket sock = appsManager.appSocket[socketIndex];
	int n,resultCode,finded = -1; 
	//int connected= htonl(CONNECTION_OK), gActiveWTPsTemp;
	
	char commandBuffer[COMMAND_BUFFER_SIZE];
	//char wtpListBuffer[WTP_LIST_BUFFER_SIZE];
	
	//int payload_size;
	//int i, j,k, nameLength, numActiveWTPs=0, wtpIndex;
   	//int iTosend, nLtoSend;
	//unsigned char msg_elem;
	unsigned short beType;
	unsigned int beLen;
	BEHeader beHeader;
	//CWProtocolVendorSpecificValues* vendorValues;
	//CWVendorWumValues* wumValues;
	CWVendorXMLValues* xmlValues;
	unsigned char operation;
	char result;
	CWResultCode rc = CW_FAILURE;
	char *filePath = NULL;
	//test
#if 0
	int BESize,resultCode = 0,WTPIndex = 0,payloadSize;
	char *beResp = NULL;
#endif
		
	/********************************************************************************
	 * Write on application socket that connection setting is happened correctly.	*
	 ********************************************************************************/
  	/*
	if ( Writen(sock, &connected, sizeof(int) ) < 0 ) {
		CWLog("Error on writing on application socket ");
		return NULL;
	}
*/
	/*
	 * Before starting, make sure to detach the thread because the parent 
 	 * doesn't do a join and we risk resource leaks.
 	 *
 	 * ref -> BUG-TRL01
	 * 15/10/2009 - Donato Capitella
	 */

        pthread_detach(pthread_self()); // no need here to check return value
	
	/************************
	 *	 Thread Main Loop	*
	 ************************/
	
	CW_REPEAT_FOREVER 
	{ 

		memset(commandBuffer, 0, COMMAND_BUFFER_SIZE);
		
		/****************************************
		 *		Waiting for client commands		*
		 ****************************************/

		//Parse BEHeader
		n = 0;
		beType = 0;
		beLen = 0;
		beHeader.type = 0;
		
		if ( ( n = Readn(sock, &beHeader.type, BE_TYPE_LEN))> 0 ) 
		{
			//type
			beHeader.type = ntohs(beHeader.type );
			if(beHeader.type != BE_CAPWAP_HEADER)
			{
				CWLog("Error on receive BEHeader !,type = %d",beHeader.type);
				goto quit_manage;
			}
			CWLog("Receive BEHeader ...");

			//len
			if  ((n = Readn(sock, &beHeader.length, BE_LENGTH_LEN)) < 0 )
			{
					CWLog("Error while reading from socket.");
					goto quit_manage;
			}
			beHeader.length = Swap32(beHeader.length);
			if(beHeader.length < BE_HEADER_MIN_LEN )
			{
				CWLog("Error beHeader.length = %d not in range !",beHeader.length);
				goto quit_manage;
			}	
			CWLog("Receive BEHeader.length = %d",beHeader.length);

			//timestamp
			if  ((n = Readn(sock, &beHeader.timestamp, TIME_LEN)) < 0 )
			{
					CWLog("Error while reading from socket.");
					goto quit_manage;
			}
			beHeader.timestamp = Swap32(beHeader.timestamp);
			
			CWLog("Receive BEHeader.timestamp = %d",beHeader.timestamp);

			//apmac
			if ((n = Readn(sock, beHeader.apMac, MAC_ADDR_LEN )) < 0 ) 
			{
				CWLog("Error while reading from socket.");
				goto quit_manage;
			}
#if 0
			memset(macTemp,0,MAC_ADDR_LEN);
			memcpy(macTemp,beHeader.apMac,MAC_ADDR_LEN);
			
			for(i = (MAC_ADDR_LEN -1);i >= 0;i--)
			{
				j = MAC_ADDR_LEN -1 -i;
				beHeader.apMac[j] = macTemp[i];				
			}
#endif			
			CWLog("Receive beHeader.apMac = %x:%x:%x:%x:%x:%x ",
								beHeader.apMac[0],
								beHeader.apMac[1],
								beHeader.apMac[2],
								beHeader.apMac[3],
								beHeader.apMac[4],
							   	beHeader.apMac[5]);


			//test
#if 0
			//BE: ap connect
			BEconnectEvent beConEve;
			int BESize;
			char *beResp = NULL;
			beConEve.type = htons(BE_CONNECT_EVENT);
			beConEve.length = htons(BE_CONNECT_EVENT_LEN);
			CWLog("[F:%s, L:%d] :-------------sendpacket(start)-------------------------",__FILE__,__LINE__);
			CWLog("[F:%s, L:%d] :connectevent type=%d,length=%d,state=%d",__FILE__,__LINE__,BE_CONNECT_EVENT,BE_CONNECT_EVENT_LEN,BE_CONNECT_EVENT_CONNECT);
			//beConEve.type = BE_CONNECT_EVENT;
			//beConEve.length = BE_CONNECT_EVENT_LEN;
			beConEve.state = BE_CONNECT_EVENT_CONNECT;
			BESize = BE_CONNECT_EVENT_LEN + BE_TYPELEN_LEN;
			wtpIndex = 0;
			
			gWTPs[wtpIndex].applicationIndex = socketIndex;
			gWTPs[wtpIndex].isNotFree = TRUE;
			gWTPs[wtpIndex].currentState = CW_ENTER_RUN;
			
			for(i=0; i<MAC_ADDR_LEN; i++)
			{
				gWTPs[wtpIndex].MAC[i] = 88;
				//CWLog("[F:%s, L:%d] :i=%d,wtpIndex= %d,gWTPs[wtpIndex].MAC[i] = %x",__FILE__,__LINE__,i,wtpIndex,gWTPs[wtpIndex].MAC[i]);	
			}
		
			beResp = AssembleBEheader((char*)&beConEve,&BESize,wtpIndex);
			CWLog("[F:%s, L:%d] :BESize = %d",__FILE__,__LINE__,BESize);
			if(beResp)
			{
				//SendBERequest(beResp,BESize);
				SendBEResponse(beResp,BESize,wtpIndex);
			CWLog("[F:%s, L:%d] :-------------sendpacket(end)-------------------------",__FILE__,__LINE__);
				CW_FREE_OBJECT(beResp);
			}
			else
			{
				CWLog("Error AssembleBEheader !");
				goto quit_manage;
			}
#endif	
			//	

			finded = FindApIndex(beHeader.apMac);
			if(finded < 0)
			{
				CWLog("[F:%s, L:%d] can't find this ap",__FILE__,__LINE__);
			}
		}
		//BEHeader end
		else
		{
			CWLog("Receive packet beHeader.type length error, drop it !",beHeader.type);
			goto quit_manage;
		}
		if ( ( n = Readn(sock, &beType, BE_TYPE_LEN) ) > 0 ) 
		{
			beType = ntohs(beType);
			
			if ( beType == BE_MONITOR_EVENT_REQUSET )
			{
				CWLog("Receive BE_MONITOR_EVENT_REQUSET !");
				
				//len
				if ( (n = Readn(sock, &beLen, BE_LENGTH_LEN) )< 0 )
				{
						CWLog("Error while reading from socket.");
						goto quit_manage;
				}
				beLen = Swap32(beLen);
				if(beLen)
				{
					CWLog("Error beLen = %d not in range !",beLen);
					goto quit_manage;
				}	
				CWLog("Receive beLen = %d",beLen);

				if(finded < 0)
				{
					CWLog("[F:%s, L:%d] can't find this ap",__FILE__,__LINE__);
					rc = CW_FAILURE_WTP_NOT_CONNECTED;
					SendBEResponseDirectly(BE_MONITOR_EVENT_RESPONSE,beHeader.apMac,socketIndex,rc);
					goto quit_manage;
				}

#if 0
//test
				char *temp = NULL;
				temp = "<?xml version=\"1.0\"?><config><SSID>1</SSID></config>";
				
				payloadSize = strlen(temp);
			       BEmonitorEventResponse beMonitorEventResp;
				beMonitorEventResp.type =htons( BE_MONITOR_EVENT_RESPONSE) ;
				beMonitorEventResp.length = htons(payloadSize);
				
				CW_CREATE_STRING_ERR(beMonitorEventResp.xml, payloadSize, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});				
				memset(beMonitorEventResp.xml, 0, payloadSize);
				memcpy(beMonitorEventResp.xml, "12345678", payloadSize);
				BESize = BE_TYPELEN_LEN+payloadSize;

				beResp = AssembleBEheader((char*)&beMonitorEventResp,&BESize,WTPIndex);
				CW_FREE_OBJECT(beMonitorEventResp.xml);

				if(beResp)
				{
					//SendBERequest(beResp,BESize);
					SendBEResponse(beResp,BESize,wtpIndex);
					CW_FREE_OBJECT(beResp);
				}
				else
				{
					CWLog("Error AssembleBEheader !");
					goto quit_manage;
				}	
#endif

				CW_CREATE_OBJECT_ERR(xmlValues, CWVendorXMLValues, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});
				xmlValues->wum_type =WTP_STATE_REQUEST;
				xmlValues->payloadLen = beLen;

				result = FALSE;
				result = BESetApValues(beHeader.apMac, socketIndex, xmlValues);
				//CW_FREE_OBJECT(xmlValues->payload);
				CW_FREE_OBJECT(xmlValues);
				if(!result)
				{
					SendBEResponseDirectly(BE_MONITOR_EVENT_RESPONSE,beHeader.apMac,socketIndex,rc);
					//goto quit_manage;
				}
				goto quit_manage;
			}

			if ( beType == BE_CONFIG_EVENT_REQUSET ) {

				/****************************************
				 * Manage CONF command			*
				 * ------------------------------------ *
				 * 1. Select the type of CONF_UPDATE,	*
				 * 2. Get Index of WTP,			*
				 * 3. Manage request.			*
				 ****************************************/
				CWLog("Receive BE_CONFIG_EVENT_REQUSET !");
				
				//len
				if ( (n = Readn(sock, &beLen, BE_LENGTH_LEN) )< 0 ) {
						CWLog("Error while reading from socket.");
						goto quit_manage;
				}
				beLen = Swap32(beLen);
				if(!beLen)
				{
					CWLog("Error beLen = %d not in range !",beLen);
					goto quit_manage;
				}	
				CWLog("Receive beLen = %d",beLen);

				if(finded < 0)
				{
					CWLog("[F:%s, L:%d] can't find this ap",__FILE__,__LINE__);
					rc = CW_FAILURE_WTP_NOT_CONNECTED;
					SendBEResponseDirectly(BE_CONFIG_EVENT_RESPONSE,beHeader.apMac,socketIndex,rc);
					goto quit_manage;
				}
//test
#if 0	
				char *filePath = NULL;

				CW_CREATE_STRING_ERR(filePath, 64, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});
				memset(filePath,0,64);
				memcpy(filePath,"/capwap_server/version/yanlong_uImage.tar",strlen("/capwap_server/version/yanlong_uImage.tar"));
				CWLog("[F:%s, L:%d] filePath= %s ",__FILE__,__LINE__,filePath);

				result = CheckUpgradeVersion(beHeader.apMac, socketIndex,filePath);	
				CW_FREE_OBJECT(filePath);
				
				if(!result)
				{
					goto quit_manage;
				}
				goto quit_manage;
#endif	
				CW_CREATE_OBJECT_ERR(xmlValues, CWVendorXMLValues, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});
				xmlValues->wum_type =WTP_CONFIG_REQUEST;
				xmlValues->payloadLen = beLen;
				CW_CREATE_STRING_ERR(xmlValues->payload, beLen+1, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});
				memset(xmlValues->payload,0,beLen+1);

				if ( (n = Readn(sock, xmlValues->payload, beLen) )< 0 ) {
						CWLog("Error while reading from socket.");
						goto quit_manage;
						
				}
				
				if(!xmlValues->payload){
					CWLog("Error xml= %s is NULL !",xmlValues->payload);
					goto quit_manage;
				}	
				
				CWLog("Receive xml len =%d,value = %s",n, xmlValues->payload);

				result = FALSE;
				result = BESetApValues(beHeader.apMac, socketIndex, xmlValues);
				CW_FREE_OBJECT(xmlValues->payload);
				CW_FREE_OBJECT(xmlValues);	
				if(!result)
				{
					SendBEResponseDirectly(BE_CONFIG_EVENT_RESPONSE,beHeader.apMac,socketIndex,rc);
					//goto quit_manage;
				}
				goto quit_manage;
			
			}

			if( beType == BE_UPGRADE_EVENT_REQUEST)
			{
				CWLog("Receive BE_UPGRADE_EVENT_REQUEST !");
				//len
				if ( (n = Readn(sock, &beLen, BE_LENGTH_LEN) )< 0 ) {
						CWLog("Error while reading from socket.");
						goto quit_manage;
				}
				beLen = Swap32(beLen);
				if(!beLen)
				{
					CWLog("Error beLen = %d not in range !",beLen);
					goto quit_manage;
				}	
				CWLog("Receive beLen = %d",beLen);

				
				if(finded < 0)
				{
					CWLog("[F:%s, L:%d] can't find this ap",__FILE__,__LINE__);
					rc = CW_FAILURE_WTP_NOT_CONNECTED;
					SendBEResponseDirectly(BE_UPGRADE_EVENT_RESPONSE,beHeader.apMac,socketIndex,rc);
					goto quit_manage;
				}

				

				CW_CREATE_STRING_ERR(filePath, beLen+1, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});
				memset(filePath,0,beLen+1);
				
				if ( (n = Readn(sock, filePath, beLen) )< 0 ) {
						CWLog("Error while reading from socket.");
						goto quit_manage;
				}

				CWLog("[F:%s, L:%d] filePath=%s",__FILE__,__LINE__,filePath);

				rc = CheckUpgradeVersion(beHeader.apMac, socketIndex,filePath);
				CW_FREE_OBJECT(filePath);

				if(rc != CW_SUCCESS)
				{
					CWLog("[F:%s, L:%d] resultCode = %d",__FILE__,__LINE__,(int)rc);
					SendBEResponseDirectly(BE_UPGRADE_EVENT_RESPONSE,beHeader.apMac,socketIndex,rc);

				}
                           goto quit_manage;
			}

			//Ray
			if( beType == BE_PORTAL_EVENT_REQUEST)
			{
				CWLog("Receive BE_PORTAL_EVENT_REQUEST !");
				//len
				if ( (n = Readn(sock, &beLen, BE_LENGTH_LEN) )< 0 ) {
						CWLog("Error while reading from socket.");
						goto quit_manage;
				}
				beLen = Swap32(beLen);
				if(!beLen)
				{
					CWLog("Error beLen = %d not in range !",beLen);
					goto quit_manage;
				}	
				CWLog("Receive beLen = %d",beLen);

				if(finded < 0)
				{
					CWLog("[F:%s, L:%d] can't find this ap",__FILE__,__LINE__);
					rc = CW_FAILURE_WTP_NOT_CONNECTED;
					SendBEResponseDirectly(BE_PORTAL_EVENT_RESPONSE,beHeader.apMac,socketIndex,rc);
				}
				
				CWVendorPortalValues bePortalEventRequest;

				if ( (n = Readn(sock, &(bePortalEventRequest.TotalFileNum), sizeof(short)) )< 0 ) {
						CWLog("Error while reading from socket.");
						goto quit_manage;
				}
				
				bePortalEventRequest.TotalFileNum = ntohs(bePortalEventRequest.TotalFileNum);
				if(!(bePortalEventRequest.TotalFileNum))
				{
					CWLog("Error TotalFileNum = %d not in range !",(bePortalEventRequest.TotalFileNum));
					goto quit_manage;
				}	
				CWLog("Receive TotalFileNum = %d",(bePortalEventRequest.TotalFileNum));

				if ( (n = Readn(sock, &(bePortalEventRequest.FileNo), sizeof(short)) )< 0 ) {
						CWLog("Error while reading from socket.");
						goto quit_manage;
				}
				
				bePortalEventRequest.FileNo = ntohs(bePortalEventRequest.FileNo);
				
				if(!(bePortalEventRequest.FileNo) ||(bePortalEventRequest.FileNo) >  (bePortalEventRequest.TotalFileNum))
				{
					CWLog("Error FileNo = %d not in range !",(bePortalEventRequest.FileNo));
					goto quit_manage;
				}	
				CWLog("Receive FileNo = %d",(bePortalEventRequest.FileNo));
				
				//fileName
				if ( (n = Readn(sock, &(bePortalEventRequest.EncodeNameLen), sizeof(short)) )< 0 ) {
						CWLog("Error while reading from socket.");
						goto quit_manage;
				}

				bePortalEventRequest.EncodeNameLen = ntohs(bePortalEventRequest.EncodeNameLen);
				
				if(!(bePortalEventRequest.EncodeNameLen) )
				{
					CWLog("Error EncodeNameLen = %d not in range !",(bePortalEventRequest.EncodeNameLen));
					goto quit_manage;
				}	
				CWLog("Receive EncodeNameLen = %d",(bePortalEventRequest.EncodeNameLen));
	
				CW_CREATE_STRING_ERR((bePortalEventRequest.EncodeName), (bePortalEventRequest.EncodeNameLen)+1, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});
				memset((bePortalEventRequest.EncodeName),0,(bePortalEventRequest.EncodeNameLen)+1);
				
				if ( (n = Readn(sock, (bePortalEventRequest.EncodeName), (bePortalEventRequest.EncodeNameLen)) )< 0 ) {
						CWLog("Error while reading from socket.");
						goto quit_manage;
				}
				
				CWLog("Receive EncodeName = %s",(bePortalEventRequest.EncodeName));


				//fileContent
				if ( (n = Readn(sock, &(bePortalEventRequest.EncodeContentLen), sizeof(int)) )< 0 ) {
						CWLog("Error while reading from socket.");
						goto quit_manage;
				}
				bePortalEventRequest.EncodeContentLen = Swap32(bePortalEventRequest.EncodeContentLen);
				
				if(!(bePortalEventRequest.EncodeContentLen) )
				{
					CWLog("Error EncodeContentLen = %d not in range !",(bePortalEventRequest.EncodeContentLen));
					goto quit_manage;
				}	
				CWLog("Receive EncodeContentLen = %d",(bePortalEventRequest.EncodeContentLen));
	
				CW_CREATE_STRING_ERR((bePortalEventRequest.EncodeContent), (bePortalEventRequest.EncodeContentLen)+1, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});
				memset((bePortalEventRequest.EncodeContent),0,(bePortalEventRequest.EncodeContentLen)+1);
				
				if ( (n = Readn(sock, (bePortalEventRequest.EncodeContent), (bePortalEventRequest.EncodeContentLen)) )< 0 ) {
						CWLog("Error while reading from socket.");
						goto quit_manage;
				}
				
				//CWLog("Receive EncodeContent = %s",(bePortalEventRequest.EncodeContent));

				result = FALSE;
				result = BESetPortalValues(beHeader.apMac, socketIndex, &bePortalEventRequest);
				CW_FREE_OBJECT(bePortalEventRequest.EncodeName);
				CW_FREE_OBJECT(bePortalEventRequest.EncodeContent);
				if(!result)
				{
					SendBEResponseDirectly(BE_PORTAL_EVENT_RESPONSE,beHeader.apMac,socketIndex,rc);
				}
				goto quit_manage;
			}
			
			//Alarm
			if( beType == BE_WTP_EVENT_RESPONSE)
			{
				CWLog("Receive BE_WTP_EVENT_RESPONSE !");
				//len
				if ( (n = Readn(sock, &beLen, BE_LENGTH_LEN) )< 0 ) {
						CWLog("Error while reading from socket.");
						goto quit_manage;
				}
				beLen = Swap32(beLen);
				if(beLen != sizeof(CWResultCode))
				{
					CWLog("Error beLen = %d, ! = sizeof(CWResultCode) not in range !",beLen);
					goto quit_manage;
				}	
				CWLog("Receive beLen = %d",beLen);

				//ResultCode
				if ( (n = Readn(sock, &resultCode, beLen) )< 0 ) {
						CWLog("Error while reading from socket.");
						goto quit_manage;
				}
				resultCode = Swap32(resultCode);
				if(!resultCode)
				{
					CWLog("resultCode = %d,ERROR !",resultCode);
					goto quit_manage;
				}	
				CWLog("Receive resultCode = %d",resultCode);
				
				goto quit_manage;

			}

			//System
			if( beType == BE_SYSTEM_EVENT_REQUEST)
			{
				CWLog("Receive BE_SYSTEM_EVENT_REQUEST !");
				//len
				if ( (n = Readn(sock, &beLen, BE_LENGTH_LEN) )< 0 ) {
						CWLog("Error while reading from socket.");
						goto quit_manage;
				}
				beLen = Swap32(beLen);
				if(beLen != sizeof(char))
				{
					CWLog("Error beLen = %d,!=sizeof(char) not in range !",beLen);
					goto quit_manage;
				}	
				CWLog("Receive beLen = %d",beLen);

				if(finded < 0)
				{
					CWLog("[F:%s, L:%d] can't find this ap",__FILE__,__LINE__);
					rc = CW_FAILURE_WTP_NOT_CONNECTED;
					SendBEResponseDirectly(BE_SYSTEM_EVENT_RESPONSE,beHeader.apMac,socketIndex,rc);
					goto quit_manage;
				}

				//Operation
				if ( (n = Readn(sock, &operation, beLen) )< 0 ) {
						CWLog("Error while reading from socket.");
						goto quit_manage;
				}

				CWLog("Receive operation = %d",operation);

				result = FALSE;
				result = BESetSysValues(beHeader.apMac, socketIndex, (SystemCode)operation);
				if(!result)
				{
					SendBEResponseDirectly(BE_SYSTEM_EVENT_RESPONSE,beHeader.apMac,socketIndex,rc);
				}
				
				goto quit_manage;
			}

			else
			{
				CWLog("BE body type = %d, unknown!",beType);
				goto quit_manage;
			}
		}
		else
		{
			CWLog("BE type exist,but no body !");
			goto quit_manage;
		}
		//don't keep long time
		goto quit_manage;
	}
	
quit_manage:

	if(!CWErr(CWThreadMutexLock(&appsManager.numSocketFreeMutex))) {
		CWLog("Error locking numSocketFree Mutex");
		return NULL;
	}
	
	appsManager.isFree[socketIndex] = CW_TRUE;
	appsManager.numSocketFree++;
	
       CWDestroyThreadMutex(&appsManager.socketMutex[socketIndex]);

	CWThreadMutexUnlock(&appsManager.numSocketFreeMutex); 
		
	close(sock);
	CWLog("Close AppThread ...");
	CWExitThread();
	return NULL;  
}

/****************************************************************************
 * CWInterface is the function that provide the interaction between AC and	*
 * extern applications. Listen on main Inet familty socket and create a		*
 * CWManageApplication thread for every client connected.					*
 ****************************************************************************/

CW_THREAD_RETURN_TYPE CWInterface(void* arg)
{
	 
	CWSocket listen_sock, conn_sock;
	struct sockaddr_in servaddr;
	CWInterfaceThreadArg *argPtr;
	CWThread thread_id;
	//int i, clientFull = htonl(FULL_CLIENT_CONNECTED), optValue = 1;
	int i, optValue = 1;
	
	/****************************************************
	 * Setup of Application Socket Management Structure	*
	 ****************************************************/
	 /* BE client */
	
	/*
	int clientSock;
	struct sockaddr_in clientAddr;
	char *clientAddress = gACBEServerAddr;
	int clientPort = gACBEServerPort;
	
	CWLog("clientSock ,addr = %s,port = %d...... ",clientAddress,clientPort);
	if ((clientSock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		CWLog("clientSock init error ");
		close(clientSock);
		return NULL;
	}

	bzero(&clientAddr, sizeof (struct sockaddr_in));
	clientAddr.sin_family = AF_INET;
	clientAddr.sin_port = htons(clientPort);
	inet_pton(AF_INET, clientAddress, &clientAddr.sin_addr);

	//flags = fcntl(sockfd,F_GETFL,0);
	//fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
	//fcntl(sockfd, F_SETFL, 0);


	
	if ((ret = connect(clientSock, (SA*) &clientAddr, sizeof(struct sockaddr_in)) )< 0) {
		CWLog("clientSock connect error,ret = %d",ret);
		//test
		close(clientSock);
		return NULL;
	}
		

	if ( !CWErr(CWCreateThreadMutex(&appsManager.appClientSocketMutex)) ) {
		CWLog("Error on mutex creation on appManager structure: appClientSocketMutex");
		return NULL;
	}

	if(!CWErr(CWThreadMutexLock(&appsManager.appClientSocketMutex))) {
				CWLog("Error locking numSocketFree Mutex");
				return NULL;
			}
	appsManager.appClientSocket = clientSock;
	bzero(&appsManager.appClientAddr, sizeof (struct sockaddr_in));
	memcpy(&appsManager.appClientAddr,&clientAddr,sizeof (struct sockaddr_in));

	CWThreadMutexUnlock(&appsManager.appClientSocketMutex);

	*/
	//
	for ( i=0; i < MAX_APPS_CONNECTED_TO_AC; i++) 
		appsManager.isFree[i] = CW_TRUE;	
	
	appsManager.numSocketFree = MAX_APPS_CONNECTED_TO_AC;
	
	if ( !CWErr(CWCreateThreadMutex(&appsManager.numSocketFreeMutex)) ) {
		CWLog("Error on mutex creation on appManager structure");
		//return NULL;
		goto Quit;
	}
		
	/****************************************************
	 * Setup (Creation and filling) of main socket		*
	 ****************************************************/
		
	if ( ( listen_sock = socket(AF_INET, SOCK_STREAM, 0 ) ) < 0 ) {
		CWLog("Error on socket creation on Interface");
		//return NULL;
		goto Quit;
	}
	
	memset(&servaddr, 0, sizeof(servaddr));
	
	servaddr.sin_family = AF_INET;
	//not same as AC
//servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* Not Extern: INADDR_ANY */
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Not Extern: INADDR_ANY */
	servaddr.sin_port = htons(LISTEN_PORT); 

	if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &optValue, sizeof(int)) == -1) {
		CWLog("Error on socket creation on Interface");
		close(listen_sock);
		goto Quit;
	}
	
	/************************************
	 * Binding socket and Listen call	*
	 ************************************/
	
	if (  bind(listen_sock, (struct sockaddr *) &servaddr, sizeof(struct sockaddr_in)) < 0 ) {
		CWLog("Error on Binding");
		close(listen_sock);
		goto Quit;
	}
	
	if ( listen(listen_sock, MAX_APPS_CONNECTED_TO_AC) < 0 ) {
		CWLog("Error on LIsTsocket creation");
		close(listen_sock);
		goto Quit;
	}

	
	/********************************
	 *			Main Loop			*
	 ********************************/
	
	
	CW_REPEAT_FOREVER
      {
		if ( ( conn_sock = accept(listen_sock, (struct sockaddr *) NULL, NULL) ) > 0 ) { 
			
			/************************************************************************	
			 * Check (with lock) the number of socket free at the moment of accept,	*
			 * if this value is greater than 0 will be spawn a new Manage thread.	*
			 ************************************************************************/
			
			
			if ( appsManager.numSocketFree > 0 ) { 
				
				CW_CREATE_OBJECT_ERR(argPtr, CWInterfaceThreadArg, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});
				
				/************************************
				 *	Search socket for application	*
				 ************************************/
					
				for (i=0; i < MAX_APPS_CONNECTED_TO_AC; i++) 
	                   {                  
					if ( appsManager.isFree[i] == CW_TRUE ) {
						argPtr->index = i;
						appsManager.isFree[i] = CW_FALSE;
						appsManager.appSocket[i] = conn_sock;				  
						break;
					}
	                    }
				if(i >= MAX_APPS_CONNECTED_TO_AC)
				{
					CWLog("%s  %d be max request > %d, refuse !"__FILE__,__LINE__,MAX_APPS_CONNECTED_TO_AC);
					sleep(2);
					continue;
				}
				if(!CWErr(CWThreadMutexLock(&appsManager.numSocketFreeMutex))) {
					CWLog("Error locking numSocketFree Mutex");
					close(listen_sock);
					goto Quit;
				}	
				appsManager.numSocketFree--;
				CWThreadMutexUnlock(&appsManager.numSocketFreeMutex);
				
				if ( !CWErr(CWCreateThreadMutex(&appsManager.socketMutex[argPtr->index])) ) {
	              			CWLog("Error on mutex creation on appManager structure");
	              			goto QuitApp;
				}
							
				if(!CWErr(CWCreateThread(&thread_id, CWManageApplication, argPtr))) {
					CWLog("Error on thread CWManageApplication creation");
					appsManager.isFree[argPtr->index] = CW_TRUE;
					close(conn_sock);
					CW_FREE_OBJECT(argPtr);
	                                    /* 
	                                     * If we can't create the thread, we have to increment numSocketFree.
	                                     *
	                                     *   ref -> BUG-LE01
	                                     *   15/10/2009 - Donato Capitella
	                                     */
	                                    if(!CWErr(CWThreadMutexLock(&appsManager.numSocketFreeMutex))) {
	                                            CWLog("Error locking numSocketFree Mutex");
	                                            goto QuitApp;
	                                    }
	                                    appsManager.numSocketFree++;
	                                    CWThreadMutexUnlock(&appsManager.numSocketFreeMutex);

				}
			}
			else {
				//CWThreadMutexUnlock(&appsManager.numSocketFreeMutex);
			
				/****************************************************************
				 *	There isn't space for another client, send error answer.	*
				 ***************************************************************/
				
				/*if ( Writen(conn_sock, &clientFull, sizeof(int) ) < 0 ) {
					printf("Error on sending Error Message\n");
					close(conn_sock);
				}*/
			}		  
		}
		else
			CWLog("Error on accept (applications) system call !");
      }
	
	//CWDestroyThreadMutex(&appsManager.numSocketFreeMutex);
	//CWDestroyThreadMutex(&appsManager.appClientSocketMutex);
QuitApp:
	CWDestroyThreadMutex(&appsManager.numSocketFreeMutex);
	close(listen_sock);
Quit:
	CWExitThread();
	return NULL;
}


int is_valid_wtp_index(int wtpIndex) 
{
	if (wtpIndex < CW_MAX_WTP && 
	    wtpIndex >= 0)
		return CW_TRUE;
	return CW_FALSE;
}

/*
 * Steven's readn().
 */
int readn(int fd, void *vptr, size_t n)
{
	size_t	nleft;
	ssize_t	nread;
	char	*ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nread = recv(fd, ptr, nleft, 0)) < 0) {
			if (errno == EINTR)
				nread = 0;		/* and call read() again */
			else
				return(-1);
		} else if (nread == 0)
			break;				/* EOF */

		nleft -= nread;
		ptr   += nread;
	}
	return(n - nleft);		/* return >= 0 */
}

int Readn(int fd, void *ptr, size_t nbytes)
{
	int n;

	if ( (n = readn(fd, ptr, nbytes)) < 0) {
		CWLog("Error while reading data from socket.");
		return -1;
	}

	return n;
}
			
