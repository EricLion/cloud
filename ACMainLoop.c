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
 *           Daniele De Sanctis (danieledesanctis@gmail.com)									* 
 *	         Antonio Davoli (antonio.davoli@gmail.com)											*
 ************************************************************************************************/

#include "CWAC.h"
#include "CWStevens.h"
#include "BELib.h"

#ifdef DMALLOC
#include "../dmalloc-5.5.0/dmalloc.h"
#endif

/* index of the current thread in the global array */
CWThreadSpecific gIndexSpecific;

int gCWWaitJoin = CW_WAIT_JOIN_DEFAULT;

CW_THREAD_RETURN_TYPE CWManageWTP(void *arg);
CW_THREAD_RETURN_TYPE CWManageTimers(void *arg);
void CWCriticalTimerExpiredHandler(int arg);
void CWSoftTimerExpiredHandler(int arg);

void CWACManageIncomingPacket(CWSocket sock,
			      char *buf,
			      int len,
			      int incomingInterfaceIndex,
			      CWNetworkLev4Address *addrPtr);
//void _CWCloseThread(int i);
void CWResetWTPProtocolManager(CWWTPProtocolManager *WTPProtocolManager);
__inline__ CWWTPManager *CWWTPByName(const char *addr);
__inline__ CWWTPManager *CWWTPByAddress(CWNetworkLev4Address *addressPtr,
					CWSocket sock);


void CWACEnterMainLoop() {

	struct sigaction act;
	
	CWLog("AC enters in the MAIN_LOOP");
	
	/* set signals
	 * all the thread we spawn will inherit these settings
	 */

        /*
         * BUG UMR03
         *
         * 20/10/2009 - Donato Capitella 
         */
        sigemptyset(&act.sa_mask);

	act.sa_flags = 0;
	/* called when a timer requested by the thread has expired */
	act.sa_handler = CWCriticalTimerExpiredHandler;
	sigaction(CW_CRITICAL_TIMER_EXPIRED_SIGNAL, &act, NULL);
	
	act.sa_flags = 0;
	/* called when a timer requested by the thread has expired */
	act.sa_handler = CWSoftTimerExpiredHandler;
	sigaction(CW_SOFT_TIMER_EXPIRED_SIGNAL, &act, NULL);
	
	/* signals will be unblocked by the threads that needs timers */
	CWThreadSetSignals(SIG_BLOCK, 2, CW_CRITICAL_TIMER_EXPIRED_SIGNAL,
 					 CW_SOFT_TIMER_EXPIRED_SIGNAL);

	if(!(CWThreadCreateSpecific(&gIndexSpecific, NULL))) {
		CWLog("Critical Error With Thread Data");
		exit(1);
	}
	
	CWThread thread_interface;
	if(!CWErr(CWCreateThread(&thread_interface, CWInterface, NULL))) {
		CWLog("Error starting Interface Thread");
		exit(1);
	}

	CW_REPEAT_FOREVER {
		/* CWACManageIncomingPacket will be called 
		 * when a new packet is ready to be read 
		 */
		if(!CWErr(CWNetworkUnsafeMultiHomed(&gACSocket, 
						    CWACManageIncomingPacket,
						    CW_FALSE)))
			{
				CWLog("Error CWNetworkUnsafeMultiHomed !!!");
				exit(1);
			}

	}
	CWLog("CWACEnterMainLoop Finish !!!");
}

/* argument passed to the thread func */
typedef struct {
	int index;
	CWSocket sock;
	int interfaceIndex;
} CWACThreadArg;

/*
 * This callback function is called when there is something to read in a 
 * CWMultiHomedSocket (see ACMultiHomed.c).
 * 
 * Params: sock,	is the socket that can receive the packet and it can be
 * 			used to reply.
 * 	   buf,		(array of len chars) contains the packet which is ready
 * 	   		on the socket's queue (obtained with MSG_PEEK).
 *	   incomingInterfaceIndex,  is the index (different from the system 
 *	   			    index, see ACMultiHomed.c) of the interface
 *	   			    the packet was sent to, in the array returned
 *	   			    by CWNetworkGetInterfaceAddresses. If the
 *	   			    packet was sent to a broadcast/multicast address,
 *	   			    incomingInterfaceIndex is -1.
 */
