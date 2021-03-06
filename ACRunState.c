/************************************************************************************************
 * Copyright (c) 2006-2009 Laboratorio di Sistemi di Elaborazione e Bioingegneria Informatica	*
 *                          Universita' Campus BioMedico - Italy								*
 *																								*
 * This program is free software; you can redistribute it and/or modify it under the terms		*
 * of the GNU General Public License as published by the Free Software Foundation; either		*
 * version 2 of the License, or (at your option) any later version.								*
 *																								*
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY				*
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A				*
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.						*
 *																								*
 * You should have received a copy of the GNU General Public License along with this			*
 * program; if not, write to the:																*
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,							*
 * MA  02111-1307, USA.																			*
 *																								*
 * -------------------------------------------------------------------------------------------- *
 * Project:  Capwap																				*
 *																								*
 * Authors : Ludovico Rossi (ludo@bluepixysw.com)												*  
 *           Del Moro Andrea (andrea_delmoro@libero.it)											*
 *           Giovannini Federica (giovannini.federica@gmail.com)								*
 *           Massimo Vellucci (m.vellucci@unicampus.it)											*
 *           Mauro Bisson (mauro.bis@gmail.com)													*
 *	         Antonio Davoli (antonio.davoli@gmail.com)											*
 ************************************************************************************************/

#include "CWAC.h"
#include "CWVendorPayloads.h"
#include "CWFreqPayloads.h"
#include "WUM.h"
#include "BELib.h"

#ifdef DMALLOC
#include "../dmalloc-5.5.0/dmalloc.h"
#endif

CWBool CWACParseGenericRunMessage(int WTPIndex,
				  CWProtocolMessage *msg,
				  CWControlHeaderValues* controlVal);

CWBool CWParseConfigurationUpdateResponseMessage(CWProtocolMessage* msgPtr,
						 int len,
						 CWProtocolResultCode* resultCode,
						 CWProtocolVendorSpecificValues** protocolValues);

CWBool CWSaveConfigurationUpdateResponseMessage(CWProtocolResultCode resultCode,
						int WTPIndex,
						CWProtocolVendorSpecificValues* protocolValues);

CWBool CWParseClearConfigurationResponseMessage(CWProtocolMessage* msgPtr,
						int len,
						CWProtocolResultCode* resultCode);
CWBool CWParseResetResponseMessage(CWProtocolMessage* msgPtr,
						int len,
						CWProtocolResultCode* resultCode);

CWBool CWParseStationConfigurationResponseMessage(CWProtocolMessage* msgPtr,
						  int len,
						  CWProtocolResultCode* resultCode);

CWBool CWParseWTPDataTransferRequestMessage(CWProtocolMessage *msgPtr,
					    int len,
					    CWProtocolWTPDataTransferRequestValues *valuesPtr);

CWBool CWAssembleWTPDataTransferResponse(CWProtocolMessage **messagesPtr,
					 int *fragmentsNumPtr,
					 int PMTU, int seqNum);

CWBool CWParseWTPEventRequestMessage(CWProtocolMessage *msgPtr,
				     int len,
				     CWProtocolWTPEventRequestValues *valuesPtr);

CWBool CWSaveWTPEventRequestMessage(CWProtocolWTPEventRequestValues *WTPEventRequest,
				    CWWTPProtocolManager *WTPProtocolManager);

CWBool CWAssembleWTPEventResponse(CWProtocolMessage **messagesPtr,
				  int *fragmentsNumPtr,
				  int PMTU,
				  int seqNum);

CWBool CWParseChangeStateEventRequestMessage2(CWProtocolMessage *msgPtr,
					      int len,
					      CWProtocolChangeStateEventRequestValues **valuesPtr);

CWBool CWParseEchoRequestMessage(CWProtocolMessage *msgPtr,
				 int len);

CWBool CWAssembleEchoResponse(CWProtocolMessage **messagesPtr,
			      int *fragmentsNumPtr,
			      int PMTU,
			      int seqNum);

CWBool CWStartNeighborDeadTimer(int WTPIndex);
CWBool CWStopNeighborDeadTimer(int WTPIndex);
CWBool CWRestartNeighborDeadTimer(int WTPIndex);
CWBool CWRestartNeighborDeadTimerForEcho(int WTPIndex);
CWBool CWStartNeighborDeadTimerForEcho(int WTPIndex);

