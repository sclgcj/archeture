#include "std_comm.h"
#include "err.h"
#include "print.h"
#include "thread.h"
#include "epoll_private.h"
#include "handle_private.h"
#include "create_private.h"
#include "addr_inet_private.h"
#include "global_log_private.h"
#include "addr_manage_private.h"
#include "transfer_proto_comm.h"
#include "transfer_proto_comm_private.h"

static struct io_data*
io_data_create(
	char *data,
	int  data_len,
	struct address *ta
)
{
	struct io_data *io_data = NULL;

	io_data = (struct io_data *)calloc(1, sizeof(*io_data));
	if (!io_data) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return NULL;
	}
	io_data->data = (char*)calloc(data_len, sizeof(char));
	if (!io_data->data) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return NULL;
	}
	memcpy(io_data->data, data, data_len);
	io_data->data_len = data_len;
	if (ta) {
		io_data->addr = address_create(ta);
		io_data->addr_type = ta->addr_type;
	}

	return io_data;
}

static void
io_data_destroy(
	struct io_data *io_data
)
{
	if (!io_data)
		return;

	FREE(io_data->addr);
	FREE(io_data->data);
	FREE(io_data);
}

int
transfer_proto_comm_accept(
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

	create_data = create_data_calloc(
					cl_data->config->transfer_proto, 
					0, 0, 
					cl_data->link_data.local_addr, 
					addr.sin_addr, 
					ntohs(addr.sin_port),
					cl_data->link_data.local_port, 
					0);
	if (!create_data) 
		PANIC("Not enough memory");

	//create_user_data_get(create_data->link_id, &create_data->user_data);

	epoll_data = create_link_data_alloc(
					sock, 
					cl_data->app_proto,
					addr.sin_addr.s_addr,
					ntohs(addr.sin_port),
					cl_data->config,
					create_data);
	if (!epoll_data) 
		PANIC("no enough memory\n");

	create_link_create_data_destroy(create_data);

	handle_node_add((unsigned long)epoll_data);

	return OK;
}

static int
transfer_proto_comm_recv_multi_packet_handle(
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
			memset(data->recv_data + recv_size, 0, ret);
			continue;
		}
		//包不完整, 这里我们假定应用程序会自动保存已经接收到的数据
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
transfer_proto_comm_recv_err_handle(
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
		if (errcode == ECONNREFUSED)
			return PEER_REFUSED;
		if (errcode == ENOTCONN)
			return NOT_CONNECT;

		return RECV_ERR;
	}
}

int
transfer_proto_comm_udp_data_recv(
	struct create_link_data *cl_data
)
{
	int ret = 0;
	int addr_len = 0;
	int recv_size = 0;
	struct address *ta = NULL;
	struct sockaddr_in addr;
	struct link_private_data *data = &cl_data->private_link_data;

	data->recv_cnt = 65535;
	data->recv_data = (char *)calloc(1, data->recv_cnt);
	if (!data->recv_data) 
			PANIC("Not enough memory for %u\n", data->recv_cnt);
	addr_len = sizeof(addr);
	memset(&addr, 0, addr_len);
	while (1) {
		recv_size = recvfrom(cl_data->private_link_data.sock, 
				 data->recv_data, 
				 data->recv_cnt, 0,
				 (struct sockaddr*)&addr, 
				 &addr_len);
		if (!cl_data->epoll_oper->recv_data)
			break;
		if (recv_size > 0) {
			(ta) = address_encode(addr_inet_id_get(), (struct sockaddr*)&addr);
			ret = cl_data->epoll_oper->udp_recv_data(
						data->recv_data, 
						recv_size, 
						(ta),
						cl_data->user_data);	
			if (ret == ERR) 
				goto out;
		}
		break;
	}
	if (recv_size <= 0) {
		ret = transfer_proto_comm_recv_err_handle(recv_size, errno);
	}

out:
	FREE(data->recv_data);
	data->recv_data = NULL;
	data->recv_cnt = 65535;

	return ret;
}

int
transfer_proto_comm_data_recv(
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
			ret = transfer_proto_comm_recv_multi_packet_handle(
								recv_size, data, cl_data);
			if (ret <= 0)
				break;
			continue;
		}
		break;
	}
	if (recv_size <= 0 || ret < 0) {
		ret = transfer_proto_comm_recv_err_handle(recv_size, errno);
		if (ret == WOULDBLOCK)
			goto out;
	}

	FREE(data->recv_data);
	data->recv_data = NULL;
	data->recv_cnt = DEFAULT_RECV_BUF;

out:
	return ret;
}

