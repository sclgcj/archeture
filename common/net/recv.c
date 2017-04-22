#include "recv_private.h"
#include "std_comm.h"
#include "err.h"
#include "cmd.h"
#include "init.h"
#include "print.h"
#include "thread.h"
#include "config.h"
#include "handle_private.h"
#include "epoll_private.h"
#include "epoll_private.h"
#include "create.h"

struct recv_data {
	int thread_num;
	int thread_stack;
	int recv_thread_id;
	int (*handle_func)(struct list_head *node);
};

static struct recv_data global_recv_data;

int 
recv_node_add(
	unsigned long user_data
)
{
	struct recv_node *recv_node = NULL;

	recv_node = (struct recv_node*)calloc(1, sizeof(*recv_node));
	if (!recv_node) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}
	recv_node->user_data = user_data;

	return thread_pool_node_add(global_recv_data.recv_thread_id, &recv_node->node);
}

static int
recv_unix_tcp_accept(
	struct create_link_data *cl_data 
)
{
	return OK;
}

static int
recv_unix_udp_accept( 
	struct recv_node *recv_node,
	struct create_link_data *cl_data 
)
{
	return OK;
}

#if 0
static int
recv_udp_accept(
	struct recv_node		*recv_node,
	struct sockaddr_in		*in_addr,
	struct create_link_data	*cl_data 
)
{
	int ret = OK;
	int event = 0;
	unsigned long user_data = 0;
	struct create_data *create_data = NULL;
	struct create_link_data *epoll_data = NULL;

	/*if (cl_data->epoll_oper->recv_data)
		ret = cl_data->epoll_oper->recv_data(
					cl_data->private_link_data.sock, 
					cl_data->user_data,
					in_addr);*/
	if (ret != OK) 
		return ret;

	create_data = create_data_calloc(
					NULL, 0, 0, 
					cl_data->link_data.local_addr, 
					in_addr->sin_addr,
					ntohs(in_addr->sin_port),
					cl_data->link_data.local_port,
					0);
	/*memset(&create_data, 0, sizeof(create_data));
	create_data.port = cl_data->link_data.local_port;
	create_data.addr = cl_data->link_data.local_addr;
	create_data.user_data = user_data;*/
	epoll_data = create_link_data_alloc(
					cl_data->private_link_data.sock,
					cl_data->app_proto,
					cl_data->transfer_proto,
					in_addr->sin_addr,
					ntohs(in_addr->sin_port),
					create_data);
	if (!epoll_data) 
		PANIC("no enough memory\n");
	

	if (cl_data->epoll_oper->accept_func)
		ret = cl_data->epoll_oper->udp_accept_func(
					(unsigned long)cl_data,
					in_addr, 
					&event, 
					user_data);
	if (ret != OK) {
		FREE(create_data);
		return ret;
	}

	return sock_event_add(cl_data->private_link_data.sock, event, cl_data);
}

static int 
recv_tcp_accept(
	struct create_link_data *cl_data
)
{
	int sock = 0, ret = 0;
	int addr_size = 0, event = 0;
	unsigned long user_data = 0;
	struct sockaddr_in addr;
	struct create_data *create_data = NULL;
	struct create_link_data *epoll_data = NULL;

	sock = accept(cl_data->private_link_data.sock, (struct sockaddr*)&addr, &addr_size);
	if (sock < 0) 
		PANIC("accept error: %s\n", strerror(errno));

	/*memset(&create_data, 0, sizeof(create_data));
	create_data.port = cl_data->link_data.local_port;
	create_data.addr = cl_data->link_data.local_addr;
	create_data.user_data = user_data;*/
	create_data = create_data_calloc(
					0, 0, 0, 0, 
					cl_data->link_data.local_addr, 
					addr.sin_addr, 
					ntohs(addr.sin_port),
					cl_data->link_data.local_port, 
					0);
	if (!create_data) 
		PANIC("Not enough memory");

	create_user_data_get(create_data->link_id, &create_data->user_data);

	epoll_data = create_link_data_alloc(
					sock, 
					cl_data->app_proto,
					cl_data->transfer_proto,
					addr.sin_addr,
					ntohs(addr.sin_port),
					create_data);
	if (!cl_data) 
		PANIC("no enough memory\n");

	if (cl_data->epoll_oper->accept_func) {
		ret = cl_data->epoll_oper->accept_func(
						(unsigned long)cl_data, 
						&addr, 
						&event, 
						user_data);
		if (ret != OK) {
			close(sock);
			return ret;
		}
	}

	epoll_data_mod(
			cl_data->private_link_data.sock, 
			EVENT_READ,
			(unsigned long )cl_data);
	return sock_event_add(sock, event, epoll_data);
}

static int
recv_err_handle(
	int ret,
	int errcode
)
{
	if (ret == 0) {
		if (errcode == ECONNRESET)
			return PEER_RESET;
		if (errcode == ETIMEDOUT)
			return WOULDBLOCK;
		return PEER_CLOSED;
	} else {
		if (errcode == EAGAIN || errcode == EWOULDBLOCK || 
				errcode == ETIMEDOUT || errcode == EBADF)
			return WOULDBLOCK;
		if (errcode == ECONNRESET)
			return PEER_RESET;

		return RECV_ERR;
	}
}

