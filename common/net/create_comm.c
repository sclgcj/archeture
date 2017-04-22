#include "std_comm.h"
#include "err.h"
#include "epoll_private.h"
#include "create_comm.h"
#include "create_private.h"


int
event_set(
	int event, 
	unsigned long user_data
)
{
	struct create_link_data *cl_data = NULL;

	cl_data = create_link_data_get(user_data);
	if (!cl_data) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	return epoll_data_mod(cl_data->private_link_data.sock, 
				 event,
				 (unsigned long)cl_data);
}

int
peer_info_get(
	unsigned long user_data,
	unsigned int  *peer_addr,
	unsigned short *peer_port
)
{
	struct create_link_data *cl_data = NULL;

	if (!user_data) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	cl_data = create_link_data_get(user_data);

	if (peer_addr)
		(*peer_addr) = cl_data->link_data.peer_addr.s_addr;
	if (peer_port)
		(*peer_port) = cl_data->link_data.peer_port;

	return OK;
}

int
local_info_get(
	unsigned long user_data,
	unsigned int  *local_addr,
	unsigned short *local_port
)
{
	struct create_link_data *cl_data = NULL;
	if (!user_data) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}
	cl_data = create_link_data_get(user_data);

	if (local_addr)
		(*local_addr) = cl_data->link_data.local_addr.s_addr;
	if (local_port)
		(*local_port) = cl_data->link_data.local_port;

	return OK;
}

int
close_link(
	unsigned long user_data
)
{
	struct create_link_data *cl_data = NULL;

	if (!user_data) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	cl_data = create_link_data_get(user_data);

	return create_link_data_destroy(cl_data);
}

