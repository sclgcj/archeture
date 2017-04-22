#ifndef HASH_HUB_PRIVATE_H
#define HASH_HUB_PRIVATE_H 1

#include "create_private.h"

void
hash_hub_link_del(
	unsigned long hub_data
);

int
hash_hub_create(
	char *app_proto,
	struct create_config *conf
);

#endif
