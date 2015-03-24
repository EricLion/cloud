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

/*max WTP thread, not exit*/
//unsigned int gWTPsThreadNum = 0;

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
__inline__ int CWWTPByAddress(CWNetworkLev4Address *addressPtr,
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
		
		//CWLog("main thread echo ...");
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
 //main thread  must signal child thread when live and wait,
 //if child do something,unknown will happen
void CWACManageIncomingPacket(CWSocket sock,
			      char *buf,
			      int readBytes,
			      int incomingInterfaceIndex,
			      CWNetworkLev4Address *addrPtr) {

	//CWWTPManager *wtpPtr = NULL;
	int index = -1;
	char* pData = NULL;

	if(buf == NULL || addrPtr == NULL )
	{
		CWLog("buf == NULL || addrPtr == NULL, Fail !",__FILE__,__LINE__);
		return;
	}
	/* check if sender address is known */
	index = CWWTPByAddress(addrPtr, sock);
	
	if(index >= 0 ) {
		/* known WTP */
		/* Clone data packet */
		CWLog("F:%s,L:%d  coming known WTP[%d], readBytes=%d",__FILE__,__LINE__,index, readBytes);
		//timer experied ,main thread just wait
		if( gWTPs[index].isRequestClose == CW_TRUE)
		{
			CWLog("%s %d Main thread find WTP[%d] time expired, req close thread: %x,main donothing!", __FILE__,__LINE__,index,  (unsigned int)gWTPs[index].thread);
			//CWThreadMutexLock(&(gWTPs[index].interfaceMutex));
			//CWSignalThreadCondition(&(gWTPs[index].interfaceWait));
			//CWThreadMutexUnlock(&(gWTPs[index].interfaceMutex));
			//CWThreadSendSignal(gWTPs[index].thread, SIGKILL);
			return;
		}
		if(gWTPs[index].isNotFree == CW_FALSE )
		{
			CWLog("%s %d Main thread find WTP[%d] offline", __FILE__,__LINE__,index);
			return;
		}
		
		CW_CREATE_OBJECT_SIZE_ERR(pData, readBytes, { CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, "Can't CWACManageIncomingPacket");CWLog("Out Of Memory"); return; });
		memcpy(pData, buf, readBytes);
		CWLog("F:%s,L:%d",__FILE__,__LINE__);
		if(gWTPs[index].packetReceiveList == NULL)
		{
			CWLog("F:%s,L:%d [%d]packetReceiveList is NULL, Fail !",__FILE__,__LINE__,index);
			//gWTPs[index].isRequestClose = CW_TRUE;
			return;
		}
		//core
		//pthread_cond_wait must lock
		if(gWTPs[index].isNotFree == CW_FALSE )
		{
			CWLog("%s %d Main thread find WTP[%d] offline", __FILE__,__LINE__,index);
			return;
		}
		//CWLog("F:%s,L:%d ",__FILE__,__LINE__);
		if(!CWErr(CWThreadMutexLock(&gWTPsMutex))) 
		{
			CWLog("F:%s L:%d Error locking  mutex!",__FILE__,__LINE__);
			exit(1);
		}
		if(CW_FALSE == CWAddElementToSafeListTail(gWTPs[index].packetReceiveList, pData, readBytes))
		{
			CWLog("F:%s,L:%d CWAddElementToSafeListTail Fail !",__FILE__,__LINE__);
			CWThreadMutexUnlock(&gWTPsMutex);
			return;
		}
		CWThreadMutexUnlock(&gWTPsMutex);
		//CWLog("F:%s,L:%d ",__FILE__,__LINE__);

		if(!CWErr(CWThreadMutexLock(&(gWTPs[index].interfaceMutex)))) 
		{
			CWLog("F:%s,L:%d [%d] Error Lock interfaceMutex, Fail!",__FILE__,__LINE__,index);
			return;
		}
		//CWLog("F:%s,L:%d ",__FILE__,__LINE__);
		//main signal,if child not exit
		if( gWTPs[index].isRequestClose == CW_TRUE)
		{
			CWLog("%s %d Main thread find WTP[%d] time expired, req close thread: %x,main donothing!", __FILE__,__LINE__,index,  (unsigned int)gWTPs[index].thread);
			//CWThreadMutexLock(&(gWTPs[index].interfaceMutex));
			//CWSignalThreadCondition(&(gWTPs[index].interfaceWait));
			//CWThreadMutexUnlock(&(gWTPs[index].interfaceMutex));
			//return;
		}
		if(gWTPs[index].isNotFree == CW_FALSE )
		{
			CWLog("%s %d Main thread find WTP[%d] offline", __FILE__,__LINE__,index);
			//return;;
		}
		if (	(gWTPs[index].isNotFree == CW_TRUE) &&
			(gWTPs[index].isRequestClose == CW_FALSE) &&
		       //(CWGetCountElementFromSafeList(gWTPs[index].packetReceiveList) == 0) &&
		       (gWTPs[index].interfaceCommand == NO_CMD)) 
		{
			CWLog("F:%s,L:%d [%d] Main signal  thread: %x",__FILE__,__LINE__,index, (unsigned int)gWTPs[index].thread);
			CWSignalThreadCondition(&(gWTPs[index].interfaceWait));
		}
		else
		{
			CWLog("F:%s,L:%d [%d] not wait, don't signal !",__FILE__,__LINE__,index);
		}
		//wtpPtr->iwvaule = 1;
		//then main thread block,because thread exit,singal api can't find
		CWLog("F:%s,L:%d",__FILE__,__LINE__);
		CWThreadMutexUnlock(&(gWTPs[index].interfaceMutex));
		CWLog("F:%s,L:%d CWUnlockSafeList",__FILE__,__LINE__);

	} else { 
		/* unknown WTP */
		CWLog("F:%s,L:%d coming unknown WTP",__FILE__,__LINE__);
		int seqNum, tmp;
		CWDiscoveryRequestValues values;
		
		if(!CWErr(CWThreadMutexLock(&gActiveWTPsMutex))) 
		{
			CWLog("F:%s L:%d Error locking the gActiveWTPsMutex mutex",__FILE__,__LINE__);
			exit(1);
		}
			
		tmp = gActiveWTPs;
		//CWThreadMutexUnlock(&gActiveWTPsMutex);

		if(gActiveWTPs >= gMaxWTPs) {

			CWLog("Too many WTPs");
			CWThreadMutexUnlock(&gActiveWTPsMutex);
			return;
		}
		CWThreadMutexUnlock(&gActiveWTPsMutex);
		CWLog("Online WTP : gActiveWTPs = %d\n",gActiveWTPs);	

		//CWLog("buf =%s",buf);
		CWLog("readBytes =%d",readBytes);
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
			return;
		}
		if(!CWParseJoinReqMsgNotValue(buf, readBytes))
		{
			CWLog("Message is not Discovery Request or Join Resquest, refuse !");
			return;
		}
		else {
			/* this isn't a Discovery Request */
			//but must be joinRequest
			int i;
			CWACThreadArg *argPtr;
			
			CWUseSockNtop(addrPtr, CWDebugLog("Possible Client Hello from %s", str););

			//CWLog("i=%d", i);
			/* look for the first free slot */
			for(i = 0; i < CW_MAX_WTP ; i++)
			{
				if(gWTPs[i].isNotFree == CW_FALSE)
				{
					break;
				}
			}
			
			if(i >= CW_MAX_WTP){
				CWLog("WTP  [%d], more than Max %d, refuse!",i, CW_MAX_WTP);
				return;	
			}
			//thread not exit, i can not be used,big error !
			if(gWTPs[i].isRequestClose == CW_TRUE)
			{
				CWLog("%s %d Main thread find WTP[%d] time expired, req close thread: %x fail once,refuse !", __FILE__,__LINE__,index,  (unsigned int)gWTPs[i].thread);
				//CWThreadMutexLock(&(gWTPs[i].interfaceMutex));
				//CWSignalThreadCondition(&(gWTPs[i].interfaceWait));
				//CWThreadMutexUnlock(&(gWTPs[i].interfaceMutex));
				return;
			}
			
			CWLog("new WTP ID is %d",i);
			if(!CWErr(CWThreadMutexLock(&gWTPsMutex))) 
			{
				CWLog("F:%s L:%d Error locking  mutex",__FILE__,__LINE__);
				exit(1);
			}

			CW_COPY_NET_ADDR_PTR(&(gWTPs[i].address), addrPtr);
			gWTPs[i].isNotFree = CW_TRUE;
			gWTPs[i].isRequestClose = CW_FALSE;
			CWLog("F:%s L:%d free gWTPs[%d].packetReceiveList begin",__FILE__,__LINE__,i);
			CWCleanSafeList(gWTPs[i].packetReceiveList, free);
			CW_FREE_OBJECT(gWTPs[i].packetReceiveList);
			CWLog("F:%s L:%d free gWTPs[%d].packetReceiveList end ",__FILE__,__LINE__,i);
			//pointer NULL
			gWTPs[i].packetReceiveList = NULL;
			CWThreadMutexUnlock(&gWTPsMutex);

			/* Capwap receive packets list */
			//if (!CWErr(CWCreateSafeList(&gWTPs[i].packetReceiveList))) {
			if(!CWErr(CWThreadMutexLock(&gWTPsMutex))) 
			{
				CWLog("F:%s L:%d Error locking  mutex!",__FILE__,__LINE__);
				exit(1);
			}
			if (!(gWTPs[i].packetReceiveList = CWCreateSafeList()) ){

				gWTPs[i].isNotFree = CW_FALSE;
				CWThreadMutexUnlock(&gWTPsMutex);
				CWLog("F:%s L:%d CWCreateSafeList error !", __FILE__,__LINE__);
				return;
			}
			CWThreadMutexUnlock(&gWTPsMutex);
			//main thread must have the mutex
			CWLog("F:%s,L:%d  CWCreateSafeList gWTPs[%d].packetReceiveList success ",__FILE__,__LINE__,i);
			//avoid child close faster than main thread find it to add packet
			if(!CWErr(CWThreadMutexLock(&gWTPsMutex))) 
			{
				CWLog("F:%s L:%d Error locking  mutex!",__FILE__,__LINE__);
				exit(1);
			}
			CWDestroyThreadMutex(&gWTPs[i].interfaceMutex);
			CWDestroyThreadCondition(&gWTPs[i].interfaceWait);
			CWDestroyThreadCondition(&gWTPs[i].interfaceComplete);	
			
			if (!CWErr(CWCreateThreadMutex(&gWTPs[i].interfaceMutex)))
			{
				CWLog("F:%s L:%d CWCreateThreadMutex Fail!",__FILE__,__LINE__);
				CWThreadMutexUnlock(&gWTPsMutex);
				return;
			}

			if (!CWErr(CWCreateThreadCondition(&gWTPs[i].interfaceWait)))
			{
				CWLog("F:%s L:%d CWCreateThreadMutex Fail!",__FILE__,__LINE__);
				CWThreadMutexUnlock(&gWTPsMutex);
				return;
			}

			//CWDestroyThreadCondition(&gWTPs[i].interfaceComplete);	
			if (!CWErr(CWCreateThreadCondition(&gWTPs[i].interfaceComplete)))
			{
				CWLog("F:%s L:%d CWCreateThreadMutex Fail!",__FILE__,__LINE__);
				CWThreadMutexUnlock(&gWTPsMutex);
				return;
			}
			CWThreadMutexUnlock(&gWTPsMutex);
			
			CWLog("F:%s,L:%d",__FILE__,__LINE__);
			
			//CWSetMutexSafeList(gWTPs[i].packetReceiveList, 
					  // &gWTPs[i].interfaceMutex);
			//CWSetConditionSafeList(gWTPs[i].packetReceiveList,
					     //  &gWTPs[i].interfaceWait);

			/* Clone data packet */
			CW_CREATE_OBJECT_SIZE_ERR(pData, readBytes, { CWLog("Out Of Memory"); return; });
			memcpy(pData, buf, readBytes);
			CWLog("F:%s,L:%d",__FILE__,__LINE__);
			/*
			CWLockSafeList(gWTPs[i].packetReceiveList);
			CWAddElementToSafeListTail(gWTPs[i].packetReceiveList,
						   pData,
						   readBytes);
			CWUnlockSafeList(gWTPs[i].packetReceiveList);
			*/

			if(!gWTPs[i].packetReceiveList)
			{
				CWLog("F:%s,L:%d gWTPs[%d].packetReceiveList is NULL, Fail !",__FILE__,__LINE__,i);
				return;
			}
			
			CWLog("F:%s,L:%d CWLockSafeList",__FILE__,__LINE__);

			if(!CWErr(CWThreadMutexLock(&gWTPsMutex))) 
			{
				CWLog("F:%s L:%d Error locking  mutex!",__FILE__,__LINE__);
				exit(1);
			}
			if(CW_FALSE == CWAddElementToSafeListTail(gWTPs[i].packetReceiveList, pData, readBytes))
			{
				CWLog("F:%s,L:%d CWAddElementToSafeListTail Fail !",__FILE__,__LINE__);
				CWThreadMutexUnlock(&gWTPsMutex);
				return;
			}
			CWThreadMutexUnlock(&gWTPsMutex);
			
#if 0
			if(i < gWTPsThreadNum)
			{
				CWLog("F:%s,L:%d WTPsThread AP is working ...,no create new thread",__FILE__,__LINE__);
				return;
			}
#endif

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
			CWLog("F:%s,L:%d CWCreateThread WTP [%d]",__FILE__,__LINE__, i);

			if(!CWErr(CWCreateThread(&(gWTPs[i].thread), CWManageWTP, argPtr))) {

				CW_FREE_OBJECT(argPtr);
				if(!CWErr(CWThreadMutexLock(&gWTPsMutex))) 
				{
					CWLog("F:%s L:%d Error locking  mutex",__FILE__,__LINE__);
					exit(1);
				}
				
				CWCleanSafeList(gWTPs[i].packetReceiveList, free);
				CWDestroySafeList(gWTPs[i].packetReceiveList);
				gWTPs[i].isNotFree = CW_FALSE;
				CWThreadMutexUnlock(&gWTPsMutex);
				CWDebugLog("CWCreateThread CWManageWTP Fail! ");
				return;
			}

// operation useless
#if 0
			if(!CWErr(CWThreadMutexLock(&gWTPs[i].interfaceMutex))) 
			{
				CWLog("F:%s,L:%d Error Lock gWTPs[%d].interfaceMutex, Fail!",__FILE__,__LINE__,i);
				return;
			}
			if(gWTPs[i].isRequestClose == CW_TRUE)
			{
				CWLog("%s %d Main thread find WTP[%d] time expired, req close thread: %x fail once,so close again !", __FILE__,__LINE__,index,  (unsigned int)gWTPs[i].thread);
				CWThreadMutexLock(&(gWTPs[i].interfaceMutex));
				CWSignalThreadCondition(&(gWTPs[i].interfaceWait));
				CWThreadMutexUnlock(&(gWTPs[i].interfaceMutex));
			}
			if ((gWTPs[i].isNotFree == CW_TRUE) &&
			(gWTPs[i].isRequestClose == CW_FALSE) &&
		       //(CWGetCountElementFromSafeList(gWTPs[index].packetReceiveList) == 0) &&
		       (gWTPs[i].interfaceCommand == NO_CMD)) 
			{
				CWLog("F:%s,L:%d [%d] Main signal  thread: %x",__FILE__,__LINE__,i, (unsigned int)gWTPs[i].thread);
				CWSignalThreadCondition(&(gWTPs[i].interfaceWait));
			}
			else
			{
				CWLog("F:%s,L:%d [%d] not wait, don't signal !",__FILE__,__LINE__,i);
			}
			CWLog("F:%s,L:%d",__FILE__,__LINE__);
			CWThreadMutexUnlock(&gWTPs[i].interfaceMutex);
			CWLog("F:%s,L:%d CWUnlockSafeList",__FILE__,__LINE__);
#endif
#if 0
			 if(gWTPsThreadNum > CW_MAX_WTP)
			{
				CWLog("F:%s,L:%d gWTPsThreadNum > CW_MAX_WTP , Fail!",__FILE__,__LINE__);
				return;
			}
			 gWTPsThreadNum++;
			 CWLog("F:%s,L:%d gWTPsThreadNum = %d",__FILE__,__LINE__,gWTPsThreadNum);
#endif
		}
	}
	CWLog("F:%s,L:%d %s end",__FILE__,__LINE__,__func__);
}

