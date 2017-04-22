#ifndef ERR_PRIVATE_H
#define ERR_PRIVATE_H 1

#include "comm.h"
#include "err.h"

struct err_handle{
	int curr_errcode;
	pthread_mutex_t err_mutex;
	hash_handle_t err_hash;
};

#endif
