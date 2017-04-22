#include "std_comm.h"
#include "err.h"
#include "cmd.h"
#include "init.h"
#include "print.h"
#include "recv_check.h"
#include "epoll_private.h"
#include "create.h"
#include "global_log_private.h"
#include "transfer_proto_private.h"
#include "transfer_proto_comm_private.h"

#define TCP_LISTEN_SIZE 1024

static int
tcp_accept(
	struct create_link_data *cl_data
)
{
	 int ret = 0;
	 int sock = cl_data->private_link_data.sock;

	 ret = listen(sock, TCP_LISTEN_SIZE);
	 if (ret < 0) 
		PANIC("listen error: %s\n", strerror(errno));

	 epoll_data_add(sock, EVENT_READ, (unsigned long)cl_data);

	 return OK;
}

static int
tcp_server_config_set()
{
	int id = 0;
	struct transfer_proto_oper oper;

	memset(&oper, 0, sizeof(oper));
	oper.proto_recv = transfer_proto_comm_accept;
	oper.is_proto_server = transfer_proto_server;
	oper.proto_handle =  transfer_proto_accept_handle;
	oper.proto_connect = tcp_accept;

	transfer_proto_add(&oper, &id);

	return transfer_proto_config_add("tcp_server", id);
}

static int
tcp_connect(
	struct create_link_data *cl_data
)
{
	int ret = 0;
	int sock = 0;
	int addr_size = sizeof(struct sockaddr_in);
	struct sockaddr_in server_addr;
	
	sock = cl_data->private_link_data.sock;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family	= AF_INET;
	server_addr.sin_port	= htons(cl_data->link_data.peer_port);
	server_addr.sin_addr	= cl_data->link_data.peer_addr;	

	GDEBUG("sever_ip = %s, server_port = %d\n", 
			inet_ntoa(server_addr.sin_addr), 
			ntohs(server_addr.sin_port));
	GDEBUG("sock = %d\n", sock);
	while (1) {
		ret = connect(sock, (struct sockaddr*)&server_addr, addr_size);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			else if (errno == EISCONN)
				break;
			else if (errno != EINPROGRESS && 
					errno != EALREADY && errno != EWOULDBLOCK) 
				PANIC("connect error :%s\n", strerror(errno));
			break;
		}
		break;
	}

	epoll_data_add(sock, EVENT_WRITE, (unsigned long)cl_data);
	recv_check_start("connect", cl_data->config->connect_timeout, cl_data->user_data);
	return OK;
}

static int
tcp_client_config_set()
{
	int id = 0;
	struct transfer_proto_oper oper;

	memset(&oper, 0, sizeof(oper));
	oper.proto_recv = transfer_proto_comm_data_recv;
	oper.proto_send = transfer_proto_comm_data_send;
	oper.proto_handle = transfer_proto_comm_data_handle;
	oper.proto_connect = tcp_connect;
	oper.is_proto_server = transfer_proto_client;

	transfer_proto_add(&oper, &id);

	return transfer_proto_config_add("tcp_client", id);
}

static int
tcp_config_set()
{
	tcp_client_config_set();
	tcp_server_config_set();

	return OK;
}

int
tcp_setup()
{
}

static int
tcp_uninit()
{
}

int
tcp_init()
{
	int ret = 0;

	ret = user_cmd_add(tcp_config_set);
	if (ret != OK)	
		return ret;

	return uninit_register(tcp_uninit);
}

MOD_INIT(tcp_init);
