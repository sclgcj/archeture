#ifndef HASH_H
#define HASH_H

#include "std_comm.h"

typedef void * hash_handle_t;
#define HASH_ERR  (void*)-1

/**
 * hash_create() - used to create a hash_table for the caller
 * @tabel_size:		tell the handler the size of caller's hash table size
 * @hash_func:		function used to determine the pos of the hash head in
 *			the hash table, can be null
 * @hash_get:		used to get check if the current node is the one that 
 *			user really needed, this function can't no be null,
 * @hash_destroy:	the fucntion used when destroy the created hash table,
 *			can be null
 *
 * Normally, hash_func and hash_destroy can not be null, but here we will give 
 * them default functions. If hash_func is null, we will always add node to the
 * fisrt hlsit_head structure; if hash_destroy is null, we will do nothing but 
 * delete the node from the head and the left response should be taken by caller.
 *
 * Return: a hash_handle_t structure wiill return if successful, otherwise 
 *	   return HASH_ERR
 */
hash_handle_t
hash_create(
	int table_size,
	int (*hash_func)(struct hlist_node *node, unsigned long user_data),
	int (*hash_get)(struct hlist_node *node, unsigned long user_data),
	int (*hash_destroy)(struct hlist_node *node)
);

/**
 * hash_destrooy() - used to destry the created hash tabel
 * @handle:		a hash_handle_t structure returned from hash_create
 *
 * Return: 0 if successful and -1 will be returned if something wrong
 */
int
hash_destroy(
	hash_handle_t handle
);

/**
 * hash_add() - add a new node to the hash table
 * @handle:		a structure returned from hash_create
 * @node:		the hash node address in the caller's structure
 * @user_data:		used in hash_func to determine which hash_head it should 
 *			be added in
 *
 * Return: 0 if successful and -1 will be returned if something wrong
 */
int
hash_add(
	hash_handle_t handle,
	struct hlist_node *node,
	unsigned long user_data
);

/**
 * hash_del() - add a new node to the hash table
 * @handle:		a structure returned from hash_create
 * @node:		the hash node address in the caller's structure
 * @user_data:		used in hash_func to determine which hash_head it should 
 *			be added in
 *
 * Return: 0 if successful and -1 will be returned if something wrong
 */
int
hash_del(
	hash_handle_t handle,
	struct hlist_node *node,
	unsigned long user_data
);

/**
 * hash_get() - get an exist  node to the hash table
 * @handle:		a structure returned from hash_create
 * @user_data:		used as parameter of hash_func
 *
 * Return: an hlist_node structure if find one and NULL will be returned if something wrong
 */
struct hlist_node * 
hash_get(
	hash_handle_t handle,
	unsigned long hash_data,
	unsigned long search_cmp_data
);
#define HASH_GET(h, type, memb, index_data, search_data) \
	list_entry(hash_get(h, index_data, search_data), type, memb)

/*
 * hash_traversal - traversal the whole hash list
 * @del_flag:	a flag to determine if we need to delete each hash node, 0 need, 1 not
 * @user_data:	user data
 * @handle:	hash handle pointer created by hash_create fucntion
 * @hash_walk_handle: function to dispose every hash node
 *	@@user_data: user data
 *	@@hnode: hash list node
 *	@@flag:  a flag to help to determine whether to destroy this hash node: 0 not, 1 need
 *
 * We hope to provide a function to simplify the operation of traversal every hash node.
 * We don't want to write this kind of code every time, so we provide this. We just need 
 * to give the node handle function which will be different at different scene. There are many
 * ugly design, but at present we don't have enough time to think more, just use it, we will
 * modify them to be better later.
 *
 * Return: no
 */
int
hash_traversal(
	unsigned long    user_data,
	hash_handle_t handle,
	int (*hash_walk_handle)(unsigned long user_data, struct hlist_node *hnode, int *flag)
);
#define HASH_WALK(handle, ud, walk_func) hash_traversal(ud, handle, walk_func)
#define HASH_WALK_DEL(handle, ud, walk_func) hash_traversal(ud, handle, walk_func)

/*
 * hash_del_and_destroy() -	delete the node from hash table and call the hash_destroy 
 *				callback to destroy the specified data	
 * @handle:	hash handle returned by hash_create function
 * @node:	the hash node of the specified structure
 * @user_data:	user data
 *
 * We just provide the hash_del function which just delete the node from structure at first.
 * However, we gradually find that just deleting node is not enough, we add this function to 
 * provide a more flexible function to handle the deleting node in the hash table.
 *
 * Return: 0 if successful, -1 if not and related errno will be set
 */
int
hash_del_and_destroy(
	hash_handle_t handle,
	struct hlist_node *node,
	unsigned long user_data
);

/*
 * hash_head_traversal() - just traversal one hash list, this means that we can get 
 *			      the same value in the hash list. 
 * @handle:	hash_handle returned by hash_create function
 * @hash_data:  used to find the hash list table index
 * @search_cmp_data: used to find the specified hash node
 * @user_data:  user data
 * @traversal:	hash node dispose function
 *
 * Return: 0 if successful, -1 if not
 */
int
hash_head_traversal(
	hash_handle_t  handle,		
	unsigned long     hash_data,
	unsigned long	  user_data,
	void (*traversal)(struct hlist_node *hnode, unsigned long user_data)
);

#endif
