#include "std_comm.h"
#include "init.h"
#include "init_module.h"

struct init_module global_init = INIT_MODULE(global_init);

int
init_test()
{
	return global_init.init_test(&global_init);
}

int
local_init_register(
	int (*init)()
)
{
	return global_init.local_init_register(&global_init, init);
}

int
local_uninit_register(
	int (*uninit)()
)
{
	return global_init.local_uninit_register(&global_init, uninit);
}

int
init_register(
	int (*init)()
)
{
	return global_init.other_init_register(&global_init, init);
}

int
uninit_register(
	int (*uninit)()
)
{
	return global_init.other_uninit_register(&global_init, uninit);
}

int
init()
{
	global_init.init_handle(&global_init);
}

int
uninit()
{
	global_init.uninit_handle(&global_init);
}
