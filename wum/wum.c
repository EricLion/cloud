#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

#include "wumLib.h"

void printWTPList(struct WTPInfo *wtpList, int nWTPs);
void printConfigHeader();
void printConfigInfo(struct config_info *c_info, int wtpId, struct WTPInfo *wtpList);
void printConfigFooter();
void do_config_cmd(int acserver, char *wtpIds, char *wtpNames);
void do_state_cmd(int acserver, char *wtpIds, char *wtpNames);
void do_update_cmd(int acserver, char *wtpIds, char *wtpNames, char *cup_path);
void do_cancel_cmd(int acserver, char *wtpIds, char *wtpNames);
int get_cmd_id(char *cmd);
char *WTP_id2name(int id);
void usage(char *name);

#define ACSERVER_ADDRESS "127.0.0.1"
#define ACSERVER_PORT	1235

/* Commands */
#define CMD_NUM 5
typedef struct { char id; const char *name; } cmd_t;

enum {NO_CMD, WTPS_CMD,STATE_CMD, CONFIG_CMD, UPDATE_CMD, CANCEL_CMD};

cmd_t CMDs[] = {
	{WTPS_CMD, "wtps"},
	{CONFIG_CMD, "config"},
	{STATE_CMD, "state"},
	{UPDATE_CMD, "update"},
	{CANCEL_CMD, "cancel"},
	{NO_CMD, ""}
};

/* Global WTP List */
struct WTPInfo *wtpList;
int nWTPs;

int main(int argc, char *argv[])
{
	int acserver, wtpId, cmd_id;
	void *cup;
	struct version_info update_v; 
    char *command = NULL, *cup_path = NULL;
    char *wtpIds = NULL, *wtpNames = NULL;
    char *acserver_address = ACSERVER_ADDRESS;
	int acserver_port = ACSERVER_PORT;;
	int index;
    int c;
    
    opterr = 0;
	/* Parse options */
    while ((c = getopt (argc, argv, "ha:p:w:c:f:n:")) != -1)
        switch (c)
        {
        fprintf(stderr, "--%s---%d--",__FILE__,__LINE__);
		case 'a':
			acserver_address = optarg;
			break;
		case 'p':
			acserver_port = atoi(optarg);
			break;
        case 'w':
            wtpIds = optarg;
            break;
        case 'n':
        	wtpNames = optarg;
        	break;
        case 'c':
            command = optarg;
            break;
        case 'f':
        	cup_path = optarg;
        	break;
        case 'h':
        	usage(argv[0]);
        	break;	
        case '?':
            if (optopt == 'w' || optopt == 'c' || optopt == 'f' || optopt == 'n')
           		fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt))
            	fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
           		fprintf (stderr,
                    "Unknown option character `\\x%x'.\n",
                    optopt);
            exit(EXIT_FAILURE);
        default:
    		usage(argv[0]);
            abort();

		}
     
    /* Check arguments */ 
	if (command == NULL) {
		fprintf(stderr, "No command specified!\n");
		exit(EXIT_FAILURE);
	}
	
	if ((cmd_id = get_cmd_id(command)) == NO_CMD) {
		fprintf(stderr, "Wrong command specified!\n");
	}

	/* Connect to server and get WTPs list */
	acserver = ACServerConnect(acserver_address, acserver_port);
	wtpList = ACServerWTPList(acserver, &nWTPs);
//	fprintf(stderr, "sizeof(int)= %d\n",sizeof(int));
//	fprintf(stderr, "sizeof(char *)= %d\n",sizeof(char *));

	/* Execute command */
	switch(cmd_id) {
		case WTPS_CMD:
			printWTPList(wtpList, nWTPs);
			break;
		case CONFIG_CMD:
			//do_version_cmd(acserver, wtpIds, wtpNames);
			do_config_cmd(acserver, wtpIds, wtpNames);
			break;
		case STATE_CMD:
			do_state_cmd(acserver, wtpIds, wtpNames);
			break;
		case UPDATE_CMD:
			do_update_cmd(acserver, wtpIds, wtpNames, cup_path);
			break;
		case CANCEL_CMD:
			do_cancel_cmd(acserver, wtpIds, wtpNames);
			break;
	}

	freeWTPList(wtpList, nWTPs);
	ACServerDisconnect(acserver);

	exit(EXIT_SUCCESS);
}

