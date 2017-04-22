#include "json.h"
#include "err.h"
#include "init.h"
#include "print.h"
#include "hash.h"
#include "config_read.h"
//#include "global_log_private.h"
//#include "interface_private.h"


#define JSON_TABLE_SIZE 26
struct json_node {
	char *val_name;
	json_node_handle_func func;
	struct hlist_node node;
};

struct json_data {
	hash_handle_t json_hash;
};

static struct json_data global_json_data;

static int
walk_json(
	int depth,
	cJSON *root,
	unsigned long out_data,
	node_handle_func node_handle,
	json_node_handle_func json_node_handle,
	unsigned long user_data
);

static int
walk_object(
	int depth,
	cJSON *root,
	unsigned long out_data,
	node_handle_func node_handle,
	json_node_handle_func json_node_handle,
	unsigned long user_data
);

static int
json_check(
	cJSON *input,
	cJSON *req,
	unsigned long data
);

static int
json_node_val_get(
	cJSON *input,	
	unsigned long out,
	unsigned long user_data
)
{
	struct hlist_node *hnode = NULL;
	struct json_node *json_node = NULL;

	if (input->type == cJSON_NULL)
		return OK;
	
	if (input->type == cJSON_String)
		hnode = hash_get(
			global_json_data.json_hash, 
			(unsigned long)input->valuestring, 
			(unsigned long)input->valuestring);
	if (!hnode) 
		return json_node_default_param(input, out, user_data);
	json_node = list_entry(hnode, struct json_node, node);
	if (json_node->func) 
		return json_node->func(input, out, user_data);
	else
		return json_node_default_param(input, out, user_data);
}

static int
json_node_val_check(
	cJSON *input,
	unsigned long req,
	unsigned long user_data
)
{
	int ret = 0;
	struct hlist_node *hnode = NULL;
	struct json_node *json_node = NULL;

	if (input->valuestring)
		hnode = hash_get(
			global_json_data.json_hash, 
			(unsigned long)input->valuestring, 
			(unsigned long)input->valuestring);
	if (!hnode)
		return json_check(input, (cJSON*)req, user_data);

	json_node = list_entry(hnode, struct json_node, node);
	if (json_node->func) {
		ret = json_node->func(input, req, user_data);
		if (ret == ERR) 
			ERRNO_SET(WRONG_JSON_DATA);
		else if (ret != OK) 
			ERRNO_SET(ret);
	}

	return ret;
}

int 
json_to_param(
	int depth,
	cJSON *input_node,
	unsigned long *out,
	json_node_handle_func json_node_handle,
	unsigned long user_data
)
{
	char val_pair[256] = { 0 };
	char *param = (char*)(*out);

	if (!input_node || !out) 
		return ERR;

	if (input_node->type == cJSON_NULL)
		return OK;

	if (json_node_handle) 
		json_node_handle(input_node, (unsigned long)param, user_data);
	else {
		sprintf(val_pair, "&%s=%s", input_node->string, input_node->valuestring);
		strcat(param, val_pair);
	}

	return OK;
}

int
json_node_default_param(
	cJSON *input_node,
	unsigned long out_data,
	unsigned long user_data
)
{
	cJSON *out = (cJSON *)out_data;
	cJSON *node = NULL;

	switch (input_node->type) {
	case cJSON_True:
	case cJSON_False:
	case cJSON_Number:
		if (input_node->valuedouble)
			node = cJSON_CreateNumber(input_node->valuedouble);
		else 
			node = cJSON_CreateNumber(input_node->valueint);
		break;
	case cJSON_String:
		node = cJSON_CreateString(input_node->valuestring);
		break;
	default:
		return OK;
	}
	if (node) {
		if (out->type == cJSON_Object)
			cJSON_AddItemToObject(out, input_node->string, node);
		else if (out->type == cJSON_Array)
			cJSON_AddItemToArray(out, node);
	}

	return OK;
}

