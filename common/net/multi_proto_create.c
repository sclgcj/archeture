#include "std_comm.h"
#include "err.h"
#include "init.h"
#include "print.h"
#include "print.h"
#include "hash.h"
#include "global_log_private.h"

/*
 * 目前的实现前提是：同时使用的协议数量比较少。
 * 本想使用hash的，但是hash需要外部传入参数，
 * 使用起来比较麻烦，而且就目前情况来说，可能
 * 同时使用的协议不多，可以考虑把hash换成链表。
 * 希望以后能找到既能少传参数又能满足快速访问
 * 多个协议中的某一个的方法
 */

struct multi_proto_node {
	char *proto;
	char *data;
	int realloc;
	int data_cnt;
	int data_size;
	pthread_mutex_t mutex;
	struct list_head lnode;
	struct hlist_node node;
};

#define CREATE_PROTO_HASH_NUM 32
struct multi_proto_data {
	int proto_cnt;
	struct list_head proto_list;
	hash_handle_t mpd_hash;
	pthread_mutex_t list_mutex;
	pthread_mutex_t proto_mutex;
};

static struct multi_proto_data global_multi_proto_data;

static struct multi_proto_node *
multi_proto_node_create(
	int data_size,
	int data_cnt,
	char *proto
)
{
	struct multi_proto_node *pnode = NULL;	

	pnode = (struct multi_proto_node *)calloc(1, sizeof(*pnode));
	if (!pnode) 
		PANIC("Not enough memory for %d bytes\n", sizeof(*pnode));

	pnode->proto = strdup(proto);
	pnode->data_size = data_size;
	pnode->data_cnt = data_cnt;
	pnode->data = (char*)calloc(data_cnt, data_size);
	if (!pnode->data) 
		PANIC("Not enough memory for %d bytes\n", data_cnt * data_size);
	pthread_mutex_init(&pnode->mutex, NULL);

	return pnode;
}

static void
multi_proto_node_destroy(
	struct multi_proto_node *pnode
)
{
	FREE(pnode->proto);
	FREE(pnode->data);
	FREE(pnode);
	pthread_mutex_destroy(&pnode->mutex);
}

int
multi_proto_add(
	int  data_size,
	int  data_cnt,
	char *proto
)
{
	int ret = 0;
	struct multi_proto_node *pnode = NULL;	

	pnode = multi_proto_node_create(data_size, data_cnt, proto);

	ret = hash_add(global_multi_proto_data.mpd_hash, 
			  &pnode->node, (unsigned long)proto);
	if (ret != OK) {
		multi_proto_node_destroy(pnode);
		return ret;
	}

	pthread_mutex_lock(&global_multi_proto_data.list_mutex);
	list_add_tail(&pnode->lnode, &global_multi_proto_data.proto_list);
	pthread_mutex_unlock(&global_multi_proto_data.list_mutex);

	return OK;
}

void
multi_proto_user_data_get(
	int id,
	char *proto,
	unsigned long *user_data
)
{
	int data_size = 0;
	struct hlist_node *hnode = NULL;		
	struct multi_proto_node *pnode = NULL;

	hnode = hash_get(global_multi_proto_data.mpd_hash, 
			    (unsigned long)proto,
			    (unsigned long)proto);
	if (!hnode) 
		return;

	pnode = list_entry(hnode, struct multi_proto_node, node);
	data_size = pnode->data_size;	
	(*user_data) = (unsigned long)&pnode->data[id * data_size];
}

#define ADDRINPATH(cur, start, end) \
	((cur >= start && cur <= end) || (cur >= end && cur <= start))

void
multi_proto_data_id_get(
	unsigned long user_data,
	int	      *id
)
{
	int ret = 0;
	int end = 0;
	struct list_head *sl = NULL;
	struct multi_proto_node *pnode = NULL;

	sl = global_multi_proto_data.proto_list.next;	
	pthread_mutex_lock(&global_multi_proto_data.list_mutex);
	while(sl != &global_multi_proto_data.proto_list) {
		pnode = list_entry(sl, struct multi_proto_node, lnode);
		end = pnode->data_cnt * pnode->data_size;
		ret = ADDRINPATH(user_data, 
				 (unsigned long)pnode->data, 
				 (unsigned long)&pnode->data[end-1]);
		if (ret) {
			*id = labs(user_data - (unsigned long)pnode->data) / pnode->data_size;
			break;
		}
		sl = sl->next;
	}
	pthread_mutex_unlock(&global_multi_proto_data.list_mutex);
}

static int
multi_proto_hash(
	struct hlist_node *hnode,
	unsigned long user_data
)
{
	int proto = 0;
	struct multi_proto_node *pnode = NULL;

	if (!hnode)
		proto = ((char*)user_data)[0];
	else {
		pnode = list_entry(hnode, struct multi_proto_node, node);
		if (pnode->proto)
			proto = pnode->proto[0];
	}

	return (proto % CREATE_PROTO_HASH_NUM);
}

static int
multi_proto_hash_get(
	struct hlist_node *hnode,
	unsigned long user_data
)
{
	struct multi_proto_node *pnode = NULL;

	pnode = list_entry(hnode, struct multi_proto_node, node);
	if (!strcmp((char*)user_data, pnode->proto))
		return OK;

	return ERR;
}

static int
multi_proto_hash_destroy(
	struct hlist_node *hnode
)
{
	struct multi_proto_node *pnode = NULL;

	pnode = list_entry(hnode, struct multi_proto_node, node);
	multi_proto_node_destroy(pnode);

	return OK;
}

int
multi_proto_init()
{
	GINFO("multi_proto_init\n");
	memset(&global_multi_proto_data, 0, sizeof(global_multi_proto_data));
	pthread_mutex_init(&global_multi_proto_data.proto_mutex, NULL);
	INIT_LIST_HEAD(&global_multi_proto_data.proto_list);
	global_multi_proto_data.mpd_hash = hash_create(
						CREATE_PROTO_HASH_NUM, 
						multi_proto_hash, 
						multi_proto_hash_get,
						multi_proto_hash_destroy);
	if (global_multi_proto_data.mpd_hash == HASH_ERR)
		return ERR;

	return OK;
}

MOD_INIT(multi_proto_init);
