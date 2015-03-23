/************************************************************************************************
 * Copyright (c) 2006-2009 Laboratorio di Sistemi di Elaborazione e Bioingegneria Informatica	*
 *                          Universita' Campus BioMedico - Italy								*
 *																								*
 * This program is free software; you can redistribute it and/or modify it under the terms		*
 * of the GNU General Public License as published by the free Software Foundation; either		*
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
 * Authors : Matteo Latini (mtylty@gmail.com)													*  
 *
 ************************************************************************************************/

#include "CWVendorPayloads.h"
#include "WUM.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h> 
#include <signal.h>

#ifdef DMALLOC
#include "../dmalloc-5.5.0/dmalloc.h"
#endif


CWBool CWAssembleWTPVendorPayloadUCI(CWProtocolMessage *msgPtr) {
	int* iPtr;
	unsigned short  msgType;
	CWProtocolVendorSpecificValues* valuesPtr;
	CWVendorUciValues* uciPtr;

	CWLog("Assembling Protocol Configuration Update Request [VENDOR CASE]...");

	if(msgPtr == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);

	if((iPtr = ((int*)CWThreadGetSpecific(&gIndexSpecific))) == NULL) {
	  return CW_FALSE;
	}

	valuesPtr =gWTPs[*iPtr].vendorValues;
	switch (valuesPtr->vendorPayloadType){
			case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_UCI:
				msgType = CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_UCI;
				uciPtr = (CWVendorUciValues *) valuesPtr->payload;
				if (uciPtr->commandArgs != NULL) {
					/* create message */
					CW_CREATE_PROTOCOL_MESSAGE(*msgPtr, sizeof(short)+sizeof(char)+sizeof(int)+(strlen(uciPtr->commandArgs)*sizeof(char)), return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
					CWProtocolStore16(msgPtr, (unsigned short) msgType);
					CWProtocolStore8(msgPtr, (unsigned char) uciPtr->command);
					CWProtocolStore32(msgPtr, (unsigned int) strlen(uciPtr->commandArgs));
					CWProtocolStoreStr(msgPtr, uciPtr->commandArgs);
				} else {
					/* create message */
					CW_CREATE_PROTOCOL_MESSAGE(*msgPtr, sizeof(short)+sizeof(char)+sizeof(int), return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
					CWProtocolStore16(msgPtr, (unsigned short) msgType);
					CWProtocolStore8(msgPtr, (unsigned char) uciPtr->command);
					CWProtocolStore32(msgPtr, 0);
				}
			break;
			default:
				return CW_FALSE;
			break;
	}
	CWLog("Assembling Protocol Configuration Update Request [VENDOR CASE]: Message Assembled.");
	CW_FREE_OBJECT(gWTPs[*iPtr].vendorValues->payload);
	CW_FREE_OBJECT(gWTPs[*iPtr].vendorValues);
	
	return CWAssembleMsgElem(msgPtr, CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_CW_TYPE);
}

CWBool CWAssembleWTPVendorPayloadWUM(CWProtocolMessage *msgPtr) {
	int* iPtr;
	unsigned short  msgType;
	CWProtocolVendorSpecificValues* valuesPtr = NULL;
	CWVendorWumValues* wumPtr = NULL;

	CWLog("Assembling Protocol Configuration Update Request [VENDOR CASE]...");

	if(msgPtr == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);

	if((iPtr = ((int*)CWThreadGetSpecific(&gIndexSpecific))) == NULL) {
	  return CW_FALSE;
	}

	valuesPtr =gWTPs[*iPtr].vendorValues;
	CWLog("*iPtr = %d ",*iPtr);
	switch (valuesPtr->vendorPayloadType){
			case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_WUM:
				/* 
				 * Here we assemble the WTP Update Messages. 
 				 */
				msgType = CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_WUM;
                                wumPtr = (CWVendorWumValues*) valuesPtr->payload;
				
				switch(wumPtr->type) {
				case WTP_CONFIG_REQUEST:
				case WTP_COMMIT_UPDATE:
				case WTP_CANCEL_UPDATE_REQUEST:
					CW_CREATE_PROTOCOL_MESSAGE(*msgPtr, sizeof(short)+sizeof(char), return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
				break;
				case WTP_UPDATE_REQUEST:
					CW_CREATE_PROTOCOL_MESSAGE(*msgPtr, sizeof(short)+4*sizeof(char)+sizeof(unsigned int), return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
				break;
				case WTP_CUP_FRAGMENT:
					CW_CREATE_PROTOCOL_MESSAGE(*msgPtr, sizeof(short)+sizeof(char)+2*sizeof(int)+wumPtr->_cup_fragment_size_, return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
				break;
				default:
					CWLog("Error! unknown WUM message type!!!,wumPtr->type =%d",wumPtr->type);
					return CW_FALSE;
				}

                                CWProtocolStore16(msgPtr, (unsigned short) msgType);
                                CWProtocolStore8(msgPtr, (unsigned char) wumPtr->type);
				if (wumPtr->type == WTP_UPDATE_REQUEST) {
					//add version
					//wumPtr->_major_v_ = 3;
					//wumPtr->_minor_v_ = 3;
					//wumPtr->_revision_v_ = 3;
					CWLog("[F:%s, L:%d]wumPtr->_major_v_ = %d,wumPtr->_minor_v_ = %d,wumPtr->_revision_v_ = %d,wumPtr->_pack_size_ = %d ",__FILE__,__LINE__,
							wumPtr->_major_v_,wumPtr->_minor_v_,wumPtr->_revision_v_,wumPtr->_pack_size_);
					CWProtocolStore8(msgPtr, wumPtr->_major_v_);	
					CWProtocolStore8(msgPtr, wumPtr->_minor_v_);	
					CWProtocolStore8(msgPtr, wumPtr->_revision_v_);
					CWProtocolStore32(msgPtr, wumPtr->_pack_size_);
				} else if (wumPtr->type == WTP_CUP_FRAGMENT) {
				CWLog("[F:%s, L:%d]wumPtr->_seq_num_= %d,wumPtr->_cup_fragment_size_= %d,",__FILE__,__LINE__,wumPtr->_seq_num_,wumPtr->_cup_fragment_size_);
					CWProtocolStore32(msgPtr, wumPtr->_seq_num_);
					CWProtocolStore32(msgPtr, wumPtr->_cup_fragment_size_);
					//CWLog("[F:%s, L:%d]",__FILE__,__LINE__);
					CWProtocolStoreRawBytes(msgPtr, wumPtr->_cup_, wumPtr->_cup_fragment_size_);
					//CWLog("[F:%s, L:%d]",__FILE__,__LINE__);
					//CW_FREE_OBJECT(wumPtr->_cup_);
					//CWLog("[F:%s, L:%d]",__FILE__,__LINE__);
				}
			break;
			default:
				return CW_FALSE;
			break;
	}
	CWLog("Assembling Protocol Configuration Update Request [VENDOR CASE]: Message Assembled.");
	CW_FREE_OBJECT(gWTPs[*iPtr].vendorValues->payload);
	CW_FREE_OBJECT(gWTPs[*iPtr].vendorValues);

	return CWAssembleMsgElem(msgPtr, CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_CW_TYPE);
}




CWBool CWAssembleWTPVendorPayloadXML(CWProtocolMessage *msgPtr) {
	int* iPtr;
	unsigned short  msgType;
	CWProtocolVendorSpecificValues* valuesPtr = NULL;
//	CWVendorXMLValues* xmlPtr = NULL;
//	char *xmlTemp;
	short xmlLen;

	//CW_CREATE_OBJECT_ERR(xmlPtr, CWVendorXMLValues, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});
	CWLog("Assembling Protocol Configuration Update Request [VENDOR CASE]...");

	if(msgPtr == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);

	if((iPtr = ((int*)CWThreadGetSpecific(&gIndexSpecific))) == NULL) {
		CWLog("CWThreadGetSpecific NULL");
	  	return CW_FALSE;
	}
	//CWLog("*iPtr = %d",*iPtr);
	valuesPtr =gWTPs[*iPtr].vendorValues;

	//Debug
#if 0
	xmlTemp = "<?xml version=\" 1.0\"?>\
<config>\
\
<SSID>1</SSID>\
\
</config>";
	xmlLen = strlen(xmlTemp)*sizeof(char);
	if (xmlTemp != NULL) {
					/* create message */
					msgType = CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_CONFIG;
					CW_CREATE_PROTOCOL_MESSAGE(*msgPtr, sizeof(int)+sizeof(short)+sizeof(short)+xmlLen, return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
					CWLog("CWAssembleWTPVendorPayloadXML xmlLen = %d",xmlLen);
					CWProtocolStore32(msgPtr, CW_MSG_ELEMENT_VENDOR_IDENTIFIER);
					CWProtocolStore16(msgPtr, (unsigned short) msgType);
					CWProtocolStore16(msgPtr, (unsigned short) xmlLen);
					CWProtocolStoreStr(msgPtr, xmlTemp);

		}
#endif
	if(valuesPtr != NULL)
	{
		//CWLog("CWAssembleWTPVendorPayloadXML valuesPtr != NULL");
	switch (valuesPtr->vendorPayloadType){
			case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_CONFIG:
			case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_STATE:
				msgType = valuesPtr->vendorPayloadType;
				//add string end 0
				xmlLen = valuesPtr->vendorPayloadLen;
				if(xmlLen > 0)
				{
					//CW_CREATE_STRING_ERR(xmlPtr->payload, valuesPtr->vendorPayloadLen+1, {CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL); return 0;});
					//memset(xmlPtr->payload,0,valuesPtr->vendorPayloadLen+1);
					//memcpy(xmlPtr->payload, (CWVendorXMLValues *) valuesPtr->payload, valuesPtr->vendorPayloadLen);
					//CW_CREATE_PROTOCOL_MESSAGE(*msgPtr, sizeof(int)+sizeof(short)+sizeof(short)+xmlLen, return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
					CW_CREATE_PROTOCOL_MESSAGE(*msgPtr, sizeof(int)+sizeof(short)+xmlLen, return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
//					CWProtocolStore32(msgPtr, CW_MSG_ELEMENT_VENDOR_IDENTIFIER);
					CWProtocolStore16(msgPtr, (unsigned short) msgType);
//					CWProtocolStore16(msgPtr, (unsigned short) xmlLen);
					CWProtocolStore32(msgPtr, (unsigned int) xmlLen);
					CWProtocolStoreStr(msgPtr, (char *) valuesPtr->payload);
				}
				else 
				{
					/* create message */
					CW_CREATE_PROTOCOL_MESSAGE(*msgPtr,  sizeof(int)+sizeof(short), return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
				//CWProtocolStore32(msgPtr, CW_MSG_ELEMENT_VENDOR_IDENTIFIER);
					CWProtocolStore16(msgPtr, (unsigned short) msgType);
					CWProtocolStore32(msgPtr, (unsigned int) xmlLen);
				}
			break;
			default:
				return CW_FALSE;
			break;
	}
	}
	else
	{
		CWLog("CWAssembleWTPVendorPayloadXML valuesPtr == NULL");
		return CW_FALSE;
	}
	CWLog("Assembling Protocol Configuration Update Request [VENDOR CASE]: Message Assembled.");
	//add free
	CW_FREE_OBJECT(gWTPs[*iPtr].vendorValues->payload);
	CW_FREE_OBJECT(gWTPs[*iPtr].vendorValues);

	return CWAssembleMsgElem(msgPtr, CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_CW_TYPE);
}


CWBool CWAssembleWTPVendorPayloadPortal(CWProtocolMessage *msgPtr) {
	int* iPtr;
	unsigned short  msgType;
	//unsigned short  msgLen = 0;

	CWLog("Assembling Protocol Configuration Update Request [VENDOR CASE Portal]...");

	if(msgPtr == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);

	if((iPtr = ((int*)CWThreadGetSpecific(&gIndexSpecific))) == NULL) {
		CWLog("CWThreadGetSpecific NULL");
	  	return CW_FALSE;
	}
	CWLog("CWAssembleWTPVendorPayloadPortal *iPtr = %d",*iPtr);

	if(gWTPs[*iPtr].vendorPortalValues == NULL)
	{
		CWLog("CWAssembleWTPVendorPayloadPortal valuesPtr == NULL !");
		return CW_FALSE;
	}
	else
	{
		if(gWTPs[*iPtr].vendorPortalValues->EncodeName == NULL || gWTPs[*iPtr].vendorPortalValues->EncodeContent == NULL)
		{
			CWLog("EncodeName or EncodeContent == NULL!");
			return CW_FALSE;
		}
		CWLog("CWAssembleWTPVendorPayloadPortal valuesPtr != NULL");
		msgType = CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_PORTAL;
		if(gWTPs[*iPtr].vendorPortalValues->EncodeNameLen == 0 || gWTPs[*iPtr].vendorPortalValues->EncodeContentLen == 0)
		{
			CWLog("EncodeNameLen or EncodeContentLen == 0!");
			return CW_FALSE;
		}	
		//msgLen = 2*sizeof(short) + gWTPs[*iPtr].vendorPortalValues->EncodeNameLen + gWTPs[*iPtr].vendorPortalValues->EncodeContentLen;
		
		CW_CREATE_PROTOCOL_MESSAGE(*msgPtr,2*sizeof(int)+3*sizeof(short)+gWTPs[*iPtr].vendorPortalValues->EncodeNameLen+gWTPs[*iPtr].vendorPortalValues->EncodeContentLen, return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
		
		CWProtocolStore16(msgPtr, msgType);
		//No msgLen
		//CWProtocolStore16(msgPtr, msgLen);
		//CWProtocolStoreStr(msgPtr, xmlPtr->payload);
		//CWLog("[F:%s, L:%d]",__FILE__,__LINE__);
		//fragment
		CWProtocolStore16(msgPtr, gWTPs[*iPtr].vendorPortalValues->isLast);
		CWProtocolStore32(msgPtr, gWTPs[*iPtr].vendorPortalValues->SeqNum);
		
		CWProtocolStore16(msgPtr, gWTPs[*iPtr].vendorPortalValues->EncodeNameLen);
		//CWLog("[F:%s, L:%d]",__FILE__,__LINE__);
		CWProtocolStoreRawBytes(msgPtr,  gWTPs[*iPtr].vendorPortalValues->EncodeName,gWTPs[*iPtr].vendorPortalValues->EncodeNameLen);
		//CWLog("[F:%s, L:%d]",__FILE__,__LINE__);
		CWProtocolStore32(msgPtr, gWTPs[*iPtr].vendorPortalValues->EncodeContentLen);
		//CWLog("[F:%s, L:%d]",__FILE__,__LINE__);
		CWProtocolStoreRawBytes(msgPtr,  gWTPs[*iPtr].vendorPortalValues->EncodeContent,gWTPs[*iPtr].vendorPortalValues->EncodeContentLen);

		CW_FREE_OBJECT(gWTPs[*iPtr].vendorPortalValues->EncodeName);
		CW_FREE_OBJECT(gWTPs[*iPtr].vendorPortalValues->EncodeContent);
			
	}
	
	CWLog("Assembling Protocol Configuration Update Request [VENDOR CASE Portal]: Message Assembled.");
	
	//need total & No
	//CW_FREE_OBJECT(gWTPs[*iPtr].vendorPortalValues);


	return CWAssembleMsgElem(msgPtr, CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_CW_TYPE);
}


