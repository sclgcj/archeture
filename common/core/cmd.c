#include <getopt.h>

#include "std_comm.h"
#include "err.h"
#include "hash.h"
#include "cmd.h"
#include "init_private.h"
#include "print.h"
#include "cmd_private.h"

/**
 * This module should begin at the beginning of the 
 * program, so we don't take any effort to make it 
 * thread-safety. Of course, we don't need to.
 *
 */

struct cmd_node{
	char *cmd;
	int  type;
	unsigned long user_data;
	int (*cmd_handle)(char *val, unsigned long user_data);
	struct hlist_node node;
};

struct cmd_config{
	int  opt_args_len;
	char *opt_args;
	int long_option_num;
	struct option *long_option;
	hash_handle_t cmd_handle;
};

struct cmd_init_node{
	int (*cmd_init)();
	struct list_head node;
};

#define CMD_TABLE_SIZE 26

static struct cmd_config global_cmd_config;
static struct list_head global_cmd_init_list =  \
				LIST_HEAD_INIT(global_cmd_init_list);

int
user_cmd_init()
{
	int ret = OK;
	struct list_head *sl = NULL;
	struct cmd_init_node *cmd_node = NULL;

	list_for_each_entry(cmd_node, &global_cmd_init_list, node) {
		fprintf(stderr, "cmd_init\n");
		if (cmd_node->cmd_init) {
			ret = cmd_node->cmd_init();
			if (ret != OK)
				return ret;
		}
	}

	return OK;
}

int
user_cmd_add(
	int (*cmd_init)()
)
{
	struct cmd_init_node *cmd_node = NULL;

	cmd_node = (struct cmd_init_node*)calloc(1, sizeof(*cmd_node));
	if (!cmd_node) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}
	cmd_node->cmd_init = cmd_init;

	PRINT("add_node\n");
	list_add_tail(&cmd_node->node, &global_cmd_init_list);

	return OK;
}

static int
user_cmd_uninit()
{
	struct list_head *sl = global_cmd_init_list.next;
	struct cmd_init_node *cmd_node = NULL;

	while (sl != &global_cmd_init_list) {
		cmd_node = list_entry(sl, struct cmd_init_node, node);
		sl = sl->next;
		list_del_init(&cmd_node->node);
		free(cmd_node);
	}

	return OK;
}

static int
cmd_hash(
	struct hlist_node *hnode,
	unsigned long     user_data
)
{
	char cmd = 0; 
	struct cmd_node *cmd_node = NULL;

	if (!hnode && !user_data) 
		return ERR;

	if (!hnode) 
		cmd = ((char*)user_data)[0];
	else {
		cmd_node = list_entry(hnode, struct cmd_node, node);
		cmd = cmd_node->cmd[0];
	}

	return (cmd % CMD_TABLE_SIZE);
}

static int
cmd_hash_get(
	struct hlist_node *hnode,
	unsigned long     user_data
)
{
	struct cmd_node *cmd_node = NULL;

	if (!hnode || !user_data) 
		return ERR;
	cmd_node = list_entry(hnode, struct cmd_node, node);
	if (strcmp(cmd_node->cmd, (char*)user_data)) 
		return ERR;	

	return OK;
}

static int
cmd_destroy(
	struct hlist_node *hnode
)
{
	struct cmd_node *cmd_node = NULL;

	if (!hnode) 
		return ERR;

	cmd_node = list_entry(hnode, struct cmd_node, node);
	FREE(cmd_node->cmd);
	FREE(cmd_node);

	return OK;
}

static int
cmd_setup()
{
	memset(&global_cmd_config, 0, sizeof(global_cmd_config));
	global_cmd_config.cmd_handle = hash_create(
						CMD_TABLE_SIZE,
						cmd_hash,
						cmd_hash_get,
						cmd_destroy
						);
	if (global_cmd_config.cmd_handle == (HASH_ERR)) 
		return ERR;

	return OK;
}


static struct cmd_node *
cmd_node_create(
	char *cmd,
	int  type,
	int (*cmd_handle)(char *val, unsigned long user_data),
	unsigned long user_data
)
{
	int cmd_len = 0;
	struct cmd_node *ret_node = NULL;

	ret_node = (struct cmd_node*)calloc(sizeof(*ret_node), 1);
	if (!ret_node) 
		return NULL;
	cmd_len = strlen(cmd);
	ret_node->cmd = (char*)calloc(sizeof(char), cmd_len + 1);
	if (!ret_node->cmd) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return NULL;
	}
	memcpy(ret_node->cmd, cmd, cmd_len);
	ret_node->type = type;
	ret_node->cmd_handle = cmd_handle;
	ret_node->user_data = user_data;

	return ret_node;
}

static int
cmd_option_calloc(
	char *cmd,
	int  type,
	int  has_arg,
	int (*cmd_handle)(char *val, unsigned long user_data),
	unsigned long user_data
)
{
	int len = 0;
	int idx = 0;
	struct option *loption = NULL;

	idx = global_cmd_config.long_option_num;
	len = (idx + 1) * sizeof(struct option);
	global_cmd_config.long_option = 
		(struct option*)realloc((void*)global_cmd_config.long_option, len);
	if (!global_cmd_config.long_option) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}
	loption = &global_cmd_config.long_option[idx];
	len = strlen(cmd);
	loption->name = (char *)malloc(len);
	if (!loption->name) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}
	memcpy((char*)loption->name, cmd, len + 1);
	loption->has_arg = has_arg;
	global_cmd_config.long_option_num++;
