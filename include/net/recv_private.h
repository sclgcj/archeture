#ifndef RECV_PRIVATE_H
#define RECV_RPIVATE_H 1

#include "std_comm.h"

struct recv_node {
	unsigned long user_data;
	struct list_head node;
};


int 
recv_node_add(
	unsigned long user_data
);

#endif
