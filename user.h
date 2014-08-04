#ifndef _USER_H_
#define _USER_H_

#include<stdio.h>
#include<libxml/parser.h>
#include<libxml/tree.h>
#include<libxml/xmlstring.h>
#include<libxml/xmlmemory.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<fcntl.h>
#include<unistd.h>


#include<netinet/in.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<libxml/xmlsave.h>

#define APCOMMANDFILE "./APCommand.xml"
#define APCOMMANDSIZE 50
#define XMLKEYSIZE 50
#define XMLVALUESIZE 20
#define APCONFIGFILE "./apconfig.xml"

#define TEMPREAD "./tempread"
#define TEMPWRITE "./tempwrite"

#define STATEREADLENGTH 350
#define STATEMSGLENGTH 350

#define CONFIGREADLEN 512
#define CONFIGMSGLEN 1024
#define WIFIPORT 4000
const int nwrite(int fd,const void *string,int length);
const int writeXmlToFile(const void* string,int length);
const int configHandle(char*filename,char *msg,int msg_length);
const int wifiHandle(int sockfd,struct sockaddr_in* server_addr,xmlNodePtr read_node,xmlNodePtr write_root,xmlDocPtr write_doc);
const int readn(int fd,void*vptr,int length);
const int readXmlFromFile(char* filename,char*msg,int length);
 int stateHandle(char*filename,char *xmlmsg);

#endif


