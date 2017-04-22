#ifndef _NET_CONFIG_H
#define _NET_CONFIG_H

/*
 * This module is an internal module used to manage all
 * the config procedure. Use this module we must register 
 * its open and close function of the config file. Please
 * note that we don't which config file we need to open and
 * close, these are decided by upstreams. Of course, this 
 * is just a test version, if we get a more wonderful way 
 * to dispose it, we will change it. We have registered some
 * config options that needed by this lib, at first we don't 
 * want to do it like this, but as the project going, we found
 * we have no reason to left to upstreams. Thus, we decide to 
 * integrate them into downstream. We will provide these 
 * changeless configure option in the config files when create 
 * a new project
 */

/*
 * toml type, used for node handle
 */
struct config_oper{
	/*
	 *
	 * get_config_file() -	get the config file from the upstream, 
	 *			Return the .
	 * @file_path_len: 	the length of the file path
	 * @file_path:		the string used to store the file path
	 * @real_len:		the real length of the file path
	 *
	 * in get_config_file, when pass a path len less than the 
	 * real lenght of the file path, please store the real path
	 * length in real_len, and return -1
	 *
	 * Return: 0 if successfull and -1 if not
	 
	 */
	int (*get_config_file)(int file_path_len, char *file_path, int *real_len);

	/*
	 * config_start() -	do some upstreams' preperation work before start 
	 *			the config analysis
	 */
	void (*config_start)();

	/*
	 * config_end() -	do some upstreams' end work after the config analysis
	 */
	void (*config_end)();
};

/*
 * config_oper_register() - register the config open, close
 *				function
 * 
 * It must be called at first in upstreams' init function. This
 * is useful, so we can be sure that our config operation will
 * work fine.
 *
 * Return: 0 if successful, -1 if not and specific errno will be 
 *	   set
 */
/*int
config_oper_register(
	struct config_oper *config_oper
);*/

/*
 * config_add() - add config option to the downstream procedure
 *
 * upstreams can use this function to add its configuration options
 * to the downstream. It's simple and flexible.
 *
 * Return: 0 if successful, -1 if not and specific errno will be set
 */
/*int
config_add(
	char *conf_name,
	unsigned long user_data,
	void (*config_handle)(int toml_type, char *name, char *val, unsigned long user_data)
);*/

/*
 * Some macro to simplify the config operation
 */
#define CONFIG_ADD(name, vp, func) 
	//config_add(name, (unsigned long)(vp), func)

#define FUNC_NAME(func) test_config_##func
#define CONFIG_FUNC(func) \
	 void FUNC_NAME(func)( \
			int toml_type,  \
			char *name, \
			char *val, \
			unsigned long user_data)
/*
 * CONFIG_FUNC(type) --  get the type config value
 * @tome_type:		config type :
 *				CONFIG_TOML_ROOT,
 *				CONFIG_TOML_LIST,
 *				CONFIG_TOML_TABLE,
 *				CONFIG_TOML_TABLE_ARRAY,
 *				CONFIG_TOML_NORMAL,
 *			we recommend to understand the toml sprcification
 * @name:		name of the configure option
 * @val:		the value of this configure option
 * @user_data:		the user data registered to this option
 *
 */
CONFIG_FUNC(INT);
CONFIG_FUNC(STR);
CONFIG_FUNC(SHORT);
CONFIG_FUNC(USHORT);
CONFIG_FUNC(IP);
CONFIG_FUNC(TABLE);
CONFIG_FUNC(DURATION);
CONFIG_FUNC(PROTO);
CONFIG_FUNC(DEV);
CONFIG_FUNC(ARRAY);
CONFIG_FUNC(SIZE);
	
#endif
