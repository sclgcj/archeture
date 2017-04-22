#include "std_comm.h"
#include "err.h"
#include "init_private.h"
#include "print.h"

struct thread_data {
	char *hd_thread_name;
	int  hd_group_id;
	int  hd_member_id;
};

struct thread_member {
	pthread_mutex_t tm_mutex;
	pthread_cond_t  tm_cond;
	struct list_head tm_head;
	struct list_head tm_node;
};

struct thread_group {
	int tg_count;
	struct thread_member *tg_member;
	void (*tg_group_free)(struct list_head *node);
	int  (*tg_group_func)(struct list_head *node);
	int  (*tg_execute_func)(struct list_head *node);
	pthread_cond_t tg_cond;
	pthread_mutex_t tg_mutex;
	struct list_head tg_head;
};


#define THREAD_GROUP_INIT 256
static pthread_condattr_t global_cond_attr;
static pthread_mutex_t global_group_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t global_exit_mutex = PTHREAD_MUTEX_INITIALIZER;
static int global_thread_group_num = 0, global_thread_group_count = 0;
static int global_thread_num = 0, global_thread_exit = 0;
static struct thread_group *global_thread_group = NULL;

int
thread_test_exit()
{
	int ret = ERR;
	pthread_mutex_lock(&global_exit_mutex);
	if (global_thread_exit) {
		pthread_mutex_lock(&global_group_mutex);
		if (global_thread_num > 0)
			global_thread_num--;
		pthread_mutex_unlock(&global_group_mutex);
		ret = OK;
	}
	pthread_mutex_unlock(&global_exit_mutex);

	return ret;
}

static void
thread_start()
{
	pthread_mutex_lock(&global_group_mutex);
	global_thread_num++;
	pthread_mutex_unlock(&global_group_mutex);
}

static int
get_thread_node(
	struct list_head *head,
	struct list_head **node
)
{
	if (list_empty(head)) 
		return ERR;

	(*node) = head->next;
	list_del_init(*node);

	return OK;
}

static int
get_thread_group_node(
	int id,
	struct list_head **sl
)
{
	int ret = 0;
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts.tv_sec += 1;
	pthread_mutex_lock(&global_thread_group[id].tg_mutex);
	ret = get_thread_node(&global_thread_group[id].tg_head, sl);
	if (ret != OK) {
		ret = pthread_cond_timedwait(
					&global_thread_group[id].tg_cond, 
					&global_thread_group[id].tg_mutex, 
					&ts);
		if (ret == ETIMEDOUT) 
			ret = TIMEOUT;

		ret = get_thread_node(&global_thread_group[id].tg_head, sl);

	}
	pthread_mutex_unlock(&global_thread_group[id].tg_mutex);

	return ret;
}

static int
add_thread_member_node(
	struct list_head *node,
	struct thread_member *thread_memb
)
{
	int ret = 0;

	pthread_mutex_lock(&thread_memb->tm_mutex);
	list_add_tail(node, &thread_memb->tm_head);
	pthread_cond_broadcast(&thread_memb->tm_cond);
	pthread_mutex_unlock(&thread_memb->tm_mutex);

	return OK;
}

static void *
master_thread(
	void *arg
)
{
	int i = 0, id = 0, ret = 0, count = 0;
	char *name = NULL;
	struct list_head *sl = NULL;
	struct thread_data *thread_data = NULL;

	thread_data = (struct thread_data*)arg;
	id = thread_data->hd_group_id;
	if (thread_data->hd_thread_name) {
		name = (char*)calloc(256, sizeof(char));
		if (!name) 
			PANIC("no enough memory\n");
		prctl(PR_SET_NAME, name);
		FREE(name);
	}
	FREE(arg);
	while (1) {
		ret = thread_test_exit();
		if (ret == OK)
			break;
		ret = get_thread_group_node(id, &sl);
		if (ret != OK || !sl)
			continue;
		if (thread_test_exit() == OK)
			break;
		if (global_thread_group[id].tg_group_func) {
			ret = global_thread_group[id].tg_group_func(sl);
			if (ret != OK)
				continue;
		}
		if (global_thread_group[id].tg_count <= 1)
			continue;

		for (i = count; i < global_thread_group[id].tg_count; i++) {
			ret = add_thread_member_node(sl, &global_thread_group[id].tg_member[i]);
			if (ret == OK)
				break;
		}
		if (i + 1 == global_thread_group[id].tg_count || 
				i == global_thread_group[id].tg_count) 
			i = 0;
		i++;
		count = i;
		if (count == global_thread_group[id].tg_count)
			count = 0;
	}

	return NULL;
}

