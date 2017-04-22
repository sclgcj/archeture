#ifndef HEAP_H
#define HEAP_H 1

typedef void* heap_handle_t;

enum {
	HEAP_LARGER,
	HEAP_LESS,
	HEAP_SAME,
	HEAP_MAX
};

/*
 * heap_create() - create the heap handle 
 * @user_cmp_func:  the function to decide if the new is larger/less than old, 
 *		    if it is, user_cmp_func return 0, otherwise return -1
 *	@first: the first parameter pointed to a user defined data
 *	@second: the second parameter pointed to a user defined data
 *	@Return: in min heap, 0 if first is larger than second, otherwise return -1;
 *		 in max heap, 0 if first is less than second, otherwise return -1;
 * @user_data_destroy: destroy the user data, this function will be called when delete
 *		       the root node or destroy the heap
 *
 * Return: a heap handle will be returned if successful, NULL if not and errno will be set
 */
heap_handle_t
heap_create(
	int (*user_cmp_func)(unsigned long first, unsigned long second)
);

/*
 * heap_node_add() - add a heap node
 * @handle:	the heap handle returned by heap_create() function
 * @use_data:	user data
 *
 * Return:	0 if successful, -1 if not and errno will be set
 */
unsigned long
heap_node_add(
	heap_handle_t handle,
	unsigned long    user_data
);

/*
 * heap_root_data_get() - get the root node's  data
 * @handle:	the heap handle returned by heap_create()
 * @user_data:  user to store the address of data in the root node
 *
 * Return:	0 if successful, -1 if not and errno will be set
 */
int
heap_root_data_get(
	heap_handle_t handle,
	unsigned long    *user_data
);

/*
 * heap_traversal()  - traversal the heap
 * @handle:	the heap handle returned by heap_create()
 *
 * Be careful, This function is not thread-safa
 *	
 * Return:	0 if successful, -1 if not and errno will be set
 */
int
heap_traversal(
	heap_handle_t handle,
	void (*heap_traversal)(unsigned long user_data)
);

/*
 * heap_destroy() - destroy the heap
 * @handle:	the heap handle returned by heap_create
 * @user_data_destroy: user defined function to destory user data
 *
 * Return:	0 if successful, -1 if not and errno will be set
 */
int
heap_destroy(
	heap_handle_t handle,
	void (*user_data_destroy)(unsigned long user_data)
);

/*
 * heap_root_data_peek():	Peek the root's user_data
 * @handle:	the heap handle returned by heap_create
 * @user_data:	the pointer pointed to space which is used 
 *		to store the root's user_data value
 *
 * This function is similar to heap_root_get, but it just
 * get the root data and don't  delete the node from the 
 * heap. It will save the adjust time.
 *		
 * Return:	0 if successful, -1 if not and errno will be set
 */
int
heap_root_data_peek(
	heap_handle_t handle,
	unsigned long	 *user_data
);

#endif
