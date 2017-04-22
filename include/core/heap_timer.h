#ifndef HEAP_TIMER_H 
#define HEAP_TIMER_H

#include "std_comm.h"

enum {
	HEAP_TIMER_FLAG_INSTANT, 
	HEAP_TIMER_FLAG_CONSTANT,
	HEAP_TIMER_FLAG_MAX
};

int
heap_timer_create(
	int alarm_sec,		
	int timer_flag,
	unsigned long user_data,
	int (*timer_func)(unsigned long user_data),
	void (*timer_free)(unsigned long user_data),
	unsigned long *timer_id
);

unsigned long
heap_timer_tick_get();

void
heap_timer_destroy( 
	unsigned long id
);

struct heap_timer_data_node {
	unsigned long data;
	unsigned long parent;
	pthread_mutex_t mutex;
	struct list_head list_node;
};




#endif