/*
 * Simple job: see if we have a thread that is serving address *addressPtr
 */
__inline__ int CWWTPByAddress(CWNetworkLev4Address *addressPtr, CWSocket sock) {

	int i;

	CWLog("CWWTPByAddress  sock = %d",sock);
	//if(addressPtr == NULL) return NULL;
	if(addressPtr == NULL) return -1;
	//CWLog("addr = %s",inet_ntoa(((struct sockaddr_in *) addressPtr)->sin_addr));
	//CWLog("port = %d",((struct sockaddr_in *) addressPtr)->sin_port);
	//CWThreadMutexLock(&gWTPsMutex);
	//if(!CWErr(CWThreadMutexLock(&gWTPsMutex))) {
		//CWLog("Error CWWTPByAddress locking the gWTPsMutex mutex");
		//return NULL;
	//}
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
			//CWThreadMutexUnlock(&gWTPsMutex);
			//return &(gWTPs[i]);
			return i;
			
		}
	}
	//CWThreadMutexUnlock(&gWTPsMutex);
	
	//return NULL;
	return -1;
	
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
	CWLog("F:%s,L:%d CWManageWTP AP index = %d",__FILE__,__LINE__, i);
	
	if(!(CWThreadSetSpecific(&gIndexSpecific, &i))) {

		CWLog("Critical Error with Thread Data");
		_CWCloseThread(i);
	}

	gInterfaces[interfaceIndex].WTPCount++;
	CWUseSockNtop(((struct sockaddr*) &(gInterfaces[interfaceIndex].addr)),
				  CWDebugLog("One more WTP on %s (%d)", str, interfaceIndex);
				  );
	
	//CWThreadMutexUnlock(&gActiveWTPsMutex);

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
	//CWDestroyThreadMutex(&gWTPs[i].interfaceMutex);	
	//CWCreateThreadMutex(&gWTPs[i].interfaceMutex);
	//CWDestroyThreadMutex(&gWTPs[i].interfaceSingleton);	
	//CWCreateThreadMutex(&gWTPs[i].interfaceSingleton);
	//CWDestroyThreadCondition(&gWTPs[i].interfaceWait);	
	//CWCreateThreadCondition(&gWTPs[i].interfaceWait);
	//CWDestroyThreadCondition(&gWTPs[i].interfaceComplete);	
	//CWCreateThreadCondition(&gWTPs[i].interfaceComplete);
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

	gWTPs[i].ofdmValues = NULL;  
	gWTPs[i].vendorValues = NULL;
	gWTPs[i].vendorPortalValues = NULL;

	CWResetWTPProtocolManager(&(gWTPs[i].WTPProtocolManager));

	//buf 
	gWTPs[i].buf = NULL; 
	CW_CREATE_OBJECT_SIZE_ERR(gWTPs[i].buf,CW_BUFFER_SIZE,{ CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL);CWLog("Out Of Memory");CWCloseThread();});
	memset(gWTPs[i].buf, 0, CW_BUFFER_SIZE);

	gWTPs[i].session = NULL;
	CWLog("New Session");

	/* start WaitJoin timer */
	if(!CWErr(CWTimerRequest(gCWWaitJoin,
				 &(gWTPs[i].thread),
				 &(gWTPs[i].currentTimer),
				 CW_CRITICAL_TIMER_EXPIRED_SIGNAL))) {	 
		CWLog("%s %d [%d] CWTimerRequest Fail, close thread!",__FILE__,__LINE__,i);
		gWTPs[i].isRequestClose = CW_TRUE;
	}

