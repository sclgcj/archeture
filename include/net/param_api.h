#ifndef PARAM_API_H
#define PARAM_API_H


#include "param_manage.h"
#include "param_api_private.h"

typedef struct param			param_t;	
typedef struct param_manage_list	param_list_t;

int
param_add(
	char		*param_name,
	char		*param_type,	
	param_t	*param
);

int
param_del(
	char	*param_name
);

param_t*
param_config_get(
	char		*param_name
);

int
param_set(
	char		*param_name,
	param_t	*param
);

char *
param_value_get(
	char *param_name,
	unsigned long user_data
);

int
param_oper(
	int oper_cmd,
	char *param_name
);

int
param_list_get(
	param_list_t *list
);

void
param_list_free(
	param_list_t *list
);

#endif
