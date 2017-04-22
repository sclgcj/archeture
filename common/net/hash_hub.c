#include "hub_private.h"
#include "err.h"
#include "cmd.h"
#include "std_comm.h"
#include "init.h"
#include "hash.h"
#include "heap_timer.h"
#include "print.h"
#include "config.h"
#include "config_read.h"
#include "thread.h"
#include "timer_private.h"
#include "create_private.h"
#include "create_log.h"
#include "hash_hub.h"
#include "global_log_private.h"

#define HASH_HUB_DATA_SIZE 26
struct hash_hub_table {
	int status;
//	int next_pos;
	int send_times;
	int expire_time;
	pthread_mutex_t mutex;
	unsigned long link_data;
	struct list_head hub_head;
	struct hlist_node node;
};

struct hash_hub_node {
	int time_offset; 
	int expire_time;
	unsigned long extra_data;
	pthread_mutex_t extra_data_mutex;
	struct list_head node;
};

struct hash_hub_config {
	int open_hub;
	int thread_num;
	int thread_stack;
	int hub_interval;
};

struct hash_hub_data_node {
	char *app_proto;
	int thread_id;
	unsigned long timer_id;
	int max_interval;
	int min_interval;
	int hub_interval;
	unsigned long *timer_id_array;
	time_t start_time;
	hash_handle_t hub_hash;
	pthread_mutex_t hub_data_mutex;
	struct hlist_node node;
};

struct hash_hub_list_data {
	int expire_time;
	struct hash_hub_data_node *dnode;
	struct list_head head;
	struct list_head node;
};

struct hash_hub_param {
	int expire_time;
	struct hash_hub_data_node *dnode;
};

struct hash_hub_data {
	hash_handle_t data_hash;
};

//static struct hash_hub_data *global_hub_data = NULL;
static struct hash_hub_data global_hub_data;
static struct hash_hub_config global_hub_config;

static int
hash_hub_table_get(
	int interval,
	struct hash_hub_data_node *dnode,
	struct hash_hub_table **hub_table
);

static int
hash_hub_node_add(
	int 			interval,
	struct hash_hub_table	*hub_table,
	struct create_link_data *cl_data,
	struct hash_hub_data_node *dnode
);

static struct hash_hub_data_node*
hash_hub_data_node_get(
	char *app_proto
)
{
	struct hlist_node *hnode = NULL;
	struct hash_hub_data_node *dnode = NULL;

	hnode = hash_get(global_hub_data.data_hash, 
			    (unsigned long)app_proto, 
			    (unsigned long)app_proto);
	if (!hnode) {
		GINFO("no such protocol: %s\n", app_proto);
		return NULL;
	}

	dnode = list_entry(hnode, struct hash_hub_data_node, node);

	return dnode;
}

/* 这个函数目前是想的很美好，实际实现呢，其实很悲催 */
int
hash_hub_time_update(
	int expire_time,
	unsigned long user_data
)
{
	int ret = 0;
	int old_pos = 0, offset = 0, cur_pos = 0;
	struct hlist_node *hnode = NULL;
	struct hash_hub_param param;
	struct hash_hub_table *hub_table = NULL;
	struct create_link_data *cl_data = NULL;
	struct hash_hub_node *hub_node = NULL;
	struct hash_hub_data_node *dnode = NULL;

	cl_data = (struct create_link_data*)user_data;

	dnode = hash_hub_data_node_get(cl_data->app_proto);
	if (!dnode)
		return ERR;
	hub_node = (struct hash_hub_node *)cl_data->hub_data;
	//offset = old_pos - global_hub_data->hub_interval;
	memset(&param, 0, sizeof(param));
	param.dnode = dnode;
	param.expire_time = hub_node->expire_time;
	hub_table = HASH_GET(
				dnode->hub_hash, 
				struct hash_hub_table, 
				node, 
				(unsigned long)&param, 
				hub_node->expire_time);

	/* 从原来的表中删除 */
	pthread_mutex_lock(&hub_table->mutex);
	list_del_init(&hub_node->node);
	pthread_mutex_unlock(&hub_table->mutex);

	hub_table = NULL;
	cur_pos = expire_time - hub_node->expire_time + hub_table->expire_time;
	hub_node->expire_time = expire_time;

	ret = hash_hub_table_get(cur_pos, dnode, &hub_table);
	if (ret != OK)
		return ret;
	pthread_mutex_lock(&hub_table->mutex);
	list_add_tail(&hub_node->node, &hub_table->hub_head);
	pthread_mutex_unlock(&hub_table->mutex);

	return OK;
}

