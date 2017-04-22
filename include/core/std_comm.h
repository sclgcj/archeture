#ifndef STD_COMM_H
#define STD_COMM_H 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <error.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/un.h>

#include "list.h"
#include "cJSON.h"

#define THREAD_DEFAULT_NUM 1
#define THREAD_DEFALUT_STACK 32 * 1024

//#define list_entry(ptr, type, memb) \
//		((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->memb)))

#define FREE(ptr) do{ \
	if (ptr) { \
		free(ptr); \
		ptr = NULL; \
	} \
}while(0)

#endif
