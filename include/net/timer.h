#ifndef TIMER_H
#define TIMER_H

#if 0
enum {
	TIMER_FLAG_INSTANT, 
	TIMER_FLAG_CONSTANT,
	TIMER_FLAG_MAX
};

int
timer_create(
	int alarm_sec,		
	int timer_flag,
	unsigned long user_data,
	int (*timer_func)(unsigned long user_data),
	void (*timer_free)(unsigned long user_data),
	int *timer_id
);

unsigned long
timer_tick_get();

void
timer_destroy( 
	int id
);
#endif

#endif
