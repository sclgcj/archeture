#ifndef ADDR_MANAGE_PRIVATE_H
#define ADDR_MANAGE_PRIVATE_H 1

#include "addr_manage.h"

struct address_oper {
	struct sockaddr* (*addr_create)(struct address* ta);
	struct address* (*addr_encode)(int type, struct sockaddr *addr);
	void (*addr_decode)(struct address *taddr, struct sockaddr *addr);
	void (*addr_destroy)(struct address *addr);
	int (*addr_length)();
};

int
address_add(
	struct address_oper *oper
);

struct address *
address_encode(
	int addr_type,
	struct sockaddr *addr
);

void
address_decode(
	struct address *ta,
	struct sockaddr *addr
);

void
address_destroy(
	struct address *ta
);

struct sockaddr *
address_create(
	struct address *ta
);

int
address_length(
	int type
);

#endif
