#include "create_log.h"
#include "comm.h"
#include "create_private.h"
#include "log.h"
#include "create_log_private.h"
#include <stdarg.h>


#define NONE		"\e[0m"
#define BLACK		"\e[0;30m"
#define L_BLACK		"\e[1;30m"
#define RED		"\e[0;31m"
#define L_RED		"\e[1,31m"
#define GREEN		"\e[0;32m"
#define L_GREEN		"\e[1,32m"
#define BROWN		"\e[0,33m"

struct log {
	unsigned short  open_log;	//开启日志
	unsigned short  open_all_log;	//打开所有日志
	unsigned short  open_info_log;	//开启info日志
	unsigned short  open_debug_log;	//开启debug日志
	unsigned short  open_fatal_log;	//开启fatal(致命错误)日志
	unsigned short  open_system_log;	//开启系统日志
	unsigned short  open_warning_log;	//开启警告日志
	unsigned short  terminal_output;	//终端输出
	unsigned short	head_set;		//是否设置了头，一次输出多个打印
	log_data_t   log_data;
};

struct log_print{
	char *name;
	char *color;
};

static struct log_print global_log_print[] = {
	{"info", RED},
	{"warning", L_RED},
	{"debug", GREEN},
	{"fatal", L_GREEN},
	{"system", BROWN},
	{"default", NONE}
};

void
log_output_type_get(
	int  type,
	char **name,
	char **color
)
{
	if (type >= LOG_MAX) 
		type  = LOG_MAX;

	*name = global_log_print[type].name;
	*color = global_log_print[type].color;
}

#define LOG_CHECK(l, memb, type) \
	case type: \
		if (l->memb == 0) { \
			PRINT("type = %d\n", type);\
			return ERR;  \
		}\
		break;

static int
log_open(
	log_t	data,	
	int		type	
)
{
	struct log *log_data = NULL;

	log_data = (struct log*)data;

	if (log_data->open_log == 0)
		return ERR;

	switch (type) {
		LOG_CHECK(log_data, open_info_log, LOG_INFO);
		LOG_CHECK(log_data, open_warning_log, LOG_WARN);
		LOG_CHECK(log_data, open_debug_log, LOG_DEBUG);
		LOG_CHECK(log_data, open_fatal_log, LOG_FATAL);
		LOG_CHECK(log_data, open_system_log, LOG_SYSTEM);
	}

	return OK;
}

int
log_start(
	unsigned long user_data,
	char *file
)
{
	int ret = 0;
	struct log *log = NULL;
	struct create_link_data *cl_data = NULL;

	if (!user_data) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}
	cl_data = list_entry(user_data, struct create_link_data, data);
	log_address_print(cl_data->cl_hnode->log_data);
	log = (struct log*)cl_data->cl_hnode->log_data;
	ret = log_open(log, LOG_MAX);
	if (ret != OK)
		return ret;
	if (!file || !file[0])
		log->terminal_output = 1;
	return log_data_start(
			log->log_data, 
			cl_data->private_link_data.link_id,
			file);
}

static int
log_head_set(
	struct log *log,
	struct create_link_data *cl_data,
	char *fmt,
	...
)
{
	int ret = 0;
	va_list arg_ptr;

	va_start(arg_ptr, fmt);
	ret = log_data_write(
			log->log_data, 
			cl_data->private_link_data.link_id, 
			fmt,
			arg_ptr);
	va_end(arg_ptr);

	return ret;
}

int
log_write(
	unsigned long user_data,
	int direct,
	int type,
	char *fmt,
	...
)
{
	int ret = 0;
	char *head_fmt = NULL;
	va_list arg_ptr;
	time_t t = 0;
	struct tm *tm = NULL;
	struct log *log = NULL;
	struct create_link_data *cl_data = NULL;

	if (!user_data) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}
	cl_data = list_entry(user_data, struct create_link_data, data);

	log = (log_t)cl_data->cl_hnode->log_data;
	ret = log_open(log, type);
	if (ret != OK)
		return  OK;	

	if (log->head_set == 0) {
		time(&t);
		tm = localtime(&t);
		if (log->terminal_output == 1) {
			ret = log_head_set(log, cl_data, "%s[%s]"NONE"[%d-%d-%d %d:%d:%d]:", 
					type < LOG_MAX ? global_log_print[type].color:
							    global_log_print[LOG_MAX].color,
					type < LOG_MAX ? global_log_print[type].name:
							    global_log_print[LOG_MAX].name, 
					tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, 
					tm->tm_hour, tm->tm_min, tm->tm_sec);
		} else {
			ret = log_head_set(log, cl_data, "[%s][%d-%d-%d %d:%d:%d]:", 
					type < LOG_MAX ? global_log_print[type].name:
							    global_log_print[LOG_MAX].name, 
					tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, 
					tm->tm_hour, tm->tm_min, tm->tm_sec);
		}
		if (ret != OK)
			return ret;
	}

	va_start(arg_ptr, fmt);
	ret = log_data_write(
			log->log_data,
			cl_data->private_link_data.link_id, 
			fmt, 
			arg_ptr);
	va_end(arg_ptr);
	if (ret != OK)
		return ret;

	if (direct == LOG_WRITE_DIRECTLY) {
		log->head_set = 0;
		return log_data_end(
				log->log_data, 
				cl_data->private_link_data.link_id);
	}
	else
		log->head_set = 1;

	return OK;
}

log_t
log_create(	
	char *app_proto
)
{
	int total_link = 0;
	cJSON *obj = NULL;
	struct log *data = NULL;

	data = (struct log*)calloc(1, sizeof(*data));
	if (!data) {
		PANIC("calloc %d bytes error: %s\n", sizeof(*data), strerror(errno));
	}

	obj = config_read_get(app_proto);

	CR_INT(obj, "total_link", total_link);
	CR_USHORT(obj, "open_log", data->open_log);
	if (data->open_log == 0)
		return (log_t)data;
	CR_USHORT(obj, "open_info_log", data->open_info_log);
	CR_USHORT(obj, "open_debug_log", data->open_debug_log);
	CR_USHORT(obj, "open_fatal_log", data->open_fatal_log);
	CR_USHORT(obj, "open_system_log", data->open_system_log);
	CR_USHORT(obj, "open_warning_log", data->open_warning_log);

	data->log_data = log_data_create(total_link);
	if (!data->log_data) {
		FREE(data);
		return NULL;
	}

	return (log_t)data;
}

void
log_destroy(
	unsigned long user_data
)
{
	int ret = 0;
	struct log *log = NULL;
	struct create_link_data *cl_data = NULL;

	if (!user_data)
		return;
	cl_data = list_entry(user_data, struct create_link_data, data);
	log = (struct log*)cl_data->cl_hnode->log_data;
	cl_data->cl_hnode->log_data = NULL;
	if (!log)
		return;
	ret = log_open(log, LOG_MAX);
	if (ret == OK)
		log_data_destroy(log->log_data);
	FREE(log->log_data);
	FREE(log);
}

void
log_address_print(
	log_t *log_data
)
{
	int ret = 0;
	struct log *log = NULL;

	log = (struct log*)log_data;

	log_data_address(log->log_data);
}

