#include"user.h"

/******set config of ap*****/
const int nwrite(int fd,const void *string,int length)
{
	int nleft = 0;
	int nwritten;
	const char*ptr = NULL;
	
	ptr = string;
	nleft = length;
	while(nleft > 0)
	{
		if((nwritten = write(fd,ptr,nleft)) <= 0)
		{
			if(errno == EINTR)
				nwritten = 0;
			else
				return -1;
		}
		nleft -= nwritten;
		ptr += nwritten;
	}
	
	return length;
}



const int writeXmlToFile(const void* string,int length)
{
	int fd = 0;
	fd = open(TEMPREAD,O_WRONLY|O_CREAT);
	if(-1 == nwrite(fd,string,length))
	{
		printf("write xml config failed!\n");
		return -1;
	}
	close(fd);
	return 0;
}


const int wifiHandle(int sockfd,struct sockaddr_in*server_addr,xmlNodePtr parse_node,xmlNodePtr creat_root,xmlDocPtr creat_doc)
{
/*save wifi config*/
	xmlNodePtr wifi_node = parse_node->children;
	xmlNodePtr root_wifi = xmlNewNode(NULL,BAD_CAST"wifi");
	xmlAddChild(creat_root,root_wifi);

	while(wifi_node != NULL)
	{
		if(!xmlStrcmp(wifi_node->name,BAD_CAST"SSID"))
		{
			xmlChar* SSID_value = xmlGetProp(wifi_node,BAD_CAST"v");
printf("SSID_value = %s\n",(char*)SSID_value);
			xmlNodePtr wifi_SSID = xmlNewNode(NULL,BAD_CAST"SSID");
			xmlAddChild(root_wifi,wifi_SSID);
			xmlNewProp(wifi_SSID,BAD_CAST"rw",BAD_CAST"RW");
			xmlNewProp(wifi_SSID,BAD_CAST"t",BAD_CAST"UnsignedInt");
			xmlNewProp(wifi_SSID,BAD_CAST"v",SSID_value);
			xmlFree(SSID_value);
		}
		/*add wifi config*/
		
		wifi_node = wifi_node->next;
	}
	
		xmlChar *msg;
        int msg_len;
        xmlDocDumpFormatMemory(creat_doc,&msg,&msg_len,0);
printf("msg = %s\n",(char*)msg);


	//xmlSaveFile(TEMPWRITE,write_doc);
	
/*read tempfile */
/*	char *msg = (char*)malloc(CONFIGMSGLEN);
	memset(msg,0,CONFIGMSGLEN*sizeof(char));
	if(-1 == readXmlFromFile(TEMPWRITE,msg,CONFIGREADLEN))
	{
		printf("read tempfile falied!\n");
		return -1;
	}*/
/*socket to another process*/
printf("%s,%d\n",__FILE__,__LINE__);
	bzero(server_addr,sizeof(struct sockaddr_in));
printf("%s,%d\n",__FILE__,__LINE__);
	server_addr->sin_family=AF_INET;
printf("%s,%d\n",__FILE__,__LINE__);
	server_addr->sin_addr.s_addr=inet_addr("127.0.0.1");
printf("%s,%d\n",__FILE__,__LINE__);
	server_addr->sin_port = htons(WIFIPORT);
printf("%s,%d\n",__FILE__,__LINE__);
	if(sendto(sockfd,msg,msg_len,0,(struct sockaddr*)server_addr,sizeof(struct sockaddr_in)) != msg_len)
	{
		 printf("sendto error\n");
		 close(sockfd);
		 return -1;
	}
printf("send to success/n");	
	xmlFree(msg);
	msg = NULL;
	return 0;
}

const int configHandle(char*filename,char *msg,int msg_length)
{
/*establish socket(UDP),maybe in the future we need to use tcp*/
	int sockfd;
	struct sockaddr_in server_addr;
	sockfd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);

	if(sockfd < 0)
	{
	 printf("socket error\n\n");
	 return -1;
	}

/*parse xml buf*/

	xmlDocPtr parse_doc = NULL;		
	xmlNodePtr parse_node = NULL;
	xmlDocPtr creat_doc = NULL;
	xmlNodePtr creat_root = NULL;
	/*creat xml init*/
	creat_doc = xmlNewDoc(BAD_CAST"1.0");
	creat_root =  xmlNewNode(NULL,BAD_CAST"config");
	xmlDocSetRootElement(creat_doc,creat_root);
	/*parse xml init*/
	parse_doc=xmlParseMemory(msg,msg_length);    
	 if (NULL == parse_doc) 
	   {  
		  fprintf(stderr,"Document not parsed successfully\n");	  
		  xmlFreeDoc(parse_doc); 
		  return -1; 
	   } 
	   parse_node = xmlDocGetRootElement(parse_doc);
	   
	   if (xmlStrcmp(parse_node->name, BAD_CAST"config")) 
	   {
		  fprintf(stderr,"document of the wrong type, root node != config\n");
			printf("rootname = %s\n",(char*)parse_node->name);
			printf("line = %d\n",__LINE__);
		  xmlFreeDoc(parse_doc); 
		  return -1; 
	   }
	
    parse_node = parse_node->children;
	while(parse_node != NULL)
	{
		if(!xmlStrcmp(parse_node->name,BAD_CAST"wifi"))
		{
			wifiHandle(sockfd,&server_addr,parse_node, creat_root, creat_doc);
			xmlFreeDoc(creat_doc);
		}
		/*add more config */	
		parse_node = parse_node->next;
	}
	xmlFreeDoc(parse_doc);
	close(sockfd);
	return 0;
}

const int readn(int fd,void*vptr,int length)
{
	int nleft = 0;
	int nread = 0;
	char*ptr = NULL;
	
	ptr = vptr;
	nleft = length;
	while(nleft > 0)
	{
		if(nread = read(fd,ptr,nleft) < 0)
		{
			if(errno == EINTR)
				nread = 0;
			else
				return -1;
		}
		else if(nread == 0)
			break;
		nleft -= nread;
		ptr += nread;
	}
	return (length - nleft);
}

/*
in:filename
out:msg
*/
const int readXmlFromFile(char* filename,char*msg,int length)
{
	int fd = 0;
	
	fd = open(filename,O_RDONLY);
	int nread = 0;
	while(nread = readn(fd,msg,length))
	{
		if(-1 == nread)
		{
			return -1;
		}
		else if(nread == length)
			continue;
		break;
	}
	return 0;
	
}

 int stateHandle(char*filename,char *xmlmsg)
{
/*request configd to refresh the config.xml*/

/*need to delay?*/

/*read cofig.xml*/
char*msg = (char*)malloc(STATEMSGLENGTH);
memset(msg,0,STATEMSGLENGTH*sizeof(char));
int length = STATEREADLENGTH;
if(NULL == msg)
{
	printf("malloc failed\n");
	return -1;
}
if(-1 == readXmlFromFile(filename,msg,length))
{
	printf("read failed\n");
	return -1;
}
/*send msg to ac*/
strcpy(xmlmsg,msg);
free(msg);
msg = NULL;
return 0;
}

















