#include "param_manage_private.h"
#include "init.h"
#include "err.h"
#include "hash.h"
#include "print.h"
#include "create_private.h"

/*
 * Here we ignore the thread-safe at first.
 * We may add it later, but we are not sure.
 */

#define PARAM_MANAGE_HASH_SIZE 26
struct param_manage_type_node {
	char *param_type;
	struct param_oper oper;
	struct hlist_node node;
};

struct param_manage_node {
	char *param_name;
	struct param_oper *oper;
	struct param *param;
	struct list_head  lnode;
	struct hlist_node node;
};

struct param_manage_data {
	int param_num;
	struct list_head lhead;
	pthread_mutex_t mutex;
	hash_handle_t param_hash;  //param name map hash
	hash_handle_t param_type_hash; //param type hash
};

static struct param_manage_data global_param_manage;

static struct param_manage_type_node *
param_manage_type_node_create(
	char *param_type,
	struct param_oper *oper
)
{
	struct param_manage_type_node *tnode = NULL;
	
	tnode = (struct param_manage_type_node *)calloc(1, sizeof(*tnode));
	if (!tnode)
		PANIC("Not enough memory for %d bytes\n", sizeof(*tnode));

	tnode->param_type = strdup(param_type);
	memcpy(&tnode->oper, oper, sizeof(*oper));

	return tnode;
}

static void
param_manage_type_node_destroy(
	struct param_manage_type_node *tnode
)
{
	FREE(tnode->param_type);
}

int
param_type_add(
	char *param_type,
	struct param_oper *oper
)
{
	struct param_manage_type_node *tnode = NULL;

	if (!oper || !param_type) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}
	if (!oper->param_copy || !oper->param_destroy || 
	    !oper->param_value_get || !oper->param_oper ||
	    !oper->param_set) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	tnode = param_manage_type_node_create(param_type, oper);

	return hash_add(
			global_param_manage.param_type_hash, 
			&tnode->node, 0);
}

static struct param_oper *
param_manage_type_oper_get(
	char *param_type
)
{
	struct hlist_node *hnode = NULL;
	struct param_manage_type_node *tnode = NULL;

	hnode = hash_get(global_param_manage.param_type_hash, 
			   (unsigned long)param_type, 
			   (unsigned long)param_type);
	if (!hnode) {
		ERRNO_SET(NO_SUCH_PARAM_TYPE);
		return NULL;
	}

	tnode = list_entry(hnode, struct param_manage_type_node, node);
	return &tnode->oper;
}

static struct param_manage_node *
param_manage_node_create(
	char *param_name,
	char *param_type,
	struct param *param
)
{
	struct param_manage_node *pm_node = NULL;

	pm_node = (struct param_manage_node*)calloc(1, sizeof(*pm_node));
	if (!pm_node)
		PANIC("Not enough memory for %d bytes\n", sizeof(*pm_node));

	pm_node->param_name = strdup(param_name);
	pm_node->oper = param_manage_type_oper_get(param_type);
	if (!pm_node) 
		goto err;

	pm_node->param = pm_node->oper->param_copy(param);

	return pm_node;
err:
	FREE(pm_node->param_name);
	FREE(pm_node);
	return NULL;
}

static void
param_manage_node_destroy(
	struct param_manage_node *pm_node
)
{
	FREE(pm_node->param_name);
	if (pm_node->oper && pm_node->oper->param_destroy)
		pm_node->oper->param_destroy(pm_node->param);
}

int
param_manage_add(
	char *param_name,
	char *param_type,
	struct param *param
)
{
	struct param_manage_node *pm_node = NULL;

	pm_node = param_manage_node_create(param_name, param_type, param);
	if (!pm_node)
		return ERR;

	pthread_mutex_lock(&global_param_manage.mutex);
	list_add_tail(&pm_node->lnode, &global_param_manage.lhead);
	global_param_manage.param_num++;
	pthread_mutex_unlock(&global_param_manage.mutex);

	return hash_add(global_param_manage.param_hash,
			   &pm_node->node, 0);
}

static struct param_manage_node *
param_manage_node_get(
	char *param_name,
	struct param_manage *pm
)
{
	hash_handle_t handle;
	struct hlist_node *hnode = NULL;	
	struct param_manage_node *pm_node = NULL;

	if (pm)
		handle = pm->param_hash;
	else
		handle = global_param_manage.param_hash;

	hnode = hash_get(handle,
			    (unsigned long)param_name, 
			    (unsigned long)param_name);
	if (!hnode) {
		ERRNO_SET(NO_SUCH_PARAMETER_SET);
		return NULL;
	}
	
	pm_node = list_entry(hnode, struct param_manage_node, node);

	return pm_node;
}

