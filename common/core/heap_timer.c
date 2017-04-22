#include "heap_timer.h"
#include "err.h"
#include "cmd.h"
#include "hash.h"
#include "heap.h"
#include "init_private.h"
#include "print.h"
#include "thread.h"
#include "config_read.h"
#include "heap_timer_private.h"

struct heap_timer_node {
	int			id;			//在hash链表中的位置
	int			status;			//运行状态
	int			check_list_id;		//检查链表的id
	int			timer_flag;		//定时器标志，持续存在，还是只运行一次
	int			timer_sec;		//定时时长
	unsigned long		tick;			//定时滴答数
	unsigned long		user_data;		//用户数据
	unsigned long		heap_data;		//存放对应的堆结点
	int (*timer_func)(unsigned long user_data);	
	void (*timer_free)(unsigned long user_data);
	pthread_mutex_t		tick_mutex;
	struct hlist_node	node;
	struct list_head	list_node;	
	struct list_head	check_list_node;
};

struct heap_timer_check_node {
/*	int		 status;
	struct list_head head;
	struct list_head check_head;
	unsigned long	end_num;
	unsigned long	start_num;
	pthread_mutex_t mutex;
	pthread_mutex_t check_mutex;*/
	struct list_head list_node;
};

struct heap_timer_count_data {
	unsigned long count;
	pthread_mutex_t mutex;
};

struct heap_timer_list_data {
	struct list_head head;
	pthread_mutex_t mutex;
};

struct heap_timer_list_config {
	int timer_sec;
	int timer_flag;
	int (*list_func)(unsigned long data);
};

#define HEAP_TIMER_TABLE_SIZE  86400
#define HEAP_TIMER_SPLICE	     8
struct heap_timer_data {
	int		 thread_num;
	int		 thread_stack;
	int		 check_list_cnt;
	int		 timer_count_id;
	int		 timer_check_id;
	int		 timer_handle_id;
	int		 check_status;
	unsigned long	 max_id;
	heap_handle_t timer_heap;
	hash_handle_t timer_hash;
	pthread_mutex_t  check_list_mutex;
	struct heap_timer_list_data  timer_list;
	struct heap_timer_list_data  free_id_list;
	struct heap_timer_count_data timer_tick;
	struct heap_timer_count_data timer_count;
	//struct heap_timer_check_node check_list[HEAP_TIMER_SPLICE];
};

enum {
	HEAP_TIMER_STATUS_IDLE,
	HEAP_TIMER_STATUS_RUNNING,
	HEAP_TIMER_STATUS_MAX
};

static struct heap_timer_data global_heap_timer_data;
static int heap_timer_id_node_add(int id);
static int heap_timer_id_node_get();

static int
heap_timer_config_setup();


static void
heap_timer_traversal(
	unsigned long user_data
)
{
	struct heap_timer_node *node = NULL;
	if (user_data == -1)
		return;

	node = (struct heap_timer_node *)user_data;
}

static void
heap_timer_list_add(
	struct heap_timer_node *timer_node
)
{
	int id = 0;
	
/*	pthread_mutex_lock(&global_heap_timer_data.check_list_mutex);
	id = (global_heap_timer_data.check_list_cnt % HEAP_TIMER_SPLICE);
	global_heap_timer_data.check_list_cnt++;
	timer_node->check_list_id = id;
	PRINT("id = %d\n", id);
	pthread_mutex_unlock(&global_heap_timer_data.check_list_mutex);*/
//	pthread_mutex_lock(&global_heap_timer_data.check_list[id].mutex);
//	list_add_tail(&timer_node->check_list_node, &global_heap_timer_data.check_list[id].head);
//	pthread_mutex_unlock(&global_heap_timer_data.check_list[id].mutex);
}

static void
heap_timer_list_del(
	struct heap_timer_node *timer_node
)
{
	int id = timer_node->check_list_id;

//	pthread_mutex_lock(&global_heap_timer_data.check_list[id].check_mutex);
//	list_del_init(&timer_node->check_list_node);
//	pthread_mutex_unlock(&global_heap_timer_data.check_list[id].check_mutex);
}

