/*******************************************************************************************
 * Copyright (c) 2006-7 Laboratorio di Sistemi di Elaborazione e Bioingegneria Informatica *
 *                      Universita' Campus BioMedico - Italy                               *
 *                                                                                         *
 * This program is free software; you can redistribute it and/or modify it under the terms *
 * of the GNU General Public License as published by the Free Software Foundation; either  *
 * version 2 of the License, or (at your option) any later version.                        *
 *                                                                                         *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY         *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 	   *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.                *
 *                                                                                         *
 * You should have received a copy of the GNU General Public License along with this       *
 * program; if not, write to the:                                                          *
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,                    *
 * MA  02111-1307, USA.                                                                    *
 *                                                                                         *
 * --------------------------------------------------------------------------------------- *
 * Project:  Capwap                                                                        *
 *                                                                                         *
 * Author :  Ludovico Rossi (ludo@bluepixysw.com)                                          *  
 *           Del Moro Andrea (andrea_delmoro@libero.it)                                    *
 *           Giovannini Federica (giovannini.federica@gmail.com)                           *
 *           Massimo Vellucci (m.vellucci@unicampus.it)                                    *
 *           Mauro Bisson (mauro.bis@gmail.com)                                            *
 *******************************************************************************************/

 
#include "CWAC.h"

#ifdef DMALLOC
#include "../dmalloc-5.5.0/dmalloc.h"
#endif
int gCWConfigStatePendingTimer = CW_CONFIG_STATE_INTERVAL_DEFAULT;
CWBool CWAssembleJoinResponse(CWProtocolMessage **messagesPtr,
			      int *fragmentsNumPtr,
			      int PMTU,
			      int seqNum,
			      CWList msgElemList);

CWBool CWParseJoinRequestMessage(char *msg,
				 int len,
				 int *seqNumPtr,
				 CWProtocolJoinRequestValues *valuesPtr);

CWBool CWSaveJoinRequestMessage(CWProtocolJoinRequestValues *joinRequest,
				CWWTPProtocolManager *WTPProtocolManager);


