#include "std_comm.h"
#include "err.h"
#include "print.h"
#include "cmd.h"
#include "init_private.h"
#include "config_read.h"

int 
main(
	int argc,
	char **argv
)
{
	int ret = 0;	
	char buf[1024] = { 0 };

	//init user cmd 
	PRINT("\n");
	ret = user_cmd_init();
	if (ret != OK)
		PANIC("cmd_init error: %s\n", CUR_ERRMSG_GET());

	PRINT("\n");
	//handle console cmd
	ret = cmd_handle(argc, argv);
	if (ret != OK) 
		PANIC("cmd_handle error: %s\n", CUR_ERRMSG_GET());

	PRINT("\n");
	//read config
	ret =config_read_handle();
	if (ret != OK)
		PANIC("config_read_handle error:%s\n", CUR_ERRMSG_GET());

	PRINT("\n");
	//init lib module and user module
	ret = init();
	if (ret != OK) {
		PRINT("init -errr: %s\n", CUR_ERRMSG_GET());
	}
	
	//execute user defined function
	//ret = execute();
	//start epoll
//	epoll_start();

	uninit();

	return 0;
}
