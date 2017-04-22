#include "std_comm.h"
#include "err.h"
#include "init.h"
#include "hash.h"
#include "print.h"
#include "heap_timer.h"
#include "create_private.h"
#include "recv_check_private.h"
#include "timer_list.h"
#include "global_log_private.h"

struct recv_check_traversal_data {
	int count;
	int timeout;
	struct timespec ts;
	struct create_link_data *epoll_data;
};

static int
recv_check_traversal(
	unsigned long	  user_data,
	struct hlist_node *hnode,
	int		  *del_flag
)
{
	int ret = 0;
	struct timespec ts1;
	struct link_timeout_node *lt_node = NULL;
	struct create_link_data *epoll_data = NULL;
	struct recv_check_traversal_data *traversal_data = NULL;

	traversal_data = (struct recv_check_traversal_data *)user_data;
	//traversal_data->count++;
	epoll_data = traversal_data->epoll_data;
	lt_node = list_entry(hnode, struct link_timeout_node, node);
	ts1 = lt_node->send_time;

	/*GINFO("timeout2 = %ld, %d\n", 
			traversal_data->ts.tv_sec - ts1.tv_sec, 
			traversal_data->timeout);*/
	if (traversal_data->ts.tv_sec - ts1.tv_sec >= traversal_data->timeout && 
			(epoll_data->epoll_oper && epoll_data->epoll_oper->err_handle)) {
		//GINFO("ip = %s, port = %d\n",  inet_ntoa(epoll_data->link_data.local_addr), epoll_data->link_data.local_port);
		(*del_flag) = 1;
		pthread_mutex_lock(&epoll_data->data_mutex);
		ret = epoll_data->epoll_oper->err_handle(
				TIMEOUT, 
				epoll_data->user_data);
		pthread_mutex_unlock(&epoll_data->data_mutex);
		epoll_data->private_link_data.err_flag = ret;
	}

	return OK;
}

static int
recv_check_handle(
	unsigned long data
) 
{
	int ret = OK;
	struct recv_check_traversal_data traversal_data;

	if (!data)
		return OK;

	memset(&traversal_data, 0, sizeof(traversal_data));
	traversal_data.epoll_data = (struct create_link_data *)data;

	clock_gettime(CLOCK_MONOTONIC, &traversal_data.ts);
	pthread_mutex_lock(&traversal_data.epoll_data->timeout_data.mutex);
	if (traversal_data.epoll_data->private_link_data.status != STATUS_CONNECT) 
		traversal_data.timeout = traversal_data.epoll_data->timeout_data.recv_timeout;
	else
		traversal_data.timeout = traversal_data.epoll_data->timeout_data.conn_timeout;
	pthread_mutex_unlock(&traversal_data.epoll_data->timeout_data.mutex);

//	GINFO("888888888888--------------%p\n", traversal_data.epoll_data);
	HASH_WALK(
		traversal_data.epoll_data->timeout_data.timeout_hash,
		(unsigned long)&traversal_data, 
		recv_check_traversal);

	ret = create_link_err_handle(traversal_data.epoll_data);
	/*
	 * When a packet in this link is timeout, we will call the user defined function to 
	 * do some error handle. User should decide if this timeout is really wrong and if 
	 * they want to delete this link when this happens(set link_data.err_flag to non-zero to
	 * tell the downstream to delete this link). Server and Client's operation may different,
	 * setting err_flag to 1 means just remove the socket from epoll, setting to 2 means 
	 * free the whole data
	 */
	//GINFO("err_flag = %d\n", traversal_data.epoll_data->link_data.err_flag);
	return ret;
}

struct recv_check_handle *
recv_check_create(	
	int recv_timeout
)
{
	int ret = 0;
	struct recv_check_handle *handle = NULL;

	handle = (struct recv_check_handle*)calloc(1, sizeof(*handle));
	if (!handle) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return NULL;
	}
	
	handle->list_handle = timer_list_start(
						recv_timeout,
						HEAP_TIMER_FLAG_CONSTANT,
						recv_check_handle);
	if (!handle->list_handle) {
		FREE(handle);
		return NULL;
	}
	return handle;
}

void
recv_check_destroy(
	struct recv_check_handle *handle	
)
{
	if (handle && handle->list_handle) {
		timer_list_handle_destroy(handle->list_handle);
		FREE(handle->list_handle);
	}
}

int
recv_check_start(
	char *name,
	int  new_recv_timeout,
	unsigned long user_data
)
{
	int len = 0, ret = 0;
	struct link_timeout_node *rc_node = NULL;
	struct create_link_data *epoll_data = NULL;

	//epoll_data = (struct create_link_data *)user_data;
	epoll_data = create_link_data_get(user_data);
	if (!epoll_data->timeout_data.check_flag)
		return OK;

	GDEBUG("new_recv_timeout = %d\n", new_recv_timeout);
	pthread_mutex_lock(&epoll_data->timeout_data.mutex);
	if (new_recv_timeout)
		epoll_data->timeout_data.recv_timeout = new_recv_timeout;
	GDEBUG("recv_time = %d\n", epoll_data->timeout_data.recv_timeout);
	pthread_mutex_unlock(&epoll_data->timeout_data.mutex);
	rc_node = (struct link_timeout_node*)calloc(1, sizeof(*rc_node));
	if (!rc_node) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}
	if (name) {
		len = strlen(name);
		rc_node->name = (char*)calloc(1, len + 1);
		memcpy(rc_node->name, name, len);
	}
	clock_gettime(CLOCK_MONOTONIC, &rc_node->send_time);
	return hash_add(
			epoll_data->timeout_data.timeout_hash, 
			&rc_node->node, 
			(unsigned long)name);
}

int
recv_check_stop(
	char *name,
	unsigned long user_data
)
{
	int ret = 0;
	struct hlist_node *hnode = NULL;
	struct create_link_data *epoll_data = NULL;
	struct link_timeout_node *rc_node = NULL;

	//epoll_data = (struct create_link_data *)extra_data;
	epoll_data = create_link_data_get(user_data);
	if (!epoll_data->timeout_data.check_flag)
		return OK;
	
	hnode = hash_get(
			epoll_data->timeout_data.timeout_hash, 
			(unsigned long)name, 
			(unsigned long)name);
	if (!hnode) 
		return OK;

	return hash_del_and_destroy(
			epoll_data->timeout_data.timeout_hash, 
			hnode, 
			(unsigned long)name);
}

int
recv_check_add(
	struct recv_check_handle	*handle,
	struct create_link_data	*epoll_data
)
{
	if (!handle) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}
	return timer_list_add(
				handle->list_handle,
				(unsigned long)epoll_data,
				&epoll_data->timer_data
				);
}