int sanitize_wtp_list(int *work_list, int n)
{
	int i, j, z, new_size = n;

	/* Delete unknown wtp ids */
	for(i = 0; i < new_size; i++) {
		if (WTP_id2name(work_list[i]) == NULL) {
			for (j = i; j < new_size - 1; j++) {
				work_list[j] = work_list[j+1];
			}
			i--;
			new_size--;
		}
	}
	
	/* Delete duplicates */
	for(i = 0; i < new_size; i++) {
		for (j = i + 1; j < new_size; j++) {
			if (work_list[i] == work_list[j]) {
				for (z = j; z < new_size - 1; z++) {
		        	work_list[z] = work_list[z+1];
		        }
				j--;
				new_size--;
			}
		} 
	}

	return new_size;
}

int *all_WTPs()
{
	int *ret, i;
	fprintf(stderr, "--%s---%d--,all_WTPs\n",__FILE__,__LINE__);
	fprintf(stderr, "--%s---%d--,nWTPs = %d\n",__FILE__,__LINE__,nWTPs);
	if( nWTPs == 0)
		return NULL;
	ret = malloc(nWTPs*sizeof(int));
	
	for (i = 0; i < nWTPs; i++) {
		ret[i] = wtpList[i].wtpId;
		fprintf(stderr, "--%s---%d--,ret[i] = %d\n",__FILE__,__LINE__,ret[i]);
	}
	fprintf(stderr, "--%s---%d--,ret = %d\n",__FILE__,__LINE__,ret);
	return ret;
}

int count_tokens(char *str1, char *str2)
{
	int n = 1;
	char *ptr;

	if (str1 != NULL)
		ptr = str1;
	else if (str2 != NULL) 
		ptr = str2;
	else
		return 0;
	
	while (*ptr != '\0') {
		if (*ptr == ',' && *(ptr + 1) != ',' && *(ptr + 1) != '\0') 
			n++;
		ptr++;
	}
	return n;
}

int *get_id_list(char *wtpIds, char *wtpNames, int *n)
{
	char *token, *ptr;
	int *ret = NULL;
	int i;

	*n = count_tokens(wtpIds, wtpNames); 
	//fprintf(stderr, "--%s---%d--*n = %d",__FILE__,__LINE__,*n);
	if (*n <= 0) return NULL;
	
	/* allocate memory */

	ret = malloc(*n*sizeof(int));
	if (ret == NULL) {
		perror("malloc error!");
		return NULL;
	}

	if (wtpIds != NULL) {
		/* read ids */
//		fprintf(stderr, "--%s---%d--,*wtpIds=%s\n",__FILE__,__LINE__,wtpIds);
		token = malloc(sizeof(char)*sizeof(wtpIds));
		strcpy(token,(const char*)wtpIds);
		token = strtok(token,(const char*)",");
		//fprintf(stderr, "--%s---%d--,*token=%s",__FILE__,__LINE__,token);
		ret[0] = atoi(token);
//		fprintf(stderr, "--%s---%d--,ret[0]= %d\n",__FILE__,__LINE__,ret[0]);
		if (ret[0] == -1) 
			return all_WTPs();
		
		for (i = 1; i < *n; i++)
			ret[i] = atoi( (const char*)strtok(NULL, (const char*)",") );
		
	} else {	
		/* read names and convert into ids */
		for (i = 0; i < *n; i++) {
			int id;

			if (i == 0) {
				token = malloc(sizeof(char)*sizeof(wtpNames));
				strcpy(token,(const char*)wtpNames);
				token = (char*)strtok(token, (const char*)",");
				if (strcmp(token, "all") == 0)
					return all_WTPs();
				
			} else {
				token = (char*)strtok(NULL, (const char*)",");
			}
			
			if ((id = WTP_name2id(token)) == -1) {
				fprintf(stderr, "%s: specified WTP does not exits\n", token);
			}

			ret[i] = id;
		}
	} 
	/* remove duplicated and unknown WTP ids */
	*n = sanitize_wtp_list(ret, *n);

	return ret;
}
#if 0
void do_version_cmd(int acserver, char *wtpIds, char *wtpNames)
{
	int *wtps, n, i;
	struct version_info v_info;

	/* WTP work list */
	wtps = get_id_list(wtpIds, wtpNames, &n);

	
	if (wtps == NULL) {
		fprintf(stderr, "Either a list of wtp ids or wtp names must be specified!\n");
		return;
	}

	printConfigHeader();	
	for (i = 0; i < n; i++) {
		//WUMGetWTPVersion(acserver, wtps[i], &v_info);
		WUMGetWTPVersion(acserver, wtps[i], &v_info);
		printConfigInfo(v_info, wtps[i], wtpList);	
	}
	printVersionFooter();
}
#endif

