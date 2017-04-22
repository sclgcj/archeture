#include "timer_list.h"
#include "err.h"
#include "std_comm.h"
#include "print.h"
#include "heap_timer.h"

void
timer_list_del(
	unsigned long data
)
{
	int ret = 0;
	struct heap_timer_data_node *data_node = NULL;
	struct timer_list_node *timer_node = NULL;

	if (!data)
		return;

	data_node = (struct heap_timer_data_node *)data;
	timer_node = (struct timer_list_node*)data_node->parent;
	pthread_mutex_lock(&data_node->mutex);
	data_node->data = 0;
	pthread_mutex_unlock(&data_node->mutex);
}

static int
timer_list_check(
	unsigned long data
)
{
	int ret = 0;
	unsigned long tmp_data = 0;
	struct list_head *sl = NULL;
	struct timer_list_node *timer_node = NULL;
	struct heap_timer_data_node *data_node = NULL;

	if (!data)
		return OK;

	timer_node = (struct timer_list_node *)data;
	ret = pthread_mutex_trylock(&timer_node->mutex);
	if (ret != 0) {
		if (ret == EBUSY)  {
			//GDEBUG("busy \n");
			return OK;
		}
		PANIC("pthread_mutex_trylock : %s\n", strerror(errno));
	}
	sl = timer_node->head.next;
	while (sl != &timer_node->head) {
		data_node = list_entry(sl, struct heap_timer_data_node, list_node);
		sl = sl->next;
		pthread_mutex_lock(&data_node->mutex);
		tmp_data = data_node->data;
		pthread_mutex_unlock(&data_node->mutex);
		if (tmp_data == 0) {
			list_del_init(&data_node->list_node);
			FREE(data_node);
			continue;
		}
		if (timer_node->handle_func) {
			ret = timer_node->handle_func(data_node->data);
			if (ret != OK) {
				timer_node->count--;
				list_del_init(&data_node->list_node);
				FREE(data_node);
			}
		}
	}
	pthread_mutex_lock(&timer_node->count_mutex);
	if (timer_node->count == 0)
		ret = ERR;
	else
		ret = OK;
	pthread_mutex_unlock(&timer_node->count_mutex);
	pthread_mutex_unlock(&timer_node->mutex);

	return ret;
}

static struct timer_list_node *
timer_list_node_create(
	int (*handle_func)(unsigned long data)
)
{
	struct timer_list_node *node = NULL;

	node = (struct timer_list_node *)calloc(1, sizeof(*node));
	if (!node) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return NULL;
	}
	node->handle_func = handle_func;
	INIT_LIST_HEAD(&node->head);
	pthread_mutex_init(&node->mutex, NULL);
	pthread_mutex_init(&node->count_mutex, NULL);

	return node;
}

static void
timer_list_node_free(
	struct list_head *head	
)
{
	struct list_head *sl = NULL;
	struct heap_timer_data_node *dnode = NULL;

	sl = head->next;

	while(sl != head) {
		dnode = list_entry(sl, struct heap_timer_data_node, list_node);
		list_del_init(sl);
		sl = head->next;
		FREE(dnode);
	}
}

static void
timer_list_destroy(
	unsigned long data
)
{
	struct timer_list_node *node = NULL;

	if (!data)
		return;

	node = (struct timer_list_node *)data;

	timer_list_node_free(&node->head);

	pthread_mutex_destroy(&node->mutex);
	pthread_mutex_destroy(&node->count_mutex);
	FREE(node);
}

static int
timer_list_end(
	unsigned long data
)
{
	struct timespec t2;
	struct timer_list_handle *handle = NULL;

	handle = (struct timer_list_handle*)data;
	if (!handle)
		return OK;

	clock_gettime(CLOCK_MONOTONIC, &t2);
	pthread_mutex_lock(&handle->timer_node_mutex);
	if (handle->timer_node && handle->list_timespec.ts.tv_sec > 0) {
		if (t2.tv_sec - handle->list_timespec.ts.tv_sec > 1 || 
			(t2.tv_sec - handle->list_timespec.ts.tv_sec == 1 && 
			 t2.tv_nsec - handle->list_timespec.ts.tv_nsec >= 0)) {
			clock_gettime(CLOCK_MONOTONIC, &handle->list_timespec.ts);
			heap_timer_create(
				handle->timer_sec,
				handle->timer_flag,
				(unsigned long)handle->timer_node,
				timer_list_check,
				timer_list_destroy,
				&handle->timer_node->timer_id);
			handle->timer_node = NULL;
		}
	}
	pthread_mutex_unlock(&handle->timer_node_mutex);
}

