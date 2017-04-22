#include "send_private.h"
#include "std_comm.h"
#include "init.h"
#include "cmd.h"
#include "err.h"
#include "print.h"
#include "config.h"
#include "thread.h"
#include "recv_check.h"
#include "recv_private.h"
#include "epoll_private.h"
#include "create_private.h"

struct send_data {
	int thread_num;
	int thread_stack;
	int send_thread_id;
};

static struct send_data global_send_data;

int
send_node_add(
	unsigned long epoll_data
)
{
	struct recv_node *recv_node = NULL;

	recv_node = (struct recv_node *)calloc(1, sizeof(*recv_node));
	if (!recv_node) 
		PANIC("not enough memory\n");

	recv_node->user_data = epoll_data;

	return thread_pool_node_add(global_send_data.send_thread_id, &recv_node->node);
}

static int
send_connect_handle(
	struct create_link_data *cl_data
)
{
	int ret = OK;
	int sock = cl_data->private_link_data.sock;
	int event = 0;
	int result = 0;	
	socklen_t result_len = sizeof(result);
	struct sockaddr_in addr;

	if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0) 
		return ERR;

	if (result != 0) {
		epoll_data_mod(sock, EPOLLONESHOT | EPOLLOUT, (unsigned long)cl_data);
		return ERR;
	}

	recv_check_stop("connect", cl_data->user_data);
	memset(&addr, 0, sizeof(addr));
	if (cl_data->epoll_oper->connected_func) {
		/*
		 * just do inet first, others later, because of time limit
		 */
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = cl_data->link_data.peer_addr.s_addr;
		addr.sin_port = htons(cl_data->link_data.peer_port);
		ret = cl_data->epoll_oper->connected_func(
							cl_data->user_data);
		//PRINT("ret = %d\n", ret);
		//epoll_data_mod(sock, event, (unsigned long)cl_data);
	}
	cl_data->private_link_data.status = STATUS_SEND_DATA;

	return ret;
}

static int
__send(
	struct list_head *list_node
)
{
	int ret = 0;
	struct recv_node *send_node = NULL;
	struct create_link_data *cl_data = NULL;

	send_node = list_entry(list_node, struct recv_node, node);
	cl_data = (struct create_link_data*)send_node->user_data;
	
	if (cl_data->private_link_data.status == STATUS_CONNECT)	{
		send_connect_handle(cl_data);
		goto out;
	}
	if (!cl_data->proto_oper || !cl_data->proto_oper->proto_send)
		goto out;
	ret = cl_data->proto_oper->proto_send(cl_data);

	if (ret == WOULDBLOCK) {
		epoll_data_send_mod( cl_data->private_link_data.sock, 
					(unsigned long)cl_data);
		ret = OK;
	}
	else if (ret == OK)
		epoll_data_recv_mod( cl_data->private_link_data.sock,
					(unsigned long)cl_data);

	if (ret != OK && cl_data->epoll_oper->err_handle)  {
		ret = cl_data->epoll_oper->err_handle(
						ret, 
						cl_data->user_data);
		cl_data->private_link_data.err_flag = ret;
	}
		
out:
	FREE(send_node);
	return OK;
}

static void
send_free(
	struct list_head *list_node
)
{
	struct recv_node *send_node = NULL;

	send_node = list_entry(list_node, struct recv_node, node);
	FREE(send_node);
}

static int
send_create()
{
	if (global_send_data.thread_num <= 1) 
		return thread_pool_create(
					1, 
					global_send_data.thread_stack,
					"send_data", 
					send_free,
					__send, 
					NULL, 
					&global_send_data.send_thread_id);
	else
		return thread_pool_create(
					global_send_data.thread_num,
					global_send_data.thread_stack,
					"send_data",
					send_free,
					NULL,
					__send, 
					&global_send_data.send_thread_id);
}

int
send_config_setup()
{
	global_send_data.thread_num   = THREAD_DEFAULT_NUM;
	global_send_data.thread_stack = THREAD_DEFALUT_STACK;	
	fprintf(stderr, "send\n");

	///CONFIG_ADD("send_thread_num", &global_send_data.thread_num, FUNC_NAME(INT));
	//CONFIG_ADD("send_stack_size", &global_send_data.thread_stack, FUNC_NAME(INT));

	return OK;
}

int
send_init()
{
	int ret = 0;

	memset(&global_send_data, 0, sizeof(global_send_data));
	ret = user_cmd_add(send_config_setup);
	if (ret != OK)
		return ret;

	return local_init_register(send_create);
}

MOD_INIT(send_init);