void
hash_hub_link_del(
	unsigned long hub_data
)
{
	struct hash_hub_node *hub_node = NULL;

	if (!hub_data)
		return;

	hub_node = (struct hash_hub_node *)hub_data;
	pthread_mutex_lock(&hub_node->extra_data_mutex);
	hub_node->extra_data = 0;
	pthread_mutex_unlock(&hub_node->extra_data_mutex);
}

static int
hub_hash(
	struct hlist_node	*hnode,
	unsigned long		user_data
)
{
	char expire = 0;
	struct hash_hub_param *param = NULL;
	struct hash_hub_table *hub_node = NULL;

	if (!user_data)
		return ERR;

	param = (struct hash_hub_param*)user_data;

	if (!hnode) 
		expire = param->expire_time;
	else {
		hub_node = list_entry(hnode, struct hash_hub_table, node);
		expire = hub_node->expire_time;
	}

	return (expire %  param->dnode->hub_interval);
}

static int
hash_hub_get(
	struct hlist_node	*hnode,
	unsigned long		user_data	
)
{
	struct hash_hub_table *hub_node = NULL;

	if (!hnode)
		return ERR;

	hub_node = list_entry(hnode, struct hash_hub_table, node);
	if (hub_node->expire_time == (int)user_data)
		return OK;

	return ERR;
}

static int
hash_hub_destroy(
	struct hlist_node *hnode
)
{
	struct hash_hub_table *hub_node = NULL;

	if (!hnode)
		return ERR;
	hub_node = list_entry(hnode, struct hash_hub_table, node);

	FREE(hub_node);
}

static int
hash_hub_uninit()
{
	return hash_destroy(global_hub_data.data_hash);
//	hash_destroy(global_hub_data->hub_hash);
}

static int
hash_hub_config_setup(
	char *app_proto
)
{
	cJSON *obj = NULL, *node = NULL;
	
	global_hub_config.thread_num = THREAD_DEFAULT_NUM;
	global_hub_config.thread_stack = THREAD_DEFALUT_STACK;

	obj = config_read_get(app_proto);
	CR_INT(obj, "hub_thread_number", global_hub_config.thread_num);
	node = cJSON_GetObjectItem(obj, "hub_thread_num");
	if (node && node->valuestring) 
		global_hub_config.thread_num = atoi(node->valuestring);
	node = cJSON_GetObjectItem(obj, "hub_stack_size");
	if (node && node->valuestring)
		global_hub_config.thread_stack = atoi(node->valuestring);
	node = cJSON_GetObjectItem(obj, "open_hub");
	if (node && node->valuestring)
		global_hub_config.open_hub = atoi(node->valuestring);
	
	/*CONFIG_ADD(
		"hub_thread_num", 
		&global_hub_config.thread_num, 
		FUNC_NAME(INT));
	CONFIG_ADD(
		"hub_stack_size", 
		&global_hub_config.thread_stack, 
		FUNC_NAME(INT));
	CONFIG_ADD(
		"hub_interval",
		&global_hub_config.hub_interval,
		FUNC_NAME(INT));
	CONFIG_ADD(
		"open_hub",
		&global_hub_config.open_hub, 
		FUNC_NAME(INT));*/

	return OK;
}

