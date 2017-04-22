#include "global_log_private.h"
#include "comm.h"
#include "init.h"
#include "create_log.h"
#include "create_log_private.h"
#include "log.h"
#include <stdarg.h>

struct global_log {
	int open_log;
	int global_info_log;
	int global_warn_log;
	int global_debug_log;
	int global_fatal_log;
	int global_system_log;
	int terminal_output;
	int head_set;
	log_data_t log;
};

static struct global_log global_log;

#define LOG_CHECK2(l, memb, type) \
	case type: \
		if (l.memb == 0) \
			return ERR; 

static int
global_log_open(
	int		type	
)
{
	if (global_log.open_log == 0)
		return ERR;

	switch (type) {
		LOG_CHECK2(global_log, global_info_log, LOG_INFO);
		LOG_CHECK2(global_log, global_warn_log, LOG_WARN);
		LOG_CHECK2(global_log, global_debug_log, LOG_DEBUG);
		LOG_CHECK2(global_log, global_fatal_log, LOG_FATAL);
		LOG_CHECK2(global_log, global_system_log, LOG_SYSTEM);
		default:
			return ERR;
	}

	return OK;
}

static int
global_log_head_set(
	char *fmt,
	...
)
{
	int ret = 0;
	va_list arg_ptr;

	va_start(arg_ptr, fmt);
	ret = log_data_write(
			global_log.log, 
			0,
			fmt,
			arg_ptr);
	va_end(arg_ptr);

	return ret;
}

int
global_log_write(
	int type,
	char *fmt,
	...
)
{
	int ret = 0;
	char *name = NULL, *color = NULL;
	time_t t;
	struct tm *tm = NULL;
	va_list arg_ptr;

	ret = global_log_open(type);
	if (ret != OK)
		return  OK;

	log_output_type_get(type, &name, &color);	
	if (ret != OK)
		return ret;

	if (global_log.head_set) {
		time(&t);
		tm = localtime(&t);
		if (global_log.terminal_output == 1) {
			ret = global_log_head_set("%s[%s]\e[0m][%d-%d-%d %d:%d:%d]:", 
				color, name,
				tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, 
				tm->tm_hour, tm->tm_min, tm->tm_sec);
		} else {
			ret = global_log_head_set( "[%s][%d-%d-%d %d:%d:%d]:", 
					name,
					tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, 
					tm->tm_hour, tm->tm_min, tm->tm_sec);
		}
	}
	va_start(arg_ptr, fmt);
	ret = log_data_write(
			global_log.log,
			0,
			fmt, 
			arg_ptr);
	va_end(arg_ptr);
	if (ret != OK)
		return ret;

	return log_data_end(
			global_log.log, 
			0);
}

static int
global_log_setup()
{
	int total_link = 0;
	cJSON *obj = NULL;
	char file[256] = { 0 };

	obj = config_read_get("general");

	CR_INT(obj, "total_link", total_link);
	CR_USHORT(obj, "global_open_log", global_log.open_log);
	if (global_log.open_log == 0)
		return OK;
	CR_USHORT(obj, "global_info_log", global_log.global_info_log);
	CR_USHORT(obj, "global_debug_log", global_log.global_debug_log);
	CR_USHORT(obj, "global_fatal_log", global_log.global_fatal_log);
	CR_USHORT(obj, "global_system_log", global_log.global_system_log);
	CR_USHORT(obj, "global_warning_log", global_log.global_warn_log);
	CR_STR(obj, "global_log_file", file);

	global_log.log = log_data_create(1);
	if (!global_log.log) 
		ERR;

	if (!file[0])
		global_log.terminal_output = 1;

	return  log_data_start(global_log.log, 0, file);
}

static int
global_log_uninit()
{
	log_data_destroy(global_log.log);

	return OK;
}

int
global_log_init()
{
	int ret = 0;

	ret = init_register(global_log_setup);
	if (ret != OK)
		return ret;
	return uninit_register(global_log_uninit);
}

MOD_INIT(global_log_init);