void do_config_cmd(int acserver, char *wtpIds, char *wtpNames)
{
	int *wtps, n, i;
	struct config_info c_info;

	/* WTP work list */
	wtps = get_id_list(wtpIds, wtpNames, &n);


	if (wtps == NULL) {
		fprintf(stderr, "Either a list of wtp ids or wtp names must be specified!\n");
		return;
	}

	printConfigHeader();
	for (i = 0; i < n; i++) {
		//WUMGetWTPVersion(acserver, wtps[i], &v_info);
		WUMConfigWTPByXML(acserver, wtps[i], &c_info, WTP_CONFIG_REQUEST);
//		fprintf(stderr, "--%s---%d--\n",__FILE__,__LINE__);
		printConfigInfo(&c_info, wtps[i], wtpList);
	}
	printConfigFooter();
}

void do_state_cmd(int acserver, char *wtpIds, char *wtpNames)
{
	int *wtps, n, i;
	struct config_info c_info;

	/* WTP work list */
	wtps = get_id_list(wtpIds, wtpNames, &n);


	if (wtps == NULL) {
		fprintf(stderr, "Either a list of wtp ids or wtp names must be specified!\n");
		return;
	}

	printConfigHeader();
	for (i = 0; i < n; i++) {
		//WUMGetWTPVersion(acserver, wtps[i], &v_info);
		WUMConfigWTPByXML(acserver, wtps[i], &c_info, WTP_STATE_REQUEST);
//		fprintf(stderr, "--%s---%d--\n",__FILE__,__LINE__);
		printConfigInfo(&c_info, wtps[i], wtpList);
	}
	printConfigFooter();
}

void do_update_cmd(int acserver, char *wtpIds, char *wtpNames, char *cup_path)
{
	int *wtps, n, i, ret;
	struct version_info update_v;
	void *cup;

	if (cup_path == NULL) {
		fprintf(stderr, "In order to execute an update, an update package must be specified! (-f pathname)\n");
		return;
	}

	/* WTP work list */
	wtps = get_id_list(wtpIds, wtpNames, &n);
	if (wtps == NULL) {
		fprintf(stderr, "Either a list of wtp ids or wtp names must be specified!\n");
		return;
	}

	int fd = open(cup_path, O_RDONLY);
	if (fd < 0) {
		perror("open error");
		return;
	}
	
	if (WUMReadCupVersion(cup_path, &update_v)) {
		return;
	}
	
	cup = mmap(NULL, update_v.size, PROT_READ, MAP_SHARED , fd, 0);
	if (cup == NULL) {
		perror("mmap error");
		return;
	}

	printf("*--------*--------------------------------*------------*\n");
	printf("| %-6s | %-30s | %-10s |\n", "WTP Id", "WTp Name", "Result");
	printf("*--------*--------------------------------*------------*\n");
	for (i = 0; i < n; i++) {
		ret = WUMUpdate(acserver, i, cup, update_v);
		printf("| %-6d | %-30s | %-10s |\n", wtpList[i].wtpId, wtpList[i].name, (ret == 0) ? "SUCCESS" : "FAILURE");
	}
	printf("*--------*--------------------------------*------------*\n");

	munmap(cup, update_v.size);
	close(fd);
}

