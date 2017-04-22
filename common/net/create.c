#include "std_comm.h"
#include "init.h"
#include "cmd.h"
#include "err.h"
#include "log.h"
#include "hash.h"
#include "print.h"
#include "thread.h"
#include "create.h"
#include "create_log.h"
#include "recv_check.h"
#include "config_read.h"
#include "hash_hub_private.h"
#include "hub_private.h"
#include "recv_private.h"
#include "send_private.h"
#include "epoll_private.h"
#include "create_private.h"
#include "socket_private.h"
#include "handle_private.h"
#include "heap_timer.h"
#include "timer_list.h"
#include "rendezvous_private.h"
#include "recv_check_private.h"
#include "create_log_private.h"
#include "global_log_private.h"
//#include "interface_private.h"
#include "multi_proto_create_private.h"


struct create_link_node {
	char *proto;
	int  data_size;
	struct create_config *config;
	struct create_link_oper oper;
	struct list_head node;
};

struct create_handle_node {
	char *app_proto;
	unsigned int ip;
	unsigned short port;
	struct create_data *create_data;
	struct create_config *config;
	struct create_link_oper oper;
	struct list_head node;
};

struct create_config_node {
	char *proto;
	struct create_config *config;
	struct list_head node;
};

struct create_user_data {
	char *data;
	int realloc;
	int data_cnt;
	int data_size;
	pthread_mutex_t mutex;
};

struct create{
	char *app_proto;
	int create_hash_num;
	int create_link_count;
	int create_id;
	int create_link_id;
	struct list_head config_list;
	pthread_mutex_t list_mutex;
	pthread_mutex_t count_mutex;
	hash_handle_t create_link_hash;
	hash_handle_t create_proto_hash;
	struct recv_check_handle *recv_check;
	struct create_link_oper oper;
	struct create_config config;
};

struct create_hash_data {
	struct create_link_hash_node *cl_hash;
	unsigned long hash_data;
};

/*
 * create_data_add() - add a new socket
 * @port_map_cnt:  the port count of the link
 */
static int
create_data_add(
//	int proto,
//	int link_type,	
	char *proto_name,
	int transfer_flag,
	int port_map_cnt,
	struct in_addr ip,
	struct in_addr server_ip,
	unsigned short server_port,
	unsigned short port,
	struct create_link_oper *oper
);

static int
create_hash_destroy(
	struct hlist_node	*hnode
);

static void
create_config_default_set(
	struct create_config *config
);

static void
create_handle_node_destroy(
	struct list_head *list_node
);

static void
create_link_hash_node_destroy(
	struct create_link_hash_node *pnode
);

#define TIMEOUT_HASH_NUM  2
#define CREATE_BASE  100
static struct create global_create_link;

static struct create_handle_node *
create_handle_node_create(
	char *app_proto,
	struct create_data *create_data,	
	struct create_config *conf,
	struct create_link_oper *oper
)
{
	struct create_handle_node *ch_node = NULL;

	ch_node = (struct create_handle_node*)calloc(1, sizeof(*ch_node));
	if (!ch_node)
		PANIC("Not enough memory for %d bytes\n", sizeof(*ch_node));
	ch_node->app_proto = strdup(app_proto);
	ch_node->config = conf;
	ch_node->create_data = create_data;
	memcpy(&ch_node->oper, oper, sizeof(*oper));

	return ch_node;
}

/*void
create_user_data_get(
	int id,	
	unsigned long *cl_user_data
)
{
	int data_size = 0;
	int old_size = 0, new_size = 0;
	//struct create_user_data *user_data = &global_create_link.user_data;

	if (id >= user_data->data_cnt) {
		PRINT("lind num larger the one set starting\n");
		return;
	}

	data_size = user_data->data_size;
	PRINT("id = %d, data_size =%d\n", id, data_size);
	*cl_user_data = (unsigned long)&user_data->data[id * data_size];
}*/

int
create_link_err_handle(
	struct create_link_data *cl_data 
)
{
	int ret = 0;

	if (cl_data->private_link_data.err_flag == 1) {
		create_link_data_del(cl_data);
		ret = ERR;
	} else if (cl_data->private_link_data.err_flag == 2){
		create_link_data_destroy(cl_data);
		ret = ERR;
	} 	

	return ret;
}

int
create_check_duration()
{
	int tick = 0;

	tick = heap_timer_tick_get();
	if (tick + 5 >= global_create_link.config.duration) 
		return OK;

	return ERR;
}

static int
link_timeout_hash(
	struct hlist_node	*hnode,
	unsigned long		user_data
)
{
	char name = 0;
	struct link_timeout_node *lt_node = NULL;
	
	if (!hnode) {
		if (!user_data)
			return 0;
		else 
			name = ((char*)user_data)[0];
	}
	else {
		lt_node = list_entry(hnode, struct link_timeout_node, node);
		if (!lt_node->name)
			return 0;
		name = lt_node->name[0];
	}

	return (name % TIMEOUT_HASH_NUM);
}

static int
link_timeout_hash_get(
	struct hlist_node	*hnode,
	unsigned long		user_data
)
{
	struct link_timeout_node *lt_node = NULL;

	lt_node = list_entry(hnode, struct link_timeout_node, node);
	if (!user_data && !lt_node->name)
		return OK;
	if (!user_data || !lt_node->name)
		return ERR;
	if (strcmp((char*)user_data, lt_node->name))
		return ERR;

	return OK;
}