struct timer_list_handle *
timer_list_start(
	int timer_sec,
	int timer_flag,
	int (*list_func)(unsigned long data)
)
{
	struct timer_list_handle *handle = NULL;

	handle = (struct timer_list_handle *)calloc(1, sizeof(*handle));
	if (!handle) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return NULL;
	}
	/*handle->timer_node = timer_list_node_create();
	if (!handle->timer_node) {
		FREE(handle);
		return NULL;
	}
	INIT_LIST_HEAD(&handle->timer_node->head);*/
	pthread_mutex_init(&handle->list_timespec.mutex, NULL);
	pthread_mutex_init(&handle->timer_node_mutex, NULL);
	handle->timer_sec = timer_sec;
	handle->timer_flag = timer_flag;
	handle->handle_func = list_func;

	heap_timer_create(
			1, 
			HEAP_TIMER_FLAG_CONSTANT, 
			(unsigned long)handle, 
			timer_list_end,
			NULL,
			&handle->end_timer_id);

	return handle;
}

void
timer_list_handle_destroy(
	struct timer_list_handle *handle
)
{
	if (!handle)
		return;
	if (handle->timer_node)
		timer_list_destroy((unsigned long)handle->timer_node);
	//heap_timer_destroy(handle->end_timer_id);
	pthread_mutex_destroy(&handle->list_timespec.mutex);
	pthread_mutex_destroy(&handle->timer_node_mutex);
}

int
timer_list_add(
	struct timer_list_handle *handle,
	unsigned long  user_data,
	unsigned long *timer_data
)
{
	int flag = 0;
	struct timespec t2;
	struct timer_list_node *tmp = NULL;
	struct heap_timer_data_node *data_node = NULL;

	data_node = (struct heap_timer_data_node *)calloc(1, sizeof(*data_node));
	if (!data_node) {
		pthread_mutex_unlock(&handle->timer_node_mutex);
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}
	data_node->data = user_data;
	*(timer_data) = (unsigned long)data_node;
	pthread_mutex_init(&data_node->mutex, NULL);

	pthread_mutex_lock(&handle->timer_node_mutex);
	if (!handle->timer_node) {
		handle->timer_node = timer_list_node_create(handle->handle_func);
		if (!handle->timer_node) {
			pthread_mutex_unlock(&handle->timer_node_mutex);
			return ERR;
		}
		clock_gettime(CLOCK_MONOTONIC, &handle->list_timespec.ts);
	}
	data_node->parent = (unsigned long)handle->timer_node;
	//*(timer_data) = (unsigned long)handle->timer_node;
	//pthread_mutex_lock(&handle->list_timespec.mutex);
	clock_gettime(CLOCK_MONOTONIC, &t2);

	/*
	 * check if over 1 second
	 */
	tmp = handle->timer_node;
	if (t2.tv_sec - handle->list_timespec.ts.tv_sec > 1 || 
		(t2.tv_sec - handle->list_timespec.ts.tv_sec == 1 && 
		 t2.tv_nsec - handle->list_timespec.ts.tv_nsec >= 0)) {
		flag = 1;
		handle->timer_node = NULL;
	}	
	pthread_mutex_unlock(&handle->timer_node_mutex);
	//pthread_mutex_unlock(&handle->list_timespec.mutex);

	pthread_mutex_lock(&tmp->mutex);
	list_add_tail(&data_node->list_node, &tmp->head);
	tmp->count++;
	pthread_mutex_unlock(&tmp->mutex);

	if (flag == 1) {
		heap_timer_create(
				handle->timer_sec, 
				handle->timer_flag, 
				(unsigned long)tmp,
				timer_list_check,
				timer_list_destroy,
				&tmp->timer_id);
	}
	
	return OK;
}


