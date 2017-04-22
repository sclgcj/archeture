#ifndef JSON_H
#define JSON_H 1

#include "std_comm.h"

/*
 * This module is used to build a commom function for 
 * json check. We read constant json data from a speicfied
 * file and use it as base to check if the response json 
 * string is right. The goal we want to achieve is that no 
 * matter how the json data changes, we don't need to 
 * change our codes and just change the config file. Of
 * course, this module will be used by api test and it 
 * is based on the api configuration
 */
typedef int (*json_node_handle_func)(
				cJSON *input_node, 
				unsigned long data, 
				unsigned long user_data);
typedef int (*node_handle_func)(
			int depth, 
			cJSON *input, 
			unsigned long *out, 
			json_node_handle_func json_node_handle, 
			unsigned long user_data);

int
json_to_param(
	int depth, 
	cJSON *input_node,
	unsigned long *out,
	json_node_handle_func json_node_handle,
	unsigned long user_data
);

int
json_to_json_param(
	int depth, 
	cJSON *input_node,
	unsigned long *out,
	json_node_handle_func json_node_handle,
	unsigned long user_data
);

int
json_node_check(
	int depth, 
	cJSON *input_node,
	unsigned long *out,
	json_node_handle_func json_node_handle,
	unsigned long user_data
);

int
json_node_default_param(
	cJSON *input_node,
	unsigned long out_data,
	unsigned long user_data
);

int 
interface_json_walk(
	char *interface,
	char *interface_path,
	unsigned long *param, 
	node_handle_func node_handle,
	json_node_handle_func json_node_handle,
	unsigned long user_data
);

int
interface_json_walk_new(	
	char *interface,
	char *interface_path,
	unsigned long *param, 
	node_handle_func node_handle,
	unsigned long user_data
);

int
json_node_param_register(
	char *input_val,
	json_node_handle_func node_handle
);

#endif
