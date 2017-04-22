#ifndef RECV_CHECK_PRIVATE_H
#define RECV_CHECK_PRIVATE_H

#include "epoll_private.h"

struct recv_check_handle {
	struct timer_list_handle *list_handle;
};

struct recv_check_handle *
recv_check_create(
	int recv_timeout
);

int
recv_check_add(
		struct recv_check_handle *handle,
		struct create_link_data *epoll_data);
void
recv_check_destroy(
	struct recv_check_handle *handle	
);
#endif