int
param_manage_set(
	char *param_name,
	struct param *param,
	struct param_manage *pm
)
{
	struct param_manage_node *pm_node = NULL;

	pm_node = param_manage_node_get(param_name, pm);
	if (!pm_node)
		return ERR;

	if (pm_node->oper && pm_node->oper->param_set) 
		pm_node->oper->param_set(param, pm_node->param);

	return OK;
}

int
param_manage_del(
	char *param_name
)
{
	struct hlist_node *hnode = NULL;

	hnode = hash_get(global_param_manage.param_hash,
			    (unsigned long)param_name, 
			    (unsigned long)param_name);
	if (!hnode) {
		ERRNO_SET(NO_SUCH_PARAMETER_SET);
		return ERR;
	}	
	return hash_del_and_destroy(global_param_manage.param_hash, 
				hnode,
				(unsigned long)param_name);
}

struct param *
param_manage_config_get(
	char *param_name,
	struct param_manage *pm
)
{
	struct param_manage_node *pm_node = NULL;

	pm_node = param_manage_node_get(param_name, pm);
	if (!pm_node)
		return NULL;
	
	return pm_node->oper->param_copy(pm_node->param);
}

char *
param_manage_value_get(
	char *param_name,
	struct param_manage *pm
)
{
	struct create_link_data *data = NULL;
	struct param_manage_node *pm_node = NULL;

	pm_node = param_manage_node_get(param_name, pm);
	if (!pm_node)
		return NULL;

	//data = list_entry((char*)user_data, struct create_link_data, data);

	//return pm_node->oper->param_value_get(pm_node->param);
}

int
param_manage_oper(
	int oper_cmd,
	char *param_name,
	struct param_manage *pm
)
{
	struct param_manage_node *pm_node = NULL;

	pm_node = param_manage_node_get(param_name, pm);
	if (!pm_node)
		return ERR;

	return pm_node->oper->param_oper(oper_cmd, pm_node->param);
}

int
param_manage_list_get(
	struct param_manage_list *plist
)
{
	int i = 0;
	struct param_manage_node *pm_node = NULL;

	if (!plist) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	pthread_mutex_lock(&global_param_manage.mutex);
	plist->param_num = global_param_manage.param_num;
	plist->param_name = (char**)calloc(plist->param_num, sizeof(char*));
	if (!plist->param_name) 
		PANIC("Not enough memory for %d bytes\n", 
					plist->param_num * sizeof(char*));
	list_for_each_entry(pm_node, (&global_param_manage.lhead), lnode) {
		plist->param_name[i] = strdup(pm_node->param_name);
		i++;
	}
	pthread_mutex_unlock(&global_param_manage.mutex);

	return OK;
}

void
param_manage_list_free(
	struct param_manage_list *plist
)
{
	int i = 0;

	if (!plist || !plist->param_name || plist->param_num <= 0)
		return;

	for (; i < plist->param_num; i++) {
		FREE(plist->param_name[i]);
	}
	FREE(plist->param_name);
}

static int
param_manage_hash(
	struct hlist_node *hnode,
	unsigned long user_data
)
{
	char name = 0;
	struct param_manage_node *pm_node = NULL;

	if (!hnode && !user_data)
		return ERR;

	if (!hnode)
		name = ((char*)user_data)[0];
	else {
		pm_node = list_entry(hnode, struct param_manage_node, node);
		if (pm_node->param_name)
			name = pm_node->param_name[0];
	}

	return (name % PARAM_MANAGE_HASH_SIZE);
}

static int
param_manage_hash_get(
	struct hlist_node *hnode,
	unsigned long user_data
)
{
	struct param_manage_node *pm_node = NULL;

	pm_node = list_entry(hnode, struct param_manage_node, node);
	if (!user_data && !pm_node->param_name)
		return OK;
	if (!user_data || !pm_node->param_name)
		return ERR;
	if (!strcmp((char*)user_data, pm_node->param_name)) 
		return OK;

	return ERR;
}

static int
param_manage_hash_destroy(
	struct hlist_node *hnode
)
{
	struct param_manage_node *pm_node = NULL;

	pm_node = list_entry(hnode, struct param_manage_node, node);
	FREE(pm_node->param_name);
	if (pm_node->oper->param_destroy && pm_node->param)
		pm_node->oper->param_destroy(pm_node->param);
	FREE(pm_node);

	return OK;
}