int
heap_timer_create(
	int timer_sec,		
	int timer_flag,
	unsigned long user_data,
	int (*timer_func)(unsigned long user_data),
	void (*timer_free)(unsigned long user_data),
	unsigned long *timer_id
)
{
	int ret = 0;
	struct timespec ts;
	struct heap_timer_node *timer_node = NULL;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	timer_node = (struct heap_timer_node *)calloc(1, sizeof(*timer_node));
	if (!timer_node) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}
	ret = heap_timer_id_node_get();
	pthread_mutex_lock(&global_heap_timer_data.timer_count.mutex);
	if (ret != ERR) {
		timer_node->id = ret;
		global_heap_timer_data.timer_count.count++;
	}
	else
		timer_node->id = global_heap_timer_data.timer_count.count++;
	pthread_mutex_unlock(&global_heap_timer_data.timer_count.mutex);
	timer_node->user_data	= user_data;
	timer_node->timer_flag  = timer_flag;
	timer_node->timer_func  = timer_func;
	timer_node->timer_free  = timer_free;
	pthread_mutex_lock(&global_heap_timer_data.timer_tick.mutex);
	timer_node->timer_sec	= timer_sec;
	timer_node->tick = timer_sec + global_heap_timer_data.timer_tick.count;
	pthread_mutex_unlock(&global_heap_timer_data.timer_tick.mutex);
	if (timer_id)
		(*timer_id) = (unsigned long)timer_node;
	pthread_mutex_init(&timer_node->tick_mutex, NULL);

	heap_node_add(global_heap_timer_data.timer_heap, (unsigned long)timer_node);
		
	//heap_timer_list_add(timer_node);

	return OK;
}

static int
heap_timer_hash(
	struct hlist_node	*hnode,
	unsigned long		user_data
)
{
	unsigned long hash_val = 0;
	struct heap_timer_node *timer_node = NULL;

	if (!hnode) 
		hash_val = user_data;
	else {
		timer_node = list_entry(hnode, struct heap_timer_node, node);
		hash_val = timer_node->id;
	}

	return (int)(hash_val % HEAP_TIMER_TABLE_SIZE);
}

static int
heap_timer_hash_get(
	struct hlist_node	*hnode,
	unsigned long		user_data
)
{
	struct heap_timer_node *timer_node = NULL;	

	timer_node = list_entry(hnode, struct heap_timer_node, node);
	if (timer_node->id == (int)user_data) 
		return OK;
	return ERR;
}

static int
heap_timer_id_node_add(
	int id
)
{
	struct heap_timer_data_node *id_node = NULL;
	id_node = (struct heap_timer_data_node *)calloc(1, sizeof(*id_node));
	if (!id_node) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}
	id_node->data = id;
	pthread_mutex_lock(&global_heap_timer_data.free_id_list.mutex);
	list_add_tail(&id_node->list_node, &global_heap_timer_data.free_id_list.head);
	pthread_mutex_unlock(&global_heap_timer_data.free_id_list.mutex);
	
	return OK;
}

static int
heap_timer_id_node_get()
{
	int ret = 0;
	struct list_head *sl = NULL;
	struct heap_timer_data_node *id_node = NULL; 

	pthread_mutex_lock(&global_heap_timer_data.free_id_list.mutex);
	if (list_empty(&global_heap_timer_data.free_id_list.head)) {
		ret = ERR;
		goto out;
	}
	sl = global_heap_timer_data.free_id_list.head.next;
	id_node = list_entry(sl, struct heap_timer_data_node, list_node);
	ret = id_node->data;
	list_del_init(sl);
	FREE(id_node);
out:
	pthread_mutex_unlock(&global_heap_timer_data.free_id_list.mutex);

	return ret;
}

static int
heap_timer_node_destroy(
	struct heap_timer_node *timer_node
)
{
	heap_timer_id_node_add(timer_node->id);

	heap_timer_list_del(timer_node);

	if (timer_node->timer_free)
		timer_node->timer_free(timer_node->user_data);
	pthread_mutex_destroy(&timer_node->tick_mutex);
	FREE(timer_node);

	return OK;
	//pthread_mutex_lock(&global_heap_timer_data.timer_count.mutex);
	//global_heap_timer_data.timer_count.count--;
	//PRINT("global_heap_timer_data.timer_count.cout = %ld\n",global_heap_timer_data.timer_count.count);
	//pthread_mutex_unlock(&global_heap_timer_data.timer_count.mutex);
}

/*static void
heap_timer_node_destroy(
	unsigned long user_data
)
{
	heap_timer_node_destroy((struct heap_timer_node*)user_data);
}*/

static int
heap_timer_hash_destroy(
	struct hlist_node	*hnode
)
{
	struct heap_timer_node *timer_node = NULL;

	timer_node = list_entry(hnode, struct heap_timer_node, node);
	heap_timer_node_destroy(timer_node);

/*	heap_timer_id_node_add(timer_node->id);

	FREE(timer_node);

	pthread_mutex_lock(&global_heap_timer_data.timer_count.mutex);
	global_heap_timer_data.timer_count.count--;
	PRINT("global_heap_timer_data.timer_count.cout = %ld\n",global_heap_timer_data.timer_count.count);
	pthread_mutex_unlock(&global_heap_timer_data.timer_count.mutex);*/
	return OK;
}

