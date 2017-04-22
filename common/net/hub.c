#include "hub_private.h"
#include "std_comm.h"
#include "init.h"
#include "cmd.h"
#include "err.h"
#include "hash.h"
#include "print.h"
#include "config.h"
#include "timer.h"
#include "heap_timer.h"
#include "timer_list.h"
#include "thread.h"
#include "create_private.h"

#if 0
#define HUB_TABLE_SIZE 150000
struct hub_table {
	int status;
	int next_pos;
	int send_times;
	int expire_time;
	struct list_head hub_head;
};

struct hub_list_data {
	int expire_time;
	struct list_head head;
	struct list_head node;
};

struct hub_node {
	int time_offset; 
	int expire_time;
	unsigned long extra_data;
	pthread_mutex_t extra_data_mutex;
	struct list_head node;
};

struct hub_config {
	int open_hub;
	int thread_num;
	int thread_stack;
	int hub_interval;
};

struct hub_data {
	int thread_id;
	unsigned long timer_id;
	int max_interval;
	int min_interval;
	int hub_interval;
	time_t start_time;
	hash_handle_t hub_hash;
	pthread_mutex_t hub_data_mutex;
	pthread_mutex_t hub_table_mutex[HUB_TABLE_SIZE];
	struct hub_table hub_table[HUB_TABLE_SIZE];
};

enum {
	HUB_STATUS_IDLE,
	HUB_STATUS_RUNNINIG,
	HUB_STATUS_MAX
};

static struct hub_data *global_hub_data = NULL;
static struct hub_config global_hub_config;

static void
hub_node_add(
	int interval, 
	int expire_time,
	struct hub_node *hub_node
);

static void
hub_node_del(
	int expire_time,
	struct hub_node *hub_node
)
{
	int prev = 0;
	int old_pos = hub_node->expire_time;

	pthread_mutex_lock(&global_hub_data->hub_table_mutex[old_pos]);
	list_del_init(&hub_node->node);	
	pthread_mutex_unlock(&global_hub_data->hub_table_mutex[old_pos]);
}

int
hub_time_update(
	int expire_time,
	unsigned long extra_data
)
{
	int save_time = 0;
	int old_pos = 0, cur_pos = 0;
	struct create_link_data *cl_data = NULL;
	struct hub_node *hub_node = NULL;

	cl_data = (struct create_link_data*)extra_data;

	hub_node = (struct hub_node *)cl_data->hub_data;
	old_pos = hub_node->expire_time;
	if (hub_node->expire_time - hub_node->time_offset == expire_time)
		return OK;

	cur_pos = expire_time + hub_node->time_offset;
	pthread_mutex_lock(&cl_data->private_link_data.mutex);
	save_time = cl_data->private_link_data.hub_interval;
	cl_data->private_link_data.hub_interval = expire_time;
	pthread_mutex_unlock(&cl_data->private_link_data.mutex);



	hub_node_add(cur_pos, expire_time, hub_node);

	return OK;
}

void
hub_link_del(
	unsigned long hub_data
)
{
	struct hub_node *hub_node = NULL;

	if (!hub_data)
		return;

	hub_node = (struct hub_node *)hub_data;
	pthread_mutex_lock(&hub_node->extra_data_mutex);
	hub_node->extra_data = 0;
	pthread_mutex_unlock(&hub_node->extra_data_mutex);
}

static void
hub_node_add(
	int interval, 
	int expire_time,
	struct hub_node *hub_node
)
{
	int prev = interval - expire_time;

	//PRINT("interval = %d, prev = %d, expire_time = %d\n", interval, prev, expire_time);
	pthread_mutex_lock(&global_hub_data->hub_table_mutex[interval]);
	global_hub_data->hub_table[interval].expire_time = expire_time;
	if (prev >= expire_time) {
		while (prev >= expire_time && 
				global_hub_data->hub_table[prev].expire_time == 0) 
			prev -= expire_time;
		if (prev >= expire_time)
			global_hub_data->hub_table[prev].next_pos = interval;
		else {
			prev += expire_time;
			global_hub_data->hub_table[prev].next_pos = interval;
		}
	}
	list_add_tail(&hub_node->node, &global_hub_data->hub_table[interval].hub_head);
	pthread_mutex_unlock(&global_hub_data->hub_table_mutex[interval]);
}