void CWACManageIncomingPacket(CWSocket sock,
			      char *buf,
			      int readBytes,
			      int incomingInterfaceIndex,
			      CWNetworkLev4Address *addrPtr) {

	CWWTPManager *wtpPtr = NULL;
	char* pData;
		
	/* check if sender address is known */
	wtpPtr = CWWTPByAddress(addrPtr, sock);
	
	if(wtpPtr != NULL) {
		/* known WTP */
		/* Clone data packet */
		CW_CREATE_OBJECT_SIZE_ERR(pData, readBytes, { CWLog("Out Of Memory"); return; });
		memcpy(pData, buf, readBytes);
		//CWLog("F:%s,L:%d",__FILE__,__LINE__);
		CWLockSafeList(wtpPtr->packetReceiveList);
		//CWLog("F:%s,L:%d",__FILE__,__LINE__);
		CWAddElementToSafeListTail(wtpPtr->packetReceiveList, pData, readBytes);
		//CWLog("F:%s,L:%d",__FILE__,__LINE__);
		CWUnlockSafeList(wtpPtr->packetReceiveList);
		//CWLog("F:%s,L:%d",__FILE__,__LINE__);
	} else { 
		/* unknown WTP */
		int seqNum, tmp;
		CWDiscoveryRequestValues values;
		
		if(!CWErr(CWThreadMutexLock(&gActiveWTPsMutex))) 
			exit(1);
			
		tmp = gActiveWTPs;
		//CWThreadMutexUnlock(&gActiveWTPsMutex);

		if(gActiveWTPs >= gMaxWTPs) {

			CWLog("Too many WTPs");
			CWThreadMutexUnlock(&gActiveWTPsMutex);
			return;
		}
		CWThreadMutexUnlock(&gActiveWTPsMutex);
		CWLog("\n");	
		
		if(CWErr(CWParseDiscoveryRequestMessage(buf, readBytes, &seqNum, &values))) {
		
			CWProtocolMessage *msgPtr;
		
			CWLog("\n");
			CWLog("######### Discovery State #########");

			CWUseSockNtop(addrPtr, CWLog("CAPWAP Discovery Request from %s", str););
	
			/* don't add this WTP to our list to minimize DoS 
			 * attacks (will be added after join) 
			 */

			/* destroy useless values */
			CWDestroyDiscoveryRequestValues(&values);
			
			/* send response to WTP 
			 * note: we can consider reassembling only changed part
			 * AND/OR do this in a new thread.
			 */
			if(!CWErr(CWAssembleDiscoveryResponse(&msgPtr, seqNum))) {
				/* 
				 * note: maybe an out-of-memory memory error 
				 * can be resolved without exit()-ing by 
				 * killing some thread or doing other funky 
				 * things.
				 */
				CWLog("Critical Error Assembling Discovery Response");
				exit(1);
			}

			if(!CWErr(CWNetworkSendUnsafeUnconnected(sock,
								 addrPtr,
								 (*msgPtr).msg,
								 (*msgPtr).offset))) {

				CWLog("Critical Error Sending Discovery Response");
				exit(1);
			}
			
			CW_FREE_PROTOCOL_MESSAGE(*msgPtr);
			CW_FREE_OBJECT(msgPtr);
		} else { 
			/* this isn't a Discovery Request */
			int i;
			CWACThreadArg *argPtr;
			
			CWUseSockNtop(addrPtr, CWDebugLog("Possible Client Hello from %s", str););
			
			if(!CWErr(CWThreadMutexLock(&gWTPsMutex))) exit(1);
			/* look for the first free slot */
			for(i = 0; i < CW_MAX_WTP && gWTPs[i].isNotFree; i++);
	
			CW_COPY_NET_ADDR_PTR(&(gWTPs[i].address), addrPtr);
			gWTPs[i].isNotFree = CW_TRUE;
			gWTPs[i].isRequestClose = CW_FALSE;
			CWThreadMutexUnlock(&gWTPsMutex);

			/* Capwap receive packets list */
			if (!CWErr(CWCreateSafeList(&gWTPs[i].packetReceiveList))) {

				if(!CWErr(CWThreadMutexLock(&gWTPsMutex))) 
					exit(1);
				gWTPs[i].isNotFree = CW_FALSE;
				CWThreadMutexUnlock(&gWTPsMutex);
				return;
			}
			
			CWSetMutexSafeList(gWTPs[i].packetReceiveList, 
					   &gWTPs[i].interfaceMutex);
			CWSetConditionSafeList(gWTPs[i].packetReceiveList,
					       &gWTPs[i].interfaceWait);

			CW_CREATE_OBJECT_ERR(argPtr, CWACThreadArg, { CWLog("Out Of Memory"); return; });

			argPtr->index = i;
			argPtr->sock = sock;
			argPtr->interfaceIndex = incomingInterfaceIndex;
						
			/* 
			 * If the packet was addressed to a broadcast address,
			 * just choose an interface we like (note: we can consider
			 * a bit load balancing instead of hard-coding 0-indexed 
			 * interface). Btw, Join Request should not really be 
			 * accepted if addressed to a broadcast address, so we 
			 * could simply discard the packet and go on.
			 * If you leave this code, the WTP Count will increase 
			 * for the interface we hard-code here, even if it is not
			 * necessary the interface we use to send packets to that
			 * WTP. If we really want to accept Join Request from 
			 * broadcast address, we can consider asking to the kernel
			 * which interface will be used to send the packet to a 
			 * specific address (if it remains the same) and than 
			 * increment WTPCount for that interface instead of 0-indexed one.
			 */
			if (argPtr->interfaceIndex < 0) argPtr->interfaceIndex = 0; 
			
			/* create the thread that will manage this WTP */
			if(!CWErr(CWCreateThread(&(gWTPs[i].thread), CWManageWTP, argPtr))) {

				CW_FREE_OBJECT(argPtr);
				if(!CWErr(CWThreadMutexLock(&gWTPsMutex))) 
					exit(1);
				
				CWDestroySafeList(&gWTPs[i].packetReceiveList);
				gWTPs[i].isNotFree = CW_FALSE;
				CWThreadMutexUnlock(&gWTPsMutex);
				CWDebugLog("CWCreateThread CWManageWTP Fail! ");
				return;
			}
	
			/* Clone data packet */
			CW_CREATE_OBJECT_SIZE_ERR(pData, readBytes, { CWLog("Out Of Memory"); return; });
			memcpy(pData, buf, readBytes);

			CWLockSafeList(gWTPs[i].packetReceiveList);
			CWAddElementToSafeListTail(gWTPs[i].packetReceiveList,
						   pData,
						   readBytes);
			CWUnlockSafeList(gWTPs[i].packetReceiveList);
		}
	}
}

/*
 * Simple job: see if we have a thread that is serving address *addressPtr
 */
