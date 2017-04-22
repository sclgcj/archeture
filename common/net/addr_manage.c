#include "std_comm.h"
#include "err.h"
#include "print.h"
#include "init.h"
#include "hash.h"
#include "addr_manage_private.h"

struct address_data {
	int address_count;
	pthread_mutex_t mutex;
	hash_handle_t addr_hash;
};

struct address_data_node {
	int addr_type;
	struct address_oper oper;
	struct hlist_node node;
};

#define ADDR_HASH_SIZE 8
static struct address_data global_addr_data;

static int
address_setup()
{
	return OK;
}

static int
address_uninit()
{
	return hash_destroy(global_addr_data.addr_hash);
}

int
address_add(
	struct address_oper *oper
)
{
	int ret = 0;
	struct address_data_node *dnode = NULL;

	dnode = (struct address_data_node *)calloc(1, sizeof(*dnode));
	if (!dnode) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}
	pthread_mutex_lock(&global_addr_data.mutex);
	dnode->addr_type = global_addr_data.address_count++;
	pthread_mutex_unlock(&global_addr_data.mutex);
	memcpy(&dnode->oper, oper, sizeof(*oper));

	ret = hash_add(global_addr_data.addr_hash, &dnode->node, dnode->addr_type);
	if (ret != OK)
		return ret;

	return dnode->addr_type;
}

static int
address_hash(
	struct hlist_node *hnode,
	unsigned long	  user_data
)
{
	int addr_type = 0;
	struct address_data_node *dnode = NULL;
	
	if (!hnode)
		addr_type = (int)user_data;
	else {
		dnode = list_entry(hnode, struct address_data_node, node);
		addr_type = dnode->addr_type;
	}

	return (addr_type % ADDR_HASH_SIZE);
}

int
address_length(
	int addr_type
)
{
	struct hlist_node *hnode = NULL;
	struct address_data_node *dnode = NULL;

	hnode = hash_get(global_addr_data.addr_hash, addr_type, addr_type);
	if (!hnode) {
		ERRNO_SET(NOT_REGISTER_ADDR);
		return ERR;
	}
	dnode = list_entry(hnode, struct address_data_node, node);
	if (dnode->oper.addr_length)
		return dnode->oper.addr_length();

	return OK;
}

struct sockaddr *
address_create(
	struct address *ta
)
{
	struct hlist_node *hnode = NULL;
	struct address_data_node *dnode = NULL;

	hnode = hash_get(global_addr_data.addr_hash, ta->addr_type, ta->addr_type);
	if (!hnode) {
		ERRNO_SET(NOT_REGISTER_ADDR);
		return NULL;
	}
	dnode = list_entry(hnode, struct address_data_node, node);
	if (dnode->oper.addr_create)
		return dnode->oper.addr_create(ta);

	return NULL;
}

struct address *
address_encode(
	int addr_type,
	struct sockaddr *addr
)
{
	struct hlist_node *hnode = NULL;
	struct address_data_node *dnode = NULL;

	hnode = hash_get(global_addr_data.addr_hash, addr_type, addr_type);
	if (!hnode) {
		ERRNO_SET(NOT_REGISTER_ADDR);
		return NULL;
	}
	dnode = list_entry(hnode, struct address_data_node, node);
	if (dnode->oper.addr_encode)
		return dnode->oper.addr_encode(addr_type, addr);

	return NULL;
}

void
address_decode(
	struct address *ta,
	struct sockaddr *addr
)
{
	struct hlist_node *hnode = NULL;
	struct address_data_node *dnode = NULL;

	hnode = hash_get(global_addr_data.addr_hash, ta->addr_type, ta->addr_type);
	if (!hnode) {
		ERRNO_SET(NOT_REGISTER_ADDR);
		return;
	}
	dnode = list_entry(hnode, struct address_data_node, node);
	if (dnode->oper.addr_decode)
		dnode->oper.addr_decode(ta, addr);
}

void
address_destroy(
	struct address *ta
)
{
	struct hlist_node *hnode = NULL;
	struct address_data_node *dnode = NULL;

	hnode = hash_get(global_addr_data.addr_hash, ta->addr_type, ta->addr_type);
	if (!hnode) {
		ERRNO_SET(NOT_REGISTER_ADDR);
		return;
	}
	dnode = list_entry(hnode, struct address_data_node, node);
	if (dnode->oper.addr_destroy)
		dnode->oper.addr_destroy(ta);
}

static int
address_hash_get(
	struct hlist_node *hnode,
	unsigned long	  user_data
)
{
	struct address_data_node *dnode = NULL;

	dnode = list_entry(hnode, struct address_data_node, node);
	if (dnode->addr_type == (int)user_data)
		return OK;

	return ERR;
}

static int
address_hash_destroy(
	struct hlist_node *hnode
)
{
	struct address_data_node *dnode = NULL;

	dnode = list_entry(hnode, struct address_data_node, node);

	FREE(dnode);
}

int
address_init()
{
	int ret = 0;

	memset(&global_addr_data, 0, sizeof(global_addr_data));
	pthread_mutex_init(&global_addr_data.mutex, NULL);
	global_addr_data.addr_hash = hash_create(
						ADDR_HASH_SIZE, 
						address_hash, 
						address_hash_get, 
						address_hash_destroy);

	ret = local_init_register(address_setup);
	if (ret != OK )
		return ret;

	return uninit_register(address_uninit);
}

MOD_INIT(address_init);