unsigned long
heap_timer_tick_get()
{
	unsigned long ret = 0;
	
	pthread_mutex_lock(&global_heap_timer_data.timer_tick.mutex);
	ret = global_heap_timer_data.timer_tick.count;
	pthread_mutex_unlock(&global_heap_timer_data.timer_tick.mutex);

	return ret;
}

static int 
heap_timer_check_node_add(
	int count
)
{
	int ret = 0;
	unsigned long tick;
	unsigned long user_data;
	struct heap_timer_node *timer_node = NULL;
	struct heap_timer_check_node *check_node = NULL;

	check_node = (struct heap_timer_check_node *)calloc(1, sizeof(*check_node));
	if (!check_node) 
		PANIC("not enough memory for %d bytes\n", sizeof(*check_node));

	thread_pool_node_add(
			global_heap_timer_data.timer_check_id, 
			&check_node->list_node);

	return OK;

}

static int
heap_timer_count(
	struct list_head *node
)
{		
	int i = 0;
	int ret = 0, max = 0;
	int left = 0, div = 0;
	int timer_count = 0;
	struct hlist_node *hnode = NULL;
	struct heap_timer_node *timer_node = NULL;

	timer_node = list_entry(node, struct heap_timer_node, list_node);
	FREE(timer_node);
	while (1) {
		ret = thread_test_exit();
		if (ret == OK)
			break;
		sleep(1);
		ret = thread_test_exit();
		if (ret == OK)
			break;
		pthread_mutex_lock(&global_heap_timer_data.timer_tick.mutex);
		global_heap_timer_data.timer_tick.count++;
		pthread_mutex_unlock(&global_heap_timer_data.timer_tick.mutex);

		pthread_mutex_lock(&global_heap_timer_data.check_list_mutex);
		if (global_heap_timer_data.check_status != HEAP_TIMER_STATUS_RUNNING) {
			heap_timer_check_node_add(i);
			global_heap_timer_data.check_status = HEAP_TIMER_STATUS_RUNNING;
		}
		pthread_mutex_unlock(&global_heap_timer_data.check_list_mutex);
		/* 
		 * we hope that we can use multi-thread to handle many 
		 * timers, so we decide to splice the total timer count 
		 * into pieces. Here we just consider that this will work 
		 * fine, more tests are required.
		 */
		/*pthread_mutex_lock(&global_heap_timer_data.timer_count.mutex);
		if (timer_count != global_heap_timer_data.timer_count.count) {
			timer_count = global_heap_timer_data.timer_count.count;
			max = global_heap_timer_data.max_id;
			div = global_heap_timer_data.max_id / HEAP_TIMER_SPLICE;
			left = global_heap_timer_data.max_id % HEAP_TIMER_SPLICE;
		}
		pthread_mutex_unlock(&global_heap_timer_data.timer_count.mutex);*/

				
//		for (i = 0; i < global_heap_timer_data.thread_num; i++) 
	}

	return OK;
}

void
heap_timer_destroy(
	unsigned long id
)
{
	struct  heap_timer_node *node = NULL;

	if (!id)
		return;
	node = (struct heap_timer_node *)id;
	node->timer_flag = HEAP_TIMER_FLAG_MAX;

/*	hnode = hash_get(global_heap_timer_data.timer_hash, id, id);
	if (!hnode)
		return;

	PRINT(" id = %d\n", id);
		
	hash_del(global_heap_timer_data.timer_hash, hnode, id);*/

	//heap_timer_hash_destroy(hnode);
}

static int
heap_timer_handle(
	struct list_head *node
)
{
	int ret = 0;
	struct heap_timer_node *timer_node = NULL;

	timer_node = list_entry(node, struct heap_timer_node, list_node);

	if (timer_node->timer_func) {
		ret = timer_node->timer_func(timer_node->user_data);
		if (ret != OK || timer_node->timer_flag == HEAP_TIMER_FLAG_INSTANT)
			heap_timer_node_destroy(timer_node);
		else {
			pthread_mutex_lock(&timer_node->tick_mutex);
			timer_node->tick += timer_node->timer_sec;
			timer_node->status = HEAP_TIMER_STATUS_IDLE;
			pthread_mutex_unlock(&timer_node->tick_mutex);
			heap_node_add(global_heap_timer_data.timer_heap, (unsigned long)timer_node);
		}
	}

	return OK;
}

