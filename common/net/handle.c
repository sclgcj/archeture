#include "handle_private.h"
#include "init.h"
#include "cmd.h"
#include "err.h"
#include "print.h"
#include "config.h"
#include "thread.h"
#include "recv_private.h"
#include "create_private.h"
#include "addr_manage.h"

struct handle_data {
	int thread_num;
	int thread_stack;
	int handle_thread_id;	
};

static struct handle_data global_handle_data;

int 
handle_node_add(
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

	return thread_pool_node_add(global_handle_data.handle_thread_id, &recv_node->node);
}

static int
handle(
	struct list_head *list_node
)
{
	int ret = 0;
	struct sockaddr_in addr;
	struct sockaddr_un un_addr;
	struct recv_node *recv_node = NULL;
	struct create_link_data *cl_data = NULL;

	recv_node = list_entry(list_node, struct recv_node, node);
	cl_data = (struct create_link_data*)recv_node->user_data;

	if (!cl_data->proto_oper || !cl_data->proto_oper->proto_handle)
		goto out;
	ret = cl_data->proto_oper->proto_handle(cl_data);

	if (ret != OK && cl_data->epoll_oper->err_handle) {
		ret = cl_data->epoll_oper->err_handle(
						cur_errno_get(),
						cl_data->user_data);
		cl_data->private_link_data.err_flag = ret;
		create_link_err_handle(cl_data);
	}

out:
	FREE(recv_node);
	return OK;
}

static void
handle_free(
	struct list_head *list_node
)
{
	struct recv_node *recv_node = NULL;	
	
	recv_node = list_entry(list_node, struct recv_node, node);
	FREE(recv_node);
}

static int
handle_create()
{
	if (global_handle_data.thread_num <= 1) 
		return thread_pool_create(
				1, 
				global_handle_data.thread_stack,
				"handle_data", 
				NULL,  
				handle, 
				NULL, 
				&global_handle_data.handle_thread_id);
	else 
		return thread_pool_create(
				global_handle_data.thread_num, 
				global_handle_data.thread_stack,
				"handle_data", 
				NULL, 
				NULL, 
				handle,
				&global_handle_data.handle_thread_id);
}

static int
handle_config_setup()
{
	global_handle_data.thread_num = THREAD_DEFAULT_NUM;
	global_handle_data.thread_stack = THREAD_DEFALUT_STACK;

	fprintf(stderr, "handle\n");
	/*CONFIG_ADD(
		"handle_thread_num", 
		&global_handle_data.thread_num, 
		FUNC_NAME(INT));
	CONFIG_ADD(
		"handle_stack_size",
		&global_handle_data.thread_stack,
		FUNC_NAME(INT));*/
	return OK;
}

int
handle_init()
{
	int ret = 0;

	memset(&global_handle_data, 0, sizeof(global_handle_data));
	ret = user_cmd_add(handle_config_setup);
	if (ret != OK)
		return ret;

	return local_init_register(handle_create);
}

MOD_INIT(handle_init);