static int
link_timeout_hash_destroy(
	struct hlist_node	*hnode
)
{
	struct link_timeout_node *lt_node = NULL;

	lt_node = list_entry(hnode, struct link_timeout_node, node);
	FREE(lt_node->name);
	FREE(lt_node);

	return OK;
}

struct create_link_data *
create_link_data_get(
	unsigned long link_data
)
{
	struct create_link_data *data = NULL;

	data = list_entry(link_data, struct create_link_data, data);

	return data;
}

int
sock_event_add(
	int sock,
	int event,
	struct create_link_data *epoll_data
)
{
	int ret = 0;
	
	//add to epoll
	ret = epoll_data_add(sock, event, (unsigned long)epoll_data);
	if (ret != OK)
		return ret;
	//PRINT("add_check = %d, %p\n", global_create_link.config.add_check, epoll_data);

	//add recv check
	if (epoll_data->config->add_check) {
		if (epoll_data->private_link_data.link_type == LINK_TCP_CLIENT)
			recv_check_start("connect", 0, epoll_data->user_data);
		recv_check_add(global_create_link.recv_check, epoll_data);
	}

	return OK;
}

struct create_link_data *
create_link_data_alloc(
	int sock, 
	char *app_proto,
	unsigned int   peer_addr,
	unsigned short peer_port,
	struct create_config *conf,
	struct create_data *create_data
)
{
	int path_len = 0;
	char *path = conf->unix_path;
	char log_path[256] = { 0 };
	struct hlist_node *hnode = NULL;
	struct create_link_data *data = NULL;
	struct create_hash_data hdata;
	struct create_link_hash_node *cl_hnode = NULL, *tmp = NULL;

	hnode = hash_get(global_create_link.create_link_hash, 
			    (unsigned long)app_proto,
			    (unsigned long)app_proto);
	if (!hnode) 
		PANIC("no such protocol %s registered\n", app_proto);

	cl_hnode = list_entry(hnode, struct create_link_hash_node, node);

	data = (struct create_link_data*)calloc(1, sizeof(*data) + conf->user_data_size);
	if (!data) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return NULL;
	}
	log_address_print(cl_hnode->log_data);
	data->cl_hnode				= cl_hnode;
	data->app_proto				= cl_hnode->proto; //strdup(app_proto);
	data->private_link_data.link_id 	= create_data->link_id;
	data->user_data				= (unsigned long)data->data;
	data->timeout_data.check_flag		= conf->add_check;
	data->timeout_data.recv_timeout 	= conf->recv_timeout;
	data->timeout_data.conn_timeout 	= conf->connect_timeout;
	data->private_link_data.sock		= sock;
	data->link_data.peer_port		= peer_port;
	data->link_data.peer_addr.s_addr	= peer_addr;
	data->link_data.local_addr		= create_data->addr;
	data->link_data.local_port		= create_data->port;
	data->private_link_data.hub_interval    = conf->hub_interval;
	data->private_link_data.port_num++;
	data->private_link_data.recv_cnt	= DEFAULT_RECV_BUF;
	data->epoll_oper			= &cl_hnode->oper;
	data->config				= conf;
	if (conf->log_path[0])
		sprintf(log_path, "%s/%s_%d.txt", 
			conf->log_path, cl_hnode->proto, data->private_link_data.link_id);

	if (create_data->proto_name)
		data->proto_oper = transfer_proto_oper_get_by_name(create_data->proto_name);
	else
		data->proto_oper = transfer_proto_oper_get();
	//获取参数配置
	data->pm = param_create();
	GINFO("log_path = %s\n", log_path);
	//开启日志
	log_start((unsigned long)data->data, log_path);
	INFO((unsigned long)data->data, "heheheh");
	INIT_LIST_HEAD(&data->private_link_data.send_list);
	pthread_cond_init(&data->interface_cond, NULL);
	pthread_mutex_init(&data->interface_mutex, NULL);
	pthread_mutex_init(&data->private_link_data.mutex, NULL);
	pthread_mutex_init(&data->private_link_data.send_mutex, NULL);
	pthread_mutex_init(&data->data_mutex, NULL);
	pthread_mutex_init(&data->recv_mutex, NULL);
	if (path && path[0] != '\0') {
		path_len = strlen(data->link_data.unix_path);
		data->link_data.unix_path = (char*)calloc(1, path_len + 1);
		if (!data->link_data.unix_path) 
			PANIC("not enough memory\n");
		memcpy(data->link_data.unix_path, path, path_len);
	}

	data->timeout_data.timeout_hash = hash_create(
							TIMEOUT_HASH_NUM,
							link_timeout_hash,
							link_timeout_hash_get,
							link_timeout_hash_destroy);
	if (data->timeout_data.timeout_hash == HASH_ERR)
		create_link_data_destroy(data);

	if (cl_hnode->oper.prepare_data_get)
		cl_hnode->oper.prepare_data_get(create_data->port_num, data->user_data);

	memset(&hdata, 0, sizeof(hdata));
	hdata.cl_hash = cl_hnode;
	hdata.hash_data = data->private_link_data.link_id;
	hash_add(
		cl_hnode->create_hash, 
		&data->node, 
		(unsigned long)&hdata);

	return data;
}