#ifndef CW_NO_DTLS
	CWDebugLog("Init DTLS Session");

 	if(!CWErr(CWSecurityInitSessionServer(&gWTPs[i],
					      sock,
					      gACSecurityContext,
					      &((gWTPs[i]).session),
					      &(gWTPs[i].pathMTU)))) {
					      
		CWTimerCancel(&(gWTPs[i].currentTimer));
		CWLog("%s %d [%d] CWSecurityInitSessionServer Fail, close thread!",__FILE__,__LINE__,i);
		gWTPs[i].isRequestClose = CW_TRUE;
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
		//if pthread not come here,the packet will wait,so packet list grow
		if(!CWErr(CWThreadMutexLock(&gWTPs[i].interfaceMutex))) 
		{
			CWLog("Error gWTPs[%d] interfaceMutex!",i);
			gWTPs[i].isRequestClose = CW_TRUE;
		}
		//CWThreadMutexLock(&gWTPs[i].interfaceMutex);
		
		while ((gWTPs[i].isRequestClose == CW_FALSE) &&
		       (CWGetCountElementFromSafeList(gWTPs[i].packetReceiveList) == 0) &&
		       (gWTPs[i].interfaceCommand == NO_CMD)) {
			
			CWLog("[%d]CWWaitThreadCondition start",i);
			 /*TODO: Check system */
			 /*
			 when parent talk, child should be here;
			 so child do sth quick as soon as possiblie.
			 */
			CWWaitThreadCondition(&gWTPs[i].interfaceWait, 
					      &gWTPs[i].interfaceMutex);
					      //gWTPs[i].iwvaule = 0;
			CWLog("[%d]CWWaitThreadCondition end",i);
		}
		CWThreadMutexUnlock(&gWTPs[i].interfaceMutex);
		//CWLog("%s %d ",__FILE__,__LINE__);
		if (gWTPs[i].isRequestClose) {

			CWLog("Request close thread");
			//can't exit?
			_CWCloseThread(i);
			//continue;
			//exit...
			return NULL;
		}
		//CWLog("%s %d",__FILE__,__LINE__);
		CWThreadSetSignals(SIG_BLOCK, 
				   2,
				   CW_SOFT_TIMER_EXPIRED_SIGNAL,
				   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);

		CWLog("WTP[%d] has %d packet to analysis ...",i,CWGetCountElementFromSafeList(gWTPs[i].packetReceiveList));

		if (CWGetCountElementFromSafeList(gWTPs[i].packetReceiveList) > 0) {
			CWBool 	bCrypt = CW_FALSE;
			char	*pBuffer = NULL;

			//CWThreadMutexLock(&gWTPs[i].interfaceMutex);
			/*
			if(!CWErr(CWThreadMutexLock(&gWTPs[i].interfaceMutex))) {
				CWLog("Error gWTPs[i].interfaceMutex !");
				CWCloseThread();
			}
			*/

			pBuffer = (char *)CWGetHeadElementFromSafeList(gWTPs[i].packetReceiveList, NULL);
			//why
			//if(pBuffer == NULL || strlen(pBuffer) < 1)
			if(pBuffer == NULL)
			{
				CWLog("%s %d  [%d] packet len not right ,exit thread !",__FILE__,__LINE__,i);
				//CWThreadMutexUnlock(&gWTPs[i].interfaceMutex);
				gWTPs[i].isRequestClose = CW_TRUE;
				continue;
			}
			//CWThreadMutexUnlock(&gWTPs[i].interfaceMutex);
			
			if ((pBuffer[0] & 0x0f) == CW_PACKET_CRYPT)
			  bCrypt = CW_TRUE;
//			CWLog("%s %d ",__FILE__,__LINE__);
			if (bCrypt) {
				CWDebugLog("Receive a security packet");
				CWLog("Don't parse security packet,drop it");
#ifdef CW_NO_DTLS
				//core fix
				//CWThreadSetSignals(SIG_UNBLOCK, 1, CW_SOFT_TIMER_EXPIRED_SIGNAL);
				//continue;

				CWLog("Close thread");
				_CWCloseThread(i);
				
#else
				//CWSecurityReceive core ????
			 if(!CWErr(CWThreadMutexLock(&gWTPsMutex))) {
				CWLog("%s %d [%d] CWThreadMutexLock Fail, close thread!",__FILE__,__LINE__,i);
				gWTPs[i].isRequestClose = CW_TRUE;
				continue;
			  }
			  CWLog("%s %d apindex = %d, CWRemoveHeadElementFromSafeList",__FILE__,__LINE__,i);
			  pBuffer = (char*)CWRemoveHeadElementFromSafeList(gWTPs[i].packetReceiveList, &readBytes);
			  CWLog("%s %d  [%d] packet len readBytes=%d",__FILE__,__LINE__,i,readBytes);
			  if(pBuffer == NULL)
			  {
				CWLog("%s %d  [%d] packet len not right ,exit thread !",__FILE__,__LINE__,i);
				//CWThreadMutexUnlock(&gWTPs[i].interfaceMutex);
				gWTPs[i].isRequestClose = CW_TRUE;
				CWThreadMutexUnlock(&gWTPsMutex);
				continue;
			  }
			  else
			  {
			   	  CWThreadMutexUnlock(&gWTPsMutex);
				  memset(gWTPs[i].buf, 0, CW_BUFFER_SIZE);
				  memcpy(gWTPs[i].buf, pBuffer, readBytes);
				  CW_FREE_OBJECT(pBuffer);
			  }
			  if(!CWErr(CWSecurityReceive(gWTPs[i].session,
										  gWTPs[i].buf,
										  CW_BUFFER_SIZE - 1,
										  &readBytes))) {
					/* error */
				CWDebugLog("Error during security receive");
				CWLog("Error during security receive");
				CWThreadSetSignals(SIG_UNBLOCK, 1, CW_SOFT_TIMER_EXPIRED_SIGNAL);
				gWTPs[i].isRequestClose = CW_TRUE;
				continue;
			  }
#endif
			}
			else {
			  //CWThreadMutexLock(&gWTPs[i].interfaceMutex);
		  	if(!CWErr(CWThreadMutexLock(&gWTPsMutex))) 
		  	{
				CWLog("%s %d [%d] CWThreadMutexLock Fail, close thread!",__FILE__,__LINE__,i);
				gWTPs[i].isRequestClose = CW_TRUE;
				continue;
			}
			  //coredump
			  CWLog("%s %d apindex = %d, CWRemoveHeadElementFromSafeList",__FILE__,__LINE__,i);
			  pBuffer = (char*)CWRemoveHeadElementFromSafeList(gWTPs[i].packetReceiveList, &readBytes);
			  CWLog("%s %d  [%d] packet len readBytes=%d",__FILE__,__LINE__,i,readBytes);
			  if(pBuffer == NULL)
			  {
				CWLog("%s %d  [%d] packet len not right ,exit thread !",__FILE__,__LINE__,i);
				//CWThreadMutexUnlock(&gWTPs[i].interfaceMutex);
				gWTPs[i].isRequestClose = CW_TRUE;
				CWThreadMutexUnlock(&gWTPsMutex);
				continue;
			  }
			  else
			  {
			  	  CWThreadMutexUnlock(&gWTPsMutex);
				  memset(gWTPs[i].buf, 0, CW_BUFFER_SIZE);
				  memcpy(gWTPs[i].buf, pBuffer, readBytes);
				  CW_FREE_OBJECT(pBuffer);
			  }
			  
		}

			if(!CWProtocolParseFragment(gWTPs[i].buf,
						    readBytes,
						    &(gWTPs[i].fragmentsList),
						    &msg,
						    &dataFlag)) {

				if(CWErrorGetLastErrorCode() == CW_ERROR_NEED_RESOURCE) {

					CWLog("Need At Least One More Fragment");
				} 
				else {
					CWErrorHandleLast();
				}
				CWThreadSetSignals(SIG_UNBLOCK, 1, CW_SOFT_TIMER_EXPIRED_SIGNAL);
				CWLog("%s %d [%d] CWProtocolParseFragment continue ...",__FILE__,__LINE__,i);
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
							CWLog("ACEnterJoin critical error !!!");
							CWLog("%s %d [%d] close thread!",__FILE__,__LINE__,i);
							gWTPs[i].isRequestClose = CW_TRUE;
							continue;
						}
					}
					break;
				}
				case CW_ENTER_CONFIGURE:
				{
					if(!ACEnterConfigure(i, &msg)) 
					{
						//CWLog("ACEnterConfigure Fail !!!");
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
							CWLog("%s %d [%d] close thread!",__FILE__,__LINE__,i);
							gWTPs[i].isRequestClose = CW_TRUE;
							continue;
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
							CWLog("%s %d [%d] close thread!",__FILE__,__LINE__,i);
							gWTPs[i].isRequestClose = CW_TRUE;
							continue;
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
							CWLog("%s %d [%d] close thread!",__FILE__,__LINE__,i);
							gWTPs[i].isRequestClose = CW_TRUE;
							continue;
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
			CWLog("packetReceiveList no content");
		  //CWThreadMutexLock(&gWTPs[i].interfaceMutex);
		  //no lock
		/*
		  if(!CWErr(CWThreadMutexLock(&gWTPs[i].interfaceMutex))) {
				CWLog("Error gWTPs[i].interfaceMutex !");
				CWCloseThread();
		  }
		  */
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
					 if(!CWErr(CWThreadMutexLock(&gWTPs[i].interfaceMutex))) {
						CWLog("%s %d [%d] Error Lock interfaceMutex, close thread!",__FILE__,__LINE__,i);
						gWTPs[i].isRequestClose = CW_TRUE;
						continue;
					  }
					 //parent child one step
					CWSignalThreadCondition(&gWTPs[i].interfaceComplete);
					CWThreadMutexUnlock(&gWTPs[i].interfaceMutex);
					CWDebugLog("Error sending command");
				}
			}
			//CWThreadMutexUnlock(&gWTPs[i].interfaceMutex);
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
	CWLog("CWMangeWtp over !!!");
	CWDebugLog("CWMangeWtp over !!!");
}

