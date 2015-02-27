
/*
 gcc -g -rdynamic test.c -o t
./t
objdump -d t >test.s
addr2line 0x400ae4 -e t -f
*/
#include "stdio.h"
#include "stdlib.h"
//#include <netdb.h>
//#include <curl/curl.h>
#include <stdarg.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>
#include "string.h"
#include <pthread.h>
//
#include <stddef.h>
#include <execinfo.h>
//
#include <signal.h>

//#define AC_LOG_FILE_NAME				"./ac.log.txt"

void dump(int signal)
{
		void *buffer[64] = {0};
		size_t size = 0;
		char **strings  = NULL;
		size_t i = 0;

		size = backtrace(buffer,64);
		printf("Process recive signal [SIGSEGV], %zd stack frames\n ",size);
		strings = backtrace_symbols(buffer, size);

		if(!strings)
		{
			perror("backtrace_symbols.");
			exit(EXIT_FAILURE);
		}

		
		for(i = 0;i < size;i++)
		{
			printf("%s\n",strings[i]);	
			
		}

		free(strings);
		strings = NULL;
		exit(0);

}


int main()
{
	if(signal(SIGSEGV,dump) == SIG_ERR)
	{
		perror("can't catch SIGSEGV");	
	}
	char mess[256];

	int i = 0,len =0;

	len = sizeof(mess);

	printf("len = %d\n",len);

	memset(mess,0,len);

	//mess[256] = 'a';

	*((volatile char *)0x0) = 0x9999;

	for(i=0;i<256;i++)
	{
		printf("%c",mess[i]);	
	}
	printf("\n");


}
