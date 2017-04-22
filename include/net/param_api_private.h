#ifndef PARAM_API_PRIVATE_H
#define PARAM_API_PRIVATE_H

#include "param_manage_private.h"

typedef struct param_manage		param_manage_t;

param_manage_t *
param_create();

void
param_destroy(
	param_manage_t *pm
);

#endif