//only close WTP by one way
void _CWCloseThread(int i) {
//BE: ap disconnect
	CWLog("_CWCloseThread WTP[%d], thead id:%x",i,(unsigned int)gWTPs[i].thread);

	if(gWTPs[i].isNotFree == CW_FALSE)
	{
		CWLog("_CWCloseThread gWTPs[%d].isNotFree == CW_FALSE,it has been closed,no need close again !",i);
		CWExitThread();
		return;
	}
	if(!CWErr(CWThreadMutexLock(&gWTPsMutex)))
	{
		CWLog("_CWCloseThread CWThreadMutexLock fail,exit !");
		exit(1);
	}
	gWTPs[i].isNotFree = CW_FALSE;
	gWTPs[i].isRequestClose = CW_FALSE;

	CWThreadMutexUnlock(&gWTPsMutex);

 	CWThreadSetSignals(SIG_BLOCK, 2, 
			   CW_SOFT_TIMER_EXPIRED_SIGNAL, 
			   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);
	
	char *beResp = NULL;
	int BESize;
	
	BEconnectEvent beConEve;
	
	//CWLog("F:%s,L:%d ",__FILE__,__LINE__);

		//num can't < 0
	if(gActiveWTPs && gWTPs[i].currentState == CW_ENTER_RUN)
	{
		if(!CWErr(CWThreadMutexLock(&gActiveWTPsMutex)))
		{
			CWLog("_CWCloseThread CWThreadMutexLock fail,exit !");
			exit(1);
		}
		gActiveWTPs--;
		CWThreadMutexUnlock(&gActiveWTPsMutex);
		
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
		CWLog("[%d]_CWCloseThread Send BE success",i,gActiveWTPs);
	}
	//CWLog("F:%s,L:%d ",__FILE__,__LINE__);
	CWDebugLog("Close Thread: %08x", (unsigned int)CWThreadSelf());

	CWACStopRetransmission(i);
	
	/**** ACInterface ****/
	
	//CWThreadMutexLock(&gWTPsMutex);
	CW_FREE_OBJECT(gWTPs[i].qosValues);
	CW_FREE_OBJECT(gWTPs[i].ofdmValues);
	//add free
	if(gWTPs[i].vendorValues != NULL)
	{
		CW_FREE_OBJECT(gWTPs[i].vendorValues->payload);
		CW_FREE_OBJECT(gWTPs[i].vendorValues);
	}
	if(gWTPs[i].vendorPortalValues != NULL)
	{
		CW_FREE_OBJECT(gWTPs[i].vendorPortalValues->EncodeName);
		CW_FREE_OBJECT(gWTPs[i].vendorPortalValues->EncodeContent);
		CW_FREE_OBJECT(gWTPs[i].vendorPortalValues);
	}
	//CWLog("F:%s,L:%d ",__FILE__,__LINE__);
	//gWTPs[i].qosValues=NULL;
	memset(gWTPs[i].MAC, 0, MAC_ADDR_LEN);
	//gWTPs[i].isConnect = CW_FALSE;

	/**** ACInterface ****/

	gInterfaces[gWTPs[i].interfaceIndex].WTPCount--;

	CWUseSockNtop( ((struct sockaddr*)&(gInterfaces[gWTPs[i].interfaceIndex].addr)),
			CWLog("Remove WTP on Interface %s (%d)", str, gWTPs[i].interfaceIndex););
	
	if(gWTPs[i].subState != CW_DTLS_HANDSHAKE_IN_PROGRESS) {
	
		CWSecurityDestroySession(gWTPs[i].session);
	}
	
	/* this will do nothing if the timer isn't active */
	CWTimerCancel(&(gWTPs[i].currentTimer));

	if (gWTPs[i].interfaceCommandProgress == CW_TRUE || gWTPs[i].interfaceCommand != NO_CMD) 
	{
		gWTPs[i].interfaceResult = 1;
		gWTPs[i].interfaceCommandProgress = CW_FALSE;
		CWThreadMutexLock(&gWTPs[i].interfaceMutex);
		//BE
		CWSignalThreadCondition(&gWTPs[i].interfaceComplete);
		//WTPThread
		//CWSignalThreadCondition(&gWTPs[i].interfaceWait);

		CWThreadMutexUnlock(&gWTPs[i].interfaceMutex);
	}
	//child thread last core 
	gWTPs[i].session = NULL;
	gWTPs[i].currentState = CW_QUIT;
	gWTPs[i].subState = CW_DTLS_HANDSHAKE_IN_PROGRESS;
	gWTPs[i].isConnect = CW_FALSE;
	gWTPs[i].interfaceCommand = NO_CMD;

	CW_FREE_OBJECT(gWTPs[i].buf);
	if(gWTPs[i].fragmentsList != NULL)
	{
		CWDeleteList(&(gWTPs[i].fragmentsList), CWProtocolDestroyFragment);
		gWTPs[i].fragmentsList = NULL;
	}
	//CWLog("F:%s,L:%d ",__FILE__,__LINE__);
	/* CW_FREE_OBJECT(gWTPs[i].configureReqValuesPtr); */
	//CWLog("F:%s,L:%d before gWTPs[%d].packetReceiveList->nCount = %d",__FILE__,__LINE__,i,gWTPs[i].packetReceiveList->nCount);

	//CWCleanSafeList(gWTPs[i].packetReceiveList, free);
	//CWLog("F:%s,L:%d after gWTPs[%d].packetReceiveList->nCount = %d",__FILE__,__LINE__,i,gWTPs[i].packetReceiveList->nCount);
	//CWDestroySafeList(gWTPs[i].packetReceiveList);
	//CWLog("F:%s,L:%d ",__FILE__,__LINE__);
	//CW_FREE_OBJECT(gWTPs[i].packetReceiveList);
	//CWLog("F:%s,L:%d ",__FILE__,__LINE__);
	
	CWResetWTPProtocolManager(&(gWTPs[i].WTPProtocolManager));
	
	//CWDestroyThreadMutex(&gWTPs[i].interfaceMutex);
	//CWDestroyThreadCondition(&gWTPs[i].interfaceWait);
	//gWTPs[i].iwvaule = 0;
	//gWTPs[i].isNotFree = CW_FALSE;  /* chenchao test */
	//gWTPs[i].isNotFree = CW_FALSE;
	//gWTPs[i].isRequestClose = CW_FALSE;
	//CWDestroyThreadCondition(&gWTPs[i].interfaceComplete);	
	
	CWLog("F:%s,L:%d ",__FILE__,__LINE__);

	CWThreadSetSignals(SIG_UNBLOCK, 2, 
			   CW_SOFT_TIMER_EXPIRED_SIGNAL,
			   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);
	
	CWExitThread();
}
//not complete safe
void CWCloseThread() {

	int *iPtr;
	
	if((iPtr = ((int*)CWThreadGetSpecific(&gIndexSpecific))) == NULL) {

		CWLog("Error Closing Thread");
		return;
	}
	
	_CWCloseThread(*iPtr);
}
//Interrupt function must run complete,can't exit pthread when it waitsignal
//pthread_cond_signal only signal other thread, not myself,but some time ok...
void CWCriticalTimerExpiredHandler(int arg) {

	int *iPtr;

	CWThreadSetSignals(SIG_BLOCK, 2,
			   CW_SOFT_TIMER_EXPIRED_SIGNAL,
			   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);
 	
	CWLog("Critical Timer Expired for Thread: %08x", (unsigned int)CWThreadSelf());
	CWLog("Abort Session");
//	CWCloseThread();

	if((iPtr = ((int*)CWThreadGetSpecific(&gIndexSpecific))) == NULL) {

		CWLog("Error Handling Critical timer");
		CWThreadSetSignals(SIG_UNBLOCK, 2, 
				   CW_SOFT_TIMER_EXPIRED_SIGNAL,
				   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);
		return;
	}
	CWLog("WTPs[%d] is  req close ...",*iPtr);
	CWThreadSetSignals(SIG_UNBLOCK, 2, 
				   CW_SOFT_TIMER_EXPIRED_SIGNAL,
				   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);
	//_CWCloseThread(*iPtr);
	//return;
	/* Request close thread */
	gWTPs[*iPtr].isRequestClose = CW_TRUE;
	CWThreadMutexLock(&gWTPs[*iPtr].interfaceMutex);
	CWLog("WTPs[%d] CWSignalThreadCondition begin ",*iPtr);
	CWSignalThreadCondition(&gWTPs[*iPtr].interfaceWait);
	CWLog("WTPs[%d] CWSignalThreadCondition end ",*iPtr);
	CWThreadMutexUnlock(&gWTPs[*iPtr].interfaceMutex);
	CWLog("WTPs[%d] CriticalTimerExpired end",*iPtr);
}

