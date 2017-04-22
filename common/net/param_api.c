#include "comm.h"
#include "err.h"
#include "param_api.h"
#include "create_private.h"
#include "param_api_private.h"
#include "param_manage_private.h"

param_manage_t *
param_create()
{
	(param_manage_t*)param_manage_create();
}

void
param_destroy(
	param_manage_t *pm
)
{
	param_manage_destroy((struct param_manage*)pm);
}

int
param_add(
	char		*param_name,
	char		*param_type,
	param_t	*param
)
{
	return param_manage_add(param_name, param_type, (struct param*)param);
}

int
param_del(
	char *param_name
)
{
	return param_manage_del(param_name);
}

int
param_list_get(
	param_list_t *list	
)
{
	return param_manage_list_get((struct param_manage_list *)list);
}

void
param_list_free(
	param_list_t *list
)
{
	param_manage_list_free((struct param_manage_list*)list);
}

param_t *
param_config_get(
	char		*param_name
)
{
	if (!param_name) {
		ERRNO_SET(PARAM_ERROR);
		return NULL;
	}

	return param_manage_config_get(param_name, NULL);
}

int
param_set(
	char		*param_name,
	param_t	*param
)
{
	if (!param_name || !param ) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	return param_manage_set(param_name, param, NULL);
}

char *
param_value_get(
	char		*param_name,
	unsigned long	user_data
)
{
	struct create_link_data *cl_data = NULL;

	if (!param_name || !user_data) {
		ERRNO_SET(PARAM_ERROR);
		return NULL;
	}
	cl_data = create_link_data_get(user_data);

	return cl_data->pm->pm_value_get(param_name, cl_data->pm);
}

int
param_oper(
	int oper_cmd,
	char *param_name
)
{
	struct create_link_data *cl_data = NULL;

	if (!param_name) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	return cl_data->pm->pm_oper(oper_cmd, param_name, NULL);
}