__inline__ CWWTPManager *CWWTPByAddress(CWNetworkLev4Address *addressPtr, CWSocket sock) {

	int i;

	CWLog("CWWTPByAddress  sock = %d",sock);
	if(addressPtr == NULL) return NULL;
	//CWLog("addr = %s",inet_ntoa(((struct sockaddr_in *) addressPtr)->sin_addr));
	//CWLog("port = %d",((struct sockaddr_in *) addressPtr)->sin_port);
	CWThreadMutexLock(&gWTPsMutex);
	for(i = 0; i < CW_MAX_WTP; i++) {
		//CWLog("gWTPs[i].isNotFree = %d",gWTPs[i].isNotFree);
		//CWLog("gWTPs[i].socket = %d",gWTPs[i].socket);
		if(gWTPs[i].isNotFree && 
		   &(gWTPs[i].address) != NULL &&
		   !sock_cmp_addr((struct sockaddr*)addressPtr, 
			   	  (struct sockaddr*)&(gWTPs[i].address), 
				  sizeof(CWNetworkLev4Address)) &&				
	   	   !sock_cmp_port((struct sockaddr*)addressPtr, 
			   	  (struct sockaddr*)&(gWTPs[i].address), 
				  sizeof(CWNetworkLev4Address)) &&				
	   	   gWTPs[i].socket == sock) { 
			
			/* we treat a WTP that sends packet to a different 
			 * AC's interface as a new WTP
			 */
			CWThreadMutexUnlock(&gWTPsMutex);
			return &(gWTPs[i]);
		}
	}
	CWThreadMutexUnlock(&gWTPsMutex);
	
	return NULL;
}

/* 
 * Session's thread function: each thread will manage a single session 
 * with one WTP.
 */
