#ifndef EPOLL_DATA_H
#define EPOLL_DATA_H

#include "std_comm.h"
#include "epoll.h"
#include "create_private.h"

struct epoll_oper {
	int (*epoll_recv)(unsigned long epoll_data);
	int (*epoll_send)(unsigned long epoll_data);
	int (*epoll_err)(int error_num, unsigned long epoll_data);
};

int
epoll_data_add(
	int		sock,
	int		event,	
	unsigned long	user_data
);

int
epoll_config_set(
	int duration,
	struct epoll_oper *oper
);

int
epoll_start();

int
epoll_data_recv_mod(
	int		sock,
	unsigned long	data
);

int
epoll_data_mod(
	int		sock,
	int		event,	
	unsigned long	user_data
);

int
epoll_data_del(
	int sock
);

int
epoll_data_send_mod(
	int		sock,
	unsigned long	data
);

#endif
