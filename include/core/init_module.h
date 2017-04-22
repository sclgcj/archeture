#ifndef INIT_MODULE_H
#define INIT_MODULE_H

struct init_module {
	int init_flag;
	int uninit_flag;
	pthread_mutex_t mutex;
	struct list_head init_list;
	struct list_head uninit_list;
	struct list_head local_init_list;
	struct list_head local_uninit_list;
	
	pthread_mutex_t init_mutex;
	pthread_mutex_t uninit_mutex;
	pthread_mutex_t local_init_mutex;
	pthread_mutex_t local_uninit_mutex;

	int (*other_init_register)(struct init_module *init_module, int (*init)());
	int (*other_uninit_register)(struct init_module *init_module, int (*uninit)());
	int (*local_init_register)(struct init_module *init_module, int (*init)());
	int (*local_uninit_register)(struct init_module *init_module, int (*uninit)());
	int (*init_handle)(struct init_module *init_module);
	int (*uninit_handle)(struct init_module *init_module);
	int (*init_test)(struct init_module *init_module);
};

struct init_module *
init_module_create();

void
init_module_destroy(
	struct init_module *mod
);

int
init_module_local_init_register(
	struct init_module *mod,
	int (*init)()
);

int
init_module_init_register(
	struct init_module *mod,
	int (*init)()
);

int
init_module_local_uninit_register(
	struct init_module *mod,
	int (*uninit)()
);

int
init_module_uninit_register(
	struct init_module *mod,
	int (*uninit)()
);

int
init_module_uninit(
	struct init_module *mod
);

int
init_module_init(
	struct init_module *mod	
);

int
init_module_init_test(
	struct init_module *mod
);

#define INIT_MODULE(mod) { \
	.init_flag = 0, \
	.uninit_flag = 0, \
	.mutex = PTHREAD_MUTEX_INITIALIZER,\
	.init_list = LIST_HEAD_INIT(mod.init_list),\
	.uninit_list = LIST_HEAD_INIT(mod.uninit_list),\
	.local_init_list = LIST_HEAD_INIT(mod.local_init_list),\
	.local_uninit_list = LIST_HEAD_INIT(mod.local_uninit_list),\
	.init_mutex = PTHREAD_MUTEX_INITIALIZER, \
	.uninit_mutex = PTHREAD_MUTEX_INITIALIZER, \
	.local_init_mutex = PTHREAD_MUTEX_INITIALIZER, \
	.local_uninit_mutex = PTHREAD_MUTEX_INITIALIZER, \
	.init_handle = init_module_init, \
	.uninit_handle = init_module_uninit, \
	.other_init_register = init_module_init_register, \
	.other_uninit_register = init_module_uninit_register, \
	.local_init_register = init_module_init_register, \
	.local_uninit_register = init_module_uninit_register, \
	.init_test = init_module_init_test, \
}

#endif