CW_THREAD_RETURN_TYPE CWManageWTP(void *arg) {

	int 		i = ((CWACThreadArg*)arg)->index;
	CWSocket 	sock = ((CWACThreadArg*)arg)->sock;
	int 		interfaceIndex = ((CWACThreadArg*)arg)->interfaceIndex;
	
	CW_FREE_OBJECT(arg);
	
	if(!(CWThreadSetSpecific(&gIndexSpecific, &i))) {

		CWLog("Critical Error with Thread Data");
		_CWCloseThread(i);
	}

	//if(!CWErr(CWThreadMutexLock(&gActiveWTPsMutex))) 
		//exit(1);
//ap in run state as active
//	gActiveWTPs++;

	gInterfaces[interfaceIndex].WTPCount++;
	CWUseSockNtop(((struct sockaddr*) &(gInterfaces[interfaceIndex].addr)),
				  CWDebugLog("One more WTP on %s (%d)", str, interfaceIndex);
				  );
	
	//CWThreadMutexUnlock(&gActiveWTPsMutex);
//������ȡ���ú��?
	CWACInitBinding(i);
	
	gWTPs[i].interfaceIndex = interfaceIndex;
	gWTPs[i].socket = sock;
	
	gWTPs[i].fragmentsList = NULL;
	/* we're in the join state for this session */
	gWTPs[i].currentState = CW_ENTER_JOIN;
	gWTPs[i].subState = CW_DTLS_HANDSHAKE_IN_PROGRESS;
	
	/**** ACInterface ****/
	gWTPs[i].interfaceCommandProgress = CW_FALSE;
	gWTPs[i].interfaceCommand = NO_CMD;
	CWDestroyThreadMutex(&gWTPs[i].interfaceMutex);	
	CWCreateThreadMutex(&gWTPs[i].interfaceMutex);
	CWDestroyThreadMutex(&gWTPs[i].interfaceSingleton);	
	CWCreateThreadMutex(&gWTPs[i].interfaceSingleton);
	CWDestroyThreadCondition(&gWTPs[i].interfaceWait);	
	CWCreateThreadCondition(&gWTPs[i].interfaceWait);
	CWDestroyThreadCondition(&gWTPs[i].interfaceComplete);	
	CWCreateThreadCondition(&gWTPs[i].interfaceComplete);
	gWTPs[i].qosValues = NULL;
	/**** ACInterface ****/

	gWTPs[i].messages = NULL;
 	gWTPs[i].messagesCount = 0;
 	gWTPs[i].isRetransmitting = CW_FALSE;
	gWTPs[i].retransmissionCount = 0;

	//MAC
	memset(gWTPs[i].MAC, 0, MAC_ADDR_LEN);

	//BE: connectEvent
	gWTPs[i].isConnect = CW_FALSE;

	CWResetWTPProtocolManager(&(gWTPs[i].WTPProtocolManager));

	CWLog("New Session");
CWLog("F:%s L:%d",__FILE__,__LINE__);
	/* start WaitJoin timer */
	if(!CWErr(CWTimerRequest(gCWWaitJoin,
				 &(gWTPs[i].thread),
				 &(gWTPs[i].currentTimer),
				 CW_CRITICAL_TIMER_EXPIRED_SIGNAL))) {

		CWCloseThread();
	}
//��Ҫ������ʱ������
#ifndef CW_NO_DTLS
	CWDebugLog("Init DTLS Session");

 	if(!CWErr(CWSecurityInitSessionServer(&gWTPs[i],
					      sock,
					      gACSecurityContext,
					      &((gWTPs[i]).session),
					      &(gWTPs[i].pathMTU)))) {

		CWTimerCancel(&(gWTPs[i].currentTimer));
		CWCloseThread();
	}
#endif
	(gWTPs[i]).subState = CW_WAITING_REQUEST;

	if(gCWForceMTU > 0) gWTPs[i].pathMTU = gCWForceMTU;

	CWDebugLog("Path MTU for this Session: %d",  gWTPs[i].pathMTU);
	
	CW_REPEAT_FOREVER
	  {
		int readBytes;
		CWProtocolMessage msg;
		CWBool dataFlag = CW_FALSE;

		msg.msg = NULL;
		msg.offset = 0;

		/* Wait WTP action */
		CWThreadMutexLock(&gWTPs[i].interfaceMutex);

		while ((gWTPs[i].isRequestClose == CW_FALSE) &&
		       (CWGetCountElementFromSafeList(gWTPs[i].packetReceiveList) == 0) &&
		       (gWTPs[i].interfaceCommand == NO_CMD)) {

			 /*TODO: Check system */
			CWWaitThreadCondition(&gWTPs[i].interfaceWait, 
					      &gWTPs[i].interfaceMutex);
		}
//CWLog("F:%s L:%d",__FILE__,__LINE__);
		CWThreadMutexUnlock(&gWTPs[i].interfaceMutex);

		if (gWTPs[i].isRequestClose) {

			CWLog("Request close thread");
			_CWCloseThread(i);
		}

		CWThreadSetSignals(SIG_BLOCK, 
				   2,
				   CW_SOFT_TIMER_EXPIRED_SIGNAL,
				   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);
//CWLog("F:%s L:%d",__FILE__,__LINE__);
		if (CWGetCountElementFromSafeList(gWTPs[i].packetReceiveList) > 0) {

			CWBool 	bCrypt = CW_FALSE;
			char	*pBuffer;

			CWThreadMutexLock(&gWTPs[i].interfaceMutex);

			pBuffer = (char *)CWGetHeadElementFromSafeList(gWTPs[i].packetReceiveList, NULL);

			if ((pBuffer[0] & 0x0f) == CW_PACKET_CRYPT)
			  bCrypt = CW_TRUE;

			CWThreadMutexUnlock(&gWTPs[i].interfaceMutex);
			
			if (bCrypt) {
				CWDebugLog("Receive a security packet");
				CWLog("Don't parse security packet,drop it");
#ifdef CW_NO_DTLS
				//core fix
				//CWThreadSetSignals(SIG_UNBLOCK, 1, CW_SOFT_TIMER_EXPIRED_SIGNAL);
				//continue;
				 CWLog("F:%s L:%d",__FILE__,__LINE__);
#else
				//CWSecurityReceive core ????
			  if(!CWErr(CWSecurityReceive(gWTPs[i].session,
										  gWTPs[i].buf,
										  CW_BUFFER_SIZE - 1,
										  &readBytes))) {
					/* error */
				CWDebugLog("Error during security receive");
				CWLog("Error during security receive");
				CWThreadSetSignals(SIG_UNBLOCK, 1, CW_SOFT_TIMER_EXPIRED_SIGNAL);
				continue;
			  }
#endif
			  CWLog("F:%s L:%d",__FILE__,__LINE__);
			}
			else {
			  CWThreadMutexLock(&gWTPs[i].interfaceMutex);
			  pBuffer = (char*)CWRemoveHeadElementFromSafeList(gWTPs[i].packetReceiveList, &readBytes);
			  CWThreadMutexUnlock(&gWTPs[i].interfaceMutex);
			  
			  memcpy(gWTPs[i].buf, pBuffer, readBytes);
			  CW_FREE_OBJECT(pBuffer);
			}
			CWLog("F:%s L:%d",__FILE__,__LINE__);
			if(!CWProtocolParseFragment(gWTPs[i].buf,
						    readBytes,
						    &(gWTPs[i].fragmentsList),
						    &msg,
						    &dataFlag)) {

				if(CWErrorGetLastErrorCode() == CW_ERROR_NEED_RESOURCE) {

					CWDebugLog("Need At Least One More Fragment");
				} 
				else {
					CWErrorHandleLast();
				}
				CWThreadSetSignals(SIG_UNBLOCK, 1, CW_SOFT_TIMER_EXPIRED_SIGNAL);
				continue;
			}
			CWLog("F:%s L:%d",__FILE__,__LINE__);
			switch(gWTPs[i].currentState) 
			{
				case CW_ENTER_JOIN:
				{
					/* we're inside the join state */
					if(!ACEnterJoin(i, &msg)) 
					{
						if(CWErrorGetLastErrorCode() == CW_ERROR_INVALID_FORMAT) 
						{
							/* Log and ignore other messages */
							CWErrorHandleLast();
							CWLog("Received something different from a Join Request");
						} 
						else 
						{
							/* critical error, close session */
							CWErrorHandleLast();
							CWThreadSetSignals(SIG_UNBLOCK, 1, CW_SOFT_TIMER_EXPIRED_SIGNAL);
							CWCloseThread();
						}
					}
					break;
				}
				case CW_ENTER_CONFIGURE:
				{
					if(!ACEnterConfigure(i, &msg)) 
					{
						CWLog("ACEnterConfigure Fail !!!");
						if(CWErrorGetLastErrorCode() == CW_ERROR_INVALID_FORMAT) 
						{
							/* Log and ignore other messages */
							CWErrorHandleLast();
							CWLog("Received something different from a Configure Request");
						} 
						else 
						{
							/* critical error, close session */
							CWErrorHandleLast();
							CWThreadSetSignals(SIG_UNBLOCK, 1, CW_SOFT_TIMER_EXPIRED_SIGNAL);
							CWLog("ACEnterConfigure critical error !!!");
							CWCloseThread();
						}
					}
					break;
				}
				case CW_ENTER_DATA_CHECK:
				{
					if(!ACEnterDataCheck(i, &msg)) 
					{
						if(CWErrorGetLastErrorCode() == CW_ERROR_INVALID_FORMAT) 
						{
							/* Log and ignore other messages */
							CWErrorHandleLast();
							CWLog("Received something different from a Change State Event Request");
						} 
						else 
						{
							/* critical error, close session */
							CWErrorHandleLast();
							CWThreadSetSignals(SIG_UNBLOCK, 1, CW_SOFT_TIMER_EXPIRED_SIGNAL);
							CWCloseThread();
						}
					}
					if(gWTPs[i].isConnect == CW_TRUE)
					{
						CWLog("[F:%s, L:%d]CW_ENTER_DATA_CHECK Connect begin...",__FILE__,__LINE__);
						 int seqNum = CWGetSeqNum();

	                                         if (CWAssembleConfigurationUpdateRequest(&(gWTPs[i].messages),
	                                                                                                                 &(gWTPs[i].messagesCount),
	                                                                                                                 gWTPs[i].pathMTU,
	                                                                                                                 seqNum, CONFIG_UPDATE_REQ_VENDOR_STATE_ELEMENT_TYPE)) {

	                                          if(CWACSendAcknowledgedPacket(i, CW_MSG_TYPE_VALUE_CONFIGURE_UPDATE_RESPONSE, seqNum))
								CWLog("[F:%s, L:%d]CONFIGURE_UPDATE_REQUEST Send...",__FILE__,__LINE__);		  	
	                                          else
	                                                CWACStopRetransmission(i);
	                                        }
					}
					
					break;
				}	
				case CW_ENTER_RUN:
				{
					CWLog("F:%s L:%d",__FILE__,__LINE__);
					if(!ACEnterRun(i, &msg, dataFlag)) 
					{
						CWLog("F:%s,L:%d",__FILE__,__LINE__);
						if(CWErrorGetLastErrorCode() == CW_ERROR_INVALID_FORMAT) 
						{
							/* Log and ignore other messages */
							CWErrorHandleLast();
							CWLog("--> Received something different from a valid Run Message");
						} 
						else 
						{
							/* critical error, close session */
							CWLog("--> Critical Error... closing thread");
							CWErrorHandleLast();
							CWThreadSetSignals(SIG_UNBLOCK, 1, CW_SOFT_TIMER_EXPIRED_SIGNAL);
							CWCloseThread();
						}
					}
					break;
				}
				default:
				{
					CWLog("Not Handled Packet");
					break;
				}
			}
			CW_FREE_PROTOCOL_MESSAGE(msg);
		}
		else {
		  CWThreadMutexLock(&gWTPs[i].interfaceMutex);
		  CWLog("[F:%s, L:%d]",__FILE__,__LINE__);
		  if (gWTPs[i].interfaceCommand != NO_CMD) {
			
			CWBool bResult = CW_FALSE;
			CWDebugLog("gWTPs[%d].interfaceCommand = %d", i,gWTPs[i].interfaceCommand);
			switch (gWTPs[i].interfaceCommand) {
			case QOS_CMD:
			  {
				int seqNum = CWGetSeqNum();

				/* CWDebugLog("~~~~~~seq num in Check: %d~~~~~~", seqNum); */
				if (CWAssembleConfigurationUpdateRequest(&(gWTPs[i].messages), 
														 &(gWTPs[i].messagesCount),
														 gWTPs[i].pathMTU,
														 seqNum, CONFIG_UPDATE_REQ_QOS_ELEMENT_TYPE)) {
				  
				  if(CWACSendAcknowledgedPacket(i, CW_MSG_TYPE_VALUE_CONFIGURE_UPDATE_RESPONSE, seqNum)) 
					bResult = CW_TRUE;
				  else
					CWACStopRetransmission(i);
				}
				break;
			  }
			//System Reset
			case CLEAR_CONFIG_MSG_CMD:
			  {
				int seqNum = CWGetSeqNum();
				
						/* Clear Configuration Request */
				if (CWAssembleClearConfigurationRequest(&(gWTPs[i].messages),
														&(gWTPs[i].messagesCount),
														gWTPs[i].pathMTU, seqNum)) {
				  
				  if(CWACSendAcknowledgedPacket(i, CW_MSG_TYPE_VALUE_CLEAR_CONFIGURATION_RESPONSE, seqNum)) 
								bResult = CW_TRUE;
				  else
					CWACStopRetransmission(i);
				}
				break;
			  }
			//System Reboot
			case SYSTEM_REBOOT_MSG_CMD:
			  {
				int seqNum = CWGetSeqNum();
				
						/* Clear Configuration Request */
				if (CWAssembleResetRequest(&(gWTPs[i].messages),
														&(gWTPs[i].messagesCount),
														gWTPs[i].pathMTU, seqNum)) {
				  
				  if(CWACSendAcknowledgedPacket(i, CW_MSG_TYPE_VALUE_RESET_RESPONSE, seqNum)) 
								bResult = CW_TRUE;
				  else
					CWACStopRetransmission(i);
				}
				break;
			  }
			/********************************************************
			 * 2009 Update:											*
			 *				New switch case for OFDM_CONTROL_CMD	*
			 ********************************************************/
			  
			case OFDM_CONTROL_CMD: 
				  {
					int seqNum = CWGetSeqNum();
					
					  if (CWAssembleConfigurationUpdateRequest(&(gWTPs[i].messages), 
														 &(gWTPs[i].messagesCount),
														 gWTPs[i].pathMTU,
														 seqNum, CONFIG_UPDATE_REQ_OFDM_ELEMENT_TYPE)) {
				  
					  if(CWACSendAcknowledgedPacket(i, CW_MSG_TYPE_VALUE_CONFIGURE_UPDATE_RESPONSE, seqNum)) 
						bResult = CW_TRUE;
					  else
						CWACStopRetransmission(i);
					}
				  break;
				  }
			/*Update 2009
				Added case to manage UCI configuration command*/
			case UCI_CONTROL_CMD: 
				  {
					int seqNum = CWGetSeqNum();
					
					  if (CWAssembleConfigurationUpdateRequest(&(gWTPs[i].messages), 
														 &(gWTPs[i].messagesCount),
														 gWTPs[i].pathMTU,
														 seqNum, CONFIG_UPDATE_REQ_VENDOR_UCI_ELEMENT_TYPE)) {
				  
					  if(CWACSendAcknowledgedPacket(i, CW_MSG_TYPE_VALUE_CONFIGURE_UPDATE_RESPONSE, seqNum)) 
						bResult = CW_TRUE;
					  else
						CWACStopRetransmission(i);
					}
				  break;
				  }
			case WTP_UPDATE_CMD:
				{
						CWLog("[F:%s, L:%d] ",__FILE__,__LINE__);	
					 int seqNum = CWGetSeqNum();

                                         if (CWAssembleConfigurationUpdateRequest(&(gWTPs[i].messages),
                                                                                                                 &(gWTPs[i].messagesCount),
                                                                                                                 gWTPs[i].pathMTU,
                                                                                                                 seqNum, CONFIG_UPDATE_REQ_VENDOR_WUM_ELEMENT_TYPE)) {

                                          if(CWACSendAcknowledgedPacket(i, CW_MSG_TYPE_VALUE_CONFIGURE_UPDATE_RESPONSE, seqNum))
                                                bResult = CW_TRUE;
                                          else
                                                CWACStopRetransmission(i);
                                        }
                                  break;
			

	
				}
			case WTP_CONFIG_CMD:
							{
								CWLog("[F:%s, L:%d]",__FILE__,__LINE__);
								 int seqNum = CWGetSeqNum();

			                                         if (CWAssembleConfigurationUpdateRequest(&(gWTPs[i].messages),
			                                                                                                                 &(gWTPs[i].messagesCount),
			                                                                                                                 gWTPs[i].pathMTU,
			                                                                                                                 seqNum, CONFIG_UPDATE_REQ_VENDOR_CONFIG_ELEMENT_TYPE)) {

			                                          if(CWACSendAcknowledgedPacket(i, CW_MSG_TYPE_VALUE_CONFIGURE_UPDATE_RESPONSE, seqNum))
			                                                bResult = CW_TRUE;
			                                          else
			                                                CWACStopRetransmission(i);
			                                        }
			                                  break;
							}

			case WTP_STATE_CMD:
							{
								CWLog("[F:%s, L:%d]WTP_STATE_CMD",__FILE__,__LINE__);
								 int seqNum = CWGetSeqNum();

			                                         if (CWAssembleConfigurationUpdateRequest(&(gWTPs[i].messages),
			                                                                                                                 &(gWTPs[i].messagesCount),
			                                                                                                                 gWTPs[i].pathMTU,
			                                                                                                                 seqNum, CONFIG_UPDATE_REQ_VENDOR_STATE_ELEMENT_TYPE)) {

			                                          if(CWACSendAcknowledgedPacket(i, CW_MSG_TYPE_VALUE_CONFIGURE_UPDATE_RESPONSE, seqNum))
			                                                bResult = CW_TRUE;
			                                          else
			                                                CWACStopRetransmission(i);
			                                        }
			                                  break;

							}
			//portal download
			case PORTAL_MSG_CMD:
							{
								CWLog("[F:%s, L:%d]PORTAL_MSG_CMD",__FILE__,__LINE__);
								 int seqNum = CWGetSeqNum();

			                                         if (CWAssembleConfigurationUpdateRequest(&(gWTPs[i].messages),
			                                                                                                                 &(gWTPs[i].messagesCount),
			                                                                                                                 gWTPs[i].pathMTU,
			                                                                                                                 seqNum, CONFIG_UPDATE_REQ_VENDOR_PORTAL_ELEMENT_TYPE)) {

			                                          if(CWACSendAcknowledgedPacket(i, CW_MSG_TYPE_VALUE_CONFIGURE_UPDATE_RESPONSE, seqNum))
			                                                bResult = CW_TRUE;
			                                          else
			                                                CWACStopRetransmission(i);
			                                        }
			                                  break;

							}
			}

				gWTPs[i].interfaceCommand = NO_CMD;

				if (bResult)
					gWTPs[i].interfaceCommandProgress = CW_TRUE;
				else {
					gWTPs[i].interfaceResult = 0;
					CWSignalThreadCondition(&gWTPs[i].interfaceComplete);
					CWDebugLog("Error sending command");
				}
			}
			CWThreadMutexUnlock(&gWTPs[i].interfaceMutex);
		}
		CWThreadSetSignals(SIG_UNBLOCK, 2, 
				   CW_SOFT_TIMER_EXPIRED_SIGNAL, 
				   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);
	}
	/********
	CW_REPEAT_FOREVER {
		if(gWTPs[i].currentState == CW_ENTER_RUN){
			//@@@
			CWThreadSetSignals(SIG_UNBLOCK, 3,CW_INCOMING_PACKET_SIGNAL,
							CW_CRITICAL_TIMER_EXPIRED_SIGNAL,
							CW_SOFT_TIMER_EXPIRED_SIGNAL);
			//@@@
			if (!CWACCheckSituation())
			{se
				
			}
		}
		else {
			CWDebugLog("Suspend");
			sigsuspend(&mymask);	// suspend this thread waiting for CW_INCOMING_PACKET_SIGNAL,
						// CW_CRITICAL_TIMER_EXPIRED_SIGNAL or CW_SOFT_TIMER_EXPIRED_SIGNAL.
			CWDebugLog("Signal Received");
		}
	}
	*********/
	CWDebugLog("CWMangeWtp over !!!");
}