//	loption->val = global_cmd_config.long_option_num++;
//	loption->flag = &loption->val;
	return OK;
}

static int
cmd_short_option_calloc(
	char *cmd 
)
{
	int len = 0, cmd_len = 0;
	char cmd_param[64] = { 0 };

	len = global_cmd_config.opt_args_len;
	cmd_len = strlen(cmd);
	global_cmd_config.opt_args_len += cmd_len + 1;
	global_cmd_config.opt_args = (char*)
		realloc(global_cmd_config.opt_args, global_cmd_config.opt_args_len);
	if (!global_cmd_config.opt_args) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}
	memset(global_cmd_config.opt_args + len, 0, cmd_len + 1);
	strcat(global_cmd_config.opt_args, cmd);
	//PRINT("opt_args = %s\n", global_cmd_config.opt_args);

	return OK;
}


int
cmd_add(
	char *cmd,
	int  type,
	int (*cmd_handle)(char *val, unsigned long user_data),
	unsigned long user_data
)
{
	int len = 0, ret = 0;
	char cmd_param[64] = { 0 };
	struct option *loption = NULL;
	struct cmd_node *cmd_node = NULL;	

	if (!cmd || !cmd_handle) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	cmd_node = cmd_node_create(cmd, type, cmd_handle, user_data);
	if (!cmd_node) 
		return ERR;

	switch (type) {
	case CMD_SHORT:
		memcpy(cmd_param, cmd, strlen(cmd));
		ret = cmd_short_option_calloc(cmd_param);
		if (ret != OK)
			return ret;
		break;
	case CMD_SHORT_PARAM:
		sprintf(cmd_param, "%s:", cmd);
		ret = cmd_short_option_calloc(cmd_param);
		if (ret != OK)
			return ret;
		break;	
	case CMD_LONG:
		ret = cmd_option_calloc(cmd, type, 0, cmd_handle, user_data);
		if (ret != OK)
			return ret;
		break;
	case CMD_LONG_PARAM:
		ret = cmd_option_calloc(cmd, type, 1, cmd_handle, user_data);
		if (ret != OK)
			return ret;
		break;	
	}

	return hash_add(global_cmd_config.cmd_handle, &cmd_node->node, 0);
}

static int
cmd_hash_node_execute(
	char *opt_arg,
	char *cmd_name
)
{
	int ret = 0;
	struct hlist_node *hnode = NULL;
	struct cmd_node *cmd_node = NULL;

	hnode = hash_get(
		global_cmd_config.cmd_handle, 
		(unsigned long)cmd_name,
		(unsigned long)cmd_name);
	if (!hnode) {
		//GINFO("not register such cmd : %s\n", cmd_name);
		return ERR;
	}
	cmd_node = list_entry(hnode, struct cmd_node, node);
	if (cmd_node->cmd_handle) {
		ret = cmd_node->cmd_handle(opt_arg, cmd_node->user_data);
		if (ret != OK) {
		//	GINFO("cmd_handle error\n");
			return ERR;
		}
	} 

	return OK;
}

int
cmd_handle(
	int argc,
	char **argv
) 
{
	int c = 0;
	int ret = 0;
	int opt_idx = 0;	
	char cmd_name[64] = { 0 };
	struct option *long_option = global_cmd_config.long_option;

	if (argc < 2)
		return OK;

	if (!global_cmd_config.opt_args && !long_option) {
		//GINFO("no options\n");
		return OK;
	}

	//if (global_cmd_config.opt_args) 
	//	GINFO( "opt_arts = %s\n", global_cmd_config.opt_args);
	while (c != -1)	 {
		opt_idx = 0;
		c = getopt_long(argc, argv, global_cmd_config.opt_args ,
				long_option, &opt_idx);
		if (c == -1) {
			continue;
		}
		if (c == 0)
			//PRINT("long_option_num = %d,idx = %d\n", global_cmd_config.long_option_num, opt_idx);
			memcpy(
				cmd_name, 
				long_option[opt_idx].name, 
				strlen(long_option[opt_idx].name));
		else 
			sprintf(cmd_name, "%c", (char)c);
			
		ret = cmd_hash_node_execute(optarg, cmd_name);
		if (ret != OK)
			return ret;
	}

	return OK;
}


int
cmd_uninit()
{
	int i = 0;

	hash_destroy(global_cmd_config.cmd_handle);
	if (global_cmd_config.opt_args) {
		free(global_cmd_config.opt_args);
		global_cmd_config.opt_args = NULL;
	}
	for (; i < global_cmd_config.long_option_num; i++) {
		free((void*)global_cmd_config.long_option[i].name);
		global_cmd_config.long_option[i].name = NULL;
	}
	free(global_cmd_config.long_option);

	user_cmd_uninit();

	return OK;
}

int
cmd_init()
{
	int ret = 0;

	ret = cmd_setup();
	if (ret != OK)
		return ret;
	/*ret = local_init_register(cmd_setup);
	if (ret != OK) 
		return ret;*/

	return uninit_register(cmd_uninit);
}

MOD_INIT(cmd_init);