int
json_to_json_param(
	int depth,
	cJSON *input_node,
	unsigned long *out_data,
	json_node_handle_func json_node_handle,
	unsigned long user_data
)
{
	int ret = OK;
	cJSON *node = NULL;
	cJSON *data = (cJSON*)(*out_data);

	switch(input_node->type) {
	case cJSON_NULL:
		break;
	case cJSON_Array:
		node = cJSON_CreateArray();
		if (!node) {
			ERRNO_SET(NOT_ENOUGH_MEMORY);
			return ERR;
		}
		ret = walk_object(
				0,
				input_node, 
				(unsigned long)node, 
				json_to_json_param, 
				json_node_handle, 
				user_data);
		if (ret == OK) {
			if (data->type == cJSON_Array)
				cJSON_AddItemToArray(data, node);
			else 
				cJSON_AddItemToObject(data, input_node->string, node);
		}
		break;
	case cJSON_Object:
		node = cJSON_CreateObject();
		if (!node) {
			ERRNO_SET(NOT_ENOUGH_MEMORY);
			return ERR;
		}
		ret = walk_object(
				0, 
				input_node, 
				(unsigned long)node,
				json_to_json_param,
				json_node_handle,
				user_data);
		if (ret == OK) {
			if (data->type == cJSON_Array)
				cJSON_AddItemToArray(data, node);
			else
				cJSON_AddItemToObject(data, input_node->string, node);
		}
		break;
	default:
		if (json_node_handle) 
			ret = json_node_handle(input_node, (*out_data), user_data);
		else
			ret = json_node_default_param(
						input_node,
						(*out_data),
						user_data);
	}
out:
	return ret;
}

static int
json_array_check(
	int depth,
	cJSON *input,
	cJSON *resp,
	json_node_handle_func json_node_handle,
	unsigned long user_data
)
{
	int i = 0;
	int ret = 0;
	int array_size = 0;
	unsigned long resp_node = 0;
	cJSON *input_node = NULL;

	array_size = cJSON_GetArraySize(resp);
	for (; i < array_size; i++) {
		resp_node = (unsigned long)cJSON_GetArrayItem(resp, i);
		input_node = cJSON_GetArrayItem(input, i);
		ret = walk_json(
				1, 
				input_node, 
				resp_node, 
				json_node_check, 
				json_node_handle, 
				user_data);
		if (ret != OK)
			return ret;
	}

	return OK;
}

static int
json_check(
	cJSON *input,
	cJSON *req,
	unsigned long data
)
{
	switch (req->type & 255) {
	case cJSON_String:
		if (!strcmp(input->valuestring, req->valuestring)) 
			return OK;
		else 
			PRINT("%s value is not the same, req = %s, input = %s\n", 
					input->string, req->valuestring, input->valuestring);
		break;
	case cJSON_Number:
	case cJSON_False:
	case cJSON_True:
		if (input->type == cJSON_String) {
			if (req->valueint == atoi(input->valuestring) ||
					(int)req->valuedouble == atoi(input->valuestring))
				return OK;
			else 
				PRINT("%s value is not the same, req = %s,%lf, input = %s\n", 
					input->string, req->valuestring, 
					req->valuedouble, input->valuestring);
		} else {
			if (req->valueint == input->valueint || 
					(int)req->valuedouble == input->valueint)
				return OK;
			else 
				PRINT("%s value is not the same, req = %s,%f, input = %d\n", 
					input->string, req->valuestring, 
					req->valuedouble, input->valueint);
		}

		break;
	default:
		break;
	}

	ERRNO_SET(WRONG_JSON_DATA);
	return ERR;
}

int
json_node_check(
	int depth,
	cJSON *input_data,
	unsigned long *param,
	json_node_handle_func json_node_handle,
	unsigned long user_data
)
{
	int ret = OK;
	cJSON *root = (cJSON *)(*param);
	cJSON *node = NULL;

	if (depth == 0 || input_data->string == NULL)
		node = root;
	else {
		if (root->type != cJSON_Object)
			node = (cJSON*)(*param);
		else {
			node = cJSON_GetObjectItem(root, input_data->string);
			if (!node) {
				PRINT("no element named %s\n", input_data->string);
				ERRNO_SET(WRONG_JSON_DATA);
				return ERR;
			}
		}
	}
	if (json_node_handle) {
		ret = json_node_handle(input_data, (unsigned long)node, user_data);
		if (ret != OK)
			return ret;
	}
	if (input_data->type == cJSON_NULL)
		return OK;
	switch (input_data->type & 255) {
	case cJSON_Array:
		if (input_data->type != node->type) {
			ERRNO_SET(WRONG_JSON_DATA_TYPE);
			return ERR;
		}
		ret = json_array_check(
					depth, 
					input_data, 
					node, 
					json_node_handle, 
					user_data);
		break;
	case cJSON_Object:
		if (input_data->type != node->type) {
			ERRNO_SET(WRONG_JSON_DATA_TYPE);
			return ERR;
		}
		ret = walk_object(
				0, 
				input_data, 
				(unsigned long)node, 
				json_node_check, 
				json_node_handle, 
				user_data);
		break;
	default:
		ret = json_node_val_check(input_data, (unsigned long)node, user_data);
		//ret = json_check(input_data, node, user_data);
	}

	return ret;
}