static int
recv_multi_packet_handle(
	int recv_size,
	struct link_private_data *data,
	struct create_link_data  *cl_data
)
{
	int ret = 0;

	while (1) {
		ret = cl_data->epoll_oper->recv_data(
					data->recv_data, 
					recv_size, 
					cl_data->user_data);	
		if (ret == ERR) 
			return ret;
		if (ret != OK) {	//成功接收到一个包
			if (ret == recv_size) //正常接收完成	
				return OK;	
			recv_size -= ret;
			if (recv_size < 0)    //返回值有问题
				return WRONG_RECV_RETURN_VALUE;
			//实际需要的数据比实际获取的数据少
			memmove(data->recv_data, 
				data->recv_data + ret, 
				recv_size);
			continue;
		}
		//包不完整
		data->recv_cnt *= 2;
		data->recv_data = (char*)realloc(
						data->recv_data, 
						data->recv_cnt);
		if (!data->recv_data)
			PANIC("Not enough memory\n");
		memset(data->recv_data, 0, data->recv_cnt);
		break;
	}
	return recv_size;
}

static int
recv_data(
	struct create_link_data *cl_data
)
{
	int ret = 0;
	int recv_size = 0;
	struct link_private_data *data = &cl_data->private_link_data;

	if (!data->recv_data) {
		data->recv_data = (char *)calloc(1, data->recv_cnt);
		if (!data->recv_data) 
			PANIC("Not enough memory for %u\n", data->recv_cnt);
	}
	while (1) {
		recv_size = recv(cl_data->private_link_data.sock, 
				 data->recv_data, 
				 data->recv_cnt, 0);
		if (!cl_data->epoll_oper->recv_data)
			break;
		if (recv_size > 0) {
			ret = recv_multi_packet_handle(recv_size, data, cl_data);
			if (ret <= 0)
				break;
			continue;
		}
		break;
	}
	if (recv_size <= 0) {
		ret = recv_err_handle(recv_size, errno);
		if (ret == WOULDBLOCK)
			goto out;
	}

	FREE(data->recv_data);
	data->recv_cnt = DEFAULT_RECV_BUF;

out:
	return ret;
}
#endif
static int
__recv(
	struct list_head *list_node
)
{
	int ret = 0, result = 0;
	struct recv_node *recv_node = NULL;
	struct create_link_data *cl_data = NULL, *new_data = NULL;

	recv_node = list_entry(list_node, struct recv_node, node);
	cl_data = (struct create_link_data *)recv_node->user_data;
	if (!cl_data)
		goto out;

	if (!cl_data->proto_oper || !cl_data->proto_oper->proto_recv)
		goto out;
	ret = cl_data->proto_oper->proto_recv(cl_data);

	result = ret;
	if (ret != OK && ret != PEER_CLOSED && ret != WOULDBLOCK) 
		goto err;

	if (ret == PEER_CLOSED) {
		ret = create_check_duration();
		goto err;
	}
	ret = epoll_data_recv_mod(cl_data->private_link_data.sock, (unsigned long)cl_data);
	if (!cl_data->proto_oper->is_proto_server() && ret != WOULDBLOCK) {
		handle_node_add((unsigned long)cl_data);
	}
	
	goto out;
err:
	if (ret != OK && cl_data->epoll_oper->err_handle) {
		ret = cl_data->epoll_oper->err_handle(
					result, 
					cl_data->user_data);
		cl_data->private_link_data.err_flag = ret;
		create_link_err_handle(cl_data);
	}
out:
	FREE(recv_node);
	return ret;
}

static void
recv_free(
	struct list_head *sl
)
{
	struct recv_node *rnode = NULL;

	rnode = list_entry(sl, struct recv_node, node);
	FREE(rnode);
}

static int
recv_create()
{
	if (global_recv_data.thread_num <= 1) {
		return thread_pool_create(
				1,
				global_recv_data.thread_stack,
				"recv_data",
				recv_free,
				__recv,
				NULL,
				&global_recv_data.recv_thread_id);
	}
	else
		return thread_pool_create(
				global_recv_data.thread_num,
				global_recv_data.thread_stack,
				"recv_data", 
				recv_free, 
				NULL,
				__recv,
				&global_recv_data.recv_thread_id);
	
}

static int
recv_config_setup()
{
	int ret = 0;	

	global_recv_data.thread_num = THREAD_DEFAULT_NUM;
	global_recv_data.thread_stack = THREAD_DEFALUT_STACK;

	fprintf(stderr,"recv\n");
	//CONFIG_ADD("recv_thread_num", &global_recv_data.thread_num, FUNC_NAME(INT));
	//CONFIG_ADD("recv_stack_size", &global_recv_data.thread_stack, FUNC_NAME(INT));

	return OK;
}

int
recv_init()
{
	int ret = 0;

	memset(&global_recv_data, 0, sizeof(global_recv_data));

	ret = user_cmd_add(recv_config_setup);
	if (ret != OK)
		return ret;

	return local_init_register(recv_create);
}

MOD_INIT(recv_init);
