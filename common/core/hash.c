#include "hash.h"
#include "err.h"
#include "print.h"

/**
 * Luckly, the design of this hash list module is thread-safety. we 
 * add a mutex lock on every hash list. What? the table? it is inited
 * at start and uninited at the end when we are sure that there is nothing
 * to use it and we're sure at the begining.
 *
 * This is a static hash table and there is some limitations on it. We hope 
 * to use the dynamical hash table to replace it...
 *
 */

struct hash_head{
	pthread_mutex_t hlist_mutex;
	struct hlist_head head;
};

struct hash_table{
	int table_size;
	struct hash_head *head;
	int (*hash_func)(struct hlist_node *node, unsigned long user_data);
	int (*hash_get)(struct hlist_node *node, unsigned long user_data);
	int (*hash_destroy)(struct hlist_node *node);
};

hash_handle_t
hash_create(
	int table_size,
	int (*hash_func)(struct hlist_node *node, unsigned long user_data),
	int (*hash_get)(struct hlist_node *node, unsigned long user_data),
	int (*hash_destroy)(struct hlist_node *node)
)
{
	int i = 0;
	struct hash_table *hash_table = NULL;;

	if (!table_size || !hash_get) {
		ERRNO_SET(PARAM_ERROR);
		return HASH_ERR;
	}

	hash_table = (struct hash_table *)calloc(sizeof(*hash_table), 1);
	if (!hash_table) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return HASH_ERR;	
	}
	hash_table->hash_func = hash_func;
	hash_table->hash_get  = hash_get;
	hash_table->hash_destroy = hash_destroy;
	hash_table->table_size = table_size;
	hash_table->head = (struct hash_head*)
				calloc(sizeof(struct hash_head), table_size);
	if (!hash_table->head) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		free(hash_table);
		return HASH_ERR;
	}
	for (; i < table_size; i++) {
		INIT_HLIST_HEAD(&hash_table->head[i].head);
		pthread_mutex_init(&hash_table->head[i].hlist_mutex, NULL);
	}

	return (hash_handle_t)hash_table;
}

int
hash_destroy(
	hash_handle_t handle	
)
{
	int i = 0;
	int ret = 0;
	struct hlist_node *hnode = NULL, *hsave = NULL;
	struct hash_table *hash_table = NULL;

	hash_table = (struct hash_table *)handle;
	if (!hash_table || (void*)handle == HASH_ERR) {
		ERRNO_SET(PARAM_ERROR);
		return -1;
	}

	for (; i < hash_table->table_size; i++) {
		pthread_mutex_lock(&hash_table->head[i].hlist_mutex);
		if (hlist_empty(&hash_table->head[i].head)) {
			pthread_mutex_unlock(&hash_table->head[i].hlist_mutex);
			continue;
		}
		hnode = hash_table->head[i].head.first;
		while(hnode) {
			hsave = hnode->next;
			hlist_del_init(hnode);
			if (hash_table->hash_destroy) {
				ret = hash_table->hash_destroy(hnode);
				if (ret != OK) {
					return ret;
				}
			}
			hnode = hsave;
		}
		pthread_mutex_unlock(&hash_table->head[i].hlist_mutex);
	}

	return OK;
}

static int
hlist_safe_check(
	int pos,
	struct hash_table *hash_table
)
{
		return ERR;		
}

int
hash_add(
	hash_handle_t handle,
	struct hlist_node *node,
	unsigned long user_data
)
{
	int pos = 0, ret = 0;
	struct hlist_node *hnode = NULL;
	struct hash_table *hash_table = NULL;

	hash_table = (struct hash_table*)handle;
	if (!hash_table || (void*)handle == HASH_ERR) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	if (hash_table->hash_func)
		pos = hash_table->hash_func(node, user_data);
	else
		pos = 0;
	if (pos < 0 || pos >= hash_table->table_size) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	pthread_mutex_lock(&hash_table->head[pos].hlist_mutex);
	hlist_add_head(node, &hash_table->head[pos].head);
	pthread_mutex_unlock(&hash_table->head[pos].hlist_mutex);

	return OK;
}