//Retransmit Send Msg
void CWSoftTimerExpiredHandler(int arg) {

	int *iPtr;

	CWThreadSetSignals(SIG_BLOCK, 2, 
			   CW_SOFT_TIMER_EXPIRED_SIGNAL,
			   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);

	CWLog("Soft Timer Expired for Thread: %08x", 
		   (unsigned int)CWThreadSelf());
	
	if((iPtr = ((int*)CWThreadGetSpecific(&gIndexSpecific))) == NULL) {

		CWLog("Error Handling Soft timer");
		CWThreadSetSignals(SIG_UNBLOCK, 2, 
				   CW_SOFT_TIMER_EXPIRED_SIGNAL,
				   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);
		return;
	}
	CWLog("WTPs[%d] is req close ...",*iPtr);
	//CWLog("Handling Soft timer,apid =%d,gWTPs[%d].isRetransmitting=%d ",*iPtr,*iPtr,gWTPs[*iPtr].isRetransmitting);
	if((!gWTPs[*iPtr].isRetransmitting) || (gWTPs[*iPtr].messages == NULL)) {

		CWLog("Soft timer expired but we are not retransmitting");
		CWLog("WTPs[%d] is req close ...",*iPtr);
		CWThreadSetSignals(SIG_UNBLOCK, 2, 
					   CW_SOFT_TIMER_EXPIRED_SIGNAL,
					   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);
		gWTPs[*iPtr].isRequestClose = CW_TRUE;
		CWThreadMutexLock(&gWTPs[*iPtr].interfaceMutex);
		CWLog("WTPs[%d] CWSignalThreadCondition begin ",*iPtr);
		CWSignalThreadCondition(&gWTPs[*iPtr].interfaceWait);
		CWLog("WTPs[%d] CWSignalThreadCondition end ",*iPtr);
		CWThreadMutexUnlock(&gWTPs[*iPtr].interfaceMutex);
		CWLog("WTPs[%d] CriticalTimerExpired end",*iPtr);
		return;
		//return;
	}

	(gWTPs[*iPtr].retransmissionCount)++;
	
	CWLog("Retransmission Count increases to %d", gWTPs[*iPtr].retransmissionCount);
	
	if(gWTPs[*iPtr].retransmissionCount >= gCWMaxRetransmit) 
	{
		CWLog("Peer is Dead");
//		 _CWCloseThread(*iPtr);
		 /* Request close thread
		 */
		//gWTPs[*iPtr].isRequestClose = CW_TRUE;
		//CWThreadMutexLock(&gWTPs[*iPtr].interfaceMutex);
		//CWSignalThreadCondition(&gWTPs[*iPtr].interfaceWait);
		//CWThreadMutexUnlock(&gWTPs[*iPtr].interfaceMutex);
		//return;
		CWLog("WTPs[%d] is  req close ...",*iPtr);
		CWThreadSetSignals(SIG_UNBLOCK, 2, 
					   CW_SOFT_TIMER_EXPIRED_SIGNAL,
					   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);
		gWTPs[*iPtr].isRequestClose = CW_TRUE;
		CWThreadMutexLock(&gWTPs[*iPtr].interfaceMutex);
		CWLog("WTPs[%d] CWSignalThreadCondition begin ",*iPtr);
		CWSignalThreadCondition(&gWTPs[*iPtr].interfaceWait);
		CWLog("WTPs[%d] CWSignalThreadCondition end ",*iPtr);
		CWThreadMutexUnlock(&gWTPs[*iPtr].interfaceMutex);
		CWLog("WTPs[%d] CriticalTimerExpired end",*iPtr);
		return;
	}

	if(!CWErr(CWACResendAcknowledgedPacket(*iPtr))) {
		CWLog("Handling Soft timer Retransmitting ,message sent  ");
		//_CWCloseThread(*iPtr);
		//gWTPs[*iPtr].isRequestClose = CW_TRUE;
		//CWThreadMutexLock(&gWTPs[*iPtr].interfaceMutex);
		//CWSignalThreadCondition(&gWTPs[*iPtr].interfaceWait);
		//CWThreadMutexUnlock(&gWTPs[*iPtr].interfaceMutex);
		CWLog("WTPs[%d] is  req close ...",*iPtr);
		CWThreadSetSignals(SIG_UNBLOCK, 2, 
					   CW_SOFT_TIMER_EXPIRED_SIGNAL,
					   CW_CRITICAL_TIMER_EXPIRED_SIGNAL);
		gWTPs[*iPtr].isRequestClose = CW_TRUE;
		CWThreadMutexLock(&gWTPs[*iPtr].interfaceMutex);
		CWLog("WTPs[%d] CWSignalThreadCondition begin ",*iPtr);
		CWSignalThreadCondition(&gWTPs[*iPtr].interfaceWait);
		CWLog("WTPs[%d] CWSignalThreadCondition end ",*iPtr);
		CWThreadMutexUnlock(&gWTPs[*iPtr].interfaceMutex);
		CWLog("WTPs[%d] CriticalTimerExpired end",*iPtr);
		return;
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