static int
create_link_data_dispatch(
	int sock,
	char *app_proto,	
	struct create_config *conf,
	struct create_data *create_data,
	struct create_link_data **cl_data
)
{
	int ret = 0;
	struct hlist_node *hnode = NULL;
	struct create_hash_data hdata;
	struct create_link_data *epoll_data = NULL;
	struct create_link_hash_node *cl_hash = NULL;

	cl_hash = (struct create_link_hash_node*)conf->create_link_data;

	memset(&hdata, 0, sizeof(hdata));
	hdata.cl_hash = cl_hash;
	hdata.hash_data = create_data->link_id;
	hnode = hash_get(
			cl_hash->create_hash,
			(unsigned long)&cl_hash,
			create_data->link_id);
	if (hnode)
		epoll_data = list_entry(hnode, struct create_link_data, node);
	else
		epoll_data = create_link_data_alloc(
				sock,	
				app_proto,
				conf->server_ip,
				conf->server_port,
				conf,
				create_data);
	if (!epoll_data) 
		return NOT_ENOUGH_MEMORY;
	
	(*cl_data) = epoll_data;

	return OK;
}

static int
link_create_handle(
	struct list_head *list_node
)
{
	int ret = 0;
	int sock = 0;
	struct create_config *conf = NULL;
	struct create_data  *create_data = NULL;
	struct create_link_data *epoll_data = NULL;
	struct create_handle_node *ch_node = NULL;

	ch_node = list_entry(list_node, struct create_handle_node, node);
	conf = ch_node->config;
	create_data = ch_node->create_data;

	ret = create_socket(
			conf->proto, 
			conf->unix_path,
			create_data->addr, 
			create_data->port,
			&conf->option,
			&sock);
	if (ret != OK) {
		ret = cur_errno_get();
		goto err;
	}

	ret = create_link_data_dispatch(sock, 
					   ch_node->app_proto, 
					   conf, 
					   create_data, 
					   &epoll_data);
	if (ret != OK)
		goto err;

	if (!epoll_data->proto_oper || !epoll_data->proto_oper->proto_connect) {
		create_link_data_destroy(epoll_data);
		goto err;
	}
	ret = epoll_data->proto_oper->proto_connect(epoll_data);
	if (ret != OK) {
		ret = cur_errno_get();
		goto err;
	}
		
	if (conf->add_check)
		recv_check_add(global_create_link.recv_check, epoll_data);

	goto out;
err:
	if (epoll_data && epoll_data->epoll_oper && epoll_data->epoll_oper->err_handle) {
		ret = epoll_data->epoll_oper->err_handle(ret, epoll_data->user_data);
		epoll_data->private_link_data.err_flag = ret;
		create_link_err_handle(epoll_data);
	} 

out:
	create_handle_node_destroy(&ch_node->node);

	return OK;
}

int 
create_link_new(
	char *proto,
	char *server_path,
	unsigned int   server_ip,
	unsigned short server_port,
	unsigned long user_data,
	struct create_link_oper *oper
)
{
	int ret = 0;
	int port_map = 0;
	struct in_addr server;
	struct create_link_data *link_data = NULL;	

	if (!user_data) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}
	//link_data = (struct create_link_data *)user_data;
	link_data = list_entry(user_data, struct create_link_data, data);

	server.s_addr = server_ip;
	if (server.s_addr == 0) 
		server.s_addr == link_data->config->server_ip;
	if (server_port == 0)
		server_port = link_data->config->server_port;

	pthread_mutex_lock(&link_data->data_mutex);
	if (link_data->private_link_data.port_num >= link_data->config->port_map) {
		ERRNO_SET(PORT_MAP_FULL);
		pthread_mutex_unlock(&link_data->data_mutex);
		return ERR;
	}	
	port_map = link_data->private_link_data.port_num;
	link_data->private_link_data.port_num++;
	pthread_mutex_unlock(&link_data->data_mutex);
	ret = create_handle_node_add(link_data->app_proto, 
					link_data->config->transfer_proto, 
					port_map, 
					link_data->link_data.local_addr, 
					server, 
					link_data->link_data.local_port + port_map, 
					server_port, 
					link_data->config,
					oper);
	return ret;
}

static int
create_same_link(
	struct in_addr ip,
	struct in_addr server_ip,
	unsigned short server_port,
	struct create_link_data *link_data
)
{
	int ret = 0;
	int sock = 0;
	int event = 0;
	char *path = NULL;
	struct create_data create_data;

/*	PRINT("ip = %s, server_ip = %s, port = %d, server_port = %d\n", 
			inet_ntoa(ip), inet_ntoa(server_ip), 
			link_data->link_data.local_port, server_port);*/
	log_write((unsigned long)link_data->data, 
		     1, 
		     LOG_INFO, 
		     "ip = %s, server_ip = %s, port= %d, server_port = %d\n",
		     inet_ntoa(ip), inet_ntoa(server_ip), 
		     link_data->link_data.local_port, server_port);
	memset(&create_data, 0, sizeof(create_data));

	create_data.addr = link_data->link_data.local_addr;
	create_data.port = link_data->link_data.local_port;
	create_data.user_data = link_data->user_data;
	create_data.server_ip = server_ip;
	create_data.server_port = server_port;

	path = link_data->config->unix_path;	
	link_data->private_link_data.sock = -1;
	ret = create_socket(
			link_data->config->proto, 
			path,
			create_data.addr, 
			create_data.port,
			&link_data->config->option,
			&link_data->private_link_data.sock);
	if (ret != OK) 
		goto out;

	if (!link_data->proto_oper || !link_data->proto_oper->proto_connect)
		goto out;
	
	ret = link_data->proto_oper->proto_connect(link_data);
	if (ret != OK)
		goto out;

out:	
	return ret;
}

