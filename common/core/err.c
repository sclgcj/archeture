#include "std_comm.h"
#include "err.h"
#include "hash.h"
#include "init.h"
#include "err_private.h"
#include "print.h"

/*
 * This module is an internal module, but we also want to make it modifiable.
 * A more portable module can be easier to maitain. What will import thread- 
 * safety problem in this module is global_errno. Of course, this is based
 * on the basic modul hash which must be designed as thread-safety
 */

struct err_msg_node{
	int err;
	int start_len;
	int end_len;
	char *msg;
	struct hlist_node node;
};


#define ERR_HASH_TABLE	128
#define ERR_HASH_MASK	0x7f 


/*
 * Use global variable may be ugly, but at present this may be 
 * the best way to solve this kind of problem.
 */
static __thread int global_errno;


static struct err_handle global_err_handle;

static int
err_hash(
	struct hlist_node *hnode, 
	unsigned long     user_data
)
{
	int err = 0;
	struct err_msg_node *err_msg = NULL;

	if (!hnode && !user_data)
		return ERR;

	if (!hnode) 
		err = (int)user_data;
	else {
		err_msg = list_entry(hnode, struct err_msg_node, node);
		err = err_msg->err;
	}

	return err & ERR_HASH_MASK;
}

static int
err_hash_get(
	struct hlist_node *hnode, 
	unsigned long user_data
)
{
	struct err_msg_node *err_msg = NULL;

	if (!hnode) 
		return PARAM_ERROR;
	err_msg = list_entry(hnode, struct err_msg_node, node);
	if (err_msg->err == (int)user_data) {
		return OK;
	}

	return ERR;
}

static int
err_hash_destroy(
	struct hlist_node *hnode
)
{
	struct err_msg_node *err_msg = NULL;

	if (!hnode) 
		return PARAM_ERROR;
	err_msg = list_entry(hnode, struct err_msg_node, node);
	if (err_msg->msg)
		free(err_msg->msg);

	free(err_msg);

	return OK;
}

int
error_setup()
{
	struct err_handle err_handle;

	memset(&err_handle, 0, sizeof(err_handle));
	global_err_handle.err_hash = hash_create(
					ERR_HASH_TABLE, 
					err_hash,
					err_hash_get,
					err_hash_destroy);
	if (global_err_handle.err_hash == (HASH_ERR))
		return ERR;

	err_add(TIMEOUT, "timeout");
	err_add(PARAM_ERROR, "parameter error");
	err_add(NOT_ENOUGH_MEMORY, "not enough memory");
	err_add(NOT_REGISTER_CMD, "the cmd is not registered");
	err_add(GETOPT_LONG_ERR, "getopt execute error");
	err_add(HASH_LIST_DELETED, "deleted the hash list");
	err_add(NOT_REGISTER_CONFIG, "not call the config_oper_register function");
	err_add(OPEN_FILE_ERR, "open file error");
	err_add(EMPTY_FILE, "the file does not exist or its size is 0");
	err_add(CREATE_THREAD_ERR, "create thread error");
	err_add(ADDR_ALREADY_INUSE, "address is already in use");
	err_add(WOULDBLOCK, "the address would block");
	err_add(CTL_EPOLL_ERR, "control socket to epoll err");
	err_add(CURL_INIT_ERR, "curl init error");
	err_add(CURL_PERFORM_ERR, "curl perform error");
	err_add(WRONG_JSON_DATA, "wrong json data");
	err_add(PEER_CLOSED, "peer closed the link");
	err_add(PEER_RESET, "peer reset the link");
	err_add(RECV_ERR, "receive error");
	err_add(SEND_ERR, "send error");
	err_add(PORT_MAP_FULL, "port map full");
	err_add(NO_INTERFACE_PATH, "don't set the interface input or check path");
	err_add(NO_INTERFACE_SET, "dont't set such interface in configure file");
	err_add(WRONG_JSON_FILE, "The json data in file is not right");
	err_add(WRONG_JSON_DATA_TYPE, "The reponse data's type is not the "
					    "same as the one in check file");
	err_add(EPOLL_ERR, "epoll err\n");
	err_add(WRONG_RECV_RETURN_VALUE, "The return value from recv callback is wrong");
	err_add(CONNECT_ERR, "connect to server error\n");
	err_add(NO_HEAP_DATA, "don't add heap data\n");
	err_add(PEER_REFUSED, "The peer refused to connect");
	err_add(NOT_CONNECT, "Don't build the connect");
	err_add(NOT_REGISTER_ADDR, "Don't register this kind of address");
	err_add(NO_SUCH_PARAM_TYPE, "Don't register this kind of parameter");
	err_add(NO_SUCH_PARAMETER_SET, "Don't register such parameter");
	err_add(FILE_MAP_FAILED, "File map error");

	return OK;
}