static int
hash_hub_send_list(
	struct list_head *list_node
)
{
	int ret = 0, pos = 0;;
	struct list_head *sl = NULL;
	struct hlist_node *hnode = NULL;
	struct hash_hub_param param;
	struct hash_hub_table *table = NULL;
	struct hash_hub_node *hub_node = NULL;
	struct hash_hub_list_data *list_data = NULL;
	struct create_link_data *cl_data = NULL;
	struct sockaddr_in addr;

	list_data = list_entry(list_node, struct hash_hub_list_data, node);
	sl = list_data->head.next;
	pos = list_data->expire_time;

	memset(&addr, 0, sizeof(addr));
	while (sl != &list_data->head) {
		hub_node = list_entry(sl, struct hash_hub_node, node);
		pthread_mutex_lock(&hub_node->extra_data_mutex);
		cl_data = (struct create_link_data *)hub_node->extra_data;
		if (!cl_data) {
			sl = sl->next;
			list_del_init(&hub_node->node);
			pthread_mutex_unlock(&hub_node->extra_data_mutex);
			pthread_mutex_destroy(&hub_node->extra_data_mutex);
			FREE(hub_node);
			continue;
		}
		if (cl_data->epoll_oper->harbor_func)  {
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = cl_data->link_data.peer_addr.s_addr;
			addr.sin_port = htons(cl_data->link_data.peer_port);
			ret = cl_data->epoll_oper->harbor_func(cl_data->user_data);
			if (ret != OK) {
				pthread_mutex_unlock(&hub_node->extra_data_mutex);
				sl = sl->next;
				list_del_init(&hub_node->node);
				create_link_data_del(cl_data);
				FREE(hub_node);
				continue;
			}
		}
		pthread_mutex_unlock(&hub_node->extra_data_mutex);
		sl = sl->next;
	}

	memset(&param, 0, sizeof(param));
	param.dnode = list_data->dnode;
	param.expire_time = pos;
	hnode = hash_get(list_data->dnode->hub_hash, 
			    (unsigned long)&param, pos);
	table = list_entry(hnode, struct hash_hub_table, node);
	pthread_mutex_lock(&table->mutex);
	list_splice_init(&list_data->head, &table->hub_head);
	pthread_mutex_unlock(&table->mutex);

	FREE(list_data);

	return OK;
}

static void
hash_hub_traversal(
	struct hlist_node	*hnode, 
	unsigned long		user_data
)
{
	struct hash_hub_table *hub_table = NULL;
	struct hash_hub_list_data *list_data = NULL;

	hub_table = list_entry(hnode, struct hash_hub_table, node);
	list_data = (struct hash_hub_list_data*)calloc(1, sizeof(*list_data));
	if (!list_data) 
		PANIC("Not Enough memory for %d\n", sizeof(*list_data));
	INIT_LIST_HEAD(&list_data->head);
	list_data->expire_time = hub_table->expire_time;
	list_data->dnode = (struct hash_hub_data_node *)user_data;
	if (!list_data->dnode) {
		FREE(list_data);
		return;
	}
	pthread_mutex_lock(&hub_table->mutex);
	list_splice_init(&hub_table->hub_head, &list_data->head);
	pthread_mutex_unlock(&hub_table->mutex);

	thread_pool_node_add(list_data->dnode->thread_id, &list_data->node);
}

static int
hash_hub_send_list_add(
	unsigned long user_data
)
{
	int cur_pos = 0;
	int next_pos = 0;
	int cur_tick = 0;
	struct hlist_node *hnode = NULL;
	struct hash_hub_param hub_param;
	struct hash_hub_data_node *dnode = NULL;

	if (!user_data)
		return ERR;
	dnode = (struct hash_hub_data_node*)user_data;
	cur_tick = heap_timer_tick_get();
	cur_pos = cur_tick % dnode->hub_interval;
	//cur_pos += dnode->hub_interval;

	memset(&hub_param, 0, sizeof(hub_param));
	hub_param.dnode = dnode;
	hub_param.expire_time = cur_pos;
	return hash_head_traversal(
				dnode->hub_hash, 
				(unsigned long)&hub_param,
				user_data, 
				hash_hub_traversal);
}

static int
hash_hub_table_add(
	int interval,
	struct hash_hub_data_node *dnode,
	struct hash_hub_table **table
)
{
	struct hash_hub_table *hub_table = NULL;
	struct hash_hub_param param;

	hub_table = (struct hash_hub_table *)calloc(1, sizeof(struct hash_hub_table));
	if (!hub_table) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}
	hub_table->expire_time = interval;
	INIT_LIST_HEAD(&hub_table->hub_head);
	pthread_mutex_init(&hub_table->mutex, NULL);
	(*table) = hub_table;

	memset(&param, 0, sizeof(param));
	param.dnode = dnode;
	param.expire_time = interval;
	return hash_add(dnode->hub_hash, &hub_table->node, (unsigned long)&param);
}