int
hub_add(
	unsigned long user_data
)
{
	int offset = 0;
	int interval = 0, expire_time = 0;
	time_t cur_time = time(NULL);
	struct hub_node *hub_node = NULL;
	struct create_link_data *cl_data = NULL;	

	if (!user_data) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	cl_data = create_link_data_get(user_data);
	//cl_data = (struct create_link_data *)user_data;
	pthread_mutex_lock(&global_hub_data->hub_data_mutex);
	if (global_hub_data->start_time == 0) 
		global_hub_data->start_time = cur_time;
	else 
		offset = cur_time - global_hub_data->start_time;
	pthread_mutex_lock(&cl_data->private_link_data.mutex);
	expire_time = cl_data->private_link_data.hub_interval;
	pthread_mutex_unlock(&cl_data->private_link_data.mutex);
	interval = expire_time + offset;
	if (global_hub_data->min_interval > interval)
		global_hub_data->min_interval = interval;
	if (global_hub_data->max_interval <= interval)
		global_hub_data->max_interval = interval + 1;
	pthread_mutex_unlock(&global_hub_data->hub_data_mutex);
	
	hub_node = (struct hub_node *)calloc(1, sizeof(*hub_node));
	if (!hub_node)
		PANIC("not enough memory: %s\n", strerror(errno));
	hub_node->extra_data = (unsigned long)cl_data;
//	hub_node->time_offset = offset;
//	hub_node->expire_time = interval;
	cl_data->hub_data = (unsigned long)hub_node;
	pthread_mutex_init(&hub_node->extra_data_mutex, NULL);

	hub_node_add(interval, expire_time, hub_node);

	return OK;
}

