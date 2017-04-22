#ifndef CMD_H
#define CMD_H 1

enum {
	CMD_SHORT,			//single dash('-') with optional param
	CMD_SHORT_PARAM,		//signle dash('-') which must have a param
	CMD_LONG,			//double dash('--') with optional param
	CMD_LONG_PARAM,		//double dash('--') which mutes have a param
	CMD_MAX
};

/**
 * cmd_add() - used to add cmd opt to the downstream
 * @cmd:	the option command
 */

int
cmd_add(
	char *cmd, 
	int type, 
	int (*cmd_handle)(char *, unsigned long),
	unsigned long user_data);

/**
 * user_cmd_add() - used to add user defined init function
 *
 * Return: 0 if successful, -1 if failed and the errno will 
 *	   be set
 */
int
user_cmd_add(
	int (*cmd_init)()
);

/**
 * cmd_handle() - dispose the console command
 * @argc:	the number of the console command
 * @argv:	the console command array
 *
 * Return: 0 if successful, -1 if failed and the errno will
 *	   be set
 */
int
cmd_handle(
	int argc,
	char **argv
);

/**
 * user_cmd_init() - init user added cmd
 *
 * Return: 0 if successful, -1 if failed and the errno will 
 *         be set	
 */
int
user_cmd_init();



#endif