void _CWCloseThread(int i) {
//BE: ap disconnect
	CWLog("_CWCloseThread apid =%d",i);
	char *beResp = NULL;
	int BESize;
	
	BEconnectEvent beConEve;

	CWLog("[F:%s, L:%d]  ",__FILE__,__LINE__);
	if(!CWErr(CWThreadMutexLock(&gActiveWTPsMutex)))
	{
		CWLog("_CWCloseThread CWThreadMutexLock fail,exit !");
		exit(1);
	}

		//num can't < 0
	if(gActiveWTPs && gWTPs[i].currentState == CW_ENTER_RUN)
	{
		CWLog("[F:%s, L:%d]  ",__FILE__,__LINE__);
		gActiveWTPs--;
		CWLog("_CWCloseThread gActiveWTPs = %d",gActiveWTPs);
		
		beConEve.type =htons( BE_CONNECT_EVENT);
		beConEve.length = Swap32(BE_CONNECT_EVENT_LEN);
		beConEve.state = BE_CONNECT_EVENT_DISCONNECT;
		BESize = BE_CONNECT_EVENT_LEN + BE_TYPELEN_LEN;
		
		beResp = AssembleBEheader((char*)&beConEve,&BESize,i,NULL);
		if(beResp)
		{
			SendBERequest(beResp,BESize);
			CW_FREE_OBJECT(beResp);
		}
		else
		{
			CWLog("Error AssembleBEheader !");
		}
	}
	
	CWLog("[F:%s, L:%d]  ",__FILE__,__LINE__);
	CWThreadMutexUnlock(&gActiveWTPsMutex);
	
 	CWThreadSetSignals(SIG_BLOCK, 2, 
			   CW_SOFT_TIMER_EXPIRED_SIGNAL, 
			   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);

	/**** ACInterface ****/
	if(!CWErr(CWThreadMutexLock(&gWTPsMutex)))
	{
		CWLog("_CWCloseThread CWThreadMutexLock fail,exit !");
		exit(1);
	}
	//CWThreadMutexLock(&gWTPsMutex);
	CWLog("[F:%s, L:%d]  ",__FILE__,__LINE__);
	gWTPs[i].qosValues=NULL;
	memset(gWTPs[i].MAC, 0, MAC_ADDR_LEN);
	gWTPs[i].isConnect = CW_FALSE;

	CWThreadMutexUnlock(&gWTPsMutex);
	/**** ACInterface ****/

	CWLog("[F:%s, L:%d]  ",__FILE__,__LINE__);
	gInterfaces[gWTPs[i].interfaceIndex].WTPCount--;

	CWUseSockNtop( ((struct sockaddr*)&(gInterfaces[gWTPs[i].interfaceIndex].addr)),
			CWLog("Remove WTP on Interface %s (%d)", str, gWTPs[i].interfaceIndex););
	
	
	CWDebugLog("Close Thread: %08x", (unsigned int)CWThreadSelf());
	
	if(gWTPs[i].subState != CW_DTLS_HANDSHAKE_IN_PROGRESS) {
	
		CWSecurityDestroySession(gWTPs[i].session);
	}
	
	/* this will do nothing if the timer isn't active */
	CWTimerCancel(&(gWTPs[i].currentTimer));

	CWLog("[F:%s, L:%d]  ",__FILE__,__LINE__);
	
	CWACStopRetransmission(i);

	if (gWTPs[i].interfaceCommandProgress == CW_TRUE) {

		CWThreadMutexLock(&gWTPs[i].interfaceMutex);
		
		gWTPs[i].interfaceResult = 1;
		gWTPs[i].interfaceCommandProgress = CW_FALSE;
		CWSignalThreadCondition(&gWTPs[i].interfaceComplete);

		CWThreadMutexUnlock(&gWTPs[i].interfaceMutex);
	}

	CWLog("[F:%s, L:%d]  ",__FILE__,__LINE__);
	if(!CWErr(CWThreadMutexLock(&gWTPsMutex)))
	{
		CWLog("_CWCloseThread CWThreadMutexLock fail,exit !");
		exit(1);
	}
	CWLog("[F:%s, L:%d]  ",__FILE__,__LINE__);
	gWTPs[i].session = NULL;
	gWTPs[i].subState = CW_DTLS_HANDSHAKE_IN_PROGRESS;
	CWDeleteList(&(gWTPs[i].fragmentsList), CWProtocolDestroyFragment);
	
	/* CW_FREE_OBJECT(gWTPs[i].configureReqValuesPtr); */
	
	CWCleanSafeList(gWTPs[i].packetReceiveList, free);
	CWDestroySafeList(gWTPs[i].packetReceiveList);
	
	
	gWTPs[i].isNotFree = CW_FALSE;
	CWThreadMutexUnlock(&gWTPsMutex);
	
	CWExitThread();
}