int	
transfer_proto_comm_udp_data_send(
	struct create_link_data *cl_data
)
{
	int ret = 0;
	int len = 0;
	struct list_head *sl = NULL;
	struct io_data *io_data = NULL;
	struct link_private_data *data = NULL;

	data = &cl_data->private_link_data;
	while (1) {
		if (thread_test_exit() == OK)
			break;
		pthread_mutex_lock(&data->send_mutex);
		if (list_empty(&data->send_list))
			ret = ERR;
		else {
			io_data = list_entry(data->send_list.next, struct io_data, node);
			list_del_init(&io_data->node);
			ret = OK;
		}
		pthread_mutex_unlock(&data->send_mutex);
		if (ret == ERR) {
			ret = OK;
			break;
		}
		len = address_length(io_data->addr_type);
		ret = sendto(data->sock, io_data->data, io_data->data_len, 
			     0, io_data->addr, len);
		if (ret > 0) {
			io_data_destroy(io_data);
			ret = OK;
			continue;
		} else {
			GDEBUG("err = %s\n", strerror(errno));
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EPIPE) {
				ret = WOULDBLOCK;
				break;
			}
			io_data_destroy(io_data);
			if (errno == ECONNRESET) {
				ret = PEER_RESET;
				break;
			}
			ret = SEND_ERR;
		}
		break;
	}

	return ret;
}

int	
transfer_proto_comm_data_send(
	struct create_link_data *cl_data
)
{
	int ret = 0;
	struct list_head *sl = NULL;
	struct io_data *io_data = NULL;
	struct link_private_data *data = NULL;

	data = &cl_data->private_link_data;
	while (1) {
		if (thread_test_exit() == OK)
			break;
		pthread_mutex_lock(&data->send_mutex);
		if (list_empty(&data->send_list))
			ret = ERR;
		else {
			io_data = list_entry(data->send_list.next, struct io_data, node);
			list_del_init(&io_data->node);
			ret = OK;
		}
		pthread_mutex_unlock(&data->send_mutex);
		if (ret == ERR) {
			ret = OK;
			break;
		}
		//PRINT("data = %s, data_len = %d\n\n", io_data->data, io_data->data_len);
		ret = send(data->sock, io_data->data, io_data->data_len, 0);
		if (ret > 0) {
			io_data_destroy(io_data);
			ret = OK;
			continue;
		} 
		else {
			PRINT("err = %s\n", strerror(errno));
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EPIPE) {
				ret = WOULDBLOCK;
				break;
			}
			io_data_destroy(io_data);
			if (errno == ECONNRESET) {
				ret = PEER_RESET;
				break;
			}
			ret = SEND_ERR;
		}
		break;
	}

	return ret;
}

int
transfer_proto_comm_data_send_add(
	char *send_data,
	int  send_len,
	unsigned long user_data
)
{
	int ret = 0;
	struct io_data *io_data = NULL;
	struct create_link_data *cl_data = NULL;

	cl_data = create_link_data_get(user_data);
	if (!cl_data) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}
	
	io_data = io_data_create(send_data, send_len, NULL);
	if (!io_data)
		return ERR;

	pthread_mutex_lock(&cl_data->private_link_data.send_mutex);
	list_add_tail(&io_data->node, &cl_data->private_link_data.send_list);
	pthread_mutex_unlock(&cl_data->private_link_data.send_mutex);

	epoll_data_send_mod(cl_data->private_link_data.sock, 
				(unsigned long)cl_data);

	return OK;
}

int
transfer_proto_accept_handle(
	struct create_link_data *cl_data
)
{
	int ret = 0;
	int sock = cl_data->private_link_data.sock;
	struct address *ta = NULL;
	struct sockaddr_in addr;

	sock_event_add(sock, EVENT_READ, cl_data);

	if (cl_data->epoll_oper->accept_func) {
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr = cl_data->link_data.peer_addr;
		addr.sin_port = htons(cl_data->link_data.peer_port);
		ta = address_encode(addr_inet_id_get(), (struct sockaddr*)&addr);
		ret = cl_data->epoll_oper->accept_func(
						ta, 
						cl_data->user_data);
		address_destroy(ta);
		if (ret != OK) {
			close(sock);
			create_link_data_destroy(cl_data);
			return ret;
		}
	}

	cl_data->proto_oper->proto_handle = transfer_proto_comm_data_handle;

	return OK;
}

int
transfer_proto_comm_data_handle(
	struct create_link_data *cl_data
)
{
	int ret = OK;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = cl_data->link_data.peer_addr.s_addr;
	addr.sin_port = htons(cl_data->link_data.peer_port);
	if (cl_data->epoll_oper->handle_data) 
		ret = cl_data->epoll_oper->handle_data(cl_data->user_data);

	if (ret != OK)
		ERRNO_SET(ret);

	return ret;
}

int
transfer_proto_server()
{
	return 1;
}

int
transfer_proto_client()
{
	return 0;
}

int
transfer_proto_comm_udp_data_send_add(
	char *send_data,
	int  send_len,
	struct address *ta,
	unsigned long user_data
)
{
	int ret = 0;
	struct io_data *io_data = NULL;
	struct create_link_data *cl_data = NULL;

	cl_data = create_link_data_get(user_data);
	if (!cl_data) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}
	
	io_data = io_data_create(send_data, send_len, ta);
	if (!io_data)
		return ERR;

	pthread_mutex_lock(&cl_data->private_link_data.send_mutex);
	list_add_tail(&io_data->node, &cl_data->private_link_data.send_list);
	pthread_mutex_unlock(&cl_data->private_link_data.send_mutex);

	epoll_data_send_mod(cl_data->private_link_data.sock, 
				(unsigned long)cl_data);

	return OK;
}
