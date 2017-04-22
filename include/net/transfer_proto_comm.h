#ifndef TRANSFER_PROTO_COMM_H
#define TRANSFER_PROTO_COMM_H

#include "addr_manage.h"

int
transfer_proto_comm_data_send_add(
	char *send_data,
	int  send_len,
	unsigned long user_data
);

int
transfer_proto_comm_udp_data_send_add(
	char *send_data,
	int  send_len,
	struct address *ta,
	unsigned long user_data
);


#endif
