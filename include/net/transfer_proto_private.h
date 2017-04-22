#ifndef TRANSFER_PROTO_H
#define TRANSFER_PROTO_H 1

/*
 * We want our transfer layer to be more flexible : no matter how 
 * we change our special protocol,there will no be any effect to 
 * other module. We regard accept as recv, so we can just use same
 * interface to dispose server and client
 */
struct create_link_data;
struct transfer_proto_oper {
	int (*proto_connect)(struct create_link_data *cl_data);
	int (*proto_recv)(struct create_link_data *cl_data);
	int (*proto_send)(struct create_link_data *cl_data);
	int (*proto_handle)(struct create_link_data *cl_data);
	int (*proto_event_add)(struct create_link_data *cl_data);
	int (*proto_recv_check_start)(struct create_link_data *cl_data);
	int (*is_proto_server)();
};

enum {
	LINK_TCP_CLIENT,
	LINK_TCP_SERVER,
	LINK_UDP_CLIENT,
	LINK_UDP_SERVER,
	LINK_HTTP_CLIENT,
	LINK_UNIX_TCP_CLIENT,
	LINK_UNIX_TCP_SERVER,
	LINK_UNIX_UDP_CLIENT,
	LINK_UNIX_UDP_SERVER,
	LINK_TRASVERSAL_TCP_CLIENT,
	LINK_TRASVERSAL_TCP_SERVER,
	LINK_TRASVERSAL_UDP_CLIENT,
	LINK_TRASVERSAL_UDP_SERVER,
	LINK_MAX
};

struct transfer_proto_oper*
transfer_proto_oper_get_by_name(
	char *proto_name
);

struct transfer_proto_oper*
transfer_proto_oper_get_by_type(
	int proto_type
);

struct transfer_proto_oper*
transfer_proto_oper_get();

int
transfer_proto_add(
	struct transfer_proto_oper *oper,
	int			      *proto_id
);

int
transfer_proto_config_add(
	char *name,
	int  proto
);

#endif