int
create_link_recreate(
	int flag,
	int close_link,
	char *server_path,
	unsigned int   server_ip,
	unsigned short server_port,
	unsigned long user_data,
	struct create_link_oper *oper
)
{
	int ret = 0;
	int port_map = 0;
	struct in_addr ip;
	struct in_addr server;
	struct create_link_data *link_data = NULL;

	if (!user_data || (flag == 1 && close_link == 2)){
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	server.s_addr = server_ip;
	link_data = create_link_data_get(user_data);
	if (close_link == 1) 
		create_hash_destroy(&link_data->node);
	else {
		block_fd_set(link_data->private_link_data.sock);
		epoll_data_del(link_data->private_link_data.sock);
		close(link_data->private_link_data.sock);
	}

	if (server.s_addr == 0) 
		server.s_addr = link_data->config->server_ip;
	if (server_port == 0)
		server_port = link_data->config->server_port;
	ip = link_data->link_data.local_addr;
	if(flag == 0) {
		pthread_mutex_lock(&link_data->data_mutex);
		port_map = link_data->private_link_data.port_num;
		link_data->private_link_data.port_num++;
		pthread_mutex_unlock(&link_data->data_mutex); 

		ret = create_handle_node_add(link_data->app_proto,
						link_data->config->transfer_proto,
						port_map,
						ip,
						server,
						link_data->link_data.local_port + port_map,
						server_port,
						link_data->config,
						oper 
					);
		return ret;
	}
	ret = create_same_link(ip, server, server_port, link_data);
	if (ret != OK)
		goto out;

	pthread_mutex_lock(&link_data->data_mutex);
	link_data->link_data.peer_addr = server;
	link_data->link_data.peer_port = server_port;
	pthread_mutex_unlock(&link_data->data_mutex);
out:
	return ret;
}

static void
netmask_num_get(
	char *netmask,
	int  *netmask_num
)
{
	unsigned int ip = inet_addr(netmask);
	unsigned int cnt = 0;

	while(cnt < 32)
	{
		if( ip & 1 << cnt)
		{
			(*netmask_num)++;
		}
		cnt++;
	}
}

static void
link_netcard_config(
	struct create_config *create_config
)
{
	int i = 0;
	int netmask_num = 0;
	char cmd[128] = { 0 };
	unsigned int ip = create_config->start_ip;
	struct in_addr addr;

	if (!create_config->netmask[0])	
		memcpy(create_config->netmask, "255.255.255.0", 13);

	netmask_num_get(create_config->netmask, &netmask_num);
	for (i = 0; i < create_config->ip_count; i++) {
		if (i != 0)
			ip += 1 << 24;
		addr.s_addr = ip;
		memset(cmd, 0, 128);
		sprintf(cmd, "ip addr add %s/%d dev %s", 
				inet_ntoa(addr), netmask_num, create_config->netcard);
		system(cmd);
	}
}

static int
create_link_epoll_err(
	int reason, 
	unsigned long data
)
{
	int ret = 0;
	struct create_link_data *cl_data = NULL;

	//PRINT("epoll_error :%s\n", strerror(errno));
	cl_data = (struct create_link_data *)data;
	log_write((unsigned long)cl_data->data, 1, LOG_DEBUG, "epoll_err: %s\n", strerror(errno));
	if (cl_data->epoll_oper->err_handle)
		ret = cl_data->epoll_oper->err_handle(
						EPOLL_ERR, 
						cl_data->user_data);
	cl_data->private_link_data.err_flag = ret;

	create_link_err_handle(cl_data);
	
	return OK;
}

static void
link_epoll_set(
	int duration
)
{
	struct epoll_oper oper;

	memset(&oper, 0, sizeof(oper));
	oper.epoll_recv = recv_node_add;
	oper.epoll_send = send_node_add;
	oper.epoll_err  = create_link_epoll_err;

	epoll_config_set(duration, &oper);
}

static void
link_environment_set(
	struct create_config *create_config
)
{
	link_netcard_config(create_config);
}

static int
create_hash(
	struct hlist_node	*hnode,
	unsigned long		user_data
)
{
	int hash_data = 0;
	struct create_hash_data *hdata = NULL;
	struct create_link_hash_node *cl_hash = NULL;
	struct create_link_data *data = NULL;

	hdata = (struct create_hash_data *)user_data;
	if (!hdata || !hdata->cl_hash)
		return PARAM_ERROR;
	if (!hnode) 
		hash_data = (int)hdata->hash_data;
	else {
		data = list_entry(hnode, struct create_link_data, node);
		hash_data = data->private_link_data.link_id;
	}

	return (hash_data % hdata->cl_hash->create_hash_num);
}

static int
create_hash_get(
	struct hlist_node	*hnode,
	unsigned long		user_data
)
{
	struct create_link_data *data = NULL;	

	data = list_entry(hnode, struct create_link_data, node);
	if (data->private_link_data.link_id == (int)user_data)
		return OK;

	return ERR;
}

static void
create_send_list_free(
	struct list_head *head
)
{
	struct list_head *sl = head->next;
	struct io_data *io_data = NULL;

	while (sl != head) {
		io_data = list_entry(sl, struct io_data, node);
		list_del_init(sl);
		if (io_data->data && io_data->data_len != 0)
			FREE(io_data->data);
		FREE(io_data);
		sl = head->next;
	}
}

static int
create_hash_destroy(
	struct hlist_node	*hnode
)
{
	struct create_link_data *data = NULL;

	data = list_entry(hnode, struct create_link_data, node);

	block_fd_set(data->private_link_data.sock);
	epoll_data_del(data->private_link_data.sock);
	close(data->private_link_data.sock);

	if (data->timeout_data.timeout_hash != HASH_ERR)
		hash_destroy(data->timeout_data.timeout_hash);

	pthread_mutex_destroy(&data->data_mutex);

	if (data->epoll_oper->data_destroy)
		data->epoll_oper->data_destroy(data->user_data);

	if (data->private_link_data.recv_data)
		FREE(data->private_link_data.recv_data);
	pthread_mutex_lock(&data->private_link_data.send_mutex);
	create_send_list_free(&data->private_link_data.send_list);
	pthread_mutex_unlock(&data->private_link_data.send_mutex);

	if (data->pm)
		param_destroy(data->pm);

	FREE(data);

	return OK;
}

static void
create_link_private_data_destroy(
	struct link_private_data *data	
)
{
	pthread_mutex_destroy(&data->mutex);
	pthread_mutex_destroy(&data->send_mutex);
	FREE(data->recv_data);
}

int
create_link_data_destroy(
	struct create_link_data  *data
)
{
	int ret = 0;
	struct create_hash_data hdata;
	struct create_link_hash_node *cl_hash = NULL;

	cl_hash = (struct create_link_hash_node*)data->config->create_link_data;
	memset(&hdata, 0, sizeof(hdata));
	hdata.hash_data = data->private_link_data.link_id;
	hdata.cl_hash = cl_hash;
	ret = hash_del(
			(hash_handle_t)data->config->create_link_data,
			&data->node,
			(unsigned long)&hdata);

	hash_hub_link_del(data->hub_data);
	timer_list_del(data->timer_data);

	create_hash_destroy(&data->node); 

	create_link_private_data_destroy(&data->private_link_data);

	pthread_cond_destroy(&data->interface_cond);
	pthread_mutex_destroy(&data->interface_mutex);
	pthread_mutex_destroy(&data->data_mutex);
	pthread_mutex_destroy(&data->recv_mutex);

	FREE(data->link_data.unix_path);
	return OK;
}

int
create_link_data_del(
	struct create_link_data *cl_data
)
{
	int ret = 0;

	pthread_mutex_lock(&cl_data->private_link_data.mutex);
	if (cl_data->private_link_data.status == STATUS_DISCONNECT) {
		pthread_mutex_unlock(&cl_data->private_link_data.mutex);
		return OK;
	}
	cl_data->private_link_data.status = STATUS_DISCONNECT;
	pthread_mutex_unlock(&cl_data->private_link_data.mutex);

	epoll_data_del(cl_data->private_link_data.sock);
	block_fd_set(cl_data->private_link_data.sock);
	close(cl_data->private_link_data.sock);
	hash_hub_link_del(cl_data->hub_data);
	timer_list_del(cl_data->timer_data);
}

int
create_link_data_traversal(
	unsigned long user_data,
	int (*traversal_func)(unsigned long user_data, struct hlist_node *hnode, int *flag)
)
{
	//HASH_WALK(global_create_link.create_hash, user_data, traversal_func);
	return OK;
}

int
create_link_data_traversal_del(
	unsigned long user_data,
	int (*traversal_func)(unsigned long user_data, struct hlist_node *node, int *flag)
)
{
	//HASH_WALK_DEL(global_create_link.create_hash, user_data, traversal_func);
	return OK;
}

void
create_link_create_data_destroy(
	struct create_data *cdata
)
{
	FREE(cdata->proto_name);
	FREE(cdata);
}

static void
create_handle_node_destroy(
	struct list_head *list_node
)
{
	struct create_handle_node *ch_node = NULL;

	ch_node = list_entry(list_node, struct create_handle_node, node);
	create_link_create_data_destroy(ch_node->create_data);
//	FREE(ch_node->app_proto);
	FREE(ch_node);
}

static struct create_link_hash_node *
create_link_hash_calloc(
	char *app_proto,
	int hash_num,
	struct create_link_oper *oper
)
{
	struct create_link_hash_node *node = NULL;

	node = (struct create_link_hash_node*)calloc(1, sizeof(*node));
	if (!node)
		PANIC("Not enough memory for %d bytes\n", sizeof(*node));

	node->proto = strdup(app_proto);
	if (!node->proto) 
		PANIC("Not enough memory for %d bytes\n", strlen(app_proto));
	memcpy(&node->oper, oper, sizeof(*oper));
	node->create_hash_num = hash_num;
	node->create_hash = hash_create(
					hash_num,
					create_hash,
					create_hash_get,
					create_hash_destroy);
	if (node->create_hash == HASH_ERR)  {
		create_link_hash_node_destroy(node);
		node = NULL;
		return node;
	}

	node->log_data = log_create(app_proto);

	return node;
}

static int
link_create(
	char *proto,
	int user_data_size,
	struct create_config *conf,
	struct create_link_oper *oper
)
{
	int ret = 0;
	int hash_num = 0;
	struct create_link_hash_node *hash_node = NULL;

	link_environment_set(conf);

	if (conf->total_link > CREATE_BASE)
		conf->create_hash_num = 
				conf->total_link / CREATE_BASE + 1;
	else
		conf->create_hash_num = 1;

	hash_node = create_link_hash_calloc(proto, conf->create_hash_num, oper);
	if (!hash_node)
		return NOT_ENOUGH_MEMORY;
	conf->create_link_data = (unsigned long)hash_node;

	return hash_add(global_create_link.create_link_hash, 
			   &hash_node->node, 
			   (unsigned long)proto);
}

static int
create_uninit()
{
	int ret = 0;

	hash_destroy(global_create_link.create_link_hash);
	recv_check_destroy(global_create_link.recv_check);
	FREE(global_create_link.recv_check);
	pthread_mutex_destroy(&global_create_link.count_mutex);

	return OK;
}

struct create_data *
create_data_calloc(
	char		*proto_name,
	int		transfer_flag,
	int		port_map_cnt,
	struct in_addr  ip,
	struct in_addr  server_ip,
	unsigned short  server_port,
	unsigned short	port,
	unsigned long   user_data
)
{
	struct create_data *data = NULL;

	data = (struct create_data*)calloc(1, sizeof(*data));
	if (!data) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return NULL;
	}
	data->port_num = port_map_cnt;
	if (proto_name)
		data->proto_name = strdup(proto_name);
	data->transfer_flag = transfer_flag;
	data->port = port;
	data->user_data = user_data;
	data->addr.s_addr = ip.s_addr;
	data->server_ip = server_ip;
	data->server_port = server_port;
	data->oper = &global_create_link.oper;
	pthread_mutex_lock(&global_create_link.count_mutex);
	data->link_id = global_create_link.create_link_count++;
	pthread_mutex_unlock(&global_create_link.count_mutex);

	return data;
}