static int
hash_hub_cur_interval_get(
	int expire_time,
	struct hash_hub_data_node *dnode
)
{
	int offset = 0;
	int interval = 0;
	//int expire_time = 0;
	time_t cur_time = time(NULL);

	pthread_mutex_lock(&dnode->hub_data_mutex);
	if (dnode->start_time == 0) 
		dnode->start_time = cur_time;
	else 
		offset = cur_time - dnode->start_time;
//	expire_time = global_hub_data->hub_interval;
	interval = expire_time + offset;
	if (dnode->min_interval > interval)
		dnode->min_interval = interval;
	if (dnode->max_interval <= interval)
		dnode->max_interval = interval + 1;
	pthread_mutex_unlock(&dnode->hub_data_mutex);

	return interval;
}

static int
hash_hub_node_add(
	int 			interval,
	struct hash_hub_table	*hub_table,
	struct create_link_data *cl_data,
	struct hash_hub_data_node *dnode
)
{
	struct hash_hub_node *hub_node = NULL;

	hub_node = (struct hash_hub_node *)calloc(1, sizeof(*hub_node));
	if (!hub_node)
		PANIC("not enough memory: %s\n", strerror(errno));
	hub_node->expire_time = dnode->hub_interval;
	hub_node->extra_data = (unsigned long)cl_data;
	cl_data->hub_data = (unsigned long)hub_node;
	pthread_mutex_init(&hub_node->extra_data_mutex, NULL);

	pthread_mutex_lock(&hub_table->mutex);
	list_add_tail(&hub_node->node, &hub_table->hub_head);
	pthread_mutex_unlock(&hub_table->mutex);

	return OK;
}

static int
hash_hub_table_get(
	int interval,
	struct hash_hub_data_node *dnode,
	struct hash_hub_table **hub_table
)
{
	int ret = 0;
	struct hash_hub_param param;
	struct hlist_node *hnode = NULL;

	memset(&param, 0, sizeof(param));
	param.dnode = dnode;
	param.expire_time = interval;
	hnode = hash_get(dnode->hub_hash, 
			    (unsigned long)&param, 
			    (unsigned long)interval);
	if (hnode) 
		(*hub_table) = list_entry(hnode, struct hash_hub_table, node);
	else
		ret = hash_hub_table_add(interval, dnode, hub_table);

	return ret; 
}

int
hash_hub_add(
	unsigned long user_data
)
{
	int id = 0;
	int ret = 0;
	char *tmp_proto = NULL;
	int interval = 0, expire_time = 0;
	struct hash_hub_table *hub_table = NULL;
	struct create_link_data *cl_data = NULL;	
	struct hash_hub_data_node *dnode = NULL;

	if (!user_data) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	cl_data = list_entry(user_data, struct create_link_data, data);
	//cl_data = create_link_data_get(user_data);

	//tmp_proto = strdup(cl_data->app_proto);
	dnode = hash_hub_data_node_get(cl_data->app_proto);
	/*
	 * We consider that a hub interval should be based on a special link. 
	 * Reasons are below:
	 * 1. Sometimes, the server may change some links' hub interval for its
	 *    network problem
	 * 2. We can easily to change every link's hub interval when server 
	 *    change its hub interval value
	 * 3. In multi-protocol situation, the hub interval may be different FROM
	 *    each other
	 */
	interval = hash_hub_cur_interval_get(cl_data->config->hub_interval, dnode);

	ret = hash_hub_table_get(interval, dnode, &hub_table);
	if (ret != OK)
		return ret;
	hub_table->link_data = (unsigned long)cl_data;
	hash_hub_node_add(interval, hub_table, cl_data, dnode);

	id = interval % dnode->hub_interval;
	if (dnode->timer_id_array[id] == -1) {
		DEBUG(
			(unsigned long)cl_data->data,
			"====================> id = %d, interval = %d, standard interval = %d\n",
			id, interval, cl_data->config->hub_interval);
		return heap_timer_create( 
				cl_data->config->hub_interval, 
				HEAP_TIMER_FLAG_CONSTANT, 
				(unsigned long)dnode,
				hash_hub_send_list_add, 
				NULL,
				&dnode->timer_id_array[id]);
	}
	return OK;
}

static void
hash_hub_free(
	struct list_head *list_node
)
{
	struct list_head *sl = NULL;
	struct hash_hub_node *hub_node = NULL;
	struct hash_hub_list_data *list_data = NULL;

	list_data = list_entry(list_node, struct hash_hub_list_data, node);
	sl = list_data->head.next;

	while (sl != &list_data->head) {
		hub_node = list_entry(sl, struct hash_hub_node, node);
		list_del_init(&hub_node->node);
		pthread_mutex_destroy(&hub_node->extra_data_mutex);
		FREE(hub_node);
		sl = list_data->head.next;
	}
}

