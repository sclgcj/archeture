#include "param_file.h"
#include "std_comm.h"
#include "err.h"
#include "init.h"
#include "param_manage_private.h"
#include "print.h"
#include "global_log_private.h"

#include <sys/mman.h>

static int
param_file_oper(
	int oper_cmd, 
	struct param *param
)
{
	return OK;
}

static void
param_file_set(
	struct param *new_param,
	struct param *old_param
)
{
	struct param_file *new_file = NULL;
	struct param_file *old_file = NULL;

	if (!new_param || !old_param)
		return;

	new_file = (struct param_file *)new_param->data;
	old_file = (struct param_file *)old_param->data;

	memset(old_file, 0, sizeof(*old_file));
	memcpy(old_file, new_file, sizeof(*old_file));
}

static void
param_file_destroy(
	struct param *param
)
{
	return;
}

static char *
param_file_value_get(
	struct create_link_data *cl_data,
	struct param *param
)
{
	int start_line = 0, end_line = 0;
	char *start = NULL, *end = NULL, *cur = NULL;
	FILE *fp = NULL;
	struct param_file *fparam = NULL;

	if (!param) {
		ERRNO_SET(PARAM_ERROR);
		return NULL;
	}
	if (fparam->file_map == MAP_FAILED || 
			fparam->file_map == NULL) {
		ERRNO_SET(FILE_MAP_FAILED);
		return NULL;
	}

	cur = start = fparam->file_map;
	
	return NULL;
}

static int
param_file_map_set(
	struct param_file *param
)
{
	int fd = 0;
	struct stat sbuf;

	memset(&sbuf, 0, sizeof(sbuf));
	fd = open(param->file_path, O_RDONLY);
	if (fd == -1) {
		ERRNO_SET(OPEN_FILE_ERR);
		GINFO("open error: %s\n", strerror(errno));
		return ERR;
	}
	fstat(fd, &sbuf);
	param->file_size = sbuf.st_size;
	param->file_map = (char*)mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (param->file_map == MAP_FAILED) {
		ERRNO_SET(FILE_MAP_FAILED);
		GINFO("mmap error: %s\n", strerror(errno));
		close(fd);
		return ERR;
	}
out:
	close(fd);	
	return OK;
}

static struct param *
param_file_copy(
	struct param *param
)
{
	int ret = 0;
	struct param *nparam = NULL;
	struct param_file *param_file = NULL;
	struct param_file *new_param = NULL;

	nparam = (struct param *)calloc(1, sizeof(*param_file) + sizeof(*nparam));
	if (!nparam)
		PANIC("Not enough memory for %d bytes\n", 
				sizeof(*param_file) + sizeof(*nparam));
	new_param = (struct param_file *)param->data;
	param_file = (struct param_file*)nparam->data;
	memcpy(param_file, new_param, sizeof(*new_param));

	ret = param_file_map_set(new_param);
	if (ret != OK) {
		FREE(nparam);
		nparam = NULL;
	}

	return nparam;
}

static int
param_file_setup()
{
	struct param_oper oper;

	memset(&oper, 0, sizeof(oper));
	oper.param_oper = param_file_oper;
	oper.param_value_get = param_file_value_get;
	oper.param_copy = param_file_copy;
	oper.param_destroy = param_file_destroy;
	oper.param_set = param_file_set;

	return param_type_add("file", &oper);
}

int
param_file_uninit()
{
}

int
param_file_init()
{
	int ret = 0;	
	
	ret = local_init_register(param_file_setup);
	if (ret != OK) 
		PANIC("local_init_register: register param_file_setup error");

	return local_uninit_register(param_file_uninit);
}

MOD_INIT(param_file_init);