static int
create_data_add(
	char *proto_name,
	int transfer_flag,
	int port_map_cnt,
	struct in_addr ip,
	struct in_addr server_ip,
	unsigned short server_port,
	unsigned short port,
	struct create_link_oper *oper
)
{
	unsigned long user_data;
	struct in_addr server_addr;
	struct create_data *data = NULL;

	server_addr.s_addr = global_create_link.config.server_ip;
	data = create_data_calloc(
				proto_name,
				transfer_flag, 
				port_map_cnt,
				ip, 
				server_ip,
				server_port,
				port, 
				user_data);
	if (!data) 
		return ERR;
	if (oper)
		data->oper = oper;

	//create_user_data_get(data->link_id, &data->user_data);

	thread_pool_node_add(global_create_link.create_id, &data->node);

	return OK;
}

static struct create_config*
create_config_get(
	char *app_proto
)
{
	struct list_head *sl = NULL;
	struct create_config *conf = NULL;

	sl = global_create_link.config_list.next;
	while (sl != &global_create_link.config_list) {
		conf = list_entry(sl, struct create_config, node);
		if (!strcmp(conf->app_proto, app_proto))	
			return conf;
	}

	return NULL;
}

static void
create_config_netcard(
	struct create_config *config
)
{
	int i = 0;	

	for (; i < config->ip_count; i++) {

	}
}

