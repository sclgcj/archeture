#include "letter_hash.h"
#include "err.h"

#define LETTER_HASH_SIZE 26

struct letter_node {
	char *name;
	letter_t letter;
	unsigned long data;
	struct hlist_node node;
};

struct letter {
	int (*letter_destroy)(unsigned long data);
	int (*letter_hash)(unsigned long user_data, unsigned long hash_data);
	int (*letter_get)(unsigned long user_data, unsigned long hash_data);
	hash_handle_t handle;
};

struct letter_traversal_param {
	unsigned long user_data;
	int (*letter_traversal)(unsigned long jl_data, unsigned long data);
};

static int
letter_hash(
	struct hlist_node *hnode,
	unsigned long user_data
)
{
	char name = 0;
	struct letter_node *jlh = NULL;
	
	if (!hnode && user_data) 
		name = ((char*)user_data)[0];
	else if (!user_data)
		name = 0;
	else {
		jlh = list_entry(hnode, typeof(*jlh), node);
		if (jlh->name)
			name = jlh->name[0];
	}

	return (name % LETTER_HASH_SIZE);
}

static int 
letter_hash_get(
	struct hlist_node *hnode,
	unsigned long user_data
)
{
	int flag = 0;
	struct letter *jl = NULL;
	struct letter_node *jln = NULL;
	struct letter_param *param = NULL;

	param = (struct letter_param *)user_data;
	jln = list_entry(hnode, typeof(*jln), node);

	if (!param->name && !jln->name)
		flag = 1;
	if (!param->name || !jln->name)
		flag = 0;
	if (!strncmp((char*)param->name, jln->name, strlen(jln->name)))
		flag = 1;

	if (flag) {
		jl = jln->letter;
		if (jl->letter_get)
			return jl->letter_get(jln->data, param->user_data);
		else
			return OK;
	}

	return ERR;
}

static int
letter_hash_destroy(
	struct hlist_node *hnode
)
{
	struct letter *jl = NULL;
	struct letter_node *jln = NULL;

	jln = list_entry(hnode, typeof(*jln), node);
	if (jln->name)
		free(jln->name);
	jl = (struct letter*)jln->letter;
	if (jl->letter_destroy)
		jl->letter_destroy(jln->data);
	free(jln);

	return OK;
}

letter_t
letter_create(
	int (*letter_hash_func)(unsigned long user_data, unsigned long hash_data),
	int (*letter_get)(unsigned long user_data, unsigned long cmp_data),
	int (*letter_destroy)(unsigned long data)
)
{
	struct letter *jl = NULL;

	jl = (typeof(*jl)*)calloc(1, sizeof(*jl));
	if (!jl) {
		fprintf(stderr, "calloc %d bytes error: %s\n",
				sizeof(*jl), strerror(errno));
		exit(0);
	}
	jl->handle = hash_create(
			LETTER_HASH_SIZE, 
			letter_hash, 
			letter_hash_get, 
			letter_hash_destroy);
	if (jl->handle == HASH_ERR) 
		return NULL;
	jl->letter_destroy = letter_destroy;
	jl->letter_get = letter_get;
	jl->letter_hash = letter_hash_func;

	return (letter_t)jl;
}

int
letter_add(
	char *name,
	unsigned long user_data,
	letter_t letter
)
{
	struct letter *jl = NULL;
	struct letter_node *jln = NULL;

	if (!jl)
		return ERR;

	jln = (typeof(*jln)*)calloc(1, sizeof(*jln));
	if (!jln) {
		fprintf(stderr, "calloc %d bytes error: %s\n", 
				sizeof(*jln), strerror(errno));
		exit(0);
	}
	if (name)
		jln->name = strdup(name);
	jln->data = user_data;
	jln->letter = letter;
	
	jl = (struct letter*)letter;
	return hash_add(jl->handle, 
			   &jln->node, 
			   (unsigned long)jl);
}

unsigned long
letter_get(
	struct letter_param *jlp,
	letter_t letter
)
{
	struct letter *jl = NULL;
	struct hlist_node *hnode = NULL;
	struct letter_node *jln = NULL;

	if (!letter)
		return (unsigned long)ERR;
	jl = (typeof(jl))letter;
	hnode = hash_get(jl->handle, 
			    (unsigned long)jlp->name, 
			    (unsigned long)jlp);
	if (!hnode) {
		fprintf(stderr, "no node named %s\n", jlp->name);
		return (unsigned long)ERR;
	}
	jln = list_entry(hnode, typeof(*jln), node);

	return jln->data;
}

int
letter_destroy(
	letter_t letter
)
{
	int ret = 0;
	struct letter *jl = NULL;

	if (!letter) return OK;

	jl = (struct letter*)letter;
	ret = hash_destroy(jl->handle);
	free(jl);

	return ret;
}

static int
letter_walk(
	unsigned long user_data,
	struct hlist_node *hnode,	
	int *flag
)
{
	struct letter_node *jln = NULL;
	struct letter_traversal_param *param = NULL;

	jln = list_entry(hnode, typeof(*jln), node);
	param = (typeof(param))user_data;
	if (param->letter_traversal)
		return param->letter_traversal(jln->data, param->user_data);

	return OK;
}

int
letter_traversal(
	unsigned long user_data,
	letter_t letter,
	int (*jl_traversal)(unsigned long jl_data, unsigned long data)
)
{
	struct letter *jl = NULL;
	struct letter_traversal_param param;

	if (!letter)
		return ERR;

	jl = (typeof(jl))letter;
	memset(&param, 0, sizeof(param));
	param.user_data = user_data;
	param.letter_traversal = jl_traversal;
	
	return hash_traversal((unsigned long)&param, 
				 jl->handle, 
				 letter_walk);
}