CWBool ACEnterJoin(int WTPIndex, CWProtocolMessage *msgPtr)
{
	int seqNum;
	int macId;
	int i,j,finded = -1;
	CWProtocolJoinRequestValues joinRequest;
	CWList msgElemList = NULL;

	CWLog("\n");
	CWLog("######### Join State #########");

	if (msgPtr == NULL)
		return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);

	if (!(CWParseJoinRequestMessage(msgPtr->msg, msgPtr->offset, &seqNum,
			&joinRequest))) {
		/* note: we can kill our thread in case of out-of-memory 
		 * error to free some space.
		 * we can see this just calling CWErrorGetLastErrorCode()
		 */
		return CW_FALSE;
	}


	CWBool ACIpv4List = CW_FALSE;
	CWBool ACIpv6List = CW_FALSE;
	CWBool resultCode = CW_TRUE;
	int resultCodeValue = CW_PROTOCOL_SUCCESS;
	/* CWBool sessionID = CW_FALSE; */

	if (!(CWSaveJoinRequestMessage(&joinRequest,
			&(gWTPs[WTPIndex].WTPProtocolManager)))) {

		resultCodeValue = CW_PROTOCOL_FAILURE_RES_DEPLETION;
		return CW_FALSE;
	}
	//add WTP MAC
	macId = gWTPs[WTPIndex].WTPProtocolManager.WTPBoardData.vendorInfosCount - 1;
	CWLog("gWTPs[%d].WTPProtocolManager->name : %s",WTPIndex,gWTPs[WTPIndex].WTPProtocolManager.name);
	CWLog("MAC = %x:%x:%x:%x:%x:%x",(u_char)(gWTPs[WTPIndex].WTPProtocolManager.WTPBoardData.vendorInfos[macId].valuePtr[0]),
			(u_char)(gWTPs[WTPIndex].WTPProtocolManager.WTPBoardData.vendorInfos[macId].valuePtr[1]),
			(u_char)(gWTPs[WTPIndex].WTPProtocolManager.WTPBoardData.vendorInfos[macId].valuePtr[2]),
			(u_char)(gWTPs[WTPIndex].WTPProtocolManager.WTPBoardData.vendorInfos[macId].valuePtr[3]),
			(u_char)(gWTPs[WTPIndex].WTPProtocolManager.WTPBoardData.vendorInfos[macId].valuePtr[4]),
			(u_char)(gWTPs[WTPIndex].WTPProtocolManager.WTPBoardData.vendorInfos[macId].valuePtr[5]));

	for(i=0;i<6;i++)
	{
		gWTPs[WTPIndex].MAC[i] = (u_char)gWTPs[WTPIndex].WTPProtocolManager.WTPBoardData.vendorInfos[macId].valuePtr[i];
	}

	for(i = 0;i < MAC_ADDR_LEN;i++)
	{
		if(gWTPs[WTPIndex].MAC[i] == 0)
		{
			if( i == (MAC_ADDR_LEN - 1))
			{
				CWLog("WTP %d MAC is 0:0:0:0:0:0",WTPIndex);
				return CW_FALSE;
			}
		}
		else
		{
			break;
		}
	}

		//compare MAC
		//add mutex
	#if 0
	if(!CWErr(CWThreadMutexLock(&gActiveWTPsMutex))) {
		CWLog("Error locking the gActiveWTPsMutex mutex");
		return CW_FALSE;
	}
	numActiveWTPs = gActiveWTPs;
	CWThreadMutexUnlock(&gActiveWTPsMutex);
	k = numActiveWTPs;
	#endif

	
	for(i = 0; i < CW_MAX_WTP ; i++)
	{
		if( i == WTPIndex || gWTPs[i].currentState != CW_ENTER_RUN)
		//if( i == WTPIndex || gWTPs[i].isNotFree != CW_TRUE)
			continue;
		//if(gWTPs[i].currentState == CW_ENTER_RUN)
		if(gWTPs[i].isNotFree == CW_TRUE && gWTPs[i].currentState == CW_ENTER_RUN)
		{
			for (j = 0; j < MAC_ADDR_LEN; j++) 
			{
				if (gWTPs[WTPIndex].MAC[j] == gWTPs[i].MAC[j]) 
				{
					if (j == (MAC_ADDR_LEN - 1)) {
						//fix 0,2 error
						CWLog("MAC:%x:%x:%x:%x:%x:%x ,repeat online, online apId = %d, new apId = %d, kill new !",
								(u_char) gWTPs[i].MAC[0],
								(u_char) gWTPs[i].MAC[1],
								(u_char) gWTPs[i].MAC[2],
								(u_char) gWTPs[i].MAC[3],
								(u_char) gWTPs[i].MAC[4],
								(u_char) gWTPs[i].MAC[5],
								i,
								WTPIndex);
						finded = 1;
						break;
					}
					continue;
				} 
				else
					break;
			}
		}
		if(finded ==1)
		{
			break;
		}
	}

	if(finded ==1)
	{
		//_CWCloseThread(WTPIndex);
		gWTPs[WTPIndex].isRequestClose = CW_TRUE;
		//CWThreadMutexLock(&gWTPs[WTPIndex].interfaceMutex);
		//CWSignalThreadCondition(&gWTPs[WTPIndex].interfaceWait);
		//CWThreadMutexUnlock(&gWTPs[WTPIndex].interfaceMutex);
	}
	
	
	CWMsgElemData *auxData;
	if (ACIpv4List) {
		CW_CREATE_OBJECT_ERR(auxData, CWMsgElemData, return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
		auxData->type = CW_MSG_ELEMENT_AC_IPV4_LIST_CW_TYPE;
		auxData->value = 0;
		CWAddElementToList(&msgElemList, auxData);
	}
	if (ACIpv6List) {
		CW_CREATE_OBJECT_ERR(auxData, CWMsgElemData, return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
		auxData->type = CW_MSG_ELEMENT_AC_IPV6_LIST_CW_TYPE;
		auxData->value = 0;
		CWAddElementToList(&msgElemList, auxData);
	}
	if (resultCode) {
		CW_CREATE_OBJECT_ERR(auxData, CWMsgElemData, return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
		auxData->type = CW_MSG_ELEMENT_RESULT_CODE_CW_TYPE;
		auxData->value = resultCodeValue;
		CWAddElementToList(&msgElemList, auxData);
	}
	/*
	 if(sessionID){
	 CW_CREATE_OBJECT_ERR(auxData, CWMsgElemData, return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
	 auxData->type =  CW_MSG_ELEMENT_SESSION_ID_CW_TYPE;
	 auxData->value = CWRandomIntInRange(0, INT_MAX);
	 CWAddElementToList(&msgElemList,auxData);
	 }
	 */

	/* random session ID */
	if (!(CWAssembleJoinResponse(&(gWTPs[WTPIndex].messages),
			&(gWTPs[WTPIndex].messagesCount), gWTPs[WTPIndex].pathMTU, seqNum,
			msgElemList))) {

		CWDeleteList(&msgElemList, CWProtocolDestroyMsgElemData);
		return CW_FALSE;
	}

	CWDeleteList(&msgElemList, CWProtocolDestroyMsgElemData);

	if (!CWACSendFragments(WTPIndex)) {
		return CW_FALSE;
	}

	/* Destroy JoinStatePending timer */
	if(!CWErr(CWTimerCancel(&(gWTPs[WTPIndex].currentTimer)))) {
		CWLog("%s %d [%d] CWTimerCancel Fail, close thread!",__FILE__,__LINE__,WTPIndex);
		gWTPs[WTPIndex].isRequestClose = CW_TRUE;
	}

	if(!CWErr(CWTimerRequest(gCWConfigStatePendingTimer,
				 &(gWTPs[WTPIndex].thread),
				 &(gWTPs[WTPIndex].currentTimer),
				 CW_CRITICAL_TIMER_EXPIRED_SIGNAL))) {
		CWLog("%s %d [%d] CWTimerRequest Fail, close thread!",__FILE__,__LINE__,WTPIndex);
		gWTPs[WTPIndex].isRequestClose = CW_TRUE;
	}
	
	gWTPs[WTPIndex].currentState = CW_ENTER_CONFIGURE;

	return CW_TRUE;
}

/*
 * Assemble Join Response.
 */
CWBool CWAssembleJoinResponse(CWProtocolMessage **messagesPtr,
			      int *fragmentsNumPtr,
			      int PMTU,
			      int seqNum,
			      CWList msgElemList) {

	CWProtocolMessage *msgElems= NULL;
	int msgElemCount = 0;
	/* Result code is not included because it's already
	 * in msgElemList. Control IPv6 to be added.
	 */
	const int mandatoryMsgElemCount=3;
	CWProtocolMessage *msgElemsBinding= NULL;
	const int msgElemBindingCount=0;
	int i;
	CWListElement *current;
	int k = -1;
	
	if(messagesPtr == NULL || fragmentsNumPtr == NULL || msgElemList == NULL) 
		return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	msgElemCount = CWCountElementInList(msgElemList);

	CW_CREATE_PROTOCOL_MSG_ARRAY_ERR(msgElems,
					 msgElemCount + mandatoryMsgElemCount,
					 return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););

	CWDebugLog("Assembling Join Response...");
	
	if((!(CWAssembleMsgElemACDescriptor(&(msgElems[++k])))) ||
	   (!(CWAssembleMsgElemACName(&(msgElems[++k])))) ||
	   (!(CWAssembleMsgElemCWControlIPv4Addresses(&(msgElems[++k]))))	  
	) {
		CWErrorHandleLast();
		int i;
		for(i = 0; i <= k; i++) {CW_FREE_PROTOCOL_MESSAGE(msgElems[i]);}
		CW_FREE_OBJECT(msgElems);
		/* error will be handled by the caller */
		return CW_FALSE;
	} 

	current=msgElemList;
	for (i=0; i<msgElemCount; i++) {

                switch (((CWMsgElemData *) (current->data))->type) {

			case CW_MSG_ELEMENT_AC_IPV4_LIST_CW_TYPE:
				if (!(CWAssembleMsgElemACIPv4List(&(msgElems[++k]))))
					goto cw_assemble_error;	
				break;			
			case CW_MSG_ELEMENT_AC_IPV6_LIST_CW_TYPE:
				if (!(CWAssembleMsgElemACIPv6List(&(msgElems[++k]))))
					goto cw_assemble_error;
				break;
			case CW_MSG_ELEMENT_RESULT_CODE_CW_TYPE:
				if (!(CWAssembleMsgElemResultCode(&(msgElems[++k]), ((CWMsgElemData *) current->data)->value)))
					goto cw_assemble_error;
				break;
			/*
			case CW_MSG_ELEMENT_SESSION_ID_CW_TYPE:
				if (!(CWAssembleMsgElemSessionID(&(msgElems[++k]), ((CWMsgElemData *) current->data)->value)))
					goto cw_assemble_error;
				break;
			*/
                        default: {
                                int j;
                                for(j = 0; j <= k; j++) { CW_FREE_PROTOCOL_MESSAGE(msgElems[j]);}
                                CW_FREE_OBJECT(msgElems);
                                return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Unrecognized Message Element for Join Response Message");
				break;
		        }
                }
		current = current->next;
	}

	if (!(CWAssembleMessage(messagesPtr,
				fragmentsNumPtr,
				PMTU,
				seqNum,
				CW_MSG_TYPE_VALUE_JOIN_RESPONSE,
				msgElems,
				msgElemCount + mandatoryMsgElemCount,
				msgElemsBinding,
				msgElemBindingCount,
#ifdef CW_NO_DTLS
				CW_PACKET_PLAIN
#else				
				CW_PACKET_CRYPT
#endif
				)))
		return CW_FALSE;

	CWDebugLog("Join Response Assembled");
	
	return CW_TRUE;

cw_assemble_error:
	{
		int i;
		for(i = 0; i <= k; i++) { CW_FREE_PROTOCOL_MESSAGE(msgElems[i]);}
		CW_FREE_OBJECT(msgElems);
		/* error will be handled by the caller */
		return CW_FALSE;
	}
	return CW_TRUE;
}

/* 
 * Parses Join Request.
 */
CWBool CWParseJoinRequestMessage(char *msg,
				 int len,
				 int *seqNumPtr,
				 CWProtocolJoinRequestValues *valuesPtr) {

	CWControlHeaderValues controlVal;
	int offsetTillMessages;
	CWProtocolMessage completeMsg;
	
	if(msg == NULL || seqNumPtr == NULL || valuesPtr == NULL) 
		return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	CWDebugLog("Parse Join Request");
	
	completeMsg.msg = msg;
	completeMsg.offset = 0;
		
	if(!(CWParseControlHeader(&completeMsg, &controlVal)))
		/* will be handled by the caller */
		return CW_FALSE;

	/* different type */
	if(controlVal.messageTypeValue != CW_MSG_TYPE_VALUE_JOIN_REQUEST)
	{
		CWLog("messageTypeValue=%d", controlVal.messageTypeValue);
		return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Message is not Join Request as Expected");
	}
	
	*seqNumPtr = controlVal.seqNum;
	/* skip timestamp */
	controlVal.msgElemsLen -= CW_CONTROL_HEADER_OFFSET_FOR_MSG_ELEMS;
	offsetTillMessages = completeMsg.offset;
	
	/* parse message elements */
	while((completeMsg.offset-offsetTillMessages) < controlVal.msgElemsLen) {

		unsigned short int elemType = 0;/* = CWProtocolRetrieve32(&completeMsg); */
		unsigned short int elemLen =0 ;	/* = CWProtocolRetrieve16(&completeMsg); */
		
		CWParseFormatMsgElem(&completeMsg,&elemType,&elemLen);
		
		/* CWDebugLog("Parsing Message Element: %u, elemLen: %u", elemType, elemLen); */
									
		switch(elemType) {
			case CW_MSG_ELEMENT_LOCATION_DATA_CW_TYPE:
				if(!(CWParseLocationData(&completeMsg, elemLen, &(valuesPtr->location)))) 
					/* will be handled by the caller */
					return CW_FALSE;
				break;
			case CW_MSG_ELEMENT_WTP_BOARD_DATA_CW_TYPE:
				if(!(CWParseWTPBoardData(&completeMsg, elemLen, &(valuesPtr->WTPBoardData)))) 
					/* will be handled by the caller */
					return CW_FALSE;
				break; 
			case CW_MSG_ELEMENT_SESSION_ID_CW_TYPE:
				if(!(CWParseSessionID(&completeMsg, elemLen, valuesPtr)))
					/* will be handled by the caller */
					return CW_FALSE;
				break;
			case CW_MSG_ELEMENT_WTP_DESCRIPTOR_CW_TYPE:
				if(!(CWParseWTPDescriptor(&completeMsg, elemLen, &(valuesPtr->WTPDescriptor))))
					/* will be handled by the caller */
					return CW_FALSE;
				break;
			case CW_MSG_ELEMENT_WTP_IPV4_ADDRESS_CW_TYPE:
				if(!(CWParseWTPIPv4Address(&completeMsg, elemLen, valuesPtr)))
					/* will be handled by the caller */
					return CW_FALSE;
				break;
			case CW_MSG_ELEMENT_WTP_NAME_CW_TYPE:
				if(!(CWParseWTPName(&completeMsg, elemLen, &(valuesPtr->name))))
					/* will be handled by the caller */
					return CW_FALSE;
				break;
			case CW_MSG_ELEMENT_WTP_FRAME_TUNNEL_MODE_CW_TYPE:
				if(!(CWParseWTPFrameTunnelMode(&completeMsg, elemLen, &(valuesPtr->frameTunnelMode))))
					/* will be handled by the caller */
					return CW_FALSE;
				break;
			case CW_MSG_ELEMENT_WTP_MAC_TYPE_CW_TYPE:
				if(!(CWParseWTPMACType(&completeMsg, elemLen, &(valuesPtr->MACType))))
					/* will be handled by the caller */
					return CW_FALSE;
				break;
			default:
				return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Unrecognized Message Element");
		}
		/*CWDebugLog("bytes: %d/%d", (completeMsg.offset-offsetTillMessages), controlVal.msgElemsLen);*/
	}
	
	if (completeMsg.offset != len) 
		return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Garbage at the End of the Message");
		
	return CW_TRUE;
}

CWBool CWSaveJoinRequestMessage(CWProtocolJoinRequestValues *joinRequest,
				CWWTPProtocolManager *WTPProtocolManager) {

	CWDebugLog("Saving Join Request...");
	
	if(joinRequest == NULL || WTPProtocolManager == NULL)
		return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	if ((joinRequest->location)!= NULL) {

		CW_FREE_OBJECT(WTPProtocolManager->locationData);
		WTPProtocolManager->locationData= joinRequest->location;
	}
	else 
		return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	if ((joinRequest->name)!= NULL) {

		CW_FREE_OBJECT(WTPProtocolManager->name);
		WTPProtocolManager->name= joinRequest->name;
	}
	else 
		return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	CW_FREE_OBJECT((WTPProtocolManager->WTPBoardData).vendorInfos);
	WTPProtocolManager->WTPBoardData = joinRequest->WTPBoardData;

	WTPProtocolManager->sessionID= joinRequest->sessionID;
	WTPProtocolManager->ipv4Address= joinRequest->addr;
	
	WTPProtocolManager->descriptor= joinRequest->WTPDescriptor;
	WTPProtocolManager->radiosInfo.radioCount = (joinRequest->WTPDescriptor).radiosInUse;
	CW_FREE_OBJECT(WTPProtocolManager->radiosInfo.radiosInfo);

	CW_CREATE_ARRAY_ERR(WTPProtocolManager->radiosInfo.radiosInfo, 
			    WTPProtocolManager->radiosInfo.radioCount, 
			    CWWTPRadioInfoValues,
			    return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
	
	int i;

	for(i=0; i< WTPProtocolManager->radiosInfo.radioCount; i++) {

		WTPProtocolManager->radiosInfo.radiosInfo[i].radioID = i;
                /*WTPProtocolManager->radiosInfo.radiosInfo[i].stationCount = 0;*/
		/* default value for CAPWAP */
                WTPProtocolManager->radiosInfo.radiosInfo[i].adminState = ENABLED;
                WTPProtocolManager->radiosInfo.radiosInfo[i].adminCause = AD_NORMAL;
                WTPProtocolManager->radiosInfo.radiosInfo[i].operationalState = DISABLED;
                WTPProtocolManager->radiosInfo.radiosInfo[i].operationalCause = OP_NORMAL;
                WTPProtocolManager->radiosInfo.radiosInfo[i].TxQueueLevel = 0;
                WTPProtocolManager->radiosInfo.radiosInfo[i].wirelessLinkFramesPerSec = 0; 
	}
	CWDebugLog("Join Request Saved");
	return CW_TRUE;
}