static int
walk_object(
	int depth,
	cJSON *root,
	unsigned long out_data,
	node_handle_func node_handle,
	json_node_handle_func json_node_handle,
	unsigned long user_data
)
{
	int ret = 0;
	int numentries = 0;
	cJSON *child = root->child;

	while (child) numentries++, child = child->next;

	if (!numentries)
		return OK;
	child = root->child;
	depth++;
	while (child) {
		ret = walk_json(
				depth, 
				child, 
				out_data, 
				node_handle, 
				json_node_handle, 
				user_data);
		if (ret != OK)
			return ret;

		child = child->next;
	}

	return OK;
}

static int 
walk_json(
	int depth,
	cJSON *root,
	unsigned long out_data,
	node_handle_func node_handle,
	json_node_handle_func json_node_handle,
	unsigned long user_data
)
{
	int ret = 0;
	unsigned long cur_data = out_data;

	if (!root) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	switch (root->type & 255) {
	case cJSON_Array:
		if (node_handle) 
			ret = node_handle(depth, root, &cur_data, json_node_handle, user_data);
		break;
	case cJSON_Object:
		if (node_handle && depth != 0) 
			ret = node_handle(depth, root, &cur_data, json_node_handle, user_data);
		else if (node_handle && depth == 0)
			ret = walk_object(
					depth, 
					root, 
					cur_data, 
					node_handle, 
					json_node_handle, 
					user_data);
		break;
	default:	
		if (node_handle && depth != 0) 
			ret = node_handle(depth, root, &cur_data, json_node_handle, user_data);
		break;
	}

	return ret;
}

int
get_file_json(
	char *file,
	unsigned long user_data,
	cJSON **json
)
{
	char *buf = NULL;
	FILE *fp = NULL;
	struct stat st_buf;
	cJSON *root = NULL;

	memset(&st_buf, 0, sizeof(st_buf));
	stat(file, &st_buf);
	if (st_buf.st_size <= 0) {
		ERRNO_SET(WRONG_JSON_FILE);
		return ERR;
	}
	buf = (char*)calloc(st_buf.st_size + 1, sizeof(char));
	if (!buf) 
		PANIC("Not enough memory for %lu bytes\n", st_buf.st_size + 1);
	fp = fopen(file, "r");
	if (!fp) {
		FREE(buf);
		PANIC("open file %s error: %s\n", file, strerror(errno));
	}
	fread(buf, 1, st_buf.st_size, fp);
	fclose(fp);

	(*json) = cJSON_Parse(buf);
	if (!(*json)) {
		ERRNO_SET(WRONG_JSON_DATA);
		FREE(buf);
		return ERR;
	}
	FREE(buf);
	return OK;
}

