#include "std_comm.h"
#include "err.h"
#include "init.h"
#include "print.h"
#include "addr_unix.h"
#include "addr_manage_private.h"

struct addr_unix_data {
	int addr_id;
};

static struct addr_unix_data global_unix_data;

int
addr_unix_id_get()
{
	return global_unix_data.addr_id;
}

static int
addr_unix_length()
{
	return sizeof(struct sockaddr_un);
}

static struct sockaddr *
addr_unix_create(
	struct address *ta
)
{
	struct sockaddr_un *addr = NULL;
	struct addr_unix *tu = NULL;

	addr = (struct sockaddr_un*)calloc(1, sizeof(*addr));
	if (!addr) 
		PANIC("Not enough memory for %d bytes\n", sizeof(*addr));
	if (ta) {
		tu = (struct addr_unix *)ta->data;
		memcpy(addr->sun_path, tu->unix_path, UNIX_PATH_MAX);
	}

	return (struct sockaddr*)addr;
}

static struct address*
addr_unix_encode(
	int addr_type,
	struct sockaddr *addr
)
{
	struct addr_unix *tu = NULL;
	struct address *ta = NULL;
	struct sockaddr_un *in_addr = NULL;
	
	in_addr = (struct sockaddr_un*)addr;
	
	ta = (struct address *)calloc(1, sizeof(*ta) + sizeof(*tu));
	if (!ta) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return NULL;
	}
	ta->addr_type = addr_type;
	tu = (struct addr_unix*)ta->data;
	memcpy(tu->unix_path, in_addr->sun_path, UNIX_PATH_MAX);
	
	return ta;
}

static void
addr_unix_decode(
	struct address *ta,
	struct sockaddr *addr
)
{
	struct addr_unix *tu = NULL;
	struct sockaddr_un *in_addr = NULL;

	in_addr = (struct sockaddr_un *)addr;
	tu = (struct addr_unix*)ta->data;
	in_addr->sun_family = AF_UNIX;
	memcpy(in_addr->sun_path, tu->unix_path, UNIX_PATH_MAX);
}

static void
addr_unix_destroy(
	struct address *ta
)
{
	FREE(ta);
}

static int
addr_unix_setup()
{
	struct address_oper oper;

	oper.addr_destroy = addr_unix_destroy;
	oper.addr_decode  = addr_unix_decode;
	oper.addr_encode  = addr_unix_encode;
	oper.addr_create  = addr_unix_create;
	oper.addr_length  = addr_unix_length;

	global_unix_data.addr_id = address_add(&oper);

	return OK;
}

int
addr_unix_init()
{
	int ret = 0;	

	ret = local_init_register(addr_unix_setup);
	if (ret != OK)
		return ret;

	return OK;
}

MOD_INIT(addr_unix_init);