CWBool ACEnterRun(int WTPIndex, CWProtocolMessage *msgPtr, CWBool dataFlag) {

	CWBool toSend= CW_FALSE, timerSet = CW_TRUE;
	CWControlHeaderValues controlVal;
	CWProtocolMessage* messages =NULL;
	int messagesCount=0;
	unsigned char StationMacAddr[MAC_ADDR_LEN];
	char string[10];
	char socketctl_path_name[50];
	char socketserv_path_name[50];

	int BESize = 0;
	char * beResp = NULL;
	BEsystemEventResponse beSysEventResp;
	BEwtpEventRequest        beWtpEventReq;
	msgPtr->offset = 0;
	
	// cancel NeighborDeadTimer timer
	//CWStopNeighborDeadTimer(WTPIndex);
	timerSet = CW_TRUE;

	if(dataFlag){
		/* We have received a Data Message... now just log this event and do actions by the dataType */
		CWLog("--> Received a DATA Message");

		if(msgPtr->data_msgType == CW_DATA_MSG_FRAME_TYPE)
		{	
		/*Retrive mac address station from msg*/
		memset(StationMacAddr, 0, MAC_ADDR_LEN);
		memcpy(StationMacAddr, msgPtr->msg+SOURCE_ADDR_START, MAC_ADDR_LEN);
	
		int seqNum = CWGetSeqNum();

		//Send a Station Configuration Request
			if (CWAssembleStationConfigurationRequest(&(gWTPs[WTPIndex].messages),
							  &(gWTPs[WTPIndex].messagesCount),
							  gWTPs[WTPIndex].pathMTU,
							  seqNum,StationMacAddr)) {

			if(CWACSendAcknowledgedPacket(WTPIndex, 
						      CW_MSG_TYPE_VALUE_STATION_CONFIGURATION_RESPONSE,
						      seqNum)) 
				return CW_TRUE;
			else
				CWACStopRetransmission(WTPIndex);
			}
		}
		else {
			
			/************************************************************
			 * Update 2009:												*
			 *				Manage special data packets with frequency	*
			 *				statistics informations.					*
			 ************************************************************/
			
			if( msgPtr->data_msgType == CW_DATA_MSG_FREQ_STATS_TYPE ) {
				
				int cells; /* How many cell are heard */
                int isAck;
				char * freqPayload; 
				int socketIndex, indexToSend = htonl(WTPIndex);
				
				int sizeofAckInfoUnit = CW_FREQ_ACK_SIZE;
				int sizeofFreqInfoUnit = CW_FREQ_CELL_INFO_PAYLOAD_SIZE;
				int sizeOfPayload = 0, payload_offset = 0;
				
				/*-----------------------------------------------------------------------------------------------
				 *	Payload Management ( infos for frequency application) :
				 *		Ack       Structure : |  WTPIndex  |   Ack Value  | 
				 *      Freq Info Structure : |  WTPIndex  |  Number of cells  |  Frequecies Info Payload | 
				 *-----------------------------------------------------------------------------------------------*/
				
                memcpy(&isAck, msgPtr->msg, sizeof(int));

				isAck = ntohl(isAck);
				
                if ( isAck == 0 ) { /* isnt an ack message */
					memcpy(&cells, msgPtr->msg+sizeof(int), sizeof(int));
					cells = ntohl(cells);
					sizeOfPayload = ( cells * sizeofFreqInfoUnit) + (2*sizeof(int)); 
				}
				else {
					sizeOfPayload = sizeofAckInfoUnit;
				}
				
                if ( ( freqPayload = malloc(sizeOfPayload) ) != NULL ) {
					
					memset(freqPayload, 0, sizeOfPayload);
					memcpy(freqPayload, &indexToSend, sizeof(int));
					payload_offset += sizeof(int);
					
					if ( isAck == 0 ) {
						memcpy(freqPayload+payload_offset, msgPtr->msg+sizeof(int), sizeOfPayload-payload_offset);
					}
					else {
						memcpy(freqPayload+payload_offset, msgPtr->msg+sizeof(int), sizeOfPayload-payload_offset);
					}
					
					socketIndex = gWTPs[WTPIndex].applicationIndex;	
					
					/****************************************************
					 *		Forward payload to correct application 		*
					 ****************************************************/
					
					if(!CWErr(CWThreadMutexLock(&appsManager.socketMutex[socketIndex]))) {
						CWLog("[ACrunState]:: Error locking socket Application Mutex");
						free(freqPayload);
						return CW_FALSE;
					}
					
					if ( Writen(appsManager.appSocket[socketIndex], freqPayload, sizeOfPayload)  < 0 ) {
                      CWThreadMutexUnlock(&appsManager.socketMutex[socketIndex]);
                      free(freqPayload);
                      CWLog("[ACrunState]:: Error writing Message To Application");
                      return CW_FALSE;
					}
					
					CWThreadMutexUnlock(&appsManager.socketMutex[socketIndex]);
					free(freqPayload);
				}
				else
					 CWLog("[ACrunState]:: Malloc error (payload to frequency application");
			}
			

		  if(msgPtr->data_msgType == CW_DATA_MSG_STATS_TYPE)
			{			  
			  if(!UnixSocksArray[WTPIndex].data_stats_sock)
				{	//Init Socket only the first time when the function is called
				  if ((UnixSocksArray[WTPIndex].data_stats_sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) 
					{
					  CWDebugLog("Error creating socket for data send");
					  return CW_FALSE;
    				}
				  
				  memset(&(UnixSocksArray[WTPIndex].clntaddr),(int)0, sizeof(UnixSocksArray[WTPIndex].clntaddr));
				  UnixSocksArray[WTPIndex].clntaddr.sun_family = AF_UNIX;
				  
				  //make unix socket client path name by index i 
				snprintf(string,sizeof(string),"%d",WTPIndex);
				string[sizeof(string)-1]=0;
				strcpy(socketctl_path_name,SOCKET_PATH_AC);
				strcat(socketctl_path_name,string);
				strcpy(UnixSocksArray[WTPIndex].clntaddr.sun_path,socketctl_path_name);
				
				unlink(socketctl_path_name);
				
				memset(&(UnixSocksArray[WTPIndex].servaddr),(int)0, sizeof(UnixSocksArray[WTPIndex].servaddr));
				UnixSocksArray[WTPIndex].servaddr.sun_family = AF_UNIX;

				//make unix socket server path name by index i 
				strcpy(socketserv_path_name, SOCKET_PATH_RECV_AGENT);
				strcat(socketserv_path_name, string);
				strcpy(UnixSocksArray[WTPIndex].servaddr.sun_path, socketserv_path_name);
				printf("\n%s\t%s",socketserv_path_name,socketctl_path_name);fflush(stdout);
				}
			  

			  int nbytes;
			  int pDataLen=656; //len of Monitoring Data
			  
			  //Send data stats from AC thread to monitor client over unix socket
			  nbytes = sendto(UnixSocksArray[WTPIndex].data_stats_sock, msgPtr->msg,pDataLen, 0,
							  (struct sockaddr *) &(UnixSocksArray[WTPIndex].servaddr),sizeof(UnixSocksArray[WTPIndex].servaddr));
			  if (nbytes < 0) 
				{
				  CWDebugLog("Error sending data over socket");
				  return CW_FALSE;
				}
			
			}
		}

		return CW_TRUE;
	}

	if(!(CWACParseGenericRunMessage(WTPIndex, msgPtr, &controlVal))) {
		/* Two possible errors: WRONG_ARG and INVALID_FORMAT
		 * In the second case we have an unexpected response: ignore the
		 * message and log the event.
		 */
		return CW_FALSE;
	}

	switch(controlVal.messageTypeValue) {
		case CW_MSG_TYPE_VALUE_CONFIGURE_UPDATE_RESPONSE:
		{
			CWProtocolResultCode resultCode;
			/*Update 2009:
				Store Protocol specific response data*/
			CWProtocolVendorSpecificValues* protocolValues = NULL;
			
			if(!(CWParseConfigurationUpdateResponseMessage(msgPtr, controlVal.msgElemsLen, &resultCode, &protocolValues)))
				return CW_FALSE;
			
			CWACStopRetransmission(WTPIndex);
			
			if (timerSet) {
				if(!CWRestartNeighborDeadTimer(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			} else {
				if(!CWStartNeighborDeadTimer(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			}

			CWSaveConfigurationUpdateResponseMessage(resultCode, WTPIndex, protocolValues);
			if (gWTPs[WTPIndex].interfaceCommandProgress == CW_TRUE) {

				//CWThreadMutexLock(&gWTPs[WTPIndex].interfaceMutex);
				if(gWTPs[WTPIndex].interfaceResult != UPGRADE_FAILED && gWTPs[WTPIndex].interfaceResult != CW_FAILURE_WTP_UPGRADING_REJECT_NWEUPGRADE)
				{
					gWTPs[WTPIndex].interfaceResult = CW_TRUE;
				}
				gWTPs[WTPIndex].interfaceCommandProgress = CW_FALSE;
				
				CWLog("[F:%s L:%d] gWTPs[WTPIndex].interfaceResult =%d",__FILE__,__LINE__,gWTPs[WTPIndex].interfaceResult);
				if(!CWErr(CWThreadMutexLock(&gWTPs[WTPIndex].interfaceMutex)))
				{
					CWLog("F:%s L:%d [ACrunState]:Error locking gWTPs[WTPIndex].interfaceMutex",__FILE__,__LINE__);
					return CW_FALSE;
				}
				CWSignalThreadCondition(&gWTPs[WTPIndex].interfaceComplete);
				CWLog("[F:%s L:%d] CWSignalThreadCondition(&gWTPs[WTPIndex].interfaceComplete)",__FILE__,__LINE__);

				CWThreadMutexUnlock(&gWTPs[WTPIndex].interfaceMutex);
			}

			break;
		}
		case CW_MSG_TYPE_VALUE_CHANGE_STATE_EVENT_REQUEST:
		{
			CWProtocolChangeStateEventRequestValues *valuesPtr;
		
			if(!(CWParseChangeStateEventRequestMessage2(msgPtr, controlVal.msgElemsLen, &valuesPtr)))
				return CW_FALSE;
			if (timerSet) {
				if(!CWRestartNeighborDeadTimer(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			} else {
				if(!CWStartNeighborDeadTimer(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			}
			if(!(CWSaveChangeStateEventRequestMessage(valuesPtr, &(gWTPs[WTPIndex].WTPProtocolManager))))
				return CW_FALSE;
			if(!(CWAssembleChangeStateEventResponse(&messages,
								&messagesCount,
								gWTPs[WTPIndex].pathMTU,
								controlVal.seqNum)))
				return CW_FALSE;

			toSend = CW_TRUE;
			break;
		}
		case CW_MSG_TYPE_VALUE_ECHO_REQUEST:
		{
			if(!(CWParseEchoRequestMessage(msgPtr, controlVal.msgElemsLen)))
				return CW_FALSE;
			if (timerSet) {
				if(!CWRestartNeighborDeadTimerForEcho(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			} else {
				if(!CWStartNeighborDeadTimerForEcho(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			}
			
			if(!(CWAssembleEchoResponse(&messages,
						    &messagesCount,
						    gWTPs[WTPIndex].pathMTU,
						    controlVal.seqNum)))
				return CW_FALSE;

			toSend = CW_TRUE;	
			break;
		}
		case CW_MSG_TYPE_VALUE_STATION_CONFIGURATION_RESPONSE:
		{
			CWProtocolResultCode resultCode;
			if(!(CWParseStationConfigurationResponseMessage(msgPtr, controlVal.msgElemsLen, &resultCode)))
				return CW_FALSE;
			CWACStopRetransmission(WTPIndex);
			if (timerSet) {
				if(!CWRestartNeighborDeadTimer(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			} else {
				if(!CWStartNeighborDeadTimer(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			}
			//CWSaveStationConfigurationResponseMessage(resultCode, WTPIndex);  <-- Must be Implemented ????

			break;
		}	
		//Reset later, DB need update,so Monitor Request
		case CW_MSG_TYPE_VALUE_CLEAR_CONFIGURATION_RESPONSE:
		{
			CWProtocolResultCode resultCode;
			if(!(CWParseClearConfigurationResponseMessage(msgPtr, controlVal.msgElemsLen, &resultCode)))
				return CW_FALSE;
			CWACStopRetransmission(WTPIndex);
			if (timerSet) {
				if(!CWRestartNeighborDeadTimer(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			} else {
				if(!CWStartNeighborDeadTimer(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			}
			
			//BE 
		
				
				beSysEventResp.type =htons( BE_SYSTEM_EVENT_RESPONSE) ;
				beSysEventResp.length = Swap32(sizeof(SystemCode));
				beSysEventResp.resultCode = Swap32(resultCode);
					
				BESize = BE_TYPELEN_LEN+BE_CODE_LEN;

				beResp = AssembleBEheader((char*)&beSysEventResp,&BESize,WTPIndex,NULL);

				if(beResp)
				{
					//SendBERequest(beResp,BESize);
					SendBEResponse(beResp,BESize,WTPIndex);
					CW_FREE_OBJECT(beResp);
				}
				else
				{
					CWLog("Error AssembleBEheader !");
				}	
				
			
			if (gWTPs[WTPIndex].interfaceCommandProgress == CW_TRUE)
			{
				//CWThreadMutexLock(&gWTPs[WTPIndex].interfaceMutex);
				
				gWTPs[WTPIndex].interfaceResult = CW_TRUE;
				gWTPs[WTPIndex].interfaceCommandProgress = CW_FALSE;
				
				if(!CWErr(CWThreadMutexLock(&gWTPs[WTPIndex].interfaceMutex))) 
				{
					CWLog("F:%s L:%d [ACrunState]:Error locking gWTPs[WTPIndex].interfaceMutex !",__FILE__,__LINE__);
					return CW_FALSE;
				}
				CWSignalThreadCondition(&gWTPs[WTPIndex].interfaceComplete);

				CWThreadMutexUnlock(&gWTPs[WTPIndex].interfaceMutex);
			}


			break;
		}
		//Reboot 
		case CW_MSG_TYPE_VALUE_RESET_RESPONSE:
		{
			CWProtocolResultCode resultCode;
			if(!(CWParseResetResponseMessage(msgPtr, controlVal.msgElemsLen, &resultCode)))
				return CW_FALSE;
			CWACStopRetransmission(WTPIndex);
			if (timerSet) {
				if(!CWRestartNeighborDeadTimer(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			} else {
				if(!CWStartNeighborDeadTimer(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			}

			//BE 
			
				beSysEventResp.type =htons( BE_SYSTEM_EVENT_RESPONSE) ;
				beSysEventResp.length = Swap32(sizeof(SystemCode));
				beSysEventResp.resultCode = Swap32(resultCode);
					
				BESize = BE_TYPELEN_LEN+BE_CODE_LEN;

				beResp = AssembleBEheader((char*)&beSysEventResp,&BESize,WTPIndex,NULL);

				if(beResp)
				{
					//SendBERequest(beResp,BESize);
					SendBEResponse(beResp,BESize,WTPIndex);
					CW_FREE_OBJECT(beResp);
				}
				else
				{
					CWLog("Error AssembleBEheader !");
				}	
			
			
			if (gWTPs[WTPIndex].interfaceCommandProgress == CW_TRUE)
			{
				//CWThreadMutexLock(&gWTPs[WTPIndex].interfaceMutex);


				gWTPs[WTPIndex].interfaceResult = CW_TRUE;

				gWTPs[WTPIndex].interfaceCommandProgress = CW_FALSE;
				
				if(!CWErr(CWThreadMutexLock(&gWTPs[WTPIndex].interfaceMutex))) 
				{
					CWLog("F:%s L:%d [ACrunState]:Error locking gWTPs[WTPIndex].interfaceMutex !",__FILE__,__LINE__);
					return CW_FALSE;
				}
				CWSignalThreadCondition(&gWTPs[WTPIndex].interfaceComplete);

				CWThreadMutexUnlock(&gWTPs[WTPIndex].interfaceMutex);
			}

			

			break;
		}		
		case CW_MSG_TYPE_VALUE_DATA_TRANSFER_REQUEST:
		{
			CWProtocolWTPDataTransferRequestValues valuesPtr;
			
			if(!(CWParseWTPDataTransferRequestMessage(msgPtr, controlVal.msgElemsLen, &valuesPtr)))
				return CW_FALSE;
			if (timerSet) {
				if(!CWRestartNeighborDeadTimer(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			} else {
				if(!CWStartNeighborDeadTimer(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			}
			if(!(CWAssembleWTPDataTransferResponse(&messages, &messagesCount, gWTPs[WTPIndex].pathMTU, controlVal.seqNum))) 
				return CW_FALSE;
			toSend = CW_TRUE;
			break;
		}
		case CW_MSG_TYPE_VALUE_WTP_EVENT_REQUEST:
		{
			CWProtocolWTPEventRequestValues valuesPtr;

			if(!(CWParseWTPEventRequestMessage(msgPtr, controlVal.msgElemsLen, &valuesPtr)))
				return CW_FALSE;
			if (timerSet) {
				if(!CWRestartNeighborDeadTimer(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			} else {
				if(!CWStartNeighborDeadTimer(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			}

			if(!(CWSaveWTPEventRequestMessage(&valuesPtr, &(gWTPs[WTPIndex].WTPProtocolManager))))
				return CW_FALSE;
			
			if(!(CWAssembleWTPEventResponse(&messages,
							&messagesCount,
							gWTPs[WTPIndex].pathMTU,
							controlVal.seqNum)))
 				return CW_FALSE;

			toSend = CW_TRUE;	
			//BE 
			
			beWtpEventReq.type =htons( BE_WTP_EVENT_REQUEST) ;
			beWtpEventReq.length = Swap32(gWTPs[WTPIndex].WTPProtocolManager.WTPVendorPayload->vendorPayloadLen);
			beWtpEventReq.xml = (char*)gWTPs[WTPIndex].WTPProtocolManager.WTPVendorPayload->payload;
				
			BESize = BE_TYPELEN_LEN+gWTPs[WTPIndex].WTPProtocolManager.WTPVendorPayload->vendorPayloadLen;

			beResp = AssembleBEheader((char*)&beWtpEventReq,&BESize,WTPIndex,(char*)gWTPs[WTPIndex].WTPProtocolManager.WTPVendorPayload->payload);

			if(beResp)
			{
				//SendBERequest(beResp,BESize);
				SendBERequest(beResp,BESize);
				CW_FREE_OBJECT(beResp);
			}
			else
			{
				CWLog("Error AssembleBEheader !");
			}	
			
			break;
		}
		default: 
			/*
			 * We have an unexpected request and we have to send
			 * a corresponding response containing a failure result code
			 */
			CWLog("--> Not valid Request in Run State... we send a failure Response");
			if (timerSet) {
				if(!CWRestartNeighborDeadTimer(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			} else {
				if(!CWStartNeighborDeadTimer(WTPIndex)) {
					//CWCloseThread();
					CWLog("%s %d [%d] CWRestartNeighborDeadTimer Fail, close thread!",__FILE__,__LINE__,WTPIndex);
					gWTPs[WTPIndex].isRequestClose = CW_TRUE;
					return CW_FALSE;
				}
			}
			if(!(CWAssembleUnrecognizedMessageResponse(&messages,
								   &messagesCount,
								   gWTPs[WTPIndex].pathMTU,
								   controlVal.seqNum,
								   controlVal.messageTypeValue + 1))) 
 				return CW_FALSE;

			toSend = CW_TRUE;
			/*return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Message not valid in Run State");*/
	}	
	if(toSend){
		int i;
	
		if(messages == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
		
		for(i = 0; i < messagesCount; i++) {
#ifdef CW_NO_DTLS
			if(!CWNetworkSendUnsafeUnconnected(gWTPs[WTPIndex].socket, 
							   &gWTPs[WTPIndex].address, 
							   messages[i].msg, 
							   messages[i].offset)	) {
#else
			if(!(CWSecuritySend(gWTPs[WTPIndex].session,
					    messages[i].msg,
					    messages[i].offset))) {
#endif
				CWFreeMessageFragments(messages, messagesCount);
				CW_FREE_OBJECT(messages);
				return CW_FALSE;
			}
		}
		CWFreeMessageFragments(messages, messagesCount);
		CW_FREE_OBJECT(messages);
	}

	gWTPs[WTPIndex].currentState = CW_ENTER_RUN;
	gWTPs[WTPIndex].subState = CW_WAITING_REQUEST;

	return CW_TRUE;

}

CWBool CWACParseGenericRunMessage(int WTPIndex,
				  CWProtocolMessage *msg,
				  CWControlHeaderValues* controlVal) {

	if(msg == NULL || controlVal == NULL)
		return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	if(!(CWParseControlHeader(msg, controlVal)))
		/* will be handled by the caller */
		return CW_FALSE;

	/* skip timestamp */
	controlVal->msgElemsLen -= CW_CONTROL_HEADER_OFFSET_FOR_MSG_ELEMS;

	/* Check if it is a request */
	if(controlVal->messageTypeValue % 2 == 1){

		return CW_TRUE;	
	}

	if((gWTPs[WTPIndex].responseSeqNum != controlVal->seqNum) ||
	   (gWTPs[WTPIndex].responseType != controlVal->messageTypeValue)) {

		//CWDebugLog("[SeqNum]expect: %d != actual: %d\n", gWTPs[WTPIndex].responseSeqNum,controlVal->seqNum);
		//CWDebugLog("[MsgType]expect: %d != actual: %d\n", gWTPs[WTPIndex].responseType,controlVal->messageTypeValue);
		CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Seq Num or Msg Type not valid!");
		return CW_FALSE;
	}

	return CW_TRUE;	
}

/*Update 2009:
	Added vendValues to include a response payload (to pass response data)*/
CWBool CWParseConfigurationUpdateResponseMessage(CWProtocolMessage* msgPtr,
						 int len,
						 CWProtocolResultCode* resultCode,
						 CWProtocolVendorSpecificValues** vendValues) {

	int offsetTillMessages;

	if(msgPtr == NULL || resultCode==NULL)
		return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	if((msgPtr->msg) == NULL)
		return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	offsetTillMessages = msgPtr->offset;
	
	CWLog("Parsing Configuration Update Response...");

	/* parse message elements */
	while((msgPtr->offset - offsetTillMessages) < len) {

		unsigned short int elemType = 0;
		unsigned short int elemLen = 0;
		
		CWParseFormatMsgElem(msgPtr, &elemType, &elemLen);

		switch(elemType) {
			case CW_MSG_ELEMENT_RESULT_CODE_CW_TYPE:
				*resultCode=CWProtocolRetrieve32(msgPtr);
				break;	

			/*Update 2009:
				Added case to implement conf update response with payload*/
			case CW_MSG_ELEMENT_RESULT_CODE_CW_TYPE_WITH_PAYLOAD:
				{
				int payloadSize = 0;
				CW_CREATE_OBJECT_ERR(*vendValues, CWProtocolVendorSpecificValues, return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););

				*resultCode=CWProtocolRetrieve32(msgPtr);
				CWLog("[F:%s, L:%d] resultCode = %d",__FILE__,__LINE__,*resultCode);
				if (CWProtocolRetrieve16(msgPtr) != CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_CW_TYPE)
					/*For now, we only have UCI payloads, so we will accept only vendor payloads for protocol data*/
						return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Unrecognized Message Element in Configuration Update Response");

				//add Vendor Identifier

//				CWProtocolRetrieve16(msgPtr);
//				(*vendValues)->vendorId = CWProtocolRetrieve32(msgPtr);
//				CWLog("[F:%s, L:%d] (*vendValues)->vendorId = %d",__FILE__,__LINE__,(*vendValues)->vendorId);
				(*vendValues)->vendorPayloadType = CWProtocolRetrieve16(msgPtr);
				CWLog("[F:%s, L:%d] (*vendValues)->vendorPayloadType= %d",__FILE__,__LINE__,(*vendValues)->vendorPayloadType);
				//(*vendValues)->vendorPayloadLen = CWProtocolRetrieve16(msgPtr);

				switch ((*vendValues)->vendorPayloadType) {
					case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_UCI:
						payloadSize = CWProtocolRetrieve32(msgPtr);
						if (payloadSize != 0) {
							(*vendValues)->payload = (void *) CWProtocolRetrieveStr(msgPtr, payloadSize);
						} else 
							(*vendValues)->payload = NULL;
						break;
					case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_WUM:
						payloadSize = CWProtocolRetrieve32(msgPtr);
						
						if (payloadSize <= 0) {
							/* Payload can't be zero here,
							 * at least the message type must be specified */
							return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Unrecognized Message Element in Configuration Update Response");
						} 
						(*vendValues)->payload = (void *) CWProtocolRetrieveRawBytes(msgPtr, payloadSize);
						break;
					case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_CONFIG:
						//(*vendValues)->vendorPayloadLen = CWProtocolRetrieve16(msgPtr);
						(*vendValues)->vendorPayloadLen = CWProtocolRetrieve32(msgPtr);
						CWLog("[F:%s, L:%d] (*vendValues)->vendorPayloadLen = %d",__FILE__,__LINE__,(*vendValues)->vendorPayloadLen);
						if ((*vendValues)->vendorPayloadLen !=0) {

							return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Unrecognized Message Element in Configuration Update Response");
						}
						break;
					case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_STATE:
//						(*vendValues)->vendorPayloadLen = CWProtocolRetrieve16(msgPtr);
						(*vendValues)->vendorPayloadLen = CWProtocolRetrieve32(msgPtr);
						CWLog("[F:%s, L:%d] (*vendValues)->vendorPayloadLen = %d",__FILE__,__LINE__,(*vendValues)->vendorPayloadLen);
						if ((*vendValues)->vendorPayloadLen <= 0) {

							return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Unrecognized Message Element in Configuration Update Response");
						}
						(*vendValues)->payload = (void *) CWProtocolRetrieveRawBytes(msgPtr, (*vendValues)->vendorPayloadLen);
					   break;
				     case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_PORTAL:
						//(*vendValues)->vendorPayloadLen = CWProtocolRetrieve16(msgPtr);
						(*vendValues)->vendorPayloadLen = CWProtocolRetrieve32(msgPtr);
						CWLog("[F:%s, L:%d] (*vendValues)->vendorPayloadLen = %d",__FILE__,__LINE__,(*vendValues)->vendorPayloadLen);
						if ((*vendValues)->vendorPayloadLen !=0) {

							return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Unrecognized Message Element in Configuration Update Response");
						}
						break;
					default:
						return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Unrecognized Message Element in Configuration Update Response");
					break;	
				}
				}
				break;	
			default:
				return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Unrecognized Message Element in Configuration Update Response");
				break;	
		}
	}
	
	if((msgPtr->offset - offsetTillMessages) != len)
		return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Garbage at the End of the Message");

	CWLog("Configuration Update Response Parsed");

	return CW_TRUE;	
}

CWBool CWParseClearConfigurationResponseMessage(CWProtocolMessage* msgPtr, int len, CWProtocolResultCode* resultCode)
{
	int offsetTillMessages;

	if(msgPtr == NULL || resultCode==NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	if((msgPtr->msg) == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	offsetTillMessages = msgPtr->offset;
	
	CWLog("Parsing Clear Configuration Response...");

	// parse message elements
	while((msgPtr->offset - offsetTillMessages) < len) {
		unsigned short int elemType=0;
		unsigned short int elemLen=0;
		
		CWParseFormatMsgElem(msgPtr, &elemType, &elemLen);
		
		switch(elemType) {
			case CW_MSG_ELEMENT_RESULT_CODE_CW_TYPE:
				*resultCode=CWProtocolRetrieve32(msgPtr);
				break;	
			default:
				return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Unrecognized Message Element in Configuration Update Response");
				break;	
		}
	}
	
	if((msgPtr->offset - offsetTillMessages) != len) return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Garbage at the End of the Message");

	CWLog("Clear Configuration Response Parsed");

	return CW_TRUE;	
}	

CWBool CWParseResetResponseMessage(CWProtocolMessage * msgPtr,int len,CWProtocolResultCode * resultCode)
{
	int offsetTillMessages;

	if(msgPtr == NULL || resultCode==NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	if((msgPtr->msg) == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	offsetTillMessages = msgPtr->offset;
	
	CWLog("Parsing Reset Response...");

	// parse message elements
	while((msgPtr->offset - offsetTillMessages) < len) {
		unsigned short int elemType=0;
		unsigned short int elemLen=0;
		
		CWParseFormatMsgElem(msgPtr, &elemType, &elemLen);
		
		switch(elemType) {
			case CW_MSG_ELEMENT_RESULT_CODE_CW_TYPE:
				*resultCode=CWProtocolRetrieve32(msgPtr);
				break;	
			default:
				return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Unrecognized Message Element in Configuration Update Response");
				break;	
		}
	}
	
	if((msgPtr->offset - offsetTillMessages) != len) return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Garbage at the End of the Message");

	CWLog("Reset Response Parsed");

	return CW_TRUE;	
}	
		
CWBool CWParseStationConfigurationResponseMessage(CWProtocolMessage* msgPtr, int len, CWProtocolResultCode* resultCode)
{
	int offsetTillMessages;

	if(msgPtr == NULL || resultCode==NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	if((msgPtr->msg) == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	offsetTillMessages = msgPtr->offset;
	
	CWLog("Parsing Station Configuration Response...");

	// parse message elements
	while((msgPtr->offset - offsetTillMessages) < len) {
		unsigned short int elemType=0;
		unsigned short int elemLen=0;
		
		CWParseFormatMsgElem(msgPtr, &elemType, &elemLen);
		
		switch(elemType) {
			case CW_MSG_ELEMENT_RESULT_CODE_CW_TYPE:
				*resultCode=CWProtocolRetrieve32(msgPtr);
				break;	
			default:
				return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Unrecognized Message Element in Station Configuration Response");
				break;	
		}
	}
	
	if((msgPtr->offset - offsetTillMessages) != len) return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Garbage at the End of the Message");

	CWLog("Station Configuration Response Parsed");

	return CW_TRUE;	
}

CWBool CWSaveConfigurationUpdateResponseMessage(CWProtocolResultCode resultCode,
						int WTPIndex,
						CWProtocolVendorSpecificValues* vendValues) {
	char *wumPayloadBytes = NULL, *beResp = NULL;
	int closeWTPManager = CW_FALSE, result = CW_FALSE,BESize = 0;
	BEconfigEventResponse beConfigEventResp;
	BEmonitorEventResponse beMonitorEventResp;
	BEPortalEventResponse  bePortalEventResp;
	BEconnectEvent beConEve;

	if (vendValues != NULL) {
		//char * responseBuffer; 
		int  payloadSize;


		/********************************
		 *Payload Management		*
		 ********************************/

		//headerSize = 3*sizeof(int);
		//CWLog("[F:%s, L:%d] :headerSize = %d",__FILE__,__LINE__,headerSize);
		
		switch (vendValues->vendorPayloadType) {
		case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_UCI:
			if (vendValues->payload != NULL)
				payloadSize = strlen((char *) vendValues->payload);
			else
				payloadSize = 0;
			break;
		case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_WUM:
			wumPayloadBytes = vendValues->payload;
			payloadSize = 1;
			CWLog("CWSaveConfigurationUpdateResponseMessage wumPayloadBytes[0]= %d", wumPayloadBytes[0]);
			/*
			 * When dealing with WUM responses, the dafault size
			 * is 1 bytes, which is used for the type.
			 *
			 * The only response message with a bigger payload is the
			 * WTP_VERSION_RESPONSE (4 bytes), as it carries the WTP version
			 * together with the response type.
			 */
			if (wumPayloadBytes[0] == WTP_CONFIG_RESPONSE)
				payloadSize = 4;

			/*
			 * If we received a positive WTP_COMMIT_ACK, we need to terminate
			 * the WTP Manager Thread.
			 */
			if (wumPayloadBytes[0] == WTP_COMMIT_ACK && resultCode == CW_PROTOCOL_SUCCESS)
				closeWTPManager = CW_TRUE;
//upgrade 
			if (wumPayloadBytes[0] == WTP_UPDATE_RESPONSE )
			{
				CWLog("Recive WTP_UPDATE_RESPONSE ");

				if( resultCode == CW_FAILURE_WTP_UPGRADING_REJECT_NWEUPGRADE)
				{	
					CWLog("Recive WTP_UPDATE_RESPONSE resultCode = %d",resultCode);
					CWLog("Recive CW_FAILURE_WTP_UPGRADING_REJECT_NWEUPGRADE ");

					//CWThreadMutexLock(&gWTPs[WTPIndex].interfaceMutex);

					gWTPs[WTPIndex].interfaceResult = CW_FAILURE_WTP_UPGRADING_REJECT_NWEUPGRADE;

					BEupgradeEventResponse beUpgradeEventResp;
					beUpgradeEventResp.type = htons(BE_UPGRADE_EVENT_RESPONSE) ;
					// 4 sizeof(int)
					payloadSize = sizeof(resultCode);
					beUpgradeEventResp.length = Swap32(sizeof(resultCode));//4
					beUpgradeEventResp.resultCode = Swap32(resultCode);

					//CW_CREATE_STRING_ERR(&beConfigEventResp.resultCode, payloadSize, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});				
					//memset(beMonitorEventResp.xml, 0, payloadSize);
					//memcpy(beMonitorEventResp.xml, vendValues->payload, payloadSize);

					BESize = BE_TYPELEN_LEN+BE_CODE_LEN;

					beResp = AssembleBEheader((char*)&beUpgradeEventResp,&BESize,WTPIndex,NULL);
					
					break;
					
				}
				
				if( resultCode != CW_PROTOCOL_SUCCESS)
				{
					gWTPs[WTPIndex].interfaceResult = UPGRADE_FAILED;
					CWLog("Recive WTP_UPDATE_RESPONSE resultCode = %d,fail !",resultCode);
				}
	
				//CWSignalThreadCondition(&gWTPs[WTPIndex].interfaceComplete);
			}
			if (wumPayloadBytes[0] == WTP_CUP_ACK )
			{
				CWLog("Recive WTP_CUP_ACK ");
				if( resultCode != CW_PROTOCOL_SUCCESS)
				{
					gWTPs[WTPIndex].interfaceResult = UPGRADE_FAILED;
					CWLog("Recive WTP_CUP_ACK resultCode = %d,fail !",resultCode);
				}
				//CWSignalThreadCondition(&gWTPs[WTPIndex].interfaceComplete);
			}
			if (wumPayloadBytes[0] == WTP_COMMIT_ACK )
			{
				CWLog("Recive WTP_COMMIT_ACK resultCode = %d",resultCode);
				BEupgradeEventResponse beUpgradeEventResp;
				beUpgradeEventResp.type = htons(BE_UPGRADE_EVENT_RESPONSE) ;
				// 4 sizeof(int)
				payloadSize = sizeof(resultCode);
				beUpgradeEventResp.length = Swap32(sizeof(resultCode));//4
				beUpgradeEventResp.resultCode = Swap32(resultCode);

				//CW_CREATE_STRING_ERR(&beConfigEventResp.resultCode, payloadSize, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});				
				//memset(beMonitorEventResp.xml, 0, payloadSize);
				//memcpy(beMonitorEventResp.xml, vendValues->payload, payloadSize);

				BESize = BE_TYPELEN_LEN+BE_CODE_LEN;

				beResp = AssembleBEheader((char*)&beUpgradeEventResp,&BESize,WTPIndex,NULL);
				
				if(resultCode != CW_PROTOCOL_SUCCESS)
				{
					gWTPs[WTPIndex].interfaceResult = UPGRADE_FAILED;
					CWLog("Recive WTP_COMMIT_ACK resultCode = %d,fail !",resultCode);
					//CWSignalThreadCondition(&gWTPs[WTPIndex].interfaceComplete);
				}
				else
				{
					gWTPs[WTPIndex].interfaceResult = UPGRADE_SUCCESS;
				}
			}
			break;
			//config
		case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_CONFIG:
			  // payloadSize = vendValues->vendorPayloadLen;
			   //CWLog("[F:%s, L:%d] strlen(vendValues->payload) =%d,payloadSize:%d",__FILE__,__LINE__,strlen(vendValues->payload),payloadSize);
			   //CWLog("Msg :CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_XML Saved");
			   

			   beConfigEventResp.type = htons(BE_CONFIG_EVENT_RESPONSE) ;
			  // 4 sizeof(int)
			   payloadSize = sizeof(resultCode);
			   beConfigEventResp.length = Swap32(sizeof(resultCode));//4
			   beConfigEventResp.resultCode = Swap32(resultCode);
			   
			   //CW_CREATE_STRING_ERR(&beConfigEventResp.resultCode, payloadSize, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});				
			   //memset(beMonitorEventResp.xml, 0, payloadSize);
			   //memcpy(beMonitorEventResp.xml, vendValues->payload, payloadSize);
			   
			   BESize = BE_TYPELEN_LEN+BE_CODE_LEN;
			   
   			   beResp = AssembleBEheader((char*)&beConfigEventResp,&BESize,WTPIndex,NULL);
			   //CW_FREE_OBJECT(beConfigEventResp.xml);
			   
			   break;
			//monnitor
		case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_STATE:
			   payloadSize = vendValues->vendorPayloadLen;
			   CWLog("[F:%s, L:%d] strlen(vendValues->payload) =%d,payloadSize:%d",__FILE__,__LINE__,strlen(vendValues->payload),payloadSize);
			   CWLog("Msg :CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_XML Saved");
			  
			   beMonitorEventResp.type =htons( BE_MONITOR_EVENT_RESPONSE) ;
			   beMonitorEventResp.length = Swap32(payloadSize+sizeof(resultCode));
			   beMonitorEventResp.resultCode = Swap32(resultCode);
			   
			   CW_CREATE_STRING_ERR(beMonitorEventResp.xml, payloadSize+1, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});				
			   memset(beMonitorEventResp.xml, 0, payloadSize+1);
			   memcpy(beMonitorEventResp.xml, (char*)vendValues->payload, payloadSize);

			    //BE: ap connect	
			  if(gWTPs[WTPIndex].isConnect == CW_TRUE)
			  {
			  		CWLog("BE_CONNECT_EVENT Assemble ...");
					beConEve.type = htons(BE_CONNECT_EVENT);
					beConEve.length = Swap32(payloadSize+BE_CONNECT_EVENT_LEN);
					beConEve.state = BE_CONNECT_EVENT_CONNECT;
					BESize = BE_CONNECT_EVENT_LEN + BE_TYPELEN_LEN + payloadSize;
					beResp = AssembleBEheader((char*)&beConEve,&BESize,WTPIndex,beMonitorEventResp.xml);
				      	CW_FREE_OBJECT(beMonitorEventResp.xml);
			   		CWLog("BE_CONNECT_EVENT Assembled");
			   		break;
			   }
			   
			   BESize = BE_TYPELEN_LEN+payloadSize+BE_CODE_LEN;
			   
			   CWLog("beMonitorEventResp.xml front char:%x(%c),%x(%c),%x(%c),%x(%c)",
			   						beMonitorEventResp.xml[0],beMonitorEventResp.xml[0],
			   						beMonitorEventResp.xml[1],beMonitorEventResp.xml[1],
			   						beMonitorEventResp.xml[2],beMonitorEventResp.xml[2],
			   						beMonitorEventResp.xml[3],beMonitorEventResp.xml[3]);

			   CWLog("beMonitorEventResp.xml last char:%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
			   						beMonitorEventResp.xml[payloadSize-20],beMonitorEventResp.xml[payloadSize-19],
			   						beMonitorEventResp.xml[payloadSize-18],beMonitorEventResp.xml[payloadSize-17],
			   						beMonitorEventResp.xml[payloadSize-16],beMonitorEventResp.xml[payloadSize-15],
			   						beMonitorEventResp.xml[payloadSize-14],beMonitorEventResp.xml[payloadSize-13],
			   						beMonitorEventResp.xml[payloadSize-12],beMonitorEventResp.xml[payloadSize-11],
			   						beMonitorEventResp.xml[payloadSize-10],beMonitorEventResp.xml[payloadSize-9],
			   						beMonitorEventResp.xml[payloadSize-8],beMonitorEventResp.xml[payloadSize-7],
			   						beMonitorEventResp.xml[payloadSize-6],beMonitorEventResp.xml[payloadSize-5],
			   						beMonitorEventResp.xml[payloadSize-4],beMonitorEventResp.xml[payloadSize-3],
			   						beMonitorEventResp.xml[payloadSize-2],beMonitorEventResp.xml[payloadSize-1]
			   						);
			   
   			   beResp = AssembleBEheader((char*)&beMonitorEventResp,&BESize,WTPIndex,beMonitorEventResp.xml);

			   CW_FREE_OBJECT(beMonitorEventResp.xml);
			   
			    break;
				
			   //portal
		case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_PORTAL:

			//isLast Resp
			  if(gWTPs[WTPIndex].vendorPortalValues->isLast == FALSE)
			  {
			  	CWLog("[F:%s, L:%d] Portal not last !",__FILE__,__LINE__);
				break;
			  }
			 
			   bePortalEventResp.type = htons(BE_PORTAL_EVENT_RESPONSE) ;
			  // 4 sizeof(int) +2
			   payloadSize = BE_CODE_LEN+2*sizeof(short);
			   bePortalEventResp.length = Swap32(payloadSize);//4
			   bePortalEventResp.FileNo = htons(gWTPs[WTPIndex].vendorPortalValues->FileNo);
			   bePortalEventResp.TotalFileNum= htons(gWTPs[WTPIndex].vendorPortalValues->TotalFileNum);
			   bePortalEventResp.resultCode = Swap32(resultCode);
			   
			   //CW_CREATE_STRING_ERR(&beConfigEventResp.resultCode, payloadSize, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});				
			   //memset(beMonitorEventResp.xml, 0, payloadSize);
			   //memcpy(beMonitorEventResp.xml, vendValues->payload, payloadSize);
			   
			   BESize = BE_TYPELEN_LEN+payloadSize;
			   
   			   beResp = AssembleBEheader((char*)&bePortalEventResp,&BESize,WTPIndex,NULL);
			   CW_FREE_OBJECT(gWTPs[WTPIndex].vendorPortalValues);
			   
			   break;
		}
//BE : assemble header
			 if(beResp)
			   {
			   	if(gWTPs[WTPIndex].isConnect == CW_TRUE)
			   	{
					SendBERequest(beResp,BESize);
					CWLog("SendBERequest BE_CONNECT_EVENT");
					gWTPs[WTPIndex].isConnect = CW_FALSE;
				}
				else
				{
					SendBEResponse(beResp,BESize,WTPIndex);
				}
				CW_FREE_OBJECT(beResp);
				result = CW_TRUE;
			   }
			   //else
			  //{
				//CWLog("Error AssembleBEheader !");
				//result = CW_FALSE;
			   //}
			//CW_FREE_OBJECT(responseBuffer);
			if(vendValues->vendorPayloadLen > 0)
			{
				CW_FREE_OBJECT(vendValues->payload);				
			}
			CW_FREE_OBJECT(vendValues);
			//return result;
		}
#if 0
		if ( ( responseBuffer = malloc( headerSize+payloadSize+1 ) ) != NULL ) {

			//memset(responseBuffer, '0', headerSize+payloadSize);
			netWTPIndex = htonl(WTPIndex);
			memcpy(responseBuffer, (char*)&netWTPIndex, sizeof(int));
			
			netresultCode = htonl(resultCode);
			memcpy(responseBuffer+sizeof(int), (char*)&netresultCode, sizeof(int));

			netpayloadSize = htonl(payloadSize);
			memcpy(responseBuffer+(2*sizeof(int)), (char*)&netpayloadSize, sizeof(int));


			if (payloadSize > 0) {
				memcpy(responseBuffer+headerSize, vendValues->payload, payloadSize);
//				if (vendValues->vendorPayloadType == CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_CONFIG)
		//mem out
		//((char*)(vendValues->payload))[payloadSize] = '\0';
		//		responseBuffer[payloadSize+headerSize] = '\0';
			}

			socketIndex = gWTPs[WTPIndex].applicationIndex;	

			/****************************************************
		         * Forward payload to correct application  	    *
			 ****************************************************/

			if(!CWErr(CWThreadMutexLock(&appsManager.socketMutex[socketIndex]))) {
				CWLog("Error locking numSocketFree Mutex");
				return CW_FALSE;
			}
			int n;
			n = 0;
			while(n != (headerSize+payloadSize))
			{
				if ( (n += Writen(appsManager.appSocket[socketIndex], responseBuffer, headerSize+payloadSize))  < 0 ) {
					CWThreadMutexUnlock(&appsManager.socketMutex[socketIndex]);
					CWLog("Error locking numSocketFree Mutex");
					return CW_FALSE;
				}
				CWLog("[F:%s, L:%d] Writen n:%d",__FILE__,__LINE__,n);
			}

			CWThreadMutexUnlock(&appsManager.socketMutex[socketIndex]);

		}
		
		CW_FREE_OBJECT(responseBuffer);
		CW_FREE_OBJECT(vendValues->payload);
		CW_FREE_OBJECT(vendValues);

			CWLog("[F:%s, L:%d] ",__FILE__,__LINE__);
	}else if(!CWBindingSaveConfigurationUpdateResponse(resultCode, WTPIndex)) {
	
		return CW_FALSE;
	}
#endif	
	/*
	 * On a positive WTP_COMMIT_ACK, we need to close the WTP Manager.
	 */
	if (closeWTPManager) {
		if(!CWErr(CWThreadMutexLock(&gWTPs[WTPIndex].wtpMutex))) {
			CWLog("Error locking the gWTPsMutex mutex !");
			return CW_FALSE;
		}
		gWTPs[WTPIndex].isRequestClose = CW_TRUE;
		CWThreadMutexUnlock(&gWTPs[WTPIndex].wtpMutex);
#if 0
		if(!CWErr(CWThreadMutexLock(&gWTPs[WTPIndex].interfaceMutex))) 
		{
			CWLog("F:%s L:%d [ACrunState]:Error locking gWTPs[WTPIndex].interfaceMutex !",__FILE__,__LINE__);
			return CW_FALSE;
		}
		CWSignalThreadCondition(&gWTPs[WTPIndex].interfaceWait);
		//gWTPs[WTPIndex].iwvaule = 1;
		CWThreadMutexUnlock(&gWTPs[WTPIndex].interfaceMutex);
#endif
		
	}

	CWDebugLog("Configuration Update Response Saved");
	return CW_TRUE;
}

CWBool CWParseWTPDataTransferRequestMessage(CWProtocolMessage *msgPtr, int len, CWProtocolWTPDataTransferRequestValues *valuesPtr)
{
	int offsetTillMessages;
	

	if(msgPtr == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	if((msgPtr->msg) == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	

	offsetTillMessages = msgPtr->offset;
	
	CWLog("");
	CWLog("#________ WTP Data Transfer (Run) ________#");
	CWLog("Parsing WTP Data Transfer Request...");
	
	

	// parse message elements
	while((msgPtr->offset - offsetTillMessages) < len) {
		unsigned short int elemType=0;
		unsigned short int elemLen=0;
		
		CWParseFormatMsgElem(msgPtr, &elemType, &elemLen);
		
		switch(elemType) {
			case CW_MSG_ELEMENT_DATA_TRANSFER_DATA_CW_TYPE:{	
				if (!(CWParseMsgElemDataTransferData(msgPtr, elemLen, valuesPtr)))
					return CW_FALSE;
				CWDebugLog("----- %s --------\n",valuesPtr->debug_info);
				break;	
			}
			default:
				return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Unrecognized Message Element in WTP Data Transfer Request");
				break;	
		}
	}
	
	if((msgPtr->offset - offsetTillMessages) != len) return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Garbage at the End of the Message");


	return CW_TRUE;	
}

CWBool CWParseWTPEventRequestMessage(CWProtocolMessage *msgPtr,
				     int len,
				     CWProtocolWTPEventRequestValues *valuesPtr) {

	int offsetTillMessages;
	int i=0, k=0;

	if(msgPtr == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	if((msgPtr->msg) == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	/*
	CW_CREATE_OBJECT_ERR(valuesPtr, CWProtocolWTPEventRequestValues, return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
	*/
	offsetTillMessages = msgPtr->offset;
	
	CWLog("");
	CWLog("#________ WTP Event (Run) ________#");
	CWLog("Parsing WTP Event Request...");
	
	valuesPtr->errorReportCount = 0;
	valuesPtr->errorReport = NULL;
	valuesPtr->duplicateIPv4 = NULL;
	valuesPtr->duplicateIPv6 = NULL;
	valuesPtr->WTPOperationalStatisticsCount = 0;
	valuesPtr->WTPOperationalStatistics = NULL;
	valuesPtr->WTPRadioStatisticsCount = 0;
	valuesPtr->WTPRadioStatistics = NULL;
	valuesPtr->WTPRebootStatistics = NULL;
	valuesPtr->WTPVendorPayload = NULL;
	
	/* parse message elements */
	while((msgPtr->offset - offsetTillMessages) < len) {

		unsigned short int elemType = 0;
		unsigned short int elemLen = 0;
		
		CWParseFormatMsgElem(msgPtr, &elemType, &elemLen);
		
		switch(elemType) {
			case CW_MSG_ELEMENT_CW_DECRYPT_ER_REPORT_CW_TYPE:
				CW_CREATE_OBJECT_ERR(valuesPtr->errorReport, 
						     CWDecryptErrorReportValues,
						     return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););

				if (!(CWParseMsgElemDecryptErrorReport(msgPtr, elemLen, valuesPtr->errorReport)))
					return CW_FALSE;
				break;	
			case CW_MSG_ELEMENT_DUPLICATE_IPV4_ADDRESS_CW_TYPE:
				CW_CREATE_OBJECT_ERR(valuesPtr->duplicateIPv4,
						     WTPDuplicateIPv4,
						     return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););	
				
				CW_CREATE_ARRAY_ERR((valuesPtr->duplicateIPv4)->MACoffendingDevice_forIpv4,
						    6,
						    unsigned char,
						    return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););

				if (!(CWParseMsgElemDuplicateIPv4Address(msgPtr, elemLen, valuesPtr->duplicateIPv4)))
					return CW_FALSE;
				break;
			case CW_MSG_ELEMENT_DUPLICATE_IPV6_ADDRESS_CW_TYPE:
				CW_CREATE_OBJECT_ERR(valuesPtr->duplicateIPv6,
						     WTPDuplicateIPv6,
						     return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););

				CW_CREATE_ARRAY_ERR((valuesPtr->duplicateIPv6)->MACoffendingDevice_forIpv6,
						    6,
						    unsigned char,
						    return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););

				if (!(CWParseMsgElemDuplicateIPv6Address(msgPtr, elemLen, valuesPtr->duplicateIPv6)))
					return CW_FALSE;
				break;
			case CW_MSG_ELEMENT_WTP_OPERAT_STATISTICS_CW_TYPE:
				valuesPtr->WTPOperationalStatisticsCount++;
				msgPtr->offset += elemLen;
				break;
			case CW_MSG_ELEMENT_WTP_RADIO_STATISTICS_CW_TYPE:
				valuesPtr->WTPRadioStatisticsCount++;
				msgPtr->offset += elemLen;
				break;
			case CW_MSG_ELEMENT_WTP_REBOOT_STATISTICS_CW_TYPE:
				CW_CREATE_OBJECT_ERR(valuesPtr->WTPRebootStatistics,
						     WTPRebootStatisticsInfo,
						     return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););

				if (!(CWParseWTPRebootStatistics(msgPtr, elemLen, valuesPtr->WTPRebootStatistics)))
					return CW_FALSE;	
				break;
			case CW_MSG_ELEMENT_WTP_VENDORPAYLOD_CW_TYPE:
				CW_CREATE_OBJECT_ERR(valuesPtr->WTPVendorPayload,
						     CWProtocolVendorSpecificValues,
						     return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););

				if (!(CWParseWTPVendorPayload(msgPtr, elemLen, valuesPtr->WTPVendorPayload)))
					return CW_FALSE;	
				break;
			default:
				return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Unrecognized Message Element in WTP Event Request");
				break;	
		}
	}
	
	if((msgPtr->offset - offsetTillMessages) != len) 
		return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Garbage at the End of the Message");
	
	CW_CREATE_ARRAY_ERR(valuesPtr->WTPOperationalStatistics,
			    valuesPtr->WTPOperationalStatisticsCount,
			    WTPOperationalStatisticsValues,
			    return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););

	CW_CREATE_ARRAY_ERR(valuesPtr->WTPRadioStatistics,
			    valuesPtr->WTPRadioStatisticsCount,
			    WTPRadioStatisticsValues,
			    return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););	

	msgPtr->offset = offsetTillMessages;

	while((msgPtr->offset - offsetTillMessages) < len) {
	
		unsigned short int elemType = 0;
		unsigned short int elemLen = 0;
		
		CWParseFormatMsgElem(msgPtr, &elemType, &elemLen);
		
		switch(elemType) {
			case CW_MSG_ELEMENT_WTP_OPERAT_STATISTICS_CW_TYPE:
				if (!(CWParseWTPOperationalStatistics(msgPtr,
								      elemLen,
								      &(valuesPtr->WTPOperationalStatistics[k++]))))
					return CW_FALSE;
				break;
			case CW_MSG_ELEMENT_WTP_RADIO_STATISTICS_CW_TYPE:
				if (!(CWParseWTPRadioStatistics(msgPtr,
								elemLen,
								&(valuesPtr->WTPRadioStatistics[i++]))))
					return CW_FALSE;
				break;
			default:
				msgPtr->offset += elemLen;
				break;
		}
	}
	CWLog("WTP Event Request Parsed");
	return CW_TRUE;	
}
//BE:add ap state req
CWBool CWSaveWTPEventRequestMessage(CWProtocolWTPEventRequestValues *WTPEventRequest,
				    CWWTPProtocolManager *WTPProtocolManager) {

	if(WTPEventRequest == NULL || WTPProtocolManager == NULL)
		return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);

	if(WTPEventRequest->WTPVendorPayload) {	

		CW_CREATE_OBJECT_ERR(WTPProtocolManager->WTPVendorPayload,
			CWProtocolVendorSpecificValues,
			return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
		WTPProtocolManager->WTPVendorPayload->vendorPayloadLen= WTPEventRequest->WTPVendorPayload->vendorPayloadLen;

		CW_CREATE_OBJECT_SIZE_ERR(WTPProtocolManager->WTPVendorPayload->payload,
						     WTPProtocolManager->WTPVendorPayload->vendorPayloadLen+1,
						     return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
		memset(WTPProtocolManager->WTPVendorPayload->payload,0,WTPProtocolManager->WTPVendorPayload->vendorPayloadLen+1);
		memcpy(WTPProtocolManager->WTPVendorPayload->payload, WTPEventRequest->WTPVendorPayload->payload,WTPProtocolManager->WTPVendorPayload->vendorPayloadLen);
		CWLog("WTPProtocolManager->WTPVendorPayload->payload = %s",WTPProtocolManager->WTPVendorPayload->payload);
		
		CW_FREE_OBJECT(WTPEventRequest->WTPVendorPayload->payload);
		
	}

	if(WTPEventRequest->WTPRebootStatistics) {	

		CW_CREATE_OBJECT_ERR(WTPProtocolManager->WTPRebootStatistics,
						     WTPRebootStatisticsInfo,
						     return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
		WTPProtocolManager->WTPRebootStatistics = WTPEventRequest->WTPRebootStatistics;
	}

	if((WTPEventRequest->WTPOperationalStatisticsCount) > 0) {

		int i,k;
		CWBool found=CW_FALSE;

		for(i = 0; i < (WTPEventRequest->WTPOperationalStatisticsCount); i++) {
			
			found=CW_FALSE;
			for(k=0; k<(WTPProtocolManager->radiosInfo).radioCount; k++) {

				if((WTPProtocolManager->radiosInfo).radiosInfo[k].radioID == (WTPEventRequest->WTPOperationalStatistics[i]).radioID) {

					found=CW_TRUE;
					(WTPProtocolManager->radiosInfo).radiosInfo[k].TxQueueLevel = (WTPEventRequest->WTPOperationalStatistics[i]).TxQueueLevel;
					(WTPProtocolManager->radiosInfo).radiosInfo[k].wirelessLinkFramesPerSec = (WTPEventRequest->WTPOperationalStatistics[i]).wirelessLinkFramesPerSec;
				}
			}
			/*if(!found)
			{
				for(k=0; k<(WTPProtocolManager->radiosInfo).radioCount; k++)
				{
					if((WTPProtocolManager->radiosInfo).radiosInfo[k].radioID == UNUSED_RADIO_ID); 
					{
						(WTPProtocolManager->radiosInfo).radiosInfo[k].radioID = (WTPEventRequest->WTPOperationalStatistics[i]).radioID;
						(WTPProtocolManager->radiosInfo).radiosInfo[k].TxQueueLevel = (WTPEventRequest->WTPOperationalStatistics[i]).TxQueueLevel;
						(WTPProtocolManager->radiosInfo).radiosInfo[k].wirelessLinkFramesPerSec = (WTPEventRequest->WTPOperationalStatistics[i]).wirelessLinkFramesPerSec;
					}
				}	
			}*/
		}
	}

	if((WTPEventRequest->WTPRadioStatisticsCount) > 0) {
		
		int i,k;
		CWBool found;

		for(i=0; i < (WTPEventRequest->WTPRadioStatisticsCount); i++) {
			found=CW_FALSE;
			for(k = 0; k < (WTPProtocolManager->radiosInfo).radioCount; k++)  {

				if((WTPProtocolManager->radiosInfo).radiosInfo[k].radioID == (WTPEventRequest->WTPOperationalStatistics[i]).radioID) {

					found=CW_TRUE;
					(WTPProtocolManager->radiosInfo).radiosInfo[k].statistics = (WTPEventRequest->WTPRadioStatistics[i]).WTPRadioStatistics;
				}
			}
			/*if(!found)
			{
				for(k=0; k<(WTPProtocolManager->radiosInfo).radioCount; k++) 
				{
					if((WTPProtocolManager->radiosInfo).radiosInfo[k].radioID == UNUSED_RADIO_ID);
					{
						(WTPProtocolManager->radiosInfo).radiosInfo[k].radioID = (WTPEventRequest->WTPOperationalStatistics[i]).radioID;
						(WTPProtocolManager->radiosInfo).radiosInfo[k].statistics = (WTPEventRequest->WTPRadioStatistics[i]).WTPRadioStatistics;
					}
				}	
			}*/
		}
	}
	/*
	CW_FREE_OBJECT((WTPEventRequest->WTPOperationalStatistics), (WTPEventRequest->WTPOperationalStatisticsCount));
	CW_FREE_OBJECTS_ARRAY((WTPEventRequest->WTPRadioStatistics), (WTPEventRequest->WTPRadioStatisticsCount));
	Da controllare!!!!!!!
	*/
	
	CW_FREE_OBJECT(WTPEventRequest->WTPVendorPayload);
	CW_FREE_OBJECT(WTPEventRequest->WTPRebootStatistics);
	CW_FREE_OBJECT(WTPEventRequest->WTPOperationalStatistics);
	CW_FREE_OBJECT(WTPEventRequest->WTPRadioStatistics);
	/*CW_FREE_OBJECT(WTPEventRequest);*/

	return CW_TRUE;
}

CWBool CWAssembleWTPDataTransferResponse (CWProtocolMessage **messagesPtr, int *fragmentsNumPtr, int PMTU, int seqNum) 
{
	CWProtocolMessage *msgElems= NULL;
	const int msgElemCount=0;
	CWProtocolMessage *msgElemsBinding= NULL;
	int msgElemBindingCount=0;

	if(messagesPtr == NULL || fragmentsNumPtr == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	CWLog("Assembling WTP Data Transfer Response...");
		
	if(!(CWAssembleMessage(messagesPtr, fragmentsNumPtr, PMTU, seqNum,
			       CW_MSG_TYPE_VALUE_DATA_TRANSFER_RESPONSE,
			       msgElems, msgElemCount, msgElemsBinding,
			       msgElemBindingCount, 
#ifdef CW_NO_DTLS
				CW_PACKET_PLAIN
#else				
				CW_PACKET_CRYPT
#endif
	))) 

		return CW_FALSE;
	
	CWLog("WTP Data Transfer Response Assembled");
	
	return CW_TRUE;
}

CWBool CWAssembleWTPEventResponse(CWProtocolMessage **messagesPtr,
				  int *fragmentsNumPtr,
				  int PMTU,
				  int seqNum) {

	CWProtocolMessage *msgElems= NULL;
	const int msgElemCount=0;
	CWProtocolMessage *msgElemsBinding= NULL;
	int msgElemBindingCount=0;

	if(messagesPtr == NULL || fragmentsNumPtr == NULL) 
		return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	CWLog("Assembling WTP Event Response...");
		
	if(!(CWAssembleMessage(messagesPtr,
			       fragmentsNumPtr,
			       PMTU,
			       seqNum,
			       CW_MSG_TYPE_VALUE_WTP_EVENT_RESPONSE,
			       msgElems,
			       msgElemCount,
			       msgElemsBinding,
			       msgElemBindingCount,
#ifdef CW_NO_DTLS
				CW_PACKET_PLAIN
#else				
				CW_PACKET_CRYPT
#endif
	))) 
		return CW_FALSE;
	
	CWLog("WTP Event Response Assembled");
	
	return CW_TRUE;
}

CWBool CWParseChangeStateEventRequestMessage2(CWProtocolMessage *msgPtr,
					      int len,
					      CWProtocolChangeStateEventRequestValues **valuesPtr) {

	int offsetTillMessages;
	int i=0;

	if(msgPtr == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	if((msgPtr->msg) == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	CW_CREATE_OBJECT_ERR(*valuesPtr,
			     CWProtocolChangeStateEventRequestValues,
			     return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););

	offsetTillMessages = msgPtr->offset;
	
	CWLog("");
	CWLog("#________ WTP Change State Event (Run) ________#");
	
	(*valuesPtr)->radioOperationalInfo.radiosCount = 0;
	(*valuesPtr)->radioOperationalInfo.radios = NULL;
	
	/* parse message elements */
	while((msgPtr->offset-offsetTillMessages) < len) {
		unsigned short int elemType = 0;/* = CWProtocolRetrieve32(&completeMsg); */
		unsigned short int elemLen = 0;	/* = CWProtocolRetrieve16(&completeMsg); */
		
		CWParseFormatMsgElem(msgPtr,&elemType,&elemLen);		

		/*CWDebugLog("Parsing Message Element: %u, elemLen: %u", elemType, elemLen);*/

		switch(elemType) {
			case CW_MSG_ELEMENT_RADIO_OPERAT_STATE_CW_TYPE:
				/* just count how many radios we have, so we 
				 * can allocate the array
				 */
				((*valuesPtr)->radioOperationalInfo.radiosCount)++;
				msgPtr->offset += elemLen;
				break;
			case CW_MSG_ELEMENT_RESULT_CODE_CW_TYPE: 
				if(!(CWParseResultCode(msgPtr, elemLen, &((*valuesPtr)->resultCode)))) 
					return CW_FALSE;
				break;
			default:
				return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Unrecognized Message Element in Change State Event Request");
		}
	}
	
	if((msgPtr->offset - offsetTillMessages) != len) return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Garbage at the End of the Message");
	
	CW_CREATE_ARRAY_ERR((*valuesPtr)->radioOperationalInfo.radios,
			    (*valuesPtr)->radioOperationalInfo.radiosCount,
			    CWRadioOperationalInfoValues,
			    return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
		
	msgPtr->offset = offsetTillMessages;
	
	i = 0;

	while(msgPtr->offset-offsetTillMessages < len) {
		unsigned short int type = 0;	/* = CWProtocolRetrieve32(&completeMsg); */
		unsigned short int len = 0;	/* = CWProtocolRetrieve16(&completeMsg); */
		
		CWParseFormatMsgElem(msgPtr,&type,&len);		

		switch(type) {
			case CW_MSG_ELEMENT_RADIO_OPERAT_STATE_CW_TYPE:
				/* will be handled by the caller */
				if(!(CWParseWTPRadioOperationalState(msgPtr, len, &((*valuesPtr)->radioOperationalInfo.radios[i])))) 
					return CW_FALSE;
				i++;
				break;
			default:
				msgPtr->offset += len;
				break;
		}
	}
	CWLog("Change State Event Request Parsed");
	return CW_TRUE;
}

CWBool CWSaveChangeStateEventRequestMessage(CWProtocolChangeStateEventRequestValues *valuesPtr,
					    CWWTPProtocolManager *WTPProtocolManager) {

	CWBool found;
	CWBool retValue = CW_TRUE;

	if(valuesPtr == NULL || WTPProtocolManager == NULL) 
		return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);

	if((valuesPtr->radioOperationalInfo.radiosCount) >0) {
	
		int i,k;
		for(i=0; i<(valuesPtr->radioOperationalInfo.radiosCount); i++) {

			found=CW_FALSE;
			for(k=0; k<(WTPProtocolManager->radiosInfo).radioCount; k++) {

				if((WTPProtocolManager->radiosInfo).radiosInfo[k].radioID == (valuesPtr->radioOperationalInfo.radios[i]).ID) {

					found=CW_TRUE;
					(WTPProtocolManager->radiosInfo).radiosInfo[k].operationalState = (valuesPtr->radioOperationalInfo.radios[i]).state;
					(WTPProtocolManager->radiosInfo).radiosInfo[k].operationalCause = (valuesPtr->radioOperationalInfo.radios[i]).cause;
				}
				if(!found) 
					retValue= CW_FALSE;
			}
		}
	}
	
	CW_FREE_OBJECT(valuesPtr->radioOperationalInfo.radios)
	CW_FREE_OBJECT(valuesPtr);	

	return retValue;
}


CWBool CWParseEchoRequestMessage(CWProtocolMessage *msgPtr, int len) {

	int offsetTillMessages;

	if(msgPtr == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	if((msgPtr->msg) == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	offsetTillMessages = msgPtr->offset;
	
	CWLog("");
	CWLog("#________ Echo Request (Run) ________#");
	
	/* parse message elements */
	while((msgPtr->offset-offsetTillMessages) < len) {
		unsigned short int elemType = 0;/* = CWProtocolRetrieve32(&completeMsg); */
		unsigned short int elemLen = 0;	/* = CWProtocolRetrieve16(&completeMsg); */
		
		CWParseFormatMsgElem(msgPtr,&elemType,&elemLen);		

		/*CWDebugLog("Parsing Message Element: %u, elemLen: %u", elemType, elemLen);*/

		switch(elemType) {
			default:
				return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Echo Request must carry no message elements");
		}
	}
	
	if((msgPtr->offset - offsetTillMessages) != len) 
		return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Garbage at the End of the Message");
	
	CWLog("Echo Request Parsed");
	
	return CW_TRUE;
}

CWBool CWAssembleEchoResponse(CWProtocolMessage **messagesPtr, int *fragmentsNumPtr, int PMTU, int seqNum) {

	CWProtocolMessage *msgElems= NULL;
	const int msgElemCount=0;
	CWProtocolMessage *msgElemsBinding= NULL;
	int msgElemBindingCount=0;
	
	if(messagesPtr == NULL || fragmentsNumPtr == NULL)
		return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	CWLog("Assembling Echo Response...");
		
	if(!(CWAssembleMessage(messagesPtr,
			       fragmentsNumPtr,
			       PMTU,
			       seqNum,
			       CW_MSG_TYPE_VALUE_ECHO_RESPONSE,
			       msgElems,
			       msgElemCount,
			       msgElemsBinding,
			       msgElemBindingCount,
#ifdef CW_NO_DTLS
				CW_PACKET_PLAIN
#else				
				CW_PACKET_CRYPT
#endif
	)))
		return CW_FALSE;
	
	CWLog("Echo Response Assembled");
	return CW_TRUE;
}

CWBool CWAssembleConfigurationUpdateRequest(CWProtocolMessage **messagesPtr,
					    int *fragmentsNumPtr,
					    int PMTU,
						int seqNum,
						int msgElement) {

	CWProtocolMessage *msgElemsBinding = NULL;
	int msgElemBindingCount=0;
	CWProtocolMessage *msgElems = NULL;
	int msgElemCount=0;
	
	if (messagesPtr == NULL || fragmentsNumPtr == NULL)
		return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	CWLog("Assembling Configuration Update Request...");

	switch (msgElement) {
	case CONFIG_UPDATE_REQ_QOS_ELEMENT_TYPE:
	  {
		if(!CWBindingAssembleConfigurationUpdateRequest(&msgElemsBinding, &msgElemBindingCount, BINDING_MSG_ELEMENT_TYPE_WTP_QOS)) {
		  return CW_FALSE;
		}
		break;
	  }
	case CONFIG_UPDATE_REQ_OFDM_ELEMENT_TYPE:
	  {
		if(!CWBindingAssembleConfigurationUpdateRequest(&msgElemsBinding, &msgElemBindingCount, BINDING_MSG_ELEMENT_TYPE_OFDM_CONTROL)) {
		  return CW_FALSE;
		}
		break;
	  }
	case CONFIG_UPDATE_REQ_VENDOR_UCI_ELEMENT_TYPE:
	  {
		CWLog("Assembling UCI Conf Update Request");
		if(!CWProtocolAssembleConfigurationUpdateRequest(&msgElems, &msgElemCount, CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_UCI)) {
		  return CW_FALSE;
		}
		break;
	  }
	case CONFIG_UPDATE_REQ_VENDOR_WUM_ELEMENT_TYPE:
	 {
                CWLog("Assembling WUM Conf Update Request");
                if(!CWProtocolAssembleConfigurationUpdateRequest(&msgElems, &msgElemCount, CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_WUM)) {
                  return CW_FALSE;
                }
                break;
         }
	case CONFIG_UPDATE_REQ_VENDOR_CONFIG_ELEMENT_TYPE:
	 {
                CWLog("Assembling XML Conf Update Request");
                if(!CWProtocolAssembleConfigurationUpdateRequest(&msgElems, &msgElemCount, CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_CONFIG)) {
                  return CW_FALSE;
                }
                break;
         }
	case CONFIG_UPDATE_REQ_VENDOR_STATE_ELEMENT_TYPE:
	 {
                CWLog("Assembling XML Conf Update Request");
                if(!CWProtocolAssembleConfigurationUpdateRequest(&msgElems, &msgElemCount, CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_STATE)) {
                  return CW_FALSE;
                }
                break;
         }
	case CONFIG_UPDATE_REQ_VENDOR_PORTAL_ELEMENT_TYPE:
	 {
                CWLog("Assembling PORTAL Conf Update Request");
                if(!CWProtocolAssembleConfigurationUpdateRequest(&msgElems, &msgElemCount, CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_PORTAL)) {
                  return CW_FALSE;
                }
                break;
         }
	case CONFIG_UPDATE_REQ_VENDOR_ACTIVE_ELEMENT_TYPE:
	 {
                CWLog("Assembling ACTIVE Conf Update Request");
                if(!CWProtocolAssembleConfigurationUpdateRequest(&msgElems, &msgElemCount, CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_ACTIVE)) {
                  return CW_FALSE;
                }
                break;
         }
	case CONFIG_UPDATE_REQ_VENDOR_UNACTIVE_ELEMENT_TYPE:
	 {
                CWLog("Assembling UNACTIVE Conf Update Request");
                if(!CWProtocolAssembleConfigurationUpdateRequest(&msgElems, &msgElemCount, CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_UNACTIVE)) {
                  return CW_FALSE;
                }
                break;
         }
	}	  

	if(!(CWAssembleMessage(messagesPtr,
			       fragmentsNumPtr,
			       PMTU,
			       seqNum,
			       CW_MSG_TYPE_VALUE_CONFIGURE_UPDATE_REQUEST,
			       msgElems,
			       msgElemCount,
			       msgElemsBinding,
			       msgElemBindingCount,
#ifdef CW_NO_DTLS
				CW_PACKET_PLAIN
#else				
				CW_PACKET_CRYPT
#endif
	)))
		return CW_FALSE;

	CWLog("Configuration Update Request Assembled");
	
	return CW_TRUE;
}

CWBool CWAssembleClearConfigurationRequest(CWProtocolMessage **messagesPtr, int *fragmentsNumPtr, int PMTU, int seqNum) 
{
	CWProtocolMessage *msgElemsBinding = NULL;
	int msgElemBindingCount=0;
	CWProtocolMessage *msgElems = NULL;
	int msgElemCount=0;
	
	if(messagesPtr == NULL || fragmentsNumPtr == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	CWLog("Assembling Clear Configuration Request...");
	
	
	if(!(CWAssembleMessage(messagesPtr, fragmentsNumPtr, PMTU, 
		seqNum,CW_MSG_TYPE_VALUE_CLEAR_CONFIGURATION_REQUEST,
		msgElems, msgElemCount, msgElemsBinding, msgElemBindingCount, 
#ifdef CW_NO_DTLS
				CW_PACKET_PLAIN
#else				
				CW_PACKET_CRYPT
#endif
	))) 
		return CW_FALSE;

	CWLog("Clear Configuration Request Assembled");
	
	return CW_TRUE;
}

CWBool CWAssembleResetRequest(CWProtocolMessage **messagesPtr, int *fragmentsNumPtr, int PMTU, int seqNum) 
{
	CWProtocolMessage *msgElemsBinding = NULL;
	int msgElemBindingCount=0;
	CWProtocolMessage *msgElems = NULL;
	int msgElemCount=0;
	
	if(messagesPtr == NULL || fragmentsNumPtr == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	CWLog("Assembling Reset Request...");
	
	
	if(!(CWAssembleMessage(messagesPtr, fragmentsNumPtr, PMTU, 
		seqNum,CW_MSG_TYPE_VALUE_RESET_REQUEST,
		msgElems, msgElemCount, msgElemsBinding, msgElemBindingCount, 
#ifdef CW_NO_DTLS
				CW_PACKET_PLAIN
#else				
				CW_PACKET_CRYPT
#endif
	))) 
		return CW_FALSE;

	CWLog("Reset Request Assembled");
	
	return CW_TRUE;
}



CWBool CWAssembleStationConfigurationRequest(CWProtocolMessage **messagesPtr, int *fragmentsNumPtr, int PMTU, int seqNum,unsigned char* StationMacAddr) 
{
	CWProtocolMessage *msgElemsBinding = NULL;
	int msgElemBindingCount=0;
	CWProtocolMessage *msgElems = NULL;
	int msgElemCount=1;
	int k = -1;
	
		
	if(messagesPtr == NULL || fragmentsNumPtr == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	CWLog("Assembling Station Configuration Request...");
	
	CW_CREATE_PROTOCOL_MSG_ARRAY_ERR(msgElems, msgElemCount, return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
	// Assemble Message Elements
	if (!(CWAssembleMsgElemAddStation(0,&(msgElems[++k]),StationMacAddr)))   //radioID = 0 -valore predefinito-
	   {
		CWErrorHandleLast();
		int i;
		for(i = 0; i <= k; i++) {CW_FREE_PROTOCOL_MESSAGE(msgElems[i]);}
		CW_FREE_OBJECT(msgElems);
		return CW_FALSE; // error will be handled by the caller
	   }
	
	
/*  to be implemented in a case of Binding with appropriate messages elements -- see draft capwap-spec && capwap-binding 
	if(!CWBindingAssembleConfigurationUpdateRequest(&msgElemsBinding, &msgElemBindingCount)){
		return CW_FALSE;
	}
*/
	if(!(CWAssembleMessage(messagesPtr, fragmentsNumPtr, PMTU, seqNum,
		CW_MSG_TYPE_VALUE_STATION_CONFIGURATION_REQUEST, msgElems, 
		msgElemCount, msgElemsBinding, msgElemBindingCount,
#ifdef CW_NO_DTLS
				CW_PACKET_PLAIN
#else				
				CW_PACKET_CRYPT
#endif
	))) {
		return CW_FALSE;}

	CWLog("Station Configuration Request Assembled");
	
	return CW_TRUE;
}

CWBool CWStartNeighborDeadTimer(int WTPIndex) {

	/* start NeighborDeadInterval timer */
	if(!CWErr(CWTimerRequest(gCWNeighborDeadInterval,
				 &(gWTPs[WTPIndex].thread),
				 &(gWTPs[WTPIndex].currentTimer),
				 CW_CRITICAL_TIMER_EXPIRED_SIGNAL))) {
		return CW_FALSE;
	}
	return CW_TRUE;
}

CWBool CWStartNeighborDeadTimerForEcho(int WTPIndex){
	
	int echoInterval;

	/* start NeighborDeadInterval timer */
	CWACGetEchoRequestTimer(&echoInterval);
	CWLog("CWStartNeighborDeadTimerForEcho echoInterval = %d",echoInterval);
	if(!CWErr(CWTimerRequest(2*echoInterval,
				 &(gWTPs[WTPIndex].thread),
				 &(gWTPs[WTPIndex].currentTimer),
				 CW_CRITICAL_TIMER_EXPIRED_SIGNAL))) {
		return CW_FALSE;
	}
	return CW_TRUE;
}

CWBool CWStopNeighborDeadTimer(int WTPIndex) {

	if(!CWTimerCancel(&(gWTPs[WTPIndex].currentTimer))) {
	
		return CW_FALSE;
	}
	return CW_TRUE;
}

CWBool CWRestartNeighborDeadTimer(int WTPIndex) {
	
	CWThreadSetSignals(SIG_BLOCK, 1, CW_SOFT_TIMER_EXPIRED_SIGNAL);	
	
	if(!CWStopNeighborDeadTimer(WTPIndex)) return CW_FALSE;
	if(!CWStartNeighborDeadTimer(WTPIndex)) return CW_FALSE;
	
	CWThreadSetSignals(SIG_UNBLOCK, 1, CW_SOFT_TIMER_EXPIRED_SIGNAL);
	
	CWDebugLog("NeighborDeadTimer restarted");
	return CW_TRUE;
}

CWBool CWRestartNeighborDeadTimerForEcho(int WTPIndex) {
	
	CWThreadSetSignals(SIG_BLOCK, 1, CW_SOFT_TIMER_EXPIRED_SIGNAL);

	if(!CWStopNeighborDeadTimer(WTPIndex)) return CW_FALSE;
	if(!CWStartNeighborDeadTimerForEcho(WTPIndex)) return CW_FALSE;

	CWThreadSetSignals(SIG_UNBLOCK, 1, CW_SOFT_TIMER_EXPIRED_SIGNAL);
	
	CWDebugLog("NeighborDeadTimer restarted for Echo interval");
	return CW_TRUE;
}