int
max_errno_get()
{
	return MAX;
}

int
cur_errno_get()
{
	int err = 0;

	return global_errno;
	/*pthread_mutex_lock(&global_err_handle);
	err = global_err_handle.errno;
	pthread_mutex_unlock(&global_err_handle);*/
}

int 
err_add(
	int errorno,
	char *errmsg
)
{
	int ret = 0;
	int msg_len = 0;
	struct err_msg_node *err_msg = NULL;

	err_msg = (struct err_msg_node*)calloc(sizeof(*err_msg), 1);
	if (!err_msg) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return ERR;
	}
	err_msg->err = errorno;
	if (errmsg) {
		msg_len = strlen(errmsg);
		err_msg->start_len = msg_len;
		err_msg->msg = (char*)calloc(sizeof(char), msg_len + 1);
		if (!err_msg->msg) {
			free(err_msg);
			return NOT_ENOUGH_MEMORY;
		}
		memcpy(err_msg->msg, errmsg, msg_len);
	}
	ret = hash_add(
			global_err_handle.err_hash, 
			&err_msg->node, 
			0);
	if (ret != OK) {
		return ret;
	}

	return OK;
}

void
errno_set(
	const char *file,
	const char *func,
	int  line,
	int err,
	int system_err
)
{
	int len = 0, old_len = 0, new_len = 0;
	char *msg = NULL;
	struct hlist_node *hnode = NULL;
	struct err_msg_node *msg_node;
	
	global_errno = err;
	if (system_err == 0)
		return;
	
	hnode = hash_get(global_err_handle.err_hash, err, err);
	if (!hnode)
		return;
	msg_node = list_entry(hnode, struct err_msg_node, node);

	msg = strerror(system_err);
	//PRINT("err: %s -> %s\n", msg_node->msg, msg);
	old_len = strlen(msg_node->msg);
	new_len = strlen(msg) + msg_node->start_len + 
		  strlen(file) + strlen(func) + 64;
	if (msg_node->end_len > new_len) {
		memset(msg_node->msg + msg_node->start_len, 
			0, 
			old_len - msg_node->start_len);
		sprintf(msg_node->msg + msg_node->start_len, 
			"->%s[%s:%d]=>%s", 
			file, func, line, msg);
		return;
	}
	if (msg_node->end_len == new_len)
		new_len += 1;
	
	msg_node->end_len = new_len;
	msg_node->msg = (char*)realloc(msg_node->msg, new_len);
	if (!msg_node->msg)  
		PANIC("Not enough memory for %d bytes\n", len + 3);
	memset(msg_node->msg + msg_node->start_len, 0, new_len - msg_node->start_len);
	sprintf(msg_node->msg + msg_node->start_len, "->%s[%s:%d]=>%s", file, func, line, msg);

	errno = 0;
}

static char *
comm_msg_get(
	int err
)
{
	switch (err) {
	case ERR:
		return "internal error";
	case OK:
		return "success";
	default: 
		return NULL;
	}
}

char *
errmsg_get(
	int err
)
{
	int len = 0;
	char *msg = NULL;
	char tmp_msg[32] = { 0 };
	struct hlist_node *hnode = NULL;
	struct err_msg_node *err_msg = NULL;
	
	msg = comm_msg_get(err);
	if (msg) {
		return msg;
	}

	hnode = hash_get(global_err_handle.err_hash, err, err);
	if (!hnode) {
		return "non defined error code";
	}
	err_msg = list_entry(hnode, struct err_msg_node, node);

	return err_msg->msg;
}

int
error_uninit()
{
	hash_destroy(global_err_handle.err_hash);

	return OK;
}


int
error_init()
{
	int ret = 0;
	
	ret = error_setup();
//	ret = local_init_register(error_setup);
	if (ret != OK) 
		return ret;
	return uninit_register(error_uninit);
}

/**
 * declare error_init as constructor 
 */
MOD_INIT(error_init);