void CWCloseThread() {

	int *iPtr;
	
	if((iPtr = ((int*)CWThreadGetSpecific(&gIndexSpecific))) == NULL) {

		CWLog("Error Closing Thread");
		return;
	}
	
	_CWCloseThread(*iPtr);
}

void CWCriticalTimerExpiredHandler(int arg) {

	int *iPtr;

	CWThreadSetSignals(SIG_BLOCK, 2,
			   CW_SOFT_TIMER_EXPIRED_SIGNAL,
			   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);
 	
	CWDebugLog("Critical Timer Expired for Thread: %08x", (unsigned int)CWThreadSelf());
	CWDebugLog("Abort Session");
//	CWCloseThread();

	if((iPtr = ((int*)CWThreadGetSpecific(&gIndexSpecific))) == NULL) {

		CWLog("Error Handling Critical timer");
		CWThreadSetSignals(SIG_UNBLOCK, 2, 
				   CW_SOFT_TIMER_EXPIRED_SIGNAL,
				   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);
		return;
	}

	/* Request close thread */
	gWTPs[*iPtr].isRequestClose = CW_TRUE;
	CWSignalThreadCondition(&gWTPs[*iPtr].interfaceWait);
}

void CWSoftTimerExpiredHandler(int arg) {

	int *iPtr;

	CWThreadSetSignals(SIG_BLOCK, 2, 
			   CW_SOFT_TIMER_EXPIRED_SIGNAL,
			   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);

	CWDebugLog("Soft Timer Expired for Thread: %08x", 
		   (unsigned int)CWThreadSelf());
	
	if((iPtr = ((int*)CWThreadGetSpecific(&gIndexSpecific))) == NULL) {

		CWLog("Error Handling Soft timer");
		CWThreadSetSignals(SIG_UNBLOCK, 2, 
				   CW_SOFT_TIMER_EXPIRED_SIGNAL,
				   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);
		return;
	}

	CWLog("Handling Soft timer,apid =%d,gWTPs[%d].isRetransmitting=%d ",*iPtr,*iPtr,gWTPs[*iPtr].isRetransmitting);
	if((!gWTPs[*iPtr].isRetransmitting) || (gWTPs[*iPtr].messages == NULL)) {

		CWDebugLog("Soft timer expired but we are not retransmitting");
		CWThreadSetSignals(SIG_UNBLOCK, 2, 
				   CW_SOFT_TIMER_EXPIRED_SIGNAL,
				   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);
//		_CWCloseThread(*iPtr);
		return;
	}

	(gWTPs[*iPtr].retransmissionCount)++;
	
	CWDebugLog("Retransmission Count increases to %d", gWTPs[*iPtr].retransmissionCount);
	
	if(gWTPs[*iPtr].retransmissionCount >= gCWMaxRetransmit) 
	{
		CWDebugLog("Peer is Dead");
//		 _CWCloseThread(*iPtr);
		 /* Request close thread
		 */
		gWTPs[*iPtr].isRequestClose = CW_TRUE;
		CWSignalThreadCondition(&gWTPs[*iPtr].interfaceWait);
		return;
	}

	if(!CWErr(CWACResendAcknowledgedPacket(*iPtr))) {
		CWLog("Handling Soft timer Retransmitting ,message sent  ");
		_CWCloseThread(*iPtr);
	}
	
	/* CWDebugLog("~~~~~~fine ritrasmissione ~~~~~"); */
//	_CWCloseThread(*iPtr);
	CWThreadSetSignals(SIG_UNBLOCK, 2, 
			   CW_SOFT_TIMER_EXPIRED_SIGNAL,
			   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);
}

