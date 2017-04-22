#ifndef INIT_H
#define INIT_H 1


/*
 * 总是想尽可能的把每个模块的完全的独立开来， 让其
 * 不会影响到其他, 这里会用到一些gcc特性，说实话，
 * 自己还是蛮喜欢用这些的... 嘿嘿
 */

#define MOD_INIT(func) \
	int __attribute__((constructor)) func()

#define MOD_EXIT(func) \
	int __attribute__((destructor)) func()

/*
 * init_register() - regiter module's init function
 * @init	the init function implemented by module
 *
 * we hope all the modules use this function to register
 * their init functions. so that we can init ervery modules
 * in a universal way.
 *		
 *
 * Return: 0 if successful, -1 if failed;
 *
 * init function will return 0 if successful and -1 if failed;
 */
int
init_register(
	int (*init)()
);

/*
 * uninit_register() - regiter module's uninit function
 * @uninit	the uninit function implemented by module
 *
 * we hope all the modules use this function to register
 * their uninit functions. so that we can uninit ervery modules
 * in a universal way.
 *		
 *
 * Return: 0 if successful, -1 if failed;
 *
 * uninit function will return 0 if successful and -1 if failed;
 */
int
uninit_register(
	int (*uninit)()
);

/*
 * init() -	used to init all the modules which registered by 
 *		local_init_register function.	
 *
 * Return: 0 if successful, -1 if failed;
 */
int
init();

/*
 * uninit() -	used to init all the modules which registered by 
 *			local_init_register function.	
 *
 * Return: 0 if successful, -1 if failed;
 */
int
uninit();

int
local_init_register(
	int (*init)()
);

int
local_uninit_register(
	int (*uninit)()
);

int 
init_test();



#endif