static struct create_config *
create_config_create(
	char *app_proto
)
{
	char *tmp = NULL;
	cJSON *obj = NULL;
	struct create_config *config = NULL;

	config = (struct create_config*)calloc(1, sizeof(*config));

	create_config_default_set(config);
	obj = config_read_get(app_proto);
	if (!obj)
		return config;

	CR_IP(obj, "start_ip", config->start_ip);
	CR_IP(obj, "server_ip", config->server_ip);
	CR_IP(obj, "multi_ip", config->multi_ip);
	CR_INT(obj, "open_hub", config->hub_enable);
	CR_INT(obj, "total_link", config->total_link);
	CR_INT(obj, "port_map", config->port_map);
	CR_INT(obj, "ip_count", config->ip_count);
	CR_INT(obj, "recv_timeout", config->recv_timeout);
	CR_INT(obj, "connect_timeout", config->connect_timeout);
	CR_INT(obj, "add_check", config->add_check);
	CR_INT(obj, "hub_interval", config->hub_interval);
	CR_INT(obj, "open_log", config->open_log);
	CR_STR(obj, "proto", config->proto_str);
	CR_STR(obj, "netcard", config->netcard);
	CR_STR(obj, "netmask", config->netmask);
	CR_DEV(obj, "device", config->link_type);
	CR_SIZE(obj, "recv_buf", config->option.recv_buf);
	CR_SIZE(obj, "send_buf", config->option.send_buf);
	CR_USHORT(obj, "server_port", config->server_port);
	CR_USHORT(obj, "start_port", config->start_port);
	CR_USHORT(obj, "end_port", config->end_port);
	CR_USHORT(obj, "multi_port", config->multi_port);
	CR_DURATION(obj, "duration", config->duration);
	CR_STR(obj, "log_file", config->log_path);

	if (!strcmp(config->proto_str, "tcp_client")) 
		config->proto = PROTO_TCP;
	else if (!strcmp(config->proto_str, "udp_client"))
		config->proto = PROTO_UDP;
	else if (!strcmp(config->proto_str, "unix_tcp_client"))
		config->proto = PROTO_UNIX_TCP;
	else 
		config->proto = PROTO_UNIX_UDP;
	link_netcard_config(config);

	pthread_mutex_lock(&global_create_link.list_mutex);
	list_add_tail(&config->node, &global_create_link.config_list);
	pthread_mutex_unlock(&global_create_link.list_mutex);

	return config;
}

