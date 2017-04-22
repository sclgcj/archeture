#ifndef ERR_H
#define ERR_H

//err number
enum{
	ERR = -1,
	OK = 0,
	TIMEOUT,
	EMPTY_FILE,
	PEER_RESET,
	PEER_CLOSED,
	OPEN_FILE_ERR,
	PARAM_ERROR,
	NOT_ENOUGH_MEMORY,
	NOT_REGISTER_CMD,
	NOT_REGITSTER_CONFIG_OPT,
	GETOPT_LONG_ERR,
	HASH_LIST_DELETED,
	NOT_REGISTER_CONFIG,
	CREATE_THREAD_ERR,
	ADDR_ALREADY_INUSE,
	WOULDBLOCK,
	CTL_EPOLL_ERR,
	CURL_INIT_ERR,
	CURL_PERFORM_ERR,
	WRONG_JSON_DATA,
	RECV_ERR,
	SEND_ERR,
	PORT_MAP_FULL,
	NO_INTERFACE_PATH,
	NO_INTERFACE_SET,
	WRONG_JSON_FILE,
	WRONG_JSON_DATA_TYPE,
	EPOLL_ERR,
	WRONG_RECV_RETURN_VALUE,
	CONNECT_ERR,
	NO_HEAP_DATA,
	PEER_REFUSED,
	NOT_CONNECT,
	NOT_REGISTER_ADDR,
	NO_SUCH_PARAM_TYPE,
	NO_SUCH_PARAMETER_SET,
	FILE_MAP_FAILED,
	MAX
};

/**
 * error_uninit() - uninit error module
 *
 * Return: 0 if successful, -1 if failed
 */
int
error_uninit();


/**
 * max_errno_get() - return the currently max errno
 *
 * When upstream wants to add its own errno, we advise 
 * to call this function first, so the errno added by 
 * upstream will no cover the one used in downstream.
 *
 * Return: the currently mac errno
 */ 
int 
max_errno_get();
#define MAX_ERRNO_GET() MAX

/** 
 * get_cur_errno_get() - return the current set errno
 *
 * If upstreams want to get the current errno, they can
 * call this function
 *
 * Return : the currently set errno value
 */
int
cur_errno_get();

/**
 * err_add() - add new error num to the err module
 * @errno:	added errno	
 * @errmsg:	the man-reading msg to the errno
 *
 * This function used to add a new errno and it msg 
 * to the downstream's error handle structure, so that
 * we can add the errcode at anytime we want.
 *
 * Return: 0 if success, otherwise return an errcode
 */
int
err_add(
	int errorno, 
	char *errmsg 
);

/**
 * errno_set() - set errno for every error
 * @errno:  the error number of the current error
 *
 * This function usually used for the downstring,
 * but in order to have the same error msg output
 * with the upstream, we decide to provide it out,
 * so that upstream function can also call it to 
 * set its own errno. Of cource, if upstream wants
 * get such effect, it should call err_add fucntion
 * first to register its own err msg.
 *
 * Return: no
 */

void
errno_set(
	const char *file,
	const char *func,
	int  line,
	int err,
	int system_err
);
#define ERRNO_SET(err) errno_set(__FILE__, __FUNCTION__, __LINE__, err, errno)

/**
 * errmsg_get() - get the errmsg of current set errno
 *
 * This can return the related err man-reading msg of current
 * errno set by an error. We are sure that it will return 
 * the ringt msg in downstring. Before calling this function,
 * upstream should call errno_set to set the right errno 
 * to current error.
 *
 * Retrun: the errmsg of the current errno if the errno is ok;
 *	   otherwise return "no related errmsg for errno current 
 *	   xxx"
 */
char *
errmsg_get( 
	int errorno
);
#define CUR_ERRMSG_GET() errmsg_get(cur_errno_get())

#endif