void CWResetWTPProtocolManager(CWWTPProtocolManager *WTPProtocolManager) {

	CW_FREE_OBJECT(WTPProtocolManager->locationData);
	CW_FREE_OBJECT(WTPProtocolManager->name);
	WTPProtocolManager->sessionID = 0;
	WTPProtocolManager->descriptor.maxRadios= 0;
	WTPProtocolManager->descriptor.radiosInUse= 0;
	WTPProtocolManager->descriptor.encCapabilities= 0;
	WTPProtocolManager->descriptor.vendorInfos.vendorInfosCount= 0;
	CW_FREE_OBJECT(WTPProtocolManager->descriptor.vendorInfos.vendorInfos);
	
	WTPProtocolManager->radiosInfo.radioCount= 0;
	CW_FREE_OBJECT(WTPProtocolManager->radiosInfo.radiosInfo);
	CW_FREE_OBJECT(WTPProtocolManager->ACName);
	(WTPProtocolManager->ACNameIndex).count = 0;
	CW_FREE_OBJECT((WTPProtocolManager->ACNameIndex).ACNameIndex);
	(WTPProtocolManager->radioAdminInfo).radiosCount = 0;
	CW_FREE_OBJECT((WTPProtocolManager->radioAdminInfo).radios);
	WTPProtocolManager->StatisticsTimer = 0;
	(WTPProtocolManager->WTPBoardData).vendorInfosCount = 0;
	CW_FREE_OBJECT((WTPProtocolManager->WTPBoardData).vendorInfos);
	CW_FREE_OBJECT(WTPProtocolManager->WTPRebootStatistics);
	CW_FREE_OBJECT(WTPProtocolManager->WTPVendorPayload);

	//CWWTPResetRebootStatistics(&(WTPProtocolManager->WTPRebootStatistics));

	/*
		**mancano questi campi:**
		CWNetworkLev4Address address;
		int pathMTU;
		struct sockaddr_in ipv4Address;
		CWProtocolConfigureRequestValues *configureReqValuesPtr;
		CWTimerID currentPacketTimer;
	*/
}