static int
hub_send_list(
	struct list_head *list_node
)
{
	int ret = 0, pos = 0;;
	struct list_head *sl = NULL;
	struct hub_node *hub_node = NULL;
	struct hub_list_data *list_data = NULL;
	struct create_link_data *cl_data = NULL;
	struct sockaddr_in addr;

	list_data = list_entry(list_node, struct hub_list_data, node);
	sl = list_data->head.next;
	pos = list_data->expire_time;

	PRINT("\n");
	memset(&addr, 0, sizeof(addr));
	while (sl != &list_data->head) {
		hub_node = list_entry(sl, struct hub_node, node);
		pthread_mutex_lock(&hub_node->extra_data_mutex);
		cl_data = (struct create_link_data *)hub_node->extra_data;
		PRINT("\n");
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
			ret = cl_data->epoll_oper->harbor_func(
						cl_data->user_data
						);
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
	pthread_mutex_lock(&global_hub_data->hub_table_mutex[pos]);
	list_splice_init(&list_data->head, &global_hub_data->hub_table[pos].hub_head);
	global_hub_data->hub_table[pos].status = HUB_STATUS_IDLE;
	pthread_mutex_unlock(&global_hub_data->hub_table_mutex[pos]);

	FREE(list_data);

	return OK;
}

static int
hub_send_list_add(
	unsigned long user_data
)
{
	int cur_pos = 0;
	int cur_tick = 0;
	int next_pos = 0;
	int min_interval = 0, max_interval = 0;
	struct hub_list_data *list_data = NULL;

	cur_tick = heap_timer_tick_get();
	cur_pos = cur_tick % global_hub_data->hub_interval;
	cur_pos += global_hub_data->hub_interval;
	pthread_mutex_lock(&global_hub_data->hub_data_mutex);
	min_interval = global_hub_data->min_interval;
	max_interval = global_hub_data->max_interval;
	pthread_mutex_unlock(&global_hub_data->hub_data_mutex);

	while (1) {
		pthread_mutex_lock(&global_hub_data->hub_table_mutex[cur_pos]);
		if (global_hub_data->hub_table[cur_pos].expire_time == 0 || 
		    global_hub_data->hub_table[cur_pos].send_times + cur_pos > cur_tick || 
		    global_hub_data->hub_table[cur_pos].status == HUB_STATUS_RUNNINIG || 
		    list_empty(&global_hub_data->hub_table[cur_pos].hub_head)) {
			goto next;
		}
		global_hub_data->hub_table[cur_pos].status = HUB_STATUS_RUNNINIG;
		//global_hub_data->hub_table[cur_pos].
		list_data = (struct hub_list_data *)calloc(1, sizeof(*list_data));
		if (!list_data) 
			PANIC("not enough memory\n");
		INIT_LIST_HEAD(&list_data->head);
		list_data->expire_time = cur_pos;
		global_hub_data->hub_table[cur_pos].send_times += 
				global_hub_data->hub_table[cur_pos].expire_time;
		list_splice_init(
				&global_hub_data->hub_table[cur_pos].hub_head, 
				&list_data->head);
		thread_pool_node_add(global_hub_data->thread_id, &list_data->node);
next:
		next_pos = global_hub_data->hub_table[cur_pos].next_pos;
		pthread_mutex_unlock(&global_hub_data->hub_table_mutex[cur_pos]);
		cur_pos = next_pos;
		if (cur_pos == -1)
			break;
	}

	return OK;
}

static void
hub_data_init( 
	int hub_interval 
)
{
	int i = 0;

	global_hub_data->hub_interval = hub_interval;
	pthread_mutex_init(&global_hub_data->hub_data_mutex, NULL);

	for (; i < HUB_TABLE_SIZE; i++) {
		global_hub_data->hub_table[i].next_pos = -1;
		global_hub_data->hub_table[i].send_times = 0;
		global_hub_data->hub_table[i].expire_time = 0;
		INIT_LIST_HEAD(&global_hub_data->hub_table[i].hub_head);
		pthread_mutex_init(&global_hub_data->hub_table_mutex[i], NULL);
	}
}

static int
hub_hash(
	struct hlist_node	*hnode,
	unsigned long		user_data
)
{
	char expire = 0;
	struct hub_node *hub_node = NULL;

	if (!hnode)
		expire = user_data;
	else {
		hub_node = list_entry(hnode, struct hub_node, node);
		expire = hub_node->expire_time;
	}

	return (expire % global_hub_data->hub_interval);
}

static int
hub_hash_get(
	struct hlist_node	*hnode,
	unsigned long		user_data	
)
{
	struct hub_node *hub_node = NULL;

	if (!hnode)
		return ERR;

	hub_node = list_entry(hnode, struct hub_node, node);
	if (hub_node->expire_time == (int)user_data)
		return OK;

	return ERR;
}

static int
hub_hash_destroy(
	struct hlist_node *hnode
)
{
	struct hub_node *hub_node = NULL;

	if (!hnode)
		return ERR;
	hub_node = list_entry(hnode, struct hub_node, node);
}

int
hub_create()
{
	int ret = 0;

	if (global_hub_config.open_hub == 0)
		return OK;

	global_hub_data = (struct hub_data *)calloc(1, sizeof(*global_hub_data));

	if (global_hub_config.thread_num <= 1)
		ret = thread_pool_create(
					global_hub_config.thread_num,
					global_hub_config.thread_stack,
					"harbor", 
					NULL, 
					hub_send_list, 
					NULL,
					&global_hub_data->thread_id);
	else 
		ret = thread_pool_create(
					global_hub_config.thread_num,
					global_hub_config.thread_stack,
					"harbor", 
					NULL,
					NULL,
					hub_send_list,
					&global_hub_data->thread_id);
	if (ret != OK) 
		PANIC("create thread pool error: %s\n", CUR_ERRMSG_GET());

	ret = heap_timer_create(
			1, 
			HEAP_TIMER_FLAG_CONSTANT, 
			0, 
			hub_send_list_add, 
			NULL,
			&global_hub_data->timer_id);
	if (ret != OK)
		PANIC("create timer error: %s\n", CUR_ERRMSG_GET());
	PRINT("hub_timer_id = %d\n", global_hub_data->timer_id);

	hub_data_init(global_hub_config.hub_interval);
//	global_hub_data->hub_interval = hub_interval;
	return OK;
}

static void
hub_list_destroy(
	struct list_head *head	
)
{
	struct list_head *sl = NULL;
	struct hub_node *hub_node = NULL;
	struct create_link_data *cl_data = NULL;

	sl = head->next;
	while (sl != head) {
		hub_node = list_entry(sl, struct hub_node, node);
		pthread_mutex_destroy(&hub_node->extra_data_mutex);
		if (hub_node->extra_data) {
			cl_data = (struct create_link_data *)hub_node->extra_data;
			cl_data->hub_data = 0;
		}
		list_del_init(sl);
		FREE(hub_node);
		sl = head->next;
	}
}

static int
hub_destroy()
{
	int i = 0;

	pthread_mutex_destroy(&global_hub_data->hub_data_mutex);

	for (; i < HUB_TABLE_SIZE; i++) {
		hub_list_destroy(&global_hub_data->hub_table[i].hub_head);
		pthread_mutex_destroy(&global_hub_data->hub_table_mutex[i]);
	}

	heap_timer_destroy(global_hub_data->timer_id);
}

static int
hub_config_setup()
{
	global_hub_config.thread_num = THREAD_DEFAULT_NUM;
	global_hub_config.thread_stack = THREAD_DEFALUT_STACK;

	CONFIG_ADD(
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
		FUNC_NAME(INT));

	return OK;
}

int
hub_init()
{
	int ret = 0;

	memset(&global_hub_config, 0, sizeof(global_hub_config));
	ret = user_cmd_add(hub_config_setup);
	if (ret != OK)
		return ret;

	ret = local_init_register(hub_create);
	if (ret != OK)
		return ret;

	return uninit_register(hub_destroy);
}

MOD_INIT(hub_init);
#endif