static int
heap_timer_count_start()
{
	struct heap_timer_node *timer_node = NULL;	

	timer_node = (struct heap_timer_node *)calloc(1, sizeof(*timer_node));
	if (!timer_node) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}
	return	thread_pool_node_add(global_heap_timer_data.timer_count_id, &timer_node->list_node);
}

static int
heap_timer_check(
	struct list_head *node
)
{
	int ret = 0;
	int tick = 0;
	int status = 0, node_tick = 0;
	int min_tick = 0;
	unsigned long user_data = 0;
	struct heap_timer_node *timer_node = NULL;
	struct heap_timer_check_node *check_node = NULL;

	check_node = list_entry(node, struct heap_timer_check_node, list_node);
	FREE(check_node);

	//heap_traversal(global_heap_timer_data.timer_heap, heap_timer_traversal);
	ret = heap_root_data_get(global_heap_timer_data.timer_heap, &user_data);
	if (ret !=OK || !user_data || user_data == -1) {
		goto out;
	}
	//heap_traversal(global_heap_timer_data.timer_heap, heap_timer_traversal);
	timer_node = (struct heap_timer_node *)user_data;
	min_tick = timer_node->tick;
	tick = heap_timer_tick_get();
	while (1) {
		pthread_mutex_lock(&timer_node->tick_mutex);
		status = timer_node->status;
		node_tick = timer_node->tick;
		pthread_mutex_unlock(&timer_node->tick_mutex);
		if (status == HEAP_TIMER_FLAG_MAX)  
			heap_timer_node_destroy(timer_node);
		else if (status != HEAP_TIMER_STATUS_RUNNING) {
			if (tick >= node_tick) {
				pthread_mutex_lock(&timer_node->tick_mutex);
				timer_node->status = HEAP_TIMER_STATUS_RUNNING;
				timer_node->tick = tick;
				pthread_mutex_unlock(&timer_node->tick_mutex);
				thread_pool_node_add(
					global_heap_timer_data.timer_handle_id, 
					&timer_node->list_node);
			} else {
				heap_node_add(
						global_heap_timer_data.timer_heap, 
						(unsigned long)timer_node);
				break;
			}
		} 
		/* 探测堆顶当前的堆顶结点是否和前一个堆顶节点的时间一致 */
		user_data = 0;
		ret = heap_root_data_peek(global_heap_timer_data.timer_heap, &user_data);
		if (ret !=OK || !user_data || user_data == -1)
			break;
		timer_node = (struct heap_timer_node *)user_data;
		pthread_mutex_lock(&timer_node->tick_mutex);
		if (min_tick != timer_node->tick) {
			pthread_mutex_unlock(&timer_node->tick_mutex);
			break;
		}
		pthread_mutex_unlock(&timer_node->tick_mutex);
		heap_root_data_get(global_heap_timer_data.timer_heap, &user_data);
	}
out:
	pthread_mutex_lock(&global_heap_timer_data.check_list_mutex);
	global_heap_timer_data.check_status = HEAP_TIMER_STATUS_IDLE;
	pthread_mutex_unlock(&global_heap_timer_data.check_list_mutex);
	
	return OK;
}

static void
heap_timer_check_node_init()
{
	int i = 0;
	int num = HEAP_TIMER_SPLICE;

	//INIT_LIST_HEAD(&global_heap_timer_data.check_list[i]);
	/*for (; i < num; i++) {
		INIT_LIST_HEAD(&global_heap_timer_data.check_list[i].head);
		INIT_LIST_HEAD(&global_heap_timer_data.check_list[i].check_head);
		pthread_mutex_init(&global_heap_timer_data.check_list[i].mutex, NULL);
		pthread_mutex_init(&global_heap_timer_data.check_list[i].check_mutex, NULL);
	}*/
}

static void
timer_free(
	unsigned long user_data
)
{
	heap_timer_node_destroy((struct heap_timer_node*)user_data);
}

static int
heap_timer_setup()
{
	int ret = 0;

	heap_timer_config_setup();

	ret = thread_pool_create(
				1,
				32 * 1024,
				"timer_count",
				NULL,
				heap_timer_count,
				NULL,
				&global_heap_timer_data.timer_count_id);
	if (ret != OK)
		return ret;

	ret = thread_pool_create(
				1,//global_heap_timer_data.thread_num,
				global_heap_timer_data.thread_stack,
				"timer_check",
				NULL,
				heap_timer_check, 
				NULL,
				&global_heap_timer_data.timer_check_id);
	if (ret != OK)
		return ret;
	/*
	 * we are not sure if we should make the timer handle threads' number 
	 * to be configurable. At present, just make it constant.
	 */
	if (global_heap_timer_data.thread_num > 1)
		ret = thread_pool_create(
				global_heap_timer_data.thread_num,
				global_heap_timer_data.thread_stack,
				"timer_handle",
				//timer_free,
				NULL,
				NULL,
				heap_timer_handle,
				&global_heap_timer_data.timer_handle_id);
	else
		ret = thread_pool_create(
				global_heap_timer_data.thread_num,
				global_heap_timer_data.thread_stack,
				"timer_handle",
				//timer_free,
				NULL,
				heap_timer_handle,
				NULL,
				&global_heap_timer_data.timer_handle_id);
	if (ret != OK) 
		return ret;

	heap_timer_count_start();
	return OK;
}

