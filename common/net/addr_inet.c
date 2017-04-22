#include "std_comm.h"
#include "err.h"
#include "init.h"
#include "print.h"
#include "addr_inet.h"
#include "addr_manage_private.h"

struct addr_inet_data {
	int addr_id;
};

static struct addr_inet_data global_inet_data;

int
addr_inet_id_get()
{
	return global_inet_data.addr_id;
}

static int
addr_inet_length()
{
	return sizeof(struct sockaddr_in);
}

static struct sockaddr *
addr_inet_create(
	struct address *ta
)
{
	struct sockaddr_in *addr = NULL;
	struct addr_inet *ti = NULL;

	addr = (struct sockaddr_in*)calloc(1, sizeof(*addr));
	if (!addr) 
		PANIC("Not enough memory for %d bytes\n", sizeof(*addr));

	if (ta) {
		ti = (struct addr_inet*)ta->data;
		addr->sin_family = AF_INET;
		addr->sin_addr.s_addr = ti->ip;
		addr->sin_port = htons(ti->port);
	}

	return (struct sockaddr*)addr;
}

static struct address*

addr_inet_encode(
	int addr_type,
	struct sockaddr *addr
)
{
	struct addr_inet *ti = NULL;
	struct address *ta = NULL;
	struct sockaddr_in *in_addr = NULL;
	
	in_addr = (struct sockaddr_in*)addr;
	
	ta = (struct address *)calloc(1, sizeof(*ta) + sizeof(*ti));
	if (!ta) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return NULL;
	}
	ta->addr_type = addr_type;
	ti = (struct addr_inet*)ta->data;
	ti->ip = in_addr->sin_addr.s_addr;
	ti->port = ntohs(in_addr->sin_port);
	
	return ta;
}

static void
addr_inet_decode(
	struct address *ta,
	struct sockaddr *addr
)
{
	struct addr_inet *ti = NULL;
	struct sockaddr_in *in_addr = NULL;

	in_addr = (struct sockaddr_in *)addr;
	ti = (struct addr_inet*)ta->data;
	in_addr->sin_family = AF_INET;
	in_addr->sin_port = htons(ti->port);
	in_addr->sin_addr.s_addr = ti->ip;
}

static void
addr_inet_destroy(
	struct address *ta
)
{
	FREE(ta);
}

static int
addr_inet_setup()
{
	struct address_oper oper;

	oper.addr_destroy = addr_inet_destroy;
	oper.addr_decode  = addr_inet_decode;
	oper.addr_encode  = addr_inet_encode;
	oper.addr_create  = addr_inet_create;
	oper.addr_length  = addr_inet_length;

	global_inet_data.addr_id = address_add(&oper);

	return OK;
}

int
addr_inet_init()
{
	int ret = 0;	

	ret = local_init_register(addr_inet_setup);
	if (ret != OK)
		return ret;

	return OK;
}

MOD_INIT(addr_inet_init);