static int
master_thread_create(
	char *thread_name,
	int  thread_id,
	pthread_attr_t *attr,
	struct thread_group *group
)
{
	pthread_t th;
	int *id = NULL;
	struct thread_data *thread_data = NULL;

	thread_data = (struct thread_data*)calloc(1, sizeof(*thread_data));
	if (!thread_data) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}
	thread_data->hd_group_id = thread_id;
	thread_data->hd_thread_name = thread_name;

	if (pthread_create(&th, attr, master_thread, (void*)thread_data) < 0) {
		ERRNO_SET(CREATE_THREAD_ERR);
		return ERR;
	}
	thread_start();

	return OK;
}

static struct thread_data *
thread_data_calloc(
	int pos,
	int group_id,
	char *thread_name
)
{
	struct thread_data *thread_data = NULL;

	thread_data = (struct thread_data*)
				calloc(1, sizeof(*thread_data));
	if (!thread_data) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return NULL;
	}
	thread_data->hd_member_id = pos;
	thread_data->hd_group_id  = group_id;
	thread_data->hd_thread_name = thread_name;

	return thread_data;
}

static void
thread_member_init(
	struct thread_member *memb
)
{
	pthread_mutex_init(&memb->tm_mutex, NULL);
	pthread_cond_init(&memb->tm_cond, &global_cond_attr);
	INIT_LIST_HEAD(&memb->tm_head);
}

static int
get_thread_member_node(
	int group_id,
	int memb_id,
	struct list_head **node
)
{
	int ret = OK;
	struct timespec ts;
	struct thread_member *thread_memb = NULL;

	thread_memb = &global_thread_group[group_id].tg_member[memb_id];
	pthread_mutex_lock(&thread_memb->tm_mutex);
	ret = get_thread_node(&thread_memb->tm_head, node);
	if (ret != OK) {
		clock_gettime(CLOCK_MONOTONIC, &ts);
		ts.tv_sec += 1;
		ret = pthread_cond_timedwait(
					&thread_memb->tm_cond, 
					&thread_memb->tm_mutex, 
					&ts);
		if (ret == ETIMEDOUT) 
			ret = TIMEOUT;
		else
			ret = get_thread_node(&thread_memb->tm_head, node);
	}
	pthread_mutex_unlock(&thread_memb->tm_mutex);

	return ret;
}

static void *
thread_member_handle(
	void *arg
)
{
	int ret = 0;
	struct list_head *sl = NULL;
	struct thread_data *thread_data = NULL;

	thread_data = (struct thread_data*)arg;
	if (thread_data->hd_thread_name)
		prctl(PR_SET_NAME, thread_data->hd_thread_name);

	while (1) {
		if(thread_test_exit() == OK)
			break;
		ret = get_thread_member_node(
					thread_data->hd_group_id,
					thread_data->hd_member_id,
					&sl);
		if (ret != OK || !sl)
			continue;
		if (thread_test_exit() == OK) 
			break;

		if (global_thread_group[thread_data->hd_group_id].tg_execute_func) {
			global_thread_group[thread_data->hd_group_id].tg_execute_func(sl);
		}
	}

	FREE(thread_data);
	pthread_exit(NULL);
}

static int
member_thread_create(
	char *thread_name,
	int  group_id,
	pthread_attr_t *attr,
	struct thread_group *group
)
{
	int i = 0;	
	pthread_t th;
	struct thread_data *thread_data = NULL;

	group->tg_member = (struct thread_member*)
				calloc(group->tg_count, sizeof(*group->tg_member));
	if (!group->tg_member) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}

	for (; i < group->tg_count; i++) {
		thread_data = thread_data_calloc(i, group_id, thread_name);
		if (!thread_data)
			return ERR;
		thread_member_init(&global_thread_group[group_id].tg_member[i]);
		if (pthread_create(&th, attr, thread_member_handle, thread_data) < 0) {
			ERRNO_SET(CREATE_THREAD_ERR);
			return ERR;
		}
		thread_start();
	}

	return OK;
}

