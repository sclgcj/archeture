#include "std_comm.h"
#include "err.h"
#include "cmd.h"
#include "init.h"
#include "hash.h"
#include "print.h"
#include "config.h"
#include "transfer_proto_private.h"

struct transfer_proto_node {
	int link_type;
	struct transfer_proto_oper oper;
	struct hlist_node node;
};

struct transfer_proto_config_node {
	char *proto_name;
	int  proto;
	struct hlist_node node;
};

struct transfer_proto_config {
	char proto_name[64];
};

struct transfer_proto_data {
	int proto_count;
	pthread_mutex_t  mutex;
	hash_handle_t proto_config_hash;
	hash_handle_t proto_hash;
};

#define PROTO_HASH_SIZE 128
#define PROTO_CONFIG_SIZE 26
static struct transfer_proto_config global_proto_config;
static struct transfer_proto_data global_proto_data;

static int
transfer_proto_type_get(
	char *proto_name
)
{
	struct hlist_node *hnode = NULL;
	struct transfer_proto_config_node *pnode = NULL;

	hnode = hash_get(global_proto_data.proto_config_hash,
			    (unsigned long)proto_name, 
			    (unsigned long)proto_name);
	if (!hnode)
		PANIC("Can't recognize proto type %s\n", proto_name);

	pnode = list_entry(hnode, struct transfer_proto_config_node, node);

	return pnode->proto;
}

/*static CONFIG_FUNC(TPROTO)
{
	int proto = 0;

	proto = transfer_proto_type_get(val);
	*(int*)user_data = proto;
}*/

static int
transfer_proto_config_set()
{
	//CONFIG_ADD("proto", global_proto_config.proto_name, FUNC_NAME(STR));
	//CONFIG_ADD("device", &global_proto_config.dev_type, FUNC_NAME(DEV));
}


int
transfer_proto_config_add(
	char *name,
	int  proto
)
{
	struct transfer_proto_config_node *pnode = NULL;

	pnode = (struct transfer_proto_config_node *)calloc(1, sizeof(*pnode));
	if (!pnode) 
		PANIC("Can't calloc memory for %d bytes\n", sizeof(*pnode));
	if (name)
		pnode->proto_name = strdup(name);
	pnode->proto = proto;

	return hash_add(global_proto_data.proto_config_hash, 
			   &pnode->node, (unsigned long)name);
}

int
transfer_proto_add(
	struct transfer_proto_oper *oper,
	int			      *proto_id
)
{
	struct transfer_proto_node *pnode = NULL;	

	pnode = (struct transfer_proto_node *)calloc(1, sizeof(*pnode));
	if (!pnode)
		PANIC("Not enough memory for %d bytes\n", sizeof(*pnode));
	pthread_mutex_lock(&global_proto_data.mutex);
	pnode->link_type = global_proto_data.proto_count++;
	pthread_mutex_unlock(&global_proto_data.mutex);
	memcpy(&pnode->oper, oper, sizeof(*oper));
	(*proto_id) = pnode->link_type;

	return hash_add(global_proto_data.proto_hash, &pnode->node, 0);
}

struct transfer_proto_oper*
transfer_proto_oper_get()
{
	struct hlist_node *hnode = NULL;
	struct transfer_proto_config_node *cnode = NULL;
	struct transfer_proto_node *pnode = NULL;

	hnode = hash_get(global_proto_data.proto_config_hash, 
			    (unsigned long)global_proto_config.proto_name, 
			    (unsigned long)global_proto_config.proto_name);
	if (!hnode) {
		ERRNO_SET(NOT_REGISTER_CMD);
		return NULL;
	}
	cnode = list_entry(hnode, struct transfer_proto_config_node, node);
	hnode = hash_get(global_proto_data.proto_hash, 
			    cnode->proto, 
			    cnode->proto);
	if (!hnode)
		return NULL;
	pnode = list_entry(hnode, struct transfer_proto_node, node);

	return &pnode->oper;
}

struct transfer_proto_oper*
transfer_proto_oper_get_by_name(
	char *proto_name
)
{
	int proto_type = 0;
	struct hlist_node *hnode = NULL;
	struct transfer_proto_node *pnode = NULL;

	proto_type = transfer_proto_type_get(proto_name);

	hnode = hash_get(global_proto_data.proto_hash, 
			    (unsigned long)proto_type, 
			    (unsigned long)proto_type);
	if (!hnode) {
		ERRNO_SET(NOT_REGISTER_CMD);
		return NULL;
	}
	pnode = list_entry(hnode, struct transfer_proto_node, node);

	return &pnode->oper;
}