int
create_handle_node_add(
	char *app_proto,
	char *proto_name,
	int  port_map_cnt,
	struct in_addr ip,
	struct in_addr server_ip,
	unsigned short port,
	unsigned short server_port,
	struct create_config *config,
	struct create_link_oper *oper
)
{
	struct create_data *data;
	struct create_handle_node *ch_node = NULL;
	
	server_ip.s_addr = config->server_ip;
	data = create_data_calloc(
				proto_name,
				0,
				port_map_cnt,
				ip, 
				server_ip,
				server_port,
				port, 
				0);

	ch_node = create_handle_node_create(app_proto, data, config, oper);

	return thread_pool_node_add(global_create_link.create_id, &ch_node->node);
}

static int
create_link(
	struct list_head *list_node
)
{
	int i = 0, j = 0;
	int ret = 0, count = 0;
	int total_link = 0;
	int port_offset = 0;
	unsigned short end_port = 0;
	struct in_addr ip, server_ip;
	struct create_link_node *lnode = NULL;
	struct create_config *config = NULL;

	lnode = list_entry(list_node, struct create_link_node, node);
	config = lnode->config;
	config->user_data_size = lnode->data_size;
	port_offset = config->port_map;
	count = config->end_port - config->start_port;
	end_port = (count - count % port_offset) + config->start_port;
	count = 0;
	if (config->link_type == DEV_SERVER)
		total_link = 1;
	else 
		total_link = config->total_link;
	ret = multi_proto_add(lnode->data_size, 
				 config->total_link * config->port_map, 
				 lnode->proto);
	if (ret != OK)
		PANIC("multi_proto_err\n");
		
	ip.s_addr = config->start_ip;
	server_ip.s_addr = config->server_ip;
	GINFO("ip = %s, %x, server_ip =%s\n", inet_ntoa(ip), config->start_ip, inet_ntoa(server_ip));
	for (i = 0; i < config->ip_count; i++) {
		if (i != 0)
			ip.s_addr += 1 << 24;
		for (j = config->start_port; j < end_port; j++) {
			ret = thread_test_exit();
			if (ret == OK) {
				i = config->ip_count;
				j = config->end_port;
				continue;
			}
			if (count >= total_link) {
				i = config->ip_count;
				break;
			}
			count++;
			ret = create_handle_node_add(
					lnode->proto,
					config->proto_str,
					0,
					ip,
					server_ip,
					j,
					config->server_port,
					config,
					&lnode->oper);
			if (ret != OK) {
				i = config->ip_count;
				j = config->end_port;
				continue;
			}
			if (lnode->oper.create_flow_ctrl)
				lnode->oper.create_flow_ctrl(count);
		}
	}
	return OK;
}

static int
create_link_global_init()
{
	int ret = 0;
	int num = THREAD_DEFAULT_NUM, 
	    size = THREAD_DEFALUT_STACK, 
	    duration = 0, 
	    timeout = 0;
	cJSON *obj = NULL, *node = NULL;

	if (global_create_link.create_id > 0)
		return OK;

	pthread_mutex_init(&global_create_link.count_mutex, NULL);
	obj = config_read_get("general");
	CR_INT(obj, "link_thread_num", num);
	CR_SIZE(obj, "stack_size", size);
	CR_DURATION(obj, "duration", duration);
	if (num <= 1) 
		ret = thread_pool_create(
			1, 
			size, 
			"create_link2", 
			create_handle_node_destroy,
			link_create_handle, 
			NULL,
			&global_create_link.create_id);
	else
		ret = thread_pool_create(
			num, 
			size, 
			"create_link2", 
			create_handle_node_destroy,
			NULL, 
			link_create_handle,
			&global_create_link.create_id);
	if (ret != OK)
		return ret;

	link_epoll_set(duration);
	
	return OK;
}

int 
link_create_start(
	char *proto,
	int user_data_size,
	struct create_link_oper *oper
)
{
	int ret = 0;
	struct in_addr addr;
	struct create_config *config = NULL;
	struct create_link_node *lnode = NULL;
	struct create_data *thread_data = NULL;

	create_link_global_init();

	config = create_config_create(proto);

	if (config->add_check)
		global_create_link.recv_check = recv_check_create(config->recv_timeout);

	if ((ret = link_create(proto, user_data_size, config, oper)) != OK)
		return ret;
	hash_hub_create(proto, config);

	lnode = (struct create_link_node *)calloc(1, sizeof(*lnode));
	lnode->config = config;
	lnode->proto = strdup(proto);
	lnode->data_size = user_data_size;
	memcpy(&lnode->oper, oper, sizeof(lnode->oper));

	return thread_pool_node_add(global_create_link.create_link_id, &lnode->node);
} 

static void
create_config_default_set(
	struct create_config *config
)
{
	config->create_link_num = THREAD_DEFAULT_NUM;
	config->link_num = THREAD_DEFAULT_NUM;
	config->stack_size = THREAD_DEFALUT_STACK;
	config->total_link = 1;
	config->port_map = 1;
	config->end_port = 1;
	config->ip_count = 1;
}