static void
heap_timer_list_data_destroy(
	struct heap_timer_list_data *list_data
)
{
	struct list_head *sl = NULL;
	struct heap_timer_data_node *data_node = NULL;

	if (!list_data)
		return;

	pthread_mutex_lock(&list_data->mutex);
	sl = list_data->head.next;
	while (sl != &list_data->head) {
		data_node = list_entry(sl ,struct heap_timer_data_node, list_node);
		sl = sl->next;
		list_del_init(&data_node->list_node);
		FREE(data_node);
	}
	pthread_mutex_unlock(&list_data->mutex);
	pthread_mutex_destroy(&list_data->mutex);
}

static int
heap_timer_uninit()
{
	
	heap_timer_list_data_destroy(&global_heap_timer_data.free_id_list);
	heap_destroy(global_heap_timer_data.timer_heap, 
			timer_free);
	return OK;
}

static int
heap_timer_config_setup()
{
	cJSON *conf = NULL;
	global_heap_timer_data.thread_num = HEAP_TIMER_SPLICE;
	global_heap_timer_data.thread_stack = THREAD_DEFALUT_STACK;

	conf = config_read_get("general");
	CR_INT(conf, "timer_thread_num", global_heap_timer_data.thread_num);
	CR_INT(conf, "timer_stack_size", global_heap_timer_data.thread_stack);
/*	CONFIG_ADD(
		"timer_thread_num", 
		&global_heap_timer_data.thread_num, 
		FUNC_NAME(INT));
	CONFIG_ADD(
		"timer_stack_size", 
		&global_heap_timer_data.thread_stack,
		FUNC_NAME(INT));*/
	
	return OK;
}

static int
heap_timer_cmp(
	unsigned long new,
	unsigned long old
)
{
	int ret = 0;
	struct heap_timer_node *new_node = NULL;
	struct heap_timer_node *old_node = NULL;

	new_node = (struct heap_timer_node*)new;
	old_node = (struct heap_timer_node*)old;

	pthread_mutex_lock(&new_node->tick_mutex);
	pthread_mutex_lock(&old_node->tick_mutex);
	if (new_node->tick > old_node->tick)
		ret = OK;
	else
		ret = ERR;
	pthread_mutex_unlock(&old_node->tick_mutex);
	pthread_mutex_unlock(&new_node->tick_mutex);

	return ret;
}

int
heap_timer_init()
{
	int ret = 0;

	memset(&global_heap_timer_data, 0, sizeof(global_heap_timer_data));

	INIT_LIST_HEAD(&global_heap_timer_data.timer_list.head);
	INIT_LIST_HEAD(&global_heap_timer_data.free_id_list.head);
	pthread_mutex_init(&global_heap_timer_data.timer_list.mutex, NULL);
	pthread_mutex_init(&global_heap_timer_data.free_id_list.mutex, NULL);
	pthread_mutex_init(&global_heap_timer_data.timer_tick.mutex, NULL);
	pthread_mutex_init(&global_heap_timer_data.timer_count.mutex, NULL);
	pthread_mutex_init(&global_heap_timer_data.check_list_mutex, NULL);
	//heap_timer_check_node_init();

	global_heap_timer_data.timer_heap = heap_create(heap_timer_cmp);
	if (!global_heap_timer_data.timer_heap)
		PANIC("create heap error : %s\n", CUR_ERRMSG_GET());
/*	global_heap_timer_data.timer_hash = 	hash_create(
						HEAP_TIMER_TABLE_SIZE,		
						heap_timer_hash, 
						heap_timer_hash_get, 
						heap_timer_hash_destroy);
	if (global_heap_timer_data.timer_hash == HASH_ERR) 
		return ERR;*/

	/*ret = user_cmd_add();
	if (ret != OK)
		return ret;*/

	ret = local_init_register(heap_timer_setup);
	if (ret != OK)
		return ret;

	return uninit_register(heap_timer_uninit);
}

MOD_INIT(heap_timer_init);
