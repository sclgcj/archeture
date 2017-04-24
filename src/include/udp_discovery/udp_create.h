#ifndef UD_CREATE_H
#define UD_CREATE_H

#include "comm.h"

struct ud_recv_data {
	int  recv_len;
	char *recv_data;
	struct list_head node;
};

struct ud_create_data {
	struct list_head head;
	pthread_mutex_t mutex;
	char user_data[0];
};

#endif
