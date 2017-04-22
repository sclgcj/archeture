#include "number_hash.h"
#include "err.h"
#include "hash.h"

struct number_node {
	int id;
	unsigned long user_data;
	void *jnumber;
	struct hlist_node node;
};

struct number_hash_param {
	int id;
	int total;
	unsigned long user_data;
	int (*jn_walk)(unsigned long jn_data, unsigned long walk_data);
};

struct number {
	int total;
	hash_handle_t handle;
	int (*number_get)(unsigned long user_data, unsigned long hash_data);
	int (*number_destroy)(unsigned long user_data);
};

static int
number_hash(
	struct hlist_node *hnode,
	unsigned long user_data
) 
{
	int id = 0;
	struct number_node *jnn = NULL;
	struct number_hash_param *param = NULL;

	param = (typeof(param))user_data;
	if (!hnode)
		id = (unsigned long)param->id;
	else {
		jnn = list_entry(hnode, typeof(*jnn), node);
		id = jnn->id;
	}

	return (id % param->total);
}

static int
number_hash_get(
	struct hlist_node *hnode,
	unsigned long user_data
)
{
	struct number *jn = NULL;
	struct number_node *jnn = NULL;
	struct number_hash_param *param = NULL;

	param = (typeof(param))user_data;
	jnn = list_entry(hnode, typeof(*jnn), node);
	if ((int)param->id != jnn->id) 
		return ERR;
	jn = (typeof(jn))jnn->jnumber;
	if (jn->number_get) 
		return jn->number_get(jnn->user_data, param->user_data);

	return OK;
}

static int
number_hash_destroy(
	struct hlist_node *hnode
)
{
	int ret = 0;
	struct number *jn = NULL;
	struct number_node *jnn = NULL;

	jnn = list_entry(hnode, typeof(*jnn), node);
	jn = (typeof(jn))jnn->jnumber;
	if (jn->number_destroy) 
		ret = jn->number_destroy(jnn->user_data);
	free(jn);
	return ret;
}

number_t
number_create(
	int total,
	int (*number_get)(unsigned long user_data, unsigned long cmp_data),
	int (*number_destroy)(unsigned long user_data)
)
{
	struct number *jn = NULL;

	jn = (typeof(jn))calloc(1, sizeof(*jn));
	if (!jn) {
		fprintf(stderr, "calloc %d bytes error: %s\n", 
				sizeof(*jn), strerror(errno));
		exit(0);
	}
	jn->total = total;
	jn->number_get  = number_get;
	jn->number_destroy = number_destroy;

	jn->handle = hash_create(
				jn->total, 
				number_hash, 
				number_hash_get,
				number_hash_destroy);

	return (number_t)jn;
}

int
number_add(
	int id,	
	unsigned long user_data,
	number_t jnumber
)
{
	struct number *jn = NULL;
	struct number_node *jnn = NULL;
	struct number_hash_param param;

	jnn = (typeof(jnn))calloc(1, sizeof(*jnn));
	if (!jnn) {
		fprintf(stderr, "calloc %d bytes error: %s\n", 
				sizeof(*jnn), strerror(errno));
		exit(0);
	}
	jnn->jnumber = jnumber;
	jnn->id = id;
	jnn->user_data = user_data;

	jn = (typeof(jn))jnumber;
	memset(&param, 0, sizeof(param));
	param.id = id;
	param.user_data = 0;
	param.total = jn->total;
	
	return hash_add(jn->handle,
			   &jnn->node,
			   (unsigned long)&param);
}

unsigned long
number_get(
	int id,
	unsigned long cmp_data,
	number_t jnumber
)
{
	struct number *jn = NULL;	
	struct hlist_node *hnode = NULL;
	struct number_node *jnn = NULL;
	struct number_hash_param param;

	jn = (typeof(jn))jnumber;
	memset(&param, 0, sizeof(param));
	param.user_data = cmp_data;
	param.id = id;
	param.total = jn->total;

	hnode = hash_get(jn->handle, 
			    (unsigned long)&param, 
			    (unsigned long)&param);
	if (!hnode) {
		fprintf(stderr, "no id = %d \n", id);
		return (unsigned long)ERR;
	}
	jnn = list_entry(hnode, typeof(*jnn), node);

	return jnn->user_data;
}

static int
number_walk(
	unsigned long user_data,
	struct hlist_node *hnode,
	int *flag
)
{
	struct number *jn = NULL;
	struct number_node *jnn = NULL;
	struct number_hash_param *param = NULL;

	(*flag) = 0;
	jnn = list_entry(hnode, typeof(*jnn), node);
	jn = (typeof(jn))jnn->jnumber;
	param = (typeof(param))user_data;
	if (param->jn_walk)
		return param->jn_walk(jnn->user_data, 
					   param->user_data);
	return OK;
}

int
number_traversal(
	unsigned long user_data,
	number_t jnumber,
	int (*jn_traversal)(unsigned long jn_data, unsigned long data)
)
{
	struct number *jn = NULL;	
	struct number_hash_param param;

	if (!jnumber) 
		return ERR;

	jn = (typeof(jn))jnumber;
	memset(&param, 0, sizeof(param));
	param.jn_walk = jn_traversal;
	param.user_data = user_data;

	return hash_traversal((unsigned long)&param,
				 jn->handle,
				 number_walk);
}

int
number_destroy(
	number_t jnumber
)
{
	struct number *jn = NULL;

	if (!jnumber)
		return OK;

	if (jn->handle)  {
		hash_destroy(jn->handle);
		free(jn->handle);
	}
	
	free(jnumber);
}