int
hash_traversal(
	unsigned long user_data,
	hash_handle_t handle,
	int (*hash_walk_handle)(unsigned long user_data, struct hlist_node *hnode, int *flag)
)
{
	int ret = 0;
	int pos = 0;
	int del_flag = 0;
	struct hlist_node *hnode = NULL, *next = NULL, **hsave = NULL;
	struct hash_table *hash_table = NULL; 

	hash_table = (struct hash_table*)handle;

	for (; pos < hash_table->table_size; pos++) {
			
		pthread_mutex_lock(&hash_table->head[pos].hlist_mutex);
		if (hlist_empty(&hash_table->head[pos].head)) {
			pthread_mutex_unlock(&hash_table->head[pos].hlist_mutex);
			continue;
		}
		hlist_for_each_safe(hnode, next, &hash_table->head[pos].head) {	
			if (hash_walk_handle) {
				ret = hash_walk_handle(user_data, hnode, &del_flag);
				if (ret != OK) {
					pthread_mutex_unlock(
						&hash_table->head[pos].hlist_mutex);
					return ret;
				}
			}
			if (del_flag) {
				hsave = hnode->pprev;
				hlist_del_init(hnode);
				hash_table->hash_destroy(hnode);
				hnode = *hsave;
			}
		}
		pthread_mutex_unlock(&hash_table->head[pos].hlist_mutex);
		pthread_mutex_destroy(&hash_table->head[pos].hlist_mutex);
	}
	FREE(hash_table->head);

	return OK;
}

int
hash_del(
	hash_handle_t handle,
	struct hlist_node *node,
	unsigned long user_data
)
{
	int pos = 0;	
	struct hlist_node *hnode = NULL;
	struct hash_table *hash_table = NULL;

	hash_table = (struct hash_table *)handle;
	if (!hash_table || (void*)handle == HASH_ERR) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	if (hash_table->hash_func) 
		pos = hash_table->hash_func(node, user_data);
	else
		pos = 0;
	if (pos < 0 || pos >= hash_table->table_size) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}
	pthread_mutex_lock(&hash_table->head[pos].hlist_mutex);
	hlist_del_init(node);
	pthread_mutex_unlock(&hash_table->head[pos].hlist_mutex);

	return OK;
}

int
hash_del_and_destroy(
	hash_handle_t handle,
	struct hlist_node *node,
	unsigned long user_data
)
{
	int pos = 0;	
	struct hlist_node *hnode = NULL;
	struct hash_table *hash_table = NULL;

	hash_table = (struct hash_table *)handle;
	if (!hash_table || (void*)handle == HASH_ERR) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	if (hash_table->hash_func) 
		pos = hash_table->hash_func(node, user_data);
	else
		pos = 0;
	if (pos < 0 || pos >= hash_table->table_size) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}
	pthread_mutex_lock(&hash_table->head[pos].hlist_mutex);
	hlist_del_init(node);
	hash_table->hash_destroy(node);
	pthread_mutex_unlock(&hash_table->head[pos].hlist_mutex);

	return OK;
}

struct hlist_node *
hash_get(
	hash_handle_t handle,
	unsigned long hash_data,
	unsigned long search_cmp_data
)
{
	int pos = 0, ret = 0;	
	struct hlist_node *hnode = NULL, *safe = NULL;
	struct hash_table *hash_table = NULL;

	hash_table = (struct hash_table *)handle;
	if (!hash_table || (void*)handle == HASH_ERR) {
		ERRNO_SET(PARAM_ERROR);
		return NULL;
	}

	if (hash_table->hash_func) 
		pos = hash_table->hash_func(hnode, hash_data);
	else
		pos = 0;
	if (pos < 0 || pos >= hash_table->table_size) {
		ERRNO_SET(PARAM_ERROR);
		return NULL;
	}

	pthread_mutex_lock(&hash_table->head[pos].hlist_mutex);
	if (hlist_empty(&hash_table->head[pos].head)) {
		pthread_mutex_unlock(&hash_table->head[pos].hlist_mutex);
		return NULL;
	}
	hlist_for_each_safe(hnode, safe, &hash_table->head[pos].head) {
		ret = hash_table->hash_get(hnode, search_cmp_data);
		if (ret == OK) {
			break;
		}
	}
	pthread_mutex_unlock(&hash_table->head[pos].hlist_mutex);

	return hnode;
}

int
hash_head_traversal(
	hash_handle_t  handle,		
	unsigned long     hash_data,
	unsigned long	  user_data,
	void (*traversal)(struct hlist_node *hnode, unsigned long user_data)
)
{
	int pos = 0, ret = 0;	
	struct hlist_node *hnode = NULL, *safe = NULL;
	struct hash_table *hash_table = NULL;

	hash_table = (struct hash_table *)handle;
	if (!hash_table || (void*)handle == HASH_ERR) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	if (hash_table->hash_func) {
		pos = hash_table->hash_func(hnode, hash_data);
	}
	else
		pos = 0;
	if (pos < 0 || pos >= hash_table->table_size) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	pthread_mutex_lock(&hash_table->head[pos].hlist_mutex);
	if (hlist_empty(&hash_table->head[pos].head)) {
		pthread_mutex_unlock(&hash_table->head[pos].hlist_mutex);
		return ERR;
	}
	hlist_for_each_safe(hnode, safe, &hash_table->head[pos].head) {
		if (traversal)
			traversal(hnode, user_data);
	}
	pthread_mutex_unlock(&hash_table->head[pos].hlist_mutex);

	return OK;
}