int
thread_pool_create(
	int count,
	int stack_size,
	char *thread_name,
	void (*group_free)(struct list_head *node),
	int  (*group_func)(struct list_head *node),
	int  (*execute_func)(struct list_head *node),
	int  *group_id
)
{
	int id = 0;
	int ret = 0;
	pthread_attr_t attr; 
	
	pthread_mutex_lock(&global_group_mutex);
	id = global_thread_group_count++;
	pthread_mutex_unlock(&global_group_mutex);
	if (group_id)
		(*group_id) = id;
	global_thread_group[(id)].tg_count = count;
	global_thread_group[(id)].tg_execute_func = execute_func;
	global_thread_group[(id)].tg_group_free = group_free;
	global_thread_group[(id)].tg_group_func = group_func;

	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, stack_size);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	ret = master_thread_create(thread_name, id, &attr, &global_thread_group[(id)]);
	if (ret != OK)
		return ret;

	if (count <= 1)
		return OK;

	return member_thread_create(thread_name, id, &attr, &global_thread_group[(id)]);
}

void
thread_exit_wait()
{
	int i = 0;
	int sec = 5;
	int num = 0;

	pthread_mutex_lock(&global_exit_mutex);
	global_thread_exit = 1;
	num = global_thread_group_count;
	pthread_mutex_unlock(&global_exit_mutex);
	for (; i < num; i++) 
		pthread_cond_broadcast(&global_thread_group[i].tg_cond);
	while(sec) {
		pthread_mutex_lock(&global_group_mutex);
		num = global_thread_num;
		pthread_mutex_unlock(&global_group_mutex);
		if (num == 0)
		{
			break;
		}
		sleep(1);
		sec--;
	}
	
}

static void
thread_list_free(
	struct thread_group *group,
	struct list_head *head
)
{
	struct list_head *sl = NULL;

	sl = head->next;
	pthread_mutex_lock(&group->tg_mutex);
	while (sl != head) {
		list_del_init(sl);
		if (group->tg_group_free)
			group->tg_group_free(sl);
		sl = head->next;
	}
	pthread_mutex_unlock(&group->tg_mutex);
}

static int
thread_uninit()
{
	int i = 0, j = 0;
	struct list_head *sl = NULL;

	/*
	 * Here, we don't think about a better way to handle the thread exit.
	 * This is just a temporary resolution, if we find a better one, we 
	 * will change it.
	 */
	pthread_mutex_lock(&global_group_mutex);
	for (; i < global_thread_group_num; i++) {
		if (global_thread_group[i].tg_count < 1)
			continue;

		thread_list_free(&global_thread_group[i], 
				    &global_thread_group[i].tg_head);
		for (j = 0; j < global_thread_group[i].tg_count; j++) {
			if (!global_thread_group[i].tg_member)
				continue;
			if (list_empty(&global_thread_group[i].tg_member[j].tm_head))
				continue;
			thread_list_free((&global_thread_group[i]),
					     &global_thread_group[i].tg_member[j].tm_head);
			//sl = global_thread_group[i].tg_member[j].tm_head.next;
			pthread_mutex_destroy(&global_thread_group[i].tg_member[j].tm_mutex);
			pthread_cond_destroy(&global_thread_group[i].tg_member[j].tm_cond);
		}
	}
	pthread_mutex_unlock(&global_group_mutex);

	FREE(global_thread_group);

	return OK;
}

int
thread_pool_node_add(
	int  group_id,
	struct list_head *node
)
{
	int ret = 0;

	pthread_mutex_lock(&global_thread_group[group_id].tg_mutex);
	list_add_tail(node, &global_thread_group[group_id].tg_head);
	pthread_cond_broadcast(&global_thread_group[group_id].tg_cond);
	pthread_mutex_unlock(&global_thread_group[group_id].tg_mutex);

	return OK;
}

int
thread_init()
{
	int i = 0;
	global_thread_group_num = THREAD_GROUP_INIT;
	pthread_condattr_init(&global_cond_attr);
	pthread_condattr_setclock(&global_cond_attr, CLOCK_MONOTONIC);

	global_thread_group = (struct thread_group*)
				calloc(global_thread_group_num, sizeof(*global_thread_group));
	if (!global_thread_group) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}
	for (; i < global_thread_group_num; i++) {
		pthread_cond_init(&global_thread_group[i].tg_cond, &global_cond_attr);
		pthread_mutex_init(&global_thread_group[i].tg_mutex, NULL);
		INIT_LIST_HEAD(&global_thread_group[i].tg_head);
	}

	return uninit_register(thread_uninit);
}

MOD_INIT(thread_init);