static int
param_manage_type_hash(
	struct hlist_node *hnode,
	unsigned long user_data
)
{
	char name = 0;
	struct param_manage_type_node *tnode = NULL;

	if (!hnode && !user_data) 
		return ERR;

	if (!hnode) 
		name = ((char*)user_data)[0];
	else {
		tnode = list_entry(hnode, struct param_manage_type_node, node);
		name = tnode->param_type[0];
	}

	return (name % PARAM_MANAGE_HASH_SIZE);
}

static int
param_manage_type_hash_get(
	struct hlist_node *hnode,
	unsigned long user_data
)
{
	struct param_manage_type_node *tnode = NULL;

	if (!user_data || !hnode)
		return ERR;

	tnode = list_entry(hnode, struct param_manage_type_node, node);
	if (!tnode->param_type)
		return ERR;
	if (!strcmp((char*)user_data, tnode->param_type))
		return OK;

	return ERR;
}

static int
param_manage_type_hash_destroy(
	struct hlist_node *hnode
)
{
	struct param_manage_type_node *tnode = NULL;

	tnode = list_entry(hnode, struct param_manage_type_node, node);
	FREE(tnode->param_type);
	FREE(tnode);

	return OK;
}

static int
param_manage_walk(
	unsigned long user_data,
	struct hlist_node *hnode,
	int *flag
)
{
	hash_handle_t handle = (hash_handle_t)user_data;
	struct param_manage_node *pm_node = NULL, *new = NULL;

	pm_node = list_entry(hnode, struct param_manage_node, node);

	new = (struct param_manage_node *)calloc(1, sizeof(*new));
	if (!new) 
		PANIC("calloc %d bytes error: %s\n", sizeof(*new), strerror(errno));

	new->param_name = strdup(pm_node->param_name);
	new->oper = pm_node->oper;
	new->param = pm_node->oper->param_copy(pm_node->param);
	return hash_add(
			handle, 
			&new->node, 
			0);
}

void
param_manage_destroy(
	struct param_manage *pm
)
{
	hash_destroy(pm->param_hash);
}

static int
param_manage_data_init(
	struct param_manage *pm_data
)
{
	int ret = OK;
	
	pm_data->param_type_hash = global_param_manage.param_type_hash;

	pm_data->param_hash = hash_create(
				        PARAM_MANAGE_HASH_SIZE,
					param_manage_hash, 
					param_manage_hash_get,
					param_manage_hash_destroy
				);
	if (pm_data->param_hash == HASH_ERR)
		return ERR;

	ret = hash_traversal((unsigned long)pm_data->param_hash, 
			  global_param_manage.param_hash, 
			  param_manage_walk);
	if (ret != OK)
		goto out;
	
	pm_data->pm_oper	= param_manage_oper;
	pm_data->pm_value_get	= param_manage_value_get;
	pm_data->pm_set		= param_manage_set;
	pm_data->pm_config_get	= param_manage_config_get;

out:
	return ret;
}

struct param_manage *
param_manage_create()
{
	int ret = 0;
	struct param_manage *pm_data = NULL;

	pm_data = (struct param_manage *)calloc(1, sizeof(*pm_data));
	if (!pm_data) 
		PANIC("calloc %d bytes error: %s\n", sizeof(*pm_data), strerror(errno));

	ret = param_manage_data_init(pm_data);
	if (ret != OK) 
		return NULL;

	return pm_data;
}

static int
param_manage_uninit()
{
	hash_destroy(global_param_manage.param_hash);
	hash_destroy(global_param_manage.param_type_hash);

	return OK;
}

int
param_manage_init()
{
	memset(&global_param_manage, 0, sizeof(global_param_manage));
	pthread_mutex_init(&global_param_manage.mutex, NULL);
	INIT_LIST_HEAD(&global_param_manage.lhead);
	global_param_manage.param_hash = hash_create(
					       PARAM_MANAGE_HASH_SIZE,
						param_manage_hash, 
						param_manage_hash_get,
						param_manage_hash_destroy);
	if (global_param_manage.param_hash == HASH_ERR)
		return ERR;
	global_param_manage.param_type_hash = hash_create(
						PARAM_MANAGE_HASH_SIZE,
						param_manage_type_hash,
						param_manage_type_hash_get, 
						param_manage_type_hash_destroy);
	if (global_param_manage.param_hash == HASH_ERR)
		return ERR;

	return param_manage_uninit(param_manage_uninit);
}

MOD_INIT(param_manage_init);