int
hash_hub_create(
	char *app_proto,
	struct create_config *conf
)
{
	int i = 0;
	int ret = 0;
	char thread_name[128] = { 0 };
	char *tmp_proto = NULL;
	struct hash_hub_data_node *dnode = NULL;

	//hash_hub_config_setup(app_proto);

	if (conf->hub_enable == 0)
		return OK;

	dnode = (struct hash_hub_data_node *)calloc(1, sizeof(*dnode));

	dnode->app_proto = strdup(app_proto);
	dnode->hub_interval = conf->hub_interval;
	pthread_mutex_init(&dnode->hub_data_mutex, NULL);
	dnode->timer_id_array = (unsigned long*)calloc(
						   conf->hub_interval, 
						   sizeof(unsigned long));
	if (!dnode->timer_id_array) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}
	for (; i < dnode->hub_interval; i++) 
		dnode->timer_id_array[i] = -1;

	sprintf(thread_name, "harbor_%s", app_proto);
	tmp_proto = strdup(app_proto);
	if (conf->hub_num <= 1)
		ret = thread_pool_create(
					1,
					conf->stack_size,
					thread_name,
					hash_hub_free,
					hash_hub_send_list, 
					NULL,
					&dnode->thread_id);
	else 
		ret = thread_pool_create(
					conf->hub_num,
					conf->stack_size,
					thread_name,
					hash_hub_free,
					NULL,
					hash_hub_send_list,
					&dnode->thread_id);
	if (ret != OK) 
		PANIC("create thread pool error: %s\n", CUR_ERRMSG_GET());

	/*ret = heap_timer_create(
			1, 
			HEAP_TIMER_FLAG_CONSTANT, 
			(unsigned long)dnode, 
			hash_hub_send_list_add, 
			NULL,
			&dnode->timer_id);
	if (ret != OK)
		PANIC("create timer error: %s\n", CUR_ERRMSG_GET());*/
	//PRINT("hub_timer_id = %d, hub_interval = %d\n", 
	//		dnode->timer_id, conf->hub_interval);

	dnode->hub_hash = hash_create(
					conf->hub_interval, 
					hub_hash, 
					hash_hub_get, 
					hash_hub_destroy);
	if (dnode->hub_hash == (void*)HASH_ERR) 
		return ERR;

	return hash_add(global_hub_data.data_hash, 
			   &dnode->node, 
			   (unsigned long)app_proto);
}

static int
hash_hub_data_hash(
	struct hlist_node *hnode,
	unsigned long user_data
)
{
	char proto = 0;
	struct hash_hub_data_node *dnode = NULL;

	if (!hnode && !user_data)
		return ERR;

	if (!hnode) 
		proto = ((char*)user_data)[0];
	else {
		dnode = list_entry(hnode, 
				      struct hash_hub_data_node, 
				      node);
		proto = dnode->app_proto[0];
	}

	return (proto % HASH_HUB_DATA_SIZE);
}

static int
hash_hub_data_hash_get(
	struct hlist_node *hnode,
	unsigned long user_data
)
{
	struct hash_hub_data_node *dnode = NULL;

	if (!user_data)
		return ERR;
	dnode = list_entry(hnode, struct hash_hub_data_node, node);
	if (!dnode->app_proto)
		return ERR;
	if (strcmp(dnode->app_proto, (char*)user_data))	
		return ERR;

	return OK;
}

static int
hash_hub_data_hash_destroy(
	struct hlist_node *hnode
)
{
	struct hash_hub_data_node *dnode = NULL;

	dnode = list_entry(hnode, struct hash_hub_data_node, node);

	FREE(dnode->app_proto);
	hash_destroy(dnode->hub_hash);
	FREE(dnode->timer_id_array);

	return OK;
}

int
hash_hub_init()
{
	int ret = 0;

	global_hub_data.data_hash = hash_create(
						HASH_HUB_DATA_SIZE, 
						hash_hub_data_hash, 
						hash_hub_data_hash_get, 
						hash_hub_data_hash_destroy);
	if (global_hub_data.data_hash == HASH_ERR)
		return ERR;

	/*ret = local_init_register(hash_hub_create);
	if (ret != OK)
		return ret;*/

	return uninit_register(hash_hub_uninit);
}

MOD_INIT(hash_hub_init);
