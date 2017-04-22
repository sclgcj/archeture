#ifndef TIMER_PRIVATE_H
#define TIMER_PRIVATE_H 1

#include "std_comm.h"
#include "timer.h"

struct timer_data_node {
	unsigned long data;
	unsigned long parent;
	pthread_mutex_t mutex;
	struct list_head list_node;
};

#endif