static int
create_config_setup()
{
	struct create_config *config = NULL;
	config = &global_create_link.config;
	
	create_config_default_set(config);

	/*CONFIG_ADD("linger", &config->option.linger, FUNC_NAME(INT));
	CONFIG_ADD("total_link", &config->total_link, FUNC_NAME(INT));
	CONFIG_ADD("port_map", &config->port_map, FUNC_NAME(INT));
	CONFIG_ADD("ip_count", &config->ip_count, FUNC_NAME(INT));
	CONFIG_ADD("start_ip", &config->start_ip, FUNC_NAME(IP));
	CONFIG_ADD("server_ip", &config->server_ip, FUNC_NAME(IP));
	CONFIG_ADD("server_port", &config->server_port, FUNC_NAME(USHORT));
	CONFIG_ADD("end_port", &config->end_port, FUNC_NAME(USHORT));
	CONFIG_ADD("start_port", &config->start_port, FUNC_NAME(USHORT));
	CONFIG_ADD("duration", &config->duration, FUNC_NAME(DURATION));
	CONFIG_ADD("recv_timeout", &config->recv_timeout, FUNC_NAME(INT));
	CONFIG_ADD("connect_timeout", &config->connect_timeout, FUNC_NAME(INT));
	CONFIG_ADD("add_check", &config->add_check, FUNC_NAME(INT));
	CONFIG_ADD("netcard", &config->netcard, FUNC_NAME(STR));
	CONFIG_ADD("device", &config->link_type, FUNC_NAME(DEV));
	CONFIG_ADD("stack_size", &config->stack_size, FUNC_NAME(INT));
	CONFIG_ADD("link_thread_num", &config->link_num, FUNC_NAME(INT));
	CONFIG_ADD("rendevous_enable", &config->rendevous_enable, FUNC_NAME(INT));
	CONFIG_ADD("hub_interval", &config->hub_interval, FUNC_NAME(INT));
	CONFIG_ADD("recv_buf", &config->option.recv_buf, FUNC_NAME(SIZE));
	CONFIG_ADD("send_buf", &config->option.send_buf, FUNC_NAME(SIZE));
	CONFIG_ADD(
		"trans_server_ip", 
		&config->transfer_server.addr, 
		FUNC_NAME(IP));
	CONFIG_ADD(
		"trans_server_port", 
		&config->transfer_server.port, 
		FUNC_NAME(USHORT));
	CONFIG_ADD(
		"trans_server_porto",
		&config->transfer_server.proto, 
		FUNC_NAME(PROTO));
	CONFIG_ADD(
		"trans_server_path", 
		config->transfer_server.unix_path, 
		FUNC_NAME(STR));
	CONFIG_ADD(
		"trans_client_path", 
		config->transfer_server.unix_path, 
		FUNC_NAME(STR));
	CONFIG_ADD(
		"trans_client_ip", 
		&config->transfer_client.addr,
		FUNC_NAME(IP));
	CONFIG_ADD(
		"trans_client_proto", 
		&config->transfer_client.proto,
		FUNC_NAME(PROTO));
	CONFIG_ADD(
		"trans_client_port",
		&config->transfer_client.port,
		FUNC_NAME(USHORT));
*/
	return OK;
}

static int
create_setup()
{
	int ret = 0;	
	int cl_thread = 0;
	cJSON *obj = NULL, *node = NULL;

	obj = config_read_get("general");
	if (obj && 
	    (node=cJSON_GetObjectItem(obj, "create_link_thread")) && 
	    node->valuestring)
		cl_thread = atoi(node->valuestring);

	ret = thread_pool_create(
				cl_thread, 
				32 * 1024, 
				"create_link", 
				NULL,
				create_link,
				NULL,
				&global_create_link.create_link_id);
	if (ret != OK)
		return ret;

	return OK;
}

static int
create_link_hash(
	struct hlist_node *hnode,
	unsigned long user_data
)
{
	int proto = 0;
	struct create_link_hash_node *pnode = NULL;

	if (!hnode)
		proto = ((char*)user_data)[0];
	else {
		pnode = list_entry(hnode, struct create_link_hash_node, node);
		if (pnode->proto)
			proto = pnode->proto[0];
	}

	return (proto % CREATE_LINK_HASH_NUM);
}

static int
create_link_hash_get(
	struct hlist_node *hnode,
	unsigned long user_data
)
{
	struct create_link_hash_node *pnode = NULL;

	pnode = list_entry(hnode, struct create_link_hash_node, node);
	if (!strcmp((char*)user_data, pnode->proto))
		return OK;

	return ERR;
}

static void
create_link_hash_node_destroy(
	struct create_link_hash_node *pnode
)
{
	FREE(pnode->proto);
	hash_destroy(pnode->create_hash);
}

static int
create_link_hash_destroy(
	struct hlist_node *hnode
)
{
	struct create_link_hash_node *pnode = NULL;

	pnode = list_entry(hnode, struct create_link_hash_node, node);
	create_link_hash_node_destroy(pnode);

	return OK;
}

int create_init() 
{ 
	int ret = 0; 
	
	memset(&global_create_link, 0, sizeof(global_create_link));
	INIT_LIST_HEAD(&global_create_link.config_list);

	global_create_link.create_link_hash = hash_create(
							CREATE_LINK_HASH_NUM, 
							create_link_hash, 
							create_link_hash_get,
							create_link_hash_destroy);
	if (global_create_link.create_link_hash == HASH_ERR)
		PANIC("Can not create hash\n");

	/*ret = user_cmd_add(create_config_setup);
	if (ret != OK)
		return ret;*/

	ret = local_init_register(create_setup); 
	if (ret != OK)
		return ret; 
	return uninit_register(create_uninit);
}

MOD_INIT(create_init);

