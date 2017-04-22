#include "result.h"
#include "std_comm.h"
#include "err.h"
#include "print.h"
#include "create_private.h"

static int
result_traversal(
	unsigned long user_data,
	struct hlist_node *hnode,
	int *flag
)
{
	struct create_link_data *cl_data = NULL;

	cl_data = list_entry(hnode, struct create_link_data, node);		

	if (cl_data->epoll_oper->result_func) 
		cl_data->epoll_oper->result_func(cl_data->user_data);
	(*flag) = 1;

	return OK;
}

int
result ()
{
	return OK;
//	return create_link_data_traversal(0, result_traversal);
}
