#ifndef NUMBER_HASH_PRIVATE_H

#define NUMBER_HASH_PRIVATE_H

#include "hash.h"
typedef void* number_t;

number_t
number_create(
	int total,
	int (*number_get)(unsigned long user_data, unsigned long cmp_data),
	int (*number_destroy)(unsigned long user_data)
);

int
number_destroy(
	number_t jnumber
);

int
number_add(
	int id,	
	unsigned long user_data,
	number_t jnumber
);

unsigned long
number_get(
	int id,
	unsigned long cmp_data,
	number_t jnumber
);

int
number_traversal(
	unsigned long user_data,
	number_t jnumber,
	int (*jn_traversal)(unsigned long jn_data, unsigned long data)
);

#endif
