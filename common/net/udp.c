#include "std_comm.h"
#include "err.h"
#include "cmd.h"
#include "init.h"
#include "print.h"
#include "recv_check.h"
#include "epoll_private.h"
#include "create_private.h"
#include "transfer_proto_private.h"
#include "transfer_proto_comm_private.h"

static int
udp_accept(
	struct create_link_data *cl_data
)
{
	int sock = cl_data->private_link_data.sock;

	epoll_data_add(sock, EVENT_READ, (unsigned long)cl_data);
	
	return OK;
}

static int
udp_server_config_set()
{
	int id = 0;
	struct transfer_proto_oper oper;

	memset(&oper, 0, sizeof(oper));
	oper.proto_recv = transfer_proto_comm_udp_data_recv;
	oper.proto_handle = transfer_proto_comm_data_handle;
	oper.proto_connect = udp_accept;
	oper.is_proto_server = transfer_proto_client;

	transfer_proto_add(&oper, &id);

	return transfer_proto_config_add("udp_server", id);
}

static int
udp_connect(
	struct create_link_data *cl_data
)
{
	int sock = cl_data->private_link_data.sock;

	/*
	 * 在udp中，不需要进行connect，直接向服务器发包即可
	 */

	epoll_data_add(sock, EVENT_WRITE, (unsigned long)cl_data);
	recv_check_start("connect", cl_data->config->connect_timeout, cl_data->user_data);

	return OK;
}

static int
udp_client_config_set()
{
	int id = 0;
	struct transfer_proto_oper oper;

	memset(&oper, 0, sizeof(oper));
	oper.proto_recv = transfer_proto_comm_udp_data_recv;
	oper.proto_send = transfer_proto_comm_udp_data_send;
	oper.proto_handle = transfer_proto_comm_data_handle;
	oper.proto_connect = udp_connect;
	oper.is_proto_server = transfer_proto_client;

	transfer_proto_add(&oper, &id);

	return transfer_proto_config_add("udp_client", id);
}

static int
udp_multi_connect(
	struct create_link_data *cl_data
)
{
	int sock = cl_data->private_link_data.sock;	
	struct ip_mreq mreq;

	memset(&mreq, 0, sizeof(mreq));
	mreq.imr_multiaddr.s_addr = (cl_data->config->multi_ip);
	mreq.imr_interface.s_addr = cl_data->config->start_ip;
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) 
		PANIC("setsocket IP_ADD_MEMBERSHIP error: %s\n", strerror(errno));

	epoll_data_add(sock, EVENT_WRITE, (unsigned long)cl_data);

	return OK;
}

static int
udp_multi_client_config_set()
{
	int id = 0;	
	struct transfer_proto_oper oper;

	memset(&oper, 0, sizeof(oper));
	oper.proto_recv = transfer_proto_comm_udp_data_recv;
	oper.proto_send = transfer_proto_comm_udp_data_send;
	oper.proto_handle = transfer_proto_comm_data_handle;
	oper.proto_connect = udp_multi_connect;
	oper.is_proto_server = transfer_proto_client;

	transfer_proto_add(&oper, &id);

	return transfer_proto_config_add("udp_multi_client", id);
}

static int
udp_multi_accept(
	struct create_link_data *cl_data
)
{
	int sock = cl_data->private_link_data.sock;	
	struct ip_mreq mreq;

	memset(&mreq, 0, sizeof(mreq));
	mreq.imr_multiaddr.s_addr = (cl_data->config->multi_ip);
	mreq.imr_interface.s_addr = cl_data->config->start_ip;
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) 
		PANIC("setsocket IP_ADD_MEMBERSHIP error: %s\n", strerror(errno));

	epoll_data_add(sock, EVENT_WRITE, (unsigned long)cl_data);

	return OK;
}

static int
udp_multi_server_config_set()
{
	int id = 0;	
	struct transfer_proto_oper oper;

	memset(&oper, 0, sizeof(oper));
	oper.proto_recv = transfer_proto_comm_udp_data_recv;
	oper.proto_send = transfer_proto_comm_udp_data_send;
	oper.proto_handle = transfer_proto_comm_data_handle;
	oper.proto_connect = udp_multi_accept;
	oper.is_proto_server = transfer_proto_client;

	transfer_proto_add(&oper, &id);

	return transfer_proto_config_add("udp_multi_server", id);
}

static int
udp_config_set()
{
	fprintf(stderr, "udp\n");
	udp_client_config_set();
	udp_server_config_set();
	udp_multi_client_config_set();
	udp_multi_server_config_set();
	fprintf(stderr, "udp\n");

	return OK;
}

int
udp_setup()
{
}

static int
udp_uninit()
{
}

int
udp_init()
{
	int ret = 0;

	ret = user_cmd_add(udp_config_set);
	if (ret != OK)	
		return ret;

	return uninit_register(udp_uninit);
}

MOD_INIT(udp_init);
