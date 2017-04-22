#ifndef LETTER_HASH_PRIVATE_H
#define LETTER_HASH_PRIVATE_H

#include "hash.h"

typedef void * letter_t;

struct letter_param {
	char *name;
	unsigned long user_data;
};

letter_t
letter_create(
		int (*letter_hash)(unsigned long user_data, unsigned long hash_data),
		int (*letter_get)(unsigned long user_data, unsigned long cmp_data),
		int (*letter_destroy)(unsigned long data));

int
letter_add(char *name,
	      unsigned long user_data,
	      letter_t letter);

unsigned long
letter_get(struct letter_param *jlp,
	      letter_t letter);

int
letter_destroy(letter_t letter);

int
letter_traversal(
	unsigned long user_data,
	letter_t letter,
	int (*jl_traversal)(unsigned long jl_data, unsigned long data)
);

#endif