void do_cancel_cmd(int acserver, char *wtpIds, char *wtpNames)
{
	int *wtps, n, i;
	struct version_info v_info;

	/* WTP work list */
	wtps = get_id_list(wtpIds, wtpNames, &n);

	if (wtps == NULL) {
		fprintf(stderr, "Either a list of wtp ids or wtp names must be specified!\n");
		return;
	}


	for (i = 0; i < n; i++) {
		if (WUMSendCancelRequest(acserver, wtps[i])) {
			fprintf(stderr, "Error while handling cancel request to WTP %d.\n", wtps[i]);
		} else {
			printf("Cancel request sent for WTP %d\n", wtps[i]);
		}
	}
}

int WTP_name2id(char *name)
{
	int i;

	for (i = 0; i < nWTPs; i++) {
		if (strcmp(name, wtpList[i].name) == 0) {
			/* found WTP! */
			return wtpList[i].wtpId;
		}
	}

	return -1;
}

char *WTP_id2name(int id)
{
	int i;

	for(i = 0; i < nWTPs; i++) {
		if (wtpList[i].wtpId == id) return wtpList[i].name;
	}

	return NULL;
}

int get_cmd_id(char *cmd)
{
	int i;
	for (i = 0; i < CMD_NUM; i++) {
		if (strcmp(CMDs[i].name, cmd) == 0)
			break;
	}
	//fprintf(stderr, "--%s---%d--,CMDs[i].name = %s,CMDs[i].id = %d",__FILE__,__LINE__,CMDs[i].name,CMDs[i].id);
	return CMDs[i].id;
}

void printWTPList(struct WTPInfo *wtpList, int nWTPs)
{
	int i;
	
	printf("*-------*--------------------------------*\n");
	printf("| %5s | %-30s |\n", "WTPId", "WTPName");
	printf("*-------*--------------------------------*\n");
	printf("| %5s | %-30s |\n", "-1", "all");

	if (wtpList != NULL) {
		for (i = 0; i < nWTPs; i++) {
			printf("| %5d | %-30s |\n", wtpList[i].wtpId, wtpList[i].name);
		}
	}

	printf("*-------*--------------------------------*\n");
}

void printConfigHeader()
{
	printf("*-------*--------------------------------*----------------------------*\n");
	printf("| %5s | %-30s | %5s | %15s |\n", "WTPId", "WTPName", "Result", "Config");
	printf("*-------*--------------------------------*----------------------------*\n");
}

void printConfigInfo(struct config_info *c_info, int wtpId, struct WTPInfo *wtpList)
{
	char Result[8];

	strcpy(Result, "FAILE");
	fprintf(stderr, "--%s---%d--\n",__FILE__,__LINE__);
	if(!c_info->resultCode)
	{
		strcpy(Result, "SUCCESS");
	}
	fprintf(stderr, "--%s---%d--\n",__FILE__,__LINE__);
	if(c_info->xml)
	{
		fprintf(stderr, "--%s---%d--\n",__FILE__,__LINE__);
		printf("| %5d | %-30s | %5s |  %-15s |\n", wtpList[wtpId].wtpId, wtpList[wtpId].name, Result, c_info->xml);
		free(c_info->xml);
		return;
	}
	printf("| %5d | %-30s | %5s |  %-15s |\n", wtpList[wtpId].wtpId, wtpList[wtpId].name, Result, "None");
	fprintf(stderr, "--%s---%d--\n",__FILE__,__LINE__);
}

void printConfigFooter()
{
	printf("*-------*--------------------------------*------------------------------*\n");
}

void usage(char *name)
{
	printf("%s -c command [-w id1,...] [-n name1,...] [-f cup_file] [-a address] [-p port]\n", name);
	printf("\nAvailable commands:\n");
	printf("   wtps: list of active wtps.\n");	
	printf(" state:  get details of the specified list of wtps (use -w or -n).\n");
	printf(" config: config of the specified list of wtps (use -w or -n).\n");
	printf(" update: sends a cup (specified with -f) to the specified list of wtps (use -w or -n).\n");		
	printf(" cancel: cancel a pending update on the desired wtps (use -w or -n).\n");			
}
