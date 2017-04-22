#ifndef TIMER_LIST_PRIVATE_H
#define TIMER_LIST_PRIVATE_H

#include "heap_timer.h"

struct timer_list_node {
	int		 count;
	unsigned long	 timer_id;
	struct list_head head;
	pthread_mutex_t  count_mutex;
	pthread_mutex_t  mutex;
	int (*handle_func)(unsigned long data);
	struct list_head list_node;
};

struct timer_list_timespec {
	struct timespec ts;
	pthread_mutex_t mutex;
};

struct timer_list_handle {
	int timer_sec;
	int timer_flag;
	unsigned long end_timer_id;
	int (*handle_func)(unsigned long data);
	pthread_mutex_t timer_node_mutex;
	struct timer_list_node *timer_node;
	struct timer_list_timespec list_timespec;	
};

struct timer_list_handle *
timer_list_start(
	int timer_sec,
	int timer_flag,
	int (*list_func)(unsigned long data)
);

int
timer_list_add(
	struct timer_list_handle *handle,
	unsigned long  user_data,
	unsigned long *timer_data
);

void
timer_list_del(
	unsigned long data
);

void
timer_list_handle_destroy(
	struct timer_list_handle *handle
);

#endif