int
interface_json_walk_new(	
	char *interface,
	char *interface_path,
	unsigned long *param, 
	node_handle_func node_handle,
	unsigned long user_data
)
{
	int ret = 0;
	char *input_path = NULL;
	cJSON *input_data = NULL;
	cJSON *input_param = NULL;

	input_param = config_read_get(interface);
	//input_param = interface_param_get(interface);
	if (!input_param) {
		ERRNO_SET(NO_INTERFACE_SET);
		return ERR;
	}
	input_data = cJSON_GetObjectItem(input_param, interface_path);
	if (!input_data) {
		ERRNO_SET(NO_INTERFACE_PATH);
		return ERR;
	}
	input_path = input_data->valuestring;
	if (!input_path) {
		ERRNO_SET(NO_INTERFACE_PATH);
		return ERR;
	}
	ret = get_file_json(input_path, user_data, &input_data);
	if (ret != OK || !input_data){
		return ERR;
	}

	if (!(*param)) {
		if (input_data->type == cJSON_Array)
			(*param) = (unsigned long)cJSON_CreateArray();
		else 
			(*param) = (unsigned long)cJSON_CreateObject();
	}

	ret = walk_json(
			0,
			input_data, 
			*param, 
			node_handle,
			json_node_val_get,
			user_data);

	cJSON_Delete(input_data);
	return ret;
}

 
int
interface_json_walk(
	char *interface,
	char *interface_path,
	unsigned long *param, 
	node_handle_func node_handle,
	json_node_handle_func json_node_handle,
	unsigned long user_data
)
{
	int ret = 0;
	char *input_path = NULL;
	cJSON *input_data = NULL;
	cJSON *input_param = NULL;

	input_param = config_read_get(interface);
	//input_param = interface_param_get(interface);
	if (!input_param) {
		ERRNO_SET(NO_INTERFACE_SET);
		return ERR;
	}
	input_data = cJSON_GetObjectItem(input_param, interface_path);
	if (!input_data) {
		ERRNO_SET(NO_INTERFACE_PATH);
		return ERR;
	}
	input_path = input_data->valuestring;
	if (!input_path) {
		ERRNO_SET(NO_INTERFACE_PATH);
		return ERR;
	}
	ret = get_file_json(input_path, user_data, &input_data);
	if (ret != OK || !input_data) {
		ERRNO_SET(WRONG_JSON_FILE);
		return ERR;
	}

	if (!(*param)) {
		if (input_data->type == cJSON_Array)
			(*param) = (unsigned long)cJSON_CreateArray();
		else 
			(*param) = (unsigned long)cJSON_CreateObject();
	}

	ret = walk_json(
			0,
			input_data, 
			*param, 
			node_handle,
			json_node_handle,
			user_data);
	if (ret != OK)
		return ret;

	cJSON_Delete(input_data);
	return ret;
}

int
json_node_param_register(
	char *input_val,
	json_node_handle_func node_handle
)
{
	int len = 0;
	struct hlist_node *hnode = NULL;
	struct json_node *json_node = NULL;

	json_node = (struct json_node *)calloc(1, sizeof(*json_node));
	if (!json_node) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}
	json_node->func = node_handle;
	if (input_val) {
		len = strlen(input_val);
		json_node->val_name = (char*)calloc(1, len + 1);
		if (!json_node->val_name) {
			FREE(json_node);
			ERRNO_SET(NOT_ENOUGH_MEMORY);
			return ERR;
		}
		memcpy(json_node->val_name, input_val, len);
	}

	hash_get(global_json_data.json_hash, 
		    (unsigned long)input_val, 
		    (unsigned long)input_val);

	return hash_add(global_json_data.json_hash, &json_node->node, 0);
}

static int
json_uninit()
{
	return hash_destroy(global_json_data.json_hash);
}

static int
json_hash(
	struct hlist_node *hnode,		
	unsigned long user_data
)
{
	char cmd = 0;	
	struct json_node *json_node = NULL;
	
	if (!hnode && !user_data)
		cmd = 0;
	else if (!hnode)
		cmd = ((char*)user_data)[2];
	else {
		json_node = list_entry(hnode, struct json_node, node);
		if (json_node->val_name)
			cmd = json_node->val_name[2];
		else 
			cmd = 0;
	}

	return (cmd % JSON_TABLE_SIZE);
}

static int
json_hash_get(
	struct hlist_node *hnode,
	unsigned long user_data
)
{
	struct json_node *json_node = NULL;

	if (!hnode)
		return ERR;

	json_node = list_entry(hnode, struct json_node, node);
	if (!json_node->val_name && !user_data)
		return OK;
	if (!strcmp(json_node->val_name, (char*)user_data)) 
		return OK;

	return ERR;
}

static int
json_hash_destroy(
	struct hlist_node *hnode
)
{
	struct json_node *json_node = NULL;

	if (!hnode)
		return OK;

	json_node = list_entry(hnode, struct json_node, node);
	FREE(json_node->val_name);
	FREE(json_node);

	return OK;
}

int
json_init()
{
	memset(&global_json_data, 0, sizeof(global_json_data));

	global_json_data.json_hash = hash_create(
						JSON_TABLE_SIZE,
						json_hash, 
						json_hash_get, 
						json_hash_destroy);
	return uninit_register(json_uninit);
}

MOD_INIT(json_init);