struct transfer_proto_oper*
transfer_proto_oper_get_by_type(
	int proto_type
)
{
	struct hlist_node *hnode = NULL;
	struct transfer_proto_node *pnode = NULL;

	hnode = hash_get(global_proto_data.proto_hash, 
			    (unsigned long)proto_type, 
			    (unsigned long)proto_type);
	if (!hnode) {
		ERRNO_SET(NOT_REGISTER_CMD);
		return NULL;
	}
	pnode = list_entry(hnode, struct transfer_proto_node, node);

	return &pnode->oper;
}

static int
transfer_proto_uninit()
{
	hash_destroy(global_proto_data.proto_hash);
	return OK;
}

static int
transfer_proto_hash(
	struct hlist_node	*hnode,
	unsigned long		user_data	
)
{
	int link_type = 0;
	struct transfer_proto_node *proto_node = NULL;

	if (!hnode)
		link_type = (int)user_data;
	else {
		proto_node = list_entry(hnode, struct transfer_proto_node, node);
		link_type = proto_node->link_type;
	}

	return (link_type % PROTO_HASH_SIZE);
}

static int
transfer_proto_hash_get(
	struct hlist_node *hnode,
	unsigned long	  user_data
)
{
	struct transfer_proto_node *pnode = NULL;

	if (!hnode)
		return ERR;

	pnode = list_entry(hnode, struct transfer_proto_node, node);
	if ((int)user_data == pnode->link_type)
		return OK;

	return ERR;
}

static int
transfer_proto_hash_destroy(
	struct hlist_node *hnode
)
{
	struct transfer_proto_node *pnode = NULL;

	if (!hnode)
		return ERR;

	pnode = list_entry(hnode, struct transfer_proto_node, node);
	FREE(pnode);

	return OK;
}

static int
transfer_proto_config_hash(
	struct hlist_node *hnode,
	unsigned long     user_data
)
{
	char proto_name = 0;
	struct transfer_proto_config_node *pnode = NULL;

	if (!hnode && !user_data) {
		return ERR;
	}

	if (!hnode) 
		proto_name = ((char*)user_data)[0];
	else {
		pnode = list_entry(hnode, struct transfer_proto_config_node, node);
		if (pnode->proto_name)
			proto_name = pnode->proto_name[0];
	}

	return (proto_name % PROTO_CONFIG_SIZE);
}

static int
transfer_proto_config_hash_get(
	struct hlist_node *hnode,
	unsigned long     user_data
)
{
	struct transfer_proto_config_node *pnode = NULL;

	if (!hnode)
		return ERR;

	pnode = list_entry(hnode, struct transfer_proto_config_node, node);
	if (!pnode->proto_name && !user_data)
		return OK;
	if (!pnode->proto_name || !user_data)
		return ERR;
	if (strcmp(pnode->proto_name,(char*)user_data))
		return ERR;

	return OK;
}

static int
transfer_proto_config_hash_destroy(
	struct hlist_node *hnode
)
{	
	struct transfer_proto_config_node *pnode = NULL;

	if (!hnode)
		return OK;

	pnode = list_entry(hnode, struct transfer_proto_config_node, node);
	FREE(pnode->proto_name);
	FREE(pnode);
}

int
transfer_proto_init()
{
	int ret = 0;

	global_proto_data.proto_count = 1;
	pthread_mutex_init(&global_proto_data.mutex, NULL);
	global_proto_data.proto_hash = hash_create(
						PROTO_HASH_SIZE, 
						transfer_proto_hash, 
						transfer_proto_hash_get,
						transfer_proto_hash_destroy);

	global_proto_data.proto_config_hash = hash_create(
						PROTO_CONFIG_SIZE, 
						transfer_proto_config_hash, 
						transfer_proto_config_hash_get, 
						transfer_proto_hash_destroy);

	ret = user_cmd_add(transfer_proto_config_set);
	if (ret != OK)
		PANIC("add transfer_proto_config_set error");

	return uninit_register(transfer_proto_uninit);
}

MOD_INIT(transfer_proto_init);
