#ifndef TRANSFER_PROTO_COMM_PRIVATE_H
#define TRANSFER_PROTO_COMM_PRIVATE_H

struct address;
struct create_link_data;
int
transfer_proto_comm_accept(
	struct create_link_data *cl_data
);

int	
transfer_proto_comm_data_send(
	struct create_link_data *cl_data
);

int
transfer_proto_comm_data_recv(
	struct create_link_data *cl_data
);

int
transfer_proto_comm_data_handle(
	struct create_link_data *cl_data
);

int
transfer_proto_accept_handle(
	struct create_link_data *cl_data
);

int
transfer_proto_comm_udp_data_recv(
	struct create_link_data *cl_data
);

int	
transfer_proto_comm_udp_data_send(
	struct create_link_data *cl_data
);

int
transfer_proto_server();

int
transfer_proto_client();

#endif
