#include "err_manage.h"
#include "err_private.h"
#include "letter_hash.h"
#include "init.h"

#include "hash.h"

struct err_manag_node {
	struct err_handle em_handle;
};

struct err_manager {
	letter_t em_hash;
};

static struct err_manager global_em;

static int
em_letter_hash(
	unsigned long em_data,
	unsigned long hash_data
)
{

}

static int
em_letter_get(
	unsigned long em_data,
	unsigned long cmp_data
)
{

}

static int
em_letter_destroy(
	unsigned long em_data
)
{
}

static int
err_manager_uninit()
{
}

int
err_manager_init() 
{
	global_em.em_hash = letter_create(
															em_letter_hash, 
															em_letter_get, 
															em_letter_destroy);
	if (!global_em.em_hash)
		PANIC("create letter hash error\n");

	return local_init_register(err_manager_uninit);
}

MOD_INIT(err_manager_init);
